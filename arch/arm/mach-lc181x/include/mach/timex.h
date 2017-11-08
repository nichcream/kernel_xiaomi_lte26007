/* include/asm-arm/arch-comip/timex.h
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

#ifndef __ASM_ARCH_TIMEX_H
#define __ASM_ARCH_TIMEX_H

#ifdef CONFIG_COMIP_32K_TIMER
#define CLOCK_TICK_RATE			32768
#else
#if defined(CONFIG_MENTOR_DEBUG)
#define CLOCK_TICK_RATE			650000
#else
#define CLOCK_TICK_RATE			13000000
#endif
#endif

#endif/* __ASM_ARCH_TIMEX_H */
