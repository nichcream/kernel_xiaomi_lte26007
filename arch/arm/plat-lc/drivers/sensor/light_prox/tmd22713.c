/*******************************************************************************
*                                                                              *
*   File Name:    taos.c                                                      *
*   Description:   Linux device driver for Taos ambient light and         *
*   proximity sensors.                                     *
*   Author:         John Koshi                                             *
*   History:   09/16/2009 - Initial creation                          *
*           10/09/2009 - Triton version         *
*           12/21/2009 - Probe/remove mode                *
*           02/07/2010 - Add proximity          *
*                                                                              *
********************************************************************************
*    Proprietary to Taos Inc., 1001 Klein Road #300, Plano, TX 75074        *
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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/miscdevice.h>

#include <plat/lightprox.h>
#include <plat/taos_common.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

// device name/id/address/counts
#define TAOS_DEVICE_NAME                "taos"
#define TAOS_DEVICE_ID                  "tritonFN"
#define TAOS_ID_NAME_SIZE               10
#define TAOS_TRITON_CHIPIDVAL           0x00
#define TAOS_TRITON_MAXREGS             32
#define TAOS_DEVICE_ADDR1               0x29
#define TAOS_DEVICE_ADDR2               0x39
#define TAOS_DEVICE_ADDR3               0x49
#define TAOS_MAX_NUM_DEVICES            3
#define TAOS_MAX_DEVICE_REGS            32
#define I2C_MAX_ADAPTERS                8

// TRITON register offsets
#define TAOS_TRITON_CNTRL               0x00
#define TAOS_TRITON_ALS_TIME            0X01
#define TAOS_TRITON_PRX_TIME            0x02
#define TAOS_TRITON_WAIT_TIME           0x03
#define TAOS_TRITON_ALS_MINTHRESHLO     0X04
#define TAOS_TRITON_ALS_MINTHRESHHI     0X05
#define TAOS_TRITON_ALS_MAXTHRESHLO     0X06
#define TAOS_TRITON_ALS_MAXTHRESHHI     0X07
#define TAOS_TRITON_PRX_MINTHRESHLO     0X08
#define TAOS_TRITON_PRX_MINTHRESHHI     0X09
#define TAOS_TRITON_PRX_MAXTHRESHLO     0X0A
#define TAOS_TRITON_PRX_MAXTHRESHHI     0X0B
#define TAOS_TRITON_INTERRUPT           0x0C
#define TAOS_TRITON_PRX_CFG             0x0D
#define TAOS_TRITON_PRX_COUNT           0x0E
#define TAOS_TRITON_GAIN                0x0F
#define TAOS_TRITON_REVID               0x11
#define TAOS_TRITON_CHIPID              0x12
#define TAOS_TRITON_STATUS              0x13
#define TAOS_TRITON_ALS_CHAN0LO         0x14
#define TAOS_TRITON_ALS_CHAN0HI         0x15
#define TAOS_TRITON_ALS_CHAN1LO         0x16
#define TAOS_TRITON_ALS_CHAN1HI         0x17
#define TAOS_TRITON_PRX_LO              0x18
#define TAOS_TRITON_PRX_HI              0x19
#define TAOS_TRITON_TEST_STATUS         0x1F

// Triton cmd reg masks
#define TAOS_TRITON_CMD_REG             0X80
#define TAOS_TRITON_CMD_AUTO            0x20
#define TAOS_TRITON_CMD_BYTE_RW         0x00
#define TAOS_TRITON_CMD_WORD_BLK_RW     0x20
#define TAOS_TRITON_CMD_SPL_FN          0x60
#define TAOS_TRITON_CMD_PROX_INTCLR     0X05
#define TAOS_TRITON_CMD_ALS_INTCLR      0X06
#define TAOS_TRITON_CMD_PROXALS_INTCLR  0X07
#define TAOS_TRITON_CMD_TST_REG         0X08
#define TAOS_TRITON_CMD_USER_REG        0X09

// Triton cntrl reg masks
#define TAOS_TRITON_CNTL_PROX_INT_ENBL  0X20
#define TAOS_TRITON_CNTL_ALS_INT_ENBL   0X10
#define TAOS_TRITON_CNTL_WAIT_TMR_ENBL  0X08
#define TAOS_TRITON_CNTL_PROX_DET_ENBL  0X04
#define TAOS_TRITON_CNTL_ADC_ENBL       0x02
#define TAOS_TRITON_CNTL_PWRON          0x01

// Triton status reg masks
#define TAOS_TRITON_STATUS_ADCVALID     0x01
#define TAOS_TRITON_STATUS_PRXVALID     0x02
#define TAOS_TRITON_STATUS_ADCINTR      0x10
#define TAOS_TRITON_STATUS_PRXINTR      0x20

// lux constants
#define TAOS_MAX_LUX                    10000
#define TAOS_SCALE_MILLILUX             3
#define TAOS_FILTER_DEPTH               3
#define CHIP_ID                         0x3d

// forward declarations
static int taos_probe(struct i2c_client *clientp, const struct i2c_device_id *idp);
static int taos_remove(struct i2c_client *client);
static int taos_get_lux(void);
static int taos_lux_filter(int raw_lux);
static int taos_als_threshold_set(void);
static int taos_prox_threshold_set(void);
static int taos_als_get_data(void);
static int taos_sensors_als_on(void);
static int taos_sensors_prox_on(void);


DECLARE_WAIT_QUEUE_HEAD(waitqueue_read);

#define ALS_PROX_DEBUG

static char pro_buf[4];
static int mcount = 0;
static char als_buf[4];
static bool enable_irq_mask = (bool)0;
// per-device data
struct taos_data {
	struct i2c_client *client;
	struct taos_cfg *taos_cfg_data;
	struct work_struct work;
	struct delayed_work work_light;
	struct workqueue_struct *workqueue;
	struct wake_lock taos_wake_lock;
	struct semaphore update_lock;
	struct miscdevice light_dev;
	struct miscdevice prox_dev;
	wait_queue_head_t light_event_wait;
	wait_queue_head_t proximity_event_wait;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int lux_state;
	int light_event;
	int light_poll_delay;
	int prox_state;
	int prox_event;
	int prox_poll_delay;
	int irq;
};
static struct taos_data *taos_datap;

struct taos_prox_info prox_cal_info[20];
struct taos_prox_info prox_cur_info;
struct taos_prox_info *prox_cur_infop = &prox_cur_info;

static int prox_work = 0;
static int prox_on = 0;
static int als_on = 0;
static u16 sat_als = 0;

// device reg init values
const static u8 taos_triton_reg_init[16] = {
	0x00, 0xff, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00
};

// lux time scale
struct time_scale_factor  {
	u16 numerator;
	u16 denominator;
	u16 saturation;
};
static struct time_scale_factor TritonTime = {1, 0, 0};
static struct time_scale_factor *lux_timep = &TritonTime;

// gain table
const static u8 taos_triton_gain_table[] = {1, 8, 16, 120};

// lux data
struct lux_data {
	u16 ratio;
	u16 clear;
	u16 ir;
};
static struct lux_data TritonFN_lux_data[] = {
	{ 9830,  8320,  15360 },
	{ 12452, 10554, 22797 },
	{ 14746, 6234,  11430 },
	{ 17695, 3968,  6400  },
	{ 0,     0,     0     }
};
static struct lux_data *lux_tablep = TritonFN_lux_data;
static int lux_history[TAOS_FILTER_DEPTH] = {-ENODATA, -ENODATA, -ENODATA};

#define TAOS_FUNC_TRACE() printk("fun[%s]:line[%d]\n", __func__,__LINE__)

#ifdef CONFIG_HAS_EARLYSUSPEND
static void taos_early_suspend(struct early_suspend *h);
static void taos_late_resume(struct early_suspend *h);
#endif

static int taos_i2c_read_byte(struct taos_data *data, u8 addr, u8 *buf)
{
	struct i2c_msg msg[2];
	msg[0].addr = data->client->addr;
	msg[0].flags= 0;
	msg[0].buf = &addr;
	msg[0].len = 1;

	msg[1].addr = data->client->addr;
	msg[1].flags= I2C_M_RD;
	msg[1].buf  = buf;
	msg[1].len  = 1;

	return i2c_transfer(data->client->adapter, msg, 2);
}

static int taos_i2c_write_byte(struct taos_data *data, u8 addr, u8 value)
{
	return i2c_smbus_write_byte_data(data->client, addr, value);
}

static int taos_i2c_write_cmd(struct taos_data *data, u8 command)
{
	struct i2c_msg msg[1];
	msg[0].addr = data->client->addr;
	msg[0].flags= 0;
	msg[0].buf  = &command;
	msg[0].len  = 1;
	return i2c_transfer(data->client->adapter, msg, 1);
}

static irqreturn_t taos_irq_handler(int irq, void *dev_id)
{
	disable_irq_nosync(taos_datap->irq);
	schedule_work(&taos_datap->work);
	enable_irq(taos_datap->irq);
	return IRQ_HANDLED;
}

static void light_work_func(struct work_struct *work)
{
	taos_als_threshold_set();
	taos_als_get_data();

	queue_delayed_work(taos_datap->workqueue, &taos_datap->work_light, 
		msecs_to_jiffies(taos_datap->light_poll_delay));
}

static int taos_get_data(void)
{
	int ret = 0;
	u8 status = 0;
	ret = taos_i2c_read_byte(taos_datap,
					TAOS_TRITON_CMD_REG | TAOS_TRITON_STATUS, &status);

	if (status & TAOS_TRITON_STATUS_PRXINTR) {
		ret |= taos_prox_threshold_set();
	}
	return ret;
}

static int taos_interrupts_clear(struct taos_data *taos_data)
{
	return taos_i2c_write_cmd(taos_data,
		TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | 0x07);
}

static void taos_work_func(struct work_struct * work)
{
	struct taos_data *taos_data = container_of(work, struct taos_data, work);
	wake_lock(&taos_datap->taos_wake_lock);
	taos_get_data();
	/*<after read data done, taos will not clear interrupts self-motion>*/
	taos_interrupts_clear(taos_data);
	wake_unlock(&taos_datap->taos_wake_lock);
}

