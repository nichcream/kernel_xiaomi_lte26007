/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip power key driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version Date		Author		Description
 *	1.0.0	2012-03-06	zouqiao 	created
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <plat/comip-pmic.h>


static struct wake_lock pwrkey_wakelock;

static void comip_powerkey_event(int event, void *puser, void* pargs)
{
	unsigned int irq_status = *((unsigned int *)pargs);
	int status = pmic_power_key_state_get();
	struct input_dev * powerkey_input = (struct input_dev *)puser;
	static unsigned long start = 0;
	static int prev_hw_state = 0xFFFF;
	static int push_release_flag;
	WARN_ON(event != PMIC_EVENT_POWER_KEY);


	if(push_release_flag == 1){
		if(((irq_status == IRQ_POWER_KEY_PRESS) &&(status == 0x00))
					||(irq_status == IRQ_POWER_KEY_PRESS_RELEASE))
			push_release_flag = 0;
	}
	if ((prev_hw_state != status) && (prev_hw_state != 0xFFFF)) {
		push_release_flag = 0;
		input_report_key(powerkey_input, KEY_POWER, status);
		input_sync(powerkey_input);
	} else if (!push_release_flag) {
		wake_lock_timeout(&pwrkey_wakelock, msecs_to_jiffies(300));
		printk("[%s] press_or release = 0x%x(0x%x),irq_event = 0x%x!\n",
			__FUNCTION__, status,prev_hw_state,irq_status);
		push_release_flag = 1;
		input_report_key(powerkey_input, KEY_POWER, !status);
		input_sync(powerkey_input);
		msleep(15);
		input_report_key(powerkey_input, KEY_POWER, status);
		input_sync(powerkey_input);
	} else{
		printk(KERN_ERR"[%s] invalid = 0x%x(0x%x),irq_event = 0x%x\n",
			__FUNCTION__, status,prev_hw_state,irq_status);
		push_release_flag = 0;
	}

	 prev_hw_state = status;

	 if (status)
		 start = jiffies;
	 else
		 printk(KERN_DEBUG "power key release. elapse %ld\n", jiffies - start);
}

static int comip_powerkey_probe(struct platform_device *pdev)
{
	struct input_dev * powerkey_input;
	int error = -1;

	powerkey_input = input_allocate_device();

	set_bit(EV_KEY, powerkey_input->evbit);

	powerkey_input->name = "comip-powerkey";
	powerkey_input->id.bustype = BUS_HOST;

	set_bit(KEY_POWER, powerkey_input->keybit);

	error = input_register_device(powerkey_input);
	if (error) {
		printk(KERN_ERR "Unable to register power key input device!\n");
		goto err_input;
	}

	platform_set_drvdata(pdev, powerkey_input);
	wake_lock_init(&pwrkey_wakelock, WAKE_LOCK_SUSPEND, "pwrkey_wake_lock");
	error = pmic_callback_register(PMIC_EVENT_POWER_KEY, powerkey_input, comip_powerkey_event);
	if (error) {
		printk(KERN_ERR "Unable to register power key callback!\n");
		goto err_pmic;
	}

	pmic_callback_unmask(PMIC_EVENT_POWER_KEY);

	return 0;

err_pmic:
	wake_lock_destroy(&pwrkey_wakelock);
	input_unregister_device(powerkey_input);
err_input:
	input_free_device(powerkey_input);

	return error;
}

static int comip_powerkey_remove(struct platform_device *pdev)
{
	struct input_dev *powerkey_input = platform_get_drvdata(pdev);

	if (powerkey_input) {
		input_unregister_device(powerkey_input);
		input_free_device(powerkey_input);
	}
	wake_lock_destroy(&pwrkey_wakelock);
	pmic_callback_mask(PMIC_EVENT_POWER_KEY);
	pmic_callback_unregister(PMIC_EVENT_POWER_KEY);

	return 0;
}

static int comip_powerkey_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int comip_powerkey_resume(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver comip_powerkey_driver = {
	.probe = comip_powerkey_probe,
	.remove = comip_powerkey_remove,
	.suspend = comip_powerkey_suspend,
	.resume = comip_powerkey_resume,
	.driver = {
		.name = "comip-powerkey",
	},
};

static int __init comip_powerkey_init(void)
{
	return platform_driver_register(&comip_powerkey_driver);
}

static void __exit comip_powerkey_exit(void)
{
	platform_driver_unregister(&comip_powerkey_driver);
}

module_init(comip_powerkey_init);
module_exit(comip_powerkey_exit);

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("pmic power key driver");
MODULE_LICENSE("GPL");
