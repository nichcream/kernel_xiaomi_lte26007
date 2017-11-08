
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
#include "ov8825.h"

#define OV8825_CHIP_ID_H	(0x88)
#define OV8825_CHIP_ID_M	(0x25)
#define OV8825_CHIP_ID_L	(0x00)

#define UXGA_WIDTH		1632//1920
#define UXGA_HEIGHT		1224//1080
#define MAX_WIDTH		3264
#define MAX_HEIGHT		2448
#define MAX_PREVIEW_WIDTH	UXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  UXGA_HEIGHT

//#define MIPI_4LANE
#define MIPI_2LANE
//#define GROUP_WRITE

#define OV8825_REG_END		0xffff
#define OV8825_REG_DELAY	0xfffe

//#define OV8825_DEBUG_FUNC

struct ov8825_format_struct;
struct ov8825_info {
	struct v4l2_subdev sd;
	struct ov8825_format_struct *fmt;
	struct ov8825_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int frame_rate;
	int binning;
	int test;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};


struct otp_struct {
	int customer_id;
	int module_integrator_id;
	int lens_id;
	int rg_ratio;
	int bg_ratio;
	int user_data[3];
	int light_rg;
	int light_bg;
	int lenc[62];
};

static struct regval_list ov8825_init_regs[] = {
	//{0x0100, 0x00}, // software standby
	{0x0103, 0x01}, // software reset
	//delay(5ms) 
	{OV8825_REG_DELAY, 5},
	{0x3000, 0x16}, // strobe disable, frex disable,vsync disable
	{0x3001, 0x00}, //
	{0x3002, 0x6c}, // SCCB ID=0x6c
	{0x3003, 0xce},
	{0x3004, 0xd4},
	{0x3005, 0x00},
	{0x3006, 0x10},
	{0x3007, 0x3b},
	{0x300d, 0x00}, // PLL2
	{0x301f, 0x09}, // frex_mask_mipi, frex_mask_mipi_phy
	{0x3020, 0x01},
	{0x3010, 0x00}, // strobe,sda,frex,vsync,shutter GPIO unselected
	{0x3011, 0x01},
	{0x3012, 0x80},
	{0x3013, 0x39},
	{0x3018, 0x00}, // clear PHY HS TX power down and PHY LP RX power down
	{0x3104, 0x20},
	{0x3106, 0x15},
	{0x3300, 0x00}, //
	{0x3500, 0x00}, // exposure[19:16]=0
	{0x3501, 0x4e},
	{0x3502, 0xa0},
	{0x3503, 0x27}, // Gain has no delay, VTS manual, AGC manual, AEC manual
	{0x3509, 0x10}, // use sensor gain, modified by Richard
	{0x350b, 0x1f},
	{0x3600, 0x06},
	{0x3601, 0x34},
	{0x3602, 0x42}, // 
	//analog control
	{0x3603, 0x5c},
	{0x3604, 0x98},
	{0x3605, 0xf5},
	{0x3609, 0xb4},
	{0x360a, 0x7c},
	{0x360b, 0xc9},
	{0x360c, 0x0b},
	{0x3612, 0x00},//pad drive 1x, analog control
	{0x3613, 0x02},
	{0x3614, 0x0f},
	{0x3615, 0x00},
	{0x3616, 0x03},
	{0x3617, 0xa1},
	{0x3618, 0x0f},//VCM position & slew rate, slew rate=0
	{0x3619, 0x00},//VCM position=0
	{0x361a, 0xb0},//VCM clock divider, VCM clock=24000000/0x4b0=20000
	{0x361b, 0x04},//VCM clock divider
	{0x361c, 0x07},
	{0x3700, 0x20},
	{0x3701, 0x44}, 
	{0x3702, 0x50},
	{0x3703, 0xcc},
	{0x3704, 0x19},
	{0x3705, 0x32},
	{0x3706, 0x4b},
	{0x3707, 0x63},
	{0x3708, 0x84},
	{0x3709, 0x40},
	{0x370a, 0x33},
	{0x370b, 0x01},
	{0x370c, 0x50}, 
	{0x370d, 0x00},
	{0x370e, 0x00},
	{0x3711, 0x0f},
	{0x3712, 0x9c},
	{0x3724, 0x01},
	{0x3725, 0x92},
	{0x3726, 0x01},
	{0x3727, 0xc7},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0x9b},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x0d},
	{0x380d, 0xbc},
	{0x380e, 0x04},
	{0x380f, 0xf0},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3816,0x02},// Hsync start H                                                                                                                         
	{0x3817,0x40},// Hsync start L                                                                                                                      
	{0x3818,0x00},// Hsync end H                                                                                                                           
	{0x3819,0x40},// Hsync end L                                                                                                                           
	{0x3820, 0x81},
	{0x3821, 0x17},
	{0x3b1f,0x00},// Frex conrol                                                                                                                           
	//clear OTP data buffer                                                                                                                            
	{0x3d00,0x00},                                                                                                                                        
	{0x3d01,0x00},                                                                                                                                        
	{0x3d02,0x00},                                                                                                                                        
	{0x3d03,0x00},                                                                                                                                        
	{0x3d04,0x00},                                                                                                                                        
	{0x3d05,0x00},                                                                                                                                        
	{0x3d06,0x00},                                                                                                                                        
	{0x3d07,0x00},                                                                                                                                        
	{0x3d08,0x00},                                                                                                                                        
	{0x3d09,0x00},                                                                                                                                        
	{0x3d0a,0x00},                                                                                                                                        
	{0x3d0b,0x00},                                                                                                                                        
	{0x3d0c,0x00},                                                                                                                                        
	{0x3d0d,0x00},                                                                                                                                        
	{0x3d0e,0x00},                                                                                                                                        
	{0x3d0f,0x00},                                                                                                                                        
	{0x3d10,0x00},                                                                                                                                        
	{0x3d11,0x00},                                                                                                                                        
	{0x3d12,0x00},                                                                                                                                        
	{0x3d13,0x00},                                                                                                                                        
	{0x3d14,0x00},                                                                                                                                        
	{0x3d15,0x00},                                                                                                                                        
	{0x3d16,0x00},                                                                                                                                        
	{0x3d17,0x00},
	
	{0x3d18, 0x00},                                                                                                                                         
	{0x3d19, 0x00},                                                                                                                                         
	{0x3d1a, 0x00},                                                                                                                                         
	{0x3d1b, 0x00},                                                                                                                                         
	{0x3d1c, 0x00},                                                                                                                                         
	{0x3d1d, 0x00},                                                                                                                                         
	{0x3d1e, 0x00},                                                                                                                                         
	{0x3d1f, 0x00},                                                                                                                                         
	{0x3d80, 0x00},                                                                                                                                         
	{0x3d81, 0x00},                                                                                                                                         
	{0x3d84, 0x00},                                                                                                                                         
	{0x3f00, 0x00},
	{0x3f01, 0xfc},
	{0x3f05, 0x10},
	{0x3f06, 0x00},                                                                                                                                         
	{0x3f07, 0x00},
	//BLC                                                                                                                                              
	{0x4000,0x29},                                                                                                                                         
	{0x4001,0x02}, // BLC start line                                                                                                                        
	{0x4002,0x45}, // BLC auto, reset 5 frames                                                                                                              
	{0x4003,0x08}, // BLC redo at 8 frames                                                                                                                  
	{0x4004,0x04}, // 4 black lines are used for BLC                                                                                                        
	{0x4005,0x18}, // no black line output, apply one channel offiset (0x400c, 0x400d) to all manual BLC channels                                                                                                                                           
	{0x404e, 0x37},
	{0x404f, 0x8f},
	{0x4300,0xff}, // max                                                                                                                                   
	{0x4303,0x00}, // format control                                                                                                                        
	{0x4304,0x08}, // output {data[7:0], data[9:8]}                                                                                                         
	{0x4307,0x00}, // embeded control                                                                                                                       
	{0x4600, 0x04},
	{0x4601, 0x00},
	{0x4602, 0x30},
	{0x4800,0x04},                                                                                                                                         
	{0x4801,0x0f}, // ECC configure                                                                                                                         
	{0x4837, 0x17},
	{0x4843,0x02}, // manual set pclk divider                                                                                                               
	//ISP                                                                                                                                          
	{0x5000,0x06}, // LENC off, BPC on, WPC on                                                                                                              
	{0x5001,0x00}, // MWB off                                                                                                                               
	{0x5002,0x00},                                                                                                                                         
	{0x5068, 0x00},
	{0x506a, 0x00},
	{0x501f,0x00}, // enable ISP                                                                                                                            
	{0x5780,0xfc},                                                                                                                                         
	{0x5c00, 0x80},
	{0x5c01, 0x00},
	{0x5c02, 0x00},
	{0x5c03, 0x00},
	{0x5c04, 0x00},
	{0x5c05,0x00}, // pre BLC                                                                                                                               
	{0x5c06,0x00}, // pre BLC                                                                                                                               
	{0x5c07,0x80}, // pre BLC                                                                                                                               
	{0x5c08, 0x10},
	{0x6700,0x05},                                                                                                                                         
	{0x6701,0x19},                                                                                                                                         
	{0x6702,0xfd},                                                                                                                                         
	{0x6703,0xd7},                                                                                                                                         
	{0x6704,0xff},                                                                                                                                         
	{0x6705,0xff},                                                                                                                                         
	{0x6800,0x10},                                                                                                                                         
	{0x6801,0x02},                                                                                                                                         
	{0x6802,0x90},
	
	{0x6803, 0x10},                                                                                                                                         
	{0x6804, 0x59},                                                                                                                                         
	{0x6900, 0x60},
	{0x6901, 0x04}, // CADC control
	//Lens Correction                                                                                                                                   
	{0x5800, 0x0f},                                                                                                                                         
	{0x5801, 0x0d},                                                                                                                                         
	{0x5802, 0x09},                                                                                                                                         
	{0x5803, 0x0a},                                                                                                                                         
	{0x5804, 0x0d},                                                                                                                                         
	{0x5805, 0x14},                                                                                                                                         
	{0x5806, 0x0a},                                                                                                                                         
	{0x5807, 0x04},                                                                                                                                         
	{0x5808, 0x03},                                                                                                                                         
	{0x5809, 0x03},                                                                                                                                         
	{0x580a, 0x05},                                                                                                                                         
	{0x580b, 0x0a},                                                                                                                                         
	{0x580c, 0x05},                                                                                                                                         
	{0x580d, 0x02},                                                                                                                                         
	{0x580e, 0x00},                                                                                                                                         
	{0x580f, 0x00},                                                                                                                                         
	{0x5810, 0x03},                                                                                                                                         
	{0x5811, 0x05},                                                                                                                                         
	{0x5812, 0x09},                                                                                                                                         
	{0x5813, 0x03},                                                                                                                                         
	{0x5814, 0x01},                                                                                                                                         
	{0x5815, 0x01},                                                                                                                                         
	{0x5816, 0x04},                                                                                                                                         
	{0x5817, 0x09},                                                                                                                                         
	{0x5818, 0x09},                                                                                                                                         
	{0x5819, 0x08},                                                                                                                                         
	{0x581a, 0x06},                                                                                                                                         
	{0x581b, 0x06},                                                                                                                                         
	{0x581c, 0x08},                                                                                                                                         
	{0x581d, 0x06},                                                                                                                                         
	{0x581e, 0x33},                                                                                                                                         
	{0x581f, 0x11},                                                                                                                                         
	{0x5820, 0x0e},                                                                                                                                         
	{0x5821, 0x0f},                                                                                                                                         
	{0x5822, 0x11},                                                                                                                                         
	{0x5823, 0x3f},                                                                                                                                         
	{0x5824, 0x08},                                                                                                                                         
	{0x5825, 0x46},                                                                                                                                         
	{0x5826, 0x46},                                                                                                                                         
	{0x5827, 0x46},                                                                                                                                         
	{0x5828, 0x46},                                                                                                                                         
	{0x5829, 0x46},                                                                                                                                         
	{0x582a, 0x42},                                                                                                                                         
	{0x582b, 0x42},
	
	{0x582c, 0x44},                                                                                                                                         
	{0x582d, 0x46},                                                                                                                                         
	{0x582e, 0x46},                                                                                                                                         
	{0x582f, 0x60},                                                                                                                                         
	{0x5830, 0x62},                                                                                                                                         
	{0x5831, 0x42},                                                                                                                                         
	{0x5832, 0x46},                                                                                                                                         
	{0x5833, 0x46},                                                                                                                                         
	{0x5834, 0x44},                                                                                                                                         
	{0x5835, 0x44},                                                                                                                                         
	{0x5836, 0x44},                                                                                                                                         
	{0x5837, 0x48},                                                                                                                                         
	{0x5838, 0x28},                                                                                                                                         
	{0x5839, 0x46},                                                                                                                                         
	{0x583a, 0x48},                                                                                                                                         
	{0x583b, 0x68},                                                                                                                                         
	{0x583c, 0x28},                                                                                                                                         
	{0x583d, 0xae},                                                                                                                                         
	{0x5842, 0x00},                                                                                                                                         
	{0x5843, 0xef},                                                                                                                                         
	{0x5844, 0x01},                                                                                                                                         
	{0x5845, 0x3f},                                                                                                                                         
	{0x5846, 0x01},                                                                                                                                         
	{0x5847, 0x3f},                                                                                                                                         
	{0x5848, 0x00},                                                                                                                                         
	{0x5849, 0xd5},                                                                                                                                         
	//Exposure                                                                                                                                        
	//MWB                                                                                                                                         
	{0x3400, 0x04}, // red h                                                                                                                                 
	{0x3401, 0x00}, // red l                                                                                                                                 
	{0x3402, 0x04}, // green h                                                                                                                               
	{0x3403, 0x00}, // green l                                                                                                                               
	{0x3404, 0x04}, // blue h                                                                                                                                
	{0x3405, 0x00}, // blue l                                                                                                                                
	{0x3406, 0x01}, // MWB manual                                                                                                                            
	//ISP                                                                                                                                         
	{0x5001, 0x01}, // MWB on                                                                                                                                
	{0x5000, 0x86}, // LENC on, BPC on, WPC on

