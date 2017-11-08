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

#include <linux/kernel.h>

#include "mipi_interface.h"

#define COMPARE(a,b,c) (a >= (b * 1000)) && (a < (c * 1000))

#if defined(CONFIG_CPU_LC1813)
static struct {
	uint32_t freq;			//upper margin of frequency range
	uint8_t hs_freq;		//hsfreqrange
	uint8_t vco_range;		//vcorange
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
	uint32_t loop_div;			// upper limit of loop divider range
	uint8_t cp_current;			// icpctrl
	uint8_t lpf_resistor;		// lpfctrl
}loop_bandwidth[] = {
	{32, 0x06, 0x10}, {64, 0x06, 0x10}, {128, 0x0C, 0x08},
	{256, 0x04, 0x04}, {512, 0x00, 0x01}, {768, 0x01, 0x01},
	{1000, 0x02, 0x01},
};
#elif defined(CONFIG_CPU_LC1860)
static struct {
	uint32_t freq;			//upper margin of frequency range
	uint8_t hs_freq;		//hsfreqrange
	uint8_t vco_range;		//vcorange
}ranges[] = {
	{90, 0x00, 0x00}, {100, 0x10, 0x00}, {110, 0x20, 0x00},
	{130, 0x01, 0x00}, {140, 0x11, 0x00}, {150, 0x21, 0x00},
	{170, 0x02, 0x00}, {180, 0x12, 0x00}, {200, 0x22, 0x00},
	{220, 0x03, 0x01}, {240, 0x13, 0x01}, {250, 0x23, 0x01},
	{270, 0x04, 0x01}, {300, 0x14, 0x01}, {330, 0x05, 0x02},
	{360, 0x15, 0x02}, {400, 0x25, 0x02}, {450, 0x06, 0x02},
	{500, 0x16, 0x02}, {550, 0x07, 0x03}, {600, 0x17, 0x03},
	{650, 0x08, 0x03}, {700, 0x18, 0x03}, {750, 0x09, 0x04},
	{800, 0x19, 0x04}, {850, 0x29, 0x04}, {900, 0x39, 0x04},
	{950, 0x0A, 0x05}, {1000, 0x1A, 0x05}, {1050, 0x2A, 0x05},
	{1100, 0x3A, 0x05}, {1150, 0x0B, 0x06}, {1200, 0x1B, 0x06},
	{1250, 0x2B, 0x06}, {1300, 0x3B, 0x06}, {1350, 0x0C, 0x07},
	{1400, 0x1C, 0x07}, {1450, 0x2C, 0x07}, {1500, 0x3C, 0x07},
};

static struct {
	uint32_t loop_div;			// upper limit of loop divider range
	uint8_t cp_current;			// icpctrl
	uint8_t lpf_resistor;		// lpfctrl
}loop_bandwidth[] = {
	{110, 0x02, 0x02}, {150, 0x02, 0x01}, {200, 0x09, 0x01}, //vcorange 0
	{250, 0x09, 0x04}, {300, 0x06, 0x04}, //vcorange 1
	{400, 0x09, 0x04}, {500, 0x06, 0x04}, //vcorange 2
	{600, 0x06, 0x04}, {700, 0x0A, 0x04}, //vcorange 3
	{900, 0x0A, 0x04}, //vcorange 4
	{1100, 0x0B, 0x08}, //vcorange 5
	{1300, 0x0B, 0x08}, //vcorange 6
	{1500, 0x0B, 0x08}, //vcorange 7
};
#endif
/************************MIPI *******************************************/
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
unsigned int mipi_dsih_read_word(struct comipfb_info *fbi, uint32_t reg_address)
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
unsigned int mipi_dsih_read_part(struct comipfb_info *fbi, uint32_t reg_address, uint8_t shift, uint8_t width)
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

/**********************DPI CONFIG***********************/
/**
 * Write the DPI video virtual channel destination
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc virtual channel
 */
static void mipi_dsih_hal_dpi_video_vc(struct comipfb_info *fbi, uint8_t vc)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_VCID, (uint32_t)(vc), 0, 2);
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

	if (color_coding >= COLOR_CODE_MAX) {
		err = -EINVAL;
	}
	else {
		mipi_dsih_write_part(fbi, MIPI_DPI_COLOR_CODING, color_coding, MIPI_DPI_COLOR_CODING_BIT, 4);
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
	mipi_dsih_write_part(fbi, MIPI_DPI_COLOR_CODING, enable, MIPI_EN18_LOOSELY, 1);
}

/**
 * Set DPI color mode pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_color_mode_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG_POL, active_low, MIPI_COLORM_ACTIVE_LOW, 1);
}

/**
 * Set DPI shut down pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_shut_down_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG_POL, active_low, MIPI_SHUTD_ACTIVE_LOW, 1);
}

/**
 * Set DPI horizontal sync pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_hsync_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG_POL, active_low, MIPI_HSYNC_ACTIVE_LOW, 1);
}

/**
 * Set DPI vertical sync pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_vsync_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG_POL, active_low, MIPI_VSYNC_ACTIVE_LOW, 1);
}

/**
 * Set DPI data enable pin polarity
 * @param instance pointer to structure holding the DSI Host core information
 * @param active_low (1) or active high (0)
 */
static void mipi_dsih_hal_dpi_dataen_pol(struct comipfb_info *fbi, int active_low)
{
	mipi_dsih_write_part(fbi, MIPI_DPI_CFG_POL, active_low, MIPI_DATAEN_ACTIVE_LOW, 1);
}

/**
 * Enable FRAME BTA ACK
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_frame_ack_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_FRAME_BTA_ACK, 1);
}

/**
 * Enable return to low power mode inside horizontal front porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_hfp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_LP_HFP_EN, 1);
}
/**
 * Enable return to low power mode inside horizontal back porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_hbp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_LP_HBP_EN, 1);
}
/**
 * Enable return to low power mode inside vertical active lines periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vactive(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_LP_VACT_EN, 1);
}
/**
 * Enable return to low power mode inside vertical front porch periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vfp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_LP_VFP_EN, 1);
}
/**
 * Enable return to low power mode inside vertical back porch periods when
 * timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vbp(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_LP_VBP_EN, 1);
}
/**
 * Enable return to low power mode inside vertical sync periods when
 *  timing allows
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_during_vsync(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_LP_VSA_EN, 1);
}

/**
 * Config enter lp state when porch in video mode
 * @param instance pointer to structure holding the DSI Host core information
 */
static void mipi_dsih_hal_video_enter_lp_config(struct comipfb_info *fbi)
{
	struct comipfb_dev_timing_mipi *mipi;
	
	mipi = &(fbi->cdev->timing.mipi);

	if (mipi->videomode_info.lp_hfp_en > 0)
		mipi_dsih_hal_dpi_lp_during_hfp(fbi, 1);
	else
		mipi_dsih_hal_dpi_lp_during_hfp(fbi, 0);
		
	if (mipi->videomode_info.lp_hbp_en > 0)
		mipi_dsih_hal_dpi_lp_during_hbp(fbi, 1);
	else
		mipi_dsih_hal_dpi_lp_during_hbp(fbi, 0);
	
	if (mipi->videomode_info.lp_vact_en > 0)
		mipi_dsih_hal_dpi_lp_during_vactive(fbi, 1);
	else
		mipi_dsih_hal_dpi_lp_during_vactive(fbi, 0);
	
	if (mipi->videomode_info.lp_vfp_en > 0)
		mipi_dsih_hal_dpi_lp_during_vfp(fbi, 1);
	else
		mipi_dsih_hal_dpi_lp_during_vfp(fbi, 0);
	
	if (mipi->videomode_info.lp_vbp_en > 0)
		mipi_dsih_hal_dpi_lp_during_vbp(fbi, 1);
	else
		mipi_dsih_hal_dpi_lp_during_vbp(fbi, 0);
	
	if (mipi->videomode_info.lp_vsa_en > 0)
		mipi_dsih_hal_dpi_lp_during_vsync(fbi, 1);
	else
		mipi_dsih_hal_dpi_lp_during_vsync(fbi, 0);
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
		mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, type, MIPI_VID_MODE_TYPE, 2);
		return 0;
	}else {
		printk("in mipi_dsih_hal_dpi_video_mode_type\n");
		return -EINVAL;
	}
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
	if (size < 0x2000) {
		/* 10-bit field */
		mipi_dsih_write_part(fbi, MIPI_VID_NULL_SIZE, size, 0, 13);
	}else {
		printk(KERN_WARNING "Null packet size is large size is %x\n",size);
		mipi_dsih_write_part(fbi, MIPI_VID_NULL_SIZE, 0x1fff, 0, 13);
	}
	return 0;
}
/**
 * Write no of chunks to core - taken into consideration only when multi packet
 * is enabled
 * @param instance pointer to structure holding the DSI Host core information
 * @param no of chunks
 */
static int mipi_dsih_hal_dpi_chunks_no(struct comipfb_info *fbi, uint16_t no)
{
	if (no < 0x2000) {
		mipi_dsih_write_part(fbi, MIPI_VID_NUM_CHUNKS, no, 0, 13);
	}else {
		printk(KERN_WARNING "chunks number is large no is %x\n",no);
		mipi_dsih_write_part(fbi, MIPI_VID_NUM_CHUNKS, 0x1fff, 0, 13);
	}
	return 0;
}
/**
 * Write video packet size. obligatory for sending video
 * @param instance pointer to structure holding the DSI Host core information
 * @param size of video packet - containing information
 * @return error code
 */
