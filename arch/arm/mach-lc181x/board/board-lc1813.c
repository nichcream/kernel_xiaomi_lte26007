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
#include <linux/akm8963.h>
//#include <linux/ltr558als.h>
#include <linux/i2c/apds990x.h>
#include <linux/input/lis3dh.h>
//#include <linux/mma845x.h>
#include <linux/mmc/host.h>

#include <plat/ft5x06_ts.h>
#include <plat/i2c.h>
#include <plat/camera.h>
#include <plat/comipfb.h>
#include <plat/comip-backlight.h>
#include <plat/comip-thermal.h>
#include <plat/comip-pmic.h>
#include <plat/comip-battery.h>
#if defined(CONFIG_COMIP_LC1100HT)
#include <plat/lc1100ht.h>
#endif
#if defined(CONFIG_COMIP_LC1100H)
#include <plat/lc1100h.h>
#endif
#if defined(CONFIG_COMIP_LC1132)
#include <plat/lc1132.h>
#include <plat/lc1132-pmic.h>
#include <plat/lc1132_adc.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_GT9XX)
#include <linux/input/gt9xx.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_GT813)
#include <linux/input/gt813_827_828.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_ICN83XX)
#include <linux/input/icn83xx.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_BL546X)
#include <linux/input/bl546x_ts.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_NVT1100X)
#include <linux/input/NVTtouch.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_RMI4_SYNAPTICS)
#include <linux/input/rmi.h>
#endif

#include <linux/cm36283.h>

#include "board-lc1813.h"
#include "board-common.c"
#include "brcm.c"

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

#if defined(CONFIG_BATTERY_BQ27510)
struct comip_fuelgauge_info fg_data ={
    .normal_capacity = 1650,
    .firmware_version = 0x16000a4,
};
#endif
#ifdef CONFIG_CHARGER_BQ24158

