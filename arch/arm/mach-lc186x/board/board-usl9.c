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
** 1.0.0	2014-3-19	xuxuefeng	created
**
*/
#include <mach/timer.h>
#include <mach/gpio.h>
#include <plat/mfp.h>
#include <plat/i2c.h>
#include <plat/cpu.h>
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

#if defined(CONFIG_UNISCOPE_TOUCHSCREEN)
//#include <plat/ft5x06_ts.h>
#include <plat/uniscope-ts.h>
#endif

#if defined(CONFIG_SENSORS_MXC400X) || defined(CONFIG_SENSORS_MXC400X_STATIC)
#include <plat/mxc400x.h>
#endif

#include "board-usl9.h"

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
	gpio_request(KEYPAD_LED_PIN, "key-pad led");

	if(onoff){
		gpio_direction_output(KEYPAD_LED_PIN, 1);
	} else {
		gpio_direction_output(KEYPAD_LED_PIN, 0);
	}

	gpio_free(KEYPAD_LED_PIN);
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
};

static struct mfp_ds_cfg comip_ds_cfg[] = {
	{MFP_PIN_CTRL_MMC0CLK,		MFP_DS_8MA},
	{MFP_PIN_CTRL_MMC0CMD,		MFP_DS_8MA},
	{MFP_PIN_CTRL_MMC0D0,		MFP_DS_8MA},
	{MFP_PIN_CTRL_MMC0D1,		MFP_DS_8MA},
	{MFP_PIN_CTRL_MMC0D2,		MFP_DS_8MA},
	{MFP_PIN_CTRL_MMC0D3,		MFP_DS_8MA},
};

