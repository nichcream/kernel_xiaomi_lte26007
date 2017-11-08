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
#include "hm2056.h"

#define HM2056_CHIP_ID_H	(0x20)
#define HM2056_CHIP_ID_L	(0x56)

#define VGA_WIDTH		640
#define VGA_HEIGHT		480
#define UXGA_WIDTH		1600
#define UXGA_HEIGHT		1200
#define MAX_WIDTH		1600
#define MAX_HEIGHT		1200

#define MAX_PREVIEW_WIDTH	1600//VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  1200//VGA_HEIGHT

#define HM2056_REG_END		0xfffe
#define HM2056_REG_DELAY	0xfffd



struct hm2056_format_struct;

struct hm2056_info {
	struct v4l2_subdev sd;
	struct hm2056_format_struct *fmt;
	struct hm2056_win_size *win;
	unsigned short vts_address1;
	unsigned short vts_address2;
	int brightness;
	int contrast;
	int saturation;
	int effect;
	int wb;
	int exposure;
	int previewmode;
	int scene_mode;
	int sharpness;
	int frame_rate;
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


enum preview_mode {
	preview_auto = 0,
	normal = 16,
	dynamic = 1,
	portrait = 2,
	landscape = 3,
	nightmode = 4,
	nightmode_portrait = 5,
	theatre = 6,
	beatch = 7,
	snow = 8,
	sunset = 9,
	anti_shake = 10,
	fireworks = 11,
	sports = 12,
	party = 13,
	candy = 14,
	bar = 15,
};


static struct regval_list hm2056_init_regs[] = {
	{0x0022,0x00},
	{0x0020,0x00},
	{0x0025,0x00},
	{0x0026,0x88},
	{0x0027,0x30},
	//{0x0028,0x81},
	{0x002A,0x2F},
	//{0x002b,0x04},
	{0x002C,0x0A},
	{0x0004,0x10},
	{0x0006,0x01},
	{0x000D,0x11},
	{0x000E,0x11},
	{0x000F,0x00},
	{0x0011,0x02},
	{0x0012,0x1C},
	{0x0013,0x01},
	{0x0015,0x02},
	{0x0016,0x80},
	{0x0018,0x00},
	{0x001D,0x40},
	{0x0040,0x20},
	{0x0053,0x0A},
	{0x0044,0x06},
	{0x0046,0xD8},
	{0x004A,0x0A},
	{0x004B,0x72},
	{0x0075,0x01},
	{0x0070,0x5F},
	{0x0071,0xFF},
	{0x0072,0x55},
	{0x0073,0x50},
	{0x0077,0x04},
	{0x0080,0xC8},
	{0x0082,0xA2},
	{0x0083,0xF0},
	{0x0085,0x11},
	{0x0086,0x02},
	{0x0087,0x80},
	{0x0088,0x6C},
	{0x0089,0x2E},
	{0x008A,0x7D},
	{0x008D,0x20},
	{0x0090,0x00},
	{0x0091,0x10},
	{0x0092,0x11},
	{0x0093,0x12},
	{0x0094,0x16},
	{0x0095,0x08},
	{0x0096,0x00},
	{0x0097,0x10},
	{0x0098,0x11},
	{0x0099,0x12},
	{0x009A,0x16},
	{0x009B,0x34},
	{0x00A0,0x00},
	{0x00A1,0x04},
	{0x011F,0xF7},
	{0x0120,0x37},
	{0x0121,0x83},
	{0x0122,0x3B},
	{0x0123,0xC2},
	{0x0124,0xCE},
	{0x0125,0x7F},
	{0x0126,0x70},
	{0x0128,0x1F},
	{0x0132,0x10},
	{0x0136,0x0A},
	{0x0131,0xBD},
	{0x0140,0x14},
	{0x0141,0x0A},
	{0x0142,0x14},
	{0x0143,0x0A},
	{0x0144,0x06},
	{0x0145,0x00},
	{0x0146,0x20},
	{0x0147,0x0A},
	{0x0148,0x10},
	{0x0149,0x0C},
	{0x014A,0x80},
	{0x014B,0x80},
	{0x014C,0x2E},
	{0x014D,0x2E},
	{0x014E,0x05},
	{0x014F,0x05},
	{0x0150,0x0D},
	{0x0155,0x00},
	{0x0156,0x10},
	{0x0157,0x0A},
	{0x0158,0x0A},
	{0x0159,0x0A},
	{0x015A,0x05},
	{0x015B,0x05},
	{0x015C,0x05},
	{0x015D,0x05},
	{0x015E,0x08},
	{0x015F,0xFF},
	{0x0160,0x50},
	{0x0161,0x20},
	{0x0162,0x14},
	{0x0163,0x0A},
	{0x0164,0x10},
	{0x0165,0x08},
	{0x0166,0x0A},
	{0x018C,0x24},
	{0x018D,0x04},
	{0x018E,0x00},
	{0x018F,0x11},
	{0x0190,0x80},
	{0x0191,0x47},
	{0x0192,0x48},
	{0x0193,0x64},
	{0x0194,0x32},
	{0x0195,0xc8},
	{0x0196,0x96},
	{0x0197,0x64},
	{0x0198,0x32},
	{0x0199,0x14},
	{0x019A,0x20},
	{0x019B,0x14},

