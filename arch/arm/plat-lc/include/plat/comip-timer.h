/*
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2010-2019  LeadCoreTech Corp.
**
**  PURPOSE:		This files contains generic timer operation.
**
**  CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2010-09-10	zouqiao		created
**
*/

#ifndef __ASM_ARCH_PLAT_TIMER_H
#define __ASM_ARCH_PLAT_TIMER_H

#include <plat/hardware.h>

#define COMIP_REG_BIT_SET(reg, val) \
	writel(readl(reg) | (val), reg)
#define COMIP_REG_BIT_CLR(reg, val) \
	writel(readl(reg) & ~(val), reg)

struct comip_timer_cfg {
	int autoload;
	unsigned int load_val;
};

#define comip_timer_start(nr) \
	COMIP_REG_BIT_SET(io_p2v(COMIP_TIMER_TCR(nr)), TIMER_TES)
#define comip_timer_stop(nr) \
	COMIP_REG_BIT_CLR(io_p2v(COMIP_TIMER_TCR(nr)), TIMER_TES)
#define comip_timer_autoload_enable(nr) \
	COMIP_REG_BIT_SET(io_p2v(COMIP_TIMER_TCR(nr)), TIMER_TMS)
#define comip_timer_autoload_disable(nr) \
	COMIP_REG_BIT_CLR(io_p2v(COMIP_TIMER_TCR(nr)), TIMER_TMS)
#define comip_timer_int_enable(nr) \
	COMIP_REG_BIT_CLR(io_p2v(COMIP_TIMER_TCR(nr)), TIMER_TIM)
#define comip_timer_int_disable(nr) \
	COMIP_REG_BIT_SET(io_p2v(COMIP_TIMER_TCR(nr)), TIMER_TIM)
#define comip_timer_int_status(nr) \
	readl(io_p2v(COMIP_TIMER_TIS(nr)))
#define comip_timer_int_clear(nr) \
	readl(io_p2v(COMIP_TIMER_TIC(nr)))
#define comip_timer_read(nr) \
	readl(io_p2v(COMIP_TIMER_TCV(nr)))
#define comip_timer_enabled(nr) \
	readl(io_p2v(COMIP_TIMER_TCR(nr)))
#define comip_timer_load_value_set(nr, load_val) \
	writel(load_val, io_p2v(COMIP_TIMER_TLC(nr)))
#define comip_timer_tcr_set(nr, val) \
	writel(val, io_p2v(COMIP_TIMER_TCR(nr)))
#define comip_timer_tlc_read(nr) \
	readl(io_p2v(COMIP_TIMER_TLC(nr)))

extern int comip_timer_config(int nr, struct comip_timer_cfg* cfg);

#endif /* __ASM_ARCH_PLAT_TIMER_H */