static int taos_als_get_data(void)
{
	int ret = 0;
	u8 reg_val;
	int lux_val = 0;

	ret = taos_i2c_read_byte(taos_datap,
				TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL, &reg_val);
	if (ret < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_data\n");
		return (ret);
	}
	if ((reg_val & (TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON)) !=
					(TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON))
				return -ENODATA;

	ret = taos_i2c_read_byte(taos_datap,
				TAOS_TRITON_CMD_REG | TAOS_TRITON_STATUS, &reg_val);
	if (ret < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_data\n");
		return (ret);
	}
	if ((reg_val & TAOS_TRITON_STATUS_ADCVALID) != TAOS_TRITON_STATUS_ADCVALID)
			return -ENODATA;

	if ((lux_val = taos_get_lux()) < 0)
			printk(KERN_ERR "TAOS: call to taos_get_lux() returned error %d in ioctl als_data\n", lux_val);
	//lux_val = taos_lux_filter(lux_val);
	taos_datap->lux_state = lux_val;
	taos_datap->light_event = 1;
	wake_up_interruptible(&taos_datap->light_event_wait);
	return ret;
}

static int taos_als_threshold_set(void)
{
	int i,ret = 0;
	u8 chdata[2];
	u16 ch0;
	struct taos_cfg *cfg = taos_datap->taos_cfg_data;

	for (i = 0; i < 2; i++) {
	taos_i2c_read_byte(taos_datap, TAOS_TRITON_CMD_REG |
		TAOS_TRITON_CMD_WORD_BLK_RW | (TAOS_TRITON_ALS_CHAN0LO + i), &chdata[i]);
	}
	ch0 = chdata[0] + chdata[1]*256;
	cfg->als_threshold_hi = (12 * ch0) / 10;
	if (cfg->als_threshold_hi >= 65535)
	cfg->als_threshold_hi = 65535;
	cfg->als_threshold_lo = (8 * ch0) / 10;
	als_buf[0] = cfg->als_threshold_lo & 0x0ff;
	als_buf[1] = cfg->als_threshold_lo >> 8;
	als_buf[2] = cfg->als_threshold_hi & 0x0ff;
	als_buf[3] = cfg->als_threshold_hi >> 8;

	for (mcount=0; mcount<4; mcount++) {
	taos_i2c_write_byte(taos_datap, (TAOS_TRITON_CMD_REG | 0x04) + mcount,
					als_buf[mcount]);
	}
	return ret;
}

