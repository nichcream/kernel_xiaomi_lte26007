/*
 * arch/arm/mach-comip/suspend.c
 *
 * CPU pm suspend & resume functions for comip SoCs
 *
 * Copyright (c) 2012, LC Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/serial_reg.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/regulator/machine.h>
#include <linux/cpu_pm.h>
#include <linux/irqchip/arm-gic.h>

#include <asm/suspend.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>
#include <asm/localtimer.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>
#include <asm/idmap.h>
#include <asm/suspend.h>
#include <asm/cp15.h>

#include <mach/irqs.h>
#include <mach/suspend.h>
#include <plat/hardware.h>
#include <plat/bootinfo.h>

#define COMIP_SUSP_DEBUG 1
#if COMIP_SUSP_DEBUG
#define CPUSUSP_PR(fmt, args...)	printk(KERN_INFO fmt, ## args)
#else
#define CPUSUSP_PR(fmt, args...)	printk(KERN_DEBUG fmt, ## args)
#endif

struct suspend_args {
	unsigned int cpu;
	unsigned int flag;
};
#define AP_PWR_HA7Cx_PD_CTL(X)	(AP_PWR_HA7_C0_PD_CTL + (X)*4)

#define CLUSTER_SLEEP_FREQ	4

#define BIT_WE_OFF				16
#define PD_EN					0
#define WK_UP					1
#define PD_MK					3
#define AUTO_PD_MK				4
#define PU_INTR_MK				5
#define PU_WITH_SCU			6
#define PU_CDBGPWRUPREQ_MK	7

#define A7CX_PD_ST(x)	(((x) + 1) * 4) // X=1,2,3
#define PWR_ON		0
#define PWR_DOWN	7
#define IRQ_BUFLEN	3

static unsigned int cpu_w_addr_reset;
static unsigned int cpu_w_addr_continue;
static unsigned int cpu_w_addr_resume;
static void (*comip_cpu_wfi)(unsigned int flag);
unsigned int device_idle_state(void);
extern unsigned long secondary_startup_addr;
extern unsigned long comip_mmu_off_addr;
extern void comip_mmu_off(void);
extern void secondary_startup(void);
extern void comip_secondary_startup(void);
extern void comip_mmu_close(unsigned int, unsigned int);
extern void comip1860_resume(void);
extern void comip1860_continue(void);
extern void sram_cpu_wfi(unsigned int flag);

static  unsigned int a7cx_slp_addr[] = {
	CPU0_SLP_STATE_ADDR,
	CPU1_SLP_STATE_ADDR,
	CPU2_SLP_STATE_ADDR,
	CPU3_SLP_STATE_ADDR,
	CPU4_SLP_STATE_ADDR,
};

static unsigned int gic_irq_enable[4];
static unsigned int gic_irq_wakeup[IRQ_BUFLEN];

#ifdef CONFIG_SLEEP_CODE_IN_RAM
#define symbol_sram_va(sym) \
		((sram_base) + ((sym) - (ddr_base)))

#define symbol_sram_pa(sym) \
		(u32)io_v2p(symbol_sram_va(sym));

#define symbol_fixed_ddr(sym, value) \
	do { \
		*(u32 *)sym = symbol_sram_pa(value)); \
	} while (0)

#define symbol_fixed_sram(sym, value) \
	do { \
		void *n; \
		n = (void *)symbol_sram_va(sym); \
		*(u32 *)n = symbol_sram_pa(value)); \
	} while (0)

#define COMIP_SRAM_SZ	(0x1000)
#define COMIP_SRAM_PA	(AP_SEC_RAM_BASE + 0x10000 - COMIP_SRAM_SZ)

static unsigned int comip_sram_start;
static unsigned int comip_sram_base;
static unsigned int comip_sram_size;
static unsigned int comip_sram_ceil;

#ifdef CONFIG_DDR_FREQ_ADJUST
static unsigned int ddr_down_ret;
static unsigned int ddr_up_ret;
extern unsigned long ddr_freq_down_ret;
extern unsigned long ddr_freq_up_ret;
#endif

extern unsigned long comip_ram_code_sz;
extern void comip_ddr2ram(unsigned int flag);

static void* comip_sram_push(void *start, unsigned int size)
{
	if (size > (comip_sram_ceil - comip_sram_base )) {
		printk(KERN_ERR "Not enough space in SRAM\n");
		return NULL;
	}
	comip_sram_ceil -= size;
	comip_sram_ceil = round_down(comip_sram_ceil, L1_CACHE_BYTES);
	memcpy((void *)comip_sram_ceil, start, size);
	return (void *)comip_sram_ceil;
}

static void __init comip_map_sram(void)
{
	comip_sram_start = COMIP_SRAM_PA;
	comip_sram_size = COMIP_SRAM_SZ;
	comip_sram_base = io_p2v(COMIP_SRAM_PA);
	comip_sram_ceil = comip_sram_base + comip_sram_size - 4;
}

static void cpu_suspend_end(unsigned long flag)
{
	unsigned int addr_phy = io_v2p((unsigned int)comip_cpu_wfi);
	setup_mm_for_reboot();
	comip_mmu_close(addr_phy, flag);
}

#endif

void comip_reset_to_phy(unsigned int addr_phy)
{
	setup_mm_for_reboot();
	comip_mmu_close(addr_phy, 0);
}

static inline void a7cx_slp_state_set(unsigned int cpu, int state)
{
	unsigned int addr = 0;

	addr = *(a7cx_slp_addr + cpu);
	__raw_writel(state, io_p2v(addr));
	dsb();
}

static inline unsigned char a7cx_slp_state_get(unsigned int cpu)
{
	unsigned int addr = 0;

	addr = *(a7cx_slp_addr + cpu);
	smp_rmb();
	return __raw_readl(io_p2v(addr));
}

static inline void wakeup_cmd_set(unsigned int cpu, int cmd)
{
	unsigned int val;

	val = cmd | cpu << COMIP_WAKEUP_ID_OFFSET;
	__raw_writel(val, io_p2v(CPU0_WAKEUP_CMD_ADDR));

	dsb();
}

static void wakeup_cpu(unsigned int cpu, int sig)
{
	long timeout;
	long cycle = 0;
	int count = 2;
	unsigned int val;

tryagain:
	/*indicate cpu to jump out of loop*/
	wakeup_cmd_set(cpu, COMIP_WAKEUP_CMD_WAKING);

	if (sig)
		arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	timeout = 10000;
	val = 0;
	while (1) {
		sev();
		val = a7cx_slp_state_get(cpu);
		if (val == COMIP_SLP_STATE_UP ||
			val == COMIP_SLP_STATE_ON)
			break;
		if (timeout-- <= 0)
			break;
		udelay(5);
		++cycle;
	}

	if (timeout <= 0 && --count > 0) {
		printk("cpu%d timeout:%ld\n", cpu, cycle);
		goto tryagain;
	}

	wakeup_cmd_set(cpu, COMIP_WAKEUP_CMD_DONE);
}

