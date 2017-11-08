/*---------------------------------------------------------------------------------
 *   PROJECT     : CW1200 Linux driver - using STE-UMAC
 *   FILE  : cw1200_common.h
 *   REVISION : 1.0
 *------------------------------------------------------------------------------*/
/**
 * \defgroup COMMON
 * \brief Contains common declarations used by all the driver sub-blocks.
 */
/**
 * \file
 * \ingroup COMMON
 * \brief
 *
 * \date 19/09/2009
 * \author Ajitpal Singh [ajitpal.singh@stericsson.com]
 * \note
 * Last person to change file : Ajitpal Singh
 *******************************************************************************
 * Copyright (c) 2008 STMicroelectronics Ltd
 *
 * Copyright ST-Ericsson, 2009 ?All rights reserved.
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


#ifndef __CW1200_COMMON__
#define  __CW1200_COMMON__

/*********************************************************************************
 *        								INCLUDE FILES
 **********************************************************************************/
#include "UMI_Api.h"  /* UMAC API declaration file */
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <net/cfg80211.h>

/*********************************************************************************
 *        								MACROS
 **********************************************************************************/
#define FALSE	0
#define TRUE		1
#define FIRMWARE_FILE  "/system/etc/firmware/wsm_test.bin"
#define FIRMWARE_FILE_ALT1  "/mnt/wsm.bin"
//#define SDD_FILE "/root/sdd.bin"

#define UMAC_NOTSTARTED 0
#define UMAC_STARTED  1

//#define gpio_wlan_enable() (machine_is_hrefv60() ? (85) : (215))
//#define gpio_wlan_irq() (machine_is_hrefv60() ? (4) : (216))
// Only support HREF > v60


//jogal
#define gpio_wlan_irq() 		0x7D 	//gpio_to_irq(mfp_to_gpio(MFP_PIN_GPIO8))
#define gpio_wlan_enable()      0x11	//mfp_to_gpio(MFP_PIN_GPIO6) 
#define MMC_DEV_ID "mmc2"

//jogal


#ifndef USE_SPI
typedef struct sdio_func  CW1200_bus_device_t;
#else
typedef struct spi_device CW1200_bus_device_t;
#endif

/* Find a better way to store MAC Address */
//static uint8_t CW1200_MACADDR[6] =  {0x00,0x80,0xe1,0x11,0x22,0x50};

#define    MACADDR_LEN     6

#define DOWNLOAD_BLOCK_SIZE          ( 512 ) 

#ifdef MOP_WORKAROUND 
   #define SDIO_BLOCK_SIZE    512
#else
   #define SDIO_BLOCK_SIZE    512  
#endif

typedef enum CW1200_STATUS
{
	SUCCESS=0,
	ERROR,
	ERROR_SDIO,
	ERROR_INVALID_PARAMETERS,
	ERROR_BUF_UNALIGNED,
	WAIT_INIT_VAL,
} CW1200_STATUS_E;


struct CW1200_priv
{
	UMI_HANDLE  umac_handle;
	UMI_HANDLE  lower_handle;
	uint32_t umac_status;
	struct net_device * netdev;
	struct net_device_stats stats;
	uint32_t  Kernel_Queue_Started;
	struct workqueue_struct * umac_WQ;
	struct work_struct umac_work;
	uint32_t tx_count;

	/*SBUS related declaraions*/
	atomic_t 	Interrupt_From_Device;
	atomic_t     Umac_Tx;
	struct workqueue_struct * sbus_WQ;
	int32_t sdio_wr_buf_num_qmode;
	int32_t sdio_rd_buf_num_qmode;
	CW1200_bus_device_t *func;
	struct work_struct sbus_work;
	uint32_t max_size_supp;
	uint32_t max_num_buffs_supp;
	uint32_t num_unprocessed_buffs_in_device;
	uint16_t in_seq_num;
	uint16_t out_seq_num;
	struct sk_buff * passed_skb;
	uint32_t hw_type;
	/* CIL related declarations */
	struct semaphore cil_sem;
	wait_queue_head_t  cil_wait;
	void * get_buff_pointer ;
	uint32_t get_buff_len;
	uint32_t set_status;
	uint32_t time_buf[999];
	//atomic_t buf_index;
	uint32_t buf_index;
	struct cfg80211_scan_request *request;
#ifdef WORKAROUND
	struct semaphore sem;
#endif
};

#define PRINT(args...)  printk(args);

/* DEBUG MACROS */
#define DBG_NONE		0x00000000
#define DBG_SBUS		0x00000001
#define DBG_EIL            	0x00000002
#define DBG_CIL              	0x00000004
#define DBG_ERROR		0x00000008
#define DBG_MESSAGE		0x00000010


#define DEBUG_LEVEL        	(DBG_ERROR | DBG_CIL | DBG_EIL| DBG_MESSAGE | DBG_SBUS)

#define K_DEBUG( f, m, args... )    do{ if(( f & m )) printk( args ); }while(0)

#if DEBUG_LEVEL > DBG_NONE
#define DEBUG( f, args... )     K_DEBUG( f, DEBUG_LEVEL , args )
#else
#define DEBUG(f,args...)
#endif

#endif