static int taos_prox_threshold_set(void)
{
	int i,ret = 0;
	u8 chdata[6];
	u16 proxdata = 0;
	u16 cleardata = 0;
	struct taos_cfg *taos_cfgp = taos_datap->taos_cfg_data;

	for (i = 0; i < 6; i++) {
		taos_i2c_read_byte(taos_datap, TAOS_TRITON_CMD_REG |
			TAOS_TRITON_CMD_WORD_BLK_RW| (TAOS_TRITON_ALS_CHAN0LO + i),
			&chdata[i]);
	}
	cleardata = chdata[0] + chdata[1]*256;
	proxdata = chdata[4] + chdata[5]*256;
	if (prox_on || proxdata < taos_cfgp->prox_threshold_lo ) {
		pro_buf[0] = 0x0;
		pro_buf[1] = 0x0;
		pro_buf[2] = taos_cfgp->prox_threshold_hi & 0x0ff;
		pro_buf[3] = taos_cfgp->prox_threshold_hi >> 8;
		/* Update prox device state */
		taos_datap->prox_state = 1;//faraway
	} else if (proxdata > taos_cfgp->prox_threshold_hi ){
		if (cleardata > ((sat_als*80)/100))
			return -ENODATA;
		pro_buf[0] = taos_cfgp->prox_threshold_lo & 0x0ff;
		pro_buf[1] = taos_cfgp->prox_threshold_lo >> 8;
		pro_buf[2] = 0xff;
		pro_buf[3] = 0xff;
		/* Update prox device state */
		taos_datap->prox_state = 0;//near
	}

	printk(KERN_DEBUG "taos prox info, proxdata = 0x%x, prox_state = %d\n", proxdata, taos_datap->prox_state);
	taos_datap->prox_event = 1;
	for (mcount=0; mcount<4; mcount++ ) {
		taos_i2c_write_byte(taos_datap, (TAOS_TRITON_CMD_REG | 0x08) + mcount,
					pro_buf[mcount]);
		}

	wake_up_interruptible(&taos_datap->proximity_event_wait);
	prox_on = 0;
	return ret;
}