#if defined(MIPI_2LANE)
	//1632_1224_2Lane_30fps_68.25Msysclk_689MBps/lane @ 19.5M MCLK
	
	//6c 3003 ce ; PLL_CTRL0
	{0x3003, 0xce}, ////PLL_CTRL0  
	//6c 3004 d4 ; PLL_CTRL1
	{0x3004, 0xcc}, ////PLL_CTRL1      
	//6c 3005 00 ; PLL_CTRL2
	{0x3005, 0x00}, ////PLL_CTRL2 
	//6c 3006 10 ; PLL_CTRL3
	{0x3006, 0x10}, ////PLL_CTRL3    
	//6c 3007 0b ; PLL_CTRL4
	{0x3007, 0x5a}, ////PLL_CTRL4  
	{0x3011, 0x01}, ////MIPI_Lane_2_Lane
	{0x3012, 0x80}, ////SC_PLL CTRL_S0  
	//6c 3013 39 ; SC_PLL CTRL_S1
	{0x3013, 0x39}, ////SC_PLL CTRL_S1
	{0x3104, 0x20}, ////SCCB_PLL
	{0x3106, 0x15}, ////SRB_CTRL                                                                                                      
#endif
	//group address define
	{0x3200, 0x00}, 
	{0x3201, 0x08}, 
	{0x3202, 0x10}, 
	{0x3203, 0x18},
	
	{0x0100, 0x01}, // wake up
	{OV8825_REG_DELAY, 100},
	//  #if defined(GROUP_WRITE)
	{0x3208, 0x01}, //group 1
	//1632_1224_2Lane_30fps_68.25Msysclk_689MBps/lane @ 19.5M MCLK
	{0x0100, 0x00}, // sleep     
	{0x3020, 0x01},
	{0x3700, 0x20},
	{0x3702, 0x50},
	{0x3703, 0xcc},
	{0x3704, 0x19},
	{0x3705, 0x32},
	{0x3706, 0x4b},
	{0x3708, 0x84},
	{0x3709, 0x40},
	{0x370a, 0x33},
	{0x3711, 0x0f},
	{0x3712, 0x9c},
	{0x3724, 0x01},
	{0x3725, 0x92},
	{0x3726, 0x01},
	{0x3727, 0xc7},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3806, 0x09},
	{0x3807, 0x9b},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x0d},
	{0x380d, 0xbc},
	//{0x380e, 0x05},
	//{0x380f, 0x00},
	{0x3811, 0x08},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x81},
	{0x3821, 0x17},
	{0x3f00, 0x00},
	{0x4005, 0x18},
	{0x4601, 0x00},
	{0x4602, 0x30},
	{0x5068, 0x00},
	{0x506a, 0x00},
	{0x0100, 0x01}, // wake up
	{0x3208, 0x11},
	{OV8825_REG_DELAY, 5},
  
	{0x3208, 0x02}, //group 2
	//3264_2448_2Lane_15fps_68.25Msysclk_689MBps/lane @ 19.5M MCLK
	{0x0100, 0x00}, // sleep     
	{0x3020, 0x81},
	{0x3700, 0x10},
	{0x3702, 0x28},
	{0x3703, 0x6c},
	{0x3704, 0x40},
	{0x3705, 0x19},
	{0x3706, 0x27},
	{0x3708, 0x48},
	{0x3709, 0x20},
	{0x370a, 0x31},
	{0x3711, 0x07},
	{0x3712, 0x4e},
	{0x3724, 0x00},
	{0x3725, 0xd4},
	{0x3726, 0x00},
	{0x3727, 0xf0},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3806, 0x09},
	{0x3807, 0x9b},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x0e},
	{0x380d, 0x00},
	//{0x380e, 0x09},
	//{0x380f, 0xb8},
	{0x3811, 0x10},
	{0x3813, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x80},
	{0x3821, 0x16},
	{0x3f00, 0x02},
	{0x4005, 0x1a}, // BLC triggered every frame
	{0x4601, 0x00},
	{0x4602, 0x20},
	{0x5068, 0x00},
	{0x506a, 0x00},
	{0x0100, 0x01}, // wake up
	{0x3208, 0x12},
	//  #endif
	{0x4202, 0x0f},/* Sensor enter LP11*/
	{0x4800, 0x01},	
	{OV8825_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8825_stream_on[] = {
	{0x4202, 0x00},
	{0x4800, 0x04},

	{OV8825_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8825_stream_off[] = {
	/* Sensor enter LP11*/	
	{0x4202, 0x0f},	
	{0x4800, 0x01},	
	{OV8825_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list ov8825_win_uxga[] = {
	{OV8825_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8825_win_8m[] = {
	{OV8825_REG_END, 0x00},	/* END MARKER */	
};

static int ov8825_read(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov8825_write(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov8825_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV8825_REG_END) {
		if (vals->reg_num == OV8825_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov8825_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#ifdef OV8825_DEBUG_FUNC
static int ov8825_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	//return 0;
	while (vals->reg_num != OV8825_REG_END) {
		if (vals->reg_num == OV8825_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			
			ret = ov8825_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}

#endif
/* R/G and B/G of typical camera module is defined here. */
static unsigned int RG_Ratio_Typical = 0x50;
static unsigned int BG_Ratio_Typical = 0x59;

// index: index of otp group. (0, 1, 2)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data


static int check_otp_wb(struct v4l2_subdev *sd, unsigned short index)
{
	unsigned char flag;
	unsigned short i;
	unsigned short address;
	int ret;
	// select bank 0
	//OV8825_write_i2c(0x3d84, 0x08);
	ov8825_write(sd, 0x3d84, 0x08);
	// read otp into buffer
	//OV8825_write_i2c(0x3d81, 0x01);
	ov8825_write(sd, 0x3d81, 0x01);
	// read flag
	address = 0x3d05 + index*9;
	//flag = OV8825_read_i2c(address);
	ret=ov8825_read( sd, address, &flag);
	// disable otp read
	//OV8825_write_i2c(0x3d81, 0x00);
	ov8825_write(sd, 0x3d81, 0x00);
	printk("check_otp_wb index=%d, flag=%x\n", index, flag);
	// clear otp buffer
	for (i=0;i<32;i++) {
	//OV8825_write_i2c(0x3d00 + i, 0x00);
	ov8825_write(sd, 0x3d00 + i, 0x00);
	}
	
	if (!flag) {
		return 0;
	}
	else if ((!(flag & 0x80)) && (flag & 0x7f)) {
		return 2;
	}
	else {
		return 1;
	}
}

// index: index of otp group. (0, 1, 2)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data

static int check_otp_lenc(struct v4l2_subdev *sd, unsigned short index)
{
	unsigned char flag;
	unsigned short i;
	unsigned short address;
	int ret;
	// select bank: index*2+1
	//OV8825_write_i2c(0x3d84, 0x09 + index*2);
	ov8825_write(sd, 0x3d84, 0x09 + index*2);
	// read otp into buffer
	//OV8825_write_i2c(0x3d81, 0x01);
	ov8825_write(sd, 0x3d81, 0x01);
	// read flag
	address = 0x3d00;
	//flag = OV8825_read_i2c(address);
	ret=ov8825_read( sd, address, &flag);
	flag = flag & 0xc0;
	// disable otp read
	//OV8825_write_i2c(0x3d81, 0x00);
	ov8825_write(sd, 0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<32;i++) {
	//OV8825_write_i2c(0x3d00 + i, 0x00);
	ov8825_write(sd, 0x3d00 + i, 0x00);
	}
	if (!flag) {
		return 0;
	}
	else if ((!(flag & 0x80)) && (flag & 0x7f)) {
		return 2;
	}
	else {
		return 1;
	}
}


// index: index of otp group. (0, 1, 2)
// otp_ptr: pointer of otp_struct
// return: 0,
static int read_otp_wb(struct v4l2_subdev *sd, unsigned short index , struct otp_struct *otp_ptr)
{
	unsigned char flag;
	unsigned short i;
	unsigned short address;
	int ret;
	// select bank 0
	//OV8825_write_i2c(0x3d84, 0x08);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ read_otp_wb \n");
	ov8825_write(sd, 0x3d84, 0x08);
	// read otp into buffer
	//OV8825_write_i2c(0x3d81, 0x01);
	ov8825_write(sd, 0x3d81, 0x01);
	
	address = 0x3d05 + index*9;
	//(*otp_ptr).customer_id = (OV8825_read_i2c(address) & 0x7f);
	ret = ov8825_read(sd, address, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).customer_id = (unsigned int)(flag&0x7f);
	
	//(*otp_ptr).module_integrator_id = OV8825_read_i2c(address);
	ret = ov8825_read(sd, address, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).module_integrator_id = (unsigned int)flag;
	
	//(*otp_ptr).lens_id = OV8825_read_i2c(address + 1);
	ret = ov8825_read(sd, address+1, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).lens_id=(unsigned int)flag;
	
	//(*otp_ptr).rg_ratio = OV8825_read_i2c(address + 2);
	ret = ov8825_read(sd, address+2, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).rg_ratio=(unsigned int)flag;
	
	//(*otp_ptr).bg_ratio = OV8825_read_i2c(address + 3);
	ret = ov8825_read(sd, address+3, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).bg_ratio=(unsigned int)flag;

	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ otp.rg_ratio=%d. \n", (*otp_ptr).rg_ratio);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ otp.bg_ratio=%d. \n", (*otp_ptr).bg_ratio);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ rg_ratio_typical=%d. \n", RG_Ratio_Typical);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ bg_ratio_typical=%d. \n", BG_Ratio_Typical);
	
	//(*otp_ptr).user_data[0] = OV8825_read_i2c(address + 4);
	ret = ov8825_read(sd, address+4, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).user_data[0]=(unsigned int)flag;
	
	//(*otp_ptr).user_data[1] = OV8825_read_i2c(address + 5);
	ret = ov8825_read(sd, address+5, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).user_data[1]=(unsigned int)flag;
	
	//(*otp_ptr).user_data[2] = OV8825_read_i2c(address + 6);
	ret = ov8825_read(sd, address+6, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).user_data[2]=(unsigned int)flag;
	
	//(*otp_ptr).light_rg = OV8825_read_i2c(address + 7);
	ret = ov8825_read(sd, address+7, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).light_rg=(unsigned int)flag;
	
	//(*otp_ptr).light_bg = OV8825_read_i2c(address + 8);
	ret = ov8825_read(sd, address+8, &flag);
	if (ret < 0)
		return ret;
	(*otp_ptr).light_bg=(unsigned int)flag;
	// disable otp read
	//OV8825_write_i2c(0x3d81, 0x00);
	ov8825_write(sd, 0x3d81, 0x00);
	
	// clear otp buffer
	for (i=0;i<32;i++) {
	//OV8825_write_i2c(0x3d00 + i, 0x00);
	ov8825_write(sd, 0x3d00 + i, 0x00);
	}
	return 0;
}

// index: index of otp group. (0, 1, 2)
// otp_ptr: pointer of otp_struct
// return: 0,
int read_otp_lenc(struct v4l2_subdev *sd, unsigned short index,  struct otp_struct *otp_ptr)
{
	unsigned short bank, i;
	unsigned short address;
	unsigned char flag;
	int ret;
	// select bank: index*2+1
	bank = index*2 + 1;
	//OV8825_write_i2c(0x3d84, bank + 0x08);
	ov8825_write(sd, 0x3d84, bank + 0x08);
	// read otp into buffer
	//OV8825_write_i2c(0x3d81, 0x01);
	ov8825_write(sd, 0x3d81, 0x01);
	address = 0x3d01;
	for(i=0;i<31;i++) {
	//(* otp_ptr).lenc[i]=OV8825_read_i2c(address);
	ret = ov8825_read(sd, address, &flag);
	if (ret < 0)
		return ret;
	(* otp_ptr).lenc[i]=(int)flag;
	address++;
	}
	
	// disable otp read
	//OV8825_write_i2c(0x3d81, 0x00);
	ov8825_write(sd, 0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<32;i++) {
	//OV8825_write_i2c(0x3d00 + i, 0x00);
	ov8825_write(sd, 0x3d00 + i, 0x00);
	}
	// select next bank
	bank++;
	//OV8825_write_i2c(0x3d84, bank + 0x08);
	ov8825_write(sd, 0x3d84, bank + 0x08);
	// read otp
	//OV8825_write_i2c(0x3d81, 0x01);
	ov8825_write(sd, 0x3d81, 0x01);
	address = 0x3d00;
	for(i=31;i<62;i++) {
	//(* otp_ptr).lenc[i]=OV8825_read_i2c(address);
	ret = ov8825_read(sd, address, &flag);
	if (ret < 0)
		return ret;
	(* otp_ptr).lenc[i]=(int)flag;
	address++;
	}
	// disable otp read
	//OV8825_write_i2c(0x3d81, 0x00);
	ov8825_write(sd, 0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<32;i++) {
	//OV8825_write_i2c(0x3d00 + i, 0x00);
	ov8825_write(sd, 0x3d00 + i, 0x00);
	}
	return 0;
}

// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
int update_awb_gain(struct v4l2_subdev *sd, int R_gain, int G_gain, int B_gain )
{
	if (R_gain>0x400) {
	//OV8825_write_i2c(0x3400, R_gain>>8);
	ov8825_write(sd, 0x3400, R_gain>>8);
	//OV8825_write_i2c(0x3401, R_gain & 0x00ff);
	ov8825_write(sd, 0x3401, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
	//OV8825_write_i2c(0x3402, G_gain>>8);
	ov8825_write(sd, 0x3402, G_gain>>8);
	//OV8825_write_i2c(0x3403, G_gain & 0x00ff);
	ov8825_write(sd, 0x3403, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
	//OV8825_write_i2c(0x3404, B_gain>>8);
	ov8825_write(sd, 0x3404, B_gain>>8);
	//OV8825_write_i2c(0x3405, B_gain & 0x00ff);
	ov8825_write(sd, 0x3405, B_gain & 0x00ff);
	}
	return 0;
}

// otp_ptr: pointer of otp_struct
int update_lenc(struct v4l2_subdev *sd,struct otp_struct * otp_ptr)
{
	unsigned short i;
	unsigned char temp;
	temp = 0x80 | (*otp_ptr).lenc[0];
	//OV8825_write_i2c(0x5800, temp);
	ov8825_write(sd, 0x5800, temp);
	
	for(i=0;i<62;i++) {
	//OV8825_write_i2c(0x5800 + i, (*otp_ptr).lenc[i]);
	ov8825_write(sd, 0x5800 + i, (*otp_ptr).lenc[i]);
	}
	return 0;
}

// call this function after OV8820 initialization
// return value: 0 update success
// 1, no OTP
int update_otp_wb(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	unsigned short i;
	unsigned short otp_index;
	unsigned short temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;

	current_otp.light_bg = 0;
	current_otp.light_rg = 0;
	current_otp.rg_ratio = 0;
	current_otp.bg_ratio = 0;

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=0;i<3;i++) {
	temp = check_otp_wb(sd, i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i==3) {
	// no valid wb OTP data
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ no valid wb OTP data \n");
	return 1;
	}
	read_otp_wb(sd, otp_index, &current_otp);
	if(current_otp.light_rg==0) {
	// no light source information in OTP, light factor = 1
	rg = current_otp.rg_ratio;
	}
	else {
	rg = current_otp.rg_ratio * ((current_otp.light_rg +128) / 256);
	}
	if(current_otp.light_bg==0) {
	// not light source information in OTP, light factor = 1
	bg = current_otp.bg_ratio;
	}
	else {
	bg = current_otp.bg_ratio * ((current_otp.light_bg +128) / 256);
	}
	//calculate G gain
	//0x400 = 1x gain
	if(bg < BG_Ratio_Typical) {
	if (rg< RG_Ratio_Typical) {
	// current_otp.bg_ratio < BG_Ratio_typical &&
	// current_otp.rg_ratio < RG_Ratio_typical
	G_gain = 0x400;
	B_gain = 0x400 * BG_Ratio_Typical / bg;
	R_gain = 0x400 * RG_Ratio_Typical / rg;
	}
	else {
	// current_otp.bg_ratio < BG_Ratio_typical &&
	// current_otp.rg_ratio >= RG_Ratio_typical
	R_gain = 0x400;
	G_gain = 0x400 * rg / RG_Ratio_Typical;
	B_gain = G_gain * BG_Ratio_Typical /bg;
	}
	}
	else {
	if (rg < RG_Ratio_Typical) {
	// current_otp.bg_ratio >= BG_Ratio_typical &&
	// current_otp.rg_ratio < RG_Ratio_typical
	B_gain = 0x400;
	G_gain = 0x400 * bg / BG_Ratio_Typical;
	R_gain = G_gain * RG_Ratio_Typical / rg;
	}
	else {
	// current_otp.bg_ratio >= BG_Ratio_typical &&
	// current_otp.rg_ratio >= RG_Ratio_typical
	G_gain_B = 0x400 * bg / BG_Ratio_Typical;
	G_gain_R = 0x400 * rg / RG_Ratio_Typical;
	if(G_gain_B > G_gain_R ) {
	B_gain = 0x400;
	G_gain = G_gain_B;
	R_gain = G_gain * RG_Ratio_Typical /rg;
	}
	else {
	R_gain = 0x400;
	G_gain = G_gain_R;
	B_gain = G_gain * BG_Ratio_Typical / bg;
	}
	}
	}
	update_awb_gain(sd, R_gain, G_gain, B_gain);
	return 0;
}

// call this function after OV8820 initialization
// return value: 0 update success
// 1, no OTP
int update_otp_lenc(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	unsigned short i;
	unsigned short otp_index;
	unsigned char temp;
	// check first lens correction OTP with valid data
	for(i=0;i<3;i++) {
	temp = check_otp_lenc(sd, i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i==3) {
	// no valid wb OTP data
	return 1;
	}
	read_otp_lenc(sd, otp_index, &current_otp);
	update_lenc(sd, &current_otp);
	// success
	return 0;
}

static int ov8825_reset(struct v4l2_subdev *sd, u32 val)
{
	printk("*ov8825_reset*\n");
	return 0;
}

static int ov8825_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov8825_info *info = container_of(sd, struct ov8825_info, sd);
	int ret=0;

	printk("*ov8825_init*\n");
	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->frame_rate = FRAME_RATE_DEFAULT;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 2;
	info->test = 0;

	ret=ov8825_write_array(sd, ov8825_init_regs);

	if (ret < 0)
		return ret;

	printk("*update_otp_wb*\n");
	ret = update_otp_wb(sd);
	if (ret < 0)
		return ret;
	printk("*update_otp_lenc*\n");
	ret = update_otp_lenc(sd);
	return ret;
}

static int ov8825_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	printk("*ov8825_detect start*\n");
	ret = ov8825_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != OV8825_CHIP_ID_H)
		return -ENODEV;
	ret = ov8825_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_M=0x%x \n", v);
	if (v != OV8825_CHIP_ID_M)
		return -ENODEV;
	
	
	/*

	ret = ov8825_read(sd, 0x3608, &v);//detect internal or external DVDD
	printk("r3608=0x%x\n ", v);
	
	if (ret < 0)
		return ret;
	if (v != OV8825_CHIP_ID_L)
		return -ENODEV;
	*/
	return 0;
}

static struct ov8825_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov8825_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV8825_FMTS ARRAY_SIZE(ov8825_formats)

static struct ov8825_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
} ov8825_win_sizes[] = {
	/* UXGA */
	{
		.width		= UXGA_WIDTH,
		.height		= UXGA_HEIGHT,
		.vts		= 0x500,
		.framerate	= 30,
		.max_gain_dyn_frm = 0x80,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x80,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov8825_win_uxga,
	},
	/* 3200*2400 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x9b8,
		.framerate	= 15,
		.max_gain_dyn_frm = 0x80,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x80,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov8825_win_8m,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(ov8825_win_sizes))

static int ov8825_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	printk("[TEST] ov8825_enum_mbus_fmt\n");
	if (index >= N_OV8825_FMTS)
		return -EINVAL;

	
	*code = ov8825_formats[index].mbus_code;
	return 0;
}

static int ov8825_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov8825_format_struct **ret_fmt,
		struct ov8825_win_size **ret_wsize)
{
	int index;
	struct ov8825_win_size *wsize;

	printk("ov8825_try_fmt_internal N_OV8825_FMTS=%d\n", N_OV8825_FMTS);
	for (index = 0; index < N_OV8825_FMTS; index++)
		if (ov8825_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV8825_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov8825_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov8825_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov8825_win_sizes; wsize < ov8825_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov8825_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov8825_formats[index].colorspace;
	return 0;
}

static int ov8825_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	printk("[TEST] ov8825_try_mbus_fmt\n");
	return ov8825_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov8825_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov8825_info *info = container_of(sd, struct ov8825_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov8825_format_struct *fmt_s;
	struct ov8825_win_size *wsize;
	int framerate = 0;
	int ret;

	printk("[TEST] ov8825_s_mbus_fmt\n");
	ret = ov8825_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	printk("[TEST] ov8825_try_fmt_internal, ret=%d, width=%d, height=%d, vts=%x\n", ret, wsize->width, wsize->height, wsize->vts);
	if (ret)
		return ret;

	data->vts = wsize->vts;
	data->mipi_clk = 689; /* Mbps. */

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov8825_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	printk("[TEST] ov8825_write_array ret=%d\n",ret);
	if ((info->win != wsize) && wsize->regs) {
		//ret = ov8825_write_array(sd, wsize->regs);
		//if (ret)
			//return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 689; /* Mbps. */
		data->sensor_vts_address1 = info->vts_address1;
		data->sensor_vts_address2 = info->vts_address2;
		data->framerate = wsize->framerate;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;
		data->binning = info->binning;
		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa2;
			if (info->frame_rate != FRAME_RATE_AUTO) {
				data->reg[1].addr = info->vts_address1;
				data->reg[1].data = data->vts >> 8;
				data->reg[2].addr = info->vts_address2;
				data->reg[2].data = data->vts & 0x00ff;
				data->reg_num = 3;
			}
		} else if ((wsize->width == UXGA_WIDTH)
			&& (wsize->height == UXGA_HEIGHT)) {
			if (!info->test) {
				info->test = 1;
				//ov8825_write(sd, 0x3208, 0xa2);
				msleep(100);
			}
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa1;
			if (info->frame_rate != FRAME_RATE_AUTO) {
				if (info->frame_rate == FRAME_RATE_DEFAULT)
					framerate = wsize->framerate;
				else framerate = info->frame_rate;
				data->framerate = framerate;
				data->vts = (data->vts * wsize->framerate) / framerate;
				data->reg[1].addr = info->vts_address1;
				data->reg[1].data = data->vts >> 8;
				data->reg[2].addr = info->vts_address2;
				data->reg[2].data = data->vts & 0x00ff;
				data->reg_num = 3;
			}
		}
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov8825_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	printk("ov8825_s_stream=%d\n", enable);
	if (enable)
		ret = ov8825_write_array(sd, ov8825_stream_on);
	else
		ret = ov8825_write_array(sd, ov8825_stream_off);
	#if 0
	else
		ov8825_read_array(sd,ov8825_init_regs );//debug only
	#endif
	return ret;
}

static int ov8825_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	printk("[TEST] ov8825_g_crop\n");
	return 0;
}

static int ov8825_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

	printk("[TEST] ov8825_cropcap\n");
	return 0;
}

static int ov8825_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	printk("[TEST] ov8825_g_parm\n");
	return 0;
}

static int ov8825_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	printk("[TEST] ov8825_s_parm\n");
	return 0;
}

static int ov8825_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov8825_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	printk("[TEST] ov8825_enum_frameintervals\n");
	if (interval->index >= ARRAY_SIZE(ov8825_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov8825_frame_rates[interval->index];
	return 0;
}

static int ov8825_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	 printk("[TEST] ov8825_enum_framesizes\n");
	for (i = 0; i < N_WIN_SIZES; i++) {
		struct ov8825_win_size *win = &ov8825_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov8825_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	printk("[TEST] ov8825_queryctrl\n");
	return 0;
}

static int ov8825_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov8825_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov8825_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov8825_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov8825_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct ov8825_info *info = container_of(sd, struct ov8825_info, sd);
	printk("ov8825_set_framerate %d\n", framerate);
	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;
	return 0;
}


static int ov8825_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		ret = ov8825_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD; 
	}
	
	return ret;
}

static int ov8825_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	printk("[TEST] ov8825_g_chip_ident\n");
	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV8825, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov8825_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov8825_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov8825_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov8825_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov8825_core_ops = {
	.g_chip_ident = ov8825_g_chip_ident,
	.g_ctrl = ov8825_g_ctrl,
	.s_ctrl = ov8825_s_ctrl,
	.queryctrl = ov8825_queryctrl,
	.reset = ov8825_reset,
	.init = ov8825_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov8825_g_register,
	.s_register = ov8825_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov8825_video_ops = {
	.enum_mbus_fmt = ov8825_enum_mbus_fmt,
	.try_mbus_fmt = ov8825_try_mbus_fmt,
	.s_mbus_fmt = ov8825_s_mbus_fmt,
	.s_stream = ov8825_s_stream,
	.cropcap = ov8825_cropcap,
	.g_crop	= ov8825_g_crop,
	.s_parm = ov8825_s_parm,
	.g_parm = ov8825_g_parm,
	.enum_frameintervals = ov8825_enum_frameintervals,
	.enum_framesizes = ov8825_enum_framesizes,
};

static const struct v4l2_subdev_ops ov8825_ops = {
	.core = &ov8825_core_ops,
	.video = &ov8825_video_ops,
};

static int ov8825_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov8825_info *info;
	int ret;

	printk("[TEST] ov8825_probe\n");
	info = kzalloc(sizeof(struct ov8825_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov8825_ops);

	/* Make sure it's an ov8825 */
	ret = ov8825_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov8825 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov8825 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int ov8825_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8825_info *info = container_of(sd, struct ov8825_info, sd);

	printk("[TEST] ov8825_remove\n");
	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov8825_id[] = {
	{ "ov8825", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov8825_id);

static struct i2c_driver ov8825_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov8825",
	},
	.probe		= ov8825_probe,
	.remove		= ov8825_remove,
	.id_table	= ov8825_id,
};

static __init int init_ov8825(void)
{
	return i2c_add_driver(&ov8825_driver);
}

static __exit void exit_ov8825(void)
{
	i2c_del_driver(&ov8825_driver);
}

fs_initcall(init_ov8825);
module_exit(exit_ov8825);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov8825 sensors");
MODULE_LICENSE("GPL");
