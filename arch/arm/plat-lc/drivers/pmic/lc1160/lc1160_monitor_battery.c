/*
 *monitor_battery.c
 *
 * pmic charging driver
 *
 * Copyright (C) leadcore, Inc.
 * Author: leadcore, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/usb/ch9.h>
#include <linux/earlysuspend.h>
#include <linux/alarmtimer.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/io.h>
#include <plat/clock.h>
#include <plat/bootinfo.h>
#include <plat/comip-pmic.h>
#include <plat/comip-battery.h>
#include <plat/usb.h>
#include <plat/comip-thermal.h>
#include <plat/lc1160.h>
#include <plat/lc1160-pmic.h>
#include <plat/lc1160_monitor_battery.h>
#include <plat/lc1160_adc.h>
#include "lc1160_capacity_lookup.h"
#include <linux/gpio.h>
#include <plat/mfp.h>


struct pmic_charge_param {
        bool enable_charger;
        bool enable_iterm;
        bool timer_clear;
        bool timer_stop;
        int  full_chg_timeout;
        unsigned int  prev_set_current;
        unsigned int  cin_limit;
        unsigned int  currentmA;
        unsigned int  voltagemV;
        unsigned int  max_voltagemV;
        unsigned int  termination_currentmA;
        unsigned int rechg_voltagemV;
        int charge_status;
        int charger_health;
};

struct monitor_battery_device_info {
        struct device  *dev;
        struct monitor_battery_platform_data *platform_data;
        struct pmic_charge_param param;

        bool bat_present;
        int bat_health;
        int bat_voltage_mV;
        short bat_current_mA;
        int bat_temperature;
        int bat_tech;
        int bat_fcc;
        int bat_rm;
        int bat_chiptemp;
        int charger_source;
        int charge_status;
        int charger_health;
        u8 usb_online;
        u8 ac_online;
        u8 plug_online;
        u8 init_plugin;
        int rbuf_volt[15];
        int rbuf_fgvolt[15];
        int capacity_filter[5];
        int capacity_filter_count;
        int volt_filter_count;
        int fgvolt_filter_count;
        int charge_full_count;
        int realsoc;
        int adc_temp_chnl;
        int voffset;
        int prev_voltage;
        int prev_temp_stage;
        u16 monitoring_interval;
        bool hand_flag; /*hand control charging process*/
        struct notifier_block dbb;
        int dbb_temp;
        bool dbb_hot_flag;/*thermal temp control charging process*/
        int dbb_hot;
        int dbb_normal;
        bool resume_flag;
        unsigned int capacity;
        unsigned int capacity_debounce_count;
        unsigned int prev_capacity;
        int history_capacity;
        __kernel_time_t history_timeval;
        __kernel_time_t last_sec;
        struct power_supply bat;
        struct power_supply usb;
        struct power_supply ac;

        unsigned long event;
        struct notifier_block   nb;
        struct work_struct  usb_work;
        struct work_struct fg_update_work;
        int  fg_update;
        struct delayed_work battery_monitor_work;
        struct wake_lock usb_wakelock;
        struct wake_lock monitor_wakelock;
        struct alarm wakeup_alarm;
        struct extern_charger_chg_config *charger_info;
        struct comip_android_battery_info *battery_info;
#ifdef CONFIG_HAS_EARLYSUSPEND
        struct early_suspend early_suspend;
#endif
        unsigned int fgchip_type; 
        int cap_valid_count;
        int cap_adjust_flag;
};

struct monitor_battery_device_info *bci;
static DEFINE_MUTEX(battery_mutex);

#define VOLT_FILTER_LEN    (10)
#define CAP_FILTER_LEN     (5)

#define ENABLE_THERMAL_CHARGING  (0)
#define THERMAL_TEMP_HOT        (72)
#define THERMAL_TEMP_NORMAL     (65)

#define ADC_VOLT_CHANNEL    (0)
#define ADC_TEMP_CHANNEL    (1)

#define FAST_POLL       (20)//second
#define SLOW_POLL       (5*60*60)

#ifdef CONFIG_BATTERY_FG
int pmic_get_charging_status(struct comip_android_battery_info *bat)
{
        if(bci) {
                return bci->charge_status;
        } else
                return 0;
}
EXPORT_SYMBOL_GPL(pmic_get_charging_status);

int pmic_get_charging_type(struct comip_android_battery_info *bat)
{
        if(bci) {
                return bci->charger_source;
        } else
                return 0;
}
EXPORT_SYMBOL_GPL(pmic_get_charging_type);
#endif

static int pmic_write_byte(struct monitor_battery_device_info *di,
                           u8 reg, u8 value)
{
        int ret = 0;
        ret = pmic_reg_write(reg, value);
        if(ret)
                pr_err("%s: Error access to reg = (%x)\n", __func__, reg);
        return ret;
}

static int pmic_read_byte(struct monitor_battery_device_info *di,
                          u8 reg,u16 *value)
{
        int ret = 0;

        ret = pmic_reg_read(reg,value);
        if(ret)
                pr_err("%s: Error access to reg = (%x)\n", __func__, reg);
        return ret;
}

#ifndef CONFIG_BATTERY_FG
#ifdef CONFIG_BATINFO_FILE
static int lcapacity = 0;
static int lvoltage = 0;
static __kernel_time_t lsec = 0;
static int load_batinfo(void)
{
        long fd = sys_open(CONFIG_BATINFO_FILE,O_RDONLY,0);
        int lbatinfo[3] = {0};

        if(fd < 0) {
                pr_err("load_batinfo: open file %s failed\n", CONFIG_BATINFO_FILE);
                return fd;
        }
        sys_read(fd, (char __user *)lbatinfo, sizeof(lbatinfo));
        lcapacity = lbatinfo[0];
        lvoltage = lbatinfo[1];
        lsec = lbatinfo[2];

        sys_close(fd);
        return 0;
}

static void store_batinfo(int capacity, int voltage)
{
        struct timespec ts;
        int lbatinfo[3] = {0};
        long fd = sys_open(CONFIG_BATINFO_FILE,O_CREAT | O_RDWR, 0);

        if(fd < 0) {
                pr_err("store_batinfo: open file %s failed\n", CONFIG_BATINFO_FILE);
                return;
        }

        getnstimeofday(&ts);
        lbatinfo[0] = capacity;
        lbatinfo[1] = voltage;
        lbatinfo[2] = ts.tv_sec;
        sys_write(fd, (const char __user *)lbatinfo, sizeof(lbatinfo));

        sys_close(fd);
}
#endif
#endif

static int monitor_check_vbus_present(struct monitor_battery_device_info *di)
{
        u16 present_charge_state = 0;
        int ret = 0;

        ret = pmic_read_byte(di,POWER_STATUS_REG, &present_charge_state);
        if(ret) {
                dev_err(di->dev, "check vbus present err %x\n",POWER_STATUS_REG);
        }
        present_charge_state &=  VBUS_DET;
        return present_charge_state;
}

/*
 *pmic charger register configulation
*/

static void monitor_charger_control_reg_set(struct monitor_battery_device_info *di)
{
        u8 data = 0;

        data = (di->param.enable_iterm << EOC_SHIFT)| di->param.enable_charger;
        pmic_write_byte(di,CHARGER_CTRL_REG, data);
        return;
}

static void monitor_charger_voltage_reg_set(struct monitor_battery_device_info *di)
{
        u8 data = 0,shift = 4;
        u8 volt = 0,rechg = 0;
        int voltagemV = 0,rechgmV = 0;

        voltagemV = di->param.voltagemV;
        if(voltagemV < 4200)
                voltagemV = 4200;
        else if(voltagemV > 4425)
                voltagemV = 4425;

        if ((voltagemV >= 4200)&&(voltagemV <= 4275)) {
                volt = ((voltagemV - 4200)/25) << 1;
                shift = 4;
        } else if((voltagemV >= 4350)&&(voltagemV <= 4425)) {
                volt = (((voltagemV - 4350)/25) << 1)|1;
                shift = 4;
        } else {
                volt = 0;
                shift = 4;
        }

        rechgmV = di->param.rechg_voltagemV;

        if(rechgmV < RCHGSEL_MIN)
                rechgmV = RCHGSEL_MIN;
        if(rechgmV > RCHGSEL_MAX)
                rechgmV = RCHGSEL_MAX;

        rechg =(rechgmV - RCHGSEL_MIN)/RCHGSEL_STEP;

        data = (volt << shift)|rechg;

        pmic_write_byte(di,CHARGER_VOLT_REG, data);
        return;
}

static void monitor_charger_current_reg_set(struct monitor_battery_device_info *di)
{
        u8 data = 0;
        int cin_limit = 0,term_current = 0;
        u8 ibat = 0,iterm = 0;

        cin_limit = di->param.currentmA;
        term_current = di->param.termination_currentmA;
        if(cin_limit < CHARGE_CURRENT_MIN)
                cin_limit = CHARGE_CURRENT_MIN;

        if(cin_limit > CHARGE_CURRENT_MAX)
                cin_limit = CHARGE_CURRENT_MAX;

        if(term_current < TERM_CURRENT_MIN)
                term_current = TERM_CURRENT_MIN;

        if(term_current > TERM_CURRENT_MAX)
                term_current = TERM_CURRENT_MAX;
        if(cin_limit <= 400) {
                ibat = (cin_limit - CHARGE_CURRENT_MIN)/CHARGE_CURRENT_STEP;
        } else {
                ibat = (cin_limit - 250)/(CHARGE_CURRENT_STEP >> 1);
        }
        iterm = (term_current - TERM_CURRENT_MIN)/TERM_CURRENT_STEP;
        data = (iterm << ITERM_SHIFT) | ibat;
        pmic_write_byte(di,CHARGER_CURRENT_REG, data);
        return;
}

static void monitor_charger_timer_reg_set(struct monitor_battery_device_info *di)
{
        u8 data = 0;
        u8 timenum = 0;

        if(di->param.full_chg_timeout < CHARGE_TIMER_MIN)
                di->param.full_chg_timeout = CHARGE_TIMER_MIN;
        if(di->param.full_chg_timeout > CHARGE_TIMER_MAX)
                di->param.full_chg_timeout = CHARGE_TIMER_MAX;

        timenum = (di->param.full_chg_timeout  -CHARGE_TIMER_MIN);

        data = (timenum << CHARGE_TIMER_SHIFT) |
               (di->param.timer_clear << 3) | (di->param.timer_stop);

        pmic_write_byte(di,CHARGER_TIMER_REG, data);
        return;
}

#ifndef CONFIG_EXTERN_CHARGER
static int monitor_charger_params_update(struct monitor_battery_device_info *di)
{
        switch(di->charger_source) {
        case POWER_SUPPLY_TYPE_USB:
                di->param.currentmA= di->platform_data->usb_battery_currentmA;
                break;
        case POWER_SUPPLY_TYPE_MAINS:
                di->param.currentmA= di->platform_data->ac_battery_currentmA ;
                break;
        default:
                di->param.currentmA= di->platform_data->usb_battery_currentmA;
                break;
        }

        di->param.voltagemV = di->param.max_voltagemV;
        di->param.termination_currentmA = di->param.termination_currentmA ;
        di->param.enable_charger = ACHGON_EN;
        di->param.enable_iterm = EOC_EN;
        di->param.timer_clear = RTIMCLRB_RUN;
        di->param.timer_stop = RTIMSTP_STOP;
        di->param.full_chg_timeout = 8;
        monitor_charger_control_reg_set(di);
        monitor_charger_voltage_reg_set(di);
        monitor_charger_current_reg_set(di);
        monitor_charger_timer_reg_set(di);
        return 0;
}
#endif

/*amt adjust code begin*/
static int g_vbatt[4] = {4200,4200,3600,3600};
static int monitor_adc_channel_voltage(int channel)
{
        int temp;
        int temp_k = 1000,temp_b = 0;
        int vbatt = 0,i = 0, volt[8]= {0};
        u32 sum = 0;

        /* unit is mV */
        for (i = 0; i < 8; i++) {
                volt[i] = pmic_get_adc_conversion(ADC_VOLT_CHANNEL);
                sum =sum + volt[i];
        }
        temp = sum/8;
        temp_k = 1000 * (g_vbatt[0] - g_vbatt[2]) / (g_vbatt[1] - g_vbatt[3]);
        temp_b = 1000 * g_vbatt[0] - temp_k * g_vbatt[1];
        vbatt = (temp_k * temp + temp_b) / 1000;

        return vbatt;
}
/*amt adjust code end*/

