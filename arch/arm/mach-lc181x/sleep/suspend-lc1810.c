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

#include <asm/sections.h>
#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/gic.h>
#include <asm/localtimer.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>
#include <asm/idmap.h>

#include <mach/iomap.h>
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

#define DDR_TO_SRAM(f)	((sram_base) + ((f) - (ddr_base)))

#define symbol_fixed_ddr(sym, value) \
	do { \
		*(u32 *)sym = (u32)io_v2p(DDR_TO_SRAM(value)); \
	} while (0)

#define symbol_fixed_sram(sym, value) \
	do { \
		void *tmp; \
		tmp = (void *)DDR_TO_SRAM(sym); \
		*(u32 *)tmp = (u32)io_v2p(DDR_TO_SRAM(value)); \
	} while (0)

#define COMIP_SRAM_SZ	(0x1000)
#define COMIP_SRAM_PA	(AP_SEC_RAM_BASE + 0x20000 - COMIP_SRAM_SZ)

#define REG_VADDR(reg)	((base_addr) + reg)

#define A9_PRIVATE_TIMERS_OFFSET	0x600

/*scu registers*/
#define SCU_CTRL			0x00
#define SCU_INVALIDATE			0x0c
#define SCU_FILTER_START		0x40
#define SCU_FILTER_END			0x44

/*
 * Maximum size of each item of context, in bytes
 * We round these up to 32 bytes to preserve cache-line alignment
 */
#define PMU_DATA_SIZE			128
#define TIMER_DATA_SIZE			128
#define VFP_DATA_SIZE			288
#define GIC_INTERFACE_DATA_SIZE		64
#define GIC_DIST_PRIVATE_DATA_SIZE	96
#define BANKED_REGISTERS_SIZE		128
#define CP15_DATA_SIZE			64
#define DEBUG_DATA_SIZE			1024
#define MMU_DATA_SIZE			64
#define OTHER_DATA_SIZE			32

#define GIC_DIST_SHARED_DATA_SIZE	2592
#define SCU_DATA_SIZE			32
#define L2_DATA_SIZE			256
#define GLOBAL_TIMER_DATA_SIZE		128

/*offset of private registers*/
struct a9_timer_registers{
	/* 0x00 */ u32 timer_load;
	/* 0x04 */ u32 timer_counter;
	/* 0x08 */ u32 timer_control;
	/* 0x0c */ u32 timer_interrupt_status;
			s8 padding1[0x10];
	/* 0x20 */ u32 watchdog_load;
	/* 0x24 */ u32 watchdog_counter;
	/* 0x28 */ u32 watchdog_control;
	/* 0x2c */ u32 watchdog_interrupt_status;
	/* 0x30 */ u32 watchdog_reset_status;
	/* 0x34 */ u32 watchdog_disable;
} ;

struct a9_scu_context {
	u32 control;
	u32 filtering_start;
	u32 filtering_end;
	u32 access_control;
	u32 ns_access_control;
};

struct a9_pl310_context {
	u32 control;
	u32 aux_control;
	u32 tag_ram_control;
	u32 data_ram_control;
	u32 prefetch_ctrl;
	u32 power_ctrl;
};

struct gic_dis_share_context {
	u32 enable[DIV_ROUND_UP(1020, 32)];
	u32 conf[DIV_ROUND_UP(1020, 16)];
	u32 pri[DIV_ROUND_UP(1020, 4)];
	u32 target[DIV_ROUND_UP(1020, 4)];
	u32 control;
};

struct gic_dis_private_context {
	u32 enable[1];
	u32 conf[2];
	u32 pri[8];
};

struct gic_interface_context {
	u32 control;
	u32 priority_mask;
	u32 binary_point;
};

struct a9_private_timer_context{
	u32 timer_load;
	u32 timer_counter;
	u32 timer_control;
	u32 timer_interrupt_status;
	u32 watchdog_load;
	u32 watchdog_counter;
	u32 watchdog_control;
	u32 watchdog_interrupt_status;
} ;

struct ap_cpu_context{
	u32 endianness;
	u32 actlr;
	u32 sctlr;
	u32 cpacr;
	u32 flags;
	u32 saved_items;
	u32 *timer_data;
	u32 *gic_interface_data;
	u32 *gic_dist_private_data;
	u32 *pmu_data;
	u32 *vfp_data;
	u32 *banked_registers;
	u32 *cp15_data;
	u32 *debug_data;
	u32 *mmu_data;
	u32 *other_data;
	u32 size;
};

struct lockdown_regs{
	u32 d, i;
};

struct pl310_context{
	u32 aux_control;
	u32 tag_ram_control;
	u32 data_ram_control;
	u32 ev_counter_ctrl;
	u32 ev_counter1_cfg;
	u32 ev_counter0_cfg;
	u32 ev_counter1;
	u32 ev_counter0;
	u32 int_mask;
	u32 lock_line_en;
	struct lockdown_regs lockdown[8];
	u32 unlock_way;
	u32 addr_filtering_start;
	u32 addr_filtering_end;
	u32 debug_ctrl;
	u32 prefetch_ctrl;
	u32 power_ctrl;
};

struct ap_cpu{
	void *ic_address; /*0 => no Interrupt Controller*/
	struct ap_cpu_context *context;
	volatile u32 power_state;
};

