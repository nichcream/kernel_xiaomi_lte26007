/*
 * Gas_Gauge driver for CW2015/2013
 * Copyright (C) 2012, CellWise
 *
 * Authors: ChenGang <ben.chen@cellwise-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.And this driver depends on
 * I2C and uses IIC bus for communication with the host.
 *
 */
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/idr.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <plat/cw201x_battery.h>
#include <plat/comip-battery.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/jiffies.h>
#include <linux/alarmtimer.h>
#include <plat/bootinfo.h>
#include <plat/lc1160-pmic.h>
#if defined(CONFIG_DEVINFO_COMIP)
#include <plat/comip_devinfo.h>
#endif

#define BAT_CHANGE_ALGORITHM
#define FILE_PATH   "/amt/lastsoc"

#ifdef BAT_CHANGE_ALGORITHM
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
  #define CPSOC  95
#endif

#define REG_VERSION             0x0
#define REG_VCELL               0x2
#define REG_SOC                 0x4
#define REG_RRT_ALERT           0x6
#define REG_CONFIG              0x8
#define REG_MODE                0xA
#define REG_BATINFO             0x10

#define MODE_SLEEP_MASK         (0x3<<6)
#define MODE_SLEEP              (0x3<<6)
#define MODE_NORMAL             (0x0<<6)
#define MODE_QUICK_START        (0x3<<4)
#define MODE_RESTART            (0xf<<0)

#define CONFIG_UPDATE_FLG       (0x1<<1)
#define ATHD                    0x3        //ATHD = 3%

#define CW_I2C_SPEED            100000          // default i2c speed set 100khz
#define BATTERY_UP_MAX_CHANGE   720             // the max time allow battery change quantity
#define BATTERY_DOWN_CHANGE   60                // the max time allow battery change quantity
#define BATTERY_DOWN_MIN_CHANGE_RUN 30          // the min time allow battery change quantity when run
#define BATTERY_DOWN_MIN_CHANGE_SLEEP 1800      // the min time allow battery change quantity when run 30min

#define BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE 1800
#define DEVICE_RUN_TIME_FIX_VALUE 40

#define NO_STANDARD_AC_BIG_CHARGE_MODE 1

#define BAT_LOW_INTERRUPT    1

#define USB_CHARGER_MODE        1
#define AC_CHARGER_MODE         2
#define I2C_RETRY_TIME_MAX      2
#define SLEEP_TIMEOUT_MS        5

static DEFINE_IDR(cw_battery_id);

static DEFINE_MUTEX(cw_battery_mutex);
static DEFINE_MUTEX(data_mutex);

static struct i2c_client *cw2015_i2c_client; /* global i2c_client to support ioctl */

//#define FG_CW2015_DEBUG 1
#define FG_CW2015_TAG                  "[FG_CW2015]"
#ifdef FG_CW2015_DEBUG
#define FG_CW2015_FUN(f)               printk(KERN_DEBUG FG_CW2015_TAG"%s\n", __FUNCTION__)
#define FG_CW2015_ERR(fmt, args...)    printk(KERN_DEBUG FG_CW2015_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define FG_CW2015_LOG(fmt, args...)    printk(KERN_DEBUG FG_CW2015_TAG fmt, ##args)
#else
#define FG_CW2015_FUN(f)
#define FG_CW2015_ERR(fmt, args...)
#define FG_CW2015_LOG(fmt, args...)

#endif
#define CW2015_DEV_NAME     "cw201x"
static const struct i2c_device_id FG_CW2015_i2c_id[] = {
	{ CW2015_DEV_NAME, 0 },
	{},
};

int g_cw2015_capacity = 0;
int g_cw2015_vol = 0;
int FG_charging_type ;
int FG_charging_status ;
static int alg_firstsuc_flag = -1;
#if 0
static int update_cap_flag = -1;
static int g_old_cap = -1;
#endif
static int read_soc_err = -1;
static int count_adjust = 0;
static int alg_exc_count = 0;

#ifdef BAT_CHANGE_ALGORITHM
static int PowerResetFlag = -1;
static int alg_run_flag = -1;
struct cw_store {
	long bts;
	int OldSOC;
	int DetSOC;
	int AlRunFlag;
};
#endif
struct cw_battery {
	struct device   *dev;
	int             id;
	struct i2c_client *client;
	struct workqueue_struct *battery_workqueue;
	struct delayed_work battery_delay_work;
	struct delayed_work dumlog_work;
	struct delayed_work dc_wakeup_work;
	struct comip_fuelgauge_info *plat_data;
	struct comip_android_battery_info *battery_info;

	long sleep_time_capacity_change;      // the sleep time from capacity change to present, it will set 0 when capacity change
	long run_time_capacity_change;

	long sleep_time_charge_start;      // the sleep time from insert ac to present, it will set 0 when insert ac
	long run_time_charge_start;

	int dc_online;
	int usb_online;
	int charger_mode;
	int charger_init_mode;
	int capacity;
	int voltage;
	int status;
	int time_to_empty;
	int alt;

	int old_voltage;
	int old_capacity;
	int bat_change;
	int data_baseaddr;
	const char* bat_name;

	bool   filp_open;
};

struct cw_battery *pcw_battery;
#ifdef BAT_CHANGE_ALGORITHM
static unsigned int cw_convertData(struct cw_battery *cw_bat, unsigned int ts)
{
	unsigned int i = ts % 4096, n = ts / 4096;
	unsigned int ret = 65536;

	if(i >= 1700) {
		i -= 1700;
		ret = (ret * 3) / 4;
	}
	if(i >= 1700) {
		i -= 1700;
		ret = (ret * 3) / 4;
	}
	if(i >= 789) {
		i -= 789;
		ret = (ret * 7) / 8;
	}
	if(i >= 381) {
		i -= 381;
		ret = (ret * 15) / 16;
	}
	if(i >= 188) {
		i -= 188;
		ret = (ret * 31) / 32;
	}
	if(i >= 188) {
		i -= 188;
		ret = (ret * 31) / 32;
	}
	if(i >= 93) {
		i -= 93;
		ret = (ret * 61) / 64;
	}
	if(i >= 46) {
		i -= 46;
		ret = (ret * 127) / 128;
	}
	if(i >= 23) {
		i -= 23;
		ret = (ret * 255) / 256;
	}
	if(i >= 11) {
		i -= 11;
		ret = (ret * 511) / 512;
	}
	if(i >= 6) {
		i -= 6;
		ret = (ret * 1023) / 1024;
	}
	if(i >= 3) {
		i -= 3;
		ret = (ret * 2047) / 2048;
	}
	if(i >= 3) {
		i -= 3;
		ret = (ret * 2047) / 2048;
	}

	return ret >> n;
}

