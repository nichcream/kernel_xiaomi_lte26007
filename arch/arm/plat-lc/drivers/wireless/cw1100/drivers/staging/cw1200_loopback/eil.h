/*---------------------------------------------------------------------------------
*   PROJECT     : CW1200 Linux driver - using STE-UMAC
*   FILE  : eil.c
*   REVISION : 1.0
*------------------------------------------------------------------------------*/
/**
* \defgroup EIL
* \brief
* This module interfaces with the Linux Kernel 802.3 stack. Registration of network device is responsilbity
* of this module.
*/
/**
* \file
* \ingroup EIL
* \brief
*
* \date 19/09/2009
* \author Ajitpal Singh [ajitpal.singh@stericsson.com]
* \note
* Last person to change file : Ajitpal Singh
*
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

#ifndef __EIL_HEADER__
#define __EIL_HEADER__

/*********************************************************************************
*        								INCLUDE FILES
**********************************************************************************/
#include "cw1200_common.h"
#include <linux/fs.h>   /* File handling */
#ifndef USE_SPI
#include <linux/mmc/sdio_func.h>  /* SDIO funcs */
#else
#include <linux/spi/spi.h>
#endif

#define EIL_TX_TIMEOUT (2*HZ)
#define ETH_HLEN			14
#define DEV_NAME "wlan%d"

#define UMAC_TX_QUEUE_SIZE  32

/*********************************************************************************
*        						EIL Global Function prototypes
**********************************************************************************/
CW1200_STATUS_E EIL_Init(CW1200_bus_device_t *func);

void EIL_Shutdown(struct CW1200_priv * priv);

void EIL_Init_Complete(struct CW1200_priv * priv);

void EIL_Shutdown(struct CW1200_priv * priv);

struct cw1200_firmware
{
    void * firmware;
    uint32_t length;
};


#endif
