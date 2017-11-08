/* drivers/input/misc/alps-input.c
 *
 * Input device driver for alps sensor
 *
 * Copyright (C) 2012 ALPS ELECTRIC CO., LTD. All Rights Reserved.
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
 */

#ifdef ALPS_IDEV_DEBUG
#define DEBUG 1
#endif

#include <linux/fs.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "alps_compass_d_io.h"
#include "alps-input.h"

static DEFINE_MUTEX(alps_lock);
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);

static struct input_polled_dev *alps_idev;
extern int apls_device_alive;

#define ALPS_INPUT_DEVICE_NAME	"alps_compass"
#define ALPS_IDEV_LOG_TAG	"[ALPS_IDEV], "
#define _HSCDTD_RANGE 4800

#define EVENT_TYPE_ACCEL_X		ABS_X
#define EVENT_TYPE_ACCEL_Y		ABS_Y
#define EVENT_TYPE_ACCEL_Z		ABS_Z
#define EVENT_TYPE_ACCEL_STATUS		ABS_WHEEL

#define EVENT_TYPE_MAGV_X		ABS_HAT0X
#define EVENT_TYPE_MAGV_Y		ABS_HAT0Y
#define EVENT_TYPE_MAGV_Z		ABS_BRAKE
#define EVENT_TYPE_MAGV_STATUS		ABS_THROTTLE

#define EVENT_TYPE_YAW			ABS_RX
#define EVENT_TYPE_PITCH		ABS_RY
#define EVENT_TYPE_ROLL			ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS	ABS_RUDDER

#define EVENT_TYPE_GYR_X		ABS_HAT1X
#define EVENT_TYPE_GYR_Y		ABS_HAT1Y
#define EVENT_TYPE_GYR_Z		ABS_HAT2X
#define EVENT_TYPE_GYR_STATUS	ABS_HAT2Y

#define EVENT_TYPE_LIN_X		ABS_GAS
#define EVENT_TYPE_LIN_Y		ABS_HAT3X
#define EVENT_TYPE_LIN_Z		ABS_HAT3Y

#define EVENT_TYPE_ROT_X		ABS_DISTANCE
#define EVENT_TYPE_ROT_Y		ABS_TILT_X
#define EVENT_TYPE_ROT_Z		ABS_TILT_Y
#define EVENT_TYPE_ROT_R		ABS_TOOL_WIDTH

#define ALPS_POLL_INTERVAL	100	/* msecs */

static void report_value(void);

static int flgActivate;
static int flgSuspend;
static int delay;
static struct TAIFD_HW_DATA sns_hw_data;
static struct TAIFD_SW_DATA sns_sw_data;
static atomic_t data_ready;

/*--------------------------------------------------------------------------
 * I/O Control for sensor HAL
 *--------------------------------------------------------------------------*/
