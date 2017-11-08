/*
 * drivers/power/bq2415x_battery.c
 *
 * BQ24153 /.BQ24158 / BQ24156 battery charging driver
 *
 * Copyright (C) 2010-2015 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
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
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/sched.h>
#include <linux/alarmtimer.h>
#include <linux/gpio.h>
#include <plat/comip-pmic.h>
#include <plat/bq24158.h>
#include <plat/comip-battery.h>
#include <plat/mfp.h>
#ifdef CONFIG_BAT_ID_BQ2022A
#include <plat/bq2022a-batid.h>
#endif
#ifdef CONFIG_OTG_GPIO
#include <plat/lc1160-pmic.h>
#define DBB_OTG_ENABLE MFP_PIN_GPIO(101)
#define SWCHG_OTG_EN MFP_PIN_GPIO(160)
#endif
//#define BQ2415x_POLL_CHARGING


struct charge_params {
    unsigned long   currentmA;
    unsigned long   voltagemV;
    unsigned long   term_currentmA;
    unsigned long   enable_iterm;
    bool    enable;
};

struct bq2415x_device_info {
    struct device       *dev;
    struct i2c_client   *client;
    struct charge_params    params;
    struct delayed_work bq2415x_charger_work;
    struct delayed_work bq2415x_irq_work;
    struct extern_charger_platform_data *pdata;
    struct notifier_block    nb;

    unsigned short  status_reg;
    unsigned short  control_reg;
    unsigned short  voltage_reg;
    unsigned int  bqchip_version;
    unsigned short  current_reg;
    unsigned short  special_charger_reg;
    unsigned int  chip_detect;
    unsigned int    cin_limit;
    unsigned int    currentmA;
    unsigned int    voltagemV;
    unsigned int    max_currentmA;
    unsigned int    max_voltagemV;
    unsigned int    term_currentmA;
    unsigned int    dpm_voltagemV;
    unsigned int    ac_cin_currentmA;
    unsigned int    ac_bat_currentmA;
    unsigned int    usb_cin_currentmA;
    unsigned int    usb_bat_currentmA;

    int    timer_fault;

    bool    cfg_params;
    bool    enable_iterm;
    bool    active;
    bool    enable_charger;
    bool    charger_flag;
    bool    hz_mode;
    bool    low_chg;
    bool    bat_id_failed;
    bool    bat_present;
    int     bat_capacity;
    int    charger_source;
    int    charge_status;
    int    charger_health;
    int    battery_temp_stage;
    struct wake_lock chrg_lock;

    bool boost_mode;
    bool opa_mode;
    bool otg_en;
};

struct bq2415x_device_info  *bq;

static DEFINE_MUTEX(bq2415x_i2c_mutex);
static int chip_detect_flag = 0;

static int bq2415x_write_block(struct bq2415x_device_info *di, u8 *value,
                    u8 reg, unsigned num_bytes)
{
    struct i2c_msg msg[1];
    int ret;

    if (!di->client->adapter)
        return -ENODEV;

    *value  = reg;

    msg[0].addr = di->client->addr;
    msg[0].flags = 0;
    msg[0].buf  = value;
    msg[0].len  = num_bytes + 1;

    mutex_lock(&bq2415x_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&bq2415x_i2c_mutex);

    /* i2c_transfer returns number of messages transferred */
    if (ret != 1) {
        dev_err(di->dev,
            "i2c_write failed to transfer all messages reg=0x%x,%d\n",reg,ret);
        if (ret < 0)
            return ret;
        else
            return -EIO;
    } else {
        return 0;
    }
}

static int bq2415x_read_block(struct bq2415x_device_info *di, u8 *value,
                    u8 reg, unsigned num_bytes)
{
    struct i2c_msg msg[2];
    u8 buf;
    int ret;

    if (!di->client->adapter)
        return -ENODEV;

    buf = reg;

    msg[0].addr = di->client->addr;
    msg[0].flags = 0;
    msg[0].buf  = &buf;
    msg[0].len  = 1;
    msg[1].addr = di->client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf = value;
    msg[1].len  = num_bytes;

    mutex_lock(&bq2415x_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&bq2415x_i2c_mutex);

    /* i2c_transfer returns number of messages transferred */
    if (ret != 2) {
        dev_err(di->dev,
            "i2c_read failed to transfer all messages reg=0x%x,%d\n",reg,ret);
        if (ret < 0)
            return ret;
        else
            return -EIO;
    }else {
        return 0;
    }
}

static int bq2415x_write_byte(struct bq2415x_device_info *di, u8 value, u8 reg)
{
#if 0
    struct i2c_msg msg[1];
    u8 data[2];
    int ret;

    if (!di->client->adapter)
        return -ENODEV;

    data[0] = reg;
    data[1] = value;

    msg[0].addr = di->client->addr;
    msg[0].flags = 0;
    msg[0].buf = data;
    msg[0].len = ARRAY_SIZE(data);

    mutex_lock(&bq2415x_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&bq2415x_i2c_mutex);

    /* i2c_transfer returns number of messages transferred */
    if (ret != 1) {
        dev_err(di->dev,
            "i2c_write failed to transfer all messages reg=0x%x,%d\n",reg,ret);
        if (ret < 0)
            return ret;
        else
            return -EIO;
    } else {
        return 0;
    }

#else
    u8 temp_buffer[2] = { 0 };

    temp_buffer[1] = value;
    return bq2415x_write_block(di, temp_buffer, reg, 1);
#endif
}

static int bq2415x_read_byte(struct bq2415x_device_info *di, u8 *value, u8 reg)
{
#if 0
    struct i2c_msg msg[2];
    u8 buf;
    int ret;

    if (!di->client->adapter)
        return -ENODEV;

    buf = reg;

    msg[0].addr = di->client->addr;
    msg[0].flags = 0;
    msg[0].buf  = &buf;
    msg[0].len  = 1;
    msg[1].addr = di->client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf = value;
    msg[1].len  = sizeof(*value);

    mutex_lock(&bq2415x_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&bq2415x_i2c_mutex);

    /* i2c_transfer returns number of messages transferred */
    if (ret != 2) {
        dev_err(di->dev,
            "i2c_read failed to transfer all messages reg=0x%x,%d\n",reg,ret);
        if (ret < 0)
            return ret;
        else
            return -EIO;
    }else {
        return 0;
    }
#else
    return bq2415x_read_block(di, value, reg, 1);
#endif
}

