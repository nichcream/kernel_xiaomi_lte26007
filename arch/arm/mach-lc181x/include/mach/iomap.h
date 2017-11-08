/* 
**
** Copyright (C) 2009 Leadcoretech, Inc.
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

#ifndef __ARCH_ARM_MACH_IOMAP_H
#define __ARCH_ARM_MACH_IOMAP_H

#include <asm/sizes.h>
#include <plat/hardware.h>

#define COMIP_ARM_PERIF_BASE        (CPU_CORE_BASE)
#define COMIP_ARM_PERIF_SIZE		SZ_8K

#define COMIP_ARM_PL310_BASE		(L2_CACHE_CTRL_BASE)
#define COMIP_ARM_PL310_SIZE		SZ_4K

#endif /* __ARCH_ARM_MACH_IOMAP_H */
