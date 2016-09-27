/*
 * Driver for FSA8108 on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/notifier.h>

#include <plat/comip-snd-notifier.h>

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <plat/mfp.h>

#define FSA8108_SLAVE_ADDRESS    0x23
#define FSA8108_VERSION_REG		0x01
#define FSA8108_IRQ_DEBOUNCE_TIME 	(HZ / 100)

#define	COMIP_GPIO_KEY_HOOK	MFP_PIN_GPIO(226)
#define	HOOK_GPIO		mfp_to_gpio(MFP_PIN_GPIO(226))
#define	FSA8108_INT_GPIO		mfp_to_gpio(MFP_PIN_GPIO(135))
#define	EARPHONE_GPIO	mfp_to_gpio(MFP_PIN_GPIO(144))
#define	HANDPHONE_GPIO	mfp_to_gpio(MFP_PIN_GPIO(176))
#define	SPEAKER_GPIO	mfp_to_gpio(MFP_PIN_GPIO(75))

#define EAR_PHONE		0x01
#define HAND_PHONE		0x02
#define SPEAKER_PHONE	0x03

struct gpio_keys_button {
	/* Configuration parameters */
	unsigned int code;	/* input event code (KEY_*, SW_*) */
	int gpio;		/* -1 if this key does not support gpio */
	int active_low;
	const char *desc;
	unsigned int type;	/* input event type (EV_KEY, EV_SW, EV_ABS) */
	int wakeup;		/* configure the button as a wake-up source */
	int debounce_interval;	/* debounce ticks interval in msecs */
	bool can_disable;
	int value;		/* axis value for EV_ABS */
	unsigned int irq;	/* Irq number in case of interrupt keys */
};

struct fsa8108_chip	{
	struct input_dev *input;
	struct gpio_keys_button button;
	int irq_int;
	int irq_hook;
	int	handphone_gpio;
	int earphone_gpio;
	int	speaker_gpio;
	struct delayed_work	irq_int_work;
	struct delayed_work	irq_hook_work;
	struct mutex mutex;
};

static struct gpio_keys_button button = {
	.code		= KEY_FN_F12,
	.gpio		= COMIP_GPIO_KEY_HOOK,
	.desc		= "Hook Button",
	.active_low	= 1,
	.debounce_interval = 30,
};

static	struct i2c_client *fsa8108_client;

int fsa8108_reg_read(u8 reg, u8 *value)
{
	int ret;

	if (!fsa8108_client)
		return -1;

	ret = i2c_smbus_read_byte_data(fsa8108_client, reg);
	if(ret < 0)
		return ret;

	*value = (u8)ret;

	return 0;
}

int fsa8108_reg_write(u8 reg, u8 value)
{
	int ret;

	if (!fsa8108_client)
		return -1;

	ret = i2c_smbus_write_byte_data(fsa8108_client, reg, value);
	if(ret > 0)
		ret = 0;

	return ret;
}

int fsa8108_read(u8 reg)
{
	u8 ret;
	fsa8108_reg_read(reg, &ret);

	return ret;
}
EXPORT_SYMBOL(fsa8108_read);

int fsa8108_chip_version_get(void)
{
	u8 chip_version;

	fsa8108_reg_read(FSA8108_VERSION_REG, &chip_version);

	return	chip_version;
}

static int audio_channel_switch(struct fsa8108_chip *fsa8108, int enable)
{
	switch(enable) {
	case EAR_PHONE:
		gpio_direction_output(fsa8108->handphone_gpio, 0);
		gpio_direction_output(fsa8108->earphone_gpio, 1);
		gpio_direction_output(fsa8108->speaker_gpio, 0);
		break;
	case HAND_PHONE:
		gpio_direction_output(fsa8108->handphone_gpio, 1);
		gpio_direction_output(fsa8108->earphone_gpio, 0);
		gpio_direction_output(fsa8108->speaker_gpio, 0);
		break;
	case SPEAKER_PHONE:
		gpio_direction_output(fsa8108->handphone_gpio, 0);
		gpio_direction_output(fsa8108->earphone_gpio, 0);
		gpio_direction_output(fsa8108->speaker_gpio, 1);
		break;
	default:
		gpio_direction_output(fsa8108->handphone_gpio, 0);
		gpio_direction_output(fsa8108->earphone_gpio, 0);
		gpio_direction_output(fsa8108->speaker_gpio, 0);
		break;
	}
	return 0;
}

static irqreturn_t fsa8108_irq_hook_handle(int irq, void* dev)
{
	struct fsa8108_chip *fsa8108 = dev;

	disable_irq_nosync(fsa8108->irq_hook);
	schedule_delayed_work(&fsa8108->irq_hook_work, FSA8108_IRQ_DEBOUNCE_TIME);

	return IRQ_HANDLED;
}

