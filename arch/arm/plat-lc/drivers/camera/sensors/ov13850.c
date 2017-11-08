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
#include "ov13850.h"

//#define OV13850_DEBUG_FUNC
//#define OV13850_CHECK_OTP_VCM_FUN

#define OV13850_CHIP_ID_H	(0xd8)
#define OV13850_CHIP_ID_L	(0x50)

#define QXGA_WIDTH		2112
#define QXGA_HEIGHT		1568
#define QSXGA_WIDTH		3264
#define QSXGA_HEIGHT	2448
#if 1
#define MAX_WIDTH		4224
#define MAX_HEIGHT		3136
#else
#define MAX_WIDTH		QSXGA_WIDTH
#define MAX_HEIGHT		QSXGA_HEIGHT
#endif

#define MAX_PREVIEW_WIDTH	QSXGA_WIDTH//QXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  QSXGA_WIDTH//QXGA_HEIGHT


#define OV13850_REG_END		0xffff
#define OV13850_REG_DELAY	0xfffe

#define OV13850_REG_VAL_SNAPSHOT  0xff

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals);

struct ov13850_format_struct;

struct ov13850_info {
	struct v4l2_subdev sd;
	struct ov13850_format_struct *fmt;
	struct ov13850_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int dynamic_framerate;
};


struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};


struct otp_struct {
//	int customer_id;
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int user_data[5];
	int lenc[62];
	int VCM_end;
	int VCM_start;
};


static struct regval_list ov13850_init_regs[] = {
	{0x0103,0x01},
	{OV13850_REG_DELAY, 5},