#ifndef CONFIG_BATTERY_FG
static int monitor_battery_capacity_filter(struct monitor_battery_device_info *di,int curr_capacity)
{
        int index = 0,i=0;
        int capacity_sum = 0;
        int soc = 0;

        index = di->capacity_filter_count%CAP_FILTER_LEN;
        di->capacity_filter[index] = curr_capacity;

        for(i=0; i < CAP_FILTER_LEN; i++) {
                capacity_sum = capacity_sum + di->capacity_filter[i];
        }

        if (++di->capacity_filter_count >= CAP_FILTER_LEN) {
                di->capacity_filter_count = 0;
        }
        if(curr_capacity == 0) {
                soc = 0;
        } else {
                if(di->bat_present) {
                        soc = capacity_sum/CAP_FILTER_LEN;
                } else {
                        soc = di->capacity_filter[index];
                }
        }

        return soc;
}

static int monitor_get_battery_exist(void)
{
#if defined(LC1160_BATTERY_EXIST_DET)
        struct monitor_battery_device_info *di = bci;
        int adc_code = 0,i = 0, adc[2]= {0};
        int sum = 0;

        for(i=0; i< 2; i++) {
                adc[i] = pmic_get_adc_conversion(di->adc_temp_chnl);
                sum = sum+adc[i];
        }
        adc_code = sum/2;
        if(adc_code > 2500)
                return 0;
#endif
        return (1);
}

static short monitor_get_battery_current(void)
{
        return (0);
}

/*
 *rbuf_volt[5] init
*/

static int monitor_get_battery_voltage_init(struct monitor_battery_device_info *di)
{
        int i = 0;
        u32 sum = 0;

        for(i=0; i<VOLT_FILTER_LEN; i++) {
                di->rbuf_volt[i] = pmic_get_adc_conversion(ADC_VOLT_CHANNEL);
                sum = sum + di->rbuf_volt[i];
        }

        return (sum/VOLT_FILTER_LEN);
}

/*
 *get battery voltage from adc conversion
*/
static int monitor_get_battery_voltage(void)
{
        struct monitor_battery_device_info *di = bci;
        int i = 0,index = 0;
        u32 sum = 0;

        index = di->volt_filter_count%VOLT_FILTER_LEN;

        di->rbuf_volt[index] = monitor_adc_channel_voltage(ADC_VOLT_CHANNEL);
        for(i=0; i < VOLT_FILTER_LEN; i++) {
                sum = sum + di->rbuf_volt[i];
        }

        if (++di->volt_filter_count >= VOLT_FILTER_LEN) {
                di->volt_filter_count = 0;
        }
        if(di->rbuf_volt[index] > 3400) {
                if(di->bat_present) {
                        return(sum/VOLT_FILTER_LEN);
                } else {
                        return di->rbuf_volt[index];
                }
        } else {
                return di->rbuf_volt[index];
        }
}


/*
 *get battery temperature from adc conversion
*/

static int monitor_get_battery_temperature(void)
{
        struct monitor_battery_device_info *di = bci;
        int temp_C = 20;
        int adc_code = 0,i = 0, adc[2]= {0};
        int sum = 0;

        for(i=0; i< 2; i++) {
                adc[i] = pmic_get_adc_conversion(di->adc_temp_chnl);
                sum = sum+adc[i];
        }
        adc_code = sum/2;
        if(adc_code > 2500)
                return temp_C;

        for (temp_C = 0; temp_C < di->platform_data->tblsize; temp_C++) {
                if (adc_code >= di->platform_data->battery_tmp_tbl[temp_C])
                        break;
        }

        /* first 20 values are for negative temperature */
        temp_C = (temp_C - 20); /* degree Celsius */
        if(temp_C > 67) {
                pr_info("monitor_battery: battery temp over 68 degree\n");
        }

        return temp_C;
}


static int monitor_get_battery_health(void)
{
        struct monitor_battery_device_info *di = bci;
        u16 data = 0;
        int bat_temp = 0,ret = 0;
        int health = di->bat_health;
        int status = POWER_SUPPLY_HEALTH_GOOD;

        ret = pmic_read_byte(di,POWER_STATUS_REG, &data);
        if (ret) {
                return POWER_SUPPLY_HEALTH_GOOD;
        }

        bat_temp = di->bat_temperature;
        if(di->charger_source != POWER_SUPPLY_TYPE_BATTERY) {
                if(bat_temp > di->platform_data->temp_high_threshold) {
                        if (health != POWER_SUPPLY_HEALTH_OVERHEAT) {
                                pr_info("monitor_battery:battery overheat (%d>%d)",
                                        bat_temp,di->platform_data->temp_high_threshold);
                                status = POWER_SUPPLY_HEALTH_OVERHEAT;
                        }
                } else if((bat_temp <= di->platform_data->temp_high_threshold)&&
                          (bat_temp >= di->platform_data->temp_zero_threshold)) {
                        if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
                            health == POWER_SUPPLY_HEALTH_COLD|| health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
                                if (data & VBATV45) {
                                        status = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
                                } else {
                                        status = POWER_SUPPLY_HEALTH_GOOD;
                                }
                        }
                } else if(bat_temp < (di->platform_data->temp_zero_threshold-TEMP_OFFSET)) {
                        if (health != POWER_SUPPLY_HEALTH_COLD) {
                                pr_info("monitor_battery:battery overcold (%d<%d)",
                                        bat_temp,di->platform_data->temp_low_threshold);
                                status = POWER_SUPPLY_HEALTH_COLD;
                        }
                } else {
                        status = health;
                }
        } else {
                status = POWER_SUPPLY_HEALTH_GOOD;
        }

        return status;
}


static int capacity_lookup(struct monitor_battery_device_info *di,int volt)
{
        int table_size;
        int i,cap = 0,cap1 = 0,cap2 = 0,volt1 = 0,volt2 = 0;
        struct batt_capacity_chart *table;

        if(di->param.max_voltagemV >= 4350) {
                table_size = ARRAY_SIZE(dischg_volt_cap_table_4350_normal);
                table = dischg_volt_cap_table_4350_normal;
        } else {
                table_size = ARRAY_SIZE(dischg_volt_cap_table_4200_normal);
                table = dischg_volt_cap_table_4200_normal;
        }

        if(volt < table[0].volt) {
                cap = 0;
        } else if(volt > table[table_size - 1].volt) {
                cap = table[table_size - 1].cap;
        } else {
                for (i = 1; i < table_size; i++) {
                        if (volt <= table[i].volt)
                                break;
                }
                volt1 = table[i-1].volt;
                volt2 = table[i].volt;
                cap1 = table[i-1].cap;
                cap2 = table[i].cap;
                /* linear interpolation */
                cap = (cap2 - cap1) * (volt - volt2) / (volt2 - volt1) + cap2;
                if(cap < 0) {
                        pr_err("%s: capacity_lookup = %d,volt = %d\n", __func__,cap,volt);
                        cap = table[i-1].cap;
                }
        }

        return cap;
}

static int monitor_get_battery_capacity(void)
{
        struct monitor_battery_device_info *di = bci;
        int volt = di->bat_voltage_mV + di->voffset;
        int i = 0,curr_capacity = di->capacity;
        struct timeval  _now;
        long time_pulse = 0;

        do_gettimeofday(&_now);
        curr_capacity = capacity_lookup(di,volt);

        if(di->capacity == -1) {
                for(i = 0; i < CAP_FILTER_LEN; i++) {
                        di->capacity_filter[i] = curr_capacity;
                }
                di->history_capacity = curr_capacity;
                di->history_timeval = _now.tv_sec;
                return curr_capacity;
        }

        curr_capacity = monitor_battery_capacity_filter(di,curr_capacity);

        time_pulse =abs(_now.tv_sec - di->history_timeval);

        if(!di->bat_present) {
                goto out0;
        }
        /*if battery voltage is lower than 3.4V, return soc = 0*/
        if(di->bat_voltage_mV <= 3400) {
                curr_capacity = 0;
                goto out0;
        }
        switch(di->charge_status) {
        case POWER_SUPPLY_STATUS_FULL:
                if(time_pulse < 40)
                        goto out2;
                curr_capacity = 100;
                break;
        case POWER_SUPPLY_STATUS_CHARGING:
                if(time_pulse < 40)
                        goto out2;
                if((curr_capacity - di->history_capacity) > 1) {
                        curr_capacity = di->history_capacity + 1;
                } else if(di->history_capacity > curr_capacity) {
                        if(di->history_capacity >= (curr_capacity + 15)) {
                                curr_capacity = di->history_capacity - 2;
                                goto out0;
                        } else {
                                if((di->bat_voltage_mV + 30)>= di->prev_voltage) {
                                        goto out1;
                                } else {
                                        curr_capacity = di->history_capacity - 1;
                                }
                        }
                }
                break;
        case POWER_SUPPLY_STATUS_NOT_CHARGING:
        case POWER_SUPPLY_STATUS_DISCHARGING:
                if(time_pulse < 60)
                        goto out2;
                if((di->history_capacity - curr_capacity) > 1) {
                        if(di->history_capacity > curr_capacity + 10) {
                                if(time_pulse >= 1000) {
                                        curr_capacity = curr_capacity;
                                } else {
                                        curr_capacity = di->history_capacity - 1;
                                }
                        } else {
                                curr_capacity = di->history_capacity - 1;
                        }
                } else if(curr_capacity > di->history_capacity) {
                        goto out1;
                }
                break;
        default:
                pr_err("capacity not update while charge status err = %d\n",di->charge_status);
                goto out2;
        }


out0:
        di->history_capacity = ((curr_capacity > 0) ? curr_capacity : 0);

out1:
        di->history_timeval = _now.tv_sec;
out2:
        return di->history_capacity;
}

static int monitor_battery_realsoc(void)
{
    struct monitor_battery_device_info *di = bci;

    return di->capacity;
}

static int monitor_get_battery_technology(void)
{
        return POWER_SUPPLY_TECHNOLOGY_LIPO;
}

static struct comip_android_battery_info monitor_battery_property= {
        .battery_voltage = monitor_get_battery_voltage,
        .battery_current = monitor_get_battery_current,
        .battery_capacity = monitor_get_battery_capacity,
        .battery_temperature = monitor_get_battery_temperature,
        .battery_exist = monitor_get_battery_exist,
        .battery_health= monitor_get_battery_health,
        .battery_technology = monitor_get_battery_technology,
        .battery_realsoc = monitor_battery_realsoc,
};
#endif