static ssize_t light_read(struct file *file,
				 char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct taos_data *data = (struct taos_data*)file->private_data;

	//TAOS_FUNC_TRACE();
	if (!data->light_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->light_event_wait, data->light_event);
	}

	if (!data->light_event || !buf)
		return 0;

	len = sizeof(data->lux_state);
	err = copy_to_user(buf, &data->lux_state, len);
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
	struct taos_data *data = (struct taos_data*)file->private_data;

	poll_wait(file, &data->light_event_wait, poll);
	if (data->light_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static long light_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;
	struct taos_data * data = file->private_data;
	u8 reg_val = 0;
	int ret=0;

	//TAOS_FUNC_TRACE();
	switch (cmd) {
		case LIGHT_SET_DELAY:
			arg = arg / 1000000;
			if (arg > LIGHT_MAX_DELAY)
				arg = LIGHT_MAX_DELAY;
			else if (arg < LIGHT_MIN_DELAY)
				arg = LIGHT_MIN_DELAY;
			data->light_poll_delay = arg;
			break;
		case LIGHT_SET_ENABLE:
			flag = arg ? 1 : 0;
			if (prox_work == 0)
			{
				if (flag)
				{
					printk(KERN_DEBUG "######## TAOS IOCTL ALS ON #########\n");
					taos_sensors_als_on();
					queue_delayed_work(taos_datap->workqueue, &taos_datap->work_light, 
						msecs_to_jiffies(taos_datap->light_poll_delay));
				}
				else
				{
					printk(KERN_DEBUG "######## TAOS IOCTL ALS OFF #########\n");
					cancel_delayed_work_sync(&taos_datap->work_light);
					if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
						printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
						return (ret);
					}
					reg_val = i2c_smbus_read_byte(taos_datap->client);
					if ((reg_val & TAOS_TRITON_CNTL_PROX_DET_ENBL) == 0x0) 
					{
						 if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), 0x00))) < 0) 
						 {
							 printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_off\n");
							 return (ret);
						 }
						 //cancel_work_sync(&taos_datap->work);//golden
					}
				}
				als_on = flag;
			}
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

