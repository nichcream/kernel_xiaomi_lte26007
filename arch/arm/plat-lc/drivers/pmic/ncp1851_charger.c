
/*
 * drivers/power/ncp1851_charger.c
 *
 * ncp1851/A charging driver
 *
 * Copyright (C) 2014-2015 onsemi/leadcore.
 * Author: leadcore Tech.
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
#include <plat/ncp1851_charger.h>
#include <plat/comip-battery.h>

//#define NCP1851_DEBUG

/*if define NCP1851_POLL_CHARGING, use poll get charge status
  if not define,use charger irq thread*/
#define NCP1851_POLL_CHARGING

struct ncp1851_device_info {
    struct device *dev;
    struct i2c_client *client;
    struct delayed_work ncp1851_charger_work;
    struct extern_charger_platform_data *pdata;
    struct wake_lock chrg_lock;
    struct notifier_block nb;
    int charger_source;
    int charge_status;
    int charger_health;
    bool bat_present;
    bool boost_mode;
    bool chg_en;
    bool otg_en;
    bool ntc_en;
    bool tchg_rst;
    bool int_mask;
    bool wdto_dis;
    bool chgto_dis;
    bool pwr_path;
    bool factry_mod;
    bool charger_flag;
    int timer_fault;
    int cfg_params;

    unsigned short  ctrl1_reg;
    unsigned short  ctrl2_reg;
    unsigned short  stat_msk_reg;
    unsigned short  ch1_msk_reg;
    unsigned short  ch2_msk_reg;
    unsigned short  bst_msk_reg;
    unsigned short  vbat_reg;
    unsigned short  ibat_reg;
    unsigned short  misc_reg;
    unsigned short  ntc1_reg;
    unsigned short  ntc2_reg;

    unsigned int iweak_currentmA;
    unsigned int ctrl_vfet_voltagemV;
    unsigned int ichred_currentmA;
    unsigned int vchred_voltagemV;
    unsigned int batcold_voltagemV;
    unsigned int bathot_voltagemV;

    unsigned int max_voltagemV;
    unsigned int voltagemV;
    unsigned int currentmA;
    unsigned int cin_limit;
    unsigned int term_currentmA;

    unsigned int ac_cin_currentmA;
    unsigned int ac_bat_currentmA;
    unsigned int usb_cin_currentmA;
    unsigned int usb_bat_currentmA;
};

struct ncp1851_device_info  *bq;

static DEFINE_MUTEX(ncp1851_i2c_mutex);


