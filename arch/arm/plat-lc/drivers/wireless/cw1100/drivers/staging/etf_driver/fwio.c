/*
 * Firmware I/O code for mac80211 ST-Ericsson CW1200 drivers
 *
 * Copyright (c) 2010, ST-Ericsson
 * Author: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 *
 * Based on:
 * ST-Ericsson UMAC CW1200 driver which is
 * Copyright (c) 2010, ST-Ericsson
 * Author: Ajitpal Singh <ajitpal.singh@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/firmware.h>
#include "cw1200_common.h"

#include "debug.h"
#include "hwio.h"
#include "sbus.h"

/* Macros are local to the file */
#define REG_WRITE(reg, val) \
	do { \
		ret = cw1200_reg_write_32(priv, (reg), (val)); \
		if (ret < 0) { \
			DEBUG(DBG_ERROR, "%s: can't write %s at line %d.\n", \
				__func__, #reg, __LINE__); \
			goto error; \
		} \
	} while (0)
#define REG_READ(reg, val) \
	do { \
		ret = cw1200_reg_read_32(priv, (reg), &(val)); \
		if (ret < 0) { \
			DEBUG(DBG_ERROR, "%s: can't read %s at line %d.\n", \
				__func__, #reg, __LINE__); \
			goto error; \
		} \
	} while (0)

static int cw1200_load_firmware_cw1260(struct CW1200_priv *priv ,
				uint8_t *firmware, uint32_t fw_length);

#ifdef FPGA
static int cw1200_load_firmware_fpga(struct CW1200_priv *priv,uint8_t *firmware,
					uint32_t fw_length)
{
	int ret = ERROR_SDIO;
	int block, num_blocks;
	unsigned i;
	u32 val32;
	u32 put = 0, get = 0;
	u8 *buf = NULL;
	uint32_t reg_value = 0;
	size_t block_size;

	/* Initialize common registers */

	/*Initializing Image size register*/
	reg_value = DOWNLOAD_ARE_YOU_HERE;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON +
					DOWNLOAD_IMAGE_SIZE_REG,
					(uint8_t *)&reg_value, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s:SBUS_SramWrite_APB()returned error \n",
			__func__);
		goto error;
	}

	/*Initializing Put register*/
	reg_value = 0;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_PUT_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Put failed \n",
				__func__);
		goto error;
	}

	/*Initializing get register*/
	reg_value = 0;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_GET_REG, (uint8_t *)&reg_value,
				sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Get failed \n",
				__func__);
		goto error;
	}

	/*Initializing Status register*/
	reg_value = DOWNLOAD_PENDING;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_STATUS_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Status Reg"
				" failed \n", __func__);
		goto error;
	}

	/*Initializing Download Flags register*/
	reg_value = 1;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_FLAGS_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Flag Reg failed \n",
				__func__);
		goto error;
	}

	buf = kmalloc(DOWNLOAD_BLOCK_SIZE, GFP_KERNEL | GFP_DMA);
	if (!buf) {
		DEBUG(DBG_ERROR, "%s: can't allocate bootloader buffer.\n", __func__);
		ret = -ENOMEM;
		goto error;
	}

	/* Check if the FPGA bootloader is ready */
	for (i = 0; i < 200; i += 1 + i / 2) {
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_IMAGE_SIZE_REG,
					(uint8_t *)&val32, BIT_32_REG))
			goto error;
		if (val32 == DOWNLOAD_I_AM_HERE)
			break;
		mdelay(i);
	} /* End of for loop */

	if (val32 != DOWNLOAD_I_AM_HERE) {
		DEBUG(DBG_ERROR, "%s: bootloader is not ready.\n", __func__);
		ret = -ETIMEDOUT;
		goto error;
	}

	/* Calculcate number of download blocks */
	num_blocks = (fw_length - 1) / DOWNLOAD_BLOCK_SIZE + 1;

	/* Updating the length in Download Ctrl Area */
	val32 = fw_length; /* Explicit cast from size_t to u32 */

	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON +
					DOWNLOAD_IMAGE_SIZE_REG,
					(uint8_t *)&val32, BIT_32_REG)) {
			DEBUG(DBG_ERROR, "%s:SBUS_SramWrite_APB()returned error \n",
				__func__);
			goto error;
		}

	/* Bootloader downloading loop */
	DEBUG(DBG_MESSAGE, "%s: NUM_BLOCKS = %d\n",__func__,num_blocks);
	for (block = 0; block < num_blocks ; block++) {
		/* check the download status */
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
						+ DOWNLOAD_STATUS_REG,
						(uint8_t *)&val32, BIT_32_REG)) {
			DEBUG(DBG_ERROR,
			"%s:SBUS_SramRead_APB()returned error \n",
					__func__);
			goto error;
		}
		if (val32 != DOWNLOAD_PENDING) {
			DEBUG(DBG_ERROR, "%s: bootloader reported error %d.\n",
				__func__, val32);
			ret = -EIO;
			goto error;
		}

		/*This loop waits for difference between put(blocks sent for writing)
		and get(blocks actually written) to be less than 24k, if even at
		the end of the loop it is not so then we timeout*/
		for (i = 0; i < 100; i++) {
			if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
						+ DOWNLOAD_GET_REG,
						(uint8_t *)&get,
						BIT_32_REG)) {
				DEBUG(DBG_ERROR, "Sram Read get failed \n");
				goto error;
			}

			if ((put - get) <=
			    (DOWNLOAD_FIFO_SIZE - DOWNLOAD_BLOCK_SIZE))
				break;
			mdelay(i);
		}

		if ((put - get) > (DOWNLOAD_FIFO_SIZE - DOWNLOAD_BLOCK_SIZE)) {
			DEBUG(DBG_ERROR, "%s: Timeout waiting for FIFO.\n",
				__func__);
			return -ETIMEDOUT;
		}

		/* calculate the block size */
		block_size = min((size_t)(fw_length - put),
			(size_t)DOWNLOAD_BLOCK_SIZE);

		memcpy(buf, &firmware[put], block_size);

		if (block_size < DOWNLOAD_BLOCK_SIZE) {
			memset(&buf[block_size],
				0, DOWNLOAD_BLOCK_SIZE - block_size);
		}

		if (SBUS_SramWrite_APB(priv,
					PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_FIFO_OFFSET
					+ (put & (DOWNLOAD_FIFO_SIZE - 1)),
					buf, DOWNLOAD_BLOCK_SIZE)) {
			DEBUG(DBG_ERROR, "%s:SRAM Write Error \n", __func__);
			goto error;
		}

		/* update the put register */
		put += block_size;
		if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_PUT_REG,
					(uint8_t *)&put, BIT_32_REG)) {
			DEBUG(DBG_ERROR, "%s: Sram Write update put failed\n",
					__func__);
			goto error;
		}
	} /* End of bootloader download loop */

	/* Wait for the download completion */
	for (i = 0; i < 600; i += 1 + i / 2) {
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_STATUS_REG,
				(uint8_t *)&val32, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s: Sram Read failed \n", __func__);
		goto error;
	}
	if (val32 != DOWNLOAD_PENDING)
		break;
	mdelay(i);
	}
	if (val32 != DOWNLOAD_SUCCESS) {
		DEBUG(DBG_ERROR, "%s: wait for download completion failed. " \
			"Read: 0x%.8X\n", __func__, val32);
		ret = -ETIMEDOUT;
	} else {
		DEBUG(DBG_ERROR, "Bin file download completed.\n");
		ret = 0;
	}
