
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
#include "bf3a20_mipi.h"

//ADD  BY  yzx  2014.06.15

#define BF3A20_MIPI_CHIP_ID_H	(0x39)
#define BF3A20_MIPI_CHIP_ID_L	(0x20)

#define VGA_WIDTH		648
#define VGA_HEIGHT		480
#define SVGA_WIDTH		800
#define SVGA_HEIGHT		600
#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200

#define MAX_PREVIEW_WIDTH		SVGA_WIDTH
#define MAX_PREVIEW_HEIGHT  	SVGA_HEIGHT

//#define V4L2_I2C_ADDR_8BIT	(0x0001)

#define BF3A20_MIPI_REG_END		0xff
//#define BF3A20_MIPI_REG_DELAY	0xfffe

struct bf3a20_mipi_format_struct;
struct bf3a20_mipi_info {
	struct v4l2_subdev sd;
	struct bf3a20_mipi_format_struct *fmt;
	struct bf3a20_mipi_win_size *win;
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
	int bf3a20_mipi_first_open;
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

//#define BF3A20_AUTO_LSC_CAL

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


static struct regval_list bf3a20_mipi_init_regs[] = {
#if 0
			//static const struct regval_list bf3a20_init_regs[] = {
			{0x15, 0x80},//f2//zzg  //Bit[1]: VSYNC option.Bit[0]: HSYNC option
			{0x09, 0x42},  //bit[7]:1-soft sleep mode
			{0x21, 0x55}, ///15//ryx //Drive capability	
			{0x3a, 0x02},  //YUV422 Sequence
			//{0x1e, 0x46},//ryx
			{0x1b, 0x2c},
			{0x11, 0x30}, //38 .
			{0x2a, 0x0c},
			{0x2b, 0x1c},  ///94ryx ///Dummy Pixel Insert LSB
			{0x92, 0x02},
			{0x93, 0x00},
			//initial AWB and AE		//ryx
			{0x13, 0x00},  //Manual AWB & AE
			{0x8c, 0x02},//0x0b//ryx
			{0x8d, 0xf1},//0xc4//ryx
			{0x24, 0xc8},
			{0x87, 0x2d},
			{0x01, 0x12},
			{0x02, 0x22},
			{0x13, 0x07},
			//black level 
			{0x27, 0x98},
			{0x28, 0xff},
			{0x29, 0x60},///70ryx
			{0x20, 0x40},///ryx
			{0x1f, 0x20},
			{0x22, 0x20},
			{0x20, 0x20},
			{0x26, 0x20},
			{0xbb, 0x30},
			{0xe2, 0xc4},
			{0xe7, 0x4e},
			{0x08, 0x16},
			{0x16, 0x28},
			{0x1c, 0x00},
			{0x1d, 0xf1},
			{0xed, 0x8f},//0b  ryx
			{0x3e, 0x80},
			{0xf9, 0x80},//00   jin bu le  mipi // ryx
			{0xdb, 0x80},
			{0xf8, 0x83},
			{0xde, 0x1e},
			{0xf9, 0xc0},
			{0x59, 0x18},
			{0x5f, 0x28},
			{0x6b, 0x70},
			{0x6c, 0x00},
			{0x6d, 0x10},
			{0x6e, 0x08},
			{0xf9, 0x80},
			{0x59, 0x00},
			{0x5f, 0x10},
			{0x6b, 0x00},
			{0x6c, 0x28},
			{0x6d, 0x08},
			{0x6e, 0x00},
			{0x21, 0x55},
			//mipi end
			/*//black sun
			{0xfb, 0x06}, //72M
			{0xfb, 0x0a}, //48M
			{0xfb, 0x0c}, //36M
			{0xfb, 0x12}, //24M
			*/
			//lens shading		//ryx
			{0x1e, 0x46}, 
			{0x35, 0x38}, //38////lens shading gain of R//38
			{0x65, 0x30}, //lens shading gain of G
			{0x66, 0x30},
			{0xbc, 0xd3},
			{0xbd, 0x5c},
			{0xbe, 0x24},
			{0xbf, 0x13},
			{0x9b, 0x5c},
			{0x9c, 0x24},
			{0x36, 0x13},
			{0x37, 0x5c},
			{0x38, 0x24},
			//denoise and edge enhancement		//ryx
			{0x70, 0x01},
			{0x72, 0x62}, //0x72[7:6]: Bright edge enhancement; 0x72[5:4]: Dark edge enhancement
			{0x78, 0x37},
			{0x7a, 0x29}, //0x7a[7:4]: The bigger, the smaller noise; 0x7a[3:0]: The smaller, the smaller noise
			{0x7d, 0x85},
			// gamma    arima//ryx
			{0x39, 0xa8},
			{0x3f, 0x28},
			{0x40, 0x25},
			{0x41, 0x28},
			{0x42, 0x25},
			{0x43, 0x23},
			{0x44, 0x1b},
			{0x45, 0x18},
			{0x46, 0x15},
			{0x47, 0x11},
			{0x48, 0x10},
			{0x49, 0x0f},
			{0x4b, 0x0e},
			{0x4c, 0x0c},
			{0x4e, 0x0a},
			{0x4f, 0x06},
			{0x50, 0x05},
			/*
			// gamma low noise (default)
			{0x40, 0x20},
			{0x41, 0x25},
			{0x42, 0x23},
			{0x43, 0x1f},
			{0x44, 0x18},
			{0x45, 0x15},
			{0x46, 0x12},
			{0x47, 0x10},
			{0x48, 0x10},
			{0x49, 0x0f},
			{0x4b, 0x0e},
			{0x4c, 0x0c},
			{0x4e, 0x0b},
			{0x4f, 0x0a},
			{0x50, 0x07},
			{0x39, 0xa4},
			{0x3f, 0x24},
			*/																		  
			/*//gamma
			{0x40, 0x28},
			{0x41, 0x28},
			{0x42, 0x30},
			{0x43, 0x29},
			{0x44, 0x23},
			{0x45, 0x1b},
			{0x46, 0x17},
			{0x47, 0x0f},
			{0x48, 0x0d},
			{0x49, 0x0b},
			{0x4b, 0x09},
			{0x4c, 0x08},
			{0x4e, 0x07},
			{0x4f, 0x05},
			{0x50, 0x04},
			{0x39, 0xa0},
			{0x3f, 0x20},
			*/
			/*
			//gamma
			{0x40, 0x28},
			{0x41, 0x26},
			{0x42, 0x24},
			{0x43, 0x22},
			{0x44, 0x1c},
			{0x45, 0x19},
			{0x46, 0x15},
			{0x47, 0x11},
			{0x48, 0x0f},
			{0x49, 0x0e},
			{0x4b, 0x0c},
			{0x4c, 0x0a},
			{0x4e, 0x09},
			{0x4f, 0x08},
			{0x50, 0x04},
			{0x39, 0x20},
			{0x3f, 0xa0},
			*/
			// !A  color    arima//ryx
			{0x5c, 0x00},
			{0x51, 0x0a},
			{0x52, 0x11},
			{0x53, 0x7f},
			{0x54, 0x55},
			{0x57, 0x7e},
			{0x58, 0x10},
			{0x5a, 0x16},
			{0x5e, 0x38},
			/*
			// !A  color default
			{0x5c, 0x00},
			{0x51, 0x32},
			{0x52, 0x17},
			{0x53, 0x8c},
			{0x54, 0x79},
			{0x57, 0x6e},
			{0x58, 0x01},
			{0x5a, 0x36},
			{0x5e, 0x38},
			*/
			/*
			//color
			{0x5c, 0x00},
			{0x51, 0x24},
			{0x52, 0x0b},
			{0x53, 0x77},
			{0x54, 0x65},
			{0x57, 0x5e},
			{0x58, 0x19},
			{0x5a, 0x16},
			{0x5e, 0x38},
			*/

