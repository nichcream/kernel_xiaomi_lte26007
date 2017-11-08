/*---------------------------------------------------------------------------------
 *   PROJECT     : CW1200 Linux driver - using STE-UMAC
 *   FILE  : sbus.c
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
/*-----------------------------------------------------------------------------*/

/*********************************************************************************
 *        								INCLUDE FILES
 **********************************************************************************/

#include "sbus.h"
#include "cw1200_common.h"
#include "eil.h"
#ifndef USE_SPI
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio.h>
#else
#include <linux/spi/spi.h>
#endif
#include <linux/delay.h>
#include <linux/stddef.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include "sbus_wrapper.h"

/******************************************************************************
 *								Global Variables
 *******************************************************************************/
#ifdef WORKAROUND
extern void irq_poll_init(struct CW1200_priv * priv);
extern void irq_poll_destroy(struct CW1200_priv * l_priv);
#endif

#define RX_BUFFER_SIZE 4096
static char* rx_buf;
static int abort_test = 0;
#ifndef INTERRUPTS_GPIO
#define INTERRUPTS_GPIO  
#endif
/*! \fn  inline void Increment_Wr_Buffer_Number(struct CW1200_priv * priv )
  \brief This function increements the WRITE buffer no.Please see HIF MAD doc for details.
  \param priv 	- pointer to device private structure.
  \return 		- None
 */
inline void Increment_Wr_Buffer_Number(struct CW1200_priv * priv )
{

	if( priv->sdio_wr_buf_num_qmode == 31 )
	{
		priv->sdio_wr_buf_num_qmode = 0 ;  // make it 0 - 31
	}
	else
	{
		priv->sdio_wr_buf_num_qmode++ ;
	}

}


/*! \fn  inline void Increment_Rd_Buffer_Number(struct CW1200_priv * priv )
  \brief This function increements the READ buffer no.Please see HIF MAD doc for details.
  \param priv 	- pointer to device private structure.
  \return 		- None
 */
inline void Increment_Rd_Buffer_Number(struct CW1200_priv * priv )
{

	if( priv->sdio_rd_buf_num_qmode == 4 )
	{
		priv->sdio_rd_buf_num_qmode = 1 ;  // make it 0 - 4
	}
	else
	{
		priv->sdio_rd_buf_num_qmode++ ;
	}

}


/*! \fn  int32_t SBUS_SDIO_RW_Reg( struct stlc9000_priv * priv, uint32 addr, char * rw_buf,
 *           uint16 length, int32 read_write )
 \brief This function reads/writes a register from the devuce
 \param priv 	- pointer to device private structure.
 \param addr	- Register address to read/write.
 \param rw_buf	- Pointer to buffer containing the write data and the buffer to store the data read.
 \param length 	- Length of the buffer to read/write
 \param read_write - 	Operation to perform - READ/WRITE
 \return 		- Status of the operation
 */
CW1200_STATUS_E SBUS_SDIO_RW_Reg( struct CW1200_priv * priv, uint32_t addr, uint8_t * rw_buf,
				  uint16_t length, int32_t read_write )
{

	uint32_t  sdio_reg_addr_17bit ;
	int32_t    retval;


	/* Check if buffer is aligned to 4 byte boundary */
	if ( ( ((unsigned)rw_buf) & 3) != 0 )
	{
		DEBUG(DBG_SBUS ,"FATAL Error:Buffer UnAligned \n");
		return ERROR_BUF_UNALIGNED;
	}

	/* Convert to SDIO Register Address */
	addr = SPI_REG_ADDR_TO_SDIO(addr);
	sdio_reg_addr_17bit = SDIO_ADDR17BIT( 0, 0, 0, addr) ;
#ifndef USE_SPI
	sdio_claim_host(priv->func);
#endif
	if ( SDIO_READ == read_write )
	{
		//READ
		retval = sbus_memcpy_fromio(priv->func, rw_buf,sdio_reg_addr_17bit,length);
	}
	else
	{
		//WRITE
		retval = sbus_memcpy_toio(priv->func,sdio_reg_addr_17bit,rw_buf,length);
	}
#ifndef USE_SPI
	sdio_release_host(priv->func);
#endif

	if(unlikely(retval))
	{
		DEBUG(DBG_ERROR,"SDIO Error [%d] \n",retval);
		return ERROR_SDIO;
	}
	else
		return SUCCESS;

}


/*! \fn  CW1200_STATUS_E SBUS_SramWrite_AHB(struct CW1200_priv * priv, uint32_t base_addr,
 *                                                                         uint8_t * buffer, uint32_t byte_count)
 \brief This function writes directly to AHB RAM.
 \param priv 	- pointer to device private structure.
 \param base_addr	- Base address in the RAM from where to upload firmware
 \param buffer	- Pointer to firmware chunk
 \param byte_count 	- Length of the buffer to read/write
 \return 		- Status of the operation
 */
CW1200_STATUS_E SBUS_SramWrite_AHB(struct CW1200_priv * priv, uint32_t base_addr,
				   uint8_t * buffer, uint32_t byte_count)
{

	int32_t retval;

	if( (byte_count/2) > 0xfff )
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_AHB: ERROR: Cannot write more than 0xfff words (requested = %d)\n", ((byte_count/2)));
		return ERROR_INVALID_PARAMETERS;
	}

	if((retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_SRAM_BASE_ADDR_REG_ID,(uint8_t *)&base_addr,BIT_32_REG,SDIO_WRITE)))
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_AHB: sbus write failed, error=%d \n", retval);
		return ERROR_SDIO;
	}


	if( byte_count < ( DOWNLOAD_BLOCK_SIZE -1) )
	{
		if((retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_AHB_DPORT_REG_ID,buffer,byte_count,SDIO_WRITE)))
		{
			DEBUG(DBG_SBUS,"SBUS_SramWrite_AHB: sbus write failed, error=%d \n", retval);
			return ERROR_SDIO;
		}
	}
	else
	{
		mdelay(1);
		/* Right now using Multi Byte */
		if((retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_AHB_DPORT_REG_ID,buffer,byte_count,SDIO_WRITE)))
		{
			DEBUG(DBG_SBUS,"SBUS_SramWrite_AHB:write failed, error=%d \n", retval);
			return ERROR_SDIO;
		}
	}
	return SUCCESS;
}


/*! \fn  CW1200_STATUS_E SBUS_Set_Prefetch(struct CW1200_priv * priv)
  \brief This function sets the prefect bit and waits for it to get cleared
  \param priv 	- pointer to device private structure.
 */
