
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
#include "gc2035.h"

#define GC2035_CHIP_ID_H	(0x20)
#define GC2035_CHIP_ID_L	(0x35)

#define VGA_WIDTH		648
#define VGA_HEIGHT		480
#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH	VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  VGA_HEIGHT

#define GC2035_REG_END		0xff
//#define GC2035_REG_DELAY	0xfffe

struct gc2035_format_struct;
struct gc2035_info {
	struct v4l2_subdev sd;
	struct gc2035_format_struct *fmt;
	struct gc2035_win_size *win;
	int brightness;
	int contrast;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list gc2035_init_regs[] = {
	{0xfe , 0x80},
	{0xfe , 0x80},
	{0xfe , 0x80},  
	{0xfc , 0x06},
	{0xf2 , 0x00},
	{0xf3 , 0x00},
	{0xf4 , 0x00},
	{0xf5 , 0x00},
	{0xf9 , 0xfe}, //[0] pll enable
	{0xfa , 0x00},
	{0xf6 , 0x00},
	{0xf7 , 0x15}, //pll enable

	{0xf8 , 0x85},
	{0xfe , 0x00},
	{0x82 , 0x00},
	{0xb3 , 0x60},
	{0xb4 , 0x40},
	{0xb5 , 0x60},

	{0x03 , 0x02},
	{0x04 , 0x80},

	//========================measure window  
	{0xfe , 0x00},
	{0xec , 0x06},//04 2012.10.26
	{0xed , 0x06},//04 2012.10.26
	{0xee , 0x62},//60 2012.10.26
	{0xef , 0x92},//90 2012.10.26

	////=============================analog

	{0x0a , 0x00}, //row start
	{0x0c , 0x00}, //col start
	{0x0d , 0x04},
	{0x0e , 0xc0},
	{0x0f , 0x06}, //Window setting
	{0x10 , 0x58}, 

	{0x17 , 0x14}, //[0]mirror [1]flip
	{0x18 , 0x0e}, //0a 2012.10.26
	{0x19 , 0x0c}, //AD pipe number

	{0x1a , 0x01}, //CISCTL mode4


	{0x1b , 0x8b},

	{0x1e , 0x88}, //analog mode1 [7] tx-high en [5:3]COL_bias
	{0x1f , 0x08}, //[3] tx-low en//

	{0x20 , 0x05}, //[0]adclk mode , 0x[1]rowclk_MODE [2]rsthigh_en
	{0x21 , 0x0f}, //[6:4]rsg
	{0x22 , 0xf0}, //[3:0]vref
	{0x23 , 0xc3}, //f3//ADC_r
	{0x24 , 0x17}, //pad drive

	//==============================aec
	//AEC
	{0xfe , 0x01},
	{0x11 , 0x20},//AEC_out_slope , 0x
	{0x1f , 0xc0},//max_post_gain
	{0x20 , 0x60},//max_pre_gain
	{0x47 , 0x30},//AEC_outdoor_th
	{0x0b , 0x10},//
	{0x13 , 0x75},//y_target
	{0xfe , 0x00},


	{0x05,0x01},//hb
	{0x06,0x11},
	{0x07,0x00},//vb
	{0x08,0x50},
	{0xfe,0x01},
	{0x27,0x00},//step
	{0x28,0xa0},
	{0x29,0x05},//level1
	{0x2a,0x00},
	{0x2b,0x05},//level2
	{0x2c,0x00},
	{0x2d,0x06},//6e8//level3
	{0x2e,0xe0},
	{0x2f,0x0a},//level4
	{0x30,0x00},
	{0xfe,0x00},
	{0xfe , 0x00},  //0x , 0x , 0x , 0x , 0x 
	{0xb6 , 0x03}, //AEC enable
	{0xfe , 0x00},

	////new exp , 0xmode/////
	//{0xfe , 0x01},
	//{0x0d , 0xf0}, //[7] exp level mode [6:4]index select
	//{0x1a , 0x9a}, //gain change ratio 0x80 =0x1x
	//{0x27 , 0x00},
	//{0x28 , 0xdd},

	//{0x29 , 0x02},
	//{0x2a , 0x97}, //level 0 , 0x18fps
	//{0x2b , 0x05}, 
	//{0x2c , 0x2e}, //level 1 16fps
	//{0x2d , 0x06},
	//{0x2e , 0x0b}, //level 2 , 0x14fps
	//{0x2f , 0x06},
	//{0x30 , 0xe8}, //level 3 12fps
	//{0x31 , 0x07},
	//{0x32 , 0xc5}, //11fps
	//{0x33 , 0x08},
	//{0x34 , 0xa2}, //10fps
	//{0x33 , 0x0a},
	//{0x34 , 0x5c}, //8fps

	//{0x37 , 0x30},//max gain1
	//{0x38 , 0x30},//max gain2
	//{0x39 , 0x30},//max gain3
	//{0x3a , 0x40},//max gain4
	//{0x3b , 0x40},//max gain5
	//{0x3c , 0x40},//max gain6
	//{0x3d , 0x40},//max gain7
	/////end/////

	////======================BLK

	{0x3f , 0x00}, //prc close
	{0x40 , 0x77},//
	{0x42 , 0x7f},
	{0x43 , 0x30},

	{0x5c , 0x08},
	{0x5e , 0x20},
	{0x5f , 0x20},
	{0x60 , 0x20},
	{0x61 , 0x20},
	{0x62 , 0x20},
	{0x63 , 0x20},
	{0x64 , 0x20},
	{0x65 , 0x20},

	///=================block

	{0x80 , 0xff},
	{0x81 , 0x26},//38 , 0x//skin_Y 8c_debug
	{0x87 , 0x90}, //[7]middle gamma 
	{0x84 , 0x02}, //output put foramat
	{0x86 , 0x06}, //02 //sync plority      // ////////   03  
	{0x8b , 0xbc},
	{0xb0 , 0x80}, //globle gain
	{0xc0 , 0x40},//Yuv bypass


	//===============================lsc
	{0xfe , 0x01},
	{0xc2 , 0x38},
	{0xc3 , 0x25},
	{0xc4 , 0x21},
	{0xc8 , 0x19},
	{0xc9 , 0x12},
	{0xca , 0x0e},
	{0xbc , 0x43},
	{0xbd , 0x18},
	{0xbe , 0x1b},
	{0xb6 , 0x40},
	{0xb7 , 0x2e},
	{0xb8 , 0x26},
	{0xc5 , 0x05},
	{0xc6 , 0x03},
	{0xc7 , 0x04},
	{0xcb , 0x00},
	{0xcc , 0x00},
	{0xcd , 0x00},
	{0xbf , 0x14},
	{0xc0 , 0x22},
	{0xc1 , 0x1b},
	{0xb9 , 0x00},
	{0xba , 0x05},
	{0xbb , 0x05},
	{0xaa , 0x35},
	{0xab , 0x33},
	{0xac , 0x33},
	{0xad , 0x25},
	{0xae , 0x22},
	{0xaf , 0x27},
	{0xb0 , 0x1d},
	{0xb1 , 0x20},
	{0xb2 , 0x22},
	{0xb3 , 0x14},
	{0xb4 , 0x15},
	{0xb5 , 0x16},
	{0xd0 , 0x00},
	{0xd2 , 0x07},
	{0xd3 , 0x08},
	{0xd8 , 0x00},
	{0xda , 0x13},
	{0xdb , 0x17},
	{0xdc , 0x00},
	{0xde , 0x0a},
	{0xdf , 0x08},
	{0xd4 , 0x00},
	{0xd6 , 0x00},
	{0xd7 , 0x0c},
	{0xa4 , 0x00},
	{0xa5 , 0x00},
	{0xa6 , 0x00},
	{0xa7 , 0x00},
	{0xa8 , 0x00},
	{0xa9 , 0x00},
	{0xa1 , 0x80},
	{0xa2 , 0x80},

	//=================================cc
	{0xfe , 0x02},
	{0xc0 , 0x01},
	{0xc1 , 0x40}, //Green_cc for d
	{0xc2 , 0xfc},
	{0xc3 , 0x05},
	{0xc4 , 0xec},
	{0xc5 , 0x42},
	{0xc6 , 0xf8},
	{0xc7 , 0x40},//for cwf 
	{0xc8 , 0xf8},
	{0xc9 , 0x06},
	{0xca , 0xfd},
	{0xcb , 0x3e},
	{0xcc , 0xf3},
	{0xcd , 0x36},//for A
	{0xce , 0xf6},
	{0xcf , 0x04},
	{0xe3 , 0x0c},
	{0xe4 , 0x44},
	{0xe5 , 0xe5},
	{0xfe , 0x00},

	//==============================awb
	//AWB clear
	{0xfe , 0x01},
	{0x4f , 0x00},
	{0x4d , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x10}, // 10
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x20}, // 20
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x30},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00}, // 30
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x40}, // 40
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x50}, // 50
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x60}, // 60
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x70}, // 70
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x80}, // 80
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0x90}, // 90
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0xa0}, // a0
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0xb0}, // b0
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0xc0}, // c0
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4d , 0xd0}, // d0
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4e , 0x00},
	{0x4f , 0x01},
	/////// awb value////////
	{0xfe , 0x01},
	{0x4f , 0x00},
	{0x4d , 0x30},
	{0x4e , 0x00},
	{0x4e , 0x80},
	{0x4e , 0x80},
	{0x4e , 0x02},
	{0x4e , 0x02},
	{0x4d , 0x40},
	{0x4e , 0x00},
	{0x4e , 0x80},
	{0x4e , 0x80},
	{0x4e , 0x02},
	{0x4e , 0x02},
	{0x4e , 0x02},
	{0x4d , 0x53},
	{0x4e , 0x08},
	{0x4e , 0x04},
	{0x4d , 0x62},
	{0x4e , 0x10},
	{0x4d , 0x72},
	{0x4e , 0x20},
	{0x4f , 0x01},

	/////awb////
	{0xfe , 0x01},
	{0x50 , 0x88},//c0//[6]green mode
	{0x52 , 0x40},
	{0x54 , 0x60},
	{0x56 , 0x06},
	{0x57 , 0x20}, //pre adjust
	{0x58 , 0x01}, 
	{0x5b , 0x02}, //AWB_gain_delta
	{0x61 , 0xaa},//R/G stand
	{0x62 , 0xaa},//R/G stand
	{0x71 , 0x00},
	{0x74 , 0x10},  //0x//AWB_C_max
	{0x77 , 0x08}, // 0x//AWB_p2_x
	{0x78 , 0xfd}, //AWB_p2_y
	{0x86 , 0x30},
	{0x87 , 0x00},
	{0x88 , 0x04},//06 , 0x//[1]dark mode
	{0x8a , 0xc0},//awb move mode
	{0x89 , 0x75},
	{0x84 , 0x08},  //0x//auto_window
	{0x8b , 0x00}, // 0x//awb compare luma
	{0x8d , 0x70}, //awb gain limit R 
	{0x8e , 0x70},//G
	{0x8f , 0xf4},//B
	{0xfe , 0x00},
	{0x82 , 0x02},//awb_en

	///==========================asde
	{0xfe , 0x01},
	{0x21 , 0xbf},
	{0xfe , 0x02},
	{0xa4 , 0x00},//asde_offset_slope 移动
	{0xa5 , 0x40}, //lsc_th
	{0xa2 , 0xa0}, //lsc_dec_slope
	{0xa6 , 0x80}, //dd_th
	{0xa7 , 0x80}, //ot_th
	{0xab , 0x31}, //[0]b_dn_effect_dark_inc_or_dec
	{0xa9 , 0x6f}, //[7:4] ASDE_DN_b_slope_high
	{0xb0 , 0x99}, //0x//edge effect slope low
	{0xb1 , 0x34},//edge effect slope low
	{0xb3 , 0x80}, //saturation dec slope
	{0xde , 0xb6},  //
	{0x38 , 0x0f}, // 
	{0x39 , 0x60}, //
	{0xfe , 0x00},
	{0x81 , 0x26},

	{0xfe , 0x02},
	{0x83 , 0x00},//[6]green_bks_auto [5]gobal_green_bks
	{0x84 , 0x45},//RB offset
	///=================YCP
	{0xd1 , 0x38},//saturation_cb
	{0xd2 , 0x38},//saturation_Cr
	{0xd3 , 0x40},//contrast 2012.10.26增加
	{0xd4 , 0x80},//contrast center 2012.10.26增加
	{0xd5 , 0x00},//luma_offset 2012.10.26增加
	{0xdc , 0x30},
	{0xdd , 0xb8},//edge_sa_g,b
	{0xfe , 0x00},
	////=================dndd
	{0xfe , 0x02},
	{0x88 , 0x15},//dn_b_base
	{0x8c , 0xf6}, //[2]b_in_dark_inc
	{0x89 , 0x03}, //dn_c_weight
	////==================EE 2012.10.26
	{0xfe , 0x02},
	{0x90 , 0x6c},// EEINTP mode1
	{0x97 , 0x48},// edge effect
	////==============RGB Gamma 
	{0xfe , 0x02},
	{0x15 , 0x0a},
	{0x16 , 0x12},
	{0x17 , 0x19},
	{0x18 , 0x1f},
	{0x19 , 0x2c},
	{0x1a , 0x38},
	{0x1b , 0x42},
	{0x1c , 0x4e},
	{0x1d , 0x63},
	{0x1e , 0x76},
	{0x1f , 0x87},
	{0x20 , 0x96},
	{0x21 , 0xa2},
	{0x22 , 0xb8},
	{0x23 , 0xca},
	{0x24 , 0xd8},
	{0x25 , 0xe3},
	{0x26 , 0xf0},
	{0x27 , 0xf8},
	{0x28 , 0xfd},
	{0x29 , 0xff},

	////RGB Gamma 
	//{0xfe , 0x02},
	//{0x15 , 0x0b},
	//{0x16 , 0x0e},
	//{0x17 , 0x10},
	//{0x18 , 0x12},
	//{0x19 , 0x19},
	//{0x1a , 0x21},
	//{0x1b , 0x29},
	//{0x1c , 0x31},
	//{0x1d , 0x41},
	//{0x1e , 0x50},
	//{0x1f , 0x5f},
	//{0x20 , 0x6d},
	//{0x21 , 0x79},
	//{0x22 , 0x91},
	//{0x23 , 0xa5},
	//{0x24 , 0xb9},
	//{0x25 , 0xc9},
	//{0x26 , 0xe1},
	//{0x27 , 0xee},
	//{0x28 , 0xf7},
	//{0x29 , 0xff},

	///=================y gamma
	{0xfe , 0x02},
	{0x2b , 0x00},
	{0x2c , 0x04},
	{0x2d , 0x09},
	{0x2e , 0x18},
	{0x2f , 0x27},
	{0x30 , 0x37},
	{0x31 , 0x49},
	{0x32 , 0x5c},
	{0x33 , 0x7e},
	{0x34 , 0xa0},
	{0x35 , 0xc0},
	{0x36 , 0xe0},
	{0x37 , 0xff},

	////dark sun/////
	//{0xfe , 0x02},
	//{0x40 , 0x06},
	//{0x41 , 0x23},
	//{0x42 , 0x3f},
	//{0x43 , 0x06},
	//{0x44 , 0x00},//[7]enable 2012.10.26
	//{0x45 , 0x00},
	//{0x46 , 0x14},
	//{0x47 , 0x09},

	/////1600x1200size// 
	{0xfe , 0x00},//
	{0x90 , 0x01}, //0x//crop enable
	{0x95 , 0x04},  //0x//1600x1200
	{0x96 , 0xb0},
	{0x97 , 0x06},
	{0x98 , 0x40},

	{0xfe , 0x03},
	{0x42 , 0x40}, 
	{0x43 , 0x06}, //output buf width 800x2
	{0x41 , 0x02}, // Pclk_polarity
	{0x40 , 0x40},  //00  
	{0x17 , 0x00}, //widv 
	{0xfe , 0x00},

	////output DVP/////
	
	{0xfe , 0x00},
	{0x82 , 0xfe},
	{0xf2 , 0x70}, 
	{0xf3 , 0xff},
	{0xf4 , 0x00},
	{0xf5 , 0x30},

	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_fmt_yuv422[] = {
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_win_vga[] = {
	
	{0xfe , 0x00},
	{0xb6 , 0x03},
	{0xfa , 0x00}, 
	{0xc8,  0x00},//close scaler
	{0x99,  0x22},// 1/2 subsample
	{0x9a ,  0x06},
	{0x9b , 0x00},
	{0x9c , 0x00},
	{0x9d , 0x00},
	{0x9e , 0x00},
	{0x9f , 0x00},
	{0xa0 , 0x00},  
	{0xa1 , 0x00},
	{0xa2  ,0x00},

	
	{0x90 , 0x01},  //crop enable
	{0x95 , 0x01},
	{0x96 , 0xe0},
	{0x97 , 0x02},
	{0x98 , 0x88},
	
	{GC2035_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list gc2035_win_SXGA[] = {
	//crop window
	{0xfe , 0x00},
	 {0xfa , 0x11}, 
       {0xc8 , 0x00}, //Scaler_disable
       
	{0x99 , 0x55},  // 4/5
	{0x9a , 0x06},
	{0x9b , 0x00},
	{0x9c , 0x00},
	{0x9d , 0x01},
	{0x9e , 0x23},
	{0x9f , 0x00},
	{0xa0 , 0x00},  
	{0xa1 , 0x01},
	{0xa2  ,0x23},
	
	{0x90 , 0x01},  //crop enable
	{0x95 , 0x03},
	{0x96 , 0xc0},
	{0x97 , 0x05},
	{0x98 , 0x00},

	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_win_2M[] = {
	//crop window
	{0xfe , 0x00},
	{0xfa , 0x11}, 
	{0xc8 , 0x00},
	{0x99 , 0x11}, // disable sambsample
	{0x9a , 0x06},
	{0x9b , 0x00},
	{0x9c , 0x00},
	{0x9d , 0x00},
	{0x9e , 0x00},
	{0x9f , 0x00},
	{0xa0 , 0x00},  
	{0xa1 , 0x00},
	{0xa2  ,0x00},
	
	{0x90 , 0x01},
	{0x95 , 0x04},
	{0x96 , 0xb0},
	{0x97 , 0x06},
	{0x98 , 0x40},

	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_stream_on[] = {
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_stream_off[] = {
	{GC2035_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list gc2035_brightness_l3[] = {
	{0xfe , 0x02},{0xd5 , 0xc0},{0xfe , 0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_brightness_l2[] = {
	{0xfe , 0x02},{0xd5 , 0xe0},{0xfe , 0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_brightness_l1[] = {
	{0xfe , 0x02},{0xd5 , 0xf0},{0xfe , 0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_brightness_h0[] = {
	{0xfe , 0x02},{0xd5 , 0x00},{0xfe , 0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_brightness_h1[] = {
	{0xfe , 0x02},{0xd5 , 0x10},{0xfe , 0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_brightness_h2[] = {
	{0xfe , 0x02},{0xd5 , 0x20},{0xfe , 0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_brightness_h3[] = {
	{0xfe , 0x02},{0xd5 , 0x30},{0xfe , 0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_contrast_l3[] = {
	{0xfe,0x02}, {0xd3,0x30}, {0xfe,0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_contrast_l2[] = {
	{0xfe,0x02}, {0xd3,0x34}, {0xfe,0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_contrast_l1[] = {
	{0xfe,0x02}, {0xd3,0x38}, {0xfe,0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_contrast_h0[] = {
	{0xfe,0x02}, {0xd3,0x40}, {0xfe,0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_contrast_h1[] = {
	{0xfe,0x02}, {0xd3,0x44}, {0xfe,0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_contrast_h2[] = {
	{0xfe,0x02}, {0xd3,0x48}, {0xfe,0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2035_contrast_h3[] = {
	{0xfe,0x02}, {0xd3,0x50}, {0xfe,0x00},
	{GC2035_REG_END, 0x00},	/* END MARKER */
};

static int gc2035_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int gc2035_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc2035_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != GC2035_REG_END) {
		int ret = gc2035_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int gc2035_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int gc2035_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc2035_info *info = container_of(sd, struct gc2035_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return gc2035_write_array(sd, gc2035_init_regs);
}

static int gc2035_shutter(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2 ;
	unsigned short v;
	
	gc2035_write(sd, 0xfe,0x00);
	gc2035_write(sd, 0xb6,0x00);  
	ret = gc2035_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;
	ret = gc2035_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret; 
		
	v =  (v1<<8) | v2	 ;
		 
	v = v / 2;
	gc2035_write(sd, 0x03, (v >> 8)&0xff ); /* Shutter */
  	gc2035_write(sd, 0x04, v&0xff);
       msleep(400);

	return  0;
}

static int gc2035_detect(struct v4l2_subdev *sd)
{
	unsigned char v=0x0;
	int ret;

	ret = gc2035_read(sd, 0xf0, &v);
	printk("GC2035_CHIP_ID_H = %x./n",v);
	if (ret < 0)
		return ret;
	if (v != GC2035_CHIP_ID_H)
		return -ENODEV;
	ret = gc2035_read(sd, 0xf1, &v);
	printk("GC2035_CHIP_ID_L = %x./n",v);
	if (ret < 0)
		return ret;
	if (v != GC2035_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct gc2035_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc2035_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= gc2035_fmt_yuv422,
	},
};
#define N_GC2035_FMTS ARRAY_SIZE(gc2035_formats)

static struct gc2035_win_size {
	int	width;
	int	height;
	struct regval_list *regs;
} gc2035_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= gc2035_win_vga,
	},
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.regs 		= gc2035_win_SXGA,
	},
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.regs 		= gc2035_win_2M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(gc2035_win_sizes))

static int gc2035_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC2035_FMTS)
		return -EINVAL;

	*code = gc2035_formats[index].mbus_code;
	return 0;
}

static int gc2035_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc2035_format_struct **ret_fmt,
		struct gc2035_win_size **ret_wsize)
{
	int index;
	struct gc2035_win_size *wsize;

	for (index = 0; index < N_GC2035_FMTS; index++)
		if (gc2035_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC2035_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc2035_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc2035_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = gc2035_win_sizes; wsize < gc2035_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc2035_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc2035_formats[index].colorspace;
	return 0;
}

static int gc2035_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc2035_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc2035_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc2035_info *info = container_of(sd, struct gc2035_info, sd);
	struct gc2035_format_struct *fmt_s;
	struct gc2035_win_size *wsize;
	int ret;

	ret = gc2035_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc2035_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = gc2035_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}

	if (((wsize->width == MAX_WIDTH)&& (wsize->height == MAX_HEIGHT)) ||
		((wsize->width == SXGA_WIDTH)&& (wsize->height == SXGA_HEIGHT)))

		{
			ret = gc2035_shutter(sd);
		}
	
	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}
static int gc2035_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = gc2035_write_array(sd, gc2035_stream_on);
	else
		ret = gc2035_write_array(sd, gc2035_stream_off);

	return ret;
}

static int gc2035_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc2035_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc2035_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2035_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2035_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc2035_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc2035_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc2035_frame_rates[interval->index];
	return 0;
}

static int gc2035_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc2035_win_size *win = &gc2035_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc2035_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc2035_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[GC2035]set brightness %d\n", brightness);

	switch (brightness) {
	case BRIGHTNESS_L3:
		gc2035_write_array(sd, gc2035_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		gc2035_write_array(sd, gc2035_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		gc2035_write_array(sd, gc2035_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		gc2035_write_array(sd, gc2035_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		gc2035_write_array(sd, gc2035_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		gc2035_write_array(sd, gc2035_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		gc2035_write_array(sd, gc2035_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int gc2035_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk(KERN_DEBUG "[GC2035]set contrast %d\n", contrast);

	switch (contrast) {
	case CONTRAST_L3:
		gc2035_write_array(sd, gc2035_contrast_l3);
		break;
	case CONTRAST_L2:
		gc2035_write_array(sd, gc2035_contrast_l2);
		break;
	case CONTRAST_L1:
		gc2035_write_array(sd, gc2035_contrast_l1);
		break;
	case CONTRAST_H0:
		gc2035_write_array(sd, gc2035_contrast_h0);
		break;
	case CONTRAST_H1:
		gc2035_write_array(sd, gc2035_contrast_h1);
		break;
	case CONTRAST_H2:
		gc2035_write_array(sd, gc2035_contrast_h2);
		break;
	case CONTRAST_H3:
		gc2035_write_array(sd, gc2035_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int gc2035_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2035_info *info = container_of(sd, struct gc2035_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&gc2035_isp_setting;
		isp_setting.size = ARRAY_SIZE(gc2035_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&gc2035_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int gc2035_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2035_info *info = container_of(sd, struct gc2035_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = gc2035_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = gc2035_set_contrast(sd, ctrl->value);
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

static int gc2035_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC2035, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc2035_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc2035_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int gc2035_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2035_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc2035_core_ops = {
	.g_chip_ident = gc2035_g_chip_ident,
	.g_ctrl = gc2035_g_ctrl,
	.s_ctrl = gc2035_s_ctrl,
	.queryctrl = gc2035_queryctrl,
	.reset = gc2035_reset,
	.init = gc2035_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc2035_g_register,
	.s_register = gc2035_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc2035_video_ops = {
	.enum_mbus_fmt = gc2035_enum_mbus_fmt,
	.try_mbus_fmt = gc2035_try_mbus_fmt,
	.s_mbus_fmt = gc2035_s_mbus_fmt,
	.s_stream = gc2035_s_stream,
	.cropcap = gc2035_cropcap,
	.g_crop	= gc2035_g_crop,
	.s_parm = gc2035_s_parm,
	.g_parm = gc2035_g_parm,
	.enum_frameintervals = gc2035_enum_frameintervals,
	.enum_framesizes = gc2035_enum_framesizes,
};

static const struct v4l2_subdev_ops gc2035_ops = {
	.core = &gc2035_core_ops,
	.video = &gc2035_video_ops,
};

static int gc2035_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc2035_info *info;
	int ret;

	info = kzalloc(sizeof(struct gc2035_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2035_ops);

	/* Make sure it's an gc2035 */
	ret = gc2035_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc2035 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc2035 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int gc2035_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2035_info *info = container_of(sd, struct gc2035_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc2035_id[] = {
	{ "gc2035", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2035_id);

static struct i2c_driver gc2035_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2035",
	},
	.probe		= gc2035_probe,
	.remove		= gc2035_remove,
	.id_table	= gc2035_id,
};

static __init int init_gc2035(void)
{
	return i2c_add_driver(&gc2035_driver);
}

static __exit void exit_gc2035(void)
{
	i2c_del_driver(&gc2035_driver);
}

fs_initcall(init_gc2035);
module_exit(exit_gc2035);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore gc2035 sensors");
MODULE_LICENSE("GPL");