static long alps_ss_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = -1;
	int tmpval;

	switch (cmd) {
	case ALPSIO_SS_SET_ACCACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ss_ioctl(ALPSIO_SS_SET_ACCACTIVATE)\n");
			return -EFAULT;
		}
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_SET_ACCACTIVATE), val = %d\n",
			tmpval);
		mutex_lock(&alps_lock);
		if (tmpval)
			flgActivate |=  ACTIVE_SS_ACC;
		else
			flgActivate &= ~ACTIVE_SS_ACC;
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_ORI)))
			accsns_activate(1, tmpval);
		mutex_unlock(&alps_lock);
		break;
	case ALPSIO_SS_SET_MAGACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ss_ioctl(ALPSIO_SS_SET_MAGACTIVATE)\n");
			return -EFAULT;
		}
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_SET_MAGACTIVATE), val = %d\n",
			tmpval);
		mutex_lock(&alps_lock);
		if (tmpval)
			flgActivate |=  ACTIVE_SS_MAG;
		else
			flgActivate &= ~ACTIVE_SS_MAG;
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_ORI)))
			hscdtd_activate(1, tmpval, delay);
		mutex_unlock(&alps_lock);
		break;

	case ALPSIO_SS_SET_ORIACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ss_ioctl(ALPSIO_SS_SET_ORIACTIVATE)\n");
			return -EFAULT;
		}
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_SET_ORIACTIVATE), val = %d\n",
			tmpval);
		mutex_lock(&alps_lock);
		if (tmpval)
			flgActivate |=  ACTIVE_SS_ORI;
		else
			flgActivate &= ~ACTIVE_SS_ORI;
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_ACC)))
			accsns_activate(1, tmpval);
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_MAG)))
			hscdtd_activate(1, tmpval, delay);
		mutex_unlock(&alps_lock);
		break;

	case ALPSIO_SS_SET_GYRACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ss_ioctl(ALPSIO_SS_SET_GYRACTIVATE)\n");
			return -EFAULT;
		}
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_SET_GYRACTIVATE), val = %d\n",
			tmpval);
		mutex_lock(&alps_lock);
		if (tmpval)
			flgActivate |=  ACTIVE_SS_GYR;
		else
			flgActivate &= ~ACTIVE_SS_GYR;
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_ACC)))
			accsns_activate(1, tmpval);
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_MAG)))
			hscdtd_activate(1, tmpval, delay);
		mutex_unlock(&alps_lock);
		break;
		
	case ALPSIO_SS_SET_LINACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ss_ioctl(ALPSIO_SS_SET_LINACTIVATE)\n");
			return -EFAULT;
		}
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_SET_LINACTIVATE), val = %d\n",
			tmpval);
		mutex_lock(&alps_lock);
		if (tmpval)
			flgActivate |=  ACTIVE_SS_LIN;
		else
			flgActivate &= ~ACTIVE_SS_LIN;
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_ACC)))
			accsns_activate(1, tmpval);
		mutex_unlock(&alps_lock);
		break;
		
	case ALPSIO_SS_SET_ROTACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ss_ioctl(ALPSIO_SS_SET_ROTACTIVATE)\n");
			return -EFAULT;
		}
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_SET_ROTACTIVATE), val = %d\n",
			tmpval);
		mutex_lock(&alps_lock);
		if (tmpval)
			flgActivate |=  ACTIVE_SS_ROT;
		else
			flgActivate &= ~ACTIVE_SS_ROT;
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_ACC)))
			accsns_activate(1, tmpval);
		if (!((tmpval == 0) && (flgActivate & ACTIVE_SS_MAG)))
			hscdtd_activate(1, tmpval, delay);
		mutex_unlock(&alps_lock);
		break;
		
	case ALPSIO_SS_SET_DELAY:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error: alps_ss_ioctl(ALPSIO_SS_SET_DELAY)\n");
			return -EFAULT;
		}
		mutex_lock(&alps_lock);
		if (flgActivate & (ACTIVE_SS_ACC | ACTIVE_SS_LIN)) {
			if (tmpval <  10)
				tmpval = 10;
			else if (tmpval > 200)
				tmpval = 200;
		} else {
			if (tmpval <= 10)
				tmpval = 10;
			else if (tmpval <= 20)
				tmpval = 20;
			else if (tmpval <= 70)
				tmpval = 50;
			else
				tmpval = 100;
		}
		delay = tmpval;
		if (flgActivate & (ACTIVE_SS_MAG | ACTIVE_SS_ORI | ACTIVE_SS_GYR | ACTIVE_SS_ROT))
			hscdtd_activate(1, 1, delay);
		else
			hscdtd_activate(1, 0, delay);
		mutex_unlock(&alps_lock);
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_SET_DELAY), delay = %d\n",
			delay);
		break;

	case ALPSIO_SS_GET_HWDATA:
	{
		int xyz[3];
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ss_ioctl(ALPSIO_SS_GET_HWDATA)\n");
		mutex_lock(&alps_lock);
		ret = hscdtd_get_hardware_data(xyz);
		mutex_unlock(&alps_lock);
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"get hw data, %d, %d, %d\n",
			xyz[0], xyz[1], xyz[2]);
		if (copy_to_user(argp, xyz, sizeof xyz)) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ss_ioctl(ALPSIO_SS_GET_HWDATA)\n");
			return -EFAULT;
		}
	}
	break;

	default:
		return -ENOTTY;
	}
	return 0;
}

static const struct file_operations alps_ss_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= alps_ss_ioctl,
};

static struct miscdevice alps_ss_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "alps_ss_io",
	.fops  = &alps_ss_fops,
};


/*--------------------------------------------------------------------------
 * I/O Control for ALPS Daemon
 *--------------------------------------------------------------------------*/
