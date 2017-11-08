
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
#include "sp5409_mipi.h"

//#define SP5409_DEBUG_FUNC


#define SP5409_CHIP_ID_H	(0x54)
#define SP5409_CHIP_ID_L	(0x09)

//#define SXGA_WIDTH		1296
//#define SXGA_HEIGHT		972
#define MAX_WIDTH		2592
#define MAX_HEIGHT		1944
#define MAX_PREVIEW_WIDTH     MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT	


#define SETTING_LOAD
#define SP5409_REG_END		0xff
#define SP5409_REG_DELAY	0xff

struct sp5409_format_struct;

struct sp5409_info {
	struct v4l2_subdev sd;
	struct sp5409_format_struct *fmt;
	struct sp5409_win_size *win;
	int binning;
	int stream_on;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int sp5409_s_stream(struct v4l2_subdev *sd, int enable);

static struct regval_list sp5409_init_regs[] = {

	{0xfd,0x00},
	{0xfd,0x00},
	{0xfd,0x00},
	{0xfd,0x00},
	{0xfd,0x00},
	//1309
	{0xfd,0x00},
	{0x1b,0x00},
	{0x30,0x15},
	{0x2f,0x14},
	{0x34,0x00},
	{0xfd,0x01},
	{0x06,0x10},
	{0x03,0x01},
	{0x04,0x0f},
	{0x0a,0x80},
	{0x24,0x50},
	{0x01,0x01},
	{0x21,0x37},
	{0x25,0xf4},
	{0x26,0x0c},
	{0x2c,0xa0}, //b0
	{0x8a,0x55},
	{0x8b,0x55},
	{0x58,0x3a},
	{0x75,0x4a},
	{0x77,0x6f},
	{0x78,0xef},
	{0x84,0x0f},
	{0x85,0x02},
	{0x11,0x68},
	{0x51,0x4a},
	{0x52,0x4a},
	{0x5d,0x00},
	{0x5e,0x00},
	{0x68,0x52},
	{0x72,0x54},

	//{0x47,0x1b},
	//{0x29,0x20},