static int ncp1851_write_block(struct ncp1851_device_info *di, u8 *value,
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

    mutex_lock(&ncp1851_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&ncp1851_i2c_mutex);

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

static int ncp1851_read_block(struct ncp1851_device_info *di, u8 *value,
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

    mutex_lock(&ncp1851_i2c_mutex);
    ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
    mutex_unlock(&ncp1851_i2c_mutex);

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

static int ncp1851_write_byte(struct ncp1851_device_info *di, u8 value, u8 reg)
{
    u8 temp_buffer[2] = { 0 };

    temp_buffer[1] = value;
    return ncp1851_write_block(di, temp_buffer, reg, 1);
}

static int ncp1851_read_byte(struct ncp1851_device_info *di, u8 *value, u8 reg)
{
    return ncp1851_read_block(di, value, reg, 1);
}

static void ncp1851_ctrl1_set_reg(struct ncp1851_device_info *di)
{
    di->ctrl1_reg = ((di->chg_en << CHG_EN_SHIFT)|(di->otg_en << OTG_EN_SHIFT)
                     |(di->ntc_en << NTC_EN_SHIFT)|(di->tchg_rst << TCHG_RST_SHIFT)
                     |(di->int_mask));
    di->ctrl1_reg = di->ctrl1_reg & 0x7F;
    ncp1851_write_byte(di, di->ctrl1_reg, REG_CTRL1_REGISTER);

    return;
}

static void ncp1851_ctrl2_set_reg(struct ncp1851_device_info *di)
{
    di->ctrl2_reg = ((di->wdto_dis   << WDTO_DIS_SHIFT)
                     |(di->chgto_dis << CHGTO_DIS_SHIFT)
                     |(di->pwr_path  << PWR_PATH_SHIFT)|(TRANS_EN_REG)
                     |(IINSET_PIN_DIS << IINSET_PIN_EN_SHIFT)
                     |(IINLIM_EN << 1)|(AICL_DIS));

    ncp1851_write_byte(di, di->ctrl2_reg, REG_CTRL2_REGISTER);

    return;
}

static void ncp1851_stat_msk_set_reg(struct ncp1851_device_info *di)
{
    di->stat_msk_reg = 0x3C;

    ncp1851_write_byte(di, di->stat_msk_reg, REG_STAT_MSK_REGISTER);

    return;
}

static void ncp1851_ch1_msk_set_reg(struct ncp1851_device_info *di)
{
    di->ch1_msk_reg = BATRMV_MASK|STATECHG_MASK;

    ncp1851_write_byte(di, di->ch1_msk_reg, REG_CH1_MSK_REGISTER);

    return;
}

static void ncp1851_ch2_msk_set_reg(struct ncp1851_device_info *di)
{
    di->ch2_msk_reg = 0xF0|WDTO_MASK|USBTO_MASK;

    ncp1851_write_byte(di, di->ch2_msk_reg, REG_CH2_MSK_REGISTER);

    return;
}

static void ncp1851_bst_msk_set_reg(struct ncp1851_device_info *di)
{
    di->bst_msk_reg = VBUSOV_MASK|VBUSLO_MASK;

    ncp1851_write_byte(di, di->bst_msk_reg, REG_BST_MSK_REGISTER);

    return;
}

/* Charge volysge set mV */
static void ncp1851_vbat_set_reg(struct ncp1851_device_info *di)
{
    unsigned int voltagemV = 0;

    voltagemV = di->voltagemV;
    if (voltagemV < CTRL_VBAT_MIN)
        voltagemV = CTRL_VBAT_MIN;

    if (voltagemV > CTRL_VBAT_MAX)
        voltagemV = CTRL_VBAT_MAX;

    di->vbat_reg = (voltagemV - CTRL_VBAT_MIN)/CTRL_VBAT_STEP;
    ncp1851_write_byte(di, di->vbat_reg, REG_VBAT_SET_REGISTER);

    return;
}

/* Charge current and termination current set mA */
static void ncp1851_ibat_set_reg(struct ncp1851_device_info *di)
{
    unsigned int currentmA = 0;
    unsigned int term_currentmA = 0;
    u8 Vichrg = 0;
    u8 Viterm = 0;

    currentmA = di->currentmA;
    term_currentmA = di->term_currentmA;
    if (currentmA < ICHG_MIN)
        currentmA = ICHG_MIN;
    if (currentmA > ICHG_MAX)
        currentmA = ICHG_MAX;

    if (term_currentmA < IEOC_MIN)
        term_currentmA = IEOC_MIN;
    if (term_currentmA > IEOC_MAX)
        term_currentmA = IEOC_MAX;

    Vichrg = (currentmA - ICHG_MIN)/ICHG_STEP;
    Viterm = (term_currentmA - IEOC_MIN)/IEOC_STEP;

    di->ibat_reg = (Viterm << IEOC_SHIFT | Vichrg);
    ncp1851_write_byte(di, di->ibat_reg, REG_IBAT_SET_REGISTER);

    return;
}

/* input current set mA */
static void ncp1851_misc_set_reg(struct ncp1851_device_info *di)
{
    u8 Iin_limit = 0;
    u8 iweak = 0,ctrl_vfet = 0;

    if (di->cin_limit <= 100)
        Iin_limit = 0;
    else if (di->cin_limit > 100 && di->cin_limit <= 500)
        Iin_limit = 1;
    else if (di->cin_limit > 500 && di->cin_limit <= 900)
        Iin_limit = 2;
    else
        Iin_limit = 3;

    if (di->iweak_currentmA < IWEAK_MIN)
        di->iweak_currentmA = IWEAK_MIN;
    if (di->iweak_currentmA > IWEAK_MAX)
        di->iweak_currentmA = IWEAK_MAX;

    if (di->ctrl_vfet_voltagemV < CTRL_VFET_MIN)
        di->ctrl_vfet_voltagemV = CTRL_VFET_MIN;
    if (di->ctrl_vfet_voltagemV > CTRL_VFET_MAX)
        di->ctrl_vfet_voltagemV = CTRL_VFET_MAX;

    iweak = (di->iweak_currentmA - IWEAK_MIN)/IWEAK_STEP;
    ctrl_vfet = (di->ctrl_vfet_voltagemV - CTRL_VFET_MIN)/CTRL_VFET_STEP;

    di->misc_reg = ((iweak << IWEAK_SHIFT)
                    |(ctrl_vfet << CTRL_VFET_SHIFT)|Iin_limit);

    ncp1851_write_byte(di, di->misc_reg, REG_MISC_SET_REGISTER);
    return;
}

static void ncp1851_ntc_set1_reg(struct ncp1851_device_info *di)
{
    u8 vchred = 0,ichred = 0;

    if (di->vchred_voltagemV < VCHRED_MIN)
        di->vchred_voltagemV = VCHRED_MIN;
    if (di->vchred_voltagemV > VCHRED_MAX)
        di->vchred_voltagemV = VCHRED_MAX;

    if (di->ichred_currentmA < ICHRED_MIN)
        di->ichred_currentmA = ICHRED_MIN;
    if (di->ichred_currentmA > ICHRED_MAX)
        di->ichred_currentmA = ICHRED_MAX;

    vchred = (di->vchred_voltagemV - VCHRED_MIN)/VCHRED_STEP;
    ichred = (di->ichred_currentmA - ICHRED_MIN)/ICHRED_STEP;
    di->ntc1_reg = ((vchred << VCHRED_SHIFT)|ichred);

    ncp1851_write_byte(di, di->ntc1_reg, REG_NTC_SET1_REGISTER);
    return;
}

static void ncp1851_ntc_set2_reg(struct ncp1851_device_info *di)
{
    u8 batcold = 0,bathot = 0;

    if (di->batcold_voltagemV < BATCOLD_MIN)
        di->batcold_voltagemV = BATCOLD_MIN;
    if (di->batcold_voltagemV > BATCOLD_MAX)
        di->batcold_voltagemV = BATCOLD_MAX;

    if (di->bathot_voltagemV < BATHOT_MIN)
        di->bathot_voltagemV = BATHOT_MIN;
    if (di->bathot_voltagemV > BATHOT_MAX)
        di->bathot_voltagemV = BATHOT_MAX;

    batcold = (di->batcold_voltagemV - BATCOLD_MIN)/BATCOLD_STEP;
    bathot = (di->bathot_voltagemV - BATHOT_MIN)/BATHOT_STEP;

    di->ntc2_reg = ((batcold << BATCOLD_SHIFT)
                    |(bathot << BATHOT_SHIFT));

    ncp1851_write_byte(di, di->ntc2_reg, REG_NTC_SET2_REGISTER);
    return;
}

bool ncp1851_check_battery_present(void)
{
    struct ncp1851_device_info *di = bq;
    u8 read_reg[18] = {0};

    ncp1851_read_byte(di, &read_reg[0], 0);
    if((read_reg[0] & BATFET_STAT)||(read_reg[0]&NTC_STAT)){
        return 1;
    }else{
        return 0;
    }
    return 1;
}
EXPORT_SYMBOL_GPL(ncp1851_check_battery_present);

static void ncp1851_config_reg_param(struct ncp1851_device_info *di)
{
    di->ntc_en = NTC_DIS;
    di->tchg_rst = TCHG_RST_DIS;
    di->int_mask = INT_UNMASK;
    di->wdto_dis = WDTO_DIS;
    di->chgto_dis = CHGTO_DIS;
    di->pwr_path  = PWR_PATH_EN;
    di->iweak_currentmA = 100;
    di->ctrl_vfet_voltagemV = 3400;
    di->vchred_voltagemV = 200;
    di->ichred_currentmA = 400;
    di->batcold_voltagemV = 1725;
    di->bathot_voltagemV = 525;

    ncp1851_vbat_set_reg(di);
    ncp1851_ctrl1_set_reg(di);
    ncp1851_ctrl2_set_reg(di);
    ncp1851_ibat_set_reg(di);
    ncp1851_misc_set_reg(di);
    ncp1851_stat_msk_set_reg(di);
    ncp1851_ch1_msk_set_reg(di);
    ncp1851_ch2_msk_set_reg(di);
    ncp1851_ntc_set1_reg(di);
    ncp1851_ntc_set2_reg(di);

    return;
}

static void ncp1851_start_usb_charging(struct ncp1851_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_USB;
    di->charge_status  =  POWER_SUPPLY_STATUS_CHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
    di->cin_limit = di->usb_cin_currentmA;
    di->currentmA = di->usb_bat_currentmA;
    di->voltagemV = di->max_voltagemV;
    di->term_currentmA = di->pdata->termination_currentmA;
    di->boost_mode = CHARGE_MODE;
    di->chg_en = CHG_EN;
    di->charger_flag = CHG_EN;
    di->otg_en = OTG_DIS;

    ncp1851_config_reg_param(di);
    dev_info(di->dev, "%s\n"
            "battery current = %d mA\n"
            "battery voltage = %d mV\n"
            "cin_limit_current = %d mA\n"
            , __func__, di->currentmA, di->voltagemV,di->cin_limit);
    if(!di->bat_present){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }
#ifdef NCP1851_POLL_CHARGING
    schedule_delayed_work(&di->ncp1851_charger_work, msecs_to_jiffies(0));
#endif

    return;
}

static void ncp1851_start_ac_charging(struct ncp1851_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_MAINS;
    di->charge_status  =  POWER_SUPPLY_STATUS_CHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
    di->cin_limit = di->ac_cin_currentmA;
    di->currentmA = di->ac_bat_currentmA;
    di->voltagemV = di->max_voltagemV;
    di->term_currentmA = di->pdata->termination_currentmA;
    di->boost_mode = CHARGE_MODE;
    di->chg_en = CHG_EN;
    di->charger_flag = CHG_EN;
    di->otg_en = OTG_DIS;

    ncp1851_config_reg_param(di);
    dev_info(di->dev, "%s\n"
            "battery current = %d mA\n"
            "battery voltage = %d mV\n"
            "cin_limit_current = %d mA\n"
            , __func__, di->currentmA, di->voltagemV,di->cin_limit);
    if(!di->bat_present){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }
#ifdef NCP1851_POLL_CHARGING
    schedule_delayed_work(&di->ncp1851_charger_work, msecs_to_jiffies(0));
#endif

    return;
}

static void ncp1851_stop_charging(struct ncp1851_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
    di->charge_status  = POWER_SUPPLY_STATUS_DISCHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
    di->boost_mode = CHARGE_MODE;
    di->chg_en = CHG_EN;
    di->charger_flag = CHG_EN;
    di->otg_en = OTG_DIS;

    ncp1851_ctrl1_set_reg(di);

#ifdef NCP1851_POLL_CHARGING
    cancel_delayed_work_sync(&di->ncp1851_charger_work);
#endif

    dev_info(di->dev,"ncp1851_stop_charging!\n");
    return;
}

static void ncp1851_start_otg(struct ncp1851_device_info *di)
{
    di->charger_source = POWER_SUPPLY_TYPE_BATTERY;
    di->charge_status  = POWER_SUPPLY_STATUS_DISCHARGING;
    di->charger_health = POWER_SUPPLY_HEALTH_UNKNOWN;
    di->boost_mode = OTG_MODE;
    di->chg_en = CHG_DIS;
    di->charger_flag = CHG_DIS;
    di->otg_en = OTG_EN;

    ncp1851_ctrl1_set_reg(di);
    ncp1851_ctrl2_set_reg(di);
    ncp1851_bst_msk_set_reg(di);

#ifdef NCP1851_POLL_CHARGING
    schedule_delayed_work(&di->ncp1851_charger_work, msecs_to_jiffies(200));
#endif

    dev_info(di->dev,"ncp1851_start_otg!\n");
    return;
}

static void
ncp1851_charger_update_status(struct ncp1851_device_info *di)
{
    int i = 0;
    u8 read_reg[19] = {0};

    di->timer_fault = 0;
    for(i = 0; i< 10; i++)
        ncp1851_read_byte(di, &read_reg[i], i);

    if(di->boost_mode == CHARGE_MODE){/*charge process mode*/

        if((read_reg[0] & 0xF0) == CHARGE_DONE){
            dev_info(di->dev, "charge done\n");
            //di->charge_status = POWER_SUPPLY_STATUS_FULL;
        }
        if((read_reg[7] & VINOVLO_SNS) == VINOVLO_SNS){
            dev_err(di->dev, "charger ovp = 0x%x\n",read_reg[7]);
            di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
            di->charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        }
        if((read_reg[8] & BAT_OV_SNS) == BAT_OV_SNS){
            di->cfg_params = 1;
            dev_err(di->dev, "battery ovp = 0x%x\n",read_reg[8]);
        }
        if((read_reg[5] & WDTO_INT) == WDTO_INT){
           di->timer_fault = 1;
           dev_err(di->dev, "watchdog time out\n");
        }
        if((read_reg[5] & 0x06) == (USBTO_INT|CHGTO_INT)){
            di->cfg_params = 1;
            di->tchg_rst = TCHG_RST_EN;
            dev_err(di->dev, "chg time out\n");
            ncp1851_ctrl1_set_reg(di);
            di->tchg_rst = TCHG_RST_DIS;
        }
        if(((read_reg[0] & 0xF0)== CHARGE_FAULT)&&(di->chg_en == CHG_EN)){
            di->cfg_params = 1;
            dev_err(di->dev, "charger status = 0x%x,reg01 = 0x%x,chg_en = %d\n",
             read_reg[0],read_reg[1],di->chg_en);
        }
        if ((di->timer_fault == 1) || (di->cfg_params == 1)){
            ncp1851_ctrl1_set_reg(di);
            ncp1851_write_byte(di, di->vbat_reg, REG_VBAT_SET_REGISTER);
            ncp1851_write_byte(di, di->ibat_reg, REG_IBAT_SET_REGISTER);
            ncp1851_write_byte(di, di->ctrl2_reg, REG_CTRL2_REGISTER);
            ncp1851_write_byte(di, di->misc_reg, REG_MISC_SET_REGISTER);
            ncp1851_write_byte(di, di->ntc1_reg, REG_NTC_SET1_REGISTER);
            ncp1851_write_byte(di, di->ntc2_reg, REG_NTC_SET2_REGISTER);
            di->cfg_params = 0;
        }
    }else{/*otg boost mode*/
        if((read_reg[6] & VBATLO_INT) == VBATLO_INT){
            dev_err(di->dev, "boost voltage low\n");
        }
        if((read_reg[6] & VBUSOV_INT) == VBUSOV_INT){
            dev_err(di->dev, "boost voltage ovp \n");
        }
        if((read_reg[6] & VBUSILIM_INT) == VBUSILIM_INT){
            dev_err(di->dev, "boost vbus overload\n");
        }
        if(((read_reg[0] & 0xF0) == BOOST_OVER)||
            ((read_reg[0] & 0xF0) == BOOST_FAULT)){
            ncp1851_ctrl1_set_reg(di);
        }
    }
    return;
}

/*===========================================================================
  Function:       ncp1851_charger_work
  Description:    send a notifier event to sysfs
  Input:          struct work_struct *
  Output:         none
  Return:         none
  Others:         none
===========================================================================*/
static void ncp1851_charger_work(struct work_struct *work)
{
    struct ncp1851_device_info *di = container_of(work,
        struct ncp1851_device_info, ncp1851_charger_work.work);

    ncp1851_charger_update_status(di);

#ifdef NCP1851_POLL_CHARGING
    schedule_delayed_work(&di->ncp1851_charger_work, NCP1851_TIMER_TIMEOUT *HZ);
#endif
    return;
}

static irqreturn_t ncp1851_charger_irq(int irq, void *_di)
{
    struct ncp1851_device_info *di = _di;

    schedule_delayed_work(&di->ncp1851_charger_work, 0);

    return IRQ_HANDLED;
}

static int ncp1851_init_irq(struct ncp1851_device_info *di)
{
    int ret = 0;

#ifdef NCP1851_POLL_CHARGING
    return 0;
#endif

    if (gpio_request(di->client->irq, "ncp1851_irq") < 0){
        ret = -ENOMEM;
        goto irq_fail;
    }

    gpio_direction_input(di->client->irq);

    /* request irq interruption */
    ret = request_irq(gpio_to_irq(di->client->irq), ncp1851_charger_irq,
        IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING| IRQF_SHARED,"ncp1851_irq",di);
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


static ssize_t ncp1851_set_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->chg_en= val;
    di->charger_flag = val;
    ncp1851_ctrl1_set_reg(di);

    return count;
}

static ssize_t ncp1851_show_enable_charger(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    val = di->chg_en;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t ncp1851_set_watchdog(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    di->wdto_dis= val^1;
    ncp1851_ctrl2_set_reg(di);

    return count;
}

static ssize_t ncp1851_show_watchdog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    val = di->wdto_dis;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t ncp1851_set_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 100) ||
        (val > 1500))
        return -EINVAL;

    di->cin_limit = val;
    ncp1851_misc_set_reg(di);

    return count;
}

