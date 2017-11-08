/*
* ST-Ericsson ETF driver
*
* Copyright (c) ST-Ericsson SA 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
/**********************************************************************
* st90tds_if.h
*
* This is the Application/Driver interface header file for the ST90Tds
* Test Driver. The file is shared by both the host driver and the
* host application.
*
***********************************************************************/

#ifndef _ST90TDS_IF_H
#define _ST90TDS_IF_H

/* Unique GUID used by users applications to forward IOCTLs to the driver*/
/* {61535730-0797-40B1-AA5F-21F5779C78BD}*/

#include "dev_ioctl.h"

#define FALSE 0
#define TRUE 1

/* Messages IDs meant for driver with IOCTL_ST90TDS_REQ_CNF*/
#define ST90TDS_READ_PCI_CONFIG 0x00 /* Read PCI Config register*/
#define ST90TDS_READ_PCI_BAR_0 0x01 /* Read from PCI memory (BAR0)*/
#define ST90TDS_READ_PCI_BAR_1 0x02 /* Read from PCI memory (BAR1)*/
#define ST90TDS_WRITE_PCI_BAR_1 0x06 /* Write to PCI memory (BAR1)*/
#define ST90TDS_WRITE_PCI_CONFIG 0x04 /* Write PCI Config register*/
#define ST90TDS_WRITE_PCI_BAR_0 0x05 /* Write to PCI memory (BAR0)*/
#define ST90TDS_PCI_TARGET_TEST 0x08 /*Write-Read-Compare BAR0,unsupported*/
#define ST90TDS_START_ADAPTER 0x09 /* Loads Firmware and start the adapter*/
#define ST90TDS_STOP_ADAPTER 0x0A /*Stops the adapter*/
#define ST90TDS_ABORT_ADAP_OP 0x0B /* Aboart start the adapter*/
#define ST90TDS_SPI_TEST 0x0C /* perform SPI/SDIO test*/
#define ST90TDS_SEND_SDIO_CMD 0x0D /* sends an sdio command*/
#define ST90TDS_CONFIG_ADAPTER 0x0E /* send adapter configuration params*/
#define ST90TDS_SDIO_TEST 0x0F /* perform SDIO test*/
#define ST90TDS_CONFIG_SPI_FREQ 0x10 /* send adapter configuration params*/
#define ST90TDS_CONFIG_SDIO_FREQ 0x11 /* send adapter configuration params*/
#define ST90TDS_START_DEVICE_TEST 0x12 /* Device test to initialize PAS SRAM*/

#define ST90TDS_SDIO_TEST 0x0F /* perform SDIO test*/
#define ST90TDS_EXIT_ADAPTER 0x1E /*exits the adapter on rmmod*/
#define ST90TDS_CONFIG_SPI_FREQ 0x10 /* send adapter configuration params*/
#define ST90TDS_CONFIG_SDIO_FREQ 0x11 /* send adapter configuration params*/
#define ST90TDS_START_DEVICE_TEST 0x12 /* Device test to initialize PAS SRAM*/
#define ST90TDS_SBUS_READ 0x13
#define ST90TDS_SBUS_WRITE 0x14
#define ST90TDS_READ_SRAM 0x15
#define ST90TDS_WRITE_SRAM 0x16
#define ST90TDS_READ_AHB 0x17
#define ST90TDS_WRITE_AHB 0x18
#define ST90TDS_GET_DEVICE_OPTION 0x19
#define ST90TDS_SET_DEVICE_OPTION 0x1A
/*To check device is sleeping or awake*/
#define ST90TDS_SLEEP_CNF 0x1B
#define ST90TDS_START_BOOT_LOADER 0x07 /*To Load Bootloader*/


#define ST90TDS_SET_UART_PRINTENABLE 0x1C
#define ST90TDS_SEND_SDD 0x1D /* SDD File for DPLL*/
#define ST90TDS_EXIT_ADAPTER 0x1E /*exits the adapter on rmmod*/