static long alps_ad_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = -1;
	int tmpval;
	struct TAIFD_HW_DATA tmpval_hw;
	struct TAIFD_SW_DATA tmpval_sw;

	switch (cmd) {
	case ALPSIO_AD_GET_ACTIVATE:
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ad_ioctl(ALPSIO_AD_GET_ACTIVATE)\n");
		mutex_lock(&alps_lock);
		tmpval = flgActivate;
		mutex_unlock(&alps_lock);
		if (copy_to_user(argp, &tmpval, sizeof(tmpval))) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ad_ioctl(ALPSIO_AD_GET_ACTIVATE)\n");
			return -EFAULT;
		}
		break;

	case ALPSIO_AD_GET_DELAY:
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ad_ioctl(ALPSIO_AD_GET_DELAY)\n");
		mutex_lock(&alps_lock);
		tmpval = delay;
		mutex_unlock(&alps_lock);
		if (copy_to_user(argp, &tmpval, sizeof(tmpval))) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error: alps_ad_ioctl(ALPSIO_AD_GET_DELAY)\n");
			return -EFAULT;
		}
		break;

	case ALPSIO_AD_GET_DATA:
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ad_ioctl(ALPSIO_AD_GET_DATA)\n");
		ret = wait_event_interruptible_timeout(data_ready_wq,
			atomic_read(&data_ready), 1000);
		if (ret < 0) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error: "
				"wait_event_interruptible_timeout, %d\n", ret);
			return -EFAULT;
		}
		mutex_lock(&alps_lock);
		if (ret == 0)
			memset(&tmpval_hw, 0, sizeof tmpval_hw);
		else
			memcpy(&tmpval_hw, &sns_hw_data, sizeof tmpval_hw);
		tmpval_hw.activate = flgActivate;
		tmpval_hw.delay    = delay;
		atomic_set(&data_ready, 0);
		mutex_unlock(&alps_lock);
		if (copy_to_user(argp, &tmpval_hw, sizeof tmpval_hw)) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error: alps_ad_ioctl(ALPSIO_AD_GET_DATA)\n");
			return -EFAULT;
		}
		break;

	case ALPSIO_AD_SET_DATA:
		ret = copy_from_user(&tmpval_sw, argp, sizeof(tmpval_sw));
		if (ret) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error: alps_ad_ioctl(ALPSIO_AD_SET_DATA)\n");
			return -EFAULT;
		}
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ad_ioctl(ALPSIO_AD_SET_DATA)\n");
		mutex_lock(&alps_lock);
		memcpy(&sns_sw_data, &tmpval_sw, sizeof sns_sw_data);
		report_value();
		mutex_unlock(&alps_lock);
		break;

	case ALPSIO_AD_EXE_SELF_TEST_A:
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ad_ioctl(ALPSIO_AD_EXE_SELF_TEST_A)\n");
		mutex_lock(&alps_lock);
		ret = hscdtd_self_test_A();
		mutex_unlock(&alps_lock);
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"Self test-A result : %d\n", ret);
		if (copy_to_user(argp, &ret, sizeof(ret))) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ad_ioctl(ALPSIO_AD_EXE_SELF_TEST_A)\n");
			return -EFAULT;
		}
		break;

	case ALPSIO_AD_EXE_SELF_TEST_B:
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ad_ioctl(ALPSIO_AD_EXE_SELF_TEST_B)\n");
		mutex_lock(&alps_lock);
		ret = hscdtd_self_test_B();
		mutex_unlock(&alps_lock);
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"Self test-B result : %d\n", ret);
		if (copy_to_user(argp, &ret, sizeof(ret))) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ad_ioctl(ALPSIO_AD_EXE_SELF_TEST_B)\n");
			return -EFAULT;
		}
		break;

	case ALPSIO_AD_GET_HWDATA:
	{
		int xyz[3];
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"alps_ad_ioctl(ALPSIO_AD_GET_HWDATA)\n");
		mutex_lock(&alps_lock);
		ret = hscdtd_get_hardware_data(xyz);
		mutex_unlock(&alps_lock);
		dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"get hw data, %d, %d, %d\n",
			xyz[0], xyz[1], xyz[2]);
		if (copy_to_user(argp, xyz, sizeof xyz)) {
			dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
				"error:"
				" alps_ad_ioctl(ALPSIO_AD_GET_HWDATA)\n");
			return -EFAULT;
		}
	}
	break;

	default:
		return -ENOTTY;
	}
	return 0;
}

