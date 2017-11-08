/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013 LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the lc1100h hardware plarform's i2c driver.
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
#include <plat/lc1100h.h>
#include <plat/bootinfo.h>

//#define LC1100H_DEBUG
#ifdef LC1100H_DEBUG
#define LC1100H_PRINT(fmt, args...) printk(KERN_ERR "[LC1100H]" fmt, ##args)
#else
#define LC1100H_PRINT(fmt, args...)
#endif

/* LC1100H IRQ mask. */
#define LC1100H_IRQ_POWER_KEY_MASK				(LC1100H_INT_KONIR | LC1100H_INT_KOFFIR)
#define LC1100H_IRQ_RTC1_MASK 					(LC1100H_INT_RTC_AL1IS)
#define LC1100H_IRQ_RTC2_MASK 					(LC1100H_INT_RTC_AL2IS)
#define LC1100H_IRQ_BATTERY_MASK 				(LC1100H_INT_RCHGIR \
								| LC1100H_INT_BATUVIR \
								| LC1100H_INT_CHGOVIR \
								| LC1100H_INT_CHGCPIR \
								| LC1100H_INT_RTIMIR)
#define LC1100H_IRQ_ADC_MASK					(LC1100H_INT_ADCCPIR)
#define LC1100H_IRQ_CHARGER_MASK      (LC1100H_INT_ADPINIR | LC1100H_INT_ADPOUTIR)


/* LDO ID macro. */
#define LC1100H_REG_ENABLE_NUM					(4)
#define LC1100H_ALDO_ID_INDEX(id) 				((id - PMIC_ALDO1) / 8)
#define LC1100H_ALDO_ID_OFFSET(id)				((id - PMIC_ALDO1) % 8)
#define LC1100H_DLDO_ID_INDEX(id) 				(((id - PMIC_DLDO1) / 8) + 2)
#define LC1100H_DLDO_ID_OFFSET(id)				((id - PMIC_DLDO1) % 8)

/* SINK macro. */
#define LC1100H_SINK_INDEX(ma)					((ma / 10) - 1)

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
 * Range of lc1100h rtc :
 *	second : 0~59
 *	minute : 0~59
 *	hour : 0~23
 *	mday : 0~30
 *	month : 0~11, since Jan
 *	week : 0~6, since Monday
 *	year : years from 2000
 **/
#define LC1100H_RTC_YEAR_CHECK(year) 		\
		((year < (LC1100H_RTC_YEAR_MIN - 1900)) || ((year) > (LC1100H_RTC_YEAR_MAX - 1900)))
#define LC1100H_RTC_YEAR2REG(year)			((year) + 1900 - LC1100H_RTC_YEAR_BASE)
#define LC1100H_RTC_REG2YEAR(reg)			((reg) + LC1100H_RTC_YEAR_BASE - 1900)
#define LC1100H_RTC_MONTH2REG(month)			((month))
#define LC1100H_RTC_REG2MONTH(reg)			((reg))
#define LC1100H_RTC_DAY2REG(day)			((day) - 1)
#define LC1100H_RTC_REG2DAY(reg)			((reg) + 1)

/* Macro*/
#define LC1100H_DIM(x)					ARRAY_SIZE(x)
#define LC1100H_ARRAY(x)				(int *)&x, ARRAY_SIZE(x)

/* Each client has this additional data */
struct lc1100h_data {
	struct mutex reg_lock;
	struct mutex adc_lock;
	struct mutex power_lock;
	struct work_struct irq_work;
	struct lc1100h_platform_data *pdata;
	int irq;
	int irq_gpio;
	int irq_mask;
	int power_on_type;
	u8 version_id;
	u8 enable_reg[LC1100H_REG_ENABLE_NUM];
};

const unsigned int g_lc1100h_enable_reg[LC1100H_REG_ENABLE_NUM] = {
			LC1100H_REG_LDOAEN1, LC1100H_REG_LDOAEN2,
			LC1100H_REG_LDODEN1, LC1100H_REG_LDODEN2
};

const u8 g_lc1100h_sink_val[] = {1, 2, 4, 8, 9, 10, 12, 13, 14, 15};

const int g_lc1100h_1800_3000_13_vout[] = {
			1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500,
			2600, 2700, 2850, 2900, 3000
};

const int g_lc1100h_1200_1800_7_vout[] = {
			1200, 1300, 1400, 1500, 1600, 1700, 1800
};

const int g_lc1100h_1800_3000_3_vout[] = {1800, 2850, 3000};

const int g_lc1100h_1800_3000_2_vout[] = {1800, 3000};

const int g_lc1100h_1100_1500_9_vout[] = {
			1100, 1150, 1200, 1250, 1300, 1350, 1400, 1450, 1500
};

#define LC1100H_LDO_VOUT_MAP_ITEM(index)			&g_lc1100h_ldo_vout_map[index]
#define LC1100H_LDO_VOUT_MAP_NUM()				LC1100H_DIM(g_lc1100h_ldo_vout_map)
static struct lc1100h_ldo_vout_map g_lc1100h_ldo_vout_map[] = {
	{PMIC_ALDO6,	LC1100H_ARRAY(g_lc1100h_1800_3000_13_vout),	LC1100H_REG_LDOA6OVS, 	0x0f},
	{PMIC_ALDO7,	LC1100H_ARRAY(g_lc1100h_1200_1800_7_vout),	LC1100H_REG_LDOA7OVS, 	0x07},
	{PMIC_DLDO1, 	LC1100H_ARRAY(g_lc1100h_1800_3000_3_vout),	LC1100H_REG_LDODOVSEL, 	0x03},
	{PMIC_DLDO2, 	LC1100H_ARRAY(g_lc1100h_1800_3000_3_vout),	LC1100H_REG_LDODOVSEL, 	0x0c},
	{PMIC_DLDO3, 	LC1100H_ARRAY(g_lc1100h_1800_3000_13_vout),	LC1100H_REG_LDOD3OVS, 	0x0f},
	{PMIC_DLDO4, 	LC1100H_ARRAY(g_lc1100h_1800_3000_2_vout),	LC1100H_REG_LDODOVSEL, 	0x10},
	{PMIC_DLDO5, 	LC1100H_ARRAY(g_lc1100h_1800_3000_2_vout),	LC1100H_REG_LDODOVSEL, 	0x20},
	{PMIC_DLDO7, 	LC1100H_ARRAY(g_lc1100h_1800_3000_13_vout),	LC1100H_REG_LDOD7OVS, 	0x0f},
	{PMIC_DLDO8, 	LC1100H_ARRAY(g_lc1100h_1800_3000_2_vout),	LC1100H_REG_LDODOVSEL, 	0x80},
	{PMIC_DLDO9, 	LC1100H_ARRAY(g_lc1100h_1800_3000_13_vout),	LC1100H_REG_LDOD9OVS, 	0x0f},
	{PMIC_DLDO10,	LC1100H_ARRAY(g_lc1100h_1100_1500_9_vout),	LC1100H_REG_LDOD10OVS, 	0x0f},
	{PMIC_DLDO11,	LC1100H_ARRAY(g_lc1100h_1100_1500_9_vout),	LC1100H_REG_LDOD11OVS, 	0x0f},
};