	{0x01BA,0x10},
	{0x01BB,0x04},
	{0x01D8,0x40},
	{0x01DE,0x60},
	{0x01E4,0x10},
	{0x01E5,0x10},
	{0x01F2,0x0C},
	{0x01F3,0x14},
	{0x01F8,0x04},
	{0x01F9,0x0C},
	{0x01FE,0x02},
	{0x01FF,0x04},
	{0x0220,0x00},
	{0x0221,0xB0},
	{0x0222,0x00},
	{0x0223,0x80},
	{0x0224,0x8E},
	{0x0225,0x00},
	{0x0226,0x88},
	{0x022A,0x88},
	{0x022B,0x00},
	{0x022C,0x88},
	{0x022D,0x13},
	{0x022E,0x0B},
	{0x022F,0x13},
	{0x0230,0x0B},
	{0x0233,0x13},
	{0x0234,0x0B},
	{0x0235,0x28},
	{0x0236,0x03},
	{0x0237,0x28},
	{0x0238,0x03},
	{0x023B,0x28},
	{0x023C,0x03},
	{0x023D,0x5C},
	{0x023E,0x02},
	{0x023F,0x5C},
	{0x0240,0x02},
	{0x0243,0x5C},
	{0x0244,0x02},
	{0x0251,0x0E},
	{0x0252,0x00},

	{0x0280,0x0A},
	{0x0282,0x14},
	{0x0284,0x2A},
	{0x0286,0x50},
	{0x0288,0x60},
	{0x028A,0x6D},
	{0x028C,0x79},
	{0x028E,0x82},
	{0x0290,0x8A},
	{0x0292,0x91},
	{0x0294,0x9C},
	{0x0296,0xA7},
	{0x0298,0xBA},
	{0x029A,0xCD},
	{0x029C,0xE0},
	{0x029E,0x2D},

	{0x02A0,0x06},
	{0x02E0,0x04},
	{0x02C0,0xB1},
	{0x02C1,0x01},
	{0x02C2,0x7D},
	{0x02C3,0x07},
	{0x02C4,0xD2},
	{0x02C5,0x07},
	{0x02C6,0xC4},
	{0x02C7,0x07},
	{0x02C8,0x79},
	{0x02C9,0x01},
	{0x02CA,0xC4},
	{0x02CB,0x07},
	{0x02CC,0xF7},
	{0x02CD,0x07},
	{0x02CE,0x3B},
	{0x02CF,0x07},
	{0x02D0,0xCF},
	{0x02D1,0x01},
	{0x0302,0x00},
	{0x0303,0x00},
	{0x0304,0x00},
	{0x02F0,0x5E},
	{0x02F1,0x07},
	{0x02F2,0xA0},
	{0x02F3,0x00},
	{0x02F4,0x02},
	{0x02F5,0x00},
	{0x02F6,0xC4},
	{0x02F7,0x07},
	{0x02F8,0x11},
	{0x02F9,0x00},
	{0x02FA,0x2A},
	{0x02FB,0x00},
	{0x02FC,0xA1},
	{0x02FD,0x07},
	{0x02FE,0xB8},
	{0x02FF,0x07},
	{0x0300,0xA7},
	{0x0301,0x00},
	{0x0305,0x00},
	{0x0306,0x00},
	{0x0307,0x7A},

	{0x032D,0x00},
	{0x032E,0x01},
	{0x032F,0x00},
	{0x0330,0x01},
	{0x0331,0x00},
	{0x0332,0x01},
	{0x0333,0x82},
	{0x0334,0x00},
	{0x0335,0x84},
	{0x0336,0x00},
	{0x0337,0x01},
	{0x0338,0x00},
	{0x0339,0x01},
	{0x033A,0x00},
	{0x033B,0x01},
	{0x0340,0x30},
	{0x0341,0x44},
	{0x0342,0x4A},
	{0x0343,0x42},
	{0x0344,0x74},
	{0x0345,0x4F},
	{0x0346,0x67},
	{0x0347,0x5C},
	{0x0348,0x59},
	{0x0349,0x67},
	{0x034A,0x4D},
	{0x034B,0x6E},
	{0x034C,0x44},
	{0x0350,0x80},
	{0x0351,0x80},
	{0x0352,0x18},
	{0x0353,0x18},
	{0x0354,0x6E},
	{0x0355,0x4A},
	{0x0356,0x7A},
	{0x0357,0xC6},
	{0x0358,0x06},
	{0x035A,0x06},
	{0x035B,0xA0},
	{0x035C,0x73},
	{0x035D,0x5A},
	{0x035E,0xC6},
	{0x035F,0xA0},
	{0x0360,0x02},
	{0x0361,0x18},
	{0x0362,0x80},
	{0x0363,0x6C},
	{0x0364,0x00},
	{0x0365,0xF0},
	{0x0366,0x20},
	{0x0367,0x0C},
	{0x0369,0x00},
	{0x036A,0x10},
	{0x036B,0x10},
	{0x036E,0x20},
	{0x036F,0x00},
	{0x0370,0x10},
	{0x0371,0x18},
	{0x0372,0x0C},
	{0x0373,0x38},
	{0x0374,0x3A},
	{0x0375,0x13},
	{0x0376,0x22},

	{0x0380,0xFF},
	{0x0381,0x4A},
	{0x0382,0x36},
	{0x038A,0x40},
	{0x038B,0x08},
	{0x038C,0xC1},
	{0x038E,0x40},
	{0x038F,0x09},
	{0x0390,0xD0},
	{0x0391,0x05},
	{0x0393,0x80},
	{0x0395,0x21},

	{0x0398,0x02},
	{0x0399,0x74},
	{0x039A,0x03},
	{0x039B,0x11},
	{0x039C,0x03},
	{0x039D,0xAE},
	{0x039E,0x04},
	{0x039F,0xE8},
	{0x03A0,0x06},
	{0x03A1,0x22},
	{0x03A2,0x07},
	{0x03A3,0x5C},
	{0x03A4,0x09},
	{0x03A5,0xD0},
	{0x03A6,0x0C},
	{0x03A7,0x0E},
	{0x03A8,0x10},
	{0x03A9,0x18},
	{0x03AA,0x20},
	{0x03AB,0x28},
	{0x03AC,0x1E},
	{0x03AD,0x1A},
	{0x03AE,0x13},
	{0x03AF,0x0C},
	{0x03B0,0x0B},
	{0x03B1,0x09},