			/*//color
			{0x5c, 0x00},
			{0x51, 0x2b},
			{0x52, 0x0e},
			{0x53, 0x9f},
			{0x54, 0x7d},
			{0x57, 0x91},
			{0x58, 0x21},
			{0x5a, 0x16},
			{0x5e, 0x38},
			*/

			/*//color
			{0x5c, 0x00},
			{0x51, 0x24},
			{0x52, 0x0b},
			{0x53, 0x77},
			{0x54, 0x65},
			{0x57, 0x5e},
			{0x58, 0x19},
			{0x5a, 0x16},
			{0x5e, 0x38},
			*/

			// A color			//ryx
			{0x5c, 0x80},
			{0x51, 0x22},
			{0x52, 0x00},
			{0x53, 0x96},
			{0x54, 0x8c},
			{0x57, 0x7f},
			{0x58, 0x0b},
			{0x5a, 0x14},
			{0x5e, 0x38},
			// saturation  and auto contrast	//ryx
			{0x56, 0x28},
			{0x55, 0x00}, //when 0x56[7:6]=2'b0x,0x55[7:0]:Brightness control.bit[7]: 0:positive  1:negative [6:0]:value
			{0xb0, 0x8d},
			{0xb1, 0x15}, //when 0x56[7:6]=2'b0x,0xb1[7]:auto contrast switch(0:on 1:off)
			{0x56, 0xe8},
			{0x55, 0xf0}, //when 0x56[7:6]=2'b11,cb coefficient of F light
			{0xb0, 0xb0}, //b8//when 0x56[7:6]=2'b11,cr coefficient of F light
			{0x56, 0xa8},
			{0x55, 0xd8}, //c8//when 0x56[7:6]=2'b10,cb coefficient of !F light
			{0xb0, 0xa8}, //98//when 0x56[7:6]=2'b10,cr coefficient of !F light
			/*
			// manual contrast
			{0x56, 0xe8},
			{0xb1, 0x95}, //when 0x56[7:6]=2'b0x,0xb1[7]:auto contrast switch(0:on 1:off)
			{0xb4, 0x40}, //when 0x56[7:6]=2'b11,manual contrast coefficient
			*/
			//AE	   //ryx
			{0x13, 0x47}, //	07//ryx
			{0x24, 0x40}, // AE Target //4f//  ryx
			{0x25, 0x88}, // Bit[7:4]: AE_LOC_INT; Bit[3:0]:AE_LOC_GLB
			{0x97, 0x40}, // Y target value1.		//30//ryx
			{0x98, 0x08}, //
			{0x80, 0xd2}, //96//ryx  Bit[3:2]: the bigger, Y_AVER_MODIFY is smaller	92	 //0x92
			{0x81, 0xf0}, //AE speed //e0//ryx
			{0x82, 0x22}, //minimum global gain  //30//ryx
			{0x83, 0x44},//60//ryx
			{0x84, 0x45},//78//ryx
			{0x85, 0x5d},//90//ryx
			{0x86, 0x90}, //maximum gain  //97//ryx
			{0x89, 0x65}, //INT_MAX_MID //6d		//9d//ryx
			{0x8a, 0x7f}, //50 hz banding  bf c3
			{0x8b, 0x69}, ////64//60hz  banding
			{0x94, 0xc2}, //82//ryx //Bit[7:4]: Threshold for over exposure pixels, the smaller, the more over exposure pixels; Bit[3:0]: Control the start of AE.42
			//AWB			  //ryx
			{0x6a, 0x81},
			{0x23, 0x66},  // green gain
			{0xa1, 0x31},
			{0xa2, 0x0b},  //the low limit of blue gain for indoor scene
			{0xa3, 0x25},  //the upper limit of blue gain for indoor scene
			{0xa4, 0x09},  //the low limit of red gain for indoor scene
			{0xa5, 0x26},  //the upper limit of red gain for indoor scene
			{0xa7, 0x1a},  //9a//ryx //Base B gain2???òéDT??
			{0xa8, 0x15},  //Base R gain2???òéDT??
			{0xa9, 0x13},
			{0xaa, 0x12},
			{0xab, 0x16},
			{0xac, 0x20},//ryx
			{0xae, 0x54},//ryx
			{0xc8, 0x10},
			{0xc9, 0x15},
			{0xd2, 0x78},
			{0xd4, 0x20},
			//Skin
			{0xee, 0x2a},
			{0xef, 0x1b},
			//mipi   setting//ryx
			{0xdb, 0x80},
			{0xf8, 0x83},
			{0xde, 0x1e},
			{0xf9, 0xc0},
			{0x59, 0x18},
			{0x5f, 0x28},
			{0x6b, 0x70},
			{0x6c, 0x00},
			{0x6d, 0x10},
			{0x6e, 0x08},
			{0xf9, 0x80},
			{0x59, 0x00},
			{0x5f, 0x10},
			{0x6b, 0x00},
			{0x6c, 0x28},
			{0x6d, 0x08},
			{0x6e, 0x00},
			{0x21, 0x55},
			//mipi end
			{0xd0, 0x11},///the offset of  F light
			{0xd1, 0x00},///c0//he offset of non-f light

