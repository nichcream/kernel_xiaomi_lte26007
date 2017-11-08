/*
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** Copyright (c) 2010-2019 LeadCoreTech Corp.
**
** PURPOSE: comip machine.
**
** CHANGE HISTORY:
**
** Version	Date		Author			Description
** 1.0.0	2013-03-21	liuyong			created
**
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/wlan_plat.h>
#include <linux/mpu.h>
#include <linux/bmc.h>
#include <linux/input/lis3dh.h>
#include <linux/akm8963.h>
#include <linux/cm36283.h>

#include <plat/ft5x06_ts.h>
#include <plat/i2c.h>
#include <plat/camera.h>
#include <plat/comipfb.h>
#include <plat/comip-backlight.h>
#include <plat/comip-thermal.h>
#include <plat/comip-pmic.h>
#include <plat/comip-battery.h>
#include <plat/lc1132_adc.h>
#include <plat/lc1132.h>
#include <plat/lc1132-pmic.h>

#include "board-ust802.h"
#include "board-common.c"

#if defined(CONFIG_LC1100H_MONITOR_BATTERY) || defined(CONFIG_LC1132_MONITOR_BATTERY)
static int batt_table[] = {
	/* adc code for temperature in degree C */
	2295, 2274, 2253, 2232, 2210, 2188, 2165, 2142, 2118, 2094,/* -20 ,-11 */
	2069, 2044, 2019, 1993, 1967, 1940, 1914, 1887, 1860, 1832,/* -10 ,-1 */
	1805, 1777, 1749, 1721, 1693, 1665, 1637, 1609, 1580, 1552,/* 00 - 09 */
	1524, 1496, 1468, 1440, 1410, 1384, 1357, 1329, 1302, 1275, /* 10 - 19 */
	1249, 1222, 1196, 1170, 1145, 1120, 1095, 1070, 1046, 1022,  /* 20 - 29 */
	998, 975, 952, 930, 908, 886, 865, 844, 823, 803,/* 30 - 39 */
	783, 764, 745, 727, 708, 691, 673, 656, 640, 623, /* 40 - 49 */
	607, 592, 577, 562, 548, 534, 520, 506, 493, 480, /* 50 - 59 */
	468, 456, 445, 433, 422, 411, 401, 391, 381, 371,/* 60 - 69 */
	361,/* 70 */
};

struct monitor_battery_platform_data bci_data ={
	.battery_tmp_tbl    = batt_table,
	.tblsize = ARRAY_SIZE(batt_table),
	.monitoring_interval    = 20,
	.max_charger_voltagemV  = 4200,//change CV to 4350 when use 4.35V battery
	.termination_currentmA  = 150,
	.usb_battery_currentmA  = 500,
	.ac_battery_currentmA   = 700,
	.rechg_voltagemV = 100,
	.temp_low_threshold = -10,/*bat limit charging temperature in degree C */
	.temp_zero_threshold = 0,
	.temp_cool_threshold = 10,
	.temp_warm_threshold = 45,
	.temp_high_threshold = 50,
};

static struct platform_device monitor_battery_dev = {
	.name = "monitor_battery",
	.id = -1,
	.dev = {
		.platform_data = &bci_data,
	}
};
#endif

#if defined(CONFIG_LC1100H_ADC) || defined(CONFIG_LC1132_ADC)
struct pmic_adc_platform_data lc_adc ={
	.irq_line = -1,
	.features = 0,
};

static struct platform_device adc_dev = {
	.name = "lc11xx_adc",
	.id = -1,
	.dev = {
		.platform_data = &lc_adc,
	}
};
#endif

#if defined(CONFIG_BACKLIGHT_COMIP)
static void comip_lcd_bl_control(int onoff)
{
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_LCD, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_LCD, PMIC_POWER_VOLTAGE_DISABLE);
	}
}

static void comip_key_bl_control(int onoff)
{
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_KEYPAD, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_KEYPAD, PMIC_POWER_VOLTAGE_DISABLE);
	}
}

