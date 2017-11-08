
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
#include "gc0329.h"

#define GC0329_CHIP_ID	(0xc0)

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

#define GC0329_REG_END	0xff
#define GC0329_REG_DELAY 0x00
#define GC0329_REG_VAL_HVFLIP 0xff

//#define GC0329_DIFF_SIZE

struct gc0329_format_struct;
struct gc0329_info {
	struct v4l2_subdev sd;
	struct gc0329_format_struct *fmt;
	struct gc0329_win_size *win;
	int brightness;
	int contrast;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list gc0329_init_regs[] = {
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xfe, 0x00},
	{0x73, 0x90},
	{0x74, 0x80}, 
	{0x75, 0x80}, 
	{0x76, 0x94}, 
	{0x42, 0x00},
	{0x03, 0x02},
	{0x04, 0xc1},
	{0x77, 0x57}, 
	{0x78, 0x4d}, 
	{0x79, 0x45}, 
	{0x0a, 0x02}, 
	{0x0c, 0x02}, 
//	{0x17, 0x17},
	{0x17, GC0329_REG_VAL_HVFLIP},
	{0x19, 0x05}, 
	{0x1b, 0x24}, 
	{0x1c, 0x04}, 
	{0x1e, 0x08}, 
	{0x1f, 0xc8},
	{0x20, 0x00}, 
	{0x21, 0x50},//48
	{0x22, 0xba},
	{0x23, 0x22},
	{0x24, 0x16},	   
	{0x26, 0xf7}, 
	{0x29, 0x80}, 
	{0x32, 0x04},
	{0x33, 0x20},
	{0x34, 0x20},
	{0x35, 0x20},
	{0x36, 0x20},
	{0x40, 0xff},
	{0x41, 0x44}, 
	{0x42, 0x7e}, 
	{0x44, 0xa2}, 
	{0x46, 0x02},
	{0x4b, 0xca},
	{0x4d, 0x01}, 
	{0x4f, 0x01},
	{0x70, 0x48}, 
	{0x80, 0xe7},
	{0x82, 0x5f},
	{0x83, 0x02},
	{0x87, 0x4a},
	{0x95, 0x34},
	{0xbf, 0x06},
	{0xc0, 0x14},
	{0xc1, 0x27},
	{0xc2, 0x3b},
	{0xc3, 0x4f},
	{0xc4, 0x62},
	{0xc5, 0x72},
	{0xc6, 0x8d},
	{0xc7, 0xa4},
	{0xc8, 0xb8},
	{0xc9, 0xc9},
	{0xcA, 0xd6},
	{0xcB, 0xe0},
	{0xcC, 0xe8},
	{0xcD, 0xf4},
	{0xcE, 0xFc},
	{0xcF, 0xFF},
	{0xfe, 0x00},
	{0xb3, 0x44},
	{0xb4, 0xfd},
	{0xb5, 0x02},
	{0xb6, 0xfa},
	{0xb7, 0x48},
	{0xb8, 0xf0},					   
	{0x50, 0x01},
	{0xfe, 0x00},
	{0xd1, 0x38},
	{0xd2, 0x38},
	{0xdd, 0x54},
	{0xfe, 0x01},
	{0x10, 0x40},
	{0x11, 0x21}, 
	{0x12, 0x07}, 
	{0x13, 0x50},
	{0x17, 0x88}, 
	{0x21, 0xb0},
	{0x22, 0x48},
	{0x3c, 0x95},
	{0x3d, 0x50},
	{0x3e, 0x48}, 
	{0xfe, 0x01},
	{0x06, 0x06},
	{0x07, 0x06},
	{0x08, 0xa6},
	{0x09, 0xee},
	{0x50, 0xfc},
	{0x51, 0x30},
	{0x52, 0x10},
	{0x53, 0x10},
	{0x54, 0x10},
	{0x55, 0x16},
	{0x56, 0x10},
//{0x57, 0x40},
	{0x58, 0x60},
	{0x59, 0x08},
	{0x5a, 0x02},
	{0x5b, 0x63},
	{0x5c, 0x35},//37 big_C_mode
	{0x5d, 0x73}, 
	{0x5e, 0x11}, 
	{0x5f, 0x40}, 
	{0x60, 0x40}, 
	{0x61, 0xc8}, 
	{0x62, 0xa0}, 
	{0x63, 0x40}, 
	{0x64, 0x50}, 
	{0x65, 0x98}, 
	{0x66, 0xfa},
	{0x67, 0x80},
	{0x68, 0x60},
	{0x69, 0x90},
	{0x6a, 0x40},
	{0x6b, 0x39},
	{0x6c, 0x30},
	{0x6d, 0x30},
	{0x6e, 0x41},
	{0x70, 0x10},
	{0x71, 0x00},
	{0x72, 0x10},
	{0x73, 0x40},
	{0x74, 0x32},
	{0x75, 0x40},
	{0x76, 0x30},
	{0x77, 0x48},
	{0x7a, 0x50},
	{0x7b, 0x20},
	{0x80, 0x40},
	{0x81, 0x5e},
	{0x82, 0x40},
	{0x83, 0x40},
	{0x84, 0x40},
	{0x85, 0x40},
	{0x9c, 0x00},
	{0x9d, 0x20},
	{0xd0, 0x00},
	{0xd2, 0x2c},
	{0xd3, 0x80}, 
	{0xfe, 0x01},
	{0xa0, 0x00},
	{0xa1, 0x3c},
	{0xa2, 0x50},
	{0xa3, 0x00},
	{0xa8, 0x0f},
	{0xa9, 0x08},
	{0xaa, 0x00},
	{0xab, 0x04},
	{0xac, 0x00},
	{0xad, 0x07},
	{0xae, 0x0e},
	{0xaf, 0x00},
	{0xb0, 0x00},
	{0xb1, 0x09},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x31},
	{0xb5, 0x19},
	{0xb6, 0x24},
	{0xba, 0x3a},
	{0xbb, 0x24},
	{0xbc, 0x2a},
	{0xc0, 0x17},
	{0xc1, 0x13},
	{0xc2, 0x17},
	{0xc6, 0x21},
	{0xc7, 0x1c},
	{0xc8, 0x1c},
	{0xb7, 0x00},
	{0xb8, 0x00},
	{0xb9, 0x00},
	{0xbd, 0x00},
	{0xbe, 0x00},
	{0xbf, 0x00},
	{0xc3, 0x00},
	{0xc4, 0x00},
	{0xc5, 0x00},
	{0xc9, 0x00},
	{0xca, 0x00},
	{0xcb, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0x00},
	{0xa7, 0x00},
	{0xfe, 0x01},
	{0x18, 0x22},
	{0x21, 0xc0},
	{0x06, 0x12},
	{0x08, 0x9c},
