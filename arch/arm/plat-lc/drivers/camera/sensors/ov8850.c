
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
#include "ov8850.h"

#define OV8850_CHIP_ID_H	(0x88)
#define OV8850_CHIP_ID_M	(0x50)
#define OV8850_CHIP_ID_L	(0x00)

#define UXGA_WIDTH		1632//1920
#define UXGA_HEIGHT	1224//1080
#define MAX_WIDTH		3264
#define MAX_HEIGHT		2448
#define HDV_WIDTH             2880 //for 1080p video
#define HDV_HEIGHT            1620
#define MAX_PREVIEW_WIDTH HDV_WIDTH 
#define MAX_PREVIEW_HEIGHT HDV_HEIGHT


#define OV8850_REG_END		0xffff
#define OV8850_REG_DELAY	0xfffe

//#define OV8850_OTP__VCM_FUNC

struct ov8850_format_struct;
struct ov8850_info {
	struct v4l2_subdev sd;
	struct ov8850_format_struct *fmt;
	struct ov8850_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int dynamic_framerate;
	int binning;
	int test;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct otp_struct {
int module_integrator_id;
int lens_id;
int production_year;
int production_month;
int production_day;
int rg_ratio;
int bg_ratio;
int light_rg;
int light_bg;
int user_data[6];
int lenc[62];
int VCM_start;
int VCM_end;
};

static struct regval_list ov8850_init_regs[] = {

