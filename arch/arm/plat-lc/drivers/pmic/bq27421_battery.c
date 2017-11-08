/*
 * drivers/power/bq27421_battery.c
 *
 * BQ27421 gasgauge driver
 *
 * Copyright (C) 2010-2015 Texas Instruments, Inc.
 * Author: Texas Instruments, Inc.
 * Based on a previous work by Copyright (C) huawei, Inc.
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
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <asm/unaligned.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <plat/bq27421_battery.h>
#include <plat/comip-battery.h>
#include <linux/syscalls.h>
#include <linux/alarmtimer.h>

/* If the system has several batteries we need
 * a different name for each of them*/
static DEFINE_IDR(bq27421_battery_id);

static DEFINE_MUTEX(bq27421_battery_mutex);
static DEFINE_MUTEX(data_flash_mutex);

static struct wake_lock low_battery_wakelock;
static struct wake_lock bqfs_wakelock;

static struct i2c_driver bq27421_battery_driver;

struct bq27421_device_info *bq27421_device;
struct i2c_client *bq27421_i2c_client;
static int battery_id_status = 0;

enum{
    BQ27421_NORMAL_MODE,
    BQ27421_UPDATE_FIRMWARE,
    BQ27421_LOCK_MODE
};

struct bq27421_context
{
    unsigned int chiptemperature;
    unsigned int temperature;
    unsigned int rtemp;
    unsigned int capacity;
    unsigned int volt;
    short bat_current;
    unsigned int battery_rm;
    unsigned int battery_fcc;
    unsigned int battery_present;
    unsigned int battery_health;
    unsigned int battery_rmcu;
    unsigned int battery_rmcf;
    unsigned int battery_fccu;
    unsigned int battery_fccf;
    unsigned int battery_socu;
    unsigned int battery_nac;
    unsigned int battery_fac;
    short battery_ap;
    short battery_mli;
    unsigned char state;
    unsigned long locked_timeout_jiffies;//after updating firmware, device can not be accessed immediately
    unsigned int i2c_error_count;
    unsigned int lock_count;
    unsigned int normal_capacity;
    unsigned int firmware_version;
    unsigned int design_capacity;      /*design capacity*/
    unsigned int design_energy;
    unsigned int taper_rate;        /* terminate current*/
    unsigned int terminate_voltage; /*shutdown voltage*/
    unsigned int v_chg_term;    /*charge voltage*/
    unsigned int taper_voltage;    /*charge voltage*/
    unsigned int opconfig;    /*intr set*/
    unsigned int hibernate_v;
    unsigned int fh_0;
    unsigned int fh_1;
    unsigned int fh_2;
    unsigned int fh_3;
    unsigned int soc1;
    unsigned int soc1c;
    unsigned int socf;
    unsigned int socfc;
    unsigned int termV_valid_t;
    unsigned int over_temp;
    unsigned int slpcurrent;
};

static struct bq27421_context gauge_context =
{
    .chiptemperature = 2930,
    .temperature = 2930, //20 degree
    .rtemp     = 25,
    .capacity = 50,      //50 percent
    .volt = 3800,        // 3.8 V
    .bat_current = 200,  // 200 mA
    .battery_rm = 800,   //mAH
    .battery_fcc = 1600, //mAH
    .battery_present = 1,
    .battery_health = POWER_SUPPLY_HEALTH_GOOD,
    .battery_mli = 200,
    .battery_rmcu = 800,
    .battery_rmcf = 1600,
    .battery_fccu = 800,
    .battery_fccf = 1600,
    .battery_socu = 50,
    .battery_nac = 800,
    .battery_fac = 1600,
    .battery_ap = 50,
    .state = BQ27421_NORMAL_MODE,
    .i2c_error_count = 0,
    .normal_capacity = 2000,
    .firmware_version = 0x3142,
/*bq27541 subclass id params*/
    .design_capacity = 2200,
    .design_energy = 8360,
    .taper_rate = 147,
    .terminate_voltage = 3400,
    .v_chg_term = 4350,
    .taper_voltage = 4250,
    .opconfig = 0x35FC,//enable BIE and pull up resistor,0xFC is select BAT_LOW function,and 0xF8 select soc_intr
    .hibernate_v = 2200,
    .fh_0 = 60,
    .fh_1 = 100,
    .fh_2 = 18000,
    .fh_3 = 25,
    .soc1 = 4,
    .soc1c = 5,
    .socf = 1,
    .socfc = 3,
    .termV_valid_t = 2,
    .over_temp = 620,
    .slpcurrent = 3,
};

static int bq27421_get_battery_temperature(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int temp_C = 25;
    int adc_code = 0,i = 0, adc[2]={0};
    int sum = 0;

    if(!di->pdata->tmptblsize){
        goto exit;
    }

    for(i=0;i< 2;i++){
        pmic_get_adc_conversion(1);
        udelay(2);
    }

    for(i=0;i< 2;i++){
        adc[i] = pmic_get_adc_conversion(1);
        sum = sum+adc[i];
    }
    adc_code = sum/2;
    if((adc_code > 2500)||(adc_code < 150)){
        dev_err(di->dev, " adc val = %d\n", adc_code);
        goto exit;
    }

    for (temp_C = 0; temp_C < di->pdata->tmptblsize; temp_C++) {
        if (adc_code >= di->pdata->battery_tmp_tbl[temp_C])
            break;
    }

    /* first 20 values are for negative temperature */
    temp_C = (temp_C - 20); /* degree Celsius */
    if(temp_C > 67){
        dev_err(di->dev, "battery temp over %d degree (adc=%d)\n", temp_C,adc_code);
        temp_C = 70;
    }
exit:
    return temp_C;
}

bool bq27421_get_gasgauge_normal_capacity(unsigned int* design_capacity)
{
    bool ret = 0;

    switch(battery_id_status){
    case BAT_ID_1:
    case BAT_ID_2:
    case BAT_ID_3:
    case BAT_ID_4:
    case BAT_ID_5:
    case BAT_ID_7:
        gauge_context.design_capacity = 2000;
        gauge_context.design_energy = 7600;
        gauge_context.v_chg_term = 4350;/*charging limit voltage*/
        break;
    case BAT_ID_8:
    case BAT_ID_9:
    case BAT_ID_10:
        gauge_context.design_capacity = 2200;
        gauge_context.design_energy = 8360;
        gauge_context.v_chg_term = 4350;
        break;
    case BAT_ID_NONE:
    default:
        gauge_context.design_capacity = 2200;
        gauge_context.design_energy = 8360;
        gauge_context.v_chg_term = 4350;/*charging limit voltage*/
        break;
    }

    gauge_context.taper_voltage = gauge_context.v_chg_term - 100;/*rechg voltage*/
    *design_capacity = gauge_context.design_capacity;
    return ret;
}

static bool bq27421_get_gasgauge_firmware_version_id(unsigned int* version_config)
{
    bool ret = 0;

    switch(battery_id_status){
    case BAT_ID_1:
    case BAT_ID_2:
    case BAT_ID_3:
    case BAT_ID_4:
    case BAT_ID_5:
    case BAT_ID_7:
    case BAT_ID_8:
    case BAT_ID_9:
    case BAT_ID_10:
    case BAT_ID_NONE:
    default:
        gauge_context.firmware_version = 0x3142;

        break;
    }
    *version_config = gauge_context.firmware_version;
    return ret;
}

static int bq27421_get_battery_id_type(struct bq27421_device_info *di)
{
    int version_config = 0,design_capacity = 0;

    battery_id_status = comip_battery_id_type();
    bq27421_get_gasgauge_firmware_version_id(&version_config);
    bq27421_get_gasgauge_normal_capacity(&design_capacity);

    dev_info(di->dev, "bat_name = %s \n",comip_battery_name(battery_id_status));

    return battery_id_status;
}



#define GAS_GAUGE_I2C_ERR_STATICS() ++gauge_context.i2c_error_count
#define GAS_GAUGE_LOCK_STATICS() ++gauge_context.lock_count

static int bq27421_is_accessible(void)
{
    if(gauge_context.state == BQ27421_UPDATE_FIRMWARE){
        BQ27421_DBG("bq27421 isn't accessible,It's updating firmware!\n");
        GAS_GAUGE_LOCK_STATICS();
        return 0;
    } else if(gauge_context.state == BQ27421_NORMAL_MODE){
        return 1;
    }else { //if(gauge_context.state == BQ27421_LOCK_MODE)
        if(time_is_before_jiffies(gauge_context.locked_timeout_jiffies)){
            gauge_context.state = BQ27421_NORMAL_MODE;
            return 1;
        }else{
            BQ27421_DBG("bq27421 isn't accessible after firmware updated immediately!\n");
            return 0;
        }
    }
}


