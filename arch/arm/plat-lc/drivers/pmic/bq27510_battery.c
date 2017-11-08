/*
 * drivers/power/bq27510_battery.c
 *
 * BQ27510 gasgauge driver
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
#include <plat/bq27510_battery.h>
#include <plat/comip-battery.h>
#include <linux/syscalls.h>

/* If the system has several batteries we need
 * a different name for each of them*/
static DEFINE_IDR(bq27510_battery_id);

static DEFINE_MUTEX(bq27510_battery_mutex);
static DEFINE_MUTEX(data_flash_mutex);

static struct wake_lock low_battery_wakelock;
static struct wake_lock bqfs_wakelock;

static struct i2c_driver bq27510_battery_driver;

struct bq27510_device_info *bq27510_device;
struct i2c_client *bq27510_i2c_client;

enum{
    BQ27510_NORMAL_MODE,
    BQ27510_UPDATE_FIRMWARE,
    BQ27510_LOCK_MODE
};

struct bq27510_context
{
    unsigned int temperature;
    unsigned int capacity;
    unsigned int volt;
    short bat_current;
    unsigned int battery_rm;
    unsigned int battery_fcc;
    unsigned int battery_present;
    unsigned int battery_health;
    unsigned char state;
    unsigned long locked_timeout_jiffies;//after updating firmware, device can not be accessed immediately
    unsigned int i2c_error_count;
    unsigned int lock_count;
    unsigned int normal_capacity;
    unsigned int firmware_version;
};

static struct bq27510_context gauge_context =
{
    .temperature = 2930, //20 degree
    .capacity = 50,      //50 percent
    .volt = 3800,        // 3.8 V
    .bat_current = 200,  // 200 mA
    .battery_rm = 800,   //mAH
    .battery_fcc = 1600, //mAH
    .battery_present = 1,
    .battery_health = POWER_SUPPLY_HEALTH_GOOD,
    .state = BQ27510_NORMAL_MODE,
    .i2c_error_count = 0,
    .normal_capacity = 1650,
    .firmware_version = 0x16000a4,
};

#define GAS_GAUGE_I2C_ERR_STATICS() ++gauge_context.i2c_error_count
#define GAS_GAUGE_LOCK_STATICS() ++gauge_context.lock_count

static int bq27510_is_accessible(void)
{
    if(gauge_context.state == BQ27510_UPDATE_FIRMWARE){
        BQ27510_DBG("bq27510 isn't accessible,It's updating firmware!\n");
        GAS_GAUGE_LOCK_STATICS();
        return 0;
    } else if(gauge_context.state == BQ27510_NORMAL_MODE){
        return 1;
    }else { //if(gauge_context.state == BQ27510_LOCK_MODE)
        if(time_is_before_jiffies(gauge_context.locked_timeout_jiffies)){
            gauge_context.state = BQ27510_NORMAL_MODE;
            return 1;
        }else{
            BQ27510_DBG("bq27510 isn't accessible after firmware updated immediately!\n");
            return 0;
        }
    }
}


