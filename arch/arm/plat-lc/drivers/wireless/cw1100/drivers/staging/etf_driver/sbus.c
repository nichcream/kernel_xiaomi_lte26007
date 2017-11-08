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
* sbus.c
*
* This module interfaces with the Linux Kernel MMC/SDIO stack.
*
****************************************************************************/

#include <linux/mmc/sdio_func.h>
#include <linux/delay.h>
#include <linux/stddef.h>
#include <asm/atomic.h>
#include <linux/gpio.h>
#include <linux/mmc/sdio.h>
#include <plat/ste-cw1200.h>

#include "hwio.h"
#include "sbus.h"
#include "cw1200_common.h"
#include "sbus_wrapper.h"
#include "debug.h"
#include "st90tds_if.h"

int irq_regd = 0;
int sbus_wq_created = 0;
int rx_wq_created = 0;

#define ETFTEST
#define MAX_WR_BUF_NUM 31
#define MAX_RD_BUF_NUM 4

static void SBUS_bh(struct work_struct *work);
CW1200_STATUS_E SBUS_Set_Prefetch_AHB (struct CW1200_priv *priv);
CW1200_STATUS_E SBUS_Sleep_Device(struct CW1200_priv *priv);
int cw1200_chip_detect(struct CW1200_priv *, int );
static CW1200_STATUS_E enqueue_rx_data(struct CW1200_priv *,
		struct sk_buff *, uint8_t *);
void async_exit(void);

#ifdef GPIO_BASED_IRQ
static irqreturn_t cw1200_gpio_irq(int irq, void *dev_id)
{
	struct CW1200_priv *priv = dev_id;

	printk("GPIO:cw1200_sbus_interrupt Called \n");

	atomic_xchg(&(priv->Interrupt_From_Device), TRUE);
	queue_work(priv->sbus_WQ, &priv->sbus_work);
	return IRQ_HANDLED;
}
#endif

/**
* \fn inline void Increment_Wr_Buffer_Number(struct CW1200_priv *priv)
*
* \brief This function increments the WRITE buffer no.
* Please see HIF MAD doc for details.
*
* \param priv - pointer to device private structure.
*
* \return - None
*/
inline void Increment_Wr_Buffer_Number(struct CW1200_priv *priv)
{
	if (priv->sdio_wr_buf_num_qmode == MAX_WR_BUF_NUM)
		priv->sdio_wr_buf_num_qmode = 0; /* make it 0 - 31*/
	else
		priv->sdio_wr_buf_num_qmode++;
}

/**
* \fn inline void Increment_Rd_Buffer_Number(struct CW1200_priv *priv)
*
* \brief This function increements the READ buffer no.
* Please see HIF MAD doc for details.
*
* \param priv - pointer to device private structure.
*
*/
inline void Increment_Rd_Buffer_Number(struct CW1200_priv *priv)
{
	if (priv->sdio_rd_buf_num_qmode == MAX_RD_BUF_NUM)
		priv->sdio_rd_buf_num_qmode = 1; /* make it 0 - 4*/
	else
		priv->sdio_rd_buf_num_qmode++;
}

/**
* \fn int32_t SBUS_SDIO_RW_Reg(struct stlc9000_priv *priv,
* uint32 addr, char *rw_buf, uint16 length, int32 read_write)
*
*\brief This function reads/writes a register from the devuce
*
* \param priv - pointer to device private structure.
*
* \param addr - Register address to read/write.
*
* \param rw_buf - Pointer to buffer containing the write data and the
* buffer to store the data read.
*
* \param length - Length of the buffer to read/write
*
* \param read_write - Operation to perform - READ/WRITE
*
* \return - Status of the operation
*/
CW1200_STATUS_E SBUS_SDIO_RW_Reg(struct CW1200_priv *priv, uint32_t addr,
		uint8_t *rw_buf, uint16_t length,
		int32_t read_write)
{
	uint32_t sdio_reg_addr_17bit;
	int32_t retval;
	uint32_t retry = 0;

	/*Check if buffer is aligned to 4 byte boundary */
	if ((((unsigned)rw_buf) & 3) != 0) {
		DEBUG(DBG_SBUS, "FATAL Error:Buffer UnAligned \n");
		return ERROR_BUF_UNALIGNED;
	}
	/*Convert to SDIO Register Address */
	addr = SPI_REG_ADDR_TO_SDIO(addr);
	sdio_reg_addr_17bit = SDIO_ADDR17BIT(0, 0, 0, addr);
	sdio_claim_host(priv->func);

	if (SDIO_READ == read_write) {
		/*READ*/
		while (retry < MAX_RETRY) {
			retval = sbus_memcpy_fromio(priv->func, rw_buf,
					sdio_reg_addr_17bit, length);
			if (unlikely(retval)) {
				retry++;
				mdelay(1);
			} else {
				break;
			}
		}
	} else {
		/*WRITE*/
		while (retry < MAX_RETRY) {
			retval = sbus_memcpy_toio(priv->func,
					sdio_reg_addr_17bit,
					rw_buf, length);
			if (unlikely(retval)) {
				retry++;
				mdelay(1);
			} else {
				break;
			}
		}
	}
	sdio_release_host(priv->func);

	if (unlikely(retval)) {
		DEBUG(DBG_ERROR, "SDIO Error [%d] \n", retval);
		return ERROR_SDIO;
	} else {
		return SUCCESS;
	}
}

/*! \fn CW1200_STATUS_E SBUS_SramRead_AHB (struct CW1200_priv *priv,
* uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
*
* \brief This function from AHB-SRAM.
*
* \param priv - pointer to device private structure.
*
* \param base_addr - Base address in the RAM from where to read from.
*
* \param buffer - Pointer to the buffer
*
* \param byte_count - Length of the buffer to read/write
*
* \return - Status of the operation
*/
CW1200_STATUS_E SBUS_SramRead_AHB (struct CW1200_priv *priv,
		uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
{
	uint32_t retval;

	if ((byte_count/2) > 0xfff) {
		DEBUG (DBG_ERROR, "%s: ERROR: Cannot write more"
				"than 0xfff words (requested = %d)\n",
				__func__, ((byte_count/2)));
		return ERROR_INVALID_PARAMETERS;
	}

	retval = SBUS_SDIO_RW_Reg (priv, ST90TDS_SRAM_BASE_ADDR_REG_ID,
			(uint8_t *)&base_addr, BIT_32_REG, SDIO_WRITE);
	if (retval) {
		DEBUG (DBG_ERROR, "%s: sbus write failed, error=%d \n",
				__func__, retval);
		return ERROR_SDIO;
	}

	if (SBUS_Set_Prefetch_AHB (priv))
		return ERROR;

	retval = SBUS_SDIO_RW_Reg (priv, ST90TDS_AHB_DPORT_REG_ID, buffer,
			byte_count, SDIO_READ);

	if (retval) {
		DEBUG (DBG_ERROR, "%s: sbus write failed, error=%d \n",
				__func__, retval);
		return ERROR_SDIO;
	}

	return SUCCESS;
}

/**
* \fn CW1200_STATUS_E SBUS_SramWrite_AHB(struct CW1200_priv *priv,
* uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
*
* \brief This function writes directly to AHB RAM.
*
* \param priv - pointer to device private structure.
*
* \param base_addr - Base address in the RAM from where to upload firmware
*
* \param buffer - Pointer to firmware chunk
*
* \param byte_count - Length of the buffer to read/write
*
* \return - Status of the operation
*/
CW1200_STATUS_E SBUS_SramWrite_AHB(struct CW1200_priv *priv,
		uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
{
	int32_t retval;

	if ((byte_count/2) > 0xfff) {
		DEBUG(DBG_ERROR, "%s: ERROR: Cannot write"
				" more than 0xfff words (requested = %d)\n",
				__func__, ((byte_count/2)));
		return ERROR_INVALID_PARAMETERS;
	}

	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_SRAM_BASE_ADDR_REG_ID,
			(uint8_t *)&base_addr, BIT_32_REG, SDIO_WRITE);
	if (retval) {
		DEBUG(DBG_ERROR, "%s: sbus write failed, "
				"error =%d \n", __func__, retval);
		return ERROR_SDIO;
	}

	if (byte_count < (DOWNLOAD_BLOCK_SIZE - 1)) {
		retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_AHB_DPORT_REG_ID,
				buffer, byte_count, SDIO_WRITE);
		if (retval) {
			DEBUG(DBG_ERROR, "%s: sbus write "
					"failed, error =%d \n",
					__func__, retval);
			return ERROR_SDIO;
		}
	} else {
		mdelay(1);
		/*Right now using Multi Byte */
		retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_AHB_DPORT_REG_ID,
				buffer, byte_count, SDIO_WRITE);
		if (retval) {
			DEBUG(DBG_ERROR, "%s:write failed,"
					" error =%d \n", __func__, retval);
			return ERROR_SDIO;
		}
	}
	return SUCCESS;
}

