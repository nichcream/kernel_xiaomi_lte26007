
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
#include "gc5004.h"

//#define GC5004_DEBUG_FUNC
//#define GC5004_ALL_SIZES

#define GC5004_CHIP_ID_H	(0x50)
#define GC5004_CHIP_ID_L	(0x04)

#define SXGA_WIDTH		1296
#define SXGA_HEIGHT		972
#define MAX_WIDTH		2592
#define MAX_HEIGHT		1944
#define MAX_PREVIEW_WIDTH     SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  SXGA_HEIGHT	


#define SETTING_LOAD
#define GC5004_REG_END		0xff
#define GC5004_REG_DELAY	0xff

struct gc5004_format_struct;

struct gc5004_info {
	struct v4l2_subdev sd;
	struct gc5004_format_struct *fmt;
	struct gc5004_win_size *win;
	int binning;
	int stream_on;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static int gc5004_s_stream(struct v4l2_subdev *sd, int enable);

static struct regval_list gc5004_init_regs[] = {
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xf2, 0x00}, //sync_pad_io_ebi
	{0xf6, 0x00}, //up down
	{0xfc, 0x06},
	{0xf7, 0x1d}, //pll enable
	{0xf8, 0x84}, //Pll mode 2
	{0xf9, 0xfe}, //[0] pll enable  change at 17:37 04/19
	{0xfa, 0x00}, //div
	{0xfe, 0x00},

	/////////////////////////////////////////////////////
	////////////////   ANALOG & CISCTL   ////////////////
	/////////////////////////////////////////////////////
	{0x00, 0x00}, //10/[4]rowskip_skip_sh
	{0x03, 0x07}, //15fps
	{0x04, 0x2a}, 
	{0x05, 0x01}, //HB
	{0x06, 0xfa}, 
	{0x07, 0x00}, //VB
	{0x08, 0x1c},
	{0x0a, 0x02}, //02//row start
	{0x0c, 0x00}, //0c//col start
	{0x0d, 0x07}, 
	{0x0e, 0xa8}, 
	{0x0f, 0x0a}, //Window setting
	{0x10, 0x50}, //50 
	{0x17, 0x15}, //01//14//[0]mirror [1]flip
	{0x18, 0x02}, //sdark off
	{0x19, 0x0c}, 
	{0x1a, 0x13}, 
	{0x1b, 0x48}, 
	{0x1c, 0x05}, 
	{0x1e, 0xb8},
	{0x1f, 0x78}, 
	{0x20, 0xc5}, //03/[7:6]ref_r [3:1]comv_r 
	{0x21, 0x4f}, //7f
	{0x22, 0x82}, //b2 
	{0x23, 0x43}, //f1/[7:3]opa_r [1:0]sRef
	{0x24, 0x2f}, //PAD drive 
	{0x2b, 0x01}, 
	{0x2c, 0x68}, //[6:4]rsgh_r 

	/////////////////////////////////////////////////////
	//////////////////////   ISP   //////////////////////
	/////////////////////////////////////////////////////
	{0x86, 0x0a},
	{0x8a, 0x83},
	{0x8c, 0x10},
	{0x8d, 0x01},
	{0x90, 0x01},
	{0x92, 0x00}, //00/crop win y
	{0x94, 0x0d}, //04/crop win x
	{0x95, 0x07}, //crop win height
	{0x96, 0x98},
	{0x97, 0x0a}, //crop win width
	{0x98, 0x20},

	/////////////////////////////////////////////////////
	//////////////////////   BLK   //////////////////////
	/////////////////////////////////////////////////////
	{0x40, 0x82},
	{0x41, 0x00},//38

