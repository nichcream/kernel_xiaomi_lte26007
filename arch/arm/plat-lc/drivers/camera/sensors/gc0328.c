
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
#include "gc0328.h"

#define GC0328_CHIP_ID	(0x9d)

#define VGA_WIDTH	640
#define VGA_HEIGHT	480
#define QVGA_WIDTH	320
#define QVGA_HEIGHT	240
#define CIF_WIDTH	352
#define CIF_HEIGHT	288
#define QCIF_WIDTH	176
#define	QCIF_HEIGHT	144
#define MAX_WIDTH	VGA_WIDTH
#define MAX_HEIGHT	VGA_HEIGHT
#define MAX_PREVIEW_WIDTH	VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  VGA_HEIGHT

#define GC0328_REG_END	0xff
#define GC0328_REG_VAL_HVFLIP 0xff

//#define GC0328_DIFF_SIZE

struct gc0328_format_struct;
struct gc0328_info {
	struct v4l2_subdev sd;
	struct gc0328_format_struct *fmt;
	struct gc0328_win_size *win;
	int brightness;
	int contrast;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list gc0328_init_regs[] = {
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xfc, 0x16},
	
	{0xfe, 0x00},
	{0x4f, 0x00}, 
	{0x42, 0x00}, 
	{0x03, 0x00}, 
	{0x04, 0xc0}, 
	{0x77, 0x62}, 
	{0x78, 0x40}, 
	{0x79, 0x4d}, 

	{0x05, 0x02}, 
	{0x06, 0x2c}, 
	{0x07, 0x00}, 
	{0x08, 0xb8}, 
	
	{0xfe, 0x01}, 
	{0x29, 0x00}, 
	{0x2a, 0x60}, 
	{0x2b, 0x02},
	{0x2c, 0xa0},
	{0x2d, 0x03},  
	{0x2e, 0x00},
	{0x2f, 0x03},
	{0x30, 0xc0},
	{0x31, 0x05},
	{0x32, 0x40},
	{0xfe, 0x00},

//*********AWB**********//	
	{0xfe , 0x01},
	{0x4f , 0x00},
	{0x4c , 0x01}, 
	{0xfe , 0x00}, 
	{0xfe , 0x01}, 
	{0x51 , 0x80},
	{0x52 , 0x12},
	{0x53 , 0x80}, 
	{0x54 , 0x60},
	{0x55 , 0x01}, 
	{0x56 , 0x06},
	{0x5b , 0x02},
	{0x61 , 0xdc},
	{0x62 , 0xdc},
	{0x7c , 0x71},
	{0x7d , 0x00},
	{0x76 , 0x00},
	{0x79 , 0x20},
	{0x7b , 0x00},
	{0x70 , 0xFF},
	{0x71 , 0x00},
	{0x72 , 0x10},
	{0x73 , 0x40},
	{0x74 , 0x40},
	
	{0x50 , 0x00},
	{0xfe , 0x01},
	{0x4f , 0x00},
	{0x4c , 0x01},
	{0x4f , 0x00},
	{0x4f , 0x00},
	{0x4f , 0x00},//wait clr finish   
	{0x4d , 0x36},  //40  
	{0x4e , 0x02},  //SPL D65 
	{0x4e , 0x02},  //SPL D65 
	{0x4d , 0x44},  //40
	{0x4e , 0x02},
	{0x4e , 0x02},
	{0x4e , 0x02},  //SPL D65
	{0x4e , 0x02},   //SPL D65		   
	{0x4d , 0x53},  //50
	{0x4e , 0x08},  //SPL CWF
	{0x4e , 0x08},   //DNP
	{0x4e , 0x02},  //04//pink      //DN
	{0x4d , 0x63},  //60
	{0x4e , 0x08},	//TL84
	{0x4e , 0x08},	//TL84
	{0x4d , 0x73},  //80 
	{0x4e , 0x20},  //SPL   A 
	{0x4d , 0x83},  //80
	{0x4e , 0x20},  //SPL   A 
	{0x4f , 0x01},   
	
	{0x50 , 0x88}, 
	{0xfe , 0x00},  //page0
////////////////////////////////////////////////
////////////     BLK      //////////////////////
////////////////////////////////////////////////	
	{0x27 , 0x00}, 
	{0x2a , 0x40},
	{0x2b , 0x40},
	{0x2c , 0x40},
	{0x2d , 0x40},
//////////////////////////////////////////////
////////// page  0    ////////////////////////
//////////////////////////////////////////////	
	{0xfe , 0x00},
	{0x0d , 0x01}, 
	{0x0e , 0xe8},
	{0x0f , 0x02},
	{0x10 , 0x88}, 
	{0x09 , 0x00}, 
	{0x0a , 0x00}, 
	{0x0b , 0x00}, 
	{0x0c , 0x00}, 
	{0x16 , 0x00}, 
	{0x17 , 0x14}, 
	{0x18 , 0x0e}, 
	{0x19 , 0x06},
	
