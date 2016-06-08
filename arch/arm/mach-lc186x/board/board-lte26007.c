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
** 1.0.0	2014-4-1	xuxuefeng	created
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

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#include <plat/ft5x06_ts.h>
#endif

#ifdef CONFIG_LIGHT_PROXIMITY_STK3X1X
#include "plat/stk3x1x.h"
#endif

#ifdef CONFIG_SENSORS_LSM330
#include "plat/lsm330.h"
#endif
#ifdef CONFIG_BAT_ID_BQ2022A
#include <plat/bq2022a-batid.h>
#endif
#if defined(CONFIG_SENSORS_AK09911)
#include <plat/akm09911.h>
#endif

#ifdef CONFIG_LIGHT_PROXIMITY_LTR55XALS
#include <plat/ltr558als.h>
#endif
#ifdef CONFIG_SENSORS_INV_MPU6880
#include <linux/mpu.h>
#endif

#include "board-lte26007.h"

#include "board-common.c"

#if defined(CONFIG_LC1132_MONITOR_BATTERY) || defined(CONFIG_LC1160_MONITOR_BATTERY)
static int batt_table[] = {
	/* adc code for temperature in degree C */
	2466, 2447, 2428, 2409, 2388, 2367, 2345, 2322, 2299, 2275,/* -20 ,-11 */
	2250, 2224, 2198, 2171, 2144, 2115, 2086, 2057, 2027, 1996,/* -10 ,-1 */
	1965, 1934, 1902, 1869, 1836, 1803, 1770, 1736, 1702, 1668,/* 00 - 09 */
	1634, 1599, 1565, 1531, 1496, 1462, 1428, 1394, 1360, 1327, /* 10 - 19 */
	1294, 1261, 1228, 1196, 1164, 1133, 1102, 1072, 1042, 1012,  /* 20 - 29 */
	983, 955, 927, 900, 873, 847, 822, 797, 773, 749,/* 30 - 39 */
	726, 703, 681, 660, 639, 619, 599, 580, 562, 544, /* 40 - 49 */
	526, 509, 493, 477, 461, 446, 432, 418, 404, 391, /* 50 - 59 */
	379, 366, 354, 343, 332, 321, 311, 300, 291, 281,/* 60 - 69 */
	272,/* 70 */
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
	.temp_high_threshold = 60,
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

#if defined(CONFIG_BATTERY_MAX17058)||defined(CONFIG_BATTERY_BQ27421)
static int batt_temp_table[] = {
	/* adc code for temperature in degree C */
	2466, 2447, 2428, 2409, 2388, 2367, 2345, 2322, 2299, 2275,/* -20 ,-11 */
	2250, 2224, 2198, 2171, 2144, 2115, 2086, 2057, 2027, 1996,/* -10 ,-1 */
	1965, 1934, 1902, 1869, 1836, 1803, 1770, 1736, 1702, 1668,/* 00 - 09 */
	1634, 1599, 1565, 1531, 1496, 1462, 1428, 1394, 1360, 1327, /* 10 - 19 */
	1294, 1261, 1228, 1196, 1164, 1133, 1102, 1072, 1042, 1012,  /* 20 - 29 */
	983, 955, 927, 900, 873, 847, 822, 797, 773, 749,/* 30 - 39 */
	726, 703, 681, 660, 639, 619, 599, 580, 562, 544, /* 40 - 49 */
	526, 509, 493, 477, 461, 446, 432, 418, 404, 391, /* 50 - 59 */
	379, 366, 354, 343, 332, 321, 311, 300, 291, 281,/* 60 - 69 */
	272,/* 70 */
};
/*max17058 fuel gauge model data table*/
    /*
    * NOTE:  don't change this table without updating the
    *  max1705_battery.c defines for battery_id about data_baseaddr
    * so they continue to match the order in this table.
    */
static int fg_batt_model_table[64*4] = {
	/*default battery (data_baseaddr = 0)*/
	0xAE,0xF0,0xB7,0x70,0xB8,0x80,0xB9,0xE0,
	0xBB,0x70,0xBC,0x50,0xBD,0x30,0xBE,0x60,
	0xBF,0xB0,0xBF,0xD0,0xC0,0xB0,0xC3,0xC0,
	0xC9,0x90,0xCF,0xE0,0xD4,0x60,0xD6,0x50,
	0x04,0x00,0x2F,0x60,0x22,0x20,0x2A,0x00,
	0x38,0x00,0x36,0x00,0x30,0xD0,0x27,0xF0,
	0x65,0xC0,0x0B,0xF0,0x1C,0xE0,0x0F,0xF0,
	0x0F,0x30,0x12,0x60,0x07,0x30,0x07,0x30,
	/*GUANGYU (data_baseaddr = 64)*/
	0xAE,0xF0,0xB7,0x70,0xB8,0x80,0xB9,0xE0,
	0xBB,0x70,0xBC,0x50,0xBD,0x30,0xBE,0x60,
	0xBF,0xB0,0xBF,0xD0,0xC0,0xB0,0xC3,0xC0,
	0xC9,0x90,0xCF,0xE0,0xD4,0x60,0xD6,0x50,
	0x04,0x00,0x2F,0x60,0x22,0x20,0x2A,0x00,
	0x38,0x00,0x36,0x00,0x30,0xD0,0x27,0xF0,
	0x65,0xC0,0x0B,0xF0,0x1C,0xE0,0x0F,0xF0,
	0x0F,0x30,0x12,0x60,0x07,0x30,0x07,0x30,
	/*ATL XINWANGDA (data_baseaddr = 128)*/
	0xA9,0x90,0xB4,0x50,0xB7,0x10,0xB8,0x60,
	0xB8,0xC0,0xBB,0x30,0xBB,0xD0,0xBC,0x80,
	0xBD,0x30,0xBE,0xD0,0xC0,0x90,0xC3,0xD0,
	0xC7,0x10,0xCD,0x10,0xD1,0xE0,0xD6,0x80,
	0x02,0x10,0x05,0x10,0x11,0x50,0x71,0x60,
	0x16,0x10,0x29,0xD0,0x44,0x90,0x4C,0xF0,
	0x31,0xF0,0x26,0xD0,0x15,0xF0,0x12,0xF0,
	0x11,0xF0,0x0E,0x10,0x0D,0x20,0x0D,0x20,
	/*DESAI (data_baseaddr = 192)*/
	0xA9,0xF0,0xB6,0x40,0xB7,0x40,0xB8,0x20,
	0xBA,0xD0,0xBB,0x20,0xBC,0x50,0xBC,0xC0,
	0xBE,0x10,0xBF,0x60,0xC2,0xD0,0xC5,0xB0,
	0xC9,0x30,0xCD,0xA0,0xD1,0xA0,0xD7,0x50,
	0x00,0x90,0x32,0x00,0x39,0x00,0x12,0xE0,
	0x50,0xE0,0x27,0x20,0x77,0x30,0x20,0xA0,
	0x3A,0xD0,0x12,0xF0,0x15,0xF0,0x11,0xE0,
	0x11,0xE0,0x0D,0xF0,0x0D,0x20,0x0D,0x20,
};

struct comip_fuelgauge_info fg_data = {
	.battery_tmp_tbl    = batt_temp_table,
	.tmptblsize = ARRAY_SIZE(batt_temp_table),
	.fg_model_tbl = fg_batt_model_table,
	.fgtblsize = ARRAY_SIZE(fg_batt_model_table),
};
#endif

#ifdef CONFIG_CHARGER_BQ24158
struct extern_charger_platform_data exchg_data ={
    .max_charger_currentmA  = 1250,
    .max_charger_voltagemV  = 4350,
    .termination_currentmA  = 100,
    .dpm_voltagemV          = 4360,
    .usb_cin_limit_currentmA= 500,
    .usb_battery_currentmA  = 550,
    .ac_cin_limit_currentmA = 1050,
    .ac_battery_currentmA   = 1050,
    .feature = 1,
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

int comip_board_info_get(char *name)
{
	int i,volt, r1551, r1552, voltage;
	int Vadc = 2850;//mv
	int min_index = 0;
	volt = pmic_get_adc_conversion(HARDWARE_BOARD_ADC);
	for(i = 0; i < ARRAY_SIZE(board_id_map); i++) {
		r1551 = board_id_map[i].r1551;
		r1552 = board_id_map[i].r1552;
		voltage = Vadc * r1552 / (r1552 + r1551);
		board_id_map[i].delta = abs(volt - voltage);
		if(i > 0 && board_id_map[i].delta < board_id_map[min_index].delta)
			min_index = i;
		}

	if(name) {
		strcpy(name, board_id_map[min_index].name);
	}

	return board_id_map[min_index].id;
}

#ifdef CONFIG_BAT_ID_BQ2022A
static int bq2022a_invalid(void)
{
	int ret = 0;
	if(comip_board_info_get(NULL)== LTE26007M20 && (cpu_is_lc1860_eco1()||cpu_is_lc1860_eco2()))
		ret = 1; // invalid
	return ret;
}

static struct bq2022a_platform_data bq2022a_info = {
	.sdq_pin = BAT_ID_BQ2022A_PIN,
	.sdq_gpio = BAT_ID_BQ2022A_PINEOC,
	.bat_id_invalid = bq2022a_invalid,
};

static struct platform_device bq2022a_device = {
	.name = "bq2022a",
	.id = -1,
	.dev = {
	        .platform_data = &bq2022a_info,
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
#ifdef CONFIG_BAT_ID_BQ2022A
	&bq2022a_device,
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
#if defined(CONFIG_BATTERY_MAX17058) || defined(CONFIG_BATTERY_BQ27421)
	{MAX17058_FG_INT_PIN,           MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_CHARGER_BQ24158)
	{EXTERN_CHARGER_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif

	/* LCD. */
	{LCD_RESET_PIN,			MFP_PIN_MODE_GPIO},
#if defined(CONFIG_SENSORS_INV_MPU6880)
	{INV6880_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	{FT5X06_RST_PIN,                MFP_PIN_MODE_GPIO},
	{FT5X06_INT_PIN,                MFP_PIN_MODE_GPIO},
#endif
	/*Camera*/
#if defined(CONFIG_VIDEO_S5K3H2)	/* S5K3H2 */
	{S5K3H2_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{S5K3H2_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV8865)	/* OV8865. */
	{OV8865_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV8865_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_VIDEO_OV9760)        /* OV9760. */
	{OV9760_POWERDOWN_PIN,          MFP_PIN_MODE_GPIO},
	{OV9760_RESET_PIN,              MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV2680)        /* OV2680. */
	{OV2680_POWERDOWN_PIN,          MFP_PIN_MODE_GPIO},
	{OV2680_RESET_PIN,              MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OCP8111)
	{OCP8111_EN_PIN,		MFP_PIN_MODE_GPIO},
	{OCP8111_MODE_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(EAR_SWITCH_PIN)
	{EAR_SWITCH_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CODEC_PA_EXTRA_PIN)
	{CODEC_PA_EXTRA_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CODEC_PA_PIN)
	{CODEC_PA_PIN,		MFP_PIN_MODE_GPIO},
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
	{MFP_PIN_GPIO(135), 	MFP_PIN_MODE_GPIO},
};

static struct mfp_pull_cfg comip_mfp_pull_cfg[] = {
#if defined(CONFIG_BATTERY_MAX17058) || defined(CONFIG_BATTERY_BQ27421)
	{MAX17058_FG_INT_PIN,	MFP_PULL_UP},
#endif
#if defined(CONFIG_CHARGER_BQ24158)
	{EXTERN_CHARGER_INT_PIN,	MFP_PULL_UP},
#endif
	{COMIP_GPIO_KEY_VOLUMEUP,	MFP_PULL_UP},
	{COMIP_GPIO_KEY_VOLUMEDOWN,	MFP_PULL_UP},

#if defined(CONFIG_RTK_BLUETOOTH) || defined(CONFIG_BRCM_BLUETOOTH)
	{UART2_CTS_PIN,	MFP_PULL_DOWN},
#endif

	/*mfp low power config*/
	{MFP_PIN_GPIO(72),		MFP_PULL_UP},
	{MFP_PIN_GPIO(73),		MFP_PULL_UP},
	{MFP_PIN_GPIO(253), 	MFP_PULL_DISABLE},
	//{MFP_PIN_GPIO(140), 	MFP_PULL_DISABLE},
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
	//{MFP_PIN_GPIO(230), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(234), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(235), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(236), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(237), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(238), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(239), 	MFP_PULL_UP},
	{MFP_PIN_GPIO(243), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(169), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(170), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(171), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(172), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(198), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(199), 	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(201), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(204), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(205), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(212), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(215), 	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(135), 	MFP_PULL_DISABLE},

};

#ifdef CONFIG_BAT_ID_BQ2022A

static struct mfp_pin_cfg comip_mfp_cfg1[] = {
	{BAT_ID_BQ2022A_PIN,            MFP_PIN_MODE_GPIO},
};
static struct mfp_pull_cfg comip_mfp_pull_cfg1[] = {
	{BAT_ID_BQ2022A_PIN,        MFP_PULL_UP},
	{MFP_PIN_GPIO(230), 	MFP_PULL_DOWN},
};

static struct mfp_pin_cfg comip_mfp_cfg2[] = {
	{BAT_ID_BQ2022A_PINEOC,            MFP_PIN_MODE_GPIO},
};
static struct mfp_pull_cfg comip_mfp_pull_cfg2[] = {
	{BAT_ID_BQ2022A_PINEOC,    MFP_PULL_UP},
	{MFP_PIN_GPIO(233), 	   MFP_PULL_DISABLE},
};
#endif

static struct mfp_ds_cfg comip_ds_cfg[] = {
	{MFP_PIN_CTRL_MMC0CLK,	MFP_DS_8MA},
	{MFP_PIN_CTRL_MMC0CMD,	MFP_DS_6MA},
	{MFP_PIN_CTRL_MMC0D0,	MFP_DS_6MA},
	{MFP_PIN_CTRL_MMC0D1,	MFP_DS_6MA},
	{MFP_PIN_CTRL_MMC0D2,	MFP_DS_6MA},
	{MFP_PIN_CTRL_MMC0D3,	MFP_DS_6MA},
};


static void __init comip_init_mfp(void)
{
	comip_mfp_config_array(comip_mfp_cfg, ARRAY_SIZE(comip_mfp_cfg));
	comip_mfp_config_pull_array(comip_mfp_pull_cfg, ARRAY_SIZE(comip_mfp_pull_cfg));
	comip_mfp_config_ds_array(comip_ds_cfg, ARRAY_SIZE(comip_ds_cfg));

#ifdef CONFIG_BAT_ID_BQ2022A
	if(cpu_is_lc1860_eco1()||cpu_is_lc1860_eco2()){
		comip_mfp_config_array(comip_mfp_cfg2, ARRAY_SIZE(comip_mfp_cfg2));
		comip_mfp_config_pull_array(comip_mfp_pull_cfg2, ARRAY_SIZE(comip_mfp_pull_cfg2));
	}else{
		comip_mfp_config_array(comip_mfp_cfg1, ARRAY_SIZE(comip_mfp_cfg1));
		comip_mfp_config_pull_array(comip_mfp_pull_cfg1, ARRAY_SIZE(comip_mfp_pull_cfg2));
	}
#endif
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
	{MFP_PIN_GPIO(135),		MFP_GPIO_INPUT},
};

static void __init comip_init_gpio_lp(void)
{
	comip_gpio_config_array(comip_init_mfp_lp_gpio_cfg, ARRAY_SIZE(comip_init_mfp_lp_gpio_cfg));
}


#if defined(CONFIG_I2C_COMIP) || defined(CONFIG_I2C_COMIP_MODULE)
#if defined(CONFIG_COMIP_LC1160)
static struct pmic_power_module_map lc1160_power_module_map[] = {
	{PMIC_DLDO3,	PMIC_POWER_SDIO,		0,	0},
	{PMIC_ALDO14,	PMIC_POWER_SDIO,		1,	0},
	{PMIC_DLDO4,	PMIC_POWER_USIM,		0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,		1,	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,		0,	0},
	{PMIC_DLDO7,	PMIC_POWER_LCD_CORE,		0,	0},
	{PMIC_DLDO8,	PMIC_POWER_TOUCHSCREEN,		0,	1},
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		0,	0},
	{PMIC_DLDO9,	PMIC_POWER_CAMERA_CORE,		1,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO11,	PMIC_POWER_CAMERA_AF_MOTOR,		0,	1},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,		0,	0},
	{PMIC_ALDO7,	PMIC_POWER_CAMERA_ANALOG,		1,	0},
	{PMIC_ALDO10,	PMIC_POWER_LCD_IO,		0,	0},
	{PMIC_ALDO10,	PMIC_POWER_TOUCHSCREEN_IO,		0,	0},
	{PMIC_ALDO10,	PMIC_POWER_CAMERA_CSI_PHY,		0,	0},
	{PMIC_ALDO12,	PMIC_POWER_USB,			1,	0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR,		0,	0}
};

static struct pmic_power_ctrl_map lc1160_power_ctrl_map[] = {
	//{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO8,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO12,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ALDO14,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	10},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	80},
};

static struct pmic_reg_st lc1160_init_regs[] = {
	/* RF power register could be write by SPI &IIC */
	{LC1160_REG_SPICR, 0x01, LC1160_REG_BITMASK_SPI_IIC_EN},
	/* ALDO3 disable */
	{LC1160_REG_LDOA3CR, 0x00, LC1160_REG_BITMASK_LDOA3EN},
	/* ALDO2 enter ECO mode when sleep */
	{LC1160_REG_LDOA2CR, 0x01, LC1160_REG_BITMASK_LDOA2SLP},
	/*DCDC9 disable when sleep */
	{LC1160_REG_DC9OVS1, 0x00, LC1160_REG_BITMASK_DC9OVS1_DC9EN},
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
	/* DLDO6/7/8/9/10/11/ disable when sleep. DLDO3 closed by mmc driver. */
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

static ssize_t virtual_key_show(struct kobject *kobj,
                                struct kobj_attribute *attr, char *buf)
{
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	/**720p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":160:1342:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":360:1342:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":570:1342:100:90"
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
		msleep(2);
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
	.max_scrx = 720,
	.max_scry = 1280,
	.virtual_scry = 1450,
	.max_points = 10,
};
#endif
#ifdef CONFIG_LIGHT_PROXIMITY_STK3X1X
static struct stk3x1x_platform_data stk3x1x_data={
	.state_reg = 0x0, /* disable all */
	.psctrl_reg = 0x31, /* ps_persistance=1, ps_gain=64X, PS_IT=0.391ms */
	.alsctrl_reg = 0x38, /* als_persistance=1, als_gain=64X, ALS_IT=50ms */
	.ledctrl_reg = 0xFF, /* 100mA IRDR, 64/64 LED duty */
	.wait_reg = 0x07, /* 50 ms */
	.ps_thd_h =1700,
	.ps_thd_l = 1500,
	.int_pin = mfp_to_gpio(STK3X1X_INT_PIN),
	.transmittance = 500,
};
#endif

#ifdef CONFIG_LIGHT_PROXIMITY_LTR55XALS
static struct ltr55x_platform_data ltr55x_pdata = {
	.irq_gpio = mfp_to_gpio(LTR55XALS_INT_PIN),
};
#endif
#if defined (CONFIG_SENSORS_AK09911)
static int addr_offset(void)
{
	if (comip_board_info_get(NULL) == LTE26047M10)
		return 0x0c;
	else if (comip_board_info_get(NULL) > LTE26047M10){
		return 0x0d;
	} else {
		return 0;
	}
}
static char layout_offset(void)
{
	if (comip_board_info_get(NULL) >= LTE26047M12)
		return 0x06;
	else {
		return 0x05;
	}
}
static struct akm09911_platform_data akm09911_pdata = {
	.layout = layout_offset,
	.addr_amend = addr_offset,
};
#endif

#ifdef CONFIG_SENSORS_LSM330
static struct lsm330_acc_platform_data lsm330_acc_data = {
	.fs_range = LSM330_ACC_G_2G,
#ifdef CONFIG_SENSORS_XYZ
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
#endif
#ifdef CONFIG_SENSORS_YXZ
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
#endif
#ifdef CONFIG_SENSORS_IOO
	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 0,
#endif
#ifdef CONFIG_SENSORS_IOI
	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 1,
#endif
	.poll_interval = 10,
	.min_interval = LSM330_ACC_MIN_POLL_PERIOD_MS,
};

static struct lsm330_gyr_platform_data lsm330_gyr_data = {
	.fs_range = LSM330_GYR_FS_2000DPS,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 0,
	.poll_interval = 10,
	.min_interval = LSM330_GYR_MIN_POLL_PERIOD_MS, /* 2ms */
};
#endif
#ifdef CONFIG_SENSORS_INV_MPU6880
static int mpuirq_init(void)
{
	int ret = 0;

	pr_info("*** MPU START *** mpuirq_init...\n");

	/* MPU-IRQ assignment */
	ret = gpio_request(INV6880_INT_PIN, "mpu6880");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return -1;
	}

	ret = gpio_direction_input(INV6880_INT_PIN);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(209);
		return -1;
	}
	//gpio_to_irq(INV6880_INT_PIN);
	pr_info("*** MPU END *** mpuirq_init...\n");
	//disable_irq_nosync(gpio_to_irq(209));
	return 0;

}

static struct mpu_platform_data mpu_gyro_data = {
	.int_config  = 0x10,
        .level_shifter = 0,
        .orientation = {   0,  -1 , 0,
                          -1,  0,  0,
                           0,  0,  -1 },
	.sec_slave_type = SECONDARY_SLAVE_TYPE_NONE,
	.key = {221, 22, 205, 7,   217, 186, 151, 55,
                206, 254, 35, 144, 225, 102,  47, 50},
	.irq_init = mpuirq_init,
};

#endif

/* NFC. */
static struct i2c_board_info comip_i2c0_board_info[] = {
	// TODO:  to be continue for device
#if defined(CONFIG_BATTERY_BQ27421)
	{
		I2C_BOARD_INFO("bq27421-battery", 0x55),
		.platform_data = &fg_data,
		.irq = mfp_to_gpio(MAX17058_FG_INT_PIN),
	},
#endif
};

/* Sensors. */
static struct i2c_board_info comip_i2c1_board_info[] = {
#ifdef CONFIG_SENSORS_HSCDTD007A
	{
		I2C_BOARD_INFO("hscdtd007a",0x0c),
	},
#endif
#ifdef CONFIG_SENSORS_LSM330
	{
		I2C_BOARD_INFO("lsm330_acc", 0x1E),
		.platform_data = &lsm330_acc_data,
	},
	{
		I2C_BOARD_INFO("lsm330_gyr", 0x6A),
		.platform_data = &lsm330_gyr_data,
	},
#endif
#if defined(CONFIG_SENSORS_AK09911)
	{
		.type = "akm09911",
		.addr = 0x0d,
		.platform_data = &akm09911_pdata,
	},
#endif
#if defined(CONFIG_SENSORS_YAS_MAGNETOMETER)
	{
		I2C_BOARD_INFO("yas_magnetometer", 0x2e),
	},
#endif

#ifdef CONFIG_LIGHT_PROXIMITY_STK3X1X
	{
		I2C_BOARD_INFO("stk_ps", 0x48),
		.platform_data = &stk3x1x_data,
	},
#endif
#ifdef CONFIG_LIGHT_PROXIMITY_LTR55XALS
	{
		I2C_BOARD_INFO("ltr55xals", 0x23),
		.platform_data = &ltr55x_pdata,
	},
#endif
#if defined (CONFIG_SENSORS_INV_MPU6880)
        {
		I2C_BOARD_INFO("mpu6880", 0x68),
		.irq = mfp_to_gpio(INV6880_INT_PIN),
		.platform_data = &mpu_gyro_data,
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

#if defined(CONFIG_CHARGER_BQ24158)
	{
		I2C_BOARD_INFO("fan5405", 0x6a),
		.platform_data = &exchg_data,
		.irq = mfp_to_gpio(EXTERN_CHARGER_INT_PIN),
	},
#endif

#if defined(CONFIG_BATTERY_MAX17058)
	{
		I2C_BOARD_INFO("max17058-battery", 0x36),
		.platform_data = &fg_data,
		.irq = mfp_to_gpio(MAX17058_FG_INT_PIN),
	},
#endif

#if defined(CONFIG_LEDS_AW2013)
	{
		.type = "aw2013",
		.addr = 0x45,
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
	int index = 0;
	struct pmic_power_ctrl_map *power_ctrl_map_item = NULL;
	if (cpu_is_lc1860_eco1() == 3) {
		printk("comip_init_i2c cpu_is_lc1860_eco1\n");
		for(index = 0; index < sizeof(lc1160_power_ctrl_map) / sizeof(lc1160_power_ctrl_map[0]); index++){

			power_ctrl_map_item = &lc1160_power_ctrl_map[index];

			if (power_ctrl_map_item->power_id == PMIC_ALDO12) {
				power_ctrl_map_item->default_mv = 1550;
				printk("lc1860_eco1:PMIC_ALDO12 default_mv=1550 \n");
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
static struct comipfb_platform_data comip_lcd_info = {
	.lcdcaxi_clk = 312000000,
	.lcdc_support_interface = COMIPFB_MIPI_IF,
	.phy_ref_freq = 26000,	/* KHz */
	.gpio_rst = LCD_RESET_PIN,
	.gpio_im = -1,
	.flags = COMIPFB_SLEEP_POWEROFF,
	.bl_control = comip_lcd_bl_control,
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
#if defined(CONFIG_VIDEO_S5K3H2)
static int s5k3h2_pwdn = mfp_to_gpio(S5K3H2_POWERDOWN_PIN);
static int s5k3h2_rst = mfp_to_gpio(S5K3H2_RESET_PIN);
static int s5k3h2_power(int onoff)
{
	printk("s5k3h2 power : %d\n", onoff);
	gpio_request(s5k3h2_pwdn, "S5K3H2 Powerdown");
	gpio_request(s5k3h2_rst, "s5k3h2 Reset");

	if (onoff) {
		gpio_direction_output(s5k3h2_pwdn, 0);
		gpio_direction_output(s5k3h2_rst, 0);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, 1200);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		gpio_direction_output(s5k3h2_pwdn, 1);
		gpio_direction_output(s5k3h2_rst, 1);

		mdelay(10);
		gpio_direction_output(s5k3h2_rst, 0);
		gpio_direction_output(s5k3h2_pwdn, 0);
		mdelay(10);
		gpio_direction_output(s5k3h2_pwdn, 1);
		gpio_direction_output(s5k3h2_rst, 1);
	} else {
		gpio_direction_output(s5k3h2_pwdn, 0);
		gpio_direction_output(s5k3h2_rst, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}
		gpio_free(s5k3h2_pwdn);
		gpio_free(s5k3h2_rst);
	return 0;
}

static int s5k3h2_reset(void)
{
	printk("s5k3h2 reset\n");
	return 0;
}

static struct i2c_board_info s5k3h2_board_info = {
	.type = "s5k3h2-mipi-raw",
	.addr = 0x10,
	//.platform_data = &s5k3h2_setting,
};
#endif
#if defined(CONFIG_VIDEO_OV8865)
static int ov8865_pwdn = mfp_to_gpio(OV8865_POWERDOWN_PIN);
static int ov8865_rst = mfp_to_gpio(OV8865_RESET_PIN);
static int ov8865_mclk = mfp_to_gpio(OV8865_MCLK_PIN);
static int ov8865_power(int onoff)
{
	printk("ov8865 power : %d\n", onoff);

	gpio_request(ov8865_pwdn, "OV8865 Powerdown");
	gpio_request(ov8865_rst, "OV8865 Reset");
	if (onoff) {
		gpio_direction_output(ov8865_pwdn, 0);
		gpio_direction_output(ov8865_rst, 1);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, 1200);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);


		gpio_direction_output(ov8865_pwdn, 1);//OV8865 different from OV5647
		mdelay(1);
		gpio_direction_output(ov8865_rst, 0);
		mdelay(1);
		gpio_direction_output(ov8865_rst, 1);
		comip_mfp_config_ds(ov8865_mclk, MFP_DS_2MA);
	} else {

		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);


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
	.addr = 0x10,
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
	.addr = 0x20,
};
static struct i2c_board_info ov9760_board_info1 = {
	.type = "ov9760-mipi-raw",
	.addr = 0x36,
};
#endif

#if defined(CONFIG_VIDEO_OV2680)
static int ov2680_pwdn = mfp_to_gpio(OV2680_POWERDOWN_PIN);
static int ov2680_rst = mfp_to_gpio(OV2680_RESET_PIN);
static int ov2680_mclk = mfp_to_gpio(OV2680_MCLK_PIN);
static int ov2680_power(int onoff)
{
	printk("ov2680 power : %d\n", onoff);

	gpio_request(ov2680_pwdn, "OV2680 Powerdown");
	gpio_request(ov2680_rst, "OV2680 Reset");

	if (onoff) {
		gpio_direction_output(ov2680_pwdn, 0);
		gpio_direction_output(ov2680_rst, 1);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, 1500);

		mdelay(10);
		gpio_direction_output(ov2680_pwdn, 1);
		gpio_direction_output(ov2680_rst, 0);
		mdelay(10);
		gpio_direction_output(ov2680_rst, 1);
		comip_mfp_config_ds(ov2680_mclk, MFP_DS_2MA);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov2680_pwdn, 0);
		gpio_direction_output(ov2680_rst, 0);
	}

	gpio_free(ov2680_pwdn);
	gpio_free(ov2680_rst);

	return 0;
}

static int ov2680_reset(void)
{
	printk("ov2680 reset\n");
	return 0;
}


static struct i2c_board_info ov2680_board_info = {
	.type = "ov2680-mipi-raw",
	.addr = 0x20,
};
static struct i2c_board_info ov2680_board_info1 = {
	.type = "ov2680-mipi-raw",
	.addr = 0x36,
};

#endif

#if defined(CONFIG_VIDEO_OCP8111)
static int ocp8111_gpio_en = mfp_to_gpio(OCP8111_EN_PIN);
static int ocp8111_gpio_mode = mfp_to_gpio(OCP8111_MODE_PIN);
static unsigned char init_flag = 0;

static int ocp8111_set(enum camera_led_mode mode, int onoff)
{
	printk(KERN_DEBUG"%s: mode=%d, onoff=%d \n",__func__,mode,onoff);

	if (!init_flag) {
		gpio_request(ocp8111_gpio_en, "OCP8111 EN");
		gpio_request(ocp8111_gpio_mode, "OCP8111 MODE");
		gpio_direction_output(ocp8111_gpio_en, 0);
		gpio_direction_output(ocp8111_gpio_mode, 0);
		gpio_free(ocp8111_gpio_en);
		gpio_free(ocp8111_gpio_mode);
		init_flag = 1;
	}

	if (mode == FLASH) {
		gpio_request(ocp8111_gpio_en, "OCP8111 EN");
		gpio_request(ocp8111_gpio_mode, "OCP8111 MODE");
		if (onoff) {
			gpio_direction_output(ocp8111_gpio_mode, 1);
			gpio_direction_output(ocp8111_gpio_en, 1);
			mdelay(1);
		} else {
			gpio_direction_output(ocp8111_gpio_en, 0);
			gpio_direction_output(ocp8111_gpio_mode, 0);
		}
		gpio_free(ocp8111_gpio_en);
		gpio_free(ocp8111_gpio_mode);
	}
	else {
		gpio_request(ocp8111_gpio_en, "OCP8111 EN");
		gpio_request(ocp8111_gpio_mode, "OCP8111 MODE");
		if (onoff) {
			gpio_direction_output(ocp8111_gpio_mode, 0);
			gpio_direction_output(ocp8111_gpio_en, 1);
			//mdelay(200);
		}
		else {
			gpio_direction_output(ocp8111_gpio_en, 0);
			gpio_direction_output(ocp8111_gpio_mode, 0);
		}
		gpio_free(ocp8111_gpio_en);
		gpio_free(ocp8111_gpio_mode);
	}

	return 0;
}

static int camera_flash(enum camera_led_mode mode, int onoff)
{
        return ocp8111_set(mode, onoff);
}
#else
static int camera_flash(enum camera_led_mode mode, int onoff)
{
        return 0;
}
#endif

static struct comip_camera_client comip_camera_clients[] = {

#if defined(CONFIG_VIDEO_S5K3H2)
	{
		.board_info = &s5k3h2_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_FRAMERATE_DYN
			|CAMERA_CLIENT_CLK_EXT
			|CAMERA_CLIENT_ISP_CLK_HIGH,
		.caps = CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE
			| CAMERA_CAP_FLASH,
		.if_id = 0,
		.mipi_lane_num = 2,
		.camera_id = CAMERA_ID_BACK,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = s5k3h2_power,
		.reset = s5k3h2_reset,
		.flash = camera_flash,
	},
#endif
#if defined(CONFIG_VIDEO_OV8865)
	{
		.board_info = &ov8865_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_FRAMERATE_DYN
			|CAMERA_CLIENT_CLK_EXT
			|CAMERA_CLIENT_ISP_CLK_HIGH,
		.caps = CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_HDR_CAPTURE
			| CAMERA_CAP_FLASH,
		.if_id = 0,
		.mipi_lane_num = 4,
		.camera_id = CAMERA_ID_BACK,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = ov8865_power,
		.reset = ov8865_reset,
		.flash = camera_flash,

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
		.camera_id = CAMERA_ID_FRONT,
		.power = ov9760_power,
		.reset = ov9760_reset,
	},
	{
		.board_info = &ov9760_board_info1,
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
		.camera_id = CAMERA_ID_FRONT,
		.power = ov9760_power,
		.reset = ov9760_reset,
	},
#endif

#if defined(CONFIG_VIDEO_OV2680)
	{
		.board_info = &ov2680_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_FRAMERATE_DYN
			|CAMERA_CLIENT_ISP_CLK_MIDDLE,
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
		.power = ov2680_power,
		.reset = ov2680_reset,
	},
	{
		.board_info = &ov2680_board_info1,
		.flags = CAMERA_CLIENT_IF_MIPI
			|CAMERA_CLIENT_FRAMERATE_DYN
			|CAMERA_CLIENT_ISP_CLK_MIDDLE,
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
		.power = ov2680_power,
		.reset = ov2680_reset,
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
		.aecgc_stable_duration = 1200,
		.torch_off_duration = 100,
		.flash_on_ramp_time = 150,
		.mean_percent = 50,
		.brightness_ratio =32,
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
static void comip_init_misc(void)
{
}

static int comip_set_mmc0_info_ext(void)
{
	if (comip_board_info_get(NULL) > LTE26007M20)
		return MMCF_UHS_I;
	else
		return 0;
}

static int ear_switch(int enable)
{
	if (comip_board_info_get(NULL) > LTE26007M20) {
		int ear_switch_gpio = mfp_to_gpio(EAR_SWITCH_PIN);
		gpio_request(ear_switch_gpio, "ear switch");
		gpio_direction_output(ear_switch_gpio, enable);
		gpio_free(ear_switch_gpio);
	}

	return 0;
}

static int pa_boost_enable(int enable)
{
	if (comip_board_info_get(NULL) > LTE26007M20) {
		int pa_extra_gpio = mfp_to_gpio(CODEC_PA_EXTRA_PIN);
		int pa_gpio=mfp_to_gpio(CODEC_PA_PIN);
		gpio_request(pa_extra_gpio, "codec_pa_extra_gpio");
		gpio_request(pa_gpio, "codec_pa_gpio");
		gpio_direction_output(pa_extra_gpio, enable);
		gpio_direction_output(pa_gpio, enable);
		gpio_free(pa_extra_gpio);
		gpio_free(pa_gpio);
	}else{
		int pa_gpio=mfp_to_gpio(CODEC_PA_PIN);
		gpio_request(pa_gpio, "codec_pa_gpio");
		gpio_direction_output(pa_gpio, enable);
		gpio_free(pa_gpio);
	}

	return 0;
}

static void comip_init_codec(void)
{
	lc1160_codec_platform_data.ear_switch = ear_switch;
	lc1160_codec_platform_data.pa_enable = pa_boost_enable;

}

static void comip_muxpin_pvdd_config(void)
{
	comip_pvdd_vol_ctrl_config(MUXPIN_PVDD4_VOL_CTRL,PVDDx_VOL_CTRL_1_8V);
}

static void __init comip_init_board_early(void)
{
	comip_muxpin_pvdd_config();
	comip_init_mfp();
	comip_init_gpio_lp();
	comip_init_i2c();
	comip_init_lcd();
	comip_init_codec();
}
static void __init comip_init_board(void)
{
	comip_init_camera();
	comip_init_devices();
	comip_init_misc();
	comip_init_ts_virtual_key();

	comip_mmc0_info.setflags = comip_set_mmc0_info_ext;
}

