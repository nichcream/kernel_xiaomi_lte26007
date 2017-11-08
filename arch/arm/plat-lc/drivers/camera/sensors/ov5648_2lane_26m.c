
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
#include "ov5648_2lane_26m.h"

#define OV5648_CHIP_ID_H	(0x56)
#define OV5648_CHIP_ID_L	(0x48)

#define SXGA_WIDTH		1280
#define SXGA_HEIGHT		960
#define MAX_WIDTH		2560
#define MAX_HEIGHT		1920
#define MAX_PREVIEW_WIDTH	SXGA_WIDTH
#define MAX_PREVIEW_HEIGHT  SXGA_HEIGHT

#define OV5648_REG_END		0xffff
#define OV5648_REG_DELAY	0xfffe

#define OV5648_REG_VAL_PREVIEW  0xff
#define OV5648_REG_VAL_SNAPSHOT 0xfe

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals);

struct ov5648_format_struct;
struct ov5648_info {
	struct v4l2_subdev sd;
	struct ov5648_format_struct *fmt;
	struct ov5648_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int frame_rate;
	int binning;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct otp_struct {
	unsigned int module_integrator_id;
	unsigned int lens_id;
	unsigned int rg_ratio;
	unsigned int bg_ratio;
	unsigned int user_data[3];
	unsigned int light_rg;
	unsigned int light_bg;
};

static struct regval_list ov5648_init_regs[] = {