CW1200_STATUS_E SBUS_Set_Prefetch(struct CW1200_priv * priv)
{
	uint32_t config_reg_val = 0;
	uint32_t count=0;

	/* Read CONFIG Register Value - We will read 32 bits*/
	if(SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(uint8_t * )&config_reg_val,BIT_32_REG,SDIO_READ) )
	{
		return ERROR_SDIO;
	}

	config_reg_val = config_reg_val | ST90TDS_CONFIG_PFETCH_BIT;
	if(SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(uint8_t * )&config_reg_val,BIT_32_REG,SDIO_WRITE) )
	{
		return ERROR_SDIO;
	}
	/* Check for PRE-FETCH bit to be cleared */
	for( count = 0; count<20; count++ )
	{
		mdelay(1);
		/* Read CONFIG Register Value - We will read 32 bits*/
		if(SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(uint8_t * )&config_reg_val,BIT_32_REG,SDIO_READ) )
		{
			return ERROR_SDIO;
		}

		if(!(config_reg_val & ST90TDS_CONFIG_PFETCH_BIT))
		{
			break;
		}
	}

	if( count >=20 )
	{
		DEBUG(DBG_ERROR,"Prefetch bit not cleared \n");
		return ERROR;
	}

	return SUCCESS;
}

/*! \fn  CW1200_STATUS_E SBUS_SramWrite_APB(struct CW1200_priv * priv, uint32_t base_addr,
 *                                                                         uint8_t * buffer, uint32_t byte_count)
 \brief This function writes directly to SRAM.
 \param priv 	- pointer to device private structure.
 \param base_addr	- Base address in the RAM from where to write to.
 \param buffer	- Pointer to the buffer
 \param byte_count 	- Length of the buffer to read/write
 \return 		- Status of the operation
 */
CW1200_STATUS_E SBUS_SramWrite_APB(struct CW1200_priv * priv, uint32_t base_addr,
				   uint8_t * buffer, uint32_t byte_count)
{

	int32_t retval;

	if( (byte_count/2) > 0xfff )
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_APB: ERROR: Cannot write more than 0xfff words (requested = %d)\n", ((byte_count/2)));
		return ERROR_INVALID_PARAMETERS;
	}

	if((retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_SRAM_BASE_ADDR_REG_ID,(uint8_t *)&base_addr,BIT_32_REG,SDIO_WRITE)))
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_APB: sbus write failed, error=%d \n", retval);
		return ERROR_SDIO;
	}

	if((retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_SRAM_DPORT_REG_ID,buffer,byte_count,SDIO_WRITE)))
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_APB: sbus write failed, error=%d \n", retval);
		return ERROR_SDIO;
	}

	return SUCCESS;
}


/*! \fn  CW1200_STATUS_E SBUS_SramRead_APB(struct CW1200_priv * priv, uint32_t base_addr,
 *                                                                         uint8_t * buffer, uint32_t byte_count)
 \brief This function from APB-SRAM.
 \param priv 	- pointer to device private structure.
 \param base_addr	- Base address in the RAM from where to read from.
 \param buffer	- Pointer to the buffer
 \param byte_count 	- Length of the buffer to read/write
 \return 		- Status of the operation
 */
CW1200_STATUS_E SBUS_SramRead_APB(struct CW1200_priv * priv, uint32_t base_addr,
				  uint8_t * buffer, uint32_t byte_count)
{
	int32_t retval;

	if( (byte_count/2) > 0xfff )
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_APB: ERROR: Cannot write more than 0xfff words (requested = %d)\n", ((byte_count/2)));
		return ERROR_INVALID_PARAMETERS;
	}

	if((retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_SRAM_BASE_ADDR_REG_ID,(uint8_t *)&base_addr,BIT_32_REG,SDIO_WRITE)))
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_APB: sbus write failed, error=%d \n", retval);
		return ERROR_SDIO;
	}

	if( SBUS_Set_Prefetch(priv) )
		return ERROR;

	if((retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_SRAM_DPORT_REG_ID,buffer,byte_count,SDIO_READ)))
	{
		DEBUG(DBG_SBUS,"SBUS_SramWrite_APB: sbus write failed, error=%d \n", retval);
		return ERROR_SDIO;
	}

	return SUCCESS;

}

#define DUMP_PACKETS 0

#if 0
static u32 calcSum(unsigned char* ptr, u32 size)
{
	int i;
	u32 sum = 0;
	for (i=0;i<size;i++)
		sum += *(ptr + i);
	return sum;
}
#endif

/*! \fn  CW1200_STATUS_E SBUS_SDIOReadWrite_QUEUE_Reg( struct CW1200_priv * priv, uint32_t addr,uint8_t * rw_buf,
 *                                                                                 uint16_t length, int32_t read_write )
 \brief This function reads/writes device QUEUE register
 \param priv 	- pointer to device private structure.
 \param rw_buf	- pointer to buffer containing the write data or the buffer to copy the data read
 \param length 	- Data read/write length
 \param read_write	- READor WRITE operation.
 \return 		- Status of the operation
 */
CW1200_STATUS_E SBUS_SDIOReadWrite_QUEUE_Reg( struct CW1200_priv * priv,uint8_t * rw_buf,
					      uint16_t length, int32_t read_write )
{

	uint32_t  sdio_reg_addr_17bit ;
	int32_t    retval;
	int32_t  addr=0;
	uint32_t retry=0;
	uint16_t orig_length;

	orig_length = length;

	/* Convert to SDIO Register Address */
	addr = SPI_REG_ADDR_TO_SDIO(ST90TDS_IN_OUT_QUEUE_REG_ID);

	length = sdio_align_size(priv->func, length);

	sdio_claim_host(priv->func);

	if ( read_write == SDIO_READ )
	{
		//READ
		sdio_reg_addr_17bit = SDIO_ADDR17BIT( priv->sdio_rd_buf_num_qmode, 0, 0, addr) ;
		while(retry <3)
		{
			if( unlikely(retval = sbus_memcpy_fromio(priv->func, rw_buf,sdio_reg_addr_17bit,length))  )
			{
				printk("STEFAN DEBUG: SDIO read request failed with error %i! Retrying...\n", retval);
				retry++;
			}
			else
			{
				break;
			}
		}
	}
	else
	{
		//WRITE
		sdio_reg_addr_17bit = SDIO_ADDR17BIT( priv->sdio_wr_buf_num_qmode, 0, 0, addr ) ;
		while(retry <3)
		{
			if( unlikely(retval = sbus_memcpy_toio(priv->func,sdio_reg_addr_17bit,rw_buf,length))  )
			{
				printk("STEFAN DEBUG: SDIO write request failed with error %i! Retrying...\n", retval);
				retry++;
			}
			else
			{
				Increment_Wr_Buffer_Number( priv );
				break;
			}
		}

	}


	sdio_release_host(priv->func);

	/* Retries failed return ERROR to Caller */
	if(unlikely(retval))
		return ERROR_SDIO;
	else
		return SUCCESS;

}


/*! \fn  CW1200_STATUS_E CW1100_UploadFirmware(struct stlc9000_priv * priv,void * firmware,
 *     uint32_t length)
 \brief This function uploads firmware to the device
 \param priv 	- pointer to device private structure.
 \param firmware	- Pointer to the firmware image
 \param length 	- Length of the firmware image.
 \return 		- Status of the operation
 */
