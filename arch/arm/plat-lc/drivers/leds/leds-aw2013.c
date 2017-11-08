/*
 * Copyright (C) 2014 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * PURPOSE: This files contains aw2013 RGB led driver.
 *
 * CHANGE HISTORY:
 *
 * Version	Date		Author		Description
 * 1.0.0	2013-3-30	taoran	 	created
 * 2.0.0	2014-4-18	tuzhiqiang	rework
 *
 */

#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <plat/aw2013.h>

enum led_colors {
	RED = 0,
	GREEN,
	BLUE,
	LED_NUM,
};

struct aw2013_led {
	struct led_classdev cdev_red;
	struct led_classdev cdev_green;
	struct led_classdev cdev_blue;
	u8 sel[LED_NUM];
	u8 brightness[LED_NUM];
	struct work_struct work;
	struct workqueue_struct *wq;
	u8 state;
	struct i2c_client *client;
};

static const unsigned char blink_time[10][3] = {
	/* Hold time,	fall time,	delay time. */
	{0x3,		0x3, 		0x3},	/* 1s */
	{0x3,		0x4,		0x3},	/* 2s */
	{0x3,		0x4,		0x4},	/* 3s */
	{0x3,		0x5,		0x0},	/* 4s */
	{0x3,		0x5,		0x4},	/* 5s */
	{0x3,		0x5,		0x5},	/* 6s */
	{0x3,		0x5,		0x5},	/* 7s */
	{0x3,		0x5,		0x6},	/* 8s */
	{0x3,		0x6,		0x4},	/* 9s */
	{0x3,		0x6,		0x5},	/* 10s */
};

static int aw2013_power_onoff(struct aw2013_led *led, u8 on)
{
	/* pull down gpio to turn off ic, if only turn off reg, there's 0.5mA base */
	i2c_smbus_write_byte_data(led->client, LEDGCR, on);
	dev_dbg(&led->client->dev, "power %s\n", on ? "on" : "off");

	return 0;
}

static inline void check_for_power_off(struct aw2013_led *led)
{
	u8 i;
	u8 val = 0;

	/* check if turn off ic */
	for (i = 0; i < LED_NUM; i++)
		val |= led->brightness[i];

	if (!val) {
		aw2013_power_onoff(led, 0);
		led->state = 0;
	}
}

static inline void check_for_power_on(struct aw2013_led *led)
{
	if (!led->state) {
		aw2013_power_onoff(led, 1);
		led->state = 1;
	}
}

static void aw2013_set_brightness(struct aw2013_led *led,
		unsigned int index)
{
	unsigned long blink_on;
	unsigned long blink_off;
	struct led_classdev *cdev;

	switch (index) {
	case RED:
		cdev = &led->cdev_red;
		break;
	case GREEN:
		cdev = &led->cdev_green;
		break;
	case BLUE:
		cdev = &led->cdev_blue;
		break;
	default:
		dev_err(&led->client->dev, "faile to set brightness, invalid color\n");
		return;
	}

	if (led->brightness[index] > 0) {
		check_for_power_on(led);
		blink_on = cdev->blink_delay_on;
		blink_off = cdev->blink_delay_off;

		if (blink_off == 0)
			i2c_smbus_write_byte_data(led->client, LED_CTRL(index), 0x1);
		else {
			i2c_smbus_write_byte_data(led->client, LED_CTRL(index), 0x51);
			i2c_smbus_write_byte_data(led->client, LED_T0(index), 0x3);
			i2c_smbus_write_byte_data(led->client, LED_T1(index),
				blink_time[(blink_off / 1000) - 1][1] << 4);
			i2c_smbus_write_byte_data(led->client, LED_T2(index),
				blink_time[(blink_off / 1000) - 1][2] << 4);
		}

		i2c_smbus_write_byte_data(led->client, LED_LEVEL(index),
			led->brightness[index]);
		i2c_smbus_write_byte_data(led->client, LEDSEL,
			i2c_smbus_read_byte_data(led->client, LEDSEL) | LED_SWITCH(index));
	} else {
		led->brightness[index] = 0;
		cdev->blink_delay_on = 0;
		cdev->blink_delay_off = 0;
		i2c_smbus_write_byte_data(led->client, LEDSEL,
			i2c_smbus_read_byte_data(led->client, LEDSEL) & (~(LED_SWITCH(index))));
		check_for_power_off(led);
	}
}

