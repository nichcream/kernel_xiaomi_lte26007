/*
 * drivers/power/bq2419x_charger.c
 *
 * BQ24190/1/2/3/4/5/6 charging driver
 *
 * Copyright (C) 2010-2020 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <plat/bq2419x_charger.h>
#include <plat/comip-battery.h>


//#define BQ2419X_DEBUG

#define BQ2419X_POLL_CHARGING



struct bq2419x_device_info {
    struct device        *dev;
    struct i2c_client    *client;
    struct delayed_work   bq2419x_charger_work;
    struct extern_charger_platform_data *pdata;

    unsigned short    input_source_reg00;
    unsigned short    power_on_config_reg01;
    unsigned short    charge_current_reg02;
    unsigned short    prechrg_term_current_reg03;
    unsigned short    charge_voltage_reg04;
    unsigned short    term_timer_reg05;
    unsigned short    thermal_regulation_reg06;
    unsigned short    misc_operation_reg07;
    unsigned short    system_status_reg08;
    unsigned short    charger_fault_reg09;
    unsigned short    bqchip_version;

    unsigned int ac_cin_currentmA;
    unsigned int ac_bat_currentmA;
    unsigned int usb_cin_currentmA;
    unsigned int usb_bat_currentmA;
        unsigned int dpm_voltagemV;
    unsigned int max_voltagemV;
    unsigned int voltagemV;
    unsigned int currentmA;
    unsigned int cin_limit;
    unsigned int term_currentmA;
    unsigned int cin_dpmmV;

    unsigned int    chrg_config;
    unsigned int    sys_minmV;
    unsigned int    prechrg_currentmA;
    unsigned int    watchdog_timer;
    unsigned int    chrg_timer;
    unsigned int    bat_compohm;
    unsigned int    comp_vclampmV;

    int charger_source;
    int charge_status;
    int charger_health;
    bool bat_present;
    bool boost_mode;
    bool charger_flag;
    int timer_fault;
    bool    hz_mode;
    bool    boost_lim;
    bool    enable_low_chg;
    bool    cfg_params;
    bool    enable_iterm;
    bool    enable_timer;
    bool    enable_dpdm;
    bool    enable_batfet;
    bool    cd_active;

    struct wake_lock chrg_lock;
};

static struct bq2419x_device_info  *bq;

static DEFINE_MUTEX(bq2419x_i2c_mutex);


static int bq2419x_write_block(struct bq2419x_device_info *di, u8 *value,
                               u8 reg, unsigned num_bytes)
{
    struct i2c_msg msg[1];
    int ret = 0;

    if (!di->client->adapter)
        return -ENODEV;

    *value  = reg;

    msg[0].addr = di->client->addr;
    msg[0].flags = 0;
    msg[0].buf  = value;
    msg[0].len  = num_bytes + 1;

    mutex_lock(&bq2419x_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&bq2419x_i2c_mutex);

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

static int bq2419x_read_block(struct bq2419x_device_info *di, u8 *value,
                            u8 reg, unsigned num_bytes)
{
    struct i2c_msg msg[2];
    u8 buf = 0;
    int ret = 0;

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
    msg[1].len = num_bytes;

    mutex_lock(&bq2419x_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&bq2419x_i2c_mutex);

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

static int bq2419x_write_byte(struct bq2419x_device_info *di, u8 value, u8 reg)
{
    /* 2 bytes offset 1 contains the data offset 0 is used by i2c_write */
    u8 temp_buffer[2] = { 0 };

    /* offset 1 contains the data */
    temp_buffer[1] = value;
    return bq2419x_write_block(di, temp_buffer, reg, 1);
}

static int bq2419x_read_byte(struct bq2419x_device_info *di, u8 *value, u8 reg)
{
    return bq2419x_read_block(di, value, reg, 1);
}

static void bq2419x_config_input_source_reg(struct bq2419x_device_info *di)
{
      unsigned int vindpm = 0;
      u8 Vdpm = 0;
      u8 Iin_limit = 0;

      vindpm = di->cin_dpmmV;

      if(vindpm < VINDPM_MIN_3880)
          vindpm = VINDPM_MIN_3880;
      else if (vindpm > VINDPM_MAX_5080)
          vindpm = VINDPM_MAX_5080;

      if (di->cin_limit <= IINLIM_100)
          Iin_limit = 0;
      else if (di->cin_limit > IINLIM_100 && di->cin_limit <= IINLIM_150)
          Iin_limit = 1;
      else if (di->cin_limit > IINLIM_150 && di->cin_limit <= IINLIM_500)
          Iin_limit = 2;
      else if (di->cin_limit > IINLIM_500 && di->cin_limit <= IINLIM_900)
          Iin_limit = 3;
      else if (di->cin_limit > IINLIM_900 && di->cin_limit <= IINLIM_1200)
          Iin_limit = 4;
      else if (di->cin_limit > IINLIM_1200 && di->cin_limit <= IINLIM_1500)
          Iin_limit = 5;
      else if (di->cin_limit > IINLIM_1500 && di->cin_limit <= IINLIM_2000)
          Iin_limit = 6;
      else if (di->cin_limit > IINLIM_2000 && di->cin_limit <= IINLIM_3000)
          Iin_limit = 7;
      else
          Iin_limit = 4;

      Vdpm = (vindpm -VINDPM_MIN_3880)/VINDPM_STEP_80;

      di->input_source_reg00 = (di->hz_mode << BQ2419x_EN_HIZ_SHIFT)
                        | (Vdpm << BQ2419x_VINDPM_SHIFT) |Iin_limit;

      bq2419x_write_byte(di, di->input_source_reg00, INPUT_SOURCE_REG00);
      return;
}