static ssize_t ncp1851_show_cin_limit(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    val = di->cin_limit;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t ncp1851_set_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 3300) ||
        (val > 4500))
        return -EINVAL;

    di->voltagemV = val;
    ncp1851_ctrl1_set_reg(di);

    return count;
}

static ssize_t ncp1851_show_regulation_voltage(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    val = di->voltagemV;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t ncp1851_set_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 400) ||
        (val > 1600))
        return -EINVAL;

    di->currentmA = val;
    ncp1851_ibat_set_reg(di);

    return count;
}

static ssize_t ncp1851_show_charge_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    val = di->currentmA;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t ncp1851_set_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 100) || (val > 275))
        return -EINVAL;

    di->term_currentmA = val;
    ncp1851_ibat_set_reg(di);

    return count;
}

static ssize_t ncp1851_show_termination_current(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    unsigned long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    val = di->term_currentmA;
    return sprintf(buf, "%lu\n", val);
}

static ssize_t ncp1851_set_charging(struct device *dev, struct device_attribute *attr,
                       const char *buf, size_t count)
{
    int status = count;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);

    if (strncmp(buf, "startac", 7) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_USB)
            ncp1851_stop_charging(di);
        ncp1851_start_ac_charging(di);
    } else if (strncmp(buf, "startusb", 8) == 0) {
        if (di->charger_source == POWER_SUPPLY_TYPE_MAINS)
            ncp1851_stop_charging(di);
        ncp1851_start_usb_charging(di);
    } else if (strncmp(buf, "stop" , 4) == 0) {
        ncp1851_stop_charging(di);
    } else {
        return -EINVAL;
    }

    return status;
}

