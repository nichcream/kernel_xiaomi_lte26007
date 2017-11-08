/* driver/video/comip/mipi_cmd.c
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 1999-2015      LeadCoreTech Corp.
**
**      PURPOSE:                        This files contains the driver of LCD controller.
**
**      CHANGE HISTORY:
**
**      Version         Date            Author          Description
**      0.1.0           2012-03-10      liuyong         created
**
*/

#include "mipi_interface.h"
#include "mipi_cmd.h"
#include "comipfb.h"

/**
 * Retrieve the controller's status of whether command mode is ON or not
 * @param instance pointer to structure holding the DSI Host core information
 * @return whether command mode is ON
 */
static int mipi_dsih_hal_gen_is_cmd_mode(struct comipfb_info *fbi)
{
	return mipi_dsih_read_part(fbi, MIPI_CMD_MODE_CFG, 0, 1);
}
/**
 * Get the status of video mode, whether enabled or not in core
 * @param instance pointer to structure holding the DSI Host core information
 * @return status
 */
static int mipi_dsih_hal_dpi_is_video_mode(struct comipfb_info *fbi)
{
	return mipi_dsih_read_part(fbi, MIPI_VID_MODE_CFG, 0, 1);
}

/**
 * Get the FULL status of generic command fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static int mipi_dsih_hal_gen_cmd_fifo_full(struct comipfb_info *fbi)
{
	return mipi_dsih_read_part(fbi, MIPI_CMD_PKD_STATUS, 1, 1);
}

/**
 * Get the FULL status of generic write payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo full
 */
static int mipi_dsih_hal_gen_write_fifo_full(struct comipfb_info *fbi)
{
	return mipi_dsih_read_part(fbi, MIPI_CMD_PKD_STATUS, 3, 1);
}

/**
 * Write the payload of the long packet commands
 * @param instance pointer to structure holding the DSI Host core information
 * @param payload array of bytes of payload
 * @return error code
 */
static int mipi_dsih_hal_gen_packet_payload(struct comipfb_info *fbi, uint32_t payload)
{
	if (mipi_dsih_hal_gen_write_fifo_full(fbi)) {
		return -EINVAL;
	}
	mipi_dsih_write_word(fbi, MIPI_GEN_PLD_DATA, payload);

	return 0;
}

/**
 * Get status of read command
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if busy
 */
static int mipi_dsih_hal_gen_rd_cmd_busy(struct comipfb_info *fbi)
{
	return mipi_dsih_read_part(fbi, MIPI_CMD_PKD_STATUS, 6, 1);
}

/**
 * Get the EMPTY status of generic read payload fifo
 * @param instance pointer to structure holding the DSI Host core information
 * @return 1 if fifo empty
 */
static int mipi_dsih_hal_gen_read_fifo_empty(struct comipfb_info *fbi)
{
	return mipi_dsih_read_part(fbi, MIPI_CMD_PKD_STATUS, 4, 1);
}

/**
 * Write the payload of the long packet commands
 * @param instance pointer to structure holding the DSI Host core information
 * @param payload pointer to 32-bit array to hold read information
 * @return error code
 */
static int mipi_dsih_hal_gen_read_payload(struct comipfb_info *fbi, uint32_t* payload)
{
	*payload = mipi_dsih_read_word(fbi, MIPI_GEN_PLD_DATA);

	return 0;
}

/**
 * Write command header in the generic interface
 * (which also sends DCS commands) as a subset
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc of destination
 * @param packet_type (or type of DCS command)
 * @param ls_byte (if DCS, it is the DCS command)
 * @param ms_byte (only parameter of short DCS packet)
 * @return error code
 */
static int mipi_dsih_hal_gen_packet_header(struct comipfb_info *fbi, uint8_t vc, uint8_t packet_type, uint8_t ms_byte, uint8_t ls_byte)
{
	if (vc < 4) {
		mipi_dsih_write_part(fbi, MIPI_GEN_HDR, (ms_byte <<  16) | (ls_byte << 8 ) 
					| ((vc << 6) | packet_type), 0, 24);
		return 0;
	}

	return  -EINVAL;
}


