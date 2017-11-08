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

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/localtimer.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>
#include <asm/cpu.h>
#include <asm/mmu_context.h>

#include <plat/hardware.h>
#include <mach/iomap.h>
#include <mach/suspend.h>

extern void comip_secondary_startup(void);

static DEFINE_SPINLOCK(boot_lock);
static void __iomem *scu_base = __io_address(CPU_CORE_BASE);

#ifdef CONFIG_HOTPLUG_CPU
static DEFINE_PER_CPU(struct completion, cpu_killed);
extern void comip_hotplug_startup(void);
#endif

static DECLARE_BITMAP(cpu_init_bits, CONFIG_NR_CPUS) __read_mostly;
const struct cpumask *const cpu_init_mask = to_cpumask(cpu_init_bits);
#define cpu_init_map (*(cpumask_t *)cpu_init_mask)

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
#ifdef CONFIG_HOTPLUG_CPU
	if (!cpumask_test_cpu(cpu, cpu_init_mask))
#endif
	gic_secondary_init(0);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
#ifdef CONFIG_HOTPLUG_CPU
	cpu_set(cpu, cpu_init_map);
	INIT_COMPLETION(per_cpu(cpu_killed, cpu));
#endif
	spin_unlock(&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */

	spin_lock(&boot_lock);

	/* set the reset vector to point to the secondary_startup routine */
#ifdef CONFIG_HOTPLUG_CPU
	if (cpumask_test_cpu(cpu, cpu_init_mask)) {
	}
	else
#endif
	__raw_writel(virt_to_phys(comip_secondary_startup), io_p2v(CPU1_CBOOT_ADDR));
		
	__raw_writel(COMIP_WAKEUP_DOWN, io_p2v(CPU1_WAKEUP_FLAG));
	smp_mb();

	comip_wake_up_cpu1();
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
void __init smp_init_cpus(void)
{
	unsigned int i, ncores = scu_get_core_count(scu_base);
	printk("smp_init_cpus:%d\n", ncores);

	if (ncores > NR_CPUS) {
		printk(KERN_ERR "comip: no. of cores (%u) greater than configured (%u), clipping\n",
			ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, 1);

	set_smp_cross_call(gic_raise_softirq);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;
	printk("platform_smp_prepare_cpus\n");
	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for  (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

#ifdef CONFIG_HOTPLUG_CPU
	for_each_present_cpu(i) {
		init_completion(&per_cpu(cpu_killed, i));
	}
#endif

	i = __raw_readl(io_p2v(CORE_SCU_BASE + 0x00));
	i |= 1<<6 | 1 << 5 | 1 << 3;
	__raw_writel(i, io_p2v(CORE_SCU_BASE + 0x00));

	scu_enable(scu_base);
}

#ifdef CONFIG_HOTPLUG_CPU

static inline void comip_cpu_is_pd(unsigned int cpu)
{
	while (SCU_PM_POWEROFF != __raw_readb(io_p2v(CORE_SCU_BASE + 0x08 + cpu)));
}

int platform_cpu_kill(unsigned int cpu)
{
	int e;

	e = wait_for_completion_timeout(&per_cpu(cpu_killed, cpu), 100);
	comip_cpu_is_pd(cpu);
	printk(KERN_DEBUG "CPU%u: power down\n", cpu);

	return e;
}

static void comip_wakeup_int_clear(void)
{
	unsigned int val;
	val = __raw_readl(io_p2v(GIC_CPU_BASE + GIC_CPU_HIGHPRI)) & 0x3ff;
	if (COMIP_WAKEUP_CPU0_SGI_ID == val) {
		val = __raw_readl(io_p2v(GIC_CPU_BASE + GIC_CPU_INTACK));
		__raw_writel(val, io_p2v(GIC_CPU_BASE + GIC_CPU_EOI));
	}
}

void platform_cpu_die(unsigned int cpu)
{
#ifdef DEBUG
	unsigned int this_cpu = hard_smp_processor_id();

	if (cpu != this_cpu) {
		printk(KERN_CRIT "Eek! platform_cpu_die running on %u, should be %u\n",
			   this_cpu, cpu);
		BUG();
	}

	if (!cpu) {
		printk(KERN_CRIT "cpu0 is dying?%u\n");
		BUG();
	}
#endif

	complete(&per_cpu(cpu_killed, cpu));
	flush_cache_all();

	if (CPU_ID_1 == cpu) {
		__raw_writel(COMIP_WAKEUP_DOWN, io_p2v(CPU1_WAKEUP_FLAG));
		smp_mb();
		comip_suspend_lp_cpu1();
		smp_mb();
		__raw_writel(COMIP_WAKEUP_ON, io_p2v(CPU1_WAKEUP_FLAG));
		comip_wakeup_int_clear();
	}
	
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */

	if (unlikely(!comip_context_area))
		return -ENXIO;

	return cpu == 0 ? -EPERM : 0;
}
#endif
