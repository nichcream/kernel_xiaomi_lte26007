/*
 * arch/arm/mach-lc186x/bL_pm.c - LC186X power management support
 *
 * Created by:	Nicolas Pitre, October 2012
 * Copyright:	(C) 2012  Linaro Limited
 *
 * Some portions of this file were originally written by Achin Gupta
 * Copyright:   (C) 2012  ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/suspend.h>

#include <asm/suspend.h>
#include <asm/mcpm.h>
#include <asm/proc-fns.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/cp15.h>

#include <plat/hardware.h>
#include <mach/suspend.h>

#define VDDCPU_POWEROFF_LEVEL	2

struct suspend_args {
	unsigned int mpid;
	unsigned int flag;
};

enum POWER_S{
	ON, OFF, UPING,
};

/* SA7 cluster is 0.
 * HA7 cluster is 1.
 */
enum{
	CLUSTER_SA7,
	CLUSTER_HA7,
};

static int pm_use_count[MAX_CPUS_PER_CLUSTER][MAX_NR_CLUSTERS];
static arch_spinlock_t pm_lock = __ARCH_SPIN_LOCK_UNLOCKED;
static u32 a7_clock[2];

extern void lc186x_bL_resume(void);
extern void lc186x_bL_pm_power_up_setup(unsigned int);
extern void comip1860_resume(void);
extern int interrupt_is_pending(void);
extern unsigned int device_idle_state(void);

static void freq_step_set(unsigned int cluster, int up)
{
	u32 reg, i, val, dvs;

	reg = cluster ? (AP_PWR_HA7_CLK_CTL) : AP_PWR_SA7CLK_CTL0;

	if (!up)
		a7_clock[cluster] =  __raw_readl(io_p2v(reg));

	if (cluster)
		val = a7_clock[cluster] & 0x1f;
	else
		val = (a7_clock[cluster] >> 8) & 0xf;
	/* Set freq lowest*/
	if (cluster) {
		/* for HA7 lowest GR is 1 */
		if (!up) {
			if (val == 1)
				return;
			if (!(val % 2)) {
				val -= 1;
				__raw_writel(val | (0x1f << 16), io_p2v(reg));
				udelay(2);
			}
			while(val > 1) {
				val -= 2;
				__raw_writel(val | (0x1f << 16), io_p2v(reg));
				udelay(2);
			}
			/* Don't need set cpu voltage low */
		} else {
			/* Restor cpu freq*/
			if (val > 8) {
				dvs = __raw_readl(io_p2v(DDR_PWR_PMU_CTL));
				dvs = (dvs >> 12) & 0x3;
				if (dvs < 3)
					val = 8;
			}

			if (val %2)
				i = 1;
			else
				i = 2;
			while (i <= val) {
				__raw_writel(i | (0x1f << 16), io_p2v(reg));
				udelay(2);
				i += 2;
			}
		}
	} else {
		if (!up) {
			if (val == 1)
				return;
			if (!(val % 2)) {
				val -= 1;
				__raw_writel((val << 8) | 0xf << 24, io_p2v(reg));
				udelay(2);
			}
			while (val > 1) {
				val -= 2;
				__raw_writel((val << 8) | 0xf << 24, io_p2v(reg));
				udelay(2);
			}
		} else {
			if (val %2)
				i = 1;
			else
				i = 2;
			while (i <= val) {
				__raw_writel((i << 8) | 0xf << 24, io_p2v(reg));
				udelay(2);
				i += 2;
			}
		}
	}
}

static inline void sleep_enable(int enable)
{
	unsigned int val;

	if(enable){
		/* bit0 HA7C0_SLP_EN */
		val = 0x1 | 0x1 << 16;
		/* bit4 SA7_SLP_EN */
		val |= 0x1 << 4 | 0x1 << 20;
	}else
		val = 0x1 << 16 | 0x1 << 20;
	__raw_writel(val, io_p2v(AP_PWR_SLPCTL0));
}