	{0x1b , 0x48}, 
	{0x1f , 0xC8}, 
	{0x20 , 0x01}, 
	{0x21 , 0x78}, 
	{0x22 , 0xb0}, 
	{0x23 , 0x06}, 
	{0x24 , 0x11}, 
	{0x26 , 0x00}, 
	
	{0x50 , 0x01}, 
	
	{0x70 , 0x85}, 
////////////////////////////////////////////////
////////////     block enable      /////////////
////////////////////////////////////////////////	
	{0x40 , 0x7f}, 
	{0x41 , 0x26}, 
	{0x42 , 0xff},
	{0x45 , 0x00},
	{0x44 , 0x02},
	{0x46 , 0x02},
	
	{0x4b , 0x01}, 
	{0x50 , 0x01}, 
	 
	{0x7e , 0x0a},
	{0x7f , 0x03},
	{0x81 , 0x15},
	{0x82 , 0x85},
	{0x83 , 0x02},
	{0x84 , 0xe5},
	{0x90 , 0xac},
	{0x92 , 0x02},
	{0x94 , 0x02},
	{0x95 , 0x54},
	
	{0xd1 , 0x32}, 
	{0xd2 , 0x32}, 
	{0xdd , 0x18}, 
	{0xde , 0x32}, 
	{0xe4 , 0x88}, 
	{0xe5 , 0x40}, 
	{0xd7 , 0x0e},
	
///////////////////////////// 
//////////////// GAMMA ////// 
///////////////////////////// 
	{0xfe , 0x00},
	{0xbf , 0x10},
	{0xc0 , 0x1c},
	{0xc1 , 0x33},
	{0xc2 , 0x48},
	{0xc3 , 0x5a},
	{0xc4 , 0x6b}, 
	{0xc5 , 0x7b},
	{0xc6 , 0x95},
	{0xc7 , 0xab},
	{0xc8 , 0xbf},
	{0xc9 , 0xcd},
	{0xca , 0xd9},
	{0xcb , 0xe3},
	{0xcc , 0xeb},
	{0xcd , 0xf7},
	{0xce , 0xfd},
	{0xcf , 0xff},
	
	{0xfe , 0x00},
	{0x63 , 0x00},
	{0x64 , 0x05},
	{0x65 , 0x0c},
	{0x66 , 0x1a},
	{0x67 , 0x29},
	{0x68 , 0x39},
	{0x69 , 0x4b},
	{0x6a , 0x5e},
	{0x6b , 0x82},
	{0x6c , 0xa4},
	{0x6d , 0xc5},
	{0x6e , 0xe5},
	{0x6f , 0xFF},
	
	{0xfe , 0x01},
	{0x18 , 0x02},
	{0xfe , 0x00},
	{0x98 , 0x00},
	{0x9b , 0x20},
	{0x9c , 0x80},
	{0xa4 , 0x10},
	{0xa8 , 0xB0},
	{0xaa , 0x40},
	{0xa2 , 0x23},
	{0xad , 0x01},
	
//////////////////////////////////////////////
////////// AEC    ////////////////////////
//////////////////////////////////////////////	
	{0xfe , 0x01},
	{0x9c , 0x02},
	{0x08 , 0xa0},
	{0x09 , 0xe8},
	
	{0x10 , 0x00},
	{0x11 , 0x11},
	{0x12 , 0x10},
	{0x13 , 0x80},
	{0x15 , 0xfc},
	{0x18 , 0x03},
	{0x21 , 0xc0},
	{0x22 , 0x60},
	{0x23 , 0x30},
	{0x25 , 0x00},
	{0x24 , 0x14},
	
//////////////////////////////////////
////////////LSC//////////////////////
//////////////////////////////////////
//gc0328 Alight lsc reg setting list
	