struct ap_cluster_context{
	spinlock_t lock;
	u32 saved_items;
	u32 *gic_dist_shared_data;
	u32 *l2_data;
	u32 *scu_data;
	u32 *global_timer_data;
	u32 size;
};

struct ap_cluster{       
	s32 num_cpus;
	volatile s32 active_cpus;
	void *scu_address;
	void *ic_address;
	void *l2_address;
	struct ap_cluster_context *context;
	struct ap_cpu *cpu_table;
	volatile s32 power_state;
};

static struct ap_cpu a9mp_cpu[] = 
{
	{(void*)io_p2v(GIC_CPU_BASE), (void *)0, 0},
      	{(void*)io_p2v(GIC_CPU_BASE), (void *)0, 0}
};

static struct ap_cluster a9mp_cluster=
{
	0, 0, 
	(void*)io_p2v(CORE_SCU_BASE), 
	(void*)io_p2v(GIC_DIST_BASE),
	(void*)io_p2v(L2_CACHE_CTRL_BASE), 
	(void *)0, (void *)0, 0
};

static DEFINE_SPINLOCK(comip_suspend_lock);
unsigned long comip_pgd_phys = 0x00; 
void *comip_context_area = NULL;

static u32 comip_sram_start;  //pa
static u32 comip_sram_base;  //va
static u32 comip_sram_size;
static u32 comip_sram_ceil;  //va

extern u32 comip_lp_startup_sz;
extern u32 *ddr_return_to_virtual_addr;
extern u32 *ddr_pgd_phys_addr;
extern u32 *ddr_ddr_slp_exit_addr;
extern u32 *comip_ddr2ram_addr;
extern u32 *comip_ddr2ram_ret_addr;
extern u32 comip_ddr2ram_ret;
extern u32 ddr_ddr_slp_exit_ret;
extern u32 comip_ddr2ram;
extern u32 *comip_mmu_off_addr;
extern u32 comip_mmu_off;
extern u32 *comip_mmu_off_return_addr;
extern u32 comip_mmu_off_return;

extern void comip_cpu_save(void *context, void *sp);
extern void comip_cpu_restore(void *context);
extern void comip_flush_dcache_all(void);
extern void comip_put_cpu_reset(int cpu);
extern void ddr_ddr_slp_enter(int flag);
extern void ddr_ddr_slp_exit(int flag);
extern void __return_to_virtual(unsigned long pgdir, void (*ctx_restore)(void));
extern void comip_mmu_close(unsigned int addr_phy);

void comip_reset_to_phy(unsigned int addr_phy)
{
	flush_cache_all();
	outer_flush_all();
	setup_mm_for_reboot();
	comip_mmu_close(addr_phy);
}

/*comip_dcache_clean_area- clean L1&L2 data cache */
static void comip_dcache_clean_area(void *start, unsigned size)
{
	unsigned long tmp;

	clean_dcache_area(start, size);
	 tmp = virt_to_phys(start);
	 outer_clean_range(tmp, tmp + size);
}

static inline void comip_outer_clean_area(void* start, unsigned size)
{
	unsigned long tmp;

	tmp = virt_to_phys(start);
	outer_clean_range(tmp, tmp + size);
}

/*
 *comip_gic_dist_share_save - save gic distratributer registeres
 */
static void  comip_gic_dist_share_save(u32 *pdata, void *base_addr)
{
	unsigned int gic_irqs, i,j;
	struct gic_dis_share_context *context;

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = __raw_readl(REG_VADDR(GIC_DIST_CTR)) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;

	context = (struct gic_dis_share_context*)pdata;

	for (i = 32, j = 2; i < gic_irqs; i += 16, ++j)
		context->conf[j] = 
			__raw_readl( REG_VADDR(GIC_DIST_CONFIG) + i * 4 / 16);

	for (i = 32, j = 8; i < gic_irqs; i += 4, ++j)
		context->target[j] =
			__raw_readl(REG_VADDR(GIC_DIST_TARGET) + i * 4 / 4);

	for (i = 32, j = 8; i < gic_irqs; i += 4, ++j)
		context->pri[j] = 
			__raw_readl(REG_VADDR(GIC_DIST_PRI) + i * 4 / 4);

	for (i = 32, j = 1; i < gic_irqs; i += 32, ++j)
		context->enable[j] = 
			__raw_readl(REG_VADDR(GIC_DIST_ENABLE_SET) +  i* 4 / 32);

	context->control = 
			__raw_readl(REG_VADDR(GIC_DIST_CTRL));
}

static noinline void  comip_gic_dist_share_restore(u32 *pdata, void *base_addr)
{
	unsigned int gic_irqs,  i,j;
	struct gic_dis_share_context *context;

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = __raw_readl(REG_VADDR(GIC_DIST_CTR)) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;

	context = (struct gic_dis_share_context*)pdata;

	__raw_writel(0x00, REG_VADDR(GIC_DIST_CTRL));
	dsb();

	for (i = 32, j = 2; i < gic_irqs; i += 16, ++j)
	      __raw_writel(context->conf[j], 
	      			REG_VADDR(GIC_DIST_CONFIG) + i * 4 / 16);

	for (i = 32, j = 8; i < gic_irqs; i += 4, ++j)
	      __raw_writel(context->target[j], 
	      			REG_VADDR(GIC_DIST_TARGET) + i * 4 / 4);

	for (i = 32, j = 8; i < gic_irqs; i += 4, ++j)
	      __raw_writel(context->pri[j], 
	      			REG_VADDR(GIC_DIST_PRI) + i * 4 / 4);

	for (i = 32, j = 1; i < gic_irqs; i += 32, ++j)
	      __raw_writel(context->enable[j], 
	      			REG_VADDR(GIC_DIST_ENABLE_SET) + i * 4 / 32);

	dsb();
	__raw_writel(context->control, 
				REG_VADDR(GIC_DIST_CTRL));
	dsb();

}

