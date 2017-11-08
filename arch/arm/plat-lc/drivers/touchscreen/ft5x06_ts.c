/*
 * drivers/input/touchscreen/ft5x06_ts.c
 *
 * FocalTech ft5x06 TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION      	DATE			AUTHOR        Note
 *    1.0		  2010-01-05			WenFS    only support mulititouch	Wenfs 2010-10-01
 *    2.0          2011-09-05                   Duxx      Add touch key, and project setting update, auto CLB command
 *
 *
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <plat/comip-pmic.h>
#include <plat/ft5x06_ts.h>
#include <plat/comip_devinfo.h>
#include <linux/input/mt.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/input/mt.h>
#ifdef CTP_CHARGER_DETECT
#include <plat/comip-pmic.h>
#endif

#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif
#if defined SYSFS_DEBUG || defined FTS_APK_DEBUG
#include "ft5x06_ex_fun.h"
#endif

#ifdef FT5X06_DEBUG
#define FT5X06_PRINT(fmt, args...) printk(KERN_ERR "FT5X06: " fmt, ##args)
#else
#define FT5X06_PRINT(fmt, args...) printk(KERN_DEBUG "FT5X06: " fmt, ##args)
#endif

struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];              //x coordinate
	u16 au16_y[CFG_MAX_TOUCH_POINTS];              //y coordinate
	u8  au8_touch_event[CFG_MAX_TOUCH_POINTS];     //touch event:  0 -- down; 1-- contact; 2 -- contact
	u8  au8_finger_id[CFG_MAX_TOUCH_POINTS];       //touch ID
        u8  au8_last_id[CFG_MAX_TOUCH_POINTS];
        u8  au8_last_event[CFG_MAX_TOUCH_POINTS];
	u8  touch_point;
};

struct ft5x06_ts_data {
	char 			fts_ic[20];	/*driver ic name*/
	unsigned char 		fw_ver;
        unsigned char           vid;
	struct input_dev	*input_dev;
	struct ts_event		event;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;
	struct early_suspend	early_suspend;
	struct early_suspend	very_early_suspend;
	struct ft5x06_info	*info;
	spinlock_t 		irq_lock;
	int			irq_is_disable;
	int 			irq;
	int 			suspend;
	unsigned int proxen_flag;
	unsigned int tp_power_flag;
#ifdef CTP_CHARGER_DETECT
        struct notifier_block	nb;
        struct work_struct	usb_work;
        unsigned long   usb_event;
        unsigned int    is_charger_plug;
        unsigned int    pre_charger_status;
        unsigned int	resume_end_flag;
#endif
};

#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
#include <plat/lightprox.h>

struct fts_proximity_data {
	struct miscdevice       prox_dev;
	struct ft5x06_ts_data	*ts_data;
	wait_queue_head_t       proximity_event_wait;
	int                     prox_distance;
	int                     prox_mode;
	int                     prox_event;
	int                     prox_poll_delay;
	u16                     prox_val;
	u8                      face_detect_enable;	/**/
	u8                      is_face_detect;		/*check face detect*/
};
static struct fts_proximity_data 		*fts_proximity_datap;
#endif

#if CFG_SUPPORT_TOUCH_KEY
static int tsp_keycodes[CFG_NUMOFKEYS] = {
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
};

static char *tsp_keyname[CFG_NUMOFKEYS] = {
	"Menu",
	"Home",
	"Back",
};

static bool tsp_keystatus[CFG_NUMOFKEYS];
#endif


static struct Upgrade_Info fts_updateinfo[ ] = {
	{0x55,"FT5x06",5,50, 30, 0x79, 0x03, 1, 2000},
	{0x08,"FT5606",5,50, 30, 0x79, 0x06, 100, 2000},
	{0x0a,"FT5x16",5,50, 30, 0x79, 0x07, 1, 1500},
	{0x05,"FT6208",2,60, 30, 0x79, 0x05, 10, 2000},
	{0x06,"FT6x06",2,100, 30, 0x79, 0x08, 10, 2000},
	{0x55,"FT5x06i",5,50, 30, 0x79, 0x03, 1, 2000},
	{0x14,"FT5336",5,30, 30, 0x79, 0x11, 10, 2000},
	{0x13,"FT3316",5,30, 30, 0x79, 0x11, 10, 2000},
	{0x12,"FT5436i",5,30, 30, 0x79, 0x11, 10, 2000},
	{0x11,"FT5336i",5,30, 30, 0x79, 0x11, 10, 2000},
};
struct Upgrade_Info fts_updateinfo_curr;
static struct i2c_client *this_client;
static DEFINE_MUTEX(tp_mutex);

