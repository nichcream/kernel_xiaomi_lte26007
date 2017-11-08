/* Lite-On LTR-558ALS Linux Driver
 *
 * Copyright (C) 2012 Leadcore
 *
 *
 */


#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <plat/ltr558als.h>
#include <plat/lightprox.h>


#define no_cali_prox_thh  180
#define no_cali_prox_thl  120
#define LTR553_PS__dynamic_CALIBERATE 1
#define ALS_AVR_COUNT 8
static int als_times = 0;
static int als_temp[ALS_AVR_COUNT] = {0};
static int first_cycle = 1;


//may adjust later
#define PS_DETECTED_THRES	        100
#define PS_UNDETECTED_THRES	        60
static int ltr55x_chipid;
static int debug = 1;

#define dprintk(level, fmt, arg...) do {                        \
	if (debug >= level)                                     \
		printk(KERN_WARNING fmt , ## arg); } while (0)

#define ltr55xprintk(format, ...) dprintk(1, format, ## __VA_ARGS__)

#define ltr55x_DRV_NAME	"ltr55xals"

struct ltr55x_data {
	struct i2c_client *client;
	struct mutex update_lock;

	int light_event;
	int proximity_event;
//	struct wake_lock proximity_wake_lock;/*----add for proximity wake*/

	struct work_struct		prox_work;
	struct delayed_work		als_work;
	struct workqueue_struct 	*sensor_workqueue;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
	int in_early_suspend;
#endif

	wait_queue_head_t light_event_wait;
	wait_queue_head_t proximity_event_wait;

	unsigned int gluxValue;  /* light lux data*/
	unsigned int prox;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;
	unsigned int enable_als_sensor;
	int light_need_restar_work;

	unsigned int als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int ps_poll_delay;

	unsigned int irq; // irq

	/*threshold*/
#if LTR553_PS__dynamic_CALIBERATE
	int (*ps_dyna_cali)(void);
#endif
	int ps_far;
	int ps_near;
};

struct ltr55x_reg {
	const char *name;
	u8 addr;
	u8 defval;
	u8 curval;
};

/*before init, should be soft reset first*/
static  struct ltr55x_reg ltr553reg_tbl[] = {
	{
		.name   = "ALS_CONTR",
		.addr   = 0x80,
		.defval = 0x00,
		.curval = 0x19,
	},
	{
		.name = "PS_CONTR",
		.addr = 0x81,
		.defval = 0x00,
		.curval = 0x03,/*LTR553_MODE_PS_ON_Gain32*/
	},
	{
		.name = "ALS_PS_STATUS",
		.addr = 0x8c,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "INTERRUPT",
		.addr = 0x8f,
		.defval = 0x00,
		.curval = 0x01,/*active low, only ps interrupt*/
	},
	{
		.name = "PS_LED",
		.addr = 0x82,
		.defval = 0x7f,
		.curval = 0x7B,//0x7f,
	},
	{
		.name = "PS_N_PULSES",
		.addr = 0x83,
		.defval = 0x01,
		.curval = 0x04,
	},
	{
		.name = "PS_MEAS_RATE",
		.addr = 0x84,
		.defval = 0x02,
		.curval = 0x02,
	},
	{
		.name = "ALS_MEAS_RATE",
		.addr = 0x85,
		.defval = 0x03,
		.curval = 0x09,
	},
	{
		.name = "MANUFACTURER_ID",
		.addr = 0x87,
		.defval = 0x05,
		.curval = 0x05,
	},
	{
		.name = "INTERRUPT_PERSIST",
		.addr = 0x9e,
		.defval = 0x00,
		.curval = 0x20, //2 consecutive ps measurement data in the highlight  0x50
	},
	/*PS THRES remain default, should never trigger intr before configure it*/
	{
		.name = "PS_THRES_LOW_0",
		.addr = 0x92,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "PS_THRES_LOW_1",
		.addr = 0x93,
		.defval = 0x00,
		.curval = 0x00,
	},

	{
		.name = "PS_THRES_UP_0",
		.addr = 0x90,
		.defval = 0xff,
		.curval = 0x00,
	},
	{
		.name = "PS_THRES_UP_1",
		.addr = 0x91,
		.defval = 0x07,
		.curval = 0x00,
	},

	{
		.name = "ALS_THRES_LOW_0",
		.addr = 0x99,
		.defval = 0x00,
		.curval = 0x00,//change threshold to top, let all value invoke interrupt, include 0
	},
	{
		.name = "ALS_THRES_LOW_1",
		.addr = 0x9A,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "ALS_THRES_UP_0",
		.addr = 0x97,
		.defval = 0xff,
		.curval = 0xff,//0x00
	},
	{
		.name = "ALS_THRES_UP_1",
		.addr = 0x98,
		.defval = 0xff,
		.curval = 0xff,//0x00
	},
	{
		.name = "ALS_DATA_CH0_0",
		.addr = 0x8a,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "ALS_DATA_CH0_1",
		.addr = 0x8b,
		.defval = 0x00,
		.curval = 0x00,
	},

	{
		.name = "ALS_DATA_CH1_0",
		.addr = 0x88,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "ALS_DATA_CH1_1",
		.addr = 0x89,
		.defval = 0x00,
		.curval = 0x00,
	},

	{
		.name = "PS_DATA_0",
		.addr = 0x8d,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "PS_DATA_1",
		.addr = 0x8e,
		.defval = 0x00,
		.curval = 0x00,
	}
};

static  struct ltr55x_reg ltr558reg_tbl[] = {
	{
		.name   = "ALS_CONTR",
		.addr   = 0x80,
		.defval = 0x00,
		.curval = 0x0b,
	},
	{
		.name = "PS_CONTR",
		.addr = 0x81,
		.defval = 0x00,
		.curval = 0x0f,//change
	},
	{
		.name = "ALS_PS_STATUS",
		.addr = 0x8c,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "INTERRUPT",
		.addr = 0x8f,
		.defval = 0x08,
		.curval = 0x09//0x0b,
	},
	{
		.name = "PS_LED",
		.addr = 0x82,
		.defval = 0x6c,
		.curval = 0x1f,
	},
	{
		.name = "PS_N_PULSES",
		.addr = 0x83,
		.defval = 0x08,
		.curval = 0x4,//0x08
	},
	{
		.name = "PS_MEAS_RATE",
		.addr = 0x84,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "ALS_MEAS_RATE",
		.addr = 0x85,
		.defval = 0x03,
		.curval = 0x03,
	},
	{
		.name = "MANUFACTURER_ID",
		.addr = 0x87,
		.defval = 0x05,
		.curval = 0x05,
	},
	{
		.name = "INTERRUPT_PERSIST",
		.addr = 0x9e,
		.defval = 0x00,
		//.curval = 0x00,//change to 0,used to be 0x23
		.curval = 0x20, //2 consecutive ps measurement data in the highlight  0x50
	},
	{
		.name = "PS_THRES_LOW_0",
		.addr = 0x92,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "PS_THRES_LOW_1",
		.addr = 0x93,
		.defval = 0x0000,
		.curval = 0x0000,
	},

	{
		.name = "PS_THRES_UP_0",
		.addr = 0x90,
		.defval = 0xff,
		.curval = 0x00,
	},
	{
		.name = "PS_THRES_UP_1",
		.addr = 0x91,
		.defval = 0x07,
		.curval = 0x00,
	},

	{
		.name = "ALS_THRES_LOW_0",
		.addr = 0x99,
		.defval = 0x0000,
		.curval = 0x00ff,//change threshold to top, let all value invoke interrupt, include 0
	},
	{
		.name = "ALS_THRES_LOW_1",
		.addr = 0x9A,
		.defval = 0x0000,
		.curval = 0x00ff,
	},
	{
		.name = "ALS_THRES_UP_0",
		.addr = 0x97,
		.defval = 0xff,
		.curval = 0x00,//0xff
	},
	{
		.name = "ALS_THRES_UP_1",
		.addr = 0x98,
		.defval = 0xff,
		.curval = 0x00,//0xff
	},
	{
		.name = "ALS_DATA_CH0_0",
		.addr = 0x8a,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "ALS_DATA_CH0_1",
		.addr = 0x8b,
		.defval = 0x0000,
		.curval = 0x0000,
	},

	{
		.name = "ALS_DATA_CH1_0",
		.addr = 0x88,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "ALS_DATA_CH1_1",
		.addr = 0x89,
		.defval = 0x0000,
		.curval = 0x0000,
	},

	{
		.name = "PS_DATA_0",
		.addr = 0x8d,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "PS_DATA_1",
		.addr = 0x8e,
		.defval = 0x0000,
		.curval = 0x0000,
	}
};

/*
 * Global data
 */
static struct ltr55x_data *ltr55x_gdata;
static int als_gainrange = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ltr55x_early_suspend(struct early_suspend *h);
static void ltr55x_late_resume(struct early_suspend *h);
#endif
static void ltr55x_ps_update(struct ltr55x_data *data);
#if LTR553_PS__dynamic_CALIBERATE
static int ltr55x_ps_dynamic_calibrate(void);
#endif

static int ltr55x_i2c_read_reg(struct i2c_client *client,u8 regnum)
{
	int readdata;

	readdata = i2c_smbus_read_byte_data(client, regnum);
	return readdata;
}

static int ltr55x_i2c_write_reg(struct i2c_client *client, u8 regnum, u8 value)
{
	int writeerror;

	writeerror = i2c_smbus_write_byte_data(client, regnum, value);

	if (writeerror < 0)
		return writeerror;
	else
		return 0;
}
static void ltr55x_set_ps_threshold(u8 addr, u16 value)
{
	ltr55x_i2c_write_reg(ltr55x_gdata->client, addr, (value & 0xff));
	ltr55x_i2c_write_reg(ltr55x_gdata->client, addr + 1, (value >> 8));
}

static int ltr55x_ps_read(struct ltr55x_data *data)
{
	int psval_lo, psval_hi, psdata;

	psval_lo = ltr55x_i2c_read_reg(data->client,LTR55x_PS_DATA_0);
	if (psval_lo < 0) {
		psdata = psval_lo;
		goto out;
	}

	psval_hi = ltr55x_i2c_read_reg(data->client,LTR55x_PS_DATA_1);
	if (psval_hi < 0) {
		psdata = psval_hi;
		goto out;
	}

	psdata = ((psval_hi & 7) << 8) + psval_lo;

	printk(KERN_DEBUG"%s psdata = %d\n",__FUNCTION__,psdata);

out:
	return psdata;
}


// Put PS into Standby mode
static int ltr55xps_shutdown(struct ltr55x_data *data)
{
	int ret;
	printk(KERN_DEBUG "%s\n",__func__);

	ret = ltr55x_i2c_write_reg(data->client, LTR55x_PS_CONTR, MODE_PS_StdBy);
	if (MODE_PS_StdBy != ltr55x_i2c_read_reg(data->client, LTR55x_PS_CONTR))
		ret = ltr55x_i2c_write_reg(data->client,LTR55x_PS_CONTR, MODE_PS_StdBy);
	if (ret)
		printk(KERN_DEBUG "ltr558ps_shutdown failed!\n");
	return ret;
}

// Put PS into startup
static int ltr55xps_startup(struct ltr55x_data *data)
{
	int ret;
	int init_ps_gain;
	int setgain;

	if (LTR553 == ltr55x_chipid) {
		init_ps_gain = LTR553_PS_RANGE16;
		printk(KERN_DEBUG "%s\n",__func__);
		switch (init_ps_gain) {
		case LTR553_PS_RANGE16:
			setgain = LTR553_MODE_PS_ON_Gain16;
			break;
		case LTR553_PS_RANGE32:
			setgain = LTR553_MODE_PS_ON_Gain32;
			break;
		case LTR553_PS_RANGE64:
			setgain = LTR553_MODE_PS_ON_Gain64;
			break;
		default:
			setgain = LTR553_MODE_PS_ON_Gain16;
		break;
		}
	}else if (LTR558 == ltr55x_chipid) {
		init_ps_gain = PS_RANGE4;
		printk(KERN_DEBUG "%s\n",__func__);
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
	} else {
		setgain = MODE_PS_Active;	/*active, gain default*/
	}
	ret = ltr55x_i2c_write_reg(data->client,LTR55x_PS_CONTR, setgain);
       
	if (setgain != ltr55x_i2c_read_reg(data->client, LTR55x_PS_CONTR))
		ret = ltr55x_i2c_write_reg(data->client,LTR55x_PS_CONTR, setgain);

	/*default far away, set thres to detect near*/
	//ltr55x_set_ps_threshold(LTR55x_PS_THRES_LOW_0, 0);
	//ltr55x_set_ps_threshold(LTR55x_PS_THRES_UP_0, data->ps_near);

	msleep(WAKEUP_DELAY);
	if (ret)
		printk(KERN_DEBUG "ltr55xps_startup failed!\n");
	return ret;
}

static int  ltr55x_enable_ps_sensor(struct ltr55x_data *data,unsigned int val)
{
	int ret;
	printk(KERN_DEBUG "%s,val=%x\n",__FUNCTION__, val);
	if ((val != 0) && (val != 1)) {
		printk(KERN_DEBUG "%s:store unvalid value=%x\n", __func__, val);
		return -1;
	}

	if(val == 1) {
		if (data->enable_ps_sensor==0) {
			//data->enable_ps_sensor= 1;
			ltr55xps_startup(data);
#if LTR553_PS__dynamic_CALIBERATE
			ret = ltr55x_ps_dynamic_calibrate();
			if (ret) {
				printk(KERN_DEBUG "dynamic calibrate failture!\n");
				return -EFAULT;
			}
#endif
			enable_irq_wake(data->irq);//enable to wake the system when calling
		}
	} else {
		if (data->enable_ps_sensor==1) {
			//data->enable_ps_sensor = 0;
			disable_irq_wake(data->irq);//disable when finish call
			ltr55xps_shutdown(data);
		}
	}

	return 0;
}

static int ltr55x_als_read(struct ltr55x_data *data,int gainrange)
{
	int alsval_ch0_lo, alsval_ch0_hi;
	int alsval_ch1_lo, alsval_ch1_hi;
	unsigned int alsval_ch0, alsval_ch1;
	int luxdata = 0;
	int ratio;
	int sum = 0;
	int i;

	alsval_ch1_lo = ltr55x_i2c_read_reg(data->client,LTR55x_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr55x_i2c_read_reg(data->client,LTR55x_ALS_DATA_CH1_1);
	alsval_ch1 = (alsval_ch1_hi << 8) + alsval_ch1_lo;

	alsval_ch0_lo = ltr55x_i2c_read_reg(data->client,LTR55x_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr55x_i2c_read_reg(data->client,LTR55x_ALS_DATA_CH0_1);
	alsval_ch0 = (alsval_ch0_hi << 8) + alsval_ch0_lo;

	if (alsval_ch0 == 0 && alsval_ch1 == 0) {
		luxdata = 0;
		return luxdata;//all 0, means dark lux=0, preventing dividing 0
	}
	ratio = alsval_ch1 * 100 / (alsval_ch0 + alsval_ch1);

	if (alsval_ch0 < 60000) {
		if (ratio < 50) {
			luxdata =(17743 * alsval_ch0 + 11059 * alsval_ch1)*8 / (20000*4);
		} else if ((ratio >= 50) && (ratio < 68)) {
			luxdata = (42785 * alsval_ch0 - 19548 * alsval_ch1)*10 / (16666*4);
		} else if ((ratio >= 68) && (ratio < 83)) {
			luxdata = (5926 * alsval_ch0 + 1185 * alsval_ch1)*9 / (40000*4);
		} else if ((ratio >= 83) && (ratio < 90)) {
			luxdata = (5926 * alsval_ch0 + 1185 * alsval_ch1)*3 / (23000*4);
		} else {
			luxdata = 0;
		}
	}
	if ((alsval_ch0 >= 60000) ||(alsval_ch1 >= 60000))
		luxdata = 65536;

	als_temp[als_times] = luxdata;
	als_times++;

	if(first_cycle){
		for(i=0; i<als_times; i++){
			sum+=als_temp[i];
		}
		luxdata = sum / als_times;
	}else{
		for(i=0; i<ALS_AVR_COUNT; i++){
			sum+=als_temp[i];
		}
		luxdata = sum / ALS_AVR_COUNT;
	}

	if(als_times >= ALS_AVR_COUNT){
		als_times = 0;
		first_cycle = 0;
	}

	return luxdata;
}

// Put ALS into Standby mode
static int ltr55xals_shutdown(struct ltr55x_data *data)
{
	int ret;

	ret = ltr55x_i2c_write_reg( data->client,LTR55x_ALS_CONTR, MODE_ALS_StdBy);
	if (MODE_ALS_StdBy != ltr55x_i2c_read_reg(data->client, LTR55x_ALS_CONTR))
		ret = ltr55x_i2c_write_reg(data->client,LTR55x_ALS_CONTR, MODE_ALS_StdBy);
	if (ret)
		printk(KERN_DEBUG "ltr55xals_shutdown failed!\n");
	return ret;
}

// Put ALS into startup
static int ltr55xals_startup(struct ltr55x_data *data)
{
	int ret;
	int als_cntr = 0;

       if(ltr55x_chipid == LTR553) {
		als_gainrange = LTR553_ALS_RANGE_1300;

		switch (als_gainrange)
		{
			case LTR553_ALS_RANGE_64K:
				als_cntr = LTR553_MODE_ALS_ON_Range1;
				break;

			case LTR553_ALS_RANGE_32K:
				als_cntr = LTR553_MODE_ALS_ON_Range2;
				break;

			case LTR553_ALS_RANGE_16K:
				als_cntr = LTR553_MODE_ALS_ON_Range3;
				break;

			case LTR553_ALS_RANGE_8K:
				als_cntr = LTR553_MODE_ALS_ON_Range4;
				break;

			case LTR553_ALS_RANGE_1300:
				als_cntr = LTR553_MODE_ALS_ON_Range5;
				break;

			case LTR553_ALS_RANGE_600:
				als_cntr = LTR553_MODE_ALS_ON_Range6;
				break;

			default:
				als_cntr = LTR553_MODE_ALS_ON_Range1;
				printk(KERN_DEBUG "als sensor gainrange %d!\n", als_gainrange);
				break;
		}

		ret = ltr55x_i2c_write_reg(data->client,LTR55x_ALS_CONTR, als_cntr);
		if (als_cntr != ltr55x_i2c_read_reg(data->client, LTR55x_ALS_CONTR))
		ret = ltr55x_i2c_write_reg(data->client,LTR55x_ALS_CONTR, als_cntr);
       }else if (ltr55x_chipid == LTR558) {
		als_gainrange = ALS_RANGE2_64K;
		switch (als_gainrange)
			{
				case ALS_RANGE2_64K:
					als_cntr = LTR558_MODE_ALS_ON_Range2;
					break;

				case ALS_RANGE1_320:
					als_cntr = LTR558_MODE_ALS_ON_Range1;
					break;

				default:
					als_cntr = LTR558_MODE_ALS_ON_Range2;
					printk(KERN_DEBUG "als sensor gainrange %d!\n", als_gainrange);
					break;
			}

			ret = ltr55x_i2c_write_reg(data->client,LTR55x_ALS_CONTR, als_cntr);
        		if (als_cntr != ltr55x_i2c_read_reg(data->client, LTR55x_ALS_CONTR))
        			ret = ltr55x_i2c_write_reg(data->client,LTR55x_ALS_CONTR, als_cntr);
	} else {
		printk(KERN_DEBUG "%sunknown chip %d\n" ,__FUNCTION__,ltr55x_chipid);
		ret = -1;
	}

	if (ret)
	     printk(KERN_DEBUG "ltr55xps_startup failed!\n");
	return ret;
}

static int ltr55x_enable_als_sensor(struct ltr55x_data * data,unsigned int val)
{
	int i;
	printk(KERN_DEBUG "%s,val=%d\n",__FUNCTION__,val);
	als_times = 0;
	for(i=0;i<ALS_AVR_COUNT;i++){
		als_temp[i] = 0;
	}
	first_cycle = 1;
	if (val == 1) {
		//turn on light  sensor
		if (data->enable_als_sensor==0) {
			data->enable_als_sensor = 1;
			ltr55xals_startup(data);
		}
	} else {
		//turn off light sensor
		if (data->enable_als_sensor==1) {
			data->enable_als_sensor = 0;
			ltr55xals_shutdown(data);
		}
	}
	return 0;
}

/*misc light device  file opreation*/
static int light_open(struct inode *inode, struct file *file)
{
	file->private_data = ltr55x_gdata;

	return 0;
}

/* close function - called when the "file" /dev/light is closed in userspace */
static int light_release(struct inode *inode, struct file *file)
{
	printk(KERN_DEBUG "%s\n",__FUNCTION__);
	return 0;
}
/* read function called when from /dev/light is read */
static ssize_t light_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct ltr55x_data *data = file->private_data;
	if (!data->light_event && (!(file->f_flags & O_NONBLOCK)))
		wait_event_interruptible(data->light_event_wait,data->light_event);
	if (!data->light_event || !buf)
		return 0;

	len = sizeof(data->gluxValue);
	err = copy_to_user(buf, &data->gluxValue, len);
	if (err) {
		printk(KERN_DEBUG "%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}
	data->light_event = 0;
	return len;
}
unsigned int light_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct ltr55x_data * p_ltr55x_data = file->private_data;
	poll_wait(file, &p_ltr55x_data->light_event_wait, poll);
	if (p_ltr55x_data->light_event)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/* ioctl - I/O control */
static long light_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;
	struct ltr55x_data * p_ltr55x_data = file->private_data;

	//printk(KERN_DEBUG "%s\n",__FUNCTION__);

	switch (cmd) {
	case LIGHT_SET_DELAY:
		arg = arg / 1000000;
		if (arg > LIGHT_MAX_DELAY)
			arg = LIGHT_MAX_DELAY;
		else if (arg < LIGHT_MIN_DELAY)
			arg = LIGHT_MIN_DELAY;
		 p_ltr55x_data->als_poll_delay = arg;
		break;
	case LIGHT_SET_ENABLE:
		flag = arg ? 1 : 0;
		mutex_lock(&ltr55x_gdata->update_lock);
		ltr55x_enable_als_sensor(p_ltr55x_data,flag);
		mutex_unlock(&ltr55x_gdata->update_lock);
		cancel_delayed_work_sync(&ltr55x_gdata->als_work);
		if (flag)
			queue_delayed_work(ltr55x_gdata->sensor_workqueue,&ltr55x_gdata->als_work,msecs_to_jiffies(200));
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
	file->private_data = ltr55x_gdata;

	return 0;
}
/* close function - called when the "file" /dev/lightprox is closed in userspace */
static int prox_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* read function called when from /dev/prox is read */
static ssize_t prox_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct ltr55x_data *data = file->private_data;

	if (!data->proximity_event && (!(file->f_flags & O_NONBLOCK)))
		wait_event_interruptible(data->proximity_event_wait,data->proximity_event);

	if (!data->proximity_event || !buf)
		return 0;

	len = sizeof(data->prox);
	err = copy_to_user(buf, &data->prox, len);
	if (err) {
		printk(KERN_DEBUG "%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}

	data->proximity_event = 0;
	return len;

}

unsigned int prox_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct ltr55x_data * p_ltr55x_data = file->private_data;

	poll_wait(file, &p_ltr55x_data->proximity_event_wait, poll);
	if (p_ltr55x_data->proximity_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}
/* ioctl - I/O control */
static long prox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;

	struct ltr55x_data * p_ltr55x_data = file->private_data;

	printk(KERN_DEBUG "%s\n",__FUNCTION__);

	switch (cmd) {
	case PROXIMITY_SET_DELAY:
		arg = arg / 1000000;
		if (arg > PROXIMITY_MAX_DELAY)
			arg = PROXIMITY_MAX_DELAY;
		else if (arg < PROXIMITY_MIN_DELAY)
			arg = PROXIMITY_MIN_DELAY;
		 p_ltr55x_data->ps_poll_delay= arg;
		break;
	case PROXIMITY_SET_ENABLE:
		flag = arg ? 1 : 0;
		mutex_lock(&ltr55x_gdata->update_lock);
		ltr55x_enable_ps_sensor(p_ltr55x_data,flag);
		ltr55x_gdata->prox = 1;
		if (flag && (ltr55x_gdata->enable_ps_sensor == 0)) {
			ltr55x_gdata->enable_ps_sensor = 1;
			ltr55x_ps_update(ltr55x_gdata);
			enable_irq(ltr55x_gdata->irq);
		} else if (!flag && (ltr55x_gdata->enable_ps_sensor)) {
			ltr55x_gdata->enable_ps_sensor = 0;
			disable_irq_nosync(ltr55x_gdata->irq);
		}
		mutex_unlock(&ltr55x_gdata->update_lock);
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

int ltr55x_device_init(struct ltr55x_data *data)
{
	int retval = 0;
	int i;

	if(ltr55x_chipid == LTR553) {
		mdelay(120);
		/*soft reset*/
		ltr55x_i2c_write_reg(data->client, LTR55x_ALS_CONTR, 0x02);
		mdelay(10);
		for (i = 2; i < sizeof(ltr553reg_tbl) / sizeof(ltr553reg_tbl[2]); i++) {
			if (ltr553reg_tbl[i].name == NULL || ltr553reg_tbl[i].addr == 0) {
				break;
			}
			if (ltr553reg_tbl[i].defval != ltr553reg_tbl[i].curval) {
				retval = ltr55x_i2c_write_reg(data->client, ltr553reg_tbl[i].addr, ltr553reg_tbl[i].curval);
			}
		}
		/*after init, should clear intr*/
	} else if (ltr55x_chipid == LTR558) {
		mdelay(PON_DELAY);
		ltr55x_i2c_write_reg(data->client, LTR55x_ALS_CONTR, 0x04);
		mdelay(WAKEUP_DELAY*10);
		for (i = 2; i < sizeof(ltr558reg_tbl) / sizeof(ltr558reg_tbl[2]); i++) {
			if (ltr558reg_tbl[i].name == NULL || ltr558reg_tbl[i].addr == 0) {
				break;
			}
			if (ltr558reg_tbl[i].defval != ltr558reg_tbl[i].curval) {
				retval = ltr55x_i2c_write_reg(data->client, ltr558reg_tbl[i].addr, ltr558reg_tbl[i].curval);
			}
		}
	} else {
		printk(KERN_DEBUG "%s unknown chip %d\n" ,__FUNCTION__,ltr55x_chipid);
		retval = -1;
	}
	return retval;
}
static ssize_t px_threshold_cali_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ps_data;

	mutex_lock(&ltr55x_gdata->update_lock);
	ps_data = ltr55x_ps_read(ltr55x_gdata);
	mutex_unlock(&ltr55x_gdata->update_lock);

	return sprintf(buf, "%d\n", ps_data);
}


static ssize_t px_threshold_cali_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long data;
	int error;
	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	/*offset + threshold: which data is offset*/
	if (data < 100) {
		ltr55x_gdata->ps_far = data + 55;
		ltr55x_gdata->ps_near = data + 90;
	} else if (data < 400) {
		ltr55x_gdata->ps_far = data + 70;
		ltr55x_gdata->ps_near = data + 130;
	} else if (data < 1000) {
		ltr55x_gdata->ps_far = data + 150;
		ltr55x_gdata->ps_near = data + 300;
	} else {
		ltr55x_gdata->ps_far = 1000;
		ltr55x_gdata->ps_near = 2047;
		printk(KERN_DEBUG "proximity structure error \n");
	}

	mutex_lock(&ltr55x_gdata->update_lock);
	ltr55x_set_ps_threshold(LTR55x_PS_THRES_LOW_0, ltr55x_gdata->ps_far);
	ltr55x_set_ps_threshold(LTR55x_PS_THRES_UP_0, ltr55x_gdata->ps_near);
	mutex_unlock(&ltr55x_gdata->update_lock);
	printk(KERN_DEBUG "%s ltr558_gdata->ps_far == %d,ltr55x_gdata->ps_near == %d!\n", __func__,ltr55x_gdata->ps_far,ltr55x_gdata->ps_near);

	return count;
}


static ssize_t ps_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ps_data;

	mutex_lock(&ltr55x_gdata->update_lock);
	ps_data = ltr55x_ps_read(ltr55x_gdata);
	mutex_unlock(&ltr55x_gdata->update_lock);	

	printk(KERN_DEBUG "%s ps_data:%d\n", __func__, ps_data);
	return sprintf(buf, "%d\n", ps_data);
}

static ssize_t als_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int als_data;

	mutex_lock(&ltr55x_gdata->update_lock);
	als_data = ltr55x_als_read(ltr55x_gdata, als_gainrange);
	mutex_unlock(&ltr55x_gdata->update_lock);

	printk(KERN_DEBUG "%s als_data:%d\n", __func__, als_data);
	return sprintf(buf, "%d\n", als_data);

}

static ssize_t ps_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ltr55x_gdata->enable_ps_sensor);
}

