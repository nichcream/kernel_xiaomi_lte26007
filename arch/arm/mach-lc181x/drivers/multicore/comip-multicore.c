/* arch/arm/mach-comip/multicore-mutex.c
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
** Copyright (c) 2012  LeadCoreTech Corp.
**
**	PURPOSE:	multicore mutex
**
**	CHANGE HISTORY:
**
**	Version		Date			Author			Description
**	1.0.0		2011-11-25		gaobing			created
**
**
**
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <asm/io.h>

#include <plat/hardware.h>
#include <mach/comip-multicore.h>

#define COMIP_MTC_TIMEOUT		50 /* us */

#define COMIP_MTC_WRITE_REG32(value, reg) \
	writel(value, io_p2v(reg))

#define COMIP_MTC_READ_BIT(bit, reg) \
	(readl(io_p2v(reg)) & (1 << (bit)))

#define COMIP_MTC_SET_BIT(bit, reg) \
	writel(readl(io_p2v(reg)) | (1 << (bit)), io_p2v(reg))

#define COMIP_MTC_CLEAR_BIT(bit, reg) \
	writel(readl(io_p2v(reg)) & ~(1 << (bit)), io_p2v(reg))

void comip_mtc_down(int type)
{
	int timeout = COMIP_MTC_TIMEOUT;

	if (type >= MTC_TYPE_MAX) {
		printk(KERN_DEBUG "%s type error:%d\n", __func__, type);
		return;
	}

	/* send req */
	COMIP_MTC_SET_BIT(type * 4, DDR_PWR_CPU0_ARBIT_REQ);

	/* wait for req */
	while((timeout--) && !(COMIP_MTC_READ_BIT(type, DDR_PWR_CPU0_ARBIT_ACK)))
		udelay(1);

	/* timeout handle: clear all req */
	if (timeout <= 0) {
		printk(KERN_ERR "%s: timeout\n", __func__);
		COMIP_MTC_CLEAR_BIT(type * 4, DDR_PWR_CPU1_ARBIT_REQ);
		COMIP_MTC_CLEAR_BIT(type * 4, DDR_PWR_CPU2_ARBIT_REQ);
		COMIP_MTC_CLEAR_BIT(type * 4, DDR_PWR_CPU3_ARBIT_REQ);
		COMIP_MTC_CLEAR_BIT(type * 4, DDR_PWR_CPU4_ARBIT_REQ);
	}
}
EXPORT_SYMBOL(comip_mtc_down);

void comip_mtc_up(int type)
{
	if (type >= MTC_TYPE_MAX) {
		printk(KERN_DEBUG "%s type error:%d\n", __func__, type);
		return;
	}

	/* Clear req. */
	COMIP_MTC_CLEAR_BIT(type * 4, DDR_PWR_CPU0_ARBIT_REQ);
}
EXPORT_SYMBOL(comip_mtc_up);

void __init comip_mtc_init(void)
{
	/* a9 Priority for arbiter 0/1: 1 */
	COMIP_MTC_WRITE_REG32(0x22, DDR_PWR_CPU0_ARBIT_REQ);
	COMIP_MTC_WRITE_REG32(0x22, DDR_PWR_CPU1_ARBIT_REQ);

	/* cp arm Priority for arbiter 0/1: 2 */
	COMIP_MTC_WRITE_REG32(0x44, DDR_PWR_CPU2_ARBIT_REQ);

	/* zsp0 Priority for arbiter 0/1: 4 */
	COMIP_MTC_WRITE_REG32(0x88, DDR_PWR_CPU3_ARBIT_REQ);

	/* zsp1 Priority for arbiter 0/1: 3 */
	COMIP_MTC_WRITE_REG32(0x66, DDR_PWR_CPU4_ARBIT_REQ);

	/* Enable all arbit. */
	COMIP_MTC_WRITE_REG32(0x3f, DDR_PWR_CPU_ARBIT_EN);

	/*ap priority is lower than cp*/
	writel(0x01, io_p2v(DDR_PWR_ICMCTL));
}

