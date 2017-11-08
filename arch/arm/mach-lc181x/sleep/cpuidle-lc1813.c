/*
 *  arch/arm/mach-lc181x/sleep/cpuidle-lc1813.c
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
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <linux/sched.h>

#include <asm/proc-fns.h>
#include <asm/system_misc.h>

#include <mach/suspend.h>
#include <plat/hardware.h>
#include <asm/io.h>
#include <linux/delay.h>

#ifdef CONFIG_CPU_IDLE

static DEFINE_PER_CPU(struct cpuidle_device, idle_device);

static int idle_enter_wfi(struct cpuidle_device *dev,
	struct cpuidle_driver *drv, int index)
{
	local_irq_disable();
	local_fiq_disable();

	cpu_do_idle();

	local_fiq_enable();
	local_irq_enable();

	return index;
}

static int idle_pd_cpu0(int index)
{
	if (1 == index) {
		cpu_do_idle();
		return 0;
	}

	if (1 == num_online_cpus()
		&& suspend_only_cpu0_on())
		suspend_idle_poweroff(CPU_ID_0);
	else
		cpu_do_idle();

	return index;
}

static int idle_pd_cpux(unsigned int cpu, int index)
{
	if (2 == index)
		index = 1;

	suspend_idle_poweroff(cpu);
	return index;
}

static int idle_enter_pd(struct cpuidle_device *dev,
	struct cpuidle_driver *drv, int index)
{
	unsigned int cpu = smp_processor_id();

	local_irq_disable();
	local_fiq_disable();

	if (!need_resched()) {
		if (CPU_ID_0 == cpu)
			index = idle_pd_cpu0(index);
		else
			index = idle_pd_cpux(cpu, index);
	}

	local_fiq_enable();
	local_irq_enable();

	return index;
}

static struct cpuidle_driver idle_driver = {
	.name			= "lc1813-idle",
	.owner			= THIS_MODULE,
	.en_core_tk_irqen	= 1,
	.power_specified	= 0,
	.safe_state_index	= 0,
	.state_count		= 3,
	.states = {
		[0] = {
			.enter			= idle_enter_wfi,
			.exit_latency		= 1,
			.target_residency	= 10,
			.power_usage		= 160,
			.flags			= CPUIDLE_FLAG_TIME_VALID,
			.name			= "WFI",
			.desc			= "CPU WFI",
		},

		[1] = {
			.enter			= idle_enter_pd,
			.exit_latency		= 180 + 20 + 85, /*in + bootrom + out*/
			.target_residency	= 3000,
			.power_usage		= 100,
			.flags			= CPUIDLE_FLAG_TIME_VALID,
			.name			= "WFI-LP",
			.desc			= "Core power-down",
		},

		[2] = {
			.enter			= idle_enter_pd,
			.exit_latency		= 355 + 20 + 90, /*in + bootrom + out*/
			.target_residency	= 5000,
			.power_usage		= 60,
			.flags			= CPUIDLE_FLAG_TIME_VALID,
			.name			= "Cluster-PD",
			.desc			= "Cluster power-down",
		},
	},
};

static int __init lc1813_cpuidle_init(void)
{
	int ret;
	unsigned int cpu;
	struct cpuidle_device *dev;
	struct cpuidle_driver *drv = &idle_driver;

	ret = cpuidle_register_driver(drv);
	if (ret) {
		pr_err("CPUidle driver registration failed\n");
		return ret;
	}

	for_each_possible_cpu(cpu) {
		dev = &per_cpu(idle_device, cpu);
		dev->cpu = cpu;
#ifdef CONFIG_ARCH_NEEDS_CPU_IDLE_COUPLED
		cpumask_copy(&dev->coupled_cpus, cpu_possible_mask);
#endif
		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("CPU%u: CPUidle device registration failed\n",
				cpu);
			return ret;
		}
	}

	return 0;
}

device_initcall(lc1813_cpuidle_init);

#else

static void (*comip_idle_old)(void);

#ifdef CONFIG_CPU_IDLE_LOWEST_FREQ
inline static unsigned int set_lowest_freq(void)
{
	unsigned int val = 0;

	/* save current cpu freq */
	val  = readl(io_p2v(AP_PWR_A7_CLK_CTL));
	/* set lowest cpu freq */
	writel((1 << 16) | (0x1), io_p2v(AP_PWR_A7_CLK_CTL));
	udelay(1);
	return val;
}

inline static void restore_cpu_freq(unsigned int val)
{
	writel((1 << 16) | (val & 0x1f), io_p2v(AP_PWR_A7_CLK_CTL));
	udelay(1);
}

/* Set cpu freq lowest, then do ilde*/
static void cpu_low_freq_idle(void)
{
	unsigned int val;
	val = set_lowest_freq();
	cpu_do_idle();
	restore_cpu_freq(val);
}
#endif

static void comip_wfi_do(void)
{
	unsigned int cpu = smp_processor_id();

	local_irq_disable();
	local_fiq_disable();

	if (CPU_ID_0 == cpu) {
#ifdef CONFIG_CPU_IDLE_LOWEST_FREQ
		if (1 == num_online_cpus() && suspend_only_cpu0_on())
			cpu_low_freq_idle();
		else
#endif
			cpu_do_idle();
	}else
		cpu_do_idle();

	local_fiq_enable();
	local_irq_enable();
}

static int __init comip_idle_init(void)
{
	comip_idle_old = arm_pm_idle;
	arm_pm_idle = comip_wfi_do;
	//cpu_idle_wait();
	return 0;
}

static void __exit comip_idle_exit(void)
{
	arm_pm_idle = comip_idle_old;
	//cpu_idle_wait();
}

module_init(comip_idle_init);
module_exit(comip_idle_exit);

#endif