static inline void core_auto_pd_enable(u32 cluster, u32 cpu, int enable)
{
	u32 reg;

	reg = cluster ? (AP_PWR_HA7_C0_PD_CTL + cpu  * 0x4)
		: AP_PWR_SA7_C_PD_CTL;

	if(enable)
		/* bit 4:auto pd mk, bit 5: pu intr mk
		 *  set 0 to enable
		 */
		__raw_writel(0x3 << 20, io_p2v(reg));
	else
		__raw_writel(0x3 << 4 | 0x3 << 20, io_p2v(reg));
}

static int  get_core_power_status(u32 cluster, u32 cpu)
{
	int offset;
	u32 val;

	offset = cluster ? ((cpu + 1) * 4 ) : 28;
	val = __raw_readl(io_p2v(AP_PWR_PDFSM_ST0));
	val >>= offset;
	val &= 0x7;

	if(val == 0x7)
		return OFF;
	else if(val == 0)
		return ON;
	else
		return UPING;
}

static int check_core_status(u32 cluster, u32 cpu, int state, int timeout)
{
	int curret;

	do{
		curret = get_core_power_status(cluster, cpu);
		if(state == curret)
			return 0;
		if(timeout-- <= 0){
			return -1;
		}
		udelay(100);
	}while(1);
}

static int get_scu_power_status(u32 cluster)
{
	int offset;
	u32 val;

	offset = cluster ? 0 : 24;
	val = __raw_readl(io_p2v(AP_PWR_PDFSM_ST0));
	val >>= offset;
	val &= 0x7;

	if(val == 0x7)
		return OFF;
	else if(val == 0)
		return ON;
	else
		return UPING;
}

static int check_scu_status(u32 cluster, int state, int timeout)
{
	int curret;

	do {
		curret = get_scu_power_status(cluster);
		if(state == curret)
			return 0;
		if(timeout-- <= 0){
			return -1;
		}
		udelay(100);
	}while(1);
}

static int get_nb_cpus(u32 cluster)
{
	return cluster ? 4 : 1;
}

/* set HA7/SA7 warm jump regs */
static void warm_address_set(u32 cluster, u32 cpu, u32 addr)
{
	u32 jump_addr;

	jump_addr = cluster ? (CTL_AP_HA7_CORE0_WBOOT_ADDR + cpu * 2 * 0x4)
		: CTL_AP_SA7_CORE0_WBOOT_ADDR;

	__raw_writel(addr, io_p2v(jump_addr));
}

/* SCU auto power down set */
static void scu_auto_pd_set(u32 cluster, u32 enable)
{
	u32 reg, val;

	reg = cluster ? AP_PWR_HA7_SCU_PD_CTL : AP_PWR_SA7_SCU_PD_CTL;

	if(enable)
		val =  0x3 << 18; /* set bit 3 to 0 */
	else
		val = (0x3 << 2) | (0x3 << 18); /* set bit 3 to 1*/

	__raw_writel(val, io_p2v(reg));
}

/* set HA7/SA7 auto power down regs */
static void core_auto_pd_set(u32 cluster, u32 cpu, u32 enable)
{
	u32 val, reg;

	reg = cluster ? (AP_PWR_HA7_C0_PD_CTL + cpu  * 0x4)
		: AP_PWR_SA7_C_PD_CTL;

	if(enable)
		val = (0x1 << 4) | (0x1 << 20);
	else
		val = (0x1 << 20);

	__raw_writel(val, io_p2v(reg));
	dsb();
}

static void poweron_core(u32 cluster, u32 cpu)
{
	int state = ON;
	u32 reg, val;

	reg = cluster ? (AP_PWR_HA7_C0_PD_CTL + cpu  * 0x4)
		: AP_PWR_SA7_C_PD_CTL;

	if(!check_core_status(cluster, cpu, state, 1)){
		pr_warning("cluster%d, cpu%d, already powered\n", cluster, cpu);
		return;
	}

	val = (0x1 << 1) | (0x1 << 17);
	__raw_writel(val, io_p2v(reg));
	udelay(50);
	if(!check_core_status(cluster, cpu, state, 100))
		pr_debug("cluster%d cpu%d is powered\n", cluster, cpu);
	else
		pr_warning("cluster%d cpu%d is not powered\n", cluster, cpu);

	if(!check_scu_status(cluster, state, 100))
		pr_debug("cluster%d is powered\n", cluster);
	else
		pr_warning("cluster%d is not powered\n", cluster);
}