static int ft5x06_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		FT5X06_PRINT("i2c read error: %d\n", ret);

	return ret;
}

static int ft5x06_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		FT5X06_PRINT("write error: %d\n", ret);

	return ret;
}

static int ft5x06_write_reg(u8 addr, u8 para)
{
	u8 buf[3];
	int ret = -1;

	buf[0] = addr;
	buf[1] = para;
	ret = ft5x06_i2c_txdata(buf, 2);
	if (ret < 0) {
		FT5X06_PRINT("write reg failed! %#x ret: %d", buf[0], ret);
		return -1;
	}

	return 0;
}

static int ft5x06_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[2];
	u8 data = 0;

	buf[0] = addr;    //register address
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;
	msgs[1].addr = this_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &data;

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		FT5X06_PRINT("i2c read error: %d\n", ret);

	*pdata = data;
	return ret;
}

#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
static int fts_sensors_pros_enable(int onoff)
{
	int ret;

	ret = ft5x06_write_reg(FT_FACE_DETECT_REG, onoff);
	if(ret < 0 )
		return ret;

	return 0;
}

static int fts_prox_get_data(void)
{

	//FT5X06_PRINT("ps=0x%x\r\n",fts_proximity_datap->is_face_detect);
	if(fts_proximity_datap->is_face_detect == FT_FACE_DETECT_ON) {
		if(fts_proximity_datap->prox_distance == 1) {
			FT5X06_PRINT("fts_proximity:close\r\n");
		}
		fts_proximity_datap->prox_distance = 0;
	} else if(fts_proximity_datap->is_face_detect == FT_FACE_DETECT_OFF) {
		if(fts_proximity_datap->prox_distance == 0) {
			FT5X06_PRINT("fts_proximity:far\r\n");
		}
		fts_proximity_datap->prox_distance = 1;
	}
	fts_proximity_datap->prox_event = 1;
	wake_up_interruptible(&fts_proximity_datap->proximity_event_wait);

	return 0;
}

static ssize_t prox_read(struct file *file,
                         char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct fts_proximity_data *data = (struct fts_proximity_data*)file->private_data;

	if (!data->prox_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->proximity_event_wait, data->prox_event);
	}

	if (!data->prox_event|| !buf)
		return 0;

	len = sizeof(data->prox_distance);
	err = copy_to_user(buf, &data->prox_distance, len);
	if (err) {
		FT5X06_PRINT("Copy to user returned %d\n", err);
		return -EFAULT;
	}

	data->prox_event = 0;
	return len;
}

static unsigned int prox_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct fts_proximity_data *data = (struct fts_proximity_data*)file->private_data;

	poll_wait(file, &data->proximity_event_wait, poll);
	if (data->prox_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static long prox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	struct fts_proximity_data *data = (struct fts_proximity_data*)file->private_data;

	switch (cmd) {
		case PROXIMITY_SET_DELAY:
			if (arg > PROXIMITY_MAX_DELAY)
				arg = PROXIMITY_MAX_DELAY;
			else if (arg < PROXIMITY_MIN_DELAY)
				arg = PROXIMITY_MIN_DELAY;
			FT5X06_PRINT("PROXIMITY_SET_DELAY--%d\r\n",(int)arg);
			data->prox_poll_delay = arg;
			break;
		case PROXIMITY_SET_ENABLE:
			mutex_lock(&tp_mutex);
			data->ts_data->proxen_flag = arg ? 1 : 0;
			FT5X06_PRINT("PROXIMITY_SET_ENABLE--%d\r\n",data->ts_data->proxen_flag);
			if(data->ts_data->tp_power_flag == 1) {
				data->ts_data->proxen_flag = 0;
				mutex_unlock(&tp_mutex);
				retval = -EINVAL;
				FT5X06_PRINT("PROXIMITY_SET_ENABLE ERROR--%d(tp power already down)\r\n",data->ts_data->proxen_flag);
			} else {
				fts_sensors_pros_enable(data->ts_data->proxen_flag);
				mutex_unlock(&tp_mutex);
				if(data->ts_data->proxen_flag) {
					mdelay(150);
					fts_prox_get_data();
				}
			}
			break;
		default:
			retval = -EINVAL;
	}
	return retval;
}

static int prox_open(struct inode *inode, struct file *file)
{
	file->private_data = fts_proximity_datap;
	return 0;
}
static int prox_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

const struct file_operations fts_prox_fops = {
	.owner = THIS_MODULE,
	.read = prox_read,
	.poll = prox_poll,
	.unlocked_ioctl = prox_ioctl,
	.open = prox_open,
	.release = prox_release,
};
#endif