	{0x0100, 0x00},  // software sleep
	{0x0103, 0x01},  // software reset
	{OV5648_REG_DELAY, 5},//delay(5ms)
	{0x3001, 0x00}, // D[7:0] set to input
	{0x3002, 0x00}, // D[11:8] set to input
	{0x3011, 0x02}, // Drive strength 2x
	{0x3018, 0x4c}, // MIPI 2 lane
	{0x3022, 0x00},
	{0x3034, 0x1a}, // 10-bit mode
	{0x3035, 0x21}, // PLL
	{0x3036, 0x63}, // PLL
	{0x3037, 0x03}, // PLL yl set
	{0x3038, 0x00}, // PLL
	{0x3039, 0x00}, // PLL
	{0x303a, 0x00}, // PLLS
	{0x303b, 0x19}, // PLLS
	{0x303c, 0x11}, // PLLS
	{0x303d, 0x30}, // PLLS
	{0x3105, 0x11},
	{0x3106, 0x05}, // PLL
	{0x3304, 0x28},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	{0x3500, 0x00}, // exposure [19:16]
	{0x3501, 0x00}, // exposure [15:8]
	{0x3502, 0x10}, // exposure [7:0], exposure = 0x3d0 = 976-->16
	{0x3503, 0x27}, // gain delay 2 frames, manual agc/aec
	{0x350a, 0x00}, // gain[9:8]
	{0x350b, 0x40}, // gain[7:0], gain = 4x
	{0x3601, 0x33}, // analog control
	{0x3602, 0x00}, // analog control
	{0x3611, 0x0e}, // analog control
	{0x3612, 0x2b}, // analog control
	{0x3614, 0x50}, // analog control
	{0x3620, 0x33}, // analog control
	{0x3622, 0x00}, // analog control
	{0x3630, 0xad}, // analog control
	{0x3631, 0x00}, // analog control
	{0x3632, 0x94}, // analog control
	{0x3633, 0x17}, // analog control
	{0x3634, 0x14}, // analog control
	{0x3705, 0x2a}, // analog control
	{0x3708, 0x66}, // analog control
	{0x3709, 0x52}, // analog control
	{0x370b, 0x23}, // analog control
	{0x370c, 0xc3}, // analog control
	{0x370d, 0x00}, // analog control
	{0x370e, 0x00}, // analog control
	{0x371c, 0x07}, // analog control
	{0x3739, 0xd2}, // analog control
	{0x373c, 0x00},
	{0x3800, 0x00}, // xstart = 0
	{0x3801, 0x00}, // xstart
	{0x3802, 0x00}, // ystart = 0
	{0x3803, 0x00}, // ystart
	{0x3804, 0x0a}, // xend = 2623
	{0x3805, 0x3f}, // yend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa3}, // yend
	{0x3808, 0x05}, // x output size = 1280
	{0x3809, 0x00}, // x output size
	{0x380a, 0x03}, // y output size = 960
	{0x380b, 0xc0}, // y output size
	{0x380c, 0x0b}, // hts = 2816
	{0x380d, 0x00}, // hts
	{0x380e, 0x03}, // vts = 992
	{0x380f, 0xe0}, // vts
	{0x3810, 0x00}, // isp x win = 8
	{0x3811, 0x08}, // isp x win
	{0x3812, 0x00}, // isp y win = 4
	{0x3813, 0x04}, // isp y win
	{0x3814, 0x31}, // x inc
	{0x3815, 0x31}, // y inc
	{0x3817, 0x00}, // hsync start
#if 0
	{0x3820, 0x08}, // flip off, v bin off
	{0x3821, 0x07}, // mirror on, h bin on
#else
  {0x3820, OV5648_REG_VAL_PREVIEW}, // flip off, v bin off
	{0x3821, OV5648_REG_VAL_PREVIEW}, // mirror on, h bin on
#endif
	{0x3826, 0x03},
	{0x3829, 0x00},
	{0x382b, 0x0b},
	{0x3830, 0x00},
	{0x3836, 0x00},
	{0x3837, 0x00},
	{0x3838, 0x00},
	{0x3839, 0x04},
	{0x383a, 0x00},
	{0x383b, 0x01},
	{0x3b00, 0x00}, // strobe off
	{0x3b02, 0x08}, // shutter delay
	{0x3b03, 0x00}, // shutter delay
	{0x3b04, 0x04}, // frex_exp
	{0x3b05, 0x00}, // frex_exp
	{0x3b06, 0x04},
	{0x3b07, 0x08}, // frex inv
	{0x3b08, 0x00}, // frex exp req
	{0x3b09, 0x02}, // frex end option
	{0x3b0a, 0x04}, // frex rst length
	{0x3b0b, 0x00}, // frex strobe width
	{0x3b0c, 0x3d}, // frex strobe width
	{0x3f01, 0x0d},
	{0x3f0f, 0xf5},
	{0x4000, 0x89}, // blc enable
	{0x4001, 0x02}, // blc start line
	{0x4002, 0x45}, // blc auto, reset frame number = 5
	{0x4004, 0x02}, // black line number
	{0x4005, 0x18}, // blc normal freeze
	{0x4006, 0x08},
	{0x4007, 0x10},
	{0x4008, 0x00},
	{0x4009, 0x10}, // blc black level target //add nick yl set
	{0x4300, 0xf8},
	{0x4303, 0xff},
	{0x4304, 0x00},
	{0x4307, 0xff},
	{0x4520, 0x00},
	{0x4521, 0x00},
	{0x4511, 0x22},
	{0x481f, 0x3c}, // MIPI clk prepare min
	{0x4826, 0x00}, // MIPI hs prepare min
	{0x4837, 0x17}, // MIPI global timing
	{0x4b00, 0x06},
	{0x4b01, 0x0a},
	{0x5000, 0xff}, // bpc on, wpc on
	{0x5001, 0x00}, // awb disable
	{0x5002, 0x41}, // win enable, awb gain enable
	{0x5003, 0x0a}, // buf en, bin auto en
	{0x5004, 0x00}, // size man off
	{0x5043, 0x00},
	{0x5013, 0x00},
	{0x501f, 0x03}, // ISP output data
	{0x503d, 0x00}, // test pattern off
	{0x5180, 0x08}, // manual wb gain on
	{0x5a00, 0x08},
	{0x5b00, 0x01},
	{0x5b01, 0x40},
	{0x5b02, 0x00},
	{0x5b03, 0xf0},
	{0x0100, 0x01}, // wake up from software sleep
	{0x350b, 0x80}, // gain = 8x
	{0x4050, 0x37}, // add nick yl set
	{0x4051, 0x8f}, // add nick yl set
	{0x4837, 0x17}, // MIPI global timing
	{OV5648_REG_DELAY, 50},
    // video setting
	{0x3201, 0x00}, // group 1 start addr 0
	{0x3208, 0x01}, // enter group 1 write mode
	// 1280x960 30fps 2 lane MIPI 420Mbps/lane
	{0x3708, 0x66},
	{0x3709, 0x52},
	{0x370c, 0xc3},
	{0x3800, 0x00}, // xstart = 0
	{0x3801, 0x00}, // x start
	{0x3802, 0x00}, // y start = 0
	{0x3803, 0x02}, // yl set
	{0x3804, 0x0a}, // xend = 2623
	{0x3805, 0x3f}, // xend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa1}, // yend  yl set
	{0x3808, 0x05}, // x output size = 1280
	{0x3809, 0x00}, // x output size
	{0x380a, 0x03}, // y output size = 960
	{0x380b, 0xc0}, // y output size
	{0x380c, 0x0b}, // hts = 2816
	{0x380d, 0x00}, // hts
	{0x3810, 0x00}, // isp x win = 8
	{0x3811, 0x08}, // isp x win
	{0x3812, 0x00}, // isp y win = 4
	{0x3813, 0x08}, // isp y win yl set
	{0x3814, 0x31}, // x inc
	{0x3815, 0x31}, // y inc
	{0x3817, 0x00}, // hsync start
#if 0
	{0x3820, 0x08}, // flip off, v bin off
	{0x3821, 0x07}, // mirror on, h bin on
#else
  {0x3820, OV5648_REG_VAL_PREVIEW}, // flip off, v bin off
	{0x3821, OV5648_REG_VAL_PREVIEW}, // mirror on, h bin on
#endif
	{0x4004, 0x02}, // black line number
	{0x4005, 0x18}, // add nick yl set
	{0x4837, 0x17}, // MIPI global timing
	{0x3208, 0x11}, // EXIT group 1 write mode
	{OV5648_REG_DELAY, 5},
	{0x3202, 0x08}, // group 2 start addr 0x80
	{0x3208, 0x02}, // enter group 2 write mode
	// 2560x1920 15fps 2 lane MIPI 420Mbps/lane

	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0xc0},
	{0x3800, 0x00}, // xstart = 0
	{0x3801, 0x00}, // xstart
	{0x3802, 0x00}, // ystart = 0
	{0x3803, 0x00}, // ystart
	{0x3804, 0x0a}, // xend = 2623
	{0x3805, 0x3f}, // xend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa3}, // yend
	{0x3808, 0x0a}, // x output size = 2560
	{0x3809, 0x00}, // x output size
	{0x380a, 0x07}, // y output size = 1920
	{0x380b, 0x80}, // y output size
	{0x380c, 0x0b}, // hts = 2816
	{0x380d, 0x00}, // hts
	{0x3810, 0x00}, // isp x win = 16
	{0x3811, 0x10}, // isp x win
	{0x3812, 0x00}, // isp y win = 6
	{0x3813, 0x06}, // isp y win
	{0x3814, 0x11}, // x inc
	{0x3815, 0x11}, // y inc
	{0x3817, 0x00}, // hsync start
#if 0
	{0x3820, 0x40}, // flip off, v bin off
	{0x3821, 0x06}, // mirror on, v bin off
#else
  {0x3820, OV5648_REG_VAL_SNAPSHOT}, // flip off, v bin off
	{0x3821, OV5648_REG_VAL_SNAPSHOT}, // mirror on, v bin off
#endif
	{0x4004, 0x04}, // black line number
	{0x4005, 0x1a}, // blc always update
	{0x4837, 0x17}, // MIPI global timing
	{0x3208, 0x12}, // EXIT group 2 write mode
	{OV5648_REG_DELAY, 5},

