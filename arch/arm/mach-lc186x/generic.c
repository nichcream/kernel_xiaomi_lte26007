/*
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 1999-2008  LeadCoreTech Corp.
**
**  PURPOSE:		This files contains the driver GPIO controller.
**
**  CHANGE HISTORY:
**
**  Version		Date			Author		Description
**  0.1.0		2013-11-22		fanjiangang		created
**
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>

#include <asm/mach/map.h>
#include <asm/pgtable.h>
#include <asm/system_misc.h>
#include <plat/hardware.h>
#include <plat/memory.h>
#include <plat/comip-memory.h>
#include <plat/reset.h>

#include "generic.h"

/*
 * Leadcore COMIP internal register mapping.
 *
 * Note : virtual 0xfffe0000-0xffffffff is reserved for the vector table
 *         and cache flush area.
 */
static struct map_desc standard_io_desc[] __initdata = {
	/* AP_SW0 */
	{
		.virtual = IO_ADDRESS(0xA0A00000),
		.pfn     = __phys_to_pfn(0xA0A00000),
		.length  = 0x00100000,
		.type    = MT_DEVICE
	},

	/* AP_Peri_SW0 */
	{
		.virtual = IO_ADDRESS(0xA0000000),
		.pfn     = __phys_to_pfn(0xA0000000),
		.length	 = 0x00300000,
		.type    = MT_DEVICE
	},

	/* AP_Peri_SW1 */
	{
		.virtual = IO_ADDRESS(0xA0300000),
		.pfn	 = __phys_to_pfn(0xA0300000),
		.length  = 0x00100000,
		.type	 = MT_DEVICE
	},

	/* AP_Peri_SW2 */
	{
		.virtual = IO_ADDRESS(0xA0400000),
		.pfn	 = __phys_to_pfn(0xA0400000),
		.length  = 0x00100000,
		.type	 = MT_DEVICE
	},

	/* AP_Peri_SW3 */
	{
		.virtual = IO_ADDRESS(0xA0500000),
		.pfn	 = __phys_to_pfn(0xA0500000),
		.length  = 0x00100000,
		.type	 = MT_DEVICE
	},

	/* AP_Peri_SW4. */
	{
		.virtual = IO_ADDRESS(0xA0600000),
		.pfn	 = __phys_to_pfn(0xA0600000),
		.length  = 0x00200000,
		.type	 = MT_DEVICE
	},

	/* AP_Peri_SW5. */
	{
		.virtual = IO_ADDRESS(0xA0800000),
		.pfn	 = __phys_to_pfn(0xA0800000),
		.length  = 0x00200000,
		.type	 = MT_DEVICE
	},

	/* TOP. */
	{
		.virtual = IO_ADDRESS(0xE1000000),
		.pfn	 = __phys_to_pfn(0xE1000000),
		.length  = 0x00200000,
		.type	 = MT_DEVICE
	},
};

void __init comip_map_io(void)
{
	//init_consistent_dma_size(CONSISTENT_DMA_SIZE);

	iotable_init(standard_io_desc, ARRAY_SIZE(standard_io_desc));

	comip_clks_init();
}

static void comip_poweroff(void)
{
	__raw_writel(0, io_p2v(AP_PWR_PWEN_CTL));
}

#if defined(CONFIG_COMIP_PMIC_REBOOT)
static void comip_restart(char mode, const char *cmd)
{
	if (pm_power_off)
		pm_power_off();
}
#else
static void comip_restart(char mode, const char *cmd)
{
	comip_reset("AP");
	while (1);
}
#endif

void __init comip_generic_init(void)
{
	pm_power_off = comip_poweroff;
	arm_pm_restart = comip_restart;
}
