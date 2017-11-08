
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <plat/comip-v4l2.h>
#include "ov5648_pip.h"
#include "pip_ov5648_ov7695.h"

//#include"./comip/comip-debugtool.h"
#define OV5648_CHIP_ID_H	(0x56)
#define OV5648_CHIP_ID_L	(0x48)


#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		2560
#define MAX_HEIGHT		1920

/*PIP */
#define PIP_PREVIEW_WIDTH	1280
#define PIP_PREVIEW_HEIGHT	720
#define PIP_MAX_WIDTH	2560
#define PIP_MAX_HEIGHT	1440

#define OV5648_REG_END		0xffff
#define OV5648_REG_DELAY	0xfffe


struct ov5648_format_struct;
struct ov5648_info {
	struct v4l2_subdev sd;
	struct ov5648_format_struct *fmt;
	struct ov5648_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int contrast;
	int white_balance;
	int frame_rate;
	int if_pip;
	int first_start;
	int max_width;
	int max_height;
	int dynamic_framerate;
	int binning;
};

struct otp_struct {
	unsigned int customer_id;
	unsigned int module_integrator_id;
	unsigned int lens_id;
	unsigned int rg_ratio;
	unsigned int bg_ratio;
	unsigned int user_data[5];
};

static struct regval_list ov5648_init_regs[] = {

	{0x0100, 0x00},  // software sleep
	{0x0103, 0x01},  // software reset
	{OV5648_REG_DELAY, 5},
	{0x3001, 0x00}, // D[7:0] set to input
	{0x3002, 0x00}, // D[11:8] set to input
	{0x3011, 0x02}, // Drive strength 2x
	{0x3018, 0x4c}, // MIPI 2 lane
	{0x3022, 0x00},
	{0x3034, 0x1a}, // 10-bit mode
 
	{0x3035, 0x21}, // PLL
	{0x3036, 0x56}, // PLL
	{0x3037, 0x02}, // PLL yl set
 
	{0x3038, 0x00}, // PLL
	{0x3039, 0x00}, // PLL
	{0x303a, 0x00}, // PLLS
	{0x303b, 0x19}, // PLLS
	{0x303c, 0x11}, // PLLS
	{0x303d, 0x30}, // PLLS
	{0x3105, 0x11},
	{0x3106, 0x05}, // PLL
	{0x3304, 0x28},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	{0x3500, 0x00}, // exposure [19:16]
	{0x3501, 0x3d}, // exposure [15:8]
	{0x3502, 0x00}, // exposure [7:0], exposure = 0x3d0 = 976
	{0x3503, 0x27}, // 07 gain has no delay, manual agc/aec
	{0x350a, 0x00}, // gain[9:8]
	{0x350b, 0x40}, // gain[7:0], gain = 4x
	{0x3601, 0x33}, // analog control
	{0x3602, 0x00}, // analog control
	{0x3611, 0x0e}, // analog control
	{0x3612, 0x2b}, // analog control
	{0x3614, 0x50}, // analog control
	{0x3620, 0x33}, // analog control
	{0x3622, 0x00}, // analog control
	{0x3630, 0xad}, // analog control
	{0x3631, 0x00}, // analog control
	{0x3632, 0x94}, // analog control
	{0x3633, 0x17}, // analog control
	{0x3634, 0x14}, // analog control
	{0x3705, 0x2a}, // analog control
	{0x3708, 0x66}, // analog control
	{0x3709, 0x52}, // analog control
	{0x370b, 0x23}, // analog control
	{0x370c, 0xc3}, // analog control
	{0x370d, 0x00}, // analog control
	{0x370e, 0x00}, // analog control
	{0x371c, 0x07}, // analog control
	{0x3739, 0xd2}, // analog control
	{0x373c, 0x00},
	{0x3800, 0x00}, // xstart = 0
	{0x3801, 0x00}, // xstart
	{0x3802, 0x00}, // ystart = 0
	{0x3803, 0x00}, // ystart
	{0x3804, 0x0a}, // xend = 2623
	{0x3805, 0x3f}, // yend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa3}, // yend
	{0x3808, 0x05}, // x output size = 1280
	{0x3809, 0x00}, // x output size
	{0x380a, 0x03}, // y output size = 960
	{0x380b, 0xc0}, // y output size
	{0x380c, 0x0b}, // hts = 2816
	{0x380d, 0x00}, // hts
	{0x380e, 0x03}, // vts = 992
	{0x380f, 0xe0}, // vts
	{0x3810, 0x00}, // isp x win = 8
	{0x3811, 0x08}, // isp x win
	{0x3812, 0x00}, // isp y win = 4
	{0x3813, 0x04}, // isp y win
	{0x3814, 0x31}, // x inc
	{0x3815, 0x31}, // y inc
	{0x3817, 0x00}, // hsync start
	{0x3820, 0x0e}, // flip off, v bin off
	{0x3821, 0x01}, // mirror on, h bin on
	{0x3826, 0x03},
	{0x3829, 0x00},
	{0x382b, 0x0b},
	{0x3830, 0x00},
	{0x3836, 0x00},
	{0x3837, 0x00},
	{0x3838, 0x00},
	{0x3839, 0x04},
	{0x383a, 0x00},
	{0x383b, 0x01},
	{0x3b00, 0x00}, // strobe off
	{0x3b02, 0x08}, // shutter delay
	{0x3b03, 0x00}, // shutter delay
	{0x3b04, 0x04}, // frex_exp
	{0x3b05, 0x00}, // frex_exp
	{0x3b06, 0x04},
	{0x3b07, 0x08}, // frex inv
	{0x3b08, 0x00}, // frex exp req
	{0x3b09, 0x02}, // frex end option
	{0x3b0a, 0x04}, // frex rst length
	{0x3b0b, 0x00}, // frex strobe width
	{0x3b0c, 0x3d}, // frex strobe width
	{0x3f01, 0x0d},
	{0x3f0f, 0xf5},
	{0x4000, 0x89}, // blc enable
	{0x4001, 0x02}, // blc start line
	{0x4002, 0x45}, // blc auto, reset frame number = 5
	{0x4004, 0x02}, // black line number
	{0x4005, 0x18}, // blc normal freeze
	{0x4006, 0x08},
	{0x4007, 0x10},
	{0x4008, 0x00},
	{0x4009, 0x10}, // blc black level target // add nick yl set
	{0x4300, 0xf8},
	{0x4303, 0xff},
	{0x4304, 0x00},
	{0x4307, 0xff},
	{0x4520, 0x00},
	{0x4521, 0x00},
	{0x4511, 0x22},
	{0x481f, 0x3c}, // MIPI clk prepare min
	{0x4826, 0x00}, // MIPI hs prepare min
	{0x4837, 0x18}, // MIPI global timing
	{0x4b00, 0x06},
	{0x4b01, 0x0a},
	{0x5000, 0xff}, // bpc on, wpc on
	{0x5001, 0x00}, // awb disable
	{0x5002, 0x41}, // win enable, awb gain enable
	{0x5003, 0x0a}, // buf en, bin auto en
	{0x5004, 0x00}, // size man off
	{0x5043, 0x00},
	{0x5013, 0x00},
	{0x501f, 0x03}, // ISP output data
	{0x503d, 0x00}, // test pattern off
	{0x5180, 0x08}, // manual wb gain on
	{0x5a00, 0x08},
	{0x5b00, 0x01},
	{0x5b01, 0x40},
	{0x5b02, 0x00},
	{0x5b03, 0xf0},
	{0x0100, 0x01}, // wake up from software sleep
	{0x350b, 0x80}, // gain = 8x
	{0x4050, 0x37}, // add nick yl set
	{0x4051, 0x8f}, // add nick yl set
	{0x4837, 0x17}, // MIPI global timing
	 
	{OV5648_REG_DELAY, 50},
	 
	// video setting
	{0x3201, 0x00}, // group 1 start addr 0
	{0x3208, 0x01}, // enter group 1 write mode
	// 1280x960 30fps 2 lane MIPI 420Mbps/lane
	{0x3708, 0x66},
	{0x3709, 0x52},
	{0x370c, 0xc3},
	{0x3800, 0x00}, // xstart = 0
	{0x3801, 0x00}, // x start
	{0x3802, 0x00}, // y start = 0
	{0x3803, 0x00}, // y start
	{0x3804, 0x0a}, // xend = 2623
	{0x3805, 0x3f}, // xend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa3}, // yend
	{0x3808, 0x05}, // x output size = 1280
	{0x3809, 0x00}, // x output size
	{0x380a, 0x03}, // y output size = 960
	{0x380b, 0xc0}, // y output size
	{0x380c, 0x0b}, // hts = 2816
	{0x380d, 0x00}, // hts
	#ifndef FIRMWARE_AUTO_FPS
	{0x380e, 0x03}, // vts = 992
	{0x380f, 0xe0}, // vts
	#endif
	{0x3810, 0x00}, // isp x win = 8
	{0x3811, 0x08}, // isp x win
	{0x3812, 0x00}, // isp y win = 4
	{0x3813, 0x04}, // isp y win
	{0x3814, 0x31}, // x inc
	{0x3815, 0x31}, // y inc
	{0x3817, 0x00}, // hsync start
	//{0x3820, 0x0e}, // flip off, v bin off
	//{0x3821, 0x01}, // mirror on, h bin on
	{0x3820, 0x08}, // flip off, v bin off
	{0x3821, 0x07}, // mirror on, h bin on
	{0x4004, 0x02}, // black line number
	{0x4005, 0x18}, // add nick yl set
	{0x3208, 0x11}, // EXIT group 1 write mode
	