/**
 * Send READ packet to peripheral using the generic interface
 * This will force command mode and stop video mode (because of BTA)
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type generic command type
 * @param lsb_byte first command parameter
 * @param msb_byte second command parameter
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
static uint16_t mipi_dsih_gen_rd_packet(struct comipfb_info *fbi, uint8_t vc, uint8_t data_type, uint8_t msb_byte, uint8_t lsb_byte, uint8_t bytes_to_read, uint8_t* read_buffer)
{
	int err_code = 0;
	int timeout = 0;
	int counter = 0;
	int i = 0;
	int last_count = 0;
	uint32_t temp[1] = {0};
	
	if ((bytes_to_read < 1) || (read_buffer == 0))
		return 0;

	/* make sure receiving is enabled */
	mipi_dsih_hal_bta_en(fbi, 1);
	/* listen to the same virtual channel as the one sent to */
	mipi_dsih_hal_gen_rd_vc(fbi, vc);
	for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++) {	
		/* check if payload Tx fifo is not full */
		if (!mipi_dsih_hal_gen_cmd_fifo_full(fbi)) {
			mipi_dsih_hal_gen_packet_header(fbi, vc, data_type, msb_byte, lsb_byte);
			break;
		}
		msleep(2);
	}
	if (!(timeout < DSIH_FIFO_ACTIVE_WAIT)) {
		printk("mipi read, tx timeout");
		return 0;
	}
	/* loop for the number of words to be read */
	for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++) {
		/* check if command transaction is done */
		if (!mipi_dsih_hal_gen_rd_cmd_busy(fbi)) {
			if (!mipi_dsih_hal_gen_read_fifo_empty(fbi)) {
				for (counter = 0; (!mipi_dsih_hal_gen_read_fifo_empty(fbi)); counter += 4) {
						err_code = mipi_dsih_hal_gen_read_payload(fbi, temp);
						if (err_code)
							return 0;
					if (counter < bytes_to_read) {
						for (i = 0; i < 4; i++) {
							if ((counter + i) < bytes_to_read) {
								/* put 32 bit temp in 4 bytes of buffer passed by user*/
								read_buffer[counter + i] = (uint8_t)(temp[0] >> (i * 8));
								last_count = i + counter;
							}else {
								if ((uint8_t)(temp[0] >> (i * 8)) != 0x00) {
									last_count = i + counter;
								}
							}
						}
					}else {
						last_count = counter;
						for (i = 0; i < 4; i++) {
							if ((uint8_t)(temp[0] >> (i * 8)) != 0x00) {
								last_count = i + counter;
							}
						}
					}
				}
				return last_count + 1;
			}else {
				printk("rx buffer empty\n");
				return 0;
			}
		}
		msleep(2);
	}
	printk("rx command timed out\n");

	return 0;
}

/**
 * Send a DCS READ command to peripheral
 * function sets the packet data type automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param command DCS code
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
uint16_t mipi_dsih_dcs_rd_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t command, uint8_t bytes_to_read, uint8_t* read_buffer)
{
	switch (command) {
		case 0xA8:
		case 0xA1:
		case 0x45:
		case 0x3E:
		case 0x2E:
		case 0x0F:
		case 0x0E:
		case 0x0D:
		case 0x0C:
		case 0x0B:
		case 0x0A:
		case 0x08:
		case 0x07:
		case 0x06:
			/* COMMAND_TYPE 0x06 - DCS Read no params refer to DSI spec p.47 */
			return mipi_dsih_gen_rd_packet(fbi, vc, 0x06, 0x0, command, bytes_to_read, read_buffer);
		default:
			printk("invalid DCS command\n");
		return 0;
	}

	return 0;
}

/**
 * Send Generic READ command to peripheral
 * function sets the packet data type automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param params byte array of command parameters
 * @param param_length length of the above array
 * @param bytes_to_read no of bytes to read (expected to arrive at buffer)
 * @param read_buffer pointer to 8-bit array to hold the read buffer words
 * return read_buffer_length
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to 2 x DSIH_FIFO_ACTIVE_WAIT
 * (waiting for command buffer, and waiting for receiving)
 * @note this function will enable BTA
 */
uint16_t mipi_dsih_gen_rd_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t* params, uint16_t param_length, uint8_t bytes_to_read, uint8_t* read_buffer)
{
	uint8_t data_type = 0;

	switch(param_length) {
		case 0:
			data_type = 0x04;
			return mipi_dsih_gen_rd_packet(fbi, vc, data_type, 0x00, 0x00, bytes_to_read, read_buffer);
		case 1:
			data_type = 0x14;
			return mipi_dsih_gen_rd_packet(fbi, vc, data_type, 0x00, params[0], bytes_to_read, read_buffer);
		case 2:
			data_type = 0x24;
			return mipi_dsih_gen_rd_packet(fbi, vc, data_type, params[1], params[0], bytes_to_read, read_buffer);
		default:
			return 0;
	}
}

