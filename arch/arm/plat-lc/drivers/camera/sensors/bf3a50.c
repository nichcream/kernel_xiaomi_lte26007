
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
#include "bf3a50.h"

//#define BF3A50_DEBUG_FUNC
//#define BF3A50_ALL_SIZES

#define BF3A50_CHIP_ID_H	(0x39)
#define BF3A50_CHIP_ID_L	(0x50)

#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		2560
#define MAX_HEIGHT		1920
#define MAX_PREVIEW_WIDTH     SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  SXGA_HEIGHT


#define SETTING_LOAD
#define BF3A50_REG_END		0xfa
#define BF3A50_REG_DELAY	0xfb

struct bf3a50_format_struct;

struct bf3a50_info {
	struct v4l2_subdev sd;
	struct bf3a50_format_struct *fmt;
	struct bf3a50_win_size *win;
	int binning;
	int stream_on;
	int frame_rate;
	int bf3a50_open;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int bf3a50_s_stream(struct v4l2_subdev *sd, int enable);

static struct regval_list bf3a50_init_regs[] = {
	{0xff , 0x01},//0xff , 0x00:page0;  0xff , 0x01:page1; 0xff , 0x02:page2;

	//{0xca , 0x83},//0xca[7]:Soft reset
	//{0xff , 0x00},
	//{0x57 , 0x07},

	//{0x65 , 0xc2},//{0x57[4] , 0x65}:50Hz Banding; //6f
	//{0x66 , 0xa2},//{0x57[5] , 0x66}:60Hz Banding; //5c

	//clock
	{0xff , 0x01},
	{0xcb , 0x12},// 48M
	{0xcc , 0x03},
	{0xcd , 0xF8},//f8 20131023
	{0xcf , 0x00},

	//{0xe3 , 0x20},

	//{0xd8 , 0x20},

	//Resolution and banding
	{0xff , 0x00},

	{0xf1 , 0x03},//disable lens correction and gamma correction
	{0x02 , 0x36},//dummy pixel low 8bits//36
	{0x07 , 0x00},//0x07[7]:Hflip;0x07[6]:Vflip;0x07[3:0]:subsample mode;

	{0xff , 0x01},

	{0x36 , 0x18},
	{0x57 , 0x00},//bit[2]:AGC Enable; bit[1]:AWB Enable;  bit[0]:AEC Enable;
	{0x65 , 0xf1},//{0x57[4] , 0x65}:50Hz Banding; //6f
	{0x66 , 0xca},//{0x57[5] , 0x66}:60Hz Banding; //5c

	//analog setting
	{0xff , 0x01},
	{0xc0 , 0x00},//{0xc0 , 0x02},chyl
	{0xc3 , 0x00},
	{0xc4 , 0x62},
	{0xc5 , 0x1f},//(36M or smaller than 36M Hz) 0xc5 , 0x0f;(More than 36M)
	{0xc6 , 0x55},//0x55//FE
	{0xc7 , 0x4f},
	{0xc8 , 0x00},
	{0xc9 , 0x34},
	{0xce , 0x77},////47 11.7
	{0xd1 , 0x17},//bit[7]:VCLK_POL1;bit[3]:CLK_POL2;//00 11.7
	{0xd6 , 0x53},
	{0xd7 , 0x03},
	{0xd8 , 0x00},
	{0xd9 , 0x54},//54 11.7
	{0xdb , 0x5a},
	{0xdc , 0xd4},
	{0xdd , 0xd4},
	{0xe3 , 0xa0},//a0 1023
	//{0xe2 , 0x03},//02 1023
	//{0xe1 , 0x55},//02 1023
	{0xff , 0x00},
	{0x11 , 0x35},

	//black level
	{0xff , 0x00},
	{0x48 , 0x20},
	{0x49 , 0x20},
	{0x4a , 0x20},
	{0x4b , 0x20},
	{0x4c , 0x1a},

