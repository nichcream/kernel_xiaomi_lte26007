
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
#include "ov5693.h"


#define OV5693_CHIP_ID_H	(0x56)
#define OV5693_CHIP_ID_L	(0x90)

#define SXGA_WIDTH		1296//1280
#define SXGA_HEIGHT		972//960
#define MAX_WIDTH		2592//2624
#define MAX_HEIGHT		1944//1956
#define MAX_PREVIEW_WIDTH SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT SXGA_HEIGHT


#define OV5693_REG_END		0xffff
#define OV5693_REG_DELAY	0xfffe

struct ov5693_format_struct;

struct ov5693_info {
	struct v4l2_subdev sd;
	struct ov5693_format_struct *fmt;
	struct ov5693_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int dynamic_framerate;
	int binning;
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

static struct regval_list ov5693_init_regs[] = {
{0x0103,0x01},                           
{0x3001,0x0a},                           
{0x3002,0x80},                           
{0x3006,0x00},                           
{0x3011,0x21},                           
{0x3012,0x09},                           
{0x3013,0x10},                           
{0x3014,0x00},                           
{0x3015,0x08},                           
{0x3016,0xf0},                           
{0x3017,0xf0},                           
{0x3018,0xf0},                           
{0x301b,0xb4},                           
{0x301d,0x02},                           
{0x3021,0x00},                           
{0x3022,0x01},                           
{0x3028,0x44},                           
{0x3098,0x02},//pll                      
{0x3099,0x1a},                           
{0x309a,0x02},                           
{0x309b,0x02},                           
{0x309c,0x00},                           
{0x30a0,0xd2},                           
{0x30a2,0x01},                           
{0x30b2,0x00},                           
{0x30b3,0x7e},//pll                      
{0x30b4,0x03},                           
{0x30b5,0x04},                           
{0x30b6,0x01},                           
{0x3104,0x21},                           
{0x3106,0x00},                           
{0x3400,0x04},                           
{0x3401,0x00},                           
{0x3402,0x04},                           
{0x3403,0x00},                           
{0x3404,0x04},                           
{0x3405,0x00},                           
{0x3406,0x01},                           
{0x3500,0x00},                           
{0x3501,0x3d},                           
{0x3502,0x00},                           
{0x3503,0x27},                           
{0x3504,0x00},                           
{0x3505,0x00},                           
{0x3506,0x00},                           
{0x3507,0x02},                           
{0x3508,0x00},                           
{0x3509,0x10},                           
{0x350a,0x00},                           
{0x350b,0x40},                           
{0x3601,0x0a},                           
{0x3602,0x38},                           
{0x3612,0x80},                           
{0x3620,0x54},                           
{0x3621,0xc7},                           
{0x3622,0x0f},                           
{0x3625,0x10},                           
{0x3630,0x55},                           
{0x3631,0xf4},                           
{0x3632,0x00},                           
{0x3633,0x34},                           
{0x3634,0x02},                           
{0x364d,0x0d},                           
{0x364f,0xdd},                           
{0x3660,0x04},                           
{0x3662,0x10},                           
{0x3663,0xf1},                           
{0x3665,0x00},                           
{0x3666,0x20},                           
{0x3667,0x00},                           
{0x366a,0x80},                           
{0x3680,0xe0},                           
{0x3681,0x00},                           
{0x3700,0x42},                           
{0x3701,0x14},                           
{0x3702,0xa0},                           
{0x3703,0xd8},                           
{0x3704,0x78},                           
{0x3705,0x02},                           
{0x3708,0xe6},                           
{0x3709,0xc7},                           
{0x370a,0x00},                           
{0x370b,0x20},                           
{0x370c,0x0c},                           
{0x370d,0x11},                           
{0x370e,0x00},                           
{0x370f,0x40},                           
{0x3710,0x00},                           
{0x371a,0x1c},                           
{0x371b,0x05},                           
{0x371c,0x01},                           
{0x371e,0xa1},                           
{0x371f,0x0c},                           
{0x3721,0x00},                           
{0x3724,0x10},                           
{0x3726,0x00},                           
{0x372a,0x01},                           
{0x3730,0x10},                           
{0x3738,0x22},                           
{0x3739,0xe5},                           
{0x373a,0x50},                           
{0x373b,0x02},                           
{0x373c,0x41},                           
{0x373f,0x02},                           
{0x3740,0x42},                           
{0x3741,0x02},                           
{0x3742,0x18},                           
{0x3743,0x01},                           
{0x3744,0x02},                           
{0x3747,0x10},                           
{0x374c,0x04},                           
{0x3751,0xf0},                           
{0x3752,0x00},                           
{0x3753,0x00},                           
{0x3754,0xc0},                           
{0x3755,0x00},                           
{0x3756,0x1a},                           
{0x3758,0x00},                           
{0x3759,0x0f},                           
{0x376b,0x44},                           
{0x375c,0x04},                           
{0x3774,0x10},                           
{0x3776,0x00},                           
{0x377f,0x08},                           
{0x3780,0x22},                           
{0x3781,0x0c},                           
{0x3784,0x2c},                           
{0x3785,0x1e},                           
{0x378f,0xf5},                           
{0x3791,0xb0},                           
{0x3795,0x00},                           
{0x3796,0x64},                           
{0x3797,0x11},                           
{0x3798,0x30},                           
{0x3799,0x41},                           
{0x379a,0x07},                           
{0x379b,0xb0},                           
{0x379c,0x0c},                           
{0x37c5,0x00},                           
{0x37c6,0x00},                           
{0x37c7,0x00},                           
{0x37c9,0x00},                           
{0x37ca,0x00},                           
{0x37cb,0x00},                           
{0x37de,0x00},                           
{0x37df,0x00},                           
{0x3800,0x00},                           
{0x3801,0x00},                           
{0x3802,0x00},                           
{0x3803,0x00},                           
{0x3804,0x0a},                           
{0x3805,0x3f},                           
{0x3806,0x07},                           
{0x3807,0xa3},                           
{0x3808,0x05},                           
{0x3809,0x10},                           
{0x380a,0x03},                           
{0x380b,0xcc},                           
{0x380c,0x0a},                           
{0x380d,0x80},                           
{0x380e,0x05},                           
{0x380f,0xd0},                           
{0x3810,0x00},                           
{0x3811,0x02},                           
{0x3812,0x00},                           
{0x3813,0x02},                           
{0x3814,0x31},                           
{0x3815,0x31},                           
{0x3820,0x04},                           
{0x3821,0x1f},                           
{0x3823,0x00},                           
{0x3824,0x00},                           
{0x3825,0x00},                           
{0x3826,0x00},                           
{0x3827,0x00},                           
{0x382a,0x04},                           
{0x3a04,0x06},                           
{0x3a05,0x14},                           
{0x3a06,0x00},                           
{0x3a07,0xfe},                           
{0x3b00,0x00},                           
{0x3b02,0x00},                           
{0x3b03,0x00},                           
{0x3b04,0x00},                           
{0x3b05,0x00},                           
{0x3e07,0x20},                           
{0x4000,0x08},                           
{0x4001,0x04},                           
{0x4002,0x45},                           
{0x4004,0x08},                           
{0x4005,0x18},                           
{0x4006,0x20},                           
{0x4008,0x24},                           
{0x4009,0x40},                           
{0x400c,0x00},                           
{0x400d,0x00},                           
{0x4058,0x00},                           
{0x404e,0x37},                           
{0x404f,0x8f},                           
{0x4058,0x00},                           
{0x4101,0xb2},                           
{0x4303,0x00},                           
{0x4304,0x08},                           
{0x4307,0x30},                           
{0x4311,0x04},                           
{0x4315,0x01},                           
{0x4511,0x05},                           
{0x4512,0x01},                           
{0x4806,0x00},                           
{0x4816,0x52},                           
{0x481f,0x30},                           
{0x4826,0x2c},                           
{0x4831,0x64},                           
{0x4d00,0x04},                           
{0x4d01,0x71},                           
{0x4d02,0xfd},                           
{0x4d03,0xf5},                           
{0x4d04,0x0c},                           
{0x4d05,0xcc},                           
{0x4837,0x0a},//pll                      
{0x5000,0x06},                           
{0x5001,0x01},                           
{0x5002,0x00},                           
{0x5003,0x20},                           
{0x5046,0x0a},                           
{0x5013,0x00},                           
{0x5046,0x0a},                           
{0x5780,0x1c},                           
{0x5786,0x20},                           
{0x5787,0x10},                           
{0x5788,0x18},                           
{0x578a,0x04},                           
{0x578b,0x02},                           
{0x578c,0x02},                           
{0x578e,0x06},                           
{0x578f,0x02},                           
{0x5790,0x02},                           
{0x5791,0xff},                           
{0x5842,0x01},                           
{0x5843,0x2b},                           
{0x5844,0x01},                           
{0x5845,0x92},                           
{0x5846,0x01},                           
{0x5847,0x8f},                           
{0x5848,0x01},                           
{0x5849,0x0c},                           
{0x5e00,0x00},                           
{0x5e10,0x0c},                           
{0x0100,0x01},                           
//preview setting                        
{0x0100,0x00},                           
{0x3708,0xe6},                           
{0x3709,0xc7},                           
{0x3800,0x00},                           
{0x3801,0x00},                           
{0x3802,0x00},                           
{0x3803,0x00},                           
{0x3804,0x0a},                           
{0x3805,0x3f},                           
{0x3806,0x07},                           
{0x3807,0xa3},                           
{0x3808,0x05},                           
{0x3809,0x10},                           
{0x380a,0x03},                           
{0x380b,0xcc},                           
{0x380c,0x0a},                           
{0x380d,0x80},                           
{0x380e,0x04}, //03                      
{0x380f,0x18}, //e8                      
{0x3814,0x31},                           
{0x3815,0x31},                           
{0x3820,0x04},                           
{0x3821,0x1f},                           
{0x3a04,0x06},                           
{0x3a05,0x14},                           
{0x5002,0x00},                           
{0x0100,0x01},                           
                                         
{OV5693_REG_DELAY,0x20},                           
                                         
//preview setting // 1280x960 30fps 2 lane
{0x3200, 0x00}, // group 1 start addr 0  
{0x3208, 0x00}, // enter group 1 write mode
{0x3708,0xe6},                           
{0x3709,0xc7},                           
{0x3800,0x00},                           
{0x3801,0x00},                           
{0x3802,0x00},                           
{0x3803,0x00},                           
{0x3804,0x0a},                           
{0x3805,0x3f},                           
{0x3806,0x07},                           
{0x3807,0xa3},                           
{0x3808,0x05},                           
{0x3809,0x10},                           
{0x380a,0x03},                           
{0x380b,0xcc},                           
{0x380c,0x20},  //0a                         
{0x380d,0x80},  //80                         
{0x380e,0x04}, //03                      
{0x380f,0x18}, //e8                      
{0x3814,0x31},                           
{0x3815,0x31},                           
{0x3820,0x04},                           
{0x3821,0x1f}, //mirror on                          
{0x3a04,0x06},                           
{0x3a05,0x14},                           
{0x5002,0x00},                           
{0x3208, 0x10}, // EXIT group 1 write mode
                                         
{OV5693_REG_DELAY,0x05},                           
                                         
//capture setting // 2692x1944 15fps 2 lane
{0x3202, 0x08}, // group 2 start addr 0x8
{0x3208, 0x02}, // enter group 2 write mode
//capture setting                        
{0x3708,0xe2},                           
{0x3709,0xc3},                           
{0x3808,0x0a},//0xa40                    
{0x3809,0x20},                           
{0x380a,0x07},                           
{0x380b,0x98},//0xa4                     
{0x380c,0x0a},                           
{0x380d,0x80},                           
{0x380e,0x08}, //08                      
{0x380f,0x30}, //vts for 15fps           
{0x3814,0x11},                           
{0x3815,0x11},                           
{0x3820,0x00},                           
{0x3821,0x1e},     //mirror on                      
{0x3a04,0x06},                           
{0x3a05,0x14},                           
{0x5002,0x00},                           
{0x3208,0x12}, // EXIT group 2 write mode
{0x4202, 0x0f},
{0x4800, 0x01},
{OV5693_REG_END, 0x00}, /* END MARKER */

};

static struct regval_list ov5693_stream_on[] = {
	{0x4202, 0x00},
	{0x4800, 0x24},