static int bq27421_i2c_read_byte_data(struct bq27421_device_info *di, u8 reg)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27421_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        udelay(20);
        ret = i2c_smbus_read_byte_data(di->client, reg);
        if (ret < 0) {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27421_ERR("[%s,%d] i2c_smbus_read_byte_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&bq27421_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c read at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}

static int bq27421_i2c_write_byte_data(struct i2c_client *client, u8 reg, u8 value)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27421_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        udelay(20);
        ret = i2c_smbus_write_byte_data(client, reg, value);
        if (ret < 0)  {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27421_ERR("[%s,%d] i2c_smbus_write_word_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&bq27421_battery_mutex);

    if(ret < 0){
        dev_err(&client->dev,
        "%s: i2c write at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}

static int bq27421_i2c_read_word_data(struct bq27421_device_info *di, u8 reg)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27421_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        msleep(5);
        ret = i2c_smbus_read_word_data(di->client, reg);
        if (ret < 0) {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27421_ERR("[%s,%d] i2c_smbus_read_byte_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c read at address 0x%02x failed\n",__func__, reg);
    }
    mutex_unlock(&bq27421_battery_mutex);
    return ret;
}

static int bq27421_i2c_write_word_data(struct bq27421_device_info *di, u8 reg, u16 value)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27421_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        msleep(5);
        ret = i2c_smbus_write_word_data(di->client, reg, value);
        if (ret < 0)  {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27421_ERR("[%s,%d] i2c_smbus_write_word_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c write at address 0x%02x failed\n",__func__, reg);
    }
    mutex_unlock(&bq27421_battery_mutex);
    return ret;
}

static int bq27421_i2c_block_read(struct i2c_client *client, u8 reg, u8 *pBuf, u16 len)
{
    int ret = 0;
    int i = 0,j = 0;
    u8 *p = pBuf;

    mutex_lock(&bq27421_battery_mutex);
    for (i = 0; i < len; i += I2C_SMBUS_BLOCK_MAX) {
        udelay(10);
        j = ((len - i) > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : (len - i);
        ret = i2c_smbus_read_i2c_block_data(client, reg+i, j, p+i);
        if (ret < 0) {
            GAS_GAUGE_I2C_ERR_STATICS();
            BQ27421_ERR("[%s,%d] i2c_transfer failed\n", __FUNCTION__, __LINE__);
            break;
        }
    }
    mutex_unlock(&bq27421_battery_mutex);

    return ret;
}

static int bq27421_i2c_block_write(struct i2c_client *client, u8 reg, u8 *pBuf, u16 len)
{
    int ret = 0;
    int i = 0,j = 0;
    u8 *p = pBuf;

    mutex_lock(&bq27421_battery_mutex);
    for (i = 0; i < len; i += I2C_SMBUS_BLOCK_MAX) {
        udelay(10);
        j = ((len - i) > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : (len - i);
        ret = i2c_smbus_write_i2c_block_data(client, reg+i, j, p+i);
        if (ret < 0)  {
            GAS_GAUGE_I2C_ERR_STATICS();
            BQ27421_ERR("[%s,%d] i2c_transfer failed\n", __FUNCTION__, __LINE__);
            break;
        }
    }
    mutex_unlock(&bq27421_battery_mutex);

    return ret;
}

static int bq27421_i2c_bytes_read_and_compare(struct i2c_client *client, u8 reg, 
                                                u8 *pSrcBuf, u8 *pDstBuf, u16 len)
{
    int ret = 0;

    ret = bq27421_i2c_block_read(client, reg, pSrcBuf, len);
    if (ret < 0) {
        GAS_GAUGE_I2C_ERR_STATICS();
        BQ27421_ERR("[%s,%d] bq27421_i2c_bytes_read failed\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = strncmp(pDstBuf, pSrcBuf, len);

    return ret;
}


static int bq27421_set_access_dataclass(struct bq27421_device_info *di,u8 subclass, u8 block_num)
{
    int ret = 0;
    //u8 block_num= offset / 32;

    /* setup fuel gauge state data block block for ram access */
    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCNTL, 0x00);//0x61
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFCLS, subclass);//0x3E
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFBLK, block_num);//0x3F
    if (ret < 0){
        goto exit;
    }
    return 0;
exit:
    return ret;
}

static int bq27421_access_data_flash(struct bq27421_device_info *di,
                        u8 subclass,u8 offset, u8 *buf, int len, bool write)
{
    int i = 0;
    unsigned sum= 0;
    u8 cmd[33];
    u8 block_num= offset / 32;
    u8 cmd_code = BQ27421_REG_DFD + offset % 32;
    u8 checksum = 0;
    int ret = 0;

    mutex_lock(&data_flash_mutex);

    /* setup fuel gauge state data block block for ram access */
    cmd[0] = BQ27421_REG_DFDCNTL;/*0x60*/
    cmd[1] = 0x00;
    ret = bq27421_i2c_block_write(di->client,cmd[0],&cmd[1],2);
    if (ret < 0){
        goto exit;
    }

    /* Set subclass */
    cmd[0] = BQ27421_REG_DFCLS;/*0x3E*/
    cmd[1] = subclass;
    //ret = bq27421_i2c_block_write(di->client,cmd[0],&cmd[1],2);
    ret = bq27421_i2c_write_byte_data(di->client,cmd[0],cmd[1]);
    if (ret < 0){
        goto exit;
    }

    if(block_num >= 0) {
        /* Set block to R/W */
        cmd[0] = BQ27421_REG_DFBLK;/*0x3F*/
        cmd[1] = block_num;
        ret = bq27421_i2c_block_write(di->client,cmd[0],&cmd[1],2);
        //ret = bq27421_i2c_write_byte_data(di->client,cmd[0],cmd[1]);
        if (ret < 0){
            goto exit;
        }
    }

    msleep(2);
    if (write) {
        /* Calculate checksum */
        for (i = 0; i < len; i++){
            sum += buf[i];
        }
        checksum = 0xFF - (sum & 0x00FF);
        /* Write data */
        cmd[0] = cmd_code;
        memcpy((cmd + 1), buf, len);
        ret = bq27421_i2c_block_write(di->client,cmd[0],&cmd[1],(len + 1));
        if (ret < 0){
            goto exit;
        }

        /* Set checksum */
        cmd[0] = BQ27421_REG_DFDCKS;
        cmd[1] = checksum;
        ret = bq27421_i2c_block_write(di->client,cmd[0],&cmd[1],2);
        if (ret < 0){
            goto exit;
        }
    } else {
        /* Read data */
        ret = bq27421_i2c_block_read(di->client, cmd_code, buf, len);
        if (ret < 0){
            goto exit;
        }
    }
    mutex_unlock(&data_flash_mutex);
    return 0;
exit:
    mutex_unlock(&data_flash_mutex);
    return ret;
}

/*===========================================================================
  Function:       bq27421_write_control
  Description:    get qmax
  Input:          struct bq27421_device_info *
  Output:         none
  Return:         value of
  Others:         none
===========================================================================*/
static int bq27421_write_control(struct bq27421_device_info *di,int subcmd)
{
    int data = 0;

    data = bq27421_i2c_write_word_data(di,BQ27421_REG_CTRL,subcmd);
    if(data < 0){
        dev_dbg(&di->client->dev,
            "Failed write sub command:0x%x. data=%d\n", subcmd, data);
    }
    dev_dbg(&di->client->dev, "%s() subcmd=0x%x data=%d\n",__func__, subcmd, data);

    return ((data < 0)? data:0);
}

/*===========================================================================
  Function:       bq27421_get_control_status
  Description:    get qmax
  Input:          struct bq27421_device_info *
  Output:         none
  Return:         value of
  Others:         none
===========================================================================*/
static int bq27421_get_control_status(struct bq27421_device_info *di,int subcmd)
{
    int data = 0;

    data = bq27421_write_control(di,subcmd);
    if(data < 0){
        goto exit;
    }
    mdelay(2);
    data = bq27421_i2c_read_word_data(di,BQ27421_REG_CTRL);
    if(data < 0){
        dev_err(&di->client->dev,"Failed to read control reg.data = %d\n",data);
    }
exit:
    return (data);
}

static int bq27421_init_complete(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    data = bq27421_get_control_status(di,BQ27421_SUBCMD_CONTROL_STATUS);
    if(data < 0)
        return 1;

    return (!!(data & 0x80));
}
/*===========================================================================
  Function:       bq27421_check_if_sealed
  Description:    check bq27421 is sealed
===========================================================================*/
static bool bq27421_check_if_sealed(struct bq27421_device_info *di)
{
    int control_status = 0;

    msleep(2);
    control_status = bq27421_get_control_status(di,BQ27421_SUBCMD_CONTROL_STATUS);
    if(control_status < 0){
        di->sealed = -1;
        return 0;
    }
    di->sealed = !!(control_status & BQ27421_SEALED_MASK);

    return di->sealed;
}
/*===========================================================================
  Function:       bq27421_unseal
  Description:    Unseal the fuel gauge for data access
===========================================================================*/
static int bq27421_unseal(struct bq27421_device_info *di)
{
    int rc = 0;
    int last_cmd = 0;

    /* Unseal the fuel gauge for data access */
    rc = bq27421_write_control(di,BQ27421_SUBCMD_ENTER_CLEAR_SEAL);
    if (rc < 0) {
        last_cmd = BQ27421_SUBCMD_ENTER_CLEAR_SEAL;
        goto unseal_exit;
    }
    msleep(2);
    rc = bq27421_write_control(di,BQ27421_SUBCMD_CLEAR_SEALED);
    if (rc < 0) {
        last_cmd = BQ27421_SUBCMD_CLEAR_SEALED;
        goto unseal_exit;
    }
    msleep(2);

    return 0;

unseal_exit:
    di->sealed = -1;
    dev_err(&di->client->dev, "Failed write unseal command :%#x\n",last_cmd);
    return rc;
}

/*===========================================================================
  Function:       bq27421_seal
  Description:    seal the fuel gauge
===========================================================================*/
static int bq27421_seal(struct bq27421_device_info *di)
{
    int rc = 0;
    int last_cmd = 0;

    /* seal the fuel gauge */
    if(di->sealed == 1){
        //rc = bq27421_write_control(di,BQ27421_SUBCMD_SEALED);
        if (rc < 0) {
            last_cmd = BQ27421_SUBCMD_SEALED;
            goto seal_exit;
        }
        di->sealed = 0;
        msleep(2);
        dev_dbg(&di->client->dev, "sealed gaguge ok!\n");
    }

    return 0;

seal_exit:
    di->sealed = -1;
    dev_err(&di->client->dev, "Failed write seal command :%#x\n",last_cmd);
    return rc;
}
/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
int bq27421_battery_voltage(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return gauge_context.volt;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_VOLT);
    if ((data < 0)||(data > 6000)) {
        dev_err(di->dev, "error %d reading voltage\n", data);
        data = gauge_context.volt;
    } else{
        gauge_context.volt = data;
    }

    BQ27421_DBG("read voltage result = %d mVolts\n", data);
    return gauge_context.volt ;
}

/*
 * Return the battery average current
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
short bq27421_battery_current(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;
    short nCurr = 0;

    if(!bq27421_is_accessible())
        return gauge_context.bat_current;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_AI);

    if (data < 0) {
        dev_err(di->dev, "error %d reading current\n", data);
        data = gauge_context.bat_current;
    } else{
        gauge_context.bat_current = (signed short)data;
    }

    nCurr = (signed short)data;

    BQ27421_DBG("read current result = %d mA\n", nCurr);

    return gauge_context.bat_current;
}

/*
 * Return the battery Relative State-of-Charge
 * The reture value is 0 - 100%
 * Or < 0 if something fails.
 */
int bq27421_battery_capacity(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0,ret = 0;

    if(!bq27421_is_accessible())
        return gauge_context.capacity;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_SOC);
    if((data == 0)&&(gauge_context.volt >= 3650)){
        bq27421_battery_rm();
        if(((gauge_context.volt >= 3650)&&(gauge_context.battery_rm < 10)&&(gauge_context.rtemp >=0)&&(gauge_context.bat_current < 50))
                ||(gauge_context.battery_fcc < 500)){
            dev_err(di->dev, "err soc = %d,rm =%d,fcc =%d,volt = %d\n",
                    data,gauge_context.battery_rm,gauge_context.battery_fcc,gauge_context.volt);
            if(bq27421_check_if_sealed(di)){
                dev_info(di->dev, "sealed device\n");
                ret = bq27421_unseal(di);
                if(0 != ret){
                    dev_err(di->dev, "Can not unseal device\n");
                }
            }
            msleep(5);
            bq27421_write_control(di,BQ27421_SUBCMD_SOFT_RESET);
            mdelay(2000);
            data = bq27421_i2c_read_word_data(di,BQ27421_REG_SOC);
        }
    }
    if((data < 0)||(data > 100)) {
        dev_err(di->dev, "error %d reading capacity\n", data);
        data = gauge_context.capacity;
    }else{
        gauge_context.capacity = data;
    }
    BQ27421_DBG("read soc result = %d Hundred Percents\n", data);

    return gauge_context.capacity;
}

static int bq27421_battery_realsoc(void)
{
    return gauge_context.capacity;
}

/*correction recharging soc display*/
int bq27421_battery_soc(void)
{
    int data = 0;

    data = bq27421_battery_capacity();
    data = (data * 1022)/1000;
    if(data > 100){
        data = 100;
    }

    return data;
}
/*
 * Return the battery temperature in Celcius degrees
 * Or < 0 if something fails.
 */
int bq27421_battery_temperature(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = -1,temp = 20,val = 2930,ret = 0;

    if(!bq27421_is_accessible())
        return ((2930-CONST_NUM_2730)/CONST_NUM_10);

    temp = bq27421_get_battery_temperature();
    val = temp *CONST_NUM_10 +CONST_NUM_2730;
    dev_dbg(di->dev, "write data %d temperature\n", val);
    //bq27421_i2c_write_word_data(di, BQ27421_REG_TEMP, val);

    //msleep(1);
    data = bq27421_i2c_read_word_data(di,BQ27421_REG_TEMP);
    dev_dbg(di->dev, "read data %d temperature\n", data);
    if ((data <= 2000)||(data > 4500)){
        dev_err(di->dev, "error %d reading temperature\n", data);
        data = gauge_context.temperature;
        if((temp > -25)&&(temp < 71)){
            if(bq27421_check_if_sealed(di)){
                dev_info(di->dev, "sealed device\n");
                ret = bq27421_unseal(di);
                if(0 != ret){
                    dev_err(di->dev, "Can not unseal device\n");
                }
            }
            msleep(5);
            bq27421_write_control(di,BQ27421_SUBCMD_SOFT_RESET);
            mdelay(2);
        }
    }else{
        gauge_context.temperature = data;
    }

    data = (data-CONST_NUM_2730)/CONST_NUM_10;
    BQ27421_DBG("read temperature result = %d Celsius\n", data);
    gauge_context.rtemp = temp;
    return temp;
}

#if 0
static int bq27421_write_battery_temperature(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = -1,val = 2930,data = 0;
    static int temp = -21;

    return 0;

    if(!bq27421_is_accessible())
        return 25;

    val = gauge_context.rtemp *CONST_NUM_10 +CONST_NUM_2730;
    if ((val <= 2000)||(val > 4500)){
        val = 2930;
    }
    dev_dbg(di->dev, "write adc temp data %d  temperature\n", val);
    if((temp ! = gauge_context.rtemp)||(temp == -21)){
        bq27421_i2c_write_word_data(di, BQ27421_REG_TEMP, val);
        msleep(20);
    }
    data = bq27421_i2c_read_word_data(di,BQ27421_REG_TEMP);
    dev_dbg(di->dev, "check data %d temperature\n", data);
    if ((data <= 2000)||(data > 4500)){
        dev_err(di->dev, "check write adc temp isok =  %d \n", data);
        bq27421_i2c_write_word_data(di, BQ27421_REG_TEMP, val);
        data = gauge_context.temperature;
    }else{
        gauge_context.temperature = data;
    }
    temp = gauge_context.rtemp;
    data = (data-CONST_NUM_2730)/CONST_NUM_10;
    BQ27421_DBG("read temperature result = %d Celsius\n", data);

    return data;
}
#endif

int bq27421_chip_temperature(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = -1;

    if(!bq27421_is_accessible())
        return ((gauge_context.chiptemperature-CONST_NUM_2730)/CONST_NUM_10);

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_ITEMP);
    if ((data <= 2000)||(data > 6553)){
        dev_err(di->dev, "error %d reading chiptemperature\n", data);
        data = gauge_context.chiptemperature;
    }else{
        gauge_context.chiptemperature = data;
    }

    data = (data-CONST_NUM_2730)/CONST_NUM_10;
    BQ27421_DBG("read temperature result = %d Celsius\n", data);

    return data;
}
/*===========================================================================
  Description:    get the status of battery exist
  Return:         1->battery present; 0->battery not present.
===========================================================================*/
int is_bq27421_battery_exist(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0,adc_code = 0;
#if 1
    int i = 0, adc[2]={0};
    int sum = 0;

    for(i=0;i< 2;i++){
        adc[i] = pmic_get_adc_conversion(1);
        sum = sum+adc[i];
    }
    adc_code = sum/2;
#endif
    if(!bq27421_is_accessible())
        return gauge_context.battery_present;

	data = bq27421_i2c_read_word_data(di,BQ27421_REG_FLAGS);
    BQ27421_DBG("is_exist flags result = 0x%x \n", data);

    if (data < 0) {
        BQ27421_DBG("is_k3_bq27421_battery_exist failed!!\n");
        data = bq27421_i2c_read_word_data(di,BQ27421_REG_FLAGS);
    }

    if(data >= 0){
        if(adc_code >= 2600){
            if(gauge_context.battery_present != 0){
                dev_err(di->dev, "battery is not present = %d\n", adc_code);
            }
            gauge_context.battery_present = 0;
        }else{
            gauge_context.battery_present = !!(data & BQ27421_FLAG_BAT_DET);
        }
    } else {
        dev_err(di->dev, "error %d reading battery present\n", data);
    }

    return gauge_context.battery_present;
}

/*
 * Return the battery RemainingCapacity
 * The reture value is mAh
 * Or < 0 if something fails.
 */
int bq27421_battery_rm(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = -1;

    if(!bq27421_is_accessible())
        return gauge_context.battery_rm;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_RM);
    if(data < 0) {
        BQ27421_ERR("i2c error in reading remain capacity!");
        dev_err(di->dev, "error %d reading rm\n", data);
        data = gauge_context.battery_rm;
    } else{
        gauge_context.battery_rm = data;
    }

    BQ27421_DBG("read rm result = %d mAh\n",data);
    return gauge_context.battery_rm;
}

/*
 * Return the battery FullChargeCapacity
 * The reture value is mAh
 * Or < 0 if something fails.
 */
int bq27421_battery_fcc(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_fcc;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_FCC);
    if(data < 0) {
        BQ27421_ERR("i2c error in reading fcc!");
        dev_err(di->dev, "error %d reading fcc\n", data);
        data = gauge_context.battery_fcc;
    } else{
        gauge_context.battery_fcc = data;
    }

    BQ27421_DBG("read fcc result = %d mAh\n",data);
    return gauge_context.battery_fcc;
}

static int bq27421_battery_rmcu(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = -1;

    if(!bq27421_is_accessible())
        return gauge_context.battery_rmcu;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_RMCU);
    if(data < 0) {
        BQ27421_ERR("i2c error in reading rmcu!");
        dev_err(di->dev, "error %d reading rmcu\n", data);
        data = gauge_context.battery_rmcu;
    } else{
        gauge_context.battery_rmcu = data;
    }

    BQ27421_DBG("read rmcu result = %d mAh\n",data);
    return gauge_context.battery_rmcu;
}