/**
* \fn CW1200_STATUS_E SBUS_Set_Prefetch(struct CW1200_priv *priv)
*
* \brief This function sets the prefect bit and waits for it to get cleared
*
* \param priv - pointer to device private structure.
*
*/
CW1200_STATUS_E SBUS_Set_Prefetch(struct CW1200_priv *priv)
{
	uint32_t config_reg_val = 0;
	uint32_t count = 0;

	/*Read CONFIG Register Value - We will read 32 bits*/
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
				(uint8_t *)&config_reg_val, BIT_32_REG,
				SDIO_READ))
		return ERROR_SDIO;

	config_reg_val = config_reg_val | ST90TDS_CONFIG_PFETCH_BIT;
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
				(uint8_t *)&config_reg_val,
				BIT_32_REG, SDIO_WRITE))
		return ERROR_SDIO;

	/*Check for PRE-FETCH bit to be cleared */
	for (count = 0; count < 20; count++) {
		mdelay(count);
		/*Read CONFIG Register Value - We will read 32 bits*/
		if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
					(uint8_t *)&config_reg_val,
					BIT_32_REG, SDIO_READ))
			return ERROR_SDIO;

		if (!(config_reg_val & ST90TDS_CONFIG_PFETCH_BIT))
			break;
	}

	if (count >= 20) {
		DEBUG(DBG_ERROR, "Prefetch bit not cleared \n");
		return ERROR;
	}

	return SUCCESS;
}

/*! \fn CW1200_STATUS_E SBUS_Set_Prefetch_AHB (struct CW1200_priv *priv)
brief This function sets the prefect bit and waits for it to get cleared
param priv - pointer to device private structure.
*/
CW1200_STATUS_E SBUS_Set_Prefetch_AHB (struct CW1200_priv *priv)
{
	uint32_t config_reg_val = 0;
	uint32_t count = 0;

	/*Read CONFIG Register Value - We will read 32 bits*/
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
				(uint8_t *)&config_reg_val,
				BIT_32_REG, SDIO_READ))
		return ERROR_SDIO;

	config_reg_val = config_reg_val | ST90TDS_CONFIG_AHB_PFETCH_BIT;
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
				(uint8_t *)&config_reg_val,
				BIT_32_REG, SDIO_WRITE))
		return ERROR_SDIO;

	/*Check for PRE-FETCH bit to be cleared */
	for (count = 0; count < 20; count++) {
		mdelay (1);
		/*Read CONFIG Register Value - We will read 32 bits*/
		if (SBUS_SDIO_RW_Reg (priv, ST90TDS_CONFIG_REG_ID,
					(uint8_t *)&config_reg_val,
					BIT_32_REG, SDIO_READ))
			return ERROR_SDIO;

		if (!(config_reg_val & ST90TDS_CONFIG_AHB_PFETCH_BIT))
			break;
	}

	if (count >= 20) {
		DEBUG (DBG_ERROR, "AHB Prefetch bit not cleared \n");
		return ERROR;
	}

	return SUCCESS;
}

/**
* \fn CW1200_STATUS_E SBUS_SramWrite_APB(struct CW1200_priv *priv,
* uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
*
* \brief This function writes directly to SRAM.
*
* \param priv - pointer to device private structure.
*
* \param base_addr - Base address in the RAM from where to write to.
*
* \param buffer - Pointer to the buffer
*
* \param byte_count - Length of the buffer to read/write
*
* \return - Status of the operation
*/
CW1200_STATUS_E SBUS_SramWrite_APB(struct CW1200_priv *priv,
		uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
{
	int32_t retval;

	if ((byte_count/2) > 0xfff) {
		DEBUG(DBG_ERROR, "%s:ERROR:Cannot write more than"
				"0xfff words(requested = %d)\n",
				__func__, ((byte_count/2)));
		return ERROR_INVALID_PARAMETERS;
	}
#ifdef MOP_WORKAROUND
	if ((byte_count > 4) && (byte_count <= DOWNLOAD_BLOCK_SIZE))
		byte_count = DOWNLOAD_BLOCK_SIZE;
#endif

	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_SRAM_BASE_ADDR_REG_ID,
			(uint8_t *)&base_addr, BIT_32_REG, SDIO_WRITE);
	if (retval) {
		DEBUG(DBG_SBUS, "SBUS_SramWrite_APB:"
				"sbus write failed, error=%d \n", retval);
		return ERROR_SDIO;
	}

	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_SRAM_DPORT_REG_ID,
			buffer, byte_count, SDIO_WRITE);
	if (retval) {
		DEBUG(DBG_SBUS, "SBUS_SramWrite_APB: sbus"
				"write failed, error=%d \n", retval);
		return ERROR_SDIO;
	}

	return SUCCESS;
}

/**
* \fn CW1200_STATUS_E SBUS_SramRead_APB(struct CW1200_priv *priv,
* uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
*
* \brief This function from APB-SRAM.
*
* \param priv - pointer to device private structure.
*
* \param base_addr - Base address in the RAM from where to read from.
*
* \param buffer - Pointer to the buffer
*
* \param byte_count - Length of the buffer to read/write
*
* \return - Status of the operation
*/
CW1200_STATUS_E SBUS_SramRead_APB(struct CW1200_priv *priv,
		uint32_t base_addr, uint8_t *buffer, uint32_t byte_count)
{
	int32_t retval;

	if ((byte_count/2) > 0xfff) {
		DEBUG(DBG_ERROR, "%s: ERROR: Cannot write more than 0xfff"
				"words (requested = %d)\n",
				__func__, ((byte_count/2)));
		return ERROR_INVALID_PARAMETERS;
	}

	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_SRAM_BASE_ADDR_REG_ID,
			(uint8_t *)&base_addr, BIT_32_REG, SDIO_WRITE);
	if (retval) {
		DEBUG(DBG_ERROR, "%s: sbus write failed"
				", error =%d \n", __func__, retval);
		return ERROR_SDIO;
	}

	if (SBUS_Set_Prefetch(priv))
		return ERROR;

	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_SRAM_DPORT_REG_ID,
			buffer, byte_count, SDIO_READ);
	if (retval) {
		DEBUG(DBG_ERROR, "%s: sbus write failed, "
				"error =%d \n", __func__, retval);
		return ERROR_SDIO;
	}
	return SUCCESS;
}

/** \fn CW1200_STATUS_E SBUS_SDIOReadWrite_QUEUE_Reg(struct CW1200_priv *priv,
uint32_t addr, uint8_t *rw_buf, *uint16_t length, int32_t read_write)
*
* \brief This function reads/writes device QUEUE register
*
* \param priv - pointer to device private structure.
*
* \param rw_buf - pointer to buffer containing the write data or the buffer
*to copy the data read
*
* \param length - Data read/write length
*
* \param read_write - READor WRITE operation.
*
* \return - Status of the operation
*/
CW1200_STATUS_E SBUS_SDIOReadWrite_QUEUE_Reg(struct CW1200_priv *priv,
		uint8_t *rw_buf, uint16_t length, int32_t read_write)
{
	uint32_t sdio_reg_addr_17bit;
	int32_t retval;
	int32_t addr = 0;
	uint32_t retry = 0;
	int32_t num_blocks = 0;
	int32_t rem = 0;

	/*Convert to SDIO Register Address */
	addr = SPI_REG_ADDR_TO_SDIO(ST90TDS_IN_OUT_QUEUE_REG_ID);

	if (length >= SDIO_BLOCK_SIZE) {
		num_blocks = length/SDIO_BLOCK_SIZE;
		if ((length - (num_blocks * SDIO_BLOCK_SIZE)) > 0)
			num_blocks = num_blocks + 1;
	}

	if (num_blocks == 0) { /*To make the Multi-Byte read a multiple of 4*/
		rem = length % 4;
		if (unlikely(rem))
			length += rem;
	} else { /*To make the Multi-Block reads a multiple of 512 */
		length = num_blocks*SDIO_BLOCK_SIZE;
	}

	sdio_claim_host(priv->func);

	if (read_write == SDIO_READ) {
		/*READ*/
		sdio_reg_addr_17bit =
			SDIO_ADDR17BIT(priv->sdio_rd_buf_num_qmode,
					0, 0, addr);
		while (retry < MAX_RETRY) {
			retval = sbus_memcpy_fromio(priv->func, rw_buf,
					sdio_reg_addr_17bit, length);
			if (unlikely(retval)) {
				retry++;
				mdelay(1);
			} else {
				break;
			}
		}
	} else {
		/*WRITE*/
		sdio_reg_addr_17bit =
			SDIO_ADDR17BIT(priv->sdio_wr_buf_num_qmode,
					0, 0, addr);
		while (retry < MAX_RETRY) {
			retval = sbus_memcpy_toio(priv->func,
					sdio_reg_addr_17bit, rw_buf, length);
			if (unlikely(retval)) {
				retry++;
				mdelay(1);
			} else {
				Increment_Wr_Buffer_Number(priv);
				break;
			}
		}
	}

	sdio_release_host(priv->func);

	/*Retries failed return ERROR to Caller */
	if (unlikely(retval))
		return ERROR_SDIO;
	else
		return SUCCESS;
}