/**
 * Send a packet on the generic interface
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param data_type type of command, inserted in first byte of header
 * @param params byte array of command parameters
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to:
 * (param_length / 4) x DSIH_FIFO_ACTIVE_WAIT x register access time
 * @note the controller restricts the sending of .
 * This function will not be able to send Null and Blanking packets due to
 *  controller restriction
 */

//*****params:for long packets params[0], params[1] is the lenghts of packet (include DCS); for short packet params[0], params[1] are packet param.
static int mipi_dsih_gen_wr_packet(struct comipfb_info *fbi, uint8_t vc, uint8_t data_type, uint8_t* params, uint16_t param_length)
{
	int err_code = 0;
	/* active delay iterator */
	int timeout = 0;
	/* iterators */
	int i = 0;
	int j = 0;
	/* holds padding bytes needed */
	int compliment_counter = 0;
	uint8_t* payload = 0;
	/* temporary variable to arrange bytes into words */
	uint32_t temp = 0;
	uint16_t word_count = 0;

	if ((params == 0) && (param_length != 0)) {
		 /* pointer NULL */
		return -EINVAL;
	}
	if (param_length > 2) {	
		/* long packet - write word count to header, and the rest to payload */
		payload = params + (2 * sizeof(params[0]));
		word_count = (params[1] << 8) | params[0];  
		if ((param_length - 2) < word_count) {
			compliment_counter = (param_length - 2) - word_count;
			printk("sent > input payload. complemented with zeroes\n");			
		}else if ((param_length - 2) > word_count)
			printk("Overflow - input > sent. payload truncated\n");

		for (i = 0; i < (param_length - 2); i += j) {
			temp = 0;
			for (j = 0; (j < 4) && ((j + i) < (param_length - 2)); j++) {	//composer  a 32 bit with 8 bit
				/* temp = (payload[i + 3] << 24) | (payload[i + 2] << 16) | (payload[i + 1] << 8) | payload[i];  */
				temp |= payload[i + j] << (j * 8);
			}
			/* check if payload Tx fifo is not full */
			for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++) {
				if (!mipi_dsih_hal_gen_packet_payload(fbi, temp)) {
					break;
				}
				mdelay(2);
			}
			if (!(timeout < DSIH_FIFO_ACTIVE_WAIT)) {
				return -EINVAL;
			}
		}
		/* if word count entered by the user more than actual parameters received
		 * fill with zeroes - a fail safe mechanism, otherwise controller will
		 * want to send data from an empty buffer */
		for (i = 0; i < compliment_counter; i++) {
			/* check if payload Tx fifo is not full */
			for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++) {
				if (!mipi_dsih_hal_gen_packet_payload(fbi, 0x00)) {
					break;
				}
				mdelay(2);
			}
			if (!(timeout < DSIH_FIFO_ACTIVE_WAIT)) {
				return -EINVAL;
			}
		}
	}
	for (timeout = 0; timeout < DSIH_FIFO_ACTIVE_WAIT; timeout++) {
		/* check if payload Tx fifo is not full */
		if (!mipi_dsih_hal_gen_cmd_fifo_full(fbi)) {
			if (param_length == 0) {
				err_code |= mipi_dsih_hal_gen_packet_header(fbi, vc, data_type, 0x0, 0x0);
			}else if (param_length == 1) {
				err_code |= mipi_dsih_hal_gen_packet_header(fbi, vc, data_type, 0x0, params[0]);
			}else {
				err_code |= mipi_dsih_hal_gen_packet_header(fbi, vc, data_type, params[1], params[0]);
			}
			break;
		}
		mdelay(2);
	}
	if (!(timeout < DSIH_FIFO_ACTIVE_WAIT)) {
		err_code = -EINVAL;
	}

	return err_code;
}

/**
 * Send a generic write command
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param params byte-addressed array of command parameters
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to DSIH_FIFO_ACTIVE_WAIT x register access time
 */
