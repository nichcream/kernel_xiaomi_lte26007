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
** Version	Date		Author		Description
** 1.0.0	2014-4-2	xuxuefeng	created
**
*/
#include <mach/timer.h>
#include <mach/gpio.h>
#include <plat/cpu.h>
#include <plat/mfp.h>
#include <plat/i2c.h>
#include <plat/comip-pmic.h>
#include <plat/comip-backlight.h>
#include <plat/comip-thermal.h>
#include <plat/comip-battery.h>
#if defined(CONFIG_COMIP_LC1160)
#include <plat/lc1160.h>
#include <plat/lc1160-pmic.h>
#include <plat/lc1160_adc.h>
#include <plat/lc1160-pwm.h>
#endif
#if defined(CONFIG_SENSORS_BMA222E)
#include <plat/bma2x2.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#include <plat/ft5x06_ts.h>
#endif
#include "board-a310t.h"

#include "board-common.c"

#if defined(CONFIG_LC1132_MONITOR_BATTERY) || defined(CONFIG_LC1160_MONITOR_BATTERY)
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
struct monitor_battery_platform_data bci_data = {
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

#if defined(CONFIG_LC1132_ADC) || defined(CONFIG_LC1160_ADC)
struct pmic_adc_platform_data lc_adc = {
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

#if defined(CONFIG_COMIP_BOARD_A310T_PHONE_V1_0)
static struct bright_pulse range_table[] = {
	{100, 1}, {97, 2}, {94, 3}, {91, 4}, {88, 5}, {85, 6},
	{82, 7}, {79, 8}, {76, 9}, {73, 10}, {70, 11}, {67, 12},
	{64, 13}, {61, 14}, {58, 15}, {55, 16}, {52, 17}, {49, 18},
	{46, 19}, {43, 20}, {40, 21}, {37, 22}, {34, 23}, {31, 24},
	{28, 25}, {25, 26}, {22, 27}, {19, 28}, {16, 29}, {12, 30},
	{9, 31}, {6, 32},
};

static struct comip_backlight_platform_data comip_backlight_data = {
	.ctrl_type = CTRL_PULSE,
	.pulse_gpio = LCD_BK_CTL_PIN,
	.ranges = range_table,
	.range_cnt = ARRAY_SIZE(range_table),
	.bl_control = comip_lcd_bl_control,
	.key_bl_control = comip_key_bl_control,
};
#elif defined(CONFIG_COMIP_BOARD_A310T_PHONE_V1_1)
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
#endif
static struct platform_device comip_backlight_device = {
	.name = "comip-backlight",
	.id = -1,
	.dev = {
		.platform_data = &comip_backlight_data,
	}
};
#endif

#if defined(CONFIG_LC1160_PWM)
static void lc1160_pwm_device_init(int id)
{
}

static struct lc1160_pwm_platform_data lc1160_pwm_info = {
	.dev_init = lc1160_pwm_device_init,
};

static struct platform_device lc1160_pwm_device = {
	.name = "lc1160-pwm",
	.id = -1,
	.dev = {
		.platform_data = &lc1160_pwm_info,
	}
};
#endif

#if defined(CONFIG_THERMAL_COMIP)
static struct sample_rate_member sample_table[] = {
	{-100, 10, SAMPLE_RATE_SLOW, THERMAL_NORMAL},
	{10, 70, SAMPLE_RATE_NORMAL, THERMAL_NORMAL},
	{70, 73, SAMPLE_RATE_FAST, THERMAL_WARM},
	{73, 76, SAMPLE_RATE_FAST, THERMAL_HOT},
	{76, 78, SAMPLE_RATE_EMERGENT, THERMAL_TORRID},
	{78, 80, SAMPLE_RATE_EMERGENT, THERMAL_CRITICAL},
	{80, 83, SAMPLE_RATE_EMERGENT, THERMAL_FATAL},
	{83, 100, SAMPLE_RATE_EMERGENT, THERMAL_ERROR},
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
#if defined(CONFIG_LC1160_PWM)
	&lc1160_pwm_device,
#endif
#if defined(CONFIG_THERMAL_COMIP)
	&comip_thermal_device,
#endif
#if defined(CONFIG_LC1132_ADC) || defined(CONFIG_LC1160_ADC)
	&adc_dev,
#endif
#if defined(CONFIG_LC1132_MONITOR_BATTERY) || defined(CONFIG_LC1160_MONITOR_BATTERY)
	&monitor_battery_dev,
#endif
};

static void __init comip_init_devices(void)
{
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

static struct mfp_pin_cfg comip_mfp_cfg[] = {
	// TODO:  to be continue for device
#if defined(CONFIG_COMIP_LC1160)
	/* LC1160. */
	{LC1160_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif
	/* LCD. */
	{LCD_RESET_PIN,			MFP_PIN_MODE_GPIO},
#if defined(CONFIG_COMIP_BOARD_A310T_PHONE_V1_0)
	{LCD_BK_CTL_PIN,                MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	{FT5X06_RST_PIN,                MFP_PIN_MODE_GPIO},
	{FT5X06_INT_PIN,                MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC2145_2LANE_26M)
	{GC2145_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC2145_RESET_PIN,      MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_HM2056)
	/* hm2056. */
	{HM2056_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{HM2056_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_BF3905)       /* BF3905. */
	{BF3905_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{BF3905_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC0310)       /* GC0310. */
	{GC0310_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC0310_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

	/*mfp low power config*/
	{MFP_PIN_GPIO(197), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(225), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(117), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(116), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(115), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(114), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(109), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(108), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(107), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(105), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(104), 	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(102), 	MFP_PIN_MODE_GPIO},
};

static struct mfp_pull_cfg comip_mfp_pull_cfg[] = {
	{COMIP_GPIO_KEY_VOLUMEUP,	MFP_PULL_UP},
	{COMIP_GPIO_KEY_VOLUMEDOWN,	MFP_PULL_UP},
#if defined(CONFIG_RTK_BLUETOOTH)
	{RTK_BT_UART_CTS_PIN,	MFP_PULL_DOWN},
#endif
	/*mfp low power config*/
	{MFP_PIN_GPIO(72),		MFP_PULL_UP},
	{MFP_PIN_GPIO(73),		MFP_PULL_UP},
	{MFP_PIN_GPIO(253), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(140), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(169), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(170), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(198), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(199), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(117), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(116), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(115), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(114), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(109), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(108), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(107), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(105), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(104), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(102), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(167), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(168), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(171), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(172), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(230), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(233), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(234), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(235), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(236), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(237), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(238), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(239), 	MFP_PULL_UP},
	{MFP_PIN_GPIO(243), 	MFP_PULL_DOWN},
};

static void __init comip_init_mfp(void)
{
	comip_mfp_config_array(comip_mfp_cfg, ARRAY_SIZE(comip_mfp_cfg));
	comip_mfp_config_pull_array(comip_mfp_pull_cfg, ARRAY_SIZE(comip_mfp_pull_cfg));
}

static struct mfp_gpio_cfg comip_init_mfp_lp_gpio_cfg[] = {
	{MFP_PIN_GPIO(117),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(116),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(115),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(114),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(109),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(108),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(107),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(105),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(104),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(102),		MFP_GPIO_INPUT},
	{MFP_PIN_GPIO(197),		MFP_GPIO_OUTPUT,		MFP_GPIO_VALUE_LOW},
	{MFP_PIN_GPIO(225),		MFP_GPIO_OUTPUT,		MFP_GPIO_VALUE_LOW},
};

static void __init comip_init_gpio_lp(void)
{
	comip_gpio_config_array(comip_init_mfp_lp_gpio_cfg, ARRAY_SIZE(comip_init_mfp_lp_gpio_cfg));
}


#if defined(CONFIG_I2C_COMIP) || defined(CONFIG_I2C_COMIP_MODULE)
#if defined(CONFIG_COMIP_LC1160)
static struct pmic_power_module_map lc1160_power_module_map[] = {
	{PMIC_DCDC9,	PMIC_POWER_WLAN_IO,		0,	0},
	{PMIC_DLDO3,	PMIC_POWER_SDIO,		0,	0},
	{PMIC_DLDO4,	PMIC_POWER_USIM,		0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,		1,	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,		0,	0},
	{PMIC_DLDO7,	PMIC_POWER_LCD_CORE,		0,	1},
	{PMIC_DLDO8,	PMIC_POWER_TOUCHSCREEN,		0,	1},
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		0,	0},
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		1,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_AF_MOTOR,		0,	0},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,		0,	0},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,		1,	0},
	{PMIC_ALDO10,	PMIC_POWER_LCD_IO,		0,	1},
	{PMIC_ALDO10,	PMIC_POWER_TOUCHSCREEN_IO,		0,	1},
	{PMIC_ALDO10,	PMIC_POWER_CAMERA_CSI_PHY,		0,	1},
	{PMIC_ALDO11,	PMIC_POWER_GPS_CORE,		0,	0},
	{PMIC_ALDO12,	PMIC_POWER_USB,			1,	0},
	{PMIC_ALDO14,	PMIC_POWER_WLAN_CORE,		0,	0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR,		0,	0}
};

static struct pmic_power_ctrl_map lc1160_power_ctrl_map[] = {
	{PMIC_DCDC9,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
	//{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO8,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO12,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	10},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	80},
};

static struct pmic_reg_st lc1160_init_regs[] = {
	/* RF power register could be write by SPI &IIC */
	{LC1160_REG_SPICR, 0x01, LC1160_REG_BITMASK_SPI_IIC_EN},
	/* DCDC9 control by oscen*/
	{LC1160_REG_DCLDOCONEN, 0x00, LC1160_REG_BITMASK_DC9CONEN},
	/* ALDO2 enter ECO mode when sleep */
	{LC1160_REG_LDOA2CR, 0x01, LC1160_REG_BITMASK_LDOA2SLP},
	/* ALDO1/4/5/6/7/8/9/10/11/12 disable when sleep */
	{LC1160_REG_LDOA_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOA5_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOA6_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOA1_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOA4_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOA7_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOA8_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE3, 0x00, LC1160_REG_BITMASK_LDOA9_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE3, 0x00, LC1160_REG_BITMASK_LDOA10_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE3, 0x00, LC1160_REG_BITMASK_LDOA11_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE3, 0x00, LC1160_REG_BITMASK_LDOA12_ALLOW_IN_SLP},
	/* DLDO1/2 enter ECO mode when sleep */
	{LC1160_REG_LDOD1CR, 0x01, LC1160_REG_BITMASK_LDOD1SLP},
	{LC1160_REG_LDOD2CR, 0x01, LC1160_REG_BITMASK_LDOD2SLP},
	{LC1160_REG_LDOD4CR, 0x01, LC1160_REG_BITMASK_LDOD4SLP},
	{LC1160_REG_LDOD5CR, 0x01, LC1160_REG_BITMASK_LDOD5SLP},
	/* DLDO3/6/7/8/9/10/11/ disable when sleep */
	{LC1160_REG_LDOD_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOD3_ALLOW_IN_SLP},
	{LC1160_REG_LDOD_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOD6_ALLOW_IN_SLP},
	{LC1160_REG_LDOD_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOD7_ALLOW_IN_SLP},
	{LC1160_REG_LDOD_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOD8_ALLOW_IN_SLP},
	{LC1160_REG_LDOD_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOD9_ALLOW_IN_SLP},
	{LC1160_REG_LDOD_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOD10_ALLOW_IN_SLP},
	{LC1160_REG_LDOD_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOD11_ALLOW_IN_SLP},
	/* Current sink for LCD backlight function selection: used for LCD backlight */
	{LC1160_REG_INDDIM, 0x00, LC1160_REG_BITMASK_DIMEN},
	/* ALDO5 0.95V, ALDO6 1.8V, BUCK7 power on */
	{LC1160_REG_LDOA5CR, 0xc4, 0xff},
	{LC1160_REG_LDOA6CR, 0xcd, 0xff},
	{LC1160_REG_DC7OVS0, 0x5a, 0xff},
};

static struct lc1160_pmic_platform_data lc1160_pmic_info = {
	.irq_gpio = mfp_to_gpio(LC1160_INT_PIN),
	.ctrl_map = lc1160_power_ctrl_map,
	.ctrl_map_num = ARRAY_SIZE(lc1160_power_ctrl_map),
	.module_map = lc1160_power_module_map,
	.module_map_num = ARRAY_SIZE(lc1160_power_module_map),
	.init_regs = lc1160_init_regs,
	.init_regs_num = ARRAY_SIZE(lc1160_init_regs),
};

static struct lc1160_platform_data i2c_lc1160_info = {
	.flags = LC1160_FLAGS_DEVICE_CHECK,
	.pmic_data = &lc1160_pmic_info,
};

#endif

/* accel */
#if defined (CONFIG_SENSORS_BMA222E)
static struct bma2x2_platform_data bma2x2_pdata = {
	#ifdef CONFIG_COMIP_BOARD_A310T_PHONE_V1_0
	.position = 1,
	#endif
	#ifdef CONFIG_COMIP_BOARD_A310T_PHONE_V1_1
	.position = 5,
	#endif
};
#endif

static ssize_t virtual_key_show(struct kobject *kobj,
                                struct kobj_attribute *attr, char *buf)
{
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#if 0
	/**1080p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":200:2000:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":530:2000:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":870:2000:100:90"
	               "\n");
#else
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":80:902:100:80"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":240:902:100:80"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":400:902:100:80"
	               "\n");

#endif
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
		.name = "virtualkeys.lc186x-default",
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

/*i2c gpio set for LC186x touchscreen*/
static struct mfp_pin_cfg comip_mfp_cfg_i2c_2[] = {
	/* I2C2. */
	{MFP_PIN_GPIO(163),		MFP_PIN_MODE_1},
	{MFP_PIN_GPIO(164),		MFP_PIN_MODE_1},
};

static struct mfp_pin_cfg comip_mfp_cfg_gpio[] = {
	/* GPIO. */
	{MFP_PIN_GPIO(163),		MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(164),		MFP_PIN_MODE_GPIO},
};

static int ts_gpio[] = {MFP_PIN_GPIO(163), MFP_PIN_GPIO(164)};

static int ts_set_i2c_to_gpio(void)
{
	int i;
	int retval;

	comip_mfp_config_array(comip_mfp_cfg_gpio, ARRAY_SIZE(comip_mfp_cfg_gpio));
	for(i = 0; i < ARRAY_SIZE(ts_gpio); i++) {
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
	if (vdd) {
		/*add for power on sequence*/
		retval = gpio_request(gpio_rst, "ft5x06_ts_rst");
		if(retval) {
			pr_err("ft5x06_ts request rst error\n");
			return retval;
		}
		gpio_direction_output(gpio_rst, 0);
		msleep(2);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(10);
		gpio_direction_output(gpio_rst, 1);
		gpio_free(gpio_rst);
		comip_mfp_config_array(comip_mfp_cfg_i2c_2, ARRAY_SIZE(comip_mfp_cfg_i2c_2));
	} else {
		retval = gpio_request(gpio_rst, "ft5x06_ts_rst");
		if(retval) {
			pr_err("ft5x06_ts request rst error\n");
			return retval;
		}
		gpio_direction_output(gpio_rst, 0);
		msleep(2);
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
	msleep(2);
	gpio_direction_output(ft5x06_rst, 0);
	msleep(10);
	gpio_direction_output(ft5x06_rst, 1);
	msleep(200);
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
	.max_scrx = 480,
	.max_scry = 800,
	.virtual_scry = 1000,
	.max_points = 2,
};
#endif

#if defined(CONFIG_RADIO_RTK8723B)
#include <plat/rtk-fm.h>
static struct rtk_fm_platform_data rtk_fm_data = {
	.fm_en = mfp_to_gpio(RTK_FM_EN_PIN),
	.powersave = rtk_set_powersave,
};
#endif

/* NFC. */
static struct i2c_board_info comip_i2c0_board_info[] = {
	// TODO:  to be continue for device
};

/* Sensors. */
static struct i2c_board_info comip_i2c1_board_info[] = {
	// TODO:  to be continue for device
	/* accel */
#if defined(CONFIG_SENSORS_BMA222E)
		{
			.type = "bma2x2",
			.addr = 0x18,
			.platform_data = &bma2x2_pdata,
		},
#endif

#if defined(CONFIG_RADIO_RTK8723B)
		{
			.type = "REALTEK_FM",
			.addr = 0x12,
			.platform_data = &rtk_fm_data,
		},
#endif
};

/* TouchScreen. */
static struct i2c_board_info comip_i2c2_board_info[] = {
	// TODO:  to be continue for device
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
	// TODO:  to be continue for device
#if defined(CONFIG_COMIP_LC1160)
	{
		.type = "lc1160",
		.addr = 0x33,
		.platform_data = &i2c_lc1160_info,
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
	if (cpu_is_lc1860_eco1() == 3) {
		printk("comip_init_i2c cpu_is_lc1860_eco1\n");
		int index = 0;
		struct pmic_power_ctrl_map *power_ctrl_map_item = NULL;
		for(index = 0; index < sizeof(lc1160_power_ctrl_map) / sizeof(lc1160_power_ctrl_map[0]); index++){

			power_ctrl_map_item = &lc1160_power_ctrl_map[index];

			if (power_ctrl_map_item->power_id == PMIC_ALDO12) {
				power_ctrl_map_item->default_mv = 1550;
				printk("lc1860_eco1:PMIC_ALDO12 default_mv = 1550 \n");
				break;
			}
		}
	}

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

#if defined(CONFIG_FB_COMIP2) || defined(CONFIG_FB_COMIP2_MODULE)

#if defined(CONFIG_LCD_BT_ILI9806E)
static int comip_lcd_power(int onoff)
{
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(30);
	} else {
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int comip_lcd_detect_dev(void)
{
	return LCD_ID_BT_ILI9806E;
}

#elif defined(CONFIG_LCD_DJN_ILI9806E)
static int comip_lcd_power(int onoff)
{
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(30);
	} else {
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int comip_lcd_detect_dev(void)
{
	return LCD_ID_DJN_ILI9806E;
}
#endif

static struct comipfb_platform_data comip_lcd_info = {
	.lcdcaxi_clk = 416000000,
	.lcdc_support_interface = COMIPFB_MIPI_IF,
	.phy_ref_freq = 26000,	/* KHz */
	.gpio_rst = LCD_RESET_PIN,
	.gpio_im = -1,
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
#if defined(CONFIG_VIDEO_GC2145_2LANE_26M)
static int gc2145_pwdn = mfp_to_gpio(GC2145_POWERDOWN_PIN);
static int gc2145_rst = mfp_to_gpio(GC2145_RESET_PIN);
static int gc2145_power(int onoff)
{
	printk("gc2145 power : %d\n", onoff);

	gpio_request(gc2145_pwdn, "GC2145 Powerdown Pin");
	gpio_request(gc2145_rst, "GC2145 Reset");
	if (onoff) {
		gpio_direction_output(gc2145_pwdn, 1);
		gpio_direction_output(gc2145_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(gc2145_pwdn, 0);
		mdelay(10);
		gpio_direction_output(gc2145_rst, 0);
		mdelay(50);
		gpio_direction_output(gc2145_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_request(gc2145_pwdn, "GC2145 Powerdown Pin");
		gpio_direction_output(gc2145_pwdn, 0);
		gpio_direction_output(gc2145_rst, 0);
	}
	gpio_free(gc2145_pwdn);
	gpio_free(gc2145_rst);

	return 0;
}
static int gc2145_reset(void)
{
	printk("GC2145 reset\n");

	return 0;
}

static struct i2c_board_info gc2145_board_info = {
	.type = "gc2145-mipi-yuv",
	.addr = 0x3c,
};
#endif

#if defined(CONFIG_VIDEO_GC0310)
static int gc0310_pwdn = mfp_to_gpio(GC0310_POWERDOWN_PIN);
static int gc0310_rst = mfp_to_gpio(GC0310_RESET_PIN);
static int gc0310_power(int onoff)
{
	printk("GC0310 power : %d\n", onoff);

	gpio_request(gc0310_pwdn, "gc0310 Powerdown Pin");
	gpio_request(gc0310_rst, "gc0310 Reset");
	if (onoff) {
		gpio_direction_output(gc0310_pwdn, 1);
		gpio_direction_output(gc0310_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
	//	pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(gc0310_pwdn, 0);
		mdelay(10);
		gpio_direction_output(gc0310_rst, 1);
	       mdelay(20);
		gpio_direction_output(gc0310_rst, 0);
		mdelay(20);
		gpio_direction_output(gc0310_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(gc0310_pwdn, "gc0310 Powerdown Pin");
		gpio_direction_output(gc0310_pwdn, 0);
		gpio_direction_output(gc0310_rst, 0);
	}
	gpio_free(gc0310_pwdn);
	gpio_free(gc0310_rst);

	return 0;
}

static int gc0310_reset(void)
{
	printk("gc0310 reset\n");

	return 0;
}

static struct comip_camera_subdev_platform_data gc0310_setting = {
	.flags = CAMERA_SUBDEV_FLAG_MIRROR | CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info gc0310_board_info = {
	.type = "gc0310-mipi-yuv",
	.addr = 0x21,
	.platform_data = &gc0310_setting,
};
#endif

#if defined(CONFIG_VIDEO_HM2056)
static int hm2056_pwdn = mfp_to_gpio(HM2056_POWERDOWN_PIN);
static int hm2056_rst = mfp_to_gpio(HM2056_RESET_PIN);
static int hm2056_power(int onoff)
{
	printk("hm2056 power : %d\n", onoff);

	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);

		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_request(hm2056_pwdn, "HM2056 Powerdown  0");
		gpio_direction_output(hm2056_pwdn, 0);
		mdelay(1);

		gpio_free(hm2056_pwdn);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_request(hm2056_pwdn, "HM2056 Powerdown  1");
		gpio_direction_output(hm2056_pwdn, 1);
		mdelay(1);
		gpio_free(hm2056_pwdn);
	}

	return 0;
}

static int hm2056_reset(void)
{
	printk("hm2056 reset\n");
	gpio_request(hm2056_rst, "HM2056 Reset");
	gpio_direction_output(hm2056_rst, 1);
	mdelay(50);
	gpio_direction_output(hm2056_rst, 0);
	mdelay(50);
	gpio_direction_output(hm2056_rst, 1);
	mdelay(50);
	gpio_free(hm2056_rst);
	return 0;
}

static struct i2c_board_info hm2056_board_info = {
	.type = "hm2056",
	.addr = 0x24,
};
#endif


#if defined(CONFIG_VIDEO_BF3905)
static int bf3905_pwdn = mfp_to_gpio(BF3905_POWERDOWN_PIN);
static int bf3905_rst = mfp_to_gpio(BF3905_RESET_PIN);
static int bf3905_power(int onoff)
{
	printk("BF3905 power : %d\n", onoff);

	gpio_request(bf3905_pwdn, "bf3905 Powerdown Pin");
	gpio_request(bf3905_rst, "bf3905 Reset");
	if (onoff) {
		gpio_direction_output(bf3905_pwdn, 1);
		gpio_direction_output(bf3905_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(bf3905_pwdn, 0);
		mdelay(10);
		gpio_direction_output(bf3905_rst, 0);
		mdelay(50);
		gpio_direction_output(bf3905_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(bf3905_pwdn, "bf3905 Powerdown Pin");
		gpio_direction_output(bf3905_pwdn, 0);
		gpio_direction_output(bf3905_rst, 0);
	}
	gpio_free(bf3905_pwdn);
	gpio_free(bf3905_rst);

	return 0;
}

static int bf3905_reset(void)
{
	printk("bf3905 reset\n");

	return 0;
}

static struct comip_camera_subdev_platform_data bf3905_setting = {
	.flags = CAMERA_SUBDEV_FLAG_MIRROR | CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info bf3905_board_info = {
	.type = "bf3905-mipi-yuv",
	.addr = 0x6e,
	.platform_data = &bf3905_setting,
};
#endif

static struct comip_camera_client comip_camera_clients[] = {
#if defined(CONFIG_VIDEO_GC2145_2LANE_26M)
	{
		.board_info = &gc2145_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_CLK_EXT
			|CAMERA_CLIENT_ISP_CLK_HIGH
			|CAMERA_CLIENT_YUV_DATA,
		.caps = CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.camera_id = CAMERA_ID_BACK,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = gc2145_power,
		.reset = gc2145_reset,
	},
#endif

#if defined(CONFIG_VIDEO_BF3905)
	{
		.board_info = &bf3905_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
				| CAMERA_CLIENT_YUV_DATA,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,

		.if_id = 1,
		.mipi_lane_num = 1,
		.camera_id = CAMERA_ID_FRONT,
		.power = bf3905_power,
		.reset = bf3905_reset,
//		.flash = camera_flash,
        },
#endif

#if defined(CONFIG_VIDEO_HM2056)
	{
		.board_info = &hm2056_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
	              |CAMERA_CLIENT_CLK_EXT
			|CAMERA_CLIENT_ISP_CLK_HIGH
			|CAMERA_CLIENT_YUV_DATA,
		.caps = CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 1,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = hm2056_power,
		.reset = hm2056_reset,
	},
#endif

#if defined(CONFIG_VIDEO_GC0310)
	{
		.board_info = &gc0310_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
				| CAMERA_CLIENT_YUV_DATA,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,

		.if_id = 1,
		.mipi_lane_num = 1,
		.power = gc0310_power,
		.reset = gc0310_reset,
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

#if defined(CONFIG_USB_COMIP) || defined(CONFIG_USB_COMIP_MODULE)
static void __init comip_init_usb_flag(void)
{
	comip_usb_info.udc.flag = USB_PD_FLAGS_USB0VBUS;
}
#else
static void inline comip_init_usb_flag(void)
{
};
#endif

static void comip_muxpin_pvdd_config(void)
{
	comip_pvdd_vol_ctrl_config(MUXPIN_PVDD4_VOL_CTRL,PVDDx_VOL_CTRL_1_8V);
}


static void comip_init_misc(void)
{
}

static void __init comip_init_board_early(void)
{
	comip_muxpin_pvdd_config();
	comip_init_mfp();
	comip_init_gpio_lp();
	comip_init_i2c();
	comip_init_lcd();
	comip_init_usb_flag();
}
static void __init comip_init_board(void)
{
	comip_init_camera();
	comip_init_devices();
	comip_init_misc();
	comip_init_ts_virtual_key();
}

