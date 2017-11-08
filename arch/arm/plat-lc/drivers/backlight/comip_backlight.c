/* drivers/video/backlight/comip_bl.c
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
**	0.1.0		2011-02-17	wangkaihui	created
**	0.1.1		2011-02-28	zouqiao		updated
**
*/

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/leds.h>
#include <linux/fb.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <asm/system.h>
#include <plat/comip-backlight.h>
#include <plat/comip-pmic.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/irq.h>
#include <plat/mfp.h>
#include <plat/comipfb.h>

#define DRIVER_NAME "comip-backlight"
#define ANDROID_LCD_BL_DEV_NAME "lcd-backlight"
#define ANDROID_KEY_BL_DEV_NAME "key-pad"

extern int comipfb_set_backlight(enum led_brightness brightness);

static int bl_flag = 1;

struct comip_backlight_data {
	int gpio_en;
	int pulse_gpio;
	spinlock_t pulse_lock;
	enum led_brightness brightness;
	struct led_classdev cdev;
	struct led_classdev kdev;
	struct comip_backlight_platform_data *pdata;
	struct pwm_device	*pwm;
};

static void comip_bl_pulse_set(struct comip_backlight_data *data, int brightness)
{
	int pulse_cnt, real_cnt, range;
	int arr_size;
	unsigned long irqflags;
	static int last_cnt = 0;

	if (!brightness) {//backlight 0ff
		last_cnt = 0;
		gpio_direction_output(data->pulse_gpio, 0);
		mdelay(2);
		return;
	}

	/* find ranges */
	arr_size = data->pdata->range_cnt;
	for (range = arr_size - 1; (range > 0) && (brightness > data->pdata->ranges[range].bright); range--);
	if (range < 0) {
		printk(KERN_INFO "Can't set brightness:%d\n", brightness);
		return;
	}
	pulse_cnt = data->pdata->ranges[range].pulse_cnt;

	//do not reset counter inside
	if (pulse_cnt > last_cnt) {
		real_cnt = pulse_cnt - last_cnt;
		last_cnt = pulse_cnt;
	} else if (pulse_cnt < last_cnt) {
		real_cnt = arr_size - last_cnt + pulse_cnt;	//loop counter inside
		last_cnt = pulse_cnt;
	} else
		return;

	spin_lock_irqsave(&data->pulse_lock, irqflags);
	//gpio_direction_output(data->pulse_gpio, 0);
	//udelay(1000);//should 200us < delay < 2ms, counter reset
	while(real_cnt--) {
		gpio_direction_output(data->pulse_gpio, 0);
		udelay(1);
		gpio_direction_output(data->pulse_gpio, 1);
		udelay(1);
	};
	spin_unlock_irqrestore(&data->pulse_lock, irqflags);

}


static void comip_lcd_backlight_set(struct comip_backlight_data *data,
				 enum led_brightness brightness)
{
	unsigned int period_ns;
	unsigned int duty_ns;
	unsigned int duty_ns_min;
	unsigned int duty_ns_max;

	if (!is_lcm_avail())
		brightness = 0;