/*
 *comip_gic_interface_save - save gic cpu interface registeres
 */
static void  comip_gic_interface_save(u32 *pdata, void *base_addr)
{
	struct gic_interface_context *context = 
					(struct gic_interface_context *)pdata;

	context->priority_mask =  
			__raw_readl(REG_VADDR(GIC_CPU_PRIMASK));
	context->binary_point = 
			__raw_readl(REG_VADDR(GIC_CPU_BINPOINT));
	context->control = 
			__raw_readl(REG_VADDR(GIC_CPU_CTRL));
}

static void  comip_gic_interface_restore(u32 *pdata, void *base_addr)
{
	struct gic_interface_context *context = 
					(struct gic_interface_context *)pdata;

	__raw_writel(context->priority_mask, REG_VADDR(GIC_CPU_PRIMASK));
	__raw_writel(context->binary_point, REG_VADDR(GIC_CPU_BINPOINT));
	dsb();
	__raw_writel(context->control, REG_VADDR(GIC_CPU_CTRL));
}

static void  comip_gic_dis_private_save(u32 *pdata, void *base_addr)
{
	int i,j;
	struct gic_dis_private_context *context = 
					(struct gic_dis_private_context *)pdata;

	context->enable[0]  = 
			__raw_readl(REG_VADDR(GIC_DIST_ENABLE_SET));
	for (i = 0, j = 0; i < 32; i += 4, ++j) {
		context->pri[j] = 
			__raw_readl(REG_VADDR(GIC_DIST_PRI) + i * 4 / 4);
	}
	context->conf[1]  = 
			__raw_readl(REG_VADDR(GIC_DIST_CONFIG) + 4);
}

static void  comip_gic_dis_private_restore(u32 *pdata, void *base_addr)
{
	int i,j;
	u32 tmp;
	struct gic_dis_private_context *context = 
					(struct gic_dis_private_context *)pdata;

	tmp = __raw_readl(REG_VADDR(GIC_DIST_CTRL));
	__raw_writel(0x00, REG_VADDR(GIC_DIST_CTRL));
	dsb();

	__raw_writel(context->enable[0], REG_VADDR(GIC_DIST_ENABLE_SET));
	for (i = 0, j = 0; i < 32; i += 4, ++j) {
		__raw_writel(context->pri[j], REG_VADDR(GIC_DIST_PRI) + i * 4 / 4);
	}
	__raw_writel(context->conf[1], REG_VADDR(GIC_DIST_CONFIG) + 4);

	dsb();
	__raw_writel(tmp, REG_VADDR(GIC_DIST_CTRL));
	dsb();

}

#ifdef CONFIG_HAVE_ARM_TWD
static void  comip_private_timer_save(u32 *pdata, void *base_addr)
{
	struct a9_private_timer_context *context = 
					(struct a9_private_timer_context *)pdata;
	
	struct a9_timer_registers *reg = 
					(struct a9_timer_registers*)base_addr;

	context->timer_control = __raw_readl(&reg->timer_control);
	__raw_writel(0, &reg->timer_control);
	
	context->watchdog_control = __raw_readl(&reg->watchdog_control);
	__raw_writel(0, &reg->watchdog_control);

	context->timer_load = __raw_readl(&reg->timer_load);
	context->timer_counter = __raw_readl(&reg->timer_counter);
	context->timer_interrupt_status = __raw_readl(&reg->timer_interrupt_status);
	context->watchdog_load = __raw_readl(&reg->watchdog_load);
	context->watchdog_counter = __raw_readl(&reg->watchdog_counter);
	context->watchdog_interrupt_status = __raw_readl(&reg->watchdog_interrupt_status);
}

static void  comip_private_timer_restore(u32 *pdata, void *base_addr)
{
	struct a9_private_timer_context *context = 
					(struct a9_private_timer_context *)pdata;
	
	struct a9_timer_registers *reg = 
					(struct a9_timer_registers*)base_addr;

	__raw_writel(0, &reg->timer_control);
	__raw_writel(0, &reg->watchdog_control);

    /*
     * We restore the load register first, because it also sets the counter register.
     */
	__raw_writel(context->timer_load, &reg->timer_load);
	__raw_writel(context->watchdog_load, &reg->watchdog_load);

    /*
     * If a timer has reached zero (presumably during the context save) and triggered
     * an interrupt, then we set it to the shortest possible expiry time, to make it
     * trigger again real soon.
     * We could fake this up properly, but we would have to wait around until the timer 
     * ticked, which could be some time if PERIPHCLK is slow. This approach should be 
     * good enough in most cases.
     */
	if (context->timer_interrupt_status) {
		__raw_writel(1, &reg->timer_counter);
	}
	else{
		__raw_writel(context->timer_counter, &reg->timer_counter);
	}

	if (context->watchdog_interrupt_status) {
		__raw_writel(1, &reg->watchdog_counter);
	}
	else{
		__raw_writel(context->watchdog_counter, &reg->watchdog_counter);
	}

	__raw_writel(context->timer_control, &reg->timer_control);
	__raw_writel(context->watchdog_control, &reg->watchdog_control);

}