static const struct file_operations alps_ad_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= alps_ad_ioctl,
};

static struct miscdevice alps_ad_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "alps_ad_io",
	.fops  = &alps_ad_fops,
};

/*--------------------------------------------------------------------------
 * sysfs
 *--------------------------------------------------------------------------*/
static ssize_t alps_idev_self_test_A_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = -1;

	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"%s\n", __func__);
	mutex_lock(&alps_lock);
	ret = hscdtd_self_test_A();
	mutex_unlock(&alps_lock);
	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"Self test-A result : %d\n", ret);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t alps_idev_self_test_B_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = -1;

	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"%s\n", __func__);
	mutex_lock(&alps_lock);
	ret = hscdtd_self_test_B();
	mutex_unlock(&alps_lock);
	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"Self test-B result : %d\n", ret);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t alps_idev_get_hw_data_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int xyz[3], ret = -1;

	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"%s\n", __func__);
	mutex_lock(&alps_lock);
	ret = hscdtd_get_hardware_data(xyz);
	mutex_unlock(&alps_lock);
	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"get hw data, %d, %d, %d\n",
		xyz[0], xyz[1], xyz[2]);

	return sprintf(buf, "%d,%d,%d\n", xyz[0], xyz[1], xyz[2]);
}

static struct device_attribute attributes[] = {
	__ATTR(self_test_A, S_IRUGO,
		alps_idev_self_test_A_show, NULL),
	__ATTR(self_test_B, S_IRUGO,
		alps_idev_self_test_B_show, NULL),
	__ATTR(get_hw_data, S_IRUGO,
		alps_idev_get_hw_data_show, NULL)
};

static int alps_idev_create_sysfs(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto out_sysfs;
	return 0;

out_sysfs:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "Unable to create interface\n");
	return -EIO;
}

static void alps_idev_remove_sysfs(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}


/*--------------------------------------------------------------------------
 * suspend/resume function
 *--------------------------------------------------------------------------*/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void alps_idev_early_suspend(struct early_suspend *handler)
{
	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG "%s\n", __func__);
	mutex_lock(&alps_lock);
	flgSuspend = 1;
	mutex_unlock(&alps_lock);
}

static void alps_idev_early_resume(struct early_suspend *handler)
{
	dev_dbg(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG "%s\n", __func__);
	mutex_lock(&alps_lock);
	flgSuspend = 0;
	mutex_unlock(&alps_lock);
}

static struct early_suspend alps_idev_early_suspend_handler = {
	.suspend = alps_idev_early_suspend,
	.resume  = alps_idev_early_resume,
};
#endif


/*--------------------------------------------------------------------------
 * input device
 *--------------------------------------------------------------------------*/

static void accsns_poll(struct input_dev *idev)
{
	int xyz[3];

	if (accsns_get_acceleration_data(xyz) == 0) {
		sns_hw_data.acc[0] = 1;
		memcpy(&sns_hw_data.acc[1], xyz, sizeof (xyz));
	}
}

static void magsns_poll(struct input_dev *idev)
{
	int xyz[3];

	if (hscdtd_get_magnetic_field_data(xyz) == 0) {
		sns_hw_data.mag[0] = 1;
		memcpy(&sns_hw_data.mag[1], xyz, sizeof (xyz));
	}
}

static void alps_poll(struct input_polled_dev *dev)
{
	struct input_dev *idev = dev->input;

	mutex_lock(&alps_lock);
	dev->poll_interval = delay;
	if (!flgSuspend) {
		memset(&sns_hw_data, 0, sizeof sns_hw_data);
		if (flgActivate &
			(ACTIVE_SS_MAG | ACTIVE_SS_ORI | ACTIVE_SS_GYR | ACTIVE_SS_LIN | ACTIVE_SS_ROT))
			magsns_poll(idev);
		
		if (flgActivate & 
			(ACTIVE_SS_ACC | ACTIVE_SS_ORI | ACTIVE_SS_GYR | ACTIVE_SS_LIN))
			accsns_poll(idev);
		
		if (flgActivate &
			(ACTIVE_SS_ACC | ACTIVE_SS_MAG | ACTIVE_SS_ORI | ACTIVE_SS_GYR | ACTIVE_SS_LIN | ACTIVE_SS_ROT)) {
			if ((sns_hw_data.acc[0] == 1) ||
				(sns_hw_data.mag[0] == 1)) {
				atomic_set(&data_ready, 1);
				wake_up(&data_ready_wq);
			}
		}
	}
	mutex_unlock(&alps_lock);
}

