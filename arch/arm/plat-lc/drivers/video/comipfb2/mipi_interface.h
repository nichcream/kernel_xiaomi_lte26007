#ifndef __MIPI_INTERFACE_H__
#define __MIPI_INTERFACE_H__
#include <linux/errno.h>
#include <linux/delay.h>
#include "comipfb.h"

#define MIPI_TIME_X1000000 1000000

#define DSIH_PIXEL_TOLERANCE	2
#define DSIH_FIFO_ACTIVE_WAIT	2000	/*20us * 2000 = 40ms, cross two frame time*/
#define PRECISION_FACTOR 		1000
#define NULL_PACKET_OVERHEAD 	6
#define MAX_NULL_SIZE			1023
#define MAX_RD_COUNT			1000	/*prevent dead loop*/

#if defined(CONFIG_CPU_LC1813)
/* Reference clock frequency divided by Input Frequency Division Ratio LIMITS */
#define DPHY_DIV_UPPER_LIMIT	800000
#define DPHY_DIV_LOWER_LIMIT	1000
#define MIN_OUTPUT_FREQ			80
#elif defined(CONFIG_CPU_LC1860)
/* Reference clock frequency divided by Input Frequency Division Ratio LIMITS */
#define DPHY_DIV_UPPER_LIMIT	40000
#define DPHY_DIV_LOWER_LIMIT	5000
#define MIN_OUTPUT_FREQ			80
#endif
/**
 * Color coding type (depth and pixel configuration)
 */
typedef enum {
	COLOR_CODE_16BIT_CONFIG1 = 0,	//PACKET RGB565
	COLOR_CODE_16BIT_CONFIG2,		//UNPACKET RGB565
	COLOR_CODE_16BIT_CONFIG3,		//UNPACKET RGB565
	COLOR_CODE_18BIT_CONFIG1,		//PACKET RGB666
	COLOR_CODE_18BIT_CONFIG2,		//UNPACKET RGB666
	COLOR_CODE_24BIT,				//PACKET RGB888
	COLOR_CODE_MAX,
}dsih_color_coding_t;

extern int mipi_dsih_config(struct comipfb_info *fbi);
extern void mipi_dsih_hal_bta_en(struct comipfb_info *fbi, int enable);
extern void mipi_dsih_hal_power(struct comipfb_info *fbi, int on);
extern void mipi_dsih_dphy_enable_hs_clk(struct comipfb_info *fbi, int enable);
extern void mipi_dsih_dphy_shutdown(struct comipfb_info *fbi, int powerup);
extern void mipi_dsih_dphy_reset(struct comipfb_info *fbi, int reset);
extern void mipi_dsih_dphy_ulps_en(struct comipfb_info *fbi, int enable);

extern void mipi_dsih_write_word(struct comipfb_info *fbi, uint32_t reg_address, uint32_t data);
extern unsigned int mipi_dsih_read_word(struct comipfb_info *fbi, uint32_t reg_address);
extern unsigned int mipi_dsih_read_part(struct comipfb_info *fbi, uint32_t reg_address, uint8_t shift, uint8_t width);
extern void mipi_dsih_write_part(struct comipfb_info *fbi, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width);
extern void mipi_dsih_hal_mode_config(struct comipfb_info *fbi, uint32_t val);
extern int mipi_dsih_hal_dcs_wr_tx_type(struct comipfb_info *fbi, unsigned no_of_param, int lp);
extern unsigned int mipi_dsih_dphy_get_status(struct comipfb_info *fbi);
extern int mipi_dsih_config_lw(struct comipfb_info *fbi);

#endif