error:
	kfree(buf);
	return ret;
}

int cw1200_load_firmware_cw1260_fpga(struct CW1200_priv *priv, uint8_t *firmware,
		uint32_t fw_length, uint8_t *bootloader, uint32_t bl_length)
{

	int ret;
	u32 val32_1;

	ret = cw1200_reg_read_32(priv, ST90TDS_CONFIG_REG_ID, &val32_1);
	if (ret < 0) {
		DEBUG(DBG_ERROR, "%s: cant read BIT 15 of Config Reg.\n", __func__);
		goto error;
        }else {
		ret = cw1200_reg_write_16(priv, ST90TDS_CONFIG_REG_ID,
				val32_1 | 1<<15);
		if (ret < 0) {
			DEBUG(DBG_ERROR, "%s: Can't set valure of BIT 15 of Config Reg\n", __func__);
			goto error;
		}
	}

	ret = cw1200_load_firmware_fpga(priv, bootloader, bl_length);

	if(ret)
		goto error;

	DEBUG(DBG_MESSAGE, "%s: BOOTLOADER DOWNLOAD SUCCESS\n",__func__);

	ret = cw1200_load_firmware_fpga(priv, firmware, fw_length);
	if(ret)
		goto error;
	DEBUG(DBG_MESSAGE, "%s: FIRMWARE DOWNLOAD SUCCESS\n",__func__);

error:
	return ret;
}
#endif

