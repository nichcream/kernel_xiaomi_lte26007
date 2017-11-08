/*
 *	inux/arch/arm/mach-lc186x/cpufreq/comip-hotplug.c
 *
 * Copyright (c) 2012 LeadcoreTech Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/percpu.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/tick.h>
#include <linux/platform_device.h>

#include <asm/cputime.h>

#define CREATE_TRACE_POINTS
#include <trace/events/comip-hotplug.h>

#define BOOT_DELAY_MS		(15000) /*15 s*/

#define CHECK_IN_EMERGENCY	(64)   /*64 ms*/
#define CHECK_IN_SOONER		(128)  /*128 ms*/
#define CHECK_IN_SOON		(200)  /*300 ms*/
#define CHECK_IN_DELAY		(300)  /* 600 ms*/
#define CHECK_OUT_DELAY		(200)  /*300 ms*/
#define CHECK_IDLE_DELAY	(1000) /*1000 ms*/
#define CHECK_BOOST_DELAY	(20)  /*20 ms*/

#define SAMPLE_TIME_MIN		(20)   /*20 ms*/
#define BOOST_DURATION_MIN	(20 * USEC_PER_MSEC)

#define NR_HA7_CPUS		4
#define CPULOAD_TABLE		(NR_HA7_CPUS + 1)
/* 0~ 20*4/1= 0~80 */
#define TRANS_LOAD_H0 20
/*10*4/2~35*4/2= 20~70*/
#define TRANS_LOAD_L1 10
#define TRANS_LOAD_H1 35
/*15*4/3~45*4/3= 20~60*/
#define TRANS_LOAD_L2 15
#define TRANS_LOAD_H2 45
/*20*4/4~100*4/4= 20~100*/
#define TRANS_LOAD_L3 20

#define LOCK_USER	0
#define LOCK_CPUFREQ	1
#define LOCK_SUSPEND	2

#define HOTPLUG_ENABLE	0
#define HOTPLUG_DISABLE	1

enum hotplug_state{
	HOTPLUG_NOP,
	HOTPLUG_IN,
	HOTPLUG_OUT
};

struct load_state {
	unsigned int cpu;
	u64 time_in_idle;
	u64 time_in_idle_timestamp;
	u64 speedadj;
	u64 speedadj_timestamp;
	spinlock_t load_lock;
};

struct load_para {
	unsigned int load;
	unsigned int select_off_cpu;
	unsigned int rq_min_cpu;
	unsigned int rq_min_nr;
};

static unsigned int trans_load_h0 = TRANS_LOAD_H0;
module_param_named(load_h0, trans_load_h0, uint, 0644);
static unsigned int trans_load_l1 = TRANS_LOAD_L1;
module_param_named(load_l1, trans_load_l1, uint, 0644);
static unsigned int trans_load_h1 = TRANS_LOAD_H1;
module_param_named(load_h1, trans_load_h1, uint, 0644);
static unsigned int trans_load_l2 = TRANS_LOAD_L2;
module_param_named(load_l2, trans_load_l2, uint, 0644);
static unsigned int trans_load_h2 = TRANS_LOAD_H2;
module_param_named(load_h2, trans_load_h2, uint, 0644);
static unsigned int trans_load_l3 = TRANS_LOAD_L3;
module_param_named(load_l3, trans_load_l3, uint, 0644);

static DEFINE_PER_CPU(struct load_state, load_state_table);
static DEFINE_MUTEX(hotplug_mutex);
static struct workqueue_struct *comip_hotplug_wq;
static struct delayed_work comip_hotplug_work;
static atomic_t hotplug_lock_val = ATOMIC_INIT(0);
static unsigned int cpus_online_max = NR_HA7_CPUS;
static unsigned int cpus_online_min = 1;
static struct hotplug_boost {
	unsigned int cpus_min;
	unsigned int duration;
	u64	end_time;
} boost_date = {
	1, 2 * BOOST_DURATION_MIN, 0
};
static unsigned int boot_delay_finished = 0;