	{OV5648_REG_DELAY, 5},
	//capture setting
	{0x3202, 0x08}, // group 2 start addr 0x80
	{0x3208, 0x02}, // enter group 2 write mode
	// 2592x1944 15fps 2 lane MIPI 420Mbps/lane
	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0xc0},
	{0x3800, 0x00}, // xstart = 0
	{0x3801, 0x00}, // xstart
	{0x3802, 0x00}, // ystart = 0
	{0x3803, 0x00}, // ystart
	{0x3804, 0x0a}, // xend = 2623
	{0x3805, 0x3f}, // xend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa3}, // yend
	{0x3808, 0x0a}, // x output size = 2592
	{0x3809, 0x00}, // x output size
	{0x380a, 0x07}, // y output size = 1944
	{0x380b, 0x80}, // y output size
	{0x380c, 0x0b}, // hts = 2816
	{0x380d, 0x00}, // hts
	#ifndef FIRMWARE_AUTO_FPS
	{0x380e, 0x07}, // vts = 1984
	{0x380f, 0xc0}, // vts
	#endif
	{0x3810, 0x00}, // isp x win = 16
	{0x3811, 0x10}, // isp x win
	{0x3812, 0x00}, // isp y win = 6
	{0x3813, 0x06}, // isp y win
	{0x3814, 0x11}, // x inc
	{0x3815, 0x11}, // y inc
	{0x3817, 0x00}, // hsync start
	{0x3820, 0x46}, // flip off, v bin off
	{0x3821, 0x00}, // mirror on, h bin on
	{0x4004, 0x04}, // black line number
	{0x4005, 0x1a}, // blc always update
	{0x3208, 0x12}, // EXIT group 2 write mode
	{OV5648_REG_DELAY, 5},
	//{0x4202, 0x0f}, // stream off
	//{0x4800, 0x01},
	{OV5648_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list ov5648_stream_on[] = {
	{0x4202, 0x00},
	{0x4800, 0x24},

	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_stream_off[] = {
	/* Sensor enter LP11*/
	{0x4202, 0x0f},
	{0x4800, 0x01},

	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_fmt_yuv422[] = {
	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_win_sxga[] = {

	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_win_5m[] = {
	 //mipi_bitrate = 338Mbps/lane                                

	{OV5648_REG_END, 0x00},	/* END MARKER */
};


//pip ov5648 1280*720@30fps
static struct regval_list regPIP_Init_ov5648[] ={
	{0x0103,0x01}, // software reset
	{0xfffe,0x05},
	{0x3001,0x00}, // D[7:0] set to input
	{0x3002,0x80}, // Vsync output, PCLK, FREX, Strobe, CSD, CSK, GPIO input
	{0x3011,0x02}, // Drive strength 2x
	{0x3018,0x4c}, // MIPI 2 lane
	{0x301c,0xf0}, // OTP enable
	{0x3022,0x00}, // MIP lane enable
	{0x3034,0x1a}, // 10-bit mode
	{0x3035,0x21}, // PLL
	{0x3036,0x54}, // PLL
	{0x3037,0x02}, // PLL
	{0x3038,0x00}, // PLL
	{0x3039,0x00}, // PLL
	{0x303a,0x00}, // PLLS
	{0x303b,0x1f}, // PLLS
	{0x303c,0x11}, // PLLS
	{0x303d,0x30}, // PLLS
	{0x3105,0x11},
	{0x3106,0x05}, // PLL
	{0x3304,0x28},
	{0x3305,0x41},
	{0x3306,0x30},
	{0x3308,0x00},
	{0x3309,0xc8},
	{0x330a,0x01},
	{0x330b,0x90},
	{0x330c,0x02},
	{0x330d,0x58},
	{0x330e,0x03},
	{0x330f,0x20},
	{0x3300,0x00},
	{0x3500,0x00}, // exposure [19:16]
	{0x3501,0x2d}, // exposure [15:8]
	{0x3502,0x60}, // exposure [7:0], exposure = 0x2d6 = 726
	{0x3503,0x27}, // gain has no delay, manual agc/aec
	{0x350a,0x00}, // gain[9:8]
	{0x350b,0x40}, // gain[7:0], gain = 4x
	//analog control
	{0x3601,0x33},
	{0x3602,0x00},
	{0x3611,0x0e},
	{0x3612,0x2b},
	{0x3614,0x50},
	{0x3620,0x33},
	{0x3622,0x00},
	{0x3630,0xad},
	{0x3631,0x00},
	{0x3632,0x94},
	{0x3633,0x17},
	{0x3634,0x14},
	{0x3705,0x2a},
	{0x3708,0x66},
	{0x3709,0x52},
	{0x370b,0x23},
	{0x370c,0xc3},
	{0x370d,0x00},
	{0x370e,0x00},
	{0x371c,0x07},
	{0x3739,0xd2},
	{0x373c,0x00},
	{0x3800,0x00}, // xstart = 0
	{0x3801,0x00}, // xstart
	{0x3802,0x00}, // ystart = 0
	{0x3803,0x00}, // ystart
	{0x3804,0x0a}, // xend = 2639
	{0x3805,0x4f}, // xend
	{0x3806,0x07}, // yend = 1955
	{0x3807,0xa3}, // yend
	{0x3808,0x05}, // x output size = 1296
	{0x3809,0x10}, // x output size
	{0x380a,0x03}, // y output size = 972
	{0x380b,0xcc}, // y output size
	{0x380c,0x0e}, // hts = 3780
	{0x380d,0xc4}, // hts
	{0x380e,0x03}, // vts = 992
	{0x380f,0xe0}, // vts
	{0x3810,0x00}, // isp x win = 8
	{0x3811,0x08}, // isp x win
	{0x3812,0x00}, // isp y win = 2
	{0x3813,0x02}, // isp y win
	{0x3814,0x31}, // x inc
	{0x3815,0x31}, // y inc
	{0x3817,0x00}, // hsync start
	{0x3820,0x08}, // v flip off
	{0x3821,0x07}, // h mirror on
	{0x3826,0x03},
	{0x3829,0x00},
	{0x382b,0x0b},
	{0x3830,0x00},
	{0x3836,0x01},
	{0x3837,0xd4}, //wayne 0704 0xd4
	{0x3838,0x02},
	{0x3839,0xe0},
	{0x383a,0x0a},
	{0x383b,0x50},
	{0x3b00,0x00}, // strobe off
	{0x3b02,0x08}, // shutter delay
	{0x3b03,0x00}, // shutter delay
	{0x3b04,0x04}, // frex_exp
	{0x3b05,0x00}, // frex_exp
	{0x3b06,0x04},
	{0x3b07,0x08}, // frex inv
	{0x3b08,0x00}, // frex exp req
	{0x3b09,0x02}, // frex end option
	{0x3b0a,0x04}, // frex rst length
	{0x3b0b,0x00}, // frex strobe width
	{0x3b0c,0x3d}, // frex strobe width
	{0x3f01,0x0d},
	{0x3f0f,0xf5},
	{0x4000,0x89}, // blc enable
	{0x4001,0x02}, // blc start line
	{0x4002,0x45}, // blc auto, reset frame number = 5
	{0x4004,0x02}, // black line number
	{0x4005,0x18}, // blc normal freeze
	{0x4006,0x08},
	{0x4007,0x10},
	{0x4008,0x00},
	{0x4050,0x37}, // blc level trigger
	{0x4051,0x8f}, // blc level trigger
	{0x4300,0xf8},
	{0x4303,0xff},
	{0x4304,0x00},
	{0x4307,0xff},
	{0x4520,0x00},
	{0x4521,0x00},
	{0x4511,0x22},
	{0x481f,0x3c}, // MIPI clk prepare min
	{0x4826,0x00}, // MIPI hs prepare min
	{0x4837,0x18}, // MIPI global timing
	{0x4b00,0x44},
	{0x4b01,0x0a},
	{0x4b04,0x30},
	{0x5000,0xff}, // bpc on, wpc on
	{0x5001,0x00}, // AWB ON 00
	{0x5002,0x40}, // AWB Gain ON 40
	{0x5003,0x0b}, // buf en, bin auto en
	{0x5004,0x0c}, // size man on
	{0x5043,0x00},
	{0x5013,0x00},
	{0x501f,0x03}, // ISP output data
	{0x503d,0x00}, // test pattern off
	{0x5a00,0x08},
	{0x5b00,0x01}, // PIP H size = 320
	{0x5b01,0x40}, // PIP H size
	{0x5b02,0x00}, // PIP V size = 240
	{0x5b03,0xf0}, // PIP V size
	{0x5b04,0x78}, // PIP color B, PIP color should match with color of RGB raw to make border gray
	{0x5b05,0xa0}, // PIP color G
	{0x5b06,0x5c}, // PIP color R
	//ISP AEC/AGC setting from here
	//{0x3503,0x27}, //delay gain for one frame, and mannual enable.
	{0x5000,0x06}, // DPC On
	{0x5001,0x00}, // AWB ON
	{0x5002,0x41}, // AWB Gain ON
	{0x383b,0x80}, //
	{0x3a0f,0x38}, // AEC in H
	{0x3a10,0x30}, // AEC in L
	{0x3a1b,0x38}, // AEC out H
	{0x3a1e,0x30}, // AEC out L
	{0x3a11,0x70}, // Control Zone H
	{0x3a1f,0x18}, // Conrol Zone L
	//// @@ OV5648 Gain Table Alight
	{0x5180,0x08}, // Manual AWB
	//0x4202, 0x0f, // MIPI stream off
	//0x0100, 0x01, // wake up from standby

	{OV5648_REG_END, 0x00},	/* END MARKER */

};


//pip ov5648 1280*720@30fps
static struct regval_list regPIP_capture2preview[] ={

	
	{OV5648_REG_END, 0x00},	/* END MARKER */


};


static struct regval_list regPIP_capture_ov5648[] = 
{
	//int regPIP_Capture_OV5648[] 
	// OV5648 wake up
	{0x3022,0x00}, // MIP lane enable
	{0x3012,0x01},
	{0x3013,0x00},
	{0x3014,0x0b},
	{0x3005,0xf0},
	{0x301a,0xf0},
	{0x301b,0xf0},
	{0x5000,0x06}, // turn on ISP
	{0x5001,0x00}, // turn on ISP
	{0x5002,0x41}, // turn on ISP
	{0x5003,0x0b}, // turn on ISP
//	{0x3501,0x5b},
//	{0x3502,0x80},
	{0x3708,0x63},
	{0x3709,0x12},
	{0x370c,0xc0},
	{0x3800,0x00}, // x start = 16
	{0x3801,0x10}, // x start
	{0x3802,0x00}, // y start = 254
	{0x3803,0xfe}, // y start
	{0x3804,0x0a}, // x end = 2607
	{0x3805,0x2f}, // x end
	{0x3806,0x06}, // y end = 1701
	{0x3807,0xa5}, // y end
	{0x3808,0x0a}, // x output size = 2560
	{0x3809,0x00}, // x output size
	{0x380a,0x05}, // y output size = 1440
	{0x380b,0xa0}, // y output size
	{0x380c,0x16}, // HTS = 5642
	{0x380d,0x0a}, // HTS
	{0x380e,0x05}, // VTS = 1480
	{0x380f,0xc8}, // VTS
	{0x3810,0x00}, // x offset = 16
	{0x3811,0x10}, // x offset
	{0x3812,0x00}, // y offset = 6
	{0x3813,0x06}, // y offset
	{0x3814,0x11}, // x inc
	{0x3815,0x11}, // y inc
	{0x3817,0x00}, // h sync start
	{0x3820,0x40}, // ISP vertical flip on, v bin off
	{0x3821,0x06}, // mirror on, h bin off
	{0x3830,0x00},
	{0x3838,0x06},
	{0x3839,0x00},
	// PIP roate 1800 deg
	{0x3836,0x07},
	{0x3837,0x37},
	{0x383a,0x01},
	{0x383b,0xa0}, // 80 PIP Location
	{0x4004,0x04}, // black line number
	{0x4005,0x1a}, // blc update every frame
	{0x5b00,0x02},
	{0x5b01,0x80},
	{0x5b02,0x01},
	{0x5b03,0xe0},
//	{0x4202,0x0f}, // MIPI stream off
//	{0x0100,0x01}, // wake up from software standby
	{OV5648_REG_END, 0x00},	/* END MARKER */

};


static struct regval_list regPIP_preview_ov5648[] = 
{
	//int regPIP_Preview_OV5648[] 
	//// OV5648 wake up
	{0x3022,0x00}, // MIPI lane enable
	{0x3012,0x01},
	{0x3013,0x00},
	{0x3014,0x0b},
	{0x3005,0xf0},
	{0x301a,0xf0},
	{0x301b,0xf0},
	{0x5000,0x06}, // turn on ISP
	{0x5001,0x00}, // turn on ISP
	{0x5002,0x41}, // turn on ISP
	{0x5003,0x0b}, // turn on ISP


//	{0x3501,0x2d},
//	{0x3502,0x60},
	{0x3708,0x66},
	{0x3709,0x52},
	{0x370c,0xc3},
	{0x3800,0x00}, // x start = 16
	{0x3801,0x10}, // x start
	{0x3802,0x00}, // y start = 254
	{0x3803,0xfe}, // y start
	{0x3804,0x0a}, // x end = 2607
	{0x3805,0x2f}, // x end
	{0x3806,0x06}, // y end = 1701
	{0x3807,0xa5}, // y end
	{0x3808,0x05}, // x output size = 1280
	{0x3809,0x00}, // x output size
	{0x380a,0x02}, // y output size = 720
	{0x380b,0xd0}, // y output size
	{0x380c,0x0e}, // HTS = 3780
	{0x380d,0xc4}, // HTS
	{0x380e,0x02}, // VTS = 742
	{0x380f,0xe6}, // VTS
	{0x3810,0x00}, // x offset = 8
	{0x3811,0x08}, // x offset
	{0x3812,0x00}, // y offset = 2
	{0x3813,0x02}, // y offset
	{0x3814,0x31}, // x inc
	{0x3815,0x31}, // y inc
	{0x3817,0x00}, // hsync start
	{0x3820,0x08}, // v flip off, v bin off
	{0x3821,0x07}, // h mirror on, h bin on
	{0x3830,0x00},
	{0x3838,0x02},
	{0x3839,0xe0},
	// PIP rotate 180 deg
	{0x3836,0x01},
	{0x3837,0xd7},//wayne
	{0x383a,0x0a},
	{0x383b,0x80},//wayne
//	{0x383b,0x88},
	{0x4004,0x02}, // black line number
	{0x4005,0x18}, // blc normal freeze
	{0x5b00,0x01},
	{0x5b01,0x40},
	{0x5b02,0x00},
	{0x5b03,0xf0},
//	{0x4202,0x0f}, // MIPI stream off
//	{0x0100,0x01}, // wake up from standby

	{OV5648_REG_END, 0x00},	/* END MARKER */

};


static struct regval_list regPIP_window_reposition[] = 
{	
	{0x3836,0x01},
	{0x3837,0xd7},
	{0x383a,0x0a},
//	{0x383b,0x80},
	{0x383b,0x88},
	{OV5648_REG_END, 0x00},	/* END MARKER */

};


static struct regval_list regPIP_video_ov5648[] = 
{
//int regPIP_Video_OV5648

	{0x3022,0x00}, // MIP lane enable
	{0x3012,0x01},
	{0x3013,0x00},
	{0x3014,0x0b},
	{0x3005,0xf0},
	{0x301a,0xf0},
	{0x301b,0xf0},
	{0x5000,0x06}, // turn on ISP
	{0x5001,0x00}, // turn on ISP
	{0x5002,0x41}, // turn on ISP
	{0x5003,0x0b}, // turn on ISP
	{0x3501,0x2d},
	{0x3502,0x60},
	{0x3708,0x66},
	{0x3709,0x52},
	{0x370c,0xc3},
	{0x3800,0x00}, // x start = 16
	{0x3801,0x10}, // x start
	{0x3802,0x00}, // y start = 254
	{0x3803,0xfe}, // y start
	{0x3804,0x0a}, // x end = 2607
	{0x3805,0x2f}, // x end
	{0x3806,0x06}, // y end = 1701
	{0x3807,0xa5}, // y end
	{0x3808,0x05}, // x output size = 1280
	{0x3809,0x00}, // x output size
	{0x380a,0x02}, // y output size = 720
	{0x380b,0xd0}, // y output size
	{0x380c,0x0e}, // HTS = 3780
	{0x380d,0xc4}, // HTS
	{0x380e,0x02}, // VTS = 742
	{0x380f,0xe6}, // VTS
	{0x3810,0x00}, // x offset = 8
	{0x3811,0x08}, // x offset
	{0x3812,0x00}, // y offset = 2
	{0x3813,0x02}, // y offset
	{0x3814,0x31}, // x inc
	{0x3815,0x31}, // y inc
	{0x3817,0x00}, // hsync start
	{0x3820,0x08}, // v flip off, bin off
	{0x3821,0x07}, // h mirror on, bin on
	{0x3830,0x00},
	{0x3838,0x02},
	{0x3839,0xe0},
// PIP rotate 180 deg
	{0x3836,0x00},
	{0x3837,0xd7},
	{0x383a,0x06},
	{0x383b,0x10},
	{0x4004,0x02}, // black line number
	{0x4005,0x18}, // blc normal freeze
	{0x5b00,0x01},
	{0x5b01,0xe0},
	{0x5b02,0x01},
	{0x5b03,0xe0},
	{OV5648_REG_END, 0x00},	/* END MARKER */

};


static struct regval_list regPIP_preview_ov7695[] ={
	// MIPI 2 lane format qvga
// MIPI bitrate = 420MBps
//int regPIP_Preview_OV7695[]

	{0x382c,0x05}, // manual control ISP window offset 0x3810~0x3813 during subsampled modes
	{0x0101,0x03}, // mirror off
	{0x3811,0x05}, //
	{0x3813,0x06},
	{0x034c,0x01}, // x output size = 320
	{0x034d,0x40}, // x output size
	{0x034e,0x00}, // y output size = 240
	{0x034f,0xf0}, // y output size
	{0x0383,0x03}, // x odd inc
	{0x4500,0x47}, // h sub sample on
	{0x0387,0x03}, // y odd inc
	{0x3820,0x94}, // v bin off
	{0x3014,0x30}, // MIPI phy rst, pd
	{0x301a,0x44},
	{0x370a,0x63},
	{0x4008,0x01}, // blc start line
	{0x4009,0x04}, // blc end line
	{0x0340,0x02}, // VTS =742
	{0x0341,0xe6}, // VTS
	{0x0342,0x02}, // HTS = 540
	{0x0343,0x1c}, // HTS
	{0x0344,0x00}, // x start = 0
	{0x0345,0x00}, // x start = 0
	{0x0348,0x02}, // x end = 639
	{0x0349,0x7f}, // x end
	{0x034a,0x01}, // y end = 479
	{0x034b,0xdf}, // y end
//	{0x3503,0x30}, // AGC/AEC on
	{0x3a09,0xde}, // B50 L
	{0x3a0b,0xb9}, // B60 L
	{0x3a0e,0x03}, // B50 Max
	{0x3a0d,0x04}, // B60 Max
	{0x3a02,0x02}, // Max expo 60
	{0x3a03,0xe6}, // Max expo 60
	{0x3a14,0x02}, // Max expo 50
	{0x3a15,0xe6}, // Max expo 50
	{0x5004,0x40}, // LCD R gain, adjust color of sub camera to match with main camera
	{0x5005,0x4a}, // LCD G gain
	{0x5006,0x52}, // LCD B gain
	{OV5648_REG_END, 0x00},	/* END MARKER */

};


static struct regval_list regPIP_capture_ov7695[] ={
	//int regPIP_Capture_OV7695[] 
	//// OV7695
	{0x382c,0xc5}, // manual control ISP window offset 0x3810~0x3813 during subsampled modes
	{0x0101,0x03}, // PIP rotate 180 deg
	{0x3811,0x05}, //
	{0x3813,0x06},
	{0x034c,0x02}, // x output size = 640
	{0x034d,0x80}, // x output size
	{0x034e,0x01}, // y output size = 480
	{0x034f,0xe0}, // y output size
	{0x0383,0x01}, // x odd inc
	{0x4500,0x25}, // h sub sample off
	{0x0387,0x01}, // y odd inc
	{0x3820,0x94}, // v bin off
	{0x3014,0x0f},
	{0x301a,0xf0},
	{0x370a,0x23},
	{0x4008,0x02}, // blc start line
	{0x4009,0x09}, // blc end line
	{0x0340,0x05}, // VTS = 1480
	{0x0341,0xc8}, // VTS
	{0x0342,0x03}, // HTS = 806
	{0x0343,0x26}, // HTS
	{0x0344,0x00}, // x start = 0
	{0x0345,0x00}, // x start = 0
	{0x0348,0x02}, // x end = 639
	{0x0349,0x7f}, // x end
	{0x034a,0x01}, // y end = 479
	{0x034b,0xdf}, // y end
//	{0x3503,0x30}, // AGC/AEC on
	{0x3a09,0x95}, // B50 L
	{0x3a0b,0x7c}, // B60 L
	{0x3a0e,0x0a}, // B50Max
	{0x3a0d,0x0c}, // B60Max
	{0x3a02,0x05}, // max expo 60
	{0x3a03,0xc8}, // max expo 60
	{0x3a14,0x05}, // max expo 50
	{0x3a15,0xc8}, // max expo 50
	{0x5004,0x40}, // LCD R gain, adjust color of sub camera to match with main camera
	{0x5005,0x4a}, // LCD G gain
	{0x5006,0x52}, // LCD B gain
	{OV5648_REG_END, 0x00},	/* END MARKER */

};


static struct regval_list regSub_Init_OV5648[] =
{
	{0x0103,0x01}, // software reset
	{0xfffe,0x05},
	{0x3001,0x00}, // D[7:0] set to input
	{0x3002,0x80}, // Vsync output, PCLK, FREX, Strobe, CSD, CSK, GPIO input
	{0x3011,0x02}, // Drive strength 2x
	{0x3018,0x4c}, // MIPI 2 lane
	{0x301c,0xf0}, // OTP enable
	{0x3022,0x00}, // MIP lane enable
	{0x3034,0x1a}, // 10-bit mode
	{0x3035,0x21}, // PLL
	{0x3036,0x54}, // PLL
	{0x3037,0x02}, // PLL
	{0x3038,0x00}, // PLL
	{0x3039,0x00}, // PLL
	{0x303a,0x00}, // PLLS
	{0x303b,0x1f}, // PLLS
	{0x303c,0x11}, // PLLS
	{0x303d,0x30}, // PLLS
	{0x3105,0x11},
	{0x3106,0x05}, // PLL
	{0x3304,0x28},
	{0x3305,0x41},
	{0x3306,0x30},
	{0x3308,0x00},
	{0x3309,0xc8},
	{0x330a,0x01},
	{0x330b,0x90},
	{0x330c,0x02},
	{0x330d,0x58},
	{0x330e,0x03},
	{0x330f,0x20},
	{0x3300,0x00},
	{0x3500,0x00}, // exposure [19:16]
	{0x3501,0x2d}, // exposure [15:8]
	{0x3502,0x60}, // exposure [7:0], exposure = 0x2d6 = 726
	{0x3503,0x27}, // gain has no delay, manual agc/aec
	{0x350a,0x00}, // gain[9:8]
	{0x350b,0x40}, // gain[7:0], gain = 4x
	//analog control
	{0x3601,0x33},
	{0x3602,0x00},
	{0x3611,0x0e},
	{0x3612,0x2b},
	{0x3614,0x50},
	{0x3620,0x33},
	{0x3622,0x00},
	{0x3630,0xad},
	{0x3631,0x00},
	{0x3632,0x94},
	{0x3633,0x17},
	{0x3634,0x14},
	{0x3705,0x2a},
	{0x3708,0x66},
	{0x3709,0x52},
	{0x370b,0x23},
	{0x370c,0xc3},
	{0x370d,0x00},
	{0x370e,0x00},
	{0x371c,0x07},
	{0x3739,0xd2},
	{0x373c,0x00},
	{0x3800,0x00}, // xstart = 0
	{0x3801,0x00}, // xstart
	{0x3802,0x00}, // ystart = 0
	{0x3803,0x00}, // ystart
	{0x3804,0x0a}, // xend = 2639
	{0x3805,0x4f}, // xend
	{0x3806,0x07}, // yend = 1955
	{0x3807,0xa3}, // yend
	{0x3808,0x05}, // x output size = 1296
	{0x3809,0x10}, // x output size
	{0x380a,0x03}, // y output size = 972
	{0x380b,0xcc}, // y output size
	{0x380c,0x0e}, // hts = 3780
	{0x380d,0xc4}, // hts
	{0x380e,0x03}, // vts = 992
	{0x380f,0xe0}, // vts
	{0x3810,0x00}, // isp x win = 8
	{0x3811,0x08}, // isp x win
	{0x3812,0x00}, // isp y win = 2
	{0x3813,0x02}, // isp y win
	{0x3814,0x31}, // x inc
	{0x3815,0x31}, // y inc
	{0x3817,0x00}, // hsync start
	{0x3820,0x08}, // v flip off
	{0x3821,0x07}, // h mirror on
	{0x3826,0x03},
	{0x3829,0x00},
	{0x382b,0x0b},
	{0x3830,0x00},
	{0x3836,0x01},
	{0x3837,0xd4},
	{0x3838,0x02},
	{0x3839,0xe0},
	{0x383a,0x0a},
	{0x383b,0x50},
	{0x3b00,0x00}, // strobe off
	{0x3b02,0x08}, // shutter delay
	{0x3b03,0x00}, // shutter delay
	{0x3b04,0x04}, // frex_exp
	{0x3b05,0x00}, // frex_exp
	{0x3b06,0x04},
	{0x3b07,0x08}, // frex inv
	{0x3b08,0x00}, // frex exp req
	{0x3b09,0x02}, // frex end option
	{0x3b0a,0x04}, // frex rst length
	{0x3b0b,0x00}, // frex strobe width
	{0x3b0c,0x3d}, // frex strobe width
	{0x3f01,0x0d},
	{0x3f0f,0xf5},
	{0x4000,0x89}, // blc enable
	{0x4001,0x02}, // blc start line
	{0x4002,0x45}, // blc auto, reset frame number = 5
	{0x4004,0x02}, // black line number
	{0x4005,0x18}, // blc normal freeze
	{0x4006,0x08},
	{0x4007,0x10},
	{0x4008,0x00},
	{0x4050,0x37}, // blc level trigger
	{0x4051,0x8f}, // blc level trigger
	{0x4300,0xf8},
	{0x4303,0xff},
	{0x4304,0x00},
	{0x4307,0xff},
	{0x4520,0x00},
	{0x4521,0x00},
	{0x4511,0x22},
	{0x481f,0x3c}, // MIPI clk prepare min
	{0x4826,0x00}, // MIPI hs prepare min
	{0x4837,0x18}, // MIPI global timing
	{0x4b00,0x44},
	{0x4b01,0x0a},
	{0x4b04,0x30},
	{0x5000,0xff}, // bpc on, wpc on
	{0x5001,0x00}, // AWB ON 00
	{0x5002,0x40}, // AWB Gain ON 40
	{0x5003,0x0b}, // buf en, bin auto en
	{0x5004,0x0c}, // size man on
	{0x5043,0x00},
	{0x5013,0x00},
	{0x501f,0x03}, // ISP output data
	{0x503d,0x00}, // test pattern off
	{0x5a00,0x08},
	{0x5b00,0x01}, // PIP H size = 320
	{0x5b01,0x40}, // PIP H size
	{0x5b02,0x00}, // PIP V size = 240
	{0x5b03,0xf0}, // PIP V size
	{0x5b04,0x78}, // PIP color B, PIP color should match with color of RGB raw to make border gray
	{0x5b05,0xa0}, // PIP color G
	{0x5b06,0x5c}, // PIP color R
	//ISP AEC/AGC setting from here
//	{0x3503,0x27}, //delay gain for one frame, and mannual enable.
	{0x5000,0x06}, // DPC On
	{0x5001,0x00}, // AWB ON
	{0x5002,0x41}, // AWB Gain ON
	{0x383b,0x80}, //
	{0x3a0f,0x38}, // AEC in H
	{0x3a10,0x30}, // AEC in L
	{0x3a1b,0x38}, // AEC out H
	{0x3a1e,0x30}, // AEC out L
	{0x3a11,0x70}, // Control Zone H
	{0x3a1f,0x18}, // Conrol Zone L
	//// @@ OV5648 Gain Table Alight
	{0x5180,0x08}, // Manual AWB
	//0x4202, 0x0f, // MIPI stream off
	//0x0100, 0x01, // wake up from standby

//section 2
	//int regPIP_sub_OV5648[] 
	{0x3830,0x40},
	{0x3808,0x02}, // Timing X output Size = 640
	{0x3809,0x80}, // Timing X output Size
	{0x380a,0x01}, // Timing Y output Size = 480
	{0x380b,0xe0}, // Timing Y output Size
	{0x380e,0x02}, // OV5648 VTS = 536
	{0x380f,0x18}, // OV5648 VTS
	{0x380c,0x14}, // OV5648 HTS = 5222
	{0x380d,0x66}, // OV5648 HTS
	//// OV5648 Block Power OFF
	{0x3022,0x03}, // pull down data lane 1/2
	{0x3012,0xa1}, // debug mode
	{0x3013,0xf0}, // debug mode
	{0x3014,0x7b}, //
	{0x3005,0xf3}, // debug mode
	{0x301a,0x96}, // debug mode
	{0x301b,0x5a}, // debug mode
	{0x5000,0x00}, // turn off ISP
	{0x5001,0x00}, // turn off ISP
	{0x5002,0x00}, // turn off ISP
	{0x5003,0x00}, // turn off ISP
	{0x0100,0x01}, // stream on

	

	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct v4l2_subdev *pip_sd = NULL;
static int ov5648_write(struct v4l2_subdev *sd, unsigned short reg,
						unsigned char value);
static int ov5648_read(struct v4l2_subdev *sd, unsigned short reg,
					   unsigned char *value);
static int ov5648_write_array(struct v4l2_subdev *sd, struct regval_list *vals);
static int ov5648_s_stream(struct v4l2_subdev *sd, int enable);
int ov5648_calc_capture_shutter(struct v4l2_subdev *sd,int capture_vts,int capture_hts);

extern int pip_ov7695_ctrl(int cmd, struct v4l2_subdev *sd);

int ov5648_pip_position_fresh(void)
{	
	printk("avoid disappearance\n");
	ov5648_write_array(pip_sd, regPIP_window_reposition);
	return 0;

}
EXPORT_SYMBOL(ov5648_pip_position_fresh);

int pip_ov5648_ctrl(int cmd, struct v4l2_subdev *sd)
{
	int rc = 0;
	struct i2c_client *client = v4l2_get_subdevdata(pip_sd);
	switch(cmd)
	{
	    	case OV5648_PIP_SUB_INIT:
			ov5648_write_array(pip_sd, regSub_Init_OV5648);
			printk("5648 pip init for sub camera only\n");
			break;
		case OV5648_PIP_INIT:
			ov5648_write_array(pip_sd, regPIP_Init_ov5648);
			ov5648_write(pip_sd,0x0100,0x01);
			printk("pip_preview ov5648 init\n");
			break;	
		case OV5648_PIP_PREVIEW_720P:
			ov5648_write_array(pip_sd, regPIP_preview_ov5648);
			break;
		case OV5648_PIP_CAPTURE_FULLSIZE:
			ov5648_write_array(pip_sd, regPIP_capture_ov5648);
			break;
		case OV5648_PIP_STREAM_ON:
			ov5648_s_stream(pip_sd, 1);	
			printk("pip_s_stream on\n");
			break;
		case OV5648_PIP_STREAM_OFF:
			ov5648_s_stream(pip_sd, 0);	
			printk("pip_s_stream off\n");
			break;
		case OV5648_PIP_STANDBY:
			ov5648_write(pip_sd,0x0100,0x00);
			printk("ov5648 standby\n");
			break;	
		case OV5648_PIP_WAKEUP:
			ov5648_write(pip_sd,0x0100,0x01);
			printk("ov5648 wake up\n");
			break;	

		default:
				break;
	}

	return rc;
}
EXPORT_SYMBOL(pip_ov5648_ctrl);
/* End */

void ov5648_update_PIP_position(int position,int Cam_Mode)
{
return 0;//wayne
	if(Cam_Mode==PREVIEW) {
		switch(position) {
			case LL:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x0f);	
				ov5648_write(pip_sd, 0x4315, 0x95);	
				ov5648_write(pip_sd, 0x4316, 0xa4);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case LR:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x0f);	
				ov5648_write(pip_sd, 0x4315, 0xa1);	
				ov5648_write(pip_sd, 0x4316, 0xcc);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case UL:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x00);	
				ov5648_write(pip_sd, 0x4315, 0x07);	
				ov5648_write(pip_sd, 0x4316, 0x24);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case UR:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x00);	
				ov5648_write(pip_sd, 0x4315, 0x02);	
				ov5648_write(pip_sd, 0x4316, 0x68);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			default:
				break;
		}
	}
	else if(Cam_Mode==VIDEO) {
		switch(position) {
			case LL:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x1f);	
				ov5648_write(pip_sd, 0x4315, 0xb9);	
				ov5648_write(pip_sd, 0x4316, 0x8a);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case LR:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x1f);	
				ov5648_write(pip_sd, 0x4315, 0xbc);	
				ov5648_write(pip_sd, 0x4316, 0xa0);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case UL:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x00);	
				ov5648_write(pip_sd, 0x4315, 0x04);	
				ov5648_write(pip_sd, 0x4316, 0xac);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case UR:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x00);	
				ov5648_write(pip_sd, 0x4315, 0x07);	
				ov5648_write(pip_sd, 0x4316, 0xbc);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			default:
				break;
		}

	}
	else {
		switch(position) {
			case LL:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x2a);	
				ov5648_write(pip_sd, 0x4315, 0xdd);	
				ov5648_write(pip_sd, 0x4316, 0xb4);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case LR:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x2a);	
				ov5648_write(pip_sd, 0x4315, 0xe5);	
				ov5648_write(pip_sd, 0x4316, 0x2c);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case UL:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x00);	
				ov5648_write(pip_sd, 0x4315, 0x05);	
				ov5648_write(pip_sd, 0x4316, 0x38);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			case UR:
				ov5648_write(pip_sd, 0x3212, 0x00);	
				ov5648_write(pip_sd, 0x4314, 0x00);	
				ov5648_write(pip_sd, 0x4315, 0x00);	
				ov5648_write(pip_sd, 0x4316, 0xf8);	
				ov5648_write(pip_sd, 0x3212, 0x10);	
				ov5648_write(pip_sd, 0x3212, 0xa0);	
				break;
			default:
				break;
		}

	}