static void bq2419x_config_power_on_reg(struct bq2419x_device_info *di)
{
      unsigned int sys_min = 0;
      u8 Sysmin = 0;

      sys_min = di->sys_minmV;

      if(sys_min < SYS_MIN_MIN_3000)
          sys_min = SYS_MIN_MIN_3000;
      else if (sys_min > SYS_MIN_MAX_3700)
          sys_min = SYS_MIN_MAX_3700;

      Sysmin = (sys_min -SYS_MIN_MIN_3000)/SYS_MIN_STEP_100;

      di->power_on_config_reg01 = WATCHDOG_TIMER_RST
           | (di->chrg_config << BQ2419x_EN_CHARGER_SHIFT)
           | (Sysmin << BQ2419x_SYS_MIN_SHIFT) | di->boost_lim;

       bq2419x_write_byte(di, di->power_on_config_reg01, POWER_ON_CONFIG_REG01);
       return;
}

static void bq2419x_config_current_reg(struct bq2419x_device_info *di)
{
    unsigned int currentmA = 0;
    u8 Vichrg = 0;

    currentmA = di->currentmA;
    /* if currentmA is below ICHG_MIN, we can set the ICHG to 5*currentmA and
       set the FORCE_20PCT in REG02 to make the true current 20% of the ICHG*/
    if (currentmA < ICHG_MIN) {
        currentmA = currentmA * 5;
        di->enable_low_chg = EN_FORCE_20PCT;
    } else {
        di->enable_low_chg = DIS_FORCE_20PCT;
    }
    if (currentmA < ICHG_MIN)
        currentmA = ICHG_MIN;
    else if(currentmA > ICHG_MAX)
        currentmA = ICHG_MAX;
    Vichrg = (currentmA - ICHG_MIN)/ICHG_STEP_64;

    di->charge_current_reg02 = (Vichrg << BQ2419x_ICHG_SHIFT) | di->enable_low_chg;

     bq2419x_write_byte(di, di->charge_current_reg02, CHARGE_CURRENT_REG02);
     return;
}

static void bq2419x_config_prechg_term_current_reg(struct bq2419x_device_info *di)
{
    unsigned int precurrentmA = 0;
    unsigned int term_currentmA = 0;
    u8 Prechg = 0;
    u8 Viterm = 0;

    precurrentmA = di->prechrg_currentmA;
    term_currentmA = di->term_currentmA;

    if(precurrentmA < IPRECHRG_MIN_128)
        precurrentmA = IPRECHRG_MIN_128;
    if(term_currentmA < ITERM_MIN_128)
        term_currentmA = ITERM_MIN_128;

    if((di->bqchip_version & BQ24192I)) {
        if(precurrentmA > IPRECHRG_640)
            precurrentmA = IPRECHRG_640;
     }

    if ((di->bqchip_version & (BQ24190|BQ24192))) {
        if (precurrentmA > IPRECHRG_MAX_2048)
            precurrentmA = IPRECHRG_MAX_2048;
    }

    if (term_currentmA > ITERM_MAX_2048)
        term_currentmA = ITERM_MAX_2048;

    Prechg = (precurrentmA - IPRECHRG_MIN_128)/IPRECHRG_STEP_128;
    Viterm = (term_currentmA-ITERM_MIN_128)/ITERM_STEP_128;

    di->prechrg_term_current_reg03 = (Prechg <<  BQ2419x_IPRECHRG_SHIFT| Viterm);
    bq2419x_write_byte(di, di->prechrg_term_current_reg03, PRECHARGE_TERM_CURRENT_REG03);
    return;
}

static void bq2419x_config_voltage_reg(struct bq2419x_device_info *di)
{
    unsigned int voltagemV = 0;
    u8 Voreg = 0;

    voltagemV = di->voltagemV;
    if (voltagemV < VCHARGE_MIN_3504)
        voltagemV = VCHARGE_MIN_3504;
    else if (voltagemV > VCHARGE_MAX_4400)
        voltagemV = VCHARGE_MAX_4400;

    Voreg = (voltagemV - VCHARGE_MIN_3504)/VCHARGE_STEP_16;

    di->charge_voltage_reg04 = (Voreg << BQ2419x_VCHARGE_SHIFT) | BATLOWV_3000 |VRECHG_100;
    bq2419x_write_byte(di, di->charge_voltage_reg04, CHARGE_VOLTAGE_REG04);
    return;
}