	//denoise
	{0xff , 0x00},
	{0x82 , 0x27},//bit[7:4]:the bigger value,the smaller noise;[3:0]:the smaller value,the smaller noise;
	{0x85 , 0x05},//0x15:smaller noise;
	{0x88 , 0x0f},//0x0d:smaller noise;
#if 0
	//gamma setting
	{0xff , 0x00},
	{0x60 , 0x20},
	{0x61 , 0x40},
	{0x62 , 0xe0},
	{0x63 , 0xc0},//bit[6:0]gamma offset
	{0x64 , 0xc0},//bit[6:0]gamma offset
	{0x65 , 0x3C},
	{0x66 , 0x38},
	{0x67 , 0x2A},
	{0x68 , 0x20},
	{0x69 , 0x1B},
	{0x6A , 0x16},
	{0x6B , 0x12},
	{0x6C , 0x12},
	{0x6D , 0x0F},
	{0x6E , 0x0E},
	{0x6F , 0x0B},
	{0x70 , 0x0B},
	{0x71 , 0x09},
	{0x72 , 0x07},
	{0x73 , 0x07},//

	//denoise
	{0xff , 0x00},
	{0x82 , 0x27},//bit[7:4]:the bigger value,the smaller noise;[3:0]:the smaller value,the smaller noise;
	{0x85 , 0x05},//0x15:smaller noise;
	{0x88 , 0x0f},//0x0d:smaller noise;

	//AWB
	{0xff , 0x01},
	{0x22 , 0x91},
	{0x23 , 0x66},//{0x22[1:0] , 0x23}:G Gain;
	{0x25 , 0x31},//0x25[7:4]:AWB Lock;
	{0x26 , 0x08},//The low limit of B Gain in indoor scene;
	{0x27 , 0x2a},//The upper limit of B Gain in indoor scene;
	{0x28 , 0x09},//The low limit of R Gain in indoor scene;
	{0x29 , 0x2a},//The upper limit of R Gain in indoor scene;
	{0x2a , 0x04},
	{0x2b , 0x99},//[6:0]Base B gain
	{0x2c , 0x16},//Base R gain
	{0x2d , 0x52},
	{0x2e , 0x52},
	{0x2f , 0x1d},
	{0x30 , 0x3c},
	{0x31 , 0xf0},
	{0x32 , 0x57},
	{0x34 , 0x30},//The offset of F light;
	{0x35 , 0x00},//The offset of NF light;
	{0x36 , 0x18},
	{0x3a , 0x0d},//The low limit of B Gain in outdoor scene;
	{0x3b , 0x10},//The upper limit of B Gain in outdoor scene;
	{0x3c , 0x09},//The low limit of R Gain in outdoor scene;
	{0x3d , 0x24},//The upper limit of R Gain in outdoor scene;

	//AE
	{0xff , 0x01},
	{0x58 , 0x48},//
	{0x5a , 0x88},
	{0x5b , 0x88},//[1]:0,choose 60Hz step;1,choose 50Hz step
	{0x5d , 0x50},//Minimum Global gain;
	{0x5e , 0x70},
	{0x5f , 0x50},
	{0x60 , 0x7d},
	{0x61 , 0x88},//Maximum Global gain;
	{0x64 , 0xa5},//{0x64[7:3]:INT_MAX
	{0x6a , 0x50},
	{0x73 , 0xb5},
	{0x70 , 0x82},
	{0x74 , 0x50},
	{0x75 , 0x10},//
	{0x76 , 0x1d},
	{0x77 , 0x1e},
	{0x78 , 0x37},
	{0x79 , 0x38},
	{0x7a , 0x62},
	{0x7b , 0x63},
	{0x7c , 0xb5},//AE Gain curve

