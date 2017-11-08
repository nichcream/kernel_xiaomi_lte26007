/* driver/video/comip/mipi_interface.c
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

static struct {
	uint32_t freq;		//upper margin of frequency range
	uint8_t hs_freq;	//hsfreqrange
	uint8_t vco_range;	//vcorange
}ranges[] = {
	{90, 0x00, 0x01}, {100, 0x10, 0x01}, {110, 0x20, 0x01},
	{125, 0x01, 0x01}, {140, 0x11, 0x01}, {150, 0x21, 0x01},
	{160, 0x02, 0x01}, {180, 0x12, 0x03}, {200, 0x22, 0x03},
	{210, 0x03, 0x03}, {240, 0x13, 0x03}, {250, 0x23, 0x03},
	{270, 0x04, 0x07}, {300, 0x14, 0x07}, {330, 0x24, 0x07},
	{360, 0x15, 0x07}, {400, 0x25, 0x07}, {450, 0x06, 0x07},
	{500, 0x16, 0x07}, {550, 0x07, 0x0f}, {600, 0x17, 0x0f},
	{650, 0x08, 0x0f}, {700, 0x18, 0x0f}, {750, 0x09, 0x0f},
	{800, 0x19, 0x0f}, {850, 0x0A, 0x0f}, {900, 0x1A, 0x0f},
	{950, 0x2A, 0x0f}, {1000, 0x3A, 0x0f},
};

static struct {
	uint32_t loop_div;	// upper limit of loop divider range
	uint8_t cp_current;	// icpctrl
	uint8_t lpf_resistor;	// lpfctrl
}loop_bandwidth[] = {
	{32, 0x06, 0x10}, {64, 0x06, 0x10}, {128, 0x0C, 0x08},
	{256, 0x04, 0x04}, {512, 0x00, 0x01}, {768, 0x01, 0x01},
	{1000, 0x02, 0x01},
};

static void mipi_dsih_dphy_write(struct comipfb_info *fbi, uint8_t address, uint8_t *data, uint8_t data_length);

/************************DPI*******************************************/
/**
 * Write a 32-bit word to the DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param reg_address register offset in core
 * @param data 32-bit word to be written to register
 */
void mipi_dsih_write_word(struct comipfb_info *fbi, uint32_t reg_address, uint32_t data)
{
	writel(data, io_p2v(fbi->interface_res->start + reg_address));
}
/**
 * Write a 32-bit word to the DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param reg_address offset of register
 * @return 32-bit word value stored in register
 */
uint32_t mipi_dsih_read_word(struct comipfb_info *fbi, uint32_t reg_address)
{
	return readl(io_p2v(fbi->interface_res->start + reg_address));
}

/**
 * Write a 32-bit word to the DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param reg_address offset of register in core
 * @param shift bit shift from the left (system is BIG ENDIAN)
 * @param width of bit field
 * @return bit field read from register
 */
uint32_t mipi_dsih_read_part(struct comipfb_info *fbi, uint32_t reg_address, uint8_t shift, uint8_t width)
{
	return (mipi_dsih_read_word(fbi, reg_address) >> shift) & ((1 << width) - 1);
}

/**
 * Write a bit field o a 32-bit word to the DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param reg_address register offset in core
 * @param data to be written to register
 * @param shift bit shift from the left (system is BIG ENDIAN)
 * @param width of bit field
 */
void mipi_dsih_write_part(struct comipfb_info *fbi, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width)
{
	unsigned int mask = (1 << width) - 1;
	unsigned int temp = mipi_dsih_read_word(fbi, reg_address);

	temp &= ~(mask << shift);
	temp |= (data & mask) << shift;
	mipi_dsih_write_word(fbi, reg_address, temp);
}
/**
 * Modify power status of DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param on (1) or off (0)
 */
void mipi_dsih_hal_power(struct comipfb_info *fbi, int on)
{
	mipi_dsih_write_part(fbi, MIPI_PWR_UP, on, 0, 1);
}
/**
 * Write transmission escape timeout
 * a safe guard so that the state machine would reset if transmission
 * takes too long
 * @param instance pointer to structure holding the DSI Host core information
 * @param tx_escape_division
 */
static void mipi_dsih_hal_tx_escape_division(struct comipfb_info *fbi, uint8_t tx_escape_division)
{
	mipi_dsih_write_part(fbi, MIPI_CLKMGR_CFG, tx_escape_division, 0, 8);
}
/**
 * Write the DPI video virtual channel destination
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc virtual channel
 */
static void mipi_dsih_hal_dpi_video_vc(struct comipfb_info *fbi, uint8_t vc)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG, (uint32_t)(vc), 0, 2);
}
/**
 * Set DPI video color coding
 * @param instance pointer to structure holding the DSI Host core information
 * @param color_coding enum (configuration and color depth)
 * @return error code
 */
