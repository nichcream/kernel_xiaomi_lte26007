/*
 * LC186X Bridge core driver
 *
 * Copyright (C) 2013 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/pagemap.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <plat/hardware.h>
#include <mach/comip-irq.h>
#include <mach/comip-bridge.h>

/*-------------------------------------------------------------------------*/

/*
 * buf_init
 *
 * Allocate a circular buffer and all associated memory.
 */
static int buf_init(struct bridge_buf *bf, char* buf_base, unsigned size, u32 put_offset, u32 get_offset, u32 unit)
{

	if (buf_base == NULL || bf == NULL){
		BRIDGE_ERR("buf_base:%x, bf:%x \n", (unsigned int)buf_base, (unsigned int)bf);
		return -EINVAL;
	}

	bf->buf_buf = buf_base;
	bf->buf_size = size;
	bf->buf_put = bf->buf_buf + put_offset * unit;
	bf->buf_get = bf->buf_buf + get_offset * unit;

	return 0;
}
static int info_init(Bridge_Pro pro, struct bridge_info *info, u32 unit)
{
	if (pro.pro) {
		struct buf_hbridge_head *phead = (struct buf_hbridge_head *)info->membase;
		info->pdata = (char *)(info->membase + sizeof(struct buf_hbridge_head));
		info->length = info->memlength - sizeof(struct buf_hbridge_head);
		if (info->length < info->pkt_num * unit) {
			BRIDGE_ERR( "%s more pkt num! pktnum:0x%x memlen:0x%x\n",
				__func__, info->pkt_num, info->memlength);
			return -EINVAL;
		}else
			info->length = info->pkt_num * unit;
		return buf_init(&info->circular_buf, info->pdata, info->length,
					phead->SndFrmOffset, phead->RcvFrmOffset, unit);
	}else{
		struct buf_lbridge_head *phead = (struct buf_lbridge_head *)info->membase;
		info->pdata = (char *)(info->membase + sizeof(struct buf_lbridge_head));
		info->length = info->memlength - sizeof(struct buf_lbridge_head);

		return buf_init(&info->circular_buf, info->pdata, info->length,
					phead->SndDataOffset, phead->RcvDataOffset, unit);
	}
}
/*
 * buf_data_avail
 *
 * Return the number of bytes of data written into the circular
 * buffer.
 */
static unsigned buf_data_avail(struct bridge_buf *bf, u32 unit)
{
	return ((bf->buf_size + bf->buf_put - bf->buf_get) % bf->buf_size) / unit;
}

/*
 * buf_space_avail
 *
 * Return the number of bytes of space available in the circular
 * buffer.
 */
static unsigned buf_space_avail(struct bridge_buf *bf, u32 unit)
{
	return ((bf->buf_size + bf->buf_get - bf->buf_put - unit) % bf->buf_size) / unit;
}

/*
 * user_buf_put_data
 *
 * Copy data data from a user buffer and put it into the circular buffer.
 * Restrict to the amount of space available.
 *
 * Return the number of bytes copied.
 */
static ssize_t
user_buf_put_data(struct bridge_drvdata *drvdata, struct bridge_buf *bf,
					const char __user *buf, unsigned count)
{
	unsigned len = 0;
	unsigned long copy_left_num = 0 ;
	unsigned char __iomem *vData = NULL;

	if (drvdata->property.pro_b.packet) {
		struct bridge_frame *frame = NULL;

		len  = buf_space_avail(bf, drvdata->unit);
		if (!len) {
			BRIDGE_INFO( "%s data No space!\n", drvdata->name);
			return -ENOMEM;
		}

		frame = (struct bridge_frame *)drvdata->txinfo.circular_buf.buf_put;
		if (!frame->pData) {
			BRIDGE_ERR( "%s %s bad parm: null addr!\n", drvdata->name, __func__);
			return -EFAULT;
		}
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
		copy_left_num = copy_from_user(frame->pData, buf, count);
#else
		vData = frame->pData + drvdata->baseaddr;
		copy_left_num = copy_from_user(vData, buf, count);
#endif
		if(unlikely(copy_left_num)){
			BRIDGE_ERR( "%s copy from user %ld!\n", drvdata->name, copy_left_num);
			return -ENOMEM;
		}
		frame->Count = count;
		bf->buf_put = bf->buf_put + drvdata->unit;
		if (bf->buf_put > bf->buf_buf + bf->buf_size)
			bf->buf_put = bf->buf_put - bf->buf_size;
		else if (bf->buf_put == bf->buf_buf + bf->buf_size)
			bf->buf_put = bf->buf_buf;
		return 1;
	}else{
		len  = buf_space_avail(bf, drvdata->unit);
		if (count > len) {
			BRIDGE_ERR( "%s no space want %d avail %d!\n",drvdata->name,count,len);
			return -ENOMEM;
		}

		len = bf->buf_buf + bf->buf_size - bf->buf_put;
		if (count > len) {
			copy_left_num = copy_from_user(bf->buf_put, buf, len);
			if(unlikely(copy_left_num)){
				BRIDGE_ERR( "%s copy from user1 %ld!\n",drvdata->name,copy_left_num);
				return -ENOMEM;
			}
			copy_left_num = copy_from_user(bf->buf_buf, buf+len, count - len);
			if(unlikely(copy_left_num)){
				BRIDGE_ERR( "%s copy from user2 %ld!\n",drvdata->name,copy_left_num);
				return -ENOMEM;
			}
			bf->buf_put = bf->buf_buf + count - len;
		} else {
			copy_left_num = copy_from_user(bf->buf_put, buf, count);
			if(unlikely(copy_left_num)){
				BRIDGE_ERR( "%s copy from user3 %ld!\n",drvdata->name,copy_left_num);
				return -ENOMEM;
			}
			if (count < len)
				bf->buf_put += count;
			else /* count == len */
				bf->buf_put = bf->buf_buf;
		}
		return count;
	}
}
static ssize_t
user_buf_put_pkt(struct bridge_drvdata *drvdata, struct bridge_buf *bf,
					const char *buf, unsigned count)
{
	unsigned len = 0;
	struct bridge_frame *frame = NULL;
	unsigned char __iomem *vData = NULL;

	len  = buf_space_avail(bf, drvdata->unit);
	if (!len){
		BRIDGE_INFO( "%s pkt No space!\n", drvdata->name);
		return -ENOMEM;
	}

	frame = (struct bridge_frame *)drvdata->txinfo.circular_buf.buf_put;
	if (!frame->pData) {
		BRIDGE_ERR( "%s %s bad parm: null addr!\n", drvdata->name, __func__);
		return -EFAULT;
	}
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
	memcpy(frame->pData, buf, count);
#else
	vData = frame->pData + drvdata->baseaddr;
	memcpy(vData, buf, count);
#endif
	frame->Count = count;
	bf->buf_put = bf->buf_put + drvdata->unit;
	if (bf->buf_put > bf->buf_buf + bf->buf_size)
		bf->buf_put = bf->buf_put - bf->buf_size;
	else if (bf->buf_put == bf->buf_buf + bf->buf_size)
		bf->buf_put = bf->buf_buf;
	return 1;
}

