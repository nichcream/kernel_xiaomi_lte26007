/*
 * arch/arm/mach-comip/headsmp-comip.S
 *
 * SMP initialization routines for LC SoCs
 *
 * Copyright (c) 2011-2013, Leadcore  Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/cache.h>
#include <asm/smp_scu.h>
#include <asm/cp15.h>

#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/suspend.h>
#include "power-macros.S"

#define POWER_STATUS_REG	(CORE_SCU_BASE + 0x08)
#define SCU_INVAL_RAM_REG	(CORE_SCU_BASE + 0xC)

.macro	FLAG_SET, value, b, con
	ldr\con	r3, =DDR_PWR_GENERAL_REG_4
	mov\con	r2, \value
	str\con\b	r2, [r3]
.endm

.macro	DELAY_US, label, val, b=b
	mov	r2, #0
\label:
	add	r2, r2, #1
	cmp	r2, #\val
	bls	\label\b
.endm

	.align L1_CACHE_SHIFT
	.pushsection	.idmap.text, "ax"

/*
 *	void comip_lp_startup(void)
 */
	.align L1_CACHE_SHIFT
ENTRY(comip_lp_startup)
	setmode PSR_F_BIT | PSR_I_BIT | SVC_MODE, r0
	bl	__invalidate_cpu_state

	cpu_id	r0

	ldr	r1, = POWER_STATUS_REG
	add	r1, r1, r0
	mov	r2, #SCU_PM_NORMAL
	strb	r2, [r1]	@ scu power status[cpu]=2'b00
	dsb

	teq	r0, #0
	beq	2f
	
	ldr	r1, =CPU0_WAKEUP_FLAG
1:	ldr	r2, [r1]
	cmp	r2, #COMIP_WAKEUP_WAKING
	bne	1b

	ldr	r1, =SCU_INVAL_RAM_REG
	mov	r0, r0, lsl #2
	mov	r2, #0xf
	mov	r2, r2, lsl r0
	str	r2, [r1]		 @ invalidate SCU tags
	dsb
	b	ddr_ddr_slp_exit_ret

2:	adr	lr, ddr_ddr_slp_exit_ret
	ldr	r1, ddr_ddr_slp_exit_addr
	mov	r0, #1
	bx	r1

ENTRY(ddr_ddr_slp_exit_ret)
	adr	r4, __comip_lp_data
	ldmia	r4, {r1, r2, r3}
	ldr	r0, [r2]
	bx	r3
ENDPROC(comip_lp_startup)

	.type	__comip_lp_data, %object
__comip_lp_data:
	.long	comip_core_restore
ENTRY(ddr_pgd_phys_addr)
	.long	comip_pgd_phys
ENTRY(ddr_return_to_virtual_addr)
	.long	__return_to_virtual
ENTRY(ddr_ddr_slp_exit_addr)
	.long	ddr_ddr_slp_exit
ENTRY(comip_ddr2ram_addr)
	.long	comip_ddr2ram

	.ltorg

/*
 *	void comip_mmu_close(unsigned int addr_phy)
 */
ENTRY(comip_mmu_close)
	mov	r12, r0
mmu_off:
	dsb
	isb
	mov	r1, #0
	mcr	p15, 0, r1, c7, c1, 6	@ flush BTAC shareable
	mcr	p15, 0, r1, c7, c1, 0	@ flush instruction cache shareable
	dsb
	isb

	mrc	p15, 0, r1, c1, c0, 0	@ Read SCTLR
	bic	r1, r1, #0x04		@ Clear C bit
	mcr	p15, 0, r1, c1, c0, 0	@ Write SCTLR
	dsb
	isb

	bl	v7_flush_kern_cache_all

	adr	r1, comip_mmu_off_addr
	ldr	r1, [r1]
	bx	r1

	.align L1_CACHE_SHIFT
ENTRY(comip_mmu_off)
	mrc	p15, 0, r1, c1, c0, 0
	movw	r2, #(CR_I | CR_Z | CR_C | CR_M)
	bic	r1, r1, r2
	dsb
	mcr	p15, 0, r1, c1, c0, 0
	mrc	p15, 0, r1, c0, c0, 0
	mov	r1, r1
	mov	r1, r1
	isb
	bx	r12

ENTRY(comip_mmu_off_addr)
	.long	comip_mmu_off
ENTRY(comip_mmu_off_return_addr)
	.long	comip_mmu_off_return

