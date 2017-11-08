/*
 *  Header files defines some SDIO inline routines
 *
 *  $Id: 8188e_sdio.h,v 1.4.4.5 2010/12/10 06:11:55 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _8188E_SDIO_H_
#define _8188E_SDIO_H_

#ifdef __KERNEL__
#include <asm/bitops.h>
#endif

#include "8192cd.h"
#include "8188e_sdio_hw.h"
#include "8188e_sdio_recv.h"
#include "8188e_sdio_cmd.h"
#include "hal_intf_xmit.h"

typedef __kernel_size_t		SIZE_T;	
typedef __kernel_ssize_t	SSIZE_T;

#define SIZE_PTR	SIZE_T
#define SSIZE_PTR	SSIZE_T

#define TX_SELE_HQ			BIT(0)		// High Queue
#define TX_SELE_LQ			BIT(1)		// Low Queue
#define TX_SELE_NQ			BIT(2)		// Normal Queue

// Note: We will divide number of page equally for each queue other than public queue!
#define TX_TOTAL_PAGE_NUMBER		0xF8
#define TX_PAGE_BOUNDARY		(TX_TOTAL_PAGE_NUMBER + 1)

// For Normal Chip Setting
// (HPQ + LPQ + NPQ + PUBQ) shall be TX_TOTAL_PAGE_NUMBER
#define NORMAL_PAGE_NUM_PUBQ		0xE7
#define NORMAL_PAGE_NUM_HPQ		0x0C
#define NORMAL_PAGE_NUM_LPQ		0x02
#define NORMAL_PAGE_NUM_NPQ		0x02

// For Test Chip Setting
// (HPQ + LPQ + PUBQ) shall be TX_TOTAL_PAGE_NUMBER
#define TEST_PAGE_NUM_PUBQ		0x7E

// For Test Chip Setting
#define WMM_TEST_TX_TOTAL_PAGE_NUMBER	0xF5
#define WMM_TEST_TX_PAGE_BOUNDARY	(WMM_TEST_TX_TOTAL_PAGE_NUMBER + 1) //F6

#define WMM_TEST_PAGE_NUM_PUBQ		0xA3
#define WMM_TEST_PAGE_NUM_HPQ		0x29
#define WMM_TEST_PAGE_NUM_LPQ		0x29

//Note: For Normal Chip Setting ,modify later
#define WMM_NORMAL_TX_TOTAL_PAGE_NUMBER	0xF5
#define WMM_NORMAL_TX_PAGE_BOUNDARY	(WMM_TEST_TX_TOTAL_PAGE_NUMBER + 1) //F6

#define WMM_NORMAL_PAGE_NUM_PUBQ	0xB0
#define WMM_NORMAL_PAGE_NUM_HPQ		0x29
#define WMM_NORMAL_PAGE_NUM_LPQ		0x1C
#define WMM_NORMAL_PAGE_NUM_NPQ		0x1C

// Note: We will divide number of page equally for each queue other than public queue!
// 22k = 22528 bytes = 176 pages (@page =  128 bytes)
// must reserved about 7 pages for LPS =>  176-7 = 169 (0xA9)
// 2*BCN / 1*ps-poll / 1*null-data /1*prob_rsp /1*QOS null-data /1*BT QOS null-data 

#ifdef SDIO_AP_OFFLOAD
#define TX_TOTAL_PAGE_NUMBER_88E        0xA0
#else
#define TX_TOTAL_PAGE_NUMBER_88E        0xA9    // 169 (21632=> 21k)
#endif

#define TX_PAGE_BOUNDARY_88E		(TX_TOTAL_PAGE_NUMBER_88E + 1)

//Note: For Normal Chip Setting ,modify later
#define WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_88E	TX_TOTAL_PAGE_NUMBER_88E  //0xA9 , 0xb0=>176=>22k
#define WMM_NORMAL_TX_PAGE_BOUNDARY_88E		(WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_88E + 1) //0xA9

#define MAX_HW_TX_QUEUE			8
#define MAX_STA_TX_SERV_QUEUE		5	// this must be smaller than or equal to MAX_HW_TX_QUEUE

enum SDIO_TX_INT_STATUS {
	SDIO_TX_INT_SETUP_TH = 0,
	SDIO_TX_INT_WORKING,
};

enum {
	ENQUEUE_TO_HEAD = 0,
	ENQUEUE_TO_TAIL =1,
};

extern const u32 reg_freepage_thres[SDIO_TX_FREE_PG_QUEUE];

struct hal_data_8188e
{
	//In /Out Pipe information
	int RtInPipe[2];
	int RtOutPipe[3];
	u8 Queue2Pipe[8];//for out pipe mapping
	// Add for dual MAC  0--Mac0 1--Mac1
	u32 interfaceIndex;

	u8 OutEpQueueSel;
	u8 OutEpNumber;
	
	// Auto FSM to Turn On, include clock, isolation, power control for MAC only
	u8 bMacPwrCtrlOn;
	
	//
	// SDIO ISR Related
	//
	u32 sdio_himr;
	u32 sdio_hisr;
	unsigned long SdioTxIntStatus;
	volatile u8 SdioTxIntQIdx;

	//
	// SDIO Tx FIFO related.
	//
	// HIQ, MID, LOW, PUB free pages; padapter->xmitpriv.free_txpg
	u8 SdioTxFIFOFreePage[SDIO_TX_FREE_PG_QUEUE];
	u8 SdioTxOQTFreeSpace;

	//
	// SDIO Rx FIFO related.
	//
	u8 SdioRxFIFOCnt;
	u16 SdioRxFIFOSize;
};

typedef struct hal_data_8188e HAL_INTF_DATA_TYPE, *PHAL_INTF_DATA_TYPE;

u8 sdio_read8(struct rtl8192cd_priv *priv, u32 addr);
u16 sdio_read16(struct rtl8192cd_priv *priv, u32 addr);
u32 sdio_read32(struct rtl8192cd_priv *priv, u32 addr);
s32 sdio_readN(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pbuf);
s32 sdio_write8(struct rtl8192cd_priv *priv, u32 addr, u8 val);
s32 sdio_write16(struct rtl8192cd_priv *priv, u32 addr, u16 val);
s32 sdio_write32(struct rtl8192cd_priv *priv, u32 addr, u32 val);
s32 sdio_writeN(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8* pbuf);

u32 sdio_read_port(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *mem);
u32 sdio_write_port(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *mem);

u8 SdioLocalCmd52Read1Byte(struct rtl8192cd_priv *priv, u32 addr);
u16 SdioLocalCmd52Read2Byte(struct rtl8192cd_priv *priv, u32 addr);
u32 SdioLocalCmd52Read4Byte(struct rtl8192cd_priv *priv, u32 addr);
u32 SdioLocalCmd53Read4Byte(struct rtl8192cd_priv *priv, u32 addr);
void SdioLocalCmd52Write1Byte(struct rtl8192cd_priv *priv, u32 addr, u8 v);
void SdioLocalCmd52Write2Byte(struct rtl8192cd_priv *priv, u32 addr, u16 v);
void SdioLocalCmd52Write4Byte(struct rtl8192cd_priv *priv, u32 addr, u32 v);

void InitSdioInterrupt(struct rtl8192cd_priv *priv);
void EnableSdioInterrupt(struct rtl8192cd_priv *priv);
void DisableSdioInterrupt(struct rtl8192cd_priv *priv);

int sdio_dvobj_init(struct rtl8192cd_priv *priv);
void sdio_dvobj_deinit(struct rtl8192cd_priv *priv);

int sdio_alloc_irq(struct rtl8192cd_priv *priv);
int sdio_free_irq(struct rtl8192cd_priv *priv);

void rtw_dev_unload(struct rtl8192cd_priv *priv);

u8 rtw_init_drv_sw(struct rtl8192cd_priv *priv);
u8 rtw_free_drv_sw(struct rtl8192cd_priv *priv);

void rtl8188es_interface_configure(struct rtl8192cd_priv *priv);
void _InitQueueReservedPage(struct rtl8192cd_priv *priv);
void _InitQueuePriority(struct rtl8192cd_priv *priv);
void _initSdioAggregationSetting(struct rtl8192cd_priv *priv);

u8 HalQueryTxBufferStatus8189ESdio(struct rtl8192cd_priv *priv);
#ifdef CONFIG_SDIO_TX_IN_INTERRUPT
u8 HalQueryTxBufferStatus8189ESdio2(struct rtl8192cd_priv *priv);
#endif
u8 HalQueryTxOQTBufferStatus8189ESdio(struct rtl8192cd_priv *priv);

#endif // _8188E_SDIO_H_