extern int cpufreq_hotplug_register_notifier(struct notifier_block *nb);
extern int cpufreq_hotplug_unregister_notifier(struct notifier_block *nb);

static u64 load_update(unsigned int cpu, unsigned int freq)
{
	struct load_state *pcpu = &per_cpu(load_state_table, cpu);
	u64 now;
	u64 now_idle;
	unsigned int delta_idle;
	unsigned int delta_time;
	u64 active_time;

	now_idle = get_cpu_idle_time_us(cpu, &now);
	delta_idle = (unsigned int)(now_idle - pcpu->time_in_idle);
	delta_time = (unsigned int)(now - pcpu->time_in_idle_timestamp);
	if (likely(delta_time > delta_idle))
		active_time = delta_time - delta_idle;
	else
		active_time = 0;
	pcpu->speedadj += active_time * freq;
	pcpu->time_in_idle = now_idle;
	pcpu->time_in_idle_timestamp = now;
	return now;
}

static void load_resched(unsigned int cpu)
{
	struct load_state *pcpu = &per_cpu(load_state_table, cpu);
	unsigned long flags;

	spin_lock_irqsave(&pcpu->load_lock, flags);
	pcpu->time_in_idle =
		get_cpu_idle_time_us(cpu, &pcpu->time_in_idle_timestamp);
	pcpu->speedadj = 0;
	pcpu->speedadj_timestamp = pcpu->time_in_idle_timestamp;
	spin_unlock_irqrestore(&pcpu->load_lock, flags);
}

static inline void load_resched_cpus(void)
{
	unsigned int cpu;

	for_each_online_cpu(cpu)
		load_resched(cpu);
}

static int load_calc(unsigned int cpu, unsigned int *load)
{
	int ret = 0;
	u64 now;
	u64 speedadj;
	unsigned int delta_time;
	unsigned int freq;
	unsigned long flags;
	struct load_state *pcpu;

	pcpu = &per_cpu(load_state_table, cpu);
	freq = cpufreq_quick_get(cpu);
	if (!freq)
		freq = 1;

	spin_lock_irqsave(&pcpu->load_lock, flags);
	now = load_update(cpu, freq);
	speedadj = pcpu->speedadj;
	delta_time = (unsigned int)(now - pcpu->speedadj_timestamp);
	if (delta_time < (SAMPLE_TIME_MIN * USEC_PER_MSEC))
		ret = -EINVAL;
	spin_unlock_irqrestore(&pcpu->load_lock, flags);

	trace_comip_hotplug_load_raw(cpu, freq,
		get_cpu_nr_running(cpu), speedadj, delta_time);

	if (!ret) {
		do_div(speedadj, delta_time);
		if (load)
			*load += (unsigned int)speedadj;
	}
	return ret;
}

static inline int load_check(struct load_para *info)
{
	int ret = 0;
	unsigned int cpu;
	unsigned int rq_min_cpu = 0;
	unsigned int rq_min_nr = -1U;
	unsigned int select_off_cpu = 0;
	unsigned int nr_r;
	unsigned int load = 0;

	for_each_possible_cpu(cpu) {
		/* if SA7, continue*/
		if(cpu == 4)
			continue;
		if (!cpu_online(cpu)) {
			select_off_cpu = cpu;
			continue;
		}
		ret = load_calc(cpu, &load);
		if (ret < 0)
			goto out;

		if (cpu) {
			nr_r = get_cpu_nr_running(cpu);
			if (rq_min_nr > nr_r) {
				rq_min_nr = nr_r;
				rq_min_cpu = cpu;
			}
		}
	}
	load_resched_cpus();
out:
	if (!ret && info) {
		info->load = load;
		info->rq_min_cpu = rq_min_cpu;
		info->rq_min_nr = rq_min_nr;
		info->select_off_cpu = select_off_cpu;
	}
	return ret;
}

static bool cpu_check_out(unsigned int nr_online_cpu,
		unsigned int threshold_up, unsigned int avg_load, unsigned int rq_min_nr)
{
	return ((nr_online_cpu > 1) &&
		(nr_running() < nr_online_cpu) &&
		(avg_load < threshold_up));
}

