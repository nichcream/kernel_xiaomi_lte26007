/*******************************************************************************
*                                                                             	 *
*   File Name:    gp2ap030.c                                                     *
*   Description:   Linux device driver for gp2ap030 ambient light and            *
*   proximity sensors.                                                        	 *
*   Author:         leadcore                                             	 *
*   History:   2014-02-019     Initial creation                                  *
*                                                                                *
********************************************************************************
*  									         *
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/workqueue.h>

#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/miscdevice.h>

#include <plat/lightprox.h>
#include <plat/gp2ap030.h>

#include <asm/gpio.h>


#define GP2AP030_DEBUG
#if defined GP2AP030_DEBUG
#define GP2AP030_PRINT(fmt, args...) printk("[gp2ap030]--" fmt, ##args)
#else
#define GP2AP030_PRINT(fmt, args...)
#endif


struct gp2ap030_data {
	struct i2c_client		*client;
	struct gp2ap030_platform_data 	*gp2ap030_cfg_data;
	struct work_struct		prox_work;
	struct delayed_work		als_work;
	struct workqueue_struct 	*sensor_workqueue;
	struct miscdevice 		light_dev;
	struct miscdevice 		prox_dev;
	wait_queue_head_t 		light_event_wait;
	wait_queue_head_t 		proximity_event_wait;

	struct early_suspend		early_suspend;
	int 				in_early_suspend;

	u8				reg_data[12];

	struct mutex			mutex;

	unsigned int			light_lux;
	int				light_mode;
	int 				light_event;
	int 				light_poll_delay;
	int             		light_need_restar_work;

	int 				prox_distance;
	int 				prox_event;
	int 				prox_poll_delay;
	u32				prox_calibration;
	spinlock_t 			irq_lock;
	int 				irq;
	int 				irq_gpio;

	unsigned long 			gp2ap030_on;/*represent ALS enabled or PROX enabled or both*/
};
static struct gp2ap030_data 		*gp2ap030_datap;
static struct i2c_client 		*this_client = NULL;

static u8 gp2ap_init_data[12] = {
	/* Reg0 shutdown */
	0x00,
	/* Reg1 PRST:01 RES_A:100 RANGE_A:011 */
	( PRST_4 | RES_A_14 | RANGE_A_8 ),
	/* Reg2 INTTYPE:0 RES_P:011 RANGE_P:010 */
	( INTTYPE_L | RES_P_10 | RANGE_P_4 ),
	/* Reg3 INTVAL:0 IS:11 PIN:11 FREQ:0 */
	( INTVAL_0 | IS_130 | PIN_DETECT | FREQ_327_5 ),
	/* Reg.4 TL[7:0]:0x00 */
	0x00,
	/* Reg.5 TL[15:8]:0x00 */
	0x00,
	/* Reg.6 TH[7:0]:0x00 */
	0xff,
	/* Reg.7 TH[15:8]:0x00 */
	0xff,
	/* Reg.8 PL[7:0]:0x0B */
	0x0B,
	/* Reg.9 PL[15:8]:0x00 */
	0x00,
	/* Reg.A PH[7:0]:0x0C */
	0x0C,
	/* Reg.B PH[15:8]:0x00 */
	0x00
};

static char *strtok(char *s1, char *s2)
{
	static char *str;
	char *start;

	if (s1 != NULL)	{
		str = s1;
	}
	start = str;

	if (str == NULL) {
		return NULL;
	}

	if (s2 != NULL) {
		str = strstr(str, s2);
		if (str != NULL) {
			*str = 0x00;
			str += strlen(s2);
		}
	} else {
		str = NULL;
	}
	return start;
}

static int gp2ap030_i2c_write_reg(u8 addr, u8 para)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[2];

	buf[0] = addr;
	buf[1] = para;
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = buf;
	ret = i2c_transfer(this_client->adapter, msgs, 1);
	if (ret < 0) {
		printk("gp2ap030_i2c_write_reg: transfer error: %d\n", ret);
		return -1;
	}

    return 0;
}
static int gp2ap030_i2c_read_reg(u8 addr, u8 *pdata, int len)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[2];

	buf[0] = addr;    //register address
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;
	msgs[1].addr = this_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = pdata;

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0) {
		printk("gp2ap030_i2c_read_reg: transfer error: %d\n", ret);
		return -1;
	}

	return 0;
}

static int gp2ap030_id_check(void)
{
	u8 device_id = 0;
	int ret;
	ret = gp2ap030_i2c_read_reg(GP2AP030_ID_REG, &device_id, 1);
	if (ret < 0)
		return ret;
	GP2AP030_PRINT("%s: get gp2ap030 id = %x\n", __func__, device_id);

	//if (device_id == 0x21)
	device_id &= 0xf0;
	if (device_id == 0x20)//0x2x
		return 0;
	return device_id;
}

static int gp2ap030_init_device(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gp2ap030_datap->reg_data); i++) {
		if (gp2ap030_i2c_write_reg(i, gp2ap030_datap->reg_data[i])) {
			GP2AP030_PRINT("%s failed\n", __func__);
			return -1;
		}
	}

	return 0;
}

static void gp2ap030_restart_device(void)
{
	u8 value;

	if ((gp2ap030_datap->gp2ap030_on & PROX_ENABLED) && (gp2ap030_datap->gp2ap030_on & ALS_ENABLED)) {
		value = (OP_RUN | OP_CONTINUOUS | OP_PS_ALS | INT_NOCLEAR);
		gp2ap030_i2c_write_reg( REG_ADR_00, value);
	}
	else if ((!(gp2ap030_datap->gp2ap030_on & PROX_ENABLED)) && (gp2ap030_datap->gp2ap030_on & ALS_ENABLED)) {
		value = (OP_RUN | OP_CONTINUOUS | OP_ALS | INT_NOCLEAR);
		gp2ap030_i2c_write_reg( REG_ADR_00, value);
	}
	else if ((gp2ap030_datap->gp2ap030_on & PROX_ENABLED) && (!(gp2ap030_datap->gp2ap030_on & ALS_ENABLED))) {
		value = (OP_RUN | OP_CONTINUOUS | OP_PS | INT_NOCLEAR);
		gp2ap030_i2c_write_reg( REG_ADR_00, value);
	}
}