void comip_wake_up_cpux(unsigned int cpu)
{
		wakeup_cpu(cpu, 0);
}

void comip_warm_up_cpux(unsigned int cpu)
{
	wakeup_cpu(cpu, 0);
}

static inline void  a7cx_warm_addr_set(unsigned int cpu,
	unsigned int restart_addr)
{
	unsigned int cpu_w_regs[] = {
		CPU0_WBOOT_ADDR, CPU1_WBOOT_ADDR,
		CPU2_WBOOT_ADDR, CPU3_WBOOT_ADDR
	};

	__raw_writel(restart_addr, io_p2v(cpu_w_regs[cpu]));
	dsb();
}

static void inline symbol_reloc(void *sym, unsigned int value)
{
	*(u32 *)sym = (u32)(value);
}

static int __init sym_reloc_init(void)
{
#ifdef CONFIG_SLEEP_CODE_IN_RAM
	void *sram_base;
	void *ddr_base = (void*)comip1860_resume;
	unsigned int val;
#endif
	symbol_reloc(&secondary_startup_addr,
			(unsigned int)virt_to_phys(secondary_startup));
	symbol_reloc(&comip_mmu_off_addr,
			(unsigned int)virt_to_phys(comip_mmu_off));

#ifdef CONFIG_SLEEP_CODE_IN_RAM
	comip_map_sram();
	sram_base = comip_sram_push(ddr_base, comip_ram_code_sz);
	if (!sram_base)
		return -ENOMEM;
	flush_icache_range((u32)sram_base,
			(u32)(sram_base + comip_ram_code_sz));

	val = symbol_sram_pa((void*)comip_secondary_startup);
	cpu_w_addr_reset = val;

	val = symbol_sram_pa((void*)comip1860_continue);
	a7cx_warm_addr_set(CPU_ID_0, val);
	a7cx_warm_addr_set(CPU_ID_1, val);
	a7cx_warm_addr_set(CPU_ID_2, val);
	a7cx_warm_addr_set(CPU_ID_3, val);
	cpu_w_addr_continue = val;

	val = symbol_sram_pa((void*)comip1860_resume);
	cpu_w_addr_resume = val;

	comip_cpu_wfi = (void*)symbol_sram_va((void*)comip_ddr2ram);
#ifdef CONFIG_DDR_FREQ_ADJUST
	ddr_down_ret = (unsigned int)symbol_sram_va((void*)&ddr_freq_down_ret);
	ddr_up_ret = (unsigned int)symbol_sram_va((void*)&ddr_freq_up_ret);
#endif
#endif
	flush_cache_all();
	outer_flush_all();

	return 0;
}