	//shading
	{0xff , 0x02},//page 2
	{0x00 , 0x1D},
	{0x01 , 0xC6},
	{0x02 , 0x31},
	{0x03 , 0x08},
	{0x04 , 0xFF},
	{0x05 , 0x91},
	{0x06 , 0x3A},
	{0x07 , 0xC7},
	{0x08 , 0xB3},
	{0x09 , 0xF5},
	{0x0A , 0x56},
	{0x0B , 0xFC},
	{0x0C , 0x63},
	{0x0D , 0x8D},
	{0x0E , 0xD9},
	{0x0F , 0xBE},
	{0x10 , 0x85},
	{0x11 , 0x0E},
	{0x12 , 0x8C},
	{0x13 , 0x0C},
	{0x14 , 0x0F},
	{0x15 , 0xA0},
	{0x16 , 0x13},
	{0x17 , 0x8D},
	{0x18 , 0x8C},
	{0x19 , 0x13},
	{0x1A , 0x87},
	{0x1B , 0x01},
	{0x1C , 0x0D},
	{0x1D , 0x91},
	{0x1E , 0x02},
	{0x1F , 0x04},
	{0x20 , 0x74},
	{0x21 , 0x63},
	{0x22 , 0xBB},
	{0x23 , 0x5C},
	{0x24 , 0x7C},
	{0x25 , 0x14},
	{0x26 , 0xBA},
	{0x27 , 0x5B},
	{0x28 , 0xFE},
	{0x29 , 0x51},
	{0x2A , 0xA9},
	{0x2B , 0x35},
	{0x2C , 0xA6},
	{0x2D , 0x0B},
	{0x2E , 0x23},
	{0x2F , 0xBE},
	{0x30 , 0x85},
	{0x31 , 0x0F},
	{0x32 , 0x8C},
	{0x33 , 0x0C},
	{0x34 , 0x0E},
	{0x35 , 0x9E},
	{0x36 , 0x12},
	{0x37 , 0x8E},
	{0x38 , 0x8A},
	{0x39 , 0x11},
	{0x3A , 0x86},
	{0x3B , 0x02},
	{0x3C , 0x0D},
	{0x3D , 0x92},
	{0x3E , 0x03},
	{0x3F , 0x04},
	{0x40 , 0x9B},
	{0x41 , 0x6A},
	{0x42 , 0xB3},
	{0x43 , 0x0C},
	{0x44 , 0x5A},
	{0x45 , 0x7B},
	{0x46 , 0x37},
	{0x47 , 0x99},
	{0x48 , 0xC9},
	{0x49 , 0x4E},
	{0x4A , 0xB0},
	{0x4B , 0xD7},
	{0x4C , 0x65},
	{0x4D , 0x78},
	{0x4E , 0xAD},
	{0x4F , 0xCB},
	{0x50 , 0x83},
	{0x51 , 0x0C},
	{0x52 , 0x8B},
	{0x53 , 0x0C},
	{0x54 , 0x10},
	{0x55 , 0xA1},
	{0x56 , 0x14},
	{0x57 , 0x8D},
	{0x58 , 0x8E},
	{0x59 , 0x17},
	{0x5A , 0x88},
	{0x5B , 0x01},
	{0x5C , 0x0D},
	{0x5D , 0x91},
	{0x5E , 0x02},
	{0x5F , 0x04},

	//AF
	{0xff , 0x01},
	{0xc4 , 0x60},
	{0xff , 0x00},
	{0xbe , 0x8a},
	{0xb0 , 0x64},
#endif
	{BF3A50_REG_END, 0x00},
};

#ifdef BF3A50_ALL_SIZES
static struct regval_list bf3a50_win_vga[] = {
	{0x05, 0x01},
	{0x06, 0x0b},
	{0x07, 0x00},
	{0x08, 0x1c},
	{0x09, 0x01},
	{0x0a, 0xb0},
	{0x0b, 0x01},
	{0x0c, 0x10},
	{0x0d, 0x04},
	{0x0e, 0x48},
	{0x0f, 0x07},
	{0x10, 0xd0},

