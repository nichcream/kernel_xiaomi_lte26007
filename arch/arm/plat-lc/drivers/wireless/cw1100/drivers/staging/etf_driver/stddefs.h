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
* stddefs.h
*
* ST standard definitions for WIN32 platform.
*
***********************************************************************/

#ifndef _STDDEFS_H
#define _STDDEFS_H

#define PUBLIC
#define PRIVATE static

#ifndef _USER_TYPES
#define _USER_TYPES
typedef unsigned char uint8;
typedef signed char sint8;
typedef unsigned short uint16;
typedef signed short sint16;
typedef unsigned int uint32;
typedef signed int sint32;

typedef unsigned long long int uint64;
typedef signed long long sint64;
#endif /* _USER_TYPES */

#define PACKED
#define WLAN_POLL_ONLY

#define WRITE_ADAPTER_TX_MAX_NUM_RETRIES 5 /*try 5 times to get a tx buffer before failing */

#define WLAN_ENABLE__ENABLE_ON_DRIVER_LOAD_CUT1 /*define it if you want to enable WLAN_Enable to be set as 1 on driver load, and 0 on driver unload */
#define WLAN_ENABLE__RESET_ON_FW_DNLD_CUT1 /*define it if you want to disable WLAN_Enable, then enable again before fw download */

#define WLAN_ENABLE__CONTROLLED_BY_SPIWRITE_CMD8 /*define it if you want to enable WLAN_Enable to be set as 1 on driver load, and 0 on driver unload */

#define ENABLE_SEQ_NUM_CHECKING /*define it if you want seq num checking */
#define ENABLE_SEQ_NUM_CHECKING__ABORT_ON_FAILURE
#define ABORT_ON_RECEIVING_IMPOSSIBLE_LARGE_PKT
#define SDIO_SEND_CCCR_IO_ABORT_ON_DETECTING_CMD53_CRC_FAILURE

#define DIRECTMODE_TESTS_TO_WORK_EVEN_IN_QUEUE_MODE /*define it to perform direct mode r/w using spi_test/sdio_test even after firmware download */


/*to check corruption over spi */
#ifdef HIF_DEBUG_CRC_CHECK
	/*debug case */
#define HIF_DEBUG_CRC_CHECK_SIZE 4 /*checksum is 4 bytes + 4 bytes of signature at the end of pkt */
#define HIF_DEBUG_CRC_CHECK_MARKER 0xAAAA
#else
	/*normal case */
#define HIF_DEBUG_CRC_CHECK_SIZE 0 /*no checksum in normal case */
#endif

//SDIO
#define DEBUG_SDIO_STOP_ON_RX_OVERRUN_CQ2020 //stop driver if rx overrun is detected
#define SDIO_START_MCI_CLOCK_AFTER_TXFIO_IS_FULL__AVOID_TX_UNDERRUN_CQ2073 //tx underrun opt

#define SDIO_CHECK_EXEC_FLAG_AFTER_CMD53_DATA_TRANSFER_HAS_FINISHED
#define DEBUG_SDIO_ENABLE_EXEC_FLAG_CHECKING_SUSPEND_RESUME
#define DEBUG_SDIO_ENABLE_EXEC_FLAG_CHECKING_SUSPEND_RESUME2

#define HIF_USE_ALL_INOUT_CHANNELS /*use 32 inp channels */
#define SDIO_DISABLE_ALL_INTERRUPTS_WHEN_ACCESSING_SDIO_FIFO_CQ_1853 //try workaround for CQ 1853
#define SDIO_SEND_CMD53_BYTE_MODE_USING_TRICK //since CRC is not supported in byte mode in AHA, we send the data in block mode even when byte mode is selected //shud be defined if AHA does not generate CRC in byte mode
#define DISABLE_ALL_INTERRUPTS_WHEN_ACCESSING_REG_USING_SPISDIO_TEST

#define HIF_USE_ALL_INOUT_CHANNELS //use 32 inp channels

#define SDIO_MAX_BLK_SZ 2048
#define SDIO_MIN_BLK_SZ 4 /* can change it to 128 */
#define SDIO_MAX_NO_BLKS 511

