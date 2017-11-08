
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
#include "ov12830.h"


#define OV12830_CHIP_ID_H	(0xc8)
#define OV12830_CHIP_ID_L	(0x30)
#define OV12830_CHIP_VERSION	(0x20)

#define QXGA_WIDTH		2112//1920
#define QXGA_HEIGHT	    1500//1080
#define MAX_WIDTH		4224
#define MAX_HEIGHT		3000
#define MAX_PREVIEW_WIDTH QXGA_WIDTH
#define MAX_PREVIEW_HEIGHT QXGA_HEIGHT



#define OV12830_REG_END		0xffff
#define OV12830_REG_DELAY	0xfffe

//#define OV12830_OTP_FUNC
//#define OV12830_DEBUG_FUNC

struct ov12830_format_struct;

struct ov12830_info {
	struct v4l2_subdev sd;
	struct ov12830_format_struct *fmt;
	struct ov12830_win_size *win;
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
//	int customer_id;
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int user_data[5];
	int lenc[62];
	int VCM_end;
	int VCM_start;
};

static struct regval_list ov12830_init_regs[] = {
//global setting
  {0x0103, 0x01},
 // delay 5ms
 {OV12830_REG_DELAY, 5},
  {0x3001, 0x06},
  {0x3002, 0x80},
  {0x3011, 0x41},
  {0x3014, 0x16},
  {0x3015, 0x0b},
  {0x3022, 0x03},
  {0x3090, 0x02},
  {0x3091, 0x11}, // for 19.5Mclk ;1b
  {0x3092, 0x00},
  {0x3093, 0x00},
  {0x3098, 0x03},
  {0x3099, 0x15}, //for 19.5Mclk ;11
  {0x309c, 0x01},
  {0x30b3, 0x82}, // for 19.5Mclk ;6e
  {0x30b4, 0x03},
  {0x30b5, 0x04},
   // {OV12830_REG_DELAY, 5},
  {0x3106, 0x01},//neil 0813
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
  {0x3500, 0x00},
  {0x3501, 0x97},
  {0x3502, 0x00},
  {0x3503, 0x27},
  {0x350a, 0x00},
  {0x350b, 0x80},
  {0x3602, 0x18},
  {0x3612, 0x80},
  {0x3621, 0xb5},
  {0x3622, 0x0b},
  {0x3623, 0x28},
  {0x3631, 0xb3},
  {0x3634, 0x04},
  {0x3660, 0x80},
  {0x3662, 0x10},
  {0x3663, 0xf0},
  {0x3667, 0x00},
  {0x366f, 0x20},
  {0x3680, 0xb5},
  {0x3682, 0x00},
  {0x3701, 0x12},
  {0x3702, 0x88},
  {0x3708, 0xe6},
  {0x3709, 0xc7},
  {0x370b, 0xa0},
  {0x370d, 0x11},
  {0x370e, 0x00},
  {0x371c, 0x01},
  {0x371f, 0x1b},
  {0x3724, 0x10},
  {0x3726, 0x00},
  {0x372a, 0x09},
  {0x3739, 0xb0},
  {0x373c, 0x40},
  {0x376b, 0x44},
  {0x377b, 0x44},
  {0x3780, 0x22},
  {0x3781, 0xc8},
  {0x3783, 0x31},
  {0x3786, 0x16},
  {0x3787, 0x02},
  {0x3796, 0x84},
  {0x379c, 0x0c},
  {0x37c5, 0x00},
  {0x37c6, 0x00},
  {0x37c7, 0x00},
  {0x37c9, 0x00},
  {0x37ca, 0x00},
  {0x37cb, 0x00},
  {0x37cc, 0x00},
  {0x37cd, 0x00},
  {0x37ce, 0x10},
  {0x37cf, 0x00},
  {0x37d0, 0x00},
  {0x37d1, 0x00},
  {0x37d2, 0x00},
  {0x37de, 0x00},
  {0x37df, 0x00},
  {0x3800, 0x00},
  {0x3801, 0x00},
  {0x3802, 0x00},
  {0x3803, 0x00},
  {0x3804, 0x10},
  {0x3805, 0x9f},
  {0x3806, 0x0b},
  {0x3807, 0xc7},
  {0x3808, 0x08},
  {0x3809, 0x40},
  {0x380a, 0x05},
  {0x380b, 0xdc},
  {0x380c, 0x09},//wayne 0813
  {0x380d, 0x80}, // for 30fps a8
  {0x380e, 0x08},//0x09
  {0x380f, 0xe0},//0x80
  {0x3810, 0x00},
  {0x3811, 0x08},
  {0x3812, 0x00},
  {0x3813, 0x02},
  {0x3814, 0x31},
  {0x3815, 0x31},
  {0x3820, 0x14},
  {0x3821, 0x0f},
  {0x3823, 0x00},
  {0x3824, 0x00},
  {0x3825, 0x00},
  {0x3826, 0x00},
  {0x3827, 0x00},
  {0x3829, 0x0b},
  {0x382b, 0x6a},
  {0x4000, 0x18},
  {0x4001, 0x06},
  {0x4002, 0x45},
  {0x4004, 0x08},
  {0x4005, 0x19},
  {0x4006, 0x20},
  {0x4007, 0x90},
  {0x4008, 0x24},
  {0x4009, 0x40},
  {0x400c, 0x00},
  {0x400d, 0x00},
  {0x404e, 0x37},
  {0x404f, 0x8f},
  {0x4058, 0x70},
  {0x4100, 0x2d},
  {0x4101, 0x22},
  {0x4102, 0x04},
  {0x4104, 0x5c},
  {0x4109, 0xa3},
  {0x410a, 0x03},
  {0x4300, 0xff},
  {0x4303, 0x00},
  {0x4304, 0x08},
  {0x4307, 0x30},
  {0x4311, 0x04},
  {0x4511, 0x05},