static int mipi_dsih_hal_dpi_color_coding(struct comipfb_info *fbi, dsih_color_coding_t color_coding)
{
	int err = 0;

	if (color_coding > 7) {
		err = -EINVAL;
	}
	else {
		mipi_dsih_write_part(fbi, MIPI_DPI_CFG, color_coding, 2, 3);
	}
	return err;
}
/**
 * Set DPI loosely packetisation video (used only when color depth = 18
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_dpi_18_loosely_packet_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG, enable, 10, 1);
}
/**
 * Set DPI color mode pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_color_mode_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG, active_low, 9, 1);
}
/**
 * Set DPI shut down pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_shut_down_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG, active_low, 8, 1);
}
/**
 * Set DPI horizontal sync pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_hsync_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG, active_low, 7, 1);
}
/**
 * Set DPI vertical sync pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_vsync_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG, active_low, 6, 1);
}
/**
 * Set DPI data enable pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_dataen_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG, active_low, 5, 1);
}
/**
 * Enable FRAME BTA ACK
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_frame_ack_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 11, 1);
}
/**
 * Enable null packets (value in null packet size will be taken in calculations)
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_null_packet_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 10, 1);
}
/**
 * Enable multi packets (value in no of chunks will be taken in calculations)
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_multi_packet_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 9, 1);
}
/**
 * Enable return to low power mode inside horizontal front porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_hfp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 8, 1);
}
/**
 * Enable return to low power mode inside horizontal back porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_hbp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 7, 1);
}
/**
 * Enable return to low power mode inside vertical active lines periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vactive(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 6, 1);
}
/**
 * Enable return to low power mode inside vertical front porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vfp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 5, 1);
}
/**
 * Enable return to low power mode inside vertical back porch periods when
 * timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vbp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 4, 1);
}
/**
 * Enable return to low power mode inside vertical sync periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vsync(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 3, 1);
}
/**
 * Set DPI video mode type (burst/non-burst - with sync pulses or events)
 * @param instance pointer to structure holding the DSI Host core information
 * @param type
 * @return error code
 */
static int mipi_dsih_hal_dpi_video_mode_type(struct comipfb_info *fbi, dsih_video_mode_t type)
{
	if (type < 3) {
		mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, type, 1, 2);
		return 0;
	}else {
		printk("in mipi_dsih_hal_dpi_video_mode_type\n");
		return -EINVAL;
	}
}
/**
 * Enable/disable DPI video mode
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
void mipi_dsih_hal_dpi_video_mode_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, 0, 1);
}
/**
 * Write the null packet size - will only be taken into account when null
 * packets are enabled.
 * @param instance pointer to structure holding the DSI Host core information
 * @param size of null packet
 * @return error code
 */
static int mipi_dsih_hal_dpi_null_packet_size(struct comipfb_info *fbi, uint16_t size)
{
	if (size < 0x3ff) {
		/* 10-bit field */
		mipi_dsih_write_part(fbi, MIPI_VID_PKT_CFG, size, 21, 10);
		return 0;
	}else {
		return -EINVAL;
	}
}
/**
 * Write no of chunks to core - taken into consideration only when multi packet
 * is enabled
 * @param instance pointer to structure holding the DSI Host core information
 * @param no of chunks
 */
static int mipi_dsih_hal_dpi_chunks_no(struct comipfb_info *fbi, uint16_t no)
{
	if (no < 0x3ff) {
		mipi_dsih_write_part(fbi, MIPI_VID_PKT_CFG, no, 11, 10);
		return 0;
	}else {
		return -EINVAL;
	}
}
/**
 * Write video packet size. obligatory for sending video
 * @param instance pointer to structure holding the DSI Host core information
 * @param size of video packet - containing information
 * @return error code
 */
static int mipi_dsih_hal_dpi_video_packet_size(struct comipfb_info *fbi, uint16_t size)
{
	if (size < 0x7ff) {
		/* 11-bit field */
		mipi_dsih_write_part(fbi, MIPI_VID_PKT_CFG, size, 0, 11);
		return 0;
	}else {
		return -EINVAL;
	}
}
/**
 * Set DCS command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static int mipi_dsih_hal_dcs_wr_tx_type(struct comipfb_info *fbi, unsigned no_of_param, int lp)
{
	switch (no_of_param) {
		case 0:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 7, 1);
		break;
		case 1:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 8, 1);
		break;
		default:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 12, 1);
		break;
	}
	return 0;
}
/**
 * Set DCS read command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static int mipi_dsih_hal_dcs_rd_tx_type(struct comipfb_info *fbi, unsigned no_of_param, int lp)
{
	int err = 0;
	switch (no_of_param) {
		case 0:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 9, 1);
		break;
		default:
			err = -EINVAL;
		break;
	}
	return err;
}
/**
 * Set generic write command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static int mipi_dsih_hal_gen_wr_tx_type(struct comipfb_info *fbi, unsigned no_of_param, int lp)
{
	switch (no_of_param) {
		case 0:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 1, 1);
		break;
		case 1:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 2, 1);
		break;
		case 2:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 3, 1);
		break;
		default:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 11, 1);
		break;
	}
	return 0;
}
/**
 * Set generic command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
static int mipi_dsih_hal_gen_rd_tx_type(struct comipfb_info *fbi, unsigned no_of_param, int lp)
{
	int err = 0;
	switch (no_of_param) {
		case 0:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 4, 1);
		break;
		case 1:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 5, 1);
		break;
		case 2:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, 6, 1);
		break;
		default:
			err = -EINVAL;
		break;
	}
	return err;
}
/**
 * Enable command mode (Generic interface)
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
void mipi_dsih_hal_gen_cmd_mode_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, enable, 0, 1);
}

void mipi_dsih_dphy_enter_ulps(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CTRL, enable, 3, 2);
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CTRL, enable, 1, 2);
}

void mipi_dsih_dphy_exit_ulps(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CTRL, enable, 3, 2);
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CTRL, enable, 1, 2);
}

/**
 * Configure the Horizontal Line time
 * @param instance pointer to structure holding the DSI Host core information
 * @param time taken to transmit the total of the horizontal line
 */
