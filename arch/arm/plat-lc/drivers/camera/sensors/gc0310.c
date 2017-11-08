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
#include "gc0310.h"

#define GC0310_CHIP_ID_H	(0xa3)
#define GC0310_CHIP_ID_L	(0x10)

#define VGA_WIDTH		640
#define VGA_HEIGHT		480
#define MAX_WIDTH		640
#define MAX_HEIGHT		480

#define MAX_PREVIEW_WIDTH	VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  VGA_HEIGHT

#define V4L2_I2C_ADDR_8BIT	(0x0001)
#define GC0310_REG_END		0xff

#define ISO_MAX_GAIN      0x50
#define ISO_MIN_GAIN      0x20


struct gc0310_format_struct;

struct gc0310_info {
	struct v4l2_subdev sd;
	struct gc0310_format_struct *fmt;
	struct gc0310_win_size *win;
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


static struct regval_list gc0310_init_regs[] = {
	{0xfe,0xf0},
	{0xfe,0xf0},
	{0xfe,0x00},
	{0xfc,0x0e},
	{0xfc,0x0e},
	{0xf2,0x80},
	{0xf3,0x00},// output_disable
	{0xf7,0x1b},
	{0xf8,0x04},
	{0xf9,0x8e},
	{0xfa,0x11},
	/////////////////////////////////////////////////      
	///////////////////   MIPI   ////////////////////      
	/////////////////////////////////////////////////      
	{0xfe,0x03},
	{0x40,0x08},
	{0x42,0x00},
	{0x43,0x00},
	{0x01,0x03},
	{0x10,0x84},

/* 
	{0x01,0x03}, ///mipi 1lane
	{0x02,0x11}, 
	{0x03,0x94}, // 14 bit7: clock_zero
	{0x04,0x01}, // fifo_prog 
	{0x05,0x00}, //fifo_prog 
	{0x06,0x80}, //
	{0x11,0x1e}, //LDI set YUV422
	{0x12,0x00}, //04 //00 //04//00 //LWC[7:0]	//
	{0x13,0x05}, //05 //LWC[15:8]
	{0x15,0x10}, //DPHYY_MODE read_ready 
	{0x21,0x10},
	{0x22,0x01},
	{0x23,0x10},
	{0x24,0x02},
	{0x25,0x10},
	{0x26,0x03},
	{0x29,0x01},
	{0x2a,0x0a},
	{0x2b,0x03},
	{0xfe,0x00},
 */                                      
	{0x01,0x03},             
	{0x02,0x00},             
	{0x03,0x94},             
	{0x04,0x01},            
	{0x05,0x00},             
	{0x06,0x80},             
	{0x11,0x1e},             
	{0x12,0x00},      
	{0x13,0x05},             
	{0x15,0x10},                                                                    
	{0x21,0x10},             
	{0x22,0x01},             
	{0x23,0x10},                                             
	{0x24,0x02},                                             
	{0x25,0x10},                                             
	{0x26,0x03},                                             
	{0x29,0x02},                                             
	{0x2a,0x0a},                                             
	{0x2b,0x04},                                             
	{0xfe,0x00},
	
	/////////////////////////////////////////////////
	/////////////////  CISCTL reg	/////////////////
	/////////////////////////////////////////////////
	{0x00,0x2f},
	{0x01,0x0f},//06
	{0x02,0x04},
	{0x03,0x03},
	{0x04,0x50},
	{0x09,0x00},
	{0x0a,0x00},
	{0x0b,0x00},
	{0x0c,0x04},
	{0x0d,0x01},
	{0x0e,0xe8},
	{0x0f,0x02},
	{0x10,0x88},
	{0x16,0x00},
	{0x17,0x14},
	{0x18,0x1a},
	{0x19,0x14},
	{0x1b,0x48},
	{0x1e,0x6b},
	{0x1f,0x28},
	{0x20,0x8b},//0x89 travis20140801
	{0x21,0x49},
	{0x22,0xb0},
	{0x23,0x04},
	{0x24,0x16},
	{0x34,0x20},
	
	/////////////////////////////////////////////////
	////////////////////   BLK	 ////////////////////
	/////////////////////////////////////////////////
	{0x26,0x23},
	{0x28,0xff},
	{0x29,0x00},
	{0x33,0x10}, 
	{0x37,0x20},
	{0x38,0x10},
	{0x47,0x80},
	{0x4e,0x66},
	{0xa8,0x02},
	{0xa9,0x80},
	
	/////////////////////////////////////////////////
	//////////////////	ISP reg   ///////////////////
	/////////////////////////////////////////////////
	{0x40,0xff},
	{0x41,0x21},
	{0x42,0xcf},
	{0x44,0x02},
	{0x45,0xa8}, 
	{0x46,0x02}, //sync
	{0x4a,0x11},
	{0x4b,0x01},
	{0x4c,0x20},
	{0x4d,0x05},
	{0x4f,0x01},
	{0x50,0x01},
	{0x55,0x01},
	{0x56,0xe0},
	{0x57,0x02},
	{0x58,0x80},
	
	/////////////////////////////////////////////////
	///////////////////   GAIN   ////////////////////
	/////////////////////////////////////////////////
	{0x70,0x70},
	{0x5a,0x84},
	{0x5b,0xc9},
	{0x5c,0xed},
	{0x77,0x74},
	{0x78,0x40},
	{0x79,0x5f},
	
