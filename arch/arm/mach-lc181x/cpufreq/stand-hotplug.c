/* linux/arch/arm/mach-lc181x/cpufreq/stand-hotplug.c
 *
 * Copyright (c) 2012 LeadcoreTech Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/clk.h>

#include <mach/clock.h>
#include <mach/suspend.h>

#define CREATE_TRACE_POINTS
#include <trace/events/stand-hotplug.h>

#define cputime64_add(__a, __b)		((__a) + (__b))
#define cputime64_sub(__a, __b)		((__a) - (__b))

#define TRANS_RQ 2
#define TRANS_LOAD_RQ 20

/* original value*/
#if 1
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
#endif

#if 0
/* 0~ 20*4/1= 0~80 */
#define TRANS_LOAD_H0 20
/*10*4/2~40*4/2= 20~80*/
#define TRANS_LOAD_L1 10
#define TRANS_LOAD_H1 40
/*30*4/3~60*4/3= 40~80*/
#define TRANS_LOAD_L2 30
#define TRANS_LOAD_H2 60
/*30*4/4~100*4/4= 30~100*/
#define TRANS_LOAD_L3 30
#endif

#define BOOT_DELAY		40
#define CHECK_DELAY_ON		(.3*HZ*4)  /* 1200 ms*/
#define CHECK_DELAY_ON_SOON	(.3*HZ*2)  /*600 ms*/
#define CHECK_DELAY_OFF		(.3*HZ)    /*300 ms*/

#define CPU_OFF 0
#define CPU_ON  1

#define HOTPLUG_UNLOCKED 0
#define HOTPLUG_LOCKED 1
#define PM_HOTPLUG_DEBUG 0
#define NUM_CPUS num_possible_cpus()
#define CPULOAD_TABLE (NR_CPUS + 1)

#define DBG_PRINT(fmt, ...)\
	if(PM_HOTPLUG_DEBUG)			\
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

static struct workqueue_struct *hotplug_wq;
static struct delayed_work hotplug_work;

static unsigned int max_performance;
static unsigned int freq_min = -1UL;

static unsigned int hotpluging_rate = CHECK_DELAY_OFF;
module_param_named(rate, hotpluging_rate, uint, 0644);
static unsigned int user_lock;
module_param_named(lock, user_lock, uint, 0644);
static unsigned int trans_rq= TRANS_RQ;
module_param_named(min_rq, trans_rq, uint, 0644);
static unsigned int trans_load_rq = TRANS_LOAD_RQ;
module_param_named(load_rq, trans_load_rq, uint, 0644);

static unsigned int trans_load_h0 = TRANS_LOAD_H0;
module_param_named(load_h0, trans_load_h0, uint, 0644);
static unsigned int trans_load_l1 = TRANS_LOAD_L1;
module_param_named(load_l1, trans_load_l1, uint, 0644);
static unsigned int trans_load_h1 = TRANS_LOAD_H1;
module_param_named(load_h1, trans_load_h1, uint, 0644);

#if (NR_CPUS > 2)
static unsigned int trans_load_l2 = TRANS_LOAD_L2;
module_param_named(load_l2, trans_load_l2, uint, 0644);
static unsigned int trans_load_h2 = TRANS_LOAD_H2;
module_param_named(load_h2, trans_load_h2, uint, 0644);
static unsigned int trans_load_l3 = TRANS_LOAD_L3;
module_param_named(load_l3, trans_load_l3, uint, 0644);
#endif

enum flag{
	HOTPLUG_NOP,
	HOTPLUG_IN,
	HOTPLUG_OUT
};

struct cpu_time_info {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	unsigned int load;
};

struct cpu_hotplug_info {
	unsigned long nr_running;
	pid_t tgid;
};

static DEFINE_PER_CPU(struct cpu_time_info, hotplug_cpu_time);

/* mutex can be used since hotplug_timer does not run in
   timer(softirq) context but in process context */
static DEFINE_MUTEX(hotplug_lock);

bool hotplug_out_chk(unsigned int nr_online_cpu, unsigned int threshold_up,
		unsigned int avg_load, unsigned int cur_freq)
{
	return ((nr_online_cpu > 1) &&
		(avg_load < threshold_up /* ||cur_freq <= freq_min*/));
}

