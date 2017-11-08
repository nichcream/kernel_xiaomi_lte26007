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
#include "sp2529_26M.h"

#define SP2529_MIPI_CHIP_ID_H	(0x25)
#define SP2529_MIPI_CHIP_ID_L	(0x29)
#define VGA_WIDTH		640
#define VGA_HEIGHT		480
#define SVGA_WIDTH		1280
#define SVGA_HEIGHT		960
#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH		MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT
#define V4L2_I2C_ADDR_8BIT	(0x0001)
#define SP2529_MIPI_REG_END		0xff
#define ISO_MAX_GAIN      0x50
#define ISO_MIN_GAIN      0x20
//heq
#define SP2529_P1_0x10		0x88		 //ku_outdoor
#define SP2529_P1_0x11		0x88		//ku_nr
#define SP2529_P1_0x12		0x80		 //ku_dummy
#define SP2529_P1_0x13		0x80		 //ku_low
#define SP2529_P1_0x14		0xa0		//c4 //kl_outdoor
#define SP2529_P1_0x15		0xa0		//c4 //kl_nr
#define SP2529_P1_0x16		0x88		//c4 //kl_dummy
#define SP2529_P1_0x17		0x88		//c4 //kl_low

//sharpness
#define SP2529_P2_0xe8     0x28//0x10//10//;sharp_fac_pos_outdoor
#define SP2529_P2_0xec     0x30//0x20//20//;sharp_fac_neg_outdoor
#define SP2529_P2_0xe9     0x28//0x0a//0a//;sharp_fac_pos_nr
#define SP2529_P2_0xed     0x30//0x20//20//;sharp_fac_neg_nr
#define SP2529_P2_0xea     0x20//0x08//08//;sharp_fac_pos_dummy
#define SP2529_P2_0xee     0x28//0x18//18//;sharp_fac_neg_dummy
#define SP2529_P2_0xeb     0x18//0x08//08//;sharp_fac_pos_low
#define SP2529_P2_0xef     0x20//0x08//18//;sharp_fac_neg_low

//saturation
#define SP2529_P1_0xd3		0x98
#define SP2529_P1_0xd4		0x98
#define SP2529_P1_0xd5  	0x90
#define SP2529_P1_0xd6  	0x60
#define SP2529_P1_0xd7		0x98
#define SP2529_P1_0xd8		0x98
#define SP2529_P1_0xd9   	0x90
#define SP2529_P1_0xda   	0x60

//ae target
#define SP2529_P1_0xeb		0x78 //indor
#define SP2529_P1_0xec		0x78//outdor

struct sp2529_mipi_format_struct;

struct sp2529_mipi_info {
	struct v4l2_subdev sd;
	struct sp2529_mipi_format_struct *fmt;
	struct sp2529_mipi_win_size *win;
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


static struct regval_list sp2529_mipi_init_regs[] = {
	//uxga
	{0xfd,0x00},
	{0xfd,0x00},
	{0xfd,0x00},
	{0xfd,0x00},
	{0xfd,0x00},
	/////////////////////////////////////////////////
	//**********前面的0xfd 绝对不可以去掉**********//
	/////////////////////////////////////////////////
	{0xfd,0x01},
	{0x36,0x02},
	{0xfd,0x00},
	{0xe7,0x03},
	{0xe7,0x00},

	{0x19,0x00},
	{0x30,0x00},//00  
	{0x31,0x00},
	{0x33,0x00},

	{0x95,0x06},
	{0x94,0x40},
	{0x97,0x04},
	{0x96,0xb0},

	{0x98,0x3a},//luo-1216  0x09   

	{0x1d,0x01},
	{0x1b,0x37},
	{0x0c,0x66},
	{0x27,0xa5},
	{0x28,0x08},
	{0x1e,0x13},
	{0x21,0x10},
	{0x15,0x16},
	{0x71,0x18},
	{0x7c,0x18},
	{0x76,0x19},
	{0xfd,0x02},
	{0x85,0x00},
	{0xfd,0x00},
	{0x11,0x40},
	{0x18,0x00},
	{0x1a,0x49},
	{0x1e,0x01},
	{0x0c,0x55},
	{0x21,0x10},
	{0x22,0x2a},
	{0x25,0xad},
	{0x27,0xa1},
	{0x1f,0xc0},
	{0x28,0x0b},
	{0x2b,0x8c},
	{0x26,0x09},
	{0x2c,0x45},
	{0x37,0x00},
	{0x16,0x01},
	{0x17,0x2f},
	{0x69,0x01},
	{0x6a,0x2d},
	{0x13,0x4f},
	{0x6b,0x50},
	{0x6c,0x50},
	{0x6f,0x50},
	{0x73,0x51},
	{0x7a,0x41},
	{0x70,0x41},
	{0x7d,0x40},
	{0x74,0x40},
	{0x75,0x40},
	{0x14,0x01},
	{0x15,0x20},
	{0x71,0x22},
	{0x76,0x22},
	{0x7c,0x22},
	{0x7e,0x21},
	{0x72,0x21},
	{0x77,0x20},
	{0xfd,0x00},
	{0xfd,0x01},
	{0xf2,0x09},
	{0x32,0x00},
	{0xfb,0x25},
	{0xfd,0x02},
	{0x85,0x00},
	{0x00,0x82},
	{0x01,0x82},

	{0xfd,0x00},
	{0x2e,0x85},
	{0x1e,0x01},
	{0xfd,0x00},
#if 0
	//19.5M  2PLL  7fps
	{0x2f,0x04},//3PLL 花屏
	{0x03,0x02},
	{0x04,0x0a},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x00},
	{0x0a,0xc7},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0x57},
	{0xf8,0x49},
	{0x02,0x0e},
	{0x03,0x01},
	{0x06,0x57},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x11},
	{0x3e,0x49},
	{0x3f,0x00},
	{0x88,0xe2},
	{0x89,0x03},
	{0x8a,0x75},
	{0xfd,0x02},
	{0xbe,0xc2},
	{0xbf,0x04},
	{0xd0,0xc2},
	{0xd1,0x04},
	{0xc9,0xc2},
	{0xca,0x04},
#else
	//26M  2PLL  9fps 50hz uxga
	{0xfd,0x00},
	{0x2f,0x04},//3PLL 花屏
	{0x03,0x02},
	{0x04,0xa0},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x00},
	{0x0a,0xef},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0x70},
	{0xf8,0x5d},
	{0x02,0x0b},
	{0x03,0x01},
	{0x06,0x70},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x0d},
	{0x3e,0x5d},
	{0x3f,0x00},
	{0x88,0x92},
	{0x89,0x81},
	{0x8a,0x54},
	{0xfd,0x02},
	{0xbe,0xd0},
	{0xbf,0x04},
	{0xd0,0xd0},
	{0xd1,0x04},
	{0xc9,0xd0},
	{0xca,0x04},

