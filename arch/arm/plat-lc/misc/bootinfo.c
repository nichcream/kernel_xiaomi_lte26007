/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip boot information.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date		Author		Description
 *	1.0.0	2012-03-06	zouqiao		created
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <asm/setup.h>
#include <asm/sections.h>

#include <plat/bootinfo.h>
#include <plat/comip-setup.h>
#include <plat/comip-pmic.h>

static const char * const boot_reason_strings[PU_REASON_MAX] = {
	[PU_REASON_HARDWARE_RESET]	= "hardware_reset",
	[PU_REASON_WATCHDOG]		= "watchdog",
	[PU_REASON_RTC_ALARM]		= "rtc_alarm",
	[PU_REASON_PWR_KEY_PRESS]	= "power_key",
	[PU_REASON_USB_CABLE]		= "usb_cable",
	[PU_REASON_USB_FACTORY]		= "usb_factory",
	[PU_REASON_USB_CHARGER]		= "usb_charger",
	[PU_REASON_REBOOT_NORMAL]	= "reboot_normal",
	[PU_REASON_REBOOT_FACTORY]	= "reboot_factory",
	[PU_REASON_REBOOT_RECOVERY]	= "reboot_recovery",
	[PU_REASON_REBOOT_FOTA]		= "reboot_fota",
	[PU_REASON_REBOOT_CRITICAL]	= "reboot_critical",
	[PU_REASON_REBOOT_UNKNOWN]	= "reboot_unknown",
	[PU_REASON_LOW_PWR_RESET]	= "lowpower_reset",
};

static const char * const reboot_reason_strings[REBOOT_MAX] = {
	[REBOOT_NONE]		= "null",
	[REBOOT_NORMAL]		= "normal",
	[REBOOT_POWER_KEY]	= "power_key",
	[REBOOT_RTC_ALARM]	= "rtc_alarm",
	[REBOOT_RECOVERY]	= "recovery",
	[REBOOT_FOTA]		= "fota",
	[REBOOT_CRITICAL]	= "critical",
	[REBOOT_FACTORY]	= "factory",
	[REBOOT_UNKNOWN]	= "unknown",
};

static const char * const boot_mode_strings[BOOT_MODE_MAX] = {
	[BOOT_MODE_NORMAL]	= "normal",
	[BOOT_MODE_AMT1]	= "amt1",
	[BOOT_MODE_AMT3]	= "amt3",
	[BOOT_MODE_RECOVERY]	= "recovery",
};

static struct kobject *bootinfo_kobj = NULL;

#define bootinfo_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define bootinfo_func_init(type, name, initval)   	\
	static type name = (initval);			\
	type get_##name(void)				\
	{						\
		return name;				\
	}						\
	const char* get_##name##_string(type __##name)	\
	{						\
		return name##_strings[__##name];	\
	}						\
	void set_##name(type __##name)			\
	{						\
		name = __##name;			\
	}						\
	EXPORT_SYMBOL(set_##name);			\
	EXPORT_SYMBOL(get_##name);

bootinfo_func_init(u32, boot_reason, 0);
bootinfo_func_init(u32, reboot_reason, 0);
bootinfo_func_init(u32, boot_mode, 0);

static ssize_t powerup_reason_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	u32 pu_reason = get_boot_reason();

	if(pu_reason < PU_REASON_MAX && pu_reason >= 0 )
		s += sprintf(s, "%s", boot_reason_strings[pu_reason]);
	else
		s += sprintf(s, "%s", boot_reason_strings[PU_REASON_HARDWARE_RESET]);

	return (s - buf);
}

static ssize_t powerup_reason_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}

bootinfo_attr(powerup_reason);

static ssize_t reboot_reason_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	int i;
	unsigned int val;

	val = get_reboot_reason();

	for (i = 0; i < REBOOT_MAX; i++) {
		s += sprintf(s,"%s ", reboot_reason_strings[i]);
		if(val == i)
			s += sprintf(s, "[Active]\n");
		else
			s += sprintf(s, "\n");
	}

	return (s - buf);
}

static ssize_t reboot_reason_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}

bootinfo_attr(reboot_reason);

static ssize_t boot_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	int i;
	unsigned int val;

	val = get_boot_mode();

	for (i = 0; i < REBOOT_MAX; i++) {
		s += sprintf(s,"%s ", boot_mode_strings[i]);
		if(val == i)
			s += sprintf(s, "[Active]\n");
		else
			s += sprintf(s, "\n");
	}

	return (s - buf);
}

