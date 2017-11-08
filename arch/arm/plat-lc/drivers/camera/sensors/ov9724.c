
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
#include "ov9724.h"


#define OV9724_CHIP_ID_H	(0x97)
#define OV9724_CHIP_ID_L	(0x24)

#define MAX_WIDTH		1280
#define MAX_HEIGHT		720
#define MAX_PREVIEW_WIDTH	MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT


#define OV9724_REG_END		0xffff
#define OV9724_REG_DELAY	0xfffe

struct ov9724_format_struct;

struct ov9724_info {
	struct v4l2_subdev sd;
	struct ov9724_format_struct *fmt;
	struct ov9724_win_size *win;
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
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int user_data[2];
};

static struct regval_list ov9724_init_regs[] = {
//720p HD     
{0x0103,0x01},
{0x3210,0x43},
{0x3606,0x75},
{0x3705,0x67},
{0x0340,0x02},
{0x0341,0xf8},
{0x0342,0x06},
{0x0343,0x28},
{0x0202,0x02},
{0x0203,0xf0},
{0x4801,0x0f},
{0x4801,0x8f},
{0x4814,0x2b},
{0x4307,0x3a},
{0x5000,0x06},
{0x5001,0x73},
{0x0205,0x3f},
{0x0100,0x01},
{0x0205,0x38},

//{OV9724_REG_DELAY, 10},
//	{0x4202, 0x0f},
	{0x4800, 0x01},

	{OV9724_REG_END, 0x00},	/* END MARKER */

};

#if 1
static struct regval_list ov9724_stream_on[] = {
	{0x4202, 0x00},
	{0x4800, 0x24},

