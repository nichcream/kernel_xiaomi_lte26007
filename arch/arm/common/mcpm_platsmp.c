/*
 * linux/arch/arm/mach-vexpress/mcpm_platsmp.c
 *
 * Created by:  Nicolas Pitre, November 2012
 * Copyright:   (C) 2012-2013  Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Code to handle secondary CPU bringup and hotplug for the cluster power API.
 */

#include <linux/init.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <asm/mcpm.h>
#include <asm/smp.h>
#include <asm/smp_plat.h>
#include <plat/hardware.h>

unsigned int lc186x_cpu_map[5] = {
	0x100,
	0x101,
	0x102,
	0x103,
	0x0,
};

extern int check_core_down(u32 cluster, u32 cpu, int timeout);
static DEFINE_SPINLOCK(boot_lock);

#ifdef CONFIG_HOTPLUG_CPU
static DEFINE_PER_CPU(struct completion, cpu_killed);
#endif

void __init mcpm_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	for  (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

#ifdef CONFIG_HOTPLUG_CPU
	for_each_present_cpu(i) {
		init_completion(&per_cpu(cpu_killed, i));
	}
#endif

}

static void __init mcpm_smp_init_cpus(void)
{
	unsigned int i, ncores = 5;

	printk("%s: cores number:%d\n", __func__, ncores);

	if (ncores > NR_CPUS) {
		printk(KERN_ERR "comip: no. of cores (%u) greater than configured (%u), clipping\n",
			ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++) {
		set_cpu_possible(i, 1);
		cpu_logical_map(i) = lc186x_cpu_map[i];
		pr_info("cpu logical map 0x%x\n", cpu_logical_map(i));
	}
}

static int __cpuinit mcpm_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned int mpidr, pcpu, pcluster, ret;
	extern void secondary_startup(void);

	spin_lock(&boot_lock);
	mpidr = cpu_logical_map(cpu);
	pcpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	pcluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	pr_debug("%s: logical CPU%d is physical CPU%d cluster%d\n",
		 __func__, cpu, pcpu, pcluster);

	mcpm_set_entry_vector(pcpu, pcluster, NULL);
	ret = mcpm_cpu_power_up(pcpu, pcluster);
	if (ret) {
		pr_err("cluster%d cpu%d  is failure to powered-up\n", pcluster, pcpu);
		spin_unlock(&boot_lock);
		return ret;
	}
	mcpm_set_entry_vector(pcpu, pcluster, secondary_startup);
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));
	dsb_sev();
	spin_unlock(&boot_lock);
	return 0;
}

static void __cpuinit mcpm_secondary_init(unsigned int cpu)
{
	spin_lock(&boot_lock);
#ifdef CONFIG_HOTPLUG_CPU
	INIT_COMPLETION(per_cpu(cpu_killed, cpu));
#endif
	spin_unlock(&boot_lock);
}

#ifdef CONFIG_HOTPLUG_CPU
static int mcpm_cpu_kill(unsigned int cpu)
{
	int e;
	unsigned int mpidr, pcpu, pcluster;

	mpidr = cpu_logical_map(cpu);
	pcpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	pcluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	e = wait_for_completion_timeout(&per_cpu(cpu_killed, cpu), 100);
	if (!e)
		printk("cluster%d cpu%d is timeout to be killed\n", pcluster, pcpu);
	if(mcpm_check_cpu_powered_down(pcpu, pcluster)){
		printk("Failed to power down cluster%d cpu%d \n", pcluster, pcpu);
	}

	return e;
}

static int mcpm_cpu_disable(unsigned int cpu)
{
	/*
	 * We assume all CPUs may be shut down.
	 * This would be the hook to use for eventual Secure
	 * OS migration requests as described in the PSCI spec.
	 */
	return cpu == 0 ? -EPERM : 0;
}

static void mcpm_cpu_die(unsigned int cpu)
{
	unsigned int mpidr, pcpu, pcluster;
	complete(&per_cpu(cpu_killed, cpu));

	mpidr = read_cpuid_mpidr();
	pcpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	pcluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	mcpm_set_entry_vector(pcpu, pcluster, NULL);
	mcpm_cpu_power_down();
}

#endif

static struct smp_operations __initdata mcpm_smp_ops = {
	.smp_init_cpus		= mcpm_smp_init_cpus,
	.smp_prepare_cpus	= mcpm_smp_prepare_cpus,
	.smp_boot_secondary	= mcpm_boot_secondary,
	.smp_secondary_init	= mcpm_secondary_init,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable		= mcpm_cpu_disable,
	.cpu_die		= mcpm_cpu_die,
	.cpu_kill		= mcpm_cpu_kill,
#endif
};

void __init mcpm_smp_set_ops(void)
{
	smp_set_ops(&mcpm_smp_ops);
}