static void monitor_charger_interrupt(int event, void *puser, void* pargs)
{
        //unsigned int int_status = *((unsigned int *)pargs);
        struct monitor_battery_device_info *di =
                (struct monitor_battery_device_info *)puser;
        u16 present_charge_state = 0,pmic_charge_sts;
        int ret = 0;

        if (comip_otg_hostmode()) {
                dev_info(di->dev, "otg intr\n");
                return;
        }

        ret = pmic_read_byte(di,POWER_STATUS_REG, &present_charge_state);
        if(ret) {
                goto err;
        }

        if( (present_charge_state & VBUS_DET) == 0) {
                if((present_charge_state & VBATV33) == 0) {
                        pr_info("%s: disabling BATUVIR interrupt\n", __func__);
                        //lc1160_int_mask(LC1160_INT_BATUVIR);
                        if(di->battery_info->battery_voltage) {
                                di->bat_voltage_mV = di->battery_info->battery_voltage();
                        }
                        if(di->bat_voltage_mV <= 3400) {
                                dev_info(di->dev, "low battery intr = %d\n",di->bat_voltage_mV);
                                di->capacity = 0;
                        }
                }
                else
               {
                    pr_info("%s: adapter/usb unplugged\n", __func__);
                    goto err;
               }
        }

        ret = pmic_read_byte(di,CHARGER_STATUS_REG, &pmic_charge_sts);
        if(ret) {
                goto err;
        }

#ifndef CONFIG_EXTERN_CHARGER
        if(pmic_charge_sts & CHARGE_TIMEOUT) {
                dev_info(di->dev, "charge over time\n");
                di->param.enable_charger = ACHGON_DIS;
                monitor_charger_control_reg_set(di);
                di->param.enable_charger = ACHGON_EN;
                monitor_charger_control_reg_set(di);
        }

        if(pmic_charge_sts & ADPOV_STATUS) {
                dev_info(di->dev, "CHARGER OVP \n");
                if(di->charger_health != POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
                        di->charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
                        power_supply_changed(&di->usb);
                        power_supply_changed(&di->ac);
                }
                di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        } else {
                if(di->charger_health != POWER_SUPPLY_HEALTH_GOOD) {
                        di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
                        power_supply_changed(&di->usb);
                        power_supply_changed(&di->ac);
                }
                di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
        }

        if(pmic_charge_sts & BATOT_STATUS) {
                dev_dbg(di->dev, "battery temperature fault\n");
        }

        if(pmic_charge_sts & RECHG_STATUS) {
                dev_dbg(di->dev, "recharge\n");
                di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
        }
        if(pmic_charge_sts & ADPUV_STATUS) {
                dev_err(di->dev, "charger under voltage  =%x\n",pmic_charge_sts);
        }
        if(!(pmic_charge_sts & CHGFAULT_STATUS)) {
                dev_info(di->dev, "charge fault=%x\n",pmic_charge_sts);
        }

        if(pmic_charge_sts & CHARGE_DONE) {
                dev_info(di->dev, "charge done\n");
                //di->charge_status = POWER_SUPPLY_STATUS_FULL;
        }
#endif

        if (di->capacity != -1) {
                power_supply_changed(&di->bat);
        } else {
                cancel_delayed_work(&di->battery_monitor_work);
                schedule_delayed_work(&di->battery_monitor_work, 0);
        }
err:
        return;
}

#ifndef CONFIG_EXTERN_CHARGER
#if defined(CONFIG_OTG_GPIO_CTL)
#define OTG_GPIO_EN MFP_PIN_GPIO(161)
#endif
static int monitor_set_charging_enable(bool enable)
{
        struct monitor_battery_device_info *di = bci;

        if(!di->bat_present) {
                return 0;
        }

        di->param.enable_charger = (enable & 0x1);
        di->param.enable_charger = di->param.enable_charger & di->hand_flag & di->dbb_hot_flag;
        monitor_charger_control_reg_set(di);

        return 0;
}

static int monitor_set_charging_temp_stage(int temp_status,int volt,bool enable)
{
        struct monitor_battery_device_info *di = bci;
        u16 pmic_charge_sts = 0;

        if(!di) {
                pr_err("%s: monitor_battery_device_info not init\n", __func__);
                return -EINVAL;
        }

        if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY))
                return 0;

        if(enable) {
                if(di->charger_source == POWER_SUPPLY_TYPE_USB) {
                        switch(temp_status) {
                        case TEMP_STAGE_OVERCOLD:
                        case TEMP_STAGE_COLD_ZERO:
                                di->param.enable_charger = 0;
                                pr_info("monitor_battery:battery cold and disable charging =%d\n",di->bat_temperature);
                                break;
                        case TEMP_STAGE_ZERO_COOL:
                        case TEMP_STAGE_COOL_NORMAL:
                        case TEMP_STAGE_NORMAL_HOT:
                                di->param.enable_charger = 1;
                                break;
                        case TEMP_STAGE_HOT:
                                di->param.enable_charger = 0;
                                pr_info("monitor_battery:battery overheat and disable charging =%d\n",di->bat_temperature);
                                break;
                        default:
                                break;
                        }
                } else if(di->charger_source == POWER_SUPPLY_TYPE_MAINS) {
                        switch(temp_status) {
                        case TEMP_STAGE_OVERCOLD:
                        case TEMP_STAGE_COLD_ZERO:
                                di->param.enable_charger = 0;
                                di->param.currentmA = di->platform_data->ac_battery_currentmA;
                                pr_info("monitor_battery:battery cold and disable charging =%d\n",di->bat_temperature);
                                break;
                        case TEMP_STAGE_ZERO_COOL:
                        case TEMP_STAGE_COOL_NORMAL:
                                di->param.enable_charger = 1;
                                di->param.currentmA = di->platform_data->ac_battery_currentmA;
                                break;
                        case TEMP_STAGE_NORMAL_HOT:
                                di->param.enable_charger = 1;
                                di->param.currentmA = di->platform_data->usb_battery_currentmA;
                                break;
                        case TEMP_STAGE_HOT:
                                di->param.enable_charger = 0;
                                di->param.currentmA = di->platform_data->usb_battery_currentmA;
                                pr_info("monitor_battery:battery cold and disable charging =%d\n",di->bat_temperature);
                                break;
                        default:
                                break;
                        }
                }
        } else {
                di->param.enable_charger = 1;
        }

        di->param.enable_charger = di->param.enable_charger & di->hand_flag & di->dbb_hot_flag;
        monitor_charger_control_reg_set(di);
        monitor_charger_current_reg_set(di);

        pmic_read_byte(di,CHARGER_STATUS_REG, &pmic_charge_sts);
        if((di->param.enable_charger)&&
            ((pmic_charge_sts & ADPOV_STATUS)!= ADPOV_STATUS)) {
                if(di->capacity < 100) {
                        if(di->bat_present) {
                                di->param.charge_status = POWER_SUPPLY_STATUS_CHARGING;
                        } else {
                                di->param.charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
                        }
                }
        } else {
                di->param.charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }

        if(pmic_charge_sts & CHGFAULT_STATUS) {
                if(pmic_charge_sts & CHARGE_DONE) {
                        if(di->capacity >= 100)
                                di->charge_status = POWER_SUPPLY_STATUS_FULL;
                        pr_info("monitor_battery:charge done\n");
                }
                if(di->param.charger_health != POWER_SUPPLY_HEALTH_GOOD) {
                        di->param.charger_health = POWER_SUPPLY_HEALTH_GOOD;
                        power_supply_changed(&di->usb);
                        power_supply_changed(&di->ac);
                }
        } else {
                if(pmic_charge_sts & ADPOV_STATUS) {
                        if(di->param.charger_health != POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
                                di->param.charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
                                power_supply_changed(&di->usb);
                                power_supply_changed(&di->ac);
                        }
                }
        }
        return 0;
}

static int monitor_get_charging_status(void)
{
        struct monitor_battery_device_info *di = bci;

        if(!di) {
                pr_err("%s: monitor_battery_device_info not init\n", __func__);
                return POWER_SUPPLY_STATUS_UNKNOWN;
        }

        return di->param.charge_status;
}

static int monitor_get_charger_health(void)
{
        struct monitor_battery_device_info *di = bci;

        if(!di) {
                pr_err("%s: monitor_battery_device_info not init\n", __func__);
                return POWER_SUPPLY_HEALTH_UNKNOWN;
        }

        return di->param.charger_health;
}

static void monitor_start_usb_charger(struct monitor_battery_device_info *di)
{
        u16 pmic_charge_sts = 0;

        dev_info(di->dev, "monitor_start_usb_charger\n");

        di->charger_source = POWER_SUPPLY_TYPE_USB;
        di->hand_flag = 1;

        pmic_read_byte(di,CHARGER_STATUS_REG, &pmic_charge_sts);
        if(pmic_charge_sts & ADPOV_STATUS) {
                di->param.charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
                di->param.charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        } else {
                di->param.charge_status = POWER_SUPPLY_STATUS_CHARGING;
                di->param.charger_health = POWER_SUPPLY_HEALTH_GOOD;
        }
        monitor_charger_params_update(di);

        return;
}

static void monitor_start_ac_charger(struct monitor_battery_device_info *di)
{
        u16 pmic_charge_sts = 0;

        dev_info(di->dev, "monitor_start_ac_charger\n");

        di->charger_source = POWER_SUPPLY_TYPE_MAINS;
        di->hand_flag = 1;

        pmic_read_byte(di,CHARGER_STATUS_REG, &pmic_charge_sts);
        if(pmic_charge_sts & ADPOV_STATUS) {
                di->param.charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
                di->param.charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        } else {
                di->param.charge_status = POWER_SUPPLY_STATUS_CHARGING;
                di->param.charger_health = POWER_SUPPLY_HEALTH_GOOD;
        }

        monitor_charger_params_update(di);

        return;
}

static void monitor_stop_charger(struct monitor_battery_device_info *di)
{
        dev_info(di->dev, "monitor_stop_charger\n");
        di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
        di->param.charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
        di->param.charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
        di->hand_flag = 1;

#if defined(CONFIG_OTG_GPIO_CTL)
        //stop charging
        di->param.enable_iterm = EOC_DIS;
        di->param.enable_charger = ACHGON_DIS;
        monitor_charger_control_reg_set(di);
        
        gpio_set_value(OTG_GPIO_EN,0);

        gpio_free(OTG_GPIO_EN);
#endif
        return;
}

#if defined(CONFIG_OTG_GPIO_CTL)
static void monitor_start_otg(struct monitor_battery_device_info *di,bool enable)
{
    if(enable){
       di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
       di->param.charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
       di->param.charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
       di->hand_flag = 1;
        dev_info(di->dev, "monitor_start_otg\n");
        gpio_direction_output(OTG_GPIO_EN, 1);
        gpio_set_value(OTG_GPIO_EN,1);

        gpio_free(OTG_GPIO_EN);
    }
    else{
        gpio_direction_output(OTG_GPIO_EN, 1);
        gpio_set_value(OTG_GPIO_EN,0);

        gpio_free(OTG_GPIO_EN);
    }

    return;
}
#endif
static int monitor_set_charging_source(int charging_source)
{
        struct monitor_battery_device_info *di = bci;

        if(!di) {
                pr_err("%s: monitor_battery_device_info not init\n", __func__);
                return  -EINVAL;;
        }

        switch(charging_source) {
        case LCUSB_EVENT_PLUGIN:
                break;
        case LCUSB_EVENT_VBUS:
                monitor_start_usb_charger(di);
                break;
        case LCUSB_EVENT_CHARGER:
                monitor_start_ac_charger(di);
                break;
        case LCUSB_EVENT_NONE:
                monitor_stop_charger(di);
                break;
        case LCUSB_EVENT_OTG:
#if defined(CONFIG_OTG_GPIO_CTL)
                monitor_start_otg(di,1);
#endif
                break;
        default:
                break;
        }
        return 0;
}

static int monitor_set_battery_exist(bool battery_exist)
{
        struct monitor_battery_device_info *di = bci;

        if(!di) {
                pr_err("%s: monitor_battery_device_info not init\n", __func__);

                return -EINVAL;
        }
        di->bat_present = battery_exist;
        if(!battery_exist) {
                dev_err(di->dev, "battery is not present! \n");
        }
        return(0);
}

static int monitor_get_charger_dectect(void)
{
    return 1;
}

struct extern_charger_chg_config extern_charger_chg = {
        .name = "monitor_charger",
        .set_charging_source = monitor_set_charging_source,
        .set_charging_enable = monitor_set_charging_enable,
        .set_battery_exist = monitor_set_battery_exist,
        .set_charging_temp_stage = monitor_set_charging_temp_stage,
        .get_charging_status = monitor_get_charging_status,
        .get_charger_health = monitor_get_charger_health,
        .get_chargerchip_detect = monitor_get_charger_dectect,
};
#endif