;
}
EXPORT_SYMBOL(ov5648_update_PIP_position);

static int ov5648_read(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;

	ret = i2c_transfer(client->adapter, msg, 2);
	//printk("mojl i2c ret %d ++++++++\n", ret);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov5648_write(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);	
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov5648_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV5648_REG_END) {
		if (vals->reg_num == OV5648_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov5648_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}


/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical = 0x55;
static unsigned int bg_ratio_typical = 0x61;

/*
	index: index of otp group. (0, 1, 2)
 	return: 0, group index is empty
 	1, group index has invalid data
 	2, group index has valid data
*/
static int ov5648_check_otp(struct v4l2_subdev *sd, unsigned short index)
{
	unsigned char temp;
	unsigned short i;
	unsigned short address;
	int ret;

	// read otp into buffer
	ov5648_write(sd, 0x3d21, 0x01);
	mdelay(5);
	address = 0x3d05 + index * 9;
	ret = ov5648_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	
	// disable otp read
	ov5648_write(sd, 0x3d21, 0x00);
	
	// clear otp buffer
	for (i = 0; i < 32; i++)
		ov5648_write(sd, 0x3d00 + i, 0x00);
	
	if (!temp)
		return 0;
	else if (!(temp & 0x80) && (temp & 0x7f))
		return 2;
	else
		return 1;
}

/*
	index: index of otp group. (0, 1, 2)
	return: 0,
*/
static int ov5648_read_otp(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp)
{
	unsigned char temp;
	unsigned short address;
	unsigned short i;
	int ret;

	// read otp into buffer
	ov5648_write(sd, 0x3d21, 0x01);
	mdelay(5);

	address = 0x3d05 + index * 9;

	ret = ov5648_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	otp->customer_id = (unsigned int)(temp & 0x7f);

	ret = ov5648_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	otp->module_integrator_id = (unsigned int)temp;

	ret = ov5648_read(sd, address + 1, &temp);
	if (ret < 0)
		return ret;
	otp->lens_id = (unsigned int)temp;

	ret = ov5648_read(sd, address + 2, &temp);
	if (ret < 0)
		return ret;
	otp->rg_ratio = (unsigned int)temp;

	ret = ov5648_read(sd, address + 3, &temp);
	if (ret < 0)
		return ret;
	otp->bg_ratio = (unsigned int)temp;

	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ otp.rg_ratio=%d. \n", otp->rg_ratio);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ otp.bg_ratio=%d. \n", otp->bg_ratio);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ rg_ratio_typical=%d. \n", rg_ratio_typical);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ bg_ratio_typical=%d. \n", bg_ratio_typical);

	for (i = 0; i<= 4; i++) {
		ret = ov5648_read(sd, address + 4 + i, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[i] = (unsigned int)temp;
	}

	// disable otp read
	ov5648_write(sd, 0x3d21, 0x00);

	// clear otp buffer
	for (i = 0; i < 32; i++)
		ov5648_write(sd, 0x3d00 + i, 0x00);

	return 0;
}

