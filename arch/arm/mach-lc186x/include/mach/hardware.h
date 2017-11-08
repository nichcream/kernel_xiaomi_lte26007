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
 *
 * 0xa0000000 - 0xa09fffff <--> 0xf8000000 - 0xf89fffff
 * 0xe0000000 - 0xe00fffff <--> 0xfc000000 - 0xfc0fffff
 * 0xe1000000 - 0xe11fffff <--> 0xfd000000 - 0xfd1fffff
 *
 */
#define __io_p2v(x) (((x & 0xf0000000) >> 4)  + (x & 0x01ffffff) + 0xee000000)
#define __io_v2p(x) ((((x - 0xee000000) & 0x0e000000) << 4)  + (x & 0x01ffffff))

#define DEBUG_UART_BASE         COM_UART_BASE

#endif /* __ASM_ARCH_MACH_HARDWARE_H__ */