#ifdef CONFIG_BAT_ID_BQ2022A
static void comip_bat_id_identify(struct bq2415x_device_info *di)
{
    int id_fail = 0;

    id_fail = bq2022a_read_bat_id();

    if (id_fail < 0) {
        di->bat_id_failed = 1;/*id identified err*/
    } else {
        di->bat_id_failed = 0;
    }

    dev_info(di->dev, "read bat_id is %s, charging function is %s\n", id_fail ? "failed" : "passed", id_fail ? "off" : "on");
}
#endif

static void bq2415x_config_status_reg(struct bq2415x_device_info *di)
{
    di->status_reg = (TIMER_RST | ENABLE_STAT_PIN);
    bq2415x_write_byte(di, di->status_reg, REG_STATUS_CONTROL);
    return;
}

static void bq2415x_config_control_reg(struct bq2415x_device_info *di)
{
    u8 Iin_limit = 0;

    if (di->cin_limit <= 100)
        Iin_limit = 0;
    else if (di->cin_limit > 100 && di->cin_limit <= 500)
        Iin_limit = 1;
    else if (di->cin_limit > 500 && di->cin_limit <= 800)
        Iin_limit = 2;
    else
        Iin_limit = 3;

    di->control_reg = ((Iin_limit << INPUT_CURRENT_LIMIT_SHIFT)
                | (di->enable_iterm << ENABLE_ITERM_SHIFT)
                | (di->enable_charger << ENABLE_CHARGER_SHIFT)
                | (di->hz_mode << 1)|di->opa_mode);
    bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
    return;
}

static void bq2415x_config_voltage_reg(struct bq2415x_device_info *di)
{
    unsigned int voltagemV = 0;
    u8 Voreg = 0;

    voltagemV = di->voltagemV;
    if (voltagemV < 3500)
        voltagemV = 3500;
    else if (voltagemV > 4440)
        voltagemV = 4440;

    Voreg = (voltagemV - 3500)/20;
    di->voltage_reg = (Voreg << VOLTAGE_SHIFT)|(OTG_PL_EN << 1)|di->otg_en;
    bq2415x_write_byte(di, di->voltage_reg, REG_BATTERY_VOLTAGE);
    return;
}

static void bq2415x_config_current_reg(struct bq2415x_device_info *di)
{
    unsigned int currentmA = 0;
    unsigned int term_currentmA = 0;
    u8 Vichrg = 0;
    u8 shift = 0;
    u8 Viterm = 0;

    currentmA = di->currentmA;
    term_currentmA = di->term_currentmA;

    if (currentmA < 550)
        currentmA = 550;

    if ((di->bqchip_version & (BQ24153 | BQ24158 | HL7005))) {
        shift = BQ24153_CURRENT_SHIFT;
        if (currentmA > 1250)
            currentmA = 1250;
    }
    if (di->bqchip_version & FAN54015) {
        shift = BQ24153_CURRENT_SHIFT;
        if (currentmA > 1450)
            currentmA = 1450;
    }
    if ((di->bqchip_version & BQ24156)) {
        shift = BQ24156_CURRENT_SHIFT;
        if (currentmA > 1550)
        currentmA = 1550;
    }
    if (term_currentmA < 50)
        term_currentmA = 50;

    if (term_currentmA > 400)
        term_currentmA = 400;

    Vichrg = (currentmA - 550)/100;
    Viterm = (term_currentmA - 50)/50;
    if (di->bqchip_version & FAN54015) {
        if(currentmA < 950){
            Vichrg = (currentmA - 550)/100;
        }else{
            if((currentmA >= 950)&&(currentmA < 1150)){
                currentmA = 1050;
                Vichrg = (currentmA - 1050)/100 + 4;
            }else if((currentmA >= 1150)&&(currentmA < 1250)){
                currentmA = 1150;
                Vichrg = (currentmA - 1050)/100 + 4;
            }else if((currentmA >= 1250)&&(currentmA < 1450)){
                currentmA = 1350;
                Vichrg = (currentmA - 1050)/100 + 3;
            }else{
                currentmA = 1450;
                Vichrg = (currentmA - 1050)/100 + 3;
            }
        }
    }
    di->current_reg = ((Vichrg << shift | Viterm) & 0x7F);
    bq2415x_write_byte(di, di->current_reg, REG_BATTERY_CURRENT);
    return;
}

static void bq2415x_config_special_charger_reg(struct bq2415x_device_info *di)
{
    u8 Vsreg = 2; /* 160/80 */
    int dpmvoltagemV = 0;

    dpmvoltagemV = di->dpm_voltagemV;
    if(dpmvoltagemV < 4200){
        dpmvoltagemV = 4200;
    }
    if(dpmvoltagemV > 4760){
        dpmvoltagemV = 4760;
    }
    Vsreg = (dpmvoltagemV - 4200)/80;

    if(di->chip_detect & (FAN5400 | FAN5401 | FAN5402)){
        return;
    }
    di->special_charger_reg = (di->low_chg << LOW_CHG_SHIFT)|Vsreg;
    bq2415x_write_byte(di, di->special_charger_reg,
                REG_SPECIAL_CHARGER_VOLTAGE);
    return;
}

static void bq2415x_config_safety_reg(struct bq2415x_device_info *di,
                unsigned int max_currentmA,
                unsigned int max_voltagemV)
{
    u8 Vmchrg = 0;
    u8 Vmreg = 0;
    u8 limit_reg = 0;

    if (max_currentmA < 550)
        max_currentmA = 550;
    else if (max_currentmA > 1550)
        max_currentmA = 1550;

    if (di->bqchip_version & FAN54015) {
        if (max_currentmA > 1450)
            max_currentmA = 1450;
    }
    if (max_voltagemV < 4200)
        max_voltagemV = 4200;
    else if (max_voltagemV > 4440)
        max_voltagemV = 4440;

    di->max_voltagemV = max_voltagemV;
    di->max_currentmA = max_currentmA;
    //di->voltagemV = max_voltagemV;
    //di->currentmA = max_currentmA;

    Vmchrg = (max_currentmA - 550)/100;
    Vmreg = (max_voltagemV - 4200)/20;

    if (di->bqchip_version & FAN54015) {
        if(max_currentmA < 950){
            Vmchrg = (max_currentmA - 550)/100;
        }else{
            if((max_currentmA >= 950)&&(max_currentmA < 1150)){
                max_currentmA = 1050;
                Vmchrg = (max_currentmA - 1050)/100 + 4;
            }else if((max_currentmA >= 1150)&&(max_currentmA < 1250)){
                max_currentmA = 1150;
                Vmchrg = (max_currentmA - 1050)/100 + 4;
            }else if((max_currentmA >= 1250)&&(max_currentmA < 1450)){
                max_currentmA = 1350;
                Vmchrg = (max_currentmA - 1050)/100 + 3;
            }else{
                max_currentmA = 1450;
                Vmchrg = (max_currentmA - 1050)/100 + 3;
            }

        }
    }

    if(di->chip_detect & (FAN5400 | FAN5401 | FAN5402)){
        return;
    }

    limit_reg = ((Vmchrg << MAX_CURRENT_SHIFT) | Vmreg);
    bq2415x_write_byte(di, limit_reg, REG_SAFETY_LIMIT);
    return;
}