/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov5648_update_awb_gain(struct v4l2_subdev *sd, 
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	printk("R_gain:%04x, G_gain:%04x, B_gain:%04x \n ", R_gain, G_gain, B_gain);
	if (R_gain > 0x400) {
		ov5648_write(sd, 0x5186, (unsigned char)(R_gain >> 8));
		ov5648_write(sd, 0x5187, (unsigned char)(R_gain & 0x00ff));
	}

	if (G_gain > 0x400) {
		ov5648_write(sd, 0x5188, (unsigned char)(G_gain >> 8));
		ov5648_write(sd, 0x5189, (unsigned char)(G_gain & 0x00ff));
	}

	if (B_gain > 0x400) {
		ov5648_write(sd, 0x518a, (unsigned char)(B_gain >> 8));
		ov5648_write(sd, 0x518b, (unsigned char)(B_gain & 0x00ff));
	}

	return 0;
}

/*
	call this function after OV5648 initialization
	return value: 0 update success
	1, no OTP
*/
static int ov5648_update_otp(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	unsigned int i;
	unsigned int otp_index;
	unsigned int temp;
	unsigned int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int ret = 0;

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for (i = 0; i < 3; i++) {
		temp = ov5648_check_otp(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}

	if (i == 3)
		// no valid wb OTP data
		return 1;

	ret = ov5648_read_otp(sd, otp_index, &current_otp);
	if (ret < 0)
		return ret;

	//calculate G gain
	//0x400 = 1x gain
	if (current_otp.bg_ratio < bg_ratio_typical) {
		if (current_otp.rg_ratio < rg_ratio_typical) {
			// current_otp.bg_ratio < bg_ratio_typical &&
			// current_otp.rg_ratio < rg_ratio_typical
			G_gain = 0x400;
			B_gain = 0x400 * bg_ratio_typical / current_otp.bg_ratio;
			R_gain = 0x400 * rg_ratio_typical / current_otp.rg_ratio;
		} else {
			// current_otp.bg_ratio < bg_ratio_typical &&
			// current_otp.rg_ratio >= rg_ratio_typical
			R_gain = 0x400;
			G_gain = (0x400 * current_otp.rg_ratio) / rg_ratio_typical;
			B_gain = (G_gain * bg_ratio_typical) / current_otp.bg_ratio;
			printk("G_gain:%04x\n ", G_gain);
		}
	} else {
		if (current_otp.rg_ratio < rg_ratio_typical) {
			// current_otp.bg_ratio >= bg_ratio_typical &&
			// current_otp.rg_ratio < rg_ratio_typical
			B_gain = 0x400;
			G_gain = 0x400 * current_otp.bg_ratio / bg_ratio_typical;
			R_gain = G_gain * rg_ratio_typical / current_otp.rg_ratio;
		} else {
			// current_otp.bg_ratio >= bg_ratio_typical &&
			// current_otp.rg_ratio >= rg_ratio_typical
			G_gain_B = 0x400 * current_otp.bg_ratio / bg_ratio_typical;
			G_gain_R = 0x400 * current_otp.rg_ratio / rg_ratio_typical;
			if (G_gain_B > G_gain_R ) {
				B_gain = 0x400;
				G_gain = G_gain_B;
				R_gain = G_gain * rg_ratio_typical / current_otp.rg_ratio;
			} else {
				R_gain = 0x400;
				G_gain = G_gain_R;
				B_gain = G_gain * bg_ratio_typical / current_otp.bg_ratio;
			}
		}
	}

	ov5648_update_awb_gain(sd, R_gain, G_gain, B_gain);

	return 0;
}





static int ov5648_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static void pip_init(void)
{	
	pip_ov7695_ctrl(OV7695_PIP_INIT, NULL);
	msleep(35);
	pip_ov5648_ctrl(OV5648_PIP_INIT, NULL);

}


static void pip_preview(void)
{	
	pip_ov5648_ctrl(OV5648_PIP_STANDBY, NULL);
	pip_ov7695_ctrl(OV7695_PIP_PREVIEW_QVGA, NULL);
	msleep(5);
	pip_ov5648_ctrl(OV5648_PIP_PREVIEW_720P, NULL);
	pip_ov5648_ctrl(OV5648_PIP_WAKEUP, NULL);	
	pip_ov5648_ctrl(OV5648_PIP_STREAM_ON, NULL);
}


static int ov5648_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
	int ret = 0;
	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->dynamic_framerate = 0;
	info->binning = 2;
	printk("info->if_pip = %d\n",info->if_pip);
	if(info->if_pip) {
		info->first_start =1;
		info->max_height = PIP_MAX_HEIGHT;
		info->max_width = PIP_MAX_WIDTH;
	}
	else {
		info->max_height = MAX_HEIGHT;
		info->max_width = MAX_WIDTH;
	}

	if(!info->if_pip) {
		/*setting for sub camera only*/
		ret = ov5648_write_array(sd, ov5648_init_regs);
	} else {
	/*setting for viv preview*/
		pip_init();
		pip_preview();
	}

	return 0;
}

