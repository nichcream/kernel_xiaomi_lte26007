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
#include "hi704.h"

#define HI704_CHIP_ID	(0x96)

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

#define HI704_REG_END	0xff
#define HI704_REG_VAL_HVFLIP	0xff

struct hi704_format_struct;
struct hi704_info {
	struct v4l2_subdev sd;
	struct hi704_format_struct *fmt;
	struct hi704_win_size *win;
	int brightness;
	int contrast;
	int frame_rate;
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list hi704_init_regs[] = {

	{0x03, 0x00},
	{0x01, 0xf1},
	{0x01, 0xf3},
	{0x01, 0xf1},

	{0x03, 0x00},
	{0x10, 0x00},
//	{0x11, 0x90},
	{0x11, HI704_REG_VAL_HVFLIP},
	{0x12, 0x05},//04

	{0x20, 0x00},
	{0x21, 0x04},
	{0x22, 0x00},
	{0x23, 0x04},

	{0x40, 0x01}, //Hblank
	{0x41, 0x58},
	{0x42, 0x00}, //Vblank
	{0x43, 0x96},

	//BLC
	{0x80, 0x2e},
	{0x81, 0x7e},
	{0x82, 0x90},
	{0x83, 0x30},
	{0x84, 0x2c},//*** Change 100406
	{0x85, 0x4b},//*** Change 100406 
	{0x89, 0x48},//BLC hold
	{0x90, 0x0a},//TIME_IN  11/100  _100318
	{0x91, 0x0a},//TIME_OUT 11/100  _100318
	{0x92, 0x48},//AG_IN
	{0x93, 0x40},//AG_OUT

	{0x98, 0x38},
	{0x99, 0x40}, //Out BLC
	{0xa0, 0x00}, //Dark BLC
	{0xa8, 0x40}, //Normal BLC

	//Page 2  Last Update 10_03_12
	{0x03, 0x02},
	{0x13, 0x40}, //*** ADD 100402 
	{0x14, 0x04}, //*** ADD 100402 
	{0x1a, 0x00}, //*** ADD 100402 
	{0x1b, 0x08}, //*** ADD 100402 
	{0x20, 0x33},
	{0x21, 0xaa},//*** Change 100402 
	{0x22, 0xa7},
	{0x23, 0x32},//*** Change 100405 
	{0x3b, 0x48},//*** ADD 100405 
	{0x50, 0x21}, //*** ADD 100406
	{0x52, 0xa2},
	{0x53, 0x0a},
	{0x54, 0x30},//*** ADD 100405 
	{0x55, 0x10},//*** Change 100402 
	{0x56, 0x0c},
	{0x59, 0x0F},//*** ADD 100405 

	{0x60, 0x54},
	{0x61, 0x5d},
	{0x62, 0x56},
	{0x63, 0x5c},
	{0x64, 0x56},
	{0x65, 0x5c},
	{0x72, 0x57},
	{0x73, 0x5b},
	{0x74, 0x57},
	{0x75, 0x5b},
	{0x80, 0x02},
	{0x81, 0x46},
	{0x82, 0x07},
	{0x83, 0x10},
	{0x84, 0x07},
	{0x85, 0x10},
	{0x92, 0x24},
	{0x93, 0x30},
	{0x94, 0x24},
	{0x95, 0x30},
	{0xa0, 0x03},
	{0xa1, 0x45},
	{0xa4, 0x45},
	{0xa5, 0x03},
	{0xa8, 0x12},
	{0xa9, 0x20},
	{0xaa, 0x34},
	{0xab, 0x40},
	{0xb8, 0x55},
	{0xb9, 0x59},
	{0xbc, 0x05},
	{0xbd, 0x09},
	{0xc0, 0x5f},
	{0xc1, 0x67},
	{0xc2, 0x5f},
	{0xc3, 0x67},
	{0xc4, 0x60},
	{0xc5, 0x66},
	{0xc6, 0x60},
	{0xc7, 0x66},
	{0xc8, 0x61},
	{0xc9, 0x65},
	{0xca, 0x61},
	{0xcb, 0x65},
	{0xcc, 0x62},
	{0xcd, 0x64},
	{0xce, 0x62},
	{0xcf, 0x64},
	{0xd0, 0x53},
	{0xd1, 0x68},

	//Page 10
	{0x03, 0x10},
	{0x10, 0x03}, //03, //ISPCTL1, YUV ORDER(FIX)

	{0x11, 0x43},
	{0x12, 0x30}, //Y offet, dy offseet enable
	{0x40, 0x80},
	{0x41, 0x00}, //00 DYOFS  00->10  _100318
	{0x48, 0x8a}, //Contrast  88->84  _100318
	{0x50, 0x48}, //AGBRT

	{0x60, 0x7f},
	{0x61, 0x00}, //Use default
	{0x62, 0x95},
	{0x63, 0x95},//
	{0x64, 0x48}, //AGSAT
	{0x66, 0x90}, //wht_th2
	{0x67, 0x36}, //wht_gain  Dark (0.4x), Normal (0.75x)

	//LPF
	{0x03, 0x11},
	{0x10, 0x25},//LPF_CTL1 //0x01
	{0x11, 0x1f},//Test Setting
	{0x20, 0x00},//LPF_AUTO_CTL
	{0x21, 0x38},//LPF_PGA_TH
	{0x23, 0x0a},//LPF_TIME_TH
	{0x60, 0x10},//ZARA_SIGMA_TH //40->10
	{0x61, 0x82},
	{0x62, 0x00},//ZARA_HLVL_CTL
	{0x63, 0x83},//ZARA_LLVL_CTL
	{0x64, 0x83},//ZARA_DY_CTL

	{0x67, 0xF0},//*** Change 100402     //Dark
	{0x68, 0x30},//*** Change 100402     //Middle
	{0x69, 0x10},//High

	//2D
	{0x03, 0x12},
	{0x40, 0xe9},//YC2D_LPF_CTL1
	{0x41, 0x09},//YC2D_LPF_CTL2
	{0x50, 0x18},//Test Setting
	{0x51, 0x24},//Test Setting
	{0x70, 0x1f},//GBGR_CTL1 //0x1f
	{0x71, 0x00},//Test Setting
	{0x72, 0x00},//Test Setting
	{0x73, 0x00},//Test Setting
	{0x74, 0x10},//GBGR_G_UNIT_TH
	{0x75, 0x10},//GBGR_RB_UNIT_TH
	{0x76, 0x20},//GBGR_EDGE_TH
	{0x77, 0x80},//GBGR_HLVL_TH
	{0x78, 0x88},//GBGR_HLVL_COMP
	{0x79, 0x18},//Test Setting
	{0xb0, 0x7d},   //dpc m 

	//Edge
	{0x03, 0x13},
	{0x10, 0x01},
	{0x11, 0x89},
	{0x12, 0x14},
	{0x13, 0x19},
	{0x14, 0x08},//Test Setting
	{0x20, 0x05},//SHARP_Negative
	{0x21, 0x03},//SHARP_Positive
	{0x23, 0x30},//SHARP_DY_CTL
	{0x24, 0x33},//40->33
	{0x25, 0x08},//SHARP_PGA_TH
	{0x26, 0x18},//Test Setting
	{0x27, 0x00},//Test Setting
	{0x28, 0x08},//Test Setting
	{0x29, 0x50},//AG_TH
	{0x2a, 0xe0},//HI704_write_cmos_sensorion ratio
	{0x2b, 0x10},//Test Setting
	{0x2c, 0x28},//Test Setting
	{0x2d, 0x40},//Test Setting
	{0x2e, 0x00},//Test Setting
	{0x2f, 0x00},//Test Setting
	{0x30, 0x11},//Test Setting
	{0x80, 0x03},//SHARP2D_CTL
	{0x81, 0x07},//Test Setting
	{0x90, 0x07},//SHARP2D_SLOPE
	{0x91, 0x07},//SHARP2D_DIFF_CTL

	{0x92, 0x00},//SHARP2D_HI_CLIP
	{0x93, 0x20},//SHARP2D_DY_CTL
	{0x94, 0x42},//Test Setting
	{0x95, 0x60},//Test Setting

	//Shading
	{0x03, 0x14},
	{0x10, 0x00},//01
	{0x20, 0x60}, //XCEN
	{0x21, 0x80}, //YCEN
	{0x22, 0x66}, //76, 34, 2b
	{0x23, 0x50}, //4b, 15, 0d
	{0x24, 0x44}, //3b, 10, 0b

	//Page 15 CMC
	{0x03, 0x15}, 
	{0x10, 0x03},

	{0x14, 0x3c},
	{0x16, 0x2c},
	{0x17, 0x2f},

	{0x30, 0xc4},
	{0x31, 0x5b},
	{0x32, 0x1f},
	{0x33, 0x2a},
	{0x34, 0xce},
	{0x35, 0x24},
	{0x36, 0x0b},
	{0x37, 0x3f},
	{0x38, 0x8a},

	{0x40, 0x87},
	{0x41, 0x18},
	{0x42, 0x91},
	{0x43, 0x94},
	{0x44, 0x9f},
	{0x45, 0x33},
	{0x46, 0x00},
	{0x47, 0x94},
	{0x48, 0x14},

	//normal bright and clear
	{0x03,0x16},
	{0x30,0x00},
	{0x31,0x0b},
	{0x32,0x13},
	{0x33,0x20},
	{0x34,0x38},
	{0x35,0x4e},
	{0x36,0x62},
	{0x37,0x76},
	{0x38,0x88},
	{0x39,0x99},
	{0x3a,0xa9},
	{0x3b,0xc6},
	{0x3c,0xdd},
	{0x3d,0xf0},
	{0x3e,0xff},

	//Page 17 AE 
	{0x03, 0x17},
	{0xc4, 0x3c},

	//Page 20 AE 
	{0x03, 0x20},
	{0x10, 0x0c},
	{0x11, 0x04},

	{0x20, 0x01},
	{0x28, 0x27},
	{0x29, 0xa1},

	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},
	{0x30, 0x78},
	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x22}, //10_03_02 by Hubert 22 -> 23 ->22
	{0x3c, 0xde},

	{0x60, 0x95}, //d5, 99
	{0x68, 0x3c},
	{0x69, 0x64},
	{0x6A, 0x28},
	{0x6B, 0xc8},

	{0x70, 0x42},//Y Target 42

	{0x76, 0x22}, //Unlock bnd1
	{0x77, 0x02}, //Unlock bnd2

	{0x78, 0x12}, //Yth 1
	{0x79, 0x27}, //Yth 2 26 -> 27
	{0x7a, 0x23}, //Yth 3

	{0x7c, 0x1d}, // 1c -> 1d
	{0x7d, 0x22},
	//50Hz
	{0x83, 0x00}, //EXP Normal 33.33 fps 
	{0x84, 0x5f}, 
	{0x85, 0x37}, 
	{0x86, 0x00}, //EXPMin 3250.00 fps
	{0x87, 0xfa}, 
	{0x88, 0x01}, //EXP Max 10.00 fps 
	{0x89, 0x3d}, 
	{0x8a, 0x62}, 
	{0x8B, 0x1f}, //EXP100 
	{0x8C, 0xbd}, 
	{0x8D, 0x1a}, //EXP120 
	{0x8E, 0x5e}, 
	{0x91, 0x04},
	{0x92, 0x91},
	{0x93, 0xec},
	{0x94, 0x01}, //fix_step
	{0x95, 0xb7},
	{0x96, 0x74},
	{0x98, 0x8C},
	{0x99, 0x23},

	{0x9c, 0x06}, //EXP Limit 464.29 fps 
	{0x9d, 0xd6}, 
	{0x9e, 0x00}, //EXP Unit 
	{0x9f, 0xfa}, 
	{0xb1, 0x14},
	{0xb2, 0x50},
	{0xb4, 0x14},
	{0xb5, 0x38},
	{0xb6, 0x26},
	{0xb7, 0x20},
	{0xb8, 0x1d},
	{0xb9, 0x1b},
	{0xba, 0x1a},
	{0xbb, 0x19},
	{0xbc, 0x19},
	{0xbd, 0x18},

	{0xc0, 0x16},//0x1a->0x16
	{0xc3, 0x48},
	{0xc4, 0x48},

	//Page 22 AWB
	{0x03, 0x22},
	{0x10, 0xe2},
	{0x11, 0x26},
	{0x20, 0x34},
	{0x21, 0x40},

	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},
	{0x40, 0xf0},
	{0x41, 0x33},
	{0x42, 0x33},
	{0x43, 0xf3},
	{0x44, 0x55},
	{0x45, 0x44},
	{0x46, 0x02},

	{0x80, 0x45},
	{0x81, 0x20},
	{0x82, 0x48},
	{0x83, 0x52}, //RMAX Default : 50 -> 48 -> 52 
	{0x84, 0x1b}, //RMIN Default : 20
	{0x85, 0x50}, //BMAX Default : 50, 5a -> 58 -> 55
	{0x86, 0x25}, //BMIN Default : 20
	{0x87, 0x4d}, //RMAXB Default : 50, 4d
	{0x88, 0x38}, //RMINB Default : 3e, 45 --> 42
	{0x89, 0x3e}, //BMAXB Default : 2e, 2d --> 30
	{0x8a, 0x29}, //BMINB Default : 20, 22 --> 26 --> 29
	{0x8b, 0x02}, //OUT TH
	{0x8d, 0x22},
	{0x8e, 0x71},

	{0x8f, 0x63},
	{0x90, 0x60},
	{0x91, 0x5c},
	{0x92, 0x56},
	{0x93, 0x52},
	{0x94, 0x4c},
	{0x95, 0x36},
	{0x96, 0x31},
	{0x97, 0x2e},
	{0x98, 0x2a},
	{0x99, 0x29},
	{0x9a, 0x26},
	{0x9b, 0x09},

	{0x03, 0x22},
	{0x10, 0xfb},

	{0x03, 0x20},
	{0x10, 0x9c},

	//PAGE 0
	{0x03, 0x00},
	{0x01, 0xf0},//0xf1 ->0x41 : For Preview Green/Red Line.

	{0xff, 0xff},
};

