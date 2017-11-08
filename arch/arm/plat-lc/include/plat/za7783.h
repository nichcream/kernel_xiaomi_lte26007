/* arch/arm/mach-comip/include/mach/comipfb.h
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2009-2018	LeadCoreTech Corp.
**
**	PURPOSE:			This files contains the driver of LCD backlight controller.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	0.1.0		2012-03-09	liuyong		created
**
*/

#ifndef __ASM_ARCH_ZA7783_H
#define __ASM_ARCH_ZA7783_H


#define ZA7783_NAME	"za7783"

struct za7783_platform_data {
	int irq_gpio;
	const char* mclk_parent_name;
	const char* mclk_name;
	unsigned long mclk_rate;
	int (*power)(int);
};


#endif /* __ASM_ARCH_COMIP_FB_H */