#define AW2013_SYSFS_DEFINE(name, color) \
static ssize_t aw2013_##name##_delay_show(struct device *dev, \
	struct device_attribute *attr, char *buf) \
{ \
	struct aw2013_led *led = i2c_get_clientdata(to_i2c_client(dev->parent)); \
	return sprintf(buf, "%lu,%lu\n", led->cdev_##name.blink_delay_on, \
		led->cdev_##name.blink_delay_off ); \
} \
static ssize_t aw2013_##name##_delay_store(struct device *dev, \
	struct device_attribute *attr, const char *buf, size_t count) \
{ \
	struct aw2013_led *led = i2c_get_clientdata(to_i2c_client(dev->parent)); \
	unsigned int val_on, val_off; \
	sscanf(buf, "%d,%d", &val_on, &val_off); \
	led->cdev_##name.blink_delay_on = val_on; \
	led->cdev_##name.blink_delay_off = val_off; \
	return count; \
} \
static ssize_t aw2013_##name##_bri_reg_show(struct device *dev, \
	struct device_attribute *attr, char *buf) \
{ \
	struct aw2013_led *led = i2c_get_clientdata(to_i2c_client(dev->parent)); \
	int ret = i2c_smbus_read_byte_data(led->client, LED_LEVEL(color)); \
	return sprintf(buf, "%d\n", ret); \
} \
static ssize_t aw2013_##name##_bri_reg_store(struct device *dev, \
	struct device_attribute *attr, const char *buf, size_t count) \
{ \
	struct aw2013_led *led = i2c_get_clientdata(to_i2c_client(dev->parent)); \
	unsigned long data; \
	int ret; \
	ret = strict_strtoul(buf, 10, &data); \
	if (ret) \
		return ret; \
	led->brightness[color] = (unsigned int)data; \
	led->sel[color] = 1; \
	queue_work(led->wq, &led->work); \
	return count; \
} \
static DEVICE_ATTR(name##_delay, 0660, aw2013_##name##_delay_show, \
	aw2013_##name##_delay_store); \
static DEVICE_ATTR(name##_bri_reg, 0664, aw2013_##name##_bri_reg_show, \
	aw2013_##name##_bri_reg_store); \
static struct device_attribute *aw2013_##name##_attributes[] = { \
	&dev_attr_##name##_delay, \
	&dev_attr_##name##_bri_reg, \
};

AW2013_SYSFS_DEFINE(red, RED)
AW2013_SYSFS_DEFINE(green, GREEN)
AW2013_SYSFS_DEFINE(blue, BLUE)

#define AW2013_SET_FUNC_DEFINE(name, color) \
static void aw2013_set_##name##_brightness(struct led_classdev *led_cdev, \
					enum led_brightness value) \
{ \
	struct aw2013_led *led = \
		container_of(led_cdev, struct aw2013_led, cdev_##name); \
	led->brightness[color] = (unsigned int)value; \
	led->sel[color] = 1; \
	if (led->brightness[color] > 0xff) \
		led->brightness[color] = 255; \
	queue_work(led->wq, &led->work); \
} \
static int aw2013_set_##name##_blink(struct led_classdev *led_cdev,	\
		unsigned long *delay_on, unsigned long *delay_off)	\
{ \
	struct aw2013_led *led = \
		container_of(led_cdev, struct aw2013_led, cdev_##name); \
	led->cdev_##name.blink_delay_on = *delay_on; \
	led->cdev_##name.blink_delay_off = *delay_off; \
	return 0; \
}

AW2013_SET_FUNC_DEFINE(red, RED)
AW2013_SET_FUNC_DEFINE(green, GREEN)
AW2013_SET_FUNC_DEFINE(blue, BLUE)

static void aw2013_work(struct work_struct *work)
{
	struct aw2013_led *led = container_of(work, struct aw2013_led, work);
	u32 index;

	for (index = 0; index < LED_NUM; index++) {
		if (led->sel[index]) {
			led->sel[index] = 0;
			aw2013_set_brightness(led, index);
		}
	}
}

static int aw2013_classdev_register(struct aw2013_led *led)
{
	int i;
	int ret;

	led->cdev_red.name = "red";
	led->cdev_red.brightness = LED_OFF;
	led->cdev_red.brightness_set = aw2013_set_red_brightness;
	led->cdev_red.blink_delay_on = 0;
	led->cdev_red.blink_delay_off = 0;
	led->cdev_red.blink_set = aw2013_set_red_blink;
	ret = led_classdev_register(&led->client->dev, &led->cdev_red);
	if (ret) {
		dev_err(&led->client->dev, "failed to register red led cdev!\n");
		goto err_register_red_cdev;
	}
	for (i = 0; i < ARRAY_SIZE(aw2013_red_attributes); i++) {
		ret = device_create_file(led->cdev_red.dev,
						aw2013_red_attributes[i]);
		if (ret) {
			dev_err(&led->client->dev, "failed to create sysfs file %s\n",
					aw2013_red_attributes[i]->attr.name);
			goto err_create_red_sysfs_files;
		}
	}

	led->cdev_green.name = "green";
	led->cdev_green.brightness = LED_OFF;
	led->cdev_green.brightness_set = aw2013_set_green_brightness;
	led->cdev_green.blink_delay_on = 0;
	led->cdev_green.blink_delay_off = 0;
	led->cdev_green.blink_set = aw2013_set_green_blink;
	ret = led_classdev_register(&led->client->dev, &led->cdev_green);
	if (ret) {
		dev_err(&led->client->dev, "failed to register green led cdev!\n");
		goto err_register_green_cdev;
	}
	for (i = 0; i < ARRAY_SIZE(aw2013_green_attributes); i++) {
		ret = device_create_file(led->cdev_green.dev,
						aw2013_green_attributes[i]);
		if (ret) {
			dev_err(&led->client->dev, "failed to create sysfs file %s\n",
					aw2013_green_attributes[i]->attr.name);
			goto err_create_green_sysfs_files;
		}
	}

	led->cdev_blue.name = "blue";
	led->cdev_blue.brightness = LED_OFF;
	led->cdev_blue.brightness_set = aw2013_set_blue_brightness;
	led->cdev_blue.blink_delay_on = 0;
	led->cdev_blue.blink_delay_off = 0;
	led->cdev_blue.blink_set = aw2013_set_blue_blink;
	ret = led_classdev_register(&led->client->dev, &led->cdev_blue);
	if (ret) {
		dev_err(&led->client->dev, "failed to register blue led cdev!\n");
		goto err_register_blue_cdev;
	}
	for (i = 0; i < ARRAY_SIZE(aw2013_blue_attributes); i++) {
		ret = device_create_file(led->cdev_blue.dev,
						aw2013_blue_attributes[i]);
		if (ret) {
			dev_err(&led->client->dev, "failed to create sysfs file %s\n",
					aw2013_blue_attributes[i]->attr.name);
			goto err_create_blue_sysfs_files;
		}
	}

	return 0;

err_create_blue_sysfs_files:
	for (i = 0; i < ARRAY_SIZE(aw2013_blue_attributes); i++)
		device_remove_file(led->cdev_blue.dev, aw2013_blue_attributes[i]);
	led_classdev_unregister(&led->cdev_blue);
err_register_blue_cdev:
err_create_green_sysfs_files:
	for (i = 0; i < ARRAY_SIZE(aw2013_green_attributes); i++)
		device_remove_file(led->cdev_green.dev, aw2013_green_attributes[i]);
	led_classdev_unregister(&led->cdev_green);
err_register_green_cdev:
err_create_red_sysfs_files:
	for (i = 0; i < ARRAY_SIZE(aw2013_red_attributes); i++)
		device_remove_file(led->cdev_red.dev, aw2013_red_attributes[i]);
	led_classdev_unregister(&led->cdev_red);
err_register_red_cdev:

	return ret;
}

static void aw2013_classdev_unregister(struct aw2013_led *led)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aw2013_blue_attributes); i++)
		device_remove_file(led->cdev_blue.dev, aw2013_blue_attributes[i]);
	led_classdev_unregister(&led->cdev_blue);

	for (i = 0; i < ARRAY_SIZE(aw2013_green_attributes); i++)
		device_remove_file(led->cdev_green.dev, aw2013_green_attributes[i]);
	led_classdev_unregister(&led->cdev_green);

	for (i = 0; i < ARRAY_SIZE(aw2013_red_attributes); i++)
		device_remove_file(led->cdev_red.dev, aw2013_red_attributes[i]);
	led_classdev_unregister(&led->cdev_red);
}

