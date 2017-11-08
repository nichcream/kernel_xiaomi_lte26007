/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2011  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip hardware plarform's rtc driver.
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
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <plat/switch.h>
#include <plat/comip_codecs_interface.h>

#define SWITCH_DBG(fmt, args...) printk(KERN_ERR "[SWITCH-COMIP]" fmt, ##args)
#define SWITCH_ERR(fmt, args...) printk(KERN_ERR "[SWITCH-COMIP]" fmt, ##args)

#define COMIP_SWITCH_DETECT_DEBOUNCE_TIME		(HZ / 100)
#define COMIP_SWITCH_BUTTON_DEBOUNCE_TIME		(10 * HZ / 100)
#define COMIP_SWITCH_BUTTON_VALID_TIME			(50 * HZ / 100)
#define COMIP_SWITCH_BUTTON_LONG_TIME			(15 * HZ / 10)
#define COMIP_SWITCH_BUTTON_LONG_TIME_OUT		(8 * HZ)
#define COMIP_SWITCH_BUTTON_RESUME_TIME			(20 * HZ / 100)
#define COMIP_SWITCH_DETECT_MIC_TIME			(10 * HZ / 100)

struct comip_switch_data {
	struct switch_dev sdev;
	struct input_dev *input;
	struct mutex lock;
	struct wake_lock wakelock;
	struct comip_switch_platform_data *pdata;
	struct comip_switch_hw_info hw_info;
	struct delayed_work detect_work;
	struct delayed_work button_work;
	struct delayed_work detect_int_work;
	struct delayed_work detect_mic_work;
	struct timer_list button_timer;
	unsigned long button_time;
	int detect_event;
	int button_event;
	int detect_state;
	int button_state;
	int button_valid;
	int call_state;
	int mic_state;
	int button_type;
	unsigned char int_type;
};

static struct comip_switch_ops *switch_ops = NULL;
static struct comip_switch_data *switch_ctx = NULL;

#define switch_dev_call(f, args...)		\
	(!(switch_ops) ? -ENODEV : (switch_ops->f ?	 \
		switch_ops->f(args) : -ENOIOCTLCMD))

int comip_switch_register_ops(struct comip_switch_ops *ops)
{
	switch_ops = ops;
	return 0;
}
EXPORT_SYMBOL_GPL(comip_switch_register_ops);