static struct regval_list hi704_fmt_yuv422[] = {
	{0xff, 0xff},	/* END MARKER */
};

static struct regval_list hi704_win_vga[] = {

	{0x03, 0x00},
    {0x10, 0x00},        //VGA Size

    {0x40, 0x01},  //HBLANK
    {0x41, 0x58},
    {0x42, 0x00},  //VBLANK
    {0x43, 0x13},

    {0x03, 0x11},
    {0x10, 0x25},  

    {0x03, 0x20},	
    {0x83, 0x00},
    {0x84, 0xbe},
    {0x85, 0x6e},
    {0x86, 0x00},
    {0x87, 0xfa},

    {0x8b, 0x3f},
    {0x8c, 0x7a},
    {0x8d, 0x34},
    {0x8e, 0xbc},

    {0x9c, 0x06},
    {0x9d, 0xd6},
    {0x9e, 0x00},
    {0x9f, 0xfa},
    
	{0xff, 0xff}

};

static struct regval_list hi704_win_cif[] = {
	
	{0xff, 0xff}
};

static struct regval_list hi704_win_qvga[] = {

	{0x03, 0x00},
	{0x10, 0x10},
	
	{0xff, 0xff}
};

static struct regval_list hi704_win_qcif[] = {
	
	{0xff, 0xff}

};