	{0xfb,0x00}, //25
	{0xf0,0x04},
	{0xf1,0x04},
	{0xf2,0x04},
	{0xf3,0x04},
	{0xfd,0x01},
	{0x0f,0x01},
	{0x0e,0x01},
	{0xb3,0x00},
	{0xb1,0x01},
	//{0xa4,0x6d},
	{0xb6,0xc0},
	{0x9d,0x25},
	{0x97,0x08},
	{0xc4,0x28},
	{0xc1,0x09},
	{SP5409_REG_END, 0x00}, /* END MARKER */
};

/*
static struct regval_list sp5409_win_SXGA[] = {
	//1296x972
	{SP5409_REG_END, 0x00},	
};
*/

static struct regval_list sp5409_win_5M[] = {
	//2592x1944
	{SP5409_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list sp5409_stream_on[] = {
	{0xfd, 0x01},
	{0xa4, 0x6d},
	{0xfd, 0x00},
	{SP5409_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list sp5409_stream_off[] = {
	{0xfd, 0x01},
	{0xa4, 0x2d},
	{0xfd, 0x00},
	{SP5409_REG_END, 0x00},     /* END MARKER */
};

static int sp5409_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int sp5409_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sp5409_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != SP5409_REG_END) {
		if (vals->reg_num == SP5409_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = sp5409_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#ifdef SP5409_DEBUG_FUNC
static int sp5409_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != SP5409_REG_END) {
		if (vals->reg_num == SP5409_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = sp5409_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

static int sp5409_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int sp5409_init(struct v4l2_subdev *sd, u32 val)
{
	struct sp5409_info *info = container_of(sd, struct sp5409_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;

	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret=sp5409_write_array(sd, sp5409_init_regs);

	msleep(100);

	if (ret < 0)
		return ret;

	return ret;
}

static int sp5409_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;
	mdelay(50);

	ret = sp5409_read(sd, 0x02, &v);
	if (ret < 0)
		return ret;
	printk("SP5409_CHIP_ID_H=0x%x ", v);
	if (v != SP5409_CHIP_ID_H)
		return -ENODEV;
	ret = sp5409_read(sd, 0x03, &v);
	if (ret < 0)
		return ret;
	printk("SP5409_CHIP_ID_L=0x%x \n", v);
	if (v != SP5409_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct sp5409_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} sp5409_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_SP5409_FMTS ARRAY_SIZE(sp5409_formats)

static struct sp5409_win_size {
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
} sp5409_win_sizes[] = {	
	/* SXGA 
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
		.max_gain_dyn_frm = 0xa0,//60
		.min_gain_dyn_frm = 0x30,//10
		.max_gain_fix_frm = 0xa0,//60
		.min_gain_fix_frm = 0x30,//10
		.regs 		= sp5409_win_SXGA,
	},*/
	
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts        = 0x510,//0xca8,
		.framerate  = 10,	
		.max_gain_dyn_frm = 0xa0,//60
		.min_gain_dyn_frm = 0x10,//10
		.max_gain_fix_frm = 0xa0,//60
		.min_gain_fix_frm = 0x10,//10
		.regs 		= sp5409_win_5M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(sp5409_win_sizes))

static int sp5409_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	
	return 0;
}

static int sp5409_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_SP5409_FMTS)
		return -EINVAL;


	*code = sp5409_formats[index].mbus_code;
	return 0;
}

static int sp5409_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct sp5409_format_struct **ret_fmt,
		struct sp5409_win_size **ret_wsize)
{
	int index;
	struct sp5409_win_size *wsize;

	for (index = 0; index < N_SP5409_FMTS; index++)
		if (sp5409_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_SP5409_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = sp5409_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = sp5409_formats + index;

	fmt->field = V4L2_FIELD_NONE;


	for (wsize = sp5409_win_sizes; wsize < sp5409_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= sp5409_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = sp5409_formats[index].colorspace;
	return 0;
}

static int sp5409_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return sp5409_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int sp5409_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct sp5409_info *info = container_of(sd, struct sp5409_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct sp5409_format_struct *fmt_s;
	struct sp5409_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret;

	ret = sp5409_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = sp5409_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs)
	{
		ret = sp5409_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		if (data->aecgc_parm.vts != 0
			|| data->aecgc_parm.exp != 0
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = data->aecgc_parm.vts;
			sp5409_set_aecgc_parm(sd, &aecgc_parm);
		}

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 195;//195;  /*Mbps*/
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
			sp5409_s_stream(sd, 1);
	}
	data->reg_num = 0;

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int sp5409_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct sp5409_info *info = container_of(sd, struct sp5409_info, sd);
	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;
	return 0;
}

static int sp5409_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sp5409_info *info = container_of(sd, struct sp5409_info, sd);
	int ret = 0;
	
	if (enable) {
		ret = sp5409_write_array(sd, sp5409_stream_on);
		info->stream_on = 1;
	}
	else {
		ret = sp5409_write_array(sd, sp5409_stream_off);
		info->stream_on = 0;
	}

	#ifdef SP5409_DEBUG_FUNC
	sp5409_read_array(sd,sp5409_init_regs );//debug only
	#endif

	return ret;
}

static int sp5409_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int sp5409_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int sp5409_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp5409_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sp5409_frame_rates[] = { 30, 15, 10, 5, 1 };

static int sp5409_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(sp5409_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = sp5409_frame_rates[interval->index];
	return 0;
}

static int sp5409_enum_framesizes(struct v4l2_subdev *sd,
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
		struct sp5409_win_size *win = &sp5409_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int sp5409_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int sp5409_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {

		case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&SP5409_isp_setting;
		isp_setting.size = ARRAY_SIZE(SP5409_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&SP5409_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;

	}

	return ret;
}

static int sp5409_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = sp5409_set_aecgc_parm(sd, parm);
		break;
	case V4L2_CID_FRAME_RATE:
		ret = sp5409_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}


static int sp5409_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SP5409, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sp5409_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sp5409_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int sp5409_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sp5409_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops sp5409_core_ops = {
	.g_chip_ident = sp5409_g_chip_ident,
	.g_ctrl = sp5409_g_ctrl,
	.s_ctrl = sp5409_s_ctrl,
	.queryctrl = sp5409_queryctrl,
	.reset = sp5409_reset,
	.init = sp5409_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = sp5409_g_register,
	.s_register = sp5409_s_register,
#endif
};

static const struct v4l2_subdev_video_ops sp5409_video_ops = {
	.enum_mbus_fmt = sp5409_enum_mbus_fmt,
	.try_mbus_fmt = sp5409_try_mbus_fmt,
	.s_mbus_fmt = sp5409_s_mbus_fmt,
	.s_stream = sp5409_s_stream,
	.cropcap = sp5409_cropcap,
	.g_crop	= sp5409_g_crop,
	.s_parm = sp5409_s_parm,
	.g_parm = sp5409_g_parm,
	.enum_frameintervals = sp5409_enum_frameintervals,
	.enum_framesizes = sp5409_enum_framesizes,
};

static const struct v4l2_subdev_ops sp5409_ops = {
	.core = &sp5409_core_ops,
	.video = &sp5409_video_ops,
};

static int sp5409_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sp5409_info *info;
	int ret;

	info = kzalloc(sizeof(struct sp5409_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &sp5409_ops);

	/* Make sure it's an sp5409 */
	ret = sp5409_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an sp5409 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "sp5409 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int sp5409_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp5409_info *info = container_of(sd, struct sp5409_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id sp5409_id[] = {
	{ "sp5409-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sp5409_id);

static struct i2c_driver sp5409_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sp5409-mipi-raw",
	},
	.probe		= sp5409_probe,
	.remove		= sp5409_remove,
	.id_table	= sp5409_id,
};

static __init int init_sp5409(void)
{
	return i2c_add_driver(&sp5409_driver);
}

static __exit void exit_sp5409(void)
{
	i2c_del_driver(&sp5409_driver);
}

fs_initcall(init_sp5409);
module_exit(exit_sp5409);

MODULE_DESCRIPTION("A low-level driver for gcoreinc sp5409 sensors");
MODULE_LICENSE("GPL");
