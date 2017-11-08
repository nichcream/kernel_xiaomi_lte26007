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
** 1.0.0	2014-1-23	xuxuefeng	created
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
#if defined(CONFIG_COMIP_LC1132)
#include <plat/lc1132.h>
#include <plat/lc1132-pmic.h>
#include <plat/lc1132_adc.h>
#endif
#if defined(CONFIG_COMIP_LC1160)
#include <plat/lc1160.h>
#include <plat/lc1160-pmic.h>
#include <plat/lc1160_adc.h>
#include <plat/lc1160-pwm.h>
#endif

#if defined(CONFIG_SENSORS_BMA222E)
#include <plat/bma2x2.h>
#endif

#if defined(CONFIG_LIGHT_PROXIMITY_GP2AP030)
#include <plat/gp2ap030.h>
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_RPR0521)
#include <plat/rpr0521_driver.h>
#endif
#if defined(CONFIG_SENSORS_KIONIX_ACCEL)
#include <plat/kionix_accel.h>
#endif
#if defined(CONFIG_SENSORS_MMA8X5X)
#include <plat/mma8x5x.h>
#endif
#if defined(CONFIG_SENSORS_ST480)
#include <plat/st480.h>
#endif
#if defined(CONFIG_SENSORS_MMC3416XPJ_MAG)
#include <plat/mmc3416x.h>
#endif
#if defined(CONFIG_SENSORS_L3GD20)
#include <plat/l3gd20.h>
#endif
#if defined(CONFIG_SENSORS_AK09911)
#include <plat/akm09911.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#include <plat/ft5x06_ts.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS200)
#include <plat/mms_ts.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS438)
#include <plat/melfas_mms.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_BF686x)
#include <plat/bf686x_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_BF67xx)
#include <plat/bf67xx_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_BF675x)
#include <plat/bf675x_ts.h>
#endif
#ifdef CONFIG_CW201X_BATTERY
#include <plat/cw201x_battery.h>
#endif
#if defined(CONFIG_SENSORS_QMC6983)
#include <plat/qmc6983.h>
#endif
#include "board-fivemode.h"

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

#ifdef CONFIG_CHARGER_BQ24158

struct extern_charger_platform_data exchg_data = {
	.max_charger_currentmA  = 1250,
	.max_charger_voltagemV  = 4200,
	.termination_currentmA  = 50,
	.dpm_voltagemV          = 4360,
	.usb_cin_limit_currentmA= 500,
	.usb_battery_currentmA  = 550,
	.ac_cin_limit_currentmA = 900,
	.ac_battery_currentmA   = 900,
};
#endif

#if defined(CONFIG_CHARGER_NCP1851)
struct extern_charger_platform_data exchg_data = {
	.max_charger_voltagemV  = 4200,
	.termination_currentmA  = 100,
	.usb_cin_limit_currentmA= 500,
	.usb_battery_currentmA  = 500,
	.ac_cin_limit_currentmA = 900,
	.ac_battery_currentmA   = 900,
};
#endif