#define CHIP_CW1200_CUT_10	2010 // "CW1200 Cut 1.0"
#define CHIP_CW1200_CUT_11	2011 // "CW1200 Cut 1.1"
#define CHIP_CW1200_CUT_20	2020 // "CW1200 Cut 2.0"
#define CHIP_CW1250_CUT_11	2511 // "CW1250 Cut 1.1"
#define CW1200_CW1250_CUT_xx	2255 // "CW1200 Cut 2.x/CW1250 Cut 1.0"
#define CW1260_HW_REV_CUT10	6010 // "CW1260 cut1.0"
#define CHIP_CANT_READ		8888 // "cant read"
#define CHIP_UNSUPPORTED	9999 // "unsupported"

/* any other global defs that both user and kernel side need*/
/* big enough mega 802.11 frame + header of DATA_IND*/
#define ST90TDS_ADAP_TX_MSG_SIZE 2500
/* big enough mega 802.11 frame + header of DATA_IND*/
#define ST90TDS_ADAP_RX_MSG_SIZE 2500
#define ST90TDS_ADAP_MGT_MSG_SIZE 512

/* SPI/SDIO Test defs*/
#define SPI_TEST_STATUS_OK 0
#define SPI_TEST_STATUS_DATA_ERROR 1
#define SPI_TEST_STATUS_INVALID_PARAMETER 2
#define SPI_TEST_STATUS_ERROR 3
#define SPI_TEST_MEM_SIZE 0x1000 /* 4k bytes */
#define SPI_TEST_DATA SPI_TEST_MEM_SIZE
#define SDIO_TEST_MMC_OPEN 0x000000EE /* if CMD = EE, open */
#define SDIO_TEST_MMC_CLOSE 0x000000FF /* if CMD = FF, close */

/* SPI/SDIO test operations (OPs)*/
#define SPI_TEST_OP_SPI_READ 0
#define SPI_TEST_OP_SPI_READ_DMA 1
#define SPI_TEST_OP_SPI_WRITE 2
#define SPI_TEST_OP_SPI_WRITE_DMA 3
#define SPI_TEST_OP_MEM_WRITE 4
#define SPI_TEST_OP_INC_COUNT 5
#define SPI_TEST_OP_GO_TO 6
#define SPI_TEST_OP_WAIT_IRQ 7
#define SPI_TEST_OP_READ_COMP 8

#define SPI_TEST_DATA_SIZE 16

/*SDIO Clock FREQ*/
#define SDIO_CLK__400KHZ 0
#define SDIO_CLK__800KHZ 1
#define SDIO_CLK__4_16MHZ 2
#define SDIO_CLK__6_25MHZ 3
#define SDIO_CLK__12_5MHZ 4
#define SDIO_CLK__25MHZ 5

/*SPI Clock FREQ*/
#define SPI_CLK_200KHZ 1
#define SPI_CLK_6_25MHZ 2
#define SPI_CLK_13MHZ 3
#define SPI_CLK_26MHZ 4
#define SPI_CLK_52MHZ 5

/*Platform Type*/
#define PLATFORM_VERSATILE 0
#define PLATFORM_SILICON 1

/*Custom Cmd*/
#define CUSTOM_CMD__READ_SRAM 1
#define CUSTOM_CMD__WRITE_SRAM 2
#define CUSTOM_CMD__READ_CCCR 3
#define CUSTOM_CMD__WRITE_CCCR 4
#define CUSTOM_CMD__WLAN_SUSPEND_RESUME_TEST__DONT_SEND_SUSPEND_CMD 5
#define CUSTOM_CMD__WLAN_SUSPEND_RESUME_TEST__SEND_SUSPEND_CMD 6
#define CUSTOM_CMD__TEST 100
#define CUSTOM_CMD_WRITE_APB 8
#define CUSTOM_CMD_WRITE_AHB 9
#define CUSTOM_CMD_READ_APB 10
#define CUSTOM_CMD_READ_AHB 11

#define ST90TDS_DEVICE_OPT_SBUS_MODE 0x00
#define ST90TDS_DEVICE_OPT_SPI_FREQ 0x01
#define ST90TDS_DEVICE_OPT_SDIO_FREQ 0x02
#define ST90TDS_DEVICE_OPT_SLEEP_MODE 0x03
#define ST90TDS_DEVICE_OPT_WLAN_RESET 0x04
#define ST90TDS_DEVICE_OPT_JTAG_UART 0x05
#define ST90TDS_DEVICE_OPT_HIF_DRIVE_STRENGTH 0x08

