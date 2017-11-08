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

#if defined(CONFIG_LIGHT_PROXIMITY_GP2AP030)
#include <plat/gp2ap030.h>
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
#if defined(CONFIG_SENSORS_L3GD20)
#include <plat/l3gd20.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#include <plat/ft5x06_ts.h>
#endif

#include "board-usl90.h"

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
	.ac_cin_limit_currentmA = 1000,
	.ac_battery_currentmA   = 1000,
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
/*max17058 fuel gauge model data table*/
static int fg_batt_model_table[64] = {
	0xAA,0x00,0xB6,0xC0,0xB7,0xE0,0xB9,0x30,
	0xBA,0xF0,0xBB,0x90,0xBC,0xA0,0xBD,0x40,
	0xBE,0x20,0xBF,0x40,0xC0,0xD0,0xC3,0xF0,
	0xC6,0xE0,0xCC,0x90,0xD1,0xA0,0xD7,0x30,
	0x01,0xC0,0x0E,0xC0,0x2C,0x00,0x1C,0x00,
	0x00,0xC0,0x46,0x00,0x51,0xE0,0x31,0xE0,
	0x22,0x80,0x2B,0xE0,0x13,0xC0,0x15,0xA0,
	0x10,0x20,0x10,0x20,0x0E,0x00,0x0E,0x00,
};

struct comip_fuelgauge_info fg_data = {
	.fg_model_tbl = fg_batt_model_table,
	.tblsize = ARRAY_SIZE(fg_batt_model_table),
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

#if defined(CONFIG_CHARGER_NCP1851)
	{EXTERN_CHARGER_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif

	/* LCD. */
	{LCD_RESET_PIN,			MFP_PIN_MODE_GPIO},
#if defined(CONFIG_LCD_TRULY_NT35595)
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
/*Camera*/
#if defined(CONFIG_VIDEO_OV5648_2LANE_19M)
	{OV5648_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV5648_RESET_PIN,              MFP_PIN_MODE_GPIO},
#endif

};

static struct mfp_pull_cfg comip_mfp_pull_cfg[] = {

#if defined(CONFIG_BATTERY_MAX17058)
	{MAX17058_FG_INT_PIN,	MFP_PULL_UP},
#endif

#if defined(CONFIG_CHARGER_NCP1851)
	{EXTERN_CHARGER_INT_PIN,	MFP_PULL_UP},
#endif

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
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR,		0,	0}
};

static struct pmic_power_ctrl_map lc1160_power_ctrl_map[] = {
	{PMIC_ALDO1,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO8,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	10},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	80},
};

static struct pmic_reg_st lc1160_init_regs[] = {
	/* ALDO7/10/11/12 disable when sleep */
	{LC1160_REG_LDOA_SLEEP_MODE2, 0x00, LC1160_REG_BITMASK_LDOA7_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE3, 0x00, LC1160_REG_BITMASK_LDOA10_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE3, 0x00, LC1160_REG_BITMASK_LDOA11_ALLOW_IN_SLP},
	{LC1160_REG_LDOA_SLEEP_MODE3, 0x00, LC1160_REG_BITMASK_LDOA12_ALLOW_IN_SLP},
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
	.position = 7,
	.mode = MODE_2G,
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

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,
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
	/**1080p**/
	return sprintf(buf,
	               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":200:2000:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":530:2000:100:90"
	               ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":870:2000:100:90"
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
	.max_scrx = 1080,
	.max_scry = 1920,
	.virtual_scry = 2200,
	.max_points = 5,
};
#endif


/* NFC. */
static struct i2c_board_info comip_i2c0_board_info[] = {
	// TODO:  to be continue for device
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
	/* accel */
#if defined(CONFIG_SENSORS_MMA8X5X)
	{
		I2C_BOARD_INFO("mma8x5x", 0x1D),
		.platform_data = &mma8x5x_pdata,
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
		.addr = 0x0c,
		.platform_data = &st480_pdata,
	},

#endif
#if defined(CONFIG_SENSORS_MMC3416XPJ_MAG)
	{
		I2C_BOARD_INFO("mmc3416x", 0x30),
	},
#endif
	/*gyroscope*/
#if defined(CONFIG_SENSORS_L3GD20)
	{
		I2C_BOARD_INFO("l3gd20_gyr", 0x6b),
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
	},
#endif

#if defined(CONFIG_CHARGER_NCP1851)
	{
		I2C_BOARD_INFO("ncp1851", 0x36),
		.platform_data = &exchg_data,
		.irq = mfp_to_gpio(EXTERN_CHARGER_INT_PIN),
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

#if defined(CONFIG_FB_COMIP2) || defined(CONFIG_FB_COMIP2_MODULE)

#if defined(CONFIG_LCD_TRULY_NT35595)
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
	return LCD_ID_TRULY_NT35595;
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
#if defined(CONFIG_VIDEO_OV5648_2LANE_19M)
static int ov5648_pwdn = mfp_to_gpio(OV5648_POWERDOWN_PIN);
static int ov5648_rst = mfp_to_gpio(OV5648_RESET_PIN);
static int ov5648_power(int onoff)
{
	printk("ov5648 power : %d\n", onoff);

	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_request(ov5648_pwdn, "OV5648 Powerdown");
		gpio_direction_output(ov5648_pwdn, 1);//OV5648 different from OV5647
		msleep(50);
		gpio_free(ov5648_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(ov5648_pwdn, "OV5648 Powerdown");
		gpio_direction_output(ov5648_pwdn, 0);
		gpio_free(ov5648_pwdn);
	}

	return 0;
}

static int ov5648_reset(void)
{
	printk("ov5648 reset\n");

	gpio_request(ov5648_rst, "OV5648 Reset");
	gpio_direction_output(ov5648_rst, 0);
	msleep(50);
	gpio_direction_output(ov5648_rst, 1);
	msleep(50);
	gpio_free(ov5648_rst);

	return 0;
}

static struct i2c_board_info ov5648_board_info = {
	.type = "ov5648",
	.addr = 0x36,
};
#endif

static struct comip_camera_client comip_camera_clients[] = {
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
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 1,
		.mipi_lane_num = 2,
		.power = ov5648_power,
		.reset = ov5648_reset,
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

