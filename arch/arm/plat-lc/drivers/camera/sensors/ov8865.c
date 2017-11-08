
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
#include "ov8865_sunny.h"
#include "ov8865_oflim.h"
#include "ov8865_qtech.h"


#define  GRPMODE
#define OV8865_CHIP_ID_H	(0x00)
#define OV8865_CHIP_ID_M	(0x88)
#define OV8865_CHIP_ID_L	(0x65)

#define MAX_WIDTH		3264
#define MAX_HEIGHT		2448

#ifndef BUILD_MASS_PRODUCTION
#define HD_WIDTH	1408
#define HD_HEIGHT	792
#define MAX_PREVIEW_WIDTH	HD_WIDTH
#define MAX_PREVIEW_HEIGHT  HD_HEIGHT
#else
#define MAX_PREVIEW_WIDTH	MAX_WIDTH
#define MAX_PREVIEW_HEIGHT  MAX_HEIGHT
#endif


#define OV8865_REG_END		0xffff
#define OV8865_REG_DELAY	0xfffe

#define OV8865_REG_VAL_PREVIEW  0xff
#define OV8865_REG_VAL_SNAPSHOT 0xfe

#define MODULE_ID_OFLIM		0x02
#define MODULE_ID_SUNNY		0x01
#define MODULE_ID_QTECH		0x0b

static char* factory_ofilm = "oflim";
static char* factory_sunny = "sunny";
static char* factory_qtech = "qtech";
static char* factory_unknown = "unknown";

struct ov8865_win_size *ov8865_win_sizes;
struct v4l2_isp_parm *ov8865_isp_parm;
static struct isp_effect_ops *sensor_effect_ops;
const struct v4l2_isp_regval *ov8865_isp_setting;

static int n_win_sizes;

static int sensor_get_aecgc_win_setting(int width, int height,int meter_mode, void **vals);


struct ov8865_format_struct;
struct ov8865_info {
	struct v4l2_subdev sd;
	struct ov8865_format_struct *fmt;
	struct ov8865_win_size *win;
	int module_id;
	int isp_setting_size;
	int n_win_sizes;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct otp_struct {
	int flag; // bit[7]: info, bit[6]:wb, bit[5]:vcm, bit[4]:lenc
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int lenc[62];
	int VCM_start;
	int VCM_end;
	int VCM_dir;
};

struct ov8865_exp_ratio_entry {
	unsigned int width_last;
	unsigned int height_last;
	unsigned int width_cur;
	unsigned int height_cur;
	unsigned int exp_ratio;
};

struct ov8865_exp_ratio_entry ov8865_exp_ratio_table[] = {
#ifndef BUILD_MASS_PRODUCTION
	{HD_WIDTH, 	HD_HEIGHT, 	MAX_WIDTH, 	MAX_HEIGHT, 0x0173},
	{MAX_WIDTH, MAX_HEIGHT, HD_WIDTH, 	HD_HEIGHT, 	0x00b0},
#else
#endif
};
#define N_EXP_RATIO_TABLE_SIZE  (ARRAY_SIZE(ov8865_exp_ratio_table))

static struct regval_list ov8865_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x0100, 0x00},
	{OV8865_REG_DELAY,0x05},
	{0x3638, 0xff},
	{0x0302, 0x1d},//;26;1e
	{0x0303, 0x00},
	{0x0304, 0x03},
	{0x030d, 0x1d},//;26;1e
	{0x030e, 0x00},
	{0x030f, 0x04},
	{0x0312, 0x01},
	{0x031e, 0x0c},
	{0x3015, 0x01},
	{0x3018, 0x72},
	{0x3020, 0x93},
	{0x3022, 0x01},
	{0x3031, 0x0a},
	{0x3106, 0x01},
	{0x3305, 0xf1},
	{0x3308, 0x00},
	{0x3309, 0x28},
	{0x330a, 0x00},
	{0x330b, 0x20},
	{0x330c, 0x00},
	{0x330d, 0x00},
	{0x330e, 0x00},
	{0x330f, 0x40},
	{0x3307, 0x04},
	{0x3604, 0x04},
	{0x3602, 0x30},
	{0x3605, 0x00},
	{0x3607, 0x20},
	{0x3608, 0x11},
	{0x3609, 0x68},
	{0x360a, 0x40},
	{0x360c, 0xdd},
	{0x360e, 0x0c},
	{0x3610, 0x07},
	{0x3612, 0x86},
	{0x3613, 0x58},
	{0x3614, 0x28},
	{0x3617, 0x40},
	{0x3618, 0x5a},
	{0x3619, 0x9b},
	{0x361c, 0x00},
	{0x361d, 0x60},
	{0x3631, 0x60},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3641, 0x55},
	{0x3646, 0x86},
	{0x3647, 0x27},
	{0x364a, 0x1b},
	{0x3500, 0x00},
	{0x3501, 0x00},
	{0x3502, 0x20},
	{0x3503, 0x30},
	{0x3507, 0x03},
	{0x3508, 0x02},
	{0x3509, 0x00},
	{0x3700, 0x48},
	{0x3701, 0x18},
	{0x3702, 0x50},
	{0x3703, 0x32},
	{0x3704, 0x28},
	{0x3705, 0x00},
	{0x3706, 0x70},
	{0x3707, 0x08},
	{0x3708, 0x48},
	{0x3709, 0x80},
	{0x370a, 0x01},
	{0x370b, 0x70},
	{0x370c, 0x07},
	{0x3718, 0x14},
	{0x3719, 0x31},
	{0x3712, 0x44},
	{0x3714, 0x12},
	{0x371e, 0x31},
	{0x371f, 0x7f},
	{0x3720, 0x0a},
	{0x3721, 0x0a},
	{0x3724, 0x04},
	{0x3725, 0x04},
	{0x3726, 0x0c},
	{0x3728, 0x0a},
	{0x3729, 0x03},
	{0x372a, 0x06},
	{0x372b, 0xa6},
	{0x372c, 0xa6},
	{0x372d, 0xa6},
	{0x372e, 0x0c},
	{0x372f, 0x20},
	{0x3730, 0x02},
	{0x3731, 0x0c},
	{0x3732, 0x28},
	{0x3733, 0x10},
	{0x3734, 0x40},
	{0x3736, 0x30},
	{0x373a, 0x04},
	{0x373b, 0x18},
	{0x373c, 0x14},
	{0x373e, 0x06},
	{0x3755, 0x40},
	{0x3758, 0x00},
	{0x3759, 0x4c},
	{0x375a, 0x0c},
	{0x375b, 0x26},
	{0x375c, 0x40},
	{0x375d, 0x04},
	{0x375e, 0x00},
	{0x375f, 0x28},
	{0x3767, 0x1e},
	{0x3768, 0x04},
	{0x3769, 0x20},
	{0x376c, 0xc0},
	{0x376d, 0xc0},
	{0x376a, 0x08},
	{0x3761, 0x00},
	{0x3762, 0x00},
	{0x3763, 0x00},
	{0x3766, 0xff},
	{0x376b, 0x42},
	{0x3772, 0x46},
	{0x3773, 0x04},
	{0x3774, 0x2c},
	{0x3775, 0x13},
	{0x3776, 0x10},
	{0x37a0, 0x88},
	{0x37a1, 0x7a},
	{0x37a2, 0x7a},
	{0x37a3, 0x02},
	{0x37a4, 0x00},
	{0x37a5, 0x09},
	{0x37a6, 0x00},
	{0x37a7, 0x88},
	{0x37a8, 0xb0},
	{0x37a9, 0xb0},
	{0x3760, 0x00},
	{0x376f, 0x01},
	{0x37aa, 0x88},
	{0x37ab, 0x5c},
	{0x37ac, 0x5c},
	{0x37ad, 0x55},
	{0x37ae, 0x19},
	{0x37af, 0x19},
	{0x37b0, 0x00},
	{0x37b1, 0x00},
	{0x37b2, 0x00},
	{0x37b3, 0x84},
	{0x37b4, 0x84},
	{0x37b5, 0x66},
	{0x37b6, 0x00},
	{0x37b7, 0x00},
	{0x37b8, 0x00},
	{0x37b9, 0xff},
	{0x3800, 0x00},
	{0x3801, 0x0c},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xd3},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x07},
	{0x380d, 0x98},//
	{0x380e, 0x09},
	{0x380f, 0xe8},//;a6
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3813, 0x02},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3820, 0x00},
	{0x3821, 0x46},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3837, 0x18},
	{0x3841, 0xff},
	{0x3846, 0x48},
	{0x3d85, 0x06},
	{0x3d8c, 0x75},
	{0x3d8d, 0xef},
	{0x3f08, 0x16},
	{0x4000, 0xf1},
	{0x4001, 0x04},
	{0x4005, 0x10},
	{0x400b, 0x0c},
	{0x400d, 0x10},
	{0x401b, 0x00},
	{0x401d, 0x00},
	{0x4020, 0x02},
	{0x4021, 0x40},
	{0x4022, 0x03},
	{0x4023, 0x3f},
	{0x4024, 0x07},
	{0x4025, 0xc0},
	{0x4026, 0x08},
	{0x4027, 0xbf},
	{0x4028, 0x00},
	{0x4029, 0x02},
	{0x402a, 0x04},
	{0x402b, 0x04},
	{0x402c, 0x02},
	{0x402d, 0x02},
	{0x402e, 0x08},
	{0x402f, 0x02},
	{0x401f, 0x00},
	{0x4034, 0x3f},
	{0x4300, 0xff},
	{0x4301, 0x00},
	{0x4302, 0x0f},
	{0x4500, 0x68},
	{0x4503, 0x10},
	{0x4601, 0x10},
	{0x481f, 0x32},
	{0x4837, 0x16},
	{0x4850, 0x10},
	{0x4851, 0x32},
	{0x4b00, 0x2a},
	{0x4b0d, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0x18},
	{0x4d02, 0xc3},
	{0x4d03, 0xff},
	{0x4d04, 0xff},
	{0x4d05, 0xff},
	{0x5000, 0x96},
	{0x5001, 0x01},
	{0x5002, 0x08},
	{0x5901, 0x00},
	{0x5e00, 0x00},
	{0x5e01, 0x41},
	{0x0100, 0x01},
	{0x5b00, 0x02},
	{0x5b01, 0xd0},
	{0x5b02, 0x03},
	{0x5b03, 0xff},
	{0x5b05, 0x6c},
	{0x5780, 0xfc},
	{0x5781, 0xdf},
	{0x5782, 0x3f},
	{0x5783, 0x08},
	{0x5784, 0x0c},
	{0x5786, 0x20},
	{0x5787, 0x40},
	{0x5788, 0x08},
	{0x5789, 0x08},
	{0x578a, 0x02},
	{0x578b, 0x01},
	{0x578c, 0x01},
	{0x578d, 0x0c},
	{0x578e, 0x02},
	{0x578f, 0x01},
	{0x5790, 0x01},
	{0x5800, 0x1d},
	{0x5801, 0x0e},
	{0x5802, 0x0c},
	{0x5803, 0x0c},
	{0x5804, 0x0f},
	{0x5805, 0x22},
	{0x5806, 0x0a},
	{0x5807, 0x06},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x07},
	{0x580b, 0x0a},
	{0x580c, 0x06},
	{0x580d, 0x02},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x07},
	{0x5812, 0x06},
	{0x5813, 0x02},
	{0x5814, 0x00},
	{0x5815, 0x00},
	{0x5816, 0x03},
	{0x5817, 0x07},
	{0x5818, 0x09},
	{0x5819, 0x06},
	{0x581a, 0x04},
	{0x581b, 0x04},
	{0x581c, 0x06},
	{0x581d, 0x0a},
	{0x581e, 0x19},
	{0x581f, 0x0d},
	{0x5820, 0x0b},
	{0x5821, 0x0b},
	{0x5822, 0x0e},
	{0x5823, 0x22},
	{0x5824, 0x23},
	{0x5825, 0x28},
	{0x5826, 0x29},
	{0x5827, 0x27},
	{0x5828, 0x13},
	{0x5829, 0x26},
	{0x582a, 0x33},
	{0x582b, 0x32},
	{0x582c, 0x33},
	{0x582d, 0x16},
	{0x582e, 0x14},
	{0x582f, 0x30},
	{0x5830, 0x31},
	{0x5831, 0x30},
	{0x5832, 0x15},
	{0x5833, 0x26},
	{0x5834, 0x23},
	{0x5835, 0x21},
	{0x5836, 0x23},
	{0x5837, 0x05},
	{0x5838, 0x36},
	{0x5839, 0x27},
	{0x583a, 0x28},
	{0x583b, 0x26},
	{0x583c, 0x24},
	{0x583d, 0xdf},

	{OV8865_REG_END, 0x00},	/* END MARKER */
	};

