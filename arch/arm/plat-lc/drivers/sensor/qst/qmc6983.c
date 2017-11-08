/****************************************************************************
*   This software is licensed under the terms of the GNU General Public License version 2,
*   as published by the Free Software Foundation,
*	and may be copied, distributed, and
*   modified under those terms.
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY
*   WARRANTY; without even the implied warranty of MERCHANTABILITY or
*	FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
*   for more details.
*
*	Copyright (C) 2012-2014 by QST(Shanghai XiRui Keji) Corporation
****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/earlysuspend.h>
#include <plat/qmc6983.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>

#define QMC6983_MAJOR	101
#define QMC6983_MINOR	4

/* Magnetometer registers */
#define CTL_REG_ONE	0x09  /* Contrl register one */
#define CTL_REG_TWO	0x0a  /* Contrl register two */

/* Output register start address*/
#define OUT_X_REG		0x00

/*Status registers */
#define STA_REG_ONE    0x06
#define STA_REG_TWO    0x0c

/* Temperature registers */
#define TEMP_H_REG 		0x08
#define TEMP_L_REG 		0x07

/*different from qmc6983,the ratio register*/
#define RATIO_REG		0x0b


#define DEBUG 1
#define PLATFORM
#define	QMC6983_BUFSIZE		0x20

#define MSE_TAG					"[Msensor] "
#define MSE_FUN(f)				printk(MSE_TAG"%s\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)		printk(KERN_ERR MSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)		printk(MSE_TAG fmt, ##args)

static wait_queue_head_t	open_wq;
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t	a_flag = ATOMIC_INIT(0);
static atomic_t	m_flag = ATOMIC_INIT(0);
static atomic_t	o_flag = ATOMIC_INIT(0);
static atomic_t open_flag = ATOMIC_INIT(0);

/*
 * QMC6983 magnetometer data
 * brief Structure containing magnetic field values for x,y and z-axis in
 * signed short
*/

struct QMC6983_t {
	short	x, /**< x-axis magnetic field data. Range -8000 to 8000. */
		y, /**< y-axis magnetic field data. Range -8000 to 8000. */
		z; /**< z-axis magnetic filed data. Range -8000 to 8000. */
};


struct QMC6983_data{
	struct i2c_client *client;
	struct QMC6983_platform_data *pdata;
	short xy_sensitivity;
	short z_sensitivity;

	struct mutex lock;
	struct delayed_work work;
	struct input_dev *input;
	struct miscdevice qmc_misc;
	int delay_ms;

	int enabled;
	struct completion data_updated;
	wait_queue_head_t state_wq;
};

static struct QMC6983_data *mag;

static struct class *qmc_mag_dev_class;

static char QMC6983_i2c_write(unsigned char reg_addr,
				    unsigned char *data,
				    unsigned char len);

static char QMC6983_i2c_read(unsigned char reg_addr,
				   unsigned char *data,
				   unsigned char len);

static int qmc6983_open(struct inode *inode, struct file *file);
static int qmc6983_release(struct inode *inode, struct file *file);

int QMC6983_read_mag_xyz(struct QMC6983_t *data);

/* 6983 Self Test data collection */
int QMC6983_self_test(char mode, short* buf)
{
	int res = 0;
	return res;
}

/* X,Y and Z-axis magnetometer data readout
 * param *mag pointer to \ref QMC6983_t structure for x,y,z data readout
 * note data will be read by multi-byte protocol into a 6 byte structure
 */
