/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013 LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the lc1160 hardware plarform's i2c driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2013-03-20	fanjiangang		created
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
#include <plat/lc1160.h>
#include <plat/lc1160-pmic.h>
#include <plat/comip-calib.h>
#include <plat/bootinfo.h>

//#define LC1160_DEBUG
#ifdef LC1160_DEBUG
#define LC1160_PRINT(fmt, args...) printk(KERN_ERR "[LC1160]" fmt, ##args)
#else
#define LC1160_PRINT(fmt, args...)
#endif

/* LC1160 IRQ mask. */
#define LC1160_IRQ_POWER_KEY_MASK				(LC1160_INT_KONIR | LC1160_INT_KOFFIR)
#define LC1160_IRQ_RTC1_MASK 					(LC1160_INT_RTC_AL1IS)
#define LC1160_IRQ_RTC2_MASK 					(LC1160_INT_RTC_AL2IS)
#define LC1160_IRQ_BATTERY_MASK 				(LC1160_INT_RCHGIR \
								| LC1160_INT_BATOTIR \
								| LC1160_INT_CHGOVIR \
								| LC1160_INT_CHGUVIR \
								| LC1160_INT_RTIMIR)
#define LC1160_IRQ_COMP1_MASK					(LC1160_INT_JACK_IN | LC1160_INT_JACK_OUT)
#define LC1160_IRQ_COMP2_MASK					(LC1160_INT_HOOK1_UP | LC1160_INT_HOOK1_DOWN \
												| LC1160_INT_HOOK2_UP | LC1160_INT_HOOK2_DOWN \
												| LC1160_INT_HOOK3_UP | LC1160_INT_HOOK3_DOWN)
#define LC1160_IRQ_ADC_MASK					(LC1160_INT_ADCCPIR)
#define LC1160_IRQ_CHARGER_MASK      (LC1160_INT_ADPINIR | LC1160_INT_ADPOUTIR)

/* SINK macro. */
#define LC1160_SINK_INDEX(ma)					(ma / 10)

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
 * Range of lc1160 rtc :
 *	second : 0~59
 *	minute : 0~59
 *	hour : 0~23
 *	mday : 0~30
 *	month : 0~11, since Jan
 *	week : 0~6, since Monday
 *	year : years from 2000
 **/
#define LC1160_RTC_YEAR_CHECK(year) 		\
		((year < (LC1160_RTC_YEAR_MIN - 1900)) || ((year) > (LC1160_RTC_YEAR_MAX - 1900)))
#define LC1160_RTC_YEAR2REG(year)			((year) + 1900 - LC1160_RTC_YEAR_BASE)
#define LC1160_RTC_REG2YEAR(reg)			((reg) + LC1160_RTC_YEAR_BASE - 1900)
#define LC1160_RTC_MONTH2REG(month)			((month))
#define LC1160_RTC_REG2MONTH(reg)			((reg))
#define LC1160_RTC_DAY2REG(day)			((day) - 1)
#define LC1160_RTC_REG2DAY(reg)			((reg) + 1)


/* Lc1160 version which RTC 4 times faster bug */
#define LC1160_RTC_YEAR_CHECK2(year) 		\
		((year < (LC1160_RTC_YEAR_MIN2 - 1900)) || ((year) >= (LC1160_RTC_YEAR_MAX2 - 1900)))

#define LC1160_RTC_YEAR2REG2(year)			((year) + 1900 - LC1160_RTC_YEAR_BASE2)
#define LC1160_RTC_REG2YEAR2(reg)			((reg) + LC1160_RTC_YEAR_BASE2 - 1900)

#define LC1160_RTC_FASTER_VER			(0)
#define LC1160_RTC_NORMAL_VER			(1)

/* Macro*/
#define LC1160_DIM(x)					ARRAY_SIZE(x)
#define LC1160_ARRAY(x)				(int *)&x, ARRAY_SIZE(x)

/* Each client has this additional data */
struct lc1160_pmic_data {
	struct mutex reg_lock;
	struct mutex adc_lock;
	struct mutex power_lock;
	struct workqueue_struct *wq;
	struct work_struct irq_work;
	struct lc1160_pmic_platform_data *pdata;
	int irq;
	int irq_gpio;
	int irq_mask;
	int power_on_type;
	u8 version_id;
	u8 eco_no;
	u8 rtc_version;
};

const u8 g_lc1160_eco_tab[] = {
	0x20,
	0x30, /* ECO1. */
};

