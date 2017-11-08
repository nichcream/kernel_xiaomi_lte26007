/* 
**Driver for led on pmu sink mode
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2009-2018	LeadCoreTech Corp.
**
**	PURPOSE:			This files contains the driver of LCD backlight controller.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**
*/

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/string.h>
#include <asm/system.h>
#include <plat/comip-pmic.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <plat/comip-leds-sink.h>


struct sink_led_data {
	struct led_classdev cdev;
	unsigned sink_no;
	struct work_struct work;
	void (*platform_sink_brightness_set)(struct led_classdev *led_cdev,
					  enum led_brightness brightness);
};


void comip_led_sink_lcd(struct led_classdev *cdev_lcd,
				 enum led_brightness brightness)
{
	if(brightness) {
		pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_LCD, PMIC_POWER_VOLTAGE_ENABLE);
	}
	else {
             pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_LCD, PMIC_POWER_VOLTAGE_DISABLE);
	}

}
EXPORT_SYMBOL_GPL(comip_led_sink_lcd);

void comip_led_sink_vibratoren(struct led_classdev *cdev_vibratoren,
                                enum led_brightness brightness)
{
	if(brightness) {
		pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_KEYPAD, PMIC_POWER_VOLTAGE_ENABLE);
	}
	else {
             pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_KEYPAD, PMIC_POWER_VOLTAGE_DISABLE);
	}
}
EXPORT_SYMBOL_GPL(comip_led_sink_vibratoren);

void comip_led_sink_flash(struct led_classdev *cdev_flash,
                                enum led_brightness brightness)
{
	if(brightness) {
		pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_FLASH, PMIC_POWER_VOLTAGE_ENABLE);
	}
	else {
             pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_FLASH, PMIC_POWER_VOLTAGE_DISABLE);
	}
}
EXPORT_SYMBOL_GPL(comip_led_sink_flash);

struct sink_leds_priv {
	int num_leds;
	struct sink_led_data leds[];
};

static inline int sizeof_sink_leds_priv(int num_leds)
{
	return sizeof(struct sink_leds_priv) +
		(sizeof(struct sink_led_data) * num_leds);
}

static int create_sink_led(const struct sink_led *template,
	struct sink_led_data *led_dat, struct device *parent)
{
	int ret, state;

	led_dat->cdev.name = template->name;
	led_dat->sink_no = template->sink_no;
	led_dat->cdev.brightness_set = template->sink_brightness_set;
	led_dat->cdev.brightness = template->default_state ? LED_FULL : LED_OFF;

	ret = led_classdev_register(parent, &led_dat->cdev);
	if (ret < 0)
		return ret;

	return 0;
}
static void delete_sink_led(struct sink_led_data *led)
{
	led_classdev_unregister(&led->cdev);
}

static int comip_led_sink_probe(struct platform_device *pdev)
{
	struct sink_led_platform_data *pdata = pdev->dev.platform_data;
	struct sink_leds_priv *priv;
	int i, ret = 0;

	if (pdata && pdata->num_leds) {
		priv = devm_kzalloc(&pdev->dev,
				sizeof_sink_leds_priv(pdata->num_leds),
					GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		priv->num_leds = pdata->num_leds;
		for (i = 0; i < priv->num_leds; i++) {
			ret = create_sink_led(&pdata->leds[i],
					      &priv->leds[i],&pdev->dev);
			if (ret < 0) {
				/* On failure: unwind the led creations */
				for (i = i - 1; i >= 0; i--)
					delete_sink_led(&priv->leds[i]);
				return ret;
			}
		}
	} 

	platform_set_drvdata(pdev, priv);

	return 0;
}

static int comip_led_sink_remove(struct platform_device *pdev)
{
	struct sink_leds_priv *priv = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < priv->num_leds; i++)
		delete_sink_led(&priv->leds[i]);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver comip_led_sink_driver = {
	.probe		= comip_led_sink_probe,
	.remove		= comip_led_sink_remove,
	.driver		= {
		.name	= "comip-leds-sink",
		.owner	= THIS_MODULE,
	},
};

static int __init comip_led_sink_init(void)
{
	return platform_driver_register(&comip_led_sink_driver);
}

static void __exit comip_led_sink_exit(void)
{
	platform_driver_unregister(&comip_led_sink_driver);
}

module_init(comip_led_sink_init);
module_exit(comip_led_sink_exit);

MODULE_AUTHOR("zhaorui <zhaorui@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP PMU SINK LED driver");
MODULE_LICENSE("GPL");