static void bq2419x_config_term_timer_reg(struct bq2419x_device_info *di)
{
    di->term_timer_reg05 = (di->enable_iterm << BQ2419x_EN_TERM_SHIFT)
        | di->watchdog_timer | (di->enable_timer << BQ2419x_EN_TIMER_SHIFT)
        | di->chrg_timer | JEITA_ISET;

    bq2419x_write_byte(di, di->term_timer_reg05, CHARGE_TERM_TIMER_REG05);
    return;
}

static void bq2419x_config_thernal_regulation_reg(struct bq2419x_device_info *di)
{
    unsigned int batcomp_ohm = 0;
    unsigned int vclampmV = 0;
    u8 Batcomp = 0;
    u8 Vclamp = 0;

    batcomp_ohm = di->bat_compohm;
    vclampmV = di->comp_vclampmV;

    if(batcomp_ohm > BAT_COMP_MAX_70)
        batcomp_ohm = BAT_COMP_MAX_70;

    if(vclampmV > VCLAMP_MAX_112)
        vclampmV = VCLAMP_MAX_112;

    Batcomp = batcomp_ohm/BAT_COMP_STEP_10;
    Vclamp = vclampmV/VCLAMP_STEP_16;

    di->thermal_regulation_reg06 = (Batcomp << BQ2419x_BAT_COMP_SHIFT)
                           |(Vclamp << BQ2419x_VCLAMP_SHIFT) |TREG_120;

    bq2419x_write_byte(di, di->thermal_regulation_reg06, THERMAL_REGUALTION_REG06);
    return;
}

static void bq2419x_config_misc_operation_reg(struct bq2419x_device_info *di)
{
    di->misc_operation_reg07 = (di->enable_dpdm << BQ2419x_DPDM_EN_SHIFT)
          | TMR2X_EN |(di->enable_batfet<< BQ2419x_BATFET_EN_SHIFT)
          | CHRG_FAULT_INT_EN |BAT_FAULT_INT_EN;

    bq2419x_write_byte(di, di->misc_operation_reg07, MISC_OPERATION_REG07);
    return;
}

static void bq2419x_config_reg_param(struct bq2419x_device_info *di)
{

    di->term_currentmA = di->pdata->termination_currentmA;
    di->watchdog_timer = WATCHDOG_DIS;
    di->chrg_timer = CHG_TIMER_8;

    di->hz_mode = DIS_HIZ;
    di->boost_lim = BOOST_LIM_500;
    di->enable_low_chg = DIS_FORCE_20PCT;
    di->enable_iterm = EN_TERM;
    di->enable_timer = EN_TIMER;
    di->enable_batfet = EN_BATFET;
    di->charger_flag = EN_CHARGER;
    di->enable_dpdm = DPDM_DIS;

    bq2419x_config_power_on_reg(di);
    msleep(500);
    bq2419x_config_input_source_reg(di);
    bq2419x_config_current_reg(di);
    bq2419x_config_prechg_term_current_reg(di);
    bq2419x_config_voltage_reg(di);
    bq2419x_config_term_timer_reg(di);
    bq2419x_config_thernal_regulation_reg(di);
    bq2419x_config_misc_operation_reg(di);
    return;
}

static void bq2419x_start_usb_charging(struct bq2419x_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_USB;
    di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_GOOD;

    di->cin_limit = di->usb_cin_currentmA;
    di->currentmA = di->usb_bat_currentmA ;
    di->voltagemV = di->max_voltagemV;
    di->cin_dpmmV = di->dpm_voltagemV;

    di->boost_mode = CHARGE_MODE;
    di->chrg_config = EN_CHARGER;
    di->charger_flag = EN_CHARGER;

    bq2419x_config_reg_param(di);

    dev_info(di->dev, "%s\n"
            "battery current = %d mA\n"
            "battery voltage = %d mV\n"
            "cin_limit_current = %d mA\n"
            , __func__, di->currentmA, di->voltagemV,di->cin_limit);
    if(!di->bat_present){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }

#ifdef BQ2419X_POLL_CHARGING
    schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));
#endif

    //wake_lock(&di->chrg_lock);
    return;
}

static void bq2419x_start_ac_charging(struct bq2419x_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_MAINS;
    di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_GOOD;

    di->cin_limit = di->ac_cin_currentmA;
    di->currentmA = di->ac_bat_currentmA;
    di->voltagemV = di->max_voltagemV;
    di->boost_mode = CHARGE_MODE;
    di->cin_dpmmV = di->dpm_voltagemV;
    di->chrg_config = EN_CHARGER;

    bq2419x_config_reg_param(di);

    dev_info(di->dev, "%s\n"
            "battery current = %d mA\n"
            "battery voltage = %d mV\n"
            "cin_limit_current = %d mA\n"
            , __func__, di->currentmA, di->voltagemV,di->cin_limit);
    if(!di->bat_present){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }

#ifdef BQ2419X_POLL_CHARGING
    schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));
#endif
    //wake_lock(&di->chrg_lock);
    return;
}


static void bq2419x_stop_charging(struct bq2419x_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
    di->charge_status = POWER_SUPPLY_STATUS_DISCHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;

    di->boost_mode = CHARGE_MODE;
    di->enable_batfet = EN_BATFET;
    di->charger_flag = 0;
    di->hz_mode = DIS_HIZ;
    di->chrg_config = DIS_CHARGER;
    di->enable_dpdm = DPDM_DIS;

    bq2419x_config_power_on_reg(di);
    bq2419x_config_input_source_reg(di);
    bq2419x_config_misc_operation_reg(di);

#ifdef BQ2419X_POLL_CHARGING
    schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));
