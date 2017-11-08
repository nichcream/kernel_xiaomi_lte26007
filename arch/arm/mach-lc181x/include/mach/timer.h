/* arch/arm/mach-comip/include/mach/timer.h
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

#ifndef __ASM_ARCH_TIMER_H
#define __ASM_ARCH_TIMER_H

#include <linux/time.h>
#include <linux/clockchips.h>

extern struct sys_timer comip_timer;
void __init comip_timer_init(void);


#define COMIP_CLOCKEVENT_TIMER_ID				1
#define COMIP_CLOCKEVENT_TIMER_IRQ_ID				INT_TIMER1
#define COMIP_CLOCKEVENT_TIMER_NAME				"timer1_clk"

#define COMIP_CLOCKEVENT1_TIMER_ID				2
#define COMIP_CLOCKEVENT1_TIMER_IRQ_ID				INT_TIMER2
#define COMIP_CLOCKEVENT1_TIMER_NAME				"timer2_clk"

#if defined(CONFIG_CPU_LC1813)
#define COMIP_CLOCKEVENT2_TIMER_ID				4
#define COMIP_CLOCKEVENT2_TIMER_IRQ_ID				INT_TIMER4
#define COMIP_CLOCKEVENT2_TIMER_NAME				"timer4_clk"

#define COMIP_CLOCKEVENT3_TIMER_ID				5
#define COMIP_CLOCKEVENT3_TIMER_IRQ_ID				INT_TIMER5
#define COMIP_CLOCKEVENT3_TIMER_NAME				"timer5_clk"

#define COMIP_CLOCKEVENT4_TIMER_ID				6
#define COMIP_CLOCKEVENT4_TIMER_IRQ_ID				INT_TIMER6
#define COMIP_CLOCKEVENT4_TIMER_NAME				"timer6_clk"
#endif

#ifdef CONFIG_COMIP_USE_WK_TIMER
#define COMIP_CLOCKEVENT_WKUP_TIMER_ID				0
#define COMIP_CLOCKEVENT_WKUP_TIMER_IRQ_ID			INT_TIMER0
#define COMIP_CLOCKEVENT_WKUP_TIMER_NAME			"timer0_clk"
#endif

#define COMIP_CLOCKSROUCE_TIMER_ID				3
#define COMIP_CLOCKSROUCE_TIMER_NAME				"timer3_clk"

#ifdef CONFIG_COMIP_32K_TIMER
#define COMIP_CLOCKEVENT_TIMER_CLKSRC				"clk_32k"
#define COMIP_CLOCKSROUCE_TIMER_CLKSRC				"clk_32k"
#define COMIP_CLOCKEVENT1_TIMER_CLKSRC				"clk_32k"

#if defined( CONFIG_CPU_LC1813)
#define COMIP_CLOCKEVENT2_TIMER_CLKSRC				"clk_32k"
#define COMIP_CLOCKEVENT3_TIMER_CLKSRC				"clk_32k"
#define COMIP_CLOCKEVENT4_TIMER_CLKSRC				"clk_32k"
#endif

#else
#define COMIP_CLOCKEVENT_TIMER_CLKSRC				"pll1_mclk"
#define COMIP_CLOCKSROUCE_TIMER_CLKSRC				"pll1_mclk"
#define COMIP_CLOCKEVENT1_TIMER_CLKSRC				"pll1_mclk"

#if defined(CONFIG_CPU_LC1813)
#define COMIP_CLOCKEVENT2_TIMER_CLKSRC				"pll1_mclk"
#define COMIP_CLOCKEVENT3_TIMER_CLKSRC				"pll1_mclk"
#define COMIP_CLOCKEVENT4_TIMER_CLKSRC				"pll1_mclk"
#endif

#endif

extern void comip_init_clockevent1(struct clock_event_device *evt, unsigned long rate);
#endif /* __ASM_ARCH_TIMER_H. */