CW1200_STATUS_E CW1100_UploadFirmware(struct CW1200_priv * priv,void * firmware,uint32_t fw_length)
{

	uint32_t		i;
	uint32_t		status = SUCCESS;
	uint8_t  *		buffer_loc;
	uint32_t		num_blocks;
	uint32_t		length=0, remain_length=0;


	num_blocks = fw_length/DOWNLOAD_BLOCK_SIZE;

	if( (fw_length  - (num_blocks * DOWNLOAD_BLOCK_SIZE)) > 0 )
	{
		num_blocks = num_blocks +1;
	}

	buffer_loc= (uint8_t *)firmware;

	for(i=0;i<num_blocks;i++)
	{

		remain_length= fw_length - (i*DOWNLOAD_BLOCK_SIZE);
		if(remain_length >=DOWNLOAD_BLOCK_SIZE)
			length=DOWNLOAD_BLOCK_SIZE;
		else
			length=remain_length;

		if(SBUS_SramWrite_AHB(priv, (FIRMWARE_DLOAD_ADDR + i*DOWNLOAD_BLOCK_SIZE), buffer_loc, length) != SUCCESS)
		{
			status = ERROR;
			break;
		}
		buffer_loc+=length;
	}

	if(status != ERROR)
	{
		status = SUCCESS;
	}
	return status;
}


/*! \fn  CW1200_STATUS_E CW1200_UploadFirmware(struct stlc9000_priv * priv,void * firmware,
 *     uint32_t length)
 \brief This function uploads firmware to the device
 \param priv 	- pointer to device private structure.
 \param firmware	- Pointer to the firmware image
 \param length 	- Length of the firmware image.
 \return 		- Status of the operation
 */