int QMC6983_read_mag_xyz(struct QMC6983_t *data)
{
	int res;
	unsigned char mag_data[6];
	int hw_d[3] = { 0 };

	res = QMC6983_i2c_read(OUT_X_REG, mag_data, 6);
			#if DEBUG
			printk(KERN_INFO "mag_data[%d, %d, %d, %d, %d, %d]\n",
			#endif
	mag_data[0], mag_data[1], mag_data[2],
	mag_data[3], mag_data[4], mag_data[5]);

	hw_d[0] = (short) (((mag_data[1]) << 8) | mag_data[0]);
	hw_d[1] = (short) (((mag_data[3]) << 8) | mag_data[2]);
	hw_d[2] = (short) (((mag_data[5]) << 8) | mag_data[4]);


	hw_d[0] = hw_d[0] * 1000 / mag->xy_sensitivity;
	hw_d[1] = hw_d[1] * 1000 / mag->xy_sensitivity;
	hw_d[2] = hw_d[2] * 1000 / mag->z_sensitivity;

			#if DEBUG
			printk(KERN_INFO "Hx=%d, Hy=%d, Hz=%d\n",hw_d[0],hw_d[1],hw_d[2]);
			#endif
	data->x = ((mag->pdata->negate_x) ? (-hw_d[mag->pdata->axis_map_x])
		   : (hw_d[mag->pdata->axis_map_x]));
	data->y = ((mag->pdata->negate_y) ? (-hw_d[mag->pdata->axis_map_y])
		   : (hw_d[mag->pdata->axis_map_y]));
	data->z = ((mag->pdata->negate_z) ? (-hw_d[mag->pdata->axis_map_z])
		   : (hw_d[mag->pdata->axis_map_z]));

	return res;
}

/* Set the Gain range */
int QMC6983_set_range(short range)
{
	int err = 0;
	int ran ;
	switch (range) {
	case QMC6983_RNG_2G:
		ran = RNG_2G;
		break;
	case QMC6983_RNG_8G:
		ran = RNG_8G;
		break;
	case QMC6983_RNG_12G:
		ran = RNG_12G;
		break;
	case QMC6983_RNG_20G:
		ran = RNG_20G;
		break;
	default:
		return -EINVAL;
	}
	mag->xy_sensitivity = 16000/ran;
	mag->z_sensitivity = 16000/ran;

	return err;
}

/* Set the sensor mode */
int QMC6983_set_mode(char mode)
{
	int err = 0;
	unsigned char data;

	QMC6983_i2c_read(CTL_REG_ONE, &data, 1);
	data &= 0xfc;
	data |= mode;
	err = QMC6983_i2c_write(CTL_REG_ONE, &data, 1);
	return err;
}

int QMC6983_set_ratio(char ratio)
{
	int err = 0;
	unsigned char data;

	data = ratio;
	err = QMC6983_i2c_write(RATIO_REG, &data, 1);
	return err;
}

int QMC6983_set_output_data_rate(char rate)
{
	int err = 0;
	unsigned char data;

	data = rate;
	data &= 0xf3;
	data |= (rate << 2);
	err = QMC6983_i2c_write(CTL_REG_ONE, &data, 1);
	return err;
}

int QMC6983_set_oversample_ratio(char ratio)
{
	int err = 0;
	unsigned char data;

	data = ratio;
	data &= 0x3f;
	data |= (ratio << 6);
	err = QMC6983_i2c_write(CTL_REG_ONE, &data, 1);
	return err;
}


/*  i2c write routine for qmc6983 magnetometer */
static char QMC6983_i2c_write(unsigned char reg_addr,
				    unsigned char *data,
				    unsigned char len)
{
	int dummy;
	int i;

	if (mag->client == NULL)  /*  No global client pointer? */
		return -1;
	for (i = 0; i < len; i++) {
		dummy = i2c_smbus_write_byte_data(mag->client,
						  reg_addr++, data[i]);
		if (dummy) {
			#if DEBUG
			printk(KERN_INFO "i2c write error\n");
			#endif
			return dummy;
		}
	}
	return 0;
}

/*  i2c read routine for QMC6983 magnetometer */
static char QMC6983_i2c_read(unsigned char reg_addr,
				   unsigned char *data,
				   unsigned char len)
{
	char dummy = 0;
	int i = 0;

	if (mag->client == NULL)  /*  No global client pointer? */
		return -1;

	while (i < len) {
		dummy = i2c_smbus_read_byte_data(mag->client,
						 reg_addr++);
			#if DEBUG
			printk(KERN_INFO "read - %d\n", dummy);
			#endif
		if (dummy >= 0) {
			data[i] = dummy;
			i++;
		} else {
			#if DEBUG
			printk(KERN_INFO" i2c read error\n ");
			#endif
			return dummy;
		}
		dummy = len;
	}
	return dummy;
}

