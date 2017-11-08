/*
 * drivers/power/max17058_battery.c
 *
 * max17058/9 fuel gauge driver
 *
 * Copyright (C) 2014-2015 maxim/leadcore.
 * Author: leadcore tech.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <plat/max17058_battery.h>
#include <plat/comip-battery.h>


struct max17058_device_info {
    struct device   *dev;
    int             id;
    struct i2c_client   *client;
    struct comip_fuelgauge_info *pdata;
    struct delayed_work notifier_work;
    struct delayed_work hand_work;
    struct delayed_work bat_id_work;
    int vcell;
    int soc;
    int bat_temp;
    u16 rcomp;
    u8 alertsoc;
    u8 alert_status;
    u16 config_status;
    u16 org_ocv;
    u16 test_ocv;
    int init_rcomp;
    int tempcoup;
    int tempcodown;
    u8 init_socchecka;
    u8 init_soccheckb;
    int data_baseaddr;
    const char* bat_name;
};

//#define MAX17058_REG_DUMP

#define VERIFY_AND_FIX   (1)
#define MAX17058_LOAD_MODEL      !(VERIFY_AND_FIX)

#define MAX17058_ALERT_SOC(x)            (32-x) /*x=1% ~ 32%*/
#define MAX17058_UPDATE_MODEL_TIMEOUT    (HZ *100)
#define MAX17058_BAT_ID_TIMEOUT          (100) /*ms,if use bq2022a set*/


#define MAX17058_RECHG_SOC_CORRECTION_COEFF  (105)/*if soc > 95,need report soc to 100 when recharging*/



/* If the system has several batteries we need
 * a different name for each of them*/
static DEFINE_IDR(max17058_battery_id);

static DEFINE_MUTEX(max17058_battery_mutex);

static struct wake_lock low_battery_wakelock;
static struct wake_lock fw_wakelock;


static struct i2c_driver max17058_battery_driver;

struct max17058_device_info *max17058_device;
struct i2c_client *max17058_i2c_client;

/*guangyu battery para for default para*/
#define INI_RCOMP                       (91)
#define INI_SOCCHECKA                   (217)
#define INI_SOCCHECKB                   (219)
#define INI_OCVTEST                     (57424)
#define INI_RCOMP_FACTOR                (1000)
#define TempCoHot                        (-0.8*INI_RCOMP_FACTOR)
#define TempCoCold                      (-5.025*INI_RCOMP_FACTOR)

#define INI_BITS                        (19)



enum{
    FG_NORMAL_MODE,
    FG_UPDATE_FIRMWARE,
    FG_LOCK_MODE
};

struct fg_context
{
    unsigned char state;
};

static struct fg_context gauge_context =
{
    .state = FG_NORMAL_MODE,
};

static int max17058_is_accessible(void)
{
    if(gauge_context.state == FG_UPDATE_FIRMWARE){
        MAX17058_DBG("max17058 isn't accessible,It's updating firmware!\n");
        return 0;
    } else if(gauge_context.state == FG_NORMAL_MODE){
        return 1;
    }else {
        gauge_context.state = FG_NORMAL_MODE;
        return 1;
    }
}