#endif
	{0xf2,0x09},

	{0xb8,0x70},
	{0xb9,0x90},
	{0xba,0x30},
	{0xbb,0x45},
	{0xbc,0x90},
	{0xbd,0x70},
	{0xfd,0x03},
	{0x77,0x70},   
	//rpc
	{0xfd,0x01},
	{0xe0,0x48},
	{0xe1,0x38},
	{0xe2,0x30},
	{0xe3,0x2c},
	{0xe4,0x2c},
	{0xe5,0x2a},
	{0xe6,0x2a},
	{0xe7,0x28},
	{0xe8,0x28},
	{0xe9,0x28},
	{0xea,0x26},
	{0xf3,0x26},
	{0xf4,0x26},
	{0xfd,0x01},
	{0x04,0xa0},
	{0x05,0x26},
	{0x0a,0x48},
	{0x0b,0x26},

	{0xfd,0x01},
	{0xeb,0x78},
	{0xec,0x78},
	{0xed,0x05},
	{0xee,0x0a},
	//去坏像数
	{0xfd,0x03},
	{0x52,0xff},
	{0x53,0x60},
	{0x94,0x00},
	{0x54,0x00},
	{0x55,0x00},
	{0x56,0x80},
	{0x57,0x80},
	{0x95,0x00},
	{0x58,0x00},
	{0x59,0x00},
	{0x5a,0xf6},
	{0x5b,0x00},
	{0x5c,0x88},
	{0x5d,0x00},
	{0x96,0x00},
	{0xfd,0x03},
	{0x8a,0x00},
	{0x8b,0x00},
	{0x8c,0xff},
	{0x22,0xff},
	{0x23,0xff},
	{0x24,0xff},
	{0x25,0xff},
	{0x5e,0xff},
	{0x5f,0xff},
	{0x60,0xff},
	{0x61,0xff},
	{0x62,0x00},
	{0x63,0x00},
	{0x64,0x00},
	{0x65,0x00},
	//lsc
	{0xfd,0x01},
	{0x21,0x00},
	{0x22,0x00},
	{0x26,0x60},  //lsc_gain_thr                      
	{0x27,0x14},  //lsc_exp_thrl                      
	{0x28,0x05},  //lsc_exp_thrh                      
	{0x29,0x00},  //lsc_dec_fac     进dummy态退shading 功能有问题，需关掉                  
	{0x2a,0x01},  //lsc_rpc_en lens 衰减自适应        
	//LSC for CHT813                                            
	{0xfd,0x01},
	{0xa1,0x33},
	{0xa2,0x2b},
	{0xa3,0x28},
	{0xa4,0x20},
	{0xa5,0x2d},
	{0xa6,0x26},
	{0xa7,0x1e},
	{0xa8,0x1a},
	{0xa9,0x27},
	{0xaa,0x20},
	{0xab,0x20},
	{0xac,0x13},
	{0xad,0x03},
	{0xae,0x03},
	{0xaf,0x03},
	{0xb0,0x03},
	{0xb1,0x03},
	{0xb2,0x03},
	{0xb3,0x03},
	{0xb4,0x03},
	{0xb5,0x03},
	{0xb6,0x03},
	{0xb7,0x03},
	{0xb8,0x03},
	//awb
	{0xfd,0x02},
	{0x26,0xa0},  //Red channel gain
	{0x27,0x96},  //Blue channel gain
	{0x28,0xcc},  //Y top value limit
	{0x29,0x01},  //Y bot value limit
	{0x2a,0x02},
	{0x2b,0x08},
	{0x2c,0x20},  //Awb image center row start
	{0x2d,0xdc},  //Awb image center row end
	{0x2e,0x20},  //Awb image center col start
	{0x2f,0x96},  //Awb image center col end
	{0x1b,0x80},  //b,g mult a constant for detect white pixel
	{0x1a,0x80},  //r,g mult a constant for detect white pixel
	{0x18,0x16},  //wb_fine_gain_step,wb_rough_gain_step
	{0x19,0x26},  //wb_dif_fine_th, wb_dif_rough_th
	{0x1d,0x04},  //skin detect u bot
	{0x1f,0x06},  //skin detect v bot
	//d65 10
	{0x66,0x2c},
	{0x67,0x54},
	{0x68,0xbc},
	{0x69,0xe4},
	{0x6a,0xa5},
	//indoor                          
	{0x7c,0x10},
	{0x7d,0x40},
	{0x7e,0xd8},
	{0x7f,0x08},
	{0x80,0xa6},
	//cwf   12                         
	{0x70,0x11},
	{0x71,0x3e},
	{0x72,0x0e},
	{0x73,0x32},
	{0x74,0xaa},
	//tl84                              
	{0x6b,0x08},
	{0x6c,0x1d},
	{0x6d,0x0a},
	{0x6e,0x20},
	{0x6f,0xaa},
	//f                                   
	{0x61,0xe7},
	{0x62,0x14},
	{0x63,0x30},
	{0x64,0x5a},
	{0x65,0x6a},
	                  
	{0x75,0x00},                                             
	{0x76,0x09},                                             
	{0x77,0x02},                                             
	{0x0e,0x16},                                             
	{0x3b,0x09},                                               
	{0xfd,0x02},                                             
	{0x02,0x00},                                               
	{0x03,0x10},                                               
	{0x04,0xf0},                                               
	{0xf5,0xb3},                                               
	{0xf6,0x80}, //outdoor rgain b
	{0xf7,0xe0},                                               
	{0xf8,0x89},
	{0xfd,0x02},
	{0x08,0x00},
	{0x09,0x04},                        
	{0xfd,0x02},
	{0xdd,0x0f},
	{0xde,0x0f},                        

	{0xfd,0x02}, // sharp
	{0x57,0x30}, //raw_sharp_y_base
	{0x58,0x10}, //raw_sharp_y_min
	{0x59,0xe0}, //raw_sharp_y_max
	{0x5a,0x00}, //raw_sharp_rangek_neg
	{0x5b,0x12}, //raw_sharp_rangek_pos 

	{0xcb,0x06},
	{0xcc,0x08},
	{0xcd,0x10}, //raw_sharp_range_base_dummy
	{0xce,0x1a}, //raw_sharp_range_base_low

	{0xfd,0x03},
	{0x87,0x04}, //raw_sharp_range_ofst1	4
	{0x88,0x08}, //raw_sharp_range_ofst2	8x
	{0x89,0x10}, //raw_sharp_range_ofst3	16x


	{0xfd,0x02},
	{0xe8,0x28},
	{0xec,0x30},
	{0xe9,0x28},
	{0xed,0x30},
	{0xea,0x20}, //sharpness gain for increasing pixel’s Y,in dummy  
	{0xee,0x28}, //sharpness gain for decreasing pixel’s Y, in dummy  
	{0xeb,0x18}, //sharpness gain for increasing pixel’s Y,in lowlight  
	{0xef,0x20}, //sharpness gain for decreasing pixel’s Y, in low light  

	{0xfd,0x02}, //skin sharpen
	{0xdc,0x04}, //skin_sharp_sel肤色降锐化          
	{0x05,0x6f}, //skin_num_th2排除肤色降锐化对分辨率卡引起的干扰 

	//平滑自适应          
	{0xfd,0x02},
	{0xf4,0x30},  //raw_ymin
	{0xfd,0x03},
	{0x97,0x98},  //raw_ymax_outdoor
	{0x98,0x88},  //raw_ymax_normal
	{0x99,0x88},  //raw_ymax_dummy
	{0x9a,0x80},  //raw_ymax_low
	{0xfd,0x02},
	{0xe4,0xff},  //raw_yk_fac_outdoor
	{0xe5,0xff},  //raw_yk_fac_normal
	{0xe6,0xff},  //raw_yk_fac_dummy
	{0xe7,0xff},  //raw_yk_fac_low

	{0xfd,0x03},
	{0x72,0x18},  //raw_lsc_fac_outdoor
	{0x73,0x28},  //raw_lsc_fac_normal
	{0x74,0x28},  //raw_lsc_fac_dummy
	{0x75,0x30},  //raw_lsc_fac_low

	//四个通道内阈值              
	{0xfd,0x02},
	{0x78,0x20},
	{0x79,0x20},
	{0x7a,0x14},
	{0x7b,0x08},

	{0x81,0x02},//raw_grgb_thr_outdoor
	{0x82,0x20},
	{0x83,0x20},
	{0x84,0x08},

	{0xfd,0x03},
	{0x7e,0x06}, //raw_noise_base_outdoor
	{0x7f,0x0d}, //raw_noise_base_normal
	{0x80,0x10}, //raw_noise_base_dummy
	{0x81,0x16}, //raw_noise_base_low
	{0x7c,0xff}, //raw_noise_base_dark
	{0x82,0x54}, //raw_dns_fac_outdoor,raw_dns_fac_normal}
	{0x83,0x43}, //raw_dns_fac_dummy,raw_dns_fac_low}
	{0x84,0x00},  //raw_noise_ofst1 	4x
	{0x85,0x20},  //raw_noise_ofst2	8x
	{0x86,0x40}, //raw_noise_ofst3	16x

	//去紫边功能          
	{0xfd,0x03},
	{0x66,0x18}, //pf_bg_thr_normal b-g>thr
	{0x67,0x28}, //pf_rg_thr_normal r-g<thr
	{0x68,0x20}, //pf_delta_thr_normal |val|>thr
	{0x69,0x88}, //pf_k_fac val/16
	{0x9b,0x18}, //pf_bg_thr_outdoor
	{0x9c,0x28}, //pf_rg_thr_outdoor
	{0x9d,0x20}, //pf_delta_thr_outdoor

	//Gamma
	{0xfd,0x01},
	{0x8b,0x00},
	{0x8c,0x0f},
	{0x8d,0x21},
	{0x8e,0x2c},
	{0x8f,0x37},
	{0x90,0x46},
	{0x91,0x53},
	{0x92,0x5e},
	{0x93,0x6a},
	{0x94,0x7d},
	{0x95,0x8d},
	{0x96,0x9e},
	{0x97,0xac},
	{0x98,0xba},
	{0x99,0xc6},
	{0x9a,0xd1},
	{0x9b,0xda},
	{0x9c,0xe4},
	{0x9d,0xeb},
	{0x9e,0xf2},
	{0x9f,0xf9},
	{0xa0,0xff}, 

	//CCM
	{0xfd,0x02}, 
	{0x15,0xb0},
	{0x16,0x82},
	//!F 
	{0xa0,0x95},
	{0xa1,0x0c},
	{0xa2,0xde},
	{0xa3,0x0e},
	{0xa4,0x77},
	{0xa5,0xfa},
	{0xa6,0xe7},
	{0xa7,0xed},
	{0xa8,0xac},
	{0xa9,0x30},
	{0xaa,0x30},
	{0xab,0x0f},
	//F 
	{0xac,0x80},
	{0xad,0x26},
	{0xae,0xda},
	{0xaf,0x1a},
	{0xb0,0x8b},
	{0xb1,0xda},
	{0xb2,0xda},
	{0xb3,0x00},
	{0xb4,0xa6},
	{0xb5,0x30},
	{0xb6,0x30},
	{0xb7,0x03},

	{0xfd,0x01},  //auto_sat
	{0xd2,0x2d},  //autosat_en[0]
	{0xd1,0x38},  //lum thr in green enhance
	{0xdd,0x3f},
	{0xde,0x37},

	//auto sat
	{0xfd,0x02},
	{0xc1,0x40},
	{0xc2,0x40},
	{0xc3,0x40},
	{0xc4,0x40},
	{0xc5,0x80},
	{0xc6,0x60},
	{0xc7,0x00},
	{0xc8,0x00},

	//sat u
	{0xfd,0x01},
	{0xd3,0x98},
	{0xd4,0x98},
	{0xd5,0x90},
	{0xd6,0x60},
	//sat v
	{0xd7,0x98},
	{0xd8,0x98},
	{0xd9,0x90},
	{0xda,0x60},

	{0xfd,0x03},
	{0x76,0x0a},
	{0x7a,0x40},
	{0x7b,0x40},
	//auto_sat
	{0xfd,0x01},
	{0xc2,0xaa},  //u_v_th_outdoor白色物体表面有彩色噪声降低此值    
	{0xc3,0xaa},  //u_v_th_nr
	{0xc4,0x66},  //u_v_th_dummy 
	{0xc5,0x66},  //u_v_th_low

	//low_lum_offset
	{0xfd,0x01},
	{0xcd,0x08},
	{0xce,0x18},
	//gw
	{0xfd,0x02},
	{0x32,0x60},
	{0x35,0x60}, //uv_fix_dat
	{0x37,0x13},

	//heq
	{0xfd,0x01},
	{0xdb,0x00}, //buf_heq_offset
	{0x10,0x88}, //ku_outdoor
	{0x11,0x88}, //ku_nr
	{0x12,0x80},
	{0x13,0x80},
	{0x14,0xa0},
	{0x15,0xa0},
	{0x16,0x88},
	{0x17,0x88}, //kl_low

	{0xfd,0x03},
	{0x00,0x80}, //ctf_heq_mean
	{0x03,0x68}, //ctf_range_thr   可以排除灰板场景的阈值   
	{0x06,0xd8}, //ctf_reg_max
	{0x07,0x28}, //ctf_reg_min
	{0x0a,0xfd}, //ctf_lum_ofst
	{0x01,0x16}, //ctf_posk_fac_outdoor
	{0x02,0x16}, //ctf_posk_fac_nr
	{0x04,0x16}, //ctf_posk_fac_dummy
	{0x05,0x16}, //ctf_posk_fac_low
	{0x0b,0x40}, //ctf_negk_fac_outdoor
	{0x0c,0x40}, //ctf_negk_fac_nr
	{0x0d,0x40}, //ctf_negk_fac_dummy
	{0x0e,0x40}, //ctf_negk_fac_low
	{0x08,0x0c},
	{0x09,0x0c},

	{0xfd,0x02}, //cnr
	{0x8e,0x0a}, //cnr_grad_thr_dummy
	{0x90,0x40}, //20 //cnr_thr_outdoor
	{0x91,0x40}, //20 //cnr_thr_nr
	{0x92,0x60}, //60 //cnr_thr_dummy
	{0x93,0x80}, //80 //cnr_thr_low
	{0x9e,0x44},
	{0x9f,0x44},
	{0xfd,0x02},
	{0x85,0x00},
	{0xfd,0x01},
	{0x00,0x00},
	{0x32,0x15},
	{0x33,0xEf},
	{0x34,0xef},
	{0x35,0x40},
	{0xfd,0x00},
	{0x3f,0x03},                       
	{0xfd,0x01},
	{0x50,0x00},                                             
	{0x66,0x00},
	{0xfd,0x02},
	{0xd6,0x0f},

	//uxga
	{0xfd,0x00},
	{0x19,0x00},
	{0x30,0x00},//00  
	{0x31,0x00},
	{0x33,0x00},
	//MIPI 
	{0x95,0x06},
	{0x94,0x40},
	{0x97,0x04},
	{0x96,0xb0},