static int AlgNeed(struct cw_battery *cw_bat, int SOC_NEW, int SOC_OLD)
{
	printk("Chaman num = %d SOC_NEW = %d   SOC_OLD = %d \n", __LINE__ ,SOC_NEW, SOC_OLD);
	if(SOC_NEW - SOC_OLD > -20 && SOC_NEW - SOC_OLD < 20){
		return 2; // this is old battery
	}else{
		return 1; // this is new battery
	}
}
static int cw_algorithm(struct cw_battery *cw_bat, int real_capacity)
{
	//struct timespec ts;
	struct file *file = NULL;
	struct cw_store st;
	struct inode *inode;
	mm_segment_t old_fs;
	int vmSOC;
	unsigned int utemp, utemp1;
	unsigned long timeNow;
	static unsigned long timeNow_last  = -1;
	long timeChanged = 0;

       unsigned int power_on_type;
       static int count_fail = 0; 

	struct timespec ts;
	static int SOC_Dvalue = 0;     //add 0702 for vmSOC to real_capacity quickly when real_capacity very low
	static int Time_real3 = 0;
	static int Time_Dvalue = 0;
	static int Join_Fast_Close_SOC = 0;

	timeNow = get_seconds();
	

	vmSOC = real_capacity;
	if(file == NULL)
		file = filp_open(FILE_PATH, O_RDWR | O_CREAT, 0644);
	if(IS_ERR(file)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR(" error occured while opening file %s,exiting...\n", FILE_PATH);
#else
              printk(KERN_INFO " [CW2015]error occured while opening file %s,exiting...\n", FILE_PATH);
#endif
		return real_capacity;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	inode = file->f_dentry->d_inode;

	if((long)(inode->i_size) < (long)sizeof(st)) { //(inode->i_size)<sizeof(st)
#ifndef FG_CW2015_DEBUG
              printk(KERN_INFO "[CW2015]cw2015_file_test  file size error!\n");
#endif
              if(count_fail < 2)
              {
                  count_fail ++;
                  filp_close(file, NULL);
                  return real_capacity;
              }
		st.bts = timeNow;
		st.OldSOC = real_capacity;
		st.DetSOC = 0;
		st.AlRunFlag = -1;
		//alg_run_flag = -1;
		//file->f_pos = 0;
		//vfs_write(file,(char*)&st,sizeof(st),&file->f_pos);
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("cw2015_file_test  file size error!\n");
#endif
	} else {
		file->f_pos = 0;
		vfs_read(file, (char*)&st, sizeof(st), &file->f_pos);
	}

	get_monotonic_boottime(&ts);
       if(timeNow_last != -1 && ts.tv_sec > 30/*DEVICE_RUN_TIME_FIX_VALUE*/){
		timeChanged = timeNow - timeNow_last;
		if(timeChanged < -60 || timeChanged > 60){
		st.bts = st.bts + timeChanged;
              FG_CW2015_ERR(" 1 st.bts = \t%ld\n", st.bts);
		}
	}   
	timeNow_last = timeNow;

       power_on_type = lc1160_power_type_get();
#ifdef FG_CW2015_DEBUG
	 FG_CW2015_ERR("power_on_type = %d,PowerResetFlag = %d, st.AlRunFlag = %d\n", power_on_type,PowerResetFlag,st.AlRunFlag);
#endif 
#if 0
       if((power_on_type == PU_REASON_USB_CHARGER)&&(PowerResetFlag ==1)&&(real_capacity<=3))
       {        
           PowerResetFlag = -1;
           st.AlRunFlag = -1;
#ifdef FG_CW2015_DEBUG
           FG_CW2015_ERR("===PowerResetFlag = %d, st.AlRunFlag = %d\n", PowerResetFlag,st.AlRunFlag);
#endif
       }
#endif

	if(((st.bts) < 0) || (st.OldSOC > 100) || (st.OldSOC < 0) || (st.DetSOC < -1)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("cw2015_file_test  reading file error!\n");
		FG_CW2015_ERR("cw2015_file_test  st.bts = %ld st.OldSOC = %d st.DetSOC = %d st.AlRunFlag = %d  vmSOC = %d  2015SOC=%d\n", st.bts, st.OldSOC, st.DetSOC, st.AlRunFlag, vmSOC, real_capacity);
#else
              printk(KERN_INFO "[CW2015]cw2015_file_test  reading file error!st.bts = %ld st.OldSOC = %d st.DetSOC = %d st.AlRunFlag = %d  vmSOC = %d  2015SOC=%d\n", st.bts, st.OldSOC, st.DetSOC, st.AlRunFlag, vmSOC, real_capacity);
#endif
		st.bts = timeNow;
		st.OldSOC = real_capacity;
		st.DetSOC = 0;
		st.AlRunFlag = -1;
		//FG_CW2015_ERR("cw2015_file_test  reading file error!\n");
	}


	if(PowerResetFlag == 1) {
		PowerResetFlag = -1;
		if(real_capacity == 0){
			st.DetSOC = 1;
		}else if(real_capacity == 1){
			st.DetSOC = 3;
		}else if(real_capacity == 2){
			st.DetSOC = 7;
		}else if(real_capacity == 3){
			st.DetSOC = 13;
		}else if(real_capacity == 4){
			st.DetSOC = 15;
		}else if(real_capacity == 7){
			st.DetSOC = 17;
		}else if(real_capacity < 15){
			st.DetSOC = 20;
		}else if(real_capacity < 25){
			st.DetSOC = 23;
		}else if(real_capacity < 30){
			st.DetSOC = 20;
		}else if(real_capacity < 35){
			st.DetSOC = 17;
		}else if(real_capacity < 40){
			st.DetSOC = 13;
		}else if(real_capacity < 50){
			st.DetSOC = 10;
		}else if(real_capacity < 51){
			st.DetSOC = 8;
		}else if(real_capacity < 61){
			st.DetSOC = 6;
		}else if(real_capacity < 71){
			st.DetSOC = 5;
		}else if(real_capacity < 91){
			st.DetSOC = 4;
		}else if(real_capacity < 95){
			st.DetSOC = 3;
		}else if(real_capacity <= 100){
			vmSOC = 100;
			st.DetSOC = 100 - real_capacity;
		}
		if(AlgNeed(cw_bat, st.DetSOC + real_capacity, st.OldSOC) == 2){
			st.DetSOC = st.OldSOC - real_capacity + 1;
		}

        st.AlRunFlag = 1;
        st.bts = timeNow;
		vmSOC = real_capacity + st.DetSOC;
        FG_CW2015_ERR("cw2015_file_test  PowerResetFlag == 1!\n");
    }
    else if(Join_Fast_Close_SOC && st.AlRunFlag > 0){ //add 0702 for vmSOC to real_capacity quickly when real_capacity very low
              printk(KERN_INFO "[FG_CW2015]cw2015_file_test  algriothm of decrease st.OldSOC= %d,real_capacity = %d\n",st.OldSOC,real_capacity);
		if(timeNow >= Time_real3 + Time_Dvalue){
                      printk(KERN_INFO "[FG_CW2015]cw2015_file_test  algriothm of decrease --\n");
                      if(st.OldSOC>=1)
			vmSOC = st.OldSOC - 1;
			Time_real3 = timeNow;
		}
		else{
			vmSOC = st.OldSOC;
		}
		if (vmSOC == real_capacity)
		{ 
			st.AlRunFlag = -1;
			printk(KERN_INFO "[FG_CW2015]cw2015_file_test  algriothm end of decrease acceleration\n");
		}

	}
    else  if(((st.AlRunFlag) >0)&&((st.DetSOC) != 0))
    {
		get_monotonic_boottime(&ts);
		if(real_capacity < 1 && cw_bat->charger_mode == 0 && ts.tv_sec > DEVICE_RUN_TIME_FIX_VALUE){//add 0702 for vmSOC to real_capacity quickly when real_capacity very low
			if (SOC_Dvalue == 0){
				SOC_Dvalue = st.OldSOC - real_capacity;
                             if(SOC_Dvalue == 0)
                             {
                                 st.AlRunFlag = -1;
                                 printk(KERN_INFO "[FG_CW2015]cw2015_file_test  algriothm end of decrease acceleration[2]\n");
                             }
                             else
                             {
                                 printk(KERN_INFO "[FG_CW2015]cw2015_file_test  begin of decrease acceleration \n");
				Time_real3 = timeNow;
                                 if((cw_bat->voltage) < 3480){
				        Time_Dvalue = 20/(SOC_Dvalue);
                                 }
                                 else{
                                     Time_Dvalue = 60/(SOC_Dvalue);
                                 }
				Join_Fast_Close_SOC = 1;
				vmSOC = st.OldSOC;
                             }
			}			
		}
     else
      {
		utemp1 = 32768 / (st.DetSOC);
		if((st.bts) < timeNow)
			utemp = cw_convertData(cw_bat, (timeNow - st.bts));
		else
			utemp = cw_convertData(cw_bat, 1);
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("cw2015_file_test  convertdata = %d\n", utemp);
#endif
		if((st.DetSOC) < 0)
			vmSOC = real_capacity - (int)((((unsigned int)((st.DetSOC) * (-1)) * utemp) + utemp1) / 65536);
		else
			vmSOC = real_capacity + (int)((((unsigned int)(st.DetSOC) * utemp) + utemp1) / 65536);
		if (vmSOC == real_capacity) {
			st.AlRunFlag = -1;
#ifdef FG_CW2015_DEBUG
			FG_CW2015_ERR("cw2015_file_test  algriothm end\n");
#else
               printk(KERN_INFO "[FW_CW2015]cw2015_file_test  algriothm end\n");
#endif
		}
              if(count_adjust < 5)
                   count_adjust ++;
              
          }
	} else {
		st.AlRunFlag = -1;
		st.bts = timeNow;
		FG_CW2015_ERR("cw2015_file_test  no algriothm\n");
	}
#ifdef FG_CW2015_DEBUG
	FG_CW2015_ERR("cw2015_file_test debugdata,\t%ld,\t%d,\t%d,\t%d,\t%d,\t%d,\t%ld,\t%d,\t%d\n", timeNow, cw_bat->capacity, cw_bat->voltage, vmSOC, st.DetSOC, st.OldSOC, st.bts, st.AlRunFlag, real_capacity);
#else
	printk(KERN_INFO "cw2015_file_test debugdata,[timeNow]\t%ld,[cw_bat->capacity]\t%d,[cw_bat->voltage]\t%d,[vmSOC]\t%d,[st.DetSOC]\t%d,[st.OldSOC]\t%d,[st.bts]\t%ld,[st.AlRunFlag]\t%d,[real_capacity]\t%d\n", 
            timeNow, cw_bat->capacity, cw_bat->voltage, vmSOC, st.DetSOC, st.OldSOC, st.bts, st.AlRunFlag, real_capacity);
#endif
	alg_run_flag = st.AlRunFlag;
	if(vmSOC > 100)
		vmSOC = 100;
	else if(vmSOC < 0)
		vmSOC = 0;

      if(alg_firstsuc_flag == -1)
      {
           alg_firstsuc_flag = 1;
      }


      if(alg_exc_count < 20)
           alg_exc_count++;

	st.OldSOC = vmSOC;
	file->f_pos = 0;
	vfs_write(file, (char*)&st, sizeof(st), &file->f_pos);
	set_fs(old_fs);
	filp_close(file, NULL);
	file = NULL;

	return vmSOC;


}
#endif

static int cw_read(struct i2c_client *client, u8 reg, u8 buf[])
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		return ret;
	} else {
		buf[0] = ret;
		ret = 0;
	}

	return ret;
}