	{0x03B3,0x10},
	{0x03B4,0x00},
	{0x03B5,0x10},
	{0x03B6,0x00},
	{0x03B7,0xEA},
	{0x03B8,0x00},
	{0x03B9,0x3A},
	{0x03BA,0x01},
	{0x03BB,0x9F},
	{0x03BC,0xCF},
	{0x03BD,0xE7},
	{0x03BE,0xF3},
	{0x03BF,0x01},
	{0x03D0,0xF8},
	{0x03E0,0x04},
	{0x03E1,0x01},
	{0x03E2,0x04},
	{0x03E4,0x10},
	{0x03E5,0x12},
	{0x03E6,0x00},
	{0x03E8,0x21},
	{0x03E9,0x23},
	{0x03EA,0x01},
	{0x03EC,0x21},
	{0x03ED,0x23},
	{0x03EE,0x01},
	{0x03F0,0x20},
	{0x03F1,0x22},
	{0x03F2,0x00},

	{0x0420,0x84},
	{0x0421,0x00},
	{0x0422,0x00},
	{0x0423,0x83},
	{0x0430,0x08},
	{0x0431,0x28},
	{0x0432,0x10},
	{0x0433,0x08},
	{0x0435,0x0C},

	{0x0450,0xFF},
	{0x0451,0xE8},
	{0x0452,0xC4},
	{0x0453,0x88},
	{0x0454,0x00},
	{0x0458,0x98},
	{0x0459,0x03},
	{0x045A,0x00},
	{0x045B,0x28},
	{0x045C,0x00},
	{0x045D,0x68},
	{0x0466,0x14},
	{0x047A,0x00},
	{0x047B,0x00},
	{0x0480,0x58},
	{0x0481,0x06},
	{0x0482,0x0C},
	{0x04B0,0x50},
	{0x04B6,0x30},
	{0x04B9,0x10},
	{0x04B3,0x10},
	{0x04B1,0x8E},
	{0x04B4,0x20},
	{0x0540,0x00},
	{0x0541,0x9D},
	{0x0542,0x00},
	{0x0543,0xBC},
	{0x0580,0x01},
	{0x0581,0x0F},
	{0x0582,0x04},

	{0x0594,0x00},
	{0x0595,0x04},

	{0x05A9,0x03},
	{0x05AA,0x40},
	{0x05AB,0x80},
	{0x05AC,0x0A},
	{0x05AD,0x10},
	{0x05AE,0x0C},
	{0x05AF,0x0C},
	{0x05B0,0x03},
	{0x05B1,0x03},
	{0x05B2,0x1C},
	{0x05B3,0x02},
	{0x05B4,0x00},
	{0x05B5,0x0C},
	{0x05B8,0x80},
	{0x05B9,0x32},
	{0x05BA,0x00},
	{0x05BB,0x80},
	{0x05BC,0x03},
	{0x05BD,0x00},
	{0x05BF,0x05},
	{0x05C0,0x10},
	{0x05C3,0x00},
	{0x05C4,0x0C},
	{0x05C5,0x20},
	{0x05C7,0x01},
	{0x05C8,0x14},
	{0x05C9,0x54},
	{0x05CA,0x14},
	{0x05CB,0xE0},
	{0x05CC,0x20},
	{0x05CD,0x00},
	{0x05CE,0x08},
	{0x05CF,0x60},
	{0x05D0,0x10},
	{0x05D1,0x05},
	{0x05D2,0x03},
	{0x05D4,0x00},
	{0x05D5,0x05},
	{0x05D6,0x05},
	{0x05D7,0x05},
	{0x05D8,0x08},
	{0x05DC,0x0C},
	{0x05D9,0x00},
	{0x05DB,0x00},
	{0x05DD,0x0F},
	{0x05DE,0x00},
	{0x05DF,0x0A},

	{0x05E0,0xA0},
	{0x05E1,0x00},
	{0x05E2,0xA0},
	{0x05E3,0x00},
	{0x05E4,0x04},
	{0x05E5,0x00},
	{0x05E6,0x83},
	{0x05E7,0x02},
	{0x05E8,0x06},
	{0x05E9,0x00},
	{0x05EA,0xE5},
	{0x05EB,0x01},