			{0x98, 0x0a},///8a   ryx  LC1810
			{0x70, 0x01},
			{0x69, 0x00},
			{0x67, 0x80},
			{0x68, 0x80},
			{0x56, 0x28},
			{0xb0, 0x8d},
			{0x56, 0x28},
			{0xb1, 0x95},///15   ryx
			{0x56, 0xe8},
			{0xb4, 0x40},

			{0x4a, 0x80},
			{0xc0, 0x00},
			{0x03, 0x60},
			{0x17, 0x00},
			{0x18, 0x40},
			{0x10, 0x40},
			{0x19, 0x00},
			{0x1a, 0xb0},
			{0x0b, 0x00},//10
			{0x1b, 0x2c},
			{0x11, 0x30},// 2c
			{0x8a, 0x7f},//0x82
			//};
#endif
	 //clock，dummy， bandingding
//	{0x15,0x80}, //c2
//	{0x09,0x42},
//	{0x21,0x55},
//	{0x3a,0x02},
//add   ryx
	{0x09,0xc2},  //c2
	{0x29,0x60},
	{0x12,0x00},
	{0x13,0x07},
	{0xe2,0xc4},
	{0xe7,0x4e},
	{0x29,0x60},
	{0x08,0x16},
	{0x16,0x2a},
	{0x1c,0x00},
	{0x1d,0xf0},
	{0xbb,0x30},

	{0x3e,0x80},
	{0x27,0x98},

	   //lens shading
//0x1e,0x46},
	{0x35,0x30}, //lens shading gain of R
       {0x65,0x30}, //lens shading gain of G
	{0x66,0x2a}, //lens shading gain of B
	{0xbc,0xd3},
	{0xbd,0x5c},
	{0xbe,0x24},
	{0xbf,0x13},
	{0x9b,0x5c},
	{0x9c,0x24},
	{0x36,0x13},
	{0x37,0x5c},
	{0x38,0x24},

	{0x92,0x02},//Dummy Line Insert LSB
	{0x93,0x00},
	{0x2a,0x0c},
	{0x2b,0x13},//Dummy Pixel Insert LSB
   
   //initial AWB and AE
	{0x13,0x00},
	{0x8c,0x0b},
	{0x8d,0xc4},
	{0x24,0xc8},
	{0x87,0x2d},
	{0x01,0x12},
	{0x02,0x22},
	{0x13,0x07},
 


 //black sun
/*
	{0xfb,0x06}, //72M
	{0xfb,0x0a},//48M
	{0xfb,0x0c}, //36M
	{0xfb,0x12}, //24M
	*/
   														  


  //denoise and edge enhancement
	{0x70,0x01},
	{0x72,0x62},//0x72[7:6]: Bright edge enhancement; 0x72[5:4]: Dark edge enhancement
	{0x78,0x37},
	{0x7a,0x29},//0x7a[7:4]: The bigger, the smaller noise; 0x7a[3:0]: The smaller, the smaller noise
	{0x7d,0x85},	//

	{0x39,0xa8}, //0xa0 zzg
	{0x3f,0x28}, //0x20 zzg


 // gamma default(low noise)
  /*
	{0x40,0x20},
	{0x41,0x25},
	{0x42,0x23},
	{0x43,0x1f},
	{0x44,0x18},
	{0x45,0x15},
	{0x46,0x12},
	{0x47,0x10},
	{0x48,0x10},
	{0x49,0x0f},
	{0x4b,0x0e},
	{0x4c,0x0c},
	{0x4e,0x0b},
	{0x4f,0x09},
	{0x50,0x07},
         * /

 //gamma清晰亮丽
/*
	{0x40,0x28},
	{0x41,0x26},
	{0x42,0x24},
	{0x43,0x22},
	{0x44,0x1c},
	{0x45,0x19},
	{0x46,0x11},
	{0x47,0x10},
	{0x48,0x0f},
	{0x49,0x0e},
	{0x4b,0x0c},
	{0x4c,0x0a},
	{0x4e,0x09},
	{0x4f,0x08},
	{0x50,0x04},
  */

     /*
  //gamma 过曝亮度好，高亮

	{0x40,0x28},
	{0x41,0x28},
	{0x42,0x30},
	{0x43,0x29},
	{0x44,0x23},
	{0x45,0x1b},
	{0x46,0x17},
	{0x47,0x0f},
	{0x48,0x0d},
	{0x49,0x0b},
	{0x4b,0x09},
	{0x4c,0x08},
	{0x4e,0x07},
	{0x4f,0x05},
	{0x50,0x04},
	*/
      //// from ryx   zzg
	{0x40, 0x25},
	{0x41, 0x28},
	{0x42, 0x25},
	{0x43, 0x23},
	{0x44, 0x1b},
	{0x45, 0x18},
	{0x46, 0x15},
	{0x47, 0x11},
	{0x48, 0x10},
	{0x49, 0x0f},
	{0x4b, 0x0e},
	{0x4c, 0x0c},
	{0x4e, 0x0a},
	{0x4f, 0x06},
	{0x50, 0x05},




     
 //color default
 /*
	{0x5c,0x00},
	{0x51,0x32},
	{0x52,0x17},
	{0x53,0x8C},
	{0x54,0x79},
	{0x57,0x6E},
	{0x58,0x01},
	{0x5a,0x36},
	{0x5e,0x38},
  */
      
  //color 绿色好红色主系数高

	{0x5c,0x00},
	{0x51,0x24},//16
	{0x52,0x0b},//e
	{0x53,0x77},//6f
	{0x54,0x65},//66
	{0x57,0x5e},//76
	{0x58,0x19},//15
	{0x5a,0x16},//16
	{0x5e,0x60},//0x38  zzg