#define LC1100H_LDO_CTRL_MAP_ITEM(index) 			&g_lc1100h_ldo_ctrl_map_default[index]
#define LC1100H_LDO_CTRL_MAP_NUM()				LC1100H_DIM(g_lc1100h_ldo_ctrl_map_default)
static struct pmic_power_ctrl_map g_lc1100h_ldo_ctrl_map_default[] = {
	{PMIC_DCDC1,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC2,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC3,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC4,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_ALDO1,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO2,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO3,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO5,	PMIC_POWER_CTRL_GPIO,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO6,	PMIC_POWER_CTRL_REG,		PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
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
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	10},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	10},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,		PMIC_POWER_CTRL_GPIO_ID_NONE,	10},
};

#define LC1100H_LDO_MODULE_MAP() 				g_lc1100h_ldo_module_map
#define LC1100H_LDO_MODULE_MAP_ITEM(index)			&g_lc1100h_ldo_module_map[index]
#define LC1100H_LDO_MODULE_MAP_NUM() 				g_lc1100h_ldo_module_map_num
static struct pmic_power_module_map* g_lc1100h_ldo_module_map;
static int g_lc1100h_ldo_module_map_num;

static struct i2c_client *lc1100h_client;

int lc1100h_reg_read(u8 reg, u8 *value)
{
	int ret;

	if (!lc1100h_client)
		return -1;

	ret = i2c_smbus_read_byte_data(lc1100h_client, reg);
	if(ret < 0)
		return ret;

	*value = (u8)ret;

	return 0;
}
EXPORT_SYMBOL(lc1100h_reg_read);

int lc1100h_reg_write(u8 reg, u8 value)
{
	int ret;

	if (!lc1100h_client)
		return -1;

	ret = i2c_smbus_write_byte_data(lc1100h_client, reg, value);
	if(ret > 0)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(lc1100h_reg_write);

int lc1100h_reg_bit_write(u8 reg, u8 mask, u8 value)
{
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);
	u8 valo, valn;
	int ret;

	if (!mask)
		return -EINVAL;

	mutex_lock(&data->reg_lock);

	ret = lc1100h_reg_read(reg, &valo);
	if (!ret) {
		valn = valo & ~mask;
		valn |= (value << (ffs(mask) - 1));
		ret = lc1100h_reg_write(reg, valn);
	}

	mutex_unlock(&data->reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100h_reg_bit_write);

int lc1100h_reg_bit_set(u8 reg, u8 value)
{
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);
	u8 val;
	int ret;

	mutex_lock(&data->reg_lock);

	ret = lc1100h_reg_read(reg, &val);
	if (!ret) {
		val |= value;
		ret = lc1100h_reg_write(reg, val);
	}

	mutex_unlock(&data->reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100h_reg_bit_set);

int lc1100h_reg_bit_clr(u8 reg, u8 value)
{
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);
	u8 val;
	int ret;

	mutex_lock(&data->reg_lock);

	ret = lc1100h_reg_read(reg, &val);
	if (!ret) {
		val &= ~value;
		ret = lc1100h_reg_write(reg, val);
	}

	mutex_unlock(&data->reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100h_reg_bit_clr);

int lc1100h_read_adc(u8 val, u16 *out)
{
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);
	u8 data0, data1;
	u8 ad_state, timeout;
	int ret = 0;

	if (in_atomic() || irqs_disabled()) {
		if (!mutex_trylock(&data->adc_lock))
			/* ADC activity is ongoing. */
			return -EAGAIN;
	} else {
		mutex_lock(&data->adc_lock);
	}

	/* Enable ADC. */
	lc1100h_reg_write(LC1100H_REG_ADCLDOEN, 0x01);
	lc1100h_reg_write(LC1100H_REG_ADCEN, 0x01);

	/* Set ADC channle and mode. */
	lc1100h_reg_write(LC1100H_REG_ADCCR, (u8)val);

	lc1100h_reg_write(LC1100H_REG_ADCFORMAT, 0x00); /* 12bit conver */

	/* Start ADC */
	lc1100h_reg_write(LC1100H_REG_ADCCMD, 0x01);

	timeout = 3;
	ad_state = 0;
	do {
		udelay(50);
		timeout--;
		lc1100h_reg_read(LC1100H_REG_ADCCMD, &ad_state);
		ad_state = !!(ad_state & LC1100H_REG_BITMASK_ADEND);
	} while(ad_state != 1 && timeout != 0);

	if (timeout == 0) {
		LC1100H_PRINT("Adc convert failed\n");
		data0 = data1 = 0xff; /* Indicate ADC convert failed */
		ret = -ETIMEDOUT; /* Indicate ADC convert failed */
	} else {
		lc1100h_reg_read(LC1100H_REG_ADCDAT0, &data0); /* Low  4 bit */
		lc1100h_reg_read(LC1100H_REG_ADCDAT1, &data1); /* High 8 bit */
	}

	*out = ((u16)data1 << 4) | ((u16)data0);

	/* Disable ADC. */
	lc1100h_reg_write(LC1100H_REG_ADCLDOEN, 0x00);

	mutex_unlock(&data->adc_lock);

	return ret;
}
EXPORT_SYMBOL(lc1100h_read_adc);

int lc1100h_int_mask(int mask)
{
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);
	u8 int_mask0;
	u8 int_mask1;
	u8 int_mask2;

	mutex_lock(&data->reg_lock);

	data->irq_mask |= mask;

	if (mask & 0x000000ff) {
		int_mask0 = (u8)(data->irq_mask & 0x000000ff);
		lc1100h_reg_write(LC1100H_REG_IRQMSK1, int_mask0);
		lc1100h_reg_write(LC1100H_REG_IRQEN1, ~int_mask0);
	}

	if ((mask >> 8) & 0x000000ff) {
		int_mask1 = (u8)((data->irq_mask >> 8) & 0x000000ff);
		lc1100h_reg_write(LC1100H_REG_IRQMSK2, int_mask1);
		lc1100h_reg_write(LC1100H_REG_IRQEN2, ~int_mask1);
	}

	if ((mask >> 16) & 0x000000ff) {
		int_mask2 = (u8)((data->irq_mask >> 16) & 0x000000ff);
		lc1100h_reg_write(LC1100H_REG_RTC_INT_EN, ~int_mask2);
	}

	mutex_unlock(&data->reg_lock);

	return 0;
}
EXPORT_SYMBOL(lc1100h_int_mask);

