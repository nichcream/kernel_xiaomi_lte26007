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
* sbus.h
*
* This module contains definitions and declarations for messages exchanged by
* the ETF test_sdio driver interfaces with the Linux Kernel MMC/SDIO stack.
*
****************************************************************************/

#ifndef _SBUS_H
#define _SBUS_H

#include <linux/types.h>
#include "ETF_Api.h"

void ETF_CB_RxComplete(LL_HANDLE LowerHandle, void *pFrame);

LL_HANDLE ETF_CB_Create(ETF_HANDLE ETFHandle, UL_HANDLE ulHandle);

ETF_STATUS_CODE ETF_CB_Start (LL_HANDLE LowerHandle,
		uint32_t FmLength, void *FirmwareImage, uint32_t BlLength, void *bootloader);

ETF_STATUS_CODE ETF_CB_Stop (LL_HANDLE LowerHandle);

ETF_STATUS_CODE ETF_CB_Devstop(void *LowerHandle);


ETF_STATUS_CODE ETF_CB_ScheduleTx (LL_HANDLE LowerHandle);

ETF_STATUS_CODE ETF_CB_Destroy (LL_HANDLE LowerHandle);

ETF_STATUS_CODE SBUS_ChipDetect (LL_HANDLE LowerHandle);

#ifdef FPGA
int cw1200_load_firmware_cw1260_fpga(struct CW1200_priv *priv, uint8_t *firmware,
		uint32_t fw_length, uint8_t *bootloader, uint32_t bl_length);
#endif
int cw1200_load_bootloader_cw1260(struct CW1200_priv *priv, uint8_t *firmware,
		uint32_t fw_length, uint8_t *bootloader, uint32_t bl_length);

/*Sdio addr is 4*spi_addr*/
#define SPI_REG_ADDR_TO_SDIO(spi_reg_addr) ((spi_reg_addr) << 2)


#define SDIO_ADDR17BIT(buf_num, mpf, rfu, reg_id_ofs) (((buf_num & 0x1F)<<7) \
		| ((mpf & 1)<<6) \
		| ((rfu & 1)<<5) \
		| ((reg_id_ofs & 0x1f)<<0) \
		)

#define ETF_ADAPTER_IO_DRIVE_STRENGTH_2MA 0
#define ETF_ADAPTER_IO_DRIVE_STRENGTH_4MA 1
#define ETF_ADAPTER_IO_DRIVE_STRENGTH_8MA 2

#define SDD_STL8601_CONFIG4_REG9_BITMASK (0x30000333)
#define SDD_STL8601_CONFIG4_REG10_BITMASK (0x03300000)

#define ST90TDS_ADDR_COUNT_MASK 0x0FFF
#define SPI_CONT_NEXT_LENGTH(_cont) (_cont & ST90TDS_ADDR_COUNT_MASK)
#define SBUS_GET_MSG_SEQ(_id) ((_id)>>13)

#define HI_PUT_MSG_SEQ(_id, _seq) ((_id) |= (uint16_t)((_seq)<<13))

#define SDIO_READ 1
#define SDIO_WRITE 2
#define STLC9000_FN 1 /* WLAN Function Number */
#define BIT_16_REG 2 /* 2 Bytes */
#define BIT_32_REG 4
#define DOWNLOAD_BLOCK_SIZE (1024)

#define MAX_RETRY 5

#ifdef MOP_WORKAROUND
#define SDIO_BLOCK_SIZE 1024
#else
#define SDIO_BLOCK_SIZE 1024
#endif
#ifdef MOP_WORKAROUND
#define RWBLK_SIZE 1024
#endif
#define FIRMWARE_DLOAD_ADDR 0x08000000
#define ST90TDS_SRAM_BASE_LSB_MASK 0x0000FFFF

#define DOWNLOAD_BLOCK_SIZE_WR (0x1000 - 4)
/*one SPI message cannot be bigger than (2"12-1)*2 bytes, "*2"
to cvt to bytes*/
#define MAX_SZ_RD_WR_BUFFERS (DOWNLOAD_BLOCK_SIZE_WR*2)

#define HI_MAX_CONFIG_TABLES 4
#define HI_SW_LABEL_MAX 128
#define HI_CNF_BASE 0x0400
#define HI_MSG_SEQ_RANGE 0x0007
#define HI_EXCEP_FILENAME_SIZE 48
#define MSG_ID_MASK 0x1FFF /* bit 0 to 12*/

#define HI_IND_BASE 0x800
#define HI_EXCEPTION_IND_ID (HI_IND_BASE+0x00) /* HI exception id */
#define HI_STARTUP_IND_ID (HI_IND_BASE+0x01) /* HI config (1st)id */
#define HI_TRACE_IND_ID (HI_IND_BASE+0x02) /* HI trace id */
#define HI_GENERIC_IND_ID (HI_IND_BASE+0x03) /* HI gereric indication id */

#define PIGGYBACK_CTRL_REG 2

