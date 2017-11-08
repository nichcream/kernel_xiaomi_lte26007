/*
 *  ltr558.c - Linux kernel modules for ambient light + proximity sensor
 *
 *  Copyright (C) 2012 leadcore
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>

#include <plat/lightprox.h>

/* LTR-558 Registers */
#define LTR558_ALS_CONTR	0x80
#define LTR558_PS_CONTR		0x81
#define LTR558_PS_LED		0x82
#define LTR558_PS_N_PULSES	0x83
#define LTR558_PS_MEAS_RATE	0x84
#define LTR558_ALS_MEAS_RATE	0x85
#define LTR558_MANUFACTURER_ID	0x87

#define LTR558_INTERRUPT	0x8F
#define LTR558_PS_THRES_UP_0	0x90
#define LTR558_PS_THRES_UP_1	0x91
#define LTR558_PS_THRES_LOW_0	0x92
#define LTR558_PS_THRES_LOW_1	0x93

#define LTR558_ALS_THRES_UP_0	0x97
#define LTR558_ALS_THRES_UP_1	0x98
#define LTR558_ALS_THRES_LOW_0	0x99
#define LTR558_ALS_THRES_LOW_1	0x9A

#define LTR558_INTERRUPT_PERSIST 0x9E

/* 558's Read Only Registers */
#define LTR558_ALS_DATA_CH1_0	0x88
#define LTR558_ALS_DATA_CH1_1	0x89
#define LTR558_ALS_DATA_CH0_0	0x8A
#define LTR558_ALS_DATA_CH0_1	0x8B
#define LTR558_ALS_PS_STATUS	0x8C
#define LTR558_PS_DATA_0	0x8D
#define LTR558_PS_DATA_1	0x8E


/* Basic Operating Modes */
#define MODE_ALS_ON_Range1	0x0B
#define MODE_ALS_ON_Range2	0x03  // 0x33
#define MODE_ALS_StdBy		0x00

#define MODE_PS_ON_Gain1	0x03
#define MODE_PS_ON_Gain2	0x07
#define MODE_PS_ON_Gain4	0x0B
#define MODE_PS_ON_Gain8	0x0C
#define MODE_PS_StdBy		0x00

#define PS_RANGE1 	1
#define PS_RANGE2	2
#define PS_RANGE4 	4
#define PS_RANGE8	8

#define ALS_RANGE1_320	1
#define ALS_RANGE2_64K 	2

/* Power On response time in ms */
#define PON_DELAY	600
#define WAKEUP_DELAY	10

#define PS_DISTANCE 50

static int debug = 1;
#define dprintk(level, fmt, arg...) do {                        \
        if (debug >= level)                                     \
        printk(KERN_WARNING fmt , ## arg); } while (0)

#define ltr558printk(format, ...) dprintk(1, format, ## __VA_ARGS__)

struct ltr558_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct delayed_work	dwork;	/* for PS interrupt */
	struct delayed_work    als_dwork; /* for ALS polling */

	int light_event;
	int proximity_event;
	wait_queue_head_t light_event_wait;   
	wait_queue_head_t proximity_event_wait;  
	unsigned int gluxValue;  /* light lux data*/
	unsigned int final_prox_val;
	unsigned int prox;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;
	unsigned int enable_als_sensor;

	unsigned int als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int ps_poll_delay;
	
	unsigned int irq; // irq 
};

/*
 * Global data
 */
static struct ltr558_data *ltr558_gdata;
int als_gainrange = 0;

static int ltr558_i2c_read_reg(struct i2c_client *client,u8 regnum)
{
	int readdata;

	readdata = i2c_smbus_read_byte_data(client, regnum);
	return readdata;
}

static int ltr558_i2c_write_reg(struct i2c_client *client, u8 regnum, u8 value)
{
	int writeerror;
	
	writeerror = i2c_smbus_write_byte_data(client, regnum, value);

	if (writeerror < 0)
		return writeerror;
	else
		return 0;
}