	{0x0660,0x04},
	{0x0661,0x16},
	{0x0662,0x04},
	{0x0663,0x28},
	{0x0664,0x04},
	{0x0665,0x18},
	{0x0666,0x04},
	{0x0667,0x21},
	{0x0668,0x04},
	{0x0669,0x0C},
	{0x066A,0x04},
	{0x066B,0x25},
	{0x066C,0x00},
	{0x066D,0x12},
	{0x066E,0x00},
	{0x066F,0x80},
	{0x0670,0x00},
	{0x0671,0x0A},
	{0x0672,0x04},
	{0x0673,0x1D},
	{0x0674,0x04},
	{0x0675,0x1D},
	{0x0676,0x00},
	{0x0677,0x7E},
	{0x0678,0x01},
	{0x0679,0x47},
	{0x067A,0x00},
	{0x067B,0x73},
	{0x067C,0x04},
	{0x067D,0x14},
	{0x067E,0x04},
	{0x067F,0x28},
	{0x0680,0x00},
	{0x0681,0x22},
	{0x0682,0x00},
	{0x0683,0xA5},
	{0x0684,0x00},
	{0x0685,0x1E},
	{0x0686,0x04},
	{0x0687,0x1D},
	{0x0688,0x04},
	{0x0689,0x19},
	{0x068A,0x04},
	{0x068B,0x21},
	{0x068C,0x04},
	{0x068D,0x0A},
	{0x068E,0x04},
	{0x068F,0x25},
	{0x0690,0x04},
	{0x0691,0x15},
	{0x0698,0x20},
	{0x0699,0x20},
	{0x069A,0x01},
	{0x069C,0x22},
	{0x069D,0x10},
	{0x069E,0x10},
	{0x069F,0x08},
	{0x0B20,0xBE},
	{0x007C,0x43},
	{0x0B02,0x04},
	{0x0B07,0x25},
	{0x0B0E,0x1D},
	{0x0B0F,0x07},
	{0x0B22,0x02},
	{0x0B39,0x03},
	{0x0B11,0x7F},
	{0x0B12,0x7F},
	{0x0B17,0xE0},
	{0x0B30,0x0F},
	{0x0B31,0x02},
	{0x0B32,0x00},
	{0x0B33,0x00},
	{0x0B39,0x0F},
	{0x0B3B,0x12},
	{0x0B3F,0x01},
	{0x0024,0x40},
	{0x0000,0x01},
	{0x0100,0x01},
	{0x0101,0x01},
	{0x0005,0x01},
	{HM2056_REG_END, 0xff},	/* END MARKER */
};


//#define N_VGA_REGS (ARRAY_SIZE(hm2056_win_VGA))

static struct regval_list hm2056_win_VGA[] = {
	/*
	{0x0006,0x01},//0x0e
	{0x000D,0x11},
	{0x000E,0x11},
	{0x011F,0x80},
	{0x0125,0xFF},
	{0x0126,0x70},
	{0x0131,0xAD},
	{0x0366,0x08},
	{0x0433,0x10},
	{0x0435,0x14},
	{0x05E0,0xA0},
	{0x05E1,0x00},
	{0x05E2,0xA0},
	{0x05E3,0x00},
	{0x05E4,0x04},
	{0x05E5,0x00},
	{0x05E6,0x83},
	{0x05E7,0x02},
	{0x05E8,0x06},
	{0x05E9,0x00},
	{0x05EA,0xE5},
	{0x05EB,0x01},
	{0x0000,0x01},
	{0x0100,0x01},
	{0x0101,0x01},
	*/
	{0x0005,0x00},
	{0x0006,0x00},//0x0e
	{0x000D,0x11},
	{0x000E,0x11},
	{0x0040,0x20},
	{0x0082,0xA2},
	{0x011F,0xf7},
	{0x0125,0xfF},
	{0x0126,0x70},
	{0x0131,0xBd},
	{0x05E0,0xa0},
	{0x05E1,0x00},
	{0x05E2,0xA0},
	{0x05E3,0x00},
	{0x05E4,0x04},
	{0x05E5,0x00},
	{0x05E6,0x83},
	{0x05E7,0x02},
	{0x05E8,0x06},
	{0x05E9,0x00},
	{0x05EA,0xe5},
	{0x05EB,0x01},	
	{0x0391,0xBc},
	{0x0123,0x21},
	{0x0391,0x05},
	{0x0024,0x00},
	{0x0023,0x00},
	{0x0026,0x87},
	{0x002B,0x00},
	{0x002C,0x0A},
	{0x0025,0x00},
	{0x007c,0x14},
	{0x0b02,0x04},
	{0x0b07,0x25},
	{0x0b0e,0x1d},
	{0x0b0f,0x07},
	{0x0b22,0x02},
	{0x0b39,0x03},
	{0x0b11,0x7f},
	{0x0b12,0x7f},
	{0x0b17,0xe0},
	{0x0b20,0xbe},
	{0x0b30,0x0f},
	{0x0b31,0x02},
	{0x0b32,0x00},
	{0x0b33,0x00},
	{0x0b39,0x0f},
	{0x0b3b,0x12},
	{0x007c,0x43},
	{0x0b3e,0x01},
	{0x0b09,0x01},
	{0x0b08,0xe0},
	{0x0b0b,0x02},
	{0x0b0a,0x80},
	{0x0b3f,0x01},
	{0x0024,0x40},

	{0x0000,0x01},
	{0x0100,0x01},
	{0x0101,0x01},
	{0x0005,0x01},
	{HM2056_REG_END, 0xff},
};



//#define N_UXGA_REGS (ARRAY_SIZE(hm2056_win_UXGA))

static struct regval_list hm2056_win_UXGA[] = {

	/*
	{0x0005,0x00},
	{0x0006,0x01},//0x0e
	{0x000D,0x00},
	{0x000E,0x00},
	{0x011F,0xff},
	{0x0125,0xDF},
	{0x0126,0x70},
	{0x05E4,0x0A},
	{0x05E5,0x00},
	{0x05E6,0x49},
	{0x05E7,0x06},
	{0x05E8,0x0A},
	{0x05E9,0x00},
	{0x05EA,0xB9},
	{0x05EB,0x04},
	{0x0000,0x01},
	{0x0100,0x01},
	{0x0101,0x01},
	{0x0005,0x01},
	*/
	{0x0005,0x00},
	{0x0006,0x00},//0x0e
	{0x000D,0x00},
	{0x000E,0x00},
	{0x0040,0x20},
	{0x0082,0xE2},
	{0x000E,0x00},
	{0x011F,0xff},
	{0x0125,0xDF},
	{0x0126,0x70},
	{0x0131,0xBC},
	{0x0133,0x00},
	{0x0391,0x05},
	{0x0433,0x10},
	{0x0435,0x14},
	{0x0391,0xB4},
	{0x0540,0x00},
	{0x0542,0x00},