static struct mfp_pin_cfg comip_mfp_cfg[] = {
	// TODO:  to be continue for device

#if defined(CONFIG_COMIP_LC1160)
	/* LC1160. */
	{LC1160_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif

	/*PA*/
#if defined(CODEC_PA_PIN)
	{CODEC_PA_PIN,		MFP_PIN_MODE_GPIO},
#endif

	/*KEYPAD LED*/
	{KEYPAD_LED_PIN,	MFP_PIN_MODE_GPIO},
	
	/* LCD. */
	{LCD_RESET_PIN,		MFP_PIN_MODE_GPIO},
#if defined(LCD_TE)
	{LCD_TE,			MFP_PIN_MODE_GPIO},
#endif

	/* Tp */
#if defined(TS_INT_PIN)
	{TS_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif
#if defined(TS_RST_PIN)
	{TS_RST_PIN,		MFP_PIN_MODE_GPIO},
#endif

	/* Sensors */
#if defined(LIGHT_INT)
	{LIGHT_INT,			MFP_PIN_MODE_GPIO},
#endif
#if defined(ECOMPASS_INT_PIN)
	{ECOMPASS_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif
#if defined(ACCELER_INT_PIN)
	{ACCELER_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif

	/*BT*/
#ifdef CONFIG_BRCM_BLUETOOTH
	{BRCM_BT_ONOFF_PIN,		MFP_PIN_MODE_GPIO},
    {BRCM_BT_RESET_PIN,		MFP_PIN_MODE_GPIO},
    {BRCM_BT_WAKE_I_PIN,	MFP_PIN_MODE_GPIO}, 
    {BRCM_BT_WAKE_O_PIN,	MFP_PIN_MODE_GPIO},
#endif
	/*WIFI*/
#ifdef CONFIG_BRCM_WIFI
	{BRCM_WIFI_ONOFF_PIN,	MFP_PIN_MODE_GPIO},
    {BRCM_WIFI_WAKE_I_PIN,	MFP_PIN_MODE_GPIO},
#endif
#ifdef CONFIG_BRCM_GPS
	{BCM_GPS_RESET_PIN,		MFP_PIN_MODE_GPIO},
    {BCM_GPS_STANDBY_PIN,	MFP_PIN_MODE_GPIO},
#endif

	/*OTG ID*/
#ifdef CONFIG_USB_COMIP_OTG
	{USB_OTG_ID_PIN,		MFP_PIN_MODE_GPIO},
#endif

	/*Camera*/
#if defined(CONFIG_VIDEO_COMIP)
    {BCAMERA_POWERDOWN_PIN,	MFP_PIN_MODE_GPIO},
    {BCAMERA_RESET_PIN,		MFP_PIN_MODE_GPIO},
    {FCAMERA_POWERDOWN_PIN,	MFP_PIN_MODE_GPIO}, 
    {FCAMERA_RESET_PIN,		MFP_PIN_MODE_GPIO},
    //{CAMERA_FALSH_PIN,	MFP_PIN_MODE_GPIO},
    //{CAMERA_LDO_PIN,		MFP_PIN_MODE_GPIO },
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

#if defined(CONFIG_BATTERY_MAX17058)
	{MAX17058_FG_INT_PIN,	MFP_PULL_UP},
#endif

#if defined(CONFIG_CHARGER_NCP1851)||defined(CONFIG_CHARGER_BQ24158)
	{EXTERN_CHARGER_INT_PIN,	MFP_PULL_UP},
#endif

#if defined(CONFIG_RTK_BLUETOOTH) || defined(CONFIG_BRCM_BLUETOOTH)
	{UART2_CTS_PIN,	MFP_PULL_DOWN},
#endif
	/*mfp low power config*/
#if defined(CONFIG_COMIP_BOARD_USL9_PHONE_V1_0)
	{MFP_PIN_GPIO(72),		MFP_PULL_UP},
	{MFP_PIN_GPIO(73),		MFP_PULL_UP},
#else
	{MFP_PIN_GPIO(72),		MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(73),		MFP_PULL_DISABLE},
#endif
	{MFP_PIN_GPIO(253), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(198), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(199), 	MFP_PULL_DISABLE},

	{MFP_PIN_GPIO(169), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(170), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(171), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(172), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(176), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(167), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(168), 	MFP_PULL_DISABLE},

	//{MFP_PIN_GPIO(140), 	MFP_PULL_DISABLE},
	
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

	{MFP_PIN_GPIO(230), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(233), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(234), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(235), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(236), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(237), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(238), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(239), 	MFP_PULL_UP},
#if defined(CONFIG_COMIP_BOARD_USL9_PHONE_V1_0)
	{MFP_PIN_GPIO(151), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(175), 	MFP_PULL_DOWN},
#endif
};

static void __init comip_init_mfp(void)
{
	comip_mfp_config_array(comip_mfp_cfg, ARRAY_SIZE(comip_mfp_cfg));
	comip_mfp_config_pull_array(comip_mfp_pull_cfg, ARRAY_SIZE(comip_mfp_pull_cfg));
	comip_mfp_config_ds_array(comip_ds_cfg, ARRAY_SIZE(comip_ds_cfg));
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

#if defined(CONFIG_COMIP_LC1132)

static struct pmic_power_module_map lc1132_power_module_map[] = {
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO3,	PMIC_POWER_CAMERA_AF_MOTOR,	0,	0},
	{PMIC_DLDO4,	PMIC_POWER_USIM,			0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,			1,	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,				0,	0},
	{PMIC_DLDO8,	PMIC_POWER_SDIO,			0,	0},
	{PMIC_DLDO9,	PMIC_POWER_TOUCHSCREEN,		0,	1},
	{PMIC_DLDO10,	PMIC_POWER_LCD_IO,			0,	1},
	{PMIC_DLDO10,	PMIC_POWER_TOUCHSCREEN_IO,	0,	1},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_CORE,		0,	0},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_CORE, 	1,	0},
	{PMIC_ALDO6,	PMIC_POWER_CAMERA_ANALOG,	0,	0},
	{PMIC_ALDO6,	PMIC_POWER_CAMERA_ANALOG,	1,	0},
	{PMIC_ALDO7,	PMIC_POWER_LCD_CORE, 		0,	1},
	{PMIC_ALDO9, 	PMIC_POWER_CAMERA_CSI_PHY,	0,	0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR, 		0, 	0}
};

static struct pmic_power_ctrl_map lc1132_power_ctrl_map[] = {
	{PMIC_ALDO2,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO3,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO6,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2800},
	{PMIC_ALDO7,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO9,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO10,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO11,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1500},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	80},
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
	.flags = LC1132_FLAGS_DEVICE_CHECK,
	.pmic_data = &lc1132_pmic_info,
};

#endif

#if defined(CONFIG_COMIP_LC1160)

static struct pmic_power_module_map lc1160_power_module_map[] = {
	{PMIC_DCDC9,	PMIC_POWER_WLAN_IO,			0,	0},
	{PMIC_DLDO3,	PMIC_POWER_SDIO,			0,	0},
#if defined(CONFIG_MMC_COMIP_IOPOWER)
	{PMIC_ALDO8,	PMIC_POWER_SDIO,			1,	0},
#endif
	{PMIC_DLDO4,	PMIC_POWER_USIM,			0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,			1,	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,				0,	0},
	{PMIC_DLDO7,	PMIC_POWER_LCD_CORE,		0,	1},
	{PMIC_DLDO8,	PMIC_POWER_TOUCHSCREEN,		0,	1},
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		0,	0},
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		1,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_AF_MOTOR,	0,	0},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,	0,	0},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,	1,	0},
	{PMIC_ALDO10,	PMIC_POWER_LCD_IO,			0,	1},
	{PMIC_ALDO10,	PMIC_POWER_TOUCHSCREEN_IO,	0,	1},
	{PMIC_ALDO10,	PMIC_POWER_CAMERA_CSI_PHY,	0,	1},
	{PMIC_ALDO11,	PMIC_POWER_GPS_CORE,		0,	0},
	{PMIC_ALDO12,	PMIC_POWER_USB,				1,	0},
	{PMIC_ALDO14,	PMIC_POWER_WLAN_CORE,		0,	0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR,		0,	0},
	{PMIC_ISINK2,	PMIC_POWER_LED, PMIC_LED_FLASH, 0}
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
	/* DCDC9 control by coscen */
	{LC1160_REG_DCLDOCONEN, 0x01, LC1160_REG_BITMASK_DC9CONEN},
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
	/* Current sink for flash add by Caoshichao */
        {LC1160_REG_VIBI_FLASHIOUT,0x00, LC1160_REG_BITMASK_FLASHIOUT}   
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