/** \fn CW1200_STATUS_E STLC9000_UploadFirmware(struct stlc9000_priv *priv,
* void *firmware, uint32_t length)
\brief This function uploads firmware to the device
\param priv - pointer to device private structure.
\param firmware - Pointer to the firmware image
\param length - Length of the firmware image.
\return - Status of the operation
*/
CW1200_STATUS_E STLC9000_UploadFirmware(struct CW1200_priv *priv,
		void *firmware, uint32_t fw_length)
{
	uint32_t i;
	uint32_t status = SUCCESS;
	uint8_t *buffer_loc;
	uint32_t num_blocks;
	uint32_t length = 0, remain_length = 0;

	num_blocks = fw_length/DOWNLOAD_BLOCK_SIZE;

	if ((fw_length - (num_blocks * DOWNLOAD_BLOCK_SIZE)) > 0)
		num_blocks = num_blocks + 1;

	buffer_loc = (uint8_t *)firmware;

	for (i = 0; i < num_blocks; i++) {

		remain_length = fw_length - (i * DOWNLOAD_BLOCK_SIZE);
		if (remain_length >= DOWNLOAD_BLOCK_SIZE)
			length = DOWNLOAD_BLOCK_SIZE;
		else
			length = remain_length;

		if (SBUS_SramWrite_AHB(priv,
					(FIRMWARE_DLOAD_ADDR +
					(i*DOWNLOAD_BLOCK_SIZE)),
					buffer_loc, length) != SUCCESS) {
			status = ERROR;
			break;
		}
		buffer_loc += length;
	}

	if (status != ERROR)
		status = SUCCESS;

	return status;
}

/*
* \fn CW1200_STATUS_E CW1200_UploadFirmware(struct stlc9000_priv *priv,
* void *firmware, uint32_t length)
*
* \brief This function uploads firmware to the device
*
* \param priv - pointer to device private structure.
*
* \param firmware - Pointer to the firmware image
*
* \param length - Length of the firmware image.
*
* \return - Status of the operation
*/
CW1200_STATUS_E CW1200_UploadFirmware(struct CW1200_priv *priv,
		uint8_t *firmware, uint32_t fw_length)
{
	uint32_t reg_value = 0;
	download_cntl_t download = {0};
	uint32_t config_reg_val = 0;
	uint32_t count = 0;
	uint32_t num_blocks = 0;
	uint32_t block_size = 0;
	uint8_t *buffer_loc = NULL;
	uint32_t i = 0;

	/* Initializing the download control Area with bootloader Signature */
	reg_value = DOWNLOAD_ARE_YOU_HERE;

	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON +
				DOWNLOAD_IMAGE_SIZE_REG,
				(uint8_t *)&reg_value, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s:SBUS_SramWrite_APB()returned error \n",
				__func__);
		return ERROR_SDIO;
	}

	/*BIT 0 should be set to 1 if the device UART is to be used */
	download.Flags = 0;
	download.Put = 0;
	download.Get = 0;
	download.Status = DOWNLOAD_PENDING;

	reg_value = download.Put;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_PUT_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Put failed \n",
				__func__);
		return ERROR_SDIO;
	}

	reg_value = download.Get;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_GET_REG, (uint8_t *)&reg_value,
				sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Get failed \n",
				__func__);
		return ERROR_SDIO;
	}

	reg_value = download.Status;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_STATUS_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Status Reg"
				" failed \n", __func__);
		return ERROR_SDIO;
	}

	reg_value = download.Flags;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_FLAGS_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Flag Reg failed \n",
				__func__);
		return ERROR_SDIO;
	}

	/*Write the NOP Instruction */
	reg_value = 0xFFF20000;
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_SRAM_BASE_ADDR_REG_ID,
			(uint8_t *)&reg_value, BIT_32_REG, SDIO_WRITE)) {
		DEBUG(DBG_ERROR, "%s:ERROR:SRAM Base Address Reg Failed \n",
			__func__);
		return ERROR_SDIO;
	}

	reg_value = 0xEAFFFFFE;
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_AHB_DPORT_REG_ID,
			(uint8_t *)&reg_value, BIT_32_REG, SDIO_WRITE)) {
		DEBUG(DBG_ERROR, "%s:ERROR:NOP Write Failed \n", __func__);
		return ERROR_SDIO;
	}

	/*Release CPU from RESET */
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val, BIT_32_REG, SDIO_READ)) {
		DEBUG(DBG_ERROR, "%s:ERROR:Read Config Reg Failed \n",
			__func__);
		return ERROR_SDIO;
	}

	config_reg_val = config_reg_val & ST90TDS_CONFIG_CPU_RESET_MASK;
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val, BIT_32_REG, SDIO_WRITE)) {
		DEBUG(DBG_ERROR, "%s:ERROR:Write Config Reg Failed \n",
			__func__);
		return ERROR_SDIO;
	}

	/*Enable Clock */
	config_reg_val = config_reg_val & ST90TDS_CONFIG_CPU_CLK_DIS_MASK;
	if (SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val, BIT_32_REG, SDIO_WRITE)) {
		DEBUG(DBG_ERROR, "%s:ERROR:Write Config Reg Failed \n",
			__func__);
		return ERROR_SDIO;
	}

	DEBUG(DBG_SBUS, "%s: Waiting for Bootloader to be ready \n",
			__func__);
	/*Check if the bootloader is ready */
	for (count = 0; count < 100; count++) {
		mdelay(1);
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_IMAGE_SIZE_REG,
					(uint8_t *)&reg_value, BIT_32_REG))
			return ERROR;

		if (reg_value == DOWNLOAD_I_AM_HERE) {
			DEBUG(DBG_SBUS, "BootLoader Ready \n");
			break;
		}
	} /*End of for loop */

	if (count >= 100) {
		DEBUG(DBG_ERROR, "Bootloader not ready:Timeout \n");
		return ERROR;
	}

	/*Calculcate number of download blocks */
	num_blocks = fw_length/DOWNLOAD_BLOCK_SIZE;

	if ((fw_length - (num_blocks * DOWNLOAD_BLOCK_SIZE)) > 0)
		num_blocks = num_blocks + 1;

	/*Updating the length in Download Ctrl Area */
	download.ImageSize = fw_length;
	reg_value = download.ImageSize;

	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON +
				DOWNLOAD_IMAGE_SIZE_REG,
				(uint8_t *)&reg_value, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s:SBUS_SramWrite_APB()returned error \n",
			__func__);
		return ERROR_SDIO;
	}

	/* Firmware downloading loop*/
	for (i = 0; i < num_blocks; i++) {
		/* check the download status*/
		reg_value = 0;
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_STATUS_REG,
					(uint8_t *)&reg_value, BIT_32_REG)) {
			DEBUG(DBG_ERROR,
				"%s:SBUS_SramRead_APB()returned error \n",
				__func__);
			return ERROR;
		}

		download.Status = reg_value;
		DEBUG(DBG_SBUS, "%s:download status = 0x%08x \n", __func__,
				download.Status);

		if (download.Status != DOWNLOAD_PENDING) {
			/* FW loader reporting an error status*/
			if (download.Status == DOWNLOAD_EXCEPTION) {
				/* read exception data*/
				DEBUG(DBG_ERROR,
					"%s:ERR:Loader DOWNLOAD_EXCEPTION\n",
					__func__);
				SBUS_SramRead_APB(priv,
					PAC_SHARED_MEMORY_SILICON +
					DOWNLOAD_DEBUG_DATA_REG,
					(uint8_t *)&download.DebugData[0],
					DOWNLOAD_DEBUG_DATA_LEN);
			}
			return ERROR;
		}

		/* loop until put - get <= 24K*/
		for (count = 0; count < 200; count++) {
			reg_value = 0;
			if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
						+ DOWNLOAD_GET_REG,
						(uint8_t *)&reg_value,
						BIT_32_REG)) {
				DEBUG(DBG_ERROR, "Sram Read get failed \n");
				return ERROR_SDIO;
			}

			download.Get = reg_value;
			if ((download.Put - download.Get) <=
					(DOWNLOAD_FIFO_SIZE -
					DOWNLOAD_BLOCK_SIZE))
				break; /* OK we can put*/
			mdelay(1); /* 1 Mil sec*/
		}

		if (count >= 200) {
			DEBUG(DBG_ERROR, "%s: PUT-GET timedout \n", __func__);
			return ERROR;
		}
		/* get the block size*/
		if ((download.ImageSize - download.Put) >=
				DOWNLOAD_BLOCK_SIZE)
			block_size = DOWNLOAD_BLOCK_SIZE;
		else
			block_size = download.ImageSize - download.Put;

		buffer_loc = firmware + download.Put; /*In bytes */

		/*send the block to sram */
		if (SBUS_SramWrite_APB(priv,
					PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_FIFO_OFFSET
					+ (download.Put &
					(DOWNLOAD_FIFO_SIZE - 1)),
					buffer_loc, block_size)) {
			DEBUG(DBG_ERROR, "%s:SRAM Write Error \n", __func__);
			return ERROR;
		}

		DEBUG(DBG_SBUS, "WDEV_Start_CW1200: Block %d loaded \n", i);

		/* update the put register*/
		download.Put = download.Put + block_size;
		reg_value = download.Put;
		if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_PUT_REG,
					(uint8_t *)&reg_value, BIT_32_REG)) {
			DEBUG(DBG_ERROR, "%s: Sram Write update put failed\n",
					__func__);
			return ERROR;
		}
	} /*End of firmware download loop*/
	/* wait for the download completion*/
	count = 0;
	reg_value = 0;
	if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_STATUS_REG,
				(uint8_t *)&reg_value, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s: Sram Read failed \n", __func__);
		return ERROR;
	}
	download.Status = reg_value;
	while (download.Status == DOWNLOAD_PENDING) {
		mdelay(5);
		reg_value = 0;
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_STATUS_REG,
					(uint8_t *)&reg_value, BIT_32_REG)) {
			DEBUG(DBG_ERROR, "%s: Sram Read failed \n", __func__);
			return ERROR;
		}
		download.Status = reg_value;
		count++;
		if (count >= 100) {
			DEBUG(DBG_SBUS,
				"%s:Status unchanged after firmware upload\n",
				__func__);
			break;
		}
	}
	if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_GET_REG,
				(uint8_t *)&reg_value, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "Sram Read get failed \n");
		return ERROR_SDIO;
	}
	download.Get = reg_value;
	if (SBUS_SramWrite_APB(priv,
				PAC_SHARED_MEMORY_SILICON + DOWNLOAD_PUT_REG,
				(uint8_t *)&reg_value, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s: Sram Write update put failed \n",
				__func__);
		return ERROR;
	}
	download.Put = reg_value;
	DEBUG(DBG_SBUS, "put register value after download %d, "
			"diff with get register %d\n", download.Put,
			download.Put - download.Get);

	if (download.Status != DOWNLOAD_SUCCESS) {
		DEBUG(DBG_ERROR,
			"%s: download failed: download status = 0x%08x \n",
			__func__, download.Status);
		return ERROR;
	}
	return SUCCESS;
}