/* data structures we need to pass with IOCTL_ST90TDS_REQ_CNF*/
typedef struct
{
	uint32 msg_id; /* common to all ST90Tds internal message*/
	/* message to be followed*/
} st90tds_msg_t;

/* spi test structures*/
typedef struct
{
	uint32 id;
	uint32 pad[5];
	uint16 data[SPI_TEST_DATA_SIZE/2];
} spi_op_common_t;

typedef struct
{
	uint32 id;
	uint32 addr;
	uint32 length;
	uint32 offset;
	uint32 pad[2];
	/* if offset = SPI_TEST_DATA, then use the following data;
	otherwise, it refers to the memory in the test driver.
	The test driver has mem[SPI_TEST_MEM_SIZE], initialised to 00 01 02,
	*/
	uint16 data[SPI_TEST_DATA_SIZE/2];
} spi_op_block_t;

typedef struct
{
	uint32 id;
	uint32 op_index;
	uint32 pad[4];
	uint16 data[SPI_TEST_DATA_SIZE/2];
} spi_op_go_to_t;

typedef struct
{
	uint32 id;
	uint32 count_max;
	uint32 pad[4];
	uint16 data[SPI_TEST_DATA_SIZE/2];
} spi_op_inc_count_t;

typedef struct
{
	uint32 id;
	uint32 wait_in_ms;
	uint32 pad[4];
	uint16 data[SPI_TEST_DATA_SIZE/2];
} spi_op_wait_irq_t;

typedef struct
{
	uint32 id;
	uint32 address; /* register address */
	uint32 reg_lenth; /* 2 or 4 bytes */
	uint32 max_retry; /* number retrials before failing it */
	uint32 data_to_match;/* bit pattern to match with */
	uint32 data_mask; /* register mask eg 0x0000FFFF for 16 bit register*/
	uint16 data[SPI_TEST_DATA_SIZE/2];
} spi_op_read_compare_t;

typedef union spi_test_op_s
{
	spi_op_common_t common;
	spi_op_block_t block;
	spi_op_inc_count_t inc_count;
	spi_op_go_to_t go_to;
	spi_op_wait_irq_t wait_irq;
	spi_op_read_compare_t read_comp;
} spi_test_op_t;

typedef struct
{
	uint32 msg_id;
	uint32 num_ops;
	uint32 op_start;
	uint32 count_start;
} spi_test_req_t;

typedef struct spi_test_cnf_s
{
	uint32 msg_id;
	uint32 status;
	uint32 op_index;
	uint32 data_index;
	uint32 read_data;
	uint32 read_data2;
	uint32 ref_data;
	uint32 loop_count;
} spi_test_cnf_t;

/* other CNF data structures */
typedef struct
{
	uint32 msg_id;
	uint32 offset;
	uint32 length;
} st90tds_iomem_t;

typedef struct
{
	uint32 msg_id;
	uint32 bus_mode; /* 0 SDIO, 1 SPI */
	/* 0/1/2 SDIO: 1-bit/4-bit/8-bit, SPI: Nok/EMP/EMP + Read Framining */
	uint32 bus_type;
} st90tds_adap_config_t;

typedef struct
{
	uint32 msg_id;
	/* "command" will have value 0 to FF, an sdio command, EE and FF for
	*  open and close */
	uint32 command;
	/* argument register contents that goes with the command */
	uint32 argument;
} st90tds_send_sdio_cmd_t;

typedef struct
{
	uint32 msg_id;
	uint32 rounds;
	uint32 length;
} st90tds_pci_test_t;

typedef struct
{
	uint32 msg_id;
	uint32 image_size;
	/* image data to follow */
} st90tds_start_t;

/* Added the structure for Device test */
typedef struct
{
	uint32 msg_id;
	uint32 flags;
	uint32 parameter;
	uint32 image_size;
	uint8 test_data[128];
	/* image data to follow */
} st90tds_start_device_t;