CW1200_STATUS_E CW1200_UploadFirmware(struct CW1200_priv * priv,uint8_t * firmware,uint32_t fw_length)
{
	uint32_t reg_value = 0;
	download_cntl_t download={0};
	uint32_t config_reg_val = 0;
	uint32_t count=0;
	uint32_t num_blocks =0;
	uint32_t block_size=0;
	uint8_t * buffer_loc = NULL;
	uint32_t  i =0;

	// Initializing the download control Area with bootloader Signature */
	reg_value = DOWNLOAD_ARE_YOU_HERE;

	if(SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_IMAGE_SIZE_REG,
			      (uint8_t *)&reg_value, BIT_32_REG))
	{
		DEBUG(DBG_ERROR,"%s:SBUS_SramWrite_APB() returned error \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	download.Flags = 0; /* BIT 0 should be set to 1 if the device UART is to be used */
	download.Put = 0;
	download.Get = 0;
	download.Status = DOWNLOAD_PENDING;

	reg_value = download.Put;
	if(SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_PUT_REG,
			      (uint8_t *)&reg_value, sizeof(uint32_t)))
	{
		DEBUG(DBG_ERROR,"%s:ERROR: Download Crtl Put failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	reg_value = download.Get;
	if(SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_GET_REG,
			      (uint8_t *)&reg_value, sizeof(uint32_t)))
	{
		DEBUG(DBG_ERROR,"%s:ERROR: Download Crtl Get failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	reg_value = download.Status;
	if(SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_STATUS_REG,
			      (uint8_t *)&reg_value, sizeof(uint32_t)))
	{
		DEBUG(DBG_ERROR,"%s:ERROR: Download Crtl Status Reg failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	reg_value = download.Flags;
	if(SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_FLAGS_REG,
			      (uint8_t *)&reg_value, sizeof(uint32_t)))
	{
		DEBUG(DBG_ERROR,"%s:ERROR: Download Crtl Flag Reg failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	/* Write the NOP Instruction */
	reg_value = 0xFFF20000;
	if( SBUS_SDIO_RW_Reg(priv, ST90TDS_SRAM_BASE_ADDR_REG_ID, (uint8_t *)&reg_value, BIT_32_REG, SDIO_WRITE) )
	{
		DEBUG(DBG_ERROR,"%s:ERROR:SRAM Base Address Reg Failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	reg_value = 0xEAFFFFFE;
	if( SBUS_SDIO_RW_Reg(priv, ST90TDS_AHB_DPORT_REG_ID, (uint8_t *)&reg_value, BIT_32_REG, SDIO_WRITE) )
	{
		DEBUG(DBG_ERROR,"%s:ERROR:NOP Write Failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	/* Release CPU from RESET */
	if( SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,BIT_32_REG,SDIO_READ) )
	{
		DEBUG(DBG_ERROR,"%s:ERROR:Read Config Reg Failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	config_reg_val = config_reg_val & ST90TDS_CONFIG_CPU_RESET_MASK;
	if( SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,BIT_32_REG,SDIO_WRITE))
	{
		DEBUG(DBG_ERROR,"%s:ERROR:Write Config Reg Failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	/* Enable Clock */
	config_reg_val = config_reg_val & ST90TDS_CONFIG_CPU_CLK_DIS_MASK;
	if( SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,BIT_32_REG ,SDIO_WRITE) )
	{
		DEBUG(DBG_ERROR,"%s:ERROR:Write Config Reg Failed \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	DEBUG(DBG_SBUS,"SBUS:CW1200_UploadFirmware:Waiting for Bootloader to be ready \n");
	/* Check if the bootloader is ready */
	for( count=0; count<100; count++ )
	{
		mdelay(1);
		if( SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_IMAGE_SIZE_REG,
				      (uint8_t *)&reg_value, BIT_32_REG) )
		{
			return ERROR;
		}

		if(reg_value == DOWNLOAD_I_AM_HERE)
		{
			DEBUG(DBG_SBUS,"BootLoader Ready \n");
			break;
		}
	} /* End of for loop */

	if( count >= 100 )
	{
		DEBUG(DBG_ERROR,"Bootloader not ready:Timeout \n");
		return ERROR;
	}

	/* Calculcate number of download blocks */
	num_blocks = fw_length/DOWNLOAD_BLOCK_SIZE;

	if( (fw_length  - (num_blocks * DOWNLOAD_BLOCK_SIZE)) > 0 )
	{
		num_blocks = num_blocks +1;
	}

	/* Updating the length in Download Ctrl Area */
	download.ImageSize = fw_length;
	reg_value = download.ImageSize;

	printk("About to download %i bytes in %i blocks\n", reg_value, num_blocks);

	if(SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_IMAGE_SIZE_REG,
			      (uint8_t *)&reg_value, BIT_32_REG))
	{
		DEBUG(DBG_ERROR,"%s:SBUS_SramWrite_APB() returned error \n", __FUNCTION__);
		return ERROR_SDIO;
	}

	// Firmware downloading loop
	for(i=0; i < num_blocks ; i++)
	{
		// check the download status
		reg_value = 0;
		if(SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_STATUS_REG,
				     (uint8_t *)&reg_value, BIT_32_REG))
		{
			DEBUG(DBG_ERROR,"%s:SBUS_SramRead_APB() returned error \n", __FUNCTION__);
			return ERROR;
		}

		download.Status = reg_value;
//		DEBUG(DBG_SBUS,"SBUS:CW1200_UploadFirmware():download status = 0x%08x \n", download.Status);

		if(download.Status != DOWNLOAD_PENDING)
		{// FW loader reporting an error status
			if(download.Status == DOWNLOAD_EXCEPTION)
			{// read exception data
				DEBUG(DBG_SBUS,"SBUS:CW1200_UploadFirmware(): ERROR: Loader DOWNLOAD_EXCEPTION \n");
				SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_DEBUG_DATA_REG, \
						  (uint8_t *)&download.DebugData[0], DOWNLOAD_DEBUG_DATA_LEN);
			}
			return ERROR;
		}

		// loop until put - get <= 24K
		for(count = 0; count <= 200; count ++)
		{
			reg_value = 0;
			if(SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_GET_REG, (uint8_t *)&reg_value, BIT_32_REG))
			{
				DEBUG(DBG_SBUS,"Sram Read get failed \n");
				return ERROR_SDIO;
			}

			download.Get = reg_value;
			if((download.Put - download.Get) <= (DOWNLOAD_FIFO_SIZE - DOWNLOAD_BLOCK_SIZE))
			{// OK we can put
				break;
			}
			mdelay(1);				// 1 Mil sec
		}

		if(count >= 200)
		{
			DEBUG(DBG_ERROR,"SBUS:CW1200_Uploadfirmware():PUT-GET timedout \n");
			return ERROR;
		}
		// get the block size
		if((download.ImageSize - download.Put) >= DOWNLOAD_BLOCK_SIZE)
		{
			block_size = DOWNLOAD_BLOCK_SIZE;
		}
		else
		{
			block_size = download.ImageSize - download.Put;    // bytes
		}
		buffer_loc = firmware + download.Put;  /* In bytes */

		/* send the block to sram */
		if(SBUS_SramWrite_APB(
				      priv,
				      PAC_SHARED_MEMORY_SILICON + DOWNLOAD_FIFO_OFFSET + (download.Put & (DOWNLOAD_FIFO_SIZE - 1)),
				      buffer_loc, block_size))
		{
			DEBUG(DBG_ERROR,"SBUS:CW1200_Uploadfirmware():SRAM Write Error \n");
			return ERROR;
		}

//		DEBUG(DBG_SBUS,"WDEV_Start_CW1200: Block %d loaded \n", i);
		// update the put register
		download.Put = download.Put + block_size;
		reg_value = download.Put;
		if(SBUS_SramWrite_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_PUT_REG, (uint8_t *)&reg_value, BIT_32_REG))
		{
			DEBUG(DBG_SBUS,"SBUS:CW1200_Uploadfirmware() Sram Write update put failed \n");
			return ERROR;
		}

	}/*End of firmware download loop*/
	// wait for the download completion
	count = 0;
	reg_value = 0;
	if(SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_STATUS_REG, (uint8_t *)&reg_value, BIT_32_REG) )
	{
		DEBUG(DBG_SBUS,"SBUS:CW1200_Uploadfirmware() Sram Read failed \n");
		return ERROR;
	}
	download.Status = reg_value;
	while(download.Status == DOWNLOAD_PENDING)
	{
		mdelay(5);
		reg_value = 0;
		if(SBUS_SramRead_APB(priv, PAC_SHARED_MEMORY_SILICON + DOWNLOAD_STATUS_REG, (uint8_t *)&reg_value, BIT_32_REG) )
		{
			DEBUG(DBG_SBUS,"SBUS:CW1200_Uploadfirmware() Sram Read failed \n");
			return ERROR;
		}
		download.Status = reg_value;
		count++;
		if(count >= 100)
		{
			DEBUG(DBG_SBUS,"SBUS:CW1200_Uploadfirmware():Status not changed after firmware upload \n");
			break;
		}
	}

	if( download.Status != DOWNLOAD_SUCCESS )
	{
		DEBUG(DBG_SBUS,"SBUS:CW1200: download failed! Reason: %i\n", download.Status);
		return ERROR;
	}
	return SUCCESS;
}



/*! \fn  static void SBUS_BH(struct work_struct *work)
  \brief This function implements the SBUS bottom half handler
  \param work 	- pointer to the Work Queue work_struct
  \return 		- None
 */
static void SBUS_bh(struct work_struct *work)
{

	uint32_t    cont_reg_val = 0 ;
	uint32_t    read_len = 0;
	uint32_t    rx_count = 0;
	uint32_t    alloc_len = 0;
	hif_rw_msg_hdr_payload_t *	payload;
	HI_STARTUP_IND     * startup_ind;
	HI_EXCEPTION_IND   * exception_ind;
	char buf[ HI_SW_LABEL_MAX + 1 ] ;
	uint16_t    msgid=0;
	int32_t retval = SUCCESS;
	UMI_GET_TX_DATA *pTxData = NULL;
//	struct sk_buff * skb_rx = NULL;
	struct sk_buff * skb_tx = NULL;
	uint32_t tx_len = 0;
	struct CW1200_priv *priv = container_of(work, struct CW1200_priv,sbus_work);
#ifdef MOP_WORKAROUND
	int tmp, bufflen =0;
	char *buff;
#endif


//	DEBUG(DBG_SBUS,"SBUS:SBUS_bh Called\n");
#ifdef WORKAROUND
	down(&priv->sem);
#endif

	/********************** Receive ******************************/
RX_INT:
	/* If the device raised an Interrupt enter the RX code */
	if( TRUE == atomic_read( &(priv->Interrupt_From_Device)))
	{
		/* Clear the Local Interrupt Flag */
		atomic_set( &(priv->Interrupt_From_Device),FALSE);
		/*  Read Control register to retrieve the buffer len */
		if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONTROL_REG_ID,(char *)&cont_reg_val,BIT_16_REG,SDIO_READ) ) )
		{
			DEBUG(DBG_SBUS,"SDIO Read/Write Error \n");
			goto err_out;
		}

		cont_reg_val = (uint16_t ) ( cont_reg_val & ST90TDS_SRAM_BASE_LSB_MASK ) ;

		/* Extract next message length(in words) from CONTROL register */
		read_len = SPI_CONT_NEXT_LENGTH(cont_reg_val);
		/* Covert length to length in BYTES */
		read_len = read_len * 2;

		/* For restarting Piggyback Jump here */
RX:
		rx_count = 0;

		/* Repeat the RX Loop 2 times.Imperical Value. Can be changed*/
		while((rx_count < 1) && (read_len > 0))
		{
			if(   (read_len < sizeof(HI_MSG_HDR) ) || read_len > (MAX_SZ_RD_WR_BUFFERS-PIGGYBACK_CTRL_REG ) )
			{
				DEBUG(DBG_ERROR,"SBUS:SBUS_bh():read buffer length not correct: rx_count = %i, read_len = %i\n", rx_count, read_len);
				goto err_out;
			}

			/* Add SIZE of PIGGYBACK reg (CONTROL Reg) to the NEXT Message length + 2 Bytes for SKB */
			read_len = read_len +  2;


			if(read_len <= SDIO_BLOCK_SIZE)
				alloc_len = SDIO_BLOCK_SIZE;
			else
				alloc_len = read_len + SDIO_BLOCK_SIZE;

			
			if( (retval = SBUS_SDIOReadWrite_QUEUE_Reg(priv, rx_buf, read_len, SDIO_READ)) )
			{
				DEBUG(DBG_SBUS,"SDIO  Queue Read/Write Error \n");
				goto err_out_free;
			}

			/*Update CONTROL Reg value from the DATA read and update NEXT Message Length */
			payload = (hif_rw_msg_hdr_payload_t *)(rx_buf);

			cont_reg_val = *( (uint16_t *) ( rx_buf +  (uint8_t)(payload->hdr.MsgLen ) ) );

			/* Extract Message ID from header */
			msgid = (uint16_t)(payload->hdr.MsgId);

			/*If Message ID is EXCEPTION then STOP HIF Communication and Report ERROR to WDEV */
			if( unlikely( ( msgid & MSG_ID_MASK ) == HI_EXCEPTION_IND_ID ) )
			{
				exception_ind = (HI_EXCEPTION_IND *)payload;

				DEBUG(DBG_ERROR,"SBUS_bh():Exception Reason [%x] \n",exception_ind->Reason);
				memset(buf,0,sizeof(buf));
				memcpy(buf,exception_ind->FileName,HI_EXCEP_FILENAME_SIZE);

				DEBUG(DBG_SBUS ,"SBUS_bh():File Name:[%s],Line No [%x],R2 [%x]\n",buf,exception_ind->R1,
				      exception_ind->R2);
				//printk("Skipping abort,...\n");

				{
					uint32_t config_reg;
					uint32_t control_reg;
					int32_t err;


					printk("Reading out CONFIG register to get cause...\n");

					err = SBUS_SDIO_RW_Reg (priv, ST90TDS_CONFIG_REG_ID,
								(char *)&config_reg, BIT_32_REG, SDIO_READ);
					if (err == 0) {
						printk("CONFIG register was 0x%.8X, analysis:\n", config_reg);
						if (config_reg & (1 << 0)) printk("   ERR 0: Buffer number missmatch!\n");
						if (config_reg & (1 << 1)) printk("   ERR 1: Host tries to read data when hif buffers underrun!\n");
						if (config_reg & (1 << 2)) printk("   ERR 2: Host tries to read data less than output message length!\n");
						if (config_reg & (1 << 3)) printk("   ERR 3: Host tries to read data with no hif output queue entry programmed!\n");
						if (config_reg & (1 << 4)) printk("   ERR 4: Host tries to send data when hif buffers overrun!\n");
						if (config_reg & (1 << 5)) printk("   ERR 5: Host tries to send data larger than hif input buffer!\n");
						if (config_reg & (1 << 6)) printk("   ERR 6: Host tries to send data with no hif input queue entry programmed!\n");
						if (config_reg & (1 << 7)) printk("   ERR 7: Host misses CRC error!\n");
					}
					else
						printk("Error %i when trying to read CONFIG register!\n", err);

					printk("Reading out CONTROL register to get state of CW1200...\n");
					err = SBUS_SDIO_RW_Reg(priv, ST90TDS_CONTROL_REG_ID,
							       (char *)&control_reg, BIT_16_REG, SDIO_READ);
					if (err == 0)
						printk("CONTROL register was 0x%.4X\n", control_reg);
					else
						printk("Error %i when trying to read CONTROL register!\n", err);
				}


				goto err_out_free;
			}

			/*If Message ID Is STARTUP then read the initialisation info from the Payload */
			if( unlikely( ( msgid & MSG_ID_MASK ) == HI_STARTUP_IND_ID ) )
			{
				startup_ind = (HI_STARTUP_IND *)payload;
				memset(buf,0,sizeof(buf));
				memcpy(buf,startup_ind->FirmwareLabel,HI_SW_LABEL_MAX);
				PRINT("Firmware Label : [%s] \n",buf);
				PRINT("InitStatus : [%d]\n",startup_ind->InitStatus);

				priv->max_size_supp = startup_ind->SizeInpChBuf;
				priv->max_num_buffs_supp = startup_ind->NumInpChBufs;

			}

			/* Extract SEQUENCE Number from the Message ID and check whether it is the same as expected */
			/*If SEQUENCE Number mismatches then Shutdown */
			if ( SBUS_GET_MSG_SEQ( payload->hdr.MsgId ) != ( priv->in_seq_num & HI_MSG_SEQ_RANGE ) )
			{
				DEBUG(DBG_ERROR,"FATAL Error:SEQUENCE Number Mismatch in Received Packet [%x],[%x] \n",
				      SBUS_GET_MSG_SEQ(payload->hdr.MsgId),priv->in_seq_num & HI_MSG_SEQ_RANGE);
				//printk("Skipping abort,...\n");
				goto err_out_free;
			}

			priv->in_seq_num++ ;
			/* Increement READ Queue Register Number */
			Increment_Rd_Buffer_Number(priv);

			/*If MESSAGE ID contains CONFIRMATION ID then Increement TX_CURRENT_BUFFER_COUNT */
			if ( payload->hdr.MsgId & HI_CNF_BASE )
			{
				if ( priv->num_unprocessed_buffs_in_device > 0 )
				{
					priv->num_unprocessed_buffs_in_device-- ;
				}
				else
				{
					DEBUG(DBG_ERROR,"HIF Lost synchronisation. Recd CONFIRM message but there was no corresponding REQUEST message\n" )  ;
				}
			}

			payload->hdr.MsgId &= MSG_ID_MASK;

			/* Pass the packet to UMAC */
			retval = UMI_ReceiveFrame(priv->lower_handle, NULL, NULL);
			rx_count++;

			if(retval != UMI_STATUS_SUCCESS )
			{
				DEBUG(DBG_SBUS,"SBUS_bh(): UMI_ReceiveFrame() returned error \n");
			}
			/* Update the piggyback length */
			if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONTROL_REG_ID,(char *)&cont_reg_val,BIT_16_REG,SDIO_READ) ) )
			{
				DEBUG(DBG_SBUS,"SDIO-Read/Write Error \n");
				goto err_out;
			}
			cont_reg_val = (uint16_t ) ( cont_reg_val & ST90TDS_SRAM_BASE_LSB_MASK ) ;

			/*If NEXT Message Length is greater than ZERO then goto step 3 else EXIT Bottom Half*/
			read_len = (SPI_CONT_NEXT_LENGTH(cont_reg_val));
			read_len = read_len * 2;

		}

	}/* End of RX Code */

	/********************** Transmit ******************************/
