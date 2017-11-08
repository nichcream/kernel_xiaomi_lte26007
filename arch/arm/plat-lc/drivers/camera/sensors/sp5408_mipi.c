
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
#include "sp5408_mipi.h"

//#define SP5408_DEBUG_FUNC


#define SP5408_CHIP_ID_H	(0x54)
#define SP5408_CHIP_ID_L	(0x08)

#define SXGA_WIDTH		1296
#define SXGA_HEIGHT		972
#define MAX_WIDTH		2592
#define MAX_HEIGHT		1944
#define MAX_PREVIEW_WIDTH     SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  SXGA_HEIGHT	


#define SETTING_LOAD
#define SP5408_REG_END		0xff
#define SP5408_REG_DELAY	0xff

struct sp5408_format_struct;

struct sp5408_info {
	struct v4l2_subdev sd;
	struct sp5408_format_struct *fmt;
	struct sp5408_win_size *win;
	int binning;
	int stream_on;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int sp5408_s_stream(struct v4l2_subdev *sd, int enable);

static struct regval_list sp5408_init_regs[] = {

		{0xfd,0x00},
		{0xfd,0x00},
		{0xfd,0x00},
		{0xfd,0x00},
		{0xfd,0x00},
		{0x10,0x04},
		{0x11,0x10},
		{0x12,0x04},
		{0x21,0x00},
		{0x20,0xcf},//data driver
		{0x0e,0x10},
		{0x26,0x00},//pclk v h driver 
		{0x13,0x21},//pclk 0x21
		//{0x15,0x07},//pclk delay

		{0xfd,0x06},
		{0xfb,0x95},//hanlei
		{0xc6,0x80},
		{0xc7,0x80},
		{0xc8,0x80},
		{0xc9,0x80},

		{0xfd,0x01},
		{0x09,0x02},//02
		{0x0a,0x16},//6c 
		{0x0b,0x02},//0x45 hanlei14.3.13
		{0x0c,0x33},
		{0x0d,0x33},
		{0x1c,0x00},//0x01

		{0x0f,0x7f},
		{0x10,0x7e},
		{0x11,0x00},
		{0x12,0x00},
		{0x13,0x2f},
		{0x16,0x00},
		{0x17,0x2f},
		{0x15,0x38},//18
		{0x6b,0x32},
		{0x6c,0x32},//60
		{0x14,0x00},//24
		{0x6f,0x32},//62
		{0x70,0x40},
		{0x71,0x40},//20
		{0x72,0x40},
		{0x73,0x33},//00
		{0x74,0x38},//a0
		{0x75,0x38},//80
		{0x76,0x44},//38
		{0x77,0x38},//40
		{0x7a,0x40},//42
		{0x7e,0x40},//20
		{0x84,0x40},

		{0xfd,0x01},
		{0x19,0x03},
		{0x31,0x04},
		{0x24,0x80},
		{0x03,0x05},
		{0x04,0x78},
		{0x22,0x19},
		{0x1d,0x04},
		{0x20,0xdf},
		{0x25,0x8d},
		{0x27,0xa9},
		{0x1a,0x43},
		{0x28,0x00},
		{0x40,0x05},
		{0x3f,0x03},//0x00
		{0x01,0x01},
		{0xfd,0x02},
		{0x37,0x00},
		{0x38,0x04},
		{0x3b,0x00},
		{0x3c,0x02},
		{0x39,0x03},
		{0x3a,0xcc},
		{0x3d,0x02},
		{0x3e,0x88},
		{0x33,0x0f},//hanlei
		{0x3f,0x01},
		{0xfd,0x03},
		{0x04,0x11},
		{0xaa,0x46},
		{0xab,0x01},
		{0xac,0xf5},
		{0xad,0x00},