static int QMC6983_acc_validate_pdata(struct QMC6983_data *mag)
{
	if (mag->pdata->axis_map_x > 2 ||
	    mag->pdata->axis_map_y > 2 ||
	    mag->pdata->axis_map_z > 2) {
		dev_err(&mag->client->dev,
			"invalid axis_map value x:%u y:%u z%u\n",
			mag->pdata->axis_map_x, mag->pdata->axis_map_y,
			mag->pdata->axis_map_z);
		return -EINVAL;
	}

	if (mag->pdata->negate_x > 1 ||
	    mag->pdata->negate_y > 1 ||
	    mag->pdata->negate_z > 1) {
		dev_err(&mag->client->dev,
			"invalid negate value x:%u y:%u z:%u\n",
			mag->pdata->negate_x, mag->pdata->negate_y,
			mag->pdata->negate_z);
		return -EINVAL;
	}

	return 0;
}

/*-----------------------move from qmc6983 driver basic opt funcs-----------------------------------*/
static void qmc6983_start_measure(struct QMC6983_data *qmc6983)
{
	int err = 0;
	unsigned char data;
	data = 0x1d;
	err = QMC6983_i2c_write(CTL_REG_ONE, &data, 1);
}

static void qmc6983_stop_measure(struct QMC6983_data *qmc6983)
{
	unsigned char data;
	data = 0x1c;
	QMC6983_i2c_write(CTL_REG_ONE, &data, 1);
}

static int qmc6983_enable(struct QMC6983_data *qmc6983)
{
	dev_dbg(&qmc6983->client->dev, "start measure!\n");
	qmc6983_start_measure(qmc6983);

	QMC6983_set_range(QMC6983_RNG_20G);
	QMC6983_set_ratio(1);
	schedule_delayed_work(&qmc6983->work,
			msecs_to_jiffies(qmc6983->delay_ms));
	return 0;
}

static int qmc6983_disable(struct QMC6983_data *qmc6983)
{
	dev_dbg(&qmc6983->client->dev, "stop measure!\n");
	qmc6983_stop_measure(qmc6983);
	cancel_delayed_work(&qmc6983->work);

	return 0;
}

/*-----------------------move from qmc6983 driver input version-----------------------------------*/
static void qmc6983_work(struct work_struct *work)
{
	int ret;
	struct QMC6983_data *qmc6983 = container_of((struct delayed_work *)work,
						struct QMC6983_data, work);
	unsigned char data[6];
	int needRetry = 0;
	mutex_lock(&qmc6983->lock);

	if (!qmc6983->enabled)
		goto out;

	ret = QMC6983_read_mag_xyz((struct QMC6983_t *)data);
	if (ret < 0) {
		dev_err(&qmc6983->client->dev, "error read data\n");
	} else {
			#if DEBUG
			printk(KERN_INFO "X axis: %d\n" , ((struct QMC6983_t *)data)->x);
			printk(KERN_INFO "Y axis: %d\n" , ((struct QMC6983_t *)data)->y);
			printk(KERN_INFO "Z axis: %d\n" , ((struct QMC6983_t *)data)->z);
			#endif
		if ((((struct QMC6983_t *)data)->x==((struct QMC6983_t *)data)->y) && (((struct QMC6983_t *)data)->x==((struct QMC6983_t *)data)->y) ){
			needRetry = 1;
		}else{
			input_report_abs(qmc6983->input, ABS_HAT0X,((struct QMC6983_t *)data)->x);
			input_report_abs(qmc6983->input, ABS_HAT0Y,((struct QMC6983_t *)data)->y);
			input_report_abs(qmc6983->input, ABS_BRAKE,((struct QMC6983_t *)data)->z);
			input_sync(qmc6983->input);
		}
	}


	schedule_delayed_work(&qmc6983->work,msecs_to_jiffies(qmc6983->delay_ms));
out:
	mutex_unlock(&qmc6983->lock);
}

