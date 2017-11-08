/*
 * arch/arm/include/asm/tc2_pm_setup.S
 *
 * Created by: Nicolas Pitre, October 2012
 (             (based on dcscb_setup.S by Dave Martin)
 * Copyright:  (C) 2012  Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/linkage.h>
#include <asm/mcpm.h>
#include <mach/comip-regs.h>



#define WAKE_INT_STAT	                0xA0110004

#define SNOOP_CTL_A15		0x404
#define SNOOP_CTL_A7		0x504

#define A15_SNOOP_MASK		(0x3 << 7)
#define A7_SNOOP_MASK		(0x1 << 13)

#define A15_BX_ADDR0		0xB68


ENTRY(lc186x_bL_resume)
	mrc	p15, 0, r0, c0, c0, 5
	ubfx	r1, r0, #0, #4		@ r1 = cpu
	ubfx	r2, r0, #8, #4		@ r2 = cluster
	add	r1, r1, r2, lsl #2	@ r1 = index of CPU in WAKE_INT_STAT
	ldr	r3, =WAKE_INT_STAT
	ldr	r3, [r3]
	lsr	r3, r1
	tst	r3, #1
	wfieq				@ if no pending IRQ reenters wfi
	b	mcpm_entry_point
ENDPROC(lc186x_bL_resume)

/*
 * Enable cluster-level coherency, in preparation for turning on the MMU.
 * The ACTLR SMP bit does not need to be set here, because cpu_resume()
 * already restores that.
 */

ENTRY(lc186x_bL_pm_power_up_setup)

	cmp	r0, #0
	beq	2f

	b cci_enable_port_for_self

2:	@ Clear the BX addr register
	@ldr	r3, =SPC_PHYS_BASE + A15_BX_ADDR0
	@mrc	p15, 0, r0, c0, c0, 5	@ MPIDR
	@ubfx	r1, r0, #8, #4		@ cluster
	@ubfx	r0, r0, #0, #4		@ cpu
	@add	r3, r3, r1, lsl #4
	@mov	r1, #0
	@str	r1, [r3, r0, lsl #2]
	@dsb

	bx	lr

ENDPROC(lc186x_bL_pm_power_up_setup)

ENTRY(cci_enable_port_for_self)
	mrc	p15, 0, r0, c0, c0, 5	@ get MPIDR value
	ubfx	r1, r0, #8, #4		@ cluster
	ubfx	r0, r0, #0, #4		@ cpu
	cmp	r0, #0                               @ cpu0
	bne	4f
	cmp	r1, #0
	ldreq	r2, =CCI_SNOOP_CTL3_SA7
	ldreq	r3, =CTL_AP_SA7_CTRL
	beq	1f
	ldr	r2, =CCI_SNOOP_CTL4_HA7
	ldr	r3, =CTL_AP_HA7_CTRL
1:
	ldr	r1, [r2]
	ubfx	r0, r1, #0, #2
	cmp	r0, #3
	beq	3f
	orr	r1, r1, #0x3
	str	r1, [r2]
	dsb
2:
	ldr	r2, =CCI_SNOOP_STATUS
	ldr	r1, [r2]
	cmp	r1, #1
	beq	2b
3:
	ldr	r1, [r3]
	ubfx	r0, r1, #0, #1
	cmp	r0, #0
	beq	4f
	bic	r1, r1, #1
	str	r1, [r3]
	dsb
4:
	bx	lr
ENDPROC(cci_enable_port_for_self)