static int cw_write(struct i2c_client *client, u8 reg, u8 const buf[])
{
	int ret = 0;

	ret =  i2c_smbus_write_byte_data(client, reg, buf[0]);
	return ret;
}

static int cw_read_word(struct i2c_client *client, u8 reg, u8 buf[])
{
	int ret = 0;
	unsigned int data = 0;
#if 1
	data = i2c_smbus_read_word_data(client, reg);
	buf[0] = data & 0x00FF;
	buf[1] = (data & 0xFF00) >> 8;
#else
	ret = i2c_master_reg8_recv(client, reg, buf, 2, CW_I2C_SPEED);
#endif
	return ret;
}
static int cw_get_version(struct cw_battery *cw_bat)
{
	int ret = 0;
	u8 reg_val = 0;
	ret = cw_read(cw_bat->client, REG_VERSION, &reg_val);
	if(ret < 0) {
		dev_err(cw_bat->dev, "failed get version =%d\n", reg_val);
		return ret;
	}
	return reg_val;
}
static int bat_id_flag = 0;
static int cw_get_battery_id_type(struct cw_battery *cw_bat)
{
	int status = 0;

	status = comip_battery_id_type();

	cw_bat->bat_name = comip_battery_name(status);

	switch(status) {
		case BAT_ID_8:
			cw_bat->data_baseaddr = 256; /*check whether board_xxxx.c defineed*/
			bat_id_flag = 1;
			break;
		case BAT_ID_9:
			cw_bat->data_baseaddr = 320;/*check whether board_xxxx.c defineed*/
			bat_id_flag = 1;
			break;
		case BAT_ID_10:
			cw_bat->data_baseaddr = 384;/*check whether board_xxxx.c defineed*/
			bat_id_flag = 1;
			break;
		case BAT_ID_1:
		case BAT_ID_2:
		case BAT_ID_3:
		case BAT_ID_4:
		case BAT_ID_5:
		case BAT_ID_6:
		case BAT_ID_7:
		case BAT_ID_NONE:
		default:
			cw_bat->data_baseaddr = 256;
			bat_id_flag = 0;
			break;
	}

	if(cw_bat->plat_data->fgtblsize <= cw_bat->data_baseaddr) {
		cw_bat->data_baseaddr = 256;
	}

	dev_info(cw_bat->dev, "bat_name = %s,data_baseaddr = %d \n",
			cw_bat->bat_name, cw_bat->data_baseaddr);

	return 0;

}
static int cw_i2c_write_word_data(struct cw_battery *cw_bat, u8 reg, u16 value)
{
	int ret = 0;
	int i = 0;

	mutex_lock(&cw_battery_mutex);
	for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
		ret = i2c_smbus_write_word_data(cw_bat->client, reg, swab16(value));
		if (ret < 0)  {
			printk("[%s,%d] i2c_smbus_write_word_data failed\n", __FUNCTION__, __LINE__);
		} else {
			break;
		}
		msleep(SLEEP_TIMEOUT_MS);
	}
	mutex_unlock(&cw_battery_mutex);

	if(ret < 0) {
		dev_err(&cw_bat->client->dev,
				"%s: i2c write at address 0x%02x failed\n", __func__, reg);
	}

	return ret;
}
static int cw_i2c_read_word_data(struct cw_battery *cw_bat, u8 reg)
{
	int ret = 0;
	int i = 0;

	mutex_lock(&cw_battery_mutex);
	for (i = 0; i < I2C_RETRY_TIME_MAX; i++) {
		ret = i2c_smbus_read_word_data(cw_bat->client, reg);
		if (ret < 0) {
			dev_err(&cw_bat->client->dev, "[%s,%d] i2c_smbus_read_byte_data failed\n", __FUNCTION__, __LINE__);
		} else {
			break;
		}
		msleep(SLEEP_TIMEOUT_MS);
	}
	mutex_unlock(&cw_battery_mutex);

	if(ret < 0) {
		dev_err(&cw_bat->client->dev,
				"%s: i2c read at address 0x%02x failed\n", __func__, reg);
	} else {
		ret = (((u16)(ret & (0x00FF)) << 8) + (u16)(((ret & (0xFF00)) >> 8)));
	}

	return ret;

}

