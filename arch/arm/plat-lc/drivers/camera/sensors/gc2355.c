
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
#include "gc2355.h"

#define GC2355_CHIP_ID_H	(0x23)
#define GC2355_CHIP_ID_L	(0x55)


#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH	MAX_WIDTH//SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT//SXGA_HEIGHT

#define GC2355_REG_END		0xffff
#define GC2355_REG_DELAY	0xfffe

#define GC2355_REG_VAL_HVFLIP   0xff


//#define GC2355_DEBUG_FUNC

struct gc2355_format_struct;
struct gc2355_info {
	struct v4l2_subdev sd;
	struct gc2355_format_struct *fmt;
	struct gc2355_win_size *win;
	int binning;
	struct mutex stream_lock;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

static struct regval_list gc2355_init_regs[] = {
	{0xfe,0x80},
	{0xfe,0x80},
	{0xfe,0x80},
	{0xf2,0x00}, //sync_pad_io_ebi
	{0xf6,0x00}, //up down
	{0xf7,0x31}, //PLL enable
	{0xf8,0x06}, //PLL mode 2
	{0xf9,0x0e}, //[0]PLL enable
	{0xfa,0x00}, //div
	{0xfc,0x06},
	{0xfe,0x00},

	/////////////////////////////////////////////////////
	///////////////    ANALOG & CISCTL    ///////////////
	/////////////////////////////////////////////////////
	{0x03,0x21},
	{0x04,0x4f},
	{0x05,0x01}, //max 20fps
	{0x06,0x22},
	{0x07,0x00},
	{0x08,0x0b},
	{0x0a,0x00}, //row start
	{0x0c,0x04}, //0c//col start
	{0x0d,0x04},
	{0x0e,0xc0},
	{0x0f,0x06},
	{0x10,0x50}, //Window setting 1616x1216
	{0x17,0x17},
	{0x19,0x0b},
	{0x1b,0x49},
	{0x1c,0x12},
	{0x1d,0x10},
	{0x1e,0xbc}, //col_r/rowclk_mode/rsthigh_en FPN
	{0x1f,0xc8}, //rsgl_s_mode/vpix_s_mode
	{0x20,0x71},
	{0x21,0x20}, //rsg
	{0x22,0xa0},
	{0x23,0x51},
	{0x24,0x19}, //drv
	{0x27,0x20},
	{0x28,0x00},
	{0x2b,0x81}, //sf_s_mode FPN
	{0x2c,0x38}, //ispg FPN
	{0x2e,0x16}, //eq width
	{0x2f,0x14},
	{0x30,0x00},
	{0x31,0x01},
	{0x32,0x02},
	{0x33,0x03},
	{0x34,0x07},
	{0x35,0x0b},
	{0x36,0x0f},

	/////////////////////////////////////////////////////
	//////////////////////	 gain   /////////////////////
	/////////////////////////////////////////////////////
	{0xb0,0x50},
	{0xb1,0x01},
	{0xb2,0x00},
	{0xb3,0x40},
	{0xb4,0x40},
	{0xb5,0x40},
	{0xb6,0x00},

	/////////////////////////////////////////////////////
	//////////////////////   crop   /////////////////////
	/////////////////////////////////////////////////////
	{0x92,0x02},
	{0x94,0x00},
	{0x95,0x04},
	{0x96,0xb0},
	{0x97,0x06},
	{0x98,0x40}, //out window set 1600x1200

	/////////////////////////////////////////////////////
	//////////////////////    BLK   /////////////////////
	/////////////////////////////////////////////////////
	{0x18,0x02},
	{0x1a,0x01},
	{0x40,0x42},
	{0x41,0x00},
	{0x44,0x00},
	{0x45,0x00},
	{0x46,0x00},
	{0x47,0x00},
	{0x48,0x00},
	{0x49,0x00},
	{0x4a,0x00},
	{0x4b,0x00},
	{0x4e,0x3c},
	{0x4f,0x00},
	{0x5e,0x00},
	{0x66,0x20},
	{0x6a,0x02}, //manual offset
	{0x6b,0x02},
	{0x6c,0x00},
	{0x6d,0x00},
	{0x6e,0x00},
	{0x6f,0x00},
	{0x70,0x02},
	{0x71,0x02},

	/////////////////////////////////////////////////////
	////////////////////  dark sun  /////////////////////
	/////////////////////////////////////////////////////
	{0x87,0x03},
	{0xe0,0xe7}, //dark sun en
	{0xe3,0xc0},

	/////////////////////////////////////////////////////
	//////////////////////   MIPI   /////////////////////
	/////////////////////////////////////////////////////
	{0xfe, 0x03},
	{0x02, 0x00},
	{0x03, 0x90},
	{0x04, 0x01},
	{0x05, 0x00},
	{0x06, 0xa2},
	{0x11, 0x2b},
	{0x12, 0xd0},
	{0x13, 0x07},
	{0x15, 0x60},