	//{0x05E0,0x00},
	//{0x05E1,0x00},
	//{0x05E2,0xA0},
	//{0x05E3,0x00},
	{0x05E4,0x0A},
	{0x05E5,0x00},
	{0x05E6,0x49},
	{0x05E7,0x06},
	{0x05E8,0x0A},
	{0x05E9,0x00},
	{0x05EA,0xB9},
	{0x05EB,0x04},
	{0x0391,0x05},
	{0x0023,0x00},
	{0x0026,0x87},
	{0x002B,0x00},
	{0x002C,0x0A},
	{0x0025,0x00},
	{0x007c,0x14},
	{0x0b02,0x04},
	{0x0b07,0x25},
	{0x0b0e,0x1d},
	{0x0b0f,0x07},
	{0x0b22,0x02},
	{0x0b39,0x03},
	{0x0b11,0x7f},
	{0x0b12,0x7f},
	{0x0b17,0xe0},
	{0x0b20,0xbe},
	{0x0b30,0x0f},
	{0x0b31,0x02},
	{0x0b32,0x00},
	{0x0b33,0x00},
	{0x0b39,0x0f},
	{0x0b3b,0x12},
	{0x007c,0x14},
	{0x0b3e,0x01},
	{0x0b09,0x04},
	{0x0b08,0xb0},
	{0x0b0b,0x06},
	{0x0b0a,0x40},
	{0x0b3f,0x01},
	{0x0024,0x40},

	{0x0000,0x01},
	{0x0100,0x01},
	{0x0101,0x01},
	{0x0005,0x01},