static struct comip_backlight_platform_data comip_backlight_data = {
	.ctrl_type = CTRL_PWM,
	.gpio_en = -1,
	.pwm_en = 1,
	.pwm_id = 0,
	.pwm_clk = 32500,  //32.5KhZ
	.pwm_ocpy_min = 10,
	.bl_control = comip_lcd_bl_control,
	.key_bl_control = comip_key_bl_control,
};

static struct platform_device comip_backlight_device = {
	.name = "comip-backlight",
	.id = -1,
	.dev = {
		.platform_data = &comip_backlight_data,
	}
};
#endif

#if defined(CONFIG_THERMAL_COMIP)
static struct sample_rate_member sample_table[] = {
	{-100, 10, SAMPLE_RATE_SLOW, THERMAL_NORMAL},
	{10, 65, SAMPLE_RATE_NORMAL, THERMAL_NORMAL},
	{65, 70, SAMPLE_RATE_FAST, THERMAL_WARM},
	{70, 74, SAMPLE_RATE_FAST, THERMAL_HOT},
	{74, 76, SAMPLE_RATE_EMERGENT, THERMAL_TORRID},
	{76, 78, SAMPLE_RATE_EMERGENT, THERMAL_CRITICAL},
	{78, 80, SAMPLE_RATE_EMERGENT, THERMAL_FATAL},
	{80, 100, SAMPLE_RATE_EMERGENT, THERMAL_ERROR},
	{101, 1000, SAMPLE_RATE_ERROR, THERMAL_END},
};

static struct comip_thermal_platform_data comip_thermal_info = {
	.channel = DBB_TEMP_ADC_CHNL,
	.sample_table_size = ARRAY_SIZE(sample_table),
	.sample_table = sample_table,
};

static struct platform_device comip_thermal_device = {
       .name = "comip-thermal",
       .id = -1,
       .dev = {
       .platform_data = &comip_thermal_info,
	}
};
#endif

static struct platform_device *devices[] __initdata = {
#if defined(CONFIG_BACKLIGHT_COMIP)
	&comip_backlight_device,
#endif
#if defined(CONFIG_THERMAL_COMIP)
	&comip_thermal_device,
#endif
#if defined(CONFIG_LC1100H_ADC) || defined(CONFIG_LC1132_ADC)
	&adc_dev,
#endif
#if defined(CONFIG_LC1100H_MONITOR_BATTERY)|| defined(CONFIG_LC1132_MONITOR_BATTERY)
	&monitor_battery_dev,
#endif
};