	{0xfe , 0x01},
	{0xc0 , 0x10},
	{0xc1 , 0x0c},
	{0xc2 , 0x0a},
	{0xc6 , 0x0e},
	{0xc7 , 0x0b},
	{0xc8 , 0x0a},
	{0xba , 0x26},
	{0xbb , 0x1c},
	{0xbc , 0x1d},
	{0xb4 , 0x23},
	{0xb5 , 0x1c},
	{0xb6 , 0x1a},
	{0xc3 , 0x00},
	{0xc4 , 0x00},
	{0xc5 , 0x00},
	{0xc9 , 0x00},
	{0xca , 0x00},
	{0xcb , 0x00},
	{0xbd , 0x00},
	{0xbe , 0x00},
	{0xbf , 0x00},
	{0xb7 , 0x07},
	{0xb8 , 0x05},
	{0xb9 , 0x05},
	{0xa8 , 0x07},
	{0xa9 , 0x06},
	{0xaa , 0x00},
	{0xab , 0x04},
	{0xac , 0x00},
	{0xad , 0x02},
	{0xae , 0x0d},
	{0xaf , 0x05},
	{0xb0 , 0x00},
	{0xb1 , 0x07},
	{0xb2 , 0x03},
	{0xb3 , 0x00},
	{0xa4 , 0x00},
	{0xa5 , 0x00},
	{0xa6 , 0x00},
	{0xa7 , 0x00},
	{0xa1 , 0x3c},
	{0xa2 , 0x50},
	{0xfe , 0x00},
	
	{0xB1 , 0x04},
	{0xB2 , 0xfd},
	{0xB3 , 0xfc},
	{0xB4 , 0xf0},
	{0xB5 , 0x05},
	{0xB6 , 0xf0},

	
	{0xfe , 0x00},
	{0x27 , 0xf7},
	{0x28 , 0x7F},
	{0x29 , 0x20},
	{0x33 , 0x20},
	{0x34 , 0x20},
	{0x35 , 0x20},
	{0x36 , 0x20},
	{0x32 , 0x08},
	
	{0x47 , 0x00},
	{0x48 , 0x00},
	
	{0xfe , 0x01},
	{0x79 , 0x00},
	{0x7d , 0x00},
	{0x50 , 0x88},
	{0x5b , 0x0c},
	{0x76 , 0x8f},
	{0x80 , 0x70},
	{0x81 , 0x70},
	{0x82 , 0xb0},
	{0x70 , 0xff},
	{0x71 , 0x00},
	{0x72 , 0x28},
	{0x73 , 0x0b},
	{0x74 , 0x0b},
	
	{0xfe , 0x00},
	{0x70 , 0x45},
	{0x4f , 0x01},
	{0xf1 , 0x07},

	{0xf2 , 0x01},	

	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_fmt_yuv422[] = {
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_win_vga[] = {
	//crop window
	//{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x01},
	{0x56, 0xe0},
	{0x57, 0x02},
	{0x58, 0x80},
	//1/2 subsample
	{0x59, 0x11},
	{0x5a, 0x0e},
	{0x5b, 0x02},
	{0x5c, 0x04},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x04},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x03},
	{0x30, 0x4e},//16.66fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0328_REG_END, 0x00},	/* END MARKER */
};

#ifdef GC0328_DIFF_SIZE
static struct regval_list gc0328_win_cif[] = {
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x10},
	{0x55, 0x01},
	{0x56, 0x20},
	{0x57, 0x01},
	{0x58, 0x60},
	//4/5 subsample
	{0x59, 0x55},
	{0x5a, 0x0e},
	{0x5b, 0x02},
	{0x5c, 0x04},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x04},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x03},
	{0x30, 0x4e},//16.66fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_win_qvga[] = {
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x00},
	{0x56, 0xf0},
	{0x57, 0x01},
	{0x58, 0x40},
	//1/2 subsample
	{0x59, 0x22},
	{0x5a, 0x03},
	{0x5b, 0x00},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x03},
	{0x30, 0x4e},//16.66fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_win_qcif[] = {
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x08},
	{0x53, 0x00},
	{0x54, 0x38},
	{0x55, 0x00},
	{0x56, 0x90},
	{0x57, 0x00},
	{0x58, 0xb0},
	//2/5 subsample
	{0x59, 0x55},
	{0x5a, 0x03},
	{0x5b, 0x02},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x03},
	{0x30, 0x4e},//16.66fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_win_144x176[] = {
	// page 0
	{0xfe, 0x00},
	{0x17, 0x14},
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x08},
	{0x53, 0x00},
	{0x54, 0x38},
	{0x55, 0x00},
	{0x56, 0xb8},
	{0x57, 0x00},
	{0x58, 0x98},
	//2/5 subsample
	{0x59, 0x55},
	{0x5a, 0x03},
	{0x5b, 0x02},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps15
	{0x05, 0x02},
	{0x06, 0x5e},//hb:0x25e
	{0x07, 0x00},
	{0x08, 0xd4},//vb:0x0d4

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x64},//step:0x64

	{0x2b, 0x02},
	{0x2c, 0xbc},//14.28fps
	{0x2d, 0x03},
	{0x2e, 0x20},//12.5fps
	{0x2f, 0x04},
	{0x30, 0xb0},//8.3fps
	{0x31, 0x06},
	{0x32, 0x40},//6.25fps
	{0x33, 0x00},//fix framerate at 14.28fps
	{0xfe, 0x00},//page0

	{GC0328_REG_END, 0x00},	/* END MARKER */
};
#endif