	if ((data->pdata != NULL)&&(data->pdata->ctrl_type == CTRL_PMU)) {
		if (brightness) {
			pmic_voltage_set(PMIC_POWER_LED, 0, (int)brightness * PMIC_POWER_VOLTAGE_MAX / LED_FULL);
			if (data->pdata->gpio_en >= 0)
				gpio_direction_output(data->gpio_en, 1);
			if (bl_flag == 1) {
				bl_flag = 0;
				printk(KERN_DEBUG "%s, set brightness %d \n", __func__, brightness);
			}
		} else {
			if (data->pdata->gpio_en >= 0)
				gpio_direction_output(data->gpio_en, 0);
			pmic_voltage_set(PMIC_POWER_LED, 0, PMIC_POWER_VOLTAGE_DISABLE);
			printk(KERN_DEBUG "%s, set brightness 0 \n", __func__);
			bl_flag = 1;
		}
	} else if ((data->pdata != NULL)&&(data->pdata->ctrl_type == CTRL_PWM)) {
		if (brightness && data->pdata->pwm_en) {
			duty_ns_min = data->pdata->pwm_ocpy_min * HZ_TO_NS(data->pdata->pwm_clk) / 100;
			duty_ns_max = data->pdata->pwm_ocpy_max * HZ_TO_NS(data->pdata->pwm_clk) / 100;
			period_ns = HZ_TO_NS(data->pdata->pwm_clk);
			duty_ns = (brightness * period_ns) / LED_FULL;
			if (duty_ns < duty_ns_min)
				duty_ns = duty_ns_min;
			else if (duty_ns > duty_ns_max)
				duty_ns = duty_ns_max;
			pwm_config(data->pwm, duty_ns, period_ns);
			pwm_enable(data->pwm);
			if (bl_flag == 1) {
				if (data->pdata->bl_control)
					data->pdata->bl_control(1);
				bl_flag = 0;
				printk(KERN_DEBUG "%s, set brightness %d \n", __func__, brightness);
			}
		} else {
			period_ns = HZ_TO_NS(100);
			duty_ns = 0;
			pwm_config(data->pwm, duty_ns, period_ns);
			pwm_enable(data->pwm);
			udelay(100);
			pwm_disable(data->pwm);
			if (data->pdata->bl_control)
				data->pdata->bl_control(0);
			printk(KERN_DEBUG "%s, set brightness 0 \n", __func__);
			bl_flag = 1;
		}
	} else if ((data->pdata != NULL)&&(data->pdata->ctrl_type == CTRL_LCDC)) {
		if (brightness) {
			comipfb_set_backlight(brightness);
			if (bl_flag == 1) {
				msleep(3);
				if (data->pdata->bl_control)
					data->pdata->bl_control(1);
				bl_flag = 0;
				printk(KERN_DEBUG "%s, set brightness %d \n", __func__, brightness);
			}
		}
		else{
			printk(KERN_DEBUG "%s, set brightness 0 \n", __func__);
			comipfb_set_backlight(0);
			if (data->pdata->bl_control)
				data->pdata->bl_control(0);
			bl_flag = 1;
		}
	} else if ((data->pdata != NULL)&&(data->pdata->ctrl_type == CTRL_PULSE)) {
		if (brightness) {
			comip_bl_pulse_set(data, brightness * 100 / LED_FULL);
			if (bl_flag == 1) {
				msleep(3);
				if (data->pdata->bl_control)
					data->pdata->bl_control(1);
				bl_flag = 0;
				printk(KERN_DEBUG "%s, set brightness %d \n", __func__, brightness);
			}
		} else {
			printk(KERN_DEBUG "%s, set brightness 0 \n", __func__);
			comip_bl_pulse_set(data, 0);
			if (data->pdata->bl_control)
				data->pdata->bl_control(0);
			bl_flag = 1;
		}
	}else if((data->pdata != NULL)&&(data->pdata->ctrl_type == CTRL_EXTERNAL)){
		if (data->pdata->bl_set_external)
			data->pdata->bl_set_external(brightness);

	}
	else{
		printk(KERN_DEBUG "%s: using fault type\n", __func__);
	}
}

static void comip_backlight_lcd(struct led_classdev *led_cdev,
				 enum led_brightness brightness)
{
	struct comip_backlight_data *data =
		container_of(led_cdev, struct comip_backlight_data, cdev);


	data->brightness = brightness;
	comip_lcd_backlight_set(data, brightness);
}

static void comip_backlight_keypad(struct led_classdev *led_kdev,
                                enum led_brightness brightness)
{
	struct comip_backlight_data *data =
		container_of(led_kdev, struct comip_backlight_data, kdev);

	if(brightness) {
		if(data->pdata->key_bl_control)
			data->pdata->key_bl_control(1);
	}
	else {
		if(data->pdata->key_bl_control)
			data->pdata->key_bl_control(0);
	}
}