static void ft5x06_ts_release(void)
{
        struct ft5x06_ts_data *data = i2c_get_clientdata(this_client);
        int i;
        for (i = 0; i < data->info->max_points; i++)
        {
                input_mt_slot(data->input_dev, i);
                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
        }
        input_mt_report_pointer_emulation(data->input_dev, false);
        input_sync(data->input_dev);

}

static int ft5x06_read_data(void)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	int max_points = data->info->max_points ;
	u8 buf[CFG_POINT_READ_BUF] = {0};
	int ret = -1;
	int i = 0;

	ret = ft5x06_i2c_rxdata(buf, CFG_POINT_READ_BUF);
	if (ret < 0) {
		FT5X06_PRINT("read_data i2c_rxdata failed: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	/*get face detect information*/
	fts_proximity_datap->face_detect_enable = FT_FACE_DETECT_DISABLE;
	fts_proximity_datap->is_face_detect = FT_FACE_DETECT_OFF;
	fts_proximity_datap->prox_val = 0;
	ft5x06_read_reg(FT_FACE_DETECT_REG, &fts_proximity_datap->face_detect_enable);

	if (FT_FACE_DETECT_ENABLE == fts_proximity_datap->face_detect_enable) {
		fts_proximity_datap->is_face_detect = buf[FT_FACE_DETECT_POS] & 0xE0;
		fts_proximity_datap->prox_val = buf[FT_FACE_DETECT_POS] & 0x1F;
#if ft5x06_debug
		FT5X06_PRINT("reg:0x%x[0x%x],buf[%d]:0x%x\n",
		       FT_FACE_DETECT_REG,fts_proximity_datap->face_detect_enable,FT_FACE_DETECT_POS,buf[FT_FACE_DETECT_POS]);
#endif
	}
#endif
	event->touch_point = buf[2] & 0x0f;
#if ft5x06_debug
	FT5X06_PRINT("event->touch_point = %d\n",event->touch_point);
#endif

	if (event->touch_point > max_points) {
		event->touch_point = max_points;
	}

	for (i = 0; i < max_points; i++) {
		event->au16_x[i] = (s16)(buf[3 + 6*i] & 0x0F)<<8 | (s16)buf[4 + 6*i];
		event->au16_y[i] = (s16)(buf[5 + 6*i] & 0x0F)<<8 | (s16)buf[6 + 6*i];
		event->au8_touch_event[i] = buf[0x3 + 6*i] >> 6;
		event->au8_finger_id[i] = (buf[5 + 6*i])>>4;

                if (event->au8_finger_id[i] >= FT_MAX_ID )
                        break;

#if ft5x06_debug
		FT5X06_PRINT("event->au16_x[%d] = %d, event->au16_y[%d] = %d\n",
                                i,
                                event->au16_x[i],
                                i,
                                event->au16_y[i]);
		FT5X06_PRINT("au8_touch_event[%d] vaule= %d \n",
                                i,
                                event->au8_touch_event[i]);
#endif
	}
	return 0;
}

#if CFG_SUPPORT_TOUCH_KEY
int ft5x06_touch_key_process(struct input_dev *dev, int x, int y, int touch_event)
{
	int i;
	int key_id;

	if (x < 66 && x > 62) {
		key_id = 0;
	} else if (x < 241 && x > 237) {
		key_id = 1;
	} else if (x < 373 && x > 369) {
		key_id = 2;
	} else {
		key_id = 0xf;
	}

	for (i = 0; i < CFG_NUMOFKEYS; i++) {
		if ( key_id == i ) {
			if ((touch_event == FT_TOUCH_DOWN) || (touch_event == FT_TOUCH_CONTACT)) {
				input_report_key(dev, tsp_keycodes[i], 1);
#if ft5x06_debug
				FT5X06_PRINT("%s key is pressed. Keycode : %d\n",
                                                tsp_keyname[i],
                                                tsp_keycodes[i]);
#endif
				tsp_keystatus[i] = KEY_PRESS;
			} else if (tsp_keystatus[i]) {
				input_report_key(dev, tsp_keycodes[i], 0);
#if ft5x06_debug
				FT5X06_PRINT("%s key is release. Keycode : %d\n",
                                                tsp_keyname[i],
                                                tsp_keycodes[i]);
#endif
				tsp_keystatus[i] = KEY_RELEASE;
			}
		}
	}
	return 0;
}
#endif

static void ft5x06_report_value(void)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	int i = 0, j, max_points = data->info->max_points;
	int max_x = SCREEN_MAX_X,  max_virtual_y = SCREEN_VIRTUAL_Y;
        bool update_input = false;

#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	if (FT_FACE_DETECT_ENABLE == fts_proximity_datap->face_detect_enable) {
		if (FT_FACE_DETECT_ON == fts_proximity_datap->is_face_detect ||
		    FT_FACE_DETECT_OFF == fts_proximity_datap->is_face_detect)
			fts_prox_get_data();
	}
