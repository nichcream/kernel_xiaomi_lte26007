/*******************************************************************************
*                                                                              *
*   File Name:    cm36283.c                                                     *
*   Description:   Linux device driver for cm36283 ambient light and            *
*   proximity sensors.                                                         *
*   Author:         xingdajing                                                 *
*   History:   2012-08-02     Initial creation                                 *
*                                                                              *
********************************************************************************
*    Proprietary to cm36283 Inc., 1001 Klein Road #300, Plano, TX 75074        *
*******************************************************************************/
// includes
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

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/miscdevice.h>

#include <plat/lightprox.h>
#include <linux/cm36283.h>

#include <asm/gpio.h>


#define CM36283_DEBUG
#if defined CM36283_DEBUG
#define CM36283_PRINT(fmt, args...) printk("[cm36283]--" fmt, ##args)
#else
#define CM36283_PRINT(fmt, args...)
#endif

/*************************************config********************************************/
#define cm36283_DEVICE_NAME                "cm36283"
#define cm36283_DEVICE_ID                  "cm36283"

#define CM3605_ALS_THDH_REG			0x01   //l:thdh-l   h:thdh-h
#define CM3605_ALS_THDL_REG    			0x02   //l:thdl-l   h:thdl-h
#define CM36283_ALS_CONF_REG			0x00   //l:als conf h:reserved
#define ALS_IT_MASK      			0x00c0
#define ALS_IT_1000      			0x0000 //80ms  0.1    LUX  MAX 6553 LUX
#define ALS_IT_500      			0x0040 //160ms 0.05   LUX  MAX 3277 LUX
#define ALS_IT_250      			0x0080 //320ms 0.025  LUX  MAX 1638 LUX
#define ALS_IT_125      			0x00c0 //640ms 0.0125 LUX  MAX 819  LUX
#define ALS_SPERS_MASK				0x000C 
#define ALS_SPERS_1				0x0000 //irq debounces time is 1* IT
#define ALS_SPERS_2				0x0004 //irq debounces time is 2* IT
#define ALS_SPERS_4				0x0008 //irq debounces time is 4* IT
#define ALS_SPERS_8				0x000C //irq debounces time is 8* IT

#define ALS_INT      				0x0002 //1:ENABLE INT 0:DISABLE
#define ALS_POWER_MASK  	       		0x0001
#define ALS_POWER_ON            		0x0000
#define ALS_POWER_OFF               		0x0001

#define CM3605_PS_CONF1_REG			0x03   //l:conf1    h:conf2
#define PS_DUTY_MASK				0x00C0
#define PS_DUTY_40				0x0000  // 1/40
#define PS_DUTY_80				0x0040  // 1/80
#define PS_DUTY_160				0x0080  // 1/160
#define PS_DUTY_320 				0x00c0  // 1/320
#define PS_IT_MASK				0x0030
#define PS_IT_10				0x0000  //1   T T=0.32ms
#define PS_IT_13				0x0010  //1.3 T
#define PS_IT_16				0x0020  //1.6 T
#define PS_IT_20				0x0030  //2   T
//Tps = PS_IT/PS_DUTY
//sample: PS_IT_13 -> 0x32*1.3 = 0.42
//        PS_DUTY_160 -> Tps = 0.42/(1/160) = 67.2ms
#define PS_PERS_MASK				0x000C  
#define PS_PERS_1				0x0000  //irq debounces time is 1* Tps
#define PS_PERS_2				0x0004  //irq debounces time is 2* Tps
#define PS_PERS_3				0x0008  //irq debounces time is 3* Tps
#define PS_PERS_4				0x000C  //irq debounces time is 4* Tps
#define PS_POWER_MASK               		0x0001
#define PS_POWER_ON                 		0x0000
#define PS_POWER_OFF                		0x0001
#define PS_ITB_MASK				0xC000
#define PS_ITB_1				0x0000  //0.5 * PS IT
#define PS_ITB_2				0x4000  //1   * PS IT
#define PS_ITB_4				0x8000  //2   * PS IT
#define PS_ITB_8				0xC000  //4   * PS IT
#define PS_INT_MASK      			0x0300
#define PS_INT_DISABLE      			0x0000
#define PS_INT_TRIGGER_CLOSE      		0x0100
#define PS_INT_TRIGGER_AWAY      		0x0200
#define PS_INT_TRIGGER_ALL      		0x0300