static int bq27510_i2c_read_byte_data(struct bq27510_device_info *di, u8 reg)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27510_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_read_byte_data(di->client, reg);
        if (ret < 0) {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27510_ERR("[%s,%d] i2c_smbus_read_byte_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&bq27510_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c read at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}

static int bq27510_i2c_write_byte_data(struct i2c_client *client, u8 reg, u8 value)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27510_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_write_byte_data(client, reg, value);
        if (ret < 0)  {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27510_ERR("[%s,%d] i2c_smbus_write_word_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&bq27510_battery_mutex);

    if(ret < 0){
        dev_err(&client->dev,
        "%s: i2c write at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}

static int bq27510_i2c_read_word_data(struct bq27510_device_info *di, u8 reg)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27510_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_read_word_data(di->client, reg);
        if (ret < 0) {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27510_ERR("[%s,%d] i2c_smbus_read_byte_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&bq27510_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c read at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}

static int bq27510_i2c_write_word_data(struct bq27510_device_info *di, u8 reg, u16 value)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&bq27510_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_write_word_data(di->client, reg, value);
        if (ret < 0)  {
            GAS_GAUGE_I2C_ERR_STATICS();
            //BQ27510_ERR("[%s,%d] i2c_smbus_write_word_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&bq27510_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c write at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}

static int bq27510_i2c_block_read(struct i2c_client *client, u8 reg, u8 *pBuf, u16 len)
{
    int ret = 0;
    int i = 0,j = 0;
    u8 *p = pBuf;

    mutex_lock(&bq27510_battery_mutex);
    for (i = 0; i < len; i += I2C_SMBUS_BLOCK_MAX) {
        j = ((len - i) > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : (len - i);
        ret = i2c_smbus_read_i2c_block_data(client, reg+i, j, p+i);
        if (ret < 0) {
            GAS_GAUGE_I2C_ERR_STATICS();
            BQ27510_ERR("[%s,%d] i2c_transfer failed\n", __FUNCTION__, __LINE__);
            break;
        }
    }
    mutex_unlock(&bq27510_battery_mutex);

    return ret;
}

static int bq27510_i2c_block_write(struct i2c_client *client, u8 reg, u8 *pBuf, u16 len)
{
    int ret = 0;
    int i = 0,j = 0;
    u8 *p = pBuf;

    mutex_lock(&bq27510_battery_mutex);
    for (i = 0; i < len; i += I2C_SMBUS_BLOCK_MAX) {
        j = ((len - i) > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : (len - i);
        ret = i2c_smbus_write_i2c_block_data(client, reg+i, j, p+i);
        if (ret < 0)  {
            GAS_GAUGE_I2C_ERR_STATICS();
            BQ27510_ERR("[%s,%d] i2c_transfer failed\n", __FUNCTION__, __LINE__);
            break;
        }
    }
    mutex_unlock(&bq27510_battery_mutex);

    return ret;
}

static int bq27510_i2c_bytes_read_and_compare(struct i2c_client *client, u8 reg, 
                                                u8 *pSrcBuf, u8 *pDstBuf, u16 len)
{
    int ret = 0;

    ret = bq27510_i2c_block_read(client, reg, pSrcBuf, len);
    if (ret < 0) {
        GAS_GAUGE_I2C_ERR_STATICS();
        BQ27510_ERR("[%s,%d] bq27510_i2c_bytes_read failed\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = strncmp(pDstBuf, pSrcBuf, len);

    return ret;
}


static int bq27510_set_access_dataclass(struct bq27510_device_info *di,u8 subclass, u8 offset)
{
    int ret = 0;
    u8 block_num= offset / 32;

    /* setup fuel gauge state data block block for ram access */
    ret = bq27510_i2c_write_byte_data(di->client, BQ27510_REG_DFDCNTL, 0x00);//0x61
    if (ret < 0){
        goto exit;
    }
    ret = bq27510_i2c_write_byte_data(di->client, BQ27510_REG_DFCLS, subclass);//0x3E
    if (ret < 0){
        goto exit;
    }
    ret = bq27510_i2c_write_byte_data(di->client, BQ27510_REG_DFBLK, block_num);//0x3F
    if (ret < 0){
        goto exit;
    }
    return 0;
exit:
    return ret;
}

static int bq27510_access_data_flash(struct bq27510_device_info *di,
                        u8 subclass,u8 offset, u8 *buf, int len, bool write)
{
    int i = 0;
    unsigned sum= 0;
    u8 cmd[33];
    u8 block_num= offset / 32;
    u8 cmd_code = BQ27510_REG_DFD + offset % 32;
    u8 checksum = 0;
    int ret = 0;

    mutex_lock(&data_flash_mutex);

    /* setup fuel gauge state data block block for ram access */
    cmd[0] = BQ27510_REG_DFDCNTL;
    cmd[1] = 0x00;
    ret = bq27510_i2c_block_write(di->client,cmd[0],&cmd[1],2);
    if (ret < 0){
        goto exit;
    }

    /* Set subclass */
    cmd[0] = BQ27510_REG_DFCLS;
    cmd[1] = subclass;
    //ret = bq27510_i2c_block_write(di->client,cmd[0],&cmd[1],2);
    ret = bq27510_i2c_write_byte_data(di->client,cmd[0],cmd[1]);
    if (ret < 0){
        goto exit;
    }

    if(block_num >= 0) {
        /* Set block to R/W */
        cmd[0] = BQ27510_REG_DFBLK;
        cmd[1] = block_num;
        ret = bq27510_i2c_block_write(di->client,cmd[0],&cmd[1],2);
        //ret = bq27510_i2c_write_byte_data(di->client,cmd[0],cmd[1]);
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
        ret = bq27510_i2c_block_write(di->client,cmd[0],&cmd[1],(len + 1));
        if (ret < 0){
            goto exit;
        }

        /* Set checksum */
        cmd[0] = BQ27510_REG_DFDCKS;
        cmd[1] = checksum;
        ret = bq27510_i2c_block_write(di->client,cmd[0],&cmd[1],2);
        if (ret < 0){
            goto exit;
        }
    } else {
        /* Read data */
        ret = bq27510_i2c_block_read(di->client, cmd_code, buf, len);
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

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
int bq27510_battery_voltage(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;

    if(!bq27510_is_accessible())
        return gauge_context.volt;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_VOLT);
    if ((data < 0)||(data > 6000)) {
        dev_err(di->dev, "error %d reading voltage\n", data);
        data = gauge_context.volt;
    } else{
        gauge_context.volt = data;
    }

    BQ27510_DBG("read voltage result = %d mVolts\n", data);
    return data ;
}

/*
 * Return the battery average current
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
short bq27510_battery_current(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;
    short nCurr = 0;

    if(!bq27510_is_accessible())
        return gauge_context.bat_current;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_AI);
    if (data < 0) {
        dev_err(di->dev, "error %d reading current\n", data);
        data = gauge_context.bat_current;
    } else{
        gauge_context.bat_current = (signed short)data;
    }

    nCurr = (signed short)data;

    BQ27510_DBG("read current result = %d mA\n", nCurr);

    return gauge_context.bat_current;
}

/*
 * Return the battery Relative State-of-Charge
 * The reture value is 0 - 100%
 * Or < 0 if something fails.
 */
int bq27510_battery_capacity(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;

    if(!bq27510_is_accessible())
        return gauge_context.capacity;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_SOC);
    if((data < 0)||(data > 100)) {
        dev_err(di->dev, "error %d reading capacity\n", data);
        data = gauge_context.capacity;
    }else{
        gauge_context.capacity = data;
    }
    BQ27510_DBG("read soc result = %d Hundred Percents\n", data);

    return data;
}

static int bq27510_battery_realsoc(void)
{
    return gauge_context.capacity;
}
/*
 * Return the battery temperature in Celcius degrees
 * Or < 0 if something fails.
 */
int bq27510_battery_temperature(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = -1;

    if(!bq27510_is_accessible())
        return ((gauge_context.temperature-CONST_NUM_2730)/CONST_NUM_10);

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_TEMP);
    if ((data < 0)||(data > 6553)){
        dev_err(di->dev, "error %d reading temperature\n", data);
        data = gauge_context.temperature;
    }else{
        gauge_context.temperature = data;
    }

    data = (data-CONST_NUM_2730)/CONST_NUM_10;
    BQ27510_DBG("read temperature result = %d Celsius\n", data);

    return data;
}

/*===========================================================================
  Description:    get the status of battery exist
  Return:         1->battery present; 0->battery not present.
===========================================================================*/
int is_bq27510_battery_exist(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;

    if(!bq27510_is_accessible())
        return gauge_context.battery_present;

	data = bq27510_i2c_read_word_data(di,BQ27510_REG_FLAGS);
    BQ27510_DBG("is_exist flags result = 0x%x \n", data);

    if (data < 0) {
        BQ27510_DBG("is_k3_bq27510_battery_exist failed!!\n");
        data = bq27510_i2c_read_word_data(di,BQ27510_REG_FLAGS);
    }

    if(data >= 0){
        gauge_context.battery_present = !!(data & BQ27510_FLAG_BAT_DET);
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
int bq27510_battery_rm(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = -1;

    if(!bq27510_is_accessible())
        return gauge_context.battery_rm;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_RM);
    if(data < 0) {
        BQ27510_ERR("i2c error in reading remain capacity!");
        dev_err(di->dev, "error %d reading rm\n", data);
        data = gauge_context.battery_rm;
    } else{
        gauge_context.battery_rm = data;
    }

    BQ27510_DBG("read rm result = %d mAh\n",data);
    return data;
}

/*
 * Return the battery FullChargeCapacity
 * The reture value is mAh
 * Or < 0 if something fails.
 */
int bq27510_battery_fcc(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;

    if(!bq27510_is_accessible())
        return gauge_context.battery_fcc;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_FCC);
    if(data < 0) {
        BQ27510_ERR("i2c error in reading fcc!");
        dev_err(di->dev, "error %d reading fcc\n", data);
        data = gauge_context.battery_fcc;
    } else{
        gauge_context.battery_fcc = data;
    }

    BQ27510_DBG("read fcc result = %d mAh\n",data);
    return data;
}

/*
 * Return whether the battery charging is full
 *
 */
int is_bq27510_battery_full(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;

    if(!bq27510_is_accessible())
        return 0;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_FLAGS);
    if (data < 0) {
        BQ27510_DBG("is_bq27510_battery_full failed!!\n");
        dev_err(di->dev, "error %d reading battery full\n", data);
        return 0;
    }

    BQ27510_DBG("read flags result = 0x%x \n", data);
    return(data & BQ27510_FLAG_FC);
}

/*===========================================================================
 Description: get the health status of battery
 Input: struct bq27510_device_info *
 Output: none
 Return: 0->"Unknown", 1->"Good", 2->"Overheat", 3->"Dead", 4->"Over voltage",
 5->"Unspecified failure", 6->"Cold",
 Others: none
===========================================================================*/
int bq27510_battery_health(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;
    int status = 0;

    if(!bq27510_is_accessible())
        return gauge_context.battery_health;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_FLAGS);
    if(data < 0) {
        dev_err(di->dev, "error %d reading battery health\n", data);
        return POWER_SUPPLY_HEALTH_GOOD;
    }

    if (data & (BQ27510_FLAG_OTC | BQ27510_FLAG_OTD)){
        status = POWER_SUPPLY_HEALTH_OVERHEAT;
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
int is_bq27510_battery_reach_threshold(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;

    if(!bq27510_is_accessible())
        return 0;

    data = bq27510_i2c_read_word_data(di,BQ27510_REG_FLAGS);
    if(data < 0)
        return 0;

    return data;
}

/*===========================================================================
  Function:       bq27510_battery_capacity_level
  Description:    get the capacity level of battery
  Input:          struct bq27510_device_info *
  Output:         none
  Return:         capacity percent
  Others:         none
===========================================================================*/
int bq27510_battery_capacity_level(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int data = 0;
    int data_capacity = 0;
    int status = 0;

    if(!bq27510_is_accessible())
       return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;

    data = bq27510_i2c_read_word_data(di, BQ27510_REG_FLAGS);

    BQ27510_DBG("read capactiylevel result = %d \n", data);
    if(data < 0)
        return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

    if (data & BQ27510_FLAG_SOCF){
        status = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
    }else if (data & BQ27510_FLAG_SOC1){
        status = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
    }else if (data & BQ27510_FLAG_FC){
        status = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
    }else{
        data_capacity = gauge_context.capacity;//bq27510_battery_capacity();
        if (data_capacity > 95){
            status = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
        }else{
            status = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
        }
    }

    return status;
}

/*===========================================================================
  Function:       bq27510_battery_technology
  Description:    get the battery technology
  Input:          struct bq27510_device_info *
  Output:         none
  Return:         value of battery technology
  Others:         none
===========================================================================*/
int bq27510_battery_technology(void)
{
    /*Default technology is "Li-poly"*/
    int data = POWER_SUPPLY_TECHNOLOGY_LIPO;

    return data;
}

/*===========================================================================
  Function:       bq27510_write_control
  Description:    get qmax
  Input:          struct bq27510_device_info *
  Output:         none
  Return:         value of 
  Others:         none
===========================================================================*/
static int bq27510_write_control(struct bq27510_device_info *di,int subcmd)
{
    int data = 0;

    data = bq27510_i2c_write_word_data(di,BQ27510_REG_CTRL,subcmd);
    if(data < 0){
        dev_dbg(&di->client->dev,
            "Failed write sub command:0x%x. data=%d\n", subcmd, data);
    }
    dev_dbg(&di->client->dev, "%s() subcmd=0x%x data=%d\n",__func__, subcmd, data);

    return ((data < 0)? data:0);
}

/*===========================================================================
  Function:       bq27510_get_control_status
  Description:    get qmax
  Input:          struct bq27510_device_info *
  Output:         none
  Return:         value of 
  Others:         none
===========================================================================*/
static int bq27510_get_control_status(struct bq27510_device_info *di,int subcmd)
{
    int data = 0;

    if(!bq27510_is_accessible()){
        return 0;
    }
    bq27510_write_control(di,subcmd);
    if(data < 0){
        goto exit;
    }
    mdelay(2);
    data = bq27510_i2c_read_word_data(di,BQ27510_REG_CTRL);
    mdelay(2);
    if(data < 0){
        dev_err(&di->client->dev,"Failed to read control reg.data = %d\n",data);
    }
exit:
    return (data);
}

/*===========================================================================
  Function:       bq27510_check_if_sealed
  Description:    check bq27510 is sealed
===========================================================================*/
static bool bq27510_check_if_sealed(struct bq27510_device_info *di)
{
    int control_status = 0;

    control_status = bq27510_get_control_status(di,BQ27510_SUBCMD_CONTROL_STATUS);
    if(control_status < 0){
        di->sealed = -1;
        return 0;
    }
    di->sealed = !!(control_status & BQ27510_SEALED_MASK);

    return di->sealed;
}

/*===========================================================================
  Function:       bq27510_unseal
  Description:    config bq27510 unseal
===========================================================================*/
static int bq27510_unseal(struct bq27510_device_info *di)
{
    int rc = 0;
    int last_cmd = 0;

    rc = bq27510_write_control(di,BQ27510_SUBCMD_ENTER_CLEAR_SEAL);
    if (rc < 0) {
        last_cmd = BQ27510_SUBCMD_ENTER_CLEAR_SEAL;
        goto unseal_exit;
    }

    msleep(2);
    rc = bq27510_write_control(di,BQ27510_SUBCMD_CLEAR_SEALED);
    if (rc < 0) {
        last_cmd = BQ27510_SUBCMD_CLEAR_SEALED;
        goto unseal_exit;
    }

    msleep(2);
    rc = bq27510_write_control(di,BQ27510_SUBCMD_CLEAR_FULL_ACCESS_SEALED);
    if (rc < 0) {
        last_cmd = BQ27510_SUBCMD_CLEAR_FULL_ACCESS_SEALED;
        goto unseal_exit;
    }

    msleep(2);
    rc = bq27510_write_control(di,BQ27510_SUBCMD_CLEAR_FULL_ACCESS_SEALED);
    if (rc < 0) {
        last_cmd = BQ27510_SUBCMD_CLEAR_FULL_ACCESS_SEALED;
        goto unseal_exit;
    }
    msleep(2);

    di->sealed = 0;

    return rc;

unseal_exit:
    dev_err(&di->client->dev, "Failed write unseal command :%#x\n",last_cmd);
    di->sealed = -1;
    return rc;
}

/*===========================================================================
  Function:       bq27510_get_qmax
  Description:    get qmax
  Input:          struct bq27510_device_info *
  Output:         none
  Return:         value of qmax
  Others:         none
===========================================================================*/
static int bq27510_get_qmax(struct bq27510_device_info *di)
{
    int qmax = 0,qmax1 = 0,data = 0;
    int error = -1,ret = 0;

    if(!bq27510_is_accessible()){
        goto err;
    }
    if(bq27510_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27510_unseal(di);
        if(0 != ret){
            dev_err(&di->client->dev,"Can not unseal device\n");
            goto err;
        }
    }
    bq27510_set_access_dataclass(di,BQ27510_SUBCLASS_ID_STATE,0x02);
    mdelay(2);
    qmax = bq27510_i2c_read_byte_data(di,BQ27510_REG_QMAX);
    if(qmax < 0){
        goto err;
    }
    mdelay(2);
    qmax1 = bq27510_i2c_read_byte_data(di,BQ27510_REG_QMAX1);
    if(qmax1 < 0){
        goto err;
    }
    data = (qmax << 8) | qmax1;

    return data;
err:
    return (error);
}

/*===========================================================================
  Function:       bq27510_get_firmware_version
  Description:    get qmax
  Input:          struct bq27510_device_info *
  Output:         none
  Return:         value of firmware_version
  Others:         none
===========================================================================*/
static int bq27510_get_firmware_version(struct bq27510_device_info *di)
{
    unsigned int data = 0;
    int id = 0,ver = 0,ret = 0;

    if(!bq27510_is_accessible()){
        goto err;
    }
    if(bq27510_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27510_unseal(di);
        if(0 != ret){
            dev_err(&di->client->dev,"Can not unseal device\n");
            goto err;
        }
    }

    mdelay(2);
    id = bq27510_get_control_status(di,BQ27510_SUBCMD_CHEM_ID);
    mdelay(2);
    if (0 != bq27510_set_access_dataclass(di,BQ27510_SUBCLASS_ID_FIRMWARE_VERSION,0x00)) {
        goto err;
    }
    mdelay(2);
    ver = bq27510_i2c_read_byte_data(di,BQ27510_REG_FLASH);//0x40
    if (id < 0 || ver < 0)
        goto err;
    data = (id << 16) | ver;

    return data;
err:
    return (-1);
}

/*===========================================================================
  Function:       bq27510_devices_reset
  Description:    
  Input:          struct bq27510_device_info *
  Output:         none
  Return:         value of 
  Others:         none
===========================================================================*/
static int bq27510_devices_reset(struct bq27510_device_info *di)
{
    return bq27510_write_control(di,BQ27510_SUBCMD_RESET);
}


EXPORT_SYMBOL_GPL(bq27510_battery_voltage);
EXPORT_SYMBOL_GPL(bq27510_battery_current);
EXPORT_SYMBOL_GPL(bq27510_battery_capacity);
EXPORT_SYMBOL_GPL(bq27510_battery_temperature);
EXPORT_SYMBOL_GPL(is_bq27510_battery_exist);
EXPORT_SYMBOL_GPL(bq27510_battery_rm);
EXPORT_SYMBOL_GPL(bq27510_battery_fcc);
EXPORT_SYMBOL_GPL(is_bq27510_battery_full);
EXPORT_SYMBOL_GPL(is_bq27510_battery_reach_threshold);
EXPORT_SYMBOL_GPL(bq27510_battery_health);
EXPORT_SYMBOL_GPL(bq27510_battery_capacity_level);
EXPORT_SYMBOL_GPL(bq27510_battery_technology);

/*battery information interface*/

/*return: 1 = true, 0 = false*/
static int bq27510_check_qmax_property(void)
{
    int data = 0;
    int design_capacity = 0;
    int status = 0;
    struct bq27510_device_info *di = bq27510_device;

   if(!bq27510_is_accessible())
       return 0;

    data = bq27510_get_qmax(di);
    design_capacity = gauge_context.normal_capacity;
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
static int is_bq27510_firmware_check(void)
{
    struct bq27510_device_info *di = bq27510_device;
    int version = 0,status = 0;

    version = bq27510_get_firmware_version(di);
    /*check firmware version is the same id*/
    if(version == gauge_context.firmware_version){
        status = bq27510_check_qmax_property();
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
int bq27510_firmware_model_update(char* dev)
{
    struct bq27510_device_info *di = bq27510_device;
    int fd = 0,soc = 0;
    int ret = 0;
    mm_segment_t oldfs;
    static int timeout = 5000;

    if(is_bq27510_firmware_check()){
        timeout = 0;
        return 0;
    }
    soc = bq27510_battery_capacity();
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
        dev_err(di->dev, "sys_open bq27510 open sysfs failed!\n");
        set_fs(oldfs);
        return fd;
    }

    ret = sys_write(fd, BSP_UPGRADE_FIRMWARE_BQFS_NAME,
                    sizeof(BSP_UPGRADE_FIRMWARE_BQFS_NAME));
    if(ret < 0){
        dev_err(di->dev, "sys_write bq27510 firmware failed!\n");
    }
    set_fs(oldfs);
    sys_close(fd);
    return ret;
}
EXPORT_SYMBOL_GPL(bq27510_firmware_model_update);

static struct comip_android_battery_info bq27510_battery_property={
    .battery_voltage = bq27510_battery_voltage,
    .battery_current = bq27510_battery_current,
    .battery_capacity = bq27510_battery_capacity,
    .battery_temperature = bq27510_battery_temperature,
    .battery_exist = is_bq27510_battery_exist,
    .battery_health= bq27510_battery_health,
    .battery_rm = bq27510_battery_rm,
    .battery_fcc = bq27510_battery_fcc,
    .capacity_level = bq27510_battery_capacity_level,
    .battery_technology = bq27510_battery_technology,
    .battery_full = is_bq27510_battery_full,
    .firmware_update = bq27510_firmware_model_update,
    .battery_realsoc = bq27510_battery_realsoc,
};



/*
 * Use BAT_LOW not BAT_GD. When battery capacity is below SOC1,
 * BAT_LOW PIN will pull up and cause a
 * interrput, this is the interrput callback.
 */
static irqreturn_t bq27510_interrupt(int irq, void *_di)
{
    struct bq27510_device_info *di = _di;

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
    struct bq27510_device_info *di = container_of(work,
        struct bq27510_device_info, notifier_work.work);

    int low_bat_flag = is_bq27510_battery_reach_threshold();

    if(!bq27510_is_accessible())
        goto exit;
    if(!is_bq27510_battery_exist() || !(low_bat_flag & BQ27510_FLAG_SOC1))
        goto exit;
#if 0
    if(time_is_after_jiffies(di->timeout_jiffies)){
        di->timeout_jiffies = jiffies + msecs_to_jiffies(DISABLE_ACCESS_TIME);
        goto exit;
    }
#endif
    if(!(low_bat_flag & BQ27510_FLAG_SOCF)){
        //wake_lock(&low_battery_wakelock);
        wake_lock_timeout(&low_battery_wakelock, 10 * HZ);
        //SOC1 operation:notify upper to get the level now
        dev_info(&di->client->dev,"low battery warning event\n");
    }else if(low_bat_flag & BQ27510_FLAG_SOCF){
        //wake_lock(&low_battery_wakelock);
        wake_lock_timeout(&low_battery_wakelock, 10 * HZ);
        dev_info(&di->client->dev,"low battery shutdown event\n");
    }else{
        goto exit;
    }

    //di->timeout_jiffies = jiffies + msecs_to_jiffies(DISABLE_ACCESS_TIME);
exit:
    if(wake_lock_active(&low_battery_wakelock)) {
        //wake_lock_timeout(&low_battery_wakelock, 10 * HZ);
        wake_unlock(&low_battery_wakelock);
    }
    return;
}

static int bq27510_init_irq(struct bq27510_device_info *di)
{
    int ret = 0;

    INIT_DELAYED_WORK(&di->notifier_work,interrupt_notifier_work);

    if (gpio_request(di->client->irq, "bq27510_irq") < 0){
        ret = -ENOMEM;
        goto irq_fail;
    }

    gpio_direction_input(di->client->irq);

    /* request irq interruption */
    ret = request_irq(gpio_to_irq(di->client->irq), bq27510_interrupt, 
        IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,"bq27510_irq",di);
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

/*===========================================================================
  Function:       bq27510_enter_rom_mode
  Description:    change i2c addr to 0x0b
===========================================================================*/
static int bq27510_enter_rom_mode(struct bq27510_device_info *di)
{
    int data = 0;

    if(di->rom_client){
        return -EALREADY;
    }

    di->rom_client = i2c_new_dummy(di->client->adapter,BQ27510_ROM_MODE_I2C_ADDR);
    if (!di->rom_client) {
        dev_err(&di->client->dev, "Failed creating ROM i2c access\n");
        return -EIO;
    }

    data = bq27510_i2c_write_word_data(di, BSP_ENTER_ROM_MODE_CMD, BSP_ENTER_ROM_MODE_DATA);
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
  Function:       bq27510_exit_rom_mode
  Description:    change i2c addr to 0x0055 
===========================================================================*/
static int bq27510_exit_rom_mode(struct bq27510_device_info *di)
{
    int data = 0;

    if(!di->rom_client){
        return -EFAULT;
    }

    data = bq27510_i2c_write_byte_data(di->rom_client,
        BSP_LEAVE_ROM_MODE_REG1,BSP_LEAVE_ROM_MODE_DATA1);
    if(data < 0){
        dev_err(&di->client->dev, "Fail exit ROM mode. data=%d\n",data);
        goto unregister_rom;
    }
    msleep(2);
    data = bq27510_i2c_write_byte_data(di->rom_client,
        BSP_LEAVE_ROM_MODE_REG2,BSP_LEAVE_ROM_MODE_DATA2);
    if(data < 0){
        dev_err(&di->client->dev, "Send Checksum for LSB. data=%d\n",data);
        goto unregister_rom;
    }
    msleep(2);
    data = bq27510_i2c_write_byte_data(di->rom_client,
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
  Function:       bq27510_rommode_read_first_two_rows
  Description:    read_first_two_rows flash data
===========================================================================*/
static int bq27510_rommode_read_first_two_rows(struct bq27510_device_info *di)
{
    u8 buf[192];
    u8 cmd[3];
    int ret = 0;
    int i = 0;

    for(i=0;i<2;i++){
        cmd[0] = 0x00;
        cmd[1] = 0x03;
        ret = bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],2);
        if(ret < 0){
            dev_err(&di->rom_client->dev, "failed write cmd = %x\n",cmd[0]);
            goto exit;
        }
        cmd[0] = 0x01;
        cmd[1] = 0x00+i;  /* 0x01 */
        cmd[2] = 0x00;  /* 0x02 */
        ret = bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        if(ret < 0){
            dev_err(&di->rom_client->dev, "failed write cmd = %x\n",cmd[0]);
            goto exit;
        }
        cmd[0] = 0x64;
        cmd[1] = 0x00+i;  /* 0x64 */
        cmd[2] = 0x00;  /* 0x65 */
        ret = bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        if(ret < 0){
            dev_err(&di->rom_client->dev, "failed write cmd = %x\n",cmd[0]);
            goto exit;
        }
        ret = bq27510_i2c_block_read(di->rom_client, 0x04,&buf[i*96],96);
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
  Function:       bq27510_rommode_erase_first_two_rows
  Description:    erase_first_two_rows flash data
===========================================================================*/
static int bq27510_rommode_erase_first_two_rows(struct bq27510_device_info *di)
{
    u8 cmd[3];
    u8 buf;
    u8 retries = 0;
    int ret = 0;

    ret = bq27510_rommode_read_first_two_rows(di);
    if(ret < 0){
        dev_err(&di->rom_client->dev, " read first two low failed\n");
        return -1;
    }

    do {
        retries++;
        cmd[0] = 0x00;
        cmd[1] = 0x03;
        bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],2);
        cmd[0] = 0x64;
        cmd[1] = 0x03;  /* 0x64 */
        cmd[2] = 0x00;  /* 0x65 */
        bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],3);
        mdelay(20);
        bq27510_i2c_block_read(di->rom_client, 0x66,&buf,sizeof(buf));
        if (buf == 0x00)
            return 0;

        if (retries > 3)
            break;
    } while (1);

    return -1;
}

#if 0
static int bq27510_rommode_mass_erase_cmd(struct bq27510_device_info *di)
{
    u8 cmd[3];
    u8 buf;
    u8 retries = 0;

    do {
        retries++;

        cmd[0] = 0x00;
        cmd[1] = 0x0C;
        bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],2);
        cmd[0] = 0x04;
        cmd[1] = 0x83;  /* 0x04 */
        cmd[2] = 0xDE;  /* 0x05 */
        bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        cmd[0] = 0x64;
        cmd[1] = 0x6D;  /* 0x64 */
        cmd[2] = 0x01;  /* 0x65 */
        bq27510_i2c_block_write(di->rom_client,cmd[0],&cmd[1],sizeof(cmd));
        mdelay(200);

        bq27510_i2c_block_read(di->rom_client, 0x66,&buf,sizeof(buf));
        if (buf == 0x00)
            return 0;

        if (retries > 3)
            break;
    } while (1);

    return -1;
}
#endif


static int bq27510_atoi(const char *s)
{
    int k = 0;

    k = 0;
    while (*s != '\0' && *s >= '0' && *s <= '9') {
        k = 10 * k + (*s - '0');
        s++;
    }
    return k;
}

static unsigned long bq27510_strtoul(const char *cp, unsigned int base)
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


static int bq27510_firmware_program(struct i2c_client *client, const unsigned char *pgm_data, unsigned int filelen)
{
    unsigned int i = 0, j = 0, ulDelay = 0, ulReadNum = 0;
    unsigned int ulCounter = 0, ulLineLen = 0;
    unsigned char temp = 0; 
    unsigned char *p_cur;
    unsigned char pBuf[BSP_MAX_ASC_PER_LINE] = { 0 };
    unsigned char p_src[BSP_I2C_MAX_TRANSFER_LEN] = { 0 };
    unsigned char p_dst[BSP_I2C_MAX_TRANSFER_LEN] = { 0 };
    unsigned char ucTmpBuf[16] = { 0 };

    dev_info(&client->dev,"gasgauge firmware is downloading ...\n");
bq27510_firmware_program_begin:
    if(ulCounter > 10){
        return -1;
    }

    p_cur = (unsigned char *)pgm_data;

    while(1){
        while (*p_cur == '\r' || *p_cur == '\n'){
            p_cur++;
        }
        if((p_cur - pgm_data) >= filelen){
            printk("Download success\n");
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
                p_src[2+j] = bq27510_strtoul(ucTmpBuf, 16);
            }
            temp = (ulLineLen -2)/2;
            ulLineLen = temp + 2;
        }else if('X' == p_src[0]){
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+2, ulLineLen-2);
            ulDelay = bq27510_atoi(ucTmpBuf);
        }else if('R' == p_src[0]){
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+2, 2);
            p_src[2] = bq27510_strtoul(ucTmpBuf, 16);
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+4, 2);
            p_src[3] = bq27510_strtoul(ucTmpBuf, 16);
            memset(ucTmpBuf, 0x00, sizeof(ucTmpBuf));
            memcpy(ucTmpBuf, pBuf+6, ulLineLen-6);
            ulReadNum = bq27510_atoi(ucTmpBuf);
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

                    if(bq27510_i2c_block_write(client, p_src[3], &p_src[4], ulLineLen-4) < 0){
                        dev_err(&client->dev,"[%s,%d] bq27510_i2c_bytes_write failed len=%d\n",__FUNCTION__,__LINE__,ulLineLen-4);
                    }
                    break;

                case 'R' :
                    if(bq27510_i2c_block_read(client, p_src[3], p_dst, ulReadNum) < 0){
                        dev_err(&client->dev,"[%s,%d] bq27510_i2c_bytes_read failed\n",__FUNCTION__,__LINE__);
                    }
                    break;

                case 'C' :
                    if(bq27510_i2c_bytes_read_and_compare(client, p_src[3], p_dst, &p_src[4], ulLineLen-4)){
                        ulCounter++;
                        dev_err(&client->dev,"[%s,%d] bq27510_i2c_bytes_read_and_compare failed\n",__FUNCTION__,__LINE__);
                        goto bq27510_firmware_program_begin;
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

static int bq27510_firmware_download(struct bq27510_device_info *di, const unsigned char *pgm_data, unsigned int len)
{
    int iRet;

    gauge_context.state = BQ27510_UPDATE_FIRMWARE;

    if(bq27510_check_if_sealed(di)){
        dev_info(&di->client->dev, "Can not enter ROM mode. "
        "Must unseal device first\n");
        iRet = bq27510_unseal(di);
        if(0 != iRet){
            dev_err(&di->client->dev,
                "Can not unseal device\n");
            goto exit;
        }
    }

    /*Enter Rom Mode to change i2c addr*/
    iRet = bq27510_enter_rom_mode(di);
    if(0 != iRet){
        dev_err(di->dev, "Fail enter ROM mode\n");
        goto exit;
    }
    mdelay(2);

    iRet = bq27510_rommode_erase_first_two_rows(di);
    if(0 != iRet){
        dev_err(&di->rom_client->dev, "Erasing IF first two rows was failed\n");
        goto exit;
    }
#if 0
    iRet = bq27510_rommode_mass_erase_cmd(di);
    if(0 != iRet){
        dev_err(&di->rom_client->dev, "Mass Erase procedure was failed\n");
        goto exit;
    }
#endif
    /*program bqfs*/
    iRet = bq27510_firmware_program(di->rom_client, pgm_data, len);
    if(0 != iRet){
        printk(KERN_ERR "[%s,%d] bq275x0_firmware_program failed\n",__FUNCTION__,__LINE__);
        goto exit;
    }
    /*exit Rom Mode to change i2c addr*/
    bq27510_exit_rom_mode(di);

    gauge_context.locked_timeout_jiffies = jiffies + msecs_to_jiffies(5000);
    gauge_context.state = BQ27510_LOCK_MODE;

    if(0 != bq27510_devices_reset(di)){ /* reset cmd*/
        dev_err(di->dev, "Download Reset Fail\n");
    }else{
        dev_info(di->dev, "Download Reset Success\n");
    }

    return iRet;
exit:
    gauge_context.state = BQ27510_LOCK_MODE;
    dev_err(di->dev, "downloading fail \n");
    /*exit Rom Mode to change i2c addr*/
    bq27510_exit_rom_mode(di);
    return iRet;
}

static int bq27510_update_firmware(struct bq27510_device_info *di, const char *pFilePath) 
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
    printk("bq27510 firmware image size is %d \n",length);
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
        if(0==(ret = bq27510_firmware_download(di, (const char*)buf, length)))
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
static ssize_t bq27510_attr_store(struct device_driver *driver, 
                            const char *buf, size_t count)
{
    struct bq27510_device_info *di = bq27510_device;
    int iRet = 0;
    unsigned char path_image[255];

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
    iRet = bq27510_update_firmware(di, path_image);
    if(iRet < 0){
        wake_unlock(&bqfs_wakelock);
        return -1;
    }
    wake_unlock(&bqfs_wakelock);

    return count;
}

static ssize_t bq27510_attr_show(struct device_driver *driver, char *buf)
{
    struct bq27510_device_info *di = bq27510_device;
    int iRet = 0;

    if(!bq27510_is_accessible()) {
        return sprintf(buf,"bq27510 is busy because of updating(%d)\n",gauge_context.state);
    }

    if(NULL == buf){
        return -1;
    }

    iRet = bq27510_get_firmware_version(di);
    if(iRet < 0){
        return sprintf(buf, "%s", "Coulometer Damaged or Firmware Error");
    }else{
        return sprintf(buf, "%x\n", iRet);
    }
}

static DRIVER_ATTR(state, 0664, bq27510_attr_show, bq27510_attr_store);

/*define a sysfs interface for firmware upgrade*/
static ssize_t bq27510_fg_store(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    struct bq27510_device_info *di = dev_get_drvdata(dev);
    int iRet = 0;
    unsigned char path_image[255];

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
    iRet = bq27510_update_firmware(di, path_image);
    if(iRet < 0){
        wake_unlock(&bqfs_wakelock);
        return -1;
    }

    wake_unlock(&bqfs_wakelock);

    return count;
}

static ssize_t bq27510_fg_show(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27510_device_info *di = dev_get_drvdata(dev);
    int iRet = 0;

    if(!bq27510_is_accessible()) {
        return sprintf(buf,"bq27510 is busy because of updating(%d)\n",gauge_context.state);
    }

    if(NULL == buf){
        return -1;
    }

    iRet = bq27510_get_firmware_version(di);
    if(iRet < 0){
        return sprintf(buf, "%s", "Coulometer Damaged or Firmware Error");
    }else{
        return sprintf(buf, "%x\n", iRet);
    }
}

static ssize_t bq27510_show_gaugelog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27510_device_info *di = dev_get_drvdata(dev);
    int volt, cur, capacity, temp, rm, fcc, control_status;
    u16 flag = 0;
    int qmax = 0;

    if(!bq27510_is_accessible())
        return sprintf(buf,"bq27510 is busy because of updating(%d)\n",gauge_context.state);

    temp =  bq27510_battery_temperature();
    mdelay(2);
    volt = bq27510_battery_voltage();
    mdelay(2);
    cur = bq27510_battery_current();
    mdelay(2);
    capacity = bq27510_battery_capacity();
    mdelay(2);
    flag = is_bq27510_battery_reach_threshold();
    mdelay(2);
    rm =  bq27510_battery_rm();
    mdelay(2);
    fcc = bq27510_battery_fcc();
    mdelay(2);
    control_status = bq27510_get_control_status(di,BQ27510_SUBCMD_CONTROL_STATUS);
    mdelay(2);
    qmax = bq27510_get_qmax(di);
    if(qmax < 0){
        return sprintf(buf, "%s", "Coulometer Damaged or Firmware Error \n");
    }else{
        sprintf(buf, "%-6d  %-6d  %-4d  %-6d  0x%-5.4x  %-6d  %-6d  0x%-5.4x  %-6d\n",
                    volt, cur, capacity, temp, flag, rm, fcc, control_status, qmax);
        return strlen(buf);
    }
}

static ssize_t bq27510_show_firmware_version(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    int val;
    struct bq27510_device_info *di = dev_get_drvdata(dev);
    int error = 0;

    if(!bq27510_is_accessible()) {
        return sprintf(buf,"bq27510 is busy because of updating(%d)\n",gauge_context.state);
    }

    val = bq27510_get_firmware_version(di);
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

static ssize_t bq27510_show_voltage(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    int val = 0;
    //struct bq27510_device_info *di = dev_get_drvdata(dev);

    if (NULL == buf) {
        BQ27510_ERR("bq27510_show_voltage Error\n");
        return -1;
    }
    val = bq27510_battery_voltage();

    val *=1000;

    return sprintf(buf, "%d\n", val);
}

static ssize_t bq27510_show_data_flash(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct bq27510_device_info *di = dev_get_drvdata(dev);
    int val = 0,chgvolt = 0,iterm = 0, descap = 0,soc1 = 0, socf = 0;
    int error = 0,ret = 0;
    u8 data_flash[32] = {0};

    if(!bq27510_is_accessible()) {
        return sprintf(buf,"bq27510 is busy because of updating(%d)\n",gauge_context.state);
    }
    if(bq27510_check_if_sealed(di)){
        dev_info(di->dev, "sealed device\n");
        ret = bq27510_unseal(di);
        if(0 != ret){
            dev_err(di->dev, "Can not unseal device\n");
            error = -1;
            goto exit;
        }
    }
    /*chg volt*/
     ret = bq27510_access_data_flash(di,BQ27510_SUBCLASS_ID_CHG,2,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    chgvolt = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);

    /*chg term current*/
    ret = bq27510_access_data_flash(di,BQ27510_SUBCLASS_ID_CHG_ITERM,4,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    iterm = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);

    /*design capcity*/
    ret = bq27510_access_data_flash(di,BQ27510_SUBCLASS_ID_DATA,10,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    descap = (data_flash[0]<< 8)|data_flash[1];
    mdelay(2);

    /*dischg soc1 and socf*/
    ret = bq27510_access_data_flash(di,BQ27510_SUBCLASS_ID_DISCHG,0,data_flash,4,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    soc1 = data_flash[0];
    socf = data_flash[2];
    mdelay(2);

    ret = bq27510_access_data_flash(di,BQ27510_SUBCLASS_ID_REG,0,data_flash,2,0);
    if(ret < 0){
        error = -1;
        goto exit;
    }
    val = (data_flash[0]<< 8)|data_flash[1];

exit:
    if(error){
        return sprintf(buf,"Fail to read\n");
    }else {
        sprintf(buf,"volt = %3d mV, iterm = %3d mA, descap = %3d mAh, soc1 = %3d mAh, socf = %3d mAh, cfg = 0x%3x\n",
                chgvolt,iterm,descap,soc1,socf,val);
        return strlen(buf);
    }
}

static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, bq27510_fg_show, bq27510_fg_store);
static DEVICE_ATTR(gaugelog, S_IWUSR | S_IRUGO, bq27510_show_gaugelog, NULL);
static DEVICE_ATTR(firmware_version, S_IWUSR | S_IRUGO, bq27510_show_firmware_version, NULL);
static DEVICE_ATTR(voltage, S_IWUSR | S_IRUGO, bq27510_show_voltage, NULL);
static DEVICE_ATTR(data_flash, S_IWUSR | S_IRUGO, bq27510_show_data_flash, NULL);

static struct attribute *bq27510_attributes[] = {
    &dev_attr_state.attr,
    &dev_attr_gaugelog.attr,
    &dev_attr_firmware_version.attr,
    &dev_attr_voltage.attr,
    &dev_attr_data_flash.attr,
    NULL,
};
static const struct attribute_group bq27510_attr_group = {
    .attrs = bq27510_attributes,
};



static int bq27510_battery_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct bq27510_device_info *di;
    struct comip_fuelgauge_info *pdata = client->dev.platform_data;
    int num;
    char *name;
    int retval = 0, ret =0;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev,"can't talk I2C?\n");
        return -ENODEV;
    }

    if(!pdata){
        dev_dbg(&client->dev, " No bq27510 platform data supplied\n");
    }else{
        gauge_context.normal_capacity = pdata->normal_capacity;
        gauge_context.firmware_version = pdata->firmware_version;
    }

    /* Get new ID for the new battery device */
    mutex_lock(&bq27510_battery_mutex);
    num = idr_alloc(&bq27510_battery_id, client, 0, 0, GFP_KERNEL);
    mutex_unlock(&bq27510_battery_mutex);
    if (num < 0) {
        dev_err(&client->dev,"bq27510 idr_alloc failed!!\n");
        retval = num;
        goto batt_failed_0;
    }

    name = kasprintf(GFP_KERNEL, "bq27510-%d", num);
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
    di->dev = &client->dev;
    di->client = client;
    di->rom_client = NULL;
    bq27510_device = di;
    bq27510_i2c_client = client;

    ret = bq27510_get_firmware_version(di);
    if(ret < 0){
        dev_err(&client->dev,"could not read firmware version!\n");
    }else{
        dev_info(&client->dev,"bq27510 read firmware version = 0x%6x\n",ret);
    }

    wake_lock_init(&low_battery_wakelock, WAKE_LOCK_SUSPEND, "gasgauge_wake_lock");
    wake_lock_init(&bqfs_wakelock, WAKE_LOCK_SUSPEND, "bqfs_wake_lock");
    retval = bq27510_init_irq(di);
    if(retval){
        dev_err(&client->dev, "irq init failed\n");
        goto batt_failed_3;
    }
    retval = driver_create_file(&(bq27510_battery_driver.driver), &driver_attr_state);
    if (0 != retval) {
        BQ27510_ERR("failed to create sysfs entry(state): %d\n", ret);
        retval =  -ENOENT;
        goto batt_failed_4;
    }
    retval = sysfs_create_group(&client->dev.kobj, &bq27510_attr_group);
    if (retval < 0){
        dev_err(&client->dev, "could not create sysfs files\n");
        retval = -ENOENT;
        goto batt_failed_5;
    }
     
    comip_battery_property_register(&bq27510_battery_property);
    dev_info(&client->dev, "bq27510 Probe Success\n");

    return 0;

batt_failed_5:
    driver_remove_file(&(bq27510_battery_driver.driver), &driver_attr_state);
batt_failed_4:
    free_irq(gpio_to_irq(di->client->irq), di);
batt_failed_3:
    wake_lock_destroy(&bqfs_wakelock);
    wake_lock_destroy(&low_battery_wakelock);
batt_failed_2:
    kfree(name);
    name = NULL;
batt_failed_1:
    mutex_lock(&bq27510_battery_mutex);
    idr_remove(&bq27510_battery_id,num);

    mutex_unlock(&bq27510_battery_mutex);
batt_failed_0:
    return retval;
}

static int bq27510_battery_remove(struct i2c_client *client)
{
    struct bq27510_device_info *di = i2c_get_clientdata(client);

    sysfs_remove_group(&client->dev.kobj, &bq27510_attr_group);
    driver_remove_file(&(bq27510_battery_driver.driver), &driver_attr_state);

    bq27510_i2c_client = NULL;

    wake_lock_destroy(&bqfs_wakelock);
    wake_lock_destroy(&low_battery_wakelock);

    free_irq(gpio_to_irq(di->client->irq), di);

    mutex_lock(&bq27510_battery_mutex);
    idr_remove(&bq27510_battery_id, di->id);
    mutex_unlock(&bq27510_battery_mutex);

    kfree(di);

    return 0;
}

static const struct i2c_device_id bq27510_id[] = {
    { "bq27510-battery", 0 },
    {},
};

#ifdef CONFIG_PM
static int bq27510_battery_suspend(struct device *dev)
{
    return 0;
}

static int bq27510_battery_resume(struct device *dev)
{
    return 0;
}
#else
#define bq27510_battery_suspend NULL
#define bq27510_battery_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops pm_ops = {
    .suspend    = bq27510_battery_suspend,
    .resume     = bq27510_battery_resume,
};

static struct i2c_driver bq27510_battery_driver = {
    .probe      = bq27510_battery_probe,
    .remove     = bq27510_battery_remove,
    .id_table   = bq27510_id,
    .driver     = {
        .name   = "bq27510-battery",
        .pm = &pm_ops,
    },
};

static int __init bq27510_battery_init(void)
{
    return i2c_add_driver(&bq27510_battery_driver);
}
fs_initcall_sync(bq27510_battery_init);

static void __exit bq27510_battery_exit(void)
{
    i2c_del_driver(&bq27510_battery_driver);
}
module_exit(bq27510_battery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments Inc");
