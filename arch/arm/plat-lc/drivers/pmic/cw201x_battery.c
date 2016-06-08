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

// #define DEBUG    1

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/i2c.h>

#include <linux/delay.h>

#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>


#include <linux/module.h>

#include <linux/init.h>
//#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>

#include <plat/cw201x_battery.h>
#include <plat/comip-battery.h>

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
#define BATTERY_UP_MAX_CHANGE   420             // the max time allow battery change quantity
#define BATTERY_DOWN_CHANGE   60                // the max time allow battery change quantity
#define BATTERY_DOWN_MIN_CHANGE_RUN 30          // the min time allow battery change quantity when run
#define BATTERY_DOWN_MIN_CHANGE_SLEEP 1800      // the min time allow battery change quantity when run 30min

#define BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE 1800

#define NO_STANDARD_AC_BIG_CHARGE_MODE 1

#define BAT_LOW_INTERRUPT    1

#define USB_CHARGER_MODE        1
#define AC_CHARGER_MODE         2

static struct i2c_client *cw2015_i2c_client; /* global i2c_client to support ioctl */
static struct workqueue_struct *cw2015_workqueue;

//#define FG_CW2015_DEBUG 0
#define FG_CW2015_TAG                  "[FG_CW2015]"
#ifdef FG_CW2015_DEBUG
#define FG_CW2015_FUN(f)               printk(KERN_ERR FG_CW2015_TAG"%s\n", __FUNCTION__)
#define FG_CW2015_ERR(fmt, args...)    printk(KERN_ERR FG_CW2015_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define FG_CW2015_LOG(fmt, args...)    printk(KERN_ERR FG_CW2015_TAG fmt, ##args)
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

struct cw_battery {
	struct i2c_client *client;
	struct workqueue_struct *battery_workqueue;
	struct delayed_work battery_delay_work;
	struct delayed_work dc_wakeup_work;
	struct delayed_work bat_low_wakeup_work;
	struct cw_bat_platform_data *plat_data;
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
};

struct cw_battery *pcw_battery;

static int cw_read(struct i2c_client *client, u8 reg, u8 buf[])
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(client,reg);
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

	ret =  i2c_smbus_write_byte_data(client,reg,buf[0]);
	return ret;
}

static int cw_read_word(struct i2c_client *client, u8 reg, u8 buf[])
{
	int ret = 0;
	unsigned int data = 0;
#if 1
	data = i2c_smbus_read_word_data(client, reg);
	buf[0] = data & 0x00FF;
	buf[1] = (data & 0xFF00)>>8;
#else
	ret = i2c_master_reg8_recv(client, reg, buf, 2, CW_I2C_SPEED);
#endif
	return ret;
}