static void bq2415x_start_usb_charging(struct bq2415x_device_info *di)
{
    u8 read_reg[2] = {0};

    di->charger_source = POWER_SUPPLY_TYPE_USB;
    di->charge_status =  POWER_SUPPLY_STATUS_CHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
    di->cin_limit = di->usb_cin_currentmA;
    di->currentmA = di->usb_bat_currentmA;
    di->voltagemV =  di->max_voltagemV;
    di->enable_iterm = 1;
    di->enable_charger = ENABLE_CHARGER;
    di->charger_flag = ENABLE_CHARGER;
    di->battery_temp_stage = -20;
    di->low_chg = LOW_CHG_DIS;
    di->boost_mode = CHARGE_MODE;
    di->opa_mode = CHARGE_MODE;
    di->otg_en = OTG_DIS;
    di->hz_mode = 0;
    bq2415x_config_control_reg(di);
    bq2415x_config_voltage_reg(di);
    bq2415x_config_current_reg(di);
    bq2415x_config_special_charger_reg(di);
    msleep(400);
    bq2415x_config_status_reg(di);
    if(di->chip_detect != 0){
        bq2415x_read_byte(di, &read_reg[1], 0x10);
    }
    dev_info(di->dev, "%s\n"
            "battery current = %d mA\n"
            "battery voltage = %d mV\n"
            "cin_limit_current = %d mA,reg[0x10] = 0x%02x\n"
            , __func__, di->currentmA, di->voltagemV,di->cin_limit,read_reg[1]);
    bq2415x_read_byte(di, &read_reg[0], REG_STATUS_CONTROL);
    if(((read_reg[0] & 0x07) == 0x01)||((read_reg[0] & 0x07) == 0x03)||(!di->bat_present)){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }
    if(di->active == 0){
        schedule_delayed_work(&di->bq2415x_charger_work,
                    msecs_to_jiffies(0));
        di->active = 1;
    }
#if defined(CONFIG_BAT_ID_BQ2022A)
    if(di->bat_id_failed){
        comip_bat_id_identify(di);
    }
#endif
    //wake_lock(&di->chrg_lock);
    return;
}

static void bq2415x_start_ac_charging(struct bq2415x_device_info *di)
{
    u8 read_reg[2] = {0};

    di->charger_source = POWER_SUPPLY_TYPE_MAINS;
    di->charge_status =  POWER_SUPPLY_STATUS_CHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
    di->cin_limit = di->ac_cin_currentmA;
    di->currentmA = di->ac_bat_currentmA;
    di->voltagemV =  di->max_voltagemV;
    di->enable_iterm = 1;
    di->enable_charger = ENABLE_CHARGER;
    di->charger_flag = ENABLE_CHARGER;
    di->battery_temp_stage = -20;
    di->low_chg = LOW_CHG_DIS;
    di->boost_mode = CHARGE_MODE;
    di->opa_mode = CHARGE_MODE;
    di->otg_en = OTG_DIS;
    di->hz_mode = 0;
    bq2415x_config_control_reg(di);
    bq2415x_config_voltage_reg(di);
    bq2415x_config_current_reg(di);
    bq2415x_config_special_charger_reg(di);
    msleep(400);
    bq2415x_config_status_reg(di);
    if(di->chip_detect != 0){
        bq2415x_read_byte(di, &read_reg[1], 0x10);
    }
    dev_info(di->dev, "%s\n"
            "battery current = %d mA\n"
            "battery voltage = %d mV\n"
            "cin_limit_current = %d mA,reg[0x10] = 0x%02x\n"
            , __func__, di->currentmA, di->voltagemV,di->cin_limit,read_reg[1]);
    bq2415x_read_byte(di, &read_reg[0], REG_STATUS_CONTROL);
    if(((read_reg[0] & 0x07) == 0x01)||((read_reg[0] & 0x07) == 0x03)||(!di->bat_present)){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }
    if(di->active == 0){
        schedule_delayed_work(&di->bq2415x_charger_work,
                msecs_to_jiffies(0));
        di->active = 1;
    }
#if defined(CONFIG_BAT_ID_BQ2022A)
    if(di->bat_id_failed){
        comip_bat_id_identify(di);
    }
#endif
    //wake_lock(&di->chrg_lock);
    return;
}

static void bq2415x_stop_charging(struct bq2415x_device_info *di)
{
    dev_info(di->dev,"bq2415x_stop_charging!\n");

    di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
    di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
    cancel_delayed_work_sync(&di->bq2415x_charger_work);
    di->boost_mode = CHARGE_MODE;
    di->opa_mode = CHARGE_MODE;
    di->otg_en = OTG_DIS;
    di->active = 0;
    di->charger_flag = ENABLE_CHARGER;
    bq2415x_config_status_reg(di);
    bq2415x_config_control_reg(di);
    bq2415x_config_voltage_reg(di);
    wake_unlock(&di->chrg_lock);
    return;
}

int extern_charger_otg_start(bool enable)
{
    struct bq2415x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return -EINVAL;
    }
#ifdef CONFIG_OTG_GPIO
    comip_mfp_config(DBB_OTG_ENABLE, MFP_PIN_MODE_GPIO);
    comip_mfp_config(SWCHG_OTG_EN, MFP_PIN_MODE_GPIO);
    gpio_request(DBB_OTG_ENABLE, "otg en");
    gpio_request(SWCHG_OTG_EN, "swchg otg en");