	//global setting
{0x0103, 0x01},
{0x0102, 0x01},
//delay(5ms)   
{OV8850_REG_DELAY, 5},
{0x3002, 0x08},
{0x3004, 0x00},
{0x3005, 0x00},
{0x3011, 0x41},
{0x3012, 0x08},
{0x3014, 0x4a},
{0x3015, 0x0a},
{0x3021, 0x00},
{0x3022, 0x02},
{0x3081, 0x02},
{0x3083, 0x01},
{0x3090, 0x02}, //pll
{0x3091, 0x10}, //pll
{0x3092, 0x00}, //pll
{0x3093, 0x00}, //pll
{0x3098, 0x02}, //pll
{0x3099, 0x18}, //pll
{0x309a, 0x00},
{0x309b, 0x00},
{0x309c, 0x00},
{0x30b3, 0x66},   //pll   //0x51
{0x30b4, 0x03},
{0x30b5, 0x04},
{0x30b6, 0x01},
{0x3104, 0xa1},
{0x3106, 0x01},
{0x3503, 0x07},
{0x3602, 0x70},
{0x3620, 0x64},
{0x3622, 0x0f},
{0x3623, 0x68},
{0x3625, 0x40},
{0x3631, 0x83},
{0x3633, 0x34},
{0x3634, 0x03},
{0x364c, 0x00},
{0x364d, 0x00},
{0x364e, 0x00},
{0x364f, 0x00},
{0x3660, 0x80},
{0x3662, 0x10},
{0x3665, 0x00},
{0x3666, 0x00},
{0x366f, 0x20},
{0x3703, 0x2e},
{0x3732, 0x05},
{0x373a, 0x51},
{0x373d, 0x22},
{0x3754, 0xc0},
{0x3756, 0x2a},
{0x3759, 0x0f},
{0x376b, 0x44},
{0x3795, 0x00},
{0x379c, 0x0c},
{0x3810, 0x00},
{0x3811, 0x04},
{0x3812, 0x00},
{0x3813, 0x04},
{0x3820, 0x10},
{0x3821, 0x0e},
{0x3826, 0x00},
{0x4000, 0x10},
{0x4002, 0xc5},
{0x4005, 0x18},
{0x4006, 0x20},
{0x4008, 0x20},
{0x4009, 0x10},
{0x404f, 0xA0},
{0x4100, 0x1d},
{0x4101, 0x23},
{0x4102, 0x44},
{0x4104, 0x5c},
{0x4109, 0x03},
{0x4300, 0xff},
{0x4301, 0x00},
{0x4315, 0x00},
{0x4512, 0x01},
{0x4800, 0x24},//by wayne 0320
{0x4837, 0x18}, //0x0a,0x1e
{0x4a00, 0xaa},
{0x4a03, 0x01},
{0x4a05, 0x08},
{0x4d00, 0x04},
{0x4d01, 0x52},
{0x4d02, 0xfe},
{0x4d03, 0x05},
{0x4d04, 0xff},
{0x4d05, 0xff},
{0x5000, 0x06},
{0x5001, 0x01},
{0x5002, 0x80},
{0x5041, 0x04},
{0x5043, 0x48},
{0x5e00, 0x00},
{0x5e10, 0x1c},
{0x0100, 0x00},
{0x3624, 0x00},
{0x3680, 0xe0},
{0x3702, 0xf3},
{0x3704, 0x71},
{0x3708, 0xe6},
{0x3709, 0xc3},
{0x371f, 0x0c},
{0x3739, 0x30},
{0x373c, 0x20},
{0x3781, 0x0c},
{0x3786, 0x16},
{0x3796, 0x64},
{0x3800, 0x00},
{0x3801, 0x00},
{0x3802, 0x00},
{0x3803, 0x00},
{0x3804, 0x0c},
{0x3805, 0xcf},
{0x3806, 0x09},
{0x3807, 0x9f},
{0x3808, 0x06},
{0x3809, 0x60},
{0x380a, 0x04},
{0x380b, 0xc8},
{0x380c, 0x0e},
{0x380d, 0x18},
{0x380e, 0x05},
{0x380f, 0xa0},
{0x3814, 0x31},
{0x3815, 0x31},
{0x3820, 0x11},
{0x3821, 0x0f},
{0x3a04, 0x05},
{0x3a05, 0x9d},
{0x4001, 0x02},
{0x4004, 0x04},
{0x4005, 0x18},
{0x404f, 0xa0},
{0x0100, 0x01},
{0x0100, 0x01},



//group 1
//preview setting 1632*1224

{OV8850_REG_DELAY, 50},
{0x3200, 0x00}, // group 0 start addr 0
{0x3208, 0x00},
//{0x0100, 0x00},
{0x3624, 0x00},
{0x3680, 0xe0},
{0x3702, 0xf3},
{0x3704, 0x71},
{0x3708, 0xe6},
{0x3709, 0xc3},
{0x371f, 0x0c},
{0x3739, 0x30},
{0x373c, 0x20},
{0x3781, 0x0c},
{0x3786, 0x16},
{0x3796, 0x64},
{0x3800, 0x00},
{0x3801, 0x00},
{0x3802, 0x00},
{0x3803, 0x00},
{0x3804, 0x0c},
{0x3805, 0xcf},
{0x3806, 0x09},
{0x3807, 0x9f},
{0x3808, 0x06},
{0x3809, 0x60},
{0x380a, 0x04},
{0x380b, 0xc8},
{0x380c, 0x0e},
{0x380d, 0x18},

#ifndef  AUTO_FPS_CTRL
//{0x380e, 0x05},
//{0x380f, 0xa0},
#endif

{0x3814, 0x31},
{0x3815, 0x31},
{0x3820, 0x11},
{0x3821, 0x0f},
{0x3a04, 0x05},
{0x3a05, 0x9d},
{0x4001, 0x02},
//{0x4004, 0x04},
{0x4005, 0x18},
{0x404f, 0xa0},
//{0x0100, 0x01},
{0x3208, 0x10}, //group 0 end
#if 1

//for capture setting 3264*2448
{OV8850_REG_DELAY, 5},
{0x3203, 0x18}, // group 1 start addr 0x80
{0x3208, 0x03},
//{0x0100, 0x00},
{0x3624, 0x04},
{0x3680, 0xb0},
{0x3702, 0x6e},
{0x3704, 0x55},
{0x3708, 0xe4},
{0x3709, 0xc3},
{0x371f, 0x0d},
{0x3739, 0x80},
{0x373c, 0x24},
{0x3781, 0xc8},
{0x3786, 0x08},
{0x3796, 0x43},
{0x3800, 0x00},
{0x3801, 0x04},
{0x3802, 0x00},
{0x3803, 0x0c},
{0x3804, 0x0c},
{0x3805, 0xcb},
{0x3806, 0x09},
{0x3807, 0xa3},
{0x3808, 0x0c},
{0x3809, 0xc0},
{0x380a, 0x09},
{0x380b, 0x90},
{0x380c, 0x0e},
{0x380d, 0x88},
#ifndef AUTO_FPS_CTRL
//{0x380e, 0x0b},
//{0x380f, 0x42},
#endif
{0x3814, 0x11},
{0x3815, 0x11},
{0x3820, 0x10},
{0x3821, 0x0e},
{0x3a04, 0x09},
{0x3a05, 0xcc},
{0x4001, 0x06},
{0x4004, 0x04},
{0x4005, 0x1a},
//{0x0100, 0x01},
{0x3208, 0x13},//group 1 end

#endif

#if 1
// for 1080p 2880*1620
{OV8850_REG_DELAY, 5},
{0x3201, 0x08}, //  // group 1 start addr 0x80
{0x3208, 0x01},
//{0x0100, 0x00},
{0x3624, 0x04},
{0x3680, 0xb0},
{0x3702, 0x6e},
{0x3704, 0x55},
{0x3708, 0xe4},
{0x3709, 0xc3},
{0x371f, 0x0d},
{0x3739, 0x80},
{0x373c, 0x24},
{0x3781, 0xc8},
{0x3786, 0x08},
{0x3796, 0x43},
{0x3800, 0x00},
{0x3801, 0xc4},
{0x3802, 0x01},
{0x3803, 0xaa},
{0x3804, 0x0c},
{0x3805, 0x0b},
{0x3806, 0x08},
{0x3807, 0x05},
{0x3808, 0x0b},
{0x3809, 0x40},
{0x380a, 0x06},
{0x380b, 0x54},
{0x380c, 0x0c},
{0x380d, 0x00},
#ifndef AUTO_FPS_CTRL
//{0x380e, 0x06},
//{0x380f, 0x9b},
#endif
{0x3814, 0x11},
{0x3815, 0x11},
{0x3820, 0x10},
{0x3821, 0x0e},
{0x3a04, 0x06},
{0x3a05, 0x98},
{0x4001, 0x06},
{0x4004, 0x04},
{0x4005, 0x1a},
//{0x0100, 0x01},
{0x3208, 0x11},//group 1 end
#endif


//enter pl11 state

//{OV8850_REG_DELAY, 10},
	{0x4202, 0x0f},
	{0x4800, 0x01},