#endif
    dev_info(di->dev,"bq2419x_stop_charging!\n");
    //wake_unlock(&di->chrg_lock);
   return;
}

static void bq2419x_start_otg(struct bq2419x_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
    di->charge_status  = POWER_SUPPLY_STATUS_DISCHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;

    di->boost_mode = OTG_MODE;
    di->hz_mode = DIS_HIZ;
    di->chrg_config = EN_CHARGER_OTG;
    di->boost_lim = BOOST_LIM_500;

    bq2419x_config_power_on_reg(di);
    bq2419x_config_input_source_reg(di);

#ifdef BQ2419X_POLL_CHARGING
    schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));
#endif
    //wake_lock(&di->chrg_lock);
    dev_info(di->dev,"bq2419x_start_otg!\n");
    return;
}

static void bq2419x_config_status_reg(struct bq2419x_device_info *di)
{
    di->power_on_config_reg01 = di->power_on_config_reg01 |WATCHDOG_TIMER_RST;
    bq2419x_write_byte(di, di->power_on_config_reg01, POWER_ON_CONFIG_REG01);
    return;
}

static void bq2419x_charger_update_status(struct bq2419x_device_info *di)
{
    int i = 0;
    u8 read_reg[11] = {0};

    di->timer_fault = 0;
    for(i = 0; i< 11; i++)
        bq2419x_read_byte(di, &read_reg[i], i);

    if(di->boost_mode == CHARGE_MODE){/*charge process mode*/
        if ((read_reg[8] & BQ2419x_CHGR_STAT_CHAEGE_DONE) == BQ2419x_CHGR_STAT_CHAEGE_DONE){
            dev_info(di->dev, "CHARGE DONE\n");
        }

        if ((read_reg[8] & BQ2419x_PG_STAT) == BQ2419x_NOT_PG_STAT){
            di->cfg_params = 1;
            dev_info(di->dev, "not power good\n");
        }

        if ((read_reg[9] & BQ2419x_POWER_SUPPLY_OVP) == BQ2419x_POWER_SUPPLY_OVP){
            dev_err(di->dev, "POWER_SUPPLY_OVERVOLTAGE = %x\n", read_reg[9]);
        }

        if ((read_reg[9] & BQ2419x_WATCHDOG_FAULT) == BQ2419x_WATCHDOG_FAULT)
            di->timer_fault = 1;

        if (read_reg[9] & 0xFF) {
            di->cfg_params = 1;
            dev_err(di->dev, "CHARGER STATUS %x\n", read_reg[9]);
        }

        if ((read_reg[9] & BQ2419x_BAT_FAULT_OVP) == BQ2419x_BAT_FAULT_OVP) {
            di->hz_mode = EN_HIZ;
            bq2419x_config_input_source_reg(di);
            bq2419x_write_byte(di, di->charge_voltage_reg04, CHARGE_VOLTAGE_REG04);
            dev_err(di->dev, "BATTERY OVP = %x\n", read_reg[9]);
            msleep(700);
            di->hz_mode = DIS_HIZ;
            bq2419x_config_input_source_reg(di);
        }

        if ((di->timer_fault == 1) || (di->cfg_params == 1)) {
            bq2419x_write_byte(di, di->input_source_reg00, INPUT_SOURCE_REG00);
            bq2419x_write_byte(di, di->power_on_config_reg01, POWER_ON_CONFIG_REG01);
            bq2419x_write_byte(di, di->charge_current_reg02, CHARGE_CURRENT_REG02);
            bq2419x_write_byte(di, di->prechrg_term_current_reg03, PRECHARGE_TERM_CURRENT_REG03);
            bq2419x_write_byte(di, di->charge_voltage_reg04, CHARGE_VOLTAGE_REG04);
            bq2419x_write_byte(di, di->term_timer_reg05, CHARGE_TERM_TIMER_REG05);
            bq2419x_write_byte(di, di->thermal_regulation_reg06, THERMAL_REGUALTION_REG06);
            di->cfg_params = 0;
        }
    }else{/*otg boost mode*/
        if(read_reg[9] & BQ2419x_OTG_FAULT){
            dev_err(di->dev, "VBUS overloaded in OTG, or VBUS OVP = %x\n", read_reg[9]);
        }
        bq2419x_config_power_on_reg(di);
    }
    /* reset 30 second timer */
    bq2419x_config_status_reg(di);
    return;
}
/*===========================================================================
  Function:       bq2419x_charger_work
  Description:    send a notifier event to sysfs
  Input:          struct work_struct *
  Output:         none
  Return:         none
  Others:         none
===========================================================================*/
static void bq2419x_charger_work(struct work_struct *work)
{
    struct bq2419x_device_info *di = container_of(work,
        struct bq2419x_device_info, bq2419x_charger_work.work);

    bq2419x_charger_update_status(di);

#ifdef BQ2419X_POLL_CHARGING
    schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(BQ2419x_WATCHDOG_TIMEOUT));