/*
 * user_buf_get_data
 *
 * Get data from the circular buffer and copy to the given user  buffer.
 * Restrict to the amount of data available.
 *
 * Return the number of bytes copied.
 */
static ssize_t
user_buf_get_data(struct bridge_drvdata *drvdata, struct bridge_buf *bf,
							char __user *buf, unsigned count)
{
	unsigned len = 0;
	unsigned tmp_len = 0;

	len = buf_data_avail(bf, drvdata->unit);
	if (count > len)
		count = len;

	if (count == 0)
		return 0;

	len = bf->buf_buf + bf->buf_size - bf->buf_get;
	if (count > len) {
		tmp_len = copy_to_user(buf, bf->buf_get, len);
		if (tmp_len == 0) {
			tmp_len = copy_to_user(buf+len, bf->buf_buf, count - len);/*xk fix me*/
			bf->buf_get = bf->buf_buf + count-tmp_len - len;
		}else{
			count = len;
			bf->buf_get += count - tmp_len;
		}
	}else{
		tmp_len = copy_to_user(buf, bf->buf_get, count);
		if ((count - tmp_len) < len)
			bf->buf_get += count - tmp_len;
		else /* count == len */
			bf->buf_get = bf->buf_buf;
	}

	return (count-tmp_len);
}
static int alloc_rx(struct bridge_rxdata *rxbuf, struct bridge_drvdata *drvdata, struct bridge_frame *frame)
{
	int status = -EINVAL;
	if (drvdata->alloc_rcvbuf) {
		status = drvdata->alloc_rcvbuf(drvdata, rxbuf, drvdata->rxinfo.pkt_size);
		if(status)
			goto out;
		list_add_tail(&rxbuf->list,&drvdata->rxbuf_list);
		frame->pData = rxbuf->frame.pData;
	}
out:
	return status;
}
static int copy_data_pkt(struct bridge_drvdata *drvdata, char *buf, u32 count)
{
	struct bridge_rxdata *data = NULL, *n = NULL;
	struct bridge_frame *src_frame = NULL;
	u32 i = 0;
	int status = -EINVAL;
	while (i < count) {
		src_frame = (struct bridge_frame *)(buf + i*drvdata->unit);
		list_for_each_entry_safe(data, n, &drvdata->rxbuf_list, list) {
			if (data->frame.pData == src_frame->pData)
				break;
		}
		if (data->frame.pData != src_frame->pData) {
			BRIDGE_ERR( "%s %s can't find rxdata!\n", drvdata->name, __func__);
			goto out;
		}
		list_del(&data->list);
		if (src_frame->Count > drvdata->rxinfo.pkt_size) {
			BRIDGE_ERR( "%s %s bad parm: more cnt 0x%x(0x%x)!\n", drvdata->name, __func__,
						src_frame->Count, drvdata->rxinfo.pkt_size);
			return -EINVAL;
		}
		data->frame.Count = src_frame->Count;
		src_frame->pData = NULL;
		src_frame->Count = 0;
		list_add_tail(&data->list, &drvdata->rx_list);
		if (list_empty(&drvdata->rxdata_list)) {
			data = kzalloc(sizeof(struct bridge_rxdata), GFP_KERNEL);
			INIT_LIST_HEAD(&data->list);
			if (!data) {
				BRIDGE_ERR("data alloc fail!\n");
				status = -ENOMEM;
				goto out;
			}
		}else{
			data = list_entry(drvdata->rxdata_list.next, struct bridge_rxdata, list);
			list_del(&data->list);
		}
		status = alloc_rx(data, drvdata, src_frame);
		if (status) {
			BRIDGE_ERR("alloc buf err :%x\n", status);
			break;
		}
		i++;
	}
	if (i == count)
		status = i;
out:
	return status;
}
static ssize_t
user_buf_get_pkt(struct bridge_drvdata *drvdata, struct bridge_buf *bf,
						u32 *buf, unsigned count)
{
	unsigned len = 0;
	int temp_len = 0;

	len = buf_data_avail(bf, drvdata->unit);
	if (count > len)
		count = len;

	if (count == 0)
		return 0;

	len = (bf->buf_buf + bf->buf_size - bf->buf_get) / drvdata->unit;
	if (count >= len) {
		temp_len = copy_data_pkt(drvdata, bf->buf_get, len);
		if (temp_len == len) {
			temp_len = copy_data_pkt(drvdata, bf->buf_buf, (count - len));
			if (temp_len == count - len)
				bf->buf_get = bf->buf_buf + (count - len) * drvdata->unit;
			else
				return temp_len;/*return fail*/
		}else{
			return temp_len;/*return fail*/
		}
	}else{
		temp_len = copy_data_pkt(drvdata, bf->buf_get, count);
		if (temp_len == count)
			bf->buf_get = bf->buf_get + count * drvdata->unit;
		else
			return temp_len;/*return fail*/
	}
	*buf = (u32)&drvdata->rx_list;
	return count;
}