       /*
 //color 艳丽

	{0x5c,0x00},
	{0x51,0x2b},
	{0x52,0x0e},
	{0x53,0x9f},
	{0x54,0x7d},
	{0x57,0x91},
	{0x58,0x21},
	{0x5a,0x16},
	{0x5e,0x38},    */

 
 //color 色彩淡
 /*
	{0x5c,0x00},
	{0x51,0x24},
	{0x52,0x0b},
	{0x53,0x77},
	{0x54,0x65},
	{0x57,0x5e},
	{0x58,0x19},
	{0x5a,0x16},
	{0x5e,0x38},
  */
 //A color
	{0x5c,0x80},
	{0x51,0x22},
	{0x52,0x00},
	{0x53,0x96},
	{0x54, 0x8c},
	{0x57, 0x7f},
	{0x58, 0x0b},
	{0x5a,0x14},
	{0x5e,0x60}, ////0x38 zzg
  	// saturation  and auto contrast
	{0x56,0x28},
	{0x55,0x00},//when 0x56[7:6]=2'b0x,0x55[7:0]:Brightness control.bit[7]: 0:positive  1:negative [6:0]:value
	{0xb0,0x8d},
	{0xb1,0x15},//when 0x56[7:6]=2'b0x,0xb1[7]:auto contrast switch(0:on 1:off)
	{0x56,0xe8},
	{0x55,0xf0},//when 0x56[7:6]=2'b11,cb coefficient of F light
	{0xb0,0xb8},//when 0x56[7:6]=2'b11,cr coefficient of F light
	{0x56,0xa8},
	{0x55,0xc8},//when 0x56[7:6]=2'b10,cb coefficient of !F light
	{0xb0,0x98},//when 0x56[7:6]=2'b10,cr coefficient of !F light
  // manual contrast
 /*
	{0x56,0xe8},
	{0xb1,0x95},//when 0x56[7:6]=2'b0x,0xb1[7]:auto contrast switch(0:on 1:off)
	{0xb4,0x40},//when 0x56[7:6]=2'b11,manual contrast coefficient
  */
 //AE
	{0x13,0x07},
	{0x24,0x40},// AE Target
	{0x25,0x88},// Bit[7:4]: AE_LOC_INT; Bit[3:0]:AE_LOC_GLB  0x88
	{0x97,0x30},// Y target value1.
	{0x98,0x08},// AE 窗口和权重  0x0a zzg 
	{0x80,0xd2},//Bit[3:2]: the bigger, Y_AVER_MODIFY is smaller     0x92
	{0x81,0xff},//AE speed  //0xe0
	{0x82,0x22},//minimum global gain   0x30 zzg
	{0x83,0x44}, //0x60 zzg
	{0x84,0x45}, ///0x65 zzg 
	{0x85,0x5d}, ///0x90 zzg
	{0x86,0x70},//maximum gain zzg 0xc0
	{0x89,0x6d},//INT_MAX_MID    9d zzg
	{0x8a,0xbf},//50 hz banding 
	{0x8b,0x9f},//60hz  banding  
	{0x94,0xc2},//Bit[7:4]: Threshold for over exposure pixels, the smaller, the more over exposure pixels; Bit[3:0]: Control the start of AE. 0x42 zzg

	//AWB
	{0x6a,0x81},
	{0x23,0x66},// green gain
	{0xa1,0x31},
	{0xa2,0x0b},//the low limit of blue gain for indoor scene
	{0xa3,0x25},//the upper limit of blue gain for indoor scene
	{0xa4,0x09},//the low limit of red gain for indoor scene
	{0xa5,0x26},//the upper limit of red gain for indoor scene
	{0xa7,0x9a},//Base B gain不建议修改      
	{0xa8,0x15},//Base R gain不建议修改         
	{0xa9,0x13},
	{0xaa,0x12},
	{0xab,0x16},
	{0xac,0x20}, //avoiding red scence when openning
	{0xc8,0x10},
	{0xc9,0x15},
	{0xd2,0x78},
	{0xd4,0x20},

	 //Skin
	{0xee, 0x2a},
	{0xef, 0x1b},


		//window 1600*1200
	{0x4a,0x80},
	{0xc0,0x00},
	{0x0b,0x00},
	{0x03,0x60},
	{0x17,0x00},
	{0x18,0x40},
	{0x10,0x40},
	{0x19,0x00},
	{0x1a,0xb0},
	//640*480
	/*
	{0x4a,0x80},
	{0xc0,0x00},
	{0x0b,0x10},
	{0x03,0x20},
	{0x17,0x00},
	{0x18,0x80},
	{0x10,0x10},
	{0x19,0x00},
	{0x1a,0xe0},
	*/
	{0x1b,0x2c,},
	{0x11,0x30,},//PLL setting  0x38:3倍频;  0x34:2.5倍频； 0x30:2倍频; 0x2c：1.5倍频；  0x28:1倍频   


         //mipi
	{0xdb,0x80},
	{0xf8,0x83},
	{0xde,0x1e},
	{0xf9,0xc0},
	{0x59,0x18},
	{0x5f,0x28},
	{0x6b,0x70},
	{0x6c,0x00},
	{0x6d,0x10},
	{0x6e,0x08},
	{0xf9,0x80},
	{0x59,0x00},
	{0x5f,0x10},
	{0x6b,0x00},
	{0x6c,0x28},
	{0x6d,0x08},
	{0x6e,0x00},
	{0x21,0x55},

	{0x15,0xf2},
	{0x1e,0x66},    //66
	{0x3a,0x00},    //66

 
	///awb base gain offset
	{0xd0,0x11},
	{0xd1,0x09}, ///0x01 zzg

