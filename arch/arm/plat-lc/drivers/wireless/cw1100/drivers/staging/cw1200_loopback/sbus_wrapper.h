/**************************************************************************
***************************************************************************
**
** COMPONENT:       SBUS
**
** MODULE:          sbus_wrapper.h
**
** VERSION:         1.0
**
** DATED:           14 Nov 2009
**
** AUTHOR:          Harald Unander
**
** DESCRIPTION:     Conversion of SBUS commands to SPI/SDIO
**
** LAST MODIFIED BY:	Dmitry Tarnyagin
** CHANGE LOG :
** 24 Sep 2009 - Initial version checked in
** 14 Nov 2009 - Refactored for unified SPI/SDIO support
**
*******************************************************************************
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


#ifndef __SBUS_WRAPPER_HEADER__
#define __SBUS_WRAPPER_HEADER__

#ifndef USE_SPI
	#include <linux/mmc/sdio_func.h>
#else
	#include <linux/spi/spi.h>
#endif

int sbus_memcpy_fromio(CW1200_bus_device_t *func, void *dst, unsigned int addr, int count);
int sbus_memcpy_toio(CW1200_bus_device_t *func, unsigned int addr, void *src, int count);

#endif
