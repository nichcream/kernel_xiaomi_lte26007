/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013 LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the lc1100ht hardware plarform's i2c driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2013-03-07	zouqiao		created
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <plat/comip-pmic.h>
#include <plat/comip-calib.h>
#include <plat/lc1100ht.h>
#include <plat/bootinfo.h>

#define LC1100HT_DEBUG
#ifdef LC1100HT_DEBUG
#define LC1100HT_PRINT(fmt, args...) printk(KERN_ERR "[LC1100HT]" fmt, ##args)
#else
#define LC1100HT_PRINT(fmt, args...)
#endif

/* LC1100HT IRQ mask. */
#define LC1100HT_IRQ_POWER_KEY_MASK			(LC1100HT_INT_PWR_ON)
#define LC1100HT_IRQ_RTC1_MASK 				(LC1100HT_INT_RTC_RTC_ALARM1)
#define LC1100HT_IRQ_RTC2_MASK 				(LC1100HT_INT_RTC_RTC_ALARM2)
#define LC1100HT_IRQ_BATTERY_MASK 			(LC1100HT_INT_TSDL \
								| LC1100HT_INT_TSDH \
								| LC1100HT_INT_UVLO \
								| LC1100HT_INT_CHG_INPUT_STATE \
								| LC1100HT_INT_EOC \
								| LC1100HT_INT_CHG_RESTART \
								| LC1100HT_INT_RESTART_TIMEOUT \
								| LC1100HT_INT_FULLCHG_TIMEOUT \
								| LC1100HT_INT_PRECHG_TIMEOUT \
								| LC1100HT_INT_RTC_ENTER_SYS_SUPPORT \
								| LC1100HT_INT_RTC_EXIT_SYS_SUPPORT \
								| LC1100HT_INT_RTC_NO_BATT)
#define LC1100HT_IRQ_COMP1_MASK				(LC1100HT_INT_COMP1)
#define LC1100HT_IRQ_COMP2_MASK				(LC1100HT_INT_COMP2)

/* LDO ID macro. */
#define LC1100HT_REG_ENABLE_NUM				(4)
#define LC1100HT_ALDO_ID_INDEX(id) 			(((id - PMIC_ALDO1 + 4) / 8) + 2)
#define LC1100HT_ALDO_ID_OFFSET(id)			((id - PMIC_ALDO1 + 4) % 8)
#define LC1100HT_DLDO_ID_INDEX(id) 			(((id - PMIC_DLDO1) / 8) + 1)
#define LC1100HT_DLDO_ID_OFFSET(id)			((id - PMIC_DLDO1) % 8)

/**
 * Range of struct rtc_time members :
 *struct rtc_time {
 *	int tm_sec;	0~59
 *	int tm_min;	0~59
 *	int tm_hour;	0~23
 *	int tm_mday;	1~31
 *	int tm_mon;	0~11, since Jan
 *	int tm_year;	years from 1900
 *	int tm_wday;	0~6, since Sunday
 *	int tm_yday;	0~365, since Jan 1
 *	int tm_isdst;
 *};
 *
 * Range of lc1100ht rtc :
 *	second : 0~59
 *	minute : 0~59
 *	hour : 0~23
 *	mday : 1~31
 *	month : 1~12, since Jan
 *	week : 0~6, since Monday
 *	year : years from 2000
 **/
#define LC1100HT_RTC_YEAR_CHECK(year) 		\
		((year < (LC1100HT_RTC_YEAR_MIN - 1900)) || ((year) > (LC1100HT_RTC_YEAR_MAX - 1900)))
#define LC1100HT_RTC_YEAR2REG(year)			((year) + 1900 - LC1100HT_RTC_YEAR_BASE)
#define LC1100HT_RTC_REG2YEAR(reg)			((reg) + LC1100HT_RTC_YEAR_BASE - 1900)
#define LC1100HT_RTC_REG2DAY(reg)			(reg)
#define LC1100HT_RTC_DAY2REG(day)			(day)
#define LC1100HT_RTC_MONTH2REG(month)			((month) + 1)
#define LC1100HT_RTC_REG2MONTH(reg)			((reg) - 1)

/* Macro*/
#define LC1100HT_DIM(x)					ARRAY_SIZE(x)
#define LC1100HT_ARRAY(x)				(int *)&x, ARRAY_SIZE(x)

/* Each client has this additional data */
struct lc1100ht_data {
	struct mutex reg_lock;
	struct mutex adc_lock;
	struct mutex power_lock;
	struct work_struct irq_work;
	struct lc1100ht_platform_data *pdata;
	int irq;
	int irq_gpio;
	int irq_mask;
	int power_on_type;
	u8 version_id;
	u8 enable_reg[LC1100HT_REG_ENABLE_NUM];
};

const unsigned int g_lc1100ht_enable_reg[LC1100HT_REG_ENABLE_NUM] = {
			LC1100HT_REG_BUCK_LDO_EN_0, LC1100HT_REG_BUCK_LDO_EN_1,
			LC1100HT_REG_BUCK_LDO_EN_2, LC1100HT_REG_BUCK_LDO_EN_3
};

const u8 g_lc1100ht_sink_sacle0_val[] = {
			6, 12, 18, 25, 31, 37, 43, 50,
			56, 62, 68, 75, 81, 87, 93, 100
};

const u8 g_lc1100ht_sink_sacle1_val[] = {
			7, 15, 22, 30, 37, 45, 52, 60,
			67, 75, 82, 90, 97, 105, 112, 120
};

const int g_lc1100ht_1800_3000_13_vout[] = {
			1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500,
			2600, 2700, 2800, 2900, 3000
};

const int g_lc1100ht_1800_3000_14_vout[] = {
			1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500,
			2600, 2700, 2800, 2900, 3000, 2850
};
const int g_lc1100ht_3000_3600_7_vout[] = {
			3000, 3100, 3200, 3300, 3400, 3500, 3600
};

const int g_lc1100ht_1200_1800_7_vout[] = {
			1200, 1300, 1400, 1500, 1600, 1700, 1800
};

const int g_lc1100ht_1800_2850_2_vout[] = {1800, 2850};

const int g_lc1100ht_1800_3000_2_vout[] = {1800, 3000};

const int g_lc1100ht_1100_1500_9_vout[] = {
			1100, 1150, 1200, 1250, 1300, 1350, 1400, 1450, 1500
};

#define LC1100HT_LDO_VOUT_MAP_ITEM(index)			&g_lc1100ht_ldo_vout_map[index]
#define LC1100HT_LDO_VOUT_MAP_NUM()				LC1100HT_DIM(g_lc1100ht_ldo_vout_map)
static struct lc1100ht_ldo_vout_map g_lc1100ht_ldo_vout_map[] = {
	{PMIC_ALDO1, 	LC1100HT_ARRAY(g_lc1100ht_1800_2850_2_vout),	LC1100HT_REG_ALDO1_OUT, 0x01},
	{PMIC_ALDO6,	LC1100HT_ARRAY(g_lc1100ht_1800_3000_13_vout),	LC1100HT_REG_ALDO6_OUT, 0x1f},
	{PMIC_ALDO7,	LC1100HT_ARRAY(g_lc1100ht_1200_1800_7_vout),	LC1100HT_REG_ALDO7_OUT, 0x1f},
	{PMIC_DLDO1, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_14_vout),	LC1100HT_REG_DLDO1_OUT, 0x1f},
	{PMIC_DLDO2, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_14_vout),	LC1100HT_REG_DLDO2_OUT, 0x1f},
	{PMIC_DLDO3, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_14_vout),	LC1100HT_REG_DLDO3_OUT, 0x1f},
	{PMIC_DLDO4, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_2_vout),	LC1100HT_REG_DLDO4_OUT, 0x01},
	{PMIC_DLDO5, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_13_vout),	LC1100HT_REG_DLDO5_OUT, 0x1f},
	{PMIC_DLDO6, 	LC1100HT_ARRAY(g_lc1100ht_3000_3600_7_vout),	LC1100HT_REG_DLDO6_OUT, 0x1f},
	{PMIC_DLDO7, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_13_vout),	LC1100HT_REG_DLDO7_OUT, 0x1f},
	{PMIC_DLDO8, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_13_vout),	LC1100HT_REG_DLDO8_OUT, 0x1f},
	{PMIC_DLDO9, 	LC1100HT_ARRAY(g_lc1100ht_1800_3000_14_vout),	LC1100HT_REG_DLDO9_OUT, 0x1f},
	{PMIC_DLDO10, 	LC1100HT_ARRAY(g_lc1100ht_1100_1500_9_vout),	LC1100HT_REG_DLDO10_OUT, 0x1f},
	{PMIC_DLDO11, 	LC1100HT_ARRAY(g_lc1100ht_1100_1500_9_vout),	LC1100HT_REG_DLDO11_OUT, 0x1f},
};