	{OV9724_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov9724_stream_off[] = {
	/* Sensor enter LP11*/	
	{0x4202, 0x0f},	
	{0x4800, 0x01},	
	{OV9724_REG_END, 0x00},	/* END MARKER */
};
#else
static struct regval_list ov9724_stream_on[] = {
//	{0x4202, 0x00},
	{0x4800, 0x24},

	{OV9724_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov9724_stream_off[] = {
	/* Sensor enter LP11*/	
//	{0x4202, 0x0f},	
	{0x4800, 0x01},	
	{OV9724_REG_END, 0x00},	/* END MARKER */
};
#endif

static struct regval_list ov9724_win_720p[] = {
	
//720p HD     
{0x0103,0x01},
{0x3210,0x43},
{0x3606,0x75},
{0x3705,0x67},
{0x0340,0x02},
{0x0341,0xf8},
{0x0342,0x06},
{0x0343,0x28},
{0x0202,0x02},
{0x0203,0xf0},
{0x4801,0x0f},
{0x4801,0x8f},
{0x4814,0x2b},
{0x4307,0x3a},
{0x5000,0x06},
{0x5001,0x73},
{0x0205,0x3f},
{0x0100,0x01},
{0x0205,0x38},

{OV9724_REG_END, 0x00},	/* END MARKER */
};

static int ov9724_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov9724_write(struct v4l2_subdev *sd, unsigned short reg,
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


static int ov9724_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV9724_REG_END) {
		if (vals->reg_num == OV9724_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov9724_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#if 0 
static int ov9724_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	while (vals->reg_num != OV9724_REG_END) {
		if (vals->reg_num == OV9724_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			
			ret = ov9724_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

void ov9724_clear_otp_buffer(struct v4l2_subdev *sd)
{
int i;
// clear otp buffer
for (i=0;i<16;i++) {
ov9724_write(sd,0x3d00 + i, 0x00);
}
}


static int ov9724_otp_read(struct v4l2_subdev *sd, unsigned short reg)
{
	unsigned char value = 0;
	int flag;
	ov9724_read(sd,reg,&value);
	flag = (int)value;
	return flag;
}



/* R/G and B/G of typical camera module is defined here. */
static unsigned int RG_Ratio_Typical = 0x33;
static unsigned int BG_Ratio_Typical = 0x7c;
// index: index of otp group. (0, 1)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data


static int ov9724_check_otp(struct v4l2_subdev *sd, int index)
{

	int temp, i;
	int address;
	// read otp into buffer
	ov9724_write(sd,0x3d81, 0x01);
	address = 0x3d00 + index*8;
	temp = ov9724_otp_read(sd,address);
	// disable otp read
	ov9724_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<32;i++) {
	ov9724_write(sd,0x3d00 + i, 0x00);
	}
	if (!temp) {
	return 0;
	}
	else if ((!(temp & 0x80)) && (temp&0x7f)) {
	return 2;
	}
	else {
	return 1;
	}

}




// index: index of otp group. (0, 1)
// return: 0,
static int ov9724_read_otp_wb(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp_ptr)
{
	int i;
	int address;
	// read otp into buffer
	ov9724_write(sd,0x3d81, 0x01);
	address = 0x3d00 + index*8;
	(*otp_ptr).module_integrator_id = (ov9724_otp_read(sd,address) & 0x7f);
	(*otp_ptr).lens_id = ov9724_otp_read(sd,address + 1);
	(*otp_ptr).rg_ratio = ov9724_otp_read(sd,address + 2);
	(*otp_ptr).bg_ratio = ov9724_otp_read(sd,address + 3);
	(*otp_ptr).light_rg = ov9724_otp_read(sd,address + 4);
	(*otp_ptr).light_bg = ov9724_otp_read(sd,address + 5);
	(*otp_ptr).user_data[0] = ov9724_otp_read(sd,address + 6);
	(*otp_ptr).user_data[1] = ov9724_otp_read(sd,address + 7);
	// disable otp read
	ov9724_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<32;i++) {
	ov9724_write(sd,0x3d00 + i, 0x00);
	}
	return 0;
}


// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
static int ov9724_update_awb_gain(struct v4l2_subdev *sd, 
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	if (R_gain>0x400) {
	ov9724_write(sd,0x5180, R_gain>>8);
	ov9724_write(sd,0x5181, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
	ov9724_write(sd,0x5182, G_gain>>8);
	ov9724_write(sd,0x5183, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
	ov9724_write(sd,0x5184, B_gain>>8);
	ov9724_write(sd,0x5185, B_gain & 0x00ff);
	}
	return 0;
}



// call this function after OV9724 initialization
// return value: 0 update success
// 1, no OTP
static int ov9724_update_otp_wb(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=0;i<2;i++) {
	temp = ov9724_check_otp(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i==2) {
	// no valid wb OTP data
	return 1;
	}
	ov9724_read_otp_wb(sd,otp_index, &current_otp);
	if(current_otp.light_rg==0) {
	// no light source information in OTP
	rg = current_otp.rg_ratio;
	}
	else {
	// light source information found in OTP
	rg = current_otp.rg_ratio * (current_otp.light_rg +128) / 256;
	}
	if(current_otp.light_bg==0) {
	// no light source information in OTP
	bg = current_otp.bg_ratio;
	}
	else {
	// light source information found in OTP
	bg = current_otp.bg_ratio * (current_otp.light_bg +128) / 256;
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
	ov9724_update_awb_gain(sd,R_gain, G_gain, B_gain);
	return 0;


}



static int ov9724_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov9724_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov9724_info *info = container_of(sd, struct ov9724_info, sd);
	int ret=0;
	
	info->fmt = NULL;
	info->win = NULL;
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->dynamic_framerate = 0;

	ret=ov9724_write_array(sd, ov9724_init_regs);
	if (ret < 0)
		return ret;

	printk("*update_otp_wb*\n");
	ret = ov9724_update_otp_wb(sd);
	if (ret < 0)
		return ret;
	return ret;
}

static int ov9724_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov9724_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != OV9724_CHIP_ID_H)
		return -ENODEV;
	ret = ov9724_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L=0x%x \n", v);
	if (v != OV9724_CHIP_ID_L)
		return -ENODEV;
	
	/*
	ret = ov9724_read(sd, 0x3608, &v);//detect internal or external DVDD
	printk("r3608=0x%x\n ", v);
	
	if (ret < 0)
		return ret;
	if (v != OV9724_CHIP_ID_L)
		return -ENODEV;
	*/
	return 0;
}

static struct ov9724_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov9724_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV9724_FMTS ARRAY_SIZE(ov9724_formats)

static struct ov9724_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
} ov9724_win_sizes[] = {
	/* 1280*720 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x2f8, // 
		.framerate	= 24,
		.max_gain_dyn_frm = 0x40,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x40,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov9724_win_720p,
	},
	
};
#define N_WIN_SIZES (ARRAY_SIZE(ov9724_win_sizes))

static int ov9724_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
		if (index >= N_OV9724_FMTS)
		return -EINVAL;

	
	*code = ov9724_formats[index].mbus_code;
	return 0;
}

static int ov9724_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov9724_format_struct **ret_fmt,
		struct ov9724_win_size **ret_wsize)
{
	int index;
	struct ov9724_win_size *wsize;

		for (index = 0; index < N_OV9724_FMTS; index++)
		if (ov9724_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV9724_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov9724_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov9724_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov9724_win_sizes; wsize < ov9724_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov9724_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov9724_formats[index].colorspace;
	return 0;
}

static int ov9724_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov9724_try_fmt_internal(sd, fmt, NULL, NULL);
}


static int ov9724_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov9724_info *info = container_of(sd, struct ov9724_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov9724_format_struct *fmt_s;
	struct ov9724_win_size *wsize;
	int ret,len,i;

	ret = ov9724_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov9724_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}
	
	if ((info->win != wsize) && wsize->regs) {
	//	ret = ov9724_write_array(sd, wsize->regs);
	//	if (ret)
   	//	return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 400;  /*Mbps*/
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
			len = ARRAY_SIZE(ov9724_win_720p);
			data->reg_num = len;  
			data->reg_num = 0; 
			for(i =0;i<len;i++) {
				data->reg[i].addr = ov9724_win_720p[i].reg_num;
				data->reg[i].data = ov9724_win_720p[i].value;
			}		
		} 
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov9724_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	
	if (enable)
		ret = ov9724_write_array(sd, ov9724_stream_on);
	else
		ret = ov9724_write_array(sd, ov9724_stream_off);
	#if 0
	else
		ov9724_read_array(sd,ov9724_init_regs );//debug only
	#endif
	return ret;
}

static int ov9724_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov9724_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov9724_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov9724_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov9724_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov9724_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov9724_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov9724_frame_rates[interval->index];
	return 0;
}

static int ov9724_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov9724_win_size *win = &ov9724_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov9724_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov9724_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov9724_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov9724_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov9724_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov9724_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct ov9724_info *info = container_of(sd, struct ov9724_info, sd);
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

static int ov9724_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV9724, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov9724_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov9724_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov9724_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov9724_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov9724_core_ops = {
	.g_chip_ident = ov9724_g_chip_ident,
	.g_ctrl = ov9724_g_ctrl,
	.s_ctrl = ov9724_s_ctrl,
	.queryctrl = ov9724_queryctrl,
	.reset = ov9724_reset,
	.init = ov9724_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov9724_g_register,
	.s_register = ov9724_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov9724_video_ops = {
	.enum_mbus_fmt = ov9724_enum_mbus_fmt,
	.try_mbus_fmt = ov9724_try_mbus_fmt,
	.s_mbus_fmt = ov9724_s_mbus_fmt,
	.s_stream = ov9724_s_stream,
	.cropcap = ov9724_cropcap,
	.g_crop	= ov9724_g_crop,
	.s_parm = ov9724_s_parm,
	.g_parm = ov9724_g_parm,
	.enum_frameintervals = ov9724_enum_frameintervals,
	.enum_framesizes = ov9724_enum_framesizes,
};

static const struct v4l2_subdev_ops ov9724_ops = {
	.core = &ov9724_core_ops,
	.video = &ov9724_video_ops,
};

static int ov9724_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov9724_info *info;
	int ret;

	info = kzalloc(sizeof(struct ov9724_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov9724_ops);

	/* Make sure it's an ov9724 */
	ret = ov9724_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov9724 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov9724 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int ov9724_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9724_info *info = container_of(sd, struct ov9724_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov9724_id[] = {
	{ "ov9724", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov9724_id);

static struct i2c_driver ov9724_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov9724",
	},
	.probe		= ov9724_probe,
	.remove		= ov9724_remove,
	.id_table	= ov9724_id,
};

static __init int init_ov9724(void)
{
	return i2c_add_driver(&ov9724_driver);
}

static __exit void exit_ov9724(void)
{
	i2c_del_driver(&ov9724_driver);
}

fs_initcall(init_ov9724);
module_exit(exit_ov9724);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov9724 sensors");
MODULE_LICENSE("GPL");
