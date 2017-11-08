/*---------------------------------------------------------------------------------
*   PROJECT     : CW1200 Linux driver - using STE-UMAC
*   FILE  : sbus.h
*   REVISION : 1.0
*------------------------------------------------------------------------------*/
/**
* \defgroup SBUS
* \brief
* This module interfaces with the SDIO Host Controller Driver and implements the UMAC Lower Interface
*/
/**
* \file
* \ingroup SBUS
* \brief
*
* \date 19/09/2009
* \author Ajitpal Singh [ajitpal.singh@stericsson.com]
* \note
* Last person to change file : Ajitpal Singh
*/

/***************************************************************************************
*                              Include Files
***************************************************************************************/
#ifndef __SBUS_HEADER__
#define __SBUS_HEADER__

#include <linux/types.h>
#include "UMI_Api.h"


/***************************************************************************************
*                              Function Declarations
***************************************************************************************/

LL_HANDLE  UMI_CB_Create(UMI_HANDLE UMACHandle,UL_HANDLE ulHandle);

UMI_STATUS_CODE  UMI_CB_Start (LL_HANDLE  LowerHandle,uint32_t FmLength,void * FirmwareImage);

UMI_STATUS_CODE  UMI_CB_Stop ( LL_HANDLE  LowerHandle );

UMI_STATUS_CODE  UMI_CB_ScheduleTx ( LL_HANDLE LowerHandle );


UMI_STATUS_CODE  UMI_CB_Destroy ( LL_HANDLE LowerHandle );
/***************************************************************************************
*                                                         MACRO definitions
***************************************************************************************/
#define SPI_REG_ADDR_TO_SDIO( spi_reg_addr  ) ( ( spi_reg_addr ) * 4 ) //Sdio addr is 4*spi_addr

#define SDIO_ADDR17BIT( buf_num, mpf, rfu, reg_id_ofs ) ( ( ( buf_num    & 0x1F )<<7 ) \
                                                        |  ( ( mpf        & 1    )<<6 ) \
                                                        |  ( ( rfu        & 1    )<<5 ) \
                                                        |  ( ( reg_id_ofs & 0x1f )<<0 ) \
                                                        )

#define ST90TDS_ADDR_COUNT_MASK         0x0FFF
#define SPI_CONT_NEXT_LENGTH(_cont) (_cont & ST90TDS_ADDR_COUNT_MASK)
#define SBUS_GET_MSG_SEQ(_id)         ((_id)>>13)

#define HI_PUT_MSG_SEQ(_id,_seq)     ((_id) |= (uint16_t)((_seq)<<13))


#define DPLL_INIT_VAL_9000		0x00000191
#define DPLL_INIT_VAL_CW1200		0x0EC4F121
#define SDIO_READ						1
#define SDIO_WRITE						2
#define STLC9000_FN						1   /* WLAN Function Number */
#define BIT_16_REG 						2   /* 2 Bytes */
#define BIT_32_REG						4

#ifdef MOP_WORKAROUND
  #define RWBLK_SIZE 1024
#endif
#define FIRMWARE_DLOAD_ADDR	0x08000000
#define ST90TDS_SRAM_BASE_LSB_MASK        0x0000FFFF

#define 	DOWNLOAD_BLOCK_SIZE_WR		(0x1000 - 4)
#define MAX_SZ_RD_WR_BUFFERS	(DOWNLOAD_BLOCK_SIZE_WR*2)	//one SPI message cannot be bigger than (2"12-1)*2 bytes // "*2" to cvt to bytes

#define HI_MAX_CONFIG_TABLES        4
#define HI_SW_LABEL_MAX             128
#define HI_CNF_BASE                 0x0400
#define HI_MSG_SEQ_RANGE            0x0007
#define HI_EXCEP_FILENAME_SIZE 48
#define  MSG_ID_MASK    0x1FFF     // bit 0 to 12

#define HI_IND_BASE                 0x800
#define HI_EXCEPTION_IND_ID         (HI_IND_BASE+0x00)    /* HI exception id */
#define HI_STARTUP_IND_ID           (HI_IND_BASE+0x01)      /* HI config (1st)id */
#define HI_TRACE_IND_ID             (HI_IND_BASE+0x02)    /* HI trace id */
#define HI_GENERIC_IND_ID           (HI_IND_BASE+0x03)    /* HI gereric indication id */

#define PIGGYBACK_CTRL_REG  2

/***************************************************************************************
*                                                         Device register definitions
***************************************************************************************/
/* WBF - SPI Register Addresses */
#define ST90TDS_ADDR_ID_BASE            0x0000
#define ST90TDS_CONFIG_REG_ID           0x0000        // 16/32 bits
#define ST90TDS_CONTROL_REG_ID          0x0001        // 16/32 bits
#define ST90TDS_IN_OUT_QUEUE_REG_ID     0x0002        // 16 bits, Q mode W/R
#define ST90TDS_AHB_DPORT_REG_ID        0x0003        // 32 bits, AHB bus R/W
#define ST90TDS_SRAM_BASE_ADDR_REG_ID   0x0004        // 16/32 bits
#define ST90TDS_SRAM_DPORT_REG_ID       0x0005        // 32 bits, APB bus R/W
#define ST90TDS_TSET_GEN_R_W_REG_ID     0x0006        // 32 bits, t_settle/general
#define ST90TDS_FRAME_OUT_REG_ID        0x0007        // 16 bits, Q mode read, no length
#define ST90TDS_ADDR_ID_MAX             ST90TDS_FRAME_OUT_REG_ID