static int ov5648_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;	
	ret = ov5648_read(sd, 0x300a, &v);
	printk("%s +++ sensor id %x", __func__,  v);
	if (ret < 0)
		return ret;
	if (v != OV5648_CHIP_ID_H)
		return -ENODEV;
	ret = ov5648_read(sd, 0x300b, &v);
	printk(" %x\n", v);
	if (ret < 0)
		return ret;
	if (v != OV5648_CHIP_ID_L)
		return -ENODEV;
	return 0;
}


static struct ov5648_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov5648_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV5648_FMTS ARRAY_SIZE(ov5648_formats)

static struct ov5648_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	struct regval_list *regs; /* Regs to tweak */
} ov5648_win_sizes[] = {
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.vts		= 0x3d8,
		.framerate	= 30,
		.regs 		= ov5648_win_sxga,
	},
	/* 2560*1920 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x7b0,
		.framerate	=15,
		.regs 		= ov5648_win_5m,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(ov5648_win_sizes))

static struct ov5648_win_size ov5648_pip_win_sizes[] = {
	{
		.width		= PIP_PREVIEW_WIDTH,
		.height		= PIP_PREVIEW_HEIGHT,
		.vts		= 0x2e6,
		.framerate	= 30,
		.regs 		= ov5648_win_sxga,
	},
	{
		.width		= PIP_MAX_WIDTH,
		.height		= PIP_MAX_HEIGHT,
		.vts		= 0x5c8,
		.framerate	= 10,
		.regs 		= ov5648_win_5m,
	},
};

#define N_PIP_WIN_SIZES (ARRAY_SIZE(ov5648_pip_win_sizes))

static long ov5648_get_sysclk(struct v4l2_subdev *sd)
{
	// calculate sysclk
	int ret;
	int XVCLK = 1950; //Real Clock/10000
	//int temp1, temp2;
	unsigned char temp1,temp2;
	int Multiplier, PreDiv, VCO, SysDiv, Pll_rdiv, Bit_div2x, sclk_rdiv;
	long sysclk;
	int sclk_rdiv_map[] = {1, 2, 4, 8};
	ret = ov5648_read(sd, 0x3034, &temp1);
//	temp1 = OV5648_read_i2c(0x3034);
	temp2 = temp1 & 0x0f;
	if (temp2 == 8 || temp2 == 10) {
		Bit_div2x = temp2>>1;
	}
	//temp1 = OV5648_read_i2c(0x3035);
	ret = ov5648_read(sd, 0x3035, &temp1);
	SysDiv = temp1>>4;
	if(SysDiv == 0) {
		SysDiv = 16;
	}
	//temp1 = OV5648_read_i2c(0x3036);
	ret = ov5648_read(sd, 0x3036, &temp1);
	Multiplier = temp1;
	//temp1 = OV5648_read_i2c(0x3037);
	ret = ov5648_read(sd, 0x3037, &temp1);
	PreDiv = temp1 & 0x0f;
	Pll_rdiv = ((temp1 >> 4) & 0x01) + 1;
	//temp1 = OV5648_read_i2c(0x3108);
	ret = ov5648_read(sd, 0x3108, &temp1);
	temp2 = temp1 & 0x03;
	sclk_rdiv = sclk_rdiv_map[temp2];
	VCO = XVCLK * Multiplier / PreDiv;
	sysclk = VCO / SysDiv / Pll_rdiv * 2 / Bit_div2x / sclk_rdiv*10000;
	printk("sysclk = 0x%x\n",sysclk);
	return sysclk;
}

static int ov5648_get_shutter(struct v4l2_subdev *sd)
{
	int ret,shutter= 0;
	unsigned char tmp;
	ret = ov5648_read(sd, 0x3500, &tmp);
	shutter = tmp<<8;
	ret = ov5648_read(sd, 0x3501, &tmp);
	shutter = (shutter + tmp)<<4;
	ret = ov5648_read(sd, 0x3502, &tmp);
	shutter = shutter + (tmp>>4);
	printk("shutter = %x\n",shutter);
	return shutter;
}

static int ov5648_set_shutter(struct v4l2_subdev *sd,int shutter)
{
	int ret;
	unsigned char tmp;
	shutter = shutter&0xffff;
	tmp = shutter&0x0f;
	tmp = tmp<<4;
	ret = ov5648_write(sd, 0x3502, tmp);
	tmp  = (shutter>>4)&0xff;
//	tmp = tmp>>4;
	printk("tmp = 0x%x\n",tmp);
	ret = ov5648_write(sd, 0x3501, tmp);
	tmp = shutter >>12;
	ret = ov5648_write(sd, 0x3500, tmp);
	return 0;

}

static int  ov5648_get_vts(struct v4l2_subdev *sd)
{
	unsigned char tmp;
	int vts;
	int ret;
	ret = ov5648_read(sd, 0x380e, &tmp);
	vts = tmp<<8;
	ret = ov5648_read(sd, 0x380f, &tmp);
	vts = vts + tmp;
	return vts;
}

static int  ov5648_get_hts(struct v4l2_subdev *sd)
{
	unsigned char tmp;
	int hts;
	int ret;
	ret = ov5648_read(sd, 0x380c, &tmp);
	hts = tmp<<8;
	ret = ov5648_read(sd, 0x380d, &tmp);
	hts = hts + tmp;
	printk("hts = 0x%x\n",hts);
	return hts;
}

static int ov5648_set_vts(struct v4l2_subdev *sd,int vts)
{
	unsigned char tmp;
	int ret;
	tmp = vts&0xff;
	ret = ov5648_write(sd, 0x380f, tmp);
	tmp = vts>>8;
	ret = ov5648_write(sd, 0x380e, tmp);
	return 0;
}

static int ov5648_get_gain16(struct v4l2_subdev *sd)
{
	int ret,gain16= 0;
	unsigned char tmp;
	ret = ov5648_read(sd, 0x350a, &tmp);
	gain16 = (tmp&0x03)<<8;
	ret = ov5648_read(sd, 0x350b, &tmp);
	gain16 = gain16 + tmp;
	printk("gain16 = %x\n",gain16);
	return gain16;

}

static int ov5648_set_gain16(struct v4l2_subdev *sd,int gain16)
{
	//write gain,16 = 1x
	int ret;
	unsigned char tmp;
	gain16 = gain16&0x3ff;
	tmp = gain16>>8;
	ret = ov5648_write(sd, 0x350a,tmp);
	tmp = gain16&0xff;
	ret = ov5648_write(sd, 0x350b,tmp);
	return 0;

}

int OV5648_get_light_frequency(void)
{
   	return 50;//50HZ in China
}

int ov5648_calc_capture_shutter(struct v4l2_subdev *sd,int capture_vts,int capture_hts)
{
	printk("calculate exposure and gain for snapshot\n");
	int light_frequency;
	int OV5648_preview_HTS, OV5648_preview_shutter, OV5648_preview_gain16;
	int OV5648_capture_HTS, OV5648_capture_VTS, OV5648_capture_shutter,
		OV5648_capture_gain16;
	int OV5648_capture_bandingfilter, OV5648_capture_max_band;
	long OV5648_preview_SCLK,OV5648_capture_SCLK;
       long OV5648_capture_gain16_shutter;
	int pre_frame_rate =31;//main camera preview framerate
	int snap_frame_rate= 10;//capture framerate
	int ret;
	unsigned char tmp;
	ret = ov5648_read(sd, 0x3501, &tmp);
	printk("ret = %d\n",ret);
	printk("0x3501 =%x\n",tmp);	
		ret = ov5648_read(sd, 0x3502, &tmp);
	printk("0x3502 =%x\n",tmp);	
			ret = ov5648_read(sd, 0x350a, &tmp);
	printk("0x350a =%x\n",tmp);	
			ret = ov5648_read(sd, 0x350b, &tmp);
	printk("0x350b =%x\n",tmp);	

	ov5648_write(sd, 0x3503,0x07);//turn off auto aec/agc
	msleep(10);
	OV5648_preview_shutter = ov5648_get_shutter(sd); // read preview shutter
	OV5648_preview_gain16 = ov5648_get_gain16(sd); // read preview gain
	OV5648_preview_SCLK = ov5648_get_sysclk(sd);//58500000;//ov5648_get_SCLK(sd); // read preview SCLK
	OV5648_preview_HTS = ov5648_get_hts(sd); // read preview HTS

	OV5648_capture_SCLK = OV5648_preview_SCLK; // read capture SCLK
	OV5648_capture_HTS = 0xb1c; // read capture HTS
	OV5648_capture_VTS = 0x7b0; // read capture VTS
	printk("ov5648 sclk = %d\n",OV5648_preview_SCLK);
	// calculate capture banding filter
	light_frequency = OV5648_get_light_frequency();
	printk("light_frequency = 0x%x\n",light_frequency);
	if (light_frequency == 60) {
	// 60Hz
		OV5648_capture_bandingfilter = OV5648_capture_SCLK/(100*OV5648_capture_HTS)*100/120;
	}
	else {
	// 50Hz
		printk("OV5648_capture_HTS = 0x%x\n",OV5648_capture_HTS);
		OV5648_capture_bandingfilter = OV5648_capture_SCLK/(100*OV5648_capture_HTS);
		printk("OV5648_capture_bandingfilter = 0x%x,OV5648_capture_SCLK = 0x%x\n",OV5648_capture_bandingfilter,OV5648_capture_SCLK);
	}
	OV5648_capture_max_band = (int)((OV5648_capture_VTS - 4)/OV5648_capture_bandingfilter);
//return 0;
	printk("OV5648_capture_max_band = 0x%x\n",OV5648_capture_max_band);

	// calculate OV5648 capture shutter/gain16
	OV5648_capture_shutter = (long)(OV5648_preview_shutter *OV5648_preview_HTS/OV5648_capture_HTS);

	printk("OV5648_capture_shutter =0x%x\n",OV5648_capture_shutter);

	if(OV5648_capture_shutter<64) {
		printk("4-\n");
		OV5648_capture_gain16_shutter = (long)(OV5648_preview_shutter * OV5648_preview_gain16*OV5648_preview_HTS/OV5648_capture_HTS/*pre_frame_rate/snap_frame_rate*/);
		OV5648_capture_shutter = (int)(OV5648_capture_gain16_shutter/16);
		if(OV5648_capture_shutter <1)
			OV5648_capture_shutter = 1;
		OV5648_capture_gain16 = (int)(OV5648_capture_gain16_shutter/OV5648_capture_shutter);
		if(OV5648_capture_gain16<16)
			OV5648_capture_gain16 = 16;
	}
	else {
		printk("4+\n");
		OV5648_capture_gain16_shutter = (long)OV5648_preview_gain16 * OV5648_capture_shutter;
		// gain to shutter
		if(OV5648_capture_gain16_shutter < (OV5648_capture_bandingfilter*16)) {
		// shutter < 1/100
		OV5648_capture_shutter = OV5648_capture_gain16_shutter/16;
		if(OV5648_capture_shutter <1)
		OV5648_capture_shutter = 1;
		OV5648_capture_gain16 = OV5648_capture_gain16_shutter/OV5648_capture_shutter;
		if(OV5648_capture_gain16<16)
		OV5648_capture_gain16 = 16;
		}
		else {
			if(OV5648_capture_gain16_shutter > (OV5648_capture_bandingfilter*OV5648_capture_max_band*16)) {

				// exposure reach max
				OV5648_capture_shutter = OV5648_capture_bandingfilter*OV5648_capture_max_band;
				OV5648_capture_gain16 = OV5648_capture_gain16_shutter/OV5648_capture_shutter;
			}
			else {
				// 1/100 < capture_shutter =< max, capture_shutter = n/100
				OV5648_capture_shutter = ((int) (OV5648_capture_gain16_shutter/16/OV5648_capture_bandingfilter)) *
				OV5648_capture_bandingfilter;
				OV5648_capture_gain16 = OV5648_capture_gain16_shutter/OV5648_capture_shutter;
			}
		}
	}
				printk("5\n");
	printk("OV5648_capture_gain16 = 0x%x,OV5648_capture_shutter =0x%x\n",OV5648_capture_gain16,OV5648_capture_shutter);

	// write capture gain
	ov5648_set_gain16(sd,OV5648_capture_gain16);
	
	// write capture shutter
	if (OV5648_capture_shutter > (OV5648_capture_VTS-4)) {
		OV5648_capture_VTS = OV5648_capture_shutter + 4;
		ov5648_set_vts(sd,OV5648_capture_VTS);
	}
	ov5648_set_shutter(sd,OV5648_capture_shutter);

	ret = ov5648_read(sd, 0x3501, &tmp);
	printk("ret = %d\n",ret);
	printk("0x3501 =%x\n",tmp);	
		ret = ov5648_read(sd, 0x3502, &tmp);
	printk("0x3502 =%x\n",tmp);	
			ret = ov5648_read(sd, 0x350a, &tmp);
	printk("0x350a =%x\n",tmp);	
			ret = ov5648_read(sd, 0x350b, &tmp);
	printk("0x350b =%x\n",tmp);	

	// calculate capture banding filter
	//OV5648_stream_on();
	// skip 2 vsync
	// start capture at 3rd vsync
	printk("OV5648_capture_max_band = 0x%x\n",OV5648_capture_max_band);
	printk("OV5648_capture_gain16 = 0x%x,OV5648_capture_shutter =0x%x\n",OV5648_capture_gain16,OV5648_capture_shutter);
	return 0;
}