u64 get_clock(void)
{
	int cpu = 0;
	cpu = smp_processor_id();
	return  cpu_clock(cpu);
}

static inline u32 get_rcv_offset(Bridge_Pro pro,
		struct buf_lbridge_head *plhead, struct buf_hbridge_head *phhead)
{
	if (pro.pro_b.packet)
		return phhead->RcvFrmOffset;
	else
		return plhead->RcvDataOffset;
}
static inline u32 get_snd_offset(Bridge_Pro *pro,
		struct buf_lbridge_head *plhead, struct buf_hbridge_head *phhead)
{
	if (pro->pro_b.packet)
		return phhead->SndFrmOffset;
	else
		return plhead->SndDataOffset;
}
static inline u32 get_snd_irq_count(Bridge_Pro *pro,
		struct buf_lbridge_head *plhead, struct buf_hbridge_head *phhead)
{
	if (pro->pro_b.packet)
		return phhead->SndFrmIrqCount;
	else
		return plhead->SndIrqCount;
}
static inline void rcv_irq_count_add(struct bridge_drvdata *drvdata,
		struct buf_lbridge_head *plhead, struct buf_hbridge_head *phhead)
{
	if (drvdata->property.pro_b.packet) {
		phhead->RcvFrmIrqCount ++;
		drvdata->rx_int_dinfo.count = phhead->SndFrmOffset;
	}else{
		plhead->RcvIrqCount ++;
		drvdata->rx_int_dinfo.count = plhead->SndDataOffset;
	}
	drvdata->rx_int_dinfo.t = get_clock();
}

static inline u32 get_snd_count(Bridge_Pro *pro,
		struct buf_lbridge_head *plhead, struct buf_hbridge_head *phhead)
{
	if (pro->pro_b.packet)
		return phhead->SndFrmIrqCount;
	else
		return plhead->SndIrqCount;
}

/*-------------------------------------------------------------------------*/
static void comip_bridge_irq_en(unsigned int irq)
{
	enable_irq(irq);
}

static void comip_bridge_irq_dis(unsigned int irq)
{
	disable_irq_nosync(irq);
}