/**
 * cci_disable_port_by_cpu() - function to disable a CCI port by CPU
 *			       reference
 *
 * @mpidr: mpidr of the CPU whose CCI port should be disabled
 *
 * Disabling a CCI port for a CPU implies disabling the CCI port
 * controlling that CPU cluster. Code disabling CPU CCI ports
 * must make sure that the CPU running the code is the last active CPU
 * in the cluster ie all other CPUs are quiescent in a low power state.
 *
 * Return:
 *	0 on success
 */
static int  cci_disable_port_by_cpu(u64 mpidr)
{
	int cluster;
	u32 port, a7_ctl, val;

	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	port = cluster ? CCI_SNOOP_CTL4_HA7 : CCI_SNOOP_CTL3_SA7;
	a7_ctl = cluster ? CTL_AP_HA7_CTRL : CTL_AP_SA7_CTRL;

	val = __raw_readl(io_p2v(port));
	if(!(val & 0x3))
		goto disable_acinactm;

	val &= ~(0x3);
	__raw_writel(val, io_p2v(port));
	dsb();

	while(__raw_readl(io_p2v(CCI_SNOOP_STATUS)) & 0x1)
		cpu_relax();

disable_acinactm:
	/* if cci port disabled, disable A7 ACINACTM */
	if(!(__raw_readl(io_p2v(port)) & 0x3)){
		val = __raw_readl(io_p2v(a7_ctl));

		if(val & 0x1)
			return 0;

		val |= 0x1;
		__raw_writel(val, io_p2v(a7_ctl));
	}else
		panic("Disalbe cluster %d cci port Error!!\n", cluster);

	return 0;
}

static int pm_power_up(unsigned int cpu, unsigned int cluster)
{
	if (cluster >= MAX_NR_CLUSTERS ||
	    cpu >= get_nb_cpus(cluster))
		return -EINVAL;
	/*
	 * Since this is called with IRQs enabled, and no arch_spin_lock_irq
	 * variant exists, we need to disable IRQs manually here.
	 */
	local_irq_disable();
	arch_spin_lock(&pm_lock);

	/* Disable SCU auto power down */
	if (!pm_use_count[0][cluster] &&
	    !pm_use_count[1][cluster] &&
	    !pm_use_count[2][cluster] &&
	    !pm_use_count[3][cluster]){
		scu_auto_pd_set(cluster, 0);
		if(cpu)
			panic("Wrong cpu %d\n", cpu);
	}

	pm_use_count[cpu][cluster]++;
	if (pm_use_count[cpu][cluster] == 1) {
		/* Disable CPU auto powerdown */
		core_auto_pd_set(cluster, cpu, 1);
		warm_address_set(cluster, cpu, virt_to_phys(mcpm_entry_point));
		poweron_core(cluster, cpu);
	} else if (pm_use_count[cpu][cluster] != 2) {
		/*
		 * The only possible values are:
		 * 0 = CPU down
		 * 1 = CPU (still) up
		 * 2 = CPU requested to be up before it had a chance
		 *     to actually make itself down.
		 * Any other value is a bug.
		 */
		BUG();
	}

	arch_spin_unlock(&pm_lock);
	local_irq_enable();

	return 0;
}