static int bq27421_battery_rmcf(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = -1;

    if(!bq27421_is_accessible())
        return gauge_context.battery_rmcf;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_RMCF);
    if(data < 0) {
        BQ27421_ERR("i2c error in reading rmcf!");
        dev_err(di->dev, "error %d reading rmcf\n", data);
        data = gauge_context.battery_rmcf;
    } else{
        gauge_context.battery_rmcf = data;
    }

    BQ27421_DBG("read rmcf result = %d mAh\n",data);
    return gauge_context.battery_rmcf;
}


int bq27421_battery_fccu(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_fccu;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_FCCU);
    if(data < 0) {
        BQ27421_ERR("i2c error in reading fccu!");
        dev_err(di->dev, "error %d reading fccu\n", data);
        data = gauge_context.battery_fccu;
    } else{
        gauge_context.battery_fccu = data;
    }

    BQ27421_DBG("read fccu result = %d mAh\n",data);
    return gauge_context.battery_fccu;
}

int bq27421_battery_fccf(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_fccf;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_FCCF);
    if(data < 0) {
        BQ27421_ERR("i2c error in reading fccf!");
        dev_err(di->dev, "error %d reading fccf\n", data);
        data = gauge_context.battery_fccf;
    } else{
        gauge_context.battery_fccf = data;
    }

    BQ27421_DBG("read fccu result = %d mAh\n",data);
    return gauge_context.battery_fccf;
}

static int bq27421_battery_socu(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_socu;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_SOCU);
    if((data < 0)||(data > 100)) {
        dev_err(di->dev, "error %d reading socu\n", data);
        data = gauge_context.battery_socu;
    }else{
        gauge_context.battery_socu = data;
    }
    BQ27421_DBG("read socu result = %d Hundred Percents\n", data);

    return gauge_context.battery_socu;
}

static int bq27421_battery_nac(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_nac;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_NAC);
    if(data < 0) {
        dev_err(di->dev, "error %d reading nac\n", data);
        data = gauge_context.battery_nac;
    }else{
        gauge_context.battery_nac = data;
    }
    BQ27421_DBG("read nac result = %d mah\n", data);

    return gauge_context.battery_nac;
}

static int bq27421_battery_fac(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_fac;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_FAC);
    if(data < 0) {
        dev_err(di->dev, "error %d reading fac\n", data);
        data = gauge_context.battery_fac;
    }else{
        gauge_context.battery_fac = data;
    }
    BQ27421_DBG("read fac result = %d mah\n", data);

    return gauge_context.battery_fac;
}

static short bq27421_battery_mli(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;
    short mli = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_mli;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_MLI);
    if (data < 0) {
        dev_err(di->dev, "error %d reading max_curr\n", data);
        data = gauge_context.battery_mli;
    } else{
        gauge_context.battery_mli = (signed short)data;
    }

    mli = (signed short)data;

    BQ27421_DBG("read max_curr result = %d mA\n", mli);

    return gauge_context.battery_mli;
}

static short bq27421_battery_ap(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;
    short ap = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_ap;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_AP);
    if (data < 0) {
        dev_err(di->dev, "error %d reading ap\n", data);
        data = gauge_context.battery_ap;
    } else{
        gauge_context.battery_ap = (signed short)data;
    }

    ap = (signed short)data;

    BQ27421_DBG("read ap result = %d mA\n", ap);

    return gauge_context.battery_ap;
}

static int bq27421_battery_dod0(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 100;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_DOD0);
    if(data < 0) {
        dev_err(di->dev, "error %d reading DOD0 \n", data);
    }else{
        data = data;
    }
    BQ27421_DBG("read DOD0 result = %d mah\n", data);

    return data;
}

static int bq27421_battery_dodeoc(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 100;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_DODEOC);
    if(data < 0) {
        dev_err(di->dev, "error %d reading DODEOC \n", data);
    }else{
        data = data;
    }
    BQ27421_DBG("read DODEOC result = %d mah\n", data);

    return data;
}

static int bq27421_battery_trc(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 100;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_TRC);
    if(data < 0) {
        dev_err(di->dev, "error %d reading TrueRemCap \n", data);
    }else{
        data = data;
    }
    BQ27421_DBG("read TrueRemCap result = %d mah\n", data);

    return data;
}

static int bq27421_battery_passchg(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 100;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_PASSCHG);
    if(data < 0) {
        dev_err(di->dev, "error %d reading PassedCharge \n", data);
    }else{
        data = data;
    }
    BQ27421_DBG("read PassedCharge result = %d mah\n", data);

    return data;
}

static int bq27421_battery_qstart(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 100;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_QSTART);
    if(data < 0) {
        dev_err(di->dev, "error %d reading qstart \n", data);
    }else{
        data = data;
    }
    BQ27421_DBG("read qstart result = %d mah\n", data);

    return data;
}

/*
 * Return whether the battery charging is full
 *
 */
int is_bq27421_battery_full(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 0;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_FLAGS);
    if (data < 0) {
        BQ27421_DBG("is_bq27421_battery_full failed!!\n");
        dev_err(di->dev, "error %d reading battery full\n", data);
        return 0;
    }

    BQ27421_DBG("read flags result = 0x%x \n", data);
    return(data & BQ27421_FLAG_FC);
}

/*===========================================================================
 Description: get the health status of battery
 Input: struct bq27421_device_info *
 Output: none
 Return: 0->"Unknown", 1->"Good", 2->"Overheat", 3->"Dead", 4->"Over voltage",
 5->"Unspecified failure", 6->"Cold",
 Others: none
===========================================================================*/
int bq27421_battery_health(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;
    int status = 0;

    if(!bq27421_is_accessible())
        return gauge_context.battery_health;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_FLAGS);
    if(data < 0) {
        dev_err(di->dev, "error %d reading battery health\n", data);
        return POWER_SUPPLY_HEALTH_GOOD;
    }

    if (data & BQ27421_FLAG_OT){
        status = POWER_SUPPLY_HEALTH_OVERHEAT;
    }else if (data & BQ27421_FLAG_UT){
        status = POWER_SUPPLY_HEALTH_COLD;
    }else{
        status = POWER_SUPPLY_HEALTH_GOOD;
    }

    gauge_context.battery_health = status;
    return status;
}

/*
 * Return the battery`s status flag
 * The reture value is in hex
 * Or < 0 if something fails.
 */
int is_bq27421_battery_reach_threshold(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 0;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_FLAGS);
    if(data < 0)
        return 0;

    return data;
}

/*===========================================================================
  Function:       bq27421_battery_capacity_level
  Description:    get the capacity level of battery
  Input:          struct bq27421_device_info *
  Output:         none
  Return:         capacity percent
  Others:         none
===========================================================================*/
int bq27421_battery_capacity_level(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;
    int data_capacity = 0;
    int status = 0;

    if(!bq27421_is_accessible())
       return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;

    data = bq27421_i2c_read_word_data(di, BQ27421_REG_FLAGS);

    BQ27421_DBG("read capactiylevel result = %d \n", data);
    if(data < 0)
        return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

    if (data & BQ27421_FLAG_SOCF){
        status = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
    }else if (data & BQ27421_FLAG_SOC1){
        status = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
    }else if (data & BQ27421_FLAG_FC){
        status = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
    }else{
        data_capacity = gauge_context.capacity;//bq27421_battery_capacity();
        if (data_capacity > 95){
            status = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
        }else{
            status = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
        }
    }

    return status;
}

/*===========================================================================
  Function:       bq27421_battery_technology
  Description:    get the battery technology
  Input:          struct bq27421_device_info *
  Output:         none
  Return:         value of battery technology
  Others:         none
===========================================================================*/
int bq27421_battery_technology(void)
{
    /*Default technology is "Li-poly"*/
    int data = POWER_SUPPLY_TECHNOLOGY_LIPO;

    return data;
}

static int bq27421_battery_state_of_health(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int data = 0;

    if(!bq27421_is_accessible())
        return 0;

    data = bq27421_i2c_read_word_data(di,BQ27421_REG_SOH);
    if(data < 0) {
        dev_err(di->dev, "error %d reading state of health\n", data);
        return 0;
    }

    return data;
}

/*===========================================================================
  Function:       bq27421_get_qmax
  Description:    get qmax
  Input:          struct bq27421_device_info *
  Output:         none
  Return:         value of qmax
  Others:         none
===========================================================================*/
static int bq27421_get_qmax(struct bq27421_device_info *di)
{
    int qmax = 0,qmax1 = 0,data = 0,descap = 0;
    int error = -1,ret = 0;

    if(!bq27421_is_accessible()){
        goto err;
    }
    if(bq27421_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27421_unseal(di);
        if(0 != ret){
            dev_err(di->dev, "Can not unseal device\n");
            goto err;
        }
    }
    if(bq27421_check_if_sealed(di)){
        dev_err(di->dev, "Can not clear seal device\n");
        return gauge_context.design_capacity;
    }
    bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_STATE,0x00);
    mdelay(2);
    /*design capcity*/
    descap = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_DESIGN_CAPACITY));//0x4A
    mdelay(2);
    qmax = bq27421_i2c_read_byte_data(di,BQ27421_REG_QMAX);
    if(qmax < 0){
        goto err;
    }
    mdelay(2);
    qmax1 = bq27421_i2c_read_byte_data(di,BQ27421_REG_QMAX1);
    if(qmax1 < 0){
        goto err;
    }
    data = (qmax << 8) | qmax1;
    data = data*descap/(1 << 14);
    bq27421_seal(di);
    return data;
err:
    bq27421_seal(di);
    return (error);
}

/*===========================================================================
  Function:       bq27421_get_firmware_version
  Description:    get qmax
  Input:          struct bq27421_device_info *
  Output:         none
  Return:         value of firmware_version
  Others:         none
===========================================================================*/
static int bq27421_get_firmware_version(struct bq27421_device_info *di)
{
    unsigned int data = 0;
    int id = 0;

    if(!bq27421_is_accessible()){
        goto err;
    }
    mdelay(2);
    id = bq27421_get_control_status(di,BQ27421_SUBCMD_CHEM_ID);

    if (id < 0)
        goto err;
    data = (id);

    return data;
err:

    return (-1);
}

static int bq27421_get_device_type(struct bq27421_device_info *di)
{
    unsigned int data = 0;

    if(!bq27421_is_accessible()){
        goto err;
    }

    data = bq27421_get_control_status(di,BQ27421_SUBCMD_DEVICES_TYPE);
    if(data < 0){
        goto err;
    }
        return data;

err:

    return (-1);
}

static int bq27421_battery_dump_reg(struct bq27421_device_info *di)
{
    int volt = 0, cur = 0, capacity = 0, temp = 0,rm = 0, fcc = 0,flag = 0,control_status = 0,chiptemp =0;
    int soh = 0,rmcu = 0,rmcf = 0,fccu = 0,fccf = 0,socu = 0,nac = 0,fac = 0,ap = 0,mli = 0;
    int dod = 0, dodeoc = 0,trc = 0, passchg = 0,qstart = 0;
    temp =  bq27421_battery_temperature();
    volt = bq27421_battery_voltage();
    cur = bq27421_battery_current();
    flag = is_bq27421_battery_reach_threshold();
    rm =  bq27421_battery_rm();
    fcc = bq27421_battery_fcc();
    capacity = bq27421_battery_capacity();
    control_status = bq27421_get_control_status(di,BQ27421_SUBCMD_CONTROL_STATUS);
    chiptemp = bq27421_chip_temperature();
    mli = bq27421_battery_mli();
    soh = bq27421_battery_state_of_health();
    rmcu =  bq27421_battery_rmcu();
    rmcf =  bq27421_battery_rmcf();
    fccu = bq27421_battery_fccu();
    fccf = bq27421_battery_fccf();
    socu = bq27421_battery_socu();
    nac = bq27421_battery_nac();
    fac = bq27421_battery_fac();
    ap = bq27421_battery_ap();
    dod = bq27421_battery_dod0();
    dodeoc = bq27421_battery_dodeoc();
    trc = bq27421_battery_trc();
    passchg = bq27421_battery_passchg();
    qstart = bq27421_battery_qstart();

    dev_info(&di->client->dev, "volt =%-5d mV, cur =%-4d mA, soc =%-4d, flag = 0x%-5x, rm =%-5d mA, fcc =%-5d mA, temp =%-4d,chiptemp =%-4d, ctrl = 0x%-4x\n",
        volt,cur,capacity,flag,rm,fcc,temp,chiptemp,control_status);
    dev_info(&di->client->dev, "soh = 0x%-5.4x, rmcu =%-5d mAh, rmcf =%-5d mAh, fccu =%-5d mAh, fccf =%-5d mAh, socu =%-4d, nac =%-5d mAh, fac =%-5d mAh,mli = %-5d mA, ap = %-6d mW\n",
        soh,rmcu,rmcf,fccu,fccf,socu,nac,fac,mli,ap);
    dev_info(&di->client->dev, "dod = 0x%-5d, dodeoc =%-5d, trc =%-5d, passchg =%-5d, qstart =%-5d \n",
        dod,dodeoc,trc,passchg,qstart);
    return 0;
}


static struct file *filp;
static mm_segment_t old_filp;