static ssize_t ps_enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long data;
	int error;
	unsigned int flag = 0;

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	flag = data ? 1 : 0;
	mutex_lock(&ltr55x_gdata->update_lock);
	ltr55x_enable_ps_sensor(ltr55x_gdata, flag);
	ltr55x_gdata->prox = 1;
	/*enable*/
	if (flag && (ltr55x_gdata->enable_ps_sensor == 0)) {
		ltr55x_gdata->enable_ps_sensor = 1;
		ltr55x_ps_update(ltr55x_gdata);
		enable_irq(ltr55x_gdata->irq);
	} else if (!flag && (ltr55x_gdata->enable_ps_sensor)) {/*disable*/
		ltr55x_gdata->enable_ps_sensor = 0;
		disable_irq_nosync(ltr55x_gdata->irq);
	}
	mutex_unlock(&ltr55x_gdata->update_lock);

	return count;
}

static ssize_t als_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ltr55x_gdata->enable_als_sensor);
}

static ssize_t als_enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long data;
	int error;
	unsigned int flag = 0;
	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	flag = data ? 1 : 0;

	mutex_lock(&ltr55x_gdata->update_lock);
	ltr55x_enable_als_sensor(ltr55x_gdata, flag);
	mutex_unlock(&ltr55x_gdata->update_lock);
	cancel_delayed_work_sync(&ltr55x_gdata->als_work);
	queue_delayed_work(ltr55x_gdata->sensor_workqueue,&ltr55x_gdata->als_work,msecs_to_jiffies(600));

	return count;
}