#endif
    return;
}

static irqreturn_t bq2419x_charger_irq(int irq, void *_di)
{
    struct bq2419x_device_info *di = _di;

    schedule_delayed_work(&di->bq2419x_charger_work, 0);

    return IRQ_HANDLED;
}

static int bq2419x_init_irq(struct bq2419x_device_info *di)
{
    int ret = 0;

#ifdef BQ2419X_POLL_CHARGING
    return 0;
#endif

    if (gpio_request(di->client->irq, "bq2419x_irq") < 0){
        ret = -ENOMEM;
        goto irq_fail;
    }

    gpio_direction_input(di->client->irq);

    /* request irq interruption */
    ret = request_irq(gpio_to_irq(di->client->irq), bq2419x_charger_irq,
        IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING| IRQF_SHARED,"bq2419x_irq",di);
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


/*
* set 1 --- hz_mode ; 0 --- not hz_mode
*/
static ssize_t bq2419x_set_enable_hz_mode(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->hz_mode= val;
    bq2419x_config_input_source_reg(di);

    return status;
}

static ssize_t bq2419x_show_enable_hz_mode(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->hz_mode;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_dppm_voltage(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < VINDPM_MIN_3880)
                      || (val > VINDPM_MAX_5080))
         return -EINVAL;

    di->cin_dpmmV = val;
    bq2419x_config_input_source_reg(di);

    return status;
}

static ssize_t bq2419x_show_dppm_voltage(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->cin_dpmmV;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < IINLIM_100)
                || (val > IINLIM_3000))
        return -EINVAL;

    di->cin_limit = val;
    bq2419x_config_input_source_reg(di);

    return status;
}

static ssize_t bq2419x_show_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->cin_limit;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < VCHARGE_MIN_3504)
                     || (val > VCHARGE_MAX_4400))
        return -EINVAL;

    di->voltagemV = val;
    bq2419x_config_voltage_reg(di);

    return status;
}

static ssize_t bq2419x_show_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->voltagemV;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 100)
                   || (val > ICHG_3000))
        return -EINVAL;

    di->currentmA = val;
    bq2419x_config_current_reg(di);

    return status;
}

static ssize_t bq2419x_show_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->currentmA;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_precharge_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > IPRECHRG_MAX_2048))
    return -EINVAL;

    di->prechrg_currentmA = val;
    bq2419x_config_prechg_term_current_reg(di);

    return status;
}

static ssize_t bq2419x_show_precharge_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->prechrg_currentmA;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > ITERM_MAX_2048))
        return -EINVAL;

    di->term_currentmA = val;
    bq2419x_config_prechg_term_current_reg(di);

    return status;
}

static ssize_t bq2419x_show_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->term_currentmA;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_enable_itermination(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->enable_iterm = val;
    bq2419x_config_term_timer_reg(di);

    return status;
}

static ssize_t bq2419x_show_enable_itermination(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->enable_iterm;
    return sprintf(buf, "%lu\n", val);
}


/* set 1 --- enable_charger; 0 --- disable charger */
static ssize_t bq2419x_set_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->chrg_config = val;
    di->charger_flag = val;
    bq2419x_config_power_on_reg(di);

    return status;
}

static ssize_t bq2419x_show_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->chrg_config ;
    return sprintf(buf, "%lu\n", val);
}

/* set 1 --- enable_batfet; 0 --- disable batfet */
static ssize_t bq2419x_set_enable_batfet(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->enable_batfet = val ^ 0x01;
    bq2419x_config_misc_operation_reg(di);

    return status;
}

static ssize_t bq2419x_show_enable_batfet(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->enable_batfet ^ 0x01;
    return sprintf(buf, "%lu\n", val);
}

/*
* set 1 --- enable bq24192 IC; 0 --- disable bq24192 IC
*
*/
static ssize_t bq2419x_set_enable_cd(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if ((strict_strtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->cd_active =val ^ 0x1;

    return status;
}

static ssize_t bq2419x_show_enable_cd(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    val = di->cd_active ^ 0x1;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t bq2419x_set_charging(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    int status = count;
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    if (strncmp(buf, "startac", 7) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_USB)
            bq2419x_stop_charging(di);
        bq2419x_start_ac_charging(di);
    } else if (strncmp(buf, "startusb", 8) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
            bq2419x_stop_charging(di);
        bq2419x_start_usb_charging(di);
    } else if (strncmp(buf, "stop" , 4) == 0) {
        bq2419x_stop_charging(di);
    } else if (strncmp(buf, "otg" , 3) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_BATTERY){
            bq2419x_stop_charging(di);
            bq2419x_start_otg(di);
        } else{
            return -EINVAL;
        }
    } else
        return -EINVAL;

    return status;
}



static ssize_t bq2419x_show_chargelog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    int i = 0;
    u8 read_reg[11] = {0};
    u8 buf_temp[26] = {0};
    struct bq2419x_device_info *di = dev_get_drvdata(dev);

    for(i=0;i<11;i++)
    bq2419x_read_byte(di, &read_reg[i], i);
    for(i=0;i<11;i++){
        snprintf(buf_temp,26,"0x%-8.2x",read_reg[i]);
        strcat(buf,buf_temp);
    }
    strcat(buf,"\n");
   return strlen(buf);
}


