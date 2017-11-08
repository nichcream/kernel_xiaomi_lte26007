#ifndef __MIPI_INTERFACE_H__
#define __MIPI_INTERFACE_H__
#include <linux/errno.h>
#include <linux/delay.h>
#include "comipfb.h"

#define MIPI_LEVEL_LOW		0
#define MIPI_LEVEL_HIGHT	1

#define DSIH_PIXEL_TOLERANCE	2
#define DSIH_FIFO_ACTIVE_WAIT	5		//no of tries to access the fifo, old value is 200
#define PRECISION_FACTOR 	1000
#define VIDEO_PACKET_OVERHEAD 	6
#define NULL_PACKET_OVERHEAD 	6
#define MAX_NULL_SIZE		1023

/* Reference clock frequency divided by Input Frequency Division Ratio LIMITS */
#define DPHY_DIV_UPPER_LIMIT	800000
#define DPHY_DIV_LOWER_LIMIT	1000
#define MIN_OUTPUT_FREQ		80

/**
 * Video stream type
 */
typedef enum {
	VIDEO_NON_BURST_WITH_SYNC_PULSES = 0,
	VIDEO_NON_BURST_WITH_SYNC_EVENTS,
	VIDEO_BURST_WITH_SYNC_PULSES
}dsih_video_mode_t;
/**
 * Color coding type (depth and pixel configuration)
 */
typedef enum {
	COLOR_CODE_16BIT_CONFIG1 = 0,		//PACKET RGB565
	COLOR_CODE_16BIT_CONFIG2,		//UNPACKET RGB565
	COLOR_CODE_16BIT_CONFIG3,		//UNPACKET RGB565
	COLOR_CODE_18BIT_CONFIG1,		//PACKET RGB666
	COLOR_CODE_18BIT_CONFIG2,		//UNPACKET RGB666
	COLOR_CODE_24BIT			//PACKET RGB888
}dsih_color_coding_t;

struct mipi_timing {
	unsigned int hsa;			//Horizontal Synchronism active preiod in lane byte clock cycles	
	unsigned int hbp;			//Horizontal back porch preiod in lane byte clock cycles
	unsigned int hact;			//Horizontal synchronism len in lane byte clock cycles
	unsigned int hfp;			//Horizontal front porch preiod in lane byte clock cycles
	unsigned int htotal;			//Horizontal total
	unsigned int vsa;			//Vertical Synchronism active period measured in vertical lines
	unsigned int vbp;			//Vertical back proch period measured in vertical lines	
	unsigned int vfp;			//Vertical front proch period measured in vertical lines	
	unsigned int vactive;			//Vertical active period vertical lines
	unsigned char virtual_channel;		//Virtual channel number to send this video stream 
	int receive_ack_packets;		// Enable receiving of ack packets
	dsih_video_mode_t trans_mode;		//Video mode, whether burst with sync pulses, or packets with either sync pulses or events
	dsih_color_coding_t color_coding;	//mipi color mode
	int is_18_loosely;			//Is 18-bit loosely packets (valid only when BPP == 18)
	int data_en_polarity;			//Data enable signal (dpidaten) whether it is active high or low
	int h_polarity;				//Horizontal synchronisation signal (dpihsync) whether it is active high or low 
	int v_polarity;				//Vertical synchronisation signal (dpivsync) whether it is active high or low
	int color_mode_polarity;
	int shut_down_polarity;
	int max_hs_to_lp_cycles;
	int max_lp_to_hs_cycles;
	int max_bta_cycles;
};

extern uint32_t mipi_dsih_read_word(struct comipfb_info *fbi, uint32_t reg_address);
extern void mipi_dsih_write_word(struct comipfb_info *fbi, uint32_t reg_address, uint32_t data);
extern uint32_t mipi_dsih_read_part(struct comipfb_info *fbi, uint32_t reg_address, uint8_t shift, uint8_t width);
extern void mipi_dsih_write_part(struct comipfb_info *fbi, uint32_t reg_address, uint32_t data, uint8_t shift, uint8_t width);
extern int mipi_dsih_dpi_video(struct comipfb_info *fbi, struct mipi_timing *mipi_time);

extern void mipi_dsih_hal_gen_rd_vc(struct comipfb_info *fbi, uint8_t vc);
extern void mipi_dsih_hal_bta_en(struct comipfb_info *fbi, int enable);
extern void mipi_dsih_hal_dpi_video_mode_en(struct comipfb_info *fbi, int enable);
extern void mipi_dsih_hal_gen_cmd_mode_en(struct comipfb_info *fbi, int enable);
extern void mipi_dsih_hal_power(struct comipfb_info *fbi, int on);

extern void mipi_dsih_dphy_enable_hs_clk(struct comipfb_info *fbi, int enable);
extern int mipi_dsih_dphy_configure(struct comipfb_info *fbi);
extern void mipi_dsih_dphy_shutdown(struct comipfb_info *fbi, int powerup);
extern void mipi_dsih_dphy_reset(struct comipfb_info *fbi, int reset);
extern void mipi_dsih_dphy_enter_ulps(struct comipfb_info *fbi, int enable);
extern void mipi_dsih_dphy_exit_ulps(struct comipfb_info *fbi, int enable);
#endif