#if 0
	{0xfd,0x00},
	{0x2f,0x04},//3PLL 花屏
	{0x03,0x02},
	{0x04,0x0a},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x00},
	{0x0a,0xc7},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0x57},
	{0xf8,0x49},
	{0x02,0x0e},
	{0x03,0x01},
	{0x06,0x57},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x11},
	{0x3e,0x49},
	{0x3f,0x00},
	{0x88,0xe2},
	{0x89,0x03},
	{0x8a,0x75},
	{0xfd,0x02},
	{0xbe,0xc2},
	{0xbf,0x04},
	{0xd0,0xc2},
	{0xd1,0x04},
	{0xc9,0xc2},
	{0xca,0x04},
#else
	//26M  2PLL  9fps 50hz uxga
	{0xfd,0x00},
	{0x2f,0x04},//3PLL 花屏
	{0x03,0x02},
	{0x04,0xa0},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x00},
	{0x0a,0xef},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0x70},
	{0xf8,0x5d},
	{0x02,0x0b},
	{0x03,0x01},
	{0x06,0x70},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x0d},
	{0x3e,0x5d},
	{0x3f,0x00},
	{0x88,0x92},
	{0x89,0x81},
	{0x8a,0x54},
	{0xfd,0x02},
	{0xbe,0xd0},
	{0xbf,0x04},
	{0xd0,0xd0},
	{0xd1,0x04},
	{0xc9,0xd0},
	{0xca,0x04},
