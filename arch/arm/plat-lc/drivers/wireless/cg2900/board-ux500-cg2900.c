/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Author: Par-Gunnar Hjalmdahl <par-gunnar.p.hjalmdahl@stericsson.com>
 * Author: Hemant Gupta <hemant.gupta@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>
#include "include/cg2900.h"
#include "devices-cg2900.h"
#include "board_non_ste.h"


enum cg2900_gpio_pull_sleep generic_cg2900_sleep_gpio[23] = {
	CG2900_NO_PULL,		/* GPIO 0:  PTA_CONFX */
	CG2900_PULL_DN,		/* GPIO 1:  PTA_STATUS */
	CG2900_NO_PULL,		/* GPIO 2:  UART_CTSN */
	CG2900_PULL_UP,		/* GPIO 3:  UART_RTSN */
	CG2900_PULL_UP,		/* GPIO 4:  UART_TXD */
	CG2900_NO_PULL,		/* GPIO 5:  UART_RXD */
	CG2900_PULL_DN,		/* GPIO 6:  IOM_DOUT */
	CG2900_NO_PULL,		/* GPIO 7:  IOM_FSC */
	CG2900_NO_PULL,		/* GPIO 8:  IOM_CLK */
	CG2900_NO_PULL,		/* GPIO 9:  IOM_DIN */
	CG2900_PULL_DN,		/* GPIO 10: PWR_REQ */
	CG2900_PULL_DN,		/* GPIO 11: HOST_WAKEUP */
	CG2900_PULL_DN,		/* GPIO 12: IIS_DOUT */
	CG2900_NO_PULL,		/* GPIO 13: IIS_WS */
	CG2900_NO_PULL,		/* GPIO 14: IIS_CLK */
	CG2900_NO_PULL,		/* GPIO 15: IIS_DIN */
	CG2900_PULL_DN,		/* GPIO 16: PTA_FREQ */
	CG2900_PULL_DN,		/* GPIO 17: PTA_RF_ACTIVE */
	CG2900_NO_PULL,		/* GPIO 18: NotConnected (J6428) */
	CG2900_NO_PULL,		/* GPIO 19: EXT_DUTY_CYCLE */
	CG2900_NO_PULL,		/* GPIO 20: EXT_FRM_SYNCH */
	CG2900_PULL_UP,		/* GPIO 21: BT_ANT_SEL_CLK */
	CG2900_PULL_UP,		/* GPIO 22: BT_ANT_SEL_DATA */
};

static struct platform_device generic_cg2900_device = {
	.name = "cg2900",
};

static struct platform_device generic_cg2900_chip_device = {
	.name = "cg2900-chip",
	.dev = {
		.parent = &generic_cg2900_device.dev,
	},
};

static struct platform_device generic_stlc2690_chip_device = {
	.name = "stlc2690-chip",
	.dev = {
		.parent = &generic_cg2900_device.dev,
	},
};

static struct cg2900_platform_data generic_cg2900_test_platform_data = {
	.bus = HCI_VIRTUAL,
	.gpio_sleep = generic_cg2900_sleep_gpio,
};

static struct platform_device generic_cg2900_test_device = {
	.name = "cg2900-test",
	.dev = {
		.parent = &generic_cg2900_device.dev,
		.platform_data = &generic_cg2900_test_platform_data,
	},
};

static struct resource cg2900_uart_resources[] = {
	{
		.start = CG29XX_BT_CTS_IRQ,
		.end = CG29XX_BT_CTS_IRQ,
		.flags = IORESOURCE_IRQ,
		.name = "cts_irq",
	},
};


static struct cg2900_platform_data generic_cg2900_uart_platform_data = {
	.bus = HCI_UART,
	.gpio_sleep = generic_cg2900_sleep_gpio,
	.uart = {
		.n_uart_gpios = 4,
	},
};

static struct platform_device generic_cg2900_uart_device = {
	.name = "cg2900-uart",
	.dev = {
		.platform_data = &generic_cg2900_uart_platform_data,
		.parent = &generic_cg2900_device.dev,
	},
};

static bool mach_supported(void)
{
	return true;
}

static void set_pdata_gpios(struct cg2900_platform_data *pdata,
			    int gbf_ena_reset, int bt_enable,
			    int cts_gpio, int pmu_en)
{
	pdata->gpios.gbf_ena_reset = gbf_ena_reset;
	pdata->gpios.bt_enable = bt_enable;
	pdata->gpios.cts_gpio = cts_gpio;
	pdata->gpios.pmu_en = pmu_en;
}

static int __init board_cg2900_init(void)
{
	int err;

	if (!mach_supported())
		return 0;

	dcg2900_init_platdata(&generic_cg2900_test_platform_data);
	/*generic_cg2900_uart_platform_data.uart.uart_enabled =
		generic_cg2900_uart_enabled;
	generic_cg2900_uart_platform_data.uart.uart_disabled =
		generic_cg2900_uart_disabled;
	generic_cg2900_uart_platform_data.regulator_id = "gbf_1v8";
	generic_cg2900_uart_platform_data.regulator_wlan_id = NULL;*/

	/* Set -1 as default (i.e. GPIO not used) */
	set_pdata_gpios(&generic_cg2900_uart_platform_data, -1, -1, -1, -1);

	dcg2900_init_platdata(&generic_cg2900_uart_platform_data);

	generic_cg2900_uart_device.num_resources =
			ARRAY_SIZE(cg2900_uart_resources);
	generic_cg2900_uart_device.resource =
			cg2900_uart_resources;


         set_pdata_gpios(&generic_cg2900_uart_platform_data,
					CG29XX_GPIO_GBF_RESET,
					-1,
					CG29XX_BT_CTS_GPIO,
					CG29XX_GPIO_PMU_EN);

	err = platform_device_register(&generic_cg2900_device);
	if (err)
		return err;
	err = platform_device_register(&generic_cg2900_uart_device);
	if (err)
		return err;
	err = platform_device_register(&generic_cg2900_test_device);
	if (err)
		return err;
	err = platform_device_register(&generic_cg2900_chip_device);
	if (err)
		return err;
	err = platform_device_register(&generic_stlc2690_chip_device);
	if (err)
		return err;

	dev_info(&generic_cg2900_device.dev, "CG2900 initialized\n");
	return 0;
}

static void __exit board_cg2900_exit(void)
{
	if (!mach_supported())
		return;

	platform_device_unregister(&generic_stlc2690_chip_device);
	platform_device_unregister(&generic_cg2900_chip_device);
	platform_device_unregister(&generic_cg2900_test_device);
	platform_device_unregister(&generic_cg2900_uart_device);
	platform_device_unregister(&generic_cg2900_device);

	dev_info(&generic_cg2900_device.dev, "CG2900 removed\n");
}

module_init(board_cg2900_init);
module_exit(board_cg2900_exit);