static void report_value(void)
{
	if (!flgSuspend) {
		if ((flgActivate & ACTIVE_SS_ACC) && sns_sw_data.acc[0]) {
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ACCEL_X, sns_sw_data.acc[1]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ACCEL_Y, sns_sw_data.acc[2]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ACCEL_Z, sns_sw_data.acc[3]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ACCEL_STATUS, sns_sw_data.acc[4]);
			input_event(alps_idev->input, EV_SYN, SYN_REPORT, 1);
		}
		if ((flgActivate & ACTIVE_SS_MAG) && sns_sw_data.mag[0]) {
			input_report_abs(alps_idev->input,
				EVENT_TYPE_MAGV_X, sns_sw_data.mag[1]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_MAGV_Y, sns_sw_data.mag[2]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_MAGV_Z, sns_sw_data.mag[3]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_MAGV_STATUS, sns_sw_data.mag[4]);
			input_event(alps_idev->input, EV_SYN, SYN_REPORT, 2);
		}
		if ((flgActivate & ACTIVE_SS_ORI) && sns_sw_data.ori[0]) {
			input_report_abs(alps_idev->input,
				EVENT_TYPE_YAW, sns_sw_data.ori[1]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_PITCH, sns_sw_data.ori[2]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ROLL, sns_sw_data.ori[3]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ORIENT_STATUS, sns_sw_data.ori[4]);
			input_event(alps_idev->input, EV_SYN, SYN_REPORT, 3);
		}
		if ((flgActivate & ACTIVE_SS_GYR) && sns_sw_data.gyr[0]) {
			input_report_abs(alps_idev->input,
				EVENT_TYPE_GYR_X, sns_sw_data.gyr[1]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_GYR_Y, sns_sw_data.gyr[2]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_GYR_Z, sns_sw_data.gyr[3]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_GYR_STATUS, sns_sw_data.gyr[4]);
			input_event(alps_idev->input, EV_SYN, SYN_REPORT, 4);
		}
		if ((flgActivate & ACTIVE_SS_LIN) && sns_sw_data.lin[0]) {
			input_report_abs(alps_idev->input,
				EVENT_TYPE_LIN_X, sns_sw_data.lin[1]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_LIN_Y, sns_sw_data.lin[2]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_LIN_Z, sns_sw_data.lin[3]);
			input_event(alps_idev->input, EV_SYN, SYN_REPORT, 5);
		}
		if ((flgActivate & ACTIVE_SS_ROT) && sns_sw_data.rot[0]) {
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ROT_R, sns_sw_data.rot[1]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ROT_X, sns_sw_data.rot[2]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ROT_Y, sns_sw_data.rot[3]);
			input_report_abs(alps_idev->input,
				EVENT_TYPE_ROT_Z, sns_sw_data.rot[4]);
			input_event(alps_idev->input, EV_SYN, SYN_REPORT, 6);
		}
	}
	memset(&sns_sw_data, 0, sizeof(sns_sw_data));
}


/*--------------------------------------------------------------------------
 * module
 *--------------------------------------------------------------------------*/
