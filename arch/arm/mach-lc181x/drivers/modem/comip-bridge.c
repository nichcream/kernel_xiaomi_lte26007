/*
 * LC1810 Bridge core driver
 *
 * Copyright (C) 2011 Leadcore Corporation
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
#include <linux/interrupt.h>
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
#include <asm/cacheflush.h>
#include <plat/hardware.h>
#include <mach/comip-irq.h>
#include <mach/comip-bridge.h>


#define COMIP_BRIDGE_DEBUG	1
#ifdef COMIP_BRIDGE_DEBUG
#define BRIDGE_PRT(format, args...)  printk(KERN_DEBUG "[COMIP Bridge] " format, ## args)
#else
#define BRIDGE_PRT(x...)  do{} while(0)
#endif


static ssize_t bridge_write(struct file *file, const char __user *buf,	size_t count, loff_t *ppos);

/*-------------------------------------------------------------------------*/

/*
 * gs_buf_alloc
 *
 * Allocate a circular buffer and all associated memory.

static int gs_buf_alloc(struct gs_buf *gb, unsigned size)
{
	gb->buf_buf = kmalloc(size, GFP_KERNEL);
	if (gb->buf_buf == NULL)
		return -ENOMEM;

	gb->buf_size = size;
	gb->buf_put = gb->buf_buf;
	gb->buf_get = gb->buf_buf;

	return 0;
}*/

/*
 * gs_buf_init
 *
 * Allocate a circular buffer and all associated memory.
 */
static int gs_buf_init(struct gs_buf *gb,char* buf_base, unsigned size, unsigned put_offset,unsigned get_offset)
{

	if (buf_base == NULL || gb == NULL )
		return -ENOMEM;

	gb->buf_buf = buf_base;
	gb->buf_size = size;
	gb->buf_put = gb->buf_buf+put_offset;
	gb->buf_get = gb->buf_buf+get_offset;

	return 0;
}
#if 0
/*
 * gs_buf_free
 *
 * Free the buffer and all associated memory.
 */
static void gs_buf_free(struct gs_buf *gb)
{
	kfree(gb->buf_buf);
	gb->buf_buf = NULL;
}


/*
 * gs_buf_clear
 *
 * Clear out all data in the circular buffer.
 */
static void gs_buf_clear(struct gs_buf *gb)
{
	gb->buf_get = gb->buf_put;
	/* equivalent to a get of all data available */
}
#endif

/*
 * gs_buf_data_avail
 *
 * Return the number of bytes of data written into the circular
 * buffer.
 */
static unsigned gs_buf_data_avail(struct gs_buf *gb)
{
	return (gb->buf_size + gb->buf_put - gb->buf_get) % gb->buf_size;
}

/*
 * gs_buf_space_avail
 *
 * Return the number of bytes of space available in the circular
 * buffer.
 */
static unsigned gs_buf_space_avail(struct gs_buf *gb)
{
	return (gb->buf_size + gb->buf_get - gb->buf_put - 1) % gb->buf_size;
}

#if 0
/*
 * gs_buf_put
 *
 * Copy data data from a buffer and put it into the circular buffer.
 * Restrict to the amount of space available.
 *
 * Return the number of bytes copied.
 */
static unsigned
gs_buf_put(struct gs_buf *gb, const char *buf, unsigned count)
{
	unsigned len;

	len  = gs_buf_space_avail(gb);
	if (count > len)
		count = len;

	if (count == 0)
		return 0;

	len = gb->buf_buf + gb->buf_size - gb->buf_put;
	if (count > len) {
		memcpy(gb->buf_put, buf, len);
		memcpy(gb->buf_buf, buf+len, count - len);
		gb->buf_put = gb->buf_buf + count - len;
	} else {
		memcpy(gb->buf_put, buf, count);
		if (count < len)
			gb->buf_put += count;
		else /* count == len */
			gb->buf_put = gb->buf_buf;
	}

	return count;
}
#endif

