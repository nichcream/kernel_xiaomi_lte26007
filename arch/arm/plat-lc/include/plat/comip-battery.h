/*
 * Copyright (C)
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _LINUX_LC_PMIC_H
#define _LINUX_LC_PMIC_H
#include <linux/notifier.h>

#define LC1100H_CLASS_ID      0x1100
#define LC1132_CLASS_ID       0x1132
/* chip-specific feature flags, for i2c_device_id.driver_data */
#define LC1100H_CLASS         BIT(0)
#define LC1132_CLASS          BIT(2)

#define RESUME_SYS_VOLT_OFFSET  (20)//mV
#define BOOT_VOLT_OFFSET        (30)//mV
#define RESUME_LCD_VOLT_OFFSET  (40)//mV

#define LCD_IS_ON   (1 << 0)
#define SYS_IS_ON   (1 << 1)

/*battery temp charging stage*/
#define BAT_TEMP_STAGE_COLD   (-10)
#define BAT_TEMP_STAGE_ZERO   (0)
#define BAT_TEMP_STAGE_COOL   (10)
#define BAT_TEMP_STAGE_NORMAL (45)
#define BAT_TEMP_STAGE_HOT    (50)
#define TEMP_OFFSET           (2)

/*batt temp stage status*/
enum temp_limit_charging_stage {
        TEMP_STAGE_OVERCOLD=0, /*[<-10]*/
        TEMP_STAGE_COLD_ZERO,  /*[-10,0]*/
        TEMP_STAGE_ZERO_COOL,  /*[0,10]*/
        TEMP_STAGE_COOL_NORMAL,/*[10,45]*/
        TEMP_STAGE_NORMAL_HOT, /*[-45,50]*/
        TEMP_STAGE_HOT,        /*[>50]*/
        TEMP_STAGE_OTHERS,
};

enum{
       FG_CHIP_TYPE_UNKNOWN = 0,
       FG_CHIP_TYPE_CW2015,
};
/*1=enable,0=disable*/
#define HIGHLOW_TEMP_LIMIT_CHARGING_ENABLE    (1)

#define VOLT_CAP_TABLE_NUM_MAX  (50)
/* Battery capacity estimation table */
struct batt_capacity_chart {
        int volt;
        unsigned int cap;
};

struct monitor_battery_platform_data {
        int *battery_tmp_tbl;
        unsigned int tblsize;
        unsigned long features;
        unsigned int monitoring_interval;
        unsigned int max_charger_currentmA;
        unsigned int max_charger_voltagemV;
        unsigned int dpm_voltagemV;
        unsigned int termination_currentmA;
        unsigned int usb_cin_limit_currentmA;
        unsigned int usb_battery_currentmA;
        unsigned int ac_cin_limit_currentmA;
        unsigned int ac_battery_currentmA;
        unsigned int rechg_voltagemV;
        int temp_low_threshold;
        int temp_zero_threshold;
        int temp_cool_threshold;
        int temp_warm_threshold;
        int temp_high_threshold;
};

struct pmic_adc_platform_data {
        int irq_line;
        unsigned long features;
};



/*extern charger recv usb or ac or remove event*/
extern int pmic_register_notifier(struct notifier_block *nb,
                                  unsigned int events);
extern int pmic_unregister_notifier(struct notifier_block *nb,
                                    unsigned int events);


/*extern special charger attr*/
#if defined(CONFIG_CHARGER_BQ24158)||defined(CONFIG_CHARGER_NCP1851)||defined(CONFIG_CHARGER_BQ24296)||defined(CONFIG_CHARGER_BQ2419X)

#define CONFIG_EXTERN_CHARGER

#endif

/*extern charger init platform data*/
struct extern_charger_platform_data {
        int max_charger_currentmA;  /*charger max charge current*/
        int max_charger_voltagemV;  /*battery limit charge voltage*/
        int dpm_voltagemV;          /*vbus min voltage*/
        int termination_currentmA;  /*charge done term current*/
        int usb_cin_limit_currentmA;/*vbus PC charger input current*/
        int usb_battery_currentmA;  /*battery usb charge current*/
        int ac_cin_limit_currentmA; /*vbus ac charger input current*/
        int ac_battery_currentmA;   /*battery ac charge current*/
        int enable_pin;
        int feature;
        int (*board_type)(void);
};

