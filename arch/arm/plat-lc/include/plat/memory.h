/* include/asm-arm/arch-comip/memory.h
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

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <linux/version.h>

/*
 * Physical DRAM offset.
 */
#define PLAT_PHYS_OFFSET	UL(CONFIG_PHYS_OFFSET)
#define BUS_OFFSET		UL(0x00000000)

#define CONSISTENT_DMA_SIZE	(28 << 20)

#endif