TX:
	/* Check if TX is possible */
	if( priv->num_unprocessed_buffs_in_device < priv->max_num_buffs_supp)
	{
		/* Check if UMAC has indicated that it has a packet to send */
		if( TRUE == atomic_read( &(priv->Umac_Tx)) )
		{
			retval = UMI_GetTxFrame( priv->lower_handle, &pTxData);
			if( unlikely(UMI_STATUS_SUCCESS != retval ) )
			{
				if( UMI_STATUS_NO_MORE_DATA == retval)
				{
					atomic_set( &(priv->Umac_Tx),FALSE);
					/* If Piggyback is active then restart RX */
					if(read_len)
						goto RX;
					else /* We do not having anything to do ,hence exit bottom half */
					{
#ifdef WORKAROUND
						up(&priv->sem);
#endif
						return;
					}
				}
				else
				{  /* Supposedly a FATAL Error*/
					DEBUG(DBG_ERROR,"SBUS_bh();UMI_GetTxFrame returned error \n");
					goto err_out;
				}
			} /* If -else check for retval ends here */
			skb_tx = (struct sk_buff *)(pTxData->pDriverInfo);
			if( NULL != skb_tx)
			{
				tx_len = skb_tx->len;
				/* Memove data if UMAC has added a 2 bytes hole in case of QOS*/
				if(  (pTxData->pTxDesc + pTxData->bufferLength) != pTxData->pDot11Frame )
				{
					memmove( (pTxData->pTxDesc + pTxData->bufferLength) ,pTxData->pDot11Frame,
						 tx_len);
				}
			}
			/* Increement TX Buffer Count */
			priv->num_unprocessed_buffs_in_device++;
			/* Write Buffer to the device */
			payload =   (hif_rw_msg_hdr_payload_t *)(pTxData->pTxDesc);
			/* Update HI Message Seq Number */
			HI_PUT_MSG_SEQ( payload->hdr.MsgId, (priv->out_seq_num & HI_MSG_SEQ_RANGE) );
			priv->out_seq_num++;
			tx_len = 0;

			if( (retval = SBUS_SDIOReadWrite_QUEUE_Reg(priv, pTxData->pTxDesc,
								   payload->hdr.MsgLen, SDIO_WRITE)) )
			{
				DEBUG(DBG_ERROR,"SBUS_bh():Device Data Queue Read/Write Error \n");
				goto err_out;
			}
		}/* If - else check for Umac_Tx flag ends here */
	}
	/* Check is Piggyback is active and needs to be restarted */
	if(read_len)
	{
		goto RX;
	} /* If interrupt was raised restart RX Interrupt Handling */
	else  if( TRUE == atomic_read( &(priv->Interrupt_From_Device)))
	{
		goto RX_INT;
	} /* if UMAC has packet to send restart TX */
	else if( TRUE == atomic_read( &(priv->Umac_Tx)) )
	{
		if( priv->num_unprocessed_buffs_in_device < priv->max_num_buffs_supp)
			goto TX;
	}