static struct regval_list gc0328_brightness_l3[] = {
	{0xd5, 0xc0},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_brightness_l2[] = {
	{0xd5, 0xe0},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_brightness_l1[] = {
	{0xd5, 0xf0},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_brightness_h0[] = {
	{0xd5, 0x00},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_brightness_h1[] = {
	{0xd5, 0x10},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_brightness_h2[] = {
	{0xd5, 0x20},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_brightness_h3[] = {
	{0xd5, 0x30},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_contrast_l3[] = {
	{0xd3, 0x28},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_contrast_l2[] = {
	{0xd3, 0x30},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_contrast_l1[] = {
	{0xd3, 0x38},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_contrast_h0[] = {
	{0xd3, 0x40},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_contrast_h1[] = {
	{0xd3, 0x48},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_contrast_h2[] = {
	{0xd3, 0x50},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_contrast_h3[] = {
	{0xd3, 0x58},	
	{GC0328_REG_END, 0x00},	/* END MARKER */
};

static int gc0328_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int gc0328_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc0328_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	unsigned char val = 0x14;
	int ret = 0;

	while (vals->reg_num != GC0328_REG_END) {
		if (vals->reg_num == 0x17 && vals->value == GC0328_REG_VAL_HVFLIP) {
			if (gc0328_isp_parm.mirror)
				val |= 0x01;
			else
				val &= 0xfe;

			if (gc0328_isp_parm.flip)
				val |= 0x02;
			else
				val &= 0xfd;

			vals->value = val;			
		}
		ret = gc0328_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int gc0328_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int gc0328_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc0328_info *info = container_of(sd, struct gc0328_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return gc0328_write_array(sd, gc0328_init_regs);
}

static int gc0328_detect(struct v4l2_subdev *sd)
{
	unsigned char v=0x0;
	int ret;

	ret = gc0328_write(sd, 0xfc, 0x16);
		if (ret < 0)
			return ret;

	ret = gc0328_read(sd, 0xfc, &v);
	if (ret < 0)
		return ret;
	
	ret = gc0328_read(sd, 0xf0, &v);
	printk("gc0328 ensor_id = %0x\n",v);
	if (ret < 0)
		return ret;
	
	if (v != GC0328_CHIP_ID)
		return -ENODEV;
	return 0;
}

static struct gc0328_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc0328_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= gc0328_fmt_yuv422,
	},
};
#define N_GC0328_FMTS ARRAY_SIZE(gc0328_formats)

static struct gc0328_win_size {
	int	width;
	int	height;
	struct regval_list *regs;
} gc0328_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= gc0328_win_vga,
	},
#ifdef GC0328_DIFF_SIZE
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
		.regs 		= gc0328_win_cif,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
		.regs 		= gc0328_win_qvga,
	},
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
		.regs 		= gc0328_win_qcif,
	},
	/* 144x176 */
	{
		.width		= QCIF_HEIGHT,
		.height		= QCIF_WIDTH,
		.regs 		= gc0328_win_144x176,
	},
#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(gc0328_win_sizes))

static int gc0328_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC0328_FMTS)
		return -EINVAL;

	*code = gc0328_formats[index].mbus_code;
	return 0;
}

static int gc0328_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc0328_format_struct **ret_fmt,
		struct gc0328_win_size **ret_wsize)
{
	int index;
	struct gc0328_win_size *wsize;

	for (index = 0; index < N_GC0328_FMTS; index++)
		if (gc0328_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC0328_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc0328_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc0328_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = gc0328_win_sizes; wsize < gc0328_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc0328_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc0328_formats[index].colorspace;
	return 0;
}

static int gc0328_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc0328_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc0328_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc0328_info *info = container_of(sd, struct gc0328_info, sd);
	struct gc0328_format_struct *fmt_s;
	struct gc0328_win_size *wsize;
	int ret;

	ret = gc0328_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc0328_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = gc0328_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int gc0328_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc0328_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc0328_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc0328_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc0328_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc0328_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc0328_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc0328_frame_rates[interval->index];
	return 0;
}

static int gc0328_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc0328_win_size *win = &gc0328_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc0328_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc0328_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[GC0328]set brightness %d\n", brightness);

	switch (brightness) {
	case BRIGHTNESS_L3:
		gc0328_write_array(sd, gc0328_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		gc0328_write_array(sd, gc0328_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		gc0328_write_array(sd, gc0328_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		gc0328_write_array(sd, gc0328_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		gc0328_write_array(sd, gc0328_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		gc0328_write_array(sd, gc0328_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		gc0328_write_array(sd, gc0328_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int gc0328_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk(KERN_DEBUG "[GC0328]set contrast %d\n", contrast);

	switch (contrast) {
	case CONTRAST_L3:
		gc0328_write_array(sd, gc0328_contrast_l3);
		break;
	case CONTRAST_L2:
		gc0328_write_array(sd, gc0328_contrast_l2);
		break;
	case CONTRAST_L1:
		gc0328_write_array(sd, gc0328_contrast_l1);
		break;
	case CONTRAST_H0:
		gc0328_write_array(sd, gc0328_contrast_h0);
		break;
	case CONTRAST_H1:
		gc0328_write_array(sd, gc0328_contrast_h1);
		break;
	case CONTRAST_H2:
		gc0328_write_array(sd, gc0328_contrast_h2);
		break;
	case CONTRAST_H3:
		gc0328_write_array(sd, gc0328_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int gc0328_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0328_info *info = container_of(sd, struct gc0328_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&gc0328_isp_setting;
		isp_setting.size = ARRAY_SIZE(gc0328_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&gc0328_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int gc0328_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0328_info *info = container_of(sd, struct gc0328_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = gc0328_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = gc0328_set_contrast(sd, ctrl->value);
		if (!ret)
			info->contrast = ctrl->value;
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

static int gc0328_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC0328, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc0328_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc0328_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int gc0328_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc0328_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}

int gc0328_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;
	printk("########gc0328 read init regs####################\n");

	//return 0;
	while (vals->reg_num != GC0328_REG_END) {
		/*if (vals->reg_num == GC0328_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else
		*/
		{			
			ret = gc0328_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}

static int gc0328_s_stream(struct v4l2_subdev *sd)
{
	int ret = 0;

	printk("GC0328_s_stream\n");	
	gc0328_read_array(sd, gc0328_init_regs);//debug only
	return ret;
}
#endif

static const struct v4l2_subdev_core_ops gc0328_core_ops = {
	.g_chip_ident = gc0328_g_chip_ident,
	.g_ctrl = gc0328_g_ctrl,
	.s_ctrl = gc0328_s_ctrl,
	.queryctrl = gc0328_queryctrl,
	.reset = gc0328_reset,
	.init = gc0328_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc0328_g_register,
	.s_register = gc0328_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc0328_video_ops = {
	.enum_mbus_fmt = gc0328_enum_mbus_fmt,
	.try_mbus_fmt = gc0328_try_mbus_fmt,
	.s_mbus_fmt = gc0328_s_mbus_fmt,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.s_stream = gc0328_s_stream,
#endif
	.cropcap = gc0328_cropcap,
	.g_crop	= gc0328_g_crop,
	.s_parm = gc0328_s_parm,
	.g_parm = gc0328_g_parm,
	.enum_frameintervals = gc0328_enum_frameintervals,
	.enum_framesizes = gc0328_enum_framesizes,
};

static const struct v4l2_subdev_ops gc0328_ops = {
	.core = &gc0328_core_ops,
	.video = &gc0328_video_ops,
};

static int gc0328_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc0328_info *info;
	struct comip_camera_subdev_platform_data *gc0328_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct gc0328_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc0328_ops);

	/* Make sure it's an gc0328 */
	ret = gc0328_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc0328 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc0328 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	gc0328_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (gc0328_setting) {
		if (gc0328_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			gc0328_isp_parm.mirror = 1;
		else
			gc0328_isp_parm.mirror = 0;

		if (gc0328_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			gc0328_isp_parm.flip = 1;
		else
			gc0328_isp_parm.flip = 0;
	}

	return 0;
}

static int gc0328_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0328_info *info = container_of(sd, struct gc0328_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc0328_id[] = {
	{ "gc0328", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0328_id);

static struct i2c_driver gc0328_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc0328",
	},
	.probe		= gc0328_probe,
	.remove		= gc0328_remove,
	.id_table	= gc0328_id,
};

static __init int init_gc0328(void)
{
	return i2c_add_driver(&gc0328_driver);
}

static __exit void exit_gc0328(void)
{
	i2c_del_driver(&gc0328_driver);
}

fs_initcall(init_gc0328);
module_exit(exit_gc0328);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore gc0328 sensors");
MODULE_LICENSE("GPL");