static int comip_backlight_probe(struct platform_device *pdev)
{
	struct comip_backlight_platform_data *pdata = pdev->dev.platform_data;
	struct comip_backlight_data *data;
	int ret;
	printk(KERN_INFO"%s\n", __func__);
	data = kzalloc(sizeof(struct comip_backlight_data), GFP_KERNEL);
	if(!data) {
		dev_err(&pdev->dev, "Can't malloc memory\n");
		return -ENOMEM;
	}

	data->brightness = LED_HALF;
	data->pdata = pdata;
	if (pdata)
		data->gpio_en = pdata->gpio_en;
	else
		data->gpio_en = -1;

	if (data->gpio_en >= 0) {
		ret = gpio_request(data->gpio_en, DRIVER_NAME);
		if(ret < 0) {
			dev_err(&pdev->dev, "Can't request gpio\n");
			goto err_gpio_request;
		}
	}

	if (data->pdata->ctrl_type == CTRL_PULSE) {
		data->pulse_gpio = pdata->pulse_gpio;
		if (data->pulse_gpio < 0) {
			dev_err(&pdev->dev, "no gpio used for pulse ctl backlight\n");
			goto err_gpio_request;
		}
		spin_lock_init(&data->pulse_lock);

		ret = gpio_request(data->pulse_gpio, "pulse_gpio");
		if(ret < 0) {
			dev_err(&pdev->dev, "Can't request gpio\n");
			goto err_gpio_request;
		}
		gpio_direction_output(data->pulse_gpio, 0);
	}

	if(data->pdata->ctrl_type == CTRL_PWM) {
		data->pwm = pwm_request(data->pdata->pwm_id, "backlight");
		if (IS_ERR(data->pwm)) {
			dev_err(&pdev->dev, "unable to request PWM for backlight\n");
			ret = PTR_ERR(data->pwm);
			goto err_pwm;
		} else
			dev_dbg(&pdev->dev, "got pwm for backlight\n");

		if (data->pdata->pwm_ocpy_max == 0)
			data->pdata->pwm_ocpy_max = 99;

		if (data->pdata->pwm_ocpy_min < 0 || data->pdata->pwm_ocpy_max > 99 ||
				data->pdata->pwm_ocpy_max < data->pdata->pwm_ocpy_min) {
			dev_err(&pdev->dev, "pwm ocpy min or max is invalid\n");
			ret = -EINVAL;
			goto err_pwm;
		}
	}
	data->cdev.name = ANDROID_LCD_BL_DEV_NAME;
	data->cdev.flags = 0;
	data->cdev.brightness = data->brightness;
	data->cdev.brightness_set = comip_backlight_lcd;
	ret = led_classdev_register(&pdev->dev, &data->cdev);
	if (ret) {
		dev_err(&pdev->dev, "Can't register led class device\n");
		goto err_led_classdev_register;
	}

	data->kdev.name = ANDROID_KEY_BL_DEV_NAME;
	data->kdev.flags = 0;
	data->kdev.brightness = 0;
	data->kdev.brightness_set = comip_backlight_keypad;
	ret = led_classdev_register(&pdev->dev, &data->kdev);
	if (ret) {
		dev_err(&pdev->dev, "Can't register led class device\n");
		goto err_led_classdev_register;
	}

	platform_set_drvdata(pdev, data);
	comip_lcd_backlight_set(data, data->brightness);

	return 0;

err_led_classdev_register:
	if (data->gpio_en >= 0)
		gpio_free(data->gpio_en);
	if (data->pulse_gpio >= 0)
		gpio_free(data->pulse_gpio);
err_pwm:
err_gpio_request:
	kfree(data);
	return ret;
}

static int comip_backlight_remove(struct platform_device *pdev)
{
	struct comip_backlight_data *data = platform_get_drvdata(pdev);

	led_classdev_unregister(&data->cdev);
	led_classdev_unregister(&data->kdev);
	comip_lcd_backlight_set(data, LED_OFF);
	if (data->gpio_en >= 0)
		gpio_free(data->gpio_en);
	if (data->pulse_gpio >= 0)
		gpio_free(data->pulse_gpio);
	kfree(data);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void comip_backlight_shutdown(struct platform_device *pdev)
{
	struct comip_backlight_data *data = platform_get_drvdata(pdev);

	printk(KERN_DEBUG "%s\n", __func__);
	led_classdev_unregister(&data->cdev);
	led_classdev_unregister(&data->kdev);
	comip_lcd_backlight_set(data, LED_OFF);
	if (data->gpio_en >= 0)
		gpio_free(data->gpio_en);
	if (data->pulse_gpio >= 0)
		gpio_free(data->pulse_gpio);
	kfree(data);
	platform_set_drvdata(pdev, NULL);
}

static struct platform_driver comip_backlight_driver = {
	.probe		= comip_backlight_probe,
	.remove		= comip_backlight_remove,
	.shutdown	= comip_backlight_shutdown,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init comip_backlight_init(void)
{
	return platform_driver_register(&comip_backlight_driver);
}

static void __exit comip_backlight_exit(void)
{
	platform_driver_unregister(&comip_backlight_driver);
}

module_init(comip_backlight_init);
module_exit(comip_backlight_exit);

MODULE_AUTHOR("wangkaihui <wangkaihui@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP PMIC LED driver");
MODULE_LICENSE("GPL");