static void pm_power_down_self(u64 residency)
{
	unsigned int mpidr, cpu, cluster;
	bool last_man = false, skip_wfi = false;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_debug("%s: cpu %u cluster %u\n", __func__, cpu, cluster);
	BUG_ON(cluster >= MAX_NR_CLUSTERS ||
	       cpu >= get_nb_cpus(cluster));

	__mcpm_cpu_going_down(cpu, cluster);

	arch_spin_lock(&pm_lock);
	BUG_ON(__mcpm_cluster_state(cluster) != CLUSTER_UP);
	pm_use_count[cpu][cluster]--;
	if (pm_use_count[cpu][cluster] == 0) {
		if (!pm_use_count[0][cluster] &&
		    !pm_use_count[1][cluster] &&
		    !pm_use_count[2][cluster] &&
		    !pm_use_count[3][cluster] &&
		    (!residency || residency > 5000)) {
			scu_auto_pd_set(cluster, 1);
			last_man = true;
		}
	} else if (pm_use_count[cpu][cluster] == 1) {
		/*
		 * A power_up request went ahead of us.
		 * Even if we do not want to shut this CPU down,
		 * the caller expects a certain state as if the WFI
		 * was aborted.  So let's continue with cache cleaning.
		 */
		printk("cpu is powered skip wfi !!\n");
		skip_wfi = true;
	} else
		BUG();

	/*
	 * If the CPU is committed to power down, make sure
	 * the power controller will be in charge of waking it
	 * up upon IRQ, ie IRQ lines are cut from GIC CPU IF
	 * to the CPU by disabling the GIC CPU IF to prevent wfi
	 * from completing execution behind power controller back
	 */
	if (!skip_wfi)
		gic_cpu_if_down();

	/* Enable CPU auto powerdown */
	core_auto_pd_set(cluster, cpu, 0);

	if (last_man && __mcpm_outbound_enter_critical(cpu, cluster)) {
		arch_spin_unlock(&pm_lock);

		/* Flush all cache levels for this cluster. */
		v7_exit_coherency_flush(all);
		cci_disable_port_by_cpu(mpidr);
		__mcpm_outbound_leave_critical(cluster, CLUSTER_DOWN);
		freq_step_set(cluster, 0);
	} else {
		/*
		 * If last man then undo any setup done previously.
		 */
		if (last_man)
			scu_auto_pd_set(cluster, 0);

		arch_spin_unlock(&pm_lock);
		v7_exit_coherency_flush(louis);
	}

	__mcpm_cpu_down(cpu, cluster);

	/* Now we are prepared for power-down, do it: */
	if (!skip_wfi){
		while (1) {
			dsb();
			wfi();
			isb();
		}
	}
	/* Not dead at this point?  Let our caller cope. */
}

static void pm_power_down(void)
{
	pm_power_down_self(0);
}

static void pm_power_suspend(u64 residency)
{
	unsigned int mpidr, cpu, cluster;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	warm_address_set(cluster, cpu, virt_to_phys(lc186x_bL_resume));
	pm_power_down_self(residency);
}

static void pm_powered_up(void)
{
	unsigned int mpidr, cluster;

	mpidr = read_cpuid_mpidr();
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	freq_step_set(cluster, 1);
}

static int pm_powered_down(unsigned int cpu, unsigned int cluster)
{
	int i = 0;

	while(1) {
		if (!check_core_status(cluster, cpu, OFF, 50))
			break;
		if (++i > 30)
			return -ETIMEDOUT;
	}
	return 0;
}

static const struct mcpm_platform_ops lc186x_pm_power_ops = {
	.power_up	= pm_power_up,
	.power_down	= pm_power_down,
	.suspend	= pm_power_suspend,
	.powered_up	= pm_powered_up,
	.powered_down 	= pm_powered_down,
};

static void __init pm_usage_count_init(void)
{
	unsigned int mpidr, cpu, cluster;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_debug("boot cpu: cpu%u cluster%u\n", cpu, cluster);
	BUG_ON(cluster >= MAX_NR_CLUSTERS ||
	       cpu >= get_nb_cpus(cluster));

	pm_use_count[cpu][cluster] = 1;
}

