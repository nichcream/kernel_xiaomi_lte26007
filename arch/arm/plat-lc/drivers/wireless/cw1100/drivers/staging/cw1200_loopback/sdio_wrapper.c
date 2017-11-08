/*******************************************************************************
 * Copyright (c) 2008 STMicroelectronics Ltd
 *
 * Copyright ST-Ericsson, 2009 – All rights reserved.
 *
 * This information, source code and any compilation or derivative thereof are
 * the proprietary information of ST-Ericsson and/or its licensors and are
 * confidential in nature. Under no circumstances is this software to be exposed
 * to or placed under an Open Source License of any type without the expressed
 * written permission of ST-Ericsson.
 *
 * Although care has been taken to ensure the accuracy of the information and
 * the software, ST-Ericsson assumes no responsibility therefore.THE INFORMATION
 * AND SOFTWARE ARE PROVIDED "AS IS" AND "AS AVAILABLE".ST-Ericsson MAKES NO
 * REPRESENTATIONS OR WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO WARRANTIES OR MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS, WITH
 * RESPECT TO THE INFORMATION AND SOFTWARE PROVIDED;
 *
 ******************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>

#include "cw1200_common.h"
#include "sbus_wrapper.h"
#include "eil.h"

extern int ste_wifi_set_carddetect(int on);

extern void u8500_sdio_detect_card(void);

static const struct sdio_device_id if_sdio_ids[] = {
	{ SDIO_DEVICE(SDIO_ANY_ID, SDIO_ANY_ID) },
	{ /* end: all zeroes */		        },
};

int sbus_memcpy_fromio(CW1200_bus_device_t *func, void *dst, unsigned int addr, int count)
{
	return sdio_memcpy_fromio(func, dst, addr, count);
}

int sbus_memcpy_toio(CW1200_bus_device_t *func, unsigned int addr, void *src, int count)
{
	return sdio_memcpy_toio(func, addr, src, count);
}

/*  Probe Function to be called by SDIO stack when device is discovered */
static int cw1200_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	CW1200_STATUS_E status = SUCCESS;
	DEBUG(DBG_SBUS,"Probe called \n");
	status = EIL_Init(func);
	DEBUG(DBG_SBUS,"EIL_Init() return status [%d] \n",status);
	return 0;
}

/*  Disconnect Function to be called by SDIO stack when device is disconnected */
static void cw1200_sdio_disconnect(struct sdio_func *func)
{
	struct CW1200_priv * priv = NULL;

	priv = sdio_get_drvdata(func);
	EIL_Shutdown(priv);
}

static struct sdio_driver sdio_driver = {
	.name = "stlc9000_sdio",
	.id_table	= if_sdio_ids,
	.probe = cw1200_sdio_probe,
	.remove = cw1200_sdio_disconnect,
};

static void power_on(void)
{
	gpio_direction_output(gpio_wlan_enable(), 1);

	gpio_set_value(gpio_wlan_enable(), 0);
	mdelay(100);

	gpio_set_value(gpio_wlan_enable(), 1);
	mdelay(10);

printk("ste_wifi_set_carddetect on \n");
	ste_wifi_set_carddetect(1);
}

static void power_off(void)
{
	gpio_direction_output(gpio_wlan_enable(), 1);

	gpio_set_value(gpio_wlan_enable(), 0);
	mdelay(100);
}

static int cw1200_detect_card(void)
{
	/* HACK!!!
	 * Rely on mmc->class_dev.class set in mmc_alloc_host
	 * Tricky part: a new mmc hook is being (temporary) created
	 * to discover mmc_host class.
	 * Do you know more elegant way how to enumerate mmc_hosts?
	 */

	struct mmc_host *mmc = NULL;
	struct class_dev_iter iter;
	struct device *dev;

	mmc = mmc_alloc_host(0, NULL);
	if (!mmc)
		return -ENOMEM;

	printk("About to search for changes for dev %s\n", MMC_DEV_ID);

	BUG_ON(!mmc->class_dev.class);
	class_dev_iter_init(&iter, mmc->class_dev.class, NULL, NULL);
	for (;;) {
		dev = class_dev_iter_next(&iter);
		if (!dev) {
			printk(KERN_ERR "cw1200: %s is not found.\n", MMC_DEV_ID);
			break;
		} else {
			struct mmc_host *host = container_of(dev,
							     struct mmc_host, class_dev);

			if (dev_name(&host->class_dev) &&
			    strcmp(dev_name(&host->class_dev),
				   MMC_DEV_ID))
				continue;

			printk("Calling mmc_detect_change for %s\n", dev_name(&host->class_dev));
			mmc_detect_change(host, 10);
			break;
		}
	}
	mmc_free_host(mmc);
	return 0;
}


/* Init Module function -> Called by insmod */
static int __init sbus_sdio_init (void)
{
	int res;
	int32_t status = 0;

	DEBUG(DBG_SBUS,"STE WLAN- Driver Init Module Called \n");
	res = sdio_register_driver (&sdio_driver);

	/* Request WLAN Enable GPIO */
	status = gpio_request(gpio_wlan_enable(), "sdio_init");
	if (status) {
		DEBUG(DBG_SBUS , "INIT : Unable to request gpio_wlan_enable \n");
		return status;
	}

	DEBUG(DBG_SBUS,"Doing WLAN power up sequence...\n");
	power_on();

	DEBUG(DBG_SBUS,"Calling detect card...\n");
	//   u8500_sdio_detect_card();
	cw1200_detect_card();

	return res;
}

/* Called at Driver Unloading */
static void __exit sbus_sdio_exit (void)
{
	DEBUG(DBG_SBUS,"Unregistering SDIO driver...\n");
	sdio_unregister_driver (&sdio_driver);

	DEBUG(DBG_SBUS,"Powering off CW1200...\n");
	power_off();

	DEBUG(DBG_SBUS,"Calling detect card...\n");
	cw1200_detect_card();

	gpio_free(gpio_wlan_enable());

	DEBUG(DBG_SBUS,"Unloaded SBUS Test driver, \n");
}


module_init(sbus_sdio_init);
module_exit(sbus_sdio_exit);
