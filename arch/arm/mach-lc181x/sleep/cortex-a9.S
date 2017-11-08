/*
 * arch/arm/mach-comip/cortex-a9.S
 *
 * CPU state save & restore routines for CPU hotplug
 *
 * Copyright (c) 2012, Leadcore Corporation.
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
#include <asm/domain.h>
#include <asm/ptrace.h>
#include <asm/cache.h>
#include <asm/vfpmacros.h>

#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/suspend.h>
#include "power-macros.S"

#define TTB_FLAGS 0x6A	@ IRGN_WBWA, OC_RGN_WBWA, S, NOS

/*
 *	comip_cpu_save(void *context, void *sp)
 *
 *	 spools out the volatile processor state to memory, so that
 *	 the CPU may be safely powered down. does not preserve:
 *	 - CP15 c0 registers (except cache size select 2,c0/c0,0)
 *	 - CP15 c1 secure registers (c1/c1, 0-3)
 *	 - CP15 c5 fault status registers (c5/c0 0&1, c5/c1 0&1)
 *	 - CP15 c6 fault address registers (c6/c0 0&2)
 *	 - CP15 c9 performance monitor registers (c9/c12 0-5,
 *	     c9/c13 0-2, c9/c14 0-2)
 *	 - CP15 c10 TLB lockdown register (c10/c0, 0)
 *	 - CP15 c12 MVBAR (c12/c0, 1)
 *	 - CP15 c15 TLB lockdown registers
 */
ENTRY(comip_cpu_save)
	stmfd	sp!, {r4-r9, lr}	
	mov	r8, r0
	
	mov	r0, r1
	mrs	r1, cpsr
	mrs	r2, spsr
	mrc	p15, 0, r3, c1, c0, 2	@ cpacr:coprocessor access control register
	stmia	r8, {r0-r3}             @ save SP,CPSR,SPSR,CPACR
	
	mrc	p15, 2, r0, c0, c0, 0	@ csselr
	mrc	p15, 0, r1, c1, c0, 0	@ sctlr
	mrc	p15, 0, r2, c1, c0, 1	@ actlr
	mrc	p15, 0, r4, c15, c0, 0	@ pctlr ,power control
	add	r9, r8, #CTX_CSSELR
	stmia	r9, {r0-r2, r4}       