static int light_open(struct inode *inode, struct file *file)
{
	TAOS_FUNC_TRACE();
	file->private_data = taos_datap;
	if (!enable_irq_mask)
	{
		enable_irq(taos_datap->irq);
		enable_irq_mask = 1;
	}
	return 0;
}
static int light_release(struct inode *inode, struct file *file)
{
	TAOS_FUNC_TRACE();
	file->private_data = NULL;
	return 0;
}

const struct file_operations taos_light_fops = {
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
	struct taos_data *data = (struct taos_data*)file->private_data;

	if (!data->prox_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->proximity_event_wait, data->prox_event);
	}

	if (!data->prox_event|| !buf)
		return 0;

	len = sizeof(data->prox_state);
	err = copy_to_user(buf, &data->prox_state, len);
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
	struct taos_data *data = (struct taos_data*)file->private_data;
	poll_wait(file, &data->proximity_event_wait, poll);
	if (data->prox_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static long prox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;
	struct taos_data * data = file->private_data;
	int ret;
	switch (cmd) {
		case PROXIMITY_SET_DELAY:
			if (arg > PROXIMITY_MAX_DELAY)
				arg = PROXIMITY_MAX_DELAY;
			else if (arg < PROXIMITY_MIN_DELAY)
				arg = PROXIMITY_MIN_DELAY;
			data->prox_poll_delay = arg;
			break;
		case PROXIMITY_SET_ENABLE:
			flag = arg ? 1 : 0;
			if (flag)
			{
				prox_work = 1;
				printk(KERN_DEBUG "^^^^^^^^^ TAOS IOCTL PROX ON  ^^^^^^^^\n");
				taos_sensors_prox_on();
				enable_irq_wake(taos_datap->irq);//add for wake sys during call suspend
			}
			else
			{
				prox_work = 0;
				printk(KERN_DEBUG "^^^^^^^^^ TAOS IOCTL PROX OFF  ^^^^^^^\n");
				disable_irq_wake(taos_datap->irq);//disable wakesource before off prox
				if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), 0x00))) < 0)
				{
						printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_off\n");
						return (ret);
					prox_on = 0;
				}
				if(als_on)
					taos_sensors_als_on();
			}
			break;
		default:
		retval = -EINVAL;
	}
	return retval;
}

static int prox_open(struct inode *inode, struct file *file)
{
	TAOS_FUNC_TRACE();
	file->private_data = taos_datap;
	if (!enable_irq_mask)
	{
		enable_irq(taos_datap->irq);
		enable_irq_mask = 1;
	}
	return 0;
}
static int prox_release(struct inode *inode, struct file *file)
{
	TAOS_FUNC_TRACE();
	file->private_data = NULL;
	return 0;
}

const struct file_operations taos_prox_fops = {
	.owner = THIS_MODULE,
	.read = prox_read,
	.poll = prox_poll,
	.unlocked_ioctl = prox_ioctl,
	.open = prox_open,
	.release = prox_release,
};