static ssize_t alsps_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 reg_data[32] = {0,};
	int i = 0;
	ssize_t size = 0;

	mutex_lock(&ltr55x_gdata->update_lock);
	for (i = 0; i < 31; i++) {
		reg_data[i] = ltr55x_i2c_read_reg(ltr55x_gdata->client, 0x80 + i);
		size += snprintf(buf + size, PAGE_SIZE - size, "reg[0x%x] = 0x%x, ", 0x80 + i, reg_data[i]);
	}
	size += snprintf(buf + size, PAGE_SIZE - size, "\n");
	mutex_unlock(&ltr55x_gdata->update_lock);
	return size;
}

static DEVICE_ATTR(ps_threshold_cali, S_IWUSR | S_IWGRP | S_IRUGO,
		px_threshold_cali_show,px_threshold_cali_store);

static DEVICE_ATTR(ps_data, S_IRUGO,
		ps_data_show, NULL);

static DEVICE_ATTR(ps_enable, S_IWUSR | S_IWGRP | S_IRUGO,
		ps_enable_show, ps_enable_store);

static DEVICE_ATTR(als_data, S_IRUGO,
		als_data_show, NULL);


static DEVICE_ATTR(als_enable, S_IWUSR | S_IWGRP | S_IRUGO,
		als_enable_show, als_enable_store);