	{0x50, 0x00},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x00},
	{0x56, 0x00},
	{0x57, 0x00},
	{0x58, 0x00},
	{0x59, 0x00},
	{0x5a, 0x00},
	{0x5b, 0x00},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0xd0, 0x00},
	{0xd1, 0x00},
	{0xd2, 0x00},
	{0xd3, 0x00},
	{0xd4, 0x00},
	{0xd5, 0x00},
	{0xd6, 0x00},
	{0xd7, 0x00},
	{0xd8, 0x00},
	{0xd9, 0x00},
	{0xda, 0x00},
	{0xdb, 0x00},
	{0xdc, 0x00},
	{0xdd, 0x00},
	{0xde, 0x00},
	{0xdf, 0x00},

	{0x70, 0x00},
	{0x71, 0x00},
	{0x72, 0x00},
	{0x73, 0x00},
	{0x74, 0x20},
	{0x75, 0x20},
	{0x76, 0x20},
	{0x77, 0x20},

	/////////////////////////////////////////////////////
	//////////////////////   GAIN   /////////////////////
	/////////////////////////////////////////////////////
	{0xb0, 0x50},
	{0xb1, 0x01},
	{0xb2, 0x02},
	{0xb3, 0x40},
	{0xb4, 0x40},
	{0xb5, 0x40},

	/////////////////////////////////////////////////////
	//////////////////////   SCALER   /////////////////////
	/////////////////////////////////////////////////////
	{0xfe, 0x00},
	{0x18, 0x02},
	{0x80, 0x08},
	{0x84, 0x03},//scaler CFA
	{0x87, 0x12},
	{0x95, 0x07},
	{0x96, 0x98},
	{0x97, 0x0a},
	{0x98, 0x20},
	{0xfe, 0x02},
	{0x86, 0x00},

	/////////////////////////////////////////////////////
	//////////////////////   MIPI   /////////////////////
	/////////////////////////////////////////////////////
	{0xfe, 0x03},
	{0x01, 0x07},
	{0x02, 0x33},
	{0x03, 0x93},
	{0x04, 0x80},
	{0x05, 0x02},
	{0x06, 0x80},
	{0x10, 0x81},
	{0x11, 0x2b},
	{0x12, 0xa8},
	{0x13, 0x0c},
	{0x15, 0x10},
	{0x17, 0xb0},
	{0x18, 0x00},
	{0x19, 0x00},
	{0x1a, 0x00},
	{0x1d, 0x00},
	{0x42, 0x20},
	{0x43, 0x0a},

	{0x21, 0x01},
	{0x22, 0x02},
	{0x23, 0x01},
	{0x29, 0x04},
	{0x2a, 0x03},
	{0xfe, 0x00},
	{GC5004_REG_END, 0x00}, /* END MARKER */
};

#ifdef GC5004_ALL_SIZES
static struct regval_list gc5004_win_vga[] = {
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
	{GC5004_REG_END, 0x00},	/* END MARKER */
};
#endif

static struct regval_list gc5004_win_SXGA[] = {
	{0x18, 0x42},//skip on
	{0x80, 0x18},//08//scaler on
	{0x89, 0x03},//03
	{0x8b, 0x61},//ad
	{0x40, 0x22},

	{0x94, 0x0d},
	{0x95, 0x03},
	{0x96, 0xcc},
	{0x97, 0x05},
	{0x98, 0x10},

