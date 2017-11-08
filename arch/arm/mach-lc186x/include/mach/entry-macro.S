/* linux/arch/arm/mach-comip/include/mach/entry-macro.S
 *
 * Copyright (c) 2011 Leadcore Tech Co., Ltd.
 *
 * Low-level IRQ helper macros for the Leadcore L1810
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <mach/io.h>
#include <mach/hardware.h>

#if defined(CONFIG_ARM_GIC)
#define HAVE_GET_IRQNR_PREAMBLE
#include <asm/hardware/entry-macro-gic.S>

	/* Uses the GIC interrupt controller built into the cpu */
#define ICTRL_BASE (IO_ADDRESS(CPU_CORE_BASE) + 0x100)

	.macro	disable_fiq
	.endm

	.macro	get_irqnr_preamble, base, tmp
	movw \base, #(ICTRL_BASE & 0x0000ffff)
	movt \base, #((ICTRL_BASE & 0xffff0000) >> 16)
	.endm

	.macro  arch_ret_to_user, tmp1, tmp2
	.endm
#else
	/* legacy interrupt controller for AP16 */
	.macro	disable_fiq
	.endm

	.macro	get_irqnr_preamble, base, tmp
	@ enable imprecise aborts
	cpsie	a
	@ EVP base at 0xf010f000
	mov \base, #0xf0000000
	orr \base, #0x00100000
	orr \base, #0x0000f000
	.endm

	.macro	arch_ret_to_user, tmp1, tmp2
	.endm

	.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
	ldr \irqnr, [\base, #0x20]	@ EVT_IRQ_STS
	cmp \irqnr, #0x80
	.endm
#endif