static bool cpu_check_in(unsigned int nr_online_cpu,
		unsigned int threshold_down, unsigned int avg_load, unsigned int rq_min_nr)
{
	return (/*(nr_running() > nr_online_cpu)*/(rq_min_nr != 0) &&
			(avg_load >= threshold_down));
}

static unsigned int
sample_rate_scale(enum hotplug_state state)
{
	unsigned int sample_rate;
	unsigned int nr_r = nr_running();
	unsigned int nr_o = num_online_cpus();
	unsigned int nr_p = num_possible_cpus();

	if (state == HOTPLUG_IN) {
		if (nr_o < nr_r && nr_o < nr_p) {
			if (nr_r > nr_p)
				sample_rate = CHECK_IN_EMERGENCY;
			else if (nr_r - nr_o > 2)
				sample_rate = CHECK_IN_EMERGENCY;
			else if (nr_r > nr_o)
				sample_rate = CHECK_IN_SOONER;
			else
				sample_rate = CHECK_IN_SOON;
		} else
			sample_rate = CHECK_IN_DELAY;
	} else if (state == HOTPLUG_OUT)
		sample_rate = CHECK_OUT_DELAY;
	else
		sample_rate = CHECK_OUT_DELAY;

	return sample_rate;
}

static inline enum hotplug_state
hotplug_check(struct load_para *info)
{
	unsigned int nr_online_cpu;
	unsigned int avg_load;
	unsigned int max_freq;
	unsigned int up_data, down_data;
	unsigned int threshold[CPULOAD_TABLE][2] = {
		{0, trans_load_h0},
		{trans_load_l1, trans_load_h1},
		{trans_load_l2, trans_load_h2},
		{trans_load_l3, 100},
		{0, 0}
	};

	nr_online_cpu = num_online_cpus();
	max_freq = cpufreq_quick_get_max(0);
	if (!max_freq)
		max_freq = 1;

	max_freq *= NR_HA7_CPUS;
	avg_load = (unsigned int)((info->load * 100) / max_freq);
	up_data = threshold[nr_online_cpu - 1][0];
	down_data = threshold[nr_online_cpu - 1][1];

	trace_comip_hotplug_info(nr_online_cpu, avg_load,
		up_data, down_data, info->rq_min_nr);

	if (cpu_check_out(nr_online_cpu, up_data, avg_load, info->rq_min_nr)) {
		return HOTPLUG_OUT;
	} else if (cpu_check_in(nr_online_cpu, down_data, avg_load, info->rq_min_nr))
		return HOTPLUG_IN;

	return HOTPLUG_NOP;
}

static inline void
hotplug_resched(unsigned int delay_time)
{
	queue_delayed_work_on(0, comip_hotplug_wq,
		&comip_hotplug_work, msecs_to_jiffies(delay_time));
}

static inline void
hotplug_resched_sync(unsigned long delay)
{
	/* not resched new work before boot delay finished */
	if(boot_delay_finished)
	{
		mod_delayed_work_on(0, comip_hotplug_wq,
			&comip_hotplug_work, msecs_to_jiffies(delay));
	}
}

static int hotplug_is_lock(void)
{
	return !!atomic_read(&hotplug_lock_val);
}

static void hotplug_lock_lock(int src)
{
	atomic_add(1 << src, &hotplug_lock_val);
}

static void hotplug_lock_unlock(int src)
{
	if (atomic_sub_and_test(1 << src, &hotplug_lock_val)) {
		load_resched_cpus();
		hotplug_resched_sync(SAMPLE_TIME_MIN + 1);
	}
}

static inline unsigned int hotplug_online_min(void)
{
	u64 now;
	unsigned int cpus_min;

	cpus_min = cpus_online_min;
	now = ktime_to_us(ktime_get());
	if (now < boost_date.end_time)
		cpus_min = max(boost_date.cpus_min, cpus_online_min);

	return cpus_min;
}