static ssize_t ncp1851_show_chargelog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    //unsigned long val;
    struct ncp1851_device_info *di = dev_get_drvdata(dev);
    u8 read_reg[19] = {0},buf_temp[19] = {0};
    int i =0;
    for(i=0;i<19;i++){
        ncp1851_read_byte(di, &read_reg[i], i);
    }
    for(i=0;i<19;i++){
        sprintf(buf_temp,"%-5.2x",read_reg[i]);
        strcat(buf,buf_temp);
    }
    strcat(buf,"\n");
    return strlen(buf);
}


static DEVICE_ATTR(enable_charger, S_IWUSR | S_IRUGO,
                ncp1851_show_enable_charger,
                ncp1851_set_enable_charger);
static DEVICE_ATTR(watchdog, S_IWUSR | S_IRUGO,
                ncp1851_show_watchdog,
                ncp1851_set_watchdog);
static DEVICE_ATTR(cin_limit, S_IWUSR | S_IRUGO,
                ncp1851_show_cin_limit,
                ncp1851_set_cin_limit);
static DEVICE_ATTR(regulation_voltage, S_IWUSR | S_IRUGO,
                ncp1851_show_regulation_voltage,
                ncp1851_set_regulation_voltage);
static DEVICE_ATTR(charge_current, S_IWUSR | S_IRUGO,
                ncp1851_show_charge_current,
                ncp1851_set_charge_current);
