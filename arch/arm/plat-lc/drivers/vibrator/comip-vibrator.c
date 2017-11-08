/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip vibrator driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date		Author		Description
 *	1.0.0	2012-03-06	zouqiao		created
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <../../../drivers/staging/android/timed_output.h>

#include <plat/comip-pmic.h>
#include <plat/comip-vibrator.h>


#define VIBRATORING		1
#define VIBSTOP			0

struct comip_vibrator {
	struct delayed_work work;
	struct timed_output_dev tdev;
	int status;
	unsigned long long last_time;
	unsigned int last_value;
	unsigned int current_val;
	unsigned int time_compensation;
	int vibrator_strength;
	struct device *vibrator_dev;
};

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock comip_vibrator_wakelock;
#endif

static int comip_set_vibrator(unsigned char value)
{
	if(value)
		pmic_voltage_set(PMIC_POWER_VIBRATOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
	else
		pmic_voltage_set(PMIC_POWER_VIBRATOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

	return 0;
}

static void vibrator_switch(struct comip_vibrator *vb, int onoff)
{
	if(onoff == VIBSTOP){
		comip_set_vibrator(0);
		vb->status = VIBSTOP;
		vb->last_time = 0;
		vb->last_value = 0;
		vb->current_val =0;
	}else if(onoff == VIBRATORING){
		comip_set_vibrator(vb->vibrator_strength);
		vb->status = VIBRATORING;
	}else
		printk(KERN_DEBUG "Invaild vibrator parameter\n");
}

static void vibrator_off(struct work_struct* pwork)
{
	struct delayed_work *work = container_of(pwork, struct delayed_work, work);
	struct comip_vibrator *pvibrator = container_of(work, struct comip_vibrator, work);

	vibrator_switch(pvibrator, VIBSTOP);
}

static int remain_vibrator_time(struct comip_vibrator *vb)
{
	int remaing;

	remaing = (vb->last_value - jiffies_to_msecs(jiffies - vb->last_time));
	if(remaing > 0)
		return remaing;
	else
		return 0;
}

static int valid(struct comip_vibrator *vb, int value)
{
	int rmt;

	if(value < 1)
		return 0;

	vb->current_val = value;
	if(vb->status == VIBSTOP)
		return 1;

	rmt = remain_vibrator_time(vb);
	if(value > rmt) {
		vb->time_compensation = value - rmt;
		return 1;
	}
	vb->current_val = 0;
	return 0;
}

static void update(struct comip_vibrator *vb)
{
	int value = vb->current_val;

	vb->status = VIBRATORING;
	vb->last_time = jiffies;
	vb->last_value = value;

	if(vb->time_compensation > 0) {
		cancel_delayed_work_sync(&vb->work);
		vb->time_compensation = 0;
	}

	vibrator_switch(vb, VIBRATORING);
	schedule_delayed_work(&vb->work, msecs_to_jiffies(value));

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&comip_vibrator_wakelock, msecs_to_jiffies(value));
#endif
}

static void comip_vibrator_init(struct comip_vibrator *vb)
{
	INIT_DELAYED_WORK(&vb->work, vibrator_off);
	vb->status = VIBSTOP;
	vb->last_time = 0;
	vb->last_value = 0;
	vb->current_val =0;
	vb->time_compensation = 0;
}

static inline void comip_vibrator_enable(struct timed_output_dev *ptdev, int value)
{
	struct comip_vibrator *pvibrator = container_of(ptdev, struct comip_vibrator, tdev);

	if (value == VIBSTOP) {
		cancel_delayed_work_sync(&pvibrator->work);
		vibrator_switch(pvibrator, VIBSTOP);
	}else {
		if(valid(pvibrator, value) == 0)
			return;
		update(pvibrator);
	}
}

static int comip_vibrator_get_time(struct timed_output_dev *ptdev)
{
	struct comip_vibrator *pvibrator = container_of(ptdev, struct comip_vibrator, tdev);

	return remain_vibrator_time(pvibrator);
}

static int comip_timed_vibrator_probe(struct platform_device * pdev)
{
	struct comip_vibrator *pvibrator;
	struct comip_vibrator_platform_data *pvb_pdata = pdev->dev.platform_data;

	pvibrator = (struct comip_vibrator *)kzalloc(sizeof(struct comip_vibrator), GFP_KERNEL);
	if(!pvibrator)
		return -ENOMEM;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&comip_vibrator_wakelock, WAKE_LOCK_SUSPEND, "vibrator");
#endif

	comip_vibrator_init(pvibrator);

	pvibrator->tdev.enable = comip_vibrator_enable;
	pvibrator->tdev.get_time = comip_vibrator_get_time;
	pvibrator->tdev.name = pvb_pdata->name;
	pvibrator->vibrator_strength = pvb_pdata->vibrator_strength;

	platform_set_drvdata(pdev, pvibrator);

	return timed_output_dev_register(&pvibrator->tdev);
}

static int comip_timed_vibrator_remove(struct platform_device * pdev)
{
	struct comip_vibrator *pvibrator = platform_get_drvdata(pdev);

	if (pvibrator) {
		timed_output_dev_unregister(&pvibrator->tdev);
		platform_set_drvdata(pdev, NULL);
		kfree(pvibrator);
	}

	return 0;
}

static struct platform_driver timed_vibrator_driver = {
	.probe	= comip_timed_vibrator_probe,
	.remove = comip_timed_vibrator_remove,
	.driver = {
		.name	= "comip-vibrator",
		.owner	= THIS_MODULE,
	},
};

static int __init timed_vibrator_init(void)
{
	return platform_driver_register(&timed_vibrator_driver);
}

static void __exit timed_vibrator_exit(void)
{
	platform_driver_unregister(&timed_vibrator_driver);
}

module_init(timed_vibrator_init);
module_exit(timed_vibrator_exit);

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("timed output pmic vibrator device");
MODULE_LICENSE("GPL");
