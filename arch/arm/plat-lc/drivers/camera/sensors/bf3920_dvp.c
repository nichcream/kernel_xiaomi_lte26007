
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
#include "bf3920_dvp.h"

#define BF3920_CHIP_ID_H	(0x39)
#define BF3920_CHIP_ID_L	(0x20)

#define VGA_WIDTH		640
#define VGA_HEIGHT		480
#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH	VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  VGA_HEIGHT

#define BF3920_REG_END		0xff
//#define BF3920_REG_DELAY	0xfffe

struct bf3920_format_struct;
struct bf3920_info {
	struct v4l2_subdev sd;
	struct bf3920_format_struct *fmt;
	struct bf3920_win_size *win;
	int brightness;
	int contrast;
	int wb;
	int effect;
	int saturation;
	int exposure;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list bf3920_init_regs[] = {
	//{0x12 , 0x80},
	{0x1b , 0x2c}, //PLL
	{0x11 , 0x30},

	{0x8a , 0x88},
	{0x2a , 0x0c},
	{0x2b , 0x2c},
	{0x92 , 0x02},
	{0x93 , 0x00},
    	   
	{0x21 , 0x15},//driver capability
	{0x15 , 0x82},////80  yzx
	{0x3a , 0x00},

	//initial AE and AWB
	{0x13 , 0x00},
	{0x01 , 0x12},
	{0x02 , 0x22},
	{0x24 , 0xc8},
	{0x87 , 0x2d},
	{0x13 , 0x07},

	//black level 
	{0x27 , 0x98},
	{0x28 , 0xff},
	{0x29 , 0x60},

	//black target 
	{0x20 , 0x40},
	{0x1F , 0x20},
	{0x22 , 0x20},
	{0x20 , 0x20},
	{0x26 , 0x20},

	//analog
	{0xe2 , 0xc4},
	{0xe7 , 0x4e},
	{0x08 , 0x16},
	{0x16 , 0x28},
	{0x1c , 0x00},
	{0x1d , 0xf1},
	{0xbb , 0x30},
	{0xf9 , 0x00},
	{0xed , 0x8f},
	{0x3e , 0x80},

	//Shading
	{0x1e , 0x46},
	{0x35 , 0x30},
	{0x65 , 0x30},
	{0x66 , 0x2a},
	{0xbc , 0xd3},
	{0xbd , 0x5c},
	{0xbe , 0x24},
	{0xbf , 0x13},
	{0x9b , 0x5c},
	{0x9c , 0x24},
	{0x36 , 0x13},
	{0x37 , 0x5c},
	{0x38 , 0x24},

	//denoise
	{0x70 , 0x01},
	{0x72 , 0x62},
	{0x78 , 0x37},
	{0x7a , 0x29},
	{0x7d , 0x85},

	// AE gain curve
	{0x13 , 0x0f},
	{0x8a , 0x10},
	{0x8b , 0x1d},
	{0x8e , 0x1e},
	{0x8f , 0x3a},
	{0x94 , 0x3d},
	{0x95 , 0x71},
	{0x96 , 0x76},
	{0x97 , 0xc5},
	{0x98 , 0xc6},
	{0x99 , 0x7b},

	// AE
	{0x13 , 0x07},
	{0x24 , 0x48},
	{0x25 , 0x88},
	{0x97 , 0x30},
	{0x98 , 0x0a},
	{0x80 , 0x9a},
	{0x81 , 0xe0},
	{0x82 , 0x30},
	{0x83 , 0x60},
	{0x84 , 0x65},
	{0x85 , 0x80},
	{0x86 , 0xc0},
	{0x89 , 0x55},
	{0x94 , 0x82},//42  c2

	//Gamma default low noise
	{0x3f , 0x20},
	{0x39 , 0xa0},

	{0x40 , 0x20},
	{0x41 , 0x25},
	{0x42 , 0x23},
	{0x43 , 0x1f},
	{0x44 , 0x18},
	{0x45 , 0x15},
	{0x46 , 0x12},
	{0x47 , 0x10},
	{0x48 , 0x10},
	{0x49 , 0x0f},
	{0x4b , 0x0e},
	{0x4c , 0x0c},
	{0x4e , 0x0b},
	{0x4f , 0x0a},
	{0x50 , 0x07},

	/*
	//gamma   guodu                                               
	{0x40 , 0x28},           
	{0x41 , 0x28},           
	{0x42 , 0x30},           
	{0x43 , 0x29},           
	{0x44 , 0x23},            
	{0x45 , 0x1b},            
	{0x46 , 0x17},
	{0x47 , 0x0f},
	{0x48 , 0x0d},
	{0x49 , 0x0b},
	{0x4b , 0x09},
	{0x4c , 0x08},
	{0x4e , 0x07},
	{0x4f , 0x05},
	{0x50 , 0x04},
	{0x39 , 0xa0},
	{0x3f , 0x20},

	//gamma   qingxi                                                      
	{0x40 , 0x28},
	{0x41 , 0x26},
	{0x42 , 0x24},
	{0x43 , 0x22},
	{0x44 , 0x1c},
	{0x45 , 0x19},
	{0x46 , 0x15},
	{0x47 , 0x11},
	{0x48 , 0x0f},
	{0x49 , 0x0e},
	{0x4b , 0x0c},
	{0x4c , 0x0a},
	{0x4e , 0x09},
	{0x4f , 0x08},
	{0x50 , 0x04},
	{0x39 , 0xa0},
	{0x3f , 0x20},
	*/


	{0x5c , 0x80},
	{0x51 , 0x22},
	{0x52 , 0x00},
	{0x53 , 0x96},
	{0x54 , 0x8C},
	{0x57 , 0x7F},
	{0x58 , 0x0B},
	{0x5a , 0x14},

	//Color default
	{0x5c , 0x00},
	{0x51 , 0x32},
	{0x52 , 0x17},
	{0x53 , 0x8C},
	{0x54 , 0x79},
	{0x57 , 0x6E},
	{0x58 , 0x01},
	{0x5a , 0x36},
	{0x5e , 0x38},

	/*                                                                                         
	//color 
	{0x5c , 0x00},               
	{0x51 , 0x24},        
	{0x52 , 0x0b},               
	{0x53 , 0x77},                
	{0x54 , 0x65},                
	{0x57 , 0x5e},                
	{0x58 , 0x19},                               
	{0x5a , 0x16},                
	{0x5e , 0x38},     


	//color
	{0x5c , 0x00},            
	{0x51 , 0x2b},              
	{0x52 , 0x0e},              
	{0x53 , 0x9f},              
	{0x54 , 0x7d},              
	{0x57 , 0x91},             
	{0x58 , 0x21},                             
	{0x5a , 0x16},            
	{0x5e , 0x38},
                        

	//color                        
	{0x5c , 0x00},             
	{0x51 , 0x24},             
	{0x52 , 0x0b},             
	{0x53 , 0x77},             
	{0x54 , 0x65},             
	{0x57 , 0x5e},             
	{0x58 , 0x19},                            
	{0x5a , 0x16},             
	{0x5e , 0x38},
	*/

	//AWB
	{0x6a , 0x81},
	{0x23 , 0x66},
	{0xa1 , 0x31},
	{0xa2 , 0x0b},
	{0xa3 , 0x25},
	{0xa4 , 0x09},
	{0xa5 , 0x26},
	{0xa7 , 0x9a},
	{0xa8 , 0x15},
	{0xa9 , 0x13},
	{0xaa , 0x12},
	{0xab , 0x16},
	{0xc8 , 0x10},
	{0xc9 , 0x15},
	{0xd2 , 0x78},
	{0xd4 , 0x20},

	//saturation
	{0x56 , 0x28},
	{0xb0 , 0x8d},
	//{0xb1 , 0x95},//15  //djj add

	{0x56 , 0xe8},
	{0x55 , 0xf0},
	{0xb0 , 0xb8},

	{0x56 , 0xa8},
	{0x55 , 0xc8},
	{0xb0 , 0x98},

	{0x0b , 0x10},	//800*600
	{0x09 , 0x42},
	//skin
	{0xee , 0x2a},
	{0xef , 0x1b},

	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_fmt_yuv422[] = {
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_win_vga[] = {

	{0x1b , 0x2c},
	{0x11 , 0x30},
	{0x8a , 0x88},
	{0x2a , 0x0c},
	{0x2b , 0x2c},
	{0x92 , 0x02},
	{0x93 , 0x00},

	{0x4a , 0x80},
	{0xcc , 0x00},
	{0xca , 0x00},
	{0xcb , 0x00},
	{0xcf , 0x00},
	{0xcd , 0x00},
	{0xce , 0x00},
	{0xc0 , 0xd3},
	{0xc1 , 0x04},
	{0xc2 , 0xc4},
	{0xc3 , 0x11},
	{0xc4 , 0x3f},
	{0xb5 , 0x3f},
	{0x03 , 0x50},
	{0x17 , 0x00},
	{0x18 , 0x00},
	{0x10 , 0x30},
	{0x19 , 0x00},
	{0x1a , 0xc0},
	{0x0B , 0x10},
	{0xff , 0xff},

	{BF3920_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3920_win_SXGA[] = {
	//crop window
	{0x1b , 0x2c},
	{0x11 , 0x30},
	{0x8a , 0x88},
	{0x2a , 0x0c},
	{0x2b , 0x2c},
	{0x92 , 0x02},
	{0x93 , 0x00},


	//window
	{0x4a , 0x80},
	{0xcc , 0x00},
	{0xca , 0x00},
	{0xcb , 0x00},
	{0xcf , 0x00},
	{0xcd , 0x00},
	{0xce , 0x00},
	{0xc0 , 0xd3},
	{0xc1 , 0x04},
	{0xc2 , 0xc4},
	{0xc3 , 0x11},
	{0xc4 , 0x3f},
	{0xb5 , 0x3f},
	{0x03 , 0x50},
	{0x17 , 0x00},
	{0x18 , 0x00},
	{0x10 , 0x30},
	{0x19 , 0x00},
	{0x1a , 0xc0},
	{0x0B , 0x00},

	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_win_2M[] = {
	//crop window
	{0x1b , 0x2c},
	{0x11 , 0x30},
	{0x8a , 0x88},
	{0x2a , 0x0c},
	{0x2b , 0x2c},
	{0x92 , 0x02},
	{0x93 , 0x00},


	//window
	{0x4a , 0x80},
	{0xcc , 0x00},
	{0xca , 0x00},
	{0xcb , 0x00},
	{0xcf , 0x00},
	{0xcd , 0x00},
	{0xce , 0x00},
	{0xc0 , 0x00},
	{0xc1 , 0x00},
	{0xc2 , 0x00},
	{0xc3 , 0x00},
	{0xc4 , 0x00},
	{0xb5 , 0x00},
	{0x03 , 0x60},
	{0x17 , 0x00},
	{0x18 , 0x40},
	{0x10 , 0x40},
	{0x19 , 0x00},
	{0x1a , 0xb0},
	{0x0B , 0x00},

	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_stream_on[] = {
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_stream_off[] = {
	{BF3920_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3920_brightness_l3[] = {
	{0x56 , 0x28},
	{0x55 , 0xe0},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_brightness_l2[] = {
	{0x56 , 0x28},
	{0x55 , 0xc0},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_brightness_l1[] = {
	{0x56 , 0x28},
	{0x55 , 0xa0},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_brightness_h0[] = {
	{0x56 , 0x28},
	{0x55 , 0x00},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_brightness_h1[] = {
	{0x56 , 0x28},
	{0x55 , 0x20},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_brightness_h2[] = {
	{0x56 , 0x28},
	{0x55 , 0x40},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_brightness_h3[] = {
	{0x56 , 0x28},
	{0x55 , 0x60},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_contrast_l3[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x10},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_contrast_l2[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x25},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_contrast_l1[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x35},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_contrast_h0[] = {
	{0x56 , 0x28},
	{0xb1 , 0x15},
	{0x56 , 0xe8},
	{0xb4 , 0x50},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_contrast_h1[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x70},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_contrast_h2[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0xa0},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_contrast_h3[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0xf0},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_whitebalance_auto[] = {
	{0x13 , 0x07},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_whitebalance_incandescent[] = {
	{0x13 , 0x05},
	{0x01 , 0x1d},
	{0x02 , 0x16},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_whitebalance_fluorescent[] = {
	{0x13 , 0x05},
	{0x01 , 0x28},
	{0x02 , 0x0a},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_whitebalance_daylight[] = {
	{0x13 , 0x05},
	{0x01 , 0x10},
	{0x02 , 0x20},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_whitebalance_cloudy[] = {
	{0x13 , 0x05},
	{0x01 , 0x0e},
	{0x02 , 0x24},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_none[] = {
	{0x56 , 0x28},
	{0xb1 , 0x15},
	{0x56 , 0xe8},
	{0xb4 , 0x40},
	{0x98 , 0x08},
	{0x70 , 0x01},
	{0x69 , 0x00},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xb0 , 0x8d},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_mono[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x40},
	{0x98 , 0x8a},
	{0x70 , 0x00},
	{0x69 , 0x20},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xb0 , 0x8d},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_negative[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x40},
	{0x98 , 0x8a},
	{0x70 , 0x00},
	{0x69 , 0x01},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xb0 , 0x8d},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_solarize[] = {
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_sepia[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x40},
	{0x98 , 0x8a},
	{0x70 , 0x00},
	{0x69 , 0x20},
	{0x67 , 0x58},
	{0x68 , 0xa0},
	{0xb0 , 0x8d},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_posterize[] = {
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_whiteboard[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x40},
	{0x98 , 0x8a},
	{0x70 , 0x71},
	{0x69 , 0x00},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xb0 , 0x8d},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_effect_blackboard[] = {
	{0x56 , 0x28},
	{0xb1 , 0x95},
	{0x56 , 0xe8},
	{0xb4 , 0x40},
	{0x98 , 0x0a},
	{0x70 , 0x61},
	{0x69 , 0x00},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xb0 , 0x8d},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_saturation_l2[] = {
	{0xf1 , 0x00},
	{0x56 , 0xa8},
	{0x55 , 0x20},
	{0xb0 , 0x08},
	{0x56 , 0xe8},
	{0x55 , 0x30},
	{0xb0 , 0x00},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_saturation_l1[] = {
	{0xf1 , 0x00},
	{0x56 , 0xa8},
	{0x55 , 0x78},
	{0xb0 , 0x58},
	{0x56 , 0xe8},
	{0x55 , 0x70},
	{0xb0 , 0x48},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_saturation_h0[] = {
	{0xf1 , 0x00},
	{0x56 , 0xa8},
	{0x55 , 0xb8},
	{0xb0 , 0x98},
	{0x56 , 0xe8},
	{0x55 , 0xf0},
	{0xb0 , 0xb8},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_saturation_h1[] = {
	{0xf1 , 0x00},
	{0x56 , 0xe0},
	{0x55 , 0xc8},
	{0xb0 , 0x58},
	{0x56 , 0xe8},
	{0x55 , 0xf6},
	{0xb0 , 0xd8},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_saturation_h2[] = {
	{0xf1 , 0x00},
	{0x56 , 0xa8},
	{0x55 , 0xf0},
	{0xb0 , 0xe8},
	{0x56 , 0xe8},
	{0x55 , 0xfb},
	{0xb0 , 0xf8},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3920_exposure_l2[] = {
	{0x24 , 0x18},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_exposure_l1[] = {
	{0x24 , 0x2f},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_exposure_h0[] = {
	{0x24 , 0x48},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_exposure_h1[] = {
	{0x24 , 0x5f},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3920_exposure_h2[] = {
	{0x24 , 0x7f},
	{BF3920_REG_END, 0x00},	/* END MARKER */
};
static int bf3920_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int bf3920_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int bf3920_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != BF3920_REG_END) {
		int ret = bf3920_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int bf3920_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int bf3920_init(struct v4l2_subdev *sd, u32 val)
{
	struct bf3920_info *info = container_of(sd, struct bf3920_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return bf3920_write_array(sd, bf3920_init_regs);
}

static int bf3920_shutter(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2 ;
	unsigned short v;

	bf3920_write(sd, 0x13,0x00);
	ret = bf3920_read(sd, 0x8c, &v1);
	if (ret < 0)
		return ret;
	ret = bf3920_read(sd, 0x8d, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2	 ;
 
	v = v ;/// 2;
	bf3920_write(sd, 0x8c, (v >> 8)&0xff ); /* Shutter */
  	bf3920_write(sd, 0x8d, v&0xff);
       msleep(400);

	return  0;
}

static int bf3920_detect(struct v4l2_subdev *sd)
{
	unsigned char v=0x0;
	int ret;

//while(1)
//{

	ret = bf3920_read(sd, 0xfc, &v);
	printk("BF3920_CHIP_ID_H = %x./n",v);

//}
	if (ret < 0)
		return ret;
	if (v != BF3920_CHIP_ID_H)
		return -ENODEV;
	ret = bf3920_read(sd, 0xfd, &v);
	printk("BF3920_CHIP_ID_L = %x./n",v);
	if (ret < 0)
		return ret;
	if (v != BF3920_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct bf3920_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} bf3920_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= bf3920_fmt_yuv422,
	},
};
#define N_BF3920_FMTS ARRAY_SIZE(bf3920_formats)

static struct bf3920_win_size {
	int	width;
	int	height;
	struct regval_list *regs;
} bf3920_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= bf3920_win_vga,
	},
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.regs 		= bf3920_win_SXGA,
	},
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.regs 		= bf3920_win_2M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(bf3920_win_sizes))

static int bf3920_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_BF3920_FMTS)
		return -EINVAL;

	*code = bf3920_formats[index].mbus_code;
	return 0;
}

static int bf3920_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct bf3920_format_struct **ret_fmt,
		struct bf3920_win_size **ret_wsize)
{
	int index;
	struct bf3920_win_size *wsize;

	for (index = 0; index < N_BF3920_FMTS; index++)
		if (bf3920_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_BF3920_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = bf3920_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = bf3920_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = bf3920_win_sizes; wsize < bf3920_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= bf3920_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = bf3920_formats[index].colorspace;
	return 0;
}

static int bf3920_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return bf3920_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int bf3920_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct bf3920_info *info = container_of(sd, struct bf3920_info, sd);
	struct bf3920_format_struct *fmt_s;
	struct bf3920_win_size *wsize;
	int ret;

	ret = bf3920_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = bf3920_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = bf3920_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}
/*
	if (((wsize->width == MAX_WIDTH)&& (wsize->height == MAX_HEIGHT)) ||
		((wsize->width == SXGA_WIDTH)&& (wsize->height == SXGA_HEIGHT)))

		{
			ret = bf3920_shutter(sd);
		}
*/

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}
static int bf3920_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = bf3920_write_array(sd, bf3920_stream_on);
	else
		ret = bf3920_write_array(sd, bf3920_stream_off);

	return ret;
}

static int bf3920_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int bf3920_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	//snapshot max size
	a->bounds.width			= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
	//snapshot max size end
	a->defrect.left			= 0;
	a->defrect.top			= 0;
	//preview max size
	a->defrect.width		= MAX_PREVIEW_WIDTH;
	a->defrect.height		= MAX_PREVIEW_HEIGHT;
	//preview max size end
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int bf3920_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3920_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3920_frame_rates[] = { 30, 15, 10, 5, 1 };

static int bf3920_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(bf3920_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = bf3920_frame_rates[interval->index];
	return 0;
}

static int bf3920_enum_framesizes(struct v4l2_subdev *sd,
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
		struct bf3920_win_size *win = &bf3920_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int bf3920_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int bf3920_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[BF3920]set brightness %d\n", brightness);

	switch (brightness) {
	case BRIGHTNESS_L3:
		bf3920_write_array(sd, bf3920_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		bf3920_write_array(sd, bf3920_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		bf3920_write_array(sd, bf3920_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		bf3920_write_array(sd, bf3920_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		bf3920_write_array(sd, bf3920_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		bf3920_write_array(sd, bf3920_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		bf3920_write_array(sd, bf3920_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3920_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk(KERN_DEBUG "[BF3920]set contrast %d\n", contrast);

	switch (contrast) {
	case CONTRAST_L3:
		bf3920_write_array(sd, bf3920_contrast_l3);
		break;
	case CONTRAST_L2:
		bf3920_write_array(sd, bf3920_contrast_l2);
		break;
	case CONTRAST_L1:
		bf3920_write_array(sd, bf3920_contrast_l1);
		break;
	case CONTRAST_H0:
		bf3920_write_array(sd, bf3920_contrast_h0);
		break;
	case CONTRAST_H1:
		bf3920_write_array(sd, bf3920_contrast_h1);
		break;
	case CONTRAST_H2:
		bf3920_write_array(sd, bf3920_contrast_h2);
		break;
	case CONTRAST_H3:
		bf3920_write_array(sd, bf3920_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3920_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	printk(KERN_DEBUG "[BF3920]set saturation %d\n", saturation);

	switch (saturation) {
	case SATURATION_L2:
		bf3920_write_array(sd, bf3920_saturation_l2);
		break;
	case SATURATION_L1:
		bf3920_write_array(sd, bf3920_saturation_l1);
		break;
	case SATURATION_H0:
		bf3920_write_array(sd, bf3920_saturation_h0);
		break;
	case SATURATION_H1:
		bf3920_write_array(sd, bf3920_saturation_h1);
		break;
	case SATURATION_H2:
		bf3920_write_array(sd, bf3920_saturation_h2);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3920_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk(KERN_DEBUG "[BF3920]set exposure %d\n", exposure);

	switch (exposure) {
	case EXPOSURE_L2:
		bf3920_write_array(sd, bf3920_exposure_l2);
		break;
	case EXPOSURE_L1:
		bf3920_write_array(sd, bf3920_exposure_l1);
		break;
	case EXPOSURE_H0:
		bf3920_write_array(sd, bf3920_exposure_h0);
		break;
	case EXPOSURE_H1:
		bf3920_write_array(sd, bf3920_exposure_h1);
		break;
	case EXPOSURE_H2:
		bf3920_write_array(sd, bf3920_exposure_h2);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int bf3920_set_wb(struct v4l2_subdev *sd, int wb)
{
	printk(KERN_DEBUG "[BF3920]set contrast %d\n", wb);

	switch (wb) {
	case WHITE_BALANCE_AUTO:
		bf3920_write_array(sd, bf3920_whitebalance_auto);
		break;
	case WHITE_BALANCE_INCANDESCENT:
		bf3920_write_array(sd, bf3920_whitebalance_incandescent);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		bf3920_write_array(sd, bf3920_whitebalance_fluorescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		bf3920_write_array(sd, bf3920_whitebalance_daylight);
		break;
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		bf3920_write_array(sd, bf3920_whitebalance_cloudy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int bf3920_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk(KERN_DEBUG "[BF3920]set contrast %d\n", effect);

	switch (effect) {
	case EFFECT_NONE:
		bf3920_write_array(sd, bf3920_effect_none);
		break;
	case EFFECT_MONO:
		bf3920_write_array(sd, bf3920_effect_mono);
		break;
	case EFFECT_NEGATIVE:
		bf3920_write_array(sd, bf3920_effect_negative);
		break;
	case EFFECT_SOLARIZE:
		bf3920_write_array(sd, bf3920_effect_solarize);
		break;
	case EFFECT_SEPIA:
		bf3920_write_array(sd, bf3920_effect_sepia);
		break;
	case EFFECT_POSTERIZE:
		bf3920_write_array(sd, bf3920_effect_posterize);
		break;
	case EFFECT_WHITEBOARD:
		bf3920_write_array(sd, bf3920_effect_whiteboard);
		break;
	case EFFECT_BLACKBOARD:
		bf3920_write_array(sd, bf3920_effect_blackboard);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int bf3920_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3920_info *info = container_of(sd, struct bf3920_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&bf3920_isp_setting;
		isp_setting.size = ARRAY_SIZE(bf3920_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&bf3920_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bf3920_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3920_info *info = container_of(sd, struct bf3920_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = bf3920_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = bf3920_set_contrast(sd, ctrl->value);
		if (!ret)
			info->contrast = ctrl->value;
		break;
	case V4L2_CID_SATURATION:
		ret = bf3920_set_saturation(sd, ctrl->value);
		if (!ret)
			info->saturation = ctrl->value;
		break;
	case V4L2_CID_EXPOSURE:
		ret = bf3920_set_exposure(sd, ctrl->value);
		if (!ret)
			info->exposure = ctrl->value;
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		ret = bf3920_set_wb(sd, ctrl->value);
		if (!ret)
			info->wb = ctrl->value;
		break;
	case V4L2_CID_EFFECT:
		ret = bf3920_set_effect(sd, ctrl->value);
		if (!ret)
			info->effect = ctrl->value;
		break;
	case V4L2_CID_FRAME_RATE:
		info->frame_rate = ctrl->value;
		break;
	case V4L2_CID_SET_FOCUS:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bf3920_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_BF3920, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bf3920_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bf3920_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int bf3920_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	bf3920_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops bf3920_core_ops = {
	.g_chip_ident = bf3920_g_chip_ident,
	.g_ctrl = bf3920_g_ctrl,
	.s_ctrl = bf3920_s_ctrl,
	.queryctrl = bf3920_queryctrl,
	.reset = bf3920_reset,
	.init = bf3920_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = bf3920_g_register,
	.s_register = bf3920_s_register,
#endif
};

static const struct v4l2_subdev_video_ops bf3920_video_ops = {
	.enum_mbus_fmt = bf3920_enum_mbus_fmt,
	.try_mbus_fmt = bf3920_try_mbus_fmt,
	.s_mbus_fmt = bf3920_s_mbus_fmt,
	.s_stream = bf3920_s_stream,
	.cropcap = bf3920_cropcap,
	.g_crop	= bf3920_g_crop,
	.s_parm = bf3920_s_parm,
	.g_parm = bf3920_g_parm,
	.enum_frameintervals = bf3920_enum_frameintervals,
	.enum_framesizes = bf3920_enum_framesizes,
};

static const struct v4l2_subdev_ops bf3920_ops = {
	.core = &bf3920_core_ops,
	.video = &bf3920_video_ops,
};

static int bf3920_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct bf3920_info *info;
	int ret;

	info = kzalloc(sizeof(struct bf3920_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &bf3920_ops);

	/* Make sure it's an bf3920 */
	ret = bf3920_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an bf3920 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "bf3920 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int bf3920_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf3920_info *info = container_of(sd, struct bf3920_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id bf3920_id[] = {
	{ "bf3920", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf3920_id);

static struct i2c_driver bf3920_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bf3920",
	},
	.probe		= bf3920_probe,
	.remove		= bf3920_remove,
	.id_table	= bf3920_id,
};

static __init int init_bf3920(void)
{
	return i2c_add_driver(&bf3920_driver);
}

static __exit void exit_bf3920(void)
{
	i2c_del_driver(&bf3920_driver);
}

fs_initcall(init_bf3920);
module_exit(exit_bf3920);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore bf3920 sensors");
MODULE_LICENSE("GPL");