	///////////////////////////////////////////////// 
	///////////////////   DNDD  /////////////////////
	///////////////////////////////////////////////// 
	{0x82,0x14}, 
	{0x83,0x0b},
	{0x89,0xf0},
	
	///////////////////////////////////////////////// 
	//////////////////   EEINTP  ////////////////////
	///////////////////////////////////////////////// 
	{0x8f,0xaa}, 
	{0x90,0x8c}, 
	{0x91,0x90},
	{0x92,0x03}, 
	{0x93,0x03}, 
	{0x94,0x05}, 
	{0x95,0x65}, 
	{0x96,0xf0}, 
	
	///////////////////////////////////////////////// 
	/////////////////////  ASDE  ////////////////////
	///////////////////////////////////////////////// 
	{0xfe,0x00},

	{0x9a,0x20},
	{0x9b,0x80},
	{0x9c,0x40},
	{0x9d,0x80},
	 
	{0xa1,0x30},
 	{0xa2,0x32},
	{0xa4,0x30},
	{0xa5,0x30},
	{0xaa,0x10}, 
	{0xac,0x22},
	 


	
	/////////////////////////////////////////////////
	///////////////////   GAMMA   ///////////////////
	/////////////////////////////////////////////////
	{0xfe,0x00},//default
	{0xbf,0x08},
	{0xc0,0x16},
	{0xc1,0x28},
	{0xc2,0x41},
	{0xc3,0x5a},
	{0xc4,0x6c},
	{0xc5,0x7a},
	{0xc6,0x96},
	{0xc7,0xac},
	{0xc8,0xbc},
	{0xc9,0xc9},
	{0xca,0xd3},
	{0xcb,0xdd},
	{0xcc,0xe5},
	{0xcd,0xf1},
	{0xce,0xfa},
	{0xcf,0xff},
	
/* 
	{0xfe,0x00},//big gamma
	{0xbf,0x08},
	{0xc0,0x1d},
	{0xc1,0x34},
	{0xc2,0x4b},
	{0xc3,0x60},
	{0xc4,0x73},
	{0xc5,0x85},
	{0xc6,0x9f},
	{0xc7,0xb5},
	{0xc8,0xc7},
	{0xc9,0xd5},
	{0xca,0xe0},
	{0xcb,0xe7},
	{0xcc,0xec},
	{0xcd,0xf4},
	{0xce,0xfa},
	{0xcf,0xff},
*/	

/*
	{0xfe,0x00},//small gamma
	{0xbf,0x08},
	{0xc0,0x18},
	{0xc1,0x2c},
	{0xc2,0x41},
	{0xc3,0x59},
	{0xc4,0x6e},
	{0xc5,0x81},
	{0xc6,0x9f},
	{0xc7,0xb5},
	{0xc8,0xc7},
	{0xc9,0xd5},
	{0xca,0xe0},
	{0xcb,0xe7},
	{0xcc,0xec},
	{0xcd,0xf4},
	{0xce,0xfa},
	{0xcf,0xff},
*/
	/////////////////////////////////////////////////
	///////////////////   YCP  //////////////////////
	/////////////////////////////////////////////////
	{0xd0,0x40}, 
	{0xd1,0x34}, 
	{0xd2,0x34}, 
	{0xd3,0x40}, 
	{0xd6,0xf2}, 
	{0xd7,0x1b}, 
	{0xd8,0x18}, 
	{0xdd,0x03}, 
                                
	/////////////////////////////////////////////////
	////////////////////   AEC   ////////////////////
	/////////////////////////////////////////////////
	{0xfe,0x01},
	{0x05,0x30},
	{0x06,0x75},
	{0x07,0x40},
	{0x08,0xb0},
	{0x0a,0xc5},
	{0x0b,0x11},
	{0x0c,0x00},
	{0x12,0x52},
	{0x13,0x38},
	{0x18,0x95},
	{0x19,0x96},
	{0x1f,0x20},
	{0x20,0xc0}, 
	{0x3e,0x40}, 
	{0x3f,0x57}, 
	{0x40,0x7d}, 
	{0x03,0x60}, 
	{0x44,0x02}, 
                                
	/////////////////////////////////////////////////
	////////////////////   AWB   ////////////////////
	/////////////////////////////////////////////////
	{0x1c,0x91}, 
	{0x21,0x15}, 
	{0x50,0x80},
	{0x56,0x04},
	{0x59,0x08}, 
	{0x5b,0x02},
	{0x61,0x8d}, 
	{0x62,0xa7}, 
	{0x63,0xd0}, 
	{0x65,0x06},
	{0x66,0x06}, 
	{0x67,0x84}, 
	{0x69,0x08}, 
	{0x6a,0x25}, 
	{0x6b,0x01}, 
	{0x6c,0x00}, 
	{0x6d,0x02}, 
	{0x6e,0xf0}, 
	{0x6f,0x80}, 
	{0x76,0x80}, 
	{0x78,0xaf}, 
	{0x79,0x75},
	{0x7a,0x40},
	{0x7b,0x50},
	{0x7c,0x0c}, 
                                
	{0xa4,0xb9}, 
	{0xa5,0xa0},
	{0x90,0xc9}, 
	{0x91,0xbe},
                                
	{0xa6,0xb8}, 
	{0xa7,0x95}, 
	{0x92,0xe6}, 
	{0x93,0xca}, 
                                