#define CM3605_PS_CONF3_REG			0x04//L:conf3    h:ps ms
#define PS_MS					0x0400 //1:ENABLE 0:DISABLE
//MS mode : irq close the pin low, irq away pin high
#define PS_PROL_MASK				0x3000
#define PS_PROL_63				0x0000
#define PS_PROL_128				0x1000
#define PS_PROL_191				0x2000
#define PS_PROL_255				0x3000 //
#define PS_SMART				0x0010 //1:ENABLE 0:DISABLE  OPEN
#define PS_FORCE_MODE   			0x0008 //1:ENABLE 0:DISABLE
#define PS_FORCE_TRIG   			0x0004 //1:ENABLE 0:DISABLE



#define CM3605_PS_CNAC_REG			0x05//l:canc     h:reserved
#define CM3605_PS_CNAC_MASK         		0xFF
#define CM3605_PS_THD_REG			0x06//l:thd-l    h:thd-h
#define CM3605_PS_DATA_REG			0x08//l:data     h:reserved
#define CM3605_PS_DATA_MASK         		0xFF

#define CM3605_ALS_DATA_REG			0x09//l:data l   h:data h
#define CM3605_INT_FLAG_REG			0x0B//l:reserved h:flag
#define CM3605_INT_PS_IN_PRO        		0x4000
#define CM3605_INT_ALS_THDH         		0x2000
#define CM3605_INT_ALS_THDL         		0x1000
#define CM3605_INT_PS_CLOSE         		0x0200 //when ps value >= thdh
#define CM3605_INT_PS_AWAY          		0x0100 //when ps value < thdl
#define CM3605_ID_REG				0x0C//l:id-l     h:id-h


#define CM36283_ALS_CONFIG           		(ALS_IT_1000 | ALS_SPERS_1)
//#define CM36283_PROS_CONFIG1         		(PS_DUTY_160 |PS_IT_10 |PS_PERS_4| PS_ITB_4 )
#define CM36283_PROS_CONFIG1    		0x8087
#define CM36283_PROS_CONFIG2         		(PS_PROL_255 |PS_SMART)
#define CMC_BIT_ALS				0x01
#define CMC_BIT_PROX				0x02
#define PROX_CLOSE_MODE				0x01
#define PROX_AWAY_MODE				0x02
/*************************************config********************************************/

struct cm36283_data {
	struct i2c_client		*client;
	struct cm36283_platform_data 	*cm36283_cfg_data;
	struct work_struct		prox_work;
	struct delayed_work		als_work;
	struct workqueue_struct 	*sensor_workqueue;
	struct miscdevice 		light_dev;
	struct miscdevice 		prox_dev;
	wait_queue_head_t 		light_event_wait;
	wait_queue_head_t 		proximity_event_wait;

	struct early_suspend		early_suspend;
	int 				in_early_suspend;

	unsigned int			light_lux;
	int 				light_event;
	int 				light_poll_delay;
	int             		light_need_restar_work;
	int 				prox_distance;
	int 				prox_mode;
	int 				prox_event;
	int 				prox_poll_delay;

	int 				irq;
	int 				irq_addr;
	int 				irq_gpio;

	unsigned long 			cm36283_on;
};
static struct cm36283_data 		*cm36283_datap;
static struct i2c_client 		*this_client = NULL;

