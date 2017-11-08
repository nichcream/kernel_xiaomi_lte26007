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

#ifndef __ASM_ARCH_MACH_HARDWARE_H__
#define __ASM_ARCH_MACH_HARDWARE_H__

/*
 * Where in virtual memory the IO devices (timers, system controllers
 * and so on)
 */
#define __io_p2v(x) ((x) + 0x58000000)
#define __io_v2p(x) ((x) - 0x58000000)

#define DEBUG_UART_BASE         COM_UART_BASE

#endif /* __ASM_ARCH_MACH_HARDWARE_H__ */
