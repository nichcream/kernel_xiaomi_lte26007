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
* sbus_wrapper.h
*
* This module contains declarations for functions that interface with the
* Linux Kernel MMC/SDIO stack.
*
****************************************************************************/

#ifndef _SBUS_WRAPPER_H
#define _SBUS_WRAPPER_H

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>

int sbus_memcpy_fromio(CW1200_bus_device_t *func, void *dst,
		unsigned int addr, int count);
int sbus_memcpy_toio(CW1200_bus_device_t *func, unsigned int addr,
		void *src, int count);

#endif /* _SBUS_WRAPPER_H */
