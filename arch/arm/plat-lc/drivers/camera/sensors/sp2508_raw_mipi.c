
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
#include "sp2508_raw_mipi.h"

#define SP2508_CHIP_ID_H	(0x25)
#define SP2508_CHIP_ID_L	(0x08)


#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH	MAX_WIDTH//SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT//SXGA_HEIGHT

#define SP2508_REG_END		0xff
#define SP2508_REG_DELAY	0xff

#define SP2508_REG_VAL_HVFLIP   0xff


//#define SP2508_DEBUG_FUNC

struct sp2508_format_struct;
struct sp2508_info {
	struct v4l2_subdev sd;
	struct sp2508_format_struct *fmt;
	struct sp2508_win_size *win;
	int binning;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

static struct regval_list sp2508_init_regs[] = {


		{0xfd,0x00},
		{0xfd,0x00},
		{0xfd,0x00},
		{0xfd,0x00},
		{0xfd,0x00},
		{0x35,0x20}, //pll bias
		{0x2f,0x08}, //0x10 pll clk 84M//0x08 pll clk 60M
		{0x1c,0x03}, //pull down parallel pad
		{0x1b,0x1f},

		{0xfd,0x01},
		{0x21,0xee},//3base fliker gpw20140607
		{0x03,0x03}, //exp time, 1 base
		{0x04,0x09},
		{0x06,0x10}, //vblank
		{0x24,0x60}, //pga gain 15x
		{0x01,0x01}, //enable reg write
		{0x2b,0xc6}, //readout vref
		{0x2e,0x20}, //dclk delay
		{0x79,0x42}, //p39 p40
		{0x85,0x0f}, //p51
		{0x09,0x01}, //hblank
		{0x0a,0x40},
		{0x25,0xf2}, //reg dac 2.7v, enable bl_en,vbl 1.4v
		{0x26,0x00}, //vref2 1v, disable ramp driver
		{0x2a,0xea}, //bypass dac res, adc range 0.745, vreg counter 0.9
		{0x2c,0xf0}, //high 8bit, pldo 2.7v
		{0x8a,0x33}, //pixel bias 2uA //pixel 1ua //20140607 0x33
		{0x8b,0x33},
		{0x19,0xf3}, //icom1 1.7u, icom2 0.6u
		{0x11,0x30}, //rst num
		{0xd0,0x00}, //boost2 enable
		{0xd1,0x01}, //boost2 start point h'1do
		{0xd2,0xd0},
		{0x55,0x10},
		{0x58,0x40},//0x30 //20140607
		{0x5d,0x15},
		{0x5e,0x05},
		{0x64,0x40},
		{0x65,0x00},
		{0x66,0x66},
		{0x67,0x00},
		{0x68,0x68},
		{0x72,0x70},
		{0xfb,0x00},//{0xfb,0x25},
		{0xfd,0x02}, //raw data digital gain
		{0x00,0x8c},
		{0x01,0x8c},
		{0x03,0x8c},
		{0x04,0x8c},
		{0xfd,0x01}, //mipi
		{0xb3,0x00},
		{0x93,0x01},
		{0x9d,0x17},
		{0xc5,0x01},
		{0xc6,0x00},
		{0xb1,0x01},
		{0x8e,0x06},
		{0x8f,0x50},
		{0x90,0x04},
		{0x91,0xc0},
		{0x92,0x01},
		{0xa1,0x05},
		{0xaa,0x00},
		{0xac,0x00},

