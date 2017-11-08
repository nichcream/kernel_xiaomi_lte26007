
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
#include "gc2235.h"

//#define GC2235_DEBUG_FUNC
//#define GC2235_ALL_SIZES

#define GC2235_CHIP_ID_H	(0x22)
#define GC2235_CHIP_ID_L	(0x35)

#define VGA_WIDTH		648
#define VGA_HEIGHT		480
#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200
#define MAX_PREVIEW_WIDTH     MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT


#define SETTING_LOAD
#define GC2235_REG_END		0xff
#define GC2235_REG_DELAY	0xff

struct gc2235_format_struct;

struct gc2235_info {
	struct v4l2_subdev sd;
	struct gc2235_format_struct *fmt;
	struct gc2235_win_size *win;
	int binning;
	int stream_on;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int gc2235_s_stream(struct v4l2_subdev *sd, int enable);

static struct regval_list gc2235_init_regs[] = {
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xf2, 0x00}, //sync_pad_io_ebi
	{0xf6, 0x00}, //up down
	{0xfc, 0x06},
	{0xf7, 0x15}, //pll enable
	{0xf8, 0x86}, //Pll mode 2
	{0xfa, 0x00}, //div
	{0xf9, 0xfe}, //[0] pll enable
	{0xfe, 0x00},

	/////////////////////////////////////////////////////
	////////////////   ANALOG & CISCTL	 ////////////////
	/////////////////////////////////////////////////////

	{0x03, 0x05},
	{0x04, 0x4b},
	{0x05, 0x00},
	{0x06, 0xe4},
	{0x07, 0x00},
	{0x08, 0x32},
	{0x0a, 0x02},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0xd0},
	{0x0f, 0x06},
	{0x10, 0x50},
	{0x17, 0x16}, // 15 //[0]mirror [1]flip
	{0x18, 0x12},
	{0x19, 0x06},
	{0x1a, 0x01},
	{0x1b, 0x48},
	{0x1e, 0x88},
	{0x1f, 0x48},
	{0x20, 0x03},
	{0x21, 0x6f},
	{0x22, 0x80},
	{0x23, 0xc1},
	{0x24, 0x2f},
	{0x26, 0x01},
	{0x27, 0x30},
	{0x3f, 0x00},

	/////////////////////////////////////////////////////
	//////////////////////	 ISP   //////////////////////
	/////////////////////////////////////////////////////

	{0x8b, 0xa0},
	{0x8c, 0x02},
	{0x90, 0x01},
	{0x92, 0x02},
	{0x94, 0x06},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},

	/////////////////////////////////////////////////////
	//////////////////////	 BLK   //////////////////////
	/////////////////////////////////////////////////////

	{0x40, 0x72},
	{0x41, 0x04},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	{0x63, 0x00},
	{0x64, 0x00},
	{0x65, 0x00},
	{0x66, 0x20},
	{0x67, 0x20},
	{0x68, 0x20},
	{0x69, 0x20},

	/////////////////////////////////////////////////////
	//////////////////////	 GAIN	/////////////////////
	/////////////////////////////////////////////////////

	{0xb2, 0x00},
	{0xb3, 0x40},
	{0xb4, 0x40},
	{0xb5, 0x40},

	/////////////////////////////////////////////////////
	////////////////////   DARK SUN   ///////////////////
	/////////////////////////////////////////////////////

	{0xb8, 0x0f},
	{0xb9, 0x23},
	{0xba, 0xff},
	{0xbc, 0x00},
	{0xbd, 0x00},
	{0xbe, 0xff},
	{0xbf, 0x09},

	/////////////////////////////////////////////////////
	//////////////////////	 OUTPUT /////////////////////
	/////////////////////////////////////////////////////

	{0xfe, 0x03},
	{0x01, 0x07},
	{0x02, 0x11},
	{0x03, 0x11},
	{0x06, 0x90},
	{0x11, 0x2b},
	{0x12, 0xd0},
	{0x13, 0x07},
	{0x15, 0x10}, // 10  lanking
	{0x04, 0x20},
	{0x05, 0x00},
	{0x17, 0x01},
	{0x21, 0x01},
	{0x22, 0x02},
	{0x23, 0x01},
	{0x29, 0x01},
	{0x2a, 0x01},
	{0x10, 0x81},
	{0xfe, 0x00},
	{0xf2, 0x00},

	{GC2235_REG_END, 0x00}, /* END MARKER */
};