static void __init comip_init_devices(void)
{
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

static struct mfp_pin_cfg comip_mfp_cfg[] = {
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	{FT5X06_RST_PIN,                MFP_PIN_MODE_GPIO},
	{FT5X06_INT_PIN,                MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_CM36283)
	{CM36283_INT_PIN,		MFP_PIN_MODE_GPIO },
#endif

	/*LCD GPIO*/
	{MFP_PIN_GPIO(96),		MFP_GPIO96_DSI_TE},
#if defined(LCD_RESET_PIN)
	{LCD_RESET_PIN,			MFP_PIN_MODE_GPIO},
#endif
#if defined(LCD_MODE_PIN)
	{LCD_MODE_PIN,			MFP_PIN_MODE_GPIO},
#endif
#if defined(LCD_ID_PIN)
	{LCD_ID_PIN,			MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV5647)
	/* OV5647. */
	{OV5647_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV5647_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC0329)
	/* GC0329. */
	{GC0329_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC0329_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_KTD262)
	{KTD262_ENF_PIN,		MFP_PIN_MODE_GPIO},
	{KTD262_ENM_PIN,		MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_VIDEO_DIODE_FLASH)
	{DIODE_ENF_PIN,			MFP_PIN_MODE_GPIO},
	{DIODE_ENM_PIN,			MFP_PIN_MODE_GPIO},
#endif
};

static struct mfp_pull_cfg comip_mfp_pull_cfg[] = {
	
};

static void __init comip_init_mfp(void)
{
	comip_mfp_config_array(comip_mfp_cfg, ARRAY_SIZE(comip_mfp_cfg));
	comip_mfp_config_pull_array(comip_mfp_pull_cfg, ARRAY_SIZE(comip_mfp_pull_cfg));
}

#if defined(CONFIG_I2C_COMIP) || defined(CONFIG_I2C_COMIP_MODULE)

#if defined(CONFIG_COMIP_LC1132)

static struct pmic_power_module_map lc1132_power_module_map[] = {
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO3,	PMIC_POWER_CAMERA_AF_MOTOR,	0,	0},
	{PMIC_DLDO4,	PMIC_POWER_USIM,		0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,		1,     	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,			0,	0},
	{PMIC_DLDO8,	PMIC_POWER_SDIO,		0,	0},
	{PMIC_DLDO9,   	PMIC_POWER_TOUCHSCREEN,         0,      1},
	{PMIC_DLDO10,	PMIC_POWER_LCD_IO,		0,	1},
	{PMIC_DLDO10,	PMIC_POWER_TOUCHSCREEN_IO,	0,	1},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_CORE,		0,	0},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_CORE, 	1,	0},
	{PMIC_ALDO6,	PMIC_POWER_CAMERA_ANALOG,	0,	0},
	{PMIC_ALDO6,	PMIC_POWER_CAMERA_ANALOG,	1,	0},
	{PMIC_ALDO7,	PMIC_POWER_LCD_CORE, 		0,	1},
	{PMIC_ALDO9, 	PMIC_POWER_CAMERA_CSI_PHY,      0,	0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR, 		0, 	0}
};

static struct pmic_power_ctrl_map lc1132_power_ctrl_map[] = {
	{PMIC_ALDO2,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO3,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO6,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2800},
	{PMIC_ALDO7,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO9,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO10,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO11,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1500},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	80},
};

static struct pmic_reg_st lc1132_init_regs[] = {
	/* A8 disable when sleep*/
	{LC1132_REG_LDOA_SLEEP_MODE1, 0x00, 0x80},
	/* D1 eco*/
	{LC1132_REG_LDODECO1, 0x01, 0x01},
	/* D4 eco*/
	{LC1132_REG_LDODECO1, 0x01, 0x08},
	/* D5 eco*/
	{LC1132_REG_LDODECO1, 0x01, 0x10},
	/* D7 eco*/
	{LC1132_REG_LDODECO1, 0x01, 0x40},
	/* Close LDOAFASTDISC. */
	{LC1132_REG_LDOAFASTDISC, 0x00, 0xff}
};

static struct lc1132_pmic_platform_data lc1132_pmic_info = {
	.irq_gpio = mfp_to_gpio(LC1132_INT_PIN),
	.ctrl_map = lc1132_power_ctrl_map,
	.ctrl_map_num = ARRAY_SIZE(lc1132_power_ctrl_map),
	.module_map = lc1132_power_module_map,
	.module_map_num = ARRAY_SIZE(lc1132_power_module_map),
	.init_regs = lc1132_init_regs,
	.init_regs_num = ARRAY_SIZE(lc1132_init_regs),
};

static struct lc1132_platform_data i2c_lc1132_info = {
	.flags = 0,
	.pmic_data = &lc1132_pmic_info,
};

#endif

#if defined(CONFIG_LIGHT_PROXIMITY_CM36283)
static struct cm36283_platform_data comip_i2c_cm36283_info = {
	.prox_threshold_hi = 75,//120,
	.prox_threshold_lo = 55,//80,
};
#endif

#if defined(CONFIG_SENSORS_LIS3DH)
static struct lis3dh_acc_platform_data  lis3dh_data = {
	.poll_interval = 66,//10
	.min_interval = 10,
	.g_range = 0,//2g
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,

	.init = NULL,
	.exit = NULL,
	.power_on = NULL,
	.power_off =NULL,