static irqreturn_t comip_bridge_rx_handler(int irq, void *dev_id)
{
	struct bridge_drvdata *drvdata = dev_id;
	struct buf_lbridge_head * plhead= (struct buf_lbridge_head *)drvdata->rxinfo.membase;
	struct buf_hbridge_head * phhead= (struct buf_hbridge_head *)drvdata->rxinfo.membase;

	wake_lock_timeout(&drvdata->wakelock, 1*HZ);
	drvdata->rxinfo.circular_buf.buf_put = get_snd_offset(&drvdata->property, plhead, phhead) * drvdata->unit +
						drvdata->rxinfo.pdata;
	rcv_irq_count_add(drvdata, plhead, phhead);
	if (drvdata->rxhandle) {
		if(!drvdata->rxhandle(drvdata))
			wake_up_interruptible(&drvdata->rxwait_queue);
	}
	return IRQ_HANDLED;
}
static irqreturn_t comip_bridge_ctl_handler(int irq, void *dev_id)
{

	struct bridge_drvdata *drvdata = dev_id;
	struct buf_hbridge_head * phhead = (struct buf_hbridge_head *)drvdata->rxinfo.membase;

	wake_lock_timeout(&drvdata->wakelock, 1*HZ);
	drvdata->tx_ctl_dinfo.t = get_clock();
	drvdata->tx_ctl_dinfo.count = (u32)phhead->RcvFrmOffset;
	if (drvdata->ctlhandle) {
		if(!drvdata->ctlhandle(drvdata))
			wake_up_interruptible(&drvdata->txwait_queue);
	}

	return IRQ_HANDLED;
}
static int comip_bridge_open(struct bridge_drvdata *drvdata)
{
	int status = 0, i = 0;
	struct bridge_rxdata *rxbuf = NULL;
	struct bridge_frame * frame = NULL;

	spin_lock_irq(&(drvdata->op_lock));

	if (drvdata->is_open) {
		BRIDGE_ERR("%s must be reopened.\n", drvdata->name);
		status = -EBUSY;
		goto error_spinlock_unlock;
	}

	status = info_init(drvdata->property, &drvdata->txinfo, drvdata->unit);
	if (status) {
		BRIDGE_ERR("%s txinfo init err.\n", drvdata->name);
		goto error_spinlock_unlock;
	}

	status = info_init(drvdata->property, &drvdata->rxinfo, drvdata->unit);
	if (status) {
		BRIDGE_ERR("%s rxinfo init err.\n", drvdata->name);
		goto error_spinlock_unlock;
	}
	if ((drvdata->property.pro_b.packet) && (!drvdata->f_open)) {
		INIT_LIST_HEAD(&drvdata->rxbuf_list);
		INIT_LIST_HEAD(&drvdata->rxdata_list);
		INIT_LIST_HEAD(&drvdata->rx_list);
		for (i = 0; i < drvdata->rxinfo.pkt_num; i++) {
			frame = (struct bridge_frame *)(drvdata->rxinfo.pdata + i*drvdata->unit);
			rxbuf = kzalloc(sizeof(struct bridge_rxdata), GFP_KERNEL);
			INIT_LIST_HEAD(&rxbuf->list);
			if (!rxbuf) {
				BRIDGE_ERR("rxbuf alloc fail!\n");
				goto error_alloc_rxbuf;
			}
			status = alloc_rx(rxbuf, drvdata, frame);
			if (status)
				goto error_alloc_rxbuf;
		}
		for(i = 0; i < BRIDGE_BUF_COUNT; i++) {
			rxbuf = kzalloc(sizeof(struct bridge_rxdata), GFP_KERNEL);
			INIT_LIST_HEAD(&rxbuf->list);
			if (!rxbuf) {
				BRIDGE_ERR("rxdata alloc fail!\n");
				goto error_alloc_rxbuf;
			}
			list_add_tail(&rxbuf->list,&drvdata->rxdata_list);
		}
		drvdata->rx_save_buf = kzalloc(drvdata->rxinfo.length, GFP_KERNEL);
		if (!rxbuf) {
				BRIDGE_ERR("rx_save_buf alloc fail!\n");
				goto error_alloc_rxbuf;
		}
	}
	wake_lock_init(&drvdata->wakelock, WAKE_LOCK_SUSPEND, drvdata->name);
	status = request_irq(drvdata->rxirq, comip_bridge_rx_handler, IRQF_DISABLED, drvdata->name, drvdata);
	if (status) {
		BRIDGE_ERR("request rxirq failed\n");
		status = -ENXIO;
		goto error_alloc_rxbuf;
	}
	irq_set_irq_wake(drvdata->rxirq, 1);

	if (drvdata->property.pro_b.flow_ctl) {
		struct buf_hbridge_head *phhead = (struct buf_hbridge_head *)drvdata->rxinfo.membase;
		status = request_irq(drvdata->rxctl_irq, comip_bridge_ctl_handler,
					IRQF_DISABLED, drvdata->name, drvdata);
		if (status) {
			BRIDGE_ERR("request ctlirq failed\n");
			status = -ENXIO;
			goto error_alloc_rxbuf;
		}
		irq_set_irq_wake(drvdata->rxctl_irq, 1);
		phhead->RcvNotBusy = 1;
	}
	drvdata->is_open = 1;
	if (drvdata->f_open == 0)
		drvdata->f_open = 1;
	spin_unlock_irq(&(drvdata->op_lock));
	BRIDGE_PRT("open %s ok.\n", drvdata->name);

	return status;
error_alloc_rxbuf:
	while (!list_empty(&drvdata->rxbuf_list)) {
		rxbuf = list_first_entry(&drvdata->rxbuf_list, struct bridge_rxdata, list);
		list_del(&rxbuf->list);
		drvdata->free_rcvbuf(rxbuf);
		kfree(rxbuf);
	}
	while (!list_empty(&drvdata->rxdata_list)) {
		rxbuf = list_first_entry(&drvdata->rxdata_list, struct bridge_rxdata, list);
		list_del(&rxbuf->list);
		kfree(rxbuf);
	}
error_spinlock_unlock:
	spin_unlock_irq(&drvdata->op_lock);
	return status;
}

static bool comip_test_rcvbusy(struct bridge_drvdata *drvdata)
{
	Bridge_Pro *pro = &drvdata->property;
	if (pro->pro_b.flow_ctl) {
		struct buf_hbridge_head *phhead = (struct buf_hbridge_head *)drvdata->txinfo.membase;
		if(phhead->RcvNotBusy)
			return false;
		else
			return true;
	}else
		return false;
}

