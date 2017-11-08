/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2011  LeadCoreTech Corp.
 *
 *	PURPOSE:			 This files contains the comip hardware plarform's rtc driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include <plat/comip-pmic.h>
#include <plat/switch.h>
#include <plat/comip_codecs_interface.h>

#define CONFIG_SWITCH_COMIP_DEBUG
#ifdef CONFIG_SWITCH_COMIP_DEBUG
#define SWITCH_PRINTK(fmt, args...) printk(KERN_INFO "[SWITCH-COMIP]" fmt, ##args)
#else
#define SWITCH_PRINTK(fmt, args...)
#endif

enum _COMIP_SWITCH {
	SWITCH_SUSPEND_NOEXIST,
	SWITCH_SUSPEND_EXIST,
};

static enum _COMIP_SWITCH  comip_switch = SWITCH_SUSPEND_NOEXIST;
static int hook_after_resume = 0;

#define COMIP_SWITCH_DETECT_DEBOUNCE_TIME		(HZ / 100)
#define COMIP_SWITCH_BUTTON_DEBOUNCE_TIME		(10 * HZ / 100)
#define COMIP_SWITCH_BUTTON_VALID_TIME			(50 * HZ / 100)
#define COMIP_SWITCH_BUTTON_LONG_TIME			(15 * HZ / 10)

#define COMIP_SWITCH_ID2IRQ(switch_id)			\
	gpio_to_irq(COMIP_SWITCH_ID(switch_id))
#define COMIP_SWITCH_ID2IRQTRIG(switch_id)		\
	(gpio_get_value(COMIP_SWITCH_ID(switch_id)) ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH)
#define COMIP_SWITCH_ID2EVENT(switch_id)		\
	((COMIP_SWITCH_ID(switch_id) == PMIC_COMP1) ? PMIC_EVENT_COMP1 : PMIC_EVENT_COMP2)

struct comip_switch_data {
	struct switch_dev sdev;
	struct input_dev *input;
	struct mutex lock;
	struct comip_switch_platform_data *pdata;
	struct delayed_work detect_work;
	struct delayed_work button_work;
	struct timer_list button_timer;
	unsigned long button_time;
	int detect_event;
	int button_event;
	int detect_state;
	int button_state;
	int button_valid;
};

static int call_state = VOICE_CALL_STOP;
static int switch_function_switch(struct notifier_block *blk,
                                  unsigned long code, void *_param)
{
	if(code == VOICE_CALL_STOP) {
		call_state = VOICE_CALL_STOP;
		printk("set comip-switch event wake:0 \n");
	} else {
		call_state = VOICE_CALL_START;
		printk("set comip-switch event wake:1 \n");
	}

	return 0;
}

struct notifier_block switch_call_state_notifier_block = {
	.notifier_call = switch_function_switch,
};

static int comip_switch_state_get(unsigned int switch_id)
{
	int id = COMIP_SWITCH_ID(switch_id);
	int state;

	if (COMIP_SWITCH_IS_PMIC(switch_id))
		state = pmic_comp_state_get((u8)id);
	else
		state = gpio_get_value(id);

	if (COMIP_SWITCH_IS_ACTIVE_LOW(switch_id))
		return !state;

	return !!state;
}

static void comip_switch_int_unmask(unsigned int switch_id, int event)
{
	if (COMIP_SWITCH_IS_PMIC(switch_id))
		pmic_callback_unmask(event);
	else
		enable_irq(event);
}

static void comip_switch_int_mask(unsigned int switch_id, int event)
{
	if (COMIP_SWITCH_IS_PMIC(switch_id))
		pmic_callback_mask(event);
	else
		disable_irq(event);
}

static void comip_switch_button_timer_irq(unsigned long arg)
{
	struct comip_switch_data *data = (struct comip_switch_data *)arg;

	data->button_valid = 1;
}

static void comip_switch_button_work(struct work_struct *work)
{
	struct comip_switch_data *data = container_of(work, struct comip_switch_data, button_work.work);
	int detect_id = data->pdata->detect_id;
	int button_id = data->pdata->button_id;
	int state;
	if (!comip_switch_state_get(detect_id) || !data->button_valid) {
		data->button_state = 0;
		return;
	}

	mutex_lock(&data->lock);

	state = comip_switch_state_get(button_id);
	if (state != data->button_state) {
		if (state) {
			data->button_time = jiffies;
		} else {
			if (time_after(jiffies, data->button_time + COMIP_SWITCH_BUTTON_LONG_TIME)) {
				SWITCH_PRINTK("Hook long pressed.\n");
				input_report_key(data->input, KEY_END, 1);
				input_report_key(data->input, KEY_END, 0);
			} else {
			    if (hook_after_resume == 1)
					hook_after_resume = 0;
				else {
				SWITCH_PRINTK("Hook pressed.\n");
				input_report_key(data->input, KEY_MEDIA, 1);
				input_report_key(data->input, KEY_MEDIA, 0);
				}
			}

			input_sync(data->input);
		}

		data->button_state = state;
	}

	mutex_unlock(&data->lock);
}