/*
 *	int comip_core_save(int cpu)
 */
ENTRY(comip_core_save)
	mrs	r3, cpsr
	cps	0x13			@ save off svc registers
	stmfd	sp!, {r3-r12, lr}

	mov	r7, r0	
	mov	r1, sp
	bl	comip_context_save
	
	ldr	r12, comip_mmu_off_return_addr
	b	mmu_off
ENTRY(comip_mmu_off_return)
	mov	r0, r7
	adr	r3, comip_ddr2ram_addr
	ldr	r3, [r3]
	bx	r3
ENTRY(comip_ddr2ram_ret) /*never come here*/
	mov	pc, lr	
ENDPROC(comip_core_save)

/*
 *	int comip_core_restore(void)
 */
	.align L1_CACHE_SHIFT
ENTRY(comip_core_restore)
	ctx_ptr	r8, r0

	ldmia	r8, {r1-r3}
	msr	cpsr_cxsf, r2
	msr	spsr_cxsf, r3
	mov	sp, r1

	bl	comip_context_restore

	mov	r0, #1
	ldmfd	sp!, {r3-r12, lr}
	msr	cpsr_fsxc, r3		@ restore original processor mode
	isb
	mov	pc, lr	

ENDPROC(comip_core_restore)

	.ltorg


/*
 *	__invalidate_cpu_state(void)
 *
 *	 Invalidates volatile CPU state (SCU tags, caches, branch address
 *	 arrays, exclusive monitor, etc.) so that they can be safely enabled
 *	 instruction caching and branch predicition enabled as early as
 *	 possible to improve performance
 */
ENTRY(__invalidate_cpu_state)
	clrex                                   @clear L1cache local exclusive flag
	mov	r0, #0
	mcr	p15, 0, r0, c1, c0, 1	@ disable SMP, prefetch, broadcast
	isb
	mcr	p15, 0, r0, c7, c5, 0	@ invalidate BTAC, i-cache
	mcr	p15, 0, r0, c7, c5, 6	@ invalidate branch pred array
	mcr	p15, 0, r0, c8, c7, 0	@ invalidate unified TLB
	dsb
	isb

	mov	r0, #0x1800
	mcr	p15, 0, r0, c1, c0, 0	@ enable branch prediction, i-cache
	isb
	
	b	__invalidate_l1		@ invalidate data cache
ENDPROC(__invalidate_cpu_state)

/*
 *	__invalidate_l1(void)
 *
 *	  Invalidates the L1 data cache (no clean) during initial boot 
 *
 */
__invalidate_l1:
	mov	r0, #0
	mcr	p15, 2, r0, c0, c0, 0
	mrc	p15, 1, r0, c0, c0, 0  @CCSIDR:dcache information

	movw	r1, #0x7fff
	and	r2, r1, r0, lsr #13

	movw	r1, #0x3ff

	and	r3, r1, r0, lsr #3  @ NumWays - 1
	add	r2, r2, #1	@ NumSets

	and	r0, r0, #0x7
	add	r0, r0, #4	@ SetShift

	clz	r1, r3		@ WayShift
	add	r4, r3, #1	@ NumWays
3:	sub	r2, r2, #1	@ NumSets--
	mov	r3, r4		@ Temp = NumWays
4:	subs    r3, r3, #1	@ Temp--
	mov	r5, r3, lsl r1
	mov	r6, r2, lsl r0
	orr	r5, r5, r6	       @ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
	mcr	p15, 0, r5, c7, c6, 2 @invalidate  by set/way
	bgt	4b
	cmp	r2, #0
	bgt	3b
	dsb
	isb
	bx	lr
ENDPROC(__invalidate_l1)

/*
 *	void comip_put_cpu_reset(cpu_nr)
 *
 *	 puts the specified CPU in wait-for-int mode.
 */
ENTRY(comip_put_cpu_reset)

	ldr	r1, =POWER_STATUS_REG
	add	r1, r1, r0
	mov	r2, #SCU_PM_POWEROFF
	strb	r2, [r1]           @ scu power status[cpu]=2'b11
	dsb

	mrc	p15, 0, r3, c1, c0, 1
	mov	r2, #0
	mcr	p15, 0, r2, c1, c0, 1	@ disable SMP, prefetch, broadcast
	isb
	dsb

	wfi
	isb
	dsb

	mov	pc, lr

ENDPROC(comip_put_cpu_reset)

	.ltorg