#ifdef GC2235_ALL_SIZES
static struct regval_list gc2235_win_vga[] = {
	{GC2235_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2235_win_SXGA[] = {
	{GC2235_REG_END, 0x00},	/* END MARKER */
};
#endif

static struct regval_list gc2235_win_2M[] = {
    {GC2235_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list gc2235_stream_on[] = {
	{0xfe, 0x03},
	{0x10, 0x91},  // 93 line_sync_mode
    {0xfe, 0x00},

    {GC2235_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list gc2235_stream_off[] = {
	{0xfe, 0x03},
	{0x10, 0x81},  // 93 line_sync_mode       
    {0xfe, 0x00},

    {GC2235_REG_END, 0x00},     /* END MARKER */
};

static int gc2235_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int gc2235_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc2235_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != GC2235_REG_END) {
		if (vals->reg_num == GC2235_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = gc2235_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#ifdef GC2235_DEBUG_FUNC
static int gc2235_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != GC2235_REG_END) {
		if (vals->reg_num == GC2235_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = gc2235_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

static int gc2235_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int gc2235_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc2235_info *info = container_of(sd, struct gc2235_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;

	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret=gc2235_write_array(sd, gc2235_init_regs);

	msleep(100);

	if (ret < 0)
		return ret;

	return ret;
}

static int gc2235_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = gc2235_read(sd, 0xf0, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != GC2235_CHIP_ID_H)
		return -ENODEV;
	ret = gc2235_read(sd, 0xf1, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L=0x%x \n", v);
	if (v != GC2235_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct gc2235_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc2235_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_GC2235_FMTS ARRAY_SIZE(gc2235_formats)

static struct gc2235_win_size {
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
} gc2235_win_sizes[] = {
	#ifdef GC2235_ALL_SIZES
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= gc2235_win_vga,
	},
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.regs 		= gc2235_win_SXGA,
	},
	#endif
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts        = 0x4e2,
		.framerate  = 25,
		.min_framerate 		= 5,
		.down_steps 		= 20,
		.down_small_step 	= 2,
		.down_large_step	= 10,
		.up_steps 			= 20,
		.up_small_step		= 2,
		.up_large_step		= 10,
		.regs 		= gc2235_win_2M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(gc2235_win_sizes))

static int gc2235_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	u16 gain, exp, vb;

	gain = parm->gain * 64 / 100;
	exp = parm->exp;
	vb = parm->vts - MAX_PREVIEW_HEIGHT;

	//exp
	gc2235_write(sd, 0x03, (exp >> 8));
	gc2235_write(sd, 0x04, exp & 0xff);
	//gain
	if(gain < 256) {
		gc2235_write(sd, 0xb0, 0x40);
		gc2235_write(sd, 0xb1, gain);
	}else {
		gc2235_write(sd, 0xb0, gain / 4);
		gc2235_write(sd, 0xb1, 0xff);
	}
	//vb
	//gc2235_write(sd, 0x07, (vb >> 8));
	//gc2235_write(sd, 0x08, (vb & 0xff));

	//printk("gain=%04x exp=%04x vts=%04x\n", parm->gain/100, exp, parm->vts);
	return 0;
}

static int gc2235_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC2235_FMTS)
		return -EINVAL;


	*code = gc2235_formats[index].mbus_code;
	return 0;
}

static int gc2235_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc2235_format_struct **ret_fmt,
		struct gc2235_win_size **ret_wsize)
{
	int index;
	struct gc2235_win_size *wsize;