static ssize_t bridge_send_fcdata_one(struct bridge_drvdata *drvdata, const char *buf,
		size_t count, bool isuser, bool is_last)
{
	struct buf_hbridge_head *phhead = (struct buf_hbridge_head *)drvdata->txinfo.membase;
	Bridge_Pro *pro = &drvdata->property;
	ssize_t status = -EINVAL;
	u32 timeout = drvdata->tx_timeout;

	while (1) {
		if (pro->pro_b.flow_ctl) {
			while (!phhead->RcvNotBusy) {
				if (timeout / BRIDGE_SND_WAIT_TIME) {
					wait_event_interruptible_timeout(drvdata->txwait_queue,
								(1 == phhead->RcvNotBusy), BRIDGE_SND_WAIT_TIME);
					timeout = timeout - BRIDGE_SND_WAIT_TIME;
				}else{
					BRIDGE_INFO("%s:CP busy!\n",drvdata->name);
					if (drvdata->type == HS_IP_PACKET)
						goto send;
					status = -EBUSY;
					goto out;
				}
			}
		}
send:
		drvdata->txinfo.circular_buf.buf_get = phhead->RcvFrmOffset * drvdata->unit
													+ drvdata->txinfo.pdata;
		if (isuser)
			status = user_buf_put_data(drvdata, &drvdata->txinfo.circular_buf, buf, count);
		else
			status = user_buf_put_pkt(drvdata, &drvdata->txinfo.circular_buf, buf, count);
		if (unlikely(status < 0)) {
			BRIDGE_INFO("%s buf put ret:%x rcvoffset:%x sndoffset:%x\n", drvdata->name, (int)status,
							phhead->RcvFrmOffset, phhead->SndFrmOffset);
			if (timeout / BRIDGE_SND_WAIT_TIME) {
				comip_trigger_cp_irq((int)drvdata->txirq);/*trigger irq to ARM0/ZSP and it will trigger tx ack irq*/
				phhead->SndFrmIrqCount++;
				msleep(BRIDGE_SND_WAIT_TIME);
				timeout = timeout - BRIDGE_SND_WAIT_TIME;
				continue;
			}else{
				goto out;
			}
		}else{
			phhead->SndFrmOffset = (unsigned int)(drvdata->txinfo.circular_buf.buf_put
										- (unsigned int)drvdata->txinfo.pdata)
										/drvdata->unit;
			break;
		}
	}
	if (is_last)
		comip_trigger_cp_irq((int)drvdata->txirq);/*trigger irq to ARM0/ZSP and it will trigger tx ack irq*/

	phhead->SndFrmIrqCount++;
	phhead->SndFrmCount += status;

out:
	return status;
}
static ssize_t bridge_send_fcdata(struct bridge_drvdata *drvdata, const char __user *buf,
		size_t count, bool isuser, bool is_last)
{
	ssize_t status = 0;
	u32 cnt = count / drvdata->txinfo.pkt_size;
	u32 rst = count % drvdata->txinfo.pkt_size;
	u32 i = 0;

	for (i = 0;i < cnt;i++) {
		status = bridge_send_fcdata_one(drvdata, buf + i*drvdata->txinfo.pkt_size,
				drvdata->txinfo.pkt_size, isuser, is_last);
		if (unlikely(status < 0))
			return status;
	}
	if (rst > 0) {
		status = bridge_send_fcdata_one(drvdata, buf + cnt*drvdata->txinfo.pkt_size,
				rst, isuser, is_last);
		if (unlikely(status < 0))
			return status;
	}
	return count;
}

static ssize_t bridge_send_rawdata(struct bridge_drvdata *drvdata, const char __user *buf, size_t count)
{
	struct buf_lbridge_head *plhead = (struct buf_lbridge_head *)drvdata->txinfo.membase;
	ssize_t status = -EINVAL;
	u32 timeout = drvdata->tx_timeout;

	while(1) {
		drvdata->txinfo.circular_buf.buf_get = plhead->RcvDataOffset + drvdata->txinfo.pdata;
		status = user_buf_put_data(drvdata, &drvdata->txinfo.circular_buf, buf, count);
		if (unlikely(status < 0)) {
			BRIDGE_INFO("%s buf put ret:%x rcvoffset:%x sndoffset:%x\n", drvdata->name, (int)status,
							plhead->RcvDataOffset, plhead->SndDataOffset);
			if (timeout / BRIDGE_SND_WAIT_TIME) {
				comip_trigger_cp_irq((int)drvdata->txirq);/*trigger irq to ARM0/ZSP and it will trigger tx ack irq*/
				plhead->SndIrqCount++;
				msleep(BRIDGE_SND_WAIT_TIME);
				timeout = timeout - BRIDGE_SND_WAIT_TIME;
				continue;
			}else{
				goto out;
			}
		}else{
			plhead->SndDataOffset = drvdata->txinfo.circular_buf.buf_put - drvdata->txinfo.pdata;
			break;
		}
	}

	comip_trigger_cp_irq((int)drvdata->txirq);/*trigger irq to ARM0/ZSP and it will trigger tx ack irq*/

	plhead->SndIrqCount++;

	if (plhead->SndIrqCount == 1) {
		mdelay(1);
		do{
			if (unlikely(plhead->SndDataOffset != plhead->RcvDataOffset)) {
				BRIDGE_INFO("%s wait Rcv get data!\n", drvdata->name);
				if(drvdata->dbg_info.len) {
					if (drvdata->dbg_info.type == ADDR_MEM) {
						BRIDGE_INFO("%s dbg addr 0x%x value 0x%x\n",
							drvdata->name, drvdata->dbg_info.addr,
							*(u32 *)(drvdata->dbg_info.addr + drvdata->baseaddr));
					}else
						BRIDGE_INFO("%s dbg addr 0x%x value 0x%x\n", drvdata->name,
							drvdata->dbg_info.addr,	readl(io_p2v(drvdata->dbg_info.addr)));
				}else
					BRIDGE_INFO("no dbg info!\n");
				if (timeout / BRIDGE_SND_WAITRDY_TIME) {
					msleep(BRIDGE_SND_WAITRDY_TIME);
					timeout = timeout - BRIDGE_SND_WAITRDY_TIME;
				}
			}else
				break;
		}while (timeout / BRIDGE_SND_WAITRDY_TIME);
	}
	plhead->SndDataCount += count;

out:
	return status;
}

static ssize_t comip_bridge_send(struct bridge_drvdata *drvdata, const char __user *buf,
		size_t count, bool isuser, bool is_last)
{
	ssize_t status = -EINVAL;

	if (0 != check_para(drvdata))
		goto out;

	if (drvdata->property.pro_b.packet) {
		if (count > (drvdata->txinfo.pkt_size * drvdata->txinfo.pkt_num)) {
			BRIDGE_ERR("%s:too many send data count:%d\n", drvdata->name, count);
			goto out;
		}
	} else {
		if (count > drvdata->txinfo.length)
			count = drvdata->txinfo.length - 1;
	}
	if (count == 0) {
		BRIDGE_INFO("%s:count = 0 \n",drvdata->name);
		status = count;
		goto out;
	}
	mutex_lock(&drvdata->write_sem);

	if (drvdata->property.pro)
		status = bridge_send_fcdata(drvdata, buf, count, isuser, is_last);
	else
		status = bridge_send_rawdata(drvdata, buf, count);

	drvdata->tx_dinfo.t = get_clock();
	drvdata->tx_dinfo.count = count;
	mutex_unlock(&drvdata->write_sem);

out:
	return status;

}

