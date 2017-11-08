/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *  PURPOSE: This files contains the comip sysfs driver.
 *
 *  CHANGE HISTORY:
 *
 *  Version	Date		Author		Description
 *  1.0.0	2012-03-06	zouqiao		created
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <plat/bootinfo.h>
#include <plat/comip-pmic.h>

#define comip_attr(_name, _mode) \
	static struct kobj_attribute _name##_attr = {	\
		.attr	= { 			\
			.name = __stringify(_name), \
			.mode = _mode,			\
		},					\
		.show	= _name##_show, 		\
		.store	= _name##_store,		\
	}

/*
	Setting value 	: Voltage(mv)
	0 			: 0
	1 			: 1800
	2 			: 3000
	3 			: 5000
*/
static ssize_t usim_voltage_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int ret;
	int value;
	int voltage = -1;

	ret = pmic_voltage_get(PMIC_POWER_USIM, 0, &voltage);
	if (ret)
		return ret;

	switch(voltage) {
		case 0:
			value = 0;
			break;
		case 1800:
			value = 1;
			break;
		case 3000:
			value = 2;
			break;
		case 5000:
		default:
			return -EINVAL;
	}

	return sprintf(buf, "%d", value);
}

static ssize_t usim_voltage_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	char *endp;
	int ret;
	int value;
	int voltage;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	switch(value) {
		case 0:
			voltage = 0;
			break;
		case 1:
			voltage = 1800;
			break;
		case 2:
			voltage = 3000;
			break;
		case 3:
		default:
			return -EINVAL;
	}

	ret = pmic_voltage_set(PMIC_POWER_USIM, 0, voltage);
	if (ret)
		return ret;

	return n;
}

comip_attr(usim_voltage, 0644);

static ssize_t usim2_voltage_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int ret;
	int value;
	int voltage = -1;

	ret = pmic_voltage_get(PMIC_POWER_USIM, 1, &voltage);
	if (ret)
		return ret;

	switch(voltage) {
		case 0:
			value = 0;
			break;
		case 1800:
			value = 1;
			break;
		case 3000:
			value = 2;
			break;
		case 5000:
		default:
			return -EINVAL;
	}

	return sprintf(buf, "%d", value);
}

static ssize_t usim2_voltage_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	char *endp;
	int ret;
	int value;
	int voltage;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	switch(value) {
		case 0:
			voltage = 0;
			break;
		case 1:
			voltage = 1800;
			break;
		case 2:
			voltage = 3000;
			break;
		case 3:
		default:
			return -EINVAL;
	}

	ret = pmic_voltage_set(PMIC_POWER_USIM, 1, voltage);
	if (ret)
		return ret;

	return n;
}

comip_attr(usim2_voltage, 0644);

static ssize_t td_voltage_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int ret;
	int value;
	int voltage = -1;

	ret = pmic_voltage_get(PMIC_POWER_RF_TDD_CORE, 0, &voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	if (ret == -ENXIO) {
		ret = pmic_voltage_get(PMIC_POWER_RF_TDD_IO, 0, &voltage);
		if (ret && (ret != -ENXIO))
			return ret;
	}

	if (!ret)
		value = voltage ? -1 : 0;
	else
		value = 0;

	return sprintf(buf, "%d", value);
}