static int cm36283_i2c_write_reg(u8 addr, u16 para)
{
	u8 buf[3];
	struct i2c_msg msgs[2];
	
	buf[0] = addr;
	buf[1] = para & 0xff;
	buf[2] = (para >> 8);
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 3;
	msgs[0].buf = buf;
	if (i2c_transfer(this_client->adapter, msgs, 1) < 0) {
		printk("cm36283_i2c_write_reg: transfer error\n");
		return -1;
	}
    
    return 0;
}
static int cm36283_i2c_read_reg(u8 addr, u16 *pdata)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[2];
	u8 data[2] = {0};

	buf[0] = addr;    //register address
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;
	msgs[1].addr = this_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = data;

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0){
		printk("i2c read error: %d\n", ret);
		return -1;
	}

	*pdata = data[0] + (data[1] << 8);
	return 0;
}

static int cm36283_id_check(void)
{
	u16 device_id;
	int ret;
	ret = cm36283_i2c_read_reg(CM3605_ID_REG,&device_id);
	if(ret < 0 )
		return ret;

	device_id &=  0xff;
	if(device_id == 0x83)
		return 0;
	return -1;
}

static int cm36283_als_config(void)
{
	int ret;
	u16 value;
	//conf1
	value = CM36283_ALS_CONFIG ;

	ret = cm36283_i2c_write_reg(CM36283_ALS_CONF_REG,value);
	if(ret < 0 )
		return ret;
	
	return 0;
}

static int cm36283_sensors_als_int_enable(int onoff)
{
	int ret;
	u16 value;
	ret = cm36283_i2c_read_reg(CM36283_ALS_CONF_REG,&value);
	if(ret < 0 )
		return ret;
	
	if(onoff)
		value |= ALS_INT;
	else
		value &= ~ALS_INT;

	ret = cm36283_i2c_write_reg(CM36283_ALS_CONF_REG,value);
	if(ret < 0 )
		return ret;
	return 0;
}

static int cm36283_sensors_als_on(int onoff)
{
	int ret;
	u16 value;

	printk(KERN_DEBUG "%s, onoff = [%d]\n", __func__, onoff);
	ret = cm36283_i2c_read_reg(CM36283_ALS_CONF_REG,&value);
	if(ret < 0 )
		return ret;

	value &= ~ALS_POWER_MASK;
	if(onoff)
		value |= ALS_POWER_ON;
	else
		value |= ALS_POWER_OFF;

	ret = cm36283_i2c_write_reg(CM36283_ALS_CONF_REG,value);
	if(ret < 0 )
		return ret;
	return 0;
}
static int cm36283_als_threshold_set(u16 thdh,u16 thdl)
{
	int ret;

	ret = cm36283_i2c_write_reg(CM3605_ALS_THDH_REG,thdh);
	if(ret < 0 )
		return ret;
	
	ret = cm36283_i2c_write_reg(CM3605_ALS_THDL_REG,thdl);
	if(ret < 0 )
		return ret;
	return 0;
}

static int cm36283_sensors_als_enable(int onoff)
{
	int ret;

	ret = cm36283_sensors_als_on(onoff);
	if(ret < 0 )
		return ret;

	return 0;
}

static int cm36283_prox_config(void)
{
	int ret;
	u16 value;

	//conf1
	value = CM36283_PROS_CONFIG1;
	ret = cm36283_i2c_write_reg(CM3605_PS_CONF1_REG,value);
	if(ret < 0 )
		return ret;

	//conf2
	value = CM36283_PROS_CONFIG2;
	ret = cm36283_i2c_write_reg(CM3605_PS_CONF3_REG,value);
	if(ret < 0 )
		return ret;
	return 0;
}


static int cm36283_sensors_prox_on(int onoff)
{
	int ret;
	u16 value;

	printk("%s, onoff = [%d]\n", __func__, onoff);

	ret = cm36283_i2c_read_reg(CM3605_PS_CONF1_REG,&value);
	if(ret < 0 )
		return ret;
	
	value &= ~PS_POWER_MASK;
	if(onoff)
		value |= PS_POWER_ON;
	else
		value |= PS_POWER_OFF;

	ret = cm36283_i2c_write_reg(CM3605_PS_CONF1_REG,value);
	if(ret < 0 )
		return ret;
	return 0;
}

