/*
 *  apds990x.c - Linux kernel modules for ambient light + proximity sensor
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
#include <linux/i2c/apds990x.h>

#include <plat/lightprox.h>

#define APDS990x_PS_DETECTION_THRESHOLD		600
#define APDS990x_PS_HSYTERESIS_THRESHOLD	500

#define APDS990x_ALS_THRESHOLD_HSYTERESIS	20	/* 20 = 20% */

#define APDS990x_ENABLE_REG	0x00
#define APDS990x_ATIME_REG		0x01
#define APDS990x_PTIME_REG		0x02
#define APDS990x_WTIME_REG	0x03
#define APDS990x_AILTL_REG		0x04
#define APDS990x_AILTH_REG		0x05
#define APDS990x_AIHTL_REG		0x06
#define APDS990x_AIHTH_REG		0x07
#define APDS990x_PILTL_REG		0x08
#define APDS990x_PILTH_REG		0x09
#define APDS990x_PIHTL_REG		0x0A
#define APDS990x_PIHTH_REG		0x0B
#define APDS990x_PERS_REG		0x0C
#define APDS990x_CONFIG_REG	0x0D
#define APDS990x_PPCOUNT_REG	0x0E
#define APDS990x_CONTROL_REG	0x0F
#define APDS990x_REV_REG		0x11
#define APDS990x_ID_REG		0x12
#define APDS990x_STATUS_REG	0x13
#define APDS990x_CDATAL_REG	0x14
#define APDS990x_CDATAH_REG	0x15
#define APDS990x_IRDATAL_REG	0x16
#define APDS990x_IRDATAH_REG	0x17
#define APDS990x_PDATAL_REG	0x18
#define APDS990x_PDATAH_REG	0x19

#define CMD_BYTE	0x80
#define CMD_WORD	0xA0
#define CMD_SPECIAL	0xE0

#define CMD_CLR_PS_INT	0xE5
#define CMD_CLR_ALS_INT	0xE6
#define CMD_CLR_PS_ALS_INT	0xE7

// Triton cntrl reg masks
#define APDS990X_CNTRL               	0x00
#define APDS990X_GAIN                		0x0F
#define APDS990X_CNTL_PROX_INT_ENBL  0X20
#define APDS990X_CNTL_ALS_INT_ENBL   0X10
#define APDS990X_CNTL_WAIT_TMR_ENBL  0X08
#define APDS990X_CNTL_PROX_DET_ENBL  0X04
#define APDS990X_CNTL_ADC_ENBL       0x02
#define APDS990X_CNTL_PWRON          0x01
#define APDS990X_INTERRUPT           0x0C
#define APDS990X_ALS_TIME            0X01
#define APDS990X_CMD_SPL_FN          0x60
#define APDS990X_CMD_PROX_INTCLR     0X05
#define APDS990X_CMD_ALS_INTCLR      0X06
#define APDS990X_CMD_WORD_BLK_RW     0x20
#define APDS990X_ALS_CHAN0LO         0x14