#endif
	if(data->suspend != 1) {
		if (data->info->max_scrx)
			max_x = data->info->max_scrx;
		if (data->info->virtual_scry)
			max_virtual_y = data->info->virtual_scry;

                /* check if any last report id had lost in the newly reported id array */
                while ((event->au8_last_id[i] < FT_MAX_ID) && (i < max_points)) {
                        j = 0;
                        while ((event->au8_finger_id[j] < FT_MAX_ID)  && (j < max_points)) {
                                if (event->au8_finger_id[j] == event->au8_last_id[i])
                                        break;
                                j++;
                        }
                        if ((event->au8_finger_id[j] >= FT_MAX_ID) && (event->au8_last_event[i] != FT_TOUCH_UP)) {
                                input_mt_slot(data->input_dev, event->au8_last_id[i]);
                                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
                                FT5X06_PRINT("lost Up point, id is %d\n", event->au8_last_id[i]);
                        }
                        i++;
                }

		for (i = 0; i < max_points; i++) {
                        event->au8_last_id[i] = event->au8_finger_id[i];
                        event->au8_last_event[i] = event->au8_touch_event[i];

                        if (event->au8_finger_id[i] >= FT_MAX_ID)
                                break;

                        update_input = true;
                        input_mt_slot(data->input_dev, event->au8_finger_id[i]);

                        if (event->au8_touch_event[i] == FT_TOUCH_DOWN || event->au8_touch_event[i] == FT_TOUCH_CONTACT) {
                                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 1);
                                input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
                                input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);
                        } else {
                                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
                        }
                }
                if (update_input) {
                        input_mt_report_pointer_emulation(data->input_dev, false);
                        input_sync(data->input_dev);
                }
                if (event->touch_point == 0)
                        ft5x06_ts_release();
        }
}

static void ft5206_irq_enable(struct ft5x06_ts_data *td)
{
	unsigned long irqflags;

	spin_lock_irqsave(&td->irq_lock, irqflags);
	if (td->irq_is_disable && !td->suspend) {
		enable_irq(td->irq);
		td->irq_is_disable = false;
	}
	spin_unlock_irqrestore(&td->irq_lock, irqflags);
}
static void ft5206_irq_disable(struct ft5x06_ts_data *td)
{
	unsigned long irqflags;

	spin_lock_irqsave(&td->irq_lock, irqflags);
	if (!td->irq_is_disable) {
		disable_irq_nosync(td->irq);
		td->irq_is_disable = true;
	}
	spin_unlock_irqrestore(&td->irq_lock, irqflags);
}

static void ft5x06_ts_pen_irq_work(struct work_struct *work)
{
	struct ft5x06_ts_data *ft5x06_ts
	        = container_of(work, struct ft5x06_ts_data, pen_event_work);
	int ret = -1;

	ret = ft5x06_read_data();
	if (ret == 0)
		ft5x06_report_value();

	ft5206_irq_enable(ft5x06_ts);
}

static irqreturn_t ft5x06_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x06_ts_data *ft5x06_ts = dev_id;

	ft5206_irq_disable(ft5x06_ts);

	if (!work_pending(&ft5x06_ts->pen_event_work))
		queue_work(ft5x06_ts->ts_workqueue, &ft5x06_ts->pen_event_work);
	else
		FT5X06_PRINT("work pending failed \n");

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x06_ts_suspend(struct early_suspend *handler)
{
	struct ft5x06_ts_data *ts
	        =  container_of(handler, struct ft5x06_ts_data, early_suspend);
	ts->suspend = 1;
#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	enable_irq_wake(ts->irq);
#endif
	mutex_lock(&tp_mutex);
#ifdef	CTP_CHARGER_DETECT
	ts->resume_end_flag = 0;
#endif
	memset(&(ts->event), 0, sizeof(struct ts_event));  // remove last_id/event
	ft5x06_ts_release();
	if(!ts->proxen_flag) {
		ts->tp_power_flag = 1;
		mutex_unlock(&tp_mutex);
		ft5206_irq_disable(ts);
		cancel_work_sync(&ts->pen_event_work);
		if (ts->info->power_ic_flag || ts->info->power_i2c_flag) {
			if(gpio_request(ts->info->irq_gpio, "ft5x06")) {
				FT5X06_PRINT("request GPIO error !!\n");
			}
			gpio_direction_input(ts->info->irq_gpio);
			gpio_free(ts->info->irq_gpio);
		}
		/* in hibernate mode (deep sleep). */
		if(ts->info->power_ic_flag)
			ts->info->power_ic(&this_client->dev, DISABLE);

		if(ts->info->power_i2c_flag)
			ts->info->power_i2c(&this_client->dev, DISABLE);

		mdelay(200);
	} else
		mutex_unlock(&tp_mutex);
	FT5X06_PRINT("early suspend\n");
}