	{0x18, 0x02},
	{0x80, 0x10},
	{0x89, 0x03},
	{0x8b, 0x61},
	{0x40, 0x22},

	{0x94, 0x4d},
	{0x95, 0x04},
	{0x96, 0x38},
	{0x97, 0x07},
	{0x98, 0x80},

	{0xfe, 0x03},
	{0x04, 0xe0},
	{0x05, 0x01},
	{0x12, 0x60},
	{0x13, 0x09},
	{0x42, 0x80},
	{0x43, 0x07},
	{0xfe, 0x00},
	{BF3A50_REG_END, 0x00},	/* END MARKER */
};
#endif

static struct regval_list bf3a50_win_SXGA[] = {
	{0xff, 0x00},
	{0x07, 0x15},
	{0x08, 0x12},
	{0x09, 0x1a},
	{0x0a, 0xa0},
	{0x0b, 0x0e},
	{0x0c, 0x96},
	{0x0d, 0x70},

	//{BF3A50_REG_END, 0x00},	/* END MARKER */
};
#define N_REG_SXGA_SIZES (ARRAY_SIZE(bf3a50_win_SXGA))

static struct regval_list bf3a50_win_5M[] = {
	{0xff , 0x00},
	{0x07 , 0x10},
	{0x08 , 0x14},
	{0x09 , 0x18},
	{0x0a , 0xA0},
	{0x0b , 0x10},
	{0x0c , 0x94},
	{0x0d , 0x70},

    //{BF3A50_REG_END, 0x00},     /* END MARKER */
};
#define N_REG_5M_SIZES (ARRAY_SIZE(bf3a50_win_5M))

static struct regval_list bf3a50_stream_on[] = {
	{0xff, 0x01},
	{0xc3, 0x00},

    {BF3A50_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list bf3a50_stream_off[] = {
	{0xff, 0x01},
	{0xc3, 0x80},

    {BF3A50_REG_END, 0x00},     /* END MARKER */
};

static int bf3a50_read(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return ret;

	*value = (unsigned char)ret;

	return 0;
}

static int bf3a50_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int bf3a50_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != BF3A50_REG_END) {
		if (vals->reg_num == BF3A50_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = bf3a50_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#ifdef BF3A50_DEBUG_FUNC
static int bf3a50_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != BF3A50_REG_END) {
		if (vals->reg_num == BF3A50_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = bf3a50_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

static int bf3a50_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int bf3a50_init(struct v4l2_subdev *sd, u32 val)
{
	struct bf3a50_info *info = container_of(sd, struct bf3a50_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;

	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret=bf3a50_write_array(sd, bf3a50_init_regs);

	msleep(100);
	info->bf3a50_open = 1;

	if (ret < 0)
		return ret;

	return ret;
}

static int bf3a50_detect(struct v4l2_subdev *sd)
{
	unsigned char v = 0;
	int ret;

	ret = bf3a50_write(sd, 0xff, 0);
	if (ret < 0)
		return ret;
	ret = bf3a50_read(sd, 0xfc, &v);
	printk("CHIP_ID_H=0x%x ", v);
	if (v != BF3A50_CHIP_ID_H)
		return -ENODEV;
	ret = bf3a50_read(sd, 0xfd, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L=0x%x \n", v);
	if (v != BF3A50_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct bf3a50_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} bf3a50_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_BF3A50_FMTS ARRAY_SIZE(bf3a50_formats)

static struct bf3a50_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	int min_framerate;
	int down_steps;
	int down_small_step;
	int down_large_step;
	int up_steps;
	int up_small_step;
	int up_large_step;
	struct regval_list *regs; /* Regs to tweak */
} bf3a50_win_sizes[] = {
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.vts        = 0x3CE,
		.framerate  = 24,
		.min_framerate 		= 6,
		.down_steps 		= 18,
		.down_small_step 	= 2,
		.down_large_step	= 6,
		.up_steps 			= 18,
		.up_small_step		= 2,
		.up_large_step		= 6,
		.regs 		= bf3a50_win_SXGA,
	},
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts        = 0x78E,
		.framerate  = 8,
		.regs 		= bf3a50_win_5M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(bf3a50_win_sizes))

static int bf3a50_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	u16 gain, exp, vb;
	gain = parm->gain * 16 / 100;
	exp = parm->exp;
	vb = parm->vts - MAX_PREVIEW_HEIGHT;

	///////exp//////////
	bf3a50_write(sd, 0xff, 0x01);
	bf3a50_write(sd, 0x6d, (exp >> 8)&0xff);
	bf3a50_write(sd, 0x6e, exp & 0xff);

	///////gain////////////
	if(gain < 0x50)
	{
		bf3a50_write(sd, 0x6b, 0x50);
	}
	else
	{
		bf3a50_write(sd, 0x6b, gain);
	}

	//vb ///// don't change
	//bf3a50_write(sd, 0x07, (vb >> 8));
	//bf3a50_write(sd, 0x08, (vb & 0xff));

	//printk("gain=%04x exp=%04x vts=%04x\n", parm->gain/100, exp, parm->vts);

	return 0;
}

static int bf3a50_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_BF3A50_FMTS)
		return -EINVAL;


	*code = bf3a50_formats[index].mbus_code;
	return 0;
}

static int bf3a50_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct bf3a50_format_struct **ret_fmt,
		struct bf3a50_win_size **ret_wsize)
{
	int index;
	struct bf3a50_win_size *wsize;