/* *****************************************************************************
		Light Sensor on/off function
***************************************************************************** */
static int als_onoff( int onoff)
{
	u8 value;
	u8 rdata;

	GP2AP030_PRINT("light_sensor onoff = %d\n", onoff);

	if (onoff) {
		if (!(gp2ap030_datap->gp2ap030_on & PROX_ENABLED)) {
			value = RST ;	// RST
			gp2ap030_i2c_write_reg(REG_ADR_03, value);
			gp2ap030_init_device();
			value = (INTVAL_0 | IS_130 | PIN_DETECT | FREQ_327_5);
			gp2ap030_i2c_write_reg(REG_ADR_03, value);
			value = (OP_RUN | OP_CONTINUOUS | OP_ALS);	// ALS mode
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
		} else {
			value = (OP_SHUTDOWN | INT_NOCLEAR); // shutdown
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
			gp2ap030_i2c_read_reg(REG_ADR_01, &rdata, sizeof(rdata));
			value = (u8)(RES_A_14 | RANGE_A_8) | (rdata & 0xC0);
			gp2ap030_i2c_write_reg(REG_ADR_01, value);
			value = (INTVAL_0 | IS_130 | PIN_DETECT | FREQ_327_5);
			gp2ap030_i2c_write_reg(REG_ADR_03, value);
			value = (OP_RUN | OP_CONTINUOUS | OP_PS_ALS | INT_NOCLEAR);	// ALS & PS mode
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
		}
	} else {
		if (!(gp2ap030_datap->gp2ap030_on & PROX_ENABLED)) {
			value = (OP_SHUTDOWN | INT_NOCLEAR); // shutdown
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
		} else {
			value = (OP_SHUTDOWN | INT_NOCLEAR); // shutdown
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
			value = ( INTVAL_16 | IS_130 | PIN_DETECT | FREQ_327_5);
			gp2ap030_i2c_write_reg(REG_ADR_03, value);
			value = ( OP_RUN | OP_CONTINUOUS | OP_PS | INT_NOCLEAR);	// PS mode
			gp2ap030_i2c_write_reg( REG_ADR_00, value);
		}
	}
	return 0 ;
}

/* *****************************************************************************
		Device file Light
***************************************************************************** */

static int lightsensor_enable(void)
{
	if (gp2ap030_datap->gp2ap030_on & ALS_ENABLED) {
		printk(KERN_INFO "lightsensor already enable!\n");
		return 0;
	}

	als_onoff(1);

    return 0;
}

static int lightsensor_disable(void)
{

	if (!(gp2ap030_datap->gp2ap030_on & ALS_ENABLED)){
		printk(KERN_INFO "lightsensor already disable!! \n");
		return 0;
	}

	als_onoff(0);

	return 0;
}

/* *****************************************************************************
		Proximity Sensor on/off function
***************************************************************************** */
static int ps_onoff(u8 onoff)
{
	u8 value;
	u8 rdata;

	GP2AP030_PRINT("proximity_sensor onoff = %d\n", onoff);

	if (onoff) {
		if (!(gp2ap030_datap->gp2ap030_on & ALS_ENABLED)) {
			value = RST ;	// RST
			gp2ap030_i2c_write_reg( REG_ADR_03, value);
			gp2ap030_init_device();
			value = (INTVAL_16 | IS_130 | PIN_DETECT | FREQ_327_5);
			gp2ap030_i2c_write_reg( REG_ADR_03, value);
			value = (OP_RUN | OP_CONTINUOUS | OP_PS);	// PS mode
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
		} else {
			value = (OP_SHUTDOWN | INT_NOCLEAR);	// shutdown
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
			gp2ap030_i2c_read_reg(REG_ADR_01, &rdata, sizeof(rdata));
			value = (u8)PRST_4 | (rdata & 0x3F);
			gp2ap030_i2c_write_reg(REG_ADR_01, value);
			value = (INTVAL_0 | IS_130 | PIN_DETECT | FREQ_327_5);
			gp2ap030_i2c_write_reg(REG_ADR_03, value);
			value = (OP_RUN | OP_CONTINUOUS | OP_PS_ALS);	// ALS & PS mode
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
		}
	} else {
		if (!(gp2ap030_datap->gp2ap030_on & ALS_ENABLED)) {
			value = (OP_SHUTDOWN | INT_NOCLEAR);	// shutdown
			gp2ap030_i2c_write_reg( REG_ADR_00, value);
		} else {
			value = (OP_SHUTDOWN | INT_NOCLEAR);	// shutdown
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
			value = (INTVAL_0 | IS_130 | PIN_DETECT | FREQ_327_5);
			gp2ap030_i2c_write_reg(REG_ADR_03, value);
			value = (OP_RUN | OP_CONTINUOUS | OP_ALS);	// ALS mode
			gp2ap030_i2c_write_reg(REG_ADR_00, value);
		}
	}

	return 0;
}


/* *****************************************************************************
		Device file PS
***************************************************************************** */
static int psensor_enable(void)
{
	if (gp2ap030_datap->gp2ap030_on & PROX_ENABLED) {
		printk( KERN_INFO "ps_sensor already enable!\n");
		return 0;
	}

	ps_onoff(1);

    return 0;
}