static int monitor_battery_temp_check(struct monitor_battery_device_info *di)
{
        if (di->bat_temperature < di->platform_data->temp_low_threshold)
                return TEMP_STAGE_OVERCOLD;
        else if ((di->bat_temperature >= di->platform_data->temp_low_threshold)&&
                 (di->bat_temperature < di->platform_data->temp_zero_threshold))
                return TEMP_STAGE_COLD_ZERO;
        else if ((di->bat_temperature >= di->platform_data->temp_zero_threshold)&&
                 (di->bat_temperature < (di->platform_data->temp_cool_threshold - TEMP_OFFSET)))
                return TEMP_STAGE_ZERO_COOL;
        else if ((di->bat_temperature >= di->platform_data->temp_cool_threshold)&&
                 (di->bat_temperature < (di->platform_data->temp_warm_threshold - TEMP_OFFSET)))
                return TEMP_STAGE_COOL_NORMAL;
        else if ((di->bat_temperature >= di->platform_data->temp_warm_threshold)&&
                 (di->bat_temperature < (di->platform_data->temp_high_threshold - TEMP_OFFSET)))
                return TEMP_STAGE_NORMAL_HOT;
        else if (di->bat_temperature > di->platform_data->temp_high_threshold)
                return TEMP_STAGE_HOT;
        else
                return TEMP_STAGE_OTHERS;
}

static void monitor_battery_temp_charging(struct monitor_battery_device_info *di)
{
        int temp_status = 0;

        if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY))
                return;

        temp_status = monitor_battery_temp_check(di);
        if(!di->dbb_hot_flag) {
                temp_status = TEMP_STAGE_HOT;
        }
        if(di->battery_info->battery_realsoc) {
                if(di->charger_info->set_battery_capacity)
                    di->charger_info->set_battery_capacity(di->realsoc);
        }
        if(di->charger_info->set_battery_exist)
                di->charger_info->set_battery_exist(di->bat_present);
        if(di->charger_info->set_charging_temp_stage)
                di->charger_info->set_charging_temp_stage(temp_status,di->bat_voltage_mV,1);/*change to 1 enable bat temp limit charging*/
        if(di->charger_info->get_charging_status)
                di->charge_status = di->charger_info->get_charging_status();
        if(di->charger_info->get_charger_health)
                di->charger_health = di->charger_info->get_charger_health();

        di->prev_temp_stage = temp_status;
        return;
}

static void monitor_dbb_thermal_charging(struct monitor_battery_device_info *di)
{

#if ENABLE_THERMAL_CHARGING
        if(di->dbb_temp >= di->dbb_hot) {
                if(di->dbb_hot_flag == 1) {
                        di->dbb_hot_flag = 0;
                        monitor_battery_temp_charging(di);
                }
                di->dbb_hot_flag = 0;
                pr_err("thermal:dbb_temp = %d, dbb_dis_charger = %d\n",
                       di->dbb_temp,di->dbb_hot_flag);
        } else if(di->dbb_temp <= di->dbb_normal) {
                if(di->dbb_hot_flag == 0) {
                        di->dbb_hot_flag = 1;
                        monitor_battery_temp_charging(di);
                }
                di->dbb_hot_flag = 1;
                pr_info("thermal:dbb_temp = %d, dbb_en_charger = %d\n",
                        di->dbb_temp,di->dbb_hot_flag);
        } else {
        }
#endif
        return;
}

static int monitor_dbb_temp_notifier(struct notifier_block *dbb,
                                     unsigned long event, void *data)
{
        struct monitor_battery_device_info *di =
                container_of(dbb, struct monitor_battery_device_info, dbb);
        int prev_temp = 0;

        di->dbb_temp = ((struct thermal_data*)data)->temperature;
        if(di->charger_source > POWER_SUPPLY_TYPE_BATTERY) {
                prev_temp = di->dbb_temp;
                if(prev_temp != di->dbb_temp) {
                        monitor_dbb_thermal_charging(di); /*CPU DBB limit charging*/
                }
        } else {
                di->dbb_hot_flag = 1;
        }
        return NOTIFY_OK;
}

static void monitor_start_usb_source(struct monitor_battery_device_info *di)
{
        wake_lock(&di->usb_wakelock);
        di->charger_source = POWER_SUPPLY_TYPE_USB;
        di->plug_online = 0;
        di->usb_online = 1;
        di->ac_online = 0;
        di->dbb_hot_flag = 1;
        di->dbb_temp = 0;
        power_supply_changed(&di->usb);
        di->charge_full_count = 0;
        if(di->battery_info->battery_exist)
                di->bat_present = di->battery_info->battery_exist();
        if(di->charger_info->set_battery_exist)
                di->charger_info->set_battery_exist(1);

        if(di->charger_info->set_charging_source)
                di->charger_info->set_charging_source(LCUSB_EVENT_VBUS);
        if(di->charger_info->get_charging_status)
                di->charge_status = di->charger_info->get_charging_status();
        if(di->charger_info->get_charger_health)
                di->charger_health = di->charger_info->get_charger_health();
        if(!di->bat_present) {
                di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }

        return;
}

static void monitor_start_ac_source(struct monitor_battery_device_info *di)
{
        //wake_lock(&di->usb_wakelock);
        di->charger_source = POWER_SUPPLY_TYPE_MAINS;

        di->plug_online = 0;
        di->usb_online = 0;
        di->ac_online = 1;
        di->dbb_hot_flag = 1;
        di->dbb_temp = 0;
        di->charge_full_count = 0;
        power_supply_changed(&di->ac);
        if(di->battery_info->battery_exist)
                di->bat_present = di->battery_info->battery_exist();
        if(di->charger_info->set_battery_exist)
                di->charger_info->set_battery_exist(1);

        if(di->charger_info->set_charging_source)
                di->charger_info->set_charging_source(LCUSB_EVENT_CHARGER);
        if(di->charger_info->get_charging_status)
                di->charge_status = di->charger_info->get_charging_status();
        if(di->charger_info->get_charger_health)
                di->charger_health = di->charger_info->get_charger_health();
        if(!di->bat_present) {
                di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }

        return;
}

static void monitor_stop_source(struct monitor_battery_device_info *di)
{
        di->charger_source = POWER_SUPPLY_TYPE_BATTERY;

        if(di->charger_info->set_charging_source)
                di->charger_info->set_charging_source(LCUSB_EVENT_NONE);
        //lc1160_int_unmask(LC1160_INT_BATUVIR);
        di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
        di->charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
        di->plug_online = 0;
        di->usb_online = 0;
        di->ac_online = 0;
        di->prev_temp_stage = -1;
        di->dbb_hot_flag = 1;
        di->charge_full_count = 0;
        power_supply_changed(&di->ac);
        power_supply_changed(&di->usb);
        wake_unlock(&di->usb_wakelock);
        return;
}
static void monitor_start_otg_source(struct monitor_battery_device_info *di)
{
        if(di->charger_info->set_charging_source)
                di->charger_info->set_charging_source(LCUSB_EVENT_OTG);
        di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
        di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
        di->plug_online = 0;
        di->usb_online = 0;
        di->ac_online = 0;
        di->prev_temp_stage = -1;
        di->dbb_hot_flag = 1;
        //wake_unlock(&di->usb_wakelock);
        return;
}

static void monitor_battery_update_data(struct monitor_battery_device_info *di)
{
        if(di->battery_info->battery_exist) {
                di->bat_present = di->battery_info->battery_exist();
        }

#ifdef CONFIG_BATTERY_FG
        if(di->battery_info->battery_voltage) {
                di->bat_voltage_mV = di->battery_info->battery_voltage();
        }
        if(di->battery_info->battery_fcc) {
                di->bat_fcc = di->battery_info->battery_fcc();
        }
        if(di->battery_info->battery_rm) {
                di->bat_rm = di->battery_info->battery_rm();
        }
        if(di->battery_info->chip_temp) {
                di->bat_chiptemp = di->battery_info->chip_temp();
        }
#endif

        if(di->battery_info->battery_current) {
                di->bat_current_mA = (-1) * di->battery_info->battery_current();
        }

        if (di->bat_present) {
                if(di->battery_info->battery_health) {
                        di->bat_health = di->battery_info->battery_health();
                }
                if(di->battery_info->battery_technology) {
                        di->bat_tech = di->battery_info->battery_technology();
                }
                if(di->battery_info->battery_temperature) {
                        di->bat_temperature = di->battery_info->battery_temperature();
                }
        } else {
                di->bat_health = POWER_SUPPLY_HEALTH_UNKNOWN;
                di->bat_tech   = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
                di->bat_temperature  = 20;
        }
        return;
}

#ifndef CONFIG_BATTERY_FG


