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
#include "sp0a20.h"

#define SP0A20_CHIP_ID	(0x2b)
//#define sp0a20_CHIP_ID_L	(0x45)

#define VGA_WIDTH		640   //648
#define VGA_HEIGHT		480

#define MAX_WIDTH		VGA_WIDTH
#define MAX_HEIGHT		VGA_HEIGHT

#define MAX_PREVIEW_WIDTH	VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  	VGA_HEIGHT

#define V4L2_I2C_ADDR_8BIT	(0x0001)
#define sp0a20_REG_END		0xff

#define ISO_MAX_GAIN      0x50
#define ISO_MIN_GAIN      0x20
//heq
#define SP0A20_P1_0x10		0x80		 //ku_outdoor
#define SP0A20_P1_0x11		0x80		//ku_nr
#define SP0A20_P1_0x12		0x80		 //ku_dummy
#define SP0A20_P1_0x13		0x80		 //ku_low  
#define SP0A20_P1_0x14		0x88		//c4 //kl_outdoor 
#define SP0A20_P1_0x15		0x88		//c4 //kl_nr      
#define SP0A20_P1_0x16		0x88		//c4 //kl_dummy    
#define SP0A20_P1_0x17		0x88		//c4 //kl_low   

//sharpness  
#define SP0A20_P2_0xe8         0x20 //0x10//10//;sharp_fac_pos_outdoor
#define SP0A20_P2_0xec         0x20 //0x20//20//;sharp_fac_neg_outdoor
#define SP0A20_P2_0xe9        	0x20 //0x0a//0a//;sharp_fac_pos_nr
#define SP0A20_P2_0xed         0x20 //0x20//20//;sharp_fac_neg_nr
#define SP0A20_P2_0xea         0x20 //0x08//08//;sharp_fac_pos_dummy
#define SP0A20_P2_0xee         0x20 //0x18//18//;sharp_fac_neg_dummy
#define SP0A20_P2_0xeb         0x20 //0x08//08//;sharp_fac_pos_low
#define SP0A20_P2_0xef          0x20 //0x08//18//;sharp_fac_neg_low

//saturation
#define SP0A20_P1_0xd3		0x66
#define SP0A20_P1_0xd4		0x6a
#define SP0A20_P1_0xd5  	0x56
#define SP0A20_P1_0xd6  	0x44
#define SP0A20_P1_0xd7		0x66
#define SP0A20_P1_0xd8		0x6a
#define SP0A20_P1_0xd9   	0x56
#define SP0A20_P1_0xda   	0x44

//ae target
#define SP0A20_P1_0xeb		0x78 //indor
#define SP0A20_P1_0xec		0x78//outdor



struct sp0a20_format_struct;