static void comip_switch_detect_work(struct work_struct *work)
{
	struct comip_switch_data *data = container_of(work, struct comip_switch_data, detect_work.work);
	int detect_id = data->pdata->detect_id;
	int button_id = data->pdata->button_id;
	int state;

	mutex_lock(&data->lock);
	state = comip_switch_state_get(detect_id);
	if (state != data->detect_state) {
		switch_set_state(&data->sdev, state);
		if (state) {
			data->button_valid = 0;
			if (data->pdata->button_power)
				data->pdata->button_power(1);
			comip_switch_int_unmask(button_id, data->button_event);
			mod_timer(&data->button_timer, jiffies + COMIP_SWITCH_BUTTON_VALID_TIME);
			SWITCH_PRINTK("Headset inserted.\n");
		} else {
			comip_switch_int_mask(button_id, data->button_event);
			if (data->pdata->button_power)
				data->pdata->button_power(0);
			del_timer(&data->button_timer);
			data->button_valid = 0;
			SWITCH_PRINTK("Headset removed.\n");
		}

		if (!COMIP_SWITCH_IS_PMIC(detect_id))
			irq_set_irq_type(data->detect_event, COMIP_SWITCH_ID2IRQTRIG(detect_id));

		data->button_state = 0;
		data->detect_state = state;
	}

	mutex_unlock(&data->lock);
}

static irqreturn_t comip_switch_button_gpio_irq(int irq, void *dev_id)
{
	struct comip_switch_data *data = (struct comip_switch_data *)dev_id;

	if (data->detect_state && data->button_valid)
		schedule_delayed_work(&data->button_work, COMIP_SWITCH_BUTTON_DEBOUNCE_TIME);

	return IRQ_HANDLED;
}

static irqreturn_t comip_switch_detect_gpio_irq(int irq, void *dev_id)
{
	struct comip_switch_data *data = (struct comip_switch_data *)dev_id;

	schedule_delayed_work(&data->detect_work, COMIP_SWITCH_DETECT_DEBOUNCE_TIME);

	return IRQ_HANDLED;
}

static void comip_switch_button_pmic_event(int event, void *puser, void* pargs)
{
	struct comip_switch_data *data = (struct comip_switch_data *)puser;

	if (data->detect_state && data->button_valid)
		schedule_delayed_work(&data->button_work, COMIP_SWITCH_BUTTON_DEBOUNCE_TIME);
}

static void comip_switch_detect_pmic_event(int event, void *puser, void* pargs)
{
	struct comip_switch_data *data = (struct comip_switch_data *)puser;

	schedule_delayed_work(&data->detect_work, COMIP_SWITCH_DETECT_DEBOUNCE_TIME);
}

static int comip_switch_hw_exit(struct comip_switch_data *data)
{
	int detect_id = data->pdata->detect_id;
	int button_id = data->pdata->button_id;

	if (COMIP_SWITCH_IS_PMIC(detect_id)) {
		pmic_callback_mask(data->detect_event);
		pmic_callback_unregister(data->detect_event);
	} else {
		free_irq(data->detect_event, data);
		gpio_free(COMIP_SWITCH_ID(detect_id));
	}

	if (COMIP_SWITCH_IS_PMIC(button_id)) {
		pmic_callback_mask(data->button_event);
		pmic_callback_unregister(data->button_event);
	} else {
		free_irq(data->button_event, data);
		gpio_free(COMIP_SWITCH_ID(button_id));
	}

	return 0;
}

static int comip_switch_hw_init(struct comip_switch_data *data)
{
	int detect_id = data->pdata->detect_id;
	int button_id = data->pdata->button_id;
	int ret = 0;
	data->button_state = 0;
	data->button_valid = 0;
	data->detect_state = comip_switch_state_get(detect_id);
	switch_set_state(&data->sdev, data->detect_state);
	if (COMIP_SWITCH_IS_PMIC(detect_id)) {
		data->detect_event = COMIP_SWITCH_ID2EVENT(detect_id);
		ret = pmic_callback_register(data->detect_event, data, comip_switch_detect_pmic_event);
		pmic_comp_polarity_set(COMIP_SWITCH_ID(detect_id), !COMIP_SWITCH_IS_ACTIVE_LOW(detect_id));
	} else {
		gpio_request(COMIP_SWITCH_ID(detect_id), data->pdata->detect_name);
		gpio_direction_input(COMIP_SWITCH_ID(detect_id));

		data->detect_event = COMIP_SWITCH_ID2IRQ(detect_id);
		ret = request_irq(data->detect_event, comip_switch_detect_gpio_irq,
		                  COMIP_SWITCH_ID2IRQTRIG(detect_id), data->pdata->detect_name, data);
	}
	if (ret)
		goto err_detect_id;

	if (COMIP_SWITCH_IS_PMIC(button_id)) {
		data->button_event = COMIP_SWITCH_ID2EVENT(button_id);
		ret = pmic_callback_register(data->button_event, data, comip_switch_button_pmic_event);
		pmic_comp_polarity_set(COMIP_SWITCH_ID(button_id), !COMIP_SWITCH_IS_ACTIVE_LOW(button_id));
	} else {
		gpio_request(COMIP_SWITCH_ID(button_id), data->pdata->detect_name);
		gpio_direction_input(COMIP_SWITCH_ID(button_id));

		data->button_event = COMIP_SWITCH_ID2IRQ(button_id);
		ret = request_irq(data->button_event, comip_switch_button_gpio_irq,
		                  COMIP_SWITCH_ID2IRQTRIG(button_id), data->pdata->button_name, data);
	}
	if (ret)
		goto err_button_id;

	comip_switch_int_unmask(detect_id, data->detect_event);

	if (data->detect_state) {
		if (data->pdata->button_power)
			data->pdata->button_power(1);
		comip_switch_int_unmask(button_id, data->button_event);
		mod_timer(&data->button_timer, jiffies + COMIP_SWITCH_BUTTON_VALID_TIME);
	} else {
		comip_switch_int_mask(button_id, data->button_event);
		if (data->pdata->button_power)
			data->pdata->button_power(0);
	}

	return 0;

err_button_id:
	if (COMIP_SWITCH_IS_PMIC(detect_id)) {
		pmic_callback_mask(data->detect_event);
		pmic_callback_unregister(data->detect_event);
	} else {
		free_irq(data->detect_event, data);
		gpio_free(COMIP_SWITCH_ID(detect_id));
	}

err_detect_id:
	return ret;
}