static int cw_update_config_info(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val;
	int k = 0;
	u8 reset_val;
	u16 value;

	/* make sure no in sleep mode */
	ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
		return ret;
	reset_val = reg_val;
	if((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("Error, device in sleep mode, cannot update battery info\n");
#endif
		return -1;
	}

	/* update new battery info */
	for(k = 0; k < 0x40; k += 2) {
		value = (cw_bat->plat_data->fg_model_tbl[cw_bat->data_baseaddr + k] << 8) + (cw_bat->plat_data->fg_model_tbl[cw_bat->data_baseaddr + (k + 1)]);
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("cw:value = %0x,swab16(value)=%0x\n", value, swab16(value));
#endif
		cw_i2c_write_word_data(cw_bat, REG_BATINFO + k, value);
		if(ret < 0) {
			dev_err(&cw_bat->client->dev, "write reg table(0x40~0x80) failed[%d]\n", __LINE__);
			return ret;
		}
	}
	/* readback & check */
	for(k = 0; k < 0x40; k += 2) {
		value = (cw_bat->plat_data->fg_model_tbl[cw_bat->data_baseaddr + k] << 8) + (cw_bat->plat_data->fg_model_tbl[cw_bat->data_baseaddr + (k + 1)]);
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("cw:value = %0x\n", value);
#endif
		ret = cw_i2c_read_word_data(cw_bat, REG_BATINFO + k);
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("cw:ret = %0x,k=%d\n", ret, k);
#endif
		if (ret != value)
			return -1;
	}

	/* set cw2015/cw2013 to use new battery info */
	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	reg_val |= CONFIG_UPDATE_FLG;   /* set UPDATE_FLAG */
	reg_val &= 0x07;                /* clear ATHD */
	reg_val |= (cw_bat->alt << 3);                /* set ATHD */
	ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	/* check 2015/cw2013 for ATHD & update_flag */
	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	if (!(reg_val & CONFIG_UPDATE_FLG)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("update flag for new battery info have not set..\n");
#endif
	}

	if ((reg_val & 0xf8) != (cw_bat->alt << 3)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("the new ATHD have not set..\n");
#endif
	}

	/* reset */
	reset_val &= ~(MODE_RESTART);
	reg_val = reset_val | MODE_RESTART;
	ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
		return ret;

	msleep(10);
	ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
	if (ret < 0)
		return ret;
#ifdef  BAT_CHANGE_ALGORITHM
	PowerResetFlag = 1;
#endif

	return 0;
}

static int cw_init(struct cw_battery *cw_bat)
{
	int ret;
	int i, k = 0;
	u8 reg_val = MODE_NORMAL;
	u16 value;

	ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
		return ret;

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	if ((reg_val & 0xf8) != (cw_bat->alt << 3)) {
		reg_val &= 0x07;    /* clear ATHD */
		reg_val |= (cw_bat->alt << 3);  /* set ATHD */
		ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
		if (ret < 0)
			return ret;
	}

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("cw_init REG_CONFIG = %d\n", reg_val);
#endif
	if (!(reg_val & CONFIG_UPDATE_FLG)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("update flag for new battery info have not set\n");
#endif
		ret = cw_update_config_info(cw_bat);
		if (ret < 0)
			return ret;
	} else {
		for(k = 0; k < 0x40; k += 2) {
			value = (cw_bat->plat_data->fg_model_tbl[cw_bat->data_baseaddr + k] << 8) + (cw_bat->plat_data->fg_model_tbl[cw_bat->data_baseaddr + (k + 1)]);
#ifdef FG_CW2015_DEBUG
			FG_CW2015_LOG("cw:value = %0x\n", value);
#endif
			ret = cw_i2c_read_word_data(cw_bat, REG_BATINFO + k);
#ifdef FG_CW2015_DEBUG
			FG_CW2015_LOG("cw:ret = %0x\n", ret);
#endif
			if (ret != value) {
				ret = cw_update_config_info(cw_bat);
				if (ret < 0)
					return ret;
			}
		}
	}

	for (i = 0; i < 30; i++) {
		ret = cw_read(cw_bat->client, REG_SOC, &reg_val);
		if (ret < 0)
			return ret;
		else if (reg_val <= 0x64)
			break;

		msleep(100);
		if (i > 25) {
#ifdef FG_CW2015_DEBUG
			FG_CW2015_ERR("cw2015/cw2013 input unvalid power error\n");
#endif
		}

	}
	if (i >= 30) {
		reg_val = MODE_SLEEP;
		ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("cw2015/cw2013 input unvalid power error_2\n");
#endif
		return -1;
	}

	return 0;
}

static void cw_update_time_member_charge_start(struct cw_battery *cw_bat)
{
	struct timespec ts;
	int new_run_time;
	int new_sleep_time;

	ktime_get_ts(&ts);
	new_run_time = ts.tv_sec;

	get_monotonic_boottime(&ts);
	new_sleep_time = ts.tv_sec - new_run_time;

	cw_bat->run_time_charge_start = new_run_time;
	cw_bat->sleep_time_charge_start = new_sleep_time;
}

static void cw_update_time_member_capacity_change(struct cw_battery *cw_bat)
{
	struct timespec ts;
	int new_run_time;
	int new_sleep_time;

	ktime_get_ts(&ts);
	new_run_time = ts.tv_sec;

	get_monotonic_boottime(&ts);
	new_sleep_time = ts.tv_sec - new_run_time;

	cw_bat->run_time_capacity_change = new_run_time;
	cw_bat->sleep_time_capacity_change = new_sleep_time;
}

static int cw_get_capacity(struct cw_battery *cw_bat)
{
	int cw_capacity;
	int ret;
	u8 reg_val[2];

	struct timespec ts;
	long new_run_time;
	long new_sleep_time;
	long capacity_or_aconline_time;
	int allow_change;
	int allow_capacity;
	static int if_quickstart = 0;
	static int jump_flag = 0;
	static int reset_loop = 0;
	int charge_time;
	u8 reset_val;

	ret = cw_read_word(cw_bat->client, REG_SOC, reg_val);
	if (ret < 0)
		return ret;
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("cw_get_capacity cw_capacity_0 = %d,cw_capacity_1 = %d\n", reg_val[0], reg_val[1]);
#endif
	cw_capacity = reg_val[0];
	if ((cw_capacity < 0) || (cw_capacity > 100)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("get cw_capacity error; cw_capacity = %d\n", cw_capacity);
#else
              printk(KERN_INFO "get cw_capacity error; cw_capacity = %d\n", cw_capacity);
#endif
		reset_loop++;
		if (reset_loop > 5) {
			reset_val = MODE_SLEEP;
			ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
			if (ret < 0)
				return ret;
			reset_val = MODE_NORMAL;
			msleep(10);
			ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
			if (ret < 0)
				return ret;
			ret = cw_init(cw_bat);
			if (ret)
				return ret;
			reset_loop = 0;
		}
		cw_capacity = cw_bat->capacity;

               read_soc_err = 0;
		return cw_capacity;
	} else {
		reset_loop = 0;
	}
#ifdef  BAT_CHANGE_ALGORITHM
	cw_capacity = cw_algorithm(cw_bat, cw_capacity);
#endif

	ktime_get_ts(&ts);
	new_run_time = ts.tv_sec;

	get_monotonic_boottime(&ts);
	new_sleep_time = ts.tv_sec - new_run_time;
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("cw_get_capacity cw_bat->charger_mode = %d\n", cw_bat->charger_mode);
#endif

	if (/*((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 9)))
			||*/ ((cw_bat->charger_mode == 0) && (cw_capacity > (cw_bat->capacity)) && (cw_capacity < (cw_bat->capacity + 20))
                     &&(alg_firstsuc_flag ==1)&&((count_adjust>=3)|| (alg_run_flag == -1)))) {             // modify battery level swing

		if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) {
			cw_capacity = cw_bat->capacity;
		}
	}

       if ((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 9)))
       {
            cw_capacity = cw_bat->capacity;
       }

	if ((cw_bat->charger_mode > 0) && (cw_capacity >= 98) && (cw_capacity <= cw_bat->capacity)) {     // avoid no charge full

		capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
		capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
		allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_UP_MAX_CHANGE;
		if (allow_change > 0) {
			allow_capacity = cw_bat->capacity + allow_change;
			cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
			jump_flag = 1;
		} else if (cw_capacity <= cw_bat->capacity) {
			cw_capacity = cw_bat->capacity;
		}

	}

	else if ((cw_bat->charger_mode == 0) && (cw_capacity <= cw_bat->capacity ) && (cw_capacity >= 90) && (jump_flag == 1)) {     // avoid battery level jump to CW_BAT
		capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
		capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
		allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_DOWN_CHANGE;
		if (allow_change > 0) {
			allow_capacity = cw_bat->capacity - allow_change;
			if (cw_capacity >= allow_capacity) {
				jump_flag = 0;
			} else {
				cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
			}
		} else if (cw_capacity <= cw_bat->capacity) {
			cw_capacity = cw_bat->capacity;
		}
	}

	if ((cw_capacity == 0) && (cw_bat->capacity > 1)) {              // avoid battery level jump to 0% at a moment from more than 2%
		allow_change = ((new_run_time - cw_bat->run_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_RUN);
		allow_change += ((new_sleep_time - cw_bat->sleep_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_SLEEP);

		allow_capacity = cw_bat->capacity - allow_change;
		cw_capacity = (allow_capacity >= cw_capacity) ? allow_capacity : cw_capacity;
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("report GGIC POR happened\n");
#endif

		reset_val = MODE_SLEEP;
		ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
		if (ret < 0)
			return ret;
		reset_val = MODE_NORMAL;
		msleep(10);
		ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
		if (ret < 0)
			return ret;

		ret = cw_init(cw_bat);
		if (ret)
			return ret;

	}

#if 1
	if((cw_bat->charger_mode > 0) && (cw_capacity == 0)) {
		charge_time = new_sleep_time + new_run_time - cw_bat->sleep_time_charge_start - cw_bat->run_time_charge_start;
		if ((charge_time > BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE) && (if_quickstart == 0)) {
			reset_val = MODE_SLEEP;
			ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
			if (ret < 0)
				return ret;
			reset_val = MODE_NORMAL;
			msleep(10);
			ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
			if (ret < 0)
				return ret;

			ret = cw_init(cw_bat);
			if (ret)
				return ret;      // if the cw_capacity = 0 the cw2015 will qstrt
#ifdef FG_CW2015_DEBUG
			FG_CW2015_LOG("report battery capacity still 0 if in changing\n");
#endif
			if_quickstart = 1;
		}
	} else if ((if_quickstart == 1) && (cw_bat->charger_mode == 0)) {
		if_quickstart = 0;
	}

#endif

	return cw_capacity;
}

static int cw_get_vol(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val[2];
	u16 value16, value16_1, value16_2, value16_3;
	int voltage;
	ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
	if (ret < 0) {

		return ret;
	}
	value16 = (reg_val[0] << 8) + reg_val[1];

	ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
	if (ret < 0) {

		return ret;
	}
	value16_1 = (reg_val[0] << 8) + reg_val[1];

	ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
	if (ret < 0) {

		return ret;
	}
	value16_2 = (reg_val[0] << 8) + reg_val[1];


	if(value16 > value16_1) {
		value16_3 = value16;
		value16 = value16_1;
		value16_1 = value16_3;
	}

	if(value16_1 > value16_2) {
		value16_3 = value16_1;
		value16_1 = value16_2;
		value16_2 = value16_3;
	}

	if(value16 > value16_1) {
		value16_3 = value16;
		value16 = value16_1;
		value16_1 = value16_3;
	}

	voltage = value16_1 * 312 / 1024;
	//  voltage = voltage * 1000;
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("cw_get_vol  voltage = %d\n", voltage);
#endif
	return voltage;
}
static void cw_bat_update_capacity(struct cw_battery *cw_bat)
{
	int cw_capacity;
	cw_capacity = cw_get_capacity(cw_bat);
#ifdef FG_CW2015_DEBUG
	FG_CW2015_ERR("cw2015_file_test userdata,	%ld,	%d, %d\n", get_seconds(), cw_capacity, cw_bat->voltage);
#endif
	if ((cw_capacity >= 0) && (cw_capacity <= 100) && (cw_bat->capacity != cw_capacity)) {
		cw_bat->capacity = cw_capacity;
		cw_bat->bat_change = 1;
		cw_update_time_member_capacity_change(cw_bat);

	}
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("cw_bat_update_capacity cw_capacity = %d\n", cw_bat->capacity);
#endif
}

static void cw_bat_update_vol(struct cw_battery *cw_bat)
{
	int ret;
	ret = cw_get_vol(cw_bat);
	if ((ret >= 0) && (cw_bat->voltage != ret)) {
		cw_bat->voltage = ret;
		cw_bat->bat_change = 1;
	}
}

static int get_usb_charge_state(struct cw_battery *cw_bat)
{
	int usb_status = 0;//dwc_vbus_status();
	FG_charging_status = pmic_get_charging_status(cw_bat->battery_info);
	FG_charging_type = pmic_get_charging_type(cw_bat->battery_info);

#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("get_usb_charge_state FG_charging_type:%d,FG_charging_status:%d\n", FG_charging_type, FG_charging_status);
#endif
	if(FG_charging_status != POWER_SUPPLY_STATUS_CHARGING) {
		usb_status = 0;
		cw_bat->charger_mode = 0;
	} else {
		if(FG_charging_type == POWER_SUPPLY_TYPE_USB) {
			usb_status = 1;
			cw_bat->charger_mode = USB_CHARGER_MODE;
		} else if(FG_charging_type == POWER_SUPPLY_TYPE_MAINS) {
			usb_status = 2;
			cw_bat->charger_mode = AC_CHARGER_MODE;
		}
	}
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("get_usb_charge_state usb_status = %d,cw_bat->charger_mode = %d\n", usb_status, cw_bat->charger_mode);
#endif
	return usb_status;

}

static int cw_usb_update_online(struct cw_battery *cw_bat)
{
	int ret = 0;
	int usb_status = 0;

#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("usb_update_online FG_charging_status = %d\n", cw_bat->charger_mode);
#endif
	usb_status = get_usb_charge_state(cw_bat);
	if (usb_status == 2) {
		if (cw_bat->charger_mode != AC_CHARGER_MODE) {
			cw_bat->charger_mode = AC_CHARGER_MODE;
			ret = 1;
		}

		if (cw_bat->usb_online != 1) {
			cw_bat->usb_online = 1;
			cw_update_time_member_charge_start(cw_bat);
		}

	} else if (usb_status == 1) {
		if (cw_bat->charger_mode != USB_CHARGER_MODE) {
			cw_bat->charger_mode = USB_CHARGER_MODE;
			ret = 1;
		}

		if (cw_bat->usb_online != 1) {
			cw_bat->usb_online = 1;
			cw_update_time_member_charge_start(cw_bat);
		}

	} else if (usb_status == 0 && cw_bat->usb_online != 0) {
		cw_bat->charger_mode = 0;
		cw_update_time_member_charge_start(cw_bat);
		cw_bat->usb_online = 0;
		ret = 1;
	}

	return ret;
}

static void cw_bat_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;
	int ret;

       mutex_lock(&data_mutex);
	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);
	ret = cw_usb_update_online(cw_bat);

	if (cw_bat->usb_online == 1)
		ret = cw_usb_update_online(cw_bat);

	cw_bat_update_capacity(cw_bat);
        mutex_unlock(&data_mutex);
	cw_bat_update_vol(cw_bat);
	//g_cw2015_capacity = cw_bat->capacity;
	g_cw2015_vol = cw_bat->voltage;
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("cw_bat_work:vol = %d,cap = %d\n", cw_bat->voltage, cw_bat->capacity);
#endif
	if (cw_bat->bat_change) {
		cw_bat->bat_change = 0;
	}
      
       if(alg_exc_count <15)
       {
           printk(KERN_INFO "[FG_CW2015]cw_bat_work timer 2s\n");
           queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(1000*2));
       }
       else
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(1000*10));

}