static int qmc6983_input_init(struct QMC6983_data *qmc6983)
{
	struct input_dev *dev;
	int ret;
	#if DEBUG
	printk(KERN_ALERT "qmc6983_input_init\n");
	#endif
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = "qmc6983";
	dev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, dev->evbit);
	input_set_abs_params(dev, ABS_X,
	                     -32768*4, 32768*4, 0, 0);
	input_set_abs_params(dev, ABS_Y,
	                     -32768*4, 32768*4, 0, 0);
	input_set_abs_params(dev, ABS_Z,
	                     -32768*4, 32768*4, 0, 0);
	input_set_abs_params(dev, ABS_WHEEL,
	                     0, 100, 0, 0);

	/* 32768 == 1gauss, range -4gauddss ~ +4gauss */
	/* magnetic raw x-axis */
	input_set_abs_params(dev, ABS_HAT0X,
	                     -32768*4, 32768*4, 0, 0);
	/* magnetic raw y-axis */
	input_set_abs_params(dev, ABS_HAT0Y,
	                     -32768*4, 32768*4, 0, 0);
	/* magnetic raw z-axis */
	input_set_abs_params(dev, ABS_BRAKE,
	                     -32768*4, 32768*4, 0, 0);
	/* magnetic raw status, 0 ~ 3 */
	input_set_abs_params(dev, ABS_GAS,
	                     0, 100, 0, 0);

	/* 65536 == 360degree */
	/* orientation yaw, 0 ~ 360 */
	input_set_abs_params(dev, ABS_RX,
	                     0, 65536, 0, 0);
	/* orientation pitch, -180 ~ 180 */
	input_set_abs_params(dev, ABS_RY,
	                     -65536/2, 65536/2, 0, 0);
	/* orientation roll, -90 ~ 90 */
	input_set_abs_params(dev, ABS_RZ,
	                     -65536/4, 65536/4, 0, 0);
	/* orientation status, 0 ~ 3 */
	input_set_abs_params(dev, ABS_RUDDER,
	                     0, 100, 0, 0);

	/* Magnetic field (limited to 16bit) */
	input_set_drvdata(dev, qmc6983);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	qmc6983->input= dev;
	return 0;
}

static ssize_t
attr_get_poll(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	int pollms;

	mutex_lock(&qmc6983->lock);
	pollms = qmc6983->delay_ms;
	mutex_unlock(&qmc6983->lock);

	return sprintf(buf, "%d\n", pollms);
}

#define QMC6983_MAX_RATE 75

static ssize_t attr_set_poll(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	unsigned long pollms = 0;

	if (strict_strtoul(buf, 10, &pollms) || pollms <= 0)
		return -EINVAL;

	if (pollms < (1000 / QMC6983_MAX_RATE))
		pollms = 1000 / QMC6983_MAX_RATE + 5;

	mutex_lock(&qmc6983->lock);
	qmc6983->delay_ms = pollms;
	mutex_unlock(&qmc6983->lock);

	return size;
}
static DEVICE_ATTR(poll, S_IRUGO | S_IWUSR, attr_get_poll, attr_set_poll);

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	int val;

	mutex_lock(&qmc6983->lock);
	val = qmc6983->enabled;
	mutex_unlock(&qmc6983->lock);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	dev_dbg(&qmc6983->client->dev, "val=%lu\n", val);

	mutex_lock(&qmc6983->lock);
	if (val) {
		qmc6983_enable(qmc6983);
	} else {
		qmc6983_disable(qmc6983);
	}
	qmc6983->enabled = val;
	mutex_unlock(&qmc6983->lock);

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, attr_get_enable, attr_set_enable);

static int id = -1;
static ssize_t attr_get_idchk(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	int val;

	mutex_lock(&qmc6983->lock);
	val = id;
	mutex_unlock(&qmc6983->lock);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_idchk(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	dev_dbg(&qmc6983->client->dev, "val=%lu\n", val);

	mutex_lock(&qmc6983->lock);
	id = i2c_smbus_read_word_data(qmc6983->client, (int)val);
	if ((id & 0x00FF) == 0x0048) {
		printk(KERN_INFO "I2C driver registered!\n");
	}
	id = id & 0x00FF;
	mutex_unlock(&qmc6983->lock);

	return size;
}

static DEVICE_ATTR(idchk, S_IRUGO | S_IWUSR, attr_get_idchk, attr_set_idchk);


#ifdef PLATFORM
static unsigned char regbuf[2] = {0};
static ssize_t show_temperature_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char strbuf[QMC6983_BUFSIZE];
	unsigned char mag_temperature[2];
	unsigned char data;
	int hw_temperature = 0;
	unsigned char rdy = 0;
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);

	/* Check status register for data availability */
	int t1 = 0;
	while (!(rdy & 0x07) && t1 < 3) {
		QMC6983_i2c_read(STA_REG_ONE, &data, 1);
		rdy = data;
		t1++;
	}

	mutex_lock(&qmc6983->lock);
	QMC6983_i2c_read(TEMP_L_REG, &data, 1);
	mag_temperature[0] = data;
	QMC6983_i2c_read(TEMP_H_REG, &data, 1);
	mag_temperature[1] = data;
	mutex_unlock(&qmc6983->lock);
	hw_temperature = ((mag_temperature[1]) << 8) | mag_temperature[0];

	snprintf(strbuf, PAGE_SIZE, "temperature = %d\n", hw_temperature);

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static DEVICE_ATTR(temperature, S_IRUGO, show_temperature_value, NULL);

