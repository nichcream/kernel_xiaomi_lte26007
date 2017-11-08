/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License terms:  GNU General Public License (GPL), version 2
 *
 * U8500 board specific cw1200 (WLAN device) initialization.
 *
 * Author: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 *
 */

#ifndef __PLAT_STE_WLAN_H__
#define __PLAT_STE_WLAN_H__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <linux/regulator/consumer.h>
#include <mach/irqs.h>
#include <linux/clk.h>
#include <plat/ste-cw1200.h>
//#include <mach/gpio.h>  // changed by si-plaza 2012.8.8
//#include <mach/irqs.h>	// changed by si-plaza 2012.8.8
#include <plat/mfp.h>		// add 2012.8.9

static int cw12xx_wlan_init(void);
#define CW12XX_SYS_CLK_ID	  "sys_clk_out" //"sdmmc_clk"//
#endif