static int debug = 1;
#define dprintk(level, fmt, arg...) do {                        \
        if (debug >= level)                                     \
        printk(KERN_WARNING fmt , ## arg); } while (0)

#define apds990xprintk(format, ...) dprintk(1, format, ## __VA_ARGS__)
struct apds990x_data {
	struct apds990x_platform_data	*apds_data;
	struct i2c_client *client;
	struct mutex update_lock;
	struct work_struct	dwork;	/* for PS interrupt */
	struct delayed_work    als_dwork; /* for ALS polling */

	int light_event;
	int proximity_event;
	wait_queue_head_t light_event_wait;   
	wait_queue_head_t proximity_event_wait;  
	unsigned int gluxValue;  /* light lux data*/

	unsigned int irq; 
	unsigned int irq_enable;
	
	unsigned int enable;
	unsigned int atime;
	unsigned int ptime;
	unsigned int wtime;
	unsigned int ailt;
	unsigned int aiht;
	unsigned int pilt;
	unsigned int piht;
	unsigned int pers;
	unsigned int config;
	unsigned int ppcount;
	unsigned int control;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;
	unsigned int enable_als_sensor;

	/* PS parameters */
	unsigned int ps_threshold;
	unsigned int ps_hysteresis_threshold; /* always lower than ps_threshold */
	unsigned int ps_detection;		/* 0 = near-to-far; 1 = far-to-near */
	unsigned int ps_data;			/* to store PS data */
	unsigned int ps_poll_delay;

	/* ALS parameters */
	unsigned int als_threshold_l;	/* low threshold */
	unsigned int als_threshold_h;	/* high threshold */
	unsigned int als_data;			/* to store ALS data */

	unsigned int als_gain;			/* needed for Lux calculation */
	unsigned int als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int als_atime;			/* storage for als integratiion time */
};

/*
 * Global data
 */
static struct apds990x_data *apds990x_gdata;
#define apds990x_irq_enable() \
				if(!apds990x_gdata->irq_enable){ \
							enable_irq(apds990x_gdata->irq); \
							apds990x_gdata->irq_enable = 1; \
				}	

#define apds990x_irq_disalbe() \
				if(apds990x_gdata->irq_enable){ \
							disable_irq_nosync(apds990x_gdata->irq); \
							apds990x_gdata->irq_enable = 0; \
				}

/*
 *	
 *I2C function define
 */
static int apds990x_i2c_write_byte(struct apds990x_data *data, u8 addr, u8 value)
{
	if(i2c_smbus_write_byte_data(data->client, addr, value) <  0){
		apds990xprintk("apds990x_i2c_write_byte error \n");
		return -1;
	}
	return 0;
} 

static int apds990x_i2c_read_byte(struct apds990x_data *data, u8 addr, u8 *buf)
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
	
	if(i2c_transfer(data->client->adapter, msg, 2) < 0){
		apds990xprintk("apds990x_i2c_read_byte error\n");
		return -1;
	}
	
	return 0;
}

static int apds990x_i2c_write_cmd(struct apds990x_data *data, u8 command)
{
	struct i2c_msg msg[1];
	msg[0].addr = data->client->addr;
	msg[0].flags= 0;
	msg[0].buf  = &command;
	msg[0].len  = 1;
	if(i2c_transfer(data->client->adapter, msg, 1) < 0){
		apds990xprintk("apds990x_i2c_write_cmd error\n");
		return -1;
	}
	
	return 0;
} 
/*
 * Management functions
 */

static int apds990x_set_enable(struct i2c_client *client, int enable)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_ENABLE_REG, enable);

	data->enable = enable;

	return ret;
}

static int apds990x_set_atime(struct i2c_client *client, int atime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_ATIME_REG, atime);

	data->atime = atime;

	return ret;
}

static int apds990x_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_PTIME_REG, ptime);

	data->ptime = ptime;

	return ret;
}

static int apds990x_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_WTIME_REG, wtime);

	data->wtime = wtime;

	return ret;
}

static int apds990x_set_ailt(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AILTL_REG, threshold);
	
	data->ailt = threshold;

	return ret;
}

static int apds990x_set_aiht(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AIHTL_REG, threshold);
	
	data->aiht = threshold;

	return ret;
}

static int apds990x_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG, threshold);
	
	data->pilt = threshold;

	return ret;
}

static int apds990x_set_piht(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG, threshold);
	
	data->piht = threshold;

	return ret;
}

static int apds990x_set_pers(struct i2c_client *client, int pers)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_PERS_REG, pers);

	data->pers = pers;

	return ret;
}

static int apds990x_set_config(struct i2c_client *client, int config)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_CONFIG_REG, config);

	data->config = config;

	return ret;
}

static int apds990x_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_PPCOUNT_REG, ppcount);

	data->ppcount = ppcount;

	return ret;
}

static int apds990x_set_control(struct i2c_client *client, int control)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_CONTROL_REG, control);

	data->control = control;

	/* obtain ALS gain value */
	if ((data->control&0x03) == 0x00) /* 1X Gain */
		data->als_gain = 1;
	else if ((data->control&0x03) == 0x01) /* 8X Gain */
		data->als_gain = 8;
	else if ((data->control&0x03) == 0x02) /* 16X Gain */
		data->als_gain = 16;
	else  /* 120X Gain */
		data->als_gain = 120;

	return ret;
}