static int taos_probe(struct i2c_client *clientp, const struct i2c_device_id *idp) {
	int ret = 0;
	int chip_id;
	chip_id = i2c_smbus_read_byte_data(clientp,
			(TAOS_TRITON_CMD_REG | (TAOS_TRITON_CNTRL + 12)));
	if (chip_id < 0) {
		printk("%s: Read chip id failed(%d)!\n", __func__, chip_id);
		return chip_id;
	}
	/* TSL27711=0x00 TSL27713=0x09 TMD27711=0x20 TMD27713=0x29 */
	taos_datap = kmalloc(sizeof(struct taos_data), GFP_KERNEL);
	if (!taos_datap) {
		printk("%s:Cannot get memory!\n", __func__);
		return -ENOMEM;
	}
	memset(taos_datap,0,sizeof(struct taos_data));

	wake_lock_init(&taos_datap->taos_wake_lock, WAKE_LOCK_SUSPEND, "taos-wake-lock");

	taos_datap->light_poll_delay = LIGHT_DEFAULT_DELAY;
	taos_datap->prox_poll_delay = PROXIMITY_DEFAULT_DELAY;
	taos_datap->client = clientp;
	i2c_set_clientdata(clientp, taos_datap);
	INIT_WORK(&(taos_datap->work),taos_work_func);
	init_waitqueue_head(&taos_datap->light_event_wait);
	init_waitqueue_head(&taos_datap->proximity_event_wait);
	taos_datap->taos_cfg_data = clientp->dev.platform_data;

	taos_datap->light_dev.minor = MISC_DYNAMIC_MINOR;
	taos_datap->light_dev.name = "light";
	taos_datap->light_dev.fops = &taos_light_fops;

	ret = misc_register(&(taos_datap->light_dev));
	if (ret) {
		printk("%s:Register misc device for light sensor failed(%d)!\n",
				__func__, ret);
		goto register_light_failed;
	}

	taos_datap->prox_dev.minor = MISC_DYNAMIC_MINOR;
	taos_datap->prox_dev.name = "proximity";
	taos_datap->prox_dev.fops = &taos_prox_fops;

	ret = misc_register(&(taos_datap->prox_dev));
	if (ret) {
		printk("%s:Register misc device for light sensor failed(%d)!\n",
				__func__, ret);
		goto register_light_failed;
	}
	if ((ret = (i2c_smbus_write_byte(clientp, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
				printk(KERN_ERR "TAOS: i2c_smbus_write_byte() to control reg failed in taos_probe()\n");
				goto register_prox_failed;
		}

	sat_als = (256 - taos_datap->taos_cfg_data->prox_int_time) << 10;

	/*dmobile ::power down for init ,Rambo liu*/
	printk("Rambo::light sensor will pwr down \n");
	if ((ret = i2c_smbus_write_byte_data(taos_datap->client,
				TAOS_TRITON_CMD_REG, 0x00)) < 0) {
				printk(KERN_ERR "%s:Set init state powerdown failed!\n", __func__);
				return (ret);
		}

	taos_datap->workqueue = create_singlethread_workqueue("taos_light");
	INIT_DELAYED_WORK(&taos_datap->work_light, light_work_func);

	ret = gpio_request(taos_datap->taos_cfg_data->int_gpio, "taos irq");
	if (ret < 0){
		printk("%s:Gpio request failed!\n", __func__);
		goto request_gpio_faile;
	}
	gpio_direction_input(taos_datap->taos_cfg_data->int_gpio);
	taos_datap->irq = gpio_to_irq(taos_datap->taos_cfg_data->int_gpio);
	ret = request_irq(taos_datap->irq, taos_irq_handler,
				IRQ_TYPE_EDGE_FALLING, "taos_irq", taos_datap);
	if (ret != 0) {
		printk("%s:IRQ request failed!\n", __func__);
		goto request_irq_faile;
	}

	disable_irq_nosync(taos_datap->irq);

#ifdef CONFIG_HAS_EARLYSUSPEND
	taos_datap->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	taos_datap->early_suspend.suspend = taos_early_suspend;
	taos_datap->early_suspend.resume = taos_late_resume;
	register_early_suspend(&taos_datap->early_suspend);
#endif

	return ret;

request_irq_faile:
request_gpio_faile:
register_prox_failed:
register_light_failed:
	return (ret);
}

static int taos_sensors_prox_on()
{
	struct taos_cfg *taos_cfgp = taos_datap->taos_cfg_data;
	u8 reg_cntrl = 0;
	int ret = 0;
	prox_on = 1;

	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x01), taos_cfgp->prox_int_time))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x02), taos_cfgp->prox_adc_time))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x03), taos_cfgp->prox_wait_time))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}

	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0C), taos_cfgp->prox_intr_filter))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0D), taos_cfgp->prox_config))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0E), taos_cfgp->prox_pulse_cnt))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG|0x0F), taos_cfgp->prox_gain))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}
	reg_cntrl = TAOS_TRITON_CNTL_PROX_DET_ENBL | TAOS_TRITON_CNTL_PWRON | TAOS_TRITON_CNTL_PROX_INT_ENBL |
												TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_WAIT_TMR_ENBL  ;
	if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), reg_cntrl))) < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl prox_on\n");
		return (ret);
	}
	taos_prox_threshold_set();
	return ret;
}

