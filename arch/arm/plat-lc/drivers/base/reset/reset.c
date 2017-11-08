/* arch/arm/mach-comip/comip-reset.c
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2010-2019 LeadCoreTech Corp.
**
**	PURPOSE: This files contains the comip module reset driver.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2012-03-19	zouqiao	created
**
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include <plat/hardware.h>
#include <plat/reset.h>
#include <mach/reset.h>

static struct rst* __get_rst(const char *name)
{
	unsigned int i;

	if (!name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(rst); i++)
		if (!strcmp(rst[i].name, name))
			return &rst[i];

	return NULL;
}

int comip_reset(const char *name)
{
	struct rst *rst;
	unsigned int val = 0;

	rst = __get_rst(name);
	if (!rst)
		return -EINVAL;

	if (strcmp(name, "AP")) {
		if (!(rst->flags & RST_HARDWARE_CLEAR))
			val = __raw_readl(rst->rst_reg);
		if (rst->flags & RST_WRITE_ENABLE)
			val |= (1 << (rst->rst_bit + 16));
		writel(val | (1 << rst->rst_bit), rst->rst_reg);
		if (!(rst->flags & RST_HARDWARE_CLEAR))
			__raw_writel(val, rst->rst_reg);
	} else
		__raw_writel(0xff, rst->rst_reg);

	return 0;
}
EXPORT_SYMBOL(comip_reset);