static void mipi_dsih_hal_dpi_hline(struct comipfb_info *fbi, uint16_t time)
{
	mipi_dsih_write_part(fbi, MIPI_TMR_LINE_CFG, time, 18, 14);
}
/**
 * Configure the Horizontal back porch time
 * @param instance pointer to structure holding the DSI Host core information
 * @param time taken to transmit the horizontal back porch
 */
static void mipi_dsih_hal_dpi_hbp(struct comipfb_info *fbi, uint16_t time)
{
	mipi_dsih_write_part(fbi, MIPI_TMR_LINE_CFG, time, 9, 9);
}
/**
 * Configure the Horizontal sync time
 * @param instance pointer to structure holding the DSI Host core information
 * @param time taken to transmit the horizontal sync
 */
static void mipi_dsih_hal_dpi_hsa(struct comipfb_info *fbi, uint16_t time)
{
	mipi_dsih_write_part(fbi, MIPI_TMR_LINE_CFG, time, 0, 9);
}
/**
 * Configure the vertical active lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vactive(struct comipfb_info *fbi, uint16_t lines)
{
	mipi_dsih_write_part(fbi, MIPI_VTIMING_CFG, lines, 16, 11);
}
/**
 * Configure the vertical front porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vfp(struct comipfb_info *fbi, uint16_t lines)
{
	mipi_dsih_write_part(fbi, MIPI_VTIMING_CFG, lines, 10, 6);
}
/**
 * Configure the vertical back porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vbp(struct comipfb_info *fbi, uint16_t lines)
{
	mipi_dsih_write_part(fbi, MIPI_VTIMING_CFG, lines, 4, 6);
}
/**
 * Configure the vertical sync lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vsync(struct comipfb_info *fbi, uint16_t lines)
{
	mipi_dsih_write_part(fbi, MIPI_VTIMING_CFG, lines, 0, 4);
}
/**
 * configure timeout divisions (so they would have more clock ticks)
 * @param instance pointer to structure holding the DSI Host core information
 * @param byte_clk_division_factor no of hs cycles before transiting back to LP in
 *  (lane_clk / byte_clk_division_factor)
 */
static void mipi_dsih_hal_timeout_clock_division(struct comipfb_info *fbi, uint8_t byte_clk_division_factor)
{
	mipi_dsih_write_part(fbi, MIPI_CLKMGR_CFG, byte_clk_division_factor, 8, 8);
}
/**
 * Configure the Low power receive time out
 * @param instance pointer to structure holding the DSI Host core information
 * @param count (of byte cycles)
 */
static void mipi_dsih_hal_lp_rx_timeout(struct comipfb_info *fbi, uint16_t count)
{
	mipi_dsih_write_part(fbi, MIPI_TO_CNT_CFG, count, 16, 16);
}
/**
 * Configure a high speed transmission time out7
 * @param instance pointer to structure holding the DSI Host core information
 * @param count (byte cycles)
 */
static void mipi_dsih_hal_hs_tx_timeout(struct comipfb_info *fbi, uint16_t count)
{
	mipi_dsih_write_part(fbi, MIPI_TO_CNT_CFG, count, 0, 16);
}
/**
 * Configure the read back virtual channel for the generic interface
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc to listen to on the line
 */
void mipi_dsih_hal_gen_rd_vc(struct comipfb_info *fbi, uint8_t vc)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, vc, 5, 2);
}
/**
 * Enable EOTp reception
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_eotp_rx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, 1, 1);
}
/**
 * Enable EOTp transmission
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_eotp_tx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, 0, 1);
}
/**
 * Enable Bus Turn-around request
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
void mipi_dsih_hal_bta_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, 2, 1);
}
/**
 * Enable ECC reception, error correction and reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_ecc_rx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, 3, 1);
}
/**
 * Enable CRC reception, error reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_crc_rx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, 4, 1);
}

/**
 * Configure MASK (hiding) of interrupts coming from error 0 source
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask to be written to the register
 */