static void __ref comip_hotplug_timer(struct work_struct *work)
{
	struct load_para info;
	enum hotplug_state flag_hotplug;
	unsigned int delay_time = CHECK_OUT_DELAY;
	unsigned int num, cpus_min;
	int delta_max, delta_min, cpu;
	u64 now;

	boot_delay_finished = 1;

	mutex_lock(&hotplug_mutex);
	if (hotplug_is_lock()) {
		now = ktime_to_us(ktime_get());
		if (now < boost_date.end_time)
			delay_time = CHECK_BOOST_DELAY;
		goto resched;
	}

	if (load_check(&info) < 0) {
		now = ktime_to_us(ktime_get());
		if (now < boost_date.end_time)
			delay_time = CHECK_BOOST_DELAY;
		goto resched;
	}

	cpus_min = hotplug_online_min();
	num = num_online_cpus();
	delta_max = num - cpus_online_max;
	delta_min = cpus_min - num;

	if (!delta_max && !delta_min)
		goto resched;

	if (delta_max > 0) {
		for_each_online_cpu(cpu) {
			if (cpu == 0)
				continue;
			cpu_down(cpu);
			if (--delta_max <= 0)
				break;
		}
	} else if (delta_min > 0) {
		for_each_possible_cpu(cpu) {
			if (0 == cpu || 4 == cpu)
				continue;
			if (!cpu_online(cpu)) {
				cpu_up(cpu);
				if (--delta_min <= 0)
					break;
			}
		}
	} else {
		flag_hotplug = hotplug_check(&info);
		if (flag_hotplug == HOTPLUG_IN) {
			if (delta_max < 0 && !cpu_online(info.select_off_cpu))
				cpu_up(info.select_off_cpu);
		} else if (flag_hotplug == HOTPLUG_OUT) {
				if (delta_min < 0 && cpu_online(info.rq_min_cpu))
					cpu_down(info.rq_min_cpu);
		}
		delay_time = sample_rate_scale(flag_hotplug);
		trace_comip_hotplug_stat(flag_hotplug, nr_running(),
			num_online_cpus(), delay_time);
	}
resched:
	hotplug_resched(delay_time);
	mutex_unlock(&hotplug_mutex);
}

static ssize_t show_cpus_max(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cpus_online_max);
}

static ssize_t store_cpus_max(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val > NR_HA7_CPUS)
		val = NR_HA7_CPUS;
	else if (!val)
		val = 1;

	mutex_lock(&hotplug_mutex);
	if (val < cpus_online_min)
		val = cpus_online_min;
	cpus_online_max = val;
	mutex_unlock(&hotplug_mutex);
	return n;
}

static DEVICE_ATTR(cpus_max, 0664, show_cpus_max, store_cpus_max);

static ssize_t show_cpus_min(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cpus_online_min);
}

static ssize_t store_cpus_min(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	int ret;
	unsigned long val, old;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val > NR_HA7_CPUS)
		val = NR_HA7_CPUS;
	else if (!val)
		val = 1;

	mutex_lock(&hotplug_mutex);
	old = cpus_online_min;
	if ( val > cpus_online_max)
		val = cpus_online_max;
	cpus_online_min = val;
	mutex_unlock(&hotplug_mutex);

	if (val != old)
		hotplug_resched_sync(0);
	return n;
}

static DEVICE_ATTR(cpus_min, 0664, show_cpus_min, store_cpus_min);

static ssize_t show_boost_duration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", boost_date.duration);
}

static ssize_t store_boost_duration(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int duration;
	int ret;

	ret = kstrtouint(buf, 0, &duration);
	if (ret < 0)
		return ret;

	if (duration < BOOST_DURATION_MIN)
		duration = BOOST_DURATION_MIN;
	boost_date.duration = duration;

	return n;
}

static DEVICE_ATTR(boost_duration, 0664, show_boost_duration, store_boost_duration);

static ssize_t show_boost(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", boost_date.cpus_min);
}