static DEVICE_ATTR(enable_hz_mode, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_hz_mode,
                bq2419x_set_enable_hz_mode);
static DEVICE_ATTR(dppm_voltage, S_IWUSR | S_IRUGO,
                bq2419x_show_dppm_voltage,
                bq2419x_set_dppm_voltage);
static DEVICE_ATTR(cin_limit, S_IWUSR | S_IRUGO,
                bq2419x_show_cin_limit,
                bq2419x_set_cin_limit);
static DEVICE_ATTR(regulation_voltage, S_IWUSR | S_IRUGO,
                bq2419x_show_regulation_voltage,
                bq2419x_set_regulation_voltage);
static DEVICE_ATTR(charge_current, S_IWUSR | S_IRUGO,
                bq2419x_show_charge_current,
                bq2419x_set_charge_current);
static DEVICE_ATTR(termination_current, S_IWUSR | S_IRUGO,
                bq2419x_show_termination_current,
                bq2419x_set_termination_current);
static DEVICE_ATTR(precharge_current, S_IWUSR | S_IRUGO,
                bq2419x_show_precharge_current,
                bq2419x_set_precharge_current);
static DEVICE_ATTR(enable_itermination, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_itermination,
                bq2419x_set_enable_itermination);

static DEVICE_ATTR(enable_charger, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_charger,
                bq2419x_set_enable_charger);
static DEVICE_ATTR(enable_batfet, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_batfet,
                bq2419x_set_enable_batfet);
static DEVICE_ATTR(enable_cd, S_IWUSR | S_IRUGO,
                bq2419x_show_enable_cd,
                bq2419x_set_enable_cd);
static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO,
                NULL,
                bq2419x_set_charging);
static DEVICE_ATTR(chargelog, S_IWUSR | S_IRUGO,
                bq2419x_show_chargelog,
                NULL);

static struct attribute *bq2419x_attributes[] = {
    &dev_attr_enable_hz_mode.attr,
    &dev_attr_dppm_voltage.attr,
    &dev_attr_cin_limit.attr,
    &dev_attr_regulation_voltage.attr,
    &dev_attr_charge_current.attr,
    &dev_attr_precharge_current.attr,
    &dev_attr_termination_current.attr,
    &dev_attr_enable_itermination.attr,
    &dev_attr_enable_charger.attr,
    &dev_attr_enable_batfet.attr,
    &dev_attr_enable_cd.attr,
    &dev_attr_charging.attr,
    &dev_attr_chargelog.attr,
    NULL,
};

static const struct attribute_group bq2419x_attr_group = {
    .attrs = bq2419x_attributes,
};


static int bq2419x_set_charging_enable(bool enable)
{
    struct bq2419x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2419x not init\n", __func__);
        return -EINVAL;
    }

#if defined(BQ2419X_DEBUG)
        return 0;
#endif

    if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY)||(!di->bat_present))
        return 0;

    if(enable){
        di->chrg_config= EN_CHARGER;
    }else{
        di->chrg_config= DIS_CHARGER;
    }
    di->chrg_config&= di->charger_flag;
    bq2419x_config_power_on_reg(di);

    return 0;
}
static int bq2419x_set_battery_exist(bool battery_exist)
{
    struct bq2419x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2419x not init\n", __func__);
        return -EINVAL;
    }
    di->bat_present = battery_exist;
    if(!di->bat_present){
        dev_err(di->dev, "battery is not present! \n");
    }
    return(0);
}

static int bq2419x_set_charging_source(int charging_source)
{
    struct bq2419x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2419x not init\n", __func__);
        return -EINVAL;
    }
    switch(charging_source){
        case LCUSB_EVENT_PLUGIN:
            di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
            di->chrg_config = EN_CHARGER;
            di->charger_flag = EN_CHARGER;
            bq2419x_config_power_on_reg(di);
            break;
        case LCUSB_EVENT_VBUS:
            bq2419x_start_usb_charging(di);
            break;
        case LCUSB_EVENT_CHARGER:
            bq2419x_start_ac_charging(di);
            break;
        case LCUSB_EVENT_NONE:
            bq2419x_stop_charging(di);
            break;
        case LCUSB_EVENT_OTG:
            bq2419x_start_otg(di);
            break;
        default:
            break;
    }
    return 0;
}