static int cw_update_config_info(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val;
	int i;
	u8 reset_val;
#ifdef FG_CW2015_DEBUG
	FG_CW2015_LOG("func: %s-------\n", __func__);
#endif
	FG_CW2015_LOG("test cw_bat_config_info = 0x%x",cw_bat->plat_data->cw_bat_config_info[0]);
	FG_CW2015_LOG("test cw_bat_config_info = 0x%x",cw_bat->plat_data->cw_bat_config_info[1]);

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
	for (i = 0; i < SIZE_BATINFO; i++) {
#ifdef FG_CW2015_DEBUG
		// FG_CW2015_LOG("cw_bat->plat_data->cw_bat_config_info[%d] = 0x%x\n", i, \
		//                      cw_bat->plat_data->cw_bat_config_info[i]);
#endif
		ret = cw_write(cw_bat->client, REG_BATINFO + i, &cw_bat->plat_data->cw_bat_config_info[i]);

		if (ret < 0)
			return ret;
	}

	/* readback & check */
	for (i = 0; i < SIZE_BATINFO; i++) {
		ret = cw_read(cw_bat->client, REG_BATINFO + i, &reg_val);
		if (reg_val != cw_bat->plat_data->cw_bat_config_info[i])
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

	return 0;
}

static int cw_init(struct cw_battery *cw_bat)
{
	int ret;
	int i;
	u8 reg_val = MODE_SLEEP;
#if 0
	ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
		return ret;
#endif
	if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
		reg_val = MODE_NORMAL;
		ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
		if (ret < 0)
			return ret;
	}
	FG_CW2015_LOG("cw_init\n");
	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	if ((reg_val & 0xf8) != (cw_bat->alt << 3)) {
		reg_val &= 0x07;	/* clear ATHD */
		reg_val |= (cw_bat->alt << 3);	/* set ATHD */
		ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
		if (ret < 0)
			return ret;
	}

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	FG_CW2015_LOG("cw_init REG_CONFIG = %d\n",reg_val);

	if (!(reg_val & CONFIG_UPDATE_FLG)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_LOG("update flag for new battery info have not set\n");
#endif
		ret = cw_update_config_info(cw_bat);
		if (ret < 0)
			return ret;
	} else {
		for(i = 0; i < SIZE_BATINFO; i++) {
			ret = cw_read(cw_bat->client, (REG_BATINFO + i), &reg_val);
			if (ret < 0)
				return ret;

			if (cw_bat->plat_data->cw_bat_config_info[i] != reg_val)
				break;
		}

		if (i != SIZE_BATINFO) {
#ifdef FG_CW2015_DEBUG
			FG_CW2015_LOG("update flag for new battery info have not set\n");

#endif
			ret = cw_update_config_info(cw_bat);
			if (ret < 0)
				return ret;
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
	if (i >=30) {
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

static int cw_quickstart(struct cw_battery *cw_bat)
{
	int ret = 0;
	u8 reg_val = MODE_QUICK_START;

	ret = cw_write(cw_bat->client, REG_MODE, &reg_val);     //(MODE_QUICK_START | MODE_NORMAL));  // 0x30
	if(ret < 0) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("Error quick start1\n");
#endif
		return ret;
	}

	reg_val = MODE_NORMAL;
	ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
	if(ret < 0) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("Error quick start2\n");
#endif
		return ret;
	}
	return 1;
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
	static int jump_flag =0;
	static int reset_loop =0;
	int charge_time;
	u8 reset_val;
	int loop =0;

	ret = cw_read_word(cw_bat->client, REG_SOC, reg_val);
	if (ret < 0)
		return ret;
	FG_CW2015_LOG("cw_get_capacity cw_capacity_0 = %d,cw_capacity_1 = %d\n",reg_val[0],reg_val[1]);
	cw_capacity = reg_val[0];
	if ((cw_capacity < 0) || (cw_capacity > 100)) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("get cw_capacity error; cw_capacity = %d\n", cw_capacity);
#endif
		reset_loop++;

		if (reset_loop >5) {

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
			reset_loop =0;
		}

		return cw_capacity;
	} else {
		reset_loop =0;
	}

	// ret = cw_read(cw_bat->client, REG_SOC + 1, &reg_val);

	ktime_get_ts(&ts);
	new_run_time = ts.tv_sec;

	get_monotonic_boottime(&ts);
	new_sleep_time = ts.tv_sec - new_run_time;
	FG_CW2015_LOG("cw_get_capacity cw_bat->charger_mode = %d\n",cw_bat->charger_mode);

	if (((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 9)))
	    || ((cw_bat->charger_mode == 0) && (cw_capacity == (cw_bat->capacity + 1)))) {             // modify battery level swing

		if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) {
			cw_capacity = cw_bat->capacity;
		}
	}

	if ((cw_bat->charger_mode > 0) && (cw_capacity >= 95) && (cw_capacity <= cw_bat->capacity)) {     // avoid no charge full

		capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
		capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
		allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_UP_MAX_CHANGE;
		if (allow_change > 0) {
			allow_capacity = cw_bat->capacity + allow_change;
			cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
			jump_flag =1;
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
				jump_flag =0;
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
		cw_capacity = (allow_capacity >= cw_capacity) ? allow_capacity: cw_capacity;
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
	if((cw_bat->charger_mode > 0) &&(cw_capacity == 0)) {
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
	} else if ((if_quickstart == 1)&&(cw_bat->charger_mode == 0)) {
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
		value16_3 =value16_1;
		value16_1 =value16_2;
		value16_2 =value16_3;
	}

	if(value16 >value16_1) {
		value16_3 =value16;
		value16 =value16_1;
		value16_1 =value16_3;
	}

	voltage = value16_1 * 312 / 1024;
	voltage = voltage * 1000;
	FG_CW2015_LOG("cw_get_vol  voltage = %d\n",voltage);
	return voltage;
}

#ifdef BAT_LOW_INTERRUPT
static int cw_get_alt(struct cw_battery *cw_bat)
{
	int ret = 0;
	u8 reg_val;
	u8 value8 = 0;
	int alrt;

	ret = cw_read(cw_bat->client, REG_RRT_ALERT, &reg_val);
	if (ret < 0)
		return ret;
	value8 = reg_val;
	alrt = value8 >>7;
	value8 = value8&0x7f;
	reg_val = value8;
	ret = cw_write(cw_bat->client, REG_RRT_ALERT, &reg_val);
	if(ret < 0) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR( "Error clear ALRT\n");
#endif
		return ret;
	}

	return alrt;
}
#endif