static int taos_sensors_als_on(void) {
	int  ret = 0, i = 0;
	u8 itime = 0, reg_val = 0, reg_cntrl = 0;
	struct taos_cfg *taos_cfgp = taos_datap->taos_cfg_data;

	for (i = 0; i < TAOS_FILTER_DEPTH; i++) {
		lux_history[i] = -ENODATA;
	}

	/*ALS interrupt clear*/
	ret = taos_i2c_write_cmd(taos_datap,
		TAOS_TRITON_CMD_REG | TAOS_TRITON_CMD_SPL_FN | TAOS_TRITON_CMD_ALS_INTCLR);
	if (ret < 0) {
		printk(KERN_ERR "TAOS: taos_i2c_write_cmd failed in %s!\n",__func__);
		return (ret);
	}

	itime = (((taos_cfgp->als_time/50) * 18) - 1);
	itime = (~itime);

	ret = taos_i2c_write_byte(taos_datap,
				TAOS_TRITON_CMD_REG | TAOS_TRITON_ALS_TIME, itime);
	if (ret < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
		return (ret);
	}

	ret = taos_i2c_write_byte(taos_datap,
				TAOS_TRITON_CMD_REG | TAOS_TRITON_INTERRUPT, taos_cfgp->prox_intr_filter);
	if (ret < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
		return (ret);
	}

	ret = taos_i2c_read_byte(taos_datap,
				TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN, &reg_val);
	reg_val &= 0xfc;
	reg_val |= (taos_cfgp->gain & 0x03);

	ret = taos_i2c_write_byte(taos_datap,
				TAOS_TRITON_CMD_REG | TAOS_TRITON_GAIN, reg_val);
	if (ret < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
		return (ret);
	}

	reg_cntrl = TAOS_TRITON_CNTL_ADC_ENBL | TAOS_TRITON_CNTL_PWRON;
	ret = taos_i2c_write_byte(taos_datap,
				TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL, reg_cntrl);
	if (ret < 0) {
		printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_on\n");
		return (ret);
	}
	taos_als_threshold_set();
	return ret;
}

// read/calculate lux value
static int taos_get_lux(void) {
	u16 raw_clear = 0, raw_ir = 0, raw_lux = 0;
	u32 lux = 0;
	u32 ratio = 0;
	u8 dev_gain = 0;
	u16 Tint = 0;
	struct lux_data *p;
	int ret = 0;
	u8 chdata[4];
	int tmp = 0, i = 0;
	struct taos_cfg *taos_cfgp = taos_datap->taos_cfg_data;

	for (i = 0; i < 4; i++) {
	ret = taos_i2c_read_byte(taos_datap,
			TAOS_TRITON_CMD_REG | (TAOS_TRITON_ALS_CHAN0LO + i), &chdata[i]);
	}
	//printk("ch0=%d\n",chdata[0]+chdata[1]*256);
	//printk("ch1=%d\n",chdata[2]+chdata[3]*256);

	tmp = (taos_cfgp->als_time + 25)/50; //if atime =100  tmp = (atime+25)/50=2.5   tine = 2.7*(256-atime)=  412.5
	TritonTime.numerator = 1;
	TritonTime.denominator = tmp;

	tmp = 300 * taos_cfgp->als_time; //tmp = 300*atime  400
	if(tmp > 65535)
			tmp = 65535;
	TritonTime.saturation = tmp;
	raw_clear = chdata[1];
	raw_clear <<= 8;
	raw_clear |= chdata[0];
	raw_ir    = chdata[3];
	raw_ir    <<= 8;
	raw_ir    |= chdata[2];

	raw_clear *= (taos_cfgp->scale_factor );
	raw_ir *= (taos_cfgp->scale_factor );

	if(raw_ir > raw_clear) {
			raw_lux = raw_ir;
			raw_ir = raw_clear;
			raw_clear = raw_lux;
	}
	dev_gain = taos_triton_gain_table[taos_cfgp->gain & 0x3];
	if(raw_clear >= lux_timep->saturation)
			return(TAOS_MAX_LUX);
	if(raw_ir >= lux_timep->saturation)
			return(TAOS_MAX_LUX);
	if(raw_clear == 0)
			return(0);
	if(dev_gain == 0 || dev_gain > 127) {
			printk(KERN_ERR "TAOS: dev_gain = 0 or > 127 in taos_get_lux()\n");
			return -1;
	}
	if(lux_timep->denominator == 0) {
			printk(KERN_ERR "TAOS: lux_timep->denominator = 0 in taos_get_lux()\n");
			return -1;
	}
	ratio = (raw_ir<<15)/raw_clear;
	for (p = lux_tablep; p->ratio && p->ratio < ratio; p++);
	if(!p->ratio) {
		if(lux_history[0] < 0)
			return 0;
		else
			return lux_history[0];
	}
	Tint = taos_cfgp->als_time;
	raw_clear = ((raw_clear*400 + (dev_gain>>1))/dev_gain + (Tint>>1))/Tint;
	raw_ir = ((raw_ir*400 +(dev_gain>>1))/dev_gain + (Tint>>1))/Tint;
	lux = ((raw_clear*(p->clear)) - (raw_ir*(p->ir)));
	lux = (lux + 32000)/64000;
	if(lux > TAOS_MAX_LUX) {
			lux = TAOS_MAX_LUX;
	}
	return(lux);
}