static void mipi_dsih_hal_error_mask_0(struct comipfb_info *fbi, uint32_t mask)
{
	mipi_dsih_write_word(fbi, MIPI_ERROR_MSK0, mask);
}
/**
 * Configure MASK (hiding) of interrupts coming from error 0 source
 * @param instance pointer to structure holding the DSI Host core information
 * @param mask the mask to be written to the register
 */
static void mipi_dsih_hal_error_mask_1(struct comipfb_info *fbi, uint32_t mask)
{
	mipi_dsih_write_word(fbi, MIPI_ERROR_MSK1, mask);
}

/*************************************** D-PHY****************************** */
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch from high speed to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles
 * @return error code
 */
static void mipi_dsih_phy_hs2lp_config(struct comipfb_info *fbi, uint8_t no_of_byte_cycles)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_TMR_CFG, no_of_byte_cycles, 20, 8);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch from to low power high speed
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles
 * @return error code
 */
static void mipi_dsih_phy_lp2hs_config(struct comipfb_info *fbi, uint8_t no_of_byte_cycles)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_TMR_CFG, no_of_byte_cycles, 12, 8);
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to turn the bus around to start receiving
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles
 * @return error code
 */
static int mipi_dsih_phy_bta_time(struct comipfb_info *fbi, uint16_t no_of_byte_cycles)
{
	if (no_of_byte_cycles < 0x8000) {
		/* 12-bit field */
		mipi_dsih_write_part(fbi, MIPI_PHY_TMR_CFG, no_of_byte_cycles, 0, 12);
	}else {
		return -EINVAL;
	}
	return 0;
}

/**
 * Reset D-PHY module
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param reset
 */
void mipi_dsih_dphy_reset(struct comipfb_info *fbi, int reset)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_RSTZ, reset, 1, 1);
}
/**
 * Power up/down D-PHY module
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param powerup (1) shutdown (0)
 */
void mipi_dsih_dphy_shutdown(struct comipfb_info *fbi, int powerup)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_RSTZ, powerup, 0, 1);
}
/**
 * Configure minimum wait period for HS transmission request after a stop state
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param no_of_byte_cycles [in byte (lane) clock cycles]
 */
static void mipi_dsih_dphy_stop_wait_time(struct comipfb_info *fbi, uint8_t no_of_byte_cycles)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CFG, no_of_byte_cycles, 2, 8);
}
/**
 * Set number of active lanes
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param no_of_lanes
 */
static void mipi_dsih_dphy_no_of_lanes(struct comipfb_info *fbi, uint8_t no_of_lanes)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CFG, no_of_lanes - 1, 0, 2);
}
/**
 * Enable clock lane module
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param en
 */
static void mipi_dsih_dphy_clock_en(struct comipfb_info *fbi, int en)
{
        mipi_dsih_write_part(fbi, MIPI_PHY_RSTZ, en, 2, 1);
}

/*
 * reset dphy pll
 */
static void mipi_dsih_dphy_pll_reset(struct comipfb_info *fbi)
{
	unsigned char data[4];

	data[0] = 0x40;
	mipi_dsih_dphy_write(fbi, 0x13, data, 1);
	udelay(10);

	data[0] = 0x70;
	mipi_dsih_dphy_write(fbi, 0x13, data, 1);
	udelay(10);
}

/**
 * Request the PHY module to start transmission of high speed clock.
 * This causes the clock lane to start transmitting DDR clock on the
 * lane interconnect.
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param enable
 * @note this function should be called explicitly by user always except for
 * transmitting
 */
void mipi_dsih_dphy_enable_hs_clk(struct comipfb_info *fbi, int enable)
{
	/* Reset dphy pll between low power mode and high speed mode to
		avoid dphy bug. */
	if (enable)
		mipi_dsih_dphy_pll_reset(fbi);

	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CTRL, enable, 0, 1);
}
/**
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void mipi_dsih_dphy_test_clock(struct comipfb_info *fbi, int value)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_TEST_CTRL0, value, 1, 1);
}
/**
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void mipi_dsih_dphy_test_clear(struct comipfb_info *fbi, int value)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_TEST_CTRL0, value, 0, 1);
}
/**
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param on_falling_edge
 */
static void mipi_dsih_dphy_test_en(struct comipfb_info *fbi, uint8_t on_falling_edge)
{
	mipi_dsih_write_part(fbi,  MIPI_PHY_TEST_CTRL1, on_falling_edge, 16, 1);
}

/**
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param test_data
 */