static int psensor_disable(void)
{
	int ret = -EIO;

	if (!(gp2ap030_datap->gp2ap030_on & PROX_ENABLED)) {
		printk(KERN_INFO "ps_sensor already disable!\n");
		return 0;
	}

	ps_onoff(0);

	return ret;
}

static unsigned int gp2ap030_als_get_data(void)
{
	u32 data_als0;
	u32 data_als1;
	u32 lux;
	u32 ratio;
	u8  value;
	u8  rdata[4] = {0};
	u8  als_hi_low=1;

	//gp2ap030_i2c_read_reg(REG_ADR_0C, rdata, sizeof(rdata));
	//data_als0 = (rdata[1] << 8) | rdata[0];
	//gp2ap030_i2c_read_reg(REG_ADR_0E, rdata, sizeof(rdata));
	//data_als1 = (rdata[1] << 8) | rdata[0];

	gp2ap030_i2c_read_reg(REG_ADR_0C, rdata, sizeof(rdata));

	data_als0 = rdata[0] + rdata[1] * 256;
	data_als1 = rdata[2] + rdata[3] * 256;

	if (gp2ap030_datap->light_mode == HIGH_LUX_MODE)
		als_hi_low = 16;
	else
		als_hi_low = 1;

	//GP2AP030_PRINT("read sensor clear=%d,ir=%d\n", data_als0, data_als1);

#if 0
	if (data->als_mode == HIGH_LUX_MODE)
	{
		data_als0 = 16 * data_als0 ;
		data_als1 = 16 * data_als1 ;
	}
#endif
	/* Lux Calculation */
	if (data_als0 == 0) {
		ratio = 100;
	} else {
		ratio = (data_als1 * 100) / data_als0;
	}

	if (((data_als0 == 0) || (data_als1 == 0))
			&& ((data_als0 < 10) && (data_als1 < 10))
				&& (gp2ap030_datap->light_mode == LOW_LUX_MODE)) {
		lux = 0;
	} else if (ratio <= 41) {
		//lux = 0.423 * (1.4 * data_als0);
		lux = (als_hi_low * 423 * ((1400 * data_als0) / 1000)) / 1000;
	} else if (ratio <= 52)	{
		//lux = 0.423 * (4.57 * data_als0 - 7.8 * data_als1);
		lux = (als_hi_low * 423 * ((4570 * data_als0 - 7800 * data_als1) / 1000)) / 1000;
	} else if (ratio <= 90)	{
		//lux = 0.423 * (1.162 * data_als0 - 1.249 * data_als1);
		lux = (als_hi_low * 423 * ((1162 * data_als0 - 1249 * data_als1) / 1000)) / 1000;
	} else {
		lux = gp2ap030_datap->light_lux;
	}
	//data->als_lux_prev = lux;

	/* Lux mode (Range) change */
	if ((data_als0 >= 16000) && (gp2ap030_datap->light_mode == LOW_LUX_MODE)) {
		gp2ap030_datap->light_mode = HIGH_LUX_MODE;
		GP2AP030_PRINT("change lux mode high!\n");
		value = (OP_SHUTDOWN | INT_NOCLEAR);
		gp2ap030_i2c_write_reg(REG_ADR_00, value);
		gp2ap030_i2c_read_reg(REG_ADR_01, rdata, 1);
		value = (u8)(rdata[0] & 0xC0) | (RES_A_14 | RANGE_A_128);
		gp2ap030_i2c_write_reg(REG_ADR_01, value);
		if (gp2ap030_datap->gp2ap030_on & PROX_ENABLED) {
			value = (OP_RUN | OP_CONTINUOUS | OP_PS_ALS | INT_NOCLEAR);
		} else {
			value = (OP_RUN | OP_CONTINUOUS | OP_ALS | INT_NOCLEAR);
		}
		gp2ap030_i2c_write_reg(REG_ADR_00, value);
		lux = gp2ap030_datap->light_lux;
	} else if ((data_als0 < 1000) && (gp2ap030_datap->light_mode == HIGH_LUX_MODE)) {
		gp2ap030_datap->light_mode = LOW_LUX_MODE;
		GP2AP030_PRINT("change lux mode low!\n");
		value = (OP_SHUTDOWN | INT_NOCLEAR);
		gp2ap030_i2c_write_reg(REG_ADR_00, value);
		gp2ap030_i2c_read_reg(REG_ADR_01, rdata, 1);
		value = (u8)(rdata[0] & 0xC0) | (RES_A_14 | RANGE_A_8);
		gp2ap030_i2c_write_reg(REG_ADR_01, value);
		if (gp2ap030_datap->gp2ap030_on & PROX_ENABLED) {
			value = (OP_RUN | OP_CONTINUOUS | OP_PS_ALS | INT_NOCLEAR);
		} else {
			value = (OP_RUN | OP_CONTINUOUS | OP_ALS | INT_NOCLEAR);
		}
		gp2ap030_i2c_write_reg( REG_ADR_00, value);
		lux = gp2ap030_datap->light_lux;
	}
	gp2ap030_datap->light_lux = lux;
	//GP2AP030_PRINT("calc lux=%d\n", lux);

	gp2ap030_datap->light_event = 1;
	wake_up_interruptible(&gp2ap030_datap->light_event_wait);

	return lux;
}