#define LC1100HT_LDO_CTRL_MAP_ITEM(index) 			&g_lc1100ht_ldo_ctrl_map_default[index]
#define LC1100HT_LDO_CTRL_MAP_NUM()				LC1100HT_DIM(g_lc1100ht_ldo_ctrl_map_default)
static struct pmic_power_ctrl_map g_lc1100ht_ldo_ctrl_map_default[] = {
	{PMIC_DCDC1,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC2,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC3,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC4,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_ALDO1,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO2,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO3,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO5,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO6,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2800},
	{PMIC_ALDO7,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
	{PMIC_ALDO8,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2500},
	{PMIC_ALDO9,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2500},
	{PMIC_ALDO10, 	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1100},
	{PMIC_DLDO1, 	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO2, 	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO3, 	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO4, 	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO5, 	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO6, 	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	3300},
	{PMIC_DLDO7, 	PMIC_POWER_CTRL_GPIO, 		PMIC_POWER_CTRL_GPIO_ID_NONE,	3000},
	{PMIC_DLDO8, 	PMIC_POWER_CTRL_REG, 		PMIC_POWER_CTRL_GPIO_ID_NONE,	3000},
	{PMIC_DLDO9, 	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO10,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	1500},
	{PMIC_DLDO11,	PMIC_POWER_CTRL_GPIO, 		PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
	{PMIC_DLDO12,	PMIC_POWER_CTRL_REG, 		PMIC_POWER_CTRL_GPIO_ID_NONE,	2500},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	6},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	6},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	6},
	{PMIC_EXTERNAL1,PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
};

#define LC1100HT_LDO_MODULE_MAP() 				g_lc1100ht_ldo_module_map
#define LC1100HT_LDO_MODULE_MAP_ITEM(index)			&g_lc1100ht_ldo_module_map[index]
#define LC1100HT_LDO_MODULE_MAP_NUM() 				g_lc1100ht_ldo_module_map_num
static struct pmic_power_module_map* g_lc1100ht_ldo_module_map;
static int g_lc1100ht_ldo_module_map_num;

static struct i2c_client *lc1100ht_client;

int lc1100ht_reg_read(u8 reg, u8 *value)
{
	int ret;

	if (!lc1100ht_client)
		return -1;

	ret = i2c_smbus_read_byte_data(lc1100ht_client, reg);
	if(ret < 0)
		return ret;

	*value = (u8)ret;

	return 0;
}
EXPORT_SYMBOL(lc1100ht_reg_read);

int lc1100ht_reg_write(u8 reg, u8 value)
{
	int ret;

	if (!lc1100ht_client)
		return -1;

	ret = i2c_smbus_write_byte_data(lc1100ht_client, reg, value);
	if(ret > 0)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(lc1100ht_reg_write);

int lc1100ht_reg_bit_write(u8 reg, u8 mask, u8 value)
{
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);
	u8 valo, valn;
	int ret;

	if (!mask)
		return -EINVAL;

	mutex_lock(&data->reg_lock);

	ret = lc1100ht_reg_read(reg, &valo);
	if (!ret) {
		valn = valo & ~mask;
		valn |= (value << (ffs(mask) - 1));
		ret = lc1100ht_reg_write(reg, valn);
	}

	mutex_unlock(&data->reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100ht_reg_bit_write);

int lc1100ht_reg_bit_set(u8 reg, u8 value)
{
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);
	u8 val;
	int ret;

	mutex_lock(&data->reg_lock);

	ret = lc1100ht_reg_read(reg, &val);
	if (!ret) {
		val |= value;
		ret = lc1100ht_reg_write(reg, val);
	}

	mutex_unlock(&data->reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100ht_reg_bit_set);

int lc1100ht_reg_bit_clr(u8 reg, u8 value)
{
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);
	u8 val;
	int ret;

	mutex_lock(&data->reg_lock);

	ret = lc1100ht_reg_read(reg, &val);
	if (!ret) {
		val &= ~value;
		ret = lc1100ht_reg_write(reg, val);
	}

	mutex_unlock(&data->reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100ht_reg_bit_clr);

int lc1100ht_read_adc(u8 val, u16 *out)
{
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);
	u8 data0, data1;
	u8 ad_state, timeout;
	int ret = 0;

	if(!out)
		return -EINVAL;

	if (in_atomic() || irqs_disabled()) {
		if (!mutex_trylock(&data->adc_lock))
			/* ADC activity is ongoing. */
			return -EAGAIN;
	} else {
		mutex_lock(&data->adc_lock);
	}

	lc1100ht_reg_write(LC1100HT_REG_ADC_CTL, (val << 1) | 0x01);

	timeout = 3;
	do {
		udelay(100);
		timeout--;
		lc1100ht_reg_read(LC1100HT_REG_ADC_CONVERSION_DONE, &ad_state);
		ad_state = !!(ad_state & LC1100HT_REG_BITMASK_ADC_CONV_DONE);
	} while(ad_state != 1 && timeout != 0);

	if (timeout == 0) {
		LC1100HT_PRINT("ADC convert timeout!\n");
		data0 = data1 = 0xff; /* Indicate ADC convert failed */
		ret = -ETIMEDOUT; /* Indicate ADC convert failed */
	} else {
		lc1100ht_reg_read(LC1100HT_REG_ADC_DATA1, &data0);
		lc1100ht_reg_read(LC1100HT_REG_ADC_DATA0, &data1);
	}

	*out = ((u16)data1 << 4) | ((u16)data0 >> 4);

	mutex_unlock(&data->adc_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100ht_read_adc);

int lc1100ht_int_mask(int mask)
{
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);
	u8 int_mask0;
	u8 int_mask1;
	u8 int_mask2;

	mutex_lock(&data->reg_lock);

	data->irq_mask |= mask;

	if (mask & 0x000000ff) {
		int_mask0 = (u8)(data->irq_mask & 0x000000ff);
		lc1100ht_reg_write(LC1100HT_REG_INT_EN_1, int_mask0);
	}

	if ((mask >> 8) & 0x000000ff) {
		int_mask1 = (u8)((data->irq_mask >> 8) & 0x000000ff);
		lc1100ht_reg_write(LC1100HT_REG_INT_EN_2, int_mask1);
	}

	if ((mask >> 16) & 0x000000ff) {
		int_mask2 = (u8)((data->irq_mask >> 16) & 0x000000ff);
		lc1100ht_reg_write(LC1100HT_REG_INT_EN_3, int_mask2);
	}

	mutex_unlock(&data->reg_lock);

	return 0;
}
EXPORT_SYMBOL(lc1100ht_int_mask);

int lc1100ht_int_unmask(int mask)
{
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);
	u8 int_mask0;
	u8 int_mask1;
	u8 int_mask2;

	mutex_lock(&data->reg_lock);

	data->irq_mask &= ~mask;

	if (mask & 0x000000ff) {
		int_mask0 = (u8)(data->irq_mask & 0x000000ff);
		lc1100ht_reg_write(LC1100HT_REG_INT_EN_1, ~int_mask0);
	}

	if ((mask >> 8) & 0x000000ff) {
		int_mask1 = (u8)((data->irq_mask >> 8) & 0x000000ff);
		lc1100ht_reg_write(LC1100HT_REG_INT_EN_2, ~int_mask1);
	}

	if ((mask >> 16) & 0x000000ff) {
		int_mask2 = (u8)((data->irq_mask >> 16) & 0x000000ff);
		lc1100ht_reg_write(LC1100HT_REG_INT_EN_3, ~int_mask2);
	}

	mutex_unlock(&data->reg_lock);

	return 0;
}
EXPORT_SYMBOL(lc1100ht_int_unmask);