int lc1100h_int_unmask(int mask)
{
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);
	u8 int_mask0;
	u8 int_mask1;
	u8 int_mask2;

	mutex_lock(&data->reg_lock);

	data->irq_mask &= ~mask;

	if (mask & 0x000000ff) {
		int_mask0 = (u8)(data->irq_mask & 0x000000ff);
		lc1100h_reg_write(LC1100H_REG_IRQMSK1, int_mask0);
		lc1100h_reg_write(LC1100H_REG_IRQEN1, ~int_mask0);
	}

	if ((mask >> 8) & 0x000000ff) {
		int_mask1 = (u8)((data->irq_mask >> 8) & 0x000000ff);
		lc1100h_reg_write(LC1100H_REG_IRQMSK2, int_mask1);
		lc1100h_reg_write(LC1100H_REG_IRQEN2, ~int_mask1);
	}

	if ((mask >> 16) & 0x000000ff) {
		int_mask2 = (u8)((data->irq_mask >> 16) & 0x000000ff);
		lc1100h_reg_write(LC1100H_REG_RTC_INT_EN, ~int_mask2);
	}

	mutex_unlock(&data->reg_lock);

	return 0;
}
EXPORT_SYMBOL(lc1100h_int_unmask);

int lc1100h_int_status_get(void)
{
	u8 status0 = 0;
	u8 status1 = 0;
	u8 status2 = 0;

	lc1100h_reg_read(LC1100H_REG_IRQST1, &status0);
	if (status0)
		lc1100h_reg_write(LC1100H_REG_IRQST1, status0);

	lc1100h_reg_read(LC1100H_REG_IRQST2, &status1);
	if (status1)
		lc1100h_reg_write(LC1100H_REG_IRQST2, status1);

	lc1100h_reg_read(LC1100H_REG_RTC_INT_STATUS, &status2);
	if (status2)
		lc1100h_reg_write(LC1100H_REG_RTC_INT_STATUS, status2);

	return (((int)status2 << 16) | ((int)status1 << 8) | (int)status0);
}
EXPORT_SYMBOL(lc1100h_int_status_get);


static int lc1100h_pmic_reg_read(u16 reg, u16 *value)
{
	u8 val;
	int ret;

	ret = lc1100h_reg_read((u8)reg, &val);
	if(ret < 0)
		return ret;

	*value = (u16)val;

	return ret;
}

static int lc1100h_pmic_reg_write(u16 reg, u16 value)
{
	return lc1100h_reg_write((u8)reg, (u8)value);
}

static void lc1100h_ldo_ctrl_set(u8 id, u8 ctrl)
{
	unsigned int reg;
	u8 bit;

	if ((ctrl != PMIC_POWER_CTRL_REG) && (ctrl != PMIC_POWER_CTRL_GPIO)) {
		LC1100H_PRINT("LDO control, invalid control id\n");
		return;
	}

	if (PMIC_IS_ALDO(id)) {
		reg = LC1100H_REG_LDOAONEN;
		if (id == PMIC_ALDO1)
				bit = 0;
		else if (id == PMIC_ALDO2)
				bit = 1;
		else if (id == PMIC_ALDO3)
				bit = 2;
		else if (id == PMIC_ALDO4)
				bit = 3;
		else if (id == PMIC_ALDO5)
			bit = 4;
		else if (id == PMIC_ALDO7)
			bit = 5;
		else {
			LC1100H_PRINT("LDO control, invalid LDO id %d\n", id);
			return;
		}
	} else if (PMIC_IS_DLDO(id)) {
		reg = LC1100H_REG_LDODONEN;
		if (id == PMIC_DLDO7)
			bit = 0;
		else if (id == PMIC_DLDO9)
			bit = 1;
		else if (id == PMIC_DLDO11)
			bit = 2;
		else {
			LC1100H_PRINT("LDO control, invalid LDO id %d\n", id);
			return;
		}
	} else {
		LC1100H_PRINT("LDO control, invalid LDO id %d\n", id);
		return;
	}

	if (ctrl == PMIC_POWER_CTRL_REG)
		lc1100h_reg_bit_clr(reg, (1 << bit));
	else
		lc1100h_reg_bit_set(reg, (1 << bit));
}

static void lc1100h_ldo_reg_enable(u8 id, u8 operation)
{
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);
	u8 index = LC1100H_REG_ENABLE_NUM;
	u8 offset;
	u8 op;

	if (PMIC_IS_ALDO(id)) {
		index = LC1100H_ALDO_ID_INDEX(id);
		offset = LC1100H_ALDO_ID_OFFSET(id);
	} else if (PMIC_IS_DLDO(id)) {
		index = LC1100H_DLDO_ID_INDEX(id);
		offset = LC1100H_DLDO_ID_OFFSET(id);
	} else {
		LC1100H_PRINT("LDO enable, invalid LDO id\n");
		return;
	}

	if (index < LC1100H_REG_ENABLE_NUM) {
		mutex_lock(&data->reg_lock);

		op = (data->enable_reg[index] & (1 << offset)) ? 1 : 0;
		if (op != operation) {
			if (operation)
				data->enable_reg[index] |= (1 << offset);
			else
				data->enable_reg[index] &= ~(1 << offset);

			lc1100h_reg_write(g_lc1100h_enable_reg[index], data->enable_reg[index]);
		}

		mutex_unlock(&data->reg_lock);
	}
}

static void lc1100h_ldo_vout_set(u8 id, int mv)
{
	struct lc1100h_ldo_vout_map *vout_map;
	u8 i;
	u8 j;

	for (i = 0; i < LC1100H_LDO_VOUT_MAP_NUM(); i++) {
		vout_map = LC1100H_LDO_VOUT_MAP_ITEM(i);
		if (vout_map->ldo_id == id)
			break;
	}

	if (i == LC1100H_LDO_VOUT_MAP_NUM()) {
		LC1100H_PRINT("LDO vout. can't find the ldo id %d\n", id);
		return;
	}

	for (j = 0; j < vout_map->vout_range_size; j++) {
		if (mv == vout_map->vout_range[j])
			break;
	}

	if (j == vout_map->vout_range_size) {
		LC1100H_PRINT("LDO(%d) vout. not support the voltage %dmV\n", id, mv);
		return;
	}

	lc1100h_reg_bit_write(vout_map->reg, vout_map->mask, j);
}

static void lc1100h_sink_enable(u8 id, u8 operation)
{
	if (operation)
		lc1100h_reg_bit_set(LC1100H_REG_SINKCR, (1 << id));
	else
		lc1100h_reg_bit_clr(LC1100H_REG_SINKCR, (1 << id));
}

static void lc1100h_sink_iout_set(u8 id, int ma)
{
	unsigned int reg;
	u8 mask;
	u8 val;
	u8 index;

	index = LC1100H_SINK_INDEX(ma);
	if (index >= LC1100H_DIM(g_lc1100h_sink_val)) {
		LC1100H_PRINT("LDO sink iout. not support the current %dmA\n", ma);
		return;
	}

	val = g_lc1100h_sink_val[index];
	reg = LC1100H_REG_LEDIOUT + (id / 2);
	mask = 0xf << ((id % 2) * 4);
	lc1100h_reg_bit_write(reg, mask, val);
}

