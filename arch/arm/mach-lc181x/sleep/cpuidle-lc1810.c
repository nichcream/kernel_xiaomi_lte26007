/*
 * arch/arm/mach-lc181x/sleep/cpuidle-lc1810.c
 *
 * CPU idle driver for comip CPUs
 *
 * Copyright (c) 2012, LeadcoreTech Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/sched.h>

#include <asm/proc-fns.h>
#include <asm/system_misc.h>

#include <mach/suspend.h>

#define CPU_IDLE_ON 1
#define CPU_IDLE_OFF 0

static cpumask_t cpu_in_idle;
static DEFINE_SPINLOCK(comip_idle_lock);
static void (*comip_idle_old)(void);

static int comip_idle_lp_prepare(int cpu)
{
	spin_lock(&comip_idle_lock);
	cpumask_set_cpu(cpu, &cpu_in_idle);
	if (cpumask_equal(&cpu_in_idle, cpu_online_mask)) {
		comip_cpufreq_idle(CPU_IDLE_ON);
	}
	spin_unlock(&comip_idle_lock);

	return 0;
}

static int comip_idle_lp_complete(int cpu, int status)
{
	spin_lock(&comip_idle_lock);
	if (cpumask_equal(&cpu_in_idle, cpu_online_mask)) {
		comip_cpufreq_idle(CPU_IDLE_OFF);
	}
	cpumask_clear_cpu(cpu, &cpu_in_idle);
	spin_unlock(&comip_idle_lock);

	return 0;
}

static void comip_idle_do(void)
{
	int cpu;

	local_irq_disable();
	local_fiq_disable();

	if (!need_resched()) {
		cpu = smp_processor_id();
		comip_idle_lp_prepare(cpu);
		cpu_do_idle();
		comip_idle_lp_complete(cpu, 0);
	}

	local_fiq_enable();
	local_irq_enable();
}

static int __init comip_idle_init(void)
{
	cpumask_empty(&cpu_in_idle);
	comip_idle_old = arm_pm_idle;
	arm_pm_idle = comip_idle_do;
	cpu_idle_wait();
	return 0;
}

static void __exit comip_idle_exit(void)
{
	arm_pm_idle = comip_idle_old;
	cpu_idle_wait();
}

module_init(comip_idle_init);
module_exit(comip_idle_exit);