static int ltr558_ps_read(struct ltr558_data *data)
{
	int psval_lo, psval_hi, psdata;

	psval_lo = ltr558_i2c_read_reg(data->client,LTR558_PS_DATA_0);
	if (psval_lo < 0){
		psdata = psval_lo;
		goto out;
	}
		
	psval_hi = ltr558_i2c_read_reg(data->client,LTR558_PS_DATA_1);
	if (psval_hi < 0){
		psdata = psval_hi;
		goto out;
	}
		
	psdata = ((psval_hi & 7) << 8) + psval_lo;

	ltr558printk("%s psdata = %d\n",__FUNCTION__,psdata);

	out:
	data->final_prox_val = psdata;
	return psdata;
}


// Put PS into Standby mode
static int ltr558ps_shutdown(struct ltr558_data *data)
{
	int ret;
	ret = ltr558_i2c_write_reg(data->client,LTR558_PS_CONTR, MODE_PS_StdBy); 

	if (ret)
		printk("ltr558ps_shutdown failed!\n");
	return ret;
}

// Put PS into startup
static int ltr558ps_startup(struct ltr558_data *data)
{
	int ret;
	int init_ps_gain;
	int setgain;

	// Enable PS to Gain1 at startup
	init_ps_gain = PS_RANGE1;

	switch (init_ps_gain) {
		case PS_RANGE1:
			setgain = MODE_PS_ON_Gain1;
			break;

		case PS_RANGE2:
			setgain = MODE_PS_ON_Gain2;
			break;

		case PS_RANGE4:
			setgain = MODE_PS_ON_Gain4;
			break;

		case PS_RANGE8:
			setgain = MODE_PS_ON_Gain8;
			break;

		default:
			setgain = MODE_PS_ON_Gain1;
			break;
	}

	ret = ltr558_i2c_write_reg(data->client,LTR558_PS_CONTR, setgain); 
	
	ret = ltr558_i2c_write_reg(data->client,LTR558_PS_LED, 0x7b); 
	
	/*LTR558_PS_N_PULSES 0x80  reg light  in work  ,default 0x08*/
	//ret = ltr558ps_i2c_write_reg(data->client,LTR558_PS_N_PULSES, 0x0);  
	
	/*01: Only PS measurement can trigger interrupt*/
	//ret = ltr558ps_i2c_write_reg(data->client,LTR558_INTERRUPT, 0x09); 
	
	/*set PS_THRES_UP_0   PS interrupt upper threshold, lower byte  */
	//ltr558ps_i2c_write_reg(data->client,LTR558_PS_THRES_UP_0, 0x40); 	
	/*set PS_THRES_UP_1  PS interrupt upper threshold, upper byte  */
	//ltr558ps_i2c_write_reg(data->client,LTR558_PS_THRES_UP_1, 0x0); 

 	/* set PS_THRES_LOW_0 */	 
	//ltr558ps_i2c_write_reg(data->client,LTR558_PS_THRES_LOW_0, 0x40); 
	/* set PS_THRES_LOW_1 */	
	//ltr558ps_i2c_write_reg(data->client,LTR558_PS_THRES_LOW_1, 0x0); 

	msleep(WAKEUP_DELAY);

	if (ret)
		printk("ltr558ps_startup failed!\n");
	return ret;
}

static int  ltr558_enable_ps_sensor(struct ltr558_data *data,unsigned int val)
{
	unsigned long flags; 

	ltr558printk("%s: enable ps senosr ( %x)\n",__FUNCTION__, val);
	if ((val != 0) && (val != 1)) {
		printk("%s:store unvalid value=%x\n", __func__, val);
		return 0;
	}
	
	if(val == 1) {
		//turn on p sensor
		if (data->enable_ps_sensor==0) {
			data->enable_ps_sensor= 1;
			ltr558ps_startup(data);
		
			mutex_lock(&data->update_lock);
			cancel_delayed_work_sync(&data->dwork);
			schedule_delayed_work(&data->dwork, msecs_to_jiffies(data->ps_poll_delay));	// 1000ms
			mutex_unlock(&data->update_lock);
		}
	} 
	else {
		//turn off p sensor - kk 25 Apr 2011 we can't turn off the entire sensor, the light sensor may be needed by HAL
		if (data->enable_ps_sensor==1) {
			data->enable_ps_sensor = 0;
			ltr558ps_shutdown(data);
			mutex_lock(&data->update_lock);
			cancel_delayed_work_sync(&data->dwork);
			mutex_unlock(&data->update_lock);
		}
	}
	return 0;
}