static DEVICE_ATTR(termination_current, S_IWUSR | S_IRUGO,
                ncp1851_show_termination_current,
                ncp1851_set_termination_current);

static DEVICE_ATTR(charging, S_IWUSR | S_IRUGO, NULL, ncp1851_set_charging);
static DEVICE_ATTR(chargelog, S_IWUSR | S_IRUGO, ncp1851_show_chargelog, NULL);

static struct attribute *ncp1851_attributes[] = {

    &dev_attr_enable_charger.attr,
    &dev_attr_watchdog.attr,
    &dev_attr_cin_limit.attr,
    &dev_attr_regulation_voltage.attr,
    &dev_attr_charge_current.attr,
    &dev_attr_termination_current.attr,
    &dev_attr_charging.attr,
    &dev_attr_chargelog.attr,
    NULL,
};

static const struct attribute_group ncp1851_attr_group = {
    .attrs = ncp1851_attributes,
};

static int ncp1851_set_charging_enable(bool enable)
{
    struct ncp1851_device_info *di = bq;

    if(!di){
        pr_err("%s: ncp1851 not init\n", __func__);
        return -EINVAL;
    }

#if defined(NCP1851_DEBUG)
        return 0;
#endif

    if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY)||(!di->bat_present))
        return 0;

    if(enable){
        di->chg_en= CHG_EN;
    }else{
        di->chg_en= CHG_DIS;
    }
    di->chg_en&= di->charger_flag;
    ncp1851_ctrl1_set_reg(di);

    return 0;
}
static int ncp1851_set_battery_exist(bool battery_exist)
{
    struct ncp1851_device_info *di = bq;

    if(!di){
        pr_err("%s: ncp1851 not init\n", __func__);
        di->bat_present = 1;
        return -EINVAL;
    }
    di->bat_present = battery_exist;
    if(!di->bat_present){
        dev_err(di->dev, "battery is not present! \n");
    }
    return(0);
}