#endif
    if(enable){
        dev_info(di->dev,"bq2415x start otg power on!\n");
        di->boost_mode = BOOST_MODE;
        di->opa_mode = BOOST_MODE;
        di->otg_en = OTG_EN;
        bq2415x_config_control_reg(di);
        bq2415x_config_voltage_reg(di);
        if(di->boost_mode == BOOST_MODE){
            schedule_delayed_work(&di->bq2415x_charger_work,
                    msecs_to_jiffies(200));
        }
#ifdef CONFIG_OTG_GPIO
        gpio_direction_output(DBB_OTG_ENABLE, 1);
	gpio_direction_output(SWCHG_OTG_EN, 1);
	gpio_free(SWCHG_OTG_EN);
	gpio_free(DBB_OTG_ENABLE);
	pmic_reg_write(LC1160_REG_QPVBUS, 0x11);
#endif
    }else{
#ifdef CONFIG_OTG_GPIO
        pmic_reg_write(LC1160_REG_QPVBUS, 0x00);
        gpio_direction_output(SWCHG_OTG_EN, 0);
        gpio_direction_output(DBB_OTG_ENABLE, 0);
        gpio_free(SWCHG_OTG_EN);
        gpio_free(DBB_OTG_ENABLE);
#endif
        bq2415x_stop_charging(di);
        di->boost_mode = CHARGE_MODE;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(extern_charger_otg_start);



static void
bq2415x_charger_update_status(struct bq2415x_device_info *di)
{
    u8 read_reg[20] = {0};
    int i = 0;

    di->timer_fault = 0;
    //bq2415x_read_block(di, &read_reg[0], 0, 7);
    for(i=0;i< 7;i++){
        bq2415x_read_byte(di, &read_reg[i], i);
    }
    if(di->chip_detect != 0){
        bq2415x_read_byte(di, &read_reg[7], 0x10);
    }

    if(di->boost_mode == CHARGE_MODE){/*charge mode*/
        if((di->bat_capacity == 5)||(di->bat_capacity == 40)||(di->bat_capacity == 85)){
            dev_err(di->dev, "dump_reg[0->6,0x10] = %2x, %2x, %2x, %2x, %2x, %2x, %2x, %2x\n",
            read_reg[0],read_reg[1],read_reg[2],read_reg[3],read_reg[4],read_reg[5],read_reg[6],read_reg[7]);
        }
        if((((read_reg[2] & 0xFC)>>2)*20 + 3500) < 3900){
            di->cfg_params = 1;
            dev_err(di->dev, "voltage register err = 0x%2x\n",read_reg[2]);
        }
        if (((read_reg[0] & 0x30) == 0x20)&&(di->voltagemV == di->max_voltagemV)){
            if(di->bat_capacity < 98){
                di->hz_mode = 1;
                bq2415x_config_control_reg(di);
                msleep(500);
                di->hz_mode = 0;
                bq2415x_config_control_reg(di);
                dev_info(di->dev, "charger done and trigger recharge again =0x%x(%d)\n",
                            read_reg[0],di->bat_capacity);
            }
            dev_dbg(di->dev, "CHARGE DONE\n");
        }

        if(di->charger_source != POWER_SUPPLY_TYPE_BATTERY){
            if (((read_reg[0] & 0x37) == 0x00)&&(di->bat_capacity < 70)){
                if((di->enable_charger == ENABLE_CHARGER)&&(di->hz_mode != 1)){
                dev_err(di->dev, "charger ready[0-6-0x10] = 0x%2x,0x%2x,0x%2x,0x%2x,0x%2x,0x%2x,0x%2x,0x%2x\n",
                read_reg[0],read_reg[1],read_reg[2],read_reg[3],read_reg[4],read_reg[5],read_reg[6],read_reg[7]);
                    di->hz_mode = 1;
                    bq2415x_config_control_reg(di);
                    msleep(500);
                    di->hz_mode = 0;
                    bq2415x_config_control_reg(di);
                }
            }
        }

        if ((read_reg[0] & 0x07) == 0x01){
            dev_dbg(di->dev, "CHARGE OVP\n");
            di->charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        }else{
            di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
        }

        if ((read_reg[0] & 0x7) == 0x6)
            di->timer_fault = 1;

        if ((read_reg[0] & 0x7) == 0x4){
            di->hz_mode = 1;
            bq2415x_config_control_reg(di);
            bq2415x_config_voltage_reg(di);
            msleep(500);
            di->hz_mode = 0;
            bq2415x_config_control_reg(di);
            dev_err(di->dev, "CHARGER BATYERY OVP = 0x%2x,reg1 = 0x%2x\n",read_reg[0],read_reg[1]);
        }
        if ((read_reg[0] & 0x7)||((read_reg[0] & 0x30)==0x30)) {
            di->cfg_params = 1;
            dev_err(di->dev, "CHARGER STATUS[0-6-0x10] = 0x%2x,0x%2x,0x%2x,0x%2x,0x%2x,0x%2x,0x%2x,0x%2x\n",
                read_reg[0],read_reg[1],read_reg[2],read_reg[3],read_reg[4],read_reg[5],read_reg[6],read_reg[7]);
        }

        if ((di->timer_fault == 1) || (di->cfg_params == 1)) {
            di->hz_mode = 0;
            bq2415x_config_control_reg(di);
            bq2415x_config_voltage_reg(di);
            bq2415x_write_byte(di, di->current_reg, REG_BATTERY_CURRENT);
            bq2415x_config_special_charger_reg(di);
            di->cfg_params = 0;
        }
    }else{ // otg mode
        if ((read_reg[0] & 0x08) == 0x00){
            dev_dbg(di->dev, "bq2415x is not in boost\n");
        }
        if ((read_reg[0] & 0x01) == 0x01){
            dev_err(di->dev, "OTG VBUS OVP\n");
        }
        if (read_reg[0] & 0x7) {
            di->cfg_params = 1;
            dev_err(di->dev, "boost status %x\n", read_reg[0]);
        }
        if (di->cfg_params == 1) {
            bq2415x_write_byte(di, di->control_reg, REG_CONTROL_REGISTER);
            bq2415x_write_byte(di, di->voltage_reg, REG_BATTERY_VOLTAGE);
            di->cfg_params = 0;
        }
    }
    return;
}

static void bq2415x_charger_work(struct work_struct *work)
{
    struct bq2415x_device_info *di = container_of(work,
        struct bq2415x_device_info, bq2415x_charger_work.work);

    bq2415x_charger_update_status(di);

    /* reset 32 second timer */
    bq2415x_config_status_reg(di);

    if(di->boost_mode == CHARGE_MODE){/*charge mode*/
        schedule_delayed_work(&di->bq2415x_charger_work,
                  msecs_to_jiffies(BQ2415x_WATCHDOG_TIMEOUT));
    }else{
        schedule_delayed_work(&di->bq2415x_charger_work,msecs_to_jiffies(5000));
    }
}

static void bq2415x_irq_work(struct work_struct *work)
{
    struct bq2415x_device_info *di = container_of(work,
        struct bq2415x_device_info, bq2415x_irq_work.work);

    bq2415x_charger_update_status(di);
    /* reset 32 second timer */
    bq2415x_config_status_reg(di);

    return;
}

static irqreturn_t bq2415x_charger_irq(int irq, void *_di)
{
    struct bq2415x_device_info *di = _di;

    schedule_delayed_work(&di->bq2415x_irq_work, 0);

    return IRQ_HANDLED;
}

static int bq2415x_init_irq(struct bq2415x_device_info *di)
{
    int ret = 0;

#ifdef BQ2415x_POLL_CHARGING
    return 0;
#endif

    INIT_DELAYED_WORK(&di->bq2415x_irq_work,
        bq2415x_irq_work);

    if (gpio_request(di->client->irq, "bq2415x_irq") < 0){
        ret = -ENOMEM;
        goto irq_fail;
    }

    gpio_direction_input(di->client->irq);

    /* request irq interruption */
    ret = request_irq(gpio_to_irq(di->client->irq), bq2415x_charger_irq,
        IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING| IRQF_SHARED,"bq24158_irq",di);
    if(ret){
        dev_err(&di->client->dev,"request irq failed %d, status %d\n",
                gpio_to_irq(di->client->irq), ret);
        goto irq_fail;
    }else{
        enable_irq_wake(gpio_to_irq(di->client->irq));
    }

    return 0;
irq_fail:
    free_irq(gpio_to_irq(di->client->irq), di);
    return ret;
}

static ssize_t bq2415x_set_hz_mode(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->hz_mode = val;
    bq2415x_config_control_reg(di);

    return count;
}

static ssize_t bq2415x_show_hz_mode(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    val = di->hz_mode;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_enable_itermination(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->enable_iterm = val;
    bq2415x_config_control_reg(di);

    return count;
}

static ssize_t bq2415x_show_enable_itermination(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    val = di->enable_iterm;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->enable_charger= 1 ^ val;
    di->charger_flag = 1 ^ val;
    bq2415x_config_control_reg(di);

    return count;
}

static ssize_t bq2415x_show_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    val = di->enable_charger;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 100) ||
        (val > 1250))
        return -EINVAL;

    di->cin_limit = val;
    bq2415x_config_control_reg(di);

    return count;
}

