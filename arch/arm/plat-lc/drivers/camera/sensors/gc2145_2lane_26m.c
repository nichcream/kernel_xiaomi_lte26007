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
#include "gc2145_2lane_26m.h"

#define GC2145_CHIP_ID_H	(0x21)
#define GC2145_CHIP_ID_L	(0x45)

#define VGA_WIDTH		648
#define VGA_HEIGHT		480


#define SVGA_WIDTH		808
#define SVGA_HEIGHT		600

#define SXGA_WIDTH		1288//1288
#define SXGA_HEIGHT		720//960

#define MAX_WIDTH		1608
#define MAX_HEIGHT		1200



#define MAX_PREVIEW_WIDTH	SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  SXGA_HEIGHT

#define V4L2_I2C_ADDR_8BIT	(0x0001)
#define GC2145_REG_END		0xff
#define GC2145_REG_VAL_HVFLIP 0xfe

#define ISO_MAX_GAIN      0x50
#define ISO_MIN_GAIN      0x20

struct gc2145_format_struct;

struct gc2145_info {
	struct v4l2_subdev sd;
	struct gc2145_format_struct *fmt;
	struct gc2145_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int brightness;
	int contrast;
	int saturation;
	int effect;
	int wb;
	int exposure;
	int previewmode;
	int iso;
	int scene_mode;
	int sharpness;
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


static struct regval_list gc2145_init_regs[] = {
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfc, 0x06},
	{0xf6, 0x00},
	{0xf7, 0x1d},
	{0xf8, 0x84},
	{0xfa, 0x00},
	{0xf9, 0x8e},
	{0xf2, 0x00},
	// Analog & Cisctl
	{0xfe, 0x00},
	{0x03, 0x04},
	{0x04, 0x00},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0xc0},
	{0x0f, 0x06},
	{0x10, 0x60},
	{0x12, 0x2e},
	{0x17, GC2145_REG_VAL_HVFLIP},  // 14
	{0x18, 0x22},
	{0x19, 0x0e},
	{0x1a, 0x01},
	{0x1b, 0x4b},
	{0x1c, 0x07},
	{0x1d, 0x10},
	{0x1e, 0x88},
	{0x1f, 0x78},
	{0x20, 0x03},
	{0x21, 0x40},
	{0x22, 0xf0},
	{0x24, 0x16},
	{0x25, 0x01},
	{0x26, 0x10},
	{0x2d, 0x60},
	{0x30, 0x01},
	{0x31, 0x90},
	{0x33, 0x06},
	{0x34, 0x01},
	//  ISP reg

	{0x80, 0x7f},
	{0x81, 0x26},
	{0x82, 0xfa},
	{0x83, 0x00},
	{0x84, 0x02},
	{0x86, 0x06},
	{0x88, 0x03},
	{0x89, 0x03},
	{0x85, 0x30},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0xb0, 0x55},
	{0xc3, 0x00},
	{0xc4, 0x80},
	{0xc5, 0x90},
	{0xc6, 0x3b},
	{0xc7, 0x46},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xb6, 0x01},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	// BLK
	{0x18, 0x02},
	{0x40, 0x42},
	{0x41, 0x00},
	{0x43, 0x54},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	{0x63, 0x00},
	{0x64, 0x00},
	{0x65, 0x00},
	{0x66, 0x20},
	{0x67, 0x20},
	{0x68, 0x20},
	{0x69, 0x20},
	{0x76, 0x00},
	{0x6a, 0x04},
	{0x6b, 0x04},
	{0x6c, 0x04},
	{0x6d, 0x04},
	{0x6e, 0x04},
	{0x6f, 0x04},
	{0x70, 0x04},
	{0x71, 0x04},
	{0x76, 0x00},
	{0x72, 0xf0},
	{0x7e, 0x3c},
	{0x7f, 0x00},
	{0xfe, 0x02},
	{0x48, 0x15},
	{0x49, 0x00},
	{0x4b, 0x0b},
	{0xfe, 0x00},
	// AEC
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x09, 0x00},
	{0x0a, 0x82},
	{0x0b, 0x11},
	{0x0c, 0x10},
	{0x11, 0x10},
	{0x13, 0x68},  // 7b
	{0x17, 0x00},
	{0x1c, 0x11},
	{0x1e, 0x61},
	{0x1f, 0x35},
	{0x20, 0x40},
	{0x22, 0x40},
	{0x23, 0x20},
	{0xfe, 0x02},
	{0x0f, 0x04},
	{0xfe, 0x01},
	{0x12, 0x00},
	{0x15, 0x50},
	{0x10, 0x31},
	{0x3e, 0x28},
	{0x3f, 0xe0},
	{0x40, 0xe0},
	{0x41, 0x0f},
    // INTPEE
    {0xfe, 0x02},
	{0x90, 0x6c},
	{0x91, 0x03},
	{0x92, 0xcb},
	{0x94, 0x33},
	{0x95, 0x84},
	{0x97, 0x65},
	{0xa2, 0x11},
	{0xfe, 0x00},
	// DNDD
	{0xfe, 0x02},
	{0x80, 0xc1},
	{0x81, 0x08},
	{0x82, 0x05},
	{0x83, 0x08},
	{0x84, 0x0a},
	{0x86, 0x50},
	{0x87, 0x30},
	{0x88, 0x15},
	{0x89, 0x80},
	{0x8a, 0x60},
	{0x8b, 0x30},
	// ASDE
	{0xfe, 0x01},
	{0x21, 0x04},
	{0xfe, 0x02},
	{0xa3, 0x50},
	{0xa4, 0x20},
	{0xa5, 0x40},
	{0xa6, 0x80},
	{0xab, 0x40},
	{0xae, 0x0c},
	{0xb3, 0x46},
	{0xb4, 0x64},
	{0xb6, 0x38},
	{0xb7, 0x02},
	{0xb9, 0x28},
	{0x3c, 0x08},
	{0x3d, 0x20},
	{0x4b, 0x08},
	{0x4c, 0x20},
	{0xfe, 0x00},
	//Gamma
	{0xfe, 0x02},
	{0x10, 0x09},
	{0x11, 0x0d},
	{0x12, 0x13},
	{0x13, 0x19},
	{0x14, 0x27},
	{0x15, 0x37},
	{0x16, 0x45},
	{0x17, 0x53},
	{0x18, 0x69},
	{0x19, 0x7d},
	{0x1a, 0x8f},
	{0x1b, 0x9d},
	{0x1c, 0xa9},
	{0x1d, 0xbd},
	{0x1e, 0xcd},
	{0x1f, 0xd9},
	{0x20, 0xe3},
	{0x21, 0xea},
	{0x22, 0xef},
	{0x23, 0xf5},
	{0x24, 0xf9},
	{0x25, 0xff},
	{0xc6, 0x20},
	{0xc7, 0x2b},
	//auto gamma
	{0xfe, 0x02},
	{0x26, 0x0f},
	{0x27, 0x14},
	{0x28, 0x19},
	{0x29, 0x1e},
	{0x2a, 0x27},
	{0x2b, 0x33},
	{0x2c, 0x3b},
	{0x2d, 0x45},
	{0x2e, 0x59},
	{0x2f, 0x69},
	{0x30, 0x7c},
	{0x31, 0x89},
	{0x32, 0x98},
	{0x33, 0xae},
	{0x34, 0xc0},
	{0x35, 0xcf},
	{0x36, 0xda},
	{0x37, 0xe2},
	{0x38, 0xe9},
	{0x39, 0xf3},
	{0x3a, 0xf9},
	{0x3b, 0xff},
	//YCP
	{0xfe, 0x02},
	{0xd1, 0x34},
	{0xd2, 0x34},
	{0xd3, 0x40},
	{0xd6, 0xf0},
	{0xd7, 0x10},
	{0xd8, 0xda},
	{0xdd, 0x14},
	{0xde, 0x86},
	{0xed, 0x81},
	{0xee, 0x2b},
	{0xef, 0x3b},
	{0xd8, 0xd8},
	{0xfe, 0x01},
	{0x9f, 0x40},
	// LSC  0.8
	{0xfe, 0x01},
	{0xc2, 0x14},
	{0xc3, 0x0d},
	{0xc4, 0x0c},
	{0xc8, 0x15},
	{0xc9, 0x11},
	{0xca, 0x0a},
	{0xbc, 0x24},
	{0xbd, 0x15},
	{0xbe, 0x0b},
	{0xb6, 0x25},
	{0xb7, 0x1b},
	{0xb8, 0x15},
	{0xc5, 0x00},
	{0xc6, 0x00},
	{0xc7, 0x00},
	{0xcb, 0x00},
	{0xcc, 0x00},
	{0xcd, 0x00},
	{0xbf, 0x07},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xb9, 0x00},
	{0xba, 0x00},
	{0xbb, 0x00},
	{0xaa, 0x01},
	{0xab, 0x01},
	{0xac, 0x00},
	{0xad, 0x05},
	{0xae, 0x06},
	{0xaf, 0x0e},
	{0xb0, 0x0b},
	{0xb1, 0x07},
	{0xb2, 0x06},
	{0xb3, 0x17},
	{0xb4, 0x0e},
	{0xb5, 0x0e},
	{0xd0, 0x09},
	{0xd1, 0x00},
	{0xd2, 0x00},
	{0xd6, 0x08},
	{0xd7, 0x00},
	{0xd8, 0x00},
	{0xd9, 0x00},
	{0xda, 0x00},
	{0xdb, 0x00},
	{0xd3, 0x0a},
	{0xd4, 0x00},
	{0xd5, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0x77},
	{0xa7, 0x77},
	{0xa8, 0x77},
	{0xa9, 0x77},
	{0xa1, 0x80},
	{0xa2, 0x80},
	{0xfe, 0x01}, 
	{0xdf, 0x0d},
	{0xdc, 0x25},
	{0xdd, 0x50},
	{0xe0, 0x77},
	{0xe1, 0x80},
	{0xe2, 0x77},
	{0xe3, 0x90},
	{0xe6, 0x90},
	{0xe7, 0xa0},
	{0xe8, 0x90},
	{0xe9, 0xa0},
	{0xfe, 0x00},
	//AWB
	{0xfe, 0x01},
	{0x4f, 0x00},
	{0x4f, 0x00},
	{0x4b, 0x01},
	{0x4f, 0x00},
	{0x4c, 0x01},// D75
	{0x4d, 0x71},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x91},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x70},
	{0x4e, 0x01},
	{0x4c, 0x01},// D65
	{0x4d, 0x90},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xb0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x8f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x6f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xaf},
	{0x4e, 0x02},
	{0x4c, 0x01},// D50
	{0x4d, 0xd0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xf0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xcf},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xef},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x6e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xae},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xce},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xee},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xad},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcd},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xed},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6c},
	{0x4e, 0x03},
	{0x4c, 0x01},// CWF
	{0x4d, 0x8c},
	{0x4e, 0x03},
	{0x4c, 0x01},// CWF
	{0x4d, 0xac},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcc},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xec},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8a},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xaa},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xab},
	{0x4e, 0x04},
	{0x4c, 0x01}, // A
	{0x4d, 0xcb},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xa9},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xca},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xc9},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0x8a},
	{0x4e, 0x04},
	{0x4c, 0x01}, // H
	{0x4d, 0x89},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xeb},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x0b},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x0c},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x2c},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x2b},
	{0x4e, 0x05},
	{0x4c, 0x01},
	{0x4d, 0xea},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x0a},
	{0x4e, 0x05}, // A
	{0x4c, 0x02},
	{0x4d, 0x8b},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x2a},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x4a},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x6a},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x8a},
	{0x4e, 0x06}, // H
	{0x4c, 0x02},
	{0x4d, 0xaa},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x09},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x29},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x49},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x69},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0xcc},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xca},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xa9},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc9},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe9}, // A
	{0x4e, 0x07},
	{0x4f, 0x01},
	{0x4f, 0x01},
	{0x50, 0x80}, 
	{0x51, 0xa8},
	{0x52, 0x47},
	{0x53, 0x38},
	{0x54, 0xc7},
	{0x56, 0x0e},
	{0x58, 0x08},
	{0x5b, 0x00},
	{0x5c, 0x74},
	{0x5d, 0x8b},
	{0x61, 0xdb},
	{0x62, 0xb8},
	{0x63, 0x86},
	{0x64, 0xc0},
	{0x65, 0x04},
	{0x67, 0xa8},
	{0x68, 0xb0},
	{0x69, 0x00},
	{0x6a, 0xa8},
	{0x6b, 0xb0}, 
	{0x6c, 0xaf},
	{0x6d, 0x8b},
	{0x6e, 0x50},
	{0x6f, 0x18},
	{0x73, 0xe0},
	{0x70, 0x0d},
	{0x71, 0x68},
	{0x72, 0x81},
	{0x74, 0x01},
	{0x75, 0x01},
	{0x7f, 0x0c},
	{0x76, 0x70},
	{0x77, 0x58},
	{0x78, 0xa0},
	{0x79, 0x5e},
	{0x7a, 0x54},
	{0x7b, 0x55},
	{0xfe, 0x00},
	//CC
	{0xfe, 0x02},
	{0xc0, 0x01},
	{0xC1, 0x48},
	{0xc2, 0xF8},
	{0xc3, 0x04},
	{0xc4, 0xf0},
	{0xc5, 0x4b},
	{0xc6, 0xfd},
	{0xC7, 0x50},
	{0xc8, 0xf2},
	{0xc9, 0x00},
	{0xcA, 0xE0},
	{0xcB, 0x45},
	{0xcC, 0xec},
	{0xCd, 0x48},
	{0xce, 0xf0},
	{0xcf, 0xf0},
	{0xe3, 0x0c},
	{0xe4, 0x4b},
	{0xe5, 0xe0},
	{0xfe, 0x00},
	{0xf2, 0x00},
	//dark  sun
	{0x18, 0x22},
	{0xfe, 0x02},
	{0x40, 0xbf},
	{0x46, 0xcf},
	{0xfe, 0x00},
	//frame rate   50Hz
	{0xfe, 0x00},
	{0x05, 0x01},
	{0x06, 0x80},
	{0x07, 0x00},
	{0x08, 0x44},
	{0xfe, 0x01},
	{0x25, 0x01},
	{0x26, 0x04},
	{0x27, 0x05},
	{0x28, 0x14},//20
	{0x29, 0x06},
	{0x2a, 0x18},//16
	{0x2b, 0x08},
	{0x2c, 0x20},//13
	{0x2d, 0x0c},
	{0x2e, 0x30},//8
	{0xfe, 0x00},
	//  MIPI
	{0xfe, 0x03},
	{0x01, 0x87},
	{0x02, 0x22},
	{0x03, 0x10},
	{0x04, 0x01},
	{0x05, 0x00},
	{0x06, 0x88},
	{0x10, 0x85},
	{0x11, 0x1e},
	{0x12, 0x80},
	{0x13, 0x0c},
	{0x15, 0x10},//12
	{0x17, 0xf0},
	
	{0x21, 0x01},
	{0x22, 0x02},
	{0x23, 0x01},
	{0x24, 0x10},
	{0x29, 0x07},// 02 03 //02
	{0x2a, 0x03},//01
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_stream_on[] = {
	{0xfe, 0x03},
	{0x10, 0x95},
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_stream_off[] = {
	/* Sensor enter LP11*/
	{0xfe, 0x03},
	{0x10, 0x85},
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};



static struct regval_list gc2145_frame_svga_30fps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x2b},
	{0x07, 0x00},
	{0x08, 0x64},
	{0xfe, 0x01},
	{0x25, 0x01},
	{0x26, 0xcc},
	{0x27, 0x05},
	{0x28, 0x64},//30
	{0x29, 0x05},
	{0x2a, 0x64},//30
	{0x2b, 0x05},
	{0x2c, 0x64},//30
	{0x2d, 0x05},
	{0x2e, 0x64},//30
	{0xfe, 0x00},
    {GC2145_REG_END, 0xff},        /* END MARKER */

};