int cw1200_load_bootloader_cw1260(struct CW1200_priv *priv, uint8_t *firmware,
		uint32_t fw_length, uint8_t *bootloader, uint32_t bl_length)
{
	int ret = ERROR_SDIO;
	u32 val32;
	int i;
	u32 addr = FIRMWARE_DLOAD_ADDR;
	u32 *data = (u32 *)bootloader;

	for(i = 0; i < (bl_length)/4; i++) {
		REG_WRITE(ST90TDS_SRAM_BASE_ADDR_REG_ID, addr);
		REG_WRITE(ST90TDS_AHB_DPORT_REG_ID,data[i]);
		REG_READ(ST90TDS_AHB_DPORT_REG_ID,val32);
		addr += 4;
	}

	DEBUG(DBG_MESSAGE, "%s:WRITE COMPLETE\n",__func__);

	ret = cw1200_load_firmware_cw1260(priv, firmware, fw_length);
error:
	return ret;
}


static int cw1200_load_firmware_cw1260(struct CW1200_priv *priv,
			uint8_t *firmware, uint32_t fw_length)
{
	int ret = ERROR_SDIO, block, num_blocks;
	unsigned i;
	u32 val32;
	u32 put = 0, get = 0;
	u8 *buf = NULL;
	uint32_t reg_value = 0;
	size_t block_size;

	/* Initialize common registers */

	/*Initializing Image size register*/
	reg_value = DOWNLOAD_ARE_YOU_HERE;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON +
					DOWNLOAD_IMAGE_SIZE_REG,
					(uint8_t *)&reg_value, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s:SBUS_SramWrite_APB()returned error \n",
			__func__);
		goto error;
	}

	/*Initializing Put register*/
	reg_value = 0;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_PUT_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Put failed \n",
				__func__);
		goto error;
	}

	/*Initializing get register*/
	reg_value = 0;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_GET_REG, (uint8_t *)&reg_value,
				sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Get failed \n",
				__func__);
		goto error;
	}

	/*Initializing Status register*/
	reg_value = DOWNLOAD_PENDING;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_STATUS_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Status Reg"
				" failed \n", __func__);
		goto error;
	}

	/*Initializing Download Flags register*/
	reg_value = 0;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_FLAGS_REG,
				(uint8_t *)&reg_value, sizeof(uint32_t))) {
		DEBUG(DBG_ERROR, "%s:ERROR: Download Crtl Flag Reg failed \n",
				__func__);
		goto error;
	}

	/* Release CPU from RESET */
	REG_READ(ST90TDS_CONFIG_REG_ID, val32);
	val32 &= ~ST90TDS_CONFIG_CPU_RESET_BIT;
	REG_WRITE(ST90TDS_CONFIG_REG_ID, val32);

	/* Enable Clock */
	val32 &= ~ST90TDS_CONFIG_CPU_CLK_DIS_BIT;
	REG_WRITE(ST90TDS_CONFIG_REG_ID, val32);

	buf = kmalloc(DOWNLOAD_BLOCK_SIZE, GFP_KERNEL | GFP_DMA);
	if (!buf) {
		DEBUG(DBG_ERROR,
			"%s: can't allocate firmware buffer.\n", __func__);
		ret = -ENOMEM;
		goto error;
	}

	/* Check if the bootloader is ready */
	for (i = 0; i < 100; i += 1 + i / 2) {
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_IMAGE_SIZE_REG,
				(uint8_t *)&val32, BIT_32_REG)){
		ret = ERROR_SDIO;
		goto error;
		}

		if (val32 == DOWNLOAD_I_AM_HERE)
			break;
		mdelay(i);
	} /* End of for loop */

	if (val32 != DOWNLOAD_I_AM_HERE) {
		DEBUG(DBG_ERROR,
			"%s: bootloader is not ready.\n", __func__);
		ret = -ETIMEDOUT;
		goto error;
	}

	/* Calculcate number of download blocks */
	num_blocks = (fw_length - 1) / DOWNLOAD_BLOCK_SIZE + 1;

	/* Updating the length in Download Ctrl Area */
	val32 = fw_length;
	if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON +
						DOWNLOAD_IMAGE_SIZE_REG,
						(uint8_t *)&val32, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s:SBUS_SramWrite_APB()returned error \n",
				__func__);
		ret = ERROR_SDIO;
		goto error;
	}

	/* Firmware downloading loop */
	for (block = 0; block < num_blocks ; block++) {
		/* check the download status */
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
						+ DOWNLOAD_STATUS_REG,
						(uint8_t *)&val32, BIT_32_REG)) {
			DEBUG(DBG_ERROR,
			"%s:SBUS_SramRead_APB()returned error \n",
					__func__);
			ret = ERROR_SDIO;
			goto error;
		}
		if (val32 != DOWNLOAD_PENDING) {
			DEBUG(DBG_ERROR,
				"%s: bootloader reported error %d.\n",
				__func__, val32);
			ret = -EIO;
			goto error;
		}

		/* loop until put - get <= 24K */
		for (i = 0; i < 100; i++) {
			if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
							+ DOWNLOAD_GET_REG,
						(uint8_t *)&get,BIT_32_REG)) {
				DEBUG(DBG_ERROR, "Sram Read get failed \n");
				ret = ERROR_SDIO;
				goto error;
			}
			if ((put - get) <=
			    (DOWNLOAD_FIFO_SIZE - DOWNLOAD_BLOCK_SIZE))
				break;
			mdelay(i);
		}

		if ((put - get) > (DOWNLOAD_FIFO_SIZE - DOWNLOAD_BLOCK_SIZE)) {
			DEBUG(DBG_ERROR,
				"%s: Timeout waiting for FIFO.\n",
				__func__);
			ret = -ETIMEDOUT;
			goto error;
		}

		/* calculate the block size */
		block_size = min((size_t)(fw_length - put),
			(size_t)DOWNLOAD_BLOCK_SIZE);

		memcpy(buf, &firmware[put], block_size);

		if (block_size < DOWNLOAD_BLOCK_SIZE) {
			memset(&buf[block_size],
				0, DOWNLOAD_BLOCK_SIZE - block_size);
		}

		if (SBUS_SramWrite_APB(priv,
					PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_FIFO_OFFSET
					+ (put & (DOWNLOAD_FIFO_SIZE - 1)),
					buf, DOWNLOAD_BLOCK_SIZE)) {
			DEBUG(DBG_ERROR, "%s:SRAM Write Error \n", __func__);
			ret = ERROR_SDIO;
			goto error;
		}

		/* update the put register */
		put += block_size;
		if (SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON
					+ DOWNLOAD_PUT_REG,
					(uint8_t *)&put, BIT_32_REG)) {
			DEBUG(DBG_ERROR, "%s: Sram Write update put failed\n",
					__func__);
			ret = ERROR_SDIO;
			goto error;
		}
	} /* End of firmware download loop */

	/* Wait for the download completion */
	for (i = 0; i < 300; i += 1 + i / 2) {
		if (SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON
				+ DOWNLOAD_STATUS_REG,
				(uint8_t *)&val32, BIT_32_REG)) {
		DEBUG(DBG_ERROR, "%s: Sram Read failed \n", __func__);
		ret = ERROR_SDIO;
		goto error;
		}
		if (val32 != DOWNLOAD_PENDING)
			break;
		mdelay(i);
	}
	if (val32 != DOWNLOAD_SUCCESS) {
		DEBUG(DBG_ERROR,
			"%s: wait for download completion failed. " \
			"Read: 0x%.8X\n", __func__, val32);
		ret = -ETIMEDOUT;
		goto error;
	} else {
		DEBUG(DBG_ERROR,
			"Firmware download completed.\n");
		ret = 0;
	}

error:
	kfree(buf);
	return ret;
}