static inline void wake_int_do(int flag)
{
	struct irq_desc *desc;
	struct irq_data *data;
	struct irq_chip *chip;
	int irq;

	for_each_irq_desc(irq, desc) {
		if (desc) {
			data = irq_desc_get_irq_data(desc);
			if (data && irqd_is_wakeup_set(data)) {
				chip = irq_data_get_irq_chip(data);
				if (chip) {
					if (flag && chip->irq_unmask)
						chip->irq_unmask(data);
					else if (!flag && chip->irq_mask)
						chip->irq_mask(data);
				}
			}
		}
	}
}

void enable_irq_print(void)
{
	unsigned int i, val;

	/* GIC enable irq */
	for(i = 0; i < 4; i++){
		val = __raw_readl(io_p2v(AP_GICD_BASE +
			GIC_DIST_ENABLE_SET + i * 0x4));
		printk("enable irq is 0x%x\n", val);
	}

	/*GPIO enable irq */
	for (i = 0; i < 8; i++) {
		val = __raw_readl(io_p2v(GPIO_BASE
						+ GPIO_INTR_MASK(0) + (i * 4)));
		printk("gpio irq is 0x%x\n", val);
	}

	printk("mailbox irq is 0x%x\n", __raw_readl(io_p2v(TOP_MAIL_APA7_INTR_EN)));
	printk("AP PWR irq is 0x%x\n", __raw_readl(io_p2v(AP_PWR_INTEN_A7)));

}

static inline void read_wakeup_irq(void)
{
	int i;

	for(i = 0; i < IRQ_BUFLEN; i++)
		gic_irq_wakeup[i] = __raw_readl(io_p2v(AP_GICD_BASE +
			GIC_DIST_ENABLE_SET + (i + 1) * 0x4));
}

static inline void wake_int_unmask(void)
{
	int i;
	/* Read gic enalbed irq */
	for(i = 0; i < 4; i++)
		gic_irq_enable[i] = __raw_readl(io_p2v(AP_GICD_BASE +
			GIC_DIST_ENABLE_SET + i * 0x4));
	/* Clear all not wakeup irq */
	for(i = 0; i < 4; i++)
		__raw_writel(gic_irq_enable[i], io_p2v(AP_GICD_BASE +
			GIC_DIST_ENABLE_CLEAR + i * 0x4));

	wake_int_do(1);
	read_wakeup_irq();
}

static inline void wake_int_mask(void)
{
	int i;

	wake_int_do(0);
	/* Restor enabled gic irq */
	for(i = 0; i < 4; i++)
		__raw_writel(gic_irq_enable[i], io_p2v(AP_GICD_BASE +
			GIC_DIST_ENABLE_SET + i * 0x4));
}

int interrupt_is_pending(void)
{
	unsigned int val;

	val = __raw_readl(io_p2v(AP_GICC_BASE + GIC_CPU_HIGHPRI)) & 0x3ff;
	return COMIP_SPURIOUS_INT_ID != val;
}

static inline void
reg_ctrl_set(unsigned int addr, unsigned int bit, bool s1c0)
{
	unsigned int val;

	if (s1c0)
		val = 1 << bit;
	else
		val = 0 << bit;
	val |= 1 << (bit + BIT_WE_OFF);
	__raw_writel(val, io_p2v(addr));
	dsb();
}