	{0x98,0x8a},
	{0x70,0x01},
	{0x69,0x00},
	{0x67,0x80},
	{0x68,0x80},
	{0x56,0x28},
	{0xb0,0x8d},  //none
	{BF3A20_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3a20_fmt_yuv422[] = {
 {BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_stream_on[] = {
	  {0x09, 0x42},  //bit[7]:1-soft sleep mode
	{BF3A20_MIPI_REG_END, 0xff},	/* END MARKER */  ///
};

static struct regval_list bf3a20_mipi_stream_off[] = {
	/* Sensor enter LP11*/
	{0x09, 0xc2},  //bit[7]:1-soft sleep mode
	{BF3A20_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_win_vga[] = {
	//WINDOW 640*480
	{0x4a , 0x80},
	{0x3a,0x00},    //66

	{0xc0 , 0xd3},
	{0xc1 , 0x04},
	{0xc2 , 0xc4},
	{0xc3 , 0x11},
	{0xc4 , 0x3f},
	{0xb5 , 0x3f},

	{0x0b , 0x10},
	{0x03 , 0x50},
	{0x17 , 0x00},
	{0x18 , 0x00},
	{0x10 , 0x30},
	{0x19 , 0x00},
	{0x1a , 0xc0},

 
	{BF3A20_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_win_sxga[] = {
	//WINDOW 1280*960
	{0x4a , 0x80},

//	{0xc0 , 0xd3},
//	{0xc1 , 0x04},
//	{0xc2 , 0xc4},
//	{0xc3 , 0x11},
//	{0xc4 , 0x3f},
//	{0xb5 , 0x3f},
	{0x3a,0x00},    //66

	{0x03 , 0x50},
	{0x17 , 0x00},
	{0x18 , 0x00},//500

	{0x10 , 0x30},
	{0x19 , 0x00},
	{0x1a , 0xc0},///3c0
	{0x0b , 0x00},
 
	{BF3A20_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_win_SVGA[] = {
	//WINDOW 800*600
	{0x4a, 0x80},
	{0xc0, 0x00},
	{0x3a,0x00},    //66

//	{0xca, 0x00},
//	{0xcb, 0x50},
//	{0xcc, 0x60},
//	{0xcd, 0x00},
//	{0xce, 0xc0},
//	{0xcf, 0x40},

	{0x03, 0x60},
	{0x17, 0x00},
	{0x18, 0x40},//640

	{0x10, 0x40},
	{0x19, 0x00},
	{0x1a, 0xb0},// 4b0
	{0x0b, 0x10},  //
 
	{BF3A20_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_win_2m[] = {
	//WINDOW 1600*1200
	{0x4a, 0x80},
	{0xc0, 0x00},
	{0x3a,0x00},    //66

//	{0xca, 0x00},
//	{0xcb, 0x50},
//	{0xcc, 0x60},
//	{0xcd, 0x00},
//	{0xce, 0xc0},
//	{0xcf, 0x40},

	{0x03, 0x60},
	{0x17, 0x00},
	{0x18, 0x40},

	{0x10, 0x40},
	{0x19, 0x00},
	{0x1a, 0xb0},
	{0x0b, 0x00},
 
#if 0//defined(BF3A20_AUTO_LSC_CAL)
	{0x21,0x55},
#endif

	{BF3A20_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_brightness_l3[] = {
	{0x24 , 0x30},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_brightness_l2[] = {
	{0x24 , 0x38},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_brightness_l1[] = {
	{0x24 , 0x40},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_brightness_h0[] = {
	{0x24 , 0x4f}, //default
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_brightness_h1[] = {
	{0x24 , 0x5f},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_brightness_h2[] = {
	{0x24 , 0x6f},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_brightness_h3[] = {
	{0x24 , 0x7f},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_saturation_l2[] = {
	{0xf1,0x00},
	{0x56,0xa8},
	{0x55,0x90},
	{0xb0,0x70},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_saturation_l1[] = {
	{0xf1,0x00},
	{0x56,0xa8},
	{0x55,0xb0},
	{0xb0,0x90},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_saturation_h0[] = {
	{0xf1,0x00},
	{0x56,0xa8},
	{0x55,0xb8},
	{0xb0,0xa8},//default
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_saturation_h1[] = {
	{0xf1,0x00},
	{0x56,0xa8},
	{0x55,0xd8},
	{0xb0,0xb8},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_saturation_h2[] = {
	{0xf1,0x00},
	{0x56,0xa8},
	{0x55,0xe8},
	{0xb0,0xd8},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3a20_mipi_contrast_l3[] = {
	{0x56,0x28},
	{0xb1,0x95},
	{0x56,0xe8},
	{0xb4,0x38},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_contrast_l2[] = {
	{0x56,0x28},
	{0xb1,0x95},
	{0x56,0xe8},
	{0xb4,0x3a},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_contrast_l1[] = {
	{0x56,0x28},
	{0xb1,0x95},
	{0x56,0xe8},
	{0xb4,0x3c},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_contrast_h0[] = {
	{0x56,0x28},
	{0xb1,0x95},
	{0x56,0xe8},
	{0xb4,0x40},//default 5level
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_contrast_h1[] = {
	{0x56,0x28},
	{0xb1,0x95},
	{0x56,0xe8},
	{0xb4,0x43},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_contrast_h2[] = {
	{0x56,0x28},
	{0xb1,0x95},
	{0x56,0xe8},
	{0xb4,0x47},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_contrast_h3[] = {
	{0x56,0x28},
	{0xb1,0x95},
	{0x56,0xe8},
	{0xb4,0x4a},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3a20_mipi_whitebalance_auto[] = {
	{0x01,0x0f},
	{0x02,0x13},
	{0x23,0x66},
	{0x13,0x07},//auto
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_whitebalance_incandescent[] = {
	{0x13,0x05},
	{0x01,0x28},
	{0x02,0x0a},
	{0x23,0x66},//INCANDESCENT
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_whitebalance_fluorescent[] = {
	{0x13,0x05},
	{0x01,0x1a},
	{0x02,0x1e},
	{0x23,0x66},//FLUORESCENT
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_whitebalance_daylight[] = {
	{0x13,0x05},
	{0x01,0x10},
	{0x02,0x20},
	{0x23,0x66},//DAYLIGHT,
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_whitebalance_cloudy[] = {
	{0x13,0x05},
	{0x01,0x0e},
	{0x02,0x24},
	{0x23,0x66},//CLOUDY_DAYLIGHT
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3a20_mipi_effect_none[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x00},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d},  //none  
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_mono[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x20},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d}, //mono
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_negative[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x01},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d},  //negative
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_solarize[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x00},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_sepia[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x20},{0x67,0x58},{0x68,0xa0},{0x56,0x28},{0xb0,0x8d},  //sepia
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_posterize[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x00},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_whiteboard[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x00},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_blackboard[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x00},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_effect_aqua[] = {
	{0x98,0x8a},{0x70,0x01},{0x69,0x00},{0x67,0x80},{0x68,0x80},{0x56,0x28},{0xb0,0x8d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3a20_mipi_exposure_l2[] = {
	{0x56,0x28},
	{0x55,0xb0},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_exposure_l1[] = {
	{0x56,0x28},
	{0x55,0x98},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_exposure_h0[] = {
	{0x56,0x28},
	{0x55,0x00}, //98  //ryx
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_exposure_h1[] = {
	{0x56,0x28},
	{0x55,0x18},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_exposure_h2[] = {
	{0x56,0x28},
	{0x55,0x30},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};


/////////////////////////next preview mode////////////////////////////////////


static struct regval_list bf3a20_mipi_previewmode_mode00[] = {

	/*case auto:
	case normal:
	case dynamic:
	case beatch:
	case bar:
	case snow:
	*/

	{0x86, 0x90},
	{0x89, 0x6d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bf3a20_mipi_previewmode_mode01[] = {

	/*
	case nightmode:
	case theatre:
	case party:
	case candy:
	case fireworks:
	case sunset:
	*/

	{0x86, 0xff},
	{0x89, 0x8d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_previewmode_mode02[] = {// portrait

	/*
	case sports:
	case anti_shake:
	*/

	{0x86, 0xc0},
	{0x89, 0x5d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3a20_mipi_previewmode_mode03[] = {//landscape

	/*
	case portrait:

	*/

	{0x86, 0xc0},
	{0x89, 0x6d},
	{BF3A20_MIPI_REG_END, 0x00},	/* END MARKER */
};


///////////////////////////////preview mode end////////////////////////


static int bf3a20_mipi_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int bf3a20_mipi_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int bf3a20_mipi_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != BF3A20_MIPI_REG_END) {
		int ret = bf3a20_mipi_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}


static int bf3a20mipi_flash_ctrl_para(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2;
//	unsigned short v,g1,g2;
//	u32 v_gain;
/*
	//2035

	//bf3a20_mipi_write(sd, 0xb6,0x00);
	ret = bf3a20_mipi_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;
	ret = bf3a20_mipi_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2;

	ret = bf3a20_mipi_read(sd, 0x03, &g1);
	if (ret < 0)
	ret = bf3a20_mipi_read(sd, 0x03, &g2);
	if (ret < 0)
	v_gain=v*g1*g2;


	if(v<1300)
		return 1;
	else if(v_gain>=(50437*49265)) //v_gain>=(37893*49157)
		return 0;

	else
		return 1;
*/
		ret = bf3a20_mipi_read(sd, 0x8c, &v1);
	if (ret<4)
		return 0;
	else
		return 1;

}

static int bf3a20_mipi_reset(struct v4l2_subdev *sd, u32 val)
{
	//bf3a20_mipi_write(sd, 0x12,0x80);

	return 0;
}

static int bf3a20_mipi_shutter(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2;
	unsigned short v;

	//unsigned char v3, v4,v5 ;


	//ret=bf3a20_mipi_read(sd, 0xb3, &v3);
	//ret=bf3a20_mipi_read(sd, 0xb4, &v4);
	//ret= bf3a20_mipi_read(sd, 0xb5, &v5);


	//bf3a20_mipi_write(sd, 0xfa,0x11);

	bf3a20_mipi_write(sd, 0x1b,0x2c);
	bf3a20_mipi_write(sd, 0x11,0x28);  // 1
	bf3a20_mipi_write(sd, 0x13,0x02);  //TURN OFF  AGC,AEC

	ret = bf3a20_mipi_read(sd, 0x8c, &v1);
	if (ret < 0)
		return ret;
	ret = bf3a20_mipi_read(sd, 0x8d, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2	 ;

	v = v /2 ;
	//if (v<1) return 0;

	bf3a20_mipi_write(sd, 0x8c, (v >> 8)&0xff );
	bf3a20_mipi_write(sd, 0x8d, v&0xff);

	return  0;
}



static int bf3a20_mipi_init(struct v4l2_subdev *sd, u32 val)
{
	struct bf3a20_mipi_info *info = container_of(sd, struct bf3a20_mipi_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;
	info->bf3a20_mipi_first_open= 1;

	return bf3a20_mipi_write_array(sd, bf3a20_mipi_init_regs);
}

static int bf3a20_mipi_detect(struct v4l2_subdev *sd)
{
		unsigned char v;
	int ret;

	ret = bf3a20_mipi_read(sd, 0xfc, &v);
	printk("CHIP_ID_H=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != BF3A20_MIPI_CHIP_ID_H)   //39
		return -ENODEV;

	ret = bf3a20_mipi_read(sd, 0xfd, &v);
	printk("CHIP_ID_L=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != BF3A20_MIPI_CHIP_ID_L)    //20
		return -ENODEV;

	return 0;
}

static struct bf3a20_mipi_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} bf3a20_mipi_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,  //
	//	 .mbus_code	= V4L2_MBUS_FMT_UYVY8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= bf3a20_fmt_yuv422,
	},
};

#define N_BF3A20_MIPI_FMTS ARRAY_SIZE(bf3a20_mipi_formats)

static struct bf3a20_mipi_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
} bf3a20_mipi_win_sizes[] = {
#if 0
		/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= bf3a20_mipi_win_vga,
	},
#endif
	/* SVGA */
	{
		.width		= SVGA_WIDTH,
		.height		= SVGA_HEIGHT,
		.regs 		= bf3a20_mipi_win_SVGA,
	},
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.regs 		= bf3a20_mipi_win_sxga,
	},
	/* MAX */

	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.regs 		= bf3a20_mipi_win_2m,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(bf3a20_mipi_win_sizes))

static int bf3a20_mipi_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_BF3A20_MIPI_FMTS)
		return -EINVAL;

	*code = bf3a20_mipi_formats[index].mbus_code;
	return 0;
}

static int bf3a20_mipi_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct bf3a20_mipi_format_struct **ret_fmt,
		struct bf3a20_mipi_win_size **ret_wsize)
{
	int index;
	struct bf3a20_mipi_win_size *wsize;

	for (index = 0; index < N_BF3A20_MIPI_FMTS; index++)
		if (bf3a20_mipi_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_BF3A20_MIPI_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = bf3a20_mipi_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = bf3a20_mipi_formats + index;

	fmt->field = V4L2_FIELD_NONE;
	for (wsize = bf3a20_mipi_win_sizes; wsize < bf3a20_mipi_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= bf3a20_mipi_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = bf3a20_mipi_formats[index].colorspace;
	return 0;
}

static int bf3a20_mipi_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return bf3a20_mipi_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int bf3a20_mipi_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct bf3a20_mipi_info *info = container_of(sd, struct bf3a20_mipi_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct bf3a20_mipi_format_struct *fmt_s;
	struct bf3a20_mipi_win_size *wsize;
	int ret,len,i = 0;
	unsigned char v;

	ret = bf3a20_mipi_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;


	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = bf3a20_mipi_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	ret = bf3a20_mipi_read(sd, 0x3a ,&v);
	printk("reg[0x3a]=%x ", v);


	if ((info->win != wsize) && wsize->regs) {
		memset(data, 0, sizeof(*data));
		data->mipi_clk = 156;//288; /* Mbps. */
		data->slave_addr = client->addr;

		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {

			len = ARRAY_SIZE(bf3a20_mipi_win_2m)-1;   //CAPTURE  WINDOWS
			for(i =0;i<len;i++) {
				data->reg[i].addr = bf3a20_mipi_win_2m[i].reg_num;
				data->reg[i].data = bf3a20_mipi_win_2m[i].value;
			}
			ret = bf3a20_mipi_shutter(sd);
			data->reg_num = len;
		}

		else if ((wsize->width == SVGA_WIDTH)
			&& (wsize->height ==SVGA_HEIGHT)) {
			if(info->bf3a20_mipi_first_open){
				info->bf3a20_mipi_first_open= 0;
				ret = bf3a20_mipi_write_array(sd, bf3a20_mipi_win_SVGA);    //CAPTURE  WINDOWS
				if (ret)
					return ret;
			}
			else{
			len = ARRAY_SIZE(bf3a20_mipi_win_SVGA) - 1;
			for(i =0;i<len;i++) {
				data->reg[i].addr = bf3a20_mipi_win_SVGA[i].reg_num;
				data->reg[i].data = bf3a20_mipi_win_SVGA[i].value;
			}
			data->reg_num = len;
			}
		}
		else if ((wsize->width == VGA_WIDTH)
			&& (wsize->height == VGA_HEIGHT)) {
#if 0
			if(info->bf3a20_mipi_first_open){
				info->bf3a20_mipi_first_open= 0;
				ret = bf3a20_mipi_write_array(sd, bf3a20_mipi_win_vga);
				if (ret)
					return ret;
			}
			else
#endif
		{
			len = ARRAY_SIZE(bf3a20_mipi_win_vga) - 1;  ///bf3a20_mipi_win_SVGA   ///PREVIEW  WINDOWS

			for(i =0;i<len;i++) {
				data->reg[i].addr = bf3a20_mipi_win_vga[i].reg_num;   ///bf3a20_mipi_win_SVGA     ///PREVIEW
				data->reg[i].data = bf3a20_mipi_win_vga[i].value;    ///bf3a20_mipi_win_SVGA        ///PREVIEW
			}
			data->reg_num = len;  //
			}
		}
	}
	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int bf3a20_mipi_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct bf3a20_mipi_info *info = container_of(sd, struct bf3a20_mipi_info, sd);
	//printk("bf3a20_mipi_set_framerate %d\n", framerate);
	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)  //30 ryx
		framerate = 30;  //30 ryx
	info->frame_rate = framerate;
	return 0;
}


static int bf3a20_mipi_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = bf3a20_mipi_write_array(sd, bf3a20_mipi_stream_on);
	else
		ret = bf3a20_mipi_write_array(sd, bf3a20_mipi_stream_off);

        //  msleep(150);//ppp
	return ret;
}

static int bf3a20_mipi_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int bf3a20_mipi_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int bf3a20_mipi_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3a20_mipi_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3a20_mipi_frame_rates[] = { 30, 15, 10, 5, 1 };

static int bf3a20_mipi_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(bf3a20_mipi_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = bf3a20_mipi_frame_rates[interval->index];
	return 0;
}

static int bf3a20_mipi_enum_framesizes(struct v4l2_subdev *sd,
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
		struct bf3a20_mipi_win_size *win = &bf3a20_mipi_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int bf3a20_mipi_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int bf3a20_mipi_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	//printk(KERN_DEBUG "[BF3A20]set brightness %d\n", brightness);
	switch (brightness) {
	case BRIGHTNESS_L3:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3a20_mipi_set_contrast(struct v4l2_subdev *sd, int contrast)
{

//printk(KERN_DEBUG "[BF3A20]set contrast %d\n", contrast);

	switch (contrast) {
	case CONTRAST_L3:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_contrast_l3);
		break;
	case CONTRAST_L2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_contrast_l2);
		break;
	case CONTRAST_L1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_contrast_l1);
		break;
	case CONTRAST_H0:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_contrast_h0);
		break;
	case CONTRAST_H1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_contrast_h1);
		break;
	case CONTRAST_H2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_contrast_h2);
		break;
	case CONTRAST_H3:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3a20_mipi_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	//printk(KERN_DEBUG "[BF3A20]set saturation %d\n", saturation);

	switch (saturation) {
	case SATURATION_L2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_saturation_l2);
		break;
	case SATURATION_L1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_saturation_l1);
		break;
	case SATURATION_H0:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_saturation_h0);
		break;
	case SATURATION_H1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_saturation_h1);
		break;
	case SATURATION_H2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_saturation_h2);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3a20_mipi_set_wb(struct v4l2_subdev *sd, int wb)
{
	//printk(KERN_DEBUG "[BF3A20]set contrast %d\n", wb);

	switch (wb) {
	case WHITE_BALANCE_AUTO:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_whitebalance_auto);
		break;
	case WHITE_BALANCE_INCANDESCENT:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_whitebalance_incandescent);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_whitebalance_fluorescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_whitebalance_daylight);
		break;
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_whitebalance_cloudy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
static int bf3a20_mipi_set_effect(struct v4l2_subdev *sd, int effect)
{
	//printk(KERN_DEBUG "[BF3A20]set contrast %d\n", effect);

	switch (effect) {
	case EFFECT_NONE:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_none);
		break;
	case EFFECT_MONO:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_mono);
		break;
	case EFFECT_NEGATIVE:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_negative);
		break;
	case EFFECT_SOLARIZE:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_solarize);
		break;
	case EFFECT_SEPIA:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_sepia);
		break;
	case EFFECT_POSTERIZE:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_posterize);
		break;
	case EFFECT_WHITEBOARD:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_whiteboard);
		break;
	case EFFECT_BLACKBOARD:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_blackboard);
		break;
	case EFFECT_AQUA:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_effect_aqua);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3a20_mipi_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	//printk(KERN_DEBUG "[BF3A20]set exposure %d\n", exposure);

	switch (exposure) {
	case EXPOSURE_L2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_exposure_l2);
		break;
	case EXPOSURE_L1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_exposure_l1);
		break;
	case EXPOSURE_H0:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_exposure_h0);
		break;
	case EXPOSURE_H1:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_exposure_h1);
		break;
	case EXPOSURE_H2:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_exposure_h2);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int bf3a20_mipi_set_previewmode(struct v4l2_subdev *sd, int mode)
{
	//printk(KERN_INFO "MIKE:[BF3A20]set_previewmode %d\n", mode);
	switch (mode) {
	case preview_auto:
	case normal:
	case dynamic:
	case beatch:
	case bar:
	case snow:
	case landscape:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_previewmode_mode00);
		break;

	case nightmode:
	case theatre:
	case party:
	case candy:
	case fireworks:
	case sunset:
	case nightmode_portrait:

		bf3a20_mipi_write_array(sd, bf3a20_mipi_previewmode_mode01);
		break;

	case sports:
	case anti_shake:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_previewmode_mode02);

		break;

	case portrait:
		bf3a20_mipi_write_array(sd, bf3a20_mipi_previewmode_mode03);
		break;
	default:

		return -EINVAL;

		//return -EINVAL;
	}

	return 0;
}



static int bf3a20_mipi_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3a20_mipi_info *info = container_of(sd, struct bf3a20_mipi_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&bf3a20_mipi_isp_setting;
		isp_setting.size = ARRAY_SIZE(bf3a20_mipi_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&bf3a20_mipi_isp_parm;
		break;
	case V4L2_CID_GET_BRIGHTNESS_LEVEL:
		ctrl->value = bf3a20mipi_flash_ctrl_para(sd);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bf3a20_mipi_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3a20_mipi_info *info = container_of(sd, struct bf3a20_mipi_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = bf3a20_mipi_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = bf3a20_mipi_set_contrast(sd, ctrl->value);
		if (!ret)
			info->contrast = ctrl->value;
		break;
	case V4L2_CID_SATURATION:
		ret = bf3a20_mipi_set_saturation(sd, ctrl->value);
		if (!ret)
			info->saturation = ctrl->value;
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		ret = bf3a20_mipi_set_wb(sd, ctrl->value);
		if (!ret)
			info->wb = ctrl->value;
		break;
	case V4L2_CID_EXPOSURE:
		ret = bf3a20_mipi_set_exposure(sd, ctrl->value);
		if (!ret)
			info->exposure = ctrl->value;
		break;
	case V4L2_CID_EFFECT:
		ret = bf3a20_mipi_set_effect(sd, ctrl->value);
		if (!ret)
			info->effect = ctrl->value;
		break;

		case 10094870:// preview mode add

			ret = bf3a20_mipi_set_previewmode(sd, ctrl->value);
			if (!ret)
				info->previewmode = ctrl->value;

			break;

	case V4L2_CID_FRAME_RATE:
		ret = bf3a20_mipi_set_framerate(sd, ctrl->value);
		break;
	case V4L2_CID_SET_FOCUS:
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static int bf3a20_mipi_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_BF3A20, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bf3a20_mipi_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bf3a20_mipi_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int bf3a20_mipi_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	bf3a20_mipi_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops bf3a20_mipi_core_ops = {
	.g_chip_ident = bf3a20_mipi_g_chip_ident,
	.g_ctrl = bf3a20_mipi_g_ctrl,
	.s_ctrl = bf3a20_mipi_s_ctrl,
	.queryctrl = bf3a20_mipi_queryctrl,
	.reset = bf3a20_mipi_reset,
	.init = bf3a20_mipi_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = bf3a20_mipi_g_register,
	.s_register = bf3a20_mipi_s_register,
#endif
};

static const struct v4l2_subdev_video_ops bf3a20_mipi_video_ops = {
	.enum_mbus_fmt = bf3a20_mipi_enum_mbus_fmt,
	.try_mbus_fmt = bf3a20_mipi_try_mbus_fmt,
	.s_mbus_fmt = bf3a20_mipi_s_mbus_fmt,
	.s_stream = bf3a20_mipi_s_stream,
	.cropcap = bf3a20_mipi_cropcap,
	.g_crop	= bf3a20_mipi_g_crop,
	.s_parm = bf3a20_mipi_s_parm,
	.g_parm = bf3a20_mipi_g_parm,
	.enum_frameintervals = bf3a20_mipi_enum_frameintervals,
	.enum_framesizes = bf3a20_mipi_enum_framesizes,
};

static const struct v4l2_subdev_ops bf3a20_mipi_ops = {
	.core = &bf3a20_mipi_core_ops,
	.video = &bf3a20_mipi_video_ops,
};





static int bf3a20_mipi_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct bf3a20_mipi_info *info;
	int ret;

	info = kzalloc(sizeof(struct bf3a20_mipi_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &bf3a20_mipi_ops);

	/* Make sure it's an bf3a20_mipi */
	ret = bf3a20_mipi_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an bf3a20_mipi chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "bf3a20_mipi chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return ret;
}

static int bf3a20_mipi_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf3a20_mipi_info *info = container_of(sd, struct bf3a20_mipi_info, sd);

//device_remove_file(&client->dev, &dev_attr_bf3a20_mipi_rg_ratio_typical);
//	device_remove_file(&client->dev, &dev_attr_bf3a20_mipi_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id bf3a20_mipi_id[] = {
	{ "bf3a20_mipi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf3a20_mipi_id);

static struct i2c_driver bf3a20_mipi_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bf3a20_mipi",
	},
	.probe		= bf3a20_mipi_probe,
	.remove		= bf3a20_mipi_remove,
	.id_table	= bf3a20_mipi_id,
};

static __init int init_bf3a20_mipi(void)
{
	return i2c_add_driver(&bf3a20_mipi_driver);
}

static __exit void exit_bf3a20_mipi(void)
{
	i2c_del_driver(&bf3a20_mipi_driver);
}

fs_initcall(init_bf3a20_mipi);
module_exit(exit_bf3a20_mipi);

MODULE_DESCRIPTION("A low-level driver for  bf3a20_mipi sensors");
MODULE_LICENSE("GPL");