static int LuxCalculation(struct i2c_client *client, int cdata, int irdata)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int luxValue = 0;

	int IAC1 = 0;
	int IAC2 = 0;
	int IAC = 0;
	int GA = 48;			/* 0.48 without glass window */
	int COE_B = 223;		/* 2.23 without glass window */
	int COE_C = 70;		/* 0.70 without glass window */
	int COE_D = 142;		/* 1.42 without glass window */
	int DF = 52;

	IAC1 = (cdata - (COE_B * irdata) / 100);	// re-adjust COE_B to avoid 2 decimal point
	IAC2 = ((COE_C * cdata) / 100 - (COE_D * irdata) / 100); // re-adjust COE_C and COE_D to void 2 decimal point

	if (IAC1 > IAC2)
		IAC = IAC1;
	else if (IAC1 <= IAC2)
		IAC = IAC2;
	else
		IAC = 0;
	luxValue = ((IAC * GA * DF) / 100) / (((272 * (256 - data->atime)) / 100) * data->als_gain);

	return luxValue;
}

static void apds990x_change_ps_threshold(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);

	data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_PDATAL_REG);

	if ( (data->ps_data > data->pilt) && (data->ps_data >= data->piht) ) {
		/* far-to-near detected */
		data->ps_detection = 1;
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG, data->ps_hysteresis_threshold);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG, 1023);
		data->pilt = data->ps_hysteresis_threshold;
		data->piht = 1023;
	}
	else if ( (data->ps_data <= data->pilt) && (data->ps_data < data->piht) ) {
		/* near-to-far detected */
		data->ps_detection = 0;
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG, data->ps_threshold);
		data->pilt = 0;
		data->piht = data->ps_threshold;
	}
	data->proximity_event = 1;
	wake_up_interruptible(&data->proximity_event_wait);
}

static int apds990x_interrupts_clear(void)
{
	return apds990x_i2c_write_cmd(apds990x_gdata,
		CMD_BYTE | APDS990X_CMD_SPL_FN | 0x07);
}

static void apds990x_als_get_data(struct i2c_client *client)
{
        struct apds990x_data *data = i2c_get_clientdata(client);
        int cdata, irdata;
        int luxValue=0;

        cdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_CDATAL_REG);
        irdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_IRDATAL_REG);
        luxValue = LuxCalculation(client, cdata, irdata);
        luxValue = luxValue>0 ? luxValue : 0;
        luxValue = luxValue<10000 ? luxValue : 10000;
        data->als_data = cdata;
        data->als_threshold_l = (data->als_data * (100 - APDS990x_ALS_THRESHOLD_HSYTERESIS) ) /100;
        data->als_threshold_h = (data->als_data * (100 + APDS990x_ALS_THRESHOLD_HSYTERESIS) ) /100;
        data->gluxValue = luxValue;
        data->light_event = 1;
        wake_up_interruptible(&data->light_event_wait);
}

static void apds990x_als_threshold_set(struct apds990x_data *data)
{
	int i;
	static char als_buf[4];
	unsigned char chdata[2];
	unsigned short ch0;
	unsigned short     als_threshold_hi;
	unsigned short     als_threshold_lo;
	
	for (i = 0; i < 2; i++) {
		apds990x_i2c_read_byte(data, CMD_BYTE |
			APDS990X_CMD_WORD_BLK_RW | (APDS990X_ALS_CHAN0LO + i), &chdata[i]);
	}
	ch0 = chdata[0] + chdata[1]*256;
	als_threshold_hi = (12 * ch0) / 10;
	
	if (als_threshold_hi >= 65535)
		als_threshold_hi = 65535;
	
	als_threshold_lo = (8 * ch0) / 10;
	als_buf[0] = als_threshold_lo & 0x0ff;
	als_buf[1] = als_threshold_lo >> 8;
	als_buf[2] = als_threshold_hi & 0x0ff;
	als_buf[3] = als_threshold_hi >> 8;

	for (i = 0; i < 4; i++) 
		apds990x_i2c_write_byte(data, (CMD_BYTE | 0x04) + i, als_buf[i]);	
}

static int apds990x_enable_als_sensor(struct apds990x_data *data) 
{
	apds990x_i2c_write_cmd(data, CMD_BYTE | APDS990X_CMD_SPL_FN | APDS990X_CMD_ALS_INTCLR);
	apds990x_i2c_write_byte(data, CMD_BYTE | APDS990x_ATIME_REG,  data->apds_data->als_time);
	apds990x_i2c_write_byte(data, CMD_BYTE | APDS990X_INTERRUPT, 0x11);
	apds990x_i2c_write_byte(data, CMD_BYTE | APDS990x_CONTROL_REG, data->apds_data->gain);
	apds990x_i2c_write_byte(data, CMD_BYTE | APDS990X_CNTRL,  APDS990X_CNTL_ADC_ENBL |
													APDS990X_CNTL_PWRON |
													APDS990X_CNTL_ALS_INT_ENBL);
	apds990x_als_threshold_set(data);
	return 0;
}