	{0x4202, 0x0f}, // stream off
	{0x4800, 0x21},

	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_stream_on[] = {
	{0x4202, 0x00},
	{0x4800, 0x24},
 
	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_stream_off[] = {
	/* Sensor enter LP11*/
	{0x4202, 0x0f},
	{0x4800, 0x21},
	{OV5648_REG_DELAY, 70},//delay(5ms)
	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_win_sxga[] = {
#if 0
	//{0x0100,0x00},//Stream Off 
	{0x3708,0x66},
	{0x3709,0x52},
	{0x370c,0xc3},
	{0x3800,0x00},// x start = 0
	{0x3801,0x00},// x start
	{0x3802,0x00},// y start = 0
	{0x3803,0x00},// y start
	{0x3804,0x0a},// xend = 2623
	{0x3805,0x3f},// xend
	{0x3806,0x07},// yend = 1955
	{0x3807,0xa3},// yend
	{0x3808,0x05},// x output size = 1296
	{0x3809,0x00},// x output size
	{0x380a,0x03},// y output size = 972
	{0x380b,0xc0},// y output size
	{0x380c,0x0b},// hts = 2816
	{0x380d,0x20},// hts
	{0x380e,0x03},// vts = 992
	{0x380f,0xe8},//E0;// vts    
	{0x3810,0x00},// isp x win = 8
	{0x3811,0x08},// isp x win
	{0x3812,0x00},// isp y win = 4
	{0x3813,0x04},// isp y win
	{0x3814,0x31},// x inc
	{0x3815,0x31},// y inc
	{0x3817,0x00},// hsync start
#if 0
	{0x3820,0x08},// flip off, v bin off
	{0x3821,0x07},// mirror on, h bin on
#else
        {0x3820, OV5648_REG_VAL_PREVIEW}, // flip off, v bin off
	{0x3821, OV5648_REG_VAL_PREVIEW}, // mirror on, h bin on
#endif


	{0x4004,0x02},// black line number    //6c 4005 1a;// blc normal freeze
	{0x4005,0x18},// blc normal freeze
	{0x4050,0x37},// blc normal freeze
	{0x4051,0x8f},// blc normal freeze    //6c 350b 80;// gain = 8x
	{0x4837,0x17},// MIPI global timing    
	//{0x0100,0x01},//Stream On
#endif
	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_win_5m[] = {
#if 0
	//{0x0100,0x00},//Stream Off 
	{0x3708,0x63},   
	{0x3709,0x12},
	{0x370c,0xc0},
	{0x3800,0x00},// xstart = 0
	{0x3801,0x00},// xstart
	{0x3802,0x00},// ystart = 0
	{0x3803,0x00},// ystart
	{0x3804,0x0a},// xend = 2623
	{0x3805,0x3f},// xend
	{0x3806,0x07},// yend = 1955
	{0x3807,0xa3},// yend
	{0x3808,0x0a},// x output size = 2592
	{0x3809,0x00},//20;// x output size
	{0x380a,0x07},// y output size = 1944             
	{0x380b,0x80},//98;// y output size               
	{0x380c,0x0b},// hts = 2816                      
	{0x380d,0x20},// hts                      
	{0x380e,0x07},// vts = 1984                      
	{0x380f,0xd0},// vts                          
	{0x3810,0x00},// isp x win = 16                   
	{0x3811,0x10},// isp x win                      
	{0x3812,0x00},// isp y win = 6                    
	{0x3813,0x06},// isp y win                      
	{0x3814,0x11},// x inc                      
	{0x3815,0x11},// y inc                      
	{0x3817,0x00},// hsync start                      
#if 0
	{0x3820,0x08},// flip off, v bin off
	{0x3820,0x40},// flip off, v bin off              
#else
        {0x3820, OV5648_REG_VAL_PREVIEW}, // flip off, v bin off
	{0x3821, OV5648_REG_VAL_PREVIEW}, // mirror on, h bin on
#endif

	{0x4004,0x04},// black line number                
	{0x4005,0x1a},// blc always update    //6c 350b 40;// gain = 4x
	{0x4837,0x17},// MIPI global timing               
	//{0x0100,0x01},//Stream On    
#endif
	{OV5648_REG_END, 0x00},	/* END MARKER */
};

static int ov5648_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov5648_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov5648_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
        unsigned char flag = 0;
        unsigned char hvflip = 0;

	while (vals->reg_num != OV5648_REG_END) {
                flag = 0;
                hvflip = 0;
		if (vals->reg_num == OV5648_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		}
                else if (vals->reg_num == 0x3821) {//set mirror
			if (vals->value == OV5648_REG_VAL_PREVIEW) {
                                vals->value = 0x01;
                                flag = 1;
                                hvflip = ov5648_isp_parm.mirror;
			} else if (vals->value == OV5648_REG_VAL_SNAPSHOT) {
				vals->value = 0x00;
                                flag = 1;
                                hvflip = ov5648_isp_parm.mirror;
			}

		}
                else if (vals->reg_num == 0x3820) {//set flip
			if (vals->value == OV5648_REG_VAL_PREVIEW) {
				vals->value = 0x00;
                                flag = 1;
                                hvflip = ov5648_isp_parm.flip;
			}
			else if (vals->value == OV5648_REG_VAL_SNAPSHOT) {
				vals->value = 0x00;
                                flag = 1;
                                hvflip = ov5648_isp_parm.flip;
			}

		}

                if (flag) {
                        if (hvflip)
                                vals->value |= 0x06;
                        else
                                vals->value &= 0xf9;

                        printk("0x%04x=0x%02x\n", vals->reg_num, vals->value);
                }


		ret = ov5648_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;

		vals++;
	}
	return 0;
}

/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical = 0x180;
static unsigned int bg_ratio_typical = 0x16e;

/*
	index: index of otp group. (0, 1, 2)
 	return: 0, group index is empty
 	1, group index has invalid data
 	2, group index has valid data
*/
static int ov5648_check_otp(struct v4l2_subdev *sd, unsigned short index)
{
	unsigned char temp;
	unsigned short i;
	unsigned short address;
	int ret;

	if (index <= 2){
		//read otp into buffer
		ov5648_write(sd, 0x3d84, 0xc0);

		ov5648_write(sd, 0x3d85, 0x00);
		ov5648_write(sd, 0x3d86, 0x0f);
		ov5648_write(sd, 0x3d81, 0x01);
		mdelay(10);
		address = 0x3d05 + (index-1) * 9;
		ret = ov5648_read(sd, address, &temp);
		if (ret < 0)
			return ret;

		// disable otp read
		ov5648_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov5648_write(sd, 0x3d00 + i, 0x00);
	} else {
		//read otp into buffer
		ov5648_write(sd, 0x3d84, 0xc0);

		ov5648_write(sd, 0x3d85, 0x10);
		ov5648_write(sd, 0x3d86, 0x1f);
		ov5648_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d07;
		ret = ov5648_read(sd, address, &temp);
		if (ret < 0)
			return ret;

		// disable otp read
		ov5648_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov5648_write(sd, 0x3d00 + i, 0x00);
	}

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
static int ov5648_read_otp(struct v4l2_subdev *sd,
				unsigned short index, struct otp_struct* otp)
{
	unsigned char temp,temp1;
	unsigned short address;
	unsigned short i;
	int ret;

	if (index == 1){
		// read otp into buffer
		ov5648_write(sd, 0x3d84, 0xc0);

		ov5648_write(sd, 0x3d85, 0x00);
		ov5648_write(sd, 0x3d86, 0x0f);
		ov5648_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d05;
		ret = ov5648_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->module_integrator_id = (unsigned int)(temp & 0x7f);

		ret = ov5648_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->lens_id = (unsigned int)temp;

		ret = ov5648_read(sd, address + 4, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[0] = (unsigned int)temp;

		ret = ov5648_read(sd, address + 5, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[1] = (unsigned int)temp;

		ret = ov5648_read(sd, address + 6, &temp1);
		if (ret < 0)
			return ret;

		ret = ov5648_read(sd, address + 2, &temp);
		if (ret < 0)
			return ret;
		otp->rg_ratio = (temp<<2) + (temp1>>6);

		ret = ov5648_read(sd, address + 3, &temp);
		if (ret < 0)
			return ret;
		otp->bg_ratio = (temp<<2) + ((temp1>>4)&0x03);

		ret = ov5648_read(sd, address + 7, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + ((temp1>>2)&0x03);

		ret = ov5648_read(sd, address + 8, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + (temp1&0x03);

		// disable otp read
		ov5648_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov5648_write(sd, 0x3d00 + i, 0x00);

		}else if (index == 2){
		// read otp into buffer
		ov5648_write(sd, 0x3d84, 0xc0);

		ov5648_write(sd, 0x3d85, 0x00);
		ov5648_write(sd, 0x3d86, 0x0f);
		ov5648_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d0e;
		ret = ov5648_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->module_integrator_id = (unsigned int)(temp & 0x7f);

		ret = ov5648_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->lens_id = (unsigned int)temp;

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov5648_write(sd, 0x3d00 + i, 0x00);

		// read otp into buffer
		ov5648_write(sd, 0x3d84, 0xc0);

		ov5648_write(sd, 0x3d85, 0x10);
		ov5648_write(sd, 0x3d86, 0x1f);
		ov5648_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d00;

		ret = ov5648_read(sd, address + 2, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[0] = (unsigned int)temp;

		ret = ov5648_read(sd, address + 3, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[1] = (unsigned int)temp;

		ret = ov5648_read(sd, address + 4, &temp1);
		if (ret < 0)
			return ret;

		ret = ov5648_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->rg_ratio = (temp<<2) + (temp1>>6);

		ret = ov5648_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->bg_ratio = (temp<<2) + ((temp1>>4)&0x03);

		ret = ov5648_read(sd, address + 5, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + ((temp1>>2)&0x03);

		ret = ov5648_read(sd, address + 6, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + (temp1&0x03);

		// disable otp read
		ov5648_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov5648_write(sd, 0x3d00 + i, 0x00);

		}else{
		// read otp into buffer
		ov5648_write(sd, 0x3d84, 0xc0);

		ov5648_write(sd, 0x3d85, 0x10);
		ov5648_write(sd, 0x3d86, 0x1f);
		ov5648_write(sd, 0x3d81, 0x01);
		mdelay(10);

		address = 0x3d17;
		ret = ov5648_read(sd, address, &temp);
		if (ret < 0)
			return ret;
		otp->module_integrator_id = (unsigned int)(temp & 0x7f);

		ret = ov5648_read(sd, address + 1, &temp);
		if (ret < 0)
			return ret;
		otp->lens_id = (unsigned int)temp;

		ret = ov5648_read(sd, address + 4, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[0] = (unsigned int)temp;

		ret = ov5648_read(sd, address + 5, &temp);
		if (ret < 0)
			return ret;
		otp->user_data[1] = (unsigned int)temp;

		ret = ov5648_read(sd, address + 6, &temp1);
		if (ret < 0)
			return ret;

		ret = ov5648_read(sd, address + 2, &temp);
		if (ret < 0)
			return ret;
		otp->rg_ratio = (temp<<2) + (temp1>>6);

		ret = ov5648_read(sd, address + 3, &temp);
		if (ret < 0)
			return ret;
		otp->bg_ratio = (temp<<2) + ((temp1>>4)&0x03);

		ret = ov5648_read(sd, address + 7, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + ((temp1>>2)&0x03);

		ret = ov5648_read(sd, address + 8, &temp);
		if (ret < 0)
			return ret;
		otp->light_rg = (temp<<2) + (temp1&0x03);

		// disable otp read
		ov5648_write(sd, 0x3d81, 0x00);

		// clear otp buffer
		for (i = 0; i < 16; i++)
			ov5648_write(sd, 0x3d00 + i, 0x00);
		}

        return ret;
}

/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov5648_update_awb_gain(struct v4l2_subdev *sd,
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	if (R_gain > 0x400) {
		ov5648_write(sd, 0x5186, (unsigned char)(R_gain >> 8));
		ov5648_write(sd, 0x5187, (unsigned char)(R_gain & 0x00ff));
	}

	if (G_gain > 0x400) {
		ov5648_write(sd, 0x5188, (unsigned char)(G_gain >> 8));
		ov5648_write(sd, 0x5189, (unsigned char)(G_gain & 0x00ff));
	}

	if (B_gain > 0x400) {
		ov5648_write(sd, 0x518a, (unsigned char)(B_gain >> 8));
		ov5648_write(sd, 0x518b, (unsigned char)(B_gain & 0x00ff));
	}

	return 0;
}

/*
	call this function after OV5648 initialization
	return value: 0 update success
	1, no OTP
*/
static int ov5648_update_otp(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	unsigned int i;
	unsigned int otp_index;
	unsigned int temp;
	unsigned int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int ret = 0;

        memset(&current_otp, 0, sizeof(current_otp));

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for (i = 1; i <= 3; i++) {
		temp = ov5648_check_otp(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}

	if (i > 3)
		// no valid wb OTP data
		return 1;

	ret = ov5648_read_otp(sd, otp_index, &current_otp);
	if (ret < 0)
		return ret;

	if (!current_otp.bg_ratio) {
		printk(KERN_ERR "ERROR ov5648_update_otp: current bg ratio is invalid.\n");
		return 1;
	}

	if (!current_otp.rg_ratio) {
		printk(KERN_ERR "ERROR ov5648_update_otp: current rg ratio is invalid.\n");
		return 1;
	}

	if (current_otp.light_rg)
		current_otp.rg_ratio = current_otp.rg_ratio * (current_otp.light_rg +512)/1024;

	if (current_otp.light_bg)
		current_otp.bg_ratio = current_otp.bg_ratio * (current_otp.light_bg +512)/1024;

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

	ov5648_update_awb_gain(sd, R_gain, G_gain, B_gain);

	return 0;
}

static int sensor_get_vcm_range(void **vals, int val)
{
	printk("*sensor_get_vcm_range\n");
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
	default:
		return -EINVAL;
	}

	return 0;
}

static struct isp_effect_ops sensor_effect_ops = {
	.get_exposure = sensor_get_exposure,
	.get_iso = sensor_get_iso,
	.get_contrast = sensor_get_contrast,
	.get_saturation = sensor_get_saturation,
	.get_white_balance = sensor_get_white_balance,
	.get_brightness = sensor_get_brightness,
	.get_sharpness = sensor_get_sharpness,
	.get_vcm_range = sensor_get_vcm_range,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,

};


static int ov5648_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov5648_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->frame_rate = FRAME_RATE_DEFAULT;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
	info->binning = 1;

	ret = ov5648_write_array(sd, ov5648_init_regs);
	if (ret < 0)
		return ret;

	ret = ov5648_update_otp(sd);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov5648_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov5648_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	if (v != OV5648_CHIP_ID_H)
		return -ENODEV;
	ret = ov5648_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	if (v != OV5648_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct ov5648_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov5648_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV5648_FMTS ARRAY_SIZE(ov5648_formats)

static struct ov5648_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
	struct isp_regb_vals *regs_aecgc_win_matrix;
	struct isp_regb_vals *regs_aecgc_win_center;
	struct isp_regb_vals *regs_aecgc_win_central_weight;

} ov5648_win_sizes[] = {
	/* SXGA */
	{
		.width		= SXGA_WIDTH,
		.height		= SXGA_HEIGHT,
		.vts		= 0x3d8,//0x3d8
		.framerate	= 30,
		.max_gain_dyn_frm = 0x80,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x80,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov5648_win_sxga,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_2M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_2M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_2M_central_weight,
	},
	/* 2560*1920 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x7c0,//0x7b6
		.framerate	= 15,//10,
		.max_gain_dyn_frm = 0x80,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x80,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov5648_win_5m,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_5M_matrix,
		.regs_aecgc_win_center    		= isp_aecgc_win_5M_center,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_5M_central_weight,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(ov5648_win_sizes))

static int ov5648_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV5648_FMTS)
		return -EINVAL;

	*code = ov5648_formats[index].mbus_code;
	return 0;
}

static int ov5648_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov5648_format_struct **ret_fmt,
		struct ov5648_win_size **ret_wsize)
{
	int index;
	struct ov5648_win_size *wsize;

	for (index = 0; index < N_OV5648_FMTS; index++)
		if (ov5648_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV5648_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov5648_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov5648_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov5648_win_sizes; wsize < ov5648_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov5648_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov5648_formats[index].colorspace;
	return 0;
}

static int ov5648_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov5648_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov5648_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov5648_format_struct *fmt_s = NULL;
	struct ov5648_win_size *wsize = NULL;
	int ret = 0;
	int framerate = 0;
	static int last_frame_rate = 0;

	ret = ov5648_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov5648_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if (((info->win != wsize)
		|| (last_frame_rate != info->frame_rate))
		&& wsize->regs) {
		last_frame_rate = info->frame_rate;
		ret = ov5648_write_array(sd, wsize->regs);
		if (ret)
			return ret;

		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 429; //422; //282; //TBD/* Mbps. */
		data->sensor_vts_address1 = info->vts_address1;
		data->sensor_vts_address2 = info->vts_address2;
		data->max_gain_dyn_frm = wsize->max_gain_dyn_frm;
		data->min_gain_dyn_frm = wsize->min_gain_dyn_frm;
		data->max_gain_fix_frm = wsize->max_gain_fix_frm;
		data->min_gain_fix_frm = wsize->min_gain_fix_frm;
		data->framerate = wsize->framerate;
		data->binning = info->binning;
	
		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg[data->reg_num].addr = 0x3208;
			data->reg[data->reg_num++].data = 0xa2;
			if (info->frame_rate != FRAME_RATE_AUTO) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num++].data = data->vts & 0x00ff;
			}
		} else if ((wsize->width == SXGA_WIDTH)
			&& (wsize->height == SXGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg[data->reg_num].addr = 0x3208;
			data->reg[data->reg_num++].data = 0xa1;
			if (info->frame_rate != FRAME_RATE_AUTO) {
				if (info->frame_rate == FRAME_RATE_DEFAULT)
					framerate = wsize->framerate;
				else
					framerate = info->frame_rate;
				data->framerate = framerate;
				data->vts = (data->vts * wsize->framerate) / framerate;
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

static int ov5648_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov5648_write_array(sd, ov5648_stream_on);
	else
		ret = ov5648_write_array(sd, ov5648_stream_off);

	return ret;
}

static int ov5648_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov5648_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov5648_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5648_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5648_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov5648_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov5648_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov5648_frame_rates[interval->index];
	return 0;
}

static int ov5648_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov5648_win_size *win = &ov5648_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov5648_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov5648_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov5648_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov5648_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov5648_isp_parm;
		break;
	case V4L2_CID_GET_EFFECT_FUNC:
		ctrl->value = (int)&sensor_effect_ops;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov5648_set_framerate(struct v4l2_subdev *sd, int framerate)
	{
		struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
		printk("ov5648_set_framerate %d\n", framerate);
		if (framerate < FRAME_RATE_AUTO)
			framerate = FRAME_RATE_AUTO;
		else if (framerate > 30)
			framerate = 30;
		info->frame_rate = framerate;
		return 0;
	}


static int ov5648_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
//	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		ret = ov5648_set_framerate(sd, ctrl->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals)
{
	int i = 0;
	int ret = 0;

	for(i = 0; i < N_WIN_SIZES; i++) {
		if (width == ov5648_win_sizes[i].width && height == ov5648_win_sizes[i].height) {
			switch (meter_mode){
				case METER_MODE_MATRIX:
					*vals = ov5648_win_sizes[i].regs_aecgc_win_matrix;
					break;
				case METER_MODE_CENTER:
					*vals = ov5648_win_sizes[i].regs_aecgc_win_center;
					break;
				case METER_MODE_CENTRAL_WEIGHT:
					*vals = ov5648_win_sizes[i].regs_aecgc_win_central_weight;
					break;
				default:
					break;
				}
			break;
		}
	}
	return ret;
}

static int ov5648_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV5648, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5648_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov5648_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov5648_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov5648_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov5648_core_ops = {
	.g_chip_ident = ov5648_g_chip_ident,
	.g_ctrl = ov5648_g_ctrl,
	.s_ctrl = ov5648_s_ctrl,
	.queryctrl = ov5648_queryctrl,
	.reset = ov5648_reset,
	.init = ov5648_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov5648_g_register,
	.s_register = ov5648_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov5648_video_ops = {
	.enum_mbus_fmt = ov5648_enum_mbus_fmt,
	.try_mbus_fmt = ov5648_try_mbus_fmt,
	.s_mbus_fmt = ov5648_s_mbus_fmt,
	.s_stream = ov5648_s_stream,
	.cropcap = ov5648_cropcap,
	.g_crop	= ov5648_g_crop,
	.s_parm = ov5648_s_parm,
	.g_parm = ov5648_g_parm,
	.enum_frameintervals = ov5648_enum_frameintervals,
	.enum_framesizes = ov5648_enum_framesizes,
};

static const struct v4l2_subdev_ops ov5648_ops = {
	.core = &ov5648_core_ops,
	.video = &ov5648_video_ops,
};

static ssize_t ov5648_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", rg_ratio_typical);
}

static ssize_t ov5648_rg_ratio_typical_store(struct device *dev,
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

static ssize_t ov5648_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", bg_ratio_typical);
}

static ssize_t ov5648_bg_ratio_typical_store(struct device *dev,
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

static DEVICE_ATTR(ov5648_rg_ratio_typical, 0664, ov5648_rg_ratio_typical_show, ov5648_rg_ratio_typical_store);
static DEVICE_ATTR(ov5648_bg_ratio_typical, 0664, ov5648_bg_ratio_typical_show, ov5648_bg_ratio_typical_store);

static int ov5648_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov5648_info *info;
	struct comip_camera_subdev_platform_data *ov5648_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct ov5648_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov5648_ops);

	/* Make sure it's an ov5648 */
	ret = ov5648_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov5648 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov5648 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ov5648_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;

	if (ov5648_setting) {
		if (ov5648_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			ov5648_isp_parm.mirror = 1;
		else ov5648_isp_parm.mirror = 0;

		if (ov5648_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			ov5648_isp_parm.flip = 1;
		else ov5648_isp_parm.flip = 0;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov5648_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5648_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5648_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov5648_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5648_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5648_bg_ratio_typical;
	}

	return 0;

err_create_dev_attr_ov5648_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_ov5648_rg_ratio_typical);
err_create_dev_attr_ov5648_rg_ratio_typical:
	kfree(info);
	return ret;
}

static int ov5648_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5648_info *info = container_of(sd, struct ov5648_info, sd);

	device_remove_file(&client->dev, &dev_attr_ov5648_rg_ratio_typical);
	device_remove_file(&client->dev, &dev_attr_ov5648_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov5648_id[] = {
	{ "ov5648-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5648_id);

static struct i2c_driver ov5648_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov5648-mipi-raw",
	},
	.probe		= ov5648_probe,
	.remove		= ov5648_remove,
	.id_table	= ov5648_id,
};

static __init int init_ov5648(void)
{
	return i2c_add_driver(&ov5648_driver);
}

static __exit void exit_ov5648(void)
{
	i2c_del_driver(&ov5648_driver);
}

fs_initcall(init_ov5648);
module_exit(exit_ov5648);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov5648 sensors");
MODULE_LICENSE("GPL");