int lc1100ht_int_status_get(void)
{
	u8 status0 = 0;
	u8 status1 = 0;
	u8 status2 = 0;

	lc1100ht_reg_read(LC1100HT_REG_INT_STATE_0, &status0);
	lc1100ht_reg_read(LC1100HT_REG_INT_STATE_1, &status1);
	lc1100ht_reg_read(LC1100HT_REG_INT_STATE_2, &status2);

	return (((int)status2 << 16) | ((int)status1 << 8) | (int)status0);
}
EXPORT_SYMBOL(lc1100ht_int_status_get);

static int lc1100ht_pmic_reg_read(u16 reg, u16 *value)
{
	u8 val;
	int ret;

	ret = lc1100ht_reg_read((u8)reg, &val);
	if(ret < 0)
		return ret;

	*value = (u16)val;

	return ret;
}

static int lc1100ht_pmic_reg_write(u16 reg, u16 value)
{
	return lc1100ht_reg_write((u8)reg, (u8)value);
}

static void lc1100ht_ldo_ctrl_set(u8 id, u8 ctrl)
{
	u8 bit;

	if ((ctrl != PMIC_POWER_CTRL_REG) && (ctrl != PMIC_POWER_CTRL_GPIO)) {
		LC1100HT_PRINT("LDO control, invalid control id\n");
		return;
	}

	if (id == PMIC_ALDO1)
		bit = 5;
	else if (id == PMIC_ALDO2)
		bit = 4;
	else if (id == PMIC_ALDO3)
		bit = 4;
	else if (id == PMIC_ALDO4)
		bit = 4;
	else if (id == PMIC_ALDO5)
		bit = 3;
	else if (id == PMIC_ALDO7)
		bit = 2;
	else if (id == PMIC_DLDO7)
		bit = 1;
	else if (id == PMIC_DLDO9)
		bit = 0;
	else if (id == PMIC_DLDO11)
		bit = 0;
	else {
		LC1100HT_PRINT("LDO control, invalid LDO id %d\n", id);
		return;
	}

	if (ctrl == PMIC_POWER_CTRL_REG)
		lc1100ht_reg_bit_clr(LC1100HT_REG_LDO_EXTERNAL_PIN, (1 << bit));
	else
		lc1100ht_reg_bit_set(LC1100HT_REG_LDO_EXTERNAL_PIN, (1 << bit));
}

static void lc1100ht_ldo_reg_enable(u8 id, u8 operation)
{
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);
	u8 index = LC1100HT_REG_ENABLE_NUM;
	u8 offset;
	u8 op;

	if (PMIC_IS_ALDO(id)) {
		index = LC1100HT_ALDO_ID_INDEX(id);
		offset = LC1100HT_ALDO_ID_OFFSET(id);
	} else if (PMIC_IS_DLDO(id)) {
		index = LC1100HT_DLDO_ID_INDEX(id);
		offset = LC1100HT_DLDO_ID_OFFSET(id);
	} else {
		LC1100HT_PRINT("LDO enable, invalid LDO id %d\n", id);
		return;
	}

	if (index < LC1100HT_REG_ENABLE_NUM) {
		mutex_lock(&data->reg_lock);

		op = (data->enable_reg[index] & (1 << offset)) ? 1 : 0;
		if (op != operation) {
			if (operation)
				data->enable_reg[index] |= (1 << offset);
			else
				data->enable_reg[index] &= ~(1 << offset);

			lc1100ht_reg_write(g_lc1100ht_enable_reg[index], data->enable_reg[index]);
		}

		mutex_unlock(&data->reg_lock);
	}
}

static void lc1100ht_ldo_vout_set(u8 id, int mv)
{
	struct lc1100ht_ldo_vout_map *vout_map;
	u8 i;
	u8 j;

	for (i = 0; i < LC1100HT_LDO_VOUT_MAP_NUM(); i++) {
		vout_map = LC1100HT_LDO_VOUT_MAP_ITEM(i);
		if (vout_map->ldo_id == id)
			break;
	}

	if (i == LC1100HT_LDO_VOUT_MAP_NUM()) {
		LC1100HT_PRINT("LDO vout. can't find the ldo id %d\n", id);
		return;
	}

	for (j = 0; j < vout_map->vout_range_size; j++) {
		if (mv == vout_map->vout_range[j])
			break;
	}

	if (j == vout_map->vout_range_size) {
		LC1100HT_PRINT("LDO(%d) vout. not support the voltage %dmV\n", id, mv);
		return;
	}

	lc1100ht_reg_bit_write(vout_map->reg, vout_map->mask, j);
}

static void lc1100ht_sink_enable(u8 id, u8 operation)
{
	if (operation)
		lc1100ht_reg_bit_set(LC1100HT_REG_SINK_CTL, (1 << id));
	else
		lc1100ht_reg_bit_clr(LC1100HT_REG_SINK_CTL, (1 << id));
}

static void lc1100ht_sink_iout_set(u8 id, int ma)
{
	unsigned int reg;
	u8 mask;
	u8 val;
	u8 scale;
	u8 i;

	if (ma <= g_lc1100ht_sink_sacle0_val[LC1100HT_DIM(g_lc1100ht_sink_sacle0_val) - 1]) {
		for (i = 0; i < LC1100HT_DIM(g_lc1100ht_sink_sacle0_val) - 1; i++) {
			if (ma < g_lc1100ht_sink_sacle0_val[i + 1])
				break;
		}
		scale = 0;
		val = i;
	} else {
		for (i = 0; i < LC1100HT_DIM(g_lc1100ht_sink_sacle1_val) - 1; i++) {
			if (ma < g_lc1100ht_sink_sacle1_val[i + 1])
				break;
		}
		scale = 1;
		val = i;
	}

	if (scale)
		lc1100ht_reg_bit_set(LC1100HT_REG_SINK_CTL, 1 << (id + 3));
	else
		lc1100ht_reg_bit_clr(LC1100HT_REG_SINK_CTL, 1 << (id + 3));

	reg = LC1100HT_REG_SINK_OUT_0 + (id / 2);
	mask = 0xf << ((id % 2) * 4);
	lc1100ht_reg_bit_write(reg, mask, val);
}

static void lc1100ht_backlight_enable(u8 operation)
{
	if (operation) {
		lc1100ht_reg_bit_set(LC1100HT_REG_BACKLIGHT_CTL, 0x01);
		lc1100ht_reg_write(LC1100HT_REG_BACKLIGHT_BRIGHTNESS, 0x7f);
	} else {
		lc1100ht_reg_bit_clr(LC1100HT_REG_BACKLIGHT_CTL, 0x01);
		lc1100ht_reg_write(LC1100HT_REG_BACKLIGHT_BRIGHTNESS, 0x00);
	}
}

/* Event. */
static int lc1100ht_event_mask(int event)
{
	int mask;

	switch (event) {
		case PMIC_EVENT_POWER_KEY:
			mask = LC1100HT_IRQ_POWER_KEY_MASK;
			break;
		case PMIC_EVENT_BATTERY:
			mask = LC1100HT_IRQ_BATTERY_MASK;
			break;
		case PMIC_EVENT_RTC1:
			mask = LC1100HT_IRQ_RTC1_MASK;
			break;
		case PMIC_EVENT_RTC2:
			mask = LC1100HT_IRQ_RTC2_MASK;
			break;
		case PMIC_EVENT_COMP1:
			mask = LC1100HT_IRQ_COMP1_MASK;
			lc1100ht_reg_bit_clr(LC1100HT_REG_COMP_1, LC1100HT_REG_BITMASK_COMP1_EN);
			break;
		case PMIC_EVENT_COMP2:
			mask = LC1100HT_IRQ_COMP2_MASK;
			lc1100ht_reg_bit_clr(LC1100HT_REG_COMP_2, LC1100HT_REG_BITMASK_COMP2_EN);
			break;
		default:
			return -EINVAL;
	}

	lc1100ht_int_mask(mask);

	return 0;
}