/* Event. */
static int lc1100h_event_mask(int event)
{
	int mask;

	switch (event) {
		case PMIC_EVENT_POWER_KEY:
			mask = LC1100H_IRQ_POWER_KEY_MASK;
			break;
		case PMIC_EVENT_CHARGER:
			mask = LC1100H_IRQ_CHARGER_MASK;
			break;
		case PMIC_EVENT_BATTERY:
			mask = LC1100H_IRQ_BATTERY_MASK;
			break;
		case PMIC_EVENT_RTC1:
			mask = LC1100H_IRQ_RTC1_MASK;
			break;
		case PMIC_EVENT_RTC2:
			mask = LC1100H_IRQ_RTC2_MASK;
			break;
		case PMIC_EVENT_ADC:
			mask = LC1100H_IRQ_ADC_MASK;
			break;
		default:
			return -EINVAL;
	}

	lc1100h_int_mask(mask);

	return 0;
}

static int lc1100h_event_unmask(int event)
{
	int mask;

	switch (event) {
		case PMIC_EVENT_POWER_KEY:
			mask = LC1100H_IRQ_POWER_KEY_MASK;
			break;
		case PMIC_EVENT_CHARGER:
			mask = LC1100H_IRQ_CHARGER_MASK;
			break;
			break;
		case PMIC_EVENT_BATTERY:
			mask = LC1100H_IRQ_BATTERY_MASK;
			break;
		case PMIC_EVENT_RTC1:
			mask = LC1100H_IRQ_RTC1_MASK;
			break;
		case PMIC_EVENT_RTC2:
			mask = LC1100H_IRQ_RTC2_MASK;
			break;
		case PMIC_EVENT_ADC:
			mask = LC1100H_IRQ_ADC_MASK;
			break;
		default:
			return -EINVAL;
	}

	lc1100h_int_unmask(mask);

	return 0;
}

static struct pmic_power_ctrl_map* lc1100h_power_ctrl_find(int power_id)
{
	struct pmic_power_ctrl_map *pctrl_map = NULL;
	u8 i;

	for (i = 0; i < LC1100H_LDO_CTRL_MAP_NUM(); i++) {
		pctrl_map = LC1100H_LDO_CTRL_MAP_ITEM(i);
		if (pctrl_map->power_id == power_id)
			break;
	}

	if (i == LC1100H_LDO_CTRL_MAP_NUM())
		return NULL;

	return pctrl_map;
}

/* Power. */
static int lc1100h_power_set(int power_id, int mv, int force)
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
	for (i = 0; i < LC1100H_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100H_LDO_MODULE_MAP_ITEM(i);
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

	pctrl_map = lc1100h_power_ctrl_find(power_id);
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
			lc1100h_ldo_vout_set(power_id, mv);

			/* Save the voltage. */
			pctrl_map->default_mv = mv;
		}

		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of this LDO. */
			if (ctrl == PMIC_POWER_CTRL_REG) {
				lc1100h_ldo_reg_enable(power_id, operation);
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
			lc1100h_sink_iout_set(PMIC_ISINK_ID(power_id), mv);

			/* Save the current. */
			pctrl_map->default_mv = mv;
		}

		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of this ISINK. */
			lc1100h_sink_enable(PMIC_ISINK_ID(power_id), operation);

			pctrl_map->state = operation;
		}
	}

	return 0;
}

static int lc1100h_buck2_voltage_tb[] = {
	900, 950, 1000,	1050, 1100, 1150, 1200, 1250,
	1300, 1800, 1210, 1220, 1230, 1240, 1130, 1180
};

static int lc1100h_direct_voltage_get(u8 power_id, int param, int *pmv)
{
	u8 val;

	if (power_id == PMIC_DCDC2) {
		switch (param) {
		case 0:
			lc1100h_reg_read(LC1100H_REG_DC2OVS0, &val);
			val = val & 0x0f;
			*pmv = lc1100h_buck2_voltage_tb[val];
			break;
		case 1:
			lc1100h_reg_read(LC1100H_REG_DC2OVS0, &val);
			val = (val >> 4) & 0x0f;
			*pmv = lc1100h_buck2_voltage_tb[val];
			break;
		case 2:
			lc1100h_reg_read(LC1100H_REG_DC2OVS1, &val);
			val = val & 0x0f;
			*pmv = lc1100h_buck2_voltage_tb[val];
			break;
		case 3:
			lc1100h_reg_read(LC1100H_REG_DC2OVS1, &val);
			val = (val >> 4) & 0x0f;
			*pmv = lc1100h_buck2_voltage_tb[val];
			break;
		default:
			break;
		}
	}

	return 0;
}

static int lc1100h_direct_voltage_set(u8 power_id, int param, int mv)
{
	int ret = -1;
	u8 i;

	if (power_id == PMIC_DCDC2) {
		for (i = 0; i < LC1100H_DIM(lc1100h_buck2_voltage_tb); i++) {
			if (lc1100h_buck2_voltage_tb[i] == mv)
				break;
		}

		if (i == LC1100H_DIM(lc1100h_buck2_voltage_tb))
			return -EINVAL;

		switch (param) {
		case 0:
			ret = lc1100h_reg_bit_write(LC1100H_REG_DC2OVS0, LC1100H_REG_BITMASK_DC2OVS0VOUT0, i);
			break;
		case 1:
			ret = lc1100h_reg_bit_write(LC1100H_REG_DC2OVS0, LC1100H_REG_BITMASK_DC2OVS0VOUT1, i);
			break;
		case 2:
			ret = lc1100h_reg_bit_write(LC1100H_REG_DC2OVS1, LC1100H_REG_BITMASK_DC2OVS1VOUT2, i);
			break;
		case 3:
			ret = lc1100h_reg_bit_write(LC1100H_REG_DC2OVS1, LC1100H_REG_BITMASK_DC2OVS1VOUT3, i);
			break;
		default:
			break;
		}
	}

	return ret;
}

static int lc1100h_voltage_get(u8 module, u8 param, int *pmv)
{
	int i;
	struct pmic_power_ctrl_map *pctrl_map;
	struct pmic_power_module_map *pmodule_map;
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);

	/* Find the LDO. */
	for (i = 0; i < LC1100H_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100H_LDO_MODULE_MAP_ITEM(i);
		if (PMIC_IS_DIRECT_POWER_ID(module)) {
			mutex_lock(&data->power_lock);
			lc1100h_direct_voltage_get(PMIC_DIRECT_POWER_ID(module), param, pmv);
			mutex_unlock(&data->power_lock);
			break;
		}
		if ((pmodule_map->module == module) && (pmodule_map->param == param)) {
			pctrl_map = lc1100h_power_ctrl_find(pmodule_map->power_id);
			if (!pctrl_map)
				return -ENODEV;

			*pmv = pmodule_map->operation ? pctrl_map->default_mv : 0;
			break;
		}
	}

	return 0;
}