	for (index = 0; index < N_GC2235_FMTS; index++)
		if (gc2235_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC2235_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc2235_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc2235_formats + index;

	fmt->field = V4L2_FIELD_NONE;


	for (wsize = gc2235_win_sizes; wsize < gc2235_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc2235_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc2235_formats[index].colorspace;
	return 0;
}

static int gc2235_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc2235_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc2235_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc2235_info *info = container_of(sd, struct gc2235_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct gc2235_format_struct *fmt_s;
	struct gc2235_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret;

	ret = gc2235_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc2235_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs)
	{
		ret = gc2235_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		if (data->aecgc_parm.vts != 0
			|| data->aecgc_parm.exp != 0
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = data->aecgc_parm.vts;
			gc2235_set_aecgc_parm(sd, &aecgc_parm);
		}

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 546;//195;  /*Mbps*/
		data->framerate = wsize->framerate;
		data->min_framerate = wsize->min_framerate;
		data->down_steps = wsize->down_steps;
		data->down_small_step = wsize->down_small_step;
		data->down_large_step = wsize->down_large_step;
		data->up_steps = wsize->up_steps;
		data->up_small_step = wsize->up_small_step;
		data->up_large_step = wsize->up_large_step;
		data->binning = info->binning;

		if (info->stream_on)
			gc2235_s_stream(sd, 1);
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int gc2235_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct gc2235_info *info = container_of(sd, struct gc2235_info, sd);
	printk("gc2235_set_framerate %d\n", framerate);
	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;
	return 0;
}

static int gc2235_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc2235_info *info = container_of(sd, struct gc2235_info, sd);
	int ret = 0;

	if (enable) {
		ret = gc2235_write_array(sd, gc2235_stream_on);
		info->stream_on = 1;
	}
	else {
		ret = gc2235_write_array(sd, gc2235_stream_off);
		info->stream_on = 0;
	}

	#ifdef GC2235_DEBUG_FUNC
	gc2235_read_array(sd,gc2235_init_regs );//debug only
	#endif

	return ret;
}

static int gc2235_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc2235_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc2235_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2235_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc2235_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc2235_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc2235_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc2235_frame_rates[interval->index];
	return 0;
}

static int gc2235_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc2235_win_size *win = &gc2235_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc2235_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc2235_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&gc2235_isp_setting;
		isp_setting.size = ARRAY_SIZE(gc2235_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&gc2235_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int gc2235_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = gc2235_set_aecgc_parm(sd, parm);
		break;
	case V4L2_CID_FRAME_RATE:
		ret = gc2235_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}


static int gc2235_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC2235, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc2235_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc2235_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int gc2235_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2235_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc2235_core_ops = {
	.g_chip_ident = gc2235_g_chip_ident,
	.g_ctrl = gc2235_g_ctrl,
	.s_ctrl = gc2235_s_ctrl,
	.queryctrl = gc2235_queryctrl,
	.reset = gc2235_reset,
	.init = gc2235_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc2235_g_register,
	.s_register = gc2235_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc2235_video_ops = {
	.enum_mbus_fmt = gc2235_enum_mbus_fmt,
	.try_mbus_fmt = gc2235_try_mbus_fmt,
	.s_mbus_fmt = gc2235_s_mbus_fmt,
	.s_stream = gc2235_s_stream,
	.cropcap = gc2235_cropcap,
	.g_crop	= gc2235_g_crop,
	.s_parm = gc2235_s_parm,
	.g_parm = gc2235_g_parm,
	.enum_frameintervals = gc2235_enum_frameintervals,
	.enum_framesizes = gc2235_enum_framesizes,
};

static const struct v4l2_subdev_ops gc2235_ops = {
	.core = &gc2235_core_ops,
	.video = &gc2235_video_ops,
};

static int gc2235_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc2235_info *info;
	int ret;

	info = kzalloc(sizeof(struct gc2235_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc2235_ops);

	/* Make sure it's an gc2235 */
	ret = gc2235_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc2235 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc2235 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int gc2235_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2235_info *info = container_of(sd, struct gc2235_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc2235_id[] = {
	{ "gc2235", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2235_id);

static struct i2c_driver gc2235_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2235",
	},
	.probe		= gc2235_probe,
	.remove		= gc2235_remove,
	.id_table	= gc2235_id,
};

static __init int init_gc2235(void)
{
	return i2c_add_driver(&gc2235_driver);
}

static __exit void exit_gc2235(void)
{
	i2c_del_driver(&gc2235_driver);
}

fs_initcall(init_gc2235);
module_exit(exit_gc2235);

MODULE_DESCRIPTION("A low-level driver for gcoreinc gc2235 sensors");
MODULE_LICENSE("GPL");