static int bq27421_gauge_reg_dump(struct bq27421_device_info *di)
{
    u8 time[100]={0},buf[200];
    int ret = 0;
    struct timespec ts;
    struct rtc_time tm;
    int volt = 0, cur = 0, soc = 0, temp = 0,rm = 0, fcc = 0,flag = 0,ctrl = 0,chiptemp =0;
    int soh = 0,rmcu = 0,rmcf = 0,fccu = 0,fccf = 0,socu = 0,nac = 0,fac = 0,ap = 0,mli = 0;
    int dod = 0, dodeoc = 0,trc = 0, passchg = 0,qstart = 0;

    getnstimeofday(&ts);
    rtc_time_to_tm(ts.tv_sec, &tm);
    snprintf(time,24,"%d-%02d-%02d %02d:%02d:%02d UTC:",tm.tm_year + 1900,
        tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    if(!di->filp_open) {
        filp = filp_open("/data/local/log/gaugelog.log", O_CREAT | O_RDWR | O_APPEND, 0777);
        if (IS_ERR(filp)){
            pr_err("gaugelog: open file error!\n");
            return -1;
        }
        old_filp = get_fs();
        set_fs(KERNEL_DS);
        di->filp_open = 1;
    }

    if(di->filp_open){
        temp =  bq27421_battery_temperature();
        volt = bq27421_battery_voltage();
        cur = bq27421_battery_current();
        flag = is_bq27421_battery_reach_threshold();
        rm =  bq27421_battery_rm();
        fcc = bq27421_battery_fcc();
        soc = bq27421_i2c_read_word_data(di,BQ27421_REG_SOC);
        ctrl = bq27421_get_control_status(di,BQ27421_SUBCMD_CONTROL_STATUS);
        chiptemp = bq27421_chip_temperature();
        mli = bq27421_battery_mli();
        soh = bq27421_battery_state_of_health();
        rmcu =  bq27421_battery_rmcu();
        rmcf =  bq27421_battery_rmcf();
        fccu = bq27421_battery_fccu();
        fccf = bq27421_battery_fccf();
        socu = bq27421_battery_socu();
        nac = bq27421_battery_nac();
        fac = bq27421_battery_fac();
        ap = bq27421_battery_ap();
        dod = bq27421_battery_dod0();
        dodeoc = bq27421_battery_dodeoc();
        trc = bq27421_battery_trc();
        passchg = bq27421_battery_passchg();
        qstart = bq27421_battery_qstart();

        //ret = filp->f_op->write(filp,(char *)time, 24, &filp->f_pos);

    snprintf(buf,151,"%4d-%02d-%02d %02d:%02d:%02d  0x%04x %4d %4d %5d %3d 0x%04x %4d %4d %4d %4d %5d %6d %4d 0x%04x %4d %4d %4d %4d %3d %4d %4d %4d %5d %5d",
                    tm.tm_year + 1900,tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                    ctrl,temp,volt,cur,soc,flag,rm,fcc,nac,fac,mli,ap,chiptemp,soh,rmcu,rmcf,fccu,fccf,socu,dod,dodeoc,trc,passchg,qstart);

        ret = filp->f_op->write(filp,(char *)buf, 151, &filp->f_pos);


        ret = filp->f_op->write(filp,"\n",1, &filp->f_pos);
    }

    //set_fs(old_filp);
    //filp_close(filp,NULL);

    return 0;
}

static void bq27421_battery_gaugelog_work(struct work_struct *work)
{
    struct bq27421_device_info *di = container_of(work,
            struct bq27421_device_info, gaugelog_work.work);

    bq27421_gauge_reg_dump(di);

    schedule_delayed_work(&di->gaugelog_work, msecs_to_jiffies(10 *1000));

    return;
}
EXPORT_SYMBOL_GPL(bq27421_battery_voltage);
EXPORT_SYMBOL_GPL(bq27421_battery_current);
EXPORT_SYMBOL_GPL(bq27421_battery_capacity);
EXPORT_SYMBOL_GPL(bq27421_battery_temperature);
EXPORT_SYMBOL_GPL(is_bq27421_battery_exist);
EXPORT_SYMBOL_GPL(bq27421_battery_rm);
EXPORT_SYMBOL_GPL(bq27421_battery_fcc);
EXPORT_SYMBOL_GPL(is_bq27421_battery_full);
EXPORT_SYMBOL_GPL(is_bq27421_battery_reach_threshold);
EXPORT_SYMBOL_GPL(bq27421_battery_health);
EXPORT_SYMBOL_GPL(bq27421_battery_capacity_level);
EXPORT_SYMBOL_GPL(bq27421_battery_technology);

/*battery information interface*/

/*return: 1 = true, 0 = false*/
static int bq27421_check_qmax_property(void)
{
    int data = 0;
    int design_capacity = 0;
    int status = 0;
    struct bq27421_device_info *di = bq27421_device;

   if(!bq27421_is_accessible())
       return 0;

    data = bq27421_get_qmax(di);
    if (!bq27421_get_gasgauge_normal_capacity(&design_capacity))
        printk("design_capacity = %d,qmax = %d\n",design_capacity,data);

    if (data >= (design_capacity * 4 / 5) && data <= (design_capacity * 6 / 5 )){
         status = 1;
    }else{
         dev_info(di->dev,"qmax error,design_capacity = %d,qmax = %d\n",design_capacity,data);
         status = 0;
    }

    return status;
}
/*0 update,1 no update*/
static int is_bq27421_firmware_check(void)
{
    struct bq27421_device_info *di = bq27421_device;
    int version = 0,status = 0;

    version = bq27421_get_firmware_version(di);
    /*check firmware version is the same id*/
    if(version == gauge_context.firmware_version){
        status = bq27421_check_qmax_property();
        if(status){
            dev_info(di->dev, "no need update gas gauge firmware\n");
            return 1;
        }else{
            dev_info(di->dev, "force gas gauge firmware\n");
            return 0;
        }
    }else{
        /*firmware version is different*/
        return 0;
    }
}

/* Firmware upgrade interface*/
int bq27421_firmware_model_update(char* dev)
{
    struct bq27421_device_info *di = bq27421_device;
    int fd = 0,soc = 0;
    int ret = 0;
    mm_segment_t oldfs;
    static int timeout = 5000;

    bq27421_get_battery_id_type(di);

#if 1
    return 0; /* if need update fw,deleted it*/
#endif

    if(is_bq27421_firmware_check()){
        timeout = 0;
        return 0;
    }

    soc = bq27421_battery_capacity();
    if(soc < 20){
        dev_err(di->dev, "capacity(%d) is less than 20 and no update firmware!\n",soc);
        timeout = 0;
        return -1;
    }
    msleep(timeout);
    timeout = 0;
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    fd = sys_open(dev, O_RDWR, S_IRUSR);
    if (fd < 0)
    {
        dev_err(di->dev, "sys_open bq27421 open sysfs failed!\n");
        set_fs(oldfs);
        return fd;
    }
    ret = sys_write(fd, BSP_UPGRADE_FIRMWARE_BQFS_NAME,
                    sizeof(BSP_UPGRADE_FIRMWARE_BQFS_NAME));


    if(ret < 0){
        dev_err(di->dev, "sys_write bq27421 firmware failed!\n");
    }
    set_fs(oldfs);
    sys_close(fd);
    return ret;
}
EXPORT_SYMBOL_GPL(bq27421_firmware_model_update);

static struct comip_android_battery_info bq27421_battery_property={
    .battery_voltage = bq27421_battery_voltage,
    .battery_current = bq27421_battery_current,
    .battery_capacity = bq27421_battery_soc,
    .battery_temperature = bq27421_battery_temperature,
    .battery_exist = is_bq27421_battery_exist,
    .battery_health= bq27421_battery_health,
    .battery_rm = bq27421_battery_rm,
    .battery_fcc = bq27421_battery_fcc,
    .capacity_level = bq27421_battery_capacity_level,
    .battery_technology = bq27421_battery_technology,
    .battery_full = is_bq27421_battery_full,
    .firmware_update = bq27421_firmware_model_update,
    .battery_realsoc = bq27421_battery_realsoc,
    .chip_temp       = bq27421_chip_temperature,
    .check_init_complete = bq27421_init_complete,
};



/*
 * Use BAT_LOW not BAT_GD. When battery capacity is below SOC1,
 * BAT_LOW PIN will pull up and cause a
 * interrput, this is the interrput callback.
 */
static irqreturn_t bq27421_interrupt(int irq, void *_di)
{
    struct bq27421_device_info *di = _di;

    schedule_delayed_work(&di->notifier_work, 0);

    return IRQ_HANDLED;
}

/*===========================================================================
  Function:       interrupt_notifier_work
  Description:    send a notifier event to sysfs
  Calls:
  Called By:
  Input:          struct work_struct *
  Output:         none
  Return:         none
  Others:         none
===========================================================================*/
static void interrupt_notifier_work(struct work_struct *work)
{
    struct bq27421_device_info *di = container_of(work,
        struct bq27421_device_info, notifier_work.work);
    static int dump_flag = 0;
    int low_bat_flag = 0;

    if(!bq27421_is_accessible())
        goto exit;

    low_bat_flag = is_bq27421_battery_reach_threshold();
    if(!is_bq27421_battery_exist() || !(low_bat_flag & BQ27421_FLAG_SOC1))
        goto exit;
#if 0
    if(time_is_after_jiffies(di->timeout_jiffies)){
        di->timeout_jiffies = jiffies + msecs_to_jiffies(DISABLE_ACCESS_TIME);
        goto exit;
    }
#endif
    if(!(low_bat_flag & BQ27421_FLAG_SOCF)){
        //wake_lock(&low_battery_wakelock);
        wake_lock_timeout(&low_battery_wakelock, 1 * HZ);
        //SOC1 operation:notify upper to get the level now
        dev_info(&di->client->dev,"low battery warning event = %x\n",low_bat_flag);
    }else if(low_bat_flag & BQ27421_FLAG_SOCF){
        //wake_lock(&low_battery_wakelock);
        wake_lock_timeout(&low_battery_wakelock, 2 * HZ);
        dev_info(&di->client->dev,"low battery shutdown event = %x\n",low_bat_flag);
        if(!dump_flag){
            bq27421_battery_dump_reg(di);
            dump_flag = 1;
        }
    }else{
        goto exit;
    }

    //di->timeout_jiffies = jiffies + msecs_to_jiffies(DISABLE_ACCESS_TIME);
exit:
    if(wake_lock_active(&low_battery_wakelock)) {
        //wake_lock_timeout(&low_battery_wakelock, 1 * HZ);
        wake_unlock(&low_battery_wakelock);
    }
    return;
}

static int bq27421_init_irq(struct bq27421_device_info *di)
{
    int ret = 0;

    return 0;
    INIT_DELAYED_WORK(&di->notifier_work,interrupt_notifier_work);

    if (gpio_request(di->client->irq, "bq27421_irq") < 0){
        ret = -ENOMEM;
        goto irq_fail;
    }

    gpio_direction_input(di->client->irq);

    /* request irq interruption */
    ret = request_irq(gpio_to_irq(di->client->irq), bq27421_interrupt, 
        IRQF_TRIGGER_FALLING,"bq27421_irq",di);
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
#if 0
/*===========================================================================
  Function:       bq27421_enter_rom_mode
  Description:    change i2c addr to 0x0b
===========================================================================*/
static int bq27421_enter_rom_mode(struct bq27421_device_info *di)
{
    int data = 0;

    if(di->rom_client){
        return -EALREADY;
    }

    di->rom_client = i2c_new_dummy(di->client->adapter,BQ27421_ROM_MODE_I2C_ADDR);
    if (!di->rom_client) {
        dev_err(&di->client->dev, "Failed creating ROM i2c access\n");
        return -EIO;
    }

    data = bq27421_i2c_write_word_data(di, BSP_ENTER_ROM_MODE_CMD, BSP_ENTER_ROM_MODE_DATA);
    if(data < 0){
        dev_err(&di->client->dev, "Fail enter ROM mode. data=%d\n",data);
        i2c_unregister_device(di->rom_client);
        di->rom_client = NULL;
    }else{
        dev_info(&di->client->dev, "Enter ROM mode Begin\n");
        data = 0;
    }
    return data;
}

/*===========================================================================
  Function:       bq27421_exit_rom_mode
  Description:    change i2c addr to 0x0055 
===========================================================================*/
static int bq27421_exit_rom_mode(struct bq27421_device_info *di)
{
    int data = 0;

    if(!di->rom_client){
        return -EFAULT;
    }

    data = bq27421_i2c_write_byte_data(di->rom_client,
        BSP_LEAVE_ROM_MODE_REG1,BSP_LEAVE_ROM_MODE_DATA1);
    if(data < 0){
        dev_err(&di->client->dev, "Fail exit ROM mode. data=%d\n",data);
        goto unregister_rom;
    }
    msleep(2);
    data = bq27421_i2c_write_byte_data(di->rom_client,
        BSP_LEAVE_ROM_MODE_REG2,BSP_LEAVE_ROM_MODE_DATA2);
    if(data < 0){
        dev_err(&di->client->dev, "Send Checksum for LSB. data=%d\n",data);
        goto unregister_rom;
    }
    msleep(2);
    data = bq27421_i2c_write_byte_data(di->rom_client,
        BSP_LEAVE_ROM_MODE_REG3,BSP_LEAVE_ROM_MODE_DATA3);
    if(data < 0){
        dev_err(&di->client->dev, "Send Checksum for MSB. data=%d\n",data);
        goto unregister_rom;
    }
    msleep(2);

unregister_rom:

    i2c_unregister_device(di->rom_client);
    di->rom_client = NULL;
    msleep(250);
    dev_info(&di->client->dev, "exit ROM mode end\n");
    return data;
}

/*===========================================================================
  Function:       bq27421_rommode_read_first_two_rows
  Description:    read_first_two_rows flash data
===========================================================================*/
static int bq27421_rommode_read_first_two_rows(struct bq27421_device_info *di)
{
    u8 buf[192];
    u8 cmd[3];
    int ret = 0;
    int i = 0;

    for(i=0;i<2;i++){
        cmd[0] = 0x00;
        cmd[1] = 0x00;
        ret = bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],2);
        if(ret < 0){
            dev_err(&di->rom_client->dev, "failed write cmd = %x\n",cmd[0]);
            goto exit;
        }
        cmd[0] = 0x01;
        cmd[1] = 0x00+i;  /* 0x01 */
        cmd[2] = 0x00;    /* 0x02 */
        ret = bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        if(ret < 0){
            dev_err(&di->rom_client->dev, "failed write cmd = %x\n",cmd[0]);
            goto exit;
        }
        cmd[0] = 0x64;
        cmd[1] = 0x00+i;  /* 0x64 */
        cmd[2] = 0x00;    /* 0x65 */
        ret = bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        if(ret < 0){
            dev_err(&di->rom_client->dev, "failed write cmd = %x\n",cmd[0]);
            goto exit;
        }
        ret = bq27421_i2c_block_read(di->rom_client, 0x04,&buf[i*96],96);
        if(ret < 0){
            dev_err(&di->rom_client->dev, "failed read cmd = 0x04 \n");
            goto exit;
        }
        mdelay(20);
    }

exit:
    return ret;
}