static void ft5x06_ts_resume(struct early_suspend *handler)
{
	struct ft5x06_ts_data *ts
	        =  container_of(handler, struct ft5x06_ts_data, early_suspend);
	/* Get some register information. */
	ft5206_irq_enable(ts);
#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	disable_irq_wake(ts->irq);
#endif
#ifdef CTP_CHARGER_DETECT
	mutex_lock(&tp_mutex);
	if (ts->pre_charger_status == CHARGER_CONNECT) {
		ft5x06_write_reg(FT5X06_REG_MAX_FRAME, 1);
		FT5X06_PRINT("work on hopping mode after resume\n");
	}
	ts->resume_end_flag = 1;
	mutex_unlock(&tp_mutex);
#endif
	FT5X06_PRINT("late resume\n");
}


static void ft5x06_ts_very_late_resume(struct early_suspend *handler)
{
	struct ft5x06_ts_data *ts
	        =  container_of(handler, struct ft5x06_ts_data, very_early_suspend);

	struct irq_desc *desc = irq_to_desc(ts->irq);
	mutex_lock(&tp_mutex);
	if(ts->tp_power_flag) {
		if (ts->info->power_ic_flag || ts->info->power_i2c_flag) {
			if(gpio_request(ts->info->irq_gpio, "ft5x06")) {
				FT5X06_PRINT("request GPIO error !!\n");
			}
			gpio_direction_input(ts->info->irq_gpio);
			gpio_free(ts->info->irq_gpio);
		}
		if(ts->info->power_i2c_flag)
			ts->info->power_i2c(&this_client->dev, ENABLE);
		if(ts->info->power_ic_flag)
			ts->info->power_ic(&this_client->dev, ENABLE);
		mdelay(5);
		ts->tp_power_flag = 0;

	}
	mutex_unlock(&tp_mutex);
	//msleep(300);
	//ts->info->reset(&this_client->dev);
	ts->suspend = 0;
	desc->irq_data.chip->irq_ack(&desc->irq_data);
	FT5X06_PRINT("very late resume\n");
}

#ifdef CTP_CHARGER_DETECT
static void ft5x06_monitor_usb_work(struct work_struct *work)
{
        struct ft5x06_ts_data* ft5x06_ts =
                container_of( work, struct ft5x06_ts_data, usb_work);

        mutex_lock(&tp_mutex);
        if (ft5x06_ts->usb_event == LCUSB_EVENT_NONE)
                ft5x06_ts->is_charger_plug = CHARGER_NONE;
        else
                ft5x06_ts->is_charger_plug = CHARGER_CONNECT;

        if (ft5x06_ts->resume_end_flag) {  // only when TP was ready can we write FT5X06's regs
                if ((ft5x06_ts->is_charger_plug == CHARGER_CONNECT) &&
                    (ft5x06_ts->pre_charger_status == CHARGER_NONE)) {
                        ft5x06_write_reg(FT5X06_REG_MAX_FRAME, 1);
                        FT5X06_PRINT("work on frequency hopping mode\n");
                }
                if ((ft5x06_ts->is_charger_plug == CHARGER_NONE) &&
                    (ft5x06_ts->pre_charger_status == CHARGER_CONNECT)) {
                        ft5x06_write_reg(FT5X06_REG_MAX_FRAME, 0);
                        FT5X06_PRINT("work on normal frequency\n");
                }
        }
        ft5x06_ts->pre_charger_status = ft5x06_ts->is_charger_plug;
        mutex_unlock(&tp_mutex);

}
static int ft5x06_monitor_usb_notifier_call(struct notifier_block *nb,
                unsigned long event, void *data)
{
	struct ft5x06_ts_data* ft5x06_ts =
	        container_of(nb, struct ft5x06_ts_data, nb);

	ft5x06_ts->usb_event = event;
	schedule_work(&ft5x06_ts->usb_work);
	return NOTIFY_OK;
}
#endif
static void ft5x06_ts_very_early_suspend(struct early_suspend *handler)
{


	FT5X06_PRINT("very early suspend\n");
}
#endif
static int fts_get_fw_vendor_info(struct ft5x06_ts_data *data) {
        u8 ver, vid;

        ft5x06_read_reg(FT5X06_REG_FIRMID, &ver);
        ft5x06_read_reg(FT5X06_REG_VENDOR_ID, &vid);
        data->vid = vid;
        data->fw_ver = ver;

        if (ver && vid)
                return 0;
        else
                return -1;
}
static ssize_t fts_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        char *vendor_name;
        int ret;
	struct ft5x06_ts_data *data = i2c_get_clientdata(this_client);

        if ((!data->vid) || (!data->fw_ver)) {// in case of upgrade failed
                FT5X06_PRINT("cur FW ver or ID is incorrect, retry to get it\n");
                ret = fts_get_fw_vendor_info(data);
                if (ret)
                        return -1;      //read id failed
        }

        if (data->vid == VENDOR_BIEL)
                vendor_name = "BIEL";
        else if (data->vid == VENDOR_O_FILM)
                vendor_name = "O-film";
        else if (data->vid == VENDOR_GIS)
                vendor_name = "GIS";
        else if (data->vid == VENDOR_WINTEK)
                vendor_name = "WINTEK";
	else if (data->vid == VENDOR_YILI)
		vendor_name = "YILI";
        else
                vendor_name = "DEFAULT";

	/*DRIVER IC , FW version*/
        return snprintf(buf, PAGE_SIZE, "TP:%s,%s,FW 0x%x\n", data->fts_ic, vendor_name, data->fw_ver);
}