/*monitor battery temp limit chargingm;1=enable,0=disable*/
static int bq2419x_set_charging_temp_stage(int temp_status,int volt,bool enable)
{
    struct bq2419x_device_info *di = bq;
    u8 read_reg[11] = {0};

    if(!di){
        pr_err("%s: bq2419x not init\n", __func__);
        return -EINVAL;
    }

#if defined(BQ2419X_DEBUG)
        return 0;
#endif

    if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY))
        return 0;
    if(enable){
        if(di->charger_source == POWER_SUPPLY_TYPE_USB){
            switch(temp_status){
            case TEMP_STAGE_OVERCOLD:
            case TEMP_STAGE_COLD_ZERO:
                di->chrg_config = DIS_CHARGER;
                break;
            case TEMP_STAGE_ZERO_COOL:
            case TEMP_STAGE_COOL_NORMAL:
                di->chrg_config = EN_CHARGER;
                break;
            case TEMP_STAGE_NORMAL_HOT:
                if(volt > 4100){
                    di->chrg_config = DIS_CHARGER;
                }else if (volt < 4000){
                    di->chrg_config = EN_CHARGER;
                }else{
                    di->chrg_config = di->chrg_config;
                }
                break;
                case TEMP_STAGE_HOT:
                    di->chrg_config = DIS_CHARGER;
                break;
                default:
                    break;
            }
        }else if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
            switch(temp_status){
            case TEMP_STAGE_OVERCOLD:
            case TEMP_STAGE_COLD_ZERO:
                di->chrg_config = DIS_CHARGER;
                break;
            case TEMP_STAGE_ZERO_COOL:
                //di->chrg_config = EN_CHARGER;
                //di->cin_limit = di->ac_cin_currentmA;
                //di->currentmA = di->usb_bat_currentmA;
                //break;
            case TEMP_STAGE_COOL_NORMAL:
                di->chrg_config = EN_CHARGER;
                di->cin_limit = di->ac_cin_currentmA;
                di->currentmA = di->ac_bat_currentmA;
                break;
            case TEMP_STAGE_NORMAL_HOT:
                if(volt > 4100){
                    di->chrg_config = DIS_CHARGER;
                }else if (volt < 4000){
                    di->chrg_config = EN_CHARGER;
                }else{
                    di->chrg_config = di->chrg_config;
                }
                di->cin_limit = di->usb_cin_currentmA;
                di->currentmA = di->usb_bat_currentmA;
                break;
            case TEMP_STAGE_HOT:
                di->chrg_config = DIS_CHARGER;
                di->cin_limit = di->ac_cin_currentmA;
                di->currentmA = di->ac_bat_currentmA;
                break;
            default:
                break;
            }
        }
    }else{
        di->chrg_config = EN_CHARGER;
    }
    di->chrg_config&= di->charger_flag;
    bq2419x_config_power_on_reg(di);
    bq2419x_config_current_reg(di);
    bq2419x_config_voltage_reg(di);

    bq2419x_read_byte(di, &read_reg[8], 8);
    bq2419x_read_byte(di, &read_reg[9], 9);
    if((!di->chrg_config)||((read_reg[9] & 0x30) != 0x00)||(!di->bat_present)){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }else{
        di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
    }
    if((read_reg[9] & BQ2419x_POWER_SUPPLY_OVP) == BQ2419x_POWER_SUPPLY_OVP){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        di->charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
    }else{
        di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
    }
    if ((read_reg[8] & BQ2419x_CHGR_STAT_CHAEGE_DONE) == BQ2419x_CHGR_STAT_CHAEGE_DONE){
        di->charge_status = POWER_SUPPLY_STATUS_FULL;
    }
    return 0;
}

static int bq2419x_get_charging_status(void)
{
    struct bq2419x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2419x not init\n", __func__);
        return POWER_SUPPLY_STATUS_UNKNOWN;
    }

    return di->charge_status;
}

static int bq2419x_get_charger_health(void)
{
    struct bq2419x_device_info *di = bq;

    if(!di){
        pr_err("%s: bq2419x not init\n", __func__);
        return POWER_SUPPLY_HEALTH_UNKNOWN;
    }

    return di->charger_health;
}

struct extern_charger_chg_config extern_charger_chg ={
    .name = "bq2419x",
    .set_charging_source = bq2419x_set_charging_source,
    .set_charging_enable = bq2419x_set_charging_enable,
    .set_battery_exist = bq2419x_set_battery_exist,
    .set_charging_temp_stage = bq2419x_set_charging_temp_stage,
    .get_charging_status = bq2419x_get_charging_status,
    .get_charger_health = bq2419x_get_charger_health,
};