#endif

static void comip_a9_scu_save(u32 *pdata, void *base_addr)
{
	struct a9_scu_context *context = (struct a9_scu_context *)pdata;

	context->control = __raw_readl(REG_VADDR(SCU_CTRL));
	context->filtering_start = __raw_readl(REG_VADDR(SCU_FILTER_START));
	context->filtering_end = __raw_readl(REG_VADDR(SCU_FILTER_END));
}

static void comip_a9_scu_restore(u32 *pdata, void *base_addr)
{
	struct a9_scu_context *context = (struct a9_scu_context *)pdata;
	u32 scu_ctrl;

	__raw_writel(context->filtering_start, REG_VADDR(SCU_FILTER_START));
	__raw_writel(context->filtering_end, REG_VADDR(SCU_FILTER_END));

	scu_ctrl = __raw_readl(REG_VADDR(SCU_CTRL));
	if (scu_ctrl & 1) 
		__raw_writel(0xf, REG_VADDR(SCU_INVALIDATE));
	else
		__raw_writel(context->control, REG_VADDR(SCU_CTRL));
}

#ifdef CONFIG_CACHE_L2X0
static inline void comip_pl310_sync(void)
{
	unsigned int base_addr = (unsigned int)a9mp_cluster.l2_address;

	dsb();
#ifdef CONFIG_ARM_ERRATA_753970
	__raw_writel(0, base_addr + L2X0_DUMMY_REG);
#else
	__raw_writel(0, base_addr + L2X0_CACHE_SYNC);
#endif
}

static void comip_pl310_disable(void)
{
	unsigned int base_addr = (unsigned int)a9mp_cluster.l2_address;
	dsb();
	__raw_writel(0, base_addr + L2X0_CTRL);
	dsb();
}

static void comip_pl310_inv_all(void)
{
	unsigned int base_addr = (unsigned int)a9mp_cluster.l2_address;
	unsigned int mask = 0xff;  //8 ways

	__raw_writel(mask, base_addr + L2X0_INV_WAY);
	while (__raw_readl(base_addr + L2X0_INV_WAY) & mask)
		;
	comip_pl310_sync();
	dsb();
}

static void comip_a9_pl310_save(u32 *pdata, void *base_addr)
{
	struct a9_pl310_context *context = (struct a9_pl310_context *)pdata;

	context->control = __raw_readl(REG_VADDR(L2X0_CTRL));
	context->aux_control = __raw_readl(REG_VADDR(L2X0_AUX_CTRL));
	context->tag_ram_control = __raw_readl(REG_VADDR(L2X0_TAG_LATENCY_CTRL));
	context->data_ram_control = __raw_readl(REG_VADDR(L2X0_DATA_LATENCY_CTRL));
	context->prefetch_ctrl = __raw_readl(REG_VADDR(L2X0_PREFETCH_CTRL));
	context->power_ctrl = __raw_readl(REG_VADDR(L2X0_POWER_CTRL));
}

static void comip_a9_pl310_restore(u32 *pdata, void *base_addr)
{
	struct a9_pl310_context *context = (struct a9_pl310_context *)pdata;

	if (__raw_readl(REG_VADDR(L2X0_CTRL))) {
		comip_pl310_sync();
		dsb();
		__raw_writel(0, REG_VADDR(L2X0_CTRL));
	}
	
	__raw_writel(context->aux_control, REG_VADDR(L2X0_AUX_CTRL));
	__raw_writel(context->tag_ram_control, REG_VADDR(L2X0_TAG_LATENCY_CTRL));
	__raw_writel(context->data_ram_control, REG_VADDR(L2X0_DATA_LATENCY_CTRL));
	__raw_writel(context->prefetch_ctrl, REG_VADDR(L2X0_PREFETCH_CTRL));
	__raw_writel(context->power_ctrl, REG_VADDR(L2X0_POWER_CTRL));

	comip_pl310_inv_all();
	barrier();
	__raw_writel(context->control, REG_VADDR(L2X0_CTRL));
	dsb();
}
#endif