static int ltr558_als_read(struct ltr558_data *data,int gainrange)
{
	int alsval_ch0_lo, alsval_ch0_hi;
	int alsval_ch1_lo, alsval_ch1_hi;
	unsigned int alsval_ch0, alsval_ch1;
	int luxdata, ratio;
	
	alsval_ch1_lo = ltr558_i2c_read_reg(data->client,LTR558_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr558_i2c_read_reg(data->client,LTR558_ALS_DATA_CH1_1);
	alsval_ch1 = (alsval_ch1_hi << 8) + alsval_ch1_lo;

	alsval_ch0_lo = ltr558_i2c_read_reg(data->client,LTR558_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr558_i2c_read_reg(data->client,LTR558_ALS_DATA_CH0_1);
	alsval_ch0 = (alsval_ch0_hi << 8) + alsval_ch0_lo;

	ratio = alsval_ch1*100/(alsval_ch0+alsval_ch1);

	if(alsval_ch0 < 60000){
		if(ratio < 45)
		{
		  luxdata =((17743*alsval_ch0 +11059*alsval_ch1)*3)/100000;
		}
	else if((ratio >= 45)&&(ratio < 64))
	{
	   luxdata = ((37725*alsval_ch0-13363*alsval_ch1)*3)/100000;
	}
	else if((ratio >= 64)&&(ratio < 90))
	{
	   luxdata = ((1690*alsval_ch0-169*alsval_ch1)*3)/10000;
	}
	else
	{
	   luxdata = 0;
	}
	}
	else if ((alsval_ch0 >= 60000) ||(alsval_ch1 >= 60000))
	{
	 luxdata = 65536;
	}
	return luxdata;

}

// Put ALS into Standby mode
static int ltr558als_shutdown(struct ltr558_data *data)
{
	int ret;
	ret = ltr558_i2c_write_reg( data->client,LTR558_ALS_CONTR, MODE_ALS_StdBy); 
	if (ret)
		printk("ltr558als_shutdown failed!\n");
	return ret;
}

// Put ALS into startup
static int ltr558als_startup(struct ltr558_data *data)
{
	int init_als_gain;
	int ret;
	
	// Enable ALS to Full Range at startup
	init_als_gain = ALS_RANGE2_64K;
	als_gainrange = init_als_gain;
	
	if (init_als_gain == 1)
		ret = ltr558_i2c_write_reg(data->client,LTR558_ALS_CONTR, MODE_ALS_ON_Range1);
	else if (init_als_gain == 2){
		ret = ltr558_i2c_write_reg(data->client,LTR558_ALS_CONTR, MODE_ALS_ON_Range2);
	}	
	else
		ret = -1;

	if (ret)
		printk("ltr558ps_startup failed!\n");
	return ret;
}

static int ltr558_enable_als_sensor(struct ltr558_data * data,unsigned int val)
{
	unsigned long flags; 
	
	if(val == 1) {
		//turn on light  sensor
		if (data->enable_als_sensor==0) {
			data->enable_als_sensor = 1; 
			ltr558als_startup(data);	
			mutex_lock(&data->update_lock);
			__cancel_delayed_work(&data->als_dwork);
			schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay)); 
			mutex_unlock(&data->update_lock);
		}
	}
	else {
		//turn off light sensor 
		if (data->enable_als_sensor==1) {
			data->enable_als_sensor = 0; 
			ltr558als_shutdown(data);
			mutex_lock(&data->update_lock);
			cancel_delayed_work_sync(&data->als_dwork); 
			mutex_unlock(&data->update_lock);
			}
		}
	return 0;
}

/* ALS polling routine */
static void ltr558_als_polling_work_handler(struct work_struct *work)
{
	struct ltr558_data *data = container_of(work, struct ltr558_data, als_dwork.work);
 	unsigned int luxValue = 0;
	
	luxValue = ltr558_als_read(data,als_gainrange);
	data->gluxValue = luxValue;
	ltr558printk("%s luxvalue = %d\n",__FUNCTION__,luxValue);
	data->light_event = 1;
	wake_up_interruptible(&data->light_event_wait);
	schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	// restart timer
}