static int cm36283_sensors_prox_int_enable(int onoff)
{
	int ret;
	u16 value;

	ret = cm36283_i2c_read_reg(CM3605_PS_CONF1_REG,&value);
	if(ret < 0 )
		return ret;
	
	value &= ~PS_INT_MASK;
	if(onoff)
		value |= PS_INT_TRIGGER_ALL;

	ret = cm36283_i2c_write_reg(CM3605_PS_CONF1_REG,value);
	if(ret < 0 )
		return ret;
	return 0;
}

static int cm36283_prox_threshold_set(u8 thdh,u8 thdl)
{
	int ret;
	u16 value;

	value = thdl | (thdh << 8);
	ret = cm36283_i2c_write_reg(CM3605_PS_THD_REG,value);
	if(ret < 0 )
		return ret;
	
	return 0;
}

static int cm36283_prox_cana_set(u16 cana)
{
	int ret;

	ret = cm36283_i2c_write_reg(CM3605_PS_CNAC_REG,cana);
	if(ret < 0 )
		return ret;
	
	return 0;
}

static int cm36283_sensors_pros_enable(int onoff)
{
	int ret;

	if(onoff){
		ret = cm36283_sensors_prox_on(true);
		if(ret < 0 )
			return ret;

		ret = cm36283_sensors_prox_int_enable(true);
		if(ret < 0 )
			return ret;
	}else{
		ret = cm36283_sensors_prox_int_enable(false);
		if(ret < 0 )
			return ret;

		ret = cm36283_sensors_prox_on(false);
		if(ret < 0 )
			return ret;
	}
	return 0;
}

static int cm36283_als_get_data(void)
{
	u16 als_data;
	int ret;

	ret = cm36283_i2c_read_reg(CM3605_ALS_DATA_REG,&als_data);
	if(ret < 0 )
		return ret;

	//printk("%s--lux=%d\r\n",__func__,als_data);
	cm36283_datap->light_lux   = als_data;
	cm36283_datap->light_event = 1;
	wake_up_interruptible(&cm36283_datap->light_event_wait);
	
	return 0;
}

static int cm36283_prox_get_data(void)
{
	int ret = 0;
	u16 prox_val = 0;
	struct cm36283_platform_data *cm36283 = cm36283_datap->cm36283_cfg_data;

	ret = cm36283_i2c_read_reg(CM3605_PS_DATA_REG,&prox_val);
	if(ret < 0 )
		return ret;
	prox_val &= CM3605_PS_DATA_MASK;
	printk("%s--ps=%d\r\n",__func__,prox_val);
	if(prox_val >= cm36283->prox_threshold_hi){
		cm36283_datap->prox_distance = 0;
		printk("%s--cm36283_datap->prox_distance=%d,close!!\r\n",__func__,cm36283_datap->prox_distance);
		}
	else if(prox_val <= cm36283->prox_threshold_lo){
		cm36283_datap->prox_distance = 1;
		printk("%s--cm36283_datap->prox_distance=%d,far!!\r\n",__func__,cm36283_datap->prox_distance);
		}
	cm36283_datap->prox_event = 1;
	wake_up_interruptible(&cm36283_datap->proximity_event_wait);

	return 0;
}

static int cm36283_interrupts_read(u16 *irq)
{
	u16 value = 0;
	int ret;

	ret = cm36283_i2c_read_reg(CM3605_INT_FLAG_REG,&value);
	if(ret < 0 )
		return ret;

	*irq = value;
	return 0;
}