#if 0
static int max17058_i2c_read_byte_data(struct max17058_device_info *di, u8 reg)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&max17058_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_read_byte_data(di->client, reg);
        if (ret < 0) {
            //MAX17058_ERR("[%s,%d] i2c_smbus_read_byte_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&max17058_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c read at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}


static int max17058_i2c_write_byte_data(struct max17058_device_info *di, u8 reg, u8 value)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&max17058_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_write_byte_data(di->client, reg, value);
        if (ret < 0)  {
            //MAX17058_ERR("[%s,%d] i2c_smbus_write_word_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&max17058_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c write at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}
#endif

static int max17058_i2c_read_word_data(struct max17058_device_info *di, u8 reg)
{
    int ret = 0;

    int i = 0;

    mutex_lock(&max17058_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_read_word_data(di->client, reg);
        if (ret < 0) {
            //MAX17058_ERR("[%s,%d] i2c_smbus_read_byte_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&max17058_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c read at address 0x%02x failed\n",__func__, reg);
    }else{
        ret = (((u16)(ret & (0x00FF)) << 8) + (u16)(((ret & (0xFF00)) >> 8)));
    }

    return ret;
}

static int max17058_i2c_write_word_data(struct max17058_device_info *di, u8 reg, u16 value)
{
    int ret = 0;
    int i = 0;

    mutex_lock(&max17058_battery_mutex);
    for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
        ret = i2c_smbus_write_word_data(di->client, reg, swab16(value));
        if (ret < 0)  {
            //MAX17058_ERR("[%s,%d] i2c_smbus_write_word_data failed\n", __FUNCTION__, __LINE__);
        } else {
            break;
        }
        msleep(SLEEP_TIMEOUT_MS);
    }
    mutex_unlock(&max17058_battery_mutex);

    if(ret < 0){
        dev_err(&di->client->dev,
        "%s: i2c write at address 0x%02x failed\n",__func__, reg);
    }

    return ret;
}
static int bat_id_flag = 0;
static int max17058_get_battery_id_type(struct max17058_device_info *di)
{
    int status = 0;

    status = comip_battery_id_type();

    di->bat_name = comip_battery_name(status);

    switch(status){
    case BAT_ID_2:
        di->test_ocv = 57424;/*GY2000*/
        di->init_rcomp = 91;
        di->tempcoup = (-800);
        di->tempcodown = (-5025);
        di->init_socchecka = 217;
        di->init_soccheckb = 219;
        di->data_baseaddr = 64; /*check whether board_xxxx.c defineed*/
        bat_id_flag = 1;
        break;
    case BAT_ID_1:
        di->test_ocv = 57472;/*XWD2000*/
        di->init_rcomp = 104;
        di->tempcoup = (-1800);
        di->tempcodown = (-5675);
        di->init_socchecka = 231;
        di->init_soccheckb = 233;
        di->data_baseaddr = 128; /*check whether board_xxxx.c defineed*/
         bat_id_flag = 1;
        break;
    case BAT_ID_5:
        di->test_ocv = 57680;/*DS2000*/
        di->init_rcomp = 95;
        di->tempcoup = (-1500);
        di->tempcodown = (-4900);
        di->init_socchecka = 232;
        di->init_soccheckb = 234;
        di->data_baseaddr = 192;/*check whether board_xxxx.c defineed*/
         bat_id_flag = 1;
        break;
    case BAT_ID_7:
        di->test_ocv = 57680;/*RS use DeSai bat model data*/
        di->init_rcomp = 95;
        di->tempcoup = (-1500);
        di->tempcodown = (-4900);
        di->init_socchecka = 232;
        di->init_soccheckb = 234;
        di->data_baseaddr = 192;/*check whether board_xxxx.c defineed*/
         bat_id_flag = 1;
        break;
    case BAT_ID_NONE:
    default:
        di->test_ocv = 57424;
        di->init_rcomp = INI_RCOMP;
        di->tempcoup = (TempCoHot);
        di->tempcodown = (TempCoCold);
        di->init_socchecka = 217;
        di->init_soccheckb = 219;
        di->data_baseaddr = 0;
        bat_id_flag = 0;
        break;
    }
    if(di->pdata->fgtblsize <= di->data_baseaddr){
        di->test_ocv = 57424;
        di->init_rcomp = INI_RCOMP;
        di->tempcoup = (TempCoHot);
        di->tempcodown = (TempCoCold);
        di->init_socchecka = 217;
        di->init_soccheckb = 219;
        di->data_baseaddr = 0;
    }

    dev_info(di->dev, "bat_name = %s \n" "init_soccheckA = %d\n"
            "init_soccheckB = %d\n" "data_baseaddr = %d \n",
            di->bat_name,di->init_socchecka,di->init_soccheckb,di->data_baseaddr);

    return 0;
}



/*===========================================================================
  Function:       max17058_write_mode
  Description:    set quick start or ensleep
  Input:          subcmd
  Output:         none
  Return:         value of
  Others:         none
===========================================================================*/
static int max17058_write_mode(u16 subcmd)
{
    struct max17058_device_info *di = max17058_device;
    int data = 0;

    data = max17058_i2c_write_word_data(di,MAX17058_REG_MODE,subcmd);
    if(data < 0){
        dev_err(&di->client->dev,
            "Failed write sub command:0x%x. data=%d\n", subcmd, data);
    }
    dev_dbg(&di->client->dev, "%s() subcmd=0x%x data=%d\n",__func__, subcmd, data);

    return ((data < 0)? data:0);
}



int max17058_compensation_rcomp(int temp)
{
    struct max17058_device_info *di = max17058_device;
    u8 rcomp = 0;
    int data = 0;

    di->bat_temp = temp;
    data = max17058_i2c_read_word_data(di, MAX17058_REG_RCOMP_MSB);
    if(data < 0){
        dev_err(di->dev,"failed read rcomp =%d\n", data);
    }
    if((data & MAX17058_ALRT) == MAX17058_ALRT){
        dev_info(di->dev,"soc alert =%x\n", data);
    }

    if(temp > CONST_TEMP_20){
        rcomp = (u8)(di->init_rcomp + ((CONST_TEMP_20 - temp) * di->tempcoup)/INI_RCOMP_FACTOR);
    }else{
        rcomp = (u8)(di->init_rcomp + ((temp - CONST_TEMP_20) * di->tempcodown)/INI_RCOMP_FACTOR);
    }
    if(rcomp > 255){
        rcomp = 255;
    }
    if(rcomp < 0){
        rcomp = 0;
    }
    di->rcomp = (rcomp << 8);
    di->config_status = di->rcomp|di->alertsoc;
    data = max17058_i2c_write_word_data(di, MAX17058_REG_RCOMP_MSB, di->config_status);
    if(data < 0){
        dev_err(di->dev,"failed write rcomp =%d\n", data);
    }

    return data;
}


static int max17058_config_vreset_voltage(int mvolt)
{
    struct max17058_device_info *di = max17058_device;
    int data = 0;
    u16 cmd = 0;

    if(mvolt > 3480)
        mvolt = 3480;
    if(mvolt < 2280)
        mvolt = 2280;

    cmd = (mvolt - 0)/40;
    cmd = ((cmd << 1) << 8);

    data = max17058_i2c_write_word_data(di, MAX17058_REG_VRESET_MSB, cmd);
    if(data < 0){
        dev_err(di->dev,"failed write vreset =%d\n", data);
    }

    return data;
}

/*===========================================================================
  Function:       max17058_get_version
  Description:    get version
  Input:          no
  Output:         none
  Return:         value of version
  Others:         none
===========================================================================*/
static int max17058_get_version(void)
{
    struct max17058_device_info *di = max17058_device;
    int data = 0;

    data = max17058_i2c_read_word_data(di, MAX17058_REG_VERSION_MSB);
    if(data < 0){
        dev_err(di->dev,"failed get version =%d\n", data);
    }

    return data;
}

static int max17058_reset_cmd(struct max17058_device_info *di)
{
    return max17058_i2c_write_word_data(di, MAX17058_REG_CMD_MSB, 0x5400);
}

static int max17058_config_status_reg(struct max17058_device_info *di)
{
    int data = 0;

    data = max17058_i2c_read_word_data(di, MAX17058_REG_STATUS);
    data = data&0xFEFF;
    data = max17058_i2c_write_word_data(di, MAX17058_REG_STATUS, data);
    if(data < 0){
        dev_err(di->dev,"failed write status =%d\n", data);
    }

    return data;
}

static int max17058_check_por_status(struct max17058_device_info *di)
{
    int ret = 0;

    ret = max17058_i2c_read_word_data(di, MAX17058_REG_STATUS);
    if(ret < 0){
        dev_err(&di->client->dev, "read reg(0x1A)=%d failed [%d]\n",ret,__LINE__);
        return 0;
    }
    return (!!(ret & MAX17058_RI));
}

void max17058_unlock_model(struct max17058_device_info *di)
{
    int ret = 0;
    u16 check_times = 0,unlock_test_ocv = 0;

    /*C1,unlock model access, enable access to OCV and table registers*/
    ret = max17058_i2c_write_word_data(di, MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_UNLOCK);
    if(ret < 0){
        dev_err(&di->client->dev, "write reg(0x3E)=%d failed [%d]\n",ret,__LINE__);
        goto exit;
    }
    do
    {
        /*C2,Read OCV, verify Model Access Unlocked*/
         msleep(100);
        ret = max17058_i2c_read_word_data(di, MAX17058_REG_OCV_MSB);
        if(ret < 0){
            dev_err(&di->client->dev, "read OCV reg(0x0E)=%d failed [%d]\n",ret,__LINE__);
            goto exit;
        }
        unlock_test_ocv = ((ret)&0xFFFF);
        if(check_times++ >= 2) {//avoid of while(1)
            check_times = 0;
            pr_err("max17058-battery:time out3...");
            break;
        }
    }while(unlock_test_ocv==0xFFFF);/*verify Model Access Unlocked*/

exit:
    return;
}

static int max17058_get_init_ocv(struct max17058_device_info *di)
{
    int ret = 0;
    u16 check_times = 0;

    do
    {
        /*C1,unlock model access, enable access to OCV and table registers*/
        ret = max17058_i2c_write_word_data(di, MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_UNLOCK);
        if(ret < 0){
            dev_err(&di->client->dev, "write reg(0x3E)=%d failed [%d]\n",ret,__LINE__);
            goto exit;
        }
        /*C2,Read OCV, verify Model Access Unlocked*/
         msleep(100);
        ret = max17058_i2c_read_word_data(di, MAX17058_REG_OCV_MSB);
        if(ret < 0){
            dev_err(&di->client->dev, "read OCV reg(0x0E)=%d failed [%d]\n",ret,__LINE__);
            goto exit;
        }
        di->org_ocv = ((ret)&0xFFFF);
        if(check_times++ >= 2) {//avoid of while(1)
            check_times = 0;
            pr_err("max17058-battery:get ocv time out1...");
            break;
        }
    }while(di->org_ocv==0xFFFF);/*verify Model Access Unlocked*/

exit:
    return ret;
}

void max17058_prepare_to_load_model(struct max17058_device_info *di)
{
    int ret = 0;
    u16 check_times = 0,unlock_test_ocv = 0;
    int chip_version = max17058_get_version();

    /*C2.5.1,MAX17058/59 Only
     To ensure good RCOMP_Seg values in MAX17058/59, a POR signal has to be sent to
     MAX17058. The OCV must be saved before the POR signal is sent for best Fuel Gauge
     performance for chip version 0x0011 only.*/
    if(chip_version != 0x0011){
        goto exit;
    }
    check_times = 0;
    do
    {
        ret = max17058_reset_cmd(di);
        if(ret < 0){
            dev_err(&di->client->dev, "write cmd reg(0xFE)=%d failed [%d]\n",ret,__LINE__);
            goto exit;
        }
        ret = max17058_i2c_write_word_data(di, MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_UNLOCK);
        if(ret < 0){
            dev_err(&di->client->dev, "write lock reg(0x3E) err=%d [%d]\n",ret,__LINE__);
            goto exit;
        }
        msleep(100);
        ret = max17058_i2c_read_word_data(di, MAX17058_REG_OCV_MSB);
        if(ret < 0){
            dev_err(&di->client->dev, "read OCV reg(0x0E)=%d failed [%d]\n",ret,__LINE__);
            goto exit;
        }
        unlock_test_ocv = ((ret)&0xFFFF);
        if(check_times++ >= 2) {//avoid of while(1)
            check_times = 0;
            pr_err("max17058-battery:get test ocv time out2...");
            break;
        }
    }while(unlock_test_ocv==0xFFFF);
    //C3, Write OCV
    //only for max17058/1/3/4,
    //do nothing for MAX17058

    //C4, Write RCOMP to its Maximum Value
    // only for max17058/1/3/4
    // max17058_i2c_write_word_data(di,0x0C, 0xFF00);
    //do nothing for MAX17058
exit:
    return;
}

void max17058_table_model_program(struct max17058_device_info *di)
{
    int k=0,ret = 0;
    u16 value = 0;

    if(!di->pdata->fgtblsize){
        goto exit;
    }
   /******************************
    C5, Write the Model
    Once the model is unlocked, the host software must write the 64 byte model
    to the device. The model is located between memory 0x40 and 0x7F.
    The model is available in the INI file provided with your performance
    report. See the end of this document for an explanation of the INI file.
    Note that the table registers are write-only and will always read
    0xFF. Step 9 will confirm the values were written correctly.
    ******************************/
    for(k=0;k<0x40;k+=2){
        value = (di->pdata->fg_model_tbl[di->data_baseaddr + k]<<8)+(di->pdata->fg_model_tbl[di->data_baseaddr + (k+1)]);
        max17058_i2c_write_word_data(di, 0x40+k, value);
        if(ret < 0){
            dev_err(&di->client->dev, "write reg table(0x40~0x80) failed[%d]\n",__LINE__);
            goto exit;
        }
    }
#if 0
    for(k=0;k<0x10;k++){
        ret = max17058_i2c_write_word_data(di,0x80, 0x0080);
        if(ret < 0){
            dev_err(&di->client->dev, "write reg(0x80) failed[%d]\n",__LINE__);
            goto exit;
        }
    }
#endif
exit:
    return;
}

bool is_max17058_verify_model_correct(struct max17058_device_info *di)
{
    u8 SOC_1 = 0, SOC_2 = 0;
    u16 msb = 0;
    int ret = 0;

    msleep(200);/*C6,Delay at least 150ms(max17058/1/3/4 only)*/
    /*C7, Write OCV:write(reg[0x0E], INI_OCVTest_High_Byte, INI_OCVTest_Low_Byte)*/
    ret = max17058_i2c_write_word_data(di,MAX17058_REG_OCV_MSB, di->test_ocv);
    if(ret < 0){
        dev_err(&di->client->dev, "write OCV reg(0x0E) err =%d [%d]\n",ret,__LINE__);
        return false;
    }
    /* C7.0,Disable Hibernate (MAX17048/49 only)*/
    //max17058_i2c_write_word_data(di,0x0A,0x0);

    /*C7.1,Lock Model Access (MAX17048/49/58/59 only)*/
    ret = max17058_i2c_write_word_data(di, MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_LOCK);
    if(ret < 0){
        dev_err(&di->client->dev, "write lock reg(0x0E) err =%d [%d]\n",ret,__LINE__);
        return false;
    }
    /*C8, Delay between 150ms and 600ms, delaying beyond 600ms
    could cause the verification to fail*/
    msleep(500);

    /*C9, Read SOC register and compare to expected result*/
    msb = max17058_i2c_read_word_data(di, MAX17058_REG_SOC_MSB);
    SOC_1 = ((msb)&(0xFF00))>>8;//"big endian":low byte save MSB
    SOC_2 = (msb)&(0x00FF);

    if(SOC_1 >= di->init_socchecka && SOC_1 <= di->init_soccheckb) {
         dev_info(&di->client->dev,"Download success [SOC_1 = %d] \n",SOC_1);
        return true;
    }
    else {
        dev_err(&di->client->dev,"Download was NOT successfully![SOC_1 = %d]\n",SOC_1);
        return false;
    }
}



void max17058_cleanup_model_load(struct max17058_device_info *di)
{
    u16 original_ocv=0;
    int ret = 0;

    original_ocv = (di->org_ocv);
    /*C9.1, Unlock Model Access (MAX17048/49/58/59 only): To write OCV, requires model access to be unlocked*/
    ret = max17058_i2c_write_word_data(di,MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_UNLOCK);
    if(ret < 0){
        dev_err(&di->client->dev, "write reg(0x3E) err =%d [%d]\n",ret,__LINE__);
        goto exit;
    }
    /*C10, Restore CONFIG and OCV: write(reg[0x0C], INI_RCOMP, Your_Desired_Alert_Configuration)*/
    ret = max17058_i2c_write_word_data(di,MAX17058_REG_RCOMP_MSB, di->config_status);
    if(ret < 0){
        dev_err(&di->client->dev, "write rcomp reg(0x0C) err=%d [%d]\n",ret,__LINE__);
        goto exit;
    }
    ret = max17058_i2c_write_word_data(di,MAX17058_REG_OCV_MSB, original_ocv);
    if(ret < 0){
        dev_err(&di->client->dev, "write OCV reg(0x0C) err=%d [%d]\n",ret,__LINE__);
        goto exit;
    }
exit:
    /*C11, Lock Model Access*/
    ret = max17058_i2c_write_word_data(di,MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_LOCK);
    /*C12,delay at least 150ms before reading SOC register*/
    mdelay(200);

    return;
}


int max17058_firmware_downdload(int load_flag)
{
    struct max17058_device_info *di = max17058_device;
    bool model_load_ok = false;
    u16 check_times = 0;

    if(!di->pdata->fgtblsize){
        dev_err(&di->client->dev, "no fuel gauge model data to update [%d]\n",__LINE__);
        return 0;
    }

    dev_info(&di->client->dev, "firmware is downloading ...\n");
    gauge_context.state = FG_UPDATE_FIRMWARE;
    max17058_get_init_ocv(di);
    do {
        if(load_flag == MAX17058_LOAD_MODEL) {
            max17058_prepare_to_load_model(di);
            max17058_table_model_program(di);
        }
        model_load_ok = is_max17058_verify_model_correct(di);
        if(!model_load_ok) {
            load_flag = MAX17058_LOAD_MODEL;
            max17058_unlock_model(di);
        }
        if(check_times++ >= 2) {//avoid of while(1)
            check_times = 0;
            pr_err("max17058-battery:time out4...");
            break;
        }
    } while (!model_load_ok);

    max17058_cleanup_model_load(di);
    gauge_context.state = FG_NORMAL_MODE;
    max17058_config_status_reg(di);
    max17058_config_vreset_voltage(VRESET_VOLT);

    return 0;
}

int max17058_handle_model(int load_flag)
{
    struct max17058_device_info *di = max17058_device;
    int ret = 0;

    ret = max17058_check_por_status(di);
    if(!ret){
        dev_dbg(&di->client->dev, "POR does not happen,don't need to load model data");
        return 0;
    }

    max17058_firmware_downdload(load_flag);

    return ret;
}

/*
 * Return the ocv in milivolts
 * Or < 0 if something fails.
 */
static int max17058_battery_ocv(void)
{
    struct max17058_device_info *di = max17058_device;
    int data = 0,ret = 0;

    if(!max17058_is_accessible()){
        return data ;
    }

    /*unlock model access, enable access to OCV and table registers*/
    ret = max17058_i2c_write_word_data(di, MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_UNLOCK);
    if(ret)
        return ret;
    data = max17058_i2c_read_word_data(di, MAX17058_REG_OCV_MSB);
    if((data < 0)||(data > 65535)) {
        dev_err(di->dev, "error %d reading ocv \n", data);
    }else{
        //dev_info(di->dev, "ocv =0x%x \n\n", data);
        data = (data*625)/8;/*uVolt*/
        data = data/CONST_VCELL_1000;/*mVolt*/
    }
    ret = max17058_i2c_write_word_data(di, MAX17058_REG_LOCK_MSB, max17058_MODEL_ACCESS_LOCK);

    return data ;
}
/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
int max17058_battery_voltage(void)
{
    struct max17058_device_info *di = max17058_device;
    int data = 0;

    if(!max17058_is_accessible())
        return di->vcell;

    data = max17058_i2c_read_word_data(di, MAX17058_REG_VCELL);
    if((data < 0)||(data > 65535)) {
        dev_err(di->dev, "error %d reading voltage\n", data);
        data = di->vcell;
    }else{
        data = (data*625)/8;/*uVolt*/
        data = data/CONST_VCELL_1000;/*mVolt*/
        di->vcell = data;
    }

    MAX17058_DBG("read voltage result = %d mVolts\n", data);
    return data ;
}


/*
 * Return the battery Relative State-of-Charge
 * The reture value is 0 - 100%
 * Or < 0 if something fails.
 */
int max17058_battery_capacity(void)
{
    struct max17058_device_info *di = max17058_device;
    int data = 0;

    if(!max17058_is_accessible())
        return di->soc;

    data = max17058_i2c_read_word_data(di,MAX17058_REG_SOC_MSB);
    if(data < 0) {
        MAX17058_ERR("error %d reading capacity\n", data);
        data = di->soc;
    }else{
        if(INI_BITS == 19){
            data = data/512;
        }else{
            data = data/256;
        }
        if(data > 100){
            data = 100;
        }
        di->soc = data;
    }
    MAX17058_DBG("read soc result = %d Hundred Percents\n", data);

    return data;
}

static int max17058_battery_realsoc(void)
{
    struct max17058_device_info *di = max17058_device;

    return di->soc;
}
/*correction recharging soc display*/
int max17058_battery_soc(void)
{
    struct max17058_device_info *di = max17058_device;
    int data = 0;

    if(!max17058_is_accessible())
        return di->soc;

    data = max17058_battery_capacity();
    data = (data * MAX17058_RECHG_SOC_CORRECTION_COEFF)/100;
    if(data > 100){
        data = 100;
    }
    di->soc = data;
    MAX17058_DBG("read soc result = %d Hundred Percents\n", data);

    return data;
}

short max17058_battery_current(void)
{
    int data = 0;

    return ((signed short)data);
}

int is_max17058_battery_exist(void)
{

    struct max17058_device_info *di = max17058_device;

    int adc_code = 0,i = 0, adc[2]={0};
    int sum = 0;

    for(i=0;i< 2;i++){
        adc[i] = pmic_get_adc_conversion(1);
        sum = sum+adc[i];
    }
    adc_code = sum/2;
    if(adc_code >= 2600){
        dev_dbg(di->dev, "battery is not present = %d\n", adc_code);
        return (0);
    }

    return (1);
}

static int max17058_get_battery_temperature(void)
{
    struct max17058_device_info *di = max17058_device;
    int temp_C = 20;
    int adc_code = 0,i = 0, adc[2]={0};
    int sum = 0;

    if(!di->pdata->tmptblsize){
        return temp_C;
    }
    for(i=0;i< 2;i++){
        adc[i] = pmic_get_adc_conversion(1);
        sum = sum+adc[i];
    }
    adc_code = sum/2;
    if((adc_code > 2500)||(adc_code < 100))
        return temp_C;

    for (temp_C = 0; temp_C < di->pdata->tmptblsize; temp_C++) {
        if (adc_code >= di->pdata->battery_tmp_tbl[temp_C])
            break;
    }

    /* first 20 values are for negative temperature */
    temp_C = (temp_C - 20); /* degree Celsius */
    if(temp_C > 67){
        MAX17058_ERR("battery temp over %d degree (adc=%d)\n", temp_C,adc_code);
        temp_C = 67;
    }
    
    return temp_C;
}

int max17058_battery_temperature(void)
{
    int data = max17058_get_battery_temperature();
    max17058_compensation_rcomp(data);
    return data;
}

int max17058_battery_health(void)
{
    return POWER_SUPPLY_HEALTH_GOOD;
}

/*===========================================================================
  Function:       max17058_battery_technology
  Description:    get the battery technology
  Input:          void
  Output:         none
  Return:         value of battery technology
  Others:         none
===========================================================================*/
int max17058_battery_technology(void)
{
    /*Default technology is "Li-poly"*/
    int data = POWER_SUPPLY_TECHNOLOGY_LIPO;

    return data;
}
struct comip_android_battery_info max17058_battery_property={
    .battery_voltage = max17058_battery_voltage,
    .battery_current = max17058_battery_current,
    .battery_capacity = max17058_battery_soc,
    .battery_temperature = max17058_battery_temperature,
    .battery_exist = is_max17058_battery_exist,
    .battery_health= max17058_battery_health,
    .battery_technology = max17058_battery_technology,
    .battery_ocv= max17058_battery_ocv,
    .battery_realsoc = max17058_battery_realsoc,
};

#ifdef MAX17058_REG_DUMP
static int max17058_reg_dump(void)
{
    struct max17058_device_info *di = max17058_device;
    int ocv, vcell,soc,mode,version,config,vreset,status,cmd;

    if(!max17058_is_accessible()){
        return 0;
    }
    ocv = max17058_battery_ocv();
    mdelay(1);
    vcell = max17058_battery_voltage();
    mdelay(1);
    soc = max17058_battery_capacity();
    mdelay(1);
    mode = max17058_i2c_read_word_data(di, MAX17058_REG_MODE);
    mdelay(1);
    version = max17058_get_version();
    mdelay(1);
    config = max17058_i2c_read_word_data(di, MAX17058_REG_CONFIG);
    mdelay(1);
    vreset = max17058_i2c_read_word_data(di, MAX17058_REG_VRESET);
    mdelay(1);
    status = max17058_i2c_read_word_data(di, MAX17058_REG_STATUS);
    mdelay(1);
    cmd = max17058_i2c_read_word_data(di, MAX17058_REG_CMD);

    dev_info(di->dev,"ocv = %-4d vcell = %-4d soc = %-2d  mode = 0x%-5.4x version = 0x%-5.4x \
config = 0x%-5.4x vreset = 0x%-5.4x status = 0x%-5.4x cmd = 0x%-5.4x\n",
                ocv, vcell, soc, mode ,version, config,vreset,status,cmd);

    return 0;
}
#endif

static int max17058_ocv_vcell_soc(void)
{
    struct max17058_device_info *di = max17058_device;
    int ocv, vcell,soc;

    if(!max17058_is_accessible()){
        return 0;
    }
    ocv = max17058_battery_ocv();
    vcell = max17058_battery_voltage();
    soc = max17058_battery_capacity();

    dev_info(di->dev,"ocv = %-4d vcell = %-4d soc = %-2d\n",ocv, vcell, soc);

    return 0;
}


static void max17058_init(struct max17058_device_info *di)
{
#ifndef MAX17058_REG_DUMP
    max17058_ocv_vcell_soc();
#else
    max17058_reg_dump();
#endif
    di->vcell = 3800;
    di->soc   = 50;
    di->alertsoc = MAX17058_ALERT_SOC(ALERT_SOC);
    di->init_rcomp = INI_RCOMP;
    di->tempcoup = (TempCoHot);
    di->tempcodown = (TempCoCold);
    di->bat_temp = max17058_battery_temperature();

    max17058_compensation_rcomp(di->bat_temp);/*set init temp*/
    return;
}

static void max17058_bat_id_work(struct work_struct *work)
{
    struct max17058_device_info *di = container_of(work,
             struct max17058_device_info, bat_id_work.work);

#ifndef MAX17058_REG_DUMP
        max17058_get_battery_id_type(di);
        max17058_compensation_rcomp(di->bat_temp);
#else
    if(bat_id_flag != 2){
        max17058_get_battery_id_type(di);
        max17058_compensation_rcomp(di->bat_temp);
        bat_id_flag = 2;
    }
    max17058_reg_dump();
    schedule_delayed_work(&di->bat_id_work,msecs_to_jiffies(2000));
#endif

    return;
}

static void max17058_handle_work(struct work_struct *work)
{
    struct max17058_device_info *di = container_of(work,
             struct max17058_device_info, hand_work.work);
    int ret = 0;

    wake_lock(&fw_wakelock);
    ret = max17058_handle_model(MAX17058_LOAD_MODEL);
    if(ret){
        max17058_ocv_vcell_soc();
    }

    schedule_delayed_work(&di->hand_work,MAX17058_UPDATE_MODEL_TIMEOUT);
    wake_unlock(&fw_wakelock);
    return;
}

/*
 * Use ALRT. When battery capacity is below alert_soc_threshold,
 * ALRT PIN will pull up and cause a
 * interrput, this is the interrput callback.
 */
static irqreturn_t max17058_interrupt(int irq, void *_di)
{
    struct max17058_device_info *di = _di;

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
    struct max17058_device_info *di = container_of(work,
        struct max17058_device_info, notifier_work.work);
    int data = 0;
    int soc = 0;

    //wake_lock(&low_battery_wakelock);
    wake_lock_timeout(&low_battery_wakelock, 10 * HZ);
    soc = max17058_battery_capacity();
    dev_info(di->dev, "low battery intr = %d\n",soc);
    data = max17058_i2c_read_word_data(di,MAX17058_REG_RCOMP_MSB);
    if(data < 0){
        dev_err(di->dev,"failed read alert status bit =%d\n", data);
        goto exit;
    }
    if((data & MAX17058_ALRT) == MAX17058_ALRT){
        di->alert_status = 0;
        di->config_status = (data & (!MAX17058_ALRT));
        max17058_i2c_write_word_data(di, MAX17058_REG_RCOMP_MSB, di->config_status);
    }
exit:
    if(wake_lock_active(&low_battery_wakelock)) {
        //wake_lock_timeout(&low_battery_wakelock, 10 * HZ);
         wake_unlock(&low_battery_wakelock);
    }
    return;
}

static int max17058_init_irq(struct max17058_device_info *di)
{
    int ret = 0;

    if (gpio_request(di->client->irq, "max17058_irq") < 0){
        ret = -ENOMEM;
        goto irq_fail;
    }

    gpio_direction_input(di->client->irq);

    /* request irq interruption */
    ret = request_irq(gpio_to_irq(di->client->irq), max17058_interrupt,
        IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,"max17058_irq",di);
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

static ssize_t max17058_show_voltage(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    int val = 0;
    //struct max17058_device_info *di = dev_get_drvdata(dev);

    if (NULL == buf) {
        MAX17058_ERR("max17058_show_voltage Error\n");
        return -1;
    }
    val = max17058_battery_voltage();

    //val *=1000;

    return sprintf(buf, "%d\n", val);
}

static ssize_t max17058_show_capacity(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    int val = 0;
    //struct max17058_device_info *di = dev_get_drvdata(dev);

    if (NULL == buf) {
        MAX17058_ERR("max17058_show_capacity Error\n");
        return -1;
    }
    val = max17058_battery_capacity();

    return sprintf(buf, "%d\n", val);
}

static ssize_t max17058_show_gaugelog(struct device *dev,
                  struct device_attribute *attr,
                  char *buf)
{
    struct max17058_device_info *di = dev_get_drvdata(dev);
    int ocv, vcell,soc,mode,version,config,vreset,status,cmd;

    if(!max17058_is_accessible()){
        return -1;
    }
    ocv = max17058_battery_ocv();
    mdelay(2);
    vcell = max17058_battery_voltage();
    mdelay(2);
    soc = max17058_battery_capacity();
    mdelay(2);
    mode = max17058_i2c_read_word_data(di, MAX17058_REG_MODE);
    mdelay(2);
    version = max17058_get_version();
    mdelay(2);
    config = max17058_i2c_read_word_data(di, MAX17058_REG_CONFIG);
    mdelay(2);
    vreset = max17058_i2c_read_word_data(di, MAX17058_REG_VRESET);
    mdelay(2);
    status = max17058_i2c_read_word_data(di, MAX17058_REG_STATUS);
    mdelay(2);
    cmd = max17058_i2c_read_word_data(di, MAX17058_REG_CMD);

    sprintf(buf, "%-6d  %-6d  %-4d  0x%-5.4x  0x%-5.4x  0x%-5.4x  0x%-5.4x  0x%-5.4x  0x%-5.4x\n",
                ocv, vcell, soc, mode ,version, config,vreset,status,cmd);

        return strlen(buf);
}

static ssize_t max17058_set_rcomp(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    //struct max17058_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < -40) || (val > 100))
        return -EINVAL;

    max17058_compensation_rcomp(val);
    return count;
}

static ssize_t max17058_set_alertsoc(struct device *dev,
                  struct device_attribute *attr,
                  const char *buf, size_t count)
{
    long val;
    struct max17058_device_info *di = dev_get_drvdata(dev);

    if ((kstrtol(buf, 10, &val) < 0) || (val < 1) || (val > 32))
        return -EINVAL;

    di->alertsoc = MAX17058_ALERT_SOC(val);
    di->config_status = di->rcomp|di->alert_status|di->alertsoc;
    max17058_i2c_write_word_data(di, MAX17058_REG_RCOMP_MSB, di->config_status);
    return count;
}

static DEVICE_ATTR(gaugelog, S_IWUSR | S_IRUGO, max17058_show_gaugelog, NULL);
static DEVICE_ATTR(voltage,  S_IWUSR | S_IRUGO, max17058_show_voltage, NULL);
static DEVICE_ATTR(capacity, S_IWUSR | S_IRUGO, max17058_show_capacity, NULL);
static DEVICE_ATTR(rcomp,    S_IWUSR | S_IRUGO, NULL, max17058_set_rcomp);
static DEVICE_ATTR(alertsoc, S_IWUSR | S_IRUGO, NULL, max17058_set_alertsoc);

static struct attribute *max17058_attributes[] = {
    &dev_attr_gaugelog.attr,
    &dev_attr_voltage.attr,
    &dev_attr_capacity.attr,
    &dev_attr_rcomp.attr,
    &dev_attr_alertsoc.attr,
    NULL,
};
static const struct attribute_group max17058_attr_group = {
    .attrs = max17058_attributes,
};

static int max17058_battery_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    struct max17058_device_info *di;
    int num;
    char *name;
    int retval = 0,version = 0;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev,"can't talk I2C?\n");
        return -ENODEV;
    }

    /* Get new ID for the new battery device */
    mutex_lock(&max17058_battery_mutex);
    num = idr_alloc(&max17058_battery_id, client, 0, 0, GFP_KERNEL);
    mutex_unlock(&max17058_battery_mutex);
    if (num < 0) {
        dev_err(&client->dev,"max17058 idr_alloc failed!!\n");
        retval = num;
        goto batt_failed_0;
    }

    name = kasprintf(GFP_KERNEL, "max17058-%d", num);
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
    if(!di->pdata){
        dev_err(&client->dev, "No max17058 fg model platform data supplied\n");
        //retval = -EINVAL;
        //goto batt_failed_2;
    }

    di->dev = &client->dev;
    di->client = client;

    max17058_device = di;
    max17058_i2c_client = client;

    version = max17058_get_version();
    if(version < 0){
        dev_err(&client->dev, "Chip wersion fail 0x%x,version=0x%x\n",
                 client->addr,version);
        retval = -ENODEV;
        goto batt_failed_2;
    }else{
        dev_info(&client->dev, "max17058 Fuel-Gauge Ver 0x%04x\n", version);
    }

    max17058_init(di);
    wake_lock_init(&low_battery_wakelock, WAKE_LOCK_SUSPEND, "gasgauge_wake_lock");
    wake_lock_init(&fw_wakelock, WAKE_LOCK_SUSPEND, "firmware_wakelock");

    INIT_DELAYED_WORK(&di->bat_id_work, max17058_bat_id_work);
    INIT_DELAYED_WORK(&di->notifier_work,interrupt_notifier_work);
    INIT_DELAYED_WORK(&di->hand_work, max17058_handle_work);
    schedule_delayed_work(&di->bat_id_work,msecs_to_jiffies(MAX17058_BAT_ID_TIMEOUT));
    schedule_delayed_work(&di->hand_work,msecs_to_jiffies(MAX17058_BAT_ID_TIMEOUT+200));

    retval = max17058_init_irq(di);
    if(retval){
        dev_err(&client->dev, "irq init failed\n");
        goto batt_failed_3;
    }

    retval = sysfs_create_group(&client->dev.kobj, &max17058_attr_group);
    if (retval < 0){
        dev_err(&client->dev, "could not create sysfs files\n");
        retval = -ENOENT;
        goto batt_failed_4;
    }
    comip_battery_property_register(&max17058_battery_property);

    dev_info(&client->dev, "max17058 Probe Success\n");

    return 0;

batt_failed_4:
    free_irq(gpio_to_irq(di->client->irq), di);
batt_failed_3:
    wake_lock_destroy(&low_battery_wakelock);
batt_failed_2:
    kfree(name);
    name = NULL;
batt_failed_1:
    mutex_lock(&max17058_battery_mutex);
    idr_remove(&max17058_battery_id,num);
    mutex_unlock(&max17058_battery_mutex);
batt_failed_0:
    return retval;
}