static int mipi_dsih_hal_dpi_video_packet_size(struct comipfb_info *fbi, uint16_t size)
{
	if (size < 0x4000) {
		mipi_dsih_write_part(fbi, MIPI_VID_PKT_SIZE, size, 0, 14);
	}else {
		printk(KERN_WARNING "Video packet size  is large size is %x\n",size);
		mipi_dsih_write_part(fbi, MIPI_VID_PKT_SIZE, 0x3fff, 0, 14);
	}
	return 0;
}

/**
 * Config time when send command in lp mode
 * @param instance pointer to structure holding the DSI Host core information
 */
static void mipi_dsih_hal_dpi_lp_cmd_time_config(struct comipfb_info *fbi, unsigned int video_size, unsigned int bytes_per_pixel, unsigned int null_packet)
{
	unsigned char outvact_lpcmd_time, invact_lpcmd_time;
	unsigned int line_time, hsa_time, hbp_time, hact_time, phy_lphs_time, phy_hslp_time;
	unsigned int lp_div;
	unsigned int val;
	unsigned int eotp_time = 0;
	struct comipfb_dev_timing_mipi *mipi;
	
	mipi = &(fbi->cdev->timing.mipi);

	lp_div = mipi_dsih_read_part(fbi, MIPI_CLKMGR_CFG, MIPI_TX_ESC_CLK_DIVISION, 8);
	line_time = mipi_dsih_read_word(fbi, MIPI_VID_HLINE_TIME);
	hsa_time = mipi_dsih_read_word(fbi, MIPI_VID_HSA_TIME);
	hbp_time = mipi_dsih_read_word(fbi, MIPI_VID_HBP_TIME);
	phy_lphs_time = mipi_dsih_read_part(fbi, MIPI_PHY_TMR_CFG, MIPI_PHY_LP2HS_TIME, 8);
	phy_hslp_time = mipi_dsih_read_part(fbi, MIPI_PHY_TMR_CFG, MIPI_PHY_HS2LP_TIME, 8);

	if ((mipi->videomode_info.mipi_trans_type == VIDEO_BURST_WITH_SYNC_PULSES) 
		 || (mipi->videomode_info.mipi_trans_type == VIDEO_NON_BURST_WITH_SYNC_EVENTS)) {
		 if (mipi->ext_info.eotp_tx_en > 0)
		 	eotp_time = 4;
		 hsa_time = 4;
	}else {
		eotp_time = 0;
	}

	if (line_time < ((hsa_time + eotp_time) + phy_hslp_time + phy_lphs_time))
		val = 0;
	else
		val = (line_time - (hsa_time + eotp_time) - phy_hslp_time - phy_lphs_time) * 1000 / lp_div;

	if (val < (22 + 2) * 1000)
		outvact_lpcmd_time = 0;
	else
		outvact_lpcmd_time = (val - (22 + 2)) / (2 * 8) / 1000;
	printk(KERN_DEBUG "%s: outvact_lpcmd_time = %d\n", __func__, outvact_lpcmd_time);
	if (outvact_lpcmd_time < 4)
		printk(KERN_WARNING "outvact_lpcmd_time val is littler 4 is %d\n", outvact_lpcmd_time);

	hsa_time = mipi_dsih_read_word(fbi, MIPI_VID_HSA_TIME);
	if (mipi->videomode_info.mipi_trans_type == VIDEO_BURST_WITH_SYNC_PULSES) {
		hact_time = video_size * bytes_per_pixel / mipi->no_lanes;
	}else {
		hact_time = (video_size * bytes_per_pixel + null_packet ) / mipi->no_lanes; // ??? TODO
	}

	if (line_time < (hsa_time + hbp_time + hact_time + phy_hslp_time + phy_lphs_time))
		val = 0;
	else if ((line_time - hsa_time - hbp_time - hact_time - phy_hslp_time - phy_lphs_time) % lp_div != 0)
		val = (line_time - hsa_time - hbp_time - hact_time - phy_hslp_time - phy_lphs_time) / lp_div + 1;
	else
		val = (line_time - hsa_time - hbp_time - hact_time - phy_hslp_time - phy_lphs_time) / lp_div;

	if (val < (22 + 2))
		invact_lpcmd_time = 0;
	else if ((val - (22 + 2)) % (2 * 8) == 0)
		invact_lpcmd_time = (val - (22 + 2)) / (2 * 8);
	else
		invact_lpcmd_time = (val - (22 + 2)) / (2 * 8) + 1;
	printk(KERN_DEBUG "%s: invact_lpcmd_time = %d\n", __func__, invact_lpcmd_time);
	if (invact_lpcmd_time < 4)
		printk(KERN_WARNING "invact_lpcmd_time val is littler 4 is %d\n", invact_lpcmd_time);
	mipi_dsih_write_word(fbi, MIPI_DPI_LP_CMD_TIM, ((outvact_lpcmd_time << MIPI_OUTVACT_LPCMD_TIME) | (invact_lpcmd_time << MIPI_INVACT_LPCMD_TIME)));

}

/**
 * Configure the Horizontal Line time
 * @param instance pointer to structure holding the DSI Host core information
 * @param time taken to transmit the total of the horizontal line
 */
static void mipi_dsih_hal_dpi_hline(struct comipfb_info *fbi, uint16_t time)
{
	if ( time < 0x8000)
		mipi_dsih_write_part(fbi, MIPI_VID_HLINE_TIME, time, 0, 15);
	else {
		printk(KERN_WARNING "mipi hline val is %x larged than 0x8000 \n",time);
		mipi_dsih_write_part(fbi, MIPI_VID_HLINE_TIME, 0x7fff, 0, 15);
	}
}
/**
 * Configure the Horizontal back porch time
 * @param instance pointer to structure holding the DSI Host core information
 * @param time taken to transmit the horizontal back porch
 */
static void mipi_dsih_hal_dpi_hbp(struct comipfb_info *fbi, uint16_t time)
{	if ( time < 0x1000)
		mipi_dsih_write_part(fbi, MIPI_VID_HBP_TIME, time, 0, 12);
	else {
		printk(KERN_WARNING "mipi hbp val is %x larged than 0x1000 \n",time);
		mipi_dsih_write_part(fbi, MIPI_VID_HBP_TIME, 0xfff, 0, 12);
	}
}
/**
 * Configure the Horizontal sync time
 * @param instance pointer to structure holding the DSI Host core information
 * @param time taken to transmit the horizontal sync
 */
static void mipi_dsih_hal_dpi_hsa(struct comipfb_info *fbi, uint16_t time)
{
	if ( time < 0x1000)
		mipi_dsih_write_part(fbi, MIPI_VID_HSA_TIME, time, 0, 12);
	else {
		printk(KERN_WARNING "mipi hsa val is %x larged than 0x1000 \n",time);
		mipi_dsih_write_part(fbi, MIPI_VID_HSA_TIME, 0xfff, 0, 12);
	}
}
/**
 * Configure the vertical active lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vactive(struct comipfb_info *fbi, uint16_t lines)
{
	if (lines < 0x4000)
		mipi_dsih_write_part(fbi, MIPI_VID_VACT_LINES, lines, 0, 14);
	else {
		printk(KERN_WARNING "mipi vactive val is %x larged than 0x4000 \n",lines);
		mipi_dsih_write_part(fbi, MIPI_VID_VACT_LINES, 0x3fff, 0, 14);
	}
}
/**
 * Configure the vertical front porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vfp(struct comipfb_info *fbi, uint16_t lines)
{
	if (lines < 0x400)
	mipi_dsih_write_part(fbi, MIPI_VID_VFP_LINES, lines, 0, 10);
	else {
		printk(KERN_WARNING "mipi vfp val is %x larged than 0x400 \n",lines);
		mipi_dsih_write_part(fbi, MIPI_VID_VFP_LINES, 0x3ff, 0, 10);
	}
}
/**
 * Configure the vertical back porch lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vbp(struct comipfb_info *fbi, uint16_t lines)
{
	if (lines < 0x400)
		mipi_dsih_write_part(fbi, MIPI_VID_VBP_LINES, lines, 0, 10);
	else {
		printk(KERN_WARNING "mipi vbp val is %x larged than 0x400 \n",lines);
		mipi_dsih_write_part(fbi, MIPI_VID_VBP_LINES, 0x3ff, 0, 10);
	}
}
/**
 * Configure the vertical sync lines of the video stream
 * @param instance pointer to structure holding the DSI Host core information
 * @param lines
 */
static void mipi_dsih_hal_dpi_vsync(struct comipfb_info *fbi, uint16_t lines)
{
	if (lines < 0x400)
		mipi_dsih_write_part(fbi, MIPI_VID_VSA_LINES, lines, 0, 10);
	else {
		printk(KERN_WARNING "mipi vsa val is %x larged than 0x400 \n",lines);
		mipi_dsih_write_part(fbi, MIPI_VID_VSA_LINES, 0x3ff, 0, 10);
	}
}
/**
 * Enable/disable lp command trans in DPI video mode
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable (1) - disable (0)
 */
static void mipi_dsih_hal_dpi_lp_cmd_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_VID_MODE_CFG, enable, MIPI_LP_CMD_EN, 1);
}
/******************************COMMAND CONFIG*************************************/
/**
 * Config timeout count in command mode
 * @param instance pointer to structure holding the DSI Host core information
 */