typedef struct
{
	uint32 msg_id;
	uint32 address;
	uint32 length;
	/* data to follow */
} st90tds_read_mem_t;

typedef struct
{
	uint32 msg_id;
	uint32 address;
	uint32 length;
	/* data to follow */
} st90tds_write_mem_t;

typedef struct
{
	uint32 msg_id;
	uint32 freq;
	uint32 delay;
} st90tds_adap_spisdio_clk_freq_t;

typedef struct
{
	uint32 address;
	uint32 len;
	uint32 data[1000];
} st90tds_write_aha_reg_req_t;

typedef struct
{
	uint32 status;
} st90tds_write_aha_reg_resp_t;

typedef struct
{
	st90tds_write_aha_reg_req_t req;
	st90tds_write_aha_reg_resp_t resp;
} st90tds_write_aha_reg_req_resp_t;

typedef struct
{
	uint32 address;
	uint32 len;
} st90tds_read_aha_reg_req_t;

typedef struct
{
	uint32 status;
	uint32 data[1000];
} st90tds_read_aha_reg_resp_t;

typedef struct
{
	st90tds_read_aha_reg_req_t req;
	st90tds_read_aha_reg_resp_t resp;
} st90tds_read_aha_reg_req_resp_t;

typedef struct
{
	uint32 param1;
	uint32 param2;
	uint32 param3;
	uint32 param4;
} st90tds_custom_cmd_t;

/* Structure to enable the firmware debug Prints */
typedef struct st90tds_set_fw_dbg_opts_s
{
	uint32 msg_id;
	uint32 flags;
} st90tds_set_fw_dbg_opts_t;

typedef struct st90tds_device_opt_sbus_mode_s
{
	uint32 bus_mode; /* 0 SDIO, 1 SPI */
	/* bus_type is 0/1/2 SDIO: 1-bit/4-bit/8-bit,
	SPI: Nok/EMP/EMP + Read Framing */
	uint32 bus_type;
} st90tds_device_opt_sbus_mode_t;

typedef struct st90tds_device_opt_sleep_mode_s
{
	uint32 sleep_enable;
} st90tds_device_opt_sleep_mode_t;

typedef struct st90tds_device_opt_hif_strength_s
{
	__u8 hif_strength;
} st90tds_device_opt_hif_strength_t;

typedef struct st90tds_device_opt_spi_freq_s
{
	uint32 freq;
	uint32 delay;
} st90tds_device_opt_spi_freq_t;

typedef struct st90tds_device_opt_sdio_freq_s
{
	uint32 freq;
} st90tds_device_opt_sdio_freq_t;

typedef struct st90tds_device_opt_generic_s
{
	uint32 data[2];
} st90tds_device_opt_generic_t;

typedef struct st90tds_set_wlan_toggle_s
{
	bool wlan_reset;
} st90tds_set_wlan_toggle_t;

typedef struct st90tds_device_opt_jtag_uart_s
{
	uint32 jtag_val;
	uint32 uart_val;
} st90tds_device_opt_jtag_uart_t;

typedef struct st90tds_set_device_opt_s
{
	uint32 msg_id;
	uint32 opt_id;
	union {
		st90tds_device_opt_generic_t generic;
		st90tds_device_opt_sbus_mode_t sbus_mode;
		st90tds_device_opt_spi_freq_t spi_freq;
		st90tds_device_opt_sdio_freq_t sdio_freq;
		st90tds_device_opt_sleep_mode_t sleep_mode;
		st90tds_device_opt_hif_strength_t hif_strength;
		st90tds_set_wlan_toggle_t wlan_reset_enable;
		st90tds_device_opt_jtag_uart_t jtag_uart_configure;
	}opt;
} st90tds_set_device_opt_t;

extern int g_exit;
extern int chip_version_global_int;
extern volatile int g_enter;
extern wait_queue_head_t detect_waitq;
extern int detect_waitq_cond_flag;
extern struct bootloader_1260 bt_ptr;

#define STATUS_SUCCESS 0

int cw1200_detect_card(void);

#endif /* _ST90TDS_IF_H */