static struct regval_list gc2145_frame_svga_15fps[] = {
	{0xfe, 0x00},
	{0x05, 0x03},
	{0x06, 0xc8},
	{0x07, 0x03},
	{0x08, 0xe8},
	{0xfe, 0x01},
	{0x25, 0x01},
	{0x26, 0x64},
	{0x27, 0x09},
	{0x28, 0x0a},//20
	{0x29, 0x09},
	{0x2a, 0x0a},//16
	{0x2b, 0x09},
	{0x2c, 0x0a},//13
	{0x2d, 0x09},
	{0x2e, 0x0a},//8
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff}, 	   /* END MARKER */
	
};

static struct regval_list gc2145_frame_svga_7fps[] = {
	{0xfe, 0x00},
	{0x05, 0x07},
	{0x06, 0xb0},
	{0x07, 0x07},
	{0x08, 0xd0},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xe6},
	{0x27, 0x0c},
	{0x28, 0x94},//20
	{0x29, 0x0c},
	{0x2a, 0x94},//16
	{0x2b, 0x0c},
	{0x2c, 0x94},//13
	{0x2d, 0x0c},
	{0x2e, 0x94},//8
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},        /* END MARKER */
};

static struct regval_list gc2145_frame_svga_autofps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x2b},
	{0x07, 0x00},
	{0x08, 0x64},
	{0xfe, 0x01},
	{0x25, 0x01},
	{0x26, 0xcc},
	{0x27, 0x05},
	{0x28, 0x64},//30
	{0x29, 0x08},
	{0x2a, 0xfc},//20
	{0x2b, 0x0e},
	{0x2c, 0x60},//12
	{0x2d, 0x0e},
	{0x2e, 0x60},//12
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},   /* END MARKER */
};