static DEVICE_ATTR(alsps_reg, S_IRUGO,
		alsps_reg_show, NULL);

#if LTR553_PS__dynamic_CALIBERATE
static int ltr55x_ps_dynamic_calibrate(void)
{
	int i,ps_data,dync_cali_data;
	int count = 4;
	int num_max = 0 , NUM_MAX = 6;
	unsigned long data_sum = 0;

	for (i = 0; i < count; i++){
		// wait for ps_data be stable
		msleep(10);
		ps_data = ltr55x_ps_read(ltr55x_gdata);
		if (ps_data < 0){
			i--;
			continue;
		}
		data_sum += ps_data;

		if (num_max++ > NUM_MAX){
			printk(KERN_DEBUG"ltr553 read data error!\n");
			return -EFAULT;
		}
	}

	dync_cali_data = data_sum/(count);
	printk(KERN_DEBUG "%s:dync_cali_data=%d,cnt=%d\n",__FUNCTION__,dync_cali_data,count);

	/*offset + threshold: which dync_cali is offset*/
	if (dync_cali_data < 120){
		ltr55x_gdata->ps_far = dync_cali_data + 35;
		ltr55x_gdata->ps_near = dync_cali_data + 85;
	}else if (dync_cali_data < 300){
		ltr55x_gdata->ps_far = dync_cali_data + 38;
		ltr55x_gdata->ps_near = dync_cali_data + 78;
	}else if (dync_cali_data < 800){
		ltr55x_gdata->ps_far = dync_cali_data + 42;
		ltr55x_gdata->ps_near = dync_cali_data + 80;
	}else{
		ltr55x_gdata->ps_far = no_cali_prox_thl;
		ltr55x_gdata->ps_near = no_cali_prox_thh;
		printk(KERN_DEBUG "very near or proximity structure fail,set_default_thres\n");
	}

	ltr55x_set_ps_threshold(LTR55x_PS_THRES_LOW_0, ltr55x_gdata->ps_far);
	ltr55x_set_ps_threshold(LTR55x_PS_THRES_UP_0, ltr55x_gdata->ps_near);
	printk(KERN_DEBUG "%s ltr553_gdata->ps_far == %d,ltr553_gdata->ps_near == %d\n", __func__,ltr55x_gdata->ps_far,ltr55x_gdata->ps_near);
	return 0;
}
#endif