#ifdef WORKAROUND
	up(&priv->sem);
#endif
	return;

err_out_free:
err_out:
	DEBUG(DBG_SBUS,"SBUS_bh():FATAL Error:Shutdown driver \n");
#ifdef WORKAROUND
	up(&priv->sem);
#endif
	EIL_Shutdown(priv);
	abort_test = 1;
	return;

}


#ifndef USE_SPI
#ifdef INTERRUPTS_SDIO
void cw1200_sbus_interrupt(CW1200_bus_device_t *func)
{
	struct CW1200_priv * priv = NULL;

//	printk("%s \n",__func__);

	priv = sdio_get_drvdata(func);

	atomic_set( &(priv->Interrupt_From_Device),TRUE);
	queue_work(priv->sbus_WQ, &priv->sbus_work);
}
#elif defined INTERRUPTS_GPIO
irqreturn_t cw1200_sbus_interrupt(int irq, void *dev_id)
{
	struct CW1200_priv * priv = dev_id;

	atomic_set(&(priv->Interrupt_From_Device), TRUE);
	queue_work(priv->sbus_WQ, &priv->sbus_work);
	return IRQ_HANDLED;
}
#else
#error No type of IRQ for SDIO specified! Edit Makefile to define either INTERRUPTS_SDIO or INTERRUPTS_GPIO
#endif
#else
/* SPI interrupts */
irqreturn_t cw1200_sbus_interrupt(int irq, void *dev_id)
{
	struct CW1200_priv * priv = dev_id;

	atomic_set(&(priv->Interrupt_From_Device), TRUE);
	queue_work(priv->sbus_WQ, &priv->sbus_work);
	return IRQ_HANDLED;
}
#endif

/**************************************************************************************
 *                                                            UMAC Callback Functions
 **************************************************************************************/

/*! \fn  UMI_STATUS_CODE  UMI_CB_ScheduleTx ( LL_HANDLE	LowerHandle )
  \brief This function writes the buffer onto the device using the SDIO Host Controller Driver.
  \param priv 	- pointer to driver private data.
  \param pTxData	- pointer the TX data descriptor.
  \return 		- Status Code
 */
UMI_STATUS_CODE  UMI_CB_ScheduleTx ( LL_HANDLE LowerHandle )
{
	struct CW1200_priv * priv = (struct CW1200_priv * )LowerHandle;


	atomic_set( &(priv->Umac_Tx),TRUE);

	/* Check if TX is possible */
	if( priv->num_unprocessed_buffs_in_device < priv->max_num_buffs_supp)
	{
		queue_work(priv->sbus_WQ, &priv->sbus_work);
	}
	else {
		printk("Unable to schedule TX work: %i < %i\n", priv->num_unprocessed_buffs_in_device, priv->max_num_buffs_supp);
	}
	return UMI_STATUS_SUCCESS;
}


/*! 	\fn  LL_HANDLE  UMI_CB_Create (UMAC_HANDLE UMACHandle,UL_HANDLE ulHandle)
  \brief This function will create the SBUS layer
  \param	UMACHandle	-	Handle to  UMAC instance.
  \param     ulHandle           - 	Upper layer driver handle.
  \return 		- Handle to Lower Layer.
 */
LL_HANDLE  UMI_CB_Create (UMI_HANDLE UMACHandle,UL_HANDLE ulHandle)
{

	int32_t retval = SUCCESS;
	struct CW1200_priv * priv;


	priv = (struct CW1200_priv * )ulHandle;
	priv->lower_handle = UMACHandle;
	/* Create WORK Queue */
	priv->sbus_WQ = create_singlethread_workqueue("sbus_work");
	/* 1. Register BH */
	INIT_WORK(&priv->sbus_work, SBUS_bh);
#ifdef WORKAROUND
	sema_init(&priv->sem,1);
#endif

	/* Init device buffer numbers */
	priv->sdio_wr_buf_num_qmode = 0;
	priv->sdio_rd_buf_num_qmode = 1;

	/* Initialise Interrupt from device flag to FALSE */
	atomic_set( &(priv->Interrupt_From_Device),FALSE);
	atomic_set( &(priv->Umac_Tx),FALSE);
	//atomic_set( &(priv->buf_index),0);
	priv->buf_index=0;
	memset(priv->time_buf,sizeof(priv->time_buf),0);

#ifndef USE_SPI
	sdio_claim_host(priv->func);
	DEBUG(DBG_SBUS,"SDIO - VENDORID [%x],DEVICEID [%x] \n",priv->func->vendor,priv->func->device);

	if( (retval = sdio_enable_func(priv->func)))
	{
		DEBUG(DBG_ERROR,"%s, Error :[%d] \n",__FUNCTION__ ,retval );
		return NULL;
	}
	sdio_release_host(priv->func);
	sdio_set_drvdata(priv->func, priv);
#else
	if( (retval = request_irq(priv->func->irq, cw1200_sbus_interrupt,
				  IRQF_TRIGGER_FALLING, "CW1200_spi", priv)) )
	{
		DEBUG(DBG_ERROR,"%s, request_irq() return error :[%d] \n",__FUNCTION__ ,retval );
		return NULL;
	}

	spi_set_drvdata(priv->func, priv);
#endif

	/* Allocate our large, reusable RX buffer */
	rx_buf = kmalloc(RX_BUFFER_SIZE, GFP_KERNEL);

	/* The Lower layer handle is the same as the upper layer handle - device priv */
	return (LL_HANDLE *)priv;

}