/*
 * gs_user_buf_put
 *
 * Copy data data from a user buffer and put it into the circular buffer.
 * Restrict to the amount of space available.
 *
 * Return the number of bytes copied.
 */
static unsigned
gs_user_buf_put(struct file *file, struct gs_buf *gb, const char __user *buf, unsigned count)
{
	unsigned len;
	unsigned long copy_left_num =0 ;
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	struct comip_bridge_drvdata *drvdata = container_of(misc,
					     struct comip_bridge_drvdata, bridge_dev);

	len  = gs_buf_space_avail(gb);
	if (count > len){
		BRIDGE_PRT( "%s no space want %d avail %d!\n",drvdata->name,count,len);
		return -ENOMEM;
	}

	len = gb->buf_buf + gb->buf_size - gb->buf_put;
	if (count > len) {
		copy_left_num = copy_from_user(gb->buf_put, buf, len);
		if(unlikely(copy_left_num)){
			BRIDGE_PRT( "%s copy from user1 %ld!\n",drvdata->name,copy_left_num);
			return -ENOMEM;
		}
		copy_left_num = copy_from_user(gb->buf_buf, buf+len, count - len);
		if(unlikely(copy_left_num)){
			BRIDGE_PRT( "%s copy from user2 %ld!\n",drvdata->name,copy_left_num);
			return -ENOMEM;
		}
		gb->buf_put = gb->buf_buf + count - len;
	} else {
		copy_left_num = copy_from_user(gb->buf_put, buf, count);
		if(unlikely(copy_left_num)){
			BRIDGE_PRT( "%s copy from user3 %ld!\n",drvdata->name,copy_left_num);
			return -ENOMEM;
		}
		if (count < len)
			gb->buf_put += count;
		else /* count == len */
			gb->buf_put = gb->buf_buf;
	}

	if((drvdata->debug_flg&(1<<start_dump_write_buf_cmd)) && (drvdata->write_file)){
		drvdata->write_file->f_op->write(drvdata->write_file,buf,count,&drvdata->write_file->f_pos);
	}
	return count;
}


/*
 * gs_user_buf_get
 *
 * Get data from the circular buffer and copy to the given user  buffer.
 * Restrict to the amount of data available.
 *
 * Return the number of bytes copied.
 */
static unsigned
gs_user_buf_get(struct file *file, struct gs_buf *gb, char __user *buf, unsigned count)
{
	unsigned len;
	unsigned tmp_len=0;
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	struct comip_bridge_drvdata *drvdata = container_of(misc,
					     struct comip_bridge_drvdata, bridge_dev);

	len = gs_buf_data_avail(gb);
	if (count > len)
		count = len;

	if (count == 0)
		return 0;

	len = gb->buf_buf + gb->buf_size - gb->buf_get;
	if (count > len) {
		tmp_len = copy_to_user(buf, gb->buf_get, len);
		if(tmp_len == 0){
			tmp_len = copy_to_user(buf+len, gb->buf_buf, count - len);/*xk fix me*/
			gb->buf_get = gb->buf_buf + count-tmp_len - len;
		}else{
			count = len;
			gb->buf_get += count - tmp_len;
		}
	} else {
		tmp_len = copy_to_user(buf, gb->buf_get, count);
		if ((count-tmp_len) < len)
			gb->buf_get += count - tmp_len;
		else /* count == len */
			gb->buf_get = gb->buf_buf;
	}

	if(drvdata->debug_flg&(1<<start_read_to_write_cmd)){
		bridge_write(file,buf,(count-tmp_len),NULL);
	}

	if(drvdata->debug_flg & (1<<start_dump_read_buf_cmd)){
		drvdata->read_file->f_op->write(drvdata->read_file,buf,(count-tmp_len),&drvdata->read_file->f_pos);
	}
	return (count-tmp_len);
}

static u64 get_clock(void)
{
	int cpu;
	cpu = smp_processor_id();
	return  cpu_clock(cpu);
}

/*-------------------------------------------------------------------------*/