static void ltr55x_ps_update(struct ltr55x_data *data)
{
	int tmp_data;

	if ((ltr55x_gdata->enable_ps_sensor == 1)) {
		tmp_data = ltr55x_ps_read(ltr55x_gdata);
		if (tmp_data >= ltr55x_gdata->ps_near/*no_cali_prox_thh*/) {
			ltr55x_gdata->prox = 0;//near
			printk(KERN_DEBUG "ltr55x_gdata->prox=%d\n", ltr55x_gdata->prox);
			ltr55x_set_ps_threshold(LTR55x_PS_THRES_LOW_0, ltr55x_gdata->ps_far/*no_cali_prox_thl*/);
			ltr55x_set_ps_threshold(LTR55x_PS_THRES_UP_0, 0x07ff);
			ltr55x_gdata->proximity_event = 1;
		} else if ((tmp_data <= ltr55x_gdata->ps_far/*no_cali_prox_thl*/)) {
			ltr55x_gdata->prox = 1;//far away
			printk(KERN_DEBUG "ltr558_gdata->prox=%d\n", ltr55x_gdata->prox);
			ltr55x_set_ps_threshold(LTR55x_PS_THRES_LOW_0, 0);
			ltr55x_set_ps_threshold(LTR55x_PS_THRES_UP_0, ltr55x_gdata->ps_near/*no_cali_prox_thh*/);
			ltr55x_gdata->proximity_event = 1;
		} else {
			ltr55x_gdata->proximity_event = 1;
		}
	}
	wake_up_interruptible(&ltr55x_gdata->proximity_event_wait);

}