static int cw_get_time_to_empty(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val;
	u16 value16;

	ret = cw_read(cw_bat->client, REG_RRT_ALERT, &reg_val);
	if (ret < 0)
		return ret;

	value16 = reg_val;

	ret = cw_read(cw_bat->client, REG_RRT_ALERT + 1, &reg_val);
	if (ret < 0)
		return ret;

	value16 = ((value16 << 8) + reg_val) & 0x1fff;
	return value16;
}

static void cw_bat_update_capacity(struct cw_battery *cw_bat)
{
	int cw_capacity;

	cw_capacity = cw_get_capacity(cw_bat);
	if ((cw_capacity >= 0) && (cw_capacity <= 100) && (cw_bat->capacity != cw_capacity)) {
		cw_bat->capacity = cw_capacity;
		cw_bat->bat_change = 1;
		cw_update_time_member_capacity_change(cw_bat);

	}
	FG_CW2015_LOG("cw_bat_update_capacity cw_capacity = %d\n",cw_bat->capacity);
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

static void cw_bat_update_time_to_empty(struct cw_battery *cw_bat)
{
	int ret;
	ret = cw_get_time_to_empty(cw_bat);
	if ((ret >= 0) && (cw_bat->time_to_empty != ret)) {
		cw_bat->time_to_empty = ret;
		cw_bat->bat_change = 1;
	}

}


static int get_usb_charge_state(struct cw_battery *cw_bat)
{
	int usb_status = 0;//dwc_vbus_status();
	FG_charging_status = pmic_get_charging_status(cw_bat->battery_info);
	FG_charging_type = pmic_get_charging_type(cw_bat->battery_info);

	FG_CW2015_LOG("get_usb_charge_state FG_charging_type:%d,FG_charging_status:%d\n",FG_charging_type,FG_charging_status);
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
	FG_CW2015_LOG("get_usb_charge_state usb_status = %d,cw_bat->charger_mode = %d\n",usb_status,cw_bat->charger_mode);
	return usb_status;

}

static int cw_usb_update_online(struct cw_battery *cw_bat)
{
	int ret = 0;
	int usb_status = 0;

	FG_CW2015_LOG("usb_update_online FG_charging_status = %d\n",cw_bat->charger_mode);

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

#ifdef BAT_LOW_INTERRUPT

#define WAKE_LOCK_TIMEOUT       (10 * HZ)
static struct wake_lock bat_low_wakelock;

static void bat_low_detect_do_wakeup(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, bat_low_wakeup_work);
	dev_info(&cw_bat->client->dev, "func: %s-------\n", __func__);
	cw_get_alt(cw_bat);
	//enable_irq(irq);
}

static irqreturn_t bat_low_detect_irq_handler(int irq, void *dev_id)
{
	struct cw_battery *cw_bat = dev_id;

	// disable_irq_nosync(irq); // for irq debounce
	wake_lock_timeout(&bat_low_wakelock, WAKE_LOCK_TIMEOUT);
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->bat_low_wakeup_work, msecs_to_jiffies(20));
	return IRQ_HANDLED;
}
#endif
static void cw_bat_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;
	int ret;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);
	ret = cw_usb_update_online(cw_bat);
	if (ret == 1) {
		//printk("cw_bat_work 222\n");

	}

	//FG_CW2015_LOG("cw_bat_work FG_charging_status = %d\n",FG_charging_status);
	//cw_bat->usb_online = FG_charging_status;
	if (cw_bat->usb_online == 1) {
		ret = cw_usb_update_online(cw_bat);
		if (ret == 1) {
			//printk("cw_bat_work 555\n");
		}
	}

	cw_bat_update_capacity(cw_bat);
	cw_bat_update_vol(cw_bat);
	g_cw2015_capacity = cw_bat->capacity;
	g_cw2015_vol = cw_bat->voltage;
	// cw_bat_update_time_to_empty(cw_bat);
	//printk("cw_bat_work 777 vol = %d,cap = %d\n",cw_bat->voltage,cw_bat->capacity);
	if (cw_bat->bat_change) {
		cw_bat->bat_change = 0;
	}
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(1000));

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
		data = cw_get_vol(di);
	else
		return data;

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
	if(di)
		data = cw_get_capacity(di);
	else
		return data;

	if ((data < 0) && (data > 100)) {
		data = di->old_capacity;
	} else
		di->old_capacity = data;

	FG_CW2015_LOG("read voltage result = %d mVolts\n", data);
	return data ;

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
	int adc_code = 0,i = 0, adc[2]= {0};
	int sum = 0;

	if(!di->plat_data->tmptblsize) {
		return temp_C;
	}
	for(i=0; i< 2; i++) {
		adc[i] = pmic_get_adc_conversion(1);
		sum = sum+adc[i];
	}
	adc_code = sum/2;
	if((adc_code > 2500)||(adc_code < 100))
		return temp_C;

	for (temp_C = 0; temp_C < di->plat_data->tmptblsize; temp_C++) {
		if (adc_code >= di->plat_data->battery_tmp_tbl[temp_C])
			break;
	}

	/* first 20 values are for negative temperature */
	temp_C = (temp_C - 20); /* degree Celsius */
	if(temp_C > 67) {
		FG_CW2015_LOG("battery temp over %d degree (adc=%d)\n", temp_C,adc_code);
		temp_C = 67;
	}
	FG_CW2015_LOG("battery temp %d degree (adc=%d)\n", temp_C,adc_code);
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
static struct comip_android_battery_info cw_battery_property= {
	.battery_voltage = cw2015_battery_voltage,
	.battery_current = cw2015_battery_current,
	.battery_capacity = cw2015_battery_capacity,
	.battery_temperature = cw2015_battery_temperature,
	.battery_exist = is_cw2015_battery_exist,
	.battery_health= cw2015_battery_health,
	.battery_technology = cw2015_battery_technology,
	.battery_realsoc = cw2015_battery_capacity,
};

