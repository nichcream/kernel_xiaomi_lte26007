/*
 * LC186X bridge common driver
 *
 * Copyright (C) 2013 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/platform_device.h>
#include <mach/comip-bridge.h>

struct bridge_local_buf{
	unsigned char	*pData;
	u32		Count;
	u32		pos;
};
static struct bridge_local_buf bridge_local_buf;
static int bridge_rcvbuf_alloc(struct bridge_drvdata *drvdata,
	struct bridge_rxdata *rxbuf, size_t size)
{
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
	rxbuf->vData = dma_alloc_coherent(NULL, size, (dma_addr_t *)(&rxbuf->pData),
			GFP_KERNEL);
#else
	rxbuf->vData = dma_alloc_coherent(NULL, size, (dma_addr_t *)(&rxbuf->frame.pData),
			GFP_KERNEL);
#endif
	if (!rxbuf->vData) {
		BRIDGE_ERR("%s rcvbuf_alloc %d err.\n", drvdata->name, size);
		return -ENOMEM;
	}
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
	rxbuf->frame.pData = rxbuf->vData;
#endif

	return 0;
}

static void bridge_rcvbuf_free(struct bridge_rxdata *rxbuf)
{
	if (rxbuf->vData)
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
		dma_free_coherent(NULL, rxbuf->frame.Count, rxbuf->vData,
				(dma_addr_t)(rxbuf->pData));
#else
		dma_free_coherent(NULL, rxbuf->frame.Count, rxbuf->vData,
				(dma_addr_t)((unsigned int)rxbuf->frame.pData));
#endif
}

static irqreturn_t bridge_rx_irq(struct bridge_drvdata *drvdata)
{
	return IRQ_NONE;
}
static irqreturn_t bridge_ctl_irq(struct bridge_drvdata *drvdata)
{
	return IRQ_NONE;
}

static ssize_t bridge_write(struct file *file, const char __user *buf,	size_t count, loff_t *ppos)
{
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	struct bridge_drvdata *drvdata = container_of(misc, struct bridge_drvdata, misc_dev);

	if (drvdata && drvdata->ops && drvdata->ops->xmit)
		return drvdata->ops->xmit(drvdata, buf, count, true, true);
	else{
		BRIDGE_ERR("write err: drvdata=%x, ops=%x, xmit=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->xmit);
		return -EINVAL;
	}
}

static ssize_t bridge_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t status = -EINVAL ;
	struct miscdevice  * misc = (struct miscdevice *)(file->private_data);
	struct bridge_drvdata *drvdata = container_of(misc,
					     struct bridge_drvdata, misc_dev);
	u32 len = count, tmp_len = 0, ret = 0;
	u32 list = 0;
	struct bridge_rxdata *rxdata = NULL;

	mutex_lock(&drvdata->read_sem);

	if (0 != check_para(drvdata))
		goto out;

	if (drvdata->property.pro) {
		if (bridge_local_buf.Count) {
			tmp_len = len <= bridge_local_buf.Count ? len : bridge_local_buf.Count;
			ret = copy_to_user(buf, (void *)((unsigned int)bridge_local_buf.pData) + bridge_local_buf.pos, tmp_len);
			if (likely(!ret)) {
				len -= tmp_len;
				bridge_local_buf.Count -= tmp_len;
				status = tmp_len;
				if (bridge_local_buf.Count == 0)
					bridge_local_buf.pos = 0;
				else
					bridge_local_buf.pos += tmp_len;
			}else{
				goto out;
			}
		}
		while (len > 0) {
			status = drvdata->ops->recv_pkt(drvdata, &list, 1, true);
			if (status >= 0) {
				status = tmp_len;
				rxdata = list_entry(((struct list_head *)list)->next, struct bridge_rxdata, list);
				if (rxdata->frame.Count <= len) {
					ret = copy_to_user(buf+tmp_len, rxdata->vData, rxdata->frame.Count);
					if (likely(!ret)) {
						list_del(&rxdata->list);
						status += rxdata->frame.Count;
						len -= rxdata->frame.Count;
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
						dma_free_coherent(NULL, rxdata->frame.Count, rxdata->vData,
												(dma_addr_t)(rxdata->pData));
#else
						dma_free_coherent(NULL, rxdata->frame.Count, rxdata->vData,
												(dma_addr_t)((unsigned int)rxdata->frame.pData));
#endif
						list_add_tail(&rxdata->list, &drvdata->rxdata_list);
					}else{
						status = -ENOMEM;
						goto out;
					}
					tmp_len = status;
				}
				else{
					ret = copy_to_user(buf+tmp_len, rxdata->vData, len);
					if (likely(!ret)) {
						status += len;
						memcpy(bridge_local_buf.pData, rxdata->vData + len, rxdata->frame.Count - len);
						bridge_local_buf.Count = (rxdata->frame.Count - len);
						len = 0;
						list_del(&rxdata->list);
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
						dma_free_coherent(NULL, rxdata->frame.Count, rxdata->vData,
												(dma_addr_t)(rxdata->pData));
#else
						dma_free_coherent(NULL, rxdata->frame.Count, rxdata->vData,
												(dma_addr_t)((unsigned int)rxdata->frame.pData));
#endif
						list_add_tail(&rxdata->list, &drvdata->rxdata_list);
					}else{
						status = -ENOMEM;
						goto out;
					}
				}
			}else
				goto out;
		}
	}
	else
		status = drvdata->ops->recv_raw(drvdata, buf, count);

	drvdata->rx_dinfo.t = get_clock();
	drvdata->rx_dinfo.count = status;
out:
	mutex_unlock(&drvdata->read_sem);
	return status;
}
static int bridge_open(struct inode *inode, struct file *file)
{
	struct miscdevice  * misc = NULL;
	struct bridge_drvdata *drvdata = NULL;

	misc = (struct miscdevice *)(file->private_data);
	drvdata = container_of(misc, struct bridge_drvdata, misc_dev);

	if (drvdata && drvdata->ops && drvdata->ops->open) {
		if ((drvdata->property.pro_b.packet) && (!drvdata->f_open)) {
			bridge_local_buf.pData = kzalloc(drvdata->rxinfo.pkt_size, GFP_ATOMIC);
			bridge_local_buf.Count = 0;
			bridge_local_buf.pos = 0;
		}
		return drvdata->ops->open(drvdata);
	}
	else{
		BRIDGE_ERR("open err: drvdata=%x, ops=%x, open=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->open);
		return -EINVAL;
	}
}
static int bridge_release(struct inode *inode, struct file *file)
{
	struct miscdevice  * misc = NULL;
	struct bridge_drvdata *drvdata = NULL;

	misc = (struct miscdevice *)(file->private_data);
	drvdata = container_of(misc, struct bridge_drvdata, misc_dev);

	if (drvdata && drvdata->ops && drvdata->ops->close)
		return drvdata->ops->close(drvdata);
	else{
		BRIDGE_ERR("release err: drvdata=%x, ops=%x, close=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->close);
		return -EINVAL;
	}
}
static long bridge_ioctl(struct file* file, unsigned int cmd, unsigned long arg){
	int left = -EINVAL;
	struct miscdevice *misc = NULL;
	struct bridge_drvdata *drvdata = NULL;
	void __user *argp = (void __user *)arg;

	misc = (struct miscdevice *)(file->private_data);
	drvdata = container_of(misc, struct bridge_drvdata, misc_dev);
	if (!drvdata) {
		BRIDGE_ERR("ioctl err drvdata null!\n");
		goto out;
	}
	switch (cmd) {
		case GET_BRIDGE_TX_LEN:
			left = copy_to_user(argp, &drvdata->txinfo.length, sizeof(drvdata->txinfo.length));
			break;

		case GET_BRIDGE_RX_LEN:
			left = copy_to_user(argp, &drvdata->rxinfo.length, sizeof(drvdata->rxinfo.length));
			break;

		case SET_BRIDGE_FLOW_CTL:
			if (drvdata->property.pro_b.flow_ctl) {
					if (drvdata->ops && drvdata->ops->set_fc)
						left = drvdata->ops->set_fc(drvdata);
					else
						BRIDGE_ERR("ioctl %s err: ops=%x, set_fc=%x\n",
							drvdata->name, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->set_fc);
			}else
				BRIDGE_ERR("ioctl %s err: no support SET_BRIDGE_FLOW_CTL\n",
					drvdata->name);
			break;

		case CLEAR_BRIDGE_FLOW_CTL:
			if (drvdata->property.pro_b.flow_ctl) {
					if (drvdata->ops && drvdata->ops->clr_fc)
						left = drvdata->ops->clr_fc(drvdata);
					else
						BRIDGE_ERR("ioctl %s err: ops=%x, clr_fc=%x\n",
							drvdata->name, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->clr_fc);
			}else
				BRIDGE_ERR("ioctl %s err: no support CLEAR_BRIDGE_FLOW_CTL\n",
					drvdata->name);
			break;
	}
out:
	return left;
}

static unsigned int bridge_poll(struct file *file, poll_table *wait)
{
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	struct bridge_drvdata *drvdata = container_of(misc,
					     struct bridge_drvdata, misc_dev);

	if (drvdata && drvdata->ops && drvdata->ops->poll) {
		poll_wait(file, &drvdata->rxwait_queue, wait);
		if (drvdata->property.pro_b.flow_ctl)
			poll_wait(file, &drvdata->txwait_queue, wait);

		return drvdata->ops->poll(drvdata);
	}else
		BRIDGE_ERR("poll err: drvdata=%x, ops=%x, poll=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->poll);
		return -EINVAL;
}
static const struct file_operations bridges_fops = {
	.owner         = THIS_MODULE,
	.open          = bridge_open,
	.read          = bridge_read,
	.poll	       = bridge_poll,
	.unlocked_ioctl= bridge_ioctl,
	.write         = bridge_write,
	.release       = bridge_release,
};
static ssize_t bridge_info_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t bridge_info_show_attrs(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct bridge_drvdata* drvdata = NULL;
	struct miscdevice *misc = NULL;

	misc = dev_get_drvdata(dev);
	drvdata = container_of(misc, struct bridge_drvdata, misc_dev);

	if (drvdata && drvdata->ops && drvdata->ops->info_show)
		return drvdata->ops->info_show(drvdata, buf);
	else{
		BRIDGE_ERR("info_show err: drvdata=%x, ops=%x, info_show=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->info_show);
		return -EINVAL;
	}
}

static ssize_t bridge_reg_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t bridge_reg_show_attrs(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct bridge_drvdata* drvdata = NULL;
	struct miscdevice *misc = NULL;

	misc = dev_get_drvdata(dev);
	drvdata = container_of(misc, struct bridge_drvdata, misc_dev);

	if (drvdata && drvdata->ops && drvdata->ops->reg_show)
		return drvdata->ops->reg_show(drvdata, buf);
	else{
		BRIDGE_ERR("reg_show err: drvdata=%x, ops=%x, info_show=%x\n",
			(unsigned int)drvdata, (unsigned int)drvdata->ops, (unsigned int)drvdata->ops->reg_show);
		return -EINVAL;
	}

}


static DEVICE_ATTR( bridge_info, S_IRWXU | S_IWUSR |S_IROTH,
						bridge_info_show_attrs, bridge_info_store_attrs );
static DEVICE_ATTR( bridge_reg_status, S_IRWXU | S_IWUSR |S_IROTH ,
						bridge_reg_show_attrs, bridge_reg_store_attrs );

static int bridge_attr_init(struct bridge_drvdata *drvdata)
{
	int retval = 0;

	retval = device_create_file(drvdata->misc_dev.this_device, &dev_attr_bridge_info);
	if(retval < 0 ){
		BRIDGE_ERR("err_create_attr_irq_num: %x\n", retval);
		goto err_create_attr_irq_num;
	}

	retval = device_create_file(drvdata->misc_dev.this_device, &dev_attr_bridge_reg_status);
	if(retval < 0 ){
		BRIDGE_ERR("err_create_attr_irq_status: %x\n", retval);
		goto err_create_attr_irq_status;
	}
	return retval;
err_create_attr_irq_status:
	device_remove_file(drvdata->misc_dev.this_device, &dev_attr_bridge_info);
err_create_attr_irq_num:
	return retval;

}
static int bridge_attr_uninit(struct bridge_drvdata *drvdata)
{
	device_remove_file(drvdata->misc_dev.this_device, &dev_attr_bridge_info);
	device_remove_file(drvdata->misc_dev.this_device, &dev_attr_bridge_reg_status);

	return 0;
}

int bridge_device_init(struct bridge_drvdata *drvdata, struct device *parent)
{
	int retval = 0;

	drvdata->misc_dev.minor = MISC_DYNAMIC_MINOR;
	drvdata->misc_dev.parent = parent;
	drvdata->misc_dev.name = drvdata->name;
	drvdata->misc_dev.fops = &bridges_fops;

	retval = misc_register(&drvdata->misc_dev);
	if(retval){
		BRIDGE_ERR("register bridge device %s failed!\n", drvdata->name);
		goto out;
	}
	if (drvdata->property.pro) {
		drvdata->alloc_rcvbuf = bridge_rcvbuf_alloc;
		drvdata->free_rcvbuf = bridge_rcvbuf_free;
	}
	drvdata->rxhandle = bridge_rx_irq;
	drvdata->ctlhandle = bridge_ctl_irq;

	bridge_attr_init(drvdata);
out:
	return retval;
}
int bridge_device_uninit(struct bridge_drvdata *drvdata)
{
	int retval = 0;

	bridge_attr_uninit(drvdata);

	misc_deregister(&drvdata->misc_dev);

	return retval;
}