const u8 g_lc1160_sink_val[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

const int g_lc1160_1750_3300_32_vout[] = {
			1750, 1800, 1850, 1900, 1950, 2000, 2050, 2100,
			2150, 2200, 2250, 2300, 2350, 2400, 2450, 2500,
			2550, 2600, 2650, 2700, 2750, 2800, 2850, 2900,
			2950, 3000, 3050, 3100, 3150, 3200, 3250, 3300
};

const int g_lc1160_850_1225_16_vout[] = {
			850, 875, 900, 925, 950, 975, 1000, 1025, 1050,
			1075, 1100, 1125, 1150, 1175, 1200, 1225
};

const int g_lc1160_850_1600_16_vout[] = {
			850, 900, 950, 1000, 1050, 1100, 1150, 1200, 1250,
			1300, 1350, 1400, 1450, 1500, 1550, 1600
};

const int g_lc1160_1150_1900_16_vout[] = {
			1150, 1200, 1250, 1300, 1350, 1400, 1450, 1500,
			1550, 1600, 1650, 1700, 1750, 1800, 1850, 1900
};

const int g_lc1200_1150_1950_16_vout[] = {
			1200, 1250, 1300, 1350, 1400, 1450, 1500, 1550,
			1600, 1650, 1700, 1750, 1800, 1850, 1900, 1950
};

#define LC1160_LDO_VOUT_MAP_ITEM(index)			&g_lc1160_ldo_vout_map[index]
#define LC1160_LDO_VOUT_MAP_NUM()				LC1160_DIM(g_lc1160_ldo_vout_map)
static struct lc1160_ldo_vout_map g_lc1160_ldo_vout_map[] = {
	{PMIC_ALDO1,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOA1CR,	0x1f},
	{PMIC_ALDO2,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOA2CR,	0x1f},
	{PMIC_ALDO3,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOA3CR,	0x1f},
	{PMIC_ALDO4,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOA4CR,	0x1f},
	{PMIC_ALDO7,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOA7CR,	0x1f},
	{PMIC_ALDO8,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOA8CR,	0x1f},
	{PMIC_ALDO9,	LC1160_ARRAY(g_lc1160_1150_1900_16_vout),	LC1160_REG_LDOA9CR,	0x1f},
	{PMIC_ALDO10,	LC1160_ARRAY(g_lc1160_1150_1900_16_vout),	LC1160_REG_LDOA10CR,	0x1f},
	{PMIC_ALDO11,	LC1160_ARRAY(g_lc1160_1150_1900_16_vout),	LC1160_REG_LDOA11CR,	0x1f},
	{PMIC_ALDO12,	LC1160_ARRAY(g_lc1160_1150_1900_16_vout),	LC1160_REG_LDOA12CR,	0x1f},
	{PMIC_ALDO13,	LC1160_ARRAY(g_lc1160_850_1600_16_vout),	LC1160_REG_LDOA13CR,	0x1f},
	{PMIC_ALDO14,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOA14CR,	0x1f},
	{PMIC_ALDO15,	LC1160_ARRAY(g_lc1200_1150_1950_16_vout),	LC1160_REG_LDOA15CR,	0x1f},
	{PMIC_DLDO1,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD1CR,	0x1f},
	{PMIC_DLDO2,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD2CR,	0x1f},
	{PMIC_DLDO3,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD3CR,	0x1f},
	{PMIC_DLDO4,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD4CR,	0x1f},
	{PMIC_DLDO5,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD5CR,	0x1f},
	{PMIC_DLDO6,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD6CR,	0x1f},
	{PMIC_DLDO7,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD7CR,	0x1f},
	{PMIC_DLDO8,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD8CR,	0x1f},
	{PMIC_DLDO9,	LC1160_ARRAY(g_lc1160_850_1600_16_vout),	LC1160_REG_LDOD9CR,	0x1f},
	{PMIC_DLDO10,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD10CR,	0x1f},
	{PMIC_DLDO11,	LC1160_ARRAY(g_lc1160_1750_3300_32_vout),	LC1160_REG_LDOD11CR,	0x1f},
};

#define LC1160_LDO_CTRL_MAP_ITEM(index) 			&g_lc1160_ldo_ctrl_map_default[index]
#define LC1160_LDO_CTRL_MAP_NUM()				LC1160_DIM(g_lc1160_ldo_ctrl_map_default)
static struct pmic_power_ctrl_map g_lc1160_ldo_ctrl_map_default[] = {
	{PMIC_DCDC1,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC2,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC3,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC4,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC5,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC6,	PMIC_POWER_CTRL_ALWAYS_ON,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_DCDC9,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1100},
	{PMIC_ALDO1,	PMIC_POWER_CTRL_GPIO,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO2,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO3,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO4,	PMIC_POWER_CTRL_GPIO,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO7,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO8,	PMIC_POWER_CTRL_GPIO,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ALDO9,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ALDO10,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ALDO11,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ALDO12,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_ALDO13,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1200},
	{PMIC_ALDO14,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	3000},
	{PMIC_ALDO15,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO1,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO2,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	3000},
	{PMIC_DLDO3,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	3000},
	{PMIC_DLDO4,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO5,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO6,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	3300},
	{PMIC_DLDO7,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO8,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_DLDO9,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1500},
	{PMIC_DLDO10,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	1800},
	{PMIC_DLDO11,	PMIC_POWER_CTRL_REG,	PMIC_POWER_CTRL_GPIO_ID_NONE,	2850},
	{PMIC_ISINK1,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_ISINK2,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
	{PMIC_ISINK3,	PMIC_POWER_CTRL_MAX,	PMIC_POWER_CTRL_GPIO_ID_NONE,	0},
};

#define LC1160_LDO_MODULE_MAP() 				g_lc1160_ldo_module_map
#define LC1160_LDO_MODULE_MAP_ITEM(index)			&g_lc1160_ldo_module_map[index]
#define LC1160_LDO_MODULE_MAP_NUM() 				g_lc1160_ldo_module_map_num
static struct pmic_power_module_map* g_lc1160_ldo_module_map;
static int g_lc1160_ldo_module_map_num;
static struct lc1160_pmic_data *g_lc1160_pmic_data;

int lc1160_read_adc(u8 val, u16 *out)
{
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;
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

	/* Open ADC power. */
	lc1160_reg_bit_write(LC1160_REG_ADCCR1, 0x01, 0x01);

	/* Set ADC channle and mode. */
	lc1160_reg_write(LC1160_REG_ADCCR2, (u8)val);
	lc1160_reg_bit_write(LC1160_REG_ADCCR1, 0x04, 0x00); /* 12bit conver */

	/* Open ADC power. */
	lc1160_reg_bit_write(LC1160_REG_ADCCR1, 0x02, 0x01);

	/* Start ADC */
	lc1160_reg_bit_write(LC1160_REG_ADCCR1, 0x08, 0x01);

	timeout = 3;
	ad_state = 0;
	do {
		udelay(50);
		timeout--;
		lc1160_reg_read(LC1160_REG_ADCCR1, &ad_state);
		ad_state = !!(ad_state & LC1160_REG_BITMASK_ADEND);
	} while(ad_state != 1 && timeout != 0);

	if (timeout == 0) {
		LC1160_PRINT("Adc convert failed\n");
		data0 = data1 = 0xff; /* Indicate ADC convert failed */
		ret = -ETIMEDOUT; /* Indicate ADC convert failed */
	} else {
		lc1160_reg_read(LC1160_REG_ADCDAT0, &data0); /* Low  4 bit */
		lc1160_reg_read(LC1160_REG_ADCDAT1, &data1); /* High 8 bit */
	}

	*out = ((u16)data1 << 4) | ((u16)data0);

	/* Disable ADC. */
	lc1160_reg_bit_write(LC1160_REG_ADCCR1, 0x01, 0x00);

	mutex_unlock(&data->adc_lock);

	return ret;
}
EXPORT_SYMBOL(lc1160_read_adc);

int lc1160_int_mask(int mask)
{
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;
	u8 int_mask0;
	u8 int_mask1;
	u8 int_mask2;
	u8 int_mask3;

	mutex_lock(&data->reg_lock);

	data->irq_mask |= mask;

	if (mask & 0x000000ff) {
		int_mask0 = (u8)(data->irq_mask & 0x000000ff);
		lc1160_reg_write(LC1160_REG_IRQMSK1, int_mask0);
		lc1160_reg_write(LC1160_REG_IRQEN1, ~int_mask0);
	}

	if ((mask >> 8) & 0x000000ff) {
		int_mask1 = (u8)((data->irq_mask >> 8) & 0x000000ff);
		lc1160_reg_write(LC1160_REG_IRQMSK2, int_mask1);
		lc1160_reg_write(LC1160_REG_IRQEN2, ~int_mask1);
	}

	if ((mask >> 16) & 0x000000ff) {
		int_mask2 = (u8)((data->irq_mask >> 16) & 0x000000ff);
		lc1160_reg_write(LC1160_REG_RTC_INT_EN, ~int_mask2);
	}

	if ((mask >> 24) & 0x000000ff) {
		int_mask3 = (u8)((data->irq_mask >> 24) & 0x000000ff);
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_JACKCR1, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
		int_mask3 = int_mask3 >> 2;
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_HS1CR, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
		int_mask3 = int_mask3 >> 2;
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_HS2CR, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
		int_mask3 = int_mask3 >> 2;
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_HS3CR, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
	}

	mutex_unlock(&data->reg_lock);

	return 0;
}
EXPORT_SYMBOL(lc1160_int_mask);

int lc1160_int_unmask(int mask)
{
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;
	u8 int_mask0;
	u8 int_mask1;
	u8 int_mask2;
	u8 int_mask3;

	mutex_lock(&data->reg_lock);

	data->irq_mask &= ~mask;

	if (mask & 0x000000ff) {
		int_mask0 = (u8)(data->irq_mask & 0x000000ff);
		lc1160_reg_write(LC1160_REG_IRQMSK1, int_mask0);
		lc1160_reg_write(LC1160_REG_IRQEN1, ~int_mask0);
	}

	if ((mask >> 8) & 0x000000ff) {
		int_mask1 = (u8)((data->irq_mask >> 8) & 0x000000ff);
		lc1160_reg_write(LC1160_REG_IRQMSK2, int_mask1);
		lc1160_reg_write(LC1160_REG_IRQEN2, ~int_mask1);
	}

	if ((mask >> 16) & 0x000000ff) {
		int_mask2 = (u8)((data->irq_mask >> 16) & 0x000000ff);
		lc1160_reg_write(LC1160_REG_RTC_INT_EN, ~int_mask2);
	}

	if ((mask >> 24) & 0x000000ff) {
		int_mask3 = (u8)((data->irq_mask >> 24) & 0x000000ff);
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_JACKCR1, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
		int_mask3 = int_mask3 >> 2;
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_HS1CR, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
		int_mask3 = int_mask3 >> 2;
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_HS2CR, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
		int_mask3 = int_mask3 >> 2;
		if (int_mask3 & 0x03)
			lc1160_reg_bit_write(LC1160_REG_HS3CR, 0xf0, (int_mask3 & 0x03) | ((~int_mask3 & 0x03) << 2));
	}

	mutex_unlock(&data->reg_lock);

	return 0;
}
EXPORT_SYMBOL(lc1160_int_unmask);

int lc1160_int_status_get(void)
{
	u8 status = 0;
	u8 status0 = 0;
	u8 status1 = 0;
	u8 status2 = 0;
	u8 status3 = 0;

	lc1160_reg_read(LC1160_REG_IRQST_FINAL, &status);

	if (status & LC1160_REG_BITMASK_CODECIRQ) {
		lc1160_reg_write(LC1160_REG_CODEC_R1_BAK, 0x03);
	}

	if (status & LC1160_REG_BITMASK_IRQ1) {
		lc1160_reg_read(LC1160_REG_IRQST1, &status0);
		if (status0)
			lc1160_reg_write(LC1160_REG_IRQST1, status0);
	}

	if (status & LC1160_REG_BITMASK_IRQ2) {
		lc1160_reg_read(LC1160_REG_IRQST2, &status1);
		if (status1)
			lc1160_reg_write(LC1160_REG_IRQST2, status1);
	}

	if (status & LC1160_REG_BITMASK_RTCIRQ) {
		lc1160_reg_read(LC1160_REG_RTC_INT_STATUS, &status2);
		if (status2)
			lc1160_reg_write(LC1160_REG_RTC_INT_STATUS, status2);
	}

	if (status & LC1160_REG_BITMASK_JKHSIRQ) {
		lc1160_reg_read(LC1160_REG_JKHSIR, &status3);
		//if (status3)
		//	lc1160_reg_write(LC1160_REG_JKHSIRQST, status3 & 0xcf);
	}

	return (((int)status3 << 24) | ((int)status2 << 16) | ((int)status1 << 8) | (int)status0);
}
EXPORT_SYMBOL(lc1160_int_status_get);

int lc1160_power_type_get(void)
{
     struct lc1160_pmic_data *data = g_lc1160_pmic_data;
     unsigned int power_on_type = PU_REASON_PWR_KEY_PRESS;
       power_on_type = data->power_on_type;
       return  power_on_type;
}
EXPORT_SYMBOL(lc1160_power_type_get);
static int lc1160_pmic_reg_read(u16 reg, u16 *value)
{
	u8 val;
	int ret;

	ret = lc1160_reg_read((u8)reg, &val);
	if(ret < 0)
		return ret;

	*value = (u16)val;

	return ret;
}

static int lc1160_pmic_reg_write(u16 reg, u16 value)
{
	return lc1160_reg_write((u8)reg, (u8)value);
}

static void lc1160_dcdc_enable(u8 id, u8 operation)
{
	if (id == PMIC_DCDC9) {
		if (operation)
			lc1160_reg_bit_set(LC1160_REG_DC9OVS0, LC1160_REG_BITMASK_DC9OVS0_DC9EN);
		else
			lc1160_reg_bit_clr(LC1160_REG_DC9OVS0, LC1160_REG_BITMASK_DC9OVS0_DC9EN);
	}
}

static void lc1160_dcdc_vout_set(u8 id, int mv)
{
	u8 i;

	if (id == PMIC_DCDC9) {
		if (mv > 3600 || mv < 450 || (mv % 50))
			return;

		i = (mv - 450) / 50;
		lc1160_reg_bit_write(LC1160_REG_DC9OVS0, LC1160_REG_BITMASK_DC9OVS0_DC9VOUT0, i);
	}
}

static void _lc1160_power_ctrl_set(u8 id, u8 ctrl)
{
	unsigned int reg;
	u8 bit;

	if (PMIC_IS_ALDO(id)) {
		reg = LC1160_REG_DCLDOCONEN;
		if (id == PMIC_ALDO1)
			bit = 2;
		else if (id == PMIC_ALDO4)
			bit = 3;
		else if (id == PMIC_ALDO8)
			bit = 4;
		else {
			LC1160_PRINT("LDO control, invalid ALDO id %d\n", id);
			return;
		}
	} else if (PMIC_IS_DCDC(id)) {
		reg = LC1160_REG_DCLDOCONEN;
		if (id == PMIC_DCDC9)
			bit = 1;
		else {
			LC1160_PRINT("LDO control, invalid DCDC id %d\n", id);
			return;
		}
	}	else {
		LC1160_PRINT("LDO control, invalid LDO id %d\n", id);
		return;
	}

	if (ctrl == PMIC_POWER_CTRL_REG || ctrl == PMIC_POWER_CTRL_OSCEN)
		lc1160_reg_bit_clr(reg, (1 << bit));
	else
		lc1160_reg_bit_set(reg, (1 << bit));
}

static int lc1160_power_ctrl_set(u8 module, u8 param, u8 ctrl)
{
	int i;
	u8 id;
	struct pmic_power_module_map *pmodule_map;

	/* Find the LDO. */
	for (i = 0; i < LC1160_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1160_LDO_MODULE_MAP_ITEM(i);
		if ((pmodule_map->module == module) && (pmodule_map->param == param)) {
			id = pmodule_map->power_id;
			break;
		}
	}
	_lc1160_power_ctrl_set(id, ctrl);
	return 0;
}

static void lc1160_ldo_reg_enable(u8 id, u8 operation)
{
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;
	struct lc1160_ldo_vout_map *vout_map;
	u8 i;
	u8 op;
	u8 val;

	for (i = 0; i < LC1160_LDO_VOUT_MAP_NUM(); i++) {
		vout_map = LC1160_LDO_VOUT_MAP_ITEM(i);
		if (vout_map->ldo_id == id)
			break;
	}

	if (i == LC1160_LDO_VOUT_MAP_NUM()) {
		LC1160_PRINT("LDO vout. can't find the ldo id %d\n", id);
		return;
	}

	mutex_lock(&data->reg_lock);

	lc1160_reg_read(vout_map->reg, &val);
	op = (val & (1 << 7)) ? 1 : 0;
	if (op != operation) {
		if (operation)
			lc1160_reg_bit_set(vout_map->reg, (1 << 7));
		else
			lc1160_reg_bit_clr(vout_map->reg, (1 << 7));
	}

	mutex_unlock(&data->reg_lock);
}

static void lc1160_ldo_vout_set(u8 id, int mv)
{
	struct lc1160_ldo_vout_map *vout_map;
	u8 i;
	u8 j;

	for (i = 0; i < LC1160_LDO_VOUT_MAP_NUM(); i++) {
		vout_map = LC1160_LDO_VOUT_MAP_ITEM(i);
		if (vout_map->ldo_id == id)
			break;
	}

	if (i == LC1160_LDO_VOUT_MAP_NUM()) {
		LC1160_PRINT("LDO vout. can't find the ldo id %d\n", id);
		return;
	}

	for (j = 0; j < vout_map->vout_range_size; j++) {
		if (mv == vout_map->vout_range[j])
			break;
	}

	if (j == vout_map->vout_range_size) {
		LC1160_PRINT("LDO(%d) vout. not support the voltage %dmV\n", id, mv);
		return;
	}

	lc1160_reg_bit_write(vout_map->reg, vout_map->mask, j);
}

static void lc1160_sink_enable(u8 id, u8 operation)
{
	u8 sinken = 0x0;
	u8 pwmen = 0x0;

	if (id == PMIC_ISINK_ID(PMIC_ISINK1)) {
		sinken = LC1160_REG_BITMASK_LCDEN;
		pwmen = 0x0;
	} else if (id == PMIC_ISINK_ID(PMIC_ISINK2)) {
		sinken = LC1160_REG_BITMASK_FLASH;
		pwmen = 0x0;
	} else if (id == PMIC_ISINK_ID(PMIC_ISINK3)) {
		sinken = LC1160_REG_BITMASK_VIBRATOREN;
		pwmen = LC1160_REG_BITMASK_PWM1_EN;
	}

	if (operation) {
		lc1160_reg_bit_set(LC1160_REG_SINKCR, sinken);
		lc1160_reg_bit_set(LC1160_REG_PWM_CTL, pwmen);
	} else {
		lc1160_reg_bit_clr(LC1160_REG_SINKCR, sinken);
		lc1160_reg_bit_clr(LC1160_REG_PWM_CTL, pwmen);
	}
}

static void lc1160_sink_iout_set(u8 id, int ma)
{
	unsigned int reg;
	u8 mask;
	u8 val;
	u8 index;

	if (id == PMIC_ISINK_ID(PMIC_ISINK1))
		ma *= 5;
	index = LC1160_SINK_INDEX(ma);
	if (index >= LC1160_DIM(g_lc1160_sink_val)) {
		LC1160_PRINT("LDO sink iout. not support the current %dmA\n", ma);
		return;
	}

	val = g_lc1160_sink_val[index];
	if (id == PMIC_ISINK_ID(PMIC_ISINK1)) {
		reg = LC1160_REG_LEDIOUT;
		mask = 0xf0;
	} else if (id == PMIC_ISINK_ID(PMIC_ISINK2)) {
		reg = LC1160_REG_VIBI_FLASHIOUT;
		mask = 0xf0;
	} else if (id == PMIC_ISINK_ID(PMIC_ISINK3)) {
		reg = LC1160_REG_VIBI_FLASHIOUT;
		mask = 0xf;
	}
	lc1160_reg_bit_write(reg, mask, val);
}

/* Event. */
static int lc1160_event_mask(int event)
{
	int mask;

	switch (event) {
		case PMIC_EVENT_POWER_KEY:
			mask = LC1160_IRQ_POWER_KEY_MASK;
			break;
		case PMIC_EVENT_CHARGER:
			mask = LC1160_IRQ_CHARGER_MASK;
			break;
		case PMIC_EVENT_BATTERY:
			mask = LC1160_IRQ_BATTERY_MASK;
			break;
		case PMIC_EVENT_ADC:
			mask = LC1160_IRQ_ADC_MASK;
			break;
		case PMIC_EVENT_RTC1:
			mask = LC1160_IRQ_RTC1_MASK;
			break;
		case PMIC_EVENT_RTC2:
			mask = LC1160_IRQ_RTC2_MASK;
			break;
		case PMIC_EVENT_COMP1:
			mask = LC1160_IRQ_COMP1_MASK;
			break;
		case PMIC_EVENT_COMP2:
			lc1160_reg_bit_clr(LC1160_REG_HS1CR, LC1160_REG_BITMASK_HS1DETEN);
			mask = LC1160_IRQ_COMP2_MASK;
			break;
		default:
			return -EINVAL;
	}

	lc1160_int_mask(mask);

	return 0;
}

static int lc1160_event_unmask(int event)
{
	int mask;

	switch (event) {
		case PMIC_EVENT_POWER_KEY:
			mask = LC1160_IRQ_POWER_KEY_MASK;
			break;
		case PMIC_EVENT_CHARGER:
			mask = LC1160_IRQ_CHARGER_MASK;
			break;
		case PMIC_EVENT_BATTERY:
			mask = LC1160_IRQ_BATTERY_MASK;
			break;
		case PMIC_EVENT_ADC:
			mask = LC1160_IRQ_ADC_MASK;
			break;
		case PMIC_EVENT_RTC1:
			mask = LC1160_IRQ_RTC1_MASK;
			break;
		case PMIC_EVENT_RTC2:
			mask = LC1160_IRQ_RTC2_MASK;
			break;
		case PMIC_EVENT_COMP1:
			mask = LC1160_IRQ_COMP1_MASK;
			break;
		case PMIC_EVENT_COMP2:
			lc1160_reg_bit_set(LC1160_REG_HS1CR, LC1160_REG_BITMASK_HS1DETEN);
			mask = LC1160_IRQ_COMP2_MASK;
			break;
		default:
			return -EINVAL;
	}

	lc1160_int_unmask(mask);

	return 0;
}

static struct pmic_power_ctrl_map* lc1160_power_ctrl_find(int power_id)
{
	struct pmic_power_ctrl_map *pctrl_map = NULL;
	u8 i;

	for (i = 0; i < LC1160_LDO_CTRL_MAP_NUM(); i++) {
		pctrl_map = LC1160_LDO_CTRL_MAP_ITEM(i);
		if (pctrl_map->power_id == power_id)
			break;
	}

	if (i == LC1160_LDO_CTRL_MAP_NUM())
		pctrl_map = NULL;

	return pctrl_map;
}

/* Power. */
static int lc1160_power_set(int power_id, int mv, int force)
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
	for (i = 0; i < LC1160_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1160_LDO_MODULE_MAP_ITEM(i);
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

	pctrl_map = lc1160_power_ctrl_find(power_id);
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
			lc1160_ldo_vout_set(power_id, mv);

			/* Save the voltage. */
			pctrl_map->default_mv = mv;
		}

		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of this LDO. */
			if (ctrl == PMIC_POWER_CTRL_REG) {
				lc1160_ldo_reg_enable(power_id, operation);
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
			lc1160_sink_iout_set(PMIC_ISINK_ID(power_id), mv);

			/* Save the current. */
			pctrl_map->default_mv = mv;
		}

		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of this ISINK. */
			lc1160_sink_enable(PMIC_ISINK_ID(power_id), operation);

			pctrl_map->state = operation;
		}
	} else if (PMIC_IS_DCDC(power_id)) {
		/* Set the voltage of this DCDC. */
		if ((mv != PMIC_POWER_VOLTAGE_DISABLE)
			&& (mv != PMIC_POWER_VOLTAGE_ENABLE)
			&& (pctrl_map->default_mv != mv)) {
			lc1160_dcdc_vout_set(power_id, mv);

			/* Save the voltage. */
			pctrl_map->default_mv = mv;
		}

		if ((pctrl_map->state != operation) || force) {
			/* Set the enable state of this DCDC. */
			lc1160_dcdc_enable(power_id, operation);

			pctrl_map->state = operation;
		}
	}

	return 0;
}