/** \fn static void SBUS_BH(struct work_struct *work)
\brief This function implements the SBUS bottom half handler
\param work - pointer to the Work Queue work_struct
\return - None
*/
static void SBUS_bh(struct work_struct *work)
{
	uint32_t cont_reg_val = 0;
	uint32_t read_len = 0;
	uint32_t rx_count = 0;
	uint32_t alloc_len = 0;
	hif_rw_msg_hdr_payload_t *payload;
	HI_STARTUP_IND *startup_ind;
	HI_EXCEPTION_IND *exception_ind;
	char buf[HI_SW_LABEL_MAX + 1];
	uint16_t msgid = 0;
	int32_t retval = SUCCESS;
	struct sk_buff *skb = NULL;
	struct CW1200_priv *priv = container_of(work,
			struct CW1200_priv, sbus_work);
#ifdef MOP_WORKAROUND
	int tmp;
#endif

	DEBUG(DBG_FUNC, "%s Called\n", __func__);
	/* protect skb data till its read by readadapter and then free*/

	if (g_exit) {
		DEBUG(DBG_SBUS, "%s: Exit set found\n", __func__);
		goto err_out;
	}

	/**********************Receive ******************************/
	/*If the device raised an Interrupt enter the RX code */
	if (TRUE == atomic_read(&(priv->Interrupt_From_Device))) {
		/*Clear the Local Interrupt Flag */
		DEBUG(DBG_SBUS, "%s Interrupt from device\n", __func__);
		atomic_set(&(priv->Interrupt_From_Device), FALSE);
		/*Read Control register to retrieve the buffer len*/
		retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONTROL_REG_ID,
				(char *)&cont_reg_val, BIT_16_REG, SDIO_READ);
		if (retval) {
			DEBUG(DBG_SBUS, "SDIO Read/Write Error \n");
			goto err_out;
		}
		DEBUG(DBG_FUNC, "%s read length= 0x%x\n",
				__func__, cont_reg_val);
		cont_reg_val = (uint16_t)(cont_reg_val
				& ST90TDS_SRAM_BASE_LSB_MASK);

		/*Extract next message length(in words)from CONTROL register*/
		read_len = SPI_CONT_NEXT_LENGTH(cont_reg_val);
		/*Covert length to length in BYTES */
		read_len = read_len * 2;
		/*For restarting Piggyback Jump here */
		rx_count = 0;

#ifdef PIGGYBACK
		/*Repeat the RX Loop 2 times.Imperical Value.
		Can be changed legacy code*/
		while ((rx_count < 2) && (read_len > 0)) {
#else
			while ((rx_count < 1) && (read_len > 0)) {
#endif
				if ((read_len < sizeof(HI_MSG_HDR)) ||
						read_len
						> (MAX_SZ_RD_WR_BUFFERS
						- PIGGYBACK_CTRL_REG)) {
					DEBUG(DBG_ERROR,
						"%s:read buffer length"
						"incorrect\n", __func__);
					goto err_out;
				}
				/*Add SIZE of PIGGYBACK reg (CONTROL Reg)to
				the NEXT Message length + 2 Bytes for SKB */
				read_len = read_len + 2;
#ifdef MOP_WORKAROUND
				tmp = read_len%RWBLK_SIZE ?
							(read_len +
							RWBLK_SIZE -
							read_len %
							RWBLK_SIZE) :
							read_len;
				alloc_len = (tmp < SDIO_BLOCK_SIZE) ?
							SDIO_BLOCK_SIZE : tmp;
#else
				if (read_len <= SDIO_BLOCK_SIZE)
					alloc_len = SDIO_BLOCK_SIZE;
				else
					alloc_len = read_len +
							SDIO_BLOCK_SIZE;
#endif

				/*Allocate SKB */
				skb = dev_alloc_skb(alloc_len);
				if (!skb) {
					DEBUG(DBG_ERROR,
						"%s: SKB alloc failed."
						"Out of memory.\n", __func__);
					goto err_out;
				}
				/*Create room for data in the SKB */
				skb_put(skb, (read_len - 2));

				/*Read DATA from the DEVICE */
#ifdef MOP_WORKAROUND
				memset(skb->data, 0, tmp);
				retval = SBUS_SDIOReadWrite_QUEUE_Reg(priv,
						(uint8_t *)(skb->data),
						tmp, SDIO_READ);
				if (retval) {
					DEBUG(DBG_ERROR, "SDIO Queue R/W "
							"Error \n");
					goto err_out;
				}
#else
				memset(skb->data, 0, read_len);
				retval = SBUS_SDIOReadWrite_QUEUE_Reg(priv,
						(uint8_t *)(skb->data),
						read_len, SDIO_READ);
				if (retval) {
					DEBUG(DBG_SBUS,
						"SDIO Queue R/W Error \n");
					goto err_out;
				}
#endif
				/*Update CONTROL Reg value from the DATA read
				and update NEXT Message Length */
				payload = (hif_rw_msg_hdr_payload_t *)
						(skb->data);
#ifdef MOP_WORKAROUND
				cont_reg_val = *((uint16_t *)
						((uint8_t *)skb->data +
						(uint8_t)(tmp - 2)));
#else
				cont_reg_val = *((uint16_t *)
						((uint8_t *)skb->data +
						(uint8_t)
						(payload->hdr.MsgLen)));
#endif
				/*Extract Message ID from header */
				msgid = (uint16_t)(payload->hdr.MsgId);
				/*If Message ID is EXCEPTION then STOP
				HIF Communication and Report ERROR to WDEV */
				if (unlikely((msgid & MSG_ID_MASK) ==
						HI_EXCEPTION_IND_ID)) {
					exception_ind = (HI_EXCEPTION_IND *)
							payload;

					DEBUG(DBG_ERROR, "%s: Firmware Exception Reason [%x] \n",
						__func__,
						exception_ind->Reason);
					memset(buf, 0, sizeof(buf));
					memcpy(buf, exception_ind->FileName,
						HI_EXCEP_FILENAME_SIZE);
					DEBUG(DBG_ERROR, "%s: File Name:[%s],"
							"Line[%x], R2 [%x], PC[%x], SP[%x],\n"
							"R0[%x],R3[%x]R4[%x],R5[%x],R6[%x],R7[%x],\n"
							"R8[%x],R9[%x],R10[%x],R11[%x],\nR12[%x],LR[%x],"
							"CPSR[%x],SPSR[%x]\n",
							__func__, buf,
							exception_ind->R1,
							exception_ind->R2,
							exception_ind->PC,
							exception_ind->SP,
							exception_ind->R0,
							exception_ind->R3,
							exception_ind->R4,
							exception_ind->R5,
							exception_ind->R6,
							exception_ind->R7,
							exception_ind->R8,
							exception_ind->R9,
							exception_ind->R10,
							exception_ind->R11,
							exception_ind->R12,
							exception_ind->LR,
							exception_ind->CPSR,
							exception_ind->SPSR);
					goto err_out;
				}
				/*If Message ID Is STARTUP then read the
				initialisation info from the Payload */
				if (unlikely((msgid & MSG_ID_MASK) ==
						HI_STARTUP_IND_ID)) {
					startup_ind = (HI_STARTUP_IND *)
							payload;
					memset(buf, 0, sizeof(buf));
					memcpy(buf,startup_ind->FirmwareLabel,
						HI_SW_LABEL_MAX);
					DEBUG(DBG_ALWAYS, "Firmware Label :"
						"[%s] \n", buf);
					DEBUG(DBG_ALWAYS, "InitStatus:[%d]\n",
						startup_ind->InitStatus);

					priv->max_size_supp =
						startup_ind->SizeInpChBuf;
					priv->max_num_buffs_supp =
						startup_ind->NumInpChBufs;
				}
#ifdef ETFTEST
				/*Extract SEQUENCE Number from the Message ID
				* and check whether it is the same as
				* expected. If SEQUENCE Number mismatches
				* then Shutdown */
				if (SBUS_GET_MSG_SEQ(payload->hdr.MsgId)
						!= (priv->in_seq_num &
						HI_MSG_SEQ_RANGE)) {
					DEBUG(DBG_ERROR, "FATAL Error:"
						"SEQUENCE Number Mismatch in"
						"Received Packet [%x],[%x]\n",
						SBUS_GET_MSG_SEQ(payload->
							hdr.MsgId),
						priv->in_seq_num &
							HI_MSG_SEQ_RANGE);
					goto err_out;
				}

				priv->in_seq_num++;
#endif
				/*Increement READ Queue Register Number */
				Increment_Rd_Buffer_Number(priv);

				/*If MESSAGE ID contains CONFIRMATION ID then
				* Increment TX_CURRENT_BUFFER_COUNT */
				if (payload->hdr.MsgId & HI_CNF_BASE) {
					if (priv->num_unprocessed_buffs_in_device > 0)
						priv->num_unprocessed_buffs_in_device--;
					else
						DEBUG(DBG_ERROR, "HIF Lost"
							"synchronisation.Recd"
							"CONFIRM message but"
							"there was no"
							"coresponding REQUEST"
							"message\n");
				}
				/*WORKAROUND - Add the SKB pointer at the end
				* of the data buffer.*/

				DEBUG(DBG_SBUS, "%s: SKB Pointer"
						"passed:%p,%d\n",
						__func__, skb,
						payload->hdr.MsgLen);

				if (etf_flavour == 1) {
					/* need msg seq in UETF */
					DEBUG(DBG_SBUS, "UETF running, msg "
						"seq not masked\n");
				} else {
					DEBUG(DBG_SBUS, "RETF/PETF running,"
						"msg seq masked\n");
					/* donot need msg seq in RETF/PETF */
					payload->hdr.MsgId &= MSG_ID_MASK;
					DEBUG(DBG_SBUS, "fw rcvd msg"
						"seq no :%d\n",
						(payload->hdr.MsgId)>>13);
				}

				/*Adjust skb->len according to
				* MsgLen in HI Header */
				skb->len = payload->hdr.MsgLen;
				/*Enqueue the packet to pkt queue */
				retval = enqueue_rx_data(priv,
							skb, skb->data);
				rx_count++;
				if (retval != ETF_STATUS_SUCCESS)
					DEBUG(DBG_ERROR, "%s: enqueue_rx_data"
							"returned error \n",
							__func__);

#ifdef PIGGYBACK
				/*Update the piggyback length */
				retval = SBUS_SDIO_RW_Reg(priv,
						ST90TDS_CONTROL_REG_ID,
						(char *)&cont_reg_val,
						BIT_16_REG, SDIO_READ);
				if (retval) {
					DEBUG(DBG_SBUS,
						"SDIO-Read/Write Error\n");
					goto err_out;
				}
				cont_reg_val = (uint16_t)(cont_reg_val &
						ST90TDS_SRAM_BASE_LSB_MASK);
				/*If NEXT Message Length is greater than ZERO
				then goto step 3 else EXIT Bottom Half */
				read_len = SPI_CONT_NEXT_LENGTH(cont_reg_val);
				read_len = read_len * 2;
#endif
			}
		} /*End of RX Code */

		return;
err_out:
		DEBUG(DBG_ERROR,
			"%s:FATAL Error:Shutdown driver \n", __func__);
		return;
}