struct extern_charger_platform_data exchg_data ={
    .max_charger_currentmA  = 1250,
    .max_charger_voltagemV  = 4200,
    .termination_currentmA  = 50,
    .dpm_voltagemV          = 4440,
    .usb_cin_limit_currentmA= 500,
    .usb_battery_currentmA  = 550,
    .ac_cin_limit_currentmA = 900,
    .ac_battery_currentmA   = 900,
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

static struct platform_device *devices[] __initdata = {
#if defined(CONFIG_BACKLIGHT_COMIP)
	&comip_backlight_device,
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
#if defined(CONFIG_COMIP_LC1100HT)
	/* LC1100HT. */
	{LC1100HT_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_COMIP_LC1100H)
	/* LC1100H. */
	{LC1100H_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_COMIP_LC1120)
	/* LC1120. */
	{LC1120_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_COMIP_LC1132)
	/* LC1132. */
	{LC1132_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_BATTERY_BQ27510)
	{BQ27510_GASGAUGE_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_CHARGER_BQ24158)
	{EXTERN_CHARGER_INT_PIN,	MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_PN65N_NFC)
	/* NFC-PN65N. */
	{PN65N_IRQ_PIN,			MFP_PIN_MODE_GPIO},
	{PN65N_POWEREN_PIN,		MFP_PIN_MODE_GPIO},
	{PN65N_FIRMWARE_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	{FT5X06_RST_PIN,		MFP_PIN_MODE_GPIO},
	{FT5X06_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_ICN83XX)
	{ICN83XX_RST_PIN,		MFP_PIN_MODE_GPIO},
	{ICN83XX_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_BL546X)
	{BL546X_RST_PIN,		MFP_PIN_MODE_GPIO},
	{BL546X_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_NVT1100X)
	{NVT1100X_RST_PIN,		MFP_PIN_MODE_GPIO},
	{NVT1100X_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_GT9XX)
	{GT9xx_RST_PIN,         MFP_PIN_MODE_GPIO},
	{GT9xx_INT_PIN,         MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_GT813)
	{GT816_RST_PIN,			MFP_PIN_MODE_GPIO},
	{GT816_INT_PIN,			MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_TOUCHSCREEN_RMI4_SYNAPTICS)
	{TM2049_ATTN,			MFP_PIN_MODE_GPIO},
	{TM2049_RST,			MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_MPU_SENSORS_MPU3050)
	{MPU3050_INT_PIN,		MFP_PIN_MODE_GPIO },
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_CM36283)
	{CM36283_INT_PIN,		MFP_PIN_MODE_GPIO },
#endif
#if defined(CONFIG_VIDEO_SGM3141)
	{SGM3141_ENF_PIN,		MFP_PIN_MODE_GPIO},
	{SGM3141_ENM_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV9724)
	/* OV9724. */
	{OV9724_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV9724_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV9760)
	/* OV9760. */
	{OV9760_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV9760_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV3650)
	/* OV3650. */
	{OV3650_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV3650_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV5647)
	/* OV5647. */
	{OV5647_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV5647_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV5648)
        /* OV5648. */
        {OV5648_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
        {OV5648_RESET_PIN,              MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV5648_PIP)
	{OV5648_PIP_POWERDOWN_PIN,          MFP_PIN_MODE_GPIO},
	{OV5648_PIP_RESET_PIN,              MFP_PIN_MODE_GPIO},
	{OV7695_PIP_POWERDOWN_PIN,          MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV5693)
        /* OV5693. */
     {OV5693_POWERDOWN_PIN,			MFP_PIN_MODE_GPIO},
     {OV5693_RESET_PIN,         MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_OV8825)
	/* OV8825. */
	{OV8825_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV8825_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_VIDEO_OV8850)
	/* OV8850. */
	{OV8850_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV8850_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_VIDEO_OV12830)
	/* OV12830. */
	{OV12830_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{OV12830_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_HM5065)
	/* HM5065. */
	{HM5065_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{HM5065_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC2035)
	/* GC2035. */
	{GC2035_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC2035_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC2035_MIPI)
    /* GC2035_MIPI. */
	{GC2035_MIPI_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC2035_MIPI_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC2235)
	/* GC2235. */
	{GC2235_POWERDOWN_PIN, 	MFP_PIN_MODE_GPIO},
	{GC2235_RESET_PIN, 	MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC5004)
	/* GC5004. */
	{GC5004_POWERDOWN_PIN, 	MFP_PIN_MODE_GPIO},
	{GC5004_RESET_PIN, 	MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC0328)
	/* GC0328. */
	{GC0328_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC0328_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_GC0329)
	/* GC0329. */
	{GC0329_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{GC0329_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_VIDEO_BF3A50)
	/* BF3A50. */
	{BF3A50_POWERDOWN_PIN,		MFP_PIN_MODE_GPIO},
	{BF3A50_RESET_PIN,		MFP_PIN_MODE_GPIO},
#endif

/*  LCD GPIO */
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

};

static struct mfp_pull_cfg comip_mfp_pull_cfg[] = {
	{MFP_PULL_MMC1_CMD,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_7,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_6,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_5,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_4,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_3,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_2,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_1,		MFP_PULL_DISABLE},
	{MFP_PULL_MMC1_DATA_0,		MFP_PULL_DISABLE},

#if defined(CONFIG_BATTERY_BQ27510)
	{BQ27510_GASGAUGE_INT_PIN,	MFP_PULL_UP},
#endif

#if defined(CONFIG_CHARGER_BQ24158)
	{EXTERN_CHARGER_INT_PIN,	MFP_PULL_UP},
#endif
};

static void __init comip_init_mfp(void)
{
	comip_mfp_config_array(comip_mfp_cfg, ARRAY_SIZE(comip_mfp_cfg));
	comip_mfp_config_pull_array(comip_mfp_pull_cfg, ARRAY_SIZE(comip_mfp_pull_cfg));
}

#if defined(CONFIG_I2C_COMIP) || defined(CONFIG_I2C_COMIP_MODULE)

#if defined(CONFIG_COMIP_LC1100HT)

static struct pmic_power_module_map lc1100ht_power_module_map[] = {
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		1,		0},
	{PMIC_DLDO3,	PMIC_POWER_CAMERA_AF_MOTOR,	0,		0},
	{PMIC_DLDO4,	PMIC_POWER_USIM,		0,		0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,		1,      	0},
	{PMIC_DLDO5,	PMIC_POWER_TOUCHSCREEN_I2C,	0,      	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,			0,		0},
	{PMIC_DLDO8,	PMIC_POWER_SDIO,		0,		0},
	{PMIC_DLDO9,	PMIC_POWER_SWITCH,		0,		0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_CORE,		0,		0},
	{PMIC_DLDO11,	PMIC_POWER_CMMB_BB_CORE,	0,		0},
	{PMIC_ALDO1,  	PMIC_POWER_RF_GSM_CORE,		1,  		1},
	{PMIC_ALDO2,  	PMIC_POWER_TOUCHSCREEN,		0,  		0},
	{PMIC_ALDO4,  	PMIC_POWER_RF_GSM_IO,		1,  		1},
	{PMIC_ALDO6,	PMIC_POWER_CAMERA_ANALOG,	1,		0},
	{PMIC_ALDO7,	PMIC_POWER_CMMB_RF_CORE,	0,		0},
	{PMIC_ALDO9,  	PMIC_POWER_CAMERA_CSI_PHY,	0,  		0},
	{PMIC_ALDO10,  	PMIC_POWER_HDMI_CORE,		0,  		0},
	{PMIC_ISINK2, 	PMIC_POWER_LED,			PMIC_LED_KEYPAD,0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR,		0, 		0},
	{PMIC_EXTERNAL1,PMIC_POWER_LED,			PMIC_LED_LCD,	1},
};

static struct pmic_power_ctrl_map lc1100ht_power_ctrl_map[] = {
	{PMIC_DLDO1, 	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	3000},
	{PMIC_DLDO9, 	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2800},
	{PMIC_DLDO11,	PMIC_POWER_CTRL_REG, 	PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
	{PMIC_ALDO2,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO3,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO7,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	120},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	120},
};

static struct pmic_reg_st lc1100ht_init_regs[] = {
	/* Set SINK PWM code.*/
	{LC1100HT_REG_SINK1_PWM, 0x1f, 0xff},
	{LC1100HT_REG_SINK2_PWM, 0x1f, 0xff},
	{LC1100HT_REG_SINK3_PWM, 0x2a, 0xff},

	/* Allow ldo in sleep.*/
	{LC1100HT_REG_LDO_SLEEP_CTL_1, 0x7f, 0xff},
	{LC1100HT_REG_LDO_SLEEP_CTL_2, 0x06, 0xff},
};

static struct lc1100ht_platform_data i2c_lc1100ht_info = {
	.flags = LC1100HT_FLAGS_DEVICE_CHECK,
	.irq_gpio = LC1100HT_INT_PIN,
	.ctrl_map = lc1100ht_power_ctrl_map,
	.ctrl_map_num = ARRAY_SIZE(lc1100ht_power_ctrl_map),
	.module_map = lc1100ht_power_module_map,
	.module_map_num = ARRAY_SIZE(lc1100ht_power_module_map),
	.init_regs = lc1100ht_init_regs,
	.init_regs_num = ARRAY_SIZE(lc1100ht_init_regs),
};
#endif

#if defined(CONFIG_COMIP_LC1100H)

static struct pmic_power_module_map lc1100h_power_module_map[] = {
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		0,	0},
	{PMIC_DLDO2,	PMIC_POWER_CAMERA_IO,		1,	0},
	{PMIC_DLDO3,	PMIC_POWER_CAMERA_AF_MOTOR,	0,	0},
	{PMIC_DLDO4,	PMIC_POWER_USIM,		0,	0},
	{PMIC_DLDO5,	PMIC_POWER_USIM,		1,     	0},
	{PMIC_DLDO6,	PMIC_POWER_USB,			0,	0},
	{PMIC_DLDO8,	PMIC_POWER_SDIO,		0,	0},
	{PMIC_DLDO9,	PMIC_POWER_SWITCH,		0,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_CORE,		0,	0},
	{PMIC_DLDO10,	PMIC_POWER_CAMERA_CORE, 	1,	0},
	{PMIC_DLDO11,	PMIC_POWER_CMMB_BB_CORE,	0,	0},
	{PMIC_ALDO2, 	PMIC_POWER_TOUCHSCREEN,         0,	1},
	{PMIC_ALDO3,	PMIC_POWER_CAMERA_ANALOG,	0,	0},
	{PMIC_ALDO3,	PMIC_POWER_CAMERA_ANALOG,	1,	0},
	{PMIC_ALDO4,    PMIC_POWER_LCD_CORE,     	0,      1},
	{PMIC_ALDO6,	PMIC_POWER_LCD_IO,		0,	1},
	{PMIC_ALDO6,	PMIC_POWER_TOUCHSCREEN_IO,	0,	1},
	{PMIC_ALDO7,	PMIC_POWER_CMMB_RF_CORE, 	0,	0},
	{PMIC_ALDO9, 	PMIC_POWER_CAMERA_CSI_PHY,      0,	0},
	{PMIC_ISINK3,	PMIC_POWER_VIBRATOR, 		0, 	0}
};

static struct pmic_power_ctrl_map lc1100h_power_ctrl_map[] = {
	{PMIC_DLDO2,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ALDO1,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO2,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO3,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO6,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ALDO7,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO9,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO10,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1500},
	{PMIC_DLDO11,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	60},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	80},
};

static struct pmic_reg_st lc1100h_init_regs[] = {
	/* Allow DCDC1~4 in sleep.*/
	{LC1100H_REG_DCMODE, 0x0f, 0xf0},
	/* Allow aldo8 in sleep.*/
	{LC1100H_REG_LDOA_SLEEP_MODE1, 0x00, 0x80},
	/* Close aldo10 .*/
	{LC1100H_REG_LDOAEN2, 0x00, 0x02}
};

static struct lc1100h_platform_data i2c_lc1100h_info = {
	.flags = LC1100H_FLAGS_DEVICE_CHECK,
	.irq_gpio = mfp_to_gpio(LC1100H_INT_PIN),
	.ctrl_map = lc1100h_power_ctrl_map,
	.ctrl_map_num = ARRAY_SIZE(lc1100h_power_ctrl_map),
	.module_map = lc1100h_power_module_map,
	.module_map_num = ARRAY_SIZE(lc1100h_power_module_map),
	.init_regs = lc1100h_init_regs,
	.init_regs_num = ARRAY_SIZE(lc1100h_init_regs),
};

#endif

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
	.flags = LC1132_FLAGS_DEVICE_CHECK,
	.pmic_data = &lc1132_pmic_info,
};

#endif

#if defined(CONFIG_PN65N_NFC)
static int pn65n_ven = mfp_to_gpio(PN65N_POWEREN_PIN);
static int pn65n_firmware = mfp_to_gpio(PN65N_FIRMWARE_PIN);
static int pn65n_power(int onoff)
{
	printk("############### pn65n power : %d###################\n", onoff);

	gpio_request(pn65n_ven, "PN65N Ven");
	gpio_request(pn65n_firmware, "PN65N Download");

	if (onoff) {
		gpio_direction_output(pn65n_firmware, 0);
		gpio_direction_output(pn65n_ven, 1);
		msleep(10);
	} else {
		gpio_direction_output(pn65n_firmware, 0);
		msleep(10);
		gpio_direction_output(pn65n_ven, 0);
		msleep(10);
	}

	gpio_free(pn65n_ven);
	gpio_free(pn65n_firmware);

	return 0;
}

static int pn65n_reset(void)
{
	printk("############### pn65n reset ###################\n");

	gpio_request(pn65n_ven, "PN65N Ven");
	gpio_request(pn65n_firmware, "PN65N Download");

	gpio_direction_output(pn65n_firmware, 1);
	gpio_direction_output(pn65n_ven, 1);
	msleep(10);
	gpio_direction_output(pn65n_ven, 0);
	msleep(50);
	gpio_direction_output(pn65n_ven, 1);
	msleep(10);

	gpio_free(pn65n_ven);
	gpio_free(pn65n_firmware);

	return 0;
}

static struct pn65n_nfc_platform_data comip_i2c_pn65n_info = {
	.power = pn65n_power,
	.reset = pn65n_reset,
	.irq_gpio = mfp_to_gpio(PN65N_IRQ_PIN),
};
#endif

/*ALS + PS*/
#if defined(CONFIG_LIGHT_PROXIMITY_APDS990X)
static struct apds990x_platform_data comip_i2c_apds990x_info = {
	.gain = 0x20,
	.als_time = 0xDB,
	.int_gpio = mfp_to_gpio(APDS990X_INT_PIN),
};
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_CM36283)
static struct cm36283_platform_data comip_i2c_cm36283_info = {
	.prox_threshold_hi = 100,
	.prox_threshold_lo = 80,
};
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_TMD22713T)
static struct taos_cfg comip_i2c_tmd22713_info = {
	.calibrate_target = 300000,
	.als_time = 50,//200,
	.scale_factor = 1,
	.gain_trim = 512,
	.filter_history = 3,
	.filter_count = 1,
	.gain = 1,//2,
	.prox_threshold_hi = 500,//120,
	.prox_threshold_lo = 400,//80,
	.als_threshold_hi = 3000,
	.als_threshold_lo = 10,
	.prox_int_time = 0xee, /* 50ms */
	.prox_adc_time = 0xff,
	.prox_wait_time = 0xee,
	.prox_intr_filter = 0x11,//0x23,
	.prox_config = 0,
	.prox_pulse_cnt = 0x08,
	.prox_gain = 0x61,//0x62,
    .int_gpio = mfp_to_gpio(TMD22713T_INT_PIN),
};
#endif
/*gyroscope*/
#if defined(CONFIG_MPU_SENSORS_MPU3050)
static struct mpu_platform_data mpu3050_data = {
	.int_config  = 0x10,
	.orientation = { -1, 0, 0,
					0, -1, 0,
					0, 0, 1 },
};
#endif
/* compass */
#if defined(CONFIG_MPU_SENSORS_AK8975)
static struct ext_slave_platform_data inv_mpu_akm8975_data = {
	.bus         = EXT_SLAVE_BUS_PRIMARY,
	.orientation = { -1, 0, 0,
							0, -1, 0,
							0, 0, 1 },
};
#endif

#if defined(CONFIG_MPU_SENSORS_YAS530)//
static struct ext_slave_platform_data inv_mpu_yas530_data = {
	.bus         = EXT_SLAVE_BUS_PRIMARY,
	.orientation = { 0, 1, 0,
							-1, 0, 0,
							0, 0, 1 },
};
#endif
/* accel */
#if defined(CONFIG_MPU_SENSORS_MMA845X)
static struct ext_slave_platform_data inv_mpu_mma8452_data = {
	.bus         = EXT_SLAVE_BUS_SECONDARY,
	.orientation = { 1, 0, 0,
							0, 1, 0,
							0, 0, 1 },
};
#endif
#if defined(CONFIG_MPU_SENSORS_LIS3DH)
static struct ext_slave_platform_data inv_mpu_lis3dh_data = {
	.bus         = EXT_SLAVE_BUS_SECONDARY,
	.orientation = { 0, -1, 0,
							1, 0, 0,
							0, 0, 1 },
};
#endif
#if defined (CONFIG_SENSORS_MMA8452)
static struct mma845x_platform_data mma845x_pdata = {
	.position = 7,
	.mode = MODE_2G,
};
#endif
#if defined(CONFIG_SENSORS_AKM8963)
static struct akm8963_platform_data akm8963_pdata = {
        .layout = 3,
        .outbit = 1,
        .gpio_DRDY = AKM8963_INT_PIN,
        .gpio_RST = 0,
};
#endif
#if defined(CONFIG_SENSORS_LIS3DH)
static struct lis3dh_acc_platform_data  lis3dh_data = {
	.poll_interval = 66,//10
	.min_interval = 10,
	.g_range = 0,//2g
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 1,