static int lc1100ht_event_unmask(int event)
{
	int mask;

	switch (event) {
		case PMIC_EVENT_POWER_KEY:
			mask = LC1100HT_IRQ_POWER_KEY_MASK;
			break;
		case PMIC_EVENT_BATTERY:
			mask = LC1100HT_IRQ_BATTERY_MASK;
			break;
		case PMIC_EVENT_RTC1:
			mask = LC1100HT_IRQ_RTC1_MASK;
			break;
		case PMIC_EVENT_RTC2:
			mask = LC1100HT_IRQ_RTC2_MASK;
			break;
		case PMIC_EVENT_COMP1:
			mask = LC1100HT_IRQ_COMP1_MASK;
			lc1100ht_reg_bit_set(LC1100HT_REG_COMP_1, LC1100HT_REG_BITMASK_COMP1_EN);
			break;
		case PMIC_EVENT_COMP2:
			mask = LC1100HT_IRQ_COMP2_MASK;
			lc1100ht_reg_bit_set(LC1100HT_REG_COMP_2, LC1100HT_REG_BITMASK_COMP2_EN);
			break;
		default:
			return -EINVAL;
	}

	lc1100ht_int_unmask(mask);

	return 0;
}

static struct pmic_power_ctrl_map* lc1100ht_power_ctrl_find(int power_id)
{
	struct pmic_power_ctrl_map *pctrl_map = NULL;
	u8 i;

	for (i = 0; i < LC1100HT_LDO_CTRL_MAP_NUM(); i++) {
		pctrl_map = LC1100HT_LDO_CTRL_MAP_ITEM(i);
		if (pctrl_map->power_id == power_id)
			break;
	}

	if (i == LC1100HT_LDO_CTRL_MAP_NUM())
		return NULL;

	return pctrl_map;
}

/* Power. */
static int lc1100ht_power_set(int power_id, int mv, int force)
{
	u8 i;
	u8 find;
	u8 ctrl;
	u8 operation;
	struct pmic_power_ctrl_map *pctrl_map;
	struct pmic_power_module_map *pmodule_map;

	if (power_id >= PMIC_POWER_MAX)
		return -EINVAL;

	find = 0;
	operation = 0;
	for (i = 0; i < LC1100HT_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100HT_LDO_MODULE_MAP_ITEM(i);
		if (pmodule_map->power_id == power_id) {
			find = 1;
			if (pmodule_map->operation) {
				operation = 1;
				break;
			}
		}
	}

	/* Can't find the LDO. */
	if (!find)
		return -ENXIO;

	pctrl_map = lc1100ht_power_ctrl_find(power_id);
	if (!pctrl_map)
		return -ENODEV;

	if (PMIC_IS_DLDO(power_id) || PMIC_IS_ALDO(power_id)) {
		ctrl = pctrl_map->ctrl;

		/* Always on or off. */
		if ((ctrl == PMIC_POWER_CTRL_ALWAYS_ON) || (ctrl == PMIC_POWER_CTRL_ALWAYS_OFF))
			return 0;

		/* Set the voltage of this LDO. */
		if ((mv != PMIC_POWER_VOLTAGE_DISABLE)
			&& (mv != PMIC_POWER_VOLTAGE_ENABLE)
			&& (pctrl_map->default_mv != mv)) {
			lc1100ht_ldo_vout_set(power_id, mv);

			/* Save the voltage. */
			pctrl_map->default_mv = mv;
		}

		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of this LDO. */
			if (ctrl == PMIC_POWER_CTRL_REG) {
				lc1100ht_ldo_reg_enable(power_id, operation);
			} else if (ctrl == PMIC_POWER_CTRL_GPIO) {
				if (pctrl_map->gpio_id != PMIC_POWER_CTRL_GPIO_ID_NONE) {
					gpio_direction_output((unsigned)pctrl_map->gpio_id, operation);
				}
			}

			pctrl_map->state = operation;
		}

		/* Wait for statble LDO. */
		if (operation)
			mdelay(1);
	} else if (PMIC_IS_ISINK(power_id)) {
		/* Set the current of this ISINK. */
		if ((mv != PMIC_POWER_VOLTAGE_DISABLE)
			&& (mv != PMIC_POWER_VOLTAGE_ENABLE)
			&& (pctrl_map->default_mv != mv)) {
			lc1100ht_sink_iout_set(PMIC_ISINK_ID(power_id), mv);

			/* Save the current. */
			pctrl_map->default_mv = mv;
		}

		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of this ISINK. */
			lc1100ht_sink_enable(PMIC_ISINK_ID(power_id), operation);

			pctrl_map->state = operation;
		}
	} else if (PMIC_IS_EXTERNAL(power_id)) {
		/* Backlight. */
		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of backlight. */
			lc1100ht_backlight_enable(operation);

			pctrl_map->state = operation;
		}
	}

	return 0;
}

static int lc1100ht_buck2_voltage_tb[] = {
	800, 800, 850, 900, 950, 1000,
	1050, 1100, 1150, 1200, 1250,
	1300, 1350, 1400, 1450, 1500,
	1550, 1600, 1650, 1700, 1750,
	1800, 1850, 1900, 1950, 2000
};

static int lc1100ht_direct_voltage_get(u8 power_id, int param, int *pmv)
{
	u8 val;

	if (power_id == PMIC_DCDC2) {
		switch (param) {
		case 0:
			lc1100ht_reg_read(LC1100HT_REG_BUCK2_OUT_0, &val);
			if ((val > 0x19) || (val < 0x01))
				return -EINVAL;
			*pmv = lc1100ht_buck2_voltage_tb[val];
			break;
		case 1:
			lc1100ht_reg_read(LC1100HT_REG_BUCK2_OUT_1, &val);
			if ((val > 0x19) || (val < 0x01))
				return -EINVAL;
			*pmv = lc1100ht_buck2_voltage_tb[val];
			break;
		case 2:
			lc1100ht_reg_read(LC1100HT_REG_BUCK2_OUT_2, &val);
			if ((val > 0x19) || (val < 0x01))
				return -EINVAL;
			*pmv = lc1100ht_buck2_voltage_tb[val];
			break;
		case 3:
			lc1100ht_reg_read(LC1100HT_REG_BUCK2_OUT_3, &val);
			if ((val > 0x19) || (val < 0x01))
				return -EINVAL;
			*pmv = lc1100ht_buck2_voltage_tb[val];
			break;
		default:
			break;
		}
	}

	return 0;
}

static int lc1100ht_direct_voltage_set(u8 power_id, int param, int mv)
{
	int ret = -1;
	u8 i;

	if (power_id == PMIC_DCDC2) {
		for (i = 0; i < LC1100HT_DIM(lc1100ht_buck2_voltage_tb); i++) {
			if (lc1100ht_buck2_voltage_tb[i] == mv)
				break;
		}

		if (i == LC1100HT_DIM(lc1100ht_buck2_voltage_tb))
			return -EINVAL;

		switch (param) {
		case 0:
			ret = lc1100ht_reg_write(LC1100HT_REG_BUCK2_OUT_0, i);
			break;
		case 1:
			ret = lc1100ht_reg_write(LC1100HT_REG_BUCK2_OUT_1, i);
			break;
		case 2:
			ret = lc1100ht_reg_write(LC1100HT_REG_BUCK2_OUT_2, i);
			break;
		case 3:
			ret = lc1100ht_reg_write(LC1100HT_REG_BUCK2_OUT_3, i);
			break;
		default:
			break;
		}
	}

	return ret;
}

static int lc1100ht_voltage_get(u8 module, u8 param, int *pmv)
{
	int i;
	struct pmic_power_ctrl_map *pctrl_map;
	struct pmic_power_module_map *pmodule_map;
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);

	/* Find the LDO. */
	for (i = 0; i < LC1100HT_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100HT_LDO_MODULE_MAP_ITEM(i);
		if (PMIC_IS_DIRECT_POWER_ID(module)) {
			mutex_lock(&data->power_lock);
			lc1100ht_direct_voltage_get(PMIC_DIRECT_POWER_ID(module), param, pmv);
			mutex_unlock(&data->power_lock);
			break;
		}
		if ((pmodule_map->module == module) && (pmodule_map->param == param)) {
			pctrl_map = lc1100ht_power_ctrl_find(pmodule_map->power_id);
			if (!pctrl_map)
				return -ENODEV;

			*pmv = pmodule_map->operation ? pctrl_map->default_mv : 0;
			break;
		}
	}

	return 0;
}