static ssize_t bq2415x_show_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    val = di->cin_limit;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 3500) ||
        (val > 4440))
        return -EINVAL;

    di->voltagemV = val;
    bq2415x_config_voltage_reg(di);

    return count;
}

static ssize_t bq2415x_show_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    val = di->voltagemV;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 550) ||
        (val > 1250))
        return -EINVAL;

    di->currentmA = val;
    bq2415x_config_current_reg(di);

    return count;
}

static ssize_t bq2415x_show_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    val = di->currentmA;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 350))
        return -EINVAL;

    di->term_currentmA = val;
    bq2415x_config_current_reg(di);

    return count;
}

static ssize_t bq2415x_show_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    val = di->term_currentmA;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2415x_set_charging(struct device *dev, struct device_attribute *attr,
                       const char *buf, size_t count)
{
    int status = count;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);

    if (strncmp(buf, "startac", 7) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_USB)
            bq2415x_stop_charging(di);
        bq2415x_start_ac_charging(di);
    } else if (strncmp(buf, "startusb", 8) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
            bq2415x_stop_charging(di);
        bq2415x_start_usb_charging(di);
    } else if (strncmp(buf, "stop" , 4) == 0) {
        bq2415x_stop_charging(di);
    } else {
        return -EINVAL;
    }

    return status;
}

static ssize_t bq2415x_show_chargelog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    //unsigned long val;
    struct bq2415x_device_info *di = dev_get_drvdata(dev);
    u8 read_reg[20] = {0};
    int i =0;
    for(i=0;i<7;i++){
        bq2415x_read_byte(di, &read_reg[i], i);
    }
    if(di->chip_detect != 0){
        bq2415x_read_byte(di, &read_reg[7], 0x10);
    }
    return sprintf(buf, "%6x  %6x  %6x  %6x  %6x  %6x  %6x  %6x  \n",
    read_reg[0],read_reg[1],read_reg[2],read_reg[3],read_reg[4],
    read_reg[5],read_reg[6],read_reg[7]);
}

static DEVICE_ATTR(hz_mode, S_IWUSR | S_IRUGO,
                bq2415x_show_hz_mode,
                bq2415x_set_hz_mode);
static DEVICE_ATTR(enable_itermination, S_IWUSR | S_IRUGO,
                bq2415x_show_enable_itermination,
                bq2415x_set_enable_itermination);
static DEVICE_ATTR(enable_charger, S_IWUSR | S_IRUGO,
                bq2415x_show_enable_charger,
                bq2415x_set_enable_charger);
static DEVICE_ATTR(cin_limit, S_IWUSR | S_IRUGO,
                bq2415x_show_cin_limit,
                bq2415x_set_cin_limit);
static DEVICE_ATTR(regulation_voltage, S_IWUSR | S_IRUGO,
                bq2415x_show_regulation_voltage,
                bq2415x_set_regulation_voltage);
static DEVICE_ATTR(charge_current, S_IWUSR | S_IRUGO,
                bq2415x_show_charge_current,
                bq2415x_set_charge_current);
static DEVICE_ATTR(termination_current, S_IWUSR | S_IRUGO,
                bq2415x_show_termination_current,
                bq2415x_set_termination_current);

static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO, NULL, bq2415x_set_charging);
static DEVICE_ATTR(chargelog, S_IWUSR | S_IRUGO, bq2415x_show_chargelog, NULL);

static struct attribute *bq2415x_attributes[] = {
    &dev_attr_hz_mode.attr,
    &dev_attr_enable_itermination.attr,
    &dev_attr_enable_charger.attr,
    &dev_attr_cin_limit.attr,
    &dev_attr_regulation_voltage.attr,
    &dev_attr_charge_current.attr,
    &dev_attr_termination_current.attr,
    &dev_attr_charging.attr,
    &dev_attr_chargelog.attr,
    NULL,
};

static const struct attribute_group bq2415x_attr_group = {
    .attrs = bq2415x_attributes,
};

static int bq2415x_set_charging_enable(bool enable)
{
    struct bq2415x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return -EINVAL;
    }

    if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY)||(!di->bat_present))
        return 0;

    if(enable){
        di->enable_charger= ENABLE_CHARGER;
    }else{
        di->enable_charger= DISABLE_CHARGER;
    }
	di->enable_charger = di->enable_charger|di->charger_flag|di->bat_id_failed ;
    bq2415x_config_control_reg(di);

    return 0;
}