static int ncp1851_set_charging_source(int charging_source)
{
    struct ncp1851_device_info *di = bq;

    if(!di){
        pr_err("%s: ncp1851 not init\n", __func__);
        return -EINVAL;
    }
    switch(charging_source){
        case LCUSB_EVENT_PLUGIN:
            di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
            ncp1851_vbat_set_reg(di);
            ncp1851_ctrl1_set_reg(di);
            break;
        case LCUSB_EVENT_VBUS:
            ncp1851_start_usb_charging(di);
            break;
        case LCUSB_EVENT_CHARGER:
            ncp1851_start_ac_charging(di);
            break;
        case LCUSB_EVENT_NONE:
            ncp1851_stop_charging(di);
            break;
        case LCUSB_EVENT_OTG:
            ncp1851_start_otg(di);
            break;
        default:
            break;
    }
    return 0;
}

/*monitor battery temp limit chargingm;1=enable,0=disable*/
static int ncp1851_set_charging_temp_stage(int temp_status,int volt,bool enable)
{
    struct ncp1851_device_info *di = bq;
    u8 read_reg[10] = {0};

    if(!di){
        pr_err("%s: ncp1851 not init\n", __func__);
        return -EINVAL;
    }

#if defined(NCP1851_DEBUG)
        return 0;
#endif

    if((di->charger_source == POWER_SUPPLY_TYPE_BATTERY))
        return 0;
    if(enable){
        if(di->charger_source == POWER_SUPPLY_TYPE_USB){
            switch(temp_status){
            case TEMP_STAGE_OVERCOLD:
            case TEMP_STAGE_COLD_ZERO:
                di->chg_en = CHG_DIS;
                break;
            case TEMP_STAGE_ZERO_COOL:
            case TEMP_STAGE_COOL_NORMAL:
                di->chg_en = CHG_EN;
                break;
            case TEMP_STAGE_NORMAL_HOT:
                if(volt > 4100){
                    di->chg_en = CHG_DIS;
                }else if (volt < 4000){
                    di->chg_en = CHG_EN;
                }else{
                    di->chg_en = di->chg_en;
                }
                break;
                case TEMP_STAGE_HOT:
                    di->chg_en = CHG_DIS;
                break;
                default:
                    break;
            }
        }else if(di->charger_source == POWER_SUPPLY_TYPE_MAINS){
            switch(temp_status){
            case TEMP_STAGE_OVERCOLD:
            case TEMP_STAGE_COLD_ZERO:
                di->chg_en = CHG_DIS;
                break;
            case TEMP_STAGE_ZERO_COOL:
                //di->chg_en = CHG_EN;
                //di->cin_limit = di->ac_cin_currentmA;
                //di->currentmA = di->usb_bat_currentmA;
                //break;
            case TEMP_STAGE_COOL_NORMAL:
                di->chg_en = CHG_EN;
                di->cin_limit = di->ac_cin_currentmA;
                di->currentmA = di->ac_bat_currentmA;
                break;
            case TEMP_STAGE_NORMAL_HOT:
                if(volt > 4100){
                    di->chg_en = CHG_DIS;
                }else if (volt < 4000){
                    di->chg_en = CHG_EN;
                }else{
                    di->chg_en = di->chg_en;
                }
                di->cin_limit = di->usb_cin_currentmA;
                di->currentmA = di->usb_bat_currentmA;
                break;
            case TEMP_STAGE_HOT:
                di->chg_en = CHG_DIS;
                di->cin_limit = di->ac_cin_currentmA;
                di->currentmA = di->ac_bat_currentmA;
                break;
            default:
                break;
            }
        }
    }else{
        di->chg_en = CHG_EN;
    }
    di->chg_en&= di->charger_flag;
    ncp1851_ctrl1_set_reg(di);
    ncp1851_ibat_set_reg(di);
    ncp1851_misc_set_reg(di);
    ncp1851_read_byte(di, &read_reg[0], 0);
    ncp1851_read_byte(di, &read_reg[7], 7);
    if((!di->chg_en)||((read_reg[0] & 0xF0) == CHARGE_FAULT)||(!di->bat_present)){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    }else{
        di->charge_status = POWER_SUPPLY_STATUS_CHARGING;
    }
    if((read_reg[7] & VINOVLO_SNS) == VINOVLO_SNS){
        di->charge_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        di->charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
    }else{
        di->charger_health = POWER_SUPPLY_HEALTH_GOOD;
    }
    if ((read_reg[0] & CHARGE_DONE) == CHARGE_DONE){
        di->charge_status = POWER_SUPPLY_STATUS_FULL;
    }
    return 0;
}