static int lc1100ht_voltage_set(u8 module, u8 param, int mv)
{
	int i;
	int ret = -ENXIO;
	struct pmic_power_module_map *pmodule_map;
	struct lc1100ht_data *data = i2c_get_clientdata(lc1100ht_client);

	/* Find the LDO. */
	for (i = 0; i < LC1100HT_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100HT_LDO_MODULE_MAP_ITEM(i);
		if (PMIC_IS_DIRECT_POWER_ID(module)) {
			mutex_lock(&data->power_lock);
			ret = lc1100ht_direct_voltage_set(PMIC_DIRECT_POWER_ID(module), param, mv);
			mutex_unlock(&data->power_lock);
			break;
		}
		if ((pmodule_map->module == module) && (pmodule_map->param == param)) {
			mutex_lock(&data->power_lock);
			pmodule_map->operation = (mv == PMIC_POWER_VOLTAGE_DISABLE) ? 0 : 1;
			ret = lc1100ht_power_set(pmodule_map->power_id, mv, 0);
			mutex_unlock(&data->power_lock);
			break;
		}
	}

	return ret;
}

/* RTC. */
static int lc1100ht_rtc_time_get(struct rtc_time *tm)
{
	u8 val;

	/* Read hour, minute and second. */
	lc1100ht_reg_read(LC1100HT_REG_RTC_SECOND, &val);
	tm->tm_sec = val & LC1100HT_REG_BITMASK_RTC_SECOND;
	lc1100ht_reg_read(LC1100HT_REG_RTC_MINUTE, &val);
	tm->tm_min = val & LC1100HT_REG_BITMASK_RTC_MINUTE;
	lc1100ht_reg_read(LC1100HT_REG_RTC_HOUR, &val);
	tm->tm_hour = val & LC1100HT_REG_BITMASK_RTC_HOUR;

	/* Read day, month and year. */
	lc1100ht_reg_read(LC1100HT_REG_RTC_DAY, &val);
	tm->tm_mday = LC1100HT_RTC_REG2DAY((val & LC1100HT_REG_BITMASK_RTC_DAY));
	lc1100ht_reg_read(LC1100HT_REG_RTC_MONTH, &val);
	tm->tm_mon = LC1100HT_RTC_REG2MONTH((val & LC1100HT_REG_BITMASK_RTC_MONTH));
	lc1100ht_reg_read(LC1100HT_REG_RTC_YEAR, &val);
	tm->tm_year = LC1100HT_RTC_REG2YEAR((val & LC1100HT_REG_BITMASK_RTC_YEAR));

	LC1100HT_PRINT("lc1100ht_rtc_time_get:%d-%d-%d %d:%d:%d\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;
}

static int lc1100ht_rtc_time_set(struct rtc_time *tm)
{
	/* Check the year. */
	if(LC1100HT_RTC_YEAR_CHECK(tm->tm_year))
		return -EINVAL;

	/* Write hour, minute and second. */
	lc1100ht_reg_write(LC1100HT_REG_RTC_SECOND, 0);
	lc1100ht_reg_write(LC1100HT_REG_RTC_HOUR, ((u8)tm->tm_hour & LC1100HT_REG_BITMASK_RTC_HOUR));
	lc1100ht_reg_write(LC1100HT_REG_RTC_MINUTE, ((u8)tm->tm_min & LC1100HT_REG_BITMASK_RTC_MINUTE));
	lc1100ht_reg_write(LC1100HT_REG_RTC_SECOND, ((u8)tm->tm_sec & LC1100HT_REG_BITMASK_RTC_SECOND));

	/* Write day, month and year. */
	lc1100ht_reg_write(LC1100HT_REG_RTC_DAY, ((u8)LC1100HT_RTC_DAY2REG(tm->tm_mday) & LC1100HT_REG_BITMASK_RTC_DAY));
	lc1100ht_reg_write(LC1100HT_REG_RTC_MONTH, ((u8)LC1100HT_RTC_MONTH2REG(tm->tm_mon) & LC1100HT_REG_BITMASK_RTC_MONTH));
	lc1100ht_reg_write(LC1100HT_REG_RTC_YEAR, ((u8)LC1100HT_RTC_YEAR2REG(tm->tm_year) & LC1100HT_REG_BITMASK_RTC_YEAR));

	/* Wait for stable rtc. */
	mdelay(5);

	return 0;
}

static void lc1100ht_rtc_alarm_enable(u8 id)
{
	u8 reg;
	u8 val;

	if (id >= LC1100HT_RTC_ALARM_NUM)
		return;

	if(id == 0) {
		reg = LC1100HT_REG_ALARM1_WEEK;
		val = LC1100HT_REG_BITMASK_ALARM1_ACTIVATED;
	} else {
		reg = LC1100HT_REG_ALARM2_WEEK;
		val = LC1100HT_REG_BITMASK_ALARM2_ACTIVATED;
	}

	lc1100ht_reg_bit_set(reg, val);
}

static void lc1100ht_rtc_alarm_disable(u8 id)
{
	if (id >= LC1100HT_RTC_ALARM_NUM)
		return;

	if(id == 0) {
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_SECOND, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_MINUTE, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_HOUR, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_DAY, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_MONTH, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_YEAR, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_WEEK, 0);
	} else {
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_SECOND, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_MINUTE, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_HOUR, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_DAY, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_MONTH, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_YEAR, 0);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_WEEK, 0);
	}
}

static int lc1100ht_rtc_alarm_get(u8 id, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alrm_time = &(alrm->time);
	u8 val;

	if (id >= LC1100HT_RTC_ALARM_NUM)
		return -EINVAL;

	if(id == 0) {
		/* Read the alarm time of RTC. */
		lc1100ht_reg_read(LC1100HT_REG_ALARM1_SECOND, &val);
		alrm_time->tm_sec = val & LC1100HT_REG_BITMASK_ALARM1_SECOND;
		lc1100ht_reg_read(LC1100HT_REG_ALARM1_MINUTE, &val);
		alrm_time->tm_min = val & LC1100HT_REG_BITMASK_ALARM1_MINUTE;
		lc1100ht_reg_read(LC1100HT_REG_ALARM1_HOUR, &val);
		alrm_time->tm_hour = val & LC1100HT_REG_BITMASK_ALARM1_HOUR;
		lc1100ht_reg_read(LC1100HT_REG_ALARM1_DAY, &val);
		alrm_time->tm_mday = LC1100HT_RTC_REG2DAY(val & LC1100HT_REG_BITMASK_ALARM1_DAY);
		lc1100ht_reg_read(LC1100HT_REG_ALARM1_MONTH, &val);
		alrm_time->tm_mon = LC1100HT_RTC_REG2MONTH(val & LC1100HT_REG_BITMASK_ALARM1_MONTH);
		lc1100ht_reg_read(LC1100HT_REG_ALARM1_YEAR, &val);
		alrm_time->tm_year = LC1100HT_RTC_REG2YEAR(val & LC1100HT_REG_BITMASK_ALARM1_YEAR);

		/* Get enabled state. */
		lc1100ht_reg_read(LC1100HT_REG_ALARM1_WEEK, &val);
		alrm->enabled = !!(val & LC1100HT_REG_BITMASK_ALARM1_ACTIVATED);

		/* Get pending state. */
		lc1100ht_reg_read(LC1100HT_REG_INT_STATE_2, &val);
		alrm->pending = !!(val & LC1100HT_REG_BITMASK_RTC_ALARM1_INT);
	} else {
		/* Read the alarm time of RTC. */
		lc1100ht_reg_read(LC1100HT_REG_ALARM2_SECOND, &val);
		if (!(val & LC1100HT_REG_BITMASK_ALARM2_IGNORE_SECOND))
			alrm_time->tm_sec = val & LC1100HT_REG_BITMASK_ALARM2_SECOND;
		else
			alrm_time->tm_sec = 0;
		lc1100ht_reg_read(LC1100HT_REG_ALARM2_MINUTE, &val);
		alrm_time->tm_min = val & LC1100HT_REG_BITMASK_ALARM2_MINUTE;
		lc1100ht_reg_read(LC1100HT_REG_ALARM2_HOUR, &val);
		alrm_time->tm_hour = val & LC1100HT_REG_BITMASK_ALARM2_HOUR;
		lc1100ht_reg_read(LC1100HT_REG_ALARM2_DAY, &val);
		alrm_time->tm_mday = LC1100HT_RTC_REG2DAY(val & LC1100HT_REG_BITMASK_ALARM2_DAY);
		lc1100ht_reg_read(LC1100HT_REG_ALARM2_MONTH, &val);
		alrm_time->tm_mon = LC1100HT_RTC_REG2MONTH(val & LC1100HT_REG_BITMASK_ALARM2_MONTH);
		lc1100ht_reg_read(LC1100HT_REG_ALARM2_YEAR, &val);
		alrm_time->tm_year = LC1100HT_RTC_REG2YEAR(val & LC1100HT_REG_BITMASK_ALARM2_YEAR);

		/* Get enabled state. */
		lc1100ht_reg_read(LC1100HT_REG_ALARM2_WEEK, &val);
		alrm->enabled = !!(val & LC1100HT_REG_BITMASK_ALARM2_ACTIVATED);

		/* Get pending state. */
		lc1100ht_reg_read(LC1100HT_REG_INT_STATE_2, &val);
		alrm->pending = !!(val & LC1100HT_REG_BITMASK_RTC_ALARM2_INT);
	}

	return 0;
}