/*
 * irq
 */
 static void ltr55x_ps_work_func(struct work_struct *work)
{
	//int als_ps_status;

	mutex_lock(&ltr55x_gdata->update_lock);

	//als_ps_status = ltr55x_i2c_read_reg(ltr55x_gdata->client, LTR55x_ALS_PS_STATUS);

	//if (als_ps_status < 0)
	//	goto workout;

	ltr55x_ps_update(ltr55x_gdata);

// workout:
	mutex_unlock(&ltr55x_gdata->update_lock);
	enable_irq(ltr55x_gdata->irq);
}

 static void ltr55x_als_work_func(struct work_struct *work)
{
	int tmp_data;

	mutex_lock(&ltr55x_gdata->update_lock);

	tmp_data = ltr55x_als_read(ltr55x_gdata, als_gainrange);
	if (tmp_data > 50000)
		tmp_data = 50000;
	if ((tmp_data >= 0) && (tmp_data != ltr55x_gdata->gluxValue))
		ltr55x_gdata->gluxValue = tmp_data;

	ltr55x_gdata->light_event = 1;
	wake_up_interruptible(&ltr55x_gdata->light_event_wait);

	mutex_unlock(&ltr55x_gdata->update_lock);

	if(ltr55x_gdata->in_early_suspend){
		printk(KERN_DEBUG "%s--will suspend so do not restar work\r\n",__func__);
		return;
	}

	if(ltr55x_gdata->enable_als_sensor)
		queue_delayed_work(ltr55x_gdata->sensor_workqueue, &ltr55x_gdata->als_work,msecs_to_jiffies(ltr55x_gdata->als_poll_delay));

}