		//add hzl
		{0xfd,0x03},
		{0x03,0x08},
		{0x04,0x08},
		{0x24,0x50},
		{0x01,0x01},
		{SP5408_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list sp5408_win_SXGA[] = {
		//1296x972     
		{0xfd , 0x00},
		{0x10 , 0x1b},
		{0x11 , 0x10},
		{0xfd , 0x01},
		{0x19 , 0x03},
		{0x31 , 0x04},
		{0x01 , 0x01},
		{0xfd , 0x02},
		{0x37 , 0x00},
		{0x38 , 0x04},
		{0x3b , 0x00},
		{0x3c , 0x02},
		{0x39 , 0x03},
		{0x3a , 0xcc},
		{0x3d , 0x02},
		{0x3e , 0x88},
		{0xfd , 0x05},
		{0x02 , 0x10},
		{0x03 , 0x05},
		{0x04 , 0xcc},
		{0x05 , 0x03},	
		{SP5408_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list sp5408_win_5M[] = {
		//2592x1944   
		{0xfd , 0x00},
		{0x10 , 0x1b},
		{0x11 , 0x00},
		{0xfd , 0x01},
		{0x19 , 0x00},
		{0x31 , 0x00},
		{0x01 , 0x01},
		{0xfd , 0x02},
		{0x37 , 0x00},
		{0x38 , 0x08},
		{0x3b , 0x00},
		{0x3c , 0x04},
		{0x39 , 0x07},
		{0x3a , 0x98},
		{0x3d , 0x05},
		{0x3e , 0x10},
		{0xfd , 0x05},
		{0x02 , 0x20},
		{0x03 , 0x0a},
		{0x04 , 0x98},
		{0x05 , 0x07},	
		{SP5408_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list sp5408_stream_on[] = {
		{0xfd, 0x05},
		{0x00, 0xc1},  // 93 
		{0xfd, 0x00},
		{SP5408_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list sp5408_stream_off[] = {
		{0xfd, 0x05},
		{0x00, 0x00},  // 93 line_sync_mode       
		{0xfd, 0x00},
		{SP5408_REG_END, 0x00},     /* END MARKER */
};

static int sp5408_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int sp5408_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sp5408_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != SP5408_REG_END) {
		if (vals->reg_num == SP5408_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = sp5408_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#ifdef SP5408_DEBUG_FUNC
static int sp5408_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != SP5408_REG_END) {
		if (vals->reg_num == SP5408_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = sp5408_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

static int sp5408_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int sp5408_init(struct v4l2_subdev *sd, u32 val)
{
	struct sp5408_info *info = container_of(sd, struct sp5408_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;

	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret=sp5408_write_array(sd, sp5408_init_regs);

	msleep(100);

	if (ret < 0)
		return ret;

	return ret;
}

static int sp5408_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = sp5408_read(sd, 0x02, &v);
	if (ret < 0)
		return ret;
	printk("SP5408_CHIP_ID_H=0x%x ", v);
	if (v != SP5408_CHIP_ID_H)
		return -ENODEV;
	ret = sp5408_read(sd, 0x03, &v);
	if (ret < 0)
		return ret;
	printk("SP5408_CHIP_ID_L=0x%x \n", v);
	if (v != SP5408_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct sp5408_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} sp5408_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_SP5408_FMTS ARRAY_SIZE(sp5408_formats)

static struct sp5408_win_size {
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
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
} sp5408_win_sizes[] = {	
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.vts        = 0x510,
		.framerate  = 25,		
		.min_framerate 		= 5,
		.down_steps 		= 20,
		.down_small_step 	= 2,
		.down_large_step	= 10,
		.up_steps 			= 20,
		.up_small_step		= 2,
		.up_large_step		= 10,
		.max_gain_dyn_frm = 0x60,//60
		.min_gain_dyn_frm = 0x10,//10
		.max_gain_fix_frm = 0x60,//60
		.min_gain_fix_frm = 0x10,//10
		.regs 		= sp5408_win_SXGA,
	},
	
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts        = 0x510,//0xca8,
		.framerate  = 10,	
		.max_gain_dyn_frm = 0x60,//60
		.min_gain_dyn_frm = 0x10,//10
		.max_gain_fix_frm = 0x60,//60
		.min_gain_fix_frm = 0x10,//10
		.regs 		= sp5408_win_5M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(sp5408_win_sizes))

static int sp5408_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	
	return 0;
}

static int sp5408_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_SP5408_FMTS)
		return -EINVAL;


	*code = sp5408_formats[index].mbus_code;
	return 0;
}

static int sp5408_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct sp5408_format_struct **ret_fmt,
		struct sp5408_win_size **ret_wsize)
{
	int index;
	struct sp5408_win_size *wsize;

	for (index = 0; index < N_SP5408_FMTS; index++)
		if (sp5408_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_SP5408_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = sp5408_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = sp5408_formats + index;

	fmt->field = V4L2_FIELD_NONE;


	for (wsize = sp5408_win_sizes; wsize < sp5408_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= sp5408_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = sp5408_formats[index].colorspace;
	return 0;
}

static int sp5408_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return sp5408_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int sp5408_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct sp5408_info *info = container_of(sd, struct sp5408_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct sp5408_format_struct *fmt_s;
	struct sp5408_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret;

	ret = sp5408_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = sp5408_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}
	
	if ((info->win != wsize) && wsize->regs)
	{
		ret = sp5408_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		if (data->aecgc_parm.vts != 0
			|| data->aecgc_parm.exp != 0
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = data->aecgc_parm.vts;
			sp5408_set_aecgc_parm(sd, &aecgc_parm);
		}

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 624;//195;  /*Mbps*/
		data->framerate = wsize->framerate;
		data->min_framerate = wsize->min_framerate;
		data->down_steps = wsize->down_steps;
		data->down_small_step = wsize->down_small_step;
		data->down_large_step = wsize->down_large_step;
		data->up_steps = wsize->up_steps;
		data->up_small_step = wsize->up_small_step;
		data->up_large_step = wsize->up_large_step;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;
		data->binning = info->binning;

		if (info->stream_on)
			sp5408_s_stream(sd, 1);
	}
	data->reg_num = 0;

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int sp5408_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct sp5408_info *info = container_of(sd, struct sp5408_info, sd);
	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;
	return 0;
}

static int sp5408_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sp5408_info *info = container_of(sd, struct sp5408_info, sd);
	int ret = 0;
	
	if (enable) {
		ret = sp5408_write_array(sd, sp5408_stream_on);
		info->stream_on = 1;
	}
	else {
		ret = sp5408_write_array(sd, sp5408_stream_off);
		info->stream_on = 0;
	}

	#ifdef SP5408_DEBUG_FUNC
	sp5408_read_array(sd,sp5408_init_regs );//debug only
	#endif

	return ret;
}

static int sp5408_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int sp5408_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int sp5408_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp5408_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp5408_frame_rates[] = { 30, 15, 10, 5, 1 };

static int sp5408_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(sp5408_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = sp5408_frame_rates[interval->index];
	return 0;
}

static int sp5408_enum_framesizes(struct v4l2_subdev *sd,
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
		struct sp5408_win_size *win = &sp5408_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int sp5408_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int sp5408_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {

		case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&SP5408_isp_setting;
		isp_setting.size = ARRAY_SIZE(SP5408_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&SP5408_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

static int sp5408_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = sp5408_set_aecgc_parm(sd, parm);
		break;
	case V4L2_CID_FRAME_RATE:
		ret = sp5408_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}


static int sp5408_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SP5408, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sp5408_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sp5408_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int sp5408_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sp5408_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops sp5408_core_ops = {
	.g_chip_ident = sp5408_g_chip_ident,
	.g_ctrl = sp5408_g_ctrl,
	.s_ctrl = sp5408_s_ctrl,
	.queryctrl = sp5408_queryctrl,
	.reset = sp5408_reset,
	.init = sp5408_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = sp5408_g_register,
	.s_register = sp5408_s_register,
#endif
};

static const struct v4l2_subdev_video_ops sp5408_video_ops = {
	.enum_mbus_fmt = sp5408_enum_mbus_fmt,
	.try_mbus_fmt = sp5408_try_mbus_fmt,
	.s_mbus_fmt = sp5408_s_mbus_fmt,
	.s_stream = sp5408_s_stream,
	.cropcap = sp5408_cropcap,
	.g_crop	= sp5408_g_crop,
	.s_parm = sp5408_s_parm,
	.g_parm = sp5408_g_parm,
	.enum_frameintervals = sp5408_enum_frameintervals,
	.enum_framesizes = sp5408_enum_framesizes,
};

static const struct v4l2_subdev_ops sp5408_ops = {
	.core = &sp5408_core_ops,
	.video = &sp5408_video_ops,
};

static int sp5408_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sp5408_info *info;
	int ret;

	info = kzalloc(sizeof(struct sp5408_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &sp5408_ops);

	/* Make sure it's an sp5408 */
	ret = sp5408_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an sp5408 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "sp5408 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int sp5408_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp5408_info *info = container_of(sd, struct sp5408_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id sp5408_id[] = {
	{ "sp5408-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sp5408_id);

static struct i2c_driver sp5408_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sp5408-mipi-raw",
	},
	.probe		= sp5408_probe,
	.remove		= sp5408_remove,
	.id_table	= sp5408_id,
};

static __init int init_sp5408(void)
{
	return i2c_add_driver(&sp5408_driver);
}

static __exit void exit_sp5408(void)
{
	i2c_del_driver(&sp5408_driver);
}

fs_initcall(init_sp5408);
module_exit(exit_sp5408);

MODULE_DESCRIPTION("A low-level driver for gcoreinc sp5408 sensors");
MODULE_LICENSE("GPL");