static inline void  ap_sleep_enable(void)
{
	reg_ctrl_set(AP_PWR_SLPCTL0, AP_PWR_SA7_SLP_EN, 1);
	reg_ctrl_set(AP_PWR_SLPCTL0, AP_PWR_HA7C0_SLP_EN, 1);
}

static inline void  ap_sleep_disable(void)
{
	reg_ctrl_set(AP_PWR_SLPCTL0, AP_PWR_SA7_SLP_EN, 0);
	reg_ctrl_set(AP_PWR_SLPCTL0, AP_PWR_HA7C0_SLP_EN, 0);
}

static inline void ha7_scu_pd_enable(void)
{
	reg_ctrl_set(AP_PWR_HA7_SCU_PD_CTL, AP_PWR_SCU_PD_MK, 0);
	reg_ctrl_set(AP_PWR_HA7_SCU_PD_CTL, AP_PWR_SCU_AUTO_PD_MK, 0);
}

static inline void ha7_scu_pd_disable(void)
{
	reg_ctrl_set(AP_PWR_HA7_SCU_PD_CTL, AP_PWR_SCU_PD_MK, 1);
	reg_ctrl_set(AP_PWR_HA7_SCU_PD_CTL, AP_PWR_SCU_AUTO_PD_MK, 1);
}

static inline void  ap_pd_enable(void)
{
	reg_ctrl_set(AP_PWR_HA7_C0_PD_CTL, AP_PWR_CORE_PU_INTR_MK, 0);
	reg_ctrl_set(AP_PWR_HA7_C0_PD_CTL, AP_PWR_CORE_AUTO_PD_MK, 0);
}

static inline void  ap_pd_disable(void)
{
	reg_ctrl_set(AP_PWR_HA7_C0_PD_CTL, AP_PWR_CORE_PU_INTR_MK, 1);
	reg_ctrl_set(AP_PWR_HA7_C0_PD_CTL, AP_PWR_CORE_AUTO_PD_MK, 1);
}

static inline void  ap_int_wake_enable(void)
{
	reg_ctrl_set(AP_PWR_HA7_C0_PD_CTL, AP_PWR_CORE_PU_INTR_MK, 0);
}

static inline void  ap_int_wake_disable(void)
{
	reg_ctrl_set(AP_PWR_HA7_C0_PD_CTL, AP_PWR_CORE_PU_INTR_MK, 1);
}

static inline void
pd_ctl_reg_set(unsigned int cpu, unsigned int bit, bool s1c0)
{
	reg_ctrl_set(AP_PWR_HA7Cx_PD_CTL(cpu), bit, s1c0);
}

static inline void  a7cx_auto_pd_enable(unsigned int cpu )
{
	pd_ctl_reg_set(cpu, AUTO_PD_MK, 0);
}

static inline void  a7cx_auto_pd_disable(unsigned int cpu )
{
	pd_ctl_reg_set(cpu, AUTO_PD_MK, 1);
}

static inline void  a7cx_pu_by_irq_en(unsigned int cpu )
{
	pd_ctl_reg_set(cpu, PU_INTR_MK, 0);
}

static inline void  a7cx_pu_by_irq_dis(unsigned int cpu )
{
	pd_ctl_reg_set(cpu, PU_INTR_MK, 1);

}

static inline int
a7cx_is_pd_pu(int cpu, int state, int timeout)
{
	unsigned int val;
#define SA7_C_PD_ST_BIT	28
	do {
		dsb();
		val = __raw_readl(io_p2v(AP_PWR_PDFSM_ST0));
		if(cpu == 4)
			val = val >> SA7_C_PD_ST_BIT;
		else
			val = val >> A7CX_PD_ST(cpu);

		val &= 0x07;
		if (state == val)
			return 1;
		if (timeout-- <= 0)
			break;
		udelay(10);
	} while (1);
	return 0;
}

static int cpu_poweroff_forced(int cpu)
{
	pd_ctl_reg_set(cpu, PD_EN, 1);
	if (a7cx_is_pd_pu(cpu, PWR_DOWN, 2))  /*wait for 20 us*/
		CPUSUSP_PR("cpu%d: powered-off\n", cpu);
	else
		pr_err("cpu%d is not power down!\n", cpu);
	pd_ctl_reg_set(cpu, PD_EN, 0);

	return 0;
}