struct extern_charger_chg_config {
        const char* name;
        int (*set_charging_source) (int);
        int (*set_charging_enable) (bool);
        int (*set_battery_exist) (bool);
        int (*set_charging_temp_stage) (int,int,bool);//(temp_stage,volt,enable)
        int (*set_battery_capacity)(int);
        int (*get_charging_status) (void);
        int (*get_charger_health) (void);
        int (*get_chargerchip_detect)(void);
};
extern struct extern_charger_chg_config extern_charger_chg;

struct comip_android_battery_info {
        const char* name;
        int (*battery_voltage)(void);
        short (*battery_current)(void);
        int (*battery_capacity)(void);
        int (*battery_temperature)(void);
        int (*battery_exist)(void);
        int (*battery_health)(void);
        int (*battery_rm)(void);
        int (*battery_fcc)(void);
        int (*capacity_level)(void);
        int (*battery_technology)(void);
        int (*battery_full)(void);
        int (*firmware_update)(char*);
        int (*battery_ocv)(void);
        int (*battery_realsoc)(void);
        int (*chip_temp)(void);
        int (*get_fgchip_detect)(void);
        int (*check_init_complete)(void);
};
extern struct comip_android_battery_info *battery_property;
extern int comip_battery_property_register(struct comip_android_battery_info *di);

struct comip_fuelgauge_info {
        int *battery_tmp_tbl;
        unsigned int tmptblsize;
        int *fg_model_tbl;

        unsigned int fgtblsize;
        unsigned int normal_capacity;
        unsigned int firmware_version;
};

#if defined(CONFIG_BATTERY_BQ27510)
#define COULOMETER_DEV    "/sys/bus/i2c/drivers/bq27510-battery/state"
#else
#define COULOMETER_DEV    "NULL"
#endif

#if defined(CONFIG_BATTERY_BQ27510)|| defined(CONFIG_BATTERY_BQ27421) || defined(CONFIG_BATTERY_MAX17058) || defined(CONFIG_CW201X_BATTERY)
#define CONFIG_BATTERY_FG
extern int pmic_get_charging_status(struct comip_android_battery_info *bat);
extern int pmic_get_charging_type(struct comip_android_battery_info *bat);
#endif

#if defined(CONFIG_EXTERN_CHARGER)
extern int extern_charger_otg_start(bool enable);
#else
static inline int extern_charger_otg_start(bool enable)
{
        return 0;
}
#endif

#if defined(CONFIG_LC1132_ADC)
/*lc1132 adc chnl select*/
#define DBB_TEMP_ADC_CHNL   (3)
#define BAT_TEMP_ADC_CHNL   (4)
#else
#define BAT_TEMP_ADC_CHNL   (1)
#define DBB_TEMP_ADC_CHNL   (2)
#endif

#if defined(CONFIG_LC1100H_ADC) || defined(CONFIG_LC1132_ADC) || defined(CONFIG_LC1160_ADC)
extern int pmic_get_adc_conversion(int channel_no);
#else
static inline int pmic_get_adc_conversion(int channel_no)
{
        if(channel_no == 0)
                return 3800;
        else if ((channel_no == 1)||(channel_no == 4))
                return 800;
        else
                return 0;
}
#endif

#if defined(CONFIG_LC1132_ADC) || defined(CONFIG_LC1160_ADC)
extern int comip_get_thermal_temperature(int channel,int *val);
#else
static inline int comip_get_thermal_temperature(int channel,int *val)
{
        return -ENODEV;
}
#endif



/*bat id type defined*/
enum battery_id_type {
        BAT_ID_NONE,
        BAT_ID_1,
        BAT_ID_2,
        BAT_ID_3,
        BAT_ID_4,
        BAT_ID_5,
        BAT_ID_6,
        BAT_ID_7,
        BAT_ID_8,
        BAT_ID_9,
        BAT_ID_10,
        BAT_ID_MAX,
};

static inline char *comip_battery_name(enum battery_id_type name)
{
        static char *battery_name[] = {
                "Unknown", "XWD_2000_61", "GY_2000_62", "XWD_2000_63", "XWD_2000_64",
                            "DS_2000_65", "XX_2000_66",  "AAC_2000_67",  "GY_2200_68",
                            "DS_2200_69", "XWD_2200_6A"
        };

        if(name >= BAT_ID_MAX) {
                return battery_name[0];
        }

        return battery_name[name];
}

#if defined(CONFIG_BAT_ID_BQ2022A)
extern int comip_battery_id_type(void);
#else
static inline int comip_battery_id_type(void)
{
        return 0;
}
#endif




#endif
