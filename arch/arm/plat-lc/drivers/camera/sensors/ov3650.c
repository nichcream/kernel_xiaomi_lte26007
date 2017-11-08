
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
#include "ov3650.h"

#define OV3650_CHIP_ID_H	(0x36)
#define OV3650_CHIP_ID_L	(0x50)


/*preview banding step 0xf4 for 50hz light source 
   capture banding step 0xf3    for 50hz light source
if enable dynamic framerate function,
backend ISP should set  AEC vts value to  0x350c/0x350d not 0x380e/0x380f as usual

 */

#define SXGA_WIDTH		1024
#define SXGA_HEIGHT		768
#define MAX_WIDTH		2048
#define MAX_HEIGHT		1536
#define MAX_PREVIEW_WIDTH SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT SXGA_HEIGHT


#define OV3650_REG_END		0xffff
#define OV3650_REG_DELAY	0xfffe

struct ov3650_format_struct;
struct ov3650_info {
	struct v4l2_subdev sd;
	struct ov3650_format_struct *fmt;
	struct ov3650_win_size *win;
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
	unsigned int customer_id;
	unsigned int module_integrator_id;
	unsigned int lens_id;
	unsigned int rg_ratio;
	unsigned int bg_ratio;
	unsigned int user_data[5];
};

static struct regval_list ov3650_init_regs[] = {
	{0x3103,0x03},
	{0x3008,0x82},
	{0xfffe,0x05},
	{0x3008,0x42}, //Added to stop streaming
	{0x302c,0x62},
	{0x3017,0x7f},
	{0x3018,0xfc},
	{0x3810,0x82},
	{0x3000,0x00},
	{0x3001,0x00},
	{0x3002,0x00},
	{0x3003,0x00},
	{0x3613,0x83},
	{0x3614,0x0a},
	{0x3622,0x38},
	{0x3631,0x05},
	{0x3632,0x54},
	{0x3612,0x8b},
	{0x300f,0x0b},
	{0x3012,0x52}, //for 19.5M input clk 02
	{0x3011,0x0c}, //for 19.5M input clk 09
	{0x3015,0x13}, //for 19.5M input clk
	{0x3013,0x03},
	{0x3010,0x00},
	{0x3621,0x50},
	{0x3623,0x06},
	{0x3705,0xda},
	{0x3703,0x4d},
	{0x3704,0x2d},
	{0x400c,0x40},
	{0x3601,0x4c},
	{0x3604,0x28},
	{0x4000,0x21},
	{0x4001,0x40},
	{0x4002,0x00},
	{0x4003,0x80},
	{0x4006,0x00},
	{0x4007,0x10},
	{0x400c,0x00},
	{0x3601,0x4c},
	{0x3606,0x00},
	{0x5020,0x02},
	{0x5000,0x00},
	{0x5182,0x00},
	{0x5197,0x01},
	{0x5000,0x06},
	{0x5001,0x01},
	{0x5080,0x08},
	{0x4801,0x0f},
	{0x3003,0x01},
	{0x3007,0x3b},
	{0x300e,0x04},
	{0x4803,0x50},
	{0x4610,0x00},
	{0x471d,0x05},
	{0x4708,0x06},
	{0x380d,0x48},
	{0x380c,0x09},
	{0x3702,0x14},
	{0x3800,0x00},
	{0x3801,0xee},
	{0x501f,0x03},
	{0x3003,0x03},
	{0x3003,0x01},
	{0x3818,0x80},
	{0x3803,0x0a},
	{0x3a00,0x68},
	{0x3a00,0x68},
	{0x3a18,0x00},
	{0x3a19,0xf8},
	{0x3a1a,0x04},
	{0x3a0d,0x08},
	{0x3a0e,0x06},
	{0x3500,0x00},
	{0x3501,0x00},
	{0x3502,0x00},
	{0x350a,0x00},
	{0x350b,0x00},
	//{0x3503,0x00},
	//{0x3503,0x00},
	{0x3503,0x07},
	{0x3212,0x00},
	{0x300e,0x04},
	{0x3212,0x10},
	{0x3212,0xa0},
	{0x3008,0x02}, //Added to start streaming
	{0x370a,0x81},
	{0x3705,0xdd},
	{0x4001,0xa0},
	{0x3804,0x04},
	{0x3805,0x00},
	{0x5682,0x04},
	{0x5683,0x00},
	{0x3806,0x03},
	{0x3807,0x00},
	{0x5686,0x03},
	{0x5687,0x00},
	{0x3808,0x04},
	{0x3809,0x00},
	{0x380a,0x03},
	{0x380b,0x00},
	{0x380e,0x03},
	{0x380f,0x2f},
	//{0x380c,0x04},
	//{0x380d,0x0c},
	{0x350c,0x03},
	{0x350d,0x0c},
	{0x3818,0x81},
	{0x3810,0x80},
	{0x3803,0x08},
	{0x3622,0x39},
	{0x3a0d,0x04},
	{0x3a0e,0x03}, 

