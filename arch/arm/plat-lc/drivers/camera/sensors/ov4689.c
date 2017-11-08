
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
#include "ov4689.h"

//#define ov4689_CHIP_ID	(0x6c)

#define ov4689_CHIP_ID_H	(0x46)
#define ov4689_CHIP_ID_L	(0x88)


#define MAX_WIDTH		1920
#define MAX_HEIGHT		1080

#define MAX_PREVIEW_WIDTH	MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT

#define ov4689_REG_END		0xffff
#define ov4689_REG_DELAY	0xfffe

#define ov4689_REG_VAL_PREVIEW  0xff
#define ov4689_REG_VAL_SNAPSHOT 0xfe
static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals);

struct ov4689_format_struct;
struct ov4689_info {
	struct v4l2_subdev sd;
	struct ov4689_format_struct *fmt;
	struct ov4689_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int frame_rate;
	int binning;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct otp_struct {
	unsigned int module_integrator_id;
	unsigned int lens_id;
	unsigned int rg_ratio;
	unsigned int bg_ratio;
	unsigned int user_data[3];
	unsigned int light_rg;
	unsigned int light_bg;
};

static struct regval_list ov4689_init_regs[] = {
	{0x0103, 0x01},
	{ov4689_REG_DELAY, 200},
	{0x3638, 0x00},
	{0x0300, 0x00},
	{0x0302, 0x18},
	{0x0303, 0x00},
	{0x0304, 0x03},
	{0x030b, 0x00},
	{0x030d, 0x1c},
	{0x030e, 0x04},
	{0x030f, 0x01},
	{0x0312, 0x01},
	{0x031e, 0x00},
	{0x3000, 0x20},
	{0x3002, 0x00},
	{0x3018, 0x72},
	{0x3020, 0x93},
	{0x3021, 0x03},
	{0x3022, 0x01},
	{0x3031, 0x0a},
	{0x303f, 0x0c},
	{0x3305, 0xf1},
	{0x3307, 0x04},
	{0x3309, 0x29},
	{0x3500, 0x00},
	{0x3501, 0x4c},
	{0x3502, 0x00},
	//{0x3503, 0x04},
	{0x3503, 0x34},
	{0x3504, 0x00},
	{0x3505, 0x00},
	{0x3506, 0x00},
	{0x3507, 0x00},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x00},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x00},
	{0x350e, 0x00},
	{0x350f, 0x80},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x00},
	{0x3513, 0x00},
	{0x3514, 0x00},
	{0x3515, 0x80},
	{0x3516, 0x00},
	{0x3517, 0x00},
	{0x3518, 0x00},
	{0x3519, 0x00},
	{0x351a, 0x00},
	{0x351b, 0x80},
	{0x351c, 0x00},
	{0x351d, 0x00},
	{0x351e, 0x00},
	{0x351f, 0x00},
	{0x3520, 0x00},
	{0x3521, 0x80},
	{0x3522, 0x08},
	{0x3524, 0x08},
	{0x3526, 0x08},
	{0x3528, 0x08},
	{0x352a, 0x08},
	{0x3602, 0x00},
	{0x3603, 0x40},
	{0x3604, 0x02},
	{0x3605, 0x00},
	{0x3606, 0x00},
	{0x3607, 0x00},
	{0x3609, 0x12},
	{0x360a, 0x40},
	{0x360c, 0x08},
	{0x360f, 0xe5},
	{0x3608, 0x8f},
	{0x3611, 0x00},
	{0x3613, 0xf7},
	{0x3616, 0x58},
	{0x3619, 0x99},
	{0x361b, 0x60},
	{0x361c, 0x7a},
	{0x361e, 0x79},
	{0x361f, 0x02},
	{0x3632, 0x00},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x15},
	{0x3646, 0x86},
	{0x364a, 0x0b},
	{0x3700, 0x17},
	{0x3701, 0x22},
	{0x3703, 0x10},
	{0x370a, 0x37},
	{0x3705, 0x00},
	{0x3706, 0x63},
	{0x3709, 0x3c},
	{0x370b, 0x01},
	{0x370c, 0x30},
	{0x3710, 0x24},
	{0x3711, 0x0c},
	{0x3716, 0x00},
	{0x3720, 0x28},
	{0x3729, 0x7b},
	{0x372a, 0x84},
	{0x372b, 0xbd},
	{0x372c, 0xbc},
	{0x372e, 0x52},
	{0x373c, 0x0e},
	{0x373e, 0x33},
	{0x3743, 0x10},
	{0x3744, 0x88},
	{0x3745, 0xc0},
	{0x374a, 0x43},
	{0x374c, 0x00},
	{0x374e, 0x23},
	{0x3751, 0x7b},
	{0x3752, 0x84},
	{0x3753, 0xbd},
	{0x3754, 0xbc},
	{0x3756, 0x52},
	{0x375c, 0x00},
	{0x3760, 0x00},
	{0x3761, 0x00},
	{0x3762, 0x00},
	{0x3763, 0x00},
	{0x3764, 0x00},
	{0x3767, 0x04},
	{0x3768, 0x04},
	{0x3769, 0x08},
	{0x376a, 0x08},
	{0x376b, 0x20},
	{0x376c, 0x00},
	{0x376d, 0x00},
	{0x376e, 0x00},
	{0x3773, 0x00},
	{0x3774, 0x51},
	{0x3776, 0xbd},
	{0x3777, 0xbd},
	{0x3781, 0x18},
	{0x3783, 0x25},
	{0x3798, 0x1b},
	{0x3800, 0x01},
	{0x3801, 0x88},
	{0x3802, 0x00},
	{0x3803, 0xe0},
	{0x3804, 0x09},
	{0x3805, 0x17},
	{0x3806, 0x05},
	{0x3807, 0x1f},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x0d},
	{0x380d, 0x98},
	{0x380e, 0x04},
	{0x380f, 0x8a},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3819, 0x01},
	{0x3820, 0x00},
	{0x3821, 0x06},
	{0x3829, 0x00},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x382d, 0x7f},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3837, 0x00},
	{0x3841, 0x02},
	{0x3846, 0x08},
	{0x3847, 0x07},
	{0x3d85, 0x36},
	{0x3d8c, 0x71},
	{0x3d8d, 0xcb},
	{0x3f0a, 0x00},
	{0x4000, 0x71},
	{0x4001, 0x40},
	{0x4002, 0x04},
	{0x4003, 0x14},
	{0x4005, 0x10},	
	{0x400e, 0x00},
	
	{0x4011, 0x00},
	{0x401a, 0x00},
	{0x401b, 0x00},
	{0x401c, 0x00},
	{0x401d, 0x00},
	{0x401f, 0x00},
	{0x4020, 0x00},
	{0x4021, 0x10},
	{0x4022, 0x06},
	{0x4023, 0x13},
	{0x4024, 0x07},
	{0x4025, 0x40},
	{0x4026, 0x07},
	{0x4027, 0x50},
	{0x4028, 0x00},
	{0x4029, 0x02},
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402c, 0x02},
	{0x402d, 0x02},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4302, 0xff},
	{0x4303, 0xff},
	{0x4304, 0x00},
	{0x4305, 0x00},
	{0x4306, 0x00},
	{0x4308, 0x02},
	{0x4500, 0x6c},
	{0x4501, 0xc4},
	{0x4502, 0x40},
	{0x4503, 0x01},
	{0x4601, 0x77},
	{0x4800, 0x04},
	{0x4813, 0x08},
	{0x481f, 0x40},
	{0x4829, 0x78},
	{0x4837, 0x1a},
	{0x4b00, 0x2a},
	{0x4b0d, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0x42},
	{0x4d02, 0xd1},
	{0x4d03, 0x93},
	{0x4d04, 0xf5},
	{0x4d05, 0xc1},
	{0x5000, 0xf3},
	{0x5001, 0x11},
	{0x5004, 0x00},
	{0x500a, 0x00},
	{0x500b, 0x00},
	{0x5032, 0x00},
	{0x5040, 0x00},
	{0x5050, 0x0c},
	{0x5500, 0x00},
	{0x5501, 0x10},
	{0x5502, 0x01},
	{0x5503, 0x0f},
	{0x8000, 0x00},
	{0x8001, 0x00},
	{0x8002, 0x00},
	{0x8003, 0x00},
	{0x8004, 0x00},
	{0x8005, 0x00},
	{0x8006, 0x00},
	{0x8007, 0x00},
	{0x8008, 0x00},
	{0x3638, 0x00},
	{0x0100, 0x01},
	{0x0300, 0x60},

	{ov4689_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov4689_stream_on[] = {
	{0x0100, 0x01},
	{0x4800, 0x04},
	{ov4689_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov4689_stream_off[] = {
	/* Sensor enter LP11*/
	{0x4800, 0x24},
	{0x0100, 0x00},
	{ov4689_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov4689_win_sxga[] = {

	{0x0100,0x00},//Stream Off 
	{0x3708,0x66},
	{0x3709,0x52},
	{0x370c,0xc3},
	{0x3800,0x00},// x start = 0
	{0x3801,0x00},// x start
	{0x3802,0x00},// y start = 0
	{0x3803,0x00},// y start
	{0x3804,0x0a},// xend = 2623
	{0x3805,0x3f},// xend
	{0x3806,0x07},// yend = 1955
	{0x3807,0xa3},// yend
	{0x3808,0x05},// x output size = 1296
	{0x3809,0x00},// x output size
	{0x380a,0x03},// y output size = 972
	{0x380b,0xc0},// y output size
	{0x380c,0x0b},// hts = 2816
	{0x380d,0x20},// hts
	{0x380e,0x03},// vts = 992
	{0x380f,0xe8},//E0;// vts    
	{0x3810,0x00},// isp x win = 8
	{0x3811,0x08},// isp x win
	{0x3812,0x00},// isp y win = 4
	{0x3813,0x04},// isp y win
	{0x3814,0x31},// x inc
	{0x3815,0x31},// y inc
	{0x3817,0x00},// hsync start
	{0x3820,0x08},// flip off, v bin off
	{0x3821,0x07},// mirror on, h bin on
	{0x4004,0x02},// black line number    //6c 4005 1a;// blc normal freeze
	{0x4005,0x18},// blc normal freeze
	{0x4050,0x37},// blc normal freeze
	{0x4051,0x8f},// blc normal freeze    //6c 350b 80;// gain = 8x
	{0x4837,0x17},// MIPI global timing    
	{0x0100,0x01},//Stream On

	{ov4689_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov4689_win_4m[] = {
#if 0

	{0x0100,0x00},//Stream Off 
	{0x3708,0x63},   
	{0x3709,0x12},
	{0x370c,0xc0},
	{0x3800,0x00},// xstart = 0
	{0x3801,0x00},// xstart
	{0x3802,0x00},// ystart = 0
	{0x3803,0x00},// ystart
	{0x3804,0x0a},// xend = 2623
	{0x3805,0x3f},// xend
	{0x3806,0x07},// yend = 1955
	{0x3807,0xa3},// yend
	{0x3808,0x0a},// x output size = 2592
	{0x3809,0x00},//20;// x output size
	{0x380a,0x07},// y output size = 1944             
	{0x380b,0x80},//98;// y output size               
	{0x380c,0x0b},// hts = 2816                      
	{0x380d,0x20},// hts                      
	{0x380e,0x07},// vts = 1984                      
	{0x380f,0xd0},// vts                          
	{0x3810,0x00},// isp x win = 16                   
	{0x3811,0x10},// isp x win                      
	{0x3812,0x00},// isp y win = 6                    
	{0x3813,0x06},// isp y win                      
	{0x3814,0x11},// x inc                      
	{0x3815,0x11},// y inc                      
	{0x3817,0x00},// hsync start                      
	{0x3820,0x40},// flip off, v bin off              
	{0x3821,0x06},// mirror on, v bin off             
	{0x4004,0x04},// black line number                
	{0x4005,0x1a},// blc always update    //6c 350b 40;// gain = 4x
	{0x4837,0x17},// MIPI global timing               
	{0x0100,0x01},//Stream On    

#endif
	{ov4689_REG_END, 0x00},	/* END MARKER */
};

static int ov4689_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov4689_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov4689_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
        unsigned char flag = 0;
        unsigned char hvflip = 0;

	while (vals->reg_num != ov4689_REG_END) {
                flag = 0;
                hvflip = 0;
		if (vals->reg_num == ov4689_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else if (vals->reg_num == 0x3821) {//set mirror
			if (vals->value == ov4689_REG_VAL_PREVIEW) {
                                vals->value = 0x01;
                                flag = 1;
                                hvflip = ov4689_isp_parm.mirror;
			} else if (vals->value == ov4689_REG_VAL_SNAPSHOT) {
				vals->value = 0x00;
                                flag = 1;
                                hvflip = ov4689_isp_parm.mirror;
			}

		} else if (vals->reg_num == 0x3820) {//set flip
			if (vals->value == ov4689_REG_VAL_PREVIEW) {
				vals->value = 0x00;
                                flag = 1;
                                hvflip = ov4689_isp_parm.flip;
			} else if (vals->value == ov4689_REG_VAL_SNAPSHOT) {
				vals->value = 0x00;
                                flag = 1;
                                hvflip = ov4689_isp_parm.flip;
			}

		} if (flag) {
                        if (hvflip)
                                vals->value |= 0x06;
                        else
                                vals->value &= 0xf9;

                        printk("0x%04x=0x%02x\n", vals->reg_num, vals->value);
                }


		ret = ov4689_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;

		vals++;
	}
	return 0;
}

/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical = 0x180;
static unsigned int bg_ratio_typical = 0x16e;

/*
	index: index of otp group. (0, 1, 2)
 	return: 0, group index is empty
	1, group index has invalid data
	2, group index has valid data
*/
static int ov4689_check_otp(struct v4l2_subdev *sd, unsigned short index)
{
	unsigned char temp;
	unsigned short i;
	unsigned short address;
	int ret;

	if (index <= 2){
		//read otp into buffer
		ov4689_write(sd, 0x3d84, 0xc0);

		ov4689_write(sd, 0x3d85, 0x00);
		ov4689_write(sd, 0x3d86, 0x0f);
		ov4689_write(sd, 0x3d81, 0x01);
		mdelay(10);
		address = 0x3d05 + (index-1) * 9;
		ret = ov4689_read(sd, address, &temp);
		if (ret < 0)
			return ret;

		// disable otp read
		ov4689_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov4689_write(sd, 0x3d00 + i, 0x00);
	} else {
		//read otp into buffer
		ov4689_write(sd, 0x3d84, 0xc0);

		ov4689_write(sd, 0x3d85, 0x10);
		ov4689_write(sd, 0x3d86, 0x1f);
		ov4689_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d07;
		ret = ov4689_read(sd, address, &temp);
		if (ret < 0)
			return ret;

		// disable otp read
		ov4689_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov4689_write(sd, 0x3d00 + i, 0x00);
	}

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
static int ov4689_read_otp(struct v4l2_subdev *sd,
				unsigned short index, struct otp_struct* otp)
{
	unsigned char temp,temp1;
	unsigned short address;
	unsigned short i;
	int ret;

	if (index == 1){
		// read otp into buffer
		ov4689_write(sd, 0x3d84, 0xc0);

		ov4689_write(sd, 0x3d85, 0x00);
		ov4689_write(sd, 0x3d86, 0x0f);
		ov4689_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d05;
		ret = ov4689_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->module_integrator_id = (unsigned int)(temp & 0x7f);

		ret = ov4689_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->lens_id = (unsigned int)temp;

		ret = ov4689_read(sd, address + 4, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[0] = (unsigned int)temp;

		ret = ov4689_read(sd, address + 5, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[1] = (unsigned int)temp;

		ret = ov4689_read(sd, address + 6, &temp1);
		if (ret < 0)
			return ret;

		ret = ov4689_read(sd, address + 2, &temp);
		if (ret < 0)
			return ret;
		otp->rg_ratio = (temp<<2) + (temp1>>6);

		ret = ov4689_read(sd, address + 3, &temp);
		if (ret < 0)
			return ret;
		otp->bg_ratio = (temp<<2) + ((temp1>>4)&0x03);

		ret = ov4689_read(sd, address + 7, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + ((temp1>>2)&0x03);

		ret = ov4689_read(sd, address + 8, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + (temp1&0x03);

		// disable otp read
		ov4689_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov4689_write(sd, 0x3d00 + i, 0x00);

		}else if (index == 2){
		// read otp into buffer
		ov4689_write(sd, 0x3d84, 0xc0);

		ov4689_write(sd, 0x3d85, 0x00);
		ov4689_write(sd, 0x3d86, 0x0f);
		ov4689_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d0e;
		ret = ov4689_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->module_integrator_id = (unsigned int)(temp & 0x7f);

		ret = ov4689_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->lens_id = (unsigned int)temp;

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov4689_write(sd, 0x3d00 + i, 0x00);

		// read otp into buffer
		ov4689_write(sd, 0x3d84, 0xc0);

		ov4689_write(sd, 0x3d85, 0x10);
		ov4689_write(sd, 0x3d86, 0x1f);
		ov4689_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d00;

		ret = ov4689_read(sd, address + 2, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[0] = (unsigned int)temp;

		ret = ov4689_read(sd, address + 3, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[1] = (unsigned int)temp;

		ret = ov4689_read(sd, address + 4, &temp1);
		if (ret < 0)
			return ret;

		ret = ov4689_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->rg_ratio = (temp<<2) + (temp1>>6);

		ret = ov4689_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->bg_ratio = (temp<<2) + ((temp1>>4)&0x03);

		ret = ov4689_read(sd, address + 5, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + ((temp1>>2)&0x03);

		ret = ov4689_read(sd, address + 6, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + (temp1&0x03);

		// disable otp read
		ov4689_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov4689_write(sd, 0x3d00 + i, 0x00);

		}else{
		// read otp into buffer
		ov4689_write(sd, 0x3d84, 0xc0);

		ov4689_write(sd, 0x3d85, 0x10);
		ov4689_write(sd, 0x3d86, 0x1f);
		ov4689_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d17;
		ret = ov4689_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->module_integrator_id = (unsigned int)(temp & 0x7f);

		ret = ov4689_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->lens_id = (unsigned int)temp;

		ret = ov4689_read(sd, address + 4, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[0] = (unsigned int)temp;

		ret = ov4689_read(sd, address + 5, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[1] = (unsigned int)temp;

		ret = ov4689_read(sd, address + 6, &temp1);
		if (ret < 0)
			return ret;

		ret = ov4689_read(sd, address + 2, &temp);
		if (ret < 0)
			return ret;
		otp->rg_ratio = (temp<<2) + (temp1>>6);

		ret = ov4689_read(sd, address + 3, &temp);
		if (ret < 0)
			return ret;
		otp->bg_ratio = (temp<<2) + ((temp1>>4)&0x03);

		ret = ov4689_read(sd, address + 7, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + ((temp1>>2)&0x03);

		ret = ov4689_read(sd, address + 8, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + (temp1&0x03);

		// disable otp read
		ov4689_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov4689_write(sd, 0x3d00 + i, 0x00);
		}

        return ret;
}

/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov4689_update_awb_gain(struct v4l2_subdev *sd,
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	if (R_gain > 0x400) {
		ov4689_write(sd, 0x5186, (unsigned char)(R_gain >> 8));
		ov4689_write(sd, 0x5187, (unsigned char)(R_gain & 0x00ff));
	}

	if (G_gain > 0x400) {
		ov4689_write(sd, 0x5188, (unsigned char)(G_gain >> 8));
		ov4689_write(sd, 0x5189, (unsigned char)(G_gain & 0x00ff));
	}

	if (B_gain > 0x400) {
		ov4689_write(sd, 0x518a, (unsigned char)(B_gain >> 8));
		ov4689_write(sd, 0x518b, (unsigned char)(B_gain & 0x00ff));
	}

	return 0;
}

/*
	call this function after ov4689 initialization
	return value: 0 update success
	1, no OTP
*/
static int ov4689_update_otp(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	unsigned int i;
	unsigned int otp_index;
	unsigned int temp;
	unsigned int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int ret = 0;

        memset(&current_otp, 0, sizeof(current_otp));

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for (i = 1; i <= 3; i++) {
		temp = ov4689_check_otp(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}

	if (i > 3)
		// no valid wb OTP data
		return 1;

	ret = ov4689_read_otp(sd, otp_index, &current_otp);
	if (ret < 0)
		return ret;

	if (!current_otp.bg_ratio) {
		printk(KERN_ERR "ERROR ov4689_update_otp: current bg ratio is invalid.\n");
		return 1;
	}

	if (!current_otp.rg_ratio) {
		printk(KERN_ERR "ERROR ov4689_update_otp: current rg ratio is invalid.\n");
		return 1;
	}

	if (current_otp.light_rg)
		current_otp.rg_ratio = current_otp.rg_ratio * (current_otp.light_rg +512)/1024;

	if (current_otp.light_bg)
		current_otp.bg_ratio = current_otp.bg_ratio * (current_otp.light_bg +512)/1024;

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
	} else {
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

	ov4689_update_awb_gain(sd, R_gain, G_gain, B_gain);

	return 0;
}

static int sensor_get_vcm_range(void **vals, int val)
{
	printk("*sensor_get_vcm_range\n");
	switch (val) {
	case FOCUS_MODE_AUTO:
		*vals = focus_mode_normal;
		return ARRAY_SIZE(focus_mode_normal);
	case FOCUS_MODE_INFINITY:
		*vals = focus_mode_infinity;
		return ARRAY_SIZE(focus_mode_infinity);
	case FOCUS_MODE_MACRO:
		*vals = focus_mode_macro;
		return ARRAY_SIZE(focus_mode_macro);
	case FOCUS_MODE_FIXED:
	case FOCUS_MODE_EDOF:
	case FOCUS_MODE_CONTINUOUS_VIDEO:
	case FOCUS_MODE_CONTINUOUS_PICTURE:
	case FOCUS_MODE_CONTINUOUS_AUTO:
		*vals = focus_mode_normal;
		return ARRAY_SIZE(focus_mode_normal);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_exposure(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L2:
		*vals = isp_exposure_l2;
		return ARRAY_SIZE(isp_exposure_l2);
	case EXPOSURE_L1:
		*vals = isp_exposure_l1;
		return ARRAY_SIZE(isp_exposure_l1);
	case EXPOSURE_H0:
		*vals = isp_exposure_h0;
		return ARRAY_SIZE(isp_exposure_h0);
	case EXPOSURE_H1:
		*vals = isp_exposure_h1;
		return ARRAY_SIZE(isp_exposure_h1);
	case EXPOSURE_H2:
		*vals = isp_exposure_h2;
		return ARRAY_SIZE(isp_exposure_h2);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_iso(void **vals, int val)
{
	switch(val) {
	case ISO_100:
		*vals = isp_iso_100;
		return ARRAY_SIZE(isp_iso_100);
	case ISO_200:
		*vals = isp_iso_200;
		return ARRAY_SIZE(isp_iso_200);
	case ISO_400:
		*vals = isp_iso_400;
		return ARRAY_SIZE(isp_iso_400);
	case ISO_800:
		*vals = isp_iso_800;
		return ARRAY_SIZE(isp_iso_800);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_contrast(void **vals, int val)
{
	switch (val) {
	case CONTRAST_L3:
		*vals = isp_contrast_l3;
		return ARRAY_SIZE(isp_contrast_l3);
	case CONTRAST_L2:
		*vals = isp_contrast_l2;
		return ARRAY_SIZE(isp_contrast_l2);
	case CONTRAST_L1:
		*vals = isp_contrast_l1;
		return ARRAY_SIZE(isp_contrast_l1);
	case CONTRAST_H0:
		*vals = isp_contrast_h0;
		return ARRAY_SIZE(isp_contrast_h0);
	case CONTRAST_H1:
		*vals = isp_contrast_h1;
		return ARRAY_SIZE(isp_contrast_h1);
	case CONTRAST_H2:
		*vals = isp_contrast_h2;
		return ARRAY_SIZE(isp_contrast_h2);
	case CONTRAST_H3:
		*vals = isp_contrast_h3;
		return ARRAY_SIZE(isp_contrast_h3);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_l2;
		return ARRAY_SIZE(isp_saturation_l2);
	case SATURATION_L1:
		*vals = isp_saturation_l1;
		return ARRAY_SIZE(isp_saturation_l1);
	case SATURATION_H0:
		*vals = isp_saturation_h0;
		return ARRAY_SIZE(isp_saturation_h0);
	case SATURATION_H1:
		*vals = isp_saturation_h1;
		return ARRAY_SIZE(isp_saturation_h1);
	case SATURATION_H2:
		*vals = isp_saturation_h2;
		return ARRAY_SIZE(isp_saturation_h2);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_white_balance(void **vals, int val)
{
	switch (val) {
	case WHITE_BALANCE_AUTO:
		*vals = isp_white_balance_auto;
		return ARRAY_SIZE(isp_white_balance_auto);
	case WHITE_BALANCE_INCANDESCENT:
		*vals = isp_white_balance_incandescent;
		return ARRAY_SIZE(isp_white_balance_incandescent);
	case WHITE_BALANCE_FLUORESCENT:
		*vals = isp_white_balance_fluorescent;
		return ARRAY_SIZE(isp_white_balance_fluorescent);
	case WHITE_BALANCE_WARM_FLUORESCENT:
		*vals = isp_white_balance_warm_fluorescent;
		return ARRAY_SIZE(isp_white_balance_warm_fluorescent);
	case WHITE_BALANCE_DAYLIGHT:
		*vals = isp_white_balance_daylight;
		return ARRAY_SIZE(isp_white_balance_daylight);
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		*vals = isp_white_balance_cloudy_daylight;
		return ARRAY_SIZE(isp_white_balance_cloudy_daylight);
	case WHITE_BALANCE_TWILIGHT:
		*vals = isp_white_balance_twilight;
		return ARRAY_SIZE(isp_white_balance_twilight);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade;
		return ARRAY_SIZE(isp_white_balance_shade);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_brightness(void **vals, int val)
{
	switch (val) {
	case BRIGHTNESS_L3:
		*vals = isp_brightness_l3;
		return ARRAY_SIZE(isp_brightness_l3);
	case BRIGHTNESS_L2:
		*vals = isp_brightness_l2;
		return ARRAY_SIZE(isp_brightness_l2);
	case BRIGHTNESS_L1:
		*vals = isp_brightness_l1;
		return ARRAY_SIZE(isp_brightness_l1);
	case BRIGHTNESS_H0:
		*vals = isp_brightness_h0;
		return ARRAY_SIZE(isp_brightness_h0);
	case BRIGHTNESS_H1:
		*vals = isp_brightness_h1;
		return ARRAY_SIZE(isp_brightness_h1);
	case BRIGHTNESS_H2:
		*vals = isp_brightness_h2;
		return ARRAY_SIZE(isp_brightness_h2);
	case BRIGHTNESS_H3:
		*vals = isp_brightness_h3;
		return ARRAY_SIZE(isp_brightness_h3);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_sharpness(void **vals, int val)
{
	switch (val) {
	case SHARPNESS_L2:
		*vals = isp_sharpness_l2;
		return ARRAY_SIZE(isp_sharpness_l2);
	case SHARPNESS_L1:
		*vals = isp_sharpness_l1;
		return ARRAY_SIZE(isp_sharpness_l1);
	case SHARPNESS_H0:
		*vals = isp_sharpness_h0;
		return ARRAY_SIZE(isp_sharpness_h0);
	case SHARPNESS_H1:
		*vals = isp_sharpness_h1;
		return ARRAY_SIZE(isp_sharpness_h1);
	case SHARPNESS_H2:
		*vals = isp_sharpness_h2;
		return ARRAY_SIZE(isp_sharpness_h2);
	default:
		return -EINVAL;
	}

	return 0;
}

static struct isp_effect_ops sensor_effect_ops = {
	.get_exposure = sensor_get_exposure,
	.get_iso = sensor_get_iso,
	.get_contrast = sensor_get_contrast,
	.get_saturation = sensor_get_saturation,
	.get_white_balance = sensor_get_white_balance,
	.get_brightness = sensor_get_brightness,
	.get_sharpness = sensor_get_sharpness,
	.get_vcm_range = sensor_get_vcm_range,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,

};


static int ov4689_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov4689_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov4689_info *info = container_of(sd, struct ov4689_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->frame_rate = FRAME_RATE_DEFAULT;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret = ov4689_write_array(sd, ov4689_init_regs);
	if (ret < 0)
		return ret;

//	ret = ov4689_update_otp(sd);
//	if (ret < 0)
//		return ret;

	return 0;
}

static int ov4689_detect(struct v4l2_subdev *sd)
{
	unsigned char v1,v2;
	int ret;

	ret = ov4689_read(sd, 0x300a, &v1);
	if (ret < 0)
		return ret;
	if (v1 != ov4689_CHIP_ID_H)
		return -ENODEV;

	ret = ov4689_read(sd, 0x300b, &v2);
	if (ret < 0)
		return ret;
	if (v2 != ov4689_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct ov4689_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov4689_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_ov4689_FMTS ARRAY_SIZE(ov4689_formats)

static struct ov4689_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
	struct isp_regb_vals *regs_aecgc_win_matrix;
	struct isp_regb_vals *regs_aecgc_win_center;
	struct isp_regb_vals *regs_aecgc_win_central_weight;

} ov4689_win_sizes[] = {

	/* 1920*1080 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x48a,//0x7b6
		.framerate	= 30,//10,
		.max_gain_dyn_frm = 0xf8,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xf8,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov4689_win_4m,
		.regs_aecgc_win_matrix	 	    = isp_aecgc_win_5M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_5M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_5M_central_weight,
	},

};
#define N_WIN_SIZES (ARRAY_SIZE(ov4689_win_sizes))

static int ov4689_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_ov4689_FMTS)
		return -EINVAL;

	*code = ov4689_formats[index].mbus_code;
	return 0;
}

static int ov4689_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov4689_format_struct **ret_fmt,
		struct ov4689_win_size **ret_wsize)
{
	int index;
	struct ov4689_win_size *wsize;

	for (index = 0; index < N_ov4689_FMTS; index++)
		if (ov4689_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_ov4689_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov4689_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov4689_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov4689_win_sizes; wsize < ov4689_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov4689_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov4689_formats[index].colorspace;
	return 0;
}

static int ov4689_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov4689_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov4689_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov4689_info *info = container_of(sd, struct ov4689_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov4689_format_struct *fmt_s = NULL;
	struct ov4689_win_size *wsize = NULL;
	int ret = 0;
	int framerate = 0;
	static int last_frame_rate = 0;

	ret = ov4689_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov4689_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if (((info->win != wsize)
		|| (last_frame_rate != info->frame_rate))
		&& wsize->regs) {
		last_frame_rate = info->frame_rate;
		ret = ov4689_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 624;//988; //282; //TBD/* Mbps. */ OK
		data->sensor_vts_address1 = info->vts_address1;
		data->sensor_vts_address2 = info->vts_address2;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;
		data->framerate = wsize->framerate;
		data->binning = info->binning;
		if ((wsize->width == MAX_WIDTH)&& (wsize->height == MAX_HEIGHT)) {
#if 0
			data->flags = V4L2_I2C_ADDR_8BIT;
			data->slave_addr = client->addr;
			data->reg[data->reg_num].addr = 0x3208;
			data->reg[data->reg_num++].data = 0xa2;
			if (info->frame_rate != FRAME_RATE_AUTO) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num++].data = data->vts & 0x00ff;
			}
#endif
		} 
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov4689_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov4689_write_array(sd, ov4689_stream_on);
	else
		ret = ov4689_write_array(sd, ov4689_stream_off);

	return ret;
}

static int ov4689_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov4689_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov4689_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov4689_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov4689_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov4689_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov4689_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov4689_frame_rates[interval->index];
	return 0;
}

static int ov4689_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov4689_win_size *win = &ov4689_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov4689_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov4689_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov4689_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov4689_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov4689_isp_parm;
		break;
	case V4L2_CID_GET_EFFECT_FUNC:
		ctrl->value = (int)&sensor_effect_ops;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov4689_set_framerate(struct v4l2_subdev *sd, int framerate)
	{
		struct ov4689_info *info = container_of(sd, struct ov4689_info, sd);
		printk("ov4689_set_framerate %d\n", framerate);
		if (framerate < FRAME_RATE_AUTO)
			framerate = FRAME_RATE_AUTO;
		else if (framerate > 30)
			framerate = 30;
		info->frame_rate = framerate;
		return 0;
	}


static int ov4689_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
//	struct ov4689_info *info = container_of(sd, struct ov4689_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		ret = ov4689_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}
static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals)
{
	int i = 0;
	int ret = 0;

	for(i = 0; i < N_WIN_SIZES; i++) {
		if (width == ov4689_win_sizes[i].width && height == ov4689_win_sizes[i].height) {
			switch (meter_mode){
				case METER_MODE_MATRIX:
					*vals = ov4689_win_sizes[i].regs_aecgc_win_matrix;
					break;
				case METER_MODE_CENTER:
					*vals = ov4689_win_sizes[i].regs_aecgc_win_center;
					break;
				case METER_MODE_CENTRAL_WEIGHT:
					*vals = ov4689_win_sizes[i].regs_aecgc_win_central_weight;
					break;
				default:
					break;
				}
			break;
		}
	}
	return ret;
}

static int ov4689_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV4689, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov4689_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov4689_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov4689_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov4689_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov4689_core_ops = {
	.g_chip_ident = ov4689_g_chip_ident,
	.g_ctrl = ov4689_g_ctrl,
	.s_ctrl = ov4689_s_ctrl,
	.queryctrl = ov4689_queryctrl,
	.reset = ov4689_reset,
	.init = ov4689_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov4689_g_register,
	.s_register = ov4689_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov4689_video_ops = {
	.enum_mbus_fmt = ov4689_enum_mbus_fmt,
	.try_mbus_fmt = ov4689_try_mbus_fmt,
	.s_mbus_fmt = ov4689_s_mbus_fmt,
	.s_stream = ov4689_s_stream,
	.cropcap = ov4689_cropcap,
	.g_crop	= ov4689_g_crop,
	.s_parm = ov4689_s_parm,
	.g_parm = ov4689_g_parm,
	.enum_frameintervals = ov4689_enum_frameintervals,
	.enum_framesizes = ov4689_enum_framesizes,
};

static const struct v4l2_subdev_ops ov4689_ops = {
	.core = &ov4689_core_ops,
	.video = &ov4689_video_ops,
};

static ssize_t ov4689_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", rg_ratio_typical);
}

static ssize_t ov4689_rg_ratio_typical_store(struct device *dev,
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

static ssize_t ov4689_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", bg_ratio_typical);
}

static ssize_t ov4689_bg_ratio_typical_store(struct device *dev,
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

static DEVICE_ATTR(ov4689_rg_ratio_typical, 0664, ov4689_rg_ratio_typical_show, ov4689_rg_ratio_typical_store);
static DEVICE_ATTR(ov4689_bg_ratio_typical, 0664, ov4689_bg_ratio_typical_show, ov4689_bg_ratio_typical_store);

static int ov4689_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov4689_info *info;
	struct comip_camera_subdev_platform_data *ov4689_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct ov4689_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov4689_ops);

	/* Make sure it's an ov4689 */
	ret = ov4689_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov4689 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov4689 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ov4689_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (ov4689_setting) {
		if (ov4689_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			ov4689_isp_parm.mirror = 1;
		else ov4689_isp_parm.mirror = 0;

		if (ov4689_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			ov4689_isp_parm.flip = 1;
		else ov4689_isp_parm.flip = 0;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov4689_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov4689_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov4689_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov4689_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov4689_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov4689_bg_ratio_typical;
	}

	return 0;

err_create_dev_attr_ov4689_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_ov4689_rg_ratio_typical);
err_create_dev_attr_ov4689_rg_ratio_typical:
	kfree(info);
	return ret;
}

static int ov4689_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov4689_info *info = container_of(sd, struct ov4689_info, sd);

	device_remove_file(&client->dev, &dev_attr_ov4689_rg_ratio_typical);
	device_remove_file(&client->dev, &dev_attr_ov4689_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov4689_id[] = {
	{ "ov4689-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov4689_id);

static struct i2c_driver ov4689_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov4689-mipi-raw",
	},
	.probe		= ov4689_probe,
	.remove		= ov4689_remove,
	.id_table	= ov4689_id,
};

static __init int init_ov4689(void)
{
	return i2c_add_driver(&ov4689_driver);
}

static __exit void exit_ov4689(void)
{
	i2c_del_driver(&ov4689_driver);
}

fs_initcall(init_ov4689);
module_exit(exit_ov4689);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov4689 sensors");
MODULE_LICENSE("GPL");