static void apds990x_get_data(struct apds990x_data *data )
{
		unsigned char status;
		unsigned int ret;
		
		ret = apds990x_i2c_read_byte(data, CMD_BYTE| APDS990x_STATUS_REG, &status);	
		
#define PS_ALS_INT 0x30
#define PS_INT	0x20
#define ALS_INT	0x10
		switch(status & 0x30){
			case PS_ALS_INT:
				/*PS and ALS int*/
				apds990xprintk("APDS990X INT is not support now\n");
				break;
			case PS_INT:
				/*PS int*/
				apds990x_change_ps_threshold(data->client);
				break;
			case ALS_INT:
				/*ALS int*/
				apds990x_als_threshold_set(data);
				apds990x_als_get_data(data->client);
				break;
			default:
				apds990xprintk("APDS990X wrong int type, int status is 0x%x", status);
		};
}

static void apds990x_work_handler(struct work_struct *work)
{
	struct apds990x_data *data = container_of(work, struct apds990x_data, dwork);
	apds990x_get_data(data);
	apds990x_interrupts_clear();
}

static irqreturn_t apds990x_interrupt(int irq, void *info)
{
	struct i2c_client *client=(struct i2c_client *)info;
	struct apds990x_data *data = i2c_get_clientdata(client);
	
	apds990x_irq_disalbe();
	schedule_work(&data->dwork);
	apds990x_irq_enable();
	
	return IRQ_HANDLED;
}
static int  apds990x_enable_ps_sensor(struct apds990x_data *data, unsigned int val)
{
	struct i2c_client *client = data->client;
		
	if ((val != 0) && (val != 1)) {
		apds990xprintk("%s:store unvalid value=%x\n", __func__, val);
		return 0;
	}
	
	if(val) {
		if (data->enable_ps_sensor == 0) {
			data->enable_ps_sensor= 1;
			apds990x_set_enable(client,0); /* Power Off */
			apds990x_set_atime(client, 0xf6); /* 27.2ms */
			apds990x_set_ptime(client, 0xff); /* 2.72ms */
		
			apds990x_set_ppcount(client, 8); /* 8-pulse */
			apds990x_set_control(client, 0x20); /* 100mA, IR-diode, 1X PGAIN, 1X AGAIN */
		
			apds990x_set_pilt(client, 0);		// init threshold for proximity
			apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);

			data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
			data->ps_hysteresis_threshold = APDS990x_PS_HSYTERESIS_THRESHOLD;
		
			apds990x_set_ailt( client, 0);
			apds990x_set_aiht( client, 0xffff);
			apds990x_set_pers(client, 0x33); /* 3 persistence */
			apds990x_set_enable(client, 0x27);	 /* only enable PS interrupt */
			apds990x_irq_enable();
		}
	} else {
		data->enable_ps_sensor = 0;
		if (data->enable_als_sensor) {
			apds990x_irq_disalbe();
			apds990x_enable_als_sensor(data);
			apds990x_irq_enable();
		}else {
			apds990x_irq_disalbe();
			apds990x_set_enable(client, 0);
		}
	}
	return 0;
}

/*misc light device  file opreation*/
static int light_open(struct inode *inode, struct file *file)
{
	apds990xprintk("enter %s\n",__FUNCTION__);
	file->private_data = apds990x_gdata;
	return 0;
}

/* close function - called when the "file" /dev/light is closed in userspace */
static int light_release(struct inode *inode, struct file *file)
{
	apds990xprintk("enter %s\n",__FUNCTION__);
	apds990x_irq_disalbe();
	return 0;
}