static struct regval_list ov8865_stream_on[] = {
	{0x0100,0x01},
	{OV8865_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8865_stream_off[] = {
	/* Sensor enter LP11*/
	{0x0100,0x00},
	{OV8865_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8865_win_8m[] = {
#ifndef BUILD_MASS_PRODUCTION
	{0x0100, 0x00},
	{0x030f, 0x04},
	{0x3612, 0x86},
	{0x3636, 0x10},
//	{0x3501, 0x90},
//	{0x3502, 0x20},
	{0x3700, 0x48},
	{0x3701, 0x18},
	{0x3702, 0x50},
	{0x3703, 0x32},
	{0x3704, 0x28},
	{0x3706, 0x70},
	{0x3707, 0x08},
	{0x3708, 0x48},
	{0x3709, 0x80},
	{0x370a, 0x01},
	{0x370b, 0x70},
	{0x370c, 0x07},
	{0x3718, 0x14},
	{0x3712, 0x44},
	{0x371e, 0x31},
	{0x371f, 0x7f},
	{0x3720, 0x0a},
	{0x3721, 0x0a},
	{0x3724, 0x04},
	{0x3725, 0x04},
	{0x3726, 0x0c},
	{0x3728, 0x0a},
	{0x3729, 0x03},
	{0x372a, 0x06},
	{0x372b, 0xa6},
	{0x372c, 0xa6},
	{0x372d, 0xa6},
	{0x372e, 0x0c},
	{0x372f, 0x20},
	{0x3730, 0x02},
	{0x3731, 0x0c},
	{0x3732, 0x28},
	{0x3736, 0x30},
	{0x373a, 0x04},
	{0x373b, 0x18},
	{0x373c, 0x14},
	{0x373e, 0x06},
	{0x375a, 0x0c},
	{0x375b, 0x26},
	{0x375c, 0x40},
	{0x375d, 0x04},
	{0x375f, 0x28},
	{0x3767, 0x1e},
	{0x376c, 0xc0},
	{0x376d, 0xc0},
	{0x3772, 0x46},
	{0x3773, 0x04},
	{0x3774, 0x2c},
	{0x3775, 0x13},
	{0x3776, 0x10},
	{0x37a0, 0x88},
	{0x37a1, 0x7a},
	{0x37a2, 0x7a},
	{0x37a3, 0x02},
	{0x37a5, 0x09},
	{0x37a7, 0x88},
	{0x37a8, 0xb0},
	{0x37a9, 0xb0},
	{0x37aa, 0x88},
	{0x37ab, 0x5c},
	{0x37ac, 0x5c},
	{0x37ad, 0x55},
	{0x37ae, 0x19},
	{0x37af, 0x19},
	{0x37b3, 0x84},
	{0x37b4, 0x84},
	{0x37b5, 0x66},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x07},
	{0x380d, 0x98},
	{0x380e, 0x09},
	{0x380f, 0xe8},
	{0x3813, 0x02},
	{0x3814, 0x01},
	{0x3821, 0x46},
	{0x382a, 0x01},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3846, 0x48},
	{0x3d85, 0x06},
	{0x3d8c, 0x75},
	{0x3d8d, 0xef},
	{0x3f08, 0x16},
	{0x4001, 0x04},
	{0x4020, 0x02},
	{0x4021, 0x40},
	{0x4022, 0x03},
	{0x4023, 0x3f},
	{0x4024, 0x07},
	{0x4025, 0xc0},
	{0x4026, 0x08},
	{0x4027, 0xbf},
	{0x4500, 0x68},
	{0x4601, 0x10},
	{0x0100, 0x01},
#endif
	{OV8865_REG_END, 0x00},	/* END MARKER */
};

#ifndef BUILD_MASS_PRODUCTION
static struct regval_list ov8865_win_hd[] = {
	{0x0100, 0x00},
	{0x030f, 0x09},
	{0x3612, 0x89},
	{0x3636, 0x0a},
	//{0x3501, 0x31},
	//{0x3502, 0x00},
	{0x3700, 0x18},
	{0x3701, 0x0c},
	{0x3702, 0x28},
	{0x3703, 0x19},
	{0x3704, 0x14},
	{0x3706, 0x28},
	{0x3707, 0x04},
	{0x3708, 0x24},
	{0x3709, 0x40},
	{0x370a, 0x00},
	{0x370b, 0xa8},
	{0x370c, 0x04},
	{0x3718, 0x12},
	{0x3712, 0x42},
	{0x371e, 0x19},
	{0x371f, 0x40},
	{0x3720, 0x05},
	{0x3721, 0x05},
	{0x3724, 0x02},
	{0x3725, 0x02},
	{0x3726, 0x06},
	{0x3728, 0x05},
	{0x3729, 0x02},
	{0x372a, 0x03},
	{0x372b, 0x53},
	{0x372c, 0xa3},
	{0x372d, 0x53},
	{0x372e, 0x06},
	{0x372f, 0x10},
	{0x3730, 0x01},
	{0x3731, 0x06},
	{0x3732, 0x14},
	{0x3736, 0x20},
	{0x373a, 0x02},
	{0x373b, 0x0c},
	{0x373c, 0x0a},
	{0x373e, 0x03},
	{0x375a, 0x06},
	{0x375b, 0x13},
	{0x375c, 0x20},
	{0x375d, 0x02},
	{0x375f, 0x14},
	{0x3767, 0x04},
	{0x376c, 0x00},
	{0x376d, 0x00},
	{0x3772, 0x23},
	{0x3773, 0x02},
	{0x3774, 0x16},
	{0x3775, 0x12},
	{0x3776, 0x08},
	{0x37a0, 0x44},
	{0x37a1, 0x3d},
	{0x37a2, 0x3d},
	{0x37a3, 0x01},
	{0x37a5, 0x08},
	{0x37a7, 0x44},
	{0x37a8, 0x4c},
	{0x37a9, 0x4c},
	{0x37aa, 0x44},
	{0x37ab, 0x2e},
	{0x37ac, 0x2e},
	{0x37ad, 0x33},
	{0x37ae, 0x0d},
	{0x37af, 0x0d},
	{0x37b3, 0x42},
	{0x37b4, 0x42},
	{0x37b5, 0x33},
	{0x3808, 0x05},
	{0x3809, 0x80},
	{0x380a, 0x03},
	{0x380b, 0x18},
	{0x380c, 0x05},
	{0x380d, 0xbe},
	{0x380e, 0x06},
	{0x380f, 0x60},
	{0x3813, 0x04},
	{0x3814, 0x03},
	{0x3821, 0x67},
	{0x382a, 0x03},
	{0x3830, 0x08},
	{0x3836, 0x02},
	{0x3846, 0x88},
	{0x3d85, 0x13},
	{0x3d8c, 0x00},
	{0x3d8d, 0x00},
	{0x3f08, 0x0b},
	{0x4001, 0x14},
	{0x4020, 0x01},
	{0x4021, 0x20},
	{0x4022, 0x01},
	{0x4023, 0x9f},
	{0x4024, 0x03},
	{0x4025, 0xe0},
	{0x4026, 0x04},
	{0x4027, 0x5f},
	{0x4500, 0x40},
	{0x4601, 0x74},
	{0x0100, 0x01},
	{OV8865_REG_END, 0x00},	/* END MARKER */
};
#endif

static int ov8865_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int ov8865_write(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	int ret;
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov8865_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char flag = 0;
	unsigned char hvflip = 0;

	while (vals->reg_num != OV8865_REG_END) {
		flag = 0;
		hvflip = 0;
		if (vals->reg_num == OV8865_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else if (vals->reg_num == 0x3821) {//set mirror
			if (vals->value == OV8865_REG_VAL_PREVIEW) {
				vals->value = 0x01;
				flag = 1;
				hvflip = ov8865_isp_parm->mirror;
			} else if (vals->value == OV8865_REG_VAL_SNAPSHOT) {
				vals->value = 0x00;
				flag = 1;
				hvflip = ov8865_isp_parm->mirror;
			}

		} else if (vals->reg_num == 0x3820) {//set flip
			if (vals->value == OV8865_REG_VAL_PREVIEW) {
				vals->value = 0x00;
				flag = 1;
				hvflip = ov8865_isp_parm->flip;
			} else if (vals->value == OV8865_REG_VAL_SNAPSHOT) {
				vals->value = 0x00;
				flag = 1;
				hvflip = ov8865_isp_parm->flip;
			}

		}

		if (flag) {
			if (hvflip)
				vals->value |= 0x06;
			else
				vals->value &= 0xf9;
		printk(KERN_DEBUG"0x%04x=0x%02x\n", vals->reg_num, vals->value);
		}
		ret = ov8865_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical;
static unsigned int bg_ratio_typical;
static unsigned int rg_ratio_typical_oflim = 0x14b;
static unsigned int bg_ratio_typical_oflim = 0x140;
static unsigned int rg_ratio_typical_sunny = 0x128;
static unsigned int bg_ratio_typical_sunny = 0x123;
static unsigned int rg_ratio_typical_qtech = 0x11a;
static unsigned int bg_ratio_typical_qtech = 0x11a;

static int ov8865_otp_read(struct v4l2_subdev *sd, unsigned short reg)
{
	int flag = 0;
	unsigned char value=0x0;
	ov8865_read(sd,reg,&value);
	flag = (int)value;
	return flag;
}

// return value:
// bit[7]: 0 no otp info, 1 valid otp info
// bit[6]: 0 no otp wb, 1 valib otp wb
// bit[5]: 0 no otp vcm, 1 valid otp vcm
// bit[4]: 0 no otp lenc, 1 valid otp lenc
static int read_otp(struct v4l2_subdev *sd, struct otp_struct *otp_ptr)
{
	int otp_flag, addr, temp, i;
	//set 0x5002 to 0x00
	ov8865_write(sd, 0x5002, 0x00); // disable OTP_DPC
	// read OTP into buffer
	ov8865_write(sd, 0x3d84, 0xC0);
	ov8865_write(sd, 0x3d88, 0x70); // OTP start address
	ov8865_write(sd, 0x3d89, 0x10);
	ov8865_write(sd, 0x3d8A, 0x70); // OTP end address
	ov8865_write(sd, 0x3d8B, 0xf4);
	ov8865_write(sd, 0x3d81, 0x01); // load otp into buffer
	msleep(10);
	// OTP base information
	otp_flag = ov8865_otp_read(sd,0x7010);
	addr = 0;
	if((otp_flag & 0xc0) == 0x40) {
	addr = 0x7011; // base address of info group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
	addr = 0x7016; // base address of info group 2
	}
	else if((otp_flag & 0x0c) == 0x04) {
	addr = 0x701b; // base address of info group 3
	}
	if(addr != 0) {
		(*otp_ptr).flag = 0x80; // valid info in OTP
		(*otp_ptr).module_integrator_id = ov8865_otp_read(sd,addr);
		(*otp_ptr).lens_id = ov8865_otp_read(sd, addr + 1);
		(*otp_ptr).production_year = ov8865_otp_read(sd, addr + 2);
		(*otp_ptr).production_month = ov8865_otp_read(sd, addr + 3);
		(*otp_ptr).production_day = ov8865_otp_read(sd,addr + 4);
	}
	else {
		(*otp_ptr).flag = 0x00; // not info in OTP
		(*otp_ptr).module_integrator_id = 0;
		(*otp_ptr).lens_id = 0;
		(*otp_ptr).production_year = 0;
		(*otp_ptr).production_month = 0;
		(*otp_ptr).production_day = 0;
	}

	// OTP WB Calibration
	otp_flag = ov8865_otp_read(sd,0x7020);
	addr = 0;
	if((otp_flag & 0xc0) == 0x40) {
		addr = 0x7021; // base address of WB Calibration group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
		addr = 0x7026; // base address of WB Calibration group 2
	}
	else if((otp_flag & 0x0c) == 0x04) {
		addr = 0x702b; // base address of WB Calibration group 3
	}
	if(addr != 0) {
		(*otp_ptr).flag |= 0x40;
		temp = ov8865_otp_read(sd,addr + 4);
		(*otp_ptr).rg_ratio = (ov8865_otp_read(sd,addr)<<2) + ((temp>>6) & 0x03);
		(*otp_ptr).bg_ratio = (ov8865_otp_read(sd,addr + 1)<<2) + ((temp>>4) & 0x03);
		(*otp_ptr).light_rg = (ov8865_otp_read(sd,addr + 2)<<2) + ((temp>>2) & 0x03);
		(*otp_ptr).light_bg = (ov8865_otp_read(sd,addr + 3)<<2) + (temp & 0x03);
	}
	else {
		(*otp_ptr).rg_ratio = 0;
		(*otp_ptr).bg_ratio = 0;
		(*otp_ptr).light_rg = 0;
		(*otp_ptr).light_bg = 0;
	}
	// OTP VCM Calibration
	otp_flag = ov8865_otp_read(sd,0x7030);
	addr = 0;
	if((otp_flag & 0xc0) == 0x40) {

		addr = 0x7031; // base address of VCM Calibration group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
		addr = 0x7034; // base address of VCM Calibration group 2
	}
	else if((otp_flag & 0x0c) == 0x04) {
		addr = 0x7037; // base address of VCM Calibration group 3
	}
	if(addr != 0) {
		(*otp_ptr).flag |= 0x20;
		temp = ov8865_otp_read(sd,addr + 2);
		(* otp_ptr).VCM_start = (ov8865_otp_read(sd,addr)<<2) | ((temp>>6) & 0x03);
		(* otp_ptr).VCM_end = (ov8865_otp_read(sd,addr + 1) << 2) | ((temp>>4) & 0x03);
		(* otp_ptr).VCM_dir = (temp>>2) & 0x03;
	}
	else {
		(* otp_ptr).VCM_start = 0;
		(* otp_ptr).VCM_end = 0;
		(* otp_ptr).VCM_dir = 0;
	}
	// OTP Lenc Calibration
	otp_flag = ov8865_otp_read(sd,0x703a);
	addr = 0;
	if((otp_flag & 0xc0) == 0x40) {
		addr = 0x703b; // base address of Lenc Calibration group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
		addr = 0x7079; // base address of Lenc Calibration group 2
	}
	else if((otp_flag & 0x0c) == 0x04) {
		addr = 0x70b7; // base address of Lenc Calibration group 3
	}
	if(addr != 0) {
		(*otp_ptr).flag |= 0x10;
		for(i=0;i<62;i++) {
		(* otp_ptr).lenc[i]=ov8865_otp_read(sd,addr + i);
	}
	}
	else {
	for(i=0;i<62;i++) {
		(* otp_ptr).lenc[i]=0;
	}
	}
	for(i=0x7010;i<=0x70f4;i++) {
		ov8865_write(sd, i,0); // clear OTP buffer, recommended use continuous write to accelarate
	}
	//set 0x5002 to 0x08
	ov8865_write(sd, 0x5002, 0x08); // enable OTP_DPC
	return (*otp_ptr).flag;
}

// return value:
// bit[7]: 0 no otp info, 1 valid otp info
// bit[6]: 0 no otp wb, 1 valib otp wb
// bit[5]: 0 no otp vcm, 1 valid otp vcm
// bit[4]: 0 no otp lenc, 1 valid otp lenc
static int apply_otp(struct v4l2_subdev *sd)
{
	int rg, bg, R_gain, G_gain, B_gain, Base_gain, temp, i;
	struct otp_struct current_otp; //wayne add
	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);
	// apply OTP WB Calibration
	read_otp(sd,&current_otp);  //wayne add

	info->module_id = current_otp.module_integrator_id;
	if(info->module_id == MODULE_ID_OFLIM){
		rg_ratio_typical = rg_ratio_typical_oflim;
		bg_ratio_typical = bg_ratio_typical_oflim;
	}else if (info->module_id == MODULE_ID_SUNNY){
		rg_ratio_typical = rg_ratio_typical_sunny;
		bg_ratio_typical = bg_ratio_typical_sunny;
	}else if (info->module_id == MODULE_ID_QTECH){
		rg_ratio_typical = rg_ratio_typical_qtech;
		bg_ratio_typical = bg_ratio_typical_qtech;
	}else{
		rg_ratio_typical = rg_ratio_typical_oflim;
		bg_ratio_typical = bg_ratio_typical_oflim;
	}

	if (current_otp.flag & 0x40) {
		if(current_otp.light_rg==0) {
		// no light source information in OTP, light factor = 1
			rg = current_otp.rg_ratio;
		}
		else {
			rg = current_otp.rg_ratio * (current_otp.light_rg + 512) / 1024; //wayne
		}
		if(current_otp.light_bg==0) {
		// not light source information in OTP, light factor = 1
			bg = current_otp.bg_ratio;
		}
		else {
		//	bg = current_otp.bg_ratio;
			bg = current_otp.bg_ratio * (current_otp.light_bg + 512) / 1024;
		}
		//calculate G gain
		R_gain = (rg_ratio_typical*1000) / rg;
		B_gain = (bg_ratio_typical*1000) / bg;
		G_gain = 1000;
		if (R_gain < 1000 || B_gain < 1000) {
			if (R_gain < B_gain)
				Base_gain = R_gain;
			else
				Base_gain = B_gain;
		}
		else {
			Base_gain = G_gain;
		}
		R_gain = 0x400 * R_gain / (Base_gain);
		B_gain = 0x400 * B_gain / (Base_gain);
		G_gain = 0x400 * G_gain / (Base_gain);
		// update sensor WB gain
		if (R_gain>0x400) {
			ov8865_write(sd, 0x5018, R_gain>>6);
			ov8865_write(sd, 0x5019, R_gain & 0x003f);
		}
		if (G_gain>0x400) {
			ov8865_write(sd, 0x501A, G_gain>>6);
			ov8865_write(sd, 0x501B, G_gain & 0x003f);
		}
		if (B_gain>0x400) {
			ov8865_write(sd, 0x501C, B_gain>>6);
			ov8865_write(sd, 0x501D, B_gain & 0x003f);
		}
	}
	// apply OTP Lenc Calibration
	if (current_otp.flag & 0x10) {
		temp = ov8865_otp_read(sd,0x5000);
		temp = 0x80 | temp;
		ov8865_write(sd,0x5000, temp);
		for(i=0;i<62;i++) {
			ov8865_write(sd,0x5800 + i, current_otp.lenc[i]);
		}
	}
	return current_otp.flag;
}

static int sensor_get_vcm_range_oflim(void **vals, int val)
{
	printk(KERN_DEBUG"*sensor_get_vcm_range\n");
	switch (val) {
	case FOCUS_MODE_AUTO:
		*vals = focus_mode_normal_oflim;
		return ARRAY_SIZE(focus_mode_normal_oflim);
	case FOCUS_MODE_INFINITY:
		*vals = focus_mode_infinity_oflim;
		return ARRAY_SIZE(focus_mode_infinity_oflim);
	case FOCUS_MODE_MACRO:
		*vals = focus_mode_macro_oflim;
		return ARRAY_SIZE(focus_mode_macro_oflim);
	case FOCUS_MODE_FIXED:
	case FOCUS_MODE_EDOF:
	case FOCUS_MODE_CONTINUOUS_VIDEO:
	case FOCUS_MODE_CONTINUOUS_PICTURE:
	case FOCUS_MODE_CONTINUOUS_AUTO:
		*vals = focus_mode_normal_oflim;
		return ARRAY_SIZE(focus_mode_normal_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_exposure_oflim(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L6:
		*vals = isp_exposure_l6_oflim;
		return ARRAY_SIZE(isp_exposure_l6_oflim);
	case EXPOSURE_L5:
		*vals = isp_exposure_l5_oflim;
		return ARRAY_SIZE(isp_exposure_l5_oflim);
	case EXPOSURE_L4:
		*vals = isp_exposure_l4_oflim;
		return ARRAY_SIZE(isp_exposure_l4_oflim);
	case EXPOSURE_L3:
		*vals = isp_exposure_l3_oflim;
		return ARRAY_SIZE(isp_exposure_l3_oflim);
	case EXPOSURE_L2:
		*vals = isp_exposure_l2_oflim;
		return ARRAY_SIZE(isp_exposure_l2_oflim);
	case EXPOSURE_L1:
		*vals = isp_exposure_l1_oflim;
		return ARRAY_SIZE(isp_exposure_l1_oflim);
	case EXPOSURE_H0:
		*vals = isp_exposure_h0_oflim;
		return ARRAY_SIZE(isp_exposure_h0_oflim);
	case EXPOSURE_H1:
		*vals = isp_exposure_h1_oflim;
		return ARRAY_SIZE(isp_exposure_h1_oflim);
	case EXPOSURE_H2:
		*vals = isp_exposure_h2_oflim;
		return ARRAY_SIZE(isp_exposure_h2_oflim);
	case EXPOSURE_H3:
		*vals = isp_exposure_h3_oflim;
		return ARRAY_SIZE(isp_exposure_h3_oflim);
	case EXPOSURE_H4:
		*vals = isp_exposure_h4_oflim;
		return ARRAY_SIZE(isp_exposure_h4_oflim);
	case EXPOSURE_H5:
		*vals = isp_exposure_h5_oflim;
		return ARRAY_SIZE(isp_exposure_h5_oflim);
	case EXPOSURE_H6:
		*vals = isp_exposure_h6_oflim;
		return ARRAY_SIZE(isp_exposure_h6_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_iso_oflim(void **vals, int val)
{
	switch(val) {
	case ISO_100:
		*vals = isp_iso_100_oflim;
		return ARRAY_SIZE(isp_iso_100_oflim);
	case ISO_200:
		*vals = isp_iso_200_oflim;
		return ARRAY_SIZE(isp_iso_200_oflim);
	case ISO_400:
		*vals = isp_iso_400_oflim;
		return ARRAY_SIZE(isp_iso_400_oflim);
	case ISO_800:
		*vals = isp_iso_800_oflim;
		return ARRAY_SIZE(isp_iso_800_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_contrast_oflim(void **vals, int val)
{
	switch (val) {
	case CONTRAST_L3:
		*vals = isp_contrast_l3_oflim;
		return ARRAY_SIZE(isp_contrast_l3_oflim);
	case CONTRAST_L2:
		*vals = isp_contrast_l2_oflim;
		return ARRAY_SIZE(isp_contrast_l2_oflim);
	case CONTRAST_L1:
		*vals = isp_contrast_l1_oflim;
		return ARRAY_SIZE(isp_contrast_l1_oflim);
	case CONTRAST_H0:
		*vals = isp_contrast_h0_oflim;
		return ARRAY_SIZE(isp_contrast_h0_oflim);
	case CONTRAST_H1:
		*vals = isp_contrast_h1_oflim;
		return ARRAY_SIZE(isp_contrast_h1_oflim);
	case CONTRAST_H2:
		*vals = isp_contrast_h2_oflim;
		return ARRAY_SIZE(isp_contrast_h2_oflim);
	case CONTRAST_H3:
		*vals = isp_contrast_h3_oflim;
		return ARRAY_SIZE(isp_contrast_h3_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_hdr_oflim(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_hdr_l2_oflim;
		return ARRAY_SIZE(isp_saturation_hdr_l2_oflim);
	case SATURATION_L1:
		*vals = isp_saturation_hdr_l1_oflim;
		return ARRAY_SIZE(isp_saturation_hdr_l1_oflim);
	case SATURATION_H0:
		*vals = isp_saturation_hdr_h0_oflim;
		return ARRAY_SIZE(isp_saturation_hdr_h0_oflim);
	case SATURATION_H1:
		*vals = isp_saturation_hdr_h1_oflim;
		return ARRAY_SIZE(isp_saturation_hdr_h1_oflim);
	case SATURATION_H2:
		*vals = isp_saturation_hdr_h2_oflim;
		return ARRAY_SIZE(isp_saturation_hdr_h2_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_oflim(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_l2_oflim;
		return ARRAY_SIZE(isp_saturation_l2_oflim);
	case SATURATION_L1:
		*vals = isp_saturation_l1_oflim;
		return ARRAY_SIZE(isp_saturation_l1_oflim);
	case SATURATION_H0:
		*vals = isp_saturation_h0_oflim;
		return ARRAY_SIZE(isp_saturation_h0_oflim);
	case SATURATION_H1:
		*vals = isp_saturation_h1_oflim;
		return ARRAY_SIZE(isp_saturation_h1_oflim);
	case SATURATION_H2:
		*vals = isp_saturation_h2_oflim;
		return ARRAY_SIZE(isp_saturation_h2_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_white_balance_oflim(void **vals, int val)
{
	switch (val) {
	case WHITE_BALANCE_AUTO:
		*vals = isp_white_balance_auto_oflim;
		return ARRAY_SIZE(isp_white_balance_auto_oflim);
	case WHITE_BALANCE_INCANDESCENT:
		*vals = isp_white_balance_incandescent_oflim;
		return ARRAY_SIZE(isp_white_balance_incandescent_oflim);
	case WHITE_BALANCE_FLUORESCENT:
		*vals = isp_white_balance_fluorescent_oflim;
		return ARRAY_SIZE(isp_white_balance_fluorescent_oflim);
	case WHITE_BALANCE_WARM_FLUORESCENT:
		*vals = isp_white_balance_warm_fluorescent_oflim;
		return ARRAY_SIZE(isp_white_balance_warm_fluorescent_oflim);
	case WHITE_BALANCE_DAYLIGHT:
		*vals = isp_white_balance_daylight_oflim;
		return ARRAY_SIZE(isp_white_balance_daylight_oflim);
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		*vals = isp_white_balance_cloudy_daylight_oflim;
		return ARRAY_SIZE(isp_white_balance_cloudy_daylight_oflim);
	case WHITE_BALANCE_TWILIGHT:
		*vals = isp_white_balance_twilight_oflim;
		return ARRAY_SIZE(isp_white_balance_twilight_oflim);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade_oflim;
		return ARRAY_SIZE(isp_white_balance_shade_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_brightness_oflim(void **vals, int val)
{
	switch (val) {
	case BRIGHTNESS_L3:
		*vals = isp_brightness_l3_oflim;
		return ARRAY_SIZE(isp_brightness_l3_oflim);
	case BRIGHTNESS_L2:
		*vals = isp_brightness_l2_oflim;
		return ARRAY_SIZE(isp_brightness_l2_oflim);
	case BRIGHTNESS_L1:
		*vals = isp_brightness_l1_oflim;
		return ARRAY_SIZE(isp_brightness_l1_oflim);
	case BRIGHTNESS_H0:
		*vals = isp_brightness_h0_oflim;
		return ARRAY_SIZE(isp_brightness_h0_oflim);
	case BRIGHTNESS_H1:
		*vals = isp_brightness_h1_oflim;
		return ARRAY_SIZE(isp_brightness_h1_oflim);
	case BRIGHTNESS_H2:
		*vals = isp_brightness_h2_oflim;
		return ARRAY_SIZE(isp_brightness_h2_oflim);
	case BRIGHTNESS_H3:
		*vals = isp_brightness_h3_oflim;
		return ARRAY_SIZE(isp_brightness_h3_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_sharpness_oflim(void **vals, int val)
{
	switch (val) {
	case SHARPNESS_L2:
		*vals = isp_sharpness_l2_oflim;
		return ARRAY_SIZE(isp_sharpness_l2_oflim);
	case SHARPNESS_L1:
		*vals = isp_sharpness_l1_oflim;
		return ARRAY_SIZE(isp_sharpness_l1_oflim);
	case SHARPNESS_H0:
		*vals = isp_sharpness_h0_oflim;
		return ARRAY_SIZE(isp_sharpness_h0_oflim);
	case SHARPNESS_H1:
		*vals = isp_sharpness_h1_oflim;
		return ARRAY_SIZE(isp_sharpness_h1_oflim);
	case SHARPNESS_H2:
		*vals = isp_sharpness_h2_oflim;
		return ARRAY_SIZE(isp_sharpness_h2_oflim);
	default:
		return -EINVAL;
	}

	return 0;
}


static struct isp_effect_ops sensor_effect_ops_oflim = {
	.get_exposure = sensor_get_exposure_oflim,
	.get_iso = sensor_get_iso_oflim,
	.get_contrast = sensor_get_contrast_oflim,
	.get_saturation = sensor_get_saturation_oflim,
	.get_saturation_hdr = sensor_get_saturation_hdr_oflim,
	.get_white_balance = sensor_get_white_balance_oflim,
	.get_brightness = sensor_get_brightness_oflim,
	.get_sharpness = sensor_get_sharpness_oflim,
	.get_vcm_range = sensor_get_vcm_range_oflim,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,
};


static int sensor_get_vcm_range_sunny(void **vals, int val)
{
	printk(KERN_DEBUG"*sensor_get_vcm_range\n");
	switch (val) {
	case FOCUS_MODE_AUTO:
		*vals = focus_mode_normal_sunny;
		return ARRAY_SIZE(focus_mode_normal_sunny);
	case FOCUS_MODE_INFINITY:
		*vals = focus_mode_infinity_sunny;
		return ARRAY_SIZE(focus_mode_infinity_sunny);
	case FOCUS_MODE_MACRO:
		*vals = focus_mode_macro_sunny;
		return ARRAY_SIZE(focus_mode_macro_sunny);
	case FOCUS_MODE_FIXED:
	case FOCUS_MODE_EDOF:
	case FOCUS_MODE_CONTINUOUS_VIDEO:
	case FOCUS_MODE_CONTINUOUS_PICTURE:
	case FOCUS_MODE_CONTINUOUS_AUTO:
		*vals = focus_mode_normal_sunny;
		return ARRAY_SIZE(focus_mode_normal_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_exposure_sunny(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L6:
		*vals = isp_exposure_l6_sunny;
		return ARRAY_SIZE(isp_exposure_l6_sunny);
	case EXPOSURE_L5:
		*vals = isp_exposure_l5_sunny;
		return ARRAY_SIZE(isp_exposure_l5_sunny);
	case EXPOSURE_L4:
		*vals = isp_exposure_l4_sunny;
		return ARRAY_SIZE(isp_exposure_l4_sunny);
	case EXPOSURE_L3:
		*vals = isp_exposure_l3_sunny;
		return ARRAY_SIZE(isp_exposure_l3_sunny);
	case EXPOSURE_L2:
		*vals = isp_exposure_l2_sunny;
		return ARRAY_SIZE(isp_exposure_l2_sunny);
	case EXPOSURE_L1:
		*vals = isp_exposure_l1_sunny;
		return ARRAY_SIZE(isp_exposure_l1_sunny);
	case EXPOSURE_H0:
		*vals = isp_exposure_h0_sunny;
		return ARRAY_SIZE(isp_exposure_h0_sunny);
	case EXPOSURE_H1:
		*vals = isp_exposure_h1_sunny;
		return ARRAY_SIZE(isp_exposure_h1_sunny);
	case EXPOSURE_H2:
		*vals = isp_exposure_h2_sunny;
		return ARRAY_SIZE(isp_exposure_h2_sunny);
	case EXPOSURE_H3:
		*vals = isp_exposure_h3_sunny;
		return ARRAY_SIZE(isp_exposure_h3_sunny);
	case EXPOSURE_H4:
		*vals = isp_exposure_h4_sunny;
		return ARRAY_SIZE(isp_exposure_h4_sunny);
	case EXPOSURE_H5:
		*vals = isp_exposure_h5_sunny;
		return ARRAY_SIZE(isp_exposure_h5_sunny);
	case EXPOSURE_H6:
		*vals = isp_exposure_h6_sunny;
		return ARRAY_SIZE(isp_exposure_h6_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_iso_sunny(void **vals, int val)
{
	switch(val) {
	case ISO_100:
		*vals = isp_iso_100_sunny;
		return ARRAY_SIZE(isp_iso_100_sunny);
	case ISO_200:
		*vals = isp_iso_200_sunny;
		return ARRAY_SIZE(isp_iso_200_sunny);
	case ISO_400:
		*vals = isp_iso_400_sunny;
		return ARRAY_SIZE(isp_iso_400_sunny);
	case ISO_800:
		*vals = isp_iso_800_sunny;
		return ARRAY_SIZE(isp_iso_800_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_contrast_sunny(void **vals, int val)
{
	switch (val) {
	case CONTRAST_L3:
		*vals = isp_contrast_l3_sunny;
		return ARRAY_SIZE(isp_contrast_l3_sunny);
	case CONTRAST_L2:
		*vals = isp_contrast_l2_sunny;
		return ARRAY_SIZE(isp_contrast_l2_sunny);
	case CONTRAST_L1:
		*vals = isp_contrast_l1_sunny;
		return ARRAY_SIZE(isp_contrast_l1_sunny);
	case CONTRAST_H0:
		*vals = isp_contrast_h0_sunny;
		return ARRAY_SIZE(isp_contrast_h0_sunny);
	case CONTRAST_H1:
		*vals = isp_contrast_h1_sunny;
		return ARRAY_SIZE(isp_contrast_h1_sunny);
	case CONTRAST_H2:
		*vals = isp_contrast_h2_sunny;
		return ARRAY_SIZE(isp_contrast_h2_sunny);
	case CONTRAST_H3:
		*vals = isp_contrast_h3_sunny;
		return ARRAY_SIZE(isp_contrast_h3_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_hdr_sunny(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_hdr_l2_sunny;
		return ARRAY_SIZE(isp_saturation_hdr_l2_sunny);
	case SATURATION_L1:
		*vals = isp_saturation_hdr_l1_sunny;
		return ARRAY_SIZE(isp_saturation_hdr_l1_sunny);
	case SATURATION_H0:
		*vals = isp_saturation_hdr_h0_sunny;
		return ARRAY_SIZE(isp_saturation_hdr_h0_sunny);
	case SATURATION_H1:
		*vals = isp_saturation_hdr_h1_sunny;
		return ARRAY_SIZE(isp_saturation_hdr_h1_sunny);
	case SATURATION_H2:
		*vals = isp_saturation_hdr_h2_sunny;
		return ARRAY_SIZE(isp_saturation_hdr_h2_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_sunny(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_l2_sunny;
		return ARRAY_SIZE(isp_saturation_l2_sunny);
	case SATURATION_L1:
		*vals = isp_saturation_l1_sunny;
		return ARRAY_SIZE(isp_saturation_l1_sunny);
	case SATURATION_H0:
		*vals = isp_saturation_h0_sunny;
		return ARRAY_SIZE(isp_saturation_h0_sunny);
	case SATURATION_H1:
		*vals = isp_saturation_h1_sunny;
		return ARRAY_SIZE(isp_saturation_h1_sunny);
	case SATURATION_H2:
		*vals = isp_saturation_h2_sunny;
		return ARRAY_SIZE(isp_saturation_h2_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_white_balance_sunny(void **vals, int val)
{
	switch (val) {
	case WHITE_BALANCE_AUTO:
		*vals = isp_white_balance_auto_sunny;
		return ARRAY_SIZE(isp_white_balance_auto_sunny);
	case WHITE_BALANCE_INCANDESCENT:
		*vals = isp_white_balance_incandescent_sunny;
		return ARRAY_SIZE(isp_white_balance_incandescent_sunny);
	case WHITE_BALANCE_FLUORESCENT:
		*vals = isp_white_balance_fluorescent_sunny;
		return ARRAY_SIZE(isp_white_balance_fluorescent_sunny);
	case WHITE_BALANCE_WARM_FLUORESCENT:
		*vals = isp_white_balance_warm_fluorescent_sunny;
		return ARRAY_SIZE(isp_white_balance_warm_fluorescent_sunny);
	case WHITE_BALANCE_DAYLIGHT:
		*vals = isp_white_balance_daylight_sunny;
		return ARRAY_SIZE(isp_white_balance_daylight_sunny);
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		*vals = isp_white_balance_cloudy_daylight_sunny;
		return ARRAY_SIZE(isp_white_balance_cloudy_daylight_sunny);
	case WHITE_BALANCE_TWILIGHT:
		*vals = isp_white_balance_twilight_sunny;
		return ARRAY_SIZE(isp_white_balance_twilight_sunny);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade_sunny;
		return ARRAY_SIZE(isp_white_balance_shade_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_brightness_sunny(void **vals, int val)
{
	switch (val) {
	case BRIGHTNESS_L3:
		*vals = isp_brightness_l3_sunny;
		return ARRAY_SIZE(isp_brightness_l3_sunny);
	case BRIGHTNESS_L2:
		*vals = isp_brightness_l2_sunny;
		return ARRAY_SIZE(isp_brightness_l2_sunny);
	case BRIGHTNESS_L1:
		*vals = isp_brightness_l1_sunny;
		return ARRAY_SIZE(isp_brightness_l1_sunny);
	case BRIGHTNESS_H0:
		*vals = isp_brightness_h0_sunny;
		return ARRAY_SIZE(isp_brightness_h0_sunny);
	case BRIGHTNESS_H1:
		*vals = isp_brightness_h1_sunny;
		return ARRAY_SIZE(isp_brightness_h1_sunny);
	case BRIGHTNESS_H2:
		*vals = isp_brightness_h2_sunny;
		return ARRAY_SIZE(isp_brightness_h2_sunny);
	case BRIGHTNESS_H3:
		*vals = isp_brightness_h3_sunny;
		return ARRAY_SIZE(isp_brightness_h3_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_sharpness_sunny(void **vals, int val)
{
	switch (val) {
	case SHARPNESS_L2:
		*vals = isp_sharpness_l2_sunny;
		return ARRAY_SIZE(isp_sharpness_l2_sunny);
	case SHARPNESS_L1:
		*vals = isp_sharpness_l1_sunny;
		return ARRAY_SIZE(isp_sharpness_l1_sunny);
	case SHARPNESS_H0:
		*vals = isp_sharpness_h0_sunny;
		return ARRAY_SIZE(isp_sharpness_h0_sunny);
	case SHARPNESS_H1:
		*vals = isp_sharpness_h1_sunny;
		return ARRAY_SIZE(isp_sharpness_h1_sunny);
	case SHARPNESS_H2:
		*vals = isp_sharpness_h2_sunny;
		return ARRAY_SIZE(isp_sharpness_h2_sunny);
	default:
		return -EINVAL;
	}

	return 0;
}


static struct isp_effect_ops sensor_effect_ops_sunny = {
	.get_exposure = sensor_get_exposure_sunny,
	.get_iso = sensor_get_iso_sunny,
	.get_contrast = sensor_get_contrast_sunny,
	.get_saturation = sensor_get_saturation_sunny,
	.get_saturation_hdr = sensor_get_saturation_hdr_sunny,
	.get_white_balance = sensor_get_white_balance_sunny,
	.get_brightness = sensor_get_brightness_sunny,
	.get_sharpness = sensor_get_sharpness_sunny,
	.get_vcm_range = sensor_get_vcm_range_sunny,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,
};

static int sensor_get_vcm_range_qtech(void **vals, int val)
{
	printk(KERN_DEBUG"*sensor_get_vcm_range\n");
	switch (val) {
	case FOCUS_MODE_AUTO:
		*vals = focus_mode_normal_qtech;
		return ARRAY_SIZE(focus_mode_normal_qtech);
	case FOCUS_MODE_INFINITY:
		*vals = focus_mode_infinity_qtech;
		return ARRAY_SIZE(focus_mode_infinity_qtech);
	case FOCUS_MODE_MACRO:
		*vals = focus_mode_macro_qtech;
		return ARRAY_SIZE(focus_mode_macro_qtech);
	case FOCUS_MODE_FIXED:
	case FOCUS_MODE_EDOF:
	case FOCUS_MODE_CONTINUOUS_VIDEO:
	case FOCUS_MODE_CONTINUOUS_PICTURE:
	case FOCUS_MODE_CONTINUOUS_AUTO:
		*vals = focus_mode_normal_qtech;
		return ARRAY_SIZE(focus_mode_normal_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}
static int sensor_get_exposure_qtech(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L6:
		*vals = isp_exposure_l6_qtech;
		return ARRAY_SIZE(isp_exposure_l6_qtech);
	case EXPOSURE_L5:
		*vals = isp_exposure_l5_qtech;
		return ARRAY_SIZE(isp_exposure_l5_qtech);
	case EXPOSURE_L4:
		*vals = isp_exposure_l4_qtech;
		return ARRAY_SIZE(isp_exposure_l4_qtech);
	case EXPOSURE_L3:
		*vals = isp_exposure_l3_qtech;
		return ARRAY_SIZE(isp_exposure_l3_qtech);
	case EXPOSURE_L2:
		*vals = isp_exposure_l2_qtech;
		return ARRAY_SIZE(isp_exposure_l2_qtech);
	case EXPOSURE_L1:
		*vals = isp_exposure_l1_qtech;
		return ARRAY_SIZE(isp_exposure_l1_qtech);
	case EXPOSURE_H0:
		*vals = isp_exposure_h0_qtech;
		return ARRAY_SIZE(isp_exposure_h0_qtech);
	case EXPOSURE_H1:
		*vals = isp_exposure_h1_qtech;
		return ARRAY_SIZE(isp_exposure_h1_qtech);
	case EXPOSURE_H2:
		*vals = isp_exposure_h2_qtech;
		return ARRAY_SIZE(isp_exposure_h2_qtech);
	case EXPOSURE_H3:
		*vals = isp_exposure_h3_qtech;
		return ARRAY_SIZE(isp_exposure_h3_qtech);
	case EXPOSURE_H4:
		*vals = isp_exposure_h4_qtech;
		return ARRAY_SIZE(isp_exposure_h4_qtech);
	case EXPOSURE_H5:
		*vals = isp_exposure_h5_qtech;
		return ARRAY_SIZE(isp_exposure_h5_qtech);
	case EXPOSURE_H6:
		*vals = isp_exposure_h6_qtech;
		return ARRAY_SIZE(isp_exposure_h6_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_iso_qtech(void **vals, int val)
{
	switch(val) {
	case ISO_100:
		*vals = isp_iso_100_qtech;
		return ARRAY_SIZE(isp_iso_100_qtech);
	case ISO_200:
		*vals = isp_iso_200_qtech;
		return ARRAY_SIZE(isp_iso_200_qtech);
	case ISO_400:
		*vals = isp_iso_400_qtech;
		return ARRAY_SIZE(isp_iso_400_qtech);
	case ISO_800:
		*vals = isp_iso_800_qtech;
		return ARRAY_SIZE(isp_iso_800_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_contrast_qtech(void **vals, int val)
{
	switch (val) {
	case CONTRAST_L3:
		*vals = isp_contrast_l3_qtech;
		return ARRAY_SIZE(isp_contrast_l3_qtech);
	case CONTRAST_L2:
		*vals = isp_contrast_l2_qtech;
		return ARRAY_SIZE(isp_contrast_l2_qtech);
	case CONTRAST_L1:
		*vals = isp_contrast_l1_qtech;
		return ARRAY_SIZE(isp_contrast_l1_qtech);
	case CONTRAST_H0:
		*vals = isp_contrast_h0_qtech;
		return ARRAY_SIZE(isp_contrast_h0_qtech);
	case CONTRAST_H1:
		*vals = isp_contrast_h1_qtech;
		return ARRAY_SIZE(isp_contrast_h1_qtech);
	case CONTRAST_H2:
		*vals = isp_contrast_h2_qtech;
		return ARRAY_SIZE(isp_contrast_h2_qtech);
	case CONTRAST_H3:
		*vals = isp_contrast_h3_qtech;
		return ARRAY_SIZE(isp_contrast_h3_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_hdr_qtech(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_hdr_l2_qtech;
		return ARRAY_SIZE(isp_saturation_hdr_l2_qtech);
	case SATURATION_L1:
		*vals = isp_saturation_hdr_l1_qtech;
		return ARRAY_SIZE(isp_saturation_hdr_l1_qtech);
	case SATURATION_H0:
		*vals = isp_saturation_hdr_h0_qtech;
		return ARRAY_SIZE(isp_saturation_hdr_h0_qtech);
	case SATURATION_H1:
		*vals = isp_saturation_hdr_h1_qtech;
		return ARRAY_SIZE(isp_saturation_hdr_h1_qtech);
	case SATURATION_H2:
		*vals = isp_saturation_hdr_h2_qtech;
		return ARRAY_SIZE(isp_saturation_hdr_h2_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_saturation_qtech(void **vals, int val)
{
	switch (val) {
	case SATURATION_L2:
		*vals = isp_saturation_l2_qtech;
		return ARRAY_SIZE(isp_saturation_l2_qtech);
	case SATURATION_L1:
		*vals = isp_saturation_l1_qtech;
		return ARRAY_SIZE(isp_saturation_l1_qtech);
	case SATURATION_H0:
		*vals = isp_saturation_h0_qtech;
		return ARRAY_SIZE(isp_saturation_h0_qtech);
	case SATURATION_H1:
		*vals = isp_saturation_h1_qtech;
		return ARRAY_SIZE(isp_saturation_h1_qtech);
	case SATURATION_H2:
		*vals = isp_saturation_h2_qtech;
		return ARRAY_SIZE(isp_saturation_h2_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_white_balance_qtech(void **vals, int val)
{
	switch (val) {
	case WHITE_BALANCE_AUTO:
		*vals = isp_white_balance_auto_qtech;
		return ARRAY_SIZE(isp_white_balance_auto_qtech);
	case WHITE_BALANCE_INCANDESCENT:
		*vals = isp_white_balance_incandescent_qtech;
		return ARRAY_SIZE(isp_white_balance_incandescent_qtech);
	case WHITE_BALANCE_FLUORESCENT:
		*vals = isp_white_balance_fluorescent_qtech;
		return ARRAY_SIZE(isp_white_balance_fluorescent_qtech);
	case WHITE_BALANCE_WARM_FLUORESCENT:
		*vals = isp_white_balance_warm_fluorescent_qtech;
		return ARRAY_SIZE(isp_white_balance_warm_fluorescent_qtech);
	case WHITE_BALANCE_DAYLIGHT:
		*vals = isp_white_balance_daylight_qtech;
		return ARRAY_SIZE(isp_white_balance_daylight_qtech);
	case WHITE_BALANCE_CLOUDY_DAYLIGHT:
		*vals = isp_white_balance_cloudy_daylight_qtech;
		return ARRAY_SIZE(isp_white_balance_cloudy_daylight_qtech);
	case WHITE_BALANCE_TWILIGHT:
		*vals = isp_white_balance_twilight_qtech;
		return ARRAY_SIZE(isp_white_balance_twilight_qtech);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade_qtech;
		return ARRAY_SIZE(isp_white_balance_shade_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_brightness_qtech(void **vals, int val)
{
	switch (val) {
	case BRIGHTNESS_L3:
		*vals = isp_brightness_l3_qtech;
		return ARRAY_SIZE(isp_brightness_l3_qtech);
	case BRIGHTNESS_L2:
		*vals = isp_brightness_l2_qtech;
		return ARRAY_SIZE(isp_brightness_l2_qtech);
	case BRIGHTNESS_L1:
		*vals = isp_brightness_l1_qtech;
		return ARRAY_SIZE(isp_brightness_l1_qtech);
	case BRIGHTNESS_H0:
		*vals = isp_brightness_h0_qtech;
		return ARRAY_SIZE(isp_brightness_h0_qtech);
	case BRIGHTNESS_H1:
		*vals = isp_brightness_h1_qtech;
		return ARRAY_SIZE(isp_brightness_h1_qtech);
	case BRIGHTNESS_H2:
		*vals = isp_brightness_h2_qtech;
		return ARRAY_SIZE(isp_brightness_h2_qtech);
	case BRIGHTNESS_H3:
		*vals = isp_brightness_h3_qtech;
		return ARRAY_SIZE(isp_brightness_h3_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_get_sharpness_qtech(void **vals, int val)
{
	switch (val) {
	case SHARPNESS_L2:
		*vals = isp_sharpness_l2_qtech;
		return ARRAY_SIZE(isp_sharpness_l2_qtech);
	case SHARPNESS_L1:
		*vals = isp_sharpness_l1_qtech;
		return ARRAY_SIZE(isp_sharpness_l1_qtech);
	case SHARPNESS_H0:
		*vals = isp_sharpness_h0_qtech;
		return ARRAY_SIZE(isp_sharpness_h0_qtech);
	case SHARPNESS_H1:
		*vals = isp_sharpness_h1_qtech;
		return ARRAY_SIZE(isp_sharpness_h1_qtech);
	case SHARPNESS_H2:
		*vals = isp_sharpness_h2_qtech;
		return ARRAY_SIZE(isp_sharpness_h2_qtech);
	default:
		return -EINVAL;
	}

	return 0;
}


static struct isp_effect_ops sensor_effect_ops_qtech = {
	.get_exposure = sensor_get_exposure_qtech,
	.get_iso = sensor_get_iso_qtech,
	.get_contrast = sensor_get_contrast_qtech,
	.get_saturation = sensor_get_saturation_qtech,
	.get_saturation_hdr = sensor_get_saturation_hdr_qtech,
	.get_white_balance = sensor_get_white_balance_qtech,
	.get_brightness = sensor_get_brightness_qtech,
	.get_sharpness = sensor_get_sharpness_qtech,
	.get_vcm_range = sensor_get_vcm_range_qtech,
	.get_aecgc_win_setting = sensor_get_aecgc_win_setting,
};


static int ov8865_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov8865_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;
	//fill sensor vts register address here
//	info->vts_address1 = 0x380e;
//	info->vts_address2 = 0x380f;
//	info->frame_rate = FRAME_RATE_DEFAULT;
	//this var is set according to sensor setting,can only be set 1,2 or 4
	//1 means no binning,2 means 2x2 binning,4 means 4x4 binning
//	info->binning = 2;

	ret = ov8865_write_array(sd, ov8865_init_regs);
	if (ret < 0)
		return ret;
	// return value:
	// bit[7]: 0 no otp info, 1 valid otp info
	// bit[6]: 0 no otp wb, 1 valib otp wb
	// bit[5]: 0 no otp vcm, 1 valid otp vcm
	// bit[4]: 0 no otp lenc, 1 valid otp lenc
	ret = apply_otp(sd);
	if((ret&0x10) == 0)
		printk(KERN_DEBUG"no otp lenc\n");
	if((ret&0x20) == 0)
		printk(KERN_DEBUG"no otp vcm\n");
	if((ret&0x40) == 0)
		printk(KERN_DEBUG"no otp wb\n");
	if((ret&0x80) == 0)
		printk(KERN_DEBUG"no otp info\n");

	return ret;


}

static int ov8865_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov8865_read(sd, 0x300b, &v);
	printk(KERN_DEBUG"CHIP_ID_H=0x%x ", v);
	ret = ov8865_read(sd, 0x300a, &v);
	printk(KERN_DEBUG"CHIP_ID_L=0x%x ", v);
	ret = ov8865_read(sd, 0x300c, &v);
	printk(KERN_DEBUG"CHIP_ID_M=0x%x \n", v);
	if (ret < 0)
		return ret;
	if (v != OV8865_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct ov8865_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} ov8865_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= NULL,
	},
};
#define N_OV8865_FMTS ARRAY_SIZE(ov8865_formats)

struct ov8865_win_size {
	int	width;
	int	height;
	int	vts;
	int	framerate;
	int	framerate_div;
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
	struct isp_regb_vals *regs_aecgc_win_matrix;
	struct isp_regb_vals *regs_aecgc_win_center;
	struct isp_regb_vals *regs_aecgc_win_central_weight;
};
static struct ov8865_win_size ov8865_win_sizes_oflim[] = {
#ifndef BUILD_MASS_PRODUCTION
	//ov8865_win_hd
	{
		.width		= HD_WIDTH,
		.height		= HD_HEIGHT,
		.vts		= 0x660,
		.framerate	= 32,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,//0x100,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x61,
		.vts_gain_ctrl_3  = 0x61,
		.gain_ctrl_1	  = 0x30,
		.gain_ctrl_2	  = 0xf0,
		.gain_ctrl_3	  = 0xff,
		.regs 			  = ov8865_win_hd,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_hd_matrix_oflim,
		.regs_aecgc_win_center    		= isp_aecgc_win_hd_center_oflim,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_hd_central_weight_oflim,
	},
#endif
	/* 3264*2448 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x9e8,//0x7b6
		.framerate	= 305,//10,
		.framerate_div	  = 10,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,//0x100,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x61,
		.vts_gain_ctrl_3  = 0x81,
		.gain_ctrl_1	  = 0x30,
		.gain_ctrl_2	  = 0x50,
		.gain_ctrl_3	  = 0xff,
		.spot_meter_win_width  = 800,
		.spot_meter_win_height = 600,
		.regs 			  = ov8865_win_8m,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_8M_matrix_oflim,
		.regs_aecgc_win_center    		= isp_aecgc_win_8M_center_oflim,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_8M_central_weight_oflim,
	},
};
static struct ov8865_win_size ov8865_win_sizes_sunny[] = {
#ifndef BUILD_MASS_PRODUCTION
	//ov8865_win_hd
	{
		.width		= HD_WIDTH,
		.height		= HD_HEIGHT,
		.vts		= 0x660,
		.framerate	= 32,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,//0x100,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x61,
		.vts_gain_ctrl_3  = 0x61,
		.gain_ctrl_1	  = 0x30,
		.gain_ctrl_2	  = 0xf0,
		.gain_ctrl_3	  = 0xff,
		.regs 			  = ov8865_win_hd,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_hd_matrix_sunny,
		.regs_aecgc_win_center    		= isp_aecgc_win_hd_center_sunny,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_hd_central_weight_sunny,
	},
#endif
	/* 3264*2448 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x9e8,//0x7b6
		.framerate	= 305,//10,
		.framerate_div 	  = 10,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,//0x100,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x61,
		.vts_gain_ctrl_3  = 0x81,
		.gain_ctrl_1	  = 0x30,
		.gain_ctrl_2	  = 0x50,
		.gain_ctrl_3	  = 0xff,
		.spot_meter_win_width  = 800,
		.spot_meter_win_height = 600,
		.regs 			  = ov8865_win_8m,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_8M_matrix_sunny,
		.regs_aecgc_win_center    		= isp_aecgc_win_8M_center_sunny,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_8M_central_weight_sunny,
	},
};
static struct ov8865_win_size ov8865_win_sizes_qtech[] = {
#ifndef BUILD_MASS_PRODUCTION
	//ov8865_win_hd
	{
		.width		= HD_WIDTH,
		.height		= HD_HEIGHT,
		.vts		= 0x660,
		.framerate	= 32,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,//0x100,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x61,
		.vts_gain_ctrl_3  = 0x61,
		.gain_ctrl_1	  = 0x30,
		.gain_ctrl_2	  = 0xf0,
		.gain_ctrl_3	  = 0xff,
		.regs 			  = ov8865_win_hd,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_hd_matrix_qtech,
		.regs_aecgc_win_center    		= isp_aecgc_win_hd_center_qtech,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_hd_central_weight_qtech,
	},
#endif
	/* 3264*2448 */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.vts		= 0x9e8,//0x7b6
		.framerate	= 305,//10,
		.framerate_div 	  = 10,
		.max_gain_dyn_frm = 0xff,
		.min_gain_dyn_frm = 0x10,
		.max_gain_fix_frm = 0xff,//0x100,
		.min_gain_fix_frm = 0x10,
		.vts_gain_ctrl_1  = 0x41,
		.vts_gain_ctrl_2  = 0x61,
		.vts_gain_ctrl_3  = 0x81,
		.gain_ctrl_1	  = 0x30,
		.gain_ctrl_2	  = 0x50,
		.gain_ctrl_3	  = 0xff,
		.spot_meter_win_width  = 800,
		.spot_meter_win_height = 600,
		.regs 			  = ov8865_win_8m,
		.regs_aecgc_win_matrix	  		= isp_aecgc_win_8M_matrix_qtech,
		.regs_aecgc_win_center    		= isp_aecgc_win_8M_center_qtech,
		.regs_aecgc_win_central_weight  = isp_aecgc_win_8M_central_weight_qtech,
	},
};

//#define N_WIN_SIZES (ARRAY_SIZE(ov8865_win_sizes))

static int ov8865_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV8865_FMTS)
		return -EINVAL;

	*code = ov8865_formats[index].mbus_code;
	return 0;
}

static int ov8865_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov8865_format_struct **ret_fmt,
		struct ov8865_win_size **ret_wsize)
{
	int index;
	struct ov8865_win_size *wsize;

	for (index = 0; index < N_OV8865_FMTS; index++)
		if (ov8865_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV8865_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov8865_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov8865_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = ov8865_win_sizes; wsize < ov8865_win_sizes + n_win_sizes;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= ov8865_win_sizes + n_win_sizes)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = ov8865_formats[index].colorspace;
	return 0;
}

static int ov8865_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return ov8865_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int ov8865_fill_i2c_regs(struct regval_list *sensor_regs, struct v4l2_fmt_data *data)
{
	while (sensor_regs->reg_num != OV8865_REG_END) {
		data->reg[data->reg_num].addr = sensor_regs->reg_num;
		data->reg[data->reg_num].data = sensor_regs->value;
		data->reg_num++;
		sensor_regs++;
	}

	return 0;
}
static int ov8865_set_exp_ratio(struct v4l2_subdev *sd, struct ov8865_win_size *win_size,
								struct v4l2_fmt_data *data)
{
	int i = 0;
	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);

	if(info->win == NULL || info->win == win_size)
		data->exp_ratio = 0x100;
	else {
		for(i = 0; i < N_EXP_RATIO_TABLE_SIZE; i++) {
			if((info->win->width == ov8865_exp_ratio_table[i].width_last)
				&& (info->win->height == ov8865_exp_ratio_table[i].height_last)
				&& (win_size->width  == ov8865_exp_ratio_table[i].width_cur)
				&& (win_size->height == ov8865_exp_ratio_table[i].height_cur)) {
				data->exp_ratio = ov8865_exp_ratio_table[i].exp_ratio;
				break;
			}
		}
		if(i >= N_EXP_RATIO_TABLE_SIZE) {
			data->exp_ratio = 0x100;
			printk(KERN_DEBUG"ov8865:exp_ratio is not found\n!");
		}
	}
	return 0;
}

static int ov8865_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct ov8865_format_struct *fmt_s = NULL;
	struct ov8865_win_size *wsize = NULL;
	int ret = 0;

	ret = ov8865_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = ov8865_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize)&& wsize->regs) {
		memset(data, 0, sizeof(*data));
		data->flags = V4L2_I2C_ADDR_16BIT;
		data->slave_addr = client->addr;
		data->vts = wsize->vts;
		data->mipi_clk = 754; //282; //TBD/* Mbps. */
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
		data->framerate = wsize->framerate;
		data->framerate_div = wsize->framerate_div;
		ov8865_fill_i2c_regs(wsize->regs, data);
		ov8865_set_exp_ratio(sd, wsize, data);
	}

	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int ov8865_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
		ret = ov8865_write_array(sd, ov8865_stream_on);
	else
		ret = ov8865_write_array(sd, ov8865_stream_off);

	return ret;
}

static int ov8865_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov8865_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int ov8865_esd_reset(struct v4l2_subdev *sd)
{
	ov8865_init(sd, 0);
	ov8865_s_stream(sd, 1);

	return 0;
}

static int ov8865_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov8865_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov8865_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov8865_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov8865_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov8865_frame_rates[interval->index];
	return 0;
}

static int ov8865_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	for (i = 0; i < n_win_sizes; i++) {
		struct ov8865_win_size *win = &ov8865_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int ov8865_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}



static int ov8865_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_isp_setting isp_setting;
	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_ISP_SETTING:
		isp_setting.setting = ov8865_isp_setting;
		isp_setting.size = info->isp_setting_size;
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)ov8865_isp_parm;
		break;
	case V4L2_CID_GET_EFFECT_FUNC:
		ctrl->value = (int)sensor_effect_ops;
		break;
		ret = -EINVAL;
		break;
	}
	return ret;
}
#if 0
static int ov8865_set_framerate(struct v4l2_subdev *sd, int framerate)
	{
		struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);
		printk(KERN_DEBUG"ov8865_set_framerate %d\n", framerate);
		if (framerate < FRAME_RATE_AUTO)
			framerate = FRAME_RATE_AUTO;
		else if (framerate > 30)
			framerate = 30;
	//	info->frame_rate = framerate;
		return 0;
	}

#endif

static int ov8865_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
//	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);
	switch (ctrl->id) {
	case V4L2_CID_FRAME_RATE:
		//ret = ov8865_set_framerate(sd, ctrl->value);
		;
		break;
	case V4L2_CID_SENSOR_ESD_RESET:
		ov8865_esd_reset(sd);
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

	for(i = 0; i < n_win_sizes; i++) {
		if (width == ov8865_win_sizes[i].width && height == ov8865_win_sizes[i].height) {
			switch (meter_mode){
				case METER_MODE_MATRIX:
					*vals = ov8865_win_sizes[i].regs_aecgc_win_matrix;
					break;
				case METER_MODE_CENTER:
					*vals = ov8865_win_sizes[i].regs_aecgc_win_center;
					break;
				case METER_MODE_CENTRAL_WEIGHT:
					*vals = ov8865_win_sizes[i].regs_aecgc_win_central_weight;
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

static int ov8865_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV8865, 0);
}

static int ov8865_get_module_factory(struct v4l2_subdev *sd, char **module_factory)
{
	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);

	if (!module_factory)
		return -1;

	if (info->module_id == MODULE_ID_OFLIM)
		*module_factory = factory_ofilm;
	else if (info->module_id == MODULE_ID_SUNNY)
		*module_factory = factory_sunny;
	else if (info->module_id == MODULE_ID_QTECH)
		*module_factory = factory_qtech;
	else
		*module_factory = factory_unknown;

	return 0;
}

static long ov8865_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	char **module_factory = NULL;

	switch (cmd) {
		case SUBDEVIOC_MODULE_FACTORY:
			module_factory = (char**)arg;
			ret = ov8865_get_module_factory(sd, module_factory);
			break;
		default:
			ret = -ENOIOCTLCMD;
	}
	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov8865_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov8865_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov8865_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov8865_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops ov8865_core_ops = {
	.g_chip_ident = ov8865_g_chip_ident,
	.g_ctrl = ov8865_g_ctrl,
	.s_ctrl = ov8865_s_ctrl,
	.queryctrl = ov8865_queryctrl,
	.reset = ov8865_reset,
	.init = ov8865_init,
	.ioctl = ov8865_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov8865_g_register,
	.s_register = ov8865_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov8865_video_ops = {
	.enum_mbus_fmt = ov8865_enum_mbus_fmt,
	.try_mbus_fmt = ov8865_try_mbus_fmt,
	.s_mbus_fmt = ov8865_s_mbus_fmt,
	.s_stream = ov8865_s_stream,
	.cropcap = ov8865_cropcap,
	.g_crop	= ov8865_g_crop,
	.s_parm = ov8865_s_parm,
	.g_parm = ov8865_g_parm,
	.enum_frameintervals = ov8865_enum_frameintervals,
	.enum_framesizes = ov8865_enum_framesizes,
};

static const struct v4l2_subdev_ops ov8865_ops = {
	.core = &ov8865_core_ops,
	.video = &ov8865_video_ops,
};

static ssize_t ov8865_rg_ratio_typical_oflim_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", rg_ratio_typical_oflim);
}

static ssize_t ov8865_rg_ratio_typical_oflim_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical_oflim = (unsigned int)value;

	return size;
}

static ssize_t ov8865_bg_ratio_typical_oflim_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bg_ratio_typical_oflim);
}

static ssize_t ov8865_bg_ratio_typical_oflim_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical_oflim = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov8865_rg_ratio_typical_oflim, 0664, ov8865_rg_ratio_typical_oflim_show, ov8865_rg_ratio_typical_oflim_store);
static DEVICE_ATTR(ov8865_bg_ratio_typical_oflim, 0664, ov8865_bg_ratio_typical_oflim_show, ov8865_bg_ratio_typical_oflim_store);

static ssize_t ov8865_rg_ratio_typical_sunny_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", rg_ratio_typical_sunny);
}

static ssize_t ov8865_rg_ratio_typical_sunny_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical_sunny = (unsigned int)value;

	return size;
}

static ssize_t ov8865_bg_ratio_typical_sunny_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bg_ratio_typical_sunny);
}

static ssize_t ov8865_bg_ratio_typical_sunny_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical_sunny = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov8865_rg_ratio_typical_sunny, 0664, ov8865_rg_ratio_typical_sunny_show, ov8865_rg_ratio_typical_sunny_store);
static DEVICE_ATTR(ov8865_bg_ratio_typical_sunny, 0664, ov8865_bg_ratio_typical_sunny_show, ov8865_bg_ratio_typical_sunny_store);

static ssize_t ov8865_rg_ratio_typical_qtech_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", rg_ratio_typical_qtech);
}

static ssize_t ov8865_rg_ratio_typical_qtech_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical_qtech = (unsigned int)value;

	return size;
}

static ssize_t ov8865_bg_ratio_typical_qtech_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bg_ratio_typical_qtech);
}

static ssize_t ov8865_bg_ratio_typical_qtech_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical_qtech = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov8865_rg_ratio_typical_qtech, 0664, ov8865_rg_ratio_typical_qtech_show, ov8865_rg_ratio_typical_qtech_store);
static DEVICE_ATTR(ov8865_bg_ratio_typical_qtech, 0664, ov8865_bg_ratio_typical_qtech_show, ov8865_bg_ratio_typical_qtech_store);


static int ov8865_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov8865_info *info;
	struct comip_camera_subdev_platform_data *ov8865_setting = NULL;
	int ret;

	info = kzalloc(sizeof(struct ov8865_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov8865_ops);

	/* Make sure it's an ov8865 */
	ret = ov8865_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov8865 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov8865 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ov8865_init(sd, 0);
	// return value:
	// bit[7]: 0 no otp info, 1 valid otp info
	// bit[6]: 0 no otp wb, 1 valib otp wb
	// bit[5]: 0 no otp vcm, 1 valid otp vcm
	// bit[4]: 0 no otp lenc, 1 valid otp lenc
	ret = apply_otp(sd);
	if((ret&0x10) == 0)
		printk(KERN_DEBUG"no otp lenc\n");
	if((ret&0x20) == 0)
		printk(KERN_DEBUG"no otp vcm\n");
	if((ret&0x40) == 0)
		printk(KERN_DEBUG"no otp wb\n");
	if((ret&0x80) == 0)
		printk(KERN_DEBUG"no otp info\n");

	if(info->module_id == MODULE_ID_OFLIM){
		printk(KERN_DEBUG"ov8865 modue id is oflim!\n");
		ov8865_win_sizes = ov8865_win_sizes_oflim;
		ov8865_isp_parm = &ov8865_isp_parm_oflim;
		ov8865_isp_setting = ov8865_isp_setting_oflim;
		sensor_effect_ops = &sensor_effect_ops_oflim;
		info->n_win_sizes = ARRAY_SIZE(ov8865_win_sizes_oflim);
		info->isp_setting_size = ARRAY_SIZE(ov8865_isp_setting_oflim);
	}
	else if(info->module_id == MODULE_ID_SUNNY) {
		printk(KERN_DEBUG"ov8865 modue id is sunny!\n");
		ov8865_win_sizes = ov8865_win_sizes_sunny;
		ov8865_isp_parm = &ov8865_isp_parm_sunny;
		ov8865_isp_setting = ov8865_isp_setting_sunny;
		sensor_effect_ops = &sensor_effect_ops_sunny;
		info->n_win_sizes = ARRAY_SIZE(ov8865_win_sizes_sunny);
		info->isp_setting_size = ARRAY_SIZE(ov8865_isp_setting_sunny);
	}
	else if(info->module_id == MODULE_ID_QTECH) {
		printk(KERN_DEBUG"ov8865 modue id is qtech!\n");
		ov8865_win_sizes = ov8865_win_sizes_qtech;
		ov8865_isp_parm = &ov8865_isp_parm_qtech;
		ov8865_isp_setting = ov8865_isp_setting_qtech;
		sensor_effect_ops = &sensor_effect_ops_qtech;
		info->n_win_sizes = ARRAY_SIZE(ov8865_win_sizes_qtech);
		info->isp_setting_size = ARRAY_SIZE(ov8865_isp_setting_qtech);
	}else{
		printk(KERN_DEBUG"ov8865 modue id can't be recognized! module id = 0x%x\n",info->module_id);
		ov8865_win_sizes = ov8865_win_sizes_oflim;
		ov8865_isp_parm = &ov8865_isp_parm_oflim;
		ov8865_isp_setting = ov8865_isp_setting_oflim;
		sensor_effect_ops = &sensor_effect_ops_oflim;
		info->n_win_sizes = ARRAY_SIZE(ov8865_win_sizes_oflim);
		info->isp_setting_size = ARRAY_SIZE(ov8865_isp_setting_oflim);
	}

	n_win_sizes = info->n_win_sizes;

	ov8865_setting = (struct comip_camera_subdev_platform_data*)client->dev.platform_data;
	if (ov8865_setting) {
		if (ov8865_setting->flags & CAMERA_SUBDEV_FLAG_MIRROR)
			ov8865_isp_parm->mirror = 1;
		else ov8865_isp_parm->mirror = 0;

		if (ov8865_setting->flags & CAMERA_SUBDEV_FLAG_FLIP)
			ov8865_isp_parm->flip = 1;
		else ov8865_isp_parm->flip = 0;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_oflim);
	if(ret){
		v4l_err(client, "create dev_attr_ov8865_rg_ratio_typical_oflim failed!\n");
		goto err_create_dev_attr_ov8865_rg_ratio_typical_oflim;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_oflim);
	if(ret){
		v4l_err(client, "create dev_attr_ov8865_bg_ratio_typical_oflim failed!\n");
		goto err_create_dev_attr_ov8865_bg_ratio_typical_oflim;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_sunny);
	if(ret){
		v4l_err(client, "create dev_attr_ov8865_rg_ratio_typical_sunny failed!\n");
		goto err_create_dev_attr_ov8865_rg_ratio_typical_sunny;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_sunny);
	if(ret){
		v4l_err(client, "create dev_attr_ov8865_bg_ratio_typical_sunny failed!\n");
		goto err_create_dev_attr_ov8865_bg_ratio_typical_sunny;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_qtech);
	if(ret){
		v4l_err(client, "create dev_attr_ov8865_rg_ratio_typical_qtech failed!\n");
		goto err_create_dev_attr_ov8865_rg_ratio_typical_qtech;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_qtech);
	if(ret){
		v4l_err(client, "create dev_attr_ov8865_bg_ratio_typical_qtech failed!\n");
		goto err_create_dev_attr_ov8865_bg_ratio_typical_qtech;
	}

	return 0;

err_create_dev_attr_ov8865_bg_ratio_typical_oflim:
	device_remove_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_oflim);
err_create_dev_attr_ov8865_rg_ratio_typical_oflim:
	device_remove_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_oflim);
err_create_dev_attr_ov8865_bg_ratio_typical_qtech:
	device_remove_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_qtech);
err_create_dev_attr_ov8865_rg_ratio_typical_qtech:
	device_remove_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_qtech);
err_create_dev_attr_ov8865_bg_ratio_typical_sunny:
	device_remove_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_sunny);
err_create_dev_attr_ov8865_rg_ratio_typical_sunny:
	kfree(info);
	return ret;
}

static int ov8865_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8865_info *info = container_of(sd, struct ov8865_info, sd);

	device_remove_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_oflim);
	device_remove_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_oflim);
	device_remove_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_sunny);
	device_remove_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_sunny);
	device_remove_file(&client->dev, &dev_attr_ov8865_rg_ratio_typical_qtech);
	device_remove_file(&client->dev, &dev_attr_ov8865_bg_ratio_typical_qtech);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov8865_id[] = {
	{ "ov8865-mipi-raw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov8865_id);

static struct i2c_driver ov8865_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov8865-mipi-raw",
	},
	.probe		= ov8865_probe,
	.remove		= ov8865_remove,
	.id_table	= ov8865_id,
};

static __init int init_ov8865(void)
{
	return i2c_add_driver(&ov8865_driver);
}

static __exit void exit_ov8865(void)
{
	i2c_del_driver(&ov8865_driver);
}

fs_initcall(init_ov8865);
module_exit(exit_ov8865);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov8865 sensors");
MODULE_LICENSE("GPL");
