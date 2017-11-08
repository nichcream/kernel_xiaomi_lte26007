
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
#include "bf3703_dvp.h"

#define BF3703_CHIP_ID	(0x37)

#define VGA_WIDTH	640
#define VGA_HEIGHT	480
#define QVGA_WIDTH	320
#define QVGA_HEIGHT	240
#define CIF_WIDTH	352
#define CIF_HEIGHT	288
#define QCIF_WIDTH	176
#define	QCIF_HEIGHT	144
#define MAX_WIDTH	VGA_WIDTH
#define MAX_HEIGHT	VGA_HEIGHT
#define MAX_PREVIEW_WIDTH	VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  VGA_HEIGHT

#define BF3703_REG_END	0xff
#define BF3703_REG_VAL_HVFLIP 0xff

//#define BF3703_DIFF_SIZE

struct bf3703_format_struct;
struct bf3703_info {
	struct v4l2_subdev sd;
	struct bf3703_format_struct *fmt;
	struct bf3703_win_size *win;
	int brightness;
	int contrast;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list bf3703_init_regs[] = {
	{0x11, 0x80},
	{0x09, 0x01}, //0x03
	{0x13, 0x00},
	{0x01, 0x13},
	{0x02, 0x25},
	{0x8c, 0x02}, //01 :devided by 2  02 :devided by 1
	{0x8d, 0xfd}, //cb: devided by 2  fd :devided by 1
	{0x87, 0x1a},
	{0x13, 0x07},

	//POLARITY of Signal 

	{0x15, 0x00},
	{0x3a, 0x00},//  If YUV422 is selected. The Sequence is:  Bit[1:0]:Output YUV422 Sequence     00: YUYV, 01: YVYU     10: UYVY, 11: VYUY 	

	//black level 
	{0x05, 0x1f},
	{0x06, 0x60},
	{0x14, 0x1f},
	{0x27, 0x03},
	{0x06, 0xe0},

	//lens shading
	{0x35, 0x68},
	{0x65, 0x68},
	{0x66, 0x62},
	{0x36, 0x05},
	{0x37, 0xf6},
	{0x38, 0x46},
	{0x9b, 0xf6},
	{0x9c, 0x46},
	{0xbc, 0x01},
	{0xbd, 0xf6},
	{0xbe, 0x46},

	//AE
	{0x82, 0x14},
	{0x83, 0x23},
	{0x9a, 0x23}, //the same as 0x83
	{0x84, 0x1a},
	{0x85, 0x20},
	{0x89, 0x04}, //02 :devided by 2  04 :devided by 1
	{0x8a, 0x08}, //04: devided by 2  05 :devided by 1
	{0x86, 0x26}, //the same as 0x7b
	{0x96, 0xa6}, //AE speed
	{0x97, 0x0c}, //AE speed
	{0x98, 0x18}, //AE speed
	//AE target
	{0x24, 0x7a}, // 0x6a
	{0x25, 0x8a}, // 0x7a
	{0x94, 0x0a}, //INT_OPEN 
	{0x80, 0x55},

	//denoise
	{0x70, 0x6f}, //denoise
	{0x72, 0x4f}, //denoise
	{0x73, 0x2f}, //denoise
	{0x74, 0x27}, //denoise
	{0x77, 0x90},
	{0x7a, 0x0e}, //denoise in	  low light, 0x8e\0x4e\0x0e//8e
	{0x7b, 0x28}, //the same as 0x86

	//black level
	{0X1F, 0x20}, //G target
	{0X22, 0x20}, //R target
	{0X26, 0x20}, //B target

	{0X16, 0x01},
	{0xbb, 0x20},
	{0xeb, 0x30},
	{0xf5, 0x21},
	{0xe1, 0x3c},
	{0xbb, 0x20},
	{0X2f,0Xf6},
	{0x06, 0xe0},



	//anti black sun spot
	{0x61, 0xd3}, //0x61[3]=0 black sun disable
	{0x79, 0x48}, //0x79[7]=0 black sun disable

	//contrast
	{0x56, 0x40},

	//Gamma

	{0x3b, 0x60}, //auto gamma offset adjust in	low light	  
	{0x3c, 0x20}, //auto gamma offset adjust in	low light	  

	{0x39, 0x80},
	/*//gamma1
	{0x3f, 0xa0},
	{0x40, 0x88},
	{0x41, 0x74},
	{0x42, 0x5E},
	{0x43, 0x4c},
	{0x44, 0x44},
	{0x45, 0x3E},
	{0x46, 0x39},
	{0x47, 0x35},
	{0x48, 0x31},
	{0x49, 0x2E},
	{0x4b, 0x2B},
	{0x4c, 0x29},
	{0x4e, 0x25},
	{0x4f, 0x22},
	{0x50, 0x1F},
	*/
	/*gamma2
	{0x3f, 0xb0},
	{0x40, 0x9b},
	{0x41, 0x88},
	{0x42, 0x6e},
	{0x43, 0x59},
	{0x44, 0x4d},
	{0x45, 0x45},
	{0x46, 0x3e},
	{0x47, 0x39},
	{0x48, 0x35},
	{0x49, 0x31},
	{0x4b, 0x2e},
	{0x4c, 0x2b},
	{0x4e, 0x26},
	{0x4f, 0x23},
	{0x50, 0x1F},
	*/

	/*//gamma3
	{0x3f, 0xb0},
	{0x40, 0x60},
	{0x41, 0x60},
	{0x42, 0x66},
	{0x43, 0x57},
	{0x44, 0x4c},
	{0x45, 0x43},
	{0x46, 0x3c},
	{0x47, 0x37},
	{0x48, 0x33},
	{0x49, 0x2f},
	{0x4b, 0x2c},
	{0x4c, 0x29},
	{0x4e, 0x25},
	{0x4f, 0x22},
	{0x50, 0x20},
	*/

	//gamma 4	low noise

	{0x3f, 0xb8},
	{0x40, 0x48},
	{0x41, 0x54},
	{0x42, 0x4E},
	{0x43, 0x44},
	{0x44, 0x3E},
	{0x45, 0x39},
	{0x46, 0x35},
	{0x47, 0x31},
	{0x48, 0x2E},
	{0x49, 0x2B},
	{0x4b, 0x29},
	{0x4c, 0x27},
	{0x4e, 0x23},
	{0x4f, 0x20},
	{0x50, 0x20},

	/*
	//color matrix
	{0x51, 0x0d},
	{0x52, 0x21},
	{0x53, 0x14},
	{0x54, 0x15},
	{0x57, 0x8d},
	{0x58, 0x78},
	{0x59, 0x5f},
	{0x5a, 0x84},
	{0x5b, 0x25},
	{0x5D, 0x95},
	{0x5C, 0x0e},
	*/

	/*color
	{0x51, 0x0e},
	{0x52, 0x16},
	{0x53, 0x07},
	{0x54, 0x1a},
	{0x57, 0x9d},
	{0x58, 0x82},
	{0x59, 0x71},
	{0x5a, 0x8d},
	{0x5b, 0x1c},
	{0x5D, 0x95},
	{0x5C, 0x0e},
	*/
	{0x51, 0x08},
	{0x52, 0x0E},
	{0x53, 0x06},
	{0x54, 0x12},
	{0x57, 0x82},
	{0x58, 0x70},
	{0x59, 0x5C},
	{0x5a, 0x77},
	{0x5b, 0x1B},
	{0x5c, 0x0e}, //0x5c[3:0] low light color coefficient£¬smaller ,lower noise
	{0x5d, 0x95},
	/*
	//color µ­
	{0x51, 0x03},
	{0x52, 0x0d},
	{0x53, 0x0b},
	{0x54, 0x14},
	{0x57, 0x59},
	{0x58, 0x45},
	{0x59, 0x41},
	{0x5a, 0x5f},
	{0x5b, 0x1e},
	{0x5c, 0x0e}, //0x5c[3:0] low light color coefficient£¬smaller ,lower noise
	{0x5d, 0x95},
	*/

	{0x60, 0x20}, //color open in low light 
	//AWB
	{0x6a, 0x81},
	{0x23, 0x55}, //Green gain
	{0xa0, 0x07},

	{0xa1,0X41}, //
	{0xa2,0X0e},
	{0xa3,0X26},
	{0xa4,0X0d},
	//{0xa5, 0x28}, //The upper limit of red gain 

	{0xa5, 0x2d},

	{0xa6, 0x04},   
	{0xa7, 0x80}, //BLUE Target            
	{0xa8, 0x80}, //RED Target             
	{0xa9, 0x28},                  
	{0xaa, 0x28},                  
	{0xab, 0x28},                
	{0xac, 0x3c},                   
	{0xad, 0xf0},                  
	{0xc8, 0x18},                   
	{0xc9, 0x20},                   
	{0xca, 0x17},                    
	{0xcb, 0x1f},                 
	{0xaf, 0x00},               
	{0xc5, 0x18},                 
	{0xc6, 0x00},            
	{0xc7, 0x20},                  
	{0xae, 0x83},
	{0xcc, 0x30},
	{0xcc, 0x30},
	{0xcd, 0x70},    
	{0xee, 0x4c}, // P_TH             


	// color saturation
	{0xb0, 0xe0},
	{0xb1, 0xc0},
	{0xb2, 0xb0},//c0

	//  {0xb1, 0xd0},
	//  {0xb2, 0xc0},

	{0xb3, 0x88},

	//anti webcamera banding
	{0x9d, 0x93},

	//switch direction
	{0x1e, 0x00},	//0x00

	{0x8e, 0x06},
	{0x8f, 0x6d},
	{0x2b, 0x64},

	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_fmt_yuv422[] = {
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_win_vga[] = {
	//crop window
	{0x12, 0x00},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

#ifdef BF3703_DIFF_SIZE
static struct regval_list bf3703_win_cif[] = {
	//crop window

	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_win_qvga[] = {
	//crop window
	{0x12, 0x10},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_win_qcif[] = {
	//crop window

	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_win_144x176[] = {
	// page 0

	{BF3703_REG_END, 0x00},	/* END MARKER */
};
#endif

static struct regval_list bf3703_brightness_l3[] = {
	{0x55, 0xc0},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_brightness_l2[] = {
	{0x55, 0xa0},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_brightness_l1[] = {
	{0x55, 0x90},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_brightness_h0[] = {
	{0x55, 0x00},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_brightness_h1[] = {
	{0x55, 0x15},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_brightness_h2[] = {
	{0x55, 0x30},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_brightness_h3[] = {
	{0x55, 0x50},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_contrast_l3[] = {
	{0x56, 0x10},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_contrast_l2[] = {
	{0x56, 0x20},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_contrast_l1[] = {
	{0x56, 0x30},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_contrast_h0[] = {
	{0x56, 0x40},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_contrast_h1[] = {
	{0x56, 0x50},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_contrast_h2[] = {
	{0x56, 0x60},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bf3703_contrast_h3[] = {
	{0x56, 0x70},
	{BF3703_REG_END, 0x00},	/* END MARKER */
};

static int bf3703_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int bf3703_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int bf3703_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	while (vals->reg_num != BF3703_REG_END) {
		int ret = bf3703_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int bf3703_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int bf3703_init(struct v4l2_subdev *sd, u32 val)
{
	struct bf3703_info *info = container_of(sd, struct bf3703_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return bf3703_write_array(sd, bf3703_init_regs);
}

static int bf3703_detect(struct v4l2_subdev *sd)
{
	unsigned char v=0x0;
	int ret;

	ret = bf3703_read(sd, 0xfc, &v);
	printk("bf3703 ensor_id = %0x\n",v);
	if (ret < 0)
		return ret;

	if (v != BF3703_CHIP_ID)
		return -ENODEV;
	return 0;
}

static struct bf3703_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} bf3703_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= bf3703_fmt_yuv422,
	},
};
#define N_BF3703_FMTS ARRAY_SIZE(bf3703_formats)

static struct bf3703_win_size {
	int	width;
	int	height;
	struct regval_list *regs;
} bf3703_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= bf3703_win_vga,
	},
#ifdef BF3703_DIFF_SIZE
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
		.regs 		= bf3703_win_cif,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
		.regs 		= bf3703_win_qvga,
	},
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
		.regs 		= bf3703_win_qcif,
	},
	/* 144x176 */
	{
		.width		= QCIF_HEIGHT,
		.height		= QCIF_WIDTH,
		.regs 		= bf3703_win_144x176,
	},
#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(bf3703_win_sizes))

static int bf3703_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_BF3703_FMTS)
		return -EINVAL;

	*code = bf3703_formats[index].mbus_code;
	return 0;
}

static int bf3703_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct bf3703_format_struct **ret_fmt,
		struct bf3703_win_size **ret_wsize)
{
	int index;
	struct bf3703_win_size *wsize;

	for (index = 0; index < N_BF3703_FMTS; index++)
		if (bf3703_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_BF3703_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = bf3703_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = bf3703_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = bf3703_win_sizes; wsize < bf3703_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= bf3703_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = bf3703_formats[index].colorspace;
	return 0;
}

static int bf3703_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return bf3703_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int bf3703_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct bf3703_info *info = container_of(sd, struct bf3703_info, sd);
	struct bf3703_format_struct *fmt_s;
	struct bf3703_win_size *wsize;
	int ret;

	ret = bf3703_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = bf3703_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = bf3703_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int bf3703_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int bf3703_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int bf3703_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3703_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bf3703_frame_rates[] = { 30, 15, 10, 5, 1 };

static int bf3703_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(bf3703_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = bf3703_frame_rates[interval->index];
	return 0;
}

static int bf3703_enum_framesizes(struct v4l2_subdev *sd,
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
		struct bf3703_win_size *win = &bf3703_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int bf3703_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int bf3703_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[BF3703]set brightness %d\n", brightness);

	switch (brightness) {
	case BRIGHTNESS_L3:
		bf3703_write_array(sd, bf3703_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		bf3703_write_array(sd, bf3703_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		bf3703_write_array(sd, bf3703_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		bf3703_write_array(sd, bf3703_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		bf3703_write_array(sd, bf3703_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		bf3703_write_array(sd, bf3703_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		bf3703_write_array(sd, bf3703_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3703_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk(KERN_DEBUG "[BF3703]set contrast %d\n", contrast);

	switch (contrast) {
	case CONTRAST_L3:
		bf3703_write_array(sd, bf3703_contrast_l3);
		break;
	case CONTRAST_L2:
		bf3703_write_array(sd, bf3703_contrast_l2);
		break;
	case CONTRAST_L1:
		bf3703_write_array(sd, bf3703_contrast_l1);
		break;
	case CONTRAST_H0:
		bf3703_write_array(sd, bf3703_contrast_h0);
		break;
	case CONTRAST_H1:
		bf3703_write_array(sd, bf3703_contrast_h1);
		break;
	case CONTRAST_H2:
		bf3703_write_array(sd, bf3703_contrast_h2);
		break;
	case CONTRAST_H3:
		bf3703_write_array(sd, bf3703_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bf3703_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3703_info *info = container_of(sd, struct bf3703_info, sd);
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ctrl->value = info->brightness;
		break;
	case V4L2_CID_CONTRAST:
		ctrl->value = info->contrast;
		break;
	case V4L2_CID_FRAME_RATE:
		ctrl->value = info->frame_rate;
		break;
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&bf3703_isp_setting;
		isp_setting.size = ARRAY_SIZE(bf3703_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&bf3703_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bf3703_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct bf3703_info *info = container_of(sd, struct bf3703_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = bf3703_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = bf3703_set_contrast(sd, ctrl->value);
		if (!ret)
			info->contrast = ctrl->value;
		break;
	case V4L2_CID_FRAME_RATE:
		info->frame_rate = ctrl->value;
		break;
	case V4L2_CID_SET_FOCUS:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bf3703_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_BF3703, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bf3703_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bf3703_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int bf3703_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	bf3703_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}

int bf3703_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;
	printk("########bf3703 read init regs####################\n");

	//return 0;
	while (vals->reg_num != BF3703_REG_END) {
		/*if (vals->reg_num == BF3703_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else
		*/
		{
			ret = bf3703_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}

static int bf3703_s_stream(struct v4l2_subdev *sd)
{
	int ret = 0;

	printk("BF3703_s_stream\n");
	bf3703_read_array(sd, bf3703_init_regs);//debug only
	return ret;
}
#endif

static const struct v4l2_subdev_core_ops bf3703_core_ops = {
	.g_chip_ident = bf3703_g_chip_ident,
	.g_ctrl = bf3703_g_ctrl,
	.s_ctrl = bf3703_s_ctrl,
	.queryctrl = bf3703_queryctrl,
	.reset = bf3703_reset,
	.init = bf3703_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = bf3703_g_register,
	.s_register = bf3703_s_register,
#endif
};

static const struct v4l2_subdev_video_ops bf3703_video_ops = {
	.enum_mbus_fmt = bf3703_enum_mbus_fmt,
	.try_mbus_fmt = bf3703_try_mbus_fmt,
	.s_mbus_fmt = bf3703_s_mbus_fmt,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.s_stream = bf3703_s_stream,
#endif
	.cropcap = bf3703_cropcap,
	.g_crop	= bf3703_g_crop,
	.s_parm = bf3703_s_parm,
	.g_parm = bf3703_g_parm,
	.enum_frameintervals = bf3703_enum_frameintervals,
	.enum_framesizes = bf3703_enum_framesizes,
};

static const struct v4l2_subdev_ops bf3703_ops = {
	.core = &bf3703_core_ops,
	.video = &bf3703_video_ops,
};

static int bf3703_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct bf3703_info *info;
	struct comip_camera_subdev_platform_data *bf3703_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct bf3703_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &bf3703_ops);

	/* Make sure it's an bf3703 */
	ret = bf3703_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an bf3703 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "bf3703 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	bf3703_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (bf3703_setting) {
		if (bf3703_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			bf3703_isp_parm.mirror = 1;
		else
			bf3703_isp_parm.mirror = 0;

		if (bf3703_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			bf3703_isp_parm.flip = 1;
		else
			bf3703_isp_parm.flip = 0;
	}

	return 0;
}

static int bf3703_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf3703_info *info = container_of(sd, struct bf3703_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id bf3703_id[] = {
	{ "bf3703", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf3703_id);

static struct i2c_driver bf3703_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bf3703",
	},
	.probe		= bf3703_probe,
	.remove		= bf3703_remove,
	.id_table	= bf3703_id,
};

static __init int init_bf3703(void)
{
	return i2c_add_driver(&bf3703_driver);
}

static __exit void exit_bf3703(void)
{
	i2c_del_driver(&bf3703_driver);
}

fs_initcall(init_bf3703);
module_exit(exit_bf3703);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore bf3703 sensors");
MODULE_LICENSE("GPL");