static int taos_lux_filter(int lux)
{
	const u8 middle[] = {1, 0, 2, 0, 0, 2, 0, 1};/*<what deep mean?>*/
	int index;

	lux_history[2] = lux_history[1];
	lux_history[1] = lux_history[0];
	lux_history[0] = lux;

	if (lux_history[2] < 0) {
		if(lux_history[1] > 0)
			return lux_history[1];
		else
			return lux_history[0];
	}
	index = 0;
	if (lux_history[0] > lux_history[1])
		index += 4;
	if (lux_history[1] > lux_history[2])
		index += 2;
	if (lux_history[0] > lux_history[2])
		index++;
	return (lux_history[middle[index]]);
}

static struct i2c_device_id taos_idtable[] = {
		{TAOS_DEVICE_ID, 0},
		{}
};
MODULE_DEVICE_TABLE(i2c, taos_idtable);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void taos_early_suspend(struct early_suspend *h)
{
	struct taos_data *taos =
		container_of(h, struct taos_data, early_suspend);
	int ret;
	u8 reg_val = 0;

	if(!prox_work && als_on){
		printk(KERN_DEBUG "tmd22713t early_suspend.\n");
		if ((ret = (i2c_smbus_write_byte(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL)))) < 0) {
			printk(KERN_ERR "TAOS: i2c_smbus_write_byte failed in ioctl als_calibrate\n");
		}
		reg_val = i2c_smbus_read_byte(taos_datap->client);
		if ((reg_val & TAOS_TRITON_CNTL_PROX_DET_ENBL) == 0x0)
		{
			if ((ret = (i2c_smbus_write_byte_data(taos_datap->client, (TAOS_TRITON_CMD_REG | TAOS_TRITON_CNTRL), 0x00))) < 0)
			{
				printk(KERN_ERR "TAOS: i2c_smbus_write_byte_data failed in ioctl als_off\n");
			}
			//cancel_work_sync(&taos_datap->work);//golden
		}
	}

	if (als_on)
		cancel_delayed_work_sync(&taos->work_light);

}
static void taos_late_resume(struct early_suspend *h)
{
	struct taos_data *taos =
		container_of(h, struct taos_data, early_suspend);
	if(!prox_work && als_on){
		printk(KERN_DEBUG "tmd22713t late resume.\n");
		taos_sensors_als_on();
	}

	if (als_on)
		queue_delayed_work(taos_datap->workqueue, &taos_datap->work_light, 
			msecs_to_jiffies(taos_datap->light_poll_delay));

}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static struct i2c_driver taos_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = TAOS_DEVICE_NAME,
	},
	.id_table = taos_idtable,
	.probe = taos_probe,
	.remove = taos_remove,
};

static int __init taos_init(void) {
	return i2c_add_driver(&taos_driver);
}

static void __exit taos_exit(void) {
	i2c_del_driver(&taos_driver);
}

static int taos_remove(struct i2c_client *client) {
	int ret = 0;
	return (ret);
}

MODULE_AUTHOR("John Koshi - Surya Software");
MODULE_DESCRIPTION("TAOS ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");

module_init(taos_init);
module_exit(taos_exit);