#endif
	{0xfd,0x01},
	{0x36,0x00},
	{0xfd, 0x00},
	{0x92, 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp2529_mipi_stream_on[] = {
	{0xfd, 0x00},
	{0x92, 0x81},
	{0xfd, 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_stream_off[] = {
	/* Sensor enter LP11*/
	{0xfd, 0x00},
	{0x92, 0x00},
	{0xfd, 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_win_vga[] = {
	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_win_svga[] = {
	//bininig svga
	{0xfd,0x00},
	{0x19,0x03},
	{0x30,0x00},//01 00
	{0x31,0x04},
	{0x33,0x01},
	{0x3f,0x00},   //mirror/flip	//test-lu
	//MIPI 
	{0x95,0x03},
	{0x94,0x20},
	{0x97,0x02},
	{0x96,0x58},

	//FPS2
#if 0
	//sp2529 19.5M 2倍频  20-20fps ABinning+DBinning AE_Parameters_20130925170449
	{0xfd,0x00},
	{0x2f,0x04},//04
	{0xfd,0x00},
	{0x03,0x05},
	{0x04,0xd0},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x01},
	{0x0a,0x0d},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0xf8},
	{0xf8,0xcf},
	{0x02,0x05},
	{0x03,0x01},
	{0x06,0xf8},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x06},
	{0x3e,0xcf},
	{0x3f,0x00},
	{0x88,0x10},
	{0x89,0x79},
	{0x8a,0x22},
	{0xfd,0x02},
	{0xbe,0xd8},
	{0xbf,0x04},
	{0xd0,0xd8},
	{0xd1,0x04},
	{0xc9,0xd8},
	{0xca,0x04},
#else
	//sp2529 26M 2倍频  20-20fps ABinning+DBinning AE_Parameters
	{0xfd,0x00},
	{0x2f,0x04},
	{0x03,0x05},
	{0x04,0xd0},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x02},
	{0x0a,0x13},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0xf8},
	{0xf8,0xcf},
	{0x02,0x05},
	{0x03,0x01},
	{0x06,0xf8},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x06},
	{0x3e,0xcf},
	{0x3f,0x00},
	{0x88,0x10},
	{0x89,0x79},
	{0x8a,0x22},
	{0xfd,0x02},
	{0xbe,0xd8},
	{0xbf,0x04},
	{0xd0,0xd8},
	{0xd1,0x04},
	{0xc9,0xd8},
	{0xca,0x04},

#endif
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_win_2m[] = {
	//uxga
	{0xfd,0x00},
	{0x19,0x00},
	{0x30,0x00},//00  
	{0x31,0x00},
	{0x33,0x00},
	//MIPI      
	{0x95,0x06},
	{0x94,0x40},
	{0x97,0x04},
	{0x96,0xb0},
#if 0
	{0xfd,0x00},
	{0x2f,0x04},//3PLL 花屏
	{0x03,0x02},
	{0x04,0x0a},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x00},
	{0x0a,0xc7},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0x57},
	{0xf8,0x49},
	{0x02,0x0e},
	{0x03,0x01},
	{0x06,0x57},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x11},
	{0x3e,0x49},
	{0x3f,0x00},
	{0x88,0xe2},
	{0x89,0x03},
	{0x8a,0x75},
	{0xfd,0x02},
	{0xbe,0xc2},
	{0xbf,0x04},
	{0xd0,0xc2},
	{0xd1,0x04},
	{0xc9,0xc2},
	{0xca,0x04},
#else
//26M 2pll 50hz 9fps uxga
	{0xfd,0x00},
	{0x2f,0x04},//3
	{0x03,0x02},
	{0x04,0xa0},
	{0x05,0x00},
	{0x06,0x00},
	{0x07,0x00},
	{0x08,0x00},
	{0x09,0x00},
	{0x0a,0xef},
	{0xfd,0x01},
	{0xf0,0x00},
	{0xf7,0x70},
	{0xf8,0x5d},
	{0x02,0x0b},
	{0x03,0x01},
	{0x06,0x70},
	{0x07,0x00},
	{0x08,0x01},
	{0x09,0x00},
	{0xfd,0x02},
	{0x3d,0x0d},
	{0x3e,0x5d},
	{0x3f,0x00},
	{0x88,0x92},
	{0x89,0x81},
	{0x8a,0x54},
	{0xfd,0x02},
	{0xbe,0xd0},
	{0xbf,0x04},
	{0xd0,0xd0},
	{0xd1,0x04},
	{0xc9,0xd0},
	{0xca,0x04},