	{OV8850_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8850_stream_on[] = {
	{0x4202, 0x00},
	{0x4800, 0x24},

	{OV8850_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8850_stream_off[] = {
	/* Sensor enter LP11*/
//	{0x4202, 0x0f},
//	{0x4800, 0x01},

	{OV8850_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8850_win_uxga[] = {

{0x0100, 0x00},
{0x3624, 0x00},
{0x3680, 0xe0},
{0x3702, 0xf3},
{0x3704, 0x71},
{0x3708, 0xe6},
{0x3709, 0xc3},
{0x371f, 0x0c},
{0x3739, 0x30},
{0x373c, 0x20},
{0x3781, 0x0c},
{0x3786, 0x16},
{0x3796, 0x64},
{0x3800, 0x00},
{0x3801, 0x00},
{0x3802, 0x00},
{0x3803, 0x00},
{0x3804, 0x0c},
{0x3805, 0xcf},
{0x3806, 0x09},
{0x3807, 0x9f},
{0x3808, 0x06},
{0x3809, 0x60},
{0x380a, 0x04},
{0x380b, 0xc8},
{0x380c, 0x0e},
{0x380d, 0x18},

#ifndef  AUTO_FPS_CTRL
{0x380e, 0x05},
{0x380f, 0xa0},
#endif
{0x3814, 0x31},
{0x3815, 0x31},
{0x3820, 0x11},
{0x3821, 0x0f},
{0x3a04, 0x05},
{0x3a05, 0x9d},
{0x4001, 0x02},
{0x4004, 0x04},
{0x4005, 0x18},

{0x404f, 0xa0},
{0x0100, 0x01},
{OV8850_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list ov8850_win_8m[] = {

{0x0100, 0x00},
{0x3624, 0x04},
{0x3680, 0xb0},

{0x3702, 0x6e},
{0x3704, 0x55},
{0x3708, 0xe4},
{0x3709, 0xc3},
{0x371f, 0x0d},
{0x3739, 0x80},
{0x373c, 0x24},
{0x3781, 0xc8},
{0x3786, 0x08},
{0x3796, 0x43},
{0x3800, 0x00},
{0x3801, 0x04},
{0x3802, 0x00},
{0x3803, 0x0c},
{0x3804, 0x0c},
{0x3805, 0xcb},
{0x3806, 0x09},
{0x3807, 0xa3},
{0x3808, 0x0c},
{0x3809, 0xc0},
{0x380a, 0x09},
{0x380b, 0x90},
{0x380c, 0x0e},
{0x380d, 0x88},

#ifndef AUTO_FPS_CTRL
{0x380e, 0x0b},
{0x380f, 0x42},
#endif
{0x3814, 0x11},
{0x3815, 0x11},
{0x3820, 0x10},
{0x3821, 0x0e},
{0x3a04, 0x09},
{0x3a05, 0xcc},
{0x4001, 0x06},
{0x4004, 0x04},

{0x4005, 0x1a},
{0x0100, 0x01},

{OV8850_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list ov8850_win_1080p[] = {
{0x0100, 0x00},
{0x3624, 0x04},
{0x3680, 0xb0},
{0x3702, 0x6e},
{0x3704, 0x55},
{0x3708, 0xe4},
{0x3709, 0xc3},
{0x371f, 0x0d},
{0x3739, 0x80},
{0x373c, 0x24},
{0x3781, 0xc8},
{0x3786, 0x08},
{0x3796, 0x43},
{0x3800, 0x00},
{0x3801, 0xc4},
{0x3802, 0x01},
{0x3803, 0xaa},
{0x3804, 0x0c},
{0x3805, 0x0b},
{0x3806, 0x08},
{0x3807, 0x05},
{0x3808, 0x0b},
{0x3809, 0x40},
{0x380a, 0x06},
{0x380b, 0x54},

{0x380c, 0x0c},
{0x380d, 0x00},
#ifndef AUTO_FPS_CTRL
{0x380e, 0x06},
{0x380f, 0x9b},
#endif
{0x3814, 0x11},
{0x3815, 0x11},
{0x3820, 0x10},
{0x3821, 0x0e},
{0x3a04, 0x06},
{0x3a05, 0x98},
{0x4001, 0x06},
{0x4004, 0x04},
{0x4005, 0x1a},
{0x0100, 0x01},
{OV8850_REG_END, 0x00},	/* END MARKER */
};

static int ov8850_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov8850_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov8850_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV8850_REG_END) {
		if (vals->reg_num == OV8850_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov8850_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

void ov8850_clear_otp_buffer(struct v4l2_subdev *sd)
{
int i;
// clear otp buffer
for (i=0;i<16;i++) {
ov8850_write(sd,0x3d00 + i, 0x00);
}
}


static int ov8850_otp_read(struct v4l2_subdev *sd, unsigned short reg)
{
	int flag = 0;
	unsigned char value=0x0;
	ov8850_read(sd,reg,&value);
	flag = (int)value;
	return flag;
}
/* R/G and B/G of typical camera module is defined here. */
static unsigned int RG_Ratio_Typical = 85;
static unsigned int BG_Ratio_Typical = 85;

// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 3, group index has valid data

static int ov8850_check_otp_wb(struct v4l2_subdev *sd, int index)
{

	int flag;
	int bank, address;
	// select bank index
	bank = 0xc0 | index;
	ov8850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	// read flag
	address = 0x3d00;
	flag = ov8850_otp_read(sd,address);
	flag = flag & 0xc0;
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
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

// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 3, group index has valid data
static int ov8850_check_otp_lenc(struct v4l2_subdev *sd, int index)
{
	int flag, bank;
	int address;
	// select bank: index*4
	bank = 0xc0 | (index<<2);
	ov8850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	// read flag
	address = 0x3d00;
	flag = ov8850_otp_read(sd,address);
	flag = flag & 0xc0;
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
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

#ifdef OV8850_OTP__VCM_FUNC
static int ov8850_check_otp_VCM(struct v4l2_subdev *sd, unsigned short index,int code)
{
	int flag, bank;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	ov8850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	// read flag
	address = 0x3d00 + index*4 + code*2;
	flag = ov8850_otp_read(sd,address);
	flag = flag & 0xc0;
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	//clear_otp_buffer();
	ov8850_clear_otp_buffer(sd);
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
// code: 0 start code, 1 stop code
// return: 0

int ov8850_read_otp_VCM(struct v4l2_subdev *sd, int index, int code, struct otp_struct * otp_ptr)
{
	int vcm, bank;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	ov8850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	// read flag
	address = 0x3d00 + index*4 + code*2;
	vcm = ov8850_otp_read(sd,address);
	vcm = (vcm & 0x03) + (ov8850_otp_read(sd,address)<<2);
	if(code==1) {
	(* otp_ptr).VCM_end = vcm;
	}
	else {
	(* otp_ptr).VCM_start = vcm;
	}
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
	return 0;
}
#endif


// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return: 0,
static int ov8850_read_otp_wb(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp_ptr)
{
	int bank;
	int address;
	// select bank index
	bank = 0xc0 | index;
	ov8850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	address = 0x3d00;
	(*otp_ptr).module_integrator_id = ov8850_otp_read(sd,address + 1);
	(*otp_ptr).lens_id = ov8850_otp_read(sd,address + 2);
	(*otp_ptr).production_year = ov8850_otp_read(sd,address + 3);
	(*otp_ptr).production_month = ov8850_otp_read(sd,address + 4);
	(*otp_ptr).production_day = ov8850_otp_read(sd,address + 5);
	(*otp_ptr).rg_ratio = ov8850_otp_read(sd,address + 6);
	(*otp_ptr).bg_ratio = ov8850_otp_read(sd,address + 7);
	(*otp_ptr).light_rg = ov8850_otp_read(sd,address + 8);
	(*otp_ptr).light_bg = ov8850_otp_read(sd,address + 9);
	(*otp_ptr).user_data[0] = ov8850_otp_read(sd,address + 10);
	(*otp_ptr).user_data[1] = ov8850_otp_read(sd,address + 11);
	(*otp_ptr).user_data[2] = ov8850_otp_read(sd,address + 12);
	(*otp_ptr).user_data[3] = ov8850_otp_read(sd,address + 13);
	(*otp_ptr).user_data[4] = ov8850_otp_read(sd,address + 14);
	(*otp_ptr).user_data[5] = ov8850_otp_read(sd,address + 15);
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
	return 0;
}


// index: index of otp group. (0, 1, 2)
// otp_ptr: pointer of otp_struct
// return: 0,

static int ov8850_read_otp_lenc(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp_ptr)
{
	int bank, i;
	int address;
	// select bank: index*4
	bank = 0xc0 + (index<<2);
	ov8850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	address = 0x3d01;
	for(i=0;i<15;i++) {
	(* otp_ptr).lenc[i]=ov8850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
	// select 2nd bank
	bank++;
	ov8850_write(sd,0x3d84, bank + 0xc0);
	// read otp
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	address = 0x3d00;
	for(i=15;i<31;i++) {
	(* otp_ptr).lenc[i]=ov8850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
	// select 3rd bank
	bank++;
	ov8850_write(sd,0x3d84, bank + 0xc0);
	// read otp
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	address = 0x3d00;
	for(i=31;i<47;i++) {
	(* otp_ptr).lenc[i]=ov8850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
	// select 4th bank
	bank++;
	ov8850_write(sd,0x3d84, bank + 0xc0);
	// read otp
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	address = 0x3d00;
	for(i=47;i<62;i++) {
	(* otp_ptr).lenc[i]=ov8850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
	return 0;

}





/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov8850_update_awb_gain(struct v4l2_subdev *sd, 
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	printk("R_gain:%04x, G_gain:%04x, B_gain:%04x \n ", R_gain, G_gain, B_gain);
	if (R_gain>0x400) {
	ov8850_write(sd,0x3400, R_gain>>8);
	ov8850_write(sd,0x3401, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
	ov8850_write(sd,0x3402, G_gain>>8);
	ov8850_write(sd,0x3403, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
	ov8850_write(sd,0x3404, B_gain>>8);
	ov8850_write(sd,0x3405, B_gain & 0x00ff);
	}
	return 0;
}

// otp_ptr: pointer of otp_struct
static int ov8850_update_lenc(struct v4l2_subdev *sd,struct otp_struct * otp_ptr)
{
	int i, temp;
	temp = ov8850_otp_read(sd,0x5000);
	temp = 0x80 | temp;
	ov8850_write(sd,0x5000, temp);
	for(i=0;i<62;i++) {
	ov8850_write(sd,0x5800 + i, (*otp_ptr).lenc[i]);
	}
	return 0;
}

// call this function after OV8850 initialization
// return value: 0 update success
// 1, no OTP
static int ov8850_update_otp_wb(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=1;i<=3;i++) {
	temp =ov8850_check_otp_wb(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i>3) {
	// no valid wb OTP data
	return 1;
	}
	ov8850_read_otp_wb(sd,otp_index, &current_otp);
	if(current_otp.light_rg==0) {
	// no light source information in OTP, light factor = 1
	rg = current_otp.rg_ratio;
	}
	else {
	rg = current_otp.rg_ratio * (current_otp.light_rg +512) / 1024;
	}
	if(current_otp.light_bg==0) {
	// not light source information in OTP, light factor = 1
	bg = current_otp.bg_ratio;
	}
	else {
	bg = current_otp.bg_ratio * (current_otp.light_bg +512) / 1024;
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
	ov8850_update_awb_gain(sd,R_gain, G_gain, B_gain);
	return 0;
}

// call this function after OV8850 initialization
// return value: 0 update success
// 1, no OTP
int ov8850_update_otp_lenc(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	// check first lens correction OTP with valid data
	for(i=1;i<=3;i++) {
	temp = ov8850_check_otp_lenc(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}

	if (i>3) {
	// no valid wb OTP data
	return 1;
	}
	ov8850_read_otp_lenc(sd,otp_index, &current_otp);
	ov8850_update_lenc(sd,&current_otp);
	// success
	return 0;
}

// return: 0 ¡§Cuse module DCBLC, 1 ¡§Cuse sensor DCBL 2 ¡§Cuse defualt DCBLC
int ov8850_check_dcblc(struct v4l2_subdev *sd)
{
	int bank, dcblc;
	int address;
	int temp, flag;
	// select bank 31
	bank = 0xc0 | 31;
	ov8850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov8850_write(sd,0x3d81, 0x01);
	msleep(1);
	temp = ov8850_otp_read(sd,0x4000);
	address = 0x3d0b;
	dcblc = ov8850_otp_read(sd,address);
	if ((dcblc>=0x15) && (dcblc<=0x40)){
	// module DCBLC value is valid
	if((temp && 0x08)==0) {
	// DCBLC auto load
	flag = 0;
	ov8850_clear_otp_buffer(sd);
	return flag;
	}
	}
	temp = temp | 0x08; // DCBLC manual load enable
	ov8850_write(sd,0x4000, temp);
	address--;
	dcblc = ov8850_otp_read(sd,address);
	if ((dcblc>=0x10) && (dcblc<=0x40)){
	// sensor DCBLC value is valid
	ov8850_write(sd,0x4006, dcblc); // manual load sensor level DCBLC
	flag = 1; // sensor level DCBLC is used
	}
	else{
	ov8850_write(sd,0x4006, 0x20);
	flag = 2; // default DCBLC is used
	}
	// disable otp read
	ov8850_write(sd,0x3d81, 0x00);
	ov8850_clear_otp_buffer(sd);
	return flag;
}


static int ov8850_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov8850_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov8850_info *info = container_of(sd, struct ov8850_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->dynamic_framerate = 0;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 2;
	info->test = 0;

	ret=ov8850_write_array(sd, ov8850_init_regs);

	if (ret < 0)
		return ret;

	printk("*update_otp_wb*\n");
	ret = ov8850_update_otp_wb(sd);
	if (ret < 0)
		return ret;
	printk("*update_otp_lenc*\n");
	ret = ov8850_update_otp_lenc(sd);


	return ret;
}

static int ov8850_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	printk("*ov8850_detect start*\n");
	ret = ov8850_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != OV8850_CHIP_ID_H)
		return -ENODEV;
	ret = ov8850_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_M=0x%x \n", v);
	if (v != OV8850_CHIP_ID_M)
		return -ENODEV;
	return 0;
}

static struct ov8850_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov8850_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV8850_FMTS ARRAY_SIZE(ov8850_formats)

static struct ov8850_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
} ov8850_win_sizes[] = {
	/* UXGA */
	{
		.width		= UXGA_WIDTH,
		.height		= UXGA_HEIGHT,
		.vts		= 0x5a0, //60hz banding step ox168
		.framerate	= 30,
		.max_gain_dyn_frm = 0x80,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x80,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov8850_win_uxga,
	},
	{
		.width		= HDV_WIDTH,
		.height 	= HDV_HEIGHT,
		.vts		= 0x69b,//60hz banding step ox1A6
		.max_gain_dyn_frm = 0x80,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x80,
		.min_gain_fix_frm = 0x10,
		.regs		= ov8850_win_1080p,
	},
	/* 3264*2448 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0xb42,//60hz banding step ox168
		.framerate	= 15,
		.max_gain_dyn_frm = 0x80,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x80,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov8850_win_8m,
	},	
};
#define N_WIN_SIZES (ARRAY_SIZE(ov8850_win_sizes))

static int ov8850_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV8850_FMTS)
		return -EINVAL;

	*code = ov8850_formats[index].mbus_code;
	return 0;
}

static int ov8850_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov8850_format_struct **ret_fmt,
		struct ov8850_win_size **ret_wsize)
{
	int index;
	struct ov8850_win_size *wsize;

	for (index = 0; index < N_OV8850_FMTS; index++)
		if (ov8850_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV8850_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov8850_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov8850_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov8850_win_sizes; wsize < ov8850_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov8850_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */

	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov8850_formats[index].colorspace;
	return 0;
}

static int ov8850_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov8850_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov8850_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov8850_info *info = container_of(sd, struct ov8850_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov8850_format_struct *fmt_s;
	struct ov8850_win_size *wsize;
	int ret;
	int len,i;

	ret = ov8850_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	//data->vts = wsize->vts;
	//data->mipi_clk = 689; /* Mbps. */

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov8850_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		//ret = ov8850_write_array(sd, wsize->regs);
		//if (ret)
			//return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 663;  //527/*Mbps*/
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
			data->reg[i].addr = 0x3208;
			data->reg[i].data = 0xa3;
		
	
			if (!info->dynamic_framerate) {
				data->reg[1].addr = info->vts_address1;
				data->reg[1].data = data->vts >> 8;
				data->reg[2].addr = info->vts_address2;
				data->reg[2].data = data->vts & 0x00ff;
				data->reg_num = 3;
			}
		} else if ((wsize->width == UXGA_WIDTH)
			&& (wsize->height == UXGA_HEIGHT)) {
			printk("switch to preview!!\n");
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			len = ARRAY_SIZE(ov8850_win_uxga);
		
			data->reg_num = 1;
			data->reg[i].addr = 0x3208;
			data->reg[i].data = 0xa0;
			
	
			if (!info->dynamic_framerate) {
				data->reg[1].addr = info->vts_address1;
				data->reg[1].data = data->vts >> 8;
				data->reg[2].addr = info->vts_address2;
				data->reg[2].data = data->vts & 0x00ff;
				data->reg_num = 3;
			}
		}else if ((wsize->width == HDV_WIDTH)
			&& (wsize->height == HDV_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			len = ARRAY_SIZE(ov8850_win_1080p);
			data->reg_num = 1;
			data->reg[i].addr = 0x3208;
			data->reg[i].data = 0xa1;
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

static int ov8850_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) 
		ret = ov8850_write_array(sd, ov8850_stream_on);
	else
		ret = ov8850_write_array(sd, ov8850_stream_off);
	return ret;
}

static int ov8850_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov8850_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov8850_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov8850_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov8850_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov8850_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov8850_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov8850_frame_rates[interval->index];
	return 0;
}

static int ov8850_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov8850_win_size *win = &ov8850_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov8850_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov8850_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov8850_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov8850_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov8850_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov8850_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct ov8850_info *info = container_of(sd, struct ov8850_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		if (ctrl->value == FRAME_RATE_AUTO)
			info->dynamic_framerate = 1;
		else
			ret = -EINVAL;
		break;
	default:
		ret = -ENOIOCTLCMD; 
	}
	
	return ret;
}

static int ov8850_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV8850, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov8850_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov8850_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov8850_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov8850_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov8850_core_ops = {
	.g_chip_ident = ov8850_g_chip_ident,
	.g_ctrl = ov8850_g_ctrl,
	.s_ctrl = ov8850_s_ctrl,
	.queryctrl = ov8850_queryctrl,
	.reset = ov8850_reset,
	.init = ov8850_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov8850_g_register,
	.s_register = ov8850_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov8850_video_ops = {
	.enum_mbus_fmt = ov8850_enum_mbus_fmt,
	.try_mbus_fmt = ov8850_try_mbus_fmt,
	.s_mbus_fmt = ov8850_s_mbus_fmt,
	.s_stream = ov8850_s_stream,
	.cropcap = ov8850_cropcap,
	.g_crop	= ov8850_g_crop,
	.s_parm = ov8850_s_parm,
	.g_parm = ov8850_g_parm,
	.enum_frameintervals = ov8850_enum_frameintervals,
	.enum_framesizes = ov8850_enum_framesizes,
};

static const struct v4l2_subdev_ops ov8850_ops = {
	.core = &ov8850_core_ops,
	.video = &ov8850_video_ops,
};

static ssize_t ov8850_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", RG_Ratio_Typical);
}

static ssize_t ov8850_rg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	RG_Ratio_Typical = (unsigned int)value;

	return size;
}

static ssize_t ov8850_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", BG_Ratio_Typical);
}

static ssize_t ov8850_bg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	BG_Ratio_Typical = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov8850_rg_ratio_typical, 0664, ov8850_rg_ratio_typical_show, ov8850_rg_ratio_typical_store);
static DEVICE_ATTR(ov8850_bg_ratio_typical, 0664, ov8850_bg_ratio_typical_show, ov8850_bg_ratio_typical_store);

static int ov8850_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov8850_info *info;
	int ret;

	info = kzalloc(sizeof(struct ov8850_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov8850_ops);

	/* Make sure it's an ov8850 */
	ret = ov8850_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov8850 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov8850 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ret = device_create_file(&client->dev, &dev_attr_ov8850_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov8850_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov8850_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov8850_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov8850_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov8850_bg_ratio_typical;
	}

	return 0;

err_create_dev_attr_ov8850_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_ov8850_rg_ratio_typical);
err_create_dev_attr_ov8850_rg_ratio_typical:
	kfree(info);
	return ret;
}

static int ov8850_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8850_info *info = container_of(sd, struct ov8850_info, sd);

	device_remove_file(&client->dev, &dev_attr_ov8850_rg_ratio_typical);
	device_remove_file(&client->dev, &dev_attr_ov8850_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov8850_id[] = {
	{ "ov8850", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov8850_id);

static struct i2c_driver ov8850_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov8850",
	},
	.probe		= ov8850_probe,
	.remove		= ov8850_remove,
	.id_table	= ov8850_id,
};

static __init int init_ov8850(void)
{
	return i2c_add_driver(&ov8850_driver);
}

static __exit void exit_ov8850(void)
{
	i2c_del_driver(&ov8850_driver);
}

fs_initcall(init_ov8850);
module_exit(exit_ov8850);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov8850 sensors");
MODULE_LICENSE("GPL");
