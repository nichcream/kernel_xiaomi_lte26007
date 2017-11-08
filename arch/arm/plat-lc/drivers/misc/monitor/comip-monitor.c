/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains leadcore monitor driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2013-05-31	tuzhiqiang	created
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <plat/comip-monitor.h>

#define COMIP_MONITOR_MODULE_NAME_MAX	(20)

struct monitor_timer {
	char name[COMIP_MONITOR_MODULE_NAME_MAX];
	struct hrtimer *hrtimer;
	struct list_head entry;
};

struct monitor_info {
	struct list_head list;
	spinlock_t lock;
	u8 panic;
};

static struct monitor_info *monitor_info = NULL;

static int comip_monitor_panic_cb(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct monitor_timer *monitor_timer;

	pr_info("%s \n", __func__);

	if (!monitor_info->panic) {
		monitor_info->panic = 1;
		list_for_each_entry(monitor_timer, &monitor_info->list, entry)
			hrtimer_cancel(monitor_timer->hrtimer);
	}

	return NOTIFY_DONE;
}

static enum hrtimer_restart comip_monitor_timer_fn(struct hrtimer *hrtimer)
{
	struct monitor_timer *monitor_timer;

	monitor_info->panic = 1;

	list_for_each_entry(monitor_timer, &monitor_info->list, entry) {
		if (monitor_timer->hrtimer == hrtimer)
			pr_info("!!![%s] monitor timer is expired \n", monitor_timer->name);
		else
			hrtimer_cancel(monitor_timer->hrtimer);
	}

	panic("comip monitor hrtimer is expired \n");

	return HRTIMER_NORESTART;
}

struct hrtimer *monitor_timer_register(const char *name)
{
	struct monitor_timer *monitor_timer;
	struct hrtimer *hrtimer;
	unsigned long flags;

	if (!monitor_info) {
		pr_err("%s: module is not init \n", __func__);
		return NULL;
	}

	if (IS_ERR_OR_NULL(name)) {
		pr_err("%s: input parameter is invalid \n", __func__);
		return NULL;
	}

	if (strlen(name) >= COMIP_MONITOR_MODULE_NAME_MAX) {
		pr_err("%s: name length is too long \n", __func__);
		return NULL;
	}

	list_for_each_entry(monitor_timer, &monitor_info->list, entry)
		if (!strcmp(monitor_timer->name, name)) {
			pr_err("%s: this module has registered \n", __func__);
			return NULL;
		}

	monitor_timer = kmalloc(sizeof(struct monitor_timer), GFP_KERNEL);
	if (!monitor_timer) {
		pr_err("%s: kmalloc monitor_timer failed \n", __func__);
		return NULL;
	}

	hrtimer = kmalloc(sizeof(struct hrtimer), GFP_KERNEL);
	if (!hrtimer) {
		pr_err("%s: kmalloc hrtimer failed \n", __func__);
		return NULL;
	}

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = comip_monitor_timer_fn;

	strcpy(monitor_timer->name, name);
	monitor_timer->hrtimer = hrtimer;
	spin_lock_irqsave(&monitor_info->lock, flags);
	list_add_tail(&monitor_timer->entry, &monitor_info->list);
	spin_unlock_irqrestore(&monitor_info->lock, flags);

	return hrtimer;
}
EXPORT_SYMBOL_GPL(monitor_timer_register);

int monitor_timer_unregister(const char *name)
{
	struct monitor_timer *monitor_timer;
	unsigned long flags;

	if (!monitor_info) {
		pr_err("%s: module is not init \n", __func__);
		return -ENODEV;
	}

	list_for_each_entry(monitor_timer, &monitor_info->list, entry)
		if (!strcmp(monitor_timer->name, name)) {
			spin_lock_irqsave(&monitor_info->lock, flags);
			list_del(&monitor_timer->entry);
			spin_unlock_irqrestore(&monitor_info->lock, flags);
			hrtimer_cancel(monitor_timer->hrtimer);
			kfree(monitor_timer->hrtimer);
			kfree(monitor_timer);
			break;
		}

	return 0;
}
EXPORT_SYMBOL_GPL(monitor_timer_unregister);

int monitor_timer_start(struct hrtimer *hrtimer, long secs)
{
	if (!monitor_info) {
		pr_err("%s: module is not init \n", __func__);
		return -ENODEV;
	}

	if (monitor_info->panic) {
		pr_err("%s: has panic", __func__);
		return -EBUSY;
	}

	if (IS_ERR_OR_NULL(hrtimer)) {
		pr_err("%s: hrtimer is invalid \n", __func__);
		return -EINVAL;
	}

	hrtimer_start(hrtimer, ktime_set(secs, 0), HRTIMER_MODE_REL);

	return 0;
}
EXPORT_SYMBOL_GPL(monitor_timer_start);

int monitor_timer_stop(struct hrtimer *hrtimer)
{
	if (!monitor_info) {
		pr_err("%s: module is not init \n", __func__);
		return -ENODEV;
	}

	if (monitor_info->panic) {
		pr_err("%s: has panic", __func__);
		return -EBUSY;
	}

	if (IS_ERR_OR_NULL(hrtimer)) {
		pr_err("%s: hrtimer is invalid \n", __func__);
		return -EINVAL; 
	}

	hrtimer_cancel(hrtimer);

	return 0;
}
EXPORT_SYMBOL_GPL(monitor_timer_stop);

static int comip_monitor_probe(struct platform_device *pdev)
{
	struct monitor_info *info;

	pr_info("%s \n", __func__);

	info = kzalloc(sizeof(struct monitor_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: kzalloc failed \n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&info->list);

	info->panic = 0;
	spin_lock_init(&info->lock);

	monitor_info = info;

	platform_set_drvdata(pdev, info);

	return 0;
}

static int comip_monitor_remove(struct platform_device *pdev)
{
	struct monitor_info *info = platform_get_drvdata(pdev);

	if (info) {
		platform_set_drvdata(pdev, NULL);
		kfree(info);
		monitor_info = NULL;
	}

	return 0;
}

static struct platform_device comip_monitor_device = {
	.name = "comip-monitor",
	.id = -1,
};

static struct platform_driver comip_monitor_driver = {
	.probe = comip_monitor_probe,
	.remove = comip_monitor_remove,
	.driver = {
		.name = "comip-monitor",
	},
};

static struct notifier_block monitor_nb = {
	.notifier_call	= comip_monitor_panic_cb,
	.priority = 1,
};

static int __init monitor_init(void)
{
	atomic_notifier_chain_register(&panic_notifier_list, &monitor_nb);
	platform_device_register(&comip_monitor_device);
	return platform_driver_register(&comip_monitor_driver);
}

static void __exit monitor_exit(void)
{
	platform_device_unregister(&comip_monitor_device);
	platform_driver_unregister(&comip_monitor_driver);
}

subsys_initcall(monitor_init);
module_exit(monitor_exit);