/*	{0x51, 0x28},
	{0x52, 0x10},
	{0x53, 0x20},
	{0x54, 0x40},
	{0x55, 0x16},
	{0x56, 0x30},
	{0x58, 0x60},
	{0x59, 0x08},
	{0x5c, 0x35},
	{0x5d, 0x72},
	{0x67, 0x80},
	{0x68, 0x60},
	{0x69, 0x90},
	{0x6c, 0x30},
	{0x6d, 0x60},
	{0x70, 0x10},*/
	{0xfe, 0x00},
	{0x9c, 0x0a},
	{0xa0, 0xaf},
	{0xa2, 0xff},
	{0xa4, 0x60},
	{0xa5, 0x31},
	{0xa7, 0x35},
	{0x42, 0xfe},
	{0xd1, 0x2c},
	{0xd2, 0x2c},
        {0xd3, 0x38},
	{0xfe, 0x00},
	{0x44, 0xa2},
	{0xf0, 0x07},
	{0xf1, 0x01},

	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_fmt_yuv422[] = {
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_win_vga[] = {
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x01},
	{0x56, 0xe0},
	{0x57, 0x02},
	{0x58, 0x80},
	//1/2 subsample
	{0x59, 0x11},
	{0x5a, 0x0e},
	{0x5b, 0x02},
	{0x5c, 0x04},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x04},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x04},
	{0x30, 0x68},//12fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0329_REG_END, 0x00},	/* END MARKER */
};