static int bq2415x_set_battery_capacity(int soc)
{
    struct bq2415x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return -EINVAL;
    }
    if(soc < 0){
        soc = 100;
    }
    di->bat_capacity = soc;

    return(0);
}

static int bq2415x_set_battery_exist(bool battery_exist)
{
    struct bq2415x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return -EINVAL;
    }
    di->bat_present = battery_exist;
    if(!di->bat_present){
        dev_err(di->dev, "battery is not present! \n");
    }
    return(0);
}

static int bq2415x_set_charging_source(int charging_source)
{
    struct bq2415x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return -EINVAL;
    }
    switch(charging_source){
        case LCUSB_EVENT_PLUGIN:
            di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
            bq2415x_config_voltage_reg(di);
            bq2415x_config_status_reg(di);
            break;

        case LCUSB_EVENT_VBUS:
            bq2415x_start_usb_charging(di);
            break;

        case LCUSB_EVENT_CHARGER:
            bq2415x_start_ac_charging(di);
            break;

        case LCUSB_EVENT_NONE:
            bq2415x_stop_charging(di);
            break;

        case LCUSB_EVENT_OTG:
            extern_charger_otg_start(1);
            break;
        default:
            break;
    }
    return 0;
}

/*monitor battery temp limit chargingm;1=enable,0=disable*/
static int bq2415x_set_charging_temp_stage(int temp_status,int volt,bool enable)
{
    struct bq2415x_device_info *di = bq;
    u8 read_reg[7] = {0};

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return -EINVAL;
    }

    if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY)){
        di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
        return 0;
    }
    if(enable){
        if(di->pdata->feature == 1){/*for xiaomi*/
          if(di->charger_source == POWER_SUPPLY_TYPE_USB){
            switch(temp_status){
                case TEMP_STAGE_OVERCOLD:
                case TEMP_STAGE_COLD_ZERO:
                    di->voltagemV = di->max_voltagemV;
                    di->enable_charger = DISABLE_CHARGER;
                    break;
                case TEMP_STAGE_ZERO_COOL:
                    di->voltagemV = 4100;
                    di->enable_charger = ENABLE_CHARGER;
                    break;
                case TEMP_STAGE_COOL_NORMAL:
                    di->voltagemV = di->max_voltagemV;
                    di->enable_charger = ENABLE_CHARGER;
                    break;
                case TEMP_STAGE_NORMAL_HOT:
                    di->voltagemV = 4000;
                    di->enable_charger = ENABLE_CHARGER;
                    break;
                case TEMP_STAGE_HOT:
                    di->enable_charger = DISABLE_CHARGER;
                    break;
                default:
                    break;
            }
          }else if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
            switch(temp_status){
                case TEMP_STAGE_OVERCOLD:
                case TEMP_STAGE_COLD_ZERO:
                    di->voltagemV = di->max_voltagemV;
                    di->enable_charger = DISABLE_CHARGER;
                    break;
                case TEMP_STAGE_ZERO_COOL:
                    di->voltagemV = 4100;
                    di->enable_charger = ENABLE_CHARGER;
                    di->cin_limit = di->ac_cin_currentmA;
                    di->currentmA = di->usb_bat_currentmA;
                    break;
                case TEMP_STAGE_COOL_NORMAL:
                    di->voltagemV = di->max_voltagemV;
                    di->enable_charger = ENABLE_CHARGER;
                    di->cin_limit = di->ac_cin_currentmA;
                    di->currentmA = di->ac_bat_currentmA;
                    break;
                case TEMP_STAGE_NORMAL_HOT:
                    di->voltagemV = 4000;
                    di->enable_charger = ENABLE_CHARGER;
                    di->cin_limit = di->usb_cin_currentmA;
                    di->currentmA = di->usb_bat_currentmA;
                    break;
                case TEMP_STAGE_HOT:
                    di->enable_charger = DISABLE_CHARGER;
                    di->cin_limit = di->usb_cin_currentmA;
                    di->currentmA = di->usb_bat_currentmA;
                    break;
                default:
                    break;
            }
          }
        }else{
          if(di->charger_source == POWER_SUPPLY_TYPE_USB){
            switch(temp_status){
                case TEMP_STAGE_OVERCOLD:
                case TEMP_STAGE_COLD_ZERO:
                    di->enable_charger = DISABLE_CHARGER;
                    break;
                case TEMP_STAGE_ZERO_COOL:
                    if(volt > 4100){
                        di->enable_charger = DISABLE_CHARGER;
                    }else if (volt < 4000){
                        di->enable_charger = ENABLE_CHARGER;
                    }else{
                        di->enable_charger = di->enable_charger;
                    }
                    bq2415x_config_control_reg(di);
                    break;
                case TEMP_STAGE_COOL_NORMAL:
                    di->enable_charger = ENABLE_CHARGER;
                    break;
                case TEMP_STAGE_NORMAL_HOT:
                    if(volt > 4100){
                        di->enable_charger = DISABLE_CHARGER;
                    }else if (volt < 4000){
                        di->enable_charger = ENABLE_CHARGER;
                    }else{
                        di->enable_charger = di->enable_charger;
                    }
                    bq2415x_config_control_reg(di);
                    break;
                case TEMP_STAGE_HOT:
                    di->enable_charger = DISABLE_CHARGER;
                    break;
                default:
                    break;
            }
          }else if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
            switch(temp_status){
                case TEMP_STAGE_OVERCOLD:
                case TEMP_STAGE_COLD_ZERO:
                    di->enable_charger = DISABLE_CHARGER;
                    break;
                case TEMP_STAGE_ZERO_COOL:
                    if(volt > 4100){
                        di->enable_charger = DISABLE_CHARGER;
                    }else if (volt < 4000){
                        di->enable_charger = ENABLE_CHARGER;
                    }else{
                        di->enable_charger = di->enable_charger;
                    }
                    di->cin_limit = di->ac_cin_currentmA;
                    di->currentmA = di->usb_bat_currentmA;
                    break;
                case TEMP_STAGE_COOL_NORMAL:
                    di->enable_charger = ENABLE_CHARGER;
                    di->cin_limit = di->ac_cin_currentmA;
                    di->currentmA = di->ac_bat_currentmA;
                    break;
                case TEMP_STAGE_NORMAL_HOT:
                    if(volt > 4100){
                        di->enable_charger = DISABLE_CHARGER;
                    }else if (volt < 4000){
                        di->enable_charger = ENABLE_CHARGER;
                    }else{
                        di->enable_charger = di->enable_charger;
                    }
                    di->cin_limit = di->usb_cin_currentmA;
                    di->currentmA = di->usb_bat_currentmA;
                    break;
                case TEMP_STAGE_HOT:
                    di->enable_charger = DISABLE_CHARGER;
                    di->cin_limit = di->usb_cin_currentmA;
                    di->currentmA = di->usb_bat_currentmA;
                    break;
                default:
                    break;
            }
          }
        }
    }else{
        di->voltagemV = di->max_voltagemV;
        di->enable_charger = ENABLE_CHARGER;
    }
    di->enable_charger = di->enable_charger |di->charger_flag|di->bat_id_failed;
    bq2415x_config_voltage_reg(di);
    bq2415x_config_control_reg(di);
    bq2415x_config_current_reg(di);

    di->battery_temp_stage = temp_status;


    bq2415x_read_byte(di, &read_reg[0], REG_STATUS_CONTROL);
    bq2415x_read_byte(di, &read_reg[2], REG_BATTERY_VOLTAGE);

    if((di->enable_charger)||((read_reg[0] & 0x07) == 0x01)||((read_reg[0] & 0x07) == 0x03)||(!di->bat_present)){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }else{
        //if((di->bat_capacity <95)&&(di->hz_mode != 1)&&((read_reg[0] & 0x37) == 0x00)){
            //di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
            //dev_info(di->dev, "not charging = %x\n",read_reg[0]);
        //}else{
            di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
        //}
    }
    if((read_reg[0] & 0x07) == 0x01){
        di->charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
    }else{
        di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
    }
    if ((read_reg[0] & 0x30) == 0x20){
        if(di->voltagemV == di->max_voltagemV){
            if((((read_reg[2] & 0xFC)>>2)*20 + 3500) > 3900){
                di->charge_status = POWER_SUPPLY_STATUS_FULL;
				if(di->bat_capacity > 97){
					di->charge_status = POWER_SUPPLY_STATUS_FULL;
				}else{
					di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
				}
            }
            dev_info(di->dev, "CHARGE DONE\n");
        }else{
            di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
            dev_info(di->dev, "reg0 = %x,reg2 = %x,CHG_V = %d\n",read_reg[0],read_reg[2],di->voltagemV);
        }

    }

    return 0;
}