static int capacity_monitor_thread(struct monitor_battery_device_info *di)
{
        int curr_capacity = di->capacity;
        int charging_disabled = 0;
        struct timespec ts;
        struct rtc_time tm;

        mutex_lock(&battery_mutex);
        getnstimeofday(&ts);
        rtc_time_to_tm(ts.tv_sec, &tm);


        if(di->charger_source > POWER_SUPPLY_TYPE_BATTERY) {
                /* We have to disable charging to read correct voltage*/
                if(di->prev_capacity <= 98) {
                        charging_disabled = 1;
                        if(di->charger_info->set_charging_enable) {
                                di->charger_info->set_charging_enable(0);
                        }
                        /*voltage setteling time*/
                        msleep(150);
                }
                if(di->battery_info->battery_voltage) {
                        di->bat_voltage_mV = di->battery_info->battery_voltage();
                }
                /* if we disabled charging to check capacity,
                * enable it again after we read the correct voltage */
                if ((charging_disabled)
                    &&(di->charge_status != POWER_SUPPLY_STATUS_NOT_CHARGING)) {
                        if(di->charger_info->set_charging_enable) {
                                di->charger_info->set_charging_enable(1);
                        }
                }
                /* if battery temp is over high or
                   over low we assume it is not charging%*/
                monitor_battery_temp_charging(di);
                di->voffset = 0;
        } else {
                if(di->capacity ==-1) {
                        di->voffset = 60;
                } else {
                        if(di->battery_info->battery_voltage) {
                                di->bat_voltage_mV = di->battery_info->battery_voltage();
                        }
                        di->voffset = 30;
                }
        }

        if(di->battery_info->battery_capacity) {
                curr_capacity = di->battery_info->battery_capacity();
        }
        if(di->battery_info->battery_realsoc) {
            di->realsoc = di->battery_info->battery_realsoc();
        }
        /* Debouncing power on init when voltage change. */
        if (di->capacity == -1) {
#ifdef CONFIG_BATINFO_FILE
                if(load_batinfo() == 0) {
                        if((ts.tv_sec - lsec < 24*3600)&&(abs(di->bat_voltage_mV - lvoltage) <250)) {
                                if((di->bat_present)&&(di->bat_voltage_mV > 3550)) {
                                        curr_capacity = lcapacity;
                                        di->history_capacity = lcapacity;
                                        pr_info("monitor_battery: load_batinfo soc = %d\n",curr_capacity);
                                }
                        }
                }
#endif
                di->voffset = 0;
                di->capacity  = curr_capacity;
                di->prev_capacity = curr_capacity;
                di->capacity_debounce_count = 0;
                di->prev_voltage = di->bat_voltage_mV;
                pr_info("monitor_battery: power on soc(volt) = %d(%d mV)\n",curr_capacity,di->bat_voltage_mV);
                mutex_unlock(&battery_mutex);
                return 1;
        }

        if((di->charger_source != POWER_SUPPLY_TYPE_BATTERY)&&(curr_capacity >= 100)) {
                di->charge_status = POWER_SUPPLY_STATUS_FULL;
                curr_capacity = 100;
        }

        if (curr_capacity != di->prev_capacity) {
                pr_info("monitor_battery: curr_capacity = %d(%d) " "curr_voltage = %d(%d) " "curr = %d "
                        "battery_temp = %d " "charge_status = %d(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
                        curr_capacity,di->prev_capacity,di->bat_voltage_mV,di->prev_voltage,di->bat_current_mA,
                        di->bat_temperature,di->charge_status,tm.tm_year + 1900,
                        tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
                di->prev_capacity = curr_capacity;
                di->capacity = curr_capacity;
                di->capacity_debounce_count = 0;
                di->prev_voltage = di->bat_voltage_mV;
                mutex_unlock(&battery_mutex);
                return 1;
        }

        if (++di->capacity_debounce_count >= 2) {
                if(curr_capacity != 100) {
                        pr_info("monitor_battery: curr_capacity = %d(%d) " "curr_voltage = %d(%d) " "curr = %d "
                                "battery_temp = %d " "charge_status = %d(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
                                curr_capacity,di->prev_capacity,di->bat_voltage_mV,di->prev_voltage,di->bat_current_mA,
                                di->bat_temperature,di->charge_status,tm.tm_year + 1900,
                                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
                }
                di->capacity = curr_capacity;
                di->capacity_debounce_count = 0;
                mutex_unlock(&battery_mutex);
                return 1;
        }
        mutex_unlock(&battery_mutex);
        return 0;
}

static void monitor_battery_work(struct work_struct *work)
{
        struct monitor_battery_device_info *di = container_of(work,
                        struct monitor_battery_device_info, battery_monitor_work.work);
        int ret = 0;

        wake_lock(&di->monitor_wakelock);
#ifdef CONFIG_BATINFO_FILE
        if (di->capacity == -1) {
                msleep(2500);
        }
#endif
        monitor_battery_update_data(di);
        ret = capacity_monitor_thread(di);
        if (ret) {
#ifdef CONFIG_BATINFO_FILE
                if(di->bat_present) {
                        store_batinfo(di->capacity, di->bat_voltage_mV);
                } else {
                        store_batinfo(di->capacity, 0);
                }
#endif
                power_supply_changed(&di->bat);
        }

        schedule_delayed_work(&di->battery_monitor_work,
                              msecs_to_jiffies(1000 * di->monitoring_interval));

        wake_unlock(&di->monitor_wakelock);
        return;
}
#else


static void fuelgauge_update_work(struct work_struct *work)
{
        struct monitor_battery_device_info *di =
                container_of(work, struct monitor_battery_device_info, fg_update_work);
        int ret = 0;

        di->fg_update = 0;
        if(di->battery_info->firmware_update) {
                ret = di->battery_info->firmware_update(COULOMETER_DEV);
        }
        if(ret < 0) {
                dev_err(di->dev, "no update and must be again!\n");
                di->fg_update = 1;
        } else {
                di->fg_update = 0;
        }

        return;
}
#define FG_VOLT_FILTER_LEN   (5)
static int monitor_battery_voltage_filter(void)
{
        struct monitor_battery_device_info *di = bci;
        int i = 0,index = 0;
        u32 sum = 0;

        index = di->fgvolt_filter_count%FG_VOLT_FILTER_LEN;

        di->rbuf_fgvolt[index] = di->bat_voltage_mV;
        for(i=0; i < FG_VOLT_FILTER_LEN; i++) {
                sum = sum + di->rbuf_fgvolt[i];
        }

        if (++di->fgvolt_filter_count >= FG_VOLT_FILTER_LEN) {
                di->fgvolt_filter_count = 0;
        }

        return(sum/FG_VOLT_FILTER_LEN);
}

static int monitor_battery_capacity_filter(struct monitor_battery_device_info *di,int curr_capacity)
{
    int index = 0,i=0;
    int capacity_sum = 0;
    int soc = 0;
    struct timespec ts;
    static __kernel_time_t last_sec = 0;

    getnstimeofday(&ts);
    if (di->charger_source == POWER_SUPPLY_TYPE_MAINS) {
        if((abs(ts.tv_sec - last_sec) >= 1000)&&(di->capacity != -1)) {
            pr_info("monitor_battery: main ac timeout");
            for(i=0; i < CAP_FILTER_LEN; i++) {
                di->capacity_filter[i] = curr_capacity;
            }
        }
    }
    last_sec = ts.tv_sec;
    index = di->capacity_filter_count%CAP_FILTER_LEN;
    di->capacity_filter[index] = curr_capacity;
    for(i=0; i < CAP_FILTER_LEN; i++) {
        capacity_sum = capacity_sum + di->capacity_filter[i];
    }
    if (++di->capacity_filter_count >= CAP_FILTER_LEN) {
        di->capacity_filter_count = 0;
    }
    soc = capacity_sum/CAP_FILTER_LEN;
    if(curr_capacity == 0){
        soc  = 0;
    }
	return curr_capacity;
}
/*if use gasgauge*/
static int monitor_capacity_from_voltage(struct monitor_battery_device_info *di)
{
        int curr_capacity  = 20;

        if(di->bat_voltage_mV < 3500) {
                curr_capacity = 5;
        } else if ((di->bat_voltage_mV >=3500)&&(di->bat_voltage_mV < 3600)) {
                curr_capacity = 10;
        } else if ((di->bat_voltage_mV >=3600)&&(di->bat_voltage_mV < 3700)) {
                curr_capacity = 20;
        } else if ((di->bat_voltage_mV >=3700)&&(di->bat_voltage_mV < 3800)) {
                curr_capacity = 50;
        } else if ((di->bat_voltage_mV >=3800)&&(di->bat_voltage_mV < 4000)) {
                curr_capacity = 80;
        } else if((di->bat_voltage_mV >=4000)&&(di->bat_voltage_mV < 4100)) {
                curr_capacity = 90;
        } else if (di->bat_voltage_mV >=4100) {
                curr_capacity = 100;
        }

        return curr_capacity;
}

static int capacity_monitor_thread(struct monitor_battery_device_info *di)
{
        int curr_capacity = di->capacity,voltage_filter = 0;
        struct timespec ts;
        struct rtc_time tm;

        mutex_lock(&battery_mutex);
        getnstimeofday(&ts);
        rtc_time_to_tm(ts.tv_sec, &tm);

        if (!di->bat_present) {
                curr_capacity = monitor_capacity_from_voltage(di);
                di->realsoc = curr_capacity;
        } else {
                if(di->battery_info->battery_capacity) {
                        curr_capacity = di->battery_info->battery_capacity();
                }
                if(di->battery_info->battery_realsoc) {
                    di->realsoc = di->battery_info->battery_realsoc();
                }
        }

        if(di->charger_source != POWER_SUPPLY_TYPE_BATTERY) {
                monitor_battery_temp_charging(di);
        }
        voltage_filter = monitor_battery_voltage_filter();
        curr_capacity = monitor_battery_capacity_filter(di,curr_capacity);
        /* Debouncing power on init when voltage change. */
        if (di->capacity == -1) {
                di->capacity  = curr_capacity;
                di->prev_capacity = curr_capacity;
                di->prev_voltage = di->bat_voltage_mV;
                di->capacity_debounce_count = 0;
                pr_info("monitor_battery: init curr_capacity = %d(%d) " "temp = %d " "rmfcc = %d(%d) \n",di->capacity,di->realsoc,di->bat_temperature,di->bat_rm,di->bat_fcc);
                mutex_unlock(&battery_mutex);
                return 1;
        }

        if(di->charger_source != POWER_SUPPLY_TYPE_BATTERY) {
            if(curr_capacity >= 100){
                di->charge_full_count++;
                if(di->charge_full_count >= 100){
                    di->charge_full_count = 100;
                    di->charge_status = POWER_SUPPLY_STATUS_FULL;
                    curr_capacity = 100;
                }else{
                    if((di->realsoc >= 100)||(di->capacity >= 100)){
                        di->charge_status = POWER_SUPPLY_STATUS_FULL;
                        curr_capacity = 100;
                    }else{
                        curr_capacity = 99;
                    }
                }
            }else{
                if(curr_capacity > (di->prev_capacity + 2)) {
                    if(di->fgchip_type != FG_CHIP_TYPE_CW2015)
                    {
                        curr_capacity = di->prev_capacity + 1;
                    }
                    else if((di->fgchip_type == FG_CHIP_TYPE_CW2015)&&(di->cap_adjust_flag ==1))
                    {
                        curr_capacity = di->prev_capacity + 1;
                    }
                    if(curr_capacity > 100){
                        curr_capacity = 100;
                    }
                }
                if(di->charge_status == POWER_SUPPLY_STATUS_FULL){
                    curr_capacity = 100;/*sw charge done set 100 soc*/
                }
            }
        }else{
            di->charge_full_count = 0;
        }

        if(di->charger_source == POWER_SUPPLY_TYPE_BATTERY) {
                if(curr_capacity > di->prev_capacity) {
                    if(di->fgchip_type != FG_CHIP_TYPE_CW2015)
                    curr_capacity = di->prev_capacity;
                    else if((di->fgchip_type == FG_CHIP_TYPE_CW2015)&&(di->cap_adjust_flag ==1))
                   {
                       pr_info("monitor_battery: in battery mode adjust curr_capacity= %d(%d)\n",curr_capacity,di->prev_capacity);
                       curr_capacity = di->prev_capacity;
                   }
                }
                
                if(di->fgchip_type != FG_CHIP_TYPE_CW2015)
               {
                if((curr_capacity >=0)&&(di->capacity <= 10)&&(di->capacity >= 1)&&(voltage_filter <= 3500)){
                    pr_info("monitor_battery: low curr_capacity = %d(%d mV)\n",curr_capacity,voltage_filter);
	                    curr_capacity = di->capacity --;
	                }
               }
               else
              {
                       if((curr_capacity >=0)&&(di->capacity <= 10)&&(di->capacity >= 1)&&(voltage_filter <= 3450)){
	                    pr_info("monitor_battery: low curr_capacity = %d(%d mV)\n",curr_capacity,voltage_filter);
	                    curr_capacity = di->capacity --;
	                }
                }
        }

        if (curr_capacity != di->prev_capacity) {
                pr_info("monitor_battery: curr_capacity = %d(%d) " "curr_voltage = %d(%d) " "curr = %d "
                        "battery_temp = %d(%d) " "rmfcc = %d(%d) " "charge_status = %d(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)%d\n",
                        curr_capacity,di->prev_capacity,di->bat_voltage_mV,di->prev_voltage,di->bat_current_mA,
                        di->bat_temperature,di->bat_chiptemp,di->bat_rm,di->bat_fcc,di->charge_status,tm.tm_year + 1900,
                        tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec,di->realsoc);
                di->prev_capacity = curr_capacity;
                di->prev_voltage = di->bat_voltage_mV;
                di->capacity = curr_capacity;
                di->capacity_debounce_count = 0;
                mutex_unlock(&battery_mutex);
                return 1;
        }

        if (++di->capacity_debounce_count >= 2) {
                if(di->realsoc != 100) {
                        pr_info("monitor_battery: curr_capacity = %d(%d) " "curr_voltage = %d(%d) " "curr = %d "
                                "battery_temp = %d(%d) " "rmfcc = %d(%d) " "charge_status = %d(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)%d\n",
                                curr_capacity,di->prev_capacity,di->bat_voltage_mV,di->prev_voltage,di->bat_current_mA,
                                di->bat_temperature,di->bat_chiptemp,di->bat_rm,di->bat_fcc,di->charge_status,tm.tm_year + 1900,
                                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec,di->realsoc);
                }
                di->capacity = curr_capacity;
                di->capacity_debounce_count = 0;
                mutex_unlock(&battery_mutex);
                return 1;
        }
        mutex_unlock(&battery_mutex);
        return 0;
}

static void monitor_battery_work(struct work_struct *work)
{
        struct monitor_battery_device_info *di = container_of(work,
                        struct monitor_battery_device_info, battery_monitor_work.work);
        int ret = 0;
#ifdef CONFIG_BATTERY_BQ27421
        unsigned long timeout = jiffies + 6*HZ;
        int curr_capacity = 0,i = 0;
#endif
        unsigned int power_on_type;
        wake_lock(&di->monitor_wakelock);
        power_on_type = lc1160_power_type_get();

        monitor_battery_update_data(di);
#ifdef CONFIG_BATTERY_BQ27421
        if(di->capacity == -1){
            if (!di->bat_present) {
                curr_capacity = monitor_capacity_from_voltage(di);
                for(i =0;i < 5; i++){
                    di->capacity_filter[i] = curr_capacity;
                }
            } else {
                if(di->battery_info->check_init_complete) {
                    while (!(di->battery_info->check_init_complete())){
                        if (time_after(jiffies, timeout)) {
                            pr_info("bq27421-battery: firmware is not init ok\n");
                            goto exit;
                        }
                        msleep(1000);
                    }
                    pr_info("bq27421-battery: firmware init ok\n");
                } else {
                    pr_info("monitor_battery_work: power_on_type = %d\n",power_on_type);	
                    if(power_on_type ==PU_REASON_USB_CHARGER){
                        msleep(7000);
                    }
                    else if(power_on_type == PU_REASON_REBOOT_RECOVERY)
                    { 
                        //nothing
                    }
                    else
                    {
                        msleep(4000);
                    }
                }
                exit:
                if(di->battery_info->battery_capacity) {
                    curr_capacity = di->battery_info->battery_capacity();
                }
                for(i =0;i < 5; i++){
                    di->capacity_filter[i] = curr_capacity;
                }
            }
        }
#endif
        /*fg firmware upload if di->fg_update==1*/
        if(di->fg_update) {
                //schedule_work(&di->fg_update_work);
        }

       if((di->fgchip_type == FG_CHIP_TYPE_CW2015)&&(di->cap_valid_count < 5))
       {
            di->cap_valid_count ++;

            if(di->cap_valid_count == 5)
            {
                 di->cap_adjust_flag = 1;
            }
       }
        ret = capacity_monitor_thread(di);
        if (ret)
                power_supply_changed(&di->bat);
         if(di->fgchip_type != FG_CHIP_TYPE_CW2015)
         {
        if(di->capacity < 15){
            schedule_delayed_work(&di->battery_monitor_work, msecs_to_jiffies(1000 * 5));
        }else{
            schedule_delayed_work(&di->battery_monitor_work,
                msecs_to_jiffies(1000 * di->monitoring_interval));
              }
         }
            else   //cw2015 fg
            {
               if(di->cap_adjust_flag == 1)
               {
                   if(di->capacity < 15){
                      schedule_delayed_work(&di->battery_monitor_work, msecs_to_jiffies(1000 * 5));
                   }else{
                  schedule_delayed_work(&di->battery_monitor_work,
                               msecs_to_jiffies(1000 * di->monitoring_interval));
                   }
               }
               else
               {
                   pr_info("monitor_battery_work: 5s work timer\n");
                   schedule_delayed_work(&di->battery_monitor_work, msecs_to_jiffies(1000 * 5));
            }
        }

        wake_unlock(&di->monitor_wakelock);
        return;
}
#endif

static void monitor_usb_charger_work(struct work_struct *work)
{
        struct monitor_battery_device_info *di =
                container_of(work, struct monitor_battery_device_info, usb_work);

        pr_info("monitor_battery: plugin_devices_event = %ld\n",di->event);
        switch (di->event) {
        case LCUSB_EVENT_PLUGIN:
                di->ac_online = 0;
                di->plug_online = 1;
                di->charger_source = POWER_SUPPLY_TYPE_USB;
                if(di->charger_info->set_charging_source)
                        di->charger_info->set_charging_source(LCUSB_EVENT_PLUGIN);
#ifdef CONFIG_HAS_WAKELOCK
                wake_lock_timeout(&di->usb_wakelock, 15 * HZ);
#endif
                if(di->init_plugin){
                    di->charge_status =POWER_SUPPLY_STATUS_CHARGING;
                    di->init_plugin = 0;
                }
                break;
        case LCUSB_EVENT_CHARGER:
                monitor_start_ac_source(di);
                break;
        case LCUSB_EVENT_VBUS:
                monitor_start_usb_source(di);
                break;
        case LCUSB_EVENT_NONE:
                monitor_stop_source(di);
                break;
        case LCUSB_EVENT_OTG: // OTG
                monitor_start_otg_source(di);
                break;
        default:
                break;
        }

        if((di->plug_online|di->usb_online | di->ac_online) && (di->capacity ==100)) {
                di->charge_status = POWER_SUPPLY_STATUS_FULL;
        }

        if (di->capacity != -1) {
                power_supply_changed(&di->bat);
        } else {
                cancel_delayed_work(&di->battery_monitor_work);
                schedule_delayed_work(&di->battery_monitor_work, 0);
        }

        if(di->charger_source == POWER_SUPPLY_TYPE_BATTERY) {
                wake_lock_timeout(&di->usb_wakelock, msecs_to_jiffies(2000));
        }

        return;
}

static int monitor_usb_notifier_call(struct notifier_block *nb,
                                     unsigned long event, void *data)
{
        struct monitor_battery_device_info *di =
                container_of(nb, struct monitor_battery_device_info, nb);

        di->event = event;
        schedule_work(&di->usb_work);

        return NOTIFY_OK;
}

static char *monitor_battery_supplied_to[] = {
        "monitor_battery",
};

static enum power_supply_property monitor_usb_prop[] = {
        POWER_SUPPLY_PROP_ONLINE,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_HEALTH,
};

static enum power_supply_property monitor_charger_prop[] = {
        POWER_SUPPLY_PROP_ONLINE,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_HEALTH,
};

static enum power_supply_property monitor_battery_prop[] = {
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_ONLINE,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_VOLTAGE_AVG,
        POWER_SUPPLY_PROP_CURRENT_NOW,
        POWER_SUPPLY_PROP_TECHNOLOGY,
        POWER_SUPPLY_PROP_CAPACITY,
        POWER_SUPPLY_PROP_TEMP,
        POWER_SUPPLY_PROP_TEMP_AMBIENT,
};

#define to_monitor_charger_bat(x) container_of((x), \
        struct monitor_battery_device_info, bat);

#define to_monitor_charger_ac(x) container_of((x), \
        struct monitor_battery_device_info, ac);