static int aw2013_probe(struct i2c_client *client,
		const struct i2c_device_id *devid)
{
	struct aw2013_led *led;
	int ret;

	dev_info(&client->dev, "aw2013 probe\n");

	led = kzalloc(sizeof(struct aw2013_led), GFP_KERNEL);
	if (!led) {
		dev_err(&client->dev, "kzalloc failed !\n");
		return -ENOMEM;
	}

	led->client = client;
	i2c_set_clientdata(client, led);

	ret = i2c_smbus_read_byte_data(client, LEDGCR);
	if (ret) {
		dev_err(&client->dev, "failed to detect aw2013 !\n");
		ret = -ENODEV;
		goto err_detect;
	}

	ret = aw2013_classdev_register(led);
	if (ret) {
		dev_err(&client->dev, "failed to register cdev !\n");
		goto err_cdev_register;
	}

	INIT_WORK(&led->work, aw2013_work);

	led->wq = create_singlethread_workqueue(dev_name(&client->dev));
	if (!led->wq) {
		dev_err(&client->dev, "failed to create workqueue !\n");
		ret = -ESRCH;
		goto err_create_wq;
	}

	aw2013_power_onoff(led, 0);

	return 0;

err_create_wq:
	aw2013_classdev_unregister(led);
err_cdev_register:
err_detect:
	kfree(led);

	return ret;
}

static int aw2013_remove(struct i2c_client *client)
{
	struct aw2013_led *led = i2c_get_clientdata(client);

	aw2013_power_onoff(led, 0);

	destroy_workqueue(led->wq);

	aw2013_classdev_unregister(led);

	kfree(led);

	return 0;
}

static const struct i2c_device_id aw2013_id[] = {
	{"aw2013", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw2013_id);

static struct i2c_driver aw2013_driver = {
	.probe = aw2013_probe,
	.remove = aw2013_remove,
	.id_table = aw2013_id,
	.driver = {
		.name = "aw2013",
		.owner = THIS_MODULE,
	},
};

static int __init aw2013_init(void)
{
	return i2c_add_driver(&aw2013_driver);
}

static void __exit aw2013_exit(void)
{
	i2c_del_driver(&aw2013_driver);
}

module_init(aw2013_init);
module_exit(aw2013_exit);

MODULE_AUTHOR("taoran@leadcoretch.com");
MODULE_DESCRIPTION("aw2013 RGB led driver");
MODULE_LICENSE("GPL");