int comip_switch_unregister_ops(void)
{
	switch_ops = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(comip_switch_unregister_ops);

static int switch_function_switch(struct notifier_block *blk,
                                  unsigned long code, void *_param)
{
	if(code == VOICE_CALL_STOP) {
		switch_ctx->call_state = VOICE_CALL_STOP;
		SWITCH_DBG("set comip-switch event wake:0 \n");
	} else {
		switch_ctx->call_state = VOICE_CALL_START;
		SWITCH_DBG("set comip-switch event wake:1 \n");
	}

	return 0;
}

struct notifier_block switch_call_state_notifier_block = {
	.notifier_call = switch_function_switch,
};

static int comip_switch_button_power(int onoff)
{
	int ret = 0;

	if (switch_ctx->pdata->button_power)
		switch_ctx->pdata->button_power(onoff);

	ret = switch_dev_call(micbias_power, onoff);
	if (ret) {
		SWITCH_ERR("%s: power up micbias failed \n", __func__);
		return ret;
	}

	return 0;
}

static void comip_switch_button_timer_irq(unsigned long arg)
{
	struct comip_switch_data *data = (struct comip_switch_data *)arg;

	data->button_valid = 1;
}

static void comip_switch_detect_mic_work(struct work_struct *work)
{
	struct comip_switch_data *data = container_of(work,
			struct comip_switch_data, detect_mic_work.work);
	int button_status1;
	int button_status2;
	int jack_status;
	int cnt1 = 0;
	int cnt2 = 0;
	int report_state = SWITCH_JACK_IN_NOMIC;
	if (switch_dev_call(button_int_mask, 0)) {
		SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
		return;
	}

	do {
		if (switch_dev_call(button_status_get, &button_status1)) {
			SWITCH_ERR("%s: get button status failed \n", __func__);
			return;
		}
		msleep(10);
		if (switch_dev_call(button_status_get, &button_status2)) {
			SWITCH_ERR("%s: get button status failed \n", __func__);
			return;
		}
		cnt1++;
		if ((button_status1 != button_status2))
			cnt1 = 0;

		if ((cnt1 == 9) && (button_status2 == SWITCH_BUTTON_RELEASE)) {
			data->mic_state = 1;
			report_state = SWITCH_JACK_IN_MIC;
		}

		cnt2++;
	} while ((cnt1 < 10) && (cnt2 < 100));

	if (switch_dev_call(jack_status_get, &jack_status)) {
		SWITCH_ERR("%s: get jack status failed \n", __func__);
		return;
	}

	if (jack_status == 0) {
		SWITCH_DBG("state = %d, but JACK OUT! \n", report_state);
		report_state = SWITCH_JACK_OUT;
		data->detect_state=0;
	}else{
		data->detect_state=1;
	}

	switch_set_state(&data->sdev, report_state);

	SWITCH_DBG("jack in. cnt1=%d, cnt2=%d, report_state=%d \n",
			cnt1, cnt2, report_state);

	if (report_state == SWITCH_JACK_IN_MIC) {
		data->button_valid = 0;

		mod_timer(&data->button_timer, jiffies + COMIP_SWITCH_BUTTON_VALID_TIME);
		SWITCH_DBG("jack in & button enable.\n");
	} else {
		data->button_valid = 0;
		if (switch_dev_call(button_int_mask, 1)) {
			SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
			return;
		}
		if (comip_switch_button_power(0))
			return;
		if (switch_dev_call(button_enable, 0)) {
			SWITCH_ERR("%s: enable button failed \n", __func__);
			return;
		}
	}

	return;
}

static void comip_switch_int_work(struct work_struct *work)
{
	struct comip_switch_data *data = container_of(work, 
			struct comip_switch_data, detect_int_work.work);
	int state;

	if (data->int_type == COMIP_SWITCH_INT_JACK){
		mutex_lock(&data->lock);
		if (switch_dev_call(jack_status_get, &state)) {
			SWITCH_ERR("%s: get jack status failed \n", __func__);
			goto out;
		}
		SWITCH_DBG("JACK_INT: %d, %d\n", state, data->detect_state);
		if (state != data->detect_state) {
			cancel_delayed_work_sync(&data->button_work);
			if (state) {
				/* Jack in. */
				if (comip_switch_button_power(1))
					goto out;
				if (switch_dev_call(button_enable, 1)) {
					SWITCH_ERR("%s: enable button failed \n", __func__);
					goto out;
				}

				schedule_delayed_work(&data->detect_mic_work, COMIP_SWITCH_DETECT_MIC_TIME);
			} else {
				/* Jack out. */
				switch_set_state(&data->sdev, state);
				SWITCH_DBG("Headset removed.\n");
				if (switch_dev_call(button_int_mask, 1)) {
					SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
					goto out;
				}
				if (switch_dev_call(button_enable, 0)) {
					SWITCH_ERR("%s: disable button failed \n", __func__);
					goto out;
				}
				if (comip_switch_button_power(0))
					goto out;
				del_timer(&data->button_timer);
				data->button_valid = 0;
				data->mic_state = 0;
			}

			data->button_state = 0;
			data->detect_state = state;
		}
		mutex_unlock(&data->lock);
	}else if ((data->int_type == COMIP_SWITCH_INT_BUTTON) 
			&& (data->button_valid == 1)) {
		mutex_lock(&data->lock);
		if (switch_dev_call(button_status_get, &state)) {
			SWITCH_ERR("%s: get button status failed \n", __func__);
			goto out;
		}
		SWITCH_DBG("HOOK: %d, %d \n", state, data->button_state);
		if (state != data->button_state) {
			if (state) {
				data->button_time = jiffies;
				if (switch_dev_call(button_type_get, &data->button_type)) {
					SWITCH_ERR("%s: get button type failed \n", __func__);
					goto out;
				}
				SWITCH_DBG("Button type is %d. \n", data->button_type);
			} else {
				if (time_after(jiffies, data->button_time + COMIP_SWITCH_BUTTON_LONG_TIME) && \
					time_before(jiffies, data->button_time + COMIP_SWITCH_BUTTON_LONG_TIME_OUT)) {
					if (data->button_type == SWITCH_KEY_HOOK) {
						input_report_key(data->input, KEY_END, 1);
						input_report_key(data->input, KEY_END, 0);
					} else if (data->button_type == SWITCH_KEY_UP) {
						input_report_key(data->input, KEY_PREVIOUSSONG, 1);
						input_report_key(data->input, KEY_PREVIOUSSONG, 0);
					} else if (data->button_type == SWITCH_KEY_DOWN) {
						input_report_key(data->input, KEY_NEXTSONG, 1);
						input_report_key(data->input, KEY_NEXTSONG, 0);
					}		
					SWITCH_DBG("button long pressed, key = %d.\n", data->button_type);
				} else {
					if (data->button_type == SWITCH_KEY_HOOK) {
						input_report_key(data->input, KEY_MEDIA, 1);
						input_report_key(data->input, KEY_MEDIA, 0);
					} else if (data->button_type == SWITCH_KEY_UP) {
						input_report_key(data->input, KEY_PREVIOUSSONG, 1);
						input_report_key(data->input, KEY_PREVIOUSSONG, 0);
					} else if (data->button_type == SWITCH_KEY_DOWN) {
						input_report_key(data->input, KEY_NEXTSONG, 1);
						input_report_key(data->input, KEY_NEXTSONG, 0);
					}		
					SWITCH_DBG("button short pressed, key = %d.\n", data->button_type);
				}
				input_sync(data->input);
			}
			data->button_state = state;
		}else if (time_after(jiffies,data->button_time + \
		COMIP_SWITCH_BUTTON_LONG_TIME_OUT) && state){
			data->button_time = jiffies;
		}
		mutex_unlock(&data->lock);
	} 

out:
	if (switch_dev_call(irq_handle_done, data->int_type)) {
		SWITCH_ERR("%s: irq handle done failed \n", __func__);
	}
}

static void comip_switch_button_work(struct work_struct *work)
{
	struct comip_switch_data *data = container_of(work, struct comip_switch_data,
		button_work.work);

	if (data->detect_state == 1 && data->mic_state == 1) {
		data->button_valid = 0;
		if (switch_dev_call(button_enable, 1)) {
			SWITCH_ERR("%s: enable button failed \n", __func__);
			return;
		}
		if (switch_dev_call(button_int_mask, 0)) {
			SWITCH_ERR("%s: unmask button interrupt failed \n", __func__);
			return;
		}
		mod_timer(&data->button_timer, jiffies + COMIP_SWITCH_BUTTON_VALID_TIME);
	}
}

static int comip_switch_isr(int type)
{
	wake_lock_timeout(&switch_ctx->wakelock, 20 * HZ / 10);

	switch_ctx->int_type = type;

	schedule_delayed_work(&switch_ctx->detect_int_work,
			COMIP_SWITCH_DETECT_DEBOUNCE_TIME);

	return 0;
}

static int comip_switch_probe(struct platform_device *pdev)
{
	struct comip_switch_platform_data *pdata = pdev->dev.platform_data;
	struct comip_switch_data *data;
	int jack_active_level = 1;
	int button_active_level = 1;
	int ret = 0;

	printk("comip_switch_probe \n");

	if (!pdata) {
		SWITCH_ERR("%s: pdata is null.\n", __func__);
		return -EBUSY;
	}

	data = kzalloc(sizeof(struct comip_switch_data), GFP_KERNEL);
	if (!data) {
		SWITCH_ERR("%s: kzalloc failed.\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, data);
	wake_lock_init(&data->wakelock, WAKE_LOCK_SUSPEND, "switch_wakelock");

	switch_ctx = data;

	data->pdata = pdata;
	data->sdev.name = pdata->detect_name;

	/* Register switch for headset detect. */
	ret = switch_dev_register(&data->sdev);
	if (ret < 0) {
		SWITCH_ERR("%s: register switch dev failed.\n", __func__);
		goto err_switch_dev_register;
	}

	/* Register input for headset button. */
	data->input = input_allocate_device();
	if (!data->input) {
		SWITCH_ERR("%s: allocate input device failed.\n", __func__);
		ret = -ENOMEM;
		goto err_request_input_dev;
	}

	data->input->name = pdata->button_name;
	__set_bit(EV_KEY, data->input->evbit);
	__set_bit(KEY_MEDIA, data->input->keybit);
	__set_bit(KEY_END, data->input->keybit);

	// TODO:
	#if 0
	__set_bit(KEY_PREVIOUSSONG, data->input->keybit);
	__set_bit(KEY_NEXTSONG, data->input->keybit);
	__set_bit(KEY_VOLUMEUP, data->input->keybit);
	__set_bit(KEY_VOLUMEDOWN, data->input->keybit);
	#endif

	ret = input_register_device(data->input);
	if (ret < 0) {
		SWITCH_ERR("%s: register input device failed.\n", __func__);
		goto err_register_input_dev;
	}

	mutex_init(&data->lock);

	INIT_DELAYED_WORK(&data->detect_int_work, comip_switch_int_work);
	INIT_DELAYED_WORK(&data->button_work, comip_switch_button_work);
	INIT_DELAYED_WORK(&data->detect_mic_work, comip_switch_detect_mic_work);

	init_timer(&data->button_timer);
	data->button_timer.function = comip_switch_button_timer_irq;
	data->button_timer.data = (unsigned long)data;
	data->button_state = 0;
	data->button_valid = 0;
	data->call_state = VOICE_CALL_STOP;
	data->mic_state = 0;

	if (COMIP_SWITCH_IS_ACTIVE_LOW(pdata->detect_id))
		jack_active_level = 0;

	if (COMIP_SWITCH_IS_ACTIVE_LOW(pdata->button_id))
		button_active_level = 0;

	if (!switch_ops) {
		SWITCH_ERR("%s: switch ops is invalid.\n", __func__);
		goto err_hardware_init;
	}

	if (switch_dev_call(button_int_mask, 1)) {
		SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
		goto err_hardware_init;
	}

	data->hw_info.comip_switch_isr = comip_switch_isr;
	data->hw_info.irq_gpio = pdata->irq_gpio;
	if (switch_dev_call(hw_init, &data->hw_info)) {
		SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
		goto err_hardware_init;
	}

	if (switch_dev_call(jack_active_level_set, jack_active_level)) {
		SWITCH_ERR("%s: set jack level failed \n", __func__);
		goto err_hardware_init;
	}

	if (switch_dev_call(button_active_level_set, button_active_level)) {
		SWITCH_ERR("%s: set button level failed \n", __func__);
		goto err_hardware_init;
	}

	if (switch_dev_call(jack_int_mask, 0)) {
		SWITCH_ERR("%s: mask jack interrupt failed \n", __func__);
		goto err_hardware_init;
	}

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

static int __exit comip_switch_remove(struct platform_device *pdev)
{
	struct comip_switch_data *data = platform_get_drvdata(pdev);

	switch_dev_call(hw_deinit);
	switch_dev_call(jack_int_mask, 1);
	switch_dev_call(button_int_mask, 1);

	input_unregister_device(data->input);
	input_free_device(data->input);
	switch_dev_unregister(&data->sdev);
	kfree(data);
	platform_set_drvdata(pdev, NULL);
	switch_ctx = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int comip_switch_suspend(struct device *dev)
{
	#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_switch_data *data = platform_get_drvdata(pdev);
	int ret;

	SWITCH_DBG("comip_switch_suspend! \n");
	#endif
	#if 0
	if(data->call_state == VOICE_CALL_START)
		return 0;

	//disable_irq(data->irq);
	ret = switch_dev_call(jack_int_mask, 1);
	if (ret) {
		SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
		return ret;
	}

	ret = switch_dev_call(button_int_mask, 1);
	if (ret) {
		SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
		return ret;
	}

	cancel_delayed_work_sync(&data->detect_int_work);
	cancel_delayed_work_sync(&data->detect_mic_work);
	cancel_delayed_work_sync(&data->button_work);

	if (data->detect_state == 1 && data->mic_state == 1) {
		data->button_valid = 0;
		ret = switch_dev_call(button_int_mask, 1);
		if (ret) {
			SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
			return ret;
		}
		ret = switch_dev_call(button_enable, 0);
		if (ret) {
			SWITCH_ERR("%s: disable button failed \n", __func__);
			return ret;
		}
		if (data->call_state == VOICE_CALL_STOP) {
			ret = comip_switch_button_power(0);
			if (ret)
				return ret;
		}
	}
	#endif
	return 0;
}

static int comip_switch_resume(struct device *dev)
{
	#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_switch_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	SWITCH_DBG("comip_switch_resume! \n");
	#endif
	#if 0
	//enable_irq(data->irq);
	ret = switch_dev_call(jack_int_mask, 0);
	if (ret) {
		SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
		return ret;
	}

	ret = switch_dev_call(button_int_mask, 0);
	if (ret) {
		SWITCH_ERR("%s: mask button interrupt failed \n", __func__);
		return ret;
	}

	if (data->detect_state == 1 && data->mic_state == 1) {
		ret = comip_switch_button_power(1);
		if (ret)
			return ret;
		schedule_delayed_work(&data->button_work, COMIP_SWITCH_BUTTON_RESUME_TIME);
	}
	#endif
	return 0;
}

static struct dev_pm_ops comip_switch_pm_ops = {
	.suspend = comip_switch_suspend,
	.resume = comip_switch_resume,
};
#endif

static struct platform_driver comip_switch_driver = {
	.probe	= comip_switch_probe,
	.remove = __exit_p(comip_switch_remove),
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
	register_call_state_notifier(&switch_call_state_notifier_block);
	return platform_driver_register(&comip_switch_driver);
}

static void __exit comip_switch_exit(void)
{
	unregister_call_state_notifier(&switch_call_state_notifier_block);
	platform_driver_unregister(&comip_switch_driver);
}

late_initcall(comip_switch_init);
module_exit(comip_switch_exit);

MODULE_DESCRIPTION("COMIP Switch driver based on CODEC");
MODULE_LICENSE("GPL");