static int ncp1851_get_charging_status(void)
{
    struct ncp1851_device_info *di = bq;

    if(!di){
        pr_err("%s: ncp1851 not init\n", __func__);
        return POWER_SUPPLY_STATUS_UNKNOWN;
    }

    return di->charge_status;
}

static int ncp1851_get_charger_health(void)
{
    struct ncp1851_device_info *di = bq;

    if(!di){
        pr_err("%s: ncp1851 not init\n", __func__);
        return POWER_SUPPLY_HEALTH_UNKNOWN;
    }

    return di->charger_health;
}

struct extern_charger_chg_config extern_charger_chg ={
    .name = "ncp1851",
    .set_charging_source = ncp1851_set_charging_source,
    .set_charging_enable = ncp1851_set_charging_enable,
    .set_battery_exist = ncp1851_set_battery_exist,
    .set_charging_temp_stage = ncp1851_set_charging_temp_stage,
    .get_charging_status = ncp1851_get_charging_status,
    .get_charger_health = ncp1851_get_charger_health,
};


static int ncp1851_charger_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct ncp1851_device_info *di;
    int ret = 0;

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
    bq = di;
    bq->client = di->client;
    bq->dev = di->dev;

    di->max_voltagemV     = di->pdata->max_charger_voltagemV;
    di->ac_cin_currentmA  = di->pdata->ac_cin_limit_currentmA;
    di->usb_cin_currentmA = di->pdata->usb_cin_limit_currentmA;
    di->ac_bat_currentmA  = di->pdata->ac_battery_currentmA;
    di->usb_bat_currentmA = di->pdata->usb_battery_currentmA;
    di->term_currentmA    = di->pdata->termination_currentmA;
    di->cin_limit         = di->usb_cin_currentmA;
    di->voltagemV         = di->max_voltagemV;
    di->currentmA         = di->usb_bat_currentmA;
    di->bat_present = 1;
    di->cfg_params = 0;
    di->chg_en = CHG_EN;
    di->charger_flag = CHG_EN;
    di->boost_mode = CHARGE_MODE;
    ncp1851_config_reg_param(di);

    wake_lock_init(&di->chrg_lock, WAKE_LOCK_SUSPEND, "chrg_wake_lock");

    INIT_DELAYED_WORK(&di->ncp1851_charger_work,ncp1851_charger_work);

    ret = ncp1851_init_irq(di);
    if(ret){
        dev_err(&client->dev, "irq init failed\n");
        goto irq_fail;
    }

    ret = sysfs_create_group(&client->dev.kobj, &ncp1851_attr_group);
    if (ret)
        dev_err(&client->dev, "could not create sysfs files\n");

    dev_info(&client->dev, "ncp1851 Driver Probe Success\n");

    return 0;