	{0xa9,0xbc}, 
	{0xaa,0x95}, 
	{0x95,0x23}, 
	{0x96,0xe7}, 
                                
	{0xab,0x9d}, 
	{0xac,0x80},
	{0x97,0x43}, 
	{0x98,0x24}, 
                                
	{0xae,0xb7}, 
	{0xaf,0x9e}, 
	{0x9a,0x43},
	{0x9b,0x24}, 
                                
	{0xb0,0xc8}, 
	{0xb1,0x97},
	{0x9c,0xc4}, 
	{0x9d,0x44}, 
                                
	{0xb3,0xb7}, 
	{0xb4,0x7f},
	{0x9f,0xc7},
	{0xa0,0xc8}, 
                                
	{0xb5,0x00}, 
	{0xb6,0x00},
	{0xa1,0x00},
	{0xa2,0x00},
                                
	{0x86,0x60},
	{0x87,0x08},
	{0x88,0x00},
	{0x89,0x00},
	{0x8b,0xde},
	{0x8c,0x80},
	{0x8d,0x00},
	{0x8e,0x00},
                                
	{0x94,0x55},
	{0x99,0xa6},
	{0x9e,0xaa},
	{0xa3,0x0a},
	{0x8a,0x0a},
	{0xa8,0x55},
	{0xad,0x55},
	{0xb2,0x55},
	{0xb7,0x05},
	{0x8f,0x05},
                                
	{0xb8,0xcc},
	{0xb9,0x9a},
                                
	/////////////////////////////////////////////////
	////////////////////  CC ////////////////////////
	/////////////////////////////////////////////////
	{0xfe,0x01},
	
	{0xd0,0x38},//skin red
	{0xd1,0x00},
	{0xd2,0x02},
	{0xd3,0x04},
	{0xd4,0x38},
	{0xd5,0x12},	
/*                     
	{0xd0,0x38},//skin white
	{0xd1,0xfd},
	{0xd2,0x06},
	{0xd3,0xf0},
	{0xd4,0x40},
	{0xd5,0x08},
*/
	
/*                       
	{0xd0,0x38},//guodengxiang
	{0xd1,0xf8},
	{0xd2,0x06},
	{0xd3,0xfd},
	{0xd4,0x40},
	{0xd5,0x00},	
*/
	{0xd6,0x30},
	{0xd7,0x00},
	{0xd8,0x0a},
	{0xd9,0x16},
	{0xda,0x39},
	{0xdb,0xf8},
                                
	/////////////////////////////////////////////////
	////////////////////   LSC   ////////////////////
	/////////////////////////////////////////////////
	{0xfe,0x01}, 
	{0xc1,0x3c}, 
	{0xc2,0x50}, 
	{0xc3,0x00}, 
	{0xc4,0x40}, 
	{0xc5,0x30}, 
	{0xc6,0x30}, 
	{0xc7,0x10}, 
	{0xc8,0x00}, 
	{0xc9,0x00}, 
	{0xdc,0x20}, 
	{0xdd,0x10}, 
	{0xdf,0x00}, 
	{0xde,0x00}, 
                                
	/////////////////////////////////////////////////
	///////////////////  Histogram  /////////////////
	/////////////////////////////////////////////////
	{0x01,0x10}, 
	{0x0b,0x31}, 
	{0x0e,0x50}, 
	{0x0f,0x0f}, 
	{0x10,0x6e}, 
	{0x12,0xa0}, 
	{0x15,0x60}, 
	{0x16,0x60}, 
	{0x17,0xe0}, 
                                
	/////////////////////////////////////////////////
	//////////////   Measure Window   ///////////////
	/////////////////////////////////////////////////
	{0xcc,0x0c}, 
	{0xcd,0x10}, 
	{0xce,0xa0}, 
	{0xcf,0xe6}, 
                                
	/////////////////////////////////////////////////
	/////////////////   dark sun   //////////////////
	/////////////////////////////////////////////////
	{0x45,0xf7},
	{0x46,0xff}, 
	{0x47,0x15},
	{0x48,0x03}, 
	{0x4f,0x60}, 
                                