	/* Sensor enter LP11*/
//	{0x4202, 0x0f},
	{0x4800, 0x01},

	{OV3650_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov3650_stream_on[] = {
	//{0x4202, 0x00},
	{0x4800, 0x24},

	{OV3650_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov3650_stream_off[] = {
	/* Sensor enter LP11*/
//	{0x4202, 0x0f},
	{0x4800, 0x01},

	{OV3650_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov3650_win_sxga[] = {
	{0x370a,0x81},                             
	{0x3705,0xdd},                             
	{0x4001,0xa0},                             
	{0x3804,0x04},                             
//	{0x3805,0x00},                             
	{0x5682,0x04},                             
//	{0x5683,0x00},                             
	{0x3806,0x03},                             
//	{0x3807,0x00},                             
	{0x5686,0x03},                             
//	{0x5687,0x00},                             
	{0x3808,0x04},                             
//	{0x3809,0x00},                             
	{0x380a,0x03},                             
//	{0x380b,0x00},   
//	{0x380c,0x09},                             
//	{0x380d,0x48}, 
	{0x380e,0x03},                             
	{0x380f,0x2f},                             
	{0x350c,0x03},                             
	{0x350d,0x0c},                             
	{0x3818,0x81},                             
	{0x3810,0x80},                             
	{0x3803,0x08},                             
	{0x3622,0x39},                             
	{0x3a0d,0x04},                             
	{0x3a0e,0x03},   

	{OV3650_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov3650_win_3m[] = {
	{0x370a,0x80},   
	{0x3705,0xda},   
	{0x4001,0x40},   
	{0x3804,0x08},   
//	{0x3805,0x00},   
	{0x5682,0x08},   
//	{0x5683,0x00},   
	{0x3806,0x06},   
//	{0x3807,0x00},   
	{0x5686,0x06},   
//	{0x5687,0x00},   
	{0x3808,0x08},   
//	{0x3809,0x00},   
	{0x380a,0x06},   
//	{0x380b,0x00},   
//	{0x380c,0x09},                             
//	{0x380d,0x48}, 
	{0x380e,0x06},   
	{0x380f,0x58},   
	{0x350c,0x18},   
	{0x350d,0x00},   
	{0x3818,0x80},   
	{0x3810,0xc2},   
	{0x3803,0x0a},   
	{0x3622,0x10},   
	{0x3a0d,0x08},   
	{0x3a0e,0x06},   

	{OV3650_REG_END, 0x00},	/* END MARKER */
};


static int ov3650_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov3650_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov3650_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV3650_REG_END) {
		if (vals->reg_num == OV3650_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov3650_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical = 0x55;
static unsigned int bg_ratio_typical = 0x61;

/*
	index: index of otp group. (0, 1, 2)
 	return: 0, group index is empty
 	1, group index has invalid data
 	2, group index has valid data
*/
static int ov3650_check_otp(struct v4l2_subdev *sd, unsigned short index)
{
	unsigned char temp;
	unsigned short i;
	unsigned short address;
	int ret;

	// read otp into buffer
	ov3650_write(sd, 0x3d21, 0x01);
	mdelay(5);
	address = 0x3d05 + index * 9;
	ret = ov3650_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	
	// disable otp read
	ov3650_write(sd, 0x3d21, 0x00);
	
	// clear otp buffer
	for (i = 0; i < 32; i++)
		ov3650_write(sd, 0x3d00 + i, 0x00);
	
	if (!temp)
		return 0;
	else if (!(temp & 0x80) && (temp & 0x7f))
		return 2;
	else
		return 1;
}

/*
	index: index of otp group. (0, 1, 2)
	return: 0,
*/
static int ov3650_read_otp(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp)
{
	unsigned char temp;
	unsigned short address;
	unsigned short i;
	int ret;

	// read otp into buffer
	ov3650_write(sd, 0x3d21, 0x01);
	mdelay(5);

	address = 0x3d05 + index * 9;

	ret = ov3650_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	otp->customer_id = (unsigned int)(temp & 0x7f);

	ret = ov3650_read(sd, address, &temp);
	if (ret < 0)
		return ret;
	otp->module_integrator_id = (unsigned int)temp;

	ret = ov3650_read(sd, address + 1, &temp);
	if (ret < 0)
		return ret;
	otp->lens_id = (unsigned int)temp;

	ret = ov3650_read(sd, address + 2, &temp);
	if (ret < 0)
		return ret;
	otp->rg_ratio = (unsigned int)temp;

	ret = ov3650_read(sd, address + 3, &temp);
	if (ret < 0)
		return ret;
	otp->bg_ratio = (unsigned int)temp;

	for (i = 0; i<= 4; i++) {
		ret = ov3650_read(sd, address + 4 + i, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[i] = (unsigned int)temp;
	}

	// disable otp read
	ov3650_write(sd, 0x3d21, 0x00);

	// clear otp buffer
	for (i = 0; i < 32; i++)
		ov3650_write(sd, 0x3d00 + i, 0x00);

	return 0;
}

/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov3650_update_awb_gain(struct v4l2_subdev *sd, 
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	printk("R_gain:%04x, G_gain:%04x, B_gain:%04x \n ", R_gain, G_gain, B_gain);
	if (R_gain > 0x400) {
		ov3650_write(sd, 0x5186, (unsigned char)(R_gain >> 8));
		ov3650_write(sd, 0x5187, (unsigned char)(R_gain & 0x00ff));
	}

	if (G_gain > 0x400) {
		ov3650_write(sd, 0x5188, (unsigned char)(G_gain >> 8));
		ov3650_write(sd, 0x5189, (unsigned char)(G_gain & 0x00ff));
	}

	if (B_gain > 0x400) {
		ov3650_write(sd, 0x518a, (unsigned char)(B_gain >> 8));
		ov3650_write(sd, 0x518b, (unsigned char)(B_gain & 0x00ff));
	}

	return 0;
}

/*
	call this function after OV3650 initialization
	return value: 0 update success
	1, no OTP
*/
static int ov3650_update_otp(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	unsigned int i;
	unsigned int otp_index;
	unsigned int temp;
	unsigned int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int ret = 0;

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for (i = 0; i < 3; i++) {
		temp = ov3650_check_otp(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}

	if (i == 3)
		// no valid wb OTP data
		return 1;

	ret = ov3650_read_otp(sd, otp_index, &current_otp);
	if (ret < 0)
		return ret;

	//calculate G gain
	//0x400 = 1x gain
	if (current_otp.bg_ratio < bg_ratio_typical) {
		if (current_otp.rg_ratio < rg_ratio_typical) {
			// current_otp.bg_ratio < bg_ratio_typical &&
			// current_otp.rg_ratio < rg_ratio_typical
			G_gain = 0x400;
			B_gain = 0x400 * bg_ratio_typical / current_otp.bg_ratio;
			R_gain = 0x400 * rg_ratio_typical / current_otp.rg_ratio;
		} else {
			// current_otp.bg_ratio < bg_ratio_typical &&
			// current_otp.rg_ratio >= rg_ratio_typical
			R_gain = 0x400;
			G_gain = (0x400 * current_otp.rg_ratio) / rg_ratio_typical;
			B_gain = (G_gain * bg_ratio_typical) / current_otp.bg_ratio;
			printk("G_gain:%04x\n ", G_gain);
		}
	} else {
		if (current_otp.rg_ratio < rg_ratio_typical) {
			// current_otp.bg_ratio >= bg_ratio_typical &&
			// current_otp.rg_ratio < rg_ratio_typical
			B_gain = 0x400;
			G_gain = 0x400 * current_otp.bg_ratio / bg_ratio_typical;
			R_gain = G_gain * rg_ratio_typical / current_otp.rg_ratio;
		} else {
			// current_otp.bg_ratio >= bg_ratio_typical &&
			// current_otp.rg_ratio >= rg_ratio_typical
			G_gain_B = 0x400 * current_otp.bg_ratio / bg_ratio_typical;
			G_gain_R = 0x400 * current_otp.rg_ratio / rg_ratio_typical;
			if (G_gain_B > G_gain_R ) {
				B_gain = 0x400;
				G_gain = G_gain_B;
				R_gain = G_gain * rg_ratio_typical / current_otp.rg_ratio;
			} else {
				R_gain = 0x400;
				G_gain = G_gain_R;
				B_gain = G_gain * bg_ratio_typical / current_otp.bg_ratio;
			}
		}
	}

	ov3650_update_awb_gain(sd, R_gain, G_gain, B_gain);

	return 0;
}

static int ov3650_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov3650_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov3650_info *info = container_of(sd, struct ov3650_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x350c;
	info->vts_address2 = 0x350d;// for ov3650 only by wayne 0514
	info->dynamic_framerate = 0;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret = ov3650_write_array(sd, ov3650_init_regs);
	if (ret < 0)
		return ret;

	ret = ov3650_update_otp(sd);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov3650_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;
	printk("ov3650_detect start\n");
	ret = ov3650_read(sd, 0x302a, &v);
	printk("CHIP_ID_H=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != OV3650_CHIP_ID_H)
		return -ENODEV;
	ret = ov3650_read(sd, 0x302b, &v);
	printk("CHIP_ID_L=0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != OV3650_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct ov3650_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov3650_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV3650_FMTS ARRAY_SIZE(ov3650_formats)

static struct ov3650_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
} ov3650_win_sizes[] = {
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.vts		= 0x32f,
		.framerate	= 30,
		.max_gain_dyn_frm = 0x40,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x40,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov3650_win_sxga,
	},
	/* 2048 *1536*/
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x658,
		.framerate	= 10,
		.max_gain_dyn_frm = 0x40,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x40,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov3650_win_3m,
	},	
};
#define N_WIN_SIZES (ARRAY_SIZE(ov3650_win_sizes))

static int ov3650_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV3650_FMTS)
		return -EINVAL;

	*code = ov3650_formats[index].mbus_code;
	return 0;
}

static int ov3650_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov3650_format_struct **ret_fmt,
		struct ov3650_win_size **ret_wsize)
{
	int index;
	struct ov3650_win_size *wsize;

	for (index = 0; index < N_OV3650_FMTS; index++)
		if (ov3650_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV3650_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov3650_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov3650_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov3650_win_sizes; wsize < ov3650_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov3650_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov3650_formats[index].colorspace;
	return 0;
}

static int ov3650_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov3650_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov3650_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov3650_info *info = container_of(sd, struct ov3650_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov3650_format_struct *fmt_s;
	struct ov3650_win_size *wsize;
	int ret,i,len;

	ret = ov3650_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov3650_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
	//	ret = ov3650_write_array(sd, wsize->regs);
	//	if (ret)
	//		return ret;


		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 390*2; /* Mbps. */
		printk("data->mipi_clk = %d\n",data->mipi_clk);
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
			len = ARRAY_SIZE(ov3650_win_3m);
			data->reg_num = len;  //
			for(i =0;i<len;i++) {
				data->reg[i].addr = ov3650_win_3m[i].reg_num;
				data->reg[i].data = ov3650_win_3m[i].value;
			}
			if (!info->dynamic_framerate) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num++].data = data->vts & 0x00ff;
			}
		} else if ((wsize->width == SXGA_WIDTH)
			&& (wsize->height == SXGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			len = ARRAY_SIZE(ov3650_win_sxga);
			data->reg_num = len;  //
			for(i =0;i<len;i++) {
				data->reg[i].addr = ov3650_win_sxga[i].reg_num;
				data->reg[i].data = ov3650_win_sxga[i].value;
			}
			if (!info->dynamic_framerate) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num++].data = data->vts & 0x00ff;
			}
		}
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov3650_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov3650_write_array(sd, ov3650_stream_on);
	else
		ret = ov3650_write_array(sd, ov3650_stream_off);

	return ret;
}

static int ov3650_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov3650_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov3650_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov3650_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov3650_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov3650_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov3650_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov3650_frame_rates[interval->index];
	return 0;
}

static int ov3650_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov3650_win_size *win = &ov3650_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov3650_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov3650_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov3650_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov3650_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov3650_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;

}

static int ov3650_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct ov3650_info *info = container_of(sd, struct ov3650_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		if (ctrl->value == FRAME_RATE_AUTO)
			info->dynamic_framerate = 1;
		else
			ret = -EINVAL;
		break;
	default:
		ret = -ENOIOCTLCMD; 
	}
	
	return ret;
}

static int ov3650_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV3650, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov3650_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov3650_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov3650_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov3650_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov3650_core_ops = {
	.g_chip_ident = ov3650_g_chip_ident,
	.g_ctrl = ov3650_g_ctrl,
	.s_ctrl = ov3650_s_ctrl,
	.queryctrl = ov3650_queryctrl,
	.reset = ov3650_reset,
	.init = ov3650_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov3650_g_register,
	.s_register = ov3650_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov3650_video_ops = {
	.enum_mbus_fmt = ov3650_enum_mbus_fmt,
	.try_mbus_fmt = ov3650_try_mbus_fmt,
	.s_mbus_fmt = ov3650_s_mbus_fmt,
	.s_stream = ov3650_s_stream,
	.cropcap = ov3650_cropcap,
	.g_crop	= ov3650_g_crop,
	.s_parm = ov3650_s_parm,
	.g_parm = ov3650_g_parm,
	.enum_frameintervals = ov3650_enum_frameintervals,
	.enum_framesizes = ov3650_enum_framesizes,
};

static const struct v4l2_subdev_ops ov3650_ops = {
	.core = &ov3650_core_ops,
	.video = &ov3650_video_ops,
};

static ssize_t ov3650_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", rg_ratio_typical);
}

static ssize_t ov3650_rg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical = (unsigned int)value;

	return size;
}

static ssize_t ov3650_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", bg_ratio_typical);
}