	{HM2056_REG_END, 0xff},
};

static struct regval_list hm2056_sharpness_l2[] = {
	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_sharpness_l1[] = {
	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_sharpness_h0[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_sharpness_h1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_sharpness_h2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_sharpness_h3[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_sharpness_h4[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_sharpness_h5[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_sharpness_h6[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_sharpness_h7[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_sharpness_h8[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_sharpness_max[] = {
	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_brightness_l4[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_brightness_l3[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_brightness_l2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_brightness_l1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_brightness_h0[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_brightness_h1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_brightness_h2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_brightness_h3[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_brightness_h4[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_saturation_l2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_saturation_l1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_saturation_h0[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_saturation_h1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_saturation_h2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_l4[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_l3[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_l2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_l1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_h0[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_h1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_h2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_h3[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_contrast_h4[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_whitebalance_auto[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_whitebalance_incandescent[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_whitebalance_fluorescent[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_whitebalance_daylight[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_whitebalance_cloudy[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_effect_none[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_mono[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_negative[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_solarize[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_sepia[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_posterize[] = {
	//{0x83,0x92},
	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_whiteboard[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_lightgreen[] = {


	{HM2056_REG_END, 0xff},	/* END MARKER */
};


static struct regval_list hm2056_effect_brown[] = {

	{HM2056_REG_END, 0xff}, /* END MARKER */
};

static struct regval_list hm2056_effect_waterblue[] = {

	{HM2056_REG_END, 0xff}, /* END MARKER */
};


static struct regval_list hm2056_effect_blackboard[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_effect_aqua[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_l6[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_l5[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_l4[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_l3[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_l2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_exposure_l1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_exposure_h0[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_exposure_h1[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_exposure_h2[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_h3[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_h4[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_h5[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_exposure_h6[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

//next preview mode

static struct regval_list hm2056_previewmode_mode_auto[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_previewmode_mode_nightmode[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_previewmode_mode_sports[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};
static struct regval_list hm2056_previewmode_mode_portrait[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};

static struct regval_list hm2056_previewmode_mode_landscape[] = {

	{HM2056_REG_END, 0xff},	/* END MARKER */
};


//preview mode end
static int hm2056_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int hm2056_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int hm2056_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret=0;
 	while (vals->reg_num != HM2056_REG_END) {
		if (vals->reg_num == HM2056_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		}
		ret= hm2056_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

static int hm2056_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}



static int hm2056_init(struct v4l2_subdev *sd, u32 val)
{
	struct hm2056_info *info = container_of(sd, struct hm2056_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;


	return hm2056_write_array(sd, hm2056_init_regs);
}

static int hm2056_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = hm2056_read(sd, 0x0001, &v);
	printk("HM2056_CHIP_ID_H = %2x\n",v);
	if (ret < 0)
		return ret;
	if (v != HM2056_CHIP_ID_H)
		return -ENODEV;
	ret = hm2056_read(sd, 0x0002, &v);
	printk("HM2056_CHIP_ID_L = %2x\n",v);
	if (ret < 0)
		return ret;
	if (v != HM2056_CHIP_ID_L)
		return -ENODEV;


	return 0;
}

static struct hm2056_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} hm2056_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= NULL,
	},
};
#define N_HM2056_FMTS ARRAY_SIZE(hm2056_formats)

static struct hm2056_win_size {
	int	width;
	int	height;
	struct regval_list *regs; /* Regs to tweak */
} hm2056_win_sizes[] = {
	/*SVGA*/

	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= hm2056_win_VGA,
	},


	/* MAX */
	{
		.width		= MAX_WIDTH,
		.height		= MAX_HEIGHT,
		.regs 		= hm2056_win_UXGA,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(hm2056_win_sizes))

static int hm2056_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_HM2056_FMTS)
		return -EINVAL;

	*code = hm2056_formats[index].mbus_code;
	return 0;
}

static int hm2056_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct hm2056_format_struct **ret_fmt,
		struct hm2056_win_size **ret_wsize)
{
	int index;
	struct hm2056_win_size *wsize;

	for (index = 0; index < N_HM2056_FMTS; index++)
		if (hm2056_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_HM2056_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = hm2056_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = hm2056_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = hm2056_win_sizes; wsize < hm2056_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;
	if (wsize >= hm2056_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = hm2056_formats[index].colorspace;
	return 0;
}

static int hm2056_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return hm2056_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int hm2056_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct hm2056_info *info = container_of(sd, struct hm2056_info, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct hm2056_format_struct *fmt_s;
	struct hm2056_win_size *wsize;
	int ret,i,len;

	ret = hm2056_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;


	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = hm2056_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}

	if ((info->win != wsize) && wsize->regs) {

		memset(data, 0, sizeof(*data));
		data->mipi_clk = 138;//288; /* Mbps. */


		if ((wsize->width == MAX_WIDTH)
			&& (wsize->height == MAX_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			len = ARRAY_SIZE(hm2056_win_UXGA);
			data->reg_num = len;
			msleep(400);
			for(i =0;i<len;i++) {
				data->reg[i].addr = hm2056_win_UXGA[i].reg_num;
				data->reg[i].data = hm2056_win_UXGA[i].value;
			}
		} else if ((wsize->width == VGA_WIDTH)
			&& (wsize->height == VGA_HEIGHT)) {
			data->flags = V4L2_I2C_ADDR_16BIT;
			data->slave_addr = client->addr;
			msleep(400);
			len = ARRAY_SIZE(hm2056_win_VGA);
			data->reg_num = len;
			for(i =0;i<len;i++) {
				data->reg[i].addr = hm2056_win_VGA[i].reg_num;
				data->reg[i].data = hm2056_win_VGA[i].value;
			}
		} 
	}

	info->fmt = fmt_s;
	info->win = wsize;
	return 0;
}

static int hm2056_s_stream(struct v4l2_subdev *sd, int enable)
{

	return 0;
}

static int hm2056_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int hm2056_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	//snapshot max size
	a->bounds.width 		= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
	//snapshot max size end
	a->defrect.left 		= 0;
	a->defrect.top			= 0;
	//preview max size
	a->defrect.width		= MAX_PREVIEW_WIDTH;
	a->defrect.height		= MAX_PREVIEW_HEIGHT;
	//preview max size end
	a->type 			= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int hm2056_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int hm2056_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int hm2056_frame_rates[] = { 30, 15, 10, 5, 1 };

static int hm2056_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(hm2056_frame_rates))
		return -EINVAL;

	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = hm2056_frame_rates[interval->index];
	return 0;
}

static int hm2056_enum_framesizes(struct v4l2_subdev *sd,
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
		struct hm2056_win_size *win = &hm2056_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int hm2056_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int hm2056_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk(KERN_DEBUG "[HM2056]set brightness %d\n", brightness);

	switch (brightness) {
		case BRIGHTNESS_L4:
			hm2056_write_array(sd, hm2056_brightness_l4);
			break;
		case BRIGHTNESS_L3:
			hm2056_write_array(sd, hm2056_brightness_l3);
			break;
		case BRIGHTNESS_L2:
			hm2056_write_array(sd, hm2056_brightness_l2);
			break;
		case BRIGHTNESS_L1:
			hm2056_write_array(sd, hm2056_brightness_l1);
			break;
		case BRIGHTNESS_H0:
			hm2056_write_array(sd, hm2056_brightness_h0);
			break;
		case BRIGHTNESS_H1:
			hm2056_write_array(sd, hm2056_brightness_h1);
			break;
		case BRIGHTNESS_H2:
			hm2056_write_array(sd, hm2056_brightness_h2);
			break;
		case BRIGHTNESS_H3:
			hm2056_write_array(sd, hm2056_brightness_h3);
			break;
		case BRIGHTNESS_H4:
			hm2056_write_array(sd, hm2056_brightness_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int hm2056_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	switch (contrast) {
		case CONTRAST_L4:
			hm2056_write_array(sd, hm2056_contrast_l4);
			break;
		case CONTRAST_L3:
			hm2056_write_array(sd, hm2056_contrast_l3);
			break;
		case CONTRAST_L2:
			hm2056_write_array(sd, hm2056_contrast_l2);
			break;
		case CONTRAST_L1:
			hm2056_write_array(sd, hm2056_contrast_l1);
			break;
		case CONTRAST_H0:
			hm2056_write_array(sd, hm2056_contrast_h0);
			break;
		case CONTRAST_H1:
			hm2056_write_array(sd, hm2056_contrast_h1);
			break;
		case CONTRAST_H2:
			hm2056_write_array(sd, hm2056_contrast_h2);
			break;
		case CONTRAST_H3:
			hm2056_write_array(sd, hm2056_contrast_h3);
			break;
		case CONTRAST_H4:
			hm2056_write_array(sd, hm2056_contrast_h4);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int hm2056_set_saturation(struct v4l2_subdev *sd, int saturation)
{
	printk(KERN_DEBUG "[HM2056]set saturation %d\n", saturation);

	switch (saturation) {
		case SATURATION_L2:
			hm2056_write_array(sd, hm2056_saturation_l2);
			break;
		case SATURATION_L1:
			hm2056_write_array(sd, hm2056_saturation_l1);
			break;
		case SATURATION_H0:
			hm2056_write_array(sd, hm2056_saturation_h0);
			break;
		case SATURATION_H1:
			hm2056_write_array(sd, hm2056_saturation_h1);
			break;
		case SATURATION_H2:
			hm2056_write_array(sd, hm2056_saturation_h2);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int hm2056_set_wb(struct v4l2_subdev *sd, int wb)
{
	printk(KERN_DEBUG "[HM2056]set hm2056_set_wb %d\n", wb);

	switch (wb) {
		case WHITE_BALANCE_AUTO:
			hm2056_write_array(sd, hm2056_whitebalance_auto);
			break;
		case WHITE_BALANCE_INCANDESCENT:
			hm2056_write_array(sd, hm2056_whitebalance_incandescent);
			break;
		case WHITE_BALANCE_FLUORESCENT:
			hm2056_write_array(sd, hm2056_whitebalance_fluorescent);
			break;
		case WHITE_BALANCE_DAYLIGHT:
			hm2056_write_array(sd, hm2056_whitebalance_daylight);
			break;
		case WHITE_BALANCE_CLOUDY_DAYLIGHT:
			hm2056_write_array(sd, hm2056_whitebalance_cloudy);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}



static int hm2056_set_sharpness(struct v4l2_subdev *sd, int sharpness)
{

	printk(KERN_DEBUG "[HM2056]set sharpness %d\n", sharpness);
	switch (sharpness) {
		case SHARPNESS_L2:
			hm2056_write_array(sd, hm2056_sharpness_l2);
			break;
		case SHARPNESS_L1:
			hm2056_write_array(sd, hm2056_sharpness_l1);
			break;
		case SHARPNESS_H0:
			hm2056_write_array(sd, hm2056_sharpness_h0);
			break;
		case SHARPNESS_H1:
			hm2056_write_array(sd, hm2056_sharpness_h1);
			break;
		case SHARPNESS_H2:
			hm2056_write_array(sd, hm2056_sharpness_h2);
			break;
		case SHARPNESS_H3:
			hm2056_write_array(sd, hm2056_sharpness_h3);
			break;
		case SHARPNESS_H4:
			hm2056_write_array(sd, hm2056_sharpness_h4);
			break;
		case SHARPNESS_H5:
			hm2056_write_array(sd, hm2056_sharpness_h5);
			break;
		case SHARPNESS_H6:
			hm2056_write_array(sd, hm2056_sharpness_h6);
			break;
		case SHARPNESS_H7:
			hm2056_write_array(sd, hm2056_sharpness_h7);
			break;
		case SHARPNESS_H8:
			hm2056_write_array(sd, hm2056_sharpness_h8);
			break;
		case SHARPNESS_MAX:
			hm2056_write_array(sd, hm2056_sharpness_max);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}



static int hm2056_set_effect(struct v4l2_subdev *sd, int effect)
{
	printk(KERN_DEBUG "[HM2056]set contrast %d\n", effect);

	switch (effect) {
		case EFFECT_NONE:
			hm2056_write_array(sd, hm2056_effect_none);
			break;
		case EFFECT_MONO:
			hm2056_write_array(sd, hm2056_effect_mono);
			break;
		case EFFECT_NEGATIVE:
			hm2056_write_array(sd, hm2056_effect_negative);
			break;
		case EFFECT_SOLARIZE:
			hm2056_write_array(sd, hm2056_effect_solarize);
			break;
		case EFFECT_SEPIA:
			hm2056_write_array(sd, hm2056_effect_sepia);
			break;
		case EFFECT_POSTERIZE:
			hm2056_write_array(sd, hm2056_effect_posterize);
			break;
		case EFFECT_WHITEBOARD:
			hm2056_write_array(sd, hm2056_effect_whiteboard);
			break;
		case EFFECT_BLACKBOARD:
			hm2056_write_array(sd, hm2056_effect_blackboard);
			break;
		case EFFECT_WATER_GREEN:
			hm2056_write_array(sd, hm2056_effect_lightgreen);
			break;
		case EFFECT_WATER_BLUE:
			hm2056_write_array(sd, hm2056_effect_waterblue);
			break;
		case EFFECT_BROWN:
			hm2056_write_array(sd, hm2056_effect_brown);
			break;
		case EFFECT_AQUA:
			hm2056_write_array(sd, hm2056_effect_aqua);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int hm2056_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	struct hm2056_info *info = container_of(sd, struct hm2056_info, sd);

	printk("hm2056_set_framerate %d\n", framerate);

	if (framerate < FRAME_RATE_AUTO)
		framerate = FRAME_RATE_AUTO;
	else if (framerate > 30)
		framerate = 30;
	info->frame_rate = framerate;

	return 0;
}

static int hm2056_set_exposure(struct v4l2_subdev *sd, int exposure)
{
	printk(KERN_DEBUG "[HM2056]set exposure %d\n", exposure);

	switch (exposure) {
		case EXPOSURE_L6:
			hm2056_write_array(sd, hm2056_exposure_l6);
			break;
		case EXPOSURE_L5:
			hm2056_write_array(sd, hm2056_exposure_l5);
			break;
		case EXPOSURE_L4:
			hm2056_write_array(sd, hm2056_exposure_l4);
			break;
		case EXPOSURE_L3:
			hm2056_write_array(sd, hm2056_exposure_l3);
			break;
		case EXPOSURE_L2:
			hm2056_write_array(sd, hm2056_exposure_l2);
			break;
		case EXPOSURE_L1:
			hm2056_write_array(sd, hm2056_exposure_l1);
			break;
		case EXPOSURE_H0:
			hm2056_write_array(sd, hm2056_exposure_h0);
			break;
		case EXPOSURE_H1:
			hm2056_write_array(sd, hm2056_exposure_h1);
			break;
		case EXPOSURE_H2:
			hm2056_write_array(sd, hm2056_exposure_h2);
			break;
		case EXPOSURE_H3:
			hm2056_write_array(sd, hm2056_exposure_h3);
			break;
		case EXPOSURE_H4:
			hm2056_write_array(sd, hm2056_exposure_h4);
			break;
		case EXPOSURE_H5:
			hm2056_write_array(sd, hm2056_exposure_h5);
			break;
		case EXPOSURE_H6:
			hm2056_write_array(sd, hm2056_exposure_h6);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int hm2056_set_previewmode(struct v4l2_subdev *sd, int mode)
{
	printk(KERN_INFO "MIKE:[HM2056]set_previewmode %d\n", mode);

	switch (mode) {
		case preview_auto:
			hm2056_write_array(sd, hm2056_previewmode_mode_auto);
			break;
		case normal:
			hm2056_write_array(sd, hm2056_previewmode_mode_auto);
			break;
		case dynamic:
		case beatch:
		case bar:
		case snow:
		case landscape:
			hm2056_write_array(sd, hm2056_previewmode_mode_landscape);
			break;
		case nightmode:
			hm2056_write_array(sd, hm2056_previewmode_mode_nightmode);
			break;
		case theatre:
		case party:
		case candy:
		case fireworks:
		case sunset:
		case nightmode_portrait:
		case sports:
			hm2056_write_array(sd, hm2056_previewmode_mode_sports);
			break;
		case anti_shake:
		case portrait:
			hm2056_write_array(sd, hm2056_previewmode_mode_portrait);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static int hm2056_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hm2056_info *info = container_of(sd, struct hm2056_info, sd);
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
			isp_setting.setting = (struct v4l2_isp_regval *)&hm2056_isp_setting;
			isp_setting.size = ARRAY_SIZE(hm2056_isp_setting);
			memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
			break;
		case V4L2_CID_ISP_PARM:
			ctrl->value = (int)&hm2056_isp_parm;
			break;
		case V4L2_CID_SCENE:
			ctrl->value = info->scene_mode;
			break;
		case V4L2_CID_SHARPNESS:
			ctrl->value = info->sharpness;
			break;
		case V4L2_CID_SHUTTER_SPEED:
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int hm2056_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hm2056_info *info = container_of(sd, struct hm2056_info, sd);
	int ret = 0;

	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ret = hm2056_set_brightness(sd, ctrl->value);
			if (!ret)
				info->brightness = ctrl->value;
			break;
		case V4L2_CID_CONTRAST:
			ret = hm2056_set_contrast(sd, ctrl->value);
			if (!ret)
				info->contrast = ctrl->value;
			break;
		case V4L2_CID_SATURATION:
			ret = hm2056_set_saturation(sd, ctrl->value);
			if (!ret)
				info->saturation = ctrl->value;
			break;
		case V4L2_CID_DO_WHITE_BALANCE:
			ret = hm2056_set_wb(sd, ctrl->value);
			if (!ret)
				info->wb = ctrl->value;
			break;
		case V4L2_CID_EXPOSURE:
			ret = hm2056_set_exposure(sd, ctrl->value);
			if (!ret)
				info->exposure = ctrl->value;
			break;
		case V4L2_CID_EFFECT:
			ret = hm2056_set_effect(sd, ctrl->value);
			if (!ret)
				info->effect = ctrl->value;
			break;
		case V4L2_CID_FRAME_RATE:
			ret = hm2056_set_framerate(sd, ctrl->value);
			break;
		case V4L2_CID_SCENE:
			ret = hm2056_set_previewmode(sd, ctrl->value);
			if (!ret)
				info->scene_mode = ctrl->value;
			break;
			case V4L2_CID_SHARPNESS:
			ret = hm2056_set_sharpness(sd, ctrl->value);
			break;
		case V4L2_CID_SET_FOCUS:
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}
	return ret;
}

static int hm2056_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_HM2056, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int hm2056_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = hm2056_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;

	return ret;
}

static int hm2056_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	hm2056_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}
#endif

static const struct v4l2_subdev_core_ops hm2056_core_ops = {
	.g_chip_ident = hm2056_g_chip_ident,
	.g_ctrl = hm2056_g_ctrl,
	.s_ctrl = hm2056_s_ctrl,
	.queryctrl = hm2056_queryctrl,
	.reset = hm2056_reset,
	.init = hm2056_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = hm2056_g_register,
	.s_register = hm2056_s_register,
#endif
};

static const struct v4l2_subdev_video_ops hm2056_video_ops = {
	.enum_mbus_fmt = hm2056_enum_mbus_fmt,
	.try_mbus_fmt = hm2056_try_mbus_fmt,
	.s_mbus_fmt = hm2056_s_mbus_fmt,
	.s_stream = hm2056_s_stream,
	.cropcap = hm2056_cropcap,
	.g_crop	= hm2056_g_crop,
	.s_parm = hm2056_s_parm,
	.g_parm = hm2056_g_parm,
	.enum_frameintervals = hm2056_enum_frameintervals,
	.enum_framesizes = hm2056_enum_framesizes,
};

static const struct v4l2_subdev_ops hm2056_ops = {
	.core = &hm2056_core_ops,
	.video = &hm2056_video_ops,
};

static int hm2056_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct hm2056_info *info;
	int ret;

	info = kzalloc(sizeof(struct hm2056_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &hm2056_ops);

	/* Make sure it's an hm2056 */
	ret = hm2056_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an hm2056 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "hm2056 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return ret;
}

static int hm2056_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct hm2056_info *info = container_of(sd, struct hm2056_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id hm2056_id[] = {
	{"hm2056", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hm2056_id);

static struct i2c_driver hm2056_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "hm2056",
	},
	.probe		= hm2056_probe,
	.remove		= hm2056_remove,
	.id_table	= hm2056_id,
};

static __init int init_hm2056(void)
{
	return i2c_add_driver(&hm2056_driver);
}

static __exit void exit_hm2056(void)
{
	i2c_del_driver(&hm2056_driver);
}

fs_initcall(init_hm2056);
module_exit(exit_hm2056);

MODULE_DESCRIPTION("A low-level driver for Himax hm2056 sensors");
MODULE_LICENSE("GPL");