#if defined(CONFIG_SENSORS_MXC400X) || defined(CONFIG_SENSORS_MXC400X_STATIC)
static struct mxc400x_platform_data mxc400x_pdata = {
	.poll_interval = 200,
	.min_interval = 100,
	.init = NULL,
	.exit = NULL,
	.power_on = NULL,
	.power_off = NULL,
};
#endif



#if defined(CONFIG_UNISCOPE_TOUCHSCREEN)
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
static int ctp_i2c_power(struct device *dev, unsigned int vdd)
{
	if (vdd){
		comip_mfp_config_array(comip_mfp_cfg_i2c_2, ARRAY_SIZE(comip_mfp_cfg_i2c_2));
	}else{
		ts_set_i2c_to_gpio();
	}
	return 0;
}
static int ctp_ic_power(struct device *dev, unsigned int vdd)
{
    /* Set power. */
	if (vdd){
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
	}else{
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}
	return 0;
}

static struct uniscope_ts_info comip_i2c_ctp_info = {
        .power_i2c = ctp_i2c_power,
        .power_ic = ctp_ic_power,
        .irq_gpio = mfp_to_gpio(TS_INT_PIN),
        .rst_gpio = mfp_to_gpio(TS_RST_PIN),
        .power_i2c_flag = 1,
        .power_ic_flag = 1,
};
#endif



/* NFC. */
static struct i2c_board_info comip_i2c0_board_info[] = {
};

/* Sensors. */
static struct i2c_board_info comip_i2c1_board_info[] = {
	// TODO:  to be continue for device
	/* als+ps */
#if defined(CONFIG_LIGHT_PROXIMITY_EPL259X)
		{
			.type = "epl259x",
			.addr = 0x49,
			.irq = gpio_to_irq(LIGHT_INT),
		},
#endif	
	/*acc*/
#if defined(CONFIG_SENSORS_MXC400X) || defined(CONFIG_SENSORS_MXC400X_STATIC)
	{
		.type = "mxc400x",
		.addr = 0x15,
		.platform_data = &mxc400x_pdata,
		//.irq = gpio_to_irq(ACCELER_INT_PIN),
	},
#endif

};

/* TouchScreen. */
static struct i2c_board_info comip_i2c2_board_info[] = {
	// TODO:  to be continue for device
#if defined(CONFIG_UNISCOPE_TOUCHSCREEN)
	{
		.type = "Uniscope-Ts",
		.addr = 0x38,
		.platform_data = &comip_i2c_ctp_info,
	},
#endif
};