static inline enum flag
standalone_hotplug(unsigned int load, unsigned long nr_rq_min,
		unsigned int cpu_rq_min)
{
	unsigned int cur_freq;
	unsigned int nr_online_cpu;
	unsigned int avg_load;
	struct clk *cpu_clk;
	/*load threshold*/
	unsigned int threshold[CPULOAD_TABLE][2] = {
		{0, trans_load_h0},
		{trans_load_l1, trans_load_h1},
#if (NR_CPUS > 2)
		{trans_load_l2, trans_load_h2},
		{trans_load_l3, 100},
#endif
		{0, 0}
	};

	cpu_clk = clk_get(NULL, CLUSTER_CLK_NAME);
	if (IS_ERR(cpu_clk))
		return HOTPLUG_NOP;
	cur_freq = clk_get_rate(cpu_clk) / 1000;
	clk_put(cpu_clk );

	nr_online_cpu = num_online_cpus();
	avg_load = (unsigned int)((cur_freq * load) / max_performance);

	trace_stand_hotplug_info(load, avg_load, cur_freq, 
		nr_rq_min, nr_online_cpu);

	if (hotplug_out_chk(nr_online_cpu, threshold[nr_online_cpu - 1][0],
			    avg_load, cur_freq)) {
		return HOTPLUG_OUT;
		/*If total nr_running is less than cpu(on-state) number, 
		 *hotplug do not hotplug-in 
		 */
	} else if (nr_running() > nr_online_cpu &&
			avg_load > threshold[nr_online_cpu - 1][1] && 
			cur_freq > freq_min) {
		return HOTPLUG_IN;
#if 1
#else
	} else if (nr_online_cpu > 1 && nr_rq_min < trans_rq) {
		struct cpu_time_info *tmp_info;

		tmp_info = &per_cpu(hotplug_cpu_time, cpu_rq_min);

		/*If CPU(cpu_rq_min) load is less than trans_load_rq, 
		 *hotplug-out
		 */
		if (tmp_info->load < trans_load_rq) {
			trace_stand_hotplug_rq_min(nr_rq_min, trans_rq, 
				tmp_info->load, trans_load_rq, nr_online_cpu);
			return HOTPLUG_OUT;
		}
#endif
	}

	return HOTPLUG_NOP;
}

static void hotplug_timer(struct work_struct *work)
{
	struct cpu_hotplug_info tmp_hotplug_info[NR_CPUS];
	int i;
	unsigned int load = 0;
	unsigned int cpu_rq_min=0;
	unsigned long nr_rq_min = -1UL;
	unsigned int select_off_cpu = 0;
	enum flag flag_hotplug;

	mutex_lock(&hotplug_lock);

	if (user_lock == 1)
		goto no_hotplug;

	for_each_online_cpu(i) {
		struct cpu_time_info *tmp_info;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;

		tmp_info = &per_cpu(hotplug_cpu_time, i);
		cur_idle_time = get_cpu_idle_time_us(i, &cur_wall_time);
		idle_time = (unsigned int)cputime64_sub(cur_idle_time,
						tmp_info->prev_cpu_idle);
		tmp_info->prev_cpu_idle = cur_idle_time;
		wall_time = (unsigned int)cputime64_sub(cur_wall_time,
						tmp_info->prev_cpu_wall);
		tmp_info->prev_cpu_wall = cur_wall_time;
		if (wall_time > idle_time) {
			tmp_info->load = 100 * (wall_time - idle_time); 
			tmp_info->load /= wall_time;
		} else
			tmp_info->load = 0;
		load += tmp_info->load;

		/*find minimum runqueue length*/
		tmp_hotplug_info[i].nr_running = get_cpu_nr_running(i);
		if (i && nr_rq_min > tmp_hotplug_info[i].nr_running) {
			nr_rq_min = tmp_hotplug_info[i].nr_running;
			cpu_rq_min = i;
		}
	}

	for (i = NUM_CPUS - 1; i > 0; --i) {
		if (cpu_online(i) == 0) {
			select_off_cpu = i;
			break;
		}
	}

	/*standallone hotplug*/
	flag_hotplug = standalone_hotplug(load, nr_rq_min, cpu_rq_min);

	/*cpu hotplug*/
	if (flag_hotplug == HOTPLUG_IN && 
			cpu_online(select_off_cpu) == CPU_OFF) {
		DBG_PRINT("cpu%d turning on!\n", select_off_cpu);
		cpu_up(select_off_cpu);
		DBG_PRINT("cpu%d on\n", select_off_cpu);

		if (nr_running() > num_online_cpus())
			hotpluging_rate = CHECK_DELAY_ON_SOON;
		else
			hotpluging_rate = CHECK_DELAY_ON;
		trace_stand_hotplug_in(flag_hotplug,
				select_off_cpu,
				cpu_rq_min);
	} else if (flag_hotplug == HOTPLUG_OUT && 
			cpu_online(cpu_rq_min) == CPU_ON) {
		DBG_PRINT("cpu%d turnning off!\n", cpu_rq_min);
		cpu_down(cpu_rq_min);
		DBG_PRINT("cpu%d off!\n", cpu_rq_min);
		hotpluging_rate = CHECK_DELAY_OFF;
		trace_stand_hotplug_out(flag_hotplug,
				select_off_cpu,
				cpu_rq_min);
	} 

no_hotplug:

	queue_delayed_work_on(0, hotplug_wq, &hotplug_work, hotpluging_rate);

	mutex_unlock(&hotplug_lock);
}