int suspend_cpu_poweroff(int cpu)
{
	if (a7cx_is_pd_pu(cpu, PWR_DOWN, 1000)) { /*wait for 10 ms*/
		CPUSUSP_PR("cpu%d: auto power-down\n", cpu);
		return 0;
	}
	cpu_poweroff_forced(cpu);

	WARN(COMIP_SLP_STATE_DOWN != a7cx_slp_state_get(cpu),
		"cpu%d state is not correct\n", cpu);
	return 0;
}

int suspend_cpu_poweron(int cpu)
{
	if (a7cx_is_pd_pu(cpu, PWR_ON, 1))  {/*wait for 10 us*/
		CPUSUSP_PR("cpu%d: already be powered-up\n", cpu);
		return 0;
	}

	pd_ctl_reg_set(cpu, WK_UP, 1);
	if (a7cx_is_pd_pu(cpu, PWR_ON, 2))  /*wait for 20 us*/
		CPUSUSP_PR("cpu%d: powered-up\n", cpu);
	else
		pr_err("cpu%d is not power on!\n", cpu);
	pd_ctl_reg_set(cpu, WK_UP, 0);
	return 0;
}

static void a7cx_power_down(unsigned int cpu)
{
	BUG_ON(CPU_ID_0 == cpu);

	set_cr(get_cr() & ~CR_C);
	flush_cache_louis();
	a7cx_slp_state_set(cpu, COMIP_SLP_STATE_DOWN);
	asm volatile ("clrex");
	set_auxcr(get_auxcr() & ~(1 << 6));

	while (1) {
		dsb();
		wfi();
		isb();
	}
}

int suspend_cpu_die(int cpu)
{
	a7cx_warm_addr_set(cpu, cpu_w_addr_reset);
	a7cx_slp_state_set(cpu, COMIP_SLP_STATE_DYING);
	a7cx_pu_by_irq_dis(cpu);
	a7cx_auto_pd_enable(cpu);
	a7cx_power_down(cpu);
	return 0;
}

void suspend_secondary_init(unsigned int cpu)
{
	a7cx_warm_addr_set(cpu, cpu_w_addr_continue);
	a7cx_slp_state_set(cpu, COMIP_SLP_STATE_ON);
	a7cx_pu_by_irq_en(cpu);
	a7cx_auto_pd_disable(cpu);
}

void sleep_state_print(void)
{
	printk("AP_PWR_SLPCTL0 is 0x%x\n", __raw_readl(io_p2v(AP_PWR_SLPCTL0)));
	printk("AP_PWR_DEVICE_ST is 0x%x\n", __raw_readl(io_p2v(AP_PWR_DEVICE_ST)));
}

/* Disable HA7 L2C memory access by other master */
void ha7_acinactm_disable(void)
{
       u32 val;

       val = __raw_readl(io_p2v(CCI_SNOOP_CTL4_HA7));
       if (!(val & 0x3)) {
	       val = __raw_readl(io_p2v(CTL_AP_HA7_CTRL));
	       if(val & 0x1)
		       return;
	       val |= 0x1;
	       __raw_writel(val, io_p2v(CTL_AP_HA7_CTRL));
       } else
	       panic("HA7 CCI SNOOP is not close!!\n");
}

static int __ref notrace cpu_suspend_finisher(unsigned long arg)
{
	unsigned int cpu;
	unsigned int flag;

	struct suspend_args *p = (struct suspend_args *)arg;

	if (interrupt_is_pending()){
		printk("Warnning have pending irq, exit sleeping\n");
		return 1;
	}

	sleep_state_print();
	cpu = p->cpu;
	flag = p->flag;
	set_cr(get_cr() & ~CR_C);
	if (CPU_ID_0 == cpu)
		flush_cache_all();
	else
		flush_cache_louis();
	if (interrupt_is_pending()){
		printk("Warnning have pending irq, exit sleeping\n");
		goto out;
	}
	a7cx_slp_state_set(cpu, COMIP_SLP_STATE_DOWN);
	gic_cpu_if_down();
	ha7_acinactm_disable();
	if (flag & FLAG_IDLE_PD_VAL)
		sram_cpu_wfi(flag);
	else
#ifdef CONFIG_SLEEP_CODE_IN_RAM
	if (flag & FLAG_MMUOFF_VAL)
		cpu_suspend_end(flag);
	else
#endif
		comip_cpu_wfi(flag);

out:
#ifndef CONFIG_CPU_DCACHE_DISABLE
	set_cr(get_cr() | CR_C);
#endif

	return 1;
}

