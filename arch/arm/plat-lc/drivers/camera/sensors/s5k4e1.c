
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
#include "s5k4e1.h"

#define S5K4E1_CHIP_ID_H	(0x4e)
#define S5K4E1_CHIP_ID_L	(0x10)

#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		2560
#define MAX_HEIGHT		1920
#define MAX_PREVIEW_WIDTH	SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  SXGA_HEIGHT

#define S5K4E1_REG_END		0xffff
#define S5K4E1_REG_DELAY	0xfffe

#define S5K4E1_REG_VAL_HVFLIP   0xff


struct s5k4e1_format_struct;
struct s5k4e1_info {
	struct v4l2_subdev sd;
	struct s5k4e1_format_struct *fmt;
	struct s5k4e1_win_size *win;
	int binning;
	int stream_on;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

static struct regval_list s5k4e1_init_regs[] = {

	// ******************* //
	// S5K4E1GX EVT3 MIPI Setting
	//
	// last update date : 2011. 11. 30
	//
	// Full size output (1304 x 980)
	// This Setfile is optimized for 15fps or lower frame rate
	//
	// ******************* //
	//$MIPI[Width:1280,Height:960,Format:Raw10,Lane:2,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:2]
	//+++++++++++++++++++++++++++++++//
	// Reset for operation ...
	{0x0103, 0x01},
	{0x0100, 0x00},//stream off
	//{0x0101, 0x02},//mirror
	//{0x0101, S5K4E1_REG_VAL_HVFLIP},
	{0x3030, 0x06},//shut streaming off

	//--> The below registers are for FACTORY ONLY. If you change them without prior notification.
	// YOU are RESPONSIBLE for the FAILURE that will happen in the future.
	//+++++++++++++++++++++++++++++++//
	//Factory only set START
	// Analog Setting
	{0x3000, 0x05},
	{0x3001, 0x03},
	{0x3002, 0x08},
	{0x3003, 0x09},
	{0x3004, 0x2E},
	{0x3005, 0x06},
	{0x3006, 0x34},
	{0x3007, 0x00},
	{0x3008, 0x3C},
	{0x3009, 0x3C},
	{0x300A, 0x28},
	{0x300B, 0x04},
	{0x300C, 0x0A},
	{0x300D, 0x02},
	{0x300E, 0xE8},
	{0x300F, 0x82},
	{0x3010, 0x00},
	{0x3011, 0x4C},
	{0x3012, 0x30},
	{0x3013, 0xC0},
	{0x3014, 0x00},
	{0x3015, 0x00},
	{0x3016, 0x2C},
	{0x3017, 0x94},
	{0x3018, 0x78},
	{0x301B, 0x75},
	{0x301C, 0x04},
	{0x301D, 0xD4},
	{0x3021, 0x02},
	{0x3022, 0x24},
	{0x3024, 0x40},
	{0x3027, 0x08},
	{0x3029, 0xC6},
	{0x30BC, 0xB0},
	{0x302B, 0x01},
	{0x30D8, 0x3F},
	//+++++++++++++++++++++++++++++++//
	// ADLC setting ...
	{0x3070, 0x5F},
	{0x3071, 0x00},
	{0x3080, 0x04},
	{0x3081, 0x38},
	//+++++++++++++++++++++++++++++++//
	// MIPI setting
	{0x30BD, 0x00},//SEL_CCP[0]
	{0x3084, 0x15},//SYNC Mode
	{0x30BE, 0x1A},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]
	{0x30C1, 0x01},//pack video enable [0]
	{0x30EE, 0x02},//DPHY enable [1]
	{0x3111, 0x86},//Embedded data off [5]

	//Factory only set END
	//+++++++++++++++++++++++++++++++//
	// Integration setting ...
	{0x0202, 0x03},//coarse integration time
	{0x0203, 0xD4},
	{0x0204, 0x00},//analog gain[msb] 0100 x8 0080 x4
	{0x0205, 0x80},//analog gain[lsb] 0040 x2 0020 x1
	// Frame Length
	{0x0340, 0x03},//Capture 07B4(1960[# of row]+12[V-blank])
	{0x0341, 0xD8},//E0//SXGA 03E0(980[# of row]+12[V-blank])
	// Line Length
	{0x0342, 0x0A},//2738
	{0x0343, 0xB2},