static int ov5648_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV5648_FMTS)
		return -EINVAL;

	*code = ov5648_formats[index].mbus_code;
	return 0;
}

static int ov5648_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov5648_format_struct **ret_fmt,
		struct ov5648_win_size **ret_wsize)
{
	int index;
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
	struct ov5648_win_size *wsize;
	struct ov5648_win_size *P_win;
	int Num_win = 0;
	
	for (index = 0; index < N_OV5648_FMTS; index++)
		if (ov5648_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV5648_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov5648_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov5648_formats + index;

	fmt->field = V4L2_FIELD_NONE;
	
	if(!info->if_pip) {
		P_win = ov5648_win_sizes;	
		Num_win = N_WIN_SIZES;
	} 
	else {
	 	P_win = ov5648_pip_win_sizes;
		Num_win = N_PIP_WIN_SIZES;
	}

	for (wsize = P_win; wsize < P_win + Num_win;
	     	wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= P_win + Num_win)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov5648_formats[index].colorspace;
	return 0;
}

static int ov5648_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov5648_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov5648_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov5648_format_struct *fmt_s;
	struct ov5648_win_size *wsize;
	int ret;
	int len_sub,len_main,i;

	printk(KERN_INFO "%s: fmt->code = %d; fmt->width = %d;fmt->height = %d; \n",__func__,fmt->code,fmt->width,fmt->height);
	ret = ov5648_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov5648_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}
	memset(data, 0, sizeof(*data));
	data->vts = wsize->vts;		
	data->framerate = wsize->framerate;
	data->binning = info->binning;
	if(info->if_pip)
		data->mipi_clk = 410; /* Mbps. viv mode */
	else 
		data->mipi_clk = 420; /* Mbps. main camera only*/

	data->sensor_vts_address1 = info->vts_address1;
	data->sensor_vts_address2 = info->vts_address2;

	if(!info->if_pip){
		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa2;    //switch to capture mode
			if (!info->dynamic_framerate) {
				data->reg[1].addr = info->vts_address1;
				data->reg[1].data = data->vts >> 8;
				data->reg[2].addr = info->vts_address2;
				data->reg[2].data = data->vts & 0x00ff;
				data->reg_num = 3;
			}
		} else if ((wsize->width == SXGA_WIDTH)
			&& (wsize->height == SXGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa1;   //switch to preview mode
			if (!info->dynamic_framerate) {
				data->reg[1].addr = info->vts_address1;
				data->reg[1].data = data->vts >> 8;
				data->reg[2].addr = info->vts_address2;
				data->reg[2].data = data->vts & 0x00ff;
				data->reg_num = 3;
			}
		}
	}
	else {
		if ((wsize->width == PIP_MAX_WIDTH) //for snapshot
				&& (wsize->height == PIP_MAX_HEIGHT)) {
				printk("viv_capture\n");
				data->flags = V4L2_I2C_ADDR_16BIT;
				data->slave_addr = 0x21;
				data->viv_addr = client->addr; //ov769
				len_sub = ARRAY_SIZE(regPIP_capture_ov7695);
				data->reg_num = len_sub;  //
				for(i =0;i<len_sub;i++) {
					data->reg[i].addr = regPIP_capture_ov7695[i].reg_num;
					data->reg[i].data = regPIP_capture_ov7695[i].value;
				}
				len_main = ARRAY_SIZE(regPIP_capture_ov5648);
				data->viv_section = len_sub;
				data->viv_len = len_main;  //
				for(i = 0;i< len_main;i++) {
					data->reg[i + len_sub].addr = regPIP_capture_ov5648[i].reg_num;
					data->reg[i + len_sub].data = regPIP_capture_ov5648[i].value;
				}

				printk("data->reg[data->viv_section].addr = %x,data->reg[data->viv_section].value = %x,\n",
				data->reg[data->viv_section].addr,
				data->reg[data->viv_section].data);

				
			} else if ((wsize->width == PIP_PREVIEW_WIDTH)
				&& (wsize->height == PIP_PREVIEW_HEIGHT)) {
				printk("viv_preview\n");
				data->flags = V4L2_I2C_ADDR_16BIT;
				data->slave_addr = client->addr;
				data->viv_addr =  0x21; //ov7695
				len_sub = ARRAY_SIZE(regPIP_preview_ov5648);
				data->reg_num = len_sub;  //
				for(i =0;i<len_sub;i++) {
					data->reg[i].addr = regPIP_preview_ov5648[i].reg_num;
					data->reg[i].data = regPIP_preview_ov5648[i].value;
				}
				len_main = ARRAY_SIZE(regPIP_preview_ov7695);
				data->viv_section = len_sub;
				data->viv_len = len_main;  //
				for(i = 0;i< len_main;i++) {
					data->reg[i + len_sub].addr = regPIP_preview_ov7695[i].reg_num;
					data->reg[i + len_sub].data = regPIP_preview_ov7695[i].value;
				}
			}

	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov5648_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	if (enable) {		
		ret = ov5648_write_array(sd, ov5648_stream_on);
	} else
		ret = ov5648_write_array(sd, ov5648_stream_off);

	return ret;
}