/*! 	\fn  UMI_STATUS_CODE  UMI_CB_Start (LL_HANDLE  		LowerHandle,uint32_t FmLength,
 *								    void * FirmwareImage)
 \brief 	This callback function will start the firmware downloading.
 \param	LowerHandle	-	Handle to lower driver instance.
 \param	FmLength	-	Length of firmware image.
 \param	FirmwareImage -	Path to the Firmware Image.
 \return 	- UMI Status Code
 */
UMI_STATUS_CODE  UMI_CB_Start (LL_HANDLE  		LowerHandle,uint32_t FmLength,
			       void * FirmwareImage)
{

	struct CW1200_priv * priv = (struct CW1200_priv * )LowerHandle;
	UMI_STATUS_CODE retval = UMI_STATUS_SUCCESS;
	uint32_t config_reg_val =0 ;
	uint32_t dpll_buff  = 0;
	uint32_t dpll_buff_read  = 0;
	uint32_t cont_reg_val =0  ;
	uint32_t i=0;
#ifdef MOP_WORKAROUND
	int temp = 0;
	char * firm_buff = NULL;
#endif
	uint32_t configmask_cw1200 = 0;
	uint32_t configmask_silicon_vers = 0;
	uint32_t config_reg_len = 0;
#ifdef INTERRUPTS_GPIO
	int32_t err_val=0;
	uint8_t cccr=0;
	uint32_t func_num;
#endif

	/* Read CONFIG Register Value - We will read 32 bits*/
	if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char * )&config_reg_val,BIT_32_REG,SDIO_READ)) )
		return UMI_STATUS_FAILURE;

	DEBUG(DBG_MESSAGE,"Initial 32 Bit CONFIG Register Value: [%x] \n",config_reg_val);

	configmask_cw1200 = (config_reg_val >> 24) & 0x3;
	configmask_silicon_vers = (config_reg_val >> 31) & 0x1;

	/* Check if we have CW1200 or STLC9000 */
	if((configmask_cw1200 == 0x1) || (configmask_cw1200 == 0x2))
	{
		PRINT("CW1200 Silicon Detected \n");
		if(configmask_silicon_vers)
		{
			priv->hw_type = HIF_8601_VERSATILE;
		}
		else
		{
			priv->hw_type = HIF_8601_SILICON;
		}
		config_reg_len = BIT_32_REG;
		dpll_buff = DPLL_INIT_VAL_CW1200;
	}
	else
	{
		PRINT("STLC 9000 Silicon Detected \n");
		priv->hw_type = HIF_9000_SILICON_VERSTAILE;
		config_reg_len = BIT_16_REG;
		dpll_buff = DPLL_INIT_VAL_9000;
	}

	if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_TSET_GEN_R_W_REG_ID,(uint8_t *)&dpll_buff,BIT_32_REG,SDIO_WRITE)) )
		return UMI_STATUS_FAILURE;

	msleep(20);

	/* Read DPLL Reg value and compare with value written */
	if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_TSET_GEN_R_W_REG_ID,(uint8_t *)&dpll_buff_read,BIT_32_REG,SDIO_READ)) )
		return UMI_STATUS_FAILURE;

	if(dpll_buff_read != dpll_buff)
	{
		DEBUG(DBG_ERROR,"Unable to initialise DPLL register.Value Read is : [%x] \n",dpll_buff_read);
	}

	/*  Set Wakeup bit in device */
	if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONTROL_REG_ID,(char *)&cont_reg_val,BIT_16_REG,SDIO_READ) ) )
		return UMI_STATUS_FAILURE;

	cont_reg_val = cpu_to_le16( cont_reg_val | ST90TDS_CONT_WUP_BIT ) ;

	if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONTROL_REG_ID,(char *)&cont_reg_val,BIT_16_REG,SDIO_WRITE) ) )
		return UMI_STATUS_FAILURE;

	for(i = 0 ;i<300;i++)
	{
		msleep(1);
		if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONTROL_REG_ID,(char *)&cont_reg_val,BIT_16_REG,SDIO_READ) ) )
			return UMI_STATUS_FAILURE;

		if( cont_reg_val & ST90TDS_CONT_RDY_BIT )
		{
			DEBUG(DBG_SBUS,"WLAN is ready [%x],[%x]\n",i,cont_reg_val);
			break;
		}
	}

	if(i==299)
	{
		DEBUG(DBG_SBUS,"WLAN READY Bit not set \n");
		return UMI_STATUS_FAILURE;
	}

	/* Checking for access mode */
	if( ( retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,config_reg_len,SDIO_READ))  )
		return UMI_STATUS_FAILURE;

	if( config_reg_val & ST90TDS_CONFIG_ACCESS_MODE_BIT)
	{
		DEBUG(DBG_SBUS,"We are in DIRECT Mode \n");
	}
	else
	{
		DEBUG(DBG_ERROR," ERROR: We are in QUEUE Mode \n");
		return UMI_STATUS_FAILURE;
	}
	/* 5. Call function to download firmware */
	/* If there was some error.Report ERROR to WDEV */
#ifdef MOP_WORKAROUND
	temp = FmLength%RWBLK_SIZE?(FmLength - FmLength%RWBLK_SIZE + RWBLK_SIZE):FmLength;
	firm_buff = kmalloc(temp,GFP_KERNEL);
	firm_buff = memset(firm_buff,0,temp);
	firm_buff = memcpy(firm_buff,FirmwareImage,FmLength);
	if( HIF_8601_SILICON  == priv->hw_type )
	{
		if( (retval = CW1200_UploadFirmware(priv,firm_buff,temp)) ){
			kfree(firm_buff);
			return retval;
		}
	}
	else
	{
		if( (retval = CW1100_UploadFirmware(priv,firm_buff,temp)) ){
			kfree(firm_buff);
			return retval;
		}
	}
	kfree(firm_buff);

#else
	if( HIF_8601_SILICON  == priv->hw_type )
	{
		if( (retval = CW1200_UploadFirmware(priv,FirmwareImage,FmLength)) )
			return retval;
	}
	else
	{
		if( (retval = CW1100_UploadFirmware(priv,FirmwareImage,FmLength)) )
			return retval;
	}