	{0x30E3, 0x2D}, //MCLK dependent reg.
	{0x30E4, 0xB4},
	{0x3113, 0x5B},
	{0x3114, 0x68},
	{0x3115, 0x64},
	{0x3116, 0x8C},


	//+++++++++++++++++++++++++++++++//
	// PLL setting ...
	//// input clock 24MHz
	////// (3) MIPI 2-lane Serial(TST = 0000b or TST = 0010b), 15 fps
	{0x0305, 0x05},//06//PLL P = 6
	{0x0306, 0x00},//PLL M[8] = 0
	{0x0307, 0x68},//70//65//PLL M = 101
	{0x30B5, 0x01},//PLL S = 1
	{0x30E2, 0x02},//num lanes[1:0] = 2//2
	{0x30F1, 0x70},//DPHY BANDCTRL 404MHz=40.4MHz

	// MIPI Size Setting
	{0x30A9, 0x02},//Horizontal Binning On
	{0x300E, 0xEB},//Vertical Binning On

	//////////////////////////for Simmian Test 1280*960 Not Use : Remove this //////////////

	{0x0344, 0x00},//x_addr_start 24
	{0x0345, 0x18},
	{0x0346, 0x00},//y_addr_start
	{0x0347, 0x14},//00
	{0x0348, 0x0A},//x_addr_end 2583
	{0x0349, 0x17},
	{0x034A, 0x07},//y_addr_end 1937
	{0x034B, 0x93},//A7

	{0x0380, 0x00},//x_even_inc 1
	{0x0381, 0x01},
	{0x0382, 0x00},//x_odd_inc 1
	{0x0383, 0x01},
	{0x0384, 0x00},//y_even_inc 1
	{0x0385, 0x01},
	{0x0386, 0x00},//y_odd_inc 3
	{0x0387, 0x03},

	{0x034C, 0x05},//x_output_size 1280
	{0x034D, 0x00},
	{0x034E, 0x03},//y_output_size 960
	{0x034F, 0xC0},//d4

	{0x30BF, 0xAB}, //outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
	{0x30C0, 0x40}, //video_offset[7:4] 1600%12
	{0x30C8, 0x06}, //video_data_length 1600 = 1280 * 1.25
	{0x30C9, 0x40},

	//S010001



	{S5K4E1_REG_END, 0x00}, /* END MARKER */

};