static int lc1100h_voltage_set(u8 module, u8 param, int mv)
{
	int i;
	int ret = -ENXIO;
	struct pmic_power_module_map *pmodule_map;
	struct lc1100h_data *data = i2c_get_clientdata(lc1100h_client);

	/* Find the LDO. */
	for (i = 0; i < LC1100H_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100H_LDO_MODULE_MAP_ITEM(i);
		if (PMIC_IS_DIRECT_POWER_ID(module)) {
			mutex_lock(&data->power_lock);
			lc1100h_direct_voltage_set(PMIC_DIRECT_POWER_ID(module), param, mv);
			mutex_unlock(&data->power_lock);
			break;
		}
		if ((pmodule_map->module == module) && (pmodule_map->param == param)) {
			mutex_lock(&data->power_lock);
			pmodule_map->operation = (mv == PMIC_POWER_VOLTAGE_DISABLE) ? 0 : 1;
			ret = lc1100h_power_set(pmodule_map->power_id, mv, 0);
			mutex_unlock(&data->power_lock);
			break;
		}
	}

	return ret;
}

/* RTC. */
static int lc1100h_rtc_time_get(struct rtc_time *tm)
{
	u8 val;

	/* Read hour, minute and second. */
	lc1100h_reg_read(LC1100H_REG_RTC_SEC, &val);
	tm->tm_sec = val & LC1100H_REG_BITMASK_RTC_SEC;
	lc1100h_reg_read(LC1100H_REG_RTC_MIN, &val);
	tm->tm_min = val & LC1100H_REG_BITMASK_RTC_MIN;
	lc1100h_reg_read(LC1100H_REG_RTC_HOUR, &val);
	tm->tm_hour = val & LC1100H_REG_BITMASK_RTC_HOUR;

	/* Read day, month and year. */
	lc1100h_reg_read(LC1100H_REG_RTC_DAY, &val);
	tm->tm_mday = LC1100H_RTC_REG2DAY((val & LC1100H_REG_BITMASK_RTC_DAY));
	lc1100h_reg_read(LC1100H_REG_RTC_MONTH, &val);
	tm->tm_mon = LC1100H_RTC_REG2MONTH((val & LC1100H_REG_BITMASK_RTC_MONTH));
	lc1100h_reg_read(LC1100H_REG_RTC_YEAR, &val);
	tm->tm_year = LC1100H_RTC_REG2YEAR((val & LC1100H_REG_BITMASK_RTC_YEAR));

	LC1100H_PRINT("lc1100h_rtc_time_get:%d-%d-%d %d:%d:%d\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;
}

static int lc1100h_rtc_time_set(struct rtc_time *tm)
{
	/* Check the year. */
	if(LC1100H_RTC_YEAR_CHECK(tm->tm_year))
		return -EINVAL;

	/* Write hour, minute and second. */
	lc1100h_reg_write(LC1100H_REG_RTC_SEC, 0);
	lc1100h_reg_write(LC1100H_REG_RTC_HOUR, ((u8)tm->tm_hour & LC1100H_REG_BITMASK_RTC_HOUR));
	lc1100h_reg_write(LC1100H_REG_RTC_MIN, ((u8)tm->tm_min & LC1100H_REG_BITMASK_RTC_MIN));
	lc1100h_reg_write(LC1100H_REG_RTC_SEC, ((u8)tm->tm_sec & LC1100H_REG_BITMASK_RTC_SEC));

	/* Write day, month and year. */
	lc1100h_reg_write(LC1100H_REG_RTC_DAY, ((u8)LC1100H_RTC_DAY2REG(tm->tm_mday) & LC1100H_REG_BITMASK_RTC_DAY));
	lc1100h_reg_write(LC1100H_REG_RTC_MONTH, ((u8)LC1100H_RTC_MONTH2REG(tm->tm_mon) & LC1100H_REG_BITMASK_RTC_MONTH));
	lc1100h_reg_write(LC1100H_REG_RTC_YEAR, ((u8)LC1100H_RTC_YEAR2REG(tm->tm_year) & LC1100H_REG_BITMASK_RTC_YEAR));

	/* Write data to RTC */
	lc1100h_reg_write(LC1100H_REG_RTC_TIME_UPDATE, 0x1a);

	/* Enable RTC */
	lc1100h_reg_write(LC1100H_REG_RTC_CTRL, 1);

	return 0;
}

static void lc1100h_rtc_alarm_enable(u8 id)
{
	if (id >= LC1100H_RTC_ALARM_NUM)
		return;

	/* Update 32k CLK RTC alarm. */
	lc1100h_reg_write(LC1100H_REG_RTC_AL_UPDATE, 0x3a);

	if(id == 0)
		lc1100h_int_unmask(LC1100H_IRQ_RTC1_MASK);
	else
		lc1100h_int_unmask(LC1100H_IRQ_RTC2_MASK);
}

static void lc1100h_rtc_alarm_disable(u8 id)
{
	if (id >= LC1100H_RTC_ALARM_NUM)
		return;

	if(id == 0) {
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_SEC, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_MIN, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_HOUR, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_DAY, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_MONTH, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_YEAR, 0);
		lc1100h_int_mask(LC1100H_IRQ_RTC1_MASK);
	} else {
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_SEC, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_MIN, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_HOUR, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_DAY, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_MONTH, 0);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_YEAR, 0);
		lc1100h_int_mask(LC1100H_IRQ_RTC2_MASK);
	}
}