/*===========================================================================
  Function:       bq27421_rommode_erase_first_two_rows
  Description:    erase_first_two_rows flash data
===========================================================================*/
static int bq27421_rommode_erase_first_two_rows(struct bq27421_device_info *di)
{
    u8 cmd[3];
    u8 buf;
    u8 retries = 0;
    int ret = 0;

    ret = bq27421_rommode_read_first_two_rows(di);
    if(ret < 0){
        dev_err(&di->rom_client->dev, " read first two low failed\n");
        return -1;
    }

    do {
        retries++;
        cmd[0] = 0x00;
        cmd[1] = 0x03;
        bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],2);
        cmd[0] = 0x64;
        cmd[1] = 0x03;  /* 0x64 */
        cmd[2] = 0x00;  /* 0x65 */
        bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],3);
        mdelay(20);
        bq27421_i2c_block_read(di->rom_client, 0x66,&buf,sizeof(buf));
        if (buf == 0x00)
            return 0;

        if (retries > 3)
            break;
    } while (1);

    return -1;
}


static int bq27421_rommode_mass_erase_cmd(struct bq27421_device_info *di)
{
    u8 cmd[3];
    u8 buf;
    u8 retries = 0;

    do {
        retries++;

        cmd[0] = 0x00;
        cmd[1] = 0x0C;
        bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],2);
        cmd[0] = 0x04;
        cmd[1] = 0x83;  /* 0x04 */
        cmd[2] = 0xDE;  /* 0x05 */
        bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        cmd[0] = 0x64;
        cmd[1] = 0x6D;  /* 0x64 */
        cmd[2] = 0x01;  /* 0x65 */
        bq27421_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        mdelay(200);

        bq27421_i2c_block_read(di->rom_client, 0x66,&buf,sizeof(buf));
        if (buf == 0x00)
            return 0;

        if (retries > 3)
            break;
    } while (1);

    return -1;
}
#endif


static int bq27421_atoi(const char *s)
{
    int k = 0;

    k = 0;
    while (*s != '\0' && *s >= '0' && *s <= '9') {
        k = 10 * k + (*s - '0');
        s++;
    }
    return k;
}

static unsigned long bq27421_strtoul(const char *cp, unsigned int base)
{
    unsigned long result = 0,value;

    while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
        ? toupper(*cp) : *cp)-'A'+10) < base) 
    {
        result = result*base + value;
        cp++;
    }

    return result;
}

//#define BQ27421_CHECKSUM_DEBUG

#ifdef BQ27421_CHECKSUM_DEBUG
static int bq27421_check_csum(u8 addr, u8 *pbuf, int len)
{
    struct bq27421_device_info *di = bq27421_device;
    int i = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0,old_sumdata = 0,new_sumdata = 0;
    u8 data[80] = {0};

    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    for(i =0; i< len;i++){
        data[i] = bq27421_i2c_read_byte_data(di,addr+i);
        old_sumdata = old_sumdata + data[i];
        new_sumdata = new_sumdata + (*(pbuf+i));
    }
    tmp_csum = (255 - old_csum - old_sumdata)%256;
    new_csum = 255 - (((tmp_csum + new_sumdata )) % 256);
    dev_info(di->dev," new_csum =%02x\n",new_csum);
    return new_csum;
}
#endif
static int bq27421_firmware_program(struct i2c_client *client, const unsigned char *pgm_data, unsigned int filelen)
{
    //struct bq27421_device_info *di = bq27421_device;
    unsigned int i = 0, j = 0, ulDelay = 0, ulReadNum = 0;
    unsigned int ulCounter = 0, ulLineLen = 0;
    unsigned char temp = 0; 
    unsigned char *p_cur;
    unsigned char pBuf[BSP_MAX_ASC_PER_LINE] = { 0 };
    unsigned char p_src[BSP_I2C_MAX_TRANSFER_LEN] = { 0 };
    unsigned char p_dst[BSP_I2C_MAX_TRANSFER_LEN] = { 0 };
    unsigned char ucTmpBuf[16] = { 0 };

    dev_info(&client->dev,"gasgauge firmware is downloading ...\n");
bq27421_firmware_program_begin:
    if(ulCounter > 1){
        return -1;
    }

    p_cur = (unsigned char *)pgm_data;

    while(1){
        while (*p_cur == '\r' || *p_cur == '\n'){
            p_cur++;
        }
        if((p_cur - pgm_data) >= filelen){
            dev_info(&client->dev,"Download success\n");
            break;
        }

        i = 0;
        ulLineLen = 0;

        memset(p_src, 0x00, sizeof(p_src));
        memset(p_dst, 0x00, sizeof(p_dst));
        memset(pBuf, 0x00, sizeof(pBuf));

        /*获取一行数据，去除空格*/
        while(i < BSP_MAX_ASC_PER_LINE){
            temp = *p_cur++;
            i++;
            if(('\r' == temp) || ('\n' == temp)){
                break;  
            }
            if(' ' != temp){
                pBuf[ulLineLen++] = temp;
            }
        }

        p_src[0] = pBuf[0];
        p_src[1] = pBuf[1];

        if(('W' == p_src[0]) || ('C' == p_src[0])){
            for(i=2,j=0; i<ulLineLen; i+=2,j++){
                memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
                memcpy(ucTmpBuf, pBuf+i, 2);
                p_src[2+j] = bq27421_strtoul(ucTmpBuf, 16);
            }
            temp = (ulLineLen -2)/2;
            ulLineLen = temp + 2;
        }else if('X' == p_src[0]){
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+2, ulLineLen-2);
            ulDelay = bq27421_atoi(ucTmpBuf);
        }else if('R' == p_src[0]){
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+2, 2);
            p_src[2] = bq27421_strtoul(ucTmpBuf, 16);
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+4, 2);
            p_src[3] = bq27421_strtoul(ucTmpBuf, 16);
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+6, ulLineLen-6);
            ulReadNum = bq27421_atoi(ucTmpBuf);
        }

        if(':' == p_src[1]){
            switch(p_src[0]){
                case 'W' :
#if 0
                    printk("W: ");
                    for(i=0; i<ulLineLen-4; i++){
                        printk("%x ", p_src[4+i]);
                    }
                    printk(KERN_ERR "\n");
#endif
#ifdef BQ27421_CHECKSUM_DEBUG
                    if(p_src[3] == 0x40){
                        bq27421_check_csum(p_src[3], &p_src[4], ulLineLen-4);
                    }
#endif
                    if(bq27421_i2c_block_write(client, p_src[3], &p_src[4], ulLineLen-4) < 0){
                        dev_err(&client->dev,"[%s,%d] bq27421_i2c_bytes_write failed len=%d\n",__FUNCTION__,__LINE__,ulLineLen-4);
                    }
                    if(p_src[3] == 0x3E){
                        mdelay(2);
                    }
                    break;

                case 'R' :
                    if(bq27421_i2c_block_read(client, p_src[3], p_dst, ulReadNum) < 0){
                        dev_err(&client->dev,"[%s,%d] bq27421_i2c_bytes_read failed\n",__FUNCTION__,__LINE__);
                    }
                    break;

                case 'C' :
                    if(bq27421_i2c_bytes_read_and_compare(client, p_src[3], p_dst, &p_src[4], ulLineLen-4)){
                        ulCounter++;
                        dev_err(&client->dev,"[%s,%d] bq27421_i2c_bytes_read_and_compare failed\n",__FUNCTION__,__LINE__);
                        goto bq27421_firmware_program_begin;
                    }
                    break;

                case 'X' :
                    mdelay(ulDelay);
                    break;

                default:
                    return 0;
            }
        }
    }

    return 0;
}

static int bq27421_firmware_download(struct bq27421_device_info *di, const unsigned char *pgm_data, unsigned int len)
{
    int iRet;

    gauge_context.state = BQ27421_UPDATE_FIRMWARE;

    if(bq27421_check_if_sealed(di)){
        dev_info(&di->client->dev, "Can not enter ROM mode. "
        "Must unseal device first\n");
        iRet = bq27421_unseal(di);
        if(0 != iRet){
            dev_err(&di->client->dev,
                "Can not unseal device\n");
            goto exit;
        }
    }
    if(bq27421_check_if_sealed(di)){
        dev_err(di->dev, "Can not clear seal device\n");
        iRet = -1;
        goto exit;
    }
#if 0
    /*Enter Rom Mode to change i2c addr*/
    iRet = bq27421_enter_rom_mode(di);
    if(0 != iRet){
        dev_err(di->dev, "Fail enter ROM mode\n");
        goto exit;
    }
    mdelay(2);

    iRet = bq27421_rommode_erase_first_two_rows(di);
    if(0 != iRet){
        dev_err(&di->rom_client->dev, "Erasing IF first two rows was failed\n");
        goto exit;
    }

    iRet = bq27421_rommode_mass_erase_cmd(di);
    if(0 != iRet){
        dev_err(&di->rom_client->dev, "Mass Erase procedure was failed\n");
        goto exit;
    }
#endif

    /*program bqfs*/
    iRet = bq27421_firmware_program(di->client, pgm_data, len);
    if(0 != iRet){
        printk(KERN_ERR "[%s,%d] bq27421_firmware_program failed\n",__FUNCTION__,__LINE__);
        goto exit;
    }


    gauge_context.locked_timeout_jiffies = jiffies + msecs_to_jiffies(2000);
    gauge_context.state = BQ27421_NORMAL_MODE;

    /* exit config update mode */
    //iRet = bq27421_write_control(di,BQ27421_SUBCMD_SOFT_RESET);
    if (iRet < 0){
        goto exit;
    }
    bq27421_seal(di);
    return iRet;
exit:
    gauge_context.state = BQ27421_NORMAL_MODE;
    dev_err(di->dev, "downloading fail \n");
    bq27421_seal(di);
    return iRet;
}

static int bq27421_update_firmware(struct bq27421_device_info *di, const char *pFilePath) 
{
    char *buf;
    struct file *filp;
    struct inode *inode = NULL;
    mm_segment_t oldfs;
    unsigned int length;
    int ret = 0;

    int reupdate_count = 0;
    /* open file */
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    filp = filp_open(pFilePath, O_RDONLY, S_IRUSR);
    if (IS_ERR(filp)) {
        printk(KERN_ERR "[%s,%d] filp_open failed\n",__FUNCTION__,__LINE__);
        set_fs(oldfs);
        return -1;
    }

    if (!filp->f_op) {
        printk(KERN_ERR "[%s,%d] File Operation Method Error\n",__FUNCTION__,__LINE__);
        filp_close(filp, NULL);
        set_fs(oldfs);
        return -2;
    }

    inode = filp->f_path.dentry->d_inode;
    if (!inode) {
        printk(KERN_ERR "[%s,%d] Get inode from filp failed\n",__FUNCTION__,__LINE__);
        filp_close(filp, NULL);
        set_fs(oldfs);
        return -3;
    }

    /* file's size */
    length = i_size_read(inode->i_mapping->host);
    printk("bq27421 firmware image size is %d \n",length);
    if (!( length > 0 && length < BSP_FIRMWARE_FILE_SIZE)){
        printk(KERN_ERR "[%s,%d] Get file size error\n",__FUNCTION__,__LINE__);
        filp_close(filp, NULL);
        set_fs(oldfs);
        return -4;
    }

    /* allocation buff size */
    buf = vmalloc(length+(length%2));       /* buf size if even */
    if (!buf) {
        printk(KERN_ERR "[%s,%d] Alloctation memory failed\n",__FUNCTION__,__LINE__);
        filp_close(filp, NULL);
        set_fs(oldfs);
        return -5;
    }

    /* read data */
    if (filp->f_op->read(filp, buf, length, &filp->f_pos) != length) {
        printk(KERN_ERR "[%s,%d] File read error\n",__FUNCTION__,__LINE__);
        filp_close(filp, NULL);
        filp_close(filp, NULL);
        set_fs(oldfs);
        vfree(buf);
        return -6;
    }

    do {
        if(0==(ret = bq27421_firmware_download(di, (const char*)buf, length)))
            break;
        reupdate_count++;
        printk(KERN_ERR "reupdate gas gauge firmware,count: %d\n",reupdate_count);
    } while(reupdate_count != 2);

    filp_close(filp, NULL);
    set_fs(oldfs);
    vfree(buf);

    return ret;
}

/* Firmware upgrade sysfs show interface*/
static ssize_t bq27421_attr_store(struct device_driver *driver, 
                            const char *buf, size_t count)
{
    struct bq27421_device_info *di = bq27421_device;
    int iRet = 0;
    unsigned char path_image[255];

    //bq27421_get_battery_id_type(di);
    if(NULL == buf || count >255 || count == 0 || strnchr(buf, count, 0x20))
        return -1;

    wake_lock(&bqfs_wakelock);

    memcpy (path_image, buf, count);
    /* replace '\n' with  '\0'  */ 
    if((path_image[count-1]) == '\n'){
        path_image[count-1] = '\0'; 
    }else{
        path_image[count] = '\0';
    }

    /*enter firmware bqfs download*/
    iRet = bq27421_update_firmware(di, path_image);
    if(iRet < 0){
        wake_unlock(&bqfs_wakelock);
        return -1;
    }
    wake_unlock(&bqfs_wakelock);

    return count;
}

static ssize_t bq27421_attr_show(struct device_driver *driver, char *buf)
{
    struct bq27421_device_info *di = bq27421_device;
    int iRet = 0;

    if(!bq27421_is_accessible()) {
        return sprintf(buf,"bq27421 is busy because of updating(%d)\n",gauge_context.state);
    }

    if(NULL == buf){
        return -1;
    }

    iRet = bq27421_get_firmware_version(di);
    if(iRet < 0){
        return sprintf(buf, "%s", "Coulometer Damaged or Firmware Error");
    }else{
        return sprintf(buf, "%x\n", iRet);
    }
}