static void gp2ap030_prox_get_dist(void)
{
	unsigned char	value;
	char		distance;
	u8		rdata;

	// 0 : proximity, 1 : away
	distance = gpio_get_value_cansleep(gp2ap030_datap->irq_gpio);
	GP2AP030_PRINT("last ps stat = %d, current ps stat = %d\n", gp2ap030_datap->prox_distance, distance);

	if (gp2ap030_datap->prox_distance != distance) {
		gp2ap030_datap->prox_distance = distance;
		value = (OP_SHUTDOWN | INT_NOCLEAR);
		gp2ap030_i2c_write_reg(REG_ADR_00, value);
		gp2ap030_i2c_read_reg(REG_ADR_01, &rdata, sizeof(rdata));

		if (distance == 0) { // detection = Falling Edge
			value = PRST_1 | (rdata & 0x3F);
			gp2ap030_i2c_write_reg( REG_ADR_01, value);
		} else { // none Detection
			value = PRST_4 | ( rdata & 0x3F ) ;
			gp2ap030_i2c_write_reg( REG_ADR_01, value);
		}

		if (gp2ap030_datap->gp2ap030_on & ALS_ENABLED) {
			value = (OP_RUN | OP_CONTINUOUS | OP_PS_ALS | INT_NOCLEAR);
		} else {
			value = (OP_RUN | OP_CONTINUOUS | OP_PS | INT_NOCLEAR);
		}
		gp2ap030_i2c_write_reg(REG_ADR_00, value);

	}
	gp2ap030_datap->prox_event = 1;
	wake_up_interruptible(&gp2ap030_datap->proximity_event_wait);

}

/*-----------------------------------gp2ap030 device control--------------------------------------*/

/*-----------------------------------irq/work-------------------------------------------*/
static irqreturn_t gp2ap030_irq_handler(int irq, void *dev_id)
{
	disable_irq_nosync(gp2ap030_datap->irq);
/*add for the system must not sleep befor the hal receive the value and processed when prox irq come
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&gp2ap030_datap->prox_wakelock, (HZ/10));
#endif
*/
	if (!work_pending(&gp2ap030_datap->prox_work))
		queue_work(gp2ap030_datap->sensor_workqueue, &gp2ap030_datap->prox_work);

	return IRQ_HANDLED;
}

static void gp2ap030_prox_work_func(struct work_struct * work)
{
	mutex_lock(&gp2ap030_datap->mutex);
	gp2ap030_prox_get_dist();
	mutex_unlock(&gp2ap030_datap->mutex);

	enable_irq(gp2ap030_datap->irq);

}

static void gp2ap030_als_work_func(struct work_struct * work)
{
	mutex_lock(&gp2ap030_datap->mutex);
	gp2ap030_als_get_data();
	mutex_unlock(&gp2ap030_datap->mutex);

	if(gp2ap030_datap->in_early_suspend){
		printk("%s--will suspend so do not restar work\r\n",__func__);
		return;
	}

	if(gp2ap030_datap->gp2ap030_on & ALS_ENABLED)
		queue_delayed_work(gp2ap030_datap->sensor_workqueue,&gp2ap030_datap->als_work,msecs_to_jiffies(gp2ap030_datap->light_poll_delay));
}