/*
 *	void ddr_dll_bypass_enable(int flag)
 *	flag:  0: mmu is on 
 *	       1: mmu is off
 */
ENTRY(ddr_dll_bypass_enable)
	FLAG_SET #SLP_BYPASS_EN_ENTER, b

41:	tst	r0, #1
	ldrne	r3, DDR_PWR_MEMCTL_STATE_PA
	ldreq	r3, DDR_PWR_MEMCTL_STATE_VA
42:	ldr	r1, [r3]
	ubfx	r1, r1, #8, #3
	cmp	r1, #5
	beq	43f
	b	42b

	mov	r0, #1
	bx	lr

43:	
	FLAG_SET #SLP_BYPASS_EN_BEGIN, b

	tst	r0, #1
	ldreq	r3, PHY_DDR_BASE_VA
	ldrne	r3, PHY_DDR_BASE_PA
	mov	r1, #0x20000
	str	r1, [r3, #4]	@PHY_DDR_PIR

	ldr	r1, [r3, #0x24]
	movw	r2, #0x708
	movt	r2, #0x83c 	@r2= 0x083c0708
	orr	r2, r1, r2
	str	r2, [r3, #0x24]	@PHY_DDR_ACIOCR

	ldr	r2, [r3, #0x1c0]
	orr	r2, r2, #0x30
	str	r2, [r3, #0x1c0]	@PHY_DDR_DX0GCR

	ldr	r2, [r3, #0x200]
	orr	r2, r2, #0x30
	str	r2, [r3, #0x200]	@PHY_DDR_DX1GCR

	ldr	r2, [r3, #0x240]
	orr	r2, r2, #0x30
	str	r2, [r3, #0x240]	@PHY_DDR_DX2GCR

	ldr	r2, [r3, #0x280]
	orr	r2, r2, #0x30
	str	r2, [r3, #0x280]	@PHY_DDR_DX3GCR

	dsb

	/*wait 1us*/
	mov	r2, #0
	movw	r3, #100
44:	add	r2, r2, #1
	cmp	r2, r3
	bls	44b

	FLAG_SET #SLP_BYPASS_EN_END, b

	mov	r0, #0
	bx	lr

/*
 *	ddr_dll_bypass_disable(int flag)
 *	flag:  0:  mmu is on
 *		1: mmu is off
 */

ENTRY(ddr_dll_bypass_disable)
	FLAG_SET #SLP_BYPASS_DIS_ENTER, b

51:	tst	r0, #1
	ldreq	r3, DDR_PWR_MEMCTL_STATE_VA
	ldrne	r3, DDR_PWR_MEMCTL_STATE_PA
52:	ldr	r1, [r3]
	ubfx	r1, r1, #8, #3
	cmp	r1, #5
	beq	53f
	b	52b

	mov	r0, #1
	bx	lr

53:	
	FLAG_SET #SLP_BYPASS_DIS_BEGIN, b

	tst	r0, #1
	ldreq	r3, DDR_PWR_BASE_VA
	ldrne	r3, DDR_PWR_BASE_PA
	ldr	r1, =0x1010001
	ldr	r2, [r3, #0x98]
	mov	r4, r2
	bic	r2, r2, r1
	str	r2, [r3, #0x98]
	dsb

	tst	r0, #1
	ldreq	r3, PHY_DDR_BASE_VA
	ldrne	r3, PHY_DDR_BASE_PA

	movw	r2, #0xfef7
	movt	r2, #0xf7f3	@r2 = 0xf7f3fef7
	ldr	r1, [r3, #0x24]
	and	r2, r1, r2
	str	r2, [r3, #0x24]	@PHY_DDR_ACIOCR

	ldr	r2, [r3, #0x1c0]
	bic	r2, r2, #0x30
	str	r2, [r3, #0x1c0] 	@PHY_DDR_DX0GCR

	ldr	r2, [r3, #0x200]
	bic	r2, r2, #0x30
	str	r2, [r3, #0x200]	@PHY_DDR_DX1GCR

	ldr	r2, [r3, #0x240]
	bic	r2, r2, #0x30
	str	r2, [r3, #0x240]	@PHY_DDR_DX2GCR

	ldr	r2, [r3, #0x280]
	bic	r2, r2, #0x30
	str	r2, [r3, #0x280]	@PHY_DDR_DX3GCR

	ldr	r2, [r3, #0x14]
	bic	r2, r2, #0x40000000
	str	r2, [r3, #0x14]	@acdllcr

	ldr	r2, [r3, #0x1cc]
	bic	r2, r2, #0x40000000
	str	r2, [r3, #0x1cc]	@PHY_DDR_DX0DLLCR

	ldr	r2, [r3, #0x20c]
	bic	r2, r2, #0x40000000
	str	r2, [r3, #0x20c]	@PHY_DDR_DX1DLLCR

	ldr	r2, [r3, #0x24c]
	bic	r2, r2, #0x40000000
	str	r2, [r3, #0x24c]	@PHY_DDR_DX2DLLCR

	ldr	r2, [r3, #0x28c]
	bic	r2, r2, #0x40000000
	str	r2, [r3, #0x28c]	@PHY_DDR_DX3DLLCR
	dsb
	isb

	mov	r2, #0
	str	r2, [r3, #4]		@PHY_DDR_PIR
	dsb
	isb

	DELAY_US 56, 200
	
	ldr	r2, [r3, #0x14]
	orr	r2, r2, #0x40000000
	str	r2, [r3, #0x14]

	ldr	r2, [r3, #0x1cc]
	orr	r2, r2, #0x40000000
	str	r2, [r3, #0x1cc]	@PHY_DDR_DX0DLLCR

	ldr	r2, [r3, #0x20c]
	orr	r2, r2, #0x40000000
	str	r2, [r3, #0x20c]	@PHY_DDR_DX1DLLCR

	ldr	r2, [r3, #0x24c]
	orr	r2, r2, #0x40000000
	str	r2, [r3, #0x24c]	@PHY_DDR_DX2DLLCR

	ldr	r2, [r3, #0x28c]
	orr	r2, r2, #0x40000000
	str	r2, [r3, #0x28c]	@PHY_DDR_DX3DLLCR
	dsb

	DELAY_US 57, 300

	mov	r2, #0x03
	str	r2, [r3, #4]		@PHY_DDR_PIR
	dsb
	isb

	/*wait ~6 us*/
	DELAY_US 55, 600

	tst	r0, #1
	ldreq	r3, DDR_PWR_BASE_VA
	ldrne	r3, DDR_PWR_BASE_PA
	str	r4, [r3, #0x98]
	dsb

	FLAG_SET #SLP_BYPASS_DIS_END, b

	mov	r0, #0
	bx	lr

DDR_PWR_MEMCTL_STATE_VA:
	.long	IO_ADDRESS(DDR_PWR_MEMCTL_STATE)
DDR_PWR_MEMCTL_STATE_PA:
	.long	DDR_PWR_MEMCTL_STATE
PHY_DDR_BASE_VA:
	.long	IO_ADDRESS(PHY_DDR_BASE)
PHY_DDR_BASE_PA:
	.long	PHY_DDR_BASE
LOOP_CNT_VAL:
	.long	0
CORE_SCU_BASE_VA:
	.long	IO_ADDRESS(CORE_SCU_BASE)

	.ltorg

/*
 *	ddr_mtc_up(int type, int flag)
 */
ENTRY(ddr_mtc_up)

	tst	r1, #1
	ldrne	r3, DDR_PWR_BASE_PA
	ldreq	r3, DDR_PWR_BASE_VA
	
	lsl	r0, r0, #2
	mov	r1, #1
	ldr	r2, [r3, #0xb0]
	bic	r0, r2, r1, lsl r0
	str	r0, [r3, #0xb0]
	dsb

	bx	lr

/*
 *	ddr_mtc_down(int type, int flag)
 */
ENTRY(ddr_mtc_down)

	tst	r1, #1
	ldrne	r3, DDR_PWR_BASE_PA
	ldreq	r3, DDR_PWR_BASE_VA

	mov	r2, #1
	lsl	r1, r0, #2
	lsl	r1, r2, r1

	ldr	r2, [r3, #0xb0]
	orr	r2, r1, r2
	str	r2, [r3, #0xb0]	@DDR_PWR_CPU0_ARBIT_REQ

	mov	r2, #1
	lsl	r2, r2, r0
	movw	r0, #59999
7:	ldr	r1, [r3, #0xc8]	@DDR_PWR_CPU0_ARBIT_ACK
	tst	r1, r2
	movne	r0, #0
	bne	8f

        /*wait ~1 us*/
	mov	r1, #0
71:	add	r1, r1, #1
	cmp	r1, #80
	bls	71b
	b 7b

	mov	r0, #1
8:	bx	lr

DDR_PWR_BASE_PA:
	.long	DDR_PWR_BASE
DDR_PWR_BASE_VA:
	.long	IO_ADDRESS(DDR_PWR_BASE)

	.ltorg

/*
 *	ddr_ddr_slp_enter(int flag)
 *	flag:  0: mmu is on
 *	       1: mmu is off
 */
ENTRY(ddr_ddr_slp_enter)
	mov	r12, lr
	mov	r11, r0

	FLAG_SET #SLP_ENTER_BF_LOCK, b

	mov	r0, #1
	mov	r1, r11
	bl	ddr_mtc_down

	FLAG_SET #SLP_ENTER_AF_LOCK, b

	tst	r11, #1
	ldrne	r3, DDR_PWR_GENERAL_REG_1_PA
	ldreq	r3, DDR_PWR_GENERAL_REG_1_VA	
	ldrb	r2, [r3]
	orr	r2, r2, #0xc0
	strb	r2, [r3]
	and	r2, r2, #0xff

	cmp	r2, #0xff
	bne	9f

	FLAG_SET #SLP_ENTER_BF_BYPASS, b

	mov	r0, r11
	bl	ddr_dll_bypass_enable
9:	
	FLAG_SET #SLP_ENTER_AF_BYPASS, b

	mov	r0, #1
	mov	r1, r11
	bl	ddr_mtc_up

	FLAG_SET #SLP_ENTER_AF_UNLOCK, b

	bx	r12	

/*
 *	ddr_ddr_slp_exit(int flag)
 *	flag:  0: mmu is on
 *	       1: mmu is off
 */
ENTRY(ddr_ddr_slp_exit)
	mov	r12, lr
	mov	r11, r0

	FLAG_SET #SLP_EXIT_BF_LOCK, b

	mov	r0, #1
	mov	r1, r11
	bl	ddr_mtc_down

	FLAG_SET #SLP_EXIT_AF_LOCK, b

	tst	r11, #1
	ldrne	r3, DDR_PWR_GENERAL_REG_1_PA
	ldreq	r3, DDR_PWR_GENERAL_REG_1_VA
	ldrb	r2, [r3]
	and	r2, r2, #0xff
	cmp	r2, #0xff
	bne	10f

	FLAG_SET #SLP_EXIT_BF_BYPASS, b

	mov	r0, r11
	bl	ddr_dll_bypass_disable
10:	
	FLAG_SET #SLP_EXIT_AF_BYPASS, b

	tst	r11, #1
	ldrne	r3, DDR_PWR_GENERAL_REG_1_PA
	ldreq	r3, DDR_PWR_GENERAL_REG_1_VA
	ldrb	r2, [r3]
	bic	r2, r2, #0xc0
	strb	r2, [r3]

	mov	r0, #1
	mov	r1, r11
	bl	ddr_mtc_up

	/*wait ~500 us*/
	mov	r2, #0
	movw	r3, #50000
11:	add	r2, r2, #1
	cmp	r2, r3
	bls	11b

	FLAG_SET #SLP_EXIT_AF_UNLOCK, b

	bx	r12	

DDR_PWR_GENERAL_REG_1_PA:
	.long	DDR_PWR_GENERAL_REG_1
DDR_PWR_GENERAL_REG_1_VA:
	.long	IO_ADDRESS(DDR_PWR_GENERAL_REG_1)

/*
 *	comip_ddr2ram(int cpu)
 */
ENTRY(comip_ddr2ram)
	mov	r7, r0

	teq	r7, #0
	FLAG_SET #SLP_ENTER_RAM, b, eq

	moveq	r0, #1
	bleq	ddr_ddr_slp_enter

	teq	r7, #0	
	FLAG_SET #SLP_BF_WFI, b, eq
	
	mov	r0, r7
	bl	comip_put_cpu_reset

	teq	r7, #0
	FLAG_SET #SLP_AF_WFI, b, eq

	b	comip_lp_startup

	.ltorg

ENTRY(comip_lp_startup_sz)
	.long	. - comip_lp_startup

	.ltorg

/*
 *	.data
 *	.align L1_CACHE_SHIFT
 *__core_stack_top:
 *	.space	8 * 16
 *__core_stack_bottom:
 *	.long	0
 */
 	.popsection
 