#ifdef CONFIG_VFPv3
	orr	r2, r3, #0xF00000
	mcr	p15, 0, r2, c1, c0, 2	@ enable access to FPU
	VFPFMRX	r2, FPEXC
	str	r2, [r8, #CTX_FPEXC]
	
	mov	r1, #0x40000000		@ enable access to FPU
	VFPFMXR	FPEXC, r1
	VFPFMRX	r1, FPSCR
	str	r1, [r8, #CTX_FPSCR]	
	isb
	
	add	r9, r8, #CTX_VFP_REGS
	VFPFSTMIA r9, r7	        @ save out (16 or 32)*8B of FPU registers
	VFPFMXR	FPEXC, r2
	mrc	p15, 0, r3, c1, c0, 2	@ restore original FPEXC/CPACR
#endif

	mrc	p15, 0, r0, c15, c0, 1	@ diag
	str	r0, [r8, #CTX_DIAGNOSTIC]

	add	r9, r8, #CTX_TTBR0
	mrc	p15, 0, r0, c2, c0, 0	@ TTBR0
	mrc	p15, 0, r1, c2, c0, 1	@ TTBR1
	mrc	p15, 0, r2, c2, c0, 2	@ TTBCR
	mrc	p15, 0, r3, c3, c0, 0	@ domain access control reg
	mrc	p15, 0, r4, c7, c4, 0	@ PAR
	mrc	p15, 0, r5, c10, c2, 0	@ PRRR
	mrc	p15, 0, r6, c10, c2, 1	@ NMRR
	mrc	p15, 0, r7, c12, c0, 0	@ VBAR
	stmia	r9!, {r0-r7}
	
	mrc	p15, 0, r0, c13, c0, 1	@ CONTEXTIDR
	mrc	p15, 0, r1, c13, c0, 2	@ TPIDRURW
	mrc	p15, 0, r2, c13, c0, 3	@ TPIDRURO
	mrc	p15, 0, r3, c13, c0, 4	@ TPIDRPRW
	stmia	r9, {r0-r3}

	cps	0x1f			@ SYS mode
	add	r9, r8, #CTX_SYS_SP
	stmia	r9, {sp,lr}

	cps	0x17			@ Abort mode
	mrs	r0, spsr
	add	r9, r8, #CTX_ABT_SPSR
	stmia	r9, {r0,sp,lr}

	cps	0x12			@ IRQ mode
	mrs	r0, spsr
	add	r9, r8, #CTX_IRQ_SPSR
	stmia	r9, {r0,sp,lr}

	cps	0x1b			@ Undefined mode
	mrs	r0, spsr
	add	r9, r8, #CTX_UND_SPSR
	stmia	r9, {r0,sp,lr}

	mov	r0, r8
	add	r1, r8, #CTX_FIQ_SPSR
	cps	0x11			@ FIQ mode
	mrs	r7, spsr
	stmia	r1, {r7-r12,sp,lr}

	cps	0x13			@ back to SVC
	mov	r8, r0

	/* Save CP14 debug controller context */
	add	r9, r8, #CTX_CP14_REGS
	mrc     p14, 0, r0, c0, c1, 0	@ DSCR
	mrc	p14, 0, r1, c0, c6, 0	@ WFAR
	mrc	p14, 0, r2, c0, c7, 0	@ VCR
	mrc	p14, 0, r3, c7, c9, 6	@ CLAIM
	stmia	r9, {r0-r3}

	add	r9, r8, #CTS_CP14_BKPT_0
	mrc	p14, 0, r2, c0, c0, 4
	mrc	p14, 0, r3, c0, c0, 5
	stmia	r9!, {r2-r3}     	@ BRKPT_0
	
	mrc	p14, 0, r2, c0, c1, 4
	mrc	p14, 0, r3, c0, c1, 5
	stmia	r9!, {r2-r3}     	@ BRKPT_1
	
	mrc	p14, 0, r2, c0, c2, 4
	mrc	p14, 0, r3, c0, c2, 5
	stmia	r9!, {r2-r3}     	@ BRKPT_2
	
	mrc	p14, 0, r2, c0, c3, 4
	mrc	p14, 0, r3, c0, c3, 5
	stmia	r9!, {r2-r3}     	@ BRKPT_3
	
	mrc	p14, 0, r2, c0, c4, 4
	mrc	p14, 0, r3, c0, c4, 5
	stmia	r9!, {r2-r3}     	@ BRKPT_4
	
	mrc	p14, 0, r2, c0, c5, 4
	mrc	p14, 0, r3, c0, c5, 5
	stmia	r9!, {r2-r3}     	@ BRKPT_5

	add	r9, r8, #CTS_CP14_WPT_0
	mrc	p14, 0, r2, c0, c0, 6
	mrc	p14, 0, r3, c0, c0, 7
	stmia	r9!, {r2-r3}     	@ WPT_0
	
	mrc	p14, 0, r2, c0, c1, 6
	mrc	p14, 0, r3, c0, c1, 7
	stmia	r9!, {r2-r3}     	@ WPT_0
	
	mrc	p14, 0, r2, c0, c2, 6
	mrc	p14, 0, r3, c0, c2, 7
	stmia	r9!, {r2-r3}     	@ WPT_0
	
	mrc	p14, 0, r2, c0, c3, 6
	mrc	p14, 0, r3, c0, c3, 7
	stmia	r9!, {r2-r3}     	@ WPT_0

	mov	r0, #0
	ldmfd	sp!, {r4-r9, pc}
ENDPROC(comip_cpu_save)

/*
 *	comip_cpu_restore(void *context)
 *
 */
	.align L1_CACHE_SHIFT
ENTRY(comip_cpu_restore)
	stmfd	sp!, {r4-r9, lr}	
	
	cps	0x11			@ FIQ mode
	add	r1, r0, #CTX_FIQ_SPSR
	ldmia	r1, {r7-r12,sp,lr}
	msr	spsr_fsxc, r7

	cps	0x12			@ IRQ mode
	add	r1, r0, #CTX_IRQ_SPSR
	ldmia	r1, {r2, sp, lr}
	msr	spsr_fsxc, r2

	cps	0x17			@ abort mode
	add	r1, r0, #CTX_ABT_SPSR
	ldmia	r1, {r2, sp, lr}
	msr	spsr_fsxc, r2

	cps	0x1f			@ SYS mode
	add	r1, r0, #CTX_SYS_SP
	ldmia	r1, {sp, lr}

	cps	0x1b			@ Undefined mode
	add	r1, r0, #CTX_UND_SPSR
	ldmia	r1, {r2, sp, lr}
	msr	spsr_fsxc, r2

	cps	0x13			@ back to SVC
	mov	r8, r0

	add	r9, r8, #CTX_CSSELR
	ldmia	r9, {r0-r3}

	mcr	p15, 2, r0, c0, c0, 0	@ csselr
	mcr	p15, 0, r1, c1, c0, 0	@ sctlr
	mcr	p15, 0, r2, c1, c0, 1	@ actlr
	orr	r3, r3, #1		@enable clock gating
	mcr	p15, 0, r3, c15, c0, 0	@ pctlr

	add	r9, r8, #CTX_TTBR0
	ldmia	r9!, {r0-r7}

	mcr	p15, 0, r4, c7, c4, 0	@ PAR
	mcr	p15, 0, r7, c12, c0, 0	@ VBAR
	mcr	p15, 0, r3, c3, c0, 0	@ domain access control reg
	isb
	mcr	p15, 0, r2, c2, c0, 2	@ TTBCR
	isb
	mcr	p15, 0, r5, c10, c2, 0	@ PRRR
	isb
	mcr	p15, 0, r6, c10, c2, 1	@ NMRR
	isb

	ldmia	r9, {r4-r7}

	mcr	p15, 0, r5, c13, c0, 2	@ TPIDRURW
	mcr	p15, 0, r6, c13, c0, 3	@ TPIDRURO
	mcr	p15, 0, r7, c13, c0, 4	@ TPIDRPRW

	/* perform context switch to previous context */
	mov	r9, #0
	mcr	p15, 0, r9, c13, c0, 1	@ set reserved context
	isb
	mcr	p15, 0, r0, c2, c0, 0	@ TTBR0
	isb
	mcr	p15, 0, r4, c13, c0, 1	@ CONTEXTIDR
	isb
	mcr	p15, 0, r1, c2, c0, 1	@ TTBR1
	isb

	mov	r4, #0
	mcr	p15, 0, r4, c8, c3, 0	@ invalidate TLB
	mcr	p15, 0, r4, c7, c5, 6	@ flush BTAC
	mcr	p15, 0, r4, c7, c5, 0	@ flush instruction cache
	dsb
	isb

	/* Restore CP14 debug controller context */
	add	r9, r8, #CTX_CP14_REGS
	ldmia	r9, {r0-r3}
	mcr	p14, 0, r1, c0, c6, 0	@ WFAR
	mcr	p14, 0, r2, c0, c7, 0	@ VCR
	mcr	p14, 0, r3, c7, c8, 6	@ CLAIM

	add	r9, r8, #CTS_CP14_BKPT_0
	ldmia	r9!, {r2-r3}		@ BRKPT_0
	mcr	p14, 0, r2, c0, c0, 4
	mcr	p14, 0, r3, c0, c0, 5
	ldmia	r9!, {r2-r3}		@ BRKPT_1
	mcr	p14, 0, r2, c0, c1, 4
	mcr	p14, 0, r3, c0, c1, 5
	ldmia	r9!, {r2-r3}		@ BRKPT_2
	mcr	p14, 0, r2, c0, c2, 4
	mcr	p14, 0, r3, c0, c2, 5
	ldmia	r9!, {r2-r3}		@ BRKPT_3
	mcr	p14, 0, r2, c0, c3, 4
	mcr	p14, 0, r3, c0, c3, 5
	ldmia	r9!, {r2-r3}		@ BRKPT_4
	mcr	p14, 0, r2, c0, c4, 4
	mcr	p14, 0, r3, c0, c4, 5
	ldmia	r9!, {r2-r3}		@ BRKPT_5
	mcr	p14, 0, r2, c0, c5, 4
	mcr	p14, 0, r3, c0, c5, 5

	add	r9, r8, #CTS_CP14_WPT_0
	ldmia	r9!, {r2-r3}		@ WPT_0
	mcr	p14, 0, r2, c0, c0, 6
	mcr	p14, 0, r3, c0, c0, 7
	ldmia	r9!, {r2-r3}		@ WPT_1
	mcr	p14, 0, r2, c0, c1, 6
	mcr	p14, 0, r3, c0, c1, 7
	ldmia	r9!, {r2-r3}		@ WPT_2
	mcr	p14, 0, r2, c0, c2, 6
	mcr	p14, 0, r3, c0, c2, 7
	ldmia	r9!, {r2-r3}		@ WPT_3
	mcr	p14, 0, r2, c0, c3, 6
	mcr	p14, 0, r3, c0, c3, 7
	isb
	mcr	p14, 0, r0, c0, c2, 2	@ DSCR
	isb

	ldr	r7, [r8, #CTX_CPACR]
#ifdef CONFIG_VFPv3
	orr	r3, r7, #0xF00000
	mcr	p15, 0, r3, c1, c0, 2	@ enable coproc access
	mov	r3, #0x40000000
	VFPFMXR	FPEXC, r3		@ enable FPU access
	add	r3, r8, #CTX_VFP_REGS
	add	r2, r8, #CTX_FPEXC
	VFPFLDMIA r3, r1
	ldmia	r2, {r0-r1}
	VFPFMXR	FPSCR, r1
	VFPFMXR	FPEXC, r0
#endif

	mcr	p15, 0, r7, c1, c0, 2 	@ cpacr (loaded before VFP)

	ldr	r9, [r8, #CTX_DIAGNOSTIC]
	mcr	p15, 0, r9, c15, c0, 1	@ diag

	mov	r0, #0
	ldmfd	sp!, {r4-r9, pc}

ENDPROC(comip_cpu_restore)



/*
 *	comip_flush_dcache_all(void)
 *
 *	  fluash the L1 data cache. Corrupted registers: r0-r6
 */
ENTRY(comip_flush_dcache_all)
	stmfd	sp!, {r4-r6, lr}
	dmb					@ ensure ordering with previous memory accesses
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
	@mcr	p15, 0, r5, c7, c10, 2		@ clean  by set/way
	mcr	p15, 0, r5, c7, c14, 2		@ clean&invalidate  by set/way
	bgt	4b
	cmp	r2, #0
	bgt	3b
	dsb
	isb
	ldmfd	sp!, {r4-r6, lr}
	bx	lr
ENDPROC(comip_flush_dcache_all)

/*
 *	__return_to_virtual(unsigned long pgdir, void (*ctx_restore)(void))
 *
 *	  Restores a CPU to the world of virtual addressing, using the
 *	  specified page tables (which must ensure that a VA=PA mapping
 *	  exists for the __enable_mmu function), and then jumps to
 *	  ctx_restore to restore CPU context and return control to the OS
 */
	.align L1_CACHE_SHIFT
ENTRY(__return_to_virtual)
	orr	r8, r0, #TTB_FLAGS
	mov	lr, r1		       	@ "return" to ctx_restore
	
	mov	r3, #0
	mcr	p15, 0, r3, c2, c0, 2	@ TTB control register
	mcr	p15, 0, r8, c2, c0, 1	@ load TTBR1

	mov	r0, #0x1f
	mcr	p15, 0, r0, c3, c0, 0	@ domain access register

	ldr	r0, =0xff0a89a8
#ifdef CONFIG_SMP
	ldr	r1, =0xc0e0c4e0
#else
	ldr	r1, =0x40e044e0
#endif
	mcr	p15, 0, r0, c10, c2, 0	@ PRRR
	mcr	p15, 0, r1, c10, c2, 1	@ NMRR
	
	mrc	p15, 0, r0, c1, c0, 0
	ldr	r1, =0x0120c302
	bic	r0, r0, r1
	ldr	r1, =0x10c03c7d
	orr	r0, r0, r1

#ifdef CONFIG_ALIGNMENT_TRAP
	orr	r0, r0, #0x2
#else
	bic	r0, r0, #0x2
#endif
	mov	r1, #(domain_val(DOMAIN_USER, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_TABLE, DOMAIN_MANAGER) | \
		      domain_val(DOMAIN_IO, DOMAIN_CLIENT))
	mcr	p15, 0, r1, c3, c0, 0	@ domain access register
	mcr	p15, 0, r8, c2, c0, 0	@ TTBR0
	b	__turn_mmu_on_again
	andeq	r0, r0, r0
	andeq	r0, r0, r0
	andeq	r0, r0, r0
	andeq	r0, r0, r0
ENDPROC(__return_to_virtual)

/*
 *	__turn_mmu_on_again
 *
 *	  does exactly what it advertises: turns the MMU on, again
 *	  jumps to the *virtual* address lr after the MMU is enabled.
 */
	.align	L1_CACHE_SHIFT
ENTRY(__turn_mmu_on_again)
	mov	r0, r0
	mcr	p15, 0, r0, c1, c0, 0
	mrc	p15, 0, r3, c0, c0, 0
	mov	r3, r3
	mov	r3, lr
	bx	lr
ENDPROC(__turn_mmu_on_again)

