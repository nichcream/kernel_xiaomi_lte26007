/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip hardware plarform's rtc driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2012-03-06	zouqiao		created
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/time.h>

#include <plat/comip-pmic.h>

struct comip_rtc_data {
	struct rtc_device *rtc;
	int event;
};

/**
 * comip_rtc_event() - handle rtc event
 * @event:	  rtc event type
 * @puser:	  user data of this rtc event
 * @pargs:	  parameters of this rtc event
 *
 * to handle rtc event
 **/
static void comip_rtc_event(int event, void *puser, void* pargs)
{
	unsigned long events = RTC_IRQF | RTC_AF;
	struct rtc_device *rtc = (struct rtc_device *)(puser);

	/* update irq data & counter */
	rtc_update_irq(rtc, 1, events);
}

/**
 * comip_rtc_read_time() - callback needed by rtc framework
 * @dev:	device structure from VFS
 * @tm: 	for setting rtc time
 *
 * to set rtc time to rtc module
 **/
static int comip_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	int ret;
	int retries = 3;

	/* Retry several times if date/time is invalid. */
	do {
		ret = pmic_rtc_time_get(tm);
		if (ret)
			return ret;
		ret = rtc_valid_tm(tm);
	} while (ret && retries--);

	if (ret)
		printk(KERN_DEBUG "comip_rtc_read_time invalid:%d-%d-%d %d:%d:%d\n",
			tm->tm_year, tm->tm_mon, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);

	return ret;
}

/**
 * comip_rtc_set_time() - callback needed by rtc framework
 * @dev:	device structure from VFS
 * @tm: 	for setting rtc time
 *
 * to set rtc time to rtc module
 **/
static int comip_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return pmic_rtc_time_set(tm);
}

/**
 * comip_rtc_read_alarm() - callback needed by rtc framework
 * @dev:	device structure from VFS
 * @alrm:	  for return alarm	setting
 *
 * for hardware reason, here we support only a daily alarm
 **/
static int comip_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	return pmic_rtc_alarm_get(PMIC_RTC_ALARM_NORMAL, alrm);
}

/**
 * comip_rtc_set_alarm() - callback needed by rtc framework
 * @dev:	device structure from VFS
 * @alrm:	  for setting alarm
 *
 * for hardware reason, here we can only set a daily alarm
 **/
static int comip_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct comip_rtc_data *data = dev_get_drvdata(dev);
	int ret;

	if (!alrm->enabled)
		pmic_callback_mask(data->event);

	ret = pmic_rtc_alarm_set(PMIC_RTC_ALARM_NORMAL, alrm);

	if (!ret && alrm->enabled)
		pmic_callback_unmask(data->event);

	return ret;
}

/**
 * comip_rtc_ioctl() - main ioctl function for rtc driver
 * @dev:	device structure from VFS
 * @cmd:	ioctl cmd
 * @arg:	ioctl argument
 *
 * rtc framework ioctl will call this function firstly, unhandled will be
 * handled by rtc framework ioctl
 *
 * typically, RTC_ALM_READ, RTC_ALM_SET, RTC_RD_TIME, RTC_SET_TIME is handled
 * by rtc framework ioctl, and which will call for callback functions:
 * read_time,set_time,read_alarm,set_alarm,  which is implemented by low level
 * rtc driver
 *
 * Please see code or test case for detailed ioctls we support.
 **/
static int comip_rtc_ioctl(struct device *dev, unsigned int cmd,
			   unsigned long arg)
{
	int ret = 0;
	struct comip_rtc_data *data = dev_get_drvdata(dev);

	switch (cmd) {
	case RTC_AIE_OFF:
		pmic_callback_mask(data->event);
		break;
	case RTC_AIE_ON:
		pmic_callback_unmask(data->event);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

/**
 * comip_rtc_proc() - output some message to user via proc interface
 * @dev:	  standard proc func parameter
 * @seq:	  standard proc func parameter
 *
 * the rtc framework will has it's own proc func, and this func append some msg
 *
 * now we output the valie of rtc registers
 **/
static int comip_rtc_proc(struct device *dev, struct seq_file *seq)
{
	return 0;
}

static const struct rtc_class_ops comip_rtc_ops = {
	.ioctl		= comip_rtc_ioctl,
	.read_time	= comip_rtc_read_time,
	.set_time	= comip_rtc_set_time,
	.read_alarm = comip_rtc_read_alarm,
	.set_alarm	= comip_rtc_set_alarm,
	.proc		= comip_rtc_proc,
};

/**
 * comip_rtc_probe() - probe function for comip driver
 * @pdev:	   device structure from bus driver
 *
 **/
static int comip_rtc_probe(struct platform_device *pdev)
{
	struct comip_rtc_data *data;
	struct rtc_device *rtc;
	int ret = 0;

	device_init_wakeup(&pdev->dev, 1);

	rtc = rtc_device_register(pdev->name, &pdev->dev, &comip_rtc_ops,
				  THIS_MODULE);
	if (IS_ERR(rtc)) {
		ret = PTR_ERR(rtc);
		goto exit;
	}

	if (!(data = kzalloc(sizeof(struct comip_rtc_data), GFP_KERNEL))) {
		printk(KERN_ERR "comip-rtc: rtc kzalloc error.\n");
		ret = -ENOMEM;
		goto exit_device_unregister;
	}

	data->rtc = rtc;
	platform_set_drvdata(pdev, data);

	data->event = PMIC_RTC_ALARM_NORMAL ? PMIC_EVENT_RTC2 : PMIC_EVENT_RTC1;
	ret = pmic_callback_register(data->event, rtc, comip_rtc_event);
	if (ret) {
		printk(KERN_ERR "comip-rtc: rtc comip event callback register error.\n");
		goto exit_kfree;
	}

	return 0;

exit_kfree:
	kfree(data);
exit_device_unregister:
	rtc_device_unregister(rtc);
exit:
	return ret;
}

/**
 * comip_rtc_remove() - called when device is removed from bus
 * @pdev:	   device structure from bus driver
 *
 * unregister rtc device from rtc framework
 **/
static int comip_rtc_remove(struct platform_device *pdev)
{
	struct comip_rtc_data *data = platform_get_drvdata(pdev);

	pmic_callback_mask(data->event);
	pmic_callback_unregister(data->event);

	if (data) {
		if (data->rtc)
			rtc_device_unregister(data->rtc);

		kfree(data);
	}

	return 0;
}

static struct platform_driver comip_rtc_drv = {
	.driver = {
		.name = "comip-rtc",
	},
	.probe	 = comip_rtc_probe,
	.remove  = comip_rtc_remove,
};

/**
 * comip_rtc_init() - module init func
 *
 * register rtc device and driver to platform bus
 **/
static int __init comip_rtc_init(void)
{
	int ret;

	ret = platform_driver_register(&comip_rtc_drv);
	if (ret)
		printk(KERN_ERR "comip-rtc: rtc driver register error.\n");

	return 0;
}

/**
 * comip_rtc_exit() - module exit func
 *
 * unregister rtc device and driver
 **/
static void __exit comip_rtc_exit(void)
{
	platform_driver_unregister(&comip_rtc_drv);
}

module_init(comip_rtc_init);
module_exit(comip_rtc_exit);

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP Realtime Clock Driver (RTC)");
MODULE_LICENSE("GPL");