static DEVICE_ATTR(fts_info, S_IRUGO, fts_show, NULL);

static struct attribute *fts_attributes[] = {
	&dev_attr_fts_info.attr,
};

static int
ft5x06_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x06_ts_data *ft5x06_ts;
	struct input_dev *input_dev;
	struct ft5x06_info *info;
	unsigned char val;
	int err = 0;
	u8 chip_id;
	int max_x = SCREEN_MAX_X, max_y = SCREEN_MAX_Y;
	int max_points = 10;
	int i, ret;

	info = client->dev.platform_data;
	if (info == NULL) {
		dev_dbg(&client->dev, "No platform data\n");
		return -EINVAL;
	}

	this_client = client;
	FT5X06_PRINT("ft5x06_ts_probe, driver version is %s.\n", CFG_FTS_CTP_DRIVER_VERSION);

	if (info->power_ic_flag)
		info->power_ic(&this_client->dev, ENABLE);
	msleep(20);
	//	msleep(300);	/*after 200ms, INT can be respond */
	//info->reset(&this_client->dev);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x06_ts = kzalloc(sizeof(*ft5x06_ts), GFP_KERNEL);
	if (!ft5x06_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	if(info->max_scrx)
		max_x = info->max_scrx;
	if(info->max_scry)
		max_y = info->max_scry;
	if(info->max_points)
		max_points = info->max_points;

	ft5x06_ts->info = info;
	ft5x06_ts->suspend = 0;
	ft5x06_ts->irq = gpio_to_irq(info->irq_gpio);
#ifdef CTP_CHARGER_DETECT
	ft5x06_ts->pre_charger_status = CHARGER_NONE;
	ft5x06_ts->resume_end_flag = 0;
#endif
	i2c_set_clientdata(client, ft5x06_ts);
	spin_lock_init(&ft5x06_ts->irq_lock);

	INIT_WORK(&ft5x06_ts->pen_event_work, ft5x06_ts_pen_irq_work);

	ft5x06_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x06_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}

	/* Request GPIO and register IRQ. */
	if(gpio_request(info->irq_gpio, "ft5x06")) {
		FT5X06_PRINT("request GPIO error !!\n");
		goto exit_irq_request_failed;
	}

	gpio_direction_input(info->irq_gpio);
	err = request_irq(ft5x06_ts->irq,
	                  ft5x06_ts_interrupt,
	                  IRQF_TRIGGER_FALLING,
	                  "ft5x06_ts",
	                  ft5x06_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x06_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}

	disable_irq_nosync(ft5x06_ts->irq);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	ft5x06_ts->input_dev = input_dev;

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

        input_mt_init_slots(input_dev, max_points, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(input_dev,  ABS_MT_POSITION_Y, 0, max_y, 0, 0);

#if CFG_SUPPORT_TOUCH_KEY
	/* Setup key code area. */
	set_bit(EV_SYN, input_dev->evbit);
	input_dev->keycode = tsp_keycodes;
	for (i = 0; i < CFG_NUMOFKEYS; i++) {
		input_set_capability(input_dev, EV_KEY,
		                     ((int*)input_dev->keycode)[i]);
		tsp_keystatus[i] = KEY_RELEASE;
	}
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	//res prepare
	fts_proximity_datap = kmalloc(sizeof(struct fts_proximity_data), GFP_KERNEL);
	if (!fts_proximity_datap) {
		FT5X06_PRINT("Cannot get memory!\n");
		return -ENOMEM;
	}
	memset(fts_proximity_datap,0,sizeof(struct fts_proximity_data));

	init_waitqueue_head(&fts_proximity_datap->proximity_event_wait);
	//prox misc register
	fts_proximity_datap->prox_dev.minor = MISC_DYNAMIC_MINOR;
	fts_proximity_datap->prox_dev.name = "proximity";
	fts_proximity_datap->prox_dev.fops = &fts_prox_fops;
	fts_proximity_datap->ts_data = ft5x06_ts;
	err = misc_register(&(fts_proximity_datap->prox_dev));
	if (err) {
		FT5X06_PRINT("Register misc device for light sensor failed(%d)!\n", err);
		goto exit_register_prox_failed;
	}
#endif
	input_dev->name	= FT5X06_NAME;	//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		        "ft5x06_ts_probe: failed to register input device: %s\n",
		        dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ft5x06_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ft5x06_ts->early_suspend.suspend = ft5x06_ts_suspend;
	ft5x06_ts->early_suspend.resume	= ft5x06_ts_resume;
	register_early_suspend(&ft5x06_ts->early_suspend);

	ft5x06_ts->very_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ft5x06_ts->very_early_suspend.suspend = ft5x06_ts_very_early_suspend;
	ft5x06_ts->very_early_suspend.resume = ft5x06_ts_very_late_resume;
	register_early_suspend(&ft5x06_ts->very_early_suspend);
#endif

#ifdef CTP_CHARGER_DETECT
        INIT_WORK(&ft5x06_ts->usb_work, ft5x06_monitor_usb_work);
        ft5x06_ts->nb.notifier_call = ft5x06_monitor_usb_notifier_call;
        val = comip_usb_register_notifier(&ft5x06_ts->nb);
        if (val)
                FT5X06_PRINT("register usb notifier failed %d\n", val);
#endif

	/* Make sure CTP already finish startup process. */
	msleep(200);

        for (i=0; i < 3; i++) {
                ret = ft5x06_read_reg(FT5X06_REG_CIPHER, &chip_id);
                if (ret > 0 && chip_id > 0) {
                        break;
                } else {  // redo power off & power on if read failed
			if (info->power_ic_flag) {
				info->power_ic(&this_client->dev, DISABLE);
				info->power_ic(&this_client->dev, ENABLE);
				msleep(200);
			}
	                FT5X06_PRINT("power on reset failed, reset again\n");
		}
        }
        FT5X06_PRINT("chip_id = %x\n", chip_id);

        if (chip_id == 0) {  // default for ft5336
                memcpy(&fts_updateinfo_curr, &fts_updateinfo[6], sizeof(struct Upgrade_Info));
        } else {
                for (i=0; i<sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info); i++) {
                        if (chip_id==fts_updateinfo[i].CHIP_ID) {
                                memcpy(&fts_updateinfo_curr, &fts_updateinfo[i], sizeof(struct Upgrade_Info));
                                sprintf(ft5x06_ts->fts_ic, "%s", fts_updateinfo_curr.FTS_NAME);
                                break;
                        }
                }
                if (i >= sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info)) {
                        memcpy(&fts_updateinfo_curr, &fts_updateinfo[0], sizeof(struct Upgrade_Info));
                        sprintf(ft5x06_ts->fts_ic, "%s", fts_updateinfo_curr.FTS_NAME);
                }
        }