static ssize_t cw201x_show_voltage(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	int val = 0;
	//struct cw201x_device_info *di = dev_get_drvdata(dev);

	if (NULL == buf) {
		FG_CW2015_LOG("cw201x_show_voltage Error\n");
		return -1;
	}
	val = cw2015_battery_voltage();

	//val *=1000;

	return sprintf(buf, "%d\n", val);
}

static ssize_t cw201x_show_capacity(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
	int val = 0;
	//struct cw201x_device_info *di = dev_get_drvdata(dev);

	if (NULL == buf) {
		FG_CW2015_LOG("cw201x_show_capacity Error\n");
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
	int vcell,soc,mode,version,config,rrt_alrt;
	int ret;
	u8 reg_val;
	u16 value16;

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
	        version,vcell,soc,rrt_alrt,config,mode);

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
	sprintf(buf, "REG_CONFIG = %d\n",(reg_val>>3));


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
		reg_val &= 0x07;	/* clear ATHD */
		reg_val |= (cw_bat->alt << 3);	/* set ATHD */
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

static int cw2015_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cw_battery *cw_bat;
	int ret;
	int irq;
	int irq_flags;
	int loop = 0;

	printk("cw2015_i2c_probe\n");
	//cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
	cw_bat = kzalloc(sizeof(struct cw_battery), GFP_KERNEL);
	if (!cw_bat) {
#ifdef FG_CW2015_DEBUG
		FG_CW2015_ERR("fail to allocate memory\n");
#endif
		return -ENOMEM;
	}

	i2c_set_clientdata(client, cw_bat);
	cw_bat->plat_data = client->dev.platform_data;
	cw_bat->alt = ATHD;

	cw_bat->client = client;

	ret = cw_init(cw_bat);
	while ((loop++ < 200) && (ret != 0)) {
		ret = cw_init(cw_bat);
	}

	if (ret)
		return ret;

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
	cw_bat->battery_info = &battery_property;

	cw_bat->battery_workqueue = create_singlethread_workqueue("cw_battery");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(10));