/* read function called when from /dev/light is read */
static ssize_t light_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct apds990x_data *data = file->private_data;

	if (!data->light_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->light_event_wait,data->light_event);
	}

	if (!data->light_event || !buf)
		return 0;
	
	len = sizeof(data->gluxValue);
	err = copy_to_user(buf, &data->gluxValue, len);
	if (err) {
		apds990xprintk("%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}
	
	data->light_event = 0;
	return len;
}

unsigned int light_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct apds990x_data * p_apds990x_data = file->private_data;
	
	poll_wait(file, &p_apds990x_data->light_event_wait, poll);
	if (p_apds990x_data->light_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

/* ioctl - I/O control */
static long light_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned char reg_val;
	unsigned int flag = 0;
	
	struct apds990x_data * p_apds990x_data = file->private_data;

	switch (cmd) {
		case LIGHT_SET_DELAY:
			if (arg > LIGHT_MAX_DELAY)
				arg = LIGHT_MAX_DELAY;
			else if (arg < LIGHT_MIN_DELAY)
				arg = LIGHT_MIN_DELAY;
			 p_apds990x_data->als_poll_delay = arg;
			break;
		case LIGHT_SET_ENABLE:
			flag = arg ? 1 : 0;
			if(!p_apds990x_data->enable_ps_sensor){
				if(flag){
					p_apds990x_data->enable_als_sensor = 1;
					apds990x_enable_als_sensor(p_apds990x_data);
					apds990x_irq_enable();
				}else{
					apds990x_irq_disalbe();
					i2c_smbus_write_byte(p_apds990x_data->client, (CMD_BYTE | APDS990x_ENABLE_REG));
					reg_val = i2c_smbus_read_byte(p_apds990x_data->client);
					if ((reg_val & APDS990X_CNTL_PROX_DET_ENBL) == 0x0) {
						i2c_smbus_write_byte_data(p_apds990x_data->client, (CMD_BYTE | APDS990x_ENABLE_REG), 0x00);
					}
					p_apds990x_data->enable_als_sensor = 0;
				}
			}
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
	apds990xprintk("enter %s\n",__FUNCTION__);
	file->private_data = apds990x_gdata;
	return 0;
}

/* close function - called when the "file" /dev/lightprox is closed in userspace */
static int prox_release(struct inode *inode, struct file *file)
{
	apds990xprintk("enter %s\n",__FUNCTION__);
	apds990x_irq_disalbe();
	return 0;
}

/* read function called when from /dev/prox is read */
static ssize_t prox_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{
	int len, err;
	struct apds990x_data *data = file->private_data;

	if (!data->proximity_event && (!(file->f_flags & O_NONBLOCK))) {
		wait_event_interruptible(data->proximity_event_wait,data->light_event);
	}

	if (!data->proximity_event || !buf)
		return 0;
	
	len = sizeof(data->ps_detection);
	err = copy_to_user(buf, &data->ps_detection, len);
	if (err) {
		apds990xprintk("%s Copy to user returned %d\n" ,__FUNCTION__,err);
		return -EFAULT;
	}
	
	data->proximity_event = 0;
	return len;

}

unsigned int prox_poll(struct file *file, struct poll_table_struct *poll)
{
	int mask = 0;
	struct apds990x_data * p_apds990x_data = file->private_data;

	poll_wait(file, &p_apds990x_data->proximity_event_wait, poll);
	if (p_apds990x_data->proximity_event)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

/* ioctl - I/O control */
static long prox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned int flag = 0;
	
	struct apds990x_data * p_apds990x_data = file->private_data;

	switch (cmd) {
		case PROXIMITY_SET_DELAY:
			if (arg > PROXIMITY_MAX_DELAY)
				arg = PROXIMITY_MAX_DELAY;
			else if (arg < PROXIMITY_MIN_DELAY)
				arg = PROXIMITY_MIN_DELAY;
			 p_apds990x_data->ps_poll_delay= arg;
			break;
		case PROXIMITY_SET_ENABLE:
			flag = arg ? 1 : 0;
			apds990x_enable_ps_sensor(p_apds990x_data, flag); 	
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
static int apds990x_init_client(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int err;
	int id;

	err = apds990x_set_enable(client, 0);

	if (err < 0)
		return err;
	
	id = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS990x_ID_REG);
	if (id == 0x20) {
		apds990xprintk("APDS-9901\n");
	}else if (id == 0x29) {
		apds990xprintk("APDS-990x\n");
	}else {
		apds990xprintk("Neither APDS-9901 nor APDS-9901\n");
		return -EIO;
	}

	apds990x_set_atime(client, 0xDB);	// 100.64ms ALS integration time
	apds990x_set_ptime(client, 0xFF);	// 2.72ms Prox integration time
	apds990x_set_wtime(client, 0xFF);	// 2.72ms Wait time

	apds990x_set_ppcount(client, 0x08);	// 8-Pulse for proximity
	apds990x_set_config(client, 0);		// no long wait
	apds990x_set_control(client, 0x20);	// 100mA, IR-diode, 1X PGAIN, 1X AGAIN

	apds990x_set_pilt(client, 0);		// init threshold for proximity
	apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);

	data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
	data->ps_hysteresis_threshold = APDS990x_PS_HSYTERESIS_THRESHOLD;

	apds990x_set_ailt(client, 0xFFFF);		// force first ALS interrupt
	apds990x_set_aiht(client, 0);

	apds990x_set_pers(client, 0x33);	// 2 consecutive Interrupt persistence

	return 0;
}

#define APDS990x_DRV_NAME	"apds990x"

/*
 * I2C init/probing/exit functions
 */
static int __devinit apds990x_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds990x_data *data;
	int err = 0;

	apds990xprintk("enter %s\n",__FUNCTION__);
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct apds990x_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	data->apds_data = client->dev.platform_data;
	data->client = client;
	i2c_set_clientdata(client, data);

	data->enable = 0;	/* default mode is standard */
	data->ps_threshold = 0;
	data->ps_hysteresis_threshold = 0;
	data->ps_detection = 0;	/* default to no detection */
	data->enable_als_sensor = 0;	// default to 0
	data->enable_ps_sensor = 0;	// default to 0
	data->als_poll_delay = 1000;	// default to 1000ms
	data->als_atime	= 0xdb;			// work in conjuction with als_poll_delay
	client->irq = data->apds_data->int_gpio;//MFP_PIN_GPIO22;
	apds990x_gdata = data;

	mutex_init(&data->update_lock);
	INIT_WORK(&data->dwork, apds990x_work_handler);
	init_waitqueue_head(&data->light_event_wait);
	init_waitqueue_head(&data->proximity_event_wait);
	/* Initialize the APDS990x chip */
	err = apds990x_init_client(client);
	if (err)
		goto exit_kfree;

	err = gpio_request(client->irq, "apds irq");
	if (err < 0){
		apds990xprintk("%s:Gpio request failed!\n", __func__);
		goto exit_kfree;
	}
	
	gpio_direction_input(client->irq);
	data->irq = gpio_to_irq(client->irq);	
	
	err = request_irq(data->irq, apds990x_interrupt,
				IRQ_TYPE_EDGE_FALLING, "apds_irq", (void *)client);
	if (err != 0) {
		apds990xprintk("%s:IRQ request failed!\n", __func__);
		goto exit_kfree;
	}

	disable_irq_nosync(data->irq);
	data->irq_enable = 0;

	if (misc_register(&lightdevice)) {
		apds990xprintk("%s : misc light device register falied!\n",__FUNCTION__);
	} 
	
	if (misc_register(&proxdevice)) {
		apds990xprintk("%s : misc proximity device register falied!\n",__FUNCTION__);
	} 
	apds990xprintk("%s support ver.  enabled\n",__FUNCTION__);

	return 0;
		
exit_kfree:
	apds990xprintk("APDS990X data free\n");
	kfree(data);
exit:
	return err;
}

static int __devexit apds990x_remove(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	/* Power down the device */
	apds990x_irq_disalbe();
	apds990x_set_enable(client, 0);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM

static int apds990x_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return apds990x_set_enable(client, 0);
}

static int apds990x_resume(struct i2c_client *client)
{
	return apds990x_set_enable(client, 0);
}

#else

#define apds990x_suspend	NULL
#define apds990x_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id apds990x_id[] = {
	{APDS990x_DRV_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, apds990x_id);

static struct i2c_driver apds990x_driver = {
	.driver = {
		.name	= APDS990x_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend = apds990x_suspend,
	.resume	= apds990x_resume,
	.probe	= apds990x_probe,
	.remove	= __devexit_p(apds990x_remove),
	.id_table = apds990x_id,
};

static int __init apds990x_init(void)
{
	return i2c_add_driver(&apds990x_driver);
}

static void __exit apds990x_exit(void)
{
	i2c_del_driver(&apds990x_driver);
}

MODULE_DESCRIPTION("APDS990x ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
module_init(apds990x_init);
module_exit(apds990x_exit);