#if ft5x06_debug
	ft5x06_read_reg(FT5X06_REG_PERIODACTIVE, &val);
	FT5X06_PRINT("report rate is %dHz.\n", val * 10);
#endif

#if ft5x06_debug
	ft5x06_read_reg(FT5X06_REG_THGROUP, &val);
	FT5X06_PRINT("touch threshold is %d.\n", val * 4);
#endif

#if CFG_SUPPORT_AUTO_UPG
	fts_ctpm_auto_upgrade(client);
#endif

#if CFG_SUPPORT_UPDATE_PROJECT_SETTING
	fts_ctpm_update_project_setting(client);
#endif

#ifdef SYSFS_DEBUG
	ft5x0x_create_sysfs(client);
#endif

#ifdef FTS_APK_DEBUG
	ft5x0x_create_apk_debug_channel(client);
#endif

#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(client) < 0)
		dev_err(&client->dev, "create fts control iic driver failed\n");
#endif

	/*export tp devinfo to sys/devinfo, used by fast amt3*/
	comip_devinfo_register(fts_attributes, ARRAY_SIZE(fts_attributes));

	/* Get tp's firmware and vendor id information. */
        ret = fts_get_fw_vendor_info(ft5x06_ts);
        if (ret == 0)
	        FT5X06_PRINT("Firmware version is 0x%x, Vendor id is 0x%x\n", ft5x06_ts->fw_ver, ft5x06_ts->vid);
        else
                FT5X06_PRINT("Get FW ver or ID failed\n");

	enable_irq(ft5x06_ts->irq);

