
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
#include "ov2680_oflim.h"
#include "ov2680_sunny.h"


#define OV2680_CHIP_ID_H	(0x26)
#define OV2680_CHIP_ID_L	(0x80)

#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH	MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT

#define SETTING_LOAD
#define OV2680_REG_END		0xffff
#define OV2680_REG_DELAY	0xfffe
#define OV2680_REG_VAL_HVFLIP	0xc0
#define OV2680_REG_VAL_MIRROR	0x00

#define I2C_ADDR_OFLIM		0x36
#define I2C_ADDR_SUNNY		0x20

static char* factory_ofilm = "oflim";
static char* factory_sunny = "sunny";
static char* factory_unknown = "unknown";

struct ov2680_win_size *ov2680_win_sizes;
struct v4l2_isp_parm *ov2680_isp_parm;
static struct isp_effect_ops *sensor_effect_ops;
const struct v4l2_isp_regval *ov2680_isp_setting;
//const unsigned char *ov2680_isp_firmware;

static int n_win_sizes;

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals);


struct ov2680_format_struct;

struct ov2680_info {
	struct v4l2_subdev sd;
	struct ov2680_format_struct *fmt;
	struct ov2680_win_size *win;
	int i2c_addr;
	//int isp_firmware_size;
	int isp_setting_size;
	int n_win_sizes;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct ov2680_exp_ratio_entry {
	unsigned int width_last;
	unsigned int height_last;
	unsigned int width_cur;
	unsigned int height_cur;
	unsigned int exp_ratio;
};

struct ov2680_exp_ratio_entry ov2680_exp_ratio_table[] = {

};
#define N_EXP_RATIO_TABLE_SIZE  (ARRAY_SIZE(ov2680_exp_ratio_table))

static struct regval_list ov2680_init_regs[] = {
	{0x0103, 0x01},
	{OV2680_REG_DELAY, 20},

	{0x3002, 0x00},
	{0x3016, 0x1c},
	{0x3018, 0x44},
	{0x3020, 0x00},
	{0x3080, 0x02},
	{0x3082, 0x33},// ; for 26M input clk 37
	{0x3084, 0x09},
	{0x3085, 0x04},
	{0x3086, 0x00},
	{0x3501, 0x4e},
	{0x3502, 0xe0},
	{0x3503, 0x23},
	{0x350b, 0x36},
	{0x3600, 0xb4},
	{0x3603, 0x35},
	{0x3604, 0x24},
	{0x3605, 0x00},
	{0x3620, 0x24},
	{0x3621, 0x34},
	{0x3622, 0x03},
	{0x3628, 0x00},
	{0x3701, 0x64},
	{0x3705, 0x3c},
	{0x370c, 0x50},
	{0x370d, 0xc0},
	{0x3718, 0x80},
	{0x3720, 0x00},
	{0x3721, 0x09},
	{0x3722, 0x06},
	{0x3723, 0x59},
	{0x3738, 0x99},
	{0x370a, 0x21},
	{0x3717, 0x58},
	{0x3781, 0x80},
	{0x3784, 0x0c},
	{0x3789, 0x60},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x06},
	{0x3805, 0x4f},
	{0x3806, 0x04},
	{0x3807, 0xbf},
	{0x3808, 0x06},//
	{0x3809, 0x40},
	{0x380a, 0x04},
	{0x380b, 0xb0},
	{0x380c, 0x06},//hts
	{0x380d, 0xa4},
	{0x380e, 0x05},//vts
	{0x380f, 0x0e},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3819, 0x04},
	{0x3820, OV2680_REG_VAL_HVFLIP},
	{0x3821, OV2680_REG_VAL_MIRROR},
	{0x4000, 0x81},
	{0x4001, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x09},
	{0x4602, 0x02},
	{0x481f, 0x36},
	{0x4825, 0x36},
	{0x4837, 0x18},
	{0x5002, 0x30},
	{0x5080, 0x00},
	{0x5081, 0x41},