static ssize_t store_boost(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int cpus_min;
	int ret;
	u64 now;

	ret = kstrtouint(buf, 0, &cpus_min);
	if (ret < 0)
		return ret;

	if (!cpus_min)
		cpus_min = 1;
	mutex_lock(&hotplug_mutex);
	now = ktime_to_us(ktime_get());
	if (now < boost_date.end_time) {
		cpus_min = max(boost_date.cpus_min, cpus_min);
	}

	if (cpus_min > cpus_online_max)
		cpus_min = cpus_online_max;
	boost_date.cpus_min = cpus_min;
	now += boost_date.duration;
	boost_date.end_time = max(boost_date.end_time, now);
	mutex_unlock(&hotplug_mutex);
	hotplug_resched_sync(0);

	return n;
}

static DEVICE_ATTR(boost, 0664, show_boost, store_boost);

static ssize_t show_user_lock(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&hotplug_lock_val));
}

static ssize_t store_user_lock(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (HOTPLUG_ENABLE != val && HOTPLUG_DISABLE != val)
		return -EINVAL;

	mutex_lock(&hotplug_mutex);
	if (HOTPLUG_ENABLE == val)
		hotplug_lock_unlock(LOCK_USER);
	else
		hotplug_lock_lock(LOCK_USER);
	mutex_unlock(&hotplug_mutex);
	return n;
}

static DEVICE_ATTR(user_lock, 0664, show_user_lock, store_user_lock);

static struct attribute *hotplug_attributes[] = {
	&dev_attr_user_lock.attr,
	&dev_attr_cpus_max.attr,
	&dev_attr_cpus_min.attr,
	&dev_attr_boost_duration.attr,
	&dev_attr_boost.attr,
	NULL,
};

static struct attribute_group hotplug_attr_group = {
	.attrs	= hotplug_attributes,
	.name	= "hotplug",
};

static int trans_notifier_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct load_state *pcpu;
	unsigned long flags;

	if (hotplug_is_lock())
		return NOTIFY_OK;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	pcpu = &per_cpu(load_state_table, freq->cpu);
	spin_lock_irqsave(&pcpu->load_lock, flags);
	load_update(freq->cpu, freq->old);
	spin_unlock_irqrestore(&pcpu->load_lock, flags);

	if (freq->new > freq->old)
		hotplug_resched_sync(0);

	return NOTIFY_DONE;
}

static int __cpuinit hotplug_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		load_resched(cpu);
		break;
	case CPU_UP_PREPARE:
		if (hotplug_is_lock())
			return NOTIFY_BAD;
	}
	return NOTIFY_OK;
}

static int pm_notifier_callback(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&hotplug_mutex);
		hotplug_lock_lock(LOCK_SUSPEND);
		mutex_unlock(&hotplug_mutex);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&hotplug_mutex);
		hotplug_lock_unlock(LOCK_SUSPEND);
		mutex_unlock(&hotplug_mutex);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static int cpufreq_notifier_callback(struct notifier_block *this,
		unsigned long action, void * ptr)
{
	if (HOTPLUG_ENABLE != action && HOTPLUG_DISABLE != action)
		return NOTIFY_BAD;

	if (HOTPLUG_DISABLE == action) {
		if (mutex_trylock(&hotplug_mutex))
			hotplug_lock_lock(LOCK_CPUFREQ);
		else
			return NOTIFY_BAD;
	} else {
		mutex_lock(&hotplug_mutex);
		hotplug_lock_unlock(LOCK_CPUFREQ);
	}

	mutex_unlock(&hotplug_mutex);
	return NOTIFY_DONE;
}
static int reboot_notifier_callback(struct notifier_block *this,
		unsigned long code, void *_cmd)
{
	hotplug_lock_lock(LOCK_SUSPEND);
	return NOTIFY_DONE;
}

static int panic_notifier_callback(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	hotplug_lock_lock(LOCK_SUSPEND);
	return NOTIFY_DONE;
}

static struct notifier_block hotplug_cpu_notifier __refdata = {
	.notifier_call = hotplug_cpu_callback,
	.priority = 1,
};

static struct notifier_block trans_notifier_block = {
	.notifier_call = trans_notifier_callback
};

static struct notifier_block pm_notifier_block = {
	.notifier_call = pm_notifier_callback
};

static struct notifier_block reboot_notifier_block = {
	.notifier_call = reboot_notifier_callback
};