static DRIVER_ATTR(state, 0664, bq27421_attr_show, bq27421_attr_store);

/*define a sysfs interface for firmware upgrade*/
static ssize_t bq27421_fg_store(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    struct bq27421_device_info *di = dev_get_drvdata(dev);
    int iRet = 0;
    unsigned char path_image[255];

    //bq27421_get_battery_id_type(di);
    if(NULL == buf || count >255 || count == 0 || strnchr(buf, count, 0x20))
        return -1;

    wake_lock(&bqfs_wakelock);

    memcpy (path_image, buf, count);
    /* replace '\n' with  '\0'  */ 
    if((path_image[count-1]) == '\n'){
        path_image[count-1] = '\0'; 
    }else{
        path_image[count] = '\0';
    }

    /*enter firmware bqfs download*/
    iRet = bq27421_update_firmware(di, path_image);
    if(iRet < 0){
        wake_unlock(&bqfs_wakelock);
        return -1;
    }

    wake_unlock(&bqfs_wakelock);

    return count;
}

static ssize_t bq27421_fg_show(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27421_device_info *di = dev_get_drvdata(dev);
    int iRet = 0;

    if(!bq27421_is_accessible()) {
        return sprintf(buf,"bq27421 is busy because of updating(%d)\n",gauge_context.state);
    }

    if(NULL == buf){
        return -1;
    }

    iRet = bq27421_get_firmware_version(di);
    if(iRet < 0){
        return sprintf(buf, "%s", "Coulometer Damaged or Firmware Error");
    }else{
        return sprintf(buf, "%x\n", iRet);
    }
}

static ssize_t bq27421_show_gaugelog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27421_device_info *di = dev_get_drvdata(dev);
    int volt, cur, capacity, temp, rm, fcc, control_status;
    u16 flag = 0;
    int qmax = 0,soh = 0,chiptemp = 0;
    int rmcu = 0,rmcf = 0,fccu = 0,fccf = 0,socu = 0,nac = 0,fac = 0,ap = 0;
    if(!bq27421_is_accessible())
        return sprintf(buf,"bq27421 is busy because of updating(%d)\n",gauge_context.state);

    temp =  bq27421_battery_temperature();
    mdelay(2);
    volt = bq27421_battery_voltage();
    mdelay(2);
    cur = bq27421_battery_current();
    mdelay(2);
    capacity = bq27421_battery_capacity();
    mdelay(2);
    flag = is_bq27421_battery_reach_threshold();
    mdelay(2);
    rm =  bq27421_battery_rm();
    mdelay(2);
    fcc = bq27421_battery_fcc();
    mdelay(2);
    control_status = bq27421_get_control_status(di,BQ27421_SUBCMD_CONTROL_STATUS);
    mdelay(2);
    soh = bq27421_battery_state_of_health();
    mdelay(2);
    chiptemp = bq27421_chip_temperature();
    mdelay(2);
    rmcu =  bq27421_battery_rmcu();
    mdelay(2);
    rmcf =  bq27421_battery_rmcf();
    mdelay(2);
    fccu = bq27421_battery_fccu();
    mdelay(2);
    fccf = bq27421_battery_fccf();
    mdelay(2);
    socu = bq27421_battery_socu();
    mdelay(2);
    nac = bq27421_battery_nac();
    mdelay(2);
    fac = bq27421_battery_fac();
    mdelay(2);
    ap = bq27421_battery_ap();
    mdelay(2);
    qmax = bq27421_get_qmax(di);
    if(qmax < 0){
        return sprintf(buf, "%s", "Coulometer Damaged or Firmware Error \n");
    }else{
        sprintf(buf, "volt =%-6d curr =%-6d soc =%-4d flag =0x%-5.4x rm =%-5d fcc=%-5d temp =%-3d chiptemp =%-3d ctrl =0x%-5.4x soh =0x%-5.4x qmax =%-4d rmcu =%-4d, rmcf =%-4d, fccu =%-4d, fccf =%-4d, socu =%-4d, nac =%-4d, fac =%-4d, ap =%-6d\n",
                    volt, cur, capacity, flag, rm, fcc, temp, chiptemp, control_status, soh, qmax,rmcu,rmcf,fccu,fccf,socu,nac,fac,ap);
        return strlen(buf);
    }
}

static ssize_t bq27421_show_firmware_version(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    int val;
    struct bq27421_device_info *di = dev_get_drvdata(dev);
    int error = 0;

    if(!bq27421_is_accessible()) {
        return sprintf(buf,"bq27421 is busy because of updating(%d)\n",gauge_context.state);
    }

    val = bq27421_get_firmware_version(di);
    if(val < 0){
        error = -1;
        goto exit;
    }
exit:
    if(error){
        return sprintf(buf,"Fail to read firmware version\n");
    }else {
        return sprintf(buf,"0x%x\n",val);
    }
}

static ssize_t bq27421_show_voltage(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    int val = 0;
    //struct bq27421_device_info *di = dev_get_drvdata(dev);

    if (NULL == buf) {
        BQ27421_ERR("bq27421_show_voltage Error\n");
        return -1;
    }
    val = bq27421_battery_voltage();

    val *=1000;

    return sprintf(buf, "%d\n", val);
}

static ssize_t bq27421_show_data_flash(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27421_device_info *di = dev_get_drvdata(dev);
    int taper_v = 0,chgvolt = 0, iterm = 0,descap = 0,soc1 = 0, socf = 0,de = 0,poweroff_v = 0;
    int error = 0,ret = 0,opconfig = 0,soc1c = 0, socfc = 0;
    u8 data_flash[32] = {0};

    if(!bq27421_is_accessible()) {
        return sprintf(buf,"bq27421 is busy because of updating(%d)\n",gauge_context.state);
    }
    if(bq27421_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27421_unseal(di);
        if(0 != ret){
            dev_err(di->dev, "Can not unseal device\n");
            error = -1;
            goto exit;
        }
    }
    if(bq27421_check_if_sealed(di)){
        dev_err(di->dev, "Can not clear seal device\n");
        error = -1;
        goto exit;
    }
    /*chg volt*/
     ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_STATE,33,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    chgvolt = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);

    /*chg term current*/
    ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_STATE,27,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    iterm = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);

    /*design capcity*/
    ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_STATE,10,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    descap = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);
    /*design capcity*/
    ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_STATE,12,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    de = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);
    /*dischg soc1 and socf*/
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_DISCHG,0x00);/*0x00 represent 0-31 byte*/
    //ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_DISCHG,0,data_flash,4,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    mdelay(2);
    soc1 = bq27421_i2c_read_byte_data(di,0x40);
    soc1c = bq27421_i2c_read_byte_data(di,0x41);
    socf = bq27421_i2c_read_byte_data(di,0x42);
     socfc = bq27421_i2c_read_byte_data(di,0x43);
    mdelay(2);

    ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_STATE,29,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    taper_v = (data_flash[0]<< 8)|data_flash[1];
    /*poweroff volt*/
    ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_STATE,16,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    poweroff_v = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_REG,0x00);
    //ret = bq27421_access_data_flash(di,BQ27421_SUBCLASS_ID_REG,0,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    mdelay(2);
    opconfig = swab16(bq27421_i2c_read_word_data(di,0x40));
exit:
    bq27421_seal(di);
    if(error){
        return sprintf(buf,"Fail to read\n");
    }else {
        sprintf(buf,"chg_v = %3d mV, iterm = %3d mA, descap = %3d mAh,de = %3d mwh,"
                    "soc1 = %2d,soc1c = %2d, socf = %2d,socfc = %2d, pwroff_v = %d, rechg_v = %3d mV,"
                    "opconfig = 0x%x\n",chgvolt,iterm,descap,de,soc1,soc1c,socf,socfc,poweroff_v,taper_v,opconfig);
        return strlen(buf);
    }
}

static ssize_t bq27421_show_fh(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27421_device_info *di = dev_get_drvdata(dev);
    int error = 0,ret = 0,hiber_v = 0;
    int fh0 = 0,fh1 = 0,fh2 = 0,fh3=0;
    if(!bq27421_is_accessible()) {
        return sprintf(buf,"bq27421 is busy because of updating(%d)\n",gauge_context.state);
    }
    if(bq27421_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27421_unseal(di);
        if(0 != ret){
            dev_err(di->dev, "Can not unseal device\n");
            error = -1;
            goto exit;
        }
    }
    if(bq27421_check_if_sealed(di)){
        dev_err(di->dev, "Can not clear seal device\n");
        error = -1;
        goto exit;
    }
    bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_POWER,0x00);/*0x00 rw 0-31 byte*/
    mdelay(2);
    hiber_v = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_HIBERNATE_V));
    mdelay(2);
    bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x00);/*0x00 rw 0-31 byte*/
    mdelay(2);
    fh0 = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_FH0));
    fh1 = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_FH1));
    fh2 = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_FH2));
    mdelay(2);
    bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x01);/* rw 32-40 byte*/
    mdelay(2);
    fh3 = bq27421_i2c_read_byte_data(di,BQ27421_REG_FH3);

exit:
    bq27421_seal(di);
    if(error){
        return sprintf(buf,"Fail to read\n");
    }else {
        sprintf(buf,"hiber_v = %4d,fh0->fh3 = %2d, %2d, %2d, %2d\n",hiber_v,fh0,fh1,fh2,fh3);
        return strlen(buf);
    }
}

static ssize_t bq27421_show_ratable(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27421_device_info *di = dev_get_drvdata(dev);
    int error = 0,ret = 0, i = 0;
    u8 RaTable[40] = {0};
    char buf_temp[40] = {0};

    if(!bq27421_is_accessible()) {
        return sprintf(buf,"bq27421 is busy because of updating(%d)\n",gauge_context.state);
    }

    if(bq27421_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27421_unseal(di);
        if(0 != ret){
            dev_err(di->dev, "Can not unseal device\n");
            error = -1;
            goto exit;
        }
    }

    if(bq27421_check_if_sealed(di)){
        dev_err(di->dev, "Can not clear seal device\n");
        error = -1;
        goto exit;
    }

    bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_RARAM,0x00);/*0x00 rw 0-31 byte*/
    msleep(2);
    for(i = 0;i < 32;i++){
        RaTable[i] = bq27421_i2c_read_byte_data(di,0x40+i);
        sprintf(buf_temp,"0x%-4.2x",RaTable[i]);
        strcat(buf,buf_temp);
    }
    strcat(buf,"\n");
exit:
    bq27421_seal(di);
    if(error){
        return sprintf(buf,"Fail to read\n");
    }else {

        return strlen(buf);
    }
}

static ssize_t bq27421_gaugefile_store(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct bq27421_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;

    if(val){
        schedule_delayed_work(&di->gaugelog_work, msecs_to_jiffies(10));
    }else{
        cancel_delayed_work_sync(&di->gaugelog_work);
    }

    return count;
}

static ssize_t bq27421_reset_store(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val = 0;
    int ret = 0;
    struct bq27421_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 1))
        return -EINVAL;



    if(!bq27421_is_accessible()) {
        return -1;
    }

    if(bq27421_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27421_unseal(di);
        if(0 != ret){
            dev_err(di->dev, "Can not unseal device\n");
            return -1;
        }
    }
    if(val){
        bq27421_write_control(di,BQ27421_SUBCMD_SOFT_RESET);
    }
    bq27421_seal(di);

    return count;
}
static DEVICE_ATTR(reset, S_IWUSR | S_IRUGO, NULL, bq27421_reset_store);
static DEVICE_ATTR(gaugefile, S_IWUSR | S_IRUGO, NULL, bq27421_gaugefile_store);
static DEVICE_ATTR(fh, S_IWUSR | S_IRUGO, bq27421_show_fh, NULL);
static DEVICE_ATTR(ratable,   S_IWUSR | S_IRUGO, bq27421_show_ratable, NULL);
static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, bq27421_fg_show, bq27421_fg_store);
static DEVICE_ATTR(gaugelog, S_IWUSR | S_IRUGO, bq27421_show_gaugelog, NULL);
static DEVICE_ATTR(firmware_version, S_IWUSR | S_IRUGO, bq27421_show_firmware_version, NULL);
static DEVICE_ATTR(voltage, S_IWUSR | S_IRUGO, bq27421_show_voltage, NULL);
static DEVICE_ATTR(data_flash, S_IWUSR | S_IRUGO, bq27421_show_data_flash, NULL);

static struct attribute *bq27421_attributes[] = {
    &dev_attr_state.attr,
    &dev_attr_gaugelog.attr,
    &dev_attr_firmware_version.attr,
    &dev_attr_voltage.attr,
    &dev_attr_data_flash.attr,
    &dev_attr_fh.attr,
    &dev_attr_ratable.attr,
    &dev_attr_gaugefile.attr,
    &dev_attr_reset.attr,
    NULL,
};
static const struct attribute_group bq27421_attr_group = {
    .attrs = bq27421_attributes,
};