static void fsa8108_irq_hook_work_handle(struct work_struct *work)
{
	struct fsa8108_chip *fsa8108 = container_of(work,
				struct fsa8108_chip, irq_hook_work.work);
	int state = (gpio_get_value_cansleep(fsa8108->button.gpio) ? 1 : 0) ^ fsa8108->button.active_low;
	
	if(state)	
	{
		audio_channel_switch(fsa8108, SPEAKER_PHONE);
		input_event(fsa8108->input, EV_KEY, fsa8108->button.code, state);
	}
	else
	{
		audio_channel_switch(fsa8108, HAND_PHONE);
		input_event(fsa8108->input, EV_KEY, fsa8108->button.code, state);
	}
	
	input_sync(fsa8108->input);
	dev_info(&fsa8108_client->dev, " %s: hook state %d .\n",__func__,state);
	enable_irq(fsa8108->irq_hook);
}

/*
status = fsa8108_read(0x02);
	dev_info(codec->dev," status = 0x%x\n", status);
	status2 = fsa8108_read(0x03);
	dev_info(codec->dev," status2 = 0x%x\n", status2);
	//jack out
	if(status&0x04)	{
		if (lc1160->ear_switch && lc1160->serial_port_state)
			lc1160->ear_switch(0);
		snd_soc_jack_report(lc1160->jack, 0, SND_JACK_HEADSET);
		lc1160->jack_mic = false;
		dev_info(codec->dev,"jack removed !!!");
	}
	//jack in
	if(status&0x03) {
		msleep(500);
		lc1160_micd_ctl(codec,true);
		lc1160_micd(codec);
		goto out;
	}

	if(!lc1160->jack_mic) {
		lc1160_micd_ctl(codec,false);
		goto out;
	}
	
	if(status&0x08)
		report |= SND_JACK_BTN_0;
	else if(status2&0x01)
		report |= SND_JACK_BTN_1;
	else if(status2&0x08)
		report |= SND_JACK_BTN_2;
*/


static int fsa8108_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fsa8108_chip *fsa8108;
	struct input_dev *input;
	int chip_version;
	int ret;

	fsa8108 = kzalloc(sizeof(struct fsa8108_chip), GFP_KERNEL);
	input = input_allocate_device();
	if (!fsa8108 || !input)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}

	fsa8108_client = client;
	fsa8108->input = input;
	fsa8108->irq_int = gpio_to_irq(FSA8108_INT_GPIO);
	fsa8108->irq_hook = gpio_to_irq(HOOK_GPIO);
	fsa8108->handphone_gpio = HANDPHONE_GPIO;
	fsa8108->earphone_gpio = EARPHONE_GPIO;
	fsa8108->speaker_gpio = SPEAKER_GPIO;
	fsa8108->button = button;
	input->name = "hook-key";

	input_set_capability(input, EV_KEY, button.code);
	ret = input_register_device(input);
	if (ret) {
		dev_err(&fsa8108_client->dev, "Unable to register input device, error: %d\n",ret);
		goto error;
	}

	gpio_request(fsa8108->handphone_gpio, "handphone_gpio");
	gpio_request(fsa8108->earphone_gpio, "earphone_gpio");
	gpio_request(fsa8108->speaker_gpio, "speaker_gpio");

	chip_version = fsa8108_chip_version_get();
	printk(KERN_INFO"%s: chip version 0x%x\n",__func__,chip_version);
	
	//zhangchg
	ret = gpio_request(HOOK_GPIO, " Hook Irq");
	if (ret < 0) {
		dev_err(&fsa8108_client->dev, " Failed to request Hook GPIO.\n");
		return ret;
	}
	ret = gpio_direction_input(HOOK_GPIO);
	if (ret) {
		dev_err(&fsa8108_client->dev, " Failed to detect Hook GPIO input.\n");
		goto error;
	}

	/* Request irq. */
	ret = request_irq(fsa8108->irq_hook, fsa8108_irq_hook_handle,
			  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "Hook Irq", fsa8108);
	if (ret) {
		dev_err(&fsa8108_client->dev, " IRQ already in use.\n");
		goto error;
	}

	disable_irq(fsa8108->irq_hook);
	INIT_DELAYED_WORK(&fsa8108->irq_hook_work, fsa8108_irq_hook_work_handle);
	irq_set_irq_wake(fsa8108->irq_hook, 1);
	enable_irq(fsa8108->irq_hook);

	/* audio default set */
	audio_channel_switch(fsa8108, SPEAKER_PHONE);

	return 0;
error:
	gpio_free(HOOK_GPIO);
exit_check_functionality_failed:
	kfree(fsa8108);
	return ret;
}

static int __exit fsa8108_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id fsa8108_i2c_id[] = {
	{ "fsa8108", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fsa8108_i2c_id);

static struct i2c_driver fsa8108_driver = {
	.driver = {
		.name = "fsa8108",
		.owner = THIS_MODULE,
	},
	.probe = fsa8108_probe,
	.remove = __exit_p(fsa8108_remove),
	.id_table = fsa8108_i2c_id,
};


static int __init fsa8018_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&fsa8108_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register fsa8108 i2s driver: %d\n",ret);
	}
	return ret;
}
module_init(fsa8018_init);

static void __exit fsa8108_exit(void)
{
	i2c_del_driver(&fsa8108_driver);
}
module_exit(fsa8108_exit);

MODULE_DESCRIPTION("FSA8108driver");
MODULE_LICENSE("GPL");

