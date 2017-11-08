#ifndef __COMIPFB_DEV_H_
#define __COMIPFB_DEV_H_

#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "mipi_interface.h"

#define ROW_LINE 60

#define COMIPFB_MPU_IF			(0x00000001)
#define COMIPFB_RGB_IF			(0x00000002)
#define COMIPFB_MIPI_IF			(0x00000003)
#define COMIPFB_HDMI_IF			(0x00000004)

#define COMIPFB_PCLK_HIGH_ACT		(0x00000002)
#define COMIPFB_HSYNC_HIGH_ACT		(0x00000004)
#define COMIPFB_VSYNC_HIGH_ACT		(0x00000008)
#define COMIPFB_INS_HIGH		(0x00000010)
#define COMIPFB_BUS_WIDTH_16BIT		(0x00000020)

enum {
	CMD_CMD = 0,
	CMD_CMD2,
	CMD_PARA,
	CMD_DELAY,
};

enum {
	DCS_CMD = 2,
	GEN_CMD,
	SW_PACK0,
	SW_PACK1,
	SW_PACK2,
	LW_PACK,
};

#define DELAY(x)			((CMD_DELAY << 24) | (x))
#define PARA(x)				((CMD_PARA << 24) | (x))
#define CMD2(x, y)			((CMD_CMD2 << 24) | (x << 8) | (y))
#define CMD(x)				((CMD_CMD << 24) | (x))

#define ARRAY_AND_SIZE(x)		(u8 *)(x), ARRAY_SIZE(x)

#ifdef CONFIG_FBCON_DRAW_PANIC_TEXT
extern int kpanic_in_progress;
#endif

struct comipfb_dev_cmds {
	u8 *cmds;
	u16 n_pack;
	u16 n_cmds;
};

struct bl_cmds {
	struct comipfb_dev_cmds bl_cmd;
	unsigned int brightness_bit;
};

struct comipfb_dev_timing_rgb {
	u_int hsync;	/* Horizontal Synchronization, unit : pclk. */
	u_int hbp;	/* Horizontal Back Porch, unit : pclk. */
	u_int hfp;	/* Horizontal Front Porch, unit : pclk. */
	u_int vsync;	/* Vertical Synchronization, unit : line. */
	u_int vbp;	/* Vertical Back Porch, unit : line. */
	u_int vfp;	/* Vertical Front Porch, unit : line. */
};

struct comipfb_dev_timing_mpu {
	u_int as;	/* Address setup time, ns. */
	u_int ah;	/* Address hold time, ns. */
	u_int cs;	/* Chip select setup time, ns. */
	u_int csh;	/* Chip select hold time, ns. */
	u_int wc;	/* Write cycle, ns. */
	u_int rc;	/* Read cycle, ns. */
	u_int wrl;	/* Write control pulse L duration, ns. */
	u_int rdl;	/* Read control pulse L duration, ns. */
};

struct comipfb_dev_timing_mipi {
	u_int reference_freq; /*PHY reference frq, default  26000 KHZ*/
	u_int output_freq; /*PHY output freq, byte clock KHZ*/
	u_int no_lanes; /*lane numbers*/
	u_int lcdc_mipi_if_mode; /*color bits*/
	u_int hsync;	/* Horizontal Synchronization, unit : pclk. */
	u_int hbp;	/* Horizontal Back Porch, unit : pclk. */
	u_int hfp;	/* Horizontal Front Porch, unit : pclk. */
	u_int vsync;	/* Vertical Synchronization, unit : line. */
	u_int vbp;	/* Vertical Back Porch, unit : line. */
	u_int vfp;	/* Vertical Front Porch, unit : line. */
};

struct comipfb_dev {
	const char* name;	/* Device name. */
	int control_id; //controller and screen config id
	int screen_num; //screen number in lcdc, diff from other dev on the same controllor
	int interface_info;//interface infomation
	int default_use; //in init, default config
	int cmdline;
	u_int bpp;		/* Bits per pixel. */
	u_int xres;		/* Device resolution. */
	u_int yres;
	u_int width;		/* Width of device in mm. */
	u_int height;		/* Height of device in mm. */
	u_int refresh;		/* Refresh frames per second. */
	u_int pclk;		/* Pixel clock in HZ. */
	u_int flags;		/* Device flags. */
	union {
		struct comipfb_dev_timing_rgb rgb;
		struct comipfb_dev_timing_mpu mpu;
		struct comipfb_dev_timing_mipi mipi;
	} timing;

	struct comipfb_dev_cmds cmds_init;
	struct comipfb_dev_cmds cmds_suspend;
	struct comipfb_dev_cmds cmds_resume;
	struct bl_cmds backlight_info;

	int (*reset)(struct comipfb_info *fbi);
	int (*suspend)(struct comipfb_info *fbi);
	int (*resume)(struct comipfb_info *fbi);
};

extern int comipfb_dev_register(struct comipfb_dev* dev);
extern int comipfb_dev_unregister(struct comipfb_dev* dev);
extern struct comipfb_dev* comipfb_dev_get(struct comipfb_info *fbi, int dev_flag, int init_flag);

#endif /*__COMIPFB_DEV_H_*/