static int lc1100h_rtc_alarm_get(u8 id, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alrm_time = &(alrm->time);
	u8 val;

	if (id >= LC1100H_RTC_ALARM_NUM)
		return -EINVAL;

	if(id == 0) {
		/* Read the alarm time of RTC. */
		lc1100h_reg_read(LC1100H_REG_RTC_AL1_SEC, &val);
		if (!(val & LC1100H_RTC_ALARM_TIME_IGNORE))
			alrm_time->tm_sec = val & LC1100H_REG_BITMASK_RTC_AL1_SEC;
		else
			alrm_time->tm_sec = 0;
		lc1100h_reg_read(LC1100H_REG_RTC_AL1_MIN, &val);
		alrm_time->tm_min = val & LC1100H_REG_BITMASK_RTC_AL1_MIN;
		lc1100h_reg_read(LC1100H_REG_RTC_AL1_HOUR, &val);
		alrm_time->tm_hour = val & LC1100H_REG_BITMASK_RTC_AL1_HOUR;
		lc1100h_reg_read(LC1100H_REG_RTC_AL1_DAY, &val);
		alrm_time->tm_mday = LC1100H_RTC_REG2DAY(val & LC1100H_REG_BITMASK_RTC_AL1_DAY);
		lc1100h_reg_read(LC1100H_REG_RTC_AL1_MONTH, &val);
		alrm_time->tm_mon = LC1100H_RTC_REG2MONTH(val & LC1100H_REG_BITMASK_RTC_AL1_MONTH);
		lc1100h_reg_read(LC1100H_REG_RTC_AL1_YEAR, &val);
		alrm_time->tm_year = LC1100H_RTC_REG2YEAR(val & LC1100H_REG_BITMASK_RTC_AL1_YEAR);

		/* Get enabled state. */
		lc1100h_reg_read(LC1100H_REG_RTC_INT_EN, &val);
		alrm->enabled = !!(val & LC1100H_REG_BITMASK_RTC_AL1_EN);

		/* Get pending state. */
		lc1100h_reg_read(LC1100H_REG_RTC_INT_STATUS, &val);
		alrm->pending = !!(val & LC1100H_REG_BITMASK_RTC_AL1IS);
	} else {
		/* Read the alarm time of RTC. */
		lc1100h_reg_read(LC1100H_REG_RTC_AL2_SEC, &val);
		if (!(val & LC1100H_RTC_ALARM_TIME_IGNORE))
			alrm_time->tm_sec = val & LC1100H_REG_BITMASK_RTC_AL2_SEC;
		else
			alrm_time->tm_sec = 0;
		lc1100h_reg_read(LC1100H_REG_RTC_AL2_MIN, &val);
		alrm_time->tm_min = val & LC1100H_REG_BITMASK_RTC_AL2_MIN;
		lc1100h_reg_read(LC1100H_REG_RTC_AL2_HOUR, &val);
		alrm_time->tm_hour = val & LC1100H_REG_BITMASK_RTC_AL2_HOUR;
		lc1100h_reg_read(LC1100H_REG_RTC_AL2_DAY, &val);
		alrm_time->tm_mday = LC1100H_RTC_REG2DAY(val & LC1100H_REG_BITMASK_RTC_AL2_DAY);
		lc1100h_reg_read(LC1100H_REG_RTC_AL2_MONTH, &val);
		alrm_time->tm_mon = LC1100H_RTC_REG2MONTH(val & LC1100H_REG_BITMASK_RTC_AL2_MONTH);
		lc1100h_reg_read(LC1100H_REG_RTC_AL2_YEAR, &val);
		alrm_time->tm_year = LC1100H_RTC_REG2YEAR(val & LC1100H_REG_BITMASK_RTC_AL2_YEAR);

		/* Get enabled state. */
		lc1100h_reg_read(LC1100H_REG_RTC_INT_EN, &val);
		alrm->enabled = !!(val & LC1100H_REG_BITMASK_RTC_AL2_EN);

		/* Get pending state. */
		lc1100h_reg_read(LC1100H_REG_RTC_INT_STATUS, &val);
		alrm->pending = !!(val & LC1100H_REG_BITMASK_RTC_AL2IS);
	}

	return 0;
}

static int lc1100h_rtc_alarm_set(u8 id, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alrm_time = &(alrm->time);
	u8 reg;

	if (id >= LC1100H_RTC_ALARM_NUM)
		return -EINVAL;

	if (!alrm->enabled) {
		lc1100h_rtc_alarm_disable(id);
		return 0;
	}

	/* Check the year. */
	if(LC1100H_RTC_YEAR_CHECK(alrm_time->tm_year))
		return -EINVAL;

	/* Write the alarm time of RTC. */
	if(id == 0) {
		reg = LC1100H_REG_RTC_AL1_SEC;
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_MIN, (u8)alrm_time->tm_min & LC1100H_REG_BITMASK_RTC_AL1_MIN);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_HOUR, (u8)alrm_time->tm_hour & LC1100H_REG_BITMASK_RTC_AL1_HOUR);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_DAY, (u8)LC1100H_RTC_DAY2REG(alrm_time->tm_mday) & LC1100H_REG_BITMASK_RTC_AL1_DAY);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_MONTH, (u8)LC1100H_RTC_MONTH2REG(alrm_time->tm_mon) & LC1100H_REG_BITMASK_RTC_AL1_MONTH);
		lc1100h_reg_write(LC1100H_REG_RTC_AL1_YEAR, (u8)LC1100H_RTC_YEAR2REG(alrm_time->tm_year) & LC1100H_REG_BITMASK_RTC_AL1_YEAR);
	} else {
		reg = LC1100H_REG_RTC_AL2_SEC;
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_MIN, (u8)alrm_time->tm_min & LC1100H_REG_BITMASK_RTC_AL2_MIN);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_HOUR, (u8)alrm_time->tm_hour & LC1100H_REG_BITMASK_RTC_AL2_HOUR);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_DAY, (u8)LC1100H_RTC_DAY2REG(alrm_time->tm_mday) & LC1100H_REG_BITMASK_RTC_AL2_DAY);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_MONTH, (u8)LC1100H_RTC_MONTH2REG(alrm_time->tm_mon) & LC1100H_REG_BITMASK_RTC_AL2_MONTH);
		lc1100h_reg_write(LC1100H_REG_RTC_AL2_YEAR, (u8)LC1100H_RTC_YEAR2REG(alrm_time->tm_year) & LC1100H_REG_BITMASK_RTC_AL2_YEAR);
	}

	/* Power-off alarm don't use rtc second register. */
	if (id == PMIC_RTC_ALARM_POWEROFF)
		lc1100h_reg_write(reg, LC1100H_RTC_ALARM_TIME_IGNORE);
	else
		lc1100h_reg_write(reg, (u8)alrm_time->tm_sec & LC1100H_RTC_ALARM_TIME_MASK);

	lc1100h_rtc_alarm_enable(id);

	return 0;
}

/* Power key. */
static int lc1100h_power_key_state_get(void)
{
	u8 val;

	/* Get general status. */
	lc1100h_reg_read(LC1100H_REG_PCST, &val);
	if (val & LC1100H_REG_BITMASK_KONMON)
		return PMIC_POWER_KEY_ON;

	return PMIC_POWER_KEY_OFF;
}

/* pmic adapin  state. */
static int lc1100h_adapin_state_get(void)
{
	u8 val = 0;

	/* Get general status. */
	lc1100h_reg_read(LC1100H_REG_PCST, &val);
	if (val & LC1100H_REG_BITMASK_ADPIN){
		return !!(val & LC1100H_REG_BITMASK_ADPIN);
	}else{
		return 0;
	}
}
/* Reboot type. */
static int lc1100h_reboot_type_set(u8 type)
{
	u8 reg;

	if (type == REBOOT_RTC_ALARM)
		lc1100h_rtc_alarm_disable(PMIC_RTC_ALARM_POWEROFF);

	reg = PMIC_RTC_ALARM_POWEROFF ? LC1100H_REG_RTC_AL2_SEC : LC1100H_REG_RTC_AL1_SEC;
	lc1100h_reg_write(reg, type | LC1100H_RTC_ALARM_TIME_IGNORE);

	return 0;
}