#ifdef BAT_LOW_INTERRUPT
	INIT_DELAYED_WORK(&cw_bat->bat_low_wakeup_work, bat_low_detect_do_wakeup);
	wake_lock_init(&bat_low_wakelock, WAKE_LOCK_SUSPEND, "bat_low_detect");
	if (cw_bat->plat_data->bat_low_pin > 0) {
		irq = gpio_to_irq(cw_bat->plat_data->bat_low_pin);
		ret = request_irq(irq, bat_low_detect_irq_handler, IRQF_TRIGGER_FALLING, "bat_low_detect", cw_bat);
		if (ret < 0) {
			dev_err(&client->dev, "irq init failed\n");
			goto batt_failed_3;

		}
		enable_irq_wake(irq);
	}
#endif

	ret = sysfs_create_group(&client->dev.kobj, &cw201x_attr_group);
	if (ret < 0) {
		dev_err(&client->dev, "could not create sysfs files\n");
		ret = -ENOENT;
		goto batt_failed_4;
	}
	comip_battery_property_register(&cw_battery_property);
	return ret;

batt_failed_4:
	free_irq(gpio_to_irq(cw_bat->plat_data->bat_low_pin), cw_bat);
batt_failed_3:
	wake_lock_destroy(&bat_low_wakelock);
batt_failed_2:

batt_failed_1:

batt_failed_0:
	return ret;

}

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	//dev_dbg(&cw_bat->client->dev, "%s\n", __func__);
	cancel_delayed_work(&cw_bat->battery_delay_work);
	return 0;
}

static int cw_bat_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);
	//dev_dbg(&cw_bat->client->dev, "%s\n", __func__);
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
	printk("cw2015_i2c_remove\n");

	sysfs_remove_group(&client->dev.kobj, &cw201x_attr_group);

	//__cancel_delayed_work(&data->battery_delay_work);
	cancel_delayed_work(&data->battery_delay_work);
	cw2015_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(data);

	return 0;
}

static int cw2015_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{

	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	cancel_delayed_work(&cw_bat->battery_delay_work);

	return 0;
}

static int cw2015_i2c_resume(struct i2c_client *client)
{
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(100));

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
	//.suspend    = cw2015_i2c_suspend,
	//.resume     = cw2015_i2c_resume,
	.id_table   = FG_CW2015_i2c_id,
	.driver = {
//		.owner          = THIS_MODULE,
		.name           = CW2015_DEV_NAME,
		.pm = &pm_ops,

	},
};


/*----------------------------------------------------------------------------*/
static int __init cw_bat_init(void)
{
	return i2c_add_driver(&cw2015_i2c_driver);
}
fs_initcall_sync(cw_bat_init);

static void __exit cw_bat_exit(void)
{
	i2c_del_driver(&cw2015_i2c_driver);
}
module_exit(cw_bat_exit);

MODULE_AUTHOR("<ben.chen@cellwise-semi.com>");
MODULE_DESCRIPTION("cw201x battery driver");
MODULE_LICENSE("GPL");

