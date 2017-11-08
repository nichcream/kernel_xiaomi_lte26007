
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
#include <plat/camera.h>
#include "pip_ov5648_ov7695.h"
#include "ov7695_raw_pip.h"

#define OV7695_CHIP_ID	(0x7695)

#define MAX_WIDTH	640
#define MAX_HEIGHT	480

#define OV7695_TABLE_WAIT_MS 0xfffe
#define OV7695_TABLE_END 0xffff
#define OV7695_MAX_RETRIES 1

struct OV7695_format_struct;
struct OV7695_info {
	struct v4l2_subdev sd;
	struct OV7695_format_struct *fmt;
	struct OV7695_win_size *win;
	int vts_address1;
	int vts_address2;
	int brightness;
	int contrast;
	int frame_rate;
	int if_pip;
	int white_balance;
	int dynamic_framerate;
	int max_width;
	int max_height;
	int binning;
};

static struct regval_list regSub_Init_OV7695_VGA[] = {
	//ov7695 setting 
	{0x0103,0x01}, // software reset
	{0xfffe,0x05}, //sl 5
	{0x0101,0x00}, // Mirror off
	{0x3811,0x06},
	{0x3813,0x06},
	{0x382c,0x05}, // manual control ISP window offset 0x3810~0x3813 during subsampled modes
	{0x3620,0x28},
	{0x3623,0x12},
	{0x3718,0x88},
	{0x3632,0x05},
	{0x3013,0xd0},
	{0x3717,0x18},
	{0x3621,0x47},
	{0x034c,0x01},
	{0x034d,0x40},
	{0x034e,0x00},
	{0x034f,0xf0},
	{0x0383,0x03}, // x odd inc
	{0x4500,0x47}, // x sub control
	{0x0387,0x03}, // y odd inc
	{0x4300,0xf8}, // output RGB raw
	{0x0301,0x0a}, // PLL
	{0x0307,0x60}, // PLL
	{0x030b,0x04}, // PLL
	{0x3106,0x91}, // PLL
	{0x301e,0x60},
	{0x3014,0x30},
	{0x301a,0x44},
	{0x4803,0x08}, // HS prepare in UI
	{0x370a,0x63},
	{0x520a,0xf4}, // red gain from 0x400 to 0xfff
	{0x520b,0xf4}, // green gain from 0x400 to 0xfff
	{0x520c,0xf4}, // blue gain from 0x400 to 0xfff
	{0x4008,0x01}, // bl start
	{0x4009,0x04}, // bl end
	{0x0340,0x03}, // VTS = 992
	{0x0341,0xe0}, // VTS
	{0x0342,0x02}, // HTS = 540
	{0x0343,0x1c}, // HTS
	{0x0344,0x00}, // x start = 0
	{0x0345,0x00}, // x start
	{0x0348,0x02}, // x end = 639
	{0x0349,0x7f}, // x end
	{0x034a,0x01}, // y end = 479
	{0x034b,0xdf}, // y end
	{0x3820,0x94}, // by pass MIPI, v bin off,
	{0x4f05,0x01}, // SPI 2 lane on
	{0x4f06,0x02}, // SPI on
	{0x3012,0x00},
	{0x3005,0x0f}, // SPI pin on
	{0x3001,0x1f}, // SPI drive 3.75x, FSIN drive 1.75x
	{0x3823,0x30},
	{0x4303,0xee}, // Y max L
	{0x4307,0xee}, // U max L
	{0x430b,0xee}, // V max L
	{0x3620,0x2f},
	{0x3621,0x77},
	{0x0100,0x01}, // stream on
	//// PIP Demo
	{0x3503,0x07}, // delay gain for one frame, and mannual enable.
	{0x5000,0x8f}, // Gama OFF, AWB OFF
	{0x5002,0x8a}, // 50Hz/60Hz select£¬88 for 60Hz, 8a for 50hz
	{0x3a0f,0x2c}, // AEC in H, exposure target = 40
	{0x3a10,0x24}, // AEC in L
	{0x3a1b,0x2c}, // AEC out H
	{0x3a1e,0x24}, // AEC out L
	{0x3a11,0x58}, // control zone H
	{0x3a1f,0x12}, // control zone L
	{0x3a18,0x00}, // gain ceiling = 10x
	{0x3a19,0xa0}, // gain ceiling
	{0x3a08,0x00}, // B50
	{0x3a09,0xde}, // B50
	{0x3a0a,0x00}, // B60
	{0x3a0b,0xb9}, // B60
	{0x3a0e,0x03}, // B50 step
	{0x3a0d,0x04}, // B60 step
	{0x3a02,0x02}, // max expo 60
	{0x3a03,0xe6}, // max expo 60
	{0x3a14,0x02}, // max expo 50
	{0x3a15,0xe6}, // max expo 50
	 //lens A Light
	{0x5100,0x01}, // red x0
	{0x5101,0xc0}, // red x0
	{0x5102,0x01}, // red y0
	{0x5103,0x00}, // red y0
	{0x5104,0x4b}, // red a1
	{0x5105,0x04}, // red a2
	{0x5106,0xc2}, // red b1
	{0x5107,0x08}, // red b2
	{0x5108,0x01}, // green x0
	{0x5109,0xd0}, // green x0
	{0x510a,0x01}, // green y0
	{0x510b,0x00}, // green y0
	{0x510c,0x30}, // green a1
	{0x510d,0x04}, // green a2
	{0x510e,0xc2}, // green b1
	{0x510f,0x08}, // green b2
	{0x5110,0x01}, // blue x0
	{0x5111,0xd0}, // blue x0
	{0x5112,0x01}, // blue y0
	{0x5113,0x00}, // blue y0
	{0x5114,0x26}, // blue a1
	{0x5115,0x04}, // blue a2
	{0x5116,0xc2}, // blue b1
	{0x5117,0x08}, // blue b2
//pip_sub_7695 vga
	{0x382c,0xc5}, // manual control ISP window offset 0x3810~0x3813 during subsampled modes
#ifdef SUB_180
	{0x0101,0x03}, //
	{0x3811,0x05}, //
	{0x3813,0x05}, //
#else
	{0x0101,0x00}, //
	{0x3811,0x06}, //
	{0x3813,0x06}, //
#endif
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
	{0x0340,0x02}, // OV7695 VTS = 536
	{0x0341,0x18}, // OV6595 VTS
	{0x0342,0x02}, // OV7695 HTS = 746
	{0x0343,0xea}, // OV7695 HTS
//	{0x3503,0x27}, // AGC/AEC on  //wayne 0703
	{0x3a09,0xa1}, // B50
	{0x3a0b,0x86}, // B60
	{0x3a0e,0x03}, // B50 steps
	{0x3a0d,0x04}, // B60 steps
	{0x3a02,0x02}, // max expo 60
	{0x3a03,0x18}, // max expo 60
	{0x3a14,0x02}, // max expo 50
	{0x3a15,0x18}, // max expo 50
	{0x5004,0x40}, // LCD R gain, keep original color of sub camera
	{0x5005,0x40}, // LCD G gain
	{0x5006,0x40}, // LCD B gain

	
	{OV7695_TABLE_END, 0x00},
};