/* ps handler */
static void ltr558_work_handler(struct work_struct *work)
{
	struct ltr558_data *data = container_of(work, struct ltr558_data, dwork.work);

	data->final_prox_val = ltr558_ps_read(data);
	ltr558printk("%s , final_prox_val = %d\n",__FUNCTION__,data->final_prox_val);
	data->prox = (data->final_prox_val >= PS_DISTANCE )? 0 : 1;	
	data->proximity_event = 1;
	wake_up_interruptible(&data->proximity_event_wait);
	schedule_delayed_work(&data->dwork, msecs_to_jiffies(data->ps_poll_delay)); // 1000ms
}

/*misc light device  file opreation*/
static int light_open(struct inode *inode, struct file *file)
{
	ltr558printk("enter %s\n",__FUNCTION__);
	file->private_data = ltr558_gdata;
	return 0;
}

/* close function - called when the "file" /dev/light is closed in userspace */
static int light_release(struct inode *inode, struct file *file)
{
	ltr558printk("enter %s\n",__FUNCTION__);
	return 0;
}

/* read function called when from /dev/light is read */
static ssize_t light_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct ltr558_data *data = file->private_data;
	
	if (!data->light_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->light_event_wait,data->light_event);
	}

	if (!data->light_event || !buf)
		return 0;
	
	len = sizeof(data->gluxValue);
	err = copy_to_user(buf, &data->gluxValue, len);
	if (err) {
		printk("%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}
	
	data->light_event = 0;
	return len;
}

unsigned int light_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct ltr558_data * p_ltr558_data = file->private_data;
	
	poll_wait(file, &p_ltr558_data->light_event_wait, poll);
	if (p_ltr558_data->light_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

/* ioctl - I/O control */
static long light_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;
	struct ltr558_data * p_ltr558_data = file->private_data;

	ltr558printk("enter %s\n",__FUNCTION__);

	switch (cmd) {
		case LIGHT_SET_DELAY:
			if (arg > LIGHT_MAX_DELAY)
				arg = LIGHT_MAX_DELAY;
			else if (arg < LIGHT_MIN_DELAY)
				arg = LIGHT_MIN_DELAY;
			 p_ltr558_data->als_poll_delay = arg;
			break;
		case LIGHT_SET_ENALBE:
			flag = arg ? 1 : 0;
			ltr558_enable_als_sensor(p_ltr558_data,flag); 	
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

/* define which file operations are supported */
const struct file_operations light_fops = {
	.owner = THIS_MODULE,
	.read = light_read,
	.poll = light_poll,

	.unlocked_ioctl = light_ioctl,
	.open = light_open,
	.release = light_release,
};

#define LIGHT_DEV_NAME "light"

static struct miscdevice lightdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = LIGHT_DEV_NAME,
	.fops = &light_fops,
};

/*misc proximity device  file opreation*/
static int prox_open(struct inode *inode, struct file *file)
{
	ltr558printk("enter %s\n",__FUNCTION__);
	file->private_data = ltr558_gdata;
	return 0;
}

/* close function - called when the "file" /dev/lightprox is closed in userspace */
static int prox_release(struct inode *inode, struct file *file)
{
	ltr558printk("enter %s\n",__FUNCTION__);
	return 0;
}

/* read function called when from /dev/prox is read */
static ssize_t prox_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct ltr558_data *data = file->private_data;
	
	if (!data->proximity_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->proximity_event_wait,data->light_event);
	}

	if (!data->proximity_event || !buf)
		return 0;
	
	len = sizeof(data->prox);
	err = copy_to_user(buf, &data->prox, len);
	if (err) {
		printk("%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}
	
	data->proximity_event = 0;
	return len;

}

unsigned int prox_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct ltr558_data * p_ltr558_data = file->private_data;

	poll_wait(file, &p_ltr558_data->proximity_event_wait, poll);
	if (p_ltr558_data->proximity_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

/* ioctl - I/O control */
static long prox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;
	
	struct ltr558_data * p_ltr558_data = file->private_data;
	ltr558printk("enter %s\n",__FUNCTION__);

	switch (cmd) {
		case PROXIMITY_SET_DELAY:
			if (arg > PROXIMITY_MAX_DELAY)
				arg = PROXIMITY_MAX_DELAY;
			else if (arg < PROXIMITY_MIN_DELAY)
				arg = PROXIMITY_MIN_DELAY;
			 p_ltr558_data->ps_poll_delay= arg;
			break;
		case PROXIMITY_SET_ENALBE:
			flag = arg ? 1 : 0;
			ltr558_enable_ps_sensor(p_ltr558_data,flag); 	
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

/* define which file operations are supported */
const struct file_operations prox_fops = {
	.owner = THIS_MODULE,
	.read = prox_read,
	.poll = prox_poll,

	.unlocked_ioctl = prox_ioctl,
	.open = prox_open,
	.release = prox_release,
};

#define PROX_DEV_NAME "proximity"

static struct miscdevice proxdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = PROX_DEV_NAME,
	.fops = &prox_fops,
};

/*
 * Initialization function
 */
static int ltr558_init_client(struct i2c_client *client)
{
	/* nothing needs to be done  , ltr558ps.c  init hw */
	unsigned char id = 0;

	id = ltr558_i2c_read_reg(client,LTR558_MANUFACTURER_ID);
	if (id != 0x5){
		printk("ltr558 id failed id =%x!\n",id);
		return -1;
	}
	printk("ltr558 id =%x\n",id);
	return 0;
}

#define ltr558_DRV_NAME	"ltr558"

/*
 * I2C init/probing/exit functions
 */
static int __devinit ltr558_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ltr558_data *data;
	int err = 0;

	ltr558printk("enter %s\n",__FUNCTION__);
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct ltr558_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	data->client = client;
	i2c_set_clientdata(client, data);

	data->enable_als_sensor = 0;
	data->als_poll_delay = 1000;	// default to 1000ms
	data->enable_ps_sensor = 0;
	data->ps_poll_delay = 300;

	ltr558_gdata = data;
	
	 mutex_init(&data->update_lock);
	INIT_DELAYED_WORK(&data->dwork, ltr558_work_handler);
	INIT_DELAYED_WORK(&data->als_dwork, ltr558_als_polling_work_handler); 
	init_waitqueue_head(&data->light_event_wait);
	init_waitqueue_head(&data->proximity_event_wait);

	/* Initialize the ltr558 chip */
	err = ltr558_init_client(client);
	if (err)
		goto exit_kfree;
/*
	if(client->irq){
		data->irq = gpio_to_irq(client->irq);
	}
	
	if(data->irq){
		if (request_irq(data->irq, ltr558_interrupt, IRQF_DISABLED|IRQ_TYPE_EDGE_FALLING,
			ltr558_DRV_NAME, (void *)client)) {
			printk("%s Could not allocate ltr558_INT !\n", __func__);
		
			goto exit_kfree;
		}
	}
*/
	if (misc_register(&lightdevice)) {
		printk("%s : misc light device register falied!\n",__FUNCTION__);
	} 
	
	if (misc_register(&proxdevice)) {
		printk("%s : misc proximity device register falied!\n",__FUNCTION__);
	} 
	printk("%s support ver.  enabled\n",__FUNCTION__);

	return 0;
	
	//free_irq(data->irq, client);	
exit_kfree:
	kfree(data);
exit:
	return err;
}

static int __devexit ltr558_remove(struct i2c_client *client)
{
	struct ltr558_data *data = i2c_get_clientdata(client);
	kfree(data);
	return 0;
}

#ifdef CONFIG_PM

static int ltr558_suspend(struct i2c_client *client, pm_message_t mesg)
{
	/*als nothing  to do */
	return 0;
}

static int ltr558_resume(struct i2c_client *client)
{
	/*als nothing  to do */
	return 0;
}

#else

#define ltr558_suspend	NULL
#define ltr558_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id ltr558_id[] = {
	{ltr558_DRV_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ltr558_id);

static struct i2c_driver ltr558_driver = {
	.driver = {
		.name	= ltr558_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend = ltr558_suspend,
	.resume	= ltr558_resume,
	.probe	= ltr558_probe,
	.remove	= __devexit_p(ltr558_remove),
	.id_table = ltr558_id,
};

static int __init ltr558_init(void)
{
	return i2c_add_driver(&ltr558_driver);
}

static void __exit ltr558_exit(void)
{
	i2c_del_driver(&ltr558_driver);
}

MODULE_DESCRIPTION("ltr558 ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
module_init(ltr558_init);
module_exit(ltr558_exit);