static void poweroff_unused_cpus_forced(void)
{
	unsigned int val;
	int timeout = 1;

	pd_ctl_reg_set(CPU_ID_1, PD_EN, 1);
	pd_ctl_reg_set(CPU_ID_2, PD_EN, 1);
	pd_ctl_reg_set(CPU_ID_3, PD_EN, 1);

	do {
		val = __raw_readl(io_p2v(AP_PWR_PDFSM_ST0));
		val &= 0x7770;
		if (0x7770 == val)
			break;
		if (timeout-- <= 0)
			break;
		udelay(10);
	} while (1);

	pd_ctl_reg_set(CPU_ID_1, PD_EN, 0);
	pd_ctl_reg_set(CPU_ID_2, PD_EN, 0);
	pd_ctl_reg_set(CPU_ID_3, PD_EN, 0);
}

static void cluster_freq_restore(unsigned int freq)
{
	unsigned int val;

	val = (freq << 0) | (0x1f << (0 + BIT_WE_OFF));
	__raw_writel(val, io_p2v(AP_PWR_HA7_CLK_CTL));
	dsb();
	udelay(1);
}

static unsigned int cluster_freq_set(unsigned int freq)
{
	unsigned int freq_val;
	unsigned int val;

	val = __raw_readl(io_p2v(AP_PWR_HA7_CLK_CTL));
	freq_val = val & 0x1f;
	val = (freq << 0) | (0x1f << (0 + BIT_WE_OFF));
	__raw_writel(val, io_p2v(AP_PWR_HA7_CLK_CTL));
	dsb();
	udelay(1);

	return freq_val;
}

static int core_power_down(unsigned int cpu,
	unsigned int flag)
{
	struct suspend_args arg = {cpu, flag};
	int ret;

	a7cx_slp_state_set(cpu, COMIP_SLP_STATE_DYING);
	ret = cpu_suspend((unsigned long)&arg,
				cpu_suspend_finisher);
	a7cx_slp_state_set(cpu, COMIP_SLP_STATE_ON);

	return ret;
}

static int power_down_cpux(unsigned int cpu)
{
	int ret;

	ret = cpu_pm_enter();
	if (ret)
		return ret;
	if (interrupt_is_pending()){
		printk("Warnning have pending irq, exit sleeping\n");
		return 1;
	}
	ret = core_power_down(cpu, FLAG_IDLE_PD_VAL);
	if (0 == ret) {
		cpu_pm_exit();
	}
	return ret;
}

static int power_down_cpu0(void)
{
	unsigned int freq;
	int ret;

	ret = cpu_pm_enter();
	if (ret)
		return ret;
	ret = cpu_cluster_pm_enter();
	if (ret)
		return ret;
	if (interrupt_is_pending())
		return 1;

	freq= cluster_freq_set(CLUSTER_SLEEP_FREQ); /*4/16 pll0_out*/
	ret = core_power_down(CPU_ID_0, FLAG_IDLE_PD_VAL);
	cluster_freq_restore(freq);
	if (0 == ret) {
		cpu_cluster_pm_exit();
		cpu_pm_exit();
	}

	return ret;
}

int suspend_only_cpu0_on(void)
{
	unsigned int val;

	val = __raw_readl(io_p2v(AP_PWR_PDFSM_ST0));
	val &= 0x77777;
	if (0x77700 == val)
		return 1;

	return 0;
}