//yuv viv 720p preview @30fps,viv 320*240
static struct regval_list regPIP_Init_ov7695[] ={
	{0x0103,0x01}, // software reset
	{0xfffe,0x05}, //sl 5
	{0x0101,0x00}, // Mirror off
	{0x3811,0x06},
	{0x3813,0x06},

	{0x0101,0x03}, //
	{0x3811,0x05}, //
	{0x3813,0x05}, //
	
	{0x382c,0x05}, // manual control ISP window offset 0x3810~0x3813 during subsampled modes
	{0x3620,0x28},
	{0x3623,0x12},
	{0x3718,0x88},
	{0x3632,0x05},
	{0x3013,0xd0},
	{0x3717,0x18},
	{0x3621,0x47},
	{0x034c,0x01},
	{0x034d,0x40},
	{0x034e,0x00},
	{0x034f,0xf0},
	{0x0383,0x03}, // x odd inc
	{0x4500,0x47}, // x sub control
	{0x0387,0x03}, // y odd inc
	{0x4300,0xf8}, // output RGB raw
	{0x0301,0x0a}, // PLL
	{0x0307,0x60}, // PLL
	{0x030b,0x04}, // PLL
	{0x3106,0x91}, // PLL
	{0x301e,0x60},
	{0x3014,0x30},
	{0x301a,0x44},
	{0x4803,0x08}, // HS prepare in UI
	{0x370a,0x63},
	{0x520a,0xf4}, // red gain from 0x400 to 0xfff
	{0x520b,0xf4}, // green gain from 0x400 to 0xfff
	{0x520c,0xf4}, // blue gain from 0x400 to 0xfff
	{0x4008,0x01}, // bl start
	{0x4009,0x04}, // bl end
	{0x0340,0x03}, // VTS = 992
	{0x0341,0xe0}, // VTS
	{0x0342,0x02}, // HTS = 540
	{0x0343,0x1c}, // HTS
	{0x0344,0x00}, // x start = 0
	{0x0345,0x00}, // x start
	{0x0348,0x02}, // x end = 639
	{0x0349,0x7f}, // x end
	{0x034a,0x01}, // y end = 479
	{0x034b,0xdf}, // y end
	{0x3820,0x94}, // by pass MIPI, v bin off,
	{0x4f05,0x01}, // SPI 2 lane on
	{0x4f06,0x02}, // SPI on
	{0x3012,0x00},
	{0x3005,0x0f}, // SPI pin on
	{0x3001,0x1f}, // SPI drive 3.75x, FSIN drive 1.75x
	{0x3823,0x30},
	{0x4303,0xee}, // Y max L
	{0x4307,0xee}, // U max L
	{0x430b,0xee}, // V max L
	{0x3620,0x2f},
	{0x3621,0x77},
	{0x0100,0x01}, // stream on
	//// PIP Demo
	{0x3503,0x30}, // delay gain for one frame, and mannual enable.
	{0x5000,0x8f}, // Gama OFF, AWB OFF
	{0x5002,0x8a}, // 50Hz/60Hz select£¬88 for 60Hz, 8a for 50hz
	{0x3a0f,0x2c}, // AEC in H, exposure target = 40
	{0x3a10,0x24}, // AEC in L
	{0x3a1b,0x2c}, // AEC out H
	{0x3a1e,0x24}, // AEC out L
	{0x3a11,0x58}, // control zone H
	{0x3a1f,0x12}, // control zone L
	{0x3a18,0x00}, // gain ceiling = 10x
	{0x3a19,0xa0}, // gain ceiling
	{0x3a08,0x00}, // B50
	{0x3a09,0xde}, // B50
	{0x3a0a,0x00}, // B60
	{0x3a0b,0xb9}, // B60
	{0x3a0e,0x03}, // B50 step
	{0x3a0d,0x04}, // B60 step
	{0x3a02,0x02}, // max expo 60
	{0x3a03,0xe6}, // max expo 60
	{0x3a14,0x02}, // max expo 50
	{0x3a15,0xe6}, // max expo 50
	{0x3a00,0x78},
	 //lens A Light
	{0x5100,0x01}, // red x0
	{0x5101,0xc0}, // red x0
	{0x5102,0x01}, // red y0
	{0x5103,0x00}, // red y0
	{0x5104,0x4b}, // red a1
	{0x5105,0x04}, // red a2
	{0x5106,0xc2}, // red b1
	{0x5107,0x08}, // red b2
	{0x5108,0x01}, // green x0
	{0x5109,0xd0}, // green x0
	{0x510a,0x01}, // green y0
	{0x510b,0x00}, // green y0
	{0x510c,0x30}, // green a1
	{0x510d,0x04}, // green a2
	{0x510e,0xc2}, // green b1
	{0x510f,0x08}, // green b2
	{0x5110,0x01}, // blue x0
	{0x5111,0xd0}, // blue x0
	{0x5112,0x01}, // blue y0
	{0x5113,0x00}, // blue y0
	{0x5114,0x26}, // blue a1
	{0x5115,0x04}, // blue a2
	{0x5116,0xc2}, // blue b1
	{0x5117,0x08}, // blue b2

