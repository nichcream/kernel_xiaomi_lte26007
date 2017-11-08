/*
 *  linux/arch/arm/mach-comip/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/mcpm.h>

#include <asm/cacheflush.h>
#include <asm/mach-types.h>
#include <asm/localtimer.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>
#include <asm/cpu.h>
#include <asm/mmu_context.h>
#include <asm/cputype.h>

#include <plat/hardware.h>
#include <mach/suspend.h>


#define COMIP_SMP_DEBUG 0
#if COMIP_SMP_DEBUG
#define SMP_PR(fmt, args...)	printk(KERN_INFO fmt, ## args)
#else
#define SMP_PR(fmt, args...)	printk(KERN_DEBUG fmt, ## args)
#endif

static DEFINE_SPINLOCK(boot_lock);

#ifdef CONFIG_HOTPLUG_CPU
static DEFINE_PER_CPU(struct completion, cpu_killed);
#endif

void __cpuinit lc1860_secondary_init(unsigned int cpu)
{
	SMP_PR("cpu%d:%s\n", cpu, __func__);

	suspend_secondary_init(cpu);
	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	//gic_secondary_init(0);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
#ifdef CONFIG_HOTPLUG_CPU
	INIT_COMPLETION(per_cpu(cpu_killed, cpu));
#endif
	spin_unlock(&boot_lock);

}

int __cpuinit lc1860_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	printk("cpu%d:%s\n", cpu, __func__);

	BUG_ON(cpu>=CONFIG_NR_CPUS);

	spin_lock(&boot_lock);

	suspend_cpu_poweron(cpu);

	comip_wake_up_cpux(cpu);

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init lc1860_smp_init_cpus(void)
{
	unsigned int i, ncores = core_count_get();

	printk("%s: cores number:%d\n", __func__, ncores);

	if (ncores > NR_CPUS) {
		printk(KERN_ERR "comip: no. of cores (%u) greater than configured (%u), clipping\n",
			ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, 1);

	//set_smp_cross_call(gic_raise_softirq);
}

void __init lc1860_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;
	printk("max_cpus:%d\n", max_cpus);

	for  (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

#ifdef CONFIG_HOTPLUG_CPU
	for_each_present_cpu(i) {
		init_completion(&per_cpu(cpu_killed, i));
	}
#endif

	suspend_smp_prepare();
}

#ifdef CONFIG_HOTPLUG_CPU
int lc1860_cpu_kill(unsigned int cpu)
{
	int e;

	e = wait_for_completion_timeout(&per_cpu(cpu_killed, cpu), 100);
	if (!e)
		printk("cpu%d is timeout to be killed\n", cpu);
	suspend_cpu_poweroff(cpu);

	return e;
}

void lc1860_cpu_die(unsigned int cpu)
{
	if (cpu == 0)
		return;

	complete(&per_cpu(cpu_killed, cpu));
	suspend_cpu_die(cpu);

	/* We should never return from idle */
	while(1)
		cpu_relax();
}

int lc1860_cpu_disable(unsigned int cpu)
{
	return cpu == 0 ? -EPERM : 0;
}
#endif

struct smp_operations lc1860_smp_ops __initdata = {
	.smp_init_cpus		= lc1860_smp_init_cpus,
	.smp_prepare_cpus	= lc1860_smp_prepare_cpus,
	.smp_secondary_init	= lc1860_secondary_init,
	.smp_boot_secondary	= lc1860_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= lc1860_cpu_die,
	.cpu_disable	= lc1860_cpu_disable,
	.cpu_kill		= lc1860_cpu_kill,
#endif
};

bool __init bL_smp_init_ops(void)
{
#ifdef CONFIG_MCPM
	/*
	 * The best way to detect a multi-cluster configuration at the moment
	 * is to look for the presence of a CCI in the system.
	 * Override the default comip_smp_ops if so.
	 */
	mcpm_smp_set_ops();
	return true;
#endif
	return false;
}
