/*
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
** PURPOSE: comip machine.
**
** CHANGE HISTORY:
**
** Version	Date		Author		Description
** 1.0.0	2013-12-06	pengyimin	created
**
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/input.h>
#include <linux/mmc/host.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <plat/mfp.h>
#include <plat/uart.h>
#include <plat/hardware.h>

#include <mach/timer.h>
#include <mach/suspend.h>
#include <mach/devices.h>

#include "../generic.h"
#include "board-lc1860-mentor.h"

static struct platform_device comip_ureg_device = {
	.name = "comip-ureg",
	.id = 0,
};

static struct platform_device *devices[] __initdata = {
	&comip_ureg_device,
};

static void __init comip_init_devices(void)
{
	platform_add_devices(devices, ARRAY_SIZE(devices));
}

#if defined(CONFIG_SERIAL_COMIP) || defined(CONFIG_SERIAL_COMIP_MODULE)
/* DEBUG. */
static struct comip_uart_platform_data comip_uart3_info = {
	.flags = 0,
};

static void __init comip_init_uart(void)
{
	comip_set_uart3_info(&comip_uart3_info);
}
#else
static inline void comip_init_uart(void)
{
}
#endif

#if defined(CONFIG_I2C_COMIP) || defined(CONFIG_I2C_COMIP_MODULE)

/* Most of this EEPROM is unused, but U-Boot uses some data:
 *  - 0x7f00, 6 bytes Ethernet Address
 *  - ... newer boards may have more
 */
static struct at24_platform_data i2c0_eeprom_info = {
        .byte_len       = (2 * 1024) / 8,
        .page_size      = 16,
};

static struct at24_platform_data i2c0_eeprom2_info = {
        .byte_len       = (2 * 1024) / 8,
        .page_size      = 16,
};

static struct i2c_board_info comip_i2c0_board_info[] = {
	{
                I2C_BOARD_INFO("24c02", 0x54),
                .platform_data  = &i2c0_eeprom_info,
        },
	{
                I2C_BOARD_INFO("24c02", 0x55),
                .platform_data  = &i2c0_eeprom2_info,
        },
};

static struct comip_i2c_platform_data comip_i2c0_info = {
	.use_pio = 1,
	.flags = COMIP_I2C_FAST_MODE,
};

static void __init comip_init_i2c(void)
{
	comip_set_i2c0_info(&comip_i2c0_info);
	i2c_register_board_info(0, comip_i2c0_board_info, ARRAY_SIZE(comip_i2c0_board_info));
}
#else
static inline void comip_init_i2c(void)
{
}
#endif

#if defined(CONFIG_SPI_COMIP) || defined(CONFIG_SPI_COMIP_MODULE)

static struct spi_eeprom spi0_eeprom_info = {
	.name		= "at25320an",
	.byte_len	= 4096,
	.page_size	= 32,
	.flags		= EE_ADDR2,
};

static struct spi_board_info comip_spi0_board_info[] = {
	{
		.modalias	= "at25",
		.max_speed_hz	= 100000,
		.bus_num	= 1,
		.chip_select	= 0,
		.platform_data = &spi0_eeprom_info,
	},
};

static struct comip_spi_platform_data comip_spi0_info = {
	.num_chipselect = 1,
};

static void __init comip_init_spi(void)
{
	comip_set_spi0_info(&comip_spi0_info);
	spi_register_board_info(comip_spi0_board_info, ARRAY_SIZE(comip_spi0_board_info));
}
#else
static inline void comip_init_spi(void)
{
}
#endif

static void __init comip_init(void)
{
	comip_generic_init();
#if !defined(CONFIG_MENTOR_DEBUG)
	comip_mtc_init();
#endif
	comip_dmas_init();
	comip_suspend_init();
	comip_init_i2c();
	comip_init_spi();
	comip_init_uart();

#if defined(CONFIG_MENTOR_DEBUG)
	printk("COMIP_BOARD_LC1860_MENTOR\n");
#endif
}

static void __init comip_init_irq(void)
{
	comip_irq_init();
#if !defined(CONFIG_MENTOR_DEBUG)
	comip_gpio_init(1);
#endif
}

MACHINE_START(LC186X, "Leadcore Innopower")
	.smp = smp_ops(lc1860_smp_ops),
	.map_io = comip_map_io,
	.init_irq = comip_init_irq,
	.init_machine = comip_init,
	.init_time = comip_timer_init,
MACHINE_END