	/////////////////////////////////////////////////
	///////////////////  banding  ///////////////////
	/////////////////////////////////////////////////
	{0xfe,0x00},
	{0x05,0x02},
	{0x06,0xd1}, //HB
	{0x07,0x00},
	{0x08,0x22}, //VB
	{0xfe,0x01},
	{0x25,0x00}, //step 
	{0x26,0x6a}, 
	{0x27,0x02}, //20fps
	{0x28,0x12},  
	{0x29,0x03}, //12.5fps
	{0x2a,0x50}, 
	{0x2b,0x05}, //7.14fps
	{0x2c,0xcc}, 
	{0x2d,0x07}, //5.55fps
	{0x2e,0x74},
	{0x3c,0x20},
	{0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_stream_on[] = {
	{0xfe, 0x03},
	{0x10, 0x94},
	{0xfe, 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_stream_off[] = {
	/* Sensor enter LP11*/
	{0xfe, 0x03},
	{0x10, 0x84},
	{0xfe, 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_win_vga[] = {
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x01},
	{0x56, 0xe0},
	{0x57, 0x02},
	{0x58, 0x80},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list gc0310_sharpness_l2[] = {
	{0xfe , 0x00},{0x95 , 0x33},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_sharpness_l1[] = {
	{0xfe , 0x00},{0x95 , 0x44},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_sharpness_h0[] = {
	{0xfe , 0x00},{0x95 , 0x65},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_sharpness_h1[] = {
	{0xfe , 0x00},{0x95 , 0x77},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_sharpness_h2[] = {
	{0xfe , 0x00},{0x95 , 0x78},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_sharpness_h3[] = {
	{0xfe , 0x00},{0x95 , 0x88},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_sharpness_h4[] = {
	{0xfe , 0x00},{0x95 , 0x89},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_sharpness_h5[] = {
	{0xfe , 0x00},{0x95 , 0x98},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_sharpness_h6[] = {
	{0xfe , 0x00},{0x95 , 0x99},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_sharpness_h7[] = {
	{0xfe , 0x00},{0x95 , 0x9a},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_sharpness_h8[] = {
	{0xfe , 0x00},{0x95 , 0x9b},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_sharpness_max[] = {
	{0xfe , 0x00},{0x95 , 0x9d},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_brightness_l4[] = {
	{0xfe , 0x01},{0x13 , 0x18},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_brightness_l3[] = {
	{0xfe , 0x01},{0x13 , 0x20},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_brightness_l2[] = {
	{0xfe , 0x01},{0x13 , 0x28},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_brightness_l1[] = {
	{0xfe , 0x01},{0x13 , 0x30},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_brightness_h0[] = {
	{0xfe , 0x01},{0x13 , 0x38},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_brightness_h1[] = {
	{0xfe , 0x01},{0x13 , 0x40},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_brightness_h2[] = {
	{0xfe , 0x01},{0x13 , 0x48},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_brightness_h3[] = {
	{0xfe , 0x01},{0x13 , 0x50},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_brightness_h4[] = {
	{0xfe , 0x01},{0x13 , 0x58},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_saturation_l2[] = {
	{0xfe , 0x00},{0xd1 , 0x24},{0xd2, 0x24},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_saturation_l1[] = {
	{0xfe , 0x00},{0xd1 , 0x30},{0xd2, 0x30},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_saturation_h0[] = {
	{0xfe , 0x00},{0xd1 , 0x34},{0xd2, 0x34},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_saturation_h1[] = {
	{0xfe , 0x00},{0xd1 , 0x3c},{0xd2, 0x3c},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_saturation_h2[] = {
	{0xfe , 0x00},{0xd1 , 0x40},{0xd2, 0x40},

	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_l4[] = {
	{0xfe,0x00}, {0xd3,0x20}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_l3[] = {
	{0xfe,0x00}, {0xd3,0x24}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_l2[] = {
	{0xfe,0x00}, {0xd3,0x30}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_l1[] = {
	{0xfe,0x00}, {0xd3,0x34}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_h0[] = {
	{0xfe,0x00}, {0xd3,0x3c}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_h1[] = {
	{0xfe,0x00}, {0xd3,0x44}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_h2[] = {
	{0xfe,0x00}, {0xd3,0x4c}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_h3[] = {
	{0xfe,0x00}, {0xd3,0x50}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_contrast_h4[] = {
	{0xfe,0x00}, {0xd3,0x60}, {0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_whitebalance_auto[] = {
	{0x77,0x74}, {0x78,0x40}, {0x79,0x5f},{0x42,0xcf},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_whitebalance_incandescent[] = {
	{0x42,0xcd},{0x77,0x48}, {0x78,0x40}, {0x79,0x5c},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_whitebalance_fluorescent[] = {
	{0x42,0xcd},{0x77,0x48}, {0x78,0x40}, {0x79,0x5c},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_whitebalance_daylight[] = {
	{0x42,0xcd},{0x77,0x74}, {0x78,0x52}, {0x79,0x40},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_whitebalance_cloudy[] = {
	{0x42,0xcd},{0x77,0x8c}, {0x78,0x50}, {0x79,0x40},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_effect_none[] = {
	{0xfe,0x00}, {0x43,0x00},{0xfe,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_mono[] = {
	{0x43,0x02}, {0xd1,0x00},{0xdb,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_negative[] = {
	{0xfe,0x00}, {0x43,0x01},{0xfe,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_solarize[] = {
	{0xfe,0x00}, {0x43,0x01},{0xfe,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_sepia[] = {
	{0x43,0x02}, {0xd1,0xd0},{0xdb,0x28}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_posterize[] = {
	{0x43,0x02}, {0xd1,0xd0},{0xdb,0x28}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_whiteboard[] = {
	{0x43,0x02}, {0xd1,0x00},{0xdb,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_lightgreen[] = {
	{0x43,0x02}, {0xd1,0x00},{0xdb,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list gc0310_effect_brown[] = {
	{0x43,0x02}, {0xd1,0x00},{0xdb,0x00}, 
	{GC0310_REG_END, 0xff}, /* END MARKER */
};

static struct regval_list gc0310_effect_waterblue[] = {
	{0x43,0x02}, {0xd1,0x00},{0xdb,0x00}, 
	{GC0310_REG_END, 0xff}, /* END MARKER */
};


static struct regval_list gc0310_effect_blackboard[] = {
	{0x43,0x02}, {0xd1,0x00},{0xdb,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_effect_aqua[] = {
	{0x43,0x02}, {0xd1,0x00},{0xdb,0x00}, 
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_l6[] = {
	{0xfe , 0x01},{0x13 , 0x10},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_l5[] = {
	{0xfe , 0x01},{0x13 , 0x18},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_l4[] = {
	{0xfe , 0x01},{0x13 , 0x20},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_l3[] = {
	{0xfe , 0x01},{0x13 , 0x28},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_l2[] = {
	{0xfe , 0x01},{0x13 , 0x30},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_exposure_l1[] = {
	{0xfe , 0x01},{0x13 , 0x38},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_exposure_h0[] = {
	{0xfe , 0x01},{0x13 , 0x40},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_exposure_h1[] = {
	{0xfe , 0x01},{0x13 , 0x48},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_exposure_h2[] = {
	{0xfe , 0x01},{0x13 , 0x50},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_h3[] = {
	{0xfe , 0x01},{0x13 , 0x58},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_h4[] = {
	{0xfe , 0x01},{0x13 , 0x60},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_h5[] = {
	{0xfe , 0x01},{0x13 , 0x68},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list gc0310_exposure_h6[] = {
	{0xfe , 0x01},{0x13 , 0x70},{0xfe , 0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};


//next preview mode

static struct regval_list gc0310_previewmode_mode00[] = {
	{0xfe,0x01},
	{0x3c,0x20},
	{0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list gc0310_previewmode_mode01[] = {
	{0xfe,0x01},
	{0x3c,0x30},
	{0xfe,0x00},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

#if 0
static struct regval_list gc0310_scene_mode_auto[] = {
	{0xfe,0x01},
	{0x3c,0x20},
	{0xfe,0x00},
	{GC0310_REG_END, 0xff}, /* END MARKER */
};

static struct regval_list gc0310_scene_mode_night[] = {
	{0xfe,0x01},
	{0x3c,0x30},
	{0xfe,0x00},
	{GC0310_REG_END, 0xff}, /* END MARKER */

};
#endif

static struct regval_list gc0310_iso_auto[] = {
	{0x4f,0x01},
	{0x71,0x20},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_iso_100[] = {
	{0x4f,0x00},
	{0x71,0x20},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_iso_200[] = {
	{0x4f,0x00},
	{0x71,0x30},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_iso_400[] = {
	{0x4f,0x00},
	{0x71,0x40},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list gc0310_iso_800[] = {
	{0x4f,0x00},
	{0x71,0x60},
	{GC0310_REG_END, 0xff},	/* END MARKER */
};

//preview mode end
static int gc0310_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int gc0310_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc0310_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != GC0310_REG_END) {
		int ret = gc0310_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int gc0310_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}




static int gc0310_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc0310_info *info = container_of(sd, struct gc0310_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return gc0310_write_array(sd, gc0310_init_regs);
}

static int gc0310_detect(struct v4l2_subdev *sd)
	{
	unsigned char v;
	int ret;

	printk("gc0310_detect\n");
	ret = gc0310_read(sd, 0xf0, &v);
	printk("GC0310___CHIP_ID_H=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != GC0310_CHIP_ID_H)   //39
		return -ENODEV;

	ret = gc0310_read(sd, 0xf1, &v);
	printk("GC0310___CHIP_ID_L=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != GC0310_CHIP_ID_L)    //05
		return -ENODEV;

	return 0;
}


static struct gc0310_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc0310_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= NULL,
	},
};
#define N_GC0310_FMTS ARRAY_SIZE(gc0310_formats)

static struct gc0310_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
} gc0310_win_sizes[] = {
	/*SVGA*/
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= gc0310_win_vga,
	},

};
#define N_WIN_SIZES (ARRAY_SIZE(gc0310_win_sizes))

static int gc0310_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC0310_FMTS)
		return -EINVAL;

	*code = gc0310_formats[index].mbus_code;
	return 0;
}

static int gc0310_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc0310_format_struct **ret_fmt,
		struct gc0310_win_size **ret_wsize)
{
	int index;
	struct gc0310_win_size *wsize;

	for (index = 0; index < N_GC0310_FMTS; index++)
		if (gc0310_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC0310_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc0310_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc0310_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = gc0310_win_sizes; wsize < gc0310_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc0310_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc0310_formats[index].colorspace;
	return 0;
}

static int gc0310_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc0310_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc0310_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc0310_info *info = container_of(sd, struct gc0310_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct gc0310_format_struct *fmt_s;
	struct gc0310_win_size *wsize;
	int ret,len,i = 0;

	ret = gc0310_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;


	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc0310_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {

		memset(data, 0, sizeof(*data));
		data->mipi_clk = 195;//288; /* Mbps. */

		if ((wsize->width == VGA_WIDTH)
			&& (wsize->height == VGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_8BIT;
			data->slave_addr = client->addr;
			//msleep(500);
			len = ARRAY_SIZE(gc0310_win_vga);
			data->reg_num = len;
			for(i =0;i<len;i++) {
				data->reg[i].addr = gc0310_win_vga[i].reg_num;
				data->reg[i].data = gc0310_win_vga[i].value;
			}
		}
	}
	info->fmt = fmt_s;
	info->win = wsize;
	return 0;
}

static int gc0310_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct gc0310_info *info = container_of(sd, struct gc0310_info, sd);

	printk("gc0310_set_framerate %d\n", framerate);

	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;

	return 0;
}


static int gc0310_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = gc0310_write_array(sd, gc0310_stream_on);
	else
		ret = gc0310_write_array(sd, gc0310_stream_off);

	msleep(150);

	return ret;
}

static int gc0310_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc0310_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc0310_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc0310_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc0310_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc0310_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc0310_frame_rates))
		return -EINVAL;

	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc0310_frame_rates[interval->index];
	return 0;
}

static int gc0310_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc0310_win_size *win = &gc0310_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc0310_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc0310_set_brightness(struct v4l2_subdev *sd, int brightness)
{

	printk(KERN_DEBUG "[GC0310]set brightness %d\n", brightness);

	switch (brightness) {
		case BRIGHTNESS_L4:
			gc0310_write_array(sd, gc0310_brightness_l4);
			break;
		case BRIGHTNESS_L3:
			gc0310_write_array(sd, gc0310_brightness_l3);
			break;
		case BRIGHTNESS_L2:
			gc0310_write_array(sd, gc0310_brightness_l2);
			break;
		case BRIGHTNESS_L1:
			gc0310_write_array(sd, gc0310_brightness_l1);
			break;
		case BRIGHTNESS_H0:
			gc0310_write_array(sd, gc0310_brightness_h0);
			break;
		case BRIGHTNESS_H1:
			gc0310_write_array(sd, gc0310_brightness_h1);
			break;
		case BRIGHTNESS_H2:
			gc0310_write_array(sd, gc0310_brightness_h2);
			break;
		case BRIGHTNESS_H3:
			gc0310_write_array(sd, gc0310_brightness_h3);
			break;
		case BRIGHTNESS_H4:
			gc0310_write_array(sd, gc0310_brightness_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc0310_set_contrast(struct v4l2_subdev *sd, int contrast)
{

	switch (contrast) {
		case CONTRAST_L4:
			gc0310_write_array(sd, gc0310_contrast_l4);
			break;
		case CONTRAST_L3:
			gc0310_write_array(sd, gc0310_contrast_l3);
			break;
		case CONTRAST_L2:
			gc0310_write_array(sd, gc0310_contrast_l2);
			break;
		case CONTRAST_L1:
			gc0310_write_array(sd, gc0310_contrast_l1);
			break;
		case CONTRAST_H0:
			gc0310_write_array(sd, gc0310_contrast_h0);
			break;
		case CONTRAST_H1:
			gc0310_write_array(sd, gc0310_contrast_h1);
			break;
		case CONTRAST_H2:
			gc0310_write_array(sd, gc0310_contrast_h2);
			break;
		case CONTRAST_H3:
			gc0310_write_array(sd, gc0310_contrast_h3);
			break;
		case CONTRAST_H4:
			gc0310_write_array(sd, gc0310_contrast_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc0310_set_saturation(struct v4l2_subdev *sd, int saturation)
{

	printk(KERN_DEBUG "[GC0310]set saturation %d\n", saturation);

	switch (saturation) {
		case SATURATION_L2:
			gc0310_write_array(sd, gc0310_saturation_l2);
			break;
		case SATURATION_L1:
			gc0310_write_array(sd, gc0310_saturation_l1);
			break;
		case SATURATION_H0:
			gc0310_write_array(sd, gc0310_saturation_h0);
			break;
		case SATURATION_H1:
			gc0310_write_array(sd, gc0310_saturation_h1);
			break;
		case SATURATION_H2:
			gc0310_write_array(sd, gc0310_saturation_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc0310_set_wb(struct v4l2_subdev *sd, int wb)
{

	printk(KERN_DEBUG "[GC0310]set gc0310_set_wb %d\n", wb);

	switch (wb) {
		case WHITE_BALANCE_AUTO:
			gc0310_write_array(sd, gc0310_whitebalance_auto);
			break;
		case WHITE_BALANCE_INCANDESCENT:
			gc0310_write_array(sd, gc0310_whitebalance_incandescent);
			break;
		case WHITE_BALANCE_FLUORESCENT:
			gc0310_write_array(sd, gc0310_whitebalance_fluorescent);
			break;
		case WHITE_BALANCE_DAYLIGHT:
			gc0310_write_array(sd, gc0310_whitebalance_daylight);
			break;
		case WHITE_BALANCE_CLOUDY_DAYLIGHT:
			gc0310_write_array(sd, gc0310_whitebalance_cloudy);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

#if 0
static int gc0310_get_iso(struct v4l2_subdev *sd, unsigned char *val)
{

	int ret;

	ret = gc0310_read(sd, 0x03, val);
	if (ret < 0)
		return ret;
	return 0;
}
static int gc0310_get_iso_min_gain(struct v4l2_subdev *sd, u32 *val)
{

	*val = ISO_MIN_GAIN ;
	return 0 ;
}

static int gc0310_get_iso_max_gain(struct v4l2_subdev *sd, u32 *val)
{
		

	*val = ISO_MAX_GAIN ;
	return 0 ;
}
#endif
static int gc0310_set_sharpness(struct v4l2_subdev *sd, int sharpness)
{
		

	printk(KERN_DEBUG "[GC0310]set sharpness %d\n", sharpness);
	switch (sharpness) {

		case SHARPNESS_L2:
			gc0310_write_array(sd, gc0310_sharpness_l2);
			break;
		case SHARPNESS_L1:
			gc0310_write_array(sd, gc0310_sharpness_l1);
			break;
		case SHARPNESS_H0:
			gc0310_write_array(sd, gc0310_sharpness_h0);
			break;
		case SHARPNESS_H1:
			gc0310_write_array(sd, gc0310_sharpness_h1);
			break;
		case SHARPNESS_H2:
			gc0310_write_array(sd, gc0310_sharpness_h2);
			break;
		case SHARPNESS_H3:
			gc0310_write_array(sd, gc0310_sharpness_h3);
			break;
		case SHARPNESS_H4:
			gc0310_write_array(sd, gc0310_sharpness_h4);
			break;
		case SHARPNESS_H5:
			gc0310_write_array(sd, gc0310_sharpness_h5);
			break;
		case SHARPNESS_H6:
			gc0310_write_array(sd, gc0310_sharpness_h6);
			break;
		case SHARPNESS_H7:
			gc0310_write_array(sd, gc0310_sharpness_h7);
			break;
		case SHARPNESS_H8:
			gc0310_write_array(sd, gc0310_sharpness_h8);
			break;
		case SHARPNESS_MAX:
			gc0310_write_array(sd, gc0310_sharpness_max);
			break;
		default:
			return -EINVAL;	

	}
	return 0;
}

#if 0
static unsigned int gc0310_calc_shutter_speed(struct v4l2_subdev *sd , unsigned int *val)
{

	unsigned char v1,v2,v3,v4;
	unsigned int speed = 0;
	int ret;
	ret = gc0310_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;

	ret = gc0310_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	ret = gc0310_write(sd, 0xfe, 0x01);
	if (ret < 0)
		return ret;

	ret = gc0310_read(sd, 0x25, &v3);
	if (ret < 0)
		return ret;

	ret = gc0310_read(sd, 0x26, &v4);
	if (ret < 0)
		return ret;

	ret = gc0310_write(sd, 0xfe, 0x00);
	if (ret < 0)
		return ret;

	speed = ((v1<<8)|v2);
	speed = (speed * 10000) / ((v3<<8)|v4);
    
	*val = speed ;
	printk("[gc0310]:speed=0x%0x\n",*val);
	return 0;
}
#endif
static int gc0310_set_iso(struct v4l2_subdev *sd, int iso)
{
	printk(KERN_DEBUG "[GC0310]set_iso %d\n", iso);
		

	switch (iso) {
		case ISO_AUTO:
			gc0310_write_array(sd, gc0310_iso_auto);
			break;
		case ISO_100:
			gc0310_write_array(sd, gc0310_iso_100);
			break;
		case ISO_200:
			gc0310_write_array(sd, gc0310_iso_200);
			break;
		case ISO_400:
			gc0310_write_array(sd, gc0310_iso_400);
			break;
		case ISO_800:
			gc0310_write_array(sd, gc0310_iso_800);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc0310_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk(KERN_DEBUG "[GC0310]set contrast %d\n", effect);

	switch (effect) {
		case EFFECT_NONE:
			gc0310_write_array(sd, gc0310_effect_none);
			break;
		case EFFECT_MONO:
			gc0310_write_array(sd, gc0310_effect_mono);
			break;
		case EFFECT_NEGATIVE:
			gc0310_write_array(sd, gc0310_effect_negative);
			break;
		case EFFECT_SOLARIZE:
			gc0310_write_array(sd, gc0310_effect_solarize);
			break;
		case EFFECT_SEPIA:
			gc0310_write_array(sd, gc0310_effect_sepia);
			break;
		case EFFECT_POSTERIZE:
			gc0310_write_array(sd, gc0310_effect_posterize);
			break;
		case EFFECT_WHITEBOARD:
			gc0310_write_array(sd, gc0310_effect_whiteboard);
			break;
		case EFFECT_BLACKBOARD:
			gc0310_write_array(sd, gc0310_effect_blackboard);
			break;
		case EFFECT_WATER_GREEN:
			gc0310_write_array(sd, gc0310_effect_lightgreen);
			break;
		case EFFECT_WATER_BLUE:
			gc0310_write_array(sd, gc0310_effect_waterblue);
			break;
		case EFFECT_BROWN:
			gc0310_write_array(sd, gc0310_effect_brown);
			break;
    	case EFFECT_AQUA:
			gc0310_write_array(sd, gc0310_effect_aqua);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int gc0310_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk(KERN_DEBUG "[GC0310]set exposure %d\n", exposure);

	switch (exposure) {
		case EXPOSURE_L6:
			gc0310_write_array(sd, gc0310_exposure_l6);
			break;
		case EXPOSURE_L5:
			gc0310_write_array(sd, gc0310_exposure_l5);
			break;
		case EXPOSURE_L4:
			gc0310_write_array(sd, gc0310_exposure_l4);
			break;
		case EXPOSURE_L3:
			gc0310_write_array(sd, gc0310_exposure_l3);
			break;
		case EXPOSURE_L2:
			gc0310_write_array(sd, gc0310_exposure_l2);
			break;
		case EXPOSURE_L1:
			gc0310_write_array(sd, gc0310_exposure_l1);
			break;
		case EXPOSURE_H0:
			gc0310_write_array(sd, gc0310_exposure_h0);
			break;
		case EXPOSURE_H1:
			gc0310_write_array(sd, gc0310_exposure_h1);
			break;
		case EXPOSURE_H2:
			gc0310_write_array(sd, gc0310_exposure_h2);
			break;
		case EXPOSURE_H3:
			gc0310_write_array(sd, gc0310_exposure_h3);
			break;
		case EXPOSURE_H4:
			gc0310_write_array(sd, gc0310_exposure_h4);
			break;
		case EXPOSURE_H5:
			gc0310_write_array(sd, gc0310_exposure_h5);
			break;
		case EXPOSURE_H6:
			gc0310_write_array(sd, gc0310_exposure_h6);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int gc0310_set_previewmode(struct v4l2_subdev *sd, int mode)
{
	printk(KERN_INFO "MIKE:[GC0310]set_previewmode %d\n", mode);

	switch (mode) {
		case preview_auto:
		case normal:
			gc0310_write_array(sd, gc0310_previewmode_mode00);
			break;
		case dynamic:
		case beatch:
		case bar:
		case snow:
		case landscape:
		case nightmode:
			gc0310_write_array(sd, gc0310_previewmode_mode01);
			break;
		case theatre:
		case party:
		case candy:
		case fireworks:
		case sunset:
		case nightmode_portrait:
		case sports:
		case anti_shake:
		case portrait:
		default:
			return -EINVAL;
	}

	return 0;
}


static int gc0310_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0310_info *info = container_of(sd, struct gc0310_info, sd);
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
			isp_setting.setting = (struct v4l2_isp_regval *)&gc0310_isp_setting;
			isp_setting.size = ARRAY_SIZE(gc0310_isp_setting);
			memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
			break;
		case V4L2_CID_ISP_PARM:
			ctrl->value = (int)&gc0310_isp_parm;
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int gc0310_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0310_info *info = container_of(sd, struct gc0310_info, sd);
	int ret = 0;

	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ret = gc0310_set_brightness(sd, ctrl->value);
			if (!ret)
				info->brightness = ctrl->value;
			break;
		case V4L2_CID_CONTRAST:
			ret = gc0310_set_contrast(sd, ctrl->value);
			if (!ret)
				info->contrast = ctrl->value;
			break;
		case V4L2_CID_SATURATION:
			ret = gc0310_set_saturation(sd, ctrl->value);
			if (!ret)
				info->saturation = ctrl->value;
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			ret = gc0310_set_wb(sd, ctrl->value);
			if (!ret)
				info->wb = ctrl->value;
			break;
		case V4L2_CID_EXPOSURE:
			ret = gc0310_set_exposure(sd, ctrl->value);
			if (!ret)
				info->exposure = ctrl->value;
			break;
		case V4L2_CID_EFFECT:
			ret = gc0310_set_effect(sd, ctrl->value);
			if (!ret)
				info->effect = ctrl->value;
			break;
		case V4L2_CID_FRAME_RATE:
			ret = gc0310_set_framerate(sd, ctrl->value);
			break;
		case V4L2_CID_SCENE:
			ret = gc0310_set_previewmode(sd, ctrl->value);
			if (!ret)
				info->scene_mode = ctrl->value;
			break;
		case V4L2_CID_ISO:
			ret = gc0310_set_iso(sd, ctrl->value);
			if (!ret)
				info->iso = ctrl->value;
			break;
		case V4L2_CID_SHARPNESS:
			ret = gc0310_set_sharpness(sd, ctrl->value);
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

static int gc0310_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC0310, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc0310_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc0310_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;

	return ret;
}

static int gc0310_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc0310_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc0310_core_ops = {
	.g_chip_ident = gc0310_g_chip_ident,
	.g_ctrl = gc0310_g_ctrl,
	.s_ctrl = gc0310_s_ctrl,
	.queryctrl = gc0310_queryctrl,
	.reset = gc0310_reset,
	.init = gc0310_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc0310_g_register,
	.s_register = gc0310_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc0310_video_ops = {
	.enum_mbus_fmt = gc0310_enum_mbus_fmt,
	.try_mbus_fmt = gc0310_try_mbus_fmt,
	.s_mbus_fmt = gc0310_s_mbus_fmt,
	.s_stream = gc0310_s_stream,
	.cropcap = gc0310_cropcap,
	.g_crop	= gc0310_g_crop,
	.s_parm = gc0310_s_parm,
	.g_parm = gc0310_g_parm,
	.enum_frameintervals = gc0310_enum_frameintervals,
	.enum_framesizes = gc0310_enum_framesizes,
};

static const struct v4l2_subdev_ops gc0310_ops = {
	.core = &gc0310_core_ops,
	.video = &gc0310_video_ops,
};

static int gc0310_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc0310_info *info;
	struct comip_camera_subdev_platform_data *gc0310_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct gc0310_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc0310_ops);
	printk("gc0310_probe++++++++++++++++++++++");

	/* Make sure it's an gc0310 */
	ret = gc0310_detect(sd);
	printk("gc0310_probe_________________________");

	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc0310 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc0310 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return ret;
}

static int gc0310_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0310_info *info = container_of(sd, struct gc0310_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc0310_id[] = {
	{ "gc0310-mipi-yuv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0310_id);

static struct i2c_driver gc0310_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc0310-mipi-yuv",
	},
	.probe		= gc0310_probe,
	.remove		= gc0310_remove,
	.id_table	= gc0310_id,
};

static __init int init_gc0310(void)
{
	return i2c_add_driver(&gc0310_driver);
}

static __exit void exit_gc0310(void)
{
	i2c_del_driver(&gc0310_driver);
}

fs_initcall(init_gc0310);
module_exit(exit_gc0310);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore gc0310 sensors with mipi interface");
MODULE_LICENSE("GPL");