#define to_monitor_charger_usb(x) container_of((x), \
        struct monitor_battery_device_info, usb);

static int monitor_usb_get_property(struct power_supply *psy,
                                    enum power_supply_property psp,
                                    union power_supply_propval *val)
{
        struct monitor_battery_device_info *di = to_monitor_charger_usb(psy);

        switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
                val->intval = di->usb_online;
                break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
                val->intval = 0;
                break;
        case POWER_SUPPLY_PROP_HEALTH:
                val->intval = di->charger_health;
                break;
        default:
                return -EINVAL;
        }
        return 0;
}

static int monitor_charger_get_property(struct power_supply *psy,
                                        enum power_supply_property psp,
                                        union power_supply_propval *val)
{
        struct monitor_battery_device_info *di = to_monitor_charger_ac(psy);

        switch (psp) {
        case POWER_SUPPLY_PROP_ONLINE:
                val->intval = di->ac_online;
                break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
                val->intval = 0;
                break;
        case POWER_SUPPLY_PROP_HEALTH:
                val->intval = di->charger_health;
                break;
        default:
                return -EINVAL;
        }
        return 0;
}

static int monitor_battery_get_property(struct power_supply *psy,
                                        enum power_supply_property psp,
                                        union power_supply_propval *val)
{
        struct monitor_battery_device_info *di = to_monitor_charger_bat(psy);

        switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
                if((di->charger_source != POWER_SUPPLY_TYPE_BATTERY)&&(di->capacity >= 100)) {
                        di->charge_status = POWER_SUPPLY_STATUS_FULL;
                }
                val->intval = di->charge_status;
                break;
        case POWER_SUPPLY_PROP_HEALTH:
                val->intval = di->bat_health;
                break;
        case POWER_SUPPLY_PROP_ONLINE:
        case POWER_SUPPLY_PROP_PRESENT:
                val->intval = di->bat_present;
                break;
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#ifdef CONFIG_BATTERY_FG
                if(di->battery_info->battery_voltage) {
                        di->bat_voltage_mV = di->battery_info->battery_voltage();
                }
                val->intval = di->bat_voltage_mV *1000;
#else
                val->intval = 1000* monitor_adc_channel_voltage(ADC_VOLT_CHANNEL);
#endif

                break;
        case POWER_SUPPLY_PROP_VOLTAGE_AVG:
                val->intval = di->bat_voltage_mV *1000;
                break;
        case POWER_SUPPLY_PROP_CURRENT_NOW:
                if(di->battery_info->battery_current) {
                        di->bat_current_mA = (-1) * di->battery_info->battery_current();
                }
                val->intval = di->bat_current_mA*1000;
                break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
                val->intval = di->bat_tech;
                break;
        case POWER_SUPPLY_PROP_CAPACITY:
                val->intval = di->capacity;
                if (val->intval == -1) {
                        return -EINVAL;
                }
                break;
        case POWER_SUPPLY_PROP_TEMP:
                val->intval = di->bat_temperature *10;
                break;
        case POWER_SUPPLY_PROP_TEMP_AMBIENT:
                val->intval = di->dbb_temp;
                break;
        default:
                return -EINVAL;
        }

        return 0;
}

static void monitor_power_supply_unregister(struct monitor_battery_device_info *di)
{
        power_supply_unregister(&di->bat);
        power_supply_unregister(&di->ac);
        power_supply_unregister(&di->usb);
}

/*sysfs*/
static ssize_t set_charging(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
        int status = count;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if (strncmp(buf, "startac", 7) == 0) {
                if (di->charger_source == POWER_SUPPLY_TYPE_USB)
                        monitor_stop_source(di);
                monitor_start_ac_source(di);
        } else if (strncmp(buf, "startusb", 8) == 0) {
                if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
                        monitor_stop_source(di);
                monitor_start_usb_source(di);
        } else if (strncmp(buf, "stop" , 4) == 0) {
                monitor_stop_source(di);
        } else {
                return -EINVAL;
        }
        return status;
}

static ssize_t show_chargelog(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
        //unsigned long val;
        u8 read_reg[15] = {0},buf_temp[15] = {0};
        u16 reg[15] = {0};
        int i =0;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        for(i=0; i<10; i++) {
                pmic_read_byte(di,i+LC11XX_BASEADD_CHARGER,&reg[i]);
        }

        for(i=0; i<10; i++) {
                read_reg[i] = (u8)reg[i];
                sprintf(buf_temp,"0x%-8.2x",read_reg[i]);
                strcat(buf,buf_temp);
        }
        strcat(buf,"\n");
        return strlen(buf);
}

static ssize_t set_charging_voltage(struct device *dev,
                                    struct device_attribute *attr, const char *buf, size_t count)
{
        long val;
        int status = count;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val < 4200) ||
            (val > 4350))
                return -EINVAL;
        di->param.voltagemV = val;
        di->param.max_voltagemV = val;
        monitor_charger_voltage_reg_set(di);

        return status;
}

static ssize_t show_charging_voltage(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
        unsigned int val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        val = di->param.voltagemV;
        return sprintf(buf, "%u\n", val);
}

static ssize_t set_termination_current(struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t count)
{
        long val;
        int status = count;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val < 50) || (val > 200))
                return -EINVAL;
        di->param.termination_currentmA = val;
        monitor_charger_current_reg_set(di);

        return status;
}

static ssize_t show_termination_current(struct device *dev,
                                        struct device_attribute *attr, char *buf)
{
        unsigned int val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        val = di->param.termination_currentmA;
        return sprintf(buf, "%u\n", val);
}

static ssize_t set_cin_limit_usb(struct device *dev,
                                 struct device_attribute *attr, const char *buf, size_t count)
{
        long val;
        int status = count;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val < 300) || (val > 1200))
                return -EINVAL;
        di->platform_data->usb_battery_currentmA = val;
        di->param.currentmA = di->platform_data->usb_battery_currentmA;
        monitor_charger_current_reg_set(di);
        return status;
}

static ssize_t show_cin_limit_usb(struct device *dev, struct device_attribute *attr,
                                  char *buf)
{
        unsigned int val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        val = di->platform_data->usb_battery_currentmA;
        return sprintf(buf, "%u\n", val);
}

static ssize_t set_cin_limit_ac(struct device *dev,
                                struct device_attribute *attr, const char *buf, size_t count)
{
        long val;
        int status = count;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val < 500) || (val > 1200))
                return -EINVAL;
        di->platform_data->ac_battery_currentmA = val;
        di->param.currentmA = di->platform_data->ac_battery_currentmA;
        monitor_charger_current_reg_set(di);
        return status;
}

static ssize_t show_cin_limit_ac(struct device *dev, struct device_attribute *attr,
                                 char *buf)
{
        unsigned int val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        val = di->platform_data->ac_battery_currentmA;
        return sprintf(buf, "%u\n", val);
}


static ssize_t set_timer_clear(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count)
{
        long val;
        int status = count;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val > 1))
                return -EINVAL;

        di->param.timer_clear = val;
        monitor_charger_timer_reg_set(di);

        return status;
}

static ssize_t set_timer_stop(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
        long val;
        int status = count;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val > 1))
                return -EINVAL;
        di->param.timer_stop = val;
        monitor_charger_timer_reg_set(di);

        return status;
}