static int comip_cpu_context_init(void **cpu_cxt)
{
	int size;
	struct ap_cpu_context *context;
	u8 *pdata;

	size = L1_CACHE_ALIGN(sizeof(*context)) + 
		TIMER_DATA_SIZE +
		GIC_INTERFACE_DATA_SIZE +
		GIC_DIST_PRIVATE_DATA_SIZE;
		/*
		PMU_DATA_SIZE + 
		VFP_DATA_SIZE + 
		BANKED_REGISTERS_SIZE +
		CP15_DATA_SIZE +
		DEBUG_DATA_SIZE +
		MMU_DATA_SIZE +
		OTHER_DATA_SIZE;
		*/
	size = L1_CACHE_ALIGN(size);
	pdata = kzalloc(size,GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	context = (struct ap_cpu_context *)pdata;
	
	pdata += L1_CACHE_ALIGN(sizeof(*context));
	context->timer_data = (u32*)pdata;
	pdata += TIMER_DATA_SIZE;

	context->gic_interface_data = (u32*)pdata;
	pdata += GIC_INTERFACE_DATA_SIZE;

	context->gic_dist_private_data = (u32*)pdata;
	pdata += GIC_DIST_PRIVATE_DATA_SIZE;

	/*
	 *context->pmu_data = (u32*)pdata;
	 *pdata += PMU_DATA_SIZE;
	 *context->vfp_data = (u32*)pdata;
	 *pdata += VFP_DATA_SIZE;
	 *context->banked_registers = (u32*)pdata;
	 *pdata += BANKED_REGISTERS_SIZE;
	 *context->cp15_data = (u32*)pdata;
	 *pdata += CP15_DATA_SIZE;
	 *context->debug_data = (u32*)pdata;
	 *pdata += DEBUG_DATA_SIZE;
	 *context->mmu_data = (u32*)pdata;
	 *pdata += MMU_DATA_SIZE;
	 *context->other_data = (u32*)pdata;
	 */
	context->size = size;
	
	if (cpu_cxt)
		*cpu_cxt = context;

	return 0;
}

static int comip_cluster_context_init(void **cluster_cxt)
{
	int size;
	struct ap_cluster_context *context;
	u8 *pdata;

	size = L1_CACHE_ALIGN(sizeof(*context)) + 
		GIC_DIST_SHARED_DATA_SIZE +
		L2_DATA_SIZE + 
		SCU_DATA_SIZE;
		/*GLOBAL_TIMER_DATA_SIZE +*/
		
	size = L1_CACHE_ALIGN(size);
	
	pdata = kzalloc(size,GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	context = (struct ap_cluster_context *)pdata;
	
	pdata += L1_CACHE_ALIGN(sizeof(*context));
	context->gic_dist_shared_data = (u32*)pdata;
	pdata += GIC_DIST_SHARED_DATA_SIZE;
	context->l2_data = (u32*)pdata;
	pdata += L2_DATA_SIZE;
	context->scu_data = (u32*)pdata;
	pdata += SCU_DATA_SIZE;
	/*
	 *context->global_timer_data = (u32*)pdata;
	 *pdata += GLOBAL_TIMER_DATA_SIZE;
	 */
	context->size = size;

	if (cluster_cxt)
		*cluster_cxt = context;

	return 0;
}

static int comip_context_init(void)
{
	int i;
	int ret;
	void *context;

	i = scu_get_core_count(a9mp_cluster.scu_address);
	i= min(i, NR_CPUS);

	a9mp_cluster.num_cpus = i;
	
	for (i = 0; i < a9mp_cluster.num_cpus; ++i){
		ret = comip_cpu_context_init(&context);
		if(ret)
			goto out;	
		 a9mp_cpu[i].context = context;

		 comip_dcache_clean_area((void*)&a9mp_cpu[i],sizeof(a9mp_cpu[i]));
	}
	ret = comip_cluster_context_init(&context);
	if (ret)
		goto out;

	a9mp_cluster.context = context;
	a9mp_cluster.cpu_table = &a9mp_cpu[0];
	comip_dcache_clean_area((void*)&a9mp_cluster,sizeof(a9mp_cluster));

	return 0;	

out:
	a9mp_cluster.num_cpus = 0;
	if (i > 0){
		while (--i >= 0){
			kfree( a9mp_cpu[i].context);
			a9mp_cpu[i].context = NULL;
		}
	}
	return ret;
}

/*
 *comip_suspend_mem_init - create page table for cpu1 reboot and malloc a block ram to save
 *cpus core registers
 */
static int comip_suspend_mem_init(void)
{
	comip_pgd_phys = virt_to_phys(idmap_pgd);
	comip_dcache_clean_area((void*)&comip_pgd_phys,sizeof(comip_pgd_phys));

	/*ram to sace cpus core registers*/
	comip_context_area = kzalloc(CONTEXT_SIZE_BYTES * NR_CPUS, GFP_KERNEL);
	if (!comip_context_area)
		return -ENOMEM;

	comip_dcache_clean_area((void*)&comip_context_area,sizeof(comip_context_area));

	comip_context_init();
	
	return 0;
}

/*
 *comip_suspend_restore - cpu1 resore the saved registers.
 */
static noinline void comip_cpu_context_restore(int cpu)
{
	struct ap_cpu *cpux = &a9mp_cluster.cpu_table[cpu];
	struct ap_cpu_context *context = cpux->context;
	struct ap_cluster_context *clustex_cxt = a9mp_cluster.context;
	
#ifdef CONFIG_HAVE_ARM_TWD
	comip_private_timer_restore(context->timer_data,
						a9mp_cluster.scu_address + A9_PRIVATE_TIMERS_OFFSET);
#endif

	if (CPU_ID_0 == cpu)
		comip_gic_dist_share_restore(clustex_cxt->gic_dist_shared_data,
						a9mp_cluster.ic_address);

	comip_gic_dis_private_restore(context->gic_dist_private_data,
						a9mp_cluster.ic_address);

	comip_gic_interface_restore(context->gic_interface_data, 
						cpux->ic_address);

}

/*
 *comip_suspend_save - cpu1 save cpu1-system registers.
 */
static void comip_cpu_context_save(int cpu)
{
	struct ap_cpu *cpux = &a9mp_cluster.cpu_table[cpu];
	struct ap_cpu_context *context = cpux->context;
	struct ap_cluster_context *clustex_cxt = a9mp_cluster.context;
	
#ifdef CONFIG_HAVE_ARM_TWD
	comip_private_timer_save(context->timer_data,
				a9mp_cluster.scu_address + A9_PRIVATE_TIMERS_OFFSET);
#endif

	comip_gic_interface_save(context->gic_interface_data, 
						cpux->ic_address);

	comip_gic_dis_private_save(context->gic_dist_private_data,
						a9mp_cluster.ic_address);

	if (CPU_ID_0 == cpu)
		comip_gic_dist_share_save(clustex_cxt->gic_dist_shared_data,
						a9mp_cluster.ic_address);
}


static  void comip_cluster_save(void)
{
	struct ap_cluster_context *context = a9mp_cluster.context;

	comip_a9_scu_save(context->scu_data, a9mp_cluster.scu_address);
#ifdef CONFIG_CACHE_L2X0
	comip_a9_pl310_save(context->l2_data, a9mp_cluster.l2_address);
#endif
}

static  void comip_cluster_restore(void)
{
	struct ap_cluster_context *context = a9mp_cluster.context;
	
	comip_a9_scu_restore(context->scu_data, a9mp_cluster.scu_address);
#ifdef CONFIG_CACHE_L2X0
	comip_a9_pl310_restore(context->l2_data, a9mp_cluster.l2_address);
#endif
}

/*WARNINIG:
 * comip_context_save is called by comip_core_save in assemble file,
 * so the stack is task idle/suspend thread stack. there is a risk: if a page fault occer when
 * store data into stack? should we use a standalone stack?
 */
void comip_context_save(int cpu, void* sp)
{
	void *context;

	if (CPU_ID_0 == cpu)
		context = comip_context_area;
	else
		context = comip_context_area + CONTEXT_SIZE_BYTES;
	
	comip_cpu_save(context, sp);
	if (CPU_ID_0 == cpu) {
		comip_cluster_save();
	}

	flush_cache_all();
	outer_flush_all();
	barrier();
#ifdef CONFIG_CACHE_L2X0
	if (CPU_ID_0 == cpu)
		comip_pl310_disable();
#endif
	setup_mm_for_reboot();
}

void comip_context_restore(int cpu)
{
	void *context;
	
	if (CPU_ID_0 == cpu) {
		comip_cluster_restore();
	}

	if (CPU_ID_0 == cpu)
		context = comip_context_area;
	else
		context = comip_context_area + CONTEXT_SIZE_BYTES;
	
	comip_cpu_restore(context);
	flush_cache_all();
}

/*
 *comip_lp_cpu -cpux saves its state and enters wfi .
 *
 *return:
 *	0:  no sleep  beacuse a interrupt is arriving during entering-sleeping
 *	1:  wakeup by anthoer interrupt when in dormant
 */
static int comip_lp_cpu(int cpu)
{
	int ret_val;

	if (!comip_context_area)
		return 0;

	comip_cpu_context_save(cpu);

	barrier();
	/*save cpux core registeres and enter wfi*/
	ret_val = comip_core_save(cpu);
	dsb();

	/*return back from power-down or wfi*/

	if (ret_val) {
		/*cpux coms back from power-down state*/
		comip_cpu_context_restore(cpu);
	}

	return ret_val;
}

int comip_suspend_lp_cpu1(void)
{
	return comip_lp_cpu(CPU_ID_1);
}

int comip_suspend_lp_cpu0(void)
{
	return comip_lp_cpu(CPU_ID_0);
}

static void cpuidle_wakeup_cpu1(int sig)
{
	long timeout;
	unsigned long cycle = 0;
	int count = 2;

tryagain:
	/*indecate cpu1 to jump out of loop*/
	__raw_writel(COMIP_WAKEUP_WAKING, io_p2v(CPU0_WAKEUP_FLAG));

	if (sig) {
		/*signal SGI 15 to cpu1*/
		gic_raise_softirq(cpumask_of(CPU_ID_1), COMIP_WAKEUP_CPU1_SGI_ID);
		dsb();
	}
	smp_mb();
	/*cpu0 wait until cpu1 has set CPU1_WAKEUP_FLAG to COMIP_WAKEUP_ON*/
	timeout = 10000;
	while (--timeout > 0) {
		if (COMIP_WAKEUP_ON == __raw_readl(io_p2v(CPU1_WAKEUP_FLAG)))
			break;
		++cycle;
	}

	if (timeout <= 0 && --count > 0)
		goto tryagain;

	smp_mb();
	__raw_writel(COMIP_WAKEUP_ON, io_p2v(CPU0_WAKEUP_FLAG));

}

void comip_warm_up_cpu1(void)
{
	cpuidle_wakeup_cpu1(0);
}

void comip_wake_up_cpu1(void)
{
	cpuidle_wakeup_cpu1(1);
}

static inline void suspend_wake_int_do(int flag)
{
	struct irq_desc *desc;
	struct irq_data *data;
	int irq;

	for_each_irq_desc(irq, desc) {
		if (desc) {
			data = &desc->irq_data;
			if (data && irqd_is_wakeup_set(data)) {
				if (data->chip) {
					if (flag && data->chip->irq_unmask)
						data->chip->irq_unmask(data);
					 else if (!flag && data->chip->irq_mask)
						data->chip->irq_mask(data);
				}
			}
		}
	}
}

static inline void suspend_wake_int_unmask(void)
{
	suspend_wake_int_do(1);
}

static inline void suspend_wake_int_mask(void)
{
	suspend_wake_int_do(0);
}

static inline int suspend_wakeup_irq(void)
{
	return __raw_readl(io_p2v(GIC_CPU_BASE + GIC_CPU_HIGHPRI)) & 0x3ff;
}

static inline int suspend_int_is_pending(void)
{
	return COMIP_SPURIOUS_INT_ID != suspend_wakeup_irq();
}

static inline void comip_ap_slpctl_set(int value)
{
	u32 reg;

	reg = __raw_readl(io_p2v(AP_PWR_SLPCTL0));
	reg &= 1 << AP_PWR_A9C0_SLP_EN;

	if ((!!reg) != value) {
		reg = value << AP_PWR_A9C0_SLP_EN;
		reg |= 1 << AP_PWR_A9C0_SLP_EN_WE;
		__raw_writel(reg, io_p2v(AP_PWR_SLPCTL0));
		dsb();
	}
}

static void  comip_ap_sleep_enable(void)
{
	comip_ap_slpctl_set(1);
}

static void  comip_ap_sleep_disable(void)
{
	comip_ap_slpctl_set(0);
}

static inline void comip_a9_pd_set(int value)
{
	u32 reg;

	spin_lock(&comip_suspend_lock);

	reg = __raw_readl(io_p2v(AP_PWR_A9_PD_CTL));
	reg &= 1 << AP_PWR_A9_PD_MK;

	if ((!!reg) != value) {
		reg = value << AP_PWR_A9_PD_MK;
		reg |= 1 << AP_PWR_A9_PD_MK_WE;
		__raw_writel(reg, io_p2v(AP_PWR_A9_PD_CTL));
		dsb();
	}

	spin_unlock(&comip_suspend_lock);
}

void  comip_a9_pd_enable(void)
{
	comip_a9_pd_set(0);
}

void  comip_a9_pd_disable(void)
{
	comip_a9_pd_set(1);
}

static void comip_resume_irq_print(int irq)
{
	unsigned int status;
	int i;

	if (irq == INT_GPIO) {
		for (i = 0; i < 4; i++) {
			status = __raw_readl(io_p2v(GPIO0_BASE
						+ GPIO_INTR_RAW(0) + (i * 4)));
			if (status) {
				CPUSUSP_PR("exit comip suspend by gpio%d\n",
						fls(status) + i * 32 - 1);
				break;
			}
		}
	} else if (irq == INT_GPIO1) {
		for (i = 0; i < 4; i++) {
			status = __raw_readl(io_p2v(GPIO1_BASE
						+ GPIO_INTR_RAW(0) + (i * 4)));
			if (status) {
				CPUSUSP_PR("exit comip suspend by gpio%d\n",
						fls(status) + i * 32 + 128 - 1);
				break;
			}
		}
	} else if (irq == INT_CTL) {
		CPUSUSP_PR("exit comip suspend by modem 0x%08x\n",
				__raw_readl(io_p2v(CTL_CP2AP_INTR_STA_RAW)));
	} else {
		CPUSUSP_PR("exit comip suspend by irq %d\n", irq);
	}
}

static int comip_suspend_enter(suspend_state_t state)
{
	unsigned long flags;
	int irq;

	CPUSUSP_PR("Entering suspend state LP%d\n", state);

	local_irq_save(flags);
	local_fiq_disable();

	suspend_wake_int_unmask();

	if (suspend_int_is_pending())
		goto out;

	comip_ap_sleep_enable();
	comip_a9_pd_enable();

	barrier();
	comip_suspend_lp_cpu0();
	barrier();

	comip_a9_pd_disable();
	comip_ap_sleep_disable();

out:
	irq = suspend_wakeup_irq();
	suspend_wake_int_mask();
	local_fiq_enable();
	local_irq_restore(flags);

	comip_resume_irq_print(irq);

	return 0;
}

static struct platform_suspend_ops comip_suspend_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= comip_suspend_enter,
};

static void* comip_sram_push(void *start, u32 size)
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

static void inline symbol_reloc(void *sym, void *value)
{
	*(u32 *)sym = (u32)(value);
}

static int __init comip_reloc_init(int eco_flag, void **start_addr)
{
	void *sram_base;
	void *ddr_base = (void*)comip_lp_startup;

	symbol_reloc(&ddr_return_to_virtual_addr, 
			(void *)virt_to_phys(__return_to_virtual));
	symbol_reloc(&ddr_pgd_phys_addr, 
			(void *)virt_to_phys(&comip_pgd_phys));
	symbol_reloc(&comip_mmu_off_addr, 
			(void *)virt_to_phys(&comip_mmu_off));
	symbol_reloc(&comip_mmu_off_return_addr, 
			(void *)virt_to_phys(&comip_mmu_off_return));

	comip_map_sram();
	sram_base = comip_sram_push(ddr_base, comip_lp_startup_sz);
	if (!sram_base)
		return -ENOMEM;

	if (eco_flag) {
		symbol_fixed_sram((void *)&ddr_ddr_slp_exit_addr,
				(void *)ddr_ddr_slp_exit);
		symbol_fixed_ddr((void *)&comip_ddr2ram_addr,
				(void *)&comip_ddr2ram);
	} else {
		symbol_fixed_sram((void *)&ddr_ddr_slp_exit_addr, 
				(void *)&ddr_ddr_slp_exit_ret);
		symbol_fixed_ddr((void *)&comip_ddr2ram_addr, 
				(void *)&comip_ddr2ram_ret);
	}

	if (start_addr)
		*start_addr = sram_base;

	flush_icache_range((u32)sram_base,
		(u32)(sram_base + comip_lp_startup_sz));
	__cpuc_flush_dcache_area(sram_base, comip_lp_startup_sz);

	flush_icache_range((u32)ddr_base,
		(u32)(ddr_base + comip_lp_startup_sz));
	__cpuc_flush_dcache_area(ddr_base, comip_lp_startup_sz);

	return 0;
}

static inline void suspend_regs_init(void)
{
	u32 reg;

	/*ap sleep is defaultly forbidden */
	/*
	 *bit4:   A9C1_SLP_MK:default=1,switch to 0
	 */
	reg = 0 << AP_PWR_A9C0_SLP_EN | 
		1 << AP_PWR_A9C0_SLP_EN_WE;
	reg |= 1 << AP_PWR_A9C1_SLP_EN | 
		1 << AP_PWR_A9C1_SLP_EN_WE;
	reg |= 1 << AP_PWR_M0_SLP_EN | 
		1 << AP_PWR_M0_SLP_EN_WE;
	reg |= 0 << AP_PWR_A9C1_SLP_MK | 
		1 << AP_PWR_A9C1_SLP_MK_WE;
	reg |= 0 << AP_PWR_DMA_SLP_MK |	
		1 << AP_PWR_DMA_SLP_MK_WE;
	reg |= 0 << AP_PWR_PWR_SLP_MK |	
		1 << AP_PWR_PWR_SLP_MK_WE;
	reg |= 0 << AP_PWR_TOP_DDRAXI_SLP_MK |	
		1 << AP_PWR_DDR_AXI_SLP_MK_WE;
	reg |= 0 << AP_PWR_CS_SLP_MK |
		1 << AP_PWR_CS_SLP_MK_WE;
	reg |= 0 << AP_PWR_L2C_SLP_MK |	
		1 << AP_PWR_L2C_SLP_MK_WE;
	__raw_writel(reg, io_p2v(AP_PWR_SLPCTL0));

	/*A9 powered-off is defaultly forbidden */
	reg = 1 << AP_PWR_A9_WK_ACK_MK | 
		1 << AP_PWR_A9_WK_ACK_MK_WE;
	reg |= 1 << AP_PWR_A9_PD_MK | 
		1 << AP_PWR_A9_PD_MK_WE;
	reg |= 0 << AP_PWR_L2C_A9PD_MK | 
		1 << AP_PWR_L2C_A9PD_MK_WE;
	__raw_writel(reg, io_p2v(AP_PWR_A9_PD_CTL));

	/*disable INT coresight-powerup */
	/*
	reg = __raw_readl(io_p2v(AP_PWR_INTEN_A9));
	reg &= ~(1 << AP_PWR_CSPWRREQ_INTR);
	__raw_writel(reg, io_p2v(AP_PWR_INTEN_A9));
	*/

	reg = __raw_readl(io_p2v(AP_PWR_INTST_A9));
	reg &= 1 << AP_PWR_CSPWRREQ_INTR;
	if (reg)
		__raw_writel(reg, io_p2v(AP_PWR_INTST_A9));

}

void __init comip_suspend_init( void)
{
	u32 reset_addr = virt_to_phys(comip_lp_startup);
	void *start_addr = NULL;
	int eco_flag = 1;
#if 0
	u32 *eco_addr;
	eco_addr = (u32 *)ioremap(0xfffffffc, 4);
	eco_flag = !__raw_readl(eco_addr);
	iounmap(eco_addr);
	CPUSUSP_PR("eco_flag = %d\n", eco_flag);
#endif

	comip_reloc_init(eco_flag, &start_addr);

	if (start_addr)
		reset_addr = (u32)io_v2p(start_addr);

	if (eco_flag) {
		/*zspx, cp arm, a9 sleep state are all normal_state*/
		__raw_writel(0x00, io_p2v(DDR_PWR_GENERAL_REG_1));
	}

	/*cpu0&cpu1 warm reboot address are the same*/
	__raw_writel(reset_addr, io_p2v(CPU1_WBOOT_ADDR0));
	__raw_writel(reset_addr, io_p2v(CPU0_WBOOT_ADDR0));

	suspend_regs_init();
	
	comip_suspend_mem_init();

#ifdef CONFIG_PM
       suspend_set_ops(&comip_suspend_ops);
#endif
}

static int __init comip_suspend_init_later(void)
{
	if (PU_REASON_USB_CHARGER == get_boot_reason()) 
		 writel(0x3f, io_p2v(DDR_PWR_GENERAL_REG_1));

	return 0;
}

late_initcall(comip_suspend_init_later);

#ifdef CONFIG_DEBUG_FS

static int suspend_time_debug_show(struct seq_file *s, void *data)
{
	seq_printf(s, "time (secs)  count\n");
	seq_printf(s, "------------------\n");

	return 0;
}

static int suspend_time_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, suspend_time_debug_show, NULL);
}

static const struct file_operations comip_suspend_time_debug_fops = {
	.open	= suspend_time_debug_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release	= single_release,
};

static int __init comip_suspend_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("suspend_time", 0755, NULL, NULL,
				&comip_suspend_time_debug_fops);
	if (!d) {
		pr_info("Failed to create suspend_time debug file\n");
		return -ENOMEM;
	}
	return 0;
}

late_initcall(comip_suspend_debug_init);
#endif