	.gpio_int1 = -EINVAL,//not use irq
	.gpio_int2 = -EINVAL,
};
#endif
#if defined(CONFIG_SENSORS_AKM8963)
static struct akm8963_platform_data akm8963_pdata = {
	.layout = 7,
	.outbit = 1,
	.gpio_DRDY = AKM8963_INT_PIN,
	.gpio_RST = 0,
};
#endif

/*i2c gpio set for LC181x touchscreen*/
static struct mfp_pin_cfg comip_mfp_cfg_i2c_2[] = {
	/* I2C2. */
	{MFP_PIN_GPIO(171),		MFP_GPIO171_I2C2_SCL},
	{MFP_PIN_GPIO(172),		MFP_GPIO172_I2C2_SDA},
};

static struct mfp_pin_cfg comip_mfp_cfg_gpio[] = {
	/* GPIO. */
	{MFP_PIN_GPIO(171),		MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(172),		MFP_PIN_MODE_GPIO},
};

static int ts_gpio[] = {MFP_PIN_GPIO(171), MFP_PIN_GPIO(172)};

static int ts_set_i2c_to_gpio(void)
{
	int i;
	int retval;

	comip_mfp_config_array(comip_mfp_cfg_gpio, ARRAY_SIZE(comip_mfp_cfg_gpio));
	for(i = 0; i < ARRAY_SIZE(ts_gpio); i++){
		retval = gpio_request(ts_gpio[i], "ts_i2c");
		if (retval) {
			pr_err("%s: Failed to get i2c gpio %d. Code: %d.",
				__func__, ts_gpio[i], retval);
			return retval;
		}
		retval = gpio_direction_output(ts_gpio[i], 0);
		if (retval) {
			pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
				__func__, ts_gpio[i], retval);
			gpio_free(ts_gpio[i]);
		}
		gpio_free(ts_gpio[i]);
	}
	return 0;
}

static ssize_t virtual_key_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	return sprintf(buf,
		   __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":73:1022:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":270:1022:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":466:1022:100:90"
		"\n");
#else
/**Set default value, have no use**/
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_MENU) ":0:1999:100:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":0:1999:100:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":0:1999:100:60"
		"\n");
#endif
}

static struct kobj_attribute ts_virtual_keys_attr = {
	.attr = {
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
		.name = "virtualkeys.ft5x06",
#else
		.name = "virtualkeys.lc181x-default",
#endif
		.mode = S_IRUGO,
	},
	.show = virtual_key_show,
};

static struct attribute *virtual_key_attributes[] = {
        &ts_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group virtual_key_group = {
	.attrs = virtual_key_attributes
};

static void __init comip_init_ts_virtual_key(void)
{
	struct kobject *properties_kobj;
	int ret = 0;

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj, &virtual_key_group);
	if (ret < 0)
	printk("Create virtual key properties failed!\n");
}