static ssize_t show_WRregisters_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char strbuf[QMC6983_BUFSIZE];
	unsigned char data;
	unsigned char rdy = 0;
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);

	/* Check status register for data availability */
	int t1 = 0;
	while (!(rdy & 0x07) && t1 < 3) {
		QMC6983_i2c_read(STA_REG_ONE, &data, 1);
		rdy = data;
		t1++;
	}

	mutex_lock(&qmc6983->lock);
	QMC6983_i2c_read(regbuf[0], &data, 1);
	mutex_unlock(&qmc6983->lock);
	snprintf(strbuf, PAGE_SIZE, "hw_registers = 0x%02x\n", data);

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t store_WRregisters_value(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	int err = 0;
	unsigned char data;
	if (NULL == qmc6983)
		return 0;

	mutex_lock(&qmc6983->lock);
	data = *buf;
	err = QMC6983_i2c_write(regbuf[0], &data, 1);
	mutex_unlock(&qmc6983->lock);

	return count;
}

static DEVICE_ATTR(WRregisters, S_IRUGO | S_IWUSR | S_IWGRP |
	S_IWOTH, show_WRregisters_value, store_WRregisters_value);

static ssize_t show_registers_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char strbuf[QMC6983_BUFSIZE];
	snprintf(strbuf, PAGE_SIZE, "hw_registers = 0x%02x\n", regbuf[0]);

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
static ssize_t store_registers_value(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct QMC6983_data *qmc6983 = dev_get_drvdata(dev);
	if (NULL == qmc6983)
		return 0;

	regbuf[0] = *buf;

	return count;
}
static DEVICE_ATTR(registers,   S_IRUGO | S_IWUSR | S_IWGRP |
	S_IWOTH, show_registers_value, store_registers_value);

static ssize_t show_dumpallreg_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char strbuf[300];
	char tempstrbuf[24];
	unsigned char data;
	int length = 0;
	unsigned char rdy = 0;
	int i;

	/* Check status register for data availability */
	int t1 = 0;
	while (!(rdy & 0x07) && t1 < 3) {
		QMC6983_i2c_read(STA_REG_ONE, &data, 1);
		rdy = data;
		t1++;
	}

	for (i = 0; i < 12; i++) {
		QMC6983_i2c_read(i, &data, 1);
		length = snprintf(tempstrbuf, PAGE_SIZE,
			"reg[0x%2x] =  0x%2x\n", i, data);
		snprintf(strbuf+length*i, PAGE_SIZE, "%s\n", tempstrbuf);
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
static DEVICE_ATTR(dumpallreg,  S_IRUGO , show_dumpallreg_value, NULL);
#endif
/********	END	  **********/

static struct attribute *qmc6983_attributes[] = {
/*****QST add at 20140605**************/
#ifdef PLATFORM
	&dev_attr_dumpallreg.attr,
	&dev_attr_WRregisters.attr,
	&dev_attr_registers.attr,
	&dev_attr_temperature.attr,
#endif
/*************end********************/
	&dev_attr_enable.attr,
	&dev_attr_poll.attr,
	&dev_attr_idchk.attr,
	NULL
};

static struct attribute_group qmc6983_attr_group = {
	.name = "qmc6983",
	.attrs = qmc6983_attributes
};


static struct device_attribute attributes[] = {
/*****QST add at 20140605**************/
#ifdef PLATFORM
	__ATTR(dumpallreg,  S_IRUGO , show_dumpallreg_value, NULL),
	__ATTR(WRregisters, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH, show_WRregisters_value, store_WRregisters_value),
	__ATTR(registers,   S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH, show_registers_value, store_registers_value),
	__ATTR(temperature, S_IRUGO, show_temperature_value, NULL),
#endif
/*************end****************/
	__ATTR(pollrate_ms, S_IRUGO | S_IWUSR, attr_get_poll, attr_set_poll),
	__ATTR(enable, S_IRUGO | S_IWUSR, attr_get_enable, attr_set_enable),
	__ATTR(idchk, S_IRUGO | S_IWUSR, attr_get_idchk, attr_set_idchk),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
		if (device_create_file(dev, attributes + i))
			goto error;
	}
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int ECS_set_ypr(int ypr[12])
{
		/* Report acceleration sensor information */
		if (atomic_read(&a_flag)) {
			input_report_abs(mag->input, ABS_X, ypr[0]);
			input_report_abs(mag->input, ABS_Y, ypr[1]);
			input_report_abs(mag->input, ABS_Z, ypr[2]);
			input_report_abs(mag->input, ABS_WHEEL, ypr[3]);
		}

		/* Report magnetic sensor information */
		if (atomic_read(&m_flag)) {
			input_report_abs(mag->input, ABS_HAT0X, ypr[4]);
			input_report_abs(mag->input, ABS_HAT0Y, ypr[5]);
			input_report_abs(mag->input, ABS_BRAKE, ypr[6]);
			input_report_abs(mag->input, ABS_GAS, ypr[7]);
		}

		/* Report orientation information */
		if (atomic_read(&o_flag)) {
			input_report_abs(mag->input, ABS_RX, ypr[8]);
			input_report_abs(mag->input, ABS_RY, ypr[9]);
			input_report_abs(mag->input, ABS_RZ, ypr[10]);
			input_report_abs(mag->input, ABS_RUDDER, ypr[11]);
		}

		input_sync(mag->input);
		return 0;
	}

static int ECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}