	.init = NULL,
	.exit = NULL,
	.power_on = NULL,
	.power_off =NULL,

	.gpio_int1 = -EINVAL,//not use irq
	.gpio_int2 = -EINVAL,
};
#endif

#if defined(CONFIG_LIGHT_PROXIMITY_LTR558ALS)
static struct ltr558_platform_data ltr558_pdata = {
	.irq_gpio = mfp_to_gpio(LTR558ALS_INT_PIN),
};
#endif

#if defined(CONFIG_SENSORS_BMA250)
static struct bma250_data bmc_bma250_data = {
	.orientation = 0,//7 for 8150D phone
};
#endif

#if defined(CONFIG_SENSORS_BMM050)
static struct bma250_data bmc_bmm050_data = {
	.orientation = 0,//7 for 8150D phone
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
#if defined(CONFIG_TOUCHSCREEN_RMI4_SYNAPTICS)
        return sprintf(buf,
               __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":107:857:100:90"
           ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":246:848:100:90"
           ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":395:844:100:90"
           "\n");
#elif defined(CONFIG_TOUCHSCREEN_GT813)
	return sprintf(buf,
		       __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":72:846:100:60"
		   ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":215:846:100:60"
		   ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":407:846:100:60"
		   "\n");
#elif defined(CONFIG_TOUCHSCREEN_FT5X06)
	return sprintf(buf,
		   __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":73:1022:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":270:1022:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":466:1022:100:90"
		"\n");
#elif defined(CONFIG_TOUCHSCREEN_ICN83XX)
	return sprintf(buf,
		   __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":73:1022:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":270:1022:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":466:1022:100:90"
		"\n");
#elif defined(CONFIG_TOUCHSCREEN_BL546X)
	return sprintf(buf,
		   __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":70:1020:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":260:1020:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":460:1020:100:90"
		"\n");
#elif defined(CONFIG_TOUCHSCREEN_NVT1100X)
	return sprintf(buf,
		   __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":70:1020:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":260:1020:100:90"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":460:1020:100:90"
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
#if defined(CONFIG_TOUCHSCREEN_RMI4_SYNAPTICS)			
		.name = "virtualkeys.sensor00fn11",
#elif defined(CONFIG_TOUCHSCREEN_GT813)
		.name = "virtualkeys.gt816",
#elif defined(CONFIG_TOUCHSCREEN_FT5X06)
                .name = "virtualkeys.ft5x06",
#elif defined(CONFIG_TOUCHSCREEN_ICN83XX)
		.name = "virtualkeys.icn83xx",
#elif defined(CONFIG_TOUCHSCREEN_BL546X)
		.name = "virtualkeys.bl546x_ts",
#elif defined(CONFIG_TOUCHSCREEN_NVT1100X)
		.name = "virtualkeys.NVT-ts",

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
                printk("Create synaptics_ts virtual key properties failed!\n");
}

#if defined(CONFIG_TOUCHSCREEN_RMI4_SYNAPTICS)
struct syna_gpio_data {
	u16 gpio_number;
	char* gpio_name;
};

static int synaptics_touchpad_reset()
{
	int synaptics_ts_rst = TM2049_RST;
	gpio_request(synaptics_ts_rst, "synaptics_touchpad Reset");
	gpio_direction_output(synaptics_ts_rst, 1);
	gpio_free(synaptics_ts_rst);
	mdelay(120);
	return 0;
}

static int synaptics_touchpad_init()
{
	int gpio_rst = TM2049_RST;
	int i;
	int retval = 0;
	printk("rmi init\n");
//1.set i2c scl & i2c sda low
	ts_set_i2c_to_gpio();
//2.set reset pin gpio12 output low
	retval = gpio_request(gpio_rst, "synaptics_touchpad_init");
	if(retval){
		pr_err("rmi_init request gpio rst error\n");
		return retval;
	}
	retval = gpio_direction_output(gpio_rst, 0);
	if (retval) {
		pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
			 __func__, gpio_rst, retval);
		gpio_free(gpio_rst);
	}
	gpio_free(gpio_rst);
	mdelay(200);
	pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_I2C, 0, PMIC_POWER_VOLTAGE_ENABLE);
	pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
	comip_mfp_config_array(comip_mfp_cfg_i2c_2, ARRAY_SIZE(comip_mfp_cfg_i2c_2));
}
static int synaptics_touchpad_power_set(bool configure)
{
	int retval = 0;
	int i = 0;
	int gpio_rst = TM2049_RST;

	if(configure){
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_I2C, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
		comip_mfp_config_array(comip_mfp_cfg_i2c_2, ARRAY_SIZE(comip_mfp_cfg_i2c_2));
		synaptics_touchpad_reset();
	}else{
	        pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_I2C, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_DISABLE);
		retval = gpio_request(gpio_rst, "rmi_init");
		if(retval){
			pr_err("rmi_init request int 12 error\n");
			return retval;
		}
		gpio_direction_output(gpio_rst, 0);
		gpio_free(gpio_rst);
		ts_set_i2c_to_gpio();
	}

	return 0;
}
static int synaptics_touchpad_gpio_setup(void *gpio_data, bool configure)
{
	int retval=0;
	static int power_status = 0;
	struct syna_gpio_data *data = gpio_data;

	if (configure) {
		retval = gpio_request(data->gpio_number, "rmi4_attn");
		if (retval) {
			pr_err("%s: Failed to get attn gpio %d. Code: %d.",
			       __func__, data->gpio_number, retval);
			return retval;
		}
		printk("rmi gpio_num is %d\n", data->gpio_number);
		retval = gpio_direction_input(data->gpio_number);
		if (retval) {
			pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
			       __func__, data->gpio_number, retval);
			gpio_free(data->gpio_number);
		}
	} else {
		pr_warn("%s: No way to deconfigure gpio %d.",
		       __func__, data->gpio_number);
	}
	return retval;
}

static struct syna_gpio_data tm2049_gpiodata = {
	.gpio_number = mfp_to_gpio(TM2049_ATTN),
	.gpio_name = "irq.gpio_18",
};

static struct rmi_device_platform_data tm2049_platformdata = {
	.driver_name = "rmi_generic",
	.sensor_name = "Espresso",
	.attn_gpio = mfp_to_gpio(TM2049_ATTN),
	.attn_polarity = RMI_ATTN_ACTIVE_LOW,
	.level_triggered = true,/* irq trriger low*/
	.gpio_data = &tm2049_gpiodata,
	.gpio_config = synaptics_touchpad_gpio_setup,
	.axis_align = { },
	.power_set = synaptics_touchpad_power_set,
	.reset = synaptics_touchpad_reset,
	.init = synaptics_touchpad_init,
};

#endif
/* rmi synaptics touchpad*/

#if defined(CONFIG_TOUCHSCREEN_GT813)
static int gt816_rst = GT816_RST_PIN;
static void gt816_i2c_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
        if (vdd)
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_I2C, 0, PMIC_POWER_VOLTAGE_ENABLE);
        else
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_I2C, 0, PMIC_POWER_VOLTAGE_DISABLE);
}

static void gt816_ic_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd) {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}
}

static int gt816_reset(struct device *dev)
{
	gpio_request(gt816_rst, "gt816 Reset");
	gpio_direction_output(gt816_rst, 1);
	mdelay(10);
	gpio_direction_output(gt816_rst, 0);
	mdelay(100);
	gpio_direction_output(gt816_rst, 1);
	mdelay(200);
	gpio_free(gt816_rst);

	return 0;
}

static struct gt816_info comip_i2c_gt816_info = {
	.reset = gt816_reset,
	.power_i2c = gt816_i2c_power,
	.power_ic = gt816_ic_power,
	.irq_gpio = mfp_to_gpio(GT816_INT_PIN),
	.power_i2c_flag = 1,
	.power_ic_flag = 1,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_GT9XX)
static int gt9xx_rst = GT9xx_RST_PIN;