	{0x21, 0x10},
	{0x24, 0x02},
	{0x26, 0x08},
	{0x27, 0x06},

	{0x01, 0x87},
	{0x10, 0x81},
	{0x22, 0x03},
	{0x23, 0x20},
	{0x25, 0x10},
	{0x29, 0x02},

	{0x2a, 0x0a},
	{0x2b, 0x08},
	{0x40, 0x00},
	{0x41, 0x00},
	{0x42, 0x40},
	{0x43, 0x06},
	{0xfe, 0x00},

	{GC2355_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list gc2355_win_2M[] = {
    {GC2355_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list gc2355_stream_on[] = {
	{0xfe, 0x03},
	{0x10, 0x91},  // 93 line_sync_mode
    {0xfe, 0x00},
	{GC2355_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2355_stream_off[] = {
		{0xfe, 0x03},
	{0x10, 0x81},  // 93 line_sync_mode
    {0xfe, 0x00},
	//{GC2355_REG_DELAY, 80},
	{GC2355_REG_END, 0x00},	/* END MARKER */
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

static int gc2355_read(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
#if 0
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf = reg;
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
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
#else
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	client->addr = 0x1a;
	ret = i2c_smbus_read_byte_data(client, reg);
	printk("Test GC2355 =0x%x\n", ret);
	client->addr = 0x3c;
	ret = i2c_smbus_read_byte_data(client, reg);
	printk("Test GC2355 =0x%x\n", ret);

	if (ret < 0)
		return ret;

	*value = (unsigned char)ret;

	return 0;
}
#endif
static int gc2355_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc2355_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != GC2355_REG_END) {
		if (vals->reg_num == GC2355_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = gc2355_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}


static int gc2355_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int gc2355_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc2355_info *info = container_of(sd, struct gc2355_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 2;
	mutex_init(&info->stream_lock);

	ret = gc2355_write_array(sd, gc2355_init_regs);
	if (ret < 0)
		return ret;
	return 0;
}

static int gc2355_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = gc2355_read(sd, 0xf0, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != GC2355_CHIP_ID_H)
		return -ENODEV;
	ret = gc2355_read(sd, 0xf1, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L=0x%x \n", v);
	if (v != GC2355_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct gc2355_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc2355_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_GC2355_FMTS ARRAY_SIZE(gc2355_formats)

static struct gc2355_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	int min_framerate;
	int binning;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
} gc2355_win_sizes[] = {

	/* MAX */
	{
		.width				= MAX_WIDTH,
		.height				= MAX_HEIGHT,
		.vts				= 0x4be,
		.framerate			= 30,
		.binning			= 0,
		.max_gain_dyn_frm 	= 0xa0,
		.min_gain_dyn_frm 	= 0x10,
		.max_gain_fix_frm 	= 0xa0,
		.min_gain_fix_frm 	= 0x10,
		.regs 				= gc2355_win_2M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(gc2355_win_sizes))

static int gc2355_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	u16 gain, exp, temp;
	gain = parm->gain * 64 / 0x10;
	exp = parm->exp;
	//vb = parm->vts - MAX_PREVIEW_HEIGHT;

	///////exp//////////
	gc2355_write(sd, 0xfe, 0x00);
	gc2355_write(sd, 0x03, (exp >> 8)&0x3f);
	gc2355_write(sd, 0x04, exp & 0xff);
	 ///////gain////////////
	gc2355_write(sd, 0xb1, 0x01);
	gc2355_write(sd, 0xb2, 0x00);
		//gain
	if(gain < 64)
	{
		gc2355_write(sd, 0xb1, 0x01);
		gc2355_write(sd, 0xb2, 0x00);

	}
	else if((64<=gain) &&(gain <88 ))
	{
		gc2355_write(sd, 0xb6, 0x00);
		temp = gain;
		gc2355_write(sd, 0xb1, temp>>6);
		gc2355_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((88<=gain) &&(gain <122 ))
	{
		gc2355_write(sd, 0xb6, 0x01);
		temp = 64*gain/88;
		gc2355_write(sd, 0xb1, temp>>6);
		gc2355_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((122<=gain) &&(gain <168 ))
	{
		gc2355_write(sd, 0xb6, 0x02);
		temp = 64*gain/122;
		gc2355_write(sd, 0xb1, temp>>6);
		gc2355_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((168<=gain) &&(gain <239 ))
	{
		gc2355_write(sd, 0xb6, 0x03);
		temp = 64*gain/168;
		gc2355_write(sd, 0xb1, temp>>6);
		gc2355_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if(239<=gain)
	{
		gc2355_write(sd, 0xb6, 0x04);
		temp = 64*gain/239;
		gc2355_write(sd, 0xb1, temp>>6);
		gc2355_write(sd, 0xb2, (temp<<2)&0xfc);
	}



	return 0;
}

static int gc2355_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC2355_FMTS)
		return -EINVAL;

	*code = gc2355_formats[index].mbus_code;
	return 0;
}

static int gc2355_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc2355_format_struct **ret_fmt,
		struct gc2355_win_size **ret_wsize)
{
	int index;
	struct gc2355_win_size *wsize;

	for (index = 0; index < N_GC2355_FMTS; index++)
		if (gc2355_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC2355_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc2355_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc2355_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = gc2355_win_sizes; wsize < gc2355_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc2355_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc2355_formats[index].colorspace;
	return 0;
}

static int gc2355_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc2355_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc2355_s_stream(struct v4l2_subdev *sd, int enable);

static int gc2355_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc2355_info *info = container_of(sd, struct gc2355_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct gc2355_format_struct *fmt_s;
	struct gc2355_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret = 0;

	ret = gc2355_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc2355_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = gc2355_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		info->fmt = fmt_s;
		info->win = wsize;
/*
		if (data->aecgc_parm.exp != 0
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = aecgc_parm.exp + 8;
			gc2355_set_aecgc_parm(sd, &aecgc_parm);
		}
*/
		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 672; /* Mbps. */
		data->framerate = wsize->framerate;
		data->binning = wsize->binning;

		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;
	}



	return 0;
}

static int gc2355_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc2355_info *info = container_of(sd, struct gc2355_info, sd);
	int ret = 0;
	
	if (enable) {
		mutex_lock(&info->stream_lock);	
		ret = gc2355_write_array(sd, gc2355_stream_on);
		mutex_unlock(&info->stream_lock);
		//info->stream_on = 1;
	}
	else {
		mutex_lock(&info->stream_lock);	
		ret = gc2355_write_array(sd, gc2355_stream_off);
		mutex_unlock(&info->stream_lock);
		//info->stream_on = 0;
	}


	return ret;
}

static int gc2355_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc2355_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc2355_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2355_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2355_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc2355_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc2355_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc2355_frame_rates[interval->index];
	return 0;
}

static int gc2355_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc2355_win_size *win = &gc2355_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc2355_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}


static int gc2355_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&gc2355_isp_setting;
		isp_setting.size = ARRAY_SIZE(gc2355_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&gc2355_isp_parm;
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

static int gc2355_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct gc2355_info *info = container_of(sd, struct gc2355_info, sd);
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		mutex_lock(&info->stream_lock);
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = gc2355_set_aecgc_parm(sd, parm);
		mutex_unlock(&info->stream_lock);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static int gc2355_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC2355, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc2355_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc2355_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int gc2355_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2355_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc2355_core_ops = {
	.g_chip_ident = gc2355_g_chip_ident,
	.g_ctrl = gc2355_g_ctrl,
	.s_ctrl = gc2355_s_ctrl,
	.queryctrl = gc2355_queryctrl,
	.reset = gc2355_reset,
	.init = gc2355_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc2355_g_register,
	.s_register = gc2355_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc2355_video_ops = {
	.enum_mbus_fmt = gc2355_enum_mbus_fmt,
	.try_mbus_fmt = gc2355_try_mbus_fmt,
	.s_mbus_fmt = gc2355_s_mbus_fmt,
	.s_stream = gc2355_s_stream,
	.cropcap = gc2355_cropcap,
	.g_crop	= gc2355_g_crop,
	.s_parm = gc2355_s_parm,
	.g_parm = gc2355_g_parm,
	.enum_frameintervals = gc2355_enum_frameintervals,
	.enum_framesizes = gc2355_enum_framesizes,
};

static const struct v4l2_subdev_ops gc2355_ops = {
	.core = &gc2355_core_ops,
	.video = &gc2355_video_ops,
};

static int gc2355_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc2355_info *info;
	struct comip_camera_subdev_platform_data *gc2355_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct gc2355_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2355_ops);

	/* Make sure it's an gc2355 */
	ret = gc2355_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc2355 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc2355 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	gc2355_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (gc2355_setting) {
		if (gc2355_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			gc2355_isp_parm.mirror = 1;
		else gc2355_isp_parm.mirror = 0;

		if (gc2355_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			gc2355_isp_parm.flip = 1;
		else gc2355_isp_parm.flip = 0;
	}

	return ret;
}

static int gc2355_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2355_info *info = container_of(sd, struct gc2355_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc2355_id[] = {
	{ "gc2355-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2355_id);

static struct i2c_driver gc2355_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2355-mipi-raw",
	},
	.probe		= gc2355_probe,
	.remove		= gc2355_remove,
	.id_table	= gc2355_id,
};

static __init int init_gc2355(void)
{
	return i2c_add_driver(&gc2355_driver);
}

static __exit void exit_gc2355(void)
{
	i2c_del_driver(&gc2355_driver);
}

fs_initcall(init_gc2355);
module_exit(exit_gc2355);

MODULE_DESCRIPTION("A low-level driver for Samsung gc2355 sensors");
MODULE_LICENSE("GPL");