static int ov5648_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);

	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= info->max_width;
	a->c.height	= info->max_height;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov5648_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);

	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= info->max_width;
	a->bounds.height		= info->max_height;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int ov5648_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5648_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5648_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov5648_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov5648_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov5648_frame_rates[interval->index];
	return 0;
}

static int ov5648_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	for (i = 0; i < N_WIN_SIZES; i++) {
		struct ov5648_win_size *win = &ov5648_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov5648_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov5648_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        struct v4l2_isp_setting isp_setting;
        int ret = 0;

        switch (ctrl->id) {
        case V4L2_CID_ISP_SETTING:
                isp_setting.setting = (struct v4l2_isp_regval *)&ov5648_isp_setting;
                isp_setting.size = ARRAY_SIZE(ov5648_isp_setting);
                memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
                break;
        case V4L2_CID_ISP_PARM:
                ctrl->value = (int)&ov5648_isp_parm;
                break;
        default:
                ret = -EINVAL;
                break;
        }

        return ret;
}

static int ov5648_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		if (ctrl->value == FRAME_RATE_AUTO) {
			info->dynamic_framerate = 1;
		} 
		else ret = -EINVAL;
		break;
	case V4L2_CID_SET_VIV_MODE:
		if (ctrl->value == VIV_ON) 
			info->if_pip = 1;
		else 
			info->if_pip = 0;
		break;
	case V4L2_CID_SET_VIV_WIN_POSITION:
		ov5648_update_PIP_position(ctrl->value,PREVIEW);
		break;
	default:
		ret = -ENOIOCTLCMD; 
		break;
	}
	
	return ret;
}

