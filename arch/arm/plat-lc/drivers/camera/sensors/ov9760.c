
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
#include "ov9760.h"


#define OV9760_CHIP_ID_H	(0x97)
#define OV9760_CHIP_ID_L	(0x60)

#define MAX_WIDTH		1472
#define MAX_HEIGHT		1104
#define MAX_PREVIEW_WIDTH	MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT

#define SETTING_LOAD
#define OV9760_REG_END		0xffff
#define OV9760_REG_DELAY	0xfffe
#define OV9760_REG_VAL_HVFLIP	0xfc

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals);

struct ov9760_format_struct;

struct ov9760_info {
	struct v4l2_subdev sd;
	struct ov9760_format_struct *fmt;
	struct ov9760_win_size *win;
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

static struct regval_list ov9760_init_regs[] = {
	{0x0103,0x01},
	{OV9760_REG_DELAY, 200},
	{0x0340,0x04},
	{0x0341,0x7C},
	{0x0342,0x06},
	{0x0343,0xc8},
	{0x0344,0x00},
	{0x0345,0x08},
	{0x0346,0x00},
	{0x0347,0x02},
	{0x0348,0x05},
	{0x0349,0xdf},
	{0x034a,0x04},
	{0x034b,0x65},
	{0x3811,0x04},
	{0x3813,0x04},
	{0x034c,0x05},
	{0x034d,0xC0},
	{0x034e,0x04},
	{0x034f,0x50},
	{0x0383,0x01},
	{0x0387,0x01},
	{0x3820,0x00},
	{0x3821,0x00},
	{0x3660,0x80},
	{0x3680,0xf4},
	{0x0100,0x00},
	{0x0101,0x01},
	{0x3002,0x80},
	{0x3012,0x08},
	{0x3014,0x04},
	{0x3022,0x02},
	{0x3023,0x0f},
	{0x0101,OV9760_REG_VAL_HVFLIP},
	{0x3080,0x00},
	{0x3090,0x02},
	{0x3091,0x28},
	{0x3092,0x02},
	{0x3093,0x02},
	{0x3094,0x00},
	{0x3095,0x00},
	{0x3096,0x01},
	{0x3097,0x00},
	{0x3098,0x04},
	{0x3099,0x14},
	{0x309a,0x03},
	{0x309c,0x00},
	{0x309d,0x00},
	{0x309e,0x01},
	{0x309f,0x00},
	{0x30a2,0x01},
	{0x30b0,0x0a},
	{0x30b3,0x32},
	{0x30b4,0x02},
	{0x30b5,0x00},
	{0x3503,0x27},
	{0x3509,0x10},
	{0x3600,0x7c},
	{0x3621,0xb8},
	{0x3622,0x23},
	{0x3631,0xe2},
	{0x3634,0x03},
	{0x3662,0x14},
	{0x366b,0x03},
	{0x3682,0x82},
	{0x3705,0x20},
	{0x3708,0x64},
	{0x371b,0x60},
	{0x3732,0x40},
	{0x3745,0x00},
	{0x3746,0x18},
	{0x3780,0x2a},
	{0x3781,0x8c},
	{0x378f,0xf5},
	{0x3823,0x37},
	{0x383d,0x88},
	{0x4000,0x23},
	{0x4001,0x04},
	{0x4002,0x45},
	{0x4004,0x08},
	{0x4005,0x40},
	{0x4006,0x40},
	{0x4009,0x10},//0x40
	{0x404F,0x8F},
	{0x4058,0x44},
	{0x4101,0x32},
	{0x4102,0xa4},
	{0x4520,0xb0},
	{0x4580,0x08},
	{0x4582,0x00},
	{0x4307,0x30},
	{0x4605,0x00},
	{0x4608,0x02},
	{0x4609,0x00},
	{0x4801,0x0f},
	{0x4819,0xB6},
	{0x4837,0x21},
	{0x4906,0xff},
	{0x4d00,0x04},
	{0x4d01,0x4b},
	{0x4d02,0xfe},
	{0x4d03,0x09},
	{0x4d04,0x1e},
	{0x4d05,0xb7},
	{0x3501,0x45},
	{0x3502,0x80},
	{0x350b,0xf0},