static int lc1160_buck2_voltage_tb[] = {
	6500, 6625, 6750, 6875, 7000, 7125, 7250, 7375,7500,7625, 7750,7875, 8000, 8125,
	8250, 8375, 8500, 8625, 8750, 8875, 9000, 9125, 9250, 9375, 9500, 9625, 9750, 9875,
	1000, 10125, 10250, 10375, 10500, 10625, 10750, 10875, 11000, 11125, 11250, 11375,
	11500, 11625, 11750, 11875, 12000, 12125, 12250, 12375, 12500, 12625, 12750, 12875,
	13000, 13125, 13250, 13375, 13500, 13625, 13750, 13875, 14000, 14125, 14250, 14375
};

static int lc1160_direct_voltage_get(u8 power_id, int param, int *pmv)
{
	u8 val;

	if (power_id == PMIC_DCDC2) {
		switch (param) {
		case 0:
			lc1160_reg_read(LC1160_REG_DC2OVS0, &val);
			val = val & 0x3f;
			*pmv = lc1160_buck2_voltage_tb[val];
			break;
		case 1:
			lc1160_reg_read(LC1160_REG_DC2OVS1, &val);
			val = val & 0x3f;
			*pmv = lc1160_buck2_voltage_tb[val];
			break;
		case 2:
			lc1160_reg_read(LC1160_REG_DC2OVS2, &val);
			val = val & 0x3f;
			*pmv = lc1160_buck2_voltage_tb[val];
			break;
		case 3:
			lc1160_reg_read(LC1160_REG_DC2OVS3, &val);
			val = val & 0x3f;
			*pmv = lc1160_buck2_voltage_tb[val];
			break;
		default:
			break;
		}
	}

	return 0;
}