static int ov5648_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV5647, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5648_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov5648_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov5648_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov5648_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov5648_core_ops = {
	.g_chip_ident = ov5648_g_chip_ident,
	.g_ctrl = ov5648_g_ctrl,
	.s_ctrl = ov5648_s_ctrl,
	.queryctrl = ov5648_queryctrl,
	.reset = ov5648_reset,
	.init = ov5648_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov5648_g_register,
	.s_register = ov5648_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov5648_video_ops = {
	.enum_mbus_fmt = ov5648_enum_mbus_fmt,
	.try_mbus_fmt = ov5648_try_mbus_fmt,
	.s_mbus_fmt = ov5648_s_mbus_fmt,
	.s_stream = ov5648_s_stream,
	.cropcap = ov5648_cropcap,
	.g_crop	= ov5648_g_crop,
	.s_parm = ov5648_s_parm,
	.g_parm = ov5648_g_parm,
	.enum_frameintervals = ov5648_enum_frameintervals,
	.enum_framesizes = ov5648_enum_framesizes,
};

static const struct v4l2_subdev_ops ov5648_ops = {
	.core = &ov5648_core_ops,
	.video = &ov5648_video_ops,
};


static ssize_t ov5648_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", rg_ratio_typical);
}

static ssize_t ov5648_rg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical = (unsigned int)value;

	return size;
}

static ssize_t ov5648_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", bg_ratio_typical);
}

static ssize_t ov5648_bg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov5648_rg_ratio_typical, 0664, ov5648_rg_ratio_typical_show, ov5648_rg_ratio_typical_store);
static DEVICE_ATTR(ov5648_bg_ratio_typical, 0664, ov5648_bg_ratio_typical_show, ov5648_bg_ratio_typical_store);


static int ov5648_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov5648_info *info;
	int ret;

	info = kzalloc(sizeof(struct ov5648_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov5648_ops);

	pip_sd = sd;

	/* Make sure it's an ov5648 */
	ret = ov5648_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov5648 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov5648 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);	

#if 1
	ret = device_create_file(&client->dev, &dev_attr_ov5648_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5648_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5648_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov5648_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5648_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5648_bg_ratio_typical;
	}


#endif
	return 0;
err_create_dev_attr_ov5648_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_ov5648_rg_ratio_typical);
err_create_dev_attr_ov5648_rg_ratio_typical:
	kfree(info);
	return ret;

}

static int ov5648_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);

	device_remove_file(&client->dev, NULL);
	device_remove_file(&client->dev, NULL);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov5648_id[] = {
	{ "ov5648_pip", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5648_id);

static struct i2c_driver ov5648_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov5648_pip",
	},
	.probe		= ov5648_probe,
	.remove		= ov5648_remove,
	.id_table	= ov5648_id,
};

static __init int init_ov5648(void)
{
	return i2c_add_driver(&ov5648_driver);
}

static __exit void exit_ov5648(void)
{
	i2c_del_driver(&ov5648_driver);
}

fs_initcall(init_ov5648);
module_exit(exit_ov5648);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov5648 sensors");
MODULE_LICENSE("GPL");