static int bq2419x_charger_probe(struct i2c_client *client,
                           const struct i2c_device_id *id)
{
    struct bq2419x_device_info *di;
    int ret = 0;
    u8 read_reg = 0;

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

    ret = bq2419x_read_byte(di, &read_reg, PART_REVISION_REG0A);
    if (ret < 0) {
        dev_err(&client->dev, "chip not present at address %x\n",
            client->addr);
        ret = -EINVAL;
        goto err_kfree;
    }

    if (((read_reg & 0x38) == BQ24190 || (read_reg & 0x38) == BQ24192)
         && (client->addr == 0x6B)){
        di->bqchip_version = BQ24192;
    }else if(((read_reg & 0x38) == BQ24192I)&& (client->addr == 0x6B)){
        di->bqchip_version = BQ24192I;
    }else{
    }

    if (di->bqchip_version == 0) {
        dev_err(&client->dev, "unknown bq chip\n");
        dev_err(&client->dev, "Chip address %x", client->addr);
        dev_err(&client->dev, "bq chip version reg value %d", read_reg);
        ret = -EINVAL;
        goto err_kfree;
    }

    bq = di;
    bq->client = di->client;
    bq->dev = di->dev;

    di->max_voltagemV     = di->pdata->max_charger_voltagemV;
    di->ac_cin_currentmA  = di->pdata->ac_cin_limit_currentmA;
    di->usb_cin_currentmA = di->pdata->usb_cin_limit_currentmA;
    di->ac_bat_currentmA  = di->pdata->ac_battery_currentmA;
    di->usb_bat_currentmA = di->pdata->usb_battery_currentmA;
    di->term_currentmA    = di->pdata->termination_currentmA;
    di->dpm_voltagemV     = di->pdata->dpm_voltagemV;
    di->cin_limit         = di->usb_cin_currentmA;
    di->voltagemV         = di->max_voltagemV;
    di->currentmA         = di->usb_bat_currentmA;

    di->cin_dpmmV = di->dpm_voltagemV;
    di->sys_minmV = SYS_MIN_3500;
    di->prechrg_currentmA = IPRECHRG_256;
    di->term_currentmA = ITERM_MIN_128;
    di->watchdog_timer = WATCHDOG_40;
    di->chrg_timer = CHG_TIMER_8;
    di->bat_compohm = BAT_COMP_40;
    di->comp_vclampmV = VCLAMP_48;

    di->hz_mode = DIS_HIZ;
    di->chrg_config = EN_CHARGER;
    di->boost_lim = BOOST_LIM_500;
    di->enable_low_chg = DIS_FORCE_20PCT;
    di->enable_iterm = EN_TERM;
    di->enable_timer = EN_TIMER;
    di->enable_batfet = EN_BATFET;
    di->charger_flag = 0;
    di->cfg_params = 1;
    di->bat_present = 1;

    bq2419x_config_power_on_reg(di);
    bq2419x_config_current_reg(di);
    bq2419x_config_prechg_term_current_reg(di);
    bq2419x_config_voltage_reg(di);
    bq2419x_config_term_timer_reg(di);

    wake_lock_init(&di->chrg_lock, WAKE_LOCK_SUSPEND, "chrg_wake_lock");

    INIT_DELAYED_WORK(&di->bq2419x_charger_work,bq2419x_charger_work);

    ret = bq2419x_init_irq(di);
    if(ret){
        dev_err(&client->dev, "irq init failed\n");
        goto irq_fail;
    }

    ret = sysfs_create_group(&client->dev.kobj, &bq2419x_attr_group);
    if (ret){
        dev_err(&client->dev, "could not create sysfs files\n");
    }

    dev_info(&client->dev, "bq2419x Charger Driver Probe\n");

    return 0;

irq_fail:
    free_irq(gpio_to_irq(di->client->irq), di);
    wake_lock_destroy(&di->chrg_lock);
err_kfree:
    kfree(di);
    di = NULL;

    return ret;
}

static int bq2419x_charger_remove(struct i2c_client *client)
{
    struct bq2419x_device_info *di = i2c_get_clientdata(client);

    wake_lock_destroy(&di->chrg_lock);
    sysfs_remove_group(&client->dev.kobj, &bq2419x_attr_group);
    cancel_delayed_work_sync(&di->bq2419x_charger_work);

    kfree(di);

    return 0;
}

static void bq2419x_charger_shutdown(struct i2c_client *client)
{
    struct bq2419x_device_info *di = i2c_get_clientdata(client);

    bq2419x_stop_charging(di);

   return;
}

static const struct i2c_device_id bq2419x_id[] = {
    { "bq2419x_charger", 0 },
    { "bq24190", 1 },
    { "bq24192", 2 },
    { "bq24193", 3 },
    { "bq24192I", 4 },
    {},
};

#ifdef CONFIG_PM
static int bq2419x_charger_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct bq2419x_device_info *di = platform_get_drvdata(pdev);

     cancel_delayed_work_sync(&di->bq2419x_charger_work);

    bq2419x_config_power_on_reg(di);
    return 0;
}

static int bq2419x_charger_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct bq2419x_device_info *di = platform_get_drvdata(pdev);

    if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
        bq2419x_config_voltage_reg(di);
        schedule_delayed_work(&di->bq2419x_charger_work, msecs_to_jiffies(0));
    }

    bq2419x_config_power_on_reg(di);
   return 0;
}

#else

#define bq2419x_charger_suspend       NULL
#define bq2419x_charger_resume        NULL

#endif /* CONFIG_PM */

static const struct dev_pm_ops pm_ops = {
    .suspend    = bq2419x_charger_suspend,
    .resume     = bq2419x_charger_resume,
};

static struct i2c_driver bq2419x_charger_driver = {
    .probe = bq2419x_charger_probe,
    .remove = bq2419x_charger_remove,
    .shutdown = bq2419x_charger_shutdown,
    .id_table = bq2419x_id,
    .driver = {
         .name = "bq2419x_charger",
        .pm = &pm_ops,
    },
};

static int __init bq2419x_charger_init(void)
{
    return i2c_add_driver(&bq2419x_charger_driver);
}
module_init(bq2419x_charger_init);

static void __exit bq2419x_charger_exit(void)
{
    i2c_del_driver(&bq2419x_charger_driver);
}
module_exit(bq2419x_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TI Inc");