/* PMU&CODEC. */
static struct i2c_board_info comip_i2c4_board_info[] = {
	// TODO:  to be continue for device
#if defined(CONFIG_COMIP_LC1132)
	{
		.type = "lc1132",
		.addr = 0x33,
		.platform_data = &i2c_lc1132_info,
	},
#endif
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
	.flags = COMIP_I2C_STANDARD_MODE,
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
		for(index = 0; index < sizeof(lc1160_power_ctrl_map) / sizeof(lc1160_power_ctrl_map[0]); index++) {

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

#define LCD_DETECT_ADC_CHANNEL	(4)
static int comip_lcd_detect_dev(void)
{    
    int voltage;
	int lcd_id = 0;
	
	voltage = pmic_get_adc_conversion(LCD_DETECT_ADC_CHANNEL);
	printk(KERN_EMERG"comip_lcd_detect_dev : %d mv\n", voltage);


	if (voltage <= 300) 
		return LCD_ID_BY_OTM8019A; // adc = 0.1
	else if(voltage > 300 && voltage <= 800)
		return LCD_ID_SH1286_CTC5; // 500
	else if (voltage > 800 && voltage <= 1600)
		return LCD_ID_HT_HX8389C;// 1220
	else if(voltage > 1600 && voltage <= 2000)
		return LCD_ID_XLD_NT35512S; // 1800	
	else 
		return LCD_ID_SH1286_CTC5;

}

static struct comipfb_platform_data comip_lcd_info = {
	.lcdcaxi_clk = 312000000,
	.lcdc_support_interface = COMIPFB_MIPI_IF,
	.phy_ref_freq = 26000,	/* KHz */
	.gpio_rst = LCD_RESET_PIN,
	.gpio_te = LCD_TE,
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

static int bcamera_pwdn = mfp_to_gpio(BCAMERA_POWERDOWN_PIN);
static int bcamera_rst = mfp_to_gpio(BCAMERA_RESET_PIN);
static int fcamera_pwdn = mfp_to_gpio(FCAMERA_POWERDOWN_PIN);
static int fcamera_rst = mfp_to_gpio(FCAMERA_RESET_PIN);
//static int ext_avdd = mfp_to_gpio(CAMERA_LDO_PIN);

//**************** Back Camera ************
#if defined(CONFIG_VIDEO_GC2155_BACK) //Lefeng temp use 200W 2lane
static int gc2155_back_power(int onoff)
{
	printk("############## GC2155 Bcak power : %d################\n", onoff);

	gpio_request(bcamera_pwdn, "GC2155B Powerdown");
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(bcamera_pwdn, 0);
		mdelay(50);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(bcamera_pwdn, 1);
	}
	gpio_free(bcamera_pwdn);
	return 0;
}

static int gc2155_back_reset(void)
{
	printk("############## GC2155 Back reset################\n");

	gpio_request(bcamera_rst, "GC2155B Reset");
	gpio_direction_output(bcamera_rst, 1);
	mdelay(50);
	gpio_direction_output(bcamera_rst, 0);
	mdelay(50);
	gpio_direction_output(bcamera_rst, 1);
	mdelay(50);
	gpio_free(bcamera_rst);

	return 0;
}
#endif

#if defined(CONFIG_VIDEO_GC2235) //JinXing 200W -back camera
static int gc2235_power(int onoff)
{
    printk("############## GC2235 power : %d################\n", onoff);

    gpio_request(bcamera_pwdn, "GC2235 Powerdown");
    gpio_request(bcamera_rst, "GC2235 Reset");
    if (onoff) {
        pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);

        gpio_direction_output(bcamera_pwdn, 0);
        udelay(50);
    } else {
        gpio_direction_output(bcamera_pwdn, 1);
        udelay(50);
//        gpio_direction_output(bcamera_rst, 0);
//        udelay(50);

        pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

    }

    gpio_free(bcamera_pwdn);
    gpio_free(bcamera_rst);
    
    return 0;
}

static int gc2235_reset(void)
{
    printk("############## GC2235 reset################\n");
    gpio_request(bcamera_rst, "GC2235 Reset");
    gpio_direction_output(bcamera_rst, 1);
    udelay(50);
    gpio_direction_output(bcamera_rst, 0);
    udelay(50);
    gpio_direction_output(bcamera_rst, 1);
    udelay(50);

    gpio_free(bcamera_rst);
    return 0;
}
#endif

#if defined(CONFIG_VIDEO_OV5648_2LANE_26M) //QuanHe & Lefeng(High) 500W -Back camera
static int ov5648_power(int onoff)
{
	printk("############## OV5648 power : %d################\n", onoff);

	gpio_request(bcamera_pwdn, "ov5648 Powerdown");
	gpio_request(bcamera_rst, "ov5648 Reset");

	if (onoff) {
		gpio_direction_output(bcamera_rst, 1);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(2);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(bcamera_pwdn, 1);
		gpio_direction_output(bcamera_rst, 0);
		mdelay(50);
        gpio_direction_output(bcamera_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_output(bcamera_pwdn, 0);
		gpio_direction_output(bcamera_rst, 0);

	}
	gpio_free(bcamera_pwdn);
	gpio_free(bcamera_rst);
	return 0;
}

static int ov5648_reset(void)
{
	printk("############## OV5648 reset################\n");
	return 0;
}
#endif

#if defined(CONFIG_VIDEO_GC2145_BACK) //LeFeng (Low) 200W - Back Camera
static int gc2145_back_power(int onoff)
{
    	printk("############## GC2145 Back power : %d################\n", onoff);
    
    	gpio_request(bcamera_pwdn, "GC2145B Powerdown");
	gpio_request(bcamera_rst, "GC2145B Reset");
	if (onoff) {
		gpio_direction_output(bcamera_pwdn, 1);
		gpio_direction_output(bcamera_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(bcamera_pwdn, 0);
		mdelay(10);
		gpio_direction_output(bcamera_rst, 0);
		mdelay(50);
		gpio_direction_output(bcamera_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_output(bcamera_pwdn, 0);
		gpio_direction_output(bcamera_rst, 0);
	}
	gpio_free(bcamera_pwdn);
	gpio_free(bcamera_rst);
	return 0;
}

static int gc2145_back_reset(void)
{
    printk("############## GC2145 Back reset################\n");
    return 0;
}
#endif

//*****************  Front Camera ******************

#if defined(CONFIG_VIDEO_GC2145_FRONT) //LeFeng (high) 200W -Front Camera
static int gc2145_front_power(int onoff)
{
    	printk("############## GC2145 Front power : %d################\n", onoff);

    	gpio_request(fcamera_pwdn, "GC2145F Powerdown");
	gpio_request(fcamera_rst, "GC2145F Reset");
	if (onoff) {
		gpio_direction_output(fcamera_pwdn, 1);
		gpio_direction_output(fcamera_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(fcamera_pwdn, 0);
		mdelay(10);
		gpio_direction_output(fcamera_rst, 0);
		mdelay(50);
		gpio_direction_output(fcamera_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_output(fcamera_pwdn, 0);
		gpio_direction_output(fcamera_rst, 0);
	}
	gpio_free(fcamera_pwdn);
	gpio_free(fcamera_rst);

    	return 0;
}

static int gc2145_front_reset(void)
{
    printk("############## GC2145 Front reset################\n");
    return 0;
}
#endif

#if defined(CONFIG_VIDEO_GC2155_FRONT) // QuanHe 200W - Front camera - 1lane
static int gc2155_power(int onoff)
{
	printk("############## GC2155 power : %d################\n", onoff);

	gpio_request(fcamera_pwdn, "GC2155 Powerdown");
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(fcamera_pwdn, 0);
		mdelay(50);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(fcamera_pwdn, 1);
	}

	gpio_free(fcamera_pwdn);
	return 0;
}

static int gc2155_reset(void)
{
	printk("############## GC2155 reset################\n");

	gpio_request(fcamera_rst, "GC2155 Reset");
	gpio_direction_output(fcamera_rst, 1);
	mdelay(50);
	gpio_direction_output(fcamera_rst, 0);
	mdelay(50);
	gpio_direction_output(fcamera_rst, 1);
	mdelay(50);
	gpio_free(fcamera_rst);

	return 0;
}
#endif

#if defined(CONFIG_VIDEO_GC2755)
static int gc2755_power(int onoff)
{
	printk("############## GC2755 Back power : %d################\n", onoff);
    
    gpio_request(bcamera_pwdn, "GC2755 Powerdown");
	gpio_request(bcamera_rst, "GC2755 Reset");
	if (onoff) {
		gpio_direction_output(bcamera_pwdn, 1);
		gpio_direction_output(bcamera_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(bcamera_pwdn, 0);
		mdelay(10);
		gpio_direction_output(bcamera_rst, 0);
		mdelay(50);
		gpio_direction_output(bcamera_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_output(bcamera_pwdn, 0);
		gpio_direction_output(bcamera_rst, 0);
	}
	gpio_free(bcamera_pwdn);
	gpio_free(bcamera_rst);
	
	return 0;
}

static int gc2755_reset(void)
{
#if 0
	printk("############## gc2755 reset################\n");

	gpio_request(bcamera_rst, "GC2755 Reset");
	gpio_direction_output(bcamera_rst, 1);
	mdelay(50);
	gpio_direction_output(bcamera_rst, 0);
	mdelay(50);
	gpio_direction_output(bcamera_rst, 1);
	mdelay(50);
	//gpio_free(gc2155_rst);
#endif 

	return 0;
}

/*static struct i2c_board_info gc2755_board_info = {
	.type = "gc2755",
	.addr = 0x3c,
};*/
#endif

#if defined(CONFIG_VIDEO_GC0310) // JinXing & LeFeng(low) 30W -Front Camera
static int gc0310_power(int onoff)
{
	printk("############## GC0310 power : %d################\n", onoff);

	gpio_request(fcamera_rst, "gc0310 Reset");
	gpio_request(fcamera_pwdn, "gc0310 Powerdown");
	if (onoff) {
		gpio_direction_output(fcamera_rst, 1);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(2);
		gpio_direction_output(fcamera_pwdn, 0);
		gpio_direction_output(fcamera_rst, 0);
		mdelay(50);
		gpio_direction_output(fcamera_rst, 1);
		mdelay(50);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_output(fcamera_pwdn, 1);
		gpio_direction_output(fcamera_rst, 0);
	}
	gpio_free(fcamera_rst);
	gpio_free(fcamera_pwdn);
	return 0;
}

static int gc0310_reset(void)
{
	printk("############## GC0310 reset################\n");
	return 0;
}
#endif

static int camera_flash(enum camera_led_mode mode, int onoff)
{

    if (mode ==  FLASH) {
        pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_FLASH, 100);
    } else if (mode == TORCH) {
        pmic_voltage_set(PMIC_POWER_LED,PMIC_LED_FLASH, 60); // 60 mA
    } else {
        printk(KERN_ERR "Wrong Flash mode!\n");
        pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_FLASH, 0);
    }

    if (onoff) {
        pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_FLASH,PMIC_POWER_VOLTAGE_ENABLE);
    } else {
        pmic_voltage_set(PMIC_POWER_LED, PMIC_LED_FLASH, PMIC_POWER_VOLTAGE_DISABLE);
    }

	return 0;
}

static struct comip_camera_subdev_platform_data ov5648_setting = {
	.flags = CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info bcamera_board_info[] = {
	{
		.type = "ov5648-mipi-raw",
		.addr = 0x36,
		.platform_data = &ov5648_setting,
	},
	{
		.type = "gc2235",
		.addr = 0x3c,
	},
	{
		.type = "gc2145-back",
		.addr = 0x3c,
	},
	{
		.type = "gc2155-back",
		.addr = 0x3c,
	 },
     {
        .type = "gc2755",
        .addr = 0x3c, 
    },

};
static struct i2c_board_info fcamera_board_info[] = {
	 {
            .type = "gc0310",
            .addr = 0x21,
        },
	 {
            .type = "gc2155-front",
            .addr = 0x3c,
	 },
	 {
            .type = "gc2145-front",
            .addr = 0x3c,
	 },
};


static struct comip_camera_client comip_camera_clients[] = {
#if defined(CONFIG_VIDEO_OV5648_2LANE_26M) 
	{
		.board_info = &bcamera_board_info[0], //ov5648
		.flags = CAMERA_CLIENT_IF_MIPI
				|CAMERA_CLIENT_CLK_EXT
				|CAMERA_CLIENT_ISP_CLK_HIGH,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FLASH
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE
				//| CAMERA_CAP_ROTATION_0
				| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 0,
		.reg = 0x300b,
		.reg_value = 0x48,
		.mipi_lane_num = 2,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = ov5648_power,
		.reset = ov5648_reset,
		.flash = camera_flash,
	},
#endif
#if defined(CONFIG_VIDEO_GC2235)
	{
		.board_info = &bcamera_board_info[1], //gc2235
		.flags = CAMERA_CLIENT_IF_MIPI
				| CAMERA_CLIENT_I2C_ADDR_8,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.reg = 0xf1,
		.reg_value = 0x35,
		.mipi_lane_num = 2,
		.power = gc2235_power,
		.reset = gc2235_reset,
		.flash = camera_flash,
	},
#endif
#if defined(CONFIG_VIDEO_GC2145_BACK) 
	{
		.board_info = &bcamera_board_info[2], //gc2145-back
		.flags =  CAMERA_CLIENT_IF_MIPI
				|CAMERA_CLIENT_CLK_EXT
				|CAMERA_CLIENT_ISP_CLK_HIGH
				|CAMERA_CLIENT_YUV_DATA
				| CAMERA_CLIENT_I2C_ADDR_8,
		.caps = CAMERA_CAP_FLASH
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_ROTATION_0,
		.if_id = 0,
		.reg = 0xf1,
		.reg_value = 0x45,
		.mipi_lane_num = 2,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = gc2145_back_power,
		.reset = gc2145_back_reset,
		.flash = camera_flash,
	},
#endif
#if defined(CONFIG_VIDEO_GC2155_BACK)
	{
		.board_info = &bcamera_board_info[3], //gc2155-back
		.flags = CAMERA_CLIENT_IF_MIPI
				|CAMERA_CLIENT_CLK_EXT
				|CAMERA_CLIENT_ISP_CLK_HIGH
				|CAMERA_CLIENT_YUV_DATA
				| CAMERA_CLIENT_I2C_ADDR_8,
		.caps = CAMERA_CAP_FLASH
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_ROTATION_0,
		.if_id = 0,
		.reg = 0xf1,
		.reg_value = 0x55,
		.mipi_lane_num = 2,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = gc2155_back_power,
		.reset = gc2155_back_reset,
		.flash = camera_flash,
	},
#endif
// Front Camera
#if defined(CONFIG_VIDEO_GC2755)
	{
		.board_info = &bcamera_board_info[4],
		.flags = CAMERA_CLIENT_IF_MIPI
				| CAMERA_CLIENT_CLK_EXT
				| CAMERA_CLIENT_ISP_CLK_HIGH
				| CAMERA_CLIENT_FRAMERATE_DYN
				| CAMERA_CLIENT_I2C_ADDR_8,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FLASH
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ROTATION_180
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = gc2755_power,
		.reset = gc2755_reset,
		.flash = camera_flash,
	},
#endif
#if defined(CONFIG_VIDEO_GC0310)
	{
		.board_info = &fcamera_board_info[0], //gc0310
		.flags = CAMERA_CLIENT_IF_MIPI
				| CAMERA_CLIENT_YUV_DATA
				| CAMERA_CLIENT_I2C_ADDR_8,
		.caps = CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 1,
		.reg = 0xf0,
		.reg_value = 0xa3,
		.mipi_lane_num = 1,
		.power = gc0310_power,
		.reset = gc0310_reset,
	},
#endif
#if defined(CONFIG_VIDEO_GC2155_FRONT)
	{
		.board_info = &fcamera_board_info[1], //gc2155-front
		.flags = CAMERA_CLIENT_IF_MIPI
				| CAMERA_CLIENT_YUV_DATA
				| CAMERA_CLIENT_I2C_ADDR_8,
		.caps = CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 1,
		.reg = 0xf1,
		.reg_value = 0x55,
		.mipi_lane_num = 1,
		.power = gc2155_power,
		.reset = gc2155_reset,
	},
#endif
#if defined(CONFIG_VIDEO_GC2145_FRONT)
	{
		.board_info = &fcamera_board_info[2], //gc2145-front
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_YUV_DATA
			| CAMERA_CLIENT_I2C_ADDR_8,
		.caps = CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 1,
		.reg = 0xf1,
		.reg_value = 0x45,
		.mipi_lane_num = 1,
		.power = gc2145_front_power,
		.reset = gc2145_front_reset,
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
		.snapshot_pre_duration = 400,
		.aecgc_stable_duration = 3500,
		.torch_off_duration = 100,
		.flash_on_ramp_time = 150,
		.mean_percent = 50,
		.brightness_ratio =10,//32
	},
	.mem_delayed_release = {
		.delayed_release_duration = 0, //10 minutes
		.memfree_threshold = 0,//40 MB
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
}
static void __init comip_init_board(void)
{
	comip_init_camera();
	comip_init_devices();
	comip_init_misc();
//	comip_init_ts_virtual_key();
//  console_verbose_uni(15);
}