static struct regval_list hi704_win_144x176[] = {

	{0xff, 0xff}

};

static struct regval_list hi704_brightness_l3[] = {
	{0x03, 0x10},	
	{0x40, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_brightness_l2[] = {
	{0x03, 0x10},	
	{0x40, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_brightness_l1[] = {
	{0x03, 0x10},	
	{0x40, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_brightness_h0[] = {
	{0x03, 0x10},	
	{0x40, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_brightness_h1[] = {
	{0x03, 0x10},	
	{0x40, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_brightness_h2[] = {
	{0x03, 0x10},	
	{0x40, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_brightness_h3[] = {
	{0x03, 0x10},	
	{0x40, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_contrast_l3[] = {
	{0x03, 0x10},	
	{0x48, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_contrast_l2[] = {
	{0x03, 0x10},	
	{0x48, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_contrast_l1[] = {
	{0x03, 0x10},	
	{0x48, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_contrast_h0[] = {
	{0x03, 0x10},	
	{0x48, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_contrast_h1[] = {
	{0x03, 0x10},	
	{0x48, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_contrast_h2[] = {
	{0x03, 0x10},	
	{0x48, 0x80},
	{0xff, 0xff}
};

static struct regval_list hi704_contrast_h3[] = {
	{0x03, 0x10},	
	{0x48, 0x80},
	{0xff, 0xff}
};

static int hi704_read(struct v4l2_subdev *sd, unsigned char reg,
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

static int hi704_write(struct v4l2_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int hi704_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret = 0;
	unsigned char val = 0x90;

	while (vals->reg_num != 0xff) {
		if (vals->reg_num == 0x11 && vals->value == HI704_REG_VAL_HVFLIP) {
			if (hi704_isp_parm.mirror)  
				val |= 0x01;
			else
				val &= 0xfe;

			if (hi704_isp_parm.flip) 
				val |= 0x02;
			else
				val &= 0xfd;

			vals->value = val; 
		}		
		ret = hi704_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int hi704_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int hi704_init(struct v4l2_subdev *sd, u32 val)
{
	struct hi704_info *info = container_of(sd, struct hi704_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;

	return hi704_write_array(sd, hi704_init_regs);
}

static int hi704_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = hi704_read(sd, 0x04, &v);
	printk("[hi704]-------------------ret=%d\n",ret);
	if (ret < 0)
		return ret;
	if (v != HI704_CHIP_ID)
		return -ENODEV;

    printk("This is HI704 sensor\n ");
	
	return 0;
}

static struct hi704_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} hi704_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= hi704_fmt_yuv422,
	},
};
#define N_HI704_FMTS ARRAY_SIZE(hi704_formats)

static struct hi704_win_size {
	int	width;
	int	height;
	struct regval_list *regs;
} hi704_win_sizes[] = {
	/* VGA */
	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= hi704_win_vga,
	},
#if 0
	/* CIF */
	{
		.width		= CIF_WIDTH,
		.height		= CIF_HEIGHT,
		.regs 		= hi704_win_cif,
	},
	/* QVGA */
	{
		.width		= QVGA_WIDTH,
		.height		= QVGA_HEIGHT,
		.regs 		= hi704_win_qvga,
	},
	/* QCIF */
	{
		.width		= QCIF_WIDTH,
		.height		= QCIF_HEIGHT,
		.regs 		= hi704_win_qcif,
	},
	/* 144x176 */
	{
		.width		= QCIF_HEIGHT,
		.height		= QCIF_WIDTH,
		.regs 		= hi704_win_144x176,
	},
#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(hi704_win_sizes))

static int hi704_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_HI704_FMTS)
		return -EINVAL;

	*code = hi704_formats[index].mbus_code;
	return 0;
}

static int hi704_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct hi704_format_struct **ret_fmt,
		struct hi704_win_size **ret_wsize)
{
	int index;
	struct hi704_win_size *wsize;

	for (index = 0; index < N_HI704_FMTS; index++)
		if (hi704_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_HI704_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = hi704_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = hi704_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = hi704_win_sizes; wsize < hi704_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= hi704_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = hi704_formats[index].colorspace;
	return 0;
}

static int hi704_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return hi704_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int hi704_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct hi704_info *info = container_of(sd, struct hi704_info, sd);
	struct hi704_format_struct *fmt_s;
	struct hi704_win_size *wsize;
	int ret;

	ret = hi704_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = hi704_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = hi704_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int hi704_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int hi704_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int hi704_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int hi704_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int hi704_frame_rates[] = { 30, 15, 10, 5, 1 };

static int hi704_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(hi704_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = hi704_frame_rates[interval->index];
	return 0;
}

static int hi704_enum_framesizes(struct v4l2_subdev *sd,
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
		struct hi704_win_size *win = &hi704_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int hi704_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int hi704_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[HI704]set brightness %d\n", brightness);

	return 0;
	switch (brightness) {
	case BRIGHTNESS_L3:
		hi704_write_array(sd, hi704_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		hi704_write_array(sd, hi704_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		hi704_write_array(sd, hi704_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		hi704_write_array(sd, hi704_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		hi704_write_array(sd, hi704_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		hi704_write_array(sd, hi704_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		hi704_write_array(sd, hi704_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int hi704_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk(KERN_DEBUG "[HI704]set contrast %d\n", contrast);

	return 0;
	switch (contrast) {
	case CONTRAST_L3:
		hi704_write_array(sd, hi704_contrast_l3);
		break;
	case CONTRAST_L2:
		hi704_write_array(sd, hi704_contrast_l2);
		break;
	case CONTRAST_L1:
		hi704_write_array(sd, hi704_contrast_l1);
		break;
	case CONTRAST_H0:
		hi704_write_array(sd, hi704_contrast_h0);
		break;
	case CONTRAST_H1:
		hi704_write_array(sd, hi704_contrast_h1);
		break;
	case CONTRAST_H2:
		hi704_write_array(sd, hi704_contrast_h2);
		break;
	case CONTRAST_H3:
		hi704_write_array(sd, hi704_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int hi704_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hi704_info *info = container_of(sd, struct hi704_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&hi704_isp_setting;
		isp_setting.size = ARRAY_SIZE(hi704_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&hi704_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int hi704_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hi704_info *info = container_of(sd, struct hi704_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = hi704_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = hi704_set_contrast(sd, ctrl->value);
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

static int hi704_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_HI704, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int hi704_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = hi704_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int hi704_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	hi704_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops hi704_core_ops = {
	.g_chip_ident = hi704_g_chip_ident,
	.g_ctrl = hi704_g_ctrl,
	.s_ctrl = hi704_s_ctrl,
	.queryctrl = hi704_queryctrl,
	.reset = hi704_reset,
	.init = hi704_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = hi704_g_register,
	.s_register = hi704_s_register,
#endif
};

static const struct v4l2_subdev_video_ops hi704_video_ops = {
	.enum_mbus_fmt = hi704_enum_mbus_fmt,
	.try_mbus_fmt = hi704_try_mbus_fmt,
	.s_mbus_fmt = hi704_s_mbus_fmt,
	.cropcap = hi704_cropcap,
	.g_crop	= hi704_g_crop,
	.s_parm = hi704_s_parm,
	.g_parm = hi704_g_parm,
	.enum_frameintervals = hi704_enum_frameintervals,
	.enum_framesizes = hi704_enum_framesizes,
};

static const struct v4l2_subdev_ops hi704_ops = {
	.core = &hi704_core_ops,
	.video = &hi704_video_ops,
};

static int hi704_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct hi704_info *info;
	struct comip_camera_subdev_platform_data *hi704_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct hi704_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &hi704_ops);

	/* Make sure it's an hi704 */
	ret = hi704_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an hi704 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "hi704 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	hi704_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (hi704_setting) {
		if (hi704_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			hi704_isp_parm.mirror = 1;
		else
			hi704_isp_parm.mirror = 0;

		if (hi704_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			hi704_isp_parm.flip = 1;
		else
			hi704_isp_parm.flip = 0;
	}

	return 0;
}

static int hi704_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct hi704_info *info = container_of(sd, struct hi704_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id hi704_id[] = {
	{ "hi704", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hi704_id);

static struct i2c_driver hi704_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "hi704",
	},
	.probe		= hi704_probe,
	.remove		= hi704_remove,
	.id_table	= hi704_id,
};

static __init int init_hi704(void)
{
	return i2c_add_driver(&hi704_driver);
}

static __exit void exit_hi704(void)
{
	i2c_del_driver(&hi704_driver);
}

fs_initcall(init_hi704);
module_exit(exit_hi704);

MODULE_DESCRIPTION("A low-level driver for GalaxyCore hi704 sensors");
MODULE_LICENSE("GPL");