#endif
	{0xfd,0x01},
	{0x36,0x00},

	//{0xfd,0x00},
	//{0x92,0x81},
	//{0xfd,0x00},

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_brightness_l3[] = {
	{0xfd, 0x01},
	{0xdb, 0xe8},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_brightness_l2[] = {
	{0xfd, 0x01},
	{0xdb, 0xf0},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_brightness_l1[] = {
	{0xfd, 0x01},
	{0xdb, 0xf8},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_brightness_h0[] = {
	{0xfd, 0x01},
	{0xdb, 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_brightness_h1[] = {
	{0xfd, 0x01},
	{0xdb, 0x08},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_brightness_h2[] = {
	{0xfd, 0x01},
	{0xdb, 0x10},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_brightness_h3[] = {
	{0xfd, 0x01},
	{0xdb, 0x18},	

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_saturation_l2[] = {
	{0xfd, 0x01},
	{0xd3, SP2529_P1_0xd3-0x20},
	{0xd4, SP2529_P1_0xd4-0x20},
	{0xd5, SP2529_P1_0xd5-0x20},
	{0xd6, SP2529_P1_0xd6-0x20},
	{0xd7, SP2529_P1_0xd7-0x20},
	{0xd8, SP2529_P1_0xd8-0x20},
	{0xd9, SP2529_P1_0xd9-0x20},
	{0xda, SP2529_P1_0xda-0x20},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_saturation_l1[] = {
	{0xfd, 0x01},
	{0xd3, SP2529_P1_0xd3-0x10},
	{0xd4, SP2529_P1_0xd4-0x10},
	{0xd5, SP2529_P1_0xd5-0x10},
	{0xd6, SP2529_P1_0xd6-0x10},
	{0xd7, SP2529_P1_0xd7-0x10},
	{0xd8, SP2529_P1_0xd8-0x10},
	{0xd9, SP2529_P1_0xd9-0x10},
	{0xda, SP2529_P1_0xda-0x10},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_saturation_h0[] = {
	{0xfd, 0x01},
	{0xd3, SP2529_P1_0xd3},
	{0xd4, SP2529_P1_0xd4},
	{0xd5, SP2529_P1_0xd5},
	{0xd6, SP2529_P1_0xd6},
	{0xd7, SP2529_P1_0xd7},
	{0xd8, SP2529_P1_0xd8},
	{0xd9, SP2529_P1_0xd9},
	{0xda, SP2529_P1_0xda},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_saturation_h1[] = {
	{0xfd, 0x01},	
	{0xd3, SP2529_P1_0xd3+0x10},
	{0xd4, SP2529_P1_0xd4+0x10},
	{0xd5, SP2529_P1_0xd5+0x10},
	{0xd6, SP2529_P1_0xd6+0x10},
	{0xd7, SP2529_P1_0xd7+0x10},
	{0xd8, SP2529_P1_0xd8+0x10},
	{0xd9, SP2529_P1_0xd9+0x10},
	{0xda, SP2529_P1_0xda+0x10},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_saturation_h2[] = {
	{0xfd, 0x01},
	{0xd3, SP2529_P1_0xd3+0x20},
	{0xd4, SP2529_P1_0xd4+0x20},
	{0xd5, SP2529_P1_0xd5+0x20},
	{0xd6, SP2529_P1_0xd6+0x20},
	{0xd7, SP2529_P1_0xd7+0x20},
	{0xd8, SP2529_P1_0xd8+0x20},
	{0xd9, SP2529_P1_0xd9+0x20},
	{0xda, SP2529_P1_0xda+0x20},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list sp2529_mipi_contrast_l3[] = {
	{0xfd,0x01},
	{0x10, SP2529_P1_0x10-0x18},
	{0x11, SP2529_P1_0x11-0x18},
	{0x12, SP2529_P1_0x12-0x18},
	{0x13, SP2529_P1_0x13-0x18},
	{0x14, SP2529_P1_0x14-0x18},
	{0x15, SP2529_P1_0x15-0x18},
	{0x16, SP2529_P1_0x16-0x18},
	{0x17, SP2529_P1_0x17-0x18},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_contrast_l2[] = {
	{0xfd,0x01},
	{0x10, SP2529_P1_0x10-0x10},
	{0x11, SP2529_P1_0x11-0x10},
	{0x12, SP2529_P1_0x12-0x10},
	{0x13, SP2529_P1_0x13-0x10},
	{0x14, SP2529_P1_0x14-0x10},
	{0x15, SP2529_P1_0x15-0x10},
	{0x16, SP2529_P1_0x16-0x10},
	{0x17, SP2529_P1_0x17-0x10},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_contrast_l1[] = {
	{0xfd,0x01},
	{0x10, SP2529_P1_0x10-0x08},
	{0x11, SP2529_P1_0x11-0x08},
	{0x12, SP2529_P1_0x12-0x08},
	{0x13, SP2529_P1_0x13-0x08},
	{0x14, SP2529_P1_0x14-0x08},
	{0x15, SP2529_P1_0x15-0x08},
	{0x16, SP2529_P1_0x16-0x08},
	{0x17, SP2529_P1_0x17-0x08},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_contrast_h0[] = {
	{0xfd,0x01},
	{0x10, SP2529_P1_0x10},
	{0x11, SP2529_P1_0x11},
	{0x12, SP2529_P1_0x12},
	{0x13, SP2529_P1_0x13},
	{0x14, SP2529_P1_0x14},
	{0x15, SP2529_P1_0x15},
	{0x16, SP2529_P1_0x16},
	{0x17, SP2529_P1_0x17},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_contrast_h1[] = {
	{0xfd,0x01},
	{0x10, SP2529_P1_0x10+0x08},
	{0x11, SP2529_P1_0x11+0x08},
	{0x12, SP2529_P1_0x12+0x08},
	{0x13, SP2529_P1_0x13+0x08},
	{0x14, SP2529_P1_0x14+0x08},
	{0x15, SP2529_P1_0x15+0x08},
	{0x16, SP2529_P1_0x16+0x08},
	{0x17, SP2529_P1_0x17+0x08},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_contrast_h2[] = {
	{0xfd,0x01},
	{0x10, SP2529_P1_0x10+0x10},
	{0x11, SP2529_P1_0x11+0x10},
	{0x12, SP2529_P1_0x12+0x10},
	{0x13, SP2529_P1_0x13+0x10},
	{0x14, SP2529_P1_0x14+0x10},
	{0x15, SP2529_P1_0x15+0x10},
	{0x16, SP2529_P1_0x16+0x10},
	{0x17, SP2529_P1_0x17+0x10},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_contrast_h3[] = {
	{0xfd,0x01},
	{0x10, SP2529_P1_0x10+0x18},
	{0x11, SP2529_P1_0x11+0x18},
	{0x12, SP2529_P1_0x12+0x18},
	{0x13, SP2529_P1_0x13+0x18},
	{0x14, SP2529_P1_0x14+0x18},
	{0x15, SP2529_P1_0x15+0x18},
	{0x16, SP2529_P1_0x16+0x18},
	{0x17, SP2529_P1_0x17+0x18},

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp2529_mipi_whitebalance_auto[] = {
	{0xfd , 0x02},
	{0x26 , 0xbf},
	{0x27 , 0xa3},
	{0xfd , 0x01},
	{0x32 , 0x15},
	{0xfd , 0x02},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_whitebalance_incandescent[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0x88},
	{0x27 , 0xd0},
	{0xfd , 0x00},	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_whitebalance_fluorescent[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0x95},
	{0x27 , 0xba},
	{0xfd , 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_whitebalance_daylight[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0xd1},
	{0x27 , 0x85},
	{0xfd , 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_whitebalance_cloudy[] = {
	{0xfd , 0x01},
	{0x32 , 0x05},
	{0xfd , 0x02},
	{0x26 , 0xdb},
	{0x27 , 0x70},
	{0xfd , 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp2529_mipi_effect_none[] = {
	{0xfd , 0x01},
	{0x66 , 0x00},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_effect_mono[] = {
	{0xfd , 0x01},
	{0x66 , 0x20},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_effect_negative[] = {
	
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_effect_solarize[] = {

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_effect_sepia[] = {
	{0xfd , 0x01},
	{0x66 , 0x00},
	{0x67 , 0x80},
	{0x68 , 0x80},
	{0xdf , 0x00},
	{0xfd , 0x02},
	{0x14 , 0x00},

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_effect_posterize[] = {

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_effect_whiteboard[] = {

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_effect_blackboard[] = {

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list sp2529_mipi_effect_aqua[] = {

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_exposure_l2[] = {
	{0xfd , 0x01},
	{0xeb , SP2529_P1_0xeb-0x08},
	{0xec , SP2529_P1_0xec-0x08},

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_exposure_l1[] = {
	{0xfd , 0x01},
	{0xeb , SP2529_P1_0xeb-0x04},
	{0xec , SP2529_P1_0xec-0x04},
	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_exposure_h0[] = {
	{0xfd , 0x01},
	{0xeb , SP2529_P1_0xeb},
	{0xec , SP2529_P1_0xec},

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_exposure_h1[] = {
	{0xfd , 0x01},
	{0xeb , SP2529_P1_0xeb+0x04},
	{0xec , SP2529_P1_0xec+0x04},	

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_exposure_h2[] = {
	{0xfd , 0x01},
	{0xeb , SP2529_P1_0xeb+0x08},
	{0xec , SP2529_P1_0xec+0x08},

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};


/////////////////////////next preview mode////////////////////////////////////

static struct regval_list sp2529_mipi_previewmode_mode00[] = {

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list sp2529_mipi_previewmode_mode01[] = {

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_previewmode_mode02[] = {// portrait

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list sp2529_mipi_previewmode_mode03[] = {//landscape

	{SP2529_MIPI_REG_END, 0xff},	/* END MARKER */
};


///////////////////////////////preview mode end////////////////////////
static int sp2529_mipi_read(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	//client->addr = 0x1a;
	//ret = i2c_smbus_read_byte_data(client, reg);
	//client->addr = 0x3c;
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return ret;

	*value = (unsigned char)ret;

	return 0;
}

static int sp2529_mipi_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sp2529_mipi_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != SP2529_MIPI_REG_END) {
		int ret = sp2529_mipi_write(sd, vals->reg_num, vals->value);

		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int sp2529_mipi_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int sp2529_mipi_shutter(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2 ;
	unsigned short v;

	sp2529_mipi_write(sd, 0xb6,0x00);

	ret = sp2529_mipi_read(sd, 0x03, &v1);
	if (ret < 0)
		return ret;
	ret = sp2529_mipi_read(sd, 0x04, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2;
	v = v /2 ;
	if (v<1)
		return 0;

	sp2529_mipi_write(sd, 0x03, (v >> 8)&0xff );
	sp2529_mipi_write(sd, 0x04, v&0xff);

	msleep(500);

	return  0;
}



static int sp2529_mipi_init(struct v4l2_subdev *sd, u32 val)
{
	struct sp2529_mipi_info *info = container_of(sd, struct sp2529_mipi_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return sp2529_mipi_write_array(sd, sp2529_mipi_init_regs);
}

static int sp2529_mipi_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	mdelay(10);
	int i;
	for(i=0;i<5;i++)
	{
		ret = sp2529_mipi_read(sd, 0x02, &v);
		printk("\n SP2529_MIPI_CHIP_ID_H=0x%x\n ", v);
	}
	if (ret < 0)
		return ret;
	if (v != SP2529_MIPI_CHIP_ID_H)	
		return -ENODEV;
	for(i=0;i<5;i++)
	{ 
	ret = sp2529_mipi_read(sd, 0xa0, &v);
		printk("\n SP2529_MIPI_CHIP_ID_L=0x%x\n ", v);
	}
	if (ret < 0)
		return ret;
	if (v != SP2529_MIPI_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct sp2529_mipi_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} sp2529_mipi_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= NULL,
	},
};
#define N_SP2529_MIPI_FMTS ARRAY_SIZE(sp2529_mipi_formats)

static struct sp2529_mipi_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
} sp2529_mipi_win_sizes[] = {
		/* VGA */
	/*
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= sp2529_mipi_win_vga,
	},
	
	{
		.width		= SVGA_WIDTH,
		.height		= SVGA_HEIGHT,
		.regs 		= sp2529_mipi_win_svga,
	},
	*/
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.regs 		= sp2529_mipi_win_2m,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(sp2529_mipi_win_sizes))

static int sp2529_mipi_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_SP2529_MIPI_FMTS)
		return -EINVAL;

	*code = sp2529_mipi_formats[index].mbus_code;
	return 0;
}

static int sp2529_mipi_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct sp2529_mipi_format_struct **ret_fmt,
		struct sp2529_mipi_win_size **ret_wsize)
{
	int index;
	struct sp2529_mipi_win_size *wsize;

	for (index = 0; index < N_SP2529_MIPI_FMTS; index++)
		if (sp2529_mipi_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_SP2529_MIPI_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = sp2529_mipi_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = sp2529_mipi_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = sp2529_mipi_win_sizes; wsize < sp2529_mipi_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= sp2529_mipi_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = sp2529_mipi_formats[index].colorspace;
	return 0;
}

static int sp2529_mipi_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return sp2529_mipi_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int sp2529_mipi_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct sp2529_mipi_info *info = container_of(sd, struct sp2529_mipi_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct sp2529_mipi_format_struct *fmt_s;
	struct sp2529_mipi_win_size *wsize;
	int ret,len,i = 0;

	ret = sp2529_mipi_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;


	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = sp2529_mipi_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {

		memset(data, 0, sizeof(*data));
		data->mipi_clk = 520;//288; /* Mbps. */

		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_8BIT;
			data->slave_addr = client->addr;
			len = ARRAY_SIZE(sp2529_mipi_win_2m);
			data->reg_num = len;  //

			msleep(400);//ppp
			for(i =0;i<len;i++) {
				data->reg[i].addr = sp2529_mipi_win_2m[i].reg_num;
				data->reg[i].data = sp2529_mipi_win_2m[i].value;
			}

			ret = sp2529_mipi_shutter(sd);
		}

		else if ((wsize->width == VGA_WIDTH)
			&& (wsize->height == VGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_8BIT;
			data->slave_addr = client->addr;
			msleep(500);
			len = ARRAY_SIZE(sp2529_mipi_win_vga);
			data->reg_num = len;  //
			for(i =0;i<len;i++) {
				data->reg[i].addr = sp2529_mipi_win_vga[i].reg_num;
				data->reg[i].data = sp2529_mipi_win_vga[i].value;
			}
		}
	}
	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int sp2529_mipi_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct sp2529_mipi_info *info = container_of(sd, struct sp2529_mipi_info, sd);

	printk("sp2529_mipi_set_framerate %d\n", framerate);

	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;

	return 0;
}


static int sp2529_mipi_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = sp2529_mipi_write_array(sd, sp2529_mipi_stream_on);
	else
		ret = sp2529_mipi_write_array(sd, sp2529_mipi_stream_off);

	msleep(150);

	return ret;
}

static int sp2529_mipi_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int sp2529_mipi_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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
	a->defrect.width		=MAX_WIDTH; //MAX_PREVIEW_WIDTH;
	a->defrect.height		=MAX_WIDTH; //MAX_PREVIEW_HEIGHT;
	//preview max size end
	a->type 			= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int sp2529_mipi_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp2529_mipi_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp2529_mipi_frame_rates[] = { 30, 15, 10, 5, 1 };

static int sp2529_mipi_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(sp2529_mipi_frame_rates))
		return -EINVAL;

	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = sp2529_mipi_frame_rates[interval->index];
	return 0;
}

static int sp2529_mipi_enum_framesizes(struct v4l2_subdev *sd,
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
		struct sp2529_mipi_win_size *win = &sp2529_mipi_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int sp2529_mipi_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int sp2529_mipi_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[SP2529]set brightness %d\n", brightness);

	switch (brightness) {
		case BRIGHTNESS_L3:
			sp2529_mipi_write_array(sd, sp2529_mipi_brightness_l3);
			break;
		case BRIGHTNESS_L2:
			sp2529_mipi_write_array(sd, sp2529_mipi_brightness_l2);
			break;
		case BRIGHTNESS_L1:
			sp2529_mipi_write_array(sd, sp2529_mipi_brightness_l1);
			break;
		case BRIGHTNESS_H0:
			sp2529_mipi_write_array(sd, sp2529_mipi_brightness_h0);
			break;
		case BRIGHTNESS_H1:
			sp2529_mipi_write_array(sd, sp2529_mipi_brightness_h1);
			break;
		case BRIGHTNESS_H2:
			sp2529_mipi_write_array(sd, sp2529_mipi_brightness_h2);
			break;
		case BRIGHTNESS_H3:
			sp2529_mipi_write_array(sd, sp2529_mipi_brightness_h3);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp2529_mipi_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	switch (contrast) {
		case CONTRAST_L3:
			sp2529_mipi_write_array(sd, sp2529_mipi_contrast_l3);
			break;
		case CONTRAST_L2:
			sp2529_mipi_write_array(sd, sp2529_mipi_contrast_l2);
			break;
		case CONTRAST_L1:
			sp2529_mipi_write_array(sd, sp2529_mipi_contrast_l1);
			break;
		case CONTRAST_H0:
			sp2529_mipi_write_array(sd, sp2529_mipi_contrast_h0);
			break;
		case CONTRAST_H1:
			sp2529_mipi_write_array(sd, sp2529_mipi_contrast_h1);
			break;
		case CONTRAST_H2:
			sp2529_mipi_write_array(sd, sp2529_mipi_contrast_h2);
			break;
		case CONTRAST_H3:
			sp2529_mipi_write_array(sd, sp2529_mipi_contrast_h3);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp2529_mipi_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	printk(KERN_DEBUG "[SP2529]set saturation %d\n", saturation);

	switch (saturation) {
		case SATURATION_L2:
			sp2529_mipi_write_array(sd, sp2529_mipi_saturation_l2);
			break;
		case SATURATION_L1:
			sp2529_mipi_write_array(sd, sp2529_mipi_saturation_l1);
			break;
		case SATURATION_H0:
			sp2529_mipi_write_array(sd, sp2529_mipi_saturation_h0);
			break;
		case SATURATION_H1:
			sp2529_mipi_write_array(sd, sp2529_mipi_saturation_h1);
			break;
		case SATURATION_H2:
			sp2529_mipi_write_array(sd, sp2529_mipi_saturation_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp2529_mipi_set_wb(struct v4l2_subdev *sd, int wb)
{
	printk(KERN_DEBUG "[SP2529]set contrast %d\n", wb);

	switch (wb) {
		case WHITE_BALANCE_AUTO:
			sp2529_mipi_write_array(sd, sp2529_mipi_whitebalance_auto);
			break;
		case WHITE_BALANCE_INCANDESCENT:
			sp2529_mipi_write_array(sd, sp2529_mipi_whitebalance_incandescent);
			break;
		case WHITE_BALANCE_FLUORESCENT:
			sp2529_mipi_write_array(sd, sp2529_mipi_whitebalance_fluorescent);
			break;
		case WHITE_BALANCE_DAYLIGHT:
			sp2529_mipi_write_array(sd, sp2529_mipi_whitebalance_daylight);
			break;
		case WHITE_BALANCE_CLOUDY_DAYLIGHT:
			sp2529_mipi_write_array(sd, sp2529_mipi_whitebalance_cloudy);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}
static int sp2529_mipi_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk(KERN_DEBUG "[SP2529]set contrast %d\n", effect);

	switch (effect) {
		case EFFECT_NONE:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_none);
			break;
		case EFFECT_MONO:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_mono);
			break;
		case EFFECT_NEGATIVE:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_negative);
			break;
		case EFFECT_SOLARIZE:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_solarize);
			break;
		case EFFECT_SEPIA:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_sepia);
			break;
		case EFFECT_POSTERIZE:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_posterize);
			break;
		case EFFECT_WHITEBOARD:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_whiteboard);
			break;
		case EFFECT_BLACKBOARD:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_blackboard);
			break;
		case EFFECT_AQUA:
			sp2529_mipi_write_array(sd, sp2529_mipi_effect_aqua);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sp2529_mipi_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk(KERN_DEBUG "[SP2529]set exposure %d\n", exposure);

	switch (exposure) {
		case EXPOSURE_L2:
			sp2529_mipi_write_array(sd, sp2529_mipi_exposure_l2);
			break;
		case EXPOSURE_L1:
			sp2529_mipi_write_array(sd, sp2529_mipi_exposure_l1);
			break;
		case EXPOSURE_H0:
			sp2529_mipi_write_array(sd, sp2529_mipi_exposure_h0);
			break;
		case EXPOSURE_H1:
			sp2529_mipi_write_array(sd, sp2529_mipi_exposure_h1);
			break;
		case EXPOSURE_H2:
			sp2529_mipi_write_array(sd, sp2529_mipi_exposure_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int sp2529_mipi_set_previewmode(struct v4l2_subdev *sd, int mode)
{
	printk(KERN_INFO "MIKE:[SP2529]set_previewmode %d\n", mode);

	switch (mode) {
		case preview_auto:
		case normal:
		case dynamic:
		case beatch:
		case bar:
		case snow:
		case landscape:
			sp2529_mipi_write_array(sd, sp2529_mipi_previewmode_mode00);
			break;

		case nightmode:
		case theatre:
		case party:
		case candy:
		case fireworks:
		case sunset:
		case nightmode_portrait:
			sp2529_mipi_write_array(sd, sp2529_mipi_previewmode_mode01);
			break;

		case sports:
		case anti_shake:
			sp2529_mipi_write_array(sd, sp2529_mipi_previewmode_mode02);
			break;

		case portrait:
			sp2529_mipi_write_array(sd, sp2529_mipi_previewmode_mode03);
			break;
		default:

			return -EINVAL;
	}

	return 0;
}





static int sp2529_mipi_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sp2529_mipi_info *info = container_of(sd, struct sp2529_mipi_info, sd);
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
			isp_setting.setting = (struct v4l2_isp_regval *)&sp2529_mipi_isp_setting;
			isp_setting.size = ARRAY_SIZE(sp2529_mipi_isp_setting);
			memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
			break;
		case V4L2_CID_ISP_PARM:
			ctrl->value = (int)&sp2529_mipi_isp_parm;
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int sp2529_mipi_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sp2529_mipi_info *info = container_of(sd, struct sp2529_mipi_info, sd);
	int ret = 0;

	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ret = sp2529_mipi_set_brightness(sd, ctrl->value);
			if (!ret)
				info->brightness = ctrl->value;
			break;
		case V4L2_CID_CONTRAST:
			ret = sp2529_mipi_set_contrast(sd, ctrl->value);
			if (!ret)
				info->contrast = ctrl->value;
			break;
		case V4L2_CID_SATURATION:
			ret = sp2529_mipi_set_saturation(sd, ctrl->value);
			if (!ret)
				info->saturation = ctrl->value;
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			ret = sp2529_mipi_set_wb(sd, ctrl->value);
			if (!ret)
				info->wb = ctrl->value;
			break;
		case V4L2_CID_EXPOSURE:
			ret = sp2529_mipi_set_exposure(sd, ctrl->value);
			if (!ret)
				info->exposure = ctrl->value;
			break;
		case V4L2_CID_EFFECT:
			ret = sp2529_mipi_set_effect(sd, ctrl->value);
			if (!ret)
				info->effect = ctrl->value;
			break;

		case 10094870:// preview mode add
			ret = sp2529_mipi_set_previewmode(sd, ctrl->value);
			if (!ret)
				info->previewmode = ctrl->value;

			break;

		case V4L2_CID_FRAME_RATE:
			ret = sp2529_mipi_set_framerate(sd, ctrl->value);
			break;
		case V4L2_CID_SET_FOCUS:
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}

	return ret;
}

static int sp2529_mipi_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SP2529, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sp2529_mipi_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sp2529_mipi_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;

	return ret;
}

static int sp2529_mipi_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sp2529_mipi_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}
#endif

static const struct v4l2_subdev_core_ops sp2529_mipi_core_ops = {
	.g_chip_ident = sp2529_mipi_g_chip_ident,
	.g_ctrl = sp2529_mipi_g_ctrl,
	.s_ctrl = sp2529_mipi_s_ctrl,
	.queryctrl = sp2529_mipi_queryctrl,
	.reset = sp2529_mipi_reset,
	.init = sp2529_mipi_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = sp2529_mipi_g_register,
	.s_register = sp2529_mipi_s_register,
#endif
};

static const struct v4l2_subdev_video_ops sp2529_mipi_video_ops = {
	.enum_mbus_fmt = sp2529_mipi_enum_mbus_fmt,
	.try_mbus_fmt = sp2529_mipi_try_mbus_fmt,
	.s_mbus_fmt = sp2529_mipi_s_mbus_fmt,
	.s_stream = sp2529_mipi_s_stream,
	.cropcap = sp2529_mipi_cropcap,
	.g_crop	= sp2529_mipi_g_crop,
	.s_parm = sp2529_mipi_s_parm,
	.g_parm = sp2529_mipi_g_parm,
	.enum_frameintervals = sp2529_mipi_enum_frameintervals,
	.enum_framesizes = sp2529_mipi_enum_framesizes,
};

static const struct v4l2_subdev_ops sp2529_mipi_ops = {
	.core = &sp2529_mipi_core_ops,
	.video = &sp2529_mipi_video_ops,
};

static int sp2529_mipi_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sp2529_mipi_info *info;
	int ret;

	info = kzalloc(sizeof(struct sp2529_mipi_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &sp2529_mipi_ops);
#if 1
	/* Make sure it's an sp2529_mipi */
	ret = sp2529_mipi_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an sp2529_mipi chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "sp2529_mipi chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);
#endif
	return ret;
}

static int sp2529_mipi_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp2529_mipi_info *info = container_of(sd, struct sp2529_mipi_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id sp2529_mipi_id[] = {
	{ "sp2529-mipi-yuv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sp2529_mipi_id);

static struct i2c_driver sp2529_mipi_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sp2529-mipi-yuv",
	},
	.probe		= sp2529_mipi_probe,
	.remove		= sp2529_mipi_remove,
	.id_table	= sp2529_mipi_id,
};

static __init int init_sp2529_mipi(void)
{
	return i2c_add_driver(&sp2529_mipi_driver);
}

static __exit void exit_sp2529_mipi(void)
{
	i2c_del_driver(&sp2529_mipi_driver);
}

fs_initcall(init_sp2529_mipi);
module_exit(exit_sp2529_mipi);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore sp2529 sensors with mipi interface");
MODULE_LICENSE("GPL");