static int gt9xx_ic_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd) {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int gt9xx_reset(struct device *dev)
{
        printk("billy gt9xx_reset! \n");

        gpio_request(gt9xx_rst, "gt816 Reset");
        gpio_direction_output(gt9xx_rst, 1);
        mdelay(10);
        gpio_direction_output(gt9xx_rst, 0);
        mdelay(100);
        gpio_direction_output(gt9xx_rst, 1);
        mdelay(200);
        gpio_free(gt9xx_rst);

        return 0;
}

static struct gt9xx_info comip_i2c_gt9xx_info = {
        .reset = gt9xx_reset,
        .power_ic = gt9xx_ic_power,
        .irq_gpio = mfp_to_gpio(GT9xx_INT_PIN),
        .rst_gpio = mfp_to_gpio(GT9xx_RST_PIN),
        .power_i2c_flag = 1,
        .power_ic_flag = 1,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_ICN83XX)
static int icn83xx_rst = ICN83XX_RST_PIN;
static int icn83xx_i2c_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd) {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int icn83xx_ic_power(struct device *dev, unsigned int vdd)
{
	int retval = 0;
	int gpio_rst = ICN83XX_RST_PIN;
	/* Set power. */
	if (vdd){
		/*add for power on sequence*/
		retval = gpio_request(gpio_rst, "icn83xx_ts_rst");
		if(retval){
			pr_err("icn83xx_ts request rst error\n");
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
		retval = gpio_request(gpio_rst, "icn83xx_ts_rst");
		if(retval){
			pr_err("icn83xx_ts request rst error\n");
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

static int icn83xx_reset(struct device *dev)
{
	gpio_request(icn83xx_rst, "icn83xx Reset");

	gpio_direction_output(icn83xx_rst, 0);
	mdelay(30);
	gpio_direction_output(icn83xx_rst, 1);
	mdelay(50);
	gpio_free(icn83xx_rst);

	return 0;
}

static struct icn83xx_info comip_i2c_icn83xx_info = {
	.reset = icn83xx_reset,
	.power_i2c = icn83xx_i2c_power,
	.power_ic = icn83xx_ic_power,
	.irq_gpio = mfp_to_gpio(ICN83XX_INT_PIN),
	.power_i2c_flag = 1,
	.power_ic_flag = 1,
	.max_scrx = 540,
	.max_scry = 960,
	.max_points = 5,
};
#endif

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

#if defined(CONFIG_TOUCHSCREEN_BL546X)
static int bl546x_rst = BL546X_RST_PIN;
static int bl546x_int = BL546X_INT_PIN;
static int bl546x_wake = BL546X_INT_PIN;

static int bl546x_set_wake(struct device *dev, unsigned char wake)
{
        int retval = 0;

        retval = gpio_request(bl546x_wake, "bl546x_ts_wake");
        if (retval) {
                pr_err("bl546x_ts request wake error\n");
                return retval;
        }

        if (wake)
                gpio_direction_output(bl546x_wake, 1);
        else
                gpio_direction_output(bl546x_wake, 0);

        gpio_free(bl546x_wake);

        return retval;
}

static int bl546x_set_int(struct device *dev)
{
        int retval = 0;

        retval = gpio_request(bl546x_int, "bl546x_ts_int");
        if (retval) {
                pr_err("bl546x_ts request int error\n");
                return retval;
        }

        gpio_direction_input(bl546x_int);

        gpio_free(bl546x_int);

        return retval;
}

static int bl546x_ic_power(struct device *dev, unsigned int vdd)
{
	int retval = 0;

	/* Set power. */
	if (vdd) {
		/*add for power on sequence*/
		retval = gpio_request(bl546x_rst, "bl546x_ts_rst");
		if (retval) {
			pr_err("bl546x_ts request rst error\n");
			return retval;
		}
		gpio_direction_output(bl546x_rst, 0);
		mdelay(2);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(10);
		gpio_direction_output(bl546x_rst, 1);
		gpio_free(bl546x_rst);
		comip_mfp_config_array(comip_mfp_cfg_i2c_2, ARRAY_SIZE(comip_mfp_cfg_i2c_2));
	} else {
		retval = gpio_request(bl546x_rst, "bl546x_ts_rst");
		if(retval){
			pr_err("bl546x_ts request rst error\n");
			return retval;
		}
		gpio_direction_output(bl546x_rst, 0);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_DISABLE);
		gpio_direction_input(bl546x_rst);
		gpio_free(bl546x_rst);

		ts_set_i2c_to_gpio();
	}
	return 0;
}

static int bl546x_reset(struct device *dev)
{
        gpio_request(bl546x_rst, "bl546x Reset");
        gpio_direction_output(bl546x_rst, 1);
        mdelay(2);
        gpio_direction_output(bl546x_rst, 0);
        mdelay(10);
        gpio_direction_output(bl546x_rst, 1);
        mdelay(200);
        gpio_free(bl546x_rst);

        return 0;
}

KEYDEF virtual_key_arr[] =
{
        {"MENU", KEY_MENU, 70, 1020, 100, 90},
        {"HOME", KEY_HOME, 260 ,1020, 100, 90},
        {"BACK", KEY_BACK, 460, 1020, 100, 90}
};

static struct bl546x_info comip_i2c_bl546x_info = {
	.reset = bl546x_reset,
	.power_ic = bl546x_ic_power,
	.set_wake = bl546x_set_wake,
	.set_int = bl546x_set_int,
	.irq_gpio = mfp_to_gpio(BL546X_INT_PIN),
	.rst_gpio = mfp_to_gpio(BL546X_RST_PIN),
	.power_ic_flag = 1,
	.max_scrx = 540,
	.max_scry = 960,
	.revert_x_flag = 0,
	.revert_y_flag = 0,
	.exchange_x_y_flag = 0,
	.max_points = 5,
	.virtual_key_group = virtual_key_arr,
	.virtual_key_num = 3,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_NVT1100X)
static int NVTtouch_rst = NVT1100X_RST_PIN;
static int NVTtouch_i2c_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd) {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	} else {
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}

	return 0;
}

static int NVTtouch_ic_power(struct device *dev, unsigned int vdd)
{
	int retval = 0;
	int gpio_rst = NVT1100X_RST_PIN;
	/* Set power. */
	if (vdd){
		/*add for power on sequence*/
		retval = gpio_request(gpio_rst, "NVTtouch_ts_rst");
		if(retval){
			pr_err("NVTtouch_ts request rst error\n");
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
		retval = gpio_request(gpio_rst, "NVTtouch_ts_rst");
		if(retval){
			pr_err("NVTtouch_ts request rst error\n");
			return retval;
		}
		gpio_direction_output(gpio_rst, 0);
		mdelay(2);
		pmic_voltage_set(PMIC_POWER_TOUCHSCREEN, 0, PMIC_POWER_VOLTAGE_DISABLE);
		mdelay(10);
		gpio_direction_input(gpio_rst);
		gpio_free(gpio_rst);

		ts_set_i2c_to_gpio();
	}
	return 0;
}

static int NVTtouch_reset(struct device *dev)
{
	gpio_request(NVTtouch_rst, "NVTtouch Reset");
	gpio_direction_output(NVTtouch_rst, 1);
	mdelay(10);
	gpio_direction_output(NVTtouch_rst, 0);
	mdelay(10);
	gpio_direction_output(NVTtouch_rst, 1);
	mdelay(50);
	gpio_free(NVTtouch_rst);

	return 0;
}

static struct NVTtouch_info comip_i2c_NVTtouch_info = {
	.reset = NVTtouch_reset,
	.power_i2c = NVTtouch_i2c_power,
	.power_ic = NVTtouch_ic_power,
	.irq_gpio = mfp_to_gpio(NVT1100X_INT_PIN),
	.power_i2c_flag = 1,
	.power_ic_flag = 1,
	.max_scrx = 540,
	.max_scry = 960,
	.virtual_scry = 1150,
	.max_points = 10,
};
#endif

/* NFC. */
static struct i2c_board_info comip_i2c0_board_info[] = {
#if defined(CONFIG_PN65N_NFC)
	{
		.type = "pn65n",
		.addr = 0x28,
		.irq = gpio_to_irq(mfp_to_gpio(PN65N_IRQ_PIN)),
		.platform_data = &comip_i2c_pn65n_info,
	},
#endif
};

/* Sensors. */
static struct i2c_board_info comip_i2c1_board_info[] = {
#if defined(CONFIG_INPUT_AKM8963)
	{
		.type = "akm8963",
		.addr = 0x1c,
	},
#endif
#if defined(CONFIG_INPUT_BMC050)
	{
		.type = "bmc050",
		.addr = 0x03,
	},
#endif
#if defined(CONFIG_INPUT_YAS530)
	{
		.type = "yas530",
		.addr = 0x2e,
	},
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_TMD22713T)
	{
		.type = "tritonFN",
		.addr = 0x39,
		.platform_data = &comip_i2c_tmd22713_info,
	},
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_APDS990X)
	{
		.type = "apds990x",
		.addr = 0x39,
		.platform_data = &comip_i2c_apds990x_info,
	},
#endif
#if defined(CONFIG_SENSORS_MMA8452)
	{
		.type = "mma8452",
		.addr = 0x1d,
		.platform_data = &mma845x_pdata,
	},
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_TAOS_TMD22713T)
	{
		.type = "tritonFN",
		.addr = 0x39,
	},
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_CM36283)
	{
		.type = "cm36283",
		.addr = 0x60,
		.platform_data = &comip_i2c_cm36283_info,
		.irq = gpio_to_irq(CM36283_INT_PIN),
	},
#endif
/*gyroscope*/
#if defined(CONFIG_MPU_SENSORS_MPU3050)
	{
		I2C_BOARD_INFO("mpu3050", 0x68),
		.platform_data = &mpu3050_data,
		.irq = gpio_to_irq(MPU3050_INT_PIN),
	},
#endif
/*compass*/
#if defined(CONFIG_MPU_SENSORS_AK8975)
	{
		I2C_BOARD_INFO("ak8975", 0x0E),
		.platform_data = &inv_mpu_akm8975_data,
	},
#endif
#if defined(CONFIG_MPU_SENSORS_YAS530)
	{
		I2C_BOARD_INFO("yas530", 0x2e),
		.platform_data = &inv_mpu_yas530_data,
	},
#endif
/* accel */
#if defined(CONFIG_MPU_SENSORS_MMA845X)
	{
		I2C_BOARD_INFO("mma845x", 0x1d),
		.platform_data = &inv_mpu_mma8452_data,
	},
#endif
#if defined(CONFIG_MPU_SENSORS_LIS3DH)
	{
		I2C_BOARD_INFO("lis3dh", 0x18),
		.platform_data = &inv_mpu_lis3dh_data,
	},
