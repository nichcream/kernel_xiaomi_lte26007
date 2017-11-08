/*
**
** Copyright (C) 2011 leadcore, Inc.
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

#ifndef __ARCH_ARM_PLAT_HARDWARE_H__
#define __ARCH_ARM_PLAT_HARDWARE_H__

#include <asm/sizes.h>
#include <mach/hardware.h>
#include <mach/comip-regs.h>

#ifndef __ASSEMBLER__
#define io_p2v(x) ((void __iomem *)__io_p2v((unsigned int)(x)))
#define io_v2p(x) ((void __iomem *)__io_v2p((unsigned int)(x)))
#else
#define io_p2v(x) __io_p2v((x))
#define io_v2p(x) __io_v2p((x))
#endif

#define __REG(x)        (io_p2v(x))
#define __PREG(x)       (io_v2p(x))

#define __REG32(x)      (*((volatile u32 *)io_p2v(x)))
#define __REG16(x)      (*((volatile u16 *)io_p2v(x)))
#define __REG8(x)       (*((volatile u8 *)io_p2v(x)))

/*
 * Common register operation
 */
#define REG32(reg)		(__REG32(reg))
#define REG16(reg)		(__REG16(reg))
#define REG8(reg)		(__REG8(reg))

/* Macro to get at IO space when running virtually */
#define IO_ADDRESS(x)		((u32)io_p2v(x))
#define __io_address(n)		IOMEM(IO_ADDRESS(n))

#endif /* __ARCH_ARM_PLAT_HARDWARE_H__ */