int mipi_dsih_gen_wr_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t* params, uint16_t param_length)
{
	uint8_t data_type = 0;
	int tran_mode;

	if (params == 0) {
		return -EINVAL;
	}
	switch(params[0]) {
		case SW_PACK0:
			data_type = 0x03; //generic write short pack 0
			tran_mode = GEN_SW_0P_TX;
		break;
		case SW_PACK1:
			data_type = 0x13; //generic write short pack 1
			tran_mode = GEN_SW_1P_TX;
		break;
		case SW_PACK2:
			data_type = 0x23; //generic write short pack 2
			tran_mode = GEN_SW_2P_TX;
		break;
		case LW_PACK:
			data_type = 0x29; //generic write long pack
			tran_mode = GEN_LW_TX;
		break;
		default:
			printk(KERN_ERR "*NOT GEN COMMAND*\n");
		return -EINVAL;
	}
	mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, 1, tran_mode, 1);
	return mipi_dsih_gen_wr_packet(fbi, vc, data_type, (params + 2), param_length);
}

/**
 * Send a DCS write command
 * function sets the packet data type automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc destination virtual channel
 * @param params byte-addressed array of command parameters, including the
 * command itself
 * @param param_length length of the above array
 * @return error code
 * @note this function has an active delay to wait for the buffer to clear.
 * The delay is limited to DSIH_FIFO_ACTIVE_WAIT x register access time
 */
int mipi_dsih_dcs_wr_cmd(struct comipfb_info *fbi, uint8_t vc, uint8_t* params, uint16_t param_length)
{
	uint8_t packet_type = 0;
	int tran_mode;

	if (params == 0) {
		return -EINVAL;
	}
	switch (params[0]) {
		case SW_PACK1:
			packet_type = 0x05; /* DCS short write no param */
			tran_mode = DCS_SW_0P_TX;
		break;
		case SW_PACK2:
			packet_type = 0x15; /* DCS short write 1 param */
			tran_mode = DCS_SW_1P_TX;
		break;
		case LW_PACK:
			packet_type = 0x39; /* DCS long write/write_LUT command packet */
			tran_mode = DCS_LW_TX;
		break;
		default:
			printk(KERN_ERR "NOT DCS COMMAND\n");
		return -EINVAL;
	}
	mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, 1, tran_mode, 1);
	return mipi_dsih_gen_wr_packet(fbi, vc, packet_type, (params + 2), param_length);
}

/**
 * Enable command mode
 * This function shall be explicitly called before commands are send if they
 * are to be sent in command mode and not interlaced with video
 * If video mode is ON, it will be turned off automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param en enable/disable
 */
void mipi_dsih_cmd_mode(struct comipfb_info *fbi, int en)
{
	if ((!mipi_dsih_hal_gen_is_cmd_mode(fbi)) && en) {
		/* disable video mode first */
		mipi_dsih_hal_dpi_video_mode_en(fbi, 0);
		mipi_dsih_hal_gen_cmd_mode_en(fbi, 1);
	}else if ((mipi_dsih_hal_gen_is_cmd_mode(fbi)) && !en)
		mipi_dsih_hal_gen_cmd_mode_en(fbi, 0);
}			

/**
 * Enable video mode
 * If command mode is ON, it will be turned off automatically
 * @param instance pointer to structure holding the DSI Host core information
 * @param en enable/disable
 */
void mipi_dsih_video_mode(struct comipfb_info *fbi, int en)
{
	if ((!mipi_dsih_hal_dpi_is_video_mode(fbi)) && en) {
		/* disable cmd mode first */
		mipi_dsih_hal_gen_cmd_mode_en(fbi, 0);
		mipi_dsih_hal_dpi_video_mode_en(fbi, 1);
	}else if ((!mipi_dsih_hal_dpi_is_video_mode(fbi)) && !en)
		mipi_dsih_hal_dpi_video_mode_en(fbi, 0);
}

EXPORT_SYMBOL(mipi_dsih_cmd_mode);
EXPORT_SYMBOL(mipi_dsih_video_mode);
EXPORT_SYMBOL(mipi_dsih_gen_wr_cmd);
EXPORT_SYMBOL(mipi_dsih_dcs_wr_cmd);
EXPORT_SYMBOL(mipi_dsih_dcs_rd_cmd);
EXPORT_SYMBOL(mipi_dsih_gen_rd_cmd);