static ssize_t td_voltage_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	char *endp;
	int ret;
	int value;
	int voltage;

	value = simple_strtol(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	voltage = value ? PMIC_POWER_VOLTAGE_ENABLE : PMIC_POWER_VOLTAGE_DISABLE;
	ret = pmic_voltage_set(PMIC_POWER_RF_TDD_CORE, 0, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	ret = pmic_voltage_set(PMIC_POWER_RF_TDD_IO, 0, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	return n;
}

comip_attr(td_voltage, 0644);

static ssize_t td2_voltage_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int ret;
	int value;
	int voltage = -1;

	ret = pmic_voltage_get(PMIC_POWER_RF_TDD_CORE, 1, &voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	if (ret == -ENXIO) {
		ret = pmic_voltage_get(PMIC_POWER_RF_TDD_IO, 1, &voltage);
		if (ret && (ret != -ENXIO))
			return ret;
	}

	if (!ret)
		value = voltage ? -1 : 0;
	else
		value = 0;

	return sprintf(buf, "%d", value);
}

static ssize_t td2_voltage_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	char *endp;
	int ret;
	int value;
	int voltage;

	value = simple_strtol(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	voltage = value ? PMIC_POWER_VOLTAGE_ENABLE : PMIC_POWER_VOLTAGE_DISABLE;
	ret = pmic_voltage_set(PMIC_POWER_RF_TDD_CORE, 1, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	ret = pmic_voltage_set(PMIC_POWER_RF_TDD_IO, 1, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	return n;
}

comip_attr(td2_voltage, 0644);

static ssize_t gsm_voltage_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int ret;
	int value;
	int voltage = -1;

	ret = pmic_voltage_get(PMIC_POWER_RF_GSM_CORE, 0, &voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	if (ret == -ENXIO) {
		ret = pmic_voltage_get(PMIC_POWER_RF_GSM_IO, 0, &voltage);
		if (ret && (ret != -ENXIO))
			return ret;
	}

	if (!ret)
		value = voltage ? -1 : 0;
	else
		value = 0;

	return sprintf(buf, "%d", value);
}

static ssize_t gsm_voltage_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	char *endp;
	int ret;
	int value;
	int voltage;

	value = simple_strtol(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	voltage = value ? PMIC_POWER_VOLTAGE_ENABLE : PMIC_POWER_VOLTAGE_DISABLE;
	ret = pmic_voltage_set(PMIC_POWER_RF_GSM_CORE, 0, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	ret = pmic_voltage_set(PMIC_POWER_RF_GSM_IO, 0, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	return n;
}

comip_attr(gsm_voltage, 0644);

static ssize_t gsm2_voltage_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	int ret;
	int value;
	int voltage = -1;

	ret = pmic_voltage_get(PMIC_POWER_RF_GSM_CORE, 1, &voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	if (ret == -ENXIO) {
		ret = pmic_voltage_get(PMIC_POWER_RF_GSM_IO, 1, &voltage);
		if (ret && (ret != -ENXIO))
			return ret;
	}

	if (!ret)
		value = voltage ? -1 : 0;
	else
		value = 0;

	return sprintf(buf, "%d", value);
}

static ssize_t gsm2_voltage_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	char *endp;
	int ret;
	int value;
	int voltage;

	value = simple_strtol(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	voltage = value ? PMIC_POWER_VOLTAGE_ENABLE : PMIC_POWER_VOLTAGE_DISABLE;
	ret = pmic_voltage_set(PMIC_POWER_RF_GSM_CORE, 1, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	ret = pmic_voltage_set(PMIC_POWER_RF_GSM_IO, 1, voltage);
	if (ret && (ret != -ENXIO))
		return ret;

	return n;
}

comip_attr(gsm2_voltage, 0644);

static ssize_t rtc_alarm_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	struct rtc_wkalrm alrm;
	unsigned long time;

	pmic_rtc_alarm_get(PMIC_RTC_ALARM_POWEROFF, &alrm);
	if (alrm.enabled)
		rtc_tm_to_time(&alrm.time, &time);
	else
		time = 0;

	return sprintf(buf, "%ld", time);
}

static ssize_t rtc_alarm_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	struct rtc_wkalrm alrm;
	unsigned long time;
	char *endp;

	time = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	if (time == 0) {
		memset(&alrm, 0, sizeof(alrm));
		alrm.enabled = 0;
	} else {
		rtc_time_to_tm(time, &alrm.time);
		alrm.enabled = 1;
	}
	pmic_rtc_alarm_set(PMIC_RTC_ALARM_POWEROFF, &alrm);

	return n;
}

comip_attr(rtc_alarm, S_IRUGO|S_IWUSR);

extern int comip_board_info_get(char *name);
int __attribute__((weak)) comip_board_info_get(char *name)
{
	if(name) {
		strcpy(name, "UNKNOW");
	}

	return -1;
}

static ssize_t board_info_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char name[20];
	comip_board_info_get(name);
	return sprintf(buf, "%s", name);
}

static ssize_t board_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}

comip_attr(board_info, 644);


static unsigned long rtc_alarm_event = 0;

static void rtc_alarm_event_callback(int event, void *puser, void* pargs)
{
	rtc_alarm_event = 1;

	if (PU_REASON_USB_CHARGER == get_boot_reason())
		kernel_restart("rtc_alarm");
}

static ssize_t rtc_alarm_event_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	return sprintf(buf, "%ld", rtc_alarm_event);
}

static ssize_t rtc_alarm_event_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	unsigned long event;
	char *endp;

	event = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rtc_alarm_event = event;

	return n;
}

comip_attr(rtc_alarm_event, 0664);

#if defined(CONFIG_CPU_LC1810)
static unsigned long jtag_switch_flag = 1;

static int jtag_switch_do(int on)
{
	unsigned int value;
	unsigned int cnt = 1000;
	int status = 0;

	if (on) {

		/*enable CS_HCLK */
		value = 1 << 5 | 1 << (5 + 16);
		__raw_writel(value, io_p2v(AP_PWR_SYSCLK_EN2));
		dsb();

		/*enable ATB clk*/
		value = 8 << 0 | 1 << 16;
		value |= 1 << 8 | 1 << 18;
		__raw_writel(value, io_p2v(AP_PWR_ATBCLK_CTL));

		/*enable dbg_pclk clk*/
		value = 1 << 0 | 1 << 16;
		__raw_writel(value, io_p2v(AP_PWR_A9DBGPCLK_CTL));
		dsb();

		/*powered-up PTM*/
		value = __raw_readl(io_p2v(AP_PWR_INT_RAW));
		if (value & (1 << AP_PWR_PTM_PU_INTR)) {
			value = __raw_readl(io_p2v(AP_PWR_INTST_A9));
			value |= 1 << AP_PWR_PTM_PU_INTR;
			__raw_writel(value, io_p2v(AP_PWR_INTST_A9));
		}

		value = 1 << 1 | 1 << (1 + 8);
		__raw_writel(value, io_p2v(AP_PWR_PTM_PD_CTL));
		dsb();

		/*about 100*/
		while (--cnt > 0) {
			value = __raw_readl(io_p2v(AP_PWR_INT_RAW));
			if (value & (1 << AP_PWR_PTM_PU_INTR)) {
				break;
			}
			udelay(50);
		}

		if (!cnt) {
			printk(KERN_ERR "jtag_switch_do: power-up failed.\n");
			status = -ETIMEDOUT;
		} else {
			value |= 1 << AP_PWR_PTM_PU_INTR;
			__raw_writel(value, io_p2v(AP_PWR_INTST_A9));
		}

		value = 0 << 1 | 1 << (1 + 8);
		__raw_writel(value, io_p2v(AP_PWR_PTM_PD_CTL));
		dsb();

	}else {

		/*powered-off PTM*/
		value = __raw_readl(io_p2v(AP_PWR_INT_RAW));
		if (value & (1 << AP_PWR_PTM_PD_INTR)) {
			value = __raw_readl(io_p2v(AP_PWR_INTST_A9));
			value |= 1 << AP_PWR_PTM_PD_INTR;
			__raw_writel(value, io_p2v(AP_PWR_INTST_A9));
		}

		value = 0 << 3 |  1 << (3 + 8);
		__raw_writel(value, io_p2v(AP_PWR_PTM_PD_CTL));
		dsb();

		value = 1 << 0 | 1 << (0 + 8);
		__raw_writel(value, io_p2v(AP_PWR_PTM_PD_CTL));
		dsb();

		/*about 200*/
		while (--cnt > 0) {
			value = __raw_readl(io_p2v(AP_PWR_INT_RAW));
			if (value & (1 << AP_PWR_PTM_PD_INTR)) {
				break;
			}
			udelay(50);
		}

		if (!cnt) {
			printk(KERN_ERR "jtag_switch_do: power-down failed.\n");
			status = -ETIMEDOUT;
		} else {
			value |= 1 << AP_PWR_PTM_PD_INTR;
			__raw_writel(value, io_p2v(AP_PWR_INTST_A9));
		}

		value = 0 << 0 | 1 << (0 + 8);
		__raw_writel(value, io_p2v(AP_PWR_PTM_PD_CTL));
		dsb();

		/*shutdown ATB clk*/
		value = 0 << 8 | 1 << 18;
		__raw_writel(value, io_p2v(AP_PWR_ATBCLK_CTL));
		value = 0 << 0 | 1 << 16;
		__raw_writel(value, io_p2v(AP_PWR_ATBCLK_CTL));

		/*shutdown dbg_pclk clk*/
		value = 0 << 0 | 1 << 16;
		__raw_writel(value, io_p2v(AP_PWR_A9DBGPCLK_CTL));

		/*shutdown CS_HCLK */
		value = 0 << 5 | 1 << (5 + 16);
		__raw_writel(value, io_p2v(AP_PWR_SYSCLK_EN2));
		dsb();
	}

	printk("jtag_switch_do: cnt = %d.\n", cnt);
	return status;
}

static inline int jtag_switch_set(int on)
{
	if (jtag_switch_flag != on) {
		jtag_switch_do(on);
		jtag_switch_flag = on;
		wmb();
	}
	return 0;
}

static ssize_t jtag_switch_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	rmb();
	return sprintf(buf, "%ld", jtag_switch_flag);
}