#if ft5x06_debug
	FT5X06_PRINT("probe over\n");
#endif

	gpio_free(info->irq_gpio);
	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq(ft5x06_ts->irq, ft5x06_ts);
exit_irq_request_failed:
#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	misc_deregister(&fts_proximity_datap->prox_dev);
	kfree(fts_proximity_datap);
exit_register_prox_failed:
#endif
	cancel_work_sync(&ft5x06_ts->pen_event_work);
	destroy_workqueue(ft5x06_ts->ts_workqueue);
exit_create_singlethread:
	FT5X06_PRINT("singlethread error\n");
	i2c_set_clientdata(client, NULL);
	kfree(ft5x06_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

static int __exit ft5x06_ts_remove(struct i2c_client *client)
{
	struct ft5x06_ts_data *ft5x06_ts;

	FT5X06_PRINT("ft5x06_ts_remove\n");
	ft5x06_ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ft5x06_ts->early_suspend);
	unregister_early_suspend(&ft5x06_ts->very_early_suspend);
	free_irq(ft5x06_ts->irq, ft5x06_ts);
	input_unregister_device(ft5x06_ts->input_dev);

#ifdef FTS_APK_DEBUG
	ft5x0x_release_apk_debug_channel();
#endif

#ifdef SYSFS_DEBUG
	ft5x0x_release_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	misc_deregister(&fts_proximity_datap->prox_dev);
	kfree(fts_proximity_datap);
	fts_proximity_datap = NULL;
#endif
	cancel_work_sync(&ft5x06_ts->pen_event_work);
	destroy_workqueue(ft5x06_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	kfree(ft5x06_ts);

	return 0;
}

static void ft5x06_ts_shutdown(struct i2c_client *client)
{
	struct ft5x06_ts_data *ft5x06_ts;

	FT5X06_PRINT("ft5x06 shutdown\n");
	ft5x06_ts = i2c_get_clientdata(client);

	if (ft5x06_ts->info != NULL) {
		if(ft5x06_ts->info->power_ic_flag)
			ft5x06_ts->info->power_ic(&this_client->dev, DISABLE);

		if(ft5x06_ts->info->power_i2c_flag)
			ft5x06_ts->info->power_i2c(&this_client->dev, DISABLE);
	}

	unregister_early_suspend(&ft5x06_ts->early_suspend);
	unregister_early_suspend(&ft5x06_ts->very_early_suspend);
	free_irq(ft5x06_ts->irq, ft5x06_ts);
	input_unregister_device(ft5x06_ts->input_dev);

#ifdef FTS_APK_DEBUG
	ft5x0x_release_apk_debug_channel();
#endif

#ifdef SYSFS_DEBUG
	ft5x0x_release_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
	misc_deregister(&fts_proximity_datap->prox_dev);
	kfree(fts_proximity_datap);
	fts_proximity_datap = NULL;
#endif
	cancel_work_sync(&ft5x06_ts->pen_event_work);
	destroy_workqueue(ft5x06_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	kfree(ft5x06_ts);
}

static const struct i2c_device_id ft5x06_ts_id[] = {
	{FT5X06_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ft5x06_ts_id);

static struct i2c_driver ft5x06_ts_driver = {
	.probe		= ft5x06_ts_probe,
	.remove		= __exit_p(ft5x06_ts_remove),
	.shutdown	= ft5x06_ts_shutdown,
	.id_table	= ft5x06_ts_id,
	.driver	= {
		.name	= FT5X06_NAME,
	},
};

static int __init ft5x06_ts_init(void)
{
	return i2c_add_driver(&ft5x06_ts_driver);
}

static void __exit ft5x06_ts_exit(void)
{
	i2c_del_driver(&ft5x06_ts_driver);
}

module_init(ft5x06_ts_init);
module_exit(ft5x06_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x06 TouchScreen driver");
MODULE_LICENSE("GPL");