static struct regval_list s5k4e1_stream_on[] = {
	{0x0100, 0x01},
	{S5K4E1_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list s5k4e1_stream_off[] = {
	{0x0100, 0x00},
	//{S5K4E1_REG_DELAY, 80},
	{S5K4E1_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list s5k4e1_win_sxga[] = {
	// ******************* //
	// S5K4E1GX EVT3 MIPI Setting
	//
	// last update date : 2011. 11. 30
	//
	// Full size output (1304 x 980)
	// This Setfile is optimized for 15fps or lower frame rate
	//
	// ******************* //
	//$MIPI[Width:1280,Height:960,Format:Raw10,Lane:2,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:2]
	//+++++++++++++++++++++++++++++++//
	// Reset for operation ...
	{0x0103, 0x01},
	//{0x0100, 0x00},//stream off
	//{0x0101, 0x02},//mirror
	//{0x0101, S5K4E1_REG_VAL_HVFLIP},
	{0x3030, 0x06},//shut streaming off

	//--> The below registers are for FACTORY ONLY. If you change them without prior notification.
	// YOU are RESPONSIBLE for the FAILURE that will happen in the future.
	//+++++++++++++++++++++++++++++++//
	//Factory only set START
	// Analog Setting
	{0x3000, 0x05},
	{0x3001, 0x03},
	{0x3002, 0x08},
	{0x3003, 0x09},
	{0x3004, 0x2E},
	{0x3005, 0x06},
	{0x3006, 0x34},
	{0x3007, 0x00},
	{0x3008, 0x3C},
	{0x3009, 0x3C},
	{0x300A, 0x28},
	{0x300B, 0x04},
	{0x300C, 0x0A},
	{0x300D, 0x02},
	{0x300E, 0xE8},
	{0x300F, 0x82},
	{0x3010, 0x00},
	{0x3011, 0x4C},
	{0x3012, 0x30},
	{0x3013, 0xC0},
	{0x3014, 0x00},
	{0x3015, 0x00},
	{0x3016, 0x2C},
	{0x3017, 0x94},
	{0x3018, 0x78},
	{0x301B, 0x75},
	{0x301C, 0x04},
	{0x301D, 0xD4},
	{0x3021, 0x02},
	{0x3022, 0x24},
	{0x3024, 0x40},
	{0x3027, 0x08},
	{0x3029, 0xC6},
	{0x30BC, 0xB0},
	{0x302B, 0x01},
	{0x30D8, 0x3F},
	//+++++++++++++++++++++++++++++++//
	// ADLC setting ...
	{0x3070, 0x5F},
	{0x3071, 0x00},
	{0x3080, 0x04},
	{0x3081, 0x38},
	//+++++++++++++++++++++++++++++++//
	// MIPI setting
	{0x30BD, 0x00},//SEL_CCP[0]
	{0x3084, 0x15},//SYNC Mode
	{0x30BE, 0x1A},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]
	{0x30C1, 0x01},//pack video enable [0]
	{0x30EE, 0x02},//DPHY enable [1]
	{0x3111, 0x86},//Embedded data off [5]

	//Factory only set END
	//+++++++++++++++++++++++++++++++//
	// Integration setting ...
	{0x0202, 0x03},//coarse integration time
	{0x0203, 0xD4},
	{0x0204, 0x00},//analog gain[msb] 0100 x8 0080 x4
	{0x0205, 0x80},//analog gain[lsb] 0040 x2 0020 x1
	// Frame Length
	{0x0340, 0x03},//Capture 07B4(1960[# of row]+12[V-blank])
	{0x0341, 0xD8},//E0//SXGA 03E0(980[# of row]+12[V-blank])
	// Line Length
	{0x0342, 0x0A},//2738
	{0x0343, 0xB2},

	{0x30E3, 0x2D}, //MCLK dependent reg.
	//{0x30E3, 0x5B}, //MCLK dependent reg.
	{0x30E4, 0xB4},
	{0x3113, 0x5B},
	//{0x3113, 0xa7},
	{0x3114, 0x68},
	{0x3115, 0x64},
	//{0x3115, 0x83},
	{0x3116, 0x8C},


	//+++++++++++++++++++++++++++++++//
	// PLL setting ...
	//// input clock 24MHz
	////// (3) MIPI 2-lane Serial(TST = 0000b or TST = 0010b), 15 fps
	{0x0305, 0x05},//06//PLL P = 6
	{0x0306, 0x00},//PLL M[8] = 0
	{0x0307, 0x68},//70//65//PLL M = 101
	{0x30B5, 0x01},//PLL S = 1
	{0x30E2, 0x02},//num lanes[1:0] = 2//2
	{0x30F1, 0x70},//DPHY BANDCTRL 404MHz=40.4MHz

	// MIPI Size Setting
	{0x30A9, 0x02},//Horizontal Binning On
	{0x300E, 0xEB},//Vertical Binning On

	//////////////////////////for Simmian Test 1280*960 Not Use : Remove this //////////////

	{0x0344, 0x00},//x_addr_start 24
	{0x0345, 0x18},
	{0x0346, 0x00},//y_addr_start
	{0x0347, 0x14},//00
	{0x0348, 0x0A},//x_addr_end 2583
	{0x0349, 0x17},
	{0x034A, 0x07},//y_addr_end 1937
	{0x034B, 0x93},//A7

	{0x0380, 0x00},//x_even_inc 1
	{0x0381, 0x01},
	{0x0382, 0x00},//x_odd_inc 1
	{0x0383, 0x01},
	{0x0384, 0x00},//y_even_inc 1
	{0x0385, 0x01},
	{0x0386, 0x00},//y_odd_inc 3
	{0x0387, 0x03},

	{0x034C, 0x05},//x_output_size 1280
	{0x034D, 0x00},
	{0x034E, 0x03},//y_output_size 960
	{0x034F, 0xC0},//d4

	{0x30BF, 0xAB}, //outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
	{0x30C0, 0x40}, //video_offset[7:4] 1600%12
	{0x30C8, 0x06}, //video_data_length 1600 = 1280 * 1.25
	{0x30C9, 0x40},

	//S010001


	{S5K4E1_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list s5k4e1_win_5m[] = {
	// ******************* //
	// S5K4E1GX EVT3 MIPI Setting
	//
	// last update date : 2011. 11. 30
	//
	// Full size output (2608 x 1960)
	// This Setfile is optimized for 15fps or lower frame rate
	//
	// ******************* //
	//$MIPI[Width:2560,Height:1920,Format:Raw10,Lane:2,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:2]
	//+++++++++++++++++++++++++++++++//
	// Reset for operation ...
	{0x0103, 0x01},
	//{0x0100, 0x00},//stream off
	//{S5K4E1_REG_DELAY, 1000},
	//{0x0101, 0x02},//mirror
	{0x3030, 0x06},//shut streaming off

	//--> The below registers are for FACTORY ONLY. If you change them without prior notification.
	// YOU are RESPONSIBLE for the FAILURE that will happen in the future.
	//+++++++++++++++++++++++++++++++//
	//Factory only set START
	// Analog Setting
	{0x3000, 0x05},
	{0x3001, 0x03},
	{0x3002, 0x08},
	{0x3003, 0x09},
	{0x3004, 0x2E},
	{0x3005, 0x06},
	{0x3006, 0x34},
	{0x3007, 0x00},
	{0x3008, 0x3C},
	{0x3009, 0x3C},
	{0x300A, 0x28},
	{0x300B, 0x04},
	{0x300C, 0x0A},
	{0x300D, 0x02},
	{0x300E, 0xE8},
	{0x300F, 0x82},
	{0x3010, 0x00},
	{0x3011, 0x4C},
	{0x3012, 0x30},
	{0x3013, 0xC0},
	{0x3014, 0x00},
	{0x3015, 0x00},
	{0x3016, 0x2C},
	{0x3017, 0x94},
	{0x3018, 0x78},
	{0x301B, 0x75},
	{0x301C, 0x04},
	{0x301D, 0xD4},
	{0x3021, 0x02},
	{0x3022, 0x24},
	{0x3024, 0x40},
	{0x3027, 0x08},
	{0x3029, 0xC6},
	{0x30BC, 0xB0},
	{0x302B, 0x01},
	{0x30D8, 0x3F},
	//+++++++++++++++++++++++++++++++//
	// ADLC setting ...
	{0x3070, 0x5F},
	{0x3071, 0x00},
	{0x3080, 0x04},
	{0x3081, 0x38},
	//+++++++++++++++++++++++++++++++//
	// MIPI setting
	{0x30BD, 0x00},//SEL_CCP[0]
	{0x3084, 0x15},//SYNC Mode
	{0x30BE, 0x1A},//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]
	{0x30C1, 0x01},//pack video enable [0]
	{0x30EE, 0x02},//DPHY enable [1]
	{0x3111, 0x86},//Embedded data off [5]

	//Factory only set END
	//+++++++++++++++++++++++++++++++//
	// Integration setting ...
	{0x0202, 0x07},//coarse integration time
	{0x0203, 0xA8},
	{0x0204, 0x00},//analog gain[msb] 0100 x8 0080 x4
	{0x0205, 0x20},//80//analog gain[lsb] 0040 x2 0020 x1
	// Frame Length
	{0x0340, 0x07},//Capture 07B4(1960[# of row]+12[V-blank])
	{0x0341, 0xB4},//SXGA 03E0(980[# of row]+12[V-blank])
	// Line Length
	{0x0342, 0x0A},//2738
	{0x0343, 0xB2},

	{0x30E3, 0x2D}, //MCLK dependent reg.
	//{0x30E3, 0x5B}, //MCLK dependent reg.
	{0x30E4, 0xB4},
	{0x3113, 0x5B},
	//{0x3113, 0xa7},
	{0x3114, 0x68},
	{0x3115, 0x64},
	//{0x3115, 0x83},
	{0x3116, 0x8C},


	//+++++++++++++++++++++++++++++++//
	// PLL setting ...
	//// input clock 24MHz
	////// (3) MIPI 2-lane Serial(TST = 0000b or TST = 0010b), 15 fps
	{0x0305, 0x05},//06//PLL P = 6
	{0x0306, 0x00},//PLL M[8] = 0
	{0x0307, 0x68},//70//65//PLL M = 101
	{0x30B5, 0x01},//PLL S = 1
	{0x30E2, 0x02},//num lanes[1:0] = 2
	{0x30F1, 0x70},//DPHY BANDCTRL 404MHz=40.4MHz

	// MIPI Size Setting
	{0x30A9, 0x03},//Horizontal Binning Off
	{0x300E, 0xE8},//Vertical Binning Off

	{0x034C, 0x0A},//x_output_size_High  //2592=0x0A20 for Simmian Jack_20120419
	{0x034D, 0x00},//20
	{0x034E, 0x07},//y_output_size_High
	{0x034F, 0x80},//A8

	{0x0344, 0x00},//x_addr_start  //2608-2592=16d 16d/2=8d  Jack_20120419
	{0x0345, 0x18},//08
	{0x0346, 0x00},//y_addr_start
	{0x0347, 0x14},//00
	{0x0348, 0x0A},//x_addr_end   //2599=0x0A27,8+2592=2600    Jack_20120419
	{0x0349, 0x17},//27
	{0x034A, 0x07},//y_addr_end
	{0x034B, 0x93},//A7

	{0x30BF, 0xAB}, // outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
	{0x30C0, 0x08},//00//C0//video_offset[7:4] video_data_length             //32403y12ии?иоидж╠иииои▓0  0x00 Jack_20120419  "C0" or "00"??
	{0x30C8, 0x0C},//x_output_size_High * 1.25                       //3240=0x0CA8 2592*1.25=3240  Jack_20120419
	{0x30C9, 0x80},//A8


	//S010001
	//////////////////////////for Simmian Test 2592*1960 //////////////

	{S5K4E1_REG_END, 0x00},	/* END MARKER */
};

static int s5k4e1_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int s5k4e1_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int s5k4e1_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char hvflip = 0;

	while (vals->reg_num != S5K4E1_REG_END) {
		if (vals->reg_num == S5K4E1_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			if (vals->reg_num == 0x0101
			    && vals->value == S5K4E1_REG_VAL_HVFLIP) {
				if (s5k4e1_isp_parm.flip)
					hvflip &= 0xfd;
				else
					hvflip |= 0x02;

				if (s5k4e1_isp_parm.mirror)
					hvflip |= 0x01;
				else
					hvflip &= 0xfe;


				ret = s5k4e1_write(sd, vals->reg_num, hvflip);
				if (ret < 0)
					return ret;
				vals++;
				continue;
			}
			ret = s5k4e1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int s5k4e1_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int s5k4e1_init(struct v4l2_subdev *sd, u32 val)
{
	struct s5k4e1_info *info = container_of(sd, struct s5k4e1_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 2;
	info->stream_on = 0;

	ret = s5k4e1_write_array(sd, s5k4e1_init_regs);
	if (ret < 0)
		return ret;

	return 0;
}

static int s5k4e1_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = s5k4e1_read(sd, 0x0000, &v);
	if (ret < 0)
		return ret;
	if (v != S5K4E1_CHIP_ID_H)
		return -ENODEV;
	ret = s5k4e1_read(sd, 0x0001, &v);
	if (ret < 0)
		return ret;
	if (v != S5K4E1_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct s5k4e1_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} s5k4e1_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_S5K4E1_FMTS ARRAY_SIZE(s5k4e1_formats)

static struct s5k4e1_win_size {
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
} s5k4e1_win_sizes[] = {
	/* SXGA */
	{
		.width				= SXGA_WIDTH,
		.height				= SXGA_HEIGHT,
		.vts				= 0x3d8,
		.framerate			= 30,
		.min_framerate 		= 10,
		.down_steps 		= 20,
		.down_small_step 	= 1,
		.down_large_step	= 6,
		.up_steps 			= 20,
		.up_small_step		= 1,
		.up_large_step		= 6,
		.regs 				= s5k4e1_win_sxga,
	},
	/* 2560*1920 */
	{
		.width				= MAX_WIDTH,
		.height				= MAX_HEIGHT,
		.vts				= 0x7b4,
		.framerate			= 15,
		.min_framerate 		= 15,
		.regs 				= s5k4e1_win_5m,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(s5k4e1_win_sizes))

static int s5k4e1_set_aecgc_parm(struct v4l2_subdev *sd, struct v4l2_aecgc_parm *parm)
{
	u16 gain, exp, vts;

	gain = parm->gain * 0x20 / 100;

	if (gain < 0x20)
	        gain = 0x20;
	else if (gain > 0x200)
	        gain = 0x200;
	exp = parm->exp;
	vts = parm->vts;
	s5k4e1_write(sd, 0x0104, 0x01);//grouped para hold on
	//exp
	s5k4e1_write(sd, 0x0202, (exp >> 8));
	s5k4e1_write(sd, 0x0203, exp & 0xff);
	//gain
	s5k4e1_write(sd, 0x0204, (gain >> 8));
	s5k4e1_write(sd, 0x0205, gain & 0xff);
	//vts
	s5k4e1_write(sd, 0x0340, (vts >> 8));
	s5k4e1_write(sd, 0x0341, vts & 0xff);
	s5k4e1_write(sd, 0x0104, 0x00);//grouped para hold on off

	//printk("gain=%04x exp=%04x vts=%04x\n", parm->gain/100, exp, vts);
	return 0;
}

static int s5k4e1_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_S5K4E1_FMTS)
		return -EINVAL;

	*code = s5k4e1_formats[index].mbus_code;
	return 0;
}

static int s5k4e1_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct s5k4e1_format_struct **ret_fmt,
		struct s5k4e1_win_size **ret_wsize)
{
	int index;
	struct s5k4e1_win_size *wsize;

	for (index = 0; index < N_S5K4E1_FMTS; index++)
		if (s5k4e1_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_S5K4E1_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = s5k4e1_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = s5k4e1_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = s5k4e1_win_sizes; wsize < s5k4e1_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= s5k4e1_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = s5k4e1_formats[index].colorspace;
	return 0;
}

static int s5k4e1_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return s5k4e1_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int s5k4e1_s_stream(struct v4l2_subdev *sd, int enable);

static int s5k4e1_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4e1_info *info = container_of(sd, struct s5k4e1_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct s5k4e1_format_struct *fmt_s;
	struct s5k4e1_win_size *wsize;
	struct v4l2_aecgc_parm aecgc_parm;
	int ret = 0;

	ret = s5k4e1_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = s5k4e1_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		ret = s5k4e1_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		if (data->aecgc_parm.vts != 0 
			|| data->aecgc_parm.exp != 0 
			|| data->aecgc_parm.gain != 0) {
			aecgc_parm.exp = data->aecgc_parm.exp;
			aecgc_parm.gain = data->aecgc_parm.gain;
			aecgc_parm.vts = data->aecgc_parm.vts;
			s5k4e1_set_aecgc_parm(sd, &aecgc_parm);
		}

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 800; /* Mbps. */
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
			s5k4e1_s_stream(sd, 1);
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int s5k4e1_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5k4e1_info *info = container_of(sd, struct s5k4e1_info, sd);
	int ret = 0;

	if (enable) {
		ret = s5k4e1_write_array(sd, s5k4e1_stream_on);
		info->stream_on = 1;
	}
	else {
		ret = s5k4e1_write_array(sd, s5k4e1_stream_off);
		info->stream_on = 0;
	}
	return ret;
}

static int s5k4e1_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int s5k4e1_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int s5k4e1_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int s5k4e1_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int s5k4e1_frame_rates[] = { 30, 15, 10, 5, 1 };

static int s5k4e1_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(s5k4e1_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = s5k4e1_frame_rates[interval->index];
	return 0;
}

static int s5k4e1_enum_framesizes(struct v4l2_subdev *sd,
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
		struct s5k4e1_win_size *win = &s5k4e1_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int s5k4e1_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int s5k4e1_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&s5k4e1_isp_setting;
		isp_setting.size = ARRAY_SIZE(s5k4e1_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&s5k4e1_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int s5k4e1_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_aecgc_parm *parm = NULL;

	switch (ctrl->id) {
	case V4L2_CID_AECGC_PARM:
		memcpy(&parm, &(ctrl->value), sizeof(struct v4l2_aecgc_parm*));
		ret = s5k4e1_set_aecgc_parm(sd, parm);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static int s5k4e1_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_S5K4E1, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int s5k4e1_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = s5k4e1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int s5k4e1_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	s5k4e1_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops s5k4e1_core_ops = {
	.g_chip_ident = s5k4e1_g_chip_ident,
	.g_ctrl = s5k4e1_g_ctrl,
	.s_ctrl = s5k4e1_s_ctrl,
	.queryctrl = s5k4e1_queryctrl,
	.reset = s5k4e1_reset,
	.init = s5k4e1_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = s5k4e1_g_register,
	.s_register = s5k4e1_s_register,
#endif
};

static const struct v4l2_subdev_video_ops s5k4e1_video_ops = {
	.enum_mbus_fmt = s5k4e1_enum_mbus_fmt,
	.try_mbus_fmt = s5k4e1_try_mbus_fmt,
	.s_mbus_fmt = s5k4e1_s_mbus_fmt,
	.s_stream = s5k4e1_s_stream,
	.cropcap = s5k4e1_cropcap,
	.g_crop	= s5k4e1_g_crop,
	.s_parm = s5k4e1_s_parm,
	.g_parm = s5k4e1_g_parm,
	.enum_frameintervals = s5k4e1_enum_frameintervals,
	.enum_framesizes = s5k4e1_enum_framesizes,
};

static const struct v4l2_subdev_ops s5k4e1_ops = {
	.core = &s5k4e1_core_ops,
	.video = &s5k4e1_video_ops,
};

static int s5k4e1_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct s5k4e1_info *info;
	struct comip_camera_subdev_platform_data *s5k4e1_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct s5k4e1_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &s5k4e1_ops);

	/* Make sure it's an s5k4e1 */
	ret = s5k4e1_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an s5k4e1 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "s5k4e1 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	s5k4e1_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (s5k4e1_setting) {
		if (s5k4e1_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			s5k4e1_isp_parm.mirror = 1;
		else s5k4e1_isp_parm.mirror = 0;

		if (s5k4e1_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			s5k4e1_isp_parm.flip = 1;
		else s5k4e1_isp_parm.flip = 0;
	}

	return ret;
}

static int s5k4e1_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4e1_info *info = container_of(sd, struct s5k4e1_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id s5k4e1_id[] = {
	{ "s5k4e1", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s5k4e1_id);

static struct i2c_driver s5k4e1_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "s5k4e1",
	},
	.probe		= s5k4e1_probe,
	.remove		= s5k4e1_remove,
	.id_table	= s5k4e1_id,
};

static __init int init_s5k4e1(void)
{
	return i2c_add_driver(&s5k4e1_driver);
}

static __exit void exit_s5k4e1(void)
{
	i2c_del_driver(&s5k4e1_driver);
}

fs_initcall(init_s5k4e1);
module_exit(exit_s5k4e1);

MODULE_DESCRIPTION("A low-level driver for Samsung s5k4e1 sensors");
MODULE_LICENSE("GPL");