static irqreturn_t comip_bridge_rx_handler(int irq, void *dev_id)
{

	struct comip_bridge_drvdata *drvdata = dev_id;
	struct GS_BUF_BRIDGE_HEAD * phead= (struct GS_BUF_BRIDGE_HEAD *)drvdata->rxinfo.membase;

	wake_lock_timeout(&drvdata->wakelock, 1*HZ);
	drvdata->rxinfo.circular_buf.buf_put = phead->SenderDataOffset  + (unsigned int)drvdata->rxinfo.pdata;
	phead->ReceiverIrqNumber++;
	drvdata->rx_int_dinfo.t = get_clock();
	drvdata->rx_int_dinfo.count = (u32)phead->SenderDataOffset;
	wake_up_interruptible(&drvdata->rxwait_queue);

	return IRQ_HANDLED;
}

static int bridge_open(struct inode *inode, struct file *file)
{
	int status=0;
	struct miscdevice  * misc;
	struct comip_bridge_drvdata *drvdata;
	struct GS_BUF_BRIDGE_HEAD *phead;

	misc= (struct miscdevice *)(file->private_data);
	drvdata = container_of(misc,struct  comip_bridge_drvdata, bridge_dev);

	if(!drvdata){
		return -EFAULT;
	}

	BRIDGE_PRT("open %s .",drvdata->name);
	spin_lock_irq(&(drvdata->op_lock));

	if (drvdata->is_open) {
		BRIDGE_PRT("%s must be reopened.\n",drvdata->name );
		status = -EBUSY;
		goto error_spinlock_unlock;
	}

	drvdata->txinfo.pdata = (u8 *)(drvdata->txinfo.membase + sizeof(struct GS_BUF_BRIDGE_HEAD));
	drvdata->txinfo.length = drvdata->txinfo.memlength - sizeof(struct GS_BUF_BRIDGE_HEAD);
	/*memset((u8 *)drvdata->txinfo.membase,0,drvdata->txinfo.memlength);*/
	phead = (struct GS_BUF_BRIDGE_HEAD *)drvdata->txinfo.membase;

	status = gs_buf_init(&drvdata->txinfo.circular_buf, drvdata->txinfo.pdata,
				drvdata->txinfo.length, (unsigned)phead->SenderDataOffset,
				(unsigned)phead->ReceiverDataOffset );
	if(status){
		goto error_spinlock_unlock;
	}

	drvdata->rxinfo.pdata = (u8 *)(drvdata->rxinfo.membase + sizeof(struct GS_BUF_BRIDGE_HEAD));
	drvdata->rxinfo.length = drvdata->rxinfo.memlength - sizeof(struct GS_BUF_BRIDGE_HEAD);
	/*memset((u8 *)drvdata->rxinfo.membase,0,drvdata->rxinfo.memlength);*/
	phead = (struct GS_BUF_BRIDGE_HEAD *)drvdata->rxinfo.membase;

	status = gs_buf_init(&drvdata->rxinfo.circular_buf, drvdata->rxinfo.pdata ,
				drvdata->rxinfo.length, (unsigned)phead->SenderDataOffset,
				(unsigned)phead->ReceiverDataOffset );
	if(status){
		goto error_spinlock_unlock;
	}
	drvdata->is_open = 1;
	spin_unlock_irq(&(drvdata->op_lock));
	wake_lock_init(&drvdata->wakelock, WAKE_LOCK_SUSPEND, "bridge_wakeup");
	status = request_irq(drvdata->rxirq, comip_bridge_rx_handler, IRQF_DISABLED, drvdata->name, drvdata);
	if(status){
		printk(KERN_ERR "request rxirq failed\n");
		status = -ENXIO;
		goto error_spinlock_unlock;
	}
	irq_set_irq_wake(drvdata->rxirq, 1);

	BRIDGE_PRT(" ok.\n");

	return status;

error_spinlock_unlock:
	spin_unlock_irq(&drvdata->op_lock);
	BRIDGE_PRT(" failed.\n");
	return status;
}