static int lc1100ht_rtc_alarm_set(u8 id, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alrm_time = &(alrm->time);
	u8 reg;

	if (id >= LC1100HT_RTC_ALARM_NUM)
		return -EINVAL;

	if (!alrm->enabled) {
		lc1100ht_rtc_alarm_disable(id);
		return 0;
	}

	/* Check the year. */
	if(LC1100HT_RTC_YEAR_CHECK(alrm_time->tm_year))
		return -EINVAL;

	/* Write the alarm time of RTC. */
	if(id == 0) {
		reg = LC1100HT_REG_ALARM1_SECOND;
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_MINUTE, (u8)alrm_time->tm_min & LC1100HT_REG_BITMASK_ALARM1_MINUTE);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_HOUR, (u8)alrm_time->tm_hour & LC1100HT_REG_BITMASK_ALARM1_HOUR);
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_DAY, (u8)(LC1100HT_RTC_DAY2REG(alrm_time->tm_mday) & LC1100HT_REG_BITMASK_ALARM1_DAY));
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_MONTH, (u8)(LC1100HT_RTC_MONTH2REG(alrm_time->tm_mon) & LC1100HT_REG_BITMASK_ALARM1_MONTH));
		lc1100ht_reg_write(LC1100HT_REG_ALARM1_YEAR, (u8)LC1100HT_RTC_YEAR2REG(alrm_time->tm_year) & LC1100HT_REG_BITMASK_ALARM1_YEAR);
	} else {
		reg = LC1100HT_REG_ALARM2_SECOND;
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_MINUTE, (u8)alrm_time->tm_min & LC1100HT_REG_BITMASK_ALARM2_MINUTE);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_HOUR, (u8)alrm_time->tm_hour & LC1100HT_REG_BITMASK_ALARM2_HOUR);
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_DAY, (u8)(LC1100HT_RTC_DAY2REG(alrm_time->tm_mday) & LC1100HT_REG_BITMASK_ALARM2_DAY));
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_MONTH, (u8)(LC1100HT_RTC_MONTH2REG(alrm_time->tm_mon) & LC1100HT_REG_BITMASK_ALARM2_MONTH));
		lc1100ht_reg_write(LC1100HT_REG_ALARM2_YEAR, (u8)LC1100HT_RTC_YEAR2REG(alrm_time->tm_year) & LC1100HT_REG_BITMASK_ALARM2_YEAR);
		lc1100ht_reg_bit_set(LC1100HT_REG_ALARM2_WEEK, LC1100HT_RTC_ALARM_WEEK_DAY_ALL);
	}

	/* Power-off alarm don't use rtc second register. */
	if (id == PMIC_RTC_ALARM_POWEROFF)
		lc1100ht_reg_write(reg, LC1100HT_RTC_ALARM_TIME_IGNORE);
	else
		lc1100ht_reg_write(reg, (u8)alrm_time->tm_sec & LC1100HT_RTC_ALARM_TIME_MASK);

	lc1100ht_rtc_alarm_enable(id);

	return 0;
}

/* Power key. */
static int lc1100ht_power_key_state_get(void)
{
	u8 val;

	/* Get general status. */
	lc1100ht_reg_read(LC1100HT_REG_STATE_0, &val);
	if (val & LC1100HT_REG_BITMASK_POWER_ON_INPUT)
		return PMIC_POWER_KEY_ON;

	return PMIC_POWER_KEY_OFF;
}

/* Comparator. */
static int lc1100ht_comp_state_get(u8 id)
{
	u8 bit;
	u8 val;

	lc1100ht_reg_read(LC1100HT_REG_STATE_0, &val);
	if (id == PMIC_COMP1)
		bit = LC1100HT_REG_BITMASK_COMP1_OUTPUT;
	else if (id == PMIC_COMP2)
		bit = LC1100HT_REG_BITMASK_COMP2_OUTPUT;
	else {
		LC1100HT_PRINT("Invalid comparator id %d\n", id);
		return 0;
	}

	return !!(val & (1 << bit));
}


/* Reboot type. */
static int lc1100ht_reboot_type_set(u8 type)
{
	u8 reg;

	if (type == REBOOT_RTC_ALARM)
		lc1100ht_rtc_alarm_disable(PMIC_RTC_ALARM_POWEROFF);

	reg = PMIC_RTC_ALARM_POWEROFF ? LC1100HT_REG_ALARM2_SECOND : LC1100HT_REG_ALARM1_SECOND;
	lc1100ht_reg_write(reg, type | LC1100HT_RTC_ALARM_TIME_IGNORE);

	return 0;
}

static struct pmic_ops lc1100ht_pmic_ops = {
	.name			= "lc1100ht",

	.reg_read		= lc1100ht_pmic_reg_read,
	.reg_write		= lc1100ht_pmic_reg_write,

	.event_mask		= lc1100ht_event_mask,
	.event_unmask		= lc1100ht_event_unmask,

	.voltage_get		= lc1100ht_voltage_get,
	.voltage_set		= lc1100ht_voltage_set,

	.rtc_time_get		= lc1100ht_rtc_time_get,
	.rtc_time_set		= lc1100ht_rtc_time_set,
	.rtc_alarm_get		= lc1100ht_rtc_alarm_get,
	.rtc_alarm_set		= lc1100ht_rtc_alarm_set,

	.reboot_type_set	= lc1100ht_reboot_type_set,
	.comp_state_get		= lc1100ht_comp_state_get,
	.power_key_state_get	= lc1100ht_power_key_state_get,
};

static irqreturn_t lc1100ht_irq_handle(int irq, void *dev_id)
{
	struct lc1100ht_data *data = (struct lc1100ht_data *)dev_id;

	disable_irq_nosync(data->irq);
	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}

static void lc1100ht_irq_work_handler(struct work_struct *work)
{
	int status;
	struct lc1100ht_data *data = container_of(work, struct lc1100ht_data, irq_work);

	status = lc1100ht_int_status_get();
	status &= ~data->irq_mask;

	/* Power key. */
	if (status & LC1100HT_IRQ_POWER_KEY_MASK)
		pmic_event_handle(PMIC_EVENT_POWER_KEY, &status);

	/* Battery. */
	if (status & LC1100HT_IRQ_BATTERY_MASK)
		pmic_event_handle(PMIC_EVENT_BATTERY, &status);

	/* RTC1. */
	if (status & LC1100HT_IRQ_RTC1_MASK)
		pmic_event_handle(PMIC_EVENT_RTC1, &status);

	/* RTC2. */
	if (status & LC1100HT_IRQ_RTC2_MASK)
		pmic_event_handle(PMIC_EVENT_RTC2, &status);

	/* Comparator1. */
	if (status & LC1100HT_IRQ_COMP1_MASK)
		pmic_event_handle(PMIC_EVENT_COMP1, &status);

	/* Comparator2. */
	if (status & LC1100HT_IRQ_COMP2_MASK)
		pmic_event_handle(PMIC_EVENT_COMP2, &status);

	enable_irq(data->irq);
}