static void mipi_dsih_hal_cmd_timeout_config(struct comipfb_info *fbi)
{
	struct rw_timeout *timeout;

	timeout = &(fbi->cdev->timing.mipi.commandmode_info.timeout);

	if (timeout->hs_rd_to_cnt > 0)
		mipi_dsih_write_word(fbi, MIPI_HS_RD_TO_CNT, timeout->hs_rd_to_cnt);
	else
		mipi_dsih_write_word(fbi, MIPI_HS_RD_TO_CNT, 0);

	if (timeout->lp_rd_to_cnt > 0)
		mipi_dsih_write_word(fbi, MIPI_LP_RD_TO_CNT, timeout->lp_rd_to_cnt);
	else
		mipi_dsih_write_word(fbi, MIPI_LP_RD_TO_CNT, 0);

	if (timeout->hs_wr_to_cnt > 0)
		mipi_dsih_write_word(fbi, MIPI_HS_WR_TO_CNT, timeout->hs_wr_to_cnt);
	else
		mipi_dsih_write_word(fbi, MIPI_HS_WR_TO_CNT, 0);

	if (timeout->lp_wr_to_cnt > 0)
		mipi_dsih_write_word(fbi, MIPI_LP_WR_TO_CNT, timeout->lp_wr_to_cnt);
	else
		mipi_dsih_write_word(fbi, MIPI_LP_WR_TO_CNT, 0);

	if (timeout->bta_to_cnt > 0)
		mipi_dsih_write_word(fbi, MIPI_BTA_TO_CNT, timeout->bta_to_cnt);
	else
		mipi_dsih_write_word(fbi, MIPI_BTA_TO_CNT, 0);
}

/**
 * Config command size in command mode
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 */
static void mipi_dsih_hal_cmd_size_config(struct comipfb_info *fbi, unsigned int bytes_per_pixel)
{
	unsigned int val;

	val = (1440 - 2) * 4 / (3 * bytes_per_pixel); // 1440 : pld_fifo_depth
	mipi_dsih_write_word(fbi, MIPI_EDPI_CMD_SIZE, val);
}

/**
 * Set DCS command packet transmission to transmission type
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_param of command
 * @param lp transmit in low power
 * @return error code
 */
int mipi_dsih_hal_dcs_wr_tx_type(struct comipfb_info *fbi, unsigned no_of_param, int lp)
{
	switch (no_of_param) {
		case 0:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_DCS_SW_0P_TX, 1);
		break;
		case 1:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_DCS_SW_1P_TX, 1);
		break;
		default:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_DCS_LW_TX, 1);
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
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_DCS_SR_0P_TX, 1);
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
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_GEN_SW_0P_TX, 1);
		break;
		case 1:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_GEN_SW_1P_TX, 1);
		break;
		case 2:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_GEN_SW_2P_TX, 1);
		break;
		default:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_GEN_LW_TX, 1);
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
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_GEN_SR_0P_TX, 1);
		break;
		case 1:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_GEN_SR_1P_TX, 1);
		break;
		case 2:
			mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_GEN_SR_2P_TX, 1);
		break;
		default:
			err = -EINVAL;
		break;
	}
	return err;
}

/**
 * Set max rd  packet type
 * @param instance pointer to structure holding the DSI Host core information
 * @param lp transmit in low power
 * @return error code
 */
static int mipi_dsih_hal_max_rd_pkt_type(struct comipfb_info *fbi, int lp)
{
	int err = 0;
	mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, lp, MIPI_MAX_RD_PKT_SIZE, 1);
	return err;
}

/**
 * Set  tear ack request in command
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable val
 */
static void mipi_dsih_hal_cmd_tear_ack_en(struct comipfb_info *fbi, unsigned int enable)
{
	mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, enable, MIPI_TEAR_FX_EN, 1);
}

/**
 * Set  packet ack request in command
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable val
 */
static void mipi_dsih_hal_cmd_pkt_ack_en(struct comipfb_info *fbi, unsigned int enable)
{
	mipi_dsih_write_part(fbi, MIPI_CMD_MODE_CFG, enable, MIPI_ACK_RQST_EN, 1);
}

/**
 * Config send command  lp state  in command mode
 * @param instance pointer to structure holding the DSI Host core information
 */
static void mipi_dsih_hal_cmd_lp_mode_config(struct comipfb_info *fbi)
{
	struct command_mode_info *command_info;

	command_info = &(fbi->cdev->timing.mipi.commandmode_info);

	if (command_info->dcs_sw_0p_tx > 0)
		mipi_dsih_hal_dcs_wr_tx_type(fbi, 0, 1);
	else
		mipi_dsih_hal_dcs_wr_tx_type(fbi, 0, 0);

	if (command_info->dcs_sw_1p_tx > 0)
		mipi_dsih_hal_dcs_wr_tx_type(fbi, 1, 1);
	else
		mipi_dsih_hal_dcs_wr_tx_type(fbi, 1, 0);

	if (command_info->dcs_lw_tx > 0)
		mipi_dsih_hal_dcs_wr_tx_type(fbi, 3, 1); /* long packet*/
	else
		mipi_dsih_hal_dcs_wr_tx_type(fbi, 3, 0);

	if (command_info->dcs_sr_0p_tx> 0)
		mipi_dsih_hal_dcs_rd_tx_type(fbi, 0, 1);
	else
		mipi_dsih_hal_dcs_rd_tx_type(fbi, 0, 0);

	if (command_info->gen_sw_0p_tx > 0)
		mipi_dsih_hal_gen_wr_tx_type(fbi, 0, 1);
	else
		mipi_dsih_hal_gen_wr_tx_type(fbi, 0, 0);

	if (command_info->gen_sw_1p_tx > 0)
		mipi_dsih_hal_gen_wr_tx_type(fbi, 1, 1);
	else
		mipi_dsih_hal_gen_wr_tx_type(fbi, 1, 0);

	if (command_info->gen_sw_2p_tx > 0)
		mipi_dsih_hal_gen_wr_tx_type(fbi, 2, 1);
	else
		mipi_dsih_hal_gen_wr_tx_type(fbi, 2, 0);

	if (command_info->gen_lw_tx > 0)
		mipi_dsih_hal_gen_wr_tx_type(fbi, 3, 1); /* long packet*/
	else
		mipi_dsih_hal_gen_wr_tx_type(fbi, 3, 0);

	if (command_info->gen_sr_0p_tx > 0)
		mipi_dsih_hal_gen_rd_tx_type(fbi, 0, 1);
	else
		mipi_dsih_hal_gen_rd_tx_type(fbi, 0, 0);

	if (command_info->gen_sr_1p_tx > 0)
		mipi_dsih_hal_gen_rd_tx_type(fbi, 1, 1);
	else
		mipi_dsih_hal_gen_rd_tx_type(fbi, 1, 0);

	if (command_info->gen_sr_2p_tx > 0)
		mipi_dsih_hal_gen_rd_tx_type(fbi, 2, 1);
	else
		mipi_dsih_hal_gen_rd_tx_type(fbi, 2, 0);

	if (command_info->max_rd_pkt_size > 0)
		mipi_dsih_hal_max_rd_pkt_type(fbi, 1);
	else
		mipi_dsih_hal_max_rd_pkt_type(fbi, 0);
}

/***********************BASE CONFIG***********************************/
/**
 * Config dsi mode 
 * @param instance pointer to structure holding the DSI Host core information
 * @param mode val
 */
void mipi_dsih_hal_mode_config(struct comipfb_info *fbi, uint32_t val)
{
	mipi_dsih_write_word(fbi, MIPI_MODE_CFG, val);	
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
	mipi_dsih_write_part(fbi, MIPI_CLKMGR_CFG, tx_escape_division, MIPI_TX_ESC_CLK_DIVISION, 8);
}

/**
 * Write the generic virtual channel destination
 * @param instance pointer to structure holding the DSI Host core information
 * @param vc virtual channel
 */
static void mipi_dsih_hal_gen_vc(struct comipfb_info *fbi, uint8_t vc)
{
	mipi_dsih_write_part(fbi, MIPI_GEN_VCID, (uint32_t)(vc), 0, 2);
}

/**
 * Enable EOTp reception
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_eotp_rx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, MIPI_EN_EOTN_RX, 1);
}
/**
 * Enable EOTp transmission
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_eotp_tx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, MIPI_EN_EOTP_TX, 1);
}

/**
 * Enable ECC reception, error correction and reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_ecc_rx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, MIPI_EN_ECC_RX, 1);
}
/**
 * Enable CRC reception, error reporting
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
static void mipi_dsih_hal_gen_crc_rx_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, MIPI_EN_CRC_RX, 1);
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

/**
 * Modify power status of DSI Host core
 * @param instance pointer to structure holding the DSI Host core information
 * @param on (1) or off (0)
 */
void mipi_dsih_hal_power(struct comipfb_info *fbi, int on)
{
	mipi_dsih_write_word(fbi, MIPI_PWR_UP, on);
}

/**
 * Enable Bus Turn-around request
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable
 */
void mipi_dsih_hal_bta_en(struct comipfb_info *fbi, int enable)
{
	mipi_dsih_write_part(fbi, MIPI_PCKHDL_CFG, enable, MIPI_EN_BTA, 1);
}

unsigned int mipi_dsih_dphy_get_status(struct comipfb_info *fbi)
{
	return mipi_dsih_read_word(fbi, MIPI_PHY_STATUS);
}