/* WBF - Control register bit set */
#define ST90TDS_CONT_RDY_ENABLE         0x8000        // bit 15:14 of control register
#define ST90TDS_CONT_IRQ_ENABLE         0x4000        // bit 15:14 of control register
#define ST90TDS_CONT_IRQ_RDY_ENABLE     0xC000        // bit 15:14 of control register
#define ST90TDS_CONT_IRQ_RDY_MASK       0x3FFF        // bit 15:14 of control register
#define ST90TDS_CONT_IRQ_SEL_MASK       0xBFFF        // bit 15:14 of control register
#define ST90TDS_CONT_RDY_SEL_MASK       0x7FFF        // bit 15:14 of control register
#define ST90TDS_CONT_RDY_BIT            0x2000        // bit 13 of control register
#define ST90TDS_CONT_WUP_BIT            0x1000        // bit 12 of the control register
#define ST90TDS_CONT_WUP_BIT_MASK       0xEFFF        // bit 12 of control register
#define ST90TDS_CONT_NEXT_LEN_MASK      0x0FFF        // next o/p length, bit 11 to 0



// SPI Config register bit set
#define ST90TDS_CONFIG_CLEAR_INT_BIT	0x8000		// bit 15 of config_reg
#define ST90TDS_CONFIG_CLEAR_INT_MASK	0x7FFF		// bit 15 of config_reg
#define ST90TDS_CONFIG_CPU_RESET_BIT	0x4000		// bit 14 of config_reg, cpu reset
#define ST90TDS_CONFIG_CPU_RESET_MASK	0xFFFFBFFF		// bit 14 of config_reg
#define ST90TDS_CONFIG_PFETCH_BIT		0x00002000		// bit 13 of config_reg, APB bus
#define ST90TDS_CONFIG_CPU_CLK_DIS_BIT	0x1000		// bit 12 of config_reg
#define ST90TDS_CONFIG_CPU_CLK_DIS_MASK	0xFFFFEFFF		// bit 12 of config_reg
#define ST90TDS_CONFIG_AHB_PFETCH_BIT	0x0800		// bit 11 of config_reg, AHB bus
#define ST90TDS_CONFIG_ACCESS_MODE_BIT	0x0400		// bit 10 of config_reg, 0 QueueM
#define ST90TDS_CONFIG_ACCESS_MODE_MASK	0xFFFFFBFF		// bit 10 of config register
#define ST90TDS_CONFIG_ERROR_4_BIT		0x0200		// bit 9 of config_reg
#define ST90TDS_CONFIG_ERROR_3_BIT		0x0100		// bit 8 of config_reg
#define ST90TDS_CONFIG_ERROR_2_BIT		0x0080		// bit 7 of config_reg
#define ST90TDS_CONFIG_ERROR_1_BIT		0x0040		// bit 6 of config_reg
#define ST90TDS_CONFIG_ERROR_0_BIT		0x0020		// bit 5 of config_reg
#define ST90TDS_CONFIG_WORD_MODE_BITS	0x0018		// bit 4:3 of config_reg
#define ST90TDS_CONFIG_WORD_MODE_1		0x0008		// bit 4:3 of config_reg
#define ST90TDS_CONFIG_WORD_MODE_2		0x0010		// bit 4:3 of config_reg
#define ST90TDS_CONFIG_WORD_MODE_MASK	0xFFE7		// bit 4:3 of config_reg
#define ST90TDS_CONFIG_CSN_FRAME_BIT		(1<<7)		// bit 7 of config_reg
#define ST90TDS_CONFIG_FRAME_BIT_MASK	0xFFFB		// bit 2 of config_reg


/* Hardware Type Definitions */
#define HIF_8601_VERSATILE  0
#define HIF_8601_SILICON  1
#define HIF_9000_SILICON_VERSTAILE 2

/* For CW1200 the IRQ Enable and Ready Bits are in CONFIG register */
#define ST90TDS_CONF_IRQ_RDY_ENABLE		0x00030000	// bit 17:16 of config register

/* For boot loader detection */
#define DOWNLOAD_ARE_YOU_HERE	0x87654321
#define DOWNLOAD_I_AM_HERE		0x12345678

#define SYS_BASE_ADDR_SILICON			0
#define PAC_BASE_ADDRESS_SILICON	(SYS_BASE_ADDR_SILICON+0x09000000)
#define PAC_SHARED_MEMORY_SILICON	PAC_BASE_ADDRESS_SILICON

