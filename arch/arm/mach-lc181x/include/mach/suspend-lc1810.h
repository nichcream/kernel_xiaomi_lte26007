/*
 * arch/arm/mach-comip/include/mach/suspend.h
 *
 * Copyright (c) 2012, LC Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
 
#ifndef _MACH_COMIP_SUSPEND_H_
#define _MACH_COMIP_SUSPEND_H_

#include <plat/hardware.h>

#define CPU0_WBOOT_ADDR0	(CTL_A9_CORE0_WBOOT_JUMP_ADDR)
#define CPU0_WAKEUP_FLAG	(CTL_A9_WBOOT_JUMP_ADDR1)

#define CPU1_CBOOT_ADDR		(CTL_A9_CORE1_CBOOT_JUMP_ADDR)
#define CPU1_WBOOT_ADDR0	(CTL_A9_CORE1_WBOOT_JUMP_ADDR)
#define CPU1_WAKEUP_FLAG	(CTL_A9_WBOOT_JUMP_ADDR2)

#define COMIP_WAKEUP_ON		0 /*cpu is running*/
#define COMIP_WAKEUP_WAKING	1 /*cpu is waking another cpu*/
#define COMIP_WAKEUP_DOWN	2 /*cpu is off*/

#define COMIP_SPURIOUS_INT_ID	1023

#define COMIP_WAKEUP_CPU1_SGI_ID	15
#define COMIP_WAKEUP_CPU0_SGI_ID	15

#define CLUSTER_CLK_NAME	"a9_clk"

/* CPU Context area (1KB per CPU) */
#define CONTEXT_SIZE_BYTES_SHIFT	10
#define CONTEXT_SIZE_BYTES	(1<<CONTEXT_SIZE_BYTES_SHIFT)

/*
 * spooled CPU context is 1KB / CPU-begin
 */
#define CTX_SP		0
#define CTX_CPSR 	4
#define CTX_SPSR	8
#define CTX_CPACR	12
#define CTX_CSSELR	16
#define CTX_SCTLR	20
#define CTX_ACTLR	24
#define CTX_PCTLR	28

#define CTX_FPEXC	32
#define CTX_FPSCR	36
#define CTX_DIAGNOSTIC	40

#define CTX_TTBR0	48
#define CTX_TTBR1	52
#define CTX_TTBCR	56
#define CTX_DACR	60
#define CTX_PAR		64
#define CTX_PRRR	68
#define CTX_NMRR	72
#define CTX_VBAR	76
#define CTX_CONTEXTIDR	80
#define CTX_TPIDRURW	84
#define CTX_TPIDRURO	88
#define CTX_TPIDRPRW	92

#define CTX_SVC_SP	0
#define CTX_SVC_LR	-1	
#define CTX_SVC_SPSR	8

#define CTX_SYS_SP	96
#define CTX_SYS_LR	100

#define CTX_ABT_SPSR	112
#define CTX_ABT_SP	116
#define CTX_ABT_LR	120

#define CTX_UND_SPSR	128
#define CTX_UND_SP	132
#define CTX_UND_LR	136

#define CTX_IRQ_SPSR	144
#define CTX_IRQ_SP	148
#define CTX_IRQ_LR	152

#define CTX_FIQ_SPSR	160
#define CTX_FIQ_R8	164
#define CTX_FIQ_R9	168
#define CTX_FIQ_R10	172
#define CTX_FIQ_R11	178
#define CTX_FIQ_R12	180
#define CTX_FIQ_SP	184
#define CTX_FIQ_LR	188

#define CTX_SCU_CTRL    200
#define CTX_SCU_FS_ADDR 204
#define CTX_SCU_FE_ADDR 208

/* context only relevant for master cpu */
#ifdef CONFIG_CACHE_L2X0
#define CTX_L2_CTRL	224
#define CTX_L2_AUX	228
#define CTX_L2_TAG_CTRL	232
#define CTX_L2_DAT_CTRL	236
#define CTX_L2_PREFETCH 240
#define CTX_L2_POWER    244
#endif

#define CTX_VFP_REGS 	256
#define CTX_VFP_SIZE   (32*8)

#define CTX_CP14_REGS	512
#define CTS_CP14_DSCR	512
#define CTX_CP14_WFAR	516
#define CTX_CP14_VCR	520
#define CTX_CP14_CLAIM	524

/* Each of the folowing is 2 32-bit registers */
#define CTS_CP14_BKPT_0	528
#define CTS_CP14_BKPT_1	536
#define CTS_CP14_BKPT_2	544
#define CTS_CP14_BKPT_3	552
#define CTS_CP14_BKPT_4	560
#define CTS_CP14_BKPT_5	568

/* Each of the folowing is 2 32-bit registers */
#define CTS_CP14_WPT_0	576
#define CTS_CP14_WPT_1	584
#define CTS_CP14_WPT_2	592
#define CTS_CP14_WPT_3	600
/*
 * spooled CPU context is 1KB / CPU-end
 */

/*sleep/wakeup status*/
#define SLP_ENTER_RAM	0x11
#define SLP_BF_WFI	0x13
#define SLP_AF_WFI	0x15
#define SLP_BF_RESET	0x17

#define SLP_EXIT_BF_LOCK	0x21
#define SLP_EXIT_AF_LOCK	0x23
#define SLP_EXIT_BF_BYPASS	0x25
#define SLP_EXIT_AF_BYPASS	0x27
#define SLP_EXIT_AF_UNLOCK	0x29

#define SLP_ENTER_BF_LOCK	0x31
#define SLP_ENTER_AF_LOCK	0x33
#define SLP_ENTER_BF_BYPASS	0x35
#define SLP_ENTER_AF_BYPASS	0x37
#define SLP_ENTER_AF_UNLOCK	0x39

#define SLP_BYPASS_EN_ENTER	0x41
#define SLP_BYPASS_EN_BEGIN	0x43
#define SLP_BYPASS_EN_END	0x45

#define SLP_BYPASS_DIS_ENTER	0x51
#define SLP_BYPASS_DIS_BEGIN	0x53
#define SLP_BYPASS_DIS_END	0x55

#ifndef __ASSEMBLY__

enum {
	CPU_ID_0 = 0,
	CPU_ID_1 = 1,
};

extern void *comip_context_area;

int comip_suspend_lp_cpu0(void);
int comip_suspend_lp_cpu1(void);
void __init comip_suspend_init(void);
void comip_lp_startup(void);
int comip_core_save(int cpu);
void comip_wake_up_cpu1(void);
void comip_warm_up_cpu1(void);
#ifdef CONFIG_HAVE_ARM_TWD
void comip_twd_update_prescaler(void *data);
#endif
int comip_cpufreq_idle(int on);
void comip_a9_pd_enable(void);
void comip_a9_pd_disable(void);
void comip_reset_to_phy(unsigned int addr_phy);

#endif
#endif /* _MACH_COMIP_SUSPEND_H_ */