static ssize_t boot_mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}

bootinfo_attr(boot_mode);

static struct attribute * g[] = {
	&powerup_reason_attr.attr,
	&reboot_reason_attr.attr,
	&boot_mode_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init get_boot_info(const struct tag *tag)
{
	struct tag_boot_info *binfo = (struct tag_boot_info *)&tag->u;

	if (binfo->boot_mode < BOOT_MODE_MAX) {
		printk(KERN_DEBUG "Boot mode: %s\n",
				get_boot_mode_string(binfo->boot_mode));
		set_boot_mode(binfo->boot_mode);
	} else
		printk(KERN_ERR "Invalid boot mode: %d\n", binfo->boot_mode);

	return 0;
}
__tagtable(ATAG_BOOT_INFO, get_boot_info);

static int __init get_buck1_info(const struct tag *tag)
{
	struct tag_buck1_voltage *vol = (struct tag_buck1_voltage *)&tag->u;
	printk(KERN_INFO "Main VOUT: %d/%d\n",
		vol->vout0, vol->vout1);
	return 0;
}
__tagtable(ATAG_BUCK1_VOLTAGE, get_buck1_info);

extern struct mutex reboot_mutex;

#if defined(CONFIG_COMIP_PMIC_REBOOT)
static void bootinfo_reboot_pmic_set(void)
{
        struct rtc_wkalrm alrm;
        unsigned long time;
        pmic_rtc_time_get(&alrm.time);

        rtc_tm_to_time(&alrm.time, &time);
        time += 3;
        rtc_time_to_tm(time, &alrm.time);

        alrm.enabled = 1;
        pmic_rtc_alarm_set(PMIC_RTC_ALARM_REBOOT, &alrm);
}
#endif

static int bootinfo_reboot_notifier(struct notifier_block *nb,
				unsigned long action, void *p)
{
	if (action == SYS_RESTART) {
		u8 reboot_reason = REBOOT_NORMAL;
		char *cmd = (char *)p;

		if (cmd) {
			if (!strcmp(cmd, "recovery"))
				reboot_reason = REBOOT_RECOVERY;
			else if (!strcmp(cmd, "power_key"))
				reboot_reason = REBOOT_POWER_KEY;
			else if (!strcmp(cmd, "rtc_alarm"))
				reboot_reason = REBOOT_RTC_ALARM;
			else if (!strcmp(cmd, "panic"))
				reboot_reason = REBOOT_CRITICAL;
			else
				reboot_reason = REBOOT_UNKNOWN;
		}

		if (mutex_is_locked(&reboot_mutex))
			reboot_reason |= REBOOT_USER_FLAG;

		pmic_reboot_type_set(reboot_reason);
#if defined(CONFIG_COMIP_PMIC_REBOOT)
                bootinfo_reboot_pmic_set();
#endif
	}

	return NOTIFY_DONE;
}

static struct notifier_block bootinfo_reboot_nb = {
	.notifier_call = bootinfo_reboot_notifier,
};

static int __init bootinfo_init(void)
{
	int ret = -ENOMEM;

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (bootinfo_kobj == NULL) {
		printk("bootinfo_init: kobject_create_and_add failed\n");
		goto fail;
	}

	ret = sysfs_create_group(bootinfo_kobj, &attr_group);
	if (ret) {
		printk("bootinfo_init: sysfs_create_group failed\n");
		goto sys_fail;
	}

	ret = register_reboot_notifier(&bootinfo_reboot_nb);
	if (ret) {
		printk("bootinfo_init: register_reboot_notifier failed\n");
		goto notifier_fail;
	}

	return ret;

notifier_fail:
	sysfs_remove_group(bootinfo_kobj, &attr_group);
sys_fail:
	kobject_del(bootinfo_kobj);
fail:
	return ret;

}

static void __exit bootinfo_exit(void)
{
	unregister_reboot_notifier(&bootinfo_reboot_nb);

	if (bootinfo_kobj) {
		sysfs_remove_group(bootinfo_kobj, &attr_group);
		kobject_del(bootinfo_kobj);
	}
}

core_initcall(bootinfo_init);
module_exit(bootinfo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Boot information collector");