#ifndef GPIO_BASED_IRQ
void cw1200_sbus_interrupt(CW1200_bus_device_t *func)
{
	struct CW1200_priv *priv = NULL;

	printk("SDIO:cw1200_sbus_interrupt Called \n");
	priv = sdio_get_drvdata(func);
	atomic_xchg(&(priv->Interrupt_From_Device), TRUE);
	queue_work(priv->sbus_WQ, &priv->sbus_work);
}
#endif

/*! \fn LL_HANDLE ETF_CB_Create (ETF_HANDLE ETFHandle, UL_HANDLE ulHandle)
*
* \brief This function will create the SBUS layer
*
* \param ETFHandle - Handle to ETF instance.
*
* \param ulHandle - Upper layer driver handle.
*
* \return - Handle to Lower Layer.
*/
LL_HANDLE ETF_CB_Create (ETF_HANDLE ETFHandle, UL_HANDLE ulHandle)
{
	int32_t retval = SUCCESS;
	struct CW1200_priv *priv;

	DEBUG(DBG_FUNC, "Entering %s\n", __func__);

	priv = (struct CW1200_priv *)ulHandle;
	priv->lower_handle = ETFHandle;
	/*Create WORK Queue */
	priv->sbus_WQ = create_singlethread_workqueue("sbus_work");
	sbus_wq_created=1;
	/* 1. Register BH */
	INIT_WORK(&priv->sbus_work, SBUS_bh);

	/*Init device buffer numbers */
	priv->sdio_wr_buf_num_qmode = 0;
	priv->sdio_rd_buf_num_qmode = 1;

	/*Initialise Interrupt from device flag to FALSE */
	atomic_set(&(priv->Interrupt_From_Device), FALSE);
	atomic_set(&(priv->Umac_Tx), FALSE);

	sdio_set_drvdata(priv->func, priv);
	sdio_claim_host(priv->func);
	DEBUG(DBG_SBUS, "SDIO - VENDORID [%x], DEVICEID [%x] \n",
			priv->func->vendor, priv->func->device);

	retval = sdio_enable_func(priv->func);
	if (retval) {
		DEBUG(DBG_ERROR, "%s, Error :[%d] \n", __func__, retval);
		return NULL;
	}
	sdio_release_host(priv->func);

	/*The Lower layer handle is the same as the upper layer handle -
	device priv */
	return (LL_HANDLE *)priv;
}