#endif
	/* If the device is STLC9000 the device is removed from RESET after firmware download */
	if( HIF_9000_SILICON_VERSTAILE  == priv->hw_type )
	{
		/* 7. Enable CLOCK and Remove device from RESET */
		if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,config_reg_len,SDIO_READ))  )
			return retval;
		config_reg_val = config_reg_val & ST90TDS_CONFIG_CPU_RESET_MASK;
		SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,config_reg_len,SDIO_WRITE);

		config_reg_val = config_reg_val & ST90TDS_CONFIG_CPU_CLK_DIS_MASK; /* Enable Clock */
		SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,config_reg_len,SDIO_WRITE);
		mdelay(100);
	}
#ifdef INTERRUPTS_SDIO
	printk("Allocating SDIO IRQ!\n");
	sdio_claim_host(priv->func);
	/* Register Interrupt Handler */
	if( (retval = sdio_claim_irq(priv->func, cw1200_sbus_interrupt)) )
	{
		DEBUG(DBG_ERROR,"%s, sdio_claim_irq() return error :[%d] \n",__FUNCTION__ ,retval );
		return UMI_STATUS_FAILURE;
	}
	/* Sleep after registering Interrupt Handler so that the IRQ thread can run */
	mdelay(1000);
	sdio_release_host(priv->func);
#elif defined INTERRUPTS_GPIO
	printk("Allocating GPIO IRQ!\n");
	if( (retval = request_any_context_irq(gpio_wlan_irq(), cw1200_sbus_interrupt,
				  IRQF_TRIGGER_RISING, "cw1200_gpio_irq", priv)) )
	{
		DEBUG(DBG_ERROR,"%s, request_irq() return error :[%d] \n",__FUNCTION__ ,retval );
		return UMI_STATUS_FAILURE;
	}

	/* Hack to access Fuction-0 */
	func_num = priv->func->num;
	priv->func->num = 0;
	sdio_claim_host(priv->func);
	cccr = sdio_readb(priv->func, SDIO_CCCR_IENx, &err_val);
	if (err_val) {
		sdio_release_host(priv->func);
		return UMI_STATUS_FAILURE;
	}

	/* We are function number 1 */
	cccr |= BIT(func_num);
	cccr |= BIT(0); /* Master interrupt enable */

	sdio_writeb(priv->func, cccr, SDIO_CCCR_IENx,&err_val);
	sdio_release_host(priv->func);
	/* Restore the WLAN function number */
	priv->func->num = func_num;

	if (err_val) {
		DEBUG(DBG_SBUS, "%s,F0 write failed\n", __func__);
		return UMI_STATUS_FAILURE;
	}
#endif



	/* If device is CW1200 the IRQ enable/disable bits are in CONFIG register */
	if( HIF_8601_SILICON  == priv->hw_type )
	{
		if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,config_reg_len,SDIO_READ))  )
			return retval;

		config_reg_val = config_reg_val | ST90TDS_CONF_IRQ_RDY_ENABLE;
		SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,config_reg_len,SDIO_WRITE);
	}
	else  /* If device is STLC9000 the IRQ enable/disable bits are in CONTROL register */
	{
		/* Enable device interrupts - Both DATA_RDY and WLAN_RDY */
		if( (retval = SBUS_SDIO_RW_Reg(priv,ST90TDS_CONTROL_REG_ID,(char *)&cont_reg_val,config_reg_len,SDIO_READ) ) )
			return retval;
		/* Enable SDIO Host Interrupt */
		/* Enable device interrupts - Both DATA_RDY and WLAN_RDY */
		cont_reg_val = cpu_to_le16( cont_reg_val | ST90TDS_CONT_IRQ_RDY_ENABLE ) ;
		SBUS_SDIO_RW_Reg(priv,ST90TDS_CONTROL_REG_ID,(char *)&cont_reg_val,config_reg_len,SDIO_WRITE);
	}

	/*Configure device for MESSSAGE MODE */
	config_reg_val = config_reg_val & ST90TDS_CONFIG_ACCESS_MODE_MASK;
	SBUS_SDIO_RW_Reg(priv,ST90TDS_CONFIG_REG_ID,(char *)&config_reg_val,config_reg_len,SDIO_WRITE);

	/* Set better block size for SDIO */
	printk("Setting SDIO block size to %i\n", SDIO_BLOCK_SIZE);
	sdio_claim_host(priv->func);
	retval = sdio_set_block_size(priv->func, SDIO_BLOCK_SIZE);
	sdio_release_host(priv->func);
	if (retval) {
		DEBUG(DBG_ERROR, "%s, sdio_set_block_size() return error :[%d]\n",
				__func__ , retval);
		return UMI_STATUS_FAILURE;
	}

	/*Configure device for MESSSAGE MODE */
	config_reg_val = config_reg_val & ST90TDS_CONFIG_ACCESS_MODE_MASK;
	SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val,
			config_reg_len, SDIO_WRITE);

	/*Unless we read the CONFIG Register we are not able to get an interrupt */
	mdelay(10);
	SBUS_SDIO_RW_Reg(priv, ST90TDS_CONFIG_REG_ID,
			(char *)&config_reg_val,
			config_reg_len, SDIO_READ);

#ifdef WORKAROUND
	irq_poll_init(priv);
#endif

	return retval;

}


/*! 	\fn  UMI_STATUS_CODE  UMI_CB_Stop ( LL_HANDLE  LowerHandle )
  \brief 	This callback function will stop the  lower layer.
  \param	LowerHandle	-	Handle to lower driver instance.
  \return 	- UMI Status Code
 */

UMI_STATUS_CODE  UMI_CB_Stop ( LL_HANDLE  	LowerHandle )
{
	struct CW1200_priv * priv = (struct CW1200_priv * )LowerHandle;

#ifdef WORKAROUND
	irq_poll_destroy(priv);
#endif
	flush_workqueue(priv->sbus_WQ);
	destroy_workqueue(priv->sbus_WQ);

	return UMI_STATUS_SUCCESS;
}


/*! 	\fn  UMI_STATUS_CODE  UMI_CB_Destroy ( LL_HANDLE	LowerHandle )
  \brief 	This callback function will destroy lower layer.
  \param	LowerHandle	-	Handle to lower driver instance.
  \return 	- UMI Status Code
 */
UMI_STATUS_CODE  UMI_CB_Destroy ( LL_HANDLE	LowerHandle )
{
	struct CW1200_priv * priv = (struct CW1200_priv * )LowerHandle;

	/* Free our large, reusable RX buffer */
	kfree(rx_buf);

#ifndef USE_SPI
	sdio_claim_host(priv->func);
#ifdef INTERRUPTS_SDIO
	sdio_release_irq(priv->func);
#elif defined INTERRUPTS_GPIO
	free_irq(gpio_wlan_irq(), priv);
#endif
	sdio_disable_func(priv->func);
	sdio_release_host(priv->func);
#else
	free_irq(priv->func->irq, priv);
#endif

	return UMI_STATUS_SUCCESS;
}