#endif
#if defined(CONFIG_LIGHT_PROXIMITY_LTR558ALS)
	{
		.type = "ltr558als",
		.addr = 0x23,
		.platform_data = &ltr558_pdata,
	},
#endif
#if defined(CONFIG_SENSORS_LIS3DH)
	{
		.type = LIS3DH_ACC_DEV_NAME,
		.addr = 0x19,
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
#if defined(CONFIG_SENSORS_BMA250)
	{
		I2C_BOARD_INFO("bma250", 0x18),
		.platform_data = &bmc_bma250_data,
	},
#endif
#if defined(CONFIG_SENSORS_BMM050)
	{
		I2C_BOARD_INFO("bmm050", 0x10),
		.platform_data = &bmc_bmm050_data,
	},
#endif
};

/* TouchScreen. */
static struct i2c_board_info comip_i2c2_board_info[] = {
/***synaptics rmi4 touchscreen*/
#if defined(CONFIG_TOUCHSCREEN_RMI4_SYNAPTICS)
	{
		.type = "rmi_i2c",
		.addr = 0x20,
		.platform_data = &tm2049_platformdata,
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	{
		.type = "ft5x06",
		.addr = 0x38,
		.platform_data = &comip_i2c_ft5x06_info,
	},
#endif

#if defined(CONFIG_TOUCHSCREEN_ICN83XX)
	{
		.type = "icn83xx",
		.addr = 0x40,
		.platform_data = &comip_i2c_icn83xx_info,
	},
#endif

#if defined(CONFIG_TOUCHSCREEN_BL546X)
        {
                .type = CTP_NAME,
                .addr = 0x28,
                .platform_data = &comip_i2c_bl546x_info,
        },
#endif

#if defined(CONFIG_TOUCHSCREEN_NVT1100X)
        {
                .type = NVTTOUCH_I2C_NAME,
                .addr = 0x01,
                .platform_data = &comip_i2c_NVTtouch_info,
        },
#endif

#if defined(CONFIG_TOUCHSCREEN_GT9XX)
        {
                .type = "Goodix-TS",
                .addr = 0x14,
                .platform_data = &comip_i2c_gt9xx_info,
        },
#endif
#if defined(CONFIG_TOUCHSCREEN_GT813)
         {
                 .type = "Goodix-TS",
                 .addr = 0x5d,
                 .platform_data = &comip_i2c_gt816_info,
         },
 #endif
};

/* PMU&CODEC. */
static struct i2c_board_info comip_i2c4_board_info[] = {
#if defined(CONFIG_COMIP_LC1100HT)
	{
		.type = "lc1100ht",
		.addr = 0x70,
		.platform_data = &i2c_lc1100ht_info,
	},
#endif
#if defined(CONFIG_COMIP_LC1100H)
	{
		.type = "lc1100h",
		.addr = 0x33,
		.platform_data = &i2c_lc1100h_info,
	},
#endif
#if defined(CONFIG_COMIP_LC1120)
	{
		.type = "comip_1120",
		.addr = 0x1a,
	},
#endif
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
#if defined(CONFIG_BUCK_FAN53555)
	{
		.type = "fan53555",
		.addr = 0x60,
	},
#endif
#if defined(CONFIG_CHARGER_BQ24158)
	{
		I2C_BOARD_INFO("bq24158", 0x6a),
		.platform_data = &exchg_data,
		.irq = mfp_to_gpio(EXTERN_CHARGER_INT_PIN),
	},
#endif
#if defined(CONFIG_BATTERY_BQ27510)
	{
		I2C_BOARD_INFO("bq27510-battery", 0x55),
		.platform_data = &fg_data,
		.irq = mfp_to_gpio(BQ27510_GASGAUGE_INT_PIN),
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

#ifdef CONFIG_COMIP_BOARD_LC1813_EVB2
	voltage = pmic_get_adc_conversion(LCD_DETECT_ADC_CHANNEL);
	pr_info("%s: voltage=%d \n", __func__, voltage);

	if (voltage <= 80)
		lcd_id = LCD_ID_HS_RM68190;
	else
		lcd_id = LCD_ID_HS_NT35517;
#endif

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
#if defined(CONFIG_VIDEO_OV9724)
static int ov9724_pwdn = mfp_to_gpio(OV9724_POWERDOWN_PIN);
static int ov9724_rst = mfp_to_gpio(OV9724_RESET_PIN);
static int ov9724_power(int onoff)
{
	printk("############## ov9724 power : %d################\n", onoff);

	if (onoff) {

		gpio_request(ov9724_pwdn, "OV9724 Powerdown");
		gpio_direction_output(ov9724_pwdn, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(5);

		gpio_request(ov9724_pwdn, "OV9724 Powerdown");
		gpio_direction_output(ov9724_pwdn, 1);
		mdelay(50);

		gpio_free(ov9724_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(ov9724_pwdn, "OV9724 Powerdown");
		gpio_direction_output(ov9724_pwdn, 0);
		mdelay(10);
		gpio_free(ov9724_pwdn);
	}

	return 0;
}

static int ov9724_reset(void)
{
	printk("############## ov9724 reset################\n");

	gpio_request(ov9724_rst, "OV9724 Reset");
	gpio_direction_output(ov9724_rst, 0);
	mdelay(50);
	gpio_direction_output(ov9724_rst, 1);
	mdelay(50);
	gpio_free(ov9724_rst);

	return 0;
}

static struct i2c_board_info ov9724_board_info = {
	.type = "ov9724",
	.addr = 0x10,
};
#endif

#if defined(CONFIG_VIDEO_OV9760)
static int ov9760_pwdn = mfp_to_gpio(OV9760_POWERDOWN_PIN);
static int ov9760_rst = mfp_to_gpio(OV9760_RESET_PIN);
static int ov9760_power(int onoff)
{
	printk("############## ov9760 power : %d################\n", onoff);

	if (onoff) {

		gpio_request(ov9760_pwdn, "OV9760 Powerdown");
		gpio_direction_output(ov9760_pwdn, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(5);
		
		gpio_request(ov9760_pwdn, "OV9760 Powerdown");
		gpio_direction_output(ov9760_pwdn, 1);
		mdelay(50);

		gpio_free(ov9760_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(ov9760_pwdn, "OV9760 Powerdown");
		gpio_direction_output(ov9760_pwdn, 0);
		mdelay(10);
		gpio_free(ov9760_pwdn);
	}

	return 0;
}

static int ov9760_reset(void)
{
	printk("############## ov9760 reset################\n");

	gpio_request(ov9760_rst, "OV9760 Reset");
	gpio_direction_output(ov9760_rst, 0);
	mdelay(50);
	gpio_direction_output(ov9760_rst, 1);
	mdelay(50);
	gpio_free(ov9760_rst);

	return 0;
}

static struct i2c_board_info ov9760_board_info = {
	.type = "ov9760",
	.addr = 0x10,
};
#endif

#if defined(CONFIG_VIDEO_OV3650)
static int ov3650_pwdn = mfp_to_gpio(OV3650_POWERDOWN_PIN);
static int ov3650_rst = mfp_to_gpio(OV3650_RESET_PIN);
static int ov3650_power(int onoff)
{
	printk("############## ov3650 power : %d################\n", onoff);

	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
	
		gpio_request(ov3650_pwdn, "OV3650 Powerdown");
		gpio_direction_output(ov3650_pwdn, 0);
		mdelay(50);

		gpio_free(ov3650_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		
		gpio_request(ov3650_pwdn, "OV3650 Powerdown");
		gpio_direction_output(ov3650_pwdn, 1);
		mdelay(10);
		gpio_free(ov3650_pwdn);
	}

	return 0;
}

static int ov3650_reset(void)
{
	printk("############## ov3650 reset################\n");

	gpio_request(ov3650_rst, "OV3650 Reset");
	gpio_direction_output(ov3650_rst, 0);
	mdelay(50);
	gpio_direction_output(ov3650_rst, 1);
	mdelay(50);
	gpio_free(ov3650_rst);

	return 0;
}

static struct i2c_board_info ov3650_board_info = {
	.type = "ov3650",
	.addr = 0x36,
};
#endif

#if defined(CONFIG_VIDEO_OV5647)
static int ov5647_pwdn = mfp_to_gpio(OV5647_POWERDOWN_PIN);
static int ov5647_rst = mfp_to_gpio(OV5647_RESET_PIN);
static int ov5647_power(int onoff)
{
	printk("############## ov5647 power : %d################\n", onoff);
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
	printk("############## ov5647 reset################\n");
#if 0
	gpio_request(ov5647_rst, "OV5647 Reset");
	gpio_direction_output(ov5647_rst, 0);
	mdelay(50);
	gpio_direction_output(ov5647_rst, 1);
	mdelay(50);
	gpio_free(ov5647_rst);
#endif
	return 0;
}

static struct comip_camera_subdev_platform_data ov5647_setting = {
	.flags = CAMERA_SUBDEV_FLAG_MIRROR,
};

static struct i2c_board_info ov5647_board_info = {
	.type = "ov5647",
	.addr = 0x36,
	.platform_data = &ov5647_setting,
};
#endif

#if defined(CONFIG_VIDEO_OV5648)
static int ov5648_pwdn = mfp_to_gpio(OV5648_POWERDOWN_PIN);
static int ov5648_rst = mfp_to_gpio(OV5648_RESET_PIN);
static int ov5648_power(int onoff)
{
	printk("############## ov5648 power : %d################\n", onoff);

	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
                pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_request(ov5648_pwdn, "OV5648 Powerdown");
		gpio_direction_output(ov5648_pwdn, 1);//OV5648 different from OV5647
		msleep(50);
		gpio_free(ov5648_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
                pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(ov5648_pwdn, "OV5648 Powerdown");
		gpio_direction_output(ov5648_pwdn, 0);
		gpio_free(ov5648_pwdn);
	}

	return 0;
}

static int ov5648_reset(void)
{
	printk("############## ov5648 reset################\n");

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

#if defined(CONFIG_VIDEO_OV5648_PIP)
static int ov5648_pip_pwdn = mfp_to_gpio(OV5648_PIP_POWERDOWN_PIN);
static int ov5648_pip_rst = mfp_to_gpio(OV5648_PIP_RESET_PIN);
static int ov7695_pip_pwdn = mfp_to_gpio(OV7695_PIP_POWERDOWN_PIN);

static int ov5648_pip_power(int onoff)
{
	printk("############## ov5648_pip power : %d################\n", onoff);
	gpio_request(ov5648_pip_pwdn, "OV5648_pip Powerdown");
	gpio_request(ov5648_pip_rst, "OV5648_pip Reset");
	gpio_request(ov7695_pip_pwdn, "OV7695_pip Powerdown");
	gpio_direction_output(ov7695_pip_pwdn, 1);

	if (onoff) {
		gpio_direction_output(ov5648_pip_pwdn, 1);
		gpio_direction_output(ov5648_pip_rst, 1);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(5);
		gpio_direction_output(ov5648_pip_pwdn, 1);
		gpio_direction_output(ov5648_pip_rst, 0);
		msleep(2);
		gpio_direction_output(ov5648_pip_rst, 1);
		msleep(30);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(ov5648_pip_pwdn, 0);
		gpio_direction_output(ov5648_pip_rst, 1);
	}

	gpio_free(ov5648_pip_pwdn);
	gpio_free(ov5648_pip_rst);
	gpio_free(ov7695_pip_pwdn);

	return 0;
}
static int ov5648_pip_reset(void)
{
	printk("############## ov5648_pip reset################\n");
#if 0
	gpio_request(ov5648_rst, "ov5648 Reset");
	gpio_direction_output(ov5648_rst, 0);
	mdelay(50);
	gpio_direction_output(ov5648_rst, 1);
	mdelay(50);
	gpio_free(ov5648_rst);
#endif
	return 0;
}

static struct i2c_board_info ov5648_pip_board_info = {
	.type = "ov5648_pip",
	.addr = 0x36,
};
static struct i2c_board_info ov7695_raw_pip_board_info = {
	.type = "ov7695_raw_pip",
	.addr = 0x21,
};

#endif

#if defined(CONFIG_VIDEO_OV5693)
static int ov5693_pwdn = mfp_to_gpio(OV5693_POWERDOWN_PIN);
static int ov5693_rst = mfp_to_gpio(OV5693_RESET_PIN);
static int ov5693_power(int onoff)
{
	printk("############## ov5693 power : %d################\n", onoff);

	if (onoff) {

		gpio_request(ov5693_pwdn, "OV5693 Powerdown");
		gpio_direction_output(ov5693_pwdn, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(5);

		gpio_request(ov5693_pwdn, "OV5693 Powerdown");
		gpio_direction_output(ov5693_pwdn, 1);
		mdelay(50);

		gpio_free(ov5693_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(ov5693_pwdn, "OV5693 Powerdown");
		gpio_direction_output(ov5693_pwdn, 0);
		mdelay(10);
		gpio_free(ov5693_pwdn);
	}

	return 0;
}

static int ov5693_reset(void)
{
	printk("############## ov5693 reset################\n");

	gpio_request(ov5693_rst, "OV5693 Reset");
	gpio_direction_output(ov5693_rst, 0);
	mdelay(50);
	gpio_direction_output(ov5693_rst, 1);
	mdelay(50);
	gpio_free(ov5693_rst);

	return 0;
}

static struct i2c_board_info ov5693_board_info = {
	.type = "ov5693",
	.addr = 0x36,
};

#endif

#if defined(CONFIG_VIDEO_OV8825)
static int ov8825_pwdn = mfp_to_gpio(OV8825_POWERDOWN_PIN);
static int ov8825_rst = mfp_to_gpio(OV8825_RESET_PIN);
static int ov8825_power(int onoff)
{
        printk("############## ov8825 power : %d################\n", onoff);

        if (onoff) {
                gpio_request(ov8825_rst, "OV8825 Reset Pin");
                gpio_request(ov8825_pwdn, "OV8825 Powerdown Pin");
                gpio_direction_output(ov8825_pwdn, 0);
                gpio_direction_output(ov8825_rst, 0);
		
                pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
                pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
                pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
                pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(6);
                gpio_direction_output(ov8825_pwdn, 1);
                msleep(2);
		gpio_direction_output(ov8825_rst, 1);
                gpio_free(ov8825_pwdn);
		gpio_free(ov8825_rst);
		msleep(20);

        } else {
                pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
                pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
                pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);
                gpio_request(ov8825_rst, "OV8825 Reset Pin");
                gpio_request(ov8825_pwdn, "OV8825 Powerdown Pin");

		gpio_direction_output(ov8825_pwdn, 0);
		gpio_direction_output(ov8825_rst, 0);
		gpio_free(ov8825_pwdn);
		gpio_free(ov8825_rst);

        }

        return 0;
}
static int ov8825_reset(void)
{
	printk("############## ov8825 reset################\n");
	return 0;
}

static struct i2c_board_info ov8825_board_info = {
	.type = "ov8825",
	.addr = 0x36,
};
#endif

#if defined(CONFIG_VIDEO_OV8850)
static int ov8850_pwdn = mfp_to_gpio(OV8850_POWERDOWN_PIN);
static int ov8850_rst = mfp_to_gpio(OV8850_RESET_PIN);
static int ov8850_power(int onoff)
{
	//onoff = 1;
	printk("############## ov8850 power : %d################\n", onoff);

	if (onoff) {

		gpio_request(ov8850_pwdn, "OV8850 Powerdown");
		gpio_direction_output(ov8850_pwdn, 0);//test by Richard

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(5);
	
		gpio_request(ov8850_pwdn, "OV8850 Powerdown");
		gpio_direction_output(ov8850_pwdn, 1);
		mdelay(50);

		gpio_free(ov8850_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(ov8850_pwdn, "OV8850 Powerdown");
		gpio_direction_output(ov8850_pwdn, 0);
		mdelay(10);
		gpio_free(ov8850_pwdn);
	}

	return 0;
}

static int ov8850_reset(void)
{
	printk("############## ov8850 reset################\n");

	gpio_request(ov8850_rst, "OV8850 Reset");
	gpio_direction_output(ov8850_rst, 0);
	mdelay(50);
	gpio_direction_output(ov8850_rst, 1);
	mdelay(50);
	gpio_free(ov8850_rst);

	return 0;
}

static struct i2c_board_info ov8850_board_info = {
	.type = "ov8850",
	.addr = 0x10,
};
#endif


#if defined(CONFIG_VIDEO_OV12830)
static int ov12830_pwdn = mfp_to_gpio(OV12830_POWERDOWN_PIN);
static int ov12830_rst = mfp_to_gpio(OV12830_RESET_PIN);
static int ov12830_power(int onoff)
{
	printk("############## ov12830 power : %d################\n", onoff);

	if (onoff) {

		gpio_request(ov12830_pwdn, "OV12830 Powerdown");
		gpio_direction_output(ov12830_pwdn, 0);//test by Richard

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(5);
	
		gpio_request(ov12830_pwdn, "OV12830 Powerdown");
		gpio_direction_output(ov12830_pwdn, 1);//for debug 1->0
		mdelay(50);

		gpio_free(ov12830_pwdn);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(ov12830_pwdn, "OV12830 Powerdown");
		gpio_direction_output(ov12830_pwdn, 0);
		mdelay(10);
		gpio_free(ov12830_pwdn);
	}

	return 0;
}

static int ov12830_reset(void)
{
	printk("############## ov12830 reset################\n");

	gpio_request(ov12830_rst, "OV12830 Reset");
	gpio_direction_output(ov12830_rst, 0);
	mdelay(50);
	gpio_direction_output(ov12830_rst, 1);
	mdelay(50);
	gpio_free(ov12830_rst);

	return 0;
}

static struct i2c_board_info ov12830_board_info = {
	.type = "ov12830",
	.addr = 0x10,
};
#endif

#if defined(CONFIG_VIDEO_HM5065)
static int hm5065_pwdn = mfp_to_gpio(HM5065_POWERDOWN_PIN);
static int hm5065_rst = mfp_to_gpio(HM5065_RESET_PIN);
static int hm5065_power(int onoff)
{
	printk("############## hm5065 power : %d################\n", onoff);

	gpio_request(hm5065_pwdn, "HM5065 Powerdown");
	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(hm5065_pwdn, 1);
		mdelay(1);

	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_direction_output(hm5065_pwdn, 0);
		mdelay(1);
	}
	gpio_free(hm5065_pwdn);
	return 0;
}

static int hm5065_reset(void)
{
	printk("############## hm5065 reset################\n");
	gpio_request(hm5065_rst, "HM5065 Reset");
	gpio_direction_output(hm5065_rst, 1);
	mdelay(50);
	gpio_direction_output(hm5065_rst, 0);
	mdelay(50);
	gpio_direction_output(hm5065_rst, 1);
	mdelay(50);
	gpio_free(hm5065_rst);
	return 0;
}

static struct i2c_board_info hm5065_board_info = {
	.type = "hm5065",
	.addr = 0x1f,
};
#endif

#if defined(CONFIG_VIDEO_GC2035)
static int gc2035_pwdn = mfp_to_gpio(GC2035_POWERDOWN_PIN);
static int gc2035_rst = mfp_to_gpio(GC2035_RESET_PIN);
static int gc2035_power(int onoff)
{
	printk("############## gc2035 power : %d################\n", onoff);

	if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_request(gc2035_pwdn, "GC2035 Powerdown");
		gpio_direction_output(gc2035_pwdn, 0);
		mdelay(1);

		gpio_free(gc2035_pwdn);
	} else {
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);

		gpio_request(gc2035_pwdn, "GC2035 Powerdown");
		gpio_direction_output(gc2035_pwdn, 1);
		mdelay(1);
		gpio_free(gc2035_pwdn);
	}

	return 0;
}

static int gc2035_reset(void)
{
	printk("############## gc2035 reset################\n");
	gpio_request(gc2035_rst, "GC2035 Reset");
	gpio_direction_output(gc2035_rst, 1);
	mdelay(50);
	gpio_direction_output(gc2035_rst, 0);
	mdelay(50);
	gpio_direction_output(gc2035_rst, 1);
	mdelay(50);
	gpio_free(gc2035_rst);
	return 0;
}

static struct i2c_board_info gc2035_board_info = {
	.type = "gc2035",
	.addr = 0x3c,
};
#endif

#if defined(CONFIG_VIDEO_GC2035_MIPI)
static int gc2035_mipi_pwdn = mfp_to_gpio(GC2035_MIPI_POWERDOWN_PIN);
static int gc2035_mipi_rst = mfp_to_gpio(GC2035_MIPI_RESET_PIN);
static int gc2035_mipi_power(int onoff)
{
    printk("############## gc2035_mipi power : %d################\n", onoff);

    if (onoff) {		
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);

		//pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);
		
        gpio_request(gc2035_mipi_pwdn, "GC2035_MIPI Powerdown Pin");
		gpio_direction_output(gc2035_mipi_pwdn, 0);
		mdelay(50);
		gpio_free(gc2035_mipi_pwdn);

    } else {
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);
        gpio_request(gc2035_mipi_pwdn, "GC2035_MIPI Powerdown Pin");

	    gpio_direction_output(gc2035_mipi_pwdn, 1);
		gpio_free(gc2035_mipi_pwdn);
    }

    return 0;
}
static int gc2035_mipi_reset(void)
{
	printk("############## GC2035_MIPI reset################\n");
	gpio_request(gc2035_mipi_rst, "GC2035_MIPI Reset");
	gpio_direction_output(gc2035_mipi_rst, 1);
	mdelay(50);
	gpio_direction_output(gc2035_mipi_rst, 0);
	mdelay(50);
	gpio_direction_output(gc2035_mipi_rst, 1);
	mdelay(50);
	gpio_free(gc2035_mipi_rst);
	return 0;
}

static struct i2c_board_info gc2035_mipi_board_info = {
	.type = "gc2035_mipi",
	.addr = 0x3c,
};
#endif

#if defined(CONFIG_VIDEO_GC2235)
static int gc2235_pwdn = mfp_to_gpio(GC2235_POWERDOWN_PIN);
static int gc2235_rst = mfp_to_gpio(GC2235_RESET_PIN);
static int gc2235_power(int onoff)
{
    printk("############## gc2235_mipi_raw power : %d################\n", onoff);

    if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);

        gpio_request(gc2235_pwdn, "GC2235 Powerdown Pin");
		gpio_direction_output(gc2235_pwdn, 0);
		udelay(50);
		gpio_free(gc2235_pwdn);

    } else {
        gpio_request(gc2235_pwdn, "GC2235 Powerdown Pin");
	    gpio_direction_output(gc2235_pwdn, 1);
		udelay(50);
		gpio_free(gc2235_pwdn);

		gpio_request(gc2235_rst, "GC2235 Reset");
		gpio_direction_output(gc2235_rst, 0);
		udelay(50);
		gpio_free(gc2235_rst);

        pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

    }

    return 0;
}

static int gc2235_reset(void)
{
	printk("############## GC2235 reset################\n");
	gpio_request(gc2235_rst, "GC2235 Reset");
	gpio_direction_output(gc2235_rst, 0);
	udelay(50);
	gpio_direction_output(gc2235_rst, 1);
	udelay(50);

	gpio_free(gc2235_rst);
	return 0;
}

static struct i2c_board_info gc2235_board_info = {
	.type = "gc2235",
	.addr = 0x3c,
};
#endif

#if defined(CONFIG_VIDEO_GC5004)
static int gc5004_pwdn = mfp_to_gpio(GC5004_POWERDOWN_PIN);
static int gc5004_rst = mfp_to_gpio(GC5004_RESET_PIN);
static int gc5004_power(int onoff)
{
    printk("############## GC5004 power : %d################\n", onoff);

    if (onoff) {
		pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_ENABLE);

        gpio_request(gc5004_pwdn, "GC5004 Powerdown Pin");
		gpio_direction_output(gc5004_pwdn, 0);
		udelay(50);
		gpio_free(gc5004_pwdn);

    } else {
        gpio_request(gc5004_pwdn, "GC5004 Powerdown Pin");
	    gpio_direction_output(gc5004_pwdn, 1);
		udelay(50);
		gpio_free(gc5004_pwdn);

		gpio_request(gc5004_rst, "GC5004 Reset");
		gpio_direction_output(gc5004_rst, 0);
		udelay(50);
		gpio_free(gc5004_rst);

        pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
        pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_AF_MOTOR, 0, PMIC_POWER_VOLTAGE_DISABLE);

    }