#if defined(CONFIG_BATTERY_MAX17058)
static int batt_temp_table[] = {
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
/*max17058 fuel gauge model data table*/
/*
* NOTE:  don't change this table without updating the
*  max1705_battery.c defines for *battery_id
* so they continue to match the order in this table.
*/
static int fg_batt_model_table[64] = {
	/*default (data_baseaddr = 0)*/
	0xAE,0xF0,0xB7,0x70,0xB8,0x80,0xB9,0xE0,
	0xBB,0x70,0xBC,0x50,0xBD,0x30,0xBE,0x60,
	0xBF,0xB0,0xBF,0xD0,0xC0,0xB0,0xC3,0xC0,
	0xC9,0x90,0xCF,0xE0,0xD4,0x60,0xD6,0x50,
	0x04,0x00,0x2F,0x60,0x22,0x20,0x2A,0x00,
	0x38,0x00,0x36,0x00,0x30,0xD0,0x27,0xF0,
	0x65,0xC0,0x0B,0xF0,0x1C,0xE0,0x0F,0xF0,
	0x0F,0x30,0x12,0x60,0x07,0x30,0x07,0x30,
};

struct comip_fuelgauge_info fg_data = {
	.battery_tmp_tbl    = batt_temp_table,
	.tmptblsize = ARRAY_SIZE(batt_temp_table),
	.fg_model_tbl = fg_batt_model_table,
	.fgtblsize = ARRAY_SIZE(fg_batt_model_table),
};
#endif
#ifdef CONFIG_CW201X_BATTERY
static int batt_temp_table[] = {
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

/*
   note the follow array must set depend on the battery that you use
   you must send the battery to cellwise-semi the contact information:
   name: chen gan; tel:13416876079; E-mail: ben.chen@cellwise-semi.com
 */
static u8 config_info[SIZE_BATINFO] = {
	0x15, 0x42, 0x60, 0x59, 0x52,
	0x58, 0x4D, 0x48, 0x48, 0x44,
	0x44, 0x46, 0x49, 0x48, 0x32,
	0x24, 0x20, 0x17, 0x13, 0x0F,
	0x19, 0x3E, 0x51, 0x45, 0x08,
	0x76, 0x0B, 0x85, 0x0E, 0x1C,
	0x2E, 0x3E, 0x4D, 0x52, 0x52,
	0x57, 0x3D, 0x1B, 0x6A, 0x2D,
	0x25, 0x43, 0x52, 0x87, 0x8F,
	0x91, 0x94, 0x52, 0x82, 0x8C,
	0x92, 0x96, 0xFF, 0x7B, 0xBB,
	0xCB, 0x2F, 0x7D, 0x72, 0xA5,
	0xB5, 0xC1, 0x46, 0xAE
};

static struct cw_bat_platform_data cw_bat_platdata = {
	.bat_low_pin    = 0,
	.battery_tmp_tbl	= batt_temp_table,
	.tmptblsize = ARRAY_SIZE(batt_temp_table),
	.cw_bat_config_info     = config_info,
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
}

static struct mfp_pin_cfg comip_mfp_cfg[] = {
	// TODO:  to be continue for device
#if defined(CONFIG_COMIP_LC1132)
	/* LC1132. */
	{LC1132_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_COMIP_LC1160)
	/* LC1160. */
	{LC1160_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_BATTERY_MAX17058)
	{MAX17058_FG_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_CHARGER_NCP1851)||defined(CONFIG_CHARGER_BQ24158)
	{EXTERN_CHARGER_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif
	/*KEYPAD LED*/
	{KEYPAD_LED_PIN, 		MFP_PIN_MODE_GPIO},
	/* LCD. */
	{LCD_RESET_PIN,			MFP_PIN_MODE_GPIO},
#if defined(CONFIG_LCD_TRULY_NT35595)||defined(CONFIG_LCD_JD_9161CPT5) ||\
	defined(CONFIG_LCD_JD_9261AA)||defined(CONFIG_LCD_JD_9365HSD)
	{LCD_AVDD_EN_PIN,		MFP_PIN_MODE_GPIO},
	{LCD_AVEE_EN_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_LIGHT_PROXIMITY_GP2AP030)
	{GP2AP030_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	{FT5X06_RST_PIN,                MFP_PIN_MODE_GPIO},
	{FT5X06_INT_PIN,                MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS200)
	{MMS200_RST_PIN,                MFP_PIN_MODE_GPIO},
	{MMS200_INT_PIN,                MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS438)
	{MMS400_RST_PIN,                MFP_PIN_MODE_GPIO},
	{MMS400_INT_PIN,                MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_BF686x)||defined(CONFIG_TOUCHSCREEN_BF67xx)||\
	defined(CONFIG_TOUCHSCREEN_BF675x)
	{BF686x_RST_PIN,                MFP_PIN_MODE_GPIO},
	{BF686x_INT_PIN,                MFP_PIN_MODE_GPIO},
#endif

	/*Camera*/
#if defined(CONFIG_VIDEO_OV5648_2LANE_19M)
	{OV5648_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV5648_RESET_PIN,              MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV4689)
	{OV4689_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV4689_RESET_PIN,          MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_VIDEO_OV13850)	/* OV13850. */
	{OV13850_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV13850_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV8865)	/* OV8865. */
	{OV8865_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV8865_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV9760)	/* OV9760. */
	{OV9760_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV9760_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC2355)
	{GC2355_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC2355_RESET_PIN,			MFP_PIN_MODE_GPIO},
	{GC2355_CORE_POWER_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_BF3A20_MIPI)
	/* BF3A20_MIPI. */
	{BF3A20_MIPI_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{BF3A20_MIPI_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_HM5040)
	{HM5040_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{HM5040_RESET_PIN,              MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_SP5408)
	{SP5408_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{SP5408_RESET_PIN,      MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_SP5409)
	{SP5409_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{SP5409_RESET_PIN,      MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_SP2508)
	{SP2508_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{SP2508_RESET_PIN,      MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_SP2529)
	{SP2529_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{SP2529_RESET_PIN,      MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	{FLASHER_EN,			MFP_PIN_MODE_GPIO},
	{FLASHER_ENM,			MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_SENSORS_AK09911)
       {AKM09911_GPIO_RST,     MFP_PIN_MODE_GPIO},
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
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB2)
	{MFP_PIN_GPIO(72),		MFP_PULL_UP},
	{MFP_PIN_GPIO(73),		MFP_PULL_UP},
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	{MFP_PIN_GPIO(74),		MFP_PULL_UP},
        {MFP_PIN_GPIO(75),		MFP_PULL_UP},
#else
	{MFP_PIN_GPIO(72),		MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(73),		MFP_PULL_DISABLE},
#endif
	{MFP_PIN_GPIO(253), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(140), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(169), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(170), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(171), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(172), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(176), 	MFP_PULL_DISABLE},
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
	{MFP_PIN_GPIO(230), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(233), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(234), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(235), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(236), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(237), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(238), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(239), 	MFP_PULL_UP},
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB2)
	{MFP_PIN_GPIO(175),     MFP_PULL_DOWN},
#endif
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB2) || defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	{MFP_PIN_GPIO(151), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(211), 	MFP_PULL_UP},
	{MFP_PIN_GPIO(243), 	MFP_PULL_UP},
#endif
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

#if defined(CONFIG_COMIP_LC1132)

static struct pmic_power_module_map lc1132_power_module_map[] = {
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO3,	PMIC_POWER_CAMERA_AF_MOTOR,	0,	0},
	{PMIC_DLDO4,	PMIC_POWER_USIM,		0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,		1,     	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,			0,	0},
	{PMIC_DLDO8,	PMIC_POWER_SDIO,		0,	0},
	{PMIC_DLDO9,	PMIC_POWER_TOUCHSCREEN,		0,	1},
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
	{PMIC_DCDC9,	PMIC_POWER_WLAN_IO,		0,	0},
	{PMIC_DLDO3,	PMIC_POWER_SDIO,		0,	0},
#if defined(CONFIG_MMC_COMIP_IOPOWER)
	{PMIC_ALDO8,	PMIC_POWER_SDIO,		1,	0},
#endif
	{PMIC_DLDO4,	PMIC_POWER_USIM,		0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,		1,	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,		0,	0},
	{PMIC_DLDO7,	PMIC_POWER_LCD_CORE,		0,	1},
	{PMIC_DLDO8,	PMIC_POWER_TOUCHSCREEN,		0,	1},
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB2)
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		0,	0},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_AF_MOTOR,	0,	0},
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	{PMIC_ALDO13,    PMIC_POWER_CAMERA_CORE,         0,      0},
	{PMIC_ALDO14,	PMIC_POWER_CAMERA_AF_MOTOR,	0,	0},
#endif
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		1,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_AF_MOTOR,		0,	0},
	{PMIC_ALDO4,	PMIC_POWER_RF_GSM_IO,		1,	1},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,		0,	0},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,		1,	0},
	{PMIC_ALDO10,	PMIC_POWER_LCD_IO,		0,	1},
	{PMIC_ALDO10,	PMIC_POWER_TOUCHSCREEN_IO,		0,	1},
	{PMIC_ALDO10,	PMIC_POWER_CAMERA_CSI_PHY,		0,	1},
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB2)
	{PMIC_ALDO11,	PMIC_POWER_GPS_CORE,		0,	0},
	{PMIC_ALDO14,	PMIC_POWER_WLAN_CORE,		0,	0},
#endif
	{PMIC_ALDO12,	PMIC_POWER_USB,		1,	0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR,		0,	0}
};

static struct pmic_power_ctrl_map lc1160_power_ctrl_map[] = {
	{PMIC_DCDC9,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	{PMIC_ALDO13,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
#endif
	{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
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
	{LC1160_REG_DCLDOCONEN, 0x00, LC1160_REG_BITMASK_LDOA4CONEN},
	/* ALDO2 enter ECO mode when sleep */
	{LC1160_REG_LDOA2CR, 0x01, LC1160_REG_BITMASK_LDOA2SLP},
	/* ALDO1/4/5/6/7/8/9/10/11/12 disable when sleep */
	{LC1160_REG_LDOA_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOA5_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE1, 0x00, LC1160_REG_BITMASK_LDOA6_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x01, LC1160_REG_BITMASK_LDOA1_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOA4_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x01, LC1160_REG_BITMASK_LDOA7_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x01, LC1160_REG_BITMASK_LDOA8_ALLOW_IN_SLP},
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

/*ALS+PS*/
#if defined(CONFIG_LIGHT_PROXIMITY_GP2AP030)
static struct gp2ap030_platform_data comip_i2c_gp2ap030_info = {
	.prox_threshold_hi = 0x0b,
	.prox_threshold_lo = 0x0c,
};
#endif
/* accel */
#if defined (CONFIG_SENSORS_MMA8X5X)
static struct mma8x5x_platform_data mma8x5x_pdata = {
	.position = 1,
	.mode = MODE_2G,
};
#endif

#if defined (CONFIG_SENSORS_BMA222E)
static struct bma2x2_platform_data bma2x2_pdata = {
	.position = 7,
};
#endif

#if defined(CONFIG_SENSORS_KIONIX_ACCEL)
static struct kionix_accel_platform_data kionix_accel_pdata = {
	.min_interval = 5,
	.poll_interval = 200,
	.accel_direction = 1,
	.accel_irq_use_drdy = 0,
	.accel_res = KIONIX_ACCEL_RES_12BIT,
	.accel_g_range = KIONIX_ACCEL_G_2G,
};
#endif
/* compass */
#if defined (CONFIG_SENSORS_ST480)
static struct platform_data_st480 st480_pdata = {
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 1,
};
#endif
#if defined (CONFIG_SENSORS_MMC3416XPJ_MAG)
static struct platform_data_mmc3416x mmc3416x_pdata = {
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 1,
};
#endif
#if defined (CONFIG_SENSORS_AK09911)
static int addr_offset(void)
{
	return 0x0d;
}

static char layout_offset(void)
{
	return 0x04;
}

static struct akm09911_platform_data akm09911_pdata = {
	.layout = layout_offset,
	.addr_amend = addr_offset,
	.gpio_RSTN = mfp_to_gpio(AKM09911_GPIO_RST),
};
#endif
#if defined (CONFIG_SENSORS_QMC6983)
static struct QMC6983_platform_data qmc6983_pdata = {
	.h_range = 0,
	.negate_x = 1,
	.negate_y = 1,
	.negate_z = 1,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
};
#endif
/*gyroscope*/
#if defined(CONFIG_SENSORS_L3GD20)
static struct l3gd20_gyr_platform_data l3gd20_gyr_pdata = {
	.fs_range = L3GD20_GYR_FS_2000DPS,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,

	.poll_interval = 10,
	.min_interval = 10

	//.gpio_int1 = L3GD20_GYR_DEFAULT_INT1_GPIO,
	//.gpio_int2 = L3GD20_GYR_DEFAULT_INT2_GPIO,
};
#endif

static ssize_t virtual_key_show(struct kobject *kobj,
                                struct kobj_attribute *attr, char *buf)
{
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB)
	/**1080p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":200:2000:150:200"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":530:2000:150:200"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":870:2000:150:200"
	               "\n");
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB2) || defined (CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	/**720p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":80:1340:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":360:1340:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":640:1340:100:90"
	               "\n");
#endif
#elif defined(CONFIG_TOUCHSCREEN_MELFAS_MMS200)
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB)
	/**1080p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":200:2000:150:200"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":530:2000:150:200"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":870:2000:150:200"
	               "\n");
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB2) || defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	/**720p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":80:1340:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":360:1340:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":640:1340:100:90"
	               "\n");
#endif
#elif defined(CONFIG_TOUCHSCREEN_MELFAS_MMS438)
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB)
	/**1080p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":200:2000:150:200"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":530:2000:150:200"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":870:2000:150:200"
	               "\n");
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB2) || defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	/**720p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":80:1340:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":360:1340:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":640:1340:100:90"
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
#elif defined(CONFIG_TOUCHSCREEN_MELFAS_MMS200)
		.name = "virtualkeys.mms200",
#elif defined(CONFIG_TOUCHSCREEN_MELFAS_MMS438)
		.name = "virtualkeys.mms400",
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


/*i2c gpio set for LC181x touchscreen*/
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
        .reset_gpio = mfp_to_gpio(FT5X06_RST_PIN),
	.irq_gpio = mfp_to_gpio(FT5X06_INT_PIN),
	.power_i2c_flag = 1,
	.power_ic_flag = 1,
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB)
	.max_scrx = 1080,
	.max_scry = 1920,
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB2) || defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	.max_scrx = 720,
	.max_scry = 1280,
#endif
	.virtual_scry = 2200,
	.max_points = 5,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS200)
static int mms200_rst = MMS200_RST_PIN;
static int mms200_int = mfp_to_gpio(MMS200_INT_PIN);
static int mms200_i2c_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd) {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int mms200_ic_power(struct device *dev, unsigned int vdd)
{
	int retval = 0;
	int gpio_rst = MMS200_RST_PIN;
	int gpio_intr = MMS200_INT_PIN;
	/* Set power. */
	if (vdd) {
		/*add for power on sequence*/
		retval = gpio_request(gpio_rst, "mms200 Reset");
		if(retval) {
			pr_err("mms200_ts request rst error\n");
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
		retval = gpio_request(gpio_rst, "mms200 Reset");
		if(retval) {
			pr_err("mms200_ts request rst error\n");
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

static int mms200_reset(struct device *dev)
{
	gpio_request(mms200_rst, "mms200 Reset");
	gpio_direction_output(mms200_rst, 1);
	msleep(2);
	gpio_direction_output(mms200_rst, 0);
	msleep(10);
	gpio_direction_output(mms200_rst, 1);
	msleep(200);
	gpio_free(mms200_rst);

	return 0;
}

static struct mms_ts_platform_data comip_i2c_mms200_info = {
	.reset = mms200_reset,
	.power_i2c = mms200_i2c_power,
	.power_ic = mms200_ic_power,
	.gpio_resetb = mfp_to_gpio(MMS200_INT_PIN),
	.power_i2c_flag = 1,
	.power_ic_flag = 1,
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB)
	.max_scrx = 720,
	.max_scry = 1280,
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB2) || defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	.max_scrx = 720,
	.max_scry = 1280,
#endif
	.virtual_scry = 2200,
	.max_points = 5,
};
#endif
#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS438)
static int mms400_rst = MMS400_RST_PIN;
static int mms400_int = mfp_to_gpio(MMS400_INT_PIN);
static int mms400_i2c_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd) {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int mms400_ic_power(struct device *dev, unsigned int vdd)
{
	int retval = 0;
	int gpio_rst = MMS400_RST_PIN;

	/* Set power. */
	if (vdd) {
		/*add for power on sequence*/
		retval = gpio_request(gpio_rst, "mms400 Reset");
		if(retval) {
			pr_err("mms400_ts request rst error\n");
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
		retval = gpio_request(gpio_rst, "mms400 Reset");
		if(retval) {
			pr_err("mms400_ts request rst error\n");
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

static int mms400_reset(struct device *dev)
{
	gpio_request(mms400_rst, "mms400 Reset");
	gpio_direction_output(mms400_rst, 1);
	msleep(2);
	gpio_direction_output(mms400_rst, 0);
	msleep(10);
	gpio_direction_output(mms400_rst, 1);
	msleep(200);
	gpio_free(mms400_rst);

	return 0;
}

static struct mms_platform_data comip_i2c_mms438_info = {
	.reset = mms400_reset,
	.power_i2c = mms400_i2c_power,
	.power_ic = mms400_ic_power,
	.gpio_intr = mfp_to_gpio(MMS400_INT_PIN),
	.power_i2c_flag = 1,
	.power_ic_flag = 1,
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB)
	.max_scrx = 1080,
	.max_scry = 1920,
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB2) || defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	.max_scrx = 720,
	.max_scry = 1280,
#endif
	.virtual_scry = 2200,
	.max_points = 5,
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
};

/* Sensors. */
static struct i2c_board_info comip_i2c1_board_info[] = {
	// TODO:  to be continue for device
	/* als+ps */
#if defined(CONFIG_LIGHT_PROXIMITY_GP2AP030)
	{
		.type = "gp2ap030",
		.addr = 0x39,
		.platform_data = &comip_i2c_gp2ap030_info,
		.irq = gpio_to_irq(GP2AP030_INT_PIN),
	},
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_RPR521)
		{
		.type = "rpr521",
		.addr = 0x38,
		.irq = gpio_to_irq(RPR521_INT_PIN),
		},
#endif

	/* accel */
#if defined(CONFIG_SENSORS_MMA8X5X)
	{
		I2C_BOARD_INFO("mma8x5x", 0x1D),
		.platform_data = &mma8x5x_pdata,
	},
#endif

#if defined(CONFIG_SENSORS_BMA222E)
	{
		.type = "bma2x2",
		.addr = 0x18,
		.platform_data = &bma2x2_pdata,
	},
#endif

#if defined(CONFIG_SENSORS_KIONIX_ACCEL)
	{
		I2C_BOARD_INFO(KIONIX_ACCEL_NAME, KIONIX_ACCEL_I2C_ADDR),
		.platform_data = &kionix_accel_pdata,
		//.irq = gpio_to_irq(GSENSOR_INT_PIN),
	},
#endif
	/*compass*/
#if defined(CONFIG_SENSORS_ST480)
	{
		.type = "st480",
		.addr = 0x0f,
		.platform_data = &st480_pdata,
	},

#endif
#if defined(CONFIG_SENSORS_MMC3416XPJ_MAG)
	{
		I2C_BOARD_INFO("mmc3416x", 0x30),
		.platform_data = &mmc3416x_pdata,
	},
#endif
#if defined(CONFIG_SENSORS_AK09911)
	{
		.type = "akm09911",
		.addr = 0x0d,
		.platform_data = &akm09911_pdata,
	},
#endif
#if defined(CONFIG_SENSORS_QMC6983)
	{
		.type = "qmc6983",
		.addr = 0x02c,
		.platform_data = &qmc6983_pdata,
	},
#endif
	/*gyroscope*/
#if defined(CONFIG_SENSORS_L3GD20)
	{
		I2C_BOARD_INFO("l3gd20_gyr", 0x6a),
		.platform_data = &l3gd20_gyr_pdata,
	},
#endif

#if defined(CONFIG_BATTERY_MAX17058)
	{
		I2C_BOARD_INFO("max17058-battery", 0x36),
		.platform_data = &fg_data,
		.irq = mfp_to_gpio(MAX17058_FG_INT_PIN),
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
#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS200)
	{
		.type = "mms200",
		.addr = 0x48,
		.platform_data = &comip_i2c_mms200_info,
	},

#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS438)
	{
		.type = "mms438",
		.addr = 0x48,
		.platform_data = &comip_i2c_mms438_info,
	},

#endif

#if defined(CONFIG_TOUCHSCREEN_BF686x) || defined(CONFIG_TOUCHSCREEN_BF67xx) ||\
	defined(CONFIG_TOUCHSCREEN_BF675x)
	{
		.type = "bfxxxx_ts",
		.addr = 0x2c,
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
#if defined(CONFIG_BUCK_NCP6335)
	{
		.type = "ncp6335f",
		.addr = 0x1c,
	},
#endif
#if defined(CONFIG_CHARGER_BQ24158)
	{
		I2C_BOARD_INFO("bq24158", 0x6a),
		.platform_data = &exchg_data,
		.irq = mfp_to_gpio(EXTERN_CHARGER_INT_PIN),
	},
#endif

#if defined(CONFIG_CHARGER_NCP1851)
	{
		I2C_BOARD_INFO("ncp1851", 0x36),
		.platform_data = &exchg_data,
		.irq = mfp_to_gpio(EXTERN_CHARGER_INT_PIN),
	},
#endif
#if defined (CONFIG_CW201X_BATTERY)
	{
		.type           = "cw201x",
		.addr           = 0x62,
		.flags          = 0,
		.platform_data  = &cw_bat_platdata,
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
		int index = 0;
		struct pmic_power_ctrl_map *power_ctrl_map_item = NULL;
		printk("comip_init_i2c cpu_is_lc1860_eco1\n");
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

#if defined(CONFIG_LCD_TRULY_NT35595)||defined(CONFIG_LCD_JD_9161CPT5)||\
	defined(CONFIG_LCD_JD_9261AA)||defined(CONFIG_LCD_JD_9365HSD)
static int lcd_avdd_en = mfp_to_gpio(LCD_AVDD_EN_PIN);
static int lcd_avee_en = mfp_to_gpio(LCD_AVEE_EN_PIN);

static int comip_lcd_power(int onoff)
{
	gpio_request(lcd_avdd_en, "LCD AVDD Enable");
	gpio_request(lcd_avee_en, "LCD AVEE Enable");

	if (onoff) {
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(lcd_avdd_en, 1);
		gpio_direction_output(lcd_avee_en, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_output(lcd_avdd_en, 0);
		gpio_direction_output(lcd_avee_en, 0);
	}

	msleep(20);

	gpio_free(lcd_avdd_en);
	gpio_free(lcd_avee_en);

	return 0;
}

static int comip_lcd_detect_dev(void)
{
	#if defined (CONFIG_LCD_JD_9161CPT5)
		return LCD_ID_JD_9161CPT5;
	#elif defined (CONFIG_LCD_JD_9261AA)
		return LCD_ID_JD_9261AA;
	#elif defined (CONFIG_LCD_JD_9365HSD)
		return LCD_ID_JD_9365HSD;
	#elif defined (CONFIG_LCD_TRULY_NT35595)
		return LCD_ID_TRULY_NT35595;
	#endif
}

#elif defined(CONFIG_LCD_HS_NT35517)
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

static int comip_lcd_detect_dev(void)
{
	return LCD_ID_HS_NT35517;
}
#elif defined(CONFIG_LCD_TRULY_H8394A)
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
/*
static int comip_lcd_detect_dev(void)
{
	return LCD_ID_TRULY_H8394A;
}
*/
#endif

static struct comipfb_platform_data comip_lcd_info = {
	.lcdcaxi_clk = 312000000,
	.lcdc_support_interface = COMIPFB_MIPI_IF,
	.phy_ref_freq = 26000,	/* KHz */
	.gpio_rst = LCD_RESET_PIN,
	.gpio_im = -1,
	.flags = COMIPFB_SLEEP_POWEROFF,
	.power = comip_lcd_power,
	.bl_control = comip_lcd_bl_control,
	//.detect_dev = comip_lcd_detect_dev,
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

#if defined(CONFIG_VIDEO_OV13850)
static int ov13850_pwdn = mfp_to_gpio(OV13850_POWERDOWN_PIN);
static int ov13850_rst = mfp_to_gpio(OV13850_RESET_PIN);

static int ov13850_power(int onoff)
{
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB2)
	static int ov13850_power_core;
	printk(" ov13850 power : %d\n", onoff);

	comip_mfp_config(MFP_PIN_GPIO(176), MFP_PIN_MODE_GPIO);
	ov13850_power_core = mfp_to_gpio(176);

	gpio_request(ov13850_pwdn, "OV13850 Powerdown");
	gpio_request(ov13850_power_core, "OV13850 power_core");
	gpio_request(ov13850_rst, "OV13850 Reset");

	if (onoff) {
		gpio_direction_output(ov13850_pwdn, 0);
		gpio_direction_output(ov13850_power_core, 0);
		gpio_direction_output(ov13850_rst, 1);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(ov13850_power_core, 1);
		mdelay(10);
		gpio_direction_output(ov13850_pwdn, 1);//OV13850 different from OV5647
		gpio_direction_output(ov13850_rst, 0);
		mdelay(50);
		gpio_direction_output(ov13850_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov13850_pwdn, 0);
		gpio_direction_output(ov13850_power_core, 0);
		gpio_direction_output(ov13850_rst, 0);
	}
	gpio_free(ov13850_pwdn);
	gpio_free(ov13850_power_core);
	gpio_free(ov13850_rst);
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	printk(" ov13850 power : %d\n", onoff);

	gpio_request(ov13850_pwdn, "OV13850 Powerdown");
	gpio_request(ov13850_rst, "OV13850 Reset");

	if (onoff) {
		gpio_direction_output(ov13850_pwdn, 0);
		gpio_direction_output(ov13850_rst, 1);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(ov13850_pwdn, 1);//OV13850 different from OV5647
		gpio_direction_output(ov13850_rst, 0);
		mdelay(50);
		gpio_direction_output(ov13850_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov13850_pwdn, 0);
		gpio_direction_output(ov13850_rst, 0);
	}
	gpio_free(ov13850_pwdn);
	gpio_free(ov13850_rst);
#endif

	return 0;
}

static int ov13850_reset(void)
{
	printk("ov13850 reset\n");

	return 0;
}

static struct comip_camera_subdev_platform_data ov13850_setting = {
        .flags = CAMERA_SUBDEV_FLAG_MIRROR | CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info ov13850_board_info = {
	.type = "ov13850-mipi-raw",
	.addr = 0x10,
	//.platform_data = &ov13850_setting,
};
#endif
#if defined(CONFIG_VIDEO_OV8865)
static int ov8865_pwdn = mfp_to_gpio(OV8865_POWERDOWN_PIN);
static int ov8865_rst = mfp_to_gpio(OV8865_RESET_PIN);
static int ov8865_power(int onoff)
{
	printk("ov8865 power : %d\n", onoff);

	gpio_request(ov8865_pwdn, "OV8865 Powerdown");
	gpio_request(ov8865_rst, "OV8865 Reset");
	if (onoff) {
		gpio_direction_output(ov8865_pwdn, 0);
		gpio_direction_output(ov8865_rst, 1);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(ov8865_pwdn, 1);//OV8865 different from OV5647
		gpio_direction_output(ov8865_rst, 0);
		msleep(50);
		gpio_direction_output(ov8865_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//	pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov8865_pwdn, 0);
		gpio_direction_output(ov8865_rst, 0);
	}
	gpio_free(ov8865_pwdn);
	gpio_free(ov8865_rst);

	return 0;
}

static int ov8865_reset(void)
{
	printk("ov8865 reset\n");

	return 0;
}

static struct i2c_board_info ov8865_board_info = {
	.type = "ov8865-mipi-raw",
	.addr = 0x36,
};
#endif

#if defined(CONFIG_VIDEO_OV5648_2LANE_19M)
static int ov5648_pwdn = mfp_to_gpio(OV5648_POWERDOWN_PIN);
static int ov5648_rst = mfp_to_gpio(OV5648_RESET_PIN);
static int ov5648_power(int onoff)
{
	printk("ov5648 power : %d\n", onoff);

	gpio_request(ov5648_pwdn, "OV5648 Powerdown");
	gpio_request(ov5648_rst, "OV5648 Reset");
	if (onoff) {
		gpio_direction_output(ov5648_pwdn, 0);
		gpio_direction_output(ov5648_rst, 1);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(ov5648_pwdn, 1);//OV5648 different from OV5647
		gpio_direction_output(ov5648_rst, 0);
		msleep(50);
		gpio_direction_output(ov5648_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov5648_pwdn, 0);
		gpio_direction_output(ov5648_rst, 0);
	}
	gpio_free(ov5648_pwdn);
	gpio_free(ov5648_rst);

	return 0;
}

static int ov5648_reset(void)
{
	printk("ov5648 reset\n");

	return 0;
}

static struct comip_camera_subdev_platform_data ov5648_setting = {
        .flags = CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info ov5648_board_info = {
	.type = "ov5648-mipi-raw",
	.addr = 0x36,
#if defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
	.platform_data = &ov5648_setting,
#endif
};
#endif

#if defined(CONFIG_VIDEO_OV4689)
static int ov4689_pwdn = mfp_to_gpio(OV4689_POWERDOWN_PIN);
static int ov4689_rst = mfp_to_gpio(OV4689_RESET_PIN);

static int ov4689_power(int onoff)
{
	static int ov4689_power_core;
	printk(" ov4689 power : %d\n", onoff);

	comip_mfp_config(MFP_PIN_GPIO(176), MFP_PIN_MODE_GPIO);
	ov4689_power_core = mfp_to_gpio(176);

	gpio_request(ov4689_pwdn, "OV4689 Powerdown");
	gpio_request(ov4689_power_core, "OV4689 power_core");
	gpio_request(ov4689_rst, "OV4689 Reset");

	if (onoff) {
		gpio_direction_output(ov4689_pwdn, 0);
		gpio_direction_output(ov4689_power_core, 0);
		gpio_direction_output(ov4689_rst, 1);

		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(5);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(5);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(ov4689_power_core, 1);
		mdelay(10);
		gpio_direction_output(ov4689_pwdn, 1);//OV4689 different from OV5647
		gpio_direction_output(ov4689_rst, 0);
		mdelay(50);
		gpio_direction_output(ov4689_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov4689_pwdn, 0);
		gpio_direction_output(ov4689_power_core, 0);
		gpio_direction_output(ov4689_rst, 0);
	}
	gpio_free(ov4689_pwdn);
	gpio_free(ov4689_power_core);
	gpio_free(ov4689_rst);

	return 0;
}

static int ov4689_reset(void)
{
	printk("ov4689 reset\n");

	return 0;
}

static struct i2c_board_info ov4689_board_info = {
	.type = "ov4689-mipi-raw",
	.addr = 0x36,
};
#endif

#if defined(CONFIG_VIDEO_GC2355)
static int gc2355_pwdn = mfp_to_gpio(GC2355_POWERDOWN_PIN);
static int gc2355_rst = mfp_to_gpio(GC2355_RESET_PIN);

static int gc2355_power(int onoff)
{
	static int gc2355_power_core;

	printk("gc2355 power : %d\n", onoff);

	comip_mfp_config(MFP_PIN_GPIO(176), MFP_PIN_MODE_GPIO);
	gc2355_power_core = mfp_to_gpio(176);

	gpio_request(gc2355_power_core, "gc2355 power_core");
	gpio_request(gc2355_pwdn, "GC2355 Powerdown");
	gpio_request(gc2355_rst, "GC2355 Reset");
	if (onoff) {
		gpio_direction_output(gc2355_power_core, 0);
		mdelay(50);
		gpio_direction_output(gc2355_pwdn, 0);
		gpio_direction_output(gc2355_rst, 1);
		gpio_direction_output(gc2355_power_core, 1);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);

		mdelay(10);
		gpio_direction_output(gc2355_pwdn, 0);//GC2355 different from OV5647
		mdelay(10);
		gpio_direction_output(gc2355_rst, 0);
		msleep(50);
		gpio_direction_output(gc2355_rst, 1);


	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(gc2355_power_core, 0);

		gpio_direction_output(gc2355_pwdn, 0);
		gpio_direction_output(gc2355_rst, 0);
	}
	gpio_free(gc2355_pwdn);
	gpio_free(gc2355_rst);

	return 0;
}

static int gc2355_reset(void)
{
	printk("gc2355 reset\n");

	return 0;
}

static struct i2c_board_info gc2355_board_info = {
	.type = "gc2355-mipi-raw",
	.addr = 0x3c,
};
#endif


#if defined(CONFIG_VIDEO_OV9760)
static int ov9760_pwdn = mfp_to_gpio(OV9760_POWERDOWN_PIN);
static int ov9760_rst = mfp_to_gpio(OV9760_RESET_PIN);

static int ov9760_power(int onoff)
{
	printk("ov9760 power : %d\n", onoff);

	gpio_request(ov9760_pwdn, "OV9760 Powerdown");
	gpio_request(ov9760_rst, "OV9760 Reset");

	if (onoff) {
		gpio_direction_output(ov9760_pwdn, 0);
		gpio_direction_output(ov9760_rst, 1);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, 1500);

		mdelay(10);
		gpio_direction_output(ov9760_pwdn, 1);
		gpio_direction_output(ov9760_rst, 0);
		mdelay(10);
		gpio_direction_output(ov9760_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov9760_pwdn, 0);
		gpio_direction_output(ov9760_rst, 0);
	}
	gpio_free(ov9760_pwdn);
	gpio_free(ov9760_rst);

	return 0;
}

static int ov9760_reset(void)
{
	printk("ov9760 reset\n");
	return 0;
}

static struct i2c_board_info ov9760_board_info = {
	.type = "ov9760-mipi-raw",
	.addr = 0x10,
};

#endif

#if defined(CONFIG_VIDEO_BF3A20_MIPI)
static int bf3a20_mipi_pwdn = mfp_to_gpio(BF3A20_MIPI_POWERDOWN_PIN);
static int bf3a20_mipi_rst = mfp_to_gpio(BF3A20_MIPI_RESET_PIN);
static int bf3a20_mipi_power(int onoff)
{
	printk("BF3A20_mipi power : %d\n", onoff);

	if (onoff) {
			pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
			pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
			pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
			//pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);   //af  off

			gpio_request(bf3a20_mipi_pwdn, "BF3A20_MIPI Powerdown Pin");
			gpio_direction_output(bf3a20_mipi_pwdn, 0);
			mdelay(50);
			gpio_free(bf3a20_mipi_pwdn);

	} else {
			pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
			pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
			pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
			//pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);  //af  off
			gpio_request(bf3a20_mipi_pwdn, "BF3A20_MIPI Powerdown Pin");

			gpio_direction_output(bf3a20_mipi_pwdn, 1);
			gpio_free(bf3a20_mipi_pwdn);
	}

	return 0;
}
static int bf3a20_mipi_reset(void)
{
	printk("BF3A20_MIPI reset\n");
	gpio_request(bf3a20_mipi_rst, "BF3A20_MIPI Reset");
	gpio_direction_output(bf3a20_mipi_rst, 1);
	mdelay(50);
	gpio_direction_output(bf3a20_mipi_rst, 0);
	mdelay(50);
	gpio_direction_output(bf3a20_mipi_rst, 1);
	mdelay(50);
	gpio_free(bf3a20_mipi_rst);
	return 0;
}

static struct i2c_board_info bf3a20_mipi_board_info = {
	.type = "bf3a20_mipi",
	.addr = 0x6e,
};
#endif

#if defined(CONFIG_VIDEO_HM5040)
static int hm5040_pwdn = mfp_to_gpio(HM5040_POWERDOWN_PIN);
static int hm5040_rst = mfp_to_gpio(HM5040_RESET_PIN);
static int hm5040_power(int onoff)
{
	printk("hm5040 power : %d\n", onoff);
	gpio_request(hm5040_pwdn, "HM5040 Powerdown");
	gpio_request(hm5040_rst, "HM5040 Reset");
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(hm5040_pwdn, 1);//HM5040 different from OV5647
		gpio_direction_output(hm5040_rst, 1);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG,0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(hm5040_pwdn, 0);
		gpio_direction_output(hm5040_rst, 1);

	}
	gpio_free(hm5040_pwdn);
	gpio_free(hm5040_rst);

	return 0;
}

static int hm5040_reset(void)
{
	printk("hm5040 reset\n");

	return 0;
}

static struct i2c_board_info hm5040_board_info = {
	.type = "hm5040-mipi-raw",
	.addr = 0x1B,
};
#endif

#if defined(CONFIG_VIDEO_SP2508)
static int sp2508_pwdn = mfp_to_gpio(SP2508_POWERDOWN_PIN);
static int sp2508_rst = mfp_to_gpio(SP2508_RESET_PIN);
static int sp2508_power(int onoff)
{
	printk("sp2508 power : %d\n", onoff);

	gpio_request(sp2508_pwdn, "SP2508 Powerdown Pin");
	gpio_request(sp2508_rst, "SP2508 Reset");
	if (onoff) {
		gpio_direction_output(sp2508_pwdn, 1);
		gpio_direction_output(sp2508_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(sp2508_pwdn, 0);
		mdelay(10);
		gpio_direction_output(sp2508_rst, 1);
		mdelay(10);
		gpio_direction_output(sp2508_rst, 0);
		mdelay(10);
		gpio_direction_output(sp2508_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_request(sp2508_pwdn, "SP2508 Powerdown Pin");
		gpio_direction_output(sp2508_pwdn, 0);
		gpio_direction_output(sp2508_rst, 0);
	}
	gpio_free(sp2508_pwdn);
	gpio_free(sp2508_rst);

	return 0;
}
static int sp2508_reset(void)
{
	printk("SP2508 reset\n");

	return 0;
}

static struct i2c_board_info sp2508_board_info = {
	.type = "sp2508-mipi-raw",
	.addr = 0x3c,
};
#endif

#if defined(CONFIG_VIDEO_SP5408)
static int sp5408_pwdn = mfp_to_gpio(SP5408_POWERDOWN_PIN);
static int sp5408_rst = mfp_to_gpio(SP5408_RESET_PIN);

static int sp5408_power(int onoff)
{
	static int sp5408_power_core;
	printk(" sp5408 power : %d\n", onoff);

	comip_mfp_config(MFP_PIN_GPIO(176), MFP_PIN_MODE_GPIO);
	sp5408_power_core = mfp_to_gpio(176);


	gpio_request(sp5408_pwdn, "SP5408 Powerdown");
	gpio_request(sp5408_power_core, "SP5408 power_core");
	gpio_request(sp5408_rst, "SP5408 Reset");

	if (onoff) {
		gpio_direction_output(sp5408_pwdn, 1);
		gpio_direction_output(sp5408_power_core, 0);
		gpio_direction_output(sp5408_rst, 1);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, 1500);
		gpio_direction_output(sp5408_power_core, 1);
		mdelay(10);
		gpio_direction_output(sp5408_pwdn, 0);
		gpio_direction_output(sp5408_rst, 0);
		mdelay(50);
		gpio_direction_output(sp5408_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, 1500);

		gpio_direction_output(sp5408_pwdn, 0);
		gpio_direction_output(sp5408_power_core, 0);
		gpio_direction_output(sp5408_rst, 0);
	}
	gpio_free(sp5408_pwdn);
	gpio_free(sp5408_power_core);
	gpio_free(sp5408_rst);

	return 0;
}
static int sp5408_reset(void)
{
	printk("sp5408 reset\n");

	return 0;
}

static struct i2c_board_info sp5408_board_info = {
	.type = "sp5408-mipi-raw",
	.addr = 0x36,
};
#endif


#if defined(CONFIG_VIDEO_SP5409)
static int sp5409_pwdn = mfp_to_gpio(SP5409_POWERDOWN_PIN);
static int sp5409_rst = mfp_to_gpio(SP5409_RESET_PIN);
static int sp5409_power(int onoff)
{
	printk("sp5409 power : %d\n", onoff);

	gpio_request(sp5409_pwdn, "SP5409 Powerdown Pin");
	gpio_request(sp5409_rst, "SP5409 Reset");
	if (onoff) {
		gpio_direction_output(sp5409_pwdn, 1);
		gpio_direction_output(sp5409_rst, 1);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(sp5409_pwdn, 0);
		mdelay(10);
		gpio_direction_output(sp5409_rst, 1);
		mdelay(10);
		gpio_direction_output(sp5409_rst, 0);
		mdelay(10);
		gpio_direction_output(sp5409_rst, 1);
		mdelay(50);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_request(sp5409_pwdn, "SP5409 Powerdown Pin");
		gpio_direction_output(sp5409_pwdn, 0);
		gpio_direction_output(sp5409_rst, 0);
	}
	gpio_free(sp5409_pwdn);
	gpio_free(sp5409_rst);

	return 0;
}
static int sp5409_reset(void)
{
	printk("SP5409 reset\n");

	return 0;
}

static struct i2c_board_info sp5409_board_info = {
	.type = "sp5409-mipi-raw",
	.addr = 0x3c,
};
#endif

#if defined(CONFIG_VIDEO_SP2529)
static int sp2529_pwdn = mfp_to_gpio(SP2529_POWERDOWN_PIN);
static int sp2529_rst = mfp_to_gpio(SP2529_RESET_PIN);
static int sp2529_power(int onoff)
{
	printk("sp2529 power : %d\n", onoff);

	gpio_request(sp2529_pwdn, "SP2529 Powerdown Pin");
	gpio_request(sp2529_rst, "SP2529 Reset");
	if (onoff) {
		gpio_direction_output(sp2529_pwdn, 1);
		gpio_direction_output(sp2529_rst, 1);
		mdelay(10);


		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, 1500);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(sp2529_pwdn, 0);
		mdelay(10);
		gpio_direction_output(sp2529_pwdn, 1);
		mdelay(10);
		gpio_direction_output(sp2529_pwdn, 0);
		mdelay(10);
		gpio_direction_output(sp2529_rst, 1);
		mdelay(10);
		gpio_direction_output(sp2529_rst, 0);
		mdelay(10);
		gpio_direction_output(sp2529_rst, 1);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_request(sp2529_pwdn, "SP2529 Powerdown Pin");
		gpio_direction_output(sp2529_pwdn, 0);
		gpio_direction_output(sp2529_rst, 0);
	}
	gpio_free(sp2529_pwdn);
	gpio_free(sp2529_rst);

	return 0;
}
static int sp2529_reset(void)
{
	printk("SP2529 reset\n");

	return 0;
}

static struct i2c_board_info sp2529_board_info = {
	.type = "sp2529-mipi-yuv",
	.addr = 0x30,
};
#endif

#if defined(CONFIG_COMIP_BOARD_LC1860_EVB2)
static init_flag = 0;
static int camera_flash(enum camera_led_mode mode, int onoff)
{
	u16 reg_val;

	printk("%s: mode=%d, onoff=%d \n",__func__,mode,onoff);

	if (!init_flag) {
		pmic_reg_read(0x53, &reg_val);
		reg_val &= 0x0f;
		reg_val |= 0xf0; //150
		pmic_reg_write(0x53, reg_val);
		init_flag = 1;
	}

	if (mode == FLASH) {
		if (onoff) {
			pmic_reg_read(0x51, &reg_val);
			reg_val |= 0x08;
			pmic_reg_write(0x51, reg_val);
		} else {
			pmic_reg_read(0x51, &reg_val);
			reg_val &= 0x07;
			pmic_reg_write(0x51, reg_val);
		}

	} else {
		if (onoff) {
			pmic_reg_read(0x51, &reg_val);
			reg_val |= 0x08;
			pmic_reg_write(0x51, reg_val);
		} else {
			pmic_reg_read(0x51, &reg_val);
			reg_val &= 0x07;
			pmic_reg_write(0x51, reg_val);
		}
	}

	return 0;
}
#elif defined(CONFIG_COMIP_BOARD_LC1860_EVB3) || defined(CONFIG_COMIP_BOARD_FOURMODE_V1_0) || defined(CONFIG_COMIP_BOARD_FIVEMODE_V1_0)
static unsigned char init_flag = 0;
static int camera_flash(enum camera_led_mode mode, int onoff)
{
	printk("%s: mode=%d, onoff=%d \n",__func__,mode,onoff);

	gpio_request(FLASHER_EN, "SGM3140B EN");
	gpio_request(FLASHER_ENM, "SGM3140B MODE");

	if (!init_flag) {
		gpio_direction_output(FLASHER_EN, 0);
		gpio_direction_output(FLASHER_ENM, 0);
		init_flag = 1;
	}

	if (mode == FLASH) {
		if (onoff) {
			gpio_direction_output(FLASHER_EN, 1);
			gpio_direction_output(FLASHER_ENM, 1);
		} else {
			gpio_direction_output(FLASHER_EN, 0);
			gpio_direction_output(FLASHER_ENM, 0);
		}
	} else {
		if (onoff) {
			gpio_direction_output(FLASHER_EN, 0);
			gpio_direction_output(FLASHER_ENM, 1);
		} else {
			gpio_direction_output(FLASHER_EN, 0);
			gpio_direction_output(FLASHER_ENM, 0);
		}
	}
	gpio_free(FLASHER_EN);
	gpio_free(FLASHER_ENM);

	return 0;

}
#endif

static struct comip_camera_client comip_camera_clients[] = {
#if defined(CONFIG_VIDEO_OV13850)
	{
		.board_info = &ov13850_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
		|CAMERA_CLIENT_CLK_EXT
		|CAMERA_CLIENT_ISP_CLK_HHIGH,
		.caps = CAMERA_CAP_FOCUS_INFINITY
		| CAMERA_CAP_FOCUS_AUTO
		| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
		| CAMERA_CAP_METER_CENTER
		| CAMERA_CAP_METER_DOT
		| CAMERA_CAP_FACE_DETECT
		| CAMERA_CAP_ANTI_SHAKE_CAPTURE
		| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 4,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = ov13850_power,
		.reset = ov13850_reset,
		.flash = camera_flash,
	},
#endif
#if defined(CONFIG_VIDEO_OV8865)
	{
		.board_info = &ov8865_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
		|CAMERA_CLIENT_CLK_EXT,
		.caps = CAMERA_CAP_FOCUS_INFINITY
		| CAMERA_CAP_FOCUS_AUTO
		| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
		| CAMERA_CAP_METER_CENTER
		| CAMERA_CAP_METER_DOT
		| CAMERA_CAP_FACE_DETECT
		| CAMERA_CAP_ANTI_SHAKE_CAPTURE
		| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 2,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = ov8865_power,
		.reset = ov8865_reset,

	},
#endif

#if defined(CONFIG_VIDEO_OV4689)
	{
		.board_info = &ov4689_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
		|CAMERA_CLIENT_CLK_EXT,
		.caps = CAMERA_CAP_FOCUS_INFINITY
		| CAMERA_CAP_FOCUS_AUTO
		| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
		| CAMERA_CAP_METER_CENTER
		| CAMERA_CAP_METER_DOT
		| CAMERA_CAP_FACE_DETECT
		| CAMERA_CAP_ANTI_SHAKE_CAPTURE
		| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 4,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = ov4689_power,
		.reset = ov4689_reset,

	},
#endif
#if defined(CONFIG_VIDEO_OV5648_2LANE_19M)
	{
		.board_info = &ov5648_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
		| CAMERA_CAP_FOCUS_AUTO
		| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
		| CAMERA_CAP_METER_CENTER
		| CAMERA_CAP_METER_DOT
		| CAMERA_CAP_FACE_DETECT
		| CAMERA_CAP_ANTI_SHAKE_CAPTURE
		| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 2,
		.power = ov5648_power,
		.reset = ov5648_reset,
	},
#endif

#if defined(CONFIG_VIDEO_GC2355)
	{
		.board_info = &gc2355_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
		| CAMERA_CAP_FOCUS_AUTO
		| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
		| CAMERA_CAP_METER_CENTER
		| CAMERA_CAP_METER_DOT
		| CAMERA_CAP_FACE_DETECT
		| CAMERA_CAP_ANTI_SHAKE_CAPTURE
		| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 2,
		.power = gc2355_power,
		.reset = gc2355_reset,
	},
#endif


#if defined(CONFIG_VIDEO_OV9760)
	{
		.board_info = &ov9760_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
		| CAMERA_CAP_FOCUS_AUTO
		| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
		| CAMERA_CAP_METER_CENTER
		| CAMERA_CAP_METER_DOT
		| CAMERA_CAP_FACE_DETECT
		| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 1,
		.power = ov9760_power,
		.reset = ov9760_reset,
	},
#endif

#if defined(CONFIG_VIDEO_BF3A20_MIPI)
	{
		.board_info = &bf3a20_mipi_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
				 | CAMERA_CLIENT_YUV_DATA,

		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 1,   ///2  //RYX
		.power = bf3a20_mipi_power,
		.reset = bf3a20_mipi_reset,
//		 .flash = camera_flash,
	},
#endif

#if defined(CONFIG_VIDEO_HM5040)
	{
		.board_info = &hm5040_board_info,
	.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_CLK_EXT
			|CAMERA_CLIENT_ISP_CLK_HIGH,
		.caps = CAMERA_CAP_FOCUS_INFINITY
			| CAMERA_CAP_FOCUS_AUTO
			| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
			| CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = hm5040_power,
		.reset = hm5040_reset,
	},
#endif

#if defined(CONFIG_VIDEO_SP5408)
	{
		.board_info = &sp5408_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_CLK_EXT,
		.caps = CAMERA_CAP_FOCUS_INFINITY
			| CAMERA_CAP_FOCUS_AUTO
			| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
			| CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = sp5408_power,
		.reset = sp5408_reset,
		//	.flash = camera_flash,
	},
#endif


#if defined(CONFIG_VIDEO_SP5409)
	{
		.board_info = &sp5409_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
			| CAMERA_CAP_FOCUS_AUTO
			| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
			| CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 2,
		.power = sp5409_power,
		.reset = sp5409_reset,
	},
#endif

#if defined(CONFIG_VIDEO_SP2508)
	{
		.board_info = &sp2508_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
			| CAMERA_CAP_FOCUS_AUTO
			| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
			| CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 1,//2
		.power = sp2508_power,
		.reset = sp2508_reset,
	},
#endif

#if defined(CONFIG_VIDEO_SP2529)
	{
		.board_info = &sp2529_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_YUV_DATA,
		.caps = CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 1,
		.power = sp2529_power,
		.reset = sp2529_reset,
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
	comip_init_ts_virtual_key();
}