	{0xfe, 0x03},
	{0x04, 0x40},
	{0x05, 0x01},
	{0x12, 0x54},
	{0x13, 0x06},
	{0x42, 0x10},
	{0x43, 0x05},
	{0xfe, 0x00},
	{GC5004_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list gc5004_win_5M[] = {
	{0x18, 0x02},//skip off
	{0x80, 0x10},//scaler off
	{0x89, 0x03},//13
	{0x8b, 0x61},//e9
	{0x40, 0x82},

	{0x94, 0x0d},
	{0x95, 0x07},
	{0x96, 0x98},
	{0x97, 0x0a},
	{0x98, 0x20},

	{0xfe, 0x03},
	{0x04, 0x80},
	{0x05, 0x02},
	{0x12, 0xa8},
	{0x13, 0x0c},
	{0x42, 0x20},
	{0x43, 0x0a},
	{0xfe, 0x00},
    {GC5004_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list gc5004_stream_on[] = {
	{0xfe, 0x03},
	{0x10, 0x93},  // 93 
    {0xfe, 0x00},

    {GC5004_REG_END, 0x00},     /* END MARKER */
};

static struct regval_list gc5004_stream_off[] = {
	{0xfe, 0x03},
	{0x10, 0x81},  // 93 line_sync_mode       
   	{0xfe, 0x00},

    {GC5004_REG_END, 0x00},     /* END MARKER */
};

static int gc5004_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int gc5004_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc5004_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != GC5004_REG_END) {
		if (vals->reg_num == GC5004_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = gc5004_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#ifdef GC5004_DEBUG_FUNC
static int gc5004_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != GC5004_REG_END) {
		if (vals->reg_num == GC5004_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = gc5004_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

static int gc5004_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int gc5004_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc5004_info *info = container_of(sd, struct gc5004_info, sd);
	int ret=0;

	info->fmt = NULL;
	info->win = NULL;

	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret=gc5004_write_array(sd, gc5004_init_regs);

	msleep(100);

	if (ret < 0)
		return ret;

	return ret;
}

static int gc5004_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = gc5004_read(sd, 0xf0, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != GC5004_CHIP_ID_H)
		return -ENODEV;
	ret = gc5004_read(sd, 0xf1, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L=0x%x \n", v);
	if (v != GC5004_CHIP_ID_L)
		return -ENODEV;

	return 0;
}

static struct gc5004_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc5004_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_GC5004_FMTS ARRAY_SIZE(gc5004_formats)

static struct gc5004_win_size {
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
} gc5004_win_sizes[] = {	
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
		.max_gain_dyn_frm = 0x60,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x60,
		.min_gain_fix_frm = 0x10,
		.regs 		= gc5004_win_SXGA,
	},
	
	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts        = 0x510,//0xca8,
		.framerate  = 10,	
		.max_gain_dyn_frm = 0x60,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x60,
		.min_gain_fix_frm = 0x10,
		.regs 		= gc5004_win_5M,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(gc5004_win_sizes))

static int gc5004_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	u16 gain, exp, vb,temp;
	gain = parm->gain * 64 / 100;
	exp = parm->exp;
	vb = parm->vts - MAX_PREVIEW_HEIGHT;
	
	///////exp//////////
	gc5004_write(sd, 0x03, (exp >> 8)&0x1f);
	gc5004_write(sd, 0x04, exp & 0xff);
	 ///////gain////////////
	gc5004_write(sd, 0xb1, 0x01);
	gc5004_write(sd, 0xb2, 0x00);
		//gain
	if(gain < 64)
	{
		gc5004_write(sd, 0xb1, 0x01);
		gc5004_write(sd, 0xb2, 0x00);
		gc5004_write(sd, 0xb6, 0x00);
		
	}
	else if((64<=gain) &&(gain <90 ))
	{
		gc5004_write(sd, 0xb6, 0x00);
		temp = gain;
		gc5004_write(sd, 0xb1, temp>>6);
		gc5004_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((90<=gain) &&(gain <128 ))
	{
		gc5004_write(sd, 0xb6, 0x01);
		temp = 64*gain/90;
		gc5004_write(sd, 0xb1, temp>>6);
		gc5004_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((128<=gain) &&(gain <178 ))
	{
		gc5004_write(sd, 0xb6, 0x02);
		temp = 64*gain/128;
		gc5004_write(sd, 0xb1, temp>>6);
		gc5004_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if((178<=gain) &&(gain <247 ))
	{
		gc5004_write(sd, 0xb6, 0x03);
		temp = 64*gain/178;
		gc5004_write(sd, 0xb1, temp>>6);
		gc5004_write(sd, 0xb2, (temp<<2)&0xfc);
	}
	else if(247<=gain) 
	{
		gc5004_write(sd, 0xb6, 0x04);
		temp = 64*gain/247;
		gc5004_write(sd, 0xb1, temp>>6);
		gc5004_write(sd, 0xb2, (temp<<2)&0xfc);
	}
			
	//vb ///// don't change
	//gc5004_write(sd, 0x07, (vb >> 8));
	//gc5004_write(sd, 0x08, (vb & 0xff));
	
	//printk("gain=%04x exp=%04x vts=%04x\n", parm->gain/100, exp, parm->vts);
	
	return 0;
}

static int gc5004_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC5004_FMTS)
		return -EINVAL;


	*code = gc5004_formats[index].mbus_code;
	return 0;
}

static int gc5004_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc5004_format_struct **ret_fmt,
		struct gc5004_win_size **ret_wsize)
{
	int index;
	struct gc5004_win_size *wsize;

	for (index = 0; index < N_GC5004_FMTS; index++)
		if (gc5004_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC5004_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc5004_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc5004_formats + index;

	fmt->field = V4L2_FIELD_NONE;


	for (wsize = gc5004_win_sizes; wsize < gc5004_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc5004_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc5004_formats[index].colorspace;
	return 0;
}

static int gc5004_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc5004_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc5004_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc5004_info *info = container_of(sd, struct gc5004_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct gc5004_format_struct *fmt_s;
	struct gc5004_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret;

	ret = gc5004_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc5004_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}
	
	if ((info->win != wsize) && wsize->regs)
	{
		ret = gc5004_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		if (data->aecgc_parm.vts != 0
			|| data->aecgc_parm.exp != 0
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = data->aecgc_parm.vts;
			gc5004_set_aecgc_parm(sd, &aecgc_parm);
		}

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 390;//195;  /*Mbps*/
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
			gc5004_s_stream(sd, 1);
	}
	data->reg_num = 0;

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int gc5004_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct gc5004_info *info = container_of(sd, struct gc5004_info, sd);
	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;
	return 0;
}

static int gc5004_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc5004_info *info = container_of(sd, struct gc5004_info, sd);
	int ret = 0;
	
	if (enable) {
		ret = gc5004_write_array(sd, gc5004_stream_on);
		info->stream_on = 1;
	}
	else {
		ret = gc5004_write_array(sd, gc5004_stream_off);
		info->stream_on = 0;
	}

	#ifdef GC5004_DEBUG_FUNC
	gc5004_read_array(sd,gc5004_init_regs );//debug only
	#endif

	return ret;
}

static int gc5004_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc5004_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc5004_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc5004_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc5004_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc5004_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc5004_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc5004_frame_rates[interval->index];
	return 0;
}

static int gc5004_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc5004_win_size *win = &gc5004_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc5004_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc5004_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&gc5004_isp_setting;
		isp_setting.size = ARRAY_SIZE(gc5004_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&gc5004_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int gc5004_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = gc5004_set_aecgc_parm(sd, parm);
		break;
	case V4L2_CID_FRAME_RATE:
		ret = gc5004_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}


static int gc5004_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC5004, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc5004_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc5004_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int gc5004_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc5004_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc5004_core_ops = {
	.g_chip_ident = gc5004_g_chip_ident,
	.g_ctrl = gc5004_g_ctrl,
	.s_ctrl = gc5004_s_ctrl,
	.queryctrl = gc5004_queryctrl,
	.reset = gc5004_reset,
	.init = gc5004_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc5004_g_register,
	.s_register = gc5004_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc5004_video_ops = {
	.enum_mbus_fmt = gc5004_enum_mbus_fmt,
	.try_mbus_fmt = gc5004_try_mbus_fmt,
	.s_mbus_fmt = gc5004_s_mbus_fmt,
	.s_stream = gc5004_s_stream,
	.cropcap = gc5004_cropcap,
	.g_crop	= gc5004_g_crop,
	.s_parm = gc5004_s_parm,
	.g_parm = gc5004_g_parm,
	.enum_frameintervals = gc5004_enum_frameintervals,
	.enum_framesizes = gc5004_enum_framesizes,
};

static const struct v4l2_subdev_ops gc5004_ops = {
	.core = &gc5004_core_ops,
	.video = &gc5004_video_ops,
};

static int gc5004_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc5004_info *info;
	int ret;

	info = kzalloc(sizeof(struct gc5004_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc5004_ops);

	/* Make sure it's an gc5004 */
	ret = gc5004_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc5004 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc5004 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int gc5004_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc5004_info *info = container_of(sd, struct gc5004_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc5004_id[] = {
	{ "gc5004", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc5004_id);

static struct i2c_driver gc5004_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc5004",
	},
	.probe		= gc5004_probe,
	.remove		= gc5004_remove,
	.id_table	= gc5004_id,
};

static __init int init_gc5004(void)
{
	return i2c_add_driver(&gc5004_driver);
}

static __exit void exit_gc5004(void)
{
	i2c_del_driver(&gc5004_driver);
}

fs_initcall(init_gc5004);
module_exit(exit_gc5004);

MODULE_DESCRIPTION("A low-level driver for gcoreinc gc5004 sensors");
MODULE_LICENSE("GPL");