    return 0;
}

static int gc5004_reset(void)
{
	printk("############## GC5004 reset################\n");
	gpio_request(gc5004_rst, "GC5004 Reset");
	gpio_direction_output(gc5004_rst, 0);
	udelay(50);
	gpio_direction_output(gc5004_rst, 1);
	udelay(50);

	gpio_free(gc5004_rst);
	return 0;
}

static struct i2c_board_info gc5004_board_info = {
	.type = "gc5004",
	.addr = 0x36,
};
#endif

#if defined(CONFIG_VIDEO_GC0328)
static int gc0328_pwdn = mfp_to_gpio(GC0328_POWERDOWN_PIN);
static int gc0328_rst = mfp_to_gpio(GC0328_RESET_PIN);
static int gc0328_power(int onoff)
{
	printk("############## gc0328 power : %d################\n", onoff);
	gpio_request(gc0328_pwdn, "GC0328 Powerdown");
	gpio_request(gc0328_rst, "GC0328 Reset");

	if (onoff) {
		gpio_direction_output(gc0328_pwdn, 1);
		gpio_direction_output(gc0328_rst, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_DIGITAL, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(gc0328_pwdn, 0);
		msleep(5);
		gpio_direction_output(gc0328_rst, 1);
		msleep(5);
	} else {
		gpio_direction_output(gc0328_pwdn, 1);
		msleep(5);
		gpio_direction_output(gc0328_rst, 0);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_DIGITAL, 1, PMIC_POWER_VOLTAGE_DISABLE);
	}

	gpio_free(gc0328_pwdn);
	gpio_free(gc0328_rst);
	return 0;
}

static int gc0328_reset(void)
{
	printk("############## gc0328 reset################\n");
	return 0;
}

static struct comip_camera_subdev_platform_data gc0328_setting = {
	.flags = CAMERA_SUBDEV_FLAG_MIRROR | CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info gc0328_board_info = {
	.type = "gc0328",
	.addr = 0x21,
	.platform_data = &gc0328_setting,
};
#endif

#if defined(CONFIG_VIDEO_GC0329)
static int gc0329_pwdn = mfp_to_gpio(GC0329_POWERDOWN_PIN);
static int gc0329_rst = mfp_to_gpio(GC0329_RESET_PIN);
static int gc0329_power(int onoff)
{
	printk("############## gc0329 power : %d################\n", onoff);
	gpio_request(gc0329_pwdn, "GC0329 Powerdown");
	gpio_request(gc0329_rst, "GC0329 Reset");

	if (onoff) {
		gpio_direction_output(gc0329_pwdn, 1);
		gpio_direction_output(gc0329_rst, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_DIGITAL, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(gc0329_pwdn, 0);
		msleep(5);
		gpio_direction_output(gc0329_rst, 1);
		msleep(5);
	} else {
		gpio_direction_output(gc0329_pwdn, 1);
		msleep(5);
		gpio_direction_output(gc0329_rst, 0);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_DIGITAL, 1, PMIC_POWER_VOLTAGE_DISABLE);
	}

	gpio_free(gc0329_pwdn);
	gpio_free(gc0329_rst);
	return 0;
}

static int gc0329_reset(void)
{
	printk("############## gc0329 reset################\n");
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

#if defined(CONFIG_VIDEO_BF3A50)
static int bf3a50_pwdn = mfp_to_gpio(BF3A50_POWERDOWN_PIN);
static int bf3a50_rst = mfp_to_gpio(BF3A50_RESET_PIN);
static int bf3a50_power(int onoff)
{
	printk("############## bf3a50 power : %d################\n", onoff);
	gpio_request(bf3a50_pwdn, "BF3A50 Powerdown");
	gpio_request(bf3a50_rst, "BF3A50 Reset");

	if (onoff) {
		gpio_direction_output(bf3a50_pwdn, 1);
		gpio_direction_output(bf3a50_rst, 0);

		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_ENABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_ENABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_ENABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_DIGITAL, 1, PMIC_POWER_VOLTAGE_ENABLE);

		gpio_direction_output(bf3a50_pwdn, 0);
		msleep(5);
		gpio_direction_output(bf3a50_rst, 1);
		msleep(5);
	} else {
		gpio_direction_output(bf3a50_pwdn, 1);
		msleep(5);
		gpio_direction_output(bf3a50_rst, 0);
		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_CORE, 1, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_CAMERA_ANALOG, 1, PMIC_POWER_VOLTAGE_DISABLE);
		//pmic_voltage_set(PMIC_POWER_CAMERA_DIGITAL, 1, PMIC_POWER_VOLTAGE_DISABLE);
	}