struct QMC6983_data *qmc6983;
#define ACC_OFFSET     0
#define QMC_OFFSET     3
#define QMC_CAL_OFFSET 7
#define ORI_OFFSET     11
short all_data[15];
short reg_data[15];
/*  ioctl command for QMC6983 device file */
static long qmc_misc_ioctl(struct file *file,unsigned int cmd, unsigned long arg)
{
	int value[12];			/* for SET_YPR */
	int err = 0;
	unsigned char data[6];
	int rawdata[3] = {0};
	int status; 				/* for OPEN/CLOSE_STATUS */
	int delay;				/* for GET_DELAY */
	short sensor_status;		/* for Orientation and Msensor status */
	short mflag,aflag,oflag;
	uint32_t enable;
	void __user *pa = (void __user *)arg;
	/* check QMC6983_client */
	if (&qmc6983->client == NULL) {
		#if DEBUG
		printk(KERN_ERR "I2C driver not install\n");
		#endif
		return -EFAULT;
	}

	switch (cmd) {
	case QMC6983_SET_RANGE:
		if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
		err = QMC6983_set_range(*data);
		return err;

	case QMC6983_SET_MODE:
		if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
		printk(KERN_ERR "SET_MODE: data %d\n",data[0]);
		err = QMC6983_set_mode(data[0]);
		return err;

	case QMC6983_READ_MAGN_XYZ:
		err = QMC6983_read_mag_xyz((struct QMC6983_t *)data);
			#if DEBUG
		        printk(KERN_INFO "QMC6983_READ_MAGN_XYZ - Ready to report \n");
			#endif
		rawdata[0]=((struct QMC6983_t *)data)->x;
		rawdata[1]=((struct QMC6983_t *)data)->y;
		rawdata[2]=((struct QMC6983_t *)data)->z;
		if (copy_to_user(pa, rawdata,sizeof(rawdata))) {
			return -EFAULT;
		}
		return err;

	case QMC6983_SET_OUTPUT_DATA_RATE:
		if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
		err = QMC6983_set_output_data_rate(data[0]);
		return err;

	case QMC6983_SET_OVERSAMPLE_RATIO:
		if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
		err = QMC6983_set_oversample_ratio(data[0]);
		return err;

	case QMC6983_SELF_TEST:
		return err;
		case ECS_IOCTL_SET_YPR:
			if(pa == NULL)
			{
				printk(KERN_ERR "invalid argument.\n");
				return -EINVAL;
			}
			if(copy_from_user(value, pa, sizeof(value)))
			{
				printk(KERN_ERR "copy_from_user failed.\n");
				return -EFAULT;
			}
			ECS_set_ypr(value);
			break;

		case ECS_IOCTL_GET_OPEN_STATUS:
			status = ECS_GetOpenStatus();
			if(copy_to_user(pa, &status, sizeof(status)))
			{
				printk(KERN_ERR "copy_to_user failed.\n");
				return -EFAULT;
			}
			break;

		case ECS_IOCTL_SET_AFLAG:
			if (copy_from_user(&aflag, pa, sizeof(aflag)))
				return -EFAULT;
			if (aflag < 0 || aflag > 1)
				return -EINVAL;
			atomic_set(&a_flag, aflag);
			break;

		case ECS_IOCTL_GET_AFLAG:
			aflag = atomic_read(&a_flag);
			if (copy_to_user(pa, &aflag, sizeof(aflag)))
				return -EFAULT;
			break;

		case ECS_IOCTL_SET_MFLAG:
			if (copy_from_user(&mflag, pa, sizeof(mflag)))
				return -EFAULT;
			printk(KERN_INFO "kernel set m_flag:%d\n",mflag);
			if (mflag < 0 || mflag > 1)
				return -EINVAL;
			atomic_set(&m_flag, mflag);
			if(atomic_read(&m_flag) == 0)
				qmc6983_disable(qmc6983);
			else
				qmc6983_enable(qmc6983);
			qmc6983->enabled = mflag;
			break;

		case ECS_IOCTL_GET_MFLAG:
			mflag = atomic_read(&m_flag);
			if(copy_to_user(pa, &mflag, sizeof(mflag)))
			{
				printk(KERN_ERR "copy_to_user failed.\n");
				return -EFAULT;
			}
			break;

		case ECS_IOCTL_SET_OFLAG:
			if (copy_from_user(&oflag, pa, sizeof(oflag)))
				return -EFAULT;
			if (oflag < 0 || oflag > 1)
				return -EINVAL;
			atomic_set(&o_flag, oflag);
			if(atomic_read(&o_flag) == 0)
				qmc6983_disable(qmc6983);
			else
				qmc6983_enable(qmc6983);
			qmc6983->enabled = oflag;
			break;

		case ECS_IOCTL_GET_OFLAG:
			sensor_status = atomic_read(&o_flag);
			if(copy_to_user(pa, &sensor_status, sizeof(sensor_status)))
			{
				printk(KERN_ERR "copy_to_user failed.\n");
				return -EFAULT;
			}
			break;

		case ECS_IOCTL_GET_DELAY:
            delay = qmc6983->delay_ms;
            if (copy_to_user(pa, &delay, sizeof(delay))) {
                 printk(KERN_ERR "copy_to_user failed.\n");
                 return -EFAULT;
            }
			break;
		case ECS_IOCTL_SET_DELAY:
			if (copy_from_user(&delay, pa, sizeof(delay)))
				return -EFAULT;
			qmc6983->delay_ms = delay;
			break;
		case ECS_IOCTL_MSENSOR_ENABLE:
			if(pa == NULL)
			{
				printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&enable, pa, sizeof(enable)))
			{
				printk(KERN_ERR "copy_from_user failed.");
				return -EFAULT;
			}
			else
			{
			    printk( "MSENSOR_IOCTL_SENSOR_ENABLE enable=%d!\r\n",enable);
				if(1 == enable)
				{
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}
				}
				wake_up(&open_wq);
			}

			break;
	default:
		return 0;
	}
 return 0;
}