static int lc1100ht_reboot_init(struct lc1100ht_data *data)
{
	u8 reg;
	u8 val;
	u8 startup = 0;
	u8 shutdown = 0;
	u8 reboot_user_flag = 0;
	unsigned int power_on_type = PU_REASON_PWR_KEY_PRESS;
	unsigned int reboot_reason = REBOOT_NONE;

	/* Read power on reson from pmic hardware */
	lc1100ht_reg_read(LC1100HT_REG_UP_STATE, &startup);
	lc1100ht_reg_read(LC1100HT_REG_DOWN_STATE, &shutdown);

	if (startup & LC1100HT_REG_BITMASK_RTC_ALARM1)
		power_on_type = PU_REASON_RTC_ALARM;
	else if (startup & LC1100HT_REG_BITMASK_RTC_ALARM2)
		power_on_type = PU_REASON_RTC_ALARM;
	else if (startup & LC1100HT_REG_BITMASK_RSTIN_N_STARTUP)
		power_on_type = PU_REASON_HARDWARE_RESET;
	else if (startup & LC1100HT_REG_BITMASK_CHARGER_STARTUP)
		power_on_type = PU_REASON_USB_CHARGER;
	else if (startup & LC1100HT_REG_BITMASK_PWR_ON_STARTUP)
		power_on_type = PU_REASON_PWR_KEY_PRESS;

	printk(KERN_INFO "LC1100HT: Raw startup 0x%02x, raw shutdown 0x%02x\n",
			startup, shutdown);

	/* Read shutdown reson from ourself setting && reset record info. */
	reg = PMIC_RTC_ALARM_POWEROFF ? LC1100HT_REG_ALARM2_SECOND : LC1100HT_REG_ALARM1_SECOND;
	lc1100ht_reg_read(reg, &val);
	reboot_reason = (val & (~LC1100HT_RTC_ALARM_TIME_IGNORE));
	if (reboot_reason) {
		lc1100ht_reg_write(reg, LC1100HT_RTC_ALARM_TIME_IGNORE);
		reboot_user_flag = (reboot_reason & REBOOT_USER_FLAG);
		reboot_reason &= REBOOT_REASON_MASK;
	}

	if (reboot_reason == REBOOT_RTC_ALARM)
		power_on_type = PU_REASON_RTC_ALARM;
	else if (reboot_reason == REBOOT_POWER_KEY)
		power_on_type = PU_REASON_PWR_KEY_PRESS;
	else if (reboot_reason == REBOOT_RECOVERY)
		power_on_type = PU_REASON_REBOOT_RECOVERY;
	else if (reboot_reason == REBOOT_FOTA)
		power_on_type = PU_REASON_REBOOT_FOTA;
	else if (reboot_reason == REBOOT_CRITICAL)
		power_on_type = PU_REASON_REBOOT_CRITICAL;
	else if (reboot_reason == REBOOT_UNKNOWN)
		power_on_type = PU_REASON_REBOOT_UNKNOWN;
	else if (reboot_reason == REBOOT_NORMAL)
		power_on_type = PU_REASON_REBOOT_NORMAL;

	if (reboot_reason == REBOOT_RTC_ALARM)
		lc1100ht_rtc_alarm_enable(PMIC_RTC_ALARM_POWEROFF);

	printk(KERN_INFO "LC1100HT: Power on reason(%s), reboot reason(%s%s)\n",
			get_boot_reason_string(power_on_type),
			get_reboot_reason_string(reboot_reason),
			reboot_user_flag ? "-user" : "");

	data->power_on_type = power_on_type;
	set_boot_reason(power_on_type);
	set_reboot_reason(reboot_reason);

	return 0;
}

static void lc1100ht_power_init(struct lc1100ht_data *data)
{
	int i;
	struct pmic_power_ctrl_map *pctrl_map;
	struct pmic_power_ctrl_map *pctrl_map_default;
	struct pmic_power_module_map *pmodule_map;

	/* Save the module map. */
	LC1100HT_LDO_MODULE_MAP() = data->pdata->module_map;
	LC1100HT_LDO_MODULE_MAP_NUM() = data->pdata->module_map_num;

	/* Set the control of LDO. */
	for (i = 0; i < data->pdata->ctrl_map_num; i++) {
		pctrl_map = &data->pdata->ctrl_map[i];
		pctrl_map_default = lc1100ht_power_ctrl_find(pctrl_map->power_id);
		if (!pctrl_map_default)
			continue;

		if (PMIC_IS_DLDO(pctrl_map->power_id) || PMIC_IS_ALDO(pctrl_map->power_id)) {
			if (pctrl_map_default->ctrl != pctrl_map->ctrl)
				lc1100ht_ldo_ctrl_set(pctrl_map->power_id, pctrl_map->ctrl);
			if (pctrl_map_default->default_mv != pctrl_map->default_mv)
				lc1100ht_ldo_vout_set(pctrl_map->power_id, pctrl_map->default_mv);
		} else if (PMIC_IS_ISINK(pctrl_map->power_id)) {
			if (pctrl_map_default->default_mv != pctrl_map->default_mv)
				lc1100ht_sink_iout_set(PMIC_ISINK_ID(pctrl_map->power_id), pctrl_map->default_mv);
		}
		memcpy(pctrl_map_default, pctrl_map, sizeof(struct pmic_power_ctrl_map));
	}

	/* Set the power of all modules. */
	for (i = 0; i < LC1100HT_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100HT_LDO_MODULE_MAP_ITEM(i);
		lc1100ht_power_set(pmodule_map->power_id, PMIC_POWER_VOLTAGE_DISABLE, 1);
	}

	/* Set the pull down of LDO. */
	lc1100ht_reg_write(LC1100HT_REG_LDO_POWERDOWN_EN_0, 0xff);
	lc1100ht_reg_write(LC1100HT_REG_LDO_POWERDOWN_EN_1, 0xff);
	lc1100ht_reg_write(LC1100HT_REG_LDO_POWERDOWN_EN_2, 0xff);
}

static int lc1100ht_irq_init(struct lc1100ht_data *data)
{
	int ret;

	INIT_WORK(&data->irq_work, lc1100ht_irq_work_handler);

	ret = gpio_request(data->irq_gpio, " LC1100HT irq");
	if (ret < 0) {
		printk(KERN_ERR "LC1100HT: Failed to request GPIO.\n");
		return ret;
	}

	ret = gpio_direction_input(data->irq_gpio);
	if (ret < 0) {
		printk(KERN_ERR "LC1100HT: Failed to detect GPIO input.\n");
		goto exit;
	}

	/* Request irq. */
	ret = request_irq(data->irq, lc1100ht_irq_handle,
			  IRQF_TRIGGER_LOW, "LC1100HT irq", data);
	if (ret) {
		printk(KERN_ERR "LC1100HT: IRQ already in use.\n");
		goto exit;
	}

	return 0;

exit:
	gpio_free(data->irq_gpio);
	return ret;
}

static int lc1100ht_rtc_init(struct lc1100ht_data *data)
{
	u8 mday;
	u8 month;

	lc1100ht_reg_read(LC1100HT_REG_RTC_DAY, &mday);
	lc1100ht_reg_read(LC1100HT_REG_RTC_MONTH, &month);
	if (!mday || !month) {
		/* Initialize the rtc time to 2000-01-01 00:00:00. */
		lc1100ht_reg_write(LC1100HT_REG_RTC_SECOND, 0);
		lc1100ht_reg_write(LC1100HT_REG_RTC_MINUTE, 0);
		lc1100ht_reg_write(LC1100HT_REG_RTC_HOUR, 0);
		lc1100ht_reg_write(LC1100HT_REG_RTC_DAY, 1);
		lc1100ht_reg_write(LC1100HT_REG_RTC_MONTH, 1);
		lc1100ht_reg_write(LC1100HT_REG_RTC_YEAR, 0);
	}

	/* Disable reboot rtc alarm. */
	lc1100ht_rtc_alarm_disable(PMIC_RTC_ALARM_REBOOT);

	/* Enable all days alarm */
	lc1100ht_reg_bit_set(LC1100HT_REG_ALARM1_WEEK, LC1100HT_RTC_ALARM_WEEK_DAY_ALL);
	lc1100ht_reg_bit_set(LC1100HT_REG_ALARM2_WEEK, LC1100HT_RTC_ALARM_WEEK_DAY_ALL);

	return 0;
}