	{OV9760_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list ov9760_stream_on[] = {
	{0x0100, 0x01},

	{OV9760_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov9760_stream_off[] = {
	/* Sensor enter LP11*/	
	{0x0100, 0x00},

	{OV9760_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov9760_win_720p[] = {
	
	{OV9760_REG_END, 0x00},	/* END MARKER */
};

static int sensor_get_exposure(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L3:
		*vals = isp_exposure_l3;
		return ARRAY_SIZE(isp_exposure_l3);
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
	case EXPOSURE_H3:
		*vals = isp_exposure_h3;
		return ARRAY_SIZE(isp_exposure_h3);
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
	case CONTRAST_L4:
		*vals = isp_contrast_l4;
		return ARRAY_SIZE(isp_contrast_l4);
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
	case CONTRAST_H4:
		*vals = isp_contrast_h4;
		return ARRAY_SIZE(isp_contrast_h4);
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
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,
};


static int ov9760_read(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char *value)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2];
	
	if (client->addr == 0x20) {
		msg[0].addr	= 0x10;
		msg[0].flags = 0;
		msg[0].len = 2;
		msg[0].buf = buf;

		msg[1].addr	= 0x10;
		msg[1].flags = I2C_M_RD;
		msg[1].len	= 1;
		msg[1].buf	= value;
	} else {
		msg[0].addr = client->addr;
		msg[0].flags = 0;
		msg[0].len = 2;
		msg[0].buf = buf;

		msg[1].addr = client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = 1;
		msg[1].buf = value;
	}
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov9760_write(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char value)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	struct i2c_msg msg;

	if (client->addr == 0x20) {
		msg.addr = 0x10;//client->addr,
		msg.flags = 0;
		msg.len	= 3;
		msg.buf	= buf;
	} else {
		msg.addr = client->addr;
		msg.flags = 0;
		msg.len	= 3;
		msg.buf	= buf;
	}
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}


static int ov9760_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned int val = 0;

	while (vals->reg_num != OV9760_REG_END) {
		if (vals->reg_num == OV9760_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			if (vals->reg_num == 0x0101 && vals->value == OV9760_REG_VAL_HVFLIP) {
				if (ov9760_isp_parm.mirror)
					val &= 0xfe;
				else
					val |= 0x01;

				if (ov9760_isp_parm.flip)
					val |= 0x02;
				else
					val &= 0xfd;

				vals->value = val;
			}
			ret = ov9760_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

#if 0 
static int ov9760_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != OV9760_REG_END) {
		if (vals->reg_num == OV9760_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			
			ret = ov9760_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

void ov9760_clear_otp_buffer(struct v4l2_subdev *sd)
{
int i;
// clear otp buffer
for (i=0;i<16;i++) {
ov9760_write(sd,0x3d00 + i, 0x00);
}
}


static int ov9760_otp_read(struct v4l2_subdev *sd, unsigned short reg)
{
	unsigned char value = 0;
	int flag;
	ov9760_read(sd,reg,&value);
	flag = (int)value;
	return flag;
}



/* R/G and B/G of typical camera module is defined here. */
static unsigned int RG_Ratio_Typical = 0x33;
static unsigned int BG_Ratio_Typical = 0x7c;
// index: index of otp group. (0, 1, 2)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data


static int ov9760_check_otp_wb(struct v4l2_subdev *sd, int index)
{

	int flag;
	int bank, address;
	// select bank index
	bank = 0xc0 | index;
	ov9760_write(sd,0x3d84, bank);
	// read otp into buffer
	ov9760_write(sd,0x3d81, 0x01);
	msleep(5);
	// read flag
	address = 0x3d00;
	flag = ov9760_otp_read(sd,address);
	flag = flag & 0xc0;
	// disable otp read
	ov9760_write(sd,0x3d81, 0x00);
	ov9760_clear_otp_buffer(sd);
	if (!flag) {
	return 0;
	}
	else if ((flag & 0x80) && (flag & 0x7f)) {
	return 1;
	}
	else {
	return 2;
	}

}




// index: index of otp group. (0, 1)
// return: 0,
static int ov9760_read_otp_wb(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp_ptr)
{
	int bank;
	int address;
	// select bank index
	bank = 0xc0 | index;
	ov9760_write(sd,0x3d84, bank);
	// read otp into buffer
	ov9760_write(sd,0x3d81, 0x01);
	msleep(1);
	address = 0x3d00;
	(*otp_ptr).module_integrator_id = ov9760_otp_read(sd,address + 1);
	(*otp_ptr).lens_id = ov9760_otp_read(sd,address + 2);
	(*otp_ptr).production_year = ov9760_otp_read(sd,address + 3);
	(*otp_ptr).production_month = ov9760_otp_read(sd,address + 4);
	(*otp_ptr).production_day = ov9760_otp_read(sd,address + 5);
	(*otp_ptr).rg_ratio = ov9760_otp_read(sd,address + 6);
	(*otp_ptr).bg_ratio = ov9760_otp_read(sd,address + 7);
	(*otp_ptr).light_rg = ov9760_otp_read(sd,address + 8);
	(*otp_ptr).light_bg = ov9760_otp_read(sd,address + 9);
	(*otp_ptr).user_data[0] = ov9760_otp_read(sd,address + 10);
	(*otp_ptr).user_data[1] = ov9760_otp_read(sd,address + 11);
	(*otp_ptr).user_data[2] = ov9760_otp_read(sd,address + 12);
	(*otp_ptr).user_data[3] = ov9760_otp_read(sd,address + 13);
	(*otp_ptr).user_data[4] = ov9760_otp_read(sd,address + 14);
	(*otp_ptr).user_data[5] = ov9760_otp_read(sd,address + 15);
	// disable otp read
	ov9760_write(sd,0x3d81, 0x00);
	ov9760_clear_otp_buffer(sd);
	return 0;
}


// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
static int ov9760_update_awb_gain(struct v4l2_subdev *sd, 
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	if (R_gain>0x400) {
	ov9760_write(sd,0x5180, R_gain>>8);
	ov9760_write(sd,0x5181, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
	ov9760_write(sd,0x5182, G_gain>>8);
	ov9760_write(sd,0x5183, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
	ov9760_write(sd,0x5184, B_gain>>8);
	ov9760_write(sd,0x5185, B_gain & 0x00ff);
	}
	return 0;
}



// call this function after OV9760 initialization
// return value: 0 update success
// 1, no OTP
static int ov9760_update_otp_wb(struct v4l2_subdev *sd)
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
	temp = ov9760_check_otp_wb(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i>3) {
	// no valid wb OTP data

	return 1;
	}
	ov9760_read_otp_wb(sd,otp_index, &current_otp);
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
	ov9760_update_awb_gain(sd,R_gain, G_gain, B_gain);
	return 0;


}



static int ov9760_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov9760_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov9760_info *info = container_of(sd, struct ov9760_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x0340;
	info->vts_address2 = 0x0341;
	info->dynamic_framerate = 0;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret=ov9760_write_array(sd, ov9760_init_regs);

	if (ret < 0)
		return ret;

	printk("*update_otp_wb*\n");
	ret = ov9760_update_otp_wb(sd);
	if (ret < 0)
		return ret;
//	printk("*update_otp_lenc*\n");
//	ret = ov9760_update_otp_lenc(sd);//lenc otp is not supported
	return ret;
}

static int ov9760_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov9760_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != OV9760_CHIP_ID_H)
		return -ENODEV;
	ret = ov9760_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L=0x%x \n", v);
	if (v != OV9760_CHIP_ID_L)
		return -ENODEV;

	
	return 0;
}

static struct ov9760_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov9760_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV9760_FMTS ARRAY_SIZE(ov9760_formats)

static struct ov9760_win_size {
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
} ov9760_win_sizes[] = {
	/* 1472*1104 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x47c, // 
		.framerate	= 24,
		.max_gain_dyn_frm = 0x40,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x40,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov9760_win_720p,
		.regs_aecgc_win_matrix	 	    = isp_aecgc_win_2M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_2M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_2M_central_weight,
		
	},
	
};
#define N_WIN_SIZES (ARRAY_SIZE(ov9760_win_sizes))

static int ov9760_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV9760_FMTS)
		return -EINVAL;

	
	*code = ov9760_formats[index].mbus_code;
	return 0;
}

static int ov9760_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov9760_format_struct **ret_fmt,
		struct ov9760_win_size **ret_wsize)
{
	int index;
	struct ov9760_win_size *wsize;

	for (index = 0; index < N_OV9760_FMTS; index++)
		if (ov9760_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV9760_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov9760_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov9760_formats + index;

	fmt->field = V4L2_FIELD_NONE;


	for (wsize = ov9760_win_sizes; wsize < ov9760_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov9760_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov9760_formats[index].colorspace;
	return 0;
}

static int ov9760_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov9760_try_fmt_internal(sd, fmt, NULL, NULL);
}


static int ov9760_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov9760_info *info = container_of(sd, struct ov9760_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov9760_format_struct *fmt_s;
	struct ov9760_win_size *wsize;
	int ret,len=0;

	ret = ov9760_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov9760_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
	//	ret = ov9760_write_array(sd, wsize->regs);
	//	if (ret)
   	//	return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 600;  /*Mbps*/
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
	
			data->reg_num = 0; 
			
			if (!info->dynamic_framerate) {
				data->reg[len].addr = info->vts_address1;
				data->reg[len].data = data->vts >> 8;
				data->reg[len+1].addr = info->vts_address2;
				data->reg[len+1].data = data->vts & 0x00ff;
				data->reg_num += 2;
			}
		} 
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov9760_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov9760_write_array(sd, ov9760_stream_on);
	else
		ret = ov9760_write_array(sd, ov9760_stream_off);
	#if 0
	else
		ov9760_read_array(sd,ov9760_init_regs );//debug only
	#endif
	return ret;
}

static int ov9760_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov9760_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov9760_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov9760_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov9760_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov9760_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov9760_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov9760_frame_rates[interval->index];
	return 0;
}

static int ov9760_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov9760_win_size *win = &ov9760_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov9760_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov9760_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov9760_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov9760_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov9760_isp_parm;
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

static int ov9760_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct ov9760_info *info = container_of(sd, struct ov9760_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		if (ctrl->value == FRAME_RATE_AUTO)
			info->dynamic_framerate = 1;
		else
			ret = -EINVAL;
		break;
	case V4L2_CID_SET_SENSOR_MIRROR:
		ov9760_isp_parm.mirror = !ov9760_isp_parm.mirror;
		break;
	case V4L2_CID_SET_SENSOR_FLIP:
		ov9760_isp_parm.flip = !ov9760_isp_parm.flip;
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
		if (width == ov9760_win_sizes[i].width && height == ov9760_win_sizes[i].height) {
			switch (meter_mode){
				case METER_MODE_MATRIX:
					*vals = ov9760_win_sizes[i].regs_aecgc_win_matrix;
					break;
				case METER_MODE_CENTER:
					*vals = ov9760_win_sizes[i].regs_aecgc_win_center;
					break;
				case METER_MODE_CENTRAL_WEIGHT:
					*vals = ov9760_win_sizes[i].regs_aecgc_win_central_weight;
					break;
				default:
					break;
				}
			break;
		}
	}
	return ret;
}

static int ov9760_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV9760, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov9760_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov9760_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov9760_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov9760_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov9760_core_ops = {
	.g_chip_ident = ov9760_g_chip_ident,
	.g_ctrl = ov9760_g_ctrl,
	.s_ctrl = ov9760_s_ctrl,
	.queryctrl = ov9760_queryctrl,
	.reset = ov9760_reset,
	.init = ov9760_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov9760_g_register,
	.s_register = ov9760_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov9760_video_ops = {
	.enum_mbus_fmt = ov9760_enum_mbus_fmt,
	.try_mbus_fmt = ov9760_try_mbus_fmt,
	.s_mbus_fmt = ov9760_s_mbus_fmt,
	.s_stream = ov9760_s_stream,
	.cropcap = ov9760_cropcap,
	.g_crop	= ov9760_g_crop,
	.s_parm = ov9760_s_parm,
	.g_parm = ov9760_g_parm,
	.enum_frameintervals = ov9760_enum_frameintervals,
	.enum_framesizes = ov9760_enum_framesizes,
};

static const struct v4l2_subdev_ops ov9760_ops = {
	.core = &ov9760_core_ops,
	.video = &ov9760_video_ops,
};

static int ov9760_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov9760_info *info;
	struct comip_camera_subdev_platform_data *ov9760_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct ov9760_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov9760_ops);

	/* Make sure it's an ov9760 */
	ret = ov9760_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov9760 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov9760 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ov9760_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;

	if (ov9760_setting) {
		if (ov9760_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			ov9760_isp_parm.mirror = 1;
		else
			ov9760_isp_parm.mirror = 0;

		if (ov9760_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			ov9760_isp_parm.flip = 1;
		else
			ov9760_isp_parm.flip = 0;
	}

	return 0;
}

static int ov9760_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9760_info *info = container_of(sd, struct ov9760_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov9760_id[] = {
	{ "ov9760-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov9760_id);

static struct i2c_driver ov9760_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov9760-mipi-raw",
	},
	.probe		= ov9760_probe,
	.remove		= ov9760_remove,
	.id_table	= ov9760_id,
};

static __init int init_ov9760(void)
{
	return i2c_add_driver(&ov9760_driver);
}

static __exit void exit_ov9760(void)
{
	i2c_del_driver(&ov9760_driver);
}

fs_initcall(init_ov9760);
module_exit(exit_ov9760);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov9760 sensors");
MODULE_LICENSE("GPL");