	{OV5693_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5693_stream_off[] = {
	/* Sensor enter LP11*/
	{0x4202, 0x0f},
	{0x4800, 0x01},
	{OV5693_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5693_win_sxga[] = {
	{OV5693_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5693_win_5m[] = {
	{OV5693_REG_END, 0x00},	/* END MARKER */
};

static int ov5693_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov5693_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov5693_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV5693_REG_END) {
		if (vals->reg_num == OV5693_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov5693_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical = 0x55;
static unsigned int bg_ratio_typical = 0x61;

/*
	index: index of otp group. (0, 1, 2)
 	return: 0, group index is empty
 	1, group index has invalid data
 	2, group index has valid data
*/
static int ov5693_check_otp(struct v4l2_subdev *sd, unsigned short index)
{
	unsigned char temp;
	unsigned short i;
	unsigned short address;
	int ret;

	// read otp into buffer
	ov5693_write(sd, 0x3d21, 0x01);
	mdelay(5);
	address = 0x3d05 + index * 9;
	ret = ov5693_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	
	// disable otp read
	ov5693_write(sd, 0x3d21, 0x00);
	
	// clear otp buffer
	for (i = 0; i < 32; i++)
		ov5693_write(sd, 0x3d00 + i, 0x00);
	
	if (!temp)
		return 0;
	else if (!(temp & 0x80) && (temp & 0x7f))
		return 2;
	else
		return 1;
}

/*
	index: index of otp group. (0, 1, 2)
	return: 0,
*/
static int ov5693_read_otp(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp)
{
	unsigned char temp;
	unsigned short address;
	unsigned short i;
	int ret;

	// read otp into buffer
	ov5693_write(sd, 0x3d21, 0x01);
	mdelay(5);

	address = 0x3d05 + index * 9;

	ret = ov5693_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	otp->customer_id = (unsigned int)(temp & 0x7f);

	ret = ov5693_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	otp->module_integrator_id = (unsigned int)temp;

	ret = ov5693_read(sd, address + 1, &temp);
	if (ret < 0)
		return ret;
	otp->lens_id = (unsigned int)temp;

	ret = ov5693_read(sd, address + 2, &temp);
	if (ret < 0)
		return ret;
	otp->rg_ratio = (unsigned int)temp;

	ret = ov5693_read(sd, address + 3, &temp);
	if (ret < 0)
		return ret;
	otp->bg_ratio = (unsigned int)temp;

	for (i = 0; i<= 4; i++) {
		ret = ov5693_read(sd, address + 4 + i, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[i] = (unsigned int)temp;
	}

	// disable otp read
	ov5693_write(sd, 0x3d21, 0x00);

	// clear otp buffer
	for (i = 0; i < 32; i++)
		ov5693_write(sd, 0x3d00 + i, 0x00);

	return 0;
}

/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov5693_update_awb_gain(struct v4l2_subdev *sd, 
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	if (R_gain > 0x400) {
		ov5693_write(sd, 0x5186, (unsigned char)(R_gain >> 8));
		ov5693_write(sd, 0x5187, (unsigned char)(R_gain & 0x00ff));
	}

	if (G_gain > 0x400) {
		ov5693_write(sd, 0x5188, (unsigned char)(G_gain >> 8));
		ov5693_write(sd, 0x5189, (unsigned char)(G_gain & 0x00ff));
	}

	if (B_gain > 0x400) {
		ov5693_write(sd, 0x518a, (unsigned char)(B_gain >> 8));
		ov5693_write(sd, 0x518b, (unsigned char)(B_gain & 0x00ff));
	}

	return 0;
}

/*
	call this function after OV5693 initialization
	return value: 0 update success
	1, no OTP
*/
static int ov5693_update_otp(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	unsigned int i;
	unsigned int otp_index;
	unsigned int temp;
	unsigned int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int ret = 0;

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for (i = 0; i < 3; i++) {
		temp = ov5693_check_otp(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}

	if (i == 3)
		// no valid wb OTP data
		return 1;

	ret = ov5693_read_otp(sd, otp_index, &current_otp);
	if (ret < 0)
		return ret;

	//calculate G gain
	//0x400 = 1x gain
	if (current_otp.bg_ratio < bg_ratio_typical) {
		if (current_otp.rg_ratio < rg_ratio_typical) {
			// current_otp.bg_ratio < bg_ratio_typical &&
			// current_otp.rg_ratio < rg_ratio_typical
			G_gain = 0x400;
			B_gain = 0x400 * bg_ratio_typical / current_otp.bg_ratio;
			R_gain = 0x400 * rg_ratio_typical / current_otp.rg_ratio;
		} else {
			// current_otp.bg_ratio < bg_ratio_typical &&
			// current_otp.rg_ratio >= rg_ratio_typical
			R_gain = 0x400;
			G_gain = (0x400 * current_otp.rg_ratio) / rg_ratio_typical;
			B_gain = (G_gain * bg_ratio_typical) / current_otp.bg_ratio;
				}
		} 
	else {
		if (current_otp.rg_ratio < rg_ratio_typical) {
			// current_otp.bg_ratio >= bg_ratio_typical &&
			// current_otp.rg_ratio < rg_ratio_typical
			B_gain = 0x400;
			G_gain = 0x400 * current_otp.bg_ratio / bg_ratio_typical;
			R_gain = G_gain * rg_ratio_typical / current_otp.rg_ratio;
		} else {
			// current_otp.bg_ratio >= bg_ratio_typical &&
			// current_otp.rg_ratio >= rg_ratio_typical
			G_gain_B = 0x400 * current_otp.bg_ratio / bg_ratio_typical;
			G_gain_R = 0x400 * current_otp.rg_ratio / rg_ratio_typical;
			if (G_gain_B > G_gain_R ) {
				B_gain = 0x400;
				G_gain = G_gain_B;
				R_gain = G_gain * rg_ratio_typical / current_otp.rg_ratio;
			} else {
				R_gain = 0x400;
				G_gain = G_gain_R;
				B_gain = G_gain * bg_ratio_typical / current_otp.bg_ratio;
			}
		}
	}

	ov5693_update_awb_gain(sd, R_gain, G_gain, B_gain);

	return 0;
}

static int ov5693_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov5693_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov5693_info *info = container_of(sd, struct ov5693_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->dynamic_framerate = 0;

#ifndef SETTING_LOAD
	ret = ov5693_write_array(sd, ov5693_init_regs);
#else
	comip_debugtool_load_sensor_setting(COMIP_DEBUGTOOL_SENSORSETTING_FILENAME,init_setting);
	ret=ov5693_write_array(sd, init_setting);
#endif

	if (ret < 0)
		return ret;

	ret = ov5693_update_otp(sd);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov5693_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov5693_read(sd, 0x300a, &v);
	printk("CHIP_ID_H=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != OV5693_CHIP_ID_H)
		return -ENODEV;
	ret = ov5693_read(sd, 0x300b, &v);
	printk("CHIP_ID_L=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != OV5693_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct ov5693_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov5693_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV5693_FMTS ARRAY_SIZE(ov5693_formats)

static struct ov5693_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	struct regval_list *regs; /* Regs to tweak */
} ov5693_win_sizes[] = {
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.vts		= 0x418,
		.framerate	= 30,
		.regs 		= ov5693_win_sxga,
	},
	/* 2560*1920 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x830,
		.framerate	= 15,
		.regs 		= ov5693_win_5m,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(ov5693_win_sizes))

static int ov5693_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV5693_FMTS)
		return -EINVAL;

	*code = ov5693_formats[index].mbus_code;
	return 0;
}

static int ov5693_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov5693_format_struct **ret_fmt,
		struct ov5693_win_size **ret_wsize)
{
	int index;
	struct ov5693_win_size *wsize;

	for (index = 0; index < N_OV5693_FMTS; index++)
		if (ov5693_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV5693_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov5693_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov5693_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov5693_win_sizes; wsize < ov5693_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov5693_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov5693_formats[index].colorspace;
	return 0;
}

static int ov5693_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov5693_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov5693_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov5693_info *info = container_of(sd, struct ov5693_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov5693_format_struct *fmt_s;
	struct ov5693_win_size *wsize;
	int ret;

	ret = ov5693_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov5693_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
	//	ret = ov5693_write_array(sd, wsize->regs);
	//	if (ret)
	//		return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 819; /* Mbps. */
		data->sensor_vts_address1 = info->vts_address1;
		data->sensor_vts_address2 = info->vts_address2;
		data->framerate = wsize->framerate;
		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			printk("switch to preview mode\n");
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa2;
			if (!info->dynamic_framerate) {
				data->reg[1].addr = info->vts_address1;
				data->reg[1].data = data->vts >> 8;
				data->reg[2].addr = info->vts_address2;
				data->reg[2].data = data->vts & 0x00ff;
				data->reg_num = 3;
			}
		} else if ((wsize->width == SXGA_WIDTH)
			&& (wsize->height == SXGA_HEIGHT)) {
			printk("switch to preview mode\n");
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa0;
			if (!info->dynamic_framerate) {
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

static int ov5693_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov5693_write_array(sd, ov5693_stream_on);
	else
		ret = ov5693_write_array(sd, ov5693_stream_off);

	return ret;
}

static int ov5693_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov5693_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov5693_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5693_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5693_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov5693_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov5693_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov5693_frame_rates[interval->index];
	return 0;
}

static int ov5693_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov5693_win_size *win = &ov5693_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov5693_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov5693_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov5693_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov5693_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov5693_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov5693_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct ov5693_info *info = container_of(sd, struct ov5693_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		if (ctrl->value == FRAME_RATE_AUTO) {
			info->dynamic_framerate = 1;
		} 
		else ret = -EINVAL;
		break;
	default:
		ret = -ENOIOCTLCMD; 
	}
	
	return ret;
}

static int ov5693_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV5693, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5693_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov5693_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov5693_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov5693_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov5693_core_ops = {
	.g_chip_ident = ov5693_g_chip_ident,
	.g_ctrl = ov5693_g_ctrl,
	.s_ctrl = ov5693_s_ctrl,
	.queryctrl = ov5693_queryctrl,
	.reset = ov5693_reset,
	.init = ov5693_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov5693_g_register,
	.s_register = ov5693_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov5693_video_ops = {
	.enum_mbus_fmt = ov5693_enum_mbus_fmt,
	.try_mbus_fmt = ov5693_try_mbus_fmt,
	.s_mbus_fmt = ov5693_s_mbus_fmt,
	.s_stream = ov5693_s_stream,
	.cropcap = ov5693_cropcap,
	.g_crop	= ov5693_g_crop,
	.s_parm = ov5693_s_parm,
	.g_parm = ov5693_g_parm,
	.enum_frameintervals = ov5693_enum_frameintervals,
	.enum_framesizes = ov5693_enum_framesizes,
};

static const struct v4l2_subdev_ops ov5693_ops = {
	.core = &ov5693_core_ops,
	.video = &ov5693_video_ops,
};

static ssize_t ov5693_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", rg_ratio_typical);
}

static ssize_t ov5693_rg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical = (unsigned int)value;

	return size;
}

static ssize_t ov5693_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", bg_ratio_typical);
}

static ssize_t ov5693_bg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov5693_rg_ratio_typical, 0664, ov5693_rg_ratio_typical_show, ov5693_rg_ratio_typical_store);
static DEVICE_ATTR(ov5693_bg_ratio_typical, 0664, ov5693_bg_ratio_typical_show, ov5693_bg_ratio_typical_store);

static int ov5693_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov5693_info *info;
	int ret;

	info = kzalloc(sizeof(struct ov5693_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov5693_ops);

	/* Make sure it's an ov5693 */
	ret = ov5693_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov5693 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov5693 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ret = device_create_file(&client->dev, &dev_attr_ov5693_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5693_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5693_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov5693_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5693_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5693_bg_ratio_typical;
	}

	return 0;

err_create_dev_attr_ov5693_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_ov5693_rg_ratio_typical);
err_create_dev_attr_ov5693_rg_ratio_typical:
	kfree(info);
	return ret;
}

static int ov5693_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5693_info *info = container_of(sd, struct ov5693_info, sd);

	device_remove_file(&client->dev, &dev_attr_ov5693_rg_ratio_typical);
	device_remove_file(&client->dev, &dev_attr_ov5693_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov5693_id[] = {
	{ "ov5693", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5693_id);

static struct i2c_driver ov5693_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov5693",
	},
	.probe		= ov5693_probe,
	.remove		= ov5693_remove,
	.id_table	= ov5693_id,
};

static __init int init_ov5693(void)
{
	return i2c_add_driver(&ov5693_driver);
}

static __exit void exit_ov5693(void)
{
	i2c_del_driver(&ov5693_driver);
}

fs_initcall(init_ov5693);
module_exit(exit_ov5693);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov5693 sensors");
MODULE_LICENSE("GPL");