static ssize_t jtag_switch_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	unsigned long on;
	char *endp;

	on = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	jtag_switch_set(on);

	return n;
}
static inline void jtag_switch_init(void)
{
#ifdef CONFIG_COMIP_DISABLE_JTAG
	int on = 0;
#else
	int on = 1;
#endif
	unsigned int value;

	/*disable PD/PU intterupt*/
	value = __raw_readl(io_p2v(AP_PWR_INTEN_A9));
	value &= ~(1 << AP_PWR_PTM_PD_INTR | 1 << AP_PWR_PTM_PU_INTR);
	__raw_writel(value, io_p2v(AP_PWR_INTEN_A9));
	dsb();

	jtag_switch_set(on);
}


comip_attr(jtag_switch, 0664);
#endif

static struct kobject *comip_kobj = NULL;

static struct attribute * comip_attrs[] = {
	&td_voltage_attr.attr,
	&td2_voltage_attr.attr,
	&gsm_voltage_attr.attr,
	&gsm2_voltage_attr.attr,
	&usim_voltage_attr.attr,
	&usim2_voltage_attr.attr,
	&rtc_alarm_attr.attr,
	&rtc_alarm_event_attr.attr,
	&board_info_attr.attr,
#if defined(CONFIG_CPU_LC1810)
	&jtag_switch_attr.attr,
#endif
	NULL,
};