static int cm36283_device_init(void)
{
	int ret;
	struct cm36283_platform_data *cm36283 = cm36283_datap->cm36283_cfg_data;
	//als setting
	ret = cm36283_als_config();
	if(ret < 0 )
		return ret;

	ret = cm36283_sensors_als_int_enable(false);
	if(ret < 0 )
		return ret;
	
	ret = cm36283_sensors_als_on(false);
	if(ret < 0 )
		return ret;

	ret = cm36283_als_threshold_set(0x3fff,0x0fff);
	if(ret < 0 )
		return ret;

	//prox setting
	ret = cm36283_prox_config();
	if(ret < 0 )
		return ret;

	ret = cm36283_prox_threshold_set(cm36283->prox_threshold_hi,cm36283->prox_threshold_lo);
	if(ret < 0 )
		return ret;

	ret = cm36283_sensors_prox_int_enable(true);// false before
	if(ret < 0 )
		return ret;

	ret = cm36283_sensors_prox_on(false);
	if(ret < 0 )
		return ret;

	ret = cm36283_prox_cana_set(0x3);
	if(ret < 0 )
		return ret;
	
	return 0;
}
/*-----------------------------------cm36283 device control--------------------------------------*/

/*-----------------------------------irq/work-------------------------------------------*/
static irqreturn_t cm36283_irq_handler(int irq, void *dev_id)
{
	disable_irq_nosync(cm36283_datap->irq);
/*add for the system must not sleep befor the hal receive the value and processed when prox irq come
#ifdef CONFIG_HAS_WAKELOCK
wake_lock_timeout(&cm36283_datap->prox_wakelock, (HZ/10));
#endif
*/
	if (!work_pending(&cm36283_datap->prox_work))
		queue_work(cm36283_datap->sensor_workqueue, &cm36283_datap->prox_work);
	
	return IRQ_HANDLED;
}

static void cm36283_prox_work_func(struct work_struct * work)
{
	u16 irq_addr;
	
	if(cm36283_interrupts_read(&irq_addr)){
		printk("%s--irq read error\r\n",__func__);
		enable_irq(cm36283_datap->irq);
		return;
	}
	
	if(irq_addr & CM3605_INT_PS_CLOSE){
		cm36283_datap->prox_mode = PROX_CLOSE_MODE;
		cm36283_prox_get_data();
	}else if(irq_addr & CM3605_INT_PS_AWAY){
		cm36283_datap->prox_mode = PROX_AWAY_MODE;
		cm36283_prox_get_data();
	}
	enable_irq(cm36283_datap->irq);
}

static void cm36283_als_work_func(struct work_struct * work)
{
	cm36283_als_get_data();
	if(cm36283_datap->in_early_suspend){
		printk("%s--will suspend so do not restar work\r\n",__func__);
		return;
	}

	if(cm36283_datap->cm36283_on & CMC_BIT_ALS)
		queue_delayed_work(cm36283_datap->sensor_workqueue,&cm36283_datap->als_work,msecs_to_jiffies(cm36283_datap->light_poll_delay));
}

/*-------------------------------------irq/work--------------------------------------------*/