/*! \fn ETF_STATUS_CODE ETF_CB_Start (LL_HANDLE LowerHandle,
*				uint32_t FmLength, void *FirmwareImage)
\brief This callback function will start the firmware downloading.
\param LowerHandle - Handle to lower driver instance.
\param FmLength - Length of firmware image.
\param FirmwareImage - Path to the Firmware Image.
\return - ETF Status Code
*/
ETF_STATUS_CODE ETF_CB_Start (LL_HANDLE LowerHandle,
		uint32_t FmLength, void *FirmwareImage,uint32_t BlLength, void *bootloader)
{
	struct CW1200_priv *priv = (struct CW1200_priv *)LowerHandle;
	ETF_STATUS_CODE retval = ETF_STATUS_SUCCESS;
	uint32_t config_reg_val = 0;
	uint32_t dpll_buff = 0;
	uint32_t dpll_buff_read = 0;
	u16 cont_reg_val = 0;
	uint32_t i = 0;
	uint32_t configmask_cw1200 = 0;
	uint32_t configmask_silicon_vers = 0;
	uint32_t config_reg_len = BIT_32_REG;
	int32_t err_val=0;
	uint8_t cccr=0;
	int major_revision = -1;
	uint32_t regval_apb9 = 0;
	uint32_t regval_apb10 = 0;
	u16 val16;

	DEBUG(DBG_ALWAYS, "In %s", __func__);
	/*Read CONFIG Register Value - We will read 32 bits*/
	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val, BIT_32_REG, SDIO_READ);
	if (retval)
		return ETF_STATUS_FAILURE;

	DEBUG(DBG_ALWAYS, "Initial 32 Bit CONFIG Register Value: [%x] \n",
			config_reg_val);

	configmask_cw1200 = (config_reg_val >> 24) & 0x3;
	configmask_silicon_vers = (config_reg_val >> 31) & 0x1;
	/*test hwid*/

	DEBUG(DBG_ALWAYS, "%s [%d]: configmask_cw1200[0x%x]\n",
			__func__, __LINE__, configmask_cw1200);
	DEBUG(DBG_ALWAYS, "configmask_silicon_vers[0x%x]\n",
			configmask_silicon_vers);

	/*Check if we have CW1200 or STLC9000 */
	if ((configmask_cw1200 == 0x1) || (configmask_cw1200 == 0x2)) {
		DEBUG(DBG_ALWAYS, "CW12x0 Silicon Detected \n");
		major_revision = configmask_cw1200;
		if (configmask_silicon_vers)
			priv->hw_type = HIF_8601_VERSATILE;
		else
			priv->hw_type = HIF_8601_SILICON;

	} else if (((config_reg_val >> 24) & 0x4) == 0x4) {
		major_revision = 0x4;
		if (configmask_silicon_vers)
			priv->hw_type = HIF_8601_VERSATILE;
		else
			priv->hw_type = HIF_8601_SILICON;
	} else {
		DEBUG(DBG_ERROR, "Error: Unknown Chip Detected \n");
		return ETF_STATUS_UNKNOWN_CHIP;
	}

	config_reg_len = BIT_32_REG;

	DEBUG(DBG_MESSAGE, "DPLL val:0x%x\n",DPLL_INIT_VAL);
	dpll_buff = DPLL_INIT_VAL;

	retval = cw1200_reg_write_32(priv, ST90TDS_TSET_GEN_R_W_REG_ID, dpll_buff);

	if (retval)
		return ETF_STATUS_FAILURE;

	msleep(20);

	/*Read DPLL Reg value and compare with value written */
	retval = cw1200_reg_read_32(priv,ST90TDS_TSET_GEN_R_W_REG_ID, &dpll_buff_read);
	if (retval){
		DEBUG(DBG_ERROR,"Failed to read DPLL Reg value");
		return ETF_STATUS_FAILURE;
	}

	if (dpll_buff_read != dpll_buff)
		DEBUG(DBG_ERROR, "Unable to initialise DPLL register."
				"Value Read is : [%x] \n", dpll_buff_read);

	/*Set Wakeup bit in device */
	retval = cw1200_reg_read_16(priv, ST90TDS_CONTROL_REG_ID, &cont_reg_val);

	if (retval)
		return ETF_STATUS_FAILURE;

	retval = cw1200_reg_write_16(priv, ST90TDS_CONTROL_REG_ID,
								cont_reg_val | ST90TDS_CONT_WUP_BIT);

	if (retval)
		return ETF_STATUS_FAILURE;

	/* Wait for wakeup */
	for (i = 0 ; i < 300 ; i += 1 + i / 2) {
		retval = cw1200_reg_read_16(priv,
			ST90TDS_CONTROL_REG_ID, &val16);
		if (retval)
			return ETF_STATUS_FAILURE;

		if (val16 & ST90TDS_CONT_RDY_BIT) {
			DEBUG(DBG_SBUS, "WLAN is ready [%x],[%x]\n",
					i, cont_reg_val);
			break;
		}
		msleep(i);
	}

	if ((val16 & ST90TDS_CONT_RDY_BIT) == 0) {
		DEBUG(DBG_SBUS, "WLAN READY Bit not set \n");
		return ETF_STATUS_FAILURE;
	}
	/* CW1200 Hardware detection logic :
	CW1200 cut2.0 / CW1250 cut (1.0/1.1) */
	DEBUG(DBG_MESSAGE, "%s: BEFORE CHIP DETECT :major_revision = %d\n",
				__func__,major_revision);
	retval = cw1200_chip_detect(priv, major_revision);

	if (HIF_8601_SILICON != priv->hw_type)
		config_reg_len = BIT_16_REG;

	/*Checking for access mode */
	retval = cw1200_reg_read_32(priv, ST90TDS_CONFIG_REG_ID, &config_reg_val);
	if (retval)
		return ETF_STATUS_FAILURE;

	if (config_reg_val & ST90TDS_CONFIG_ACCESS_MODE_BIT) {
		DEBUG(DBG_SBUS, "We are in DIRECT Mode \n");
	} else {
		DEBUG(DBG_ERROR, " ERROR: We are in QUEUE Mode \n");
		return ETF_STATUS_FAILURE;
	}

	/*set hif drive strength*/
	if(SBUS_SramRead_AHB(priv, 0x0AA80044, (uint8_t*)&regval_apb9, 4)){
		DEBUG(DBG_ERROR, "regval_apb9 read failed for hif drive"
				"strength \n");
	}
	if(SBUS_SramRead_AHB(priv, 0x0AA80048, (uint8_t*)&regval_apb10, 4)){
		DEBUG(DBG_ERROR, "regval_apb10 read failed for hif drive"
				"strength \n");
	}

	regval_apb9 &= ~(SDD_STL8601_CONFIG4_REG9_BITMASK);
	regval_apb10 &= ~(SDD_STL8601_CONFIG4_REG10_BITMASK);

	switch (hif_strength) {
		case 0 :
			break;
		case 1 :
			regval_apb9 |= 0x20000222;
			regval_apb10 |= 0x02200000;
			break;
		case 2 :
			regval_apb9 |= 0x30000333;
			regval_apb10 |= 0x03300000;
			break;
	}
	DEBUG(DBG_MESSAGE, "APB Reg 9 = %08x\r\n",regval_apb9);
	DEBUG(DBG_MESSAGE, "APB Reg 10 = %08x\r\n",regval_apb10);

	if(SBUS_SramWrite_AHB(priv, 0x0AA80044, (uint8_t*)&regval_apb9, 4)) {
		DEBUG(DBG_ERROR, "regval_apb9 write failed for hif drive"
				"strength \n");
	}
	if(SBUS_SramWrite_AHB(priv, 0x0AA80048, (uint8_t*)&regval_apb10, 4)){
		DEBUG(DBG_ERROR, "regval_apb10 write failed for hif drive"
				"strength \n");
	}

	/* 5. Call function to download firmware */
	if (HIF_8601_SILICON == priv->hw_type) {
		DEBUG(DBG_MESSAGE, "CW1200 (HIF_8601_SILICON)"
					"detected %s", __func__);
		if(chip_version_global_int == CW1260_HW_REV_CUT10)
			retval = cw1200_load_bootloader_cw1260(priv,
				FirmwareImage, FmLength, bootloader, BlLength);
		else
			retval = CW1200_UploadFirmware(priv,
						FirmwareImage, FmLength);
	} else if (HIF_8601_VERSATILE == priv->hw_type ){
		if(chip_version_global_int == CW1260_HW_REV_CUT10) {
#ifdef FPGA
			retval = cw1200_load_firmware_cw1260_fpga(priv,
				FirmwareImage, FmLength, bootloader, BlLength);
#endif
		} else {
			retval = STLC9000_UploadFirmware(priv,
				FirmwareImage, FmLength);
		}
	} else if ( HIF_9000_SILICON_VERSTAILE == priv->hw_type ){
		//Not implemented
		retval = ETF_STATUS_UNKNOWN_CHIP;
	}
	if (retval)
		return retval;

	DEBUG(DBG_SBUS, "Before claim host \n");
	sdio_claim_host(priv->func);

#ifdef GPIO_BASED_IRQ
	DEBUG(DBG_SBUS, "wpd mmc id:%s \n", wpd->mmc_id);
	DEBUG(DBG_SBUS, "wpd irq name:%s \n", wpd->irq->name);
	DEBUG(DBG_SBUS, "wpd irq start:%d \n", wpd->irq->start);
	DEBUG(DBG_SBUS, "wpd reset name:%s \n", wpd->reset->name);
	DEBUG(DBG_SBUS, "wpd reset start:%d \n",wpd->reset->start);
	DEBUG(DBG_SBUS, "Register GPIO based Interrupts\n");

	retval = request_any_context_irq(wpd->irq->start,
			cw1200_gpio_irq,IRQF_TRIGGER_RISING,
			wpd->irq->name, priv);
	irq_regd=1;

	if ((retval == 0) || (retval == 1)) {
		DEBUG(DBG_SBUS, "%s,WLAN_IRQ registered on GPIO \n",
				__func__);
	} else {
		DEBUG(DBG_ERROR, "%s,Unable to request IRQ on GPIO \n",
				__func__);
	}

	/* Hack to access Fuction-0 */
	priv->func->num= 0;
	cccr = sdio_readb(priv->func, SDIO_CCCR_IENx,&err_val);

	if (err_val)
		return ETF_STATUS_FAILURE;

	/* We are function number 1 */
	cccr |= 1 << 1;

	cccr |= 1; /* Master interrupt enable */

	sdio_writeb(priv->func, cccr, SDIO_CCCR_IENx,&err_val);
	/* Restore the WLAN function number */
	priv->func->num = 1;

	if (err_val) {
		DEBUG(DBG_SBUS, "%s,F0 write failed \n", __func__);
		return ETF_STATUS_FAILURE;
	}