static ssize_t bridge_write(struct file *file, const char __user *buf,	size_t count, loff_t *ppos)
{
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	struct comip_bridge_drvdata *drvdata = container_of(misc,
					     struct comip_bridge_drvdata, bridge_dev);
	struct GS_BUF_BRIDGE_HEAD *phead = (struct GS_BUF_BRIDGE_HEAD *)drvdata->txinfo.membase;
	ssize_t status;

	if(!drvdata){
		BRIDGE_PRT( "%s:drvdata null!\n",drvdata->name);
		status = -EFAULT;
		goto out;
	}

	if(!drvdata->is_open){
		BRIDGE_PRT( "%s:not open!\n",drvdata->name);
		status = -ENXIO;
		goto out;
	}

	if(count > drvdata->txinfo.length ){
		BRIDGE_PRT( "%s:too many send data!count = %d \n",drvdata->name,count);
		status = -EFAULT;
		goto out;
	}

	if(count == 0){
		BRIDGE_PRT( "%s:count == 0 \n",drvdata->name);
		status = count;
		goto out;
	}
	mutex_lock(&drvdata->write_sem);

	while(1) {
		drvdata->txinfo.circular_buf.buf_get = phead->ReceiverDataOffset +(unsigned int)drvdata->txinfo.pdata;
		status = gs_user_buf_put(file,&drvdata->txinfo.circular_buf, buf, count);
		if(unlikely(status < 0)){
			BRIDGE_PRT( "%s exception:gs_user_buf_put return (%d) .sendalldata(%d)\n",drvdata->name, (int)status,phead->SenderDataCount );
			/*DUMP_BUF(drvdata->txinfo.pdata, count);			*/
			comip_trigger_cp_irq((int)drvdata->txirq);/*trigger irq to ARM0/ZSP and it will trigger tx ack irq*/
			phead->SenderIrqNumber++;
			msleep(50);
			/*DUMP_REG();*/
			continue;
		}else{
			phead->SenderDataOffset = drvdata->txinfo.circular_buf.buf_put - (unsigned int)drvdata->txinfo.pdata;
			break;
		}
	}

	if(unlikely(drvdata->debug_flg & (1<<start_write_to_read_cmd)))
		comip_bridge_rx_handler(0,drvdata);
	else
		comip_trigger_cp_irq((int)drvdata->txirq);/*trigger irq to ARM0/ZSP and it will trigger tx ack irq*/
	phead->SenderIrqNumber++;
	if(phead->SenderIrqNumber== 1){
		mdelay(1);
		while(unlikely((phead->SenderDataOffset != phead->ReceiverDataOffset) && !(drvdata->debug_flg & (1<<start_write_to_read_cmd))) ){
			msleep(500);
			if(drvdata->debug_info){
				BRIDGE_PRT("modem %s is not ready! debug info 0x%x\n",drvdata->name,*(drvdata->debug_info));
			}else{
				BRIDGE_PRT("modem %s is not ready! no debug info\n",drvdata->name);
			}
		}
	}
	phead->SenderDataCount += count;
	drvdata->tx_dinfo.t = get_clock();
	drvdata->tx_dinfo.count = count;
	mutex_unlock(&drvdata->write_sem);

out:
	return status;
}


static ssize_t bridge_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t status = -EFAULT ;
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	struct comip_bridge_drvdata *drvdata = container_of(misc,
					     struct comip_bridge_drvdata, bridge_dev);
	struct GS_BUF_BRIDGE_HEAD * phead= (struct GS_BUF_BRIDGE_HEAD *)drvdata->rxinfo.membase;

	if(!drvdata){
		status = -EFAULT;
		return status;
	}

	if(!drvdata->is_open){
		BRIDGE_PRT( "%s:not open!\n",drvdata->name);
		status = -ENXIO;
		goto error;
	}

	mutex_lock(&drvdata->read_sem);