int suspend_idle_poweroff(unsigned int cpu)
{
	int ret;

	if (interrupt_is_pending())
		return 1;

	if (CPU_ID_0 == cpu) {
		ap_int_wake_enable();
		ap_pd_enable();
		barrier();
		ret = power_down_cpu0();
		barrier();
		ap_int_wake_disable();
		ap_pd_disable();
		if (0 == ret)
			poweroff_unused_cpus_forced();
	} else {
		a7cx_auto_pd_enable(cpu);
		barrier();
		ret = power_down_cpux(cpu);
		barrier();
		a7cx_auto_pd_disable(cpu);
	}

	return ret;
}
static void choose_module_irq_print(unsigned int irq)
{
	unsigned int status;
	int i;
	int cur;
	irq += 32;

	if (irq == INT_GPIO) {
		for (i = 0; i < 8; i++) {
			status = __raw_readl(io_p2v(GPIO_BASE
						+ GPIO_INTR_STATUS(0) + (i * 4)));
			if (status) {
				cur = find_first_bit((const unsigned long *)&status, BITS_PER_LONG);
				if (cur < BITS_PER_LONG)
					printk(KERN_DEBUG "exit comip suspend by gpio%d\n",
							cur + i * 32);
				else
					continue;

				while (cur < BITS_PER_LONG) {
					cur = find_next_bit((const unsigned long *)&status, BITS_PER_LONG, cur + 1);
					if (cur >= BITS_PER_LONG)
						break;
					printk(KERN_DEBUG "exit comip suspend by gpio%d\n",
							cur + i * 32);
				}
			}
		}
	} else if (irq == INT_MAILBOX) {
		printk(KERN_DEBUG "exit comip suspend by modem 0x%08x|0x%08x|0x%08x|0x%08x \n",
				__raw_readl(io_p2v(TOP_MAIL_APA7_INTR_STA_RAW0)),
				__raw_readl(io_p2v(TOP_MAIL_APA7_INTR_STA_RAW1)),
				__raw_readl(io_p2v(TOP_MAIL_APA7_INTR_STA_RAW2)),
				__raw_readl(io_p2v(TOP_MAIL_APA7_INTR_STA_RAW3)));
	} else {
		printk(KERN_DEBUG "exit comip suspend by irq %d\n", irq);
	}
}

static void wakeup_irq_print(void)
{
	int nmaskbits = IRQ_BUFLEN * BITS_PER_LONG;
	unsigned int pending;
	int cur, i;
	unsigned int irq[IRQ_BUFLEN];

	for(i = 0; i < IRQ_BUFLEN; i++){
		/* omit irq0~irq31 */
		pending = __raw_readl(io_p2v(AP_GICD_BASE +
			GIC_DIST_PENDING_SET + (i + 1) * 0x4));
		irq[i] = pending;
	}

	cur = find_first_bit((const unsigned long *)irq, nmaskbits);

	if(cur < nmaskbits)
		choose_module_irq_print(cur);
	else
		return;

	while (cur < nmaskbits) {
		cur = find_next_bit((const unsigned long *)irq, nmaskbits, cur + 1);
		if (cur >= nmaskbits)
			break;
		choose_module_irq_print(cur);
	}
}

unsigned int device_idle_state(void)
{
	dsb();
	return __raw_readl(io_p2v(AP_PWR_DEVICE_ST));
}

void arch_suspend_disable_irqs(void)
{
	local_irq_disable();
	local_fiq_disable();
	wake_int_unmask();
}

void arch_suspend_enable_irqs(void)
{
	wakeup_irq_print();
	wake_int_mask();

	local_irq_enable();
	local_fiq_enable();
}

#ifndef CONFIG_MCPM
#ifdef CONFIG_PM
static int comip_suspend_enter(suspend_state_t state)
{
	unsigned int status;
	unsigned int flag = 0;
	int ret;

	CPUSUSP_PR("Entering suspend state LP%d\n", state);
	status = device_idle_state();
	CPUSUSP_PR("device idle state:0x%x\n", status);

	if (interrupt_is_pending()){
		goto out;
	}

#ifdef CONFIG_DDR_FREQ_ADJUST
	flag |= FLAG_MMUOFF_VAL;
#endif
	a7cx_warm_addr_set(CPU_ID_0, cpu_w_addr_resume);
	ap_sleep_enable();
	ap_pd_enable();
	ha7_scu_pd_enable();
	ret = core_power_down(CPU_ID_0, flag);
	ha7_scu_pd_disable();
	ap_pd_disable();
	ap_sleep_disable();
	a7cx_warm_addr_set(CPU_ID_0, cpu_w_addr_continue);
#ifdef CONFIG_DDR_FREQ_ADJUST
	if (!ret) {
		int down_ret = *(int*)(ddr_down_ret);
		int up_ret = *(int*)(ddr_up_ret);
		if (up_ret || down_ret)
			printk("ddr-freq-adjust:down ret:%d, up ret:%d\n",
				down_ret, up_ret);
	}
#endif
out:
	return 0;
}

