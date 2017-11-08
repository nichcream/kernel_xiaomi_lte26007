/*
 *  Header files defines some USB inline routines
 *
 *  $Id: 8192cd_usb.h,v 1.4.4.5 2010/12/10 06:11:55 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _8192CD_USB_H_
#define _8192CD_USB_H_

#ifdef __KERNEL__
#include <asm/bitops.h>
#endif

#include "8192cd.h"
#include "8192cd_usb_hw.h"
#include "8192cd_usb_recv.h"
#include "8192cd_usb_cmd.h"
#include "hal_intf_xmit.h"

typedef __kernel_size_t		SIZE_T;	
typedef __kernel_ssize_t	SSIZE_T;

#define SIZE_PTR		SIZE_T
#define SSIZE_PTR	SSIZE_T

#define USB_VENDER_ID_REALTEK		0x0BDA

#define REALTEK_USB_VENQT_READ		0xC0
#define REALTEK_USB_VENQT_WRITE	0x40
#define REALTEK_USB_VENQT_CMD_REQ	0x05
#define REALTEK_USB_VENQT_CMD_IDX	0x00

enum{
	VENDOR_WRITE = 0x00,
	VENDOR_READ = 0x01,
};

enum {
	ENQUEUE_TO_HEAD = 0,
	ENQUEUE_TO_TAIL =1,
};

#define ALIGNMENT_UNIT				16
#define MAX_VENDOR_REQ_CMD_SIZE	254		//8188cu SIE Support
#define MAX_USB_IO_CTL_SIZE		(MAX_VENDOR_REQ_CMD_SIZE +ALIGNMENT_UNIT)


#define FW_8192C_START_ADDRESS		0x1000
//#define FW_8192C_END_ADDRESS		0x3FFF //Filen said this is for test chip
#define FW_8192C_END_ADDRESS		0x1FFF

#define MAX_CONTINUAL_URB_ERR		4

#ifdef CONFIG_USB_RX_AGGREGATION

typedef enum _USB_RX_AGG_MODE{
	USB_RX_AGG_DISABLE,
	USB_RX_AGG_DMA,
	USB_RX_AGG_USB,
	USB_RX_AGG_MIX
}USB_RX_AGG_MODE;

#define MAX_RX_DMA_BUFFER_SIZE	10240		// 10K for 8192C RX DMA buffer

#endif


#define TX_SELE_HQ			BIT(0)		// High Queue
#define TX_SELE_LQ			BIT(1)		// Low Queue
#define TX_SELE_NQ			BIT(2)		// Normal Queue

// Note: We will divide number of page equally for each queue other than public queue!
#define TX_TOTAL_PAGE_NUMBER		0xF8
#define TX_PAGE_BOUNDARY		(TX_TOTAL_PAGE_NUMBER + 1)

// For Normal Chip Setting
// (HPQ + LPQ + NPQ + PUBQ) shall be TX_TOTAL_PAGE_NUMBER
#define NORMAL_PAGE_NUM_PUBQ			0xE7
#define NORMAL_PAGE_NUM_HPQ			0x0C
#define NORMAL_PAGE_NUM_LPQ			0x02
#define NORMAL_PAGE_NUM_NPQ			0x02

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

#define WMM_NORMAL_PAGE_NUM_PUBQ		0xB0
#define WMM_NORMAL_PAGE_NUM_HPQ		0x29
#define WMM_NORMAL_PAGE_NUM_LPQ			0x1C
#define WMM_NORMAL_PAGE_NUM_NPQ		0x1C

// Note: We will divide number of page equally for each queue other than public queue!
// 22k = 22528 bytes = 176 pages (@page =  128 bytes)
// must reserved about 7 pages for LPS =>  176-7 = 169 (0xA9)
// 2*BCN / 1*ps-poll / 1*null-data /1*prob_rsp /1*QOS null-data /1*BT QOS null-data 

#define TX_TOTAL_PAGE_NUMBER_88E		0xA9//  169 (21632=> 21k)
#define TX_PAGE_BOUNDARY_88E	(TX_TOTAL_PAGE_NUMBER_88E + 1)

//Note: For Normal Chip Setting ,modify later
#define WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_88E	TX_TOTAL_PAGE_NUMBER_88E  //0xA9 , 0xb0=>176=>22k
#define WMM_NORMAL_TX_PAGE_BOUNDARY_88E	(WMM_NORMAL_TX_TOTAL_PAGE_NUMBER_88E + 1) //0xA9

#define POLLING_READY_TIMEOUT_COUNT	1000

#define RTW_USB_CONTROL_MSG_TIMEOUT	500		//ms

#define MAX_HW_TX_QUEUE			8
#define MAX_STA_TX_SERV_QUEUE		5	// this must be smaller than or equal to MAX_HW_TX_QUEUE

struct hal_data_8192cu
{
	u32	UsbBulkOutSize;

	int	RtBulkOutPipe[3];
	int	RtBulkInPipe;
	int	RtIntInPipe;
	// Add for dual MAC  0--Mac0 1--Mac1
	u32	interfaceIndex;

	u8	OutEpQueueSel;
	u8	OutEpNumber;

	u8	Queue2EPNum[8];//for out endpoint number mapping

#ifdef CONFIG_RTL_88E_SUPPORT
	// Interrupt related register information.
	u32	IntArray[3];//HISR0,HISR1,HSISR
	u32	IntrMask[3];
	u8	C2hArray[16];
#endif
#ifdef CONFIG_USB_TX_AGGREGATION
	u8	UsbTxAggMode;
	u8	UsbTxAggDescNum;
#endif
#ifdef CONFIG_USB_RX_AGGREGATION
	u32	MaxUsbRxAggBlock;

	USB_RX_AGG_MODE	UsbRxAggMode;
	u8	UsbRxAggBlockCount;			// USB Block count. Block size is 512-byte in hight speed and 64-byte in full speed
	u8	UsbRxAggBlockTimeout;
	u8	UsbRxAggPageCount;			// 8192C DMA page count
	u8	UsbRxAggPageTimeout;
#endif

	// 2010/12/10 MH Add for USB aggreation mode dynamic shceme.
	BOOLEAN		UsbRxHighSpeedMode;
};

typedef struct hal_data_8192cu HAL_INTF_DATA_TYPE, *PHAL_INTF_DATA_TYPE;


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)) 
#define rtw_usb_control_msg(dev, pipe, request, requesttype, value, index, data, size, timeout_ms) \
	usb_control_msg((dev), (pipe), (request), (requesttype), (value), (index), (data), (size), (timeout_ms)) 
#define rtw_usb_bulk_msg(usb_dev, pipe, data, len, actual_length, timeout_ms) \
	usb_bulk_msg((usb_dev), (pipe), (data), (len), (actual_length), (timeout_ms))
#else
#define rtw_usb_control_msg(dev, pipe, request, requesttype, value, index, data, size,timeout_ms) \
	usb_control_msg((dev), (pipe), (request), (requesttype), (value), (index), (data), (size), \
		((timeout_ms) == 0) ||((timeout_ms)*HZ/1000>0)?((timeout_ms)*HZ/1000):1) 
#define rtw_usb_bulk_msg(usb_dev, pipe, data, len, actual_length, timeout_ms) \
	usb_bulk_msg((usb_dev), (pipe), (data), (len), (actual_length), \
		((timeout_ms) == 0) ||((timeout_ms)*HZ/1000>0)?((timeout_ms)*HZ/1000):1) 
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
#define rtw_usb_buffer_alloc(dev, size, mem_flags, dma) usb_alloc_coherent((dev), (size), (mem_flags), (dma))
#define rtw_usb_buffer_free(dev, size, addr, dma) usb_free_coherent((dev), (size), (addr), (dma))
#else
#define rtw_usb_buffer_alloc(dev, size, mem_flags, dma) usb_buffer_alloc((dev), (size), (mem_flags), (dma))
#define rtw_usb_buffer_free(dev, size, addr, dma) usb_buffer_free((dev), (size), (addr), (dma))
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
#define PTR_ALIGN(p, a)         ((typeof(p))ALIGN((unsigned long)(p), (a)))
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23))
static inline int usb_endpoint_num(const struct usb_endpoint_descriptor *epd)
{
	return epd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}
#endif

u8 usb_read8(struct rtl8192cd_priv *priv, u32 addr);
u16 usb_read16(struct rtl8192cd_priv *priv, u32 addr);
u32 usb_read32(struct rtl8192cd_priv *priv, u32 addr);
int usb_write8(struct rtl8192cd_priv *priv, u32 addr, u8 val);
int usb_write16(struct rtl8192cd_priv *priv, u32 addr, u16 val);
int usb_write32(struct rtl8192cd_priv *priv, u32 addr, u32 val);
int usb_writeN(struct rtl8192cd_priv *priv, u32 addr, u32 len, u8* val);

u32 usb_dvobj_init(struct rtl8192cd_priv *priv);
void usb_dvobj_deinit(struct rtl8192cd_priv *priv);

void rtw_dev_unload(struct rtl8192cd_priv *priv);

u8 rtw_init_drv_sw(struct rtl8192cd_priv *priv);
u8 rtw_free_drv_sw(struct rtl8192cd_priv *priv);

BOOLEAN _MappingOutEP(struct rtl8192cd_priv *priv, u8 NumOutPipe, BOOLEAN IsTestChip);
void _InitQueueReservedPage(struct rtl8192cd_priv *priv);
void _InitQueuePriority(struct rtl8192cd_priv *priv);
void InitUsbAggregationSetting(struct rtl8192cd_priv *priv);

unsigned int ffaddr2pipehdl(struct rtl8192cd_priv *priv, u32 addr);

#endif // _8192CD_USB_H_