retry:
	status = gs_user_buf_get(file,&drvdata->rxinfo.circular_buf,buf,count);
	if( status ==0 ){
		status = wait_event_interruptible_timeout(drvdata->rxwait_queue,(0 != gs_buf_data_avail(&drvdata->rxinfo.circular_buf)),1*HZ);
		if( status == -ERESTARTSYS){
			BRIDGE_PRT("%s wait_event_interruptible return -ERESTARTSYS\n",drvdata->name);
			mutex_unlock(&drvdata->read_sem);
			return status;
		}
		disable_irq(drvdata->rxirq);
		drvdata->rxinfo.circular_buf.buf_put = phead->SenderDataOffset  + (unsigned int)drvdata->rxinfo.pdata;
		enable_irq(drvdata->rxirq);
		goto retry;
	}
	phead->ReceiverDataCount += status;
	phead->ReceiverDataOffset = drvdata->rxinfo.circular_buf.buf_get - (unsigned int)drvdata->rxinfo.pdata;
	/*BRIDGE_PRT( "gs_user_buf_get  read count=%d return %d\n", count, status); */
	/*BRIDGE_PRT( "R: %s read ok  len=%d\n", drvdata->name, status);*/
	drvdata->rx_dinfo.t = get_clock();
	drvdata->rx_dinfo.count = status;
	mutex_unlock(&drvdata->read_sem);
error:
	return status;
}

static int bridge_release(struct inode *inode, struct file *file)
{
	struct miscdevice  * misc;
	struct comip_bridge_drvdata *drvdata;

	misc= (struct miscdevice *)(file->private_data);
	drvdata = container_of(misc,struct  comip_bridge_drvdata, bridge_dev);
	if(!drvdata){
		return -EFAULT;
	}
	spin_lock_irq(&(drvdata->op_lock));
	irq_set_irq_wake(drvdata->rxirq, 0);
	free_irq(drvdata->rxirq, drvdata);
	drvdata->is_open = 0;
	spin_unlock_irq(&(drvdata->op_lock));
	wake_lock_destroy(&drvdata->wakelock);

	return 0;
}