static struct regval_list gc2145_frame_sxga_autofps[] = { //1280x720
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x00},
	{0x08, 0x50},
	{0xfe, 0x01},
	{0x25, 0x01},//01
	{0x26, 0x04},//04
	{0x27, 0x03},
	{0x28, 0x0c},//20
	{0x29, 0x05},
	{0x2a, 0x14},//16
	{0x2b, 0x08},
	{0x2c, 0x20},//13
	{0x2d, 0x08},
	{0x2e, 0x20},//8
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},        /* END MARKER */
};

static struct regval_list gc2145_frame_sxga_30fps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x00},
	{0x08, 0x50},
	{0xfe, 0x01},
	{0x25, 0x01},//01
	{0x26, 0x04},//04
	{0x27, 0x03},
	{0x28, 0x0c},//20
	{0x29, 0x03},
	{0x2a, 0x0c},//16
	{0x2b, 0x03},
	{0x2c, 0x0c},//13
	{0x2d, 0x03},
	{0x2e, 0x0c},//8
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},        /* END MARKER */

};


static struct regval_list gc2145_frame_sxga_15fps[] = {
	{0xfe, 0x00},
	{0x05, 0x04},
	{0x06, 0x95},
	{0x07, 0x01},
	{0x08, 0x2c},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xad},
	{0x27, 0x04},
	{0x28, 0x0e},//20
	{0x29, 0x04},
	{0x2a, 0x0e},//16
	{0x2b, 0x04},
	{0x2c, 0x0e},//13
	{0x2d, 0x04},
	{0x2e, 0x0e},//8
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},        /* END MARKER */

};
static struct regval_list gc2145_frame_sxga_7fps[] = {
	{0xfe, 0x00},
	{0x05, 0x05},
	{0x06, 0xc3},
	{0x07, 0x03},
	{0x08, 0xe8},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0x95},
	{0x27, 0x08},
	{0x28, 0x26},
	{0x29, 0x08},
	{0x2a, 0x26},
	{0x2b, 0x08},
	{0x2c, 0x26},
	{0x2d, 0x08},
	{0x2e, 0x26},
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},        /* END MARKER */

};


static struct regval_list gc2145_frame_2m_30fps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x25},
	{0x07, 0x00},
	{0x08, 0x50},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xe7},
	{0x27, 0x04},
	{0x28, 0x83},//20
	{0x29, 0x07},
	{0x2a, 0xe8},//16
	{0x2b, 0x09},
	{0x2c, 0xed},//13
	{0x2d, 0x0b},
	{0x2e, 0xbb},//8
	{0xfe, 0x00},

	{GC2145_REG_END, 0xff},        /* END MARKER */

};


static struct regval_list gc2145_frame_2m_15fps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x25},
	{0x07, 0x00},
	{0x08, 0x50},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xe7},
	{0x27, 0x04},
	{0x28, 0x83},//20
	{0x29, 0x07},
	{0x2a, 0xe8},//16
	{0x2b, 0x09},
	{0x2c, 0xed},//13
	{0x2d, 0x0b},
	{0x2e, 0xbb},//8
	{0xfe, 0x00},

	{GC2145_REG_END, 0xff},        /* END MARKER */

};

static struct regval_list gc2145_frame_2m_7fps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x25},
	{0x07, 0x00},
	{0x08, 0x50},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xe7},
	{0x27, 0x04},
	{0x28, 0x83},//20
	{0x29, 0x07},
	{0x2a, 0xe8},//16
	{0x2b, 0x09},
	{0x2c, 0xed},//13
	{0x2d, 0x0b},
	{0x2e, 0xbb},//8
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},        /* END MARKER */

};
static struct regval_list gc2145_frame_2m_autofps[] = {
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x2b},
	{0x07, 0x00},
	{0x08, 0x64},
	{0xfe, 0x01},
	{0x25, 0x01},
	{0x26, 0xcc},
	{0x27, 0x05},
	{0x28, 0x64},//30
	{0x29, 0x08},
	{0x2a, 0xfc},//20
	{0x2b, 0x0e},
	{0x2c, 0x60},//12
	{0x2d, 0x0e},
	{0x2e, 0x60},//12
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},        /* END MARKER */
};