static struct attribute_group comip_attr_group = {
	.attrs = comip_attrs,
};

static int __init comip_sysfs_init(void)
{
	int ret = -ENOMEM;
	int event = PMIC_RTC_ALARM_POWEROFF ? PMIC_EVENT_RTC2 : PMIC_EVENT_RTC1;

	ret = pmic_callback_register(event, NULL, rtc_alarm_event_callback);
	if (ret) {
		printk(KERN_ERR "comip_sysfs_init: pmic_callback_register failed.\n");
		goto failed;
	}

	pmic_callback_unmask(event);

#if defined(CONFIG_CPU_LC1810)
	jtag_switch_init();
#endif

	comip_kobj = kobject_create_and_add("comip", NULL);
	if (comip_kobj == NULL) {
		printk(KERN_ERR "comip_sysfs_init: kobject_create_and_add failed.\n");
		goto kobject_create_failed;
	}

	ret = sysfs_create_group(comip_kobj, &comip_attr_group);
	if (ret) {
		printk(KERN_ERR "comip_sysfs_init: sysfs_create_group failed.\n");
		goto sysfs_create_failed;
	}

	return ret;

sysfs_create_failed:
	kobject_del(comip_kobj);
kobject_create_failed:
	pmic_callback_mask(event);
	pmic_callback_unregister(event);
failed:
	return ret;
}

static void __exit comip_sysfs_exit(void)
{
	int event = PMIC_RTC_ALARM_POWEROFF ? PMIC_EVENT_RTC2 : PMIC_EVENT_RTC1;

	pmic_callback_mask(event);
	pmic_callback_unregister(event);

	if (comip_kobj) {
		sysfs_remove_group(comip_kobj, &comip_attr_group);
		kobject_del(comip_kobj);
		comip_kobj = NULL;
	}
}

module_init(comip_sysfs_init);
module_exit(comip_sysfs_exit);


MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP sysfs");
MODULE_LICENSE("GPL");
