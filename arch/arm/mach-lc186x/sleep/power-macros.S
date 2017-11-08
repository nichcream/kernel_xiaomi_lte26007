/*
 * arch/arm/mach-comip/power-macros.S
 *
 * Assembly macros useful for power state save / restore routines
 *
 * Copyright (c) 2011, Leadcore Corporation.
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
 
#define TTB_IRGN_NC	(0 << 8)
#define TTB_IRGN_WBWA	(1 << 8)
#define TTB_IRGN_WT	(2 << 8)
#define TTB_IRGN_WB	(3 << 8)
#define TTB_RGN_NC	(0 << 10)
#define TTB_RGN_OC_WBWA	(1 << 10)
#define TTB_RGN_OC_WT	(2 << 10)
#define TTB_RGN_OC_WB	(3 << 10)
#define TTB_S		(3 << 12)
#define TTB_EAE		(1 << 31)

/* PTWs cacheable, inner WB not shareable, outer WB not shareable */
#define TTB_FLAGS_UP	(TTB_IRGN_WB|TTB_RGN_OC_WB)
#define PMD_FLAGS_UP	(PMD_SECT_WB)

/* PTWs cacheable, inner WBWA shareable, outer WBWA not shareable */
#define TTB_FLAGS_SMP	(TTB_IRGN_WBWA|TTB_S|TTB_RGN_OC_WBWA)
#define PMD_FLAGS_SMP	(PMD_SECT_WBWA|PMD_SECT_S)

.equ	PRRR,	0xeeaa4400			@ MAIR0
.equ	NMRR,	0xff000004			@ MAIR1

/* returns the ID of the current processor */
.macro	cpu_id, rd
	mrc	p15, 0, \rd, c0, c0, 5
	and	\rd, \rd, #0xF
.endm

.macro  slp_addr, id, addr
	cmp 	\id, #0
	beq	cpu0
	cmp	\id, #1
	beq	cpu1
	cmp	\id, #2
	beq	cpu2
	cmp	\id, #3
	beq	cpu3
	cmp	\id, #4
	beq	cpu4
cpu0:
	ldr	\addr, =CPU0_SLP_STATE_ADDR
	b	out
cpu1:
	ldr	\addr, =CPU1_SLP_STATE_ADDR
	b	out
cpu2:
	ldr	\addr, =CPU2_SLP_STATE_ADDR
	b	out
cpu3:
	ldr	\addr, =CPU3_SLP_STATE_ADDR
	b	out
cpu4:
	ldr	\addr, =CPU4_SLP_STATE_ADDR
out:
	dsb
.endm

.macro mov32, reg, val
	movw	\reg, #:lower16:\val
	movt	\reg, #:upper16:\val
.endm

/* waits until the microsecond counter (base) ticks, for exact timing loops */
.macro	wait_for_us, rd, base, tmp
	ldr	\rd, [\base]
1001:	ldr	\tmp, [\base]
	cmp	\rd, \tmp
	beq	1001b
	mov	\tmp, \rd
.endm

/* waits until the microsecond counter (base) is >= rn */
.macro	wait_until, rn, base, tmp
1002:	ldr	\tmp, [\base]
	sub	\tmp, \tmp, \rn
	ands	\tmp, \tmp, #0x80000000
	dmb
	bne	1002b
.endm

/* Enable Coresight access on cpu */
.macro	enable_coresite, tmp
	mov32	\tmp, 0xC5ACCE55
	mcr	p14, 0, \tmp, c7, c12, 6
.endm

.macro	ctx_ptr, rd, tmp
	cpu_id	\tmp
	mov32	\rd, comip_context_area
	ldr	\rd, [\rd]
	add	\rd, \rd, \tmp, lsl #(CONTEXT_SIZE_BYTES_SHIFT)
.endm
#if defined(CONFIG_CPU_LC1813) || defined(CONFIG_CPU_LC1860)
.macro	FLAG_SET, value, b, con
	ldr\con	r3, =DDR_PWR_GENERAL_REG_4
	mov\con	r2, \value
	str\con\b	r2, [r3]
.endm
#endif
