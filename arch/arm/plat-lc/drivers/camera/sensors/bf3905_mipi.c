
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
#include "bf3905_mipi.h"

//ADD  BY  RYX 2014.04.12    for  LC1860

#define BF3905_CHIP_ID_H	(0x39)
#define BF3905_CHIP_ID_L	(0x05)

#define VGA_WIDTH		640   //648
#define VGA_HEIGHT		480
#define QCIF_WIDTH		176
#define QCIF_HEIGHT		256

#define MAX_WIDTH		VGA_WIDTH
#define MAX_HEIGHT		VGA_HEIGHT
#define MAX_PREVIEW_WIDTH	VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  	VGA_HEIGHT
//#define MAX_PREVIEW_WIDTH	QCIF_WIDTH
//#define MAX_PREVIEW_HEIGHT  	QCIF_HEIGHT

#define BF3905_REG_END		0xff
#define BF3905_REG_DELAY	0xfffe
#define BF3905_REG_VAL_HVFLIP	0xfe

#define BF3905_MAX_GAIN		0x2d
#define BF3905_MIN_GAIN		0x14
#define BF3905_MCLK		19500000

struct bf3905_format_struct;
struct bf3905_info {
	struct v4l2_subdev sd;
	struct bf3905_format_struct *fmt;
	struct bf3905_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int brightness;
	int contrast;
	int saturation;
	int effect;
	int wb;
	int exposure;
	int scene_mode;
	int frame_rate;
	int sharpness;
	int bf3905_first_open;
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
	preview_auto=0,
	normal=16,
	dynamic=1,
	portrait=2,
	landscape=3,
	nightmode=4,
	nightmode_portrait=5,
	theatre=6,
	beatch=7,
	snow=8,
	sunset=9,
	anti_shake=10,
	fireworks=11,
	sports=12,
	party=13,
	candy=14,
	bar=15,
};


static struct regval_list bf3905_init_regs[] = {
	
	{0x09, 0x83},  //bit[7]:1-soft sleep mode   					  //RYX  ADD
	//{0xf5, 0x80},  //Bit[7]:  1:MIPI enable    Bit[7]:  0: MIPI disalble    //RYX  ADD
	
      	//{BF3905_REG_DELAY, 20},
	//{0x12, 0x80},  //Bit[7]:1-soft reset
	{0x15, 0x12},  //Bit[1]: VSYNC option; Bit[0]: HREF option  
	{0x3e, 0x83},
	//{0x09, 0x03},  //Bit[7]: Soft sleep mode; Bit[6:0]: Drive capability 
	{0x12, 0x00},
	{0x3a, 0x00},  //YUV422 Sequence    //20   //00//01//02//03
	{0x1e, 0x30}, 
	{0x1e, BF3905_REG_VAL_HVFLIP},

	//clock£¬dummy£¬ banding                                                     
	{0x1b, 0x0e},  //0x02: 4bei pin ;  0x06: 2bei pin   ;0x09: 4/3bei pin  ;    0x0e£º1 fen  pin  ;      0x2e£º2 fen  pin  ; 0x4e: 4 fen  pin ; 0x6e: 8 fen  pin ;
	{0x2a, 0x00},  
	{0x2b, 0x1c},  //Dummy Pixel Insert LSB// 784+28=812   //0x10   //RYX  ADD
	{0x92, 0x0c},//0x0c   //0x09    //Dummy Pixel Insert LSB// 510+12=522  //19.5 /2/812/522=23 (MAX=23/S)  //RYX  ADD
	{0x93, 0x00},//0x00

	 //banding
	{0x8a, 0x78},  //50 hz banding   //24/2/(784+16)/100=0x96 //19.5/2/(784+28)/100=0x78  //RYX  ADD
	{0x8b, 0x64},  //60 hz banding    //24/2/(784+16)/120=0x7d //19.5/2/(784+28)/120=0x64  //RYX  ADD

	//initial AWB and AE                                                     
	{0x13, 0x00},  //Manual AWB & AE    
	{0x01, 0x15},
	{0x02, 0x23},
	{0x9d, 0x20},
	{0x8c, 0x02},
	{0x8d, 0xee},
	{0x13, 0x07},
	                                  
	//Simulation Parameters
	{0x5d, 0xb3},
	{0xbf, 0x08},
	{0xc3, 0x08},
	{0xca, 0x10},
	{0x62, 0x00},
	{0x63, 0x00},
	{0xb9, 0x00},
	{0x64, 0x00},  

	{0x0e,0x10},
	{0x22,0x12},

	{0xbb, 0x10},
	{0x08, 0x02},
	{0x20, 0x09},
	{0x21, 0x4f},
	             
	{0xd9, 0x25},
	{0xdf, 0x26},
	{0x2f, 0x84},
	{0x16, 0xa1},  
	{0x6c, 0xc2},  

	//black sun
	{0x71, 0x0f},
	{0x7e, 0x84},
	{0x7f, 0x3c},
	{0x60, 0xe5},
	{0x61, 0xf2},
	{0x6d, 0xc0},

	//WINDOW 640*480   //	{0x4a , 0x0c},     {0x12 , 0x00},    //ryx  add
	{0x0a, 0x21},  
	{0x10, 0x21},  
	{0x3d, 0x59},  
	{0x6b, 0x02},  
	{0x17,0x00},
	{0x18,0xa0},
	{0x19,0x00},
	{0x1a,0x78},
	{0x03,0xa0},
//{0x03,0x00},
	{0x4a,0x0c},  

	{0xda,0x00},
	{0xdb,0xa2},
	{0xdc,0x00},
	{0xdd,0x7a},
	{0xde,0x00},
	{0x33,0x10},
	{0x34,0x08},
	{0x36,0xc5},
	{0x6e,0x20},
	{0xbc,0x0d},

	                                                                                 
	//lens shading
	{0x34, 0x1d},
	{0x36, 0x45},
	{0x6e, 0x20},
	{0xbc, 0x0d},
	{0x35, 0x30}, //lens shading gain of R
	{0x65, 0x2a}, //lens shading gain of G
	{0x66, 0x2a}, //lens shading gain of B
	{0xbd, 0xf4},
	{0xbe, 0x44},
	{0x9b, 0xf4},
	{0x9c, 0x44},
	{0x37, 0xf4},
	{0x38, 0x44},
	                     