static ssize_t comip_bridge_rcv_rawdata(struct bridge_drvdata *drvdata, char __user *buf, size_t count)
{
	ssize_t status = -EINVAL;
	struct buf_lbridge_head * plhead = (struct buf_lbridge_head *)drvdata->rxinfo.membase;

retry:
	status = user_buf_get_data(drvdata,&drvdata->rxinfo.circular_buf, buf, count);
	if (status == 0) {
		status = wait_event_interruptible_timeout(drvdata->rxwait_queue,
					(0 != buf_data_avail(&drvdata->rxinfo.circular_buf, drvdata->unit)),1*HZ);
		if( status == -ERESTARTSYS){
			//BRIDGE_INFO("%s wait_event_interruptible return -ERESTARTSYS\n",drvdata->name);
			return status;
		}
		disable_irq(drvdata->rxirq);
		drvdata->rxinfo.circular_buf.buf_put = plhead->SndDataOffset + drvdata->rxinfo.pdata;
		enable_irq(drvdata->rxirq);
		goto retry;
	}
	plhead->RcvDataCount += status;
	plhead->RcvDataOffset = drvdata->rxinfo.circular_buf.buf_get - drvdata->rxinfo.pdata;

	return status;
}

static ssize_t comip_bridge_rcv_fcdata(struct bridge_drvdata *drvdata, u32 *buf, size_t count, bool isuser)
{
	ssize_t status = -EINVAL;
	struct buf_hbridge_head * phead = (struct buf_hbridge_head *)drvdata->rxinfo.membase;

retry:
	disable_irq(drvdata->rxirq);
	drvdata->rxinfo.circular_buf.buf_put = phead->SndFrmOffset * drvdata->unit
												+ drvdata->rxinfo.pdata;
	enable_irq(drvdata->rxirq);
	status = user_buf_get_pkt(drvdata,&drvdata->rxinfo.circular_buf, buf, count);
	if (status == 0) {
		if (!isuser)
			return status;
		status = wait_event_interruptible_timeout(drvdata->rxwait_queue,
					(0 != buf_data_avail(&drvdata->rxinfo.circular_buf,
					drvdata->unit)),1*HZ);
		if( status == -ERESTARTSYS){
			//BRIDGE_INFO("%s wait_event_interruptible return -ERESTARTSYS\n",drvdata->name);
			return status;
		}
		goto retry;
	}

	phead->RcvFrmCount += status;
	phead->RcvFrmOffset = (unsigned int)(drvdata->rxinfo.circular_buf.buf_get
								- (unsigned int)drvdata->rxinfo.pdata)
								/ drvdata->unit;

	return status;
}

static int comip_bridge_close(struct bridge_drvdata *drvdata)
{
	spin_lock_irq(&(drvdata->op_lock));
	irq_set_irq_wake(drvdata->rxirq, 0);
	free_irq(drvdata->rxirq, drvdata);
	if (drvdata->property.pro_b.flow_ctl) {
		struct buf_hbridge_head *phhead = (struct buf_hbridge_head *)drvdata->rxinfo.membase;
		irq_set_irq_wake(drvdata->rxctl_irq, 0);
		free_irq(drvdata->rxctl_irq, drvdata);
		phhead->RcvNotBusy = 0;
	}
	drvdata->is_open = 0;
	spin_unlock_irq(&(drvdata->op_lock));
	wake_lock_destroy(&drvdata->wakelock);
	BRIDGE_PRT("close %s .\n", drvdata->name);

	return 0;
}

static unsigned int comip_bridge_poll(struct bridge_drvdata *drvdata)
{
	unsigned int mask = 0;

	if (0 != buf_data_avail(&drvdata->rxinfo.circular_buf, drvdata->unit))
		mask |= (POLLIN | POLLRDNORM);

	if (drvdata->property.pro_b.flow_ctl)
		if (0 != buf_space_avail(&drvdata->txinfo.circular_buf, drvdata->unit))
			mask |= (POLLOUT | POLLWRNORM);

	return mask;
}

