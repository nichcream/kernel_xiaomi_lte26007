/* arch/arm/mach-comip/Generic.c
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
**  0.1.0		2011-11-22		gaojian		created
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
	{	/* AP_SEC_RAM */
		.virtual	= IO_ADDRESS(0x9FF80000),
		.pfn		= __phys_to_pfn(0x9FF80000),
		.length 	= 0x00020000,
		.type		= MT_MEMORY_NONCACHED
	}, {	/* AP DMA */
		.virtual	= IO_ADDRESS(0xA0000000),
		.pfn		= __phys_to_pfn(0xA0000000),
		.length		= 0x00005000,
		.type		= MT_DEVICE
	}, {	/* LCDC */
		.virtual	= IO_ADDRESS(0xA0008000),
		.pfn		= __phys_to_pfn(0xA0008000),
		.length		= 0x00002000,
		.type		= MT_DEVICE
	}, {	/* MO_RAM */
		.virtual	= IO_ADDRESS(0xA0040000),
		.pfn		= __phys_to_pfn(0xA0040000),
		.length		= 0x00020000,
		.type		= MT_DEVICE
	}, {	/* CTL */
		.virtual	= IO_ADDRESS(0xA0060000),
		.pfn		= __phys_to_pfn(0xA0060000),
		.length		= 0x00001000,
		.type		= MT_DEVICE
	}, {	/* NFC */
		.virtual	= IO_ADDRESS(0xA0061000),
		.pfn		= __phys_to_pfn(0xA0061000),
		.length		= 0x00001000,
		.type		= MT_DEVICE
	}, {	/* HDMI */
		.virtual	= IO_ADDRESS(0xA00A0000),
		.pfn		= __phys_to_pfn(0xA00A0000),
		.length		= 0x00008000,
		.type		= MT_DEVICE
	}, {	/* HPI Region0*/
		.virtual	= IO_ADDRESS(0xA00B0000),
		.pfn		= __phys_to_pfn(0xA00B0000),
		.length		= 0x00001000,
		.type		= MT_DEVICE
	}, {	/* HPI Region1 */
		.virtual	= IO_ADDRESS(0xA00C0000),
		.pfn		= __phys_to_pfn(0xA00C0000),
		.length		= 0x00040000,
		.type		= MT_DEVICE
	}, {	/* CTL_APB */
		.virtual	= IO_ADDRESS(0xA0108000),
		.pfn		= __phys_to_pfn(0xA0108000),
		.length		= 0x00018000,
		.type		= MT_DEVICE
	}, {	/* DATA_APB */
		.virtual	= IO_ADDRESS(0xA0140000),
		.pfn		= __phys_to_pfn(0xA0140000),
		.length 	= 0x0000A000,
		.type		= MT_DEVICE
	}, {	/* ISP */
		.virtual	= IO_ADDRESS(0xA0200000),
		.pfn		= __phys_to_pfn(0xA0200000),
		.length		= 0x00080000,
		.type		= MT_DEVICE
	}, {	/* USB */
		.virtual	= IO_ADDRESS(0xA0300000),
		.pfn		= __phys_to_pfn(0xA0300000),
		.length		= 0x000C0000,
		.type		= MT_DEVICE
	}, {	/* COM_APB */
		.virtual	= IO_ADDRESS(0xA1000000),
		.pfn		= __phys_to_pfn(0xA1000000),
		.length		= 0x00008000,
		.type		= MT_DEVICE
	}, {	/* Secure_APB */
		.virtual	= IO_ADDRESS(0xA3000000),
		.pfn		= __phys_to_pfn(0xA3000000),
		.length		= 0x00009000,
		.type		= MT_DEVICE
	}, {	/* SECURITY */
		.virtual	= IO_ADDRESS(0xA4002000),
		.pfn		= __phys_to_pfn(0xA4002000),
		.length		= 0x00001000,
		.type		= MT_DEVICE
	}, {	/* DDR_AXI_GPV */
		.virtual	= IO_ADDRESS(0xA4100000),
		.pfn		= __phys_to_pfn(0xA4100000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}, {	/* PERI_AXI_GPV */
		.virtual	= IO_ADDRESS(0xA4200000),
		.pfn		= __phys_to_pfn(0xA4200000),
		.length		= 0x00200000,
		.type		= MT_DEVICE
	}, {	/* A9_Private */
		.virtual	= IO_ADDRESS(0xA6000000),
		.pfn		= __phys_to_pfn(0xA6000000),
		.length		= 0x00003000,
		.type		= MT_DEVICE
	}
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