	 //denoise and edge enhancement                                                         
	{0x70, 0x0b},
	{0x71, 0x0f},
	{0x72, 0x4c},
	{0x73, 0x46},//27  yzx 4.15 //0x73[7:4]: The bigger, the smaller noise; 0x73[3:0]: The smaller, the smaller noise 
	{0x79, 0x24},
	{0x7a, 0x23}, //0x7a[6:4]: Bright edge enhancement; 0x7a[2:0]: Dark edge enhancement
	  //color fring correction

	{0x7b, 0x58},
	{0x7d, 0x00},
	//low light and outdoor denoise
	{0x75, 0xaa},    //0x8a
	{0x76, 0x28},    //0x98
	{0x77, 0x2a}, 

	{0x39, 0x90},    //9c    //ryx
	{0x3f, 0x90},    //9c    //ryx
	{0x90, 0x28},      // 20   //  ryx
	{0x91, 0xd0},    //d0    //ryx

	//gamma//    highlight
	{0x40, 0x36},                 
	{0x41, 0x33},                 
	{0x42, 0x2a},                 
	{0x43, 0x22},                 
	{0x44, 0x1b},                 
	{0x45, 0x16},                 
	{0x46, 0x13},
	{0x47, 0x10},
	{0x48, 0x0e},
	{0x49, 0x0c},
	{0x4b, 0x0b},
	{0x4c, 0x0a},
	{0x4e, 0x09},
	{0x4f, 0x08},
	{0x50, 0x08},     

	/*
	// gamma //default
	{0x40, 0x3b},
	{0x41, 0x36},
	{0x42, 0x2b},
	{0x43, 0x1d},
	{0x44, 0x1a},
	{0x45, 0x14},
	{0x46, 0x11},
	{0x47, 0x0e},
	{0x48, 0x0d},
	{0x49, 0x0c},    ///0x0d
	{0x4b, 0x0b},    //0x0c
	{0x4c, 0x09},
	{0x4e, 0x08},   //0x0a
	{0x4f, 0x07},    //0x09
	{0x50, 0x07},    //0x09

	//gamma //  clear                                                         
	{0x40, 0x20},
	{0x41, 0x28},
	{0x42, 0x26},
	{0x43, 0x25},
	{0x44, 0x1f},
	{0x45, 0x1a},
	{0x46, 0x16},
	{0x47, 0x12},
	{0x48, 0x0f},
	{0x49, 0x0D},
	{0x4b, 0x0b},
	{0x4c, 0x0a},
	{0x4e, 0x08},
	{0x4f, 0x06},
	{0x50, 0x06},

	 //gamma //  low  noise                
	{0x40, 0x24},
	{0x41, 0x30},
	{0x42, 0x24},
	{0x43, 0x1d},
	{0x44, 0x1a},
	{0x45, 0x14},
	{0x46, 0x11},
	{0x47, 0x0e},
	{0x48, 0x0d},
	{0x49, 0x0c},
	{0x4b, 0x0b},
	{0x4c, 0x09},
	{0x4e, 0x09},
	{0x4f, 0x08},
	{0x50, 0x07},
	
	//gamma //  clear                                                         
	{0x40, 0x20},
	{0x41, 0x28},
	{0x42, 0x26},
	{0x43, 0x25},
	{0x44, 0x1f},
	{0x45, 0x1a},
	{0x46, 0x16},
	{0x47, 0x12},
	{0x48, 0x0f},
	{0x49, 0x0D},
	{0x4b, 0x0b},
	{0x4c, 0x0a},
	{0x4e, 0x08},
	{0x4f, 0x06},
	{0x50, 0x06},      
	 */
	                                                 
	 // color //default(outdoor)
	{0x5a, 0x56},
	{0x51, 0x13},
	{0x52, 0x05},
	{0x53, 0x91},
	{0x54, 0x72},
	{0x57, 0x96},
	{0x58, 0x35},

	 // color// default(normal)
	{0x5a, 0xd6},
	#if 0
	{0x51, 0x28},
	{0x52, 0x35},
	{0x53, 0x9e},
	{0x54, 0x7d},
	{0x57, 0x50},
	{0x58, 0x15},
	{0x5c, 0x26},
       #endif
	{0x51, 0x29},
	{0x52, 0x0d},
	{0x53, 0x91},
	{0x54, 0x81},
	{0x57, 0x56},
	{0x58, 0x09},
	{0x5b, 0x02},
	{0x5c, 0x30},
	                 
	 /*	
	 //color //   beautiful in colour

	{0x5a, 0xd6},
	{0x51, 0x2d},
	{0x52, 0x52},
	{0x53, 0xb7},
	{0x54, 0xa9},
	{0x57, 0xa4},
	{0x58, 0x04},
	
	 //color //good skin
	{0x5a, 0xd6},
	{0x51, 0x17},
	{0x52, 0x13},
	{0x53, 0x5e},
	{0x54, 0x38},
	{0x57, 0x38},
	{0x58, 0x02},
	*/ 
	                 
	// saturation and contrast                                              
	{0x56, 0x40},
	{0x55, 0x85},
	{0xb0, 0xe0},
	{0xb3, 0x58}, ///0x48
	{0xb4, 0xa3}, //bit[7]: A light saturation
	{0xb1, 0xff},
	{0xb2, 0xa0},
	{0xb4, 0x63},  //bit[7]: non-A light saturation
	{0xb1, 0xc0},  //90
	{0xb2, 0xa8},   //80
	                      
	 //AE       
	{0x13, 0x07},                                 
	{0x24, 0x40}, // AE Target
	{0x25, 0x88}, // Bit[7:4]: AE_LOC_INT; Bit[3:0]:AE_LOC_GLB  
	{0x97, 0x3c}, // AE target value2.
	{0x98, 0x8b}, // AE    Window and weight   //8a
	{0x80, 0x92}, //Bit[3:2]: the bigger, Y_AVER_MODIFY is smaller     
	{0x81, 0x00}, //AE speed 
	{0x82, 0x2a}, //minimum global gain  
	{0x83, 0x54},                
	{0x84, 0x39},              
	{0x85, 0x5d},                
	{0x86, 0x77}, //maximum gain
	{0x89, 0x53}, //0x53//INT_MAX_MID
	{0x94, 0x42}, //Bit[7:4]: Threshold for over exposure pixels, the smaller, the more over exposure pixels; Bit[3:0]: Control the start of AE.  ///82  //ryx
	{0x96, 0xb3},
	{0x9a, 0x50},
	{0x99, 0x10},
	{0x9f, 0x64},