static int bq27421_hibernate_setting(struct bq27421_device_info *di)
{
    int ret = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0;
    int old_hibernate_v = 0,old_fh_0 = 0,old_fh_1 = 0,old_fh_2 = 0,old_fh_3 = 0;
    u8 old_hibernate_v_lsb = 0,old_hibernate_v_msb = 0;
    u8 old_fh_0_lsb = 0,old_fh_0_msb = 0;
    u8 old_fh_1_lsb = 0,old_fh_1_msb = 0;
    u8 old_fh_2_lsb = 0,old_fh_2_msb = 0;

    /* setup fuel gauge state hibernate_v data block block for ram access */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_POWER,0x00);/*9 represent rw 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /* hibernatev addr= 0x49*/
    old_hibernate_v = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_HIBERNATE_V));
    old_hibernate_v_lsb = old_hibernate_v & 0xFF;
    old_hibernate_v_msb = (old_hibernate_v & 0xFF00) >> 8;
    if (gauge_context.hibernate_v == old_hibernate_v) {
        return 0;
    }
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_HIBERNATE_V,swab16(gauge_context.hibernate_v));
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (255 - old_csum - old_hibernate_v_lsb - old_hibernate_v_msb) % 256;

    new_csum = 255 - ((tmp_csum
                + (gauge_context.hibernate_v & 0xFF)
                + ((gauge_context.hibernate_v & 0xFF00) >> 8)
                ) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(5);

    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_POWER,0x00);/*9 represent rw 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60

    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (hibernatev)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (hibernatev)written old:%d, new:%d\n", new_csum, old_csum);
    }
    msleep(2);

    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x00);/*11 represent rw 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /*fh_0 addr= 0x4B*/
    old_fh_0 = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_FH0));
    old_fh_0_lsb = old_fh_0 & 0xFF;
    old_fh_0_msb = (old_fh_0 & 0xFF00) >> 8;
    /*fh_1 addr= 0x58*/
    old_fh_1 = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_FH1));
    old_fh_1_lsb = old_fh_1 & 0xFF;
    old_fh_1_msb = (old_fh_1 & 0xFF00) >> 8;
    /*fh_2 addr= 0x5A*/
    old_fh_2 = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_FH2));
    old_fh_2_lsb = old_fh_2 & 0xFF;
    old_fh_2_msb = (old_fh_2 & 0xFF00) >> 8;

    /* update new fh setting to fuel gauge */
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_FH0,swab16(gauge_context.fh_0));
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_FH1,swab16(gauge_context.fh_1));
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_FH2,swab16(gauge_context.fh_2));
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (tmp_csum - old_fh_0_lsb -old_fh_0_msb
                        - old_fh_1_lsb -old_fh_1_msb
                        - old_fh_2_lsb -old_fh_2_msb
                        )%256;

    new_csum = 255 - (((tmp_csum
                + ( gauge_context.fh_0 & 0xFF)
                + ((gauge_context.fh_0 & 0xFF00) >> 8)
                + ( gauge_context.fh_1 & 0xFF)
                + ((gauge_context.fh_1 & 0xFF00) >> 8)
                + ( gauge_context.fh_2 & 0xFF)
                + ((gauge_context.fh_2 & 0xFF00) >> 8)
                )) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(5);
    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x00);/*11 represent rw 0-31 byte*/
    if (ret < 0){
        goto exit;
    }

    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (fh0->2)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (fh0->2)written old:%d, new:%d\n", new_csum, old_csum);
    }
    msleep(2);

    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x01);/*33 represent rw 32-63 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /*fh_3 addr= 0x41*/
    old_fh_3 = bq27421_i2c_read_byte_data(di,BQ27421_REG_FH3);
    old_fh_3 = old_fh_3 & 0xFF;

    /* update new fh setting to fuel gauge */
    ret = bq27421_i2c_write_byte_data(di->client,BQ27421_REG_FH3,gauge_context.fh_3);
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (255 - old_csum - old_fh_3)%256;

    new_csum = 255 - (((tmp_csum + gauge_context.fh_3 )) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(5);
    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x01);/*33 represent rw 32-63 byte*/
    if (ret < 0){
        goto exit;
    }

    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (fh3)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (fh3)written old:%d, new:%d\n", new_csum, old_csum);
    }

    return 0;
exit:
    return ret;
}

static int bq27421_set_over_temp(struct bq27421_device_info *di)
{
    int ret = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0;
    u8 old_overtemp_lsb = 0,old_overtemp_msb = 0;
    int old_overtemp = 0;

    /* setup fuel gauge state over temp data block block for ram access */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_SAFETY,0x00);/*0x00 represent 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /* overtemp = 0x40*/
    old_overtemp = swab16(bq27421_i2c_read_word_data(di,0x40));
    old_overtemp_lsb = old_overtemp & 0xFF;
    old_overtemp_msb = (old_overtemp & 0xFF00) >> 8;
    if (gauge_context.over_temp == old_overtemp) {
        return 0;
    }
    ret = bq27421_i2c_write_word_data(di,0x40,swab16(gauge_context.over_temp));/*62*10*/
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (255 - old_csum - old_overtemp_lsb - old_overtemp_msb) % 256;

    new_csum = 255 - ((tmp_csum
                + (gauge_context.over_temp & 0xFF)
                + ((gauge_context.over_temp & 0xFF00) >> 8)
                ) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(2);

    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_SAFETY,0x00);/*0x00 represent 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60

    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (over temp)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (over temp)written old:%d, new:%d\n", new_csum, old_csum);
    }

    return 0;

exit:
    return ret;
}

static int bq27421_set_lowbattery_threshold(struct bq27421_device_info *di)
{
    int ret = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0;
    int old_soc1 =0,old_socf = 0,old_soc1c =0,old_socfc = 0;

    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_DISCHG,0x00);/*0x00 represent 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /*soc1 addr= 0x40*/
    old_soc1 = bq27421_i2c_read_byte_data(di,0x40);
    old_soc1 = old_soc1 & 0xFF;
    old_soc1c = bq27421_i2c_read_byte_data(di,0x41);
    old_soc1c = old_soc1c & 0xFF;
    /*socf addr= 0x42*/
    old_socf = bq27421_i2c_read_byte_data(di,0x42);
    old_socf = old_socf & 0xFF;
    old_socfc = bq27421_i2c_read_byte_data(di,0x43);
    old_socfc = old_socfc & 0xFF;
    /* update new fh setting to fuel gauge */
    ret = bq27421_i2c_write_byte_data(di->client,0x40,gauge_context.soc1);
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_byte_data(di->client,0x41,gauge_context.soc1c);
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_byte_data(di->client,0x42,gauge_context.socf);
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_byte_data(di->client,0x43,gauge_context.socfc);
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (255 - old_csum - old_soc1 - old_socf - old_soc1c - old_socfc)%256;

    new_csum = 255 - (((tmp_csum + gauge_context.soc1 + gauge_context.socf
                    + gauge_context.soc1c + gauge_context.socfc )) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_DISCHG,0x00);/*0x00 represent 0-31 byte*/
    if (ret < 0){
        goto exit;
    }

    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (soc1-socf)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (soc1-socf)written old:%d, new:%d\n", new_csum, old_csum);
    }

    return 0;

exit:
    return ret;
}

static int bq27421_set_termV_valid_t(struct bq27421_device_info *di)
{
    int ret = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0;
    int old_termV_valid_t = 0;

    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x02);/* rw 78 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /*Term valid T addr= 0x4E*/
    old_termV_valid_t = bq27421_i2c_read_byte_data(di,0x4E);
    old_termV_valid_t = old_termV_valid_t & 0xFF;
    if (gauge_context.termV_valid_t == old_termV_valid_t) {
        return 0;
    }
    /* update new fh setting to fuel gauge */
    ret = bq27421_i2c_write_byte_data(di->client,0x4E,gauge_context.termV_valid_t);
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (255 - old_csum - old_termV_valid_t)%256;

    new_csum = 255 - (((tmp_csum + gauge_context.termV_valid_t )) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_IT_CFG,0x02);/*78 represent rw 64-95 byte*/
    if (ret < 0){
        goto exit;
    }

    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (termV_valid_t)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (termV_valid_t)written old:%d, new:%d\n", new_csum, old_csum);
    }

    return 0;

exit:
    return ret;
}

static int bq27421_set_opconfig(struct bq27421_device_info *di)
{
    int ret = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0;
    u8 old_opconfig_lsb = 0,old_opconfig_msb = 0;
    int old_opconfig = 0;

    /* setup fuel gauge state opconfig data block block for ram access */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_REG,0x00);/*0x00 represent 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /* opconfig*/
    old_opconfig = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_FLASH));
    old_opconfig_lsb = old_opconfig & 0xFF;
    old_opconfig_msb = (old_opconfig & 0xFF00) >> 8;

    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_FLASH,swab16(gauge_context.opconfig));/*0x25FC select BAT_LOW intr*/
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (255 - old_csum - old_opconfig_lsb - old_opconfig_msb) % 256;

    new_csum = 255 - ((tmp_csum
                + (gauge_context.opconfig & 0xFF)
                + ((gauge_context.opconfig & 0xFF00) >> 8)
                ) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(2);

    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_REG,0x00);/*0x00 represent 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60

    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (opconfig)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (opconfig)written old:%d, new:%d\n", new_csum, old_csum);
    }
    return 0;

exit:
    return ret;
}

static int bq27421_set_v_chg_term(struct bq27421_device_info *di)
{
    int ret = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0;
    u8 old_v_chg_term_lsb = 0,old_v_chg_term_msb = 0;
    int old_v_chg_term = 0,old_slpcurr = 0;

/* setup fuel gauge state data (charge volt) block for ram access */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_STATE,0x01);/*33 represent 32-63 byte*/
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    old_slpcurr = bq27421_i2c_read_byte_data(di,0x40);
    old_v_chg_term = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_VCHG_TERM));
    old_v_chg_term_lsb = old_v_chg_term & 0xFF;
    old_v_chg_term_msb = (old_v_chg_term & 0xFF00) >> 8;

    if ((gauge_context.v_chg_term == old_v_chg_term)&&((gauge_context.slpcurrent == old_slpcurr))) {
        return 0;
    }

    ret = bq27421_i2c_write_byte_data(di->client,0x40,gauge_context.slpcurrent);
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_VCHG_TERM,swab16(gauge_context.v_chg_term));
    if (ret < 0){
        goto exit;
    }

    tmp_csum = (255 - old_csum - old_v_chg_term_lsb - old_v_chg_term_msb-old_slpcurr) % 256;
    new_csum = 255 - ((tmp_csum
                + (gauge_context.slpcurrent &0xFF)
                + (gauge_context.v_chg_term & 0xFF)
                + ((gauge_context.v_chg_term & 0xFF00) >> 8)
                ) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_STATE,0x01);/*33 represent 32-63 byte*/
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60

    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (chg_v)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (chg_v)written old:%d, new:%d\n", new_csum, old_csum);
    }
    return 0;

exit:
    return ret;
}


#if 0
static int bq27421_set_flashdata(struct bq27421_device_info *di,u8 subclass,u8 block_num, u8 *pbuf, int len)
{
    int ret = 0,i = 0;
    int old_csum = 0, new_csum = 0,tmp_csum = 0,old_sumdata = 0,new_sumdata = 0;
    u8 data[80] = {0};

    ret = bq27421_set_access_dataclass(di,subclass,block_num);
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    for(i =0; i< len;i++){
        data[i] = bq27421_i2c_read_byte_data(di,0x40+i);
        old_sumdata = old_sumdata + data[i];
        new_sumdata = new_sumdata + (*(pbuf+i));
    }
        /* update new fuel gauge data*/
    ret = bq27421_i2c_block_write(di->client, 0x40, pbuf,len);
    //for(i =0; i< len;i++){
        //ret = bq27421_i2c_write_byte_data(di->client,0x40+i,*(pbuf+i));
    //}
    if (ret < 0){
        goto exit;
    }
    tmp_csum = (255 - old_csum - old_sumdata)%256;

    new_csum = 255 - (((tmp_csum + new_sumdata )) % 256);
    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    ret = bq27421_set_access_dataclass(di,subclass,block_num);
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (subclass = 0x%02x)write failed old:%02x, new:%02x\n", subclass,new_csum, old_csum);
    }else{
        dev_info(&di->client->dev,
        "checksum (subclass = 0x%02x)written old:%02x, new:%02x\n",subclass, new_csum, old_csum);
    }
    return 0;

exit:
    return ret;
}
#endif


