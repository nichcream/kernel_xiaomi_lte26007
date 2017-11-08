/*
 * LC186X bridge net driver
 *
 * Copyright (C) 2013 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/gfp.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <mach/comip-bridge.h>
#include <asm/cacheflush.h>

struct bridge_net_priv{
	bool					use_napi;
	struct napi_struct		napi;
	struct list_head		tx_list;
	struct list_head		free_list;
	struct work_struct		tx_work;
	struct workqueue_struct *tx_wq;
	spinlock_t				lock;
	bool					dbg_flag;
	struct bridge_drvdata	*priv;
};

struct bridge_net_txreq{
	struct list_head	list;
	struct sk_buff		*skb;
};

#define BRIDGENET_VERSION "0.1"
#define MIN_MTU			(68)
#define MAX_MTU			(65535)
#define BRIDGE_NET_MTU	(1500)
#define TX_QUEUE_LEN	(500)

static irqreturn_t bridge_net_rx_irq(struct bridge_drvdata *drvdata)
{
	struct net_device *dev = drvdata->net_dev;
	struct bridge_net_priv *netpriv = netdev_priv(dev);

	if (netpriv->use_napi) {
		if (napi_schedule_prep(&netpriv->napi)) {
			drvdata->ops->irq_dis(drvdata->rxirq);
			__napi_schedule(&netpriv->napi);
		}
		return IRQ_HANDLED;
	}else
		return IRQ_NONE;
}
static irqreturn_t bridge_net_ctl_irq(struct bridge_drvdata *drvdata)
{
	struct net_device *dev = drvdata->net_dev;
	if (netif_queue_stopped(dev))
		netif_wake_queue(dev);
	return IRQ_HANDLED;
}

static int bridge_net_rcvbuf_alloc(struct bridge_drvdata *drvdata,
		struct bridge_rxdata *rxbuf, size_t size)
{
	struct sk_buff *skb = NULL;
	struct net_device * dev = drvdata->net_dev;

	skb = netdev_alloc_skb_ip_align(dev, size);
	if (skb) {
		dma_map_single(&dev->dev, (void *)skb->data, size, DMA_FROM_DEVICE);
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
		rxbuf->frame.pData = skb->data;
#else
		rxbuf->vData = skb->data;
		rxbuf->frame.pData = __virt_to_phys(rxbuf->vData);
#endif
		if (!rxbuf->frame.pData) {
			BRIDGE_ERR("%s net_rcvbuf_alloc err.\n", drvdata->name);
			return -ENOMEM;
		}
		rxbuf->context = skb;
		return 0;
	}
	else {
		BRIDGE_ERR("%s net_alloc skb err.\n", drvdata->name);
		return -ENOMEM;
	}
}
static inline void bridge_net_rcvbuf_free(struct bridge_rxdata *rxbuf)
{
	if (rxbuf && rxbuf->context)
		dev_kfree_skb_any(rxbuf->context);
	else
		BRIDGE_ERR("net_rcvbuf_free err. rxbuf=%x, context=%x\n", (u32)rxbuf, (u32)rxbuf->context);
}

static int bridge_net_poll(struct napi_struct *napi, int budget)
{
	struct bridge_net_priv	*netpriv = container_of(napi, struct bridge_net_priv, napi);
	struct bridge_drvdata *drvdata = netpriv->priv;
	struct net_device * dev = drvdata->net_dev;
	int work_done = 0;
	u32 list = 0;
	struct sk_buff *skb = NULL;
	struct bridge_rxdata *rxdata = NULL, *n = NULL;

	work_done = check_para(drvdata);
	if (work_done)
		goto error;

	if (drvdata->ops && drvdata->ops->recv_pkt) {
		work_done = drvdata->ops->recv_pkt(drvdata, &list, budget, false);
		if (work_done >= 0) {
			if (work_done == 0)
				drvdata->rx_dinfo.count = work_done;
			else
				list_for_each_entry_safe(rxdata, n, (struct list_head *)list, list)
				{
					list_del(&rxdata->list);
					skb = rxdata->context;
					skb_put(skb, rxdata->frame.Count);
					list_add_tail(&rxdata->list, &drvdata->rxdata_list);
					dma_map_single(&dev->dev, (void *)skb->data, rxdata->frame.Count, DMA_FROM_DEVICE);
					switch (skb->data[0] & 0xf0) {
						case 0x40:
							skb->protocol = htons(ETH_P_IP);
							break;
						case 0x60:
							skb->protocol = htons(ETH_P_IPV6);
							break;
						default:
							BRIDGE_ERR("net_poll drop data0:0x%x\n",skb->data[0]);
							dev->stats.rx_dropped++;
							dev_kfree_skb_any(skb);
							continue;
					}
					skb_reset_mac_header(skb);
					skb->dev = dev;
					dev->stats.rx_packets++;
					dev->stats.rx_bytes += skb->len;
					drvdata->rx_dinfo.count = skb->len;
					napi_gro_receive(&netpriv->napi, skb);
				}
			drvdata->rx_dinfo.t = get_clock();
		}
		/* If budget not fully consumed, exit the polling mode */
		if (work_done < budget) {
			napi_complete(napi);
			if (drvdata->ops->irq_en)
				drvdata->ops->irq_en(drvdata->rxirq);
		}
	}