#if defined(CONFIG_TOUCHSCREEN_FT5X06)
static int ft5x06_rst = FT5X06_RST_PIN;
static int ft5x06_i2c_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd) {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int ft5x06_ic_power(struct device *dev, unsigned int vdd)
{
	int retval = 0;
	int gpio_rst = FT5X06_RST_PIN;
	/* Set power. */
	if (vdd){
		/*add for power on sequence*/
		retval = gpio_request(gpio_rst, "ft5x06_ts_rst");
		if(retval){
			pr_err("ft5x06_ts request rst error\n");
			return retval;
		}
		gpio_direction_output(gpio_rst, 0);
		mdelay(2);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
		gpio_free(gpio_rst);
		comip_mfp_config_array(comip_mfp_cfg_i2c_2, ARRAY_SIZE(comip_mfp_cfg_i2c_2));
	}else{
		retval = gpio_request(gpio_rst, "ft5x06_ts_rst");
		if(retval){
			pr_err("ft5x06_ts request rst error\n");
			return retval;
		}
		gpio_direction_output(gpio_rst, 0);
		mdelay(2);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_input(gpio_rst);
		gpio_free(gpio_rst);

		ts_set_i2c_to_gpio();
	}
	return 0;
}

static int ft5x06_reset(struct device *dev)
{
	gpio_request(ft5x06_rst, "ft5x06 Reset");
	gpio_direction_output(ft5x06_rst, 1);
	mdelay(2);
	gpio_direction_output(ft5x06_rst, 0);
	mdelay(10);
	gpio_direction_output(ft5x06_rst, 1);
	mdelay(200);
	gpio_free(ft5x06_rst);

	return 0;
}

static struct ft5x06_info comip_i2c_ft5x06_info = {
	.reset = ft5x06_reset,
	.power_i2c = ft5x06_i2c_power,
	.power_ic = ft5x06_ic_power,
	.irq_gpio = mfp_to_gpio(FT5X06_INT_PIN),
	.power_i2c_flag = 1,
	.power_ic_flag = 1,
	.max_scrx = 540,
	.max_scry = 960,
	.virtual_scry = 1150,
	.max_points = 5,
};
#endif

#if defined(CONFIG_RADIO_RTK8723B)
#include <plat/rtk-fm.h>
static struct rtk_fm_platform_data rtk_fm_data = {
	.fm_en = mfp_to_gpio(RTK_FM_EN_PIN),
};
#endif

/* NFC. */
static struct i2c_board_info comip_i2c0_board_info[] = {

#if defined(CONFIG_RADIO_RTK8723B)
	{
		.type = "REALTEK_FM",
		.addr = 0x12,
		.platform_data = &rtk_fm_data,
	},
#endif

};

/* Sensors. */
static struct i2c_board_info comip_i2c1_board_info[] = {
#if defined(CONFIG_LIGHT_PROXIMITY_CM36283)
	{
		.type = "cm36283",
		.addr = 0x60,
		.platform_data = &comip_i2c_cm36283_info,
		.irq = gpio_to_irq(CM36283_INT_PIN),
	},
#endif

#if defined(CONFIG_SENSORS_LIS3DH)
	{
		.type = "lis3dh_acc",
		.addr = 0x18,
		.platform_data = &lis3dh_data,
	},
#endif
#if defined(CONFIG_SENSORS_AKM8963)
	{
		.type = "akm8963",
		.addr = 0x0e,
		.platform_data = &akm8963_pdata,
		.irq = gpio_to_irq(AKM8963_INT_PIN),
	},
#endif
};

/* TouchScreen. */
static struct i2c_board_info comip_i2c2_board_info[] = {
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	{
		.type = "ft5x06",
		.addr = 0x38,
		.platform_data = &comip_i2c_ft5x06_info,
	},
#endif
};

/* PMU&CODEC. */
static struct i2c_board_info comip_i2c4_board_info[] = {
#if defined(CONFIG_COMIP_LC1132)
	{
		.type = "lc1132",
		.addr = 0x33,
		.platform_data = &i2c_lc1132_info,
	},
#endif
#if defined(CONFIG_BUCK_NCP6335)
	{
		.type = "ncp6335f",
		.addr = 0x1c,
	},
#endif
};

/* NFC. */
static struct comip_i2c_platform_data comip_i2c0_info = {
	.use_pio = 1,
	.flags = COMIP_I2C_FAST_MODE,
};

/* Sensors. */
static struct comip_i2c_platform_data comip_i2c1_info = {
	.use_pio = 1,
	.flags = COMIP_I2C_FAST_MODE,
};

/* TouchScreen. */
static struct comip_i2c_platform_data comip_i2c2_info = {
	.use_pio = 1,
	.flags = COMIP_I2C_FAST_MODE,
};

/* PMU&CODEC. */
static struct comip_i2c_platform_data comip_i2c4_info = {
	.use_pio = 1,
	.flags = COMIP_I2C_STANDARD_MODE,
};

static void __init comip_init_i2c(void)
{
	comip_set_i2c0_info(&comip_i2c0_info);
	comip_set_i2c1_info(&comip_i2c1_info);
	comip_set_i2c2_info(&comip_i2c2_info);
	comip_set_i2c4_info(&comip_i2c4_info);
	i2c_register_board_info(0, comip_i2c0_board_info, ARRAY_SIZE(comip_i2c0_board_info));
	i2c_register_board_info(1, comip_i2c1_board_info, ARRAY_SIZE(comip_i2c1_board_info));
	i2c_register_board_info(2, comip_i2c2_board_info, ARRAY_SIZE(comip_i2c2_board_info));
	i2c_register_board_info(4, comip_i2c4_board_info, ARRAY_SIZE(comip_i2c4_board_info));
}
#else
static inline void comip_init_i2c(void)
{
}
#endif

#if defined(CONFIG_FB_COMIP) || defined(CONFIG_FB_COMIP2)
static int comip_lcd_power(int onoff)
{
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

#define LCD_DETECT_ADC_CHANNEL	(2)
static int comip_lcd_detect_dev(void)
{
	int voltage;
	int lcd_id = 0;

	voltage = pmic_get_adc_conversion(LCD_DETECT_ADC_CHANNEL);
	pr_info("%s: voltage=%d \n", __func__, voltage);

	return LCD_ID_HS_NT35517;

	if (voltage <= 80)
		lcd_id = LCD_ID_HS_RM68190;
	else
		lcd_id = LCD_ID_HS_NT35517;

	return lcd_id;
}

static struct comipfb_platform_data comip_lcd_info = {
	.lcdcaxi_clk = 312000000,
	.lcdc_support_interface = COMIPFB_MIPI_IF,
	.phy_ref_freq = 26000,	//KHz
#if defined(LCD_RESET_PIN)
	.gpio_rst = LCD_RESET_PIN,
#else
	.gpio_rst = -1,
#endif
#if defined(LCD_MODE_PIN)
	.gpio_im = LCD_MODE_PIN,
#else
	.gpio_im = -1,
#endif
	.flags = COMIPFB_SLEEP_POWEROFF,
	.power = comip_lcd_power,
	.bl_control = comip_lcd_bl_control,
	.detect_dev = comip_lcd_detect_dev,
};

static void __init comip_init_lcd(void)
{
	comip_set_fb_info(&comip_lcd_info);
}
#else
static void inline comip_init_lcd(void)
{
};
#endif

#if defined(CONFIG_VIDEO_COMIP)
#if defined(CONFIG_VIDEO_OV5647)
static int ov5647_pwdn = mfp_to_gpio(OV5647_POWERDOWN_PIN);
static int ov5647_rst = mfp_to_gpio(OV5647_RESET_PIN);
static int ov5647_power(int onoff)
{
	printk("ov5647 power : %d\n", onoff);
	gpio_request(ov5647_pwdn, "OV5647 Powerdown");
	gpio_request(ov5647_rst, "OV5647 Reset");

	if (onoff) {
		gpio_direction_output(ov5647_pwdn, 1);
		gpio_direction_output(ov5647_rst, 1);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(5);
		gpio_direction_output(ov5647_pwdn, 0);
		gpio_direction_output(ov5647_rst, 0);
		msleep(2);
		gpio_direction_output(ov5647_rst, 1);
		msleep(30);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov5647_pwdn, 1);
		gpio_direction_output(ov5647_rst, 1);
	}

	gpio_free(ov5647_pwdn);
	gpio_free(ov5647_rst);

	return 0;
}

static int ov5647_reset(void)
{
	printk("ov5647 reset\n");
	return 0;
}

static struct comip_camera_subdev_platform_data ov5647_setting = {
	.flags = CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info ov5647_board_info = {
	.type = "ov5647",
	.addr = 0x36,
	.platform_data = &ov5647_setting,
};
#endif

#if defined(CONFIG_VIDEO_GC0329)
static int gc0329_pwdn = mfp_to_gpio(GC0329_POWERDOWN_PIN);
static int gc0329_rst = mfp_to_gpio(GC0329_RESET_PIN);
static int gc0329_power(int onoff)
{
	printk("gc0329 power : %d\n", onoff);
	gpio_request(gc0329_pwdn, "GC0329 Powerdown");
	gpio_request(gc0329_rst, "GC0329 Reset");

	if (onoff) {
		gpio_direction_output(gc0329_pwdn, 1);
		gpio_direction_output(gc0329_rst, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(gc0329_pwdn, 0);
		msleep(5);
		gpio_direction_output(gc0329_rst, 1);
		msleep(5);
	} else {
		gpio_direction_output(gc0329_pwdn, 1);
		msleep(5);
		gpio_direction_output(gc0329_rst, 0);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
	}

	gpio_free(gc0329_pwdn);
	gpio_free(gc0329_rst);

	return 0;
}
static int gc0329_reset(void)
{
	printk("gc0329 reset\n");
	return 0;
}

static struct comip_camera_subdev_platform_data gc0329_setting = {
	.flags = CAMERA_SUBDEV_FLAG_MIRROR | CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info gc0329_board_info = {
	.type = "gc0329",
	.addr = 0x31,
	.platform_data = &gc0329_setting,
};
#endif

#if defined(CONFIG_VIDEO_KTD262)
static int ktd262_gpio_enf = mfp_to_gpio(KTD262_ENF_PIN);
static int ktd262_gpio_enm = mfp_to_gpio(KTD262_ENM_PIN);

static int ktd262_set(enum camera_led_mode mode, int onoff)
{
	static unsigned char init_flag = 0;
	int flash_brightness = 12;	/* 1 ~ 16. */
	unsigned char i;

	printk("ktd262_set: mode=%d, onoff=%d.\n",
		mode, onoff);

	if (!init_flag) {
		gpio_request(ktd262_gpio_enf, "KTD262 ENF");
		gpio_request(ktd262_gpio_enm, "KTD262 ENM");
		gpio_direction_output(ktd262_gpio_enf, 0);
		gpio_direction_output(ktd262_gpio_enm, 0);
	}

	if (mode == FLASH) {
		if (flash_brightness > 16)
			flash_brightness = 16;

		/* turn off */
		gpio_direction_output(ktd262_gpio_enf, 0);
		udelay(1000);

		if (!onoff)
			return 0;

		/* start */
		gpio_direction_output(ktd262_gpio_enf, 1);
		udelay(150);

		for (i = 0; i < flash_brightness - 1; i++) {
			gpio_direction_output(ktd262_gpio_enf, 0);
			udelay(0);
			gpio_direction_output(ktd262_gpio_enf, 1);
			udelay(1);
		}

		return 0;
	}else {
		if (onoff) {
			gpio_direction_output(ktd262_gpio_enf, 0);
			gpio_direction_output(ktd262_gpio_enm, 1);
		} else {
			gpio_direction_output(ktd262_gpio_enf, 0);
			gpio_direction_output(ktd262_gpio_enm, 0);
		}

		return 0;
	}
}

static int camera_flash(enum camera_led_mode mode, int onoff)
{
	return ktd262_set(mode, onoff);
}
#elif defined(CONFIG_VIDEO_DIODE_FLASH)
static int diode_gpio_enf = mfp_to_gpio(DIODE_ENF_PIN);
static int diode_gpio_enm = mfp_to_gpio(DIODE_ENM_PIN);

static int diode_flash_set(enum camera_led_mode mode, int onoff)
{
	printk("diode_flash_set: mode=%d, onoff=%d.\n",
		mode, onoff);

	gpio_request(diode_gpio_enf, "DIODE_FLASH ENF");
	gpio_request(diode_gpio_enm, "DIODE_FLASH ENM");

	if (mode == TORCH)
		gpio_direction_output(diode_gpio_enm, 0);
	else if (mode == FLASH)
		gpio_direction_output(diode_gpio_enm, 1);

	if (onoff) {
		gpio_direction_output(diode_gpio_enf, 1);
	} else {
		gpio_direction_output(diode_gpio_enf, 0);
	}

	gpio_free(diode_gpio_enf);
	gpio_free(diode_gpio_enm);
	
	return 0;
}

static int camera_flash(enum camera_led_mode mode, int onoff)
{
	return diode_flash_set(mode, onoff);
}
#else
static int camera_flash(enum camera_led_mode mode, int onoff)
{
	return 0;
}
#endif

static struct comip_camera_client comip_camera_clients[] = {
#if defined(CONFIG_VIDEO_OV5647)
	{
		.board_info = &ov5647_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI | CAMERA_CLIENT_FRAMERATE_DYN,
		.caps = CAMERA_CAP_FOCUS_INFINITY
			| CAMERA_CAP_FOCUS_AUTO
			| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
			| CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_FLASH,
		.if_id = 0,
		.mipi_lane_num = 2,
		.power = ov5647_power,
		.reset = ov5647_reset,
		.flash = camera_flash,
	},
#endif

#if defined(CONFIG_VIDEO_GC0329)
        {
		.board_info = &gc0329_board_info,
		.flags = CAMERA_CLIENT_IF_DVP
				| CAMERA_CLIENT_CLK_EXT
				| CAMERA_CLIENT_YUV_DATA,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = gc0329_power,
		.reset = gc0329_reset,
	},
#endif
};

static struct comip_camera_platform_data comip_camera_info = {
	.i2c_adapter_id = 3,
	.flags = CAMERA_USE_ISP_I2C | CAMERA_USE_HIGH_BYTE
			| CAMERA_I2C_PIO_MODE | CAMERA_I2C_STANDARD_SPEED,
	.anti_shake_parms = {
		.a = 12,
		.b = 3,
		.c = 3,
		.d = 4,
	},
	.flash_parms = {
		.redeye_on_duration = 500,
		.redeye_off_duration = 100,
		.snapshot_pre_duration = 500,
		.aecgc_stable_duration = 1,
		.mean_percent = 50,
		.flash_brightness = 100,
	},
	.client = comip_camera_clients,
	.client_num = ARRAY_SIZE(comip_camera_clients),
};

static void __init comip_init_camera(void)
{
	comip_set_camera_info(&comip_camera_info);
}
#else
static void inline comip_init_camera(void)
{
};
#endif

static void comip_init_misc(void)
{
	int gpio1 = mfp_to_gpio(BOARD_FLAG_PIN1);
	int gpio2 = mfp_to_gpio(BOARD_FLAG_PIN2);
	int board;
	enum {
		BOARD_P1,
		BOARD_P2,
		BOARD_COSTDOWN,
		BOARD_P0,
	};
	const char *board_str[] = {"P1", "P2", "COSTDOWN", "P0"};

	comip_mfp_config(BOARD_FLAG_PIN1, MFP_PIN_MODE_GPIO);
	comip_mfp_config(BOARD_FLAG_PIN2, MFP_PIN_MODE_GPIO);
	gpio_request(gpio1, "flag pin1");
	gpio_request(gpio2, "flag pin2");
	gpio_direction_input(gpio1);
	gpio_direction_input(gpio2);
	board = (gpio_get_value(gpio1) << 1) | gpio_get_value(gpio2);
	gpio_free(gpio1);
	gpio_free(gpio2);
#if defined(CONFIG_COMIP_BOARD_UST802_PHONE_V1_0)
	printk("Build-UST802P0: %s(0x%02x)\n", board_str[board], board);
#elif defined(CONFIG_COMIP_BOARD_UST802_PHONE_V1_1)
	printk("Build-UST802P1: %s(0x%02x)\n", board_str[board], board);
#endif

#if defined(CONFIG_SND_LC181X_SOC)
	if (board == BOARD_P0)
		comip_codec_interface_data.main_mic_flag = 1;
#endif
}

static void __init comip_init_board_early(void)
{
	comip_init_mfp();
	comip_init_i2c();
	comip_init_lcd();
}

static void __init comip_init_board(void)
{
	comip_init_camera();
	comip_init_devices();
	comip_init_misc();
	comip_init_ts_virtual_key();
}