	for (index = 0; index < N_BF3A50_FMTS; index++)
		if (bf3a50_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_BF3A50_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = bf3a50_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = bf3a50_formats + index;

	fmt->field = V4L2_FIELD_NONE;


	for (wsize = bf3a50_win_sizes; wsize < bf3a50_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= bf3a50_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = bf3a50_formats[index].colorspace;
	return 0;
}

static int bf3a50_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return bf3a50_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int bf3a50_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct bf3a50_info *info = container_of(sd, struct bf3a50_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct bf3a50_format_struct *fmt_s;
	struct bf3a50_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret,reg_index;

	ret = bf3a50_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = bf3a50_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs)
	{
		if (data->aecgc_parm.vts != 0
			|| data->aecgc_parm.exp != 0
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = data->aecgc_parm.vts;
			bf3a50_set_aecgc_parm(sd, &aecgc_parm);
		}

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->framerate = wsize->framerate;
		data->binning = info->binning;
		//data->flags = V4L2_I2C_ADDR_16BIT;
		data->slave_addr = client->addr;

		if (bf3a50_win_SXGA == wsize->regs) {
			data->min_framerate = wsize->min_framerate;
			data->down_steps = wsize->down_steps;
			data->down_small_step = wsize->down_small_step;
			data->down_large_step = wsize->down_large_step;
			data->up_steps = wsize->up_steps;
			data->up_small_step = wsize->up_small_step;
			data->up_large_step = wsize->up_large_step;

			for(reg_index = 0;reg_index < N_REG_SXGA_SIZES;reg_index++){
				data->reg[reg_index].addr = bf3a50_win_SXGA[reg_index].reg_num;
				data->reg[reg_index].data = bf3a50_win_SXGA[reg_index].value;
			}
			data->reg_num = N_REG_SXGA_SIZES;

			if(!info->bf3a50_open)
				msleep(60);
			else
				info->bf3a50_open = 0;

		} else if (bf3a50_win_5M == wsize->regs) {
			for(reg_index = 0;reg_index < N_REG_5M_SIZES;reg_index++){
				data->reg[reg_index].addr = bf3a50_win_5M[reg_index].reg_num;
				data->reg[reg_index].data = bf3a50_win_5M[reg_index].value;
			}
			data->reg_num = N_REG_5M_SIZES;
		}

		if (info->stream_on)
			bf3a50_s_stream(sd, 1);
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int bf3a50_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct bf3a50_info *info = container_of(sd, struct bf3a50_info, sd);
	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;
	return 0;
}

static int bf3a50_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct bf3a50_info *info = container_of(sd, struct bf3a50_info, sd);
	int ret = 0;

	if (enable) {
		ret = bf3a50_write_array(sd, bf3a50_stream_on);
		info->stream_on = 1;
	}
	else {
		ret = bf3a50_write_array(sd, bf3a50_stream_off);
		info->stream_on = 0;
	}

	#ifdef BF3A50_DEBUG_FUNC
	bf3a50_read_array(sd,bf3a50_init_regs );//debug only
	#endif

	return ret;
}

static int bf3a50_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int bf3a50_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int bf3a50_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3a50_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3a50_frame_rates[] = { 30, 15, 10, 5, 1 };

static int bf3a50_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(bf3a50_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = bf3a50_frame_rates[interval->index];
	return 0;
}

static int bf3a50_enum_framesizes(struct v4l2_subdev *sd,
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
		struct bf3a50_win_size *win = &bf3a50_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int bf3a50_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int bf3a50_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&bf3a50_isp_setting;
		isp_setting.size = ARRAY_SIZE(bf3a50_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&bf3a50_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bf3a50_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = bf3a50_set_aecgc_parm(sd, parm);
		break;
	case V4L2_CID_FRAME_RATE:
		ret = bf3a50_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}


static int bf3a50_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_BF3A50, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bf3a50_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bf3a50_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int bf3a50_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	bf3a50_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops bf3a50_core_ops = {
	.g_chip_ident = bf3a50_g_chip_ident,
	.g_ctrl = bf3a50_g_ctrl,
	.s_ctrl = bf3a50_s_ctrl,
	.queryctrl = bf3a50_queryctrl,
	.reset = bf3a50_reset,
	.init = bf3a50_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = bf3a50_g_register,
	.s_register = bf3a50_s_register,
#endif
};

static const struct v4l2_subdev_video_ops bf3a50_video_ops = {
	.enum_mbus_fmt = bf3a50_enum_mbus_fmt,
	.try_mbus_fmt = bf3a50_try_mbus_fmt,
	.s_mbus_fmt = bf3a50_s_mbus_fmt,
	.s_stream = bf3a50_s_stream,
	.cropcap = bf3a50_cropcap,
	.g_crop	= bf3a50_g_crop,
	.s_parm = bf3a50_s_parm,
	.g_parm = bf3a50_g_parm,
	.enum_frameintervals = bf3a50_enum_frameintervals,
	.enum_framesizes = bf3a50_enum_framesizes,
};

static const struct v4l2_subdev_ops bf3a50_ops = {
	.core = &bf3a50_core_ops,
	.video = &bf3a50_video_ops,
};

static int bf3a50_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct bf3a50_info *info;
	int ret;

	info = kzalloc(sizeof(struct bf3a50_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &bf3a50_ops);

	/* Make sure it's an bf3a50 */
	ret = bf3a50_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an bf3a50 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "bf3a50 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int bf3a50_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf3a50_info *info = container_of(sd, struct bf3a50_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id bf3a50_id[] = {
	{ "bf3a50", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf3a50_id);

static struct i2c_driver bf3a50_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bf3a50",
	},
	.probe		= bf3a50_probe,
	.remove		= bf3a50_remove,
	.id_table	= bf3a50_id,
};

static __init int init_bf3a50(void)
{
	return i2c_add_driver(&bf3a50_driver);
}

static __exit void exit_bf3a50(void)
{
	i2c_del_driver(&bf3a50_driver);
}

fs_initcall(init_bf3a50);
module_exit(exit_bf3a50);

MODULE_DESCRIPTION("A low-level driver for BYD bf3a50 sensors");
MODULE_LICENSE("GPL");