#ifdef GC0329_DIFF_SIZE
static struct regval_list gc0329_win_cif[] = {
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x10},
	{0x55, 0x01},
	{0x56, 0x20},
	{0x57, 0x01},
	{0x58, 0x60},
	//4/5 subsample
	{0x59, 0x55},
	{0x5a, 0x0e},
	{0x5b, 0x02},
	{0x5c, 0x04},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x04},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x03},
	{0x30, 0x4e},//16.66fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_win_qvga[] = {
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x00},
	{0x53, 0x00},
	{0x54, 0x00},
	{0x55, 0x00},
	{0x56, 0xf0},
	{0x57, 0x01},
	{0x58, 0x40},
	//1/2 subsample
	{0x59, 0x22},
	{0x5a, 0x03},
	{0x5b, 0x00},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x03},
	{0x30, 0x4e},//16.66fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_win_qcif[] = {
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x08},
	{0x53, 0x00},
	{0x54, 0x38},
	{0x55, 0x00},
	{0x56, 0x90},
	{0x57, 0x00},
	{0x58, 0xb0},
	//2/5 subsample
	{0x59, 0x55},
	{0x5a, 0x03},
	{0x5b, 0x02},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps25
	{0x05, 0x02},
	{0x06, 0x28},//hb:0x228
	{0x07, 0x00},
	{0x08, 0x4c},//vb:0x04c

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x8d},//step:0x8d

	{0x2b, 0x02},
	{0x2c, 0x34},//24.99fps
	{0x2d, 0x02},
	{0x2e, 0xc1},//19.99fps
	{0x2f, 0x03},
	{0x30, 0x4e},//16.66fps
	{0x31, 0x06},
	{0x32, 0x9c},//8.33fps
	{0xfe, 0x00},//page0

	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_win_144x176[] = {
	// page 0
	{0xfe, 0x00},
	{0x17, 0x14},
	//crop window
	{0x4b, 0xca},
	{0x50, 0x01},
	{0x51, 0x00},
	{0x52, 0x08},
	{0x53, 0x00},
	{0x54, 0x38},
	{0x55, 0x00},
	{0x56, 0xb8},
	{0x57, 0x00},
	{0x58, 0x98},
	//2/5 subsample
	{0x59, 0x55},
	{0x5a, 0x03},
	{0x5b, 0x02},
	{0x5c, 0x00},
	{0x5d, 0x00},
	{0x5e, 0x00},
	{0x5f, 0x02},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	//fps15
	{0x05, 0x02},
	{0x06, 0x5e},//hb:0x25e
	{0x07, 0x00},
	{0x08, 0xd4},//vb:0x0d4

	{0xfe, 0x01},//page1
	{0x29, 0x00},
	{0x2a, 0x64},//step:0x64

	{0x2b, 0x02},
	{0x2c, 0xbc},//14.28fps
	{0x2d, 0x03},
	{0x2e, 0x20},//12.5fps
	{0x2f, 0x04},
	{0x30, 0xb0},//8.3fps
	{0x31, 0x06},
	{0x32, 0x40},//6.25fps
	{0x33, 0x00},//fix framerate at 14.28fps
	{0xfe, 0x00},//page0

	{GC0329_REG_END, 0x00},	/* END MARKER */
};
#endif
static struct regval_list gc0329_brightness_l3[] = {
	{0xd5, 0xc0},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_brightness_l2[] = {
	{0xd5, 0xe0},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_brightness_l1[] = {
	{0xd5, 0xf0},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_brightness_h0[] = {
	{0xd5, 0x00},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_brightness_h1[] = {
	{0xd5, 0x10},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_brightness_h2[] = {
	{0xd5, 0x20},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_brightness_h3[] = {
	{0xd5, 0x30},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_contrast_l3[] = {
	{0xd3, 0x28},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_contrast_l2[] = {
	{0xd3, 0x30},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_contrast_l1[] = {
	{0xd3, 0x38},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_contrast_h0[] = {
	{0xd3, 0x40},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_contrast_h1[] = {
	{0xd3, 0x48},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_contrast_h2[] = {
	{0xd3, 0x50},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0329_contrast_h3[] = {
	{0xd3, 0x58},	
	{GC0329_REG_END, 0x00},	/* END MARKER */
};

static int gc0329_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int gc0329_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc0329_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	unsigned char val = 0x14;
	int ret = 0;

	while (vals->reg_num != GC0329_REG_END) {
		if (vals->reg_num == GC0329_REG_DELAY) {
			mdelay(vals->value);
			vals++;
		}
		else {
			if (vals->reg_num == 0x17 && vals->value == GC0329_REG_VAL_HVFLIP) {
				if (gc0329_isp_parm.mirror)
					val |= 0x01;
				else
					val &= 0xfe;

				if (gc0329_isp_parm.flip)
					val |= 0x02;
				else
					val &= 0xfd;

				vals->value = val;
			}
			ret = gc0329_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
			vals++;
		}
	}
	return 0;
}

static int gc0329_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int gc0329_init(struct v4l2_subdev *sd, u32 val)
{
	struct gc0329_info *info = container_of(sd, struct gc0329_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return gc0329_write_array(sd, gc0329_init_regs);
}

static int gc0329_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;





	ret = gc0329_write(sd, 0xfc, 0x16);
	if (ret < 0)
		return ret;
	ret = gc0329_read(sd, 0xfc, &v);
	if (ret < 0)
		return ret;
	ret = gc0329_read(sd, 0x00, &v);
	if (ret < 0)
		return ret;
	if (v != GC0329_CHIP_ID)
		return -ENODEV;
	return 0;
}

static struct gc0329_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} gc0329_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= gc0329_fmt_yuv422,
	},
};
#define N_GC0329_FMTS ARRAY_SIZE(gc0329_formats)

static struct gc0329_win_size {
	int	width;
	int	height;
	struct regval_list *regs;
} gc0329_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= gc0329_win_vga,
	},
#if 0
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
		.regs 		= gc0329_win_cif,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
		.regs 		= gc0329_win_qvga,
	},
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
		.regs 		= gc0329_win_qcif,
	},
	/* 144x176 */
	{
		.width		= QCIF_HEIGHT,
		.height		= QCIF_WIDTH,
		.regs 		= gc0329_win_144x176,
	},
#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(gc0329_win_sizes))

static int gc0329_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_GC0329_FMTS)
		return -EINVAL;

	*code = gc0329_formats[index].mbus_code;
	return 0;
}

static int gc0329_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct gc0329_format_struct **ret_fmt,
		struct gc0329_win_size **ret_wsize)
{
	int index;
	struct gc0329_win_size *wsize;

	for (index = 0; index < N_GC0329_FMTS; index++)
		if (gc0329_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_GC0329_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = gc0329_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = gc0329_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = gc0329_win_sizes; wsize < gc0329_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= gc0329_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = gc0329_formats[index].colorspace;
	return 0;
}

static int gc0329_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return gc0329_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int gc0329_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct gc0329_info *info = container_of(sd, struct gc0329_info, sd);
	struct gc0329_format_struct *fmt_s;
	struct gc0329_win_size *wsize;
	int ret;

	ret = gc0329_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = gc0329_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = gc0329_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int gc0329_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int gc0329_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int gc0329_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc0329_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int gc0329_frame_rates[] = { 30, 15, 10, 5, 1 };

static int gc0329_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(gc0329_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = gc0329_frame_rates[interval->index];
	return 0;
}

static int gc0329_enum_framesizes(struct v4l2_subdev *sd,
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
		struct gc0329_win_size *win = &gc0329_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int gc0329_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int gc0329_s_stream(struct v4l2_subdev *sd, int enable)
{
	if (enable)
		msleep(500);

	return 0;
}

static int gc0329_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[GC0329]set brightness %d\n", brightness);

	switch (brightness) {
	case BRIGHTNESS_L3:
		gc0329_write_array(sd, gc0329_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		gc0329_write_array(sd, gc0329_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		gc0329_write_array(sd, gc0329_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		gc0329_write_array(sd, gc0329_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		gc0329_write_array(sd, gc0329_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		gc0329_write_array(sd, gc0329_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		gc0329_write_array(sd, gc0329_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int gc0329_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk(KERN_DEBUG "[GC0329]set contrast %d\n", contrast);

	switch (contrast) {
	case CONTRAST_L3:
		gc0329_write_array(sd, gc0329_contrast_l3);
		break;
	case CONTRAST_L2:
		gc0329_write_array(sd, gc0329_contrast_l2);
		break;
	case CONTRAST_L1:
		gc0329_write_array(sd, gc0329_contrast_l1);
		break;
	case CONTRAST_H0:
		gc0329_write_array(sd, gc0329_contrast_h0);
		break;
	case CONTRAST_H1:
		gc0329_write_array(sd, gc0329_contrast_h1);
		break;
	case CONTRAST_H2:
		gc0329_write_array(sd, gc0329_contrast_h2);
		break;
	case CONTRAST_H3:
		gc0329_write_array(sd, gc0329_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int gc0329_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0329_info *info = container_of(sd, struct gc0329_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&gc0329_isp_setting;
		isp_setting.size = ARRAY_SIZE(gc0329_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&gc0329_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int gc0329_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0329_info *info = container_of(sd, struct gc0329_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = gc0329_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = gc0329_set_contrast(sd, ctrl->value);
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

static int gc0329_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_GC0329, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int gc0329_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc0329_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int gc0329_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc0329_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops gc0329_core_ops = {
	.g_chip_ident = gc0329_g_chip_ident,
	.g_ctrl = gc0329_g_ctrl,
	.s_ctrl = gc0329_s_ctrl,
	.queryctrl = gc0329_queryctrl,
	.reset = gc0329_reset,
	.init = gc0329_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = gc0329_g_register,
	.s_register = gc0329_s_register,
#endif
};

static const struct v4l2_subdev_video_ops gc0329_video_ops = {
	.enum_mbus_fmt = gc0329_enum_mbus_fmt,
	.try_mbus_fmt = gc0329_try_mbus_fmt,
	.s_mbus_fmt = gc0329_s_mbus_fmt,
	.cropcap = gc0329_cropcap,
	.g_crop	= gc0329_g_crop,
	.s_parm = gc0329_s_parm,
	.g_parm = gc0329_g_parm,
	.s_stream = gc0329_s_stream,
	.enum_frameintervals = gc0329_enum_frameintervals,
	.enum_framesizes = gc0329_enum_framesizes,
};

static const struct v4l2_subdev_ops gc0329_ops = {
	.core = &gc0329_core_ops,
	.video = &gc0329_video_ops,
};

static int gc0329_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct gc0329_info *info;
	struct comip_camera_subdev_platform_data *gc0329_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct gc0329_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &gc0329_ops);

	/* Make sure it's an gc0329 */
	ret = gc0329_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an gc0329 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "gc0329 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	gc0329_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (gc0329_setting) {
		if (gc0329_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			gc0329_isp_parm.mirror = 1;
		else
			gc0329_isp_parm.mirror = 0;

		if (gc0329_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			gc0329_isp_parm.flip = 1;
		else
			gc0329_isp_parm.flip = 0;
	}

	return 0;
}

static int gc0329_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0329_info *info = container_of(sd, struct gc0329_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id gc0329_id[] = {
	{ "gc0329", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0329_id);

static struct i2c_driver gc0329_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc0329",
	},
	.probe		= gc0329_probe,
	.remove		= gc0329_remove,
	.id_table	= gc0329_id,
};

static __init int init_gc0329(void)
{
	return i2c_add_driver(&gc0329_driver);
}

static __exit void exit_gc0329(void)
{
	i2c_del_driver(&gc0329_driver);
}

fs_initcall(init_gc0329);
module_exit(exit_gc0329);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore gc0329 sensors");
MODULE_LICENSE("GPL");