static ssize_t set_enable_charger(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
        long val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
                return -EINVAL;

        di->param.enable_charger= val;
        di->hand_flag = val;
        if(di->charger_info->set_charging_enable) {
                di->charger_info->set_charging_enable(di->hand_flag);
        }
        if(di->param.enable_charger) {
                di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
        } else {
                di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }
        power_supply_changed(&di->bat);
        return count;
}

static ssize_t show_enable_charger(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
        unsigned long val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        val = di->param.enable_charger;
        return sprintf(buf, "%lu\n", val);
}
static ssize_t set_enable_itermination(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf, size_t count)
{
        long val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
                return -EINVAL;

        di->param.enable_iterm= val;
        monitor_charger_control_reg_set(di);

        return count;
}

static ssize_t show_enable_itermination(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)
{
        unsigned long val;
        struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        val = di->param.enable_iterm;
        return sprintf(buf, "%lu\n", val);
}

static ssize_t show_chargerchipdetect(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    int val = -1;
    struct monitor_battery_device_info *di = dev_get_drvdata(dev);

        if(di->charger_info->get_chargerchip_detect)
            val = di->charger_info->get_chargerchip_detect();

    return sprintf(buf, "%d\n", val);
}


static DEVICE_ATTR(chargerdetect, S_IWUSR | S_IRUGO,
                show_chargerchipdetect,
                NULL);
static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO, NULL, set_charging);
static DEVICE_ATTR(chargelog, S_IWUSR | S_IRUGO, show_chargelog, NULL);
static DEVICE_ATTR(charging_voltage, S_IWUSR | S_IRUGO,
                   show_charging_voltage, set_charging_voltage);
static DEVICE_ATTR(termination_current, S_IWUSR | S_IRUGO,
                   show_termination_current, set_termination_current);
static DEVICE_ATTR(cin_limit_usb, S_IWUSR | S_IRUGO, show_cin_limit_usb,
                   set_cin_limit_usb);
static DEVICE_ATTR(cin_limit_ac, S_IWUSR | S_IRUGO, show_cin_limit_ac,
                   set_cin_limit_ac);
static DEVICE_ATTR(timer_clear, S_IWUSR, NULL, set_timer_clear);
static DEVICE_ATTR(timer_stop, S_IWUSR, NULL, set_timer_stop);
static DEVICE_ATTR(enable_charger, S_IWUSR | S_IRUGO,
                   show_enable_charger,
                   set_enable_charger);
static DEVICE_ATTR(enable_iterm, S_IWUSR | S_IRUGO,
                   show_enable_itermination,
                   set_enable_itermination);

static struct attribute *monitor_battery_attributes[] = {
        &dev_attr_charging.attr,
        &dev_attr_chargelog.attr,
        &dev_attr_charging_voltage.attr,
        &dev_attr_termination_current.attr,
        &dev_attr_cin_limit_usb.attr,
        &dev_attr_cin_limit_ac.attr,
        &dev_attr_timer_clear.attr,
        &dev_attr_timer_stop.attr,
        &dev_attr_enable_charger.attr,
        &dev_attr_enable_iterm.attr,
        &dev_attr_chargerdetect.attr,
        NULL,
};

static const struct attribute_group monitor_battery_attr_group = {
        .attrs = monitor_battery_attributes,
};

#ifdef CONFIG_PROC_FS
/*amt adjust code begin*/
static char proc_cmd[100] = {0};
static int  proc_data[20] = {0};

#define G_VBATT_MIN(x)  (g_vbatt[0] <= x || g_vbatt[1] <= x || g_vbatt[2] <= x || g_vbatt[3] <= x)
#define G_VBATT_MAX(x)  (g_vbatt[0] >= x || g_vbatt[1] >= x || g_vbatt[2] >= x || g_vbatt[3] >= x)

#if 0
static ssize_t monitor_battery_proc_read(struct file *file, char __user *buffer,
                size_t count, loff_t *ppos)
{
        struct monitor_battery_device_info *di = PDE_DATA(file_inode(file));

        if (proc_cmd[0] == '\0') {
                return snprintf(buffer, PAGE_SIZE, "vbat=%d chn1=%d chn2=%d chn3=%d, chn4=%d\n",
                                monitor_adc_channel_voltage(ADC_VOLT_CHANNEL),
                                pmic_get_adc_conversion(1),
                                pmic_get_adc_conversion(2),
                                pmic_get_adc_conversion(3),
                                pmic_get_adc_conversion(4));
        } else if (!strcmp(proc_cmd, "vbatt")) {
                return snprintf(buffer, PAGE_SIZE, "vbatt %d", monitor_adc_channel_voltage(ADC_VOLT_CHANNEL));
        } else if (!strcmp(proc_cmd, "amt_get_vbatt")) {
                int vbatt;
                if(di->charger_info->set_charging_enable) {
                        di->charger_info->set_charging_enable(0);
                }
                mdelay(100);
                vbatt = monitor_adc_channel_voltage(ADC_VOLT_CHANNEL);
                if(di->charger_info->set_charging_enable) {
                        di->charger_info->set_charging_enable(1); /* restore charging */
                }
                return snprintf(buffer, PAGE_SIZE, "%d\n", vbatt);
        } else {
                return snprintf(buffer, PAGE_SIZE, "%s", "Unknown command");
        }
}
#endif

static int monitor_battery_proc_write(struct file *file, const char __user *buffer,
                                      size_t count, loff_t *ppos)
{
        static char kbuf[4096];
        char *buf = kbuf;
        struct monitor_battery_device_info *di = PDE_DATA(file_inode(file));
        char cmd;

        if (count >= 4096)
                return -EINVAL;

        if (copy_from_user(buf, buffer, count))
                return -EFAULT;

        cmd = buf[0];
        pr_info("%s:%s\n", __func__, buf);

        if ('g' == cmd) {
                char type;
                sscanf(buf, "%c %c", &cmd, &type);
                if (type == 'b') {
                        sprintf(proc_cmd, "%s", "vbatt");
                        proc_data[0] = monitor_adc_channel_voltage(ADC_VOLT_CHANNEL);
                        pr_info("Get battery voltage %d", proc_data[0]);
                }
        } else if ('a' == cmd) {
                if (!strncmp(buf, "amt_get_vbatt", strlen("amt_get_vbatt"))) {
                        sprintf(proc_cmd, "%s", "amt_get_vbatt");
                } else {
                        int ret;
                        pr_info("AMT vbatt init...");
                        ret = sscanf(buf, "amt_set_vbatt[%d] [%d] [%d] [%d] \n", &g_vbatt[0], &g_vbatt[1], &g_vbatt[2], &g_vbatt[3]);
                        pr_info("AMT orignal g_vbatt[%d] [%d] [%d] [%d]\n", g_vbatt[0], g_vbatt[1], g_vbatt[2], g_vbatt[3]);
                        if (!ret || (G_VBATT_MIN(3400)) || (G_VBATT_MAX(4400))) {
                                g_vbatt[0] = 4200;
                                g_vbatt[1] = 4200;
                                g_vbatt[2] = 3600;
                                g_vbatt[3] = 3600;
                                pr_info("read from AMT param failed or the value is invalid! use fault value!\n");
                        }
                        cancel_delayed_work(&di->battery_monitor_work);
                        schedule_delayed_work(&di->battery_monitor_work, 0);
                }
        }
        pr_info("\n");

        return count;
}

static int monitor_battery_proc_show(struct seq_file *s, void *v)
{
        struct monitor_battery_device_info *di = s->private;

        if (proc_cmd[0] == '\0') {
                seq_printf(s, "vbat=%d chn1=%d chn2=%d chn3=%d, chn4=%d\n",
                           monitor_adc_channel_voltage(ADC_VOLT_CHANNEL),
                           pmic_get_adc_conversion(1),
                           pmic_get_adc_conversion(2),
                           pmic_get_adc_conversion(3),
                           pmic_get_adc_conversion(4));
        } else if (!strcmp(proc_cmd, "vbatt")) {
                seq_printf(s, "vbatt = %d\n", monitor_adc_channel_voltage(ADC_VOLT_CHANNEL));
        } else if (!strcmp(proc_cmd, "amt_get_vbatt")) {
                int vbatt;
                if(di->charger_info->set_charging_enable) {
                        di->charger_info->set_charging_enable(0);
                }
                mdelay(100);
                vbatt = monitor_adc_channel_voltage(ADC_VOLT_CHANNEL);
                if(di->charger_info->set_charging_enable) {
                        di->charger_info->set_charging_enable(1); /* restore charging */
                }
                seq_printf(s,"%d\n", vbatt);
        } else {
                seq_printf(s, "%s", "Unknown command");
        }
        return 0;
}

static int monitor_battery_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, monitor_battery_proc_show, PDE_DATA(inode));
}

static const struct file_operations monitor_battery_operations = {
        .owner = THIS_MODULE,
        .open  = monitor_battery_proc_open,
        .read  = seq_read,
        .write = monitor_battery_proc_write,
};
#endif

static void monitor_battery_proc_init(struct monitor_battery_device_info *data)
{
#ifdef CONFIG_PROC_FS
        struct proc_dir_entry *proc_entry = NULL;
        proc_entry = proc_create_data("driver/comip_battery", 0, NULL,
                                      &monitor_battery_operations, data);
#endif
}
/*amt adjust code end*/