static int notrace core_suspend_finisher(unsigned long arg)
{
	struct suspend_args *para = (struct suspend_args *)arg;

	/* Flush all cache levels for this cluster. */
	v7_exit_coherency_flush(all);
	cci_disable_port_by_cpu(para->mpid);
	while (1) {
		dsb();
		wfi();
		isb();
	}

	return -ETIMEDOUT;
}

static int __ref pm_suspend_enter(suspend_state_t state)
{
	unsigned int mpidr, cpu, cluster, status, ret;
	struct suspend_args arg;

	if (interrupt_is_pending()){
		printk("INT is pending, exit \n");
		return 0;
	}
	gic_cpu_if_down();

	status = device_idle_state();
	printk(KERN_DEBUG "device idle state:0x%x\n", status);

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	warm_address_set(cluster, cpu,
		virt_to_phys(comip1860_resume));

	sleep_enable(1);
	scu_auto_pd_set(cluster, 1);
	core_auto_pd_enable(cluster, cpu, 1);
	freq_step_set(cluster, 0);
	arg.mpid = mpidr;
	arg.flag = 0;
	ret = cpu_suspend((unsigned long)&arg, core_suspend_finisher);
	freq_step_set(cluster, 1);
	scu_auto_pd_set(cluster, 0);
	core_auto_pd_enable(cluster, cpu, 0);
	sleep_enable(0);

	return 0;
}