	{OV7695_TABLE_END, 0x00},	/* END MARKER */

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
	{OV7695_TABLE_END, 0x00},	/* END MARKER */

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
//	{0x3503,0x30}, // AGC/AEC off
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
	{OV7695_TABLE_END, 0x00},	/* END MARKER */

};

static struct regval_list regPIP_video_ov7695[] ={
	//int regPIP_Video_OV7695[] 
	//// OV7695
	{0x382c,0x05}, // manual control ISP window offset 0x3810~0x3813 during subsampled modes
	{0x0101,0x03},
	{0x3811,0x05}, // shift one pixel
	{0x3813,0x06},
	{0x034c,0x01}, // x output size = 480
	{0x034d,0xe0}, // x output size
	{0x034e,0x01}, // y output size = 480
	{0x034f,0xe0}, // y output size
	{0x0383,0x01}, // x odd inc
	{0x4500,0x25}, // h sub sample on
	{0x0387,0x01}, // y odd inc
	{0x3820,0x94}, // v bin off
	{0x3014,0x0f},
	{0x301a,0xf0},
	{0x370a,0x23},
	{0x4008,0x02}, // blc start line
	{0x4009,0x09}, // blc end line
	{0x0340,0x02}, // VTS = 742
	{0x0341,0xe6}, // VTS
	{0x0342,0x02}, // HTS = 540
	{0x0343,0x1c}, // HTS
	{0x0344,0x00}, // x start = 80
	{0x0345,0x50}, // x start
	{0x0348,0x02}, // x end = 479
	{0x0349,0x2f}, // x end
	{0x034a,0x01}, // y end = 479
	{0x034b,0xdf}, // y end
	{0x3503,0x07}, // AGC/AEC on
	{0x3a09,0xde}, // B50
	{0x3a0b,0xb9}, // B60
	{0x3a0e,0x03}, // B50 Max
	{0x3a0d,0x04}, // B60 Max
	{0x3a02,0x02}, // max expo 60
	{0x3a03,0xe6}, // max expo 60
	{0x3a14,0x02}, // max expo 50
	{0x3a15,0xe6}, // max expo 50
	{0x5004,0x40}, // LCD R gain, adjust color of sub camera to match with main camera
	{0x5005,0x4a}, // LCD G gain
	{0x5006,0x52}, // LCD B gain
	{OV7695_TABLE_END, 0x00},	/* END MARKER */

};