#else /* Register SDIO based interrupts */
	DEBUG(DBG_SBUS, "Register SDIO based Interrupts\n");
	DEBUG(DBG_SBUS, "Going to register SDIO ISR \n");
	/*Register Interrupt Handler */
	retval = sdio_claim_irq(priv->func, cw1200_sbus_interrupt);
	if (retval) {
		DEBUG(DBG_ERROR, "%s, sdio_claim_irq() return error :[%d] \n",
				__func__, retval);
		return ETF_STATUS_FAILURE;
	}
	irq_regd=1;
#endif /*GPIO_BASED_IRQ*/

	/*Sleep after registering Int Handler so that the IRQ thread can run*/
	mdelay(100);

	sdio_release_host(priv->func);

	/*If device is CW1200 the IRQ enable/disable bits are in
	CONFIG register */
	if (HIF_8601_SILICON == priv->hw_type) {
		retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
				(char *)&config_reg_val,
				config_reg_len, SDIO_READ);
		if (retval)
			return retval;

		config_reg_val = config_reg_val | ST90TDS_CONF_IRQ_RDY_ENABLE;
		SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
				(char *)&config_reg_val,
				config_reg_len, SDIO_WRITE);
	} else {
		/*If device is STLC9000 the IRQ enable/disable bits are
		in CONTROL register */
		/*Enable device interrupts - Both DATA_RDY and WLAN_RDY */
		retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONTROL_REG_ID,
				(char *)&cont_reg_val,
				config_reg_len, SDIO_READ);
		if (retval)
			return retval;

		/*Enable SDIO Host Interrupt */
		/*Enable device interrupts - Both DATA_RDY and WLAN_RDY */
		cont_reg_val = cpu_to_le16(cont_reg_val |
				ST90TDS_CONT_IRQ_RDY_ENABLE);
		SBUS_SDIO_RW_Reg(priv, ST90TDS_CONTROL_REG_ID,
				(char *)&cont_reg_val,
				config_reg_len, SDIO_WRITE);
	}

	/*Configure device for MESSSAGE MODE */
	config_reg_val = config_reg_val & ST90TDS_CONFIG_ACCESS_MODE_MASK;
	SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val, config_reg_len, SDIO_WRITE);
	/*Unless we read the CONFIG Register we are not able to get
	an interrupt */
	mdelay (10);
	SBUS_SDIO_RW_Reg (priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val, config_reg_len, SDIO_READ);
	return retval;
}

ETF_STATUS_CODE SBUS_ChipDetect(LL_HANDLE LowerHandle)
{
	struct CW1200_priv *priv_local = (struct CW1200_priv *)LowerHandle;
	ETF_STATUS_CODE retval = ETF_STATUS_SUCCESS;
	uint32_t config_reg_val = 0;
	u16 cont_reg_val = 0;
	uint32_t i = 0;
	uint32_t configmask_cw1200 = 0;
	uint32_t configmask_silicon_vers = 0;
	int major_revision = -1;

	DEBUG(DBG_ALWAYS, "In %s", __func__);
	/*Read CONFIG Register Value - We will read 32 bits*/
	retval = SBUS_SDIO_RW_Reg(priv_local, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val, BIT_32_REG, SDIO_READ);
	if (retval) {
		DEBUG(DBG_ERROR, "%s(%d): config_reg_val read error\n",
			__func__, __LINE__);
		return ETF_STATUS_FAILURE;
	}

	DEBUG(DBG_ALWAYS, "Initial 32 Bit CONFIG Register Value: [%x] \n",
			config_reg_val);

	configmask_cw1200 = (config_reg_val >> 24) & 0x3;
	configmask_silicon_vers = (config_reg_val >> 31) & 0x1;
	/*test hwid*/

	DEBUG(DBG_ALWAYS, "%s: configmask_cw1200(0x%x)\n",
			__func__, configmask_cw1200);
	DEBUG(DBG_ALWAYS, "configmask_silicon_vers(0x%x)\n",
			configmask_silicon_vers);

	/*Check if we have CW1200 or STLC9000 */
	if ((configmask_cw1200 == 0x1) || (configmask_cw1200 == 0x2)) {
		DEBUG(DBG_ALWAYS, "CW12x0 Silicon Detected \n");
		major_revision = configmask_cw1200;
		if (configmask_silicon_vers)
			priv_local->hw_type = HIF_8601_VERSATILE;
		else
			priv_local->hw_type = HIF_8601_SILICON;

	} else if (((config_reg_val >> 24) & 0x4) == 0x4) {
		major_revision = 0x4;
		if (configmask_silicon_vers)
			priv_local->hw_type = HIF_8601_VERSATILE;
		else
			priv_local->hw_type = HIF_8601_SILICON;
	} else {
		DEBUG(DBG_ERROR, "Error: Unknown Chip Detected \n");
		return ETF_STATUS_UNKNOWN_CHIP;
	}

	/*Set Wakeup bit in device */
	retval = cw1200_reg_read_16(priv_local,
				ST90TDS_CONTROL_REG_ID, &cont_reg_val);
	if (retval){
		DEBUG(DBG_ERROR, "%s(%d): cont_reg_val read error\n",
			__func__, __LINE__);
		return ETF_STATUS_FAILURE;
	}

	retval = cw1200_reg_write_16(priv_local, ST90TDS_CONTROL_REG_ID,
				cont_reg_val | ST90TDS_CONT_WUP_BIT);
	if (retval){
		DEBUG(DBG_ERROR, "%s(%d): cont_reg_val read error\n",
				__func__, __LINE__);
		return ETF_STATUS_FAILURE;
	}

	for (i = 0; i < 300; i++) {
		retval = cw1200_reg_read_16(priv_local,
			ST90TDS_CONTROL_REG_ID, &cont_reg_val);
		if (retval){
			DEBUG(DBG_ERROR, "%s(%d): cont_reg_val read error\n",
				__func__, __LINE__);
			return ETF_STATUS_FAILURE;
		}

		if (cont_reg_val & ST90TDS_CONT_RDY_BIT) {
			DEBUG(DBG_SBUS, "WLAN is ready [%x],[%x]\n",
					i, cont_reg_val);
			break;
		}
		msleep(i);
	}

	if ((cont_reg_val & ST90TDS_CONT_RDY_BIT) == 0) {
		DEBUG(DBG_SBUS, "WLAN READY Bit not set \n");
		return ETF_STATUS_FAILURE;
	}
	/* CW1200 Hardware detection logic :
	CW1200 cut2.0 / CW1250 cut (1.0/1.1) */
	retval = cw1200_chip_detect(priv_local, major_revision);

	return retval;
}

/**
* \fn int Device_SleepStatus(struct CW1200_priv *priv, int *sleep_status)
* \brief Function to check if the device is sleeping or awake.
* \param priv - global structure for cw1200 data
* \param sleep_status - Out parameter to get sleep status.
* \return - success or failure
*/
int Device_SleepStatus(struct CW1200_priv *priv, int *sleep_status)
{
	int retval = SUCCESS;
	uint32_t cont_reg_val = 0;

	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONTROL_REG_ID,
			(char *)&cont_reg_val, BIT_16_REG, SDIO_READ);
	if (retval != SUCCESS)
		goto end;

	if (cont_reg_val & ST90TDS_CONT_RDY_BIT)
		*sleep_status = TRUE;
	else
		*sleep_status = FALSE;

end:
	return retval;
}

/**
* \fn ETF_STATUS_CODE ETF_CB_Stop (LL_HANDLE LowerHandle)
* \brief This callback function will stop the lower layer.
* \param LowerHandle - Handle to lower driver instance.
* \return - ETF Status Code
*/
ETF_STATUS_CODE ETF_CB_Stop (LL_HANDLE LowerHandle)
{
	struct CW1200_priv *priv = (struct CW1200_priv *)LowerHandle;

	DEBUG(DBG_MESSAGE, "%s: going to flush sbus_wq\n", __func__);
	flush_workqueue(priv->sbus_WQ);
	destroy_workqueue(priv->sbus_WQ);

	return ETF_STATUS_SUCCESS;
}

