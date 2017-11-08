/* arch/arm/mach-comip/comip-timer.c
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

#include <linux/module.h>
#include <linux/io.h>
#include <linux/mm.h>

#include <plat/comip-timer.h>

int comip_timer_config(int nr, struct comip_timer_cfg* cfg)
{
	if (nr >= COMIP_TIMER_NUM)
		return -EINVAL;

	comip_timer_load_value_set(nr, cfg->load_val);
	if (cfg->autoload)
		comip_timer_autoload_enable(nr);
	else
		comip_timer_autoload_disable(nr);

	return 0;
}

EXPORT_SYMBOL(comip_timer_config);

