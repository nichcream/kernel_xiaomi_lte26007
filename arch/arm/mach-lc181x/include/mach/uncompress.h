/* include/asm-arm/arch-comip/uncompress.h
**
** Copyright (C) 2009 leadcore, Inc.
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
*/

#ifndef __ASM_ARCH_UNCOMPRESS_H
#define __ASM_ARCH_UNCOMPRESS_H

#include <plat/hardware.h>

#define UART_THR			0x0
#define UART_TSR			0x14

#define TSR_THRE	0x20	/* Xmit holding register empty */

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	volatile unsigned char * uart = (unsigned char *)DEBUG_UART_BASE;
	while (!(uart[UART_TSR] & TSR_THRE))
		barrier();
	uart[UART_THR] = c;
}

static inline void flush(void)
{
}

/*
 * nothing to do
 */
#define arch_decomp_setup()

#define arch_decomp_wdog()

#endif
