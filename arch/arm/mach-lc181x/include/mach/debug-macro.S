/* linux/include/asm-arm/arch-comip/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <plat/hardware.h>

	.macro  addruart, rp, rv, tmp
	ldr     \rp, =DEBUG_UART_BASE       @ physical
	ldr     \rv, =io_p2v(DEBUG_UART_BASE)        @ virtual
	.endm

#define UART_SHIFT	2
#include <asm/hardware/debug-8250.S>