error:
	return work_done;
}

static void bridge_net_txwork(struct work_struct *data)
{
	struct bridge_net_priv	*netpriv = container_of(data, struct bridge_net_priv, tx_work);
	struct bridge_drvdata* drvdata = netpriv->priv;
	struct bridge_net_txreq *txreq = NULL;
	int status = -EINVAL;
	bool is_last = true;
	unsigned long flags;

	if (drvdata && drvdata->ops && drvdata->ops->xmit) {
		while (1) {
			spin_lock_irqsave(&netpriv->lock, flags);
			if (!list_empty(&netpriv->tx_list)) {
				txreq = list_entry(netpriv->tx_list.next, struct bridge_net_txreq, list);
				list_del(&txreq->list);
				is_last = list_empty(&netpriv->tx_list);
				spin_unlock_irqrestore(&netpriv->lock, flags);

				skb_tx_timestamp(txreq->skb);
				status = drvdata->ops->xmit(drvdata, txreq->skb->data, txreq->skb->len, false, is_last);
				if (status >= 0) {
					drvdata->net_dev->stats.tx_bytes += txreq->skb->len;
					drvdata->net_dev->stats.tx_packets++;
				}else
					drvdata->net_dev->stats.tx_dropped++;
				/* free this SKB */
				dev_kfree_skb(txreq->skb);
				spin_lock_irqsave(&netpriv->lock, flags);
				list_add_tail(&txreq->list,&netpriv->free_list);
				spin_unlock_irqrestore(&netpriv->lock, flags);
			}else{
				spin_unlock_irqrestore(&netpriv->lock, flags);
				break;
			}
		}
	}else{
			BRIDGE_ERR("start_xmit err: drvdata=%x, ops=%x, xmit=%x\n",
				(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->xmit);
			return ;
	}
}

static int bridge_net_open(struct net_device *net)
{
	struct bridge_net_priv	*netpriv = netdev_priv(net);
	struct bridge_drvdata *drvdata = netpriv->priv;
	struct bridge_net_txreq *txreq = NULL;
	int i = 0;

	if (drvdata && drvdata->ops && drvdata->ops->open)
		drvdata->ops->open(drvdata);
	else{
		BRIDGE_ERR("net_open err: drvdata=%x, ops=%x, open=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->open);
		return -EINVAL;
	}
	spin_lock_init(&netpriv->lock);
	INIT_LIST_HEAD(&netpriv->free_list);
	INIT_LIST_HEAD(&netpriv->tx_list);

	netpriv->tx_wq = alloc_ordered_workqueue("%s", 0, drvdata->name);
	if (!netpriv->tx_wq) {
		BRIDGE_ERR("net_open err: alloc workequeue failed\n");
		return -ENOMEM;
	}
	INIT_WORK(&netpriv->tx_work, bridge_net_txwork);

	for (i = 0 ; i < BRIDGE_BUF_COUNT ; i ++) {
		txreq = kzalloc(sizeof(struct bridge_net_txreq), GFP_KERNEL);
		INIT_LIST_HEAD(&txreq->list);
		list_add_tail(&txreq->list,&netpriv->free_list);
	}
	memset(&drvdata->net_dev->stats, 0, sizeof(drvdata->net_dev->stats));
	if (netpriv->use_napi)
		napi_enable(&netpriv->napi);
	netif_start_queue(net);
	netif_carrier_on(net);
	netpriv->dbg_flag = true;
	return 0;
}
static int bridge_net_stop(struct net_device *dev)
{
	struct bridge_net_priv	*netpriv = netdev_priv(dev);
	struct bridge_drvdata *drvdata = netpriv->priv;
	struct bridge_net_txreq *txreq = NULL;

	netif_stop_queue(dev);
	if (netpriv->use_napi)
		napi_disable(&netpriv->napi);

	cancel_work_sync(&netpriv->tx_work);
	destroy_workqueue(netpriv->tx_wq);
	while (!list_empty(&netpriv->free_list)) {
		txreq = list_first_entry(&netpriv->free_list, struct bridge_net_txreq, list);
		list_del(&txreq->list);
		kfree(txreq);
	}
	while (!list_empty(&netpriv->tx_list)) {
		txreq = list_first_entry(&netpriv->tx_list, struct bridge_net_txreq, list);
		list_del(&txreq->list);
		kfree(txreq);
	}

	if (drvdata && drvdata->ops && drvdata->ops->close)
		drvdata->ops->close(drvdata);
	else{
		BRIDGE_ERR("net_stop err: drvdata=%x, ops=%x, stop=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->close);
		return -EINVAL;
	}
	return 0;
}
static int bridge_net_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int status = -EINVAL;
	struct bridge_net_priv *netpriv = netdev_priv(dev);
	struct bridge_drvdata* drvdata = netpriv->priv;
	struct bridge_net_txreq *txreq = NULL;
	unsigned long flags;

	if (drvdata && drvdata->ops && drvdata->ops->test_rcvbusy) {
		drvdata->ops->irq_dis(drvdata->rxctl_irq);
		status = drvdata->ops->test_rcvbusy(drvdata);
	}else{
		BRIDGE_ERR("start_xmit err: drvdata=%x, ops=%x, test_rcvbusy=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->test_rcvbusy);
		return -EINVAL;
	}
	if (status) {
		netif_stop_queue(dev);
		drvdata->ops->irq_en(drvdata->rxctl_irq);
		if (netpriv->dbg_flag == true) {
			netpriv->dbg_flag = false;
			BRIDGE_INFO("start_xmit info: %s xmit skb %x CP busy!\n", drvdata->name, (unsigned int)skb);
		}
		return NETDEV_TX_BUSY;
	}else{
		drvdata->ops->irq_en(drvdata->rxctl_irq);
		spin_lock_irqsave(&netpriv->lock, flags);
		if (!list_empty(&netpriv->free_list)) {
			txreq = list_entry(netpriv->free_list.next, struct bridge_net_txreq, list);
			list_del(&txreq->list);
		}else{
			spin_unlock_irqrestore(&netpriv->lock, flags);
			txreq = kzalloc(sizeof(struct bridge_net_txreq), GFP_KERNEL);
			INIT_LIST_HEAD(&txreq->list);
			spin_lock_irqsave(&netpriv->lock, flags);
		}
		txreq->skb = skb;
		list_add_tail(&txreq->list,&netpriv->tx_list);
		if (netpriv->dbg_flag == false) {
			netpriv->dbg_flag = true;
			BRIDGE_INFO("start_xmit info: %s xmit skb %x CP ready!\n", drvdata->name, (unsigned int)skb);
		}
		queue_work(netpriv->tx_wq, &netpriv->tx_work);
		spin_unlock_irqrestore(&netpriv->lock, flags);
		return NETDEV_TX_OK;
	}
}

static void bridge_net_get_drvinfo(struct net_device *net, struct ethtool_drvinfo *p)
{
	strlcpy(p->driver, net->name, sizeof p->driver);
	strlcpy(p->version, BRIDGENET_VERSION, sizeof p->version);
	strlcpy(p->bus_info, dev_name(&net->dev), sizeof p->bus_info);
}

static int bridge_net_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < MIN_MTU || new_mtu + dev->hard_header_len > MAX_MTU)
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static const struct net_device_ops bridge_netdev_ops = {
	.ndo_open		= bridge_net_open,
	.ndo_stop		= bridge_net_stop,
	.ndo_start_xmit		= bridge_net_start_xmit,
	.ndo_change_mtu		= bridge_net_change_mtu,
};
static const struct ethtool_ops bridge_ethtool_ops = {
	.get_drvinfo	= bridge_net_get_drvinfo,
	.get_link	= ethtool_op_get_link,
};
static struct device_type bridge_type = {
	.name	= "bridgenet",
};
static ssize_t bridge_net_info_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t bridge_net_info_show_attrs(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct net_device *net = dev->platform_data;
	struct bridge_net_priv *netpriv = netdev_priv(net);
	struct bridge_drvdata* drvdata = netpriv->priv;

	if (drvdata && drvdata->ops && drvdata->ops->info_show)
		return drvdata->ops->info_show(drvdata, buf);
	else{
		BRIDGE_ERR("net_info_show err: drvdata=%x, ops=%x, info_show=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->info_show);
		return -EINVAL;
	}
}

static ssize_t bridge_net_reg_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t bridge_net_reg_show_attrs(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct net_device *net = dev->platform_data;
	struct bridge_net_priv *netpriv = netdev_priv(net);
	struct bridge_drvdata* drvdata = netpriv->priv;

	if (drvdata && drvdata->ops && drvdata->ops->reg_show)
		return drvdata->ops->reg_show(drvdata, buf);
	else{
		BRIDGE_ERR("net_reg_show err: drvdata=%x, ops=%x, reg_show=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->reg_show);
		return -EINVAL;
	}

}

static DEVICE_ATTR( bridge_net_info, S_IRWXU | S_IWUSR |S_IROTH,
						bridge_net_info_show_attrs, bridge_net_info_store_attrs );
static DEVICE_ATTR( bridge_net_reg_status, S_IRWXU | S_IWUSR |S_IROTH ,
						bridge_net_reg_show_attrs, bridge_net_reg_store_attrs );

static int bridge_net_attr_init(struct bridge_drvdata *drvdata)
{
	int retval = 0;

	retval = device_create_file(&drvdata->net_dev->dev, &dev_attr_bridge_net_info);
	if(retval < 0 ){
		BRIDGE_ERR("net err_create_attr_irq_num: %x", retval);
		goto err_create_attr_irq_num;
	}

	retval = device_create_file(&drvdata->net_dev->dev, &dev_attr_bridge_net_reg_status);
	if(retval < 0 ){
		BRIDGE_ERR("net err_create_attr_irq_status: %x", retval);
		goto err_create_attr_irq_status;
	}

	return retval;
err_create_attr_irq_status:
	device_remove_file(&drvdata->net_dev->dev, &dev_attr_bridge_net_info);
err_create_attr_irq_num:
	return retval;

}
static int bridge_net_attr_uninit(struct bridge_drvdata *drvdata)
{
	device_remove_file(&drvdata->net_dev->dev, &dev_attr_bridge_net_info);
	device_remove_file(&drvdata->net_dev->dev, &dev_attr_bridge_net_reg_status);

	return 0;
}
static void bridge_net_setup(struct net_device *dev)
{
	dev->netdev_ops = &bridge_netdev_ops;
	dev->ethtool_ops = &bridge_ethtool_ops;
	dev->hard_header_len = 0;
	dev->addr_len = 0;
	dev->mtu = BRIDGE_NET_MTU;
	/* Zero header length */
	dev->type = ARPHRD_NONE;
	dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	dev->tx_queue_len = TX_QUEUE_LEN;

}
int bridge_net_device_init(struct bridge_drvdata *drvdata, struct device *parent)
{
	struct bridge_net_priv *netpriv = NULL;
	struct net_device	*net = NULL;
	int	status = -EINVAL;

	net = alloc_netdev(sizeof(struct bridge_net_priv), drvdata->name, bridge_net_setup);
	if (!net) {
		BRIDGE_ERR("%s alloc_netdev fail!\n", drvdata->name);
		return -ENOMEM;
	}
	netpriv = netdev_priv(net);
	netpriv->use_napi = true;
	netpriv->priv = drvdata;

	/* network device setup */
	drvdata->net_dev = net;
	drvdata->alloc_rcvbuf = bridge_net_rcvbuf_alloc;
	drvdata->free_rcvbuf = bridge_net_rcvbuf_free;
	drvdata->rxhandle = bridge_net_rx_irq;
	drvdata->ctlhandle = bridge_net_ctl_irq;

	if (netpriv->use_napi)
		netif_napi_add(net, &netpriv->napi, bridge_net_poll, 64);

	SET_NETDEV_DEV(net, parent);
	SET_NETDEV_DEVTYPE(net, &bridge_type);

	status = register_netdev(net);
	if (status < 0) {
		BRIDGE_ERR("register %s netdev failed, %d\n", drvdata->name, status);
		free_netdev(net);
		return status;
	}

	status = bridge_net_attr_init(drvdata);

	return status;
}
int bridge_net_device_uninit(struct bridge_drvdata *drvdata)
{
	bridge_net_attr_uninit(drvdata);

	unregister_netdev(drvdata->net_dev);

	free_netdev(drvdata->net_dev);

	return 0;
}

struct bridge_drvdata * get_bridge_net_drvdata(struct net_device	*dev)
{
	struct bridge_net_priv *netpriv = netdev_priv(dev);
	return netpriv->priv;
}