static struct pmic_ops lc1100h_pmic_ops = {
	.name			= "lc1100h",

	.reg_read		= lc1100h_pmic_reg_read,
	.reg_write		= lc1100h_pmic_reg_write,

	.event_mask		= lc1100h_event_mask,
	.event_unmask		= lc1100h_event_unmask,

	.voltage_get		= lc1100h_voltage_get,
	.voltage_set		= lc1100h_voltage_set,

	.rtc_time_get		= lc1100h_rtc_time_get,
	.rtc_time_set		= lc1100h_rtc_time_set,
	.rtc_alarm_get		= lc1100h_rtc_alarm_get,
	.rtc_alarm_set		= lc1100h_rtc_alarm_set,

	.reboot_type_set	= lc1100h_reboot_type_set,
	.power_key_state_get	= lc1100h_power_key_state_get,
	.vchg_input_state_get	= lc1100h_adapin_state_get,
};

static irqreturn_t lc1100h_irq_handle(int irq, void *dev_id)
{
	struct lc1100h_data *data = (struct lc1100h_data *)dev_id;

	disable_irq_nosync(data->irq);
	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}

static void lc1100h_irq_work_handler(struct work_struct *work)
{
	int status = 0,power_key_status = 0;
	struct lc1100h_data *data = container_of(work, struct lc1100h_data, irq_work);

	status = lc1100h_int_status_get();
	status &= ~data->irq_mask;

	/* Power key. */
	if (status & LC1100H_IRQ_POWER_KEY_MASK){
		if ((status & LC1100H_IRQ_POWER_KEY_MASK) == LC1100h_INT_KONIR){
			power_key_status = IRQ_POWER_KEY_PRESS;
		}else if ((status & LC1100H_IRQ_POWER_KEY_MASK) == LC1100h_INT_KOFFIR){
			power_key_status = IRQ_POWER_KEY_RELEASE;
		}else{
			power_key_status = IRQ_POWER_KEY_PRESS_RELEASE;
		}
		pmic_event_handle(PMIC_EVENT_POWER_KEY, &power_key_status);
	}

	/* CHARGER */
	if (status & LC1100H_IRQ_CHARGER_MASK)
		pmic_event_handle(PMIC_EVENT_CHARGER, &status);

	/* Battery. */
	if (status & LC1100H_IRQ_BATTERY_MASK)
		pmic_event_handle(PMIC_EVENT_BATTERY, &status);

	/* ADC. */
	if (status & LC1100H_IRQ_ADC_MASK)
		pmic_event_handle(PMIC_EVENT_ADC, &status);

	/* RTC1. */
	if (status & LC1100H_IRQ_RTC1_MASK)
		pmic_event_handle(PMIC_EVENT_RTC1, &status);

	/* RTC2. */
	if (status & LC1100H_IRQ_RTC2_MASK)
		pmic_event_handle(PMIC_EVENT_RTC2, &status);

	enable_irq(data->irq);
}