	 //AWB             
	{0x6a, 0x81},                 
	{0x23, 0x55},  // green gain 
	{0xa1, 0x31},	      
	{0xa2, 0x0d},  //the low limit of blue gain for indoor scene
	{0xa3, 0x28},  //the upper limit of blue gain for indoor scene
	{0xa4, 0x0a},  //the low limit of red gain for indoor scene
	{0xa5, 0x27},  //the upper limit of red gain for indoor scene   //2c  yzx 4,15
	{0xa6, 0x04},
	{0xa7, 0x1a},  //Base B gain//Don't modify               
	{0xa8, 0x18},  //Base R gain//Don't modify             
	{0xa9, 0x13},
	{0xaa, 0x18},
	{0xab, 0x24},
	{0xac, 0x3c},
	{0xad, 0xf0},
	{0xae, 0x59},
	{0xd0, 0xa3},
	{0xd1, 0x00},
	{0xd3, 0x09},
	{0xd4, 0x24},

	{0xd2, 0x58},
	{0xc5, 0x55},     //0xaa
	{0xc6, 0x88},
	{0xc7, 0x30},     //0x10

	{0xc8, 0x0d},
	{0xc9, 0x10},
	{0xd3, 0x09},
	{0xd4, 0x24},
	 //Skin
	{0xee, 0x30},