struct sp0a20_info {
	struct v4l2_subdev sd;
	struct sp0a20_format_struct *fmt;
	struct sp0a20_win_size *win;
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


static struct regval_list sp0a20_init_regs[] = {
	#if 1
	{0xfd,0x00},
{0x0c,0x00},
{0x12,0x02},
{0x13,0x2f},
{0x6d,0x32},
{0x6c,0x32},
{0x6f,0x33},
{0x6e,0x34},
{0x92,0x01},
{0x99,0x05},
{0x16,0x38},
{0x17,0x38},
{0x70,0x3a},
{0x14,0x02},
{0x15,0x20},
{0x71,0x23},
{0x69,0x25},
{0x6a,0x1a},
{0x72,0x1c},
{0x75,0x1e},
{0x73,0x3c},
{0x74,0x21},
{0x79,0x00},
{0x77,0x10},
{0x1a,0x4d},
{0x1b,0x27},
{0x1c,0x07},
{0x1e,0x15},
{0x21,0x0e},
{0x22,0x28},
{0x26,0x66},
{0x28,0x0b},
{0x37,0x5a},
{0xfd,0x02},
{0x01,0x80},
{0x52,0x10},
{0x54,0x00},
{0xfd,0x01},
{0x41,0x00},
{0x42,0x00},
{0x43,0x00},
{0x44,0x00},
{0xfd,0x00},
{0x03,0x01},
{0x04,0xc2},
{0x05,0x00},
{0x06,0x00},
{0x07,0x00},
{0x08,0x00},
{0x09,0x02},
{0x0a,0xf4},
{0xfd,0x01},
{0xf0,0x00},
{0xf7,0x4b},
{0x02,0x0e},
{0x03,0x01},
{0x06,0x4b},
{0x07,0x00},
{0x08,0x01},
{0x09,0x00},
{0xfd,0x02},
{0xbe,0x1a},
{0xbf,0x04},
{0xd0,0x1a},
{0xd1,0x04},
{0xfd,0x01},
{0x5a,0x40},
{0xfd,0x02},
{0xbc,0x70},
{0xbd,0x50},
{0xb8,0x66},
{0xb9,0x88},
{0xba,0x30},
{0xbb,0x45},
{0xfd,0x01},
{0xe0,0x44},
{0xe1,0x36},
{0xe2,0x30},
{0xe3,0x2a},
{0xe4,0x2a},
{0xe5,0x28},
{0xe6,0x28},
{0xe7,0x26},
{0xe8,0x26},
{0xe9,0x26},
{0xea,0x24},
{0xf3,0x24},
{0xf4,0x24},
{0xfd,0x01},
{0x04,0xa0},
{0x05,0x24},
{0x0a,0xa0},
{0x0b,0x24},
{0xfd,0x01},
{0xeb,0x78},
{0xec,0x78},
{0xed,0x05},
{0xee,0x0c},
{0xfd,0x01},
{0xf2,0x4d},
{0xfd,0x02},
{0x5b,0x05},
{0x5c,0xa0},
{0xfd,0x01},
{0x26,0x80},
{0x27,0x4f},
{0x28,0x00},
{0x29,0x20},
{0x2a,0x00},
{0x2b,0x03},
{0x2c,0x00},
{0x2d,0x20},
{0x30,0x00},
{0x31,0x00},
{0xfd,0x01},
{0xa1,0x31},
{0xa2,0x33},
{0xa3,0x2f},
{0xa4,0x2f},
{0xa5,0x26},
{0xa6,0x25},
{0xa7,0x28},
{0xa8,0x28},
{0xa9,0x23},
{0xaa,0x23},
{0xab,0x24},
{0xac,0x24},
{0xad,0x0e},
{0xae,0x0e},
{0xaf,0x0e},
{0xb0,0x0e},
{0xb1,0x00},
{0xb2,0x00},
{0xb3,0x00},
{0xb4,0x00},
{0xb5,0x00},
{0xb6,0x00},
{0xb7,0x00},
{0xb8,0x00},
{0xfd,0x02},
{0x08,0x00},
{0x09,0x06},
{0x1d,0x03},
{0x1f,0x05},
{0xfd,0x01},
{0x32,0x00},
{0xfd,0x02},
{0x26,0xbf},
{0x27,0xa3},
{0x10,0x00},
{0x11,0x00},
{0x1b,0x80},
{0x1a,0x80},
{0x18,0x27},
{0x19,0x26},
{0x2a,0x00},
{0x2b,0x00},
{0x28,0xf8},
{0x29,0x08},
{0x66,0x4a},
{0x67,0x6c},
{0x68,0xd4},
{0x69,0xf4},
{0x6a,0xa5},
{0x7c,0x30},
{0x7d,0x4b},
{0x7e,0xf7},
{0x7f,0x13},
{0x80,0xa6},
{0x70,0x26},
{0x71,0x4a},
{0x72,0x25},
{0x73,0x46},
{0x74,0xaa},
{0x6b,0x0a},
{0x6c,0x31},
{0x6d,0x2e},
{0x6e,0x52},
{0x6f,0xaa},
{0x61,0xfb},
{0x62,0x13},
{0x63,0x48},
{0x64,0x6e},
{0x65,0x6a},
{0x75,0x80},
{0x76,0x09},
{0x77,0x02},
{0x24,0x25},
{0x0e,0x16},
{0x3b,0x09},
{0xfd,0x02},
{0xde,0x0f},
{0xd7,0x08},
{0xd8,0x08},
{0xd9,0x10},
{0xda,0x14},
{0xe8,0x20},
{0xe9,0x20},
{0xea,0x20},
{0xeb,0x20},
{0xec,0x20},
{0xed,0x20},
{0xee,0x20},
{0xef,0x20},
{0xd3,0x20},
{0xd4,0x48},
{0xd5,0x20},
{0xd6,0x08},
{0xfd,0x01},
{0xd1,0x20},
{0xfd,0x02},
{0xdc,0x05},
{0x05,0x20},
{0xfd,0x02},
{0x81,0x00},
{0xfd,0x01},
{0xfc,0x00},
{0x7d,0x05},
{0x7e,0x05},
{0x7f,0x09},
{0x80,0x08},
{0xfd,0x02},
{0xdd,0x0f},
{0xfd,0x01},
{0x6d,0x08},
{0x6e,0x08},
{0x6f,0x10},
{0x70,0x18},
{0x86,0x18},
{0x71,0x0a},
{0x72,0x0a},
{0x73,0x14},
{0x74,0x14},
{0x75,0x08},
{0x76,0x0a},
{0x77,0x06},
{0x78,0x06},
{0x79,0x25},
{0x7a,0x23},
{0x7b,0x22},
{0x7c,0x00},
{0x81,0x0d},
{0x82,0x18},
{0x83,0x20},
{0x84,0x24},
{0xfd,0x02},
{0x83,0x12},
{0x84,0x14},
{0x86,0x04},
{0xfd,0x01},
{0x61,0x60},
{0x62,0x28},
{0x8a,0x10},
{0xfd,0x01},
{0x8b,0x00},
{0x8c,0x09},
{0x8d,0x17},
{0x8e,0x22},
{0x8f,0x2e},
{0x90,0x42},
{0x91,0x53},
{0x92,0x5f},
{0x93,0x6d},
{0x94,0x84},
{0x95,0x95},
{0x96,0xa5},
{0x97,0xb3},
{0x98,0xc0},
{0x99,0xcc},
{0x9a,0xd6},
{0x9b,0xdf},
{0x9c,0xe7},
{0x9d,0xee},
{0x9e,0xf4},
{0x9f,0xfa},
{0xa0,0xff},
{0xfd,0x02},
{0x15,0xc0},
{0x16,0x8c},
{0xa0,0x86},
{0xa1,0xfa},
{0xa2,0x00},
{0xa3,0xdb},
{0xa4,0xc0},
{0xa5,0xe6},
{0xa6,0xed},
{0xa7,0xda},
{0xa8,0xb9},
{0xa9,0x0c},
{0xaa,0x33},
{0xab,0x0f},
{0xac,0x93},
{0xad,0xe7},
{0xae,0x06},
{0xaf,0xda},
{0xb0,0xcc},
{0xb1,0xda},
{0xb2,0xda},
{0xb3,0xda},
{0xb4,0xcc},
{0xb5,0x0c},
{0xb6,0x33},
{0xb7,0x0f},
{0xfd,0x01},
{0xd3,0x66},
{0xd4,0x6a},
{0xd5,0x56},
{0xd6,0x44},
{0xd7,0x66},
{0xd8,0x6a},
{0xd9,0x56},
{0xda,0x44},
{0xfd,0x01},
{0xdd,0x30},
{0xde,0x10},
{0xdf,0xff},
{0x00,0x00},
{0xfd,0x01},
{0xc2,0xaa},
{0xc3,0x88},
{0xc4,0x77},
{0xc5,0x66},
{0xfd,0x01},
{0xcd,0x10},
{0xce,0x1f},
{0xcf,0x30},
{0xd0,0x45},
{0xfd,0x02},
{0x31,0x60},
{0x32,0x60},
{0x33,0xc0},
{0x35,0x60},
{0x37,0x13},
{0xfd,0x01},
{0x0e,0x80},
{0x0f,0x20},
{0x10,0x80},
{0x11,0x80},
{0x12,0x80},
{0x13,0x80},
{0x14,0x88},
{0x15,0x88},
{0x16,0x88},
{0x17,0x88},
{0xfd,0x00},
{0x31,0x00},
{0xfd,0x01},
{0x32,0x15},
{0x33,0xef},
{0x34,0x07},
{0xd2,0x01},
{0xfb,0x25},
{0xf2,0x49},
{0x35,0x40},
{0x5d,0x11},
{0xfd,0x00},
{0x92,0x70},//stream off
#else
{0xfd,0x00},
{0xfd,0x01},
{0xfd,0x00},
{0xfd,0x01},
{0xfd,0x00},
{0x99,0x04},
{0xc0,0x01},
{0x0c,0x00},
{0x92,0x01},
{0xfd,0x00},
#endif
{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_stream_on[] = {
	{0xfd, 0x00},
	{0x92, 0x71},//01 mipi stream on
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_stream_off[] = {
	{0xfd, 0x00},
	{0x92, 0x70},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_win_vga[] = {

	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_win_qcif[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_sharpness_l2[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8-0x10},
	{0xec, SP0A20_P2_0xec-0x10},
	{0xe9, SP0A20_P2_0xe9-0x10},
	{0xed, SP0A20_P2_0xed-0x10},
	{0xea, SP0A20_P2_0xea-0x10},
	{0xee, SP0A20_P2_0xee-0x10},
	{0xeb, SP0A20_P2_0xeb-0x10},
	{0xef, SP0A20_P2_0xef-0x10}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_sharpness_l1[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8-0x08},
	{0xec, SP0A20_P2_0xec-0x08},
	{0xe9, SP0A20_P2_0xe9-0x08},
	{0xed, SP0A20_P2_0xed-0x08},
	{0xea, SP0A20_P2_0xea-0x08},
	{0xee, SP0A20_P2_0xee-0x08},
	{0xeb, SP0A20_P2_0xeb-0x08},
	{0xef, SP0A20_P2_0xef-0x08}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_sharpness_h0[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8},
	{0xec, SP0A20_P2_0xec},
	{0xe9, SP0A20_P2_0xe9},
	{0xed, SP0A20_P2_0xed},
	{0xea, SP0A20_P2_0xea},
	{0xee, SP0A20_P2_0xee},
	{0xeb, SP0A20_P2_0xeb},
	{0xef, SP0A20_P2_0xef}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_sharpness_h1[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x08},
	{0xec, SP0A20_P2_0xec+0x08},
	{0xe9, SP0A20_P2_0xe9+0x08},
	{0xed, SP0A20_P2_0xed+0x08},
	{0xea, SP0A20_P2_0xea+0x08},
	{0xee, SP0A20_P2_0xee+0x08},
	{0xeb, SP0A20_P2_0xeb+0x08},
	{0xef, SP0A20_P2_0xef+0x08}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_sharpness_h2[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x10},
	{0xec, SP0A20_P2_0xec+0x10},
	{0xe9, SP0A20_P2_0xe9+0x10},
	{0xed, SP0A20_P2_0xed+0x10},
	{0xea, SP0A20_P2_0xea+0x10},
	{0xee, SP0A20_P2_0xee+0x10},
	{0xeb, SP0A20_P2_0xeb+0x10},
	{0xef, SP0A20_P2_0xef+0x10}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_sharpness_h3[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x18},
	{0xec, SP0A20_P2_0xec+0x18},
	{0xe9, SP0A20_P2_0xe9+0x18},
	{0xed, SP0A20_P2_0xed+0x18},
	{0xea, SP0A20_P2_0xea+0x18},
	{0xee, SP0A20_P2_0xee+0x18},
	{0xeb, SP0A20_P2_0xeb+0x18},
	{0xef, SP0A20_P2_0xef+0x18}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_sharpness_h4[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x20},
	{0xec, SP0A20_P2_0xec+0x20},
	{0xe9, SP0A20_P2_0xe9+0x20},
	{0xed, SP0A20_P2_0xed+0x20},
	{0xea, SP0A20_P2_0xea+0x20},
	{0xee, SP0A20_P2_0xee+0x20},
	{0xeb, SP0A20_P2_0xeb+0x20},
	{0xef, SP0A20_P2_0xef+0x20}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_sharpness_h5[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x28},
	{0xec, SP0A20_P2_0xec+0x28},
	{0xe9, SP0A20_P2_0xe9+0x28},
	{0xed, SP0A20_P2_0xed+0x28},
	{0xea, SP0A20_P2_0xea+0x28},
	{0xee, SP0A20_P2_0xee+0x28},
	{0xeb, SP0A20_P2_0xeb+0x28},
	{0xef, SP0A20_P2_0xef+0x28}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_sharpness_h6[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x30},
	{0xec, SP0A20_P2_0xec+0x30},
	{0xe9, SP0A20_P2_0xe9+0x30},
	{0xed, SP0A20_P2_0xed+0x30},
	{0xea, SP0A20_P2_0xea+0x30},
	{0xee, SP0A20_P2_0xee+0x30},
	{0xeb, SP0A20_P2_0xeb+0x30},
	{0xef, SP0A20_P2_0xef+0x30}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_sharpness_h7[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x38},
	{0xec, SP0A20_P2_0xec+0x38},
	{0xe9, SP0A20_P2_0xe9+0x38},
	{0xed, SP0A20_P2_0xed+0x38},
	{0xea, SP0A20_P2_0xea+0x38},
	{0xee, SP0A20_P2_0xee+0x38},
	{0xeb, SP0A20_P2_0xeb+0x38},
	{0xef, SP0A20_P2_0xef+0x38}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_sharpness_h8[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x40},
	{0xec, SP0A20_P2_0xec+0x40},
	{0xe9, SP0A20_P2_0xe9+0x40},
	{0xed, SP0A20_P2_0xed+0x40},
	{0xea, SP0A20_P2_0xea+0x40},
	{0xee, SP0A20_P2_0xee+0x40},
	{0xeb, SP0A20_P2_0xeb+0x40},
	{0xef, SP0A20_P2_0xef+0x40}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_sharpness_max[] = {
	{0xfd, 0x02},
	{0xe8, SP0A20_P2_0xe8+0x48},
	{0xec, SP0A20_P2_0xec+0x48},
	{0xe9, SP0A20_P2_0xe9+0x48},
	{0xed, SP0A20_P2_0xed+0x48},
	{0xea, SP0A20_P2_0xea+0x48},
	{0xee, SP0A20_P2_0xee+0x48},
	{0xeb, SP0A20_P2_0xeb+0x48},
	{0xef, SP0A20_P2_0xef+0x48}, 	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_brightness_l4[] = {
	{0xfd, 0x01},
	{0xdb, 0xe0},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_brightness_l3[] = {
	{0xfd, 0x01},
	{0xdb, 0xe8},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_brightness_l2[] = {
	{0xfd, 0x01},
	{0xdb, 0xf0},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_brightness_l1[] = {
	{0xfd, 0x01},
	{0xdb, 0xf8},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_brightness_h0[] = {
	{0xfd, 0x01},
	{0xdb, 0x00},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_brightness_h1[] = {
	{0xfd, 0x01},
	{0xdb, 0x08},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_brightness_h2[] = {
	{0xfd, 0x01},
	{0xdb, 0x10},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_brightness_h3[] = {
	{0xfd, 0x01},
	{0xdb, 0x18},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_brightness_h4[] = {
	{0xfd, 0x01},
	{0xdb, 0x20},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_saturation_l2[] = {
	{0xfd, 0x01},
	{0xd3, SP0A20_P1_0xd3-0x20},
	{0xd4, SP0A20_P1_0xd4-0x20},
	{0xd5, SP0A20_P1_0xd5-0x20},
	{0xd6, SP0A20_P1_0xd6-0x20},
	{0xd7, SP0A20_P1_0xd7-0x20},
	{0xd8, SP0A20_P1_0xd8-0x20},
	{0xd9, SP0A20_P1_0xd9-0x20},
	{0xda, SP0A20_P1_0xda-0x20},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_saturation_l1[] = {
	{0xfd, 0x01},
	{0xd3, SP0A20_P1_0xd3-0x10},
	{0xd4, SP0A20_P1_0xd4-0x10},
	{0xd5, SP0A20_P1_0xd5-0x10},
	{0xd6, SP0A20_P1_0xd6-0x10},
	{0xd7, SP0A20_P1_0xd7-0x10},
	{0xd8, SP0A20_P1_0xd8-0x10},
	{0xd9, SP0A20_P1_0xd9-0x10},
	{0xda, SP0A20_P1_0xda-0x10},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_saturation_h0[] = {
	{0xfd, 0x01},
	{0xd3, SP0A20_P1_0xd3},
	{0xd4, SP0A20_P1_0xd4},
	{0xd5, SP0A20_P1_0xd5},
	{0xd6, SP0A20_P1_0xd6},
	{0xd7, SP0A20_P1_0xd7},
	{0xd8, SP0A20_P1_0xd8},
	{0xd9, SP0A20_P1_0xd9},
	{0xda, SP0A20_P1_0xda},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_saturation_h1[] = {
	{0xfd, 0x01},	
	{0xd3, SP0A20_P1_0xd3+0x10},
	{0xd4, SP0A20_P1_0xd4+0x10},
	{0xd5, SP0A20_P1_0xd5+0x10},
	{0xd6, SP0A20_P1_0xd6+0x10},
	{0xd7, SP0A20_P1_0xd7+0x10},
	{0xd8, SP0A20_P1_0xd8+0x10},
	{0xd9, SP0A20_P1_0xd9+0x10},
	{0xda, SP0A20_P1_0xda+0x10},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_saturation_h2[] = {
	{0xfd, 0x01},
	{0xd3, SP0A20_P1_0xd3+0x20},
	{0xd4, SP0A20_P1_0xd4+0x20},
	{0xd5, SP0A20_P1_0xd5+0x20},
	{0xd6, SP0A20_P1_0xd6+0x20},
	{0xd7, SP0A20_P1_0xd7+0x20},
	{0xd8, SP0A20_P1_0xd8+0x20},
	{0xd9, SP0A20_P1_0xd9+0x20},
	{0xda, SP0A20_P1_0xda+0x20},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_l4[] = {\
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10-0x20},
	{0x11, SP0A20_P1_0x11-0x20},
	{0x12, SP0A20_P1_0x12-0x20},
	{0x13, SP0A20_P1_0x13-0x20},
	{0x14, SP0A20_P1_0x14-0x20},
	{0x15, SP0A20_P1_0x15-0x20},
	{0x16, SP0A20_P1_0x16-0x20},
	{0x17, SP0A20_P1_0x17-0x20},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_l3[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10-0x18},
	{0x11, SP0A20_P1_0x11-0x18},
	{0x12, SP0A20_P1_0x12-0x18},
	{0x13, SP0A20_P1_0x13-0x18},
	{0x14, SP0A20_P1_0x14-0x18},
	{0x15, SP0A20_P1_0x15-0x18},
	{0x16, SP0A20_P1_0x16-0x18},
	{0x17, SP0A20_P1_0x17-0x18},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_l2[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10-0x10},
	{0x11, SP0A20_P1_0x11-0x10},
	{0x12, SP0A20_P1_0x12-0x10},
	{0x13, SP0A20_P1_0x13-0x10},
	{0x14, SP0A20_P1_0x14-0x10},
	{0x15, SP0A20_P1_0x15-0x10},
	{0x16, SP0A20_P1_0x16-0x10},
	{0x17, SP0A20_P1_0x17-0x10},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_l1[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10-0x08},
	{0x11, SP0A20_P1_0x11-0x08},
	{0x12, SP0A20_P1_0x12-0x08},
	{0x13, SP0A20_P1_0x13-0x08},
	{0x14, SP0A20_P1_0x14-0x08},
	{0x15, SP0A20_P1_0x15-0x08},
	{0x16, SP0A20_P1_0x16-0x08},
	{0x17, SP0A20_P1_0x17-0x08},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_h0[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10},
	{0x11, SP0A20_P1_0x11},
	{0x12, SP0A20_P1_0x12},
	{0x13, SP0A20_P1_0x13},
	{0x14, SP0A20_P1_0x14},
	{0x15, SP0A20_P1_0x15},
	{0x16, SP0A20_P1_0x16},
	{0x17, SP0A20_P1_0x17},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_h1[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10+0x08},
	{0x11, SP0A20_P1_0x11+0x08},
	{0x12, SP0A20_P1_0x12+0x08},
	{0x13, SP0A20_P1_0x13+0x08},
	{0x14, SP0A20_P1_0x14+0x08},
	{0x15, SP0A20_P1_0x15+0x08},
	{0x16, SP0A20_P1_0x16+0x08},
	{0x17, SP0A20_P1_0x17+0x08},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_h2[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10+0x10},
	{0x11, SP0A20_P1_0x11+0x10},
	{0x12, SP0A20_P1_0x12+0x10},
	{0x13, SP0A20_P1_0x13+0x10},
	{0x14, SP0A20_P1_0x14+0x10},
	{0x15, SP0A20_P1_0x15+0x10},
	{0x16, SP0A20_P1_0x16+0x10},
	{0x17, SP0A20_P1_0x17+0x10},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_h3[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10+0x18},
	{0x11, SP0A20_P1_0x11+0x18},
	{0x12, SP0A20_P1_0x12+0x18},
	{0x13, SP0A20_P1_0x13+0x18},
	{0x14, SP0A20_P1_0x14+0x18},
	{0x15, SP0A20_P1_0x15+0x18},
	{0x16, SP0A20_P1_0x16+0x18},
	{0x17, SP0A20_P1_0x17+0x18},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_contrast_h4[] = {
	{0xfd,0x01},
	{0x10, SP0A20_P1_0x10+0x20},
	{0x11, SP0A20_P1_0x11+0x20},
	{0x12, SP0A20_P1_0x12+0x20},
	{0x13, SP0A20_P1_0x13+0x20},
	{0x14, SP0A20_P1_0x14+0x20},
	{0x15, SP0A20_P1_0x15+0x20},
	{0x16, SP0A20_P1_0x16+0x20},
	{0x17, SP0A20_P1_0x17+0x20},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_whitebalance_auto[] = {
	{0xfd , 0x02},
	{0x26 , 0xbf},
	{0x27 , 0xa3},
	{0xfd , 0x01},
	{0x32 , 0x15},
	{0xfd , 0x02},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_whitebalance_incandescent[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0x88},
	{0x27 , 0xd0},
	{0xfd , 0x00},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_whitebalance_fluorescent[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0x95},
	{0x27 , 0xba},
	{0xfd , 0x00},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_whitebalance_daylight[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0xd1},
	{0x27 , 0x85},
	{0xfd , 0x00},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_whitebalance_cloudy[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0xdb},
	{0x27 , 0x70},
	{0xfd , 0x00},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_effect_none[] = {
	{0xfd , 0x01},
	{0x66 , 0x00},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_mono[] = {
	{0xfd , 0x01},
	{0x66 , 0x20},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_negative[] = {
	{0xfd , 0x01},
	{0x66 , 0x04},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_solarize[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_sepia[] = {
	{0xfd , 0x01},
	{0x66 , 0x00},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_posterize[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_whiteboard[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_lightgreen[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list sp0a20_effect_brown[] = {
	
	{sp0a20_REG_END, 0xff}, /* END MARKER */
};

static struct regval_list sp0a20_effect_waterblue[] = {
	{0xfd , 0x01},
	{0x66 , 0x10},
	{0x67 , 0x80},
	{0x68 , 0xb0},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},	
	{sp0a20_REG_END, 0xff}, /* END MARKER */
};

static struct regval_list sp0a20_effect_blackboard[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_effect_aqua[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_l6[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb-0x18},
	{0xec , SP0A20_P1_0xec-0x18},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_l5[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb-0x14},
	{0xec , SP0A20_P1_0xec-0x14},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_l4[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb-0x10},
	{0xec , SP0A20_P1_0xec-0x10},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_l3[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb-0x0c},
	{0xec , SP0A20_P1_0xec-0x0c},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_l2[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb-0x08},
	{0xec , SP0A20_P1_0xec-0x08},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_exposure_l1[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb-0x04},
	{0xec , SP0A20_P1_0xec-0x04},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_exposure_h0[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb},
	{0xec , SP0A20_P1_0xec},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_exposure_h1[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb+0x04},
	{0xec , SP0A20_P1_0xec+0x04},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_exposure_h2[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb+0x08},
	{0xec , SP0A20_P1_0xec+0x08},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_h3[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb+0x0c},
	{0xec , SP0A20_P1_0xec+0x0c},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_h4[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb+0x10},
	{0xec , SP0A20_P1_0xec+0x10},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_h5[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb+0x14},
	{0xec , SP0A20_P1_0xec+0x14},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_exposure_h6[] = {
	{0xfd , 0x01},
	{0xeb , SP0A20_P1_0xeb+0x18},
	{0xec , SP0A20_P1_0xec+0x18},
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

//next preview mode
static struct regval_list sp0a20_previewmode_mode_auto[] = {  
{0xfd,0x00},
{0x03,0x01},
{0x04,0xc2},
{0x05,0x00},
{0x06,0x00},
{0x07,0x00},
{0x08,0x00},
{0x09,0x02},
{0x0a,0xf4},
{0xfd,0x01},
{0xf0,0x00},
{0xf7,0x4b},
{0x02,0x0e},
{0x03,0x01},
{0x06,0x4b},
{0x07,0x00},
{0x08,0x01},
{0x09,0x00},
{0xfd,0x02},
{0xbe,0x1a},
{0xbf,0x04},
{0xd0,0x1a},
{0xd1,0x04},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_previewmode_mode_nightmode[] = {
{0xfd,0x00},
{0x03,0x01},
{0x04,0xc2},
{0x05,0x00},
{0x06,0x00},
{0x07,0x00},
{0x08,0x00},
{0x09,0x02},
{0x0a,0xf4},
{0xfd,0x01},
{0xf0,0x00},
{0xf7,0x4b},
{0x02,0x0e},
{0x03,0x01},
{0x06,0x4b},
{0x07,0x00},
{0x08,0x01},
{0x09,0x00},
{0xfd,0x02},
{0xbe,0x1a},
{0xbf,0x04},
{0xd0,0x1a},
{0xd1,0x04},	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_previewmode_mode_sports[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp0a20_previewmode_mode_portrait[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_previewmode_mode_landscape[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list sp0a20_iso_auto[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_iso_100[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_iso_200[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_iso_400[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp0a20_iso_800[] = {
	
	{sp0a20_REG_END, 0xff},	/* END MARKER */
};

//preview mode end
static int sp0a20_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int sp0a20_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sp0a20_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != sp0a20_REG_END) {
		int ret = sp0a20_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int sp0a20_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}


static int sp0a20_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;
 mdelay(200);
	ret = sp0a20_read(sd, 0x02, &v);
	
	if (ret < 0)
		return ret;
	if (v != SP0A20_CHIP_ID)
		return -ENODEV;
	/*	
	ret = sp0a20_read(sd, 0xf1, &v);
	
	if (ret < 0)
		return ret;
	if (v != SP0A20_CHIP_ID_L)
		return -ENODEV;
*/

	return 0;
}
static int sp0a20_init(struct v4l2_subdev *sd, u32 val)
{
	struct sp0a20_info *info = container_of(sd, struct sp0a20_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return sp0a20_write_array(sd, sp0a20_init_regs);
}

static struct sp0a20_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} sp0a20_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= NULL,
	},
};
#define N_sp0a20_FMTS ARRAY_SIZE(sp0a20_formats)

static struct sp0a20_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
} sp0a20_win_sizes[] = {

	/* VGA */
	{
		.width          = VGA_WIDTH,
		.height         = VGA_HEIGHT,
		.regs           = sp0a20_win_vga,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(sp0a20_win_sizes))

static int sp0a20_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_sp0a20_FMTS)
		return -EINVAL;

	*code = sp0a20_formats[index].mbus_code;
	return 0;
}

static int sp0a20_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct sp0a20_format_struct **ret_fmt,
		struct sp0a20_win_size **ret_wsize)
{
	int index;
	struct sp0a20_win_size *wsize;

	for (index = 0; index < N_sp0a20_FMTS; index++)
		if (sp0a20_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_sp0a20_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = sp0a20_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = sp0a20_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = sp0a20_win_sizes; wsize < sp0a20_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= sp0a20_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = sp0a20_formats[index].colorspace;
	return 0;
}

static int sp0a20_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return sp0a20_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int sp0a20_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct sp0a20_info *info = container_of(sd, struct sp0a20_info, sd);
	struct sp0a20_format_struct *fmt_s;
	struct sp0a20_win_size *wsize;
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	int ret;

	ret = sp0a20_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = sp0a20_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		data->mipi_clk = 156;//按照真实mipi bps来配置，单位为Mbps
		ret = sp0a20_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int sp0a20_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct sp0a20_info *info = container_of(sd, struct sp0a20_info, sd);

	printk("sp0a20_set_framerate %d\n", framerate);

	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;

	return 0;
}


static int sp0a20_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = sp0a20_write_array(sd, sp0a20_stream_on);
	else
		ret = sp0a20_write_array(sd, sp0a20_stream_off);

	return ret;
}

static int sp0a20_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int sp0a20_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int sp0a20_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp0a20_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp0a20_frame_rates[] = { 30, 15, 10, 5, 1 };

static int sp0a20_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(sp0a20_frame_rates))
		return -EINVAL;

	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = sp0a20_frame_rates[interval->index];
	return 0;
}

static int sp0a20_enum_framesizes(struct v4l2_subdev *sd,
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
		struct sp0a20_win_size *win = &sp0a20_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int sp0a20_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int sp0a20_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[sp0a20]set brightness %d\n", brightness);

	switch (brightness) {
		case BRIGHTNESS_L4:
			sp0a20_write_array(sd, sp0a20_brightness_l4);
			break;
		case BRIGHTNESS_L3:
			sp0a20_write_array(sd, sp0a20_brightness_l3);
			break;
		case BRIGHTNESS_L2:
			sp0a20_write_array(sd, sp0a20_brightness_l2);
			break;
		case BRIGHTNESS_L1:
			sp0a20_write_array(sd, sp0a20_brightness_l1);
			break;
		case BRIGHTNESS_H0:
			sp0a20_write_array(sd, sp0a20_brightness_h0);
			break;
		case BRIGHTNESS_H1:
			sp0a20_write_array(sd, sp0a20_brightness_h1);
			break;
		case BRIGHTNESS_H2:
			sp0a20_write_array(sd, sp0a20_brightness_h2);
			break;
		case BRIGHTNESS_H3:
			sp0a20_write_array(sd, sp0a20_brightness_h3);
			break;
		case BRIGHTNESS_H4:
			sp0a20_write_array(sd, sp0a20_brightness_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp0a20_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	switch (contrast) {
		case CONTRAST_L4:
			sp0a20_write_array(sd, sp0a20_contrast_l4);
			break;
		case CONTRAST_L3:
			sp0a20_write_array(sd, sp0a20_contrast_l3);
			break;
		case CONTRAST_L2:
			sp0a20_write_array(sd, sp0a20_contrast_l2);
			break;
		case CONTRAST_L1:
			sp0a20_write_array(sd, sp0a20_contrast_l1);
			break;
		case CONTRAST_H0:
			sp0a20_write_array(sd, sp0a20_contrast_h0);
			break;
		case CONTRAST_H1:
			sp0a20_write_array(sd, sp0a20_contrast_h1);
			break;
		case CONTRAST_H2:
			sp0a20_write_array(sd, sp0a20_contrast_h2);
			break;
		case CONTRAST_H3:
			sp0a20_write_array(sd, sp0a20_contrast_h3);
			break;
		case CONTRAST_H4:
			sp0a20_write_array(sd, sp0a20_contrast_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp0a20_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	printk(KERN_DEBUG "[sp0a20]set saturation %d\n", saturation);

	switch (saturation) {
		case SATURATION_L2:
			sp0a20_write_array(sd, sp0a20_saturation_l2);
			break;
		case SATURATION_L1:
			sp0a20_write_array(sd, sp0a20_saturation_l1);
			break;
		case SATURATION_H0:
			sp0a20_write_array(sd, sp0a20_saturation_h0);
			break;
		case SATURATION_H1:
			sp0a20_write_array(sd, sp0a20_saturation_h1);
			break;
		case SATURATION_H2:
			sp0a20_write_array(sd, sp0a20_saturation_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp0a20_set_wb(struct v4l2_subdev *sd, int wb)
{
	printk(KERN_DEBUG "[sp0a20]set sp0a20_set_wb %d\n", wb);

	switch (wb) {
		case WHITE_BALANCE_AUTO:
			sp0a20_write_array(sd, sp0a20_whitebalance_auto);
			break;
		case WHITE_BALANCE_INCANDESCENT:
			sp0a20_write_array(sd, sp0a20_whitebalance_incandescent);
			break;
		case WHITE_BALANCE_FLUORESCENT:
			sp0a20_write_array(sd, sp0a20_whitebalance_fluorescent);
			break;
		case WHITE_BALANCE_DAYLIGHT:
			sp0a20_write_array(sd, sp0a20_whitebalance_daylight);
			break;
		case WHITE_BALANCE_CLOUDY_DAYLIGHT:
			sp0a20_write_array(sd, sp0a20_whitebalance_cloudy);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp0a20_get_iso(struct v4l2_subdev *sd, unsigned char val)
{
	int ret;

	ret = sp0a20_read(sd, 0xb1, &val);
	if (ret < 0)
		return ret;
	return 0;
}
static int sp0a20_get_iso_min_gain(struct v4l2_subdev *sd, u32 *val)
{
	*val = ISO_MIN_GAIN ;
	return 0 ;
}

static int sp0a20_get_iso_max_gain(struct v4l2_subdev *sd, u32 *val)
{
	*val = ISO_MAX_GAIN ;
	return 0 ;
}

static int sp0a20_set_sharpness(struct v4l2_subdev *sd, int sharpness)
{

	printk(KERN_DEBUG "[sp0a20]set sharpness %d\n", sharpness);
	switch (sharpness) {
		case SHARPNESS_L2:
			sp0a20_write_array(sd, sp0a20_sharpness_l2);
			break;
		case SHARPNESS_L1:
			sp0a20_write_array(sd, sp0a20_sharpness_l1);
			break;
		case SHARPNESS_H0:
			sp0a20_write_array(sd, sp0a20_sharpness_h0);
			break;
		case SHARPNESS_H1:
			sp0a20_write_array(sd, sp0a20_sharpness_h1);
			break;
		case SHARPNESS_H2:
			sp0a20_write_array(sd, sp0a20_sharpness_h2);
			break;
		case SHARPNESS_H3:
			sp0a20_write_array(sd, sp0a20_sharpness_h3);
			break;
		case SHARPNESS_H4:
			sp0a20_write_array(sd, sp0a20_sharpness_h4);
			break;
		case SHARPNESS_H5:
			sp0a20_write_array(sd, sp0a20_sharpness_h5);
			break;
		case SHARPNESS_H6:
			sp0a20_write_array(sd, sp0a20_sharpness_h6);
			break;
		case SHARPNESS_H7:
			sp0a20_write_array(sd, sp0a20_sharpness_h7);
			break;
		case SHARPNESS_H8:
			sp0a20_write_array(sd, sp0a20_sharpness_h8);
			break;
		case SHARPNESS_MAX:
			sp0a20_write_array(sd, sp0a20_sharpness_max);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static unsigned int sp0a20_calc_shutter_speed(struct v4l2_subdev *sd , unsigned int *val)
{
	unsigned char v1,v2,v3,v4;
	unsigned int speed = 0;
	int ret;
	ret = sp0a20_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;

	ret = sp0a20_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	ret = sp0a20_write(sd, 0xfe, 0x01);
	if (ret < 0)
		return ret;

	ret = sp0a20_read(sd, 0x25, &v3);
	if (ret < 0)
		return ret;

	ret = sp0a20_read(sd, 0x26, &v4);
	if (ret < 0)
		return ret;

	ret = sp0a20_write(sd, 0xfe, 0x00);
	if (ret < 0)
		return ret;

	speed = ((v1<<8)|v2);
	speed = (speed * 10000) / ((v3<<8)|v4);
	*val = speed ;
	printk("[sp0a20]:speed=0x%0x\n",*val);
	return 0;
}

static int sp0a20_set_iso(struct v4l2_subdev *sd, int iso)
{
	printk(KERN_DEBUG "[sp0a20]set_iso %d\n", iso);
	switch (iso) {
		case ISO_AUTO:
			sp0a20_write_array(sd, sp0a20_iso_auto);
			break;
		case ISO_100:
			sp0a20_write_array(sd, sp0a20_iso_100);
			break;
		case ISO_200:
			sp0a20_write_array(sd, sp0a20_iso_200);
			break;
		case ISO_400:
			sp0a20_write_array(sd, sp0a20_iso_400);
			break;
		case ISO_800:
			sp0a20_write_array(sd, sp0a20_iso_800);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp0a20_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk(KERN_DEBUG "[sp0a20]set contrast %d\n", effect);

	switch (effect) {
		case EFFECT_NONE:
			sp0a20_write_array(sd, sp0a20_effect_none);
			break;
		case EFFECT_MONO:
			sp0a20_write_array(sd, sp0a20_effect_mono);
			break;
		case EFFECT_NEGATIVE:
			sp0a20_write_array(sd, sp0a20_effect_negative);
			break;
		case EFFECT_SOLARIZE:
			sp0a20_write_array(sd, sp0a20_effect_solarize);
			break;
		case EFFECT_SEPIA:
			sp0a20_write_array(sd, sp0a20_effect_sepia);
			break;
		case EFFECT_POSTERIZE:
			sp0a20_write_array(sd, sp0a20_effect_posterize);
			break;
		case EFFECT_WHITEBOARD:
			sp0a20_write_array(sd, sp0a20_effect_whiteboard);
			break;
		case EFFECT_BLACKBOARD:
			sp0a20_write_array(sd, sp0a20_effect_blackboard);
			break;
		case EFFECT_WATER_GREEN:
			sp0a20_write_array(sd, sp0a20_effect_lightgreen);
			break;
		case EFFECT_WATER_BLUE:
			sp0a20_write_array(sd, sp0a20_effect_waterblue);
			break;
		case EFFECT_BROWN:
			sp0a20_write_array(sd, sp0a20_effect_brown);
			break;
		case EFFECT_AQUA:
			sp0a20_write_array(sd, sp0a20_effect_aqua);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp0a20_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk(KERN_DEBUG "[sp0a20]set exposure %d\n", exposure);

	switch (exposure) {
		case EXPOSURE_L6:
			sp0a20_write_array(sd, sp0a20_exposure_l6);
			break;
		case EXPOSURE_L5:
			sp0a20_write_array(sd, sp0a20_exposure_l5);
			break;
		case EXPOSURE_L4:
			sp0a20_write_array(sd, sp0a20_exposure_l4);
			break;
		case EXPOSURE_L3:
			sp0a20_write_array(sd, sp0a20_exposure_l3);
			break;
		case EXPOSURE_L2:
			sp0a20_write_array(sd, sp0a20_exposure_l2);
			break;
		case EXPOSURE_L1:
			sp0a20_write_array(sd, sp0a20_exposure_l1);
			break;
		case EXPOSURE_H0:
			sp0a20_write_array(sd, sp0a20_exposure_h0);
			break;
		case EXPOSURE_H1:
			sp0a20_write_array(sd, sp0a20_exposure_h1);
			break;
		case EXPOSURE_H2:
			sp0a20_write_array(sd, sp0a20_exposure_h2);
			break;
		case EXPOSURE_H3:
			sp0a20_write_array(sd, sp0a20_exposure_h3);
			break;
		case EXPOSURE_H4:
			sp0a20_write_array(sd, sp0a20_exposure_h4);
			break;
		case EXPOSURE_H5:
			sp0a20_write_array(sd, sp0a20_exposure_h5);
			break;
		case EXPOSURE_H6:
			sp0a20_write_array(sd, sp0a20_exposure_h6);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int sp0a20_set_previewmode(struct v4l2_subdev *sd, int mode)
{
	printk(KERN_INFO "MIKE:[sp0a20]set_previewmode %d\n", mode);

	switch (mode) {
		case preview_auto:
			sp0a20_write_array(sd, sp0a20_previewmode_mode_auto);
			break;
		case normal:
			sp0a20_write_array(sd, sp0a20_previewmode_mode_auto);
			break;
		case dynamic:
		case beatch:
		case bar:
		case snow:
		case landscape:
			sp0a20_write_array(sd, sp0a20_previewmode_mode_landscape);
			break;
		case nightmode:
			sp0a20_write_array(sd, sp0a20_previewmode_mode_nightmode);
			break;
		case theatre:
		case party:
		case candy:
		case fireworks:
		case sunset:
		case nightmode_portrait:
		case sports:
			sp0a20_write_array(sd, sp0a20_previewmode_mode_sports);
			break;
		case anti_shake:
		case portrait:
			sp0a20_write_array(sd, sp0a20_previewmode_mode_portrait);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int sp0a20_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sp0a20_info *info = container_of(sd, struct sp0a20_info, sd);
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
			isp_setting.setting = (struct v4l2_isp_regval *)&sp0a20_isp_setting;
			isp_setting.size = ARRAY_SIZE(sp0a20_isp_setting);
			memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
			break;
		case V4L2_CID_ISP_PARM:
			ctrl->value = (int)&sp0a20_isp_parm;
			break;
		case V4L2_CID_ISO:
			ret = sp0a20_get_iso(sd, ctrl->value);
			if (ret)
				printk("Get iso value failed.\n");
			break;
		case V4L2_CID_SNAPSHOT_GAIN:
			ret = sp0a20_get_iso(sd, ctrl->value);
			if (ret)
				printk("Get iso value failed.\n");
			break;
		case V4L2_CID_MIN_GAIN:
			ret = sp0a20_get_iso_min_gain(sd, &ctrl->value);
			if (ret)
				printk("Get iso min gain value failed.\n");
			break;
		case V4L2_CID_MAX_GAIN:
			ret = sp0a20_get_iso_max_gain(sd, &ctrl->value);
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
			ret = sp0a20_calc_shutter_speed(sd, &ctrl->value);
			if (ret)
				printk("Get shutter speed value failed.\n");
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int sp0a20_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sp0a20_info *info = container_of(sd, struct sp0a20_info, sd);
	int ret = 0;

	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ret = sp0a20_set_brightness(sd, ctrl->value);
			if (!ret)
				info->brightness = ctrl->value;
			break;
		case V4L2_CID_CONTRAST:
			ret = sp0a20_set_contrast(sd, ctrl->value);
			if (!ret)
				info->contrast = ctrl->value;
			break;
		case V4L2_CID_SATURATION:
			ret = sp0a20_set_saturation(sd, ctrl->value);
			if (!ret)
				info->saturation = ctrl->value;
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			ret = sp0a20_set_wb(sd, ctrl->value);
			if (!ret)
				info->wb = ctrl->value;
			break;
		case V4L2_CID_EXPOSURE:
			ret = sp0a20_set_exposure(sd, ctrl->value);
			if (!ret)
				info->exposure = ctrl->value;
			break;
		case V4L2_CID_EFFECT:
			ret = sp0a20_set_effect(sd, ctrl->value);
			if (!ret)
				info->effect = ctrl->value;
			break;
		case V4L2_CID_FRAME_RATE:
			ret = sp0a20_set_framerate(sd, ctrl->value);
			break;
		case V4L2_CID_SCENE:
			ret = sp0a20_set_previewmode(sd, ctrl->value);
			if (!ret)
				info->scene_mode = ctrl->value;
			break;
		case V4L2_CID_ISO:
			ret = sp0a20_set_iso(sd, ctrl->value);
			if (!ret)
				info->iso = ctrl->value;
			break;
		case V4L2_CID_SHARPNESS:
			ret = sp0a20_set_sharpness(sd, ctrl->value);
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

static int sp0a20_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SP0A20, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sp0a20_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sp0a20_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;

	return ret;
}

static int sp0a20_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sp0a20_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}
#endif

static const struct v4l2_subdev_core_ops sp0a20_core_ops = {
	.g_chip_ident = sp0a20_g_chip_ident,
	.g_ctrl = sp0a20_g_ctrl,
	.s_ctrl = sp0a20_s_ctrl,
	.queryctrl = sp0a20_queryctrl,
	.reset = sp0a20_reset,
	.init = sp0a20_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = sp0a20_g_register,
	.s_register = sp0a20_s_register,
#endif
};

static const struct v4l2_subdev_video_ops sp0a20_video_ops = {
	.enum_mbus_fmt = sp0a20_enum_mbus_fmt,
	.try_mbus_fmt = sp0a20_try_mbus_fmt,
	.s_mbus_fmt = sp0a20_s_mbus_fmt,
	.s_stream = sp0a20_s_stream,
	.cropcap = sp0a20_cropcap,
	.g_crop	= sp0a20_g_crop,
	.s_parm = sp0a20_s_parm,
	.g_parm = sp0a20_g_parm,
	.enum_frameintervals = sp0a20_enum_frameintervals,
	.enum_framesizes = sp0a20_enum_framesizes,
};

static const struct v4l2_subdev_ops sp0a20_ops = {
	.core = &sp0a20_core_ops,
	.video = &sp0a20_video_ops,
};

static int sp0a20_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sp0a20_info *info;
	int ret;

	info = kzalloc(sizeof(struct sp0a20_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &sp0a20_ops);

	/* Make sure it's an sp0a20 */
	ret = sp0a20_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an sp0a20 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "sp0a20 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return ret;
}

static int sp0a20_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp0a20_info *info = container_of(sd, struct sp0a20_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id sp0a20_id[] = {
	{ "sp0a20-mipi-yuv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sp0a20_id);

static struct i2c_driver sp0a20_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sp0a20-mipi-yuv",
	},
	.probe		= sp0a20_probe,
	.remove		= sp0a20_remove,
	.id_table	= sp0a20_id,
};

static __init int init_sp0a20(void)
{
	return i2c_add_driver(&sp0a20_driver);
}

static __exit void exit_sp0a20(void)
{
	i2c_del_driver(&sp0a20_driver);
}

fs_initcall(init_sp0a20);
module_exit(exit_sp0a20);

MODULE_DESCRIPTION("A low-level driver for Superpix sp0a20 sensors");
MODULE_LICENSE("GPL");