static irqreturn_t ltr55x_irq_handler(int irq, void *dev_id)
{
	/* disable an irq without waiting */
//	disable_irq_nosync(ltr558_gdata->irq);
	disable_irq_nosync(irq);

	/* schedule_work - put work task in global workqueue
	 * @work: job to be done
	 *
	 * Returns zero if @work was already on the kernel-global workqueue and
	 * non-zero otherwise.
	 *
	 * This puts a job in the kernel-global workqueue if it was not already
	 * queued and leaves it in the same position on the kernel-global
	 * workqueue otherwise.
	 */
	 queue_work(ltr55x_gdata->sensor_workqueue, &ltr55x_gdata->prox_work);
	//schedule_work(&ltr55x_gdata->prox_work);

	return IRQ_HANDLED;
}
static int ltr55x_gpio_irq(struct ltr55x_data *data, int gpio)
{
	int ret;

	ret = gpio_request(gpio, ltr55x_DRV_NAME);
	if (ret) {
		printk(KERN_ALERT "%s: LTR-55xALS request gpio failed.\n", __func__);
		return ret;
	}
	ret = gpio_direction_input(gpio);
	if (ret) {
		printk(KERN_ALERT "%s: LTR-55xALS gpio direction output failed.\n", __func__);
		return ret;
	}
	data->irq = gpio_to_irq(gpio);
	ret = request_irq(data->irq, ltr55x_irq_handler, IRQF_TRIGGER_LOW, ltr55x_DRV_NAME, NULL);
	if (ret) {
		printk(KERN_ALERT "%s: LTR-55xALS request irq failed.\n", __func__);
		return ret;
	}
	printk(KERN_DEBUG " %s, irq= %d\n", __func__, data->irq);

	return 0;
}


/*
 * Initialization function
 */
static int ltr55x_init_client(struct i2c_client *client)
{
	/* nothing needs to be done  , ltr558ps.c  init hw */
	int id = 0;

    id = ltr55x_i2c_read_reg(client,LTR55x_MANUFACTURER_ID);
	if (id != 0x5){
		printk(KERN_DEBUG "ltr55x id failed id =%d!\n",id);
		return -1;
	}
    
	id = ltr55x_i2c_read_reg(client,LTR55x_PART_ID);
	if (id == 0x92) 
    	    ltr55x_chipid = LTR553;
	else if(id == 0x80)
	    ltr55x_chipid = LTR558;
       else{
              printk(KERN_DEBUG "ltr55x id fail chip id =%d!\n",id);
              return -1;
       }
	printk(KERN_DEBUG "ltr55x_chipid =0x%x\n",id);
	return 0;
}