static void mipi_dsih_dphy_test_data_in(struct comipfb_info *fbi, uint8_t test_data)
{
	mipi_dsih_write_word(fbi, MIPI_PHY_TEST_CTRL1, test_data);
}
/**
 * Write to D-PHY module (encapsulating the digital interface)
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param address offset inside the D-PHY digital interface
 * @param data array of bytes to be written to D-PHY
 * @param data_length of the data array
 */
static void mipi_dsih_dphy_write(struct comipfb_info *fbi, uint8_t address, uint8_t *data, uint8_t data_length)
{
	unsigned i = 0;
	if (data != 0) {
		/* set the TESTCLK input high in preparation to latch in the desired test mode */
		mipi_dsih_dphy_test_clock(fbi, 0);
		/* set the desired test code in the input 8-bit bus TESTDIN[7:0] */
		mipi_dsih_dphy_test_data_in(fbi, address);
		/* set TESTEN input high  */
		mipi_dsih_dphy_test_en(fbi, 1);
		/* drive the TESTCLK input low; the falling edge captures the chosen test code into the transceiver */
		mipi_dsih_dphy_test_clock(fbi, 1);
		mipi_dsih_dphy_test_clock(fbi, 0);
		/* set TESTEN input low to disable further test mode code latching  */
		mipi_dsih_dphy_test_en(fbi, 0);
		/* start writing MSB first */
		for (i = data_length; i > 0; i--) {	
			/* set TESTDIN[7:0] to the desired test data appropriate to the chosen test mode */
			mipi_dsih_dphy_test_data_in(fbi, data[i - 1]);
			/* pulse TESTCLK high to capture this test data into the macrocell; repeat these two steps as necessary */
			mipi_dsih_dphy_test_clock(fbi, 1);
			mipi_dsih_dphy_test_clock(fbi, 0);
		}
	}
}

/**************************PHY***INIT************************************/
/**
 * Configure D-PHY and PLL module to desired operation mode
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param no_of_lanes active
 * @param output_freq desired high speed frequency
 * @return error code
 */