static int bq27421_download_firmware_version(struct bq27421_device_info *di)
{
    int ret = 0;
    int filelen = 0;

    //return -1;

    gauge_context.state = BQ27421_UPDATE_FIRMWARE;

    switch(battery_id_status){
    case BAT_ID_1:
    case BAT_ID_2:
    case BAT_ID_3:
    case BAT_ID_4:
    case BAT_ID_5:
    case BAT_ID_7:
        filelen = (unsigned char *)&bq27421_xwd_2200_fw_end - (unsigned char *)&bq27421_xwd_2200_fw;
        ret = -1;
        goto exit;
        break;
    case BAT_ID_8:
        filelen = (unsigned char *)&bq27421_gy_2200_fw_end - (unsigned char *)&bq27421_gy_2200_fw;
        ret = bq27421_firmware_program(di->client, (const char *)&bq27421_gy_2200_fw, filelen);
        break;
    case BAT_ID_9:
        filelen = (unsigned char *)&bq27421_ds_2200_fw_end - (unsigned char *)&bq27421_ds_2200_fw;
        ret = bq27421_firmware_program(di->client, (const char *)&bq27421_ds_2200_fw, filelen);
        break;
    case BAT_ID_10:
        filelen = (unsigned char *)&bq27421_xwd_2200_fw_end - (unsigned char *)&bq27421_xwd_2200_fw;
        bq27421_firmware_program(di->client, (const char *)&bq27421_xwd_2200_fw, filelen);
        break;
    case BAT_ID_NONE:
    default:
        filelen = (unsigned char *)&bq27421_xwd_2200_fw_end - (unsigned char *)&bq27421_xwd_2200_fw;
        ret = bq27421_firmware_program(di->client, (const char *)&bq27421_xwd_2200_fw, filelen);
        break;
    }

    gauge_context.state = BQ27421_NORMAL_MODE;

    dev_info(di->dev,"bq27421 firmware image size is = %d\n",filelen);
exit:
    return ret;
}
static int bq27421_init_gaguge_cfgparam(struct bq27421_device_info *di)
{
    int ret = 0;
    unsigned long timeout = jiffies + 2*HZ;
    int old_csum = 0, new_csum = 0,tmp_csum = 0;
    int old_des_cap = 0;
    int old_des_energy = 0;
    int old_tv = 0;
    int old_tr = 0;
    int old_taper_volt = 0;
    u8 old_dc_lsb = 0,old_dc_msb = 0;
    u8 old_de_lsb = 0,old_de_msb = 0;
    u8 old_tv_lsb = 0,old_tv_msb = 0;
    u8 old_tr_lsb = 0,old_tr_msb = 0;
    u8 old_taper_volt_lsb = 0,old_taper_volt_msb = 0;
    int flags_lsb = 0,fcc = 0,rm = 0,volt = 0,curr = 0;

    bq27421_get_battery_id_type(di);
    flags_lsb = bq27421_i2c_read_byte_data(di,BQ27421_REG_FLAGS);
    fcc = bq27421_battery_fcc();
    rm = bq27421_battery_rm();
    volt = bq27421_battery_voltage();
    curr = bq27421_battery_current();
    /*if the values match with the required ones or if POR bit is not set
    seal the fuel gauge and return
    */
    if (!(flags_lsb & BQ27421_FLAG_ITPOR)) {
        if(((volt >= 3700)&&(rm < 10)&&(curr <= 200))||(fcc < 500)){
            dev_info(di->dev, "soft_reset fg(flag =0x%2x,fcc =%d,rm =%d)\n",flags_lsb,fcc,rm);
            if(bq27421_check_if_sealed(di)){
                dev_info(di->dev, "sealed device\n");
                ret = bq27421_unseal(di);
                if(0 != ret){
                    dev_err(di->dev, "Can not unseal device\n");
                }
            }
            msleep(5);
            bq27421_write_control(di,BQ27421_SUBCMD_SOFT_RESET);
            msleep(2000);
            goto normal;
        }else{
            dev_info(di->dev, "FG is already programmed (flag = 0x%2x,fcc = %d)\n",flags_lsb,fcc);
            goto normal;
        }

    }

    if(bq27421_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27421_unseal(di);
        if(0 != ret){
            dev_err(di->dev, "Can not unseal device\n");
            goto exit;
        }
    }

    ret = bq27421_write_control(di,BQ27421_SUBCMD_SET_CFGUPDATE);
    if (ret < 0){
        goto exit;
    }

    while (!(bq27421_i2c_read_byte_data(di, BQ27421_REG_FLAGS) & BQ27421_FLAG_CFGUPMODE)) {
        if (time_after(jiffies, timeout)) {
            dev_warn(&di->client->dev,"timeout waiting for cfg update\n");
            ret =  -ETIMEDOUT;
            goto exit;
        }
        msleep(2);
    }

    ret = bq27421_download_firmware_version(di);
    if (ret == 0){
        if((battery_id_status == 0)||(battery_id_status > 7)){
            goto soft_reset;
        }
    }

    /*download fail or use 2000mah battery must init params again*/
    /* place the fuel gauge into config update */
    dev_info(di->dev, "Bq27421 update params\n");
    /* setup fuel gauge state data block block for ram access */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_STATE,0x00);/*0x00 represent rw 0-31 byte*/
    if (ret < 0){
        goto exit;
    }
    /* read check sum */
    mdelay(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    tmp_csum = 255 - old_csum;
    /*design capacity*/
    old_des_cap = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_DESIGN_CAPACITY));
    old_dc_lsb = old_des_cap & 0xFF;
    old_dc_msb = (old_des_cap & 0xFF00) >> 8;
    /*design energy*/
    old_des_energy = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_DESIGN_ENERGE));
    old_de_lsb = old_des_energy & 0xFF;
    old_de_msb = (old_des_energy & 0xFF00) >> 8;
    /*poweroff volt*/
    old_tv = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_TERMINATE_VOLT));
    old_tv_lsb = old_tv & 0xFF;
    old_tv_msb = (old_tv & 0xFF00) >> 8;
    /*terminate current*/
    old_tr = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_TAPER_RATE));
    old_tr_lsb = old_tr & 0xFF;
    old_tr_msb = (old_tr & 0xFF00) >> 8;
    /*recharge volt*/
    old_taper_volt = swab16(bq27421_i2c_read_word_data(di,BQ27421_REG_TAPER_VOLT));
    old_taper_volt_lsb = old_taper_volt & 0xFF;
    old_taper_volt_msb = (old_taper_volt & 0xFF00) >> 8;


    /* update new config to fuel gauge */
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_DESIGN_CAPACITY,swab16(gauge_context.design_capacity));
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_DESIGN_ENERGE,swab16(gauge_context.design_energy));
    if (ret < 0){
        goto exit;
    }
    /*poweroff volt*/
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_TERMINATE_VOLT,swab16(gauge_context.terminate_voltage));
    if (ret < 0){
        goto exit;
    }
    /*terminate current*/
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_TAPER_RATE,swab16(gauge_context.taper_rate));
    if (ret < 0){
        goto exit;
    }
    /*recharge volt*/
    ret = bq27421_i2c_write_word_data(di,BQ27421_REG_TAPER_VOLT,swab16(gauge_context.taper_voltage));
    if (ret < 0){
        goto exit;
    }

    tmp_csum = (tmp_csum - old_dc_lsb -old_dc_msb
                        - old_de_lsb -old_de_msb
                        - old_tv_lsb -old_tv_msb
                        - old_tr_lsb -old_tr_msb
                        -old_taper_volt_lsb - old_taper_volt_msb
                        )%256;

    new_csum = 255 - (((tmp_csum
                + (gauge_context.design_capacity & 0xFF)
                + ((gauge_context.design_capacity & 0xFF00) >> 8)
                + (gauge_context.design_energy & 0xFF)
                + ((gauge_context.design_energy & 0xFF00) >> 8)
                + (gauge_context.taper_rate & 0xFF)
                + ((gauge_context.taper_rate & 0xFF00) >> 8)
                + (gauge_context.terminate_voltage & 0xFF)
                + ((gauge_context.terminate_voltage & 0xFF00) >> 8)
                + (gauge_context.taper_voltage & 0xFF)
                + ((gauge_context.taper_voltage & 0xFF00) >> 8)
                )) % 256);

    ret = bq27421_i2c_write_byte_data(di->client, BQ27421_REG_DFDCKS, new_csum);//0x60
    if (ret < 0){
        goto exit;
    }
    msleep(2);
    /* read checksum again to ensure that data was written properly */
    ret = bq27421_set_access_dataclass(di,BQ27421_SUBCLASS_ID_STATE,0x00);/*rw 0-31 byte*/
    if (ret < 0){
        goto exit;
    }

    msleep(2);
    old_csum = bq27421_i2c_read_byte_data(di,BQ27421_REG_DFDCKS);//0x60
    if (old_csum != new_csum){
        dev_info(&di->client->dev,
        "checksum (cfg_param)write failed old:%d, new:%d\n", new_csum, old_csum);
    }else{
        dev_dbg(&di->client->dev,
        "checksum (cfg_param)written old:%d, new:%d\n", new_csum, old_csum);
    }

    ret = bq27421_set_v_chg_term(di);
    if (ret < 0){
        goto exit;
    }

    ret = bq27421_set_opconfig(di);
    if (ret < 0){
        goto exit;
    }

    ret = bq27421_set_lowbattery_threshold(di);
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_set_over_temp(di);
    if (ret < 0){
        goto exit;
    }
    ret = bq27421_set_termV_valid_t(di);
    if (ret < 0){
        goto exit;
    }

    ret = bq27421_hibernate_setting(di);
    if (ret < 0){
        goto exit;
    }
soft_reset:
    /* exit config update mode */
    ret = bq27421_write_control(di,BQ27421_SUBCMD_SOFT_RESET);
    if (ret < 0){
        goto exit;
    }
    msleep(2000);
    while ((bq27421_i2c_read_byte_data(di, BQ27421_REG_FLAGS) & BQ27421_FLAG_CFGUPMODE)) {
        if (time_after(jiffies, timeout)) {
            dev_err(&di->client->dev,"timeout waiting for cfg update\n");
            ret = -ETIMEDOUT;
            goto exit;
        }
        msleep(100);
    }

    while (!(bq27421_get_control_status(di,BQ27421_SUBCMD_CONTROL_STATUS) & 0x80)) {
        if (time_after(jiffies, (jiffies + 3*HZ))) {
            dev_err(&di->client->dev,"init is doing\n");
            ret = -ETIMEDOUT;
            goto exit;
        }
        msleep(100);
    }
    dev_info(di->dev, "init completed\n");

exit:
    /* seal the fuel gauge before exit */
    bq27421_seal(di);
normal:
    gauge_context.state = BQ27421_NORMAL_MODE;
    bq27421_battery_dump_reg(di);
    msleep(6 *1000);
    bq27421_battery_dump_reg(di);
    return ret;
}

static void bq27421_param_cfg_work(struct work_struct *work)
{
        struct bq27421_device_info *di =
                container_of(work, struct bq27421_device_info, cfg_work);

    bq27421_init_gaguge_cfgparam(di);
    return;
}

static int bq27421_battery_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct bq27421_device_info *di;
    int num;
    char *name;
    int retval = 0, ret =0;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev,"can't talk I2C?\n");
        return -ENODEV;
    }

    /* Get new ID for the new battery device */
    mutex_lock(&bq27421_battery_mutex);
    num = idr_alloc(&bq27421_battery_id, client, 0, 0, GFP_KERNEL);
    mutex_unlock(&bq27421_battery_mutex);
    if (num < 0) {
        dev_err(&client->dev,"bq27421 idr_alloc failed!!\n");
        retval = num;
        goto batt_failed_0;
    }

    name = kasprintf(GFP_KERNEL, "bq27421-%d", num);
    if (!name) {
        dev_err(&client->dev, "failed to allocate device name\n");
        retval = -ENOMEM;
        goto batt_failed_1;
    }

    di = kzalloc(sizeof(*di), GFP_KERNEL);
    if (!di) {
        dev_err(&client->dev, "failed to allocate device info data\n");
        retval = -ENOMEM;
        goto batt_failed_2;
    }

    di->id = num;
    i2c_set_clientdata(client,di);
    di->pdata = client->dev.platform_data;
    di->dev = &client->dev;
    di->client = client;
    di->rom_client = NULL;
    bq27421_device = di;
    bq27421_i2c_client = client;
    if(!di->pdata){
        dev_err(&client->dev, "No bq27421 platform data supplied\n");
    }

    retval = bq27421_get_device_type(di);
    if(retval < 0){
        retval = -ENODEV;
        goto batt_failed_1;
    }

    ret = bq27421_get_firmware_version(di);
    if(ret < 0){
        dev_err(&client->dev,"could not read firmware version!\n");
    }else{
        dev_info(&client->dev,"bq27421 read firmware version = 0x%6x\n",ret);
    }

    wake_lock_init(&low_battery_wakelock, WAKE_LOCK_SUSPEND, "gasgauge_wake_lock");
    wake_lock_init(&bqfs_wakelock, WAKE_LOCK_SUSPEND, "bqfs_wake_lock");

    retval = bq27421_init_irq(di);
    if(retval){
        dev_err(&client->dev, "irq init failed\n");
        goto batt_failed_3;
    }

    retval = driver_create_file(&(bq27421_battery_driver.driver), &driver_attr_state);
    if (0 != retval) {
        BQ27421_ERR("failed to create sysfs entry(state): %d\n", ret);
        retval =  -ENOENT;
        goto batt_failed_4;
    }
    retval = sysfs_create_group(&client->dev.kobj, &bq27421_attr_group);
    if (retval < 0){
        dev_err(&client->dev, "could not create sysfs files\n");
        retval = -ENOENT;
        goto batt_failed_5;
    }

    INIT_WORK(&di->cfg_work, bq27421_param_cfg_work);
    schedule_work(&di->cfg_work);

    comip_battery_property_register(&bq27421_battery_property);
    INIT_DEFERRABLE_WORK(&di->gaugelog_work,bq27421_battery_gaugelog_work);
    dev_dbg(&client->dev, "bq27421 Probe Success\n");
    //schedule_delayed_work(&di->gaugelog_work, msecs_to_jiffies(5000));
    return 0;

batt_failed_5:
    driver_remove_file(&(bq27421_battery_driver.driver), &driver_attr_state);
batt_failed_4:
    free_irq(gpio_to_irq(di->client->irq), di);
batt_failed_3:
    wake_lock_destroy(&bqfs_wakelock);
    wake_lock_destroy(&low_battery_wakelock);
batt_failed_2:
    kfree(name);
    name = NULL;
batt_failed_1:
    mutex_lock(&bq27421_battery_mutex);
    idr_remove(&bq27421_battery_id,num);
    mutex_unlock(&bq27421_battery_mutex);
batt_failed_0:
    return retval;
}

static int bq27421_battery_remove(struct i2c_client *client)
{
    struct bq27421_device_info *di = i2c_get_clientdata(client);

    sysfs_remove_group(&client->dev.kobj, &bq27421_attr_group);
    driver_remove_file(&(bq27421_battery_driver.driver), &driver_attr_state);

    bq27421_i2c_client = NULL;

    wake_lock_destroy(&bqfs_wakelock);
    wake_lock_destroy(&low_battery_wakelock);

    free_irq(gpio_to_irq(di->client->irq), di);

    mutex_lock(&bq27421_battery_mutex);
    idr_remove(&bq27421_battery_id, di->id);
    mutex_unlock(&bq27421_battery_mutex);

    kfree(di);

    return 0;
}

static void bq27421_battery_shutdown(struct i2c_client *client)
{
    struct bq27421_device_info *di = i2c_get_clientdata(client);

    bq27421_battery_dump_reg(di);

    return;
}

static const struct i2c_device_id bq27421_id[] = {
    { "bq27421-battery", 0 },
    {},
};

#ifdef CONFIG_PM
static int bq27421_battery_suspend(struct device *dev)
{
    return 0;
}

static int bq27421_battery_resume(struct device *dev)
{
    return 0;
}
#else
#define bq27421_battery_suspend NULL
#define bq27421_battery_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops pm_ops = {
    .suspend    = bq27421_battery_suspend,
    .resume     = bq27421_battery_resume,
};

static struct i2c_driver bq27421_battery_driver = {
    .probe      = bq27421_battery_probe,
    .remove     = bq27421_battery_remove,
    .shutdown   = bq27421_battery_shutdown,
    .id_table   = bq27421_id,
    .driver     = {
        .name   = "bq27421-battery",
        .pm = &pm_ops,
    },
};

static int __init bq27421_battery_init(void)
{
    return i2c_add_driver(&bq27421_battery_driver);
}
fs_initcall(bq27421_battery_init);

static void __exit bq27421_battery_exit(void)
{
    i2c_del_driver(&bq27421_battery_driver);
}
module_exit(bq27421_battery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments Inc");