static long bridge_ioctl(struct file* file, unsigned int cmd, unsigned long arg){
	int left=0;
	struct miscdevice  * misc;
	struct comip_bridge_drvdata *drvdata;
	void __user *argp = (void __user *)arg;

	misc= (struct miscdevice *)(file->private_data);
	drvdata = container_of(misc,struct  comip_bridge_drvdata, bridge_dev);

	switch (cmd) {
		case GET_BRIDGE_TX_LEN:
			left = copy_to_user(argp,&drvdata->txinfo.length,sizeof(drvdata->txinfo.length));
			break;

		case GET_BRIDGE_RX_LEN:
			left = copy_to_user(argp,&drvdata->txinfo.length,sizeof(drvdata->rxinfo.length));
			break;
	}

	return left;
}
static unsigned int bridge_poll(struct file *file, poll_table *wait){
	struct miscdevice  * misc= (struct miscdevice *)(file->private_data);
	struct comip_bridge_drvdata *drvdata = container_of(misc,
					     struct comip_bridge_drvdata, bridge_dev);

	poll_wait(file, &drvdata->rxwait_queue, wait);
	if(0 != gs_buf_data_avail(&drvdata->rxinfo.circular_buf))
		return (POLLIN | POLLRDNORM);
	else
		return 0;
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

static int bridge_device_init(struct comip_bridge_drvdata *drvdata,Modem_Bridge_Info *bridge_info){
	int retval=0;
	void *membase;

	membase = ioremap(bridge_info->tx_addr, bridge_info->tx_len);
	if ( !membase ){
		printk(KERN_ERR "rx ioremap failed\n");
		retval = -EINVAL;
		goto out;
	}
	drvdata->txinfo.membase = membase;
	drvdata->txinfo.memlength = bridge_info->tx_len;

	membase = ioremap(bridge_info->rx_addr, bridge_info->rx_len);
	if ( !membase ){
		printk(KERN_ERR "rx ioremap failed\n");
		retval = -EINVAL;
		iounmap(drvdata->txinfo.membase);
		goto out;
	}
	drvdata->rxinfo.membase = membase;
	drvdata->rxinfo.memlength = bridge_info->rx_len;
	drvdata->debug_info = (unsigned int __iomem *)bridge_info->debug_info;
out:
	return retval;
}

static ssize_t comip_bridge_info_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t comip_bridge_info_show_attrs(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct comip_bridge_drvdata* drvdata;
	struct GS_BUF_BRIDGE_HEAD * rxphead;
	struct GS_BUF_BRIDGE_HEAD * txphead;
	struct miscdevice  * misc;
	u64 int_t,rx_t,tx_t;
	unsigned long int_nt,rx_nt,tx_nt;
	struct timespec ts;
	struct rtc_time tm;

	misc = dev_get_drvdata(dev);
	drvdata = container_of(misc,struct  comip_bridge_drvdata, bridge_dev);

	int_t = drvdata->rx_int_dinfo.t;
	int_nt = do_div(int_t, 1000000000);
	rx_t = drvdata->rx_dinfo.t;
	rx_nt = do_div(rx_t, 1000000000);
	tx_t = drvdata->tx_dinfo.t;
	tx_nt = do_div(tx_t, 1000000000);

	rxphead= (struct GS_BUF_BRIDGE_HEAD *)drvdata->rxinfo.membase;
	txphead= (struct GS_BUF_BRIDGE_HEAD *)drvdata->txinfo.membase;
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	return sprintf(buf, "===================================================\n"
		"%s information:(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n"
		"rxirq =%d txirq=%d isopen=%d tx_buf_len=0x%x rx_buf_len=0x%x debug_flg=0x%x\n"
		"\n"
		"a9 send to modem 0x%x byte(s) data.write data offset 0x%x\n"
		"a9 trigger modem read irq %d times\n"
		"modem recive 0x%x byte(s) data.read data offset 0x%x\n"
		"modem ack irq %d times\n"
		"\n"
		"modem send to a9 0x%x byte(s) data.write data offset 0x%x\n"
		"modem trigger a9 read irq %d times\n"
		"a9 recive 0x%x byte(s) data.read data offset 0x%x\n"
		"a9 ack irq %d times\n"
		"debug info:\n"
		"rx last int at [%5lu.%06lu], updata offset 0x%x\n"
		"rx last return at [%5lu.%06lu], count 0x%x\n"
		"tx last return at [%5lu.%06lu], count 0x%x\n"
		"===================================================\n",
		drvdata->name,tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec,
		drvdata->rxirq,drvdata->txirq,drvdata->is_open,drvdata->txinfo.memlength,drvdata->rxinfo.memlength,drvdata->debug_flg,
		txphead->SenderDataCount, (u32)txphead->SenderDataOffset,
		txphead->SenderIrqNumber,
		txphead->ReceiverDataCount, (u32)txphead->ReceiverDataOffset,
		txphead->ReceiverIrqNumber,
		rxphead->SenderDataCount, (u32)rxphead->SenderDataOffset,
		rxphead->SenderIrqNumber,
		rxphead->ReceiverDataCount, (u32)rxphead->ReceiverDataOffset,
		rxphead->ReceiverIrqNumber,
		(unsigned long)int_t,int_nt/1000,drvdata->rx_int_dinfo.count,
		(unsigned long)rx_t,rx_nt/1000,drvdata->rx_dinfo.count,
		(unsigned long)tx_t,tx_nt/1000,drvdata->tx_dinfo.count
		);
}

static ssize_t comip_bridge_reg_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t comip_bridge_reg_show_attrs(struct device *dev,
        struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CPU_LC1810)
	return sprintf(buf, "===================================================\n"
		"CTL_ ARM02A9_INTR_STA		:0x%08x\n"
		"CTL_ARM02A9_INTR_EN		:0x%08x\n"
		"CTL_CP2AP_INTR_STA_RAW		:0x%08x\n"
		"CTL_AP2ARM0_INTR_SET_STA	:0x%08x\n"
		"CTL_AP2ARM0_INTR_SET_STA	:0x%08x\n"
		"===================================================\n",
		readl(io_p2v(CTL_ARM02A9_INTR_STA)),
		readl(io_p2v(CTL_ARM02A9_INTR_EN)),
		readl(io_p2v(CTL_CP2AP_INTR_STA_RAW)),
		readl(io_p2v(CTL_AP2ARM0_INTR_SET_STA)),
		readl(io_p2v(CTL_AP2ARM0_INTR_SET_STA))
		);
#elif defined(CONFIG_CPU_LC1813)
	return sprintf(buf, "===================================================\n"
		"CTL_ARM2A7_INTR_STA		:0x%08x\n"
		"CTL_ARM2A7_INTR_EN		:0x%08x\n"
		"CTL_CP2AP_INTR_STA_RAW		:0x%08x\n"
		"CTL_CP2AP_INTR_STA_RAW1	:0x%08x\n"
		"CTL_AP2ARM926_INTR_SET_STA	:0x%08x\n"
		"CTL_AP2ARM926_INTR_SET_STA	:0x%08x\n"
		"===================================================\n",
		readl(io_p2v(CTL_ARM2A7_INTR_STA)),
		readl(io_p2v(CTL_ARM2A7_INTR_EN)),
		readl(io_p2v(CTL_CP2AP_INTR_STA_RAW)),
		readl(io_p2v(CTL_CP2AP_INTR_STA_RAW1)),
		readl(io_p2v(CTL_AP2ARM926_INTR_SET_STA)),
		readl(io_p2v(CTL_AP2ARM926_INTR_SET_STA))
		);
#endif
}


