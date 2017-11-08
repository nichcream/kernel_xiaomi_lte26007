/*
 * arch/arm/mach-181x/sleep.S
 *
 * SMP initialization routines for Leadcore SoCs
 *
 * Copyright (c) 2012, Leadcore Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/cache.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/cp15.h>

#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/suspend.h>

	.pushsection	.idmap.text, "ax"
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
	mov	r0, r11
	bx	r12
	.ltorg
	.popsection

/*
 *	void comip_mmu_close(unsigned int addr_phy, unsigned int args)
 */
ENTRY(comip_mmu_close)
	mov	r12, r0
	mov	r11, r1
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

	ldr	r1, comip_mmu_off_addr
	bx	r1
ENTRY(comip_mmu_off_addr)
	.long	comip_mmu_off	@FIXED to PA

	.ltorg