/*-------------------------------------hal interface----------------------------------*/
static ssize_t light_read(struct file *file,
				 char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct gp2ap030_data *data = (struct gp2ap030_data*)file->private_data;

	if (!data->light_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->light_event_wait, data->light_event);
	}

	if (!data->light_event || !buf)
		return 0;

	len = sizeof(data->light_lux);
	err = copy_to_user(buf, &data->light_lux, len);
	if (err) {
		printk("%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}

	data->light_event = 0;
	return len;
}

static unsigned int light_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct gp2ap030_data *data = (struct gp2ap030_data*)file->private_data;

	poll_wait(file, &data->light_event_wait, poll);
	if (data->light_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static long light_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;

	switch (cmd) {
		case LIGHT_SET_DELAY:

			if (arg > LIGHT_MAX_DELAY)
				arg = LIGHT_MAX_DELAY;
			else if (arg < LIGHT_MIN_DELAY)
				arg = LIGHT_MIN_DELAY;
			printk("LIGHT_SET_DELAY--%d\r\n",(int)arg);
			gp2ap030_datap->light_poll_delay = arg;
			break;
		case LIGHT_SET_ENABLE:
			flag = arg ? 1 : 0;
			printk("LIGHT_SET_ENABLE--%d\r\n", flag);

			if (flag) {
				mutex_lock(&gp2ap030_datap->mutex);
				lightsensor_enable();
				gp2ap030_datap->light_mode = LOW_LUX_MODE;
				gp2ap030_datap->gp2ap030_on |= ALS_ENABLED;
				mutex_unlock(&gp2ap030_datap->mutex);
				msleep(150);
				cancel_delayed_work_sync(&gp2ap030_datap->als_work);
				queue_delayed_work(gp2ap030_datap->sensor_workqueue,&gp2ap030_datap->als_work,msecs_to_jiffies(gp2ap030_datap->light_poll_delay));

			} else {
				mutex_lock(&gp2ap030_datap->mutex);
				lightsensor_disable();
				gp2ap030_datap->light_mode = LOW_LUX_MODE;
				gp2ap030_datap->gp2ap030_on &= ~ALS_ENABLED;
				mutex_unlock(&gp2ap030_datap->mutex);
				cancel_delayed_work_sync(&gp2ap030_datap->als_work);
			}
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

static int light_open(struct inode *inode, struct file *file)
{
	file->private_data = gp2ap030_datap;
	return 0;
}
static int light_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

const struct file_operations gp2ap030_light_fops = {
	.owner = THIS_MODULE,
	.read = light_read,
	.poll = light_poll,
	.unlocked_ioctl = light_ioctl,
	.open = light_open,
	.release = light_release,
};

static ssize_t prox_read(struct file *file,
				 char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct gp2ap030_data *data = (struct gp2ap030_data*)file->private_data;

	if (!data->prox_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->proximity_event_wait, data->prox_event);
	}

	if (!data->prox_event|| !buf)
		return 0;

	len = sizeof(data->prox_distance);
	err = copy_to_user(buf, &data->prox_distance, len);
	if (err) {
		printk("%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}

	data->prox_event = 0;
	return len;
}

static unsigned int prox_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct gp2ap030_data *data = (struct gp2ap030_data*)file->private_data;

	poll_wait(file, &data->proximity_event_wait, poll);
	if (data->prox_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static long prox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;

	switch (cmd) {
		case PROXIMITY_SET_DELAY:
			if (arg > PROXIMITY_MAX_DELAY)
				arg = PROXIMITY_MAX_DELAY;
			else if (arg < PROXIMITY_MIN_DELAY)
				arg = PROXIMITY_MIN_DELAY;
			printk("PROXIMITY_SET_DELAY--%d\r\n",(int)arg);
			gp2ap030_datap->prox_poll_delay = arg;
			break;
		case PROXIMITY_SET_ENABLE:
			flag = arg ? 1 : 0;
			printk("PROXIMITY_SET_ENABLE--%d\r\n",flag);
			if (flag) {
				mutex_lock(&gp2ap030_datap->mutex);
				psensor_enable();
				gp2ap030_datap->prox_distance = 1;
				msleep(200);
				gp2ap030_prox_get_dist();
				if (!(gp2ap030_datap->gp2ap030_on & PROX_ENABLED)) {
					gp2ap030_datap->gp2ap030_on |= PROX_ENABLED;
					enable_irq(gp2ap030_datap->irq);
				}
				mutex_unlock(&gp2ap030_datap->mutex);
			} else {
				mutex_lock(&gp2ap030_datap->mutex);
				psensor_disable();
				gp2ap030_datap->prox_distance = 1;
				if (gp2ap030_datap->gp2ap030_on & PROX_ENABLED) {
					gp2ap030_datap->gp2ap030_on &= ~PROX_ENABLED;
					disable_irq_nosync(gp2ap030_datap->irq);
				}
				mutex_unlock(&gp2ap030_datap->mutex);
			}
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

static int prox_open(struct inode *inode, struct file *file)
{
	file->private_data = gp2ap030_datap;
	return 0;
}
static int prox_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

const struct file_operations gp2ap030_prox_fops = {
	.owner = THIS_MODULE,
	.read = prox_read,
	.poll = prox_poll,
	.unlocked_ioctl = prox_ioctl,
	.open = prox_open,
	.release = prox_release,
};

/*-------------------------------------hal interface----------------------------------*/

/*-------------------------------------debug------------------------------------------*/
static ssize_t gp2ap030_store_als(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int arg = 0, flag = 0;

	sscanf(buf, "%du", &arg);

	flag = arg ? 1 : 0;
	printk("LIGHT_SET_ENABLE--%d\r\n", flag);

	if (flag) {
		mutex_lock(&gp2ap030_datap->mutex);
		lightsensor_enable();
		gp2ap030_datap->light_mode = LOW_LUX_MODE;
		gp2ap030_datap->gp2ap030_on |= ALS_ENABLED;
		mutex_unlock(&gp2ap030_datap->mutex);
		msleep(150);
		cancel_delayed_work_sync(&gp2ap030_datap->als_work);
		queue_delayed_work(gp2ap030_datap->sensor_workqueue,&gp2ap030_datap->als_work,msecs_to_jiffies(gp2ap030_datap->light_poll_delay));

	} else {
		mutex_lock(&gp2ap030_datap->mutex);
		lightsensor_disable();
		gp2ap030_datap->light_mode = LOW_LUX_MODE;
		gp2ap030_datap->gp2ap030_on &= ~ALS_ENABLED;
		mutex_unlock(&gp2ap030_datap->mutex);
		cancel_delayed_work_sync(&gp2ap030_datap->als_work);
	}

	return count;
}

static ssize_t gp2ap030_store_ps(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int arg = 0, flag = 0;

	sscanf(buf, "%du", &arg);

	flag = arg ? 1 : 0;
	printk("PROXIMITY_SET_ENABLE--%d\r\n",flag);

	if (flag) {
		mutex_lock(&gp2ap030_datap->mutex);
		psensor_enable();
		gp2ap030_datap->prox_distance = 1;
		mdelay(200);
		gp2ap030_prox_get_dist();
		gp2ap030_datap->gp2ap030_on |= PROX_ENABLED;
		enable_irq(gp2ap030_datap->irq);
		mutex_unlock(&gp2ap030_datap->mutex);
	} else {
		mutex_lock(&gp2ap030_datap->mutex);
		psensor_disable();
		gp2ap030_datap->prox_distance = 1;
		gp2ap030_datap->gp2ap030_on &= ~PROX_ENABLED;
		disable_irq_nosync(gp2ap030_datap->irq);
		mutex_unlock(&gp2ap030_datap->mutex);
	}

	return count;
}

static ssize_t gp2ap030_show_reg(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	u8  value0, value1, value2, value3;
	u16 value4, value5, value6, value7;
	mutex_lock(&gp2ap030_datap->mutex);
	gp2ap030_i2c_read_reg(REG_ADR_00, &value0, 1);
	gp2ap030_i2c_read_reg(REG_ADR_01, &value1, 1);
	gp2ap030_i2c_read_reg(REG_ADR_02, &value2, 1);
	gp2ap030_i2c_read_reg(REG_ADR_03, &value3, 1);
	gp2ap030_i2c_read_reg(REG_ADR_04, (u8 *)&value4, 2);
	gp2ap030_i2c_read_reg(REG_ADR_06, (u8 *)&value5, 2);
	gp2ap030_i2c_read_reg(REG_ADR_08, (u8 *)&value6, 2);
	gp2ap030_i2c_read_reg(REG_ADR_0A, (u8 *)&value7, 2);
	mutex_unlock(&gp2ap030_datap->mutex);
	return snprintf(buf, PAGE_SIZE,
		"GP2AP030 : ADR00=0x%x, ADR01=0x%x, ADR02=0x%x, ADR03=0x%x, \
		ADR04=0x%x, ADR06=0x%x, ADR08=0x%x, ADR0A=0x%x\n",
		value0, value1, value2, value3, value4, value5, value6, value7);
}

static ssize_t gp2ap030_store_reg(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	u8 reg, value;

	sscanf(buf, "%hhu %hhu", (u8*)&reg, (u8*)&value);

	//printk("%s-reg[%d]=0x%x\r\n",__func__,reg,value);
	mutex_lock(&gp2ap030_datap->mutex);
	gp2ap030_i2c_write_reg(reg, value);
	mutex_unlock(&gp2ap030_datap->mutex);

	return count;
}

static ssize_t
raw_data_show( struct device *dev,
					struct device_attribute *attr,
						char *buf )
{
	u32 data_clear;
	u32 data_ir;
	u32 data_ps;
	u8  rdata[2];
	u32 ratio;
	int mode;

	pr_debug("raw_data_show \n");
	mutex_lock(&gp2ap030_datap->mutex);
	gp2ap030_i2c_read_reg(REG_ADR_0C, rdata, sizeof(rdata));
	data_clear = (rdata[1] << 8) | rdata[0];
	gp2ap030_i2c_read_reg(REG_ADR_0E, rdata, sizeof(rdata));
	data_ir = (rdata[1] << 8) | rdata[0];
	gp2ap030_i2c_read_reg(REG_ADR_10, rdata, sizeof(rdata));
	data_ps = (rdata[1] << 8) | rdata[0];
	mutex_unlock(&gp2ap030_datap->mutex);
	if (data_clear == 0) {
		ratio = 100;
	} else {
		ratio = (data_ir * 100) / data_clear;
	}

	mode = gp2ap030_datap->light_mode;

	return sprintf(buf, "clear=%d, ir=%d, ps=%d, %d.%02d,%d\n", data_clear, data_ir, data_ps, (ratio / 100), (ratio % 100), mode);
}

static ssize_t
ps_calibration_show( struct device *dev,
						struct device_attribute *attr,
							char *buf )
{
	u8 rdata[2];
	ssize_t cali_cnt;
	u32 prox_cali;

	printk("ps_calibration_show \n");

	mutex_lock(&gp2ap030_datap->mutex);
	gp2ap030_i2c_read_reg(REG_ADR_10, rdata, sizeof(rdata));
	mutex_unlock(&gp2ap030_datap->mutex);
	/*get noise, should be done when in amt calibration*/
	prox_cali = (rdata[1] << 8) | rdata[0];

	cali_cnt = snprintf(buf, PAGE_SIZE, "%02X\n", prox_cali);
	printk("calibration=%d\n", prox_cali);

	return cali_cnt;
}

/*
*      NOTICE:calibration only do once when the first enable is done
*      format: cmd,noise
*	1, noise	add noise to threshould
*	0, 		add internal val to threshould
*/
static ssize_t
ps_calibration_store( struct device *dev,
						struct device_attribute *attr,
							const char *buf,
								size_t count )
{
	u8 value;
	char *cmdp;
	char *offsetp;
	u32 cmd;
	u32 offset;
	u32 th_l;
	u32 th_h;
	char tmpbuf[16];
	char *endp;

	//pr_debug("ps_calibration_store data=%s\n", buf);
	printk("<%s> ps_calibration_store data=%s\n", __func__, buf);//debug

	if (strlen(buf) > 15) {
		return count;
	}

	memset(tmpbuf, 0x00, sizeof(tmpbuf));
	memcpy(tmpbuf, buf, strlen(buf));

	cmdp = strtok(tmpbuf, ",");
	if (cmdp == NULL) {
		return count;
	}
	cmd = simple_strtoul(cmdp, &endp, 10);
	printk("<%s> cmd = %d\n", __func__, cmd);//debug
	if(!(endp != cmdp && (endp == (cmdp + strlen(cmdp))
				|| (endp == (cmdp + strlen(cmdp) - 1) && *endp == '\n')))) {
		printk(KERN_ERR "invalid value (%s)\n", buf);
		return count;
	}
	if (cmd == 1) {
		offsetp = strtok(NULL, ",");
		if (offsetp == NULL) {
			printk(KERN_ERR "invalid value (%s)\n", buf);
			return count;
		}
		offset = simple_strtoul(offsetp, &endp, 10);/*noise set from upper layer*/
		printk("<%s> offset = %d\n", __func__, offset);//debug
		if (!(endp != offsetp && (endp == (offsetp + strlen(offsetp))
					|| (endp == (offsetp + strlen(offsetp) - 1) && *endp == '\n')))) {
			printk(KERN_ERR "invalid value (%s)\n", buf);
			return count;
		}
		if (offset < 0 || offset > 0xffff) {
			printk(KERN_ERR "invalid value (%s)\n", buf);
			return count;
		}
		gp2ap030_datap->prox_calibration = offset;
	} else if (cmd == 0) {
		offsetp = strtok(NULL, ",");
		if (offsetp != NULL) {
			printk(KERN_ERR "invalid value (%s)\n", buf);
			return count;
		}
		offset = gp2ap030_datap->prox_calibration;
	} else {
		printk(KERN_ERR "invalid value (%s)\n", buf);
		return count;
	}

	printk("<%s> cmd=%d,offset=%d\n", __func__, cmd, offset);

	/*get threshould high & low*/
	/*gp2ap030_i2c_read_reg( REG_ADR_08, rdata, sizeof(rdata));
	th_l = (rdata[1] << 8) | rdata[0];
	gp2ap030_i2c_read_reg( REG_ADR_0A, rdata, sizeof(rdata));
	th_h = (rdata[1] << 8) | rdata[0];*/
	if (gp2ap030_datap->gp2ap030_cfg_data) {
		th_l = gp2ap030_datap->gp2ap030_cfg_data->prox_threshold_lo;
		th_h = gp2ap030_datap->gp2ap030_cfg_data->prox_threshold_hi;
	}

	th_l += offset;
	th_h += offset;
	if (th_l > 0xffff) {
		th_l = 0xffff;
	}
	if (th_h > 0xffff) {
		th_h = 0xffff;
	}

	mutex_lock(&gp2ap030_datap->mutex);
	value = (OP_SHUTDOWN | INT_NOCLEAR);	// shutdown
	gp2ap030_i2c_write_reg(REG_ADR_00, value);

	value = (u8)(th_l & 0x000000ff);
	gp2ap030_datap->reg_data[REG_ADR_08] = value;
	gp2ap030_i2c_write_reg( REG_ADR_08, value);
	value = (u8)((th_l  & 0x0000ff00) >> 8);
	gp2ap030_datap->reg_data[REG_ADR_09] = value;
	gp2ap030_i2c_write_reg(REG_ADR_09, value);

	value = (u8)(th_h & 0x000000ff);
	gp2ap030_datap->reg_data[REG_ADR_0A] = value;
	gp2ap030_i2c_write_reg(REG_ADR_0A, value);
	value = (u8)((th_h  & 0x0000ff00) >> 8);
	gp2ap030_datap->reg_data[REG_ADR_0B] = value;
	gp2ap030_i2c_write_reg(REG_ADR_0B, value);

	gp2ap030_restart_device();

	mutex_unlock(&gp2ap030_datap->mutex);

	return count;
}

static struct device_attribute als_attribute = __ATTR(als, 0664, NULL, gp2ap030_store_als);
static struct device_attribute ps_attribute = __ATTR(ps, 0664, NULL, gp2ap030_store_ps);
static struct device_attribute reg_attribute = __ATTR(reg, 0664, gp2ap030_show_reg, gp2ap030_store_reg);
static struct device_attribute raw_data = __ATTR(raw_data, 0664, raw_data_show, NULL);
static struct device_attribute ps_calibration = __ATTR(ps_calibration, 0664, ps_calibration_show, ps_calibration_store);

static struct attribute *attrs[] = {

	&als_attribute.attr,

	&ps_attribute.attr,

	&reg_attribute.attr,

	&raw_data.attr,

	&ps_calibration.attr,

	NULL,
};

static struct attribute_group sensor_light_attribute_group = {.attrs = attrs,};

/*-------------------------------------debug------------------------------------------*/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gp2ap030_early_suspend(struct early_suspend *handler)
{
	gp2ap030_datap->in_early_suspend = 1;

	//cancel_delayed_work_sync(&gp2ap030_datap->work_lus);
	//cancel_work_sync(&gp2ap030_datap->prox_work);
}

static void gp2ap030_later_resume(struct early_suspend *handler)
{
	gp2ap030_datap->in_early_suspend = 0;
	if (gp2ap030_datap->gp2ap030_on & ALS_ENABLED)
		gp2ap030_datap->light_need_restar_work = 1;

	if (gp2ap030_datap->light_need_restar_work) {
		printk("%s--will restart lux work when resume\r\n",__func__);
		gp2ap030_datap->light_need_restar_work = 0;
		queue_delayed_work(gp2ap030_datap->sensor_workqueue,&gp2ap030_datap->als_work,msecs_to_jiffies(gp2ap030_datap->light_poll_delay));
	}
}
#endif
static int gp2ap030_suspend(struct i2c_client *client, pm_message_t mesg)
{
	if (gp2ap030_datap->gp2ap030_on & PROX_ENABLED)
		enable_irq_wake(gp2ap030_datap->irq);

	gp2ap030_datap->light_need_restar_work = 0;
	return 0;
}
static int gp2ap030_resume(struct i2c_client *client)
{
	if (gp2ap030_datap->gp2ap030_on & PROX_ENABLED)
		disable_irq_wake(gp2ap030_datap->irq);

	if (gp2ap030_datap->gp2ap030_on & ALS_ENABLED)
		gp2ap030_datap->light_need_restar_work = 1;
	return 0;
}
static int gp2ap030_probe(struct i2c_client *clientp, const struct i2c_device_id *idp) {
	int ret = 0;
	u16 thd = 0;

	this_client = clientp;

	if (gp2ap030_id_check()) {
		printk("%s:device id error!\n", __func__);
		return -1;
	}

	printk("%s, start. \n", __func__);

	//res prepare
	gp2ap030_datap = kmalloc(sizeof(struct gp2ap030_data), GFP_KERNEL);
	if (!gp2ap030_datap) {
		printk("%s:Cannot get memory!\n", __func__);
		return -ENOMEM;
	}
	memset(gp2ap030_datap,0,sizeof(struct gp2ap030_data));

	gp2ap030_datap->light_poll_delay = LIGHT_DEFAULT_DELAY;
	gp2ap030_datap->prox_poll_delay = PROXIMITY_DEFAULT_DELAY;
	gp2ap030_datap->client = clientp;
	i2c_set_clientdata(clientp, gp2ap030_datap);

	mutex_init(&gp2ap030_datap->mutex);

	INIT_WORK(&gp2ap030_datap->prox_work, gp2ap030_prox_work_func);
	INIT_DELAYED_WORK(&gp2ap030_datap->als_work, gp2ap030_als_work_func);

	gp2ap030_datap->sensor_workqueue = create_singlethread_workqueue(dev_name(&clientp->dev));
	if (!gp2ap030_datap->sensor_workqueue) {
		ret = -ESRCH;
		goto register_singlethread_failed;
	}

	init_waitqueue_head(&gp2ap030_datap->light_event_wait);
	init_waitqueue_head(&gp2ap030_datap->proximity_event_wait);

	gp2ap030_datap->gp2ap030_cfg_data = clientp->dev.platform_data;
	//light misc register
	gp2ap030_datap->light_dev.minor = MISC_DYNAMIC_MINOR;
	gp2ap030_datap->light_dev.name = "light";
	gp2ap030_datap->light_dev.fops = &gp2ap030_light_fops;
	ret = misc_register(&(gp2ap030_datap->light_dev));
	if (ret) {
		printk("%s:Register misc device for light sensor failed(%d)!\n",
				__func__, ret);
		goto register_light_failed;
	}

	//prox misc register
	gp2ap030_datap->prox_dev.minor = MISC_DYNAMIC_MINOR;
	gp2ap030_datap->prox_dev.name = "proximity";
	gp2ap030_datap->prox_dev.fops = &gp2ap030_prox_fops;
	ret = misc_register(&(gp2ap030_datap->prox_dev));
	if (ret) {
		printk("%s:Register misc device for light sensor failed(%d)!\n",
				__func__, ret);
		goto register_prox_failed;
	}

	//irq request
	if (clientp->irq) {
		gp2ap030_datap->irq_gpio = irq_to_gpio(clientp->irq);
		printk("irq =[%d],gp2ap030_datap->irq_gpio = [%d] \n", clientp->irq, gp2ap030_datap->irq_gpio);
		ret = gpio_request(gp2ap030_datap->irq_gpio, "gp2ap030 irq");
		if (ret < 0){
			printk("%s:Gpio request failed!\n", __func__);
			goto request_gpio_faile;
		}
		gpio_direction_input(gp2ap030_datap->irq_gpio);

		gp2ap030_datap->irq = gpio_to_irq(gp2ap030_datap->irq_gpio);
		printk("gp2ap030_datap->irq = [%d]\n", gp2ap030_datap->irq);

		ret = request_irq(gp2ap030_datap->irq, gp2ap030_irq_handler,
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gp2ap030_irq", gp2ap030_datap);
		if (ret != 0) {
			printk("%s:IRQ request failed!\n", __func__);
			goto request_irq_faile;
		}
		disable_irq_nosync(gp2ap030_datap->irq);
	}

	memcpy(gp2ap030_datap->reg_data, gp2ap_init_data, ARRAY_SIZE(gp2ap_init_data));
	/*ps threshold config*/
	if (gp2ap030_datap->gp2ap030_cfg_data) {
		thd = gp2ap030_datap->gp2ap030_cfg_data->prox_threshold_lo & 0xff;
		gp2ap030_datap->reg_data[REG_ADR_08] = thd;
		thd = (gp2ap030_datap->gp2ap030_cfg_data->prox_threshold_lo >> 8) & 0xff;
		gp2ap030_datap->reg_data[REG_ADR_09] = thd;
		thd = gp2ap030_datap->gp2ap030_cfg_data->prox_threshold_hi & 0xff;
		gp2ap030_datap->reg_data[REG_ADR_0A] = thd;
		thd = (gp2ap030_datap->gp2ap030_cfg_data->prox_threshold_hi >> 8) & 0xff;
		gp2ap030_datap->reg_data[REG_ADR_0B] = thd;
	}
	gp2ap030_init_device();

	gp2ap030_datap->gp2ap030_on = 0x00;/*both als and prox are off*/
	gp2ap030_datap->prox_distance = 1;

	ret = sysfs_create_group(&gp2ap030_datap->light_dev.this_device->kobj,&sensor_light_attribute_group);

#ifdef CONFIG_HAS_EARLYSUSPEND
	gp2ap030_datap->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
	gp2ap030_datap->early_suspend.suspend = gp2ap030_early_suspend;
	gp2ap030_datap->early_suspend.resume	= gp2ap030_later_resume;
	register_early_suspend(&gp2ap030_datap->early_suspend);
	gp2ap030_datap->in_early_suspend = 0;
#endif

	printk("%s, end. \n", __func__);

	return ret;

request_irq_faile:
	gpio_free(clientp->irq);
request_gpio_faile:
	misc_deregister(&gp2ap030_datap->prox_dev);
register_prox_failed:
	misc_deregister(&gp2ap030_datap->light_dev);
register_light_failed:
	destroy_workqueue(gp2ap030_datap->sensor_workqueue);
register_singlethread_failed:
	mutex_destroy(&gp2ap030_datap->mutex);
	kfree(gp2ap030_datap);
	return (ret);
}

static int gp2ap030_remove(struct i2c_client *client) {
	int ret = 0;
	free_irq(gpio_to_irq(client->irq), gp2ap030_datap);
	gpio_free(client->irq);

	misc_deregister(&gp2ap030_datap->prox_dev);
	misc_deregister(&gp2ap030_datap->light_dev);
	destroy_workqueue(gp2ap030_datap->sensor_workqueue);

	kfree(gp2ap030_datap);
	gp2ap030_datap = NULL;
	return (ret);
}

static struct i2c_device_id gp2ap030_idtable[] = {
		{gp2ap030_DEVICE_ID, 0},
		{}
};
MODULE_DEVICE_TABLE(i2c, gp2ap030_idtable);

static struct i2c_driver gp2ap030_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = gp2ap030_DEVICE_NAME,
	},
	.id_table = gp2ap030_idtable,
	.probe = gp2ap030_probe,
	.suspend = gp2ap030_suspend,
	.resume = gp2ap030_resume,
	.remove = gp2ap030_remove,
};

static int __init gp2ap030_init(void) {
	return i2c_add_driver(&gp2ap030_driver);
}

static void __exit gp2ap030_exit(void) {
	i2c_del_driver(&gp2ap030_driver);
}

MODULE_AUTHOR("leadcore tech");
MODULE_DESCRIPTION("gp2ap030 ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");

module_init(gp2ap030_init);
module_exit(gp2ap030_exit);

