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
#include "gc2035_mipi.h"

#define GC2035_MIPI_CHIP_ID_H	(0x20)
#define GC2035_MIPI_CHIP_ID_L	(0x35)
#define VGA_WIDTH		648
#define VGA_HEIGHT		480
#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH	VGA_WIDTH//MAX_WIDTH//SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  VGA_HEIGHT//MAX_HEIGHT//SXGA_HEIGHT
#define V4L2_I2C_ADDR_8BIT	(0x0001)
#define GC2035_MIPI_REG_END		0xff

struct gc2035_mipi_format_struct;

struct gc2035_mipi_info {
	struct v4l2_subdev sd;
	struct gc2035_mipi_format_struct *fmt;
	struct gc2035_mipi_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int brightness;
	int contrast;
	int saturation;
	int effect;
	int wb;
	int exposure;
	int previewmode;
	int frame_rate;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct otp_struct {
	unsigned int customer_id;
	unsigned int module_integrator_id;
	unsigned int lens_id;
	unsigned int rg_ratio;
	unsigned int bg_ratio;
	unsigned int user_data[5];
};


enum preview_mode {
	preview_auto = 0,
	normal = 16,
	dynamic = 1,
	portrait = 2,
	landscape = 3,
	nightmode = 4,
	nightmode_portrait = 5,
	theatre = 6,
	beatch = 7,
	snow = 8,
	sunset = 9,
	anti_shake = 10,
	fireworks = 11,
	sports = 12,
	party = 13,
	candy = 14,
	bar = 15,
};


static struct regval_list gc2035_mipi_init_regs[] = {
	{0xfe, 0x80},  
	{0xfe, 0x80},  
	{0xfe, 0x80},  
	{0xfc, 0x06},  
	{0xf9, 0xfe},  
	{0xfa, 0x00},  
	{0xf6, 0x00},  
	{0xf7, 0x05},//0d
	{0xf8, 0x86}, //82 //
	{0xfe, 0x00},  
	{0x82, 0x00},  
	{0xb3, 0x60},  
	{0xb4, 0x40},  
	{0xb5, 0x60},
	{0x03, 0x02},
	{0x04, 0x8a},
	{0xfe, 0x00},  
	{0xec, 0x04},  
	{0xed, 0x04},  
	{0xee, 0x60},  
	{0xef, 0x90},  
	{0x0a, 0x00},  
	{0x0c, 0x00},  
	{0x0d, 0x04},  
	{0x0e, 0xc0},  
	{0x0f, 0x06},  
	{0x10, 0x58},  
	{0x17, 0x14},//0x14
	{0x18, 0x0a},  
	{0x19, 0x0c},  
	{0x1a, 0x01},  
	{0x1b, 0x48},  
	{0x1e, 0x88},  
	{0x1f, 0x0f},  
	{0x20, 0x05},  
	{0x21, 0x0f},  
	{0x22, 0xf0},  
	{0x23, 0xc3},  
	{0x24, 0x16},  
	{0xfe, 0x01},  
	{0x09, 0x00},  
	{0x0b, 0x90},  
	{0x13, 0x75},  
	{0xfe, 0x00},  
	{0xfe, 0x00},  
	{0x05, 0x01},//
	{0x06, 0x11},// 0d
	{0x07, 0x00},//12.9
	{0x08, 0x54},//40
	{0xfe, 0x01},  
	{0x27, 0x00},//
	{0x28, 0x82},//0xa2  7e
	{0x29, 0x04},//12.5
	{0x2a, 0x10},
	{0x2b, 0x05},//11.1
	{0x2c, 0x14},
	{0x2d, 0x05},//
	{0x2e, 0x14},
	{0x2f, 0x08},//
	{0x30, 0x20},
	{0x3e, 0x40},//
	{0xfe, 0x00},
	{0xb6, 0x03},
	{0xfe, 0x00},
	{0x3f, 0x00},
	{0x40, 0x77},
	{0x42, 0x7f},
	{0x43, 0x30},
	{0x5c, 0x08},
	{0x5e, 0x20},
	{0x5f, 0x20},
	{0x60, 0x20},
	{0x61, 0x20},
	{0x62, 0x20},
	{0x63, 0x20},
	{0x64, 0x20},
	{0x65, 0x20},
	{0x66, 0x20},
	{0x67, 0x20},
	{0x68, 0x20},
	{0x69, 0x20},
	{0x90, 0x01},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	{0xfe, 0x03},
	{0x42, 0x80},
	{0x43, 0x06},
	{0x41, 0x00},
	{0x40, 0x00},
	{0x17, 0x01},
	{0xfe, 0x00},  
	{0x80, 0xff},
	{0x81, 0x26},
	{0x03, 0x02},
	{0x04, 0x8a},
	{0x84, 0x02},
	{0x86, 0x06},
	{0x87, 0x80},  
	{0x8b, 0xbc},  
	{0xa7, 0x80},  
	{0xa8, 0x80},  
	{0xb0, 0x80},  
	{0xc0, 0x40},  
	{0xfe, 0x01},
	///////////////////////////////////////////////
	///////////////// 2035 LSC SET ////////////////
	///////////////////////////////////////////////
	{0xfe,0x01},
	{0xa1,0x80},  // center_row
	{0xa2,0x80},  // center_col
	{0xa4,0x77},  // sign of b1
	{0xa5,0x77},  // sign of b1
	{0xa6,0x77},  // sign of b4
	{0xa7,0x77},  // sign of b4
	{0xa8,0x70},  // sign of b22
	{0xa9,0x00},  // sign of b22
	{0xaa,0x12},  // Q1_b1 of R
	{0xab,0x12},  // Q1_b1 of G
	{0xac,0x09},  // Q1_b1 of B
	{0xad,0x24},  // Q2_b1 of R
	{0xae,0x22},  // Q2_b1 of G
	{0xaf,0x10},  // Q2_b1 of B
	{0xb0,0x3a},  // Q3_b1 of R
	{0xb1,0x3a},  // Q3_b1 of G
	{0xb2,0x30},  // Q3_b1 of B
	{0xb3,0x1d},  // Q4_b1 of R
	{0xb4,0x17},  // Q4_b1 of G
	{0xb5,0x0e},  // Q4_b1 of B
	{0xb6,0x5c},  // right_b2 of R
	{0xb7,0x53},  // right_b2 of G
	{0xb8,0x2d},  // right_b2 of B
	{0xb9,0x25},  // right_b4 of R
	{0xba,0x2d},  // right_b4 of G
	{0xbb,0x0c},  // right_b4 of B
	{0xbc,0x52},  // left_b2 of R
	{0xbd,0x38},  // left_b2 of G
	{0xbe,0x30},  // left_b2 of B
	{0xbf,0x10},  // left_b4 of R
	{0xc0,0x06},  // left_b4 of G
	{0xc1,0x09},  // left_b4 of B
	{0xc2,0x48},  // up_b2 of R
	{0xc3,0x3e},  // up_b2 of G
	{0xc4,0x32},  // up_b2 of B
	{0xc5,0x30},  // up_b4 of R
	{0xc6,0x34},  // up_b4 of G
	{0xc7,0x2b},  // up_b4 of B
	{0xc8,0x35},  // down_b2 of R
	{0xc9,0x2a},  // down_b2 of G
	{0xca,0x1b},  // down_b2 of B
	{0xcb,0x2e},  // down_b4 of R
	{0xcc,0x1e},  // down_b4 of G
	{0xcd,0x16},  // down_b4 of B
	{0xd0,0x02},  // right_up_b22 of R
	{0xd2,0x0b},  // right_up_b22 of G
	{0xd3,0x0c},  // right_up_b22 of B
	{0xd4,0x57},  // right_down_b22 of R
	{0xd6,0x3e},  // right_down_b22 of G
	{0xd7,0x49},  // right_down_b22 of B
	{0xd8,0x7b},  // left_up_b22 of R
	{0xda,0x58},  // left_up_b22 of G
	{0xdb,0x53},  // left_up_b22 of B
	{0xdc,0x89},  // left_down_b22 of R
	{0xde,0x7b},  // left_down_b22 of G
	{0xdf,0x79},  // left_down_b22 of B
	{0xfe,0x00},
	{0xfe, 0x02},  
	{0xa4, 0x00},  
	{0xfe, 0x00},  
	{0xfe, 0x02},  
	{0xc0, 0x01},  
	{0xc1, 0x38},
	{0xc2, 0xfc},
	{0xc3, 0x05},  
	{0xc4, 0xe8},
	{0xc5, 0x42},  
	{0xc6, 0xf8},  
	{0xc7, 0x38},
	{0xc8, 0xf8},  
	{0xc9, 0x06},  
	{0xca, 0xf9},
	{0xcb, 0x3e},  
	{0xcc, 0xf3},
	{0xcd, 0x36},  
	{0xce, 0xf6},  
	{0xcf, 0x04},  
	{0xe3, 0x0c},  
	{0xe4, 0x44},  
	{0xe5, 0xe5}, 
	{0xfe, 0x00}, 
	{0xfe, 0x01},
	{0x4f, 0x00}, 
	{0x4d, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x10}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4d, 0x20},  ///////////////20
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x30}, //////////////////30
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x02},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x40},  //////////////////////40
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x04},
	{0x4e, 0x02},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x50}, //////////////////50
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x08}, 
	{0x4e, 0x08},
	{0x4e, 0x04},
	{0x4e, 0x04},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x60}, /////////////////60
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x20},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x70}, ///////////////////70
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x20},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0x80}, /////////////////////80
	{0x4e, 0x00},
	{0x4e, 0x20},
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 		  
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0x90}, //////////////////////90
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0xa0}, /////////////////a0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0xb0}, //////////////////////////////////b0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0xc0}, //////////////////////////////////c0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0xd0}, ////////////////////////////d0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4d, 0xe0}, /////////////////////////////////e0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4d, 0xf0}, /////////////////////////////////f0
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00}, 
	{0x4e, 0x00}, 
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4e, 0x00},
	{0x4f, 0x01},   
	{0x50, 0x80},
	{0x56, 0x06},
	{0x52, 0x40},
	{0x54, 0x60},
	{0x57, 0x20},
	{0x58, 0x01}, 
	{0x5b, 0x08},
	{0x61, 0xaa},
	{0x62, 0xaa},
	{0x71, 0x00},
	{0x72, 0x25},
	{0x74, 0x10},
	{0x77, 0x08},
	{0x78, 0xfd},
	{0x86, 0x30},
	{0x87, 0x00},
	{0x88, 0x04},
	{0x8a, 0xc0},
	{0x89, 0x71},
	{0x84, 0x08},
	{0x8b, 0x00},
	{0x8d, 0x70},
	{0x8e, 0x70},
	{0x8f, 0xf4},
	{0xfe, 0x00},  
	{0x82, 0x02},  
	{0xfe, 0x01},  
	{0x21, 0xbf},  
	{0xfe, 0x02},  
	{0xa5, 0x50}, 
	{0xa2, 0xb0},  
	{0xa6, 0x50},  
	{0xa7, 0x30},  
	{0xab, 0x31},  
	{0x88, 0x3f},
	{0xa9, 0x6c},  
	{0xb0, 0x55},  
	{0xb3, 0x70},  
	{0x8c, 0xf6},  
	{0x89, 0x01},//03
	{0xde, 0xb6},  
	{0x38, 0x08},  
	{0x39, 0x50},  
	{0xfe, 0x00},  
	{0x81, 0x26},  
	{0x87, 0x80},  
	{0xfe, 0x02},  
	{0x83, 0x00},  
	{0x97, 0x45}, //22
	{0x84, 0x45},  
	{0xd1, 0x32},
	{0xd2, 0x32},
	{0xdd, 0x38},  
	{0xfe, 0x00},  
	{0xfe, 0x02},  
	{0x2b, 0x00},  
	{0x2c, 0x04},  
	{0x2d, 0x09},  
	{0x2e, 0x18},  
	{0x2f, 0x27},  
	{0x30, 0x37},  
	{0x31, 0x49},  
	{0x32, 0x5c},  
	{0x33, 0x7e},  
	{0x34, 0xa0},  
	{0x35, 0xc0},  
	{0x36, 0xe0},  
	{0x37, 0xff},  
	{0xfe, 0x00},  
	{0x82, 0xfe},
	{0xc8, 0x00},
	{0xfa, 0x00},
	{0x90, 0x01},
	{0x95, 0x01},
	{0x96, 0xe0},
	{0x97, 0x02},
	{0x98, 0x80},
	{0xfe, 0x03},
	{0x12, 0x00},
	{0x13, 0x05},
	{0x04, 0x90},
	{0x05, 0x00},
	{0xfe, 0x00},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf4, 0x00},
	{0xf5, 0x00},
	{0xfe, 0x01},
	{0x0b, 0x90},
	{0x87, 0x10},
	{0xfe, 0x00},
	{0xfe, 0x03},
	{0x01, 0x03},
	{0x02, 0x37},
	{0x03, 0x10},
	{0x06, 0x90},//leo changed
	{0x11, 0x1E},
	{0x12, 0x80},
	{0x13, 0x0c},
	{0x15, 0x10},//10
	{0x04, 0x20},
	{0x05, 0x00},
	{0x17, 0x00},
	{0x21, 0x01},
	{0x22, 0x03},
	{0x23, 0x01},
	{0x29, 0x01},
	{0x2a, 0x01},
	{0x10, 0x00},//94
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2035_mipi_stream_on[] = {
	{0xfe, 0x03},
	{0x10, 0x94},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2035_mipi_stream_off[] = {
	/* Sensor enter LP11*/
	{0xfe, 0x03},
	{0x10, 0x00},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2035_mipi_win_vga[] = {
	{0xc8, 0x00},
	{0xfa, 0x00},
	{0x03, 0x02},
	{0x04, 0x8a},
	{0x99, 0x22},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x95, 0x01},
	{0x96, 0xe0},
	{0x97, 0x02},
	{0x98, 0x88},
	{0xfe, 0x03},
	{0x12, 0x10},
	{0x13, 0x05},
	{0x04, 0x90},
	{0x05, 0x00},
	{0xfe, 0x00},
	{0xb6, 0x03},
	{0xfe, 0x03},
	{0x10, 0x94},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2035_mipi_win_sxga[] = {
	{0xc8, 0x00},
	{0xfa, 0x11},
	{0x99, 0x55},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x01},
	{0x9e, 0x23},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x01},
	{0xa2, 0x23},
	{0x90, 0x01},
	{0x95, 0x03},
	{0x96, 0xc0},
	{0x97, 0x05},
	{0x98, 0x00},
	{0xfe, 0x03},
	{0x12, 0x00},
	{0x13, 0x0a},
	{0x04, 0x90},
	{0x05, 0x01},
	{0xfe, 0x00},
	{0xfe, 0x03},
	{0x10, 0x94},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2035_mipi_win_2m[] = {
	{0xc8, 0x00},
	{0xfa, 0x11},
	{0x99, 0x11},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	{0xfe, 0x03},
	{0x12, 0x80},
	{0x13, 0x0c},
	{0x04, 0x20},
	{0x05, 0x00},
	{0xfe, 0x00},
#if defined(GC2035_AUTO_LSC_CAL)
	{0xfe,0x00},
	{0x80,0x08},// 09 if tunning AWB
	{0x81,0x00},
	{0x82,0x00},
	{0xa3,0x80},
	{0xa4,0x80},
	{0xa5,0x80},
	{0xa6,0x80},
	{0xa7,0x80},
	{0xa8,0x80},
	{0xa9,0x80},
	{0xaa,0x80},
	{0xad,0x80},
	{0xae,0x80},
	{0xaf,0x80},
	{0xb3,0x40},
	{0xb4,0x40},
	{0xb5,0x40},
	{0xfe,0x01},
	{0x0a,0x40},
	{0x13,0x48},
	{0x9f,0x40},
	{0xfe,0x02},
	{0xd0,0x40},
	{0xd1,0x20},
	{0xd2,0x20},
	{0xd3,0x40},
	{0xd5,0x00},
	{0xdd,0x00},
	{0xfe,0x00},
#endif
	{0xfe, 0x03},
	{0x10, 0x94},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2035_mipi_brightness_l3[] = {
	{0xfe , 0x02},{0xd5 , 0xc0},{0xfe , 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_brightness_l2[] = {
	{0xfe , 0x02},{0xd5 , 0xe0},{0xfe , 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_brightness_l1[] = {
	{0xfe , 0x02},{0xd5 , 0xf0},{0xfe , 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_brightness_h0[] = {
	{0xfe , 0x02},{0xd5 , 0x00},{0xfe , 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_brightness_h1[] = {
	{0xfe , 0x02},{0xd5 , 0x08},{0xfe , 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_brightness_h2[] = {
	{0xfe , 0x02},{0xd5 , 0x20},{0xfe , 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_brightness_h3[] = {
	{0xfe , 0x02},{0xd5 , 0x30},{0xfe , 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_saturation_l2[] = {
	{0xfe,0x02},
	{0xd1,0x20},
	{0xd2,0x20},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_saturation_l1[] = {
	{0xfe,0x02},
	{0xd1,0x28},
	{0xd2,0x28},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_saturation_h0[] = {
	{0xfe,0x02},
	{0xd1,0x32},
	{0xd2,0x32},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_saturation_h1[] = {
	{0xfe,0x02},
	{0xd1,0x40},
	{0xd2,0x40},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_saturation_h2[] = {
	{0xfe,0x02},
	{0xd1,0x48},
	{0xd2,0x48},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list gc2035_mipi_contrast_l3[] = {
	{0xfe,0x02}, {0xd3,0x30}, {0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_contrast_l2[] = {
	{0xfe,0x02}, {0xd3,0x34}, {0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_contrast_l1[] = {
	{0xfe,0x02}, {0xd3,0x38}, {0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_contrast_h0[] = {
	{0xfe,0x02}, {0xd3,0x3c}, {0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_contrast_h1[] = {
	{0xfe,0x02}, {0xd3,0x44}, {0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_contrast_h2[] = {
	{0xfe,0x02}, {0xd3,0x48}, {0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_contrast_h3[] = {
	{0xfe,0x02}, {0xd3,0x50}, {0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list gc2035_mipi_whitebalance_auto[] = {
	{0xb3,0x61}, {0xb4,0x40}, {0xb5,0x61},{0x82,0xfe},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_whitebalance_incandescent[] = {
	{0x82,0xfc},{0xb3,0x50}, {0xb4,0x40}, {0xb5,0xa8},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_whitebalance_fluorescent[] = {
	{0x82,0xfc},{0xb3,0x72}, {0xb4,0x40}, {0xb5,0x5b},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_whitebalance_daylight[] = {
	{0x82,0xfc},{0xb3,0x70}, {0xb4,0x40}, {0xb5,0x50},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_whitebalance_cloudy[] = {
	{0x82,0xfc},{0xb3,0x58}, {0xb4,0x40}, {0xb5,0x50},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list gc2035_mipi_effect_none[] = {
	{0x83,0xe0},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_effect_mono[] = {
	{0x83,0x12},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_effect_negative[] = {
	{0x83,0x01},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_effect_solarize[] = {
	{0x83 , 0x92},
	{0xff , 0xff},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_effect_sepia[] = {
	{0x83,0x82},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_effect_posterize[] = {
	{0x83,0x92},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_effect_whiteboard[] = {
	{0x83,0x12},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_effect_blackboard[] = {
	{0x83 , 0x12},
	{0xff , 0xff},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list gc2035_mipi_effect_aqua[] = {
	{0x83 , 0x62},
	{0xff , 0xff},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_exposure_l2[] = {
	{0xfe,0x01},
	{0x13,0x60},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_exposure_l1[] = {
	{0xfe,0x01},
	{0x13,0x68},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_exposure_h0[] = {
	{0xfe,0x01},
	{0x13,0x80},  // lanking  +8 88 -90
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_exposure_h1[] = {
	{0xfe,0x01},
	{0x13,0x88},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_exposure_h2[] = {
	{0xfe,0x01},
	{0x13,0x90},
	{0xfe,0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};


/////////////////////////next preview mode////////////////////////////////////

static struct regval_list gc2035_mipi_previewmode_mode00[] = {

	{0xfe, 0x01},
	{0x3e, 0x40},
	{0xfe, 0x00},
	{0xfe, 0x02},
	{0xc1, 0x38},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list gc2035_mipi_previewmode_mode01[] = {
	{0xfe, 0x01},
	{0x3e, 0x60},
	{0xfe, 0x00},
	{0xfe, 0x02},
	{0xc1, 0x40},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_previewmode_mode02[] = {// portrait
	{0xfe, 0x01},
	{0x3e, 0x01},
	{0xfe, 0x00},
	{0xfe, 0x02},
	{0xc1, 0x40},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_mipi_previewmode_mode03[] = {//landscape
	/*
	case portrait:

	*/
	{0xfe, 0x02},
	{0xc1, 0x48},
	{0xfe, 0x00},
	{GC2035_MIPI_REG_END, 0x00},	/* END MARKER */
};


///////////////////////////////preview mode end////////////////////////
static int gc2035_mipi_read(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return ret;

	*value = (unsigned char)ret;

	return 0;
}

static int gc2035_mipi_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc2035_mipi_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != GC2035_MIPI_REG_END) {
		int ret = gc2035_mipi_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int gc2035_mipi_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int gc2035_mipi_shutter(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2 ;
	unsigned short v;

	gc2035_mipi_write(sd, 0xb6,0x00);

	ret = gc2035_mipi_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;
	ret = gc2035_mipi_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2;
	v = v /2 ;
	if (v<1)
		return 0;

	gc2035_mipi_write(sd, 0x03, (v >> 8)&0xff );
	gc2035_mipi_write(sd, 0x04, v&0xff);

	msleep(500);

	return  0;
}



static int gc2035_mipi_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc2035_mipi_info *info = container_of(sd, struct gc2035_mipi_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return gc2035_mipi_write_array(sd, gc2035_mipi_init_regs);
}

static int gc2035_mipi_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = gc2035_mipi_read(sd, 0xf0, &v);
	printk("GC2035_CHIP_ID_H = %x./n",v);
	
	if (ret < 0)
		return ret;
	if (v != GC2035_MIPI_CHIP_ID_H)
		return -ENODEV;
	ret = gc2035_mipi_read(sd, 0xf1, &v);
	
	if (ret < 0)
		return ret;
	if (v != GC2035_MIPI_CHIP_ID_L)
		return -ENODEV;


	return 0;
}

static struct gc2035_mipi_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc2035_mipi_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= NULL,
	},
};
#define N_GC2035_MIPI_FMTS ARRAY_SIZE(gc2035_mipi_formats)

static struct gc2035_mipi_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
} gc2035_mipi_win_sizes[] = {
		/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= gc2035_mipi_win_vga,
	},
	/* SXGA */
	/*{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.regs 		= gc2035_mipi_win_sxga,
	},*/
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.regs 		= gc2035_mipi_win_2m,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(gc2035_mipi_win_sizes))

static int gc2035_mipi_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC2035_MIPI_FMTS)
		return -EINVAL;

	*code = gc2035_mipi_formats[index].mbus_code;
	return 0;
}

static int gc2035_mipi_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc2035_mipi_format_struct **ret_fmt,
		struct gc2035_mipi_win_size **ret_wsize)
{
	int index;
	struct gc2035_mipi_win_size *wsize;

	for (index = 0; index < N_GC2035_MIPI_FMTS; index++)
		if (gc2035_mipi_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC2035_MIPI_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc2035_mipi_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc2035_mipi_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = gc2035_mipi_win_sizes; wsize < gc2035_mipi_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc2035_mipi_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc2035_mipi_formats[index].colorspace;

	return 0;
}

static int gc2035_mipi_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc2035_mipi_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc2035_mipi_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc2035_mipi_info *info = container_of(sd, struct gc2035_mipi_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct gc2035_mipi_format_struct *fmt_s;
	struct gc2035_mipi_win_size *wsize;
	int ret,len,i = 0;

	ret = gc2035_mipi_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;


	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc2035_mipi_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {

		memset(data, 0, sizeof(*data));
		data->mipi_clk = 156;//288; /* Mbps. */

		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_8BIT;
			data->slave_addr = client->addr;
			len = ARRAY_SIZE(gc2035_mipi_win_2m);
			data->reg_num = len;  //

			msleep(400);//ppp
			for(i =0;i<len;i++) {
				data->reg[i].addr = gc2035_mipi_win_2m[i].reg_num;
				data->reg[i].data = gc2035_mipi_win_2m[i].value;
			}

			ret = gc2035_mipi_shutter(sd);
		}

		else if ((wsize->width == VGA_WIDTH)
			&& (wsize->height == VGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_8BIT;
			data->slave_addr = client->addr;
			msleep(500);
			len = ARRAY_SIZE(gc2035_mipi_win_vga);
			data->reg_num = len;  //
			for(i =0;i<len;i++) {
				data->reg[i].addr = gc2035_mipi_win_vga[i].reg_num;
				data->reg[i].data = gc2035_mipi_win_vga[i].value;
			}
		}
	}
	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int gc2035_mipi_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct gc2035_mipi_info *info = container_of(sd, struct gc2035_mipi_info, sd);

	printk("gc2035_mipi_set_framerate %d\n", framerate);

	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;

	return 0;
}


static int gc2035_mipi_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = gc2035_mipi_write_array(sd, gc2035_mipi_stream_on);
	else
		ret = gc2035_mipi_write_array(sd, gc2035_mipi_stream_off);

	msleep(150);

	return ret;
}

static int gc2035_mipi_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc2035_mipi_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	//snapshot max size
	a->bounds.width 		= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
	//snapshot max size end
	a->defrect.left 		= 0;
	a->defrect.top			= 0;
	//preview max size
	a->defrect.width		= MAX_PREVIEW_WIDTH;
	a->defrect.height		= MAX_PREVIEW_HEIGHT;
	//preview max size end
	a->type 			= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int gc2035_mipi_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2035_mipi_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2035_mipi_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc2035_mipi_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc2035_mipi_frame_rates))
		return -EINVAL;

	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc2035_mipi_frame_rates[interval->index];
	return 0;
}

static int gc2035_mipi_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc2035_mipi_win_size *win = &gc2035_mipi_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc2035_mipi_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc2035_mipi_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[GC2035]set brightness %d\n", brightness);

	switch (brightness) {
		case BRIGHTNESS_L3:
			gc2035_mipi_write_array(sd, gc2035_mipi_brightness_l3);
			break;
		case BRIGHTNESS_L2:
			gc2035_mipi_write_array(sd, gc2035_mipi_brightness_l2);
			break;
		case BRIGHTNESS_L1:
			gc2035_mipi_write_array(sd, gc2035_mipi_brightness_l1);
			break;
		case BRIGHTNESS_H0:
			gc2035_mipi_write_array(sd, gc2035_mipi_brightness_h0);
			break;
		case BRIGHTNESS_H1:
			gc2035_mipi_write_array(sd, gc2035_mipi_brightness_h1);
			break;
		case BRIGHTNESS_H2:
			gc2035_mipi_write_array(sd, gc2035_mipi_brightness_h2);
			break;
		case BRIGHTNESS_H3:
			gc2035_mipi_write_array(sd, gc2035_mipi_brightness_h3);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2035_mipi_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	switch (contrast) {
		case CONTRAST_L3:
			gc2035_mipi_write_array(sd, gc2035_mipi_contrast_l3);
			break;
		case CONTRAST_L2:
			gc2035_mipi_write_array(sd, gc2035_mipi_contrast_l2);
			break;
		case CONTRAST_L1:
			gc2035_mipi_write_array(sd, gc2035_mipi_contrast_l1);
			break;
		case CONTRAST_H0:
			gc2035_mipi_write_array(sd, gc2035_mipi_contrast_h0);
			break;
		case CONTRAST_H1:
			gc2035_mipi_write_array(sd, gc2035_mipi_contrast_h1);
			break;
		case CONTRAST_H2:
			gc2035_mipi_write_array(sd, gc2035_mipi_contrast_h2);
			break;
		case CONTRAST_H3:
			gc2035_mipi_write_array(sd, gc2035_mipi_contrast_h3);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2035_mipi_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	printk(KERN_DEBUG "[GC2035]set saturation %d\n", saturation);

	switch (saturation) {
		case SATURATION_L2:
			gc2035_mipi_write_array(sd, gc2035_mipi_saturation_l2);
			break;
		case SATURATION_L1:
			gc2035_mipi_write_array(sd, gc2035_mipi_saturation_l1);
			break;
		case SATURATION_H0:
			gc2035_mipi_write_array(sd, gc2035_mipi_saturation_h0);
			break;
		case SATURATION_H1:
			gc2035_mipi_write_array(sd, gc2035_mipi_saturation_h1);
			break;
		case SATURATION_H2:
			gc2035_mipi_write_array(sd, gc2035_mipi_saturation_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2035_mipi_set_wb(struct v4l2_subdev *sd, int wb)
{
	printk(KERN_DEBUG "[GC2035]set contrast %d\n", wb);

	switch (wb) {
		case WHITE_BALANCE_AUTO:
			gc2035_mipi_write_array(sd, gc2035_mipi_whitebalance_auto);
			break;
		case WHITE_BALANCE_INCANDESCENT:
			gc2035_mipi_write_array(sd, gc2035_mipi_whitebalance_incandescent);
			break;
		case WHITE_BALANCE_FLUORESCENT:
			gc2035_mipi_write_array(sd, gc2035_mipi_whitebalance_fluorescent);
			break;
		case WHITE_BALANCE_DAYLIGHT:
			gc2035_mipi_write_array(sd, gc2035_mipi_whitebalance_daylight);
			break;
		case WHITE_BALANCE_CLOUDY_DAYLIGHT:
			gc2035_mipi_write_array(sd, gc2035_mipi_whitebalance_cloudy);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}
static int gc2035_mipi_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk(KERN_DEBUG "[GC2035]set contrast %d\n", effect);

	switch (effect) {
		case EFFECT_NONE:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_none);
			break;
		case EFFECT_MONO:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_mono);
			break;
		case EFFECT_NEGATIVE:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_negative);
			break;
		case EFFECT_SOLARIZE:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_solarize);
			break;
		case EFFECT_SEPIA:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_sepia);
			break;
		case EFFECT_POSTERIZE:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_posterize);
			break;
		case EFFECT_WHITEBOARD:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_whiteboard);
			break;
		case EFFECT_BLACKBOARD:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_blackboard);
			break;
		case EFFECT_AQUA:
			gc2035_mipi_write_array(sd, gc2035_mipi_effect_aqua);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2035_mipi_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk(KERN_DEBUG "[GC2035]set exposure %d\n", exposure);

	switch (exposure) {
		case EXPOSURE_L2:
			gc2035_mipi_write_array(sd, gc2035_mipi_exposure_l2);
			break;
		case EXPOSURE_L1:
			gc2035_mipi_write_array(sd, gc2035_mipi_exposure_l1);
			break;
		case EXPOSURE_H0:
			gc2035_mipi_write_array(sd, gc2035_mipi_exposure_h0);
			break;
		case EXPOSURE_H1:
			gc2035_mipi_write_array(sd, gc2035_mipi_exposure_h1);
			break;
		case EXPOSURE_H2:
			gc2035_mipi_write_array(sd, gc2035_mipi_exposure_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int gc2035_mipi_set_previewmode(struct v4l2_subdev *sd, int mode)
{
	printk(KERN_INFO "MIKE:[GC2035]set_previewmode %d\n", mode);

	switch (mode) {
		case preview_auto:
		case normal:
		case dynamic:
		case beatch:
		case bar:
		case snow:
		case landscape:
			gc2035_mipi_write_array(sd, gc2035_mipi_previewmode_mode00);
			break;

		case nightmode:
		case theatre:
		case party:
		case candy:
		case fireworks:
		case sunset:
		case nightmode_portrait:
			gc2035_mipi_write_array(sd, gc2035_mipi_previewmode_mode01);
			break;

		case sports:
		case anti_shake:
			gc2035_mipi_write_array(sd, gc2035_mipi_previewmode_mode02);
			break;

		case portrait:
			gc2035_mipi_write_array(sd, gc2035_mipi_previewmode_mode03);
			break;
		default:

			return -EINVAL;
	}

	return 0;
}





static int gc2035_mipi_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2035_mipi_info *info = container_of(sd, struct gc2035_mipi_info, sd);
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ctrl->value = info->brightness;
			break;
		case V4L2_CID_CONTRAST:
			ctrl->value = info->contrast;
			break;
		case V4L2_CID_FRAME_RATE:
			ctrl->value = info->frame_rate;
			break;
		case V4L2_CID_ISP_SETTING:
			isp_setting.setting = (struct v4l2_isp_regval *)&gc2035_mipi_isp_setting;
			isp_setting.size = ARRAY_SIZE(gc2035_mipi_isp_setting);
			memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
			break;
		case V4L2_CID_ISP_PARM:
			ctrl->value = (int)&gc2035_mipi_isp_parm;
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int gc2035_mipi_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2035_mipi_info *info = container_of(sd, struct gc2035_mipi_info, sd);
	int ret = 0;

	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ret = gc2035_mipi_set_brightness(sd, ctrl->value);
			if (!ret)
				info->brightness = ctrl->value;
			break;
		case V4L2_CID_CONTRAST:
			ret = gc2035_mipi_set_contrast(sd, ctrl->value);
			if (!ret)
				info->contrast = ctrl->value;
			break;
		case V4L2_CID_SATURATION:
			ret = gc2035_mipi_set_saturation(sd, ctrl->value);
			if (!ret)
				info->saturation = ctrl->value;
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			ret = gc2035_mipi_set_wb(sd, ctrl->value);
			if (!ret)
				info->wb = ctrl->value;
			break;
		case V4L2_CID_EXPOSURE:
			ret = gc2035_mipi_set_exposure(sd, ctrl->value);
			if (!ret)
				info->exposure = ctrl->value;
			break;
		case V4L2_CID_EFFECT:
			ret = gc2035_mipi_set_effect(sd, ctrl->value);
			if (!ret)
				info->effect = ctrl->value;
			break;

		case 10094870:// preview mode add
			ret = gc2035_mipi_set_previewmode(sd, ctrl->value);
			if (!ret)
				info->previewmode = ctrl->value;

			break;

		case V4L2_CID_FRAME_RATE:
			ret = gc2035_mipi_set_framerate(sd, ctrl->value);
			break;
		case V4L2_CID_SET_FOCUS:
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}

	return ret;
}

static int gc2035_mipi_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC2035, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc2035_mipi_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc2035_mipi_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;

	return ret;
}

static int gc2035_mipi_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2035_mipi_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc2035_mipi_core_ops = {
	.g_chip_ident = gc2035_mipi_g_chip_ident,
	.g_ctrl = gc2035_mipi_g_ctrl,
	.s_ctrl = gc2035_mipi_s_ctrl,
	.queryctrl = gc2035_mipi_queryctrl,
	.reset = gc2035_mipi_reset,
	.init = gc2035_mipi_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc2035_mipi_g_register,
	.s_register = gc2035_mipi_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc2035_mipi_video_ops = {
	.enum_mbus_fmt = gc2035_mipi_enum_mbus_fmt,
	.try_mbus_fmt = gc2035_mipi_try_mbus_fmt,
	.s_mbus_fmt = gc2035_mipi_s_mbus_fmt,
	.s_stream = gc2035_mipi_s_stream,
	.cropcap = gc2035_mipi_cropcap,
	.g_crop	= gc2035_mipi_g_crop,
	.s_parm = gc2035_mipi_s_parm,
	.g_parm = gc2035_mipi_g_parm,
	.enum_frameintervals = gc2035_mipi_enum_frameintervals,
	.enum_framesizes = gc2035_mipi_enum_framesizes,
};

static const struct v4l2_subdev_ops gc2035_mipi_ops = {
	.core = &gc2035_mipi_core_ops,
	.video = &gc2035_mipi_video_ops,
};

static int gc2035_mipi_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc2035_mipi_info *info;
	int ret;

	info = kzalloc(sizeof(struct gc2035_mipi_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2035_mipi_ops);

	/* Make sure it's an gc2035_mipi */
	ret = gc2035_mipi_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc2035_mipi chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc2035_mipi chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return ret;
}

static int gc2035_mipi_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2035_mipi_info *info = container_of(sd, struct gc2035_mipi_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc2035_mipi_id[] = {
	{ "gc2035_mipi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2035_mipi_id);

static struct i2c_driver gc2035_mipi_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2035_mipi",
	},
	.probe		= gc2035_mipi_probe,
	.remove		= gc2035_mipi_remove,
	.id_table	= gc2035_mipi_id,
};

static __init int init_gc2035_mipi(void)
{
	return i2c_add_driver(&gc2035_mipi_driver);
}

static __exit void exit_gc2035_mipi(void)
{
	i2c_del_driver(&gc2035_mipi_driver);
}

fs_initcall(init_gc2035_mipi);
module_exit(exit_gc2035_mipi);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore gc2035 sensors with mipi interface");
MODULE_LICENSE("GPL");