static int max17058_battery_remove(struct i2c_client *client)
{
    struct max17058_device_info *di = i2c_get_clientdata(client);

    sysfs_remove_group(&client->dev.kobj, &max17058_attr_group);

    max17058_i2c_client = NULL;
    wake_lock_destroy(&low_battery_wakelock);

    free_irq(gpio_to_irq(di->client->irq), di);
    cancel_delayed_work_sync(&di->bat_id_work);
    cancel_delayed_work_sync(&di->hand_work);
    mutex_lock(&max17058_battery_mutex);
    idr_remove(&max17058_battery_id, di->id);
    mutex_unlock(&max17058_battery_mutex);

    kfree(di);

    return 0;
}

static const struct i2c_device_id max17058_id[] = {
    { "max17058-battery", 0 },
    {},
};

#ifdef CONFIG_PM
static int max17058_battery_suspend(struct device *dev)
{
#ifndef MAX17058_REG_DUMP
    struct platform_device *pdev = to_platform_device(dev);
    struct max17058_device_info *di = platform_get_drvdata(pdev);
#if 0
    max17058_write_mode(MAX17058_ENSLEEP);
    max17058_i2c_write_word_data(di, MAX17058_REG_RCOMP_MSB, (di->config_status|(1 << 7)));
#endif

    cancel_delayed_work_sync(&di->bat_id_work);
#endif
    return 0;
}

static int max17058_battery_resume(struct device *dev)
{
#if 0
    struct platform_device *pdev = to_platform_device(dev);
    struct max17058_device_info *di = platform_get_drvdata(pdev);

    max17058_i2c_write_word_data(di, MAX17058_REG_RCOMP_MSB, di->config_status);
#endif
    max17058_write_mode(0);
    return 0;
}
#else
#define max17058_battery_suspend NULL
#define max17058_battery_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops pm_ops = {
    .suspend    = max17058_battery_suspend,
    .resume     = max17058_battery_resume,
};

static struct i2c_driver max17058_battery_driver = {
    .probe      = max17058_battery_probe,
    .remove     = max17058_battery_remove,
    .id_table   = max17058_id,
    .driver     = {
        .name   = "max17058-battery",
        .pm = &pm_ops,
    },
};

static int __init max17058_battery_init(void)
{
    return i2c_add_driver(&max17058_battery_driver);
}
fs_initcall_sync(max17058_battery_init);

static void __exit max17058_battery_exit(void)
{
    i2c_del_driver(&max17058_battery_driver);
}
module_exit(max17058_battery_exit);

MODULE_DESCRIPTION("max17058 fuelgauge driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maxim");