/*************************************** D-PHY****************************** */
/**
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void mipi_dsih_dphy_test_clock(struct comipfb_info *fbi, int value)
{
        mipi_dsih_write_part(fbi, MIPI_PHY_TEST_CTRL0, value, MIPI_CTRL0_PHY_TESTCLK, 1);
}
/**
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void mipi_dsih_dphy_test_clear(struct comipfb_info *fbi, int value)
{
        mipi_dsih_write_part(fbi, MIPI_PHY_TEST_CTRL0, value, MIPI_CTRL0_PHY_TESTCLR, 1);
}
/**
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param on_falling_edge
 */
static void mipi_dsih_dphy_test_en(struct comipfb_info *fbi, uint8_t on_falling_edge)
{
        mipi_dsih_write_part(fbi,  MIPI_PHY_TEST_CTRL1, on_falling_edge, MIPI_CTRL1_PHY_TESTEN, 1);
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

/**
  *Configure phy timeing, for example lpx,t-prepare  and so on. 
  *@param instance pointer to structure holding the DSI Host core information
  */
static void mipi_dsih_phy_check_timing(struct phy_time_info *phy_timeinfo)
{
	struct comipfb_dev_timing_mipi *mipi;
	unsigned int hs_time;
	unsigned int val;

	mipi = container_of(phy_timeinfo, struct comipfb_dev_timing_mipi, phytime_info); 
	hs_time = MIPI_TIME_X1000000 / mipi->hs_freq;

	val = hs_time * phy_timeinfo->clk_tprepare;
	if (val < 38) {
		printk(KERN_WARNING "clk tprepare < 38ns \n");
		phy_timeinfo->clk_tprepare = 38 / hs_time + 2;
	}else if (val > 95) {
		printk(KERN_WARNING "clk tprepare > 95ns \n");
		phy_timeinfo->clk_tprepare = 95 / hs_time - 2;
	}

	val = hs_time * phy_timeinfo->data_tprepare;
	if (val < 48) {
		printk(KERN_WARNING "clk tprepare < 38ns \n");
		phy_timeinfo->data_tprepare = 38 / hs_time + 2;
	}else if (val > 97) {
		printk(KERN_WARNING "clk tprepare > 95ns \n");
		phy_timeinfo->data_tprepare = 95 / hs_time - 2;
	}

	val = hs_time * phy_timeinfo->lpx;
	if (val < 50) {
		printk(KERN_WARNING "lpx val < 50ns \n");
		phy_timeinfo->lpx = 50 / hs_time + 5;
	}

	val = (phy_timeinfo->clk_hs_zero + phy_timeinfo->clk_tprepare) * hs_time;
	if (val < 300) {
		printk(KERN_WARNING "clk hszero + clk tprepare < 300ns \n");
		phy_timeinfo->clk_hs_zero = 300 / hs_time - phy_timeinfo->clk_tprepare + 5;
	}

	val = (phy_timeinfo->data_hs_zero + phy_timeinfo->data_tprepare) * hs_time;
	if (val < 165) {
		printk(KERN_WARNING "clk hszero + clk tprepare < 300ns \n");
		phy_timeinfo->data_hs_zero = 165 / hs_time - phy_timeinfo->clk_tprepare + 5;
	}



}
static void mipi_dsih_phy_timing_config(struct comipfb_info *fbi)
{
	unsigned char data[4];
	unsigned char ta_get;
	unsigned char ta_go;
	unsigned char ta_sure;
	unsigned int escap_div = 0;
	unsigned int const_time = 2;
	struct phy_time_info *phy_timeinfo;
return;

	phy_timeinfo = &(fbi->cdev->timing.mipi.phytime_info);

	mipi_dsih_phy_check_timing(phy_timeinfo);
	
	escap_div = mipi_dsih_read_part(fbi, MIPI_CLKMGR_CFG, MIPI_TX_ESC_CLK_DIVISION, 8);
	ta_get = (unsigned char)(5 * phy_timeinfo->lpx / escap_div); // no register to set
	ta_go = (unsigned char)(4 * phy_timeinfo->lpx / escap_div);
	ta_sure = (unsigned char)(2 * phy_timeinfo->lpx / escap_div);

	if (phy_timeinfo->lpx > 0) {
		data[0] = (1 << 6) | (phy_timeinfo->lpx - 1 - const_time);
		mipi_dsih_dphy_write(fbi, 0x60, data, 1);
		mipi_dsih_dphy_write(fbi, 0x70, data, 1);
	}

	// TXDDRCLKHS = 1/2 TXBYTECLKHS
	if (phy_timeinfo->clk_tprepare > 0) {
		data[0] = (1 << 7) | (phy_timeinfo->clk_tprepare * 2 - 1 - const_time);
		mipi_dsih_dphy_write(fbi, 0x61, data, 1);		
	}
	if (phy_timeinfo->data_tprepare > 0) {
		data[0] = (1 << 7) | (phy_timeinfo->data_tprepare * 2 - 1 - const_time);
		mipi_dsih_dphy_write(fbi, 0x71, data, 1);		
	}
	if (phy_timeinfo->clk_hs_zero > 0) {
		data[0] = (1 << 6) | (phy_timeinfo->clk_hs_zero - 1 - const_time);
		mipi_dsih_dphy_write(fbi, 0x62, data, 1);		
	}
	if (phy_timeinfo->data_hs_zero > 0) {
		data[0] = (1 << 6) | (phy_timeinfo->data_hs_zero - 1 - const_time);
		mipi_dsih_dphy_write(fbi, 0x72, data, 1);		
	}
	if (ta_go > 0) {
		data[0] = ta_go;
		mipi_dsih_dphy_write(fbi, 0x05, data, 1);
	}
	if (ta_sure > 0) {
		data[0] = ta_sure;
		mipi_dsih_dphy_write(fbi, 0x06, data, 1);
	}
	
	//TODO...
}

/*Config HS clk enters LP automaticly*/
static void mipi_dsih_phy_hs_enter_lp(struct comipfb_info *fbi, int en)
{
	mipi_dsih_write_part(fbi, MIPI_LPCLK_CTRL, en, MIPI_PHY_AUTO_CLKLANE_CTRL, 1);
}

static int mipi_dsih_phy_clkhs2lp_config(struct comipfb_info *fbi, uint16_t no_of_byte_cycles)
{
	if (no_of_byte_cycles < 0x400) {
		mipi_dsih_write_part(fbi, MIPI_PHY_TMR_LPCLK_CFG ,no_of_byte_cycles, MIPI_PHY_CLKHS2LP_TIME, 10);
	}else {
		return -EINVAL;
	}
	return 0;
}
static int mipi_dsih_phy_clklp2hs_config(struct comipfb_info *fbi, uint16_t no_of_byte_cycles)
{
	if (no_of_byte_cycles < 0x400) {
		mipi_dsih_write_part(fbi, MIPI_PHY_TMR_LPCLK_CFG, no_of_byte_cycles, MIPI_PHY_CLKLP2HS_TIME, 10);
	}else {
		return -EINVAL;
	}
	return 0;
}

/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch from high speed to low power
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles
 * @return error code
 */
static int mipi_dsih_phy_datahs2lp_config(struct comipfb_info *fbi, uint8_t no_of_byte_cycles)
{
	if (no_of_byte_cycles < 0x100) {
		mipi_dsih_write_part(fbi, MIPI_PHY_TMR_CFG,no_of_byte_cycles,MIPI_PHY_HS2LP_TIME, 8);
	}else {
		return -EINVAL;
	}
	return 0;
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to switch from to low power high speed
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles
 * @return error code
 */
static int mipi_dsih_phy_datalp2hs_config(struct comipfb_info *fbi, uint8_t no_of_byte_cycles)
{
	if (no_of_byte_cycles < 0x100) {
		mipi_dsih_write_part(fbi, MIPI_PHY_TMR_CFG,no_of_byte_cycles, MIPI_PHY_LP2HS_TIME, 8);
	}else {
		return -EINVAL; 
	}
	return 0;
}
/**
 * Configure how many cycles of byte clock would the PHY module take
 * to turn the bus around to start receiving
 * @param instance pointer to structure holding the DSI Host core information
 * @param no_of_byte_cycles
 * @return error code
 */
static int mipi_dsih_phy_max_rd_time_config(struct comipfb_info *fbi, uint16_t no_of_byte_cycles)
{
	if (no_of_byte_cycles < 0x8000) {
		mipi_dsih_write_part(fbi, MIPI_PHY_TMR_CFG, no_of_byte_cycles, MIPI_MAX_RD_TIME, 15);
	}else {
		return -EINVAL;
	}
	return 0;
}

static void mipi_dsih_phy_lphs_config(struct comipfb_info *fbi)
{
        struct comipfb_dev_timing_mipi *mipi;
        unsigned int hs_freq;
        unsigned short clk_lphs = 0;
	unsigned short clk_hslp = 0;
	unsigned short data_lphs = 0;
	unsigned short data_hslp = 0;
	unsigned short max_rd_time;

        mipi = &(fbi->cdev->timing.mipi);
        hs_freq = mipi->hs_freq * 8;

#if defined(CONFIG_CPU_LC1813)
        if (COMPARE(hs_freq,90,99)) {
                clk_lphs = 35;
                clk_hslp = 23;
                data_lphs = 29;
                data_hslp = 16;
        }else if (COMPARE(hs_freq,99,108)) {
                clk_lphs = 38;
                clk_hslp = 26;
                data_lphs = 31;
                data_hslp = 17;
        }else if (COMPARE(hs_freq,108,123)) {
                clk_lphs = 35;
                clk_hslp = 25;
                data_lphs = 29;
                data_hslp = 16;
        }else if (COMPARE(hs_freq,123,135)) {
                clk_lphs = 34;
                clk_hslp = 23;
                data_lphs = 27;
                data_hslp = 16;
        }else if (COMPARE(hs_freq,135,150)) {
                clk_lphs = 36;
                clk_hslp = 25;
                data_lphs = 29;
                data_hslp = 17;
        }else if (COMPARE(hs_freq,150,159)) {
                clk_lphs = 36;
                clk_hslp = 24;
                data_lphs = 29;
                data_hslp = 17;
        }else if (COMPARE(hs_freq,159,180)) {
                clk_lphs = 35;
                clk_hslp = 23;
                data_lphs = 30;
                data_hslp = 16;
        }else if (COMPARE(hs_freq,180,198)) {
                clk_lphs = 39;
                clk_hslp = 26;
                data_lphs = 33;
                data_hslp = 18;
        }else if (COMPARE(hs_freq,198,210)) {
                clk_lphs = 43;
                clk_hslp = 25;
                data_lphs = 36;
                data_hslp = 18;
        }else if (COMPARE(hs_freq,210,240)) {
                clk_lphs = 43;
                clk_hslp = 25;
                data_lphs = 36;
                data_hslp = 18;
        }else if (COMPARE(hs_freq,240,249)) {
                clk_lphs = 47;
                clk_hslp = 27;
                data_lphs = 39;
                data_hslp = 19;
        }else if (COMPARE(hs_freq,249,270)) {
                clk_lphs = 51;
                clk_hslp = 27;
                data_lphs = 41;
                data_hslp = 20;
        }else if (COMPARE(hs_freq,270,300)) {
                clk_lphs = 51;
                clk_hslp = 27;
                data_lphs = 41;
                data_hslp = 20;
        }else if (COMPARE(hs_freq,300,330)) {
                clk_lphs = 53;
                clk_hslp = 30;
                data_lphs = 44;
                data_hslp = 21;
        }else if (COMPARE(hs_freq,330,360)) {
                clk_lphs = 59;
                clk_hslp = 31;
                data_lphs = 48;
                data_hslp = 21;
        }else if (COMPARE(hs_freq,360,399)) {
                clk_lphs = 62;
                clk_hslp = 31;
                data_lphs = 51;
                data_hslp = 22;
        }else if (COMPARE(hs_freq,399,450)) {
                clk_lphs = 64;
                clk_hslp = 33;
                data_lphs = 53;
                data_hslp = 23;
        }else if (COMPARE(hs_freq,450,486)) {
                clk_lphs = 70;
                clk_hslp = 34;
                data_lphs = 58;
                data_hslp = 24;
        }else if (COMPARE(hs_freq,486,549)) {
                clk_lphs = 76;
                clk_hslp = 34;
                data_lphs = 62;
                data_hslp = 25;
        }else if (COMPARE(hs_freq,549,600)) {
                clk_lphs = 82;
                clk_hslp = 39;
                data_lphs = 66;
                data_hslp = 27;
        }else if (COMPARE(hs_freq,600,648)) {
                clk_lphs = 86;
                clk_hslp = 40;
                data_lphs = 71;
                data_hslp = 28;
        }else if (COMPARE(hs_freq,648,699)) {
                clk_lphs = 93;
                clk_hslp = 41;
                data_lphs = 76;
                data_hslp = 30;
        }else if (COMPARE(hs_freq,699,750)) {
                clk_lphs = 98;
                clk_hslp = 43;
                data_lphs = 80;
                data_hslp = 31;
        }else if (COMPARE(hs_freq,750,783)) {
                clk_lphs = 105;
                clk_hslp = 43;
                data_lphs = 87;
                data_hslp = 31;
        }else if (COMPARE(hs_freq,783,849)) {
                clk_lphs = 109;
                clk_hslp = 45;
                data_lphs = 90;
                data_hslp = 33;
        }else if (COMPARE(hs_freq,849,900)) {
                clk_lphs = 116;
                clk_hslp = 47;
                data_lphs = 96;
                data_hslp = 34;
        }else if (COMPARE(hs_freq,900,972)) {
                clk_lphs = 121;
                clk_hslp = 50;
                data_lphs = 101;
                data_hslp = 35;
        }else if (COMPARE(hs_freq,972,999)) {
                clk_lphs = 127;
                clk_hslp = 50;
                data_lphs = 105;
                data_hslp = 37;
        }else if (hs_freq >= 999000) {
                clk_lphs = 133;
                clk_hslp = 52;
                data_lphs = 110;
                data_hslp = 38;
        }
#elif defined(CONFIG_CPU_LC1860)
	if (COMPARE(hs_freq,80,90)) {
                clk_lphs = 32;
                clk_hslp = 20;
                data_lphs = 26;
                data_hslp = 13;
	}else if (COMPARE(hs_freq,90,100)) {
                clk_lphs = 35;
                clk_hslp = 23;
                data_lphs = 28;
                data_hslp = 14;
        }else if (COMPARE(hs_freq,100,110)) {
                clk_lphs = 32;
                clk_hslp = 22;
                data_lphs = 26;
                data_hslp = 13;
        }else if (COMPARE(hs_freq,110,130)) {
                clk_lphs = 31;
                clk_hslp = 20;
                data_lphs = 27;
                data_hslp = 13;
        }else if (COMPARE(hs_freq,130,140)) {
                clk_lphs = 33;
                clk_hslp = 22;
                data_lphs = 26;
                data_hslp = 14;
        }else if (COMPARE(hs_freq,140,150)) {
                clk_lphs = 33;
                clk_hslp = 21;
                data_lphs = 26;
                data_hslp = 14;
        }else if (COMPARE(hs_freq,150,170)) {
                clk_lphs = 32;
                clk_hslp = 20;
                data_lphs = 27;
                data_hslp = 13;
        }else if (COMPARE(hs_freq,170,180)) {
                clk_lphs = 36;
                clk_hslp = 23;
                data_lphs = 30;
                data_hslp = 15;
        }else if (COMPARE(hs_freq,180,200)) {
                clk_lphs = 40;
                clk_hslp = 22;
                data_lphs = 33;
                data_hslp = 15;
        }else if (COMPARE(hs_freq,200,220)) {
                clk_lphs = 40;
                clk_hslp = 22;
                data_lphs = 33;
                data_hslp = 15;
        }else if (COMPARE(hs_freq,220,240)) {
                clk_lphs = 44;
                clk_hslp = 24;
                data_lphs = 36;
                data_hslp = 16;
        }else if (COMPARE(hs_freq,240,250)) {
                clk_lphs = 48;
                clk_hslp = 24;
                data_lphs = 38;
                data_hslp = 17;
        }else if (COMPARE(hs_freq,250,270)) {
                clk_lphs = 48;
                clk_hslp = 24;
                data_lphs = 38;
                data_hslp = 17;
        }else if (COMPARE(hs_freq,270,300)) {
                clk_lphs = 50;
                clk_hslp = 27;
                data_lphs = 41;
                data_hslp = 18;
        }else if (COMPARE(hs_freq,300,330)) {
                clk_lphs = 56;
                clk_hslp = 28;
                data_lphs = 45;
                data_hslp = 18;
        }else if (COMPARE(hs_freq,330,360)) {
                clk_lphs = 59;
                clk_hslp = 28;
                data_lphs = 48;
                data_hslp = 19;
        }else if (COMPARE(hs_freq,360,400)) {
                clk_lphs = 61;
                clk_hslp = 30;
                data_lphs = 50;
                data_hslp = 20;
        }else if (COMPARE(hs_freq,400,450)) {
                clk_lphs = 67;
                clk_hslp = 31;
                data_lphs = 55;
                data_hslp = 21;
        }else if (COMPARE(hs_freq,450,500)) {
                clk_lphs = 73;
                clk_hslp = 31;
                data_lphs = 59;
                data_hslp = 22;
        }else if (COMPARE(hs_freq,500,550)) {
                clk_lphs = 79;
                clk_hslp = 36;
                data_lphs = 63;
                data_hslp = 24;
        }else if (COMPARE(hs_freq,550,600)) {
                clk_lphs = 83;
                clk_hslp = 37;
                data_lphs = 68;
                data_hslp = 25;
        }else if (COMPARE(hs_freq,600,650)) {
                clk_lphs = 90;
                clk_hslp = 38;
                data_lphs = 73;
                data_hslp = 27;
        }else if (COMPARE(hs_freq,650,700)) {
                clk_lphs = 95;
                clk_hslp = 40;
                data_lphs = 77;
                data_hslp = 28;
        }else if (COMPARE(hs_freq,700,750)) {
                clk_lphs = 102;
                clk_hslp = 40;
                data_lphs = 84;
                data_hslp = 28;
        }else if (COMPARE(hs_freq,750,800)) {
                clk_lphs = 106;
                clk_hslp = 42;
                data_lphs = 87;
                data_hslp = 30;
        }else if (COMPARE(hs_freq,800,850)) {
                clk_lphs = 113;
                clk_hslp = 44;
                data_lphs = 93;
                data_hslp = 31;
        }else if (COMPARE(hs_freq,850,900)) {
                clk_lphs = 118;
                clk_hslp = 47;
                data_lphs = 98;
                data_hslp = 32;
        }else if (COMPARE(hs_freq,900,950)) {
                clk_lphs = 124;
                clk_hslp = 47;
                data_lphs = 102;
                data_hslp = 34;
        }else if (COMPARE(hs_freq,950,1000)) {
                clk_lphs = 130;
                clk_hslp = 49;
                data_lphs = 107;
                data_hslp = 35;
        }else if (COMPARE(hs_freq,1000,1050)) {
                clk_lphs = 135;
                clk_hslp = 51;
                data_lphs = 111;
                data_hslp = 37;
        }else if(COMPARE(hs_freq,1050,1100)) {
                clk_lphs = 139;
                clk_hslp = 51;
                data_lphs = 114;
                data_hslp = 38;
	}else if(COMPARE(hs_freq,1100,1150)) {
                clk_lphs = 146;
                clk_hslp = 54;
                data_lphs = 120;
                data_hslp = 40;
	}else if(COMPARE(hs_freq,1150,1200)) {
                clk_lphs = 153;
                clk_hslp = 57;
                data_lphs = 125;
                data_hslp = 41;
	}else if(COMPARE(hs_freq,1200,1250)) {
                clk_lphs = 158;
                clk_hslp = 58;
                data_lphs = 130;
                data_hslp = 42;
	}else if(COMPARE(hs_freq,1250,1300)) {
                clk_lphs = 163;
                clk_hslp = 58;
                data_lphs = 135;
                data_hslp = 44;
	}else if(COMPARE(hs_freq,1300,1350)) {
                clk_lphs = 168;
                clk_hslp = 60;
                data_lphs = 140;
                data_hslp = 45;
	}else if(COMPARE(hs_freq,1350,1400)) {
                clk_lphs = 172;
                clk_hslp = 64;
                data_lphs = 144;
                data_hslp = 47;
	}else if(COMPARE(hs_freq,1400,1450)) {
                clk_lphs = 176;
                clk_hslp = 65;
                data_lphs = 148;
                data_hslp = 48;
	} else if(hs_freq >= 1450) {
                clk_lphs = 181;
                clk_hslp = 66;
                data_lphs = 153;
                data_hslp = 50;
	}
#endif
        mipi_dsih_phy_clklp2hs_config(fbi, clk_lphs);
        mipi_dsih_phy_clkhs2lp_config(fbi, clk_hslp);
        mipi_dsih_phy_datahs2lp_config(fbi, data_hslp);
        mipi_dsih_phy_datalp2hs_config(fbi, data_lphs);

	/* FIX ME, set to max to force send lp read command in the last line of a frame,
	* instead of the region of vbp & vfp, to avoid flicker
	*/
        //max_rd_time = (unsigned short)(data_hslp + data_lphs + mipi->ext_info.dev_read_time);
        max_rd_time = 0x7fff;
        mipi_dsih_phy_max_rd_time_config(fbi, max_rd_time);
}

/**
 * Enable clock lane module
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param en
 */
static void mipi_dsih_dphy_clock_en(struct comipfb_info *fbi, int en)
{
        mipi_dsih_write_part(fbi, MIPI_PHY_RSTZ, en, MIPI_PHY_ENABLE, 1);
}

/**
 * Configure minimum wait period for HS transmission request after a stop state
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param no_of_byte_cycles [in byte (lane) clock cycles]
 */
static void mipi_dsih_dphy_stop_wait_time(struct comipfb_info *fbi, uint8_t no_of_byte_cycles)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CFG, no_of_byte_cycles, MIPI_PHY_STOP_WAIT_TIME, 8);
}
/**
 * Set number of active lanes
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param no_of_lanes
 */