static ssize_t comip_bridge_test_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u32 cmd_id=0;
	struct comip_bridge_drvdata* drvdata;
	struct miscdevice  * misc;

	misc = dev_get_drvdata(dev);
	drvdata = container_of(misc,struct  comip_bridge_drvdata, bridge_dev);

	sscanf(buf, "%d", &cmd_id);
	switch(cmd_id){
		case start_dump_read_buf_cmd:
			if(!drvdata->read_file)
				drvdata->read_file = filp_open("/data/bridge_read_dump.log", O_WRONLY|O_CREAT, 666);
			drvdata->debug_flg |= (1<<start_dump_read_buf_cmd);
			break;
		case start_dump_write_buf_cmd:
			if(!drvdata->write_file)
				drvdata->write_file = filp_open("/data/bridge_write_dump.log", O_WRONLY|O_CREAT, 666);
			drvdata->debug_flg |= (1<<start_dump_write_buf_cmd);
			break;
		case start_write_to_read_cmd:
			drvdata->debug_flg |= (1<<start_write_to_read_cmd);
			break;
		case start_read_to_write_cmd:
			drvdata->debug_flg |= (1<<start_read_to_write_cmd);
			break;
		case stop_dump_read_buf_cmd:
			drvdata->debug_flg &= ~(1<<start_dump_read_buf_cmd);
			if(!drvdata->read_file){
				filp_close(drvdata->read_file,NULL);
				drvdata->read_file = NULL;
			}
			break;
		case stop_dump_write_buf_cmd:
			drvdata->debug_flg &= ~(1<<start_dump_write_buf_cmd);
			if(!drvdata->write_file){
				filp_close(drvdata->write_file,NULL);
				drvdata->write_file = NULL;
			}
			break;
		case stop_write_to_read_cmd:
			drvdata->debug_flg &= ~(1<<start_write_to_read_cmd);
			break;
		case stop_read_to_write_cmd:
			drvdata->debug_flg &= ~(1<<start_read_to_write_cmd);
			break;
	}

	return size;
}