/* download control area */
#define DOWNLOAD_BOOT_LOADER_OFFSET 0x00000000  /* boot loader start address in SRAM */
#define DOWNLOAD_FIFO_OFFSET        0x00004000  /* 32K, 0x4000 to 0xDFFF */
#define DOWNLOAD_FIFO_SIZE          0x00008000  /* 32K */
#define DOWNLOAD_CTRL_OFFSET        0x0000FF80  /* 128 bytes, 0xFF80 to 0xFFFF */
#define DOWNLOAD_CTRL_DATA_DWORDS   (32-6)


typedef volatile struct
{
	uint32	ImageSize;	/* size of whole firmware file (including Cheksum), host init */
	uint32	Flags;	/* downloading flags */
	uint32	Put;		/* No. of bytes put into the download, init & updated by host */
	uint32	TracePc;	/* last traced program counter, last ARM reg_pc */
	uint32	Get;		/* No. of bytes read from the download, host init, device updates */
	uint32	Status;		/* r0, boot losader status, host init to pending, device updates */
	uint32	DebugData[DOWNLOAD_CTRL_DATA_DWORDS]; /* Extra debug info, r1 to r14 if status=r0=DOWNLOAD_EXCEPTION */
}download_cntl_t;


// download control area
#define	DOWNLOAD_IMAGE_SIZE_REG	(DOWNLOAD_CTRL_OFFSET + offsetof(download_cntl_t, ImageSize))
#define	DOWNLOAD_FLAGS_REG		(DOWNLOAD_CTRL_OFFSET + offsetof(download_cntl_t, Flags))
#define DOWNLOAD_PUT_REG		(DOWNLOAD_CTRL_OFFSET + offsetof(download_cntl_t, Put))
#define DOWNLOAD_TRACE_PC_REG	(DOWNLOAD_CTRL_OFFSET + offsetof(download_cntl_t, TracePc))
#define	DOWNLOAD_GET_REG		(DOWNLOAD_CTRL_OFFSET + offsetof(download_cntl_t, Get))
#define	DOWNLOAD_STATUS_REG		(DOWNLOAD_CTRL_OFFSET + offsetof(download_cntl_t, Status))
#define DOWNLOAD_DEBUG_DATA_REG	(DOWNLOAD_CTRL_OFFSET + offsetof(download_cntl_t, DebugData))
#define DOWNLOAD_DEBUG_DATA_LEN	108


/* download error code */
#define DOWNLOAD_PENDING		0xFFFFFFFF
#define DOWNLOAD_SUCCESS		0
#define DOWNLOAD_EXCEPTION		1
#define DOWNLOAD_ERR_MEM_1		2
#define DOWNLOAD_ERR_MEM_2		3
#define DOWNLOAD_ERR_SOFTWARE	4
#define DOWNLOAD_ERR_FILE_SIZE	5
#define DOWNLOAD_ERR_CHECKSUM	6
#define DOWNLOAD_ERR_OVERFLOW	7


typedef struct HI_MSG_HDR_S
{
    uint16_t  MsgLen  ;                /* length in bytes from MsgLen to end of payload */
    uint16_t  MsgId   ;
} HI_MSG_HDR;

typedef struct msg_hdr_payload_s
{
  HI_MSG_HDR    hdr           ;
  uint8_t  payload[];
} hif_rw_msg_hdr_payload_t ;


/* Startup Indication message */
typedef struct HI_STARTUP_IND_S
{
  uint16_t  MsgLen                              ;
  uint16_t  MsgId                               ;
  uint16_t  NumInpChBufs                        ;
  uint16_t  SizeInpChBuf                        ;
  uint16_t  HardwareId                          ;
  uint16_t  HardwareSubId                       ;
  uint16_t  InitStatus                          ;
  uint16_t  InitFlags                           ;
  uint16_t  FirmwareType                        ;
  uint16_t  FirmwareApiVer                      ;
  uint16_t  FirmwareBuildNumber                 ;
  uint16_t  FirmwareVersion                     ;
  uint8_t   FirmwareLabel[HI_SW_LABEL_MAX]      ;
  uint32_t  Configuration[HI_MAX_CONFIG_TABLES] ;
} HI_STARTUP_IND ;



/* Exception Indication message */
typedef struct HI_EXCEPTION_IND_S
{
    uint16_t  MsgLen  ;
    uint16_t  MsgId   ;
    uint32_t  Reason  ;
    uint32_t  R0      ;
    uint32_t  R1      ;
    uint32_t  R2      ;
    uint32_t  R3      ;
    uint32_t  R4      ;
    uint32_t  R5      ;
    uint32_t  R6      ;
    uint32_t  R7      ;
    uint32_t  R8      ;
    uint32_t  R9      ;
    uint32_t  R10     ;
    uint32_t  R11     ;
    uint32_t  R12     ;
    uint32_t  SP      ;
    uint32_t  LR      ;
    uint32_t  PC      ;
    uint32_t  CPSR    ;
    uint32_t  SPSR    ;
    uint8_t  FileName[HI_EXCEP_FILENAME_SIZE] ;
} HI_EXCEPTION_IND ;


#endif