static ssize_t comip_bridge_info_show(struct bridge_drvdata* drvdata, char *buf)
{
	u64 rxint_t = 0, rx_t = 0, tx_t = 0;
	unsigned long rxint_nt = 0, rx_nt = 0, tx_nt = 0;
	struct timespec ts;
	struct rtc_time tm;

	rxint_t = drvdata->rx_int_dinfo.t;
	rxint_nt = do_div(rxint_t, 1000000000);
	rx_t = drvdata->rx_dinfo.t;
	rx_nt = do_div(rx_t, 1000000000);
	tx_t = drvdata->tx_dinfo.t;
	tx_nt = do_div(tx_t, 1000000000);

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	if (drvdata->property.pro_b.packet && drvdata->property.pro_b.flow_ctl) {
		u64 txint_t = 0;
		unsigned long txint_nt = 0;
		struct buf_hbridge_head *rxphead = (struct buf_hbridge_head *)drvdata->rxinfo.membase;
		struct buf_hbridge_head *txphead = (struct buf_hbridge_head *)drvdata->txinfo.membase;
		txint_t = drvdata->tx_ctl_dinfo.t;
		txint_nt = do_div(txint_t, 1000000000);

		return sprintf(buf, "===================================================\n"
			"%s information:(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n"
			"rxirq=%d txirq=%d rxctl_irq=%d txctl_irq=%d"
			"isopen=%d tx_buf_len=0x%x rx_buf_len=0x%x\n"
			"\n"
			"ap send to cp 0x%x frame(s) data.write frame offset 0x%x\n"
			"ap trigger cp read irq %d times.ap ack ctl clr irq %d times\n"
			"cp notbusy=%d receive 0x%x frame(s) data.read frame offset 0x%x\n"
			"cp ack irq %d times.cp trigger ctl clr irq %d times\n"
			"\n"
			"cp send to ap 0x%x frame(s) data.write frame offset 0x%x\n"
			"cp trigger ap read irq %d times.cp ack ctl clr irq %d times\n"
			"ap notbusy=%d recive 0x%x frame(s) data.read frame offset 0x%x\n"
			"ap ack irq %d times.ap trigger ctl clr irq %d times\n"
			"debug info:\n"
			"rx last int at [%5lu.%06lu], updata offset 0x%x\n"
			"rx last return at [%5lu.%06lu], count 0x%x\n"
			"tx last return at [%5lu.%06lu], count 0x%x\n"
			"tx last ctl clr int at [%5lu.%06lu], count 0x%x\n"
			"===================================================\n",
			drvdata->name,tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,tm.tm_hour,
			tm.tm_min, tm.tm_sec, ts.tv_nsec,
			drvdata->rxirq, drvdata->txirq, drvdata->rxctl_irq, drvdata->txctl_irq,
			drvdata->is_open, drvdata->txinfo.memlength, drvdata->rxinfo.memlength,
			txphead->SndFrmCount, txphead->SndFrmOffset,
			txphead->SndFrmIrqCount, txphead->SndFCIrqCount,
			txphead->RcvNotBusy, txphead->RcvFrmCount, txphead->RcvFrmOffset,
			txphead->RcvFrmIrqCount, txphead->RcvFCIrqCount,
			rxphead->SndFrmCount, rxphead->SndFrmOffset,
			rxphead->SndFrmIrqCount, rxphead->SndFCIrqCount,
			rxphead->RcvNotBusy, rxphead->RcvFrmCount, rxphead->RcvFrmOffset,
			rxphead->RcvFrmIrqCount, rxphead->RcvFCIrqCount,
			(unsigned long)rxint_t, rxint_nt/1000, drvdata->rx_int_dinfo.count,
			(unsigned long)rx_t, rx_nt/1000, drvdata->rx_dinfo.count,
			(unsigned long)tx_t, tx_nt/1000, drvdata->tx_dinfo.count,
			(unsigned long)txint_t, txint_nt/1000, drvdata->tx_ctl_dinfo.count
			);
	}else{
		struct buf_lbridge_head *rxphead= (struct buf_lbridge_head *)drvdata->rxinfo.membase;
		struct buf_lbridge_head *txphead= (struct buf_lbridge_head *)drvdata->txinfo.membase;

		return sprintf(buf, "===================================================\n"
			"%s information:(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n"
			"rxirq=%d txirq=%d isopen=%d tx_buf_len=0x%x rx_buf_len=0x%x\n"
			"\n"
			"ap send to cp 0x%x byte(s) data.write data offset 0x%x\n"
			"ap trigger cp read irq %d times\n"
			"cp receive 0x%x byte(s) data.read data offset 0x%x\n"
			"cp ack irq %d times\n"
			"\n"
			"cp send to ap 0x%x byte(s) data.write data offset 0x%x\n"
			"cp trigger ap read irq %d times\n"
			"ap recive 0x%x byte(s) data.read data offset 0x%x\n"
			"ap ack irq %d times\n"
			"debug info:\n"
			"rx last int at [%5lu.%06lu], updata offset 0x%x\n"
			"rx last return at [%5lu.%06lu], count 0x%x\n"
			"tx last return at [%5lu.%06lu], count 0x%x\n"
			"===================================================\n",
			drvdata->name,tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec,
			drvdata->rxirq, drvdata->txirq, drvdata->is_open,
			drvdata->txinfo.memlength, drvdata->rxinfo.memlength,
			txphead->SndDataCount, txphead->SndDataOffset, txphead->SndIrqCount,
			txphead->RcvDataCount, txphead->RcvDataOffset, txphead->RcvIrqCount,
			rxphead->SndDataCount, rxphead->SndDataOffset, rxphead->SndIrqCount,
			rxphead->RcvDataCount, rxphead->RcvDataOffset, rxphead->RcvIrqCount,
			(unsigned long)rxint_t, rxint_nt/1000, drvdata->rx_int_dinfo.count,
			(unsigned long)rx_t, rx_nt/1000, drvdata->rx_dinfo.count,
			(unsigned long)tx_t, tx_nt/1000, drvdata->tx_dinfo.count
			);
	}

}
static ssize_t comip_bridge_reg_show(struct bridge_drvdata* drvdata, char *buf)
{
	return sprintf(buf, "===================================================\n"
		"TOP_MAIL_CPA7_INTR_EN				:0x%32x\n"
		"TOP_MAIL_CPA7_INTR_SRC_EN0			:0x%32x\n"
		"TOP_MAIL_CPA7_INTR_SRC_EN1			:0x%08x\n"
		"TOP_MAIL_CPA7_INTR_STA0			:0x%08x\n"
		"TOP_MAIL_CPA7_INTR_STA1			:0x%08x\n"
		"TOP_MAIL_CPA7_DSP_INTR_STA_RAW0	:0x%32x\n"
		"TOP_MAIL_CPA7_DSP_INTR_STA_RAW1	:0x%08x\n"
		"TOP_MAIL_CPA7_DSP_INTR_STA_RAW2	:0x%08x\n"
		"TOP_MAIL_CPA7_DSP_INTR_STA_RAW3	:0x%08x\n"
		"===================================================\n",
		readl(io_p2v(TOP_MAIL_CPA7_INTR_EN)),
		readl(io_p2v(TOP_MAIL_CPA7_INTR_SRC_EN0)),
		readl(io_p2v(TOP_MAIL_CPA7_INTR_SRC_EN1)),
		readl(io_p2v(TOP_MAIL_CPA7_INTR_STA0)),
		readl(io_p2v(TOP_MAIL_CPA7_INTR_STA1)),
		readl(io_p2v(TOP_MAIL_CPA7_DSP_INTR_STA_RAW0)),
		readl(io_p2v(TOP_MAIL_CPA7_DSP_INTR_STA_RAW1)),
		readl(io_p2v(TOP_MAIL_CPA7_DSP_INTR_STA_RAW2)),
		readl(io_p2v(TOP_MAIL_CPA7_DSP_INTR_STA_RAW3))
		);
}