	gpio_free(bf3a50_pwdn);
	gpio_free(bf3a50_rst);
	return 0;
}

static int bf3a50_reset(void)
{
	printk("############## bf3a50 reset################\n");
	return 0;
}

static struct comip_camera_subdev_platform_data bf3a50_setting = {
	.flags = CAMERA_SUBDEV_FLAG_MIRROR | CAMERA_SUBDEV_FLAG_FLIP,
};

static struct i2c_board_info bf3a50_board_info = {
	.type = "bf3a50",
	.addr = 0x6e,
	.platform_data = &bf3a50_setting,
};
#endif

#if defined(CONFIG_VIDEO_SGM3141)
static int sgm3141_gpio_enf = mfp_to_gpio(SGM3141_ENF_PIN);
static int sgm3141_gpio_enm = mfp_to_gpio(SGM3141_ENM_PIN);

static int sgm3141_set(enum camera_led_mode mode, int onoff)
{
	static unsigned char init_flag = 0;

	printk("############ sgm3141_set: mode=%d, onoff=%d. #############\n",
		mode, onoff);

	if (!init_flag) {
		gpio_request(sgm3141_gpio_enf, "SGM3141 ENF");
		gpio_request(sgm3141_gpio_enm, "SGM3141 ENM");
		gpio_direction_output(sgm3141_gpio_enf, 0);
		gpio_direction_output(sgm3141_gpio_enm, 0);
		gpio_free(sgm3141_gpio_enm);
		gpio_free(sgm3141_gpio_enf);
		init_flag = 1;
	}

	if (mode == FLASH) {
		gpio_request(sgm3141_gpio_enf, "SGM3141 ENF");
		gpio_request(sgm3141_gpio_enm, "SGM3141 ENM");
		if (onoff) {
			gpio_direction_output(sgm3141_gpio_enf, 1);
			gpio_direction_output(sgm3141_gpio_enm, 1);
		} else {
			gpio_direction_output(sgm3141_gpio_enf, 0);
			gpio_direction_output(sgm3141_gpio_enm, 0);
		}
		gpio_free(sgm3141_gpio_enm);
		gpio_free(sgm3141_gpio_enf);
	}
	else {
		gpio_request(sgm3141_gpio_enf, "SGM3141 ENF");
		gpio_request(sgm3141_gpio_enm, "SGM3141 ENM");
		if (onoff) {
			gpio_direction_output(sgm3141_gpio_enf, 0);
			gpio_direction_output(sgm3141_gpio_enm, 1);
		}
		else {
			gpio_direction_output(sgm3141_gpio_enf, 0);
			gpio_direction_output(sgm3141_gpio_enm, 0);
		}
		gpio_free(sgm3141_gpio_enm);
		gpio_free(sgm3141_gpio_enf);
	}

	return 0;
}

static int camera_flash(enum camera_led_mode mode, int onoff)
{
	return sgm3141_set(mode, onoff);
}
#else
static int camera_flash(enum camera_led_mode mode, int onoff)
{
	return 0;
}
#endif

static struct comip_camera_client comip_camera_clients[] = {
#if defined(CONFIG_VIDEO_OV3650)	
			{		
				.board_info = &ov3650_board_info,		
				.flags = CAMERA_CLIENT_IF_MIPI, 
				.caps = CAMERA_CAP_FOCUS_INFINITY
						| CAMERA_CAP_FOCUS_AUTO
						| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
						| CAMERA_CAP_METER_CENTER
						| CAMERA_CAP_METER_DOT
						| CAMERA_CAP_FACE_DETECT
						| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
				.if_id = 0, 	
				.mclk_rate = 26000000,		
				.mipi_lane_num = 1, 			
				.power = ov3650_power,		
				.reset = ov3650_reset,	
				.flash = camera_flash,
			},
#endif

#if defined(CONFIG_VIDEO_OV5647)
	{
		.board_info = &ov5647_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.power = ov5647_power,
		.reset = ov5647_reset,
		.flash = camera_flash,
	},
#endif
#if defined(CONFIG_VIDEO_OV5648)
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
		.if_id = 0,
		.mipi_lane_num = 2,
		.power = ov5648_power,
		.reset = ov5648_reset,
		.flash = camera_flash,
	},
#endif

#if defined(CONFIG_VIDEO_OV5648_PIP)
	{
		.board_info = &ov5648_pip_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
			| CAMERA_CAP_FOCUS_AUTO
			| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
			| CAMERA_CAP_METER_CENTER
			| CAMERA_CAP_METER_DOT
			| CAMERA_CAP_FACE_DETECT
			| CAMERA_CAP_ANTI_SHAKE_CAPTURE
			| CAMERA_CAP_FLASH
			| CAMERA_CAP_VIV_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.power = ov5648_pip_power,
		.reset = ov5648_pip_reset,
		.flash = camera_flash,
	},
	{
		.board_info = &ov7695_raw_pip_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.if_id = 0,//share primary mipi inerface with ov5648
		.mipi_lane_num = 2,
		.power = ov5648_pip_power,
		.reset = ov5648_pip_reset,
	},
#endif

#if defined(CONFIG_VIDEO_OV5693)
	{
		.board_info = &ov5693_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2, 
		.power = ov5693_power,
		.reset = ov5693_reset,
		.flash = camera_flash,
	},
#endif

#if defined(CONFIG_VIDEO_OV8825)
        {
		.board_info = &ov8825_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.power = ov8825_power,
		.reset = ov8825_reset,
		.flash = camera_flash,
        },
#endif
#if defined(CONFIG_VIDEO_OV8850)	
	{
		.board_info = &ov8850_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 4,
		.power = ov8850_power,
		.reset = ov8850_reset,
		.flash = camera_flash,
    },
#endif

#if defined(CONFIG_VIDEO_OV12830)
	{
		.board_info = &ov12830_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 4,  
		.power = ov12830_power,
		.reset = ov12830_reset,
		.flash = camera_flash,
	},
#endif

#if defined(CONFIG_VIDEO_HM5065)
	{
		.board_info = &hm5065_board_info,
		.flags = CAMERA_CLIENT_IF_DVP
				| CAMERA_CLIENT_CLK_EXT
				| CAMERA_CLIENT_YUV_DATA,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = hm5065_power,
		.reset = hm5065_reset,
	},
#endif

#if defined(CONFIG_VIDEO_GC2035)
	{
		.board_info = &gc2035_board_info,
		.flags = CAMERA_CLIENT_IF_DVP
				| CAMERA_CLIENT_CLK_EXT
				| CAMERA_CLIENT_YUV_DATA,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = gc2035_power,
		.reset = gc2035_reset,
	},
#endif
#if defined(CONFIG_VIDEO_GC2035_MIPI)
	{
		.board_info = &gc2035_mipi_board_info,
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
		.mipi_lane_num = 1,
		.mclk_rate = 26000000,
		.power = gc2035_mipi_power,
		.reset = gc2035_mipi_reset,
		.flash = camera_flash,
	},
#endif

#if defined(CONFIG_VIDEO_GC2235)
	{
		.board_info = &gc2235_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.power = gc2235_power,
		.reset = gc2235_reset,
		.flash = camera_flash,
	},
#endif


#if defined(CONFIG_VIDEO_GC5004)
	{
		.board_info = &gc5004_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,
		.mipi_lane_num = 2,
		.power = gc5004_power,
		.reset = gc5004_reset,
		.flash = camera_flash,
	},
#endif

#if defined(CONFIG_VIDEO_GC0328)
	{
		.board_info = &gc0328_board_info,
		.flags = CAMERA_CLIENT_IF_DVP
				| CAMERA_CLIENT_CLK_EXT
				| CAMERA_CLIENT_YUV_DATA,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = gc0328_power,
		.reset = gc0328_reset,
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

#if defined(CONFIG_VIDEO_BF3A50)
	{
		.board_info = &bf3a50_board_info,
		.flags = CAMERA_CLIENT_IF_DVP
				| CAMERA_CLIENT_CLK_EXT,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.mclk_parent_name = "pll1_mclk",
		.mclk_name = "clkout1_clk",
		.mclk_rate = 26000000,
		.power = bf3a50_power,
		.reset = bf3a50_reset,
	},
#endif

#if defined(CONFIG_VIDEO_OV9724)
	{
		.board_info = &ov9724_board_info,
		.flags = CAMERA_CLIENT_IF_MIPI,
		.caps = CAMERA_CAP_FOCUS_INFINITY
				| CAMERA_CAP_FOCUS_AUTO
				| CAMERA_CAP_FOCUS_CONTINUOUS_AUTO
				| CAMERA_CAP_METER_CENTER
				| CAMERA_CAP_METER_DOT
				| CAMERA_CAP_FACE_DETECT
				| CAMERA_CAP_ANTI_SHAKE_CAPTURE,
		.if_id = 0,//For front camera use
		.mipi_lane_num = 1,
		.power = ov9724_power,
		.reset = ov9724_reset,
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
		.if_id = 0,
		.mipi_lane_num = 1,
		.power = ov9760_power,
		.reset = ov9760_reset,
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
	comip_init_ts_virtual_key();
}