static ssize_t comip_bridge_test_show_attrs(struct device *dev,
              struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR( bridge_info, S_IRWXU | S_IWUSR |S_IROTH,
						comip_bridge_info_show_attrs, comip_bridge_info_store_attrs );
static DEVICE_ATTR( bridge_reg_status, S_IRWXU | S_IWUSR |S_IROTH ,
						comip_bridge_reg_show_attrs, comip_bridge_reg_store_attrs );
static DEVICE_ATTR( bridge_test, S_IRWXU | S_IWUSR |S_IROTH ,
						comip_bridge_test_show_attrs, comip_bridge_test_store_attrs );

struct miscdevice *
register_comip_bridge(Modem_Bridge_Info *bridge_info,struct device *parent,struct file_operations *ops){
	int retval = 0;
	struct comip_bridge_drvdata *drvdata;

	if(!bridge_info || !parent)
		goto out;

	drvdata = kzalloc(sizeof(struct comip_bridge_drvdata), GFP_KERNEL);
	if (!drvdata) {
		printk(KERN_ERR "out of memory\n");
		retval = -ENOMEM;
		goto out;
	}
	drvdata->is_open = 0;
	drvdata->rxirq= IRQ_CP(bridge_info->irq_id);
	drvdata->txirq = drvdata->rxirq;
	sprintf(drvdata->name, "%s", bridge_info->name);


	retval = bridge_device_init(drvdata,bridge_info);
	if(retval < 0){
		goto out_free;
	}

	drvdata->bridge_dev.minor = MISC_DYNAMIC_MINOR;
	drvdata->bridge_dev.parent = parent;
	drvdata->bridge_dev.name = drvdata->name;
	if(!ops)
		drvdata->bridge_dev.fops = &bridges_fops;
	else
		drvdata->bridge_dev.fops = ops;

	/*dev_set_drvdata(drvdata->bridge_dev.this_device,(void *)drvdata);*/
	retval = misc_register(&drvdata->bridge_dev);
	if(retval){
		printk(KERN_ERR "register bridge device %s failed!\n",drvdata->name);
		goto out_free;
	}
	bridge_info->bridge_miscdev=&drvdata->bridge_dev;


	retval = device_create_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_info);
	if(retval < 0 ){
		goto err_create_dev_attr_bridge_irq_num;
	}

	retval = device_create_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_reg_status);
	if(retval < 0 ){
		goto err_create_dev_attr_bridge_irq_status;
	}

	retval = device_create_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_test);
	if(retval < 0 ){
		goto err_create_dev_attr_bridge_reg_status;
	}

	mutex_init(&drvdata->write_sem);
	mutex_init(&drvdata->read_sem);
	spin_lock_init(&(drvdata->op_lock));
	init_waitqueue_head(&drvdata->rxwait_queue);

	BRIDGE_PRT( "%s found! \n", drvdata->name);

	return &drvdata->bridge_dev;

err_create_dev_attr_bridge_reg_status:
	device_remove_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_reg_status);
err_create_dev_attr_bridge_irq_status:
	device_remove_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_info);
err_create_dev_attr_bridge_irq_num:
	misc_deregister(&drvdata->bridge_dev);
out_free:
	kfree(drvdata);
out:
	return NULL;
}

void deregister_comip_bridge(Modem_Bridge_Info *bridge_info){
	struct comip_bridge_drvdata *drvdata;

	if(!bridge_info)
		return;

	if(bridge_info->bridge_miscdev)
		return;

	drvdata = container_of(bridge_info->bridge_miscdev,
					     struct comip_bridge_drvdata, bridge_dev);

	iounmap(drvdata->txinfo.membase);
	iounmap(drvdata->rxinfo.membase);
	device_remove_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_test);
	device_remove_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_info);
	device_remove_file(drvdata->bridge_dev.this_device, &dev_attr_bridge_reg_status);
	misc_deregister(bridge_info->bridge_miscdev);
	kfree(drvdata);
	bridge_info->bridge_miscdev = NULL;
	return;
}

EXPORT_SYMBOL(register_comip_bridge);
EXPORT_SYMBOL(deregister_comip_bridge);