static int comip_bridge_driver_init(struct bridge_drvdata *drvdata, struct modem_bridge_info *bridge_info)
{

	drvdata->is_open = 0;
	drvdata->baseaddr = bridge_info->baseaddr;
	drvdata->type = bridge_info->type;
	drvdata->property = bridge_info->property;
	drvdata->rxirq= IRQ_CP(bridge_info->rx_id);
	drvdata->txirq = IRQ_CP(bridge_info->tx_id);
	if (drvdata->property.pro_b.flow_ctl) {
		drvdata->rxctl_irq = IRQ_CP(bridge_info->rx_ctl_id);
		drvdata->txctl_irq = IRQ_CP(bridge_info->tx_ctl_id);
	}
	sprintf(drvdata->name, "%s", bridge_info->name);

	drvdata->txinfo.membase = bridge_info->tx_addr + bridge_info->baseaddr;
	drvdata->txinfo.memlength = bridge_info->tx_len;

	drvdata->rxinfo.membase = bridge_info->rx_addr + bridge_info->baseaddr;
	drvdata->rxinfo.memlength = bridge_info->rx_len;
	memcpy(&drvdata->dbg_info, &bridge_info->dbg_info, sizeof(struct info));

	if (drvdata->property.pro_b.packet) {
		drvdata->txinfo.pkt_num = bridge_info->tx_pkt_maxnum;
		drvdata->txinfo.pkt_size = bridge_info->tx_pkt_maxsize;
		drvdata->rxinfo.pkt_num = bridge_info->rx_pkt_maxnum;
		drvdata->rxinfo.pkt_size = bridge_info->rx_pkt_maxsize;
		drvdata->unit = sizeof(struct bridge_frame);
	}else
		drvdata->unit = NO_FRAME_UNIT;
	mutex_init(&drvdata->write_sem);
	mutex_init(&drvdata->read_sem);
	spin_lock_init(&(drvdata->op_lock));
	init_waitqueue_head(&drvdata->rxwait_queue);
	if (drvdata->property.pro_b.flow_ctl)
		init_waitqueue_head(&drvdata->txwait_queue);

	return 0;
}
static int comip_bridge_driver_uninit(struct bridge_drvdata *drvdata)
{
	if (drvdata->rx_save_buf)
		kfree(drvdata->rx_save_buf);
	return 0;
}

static int comip_bridge_save_rxbuf(struct bridge_drvdata *drvdata)
{
	if (drvdata->f_open && drvdata->rx_save_buf)
		memcpy(drvdata->rx_save_buf, drvdata->rxinfo.pdata, drvdata->rxinfo.length);
	return 0;
}

static int comip_bridge_restore_rxbuf(struct bridge_drvdata *drvdata)
{
	if (drvdata->f_open && drvdata->rx_save_buf)
		memcpy(drvdata->rxinfo.pdata, drvdata->rx_save_buf, drvdata->rxinfo.length);
	return 0;
}

static int comip_bridge_set_flowctl(struct bridge_drvdata *drvdata)
{
	struct buf_hbridge_head *phhead = (struct buf_hbridge_head *)drvdata->rxinfo.membase;

	phhead->RcvNotBusy = 0;
	return 0;
}

static int comip_bridge_clr_flowctl(struct bridge_drvdata *drvdata)
{
	struct buf_hbridge_head *phhead = (struct buf_hbridge_head *)drvdata->rxinfo.membase;

	phhead->RcvNotBusy = 1;
	comip_trigger_cp_irq((int)drvdata->txctl_irq);
	return 0;
}

static const struct comip_bridge_operations bridge_ops = {
	.drvinit = comip_bridge_driver_init,
	.drvuninit = comip_bridge_driver_uninit,
	.rxsavebuf = comip_bridge_save_rxbuf,
	.rxstorebuf = comip_bridge_restore_rxbuf,
	.open = comip_bridge_open,
	.close = comip_bridge_close,
	.poll = comip_bridge_poll,
	.xmit = comip_bridge_send,
	.recv_raw = comip_bridge_rcv_rawdata,
	.recv_pkt = comip_bridge_rcv_fcdata,
	.irq_en = comip_bridge_irq_en,
	.irq_dis = comip_bridge_irq_dis,
	.set_fc = comip_bridge_set_flowctl,
	.clr_fc = comip_bridge_clr_flowctl,
	.info_show = comip_bridge_info_show,
	.reg_show = comip_bridge_reg_show,
	.test_rcvbusy = comip_test_rcvbusy,
};
const struct comip_bridge_operations *get_bridge_ops(void)
{
	return &bridge_ops;
}
EXPORT_SYMBOL(get_bridge_ops);