/* WBF - Control register bit set */
#define ST90TDS_CONT_IRQ_RDY_MASK 0x3FFF /* bit 15:14 of control register*/
#define ST90TDS_CONT_IRQ_SEL_MASK 0xBFFF /* bit 15:14 of control register*/
#define ST90TDS_CONT_RDY_SEL_MASK 0x7FFF /* bit 15:14 of control register*/
#define ST90TDS_CONT_WUP_BIT_MASK 0xEFFF /* bit 12 of control register*/


/* SPI Config register bit set*/
#define ST90TDS_CONFIG_CLEAR_INT_MASK 0x7FFF /* bit 15 of config_reg*/
#define ST90TDS_CONFIG_CPU_RESET_MASK 0xFFFFBFFF /* bit 14 of config_reg */
#define ST90TDS_CONFIG_CPU_CLK_DIS_MASK 0xFFFFEFFF /* bit 12 of config_reg */
#define ST90TDS_CONFIG_ACCESS_MODE_MASK 0xFFFFFBFF /* bit 10 of config register*/
#define ST90TDS_CONFIG_WORD_MODE_MASK 0xFFE7 /* bit 4:3 of config_reg*/
#define ST90TDS_CONFIG_FRAME_BIT_MASK 0xFFFB /* bit 2 of config_reg*/

/* download control area */
#define DOWNLOAD_CTRL_DATA_DWORDS (32-6)


#define BUF_TYPE_NONE 0
#define BUF_TYPE_DATA 1
#define BUF_TYPE_MANG 2

/* Macros for CW1200 type detection */
#define CW1200_CUT_1x_ID_ADDR (0xFFF17F90)
#define CW12x0_CUT_xx_ID_ADDR (0xFFF1FF90)

#define CW1200_CUT_11_ID_STR (0x302E3830)
#define CW1200_CUT_22_ID_STR1 (0x302e3132)
#define CW1200_CUT_22_ID_STR2 (0x32302e30)
#define CW1200_CUT_22_ID_STR3 (0x3335)
#define CW1250_CUT_11_ID_STR1 (0x302e3033)
#define CW1250_CUT_11_ID_STR2 (0x33302e32)
#define CW1250_CUT_11_ID_STR3 (0x3535)

#define DEVICE_UP 10
#define DEVICE_DOWN 11

extern uint32 hif_strength;
extern uint32 DPLL_INIT_VAL;

CW1200_STATUS_E SBUS_SramRead_APB(struct CW1200_priv *priv,
		uint32_t base_addr, uint8_t *buffer, uint32_t byte_count);

CW1200_STATUS_E SBUS_SramWrite_APB(struct CW1200_priv *priv,
		uint32_t base_addr, uint8_t *buffer, uint32_t byte_count);

typedef volatile struct {
	uint32 ImageSize; /* size of whole firmware file (including Cheksum), host init */
	uint32 Flags; /* downloading flags */
	uint32 Put; /* No. of bytes put into the download, init & updated by host */
	uint32 TracePc; /* last traced program counter, last ARM reg_pc */
	uint32 Get; /* No. of bytes read from the download, host init, device updates */
	uint32 Status; /* r0, boot losader status, host init to pending, device updates */
	uint32 DebugData[DOWNLOAD_CTRL_DATA_DWORDS]; /* Extra debug info, r1 to r14 if status=r0=DOWNLOAD_EXCEPTION */
} download_cntl_t;

typedef struct HI_MSG_HDR_S {
	uint16_t MsgLen; /* length in bytes from MsgLen to end of payload */
	uint16_t MsgId;
} HI_MSG_HDR;

typedef struct msg_hdr_payload_s {
	HI_MSG_HDR hdr;
	uint8_t payload[];
} hif_rw_msg_hdr_payload_t;

/* Startup Indication message */
typedef struct HI_STARTUP_IND_S {
	uint16_t MsgLen;
	uint16_t MsgId;
	uint16_t NumInpChBufs;
	uint16_t SizeInpChBuf;
	uint16_t HardwareId;
	uint16_t HardwareSubId;
	uint16_t InitStatus;
	uint16_t InitFlags;
	uint16_t FirmwareType;
	uint16_t FirmwareApiVer;
	uint16_t FirmwareBuildNumber;
	uint16_t FirmwareVersion;
	uint8_t FirmwareLabel[HI_SW_LABEL_MAX];
	uint32_t Configuration[HI_MAX_CONFIG_TABLES];
} HI_STARTUP_IND;

/* Exception Indication message */
typedef struct HI_EXCEPTION_IND_S {
	uint16_t MsgLen;
	uint16_t MsgId;
	uint32_t Reason;
	uint32_t R0;
	uint32_t R1;
	uint32_t R2;
	uint32_t R3;
	uint32_t R4;
	uint32_t R5;
	uint32_t R6;
	uint32_t R7;
	uint32_t R8;
	uint32_t R9;
	uint32_t R10;
	uint32_t R11;
	uint32_t R12;
	uint32_t SP;
	uint32_t LR;
	uint32_t PC;
	uint32_t CPSR;
	uint32_t SPSR;
	uint8_t FileName[HI_EXCEP_FILENAME_SIZE];
} HI_EXCEPTION_IND;

#endif /* _SBUS_H */