static int hotplug_pm_notifier_event(struct notifier_block *this,
					     unsigned long event, void *ptr)
{
	static unsigned user_lock_saved;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&hotplug_lock);
		user_lock_saved = user_lock;
		user_lock = 1;
		pr_info("saving pm_hotplug lock %x\n",
			user_lock_saved);
		mutex_unlock(&hotplug_lock);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&hotplug_lock);
		pr_info("restoring pm_hotplug lock %x\n",
			user_lock_saved);
		user_lock = user_lock_saved;
		mutex_unlock(&hotplug_lock);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block hotplug_pm_notifier = {
	.notifier_call = hotplug_pm_notifier_event,
};

static int hotplug_reboot_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	mutex_lock(&hotplug_lock);
	user_lock = 1;
	mutex_unlock(&hotplug_lock);

	return NOTIFY_DONE;
}

static struct notifier_block hotplug_reboot_notifier = {
	.notifier_call = hotplug_reboot_notifier_call,
};

static int __init stand_hotplug_init(void)
{
	unsigned int i;
	unsigned int freq;
	unsigned int freq_max = 0;
	struct cpufreq_frequency_table *table;

	printk(KERN_INFO "stand_hotplug init \n");

#ifdef CONFIG_CPU_FREQ
	table = cpufreq_frequency_get_table(0);
	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		freq = table[i].frequency;
		if (freq != CPUFREQ_ENTRY_INVALID && freq > freq_max)
			freq_max = freq;
		else if (freq != CPUFREQ_ENTRY_INVALID && freq_min > freq)
			freq_min = freq;
	}
	max_performance = freq_max * NUM_CPUS;
#else
	return 0;
#endif

	hotplug_wq = alloc_workqueue("dynamic hotplug", 0, 0);
	if (!hotplug_wq) {
		printk(KERN_ERR "Creation of hotplug work failed\n");
		return -EFAULT;
	}

	INIT_DELAYED_WORK(&hotplug_work, hotplug_timer);
	queue_delayed_work_on(0, hotplug_wq, &hotplug_work, BOOT_DELAY * HZ);

	register_pm_notifier(&hotplug_pm_notifier);
	register_reboot_notifier(&hotplug_reboot_notifier);
    
	return 0;
}

late_initcall(stand_hotplug_init);

static struct platform_device stand_hotplug_device = {
	.name = "dynamic-cpu-hotplug",
	.id = -1,
};

static int __init stand_hotplug_device_init(void)
{
	int ret;

	ret = platform_device_register(&stand_hotplug_device);
	if (ret) {
		printk(KERN_ERR "failed at(%d)\n", __LINE__);
		return ret;
	}

	return ret;
}

late_initcall(stand_hotplug_device_init);
