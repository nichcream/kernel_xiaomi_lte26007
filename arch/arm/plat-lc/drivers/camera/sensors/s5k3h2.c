
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
#include "s5k3h2.h"

#define S5K3H2_CHIP_ID_H	(0x38)
#define S5K3H2_CHIP_ID_L	(0x2B)

#define SXGA_WIDTH		1632
#define SXGA_HEIGHT		1232
#define FULL_HD_WIDTH	3264
#define FULL_HD_HEIGHT	1836
#define MAX_WIDTH		3264
#define MAX_HEIGHT		2448//1836
#define MAX_PREVIEW_WIDTH	MAX_WIDTH//SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT//SXGA_HEIGHT

#define S5K3H2_REG_END		0xfffe
#define S5K3H2_REG_DELAY	0xffff

#define S5K3H2_REG_VAL_HVFLIP   0xff
#define S5K3H2_USE_AWB_OTP
#define S5K3H2_USE_LENC_OTP

#define S5K3H2_FULLHD_USING_FULLSIZE_CROP	(1)


static char* factory_oflim = "oflim";

//#define S5K3H2_DEBUG_FUNC


struct s5k3h2_format_struct;
struct s5k3h2_info {
	struct v4l2_subdev sd;
	struct s5k3h2_format_struct *fmt;
	struct s5k3h2_win_size *win;
	int binning;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct s5k3h2_exp_ratio_entry {
	unsigned int width_last;
	unsigned int height_last;
	unsigned int width_cur;
	unsigned int height_cur;
	unsigned int exp_ratio;
};

struct s5k3h2_exp_ratio_entry s5k3h2_exp_ratio_table[] = {
	{SXGA_WIDTH, 	SXGA_HEIGHT, 	FULL_HD_WIDTH, 	FULL_HD_HEIGHT, 0x0f4},
	{SXGA_WIDTH, 	SXGA_HEIGHT, 	MAX_WIDTH, 		MAX_HEIGHT, 	0x118},
	{FULL_HD_WIDTH, FULL_HD_HEIGHT, SXGA_WIDTH, 	SXGA_HEIGHT, 	0x10c},
	{FULL_HD_WIDTH, FULL_HD_HEIGHT, MAX_WIDTH, 		MAX_HEIGHT, 	0x125},
	{MAX_WIDTH, 	MAX_HEIGHT, 	SXGA_WIDTH, 	SXGA_HEIGHT,	0x0ea},
	{MAX_WIDTH, 	MAX_HEIGHT, 	FULL_HD_WIDTH, 	FULL_HD_HEIGHT,	0x0df},
};
#define N_EXP_RATIO_TABLE_SIZE  (ARRAY_SIZE(s5k3h2_exp_ratio_table))

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals);
static int sensor_get_isp_win_setting(int width, int height, void **vals);
static int sensor_get_lens_threshold(void **v, int input_widht, int input_height, int outdoor);

static struct regval_list s5k3h2_init_regs[] = {
	{0x0100,0x00},    //streamin off
	{0x0103,0x01},    // sw rstn
	{S5K3H2_REG_DELAY, 20},  // MIPI Setting
#ifdef S5K3H2_USE_LENC_OTP
	{0x3210,0x81},  // shade clock enable
	{0x3200,0x08},  // shade decompression disable
#endif
	//Mipi setting
	{0x3065,0x35},
	{0x310E,0x00},
	{0x3098,0xAB},
	{0x30C7,0x0A},
	{0x309A,0x01},
	{0x310D,0xC6},
	{0x30C3,0x40},
	{0x30BB,0x02},
	{0x30BC,0x3C}, // 26Mhz
	{0x30BD,0xF0},
	{0x3110,0x79},
	{0x3111,0xE0},
	{0x3112,0x86},
	{0x3113,0x10},
	{0x30C7,0x1A},


	//T&P
	{0x3000,0x08},
	{0x3001,0x05},
	{0x3002,0x0D},
	{0x3003,0x21},
	{0x3004,0x62},
	{0x3005,0x0B},
	{0x3006,0x6D},
	{0x3007,0x02},
	{0x3008,0x62},
	{0x3009,0x62},
	{0x300A,0x41},
	{0x300B,0x10},
	{0x300C,0x21},
	{0x300D,0x04},
	{0x307E,0x03},
	{0x307F,0xA5},
	{0x3080,0x04},
	{0x3081,0x29},
	{0x3082,0x03},
	{0x3083,0x21},
	{0x3011,0x5F},
	{0x3156,0xE2},
	{0x3027,0xBE},
	{0x300f,0x02},
	{0x3010,0x10},
	{0x3017,0x74},
	{0x3018,0x00},
	{0x3020,0x02},
	{0x3021,0x00},
	{0x3023,0x80},
	{0x3024,0x08},
	{0x3025,0x08},
	{0x301C,0xD4},
	{0x315D,0x00},
	{0x3054,0x00},
	{0x3055,0x35},
	{0x3062,0x04},
	{0x3063,0x38},
	{0x31A4,0x04},
	{0x3016,0x54},
	{0x3157,0x02},
	{0x3158,0x00},
	{0x315B,0x02},
	{0x315C,0x00},
	{0x301B,0x05},
	{0x3028,0x41},
	{0x302A,0x10},
	{0x3060,0x00},
	{0x302D,0x19},
	{0x302B,0x05},
	{0x3072,0x13},
	{0x3073,0x21},
	{0x3074,0x82},
	{0x3075,0x20},
	{0x3076,0xA2},
	{0x3077,0x02},
	{0x3078,0x91},
	{0x3079,0x91},
	{0x307A,0x61},
	{0x307B,0x28},
	{0x307C,0x31},
	{0x304E,0x40},
	{0x304F,0x01},
	{0x3050,0x00},
	{0x3088,0x01},
	{0x3089,0x00},
	//{0x3210,0x01},
	{0x3211,0x00},
	{0x308E,0x01},
	{0x308F,0x8F},
	{0x3064,0x03},
	{0x31A7,0x0F},

	//PLL
	{0x0305,0x05},	//05},  // pre_pll_clk_div
	{0x0306,0x00},	//00},  // pll_multiplier MSB
	{0x0307,0xB6},	//AF}, // pll_multiplier LSB
	{0x0303,0x01},	//01}, // vt_sys_clk_div
	{0x0301,0x05},	//05}, // vt_pix_clk_div
	{0x030B,0x01},	//01}, // op_sys_clk_div
	{0x0309,0x05},	//05}, // op_pix_clk_div
	{0x30CC,0xE0},	//E0}, // DPHY_band_ctrl
	{0x31A1,0x5A},	//5A}, // DIV_G CLK

	{0x0344,0x00},
	{0x0345,0x00},
	{0x0346,0x00},
	{0x0347,0x00},
	{0x0348,0x0C},
	{0x0349,0xCD},
	{0x034A,0x09},
	{0x034B,0x9F},
	{0x0381,0x01},
	{0x0383,0x03},
	{0x0385,0x01},
	{0x0387,0x03},
	{0x0401,0x00},
	{0x0405,0x10},
	{0x0700,0x05},
	{0x0701,0x30},
	{0x034C,0x06},
	{0x034D,0x60}, //1632
	{0x034E,0x04},
	{0x034F,0xD0}, //1232
	{0x0200,0x02},
	{0x0201,0x50},
#if 0
	{0x0342,0x0D}, //sxga //line length
	{0x0343,0x8E},
	{0x0202,0x04},
	{0x0203,0xDB},
	{0x0204,0x00},
	{0x0205,0x20},
	{0x0340,0x07}, //frame length
	{0x0341,0x10},
#endif
#if 0
	{0x0342,0x0D}, //full_hd //line length
	{0x0343,0x7E},
	{0x0202,0x04},
	{0x0203,0xDB},
	{0x0204,0x00},
	{0x0205,0x20},
	{0x0340,0x07}, //frame length
	{0x0341,0x3c},
#endif
#if 1
	{0x0342, 0x0D}, //8M //line length
	{0x0343, 0x8E},
	{0x0202, 0x04},
	{0x0203, 0xE7},
	{0x0204, 0x00},
	{0x0205, 0x20},
	{0x0340, 0x09}, //frame length
	{0x0341, 0xA0},
#endif