#define DRIVE_SPI_CS_BY_SW //define it to drive SPI_CS by sw
#define DEBUG_SET_TRIGGER_ON_DETECTING_CORRUPTED_LOOPBACK_MESSAGE //define it to trigger on receiving corrupted loopback message
#define DEBUG_SET_TRIGGER_ON_DETECTING_ANY_HIF_FAILURE //define it to trigger on execution of WDEV_StopCommunicationDueToHifFailure()

//trigger line to use
#define TRIGGER_PIN_0 (1<<3) //we have 4 gpios for triggering (bits 3-6 of local gpio reg in aha)
#define TRIGGER_PIN_1 (1<<4)
#define TRIGGER_PIN_2 (1<<5)
#define TRIGGER_PIN_3 (1<<6)
//make it high or low
#define TRIGGER_LINE_LOW 0 //write 0 to it to make it low
#define TRIGGER_LINE_HIGH 1 //write 0 to it to make it high

//for triggerring logic analyzer
#define TRIGGER_LINE_FOR_DETECTING_SPURIOUS_INTERRUPT TRIGGER_PIN_0
#define TRIGGER_LINE_FOR_DETECTING_CORRUPTED_LOOPBACK_MESSAGE TRIGGER_PIN_0
#define TRIGGER_LINE_FOR_DETECTING_SDIO_CMD_FAILURE TRIGGER_PIN_0
//#define TRIGGER_LINE_FOR_DETECTING_BT_OR_WLAN TRIGGER_PIN_0
#define TRIGGER_LINE_FOR_DETECTING_ANY_HIF_FAILURE TRIGGER_PIN_0
#define TRIGGER_LINE_FOR_DETECTING_CMD53_SUSPEND_RESUME TRIGGER_PIN_0
#define TRIGGER_LINE_FOR_DETECTING_SUSPEND_RESUME_FAILURE TRIGGER_PIN_1
#define TRIGGER_LINE_FOR_DETECTING_SPI_SPURIOS_INTR__DRIVEN_ON_INTR_EN_DISABLE TRIGGER_PIN_1
#define TRIGGER_LINE_FOR_DETECTING_SPI_SPURIOS_INTR__DRIVEN_ON_INTR_EN_DISABLE TRIGGER_PIN_1
#define TRIGGER_LINE_FOR_DETECTING_EXTRA_INTR_ON_SPI_PIGGYBACK__DRIVEN_ON_SPI_READ_START_END TRIGGER_PIN_0//TRIGGER_PIN_1
#define TRIGGER_LINE_FOR_DETECTING_EXTRA_INTR_ON_SPI_PIGGYBACK TRIGGER_PIN_0
#define TRIGGER_LINE_FOR_DETECTING_PROBLEM__WR_ADAPTER_SIGNALLED_EXTRA_TIME TRIGGER_PIN_0
#define TRIGGER_LINE_FOR_DETECTING_PROBLEM__SDIO_START_BIT_ERROR TRIGGER_PIN_0

//for knowing r/w
#define TRIGGER_LINE_FOR_DETECTING_RW_TO_FN0_WLAN_BT_COMMON TRIGGER_PIN_1 //trigger line 1
#define TRIGGER_LINE_FOR_DETECTING_RW_TO_FN1_WLAN TRIGGER_PIN_2 //trigger line 2
#define TRIGGER_LINE_FOR_DETECTING_RW_TO_FN2_BT TRIGGER_PIN_3 //trigger line 3
//make it high or low
#define TRIGGER_RW_START TRIGGER_LINE_HIGH
#define TRIGGER_RW_END TRIGGER_LINE_LOW

//Spi optimisation
#define SPI_OPT3__MIN_ELEMENTS_IN_TX_FIFO 4
//#define SPI_ACCESS_32BIT
#define SPI_ACCESS_MODE__16B_OPT1 1
#define SPI_ACCESS_MODE__16B_OPT2 2
#define SPI_ACCESS_MODE__16B_OPT3 3
#define SPI_ACCESS_MODE__32B 11
#define SPI_ACCESS_MODE__TO_USE SPI_ACCESS_MODE__16B_OPT3

#endif /* _STDDEFS_H */