static void mipi_dsih_dphy_no_of_lanes(struct comipfb_info *fbi, uint8_t no_of_lanes)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_IF_CFG, no_of_lanes - 1, MIPI_PHY_N_LANES, 2);
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

	mipi_dsih_write_part(fbi, MIPI_LPCLK_CTRL, enable, MIPI_PHY_TXREQUESTCLKHS, 1);
}

/**
 * Reset D-PHY module
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param reset
 */
void mipi_dsih_dphy_reset(struct comipfb_info *fbi, int reset)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_RSTZ, reset, MIPI_PHY_RESET, 1);
}
/**
 * Power up/down D-PHY module
 * @param instance pointer to structure which holds information about the d-phy
 * module
 * @param powerup (1) shutdown (0)
 */
void mipi_dsih_dphy_shutdown(struct comipfb_info *fbi, int powerup)
{
	mipi_dsih_write_part(fbi, MIPI_PHY_RSTZ, powerup, MIPI_PHY_SHUTDOWN, 1);
}
/**
 * Config ulps mode  
 * @param instance pointer to structure holding the DSI Host core information
 * @param enable bit
 */
void mipi_dsih_dphy_ulps_en(struct comipfb_info *fbi, int enable)
{
	if (enable) {
		mipi_dsih_write_word(fbi, MIPI_PHY_ULPS_CTRL, 0x05);
		mdelay(3);
	}else {
		mipi_dsih_write_word(fbi, MIPI_PHY_ULPS_CTRL, 0x0A);
		mdelay(3);
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
static int mipi_dsih_dphy_configure(struct comipfb_info *fbi)
{
	unsigned int loop_divider = 0;		// (M)
	unsigned int input_divider = 1;		// (N)
	unsigned char data[4];				// maximum data for now are 4 bytes per test mode
	unsigned char no_of_bytes = 0;
	unsigned char i = 0;				//iterator
	unsigned char range = 0;			//ranges iterator
	int flag = 0;
	unsigned int hs_freq;				//data lane hs freq (bits)
	struct comipfb_dev_timing_mipi *mipi;
	struct comipfb_platform_data *pdata;

	mipi = &(fbi->cdev->timing.mipi);
	pdata = fbi->pdata;
	hs_freq = mipi->hs_freq * 8;

	if (hs_freq < MIN_OUTPUT_FREQ) {
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
	
	/* find M and N dividers;  here the >= DPHY_DIV_LOWER_LIMIT is a phy constraint, formula should be above 1 MHz */
	for (input_divider = (1 + (pdata->phy_ref_freq / DPHY_DIV_UPPER_LIMIT));
		((pdata->phy_ref_freq / input_divider) >= DPHY_DIV_LOWER_LIMIT) && (!flag); input_divider++) {
		if (((hs_freq * input_divider) % (pdata->phy_ref_freq)) == 0) {
			loop_divider = ((hs_freq * input_divider) / (pdata->phy_ref_freq));	//values found
//#if defined(CONFIG_CPU_LC1813)
			if (loop_divider >= 12) {
				flag = 1;
			}
//#elif defined(CONFIG_CPU_LC1860)
//			if ((loop_divider % 2) == 0) {
//				flag = 1;
//			}
//#endif
		}
	}
	/*FIXME, formula below should be fixed to find a even num for loop_divider*/
	if ((!flag) || ((pdata->phy_ref_freq / input_divider) < DPHY_DIV_LOWER_LIMIT)) {	//value not found
		loop_divider = hs_freq / DPHY_DIV_LOWER_LIMIT;
		input_divider = pdata->phy_ref_freq / DPHY_DIV_LOWER_LIMIT;
	}
	else {
		input_divider--;
	}
	
	for (i = 0; (i < (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) && (loop_divider > loop_bandwidth[i].loop_div); i++);
	if (i >= (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) {
		return -EINVAL;
	}
	
	/* find ranges */
	for (range = 0; (range < (sizeof(ranges)/sizeof(ranges[0]))) && ((hs_freq / 1000) > ranges[range].freq); range++);
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

	mipi_dsih_phy_timing_config(fbi);
	
	mipi_dsih_phy_lphs_config(fbi);
	mipi_dsih_dphy_stop_wait_time(fbi, 0x0A); 	//old  0x1C

	mipi_dsih_dphy_no_of_lanes(fbi, mipi->no_lanes);
	if (mipi->auto_stop_clklane_en > 0)
		mipi_dsih_phy_hs_enter_lp(fbi, 1);
	mipi_dsih_dphy_clock_en(fbi, 1);
	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);

	return 0;
}

/**********************Config eDPI video mode or command mode*****************************/
/**
 * Configure DPI video interface
 * - If not in burst mode, it will compute the video and null packet sizes
 * according to necessity
 * @param instance pointer to structure holding the DSI Host core information
 * @param video_params pointer to video stream-to-send information
 * @return error code
 */

static int mipi_dsih_base_config(struct comipfb_info *fbi, unsigned int *bytes, unsigned int *step, unsigned int *video_size)
{
	int ret = 0;
	unsigned char val;
	struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);
	*video_size = fbi->display_info.xres;
	
	/*disable error report*/
	mipi_dsih_hal_error_mask_0(fbi, 0xffffff);
	mipi_dsih_hal_error_mask_1(fbi, 0xffffff);

	/* TX_ESC_CLOCK_DIV must be less than 20000KHz */
	if (!mipi->lp_freq)
		mipi->lp_freq = 10000;
	if ((mipi->hs_freq % mipi->lp_freq) != 0)
		val = mipi->hs_freq / mipi->lp_freq + 1;
	else
		val = mipi->hs_freq / mipi->lp_freq;
	printk(KERN_INFO " mipi lp_freq div val is %d\n", val);
	mipi_dsih_hal_tx_escape_division(fbi, val);
	mipi->lp_freq = mipi->hs_freq / val;
	printk(KERN_INFO " mipi lp_freq val  is %d KHz\n", mipi->lp_freq);

	/*config extern info*/
	if (mipi->ext_info.crc_rx_en > 0)
		mipi_dsih_hal_gen_crc_rx_en(fbi, 1);
	else 
		mipi_dsih_hal_gen_crc_rx_en(fbi, 0);
	
	if (mipi->ext_info.ecc_rx_en > 0)
		mipi_dsih_hal_gen_ecc_rx_en(fbi, 1);
	else
		mipi_dsih_hal_gen_ecc_rx_en(fbi, 0);

	if (mipi->ext_info.eotp_rx_en > 0)
		mipi_dsih_hal_gen_eotp_rx_en(fbi, 1);
	else
		mipi_dsih_hal_gen_eotp_rx_en(fbi, 0);
	
	if (mipi->ext_info.eotp_tx_en > 0)
		mipi_dsih_hal_gen_eotp_tx_en(fbi, 1);
	else
		mipi_dsih_hal_gen_eotp_tx_en(fbi, 0);
	
	if (fbi->display_mode == MIPI_COMMAND_MODE)
		mipi_dsih_hal_mode_config(fbi, 1);
	else
		mipi_dsih_hal_mode_config(fbi, 0);
	
	mipi_dsih_hal_dpi_video_vc(fbi, 0);
	mipi_dsih_hal_gen_vc(fbi, 0);
	
	/* get bytes per pixel and video size step (depending if loosely or not */
	switch (mipi->color_mode.color_bits) {
		case COLOR_CODE_16BIT_CONFIG1:
		case COLOR_CODE_16BIT_CONFIG2:
		case COLOR_CODE_16BIT_CONFIG3:
			*bytes = 200;
			*step = 1;
		break;
		case COLOR_CODE_18BIT_CONFIG1:
		case COLOR_CODE_18BIT_CONFIG2:
			mipi_dsih_hal_dpi_18_loosely_packet_en(fbi, mipi->color_mode.is_18bit_loosely);
			*bytes = 225;
			if (!mipi->color_mode.is_18bit_loosely) { 
				/* 18bits per pixel and NOT loosely, packets should be multiples of 4 */
				*step = 4;
				/* round up active H pixels to a multiple of 4 */
				for (; ((*video_size) % 4) != 0; (*video_size)++);
			}
			else {
				*step = 1;
			}
		break;
		case COLOR_CODE_24BIT:
			*bytes = 300;
			*step = 1;
		break;
		default:
		break;
	}
	ret = mipi_dsih_hal_dpi_color_coding(fbi, mipi->color_mode.color_bits);

	return ret;
}

int mipi_dsih_config(struct comipfb_info *fbi)
{
	unsigned int i = 0;
	unsigned int err_code = 0;
	unsigned int bytes_per_pixel_x100 = 0; /* bpp x 100 because it can be 2.25 */
	unsigned int video_size = 0;
	unsigned int ratio_clock_xPF = 0; /* holds dpi clock/byte clock times precision factor */
	unsigned int null_packet_size = 0;
	unsigned int video_size_step = 1;
	unsigned int hs_timeout = 0;
	unsigned int total_bytes = 0;
	unsigned int no_of_chunks = 0;
	unsigned int chunk_overhead = 0;
	unsigned int htotal = 0;
	unsigned int time_val = 0;
	struct comipfb_dev *cdev;
	struct comipfb_dev_timing_mipi *mipi;
	struct fb_videomode *videoinfo;
	
	cdev = fbi->cdev;
	mipi = &(cdev->timing.mipi);
	videoinfo = &(fbi->display_info);
	htotal = mipi->videomode_info.hsync + mipi->videomode_info.hbp + mipi->videomode_info.hfp + videoinfo->xres;
	ratio_clock_xPF = (mipi->hs_freq * PRECISION_FACTOR) / (fbi->pixclock / 1000);

	/* set up mipi host base config */	
	err_code = mipi_dsih_base_config(fbi, &bytes_per_pixel_x100, &video_size_step, &video_size); //TODO 18BIT need to modify
	if (err_code)
		return -EINVAL;
	
	/* set up phy pll to required lane clock */
	err_code = mipi_dsih_dphy_configure(fbi);
	if (err_code)
		return -EINVAL;

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_dpi_hsync_pol(fbi, 0);
		mipi_dsih_hal_dpi_vsync_pol(fbi, 0);
		mipi_dsih_hal_dpi_dataen_pol(fbi, 0);
		mipi_dsih_hal_dpi_color_mode_pol(fbi, 0);
		mipi_dsih_hal_dpi_shut_down_pol(fbi, 0);
		mipi_dsih_hal_video_enter_lp_config(fbi);
		if (mipi->videomode_info.lp_cmd_en > 0)
			mipi_dsih_hal_dpi_lp_cmd_en(fbi, 1);
		else
			mipi_dsih_hal_dpi_lp_cmd_en(fbi, 0);

		mipi_dsih_hal_dpi_frame_ack_en(fbi, mipi->videomode_info.frame_bta_ack);

		if (mipi->videomode_info.hsync * mipi->hs_freq % (fbi->pixclock / 1000) != 0)
			time_val = mipi->videomode_info.hsync * mipi->hs_freq / (fbi->pixclock / 1000) + 1;
		else
			time_val = mipi->videomode_info.hsync * mipi->hs_freq / (fbi->pixclock / 1000);
		mipi_dsih_hal_dpi_hsa(fbi, time_val);

		if (mipi->videomode_info.hbp * mipi->hs_freq % (fbi->pixclock / 1000) != 0)
			time_val = mipi->videomode_info.hbp * mipi->hs_freq / (fbi->pixclock / 1000) + 1;
		else
			time_val = mipi->videomode_info.hbp * mipi->hs_freq / (fbi->pixclock / 1000);
		mipi_dsih_hal_dpi_hbp(fbi, time_val);

		if (htotal * mipi->hs_freq % (fbi->pixclock / 1000) != 0)
			time_val = htotal * mipi->hs_freq / (fbi->pixclock / 1000) + 1;
		else
			time_val = htotal * mipi->hs_freq / (fbi->pixclock / 1000);
		mipi_dsih_hal_dpi_hline(fbi, time_val);
		
		mipi_dsih_hal_dpi_vsync(fbi, mipi->videomode_info.vsync);
		mipi_dsih_hal_dpi_vbp(fbi, mipi->videomode_info.vbp);
		mipi_dsih_hal_dpi_vfp(fbi, mipi->videomode_info.vfp);
		mipi_dsih_hal_dpi_vactive(fbi, videoinfo->yres);

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
		mipi_dsih_hal_max_rd_pkt_type(fbi, 1);

		mipi_dsih_hal_dpi_video_mode_type(fbi, mipi->videomode_info.mipi_trans_type);
		if (mipi->videomode_info.mipi_trans_type == VIDEO_BURST_WITH_SYNC_PULSES) {
			no_of_chunks = 1;
			null_packet_size = 0;
			hs_timeout = htotal + (DSIH_PIXEL_TOLERANCE * bytes_per_pixel_x100) / 100;
		}else {

#if 0
			/* non burst transmission */
			for (i = 1; i < 100; i++) {
				if ((mipi->hs_freq * mipi->no_lanes * i) % (fbi->pixclock / 1000) == 0)
					break;
			}
			init_chunk_overhead = i * bytes_per_pixel_x100 / 100;
			/* bytes to be sent - first as one chunk*/
			bytes_per_chunk = (bytes_per_pixel_x100 * videoinfo->xres) / 100 + init_chunk_overhead;
			/* the total bytes being sended through the DPI interface in number of xres clock pixel cycle */
			total_bytes = (ratio_clock_xPF * mipi->no_lanes * videoinfo->xres) / PRECISION_FACTOR;
			/* check if the in pixels actually fit on the DSI link */
			if (total_bytes >= bytes_per_chunk) {
				chunk_overhead = total_bytes - bytes_per_chunk;
				/* overhead higher than 1 -> enable multi packets */
				if (chunk_overhead > 1) {	
					/* MULTI packets */
					for (video_size = video_size_step; video_size < videoinfo->xres; video_size += video_size_step) {
						/* determine no of chunks */
						if ((((videoinfo->xres * PRECISION_FACTOR) / video_size) % PRECISION_FACTOR) == 0) {
							no_of_chunks = videoinfo->xres / video_size;
							bytes_per_chunk = (bytes_per_pixel_x100 * video_size) / 100 + init_chunk_overhead;
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
					for (video_size = videoinfo->xres; (video_size % video_size_step) != 0; video_size++);
				}
			}
#else
			for (i = 1; i < 1000; i++) {
				if ((mipi->hs_freq * mipi->no_lanes * i) % (fbi->pixclock / 1000) == 0) {
					total_bytes = (mipi->hs_freq * mipi->no_lanes * i) / (fbi->pixclock / 1000);
					if ((total_bytes - i * bytes_per_pixel_x100 / 100) >= 12)
						break;
				}
			}
			video_size = i;
			if ((videoinfo->xres % video_size) != 0)
				no_of_chunks = videoinfo->xres / video_size + 1;
			else
				no_of_chunks = videoinfo->xres / video_size;

			chunk_overhead = total_bytes - i * bytes_per_pixel_x100 / 100;
			null_packet_size = chunk_overhead - 12;
#endif
			hs_timeout = (htotal * videoinfo->yres) + (DSIH_PIXEL_TOLERANCE * bytes_per_pixel_x100) / 100;
		}
		#if defined(CONFIG_RGB_LVDS_ZA7783)
		mipi_dsih_hal_dpi_chunks_no(fbi, 0x1);
		mipi_dsih_hal_dpi_video_packet_size(fbi, videoinfo->xres);
		mipi_dsih_hal_dpi_null_packet_size(fbi, 0x0);
		#else 
		mipi_dsih_hal_dpi_chunks_no(fbi, no_of_chunks);
		mipi_dsih_hal_dpi_video_packet_size(fbi, video_size);
		mipi_dsih_hal_dpi_null_packet_size(fbi, null_packet_size);
		#endif
		mipi_dsih_hal_dpi_lp_cmd_time_config(fbi, video_size, (bytes_per_pixel_x100 / 100), null_packet_size);

	}else if (mipi->display_mode == MIPI_COMMAND_MODE) {
		mipi_dsih_hal_cmd_size_config(fbi, (bytes_per_pixel_x100 / 100));
		/* by default, all commands are sent in LP */
		mipi_dsih_hal_cmd_lp_mode_config(fbi);
		mipi_dsih_hal_cmd_tear_ack_en(fbi, mipi->commandmode_info.tear_fx_en);
		mipi_dsih_hal_cmd_pkt_ack_en(fbi, mipi->commandmode_info.ack_rqst_en);
		mipi_dsih_hal_cmd_timeout_config(fbi);
		hs_timeout = (htotal * videoinfo->yres) + (DSIH_PIXEL_TOLERANCE * bytes_per_pixel_x100) / 100;
	}
	
	if ((mipi->videomode_info.frame_bta_ack > 0)|| (mipi->commandmode_info.ack_rqst_en > 0) || (mipi->commandmode_info.tear_fx_en > 0))
		mipi_dsih_hal_bta_en(fbi, 1);
	else
		mipi_dsih_hal_bta_en(fbi, 0);

#if 0 
	for (counter = 0xFF; (counter < hs_timeout) && (counter > 2); counter--) {
		if (((hs_timeout % counter) == 0) || (counter <= 10)) {
			mipi_dsih_hal_timeout_clock_division(fbi, counter);
			mipi_dsih_hal_lp_rx_timeout(fbi, (uint16_t)(hs_timeout / counter));
			mipi_dsih_hal_hs_tx_timeout(fbi, (uint16_t)(hs_timeout / counter));
			break;
		}
	}
#endif
	/* Reset the DSI Host controller */
	mipi_dsih_hal_power(fbi, 0); 
	mipi_dsih_hal_power(fbi, 1);

	return err_code;
}

/* set up mipi host base config */
static int mipi_dsih_base_config_lw(struct comipfb_info *fbi)
{
	/*disable error report*/
	mipi_dsih_hal_error_mask_0(fbi, 0xffffff);
	mipi_dsih_hal_error_mask_1(fbi, 0xffffff);

	mipi_dsih_hal_tx_escape_division(fbi, 4);

	mipi_dsih_hal_gen_crc_rx_en(fbi, 0);
	mipi_dsih_hal_gen_ecc_rx_en(fbi, 0);
	mipi_dsih_hal_gen_eotp_rx_en(fbi, 0);
	mipi_dsih_hal_gen_eotp_tx_en(fbi, 0);

	mipi_dsih_hal_mode_config(fbi, 1);//COMMAND MODE

	mipi_dsih_hal_dpi_video_vc(fbi, 0);
	mipi_dsih_hal_gen_vc(fbi, 0);

	return 0;
}

/* set up phy pll to required lane clock */
static int mipi_dsih_dphy_configure_lw(struct comipfb_info *fbi)
{
	unsigned int loop_divider = 0;		// (M)
	unsigned int input_divider = 1;		// (N)
	unsigned char data[4];				// maximum data for now are 4 bytes per test mode
	unsigned char no_of_bytes = 0;
	unsigned char i = 0;				//iterator
	unsigned char range = 0;			//ranges iterator
	int flag = 0;
	unsigned int hs_freq = 65000 * 8;		//data lane hs freq (bits)
	unsigned int phy_ref_freq = 26000;

	unsigned short clk_lphs = 56;
	unsigned short clk_hslp = 28;
	unsigned short data_lphs = 45;
	unsigned short data_hslp = 18;
	unsigned short max_rd_time;

	if (hs_freq < MIN_OUTPUT_FREQ) {
		printk("<%s>invalid hs_freq\n", __func__);
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

	/* find M and N dividers;  here the >= DPHY_DIV_LOWER_LIMIT is a phy constraint, formula should be above 1 MHz */
	for (input_divider = (1 + (phy_ref_freq / DPHY_DIV_UPPER_LIMIT));
		((phy_ref_freq / input_divider) >= DPHY_DIV_LOWER_LIMIT) && (!flag); input_divider++) {
		if (((hs_freq * input_divider) % (phy_ref_freq)) == 0) {
			loop_divider = ((hs_freq * input_divider) / (phy_ref_freq));	//values found
#if defined(CONFIG_CPU_LC1813)
			if (loop_divider >= 12) {
				flag = 1;
			}
#elif defined(CONFIG_CPU_LC1860)
			if ((loop_divider % 2) == 0) {
				flag = 1;
			}
#endif
		}
	}
	/*FIXME, formula below should be fixed to find a even num for loop_divider*/
	if ((!flag) || ((phy_ref_freq / input_divider) < DPHY_DIV_LOWER_LIMIT)) {	//value not found
		loop_divider = hs_freq / DPHY_DIV_LOWER_LIMIT;
		input_divider = phy_ref_freq / DPHY_DIV_LOWER_LIMIT;
	}
	else {
		input_divider--;
	}

	for (i = 0; (i < (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) && (loop_divider > loop_bandwidth[i].loop_div); i++);
	if (i >= (sizeof(loop_bandwidth)/sizeof(loop_bandwidth[0]))) {
		return -EINVAL;
	}

	/* find ranges */
	for (range = 0; (range < (sizeof(ranges)/sizeof(ranges[0]))) && ((hs_freq / 1000) > ranges[range].freq); range++);
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

	//mipi_dsih_phy_timing_config(fbi);

	//mipi_dsih_phy_lphs_config(fbi);
        mipi_dsih_phy_clklp2hs_config(fbi, clk_lphs);
        mipi_dsih_phy_clkhs2lp_config(fbi, clk_hslp);
        mipi_dsih_phy_datahs2lp_config(fbi, data_hslp);
        mipi_dsih_phy_datalp2hs_config(fbi, data_lphs);

	max_rd_time = (unsigned short)(data_hslp + data_lphs);
	mipi_dsih_phy_max_rd_time_config(fbi, max_rd_time);

	mipi_dsih_dphy_stop_wait_time(fbi, 0x0A); 	//old  0x1C

	mipi_dsih_dphy_no_of_lanes(fbi, 1);

	mipi_dsih_dphy_clock_en(fbi, 1);
	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);

	return 0;
}

int mipi_dsih_config_lw(struct comipfb_info *fbi)
{
	int err_code = 0;


	/* set up mipi host base config */
	err_code = mipi_dsih_base_config_lw(fbi);
	if (err_code)
		return -EINVAL;

	/* set up phy pll to required lane clock */
	err_code = mipi_dsih_dphy_configure_lw(fbi);
	if (err_code)
		return -EINVAL;
#if 0
/*if in video mode ,config below*/
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
	mipi_dsih_hal_max_rd_pkt_type(fbi, 1);
#endif

#if 1
/*if in command mode, config below*/
	/* by default, all commands are sent in LP */
	//from mipi_dsih_hal_cmd_lp_mode_config
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
	mipi_dsih_hal_max_rd_pkt_type(fbi, 1);

	/*timeout config*/
	//from mipi_dsih_hal_cmd_timeout_config
	mipi_dsih_write_word(fbi, MIPI_HS_RD_TO_CNT, 0);
	mipi_dsih_write_word(fbi, MIPI_LP_RD_TO_CNT, 0);
	mipi_dsih_write_word(fbi, MIPI_HS_WR_TO_CNT, 20);
	mipi_dsih_write_word(fbi, MIPI_LP_WR_TO_CNT, 20);
	mipi_dsih_write_word(fbi, MIPI_BTA_TO_CNT, 100);

	/*FIXME, it's a risk not to enable BTA*/
	//mipi_dsih_hal_bta_en(fbi, 1);

#endif

	/* Reset the DSI Host controller */
	mipi_dsih_hal_power(fbi, 0);
	mipi_dsih_hal_power(fbi, 1);
	mdelay(20);

	return 0;

}

EXPORT_SYMBOL(mipi_dsih_hal_mode_config);
EXPORT_SYMBOL(mipi_dsih_config);
EXPORT_SYMBOL(mipi_dsih_hal_bta_en);
EXPORT_SYMBOL(mipi_dsih_hal_power);
EXPORT_SYMBOL(mipi_dsih_dphy_reset);
EXPORT_SYMBOL(mipi_dsih_dphy_shutdown);
EXPORT_SYMBOL(mipi_dsih_dphy_enable_hs_clk);
EXPORT_SYMBOL(mipi_dsih_dphy_ulps_en);
EXPORT_SYMBOL(mipi_dsih_hal_dcs_wr_tx_type);
EXPORT_SYMBOL(mipi_dsih_dphy_get_status);
EXPORT_SYMBOL(mipi_dsih_config_lw);