static struct platform_suspend_ops lc186x_suspend_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= pm_suspend_enter,
};

 static void pm_register_init(void)
{
	u32 reg;

	reg = 0 << AP_PWR_HA7C0_SLP_EN |
		1 << AP_PWR_HA7C0_SLP_EN_WE;
	reg |= 1 << AP_PWR_HA7C1_SLP_EN |
		1 << AP_PWR_HA7C1_SLP_EN_WE;
	reg |= 1 << AP_PWR_HA7C2_SLP_EN |
		1 << AP_PWR_HA7C2_SLP_EN_WE;
	reg |= 1 << AP_PWR_HA7C3_SLP_EN |
		1 << AP_PWR_HA7C3_SLP_EN_WE;
	reg |= 1 << AP_PWR_SA7_SLP_EN |
		1 << AP_PWR_SA7_SLP_EN_WE;

	reg |= 0 << AP_PWR_SA7_SLP_MK |
		1 << AP_PWR_SA7_SLP_MK_WE;
	reg |= 0 << AP_PWR_SA7_L2C_SLP_MK |
		1 << AP_PWR_SA7_L2C_SLP_MK_WE;
	reg |= 0 << AP_PWR_HA7_L2C_SLP_MK |
		1 << AP_PWR_HA7_L2C_SLP_MK_WE;

	reg |= 0 << AP_PWR_HA7C1_SLP_MK |
		1 << AP_PWR_HA7C1_SLP_MK_WE;
	reg |= 0 << AP_PWR_HA7C2_SLP_MK |
		1 << AP_PWR_HA7C2_SLP_MK_WE;
	reg |= 0 << AP_PWR_HA7C3_SLP_MK |
		1 << AP_PWR_HA7C3_SLP_MK_WE;

	reg |= 0 << AP_PWR_DMA_SLP_MK |
		1 << AP_PWR_DMA_SLP_MK_WE;
	reg |= 0 << AP_PWR_POWER_SLP_MK |
		1 << AP_PWR_POWER_SLP_MK_WE;
	reg |= 0 << AP_PWR_ADB_SLP_MK |
		1 << AP_PWR_ADB_SLP_MK_WE;
	reg |= 0 << AP_PWR_CS_SLP_MK |
		1 << AP_PWR_CS_SLP_MK_WE;
	__raw_writel(reg, io_p2v(AP_PWR_SLPCTL0));

	reg = __raw_readl(io_p2v(AP_PWR_SLPCTL1));
	reg |= 1 << 2;
	__raw_writel(reg, io_p2v(AP_PWR_SLPCTL1));

	reg = 1 << AP_PWR_CORE_WK_ACK_MK |
		1 << AP_PWR_CORE_WK_ACK_MK_WE;
	reg |= 0 << AP_PWR_CORE_PU_CDBGPWRUPREQ_MK |
		1 << AP_PWR_CORE_PU_CDBGPWRUPREQ_MK_WE;
	reg |= 0 << AP_PWR_CORE_PU_INTR_MK |
		1 << AP_PWR_CORE_PU_INTR_MK_WE;
	__raw_writel(reg, io_p2v(AP_PWR_HA7_C0_PD_CTL));

	reg = __raw_readl(io_p2v(AP_PWR_INTST_A7));

	reg &= 1 << AP_PWR_CSPWRREQ_INTR;
	if (reg)
		__raw_writel(reg, io_p2v(AP_PWR_INTST_A7));

	/* Donot power witch SCU */
	reg = 0 << 6 | 1 << (6 + 16); /* PU_WITH_SCU = 1 */
	reg |= 0 << 3 | 1 << (3 + 16); /* PD_MK = 0 */
	reg |= 1 << 5 | 1 << (5 + 16); /*Disable power with INTR */
	__raw_writel(reg, io_p2v(AP_PWR_HA7_C1_PD_CTL));
	__raw_writel(reg, io_p2v(AP_PWR_HA7_C2_PD_CTL));
	__raw_writel(reg, io_p2v(AP_PWR_HA7_C3_PD_CTL));

	reg = 0 << 0 | 0xf << (0 + 16); /* L1RSTDISABLE = 0 */
	reg |= 0 << 4 | 1 << (1 + 16);  /* L2RSTDISABLE = 0 */
	__raw_writel(reg, io_p2v(AP_PWR_HA7_RSTCTL1));
	reg = 0 << 0 | 1 << (0 + 16); /* L1RSTDISABLE = 0 */
	reg |= 0 << 1 | 1 << (1 + 16);  /* L2RSTDISABLE = 0 */
	__raw_writel(reg, io_p2v(AP_PWR_SA7_RSTCTL1));

	reg = __raw_readl(io_p2v(AP_PWR_PLLCR));
	reg |= ((0x3f << 8) | 0xff);
	__raw_writel(reg, io_p2v(AP_PWR_PLLCR));

	__raw_writel(0xa, io_p2v(AP_PWR_HA7_C_PD_CNT1));
	__raw_writel(0x100, io_p2v(AP_PWR_HA7_C_PD_CNT2));
	__raw_writel(0x100, io_p2v(AP_PWR_HA7_C_PD_CNT3));
       /* set SA7 PD cont */
       __raw_writel(0xa, io_p2v(AP_PWR_SA7_C_PD_CNT1));
       __raw_writel(0x100, io_p2v(AP_PWR_SA7_C_PD_CNT2));
       __raw_writel(0x100, io_p2v(AP_PWR_SA7_C_PD_CNT3));
       __raw_writel(0x100, io_p2v(AP_PWR_SA7_SCU_PD_CNT1));
       __raw_writel(0x100, io_p2v(AP_PWR_SA7_SCU_PD_CNT2));
       __raw_writel(0x100, io_p2v(AP_PWR_SA7_SCU_PD_CNT3));

       /* Set HA7 SCU PD COUNT */
       __raw_writel(0x100, io_p2v(AP_PWR_HA7_SCU_PD_CNT1));
       __raw_writel(0x100, io_p2v(AP_PWR_HA7_SCU_PD_CNT2));
       __raw_writel(0x100, io_p2v(AP_PWR_HA7_SCU_PD_CNT3));
	dsb();
}

static int __init lc186x_pm_init(void)
{
	int ret;

	pm_register_init();
	pm_usage_count_init();
	suspend_set_ops(&lc186x_suspend_ops);
	ret = mcpm_platform_register(&lc186x_pm_power_ops);
	if (!ret)
		ret = mcpm_sync_init(lc186x_bL_pm_power_up_setup);

	pr_info("pm initialized %s \n", ret ? "fail" : "success");
	return ret;
}

early_initcall(lc186x_pm_init);