int mipi_dsih_dphy_configure(struct comipfb_info *fbi)
{
	unsigned int loop_divider = 0;		// (M)
	unsigned int input_divider = 1;		// (N)
	unsigned char data[4];			// maximum data for now are 4 bytes per test mode
	unsigned char no_of_bytes = 0;
	unsigned char i = 0;			//iterator
	unsigned char range = 0;		//ranges iterator
	int flag = 0;
	unsigned int output_freq;		//data lane out freq (bytes)
	struct comipfb_dev *cdev;
	struct comipfb_dev_timing_mipi mipi;
	
	cdev = fbi->cdev;
	mipi = cdev->timing.mipi;
	output_freq = mipi.output_freq * 8;
	if (output_freq < MIN_OUTPUT_FREQ) {
		return -EINVAL;
	}
	// find M and N dividers;  here the >= DPHY_DIV_LOWER_LIMIT is a phy constraint, formula should be above 1 MHz
	for (input_divider = (1 + (mipi.reference_freq / DPHY_DIV_UPPER_LIMIT));
		((mipi.reference_freq / input_divider) >= DPHY_DIV_LOWER_LIMIT) && (!flag); input_divider++) {
		if (((output_freq * input_divider) % (mipi.reference_freq )) == 0) {
			loop_divider = ((output_freq * input_divider) / (mipi.reference_freq));	//values found
			if (loop_divider >= 12) {
				flag = 1;
			}
		}
	}
	if ((!flag) || ((mipi.reference_freq / input_divider) < DPHY_DIV_LOWER_LIMIT)) {	//value not found
		loop_divider = output_freq / DPHY_DIV_LOWER_LIMIT;
		input_divider = mipi.reference_freq / DPHY_DIV_LOWER_LIMIT;
	}
	else {
		input_divider--;
	}
	for (i = 0; (i < (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) && (loop_divider > loop_bandwidth[i].loop_div); i++);

	if (i >= (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) {
		return -EINVAL;
	}
	/* get the PHY in power down mode (shutdownz=0) and reset it (rstz=0) to
	avoid transient periods in PHY operation during re-configuration procedures. */
	mipi_dsih_dphy_reset(fbi, 0);
	mipi_dsih_dphy_clock_en(fbi, 0);
	mipi_dsih_dphy_shutdown(fbi, 0);

	/* provide an initial active-high test clear pulse in TESTCLR  */
	mipi_dsih_dphy_test_clear(fbi, 1);
	mipi_dsih_dphy_test_clear(fbi, 0);
	/* find ranges */
	for (range = 0; (range < (sizeof(ranges)/sizeof(ranges[0]))) && ((output_freq / 1000) > ranges[range].freq); range++);
	if (range >= (sizeof(ranges)/sizeof(ranges[0]))) {
		return -EINVAL;
	}

	/* setup digital part */
	/* hs frequency range [7]|[6:1]|[0]*/
	data[0] = (0 << 7) | (ranges[range].hs_freq << 1) | 0;
	mipi_dsih_dphy_write(fbi, 0x44, data, 1);
	/* setup PLL */
	/* vco range  [7]|[6:3]|[2:1]|[0] */
	data[0] = (1 << 7) | (ranges[range].vco_range << 3) | (0 << 1) | 0;
	mipi_dsih_dphy_write(fbi, 0x10, data, 1);
	/* PLL  reserved|Input divider control|Loop Divider Control|Post Divider Ratio [7:6]|[5]|[4]|[3:0] */
	data[0] = (0x00 << 6) | (0x01 << 5) | (0x01 << 4) | (0x03 << 0); /* post divider default = 0x03 - it is only used for clock out 2*/
	mipi_dsih_dphy_write(fbi, 0x19, data, 1);
	/* PLL Lock bypass|charge pump current [7:4]|[3:0] */
	data[0] = (0x00 << 4) | (loop_bandwidth[i].cp_current << 0);
	mipi_dsih_dphy_write(fbi, 0x11, data, 1);
	/* bypass CP default|bypass LPF default| LPF resistor [7]|[6]|[5:0] */
	data[0] = (0x01 << 7) | (0x01 << 6) |(loop_bandwidth[i].lpf_resistor << 0);
	mipi_dsih_dphy_write(fbi, 0x12, data, 1);
	/* PLL input divider ratio [7:0] */
	data[0] = input_divider - 1;
	mipi_dsih_dphy_write(fbi, 0x17, data, 1);
	no_of_bytes = 2; /* pll loop divider (code 0x18) takes only 2 bytes (10 bits in data) */
	for (i = 0; i < no_of_bytes; i++) {
		data[i] = ((unsigned char)((((loop_divider - 1) >> (5 * i)) & 0x1F) | (i << 7) ));
		/* 7 is dependent on no_of_bytes
		make sure 5 bits only of value are written at a time */
	}
	/* PLL loop divider ratio - SET no|reserved|feedback divider [7]|[6:5]|[4:0] */
	mipi_dsih_dphy_write(fbi, 0x18, data, no_of_bytes);
	data[0] = 0x92;	//35ns
	mipi_dsih_dphy_write(fbi, 0x71, data, 1);	//T-prepare time
	mipi_dsih_dphy_stop_wait_time(fbi, 0x0A); //old  0x1C
	mipi_dsih_dphy_clock_en(fbi, 1);
	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);

	return 0;
}

/**********************Config DPI video mode*****************************/
/**
 * Configure DPI video interface
 * - If not in burst mode, it will compute the video and null packet sizes
 * according to necessity
 * @param instance pointer to structure holding the DSI Host core information
 * @param video_params pointer to video stream-to-send information
 * @return error code
 */
int mipi_dsih_dpi_video(struct comipfb_info *fbi, struct mipi_timing *mipi_time)
{
	unsigned int err_code = 0;
	unsigned int bytes_per_pixel_x100 = 0; /* bpp x 100 because it can be 2.25 */
	unsigned int video_size = 0;
	unsigned int ratio_clock_xPF = 0; /* holds dpi clock/byte clock times precision factor */
	unsigned int null_packet_size = 0;
	unsigned int video_size_step = 1;
	unsigned int hs_timeout = 0;
	unsigned int total_bytes = 0;
	unsigned int bytes_per_chunk = 0;
	unsigned int no_of_chunks = 0;
	unsigned int bytes_left = 0;
	unsigned int chunk_overhead = 0;
	int counter = 0;
	struct comipfb_dev *cdev;
	struct comipfb_dev_timing_mipi mipi;
	
	cdev = fbi->cdev;
	mipi = cdev->timing.mipi;
	/* set up phy pll to required lane clock */
	err_code = mipi_dsih_dphy_configure(fbi);
	if (err_code)
		goto end;

	ratio_clock_xPF = (mipi.output_freq * PRECISION_FACTOR) / (fbi->pixclock / 1000);
	video_size = mipi_time->hact;

	/*disable error report*/
	mipi_dsih_hal_error_mask_0(fbi, 0xffffff);
	mipi_dsih_hal_error_mask_1(fbi, 0xffffff);
	/* automatically disable CRC reporting */
	mipi_dsih_hal_gen_crc_rx_en(fbi, 0);
	/* set up ACKs and error reporting */
	mipi_dsih_hal_dpi_frame_ack_en(fbi, mipi_time->receive_ack_packets);
	if (mipi_time->receive_ack_packets) { 
	/* if ACK is requested, enable BTA, otherwise leave as is */
		mipi_dsih_hal_bta_en(fbi, 1);
	}
	else {
		mipi_dsih_hal_bta_en(fbi, 0);
	}
	mipi_dsih_hal_gen_cmd_mode_en(fbi, 0);
	mipi_dsih_hal_dpi_video_mode_en(fbi, 0);
	/* get bytes per pixel and video size step (depending if loosely or not */
	switch (mipi_time->color_coding) {
		case COLOR_CODE_16BIT_CONFIG1:
		case COLOR_CODE_16BIT_CONFIG2:
		case COLOR_CODE_16BIT_CONFIG3:
			bytes_per_pixel_x100 = 200;
			video_size_step = 1;
		break;
		case COLOR_CODE_18BIT_CONFIG1:
		case COLOR_CODE_18BIT_CONFIG2:
			mipi_dsih_hal_dpi_18_loosely_packet_en(fbi, mipi_time->is_18_loosely);
			bytes_per_pixel_x100 = 225;
			if (!mipi_time->is_18_loosely) { 
				/* 18bits per pixel and NOT loosely, packets should be multiples of 4 */
				video_size_step = 4;
				/* round up active H pixels to a multiple of 4 */
				for (; (video_size % 4) != 0; video_size++);
			}
			else {
				video_size_step = 1;
			}
		break;
		case COLOR_CODE_24BIT:
			bytes_per_pixel_x100 = 300;
			video_size_step = 1;
		break;
		default:
		break;
	}
	err_code = mipi_dsih_hal_dpi_color_coding(fbi, mipi_time->color_coding);
	if (err_code != 0)
		goto end;
	
	err_code = mipi_dsih_hal_dpi_video_mode_type(fbi, mipi_time->trans_mode);
	if (err_code != 0)
		goto end;
	mipi_dsih_hal_dpi_hline(fbi, (unsigned int)((mipi_time->htotal * ratio_clock_xPF) / PRECISION_FACTOR));
	mipi_dsih_hal_dpi_hbp(fbi, ((mipi_time->hbp * ratio_clock_xPF) / PRECISION_FACTOR));
	mipi_dsih_hal_dpi_hsa(fbi, ((mipi_time->hsa * ratio_clock_xPF) / PRECISION_FACTOR));
	mipi_dsih_hal_dpi_vactive(fbi, mipi_time->vactive);
	mipi_dsih_hal_dpi_vfp(fbi, mipi_time->vfp);
	mipi_dsih_hal_dpi_vbp(fbi, mipi_time->vbp);
	mipi_dsih_hal_dpi_vsync(fbi, mipi_time->vsa);
	mipi_dsih_hal_dpi_hsync_pol(fbi, !mipi_time->h_polarity);
	mipi_dsih_hal_dpi_vsync_pol(fbi, !mipi_time->v_polarity);
	mipi_dsih_hal_dpi_dataen_pol(fbi, !mipi_time->data_en_polarity);
	mipi_dsih_hal_dpi_color_mode_pol(fbi, !mipi_time->color_mode_polarity);
	mipi_dsih_hal_dpi_shut_down_pol(fbi, !mipi_time->shut_down_polarity);
	mipi_dsih_phy_hs2lp_config(fbi, mipi_time->max_hs_to_lp_cycles);
	mipi_dsih_phy_lp2hs_config(fbi, mipi_time->max_lp_to_hs_cycles);
	err_code = mipi_dsih_phy_bta_time(fbi, mipi_time->max_bta_cycles);
	if (err_code != 0)
		goto end;
	/* HS timeout */
	hs_timeout = ((mipi_time->htotal * mipi_time->vactive) + (DSIH_PIXEL_TOLERANCE * bytes_per_pixel_x100) / 100);
	for (counter = 0x80; (counter < hs_timeout) && (counter > 2); counter--) {
		if ((hs_timeout % counter) == 0) {
			mipi_dsih_hal_timeout_clock_division(fbi, counter);
			mipi_dsih_hal_lp_rx_timeout(fbi, (uint16_t)(hs_timeout / counter));
			mipi_dsih_hal_hs_tx_timeout(fbi, (uint16_t)(hs_timeout / counter));
			break;
		}
	}
	/* video packetisation */
	if (mipi_time->trans_mode == VIDEO_BURST_WITH_SYNC_PULSES) { 
		/* BURST */
		no_of_chunks = 1;
		null_packet_size = 0;
		/* BURST by default, returns to LP during ALL empty periods - energy saving */
		mipi_dsih_hal_dpi_lp_during_hfp(fbi, 1);
		mipi_dsih_hal_dpi_lp_during_hbp(fbi, 1);
		mipi_dsih_hal_dpi_lp_during_vactive(fbi, 1);
		mipi_dsih_hal_dpi_lp_during_vfp(fbi, 1);
		mipi_dsih_hal_dpi_lp_during_vbp(fbi, 1);
		mipi_dsih_hal_dpi_lp_during_vsync(fbi, 1);
	}else {	
		/* non burst transmission */
		null_packet_size = 0;
		/* bytes to be sent - first as one chunk*/
		bytes_per_chunk = (bytes_per_pixel_x100 * mipi_time->hact) / 100 + VIDEO_PACKET_OVERHEAD;
		/* bytes being received through the DPI interface per byte clock cycle */
		total_bytes = (ratio_clock_xPF * mipi.no_lanes * 
				(mipi_time->htotal - mipi_time->hbp - mipi_time->hsa - mipi_time->hfp)) / PRECISION_FACTOR;
		/* check if the in pixels actually fit on the DSI link */
		if (total_bytes >= bytes_per_chunk) {
			chunk_overhead = total_bytes - bytes_per_chunk;
			/* overhead higher than 1 -> enable multi packets */
			if (chunk_overhead > 1) {	
				/* MULTI packets */
				for (video_size = video_size_step; video_size < mipi_time->hact; 
							video_size += video_size_step) {
					/* determine no of chunks */
					if ((((mipi_time->hact * PRECISION_FACTOR) / video_size) % PRECISION_FACTOR) == 0) {
						no_of_chunks = mipi_time->hact / video_size;
						bytes_per_chunk = (bytes_per_pixel_x100 * video_size) / 100 + VIDEO_PACKET_OVERHEAD;
						if (total_bytes >= (bytes_per_chunk * no_of_chunks)) {
							bytes_left = total_bytes - (bytes_per_chunk * no_of_chunks);
							break;
						}
					}
				}
				/* prevent overflow (unsigned - unsigned) */
				if (bytes_left > (NULL_PACKET_OVERHEAD * no_of_chunks)) {
					null_packet_size = (bytes_left - (NULL_PACKET_OVERHEAD * no_of_chunks)) / no_of_chunks;
					if (null_packet_size > MAX_NULL_SIZE) {	
						/* avoid register overflow */
						null_packet_size = MAX_NULL_SIZE;
					}
				}
			}else {	
				/* no multi packets */
				no_of_chunks = 1;
				/* video size must be a multiple of 4 when not 18 loosely */
				for (video_size = mipi_time->hact; (video_size % video_size_step) != 0; video_size++);

			}
		}else {
			err_code = -EINVAL;
			goto end;
		}
	}
	err_code = mipi_dsih_hal_dpi_chunks_no(fbi, no_of_chunks);
	err_code = err_code ? err_code : mipi_dsih_hal_dpi_video_packet_size(fbi, video_size);
	err_code = err_code ? err_code : mipi_dsih_hal_dpi_null_packet_size(fbi, null_packet_size);
	if (err_code != 0)
		goto end;
	mipi_dsih_hal_dpi_null_packet_en(fbi, null_packet_size > 0 ? 1 : 0);
	mipi_dsih_hal_dpi_multi_packet_en(fbi, (no_of_chunks > 1) ? 1 : 0);
	mipi_dsih_hal_dpi_video_vc(fbi, mipi_time->virtual_channel);
	mipi_dsih_dphy_no_of_lanes(fbi, mipi.no_lanes);
	/* TX_ESC_CLOCK_DIV must be less than 20000KHz */
	mipi_dsih_hal_tx_escape_division(fbi, 0x4);

	/* by default, all commands are sent in LP */
	mipi_dsih_hal_dcs_wr_tx_type(fbi, 0, 1);
	mipi_dsih_hal_dcs_wr_tx_type(fbi, 1, 1);
	mipi_dsih_hal_dcs_wr_tx_type(fbi, 3, 1); /* long packet*/
	mipi_dsih_hal_dcs_rd_tx_type(fbi, 0, 1);
	mipi_dsih_hal_gen_wr_tx_type(fbi, 0, 1);
	mipi_dsih_hal_gen_wr_tx_type(fbi, 1, 1);
	mipi_dsih_hal_gen_wr_tx_type(fbi, 2, 1);
	mipi_dsih_hal_gen_wr_tx_type(fbi, 3, 1); /* long packet*/
	mipi_dsih_hal_gen_rd_tx_type(fbi, 0, 1);
	mipi_dsih_hal_gen_rd_tx_type(fbi, 1, 1);
	mipi_dsih_hal_gen_rd_tx_type(fbi, 2, 1);

	/* by default, RX_VC = 0, NO EOTp, EOTn, BTA, ECC rx and CRC rx */
	mipi_dsih_hal_gen_rd_vc(fbi, 0);
	mipi_dsih_hal_gen_eotp_rx_en(fbi, 0); //byd not support.
	mipi_dsih_hal_gen_eotp_tx_en(fbi, 0);  //byd not support.
	mipi_dsih_hal_gen_ecc_rx_en(fbi, 0);
	
	/* enable high speed clock */
	mipi_dsih_hal_power(fbi, 0);  // Reset the DSI Host controller
	mipi_dsih_hal_power(fbi, 1);  // Reset the DSI Host controller
end:
	return err_code;
}

EXPORT_SYMBOL(mipi_dsih_dpi_video);
EXPORT_SYMBOL(mipi_dsih_hal_bta_en);
EXPORT_SYMBOL(mipi_dsih_hal_gen_rd_vc);
EXPORT_SYMBOL(mipi_dsih_hal_power);
