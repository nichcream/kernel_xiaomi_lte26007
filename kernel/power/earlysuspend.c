/* kernel/power/earlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/syscalls.h> /* sys_sync */
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include "power.h"

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
};
static int debug_mask = DEBUG_USER_STATE;

struct early_drv_wd_data {
	int level;
	void *func;
	struct task_struct *tsk;
};

static struct workqueue_struct *suspend_work_queue;
static suspend_state_t requested_suspend_state = PM_SUSPEND_MEM;
static struct wake_lock main_wake_lock;

static void earlysuspend_drv_timeout(unsigned long data)
{
	struct early_drv_wd_data *wd_data = (void *)data;
	struct task_struct *tsk = wd_data->tsk;

	printk(KERN_EMERG "device timeout %pf\n", wd_data->func);
	printk(KERN_EMERG "fucntion level is %d\n", wd_data->level);
	printk(KERN_EMERG "earlysuspend  stack:\n");
	show_stack(tsk, NULL);

	BUG();
}

static void lateresume_drv_timeout(unsigned long data)
{
	struct early_drv_wd_data *wd_data = (void *)data;
	struct task_struct *tsk = wd_data->tsk;

	printk(KERN_EMERG "device timeout %pf\n", wd_data->func);
	printk(KERN_EMERG "fucntion level is %d\n", wd_data->level);
	printk(KERN_EMERG "lateresume  stack:\n");
	show_stack(tsk, NULL);

	BUG();
}

static void timer_start(struct timer_list* timer,  struct early_drv_wd_data* data, void* func)
{
	unsigned long expires = jiffies + HZ * 12;

	data->tsk = get_current();
	init_timer_on_stack(timer);
	timer->expires = expires;
	timer->function = func;
	timer->data = (unsigned long)data;
	add_timer(timer);
}

static void timer_end(struct timer_list* timer)
{
	del_timer_sync(timer);
	destroy_timer_on_stack(timer);
}

module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static DEFINE_MUTEX(early_suspend_lock);
static LIST_HEAD(early_suspend_handlers);
static void early_suspend(struct work_struct *work);
static void late_resume(struct work_struct *work);
static DECLARE_WORK(early_suspend_work, early_suspend);
static DECLARE_WORK(late_resume_work, late_resume);
static DEFINE_SPINLOCK(state_lock);
enum {
	SUSPEND_REQUESTED = 0x1,
	SUSPENDED = 0x2,
	SUSPEND_REQUESTED_AND_SUSPENDED = SUSPEND_REQUESTED | SUSPENDED,
};
static int state;

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;
		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	if ((state & SUSPENDED) && handler->suspend)
		handler->suspend(handler);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static void early_suspend(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

	struct timer_list timer;
	struct early_drv_wd_data data;

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED)
		state |= SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("early_suspend: abort, state %d\n", state);
		mutex_unlock(&early_suspend_lock);
		goto abort;
	}

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: call handlers\n");
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("early_suspend: calling %pf\n", pos->suspend);

			data.level = pos->level;
			data.func = pos->suspend;
			timer_start(&timer, &data, earlysuspend_drv_timeout);
			pos->suspend(pos);
			timer_end(&timer);
		}
	}
	mutex_unlock(&early_suspend_lock);

/*
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: sync\n");

	sys_sync();
*/
abort:
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED_AND_SUSPENDED)
#ifdef CONFIG_SMARTMIFI_DEBUG
		printk(KERN_ERR"debug version for smartmifi\n");
#else
		wake_unlock(&main_wake_lock);
#endif
	spin_unlock_irqrestore(&state_lock, irqflags);
}

static void late_resume(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;
	struct timer_list timer;
	struct early_drv_wd_data data;

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPENDED)
		state &= ~SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("late_resume: abort, state %d\n", state);
		goto abort;
	}
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: call handlers\n");
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link) {
		if (pos->resume != NULL) {
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("late_resume: calling %pf\n", pos->resume);

			data.level = pos->level;
			data.func = pos->resume;
			timer_start(&timer, &data, lateresume_drv_timeout);
			pos->resume(pos);
			timer_end(&timer);
		}
	}
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: done\n");
abort:
	mutex_unlock(&early_suspend_lock);
}

void request_suspend_state(suspend_state_t new_state)
{
	unsigned long irqflags;
	int old_sleep;

	spin_lock_irqsave(&state_lock, irqflags);
	old_sleep = state & SUSPEND_REQUESTED;
	if (debug_mask & DEBUG_USER_STATE) {
		struct timespec ts;
		struct rtc_time tm;
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
		pr_info("request_suspend_state: %s (%d->%d) at %lld "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			new_state != PM_SUSPEND_ON ? "sleep" : "wakeup",
			requested_suspend_state, new_state,
			ktime_to_ns(ktime_get()),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	}
	if (!old_sleep && new_state != PM_SUSPEND_ON) {
		state |= SUSPEND_REQUESTED;
		queue_work(suspend_work_queue, &early_suspend_work);
	} else if (old_sleep && new_state == PM_SUSPEND_ON) {
		state &= ~SUSPEND_REQUESTED;
		wake_lock(&main_wake_lock);
		queue_work(suspend_work_queue, &late_resume_work);
	}
	requested_suspend_state = new_state;
	spin_unlock_irqrestore(&state_lock, irqflags);
}

suspend_state_t get_suspend_state(void)
{
	return requested_suspend_state;
}

static int __init earlysuspend_init(void)
{
	wake_lock_init(&main_wake_lock, WAKE_LOCK_SUSPEND, "main");
	wake_lock(&main_wake_lock);

	suspend_work_queue = create_singlethread_workqueue("suspend");
	if (suspend_work_queue == NULL) {
		return -ENOMEM;
	}
	return 0;
}

static void  __exit earlysuspend_exit(void)
{
	wake_lock_destroy(&main_wake_lock);
	destroy_workqueue(suspend_work_queue);
}

core_initcall(earlysuspend_init);
module_exit(earlysuspend_exit);

