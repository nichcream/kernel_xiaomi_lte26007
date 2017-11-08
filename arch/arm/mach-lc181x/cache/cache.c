/* arch/arm/mach-comip/irq.c
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
** Copyright (c) 2010-2019 LeadCoreTech Corp.
**
** PURPOSE: cache.
**
** CHANGE HISTORY:
**
** Version	Date		Author		Description
** 1.0.0	2012-03-06	gaojian	created
**
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <asm/hardware/cache-l2x0.h>
#include <plat/hardware.h>

void __init comip_cache_init(void)
{
#ifdef CONFIG_CACHE_L2X0
	__u32 aux_val;
	__u32 aux_mask;
	__u32 prefetch_val;
	void __iomem* p=__io_address(L2_CACHE_CTRL_BASE);
	writel(0x131, p + L2X0_TAG_LATENCY_CTRL);
	writel(0x131, p + L2X0_DATA_LATENCY_CTRL);
	
	//Dynamic clock gating enable
	//Standby mode enable
	writel(3, p + L2X0_POWER_CTRL);
	/*
	1<<30	 ; Double LF enable
	1<<29	 ; I prefetch enable
	;1<<28	  ; D prefetch enable
	1<<24	 ; Prefetch drop enable
	1<<23	 ; Incr double LF enable
	~(0x1f<<0)	; Prefetch line increment [4:0], ex: 0 = line + 1
	*/
	prefetch_val=readl(p + L2X0_PREFETCH_CTRL);
	prefetch_val |=(1<<30 | 1<<29 | 1<<28 | 1<<24 | 1<<23);
	prefetch_val &= ~0x1f;
	writel(prefetch_val, p + L2X0_PREFETCH_CTRL);
	
	/*
	;	Default values of Way size and Associativity are implementation defined (given by input pins).
	;	As a result, there should be no need to write them during this init sequence.
	;	If these bits really need to be modified, define values as required by the system.
	;
	*/
	//bit30 Early BRESP enable
	//bit25 Random replacement
        //bit22 Shared attribute override enable
	//bit0 Full line of zero enable
	aux_val = 1<<30 | 0<<25 | 1<<22 | 1<<0;
	aux_mask = ~(1<<30 | 1<<25 | 1<<22 | 1<<0);
	l2x0_init(p, aux_val, aux_mask);
#endif
}