static int __init alps_init(void)
{
	struct input_dev *idev;
	int ret;

	if (apls_device_alive == 0)
		return -1;
	alps_idev = input_allocate_polled_device();
	if (!alps_idev) {
		ret = -ENOMEM;
		pr_err(ALPS_IDEV_LOG_TAG
			"error: input_allocate_polled_device\n");
		goto out_region;
	}
	pr_info(ALPS_IDEV_LOG_TAG
		"input_allocate_polled_device\n");

	alps_idev->poll = alps_poll;
	alps_idev->poll_interval = ALPS_POLL_INTERVAL;

	/* initialize the input class */
	idev = alps_idev->input;
	idev->name = ALPS_INPUT_DEVICE_NAME;
	idev->id.bustype = BUS_I2C;
	idev->evbit[0] = BIT_MASK(EV_ABS);

	input_set_abs_params(idev, EVENT_TYPE_ACCEL_X,
		-2048, 2048, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_Y,
		-2048, 2048, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_Z,
		-2048, 2048, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_STATUS,
		0, 3, 0, 0);

	input_set_abs_params(idev, EVENT_TYPE_MAGV_X,
		-_HSCDTD_RANGE, _HSCDTD_RANGE, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Y,
		-_HSCDTD_RANGE, _HSCDTD_RANGE, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Z,
		-_HSCDTD_RANGE, _HSCDTD_RANGE, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_STATUS,
		0, 3, 0, 0);

	input_set_abs_params(idev, EVENT_TYPE_YAW,
		0,  360, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_PITCH,
		-180,  180, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ROLL,
		-180,  180, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ORIENT_STATUS,
		0, 3, 0, 0);

	input_set_abs_params(idev, EVENT_TYPE_GYR_X,
		-2000, 2000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_GYR_Y,
		-2000, 2000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_GYR_Z,
		-2000, 2000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_GYR_STATUS,
		0, 3, 0, 0);
	
	input_set_abs_params(idev, EVENT_TYPE_LIN_X,
		-8000000, 8000000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_LIN_Y,
		-8000000, 8000000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_LIN_Z,
		-8000000, 8000000, 0, 0);
	
	input_set_abs_params(idev, EVENT_TYPE_ROT_X,
		-1000000, 1000000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ROT_Y,
		-1000000, 1000000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ROT_Z,
		-1000000, 1000000, 0, 0);
	input_set_abs_params(idev, EVENT_TYPE_ROT_R,
		-1000000, 1000000, 0, 0);
	
	ret = input_register_polled_device(alps_idev);
	if (ret) {
		dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"error: input_register_polled_device\n");
		goto out_alc_poll;
	}
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"input_register_polled_device\n");

	ret = misc_register(&alps_ss_device);
	if (ret) {
		dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"error: misc_register(alps_ss_device)\n");
		goto out_reg_poll;
	}
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"misc_register(alps_ss_device)\n");

	ret = misc_register(&alps_ad_device);
	if (ret) {
		dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"error: misc_register(alps_ad_device)\n");
		goto out_misc_ss;
	}
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"misc_register(alps_ad_device)\n");

	ret = alps_idev_create_sysfs(&alps_idev->input->dev);
	if (ret) {
		dev_err(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
			"error: alps_idev_create_sysfs\n");
		goto out_misc_ad;
	}
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"alps_idev_create_sysfs\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&alps_idev_early_suspend_handler);
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"register_early_suspend\n");
#endif

	mutex_lock(&alps_lock);
	flgActivate = ACTIVE_SS_NUL;
	flgSuspend = 0;
	delay = ALPS_POLL_INTERVAL;
	memset(&sns_hw_data, 0, sizeof sns_hw_data);
	memset(&sns_sw_data, 0, sizeof sns_sw_data);
	mutex_unlock(&alps_lock);
	init_waitqueue_head(&data_ready_wq);

	return 0;

out_misc_ad:
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		" misc_deregister(alps_ad_device)\n");
	misc_deregister(&alps_ad_device);
out_misc_ss:
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		" misc_deregister(alps_ss_device)\n");
	misc_deregister(&alps_ss_device);
out_reg_poll:
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"input_free_polled_device\n");
	input_unregister_polled_device(alps_idev);
out_alc_poll:
	pr_info(ALPS_IDEV_LOG_TAG
		"input_free_polled_device\n");
	input_free_polled_device(alps_idev);
out_region:
	return ret;
}

static void __exit alps_exit(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"unregister_early_suspend\n");
	unregister_early_suspend(&alps_idev_early_suspend_handler);
#endif
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"sysfs_remove_group\n");
	alps_idev_remove_sysfs(&alps_idev->input->dev);
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"misc_deregister(alps_ad_device)\n");
	misc_deregister(&alps_ad_device);
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"misc_deregister(alps_ss_device)\n");
	misc_deregister(&alps_ss_device);
	dev_info(&alps_idev->input->dev, ALPS_IDEV_LOG_TAG
		"input_unregister_polled_device\n");
	input_unregister_polled_device(alps_idev);
	pr_info(ALPS_IDEV_LOG_TAG
		" input_free_polled_device\n");
	input_free_polled_device(alps_idev);
}

module_init(alps_init);
module_exit(alps_exit);

MODULE_DESCRIPTION("Alps Input Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");