static struct regval_list gc2145_win_svga[] = {
	{0xfa, 0x00},
	{0xfd, 0x03}, //scaler
	{0x18, 0x62},
	{0x19, 0x0e},
	{0x03, 0x0e},
	{0x04, 0x60},

	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0xc0},
	{0x0f, 0x06},
	{0x10, 0x60},

	{0xfe, 0x00},
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
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x28},
	//// AWB
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x01},
	{0x74, 0x00},

	//// AEC
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0xc0},
	{0xfe, 0x00},
	//// blk
	{0xfe, 0x00},
	{0x7e, 0x00},
	{0x7f, 0x60},
	//// Lsc 
	{0xfe, 0x01},
	{0xa0, 0x0b}, 

	{0xfe, 0x03},
	{0x12, 0x50},
	{0x13, 0x06},
	{0x04, 0x94},
	{0x05, 0x01},
	{0xfe, 0x00},

	{GC2145_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list gc2145_win_sxga[] = {//720p
	{0xfd, 0x00},
	{0xfa, 0x00},
	{0x18, 0x22},
	{0x19, 0x0e},
	//windowing//
	{0x09, 0x00},
	{0x0a, 0x90},
	{0x0b, 0x00},
	{0x0c, 0x98},
	{0x0d, 0x02},
	{0x0e, 0xf0},
	{0x0f, 0x05},
	{0x10, 0x10},

	// crop window
	{0xfe, 0x00},
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
	{0x91, 0x00},
	{0x92, 0x10},//00
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0xd0},
	{0x97, 0x05},
	{0x98, 0x08},
	// AWB
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x08},
	{0xee, 0x4e},
	{0xef, 0x58},
	{0xfe, 0x01},
	{0x74, 0x01},
	// AEC
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x9e},
	{0x03, 0x08},
	{0x04, 0x58},
	{0x05, 0x28},
	{0x06, 0x80},
	{0x07, 0x18},
	{0x08, 0x44},
	{0x0a, 0xc2},
	{0xfe, 0x00},
	//// blk 
	{0xfe, 0x00},
	{0x7e, 0x00},
	{0x7f, 0x60},
	//// Lsc 
	{0xfe, 0x01},
	{0xa0, 0x0b}, 
	// mipi
	{0xfe, 0x03},
	{0x12, 0x10},
	{0x13, 0x0a},
	{0x04, 0x20},
	{0x05, 0x00},
	{0xfe, 0x00},

	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_win_2m[] = {
	{0xfd, 0x00},
	{0xfa, 0x00},
	{0x18, 0x22},
	{0x19, 0x0e},
	// crop window
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0xc0},
	{0x0f, 0x06},
	{0x10, 0x60},

	{0xfe, 0x00},
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
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x48},
	// AWB
	{0xfe, 0x00},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xfe, 0x01},
	{0x74, 0x01},
	// AEC
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x0a, 0x82}, //c2 
	{0xfe, 0x00},
	//// blk 
	{0xfe, 0x00},
	{0x7e, 0x3c},
	{0x7f, 0x00},
	//// Lsc 
	{0xfe, 0x01},
	{0xa0, 0x03}, 
	// mipi
	{0xfe, 0x03},
	{0x12, 0x90},
	{0x13, 0x0c},
	{0x04, 0x01},
	{0x05, 0x00},
	{0xfe, 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_sharpness_l2[] = {
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_sharpness_l1[] = {
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_sharpness_h0[] = {
	{0xfe , 0x02},{0x97 , 0x65},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_sharpness_h1[] = {
	{0xfe , 0x02},{0x97 , 0x76},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_sharpness_h2[] = {
	{0xfe , 0x02},{0x97 , 0x87},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_sharpness_h3[] = {
	{0xfe , 0x02},{0x97 , 0x98},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_sharpness_h4[] = {
	{0xfe , 0x02},{0x97 , 0xa9},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_sharpness_h5[] = {
	{0xfe , 0x02},{0x97 , 0xba},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_sharpness_h6[] = {
	{0xfe , 0x02},{0x97 , 0xcb},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_sharpness_h7[] = {
	{0xfe , 0x02},{0x97 , 0xdc},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_sharpness_h8[] = {
	{0xfe , 0x02},{0x97 , 0xff},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_sharpness_max[] = {
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_brightness_l4[] = {
	{0xfe , 0x01},{0x13 , 0x20},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_brightness_l3[] = {
	{0xfe , 0x01},{0x13 , 0x30},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_brightness_l2[] = {
	{0xfe , 0x01},{0x13 , 0x40},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_brightness_l1[] = {
	{0xfe , 0x01},{0x13 , 0x50},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_brightness_h0[] = {
	{0xfe , 0x01},{0x13 , 0x68},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_brightness_h1[] = {
	{0xfe , 0x01},{0x13 , 0x68},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_brightness_h2[] = {
	{0xfe , 0x01},{0x13 , 0x78},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_brightness_h3[] = {
	{0xfe , 0x01},{0x13 , 0x88},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_brightness_h4[] = {
	{0xfe , 0x01},{0x13 , 0x98},{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_saturation_l2[] = {
	{0xfe,0x02},
	{0xd1,0x20},
	{0xd2,0x20},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_saturation_l1[] = {
	{0xfe,0x02},
	{0xd1,0x28},
	{0xd2,0x28},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_saturation_h0[] = {
	{0xfe,0x02},
	{0xd1,0x34},
	{0xd2,0x34},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_saturation_h1[] = {
	{0xfe,0x02},
	{0xd1,0x40},
	{0xd2,0x40},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_saturation_h2[] = {
	{0xfe,0x02},
	{0xd1,0x48},
	{0xd2,0x48},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_l4[] = {
	{0xfe,0x02}, {0xd3,0x20}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_l3[] = {
	{0xfe,0x02}, {0xd3,0x30}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_l2[] = {
	{0xfe,0x02}, {0xd3,0x34}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_l1[] = {
	{0xfe,0x02}, {0xd3,0x38}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_h0[] = {
	{0xfe,0x02}, {0xd3,0x40}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_h1[] = {
	{0xfe,0x02}, {0xd3,0x44}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_h2[] = {
	{0xfe,0x02}, {0xd3,0x48}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_h3[] = {
	{0xfe,0x02}, {0xd3,0x50}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_contrast_h4[] = {
	{0xfe,0x02}, {0xd3,0x58}, {0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_whitebalance_auto[] = {
	{0xb3,0x61}, {0xb4,0x40}, {0xb5,0x61},{0x82,0xfe},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_whitebalance_incandescent[] = {
	{0x82,0xfc},{0xb3,0x50}, {0xb4,0x40}, {0xb5,0xa8},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_whitebalance_fluorescent[] = {
	{0x82,0xfc},{0xb3,0x72}, {0xb4,0x40}, {0xb5,0x5b},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_whitebalance_daylight[] = {
	{0x82,0xfc},{0xb3,0x70}, {0xb4,0x40}, {0xb5,0x50},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_whitebalance_cloudy[] = {
	{0x82,0xfc},{0xb3,0x58}, {0xb4,0x40}, {0xb5,0x50},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_effect_none[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0xe0},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_mono[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x12},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_negative[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x01},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_solarize[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x90},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_sepia[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x82},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_posterize[] = {
	//{0x83,0x92},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_whiteboard[] = {
	{0xfe , 0x02},
	{0x90 , 0xec},
	{0xfe , 0x01},
	{0x0c , 0x81},
	{0x13 , 0x40},
	{0x1f , 0x40},
	{0x0a , 0x41},
	{0xfe , 0x00},
	{0x83 , 0x08},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_lightgreen[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x02},
	{0xfe , 0x02},
	{0xda , 0xf0},
	{0xdb , 0xf0},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list gc2145_effect_brown[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x02},
	{0xfe , 0x02},
	{0xda , 0x80},
	{0xdb , 0x40},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff}, /* END MARKER */
};

static struct regval_list gc2145_effect_waterblue[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x02},
	{0xfe , 0x02},
	{0xda , 0x20},
	{0xdb , 0xc0},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff}, /* END MARKER */
};


static struct regval_list gc2145_effect_blackboard[] = {
	{0xfe , 0x02},
	{0x90 , 0xec},
	{0xfe , 0x01},
	{0x0c , 0x81},
	{0x13 , 0x40},
	{0x1f , 0x40},
	{0x0a , 0x41},
	{0xfe , 0x00},
	{0x83 , 0x09},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_effect_aqua[] = {
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0xfe , 0x01},
	{0x0c , 0x10},
	{0x13 , 0x68},
	{0x1f , 0x35},
	{0x0a , 0x80},
	{0xfe , 0x00},
	{0x83 , 0x52},
	{0xfe , 0x02},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xfe , 0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_l6[] = {
	{0xfe,0x02},
	{0xd5,0xd0},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_l5[] = {
	{0xfe,0x02},
	{0xd5,0xd8},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_l4[] = {
	{0xfe,0x02},
	{0xd5,0xe0},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_l3[] = {
	{0xfe,0x02},
	{0xd5,0xe8},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_l2[] = {
	{0xfe,0x02},
	{0xd5,0xf0},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_exposure_l1[] = {
	{0xfe,0x02},
	{0xd5,0xf8},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_exposure_h0[] = {
	{0xfe,0x02},
	{0xd5,0x00},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_exposure_h1[] = {
	{0xfe,0x02},
	{0xd5,0x10},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_exposure_h2[] = {
	{0xfe,0x02},
	{0xd5,0x20},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_h3[] = {
	{0xfe,0x02},
	{0xd5,0x30},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_h4[] = {
	{0xfe,0x02},
	{0xd5,0x40},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_h5[] = {
	{0xfe,0x02},
	{0xd5,0x50},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_exposure_h6[] = {
	{0xfe,0x02},
	{0xd5,0x60},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};


//next preview mode

static struct regval_list gc2145_previewmode_mode_auto[] = {  
	{0xfe,0x01},
	{0x3c,0x40},
	{0xfe,0x00},
	{0x82,0xfa},
	{0xfe,0x02},
	{0xd0,0x40},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list gc2145_previewmode_mode_nightmode[] = {
	{0xfe,0x01},
	{0x3c,0x60},
	{0xfe,0x00},
	{0x82,0xfa},
	{0xfe,0x02},
	{0xd0,0x40},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_previewmode_mode_sports[] = {
	{0xfe,0x01},
	{0x3c,0x00},
	{0xfe,0x00},
	{0x82,0x7a},
	{0xfe,0x02},
	{0xd0,0x50},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc2145_previewmode_mode_portrait[] = {
	{0xfe,0x01},
	{0x3c,0x40},
	{0xfe,0x00},
	{0x82,0x7a},
	{0xfe,0x02},
	{0xd0,0x48},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_previewmode_mode_landscape[] = {
	{0xfe,0x01},
	{0x3c,0x00},
	{0xfe,0x00},
	{0x82,0x7a},
	{0xfe,0x02},
	{0xd0,0x58},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

#if 0
static struct regval_list gc2145_scene_mode_auto[] = {
	{0xfe,0x01},
	{0x3c,0x40},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff}, /* END MARKER */
};

static struct regval_list gc2145_scene_mode_night[] = {
	{0xfe,0x01},
	{0x3c,0x60},
	{0xfe,0x00},
	{GC2145_REG_END, 0xff}, /* END MARKER */

};
#endif

static struct regval_list gc2145_iso_auto[] = {
	{0xb6,0x01},
	{0xb1,0x20},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_iso_100[] = {
	{0xb6,0x00},
	{0xb1,0x20},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_iso_200[] = {
	{0xb6,0x00},
	{0xb1,0x30},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_iso_400[] = {
	{0xb6,0x00},
	{0xb1,0x40},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc2145_iso_800[] = {
	{0xb6,0x00},
	{0xb1,0x50},
	{GC2145_REG_END, 0xff},	/* END MARKER */
};

//preview mode end
static int gc2145_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int gc2145_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc2145_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	unsigned char val = 0x17;
	int ret;
	while (vals->reg_num != GC2145_REG_END) {
		if (vals->reg_num == 0x17 && vals->value == GC2145_REG_VAL_HVFLIP) {
			if (gc2145_isp_parm.mirror)
				val &= 0x16;
			if (gc2145_isp_parm.flip)
				val &= 0x15;
			vals->value = val;
		}
		ret = gc2145_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int gc2145_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

#if 0
static int gc2145_shutter(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2 ;
	unsigned short v;

	gc2145_write(sd, 0xb6,0x00);

	ret = gc2145_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;
	ret = gc2145_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2;
	//v = v /2 ;
	if (v<1)
		return 0;

	gc2145_write(sd, 0x03, (v >> 8)&0xff );
	gc2145_write(sd, 0x04, v&0xff);

	msleep(500);
	gc2145_write(sd, 0xb6,0x01);

	return  0;
}

#endif

static int gc2145_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc2145_info *info = container_of(sd, struct gc2145_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;
	return gc2145_write_array(sd, gc2145_init_regs);
}

static int gc2145_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = gc2145_read(sd, 0xf0, &v);
	
	if (ret < 0)
		return ret;
	if (v != GC2145_CHIP_ID_H)
		return -ENODEV;
	ret = gc2145_read(sd, 0xf1, &v);
	
	if (ret < 0)
		return ret;
	if (v != GC2145_CHIP_ID_L)
		return -ENODEV;


	return 0;
}

static struct gc2145_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc2145_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= NULL,
	},
};
#define N_GC2145_FMTS ARRAY_SIZE(gc2145_formats)



static struct gc2145_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
	struct regval_list *framerate_30fps_regs;
	struct regval_list *framerate_15fps_regs;
	struct regval_list *framerate_7fps_regs;
	struct regval_list *framerate_autofps_regs;
} gc2145_win_sizes[] = {
	/*SVGA*/
	{
		.width		= SVGA_WIDTH,
		.height		= SVGA_HEIGHT,
		.regs 		= gc2145_win_svga,
		.framerate_30fps_regs = gc2145_frame_svga_30fps,
		.framerate_15fps_regs = gc2145_frame_svga_15fps,
		.framerate_7fps_regs = gc2145_frame_svga_7fps,
		.framerate_autofps_regs = gc2145_frame_svga_autofps,
	},
	/*720p*/
	{
		.width		= SXGA_WIDTH,
		.height 	= SXGA_HEIGHT,
		.regs		= gc2145_win_sxga,
		.framerate_30fps_regs = gc2145_frame_sxga_30fps,
		.framerate_15fps_regs = gc2145_frame_sxga_15fps,
		.framerate_7fps_regs = gc2145_frame_sxga_7fps,
		.framerate_autofps_regs = gc2145_frame_sxga_autofps,
	},

	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.regs 		= gc2145_win_2m,
		.framerate_30fps_regs = gc2145_frame_2m_30fps,
		.framerate_15fps_regs = gc2145_frame_2m_15fps,
		.framerate_7fps_regs = gc2145_frame_2m_7fps,
		.framerate_autofps_regs = gc2145_frame_2m_autofps,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(gc2145_win_sizes))


static int gc2145_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC2145_FMTS)
		return -EINVAL;

	*code = gc2145_formats[index].mbus_code;
	return 0;
}

static int gc2145_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc2145_format_struct **ret_fmt,
		struct gc2145_win_size **ret_wsize)
{
	int index;
	struct gc2145_win_size *wsize;

	for (index = 0; index < N_GC2145_FMTS; index++)
		if (gc2145_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC2145_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc2145_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc2145_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = gc2145_win_sizes; wsize < gc2145_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc2145_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc2145_formats[index].colorspace;
	return 0;
}

static int gc2145_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc2145_try_fmt_internal(sd, fmt, NULL, NULL);
}

#define GC2145_COPY_REGS(regs)	do {	\
					while (regs[len].reg_num != GC2145_REG_END && regs[len].value != 0xff) {	\
						data->reg[data->reg_num].addr = regs[len].reg_num;	\
						data->reg[data->reg_num].data = regs[len].value;	\
						len++;	\
						data->reg_num++;	\
					}	\
				} while (0)

static int gc2145_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc2145_info *info = container_of(sd, struct gc2145_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct gc2145_format_struct *fmt_s;
	struct gc2145_win_size *wsize;
	int ret,len;

	ret = gc2145_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;


	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc2145_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {

		memset(data, 0, sizeof(*data));
		data->mipi_clk = 480;//288; /* Mbps. */
		len = 0;
		data->flags = V4L2_I2C_ADDR_8BIT;
		data->slave_addr = client->addr;
		GC2145_COPY_REGS(wsize->regs);

		len = 0;
		switch (info->frame_rate) {
			case FRAME_RATE_30:
				GC2145_COPY_REGS(wsize->framerate_30fps_regs);
				break;
			case FRAME_RATE_15:
				GC2145_COPY_REGS(wsize->framerate_15fps_regs);
				break;
			case FRAME_RATE_7:
				GC2145_COPY_REGS(wsize->framerate_7fps_regs);
				break;
			default:
				GC2145_COPY_REGS(wsize->framerate_autofps_regs);
				break;
		}
	}
	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int gc2145_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct gc2145_info *info = container_of(sd, struct gc2145_info, sd);
	printk("gc2145_set_framerate %d\n", framerate);

	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;

	info->frame_rate = framerate;
	return 0;
}


static int gc2145_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = gc2145_write_array(sd, gc2145_stream_on);
	else
		ret = gc2145_write_array(sd, gc2145_stream_off);

	return ret;
}

static int gc2145_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc2145_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc2145_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2145_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2145_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc2145_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc2145_frame_rates))
		return -EINVAL;

	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc2145_frame_rates[interval->index];
	return 0;
}

static int gc2145_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc2145_win_size *win = &gc2145_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc2145_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc2145_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[GC2145]set brightness %d\n", brightness);

	switch (brightness) {
		case BRIGHTNESS_L4:
			gc2145_write_array(sd, gc2145_brightness_l4);
			break;
		case BRIGHTNESS_L3:
			gc2145_write_array(sd, gc2145_brightness_l3);
			break;
		case BRIGHTNESS_L2:
			gc2145_write_array(sd, gc2145_brightness_l2);
			break;
		case BRIGHTNESS_L1:
			gc2145_write_array(sd, gc2145_brightness_l1);
			break;
		case BRIGHTNESS_H0:
			gc2145_write_array(sd, gc2145_brightness_h0);
			break;
		case BRIGHTNESS_H1:
			gc2145_write_array(sd, gc2145_brightness_h1);
			break;
		case BRIGHTNESS_H2:
			gc2145_write_array(sd, gc2145_brightness_h2);
			break;
		case BRIGHTNESS_H3:
			gc2145_write_array(sd, gc2145_brightness_h3);
			break;
		case BRIGHTNESS_H4:
			gc2145_write_array(sd, gc2145_brightness_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2145_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	switch (contrast) {

		case CONTRAST_L4:
			gc2145_write_array(sd, gc2145_contrast_l4);
			break;
		case CONTRAST_L3:
			gc2145_write_array(sd, gc2145_contrast_l3);
			break;
		case CONTRAST_L2:
			gc2145_write_array(sd, gc2145_contrast_l2);
			break;
		case CONTRAST_L1:
			gc2145_write_array(sd, gc2145_contrast_l1);
			break;
		case CONTRAST_H0:
			gc2145_write_array(sd, gc2145_contrast_h0);
			break;
		case CONTRAST_H1:
			gc2145_write_array(sd, gc2145_contrast_h1);
			break;
		case CONTRAST_H2:
			gc2145_write_array(sd, gc2145_contrast_h2);
			break;
		case CONTRAST_H3:
			gc2145_write_array(sd, gc2145_contrast_h3);
			break;
		case CONTRAST_H4:
			gc2145_write_array(sd, gc2145_contrast_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2145_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	printk(KERN_DEBUG "[GC2145]set saturation %d\n", saturation);

	switch (saturation) {
		case SATURATION_L2:
			gc2145_write_array(sd, gc2145_saturation_l2);
			break;
		case SATURATION_L1:
			gc2145_write_array(sd, gc2145_saturation_l1);
			break;
		case SATURATION_H0:
			gc2145_write_array(sd, gc2145_saturation_h0);
			break;
		case SATURATION_H1:
			gc2145_write_array(sd, gc2145_saturation_h1);
			break;
		case SATURATION_H2:
			gc2145_write_array(sd, gc2145_saturation_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2145_set_wb(struct v4l2_subdev *sd, int wb)
{
	printk(KERN_DEBUG "[GC2145]set gc2145_set_wb %d\n", wb);

	switch (wb) {
		case WHITE_BALANCE_AUTO:
			gc2145_write_array(sd, gc2145_whitebalance_auto);
			break;
		case WHITE_BALANCE_INCANDESCENT:
			gc2145_write_array(sd, gc2145_whitebalance_incandescent);
			break;
		case WHITE_BALANCE_FLUORESCENT:
			gc2145_write_array(sd, gc2145_whitebalance_fluorescent);
			break;
		case WHITE_BALANCE_DAYLIGHT:
			gc2145_write_array(sd, gc2145_whitebalance_daylight);
			break;
		case WHITE_BALANCE_CLOUDY_DAYLIGHT:
			gc2145_write_array(sd, gc2145_whitebalance_cloudy);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2145_get_iso(struct v4l2_subdev *sd, unsigned char val)
{
	int ret;

	ret = gc2145_read(sd, 0xb1, &val);
	if (ret < 0)
		return ret;
	return 0;
}
static int gc2145_get_iso_min_gain(struct v4l2_subdev *sd, u32 *val)
{
	*val = ISO_MIN_GAIN ;
	return 0 ;
}

static int gc2145_get_iso_max_gain(struct v4l2_subdev *sd, u32 *val)
{
	*val = ISO_MAX_GAIN ;
	return 0 ;
}

static int gc2145_set_sharpness(struct v4l2_subdev *sd, int sharpness)
{

	printk(KERN_DEBUG "[GC2145]set sharpness %d\n", sharpness);
	switch (sharpness) {
		case SHARPNESS_L2:
			gc2145_write_array(sd, gc2145_sharpness_l2);
			break;
		case SHARPNESS_L1:
			gc2145_write_array(sd, gc2145_sharpness_l1);
			break;
		case SHARPNESS_H0:
			gc2145_write_array(sd, gc2145_sharpness_h0);
			break;
		case SHARPNESS_H1:
			gc2145_write_array(sd, gc2145_sharpness_h1);
			break;
		case SHARPNESS_H2:
			gc2145_write_array(sd, gc2145_sharpness_h2);
			break;
		case SHARPNESS_H3:
			gc2145_write_array(sd, gc2145_sharpness_h3);
			break;
		case SHARPNESS_H4:
			gc2145_write_array(sd, gc2145_sharpness_h4);
			break;
		case SHARPNESS_H5:
			gc2145_write_array(sd, gc2145_sharpness_h5);
			break;
		case SHARPNESS_H6:
			gc2145_write_array(sd, gc2145_sharpness_h6);
			break;
		case SHARPNESS_H7:
			gc2145_write_array(sd, gc2145_sharpness_h7);
			break;
		case SHARPNESS_H8:
			gc2145_write_array(sd, gc2145_sharpness_h8);
			break;
		case SHARPNESS_MAX:
			gc2145_write_array(sd, gc2145_sharpness_max);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static unsigned int gc2145_calc_shutter_speed(struct v4l2_subdev *sd , unsigned int *val)
{
	unsigned char v1,v2,v3,v4;
	unsigned int speed = 0;
	int ret;
	ret = gc2145_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;

	ret = gc2145_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	ret = gc2145_write(sd, 0xfe, 0x01);
	if (ret < 0)
		return ret;

	ret = gc2145_read(sd, 0x25, &v3);
	if (ret < 0)
		return ret;

	ret = gc2145_read(sd, 0x26, &v4);
	if (ret < 0)
		return ret;

	ret = gc2145_write(sd, 0xfe, 0x00);
	if (ret < 0)
		return ret;

	speed = ((v1<<8)|v2);
	speed = (speed * 10000) / ((v3<<8)|v4);
	*val = speed ;
	printk("[gc2145]:speed=0x%0x\n",*val);
	return 0;
}

static int gc2145_get_brightlevel(struct v4l2_subdev *sd, unsigned int *val)
{
	unsigned char v1,v2;
	int ret;

	ret = gc2145_read(sd, 0x25, &v1);
	if (ret) {
		printk("%s, read 0x25 failed\n",__func__);
		*val = 1;
		return 0;
	}
	ret = gc2145_read(sd, 0xb1, &v2);
	if (ret) {
		printk("%s, read 0xb1 failed\n",__func__);
		*val = 1;
		return 0;
	}
	if ((v1 > 0x03)&&(v2 >= 0x35))
		*val = 0;
	else
		*val = 1;

	return 0;
}

static int gc2145_set_iso(struct v4l2_subdev *sd, int iso)
{
	printk(KERN_DEBUG "[GC2145]set_iso %d\n", iso);
	switch (iso) {
		case ISO_AUTO:
			gc2145_write_array(sd, gc2145_iso_auto);
			break;
		case ISO_100:
			gc2145_write_array(sd, gc2145_iso_100);
			break;
		case ISO_200:
			gc2145_write_array(sd, gc2145_iso_200);
			break;
		case ISO_400:
			gc2145_write_array(sd, gc2145_iso_400);
			break;
		case ISO_800:
			gc2145_write_array(sd, gc2145_iso_800);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2145_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk(KERN_DEBUG "[GC2145]set contrast %d\n", effect);

	switch (effect) {
		case EFFECT_NONE:
			gc2145_write_array(sd, gc2145_effect_none);
			break;
		case EFFECT_MONO:
			gc2145_write_array(sd, gc2145_effect_mono);
			break;
		case EFFECT_NEGATIVE:
			gc2145_write_array(sd, gc2145_effect_negative);
			break;
		case EFFECT_SOLARIZE:
			gc2145_write_array(sd, gc2145_effect_solarize);
			break;
		case EFFECT_SEPIA:
			gc2145_write_array(sd, gc2145_effect_sepia);
			break;
		case EFFECT_POSTERIZE:
			gc2145_write_array(sd, gc2145_effect_posterize);
			break;
		case EFFECT_WHITEBOARD:
			gc2145_write_array(sd, gc2145_effect_whiteboard);
			break;
		case EFFECT_BLACKBOARD:
			gc2145_write_array(sd, gc2145_effect_blackboard);
			break;
		case EFFECT_WATER_GREEN:
			gc2145_write_array(sd, gc2145_effect_lightgreen);
			break;
		case EFFECT_WATER_BLUE:
			gc2145_write_array(sd, gc2145_effect_waterblue);
			break;
		case EFFECT_BROWN:
			gc2145_write_array(sd, gc2145_effect_brown);
			break;
		case EFFECT_AQUA:
			gc2145_write_array(sd, gc2145_effect_aqua);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc2145_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk(KERN_DEBUG "[GC2145]set exposure %d\n", exposure);

	switch (exposure) {
		case EXPOSURE_L6:
			gc2145_write_array(sd, gc2145_exposure_l6);
			break;
		case EXPOSURE_L5:
			gc2145_write_array(sd, gc2145_exposure_l5);
			break;
		case EXPOSURE_L4:
			gc2145_write_array(sd, gc2145_exposure_l4);
			break;
		case EXPOSURE_L3:
			gc2145_write_array(sd, gc2145_exposure_l3);
			break;
		case EXPOSURE_L2:
			gc2145_write_array(sd, gc2145_exposure_l2);
			break;
		case EXPOSURE_L1:
			gc2145_write_array(sd, gc2145_exposure_l1);
			break;
		case EXPOSURE_H0:
			gc2145_write_array(sd, gc2145_exposure_h0);
			break;
		case EXPOSURE_H1:
			gc2145_write_array(sd, gc2145_exposure_h1);
			break;
		case EXPOSURE_H2:
			gc2145_write_array(sd, gc2145_exposure_h2);
			break;
		case EXPOSURE_H3:
			gc2145_write_array(sd, gc2145_exposure_h3);
			break;
		case EXPOSURE_H4:
			gc2145_write_array(sd, gc2145_exposure_h4);
			break;
		case EXPOSURE_H5:
			gc2145_write_array(sd, gc2145_exposure_h5);
			break;
		case EXPOSURE_H6:
			gc2145_write_array(sd, gc2145_exposure_h6);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int gc2145_set_previewmode(struct v4l2_subdev *sd, int mode)
{
	printk(KERN_INFO "MIKE:[GC2145]set_previewmode %d\n", mode);

	switch (mode) {
		case preview_auto:
			gc2145_write_array(sd, gc2145_previewmode_mode_auto);
			break;
		case normal:
			gc2145_write_array(sd, gc2145_previewmode_mode_auto);
			break;
		case dynamic:
		case beatch:
		case bar:
		case snow:
		case landscape:
			gc2145_write_array(sd, gc2145_previewmode_mode_landscape);
			break;
		case nightmode:
			gc2145_write_array(sd, gc2145_previewmode_mode_nightmode);
			break;
		case theatre:
		case party:
		case candy:
		case fireworks:
		case sunset:
		case nightmode_portrait:
		case sports:
			gc2145_write_array(sd, gc2145_previewmode_mode_sports);
			break;
		case anti_shake:
		case portrait:
			gc2145_write_array(sd, gc2145_previewmode_mode_portrait);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int gc2145_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2145_info *info = container_of(sd, struct gc2145_info, sd);
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
			isp_setting.setting = (struct v4l2_isp_regval *)&gc2145_isp_setting;
			isp_setting.size = ARRAY_SIZE(gc2145_isp_setting);
			memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
			break;
		case V4L2_CID_ISP_PARM:
			ctrl->value = (int)&gc2145_isp_parm;
			break;
		case V4L2_CID_ISO:
			ret = gc2145_get_iso(sd, ctrl->value);
			if (ret)
				printk("Get iso value failed.\n");
			break;
		case V4L2_CID_SNAPSHOT_GAIN:
			ret = gc2145_get_iso(sd, ctrl->value);
			if (ret)
				printk("Get iso value failed.\n");
			break;
		case V4L2_CID_MIN_GAIN:
			ret = gc2145_get_iso_min_gain(sd, &ctrl->value);
			if (ret)
				printk("Get iso min gain value failed.\n");
			break;
		case V4L2_CID_MAX_GAIN:
			ret = gc2145_get_iso_max_gain(sd, &ctrl->value);
			if (ret)
				printk("Get iso max gain value failed.\n");
			break;
		case V4L2_CID_SCENE:
			ctrl->value = info->scene_mode;
			break;
		case V4L2_CID_SHARPNESS:
			ctrl->value = info->sharpness;
			break;
		case V4L2_CID_SHUTTER_SPEED:
			ret = gc2145_calc_shutter_speed(sd, &ctrl->value);
			if (ret)
				printk("Get shutter speed value failed.\n");
			break;
		case V4L2_CID_GET_BRIGHTNESS_LEVEL:
			ret = gc2145_get_brightlevel(sd, &ctrl->value);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int gc2145_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2145_info *info = container_of(sd, struct gc2145_info, sd);
	int ret = 0;
	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ret = gc2145_set_brightness(sd, ctrl->value);
			if (!ret)
				info->brightness = ctrl->value;
			break;
		case V4L2_CID_CONTRAST:
			ret = gc2145_set_contrast(sd, ctrl->value);
			if (!ret)
				info->contrast = ctrl->value;
			break;
		case V4L2_CID_SATURATION:
			ret = gc2145_set_saturation(sd, ctrl->value);
			if (!ret)
				info->saturation = ctrl->value;
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			ret = gc2145_set_wb(sd, ctrl->value);
			if (!ret)
				info->wb = ctrl->value;
			break;
		case V4L2_CID_EXPOSURE:
			ret = gc2145_set_exposure(sd, ctrl->value);
			if (!ret)
				info->exposure = ctrl->value;
			break;
		case V4L2_CID_EFFECT:
			ret = gc2145_set_effect(sd, ctrl->value);
			if (!ret)
				info->effect = ctrl->value;
			break;
		case V4L2_CID_FRAME_RATE:
			ret = gc2145_set_framerate(sd, ctrl->value);
			break;
		case V4L2_CID_SCENE:
			ret = gc2145_set_previewmode(sd, ctrl->value);
			if (!ret)
				info->scene_mode = ctrl->value;
			break;
		case V4L2_CID_ISO:
			ret = gc2145_set_iso(sd, ctrl->value);
			if (!ret)
				info->iso = ctrl->value;
			break;
		case V4L2_CID_SHARPNESS:
			ret = gc2145_set_sharpness(sd, ctrl->value);
			if (!ret)
				info->sharpness = ctrl->value ;
			break;
		case V4L2_CID_SET_FOCUS:
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}
	return ret;
}

static int gc2145_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC2145, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc2145_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc2145_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;

	return ret;
}

static int gc2145_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2145_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc2145_core_ops = {
	.g_chip_ident = gc2145_g_chip_ident,
	.g_ctrl = gc2145_g_ctrl,
	.s_ctrl = gc2145_s_ctrl,
	.queryctrl = gc2145_queryctrl,
	.reset = gc2145_reset,
	.init = gc2145_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc2145_g_register,
	.s_register = gc2145_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc2145_video_ops = {
	.enum_mbus_fmt = gc2145_enum_mbus_fmt,
	.try_mbus_fmt = gc2145_try_mbus_fmt,
	.s_mbus_fmt = gc2145_s_mbus_fmt,
	.s_stream = gc2145_s_stream,
	.cropcap = gc2145_cropcap,
	.g_crop	= gc2145_g_crop,
	.s_parm = gc2145_s_parm,
	.g_parm = gc2145_g_parm,
	.enum_frameintervals = gc2145_enum_frameintervals,
	.enum_framesizes = gc2145_enum_framesizes,
};

static const struct v4l2_subdev_ops gc2145_ops = {
	.core = &gc2145_core_ops,
	.video = &gc2145_video_ops,
};

static int gc2145_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc2145_info *info;
	struct comip_camera_subdev_platform_data *gc2145_setting = NULL;

	int ret;

	info = kzalloc(sizeof(struct gc2145_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2145_ops);

	/* Make sure it's an gc2145 */
	ret = gc2145_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc2145 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc2145 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	gc2145_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;

	if (gc2145_setting) {
		if (gc2145_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			gc2145_isp_parm.mirror = 1;
		else
			gc2145_isp_parm.mirror = 0;

		if (gc2145_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			gc2145_isp_parm.flip = 1;
		else
			gc2145_isp_parm.flip = 0;
	}
	return ret;
}

static int gc2145_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145_info *info = container_of(sd, struct gc2145_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc2145_id[] = {
	{ "gc2145-mipi-yuv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2145_id);

static struct i2c_driver gc2145_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2145-mipi-yuv",
	},
	.probe		= gc2145_probe,
	.remove		= gc2145_remove,
	.id_table	= gc2145_id,
};

static __init int init_gc2145(void)
{
	return i2c_add_driver(&gc2145_driver);
}

static __exit void exit_gc2145(void)
{
	i2c_del_driver(&gc2145_driver);
}

fs_initcall(init_gc2145);
module_exit(exit_gc2145);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore gc2145 sensors with mipi interface");
MODULE_LICENSE("GPL");