static ssize_t ov3650_bg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov3650_rg_ratio_typical, 0664, ov3650_rg_ratio_typical_show, ov3650_rg_ratio_typical_store);
static DEVICE_ATTR(ov3650_bg_ratio_typical, 0664, ov3650_bg_ratio_typical_show, ov3650_bg_ratio_typical_store);

static int ov3650_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov3650_info *info;
	int ret;

	info = kzalloc(sizeof(struct ov3650_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov3650_ops);

	/* Make sure it's an ov3650 */
	ret = ov3650_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov3650 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov3650 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ret = device_create_file(&client->dev, &dev_attr_ov3650_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov3650_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov3650_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov3650_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov3650_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov3650_bg_ratio_typical;
	}

	return 0;

err_create_dev_attr_ov3650_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_ov3650_rg_ratio_typical);
err_create_dev_attr_ov3650_rg_ratio_typical:
	kfree(info);
	return ret;
}

static int ov3650_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov3650_info *info = container_of(sd, struct ov3650_info, sd);

	device_remove_file(&client->dev, &dev_attr_ov3650_rg_ratio_typical);
	device_remove_file(&client->dev, &dev_attr_ov3650_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov3650_id[] = {
	{ "ov3650", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov3650_id);

static struct i2c_driver ov3650_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov3650",
	},
	.probe		= ov3650_probe,
	.remove		= ov3650_remove,
	.id_table	= ov3650_id,
};

static __init int init_ov3650(void)
{
	return i2c_add_driver(&ov3650_driver);
}

static __exit void exit_ov3650(void)
{
	i2c_del_driver(&ov3650_driver);
}

fs_initcall(init_ov3650);
module_exit(exit_ov3650);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov3650 sensors");
MODULE_LICENSE("GPL");