static int qmc6983_open(struct inode *inode, struct file *file)
{

	atomic_set(&open_flag, 1);
	wake_up(&open_wq);

	return 0;
}

static int qmc6983_release(struct inode *inode, struct file *file)
{
	atomic_set(&open_flag, 0);
	wake_up(&open_wq);

	return 0;
}

static struct file_operations qmc_misc_fops = {
	.owner		 = THIS_MODULE,
	.open		= qmc6983_open,
	.release	= qmc6983_release,
	.unlocked_ioctl = qmc_misc_ioctl,
};

static int QMC6983_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{

	int err = 0;

	if(client == NULL){
		printk(KERN_INFO "QMC5983 CLIENT IS NULL");
	}

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		goto exit;

	init_waitqueue_head(&data_ready_wq);
	init_waitqueue_head(&open_wq);
	qmc6983 = kzalloc(sizeof(struct QMC6983_data), GFP_KERNEL);

	if (qmc6983 == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, qmc6983);
	qmc6983->client = client;

	qmc6983->pdata = kmalloc(sizeof(*qmc6983->pdata), GFP_KERNEL);
	if (qmc6983->pdata == NULL)
		goto exit_kfree;

	memcpy(qmc6983->pdata, client->dev.platform_data, sizeof(*qmc6983->pdata));

	err = QMC6983_acc_validate_pdata(qmc6983);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}
	mutex_init(&qmc6983->lock);
	INIT_DELAYED_WORK(&qmc6983->work, qmc6983_work);

	mag = qmc6983;
	/* Create input device for qmc6983 */
	err = qmc6983_input_init(qmc6983);
	if (err < 0) {
		dev_err(&client->dev, "error init input dev interface\n");
		goto exit_kfree;
	}

	err = sysfs_create_group(&client->dev.kobj, &qmc6983_attr_group);
	if (err < 0) {
		dev_err(&client->dev, "sysfs register failed\n");
		goto exit_kfree_input;
	}

	err = create_sysfs_interfaces(&qmc6983->input->dev);
	if (err < 0) {
		dev_err(&client->dev, "sysfs register failed\n");
		goto exit_kfree_input;
	}

	qmc6983->qmc_misc.minor = MISC_DYNAMIC_MINOR;
	qmc6983->qmc_misc.name = "qmc6983";
	qmc6983->qmc_misc.fops = &qmc_misc_fops;

	err = misc_register(&qmc6983->qmc_misc);
	if(err){
		dev_err(&client->dev, "misc register failed\n");
	goto exit_kfree_misc;
  }

	init_completion(&qmc6983->data_updated);
	init_waitqueue_head(&qmc6983->state_wq);

	printk(KERN_INFO "QMC6983 device created successfully\n");
	dev_info(&client->dev, "QMC6983_probe --- 12\n");
	printk(KERN_ALERT "QMC6983_probe --- 12\n");

	qmc6983_enable(qmc6983);

	return 0;