  {0x4816, 0x52},
  {0x481f, 0x30},
  {0x4826, 0x2c},
  {0x4a00, 0xaa},
  {0x4a03, 0x01},
  {0x4a05, 0x08},
  {0x4d00, 0x05},
  {0x4d01, 0x19},
  {0x4d02, 0xfd},
  {0x4d03, 0xd1},
  {0x4d04, 0xff},
  {0x4d05, 0xff},
  {0x4d07, 0x04},
  {0x4837, 0x09},
  {0x4800, 0x24},//add by wayne 0327
  {0x484b, 0x05},
  {0x5000, 0x06},
  {0x5001, 0x01},
  {0x5002, 0x00},
  {0x5003, 0x21},
  {0x5043, 0x48},
  {0x5013, 0x80},
  {0x501f, 0x00},
  {0x5e00, 0x00},
  {0x5a01, 0x00},
  {0x5a02, 0x00},
  {0x5a03, 0x00},
  {0x5a04, 0x10},
  {0x5a05, 0xa0},
  {0x5a06, 0x0c},
  {0x5a07, 0x78},
  {0x5a08, 0x00},
  {0x5e00, 0x00},
  {0x5e01, 0x41},
  {0x5e11, 0x30},
 {0x0100, 0x01},

//group 1
//preview setting 2112*1500 30fps

{OV12830_REG_DELAY, 50},
{0x3200, 0x00}, // group 0 start addr 0
{0x3208, 0x00},
 //{0x3106, 0x01},
 {0x3708, 0xe6},
 {0x3709, 0xc7},
 {0x3808, 0x08},
 {0x3809, 0x40},
 {0x380a, 0x05},
 {0x380b, 0xdc},
 {0x380c, 0x09},
 {0x380d, 0x80}, //a8
 //{0x380e, 0x08},
 //{0x380f, 0xe0},
 {0x3810, 0x00},
 {0x3811, 0x08},
 {0x3812, 0x00},
 {0x3813, 0x02},
 {0x3814, 0x31},
 {0x3815, 0x31},
 {0x3820, 0x14},
 {0x3821, 0x0f},
{0x3208, 0x10}, //group 0 end

//for capture setting 4224*3000 10/fps
{OV12830_REG_DELAY, 5},
{0x3203, 0x18}, // group 1 start addr 0x80
{0x3208, 0x03},
 //{0x3106, 0x01},
 {0x3708, 0xe3},
 {0x3709, 0xc3},
 {0x3808, 0x10},
 {0x3809, 0x80},
 {0x380a, 0x0b},
 {0x380b, 0xb8},
 {0x380c, 0x12}, //11 for 10fps capture
 {0x380d, 0x80}, //50
// {0x380e, 0x0d}, //0b
 //{0x380f, 0xab}, //e4
 {0x3810, 0x00},
 {0x3811, 0x10},
 {0x3812, 0x00},
 {0x3813, 0x08},
 {0x3814, 0x11},
 {0x3815, 0x11},
 {0x3820, 0x10},
 {0x3821, 0x0e},
 {0x3208, 0x13},//group 1 end
//{0x3106, 0x21},//add for debug 0x3106 write

//enter pl11 state

//{OV12830_REG_DELAY, 10},
	{0x4202, 0x0f},
	{0x4800, 0x01},