static int comip_switch_probe(struct platform_device *pdev)
{
	struct comip_switch_platform_data *pdata = pdev->dev.platform_data;
	struct comip_switch_data *data;
	int ret = 0;
	printk(KERN_INFO "comip_switch_probe");
	if (!pdata)
		return -EBUSY;

	data = kzalloc(sizeof(struct comip_switch_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);

	data->pdata = pdata;
	data->sdev.name = pdata->detect_name;

	/* Register switch for headset detect. */
	ret = switch_dev_register(&data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	/* Register input for headset button. */
	data->input = input_allocate_device();
	if (!data->input) {
		ret = -ENOMEM;
		goto err_request_input_dev;
	}

	data->input->name = pdata->button_name;
	__set_bit(EV_KEY, data->input->evbit);
	__set_bit(KEY_MEDIA, data->input->keybit);
	__set_bit(KEY_END, data->input->keybit);

	ret = input_register_device(data->input);
	if (ret < 0)
		goto err_register_input_dev;

	mutex_init(&data->lock);
	INIT_DELAYED_WORK(&data->detect_work, comip_switch_detect_work);
	INIT_DELAYED_WORK(&data->button_work, comip_switch_button_work);

	init_timer(&data->button_timer);
	data->button_timer.function = comip_switch_button_timer_irq;
	data->button_timer.data = (unsigned long)data;

	ret = comip_switch_hw_init(data);
	if (ret < 0)
		goto err_hardware_init;

	return 0;

err_hardware_init:
	input_unregister_device(data->input);
err_register_input_dev:
	input_free_device(data->input);
err_request_input_dev:
	switch_dev_unregister(&data->sdev);
err_switch_dev_register:
	kfree(data);
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int __devexit comip_switch_remove(struct platform_device *pdev)
{
	struct comip_switch_data *data = platform_get_drvdata(pdev);

	comip_switch_hw_exit(data);
	input_unregister_device(data->input);
	input_free_device(data->input);
	switch_dev_unregister(&data->sdev);
	kfree(data);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int comip_switch_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_switch_data *data = platform_get_drvdata(pdev);
	int button_id = data->pdata->button_id;
	comip_switch_int_mask(button_id, data->button_event);
	if (call_state == VOICE_CALL_STOP) {
		comip_switch = SWITCH_SUSPEND_EXIST;
		if (data->pdata->button_power)
			data->pdata->button_power(0);
	}
	return 0;
}

static int comip_switch_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_switch_data *data = platform_get_drvdata(pdev);
	int button_id = data->pdata->button_id;
	if (comip_switch) {
		if (data->pdata->button_power)
			data->pdata->button_power(1);
	}
	comip_switch_int_unmask(button_id, data->button_event);
	hook_after_resume = 1;
	comip_switch = SWITCH_SUSPEND_NOEXIST;
	return 0;
}

static struct dev_pm_ops comip_switch_pm_ops = {
	.suspend = comip_switch_suspend,
	.resume = comip_switch_resume,
};
#endif

static struct platform_driver comip_switch_driver = {
	.probe	= comip_switch_probe,
	.remove = __devexit_p(comip_switch_remove),
	.driver = {
		.name  = "comip-switch",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &comip_switch_pm_ops,
#endif
	},
};

static int __init comip_switch_init(void)
{
	//register_call_state_notifier(&switch_call_state_notifier_block);
	return platform_driver_register(&comip_switch_driver);
}

static void __exit comip_switch_exit(void)
{
	//unregister_call_state_notifier(&switch_call_state_notifier_block);
	platform_driver_unregister(&comip_switch_driver);
}

module_init(comip_switch_init);
module_exit(comip_switch_exit);

MODULE_DESCRIPTION("COMIP Switch driver based on PMIC or GPIO");
MODULE_LICENSE("GPL");