	{0x300E,0x2D},
	{0x31A3,0x40},
	{0x301A,0xA7},
	{0x3053,0xCF},
#ifdef S5K3H2_USE_LENC_OTP
	{0x3202,0x34}, //sh4ch_blk_width
	{0x3203,0x4D}, //sh4ch_blk_height
	{0x3204,0x04},
	{0x3205,0xEC},
	{0x3206,0x03},
	{0x3207,0x54},

	{S5K3H2_REG_DELAY, 1},
	{0x3201,0x00},           //Shadig On
	{S5K3H2_REG_DELAY, 10},

	//{0x3210,0x81},         // shade clock enable

	{0x3200,0x28},  // shade decompression auto start
	{0x3A2D,0x00}, // Auto Read Off
	{0x3A2D,0x01}, // Auto Read ON
#endif

	{S5K3H2_REG_END, 0x00}, /* END MARKER */

};

static struct regval_list s5k3h2_stream_on[] = {
//	{0x0100, 0x01},
	{S5K3H2_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list s5k3h2_stream_off[] = {
//	{0x0100, 0x00},
	{S5K3H2_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list s5k3h2_win_sxga[] = {

	{0x0100,0x00},	  //streamin off

	//PLL
#if 0
	{0x0305,0x05},	// pre_pll_clk_div
	{0x0306,0x00},	// pll_multiplier MSB
	{0x0307,0xAF}, // pll_multiplier LSB
	{0x0303,0x01}, // vt_sys_clk_div
	{0x0301,0x05}, // vt_pix_clk_div
	{0x030B,0x01}, // op_sys_clk_div
	{0x0309,0x05}, // op_pix_clk_div
	{0x30CC,0xE0}, // DPHY_band_ctrl
	{0x31A1,0x5A}, // DIV_G CLK
#endif
	{0x0344,0x00},
	{0x0345,0x00},
	{0x0346,0x00},
	{0x0347,0x00},
	{0x0348,0x0C},
	{0x0349,0xCD},
	{0x034A,0x09},
	{0x034B,0x9F},
	{0x0381,0x01},
	{0x0383,0x03},
	{0x0385,0x01},
	{0x0387,0x03},
	{0x0401,0x00},
	{0x0405,0x10},
	{0x0700,0x05},
	{0x0701,0x30},
	{0x034C,0x06},
	{0x034D,0x60},	//1632
	{0x034E,0x04},
	{0x034F,0xD0},	//1232
	{0x0200,0x02},
	{0x0201,0x50},
#if 0
	{0x0202,0x04},
	{0x0203,0xDB},
	{0x0204,0x00},
	{0x0205,0x20},
	{0x0340,0x07}, //frame length
	{0x0341,0x10},
#endif

	{0x0342,0x0D}, //line length
	{0x0343,0x8E},

	{0x300E,0x2D},
	{0x31A3,0x40},
	{0x301A,0xA7},
	{0x3053,0xCF},
#ifdef S5K3H2_USE_LENC_OTP
	{0x3202,0x34}, //sh4ch_blk_width
	{0x3203,0x4D}, //sh4ch_blk_height
	{0x3204,0x04},
	{0x3205,0xEC},
	{0x3206,0x03},
	{0x3207,0x54},

//	{S5K3H2_REG_DELAY, 1},
#endif

	{0x0100,0x01},
	{S5K3H2_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list s5k3h2_win_full_hd[] = {

	{0x0100,0x00},	  //streamin off

	{0x0344,0x00},
	{0x0345,0x08},	// x_addr_start (146d)
	{0x0346,0x01},
	{0x0347,0x3a}, // y_addr_start (392d)
	{0x0348,0x0C},
	{0x0349,0xc7}, // x_addr_end (3133d)
	{0x034A,0x08},
	{0x034B,0x65}, // y_addr_end (2071d)
	{0x0381,0x01},
	{0x0383,0x01},
	{0x0385,0x01},
	{0x0387,0x01},
	{0x0401,0x00},
	{0x0405,0x10},
	{0x0700,0x05},
	{0x0701,0x30},
	{0x034C,0x0C},
	{0x034D,0xC0}, //1920(3264)
	{0x034E,0x07},
	{0x034F,0x2C}, //1080(1836)
	{0x0200,0x02},
	{0x0201,0x50},
#if 0
	{0x0202,0x04},
	{0x0203,0xE7},
	{0x0204,0x00},
	{0x0205,0x40},
	{0x0340,0x07}, //frame length
	{0x0341,0x10},
#endif
	{0x0342,0x0D}, //line length
	{0x0343,0x7E},

	{0x300E,0x29},
	{0x31A3,0x00},
	{0x301A,0xA7},
	{0x3053,0xCB},
#ifdef S5K3H2_USE_LENC_OTP
	{0x3202,0x66}, //sh4ch_blk_width
	{0x3203,0x73},  //sh4ch_blk_height
	{0x3204,0x02},
	{0x3205,0x7F},
	{0x3206,0x02},
	{0x3207,0x39},

//	{S5K3H2_REG_DELAY, 1},
#endif
	{0x0100,0x01},
	{S5K3H2_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list s5k3h2_win_8m[] = {
	{0x0100, 0x00},    //streamin off

#if 0
	{0x0305,0x05},  // pre_pll_clk_div
	{0x0306,0x00},  // pll_multiplier MSB
	{0x0307,0xB6}, // pll_multiplier LSB
	{0x0303,0x01}, // vt_sys_clk_div
	{0x0301,0x05}, // vt_pix_clk_div
	{0x030B,0x01}, // op_sys_clk_div
	{0x0309,0x05}, // op_pix_clk_div
	{0x30CC,0xE0}, // DPHY_band_ctrl
	{0x31A1,0x5B}, // DIV_G CLK
#endif
	    //Size setting
	{0x0344, 0x00},
	{0x0345, 0x08}, // x_addr_start (8d)
	{0x0346, 0x00},
	{0x0347, 0x08}, // y_addr_start (8d)
	{0x0348, 0x0C},
	{0x0349, 0xC7}, // x_addr_end (3271d)
	{0x034A, 0x09},
	{0x034B, 0x97}, // y_addr_end (2455d)
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x0700, 0x05},
	{0x0701, 0x30},
	{0x034C, 0x0C},
	{0x034D, 0xC0}, //3264
	{0x034E, 0x09},
	{0x034F, 0x90}, //2448//2464
	{0x0200, 0x02},
	{0x0201, 0x50},
#if 0
	{0x0202, 0x04},
	{0x0203, 0xE7},
	{0x0204, 0x00},
	{0x0205, 0x20},
	{0x0340, 0x09}, //frame length
	{0x0341, 0xA0},
#endif

	{0x0342, 0x0D}, //line length
	{0x0343, 0x8E},

	{0x300E, 0x29},
	{0x31A3, 0x00},
	{0x301A, 0xA7},
	{0x3053, 0xCF},

#ifdef S5K3H2_USE_LENC_OTP
	{0x3202,0x67}, //sh4ch_blk_width
	{0x3203,0x9A}, //sh4ch_blk_height
	{0x3204,0x02},
	{0x3205,0x7C},
	{0x3206,0x01},
	{0x3207,0xAA},

	//    {S5K3H2_REG_DELAY, 1},
#endif
	{0x0100, 0x01},

	{S5K3H2_REG_END, 0x00},      /* END MARKER */
};

static int sensor_get_vcm_range(void **vals, int val)
{
	printk(KERN_DEBUG"*sensor_get_vcm_range\n");
	switch (val) {
	case FOCUS_MODE_AUTO:
		*vals = focus_mode_normal;
		return ARRAY_SIZE(focus_mode_normal);
	case FOCUS_MODE_INFINITY:
		*vals = focus_mode_infinity;
		return ARRAY_SIZE(focus_mode_infinity);
	case FOCUS_MODE_MACRO:
		*vals = focus_mode_macro;
		return ARRAY_SIZE(focus_mode_macro);
	case FOCUS_MODE_FIXED:
	case FOCUS_MODE_EDOF:
	case FOCUS_MODE_CONTINUOUS_VIDEO:
	case FOCUS_MODE_CONTINUOUS_PICTURE:
	case FOCUS_MODE_CONTINUOUS_AUTO:
		*vals = focus_mode_normal;
		return ARRAY_SIZE(focus_mode_normal);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_exposure(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L6:
		*vals = isp_exposure_l6;
		return ARRAY_SIZE(isp_exposure_l6);
	case EXPOSURE_L5:
		*vals = isp_exposure_l5;
		return ARRAY_SIZE(isp_exposure_l5);
	case EXPOSURE_L4:
		*vals = isp_exposure_l4;
		return ARRAY_SIZE(isp_exposure_l4);
	case EXPOSURE_L3:
		*vals = isp_exposure_l3;
		return ARRAY_SIZE(isp_exposure_l3);
	case EXPOSURE_L2:
		*vals = isp_exposure_l2;
		return ARRAY_SIZE(isp_exposure_l2);
	case EXPOSURE_L1:
		*vals = isp_exposure_l1;
		return ARRAY_SIZE(isp_exposure_l1);
	case EXPOSURE_H0:
		*vals = isp_exposure_h0;
		return ARRAY_SIZE(isp_exposure_h0);
	case EXPOSURE_H1:
		*vals = isp_exposure_h1;
		return ARRAY_SIZE(isp_exposure_h1);
	case EXPOSURE_H2:
		*vals = isp_exposure_h2;
		return ARRAY_SIZE(isp_exposure_h2);
	case EXPOSURE_H3:
		*vals = isp_exposure_h3;
		return ARRAY_SIZE(isp_exposure_h3);
	case EXPOSURE_H4:
		*vals = isp_exposure_h4;
		return ARRAY_SIZE(isp_exposure_h4);
	case EXPOSURE_H5:
		*vals = isp_exposure_h5;
		return ARRAY_SIZE(isp_exposure_h5);
	case EXPOSURE_H6:
		*vals = isp_exposure_h6;
		return ARRAY_SIZE(isp_exposure_h6);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_iso(void **vals, int val)
{
	switch(val) {
	case ISO_100:
		*vals = isp_iso_100;
		return ARRAY_SIZE(isp_iso_100);
	case ISO_200:
		*vals = isp_iso_200;
		return ARRAY_SIZE(isp_iso_200);
	case ISO_400:
		*vals = isp_iso_400;
		return ARRAY_SIZE(isp_iso_400);
	case ISO_800:
		*vals = isp_iso_800;
		return ARRAY_SIZE(isp_iso_800);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_contrast(void **vals, int val)
{
	switch (val) {
	case CONTRAST_L4:
	case CONTRAST_L3:
		*vals = isp_contrast_l3;
		return ARRAY_SIZE(isp_contrast_l3);
	case CONTRAST_L2:
		*vals = isp_contrast_l2;
		return ARRAY_SIZE(isp_contrast_l2);
	case CONTRAST_L1:
		*vals = isp_contrast_l1;
		return ARRAY_SIZE(isp_contrast_l1);
	case CONTRAST_H0:
		*vals = isp_contrast_h0;
		return ARRAY_SIZE(isp_contrast_h0);
	case CONTRAST_H1:
		*vals = isp_contrast_h1;
		return ARRAY_SIZE(isp_contrast_h1);
	case CONTRAST_H2:
		*vals = isp_contrast_h2;
		return ARRAY_SIZE(isp_contrast_h2);
	case CONTRAST_H3:
		*vals = isp_contrast_h3;
		return ARRAY_SIZE(isp_contrast_h3);
	case CONTRAST_H4:
		*vals = isp_contrast_h3;
		return ARRAY_SIZE(isp_contrast_h3);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_l2;
		return ARRAY_SIZE(isp_saturation_l2);
	case SATURATION_L1:
		*vals = isp_saturation_l1;
		return ARRAY_SIZE(isp_saturation_l1);
	case SATURATION_H0:
		*vals = isp_saturation_h0;
		return ARRAY_SIZE(isp_saturation_h0);
	case SATURATION_H1:
		*vals = isp_saturation_h1;
		return ARRAY_SIZE(isp_saturation_h1);
	case SATURATION_H2:
		*vals = isp_saturation_h2;
		return ARRAY_SIZE(isp_saturation_h2);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_ct_saturation(void **a, void **c, void **d, int val)
{
	switch (val) {
	case SATURATION_L2:
		*a = isp_a_saturation_l2;
		*c = isp_c_saturation_l2;
		*d = isp_d_saturation_l2;
		return ARRAY_SIZE(isp_a_saturation_l2);
	case SATURATION_L1:
		*a = isp_a_saturation_l1;
		*c = isp_c_saturation_l1;
		*d = isp_d_saturation_l1;
		return ARRAY_SIZE(isp_a_saturation_l1);
	case SATURATION_H0:
		*a = isp_a_saturation_h0;
		*c = isp_c_saturation_h0;
		*d = isp_d_saturation_h0;
		return ARRAY_SIZE(isp_a_saturation_h0);
	case SATURATION_H1:
		*a = isp_a_saturation_h1;
		*c = isp_c_saturation_h1;
		*d = isp_d_saturation_h1;
		return ARRAY_SIZE(isp_a_saturation_h1);
	case SATURATION_H2:
		*a = isp_a_saturation_h2;
		*c = isp_c_saturation_h2;
		*d = isp_d_saturation_h2;
		return ARRAY_SIZE(isp_a_saturation_h2);
	default:
		return -EINVAL;
	}

	return 0;
}


static int sensor_get_white_balance(void **vals, int val)
{
	switch (val) {
	case WHITE_BALANCE_AUTO:
		*vals = isp_white_balance_auto;
		return ARRAY_SIZE(isp_white_balance_auto);
	case WHITE_BALANCE_INCANDESCENT:
		*vals = isp_white_balance_incandescent;
		return ARRAY_SIZE(isp_white_balance_incandescent);
	case WHITE_BALANCE_FLUORESCENT:
		*vals = isp_white_balance_fluorescent;
		return ARRAY_SIZE(isp_white_balance_fluorescent);
	case WHITE_BALANCE_WARM_FLUORESCENT:
		*vals = isp_white_balance_warm_fluorescent;
		return ARRAY_SIZE(isp_white_balance_warm_fluorescent);
	case WHITE_BALANCE_DAYLIGHT:
		*vals = isp_white_balance_daylight;
		return ARRAY_SIZE(isp_white_balance_daylight);
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		*vals = isp_white_balance_cloudy_daylight;
		return ARRAY_SIZE(isp_white_balance_cloudy_daylight);
	case WHITE_BALANCE_TWILIGHT:
		*vals = isp_white_balance_twilight;
		return ARRAY_SIZE(isp_white_balance_twilight);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade;
		return ARRAY_SIZE(isp_white_balance_shade);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_brightness(void **vals, int val)
{
	switch (val) {
	case BRIGHTNESS_L4:
	case BRIGHTNESS_L3:
		*vals = isp_brightness_l3;
		return ARRAY_SIZE(isp_brightness_l3);
	case BRIGHTNESS_L2:
		*vals = isp_brightness_l2;
		return ARRAY_SIZE(isp_brightness_l2);
	case BRIGHTNESS_L1:
		*vals = isp_brightness_l1;
		return ARRAY_SIZE(isp_brightness_l1);
	case BRIGHTNESS_H0:
		*vals = isp_brightness_h0;
		return ARRAY_SIZE(isp_brightness_h0);
	case BRIGHTNESS_H1:
		*vals = isp_brightness_h1;
		return ARRAY_SIZE(isp_brightness_h1);
	case BRIGHTNESS_H2:
		*vals = isp_brightness_h2;
		return ARRAY_SIZE(isp_brightness_h2);
	case BRIGHTNESS_H3:
		*vals = isp_brightness_h3;
		return ARRAY_SIZE(isp_brightness_h3);
	case BRIGHTNESS_H4:
		*vals = isp_brightness_h3;
		return ARRAY_SIZE(isp_brightness_h3);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_sharpness(void **vals, int val)
{
	switch (val) {
	case SHARPNESS_L2:
		*vals = isp_sharpness_l2;
		return ARRAY_SIZE(isp_sharpness_l2);
	case SHARPNESS_L1:
		*vals = isp_sharpness_l1;
		return ARRAY_SIZE(isp_sharpness_l1);
	case SHARPNESS_H0:
		*vals = isp_sharpness_h0;
		return ARRAY_SIZE(isp_sharpness_h0);
	case SHARPNESS_H1:
		*vals = isp_sharpness_h1;
		return ARRAY_SIZE(isp_sharpness_h1);
	case SHARPNESS_H2:
		*vals = isp_sharpness_h2;
		return ARRAY_SIZE(isp_sharpness_h2);
	case SHARPNESS_H3:
	case SHARPNESS_H4:
	case SHARPNESS_H5:
	case SHARPNESS_H6:
	case SHARPNESS_H7:
	case SHARPNESS_H8:
		*vals = isp_sharpness_h2;
		return ARRAY_SIZE(isp_sharpness_h2);
	default:
		return -EINVAL;
	}

	return 0;
}

static struct isp_effect_ops  sensor_effect_ops = {
	.get_iso = sensor_get_iso,
	.get_contrast = sensor_get_contrast,
	.get_saturation = sensor_get_saturation,
	.get_ct_saturation = sensor_get_ct_saturation,
	.get_lens_threshold = sensor_get_lens_threshold,
	.get_white_balance = sensor_get_white_balance,
	.get_brightness = sensor_get_brightness,
	.get_sharpness = sensor_get_sharpness,
	.get_vcm_range = sensor_get_vcm_range,
	.get_exposure = sensor_get_exposure,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,
	.get_isp_win_setting = sensor_get_isp_win_setting,
};

static int s5k3h2_read(struct v4l2_subdev *sd, unsigned short reg,
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
#ifdef S5K3H2_DEBUG_FUNC
static int s5k3h2_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	//return 0;
	while (vals->reg_num != S5K3H2_REG_END) {
		if (vals->reg_num == S5K3H2_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {

			ret = s5k3h2_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk(KERN_DEBUG"reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

static int s5k3h2_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int s5k3h2_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char hvflip = 0;

	while (vals->reg_num != S5K3H2_REG_END) {
		if (vals->reg_num == S5K3H2_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
#if 0
				msleep(vals->value);
			else
#endif
				mdelay(vals->value);
		} else {
			if (vals->reg_num == 0x0101
			    && vals->value == S5K3H2_REG_VAL_HVFLIP) {
				if (s5k3h2_isp_parm.flip)
					hvflip &= 0xfd;
				else
					hvflip |= 0x02;

				if (s5k3h2_isp_parm.mirror)
					hvflip |= 0x01;
				else
					hvflip &= 0xfe;


				ret = s5k3h2_write(sd, vals->reg_num, hvflip);
				if (ret < 0)
					return ret;
				vals++;
				continue;
			}
			ret = s5k3h2_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#ifdef S5K3H2_USE_AWB_OTP

#define BYTE			unsigned char
#define Sleep(ms)		mdelay(ms)

#define Oflim_ID           0x07
#define VALID_OTP          0x00

#define GAIN_DEFAULT       0x0100
#define GAIN_GREEN1_ADDR   0x020E
#define GAIN_BLUE_ADDR     0x0212
#define GAIN_RED_ADDR      0x0210
#define GAIN_GREEN2_ADDR   0x0214

unsigned short current_rg;
unsigned short current_bg;
unsigned short golden_rg;
unsigned short golden_bg;

static int s5k3h2_wordwrite(struct v4l2_subdev *sd, unsigned short reg,
		unsigned short value)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[4] = {reg >> 8, reg & 0xff, value >> 8,value & 0xff};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 4,
		.buf	= buf,
	};


	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret)
		printk(KERN_DEBUG"s5k3h2_wordwrite error!\n");

	return ret;
}



/*************************************************************************************************
* Function    :  start_read_otp
* Description :  before read otp , set the reading block setting
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
* Return      :  0, reading block setting err
                 1, reading block setting ok
**************************************************************************************************/
static int s5k3h2_start_read_otp(struct v4l2_subdev *sd, BYTE zone)
{
	s5k3h2_wordwrite(sd, 0xFCFC, 0xD000);
	s5k3h2_write(sd, 0x3A1C, 0x00);   //disable test mode
	s5k3h2_write(sd, 0x0A00, 0x04);   //make initial state
	s5k3h2_write(sd, 0x0A02, zone);   //set the page 10 of OTP
	s5k3h2_write(sd, 0x0A00, 0x01);   //set read mode

	mdelay(20);
	return 1;
}

/*************************************************************************************************
* Function    :  stop_read_otp
* Description :  after read otp , stop and reset otp block setting
**************************************************************************************************/
void s5k3h2_stop_read_otp(struct v4l2_subdev *sd)
{
	s5k3h2_write(sd, 0x0A00, 0x00);   //Reset the NVM interface by writing 00h to 0xD0000A00
}


/*************************************************************************************************
* Function    :  get_otp_flag
* Description :  get otp WRITTEN_FLAG
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
* Return      :  [BYTE], if 0x40 , this type has valid otp data, otherwise, invalid otp data
**************************************************************************************************/
static BYTE s5k3h2_get_otp_flag(struct v4l2_subdev *sd, BYTE zone)
{
	BYTE flag = 0;
	int ret;
	if(!s5k3h2_start_read_otp(sd, zone))
	{
		printk(KERN_DEBUG"Start read Page %d Fail!", zone);
		return 0;
	}
	ret = s5k3h2_read(sd,0x0A0B, &flag);
	if(ret)
		printk(KERN_DEBUG"Get otp flag error!\n");
	s5k3h2_stop_read_otp(sd);

	flag = flag;

	printk(KERN_DEBUG"Flag:0x%02x",flag );

	return flag;
}

/*************************************************************************************************
* Function    :  get_otp_module_id
* Description :  get otp MID value
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
* Return      :  [BYTE] 0 : OTP data fail
                 other value : module ID data , TRULY ID is 0x0001
**************************************************************************************************/
#if 0
static BYTE s5k3h2_get_otp_module_id(struct v4l2_subdev *sd, BYTE zone)
{
	BYTE module_id = 0;
	int ret;

	if(!s5k3h2_start_read_otp(sd, zone))
	{
		printk(KERN_DEBUG"Start read Page %d Fail!", zone);
		return 0;
	}

	ret = s5k3h2_read(sd, 0x0A0A, &module_id);
	if(ret)
		printk(KERN_DEBUG"Read otp module id error!\n");

	s5k3h2_stop_read_otp(sd);

	//printk(KERN_DEBUG"Module ID: 0x%02x.",module_id);

	return module_id;
}
#endif

/*************************************************************************************************
* Function    :  get_otp_lens_id
* Description :  get otp LENS_ID value
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
* Return      :  [BYTE] 0 : OTP data fail
                 other value : LENS ID data
**************************************************************************************************/
/*
BYTE get_otp_lens_id(BYTE zone)
{
	BYTE lens_id = 0;

	if(!start_read_otp(zone))
	{
		printk(KERN_DEBUG"Start read Page %d Fail!", zone);
		return 0;
	}
	lens_id = S5K3H2YXMIPI_read_cmos_sensor(0x0A0A);
	stop_read_otp();

	printk(KERN_DEBUG"Lens ID: 0x%02x.",lens_id);

	return lens_id;
}
*/

/*************************************************************************************************
* Function    :  get_otp_vcm_id
* Description :  get otp VCM_ID value
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
* Return      :  [BYTE] 0 : OTP data fail
                 other value : VCM ID data
**************************************************************************************************/

#if 0
static BYTE s5k3h2_get_otp_vcm_id(struct v4l2_subdev *sd, BYTE zone)
{
	BYTE vcm_id = 0;
	int ret = 0;

	if(!s5k3h2_start_read_otp(sd, zone))
	{
		printk(KERN_DEBUG"Start read Page %d Fail!", zone);
		return 0;
	}

	ret = s5k3h2_read(sd, 0x0A0C, &vcm_id );
	if(ret)
		printk(KERN_DEBUG"Read OTP VCM ID Error\n");

	s5k3h2_stop_read_otp(sd);

	return vcm_id;
}
#endif

/*************************************************************************************************
* Function    :  get_otp_driver_id
* Description :  get otp driver id value
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
* Return      :  [BYTE] 0 : OTP data fail
                 other value : driver ID data
**************************************************************************************************/
/*
BYTE get_otp_driver_id(BYTE zone)
{
	BYTE driver_id = 0;

	if(!start_read_otp(zone))
	{
		printk(KERN_DEBUG"Start read Page %d Fail!", zone);
		return 0;
	}

	driver_id = S5K3H2YXMIPI_read_cmos_sensor(0x0A0C);

	stop_read_otp();

	printk(KERN_DEBUG"Driver ID: 0x%02x.",driver_id);

	return driver_id;
}
*/
/*************************************************************************************************
* Function    :  get_light_id
* Description :  get otp environment light temperature value
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
* Return      :  [BYTE] 0 : OTP data fail
                        other value : driver ID data
						BIT0:D65(6500K) EN
						BIT1:D50(5100K) EN
						BIT2:CWF(4000K) EN
						BIT3:A Light(2800K) EN
**************************************************************************************************/
/*
BYTE get_light_id(BYTE zone)
{
	BYTE light_id = 0;

	if(!start_read_otp(zone))
	{
		printk(KERN_DEBUG"Start read Page %d Fail!", zone);
		return 0;
	}

	light_id = S5K3H2YXMIPI_read_cmos_sensor(0x0A0D);

	stop_read_otp();

	printk(KERN_DEBUG"Light ID: 0x%02x.",light_id);

	return light_id;
}
*/


/*************************************************************************************************
* Function    :  wb_gain_set
* Description :  Set WB ratio to register gain setting  512x
* Parameters  :  [int] r_ratio : R ratio data compared with golden module R
                       b_ratio : B ratio data compared with golden module B
* Return      :  [bool] 0 : set wb fail
                        1 : WB set success
**************************************************************************************************/
static bool s5k3h2_wb_gain_set(struct v4l2_subdev *sd, u32 r_ratio, u32 b_ratio)
{
	unsigned short R_GAIN;
	unsigned short B_GAIN;
	unsigned short Gr_GAIN;
	unsigned short Gb_GAIN;
	unsigned short G_GAIN;
	if(!r_ratio || !b_ratio)
	{
		printk(KERN_DEBUG"OTP WB ratio Data Err!");
		return 0;
	}

//	S5K3H2YXMIPI_wordwrite_cmos_sensor(GAIN_GREEN1_ADDR, GAIN_DEFAULT); //Green 1 default gain 1x
//	S5K3H2YXMIPI_wordwrite_cmos_sensor(GAIN_GREEN2_ADDR, GAIN_DEFAULT); //Green 2 default gain 1x

	if(r_ratio >= 512 ){
		if(b_ratio>=512){
			R_GAIN = (unsigned short)(GAIN_DEFAULT * r_ratio / 512);
			G_GAIN = GAIN_DEFAULT;
			B_GAIN = (unsigned short)(GAIN_DEFAULT * b_ratio / 512);
		}
		else {
			R_GAIN =  (unsigned short)(GAIN_DEFAULT * r_ratio / b_ratio);
			G_GAIN = (unsigned short)(GAIN_DEFAULT * 512 / b_ratio);
			B_GAIN = GAIN_DEFAULT;
		}
	}
	else {
		if(b_ratio >= 512){
			R_GAIN = GAIN_DEFAULT;
			G_GAIN =(unsigned short)(GAIN_DEFAULT * 512 / r_ratio);
			B_GAIN =(unsigned short)(GAIN_DEFAULT *  b_ratio / r_ratio);
		}
		else {
			Gr_GAIN = (unsigned short)(GAIN_DEFAULT * 512 / r_ratio );
			Gb_GAIN = (unsigned short)(GAIN_DEFAULT * 512 / b_ratio );
			if(Gr_GAIN >= Gb_GAIN){
				R_GAIN = GAIN_DEFAULT;
				G_GAIN = (unsigned short)(GAIN_DEFAULT * 512 / r_ratio );
				B_GAIN = (unsigned short)(GAIN_DEFAULT * b_ratio / r_ratio);
			}
	        else {
				R_GAIN =  (unsigned short)(GAIN_DEFAULT * r_ratio / b_ratio );
				G_GAIN = (unsigned short)(GAIN_DEFAULT * 512 / b_ratio );
				B_GAIN = GAIN_DEFAULT;
			}
		}
	}

	s5k3h2_wordwrite(sd, GAIN_RED_ADDR, R_GAIN);
	s5k3h2_wordwrite(sd, GAIN_BLUE_ADDR, B_GAIN);
	s5k3h2_wordwrite(sd, GAIN_GREEN1_ADDR, G_GAIN); //Green 1 default gain 1x
	s5k3h2_wordwrite(sd, GAIN_GREEN2_ADDR, G_GAIN); //Green 2 default gain 1x

	//printk(KERN_DEBUG"OTP WB Update Finished!");
	return 1;
}


/*************************************************************************************************
* Function    :  get_otp_wb
* Description :  Get WB data
* Parameters  :  [BYTE] zone : OTP PAGE index , 0x00~0x0f
**************************************************************************************************/
static bool s5k3h2_get_otp_wb(struct v4l2_subdev *sd, BYTE zone)
{
	BYTE temph = 0;
	BYTE templ = 0;
	int ret = 0;
	current_rg = 0;
	current_bg = 0;

	if(!s5k3h2_start_read_otp(sd, zone))
	{
		printk(KERN_DEBUG"Start read Page %d Fail!", zone);
		return 0;
	}
	golden_rg = 0x0324;//0x0346;  //0x0303
	golden_bg = 0x02a9;//0x027c;  //0x028B

	ret = s5k3h2_read(sd, 0x0A04, &temph);
	ret = s5k3h2_read(sd, 0x0A05, &templ);
	current_rg  = (unsigned short)templ + ((unsigned short)temph << 8);
	printk(KERN_DEBUG"get_otp_wb  0x0A04= 0x%02x , 0x0A05 =0x%02x  \n ",temph,templ);

	ret = s5k3h2_read(sd, 0x0A06, &temph);
	ret = s5k3h2_read(sd, 0x0A07, &templ);
	current_bg  = (unsigned short)templ + ((unsigned short)temph << 8);
	printk(KERN_DEBUG"get_otp_wb  0x0A06= 0x%02x , 0x0A07 =0x%02x  \n",temph,templ);

	s5k3h2_stop_read_otp(sd);

	return 0;
}
/*************************************************************************************************
* Function    :  otp_wb_update
* Description :  Update WB correction
* Return      :  [bool] 0 : OTP data fail
                        1 : otp_WB update success
**************************************************************************************************/
static int s5k3h2_otp_wb_update(struct v4l2_subdev *sd, BYTE zone)
{
	u32 r_ratio;
	u32 b_ratio;

	if(!s5k3h2_get_otp_wb(sd, zone))  // get wb data from otp
		return 0;

	if(!golden_rg || !golden_bg || !current_rg || !current_bg)
	{
		printk(KERN_DEBUG"WB update Err !");
		return 0;
	}

	r_ratio = 512 * (golden_rg) /(current_rg);
	b_ratio = 512 * (golden_bg) /(current_bg);

	s5k3h2_wb_gain_set(sd, r_ratio, b_ratio);

	//printk(KERN_DEBUG"WB update finished!");

	return 0;
}


/************************************************/
#if 0
static int s5k3h2_otp_3h2_read_MID_VCMID(struct v4l2_subdev *sd)
{
	BYTE zone = 0x10;
	BYTE FLG = 0x00;
	BYTE MID = 0x00;
	BYTE VCMID = 0x00;
	int i;

	for(i=0;i<3;i++)
	{
		FLG = s5k3h2_get_otp_flag(sd, zone);
		if(FLG == VALID_OTP)
			break;
		else
			zone++;
	}
	if(i==3)
	{
		printk(KERN_DEBUG"No OTP Data or OTP data is invalid");
		return 0;
	}

	MID = s5k3h2_get_otp_module_id(sd, zone);
//	Alive_MainSensor.ModuleID = MID;

	VCMID = s5k3h2_get_otp_vcm_id(sd, zone);
//	Alive_MainSensor.VcmID = VCMID;

	return 0;
}
#endif

/*************************************************************************************************
* Function    :  otp_update()
* Description :  update otp data from otp , it otp data is valid,
                 it include get ID and WB update function
* Return      :  [bool] 0 : update fail
                        1 : update success
**************************************************************************************************/
static int s5k3h2_otp_update(struct v4l2_subdev *sd)
{
	BYTE zone = 0x10;
	BYTE FLG = 0x00;
	//BYTE MID = 0x00;
	int i;

	for(i=0;i<3;i++)
	{
		FLG = s5k3h2_get_otp_flag(sd, zone);
		if(FLG == VALID_OTP)
			break;
		else
			zone++;
	}
	if(i==3)
	{
		printk(KERN_DEBUG"No OTP Data or OTP data is invalid");
		return 0;
	}
#if 0
	MID = s5k3h2_get_otp_module_id(sd, zone);
	if(MID != Oflim_ID)
	{
		printk(KERN_DEBUG"No Oflim Module !");
		return 0;
	}
#endif
	s5k3h2_otp_wb_update(sd, zone);

	return 0;
}
#endif

static int s5k3h2_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int s5k3h2_init(struct v4l2_subdev *sd, u32 val)
{
	struct s5k3h2_info *info = container_of(sd, struct s5k3h2_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 2;

	ret = s5k3h2_write_array(sd, s5k3h2_init_regs);
	if (ret < 0)
		return ret;

#ifdef S5K3H2_USE_AWB_OTP
	ret = s5k3h2_otp_update(sd);
	if (ret < 0)
		return ret;
#endif

	return 0;
}

static int s5k3h2_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = s5k3h2_read(sd, 0x0000, &v);
	printk(KERN_DEBUG"CHIP_ID_H = 0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != S5K3H2_CHIP_ID_H)
		return -ENODEV;
	ret = s5k3h2_read(sd, 0x0001, &v);
	printk(KERN_DEBUG"CHIP_ID_L = 0x%x ", v);
	if (ret < 0)
		return ret;
	if (v != S5K3H2_CHIP_ID_L)
		return -ENODEV;
#if 0
#ifdef S5K3H2_USE_AWB_OTP
	s5k3h2_otp_3h2_read_MID_VCMID(sd);
#endif
#endif
	return 0;
}

static struct s5k3h2_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} s5k3h2_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_S5K3H2_FMTS ARRAY_SIZE(s5k3h2_formats)

static struct s5k3h2_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	int	framerate_div;
	int binning;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	unsigned char  vts_gain_ctrl_1;
	unsigned char  vts_gain_ctrl_2;
	unsigned char  vts_gain_ctrl_3;
	unsigned short gain_ctrl_1;
	unsigned short gain_ctrl_2;
	unsigned short gain_ctrl_3;
	unsigned short spot_meter_win_width;
	unsigned short spot_meter_win_height;
	struct regval_list *regs; /* Regs to tweak */
	struct isp_regb_vals *isp_regs;
	struct isp_regb_vals *regs_lens_threshold_indoor;
	struct isp_regb_vals *regs_lens_threshold_outdoor;
	struct isp_regb_vals *regs_aecgc_win_matrix;
	struct isp_regb_vals *regs_aecgc_win_center;
	struct isp_regb_vals *regs_aecgc_win_central_weight;

} s5k3h2_win_sizes[] = {
#if 1
	/* SXGA */
	{
		.width				= SXGA_WIDTH,
		.height				= SXGA_HEIGHT,
		.vts				= 0x710,
		.framerate			= 30,
		.binning			= 1,
		.max_gain_dyn_frm 	= 0xff,
		.min_gain_dyn_frm	= 0x10,
		.max_gain_fix_frm	= 0xff,//0x100,
		.min_gain_fix_frm 	= 0x10,
		.vts_gain_ctrl_1  	= 0x41,
		.vts_gain_ctrl_2  	= 0x61,
		.vts_gain_ctrl_3  	= 0x81,
		.gain_ctrl_1	  	= 0x80,
		.gain_ctrl_2	  	= 0xc0,
		.gain_ctrl_3	  	= 0xff,
		.spot_meter_win_width  = 400,
		.spot_meter_win_height = 300,
		.regs 				= s5k3h2_win_sxga,
		.isp_regs			= isp_win_setting_8M,
		.regs_lens_threshold_indoor = isp_lens_threshold_8M_indoor,
		.regs_lens_threshold_outdoor = isp_lens_threshold_8M_outdoor,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_2M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_2M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_2M_central_weight,
	},
#endif

#if 1
/* FULL_HD */
	{
		.width				= FULL_HD_WIDTH,
		.height				= FULL_HD_HEIGHT,
		.vts				= 0x73c,
		.framerate			= 30,
		.binning			= 0,
		.max_gain_dyn_frm	= 0xff,
		.min_gain_dyn_frm	= 0x10,
		.max_gain_fix_frm	= 0xff,//0x100,
		.min_gain_fix_frm	= 0x10,
		.vts_gain_ctrl_1	= 0x41,
		.vts_gain_ctrl_2	= 0x61,
		.vts_gain_ctrl_3	= 0x81,//61,
		.gain_ctrl_1		= 0x80,//0x30,
		.gain_ctrl_2		= 0xc0,//0xf0,
		.gain_ctrl_3		= 0xff,
		.spot_meter_win_width  = 700,
		.spot_meter_win_height = 500,
		.regs 				= s5k3h2_win_full_hd,
		.isp_regs			= isp_win_setting_fullhd,
		.regs_lens_threshold_indoor = isp_lens_threshold_fullhd_indoor,
		.regs_lens_threshold_outdoor = isp_lens_threshold_fullhd_outdoor,
		.regs_aecgc_win_matrix			= isp_aecgc_win_full_hd_matrix,
		.regs_aecgc_win_center			= isp_aecgc_win_full_hd_center,
		.regs_aecgc_win_central_weight	= isp_aecgc_win_full_hd_central_weight,

	},
#endif

#if 1
	/* MAX */
	{
		.width				= MAX_WIDTH,
		.height				= MAX_HEIGHT,
		.vts				= 0x9a0,
		.framerate			= 22,
		//.framerate_div	= 10,
		.binning			= 0,
		.max_gain_dyn_frm	= 0xff,
		.min_gain_dyn_frm	= 0x10,
		.max_gain_fix_frm	= 0xff,//0x100,
		.min_gain_fix_frm	= 0x10,
		.vts_gain_ctrl_1	= 0x29,//0x41,
		.vts_gain_ctrl_2	= 0x41,//0x61,
		.vts_gain_ctrl_3	= 0x61,
		.gain_ctrl_1		= 0x80,//0x30,
		.gain_ctrl_2		= 0xc0,//0xf0,
		.gain_ctrl_3		= 0xff,
		.spot_meter_win_width  = 800,
		.spot_meter_win_height = 600,
		.regs 				= s5k3h2_win_8m,
		.isp_regs			= isp_win_setting_8M,
		.regs_lens_threshold_indoor = isp_lens_threshold_8M_indoor,
		.regs_lens_threshold_outdoor = isp_lens_threshold_8M_outdoor,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_8M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_8M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_8M_central_weight,
	},
#endif
};
#define N_WIN_SIZES (ARRAY_SIZE(s5k3h2_win_sizes))
#if 0
static int s5k3h2_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{

	u16 gain, exp, vts;
	struct s5k3h2_info *info = container_of(sd, struct s5k3h2_info, sd);

	gain = parm->gain * 0x20 / 0x10;

	if (gain < 0x20)
		gain = 0x20;
	else if (gain > 0x200)
		gain = 0x200;
	exp = parm->exp;
	vts = exp + 8;
//	s5k3h2_write(sd, 0x0104, 0x01);//grouped para hold on

	//exp+vts
	if(parm->exp != COMIP_CAMERA_AECGC_INVALID_VALUE) {
		s5k3h2_write(sd, 0x0202, (exp >> 8));
		s5k3h2_write(sd, 0x0203, exp & 0xff);
		if(vts < info->win->vts)
			vts = info->win->vts;
		s5k3h2_write(sd, 0x0340, (vts >> 8));
		s5k3h2_write(sd, 0x0341, vts & 0xff);
	}
	//gain
	if(parm->gain != COMIP_CAMERA_AECGC_INVALID_VALUE) {
		s5k3h2_write(sd, 0x0204, (gain >> 8));
		s5k3h2_write(sd, 0x0205, gain & 0xff);
	}

//	s5k3h2_write(sd, 0x0104, 0x00);//grouped para hold on off

	//printk(KERN_DEBUG"gain=%04x exp=%04x vts=%04x\n", parm->gain/100, exp, vts);

	return 0;
}
#endif
static int s5k3h2_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_S5K3H2_FMTS)
		return -EINVAL;

	*code = s5k3h2_formats[index].mbus_code;
	return 0;
}

static int s5k3h2_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct s5k3h2_format_struct **ret_fmt,
		struct s5k3h2_win_size **ret_wsize)
{
	int index;
	struct s5k3h2_win_size *wsize;

	for (index = 0; index < N_S5K3H2_FMTS; index++)
		if (s5k3h2_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_S5K3H2_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = s5k3h2_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = s5k3h2_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = s5k3h2_win_sizes; wsize < s5k3h2_win_sizes + N_WIN_SIZES;
	     wsize++) {
		if (fmt->width <= wsize->width && fmt->height <= wsize->height) {
#if S5K3H2_FULLHD_USING_FULLSIZE_CROP
			if (fmt->width == wsize->width &&
				fmt->width == FULL_HD_WIDTH &&
				fmt->height == wsize->height &&
				fmt->height == FULL_HD_HEIGHT)
				continue;
#endif
			break;
		}
	}
	if (wsize >= s5k3h2_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = s5k3h2_formats[index].colorspace;
	return 0;
}

static int s5k3h2_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return s5k3h2_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int s5k3h2_s_stream(struct v4l2_subdev *sd, int enable);

static int s5k3h2_fill_i2c_regs(struct regval_list *sensor_regs, struct v4l2_fmt_data *data)
{
	while (sensor_regs->reg_num != S5K3H2_REG_END) {
		data->reg[data->reg_num].addr = sensor_regs->reg_num;
		data->reg[data->reg_num].data = sensor_regs->value;
		data->reg_num++;
		sensor_regs++;
	}

	return 0;
}

static int s5k3h2_set_exp_ratio(struct v4l2_subdev *sd, struct s5k3h2_win_size *win_size,
								struct v4l2_fmt_data *data)
{
	int i = 0;
	struct s5k3h2_info *info = container_of(sd, struct s5k3h2_info, sd);

	if(info->win == NULL || info->win == win_size)
		data->exp_ratio = 0x100;
	else {
		for(i = 0; i < N_EXP_RATIO_TABLE_SIZE; i++) {
			if((info->win->width == s5k3h2_exp_ratio_table[i].width_last)
				&& (info->win->height == s5k3h2_exp_ratio_table[i].height_last)
				&& (win_size->width  == s5k3h2_exp_ratio_table[i].width_cur)
				&& (win_size->height == s5k3h2_exp_ratio_table[i].height_cur)) {
				data->exp_ratio = s5k3h2_exp_ratio_table[i].exp_ratio;
				break;
			}
		}
		if(i >= N_EXP_RATIO_TABLE_SIZE) {
			data->exp_ratio = 0x100;
			printk(KERN_DEBUG"s5k3h2:exp_ratio is not found\n!");
		}
	}
	return 0;
}

static int s5k3h2_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct s5k3h2_info *info = container_of(sd, struct s5k3h2_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct s5k3h2_format_struct *fmt_s;
	struct s5k3h2_win_size *wsize;
	int ret = 0;

	ret = s5k3h2_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		//ret = s5k3h2_write_array(sd, fmt_s->regs);
		info->fmt = fmt_s;
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		memset(data, 0, sizeof(*data));
		data->flags = V4L2_I2C_ADDR_16BIT;
		data->slave_addr = client->addr;
		data->vts = wsize->vts;
		data->mipi_clk = 946; /* Mbps. */
		data->framerate = wsize->framerate;
		data->framerate_div = wsize->framerate_div;
		data->binning = wsize->binning;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;
		data->vts_gain_ctrl_1 = wsize->vts_gain_ctrl_1;
		data->vts_gain_ctrl_2 = wsize->vts_gain_ctrl_2;
		data->vts_gain_ctrl_3 = wsize->vts_gain_ctrl_3;
		data->gain_ctrl_1 = wsize->gain_ctrl_1;
		data->gain_ctrl_2 = wsize->gain_ctrl_2;
		data->gain_ctrl_3 = wsize->gain_ctrl_3;
		data->spot_meter_win_width = wsize->spot_meter_win_width;
		data->spot_meter_win_height = wsize->spot_meter_win_height;
		s5k3h2_fill_i2c_regs(wsize->regs, data);
		s5k3h2_set_exp_ratio(sd, wsize, data);
		info->win = wsize;
	}

	return 0;
}

static int s5k3h2_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = s5k3h2_write_array(sd, s5k3h2_stream_on);
	}
	else {
		ret = s5k3h2_write_array(sd, s5k3h2_stream_off);
	}

	return ret;
}

static int s5k3h2_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int s5k3h2_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int s5k3h2_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int s5k3h2_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int s5k3h2_frame_rates[] = { 30, 15, 10, 5, 1 };

static int s5k3h2_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(s5k3h2_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = s5k3h2_frame_rates[interval->index];
	return 0;
}

static int s5k3h2_enum_framesizes(struct v4l2_subdev *sd,
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
		struct s5k3h2_win_size *win = &s5k3h2_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int s5k3h2_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}


static int s5k3h2_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	//struct v4l2_isp_firmware_setting isp_firmware_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&s5k3h2_isp_setting;
		isp_setting.size = ARRAY_SIZE(s5k3h2_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&s5k3h2_isp_parm;
		break;
	case V4L2_CID_GET_EFFECT_FUNC:
		ctrl->value = (int)&sensor_effect_ops;
		break;
	/*
	case V4L2_CID_GET_ISP_FIRMWARE:
		isp_firmware_setting.setting = s5k3h2_isp_firmware;
		isp_firmware_setting.size = ARRAY_SIZE(s5k3h2_isp_firmware);
		memcpy((void *)ctrl->value, (void *)&isp_firmware_setting, sizeof(isp_firmware_setting));
		break;
	*/
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int s5k3h2_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
//	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
//		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
//		ret = s5k3h2_set_aecgc_parm(sd, parm);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static int sensor_get_isp_win_setting(int width, int height, void **vals)
{
	int i = 0;

	*vals = NULL;
	for (i = 0; i < N_WIN_SIZES; i++) {
		if (width == s5k3h2_win_sizes[i].width && height == s5k3h2_win_sizes[i].height) {
			*vals = s5k3h2_win_sizes[i].isp_regs;
			break;
		}
	}

	return 0;
}

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals)
{
	int i = 0;
	int ret = 0;

	for(i = 0; i < N_WIN_SIZES; i++) {
		if (width == s5k3h2_win_sizes[i].width && height == s5k3h2_win_sizes[i].height) {
			switch (meter_mode){
				case METER_MODE_MATRIX:
					*vals = s5k3h2_win_sizes[i].regs_aecgc_win_matrix;
					break;
				case METER_MODE_CENTER:
					*vals = s5k3h2_win_sizes[i].regs_aecgc_win_center;
					break;
				case METER_MODE_CENTRAL_WEIGHT:
					*vals = s5k3h2_win_sizes[i].regs_aecgc_win_central_weight;
					break;
				default:
					break;
				}
			//ret = ARRAY_SIZE(ov8865_win_sizes[i].regs_aecgc_win);
			break;
		}
	}
	return ret;
}

static int sensor_get_lens_threshold(void **v, int input_widht, int input_height, int outdoor)
{
	int loop = 0;
	*v = NULL;

	for (loop = 0; loop < N_WIN_SIZES; loop++) {
		if (s5k3h2_win_sizes[loop].width == input_widht &&
			s5k3h2_win_sizes[loop].height == input_height) {
			if (outdoor)
				*v = s5k3h2_win_sizes[loop].regs_lens_threshold_outdoor;
			else
				*v = s5k3h2_win_sizes[loop].regs_lens_threshold_indoor;

			break;
		}
	}

	return 0;
}

static int s5k3h2_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_S5K3H2, 0);
}
static int s5k3h2_get_module_factory(struct v4l2_subdev *sd, char **module_factory)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!module_factory)
		return -1;
	else
		*module_factory = factory_oflim;

	return 0;
}

static long s5k3h2_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	char **module_factory = NULL;

	switch (cmd) {
		case SUBDEVIOC_MODULE_FACTORY:
			module_factory = (char**)arg;
			ret = s5k3h2_get_module_factory(sd, module_factory);
			break;
		default:
			ret = -ENOIOCTLCMD;
	}
	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int s5k3h2_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = s5k3h2_read(sd, reg->reg & 0xffff, &val);
	printk(KERN_DEBUG"s5k3h2::reg->reg = 0x%x,val=0x%x\n",(unsigned short)reg->reg,val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int s5k3h2_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	s5k3h2_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}


#endif

static const struct v4l2_subdev_core_ops s5k3h2_core_ops = {
	.g_chip_ident = s5k3h2_g_chip_ident,
	.g_ctrl = s5k3h2_g_ctrl,
	.s_ctrl = s5k3h2_s_ctrl,
	.queryctrl = s5k3h2_queryctrl,
	.reset = s5k3h2_reset,
	.init = s5k3h2_init,
	.ioctl = s5k3h2_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = s5k3h2_g_register,
	.s_register = s5k3h2_s_register,
#endif
};

static const struct v4l2_subdev_video_ops s5k3h2_video_ops = {
	.enum_mbus_fmt = s5k3h2_enum_mbus_fmt,
	.try_mbus_fmt = s5k3h2_try_mbus_fmt,
	.s_mbus_fmt = s5k3h2_s_mbus_fmt,
	.s_stream = s5k3h2_s_stream,
	.cropcap = s5k3h2_cropcap,
	.g_crop	= s5k3h2_g_crop,
	.s_parm = s5k3h2_s_parm,
	.g_parm = s5k3h2_g_parm,
	.enum_frameintervals = s5k3h2_enum_frameintervals,
	.enum_framesizes = s5k3h2_enum_framesizes,
};

static const struct v4l2_subdev_ops s5k3h2_ops = {
	.core = &s5k3h2_core_ops,
	.video = &s5k3h2_video_ops,
};

static ssize_t s5k3h2_saturation_ct_threshold_d_right_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%04x\n", s5k3h2_isp_parm.saturation_ct_threshold_d_right);
}

static ssize_t s5k3h2_saturation_ct_threshold_d_right_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	unsigned long value;

	value = simple_strtoul(buf, &endp, 16);
	if (buf == endp)
		return -EINVAL;

	s5k3h2_isp_parm.saturation_ct_threshold_d_right = (int)value;

	return size;
}

static DEVICE_ATTR(s5k3h2_saturation_ct_threshold_d_right, 0664, s5k3h2_saturation_ct_threshold_d_right_show, s5k3h2_saturation_ct_threshold_d_right_store);

static ssize_t s5k3h2_saturation_ct_threshold_c_left_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%04x\n", s5k3h2_isp_parm.saturation_ct_threshold_c_left);
}

static ssize_t s5k3h2_saturation_ct_threshold_c_left_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	unsigned long value;

	value = simple_strtoul(buf, &endp, 16);
	if (buf == endp)
		return -EINVAL;

	s5k3h2_isp_parm.saturation_ct_threshold_c_left = (int)value;

	return size;
}

static DEVICE_ATTR(s5k3h2_saturation_ct_threshold_c_left, 0664, s5k3h2_saturation_ct_threshold_c_left_show, s5k3h2_saturation_ct_threshold_c_left_store);

static ssize_t s5k3h2_saturation_ct_threshold_c_right_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%04x\n", s5k3h2_isp_parm.saturation_ct_threshold_c_right);
}

static ssize_t s5k3h2_saturation_ct_threshold_c_right_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	unsigned long value;

	value = simple_strtoul(buf, &endp, 16);
	if (buf == endp)
		return -EINVAL;

	s5k3h2_isp_parm.saturation_ct_threshold_c_right = (int)value;

	return size;
}

static DEVICE_ATTR(s5k3h2_saturation_ct_threshold_c_right, 0664, s5k3h2_saturation_ct_threshold_c_right_show, s5k3h2_saturation_ct_threshold_c_right_store);

static ssize_t s5k3h2_saturation_ct_threshold_a_left_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%04x\n", s5k3h2_isp_parm.saturation_ct_threshold_a_left);
}

static ssize_t s5k3h2_saturation_ct_threshold_a_left_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	unsigned long value;

	value = simple_strtoul(buf, &endp, 16);
	if (buf == endp)
		return -EINVAL;

	s5k3h2_isp_parm.saturation_ct_threshold_a_left = (int)value;

	return size;
}

static DEVICE_ATTR(s5k3h2_saturation_ct_threshold_a_left, 0664, s5k3h2_saturation_ct_threshold_a_left_show, s5k3h2_saturation_ct_threshold_a_left_store);


static int s5k3h2_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct s5k3h2_info *info;
	struct comip_camera_subdev_platform_data *s5k3h2_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct s5k3h2_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &s5k3h2_ops);

	/* Make sure it's an s5k3h2 */
	ret = s5k3h2_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an s5k3h2 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "s5k3h2 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	s5k3h2_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (s5k3h2_setting) {
		if (s5k3h2_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			s5k3h2_isp_parm.mirror = 1;
		else s5k3h2_isp_parm.mirror = 0;

		if (s5k3h2_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			s5k3h2_isp_parm.flip = 1;
		else s5k3h2_isp_parm.flip = 0;
	}

	device_create_file(&client->dev, &dev_attr_s5k3h2_saturation_ct_threshold_d_right);
	device_create_file(&client->dev, &dev_attr_s5k3h2_saturation_ct_threshold_c_left);
	device_create_file(&client->dev, &dev_attr_s5k3h2_saturation_ct_threshold_c_right);
	device_create_file(&client->dev, &dev_attr_s5k3h2_saturation_ct_threshold_a_left);
	return ret;
}

static int s5k3h2_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3h2_info *info = container_of(sd, struct s5k3h2_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id s5k3h2_id[] = {
	{ "s5k3h2-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s5k3h2_id);

static struct i2c_driver s5k3h2_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "s5k3h2-mipi-raw",
	},
	.probe		= s5k3h2_probe,
	.remove		= s5k3h2_remove,
	.id_table	= s5k3h2_id,
};

static __init int init_s5k3h2(void)
{
	return i2c_add_driver(&s5k3h2_driver);
}

static __exit void exit_s5k3h2(void)
{
	i2c_del_driver(&s5k3h2_driver);
}

fs_initcall(init_s5k3h2);
module_exit(exit_s5k3h2);

MODULE_DESCRIPTION("A low-level driver for Samsung s5k3h2 sensors");
MODULE_LICENSE("GPL");