static int lc1100h_reboot_init(struct lc1100h_data *data)
{
	u8 reg;
	u8 val;
	u8 startup = 0;
	u8 shutdown = 0;
	u8 reboot_user_flag = 0;
	unsigned int power_on_type = PU_REASON_PWR_KEY_PRESS;
	unsigned int reboot_reason = REBOOT_NONE;

	/* Read power on reson from pmic hardware */
	lc1100h_reg_read(LC1100H_REG_STARTUP_STATUS, &startup);
	lc1100h_reg_read(LC1100H_REG_SHDN_STATUS, &shutdown);

	if (startup & LC1100H_REG_BITMASK_ALARM_STARTUP)
		power_on_type = PU_REASON_RTC_ALARM;
	else if (startup & LC1100H_REG_BITMASK_ADPIN_STARTUP)
		power_on_type = PU_REASON_USB_CHARGER;
	else if (startup & LC1100H_REG_BITMASK_KEYON_STARTUP)
		power_on_type = PU_REASON_PWR_KEY_PRESS;

	printk(KERN_INFO "LC1100H: Raw startup 0x%02x, raw shutdown 0x%02x\n",
			startup, shutdown);

	/* Read shutdown reson from ourself setting && reset record info. */
	reg = PMIC_RTC_ALARM_POWEROFF ? LC1100H_REG_RTC_AL2_SEC : LC1100H_REG_RTC_AL1_SEC;
	lc1100h_reg_read(reg, &val);
	reboot_reason = (val & (~LC1100H_RTC_ALARM_TIME_IGNORE));
	if (reboot_reason) {
		lc1100h_reg_write(reg, LC1100H_RTC_ALARM_TIME_IGNORE);
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
		lc1100h_rtc_alarm_enable(PMIC_RTC_ALARM_POWEROFF);

	printk(KERN_INFO "LC1100H: Power on reason(%s), reboot reason(%s%s)\n",
			get_boot_reason_string(power_on_type),
			get_reboot_reason_string(reboot_reason),
			reboot_user_flag ? "-user" : "");

	data->power_on_type = power_on_type;
	set_boot_reason(power_on_type);
	set_reboot_reason(reboot_reason);

	return 0;
}

static void lc1100h_power_init(struct lc1100h_data *data)
{
	int i;
	struct pmic_power_ctrl_map *pctrl_map;
	struct pmic_power_ctrl_map *pctrl_map_default;
	struct pmic_power_module_map *pmodule_map;

	/* Save the module map. */
	LC1100H_LDO_MODULE_MAP() = data->pdata->module_map;
	LC1100H_LDO_MODULE_MAP_NUM() = data->pdata->module_map_num;

	/* Set the control of LDO. */
	for (i = 0; i < data->pdata->ctrl_map_num; i++) {
		pctrl_map = &data->pdata->ctrl_map[i];
		pctrl_map_default = lc1100h_power_ctrl_find(pctrl_map->power_id);
		if (!pctrl_map_default)
			continue;

		if (PMIC_IS_DLDO(pctrl_map->power_id) || PMIC_IS_ALDO(pctrl_map->power_id)) {
			if (pctrl_map_default->ctrl != pctrl_map->ctrl)
				lc1100h_ldo_ctrl_set(pctrl_map->power_id, pctrl_map->ctrl);
			if (pctrl_map_default->default_mv != pctrl_map->default_mv)
				lc1100h_ldo_vout_set(pctrl_map->power_id, pctrl_map->default_mv);
		} else if (PMIC_IS_ISINK(pctrl_map->power_id)) {
			if (pctrl_map_default->default_mv != pctrl_map->default_mv)
				lc1100h_sink_iout_set(PMIC_ISINK_ID(pctrl_map->power_id), pctrl_map->default_mv);
		}
		memcpy(pctrl_map_default, pctrl_map, sizeof(struct pmic_power_ctrl_map));
	}

	/* Set the power of all modules. */
	for (i = 0; i < LC1100H_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1100H_LDO_MODULE_MAP_ITEM(i);
		lc1100h_power_set(pmodule_map->power_id, PMIC_POWER_VOLTAGE_DISABLE, 1);
	}
}

static int lc1100h_irq_init(struct lc1100h_data *data)
{
	int ret;

	INIT_WORK(&data->irq_work, lc1100h_irq_work_handler);

	ret = gpio_request(data->irq_gpio, " LC1100H irq");
	if (ret < 0) {
		printk(KERN_ERR "LC1100H: Failed to request GPIO.\n");
		return ret;
	}

	ret = gpio_direction_input(data->irq_gpio);
	if (ret < 0) {
		printk(KERN_ERR "LC1100H: Failed to detect GPIO input.\n");
		goto exit;
	}

	/* Request irq. */
	ret = request_irq(data->irq, lc1100h_irq_handle,
			  IRQF_TRIGGER_HIGH, "LC1100H irq", data);
	if (ret) {
		printk(KERN_ERR "LC1100H: IRQ already in use.\n");
		goto exit;
	}

	return 0;

exit:
	gpio_free(data->irq_gpio);
	return ret;
}

static int lc1100h_rtc_init(struct lc1100h_data *data)
{
	/* Disable reboot rtc alarm. */
	lc1100h_rtc_alarm_disable(PMIC_RTC_ALARM_REBOOT);

	return 0;
}

static int lc1100h_hw_init(struct lc1100h_data *data)
{
	int i;
	int ret;

	/* Mask all interrupts. */
	lc1100h_reg_write(LC1100H_REG_IRQMSK1, 0xff);
	lc1100h_reg_write(LC1100H_REG_IRQEN1, 0x00);
	lc1100h_reg_write(LC1100H_REG_IRQMSK2, 0xff);
	lc1100h_reg_write(LC1100H_REG_IRQEN2, 0x00);
	lc1100h_reg_write(LC1100H_REG_RTC_INT_MASK, 0x00);

	/* Platform init, */
	for (i = 0; i < data->pdata->init_regs_num; i++)
		lc1100h_reg_bit_write((u8)data->pdata->init_regs[i].reg,
					(u8)data->pdata->init_regs[i].mask,
					(u8)data->pdata->init_regs[i].value);

	if (data->pdata->init)
		data->pdata->init();

	/* Read version id. */
	lc1100h_reg_read(LC1100H_REG_VERSION, &data->version_id);

	/* Read enable registers. */
	for (i = 0; i < LC1100H_REG_ENABLE_NUM; i++)
		lc1100h_reg_read(g_lc1100h_enable_reg[i], &data->enable_reg[i]);

	/* Initialize LC1100H reboot. */
	lc1100h_reboot_init(data);

	/* Initialize LC1100H RTC. */
	lc1100h_rtc_init(data);

	/* Initialize LC1100H power. */
	lc1100h_power_init(data);

	/* Initialize LC1100H irq. */
	ret = lc1100h_irq_init(data);
	if (ret)
		goto exit;

	return 0;

exit:
	return ret;
}

static void lc1100h_hw_exit(struct lc1100h_data *data)
{
	/* Mask all interrupts. */
	lc1100h_reg_write(LC1100H_REG_IRQMSK1, 0xff);
	lc1100h_reg_write(LC1100H_REG_IRQEN1, 0x00);
	lc1100h_reg_write(LC1100H_REG_IRQMSK2, 0xff);
	lc1100h_reg_write(LC1100H_REG_IRQEN2, 0x00);

	/* Free irq. */
	free_irq(data->irq, data);
	gpio_free(data->irq_gpio);
}

static int lc1100h_check(void)
{
	int ret = 0;
	u8 val;

	ret = lc1100h_reg_read(LC1100H_REG_VERSION, &val);
	if (ret)
		return 0;

	return 1;
}

static struct platform_device lc1100h_battery_device = {
	.name = "lc1100h-battery",
	.id   = -1,
};

#ifdef CONFIG_PM
static int lc1100h_suspend(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static int lc1100h_resume(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static struct dev_pm_ops lc1100h_pm_ops = {
	.suspend = lc1100h_suspend,
	.resume = lc1100h_resume,
};
#endif

static int __devinit lc1100h_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct lc1100h_platform_data *pdata;
	struct lc1100h_data *data;
	int err = 0;

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		printk(KERN_ERR "LC1100H: No platform data\n");
		return -EINVAL;
	}

	lc1100h_client = client;
	if ((pdata->flags & LC1100H_FLAGS_DEVICE_CHECK) && !lc1100h_check()) {
		printk(KERN_ERR "LC1100H: Can't find the device\n");
		return -ENODEV;
	}

	if (!(data = kzalloc(sizeof(struct lc1100h_data), GFP_KERNEL)))
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

	/* Initialize LC1100H hardware. */
	if ((err = lc1100h_hw_init(data)) != 0)
		goto exit;

	/* Set PMIC ops. */
	pmic_ops_set(&lc1100h_pmic_ops);

	if (!(pdata->flags & LC1100H_FLAGS_NO_BATTERY_DEVICE)) {
		err = platform_device_register(&lc1100h_battery_device);
		if (err) {
			printk(KERN_ERR "LC1100H: Register battery device failed\n");
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

static int __devexit lc1100h_remove(struct i2c_client *client)
{
	struct lc1100h_data *data = i2c_get_clientdata(client);

	if (!(data->pdata->flags & LC1100H_FLAGS_NO_BATTERY_DEVICE))
		platform_device_unregister(&lc1100h_battery_device);

	/* Clean PMIC ops. */
	pmic_ops_set(NULL);

	/* Deinitialize LC1100H hardware. */
	if (data) {
		lc1100h_hw_exit(data);
		kfree(data);
	}

	lc1100h_client = NULL;

	return 0;
}

static const struct i2c_device_id lc1100h_id[] = {
	{ "lc1100h", 0 },
	{ },
};

static struct i2c_driver lc1100h_driver = {
	.driver = {
		.name = "lc1100h",
#ifdef CONFIG_PM
		.pm = &lc1100h_pm_ops,
#endif
	},
	.probe		= lc1100h_probe,
	.remove 	= __devexit_p(lc1100h_remove),
	.id_table	= lc1100h_id,
};

static int __init lc1100h_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&lc1100h_driver);
	if (ret) {
		printk(KERN_WARNING "lc1100h: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit lc1100h_exit(void)
{
	i2c_del_driver(&lc1100h_driver);
}

subsys_initcall(lc1100h_init);
module_exit(lc1100h_exit);

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("LC1100H driver");
MODULE_LICENSE("GPL");