/*-------------------------------------hal interface----------------------------------*/
static ssize_t light_read(struct file *file,
				 char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct cm36283_data *data = (struct cm36283_data*)file->private_data;

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
	struct cm36283_data *data = (struct cm36283_data*)file->private_data;

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
			cm36283_datap->light_poll_delay = arg;
			break;
		case LIGHT_SET_ENABLE:
			flag = arg ? 1 : 0;
			printk("LIGHT_SET_ENABLE--%d\r\n",flag);
			cm36283_sensors_als_enable(flag);
			if(flag){
				cancel_delayed_work_sync(&cm36283_datap->als_work);
				queue_delayed_work(cm36283_datap->sensor_workqueue,&cm36283_datap->als_work,msecs_to_jiffies(cm36283_datap->light_poll_delay));
				cm36283_datap->cm36283_on |= CMC_BIT_ALS;
			}
			else{
				cm36283_datap->cm36283_on &= ~CMC_BIT_ALS;
				cancel_delayed_work_sync(&cm36283_datap->als_work);
			}
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

static int light_open(struct inode *inode, struct file *file)
{
	file->private_data = cm36283_datap;
	return 0;
}
static int light_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

const struct file_operations cm36283_light_fops = {
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
	struct cm36283_data *data = (struct cm36283_data*)file->private_data;

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
	struct cm36283_data *data = (struct cm36283_data*)file->private_data;

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
			cm36283_datap->prox_poll_delay = arg;
			break;
		case PROXIMITY_SET_ENABLE:
			flag = arg ? 1 : 0;
			printk("PROXIMITY_SET_ENABLE--%d\r\n",flag);
			cm36283_sensors_pros_enable(flag);
			if(flag){
				mdelay(150);
				cm36283_prox_get_data();
				cm36283_datap->cm36283_on |= CMC_BIT_PROX;
				enable_irq(cm36283_datap->irq);
			}
			else{
				cm36283_datap->cm36283_on &= ~CMC_BIT_PROX;
				disable_irq_nosync(cm36283_datap->irq);

			}
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

static int prox_open(struct inode *inode, struct file *file)
{
	file->private_data = cm36283_datap;
	return 0;
}
static int prox_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

const struct file_operations cm36283_prox_fops = {
	.owner = THIS_MODULE,
	.read = prox_read,
	.poll = prox_poll,
	.unlocked_ioctl = prox_ioctl,
	.open = prox_open,
	.release = prox_release,
};

/*-------------------------------------hal interface----------------------------------*/

/*-------------------------------------debug------------------------------------------*/
static ssize_t cm36283_store_als(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int flag=0;

	sscanf(buf, "%du", &flag);

	cm36283_sensors_als_on(flag);
	if(flag){
		cm36283_datap->cm36283_on |= CMC_BIT_ALS;
	}
	else{
		cm36283_datap->cm36283_on &= ~CMC_BIT_ALS;
	}
	return count;    
}

static ssize_t cm36283_store_ps(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int flag;
	sscanf(buf, "%du", &flag);

	cm36283_sensors_prox_on(flag);
	if(flag){
		cm36283_datap->cm36283_on |= CMC_BIT_PROX;
		enable_irq(cm36283_datap->irq);
	}
	else{
		cm36283_datap->cm36283_on &= ~CMC_BIT_PROX;
		disable_irq_nosync(cm36283_datap->irq);
	}
	return count;    
}

static ssize_t cm36283_show_reg(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	u16 value0,value1,value2,value3,value4,value5,value6,value7;

	cm36283_i2c_read_reg(CM36283_ALS_CONF_REG,&value0);
	cm36283_i2c_read_reg(CM3605_PS_CONF1_REG,&value1);
	cm36283_i2c_read_reg(CM3605_PS_CONF3_REG,&value2);
	cm36283_i2c_read_reg(CM3605_PS_CNAC_REG,&value3);
	cm36283_i2c_read_reg(CM3605_PS_THD_REG,&value4);
	cm36283_i2c_read_reg(CM3605_PS_DATA_REG,&value5);
	cm36283_i2c_read_reg(CM3605_ALS_DATA_REG,&value6);
	cm36283_i2c_read_reg(CM3605_INT_FLAG_REG,&value7);
	value3 &= 0xff;
	value5 &= 0xff;
	value7 &= 0xff00;
	return snprintf(buf,PAGE_SIZE,"CM36283:als conf=0x%x,ps conf1=0x%x,ps conf2=0x%x,canc = %d,ps th=0x%x,ps=%d,als=%d,int=0x%x\n",
		            value0,value1,value2,value3,value4,value5,value6,value7);
}

static ssize_t cm36283_store_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	u16 reg,value;

	sscanf(buf, "%du %du", (int*)&reg,(int*)&value);
	//printk("%s-reg[%d]=0x%x\r\n",__func__,reg,value);
	cm36283_i2c_write_reg(reg,value);
	return count;    
}

static struct device_attribute als_attribute =__ATTR(als, 0664, NULL, cm36283_store_als);
static struct device_attribute ps_attribute =__ATTR(ps, 0664, NULL, cm36283_store_ps);
static struct device_attribute reg_attribute =__ATTR(reg, 0664, cm36283_show_reg, cm36283_store_reg);

static struct attribute *attrs[] = {
	&als_attribute.attr,	
	&ps_attribute.attr,
	&reg_attribute.attr,
	NULL,};

static struct attribute_group sensor_light_attribute_group = {.attrs = attrs,};
/*-------------------------------------debug------------------------------------------*/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cm36283_early_suspend(struct early_suspend *handler)
{
	cm36283_datap->in_early_suspend = 1;

	//cancel_delayed_work_sync(&cm36283_datap->work_lus);
	//cancel_work_sync(&cm36283_datap->prox_work);
}

static void cm36283_later_resume(struct early_suspend *handler)
{
	cm36283_datap->in_early_suspend = 0;
	if(cm36283_datap->cm36283_on & CMC_BIT_ALS)
		cm36283_datap->light_need_restar_work = 1;

	if(cm36283_datap->light_need_restar_work){
		printk("%s--will restart lux work when resume\r\n",__func__);
		cm36283_datap->light_need_restar_work = 0;
		queue_delayed_work(cm36283_datap->sensor_workqueue,&cm36283_datap->als_work,msecs_to_jiffies(cm36283_datap->light_poll_delay));
	}
}
#endif
static int cm3623_suspend(struct i2c_client *client, pm_message_t mesg)
{
	if(cm36283_datap->cm36283_on == CMC_BIT_PROX)
		enable_irq_wake(cm36283_datap->irq);
	
	cm36283_datap->light_need_restar_work = 0;
	return 0;
}
static int cm3623_resume(struct i2c_client *client)
{
	if(cm36283_datap->cm36283_on == CMC_BIT_PROX)
		disable_irq_wake(cm36283_datap->irq);
	
	if(cm36283_datap->cm36283_on == CMC_BIT_ALS)
		cm36283_datap->light_need_restar_work = 1;
	return 0;
}
static int cm36283_probe(struct i2c_client *clientp, const struct i2c_device_id *idp) {
	int ret = 0;

	this_client = clientp;
	if(cm36283_id_check()){
		printk("%s:device id error!\n", __func__);
		return -1;
	}

	printk("%s, start. \n", __func__);//add for debug

	//res prepare
	cm36283_datap = kmalloc(sizeof(struct cm36283_data), GFP_KERNEL);
	if (!cm36283_datap) {
		printk("%s:Cannot get memory!\n", __func__);
		return -ENOMEM;
	}
	memset(cm36283_datap,0,sizeof(struct cm36283_data));
	
	cm36283_datap->light_poll_delay = LIGHT_DEFAULT_DELAY;
	cm36283_datap->prox_poll_delay = PROXIMITY_DEFAULT_DELAY;
	cm36283_datap->client = clientp;
	i2c_set_clientdata(clientp, cm36283_datap);

	INIT_WORK(&cm36283_datap->prox_work, cm36283_prox_work_func);
	INIT_DELAYED_WORK(&cm36283_datap->als_work, cm36283_als_work_func);

	cm36283_datap->sensor_workqueue = create_singlethread_workqueue(dev_name(&clientp->dev));
	if (!cm36283_datap->sensor_workqueue) {
		ret = -ESRCH;
		goto register_singlethread_failed;
	}
	
	init_waitqueue_head(&cm36283_datap->light_event_wait);
	init_waitqueue_head(&cm36283_datap->proximity_event_wait);

	cm36283_datap->cm36283_cfg_data = clientp->dev.platform_data;
	//light misc register
	cm36283_datap->light_dev.minor = MISC_DYNAMIC_MINOR;
	cm36283_datap->light_dev.name = "light";
	cm36283_datap->light_dev.fops = &cm36283_light_fops;
	ret = misc_register(&(cm36283_datap->light_dev));
	if (ret) {
		printk("%s:Register misc device for light sensor failed(%d)!\n",
				__func__, ret);
		goto register_light_failed;
	}

	//prox misc register
	cm36283_datap->prox_dev.minor = MISC_DYNAMIC_MINOR;
	cm36283_datap->prox_dev.name = "proximity";
	cm36283_datap->prox_dev.fops = &cm36283_prox_fops;
	ret = misc_register(&(cm36283_datap->prox_dev));
	if (ret) {
		printk("%s:Register misc device for light sensor failed(%d)!\n",
				__func__, ret);
		goto register_prox_failed;
	}

	//irq request
	if(clientp->irq){
		cm36283_datap->irq_gpio = irq_to_gpio(clientp->irq);
		printk("irq =[%d],cm36283_datap->irq_gpio = [%d] \n", clientp->irq, cm36283_datap->irq_gpio);
		ret = gpio_request(cm36283_datap->irq_gpio, "cm36283 irq");
		if (ret < 0){
			printk("%s:Gpio request failed!\n", __func__);
			goto request_gpio_faile;
		}
		gpio_direction_input(cm36283_datap->irq_gpio);

		cm36283_datap->irq = gpio_to_irq(cm36283_datap->irq_gpio);
		printk("cm36283_datap->irq = [%d]\n", cm36283_datap->irq);
		ret = request_irq(cm36283_datap->irq, cm36283_irq_handler,
					IRQ_TYPE_LEVEL_LOW, "cm36283_irq", cm36283_datap);
		if (ret != 0) {
			printk("%s:IRQ request failed!\n", __func__);
			goto request_irq_faile;
		}
		disable_irq_nosync(cm36283_datap->irq);
	}

	cm36283_device_init();
	
	ret = sysfs_create_group(&cm36283_datap->light_dev.this_device->kobj,&sensor_light_attribute_group);

#ifdef CONFIG_HAS_EARLYSUSPEND
	cm36283_datap->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
	cm36283_datap->early_suspend.suspend = cm36283_early_suspend;
	cm36283_datap->early_suspend.resume	= cm36283_later_resume;
	register_early_suspend(&cm36283_datap->early_suspend);
	cm36283_datap->in_early_suspend = 0;
#endif

	printk("%s, end. \n", __func__);

	return ret;

request_irq_faile:
	gpio_free(clientp->irq);
request_gpio_faile:
	misc_deregister(&cm36283_datap->prox_dev);
register_prox_failed:
	misc_deregister(&cm36283_datap->light_dev);
register_light_failed:
	destroy_workqueue(cm36283_datap->sensor_workqueue);
register_singlethread_failed:
	kfree(cm36283_datap);
	return (ret);
}

static int cm36283_remove(struct i2c_client *client) {
	int ret = 0;
	free_irq(gpio_to_irq(client->irq), cm36283_datap);
	gpio_free(client->irq);
	
	misc_deregister(&cm36283_datap->prox_dev);
	misc_deregister(&cm36283_datap->light_dev);
	destroy_workqueue(cm36283_datap->sensor_workqueue);
	
	kfree(cm36283_datap);
	cm36283_datap = NULL;
	return (ret);
}

static struct i2c_device_id cm36283_idtable[] = {
		{cm36283_DEVICE_ID, 0},
		{}
};
MODULE_DEVICE_TABLE(i2c, cm36283_idtable);

static struct i2c_driver cm36283_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = cm36283_DEVICE_NAME,
	},
	.id_table = cm36283_idtable,
	.probe = cm36283_probe,
	.suspend = cm3623_suspend,
	.resume = cm3623_resume,
	.remove = cm36283_remove,
};

static int __init cm36283_init(void) {
	return i2c_add_driver(&cm36283_driver);
}

static void __exit cm36283_exit(void) {
	i2c_del_driver(&cm36283_driver);
}

MODULE_AUTHOR("John Koshi - Surya Software");
MODULE_DESCRIPTION("cm36283 ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");

module_init(cm36283_init);
module_exit(cm36283_exit);