static int lc1160_direct_voltage_set(u8 power_id, int param, int mv)
{
	int ret = -1;
	u8 i;

	if (power_id == PMIC_DCDC2) {
		for (i = 0; i < LC1160_DIM(lc1160_buck2_voltage_tb); i++) {
			if (lc1160_buck2_voltage_tb[i] == mv)
				break;
		}

		if (i == LC1160_DIM(lc1160_buck2_voltage_tb))
			return -EINVAL;

		switch (param) {
		case 0:
			ret = lc1160_reg_bit_write(LC1160_REG_DC2OVS0, LC1160_REG_BITMASK_DC2OVS0_DC2VOUT0, i);
			break;
		case 1:
			ret = lc1160_reg_bit_write(LC1160_REG_DC2OVS1, LC1160_REG_BITMASK_DC2OVS1_DC2VOUT1, i);
			break;
		case 2:
			ret = lc1160_reg_bit_write(LC1160_REG_DC2OVS2, LC1160_REG_BITMASK_DC2OVS2_DC2VOUT2, i);
			break;
		case 3:
			ret = lc1160_reg_bit_write(LC1160_REG_DC2OVS3, LC1160_REG_BITMASK_DC2OVS2_DC2VOUT3, i);
			break;
		default:
			break;
		}
	}

	return ret;
}

static int lc1160_voltage_get(u8 module, u8 param, int *pmv)
{
	int i;
	struct pmic_power_ctrl_map *pctrl_map;
	struct pmic_power_module_map *pmodule_map;
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;

	/* Find the LDO. */
	for (i = 0; i < LC1160_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1160_LDO_MODULE_MAP_ITEM(i);
		if (PMIC_IS_DIRECT_POWER_ID(module)) {
			mutex_lock(&data->power_lock);
			lc1160_direct_voltage_get(PMIC_DIRECT_POWER_ID(module), param, pmv);
			mutex_unlock(&data->power_lock);
			break;
		}
		if ((pmodule_map->module == module) && (pmodule_map->param == param)) {
			pctrl_map = lc1160_power_ctrl_find(pmodule_map->power_id);
			if (!pctrl_map)
				return -ENODEV;

			*pmv = pmodule_map->operation ? pctrl_map->default_mv : 0;
			break;
		}
	}

	return 0;
}

static int lc1160_voltage_set(u8 module, u8 param, int mv)
{
	int i;
	int ret = -ENXIO;
	struct pmic_power_module_map *pmodule_map;
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;

	/* Find the LDO. */
	for (i = 0; i < LC1160_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1160_LDO_MODULE_MAP_ITEM(i);
		if (PMIC_IS_DIRECT_POWER_ID(module)) {
			mutex_lock(&data->power_lock);
			lc1160_direct_voltage_set(PMIC_DIRECT_POWER_ID(module), param, mv);
			mutex_unlock(&data->power_lock);
			break;
		}
		if ((pmodule_map->module == module) && (pmodule_map->param == param)) {
			mutex_lock(&data->power_lock);
			pmodule_map->operation = (mv == PMIC_POWER_VOLTAGE_DISABLE) ? 0 : 1;
			ret = lc1160_power_set(pmodule_map->power_id, mv, 0);
			mutex_unlock(&data->power_lock);
			break;
		}
	}

	return ret;
}

/* RTC. */
void lc1160_rtc_normal2fast(struct rtc_time *tm, unsigned int direction)
{
	static unsigned long time_gap = 0;
	unsigned long time;

	if (!time_gap) {
		struct rtc_time tm_gap;
		tm_gap.tm_year = LC1160_RTC_YEAR_MIN2 - 1900;
		tm_gap.tm_mon = 0;
		tm_gap.tm_mday = 1;
		tm_gap.tm_hour = 0;
		tm_gap.tm_min = 0;
		tm_gap.tm_sec = 0;
		rtc_tm_to_time(&tm_gap, &time_gap);
	}

	rtc_tm_to_time(tm, &time);

	if (direction)
		time = time_gap + ((time - time_gap) * 4);
	else
		time = time_gap + ((time - time_gap) / 4);

	rtc_time_to_tm(time, tm);
}