/*----------------------------------------------------------------------------*/
static int cw2015_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{

	strcpy(info->type, CW2015_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
int cw2015_battery_voltage(void)
{
	struct cw_battery *di = pcw_battery;

	int data = -1;
       

	if(di)
       {
		data = cw_get_vol(di);
               if ((data >= 0) && (di->voltage != data)) {
		    di->voltage = data;
	       }
       }
	else
       {

		return data;
        }

	if (data < 0) {
		data = di->old_voltage;
	} else
		di->old_voltage = data;


	FG_CW2015_LOG("read voltage result = %d mVolts\n", data);
	return data ;
}


/*
 * Return the battery Relative State-of-Charge
 * The reture value is 0 - 100%
 * Or < 0 if something fails.
 */
int cw2015_battery_capacity(void)
{
	struct cw_battery *di = pcw_battery;

	int data = -1;

       mutex_lock(&data_mutex);
	if(di)
       {
              	cw_usb_update_online(di);
		data = cw_get_capacity(di);
       }
	else
       {
              mutex_unlock(&data_mutex);
		return data;
       }

	if ((data < 0) && (data > 100)) {
		data = di->old_capacity;
	} else
		di->old_capacity = data;

	if ((data >= 0) && (data <= 100) && (di->capacity != data)) { //update di when monitor_battery call
		    di->capacity = data;
		    cw_update_time_member_capacity_change(di);
	}
       mutex_unlock(&data_mutex);
	FG_CW2015_LOG("read capacity result = %d\n", data);
	return data ;

}

/*correction recharging soc display*/
int cw2015_battery_soc(void)
{
    int data = 0;

    data = cw2015_battery_capacity();
    g_cw2015_capacity = data;
    data = (data * 1022)/1000;
    if(data > 100){
        data = 100;
    }

    return data;
}

int cw2015_battery_realsoc(void)
{
    return g_cw2015_capacity;
}

short cw2015_battery_current(void)
{
	int data = 0;

	return ((signed short)data);
}

int is_cw2015_battery_exist(void)
{

	return (1);
}

static int cw2015_get_battery_temperature(void)
{
	struct cw_battery *di = pcw_battery;

	int temp_C = 20;
	int adc_code = 0, i = 0, adc[2] = {0};
	int sum = 0;

	if(!di->plat_data->tmptblsize) {
		return temp_C;
	}
	for(i = 0; i < 2; i++) {
		adc[i] = pmic_get_adc_conversion(1);
		sum = sum + adc[i];
	}
	adc_code = sum / 2;
	if((adc_code > 2500) || (adc_code < 100))
		return temp_C;

	for (temp_C = 0; temp_C < di->plat_data->tmptblsize; temp_C++) {
		if (adc_code >= di->plat_data->battery_tmp_tbl[temp_C])
			break;
	}

	/* first 20 values are for negative temperature */
	temp_C = (temp_C - 20); /* degree Celsius */
	if(temp_C > 67) {
		FG_CW2015_LOG("battery temp over %d degree (adc=%d)\n", temp_C, adc_code);
		temp_C = 67;
	}
	return temp_C;
}


int cw2015_battery_temperature(void)
{
	int data = cw2015_get_battery_temperature();
	return data;
}

int cw2015_battery_health(void)
{
	return POWER_SUPPLY_HEALTH_GOOD;
}

int cw2015_battery_technology(void)
{
	/*Default technology is "Li-poly"*/
	int data = POWER_SUPPLY_TECHNOLOGY_LIPO;

	return data;
}
int cw2015_battery_get_fgchip_detect(void)
{
        int data = FG_CHIP_TYPE_CW2015;
        return data;
}

static struct comip_android_battery_info cw_battery_property = {
	.battery_voltage = cw2015_battery_voltage,
	.battery_current = cw2015_battery_current,
	.battery_capacity = cw2015_battery_soc,
	.battery_temperature = cw2015_battery_temperature,
	.battery_exist = is_cw2015_battery_exist,
	.battery_health = cw2015_battery_health,
	.battery_technology = cw2015_battery_technology,
	.battery_realsoc = cw2015_battery_realsoc,
        .get_fgchip_detect = cw2015_battery_get_fgchip_detect,
};

static ssize_t cw201x_show_voltage(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;

	if (NULL == buf) {
		return -1;
	}
	val = cw2015_battery_voltage();

	return sprintf(buf, "%d\n", val);
}

static ssize_t cw201x_show_capacity(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;
	if (NULL == buf) {
		return -1;
	}
	val = cw2015_battery_capacity();

	return sprintf(buf, "%d\n", val);
}

static ssize_t cw201x_show_reg(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct cw_battery *cw_bat = dev_get_drvdata(dev);
	int vcell, soc, mode, version, config, rrt_alrt;
	int ret;
	u8 reg_val;

	ret = cw_read(cw_bat->client, REG_VERSION, &reg_val);
	if (ret < 0)
		return ret;
	version = reg_val;

	ret = cw_read(cw_bat->client, REG_VCELL, &reg_val);
	if (ret < 0)
		return ret;
	vcell = reg_val;
	ret = cw_read(cw_bat->client, REG_VCELL + 1, &reg_val);
	if (ret < 0)
		return ret;
	vcell = ((vcell << 8) + reg_val) & 0xffff;

	ret = cw_read(cw_bat->client, REG_SOC, &reg_val);
	if (ret < 0)
		return ret;
	soc = reg_val;
	ret = cw_read(cw_bat->client, REG_SOC + 1, &reg_val);
	if (ret < 0)
		return ret;
	soc = ((soc << 8) + reg_val) & 0xffff;

	ret = cw_read(cw_bat->client, REG_RRT_ALERT, &reg_val);
	if (ret < 0)
		return ret;
	rrt_alrt = reg_val;
	ret = cw_read(cw_bat->client, REG_RRT_ALERT + 1, &reg_val);
	if (ret < 0)
		return ret;
	rrt_alrt = ((rrt_alrt << 8) + reg_val) & 0xffff;

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	config = reg_val;

	ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
		return ret;
	mode = reg_val;

	sprintf(buf, "cw201x reg, version:0x%x,vcell:0x%x,soc:0x%x,rrt_alrt:0x%x,config:0x%x,mode:0x%x\n",
			version, vcell, soc, rrt_alrt, config, mode);

	return strlen(buf);
}

static ssize_t cw201x_show_alertsoc(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct cw_battery *cw_bat = dev_get_drvdata(dev);
	int ret;
	u8 reg_val;

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	sprintf(buf, "REG_CONFIG = %d\n", (reg_val >> 3));


	return strlen(buf);
}


static ssize_t cw201x_set_alertsoc(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	long val;
	u8 reg_val;
	int ret;

	struct cw_battery *cw_bat = dev_get_drvdata(dev);

	if ((kstrtol(buf, 10, &val) < 0) || (val < 0) || (val > 31))
		return -EINVAL;

	cw_bat->alt = val;

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	if ((reg_val & 0xf8) != (cw_bat->alt << 3)) {
		reg_val &= 0x07;    /* clear ATHD */
		reg_val |= (cw_bat->alt << 3);  /* set ATHD */
		ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
		if (ret < 0)
			return ret;
	}

	return count;
}

static DEVICE_ATTR(gaugelog, S_IWUSR | S_IRUGO, cw201x_show_reg, NULL);
static DEVICE_ATTR(voltage,  S_IWUSR | S_IRUGO, cw201x_show_voltage, NULL);
static DEVICE_ATTR(capacity, S_IWUSR | S_IRUGO, cw201x_show_capacity, NULL);
static DEVICE_ATTR(alertsoc, S_IWUSR | S_IRUGO, cw201x_show_alertsoc, cw201x_set_alertsoc);

static struct attribute *cw201x_attributes[] = {
	&dev_attr_gaugelog.attr,
	&dev_attr_voltage.attr,
	&dev_attr_capacity.attr,
	&dev_attr_alertsoc.attr,
	NULL,
};
static const struct attribute_group cw201x_attr_group = {
	.attrs = cw201x_attributes,
};

#if defined(CONFIG_DEVINFO_COMIP)
static ssize_t cw201x_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Fuel-Gauge:CellWise,CW201x\n");
}

static DEVICE_ATTR(cw201x_info, S_IRUGO, cw201x_show, NULL);

static struct attribute *cw201xx_attributes[] = {
	&dev_attr_cw201x_info.attr,
};
#endif

static struct file *filp;
static mm_segment_t old_filp;
static int cw_reg_dump(struct cw_battery *cw_bat)
{
	u8 time[100]={0},buf[80];
	int ret = 0;
	struct timespec ts;
	struct rtc_time tm;
	int volt = 0, soc = 0, temp = 0;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	snprintf(time,24,"%d-%02d-%02d %02d:%02d:%02d UTC:",tm.tm_year + 1900,
	    tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	if(!cw_bat->filp_open) {
	    filp = filp_open("/data/local/log/cwlog.log", O_CREAT | O_RDWR | O_APPEND, 0777);
	    if (IS_ERR(filp)){
	        pr_err("cwlog: open file error!\n");
	        return -1;
	    }
	    old_filp = get_fs();
	    set_fs(KERNEL_DS);
	    cw_bat->filp_open = 1;
	}

	if(cw_bat->filp_open){
	    temp =  cw2015_battery_temperature();
	    volt = cw2015_battery_voltage();
		soc = cw2015_battery_capacity();

	    //ret = filp->f_op->write(filp,(char *)time, 24, &filp->f_pos);

		snprintf(buf,34,"%4d-%02d-%02d %02d:%02d:%02d %4d %4d %3d ",
	                tm.tm_year + 1900,tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,temp,volt,soc);

	    ret = filp->f_op->write(filp,(char *)buf, 34, &filp->f_pos);

	    ret = filp->f_op->write(filp,"\n",1, &filp->f_pos);
	}

	//set_fs(old_filp);
	//filp_close(filp,NULL);

	return 0;
}

static void cw2015_battery_dumlog_work(struct work_struct *work)
{
    struct cw_battery *cw_bat = container_of(work,
            struct cw_battery, dumlog_work.work);

    cw_reg_dump(cw_bat);

    schedule_delayed_work(&cw_bat->dumlog_work, msecs_to_jiffies(5 *1000));

    return;
}

static int cw2015_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cw_battery *cw_bat;
	int num;
	char *name;
	int retval;
	int loop = 0;
	int version;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "can't talk I2C?\n");
		return -ENODEV;
	}
	/* Get new ID for the new battery device */
	mutex_lock(&cw_battery_mutex);
	num = idr_alloc(&cw_battery_id, client, 0, 0, GFP_KERNEL);
	mutex_unlock(&cw_battery_mutex);
	if (num < 0) {
		dev_err(&client->dev, "cw201 idr_alloc failed!!\n");
		retval = num;
		goto batt_failed_0;
	}
	name = kasprintf(GFP_KERNEL, "cw201-%d", num);
	if (!name) {
		dev_err(&client->dev, "failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}
	cw_bat = kzalloc(sizeof(struct cw_battery), GFP_KERNEL);
	if (!cw_bat) {
		return -ENOMEM;
	}
	cw_bat->id = num;
	i2c_set_clientdata(client, cw_bat);
	cw_bat->plat_data = client->dev.platform_data;
	cw_bat->alt = ATHD;

	cw_bat->dev = &client->dev;
	cw_bat->client = client;
	version = cw_get_version(cw_bat);
	if(version < 0) {
		dev_err(&client->dev, "Chip version fail 0x%x,version=0x%x\n",
				client->addr, version);
		retval = -ENODEV;
		goto batt_failed_2;
	} else {
		dev_info(&client->dev, "Cw Fuel-Gauge Verion 0x%x\n", version);
	}
	cw_get_battery_id_type(cw_bat);
	retval = cw_init(cw_bat);
	while ((loop++ < 200) && (retval != 0)) {
		retval = cw_init(cw_bat);
	}

	if (retval) {
		goto batt_failed_2;
	}

	cw_bat->dc_online = 0;
	cw_bat->usb_online = 0;
	cw_bat->charger_mode = 0;
	cw_bat->capacity = 1;
	cw_bat->voltage = 0;
	cw_bat->status = 0;
	cw_bat->time_to_empty = 0;
	cw_bat->bat_change = 0;

	cw_update_time_member_capacity_change(cw_bat);
	cw_update_time_member_charge_start(cw_bat);
	pcw_battery = cw_bat;

	/*set battery info interface*/
	cw_bat->battery_info = battery_property;

	cw_bat->battery_workqueue = create_singlethread_workqueue("cw_battery");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(10));
	INIT_DEFERRABLE_WORK(&cw_bat->dumlog_work,cw2015_battery_dumlog_work);
	//schedule_delayed_work(&cw_bat->dumlog_work, msecs_to_jiffies(5000));

	retval = sysfs_create_group(&client->dev.kobj, &cw201x_attr_group);
	if (retval < 0) {
		dev_err(&client->dev, "could not create sysfs files\n");
		retval = -ENOENT;
		goto batt_failed_2;
	}
	comip_battery_property_register(&cw_battery_property);
	dev_info(&client->dev, "cw2015_i2c_probe success\n");
#if defined(CONFIG_DEVINFO_COMIP)
	/*export cw201x devinfo to sys/devinfo, used by fast amt3*/
	comip_devinfo_register(cw201xx_attributes, ARRAY_SIZE(cw201xx_attributes));
#endif
	return 0;

batt_failed_2:
	kfree(name);
	name = NULL;
batt_failed_1:
	mutex_lock(&cw_battery_mutex);
	idr_remove(&cw_battery_id, num);
	mutex_unlock(&cw_battery_mutex);
batt_failed_0:
	return retval;
}

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	cancel_delayed_work(&cw_bat->battery_delay_work);
	return 0;
}

