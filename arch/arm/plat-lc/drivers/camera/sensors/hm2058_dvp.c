
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
#include "hm2058_dvp.h"
#include <linux/time.h>
#include <linux/unistd.h>





#define HM2058_CHIP_ID_H	(0x20)
#define HM2058_CHIP_ID_L	(0x56)

#define UXGA_WIDTH      1600
#define UXGA_HEIGHT     1200

#define VGA_WIDTH		640
#define VGA_HEIGHT		480

//#define VGA_WIDTH		640
//#define VGA_HEIGHT		480



#define MAX_WIDTH		1600
#define	MAX_HEIGHT		1200

#define MAX_PREVIEW_WIDTH  1600	//VGA_WIDTH
#define MAX_PREVIEW_HEIGHT  1200//VGA_HEIGHT

#define HM2058_REG_END		0xfffe
#define HM2058_REG_DELAY	0xfffd



struct hm2058_format_struct;
struct hm2058_info {
	struct v4l2_subdev sd;
	struct hm2058_format_struct *fmt;
	struct hm2058_win_size *win;
	int brightness;
	int contrast;
	int frame_rate;
	int hm2058_open;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};
static int hm2058_s_stream(struct v4l2_subdev *sd, int enable);


static struct regval_list hm2058_init_regs[] ={
		{0x0022,0x00},//	; Reset
		{0x0004,0x10},//	;
		{0x0006,0x00},//	; Flip/Mirror
		{0x000D,0x11},//	; 20120220 to fix morie
		{0x000E,0x11},//	; Binning ON
		{0x000F,0x00},//	; IMGCFG
		{0x0011,0x02},//	;
		{0x0012,0x1C},//	; 2012.02.08
		{0x0013,0x01},//	;
		{0x0015,0x02},//	;
		{0x0016,0x80},//	;
		{0x0018,0x00},//	;
		{0x001D,0x40},//	;
		{0x0020,0x40},//	;
		{0x0025,0x00},//	; CKCFG 80 from system clock, 00 from PLL
		{0x0026,0x87},//	; PLL1CFG should be 07 when system clock, should be 87 when PLL
		{0x0027,0x30},//	; YUV output //0x18
		{0x002C,0x0A},//	; Set default vaule for CP and resistance of LPF to 1010
		{0x0040,0x20},//	; 20120224 for BLC stable
		{0x0053,0x0A},//	;
		{0x0044,0x06},//	; enable BLC_phase2
		{0x0046,0xD8},//	; enable BLC_phase1, disable BLC_phase2 dithering
		{0x004A,0x0A},//	; disable BLC_phase2 hot pixel filter
		{0x004B,0x72},//	;
		{0x0075,0x01},//	; in OMUX data swap for debug usage
		{0x002A,0x1F},//	; Output=48MHz
		{0x0070,0x5F},//	;
		{0x0071,0xFF},//	;
		{0x0072,0x55},//	;
		{0x0073,0x50},//	;
		{0x0080,0xC8},//	; 2012.02.08
		{0x0082,0xA2},//	;
		{0x0083,0xF0},//	;
		{0x0085,0x11},//	; Enable Thin-Oxide Case (Kwangoh kim), Set ADC power to 100% Enable thermal sensor control bit[7] 0:on 1:off 2012 02 13 (YL)
		{0x0086,0x02},//	; K.Kim 2011.12.09
		{0x0087,0x80},//	; K.Kim 2011.12.09
		{0x0088,0x6C},//	;
		{0x0089,0x2E},//	;
		{0x008A,0x7D},//	; 20120224 for BLC stable
		{0x008D,0x20},//	;
		{0x0090,0x00},//	; 1.5x(Change Gain Table )
		{0x0091,0x10},//	; 3x  (3x CTIA)
		{0x0092,0x11},//	; 6x  (3x CTIA + 2x PGA)
		{0x0093,0x12},//	; 12x (3x CTIA + 4x PGA)
		{0x0094,0x16},//	; 24x (3x CTIA + 8x PGA)
		{0x0095,0x08},//	; 1.5x  20120217 for color shift
		{0x0096,0x00},//	; 3x    20120217 for color shift
		{0x0097,0x10},//	; 6x    20120217 for color shift
		{0x0098,0x11},//	; 12x   20120217 for color shift
		{0x0099,0x12},//	; 24x   20120217 for color shift
		{0x009A,0x06},//	; 24x
		{0x009B,0x34},//	;
		{0x00A0,0x00},//	;
		{0x00A1,0x04},//	; 2012.02.06(for Ver.C)
		{0x011F,0xF7},//	; simple bpc P31 & P33[4] P40 P42 P44[5]
		{0x0120,0x37},//	; 36:50Hz, 37:60Hz, BV_Win_Weight_En=1
		{0x0121,0x83},//	; NSatScale_En=0, NSatScale=0
		{0x0122,0x7B},//
		{0x0123,0xC2},//
		{0x0124,0xDE},//
		{0x0125,0xFF},//
		{0x0126,0x70},//
		{0x0128,0x1F},//
		{0x0132,0x10},//
		{0x0131,0xBD},//	; simle bpc enable[4]
		{0x0140,0x14},//
		{0x0141,0x0A},//
		{0x0142,0x14},//
		{0x0143,0x0A},//
		{0x0144,0x06},//	; Sort bpc hot pixel ratio
		{0x0145,0x00},//
		{0x0146,0x20},//
		{0x0147,0x0A},//
		{0x0148,0x10},//
		{0x0149,0x0C},//
		{0x014A,0x80},//
		{0x014B,0x80},//
		{0x014C,0x2E},//
		{0x014D,0x2E},//
		{0x014E,0x05},//
		{0x014F,0x05},//
		{0x0150,0x0D},//
		{0x0155,0x00},//
		{0x0156,0x10},//
		{0x0157,0x0A},//
		{0x0158,0x0A},//
		{0x0159,0x0A},//
		{0x015A,0x05},//
		{0x015B,0x05},//
		{0x015C,0x05},//
		{0x015D,0x05},//
		{0x015E,0x08},//
		{0x015F,0xFF},//
		{0x0160,0x50},//	; OTP BPC 2line & 4line enable
		{0x0161,0x20},//
		{0x0162,0x14},//
		{0x0163,0x0A},//
		{0x0164,0x10},//	; OTP 4line Strength
		{0x0165,0x08},//
		{0x0166,0x0A},//
		{0x018C,0x24},//
		{0x018D,0x04},//	; Cluster correction enable singal from thermal sensor (YL 2012 02 13)
		{0x018E,0x00},//	; Enable Thermal sensor control bit[7] (YL 2012 02 13)
		{0x018F,0x11},//	; Cluster Pulse enable T1[0] T2[1] T3[2] T4[3]
		{0x0190,0x80},//	; A11 BPC Strength[7:3], cluster correct P11[0]P12[1]P13[2]
		{0x0191,0x47},//	; A11[0],A7[1],Sort[3],A13 AVG[6]
		{0x0192,0x48},//	; A13 Strength[4:0],hot pixel detect for cluster[6]
		{0x0193,0x64},//
		{0x0194,0x32},//
		{0x0195,0xc8},//
		{0x0196,0x96},//
		{0x0197,0x64},//
		{0x0198,0x32},//
		{0x0199,0x14},//	; A13 hot pixel th
		{0x019A,0x20},//	; A13 edge detect th
		{0x019B,0x14},//
		{0x01B0,0x55},//	; G1G2 Balance
		{0x01B1,0x0C},//
		{0x01B2,0x0A},//
		{0x01B3,0x10},//
		{0x01B4,0x0E},//
		{0x01BA,0x10},//	; BD
		{0x01BB,0x04},//
		{0x01D8,0x40},//
		{0x01DE,0x60},//
		{0x01E4,0x10},//
		{0x01E5,0x10},//
		{0x01F2,0x0C},//
		{0x01F3,0x14},//
		{0x01F8,0x04},//
		{0x01F9,0x0C},//
		{0x01FE,0x02},//
		{0x01FF,0x04},//
		{0x0220,0x00},//	; LSC
		{0x0221,0xB0},//
		{0x0222,0x00},//
		{0x0223,0x80},//
		{0x0224,0x8E},//
		{0x0225,0x00},//
		{0x0226,0x88},//
		{0x022A,0x88},//
		{0x022B,0x00},//
		{0x022C,0x88},//
		{0x022D,0x13},//
		{0x022E,0x0B},//
		{0x022F,0x13},//
		{0x0230,0x0B},//
		{0x0233,0x13},//
		{0x0234,0x0B},//
		{0x0235,0x28},//
		{0x0236,0x03},//
		{0x0237,0x28},//
		{0x0238,0x03},//
		{0x023B,0x28},//
		{0x023C,0x03},//
		{0x023D,0x5C},//
		{0x023E,0x02},//
		{0x023F,0x5C},//
		{0x0240,0x02},//
		{0x0243,0x5C},//
		{0x0244,0x02},//
		{0x0251,0x0E},//
		{0x0252,0x00},//
		{0x0280,0x0A},//	; Gamma
		{0x0282,0x14},//
		{0x0284,0x2A},//
		{0x0286,0x50},//
		{0x0288,0x60},//
		{0x028A,0x6D},//
		{0x028C,0x79},//
		{0x028E,0x82},//
		{0x0290,0x8A},//
		{0x0292,0x91},//
		{0x0294,0x9C},//
		{0x0296,0xA7},//
		{0x0298,0xBA},//
		{0x029A,0xCD},//
		{0x029C,0xE0},//
		{0x029E,0x2D},//
		{0x02A0,0x06},//	; Gamma by Alpha
		{0x02E0,0x04},//	; CCM by Alpha
		{0x02C0,0xB1},//	; CCM
		{0x02C1,0x01},//
		{0x02C2,0x7D},//
		{0x02C3,0x07},//
		{0x02C4,0xD2},//
		{0x02C5,0x07},//
		{0x02C6,0xC4},//
		{0x02C7,0x07},//
		{0x02C8,0x79},//
		{0x02C9,0x01},//
		{0x02CA,0xC4},//
		{0x02CB,0x07},//
		{0x02CC,0xF7},//
		{0x02CD,0x07},//
		{0x02CE,0x3B},//
		{0x02CF,0x07},//
		{0x02D0,0xCF},//
		{0x02D1,0x01},//
		{0x0302,0x00},//
		{0x0303,0x00},//
		{0x0304,0x00},//
		{0x02F0,0x5E},//
		{0x02F1,0x07},//
		{0x02F2,0xA0},//
		{0x02F3,0x00},//
		{0x02F4,0x02},//
		{0x02F5,0x00},//
		{0x02F6,0xC4},//
		{0x02F7,0x07},//
		{0x02F8,0x11},//
		{0x02F9,0x00},//
		{0x02FA,0x2A},//
		{0x02FB,0x00},//
		{0x02FC,0xA1},//
		{0x02FD,0x07},//
		{0x02FE,0xB8},//
		{0x02FF,0x07},//
		{0x0300,0xA7},//
		{0x0301,0x00},//
		{0x0305,0x00},//
		{0x0306,0x00},//
		{0x0307,0x7A},//
		{0x032D,0x00},//
		{0x032E,0x01},//
		{0x032F,0x00},//
		{0x0330,0x01},//
		{0x0331,0x00},//
		{0x0332,0x01},//
		{0x0333,0x82},//	; AWB channel offset
		{0x0334,0x00},//
		{0x0335,0x84},//
		{0x0336,0x00},// 	; LED AWB gain
		{0x0337,0x01},//
		{0x0338,0x00},//
		{0x0339,0x01},//
		{0x033A,0x00},//
		{0x033B,0x01},//
		{0x033E,0x04},//
		{0x033F,0x86},//
		{0x0340,0x30},//	; AWB
		{0x0341,0x44},//
		{0x0342,0x4A},//
		{0x0343,0x42},//	; CT1
		{0x0344,0x74},//  	;
		{0x0345,0x4F},//	; CT2
		{0x0346,0x67},//  	;
		{0x0347,0x5C},//	; CT3
		{0x0348,0x59},//	;
		{0x0349,0x67},//	; CT4
		{0x034A,0x4D},//	;
		{0x034B,0x6E},//	; CT5
		{0x034C,0x44},//	;
		{0x0350,0x80},//
		{0x0351,0x80},//
		{0x0352,0x18},//
		{0x0353,0x18},//
		{0x0354,0x6E},//
		{0x0355,0x4A},//
		{0x0356,0x7A},//
		{0x0357,0xC6},//
		{0x0358,0x06},//
		{0x035A,0x06},//
		{0x035B,0xA0},//
		{0x035C,0x73},//
		{0x035D,0x5A},//
		{0x035E,0xC6},//
		{0x035F,0xA0},//
		{0x0360,0x02},//
		{0x0361,0x18},//
		{0x0362,0x80},//
		{0x0363,0x6C},//
		{0x0364,0x00},//
		{0x0365,0xF0},//
		{0x0366,0x20},//
		{0x0367,0x0C},//
		{0x0369,0x00},//
		{0x036A,0x10},//
		{0x036B,0x10},//
		{0x036E,0x20},//
		{0x036F,0x00},//
		{0x0370,0x10},//
		{0x0371,0x18},//
		{0x0372,0x0C},//
		{0x0373,0x38},//
		{0x0374,0x3A},//
		{0x0375,0x13},//
		{0x0376,0x22},//
		{0x0380,0xFF},//
		{0x0381,0x4A},//
		{0x0382,0x36},//
		{0x038A,0x40},//
		{0x038B,0x08},//
		{0x038C,0xC1},//
		{0x038E,0x40},//
		{0x038F,0x09},//
		{0x0390,0xD0},//
		{0x0391,0x05},//
		{0x0393,0x80},//
		{0x0395,0x21},//	; AEAWB skip count
		{0x0398,0x02},//	; AE Frame Control
		{0x0399,0x74},//
		{0x039A,0x03},//
		{0x039B,0x11},//
		{0x039C,0x03},//
		{0x039D,0xAE},//
		{0x039E,0x04},//
		{0x039F,0xE8},//
		{0x03A0,0x06},//
		{0x03A1,0x22},//
		{0x03A2,0x07},//
		{0x03A3,0x5C},//
		{0x03A4,0x09},//
		{0x03A5,0xD0},//
		{0x03A6,0x0C},//
		{0x03A7,0x0E},//
		{0x03A8,0x10},//
		{0x03A9,0x18},//
		{0x03AA,0x20},//
		{0x03AB,0x28},//
		{0x03AC,0x1E},//
		{0x03AD,0x1A},//
		{0x03AE,0x13},//
		{0x03AF,0x0C},//
		{0x03B0,0x0B},//
		{0x03B1,0x09},//
		{0x03B3,0x10},//	; AE window array
		{0x03B4,0x00},//
		{0x03B5,0x10},//
		{0x03B6,0x00},//
		{0x03B7,0xEA},//
		{0x03B8,0x00},//
		{0x03B9,0x3A},//
		{0x03BA,0x01},//
		{0x03BB,0x9F},//	; enable 5x5 window
		{0x03BC,0xCF},//
		{0x03BD,0xE7},//
		{0x03BE,0xF3},//
		{0x03BF,0x01},//
		{0x03D0,0xF8},//	; AE NSMode YTh
		{0x03E0,0x04},//	; weight
		{0x03E1,0x01},//
		{0x03E2,0x04},//
		{0x03E4,0x10},//
		{0x03E5,0x12},//
		{0x03E6,0x00},//
		{0x03E8,0x21},//
		{0x03E9,0x23},//
		{0x03EA,0x01},//
		{0x03EC,0x21},//
		{0x03ED,0x23},//
		{0x03EE,0x01},//
		{0x03F0,0x20},//
		{0x03F1,0x22},//
		{0x03F2,0x00},//
		{0x0420,0x84},//	; Digital Gain offset
		{0x0421,0x00},//
		{0x0422,0x00},//
		{0x0423,0x83},//
		{0x0430,0x08},//	; ABLC
		{0x0431,0x28},//
		{0x0432,0x10},//
		{0x0433,0x08},//
		{0x0435,0x0C},//
		{0x0450,0xFF},//
		{0x0451,0xE8},//
		{0x0452,0xC4},//
		{0x0453,0x88},//
		{0x0454,0x00},//
		{0x0458,0x98},//
		{0x0459,0x03},//
		{0x045A,0x00},//
		{0x045B,0x28},//
		{0x045C,0x00},//
		{0x045D,0x68},//
		{0x0466,0x14},//
		{0x047A,0x00},//	; ELOFFNRB
		{0x047B,0x00},//	; ELOFFNRY
		{0x0480,0x58},//
		{0x0481,0x06},//
		{0x0482,0x0C},//
		{0x04B0,0x50},//	; Contrast
		{0x04B6,0x30},//
		{0x04B9,0x10},//
		{0x04B3,0x10},//
		{0x04B1,0x8E},//
		{0x04B4,0x20},//
		{0x0540,0x00},//	;
		{0x0541,0x9D},//	; 60Hz Flicker
		{0x0542,0x00},//	;
		{0x0543,0xBC},//	; 50Hz Flicker
		{0x0580,0x01},//	; Blur str sigma
		{0x0581,0x0F},//	; Blur str sigma ALPHA
		{0x0582,0x04},//	; Blur str sigma OD
		{0x0594,0x00},//	; UV Gray TH
		{0x0595,0x04},//	; UV Gray TH Alpha
		{0x05A9,0x03},//
		{0x05AA,0x40},//
		{0x05AB,0x80},//
		{0x05AC,0x0A},//
		{0x05AD,0x10},//
		{0x05AE,0x0C},//
		{0x05AF,0x0C},//
		{0x05B0,0x03},//
		{0x05B1,0x03},//
		{0x05B2,0x1C},//
		{0x05B3,0x02},//
		{0x05B4,0x00},//
		{0x05B5,0x0C},//	; BlurW
		{0x05B8,0x80},//
		{0x05B9,0x32},//
		{0x05BA,0x00},//
		{0x05BB,0x80},//
		{0x05BC,0x03},//
		{0x05BD,0x00},//
		{0x05BF,0x05},//
		{0x05C0,0x10},//	; BlurW LowLight
		{0x05C3,0x00},//
		{0x05C4,0x0C},//	; BlurW Outdoor
		{0x05C5,0x20},//
		{0x05C7,0x01},//
		{0x05C8,0x14},//
		{0x05C9,0x54},//
		{0x05CA,0x14},//
		{0x05CB,0xE0},//
		{0x05CC,0x20},//
		{0x05CD,0x00},//
		{0x05CE,0x08},//
		{0x05CF,0x60},//
		{0x05D0,0x10},//
		{0x05D1,0x05},//
		{0x05D2,0x03},//
		{0x05D4,0x00},//
		{0x05D5,0x05},//
		{0x05D6,0x05},//
		{0x05D7,0x05},//
		{0x05D8,0x08},//
		{0x05DC,0x0C},//
		{0x05D9,0x00},//
		{0x05DB,0x00},//
		{0x05DD,0x0F},//
		{0x05DE,0x00},//
		{0x05DF,0x0A},//
		{0x05E0,0xA0},//	; Scaler
		{0x05E1,0x00},//
		{0x05E2,0xA0},//
		{0x05E3,0x00},//
		{0x05E4,0x04},//	; Windowing
		{0x05E5,0x00},//
		{0x05E6,0x83},//
		{0x05E7,0x02},//
		{0x05E8,0x06},//
		{0x05E9,0x00},//
		{0x05EA,0xE5},//
		{0x05EB,0x01},//
		{0x0660,0x04},//
		{0x0661,0x16},//
		{0x0662,0x04},//
		{0x0663,0x28},//
		{0x0664,0x04},//
		{0x0665,0x18},//
		{0x0666,0x04},//
		{0x0667,0x21},//
		{0x0668,0x04},//
		{0x0669,0x0C},//
		{0x066A,0x04},//
		{0x066B,0x25},//
		{0x066C,0x00},//
		{0x066D,0x12},//
		{0x066E,0x00},//
		{0x066F,0x80},//
		{0x0670,0x00},//
		{0x0671,0x0A},//
		{0x0672,0x04},//
		{0x0673,0x1D},//
		{0x0674,0x04},//
		{0x0675,0x1D},//
		{0x0676,0x00},//
		{0x0677,0x7E},//
		{0x0678,0x01},//
		{0x0679,0x47},//
		{0x067A,0x00},//
		{0x067B,0x73},//
		{0x067C,0x04},//
		{0x067D,0x14},//
		{0x067E,0x04},//
		{0x067F,0x28},//
		{0x0680,0x00},//
		{0x0681,0x22},//
		{0x0682,0x00},//
		{0x0683,0xA5},//
		{0x0684,0x00},//
		{0x0685,0x1E},//
		{0x0686,0x04},//
		{0x0687,0x1D},//
		{0x0688,0x04},//
		{0x0689,0x19},//
		{0x068A,0x04},//
		{0x068B,0x21},//
		{0x068C,0x04},//
		{0x068D,0x0A},//
		{0x068E,0x04},//
		{0x068F,0x25},//
		{0x0690,0x04},//
		{0x0691,0x15},//
		{0x0698,0x20},//
		{0x0699,0x20},//
		{0x069A,0x01},//
		{0x069C,0x22},//
		{0x069D,0x10},//
		{0x069E,0x10},//
		{0x069F,0x08},//
		{0x0000,0x01},//
		{0x0100,0x01},//
		{0x0101,0x01},//
		{0x0005,0x01},//	; Turn on rolling shutter
		{HM2058_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list hm2058_fmt_yuv422[] = {
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

#define N_UXGA_REGS (ARRAY_SIZE(hm2058_win_UXGA))

static struct regval_list hm2058_win_UXGA[] = {
	{0x0006,0x00},//0x0e

	{0x000D,0x00},
	{0x000E,0x00},
	{0x011F ,0x88},
	{0x0125 ,0xDF},
	{0x0126 ,0x70},
	{0x05E4, 0x0A},
	{0x05E5, 0x00},
	{0x05E6, 0x49},
	{0x05E7, 0x06},
	{0x05E8, 0x0A},
	{0x05E9, 0x00},
	{0x05EA, 0xB9},
	{0x05EB, 0x04},
	{0x0000, 0x01},
	{0x0100, 0x01},
	{0x0101, 0x01},

};

#define N_VGA_REGS (ARRAY_SIZE(hm2058_win_VGA))

static struct regval_list hm2058_win_VGA[] = {
	{0x0006,0x00},//0x0e

	{0x000D,0x11},
	{0x000E,0x11},
	{0x011F ,0x80},
	{0x0125 ,0xFF},
	{0x0126 ,0x70},
	{0x0131 ,0xAD},
	{0x0366 ,0x08},
	{0x0433 ,0x10},
	{0x0435 ,0x14},
	{0x05E0, 0xA0},
	{0x05E1, 0x00},
	{0x05E2, 0xA0},
	{0x05E3, 0x00},
	{0x05E4, 0x04},
	{0x05E5, 0x00},
	{0x05E6, 0x83},
	{0x05E7, 0x02},
	{0x05E8, 0x06},
	{0x05E9, 0x00},
	{0x05EA, 0xE5},
	{0x05EB, 0x01},


	{0x0000, 0x01},
	{0x0100, 0x01},
	{0x0101, 0x01},

	{HM2058_REG_END, 0x00},

};



static struct regval_list hm2058_stream_on[] = {
	//Stream on
	{HM2058_REG_DELAY,150},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_stream_off[] = {
	//Stream off
	{HM2058_REG_DELAY,150},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list hm2058_brightness_l3[] = {
	{0x04c0, 0xb0},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_brightness_l2[] = {
	{0x04c0, 0xa0},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_brightness_l1[] = {
	{0x04c0, 0x90},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_brightness_h0[] = {
	{0x04c0, 0x00},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_brightness_h1[] = {
	{0x04c0, 0x10},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_brightness_h2[] = {
	{0x04c0, 0x20},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_brightness_h3[] = {
	{0x04c0, 0x30},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_contrast_l3[] = {
	{0x04b0, 0x10},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_contrast_l2[] = {
	{0x04b0, 0x20},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_contrast_l1[] = {
	{0x04b0, 0x30},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_contrast_h0[] = {
		{0x04b0, 0x40},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};
	
static struct regval_list hm2058_contrast_h1[] = {
	{0x04b0, 0x50},

	{HM2058_REG_END, 0x00},	/* END MARKER */
};
	
static struct regval_list hm2058_contrast_h2[] = {
	{0x04b0, 0x60},

	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list hm2058_contrast_h3[] = {
	{0x04b0, 0x70},
	{HM2058_REG_END, 0x00},	/* END MARKER */
};

static int hm2058_read(struct v4l2_subdev *sd, unsigned short reg,
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

static int hm2058_write(struct v4l2_subdev *sd, unsigned short reg,
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

static int hm2058_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret=0;
	while (vals->reg_num != HM2058_REG_END) {
		if (vals->reg_num == HM2058_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		}
		ret= hm2058_write(sd, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

#ifdef HM2058_DEBUG_FUNC
static int hm2058_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char tmpv;
/
	while (vals->reg_num != HM2058_REG_END) {
			if (vals->reg_num == HM2058_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {

			ret = hm2058_read(sd, vals->reg_num, &tmpv);
			if (ret < 0)
				return ret;
			printk("reg=0x%2x, val=0x%2x\n",vals->reg_num, tmpv );
		}
		vals++;
	}
	return 0;
}
#endif


static int hm2058_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int hm2058_init(struct v4l2_subdev *sd, u32 val)
{
	struct hm2058_info *info = container_of(sd, struct hm2058_info, sd);

	info->fmt = NULL;
	info->win = NULL;
	info->brightness = 0;
	info->contrast = 0;
	info->frame_rate = 0;
	info->hm2058_open = 1;

	return hm2058_write_array(sd, hm2058_init_regs);
}

static int hm2058_shutter(struct v4l2_subdev *sd)
{
	int ret;
	unsigned char v1, v2 ;
	unsigned short v;

	ret = hm2058_read(sd, 0x17c, &v1);
	if (ret < 0)
		return ret;
	ret = hm2058_read(sd, 0x17d, &v2);
	if (ret < 0)
		return ret;

	v =  (v1<<8) | v2	 ;
		v = v / 2;
	hm2058_write(sd, 0x17c, (v >> 8)&0xff ); /* Shutter */
	hm2058_write(sd, 0x17d, v&0xff);
  //     msleep(400);
	return  0;
}

static int hm2058_detect(struct v4l2_subdev *sd)
{
	unsigned char v=0x0;
	int ret;

	ret = hm2058_read(sd, 0x0001, &v);
	printk("HM2058_CHIP_ID_H = %2x\n",v);
	if (ret < 0)
		return ret;
	if (v != HM2058_CHIP_ID_H)
		return -ENODEV;
	ret = hm2058_read(sd, 0x0002, &v);
	printk("HM2058_CHIP_ID_L = %2x\n",v);
	if (ret < 0)
		return ret;
	if (v != HM2058_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static struct hm2058_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;
} hm2058_formats[] = {
	{
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.regs 		= hm2058_fmt_yuv422,
	},
};
#define N_HM2058_FMTS ARRAY_SIZE(hm2058_formats)

static struct hm2058_win_size {
	int	width;
	int	height;
	struct regval_list *regs;
} hm2058_win_sizes[] = {



	{
		.width		= VGA_WIDTH,
		.height		= VGA_HEIGHT,
		.regs 		= hm2058_win_VGA,
	},


	/* UXGA */

	{
		.width		= UXGA_WIDTH,
		.height		= UXGA_HEIGHT,
		.regs 		= hm2058_win_UXGA,
	},




};
#define N_WIN_SIZES (ARRAY_SIZE(hm2058_win_sizes))

static int hm2058_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_HM2058_FMTS)
		return -EINVAL;

	*code = hm2058_formats[index].mbus_code;
	return 0;
}

static int hm2058_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct hm2058_format_struct **ret_fmt,
		struct hm2058_win_size **ret_wsize)
{
	int index;
	struct hm2058_win_size *wsize;

	for (index = 0; index < N_HM2058_FMTS; index++)
		if (hm2058_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_HM2058_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = hm2058_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = hm2058_formats + index;

	fmt->field = V4L2_FIELD_NONE;

	for (wsize = hm2058_win_sizes; wsize < hm2058_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width <= wsize->width && fmt->height <= wsize->height)
			break;

	if (wsize >= hm2058_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the biggest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->colorspace = hm2058_formats[index].colorspace;

	return 0;
}

static int hm2058_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return hm2058_try_fmt_internal(sd, fmt, NULL, NULL);
}

static int hm2058_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct hm2058_info *info = container_of(sd, struct hm2058_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct hm2058_format_struct *fmt_s;
	struct hm2058_win_size *wsize;
	int ret,reg_index;

	ret = hm2058_try_fmt_internal(sd, fmt, &fmt_s, &wsize);
	if (ret)
		return ret;

	if ((info->fmt != fmt_s) && fmt_s->regs) {
		ret = hm2058_write_array(sd, fmt_s->regs);
		if (ret)
			return ret;
	}
//	ret = hm2058_s_stream(sd, 0);
//	if(ret)
//		return ret;
	if ((info->win != wsize) && wsize->regs) {
		data->flags = V4L2_I2C_ADDR_16BIT;
		data->slave_addr = client->addr;

		if (hm2058_win_UXGA == wsize->regs){
			for(reg_index = 0;reg_index < N_UXGA_REGS; reg_index++){
				data->reg[reg_index].addr = hm2058_win_UXGA[reg_index].reg_num;
				data->reg[reg_index].data = hm2058_win_UXGA[reg_index].value;
			}
			data->reg_num = N_UXGA_REGS;
			if(!info->hm2058_open)
				msleep(40);
			else
				info->hm2058_open = 0;
			}

		else if(hm2058_win_VGA == wsize->regs) {
			printk("IN vga\n");
			for(reg_index = 0;reg_index < N_VGA_REGS; reg_index++){
				data->reg[reg_index].addr = hm2058_win_VGA[reg_index].reg_num;
				data->reg[reg_index].data = hm2058_win_VGA[reg_index].value;
			}
			data->reg_num = N_VGA_REGS;
		}

	}
//	ret = hm2058_s_stream(sd, 1);
//	if(ret)
//		return ret;
	info->fmt = fmt_s;
	info->win = wsize;

	return 0;
}

static int hm2058_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable){
		ret = hm2058_write_array(sd, hm2058_stream_on);
		printk("Enable=%d,write_stream_on_array\n",enable);
	}
	else {
		ret = hm2058_write_array(sd, hm2058_stream_off);
		printk("Enable=%d,write_stream_off_array\n",enable);
	}
#ifdef HM2058_DEBUG_FUNC
	hm2058_read_array(sd,hm2058_init_regs );//debug only
#endif

	return ret;
}

static int hm2058_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int hm2058_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
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

static int hm2058_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int hm2058_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int hm2058_frame_rates[] = { 30, 15, 10, 5, 1 };

static int hm2058_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(hm2058_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = hm2058_frame_rates[interval->index];
	return 0;
}

static int hm2058_enum_framesizes(struct v4l2_subdev *sd,
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
		struct hm2058_win_size *win = &hm2058_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int hm2058_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int hm2058_set_brightness(struct v4l2_subdev *sd, int brightness)
{
	printk("[hm2058]set brightness %d\n", brightness);

	switch (brightness) {
	case BRIGHTNESS_L3:
		hm2058_write_array(sd, hm2058_brightness_l3);
		break;
	case BRIGHTNESS_L2:
		hm2058_write_array(sd, hm2058_brightness_l2);
		break;
	case BRIGHTNESS_L1:
		hm2058_write_array(sd, hm2058_brightness_l1);
		break;
	case BRIGHTNESS_H0:
		hm2058_write_array(sd, hm2058_brightness_h0);
		break;
	case BRIGHTNESS_H1:
		hm2058_write_array(sd, hm2058_brightness_h1);
		break;
	case BRIGHTNESS_H2:
		hm2058_write_array(sd, hm2058_brightness_h2);
		break;
	case BRIGHTNESS_H3:
		hm2058_write_array(sd, hm2058_brightness_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int hm2058_set_contrast(struct v4l2_subdev *sd, int contrast)
{
	printk("[hm2058]set contrast %d\n", contrast);

	switch (contrast) {
	case CONTRAST_L3:
		hm2058_write_array(sd, hm2058_contrast_l3);
		break;
	case CONTRAST_L2:
		hm2058_write_array(sd, hm2058_contrast_l2);
		break;
	case CONTRAST_L1:
		hm2058_write_array(sd, hm2058_contrast_l1);
		break;
	case CONTRAST_H0:
		hm2058_write_array(sd, hm2058_contrast_h0);
		break;
	case CONTRAST_H1:
		hm2058_write_array(sd, hm2058_contrast_h1);
		break;
	case CONTRAST_H2:
		hm2058_write_array(sd, hm2058_contrast_h2);
		break;
	case CONTRAST_H3:
		hm2058_write_array(sd, hm2058_contrast_h3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int hm2058_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hm2058_info *info = container_of(sd, struct hm2058_info, sd);
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
		isp_setting.setting = (struct v4l2_isp_regval *)&hm2058_isp_setting;
		isp_setting.size = ARRAY_SIZE(hm2058_isp_setting);
		memcpy((void *)ctrl->value, (void *)&isp_setting, sizeof(isp_setting));
		break;
	case V4L2_CID_ISP_PARM:
		ctrl->value = (int)&hm2058_isp_parm;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int hm2058_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hm2058_info *info = container_of(sd, struct hm2058_info, sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = hm2058_set_brightness(sd, ctrl->value);
		if (!ret)
			info->brightness = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ret = hm2058_set_contrast(sd, ctrl->value);
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

static int hm2058_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_HM2058, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int hm2058_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = hm2058_read(sd, reg->reg & 0xff, &val);
	reg->val = val;
	reg->size = 1;
	return ret;
}

static int hm2058_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	hm2058_write(sd, reg->reg & 0xff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops hm2058_core_ops = {
	.g_chip_ident = hm2058_g_chip_ident,
	.g_ctrl = hm2058_g_ctrl,
	.s_ctrl = hm2058_s_ctrl,
	.queryctrl = hm2058_queryctrl,
	.reset = hm2058_reset,
	.init = hm2058_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = hm2058_g_register,
	.s_register = hm2058_s_register,
#endif
};

static const struct v4l2_subdev_video_ops hm2058_video_ops = {
	.enum_mbus_fmt = hm2058_enum_mbus_fmt,
	.try_mbus_fmt = hm2058_try_mbus_fmt,
	.s_mbus_fmt = hm2058_s_mbus_fmt,
	.s_stream = hm2058_s_stream,
	.cropcap = hm2058_cropcap,
	.g_crop	= hm2058_g_crop,
	.s_parm = hm2058_s_parm,
	.g_parm = hm2058_g_parm,
	.enum_frameintervals = hm2058_enum_frameintervals,
	.enum_framesizes = hm2058_enum_framesizes,
};

static const struct v4l2_subdev_ops hm2058_ops = {
	.core = &hm2058_core_ops,
	.video = &hm2058_video_ops,
};

static int hm2058_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct hm2058_info *info;
	int ret;

	info = kzalloc(sizeof(struct hm2058_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &hm2058_ops);

	/* Make sure it's an hm2058 */
	ret = hm2058_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an hm2058 chip.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "hm2058 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	return 0;
}

static int hm2058_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct hm2058_info *info = container_of(sd, struct hm2058_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id hm2058_id[] = {
	{"hm2058", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hm2058_id);

static struct i2c_driver hm2058_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "hm2058",
	},
	.probe		= hm2058_probe,
	.remove		= hm2058_remove,
	.id_table	= hm2058_id,
};

static __init int init_hm2058(void)
{
	return i2c_add_driver(&hm2058_driver);
}

static __exit void exit_hm2058(void)
{
	i2c_del_driver(&hm2058_driver);
}

fs_initcall(init_hm2058);
module_exit(exit_hm2058);

MODULE_DESCRIPTION("A low-level driver for Himax hm2058 sensors");
MODULE_LICENSE("GPL");