static struct notifier_block panic_notifier = {
	.notifier_call	= panic_notifier_callback,
	.priority	= 1,
};

#ifdef CONFIG_ARM_BIG_LITTLE_CPUFREQ
static struct notifier_block cpufreq_notifier_block = {
	.notifier_call = cpufreq_notifier_callback
};
#endif

static int __init comip_hotplug_init(struct platform_device *pdev)
{
	unsigned int cpu;
	struct load_state *pcpu;

	for_each_possible_cpu(cpu) {
		pcpu = &per_cpu(load_state_table, cpu);
		spin_lock_init(&pcpu->load_lock);
	}

	load_resched_cpus();

	comip_hotplug_wq =
		alloc_workqueue(dev_name(&pdev->dev), WQ_FREEZABLE, 1);
	if (!comip_hotplug_wq) {
		printk(KERN_ERR "Creation of hotplug work failed\n");
		return -EFAULT;
	}

	if (sysfs_create_group(&pdev->dev.kobj, &hotplug_attr_group))
		printk("comip_hotplug sys create failed\n");
#ifdef CONFIG_ARM_BIG_LITTLE_CPUFREQ
	cpufreq_hotplug_register_notifier(&cpufreq_notifier_block);
#endif
	cpufreq_register_notifier(&trans_notifier_block,
				CPUFREQ_TRANSITION_NOTIFIER);
	register_pm_notifier(&pm_notifier_block);
	register_reboot_notifier(&reboot_notifier_block);
	register_hotcpu_notifier(&hotplug_cpu_notifier);
	atomic_notifier_chain_register(&panic_notifier_list,
				&panic_notifier);

	INIT_DEFERRABLE_WORK(&comip_hotplug_work,
				comip_hotplug_timer);
	hotplug_resched(BOOT_DELAY_MS);
	boot_delay_finished = 0;

	return 0;
}

static int __exit comip_hotplug_exit(struct platform_device *pdev)
{
	destroy_workqueue(comip_hotplug_wq);
#ifdef CONFIG_ARM_BIG_LITTLE_CPUFREQ
	cpufreq_hotplug_unregister_notifier(&cpufreq_notifier_block);
#endif
	cpufreq_unregister_notifier(&trans_notifier_block,
			CPUFREQ_TRANSITION_NOTIFIER);
	unregister_hotcpu_notifier(&hotplug_cpu_notifier);
	unregister_pm_notifier(&pm_notifier_block);
	unregister_reboot_notifier(&reboot_notifier_block);
	atomic_notifier_chain_unregister(&panic_notifier_list,
				&panic_notifier);
	sysfs_remove_group(&pdev->dev.kobj,
				&hotplug_attr_group);
	return 0;
}

static struct platform_driver comip_hotplug_driver = {
	.remove	= __exit_p(comip_hotplug_exit),
	.driver	= {
		.name	= "comip-hotplug",
		.owner	= THIS_MODULE,
	},
};

static int __init comip_hotplug_driver_init(void)
{
	return platform_driver_probe(&comip_hotplug_driver, comip_hotplug_init);
}

static void __exit comip_hotplug_driver_exit(void)
{
	platform_driver_unregister(&comip_hotplug_driver);
}

late_initcall(comip_hotplug_driver_init);
module_exit(comip_hotplug_driver_exit);

static struct platform_device comip_hotplug_device = {
	.name	= "comip-hotplug",
	.id	= -1,
};

static int __init comip_hotplug_device_init(void)
{
	int ret;

	ret = platform_device_register(&comip_hotplug_device);
	if (ret) {
		printk(KERN_ERR "failed at(%d)\n", __LINE__);
		return ret;
	}
	return ret;
}

static void __exit comip_hotplug_device_exit(void)
{
	platform_device_unregister(&comip_hotplug_device);
}

late_initcall(comip_hotplug_device_init);
module_exit(comip_hotplug_device_exit);

MODULE_DESCRIPTION("A driver to hot-(un)plug cpu based on load ");
MODULE_LICENSE("GPL");