static int lc1160_rtc_time_get(struct rtc_time *tm)
{
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;
	u8 val;

	/* Read hour, minute and second. */
	lc1160_reg_read(LC1160_REG_RTC_SEC, &val);
	tm->tm_sec = val & LC1160_REG_BITMASK_RTC_SEC;
	lc1160_reg_read(LC1160_REG_RTC_MIN, &val);
	tm->tm_min = val & LC1160_REG_BITMASK_RTC_MIN;
	lc1160_reg_read(LC1160_REG_RTC_HOUR, &val);
	tm->tm_hour = val & LC1160_REG_BITMASK_RTC_HOUR;

	/* Read day, month and year. */
	lc1160_reg_read(LC1160_REG_RTC_DAY, &val);
	tm->tm_mday = LC1160_RTC_REG2DAY((val & LC1160_REG_BITMASK_RTC_DAY));
	lc1160_reg_read(LC1160_REG_RTC_MONTH, &val);
	tm->tm_mon = LC1160_RTC_REG2MONTH((val & LC1160_REG_BITMASK_RTC_MONTH));
	lc1160_reg_read(LC1160_REG_RTC_YEAR, &val);

	if (data->rtc_version == LC1160_RTC_FASTER_VER)
		tm->tm_year = LC1160_RTC_REG2YEAR2((val & LC1160_REG_BITMASK_RTC_YEAR));
	else
		tm->tm_year = LC1160_RTC_REG2YEAR((val & LC1160_REG_BITMASK_RTC_YEAR));

	/* Fix rtc bug*/
	if (data->rtc_version == LC1160_RTC_FASTER_VER)
		lc1160_rtc_normal2fast(tm, 0);

	LC1160_PRINT("lc1160_rtc_time_get:%d-%d-%d %d:%d:%d\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return 0;
}

static int lc1160_rtc_time_set(struct rtc_time *tm)
{
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;

	/* Fix rtc bug*/
	if (data->rtc_version == LC1160_RTC_FASTER_VER) {
		if (LC1160_RTC_YEAR_CHECK2(tm->tm_year))
			return -EINVAL;

		lc1160_rtc_normal2fast(tm, 1);
	} else {
		if (LC1160_RTC_YEAR_CHECK(tm->tm_year))
			return -EINVAL;
	}

	/* Write hour, minute and second. */
	lc1160_reg_write(LC1160_REG_RTC_SEC, 0);
	lc1160_reg_write(LC1160_REG_RTC_HOUR, ((u8)tm->tm_hour & LC1160_REG_BITMASK_RTC_HOUR));
	lc1160_reg_write(LC1160_REG_RTC_MIN, ((u8)tm->tm_min & LC1160_REG_BITMASK_RTC_MIN));
	lc1160_reg_write(LC1160_REG_RTC_SEC, ((u8)tm->tm_sec & LC1160_REG_BITMASK_RTC_SEC));

	/* Write day, month and year. */
	lc1160_reg_write(LC1160_REG_RTC_DAY, ((u8)LC1160_RTC_DAY2REG(tm->tm_mday) & LC1160_REG_BITMASK_RTC_DAY));
	lc1160_reg_write(LC1160_REG_RTC_MONTH, ((u8)LC1160_RTC_MONTH2REG(tm->tm_mon) & LC1160_REG_BITMASK_RTC_MONTH));

	if (data->rtc_version == LC1160_RTC_FASTER_VER)
		lc1160_reg_write(LC1160_REG_RTC_YEAR, ((u8)LC1160_RTC_YEAR2REG2(tm->tm_year) & LC1160_REG_BITMASK_RTC_YEAR));
	else
		lc1160_reg_write(LC1160_REG_RTC_YEAR, ((u8)LC1160_RTC_YEAR2REG(tm->tm_year) & LC1160_REG_BITMASK_RTC_YEAR));

	/* Write data to RTC */
	lc1160_reg_write(LC1160_REG_RTC_TIME_UPDATE, 0x1a);

	/* Enable RTC */
	lc1160_reg_write(LC1160_REG_RTC_CTRL, 1);

	return 0;
}

static void lc1160_rtc_alarm_enable(u8 id)
{
	if (id >= LC1160_RTC_ALARM_NUM)
		return;

	/* Update 32k CLK RTC alarm. */
	lc1160_reg_write(LC1160_REG_RTC_AL_UPDATE, 0x3a);

	if(id == 0)
		lc1160_int_unmask(LC1160_IRQ_RTC1_MASK);
	else
		lc1160_int_unmask(LC1160_IRQ_RTC2_MASK);
}

static void lc1160_rtc_alarm_disable(u8 id)
{
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;

	if (id >= LC1160_RTC_ALARM_NUM)
		return;

	if(id == 0) {
		lc1160_reg_write(LC1160_REG_RTC_AL1_SEC, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL1_MIN, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL1_HOUR, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL1_DAY, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL1_MONTH, 0);

		if (data->rtc_version == LC1160_RTC_FASTER_VER)
			lc1160_reg_write(LC1160_REG_RTC_AL1_YEAR, LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2);
		else
			lc1160_reg_write(LC1160_REG_RTC_AL1_YEAR, 0);

		lc1160_int_mask(LC1160_IRQ_RTC1_MASK);
	} else {
		lc1160_reg_write(LC1160_REG_RTC_AL2_SEC, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL2_MIN, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL2_HOUR, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL2_DAY, 0);
		lc1160_reg_write(LC1160_REG_RTC_AL2_MONTH, 0);

		if (data->rtc_version == LC1160_RTC_FASTER_VER)
			lc1160_reg_write(LC1160_REG_RTC_AL2_YEAR, LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2);
		else
			lc1160_reg_write(LC1160_REG_RTC_AL2_YEAR, 0);

		lc1160_int_mask(LC1160_IRQ_RTC2_MASK);
	}
	/* Update 32k CLK RTC alarm. */
	lc1160_reg_write(LC1160_REG_RTC_AL_UPDATE, 0x3a);
}

static int lc1160_rtc_alarm_get(u8 id, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alrm_time = &(alrm->time);
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;

	u8 val;

	if (id >= LC1160_RTC_ALARM_NUM)
		return -EINVAL;

	if(id == 0) {
		/* Read the alarm time of RTC. */
		lc1160_reg_read(LC1160_REG_RTC_AL1_SEC, &val);
		if (!(val & LC1160_RTC_ALARM_TIME_IGNORE))
			alrm_time->tm_sec = val & LC1160_REG_BITMASK_RTC_AL1_SEC;
		else
			alrm_time->tm_sec = 0;
		lc1160_reg_read(LC1160_REG_RTC_AL1_MIN, &val);
		alrm_time->tm_min = val & LC1160_REG_BITMASK_RTC_AL1_MIN;
		lc1160_reg_read(LC1160_REG_RTC_AL1_HOUR, &val);
		alrm_time->tm_hour = val & LC1160_REG_BITMASK_RTC_AL1_HOUR;
		lc1160_reg_read(LC1160_REG_RTC_AL1_DAY, &val);
		alrm_time->tm_mday = LC1160_RTC_REG2DAY(val & LC1160_REG_BITMASK_RTC_AL1_DAY);
		lc1160_reg_read(LC1160_REG_RTC_AL1_MONTH, &val);
		alrm_time->tm_mon = LC1160_RTC_REG2MONTH(val & LC1160_REG_BITMASK_RTC_AL1_MONTH);
		lc1160_reg_read(LC1160_REG_RTC_AL1_YEAR, &val);

		if (data->rtc_version == LC1160_RTC_FASTER_VER)
			alrm_time->tm_year = LC1160_RTC_REG2YEAR2(val & LC1160_REG_BITMASK_RTC_AL1_YEAR);
		else
			alrm_time->tm_year = LC1160_RTC_REG2YEAR(val & LC1160_REG_BITMASK_RTC_AL1_YEAR);

		/* Get enabled state. */
		lc1160_reg_read(LC1160_REG_RTC_INT_EN, &val);
		alrm->enabled = !!(val & LC1160_REG_BITMASK_RTC_AL1_EN);

		/* Get pending state. */
		lc1160_reg_read(LC1160_REG_RTC_INT_STATUS, &val);
		alrm->pending = !!(val & LC1160_REG_BITMASK_RTC_AL1IS);
	} else {
		/* Read the alarm time of RTC. */
		lc1160_reg_read(LC1160_REG_RTC_AL2_SEC, &val);
		if (!(val & LC1160_RTC_ALARM_TIME_IGNORE))
			alrm_time->tm_sec = val & LC1160_REG_BITMASK_RTC_AL2_SEC;
		else
			alrm_time->tm_sec = 0;
		lc1160_reg_read(LC1160_REG_RTC_AL2_MIN, &val);
		alrm_time->tm_min = val & LC1160_REG_BITMASK_RTC_AL2_MIN;
		lc1160_reg_read(LC1160_REG_RTC_AL2_HOUR, &val);
		alrm_time->tm_hour = val & LC1160_REG_BITMASK_RTC_AL2_HOUR;
		lc1160_reg_read(LC1160_REG_RTC_AL2_DAY, &val);
		alrm_time->tm_mday = LC1160_RTC_REG2DAY(val & LC1160_REG_BITMASK_RTC_AL2_DAY);
		lc1160_reg_read(LC1160_REG_RTC_AL2_MONTH, &val);
		alrm_time->tm_mon = LC1160_RTC_REG2MONTH(val & LC1160_REG_BITMASK_RTC_AL2_MONTH);
		lc1160_reg_read(LC1160_REG_RTC_AL2_YEAR, &val);

		if (data->rtc_version == LC1160_RTC_FASTER_VER)
			alrm_time->tm_year = LC1160_RTC_REG2YEAR2(val & LC1160_REG_BITMASK_RTC_AL2_YEAR);
		else
			alrm_time->tm_year = LC1160_RTC_REG2YEAR(val & LC1160_REG_BITMASK_RTC_AL2_YEAR);

		/* Get enabled state. */
		lc1160_reg_read(LC1160_REG_RTC_INT_EN, &val);
		alrm->enabled = !!(val & LC1160_REG_BITMASK_RTC_AL2_EN);

		/* Get pending state. */
		lc1160_reg_read(LC1160_REG_RTC_INT_STATUS, &val);
		alrm->pending = !!(val & LC1160_REG_BITMASK_RTC_AL2IS);
	}

	if (data->rtc_version == LC1160_RTC_FASTER_VER)
		lc1160_rtc_normal2fast(alrm_time, 0);

	return 0;
}

static int lc1160_rtc_alarm_set(u8 id, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alrm_time = &(alrm->time);
	struct lc1160_pmic_data *data = g_lc1160_pmic_data;
	u8 reg;

	if (id >= LC1160_RTC_ALARM_NUM)
		return -EINVAL;

	if (!alrm->enabled) {
		lc1160_rtc_alarm_disable(id);
		return 0;
	}

	/* Fix rtc bug*/
	if (data->rtc_version == LC1160_RTC_FASTER_VER) {
		if (LC1160_RTC_YEAR_CHECK2(alrm_time->tm_year))
			return -EINVAL;

		lc1160_rtc_normal2fast(alrm_time, 1);
	} else {
		if (LC1160_RTC_YEAR_CHECK(alrm_time->tm_year))
			return -EINVAL;
	}

	/* Write the alarm time of RTC. */
	if(id == 0) {
		reg = LC1160_REG_RTC_AL1_SEC;
		lc1160_reg_write(LC1160_REG_RTC_AL1_MIN, (u8)alrm_time->tm_min & LC1160_REG_BITMASK_RTC_AL1_MIN);
		lc1160_reg_write(LC1160_REG_RTC_AL1_HOUR, (u8)alrm_time->tm_hour & LC1160_REG_BITMASK_RTC_AL1_HOUR);
		lc1160_reg_write(LC1160_REG_RTC_AL1_DAY, (u8)LC1160_RTC_DAY2REG(alrm_time->tm_mday) & LC1160_REG_BITMASK_RTC_AL1_DAY);
		lc1160_reg_write(LC1160_REG_RTC_AL1_MONTH, (u8)LC1160_RTC_MONTH2REG(alrm_time->tm_mon) & LC1160_REG_BITMASK_RTC_AL1_MONTH);

		if (data->rtc_version == LC1160_RTC_FASTER_VER)
			lc1160_reg_write(LC1160_REG_RTC_AL1_YEAR, (u8)LC1160_RTC_YEAR2REG2(alrm_time->tm_year) & LC1160_REG_BITMASK_RTC_AL1_YEAR);
		else
			lc1160_reg_write(LC1160_REG_RTC_AL1_YEAR, (u8)LC1160_RTC_YEAR2REG(alrm_time->tm_year) & LC1160_REG_BITMASK_RTC_AL1_YEAR);

	} else {
		reg = LC1160_REG_RTC_AL2_SEC;
		lc1160_reg_write(LC1160_REG_RTC_AL2_MIN, (u8)alrm_time->tm_min & LC1160_REG_BITMASK_RTC_AL2_MIN);
		lc1160_reg_write(LC1160_REG_RTC_AL2_HOUR, (u8)alrm_time->tm_hour & LC1160_REG_BITMASK_RTC_AL2_HOUR);
		lc1160_reg_write(LC1160_REG_RTC_AL2_DAY, (u8)LC1160_RTC_DAY2REG(alrm_time->tm_mday) & LC1160_REG_BITMASK_RTC_AL2_DAY);
		lc1160_reg_write(LC1160_REG_RTC_AL2_MONTH, (u8)LC1160_RTC_MONTH2REG(alrm_time->tm_mon) & LC1160_REG_BITMASK_RTC_AL2_MONTH);

		if (data->rtc_version == LC1160_RTC_FASTER_VER)
			lc1160_reg_write(LC1160_REG_RTC_AL2_YEAR, (u8)LC1160_RTC_YEAR2REG2(alrm_time->tm_year) & LC1160_REG_BITMASK_RTC_AL2_YEAR2);
		else
			lc1160_reg_write(LC1160_REG_RTC_AL2_YEAR, (u8)LC1160_RTC_YEAR2REG(alrm_time->tm_year) & LC1160_REG_BITMASK_RTC_AL2_YEAR);

	}

	/* Power-off alarm don't use rtc second register. */
	if (id == PMIC_RTC_ALARM_POWEROFF)
		lc1160_reg_write(reg, LC1160_RTC_ALARM_TIME_IGNORE);
	else
		lc1160_reg_write(reg, (u8)alrm_time->tm_sec & LC1160_RTC_ALARM_TIME_MASK);

	lc1160_rtc_alarm_enable(id);

	return 0;
}

/* Comparator. */
static int lc1160_comp_polarity_set(u8 id, u8 pol)
{
	if (id == PMIC_COMP1)
		lc1160_reg_bit_write(LC1160_REG_JACKCR1, LC1160_REG_BITMASK_JACKDETPOL, !pol);

	return 0;
}

static int lc1160_comp_state_get(u8 id)
{
	u8 reg;
	u8 mask;

	if (id == PMIC_COMP1) {
		lc1160_reg_read(LC1160_REG_JACKCR1, &reg);
		mask = LC1160_REG_BITMASK_JACK_CMP;
	}
	else {
		lc1160_reg_read(LC1160_REG_HS1CR, &reg);
		mask = LC1160_REG_BITMASK_HS1_CMP;
	}

	return !!(reg & mask);
}

/* Power key. */
static int lc1160_power_key_state_get(void)
{
	u8 val;

	/* Get general status. */
	lc1160_reg_read(LC1160_REG_PCST, &val);
	if (val & LC1160_REG_BITMASK_KONMON)
		return PMIC_POWER_KEY_ON;

	return PMIC_POWER_KEY_OFF;
}

/* pmic adapin  state. */
static int lc1160_adapin_state_get(void)
{
	u8 val = 0;

	/* Get general status. */
	lc1160_reg_read(LC1160_REG_PCST, &val);
	if (val & LC1160_REG_BITMASK_ADPIN){
		return !!(val & LC1160_REG_BITMASK_ADPIN);
	}else{
		return 0;
	}
}
/* Reboot type. */
static int lc1160_reboot_type_set(u8 type)
{

	if (type == REBOOT_RTC_ALARM)
		lc1160_rtc_alarm_disable(PMIC_RTC_ALARM_POWEROFF);

	lc1160_reg_write(LC1160_REG_RTC_DATA2, type);


	return 0;
}

static struct pmic_ops lc1160_pmic_ops = {
	.name			= "lc1160",

	.reg_read		= lc1160_pmic_reg_read,
	.reg_write		= lc1160_pmic_reg_write,

	.event_mask		= lc1160_event_mask,
	.event_unmask		= lc1160_event_unmask,

	.voltage_get		= lc1160_voltage_get,
	.voltage_set		= lc1160_voltage_set,

	.rtc_time_get		= lc1160_rtc_time_get,
	.rtc_time_set		= lc1160_rtc_time_set,
	.rtc_alarm_get		= lc1160_rtc_alarm_get,
	.rtc_alarm_set		= lc1160_rtc_alarm_set,

	.comp_polarity_set 	= lc1160_comp_polarity_set,
	.comp_state_get 	= lc1160_comp_state_get,

	.reboot_type_set	= lc1160_reboot_type_set,
	.power_key_state_get	= lc1160_power_key_state_get,
	.vchg_input_state_get	= lc1160_adapin_state_get,
	.power_ctrl_set		= lc1160_power_ctrl_set,
};

static irqreturn_t lc1160_irq_handle(int irq, void *dev_id)
{
	struct lc1160_pmic_data *data = (struct lc1160_pmic_data *)dev_id;

	disable_irq_nosync(data->irq);
	queue_work(data->wq, &data->irq_work);

	return IRQ_HANDLED;
}

static void lc1160_irq_work_handler(struct work_struct *work)
{
	int status = 0,power_key_status = 0;
	struct lc1160_pmic_data *data = container_of(work, struct lc1160_pmic_data, irq_work);

	status = lc1160_int_status_get();
	status &= ~data->irq_mask;

	/* Power key. */
	if (status & LC1160_IRQ_POWER_KEY_MASK){
		if ((status & LC1160_IRQ_POWER_KEY_MASK) == LC1160_INT_KONIR){
			power_key_status = IRQ_POWER_KEY_PRESS;
		}else if ((status & LC1160_IRQ_POWER_KEY_MASK) == LC1160_INT_KOFFIR){
			power_key_status = IRQ_POWER_KEY_RELEASE;
		}else{
			power_key_status = IRQ_POWER_KEY_PRESS_RELEASE;
		}
		pmic_event_handle(PMIC_EVENT_POWER_KEY, &power_key_status);
	}
	/* CHARGER . */
	if (status & LC1160_IRQ_CHARGER_MASK)
		pmic_event_handle(PMIC_EVENT_CHARGER, &status);

	/* Battery. */
	if (status & LC1160_IRQ_BATTERY_MASK)
		pmic_event_handle(PMIC_EVENT_BATTERY, &status);

	/* ADC. */
	if (status & LC1160_IRQ_ADC_MASK)
		pmic_event_handle(PMIC_EVENT_ADC, &status);

	/* RTC1. */
	if (status & LC1160_IRQ_RTC1_MASK)
		pmic_event_handle(PMIC_EVENT_RTC1, &status);

	/* RTC2. */
	if (status & LC1160_IRQ_RTC2_MASK)
		pmic_event_handle(PMIC_EVENT_RTC2, &status);

	/* Comparator1. */
	if (status & LC1160_IRQ_COMP1_MASK)
		pmic_event_handle(PMIC_EVENT_COMP1, &status);

	/* Comparator2. */
	if (status & LC1160_IRQ_COMP2_MASK)
		pmic_event_handle(PMIC_EVENT_COMP2, &status);

	enable_irq(data->irq);
}

static void lc1160_hw_boot_reason(u8 startup_type, u8 shutdown_type, u8 saved_shutdown)
{
	int i;
	char startup_type_str[50] = "";
	char shutdown_type_str[50] = "";
	char saved_shutdown_str[50] = "";
	const char *startup_str[] = {"pwr_key", "hf_pwr", "charger", "alarm1", "alarm2", "reset"};
	const char *shutdown_str[] = {"ps_hold", "hi_temp", "uvlo", "", "pwr_key", "reset"};

	for (i = 0; i < ARRAY_SIZE(startup_str); i++) {
		if (startup_type & (1 << i)) {
			if (strlen(startup_type_str))
				strcat(startup_type_str, " ");
			strcat(startup_type_str, startup_str[i]);
		}
	}

	for (i = 0; i < ARRAY_SIZE(shutdown_str); i++) {
		if (shutdown_type & (1 << i)) {
			if (strlen(shutdown_type_str))
				strcat(shutdown_type_str, " ");
			strcat(shutdown_type_str, shutdown_str[i]);
		}
	}


	for (i = 0; i < ARRAY_SIZE(shutdown_str); i++) {
		if (saved_shutdown & (1 << i)) {
			if (strlen(saved_shutdown_str))
				strcat(saved_shutdown_str, " ");
			strcat(saved_shutdown_str, shutdown_str[i]);
		}
	}

	printk(KERN_INFO "LC1160: Raw startup 0x%02x(%s),raw shutdown 0x%02x(%s), saved shutdown 0x%02x(%s)\n",
			startup_type, startup_type_str, shutdown_type,
			shutdown_type_str, saved_shutdown ,saved_shutdown_str);


}

static int lc1160_reboot_init(struct lc1160_pmic_data *data)
{
	u8 val;
	u8 startup = 0;
	u8 shutdown = 0;
	u8 saved_shutdown = 0;
	u8 reboot_user_flag = 0;
	unsigned int power_on_type = PU_REASON_PWR_KEY_PRESS;
	unsigned int reboot_reason = REBOOT_NONE;

	/* Read power on reson from pmic hardware */
	lc1160_reg_read(LC1160_REG_STARTUP_STATUS, &startup);
	lc1160_reg_read(LC1160_REG_SHDN_STATUS, &shutdown);

	lc1160_reg_read(LC1160_REG_RTC_DATA1, &saved_shutdown);
	lc1160_reg_write(LC1160_REG_RTC_DATA1, 0);
	saved_shutdown &= (~(0x3<<6));
	lc1160_hw_boot_reason(startup, shutdown, saved_shutdown);

	if (startup & (LC1160_REG_BITMASK_ALARM2_STARTUP | LC1160_REG_BITMASK_ALARM1_STARTUP))
		power_on_type = PU_REASON_RTC_ALARM;
	else if (startup & LC1160_REG_BITMASK_RSTINB_STARTUP)
		power_on_type = PU_REASON_HARDWARE_RESET;
	else if (startup & LC1160_REG_BITMASK_ADPIN_STARTUP)
		power_on_type = PU_REASON_USB_CHARGER;
	else if (startup & LC1160_REG_BITMASK_KEYON_STARTUP)
		power_on_type = PU_REASON_PWR_KEY_PRESS;

	if ((!startup) && (LC1160_REG_BITMASK_RSTINB_SHDN & shutdown))
		power_on_type = PU_REASON_HARDWARE_RESET;

	/* Read shutdown reson from ourself setting && reset record info. */
	lc1160_reg_read(LC1160_REG_RTC_DATA2, &val);
	reboot_reason = (val & (~LC1160_RTC_ALARM_TIME_IGNORE));
	if (reboot_reason) {
		lc1160_reg_write(LC1160_REG_RTC_DATA2, 0);
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
		lc1160_rtc_alarm_enable(PMIC_RTC_ALARM_POWEROFF);

	printk(KERN_INFO "LC1160: Power on reason(%s), reboot reason(%s%s)\n",
			get_boot_reason_string(power_on_type),
			get_reboot_reason_string(reboot_reason),
			reboot_user_flag ? "-user" : "");

	data->power_on_type = power_on_type;
	set_boot_reason(power_on_type);
	set_reboot_reason(reboot_reason);

	return 0;
}

static void lc1160_power_init(struct lc1160_pmic_data *data)
{
	int i;
	struct pmic_power_ctrl_map *pctrl_map;
	struct pmic_power_ctrl_map *pctrl_map_default;
	struct pmic_power_module_map *pmodule_map;

	/* Save the module map. */
	LC1160_LDO_MODULE_MAP() = data->pdata->module_map;
	LC1160_LDO_MODULE_MAP_NUM() = data->pdata->module_map_num;

	/* Set the control of LDO. */
	for (i = 0; i < data->pdata->ctrl_map_num; i++) {
		pctrl_map = &data->pdata->ctrl_map[i];
		pctrl_map_default = lc1160_power_ctrl_find(pctrl_map->power_id);
		if (!pctrl_map_default)
			continue;

		if (PMIC_IS_DCDC(pctrl_map->power_id)) {
			if (pctrl_map_default->default_mv != pctrl_map->default_mv)
				lc1160_dcdc_vout_set(pctrl_map->power_id, pctrl_map->default_mv);
		} else if (PMIC_IS_DLDO(pctrl_map->power_id) || PMIC_IS_ALDO(pctrl_map->power_id)) {
			if (pctrl_map_default->ctrl != pctrl_map->ctrl)
				_lc1160_power_ctrl_set(pctrl_map->power_id, pctrl_map->ctrl);
			if (pctrl_map_default->default_mv != pctrl_map->default_mv)
				lc1160_ldo_vout_set(pctrl_map->power_id, pctrl_map->default_mv);
		} else if (PMIC_IS_ISINK(pctrl_map->power_id)) {
			if (pctrl_map_default->default_mv != pctrl_map->default_mv)
				lc1160_sink_iout_set(PMIC_ISINK_ID(pctrl_map->power_id), pctrl_map->default_mv);
		}
		memcpy(pctrl_map_default, pctrl_map, sizeof(struct pmic_power_ctrl_map));
	}

	/* Set the power of all modules. */
	for (i = 0; i < LC1160_LDO_MODULE_MAP_NUM(); i++) {
		pmodule_map = LC1160_LDO_MODULE_MAP_ITEM(i);
		lc1160_power_set(pmodule_map->power_id, PMIC_POWER_VOLTAGE_DISABLE, 1);
	}
}

static int lc1160_irq_init(struct lc1160_pmic_data *data)
{
	int ret;

	INIT_WORK(&data->irq_work, lc1160_irq_work_handler);

	ret = gpio_request(data->irq_gpio, " LC1160 irq");
	if (ret < 0) {
		printk(KERN_ERR "LC1160: Failed to request GPIO.\n");
		return ret;
	}

	ret = gpio_direction_input(data->irq_gpio);
	if (ret < 0) {
		printk(KERN_ERR "LC1160: Failed to detect GPIO input.\n");
		goto exit;
	}

	/* Request irq. */
	ret = request_irq(data->irq, lc1160_irq_handle,
			  IRQF_TRIGGER_HIGH | IRQF_SHARED, "LC1160 irq", data);
	if (ret) {
		printk(KERN_ERR "LC1160: IRQ already in use.\n");
		goto exit;
	}

	return 0;

exit:
	gpio_free(data->irq_gpio);
	return ret;
}

static void lc1160_rtc_year_check(void)
{
	int year;
	u8 val;

	/* Read rtc year */
	lc1160_reg_read(LC1160_REG_RTC_YEAR, &val);
	year = (val & LC1160_REG_BITMASK_RTC_YEAR);

	/* Check rtc year */
	if (year < (LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2)) {

		lc1160_reg_write(LC1160_REG_RTC_YEAR,
			LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2);

		/* Write data to RTC */
		lc1160_reg_write(LC1160_REG_RTC_TIME_UPDATE, 0x1a);

		/* Enable RTC */
		lc1160_reg_write(LC1160_REG_RTC_CTRL, 1);
	}

	return;
}

static void lc1160_alarm_year_check(u8 id)
{
	int year;
	u8 val;

	if (id >= LC1160_RTC_ALARM_NUM)
		return;

	if(id == 0) {
		lc1160_reg_read(LC1160_REG_RTC_AL1_YEAR, &val);
		year = val & LC1160_REG_BITMASK_RTC_AL1_YEAR;

		if (year < (LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2))
			lc1160_reg_write(LC1160_REG_RTC_AL1_YEAR,
				LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2);
	} else {
		lc1160_reg_read(LC1160_REG_RTC_AL2_YEAR, &val);
		year = val & LC1160_REG_BITMASK_RTC_AL2_YEAR;

		if (year < (LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2))
			lc1160_reg_write(LC1160_REG_RTC_AL2_YEAR,
				LC1160_RTC_YEAR_MIN2 - LC1160_RTC_YEAR_BASE2);
	}
	return;
}

static int lc1160_rtc_init(struct lc1160_pmic_data *data)
{
	/* Disable reboot rtc alarm. */
	lc1160_rtc_alarm_disable(PMIC_RTC_ALARM_REBOOT);

	/* RTC 4 timer faster bug version*/
	if (data->rtc_version == LC1160_RTC_FASTER_VER) {
		lc1160_rtc_year_check();
		lc1160_alarm_year_check(PMIC_RTC_ALARM_POWEROFF);
	}

	return 0;
}

static int lc1160_hw_init(struct lc1160_pmic_data *data)
{
	int i;
	int ret;
	u8 val;

	/* Mask all interrupts. */
	lc1160_int_mask(0xffffffff);

	/* Platform init, */
	for (i = 0; i < data->pdata->init_regs_num; i++)
		lc1160_reg_bit_write((u8)data->pdata->init_regs[i].reg,
					(u8)data->pdata->init_regs[i].mask,
					(u8)data->pdata->init_regs[i].value);

	if (data->pdata->init)
		data->pdata->init();

	/* Read version id. */
	lc1160_reg_read(LC1160_REG_VERSION, &data->version_id);

	data->rtc_version =lc1160_chip_version_get();

	data->rtc_version =
		(data->rtc_version == LC1160_ECO1) ? LC1160_RTC_NORMAL_VER : LC1160_RTC_FASTER_VER;

	/* RTC 4 timer faster bug version:
	 * set KEYON time 20s, actually 20 / 4 = 5s
	 */
	if (data->rtc_version == LC1160_RTC_FASTER_VER) {
		lc1160_reg_read(LC1160_REG_SHDN_EN, &val);
		val |= 1 << 6;
		lc1160_reg_write(LC1160_REG_SHDN_EN, val);
	}

	/* Read enable registers. */
	for (i = 0; i < LC1160_DIM(g_lc1160_eco_tab); i++) {
		if (g_lc1160_eco_tab[i] == data->version_id) {
			data->eco_no = i;
			break;
		}
	}

	printk(KERN_INFO "LC1160: version 0x%02x, eco no %d, rtc version %d\n",
		data->version_id, data->eco_no, data->rtc_version);

	/* Initialize LC1160 reboot. */
	lc1160_reboot_init(data);
	/* Initialize LC1160 RTC. */
	lc1160_rtc_init(data);

	/* Initialize LC1160 power. */
	lc1160_power_init(data);

	/* Initialize LC1160 irq. */
	ret = lc1160_irq_init(data);
	printk("%s %d\r\n",__func__,__LINE__);
	if (ret)
		goto exit;

	return 0;

exit:
	return ret;
}

static void lc1160_hw_exit(struct lc1160_pmic_data *data)
{
	/* Mask all interrupts. */
	lc1160_int_mask(0xffffffff);

	/* Free irq. */
	free_irq(data->irq, data);
	gpio_free(data->irq_gpio);
}

#ifdef CONFIG_COMIP_CALIBRATION
static int lc1160_calibration_notify(struct notifier_block *this,
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
		printk(KERN_INFO "LC1160: 32K clock rate %d, rem %d\n",
				freq_32k, rem);
		lc1160_reg_write(LC1160_REG_RTC_SEC_CYCL, (u8)(freq_32k & 0xff));
		lc1160_reg_write(LC1160_REG_RTC_SEC_CYCH, (u8)(freq_32k >> 8));
		lc1160_reg_write(LC1160_REG_RTC_32K_ADJL, (u8)(rem & 0xff));
		lc1160_reg_write(LC1160_REG_RTC_32K_ADJH, (u8)(rem >> 8));
		lc1160_reg_write(LC1160_REG_RTC_CALIB_UPDATE, 0x2a);
	}

	return NOTIFY_DONE;
}

static struct notifier_block lc1160_calibration_notifier = {
	.notifier_call = lc1160_calibration_notify,
};
#endif

#ifdef CONFIG_PM
static int lc1160_suspend(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static int lc1160_resume(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static struct dev_pm_ops lc1160_pm_ops = {
	.suspend = lc1160_suspend,
	.resume = lc1160_resume,
};
#endif

static int __init lc1160_pmic_probe(struct platform_device *pdev)
{
	struct lc1160_pmic_platform_data *pdata;
	struct lc1160_pmic_data *data;
	int err = 0;

	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		printk(KERN_ERR "LC1160-PMIC: No platform data\n");
		return -EINVAL;
	}

	if (!(data = kzalloc(sizeof(struct lc1160_pmic_data), GFP_KERNEL)))
		return -ENOMEM;

	data->wq = create_freezable_workqueue("lc1160");	/* report power-key event until system resume for fast boot */
	if (!data->wq) {
		err = -ENOMEM;
		goto exit;
	}

	/* Init real i2c_client */
	g_lc1160_pmic_data = data;
	data->pdata = pdata;
	data->irq_mask = 0xffffffff;
	data->irq_gpio = pdata->irq_gpio;
	data->irq = gpio_to_irq(pdata->irq_gpio);
	mutex_init(&data->power_lock);
	mutex_init(&data->adc_lock);
	mutex_init(&data->reg_lock);

#ifdef CONFIG_COMIP_CALIBRATION
	register_calibration_notifier(&lc1160_calibration_notifier);
#endif

	/* Initialize LC1160 hardware. */
	if ((err = lc1160_hw_init(data)) != 0)
		goto exit_wq;

	/* Set PMIC ops. */
	pmic_ops_set(&lc1160_pmic_ops);

	irq_set_irq_wake(data->irq, 1);

	return 0;

exit_wq:
	destroy_workqueue(data->wq);
exit:
	kfree(data);
	return err;
}

static int __exit lc1160_pmic_remove(struct platform_device *pdev)
{
	struct lc1160_pmic_data *data = platform_get_drvdata(pdev);

#ifdef CONFIG_COMIP_CALIBRATION
	unregister_calibration_notifier(&lc1160_calibration_notifier);
#endif

	/* Clean PMIC ops. */
	pmic_ops_set(NULL);

	/* Deinitialize LC1160 PMIC hardware. */
	if (data) {
		destroy_workqueue(data->wq);
		lc1160_hw_exit(data);
		kfree(data);
	}

	g_lc1160_pmic_data = NULL;

	return 0;
}

static struct platform_driver lc1160_pmic_device = {
	.remove = __exit_p(lc1160_pmic_remove),
	.driver = {
		.name = "lc1160-pmic",
#ifdef CONFIG_PM
		.pm = &lc1160_pm_ops,
#endif
	}
};

static int __init lc1160_pmic_init(void)
{
	return platform_driver_probe(&lc1160_pmic_device, lc1160_pmic_probe);
}

static void __exit lc1160_pmic_exit(void)
{
	platform_driver_unregister(&lc1160_pmic_device);
}

subsys_initcall(lc1160_pmic_init);
module_exit(lc1160_pmic_exit);

MODULE_AUTHOR("fanjiangang <fanjiangang@leadcoretech.com>");
MODULE_DESCRIPTION("LC1160 PMIC driver");
MODULE_LICENSE("GPL");