static int bq2415x_get_charging_status(void)
{
    struct bq2415x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return POWER_SUPPLY_STATUS_UNKNOWN;
    }

    return di->charge_status;
}

static int bq2415x_get_charger_health(void)
{
    struct bq2415x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2415x not init\n", __func__);
        return POWER_SUPPLY_HEALTH_UNKNOWN;
    }

    return di->charger_health;
}

static int bq2415x_get_charger_dectect(void)
{
    return chip_detect_flag;
}

struct extern_charger_chg_config extern_charger_chg ={
    .name = "bq2415x",
    .set_charging_source = bq2415x_set_charging_source,
    .set_charging_enable = bq2415x_set_charging_enable,
    .set_battery_exist = bq2415x_set_battery_exist,
    .set_battery_capacity = bq2415x_set_battery_capacity,
    .set_charging_temp_stage = bq2415x_set_charging_temp_stage,
    .get_charging_status = bq2415x_get_charging_status,
    .get_charger_health = bq2415x_get_charger_health,
    .get_chargerchip_detect = bq2415x_get_charger_dectect,
};

static int bq2415x_detect_chip(struct bq2415x_device_info *di,
                        const struct i2c_device_id *id)
{
    struct i2c_client *client = to_i2c_client(di->dev);
    int ret = 0;
    u8 read_reg = 0;

    ret = bq2415x_read_byte(di, &read_reg, REG_PART_REVISION);

    if (ret < 0) {
        dev_err(&client->dev, "chip not present at address %x\n",
            client->addr);
        ret = -EINVAL;
        goto err_kfree;
    }
     dev_info(&client->dev, "charger(client = 0x%x) id->driver_data = 0x%lx\n",client->addr,id->driver_data);
    switch (client->addr) {
    case 0x6a:
        switch(id->driver_data){
            case BQ24158: /*I2C_BOARD_INFO("bq24158", 0x6a),*/
            if ((read_reg & 0x18) == 0x10){
                di->bqchip_version = BQ24158;
            }
            break;
            case FAN5405:
            if ((read_reg & 0x18) == 0x10){
                di->bqchip_version = BQ24158;
                di->chip_detect = FAN5405;
            }
            break;
            case BQ24156:/*I2C_BOARD_INFO("bq24156", 0x6a),*/
            if ((read_reg & 0x18) == 0x00){
                di->bqchip_version = BQ24156;
            }
            break;
            case FAN54015: /*I2C_BOARD_INFO("fan54015", 0x6a),*/
            if ((read_reg & 0x1C) == 0x14){
                di->bqchip_version = FAN54015;
                di->chip_detect = FAN54015;
            }
            break;
            case HL7005: /*I2C_BOARD_INFO("HL7005", 0x6a),*/
            if ((read_reg & 0x18) == 0x00){
                di->bqchip_version = FAN54015;
                di->chip_detect = FAN54015;
            }
            break;
            default:
                dev_err(&client->dev, "version reg value reg = 0x%2x\n", read_reg);
                di->bqchip_version = BQ24158;
                di->chip_detect = FAN5405;
                break;
        }
        break;
    case 0x6b:
        switch(id->driver_data){
            case FAN5403:
            if ((read_reg & 0x18) == 0x10){
                di->bqchip_version = BQ24158;
                di->chip_detect = FAN5405;
            }
            break;
            case FAN5401:
            if ((read_reg & 0x18) == 0x00){
                di->bqchip_version = BQ24158;
                di->chip_detect = FAN5401;
            }
            break;
            case FAN5400:
            case FAN5402:
            if ((read_reg & 0x18) == 0x08){
                di->bqchip_version = BQ24158;
                di->chip_detect = FAN5400|FAN5402;
            }
            break;
            case FAN5404:
            if ((read_reg & 0x18) == 0x11){
                di->bqchip_version = BQ24158;
                di->chip_detect = FAN5404;
            }
            break;
            default:
                dev_err(&client->dev, "version reg value reg = 0x%2x\n", read_reg);
                di->bqchip_version = BQ24158;
                di->chip_detect = FAN5405;
                break;
        }
        break;
    }

    if (di->bqchip_version == 0) {
        dev_err(&client->dev, "unknown bq chip\n");
        dev_err(&client->dev, "Chip address %x\n", client->addr);
        dev_err(&client->dev, "bq chip version reg value %d\n", read_reg);
        ret = -EINVAL;
        goto err_kfree;
    }
    return 0;
err_kfree:
    return ret;
}
static int bq2415x_charger_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct bq2415x_device_info *di;
    int ret = 0;
    u8 read_reg[10] = {0};

    if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
        dev_info(&client->dev, "can't talk I2C?\n");
        return -EIO;
    }

    di = kzalloc(sizeof(*di), GFP_KERNEL);
    if (!di)
        return -ENOMEM;

    di->dev = &client->dev;
    di->client = client;
    di->pdata = client->dev.platform_data;
    if(!di->pdata){
        dev_err(&client->dev, "No platform data supplied\n");
        ret = -EINVAL;
        goto err_kfree;
    }

    i2c_set_clientdata(client, di);