static struct platform_suspend_ops comip_suspend_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= comip_suspend_enter,
};
#endif
#endif

void __init suspend_smp_prepare(void)
{
	u32 val;

	ap_pd_disable();

	a7cx_slp_state_set(CPU_ID_0, COMIP_SLP_STATE_ON);
	a7cx_slp_state_set(CPU_ID_1, COMIP_SLP_STATE_DOWN);
	a7cx_slp_state_set(CPU_ID_2, COMIP_SLP_STATE_DOWN);
	a7cx_slp_state_set(CPU_ID_3, COMIP_SLP_STATE_DOWN);

	wakeup_cmd_set(CPU_ID_0, COMIP_WAKEUP_CMD_NONE);

	val = virt_to_phys(comip_secondary_startup);

	cpu_w_addr_reset = val;

	a7cx_warm_addr_set(CPU_ID_0, val);
	a7cx_warm_addr_set(CPU_ID_1, val);
	a7cx_warm_addr_set(CPU_ID_2, val);
	a7cx_warm_addr_set(CPU_ID_3, val);

	cpu_w_addr_continue = virt_to_phys(comip1860_continue);

	val = virt_to_phys(comip1860_resume);
	cpu_w_addr_resume = val;

	comip_cpu_wfi = sram_cpu_wfi;
	sym_reloc_init();
}

 void suspend_regs_init(void)
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

	/*WARNNING:
	 *if AP_PWR_DMA_SLP_MK == 0, DMA-LP MUST be enabled
	 */
	reg |= 0 << AP_PWR_DMA_SLP_MK |
		1 << AP_PWR_DMA_SLP_MK_WE;
	reg |= 0 << AP_PWR_POWER_SLP_MK |
		1 << AP_PWR_POWER_SLP_MK_WE;
	reg |= 0 << AP_PWR_ADB_SLP_MK |
		1 << AP_PWR_ADB_SLP_MK_WE;
	reg |= 0 << AP_PWR_CS_SLP_MK |
		1 << AP_PWR_CS_SLP_MK_WE;
	__raw_writel(reg, io_p2v(AP_PWR_SLPCTL0));

	/* Set AP_PWR_SLPCTL1.WAKE_DLY_EN(bit2), fix bug by shiwanggen */
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
	reg = 0 << 6 | 1 << (6 + BIT_WE_OFF); /* PU_WITH_SCU = 1 */
	reg |= 0 << 3 | 1 << (3 + BIT_WE_OFF); /* PD_MK = 0 */
	reg |= 1 << 5 | 1 << (5 + BIT_WE_OFF); /*Disable power with INTR */
	__raw_writel(reg, io_p2v(AP_PWR_HA7_C1_PD_CTL));
	__raw_writel(reg, io_p2v(AP_PWR_HA7_C2_PD_CTL));
	__raw_writel(reg, io_p2v(AP_PWR_HA7_C3_PD_CTL));

	reg = 0 << 0 | 0xf << (0 + BIT_WE_OFF); /* L1RSTDISABLE = 0 */
	reg |= 0 << 4 | 1 << (1 + BIT_WE_OFF);  /* L2RSTDISABLE = 0 */
	__raw_writel(reg, io_p2v(AP_PWR_HA7_RSTCTL1));
	reg = 0 << 0 | 1 << (0 + BIT_WE_OFF); /* L1RSTDISABLE = 0 */
	reg |= 0 << 1 | 1 << (1 + BIT_WE_OFF);  /* L2RSTDISABLE = 0 */
	__raw_writel(reg, io_p2v(AP_PWR_SA7_RSTCTL1));

	reg = __raw_readl(io_p2v(AP_PWR_PLLCR));
	reg |= ((0x3f << 8) | 0xff);
	__raw_writel(reg, io_p2v(AP_PWR_PLLCR));

	/* Sometime cpu could not auto powerdown,
	 * Setting PD_CNT1 to a large value can reduce the probability of the problem.
	 * but could not solve this poroblem
	 */
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

void __init comip_suspend_init( void)
{
#ifndef CONFIG_MCPM
	suspend_regs_init();
#ifdef CONFIG_PM
	suspend_set_ops(&comip_suspend_ops);
#endif
#endif
}
