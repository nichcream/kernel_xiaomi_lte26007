/*
* ST-Ericsson ETF driver
*
* Copyright (c) ST-Ericsson SA 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
/****************************************************************************
* sdio_wrapper.c
*
* This module interfaces with the Linux Kernel MMC/SDIO stack.
*
****************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include <linux/wait.h>
#include <plat/ste-cw1200.h>

#include "driver.h"
#include "st90tds_if.h"
#include "ETF_Api.h"
#include "cw1200_common.h"
#include "sbus_wrapper.h"
#include "debug.h"

MODULE_LICENSE("GPL");

int drv_etf_ioctl_enter(void);
void drv_etf_ioctl_release(void);

/*
To enable logs : insmod testsdio.ko debug_level=<debug_level_num>
where:
	debug_level_num=0; DBG_NONE
	debug_level_num=1; DBG_SBUS
	debug_level_num=2; DBG_MESSAGE
	debug_level_num=4; DBG_FUNC
	debug_level_num=8; DBG_ERROR/DBG_ALWAYS

Note: If driver is inserted without providing the debug_level argument, logs
shown will be of level DBG_ERROR/DBG_ALWAYS
*/
int debug_level = 0;
module_param(debug_level, int, 0644);

/*
To run testsdio driver for RETF/PETF: insmod testsdio.ko <etf_flavour=0>
To run testsdio driver for UETF : insmod testsdio.ko etf_flavour=1
Note: By default value of etf_flavour is 0 so just insmod .ko will
load testsdio driver for RETF/PETF
*/
int etf_flavour = 0;
module_param(etf_flavour, int, 0644);

struct sdio_func *g_func;
struct mmc_host *host;
struct work_struct init_work;
const struct cw1200_platform_data *wpd;
static const struct sdio_device_id if_sdio_ids[] = {
	{ SDIO_DEVICE(SDIO_ANY_ID, SDIO_ANY_ID) },
	{ /* end: all zeroes */ },
};

void Sh_adap_init(struct sdio_func *func);
extern int ste_wifi_set_carddetect(int on); //05022013

int cw1200_detect_card(void)
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

	BUG_ON(!mmc->class_dev.class);
	class_dev_iter_init(&iter, mmc->class_dev.class, NULL, NULL);
	for (;;) {
		dev = class_dev_iter_next(&iter);
		if (!dev) {
			DEBUG(DBG_ERROR, "ERROR: cw1200: %s is not found.\n",
					wpd->mmc_id);
			break;
		} else {
			struct mmc_host *host = container_of(dev,
				struct mmc_host, class_dev);

			DEBUG(DBG_SBUS, "%s:%d, wpd->mmc_id:%s\n",
				__func__, __LINE__, wpd->mmc_id);
			DEBUG(DBG_SBUS,
				"%s: dev_name(&host->class_dev):%s\n",
				__func__, dev_name(&host->class_dev));

			if (dev_name(&host->class_dev) &&
					strcmp(dev_name(&host->class_dev),
						wpd->mmc_id))
				continue;

			mmc_detect_change(host, 10);
			break;
		}
	}
	mmc_free_host(mmc);
	return 0;
}

int sbus_memcpy_fromio(CW1200_bus_device_t *func, void *dst,
		unsigned int addr, int count)
{
	return sdio_memcpy_fromio(func, dst, addr, count);
}

int sbus_memcpy_toio(CW1200_bus_device_t *func, unsigned int addr,
		void *src, int count)
{
	return sdio_memcpy_toio(func, addr, src, count);
}

/* Probe Function to be called by SDIO stack when device is discovered */
static int cw1200_sdio_probe(struct sdio_func *func,
		const struct sdio_device_id *id)
{
	g_func = func;
	Sh_adap_init(func);
	DEBUG(DBG_SBUS, "Probe called \n");
	g_enter = TRUE;
	DEBUG(DBG_SBUS, "waking up start procedure\n");
	detect_waitq_cond_flag = 1;
	wake_up(&detect_waitq);/* wake to proceed start procedure */
	DEBUG(DBG_MESSAGE, "woke detect waitq\n");
	return 0;
}

/* Disconnect Function to be called by SDIO stack when device is
disconnected */
static void cw1200_sdio_disconnect(struct sdio_func *func)
{
	return;
}

static struct sdio_driver sdio_driver = {
	.name = "cw1200_sdio",
	.id_table = if_sdio_ids,
	.probe = cw1200_sdio_probe,
	.remove = cw1200_sdio_disconnect,
};

void async_exit(void)
{
	DEBUG(DBG_MESSAGE, "Detect Change called \n");
	mdelay(10);
	gpio_set_value(wpd->reset->start, 0);
	DEBUG(DBG_MESSAGE, "%s: set GPIO pin to LOW\n", __func__);
	mdelay(10);
	ste_wifi_set_carddetect(0);//05022013
	cw1200_detect_card();
	DEBUG(DBG_SBUS, "WLAN chip powered down \n");
	mdelay(100); /*Debug delay*/
}

/* Init Module function -> Called by insmod */
static int __init sbus_sdio_init (void)
{
	if (debug_level > 15)
		debug_level = 15;
	else if (debug_level < 8)
		debug_level += DBG_ALWAYS;

	if (debug_level != 8) {
		DEBUG(DBG_SBUS,    "debug_level set to DBG_SBUS\n");
		DEBUG(DBG_MESSAGE, "debug_level set to DBG_MESSAGE\n");
		DEBUG(DBG_FUNC,    "debug_level set to DBG_FUNC\n");
		DEBUG(DBG_ERROR,   "debug_level set to DBG_ERROR\n");
		DEBUG(DBG_ALWAYS,  "debug_level set to DBG_ALWAYS\n");
	}

	DEBUG(DBG_ALWAYS, "ETF driver version: ETF_DRIVER_v002\n");

	if (etf_flavour == 1) {
		DEBUG(DBG_ALWAYS, "UETF running...\n");
	} else {
		DEBUG(DBG_ALWAYS, "RETF/PETF running...\n");
	}

	sdio_register_driver(&sdio_driver);
	drv_etf_ioctl_enter();
	return 0;
}

static void __exit sbus_sdio_exit (void)
{
	drv_etf_ioctl_release();

	/*If func is null then probe was not called */
	if (NULL == g_func) {
		sdio_unregister_driver(&sdio_driver);
		return;
	}

	sdio_unregister_driver(&sdio_driver);
	DEBUG(DBG_MESSAGE, "Detect Change called \n");
	mdelay(10);
	gpio_set_value(wpd->reset->start, 0);
	mdelay(10);
	gpio_free(wpd->reset->start);
	DEBUG(DBG_ALWAYS, "Unloaded ETF driver\n");
}

module_init(sbus_sdio_init);
module_exit(sbus_sdio_exit);