	{0x0300,0x01},	//;for 641.33Mbps
	{0x0301,0x00},
	{0x0302,0x25},
	{0x0303,0x00},
	{0x030a,0x00},
	{0x300f,0x11},
	{0x3010,0x01},
	{0x3011,0x76},
	{0x3012,0x41},
	{0x3013,0x12},
	{0x3014,0x11},
	{0x301f,0x03},
	{0x3106,0x00},
	{0x3210,0x47},
	{0x3500,0x00},
	{0x3501,0x60},
	{0x3502,0x00},
	{0x3506,0x00},
	{0x3507,0x02},
	{0x3508,0x00},
	{0x350a,0x00},
	{0x350b,0x80},
	{0x350e,0x00},
	{0x350f,0x10},
	{0x3600,0x40},
	{0x3601,0xfc},
	{0x3602,0x02},
	{0x3603,0x48},
	{0x3604,0xa5},
	{0x3605,0x9f},
	{0x3607,0x00},
	{0x360a,0x40},
	{0x360b,0x91},
	{0x360c,0x49},
	{0x360f,0x8a},
	{0x3611,0x10},
	{0x3612,0x27},
	{0x3613,0x33},
	{0x3614,0x25}, //;for 60.13Mhz sclk
	{0x3615,0x08},
	{0x3641,0x02},
	{0x3660,0x82},
	{0x3668,0x54},
	{0x3669,0x00},
	{0x3667,0xa0},
	{0x3702,0x40},
	{0x3703,0x44},
	{0x3704,0x2c},
	{0x3705,0x24},
	{0x3706,0x50},
	{0x3707,0x44},
	{0x3708,0x3c},
	{0x3709,0x1f},
	{0x370a,0x26},
	{0x370b,0x3c},
	{0x3720,0x66},
	{0x3722,0x84},
	{0x3728,0x40},
	{0x372a,0x00},
	{0x372e,0x22},
	{0x372f,0xa0},
	{0x3730,0x00},
	{0x3731,0x00},
	{0x3732,0x00},
	{0x3733,0x00},
	{0x3748,0x00},
	{0x3710,0x28},
	{0x3716,0x03},
	{0x3718,0x1C},
	{0x3719,0x08},
	{0x371c,0xfc},
	{0x3760,0x13},
	{0x3761,0x34},
	{0x3762,0x86},
	{0x3763,0x16},
	{0x3767,0x24},
	{0x3768,0x06},
	{0x3769,0x45},
	{0x376c,0x23},
	{0x3d84,0x00},
	{0x3d85,0x17},// ;
	{0x3d8c,0x73}, //;
	{0x3d8d,0xbf}, //;
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x04},
	{0x3804,0x10},
	{0x3805,0x9f},
	{0x3806,0x0c},
	{0x3807,0x4b},
	{0x3808,0x08},
	{0x3809,0x40},
	{0x380a,0x06},
	{0x380b,0x20},
	{0x380c,0x09},
	{0x380d,0x60},
	{0x380e,0x0d},
	{0x380f,0x00},
	{0x3810,0x00},
	{0x3811,0x08},
	{0x3812,0x00},
	{0x3813,0x02},
	{0x3814,0x31},
	{0x3815,0x31},
	{0x3820,0x01},
	{0x3821,0x04},
	{0x3834,0x00},
	{0x3835,0x1c},
	{0x3836,0x08},
	{0x3837,0x02},
	{0x4000,0xf1},
	{0x4001,0x00},
	{0x4006,0x1f},
	{0x4007,0x1f},
	{0x400b,0x0c},
	{0x4011,0x00},
	{0x401a,0x00},
	{0x401b,0x00},
	{0x401c,0x00},
	{0x401d,0x00},
	{0x4020,0x00},
	{0x4021,0xe4},
	{0x4022,0x04},
	{0x4023,0xd7},
	{0x4024,0x05},
	{0x4025,0xbc},
	{0x4026,0x05},
	{0x4027,0xbf},
	{0x4028,0x00},
	{0x4029,0x02},
	{0x402a,0x04},
	{0x402b,0x08},
	{0x402c,0x02},
	{0x402d,0x02},
	{0x402e,0x0c},
	{0x402f,0x08},
	{0x403d,0x2c},
	{0x403f,0x7f},
	{0x4500,0x82},
	{0x4501,0x3c},
	{0x4601,0x83},
	{0x4602,0x22},
	{0x4603,0x01},
	{0x4837,0x19},
	{0x4d00,0x04},
	{0x4d01,0x42},
	{0x4d02,0xd1},
	{0x4d03,0x90},
	{0x4d04,0x66},
	{0x4d05,0x65},
	{0x5000,0x0e},
	{0x5001,0x01},
	{0x5002,0x07},
	{0x5013,0x40},
	{0x501c,0x00},
	{0x501d,0x10},
	{0x5242,0x00},
	{0x5243,0xb8},
	{0x5244,0x00},
	{0x5245,0xf9},
	{0x5246,0x00},
	{0x5247,0xf6},
	{0x5248,0x00},
	{0x5249,0xa6},
	{0x5300,0xfc},
	{0x5301,0xdf},
	{0x5302,0x3f},
	{0x5303,0x08},
	{0x5304,0x0c},
	{0x5305,0x10},
	{0x5306,0x20},
	{0x5307,0x40},
	{0x5308,0x08},
	{0x5309,0x08},
	{0x530a,0x02},
	{0x530b,0x01},
	{0x530c,0x01},
	{0x530d,0x0c},
	{0x530e,0x02},
	{0x530f,0x01},
	{0x5310,0x01},
	{0x5400,0x00},
	{0x5401,0x61},
	{0x5402,0x00},
	{0x5403,0x00},
	{0x5404,0x00},
	{0x5405,0x40},
	{0x540c,0x05},
	{0x5b00,0x00},
	{0x5b01,0x00},
	{0x5b02,0x01},
	{0x5b03,0xff},
	{0x5b04,0x02},
	{0x5b05,0x6c},
	{0x5b09,0x02},
	{0x5e00,0x00},
	{0x5e10,0x1c},
	{0x0102,0x01},
	{OV13850_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list ov13850_stream_on[] = {
//	{0x4202, 0x00},
//	{0x4800, 0x04},
	{0x0100,0x01},


	{OV13850_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov13850_stream_off[] = {
	/* Sensor enter LP11*/
//	{0x4202, 0x0f},
//	{0x4800, 0x01},
	{0x0100, 0x00},

	{OV13850_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list ov13850_win_qxga[] = {
//	{0x0100,0x00},
	{0x0300,0x01}, //;FOR 641.33Mbps
	{0x0302,0x25},
	{0x0303,0x00},
	{0x3612,0x27},
	{0x370a,0x26},
	{0x372a,0x00},
	{0x3718,0x1C},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x04},
	{0x3804,0x10},
	{0x3805,0x9f},
	{0x3806,0x0c},
	{0x3807,0x4b},
	{0x3808,0x08},
	{0x3809,0x40},
	{0x380a,0x06},
	{0x380b,0x20},
	{0x380c,0x09},
	{0x380d,0x60},
	{0x380e,0x0d},
	{0x380f,0x00},
	{0x3810,0x00},
	{0x3811,0x08},
	{0x3812,0x00},
	{0x3813,0x02},
	{0x3814,0x31},
	{0x3815,0x31},
	{0x3820,0x01},
	{0x3821,0x00},
	{0x3836,0x08},
	{0x3837,0x02},
	{0x4020,0x00},
	{0x4021,0xe4},
	{0x4022,0x04},
	{0x4023,0xd7},
	{0x4024,0x05},
	{0x4025,0xbc},
	{0x4026,0x05},
	{0x4027,0xbf},
	{0x4501,0x3c},
	{0x4601,0x83},
	{0x4603,0x01},
	{0x4837,0x19},
	{0x5401,0x61},
	{0x5405,0x40},
	//{0x0100,0x01},


//	{OV13850_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov13850_win_qsxga[] = {
	{0x0300,0x00},
	{0x0302,0x25},
	{0x3612,0x07},//26:17fps//27:15fps //25:20fps
	{0x370a,0x24},
	{0x372a,0x04},
	{0x372f,0xa0},
	{0x3800,0x00},
	{0x3801,0x0C},
	{0x3802,0x00},
	{0x3803,0x04},
	{0x3804,0x10},
	{0x3805,0x93},
	{0x3806,0x0c},
	{0x3807,0x4B},
	{0x3808,0x0c},
	{0x3809,0xc0},
	{0x380a,0x09},
	{0x380b,0x90},
	{0x380c,0x12},
	{0x380d,0xc0},
	{0x380e,0x0d}, //
	{0x380f,0x00},
	{0x3810,0x00},
	{0x3811,0xec},
	{0x3812,0x01},
	{0x3813,0x50},
	{0x3814,0x11},
	{0x3815,0x11},
	{0x3820,0x00},
	{0x3821,0x04},
	{0x3836,0x04},
	{0x3837,0x01},
	{0x4020,0x02},
	{0x4021,0x4C},
	{0x4022,0x0E},
	{0x4023,0x37},
	{0x4024,0x0F},
	{0x4025,0x1C},
	{0x4026,0x0F},
	{0x4027,0x1F},
	{0x4603,0x00},
	{0x4837,0x11},
	{0x5401,0x71},
	{0x5405,0x80},

//	{OV13850_REG_END, 0x00},	/* END MARKER */
};
#if 1
static struct regval_list ov13850_win_13m[] = {

	{0x0300,0x00},
	{0x0302,0x25},//25:24fps
	{0x0303,0x00},
	{0x3612,0x07},
	{0x370a,0x24},
	{0x372a,0x04},
	{0x3718,0x10},
	{0x3800,0x00},
	{0x3801,0x0C},
	{0x3802,0x00},
	{0x3803,0x04},
	{0x3804,0x10},
	{0x3805,0x93},
	{0x3806,0x0c},
	{0x3807,0x4B},
	{0x3808,0x10},
	{0x3809,0x80},
	{0x380a,0x0c},
	{0x380b,0x40},
	{0x380c,0x17},
	{0x380d,0x80},
	{0x380e,0x0d},
	{0x380f,0x00},
	{0x3810,0x00},
	{0x3811,0x04},
	{0x3812,0x00},
	{0x3813,0x04},
	{0x3814,0x11},
	{0x3815,0x11},
	{0x3820,0x00},
	{0x3821,0x00},
	{0x3836,0x04},
	{0x3837,0x01},
	{0x4020,0x02},
	{0x4021,0x4C},
	{0x4022,0x0E},
	{0x4023,0x37},
	{0x4024,0x0F},
	{0x4025,0x1C},
	{0x4026,0x0F},
	{0x4027,0x1F},
	{0x4501,0x38},
	{0x4601,0x04},
	{0x4603,0x01},
	{0x4837,0x11},
	{0x5401,0x71},
	{0x5405,0x80},


//{OV13850_REG_END, 0x00},	/* END MARKER */
};
#else
static struct regval_list ov13850_win_13m[] = {//30fps

	{0x0300,0x00},
	{0x0302,0x2e},//
	{0x0303,0x00},
	{0x3612,0x07},
	{0x370a,0x24},
	{0x372a,0x04},
	{0x3718,0x10},
	{0x3800,0x00},
	{0x3801,0x0C},
	{0x3802,0x00},
	{0x3803,0x04},
	{0x3804,0x10},
	{0x3805,0x93},
	{0x3806,0x0c},
	{0x3807,0x4B},
	{0x3808,0x10},
	{0x3809,0x80},
	{0x380a,0x0c},
	{0x380b,0x40},
	{0x380c,0x14},//
	{0x380d,0x52},//
	{0x380e,0x0d},
	{0x380f,0x00},
	{0x3810,0x00},
	{0x3811,0x04},
	{0x3812,0x00},
	{0x3813,0x04},
	{0x3814,0x11},
	{0x3815,0x11},
	{0x3820,0x00},
	{0x3821,0x00},
	{0x3836,0x04},
	{0x3837,0x01},
	{0x4020,0x02},
	{0x4021,0x4C},
	{0x4022,0x0E},
	{0x4023,0x37},
	{0x4024,0x0F},
	{0x4025,0x1C},
	{0x4026,0x0F},
	{0x4027,0x1F},
	{0x4501,0x38},
	{0x4601,0x04},
	{0x4603,0x00},
	{0x4837,0x0d},//1.196G
	{0x5401,0x71},
	{0x5405,0x80},


//{OV13850_REG_END, 0x00},	/* END MARKER */
};


#endif

static int ov13850_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov13850_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov13850_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV13850_REG_END) {
		if (vals->reg_num == OV13850_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else if (vals->reg_num == 0x3820) {
			if (ov13850_isp_parm.mirror)
				vals->value |= 0x02;
			else
				vals->value &= 0xfd;
		} else if (vals->reg_num == 0x3821) {
			if (ov13850_isp_parm.flip)
				vals->value |= 0x02;
			else
				vals->value &= 0xfd;
		}

		ret = ov13850_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;

		vals++;
	}
	return 0;
}

#ifdef OV13850_DEBUG_FUNC
static int ov13850_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	//return 0;
	while (vals->reg_num != OV13850_REG_END) {
		if (vals->reg_num == OV13850_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {

			ret = ov13850_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}

#endif

void ov13850_clear_otp_buffer(struct v4l2_subdev *sd)
{
	int i;
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov13850_write(sd,0x3d00 + i, 0x00);
	}
}


static int ov13850_otp_read(struct v4l2_subdev *sd, unsigned short reg)
{
	unsigned char flag;
	ov13850_read(sd,reg,&flag);
	return flag;
}
/* R/G and B/G of typical camera module is defined here. */
static unsigned int RG_Ratio_Typical = 85;
static unsigned int BG_Ratio_Typical = 85;

// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 3, group index has valid data

static int ov13850_check_otp_wb(struct v4l2_subdev *sd, int index)
{

	int i,ret;
	unsigned char flag;
	int bank, address;
	// select bank index
	bank = 0xc0 | index;
	ov13850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	// read flag
	address = 0x3d00;
	ret = ov13850_read(sd, address, &flag);
	if(ret<0)
		return ret;
	flag = flag & 0xc0;
	flag = flag >>6;
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	return flag;

}


// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 3, group index has valid data
static int ov13850_check_otp_lenc(struct v4l2_subdev *sd, int index)
{
	int  i, bank,ret;
	unsigned char flag;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 | (index*4);
	ov13850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	// read flag
	address = 0x3d00;
	ret= ov13850_read(sd,address,&flag);
	flag = flag & 0xc0;
	flag = flag>>6;
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	return flag;
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
static struct isp_effect_ops  sensor_effect_ops = {
	.get_iso = sensor_get_iso,
	.get_contrast = sensor_get_contrast,
	.get_saturation = sensor_get_saturation,
	.get_white_balance = sensor_get_white_balance,
	.get_brightness = sensor_get_brightness,
	.get_sharpness = sensor_get_sharpness,
	.get_vcm_range = sensor_get_vcm_range,
	.get_exposure = sensor_get_exposure,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,

};


#ifdef OV13850_CHECK_OTP_VCM_FUN

static int ov13850_check_otp_VCM(struct v4l2_subdev *sd, unsigned short index,int code)
{
	int i, bank,ret;
	unsigned char flag;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	ov13850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov13850_write(sd,0x3d81, 0x01);
	// read flag
	address = 0x3d00 + index*4 + code*2;
	ret= ov13850_read(sd,address,&flag);
	flag = flag & 0xc0;
	flag=flag>>6;
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	return flag;
}

#endif

// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return: 0,
static int ov13850_read_otp_wb(struct v4l2_subdev *sd,
				unsigned short index, struct otp_struct* otp_ptr)
{
	int i, bank;
	int address,temp;
	// select bank index
	bank = 0xc0 | index;
	ov13850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	(*otp_ptr).module_integrator_id = ov13850_otp_read(sd,address + 1);
	(*otp_ptr).lens_id = ov13850_otp_read(sd,address + 2);
	(*otp_ptr).production_year = ov13850_otp_read(sd,address + 3);
	(*otp_ptr).production_month = ov13850_otp_read(sd,address + 4);
	(*otp_ptr).production_day = ov13850_otp_read(sd,address + 5);
	temp = ov13850_otp_read(sd,address + 10);
	(*otp_ptr).rg_ratio = (ov13850_otp_read(sd,(address + 6)<<2) + ((temp>>6) & 0x03));
	(*otp_ptr).bg_ratio = (ov13850_otp_read(sd,address + 7)<<2) + ((temp>>4) & 0x03);
	(*otp_ptr).light_rg = (ov13850_otp_read(sd,address + 8) <<2) + ((temp>>2) & 0x03);
	(*otp_ptr).light_bg = (ov13850_otp_read(sd,address + 9)<<2) + (temp & 0x03);
	(*otp_ptr).user_data[0] = ov13850_otp_read(sd,address + 11);
	(*otp_ptr).user_data[1] = ov13850_otp_read(sd,address + 12);
	(*otp_ptr).user_data[2] = ov13850_otp_read(sd,address + 13);
	(*otp_ptr).user_data[3] = ov13850_otp_read(sd,address + 14);
	(*otp_ptr).user_data[4] = ov13850_otp_read(sd,address + 15);
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	return 0;
}


// index: index of otp group. (0, 1, 2)
// otp_ptr: pointer of otp_struct
// return: 0,

static int ov13850_read_otp_lenc(struct v4l2_subdev *sd,
				unsigned short index, struct otp_struct* otp_ptr)
{
	int bank, i;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 + (index*4);
	ov13850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d01;
	for(i=0;i<15;i++) {
	(* otp_ptr).lenc[i]=ov13850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	// select 2nd bank
	bank++;
	ov13850_write(sd,0x3d84, bank);
	// read otp
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	for(i=15;i<31;i++) {
	(* otp_ptr).lenc[i]=ov13850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	// select 3rd bank
	bank++;
	ov13850_write(sd,0x3d84, bank);
	// read otp
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	for(i=31;i<47;i++) {
	(* otp_ptr).lenc[i]=ov13850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	// select 4th bank
	bank++;
	ov13850_write(sd,0x3d84, bank);
	// read otp
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	for(i=47;i<62;i++) {
	(* otp_ptr).lenc[i]=ov13850_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	return 0;


}

// index: index of otp group. (0, 1, 2)
// code: 0 start code, 1 stop code
// return: 0



int ov13850_read_otp_VCM(struct v4l2_subdev *sd, int index, int code, struct otp_struct * otp_ptr)
{
	int vcm, i, bank;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	ov13850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov13850_write(sd,0x3d81, 0x01);
	// read flag
	address = 0x3d00 + index*4 + code*2;
	vcm = ov13850_otp_read(sd,address);
	address++;
	vcm = (vcm & 0x03) + (ov13850_otp_read(sd,address)<<2);
	if(code==1) {
	(* otp_ptr).VCM_end = vcm;
	}
	else {
	(* otp_ptr).VCM_start = vcm;
	}
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov13850_write(sd,0x3d00 + i, 0x00);
	}
	return 0;
}



/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov13850_update_awb_gain(struct v4l2_subdev *sd,
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	if (R_gain>0x400) {
	ov13850_write(sd,0x3400, R_gain>>8);
	ov13850_write(sd,0x3401, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
	ov13850_write(sd,0x3402, G_gain>>8);
	ov13850_write(sd,0x3403, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
	ov13850_write(sd,0x3404, B_gain>>8);
	ov13850_write(sd,0x3405, B_gain & 0x00ff);
	}
	return 0;
}

// otp_ptr: pointer of otp_struct
static int ov13850_update_lenc(struct v4l2_subdev *sd,struct otp_struct * otp_ptr)
{
	int i, temp;
	temp = ov13850_otp_read(sd,0x5000);
	temp = 0x80 | temp;
	ov13850_write(sd,0x5000, temp);
	for(i=0;i<62;i++) {
	ov13850_write(sd,0x5800 + i, (*otp_ptr).lenc[i]);
	}
	return 0;
}


// call this function after OV13850 initialization
// return value: 0 update success
// 1, no OTP
static int ov13850_update_otp_wb(struct v4l2_subdev *sd)
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
	temp = ov13850_check_otp_wb(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i>3) {
	// no valid wb OTP data
	return 1;
	}
	ov13850_read_otp_wb(sd,otp_index, &current_otp);
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
	ov13850_update_awb_gain(sd,R_gain, G_gain, B_gain);
	return 0;


}



// call this function after OV13850 initialization
// return value: 0 update success
// 1, no OTP
int ov13850_update_otp_lenc(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	// check first lens correction OTP with valid data
	for(i=1;i<=3;i++) {
	temp = ov13850_check_otp_lenc(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i>3) {
	// no valid wb OTP data
	return 1;
	}
	ov13850_read_otp_lenc(sd,otp_index, &current_otp);
	ov13850_update_lenc(sd,&current_otp);
	// success
	return 0;
}

// return: 0 \A8Cuse module DCBLC, 1 \A8Cuse sensor DCBL 2 \A8Cuse defualt DCBLC
int ov13850_check_dcblc(struct v4l2_subdev *sd)
{
	int bank, dcblc;
	int address;
	int temp, flag;
	// select bank 31
	bank = 0xc0 | 31;
	ov13850_write(sd,0x3d84, bank);
	// read otp into buffer
	ov13850_write(sd,0x3d81, 0x01);
	msleep(10);
	address = 0x3d0b;
	dcblc = ov13850_otp_read(sd,address);
	if ((dcblc>=0x15) && (dcblc<=0x40)){
	// module DCBLC value is valid
	flag = 0;
//	clear_otp_buffer();
	ov13850_clear_otp_buffer(sd);
	return flag;
	}
	temp = ov13850_otp_read(sd,0x4000);
	temp = temp | 0x08; // DCBLC manual load enable
	ov13850_write(sd,0x4000, temp);
	address--;
	dcblc = ov13850_otp_read(sd,address);
	if ((dcblc>=0x10) && (dcblc<=0x40)){
	// sensor DCBLC value is valid
	ov13850_write(sd,0x4006, dcblc); // manual load sensor level DCBLC
	flag = 1; // sensor level DCBLC is used
	}
	else{
	ov13850_write(sd,0x4006, 0x20);
	flag = 2; // default DCBLC is used
	}
	// disable otp read
	ov13850_write(sd,0x3d81, 0x00);
//	clear_otp_buffer();
	ov13850_clear_otp_buffer(sd);
	return flag;
}




static int ov13850_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov13850_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov13850_info *info = container_of(sd, struct ov13850_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->dynamic_framerate = 0;

	ret=ov13850_write_array(sd, ov13850_init_regs);
	if (ret < 0)
		return ret;

	ret = ov13850_update_otp_wb(sd);
	if (ret < 0)
		return ret;
	ret = ov13850_update_otp_lenc(sd);

	return ret;
}

static int ov13850_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov13850_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H = 0x%x ", v);
	if (v != OV13850_CHIP_ID_H)
		return -ENODEV;
	ret = ov13850_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L = 0x%x \n", v);
	if (v != OV13850_CHIP_ID_L)
		return -ENODEV;
	ret = ov13850_read(sd, 0x300c, &v);
	printk("SCCB ID = 0x%x \n", v);

	return 0;
}

static struct ov13850_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov13850_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV13850_FMTS ARRAY_SIZE(ov13850_formats)

static struct ov13850_win_size {
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
} ov13850_win_sizes[] = {
#if 0
	/* UXGA */

	{
		.width		= QXGA_WIDTH,
		.height		= QXGA_HEIGHT,
		.vts		= 0xd00,//0x680,
		.framerate    = 30,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov13850_win_qxga,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_2M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_2M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_2M_central_weight,
	},
#endif
#if 0
/* QSXGA */

	{
		.width		= QSXGA_WIDTH,
		.height		= QSXGA_HEIGHT,
		.vts		= 0xd00,//0x680,
		.framerate    = 30,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov13850_win_qsxga,
		.regs_aecgc_win_matrix			= isp_aecgc_win_8M_matrix,
		.regs_aecgc_win_center			= isp_aecgc_win_8M_center,
		.regs_aecgc_win_central_weight	= isp_aecgc_win_8M_central_weight,
	},
#endif
#if 1
/* 4208*3120 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0xd00,
		.framerate      = 10,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov13850_win_13m,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_13M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_13M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_13M_central_weight,
	},
#endif

};
#define N_WIN_SIZES (ARRAY_SIZE(ov13850_win_sizes))

static int ov13850_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{

	if (index >= N_OV13850_FMTS)
		return -EINVAL;

	*code = ov13850_formats[index].mbus_code;
	return 0;
}

static int ov13850_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov13850_format_struct **ret_fmt,
		struct ov13850_win_size **ret_wsize)
{
	int index;
	struct ov13850_win_size *wsize;

	for (index = 0; index < N_OV13850_FMTS; index++)
		if (ov13850_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV13850_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov13850_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov13850_formats + index;

	fmt->field = V4L2_FIELD_NONE;
	for (wsize = ov13850_win_sizes; wsize < ov13850_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov13850_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov13850_formats[index].colorspace;
	return 0;
}

static int ov13850_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov13850_try_fmt_internal(sd, fmt, NULL, NULL);
}


static int ov13850_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov13850_info *info = container_of(sd, struct ov13850_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov13850_format_struct *fmt_s;
	struct ov13850_win_size *wsize;
	int ret,len,i = 0;

	ret = ov13850_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov13850_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
	//	ret = ov13850_write_array(sd, wsize->regs);
	//	if (ret)
   	//	return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->framerate = wsize->framerate;
//		data->mipi_clk = 721; /*Mbps*/
		data->sensor_vts_address1 = info->vts_address1;
		data->sensor_vts_address2 = info->vts_address2;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;

		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->mipi_clk = 962; ///*Mbps*/1224
			//data->mipi_clk = 1224; ///*Mbps*/1224
			len = ARRAY_SIZE(ov13850_win_13m);
			data->reg_num = len;
			for(i =0;i<len;i++) {
				data->reg[i].addr = ov13850_win_13m[i].reg_num;
				data->reg[i].data = ov13850_win_13m[i].value;
			}

			if (!info->dynamic_framerate) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num].data = data->vts & 0x00ff;
			}

		} 	else if((wsize->width == QSXGA_WIDTH)
			&& (wsize->height == QSXGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->mipi_clk = 962; /*Mbps*/
			len = ARRAY_SIZE(ov13850_win_qsxga);
			data->reg_num = len;  //
			for(i =0;i<len;i++) {
				data->reg[i].addr = ov13850_win_qsxga[i].reg_num;
				data->reg[i].data = ov13850_win_qsxga[i].value;
			}

			if (!info->dynamic_framerate) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num].data = data->vts & 0x00ff;

			}

		}
		else if ((wsize->width == QXGA_WIDTH)
			&& (wsize->height == QXGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->mipi_clk = 642; /*Mbps*/
			len = ARRAY_SIZE(ov13850_win_qxga);
			data->reg_num = len;  //
			for(i =0;i<len;i++) {
				data->reg[i].addr = ov13850_win_qxga[i].reg_num;
				data->reg[i].data = ov13850_win_qxga[i].value;
			}

			if (!info->dynamic_framerate) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num].data = data->vts & 0x00ff;

			}
		}
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov13850_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov13850_write_array(sd, ov13850_stream_on);
	else
		ret = ov13850_write_array(sd, ov13850_stream_off);
	#if 0
	else
		ov13850_read_array(sd,ov13850_init_regs );//debug only
	#endif
	return ret;
}

static int ov13850_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov13850_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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


static int ov13850_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{

	return 0;
}

static int ov13850_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov13850_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov13850_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov13850_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov13850_frame_rates[interval->index];
	return 0;
}

static int ov13850_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov13850_win_size *win = &ov13850_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov13850_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}


static int ov13850_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov13850_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov13850_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov13850_isp_parm;
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

static int ov13850_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals)
{
	int i = 0;
	int ret = 0;

	for(i = 0; i < N_WIN_SIZES; i++) {
		if (width == ov13850_win_sizes[i].width && height == ov13850_win_sizes[i].height) {
			switch (meter_mode){
				case METER_MODE_MATRIX:
					*vals = ov13850_win_sizes[i].regs_aecgc_win_matrix;
					break;
				case METER_MODE_CENTER:
					*vals = ov13850_win_sizes[i].regs_aecgc_win_center;
					break;
				case METER_MODE_CENTRAL_WEIGHT:
					*vals = ov13850_win_sizes[i].regs_aecgc_win_central_weight;
					break;
				default:
					break;
				}
			break;
		}
	}
	return ret;
}

static int ov13850_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV13850, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov13850_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov13850_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov13850_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov13850_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov13850_core_ops = {
	.g_chip_ident = ov13850_g_chip_ident,
	.g_ctrl = ov13850_g_ctrl,
	.s_ctrl = ov13850_s_ctrl,
	.queryctrl = ov13850_queryctrl,
	.reset = ov13850_reset,
	.init = ov13850_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov13850_g_register,
	.s_register = ov13850_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov13850_video_ops = {
	.enum_mbus_fmt = ov13850_enum_mbus_fmt,
	.try_mbus_fmt = ov13850_try_mbus_fmt,
	.s_mbus_fmt = ov13850_s_mbus_fmt,
	.s_stream = ov13850_s_stream,
	.cropcap = ov13850_cropcap,
	.g_crop	= ov13850_g_crop,
	.s_parm = ov13850_s_parm,
	.g_parm = ov13850_g_parm,
	.enum_frameintervals = ov13850_enum_frameintervals,
	.enum_framesizes = ov13850_enum_framesizes,
};

static const struct v4l2_subdev_ops ov13850_ops = {
	.core = &ov13850_core_ops,
	.video = &ov13850_video_ops,
};

static int ov13850_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov13850_info *info;
	struct comip_camera_subdev_platform_data *ov13850_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct ov13850_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov13850_ops);

	/* Make sure it's an ov13850 */
//	while(1);
	ret = ov13850_detect(sd);

	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov13850 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov13850 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ov13850_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;

	if (ov13850_setting) {
		if (ov13850_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			ov13850_isp_parm.mirror = 1;
		else
			ov13850_isp_parm.mirror = 0;

		if (ov13850_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			ov13850_isp_parm.flip = 1;
		else
			ov13850_isp_parm.flip = 0;
	}

	return 0;
}

static int ov13850_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov13850_info *info = container_of(sd, struct ov13850_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov13850_id[] = {
	{ "ov13850-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov13850_id);

static struct i2c_driver ov13850_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov13850-mipi-raw",
	},
	.probe		= ov13850_probe,
	.remove		= ov13850_remove,
	.id_table	= ov13850_id,
};

static __init int init_ov13850(void)
{
	return i2c_add_driver(&ov13850_driver);
}

static __exit void exit_ov13850(void)
{
	i2c_del_driver(&ov13850_driver);
}

//module_init(init_ov13850);
fs_initcall(init_ov13850);
module_exit(exit_ov13850);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov13850 sensors");
MODULE_LICENSE("GPL");