static enum alarmtimer_restart
monitor_battery_alarm_restart(struct alarm *wk_alarm, ktime_t now)
{
        struct monitor_battery_device_info *di =
                container_of(wk_alarm, struct monitor_battery_device_info,
                             wakeup_alarm);

        dev_dbg(di->dev, "Wake-up by alarm timer\n");
        return ALARMTIMER_NORESTART;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void monitor_battery_early_suspend(struct early_suspend *h)
{
        // struct monitor_battery_device_info *di =
        //     container_of(h, struct monitor_battery_device_info, early_suspend);

        printk(KERN_DEBUG "monitor_battery:early_suspend \n");
        return;
}

static void monitor_battery_early_resume(struct early_suspend *h)
{
        struct monitor_battery_device_info *di =
                container_of(h, struct monitor_battery_device_info, early_suspend);

        if(!di->resume_flag) {
                schedule_delayed_work(&di->battery_monitor_work, 0);
                di->resume_flag = 1;
        }
        printk(KERN_DEBUG "monitor_battery:early_resume \n");
        return;
}

#endif /* CONFIG_HAS_EARLYSUSPEND */


static int __init monitor_battery_probe(struct platform_device *pdev)
{
        struct monitor_battery_device_info *di;
        struct monitor_battery_platform_data *pdata = pdev->dev.platform_data;
        int ret = 0,i = 0;
        int vbus_state = 0,curr_capacity = 0;
        enum lcusb_xceiv_events plugin_stat;

        if (!pdata) {
                dev_err(&pdev->dev, "platform_data not available\n");
                return -EINVAL;
        }

        di = kzalloc(sizeof(*di), GFP_KERNEL);
        if (!di) {
                dev_err(&pdev->dev, "di not available\n");
                return -ENOMEM;
        }

        if(pdata->max_charger_voltagemV < 4200)
                pdata->max_charger_voltagemV = 4200;
        if(pdata->ac_battery_currentmA < 500)
                pdata->ac_battery_currentmA = 500;
        if(pdata->usb_battery_currentmA < 300)
                pdata->usb_battery_currentmA = 300;
        if(pdata->rechg_voltagemV < 100)
                pdata->rechg_voltagemV = RCHGSEL_VOLT;
        if(pdata->temp_low_threshold == 0)
                pdata->temp_low_threshold = BAT_TEMP_STAGE_COLD;
        if(pdata->temp_zero_threshold == 0)
                pdata->temp_zero_threshold = BAT_TEMP_STAGE_ZERO;
        if(pdata->temp_cool_threshold == 0)
                pdata->temp_cool_threshold = BAT_TEMP_STAGE_COOL;
        if(pdata->temp_warm_threshold == 0)
                pdata->temp_warm_threshold = BAT_TEMP_STAGE_NORMAL;
        if(pdata->temp_high_threshold == 0)
                pdata->temp_high_threshold = BAT_TEMP_STAGE_HOT;
        di->platform_data = pdata ;

        if (pdata->monitoring_interval == 0) {
                di->monitoring_interval = 20;
        } else {
                di->monitoring_interval = pdata->monitoring_interval;
        }
        di->param.max_voltagemV = di->platform_data->max_charger_voltagemV;
        di->param.termination_currentmA = di->platform_data->termination_currentmA;
        di->param.cin_limit = di->platform_data->usb_battery_currentmA;
        di->param.currentmA = di->platform_data->usb_battery_currentmA;
        di->param.voltagemV = di->param.max_voltagemV ;
        di->param.rechg_voltagemV = di->platform_data->rechg_voltagemV;
        di->adc_temp_chnl = ADC_TEMP_CHANNEL;
        di->fgchip_type = FG_CHIP_TYPE_UNKNOWN;

        di->dev = &pdev->dev;
        bci = di;

        di->charger_info = &extern_charger_chg;
        if(!di->charger_info) {
                dev_err(&pdev->dev, "monitor_battery_charger is not init \n");
        }
#ifndef CONFIG_BATTERY_FG
        comip_battery_property_register(&monitor_battery_property);
#endif
        /*set battery info interface*/
        di->battery_info = battery_property;

        di->bat.name = "battery";
        di->bat.supplied_to = monitor_battery_supplied_to;
        di->bat.num_supplicants = ARRAY_SIZE(monitor_battery_supplied_to);
        di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
        di->bat.properties = monitor_battery_prop;
        di->bat.num_properties = ARRAY_SIZE(monitor_battery_prop);
        di->bat.get_property = monitor_battery_get_property;

        di->usb.name = "usb";
        di->usb.type = POWER_SUPPLY_TYPE_USB;
        di->usb.properties = monitor_usb_prop;
        di->usb.num_properties = ARRAY_SIZE(monitor_usb_prop);
        di->usb.get_property = monitor_usb_get_property;

        di->ac.name = "ac";
        di->ac.type = POWER_SUPPLY_TYPE_MAINS;
        di->ac.properties = monitor_charger_prop;
        di->ac.num_properties = ARRAY_SIZE(monitor_charger_prop);
        di->ac.get_property = monitor_charger_get_property;

        platform_set_drvdata(pdev, di);

        ret = power_supply_register(&pdev->dev, &di->bat);
        if(ret) {
                dev_err(&pdev->dev, "failed to register main battery\n");
                goto bat_failed;
        }
        ret = power_supply_register(&pdev->dev, &di->usb);
        if(ret) {
                dev_err(&pdev->dev, "failed to register usb power supply\n");
                goto usb_failed;
        }
        ret = power_supply_register(&pdev->dev, &di->ac);
        if(ret) {
                dev_err(&pdev->dev, "failed to register ac power supply\n");
                goto ac_failed;
        }

        if(di->battery_info->battery_exist) {
                di->bat_present = di->battery_info->battery_exist();
        } else {
                di->bat_present = 1;
        }
        di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
        di->charge_status  = POWER_SUPPLY_STATUS_DISCHARGING;
        di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
        di->bat_health     = POWER_SUPPLY_HEALTH_GOOD;
        di->bat_tech       = POWER_SUPPLY_TECHNOLOGY_LION;
        di->bat_temperature  = 20;

        di->bat_current_mA = 200;
        di->capacity = -1;
        di->volt_filter_count = 0;

        di->prev_capacity = -1;
        di->prev_temp_stage = -1;
        di->hand_flag = 1;
        di->dbb_hot_flag = 1;
        di->dbb_hot = THERMAL_TEMP_HOT;
        di->dbb_normal = THERMAL_TEMP_NORMAL;

        di->cap_valid_count = 0;
        di->cap_adjust_flag = -1;

#ifndef CONFIG_BATTERY_FG
        di->param.currentmA = 100;
        monitor_charger_current_reg_set(di);
        di->bat_voltage_mV = monitor_get_battery_voltage_init(di);
        di->param.currentmA = 500;
        monitor_charger_current_reg_set(di);
        curr_capacity = 0;
        i = 0;
        if (!comip_otg_hostmode()) {
                if(monitor_check_vbus_present(di)) {
                        di->charger_source = POWER_SUPPLY_TYPE_USB;
                }
        }
#else
        if(di->battery_info->battery_voltage)
                di->bat_voltage_mV = di->battery_info->battery_voltage();
        if(di->battery_info->battery_capacity)
                curr_capacity = di->battery_info->battery_capacity();
        for(i =0;i < 5; i++){
        di->rbuf_fgvolt[i] = di->bat_voltage_mV;
            di->capacity_filter[i] = curr_capacity;
        }
        if(di->battery_info->get_fgchip_detect)
                di->fgchip_type = di->battery_info->get_fgchip_detect();
        dev_info(&pdev->dev, "fgchip_type is %d\n",di->fgchip_type);
#endif
        dev_info(&pdev->dev, "Battery Voltage at Bootup is %d mV\n",
                 di->bat_voltage_mV);

        wake_lock_init(&di->usb_wakelock, WAKE_LOCK_SUSPEND, "vbus_wakelock");
        wake_lock_init(&di->monitor_wakelock, WAKE_LOCK_SUSPEND, "monitor_wakelock");
        alarm_init(&di->wakeup_alarm, ALARM_REALTIME, monitor_battery_alarm_restart);

        ret = sysfs_create_group(&pdev->dev.kobj, &monitor_battery_attr_group);
        if (ret) {
                dev_err(&pdev->dev, "could not create sysfs files\n");
                goto i2c_reg_failed;
        }

        INIT_DEFERRABLE_WORK(&di->battery_monitor_work,
                             monitor_battery_work);
        schedule_delayed_work(&di->battery_monitor_work, 0);

        di->dbb.notifier_call = monitor_dbb_temp_notifier;
        ret = thermal_notifier_register(&di->dbb);
        if (ret) {
                pr_err("thermal_notifier_register not register =%d\n",ret);
        }

        INIT_WORK(&di->usb_work, monitor_usb_charger_work);
        di->nb.notifier_call = monitor_usb_notifier_call;
        ret = comip_usb_register_notifier(&di->nb);
        if (ret)
                dev_err(&pdev->dev,"comip usb register notifier"
                        " failed %d\n", ret);
        plugin_stat = comip_get_plugin_device_status();
        if (!comip_otg_hostmode()) {
                vbus_state = monitor_check_vbus_present(di);
                if (vbus_state) {
                        if(plugin_stat == LCUSB_EVENT_VBUS) {
                            di->usb_online = 1;
                            di->event = LCUSB_EVENT_VBUS;
                        }else if(plugin_stat == LCUSB_EVENT_CHARGER){
                            di->ac_online = 1;
                            di->event = LCUSB_EVENT_CHARGER;
                        } else if (plugin_stat == LCUSB_EVENT_PLUGIN) {
                                di->plug_online = 1;
                                di->init_plugin = 1;
                                di->event = LCUSB_EVENT_PLUGIN;
                        }else{
                        }
                } else {
                        di->event = LCUSB_EVENT_NONE;
                        di->init_plugin = 0;
                }
        } else {
                /* power on init when otg event received*/
                di->event = LCUSB_EVENT_OTG;
        }
        schedule_work(&di->usb_work);

        ret = pmic_callback_register(PMIC_EVENT_BATTERY, di, monitor_charger_interrupt);
        if (ret) {
                dev_err(&pdev->dev,"Fail to register pmic event callback.\n");
                goto charger_interrupt_fail;
        }
        pmic_callback_unmask(PMIC_EVENT_BATTERY);

#ifdef CONFIG_HAS_EARLYSUSPEND
        di->early_suspend.level = 55;
        di->early_suspend.suspend = monitor_battery_early_suspend;
        di->early_suspend.resume = monitor_battery_early_resume;
        register_early_suspend(&di->early_suspend);
#endif
        monitor_battery_proc_init(di);
#ifdef CONFIG_BATTERY_FG
        /*fg firmware upload when bootup*/
        di->fg_update = 0;
        INIT_WORK(&di->fg_update_work, fuelgauge_update_work);
        schedule_work(&di->fg_update_work);
#endif
        pr_debug("monitor_battery:Initialized Monitor Battery module\n");
        return 0;

charger_interrupt_fail:
        sysfs_remove_group(&pdev->dev.kobj, &monitor_battery_attr_group);
i2c_reg_failed:
        alarm_cancel(&di->wakeup_alarm);
        wake_lock_destroy(&di->monitor_wakelock);
        wake_lock_destroy(&di->usb_wakelock);
        power_supply_unregister(&di->ac);
ac_failed:
        power_supply_unregister(&di->usb);
usb_failed:
        power_supply_unregister(&di->bat);
bat_failed:
        platform_set_drvdata(pdev, NULL);

        kfree(di);
        return ret;
}

static int __exit monitor_battery_remove(struct platform_device *pdev)
{
        struct monitor_battery_device_info *di = platform_get_drvdata(pdev);

        cancel_delayed_work_sync(&di->battery_monitor_work);

        comip_usb_unregister_notifier(&di->nb);
        thermal_notifier_unregister(&di->dbb);
        monitor_power_supply_unregister(di);
        pmic_callback_unregister(PMIC_EVENT_BATTERY);
        sysfs_remove_group(&pdev->dev.kobj, &monitor_battery_attr_group);
        alarm_cancel(&di->wakeup_alarm);
        wake_lock_destroy(&di->monitor_wakelock);
        wake_lock_destroy(&di->usb_wakelock);

        platform_set_drvdata(pdev, NULL);
        kfree(di);

        return 0;
}

#ifdef CONFIG_PM
static int monitor_battery_suspend(struct device *dev)
{
        struct platform_device *pdev = to_platform_device(dev);
        struct monitor_battery_device_info *di = platform_get_drvdata(pdev);

        cancel_delayed_work_sync(&di->battery_monitor_work);

        di->resume_flag = 0;
        if (di->charger_source == POWER_SUPPLY_TYPE_MAINS) {

                alarm_start(&di->wakeup_alarm,
                            ktime_add(ktime_get_real(), ktime_set(FAST_POLL, 0)));

        }

        printk(KERN_DEBUG "monitor_battery:%s:suspend \n", __func__);
        return 0;
}

static void monitor_battery_resume(struct device *dev)
{
        struct platform_device *pdev = to_platform_device(dev);
        struct monitor_battery_device_info *di = platform_get_drvdata(pdev);
        struct timespec ts;

        getnstimeofday(&ts);
        alarm_cancel(&di->wakeup_alarm);

        if(abs(ts.tv_sec - di->last_sec) >= FAST_POLL) {
                schedule_delayed_work(&di->battery_monitor_work, 0);
                di->last_sec = ts.tv_sec;
                di->resume_flag = 1;
        }

        printk(KERN_DEBUG "monitor_battery:%s:resume\n", __func__);
        return;
}
#else
#define monitor_battery_suspend NULL
#define monitor_battery_resume  NULL
#endif /* CONFIG_PM */

static void monitor_battery_shutdown(struct platform_device *pdev)
{
#if defined(CONFIG_OTG_GPIO_CTL)
    printk(KERN_DEBUG "monitor_battery:%s:shutdown\n", __func__);
    struct monitor_battery_device_info *di = platform_get_drvdata(pdev);
    monitor_start_otg(di,0);
#endif
    return;
}
static const struct dev_pm_ops pm_ops = {
        .prepare  = monitor_battery_suspend,
        .complete = monitor_battery_resume,
};

static struct platform_driver monitor_battery_driver = {
        .remove    = __exit_p(monitor_battery_remove),
        .shutdown = monitor_battery_shutdown,
        .driver    = {
                .name = "monitor_battery",
                .pm	= &pm_ops,
        },
};

static int __init monitor_battery_init(void)
{
        return platform_driver_probe(&monitor_battery_driver, monitor_battery_probe);
}
late_initcall_sync(monitor_battery_init);

static void __exit monitor_battery_exit(void)
{
        platform_driver_unregister(&monitor_battery_driver);
}
module_exit(monitor_battery_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:monitor_battery");
MODULE_AUTHOR("leadcore tech");