static struct regval_list OV7695_fmt_yuv422[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_win_vga[] = {
	{OV7695_TABLE_END, 0x00},
};

static struct regval_list OV7695_win_5m[] = {
	{OV7695_TABLE_END, 0x00},
};

static struct regval_list OV7695_stream_on[] = {
	{0x0100, 0x01}, 	
	{OV7695_TABLE_WAIT_MS,100},
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_stream_off[] = {
	{0x0100, 0x00}, 	
	{OV7695_TABLE_WAIT_MS,100},
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_brightness_l3[] = {

	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_brightness_l2[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_brightness_l1[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_brightness_h0[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_brightness_h1[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_brightness_h2[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_brightness_h3[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_contrast_l3[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_contrast_l2[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_contrast_l1[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_contrast_h0[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_contrast_h1[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_contrast_h2[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_contrast_h3[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_white_balance_auto[] = {
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_white_balance_incandescent[] = {  
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_white_balance_fluorescent[] = {  //
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_white_balance_warm_flourescent[] = {   // same as fluorescent
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct regval_list OV7695_white_balance_daylight[] = {  
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};
static struct regval_list OV7695_white_balance_cloudy_daylight[] = {  
	{OV7695_TABLE_END, 0x00},	/* END MARKER */
};

static struct v4l2_subdev *sub_sd = NULL;
static int debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

extern int pip_ov5648_ctrl(int cmd, struct v4l2_subdev *sd);

static int OV7695_read(struct v4l2_subdev *sd, unsigned short reg,
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
	printk("mojl i2c addr %d ++++++++\n", client->addr);
	ret = i2c_transfer(client->adapter, msg, 2);
//	printk("mojl i2c ret %d ++++++++\n", ret);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int OV7695_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int OV7695_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;	
     int count = 0;
	 printk("count = %d\n",count);
	while (vals->reg_num != OV7695_TABLE_END) {
		count++;
		if (vals->reg_num == OV7695_TABLE_WAIT_MS) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret =  OV7695_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

int pip_ov7695_ctrl(int cmd, struct v4l2_subdev *sd)
{
	int rc = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sub_sd);

	switch(cmd)
	{
	    case OV7695_PIP_INIT:
			OV7695_write_array(sub_sd, regPIP_Init_ov7695);
			printk("loading ov7695 setting for viv mode\n");
			break;
		case OV7695_PIP_PREVIEW_QVGA:
			OV7695_write_array(sub_sd, regPIP_preview_ov7695);
			printk("loading ov7695 preview setting for viv mode\n");
			break;
		case OV7695_PIP_CAPTURE_FULLSIZE:
			OV7695_write_array(sub_sd, regPIP_capture_ov7695);
			printk("loading ov7695 capture setting for viv mode\n");
			break;
		case OV7695_PIP_STREAM_OFF:
			OV7695_write(sub_sd, 0x0100, 0x00);//stream off
			break;
		case OV7695_PIP_STREAM_ON:
			OV7695_write(sub_sd, 0x0100, 0x01);//stream on
			break;
		case OV7695_READ_CHIP_VERSION:
//			OV7695_read(sub_sd, 0x302a, &value);
			
		default:
				break;
	}

	return rc;
}
EXPORT_SYMBOL(pip_ov7695_ctrl);



static int OV7695_reset(struct v4l2_subdev *sd, u32 val)
{
	printk(KERN_INFO "%s: \n",__func__);
	return 0;
}

void sensor_setting_read_check(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char tmp;
	ret = OV7695_read(sd, 0x550c, &tmp);
	printk("ret = %d\n",ret);
	printk("0x550c =%x\n",tmp);	
		ret = OV7695_read(sd, 0x3632, &tmp);
	printk("0x3632 =%x\n",tmp);	
			ret = OV7695_read(sd, 0x300a, &tmp);
	printk("0x300a =%x\n",tmp);	
			ret = OV7695_read(sd, 0x300b, &tmp);
	printk("0x300b =%x\n",tmp);	

}

static int OV7695_get_shutter(struct v4l2_subdev *sd)
{
	int ret,shutter= 0;
	unsigned char tmp;
	ret = OV7695_read(sd, 0x3500, &tmp);
	shutter = tmp<<8;
	ret = OV7695_read(sd, 0x3501, &tmp);
	shutter = (shutter + tmp)<<4;
	ret = OV7695_read(sd, 0x3502, &tmp);
	shutter = shutter + (tmp>>4);
	printk("shutter = %x\n",shutter);
	return shutter;
}

static int OV7695_set_shutter(struct v4l2_subdev *sd,int shutter)
{
	int ret;
	unsigned char tmp;
	shutter = shutter&0xffff;
	tmp = shutter&0x0f;
	tmp = tmp<<4;
	ret = OV7695_write(sd, 0x3502, tmp);
	tmp  = (shutter>>4)&0xff;
//	tmp = tmp>>4;
	printk("tmp = 0x%x\n",tmp);
	ret = OV7695_write(sd, 0x3501, tmp);
	tmp = shutter >>12;
	ret = OV7695_write(sd, 0x3500, tmp);
	return 0;

}

static int  OV7695_get_vts(struct v4l2_subdev *sd)
{
	unsigned char tmp;
	int vts;
	int ret;
	ret = OV7695_read(sd, 0x380e, &tmp);
	vts = tmp<<8;
	ret = OV7695_read(sd, 0x380f, &tmp);
	vts = vts + tmp;
	return vts;
}

static int  OV7695_get_hts(struct v4l2_subdev *sd)
{
	unsigned char tmp;
	int hts;
	int ret;
	ret = OV7695_read(sd, 0x380c, &tmp);
	hts = tmp<<8;
	ret = OV7695_read(sd, 0x380d, &tmp);
	hts = hts + tmp;
	printk("hts = 0x%x\n",hts);
	return hts;
}

static int OV7695_set_vts(struct v4l2_subdev *sd,int vts)
{
	unsigned char tmp;
	int ret;
	tmp = vts&0xff;
	ret = OV7695_write(sd, 0x380f, tmp);
	tmp = vts>>8;
	ret = OV7695_write(sd, 0x380e, tmp);
	return 0;
}

static int OV7695_get_gain16(struct v4l2_subdev *sd)
{
	int ret,gain16= 0;
	unsigned char tmp;
	ret = OV7695_read(sd, 0x350a, &tmp);
	gain16 = (tmp&0x03)<<8;
	ret = OV7695_read(sd, 0x350b, &tmp);
	gain16 = gain16 + tmp;
	printk("gain16 = %x\n",gain16);
	return gain16;

}

static int OV7695_set_gain16(struct v4l2_subdev *sd,int gain16)
{
	//write gain,16 = 1x
	int ret;
	unsigned char tmp;
	gain16 = gain16&0x3ff;
	tmp = gain16>>8;
	ret = OV7695_write(sd, 0x350a,tmp);
	tmp = gain16&0xff;
	ret = OV7695_write(sd, 0x350b,tmp);
	return 0;

}

int OV7695_get_light_frequency(void)
{
   	return 50;//50HZ in China
}


int OV7695_calc_capture_shutter(struct v4l2_subdev *sd)
{
	printk("calculate exposure and gain for snapshot\n");
	int light_frequency;
	int OV5645_preview_HTS, OV5645_preview_shutter, OV5645_preview_gain16;
	int OV5645_capture_HTS, OV5645_capture_VTS, OV5645_capture_shutter,
		OV5645_capture_gain16;
	int OV5645_capture_bandingfilter, OV5645_capture_max_band;
	long OV5645_preview_SCLK,OV5645_capture_SCLK;
	long OV5645_capture_gain16_shutter;
	int pre_frame_rate =31;//main camera preview framerate
	int snap_frame_rate= 10;//capture framerate
	int ret;
	unsigned char tmp;

	ret = OV7695_read(sd, 0x3501, &tmp);
	printk("ret = %d\n",ret);
	printk("0x3501 =%x\n",tmp);	
		ret = OV7695_read(sd, 0x3502, &tmp);
	printk("0x3502 =%x\n",tmp);	
			ret = OV7695_read(sd, 0x350a, &tmp);
	printk("0x350a =%x\n",tmp);	
			ret = OV7695_read(sd, 0x350b, &tmp);
	printk("0x350b =%x\n",tmp);	

	OV7695_write(sd, 0x3503,0x07);//turn off auto aec/agc
	msleep(10);
	OV5645_preview_shutter = OV7695_get_shutter(sd); // read preview shutter
	OV5645_preview_gain16 = OV7695_get_gain16(sd); // read preview gain
	OV5645_preview_SCLK = 58500000;//OV7695_get_SCLK(sd); // read preview SCLK
	OV5645_preview_HTS = OV7695_get_hts(sd); // read preview HTS

	OV5645_capture_SCLK = OV5645_preview_SCLK; // read capture SCLK
	OV5645_capture_HTS = 0xb1c; // read capture HTS
	OV5645_capture_VTS = 0x7b0; // read capture VTS
	
	// calculate capture banding filter
	light_frequency = OV7695_get_light_frequency();
	printk("light_frequency = 0x%x\n",light_frequency);
	if (light_frequency == 60) {
	// 60Hz
		OV5645_capture_bandingfilter = OV5645_capture_SCLK/(100*OV5645_capture_HTS)*100/120;
	}
	else {
	// 50Hz
		printk("OV5645_capture_HTS = 0x%x\n",OV5645_capture_HTS);
		OV5645_capture_bandingfilter = OV5645_capture_SCLK/(100*OV5645_capture_HTS);
		printk("OV5645_capture_bandingfilter = 0x%x,OV5645_capture_SCLK = 0x%x\n",OV5645_capture_bandingfilter,OV5645_capture_SCLK);
	}
	OV5645_capture_max_band = (int)((OV5645_capture_VTS - 4)/OV5645_capture_bandingfilter);
//return 0;
	printk("OV5645_capture_max_band = 0x%x\n",OV5645_capture_max_band);

	// calculate OV5645 capture shutter/gain16
	OV5645_capture_shutter = (long)(OV5645_preview_shutter *OV5645_preview_HTS/OV5645_capture_HTS*pre_frame_rate/snap_frame_rate);

	printk("OV5645_capture_shutter =0x%x\n",OV5645_capture_shutter);

	if(OV5645_capture_shutter<64) {
		printk("4-\n");
		OV5645_capture_gain16_shutter = (long)(OV5645_preview_shutter * OV5645_preview_gain16 *
		OV5645_capture_SCLK/OV5645_preview_SCLK * OV5645_preview_HTS/OV5645_capture_HTS*pre_frame_rate/snap_frame_rate);
		OV5645_capture_shutter = (int)(OV5645_capture_gain16_shutter/16);
		if(OV5645_capture_shutter <1)
			OV5645_capture_shutter = 1;
		OV5645_capture_gain16 = (int)(OV5645_capture_gain16_shutter/OV5645_capture_shutter);
		if(OV5645_capture_gain16<16)
			OV5645_capture_gain16 = 16;
	}
	else {
		printk("4+\n");
		OV5645_capture_gain16_shutter = (long)OV5645_preview_gain16 * OV5645_capture_shutter;
		// gain to shutter
		if(OV5645_capture_gain16_shutter < (OV5645_capture_bandingfilter*16)) {
		// shutter < 1/100
		OV5645_capture_shutter = OV5645_capture_gain16_shutter/16;
		if(OV5645_capture_shutter <1)
		OV5645_capture_shutter = 1;
		OV5645_capture_gain16 = OV5645_capture_gain16_shutter/OV5645_capture_shutter;
		if(OV5645_capture_gain16<16)
		OV5645_capture_gain16 = 16;
		}
		else {
			if(OV5645_capture_gain16_shutter > (OV5645_capture_bandingfilter*OV5645_capture_max_band*16)) {

				// exposure reach max
				OV5645_capture_shutter = OV5645_capture_bandingfilter*OV5645_capture_max_band;
				OV5645_capture_gain16 = OV5645_capture_gain16_shutter/OV5645_capture_shutter;
			}
			else {
				// 1/100 < capture_shutter =< max, capture_shutter = n/100
				OV5645_capture_shutter = ((int) (OV5645_capture_gain16_shutter/16/OV5645_capture_bandingfilter)) *
				OV5645_capture_bandingfilter;
				OV5645_capture_gain16 = OV5645_capture_gain16_shutter/OV5645_capture_shutter;
			}
		}
	}
	printk("5\n");
	printk("OV5645_capture_gain16 = 0x%x,OV5645_capture_shutter =0x%x\n",OV5645_capture_gain16,OV5645_capture_shutter);

	// write capture gain
	OV7695_set_gain16(sd,OV5645_capture_gain16);
	
	// write capture shutter
	if (OV5645_capture_shutter > (OV5645_capture_VTS-4)) {
		OV5645_capture_VTS = OV5645_capture_shutter + 4;
		OV7695_set_vts(sd,OV5645_capture_VTS);
	}
	OV7695_set_shutter(sd,OV5645_capture_shutter);

	ret = OV7695_read(sd, 0x3501, &tmp);
	printk("ret = %d\n",ret);
	printk("0x3501 =%x\n",tmp);	
		ret = OV7695_read(sd, 0x3502, &tmp);
	printk("0x3502 =%x\n",tmp);	
			ret = OV7695_read(sd, 0x350a, &tmp);
	printk("0x350a =%x\n",tmp);	
			ret = OV7695_read(sd, 0x350b, &tmp);
	printk("0x350b =%x\n",tmp);	

	// calculate capture banding filter
	//OV5645_stream_on();
	// skip 2 vsync
	// start capture at 3rd vsync
	printk("OV5645_capture_max_band = 0x%x\n",OV5645_capture_max_band);
	printk("OV5645_capture_gain16 = 0x%x,OV5645_capture_shutter =0x%x\n",OV5645_capture_gain16,OV5645_capture_shutter);
	return 0;
}

static int OV7695_init(struct v4l2_subdev *sd, u32 val)
{
	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);
	struct regval_list ov7695_init_setting[500];
	u8 tmp;
	int ret;
	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;
	info->dynamic_framerate = 0;
	info->vts_address1 = 0x0340;
	info->vts_address2 = 0x0341;
	info->max_height = MAX_HEIGHT;
	info->max_width = MAX_WIDTH;
	printk(KERN_INFO "%s: \n",__func__);

	pip_ov5648_ctrl(OV5648_PIP_SUB_INIT, NULL); //loading ov5645 initialized setting
	msleep(10);
	OV7695_write_array(sd, regSub_Init_OV7695_VGA);
	pip_ov5648_ctrl(OV5648_PIP_WAKEUP, NULL); //wake up ov5645

	return 0;
}

static int OV7695_detect(struct v4l2_subdev *sd)
{	
	int ret;
	u8 temp;
	u16 chip_id;	
	ret = OV7695_read(sd, 0x300a, &temp);
	chip_id = temp << 8;	
	ret = OV7695_read(sd, 0x300b, &temp);
	chip_id |= temp;
	
	printk("%s sensor id 0x%x", __func__, chip_id);
	if (chip_id != OV7695_CHIP_ID)
		return -ENODEV;

	return 0;
}

static struct OV7695_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} OV7695_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV7695_FMTS ARRAY_SIZE(OV7695_formats)

static struct OV7695_win_size {
	int	width;
	int	height;
	int vts;
	int framerate;
	struct regval_list *regs;
} OV7695_win_sizes[] = {
/* VGA */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x2e4,
		.framerate      = 30,
		.regs 		= OV7695_win_vga,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(OV7695_win_sizes))

static int OV7695_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV7695_FMTS)
		return -EINVAL;

	*code = OV7695_formats[index].mbus_code;
	return 0;
}

static int OV7695_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct OV7695_format_struct **ret_fmt,
		struct OV7695_win_size **ret_wsize)
{
	int index;
	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);
	struct OV7695_win_size *wsize;
	struct OV7695_win_size *P_win;
	int Num_win = 0;

	printk(KERN_INFO "%s: fmt->code = %d; fmt->width = %d;fmt->height = %d; \n",__func__,fmt->code,fmt->width,fmt->height);
	for (index = 0; index < N_OV7695_FMTS; index++)
		if (OV7695_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV7695_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = OV7695_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = OV7695_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	P_win = OV7695_win_sizes;	
	Num_win = N_WIN_SIZES;

	for (wsize = OV7695_win_sizes; wsize < OV7695_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= OV7695_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = OV7695_formats[index].colorspace;
	return 0;
}

static int OV7695_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return OV7695_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int OV7695_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct OV7695_format_struct *fmt_s;
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct OV7695_win_size *wsize;
	int ret;
	int len_sub,len_main,i;
	printk(KERN_INFO "%s: fmt->code = %d; fmt->width = %d;fmt->height = %d; \n",__func__,fmt->code,fmt->width,fmt->height);
	ret = OV7695_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = OV7695_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;	
	}	

	if ((info->win != wsize) && wsize->regs) {
		printk("%s;%dX%d;",__func__,wsize->width,wsize->height);
	}    
	memset(data, 0, sizeof(*data));

	data->mipi_clk = 420; /* Mbps. */
	data->vts = wsize->vts;	
	data->framerate = wsize->framerate;
	data->binning = info->binning;
	data->sensor_vts_address1 = info->vts_address1;
	data->sensor_vts_address2 = info->vts_address2;	
	data->reg_num = 0;
	info->fmt = fmt_s;
	info->win = wsize;
	printk("%s------------- \n", __func__);
	return 0;
}


static int OV7695_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	printk(KERN_INFO "%s: %d; \n",__func__,enable);

	if (enable) {		
		pip_ov5648_ctrl(OV5648_PIP_STREAM_ON, NULL);
		
	} else {
		pip_ov5648_ctrl(OV5648_PIP_STREAM_OFF, NULL);
	}	    

	return ret;
}

static int OV7695_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= info->max_width;
	a->c.height	= info->max_height;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int OV7695_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);
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

static int OV7695_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int OV7695_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int OV7695_frame_rates[] = { 30, 15, 10, 5, 1 };

static int OV7695_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(OV7695_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = OV7695_frame_rates[interval->index];
	return 0;
}

static int OV7695_enum_framesizes(struct v4l2_subdev *sd,
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
		struct OV7695_win_size *win = &OV7695_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int OV7695_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int OV7695_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_INFO "%s: [OV7695]set brightness %d\n",__func__, brightness);
	return 0;

	switch (brightness) {
	case BRIGHTNESS_L3:
		OV7695_write_array(sd, OV7695_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		OV7695_write_array(sd, OV7695_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		OV7695_write_array(sd, OV7695_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		OV7695_write_array(sd, OV7695_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		OV7695_write_array(sd, OV7695_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		OV7695_write_array(sd, OV7695_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		OV7695_write_array(sd, OV7695_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int OV7695_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk(KERN_INFO "%s:[OV7695]set contrast %d\n",__func__ ,contrast);
	return 0;

	switch (contrast) {
	case CONTRAST_L3:
		OV7695_write_array(sd, OV7695_contrast_l3);
		break;
	case CONTRAST_L2:
		OV7695_write_array(sd, OV7695_contrast_l2);
		break;
	case CONTRAST_L1:
		OV7695_write_array(sd, OV7695_contrast_l1);
		break;
	case CONTRAST_H0:
		OV7695_write_array(sd, OV7695_contrast_h0);
		break;
	case CONTRAST_H1:
		OV7695_write_array(sd, OV7695_contrast_h1);
		break;
	case CONTRAST_H2:
		OV7695_write_array(sd, OV7695_contrast_h2);
		break;
	case CONTRAST_H3:
		OV7695_write_array(sd, OV7695_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int OV7695_set_white_balance(struct v4l2_subdev *sd, int white_balance)
{
	printk(KERN_INFO "%s: %d;\n",__func__ ,white_balance);
	return 0;

	switch (white_balance) {
	case WHITE_BALANCE_AUTO:
		OV7695_write_array(sd, OV7695_white_balance_auto);
		break;
	case WHITE_BALANCE_INCANDESCENT:
		OV7695_write_array(sd, OV7695_white_balance_incandescent);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		OV7695_write_array(sd, OV7695_white_balance_fluorescent);
		break;
	case WHITE_BALANCE_WARM_FLUORESCENT:
		OV7695_write_array(sd, OV7695_white_balance_warm_flourescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		OV7695_write_array(sd, OV7695_white_balance_daylight);
		break;
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		OV7695_write_array(sd, OV7695_white_balance_cloudy_daylight);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int OV7695_set_frame_rate(struct v4l2_subdev *sd, int frame_rate)
{
	printk(KERN_INFO "%s: %d;\n",__func__ ,frame_rate);
	return 0;
}
static int OV7695_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);
	int ret = 0;
	struct v4l2_isp_setting isp_setting;

	printk(KERN_INFO "%s:  %x;\n",__func__, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ctrl->value = info->brightness;
		break;
	case V4L2_CID_FRAME_RATE:
		ctrl->value = info->frame_rate;
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		ctrl->value = info->white_balance;
		break;
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

static int OV7695_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);
	int ret = 0;

	printk(KERN_INFO "%s: %x;\n",__func__ ,ctrl->id);
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = OV7695_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = OV7695_set_contrast(sd, ctrl->value);
		if (!ret)
			info->contrast = ctrl->value;
		break;
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
	case V4L2_CID_SET_FOCUS:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		ret = OV7695_set_white_balance(sd, ctrl->value);
		if (!ret)
			info->white_balance = ctrl->value;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int OV7695_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = OV7695_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int OV7695_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	OV7695_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops OV7695_core_ops = {
	//.g_chip_ident = OV7695_g_chip_ident,
	.g_ctrl = OV7695_g_ctrl,
	.s_ctrl = OV7695_s_ctrl,
	.queryctrl = OV7695_queryctrl,
	.reset = OV7695_reset,
	.init = OV7695_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = OV7695_g_register,
	.s_register = OV7695_s_register,
#endif
};

static const struct v4l2_subdev_video_ops OV7695_video_ops = {
	.enum_mbus_fmt = OV7695_enum_mbus_fmt,
	.try_mbus_fmt = OV7695_try_mbus_fmt,
	.s_mbus_fmt = OV7695_s_mbus_fmt,
	.s_stream = OV7695_s_stream,
	.cropcap = OV7695_cropcap,
	.g_crop	= OV7695_g_crop,
	.s_parm = OV7695_s_parm,
	.g_parm = OV7695_g_parm,
	.enum_frameintervals = OV7695_enum_frameintervals,
	.enum_framesizes = OV7695_enum_framesizes,
};

static const struct v4l2_subdev_ops OV7695_ops = {
	.core = &OV7695_core_ops,
	.video = &OV7695_video_ops,
};

static int OV7695_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct OV7695_info *info;
	int ret;

	info = kzalloc(sizeof(struct OV7695_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &OV7695_ops);


	sub_sd = sd;
	/* Make sure it's an OV7695 */
	ret = OV7695_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an OV7695 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}

	v4l_info(client, "OV7695 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int OV7695_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct OV7695_info *info = container_of(sd, struct OV7695_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id OV7695_id[] = {
	{ "ov7695_raw_pip", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, OV7695_id);

static struct i2c_driver OV7695_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov7695_raw_pip",
	},
	.probe		= OV7695_probe,
	.remove		= OV7695_remove,
	.id_table	= OV7695_id,
};

static __init int init_OV7695(void)
{
	return i2c_add_driver(&OV7695_driver);
}

static __exit void exit_OV7695(void)
{
	i2c_del_driver(&OV7695_driver);
}

fs_initcall(init_OV7695);
module_exit(exit_OV7695);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore OV7695 sensors");
MODULE_LICENSE("GPL");