irq_fail:
    free_irq(gpio_to_irq(di->client->irq), di);
    wake_lock_destroy(&di->chrg_lock);
err_kfree:
    kfree(di);

    return ret;
}

static int ncp1851_charger_remove(struct i2c_client *client)
{
    struct ncp1851_device_info *di = i2c_get_clientdata(client);

    wake_lock_destroy(&di->chrg_lock);

    cancel_delayed_work_sync(&di->ncp1851_charger_work);

    sysfs_remove_group(&client->dev.kobj, &ncp1851_attr_group);
    free_irq(gpio_to_irq(di->client->irq), di);

    kfree(di);

	return 0;
}

static void ncp1851_charger_shutdown(struct i2c_client *client)
{
    struct ncp1851_device_info *di = i2c_get_clientdata(client);

    ncp1851_stop_charging(di);

    return;
}

static const struct i2c_device_id ncp1851_id[] = {
    { "ncp1851", 0 },
    {},
};

#ifdef CONFIG_PM
static int ncp1851_charger_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct ncp1851_device_info *di = platform_get_drvdata(pdev);

    /* reset 32 second timer */
#ifdef NCP1851_POLL_CHARGING
    cancel_delayed_work_sync(&di->ncp1851_charger_work);
#endif
    ncp1851_ctrl1_set_reg(di);
    return 0;
}

static int ncp1851_charger_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct ncp1851_device_info *di = platform_get_drvdata(pdev);

    ncp1851_vbat_set_reg(di);
#ifdef NCP1851_POLL_CHARGING
    if (di->charger_source >= POWER_SUPPLY_TYPE_MAINS)
        schedule_delayed_work(&di->ncp1851_charger_work, 0);
#endif
    return 0;
}
#else
#define ncp1851_charger_suspend NULL
#define ncp1851_charger_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops pm_ops = {
    .suspend    = ncp1851_charger_suspend,
    .resume     = ncp1851_charger_resume,
};

static struct i2c_driver ncp1851_charger_driver = {
    .probe      = ncp1851_charger_probe,
    .remove     = ncp1851_charger_remove,
    .shutdown   = ncp1851_charger_shutdown,
    .id_table   = ncp1851_id,
    .driver     = {
        .name   = "ncp1851_charger",
        .pm = &pm_ops,
    },
};

static int __init ncp1851_charger_init(void)
{
    return i2c_add_driver(&ncp1851_charger_driver);
}
module_init(ncp1851_charger_init);

static void __exit ncp1851_charger_exit(void)
{
    i2c_del_driver(&ncp1851_charger_driver);
}
module_exit(ncp1851_charger_exit);

MODULE_DESCRIPTION("onsemi ncp1851 charger driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("leadcore tech");