		{SP2508_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list sp2508_win_2M[] = {
		{0xfd,0x00},
		{0xfd,0x00},
		{0x35,0x20}, //pll bias
		{0x2f,0x08}, //0x10 pll clk 84M//0x08 pll clk 60M
		{0x1c,0x03}, //pull down parallel pad
		{0x1b,0x1f},

		{0xfd,0x01},
		{0x21,0xee},//3base fliker gpw20140607
		{0x03,0x03}, //exp time, 1 base
		{0x04,0x09},
		{0x06,0x10}, //vblank
		{0x24,0x60}, //pga gain 15x
		{0x01,0x01}, //enable reg write
		{0x2b,0xc6}, //readout vref
		{0x2e,0x20}, //dclk delay
		{0x79,0x42}, //p39 p40
		{0x85,0x0f}, //p51
		{0x09,0x01}, //hblank
		{0x0a,0x40},
		{0x25,0xf2}, //reg dac 2.7v, enable bl_en,vbl 1.4v
		{0x26,0x00}, //vref2 1v, disable ramp driver
		{0x2a,0xea}, //bypass dac res, adc range 0.745, vreg counter 0.9
		{0x2c,0xf0}, //high 8bit, pldo 2.7v
		{0x8a,0x33}, //pixel bias 2uA //pixel 1ua //20140607 0x33
		{0x8b,0x33},
		{0x19,0xf3}, //icom1 1.7u, icom2 0.6u
		{0x11,0x30}, //rst num
		{0xd0,0x00}, //boost2 enable
		{0xd1,0x01}, //boost2 start point h'1do
		{0xd2,0xd0},
		{0x55,0x10},
		{0x58,0x40},//0x30 //20140607
		{0x5d,0x15},
		{0x5e,0x05},
		{0x64,0x40},
		{0x65,0x00},
		{0x66,0x66},
		{0x67,0x00},
		{0x68,0x68},
		{0x72,0x70},
		{0xfb,0x00},//{0xfb,0x25},
		{0xfd,0x02}, //raw data digital gain
		{0x00,0x8c},
		{0x01,0x8c},
		{0x03,0x8c},
		{0x04,0x8c},
		{0xfd,0x01}, //mipi
		{0xb3,0x00},
		{0x93,0x01},
		{0x9d,0x17},
		{0xc5,0x01},
		{0xc6,0x00},
		{0xb1,0x01},
		{0x8e,0x06},
		{0x8f,0x50},
		{0x90,0x04},
		{0x91,0xc0},
		{0x92,0x01},
		{0xa1,0x05},
		{0xaa,0x00},
		{0xac,0x00},
		{SP2508_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list sp2508_stream_on[] = {
		{0xfd, 0x01},
		{0xac, 0x01},  // 93 line_sync_mode
		{0xfd, 0x00},
		{SP2508_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sp2508_stream_off[] = {
		{0xfd, 0x01},
		{0xac, 0x00},  // 93 line_sync_mode
		{0xfd, 0x00},
		//{SP2508_REG_DELAY, 80},
		{SP2508_REG_END, 0x00},	/* END MARKER */
};



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
	case EXPOSURE_L6:
	case EXPOSURE_L5:
	case EXPOSURE_L4:
	case EXPOSURE_L3:
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
	case EXPOSURE_H4:
	case EXPOSURE_H5:
	case EXPOSURE_H6:
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
	case CONTRAST_L4:
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
	case BRIGHTNESS_L4:
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
	case BRIGHTNESS_H4:
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
	case SHARPNESS_H3:
	case SHARPNESS_H4:
	case SHARPNESS_H5:
	case SHARPNESS_H6:
	case SHARPNESS_H7:
	case SHARPNESS_H8:
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
};

static int sp2508_read(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	client->addr = 0x1a;
	ret = i2c_smbus_read_byte_data(client, reg);
	printk("Test SP2508 =0x%x\n", ret);
	client->addr = 0x3c;
	ret = i2c_smbus_read_byte_data(client, reg);
	printk("Test SP2508 =0x%x\n", ret);

	if (ret < 0)
		return ret;

	*value = (unsigned char)ret;

	return 0;
}

static int sp2508_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sp2508_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != SP2508_REG_END) {
		if (vals->reg_num == SP2508_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = sp2508_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}


static int sp2508_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int sp2508_init(struct v4l2_subdev *sd, u32 val)
{
	struct sp2508_info *info = container_of(sd, struct sp2508_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 2;

	ret = sp2508_write_array(sd, sp2508_init_regs);
	if (ret < 0)
		return ret;
	return 0;
}

static int sp2508_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = sp2508_read(sd, 0x02, &v);
	if (ret < 0)
		return ret;
	printk("SP2508_CHIP_ID_H=0x%x ", v);
	if (v != SP2508_CHIP_ID_H)
		return -ENODEV;
	ret = sp2508_read(sd, 0x03, &v);
	if (ret < 0)
		return ret;
	printk("SP2508_CHIP_ID_L=0x%x \n", v);
	if (v != SP2508_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct sp2508_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} sp2508_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_SP2508_FMTS ARRAY_SIZE(sp2508_formats)

static struct sp2508_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	int min_framerate;
	int binning;
	int bandfilter_thres; /*(exp*gain)>>6,to close bandfilter */
	int bandfilter_range; /*to open bandfilter */
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	int down_steps;
	int down_small_step;
	int down_large_step;
	int up_steps;
	int up_small_step;
	int up_large_step;
	struct regval_list *regs; /* Regs to tweak */
} sp2508_win_sizes[] = {

	/* MAX */
	{
		.width				= MAX_WIDTH,
		.height				= MAX_HEIGHT,
		.vts				= 0xa60,
		.framerate			= 20,
		.binning			= 0,
		.bandfilter_thres	= 0x250,
		.bandfilter_range	= 0x50,
		.max_gain_dyn_frm 	= 0xa0,
		.min_gain_dyn_frm 	= 0x10,
		.max_gain_fix_frm 	= 0xa0,
		.min_gain_fix_frm 	= 0x10,
		.regs 				= sp2508_win_2M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(sp2508_win_sizes))

static int sp2508_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	u16 gain, exp, vb,temp;
	gain = parm->gain * 64 / 100;
	exp = parm->exp;
	vb = parm->vts - MAX_PREVIEW_HEIGHT;

	///////exp//////////
	sp2508_write(sd, 0x03, (exp >> 8)&0x1f);
	sp2508_write(sd, 0x04, exp & 0xff);
	 ///////gain////////////
	sp2508_write(sd, 0xb1, 0x01);
	sp2508_write(sd, 0xb2, 0x00);
		//gain
	if(gain < 64)
	{
		sp2508_write(sd, 0xb1, 0x01);
		sp2508_write(sd, 0xb2, 0x00);

	}
	else if((64<=gain) &&(gain <88 ))
	{
		sp2508_write(sd, 0xb6, 0x00);
		temp = gain;
		sp2508_write(sd, 0xb1, temp>>6);
		sp2508_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((88<=gain) &&(gain <122 ))
	{
		sp2508_write(sd, 0xb6, 0x01);
		temp = 64*gain/88;
		sp2508_write(sd, 0xb1, temp>>6);
		sp2508_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((122<=gain) &&(gain <168 ))
	{
		sp2508_write(sd, 0xb6, 0x02);
		temp = 64*gain/122;
		sp2508_write(sd, 0xb1, temp>>6);
		sp2508_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((168<=gain) &&(gain <239 ))
	{
		sp2508_write(sd, 0xb6, 0x03);
		temp = 64*gain/168;
		sp2508_write(sd, 0xb1, temp>>6);
		sp2508_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if(239<=gain)
	{
		sp2508_write(sd, 0xb6, 0x04);
		temp = 64*gain/239;
		sp2508_write(sd, 0xb1, temp>>6);
		sp2508_write(sd, 0xb2, (temp<<2)&0xfc);
	}



	return 0;
}

static int sp2508_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_SP2508_FMTS)
		return -EINVAL;

	*code = sp2508_formats[index].mbus_code;
	return 0;
}

static int sp2508_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct sp2508_format_struct **ret_fmt,
		struct sp2508_win_size **ret_wsize)
{
	int index;
	struct sp2508_win_size *wsize;

	for (index = 0; index < N_SP2508_FMTS; index++)
		if (sp2508_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_SP2508_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = sp2508_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = sp2508_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = sp2508_win_sizes; wsize < sp2508_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= sp2508_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = sp2508_formats[index].colorspace;
	return 0;
}

static int sp2508_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return sp2508_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int sp2508_s_stream(struct v4l2_subdev *sd, int enable);

static int sp2508_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct sp2508_info *info = container_of(sd, struct sp2508_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct sp2508_format_struct *fmt_s;
	struct sp2508_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret = 0;

	ret = sp2508_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = sp2508_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = sp2508_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		info->fmt = fmt_s;
		info->win = wsize;

		if (data->aecgc_parm.exp != 0
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = aecgc_parm.exp + 8;
			sp2508_set_aecgc_parm(sd, &aecgc_parm);
		}

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 468; //hzl 672 /* Mbps. */
		data->framerate = wsize->framerate;
		data->binning = wsize->binning;
		data->bandfilter_range = wsize->bandfilter_range;
		data->bandfilter_thres = wsize->bandfilter_thres;
		//data->min_framerate = wsize->min_framerate;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;

	}

	return 0;
}

static int sp2508_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sp2508_info *info = container_of(sd, struct sp2508_info, sd);
	int ret = 0;

	if (enable) {
		ret = sp2508_write_array(sd, sp2508_stream_on);
		//info->stream_on = 1;
	}
	else {
		ret = sp2508_write_array(sd, sp2508_stream_off);
		//info->stream_on = 0;
	}


	return ret;
}

static int sp2508_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int sp2508_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int sp2508_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp2508_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp2508_frame_rates[] = { 30, 15, 10, 5, 1 };

static int sp2508_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(sp2508_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = sp2508_frame_rates[interval->index];
	return 0;
}

static int sp2508_enum_framesizes(struct v4l2_subdev *sd,
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
		struct sp2508_win_size *win = &sp2508_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int sp2508_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}


static int sp2508_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&sp2508_isp_setting;
		isp_setting.size = ARRAY_SIZE(sp2508_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&sp2508_isp_parm;
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

static int sp2508_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = sp2508_set_aecgc_parm(sd, parm);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static int sp2508_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SP2508, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sp2508_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sp2508_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int sp2508_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sp2508_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops sp2508_core_ops = {
	.g_chip_ident = sp2508_g_chip_ident,
	.g_ctrl = sp2508_g_ctrl,
	.s_ctrl = sp2508_s_ctrl,
	.queryctrl = sp2508_queryctrl,
	.reset = sp2508_reset,
	.init = sp2508_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = sp2508_g_register,
	.s_register = sp2508_s_register,
#endif
};

static const struct v4l2_subdev_video_ops sp2508_video_ops = {
	.enum_mbus_fmt = sp2508_enum_mbus_fmt,
	.try_mbus_fmt = sp2508_try_mbus_fmt,
	.s_mbus_fmt = sp2508_s_mbus_fmt,
	.s_stream = sp2508_s_stream,
	.cropcap = sp2508_cropcap,
	.g_crop	= sp2508_g_crop,
	.s_parm = sp2508_s_parm,
	.g_parm = sp2508_g_parm,
	.enum_frameintervals = sp2508_enum_frameintervals,
	.enum_framesizes = sp2508_enum_framesizes,
};

static const struct v4l2_subdev_ops sp2508_ops = {
	.core = &sp2508_core_ops,
	.video = &sp2508_video_ops,
};

static int sp2508_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sp2508_info *info;
	struct comip_camera_subdev_platform_data *sp2508_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct sp2508_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &sp2508_ops);

	/* Make sure it's an sp2508 */
	ret = sp2508_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an sp2508 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "sp2508 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	sp2508_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (sp2508_setting) {
		if (sp2508_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			sp2508_isp_parm.mirror = 1;
		else sp2508_isp_parm.mirror = 0;

		if (sp2508_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			sp2508_isp_parm.flip = 1;
		else sp2508_isp_parm.flip = 0;
	}

	return ret;
}

static int sp2508_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp2508_info *info = container_of(sd, struct sp2508_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id sp2508_id[] = {
	{ "sp2508-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sp2508_id);

static struct i2c_driver sp2508_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sp2508-mipi-raw",
	},
	.probe		= sp2508_probe,
	.remove		= sp2508_remove,
	.id_table	= sp2508_id,
};

static __init int init_sp2508(void)
{
	return i2c_add_driver(&sp2508_driver);
}

static __exit void exit_sp2508(void)
{
	i2c_del_driver(&sp2508_driver);
}

fs_initcall(init_sp2508);
module_exit(exit_sp2508);

MODULE_DESCRIPTION("A low-level driver for Samsung sp2508 sensors");
MODULE_LICENSE("GPL");