	{BF3905_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3905_fmt_yuv422[] = {
 	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_stream_on[] = {
	{0x09, 0x03},     //bit[7]:1-soft sleep mode
	{BF3905_REG_END, 0xff},	/* END MARKER */ 
};

static struct regval_list bf3905_stream_off[] = {
	/* Sensor enter LP11*/
	{0x09, 0x83},    //bit[7]:1-soft sleep mode
	{BF3905_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3905_frame_30fps[] = {
	{0x89, 0x1d},
	{BF3905_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3905_frame_15fps[] = {
	{0x93, 0x01},
	{0x92, 0x3f},
	{0x89, 0x35},
	{BF3905_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3905_frame_12fps[] = {
	{0x93, 0x02},
	{0x92, 0x0e},
	{0x89, 0x45},
	{BF3905_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3905_frame_10fps[] = {
	{0x93, 0x02},
	{0x92, 0xdd},
	{0x89, 0x55},
	{BF3905_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3905_frame_7fps[] = {
	{0x93, 0x04},
	{0x92, 0xf2},
	{0x89, 0x75},
	{BF3905_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list bf3905_frame_autofps[] = {
	{0x89, 0x65},
	{BF3905_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list bf3905_win_vga[] = {
	//WINDOW 640*480  
	{0x4a , 0x0c},  
	{0x12 , 0x00},  
	{0xda , 0x00},  
	{0xdb , 0xa2},
	{0xdc , 0x00},
	{0xdd , 0x7a},
	{0xde , 0x00},

	{0x0a , 0x21},  
	{0x10 , 0x21},  
	{0x3d , 0x59},  
	{0x6b , 0x02},  
	{0x03 , 0xa0},  //00  //RYX  ADD
//{0x03 , 0x00},  //00  //RYX  ADD
	{0x17 , 0x00},
	{0x18 , 0xa0},
	{0x19 , 0x00},
	{0x1a , 0x78},
	
	//clock£¬dummy£¬ banding                                                     
	{0x1b, 0x0e},  //0x02: 4bei pin ;  0x06: 2bei pin   ;0x09: 4/3bei pin  ;    0x0e£º1 fen  pin  ;      0x2e£º2 fen  pin  ; 0x4e: 4 fen  pin ; 0x6e: 8 fen  pin ;
	{0x2a, 0x00},  
	{0x2b, 0x1c},  //Dummy Pixel Insert LSB// 784+28=812   //0x10   //RYX  ADD
	{0x92, 0x0c}, //0x0c  //0x09    //Dummy Pixel Insert LSB// 510+12=522  //19.5 /2/812/522=23 (MAX=23/S)  //RYX  ADD
	{0x93, 0x00},//0x00

	 //banding
	{0x8a, 0x78},  //50 hz banding   //24/2/(784+16)/100=0x96 //19.5/2/(784+28)/100=0x78  //RYX  ADD
	{0x8b, 0x64},  //60 hz banding    //24/2/(784+16)/120=0x7d //19.5/2/(784+28)/120=0x64  //RYX  ADD
	{0x89, 0x53},     //0x53 //0X23, min 25     //0X2b, min 20     //0X33, min 15   //0X33, min 15   //0X3b, min 14    //0X3b, min 13    //0X43, min 12     //0X4b, min 11         //
	{BF3905_REG_END, 0xff},	/* END MARKER */
};

#ifdef CONFIG_VIDEO_COMIP_LOW_POWER
static struct regval_list bf3905_win_qcif[] = {
	//WINDOW 176*144      //RYX  ADD

     //line= 492*316
	{0x4a , 0x0e},   //0x4a[1:0] = 10 //Ç°ºó¶Ëwindow£¬ºó¶Ë1/2 subsample  /
//{0x12 , 0x10},  //bit 4:  1
	{0x12 , 0x00},  //bit 4:  1

	{0xda , 0x25},  
	{0xdb , 0x7e},
	{0xdc , 0x19},
	{0xdd , 0x62},
	{0xde , 0x80},

	{0x0a , 0x21},  
	{0x10 , 0x21},  
	{0x3d , 0x59},  
	{0x6b , 0x02},  
	
	{0x03 , 0xa0},  //00  //RYX  ADD
	{0x17 , 0x00},
//{0x18 , 0x58},
	{0x18 , 0x2c},
	{0x19 , 0x00},
//{0x1a , 0x48},
	{0x1a , 0x40},
	{0x1b, 0x0e},  //0x02: 4bei pin ;  0x06: 2bei pin   ;0x09: 4/3bei pin  ;    0x0e£º1 fen  pin  ;      0x2e£º2 fen  pin  ; 0x4e: 4 fen  pin ; 0x6e: 8 fen  pin ;
	{0x2a, 0x00},  
	{0x2b, 0x08},  //Dummy Pixel Insert LSB// 492+8=500   //0x10   //RYX  ADD
//	{0x92, 0xf1},   //	{0x92, 0xf1},   //0x09    //Dummy Pixel Insert LSB// 316+241=557  //19.5 /2/500/557=35 (MAX=35/S)  //RYX  ADD
//	{0x93, 0x00},   //00

	//banding
	{0x8a, 0x78},  //50 hz banding   //24/2/(784+16)/100=0x96 //19.5/2/(492+8)/100=0xc3  //RYX  ADD
	{0x8b, 0x64},  //60 hz banding    //24/2/(784+16)/120=0x7d //19.5/2/(492+8)/120=0xa2  //RYX  ADD
	{0x89, 0x1b},   //0X1b, min 33  //0X2b, min 20    //0X33, min 15   //0X3b, min 14    //0X3b, min 13    //0X43, min 12     //0X4b, min 11         //
	   //0X53, min 10 //0X5b, min 9   //0X63, min 8    //0X73, min 7    //0X83, min 6       //0Xa3, min 5 
	//clock£¬dummy£¬ banding                                                     
	
	{BF3905_REG_END, 0xff},	/* END MARKER */
};

#endif

static struct regval_list bf3905_brightness_l4[] = {
	{0x24 , 0x15},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_brightness_l3[] = {
	{0x24 , 0x20},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_brightness_l2[] = {
	{0x24 , 0x25},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_brightness_l1[] = {
	{0x24 , 0x30},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_brightness_h0[] = {
	{0x24 , 0x40}, //default
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_brightness_h1[] = {
	{0x24 , 0x4f},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_brightness_h2[] = {
	{0x24 , 0x5f},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_brightness_h3[] = {
	{0x24 , 0x6f},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_brightness_h4[] = {
	{0x24 , 0x7f},
	{BF3905_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list bf3905_saturation_l2[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0x80},
	{0xb2,0x70},  /* SATURATION LEVEL-2*/
		
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_saturation_l1[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0x90},
	{0xb2,0x80},  /* SATURATION LEVEL-1*/
		
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_saturation_h0[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xc0},
	{0xb2,0xa8}, /* SATURATION LEVEL0*/
		
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_saturation_h1[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xd8},
	{0xb2,0xd0},  /* SATURATION LEVEL+1*/

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_saturation_h2[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xe8},
	{0xb2,0xe8},   /* SATURATION LEVEL+2*/

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_l4[] = {
	{0x56,0x20},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_l3[] = {
	{0x56,0x26},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_l2[] = {
	{0x56,0x30},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_l1[] = {
	{0x56,0x36},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_h0[] = {
	{0x56,0x40},//default level
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_h1[] = {
	{0x56,0x48},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_h2[] = {
	{0x56,0x53},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_h3[] = {
 	{0x56,0x58},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_contrast_h4[] = {
	{0x56,0x62},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_whitebalance_auto[] = {
	{0x01,0x15},
	{0x02,0x23},
	{0x13,0x07},//auto
	
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_whitebalance_incandescent[] = {
	{0x13,0x05},
	{0x01,0x1f},
	{0x02,0x15}, /*INCANDISCENT*/

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_whitebalance_fluorescent[] = {
	{0x13,0x05},
	{0x01,0x1a},
	{0x02,0x1e}, /*FLOURESECT*/

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_whitebalance_daylight[] = {
	{0x13,0x05},
	{0x01,0x11},
	{0x02,0x26}, /*DAYLIGHT*/

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_whitebalance_cloudy[] = {
	{0x13,0x05},
	{0x01,0x10},
	{0x02,0x28},/*CLOUDY*/

	{BF3905_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3905_effect_none[] = {
	{0x70,0x0b},
	{0x69,0x00},
	{0x67,0x80},
	{0x68,0x80},
	{0xb4,0x63},//none  

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_mono[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x20},
	{0x67,0x80},
	{0x68,0x80},
	{0xb4,0x03},//mono

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_negative[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x01},
	{0x67,0x80},
	{0x68,0x80},
	{0xb4,0x03}, //negative

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_solarize[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x40},
	{0x67,0x80},
	{0x68,0x80},
	{0xb4,0x03},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_sepia[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x20},
	{0x67,0x60},
	{0x68,0xa0},
	{0xb4,0x03}, //sepia

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_posterize[] = {
	
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_whiteboard[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x7b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x00},
	{0x67,0x80},
	{0x68,0x80},
	{0xb4,0x03},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_blackboard[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x6b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x00},
	{0x67,0x80},
	{0x68,0x80},
	{0xb4,0x03},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_aqua[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x20},
	{0x67,0x90},
	{0x68,0x70},
	{0xb4,0x03},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_green[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x20},
	{0x67,0x60},
	{0x68,0x70},
	{0xb4,0x03},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_blue[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x20},
	{0x67,0xe0},
	{0x68,0x60},
	{0xb4,0x03},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_effect_brown[] = {
	{0xb4,0xe3},
	{0xb1,0xef},
	{0xb2,0xef},
	{0xb4,0x63},
	{0xb1,0xba},
	{0xb2,0xaa},
	{0x56,0x40},
	{0x70,0x0b},
	{0x7a,0x12},
	{0x73,0x27},
	{0x69,0x20},
	{0x67,0x55},
	{0x68,0xc0},
	{0xb4,0x03},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_exposure_l6[] = {
	{0x55,0xe0},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_l5[] = {
	{0x55,0xd0},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_l4[] = {
	{0x55,0xc0},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_exposure_l3[] = {
	{0x55,0xb0},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_l2[] = {
	{0x55,0xa0}, 

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_l1[] = {
	{0x55,0x90}, 

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_h0[] = {
	{0x55,0x85}, //98  //ryx

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_h1[] = {
	{0x55,0x10}, 

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_h2[] = {
	{0x55,0x20}, 

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_h3[] = {
	{0x55,0x30},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_h4[] = {
	{0x55,0x40},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_h5[] = {
	{0x55,0x50},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list bf3905_exposure_h6[] = {
	{0x55,0x60},

	{BF3905_REG_END, 0x00},	/* END MARKER */
};

/////////////////////////next preview mode////////////////////////////////////

static struct regval_list bf3905_previewmode_mode00[] = {

	/*case auto:
	case normal:
	case dynamic:
	case beatch:
	case bar:
	case snow:
	*/

	{0x86, 0x77},
	{0x89, 0x53},       //0X33, min 15   //0X3b, min 14    //0X3b, min 13    //0X43, min 12     //0X4b, min 11         //
	                           //0X53, min 10 //0X5b, min 9   //0X63, min 8    //0X73, min 7    //0X83, min 6       //0Xa3, min 5 
	{BF3905_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3905_previewmode_mode01[] = {

	/*
	case nightmode:
	case theatre:
	case party:
	case candy:
	case fireworks:
	case sunset:
	*/

	{0x86, 0x80},
	{0x89, 0x7d},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3905_previewmode_mode02[] = {// portrait

	/*
	case sports:
	case anti_shake:
	*/

	{0x86, 0x77},
	{0x89, 0x35},
	{BF3905_REG_END, 0x00},	/* END MARKER */
};

///////////////////////////////preview mode end////////////////////////

static struct regval_list bf3905_iso_auto[] = {
	{0x13,0x07},
	{0x9f,0x61},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_iso_100[] = {
	{0x13,0x47},
	{0x9f,0x61},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_iso_200[] = {
	{0x13,0x47},
	{0x9f,0x62},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_iso_400[] = {
	{0x13,0x47},
	{0x9f,0x63},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_iso_800[] = {
	{0x13,0x47},
	{0x9f,0x64},
	{BF3905_REG_END, 0x00},
};

static struct regval_list bf3905_sharpness_l1[] = {
	{0x7a, 0x11},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_l2[] = {
	{0x7a, 0x01},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h0[] = {
	{0x7a, 0x23},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h1[] = {
	{0x7a, 0x33},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h2[] = {
	{0x7a, 0x43},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h3[] = {
	{0x7a, 0x53},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h4[] = {
	{0x7a, 0x63},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h5[] = {
	{0x7a, 0x74},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h6[] = {
	{0x7a, 0x75},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h7[] = {
	{0x7a, 0x76},
	{BF3905_REG_END, 0x00},
};
static struct regval_list bf3905_sharpness_h8[] = {
	{0x7a, 0x77},
	{BF3905_REG_END, 0x00},
};

static int bf3905_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int bf3905_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int bf3905_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	unsigned char val = 0x30;
	int ret;

	while (vals->reg_num != BF3905_REG_END) {
		if (vals->reg_num == BF3905_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
                                msleep(vals->value);
                        else
                                mdelay(vals->value);

		}
		else {
			if (vals->reg_num == 0x1e && vals->value == BF3905_REG_VAL_HVFLIP) {
				if (bf3905_isp_parm.mirror)
					val &= 0x1f;

				if (bf3905_isp_parm.flip)
					val &= 0x2f;

				vals->value = val;
			}
			ret = bf3905_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}


static int bf3905_flash_ctrl_para(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1;
//	unsigned short v,g1,g2;
//	u32 v_gain;
/*
	//2035
	
	//bf3905_write(sd, 0xb6,0x00);
	ret = bf3905_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;
	ret = bf3905_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2;

	ret = bf3905_read(sd, 0x03, &g1);
	if (ret < 0)
	ret = bf3905_read(sd, 0x03, &g2);
	if (ret < 0)
	v_gain=v*g1*g2;


	if(v<1300)
		return 1;
	else if(v_gain>=(50437*49265)) //v_gain>=(37893*49157)
		return 0;

	else
		return 1;
*/
		ret = bf3905_read(sd, 0x8c, &v1);
	if (ret<4)
		return 0;
	else
		return 1;
	
}

static int bf3905_reset(struct v4l2_subdev *sd, u32 val)
{
	//bf3905_write(sd, 0x12,0x80);  

	return 0;
}

static int bf3905_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	printk("bf3905_detect\n");
	ret = bf3905_read(sd, 0xfc, &v);
	printk("CHIP_ID_H=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != BF3905_CHIP_ID_H)   //39
		return -ENODEV;
	
	ret = bf3905_read(sd, 0xfd, &v);
	printk("CHIP_ID_L=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != BF3905_CHIP_ID_L)    //05
		return -ENODEV;

	return 0;
}

static int bf3905_init(struct v4l2_subdev *sd, u32 val)
{
	int ret = -1;
	//unsigned char v;
	struct bf3905_info *info = container_of(sd, struct bf3905_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;
	info->bf3905_first_open= 1;

	ret = bf3905_write_array(sd, bf3905_init_regs);
	
	return ret;
}


static struct bf3905_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} bf3905_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,  
	//	 .mbus_code	= V4L2_MBUS_FMT_UYVY8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= bf3905_fmt_yuv422,
	},
};

#define N_BF3905_FMTS ARRAY_SIZE(bf3905_formats)

static struct bf3905_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
} bf3905_win_sizes[] = {
#ifdef CONFIG_VIDEO_COMIP_LOW_POWER
	/*QCIF*/
	{
		.width          = QCIF_WIDTH,
		.height         = QCIF_HEIGHT,
		.regs           = bf3905_win_qcif,
	},
#endif
	/* VGA */
	{
		.width          = VGA_WIDTH,
		.height         = VGA_HEIGHT,
		.regs           = bf3905_win_vga,
	},
	
};
#define N_WIN_SIZES (ARRAY_SIZE(bf3905_win_sizes))

static int bf3905_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_BF3905_FMTS)
		return -EINVAL;

	*code = bf3905_formats[index].mbus_code;
	return 0;
}

static int bf3905_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct bf3905_format_struct **ret_fmt,
		struct bf3905_win_size **ret_wsize)
{
	int index;
	struct bf3905_win_size *wsize;

	for (index = 0; index < N_BF3905_FMTS; index++)
		if (bf3905_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_BF3905_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = bf3905_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = bf3905_formats + index;

	fmt->field = V4L2_FIELD_NONE;
	for (wsize = bf3905_win_sizes; wsize < bf3905_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= bf3905_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = bf3905_formats[index].colorspace;
	return 0;
}

static int bf3905_set_framerate_internal(struct v4l2_subdev *sd, int framerate)
{
	int ret;
	printk("bf3905_set_framerate %d\n", framerate);
	switch (framerate){
		case FRAME_RATE_30:
			ret = bf3905_write_array(sd,bf3905_frame_30fps);
			break;
		case FRAME_RATE_15:
			ret = bf3905_write_array(sd,bf3905_frame_15fps);
			break;
		case 12:
			ret = bf3905_write_array(sd,bf3905_frame_12fps);
			break;
		case 10:
			ret = bf3905_write_array(sd,bf3905_frame_10fps);
			break;
		case FRAME_RATE_7:
			ret = bf3905_write_array(sd,bf3905_frame_7fps);
			break;
		default:
			ret = bf3905_write_array(sd,bf3905_frame_autofps);
			break;
	}
	if (ret)
		return ret;
	return 0;
}


static int bf3905_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return bf3905_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int bf3905_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct bf3905_info *info = container_of(sd, struct bf3905_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct bf3905_format_struct *fmt_s;
	struct bf3905_win_size *wsize;
	int ret;

	ret = bf3905_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;


	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = bf3905_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}
	
	if ((info->win != wsize) && wsize->regs) {
		ret = bf3905_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		memset(data, 0, sizeof(*data));
		data->mipi_clk = 156;//288; /* Mbps. */
		data->slave_addr = client->addr;
	}
	bf3905_set_framerate_internal (sd,info->frame_rate);
	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int bf3905_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct bf3905_info *info = container_of(sd, struct bf3905_info, sd);
	printk("bf3905_set_framerate %d\n", framerate);

	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;

	info->frame_rate = framerate;
	return 0;
}


static int bf3905_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = bf3905_write_array(sd, bf3905_stream_on);
	else
		ret = bf3905_write_array(sd, bf3905_stream_off);

        //  msleep(150);//ppp
	return ret;
}

static int bf3905_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int bf3905_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
//snapshot max size
	a->bounds.width 		= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
//snapshot max size end
	a->defrect.left 		= 0;
	a->defrect.top		= 0;
//preview max size
	a->defrect.width		= MAX_PREVIEW_WIDTH;
	a->defrect.height		= MAX_PREVIEW_HEIGHT;
//preview max size end
	a->type 			= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int bf3905_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3905_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3905_frame_rates[] = { 30, 15, 10, 5, 1 };

static int bf3905_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(bf3905_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = bf3905_frame_rates[interval->index];
	return 0;
}

static int bf3905_enum_framesizes(struct v4l2_subdev *sd,
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
		struct bf3905_win_size *win = &bf3905_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int bf3905_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int bf3905_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk("%s, set brightness %d\n",__func__, brightness);

	switch (brightness) {
	case BRIGHTNESS_L4:
		bf3905_write_array(sd, bf3905_brightness_l4);
		break;
	case BRIGHTNESS_L3:
		bf3905_write_array(sd, bf3905_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		bf3905_write_array(sd, bf3905_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		bf3905_write_array(sd, bf3905_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		bf3905_write_array(sd, bf3905_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		bf3905_write_array(sd, bf3905_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		bf3905_write_array(sd, bf3905_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		bf3905_write_array(sd, bf3905_brightness_h3);
		break;
	case BRIGHTNESS_H4:
		bf3905_write_array(sd, bf3905_brightness_h4);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3905_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk("%s, set contrast %d\n",__func__, contrast);

	switch (contrast) {
	case CONTRAST_L4:
		bf3905_write_array(sd, bf3905_contrast_l4);
		break;
	case CONTRAST_L3:
		bf3905_write_array(sd, bf3905_contrast_l3);
		break;
	case CONTRAST_L2:
		bf3905_write_array(sd, bf3905_contrast_l2);
		break;
	case CONTRAST_L1:
		bf3905_write_array(sd, bf3905_contrast_l1);
		break;
	case CONTRAST_H0:
		bf3905_write_array(sd, bf3905_contrast_h0);
		break;
	case CONTRAST_H1:
		bf3905_write_array(sd, bf3905_contrast_h1);
		break;
	case CONTRAST_H2:
		bf3905_write_array(sd, bf3905_contrast_h2);
		break;
	case CONTRAST_H3:
		bf3905_write_array(sd, bf3905_contrast_h3);
		break;
	case CONTRAST_H4:
		bf3905_write_array(sd, bf3905_contrast_h4);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3905_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	printk("%s, set saturation %d\n",__func__, saturation);

	switch (saturation) {
	case SATURATION_L2:
		bf3905_write_array(sd, bf3905_saturation_l2);
		break;
	case SATURATION_L1:
		bf3905_write_array(sd, bf3905_saturation_l1);
		break;
	case SATURATION_H0:
		bf3905_write_array(sd, bf3905_saturation_h0);
		break;
	case SATURATION_H1:
		bf3905_write_array(sd, bf3905_saturation_h1);
		break;
	case SATURATION_H2:
		bf3905_write_array(sd, bf3905_saturation_h2);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3905_set_wb(struct v4l2_subdev *sd, int wb)
{
	printk("%s, set wb %d\n",__func__, wb);

	switch (wb) {
	case WHITE_BALANCE_AUTO:
		bf3905_write_array(sd, bf3905_whitebalance_auto);
		break;
	case WHITE_BALANCE_INCANDESCENT:
		bf3905_write_array(sd, bf3905_whitebalance_incandescent);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		bf3905_write_array(sd, bf3905_whitebalance_fluorescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		bf3905_write_array(sd, bf3905_whitebalance_daylight);
		break;
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		bf3905_write_array(sd, bf3905_whitebalance_cloudy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int bf3905_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk("%s, set effect %d\n",__func__, effect);

	switch (effect) {
	case EFFECT_NONE:
		bf3905_write_array(sd, bf3905_effect_none);
		break;
	case EFFECT_MONO:
		bf3905_write_array(sd, bf3905_effect_mono);
		break;
	case EFFECT_NEGATIVE:
		bf3905_write_array(sd, bf3905_effect_negative);
		break;
	case EFFECT_SOLARIZE:
		bf3905_write_array(sd, bf3905_effect_solarize);
		break;
	case EFFECT_SEPIA: // old picture
		bf3905_write_array(sd, bf3905_effect_sepia);
		break;
	case EFFECT_POSTERIZE:
		bf3905_write_array(sd, bf3905_effect_posterize);
		break;
	case EFFECT_WHITEBOARD:
		bf3905_write_array(sd, bf3905_effect_whiteboard);
		break;
	case EFFECT_BLACKBOARD:
		bf3905_write_array(sd, bf3905_effect_blackboard);
		break;
	case EFFECT_AQUA: //cool
		bf3905_write_array(sd, bf3905_effect_aqua);
		break;
	case EFFECT_WATER_GREEN:
		bf3905_write_array(sd, bf3905_effect_green);
		break;
	case EFFECT_WATER_BLUE:
		bf3905_write_array(sd, bf3905_effect_blue);
		break;
	case EFFECT_BROWN:
		bf3905_write_array(sd, bf3905_effect_brown);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3905_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk("%s, set exposure %d\n",__func__, exposure);

	switch (exposure) {
	case EXPOSURE_L6:
		bf3905_write_array(sd, bf3905_exposure_l6);
		break;
	case EXPOSURE_L5:
		bf3905_write_array(sd, bf3905_exposure_l5);
		break;
	case EXPOSURE_L4:
		bf3905_write_array(sd, bf3905_exposure_l4);
		break;
	case EXPOSURE_L3:
		bf3905_write_array(sd, bf3905_exposure_l3);
		break;
	case EXPOSURE_L2:
		bf3905_write_array(sd, bf3905_exposure_l2);
		break;
	case EXPOSURE_L1:
		bf3905_write_array(sd, bf3905_exposure_l1);
		break;
	case EXPOSURE_H0:
		bf3905_write_array(sd, bf3905_exposure_h0);
		break;
	case EXPOSURE_H1:
		bf3905_write_array(sd, bf3905_exposure_h1);
		break;
	case EXPOSURE_H2:
		bf3905_write_array(sd, bf3905_exposure_h2);
		break;
	case EXPOSURE_H3:
		bf3905_write_array(sd, bf3905_exposure_h3);
		break;
	case EXPOSURE_H4:
		bf3905_write_array(sd, bf3905_exposure_h4);
		break;
	case EXPOSURE_H5:
		bf3905_write_array(sd, bf3905_exposure_h5);
		break;
	case EXPOSURE_H6:
		bf3905_write_array(sd, bf3905_exposure_h6);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3905_set_scene(struct v4l2_subdev *sd, int mode)
{
	printk("%s, set scene = %d\n", __func__, mode);

	switch (mode) {
	case normal:
	case preview_auto:
		bf3905_write_array(sd, bf3905_previewmode_mode00);
		break;
	case nightmode:
		bf3905_write_array(sd, bf3905_previewmode_mode01);
		break;
	case dynamic:
	case beatch:
	case bar:
	case snow:
	case landscape:
		break;
	case theatre:
	case party:
	case candy:
	case fireworks:
	case sunset:
	case nightmode_portrait:
		break;
	case sports:
	case anti_shake:
		bf3905_write_array(sd, bf3905_previewmode_mode02);
		break;
	case portrait:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3905_get_max_gain(struct v4l2_subdev *sd, int *value)
{
	int ret = 0;

	*value = BF3905_MAX_GAIN;

	return ret;
}

static int bf3905_get_min_gain(struct v4l2_subdev *sd, int *value)
{
	int ret = 0;

	*value = BF3905_MIN_GAIN;

	return ret;
}

static int bf3905_get_iso(struct v4l2_subdev *sd, int *value)
{
	int ret = 0;
	unsigned char val;

	ret = bf3905_read(sd, 0x87, &val);
	if (ret < 0)
		return ret;
	*value = (int)val;

	return ret;
}

static int bf3905_set_iso(struct v4l2_subdev *sd, int iso)
{
	printk("%s, set iso %d\n",__func__, iso);

	switch(iso) {
	case ISO_AUTO:
		bf3905_write_array(sd, bf3905_iso_auto);
		break;
	case ISO_100:
		bf3905_write_array(sd, bf3905_iso_100);
		break;
	case ISO_200:
		bf3905_write_array(sd, bf3905_iso_200);
		break;
	case ISO_400:
		bf3905_write_array(sd, bf3905_iso_400);
		break;
	case ISO_800:
		bf3905_write_array(sd, bf3905_iso_800);
		break;
	default:
		return -EINVAL;
	}

        return 0;
}

static int bf3905_set_sharpness(struct v4l2_subdev *sd, int sharp)
{
	printk("%s,set sharp %d\n",__func__, sharp);

	switch (sharp) {
	case SHARPNESS_L2:
		bf3905_write_array(sd, bf3905_sharpness_l2);
		break;
	case SHARPNESS_L1:
		bf3905_write_array(sd, bf3905_sharpness_l1);
		break;
	case SHARPNESS_H0:
		bf3905_write_array(sd, bf3905_sharpness_h0);
		break;
	case SHARPNESS_H1:
		bf3905_write_array(sd, bf3905_sharpness_h1);
		break;
	case SHARPNESS_H2:
		bf3905_write_array(sd, bf3905_sharpness_h2);
		break;
	case SHARPNESS_H3:
		bf3905_write_array(sd, bf3905_sharpness_h3);
		break;
	case SHARPNESS_H4:
		bf3905_write_array(sd, bf3905_sharpness_h4);
		break;
	case SHARPNESS_H5:
		bf3905_write_array(sd, bf3905_sharpness_h5);
		break;
	case SHARPNESS_H6:
		bf3905_write_array(sd, bf3905_sharpness_h6);
		break;
	case SHARPNESS_H7:
		bf3905_write_array(sd, bf3905_sharpness_h7);
		break;
	case SHARPNESS_H8:
		bf3905_write_array(sd, bf3905_sharpness_h8);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3905_get_shutter_speed(struct v4l2_subdev *sd, int *value)
{
	unsigned char val1 = 0;
	unsigned char val2 = 0;
	unsigned int ret = 0;
	unsigned int exp_line, pclk;

	ret = bf3905_read(sd, 0x8c, &val1);
	if (ret) {
		printk("bf3905 read 0x8c failed\n");
		return ret;
	}

	ret = bf3905_read(sd, 0x8d, &val2);
	if (ret) {
		printk("bf3905 read 0x8d failed\n");
		return ret;
	}

	exp_line = (int)((val1 & 0xff) << 8 | val2);
	pclk = BF3905_MCLK / 2;
	if (exp_line > 522) {
		*value = 1000000 * exp_line / pclk;
	}
	else
		*value = 1000000 * 812 /pclk;

	return ret;
}

static int bf3905_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3905_info *info = container_of(sd, struct bf3905_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&bf3905_isp_setting;
		isp_setting.size = ARRAY_SIZE(bf3905_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&bf3905_isp_parm;
		break;
	case V4L2_CID_GET_BRIGHTNESS_LEVEL:
		ctrl->value = bf3905_flash_ctrl_para(sd);
		break;
	case V4L2_CID_ISO:
		ret = bf3905_get_iso(sd, &ctrl->value);
		if (ret)
			printk("bf3905 get iso failed\n");
		break;
	case V4L2_CID_SNAPSHOT_GAIN:
		ret = bf3905_get_iso(sd, &ctrl->value);
		if (ret)
			printk("bf3905 get iso failed\n");
		break;
	case V4L2_CID_MIN_GAIN:
		ret = bf3905_get_min_gain(sd, &ctrl->value);
		if (ret)
			printk("bf3905 get min gain failed\n");
		break;
	case V4L2_CID_MAX_GAIN:
		ret = bf3905_get_max_gain(sd, &ctrl->value);
		if (ret)
			printk("bf3905 get max gain failed\n");
		break;
	case V4L2_CID_SHARPNESS:
		ctrl->value = info->sharpness;
		break;
	case V4L2_CID_SCENE:
		ctrl->value = info->scene_mode;
		break;
	case V4L2_CID_SHUTTER_SPEED:
		ret = bf3905_get_shutter_speed(sd, &ctrl->value);
		if (ret)
			printk("bf3905 get shutter speed failed\n");
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bf3905_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3905_info *info = container_of(sd, struct bf3905_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = bf3905_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = bf3905_set_contrast(sd, ctrl->value);
		if (!ret)
			info->contrast = ctrl->value;
		break;
	case V4L2_CID_SATURATION:
		ret = bf3905_set_saturation(sd, ctrl->value);
		if (!ret)
			info->saturation = ctrl->value;
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		ret = bf3905_set_wb(sd, ctrl->value);
		if (!ret)
			info->wb = ctrl->value;
		break;
	case V4L2_CID_EXPOSURE:
		ret = bf3905_set_exposure(sd, ctrl->value);
		if (!ret)
			info->exposure = ctrl->value;
		break;
	case V4L2_CID_EFFECT:
		ret = bf3905_set_effect(sd, ctrl->value);
		if (!ret)
			info->effect = ctrl->value;
		break;
	case V4L2_CID_SCENE:
		ret = bf3905_set_scene(sd, ctrl->value);
		if (!ret)
			info->scene_mode = ctrl->value;
		break;
	case V4L2_CID_FRAME_RATE:
		ret = bf3905_set_framerate(sd, ctrl->value);
		break;
	case V4L2_CID_SET_FOCUS:
		break;
	case V4L2_CID_ISO:
		ret = bf3905_set_iso(sd, ctrl->value);
		break;
	case V4L2_CID_SHARPNESS:
		ret = bf3905_set_sharpness(sd, ctrl->value);
		if (!ret)
			info->sharpness = ctrl->value;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static int bf3905_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_BF3905, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bf3905_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bf3905_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int bf3905_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	bf3905_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops bf3905_core_ops = {
	.g_chip_ident = bf3905_g_chip_ident,
	.g_ctrl = bf3905_g_ctrl,
	.s_ctrl = bf3905_s_ctrl,
	.queryctrl = bf3905_queryctrl,
	.reset = bf3905_reset,
	.init = bf3905_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = bf3905_g_register,
	.s_register = bf3905_s_register,
#endif
};

static const struct v4l2_subdev_video_ops bf3905_video_ops = {
	.enum_mbus_fmt = bf3905_enum_mbus_fmt,
	.try_mbus_fmt = bf3905_try_mbus_fmt,
	.s_mbus_fmt = bf3905_s_mbus_fmt,
	.s_stream = bf3905_s_stream,
	.cropcap = bf3905_cropcap,
	.g_crop	= bf3905_g_crop,
	.s_parm = bf3905_s_parm,
	.g_parm = bf3905_g_parm,
	.enum_frameintervals = bf3905_enum_frameintervals,
	.enum_framesizes = bf3905_enum_framesizes,
};

static const struct v4l2_subdev_ops bf3905_ops = {
	.core = &bf3905_core_ops,
	.video = &bf3905_video_ops,
};

static int bf3905_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct bf3905_info *info;
	struct comip_camera_subdev_platform_data *bf3905_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct bf3905_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &bf3905_ops);

	/* Make sure it's an bf3905*/
	ret = bf3905_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an bf3905 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "bf3905 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	bf3905_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;

	if (bf3905_setting) {
		if (bf3905_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			bf3905_isp_parm.mirror = 1;
		else
			bf3905_isp_parm.mirror = 0;

		if (bf3905_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			bf3905_isp_parm.flip = 1;
		else
			bf3905_isp_parm.flip = 0;
	}

	return ret;
}

static int bf3905_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf3905_info *info = container_of(sd, struct bf3905_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id bf3905_id[] = {
	{ "bf3905-mipi-yuv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf3905_id);

static struct i2c_driver bf3905_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bf3905-mipi-yuv",
	},
	.probe		= bf3905_probe,
	.remove		= bf3905_remove,
	.id_table	= bf3905_id,
};

static __init int init_bf3905(void)
{
	return i2c_add_driver(&bf3905_driver);
}

static __exit void exit_bf3905(void)
{
	i2c_del_driver(&bf3905_driver);
}

fs_initcall(init_bf3905);
module_exit(exit_bf3905);

MODULE_DESCRIPTION("A low-level driver for BYD bf3905 sensors");
MODULE_LICENSE("GPL");