static int lc1100ht_misc_init(struct lc1100ht_data *data)
{
	/* Set external crystal oscillator. */
	lc1100ht_reg_write(LC1100HT_REG_MISC_2, 0x81);

	/* LDO & BUCK low power in sleep. */
	lc1100ht_reg_bit_set(LC1100HT_REG_BUCK_LDO_EN_3,
		LC1100HT_REG_BITMASK_BUCK_LOW_PWR_IN_SLEEP | LC1100HT_REG_BITMASK_LDO_LOW_PWR_IN_SLEEP);

	/* Init Backlight. */
	lc1100ht_reg_write(LC1100HT_REG_BACKLIGHT_SEQUENCE, 0x00);
	lc1100ht_reg_write(LC1100HT_REG_BACKLIGHT_CTL, 0x32);
	lc1100ht_reg_write(LC1100HT_REG_BACKLIGHT_BRIGHTNESS, 0x00);

	return 0;
}

static int lc1100ht_hw_init(struct lc1100ht_data *data)
{
	int i;
	int ret;

	/* Mask all interrupts. */
	lc1100ht_reg_write(LC1100HT_REG_INT_EN_1, 0x00);
	lc1100ht_reg_write(LC1100HT_REG_INT_EN_2, 0x00);
	lc1100ht_reg_write(LC1100HT_REG_INT_EN_3, 0x00);

	/* Initialize LC1100HT misc. */
	lc1100ht_misc_init(data);

	/* Platform init, */
	for (i = 0; i < data->pdata->init_regs_num; i++)
		lc1100ht_reg_bit_write((u8)data->pdata->init_regs[i].reg,
					(u8)data->pdata->init_regs[i].mask,
					(u8)data->pdata->init_regs[i].value);

	if (data->pdata->init)
		data->pdata->init();

	/* Read enable registers. */
	for (i = 0; i < LC1100HT_REG_ENABLE_NUM; i++)
		lc1100ht_reg_read(g_lc1100ht_enable_reg[i], &data->enable_reg[i]);


	/* Initialize LC1100HT reboot. */
	lc1100ht_reboot_init(data);

	/* Initialize LC1100HT RTC. */
	lc1100ht_rtc_init(data);

	/* Initialize LC1100HT power. */
	lc1100ht_power_init(data);

	/* Initialize LC1100HT irq. */
	ret = lc1100ht_irq_init(data);
	if (ret)
		goto exit;

	return 0;

exit:
	return ret;
}

static void lc1100ht_hw_exit(struct lc1100ht_data *data)
{
	/* Mask all interrupts. */
	lc1100ht_reg_write(LC1100HT_REG_INT_EN_1, 0x00);
	lc1100ht_reg_write(LC1100HT_REG_INT_EN_2, 0x00);
	lc1100ht_reg_write(LC1100HT_REG_INT_EN_3, 0x00);

	/* Free irq. */
	free_irq(data->irq, data);
	gpio_free(data->irq_gpio);
}

static int lc1100ht_check(void)
{
	int ret = 0;
	u8 val;

	ret = lc1100ht_reg_read(LC1100HT_REG_RTC_SECOND, &val);
	if (ret)
		return 0;

	return 1;
}

#ifdef CONFIG_COMIP_CALIBRATION
static int lc1100ht_calibration_notify(struct notifier_block *this,
					unsigned long code, void *param)
{
	calibration_get calib_get = param;
	unsigned int freq_32k;
	unsigned int rem;
	int ret;

	if (calib_get)
		return NOTIFY_DONE;

	ret = calib_get(3600, &freq_32k, &rem);
	if (!ret) {
		printk(KERN_INFO "LC1100HT: 32K clock rate %d, rem %d\n",
				freq_32k, rem);
	}

	return NOTIFY_DONE;
}

static struct notifier_block lc1100ht_calibration_notifier = {
	.notifier_call = lc1100ht_calibration_notify,
};
#endif

static struct platform_device lc1100ht_battery_device = {
	.name = "lc1100ht-battery",
	.id   = -1,
};

#ifdef CONFIG_PM
static int lc1100ht_suspend(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static int lc1100ht_resume(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static struct dev_pm_ops lc1100ht_pm_ops = {
	.suspend = lc1100ht_suspend,
	.resume = lc1100ht_resume,
};
#endif

static int __devinit lc1100ht_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct lc1100ht_platform_data *pdata;
	struct lc1100ht_data *data;
	int err = 0;

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		printk(KERN_ERR "LC1100HT: No platform data\n");
		return -EINVAL;
	}

	lc1100ht_client = client;
	if ((pdata->flags & LC1100HT_FLAGS_DEVICE_CHECK) && !lc1100ht_check()) {
		printk(KERN_ERR "LC1100HT: Can't find the device\n");
		return -ENODEV;
	}

	if (!(data = kzalloc(sizeof(struct lc1100ht_data), GFP_KERNEL)))
		return -ENOMEM;

	/* Init real i2c_client */
	data->pdata = pdata;
	data->irq_mask = 0xffffffff;
	data->irq_gpio = pdata->irq_gpio;
	data->irq = gpio_to_irq(pdata->irq_gpio);
	i2c_set_clientdata(client, data);
	mutex_init(&data->power_lock);
	mutex_init(&data->adc_lock);
	mutex_init(&data->reg_lock);

#ifdef CONFIG_COMIP_CALIBRATION
	register_calibration_notifier(&lc1100ht_calibration_notifier);
#endif

	/* Initialize LC1100HT hardware. */
	if ((err = lc1100ht_hw_init(data)) != 0)
		goto exit;

	/* Set PMIC ops. */
	pmic_ops_set(&lc1100ht_pmic_ops);

	if (!(pdata->flags & LC1100HT_FLAGS_NO_BATTERY_DEVICE)) {
		err = platform_device_register(&lc1100ht_battery_device);
		if (err) {
			printk(KERN_ERR "LC1100HT: Register battery device failed\n");
			goto exit_clean;
		}
	}

	irq_set_irq_wake(data->irq, 1);

	return 0;

exit_clean:
	pmic_ops_set(NULL);
exit:
	kfree(data);
	return err;
}

static int __devexit lc1100ht_remove(struct i2c_client *client)
{
	struct lc1100ht_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_COMIP_CALIBRATION
	unregister_calibration_notifier(&lc1100ht_calibration_notifier);
#endif

	if (!(data->pdata->flags & LC1100HT_FLAGS_NO_BATTERY_DEVICE))
		platform_device_unregister(&lc1100ht_battery_device);

	/* Clean PMIC ops. */
	pmic_ops_set(NULL);

	/* Deinitialize LC1100HT hardware. */
	if (data) {
		lc1100ht_hw_exit(data);
		kfree(data);
	}

	lc1100ht_client = NULL;

	return 0;
}

static const struct i2c_device_id lc1100ht_id[] = {
	{ "lc1100ht", 0 },
	{ },
};

static struct i2c_driver lc1100ht_driver = {
	.driver = {
		.name = "lc1100ht",
#ifdef CONFIG_PM
		.pm = &lc1100ht_pm_ops,
#endif
	},
	.probe		= lc1100ht_probe,
	.remove 	= __devexit_p(lc1100ht_remove),
	.id_table	= lc1100ht_id,
};

static int __init lc1100ht_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&lc1100ht_driver);
	if (ret) {
		printk(KERN_WARNING "lc1100ht: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit lc1100ht_exit(void)
{
	i2c_del_driver(&lc1100ht_driver);
}

subsys_initcall(lc1100ht_init);
module_exit(lc1100ht_exit);

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("LC1100HT driver");
MODULE_LICENSE("GPL");