//	{0x0100, 0x01},

	{OV2680_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list ov2680_stream_on[] = {
	{0x0100, 0x01},

	{OV2680_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov2680_stream_off[] = {
	/* Sensor enter LP11*/
	{0x0100, 0x00},

	{OV2680_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov2680_win_720p[] = {

	{OV2680_REG_END, 0x00},	/* END MARKER */
};

static int sensor_get_exposure_oflim(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L6:
		*vals = isp_exposure_l6_oflim;
		return ARRAY_SIZE(isp_exposure_l6_oflim);
	case EXPOSURE_L5:
		*vals = isp_exposure_l5_oflim;
		return ARRAY_SIZE(isp_exposure_l5_oflim);
	case EXPOSURE_L4:
		*vals = isp_exposure_l4_oflim;
		return ARRAY_SIZE(isp_exposure_l4_oflim);
	case EXPOSURE_L3:
		*vals = isp_exposure_l3_oflim;
		return ARRAY_SIZE(isp_exposure_l3_oflim);
	case EXPOSURE_L2:
		*vals = isp_exposure_l2_oflim;
		return ARRAY_SIZE(isp_exposure_l2_oflim);
	case EXPOSURE_L1:
		*vals = isp_exposure_l1_oflim;
		return ARRAY_SIZE(isp_exposure_l1_oflim);
	case EXPOSURE_H0:
		*vals = isp_exposure_h0_oflim;
		return ARRAY_SIZE(isp_exposure_h0_oflim);
	case EXPOSURE_H1:
		*vals = isp_exposure_h1_oflim;
		return ARRAY_SIZE(isp_exposure_h1_oflim);
	case EXPOSURE_H2:
		*vals = isp_exposure_h2_oflim;
		return ARRAY_SIZE(isp_exposure_h2_oflim);
	case EXPOSURE_H3:
		*vals = isp_exposure_h3_oflim;
		return ARRAY_SIZE(isp_exposure_h3_oflim);
	case EXPOSURE_H4:
		*vals = isp_exposure_h4_oflim;
		return ARRAY_SIZE(isp_exposure_h4_oflim);
	case EXPOSURE_H5:
		*vals = isp_exposure_h5_oflim;
		return ARRAY_SIZE(isp_exposure_h5_oflim);
	case EXPOSURE_H6:
		*vals = isp_exposure_h6_oflim;
		return ARRAY_SIZE(isp_exposure_h6_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_iso_oflim(void **vals, int val)
{
	switch(val) {
	case ISO_100:
		*vals = isp_iso_100_oflim;
		return ARRAY_SIZE(isp_iso_100_oflim);
	case ISO_200:
		*vals = isp_iso_200_oflim;
		return ARRAY_SIZE(isp_iso_200_oflim);
	case ISO_400:
		*vals = isp_iso_400_oflim;
		return ARRAY_SIZE(isp_iso_400_oflim);
	case ISO_800:
		*vals = isp_iso_800_oflim;
		return ARRAY_SIZE(isp_iso_800_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_contrast_oflim(void **vals, int val)
{
	switch (val) {
	case CONTRAST_L4:
		*vals = isp_contrast_l4_oflim;
		return ARRAY_SIZE(isp_contrast_l4_oflim);
	case CONTRAST_L3:
		*vals = isp_contrast_l3_oflim;
		return ARRAY_SIZE(isp_contrast_l3_oflim);
	case CONTRAST_L2:
		*vals = isp_contrast_l2_oflim;
		return ARRAY_SIZE(isp_contrast_l2_oflim);
	case CONTRAST_L1:
		*vals = isp_contrast_l1_oflim;
		return ARRAY_SIZE(isp_contrast_l1_oflim);
	case CONTRAST_H0:
		*vals = isp_contrast_h0_oflim;
		return ARRAY_SIZE(isp_contrast_h0_oflim);
	case CONTRAST_H1:
		*vals = isp_contrast_h1_oflim;
		return ARRAY_SIZE(isp_contrast_h1_oflim);
	case CONTRAST_H2:
		*vals = isp_contrast_h2_oflim;
		return ARRAY_SIZE(isp_contrast_h2_oflim);
	case CONTRAST_H3:
		*vals = isp_contrast_h3_oflim;
		return ARRAY_SIZE(isp_contrast_h3_oflim);
	case CONTRAST_H4:
		*vals = isp_contrast_h4_oflim;
		return ARRAY_SIZE(isp_contrast_h4_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_oflim(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_l2_oflim;
		return ARRAY_SIZE(isp_saturation_l2_oflim);
	case SATURATION_L1:
		*vals = isp_saturation_l1_oflim;
		return ARRAY_SIZE(isp_saturation_l1_oflim);
	case SATURATION_H0:
		*vals = isp_saturation_h0_oflim;
		return ARRAY_SIZE(isp_saturation_h0_oflim);
	case SATURATION_H1:
		*vals = isp_saturation_h1_oflim;
		return ARRAY_SIZE(isp_saturation_h1_oflim);
	case SATURATION_H2:
		*vals = isp_saturation_h2_oflim;
		return ARRAY_SIZE(isp_saturation_h2_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_white_balance_oflim(void **vals, int val)
{
	switch (val) {
	case WHITE_BALANCE_AUTO:
		*vals = isp_white_balance_auto_oflim;
		return ARRAY_SIZE(isp_white_balance_auto_oflim);
	case WHITE_BALANCE_INCANDESCENT:
		*vals = isp_white_balance_incandescent_oflim;
		return ARRAY_SIZE(isp_white_balance_incandescent_oflim);
	case WHITE_BALANCE_FLUORESCENT:
		*vals = isp_white_balance_fluorescent_oflim;
		return ARRAY_SIZE(isp_white_balance_fluorescent_oflim);
	case WHITE_BALANCE_WARM_FLUORESCENT:
		*vals = isp_white_balance_warm_fluorescent_oflim;
		return ARRAY_SIZE(isp_white_balance_warm_fluorescent_oflim);
	case WHITE_BALANCE_DAYLIGHT:
		*vals = isp_white_balance_daylight_oflim;
		return ARRAY_SIZE(isp_white_balance_daylight_oflim);
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		*vals = isp_white_balance_cloudy_daylight_oflim;
		return ARRAY_SIZE(isp_white_balance_cloudy_daylight_oflim);
	case WHITE_BALANCE_TWILIGHT:
		*vals = isp_white_balance_twilight_oflim;
		return ARRAY_SIZE(isp_white_balance_twilight_oflim);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade_oflim;
		return ARRAY_SIZE(isp_white_balance_shade_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_brightness_oflim(void **vals, int val)
{
	switch (val) {
	case BRIGHTNESS_L3:
		*vals = isp_brightness_l3_oflim;
		return ARRAY_SIZE(isp_brightness_l3_oflim);
	case BRIGHTNESS_L2:
		*vals = isp_brightness_l2_oflim;
		return ARRAY_SIZE(isp_brightness_l2_oflim);
	case BRIGHTNESS_L1:
		*vals = isp_brightness_l1_oflim;
		return ARRAY_SIZE(isp_brightness_l1_oflim);
	case BRIGHTNESS_H0:
		*vals = isp_brightness_h0_oflim;
		return ARRAY_SIZE(isp_brightness_h0_oflim);
	case BRIGHTNESS_H1:
		*vals = isp_brightness_h1_oflim;
		return ARRAY_SIZE(isp_brightness_h1_oflim);
	case BRIGHTNESS_H2:
		*vals = isp_brightness_h2_oflim;
		return ARRAY_SIZE(isp_brightness_h2_oflim);
	case BRIGHTNESS_H3:
		*vals = isp_brightness_h3_oflim;
		return ARRAY_SIZE(isp_brightness_h3_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_sharpness_oflim(void **vals, int val)
{
	switch (val) {
	case SHARPNESS_L2:
		*vals = isp_sharpness_l2_oflim;
		return ARRAY_SIZE(isp_sharpness_l2_oflim);
	case SHARPNESS_L1:
		*vals = isp_sharpness_l1_oflim;
		return ARRAY_SIZE(isp_sharpness_l1_oflim);
	case SHARPNESS_H0:
		*vals = isp_sharpness_h0_oflim;
		return ARRAY_SIZE(isp_sharpness_h0_oflim);
	case SHARPNESS_H1:
		*vals = isp_sharpness_h1_oflim;
		return ARRAY_SIZE(isp_sharpness_h1_oflim);
	case SHARPNESS_H2:
		*vals = isp_sharpness_h2_oflim;
		return ARRAY_SIZE(isp_sharpness_h2_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}


static struct isp_effect_ops sensor_effect_ops_oflim = {
	.get_exposure = sensor_get_exposure_oflim,
	.get_iso = sensor_get_iso_oflim,
	.get_contrast = sensor_get_contrast_oflim,
	.get_saturation = sensor_get_saturation_oflim,
	.get_white_balance = sensor_get_white_balance_oflim,
	.get_brightness = sensor_get_brightness_oflim,
	.get_sharpness = sensor_get_sharpness_oflim,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,
};

static int sensor_get_exposure_sunny(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L6:
		*vals = isp_exposure_l6_sunny;
		return ARRAY_SIZE(isp_exposure_l6_sunny);
	case EXPOSURE_L5:
		*vals = isp_exposure_l5_sunny;
		return ARRAY_SIZE(isp_exposure_l5_sunny);
	case EXPOSURE_L4:
		*vals = isp_exposure_l4_sunny;
		return ARRAY_SIZE(isp_exposure_l4_sunny);
	case EXPOSURE_L3:
		*vals = isp_exposure_l3_sunny;
		return ARRAY_SIZE(isp_exposure_l3_sunny);
	case EXPOSURE_L2:
		*vals = isp_exposure_l2_sunny;
		return ARRAY_SIZE(isp_exposure_l2_sunny);
	case EXPOSURE_L1:
		*vals = isp_exposure_l1_sunny;
		return ARRAY_SIZE(isp_exposure_l1_sunny);
	case EXPOSURE_H0:
		*vals = isp_exposure_h0_sunny;
		return ARRAY_SIZE(isp_exposure_h0_sunny);
	case EXPOSURE_H1:
		*vals = isp_exposure_h1_sunny;
		return ARRAY_SIZE(isp_exposure_h1_sunny);
	case EXPOSURE_H2:
		*vals = isp_exposure_h2_sunny;
		return ARRAY_SIZE(isp_exposure_h2_sunny);
	case EXPOSURE_H3:
		*vals = isp_exposure_h3_sunny;
		return ARRAY_SIZE(isp_exposure_h3_sunny);
	case EXPOSURE_H4:
		*vals = isp_exposure_h4_sunny;
		return ARRAY_SIZE(isp_exposure_h4_sunny);
	case EXPOSURE_H5:
		*vals = isp_exposure_h5_sunny;
		return ARRAY_SIZE(isp_exposure_h5_sunny);
	case EXPOSURE_H6:
		*vals = isp_exposure_h6_sunny;
		return ARRAY_SIZE(isp_exposure_h6_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_iso_sunny(void **vals, int val)
{
	switch(val) {
	case ISO_100:
		*vals = isp_iso_100_sunny;
		return ARRAY_SIZE(isp_iso_100_sunny);
	case ISO_200:
		*vals = isp_iso_200_sunny;
		return ARRAY_SIZE(isp_iso_200_sunny);
	case ISO_400:
		*vals = isp_iso_400_sunny;
		return ARRAY_SIZE(isp_iso_400_sunny);
	case ISO_800:
		*vals = isp_iso_800_sunny;
		return ARRAY_SIZE(isp_iso_800_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_contrast_sunny(void **vals, int val)
{
	switch (val) {
	case CONTRAST_L4:
		*vals = isp_contrast_l4_sunny;
		return ARRAY_SIZE(isp_contrast_l4_sunny);
	case CONTRAST_L3:
		*vals = isp_contrast_l3_sunny;
		return ARRAY_SIZE(isp_contrast_l3_sunny);
	case CONTRAST_L2:
		*vals = isp_contrast_l2_sunny;
		return ARRAY_SIZE(isp_contrast_l2_sunny);
	case CONTRAST_L1:
		*vals = isp_contrast_l1_sunny;
		return ARRAY_SIZE(isp_contrast_l1_sunny);
	case CONTRAST_H0:
		*vals = isp_contrast_h0_sunny;
		return ARRAY_SIZE(isp_contrast_h0_sunny);
	case CONTRAST_H1:
		*vals = isp_contrast_h1_sunny;
		return ARRAY_SIZE(isp_contrast_h1_sunny);
	case CONTRAST_H2:
		*vals = isp_contrast_h2_sunny;
		return ARRAY_SIZE(isp_contrast_h2_sunny);
	case CONTRAST_H3:
		*vals = isp_contrast_h3_sunny;
		return ARRAY_SIZE(isp_contrast_h3_sunny);
	case CONTRAST_H4:
		*vals = isp_contrast_h4_sunny;
		return ARRAY_SIZE(isp_contrast_h4_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_sunny(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_l2_sunny;
		return ARRAY_SIZE(isp_saturation_l2_sunny);
	case SATURATION_L1:
		*vals = isp_saturation_l1_sunny;
		return ARRAY_SIZE(isp_saturation_l1_sunny);
	case SATURATION_H0:
		*vals = isp_saturation_h0_sunny;
		return ARRAY_SIZE(isp_saturation_h0_sunny);
	case SATURATION_H1:
		*vals = isp_saturation_h1_sunny;
		return ARRAY_SIZE(isp_saturation_h1_sunny);
	case SATURATION_H2:
		*vals = isp_saturation_h2_sunny;
		return ARRAY_SIZE(isp_saturation_h2_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_white_balance_sunny(void **vals, int val)
{
	switch (val) {
	case WHITE_BALANCE_AUTO:
		*vals = isp_white_balance_auto_sunny;
		return ARRAY_SIZE(isp_white_balance_auto_sunny);
	case WHITE_BALANCE_INCANDESCENT:
		*vals = isp_white_balance_incandescent_sunny;
		return ARRAY_SIZE(isp_white_balance_incandescent_sunny);
	case WHITE_BALANCE_FLUORESCENT:
		*vals = isp_white_balance_fluorescent_sunny;
		return ARRAY_SIZE(isp_white_balance_fluorescent_sunny);
	case WHITE_BALANCE_WARM_FLUORESCENT:
		*vals = isp_white_balance_warm_fluorescent_sunny;
		return ARRAY_SIZE(isp_white_balance_warm_fluorescent_sunny);
	case WHITE_BALANCE_DAYLIGHT:
		*vals = isp_white_balance_daylight_sunny;
		return ARRAY_SIZE(isp_white_balance_daylight_sunny);
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		*vals = isp_white_balance_cloudy_daylight_sunny;
		return ARRAY_SIZE(isp_white_balance_cloudy_daylight_sunny);
	case WHITE_BALANCE_TWILIGHT:
		*vals = isp_white_balance_twilight_sunny;
		return ARRAY_SIZE(isp_white_balance_twilight_sunny);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade_sunny;
		return ARRAY_SIZE(isp_white_balance_shade_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_brightness_sunny(void **vals, int val)
{
	switch (val) {
	case BRIGHTNESS_L3:
		*vals = isp_brightness_l3_sunny;
		return ARRAY_SIZE(isp_brightness_l3_sunny);
	case BRIGHTNESS_L2:
		*vals = isp_brightness_l2_sunny;
		return ARRAY_SIZE(isp_brightness_l2_sunny);
	case BRIGHTNESS_L1:
		*vals = isp_brightness_l1_sunny;
		return ARRAY_SIZE(isp_brightness_l1_sunny);
	case BRIGHTNESS_H0:
		*vals = isp_brightness_h0_sunny;
		return ARRAY_SIZE(isp_brightness_h0_sunny);
	case BRIGHTNESS_H1:
		*vals = isp_brightness_h1_sunny;
		return ARRAY_SIZE(isp_brightness_h1_sunny);
	case BRIGHTNESS_H2:
		*vals = isp_brightness_h2_sunny;
		return ARRAY_SIZE(isp_brightness_h2_sunny);
	case BRIGHTNESS_H3:
		*vals = isp_brightness_h3_sunny;
		return ARRAY_SIZE(isp_brightness_h3_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_sharpness_sunny(void **vals, int val)
{
	switch (val) {
	case SHARPNESS_L2:
		*vals = isp_sharpness_l2_sunny;
		return ARRAY_SIZE(isp_sharpness_l2_sunny);
	case SHARPNESS_L1:
		*vals = isp_sharpness_l1_sunny;
		return ARRAY_SIZE(isp_sharpness_l1_sunny);
	case SHARPNESS_H0:
		*vals = isp_sharpness_h0_sunny;
		return ARRAY_SIZE(isp_sharpness_h0_sunny);
	case SHARPNESS_H1:
		*vals = isp_sharpness_h1_sunny;
		return ARRAY_SIZE(isp_sharpness_h1_sunny);
	case SHARPNESS_H2:
		*vals = isp_sharpness_h2_sunny;
		return ARRAY_SIZE(isp_sharpness_h2_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}


static struct isp_effect_ops sensor_effect_ops_sunny = {
	.get_exposure = sensor_get_exposure_sunny,
	.get_iso = sensor_get_iso_sunny,
	.get_contrast = sensor_get_contrast_sunny,
	.get_saturation = sensor_get_saturation_sunny,
	.get_white_balance = sensor_get_white_balance_sunny,
	.get_brightness = sensor_get_brightness_sunny,
	.get_sharpness = sensor_get_sharpness_sunny,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,
};


static int ov2680_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov2680_write(struct v4l2_subdev *sd, unsigned short reg,
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


static int ov2680_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned int val = 0;

	while (vals->reg_num != OV2680_REG_END) {
		if (vals->reg_num == OV2680_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			if (vals->reg_num == 0x3820 && vals->value == OV2680_REG_VAL_HVFLIP) {
				if (ov2680_isp_parm->flip)
					val |= 0x04;
				else
					val &= 0xfb;
				val = val | OV2680_REG_VAL_HVFLIP;
				vals->value = val;
			}
			if (vals->reg_num == 0x3821 && vals->value == OV2680_REG_VAL_MIRROR) {
				if (ov2680_isp_parm->mirror)
					val |= 0x04;
				else
					val &= 0xfb;

				val = val | OV2680_REG_VAL_MIRROR;
				vals->value = val;
			}
			val = 0;
			ret = ov2680_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

#if 0
static int ov2680_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != OV2680_REG_END) {
		if (vals->reg_num == OV2680_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {

			ret = ov2680_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk(KERN_DEBUG"reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif


static int ov2680_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov2680_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov2680_info *info = container_of(sd, struct ov2680_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;
//fill sensor vts register address here
//	info->vts_address1 = 0x0340;
//	info->vts_address2 = 0x0341;
//	info->dynamic_framerate = 0;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
//	info->binning = 1;

	ret=ov2680_write_array(sd, ov2680_init_regs);

	if (ret < 0)
		return ret;

	return ret;
}

static int ov2680_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov2680_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	printk(KERN_DEBUG"CHIP_ID_H=0x%x ", v);
	if (v != OV2680_CHIP_ID_H)
		return -ENODEV;
	ret = ov2680_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	printk(KERN_DEBUG"CHIP_ID_L=0x%x \n", v);
	if (v != OV2680_CHIP_ID_L)
		return -ENODEV;


	return 0;
}

static struct ov2680_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov2680_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV2680_FMTS ARRAY_SIZE(ov2680_formats)

struct ov2680_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	unsigned char  vts_gain_ctrl_1;
	unsigned char  vts_gain_ctrl_2;
	unsigned char  vts_gain_ctrl_3;
	unsigned short gain_ctrl_1;
	unsigned short gain_ctrl_2;
	unsigned short gain_ctrl_3;
	unsigned short spot_meter_win_width;
	unsigned short spot_meter_win_height;
	struct regval_list *regs; /* Regs to tweak */
	struct isp_regb_vals *regs_aecgc_win_matrix;
	struct isp_regb_vals *regs_aecgc_win_center;
	struct isp_regb_vals *regs_aecgc_win_central_weight;

};
static struct ov2680_win_size ov2680_win_size_oflim[] = {
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x50E, //
		.framerate	= 30,
		#if OV2680_LAGEN_LENS
		.max_gain_dyn_frm = 0x40,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x40,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0xb1,
		.vts_gain_ctrl_3  = 0xc2,
		.gain_ctrl_1	  = 0x18,
		.gain_ctrl_2	  = 0x28,
		.gain_ctrl_3	  = 0x48,
		#else
		.max_gain_dyn_frm = 0x60,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x60,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x71,
		.vts_gain_ctrl_3  = 0x82,
		.gain_ctrl_1	  = 0x18,
		.gain_ctrl_2	  = 0x28,
		.gain_ctrl_3	  = 0x48,
		#endif
		.spot_meter_win_width  = 400,
		.spot_meter_win_height = 300,
		.regs 			  = ov2680_win_720p,
		.regs_aecgc_win_matrix			= isp_aecgc_win_2M_matrix_oflim,
		.regs_aecgc_win_center			= isp_aecgc_win_2M_center_oflim,
		.regs_aecgc_win_central_weight	= isp_aecgc_win_2M_central_weight_oflim,

	},

};

static struct ov2680_win_size ov2680_win_size_sunny[] = {
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x50E, //
		.framerate	= 30,
		.max_gain_dyn_frm = 0x60,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x60,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x71,
		.vts_gain_ctrl_3  = 0x82,
		.gain_ctrl_1	  = 0x18,
		.gain_ctrl_2	  = 0x28,
		.gain_ctrl_3	  = 0x48,
		.spot_meter_win_width  = 400,
		.spot_meter_win_height = 300,
		.regs 			  = ov2680_win_720p,
		.regs_aecgc_win_matrix			= isp_aecgc_win_2M_matrix_sunny,
		.regs_aecgc_win_center			= isp_aecgc_win_2M_center_sunny,
		.regs_aecgc_win_central_weight	= isp_aecgc_win_2M_central_weight_sunny,

	},

};

//#define n_win_sizes (ARRAY_SIZE(ov2680_win_sizes))

static int ov2680_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV2680_FMTS)
		return -EINVAL;


	*code = ov2680_formats[index].mbus_code;
	return 0;
}

static int ov2680_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov2680_format_struct **ret_fmt,
		struct ov2680_win_size **ret_wsize)
{
	int index;
	struct ov2680_win_size *wsize;

	for (index = 0; index < N_OV2680_FMTS; index++)
		if (ov2680_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV2680_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov2680_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov2680_formats + index;

	fmt->field = V4L2_FIELD_NONE;


	for (wsize = ov2680_win_sizes; wsize < ov2680_win_sizes + n_win_sizes;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov2680_win_sizes + n_win_sizes)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov2680_formats[index].colorspace;
	return 0;
}

static int ov2680_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov2680_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov2680_set_exp_ratio(struct v4l2_subdev *sd, struct ov2680_win_size *win_size,
								struct v4l2_fmt_data *data)
{
	int i = 0;
	struct ov2680_info *info = container_of(sd, struct ov2680_info, sd);

	if(info->win == NULL || info->win == win_size)
		data->exp_ratio = 0x100;
	else {
		for(i = 0; i < N_EXP_RATIO_TABLE_SIZE; i++) {
			if((info->win->width == ov2680_exp_ratio_table[i].width_last)
				&& (info->win->height == ov2680_exp_ratio_table[i].height_last)
				&& (win_size->width  == ov2680_exp_ratio_table[i].width_cur)
				&& (win_size->height == ov2680_exp_ratio_table[i].height_cur)) {
				data->exp_ratio = ov2680_exp_ratio_table[i].exp_ratio;
				break;
			}
		}
		if(i >= N_EXP_RATIO_TABLE_SIZE) {
			data->exp_ratio = 0x100;
			printk(KERN_DEBUG"ov2680:exp_ratio is not found\n!");
		}
	}
	return 0;
}

static int ov2680_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov2680_info *info = container_of(sd, struct ov2680_info, sd);
//	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov2680_format_struct *fmt_s;
	struct ov2680_win_size *wsize;
	int ret;

	ret = ov2680_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov2680_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
	//	ret = ov2680_write_array(sd, wsize->regs);
	//	if (ret)
   	//	return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 664;  /*Mbps*/
	//	data->sensor_vts_address1 = info->vts_address1;
	//	data->sensor_vts_address2 = info->vts_address2;
		data->framerate = wsize->framerate;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;
		data->vts_gain_ctrl_1 = wsize->vts_gain_ctrl_1;
		data->vts_gain_ctrl_2 = wsize->vts_gain_ctrl_2;
		data->vts_gain_ctrl_3 = wsize->vts_gain_ctrl_3;
		data->gain_ctrl_1 = wsize->gain_ctrl_1;
		data->gain_ctrl_2 = wsize->gain_ctrl_2;
		data->gain_ctrl_3 = wsize->gain_ctrl_3;
		data->spot_meter_win_width = wsize->spot_meter_win_width;
		data->spot_meter_win_height = wsize->spot_meter_win_height;
		ov2680_set_exp_ratio(sd, wsize, data);
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov2680_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov2680_write_array(sd, ov2680_stream_on);
	else
		ret = ov2680_write_array(sd, ov2680_stream_off);

	return ret;
}

static int ov2680_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov2680_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov2680_esd_reset(struct v4l2_subdev *sd)
{
	ov2680_init(sd, 0);
	ov2680_s_stream(sd, 1);

	return 0;
}

static int ov2680_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov2680_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov2680_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov2680_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov2680_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov2680_frame_rates[interval->index];
	return 0;
}

static int ov2680_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	for (i = 0; i < n_win_sizes; i++) {
		struct ov2680_win_size *win = &ov2680_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov2680_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov2680_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	//struct v4l2_isp_firmware_setting isp_firmware_setting;
	struct ov2680_info *info = container_of(sd, struct ov2680_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = ov2680_isp_setting;
		isp_setting.size = info->isp_setting_size;
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)ov2680_isp_parm;
		break;
	case V4L2_CID_GET_EFFECT_FUNC:
		ctrl->value = (int)sensor_effect_ops;
		break;
/*
	case V4L2_CID_GET_ISP_FIRMWARE:
		isp_firmware_setting.setting = ov2680_isp_firmware;
		isp_firmware_setting.size = info->isp_firmware_size;
		memcpy((void *)ctrl->value, (void *)&isp_firmware_setting, sizeof(isp_firmware_setting));
		break;
*/
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov2680_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	//struct ov2680_info *info = container_of(sd, struct ov2680_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		if (ctrl->value == FRAME_RATE_AUTO)
			//info->dynamic_framerate = 1;
			;
		else
			ret = -EINVAL;
		break;
	case V4L2_CID_SET_SENSOR_MIRROR:
		ov2680_isp_parm->mirror = !ov2680_isp_parm->mirror;
		break;
	case V4L2_CID_SET_SENSOR_FLIP:
		ov2680_isp_parm->flip = !ov2680_isp_parm->flip;
		break;
	case V4L2_CID_SENSOR_ESD_RESET:
		ov2680_esd_reset(sd);
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

	for(i = 0; i < n_win_sizes; i++) {
		if (width == ov2680_win_sizes[i].width && height == ov2680_win_sizes[i].height) {
			switch (meter_mode){
				case METER_MODE_MATRIX:
					*vals = ov2680_win_sizes[i].regs_aecgc_win_matrix;
					break;
				case METER_MODE_CENTER:
					*vals = ov2680_win_sizes[i].regs_aecgc_win_center;
					break;
				case METER_MODE_CENTRAL_WEIGHT:
					*vals = ov2680_win_sizes[i].regs_aecgc_win_central_weight;
					break;
				default:
					break;
			}
			//ret = ARRAY_SIZE(ov2680_win_sizes[i].regs_aecgc_win);
			break;
		}
	}
	return ret;
}


static int ov2680_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV2680, 0);
}

static int ov2680_get_module_factory(struct v4l2_subdev *sd, char **module_factory)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!module_factory)
		return -1;

	if (client->addr == I2C_ADDR_OFLIM)
		*module_factory = factory_ofilm;
	else if (client->addr == I2C_ADDR_SUNNY)
		*module_factory = factory_sunny;
	else
		*module_factory = factory_unknown;

	return 0;
}

static long ov2680_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	char **module_factory = NULL;

	switch (cmd) {
		case SUBDEVIOC_MODULE_FACTORY:
			module_factory = (char**)arg;
			ret = ov2680_get_module_factory(sd, module_factory);
			break;
		default:
			ret = -ENOIOCTLCMD;
	}
	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov2680_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov2680_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov2680_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov2680_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov2680_core_ops = {
	.g_chip_ident = ov2680_g_chip_ident,
	.g_ctrl = ov2680_g_ctrl,
	.s_ctrl = ov2680_s_ctrl,
	.queryctrl = ov2680_queryctrl,
	.reset = ov2680_reset,
	.init = ov2680_init,
	.ioctl = ov2680_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov2680_g_register,
	.s_register = ov2680_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov2680_video_ops = {
	.enum_mbus_fmt = ov2680_enum_mbus_fmt,
	.try_mbus_fmt = ov2680_try_mbus_fmt,
	.s_mbus_fmt = ov2680_s_mbus_fmt,
	.s_stream = ov2680_s_stream,
	.cropcap = ov2680_cropcap,
	.g_crop	= ov2680_g_crop,
	.s_parm = ov2680_s_parm,
	.g_parm = ov2680_g_parm,
	.enum_frameintervals = ov2680_enum_frameintervals,
	.enum_framesizes = ov2680_enum_framesizes,
};

static const struct v4l2_subdev_ops ov2680_ops = {
	.core = &ov2680_core_ops,
	.video = &ov2680_video_ops,
};

static int ov2680_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov2680_info *info;
	struct comip_camera_subdev_platform_data *ov2680_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct ov2680_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov2680_ops);

	/* Make sure it's an ov2680 */
	ret = ov2680_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov2680 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov2680 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	if(client->addr == I2C_ADDR_OFLIM) {
		printk(KERN_DEBUG"ov2680 modue id is oflim!\n");
		info->i2c_addr = I2C_ADDR_OFLIM;
		ov2680_win_sizes = ov2680_win_size_oflim;
		ov2680_isp_parm = &ov2680_isp_parm_oflim;
		ov2680_isp_setting = ov2680_isp_setting_oflim;
		//ov2680_isp_firmware = ov2680_isp_firmware_oflim;
		sensor_effect_ops = &sensor_effect_ops_oflim;
		info->n_win_sizes = ARRAY_SIZE(ov2680_win_size_oflim);
		//info->isp_firmware_size = ARRAY_SIZE(ov2680_isp_firmware_oflim);
		info->isp_setting_size = ARRAY_SIZE(ov2680_isp_setting_oflim);
	}else if(client->addr == I2C_ADDR_SUNNY) {
		printk(KERN_DEBUG"ov2680 modue id is sunny!\n");
		info->i2c_addr = I2C_ADDR_SUNNY;
		ov2680_win_sizes = ov2680_win_size_sunny;
		ov2680_isp_parm = &ov2680_isp_parm_sunny;
		ov2680_isp_setting = ov2680_isp_setting_sunny;
		//ov2680_isp_firmware = ov2680_isp_firmware_sunny;
		sensor_effect_ops = &sensor_effect_ops_sunny;
		info->n_win_sizes = ARRAY_SIZE(ov2680_win_size_sunny);
		//info->isp_firmware_size = ARRAY_SIZE(ov2680_isp_firmware_sunny);
		info->isp_setting_size = ARRAY_SIZE(ov2680_isp_setting_sunny);
	}else {
		printk(KERN_DEBUG"ov2680 modue id can't be recognized! i2c_addr = 0x%x\n",info->i2c_addr);
		info->i2c_addr = I2C_ADDR_OFLIM;
		ov2680_win_sizes = ov2680_win_size_oflim;
		ov2680_isp_parm = &ov2680_isp_parm_oflim;
		ov2680_isp_setting = ov2680_isp_setting_oflim;
		//ov2680_isp_firmware = ov2680_isp_firmware_oflim;
		sensor_effect_ops = &sensor_effect_ops_oflim;
		info->n_win_sizes = ARRAY_SIZE(ov2680_win_size_oflim);
		//info->isp_firmware_size = ARRAY_SIZE(ov2680_isp_firmware_oflim);
		info->isp_setting_size = ARRAY_SIZE(ov2680_isp_setting_oflim);
	}
	n_win_sizes = info->n_win_sizes;

	ov2680_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;

	if (ov2680_setting) {
		if (ov2680_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			ov2680_isp_parm->mirror = 1;
		else
			ov2680_isp_parm->mirror = 0;

		if (ov2680_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			ov2680_isp_parm->flip = 1;
		else
			ov2680_isp_parm->flip = 0;
	}

	return 0;
}

static int ov2680_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2680_info *info = container_of(sd, struct ov2680_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov2680_id[] = {
	{ "ov2680-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov2680_id);

static struct i2c_driver ov2680_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov2680-mipi-raw",
	},
	.probe		= ov2680_probe,
	.remove		= ov2680_remove,
	.id_table	= ov2680_id,
};

static __init int init_ov2680(void)
{
	return i2c_add_driver(&ov2680_driver);
}

static __exit void exit_ov2680(void)
{
	i2c_del_driver(&ov2680_driver);
}

fs_initcall(init_ov2680);
module_exit(exit_ov2680);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov2680 sensors");
MODULE_LICENSE("GPL");