ETF_STATUS_CODE ETF_CB_Devstop (LL_HANDLE LowerHandle)
{
	struct CW1200_priv *priv = (struct CW1200_priv *)LowerHandle;
	DEBUG(DBG_MESSAGE, "%s: Toggling WLAN_Enable to reset device\n",
			__func__);
	DEBUG(DBG_MESSAGE, "%s: going to flush sbus_wq\n", __func__);

	if(sbus_wq_created ){
		flush_workqueue(priv->sbus_WQ);
		destroy_workqueue(priv->sbus_WQ);
		DEBUG(DBG_MESSAGE, "%s: destroyed sbus_wq\n", __func__);
		mdelay(10);
	}
#ifdef GPIO_BASED_IRQ
	if(irq_regd == 1) {
		DEBUG(DBG_MESSAGE, "irq_regd:%d",irq_regd);
		free_irq(wpd->irq->start, priv);
		gpio_free(wpd->irq->start);
	}
#endif
	DEBUG(DBG_MESSAGE, "%s: free gpio irq\n", __func__);

#ifndef GPIO_BASED_IRQ
	if(g_enter) {
		sdio_claim_host(priv->func);
		sdio_release_irq(priv->func);
		sdio_release_host(priv->func);
	}
#endif

	if(g_enter) {
		sdio_claim_host(priv->func);
		sdio_disable_func(priv->func);
		sdio_release_host(priv->func);
	}

	async_exit();
	DEBUG(DBG_FUNC, "%s:returning status:ETF_STATUS_SUCCESS to caller\n",
			__func__);
	return ETF_STATUS_SUCCESS;
}

ETF_STATUS_CODE ETF_CB_Resetwq (LL_HANDLE LowerHandle) {
	struct CW1200_priv *priv = (struct CW1200_priv *)LowerHandle;
	int retval = ETF_STATUS_SUCCESS;
	/*Create WORK Queue */
	priv->sbus_WQ = create_singlethread_workqueue("sbus_work");
	/* 1. Register BH */
	INIT_WORK(&priv->sbus_work, SBUS_bh);
	sdio_claim_host(priv->func);
	DEBUG(DBG_SBUS, "SDIO - VENDORID [%x], DEVICEID [%x] \n",
			priv->func->vendor, priv->func->device);

	retval = sdio_enable_func(priv->func);
	if (retval) {
		DEBUG(DBG_ERROR, "%s, Error :[%d] \n", __func__, retval);
		return ETF_STATUS_FAILURE;
	}
	sdio_release_host(priv->func);
	sdio_set_drvdata(priv->func, priv);
	return retval;
}

/*! \fn ETF_STATUS_CODE ETF_CB_Destroy (LL_HANDLE LowerHandle)
\brief This callback function will destroy lower layer.
\param LowerHandle - Handle to lower driver instance.
\return - ETF Status Code
*/
ETF_STATUS_CODE ETF_CB_Destroy (LL_HANDLE LowerHandle)
{
	struct CW1200_priv *priv = (struct CW1200_priv *)LowerHandle;
#ifdef GPIO_BASED_IRQ
	free_irq(wpd->irq->start, priv);
	gpio_free(wpd->irq->start);
#endif

	sdio_claim_host(priv->func);
	sdio_release_irq(priv->func);
	sdio_disable_func(priv->func);
	sdio_release_host(priv->func);

	return ETF_STATUS_SUCCESS;
}

static CW1200_STATUS_E enqueue_rx_data(struct CW1200_priv *priv,
		struct sk_buff *skb, uint8_t *data)
{
	struct cw1200_rx_buf *rx_buf = NULL;
	rx_buf = kzalloc(sizeof(struct cw1200_rx_buf), GFP_KERNEL);

	if (!rx_buf)
		return ERROR;

	rx_buf->skb = skb;
	rx_buf->data = data;

	mutex_lock(&priv->rx_list_lock);
	list_add_tail(&rx_buf->list, &priv->rx_list);
	mutex_unlock(&priv->rx_list_lock);
	DEBUG(DBG_MESSAGE, "complete from enqueue\n");
	complete(&priv->comp);

	return SUCCESS;
}

CW1200_STATUS_E SBUS_Sleep_Device(struct CW1200_priv *priv)
{
	uint32_t cont_reg_val = 0;
	uint32_t retval = SUCCESS;

	/*Set Wakeup bit in device */
	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONTROL_REG_ID,
			(char *)&cont_reg_val, BIT_16_REG, SDIO_READ);
	if (retval)
		return ERROR;

	/* Clear wakeup bit in device */
	cont_reg_val = cpu_to_le16(cont_reg_val & ST90TDS_CONT_WUP_BIT_MASK);

	retval = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONTROL_REG_ID,
			(char *)&cont_reg_val, BIT_16_REG, SDIO_WRITE);
	if (retval)
		return ERROR;

	return SUCCESS;
}


int cw1200_chip_detect(struct CW1200_priv *priv, int major_revision)
{
	uint32_t ret = SUCCESS;
	uint32_t val32;
	uint32_t ar1=0, ar2=0, ar3=0;

	DEBUG(DBG_FUNC, "%s [%d]: CW1200 Hardware detection logic start\n",
			__func__, __LINE__);
	DEBUG(DBG_MESSAGE, "major_revision[%d]\n", major_revision);

	if (major_revision == 1) {
		ret = cw1200_ahb_read_32(priv, CW1200_CUT_1x_ID_ADDR, &val32);
		if (ret) {
			DEBUG(DBG_ERROR, "%s: HW detection : "
					"can't read CUT ID\n",
					__func__);
			chip_version_global_int = CHIP_CANT_READ;
			goto out;
		}

		switch (val32) {
			case CW1200_CUT_11_ID_STR:
				DEBUG(DBG_ALWAYS,
					"CW1200 Cut 1.1 silicon detected\n");
				chip_version_global_int = CHIP_CW1200_CUT_11;
				break;
			default:
				DEBUG(DBG_ALWAYS,
					"CW1200 Cut 1.0 silicon detected\n");
				chip_version_global_int = CHIP_CW1200_CUT_10;
				break;
		}
	} else if (major_revision == 2) {
		DEBUG(DBG_MESSAGE, "\n[%d]: Before ahb_read, ar1[0x%x]\n",
				__LINE__, ar1);
		ret = cw1200_ahb_read_32(priv, CW12x0_CUT_xx_ID_ADDR, &ar1);
		DEBUG(DBG_MESSAGE, "[%d]: After ahb_read, ar1[0x%x]\n",
				__LINE__, ar1);
		if (ret) {
			DEBUG(DBG_ERROR, "%s: (1) HW detection: "
					"can't read CUT ID\n", __func__);
			chip_version_global_int = CHIP_CANT_READ;
			goto out;
		}

		DEBUG(DBG_MESSAGE, "\n[%d]: Before ahb_read, ar2[0x%x]\n",
				__LINE__, ar2);
		ret = cw1200_ahb_read_32(priv,
				CW12x0_CUT_xx_ID_ADDR + 4, &ar2);
		DEBUG(DBG_MESSAGE, "\n[%d]: After ahb_read, ar2[0x%x]\n",
				__LINE__, ar2);
		if (ret) {
			DEBUG(DBG_ERROR,
					"%s: (2) HW detection: "
					"can't read CUT ID.\n", __func__);
			chip_version_global_int = CHIP_CANT_READ;
			goto out;
		}

		DEBUG(DBG_MESSAGE, "\n[%d]: Before ahb_read, ar3[0x%x]\n",
				__LINE__, ar3);
		ret = cw1200_ahb_read_32(priv,
				CW12x0_CUT_xx_ID_ADDR + 8, &ar3);
		DEBUG(DBG_MESSAGE, "\n[%d]: After ahb_read, ar3[0x%x]\n",
				__LINE__, ar3);
		if (ret) {
			DEBUG(DBG_ERROR,
					"%s: (3) HW detection: "
					"can't read CUT ID.\n", __func__);
			chip_version_global_int = CHIP_CANT_READ;
			goto out;
		}

		if (ar1 == CW1200_CUT_22_ID_STR1 &&
				ar2 == CW1200_CUT_22_ID_STR2 &&
				ar3 == CW1200_CUT_22_ID_STR3) {
			DEBUG(DBG_ALWAYS, "CW1200 Cut 2.x/CW1250 Cut 1.0 \n");
			chip_version_global_int = CW1200_CW1250_CUT_xx;
		} else if (ar1 == CW1250_CUT_11_ID_STR1 &&
				ar2 == CW1250_CUT_11_ID_STR2 &&
				ar3 == CW1250_CUT_11_ID_STR3) {
			DEBUG(DBG_ALWAYS, "CW1250 Cut 1.1 detected.\n");
			chip_version_global_int = CHIP_CW1250_CUT_11;
		} else {
			DEBUG(DBG_ALWAYS, "CW1200 Cut 2.0 detected.\n");
			chip_version_global_int = CHIP_CW1200_CUT_20;
		}
	} else if (major_revision == 4 ) {
			DEBUG(DBG_ALWAYS, "CW1260 cut1.0 detected.\n");
			chip_version_global_int = CW1260_HW_REV_CUT10;
	} else {
		DEBUG(DBG_ERROR, "%s: unsupported major revision %d.\n",
				__func__, major_revision);
		chip_version_global_int = CHIP_UNSUPPORTED;
		ret = -ENOTSUPP;
		goto out;
	}
out:
	DEBUG(DBG_FUNC, "%s: CW1200 Hardware detection logic end.\n",
			__func__);
	return ret;
}