exit_kfree_misc:
	misc_deregister(&qmc6983->qmc_misc);
exit_kfree_pdata:
	kfree(qmc6983->pdata);
exit_kfree_input:
	input_unregister_device(qmc6983->input);
exit_kfree:
	kfree(qmc6983);
exit:
	return err;
}

static int QMC6983_remove(struct i2c_client *client)
{
	struct QMC6983_data *dev = i2c_get_clientdata(client);
	#if DEBUG
		printk(KERN_INFO "QMC6983 driver removing\n");
	#endif
	device_destroy(qmc_mag_dev_class, MKDEV(QMC6983_MAJOR, 0));
	class_destroy(qmc_mag_dev_class);
	unregister_chrdev(QMC6983_MAJOR, "QMC6983");

	kfree(dev);
	mag->client = NULL;
	return 0;
}
#ifdef CONFIG_PM
static int QMC6983_suspend(struct i2c_client *client, pm_message_t state)
{
	#if DEBUG
	printk(KERN_INFO "QMC6983_resume\n");
	#endif
	return 0;
}

static int QMC6983_resume(struct i2c_client *client)
{
	#if DEBUG
	printk(KERN_INFO "QMC6983_resume\n");
	#endif
	return 0;
}
#endif

static const struct i2c_device_id QMC6983_id[] = {
	{ "qmc6983", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, QMC6983_id);

static struct i2c_driver QMC6983_driver = {
	.probe = QMC6983_probe,
	.remove = QMC6983_remove,
	.id_table = QMC6983_id,
	#ifdef CONFIG_PM
	.suspend = QMC6983_suspend,
	.resume = QMC6983_resume,
	#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "qmc6983",
	},
};

static int __init QMC6983_init(void)
{
	int ret;

	ret = i2c_add_driver(&QMC6983_driver);
	printk(KERN_ALERT "QMC6983_init, ret = %d\n", ret);
	return ret;
}

static void __exit QMC6983_exit(void)
{
	#if DEBUG
	printk(KERN_INFO "QMC6983 exit\n");
	#endif
	i2c_del_driver(&QMC6983_driver);
	return;
}


module_init(QMC6983_init);
module_exit(QMC6983_exit);

MODULE_DESCRIPTION("QMC6983 magnetometer driver");
MODULE_AUTHOR("QST");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.1");