/*
 * I2C init/probing/exit functions
 */
static int ltr55x_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ltr55x_data *data;
	struct ltr55x_platform_data *pdata;
	int err = 0;

	ltr55xprintk("enter %s\n",__FUNCTION__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct ltr55x_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit_kfree;
	}
	data->client = client;
	i2c_set_clientdata(client, data);

	ltr55x_gdata = data;
	pdata = client->dev.platform_data;

	data->enable_als_sensor = 0;
	data->enable_ps_sensor = 0;
#if LTR553_PS__dynamic_CALIBERATE
	data->ps_dyna_cali = ltr55x_ps_dynamic_calibrate;
#endif
	data->als_poll_delay = LIGHT_DEFAULT_DELAY;
	data->ps_poll_delay = PROXIMITY_DEFAULT_DELAY;

	/*ps threshold can be update by calibration*/
	data->ps_near = no_cali_prox_thh;
	data->ps_far = no_cali_prox_thl;

	mutex_init(&data->update_lock);
	init_waitqueue_head(&data->light_event_wait);
	init_waitqueue_head(&data->proximity_event_wait);

	/* Initialize the ltr558 chip--read chip id */
	err = ltr55x_init_client(client);
        if (err) {
                printk(KERN_ALERT "%s: LTR-55xALS init_client failed.\n",
                       __func__);
                goto exit_kfree;
        }

	err = ltr55x_device_init(data);
	if (err) {
		printk(KERN_ALERT "%s: LTR-55xALS device init failed.\n",
		       __func__);
		goto exit_kfree;
	}

//	wake_lock_init(&proximity_wake_lock, WAKE_LOCK_SUSPEND, "proximity");
	INIT_WORK((&data->prox_work),ltr55x_ps_work_func);
	INIT_DELAYED_WORK(&data->als_work, ltr55x_als_work_func);
	data->sensor_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!data->sensor_workqueue) {
		goto exit_kfree;
	}

	err = ltr55x_gpio_irq(data,pdata->irq_gpio);
	if (err)
		goto exit_kfree;

	disable_irq_nosync(data->irq);

	if (misc_register(&lightdevice)) {
		printk(KERN_DEBUG "%s : misc light device register falied!\n",__FUNCTION__);
	}

	device_create_file(lightdevice.this_device, &dev_attr_als_data);
	device_create_file(lightdevice.this_device, &dev_attr_als_enable);
	device_create_file(lightdevice.this_device, &dev_attr_alsps_reg);

	if (misc_register(&proxdevice)) {
		printk(KERN_DEBUG "%s : misc proximity device register falied!\n",__FUNCTION__);
	}

	device_create_file(proxdevice.this_device, &dev_attr_ps_threshold_cali);
	device_create_file(proxdevice.this_device, &dev_attr_ps_data);
	device_create_file(proxdevice.this_device, &dev_attr_ps_enable);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ltr55x_gdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ltr55x_gdata->early_suspend.suspend = ltr55x_early_suspend;
	ltr55x_gdata->early_suspend.resume = ltr55x_late_resume;
	register_early_suspend(&ltr55x_gdata->early_suspend);
#endif

	return 0;

exit_kfree:
	kfree(data);
exit:
	return err;
}

static int __exit ltr55x_remove(struct i2c_client *client)
{
	struct ltr55x_data *data = i2c_get_clientdata(client);

	destroy_workqueue(data->sensor_workqueue);
	kfree(data);
	ltr55x_gdata = NULL;
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void ltr55x_early_suspend(struct early_suspend *h)
{
	struct ltr55x_data *data =
		container_of(h, struct ltr55x_data, early_suspend);

	data->in_early_suspend = 1;
	cancel_delayed_work_sync(&data->als_work);

	mutex_lock(&ltr55x_gdata->update_lock);
	if(data->enable_als_sensor) {
		ltr55xals_shutdown(data);
	}
	mutex_unlock(&ltr55x_gdata->update_lock);

}

static void ltr55x_late_resume(struct early_suspend *h)
{
	struct ltr55x_data *data = container_of(h, struct ltr55x_data, early_suspend);

	data->in_early_suspend = 0;

	mutex_lock(&ltr55x_gdata->update_lock);
	if (data->enable_als_sensor) {
		data->light_need_restar_work = 1;
		ltr55xals_startup(data);
	}
	mutex_unlock(&ltr55x_gdata->update_lock);

	if (data->light_need_restar_work) {
		printk(KERN_DEBUG "%s--will restart lux work when resume\r\n",__func__);
		data->light_need_restar_work = 0;
		queue_delayed_work(data->sensor_workqueue,&data->als_work,msecs_to_jiffies(200));
	}
}

#endif /* CONFIG_HAS_EARLYSUSPEND */

static const struct i2c_device_id ltr55x_id[] = {
	{ltr55x_DRV_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ltr55x_id);

static struct i2c_driver ltr55x_driver = {
	.driver = {
		.name	= ltr55x_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= ltr55x_probe,
	.remove	= __exit_p(ltr55x_remove),
	.id_table = ltr55x_id,
};

static int __init ltr55x_init(void)
{
	return i2c_add_driver(&ltr55x_driver);
}

static void __exit ltr55x_exit(void)
{
	i2c_del_driver(&ltr55x_driver);
}

MODULE_DESCRIPTION("ltr55x ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
module_init(ltr55x_init);
module_exit(ltr55x_exit);