static int cw_bat_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(100));
	return 0;
}
#else
#define cw_bat_suspend NULL
#define cw_bat_resume NULL
#endif

static int cw2015_i2c_remove(struct i2c_client *client)
{
	struct cw_battery *data = i2c_get_clientdata(client);

	FG_CW2015_FUN();

	sysfs_remove_group(&client->dev.kobj, &cw201x_attr_group);
	cancel_delayed_work(&data->battery_delay_work);
	cw2015_i2c_client = NULL;
	mutex_lock(&cw_battery_mutex);
	idr_remove(&cw_battery_id, data->id);
	mutex_unlock(&cw_battery_mutex);
	i2c_unregister_device(client);
	kfree(data);

	return 0;
}
static const struct dev_pm_ops pm_ops = {
	.suspend    = cw_bat_suspend,
	.resume     = cw_bat_resume,
};


static struct i2c_driver cw2015_i2c_driver = {
	.probe      = cw2015_i2c_probe,
	.remove     = cw2015_i2c_remove,
	.detect     = cw2015_i2c_detect,
	.id_table   = FG_CW2015_i2c_id,
	.driver = {
		.name           = CW2015_DEV_NAME,
		.pm = &pm_ops,

	},
};


/*----------------------------------------------------------------------------*/
static int __init cw_bat_init(void)
{
	return i2c_add_driver(&cw2015_i2c_driver);
}
module_init(cw_bat_init);

static void __exit cw_bat_exit(void)
{
	i2c_del_driver(&cw2015_i2c_driver);
}
module_exit(cw_bat_exit);

MODULE_AUTHOR("<ben.chen@cellwise-semi.com>");
MODULE_DESCRIPTION("cw201x battery driver");
MODULE_LICENSE("GPL");