#if defined(CONFIG_BAT_ID_BQ2022A)
    comip_bat_id_identify(di);
#endif

    chip_detect_flag = 0;
    ret = bq2415x_detect_chip(di,id);
    if (ret < 0) {
        chip_detect_flag = -1;
        ret = -EINVAL;
        goto err_kfree;
    }
    chip_detect_flag = 1;
    bq = di;
    bq->client = di->client;
    bq->dev = di->dev;

    bq2415x_config_safety_reg(di, di->pdata->max_charger_currentmA,
                            di->pdata->max_charger_voltagemV);

    di->dpm_voltagemV     = di->pdata->dpm_voltagemV;
    di->ac_cin_currentmA  = di->pdata->ac_cin_limit_currentmA;
    di->usb_cin_currentmA = di->pdata->usb_cin_limit_currentmA;
    di->ac_bat_currentmA  = di->pdata->ac_battery_currentmA;
    di->usb_bat_currentmA = di->pdata->usb_battery_currentmA;
    di->term_currentmA    = di->pdata->termination_currentmA;
    di->cin_limit         = di->usb_cin_currentmA;
    di->voltagemV         = di->max_voltagemV;
    di->currentmA         = di->usb_bat_currentmA;

    di->active = 0;
    di->params.enable = 1;
    di->cfg_params = 1;
    di->enable_iterm = 1;
    di->enable_charger = ENABLE_CHARGER;
    di->charger_flag = ENABLE_CHARGER;
    di->low_chg = LOW_CHG_DIS;
    di->battery_temp_stage = -20;
    di->bat_capacity = 100;
    di->boost_mode = CHARGE_MODE;
    di->opa_mode = CHARGE_MODE;
    di->otg_en = OTG_DIS;

    bq2415x_config_special_charger_reg(di);
    bq2415x_config_control_reg(di);
    bq2415x_config_voltage_reg(di);
    bq2415x_config_current_reg(di);

    di->bat_present = 1;

    wake_lock_init(&di->chrg_lock, WAKE_LOCK_SUSPEND, "chrg_wake_lock");
    //di->nb.notifier_call = bq2415x_charger_event;
    INIT_DELAYED_WORK(&di->bq2415x_charger_work,
        bq2415x_charger_work);

    ret = bq2415x_read_byte(di, &read_reg[0], 0x00);
    ret = bq2415x_read_byte(di, &read_reg[1], 0x01);
    if(di->chip_detect != 0){
        bq2415x_read_byte(di, &read_reg[8], 0x10);
    }
    
    ret = bq2415x_init_irq(di);
    if(ret){
        dev_err(&client->dev, "irq init failed\n");
        goto irq_fail;
    }

    ret = sysfs_create_group(&client->dev.kobj, &bq2415x_attr_group);
    if (ret)
        dev_err(&client->dev, "could not create sysfs files\n");

    //ret=pmic_register_notifier(&di->nb, 1);

    dev_info(&client->dev, "bq24158 Charger Driver Probe %02x,%02x,%02x\n",read_reg[0],read_reg[1],read_reg[8]);

    return 0;

irq_fail:
    free_irq(gpio_to_irq(di->client->irq), di);
    wake_lock_destroy(&di->chrg_lock);
err_kfree:
    kfree(di);

    return ret;
}

static int bq2415x_charger_remove(struct i2c_client *client)
{
    struct bq2415x_device_info *di = i2c_get_clientdata(client);

    //pmic_unregister_notifier(&di->nb, 1);
    wake_lock_destroy(&di->chrg_lock);
    sysfs_remove_group(&client->dev.kobj, &bq2415x_attr_group);
    cancel_delayed_work_sync(&di->bq2415x_charger_work);

    kfree(di);

	return 0;
}

static void bq2415x_charger_shutdown(struct i2c_client *client)
{
    //struct bq2415x_device_info *di = i2c_get_clientdata(client);

    extern_charger_otg_start(0);

    return;
}

static const struct i2c_device_id bq2415x_id[] = {
    { "bq24158", BQ24158 },
    { "bq24156", BQ24156 },
    { "fan5400", FAN5400 },
    { "fan5401", FAN5401 },
    { "fan5402", FAN5402 },
    { "fan5403", FAN5403 },
    { "fan5404", FAN5404 },
    { "fan5405", FAN5405 },
    { "fan54015", FAN54015 },
    { "hl7005", HL7005 },
    {},
};

#ifdef CONFIG_PM
static int bq2415x_charger_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct bq2415x_device_info *di = platform_get_drvdata(pdev);

    cancel_delayed_work_sync(&di->bq2415x_charger_work);

    /* reset 32 second timer */

    bq2415x_config_status_reg(di);
    return 0;
}

static int bq2415x_charger_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct bq2415x_device_info *di = platform_get_drvdata(pdev);

    bq2415x_config_voltage_reg(di);

    if (di->charger_source >= POWER_SUPPLY_TYPE_MAINS)
        schedule_delayed_work(&di->bq2415x_charger_work, 0);

    return 0;
}
#else
#define bq2415x_charger_suspend NULL
#define bq2415x_charger_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops pm_ops = {
    .suspend    = bq2415x_charger_suspend,
    .resume     = bq2415x_charger_resume,
};

static struct i2c_driver bq2415x_charger_driver = {
    .probe      = bq2415x_charger_probe,
    .remove     = bq2415x_charger_remove,
    .shutdown   = bq2415x_charger_shutdown,
    .id_table   = bq2415x_id,
    .driver     = {
        .name   = "bq24158_charger",
        .pm = &pm_ops,
    },
};

static int __init bq2415x_charger_init(void)
{
    return i2c_add_driver(&bq2415x_charger_driver);
}
module_init(bq2415x_charger_init);

static void __exit bq2415x_charger_exit(void)
{
    i2c_del_driver(&bq2415x_charger_driver);
}
module_exit(bq2415x_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments Inc");