	{OV12830_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list ov12830_stream_on[] = {
	{0x4202, 0x00},
	{0x4800, 0x24},

	{OV12830_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov12830_stream_off[] = {
	/* Sensor enter LP11*/	
	{0x4202, 0x0f},	
	{0x4800, 0x01},	
	{OV12830_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list ov12830_win_qxga[] = {

//{0x3106, 0x21},
 {0x3708, 0xe6},
 {0x3709, 0xc7},
 {0x3808, 0x08},
 {0x3809, 0x40},
 {0x380a, 0x05},
 {0x380b, 0xdc},
 {0x380c, 0x09},
 {0x380d, 0x80}, //a8
 //{0x380e, 0x08},
 //{0x380f, 0xe0},
 {0x3810, 0x00},
 {0x3811, 0x08},
 {0x3812, 0x00},
 {0x3813, 0x02},
 {0x3814, 0x31},
 {0x3815, 0x31},
 {0x3820, 0x14},
 {0x3821, 0x0f},	
{OV12830_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov12830_win_12m[] = {
	
//{0x3106, 0x01},
 {0x3708, 0xe3},
 {0x3709, 0xc3},
 {0x3808, 0x10},
 {0x3809, 0x80},
 {0x380a, 0x0b},
 {0x380b, 0xb8},
 {0x380c, 0x12}, //11 for 10fps capture
 {0x380d, 0x80}, //50
 //{0x380e, 0x0d}, //0b
 //{0x380f, 0xab}, //e4
 {0x3810, 0x00},
 {0x3811, 0x10},
 {0x3812, 0x00},
 {0x3813, 0x08},
 {0x3814, 0x11},
 {0x3815, 0x11},
 {0x3820, 0x10},
 {0x3821, 0x0e},	
{OV12830_REG_END, 0x00},	/* END MARKER */
};

static int ov12830_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov12830_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov12830_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV12830_REG_END) {
		if (vals->reg_num == OV12830_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov12830_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

#ifdef OV12830_DEBUG_FUNC
static int ov12830_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;

	//return 0;
	while (vals->reg_num != OV12830_REG_END) {
		if (vals->reg_num == OV12830_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			
			ret = ov12830_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%x, val=0x%x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif

#ifdef OV12830_OTP_FUNC
void ov12830_clear_otp_buffer(struct v4l2_subdev *sd)
{
int i;
// clear otp buffer
for (i=0;i<16;i++) {
ov12830_write(sd,0x3d00 + i, 0x00);
}
}


static int ov12830_otp_read(struct v4l2_subdev *sd, unsigned short reg)
{
	unsigned char value = 0;
	int flag;
	ov12830_read(sd,reg,&value);
	flag = (int)value;
	return flag;
}
/* R/G and B/G of typical camera module is defined here. */
static unsigned int RG_Ratio_Typical = 85;
static unsigned int BG_Ratio_Typical = 85;

// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 3, group index has valid data

static int ov12830_check_otp_wb(struct v4l2_subdev *sd, int index)
{

	int flag, i,ret;
	unsigned char value = 0;
	int bank, address;
	// select bank index
	bank = 0xc0 | index;
	ov12830_write(sd,0x3d84, bank);
	// read otp into buffer
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	// read flag
	address = 0x3d00;
	ret = ov12830_read(sd,address,&value);
	flag = (int)value;
	if(ret<0)
		return ret;
	flag = flag & 0xc0;
	flag = flag >>6;
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	return flag;

}


// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 3, group index has valid data
static int ov12830_check_otp_lenc(struct v4l2_subdev *sd, int index)
{
	int flag, i, bank,ret;
	unsigned char value;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 | (index*4);
	ov12830_write(sd,0x3d84, bank);
	// read otp into buffer
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	// read flag
	address = 0x3d00;
	ret= ov12830_read(sd,address,&value);
	flag = (int)value;
	flag = flag & 0xc0;
	flag = flag>>6;
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	return flag;
}





static int ov12830_check_otp_VCM(struct v4l2_subdev *sd, unsigned short index,int code)
{
	int flag, i, bank,ret;
	unsigned char value;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	ov12830_write(sd,0x3d84, bank);
	// read otp into buffer
	ov12830_write(sd,0x3d81, 0x01);
	// read flag
	address = 0x3d00 + index*4 + code*2;
	ret= ov12830_read(sd,address,&value);
	flag = (int)value;
	flag = flag & 0xc0;
	flag=flag>>6;
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	return flag;
}

// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return: 0,
static int ov12830_read_otp_wb(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp_ptr)
{
	int i, bank;
	int address,temp;
	// select bank index
	bank = 0xc0 | index;
	ov12830_write(sd,0x3d84, bank);
	// read otp into buffer
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	(*otp_ptr).module_integrator_id = ov12830_otp_read(sd,address + 1);
	(*otp_ptr).lens_id = ov12830_otp_read(sd,address + 2);
	(*otp_ptr).production_year = ov12830_otp_read(sd,address + 3);
	(*otp_ptr).production_month = ov12830_otp_read(sd,address + 4);
	(*otp_ptr).production_day = ov12830_otp_read(sd,address + 5);
	temp = ov12830_otp_read(sd,address + 10);
	(*otp_ptr).rg_ratio = (ov12830_otp_read(sd,(address + 6)<<2) + ((temp>>6) & 0x03));
	(*otp_ptr).bg_ratio = (ov12830_otp_read(sd,address + 7)<<2) + ((temp>>4) & 0x03);
	(*otp_ptr).light_rg = (ov12830_otp_read(sd,address + 8) <<2) + ((temp>>2) & 0x03);
	(*otp_ptr).light_bg = (ov12830_otp_read(sd,address + 9)<<2) + (temp & 0x03);
	(*otp_ptr).user_data[0] = ov12830_otp_read(sd,address + 11);
	(*otp_ptr).user_data[1] = ov12830_otp_read(sd,address + 12);
	(*otp_ptr).user_data[2] = ov12830_otp_read(sd,address + 13);
	(*otp_ptr).user_data[3] = ov12830_otp_read(sd,address + 14);
	(*otp_ptr).user_data[4] = ov12830_otp_read(sd,address + 15);
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	return 0;
}


// index: index of otp group. (0, 1, 2)
// otp_ptr: pointer of otp_struct
// return: 0,

static int ov12830_read_otp_lenc(struct v4l2_subdev *sd, 
				unsigned short index, struct otp_struct* otp_ptr)
{
	int bank, i;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 + (index*4);
	ov12830_write(sd,0x3d84, bank);
	// read otp into buffer
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d01;
	for(i=0;i<15;i++) {
	(* otp_ptr).lenc[i]=ov12830_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	// select 2nd bank
	bank++;
	ov12830_write(sd,0x3d84, bank);
	// read otp
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	for(i=15;i<31;i++) {
	(* otp_ptr).lenc[i]=ov12830_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	// select 3rd bank
	bank++;
	ov12830_write(sd,0x3d84, bank);
	// read otp
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	for(i=31;i<47;i++) {
	(* otp_ptr).lenc[i]=ov12830_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	// select 4th bank
	bank++;
	ov12830_write(sd,0x3d84, bank);
	// read otp
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10); // delay 10ms
	address = 0x3d00;
	for(i=47;i<62;i++) {
	(* otp_ptr).lenc[i]=ov12830_otp_read(sd,address);
	address++;
	}
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	return 0;


}

// index: index of otp group. (0, 1, 2)
// code: 0 start code, 1 stop code
// return: 0



int ov12830_read_otp_VCM(struct v4l2_subdev *sd, int index, int code, struct otp_struct * otp_ptr)
{
	int vcm, i, bank;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	ov12830_write(sd,0x3d84, bank);
	// read otp into buffer
	ov12830_write(sd,0x3d81, 0x01);
	// read flag
	address = 0x3d00 + index*4 + code*2;
	vcm = ov12830_otp_read(sd,address);
	address++;
	vcm = (vcm & 0x03) + (ov12830_otp_read(sd,address)<<2);
	if(code==1) {
	(* otp_ptr).VCM_end = vcm;
	}
	else {
	(* otp_ptr).VCM_start = vcm;
	}
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
	ov12830_write(sd,0x3d00 + i, 0x00);
	}
	return 0;
}



/*
	R_gain, sensor red gain of AWB, 0x400 =1
	G_gain, sensor green gain of AWB, 0x400 =1
	B_gain, sensor blue gain of AWB, 0x400 =1
	return 0;
*/
static int ov12830_update_awb_gain(struct v4l2_subdev *sd, 
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	printk("R_gain:%04x, G_gain:%04x, B_gain:%04x \n ", R_gain, G_gain, B_gain);
	if (R_gain>0x400) {
	ov12830_write(sd,0x3400, R_gain>>8);
	ov12830_write(sd,0x3401, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
	ov12830_write(sd,0x3402, G_gain>>8);
	ov12830_write(sd,0x3403, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
	ov12830_write(sd,0x3404, B_gain>>8);
	ov12830_write(sd,0x3405, B_gain & 0x00ff);
	}
	return 0;
}

// otp_ptr: pointer of otp_struct
static int ov12830_update_lenc(struct v4l2_subdev *sd,struct otp_struct * otp_ptr)
{
	int i, temp;
	temp = ov12830_otp_read(sd,0x5000);
	temp = 0x80 | temp;
	ov12830_write(sd,0x5000, temp);
	for(i=0;i<62;i++) {
	ov12830_write(sd,0x5800 + i, (*otp_ptr).lenc[i]);
	}
	return 0;
}


// call this function after OV12830 initialization
// return value: 0 update success
// 1, no OTP
static int ov12830_update_otp_wb(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=1;i<=3;i++) {
	temp = ov12830_check_otp_wb(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i>3) {
	// no valid wb OTP data
	return 1;
	}
	ov12830_read_otp_wb(sd,otp_index, &current_otp);
	if(current_otp.light_rg==0) {
	// no light source information in OTP, light factor = 1
	rg = current_otp.rg_ratio;
	}
	else {
	rg = current_otp.rg_ratio * (current_otp.light_rg +512) / 1024;
	}
	if(current_otp.light_bg==0) {
	// not light source information in OTP, light factor = 1
	bg = current_otp.bg_ratio;
	}
	else {
	bg = current_otp.bg_ratio * (current_otp.light_bg +512) / 1024;
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
	ov12830_update_awb_gain(sd,R_gain, G_gain, B_gain);
	return 0;


}



// call this function after OV12830 initialization
// return value: 0 update success
// 1, no OTP
int ov12830_update_otp_lenc(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	// check first lens correction OTP with valid data
	for(i=1;i<=3;i++) {
	temp = ov12830_check_otp_lenc(sd,i);
	if (temp == 2) {
	otp_index = i;
	break;
	}
	}
	if (i>3) {
	// no valid wb OTP data
	return 1;
	}
	ov12830_read_otp_lenc(sd,otp_index, &current_otp);
	ov12830_update_lenc(sd,&current_otp);
	// success
	return 0;
}

// return: 0 ¨Cuse module DCBLC, 1 ¨Cuse sensor DCBL 2 ¨Cuse defualt DCBLC
int ov12830_check_dcblc(struct v4l2_subdev *sd)
{
	int bank, dcblc;
	int address;
	int temp, flag;
	// select bank 31
	bank = 0xc0 | 31;
	ov12830_write(sd,0x3d84, bank);
	// read otp into buffer
	ov12830_write(sd,0x3d81, 0x01);
	msleep(10);
	address = 0x3d0b;
	dcblc = ov12830_otp_read(sd,address);
	if ((dcblc>=0x15) && (dcblc<=0x40)){
	// module DCBLC value is valid
	flag = 0;
//	clear_otp_buffer();
	ov12830_clear_otp_buffer(sd);
	return flag;
	}
	temp = ov12830_otp_read(sd,0x4000);
	temp = temp | 0x08; // DCBLC manual load enable
	ov12830_write(sd,0x4000, temp);
	address--;
	dcblc = ov12830_otp_read(sd,address);
	if ((dcblc>=0x10) && (dcblc<=0x40)){
	// sensor DCBLC value is valid
	ov12830_write(sd,0x4006, dcblc); // manual load sensor level DCBLC
	flag = 1; // sensor level DCBLC is used
	}
	else{
	ov12830_write(sd,0x4006, 0x20);
	flag = 2; // default DCBLC is used
	}
	// disable otp read
	ov12830_write(sd,0x3d81, 0x00);
//	clear_otp_buffer();
	ov12830_clear_otp_buffer(sd);
	return flag;
}

#endif



static int ov12830_reset(struct v4l2_subdev *sd, u32 val)
{
	printk("*ov12830_reset*\n");
	return 0;
}

static int ov12830_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov12830_info *info = container_of(sd, struct ov12830_info, sd);
	int ret=0;

	printk("*ov12830_init*\n");
	info->fmt = NULL;
	info->win = NULL;
	info->vts_address1 = 0x380e;
	info->vts_address2 = 0x380f;
	info->dynamic_framerate = 0;
	info->binning = 1;
	
	ret=ov12830_write_array(sd, ov12830_init_regs);
	if (ret < 0)
		return ret;

#ifdef OV12830_OTP_FUNC
	printk("*update_otp_wb*\n");
	ret = ov12830_update_otp_wb(sd);
	if (ret < 0)
		return ret;
	printk("*update_otp_lenc*\n");
	ret = ov12830_update_otp_lenc(sd);
#endif

	return ret;
}

static int ov12830_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	printk("*ov12830_detect start*\n");
	ret = ov12830_read(sd, 0x300a, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_H=0x%x ", v);
	if (v != OV12830_CHIP_ID_H)
		return -ENODEV;
	ret = ov12830_read(sd, 0x300b, &v);
	if (ret < 0)
		return ret;
	printk("CHIP_ID_L=0x%x \n", v);
	if (v != OV12830_CHIP_ID_L)
		return -ENODEV;
	ret = ov12830_read(sd, 0x300c, &v);
	printk("CHIP_ID_VERVSION=0x%x \n", v);

	return 0;
}

static struct ov12830_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov12830_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV12830_FMTS ARRAY_SIZE(ov12830_formats)

static struct ov12830_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	struct regval_list *regs; /* Regs to tweak */
} ov12830_win_sizes[] = {
		/* UXGA */
	{
		.width		= QXGA_WIDTH,
		.height		= QXGA_HEIGHT,
		.vts		= 0x8e0,//60hz banding step 0x260
		.framerate	= 30,
		.max_gain_dyn_frm = 0x40,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x40,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov12830_win_qxga,
	},

	/* 4224*3000 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0xdab, //60hz banding step 0x123 
		.framerate	= 10,
		.max_gain_dyn_frm = 0x40,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0x40,
		.min_gain_fix_frm = 0x10,
		.regs 		= ov12830_win_12m,
	},

};
#define N_WIN_SIZES (ARRAY_SIZE(ov12830_win_sizes))

static int ov12830_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	printk("[TEST] ov12830_enum_mbus_fmt\n");
	if (index >= N_OV12830_FMTS)
		return -EINVAL;

	
	*code = ov12830_formats[index].mbus_code;
	return 0;
}

static int ov12830_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov12830_format_struct **ret_fmt,
		struct ov12830_win_size **ret_wsize)
{
	int index;
	struct ov12830_win_size *wsize;

	printk("ov12830_try_fmt_internal N_OV12830_FMTS=%d\n", N_OV12830_FMTS);
	for (index = 0; index < N_OV12830_FMTS; index++)
		if (ov12830_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV12830_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov12830_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov12830_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov12830_win_sizes; wsize < ov12830_win_sizes + N_WIN_SIZES;
	     wsize++)
		{
			if (fmt->width <= wsize->width && fmt->height <= wsize->height)
				break;
	     }
	if (wsize >= ov12830_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov12830_formats[index].colorspace;
	return 0;
}

static int ov12830_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov12830_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov12830_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov12830_info *info = container_of(sd, struct ov12830_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov12830_format_struct *fmt_s;
	struct ov12830_win_size *wsize;
	int ret;

	ret = ov12830_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov12830_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {
		memset(data, 0, sizeof(*data));
		data->vts = wsize->vts;
		data->mipi_clk = 845; /* Mbps. */
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
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa3;
		
			if (!info->dynamic_framerate) {
				data->reg[data->reg_num].addr = info->vts_address1;
				data->reg[data->reg_num++].data = data->vts >> 8;
				data->reg[data->reg_num].addr = info->vts_address2;
				data->reg[data->reg_num++].data = data->vts & 0x00ff;
			}
		} else if ((wsize->width == QXGA_WIDTH)
			&& (wsize->height == QXGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			data->reg_num = 1;
			data->reg[0].addr = 0x3208;
			data->reg[0].data = 0xa0;
			
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

static int ov12830_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	printk("ov12830_s_stream=%d\n", enable);
	if (enable)
		ret = ov12830_write_array(sd, ov12830_stream_on);
	else
		ret = ov12830_write_array(sd, ov12830_stream_off);

	return ret;
}

static int ov12830_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov12830_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov12830_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov12830_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov12830_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov12830_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov12830_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov12830_frame_rates[interval->index];
	return 0;
}

static int ov12830_enum_framesizes(struct v4l2_subdev *sd,
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
		struct ov12830_win_size *win = &ov12830_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov12830_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov12830_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = (struct v4l2_isp_regval *)&ov12830_isp_setting;
		isp_setting.size = ARRAY_SIZE(ov12830_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&ov12830_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int ov12830_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int ov12830_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV12830, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov12830_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov12830_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov12830_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov12830_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov12830_core_ops = {
	.g_chip_ident = ov12830_g_chip_ident,
	.g_ctrl = ov12830_g_ctrl,
	.s_ctrl = ov12830_s_ctrl,
	.queryctrl = ov12830_queryctrl,
	.reset = ov12830_reset,
	.init = ov12830_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov12830_g_register,
	.s_register = ov12830_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov12830_video_ops = {
	.enum_mbus_fmt = ov12830_enum_mbus_fmt,
	.try_mbus_fmt = ov12830_try_mbus_fmt,
	.s_mbus_fmt = ov12830_s_mbus_fmt,
	.s_stream = ov12830_s_stream,
	.cropcap = ov12830_cropcap,
	.g_crop	= ov12830_g_crop,
	.s_parm = ov12830_s_parm,
	.g_parm = ov12830_g_parm,
	.enum_frameintervals = ov12830_enum_frameintervals,
	.enum_framesizes = ov12830_enum_framesizes,
};

static const struct v4l2_subdev_ops ov12830_ops = {
	.core = &ov12830_core_ops,
	.video = &ov12830_video_ops,
};

static int ov12830_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov12830_info *info;
	int ret;

	info = kzalloc(sizeof(struct ov12830_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov12830_ops);

	/* Make sure it's an ov12830 */
	ret = ov12830_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov12830 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov12830 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int ov12830_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov12830_info *info = container_of(sd, struct ov12830_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov12830_id[] = {
	{ "ov12830", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov12830_id);

static struct i2c_driver ov12830_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov12830",
	},
	.probe		= ov12830_probe,
	.remove		= ov12830_remove,
	.id_table	= ov12830_id,
};

static __init int init_ov12830(void)
{
	return i2c_add_driver(&ov12830_driver);
}

static __exit void exit_ov12830(void)
{
	i2c_del_driver(&ov12830_driver);
}

fs_initcall(init_ov12830);
module_exit(exit_ov12830);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov12830 sensors");
MODULE_LICENSE("GPL");
