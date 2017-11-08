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

#define AP_PWR_A7Cx_PD_CTL(X)	(AP_PWR_A7C1_PD_CTL + ((X)-1)*4) //X=1,2,3

#define CPU0_WAKEUP_CMD_ADDR	(CTL_A7_WBOOT_JUMP_ADDR1)

/*bit wide: 8 bits*/
#define CPUX_SLP_STATE_BASE	(CTL_A7_WBOOT_JUMP_ADDR2)
#define CPU0_SLP_STATE_ADDR	(CPUX_SLP_STATE_BASE)
#define CPU1_SLP_STATE_ADDR	(CPU0_SLP_STATE_ADDR + 1)
#define CPU2_SLP_STATE_ADDR	(CPU1_SLP_STATE_ADDR + 1)
#define CPU3_SLP_STATE_ADDR	(CPU2_SLP_STATE_ADDR + 1)

#define CPU0_WBOOT_ADDR	(CTL_A7_CORE0_WBOOT_JUMP_ADDR)
#define CPU1_CBOOT_ADDR	(CTL_A7_CORE1_CBOOT_JUMP_ADDR)
#define CPU1_WBOOT_ADDR	(CTL_A7_CORE1_WBOOT_JUMP_ADDR)
#define CPU2_CBOOT_ADDR	(CTL_A7_CORE2_CBOOT_JUMP_ADDR)
#define CPU2_WBOOT_ADDR	(CTL_A7_CORE2_WBOOT_JUMP_ADDR)
#define CPU3_CBOOT_ADDR	(CTL_A7_CORE3_CBOOT_JUMP_ADDR)
#define CPU3_WBOOT_ADDR	(CTL_A7_CORE3_WBOOT_JUMP_ADDR)

#define COMIP_WAKEUP_ID_OFFSET 8 /* cpu id offset*/

#define COMIP_SLP_STATE_DYING	1
#define COMIP_SLP_STATE_DOWN	2
#define COMIP_SLP_STATE_UPPING	3
#define COMIP_SLP_STATE_UP	4
#define COMIP_SLP_STATE_ON	5

#define COMIP_SLP_STATE_START	6
#define COMIP_SLP_STATE_END	7

#define COMIP_WAKEUP_CMD_NONE	8
#define COMIP_WAKEUP_CMD_WAKING	9
#define COMIP_WAKEUP_CMD_DONE	10

#define COMIP_SPURIOUS_INT_ID	1023
#define COMIP_WAKEUP_SGI_ID	15

#define CLUSTER_CLK_NAME	"a7_clk"

#define FLAG_DDR_ADJFREQ_BIT	0 /*adjust frequence when sleep*/
#define FLAG_MMUOFF_BIT	1
#define FLAG_IDLE_PD_BIT	2

#define FLAG_DDR_ADJFREQ_VAL	(1 << FLAG_DDR_ADJFREQ_BIT)
#define FLAG_MMUOFF_VAL	(1 << FLAG_MMUOFF_BIT)
#define FLAG_IDLE_PD_VAL	(1 << FLAG_IDLE_PD_BIT)

#ifndef __ASSEMBLY__
enum {
	CPU_ID_0 = 0,
	CPU_ID_1 = 1,
	CPU_ID_2 = 2,
	CPU_ID_3 = 3,
};

static inline int core_count_get(void)
{
	int count;

	asm("mrc	p15, 1, %0, c9, c0, 2"
		: "=r" (count));
	count >>= 24;
	count &= 0x03;
	return count + 1;
}

void __init comip_suspend_init(void);
void comip_wake_up_cpux(unsigned int cpu);
int comip_cpufreq_idle(int on);
int suspend_cpu_die(int cpu);
void suspend_secondary_init(unsigned int cpu);
int suspend_cpu_poweron(int cpu);
int suspend_cpu_poweroff(int cpu);
void suspend_smp_prepare(void);
void comip_reset_to_phy(unsigned int addr_phy);
int suspend_idle_poweroff(unsigned int cpu);
int suspend_only_cpu0_on(void);
#endif
#endif /* _MACH_COMIP_SUSPEND_H_ */
