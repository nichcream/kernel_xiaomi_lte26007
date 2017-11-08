
#include "isp2-ctrl.h"
#include "camera-debug.h"

#define ISP_CTRL_DEBUG
#ifdef ISP_CTRL_DEBUG
#define ISP_CTRL_PRINT(fmt, args...) printk(KERN_DEBUG "[ISP_CTRL]" fmt, ##args)
#else
#define ISP_CTRL_PRINT(fmt, args...)
#endif
#define ISP_CTRL_ERR(fmt, args...) printk(KERN_ERR "[ISP_CTRL]" fmt, ##args)

#define ISP_WRITEB_ARRAY(vals)	\
	isp_writeb_array(isp, vals)


static struct isp_regb_vals isp_exposure_l2[] = {
	{0x1c146, 0x11},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_l1[] = {
	{0x1c146, 0x22},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_h0[] = {
	{0x1c146, 0x44},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_h1[] = {
	{0x1c146, 0x88},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_h2[] = {
	{0x1c146, 0xf0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_iso_100[] = {
	{0x1c150, 0x00}, //max gain
	{0x1c151, 0x14}, //max gain
	{0x1c154, 0x00}, //max gain
	{0x1c155, 0x10}, //min gain
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_iso_200[] = {
	{0x1c150, 0x00},
	{0x1c151, 0x23},
	{0x1c154, 0x00},
	{0x1c155, 0x1d},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_iso_400[] = {
	{0x1c150, 0x00},
	{0x1c151, 0x44},
	{0x1c154, 0x00},
	{0x1c155, 0x3c},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_iso_800[] = {
	{0x1c150, 0x00},
	{0x1c151, 0x80},
	{0x1c154, 0x00},
	{0x1c155, 0x70},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_l3[] = {
	{0x67200, 0x00},
	{0x67201, 0x48},
	{0x67202, 0x00},
	{0x67203, 0x87},
	{0x67204, 0x00},
	{0x67205, 0xDD},
	{0x67206, 0x01},
	{0x67207, 0x1E},

	{0x67208, 0x01},
	{0x67209, 0x54},
	{0x6720A, 0x01},
	{0x6720B, 0x83}, //387
	{0x6720C, 0x01},
	{0x6720D, 0xAD},
	{0x6720E, 0x01},
	{0x6720F, 0xD3},
	{0x67210, 0x01},
	{0x67211, 0xF6},
	{0x67212, 0x02},
	{0x67213, 0x17}, //535

	{0x67214, 0x02},
	{0x67215, 0x36},
	{0x67216, 0x02},
	{0x67217, 0x53},
	{0x67218, 0x02},
	{0x67219, 0x6F}, //623
	{0x6721A, 0x02},
	{0x6721B, 0x8A},
	{0x6721C, 0x02},
	{0x6721D, 0xA3},
	{0x6721E, 0x02},
	{0x6721F, 0xBB},
	{0x67220, 0x02},
	{0x67221, 0xD3},
	{0x67222, 0x02},
	{0x67223, 0xEA}, //746
	{0x67224, 0x03},
	{0x67225, 0x00},
	{0x67226, 0x03},
	{0x67227, 0x15},
	{0x67228, 0x03},
	{0x67229, 0x2A}, //810
	{0x6722A, 0x03},
	{0x6722B, 0x3E},
	{0x6722C, 0x03},
	{0x6722D, 0x51},
	{0x6722E, 0x03},
	{0x6722F, 0x65}, //869
	{0x67230, 0x03},
	{0x67231, 0x77},
	{0x67232, 0x03},
	{0x67233, 0x89},
	{0x67234, 0x03},
	{0x67235, 0x9B}, //923
	{0x67236, 0x03},
	{0x67237, 0xAD},
	{0x67238, 0x03},
	{0x67239, 0xBE},
	{0x6723A, 0x03},
	{0x6723B, 0xCF},
	{0x6723C, 0x03},
	{0x6723D, 0xDF},
	{0x6723E, 0x03},
	{0x6723F, 0xEF},
	{0x67240, 0x03},
	{0x67241, 0xFF},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_l2[] = {
	{0x67200, 0x00},
	{0x67201, 0x3C},
	{0x67202, 0x00},
	{0x67203, 0x75},
	{0x67204, 0x00},
	{0x67205, 0xC9}, //201
	{0x67206, 0x01},
	{0x67207, 0x0F},
	{0x67208, 0x01},
	{0x67209, 0x4F}, //335
	{0x6720A, 0x01},
	{0x6720B, 0x8A}, //394

	{0x6720C, 0x01},
	{0x6720D, 0xBE},
	{0x6720E, 0x01},
	{0x6720F, 0xEB}, //491
	{0x67210, 0x02},
	{0x67211, 0x15},
	{0x67212, 0x02},
	{0x67213, 0x3B}, //571

	{0x67214, 0x02},
	{0x67215, 0x5E}, //606
	{0x67216, 0x02},
	{0x67217, 0x7D},
	{0x67218, 0x02},
	{0x67219, 0x99}, //665
	{0x6721A, 0x02},
	{0x6721B, 0xB4},
	{0x6721C, 0x02},
	{0x6721D, 0xCE},
	{0x6721E, 0x02},
	{0x6721F, 0xE6}, //742
	{0x67220, 0x02},
	{0x67221, 0xFD},
	{0x67222, 0x03},
	{0x67223, 0x12}, //786
	{0x67224, 0x03},
	{0x67225, 0x26},
	{0x67226, 0x03},
	{0x67227, 0x39},
	{0x67228, 0x03},
	{0x67229, 0x4B}, //843

	{0x6722A, 0x03},
	{0x6722B, 0x5D},
	{0x6722C, 0x03},
	{0x6722D, 0x6E},
	{0x6722E, 0x03},
	{0x6722F, 0x7F}, //895
	{0x67230, 0x03},
	{0x67231, 0x8F},
	{0x67232, 0x03},
	{0x67233, 0x9E}, //926
	{0x67234, 0x03},
	{0x67235, 0xAD}, //941
	{0x67236, 0x03},
	{0x67237, 0xBC}, //956
	{0x67238, 0x03},
	{0x67239, 0xCA},
	{0x6723A, 0x03},
	{0x6723B, 0xD8},
	{0x6723C, 0x03},
	{0x6723D, 0xE5},
	{0x6723E, 0x03},
	{0x6723F, 0xF2},
	{0x67240, 0x03},
	{0x67241, 0xFF},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_l1[] = {
	{0x67200, 0x00},
	{0x67201, 0x31},
	{0x67202, 0x00},
	{0x67203, 0x63},
	{0x67204, 0x00},
	{0x67205, 0xB6}, //182
	{0x67206, 0x01},
	{0x67207, 0x00},
	{0x67208, 0x01},
	{0x67209, 0x4A}, //330
	{0x6720A, 0x01},
	{0x6720B, 0x92}, //402

	{0x6720C, 0x01},
	{0x6720D, 0xCF},
	{0x6720E, 0x02},
	{0x6720F, 0x04}, //516
	{0x67210, 0x02},
	{0x67211, 0x34},
	{0x67212, 0x02},
	{0x67213, 0x5F}, //607

	{0x67214, 0x02},
	{0x67215, 0x85}, //645
	{0x67216, 0x02},
	{0x67217, 0xA7},
	{0x67218, 0x02},
	{0x67219, 0xC4}, //708
	{0x6721A, 0x02},
	{0x6721B, 0xDE}, //734
	{0x6721C, 0x02},
	{0x6721D, 0xF8},
	{0x6721E, 0x03},
	{0x6721F, 0x11}, //785
	{0x67220, 0x03},
	{0x67221, 0x26},
	{0x67222, 0x03},
	{0x67223, 0x3A}, //826
	{0x67224, 0x03},
	{0x67225, 0x4C},
	{0x67226, 0x03},
	{0x67227, 0x5D},
	{0x67228, 0x03},
	{0x67229, 0x6D}, //877

	{0x6722A, 0x03},
	{0x6722B, 0x7C},
	{0x6722C, 0x03},
	{0x6722D, 0x8B},
	{0x6722E, 0x03},
	{0x6722F, 0x99}, //921
	{0x67230, 0x03},
	{0x67231, 0xA7},
	{0x67232, 0x03},
	{0x67233, 0xB3}, //947
	{0x67234, 0x03},
	{0x67235, 0xC0}, //960
	{0x67236, 0x03},
	{0x67237, 0xCB}, //971
	{0x67238, 0x03},
	{0x67239, 0xD7},
	{0x6723A, 0x03},
	{0x6723B, 0xE2},
	{0x6723C, 0x03},
	{0x6723D, 0xEC},
	{0x6723E, 0x03},
	{0x6723F, 0xF6},
	{0x67240, 0x03},
	{0x67241, 0xFF},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h0[] = {
	{0x67200, 0x00},
	{0x67201, 0x25},
	{0x67202, 0x00},
	{0x67203, 0x52},
	{0x67204, 0x00},
	{0x67205, 0xA2}, //162
	{0x67206, 0x00},
	{0x67207, 0xF1},
	{0x67208, 0x01},
	{0x67209, 0x46}, //326
	{0x6720A, 0x01},
	{0x6720B, 0x99}, //409

	{0x6720C, 0x01},
	{0x6720D, 0xE0},
	{0x6720E, 0x02},
	{0x6720F, 0x1C}, //540
	{0x67210, 0x02},
	{0x67211, 0x54},
	{0x67212, 0x02},
	{0x67213, 0x82}, //642

	{0x67214, 0x02},
	{0x67215, 0xAD}, //685
	{0x67216, 0x02},
	{0x67217, 0xD0},
	{0x67218, 0x02},
	{0x67219, 0xEE}, //750
	{0x6721A, 0x03},
	{0x6721B, 0x09}, //777
	{0x6721C, 0x03},
	{0x6721D, 0x23},
	{0x6721E, 0x03},
	{0x6721F, 0x3B}, //827
	{0x67220, 0x03},
	{0x67221, 0x50},
	{0x67222, 0x03},
	{0x67223, 0x62}, //866
	{0x67224, 0x03},
	{0x67225, 0x72},
	{0x67226, 0x03},
	{0x67227, 0x81},
	{0x67228, 0x03},
	{0x67229, 0x8F}, //911

	{0x6722A, 0x03},
	{0x6722B, 0x9C},
	{0x6722C, 0x03},
	{0x6722D, 0xA8},
	{0x6722E, 0x03},
	{0x6722F, 0xB3}, //947
	{0x67230, 0x03},
	{0x67231, 0xBE},
	{0x67232, 0x03},
	{0x67233, 0xC8}, //968
	{0x67234, 0x03},
	{0x67235, 0xD2}, //978
	{0x67236, 0x03},
	{0x67237, 0xDB}, //987
	{0x67238, 0x03},
	{0x67239, 0xE3},
	{0x6723A, 0x03},
	{0x6723B, 0xEB},
	{0x6723C, 0x03},
	{0x6723D, 0xF2},
	{0x6723E, 0x03},
	{0x6723F, 0xF9},
	{0x67240, 0x03},
	{0x67241, 0xFF},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h1[] = {
	{0x67200, 0x00},
	{0x67201, 0x1D},
	{0x67202, 0x00},
	{0x67203, 0x46},
	{0x67204, 0x00},
	{0x67205, 0x95}, //149
	{0x67206, 0x00},
	{0x67207, 0xE7},
	{0x67208, 0x01},
	{0x67209, 0x43}, //323
	{0x6720A, 0x01},
	{0x6720B, 0x9E}, //414

	{0x6720C, 0x01},
	{0x6720D, 0xEB},
	{0x6720E, 0x02},
	{0x6720F, 0x2D}, //557
	{0x67210, 0x02},
	{0x67211, 0x68},
	{0x67212, 0x02},
	{0x67213, 0x9A}, //666

	{0x67214, 0x02},
	{0x67215, 0xC7}, //711
	{0x67216, 0x02},
	{0x67217, 0xEC},
	{0x67218, 0x03},
	{0x67219, 0x0A}, //778
	{0x6721A, 0x03},
	{0x6721B, 0x25}, //805
	{0x6721C, 0x03},
	{0x6721D, 0x3F},
	{0x6721E, 0x03},
	{0x6721F, 0x57}, //855
	{0x67220, 0x03},
	{0x67221, 0x6C},
	{0x67222, 0x03},
	{0x67223, 0x7D}, //893
	{0x67224, 0x03},
	{0x67225, 0x8C},
	{0x67226, 0x03},
	{0x67227, 0x99},
	{0x67228, 0x03},
	{0x67229, 0xA5}, //933

	{0x6722A, 0x03},
	{0x6722B, 0xB1},
	{0x6722C, 0x03},
	{0x6722D, 0x8B},
	{0x6722E, 0x03},
	{0x6722F, 0xC5}, //965
	{0x67230, 0x03},
	{0x67231, 0xCE},
	{0x67232, 0x03},
	{0x67233, 0xD6}, //982
	{0x67234, 0x03},
	{0x67235, 0xDE}, //990
	{0x67236, 0x03},
	{0x67237, 0xE5}, //997
	{0x67238, 0x03},
	{0x67239, 0xEC},
	{0x6723A, 0x03},
	{0x6723B, 0xF1},
	{0x6723C, 0x03},
	{0x6723D, 0xF6},
	{0x6723E, 0x03},
	{0x6723F, 0xFB},
	{0x67240, 0x03},
	{0x67241, 0xFF},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h2[] = {
	{0x67200, 0x00},
	{0x67201, 0x16},
	{0x67202, 0x00},
	{0x67203, 0x3A},
	{0x67204, 0x00},
	{0x67205, 0x87}, 
	{0x67206, 0x00},
	{0x67207, 0xDE},
	{0x67208, 0x01},
	{0x67209, 0x40}, 
	{0x6720A, 0x01},
	{0x6720B, 0xA3}, 

	{0x6720C, 0x01},
	{0x6720D, 0xF7},
	{0x6720E, 0x02},
	{0x6720F, 0x3D}, 
	{0x67210, 0x02},
	{0x67211, 0x7D},
	{0x67212, 0x02},
	{0x67213, 0xB2}, 

	{0x67214, 0x02},
	{0x67215, 0xE2}, 
	{0x67216, 0x03},
	{0x67217, 0x08},
	{0x67218, 0x03},
	{0x67219, 0x26}, 
	{0x6721A, 0x03},
	{0x6721B, 0x41}, 
	{0x6721C, 0x03},
	{0x6721D, 0x5C},
	{0x6721E, 0x03},
	{0x6721F, 0x74}, 
	{0x67220, 0x03},
	{0x67221, 0x88},
	{0x67222, 0x03},
	{0x67223, 0x98}, 
	{0x67224, 0x03},
	{0x67225, 0xA5},
	{0x67226, 0x03},
	{0x67227, 0xB1},
	{0x67228, 0x03},
	{0x67229, 0xBC}, 
	{0x6722A, 0x03},
	{0x6722B, 0xC5},
	{0x6722C, 0x03},
	{0x6722D, 0xCE},
	{0x6722E, 0x03},
	{0x6722F, 0xD6}, 
	{0x67230, 0x03},
	{0x67231, 0xDE},
	{0x67232, 0x03},
	{0x67233, 0xE4}, 
	{0x67234, 0x03},
	{0x67235, 0xEA}, 
	{0x67236, 0x03},
	{0x67237, 0xEF}, 
	{0x67238, 0x03},
	{0x67239, 0xF4},
	{0x6723A, 0x03},
	{0x6723B, 0xF8},
	{0x6723C, 0x03},
	{0x6723D, 0xFB},
	{0x6723E, 0x03},
	{0x6723F, 0xFD},
	{0x67240, 0x03},
	{0x67241, 0xFF},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h3[] = {
	{0x67200, 0x00},
	{0x67201, 0x0E},
	{0x67202, 0x00},
	{0x67203, 0x2E},
	{0x67204, 0x00},
	{0x67205, 0x7A}, 
	{0x67206, 0x00},
	{0x67207, 0xD4},
	{0x67208, 0x01},
	{0x67209, 0x3C}, 
	{0x6720A, 0x01},
	{0x6720B, 0xA8}, 

	{0x6720C, 0x02},
	{0x6720D, 0x02},
	{0x6720E, 0x02},
	{0x6720F, 0x4E}, 
	{0x67210, 0x02},
	{0x67211, 0x92},
	{0x67212, 0x02},
	{0x67213, 0xCA},

	{0x67214, 0x02},
	{0x67215, 0xFC}, 
	{0x67216, 0x03},
	{0x67217, 0x24},
	{0x67218, 0x03},
	{0x67219, 0x42}, 
	{0x6721A, 0x03},
	{0x6721B, 0x5E}, 
	{0x6721C, 0x03},
	{0x6721D, 0x78},
	{0x6721E, 0x03},
	{0x6721F, 0x90}, 
	{0x67220, 0x03},
	{0x67221, 0xA4},
	{0x67222, 0x03},
	{0x67223, 0xB3}, 
	{0x67224, 0x03},
	{0x67225, 0xBF},
	{0x67226, 0x03},
	{0x67227, 0xC9},
	{0x67228, 0x03},
	{0x67229, 0xD2}, 

	{0x6722A, 0x03},
	{0x6722B, 0xDA},
	{0x6722C, 0x03},
	{0x6722D, 0xE2},
	{0x6722E, 0x03},
	{0x6722F, 0xE8}, 
	{0x67230, 0x03},
	{0x67231, 0xEE},
	{0x67232, 0x03},
	{0x67233, 0xF2}, 
	{0x67234, 0x03},
	{0x67235, 0xF6}, 
	{0x67236, 0x03},
	{0x67237, 0xFA}, 
	{0x67238, 0x03},
	{0x67239, 0xFC},
	{0x6723A, 0x03},
	{0x6723B, 0xFE},
	{0x6723C, 0x03},
	{0x6723D, 0xFF},
	{0x6723E, 0x03},
	{0x6723F, 0xFF},
	{0x67240, 0x03},
	{0x67241, 0xFF},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_saturation_l2[] = {
	{0x1c4eb, 0x60},
	{0x1c4ec, 0x50},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_saturation_l1[] = {
	{0x1c4eb, 0x70},
	{0x1c4ec, 0x60},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_saturation_h0[] = {
	{0x1c4eb, 0x80},
	{0x1c4ec, 0x70},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_saturation_h1[] = {
	{0x1c4eb, 0x90},
	{0x1c4ec, 0x80},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_saturation_h2[] = {
	{0x1c4eb, 0xa0},
	{0x1c4ec, 0x90},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_auto[] = {
	{0x1c190, 0x02}, //enable white balance
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_daylight[] = {
	{0x1c190, 0x03}, //yueding
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_cloudy_daylight[] = {
	{0x1c190, 0x04},
	{ISP_REG_END, 0x00},
};
static struct isp_regb_vals isp_white_balance_incandescent[] = {
	{0x1c190, 0x05},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_fluorescent[] = {
	{0x1c190, 0x06},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_warm_fluorescent[] = {
	{0x1c190, 0x07},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_twilight[] = {
	{0x1c190, 0x08},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_shade[] = {
	{0x1c190, 0x09},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_l3[] = {
	{0x1c1c5, 0x20},
	{0x1c1c6, 0x20},
	{0x1c1c7, 0x20},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_l2[] = {
	{0x1c1c5, 0x40},
	{0x1c1c6, 0x40},
	{0x1c1c7, 0x40},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_l1[] = {
	{0x1c1c5, 0x60},
	{0x1c1c6, 0x60},
	{0x1c1c7, 0x60},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h0[] = {
	{0x1c1c5, 0x80},
	{0x1c1c6, 0x80},
	{0x1c1c7, 0x80},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h1[] = {
	{0x1c1c5, 0xa0},
	{0x1c1c6, 0xa0},
	{0x1c1c7, 0xa0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h2[] = {
	{0x1c1c5, 0xc0},
	{0x1c1c6, 0xc0},
	{0x1c1c7, 0xc0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h3[] = {
	{0x1c1c5, 0xe0},
	{0x1c1c6, 0xe0},
	{0x1c1c7, 0xe0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_l2[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x0b},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_l1[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x10},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_h0[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x18},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_h1[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x20},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_h2[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x30},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_flicker_50hz[] = {
	{0x1c140, 0x01},
	{0x1c13f, 0x02},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_flicker_60hz[] = {
	{0x1c140, 0x01},
	{0x1c13f, 0x01},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_flicker_auto[] = {
	{0x1c140, 0x01},
	{0x1c13f, 0x02},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_flicker_Off[] = {
	{0x1c140, 0x00},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_none[] = {
	{0x65b0c, 0x00},
	{0x65b0d, 0x80},
	{0x65b16, 0x00},
	{0x65b0e, 0x00},
	{0x65b0f, 0x40},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x00},
	
	{0x65b15, 0x40},
	{0x65b17, 0x00},
	{0x65b18, 0x00},
	{0x65b19, 0x00},
	{0x65b1a, 0x00},
	{0x65b1b, 0x04},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_mono[] = {
	{0x65b0c, 0x00},
	{0x65b0d, 0x80},
	{0x65b16, 0x00},
	{0x65b0e, 0x00},
	{0x65b0f, 0x00},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x00},
	
	{0x65b15, 0x00},
	{0x65b17, 0x00},
	{0x65b18, 0x00},
	{0x65b19, 0x00},
	{0x65b1a, 0x00},
	{0x65b1b, 0x04},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_negative[] = {
	{0x65b0c, 0x02},
	{0x65b0d, 0x80},
	{0x65b16, 0xff},
	{0x65b0e, 0x02},
	{0x65b0f, 0x40},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x02},
	
	{0x65b15, 0x40},
	{0x65b17, 0x00},
	{0x65b18, 0x00},
	{0x65b19, 0x00},
	{0x65b1a, 0x00},
	{0x65b1b, 0x84},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_solarize[] = {
	{0x65b0c, 0x02},
	{0x65b0d, 0x80},
	{0x65b16, 0xff},
	{0x65b0e, 0x02},
	{0x65b0f, 0x40},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x02},
	
	{0x65b15, 0x40},
	{0x65b17, 0x00},
	{0x65b18, 0x00},
	{0x65b19, 0x00},
	{0x65b1a, 0x00},
	{0x65b1b, 0x84},
	
	{0x65b0a, 0x20},
	{0x65b00, 0x80},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_sepia[] = {
	{0x65b0c, 0x00},
	{0x65b0d, 0x80},
	{0x65b16, 0x00},
	{0x65b0e, 0x00},
	{0x65b0f, 0x00},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x00},
	
	{0x65b15, 0x00},
	{0x65b17, 0x8f},
	{0x65b18, 0x0e},
	{0x65b19, 0x00},
	{0x65b1a, 0x00},
	{0x65b1b, 0x04},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_posterize[] = {
	{0x65b0c, 0x00},
	{0x65b0d, 0x80},
	{0x65b16, 0x00},
	{0x65b0e, 0x00},
	{0x65b0f, 0x40},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x00},
	
	{0x65b15, 0x40},
	{0x65b17, 0x00},
	{0x65b18, 0x00},
	{0x65b19, 0x05},
	{0x65b1a, 0x00},
	{0x65b1b, 0x04},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_whiteboard[] = {
	{0x65b0c, 0x00},
	{0x65b0d, 0xf0},
	{0x65b16, 0x00},
	{0x65b0e, 0x00},
	{0x65b0f, 0x00},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x00},
	
	{0x65b15, 0x00},
	{0x65b17, 0x00},
	{0x65b18, 0x00},
	{0x65b19, 0x07},
	{0x65b1a, 0x00},
	{0x65b1b, 0x0f},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_blackboard[] = {
	{0x65b0c, 0x02},
	{0x65b0d, 0x80},
	{0x65b16, 0x00},
	{0x65b0e, 0x00},
	{0x65b0f, 0x00},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x00},
	
	{0x65b15, 0x00},
	{0x65b17, 0x00},
	{0x65b18, 0x00},
	{0x65b19, 0x07},
	{0x65b1a, 0x00},
	{0x65b1b, 0xb0},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_effect_aqua[] = {
	{0x65b0c, 0x00},
	{0x65b0d, 0x80},
	{0x65b16, 0x00},
	{0x65b0e, 0x00},
	{0x65b0f, 0x00},
	
	{0x65b10, 0x00},
	{0x65b11, 0x00},
	{0x65b12, 0x00},
	{0x65b13, 0x00},
	{0x65b14, 0x00},
	
	{0x65b15, 0x00},
	{0x65b17, 0x21},
	{0x65b18, 0xd9},
	{0x65b19, 0x00},
	{0x65b1a, 0x00},
	{0x65b1b, 0x04},
	
	{0x65b0a, 0x00},
	{0x65b00, 0x00},
	{0x65b01, 0xff},
	{ISP_REG_END, 0x00},
};

static int default_get_exposure(void **vals, int val)
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

static int defalut_get_iso(void **vals, int val)
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

static int default_get_contrast(void **vals, int val)
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

static int default_get_saturation(void **vals, int val)
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

static int default_get_white_balance(void **vals, int val)
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
		*vals = isp_white_balance_cloudy_daylight;
		return ARRAY_SIZE(isp_white_balance_twilight);
	case WHITE_BALANCE_SHADE:
		*vals = isp_white_balance_shade;
		return ARRAY_SIZE(isp_white_balance_shade);
	default:
		return -EINVAL;
	}

	return 0;
}

static int default_get_brightness(void **vals, int val)
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

static int default_get_sharpness(void **vals, int val)
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

static int default_get_aecgc_win_setting(int width, int height, int meter_mode, void **vals)
{
	return 0;
}

static struct isp_effect_ops default_effect_ops = {
	.get_exposure = default_get_exposure,
	.get_iso = defalut_get_iso,
	.get_contrast = default_get_contrast,
	.get_saturation = default_get_saturation,
	.get_white_balance = default_get_white_balance,
	.get_brightness = default_get_brightness,
	.get_sharpness = default_get_sharpness,
	.get_aecgc_win_setting = default_get_aecgc_win_setting,
};

static void isp_writeb_array(struct isp_device *isp,
	struct isp_regb_vals *vals)
{
	while (vals->reg != ISP_REG_END) {
		isp_reg_writeb(isp, vals->value, vals->reg);
		vals++;
	}
}


void isp_get_func(struct isp_device *isp, struct isp_effect_ops **ops)
{
	*ops = &default_effect_ops;
}
EXPORT_SYMBOL(isp_get_func);

int isp_set_exposure(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_exposure)
		count = effect_ops->get_exposure(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_exposure);

int isp_set_vcm_range(struct isp_device *isp, int focus_mode)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_vcm_range)
		count = effect_ops->get_vcm_range(&vals, focus_mode);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_vcm_range);

int isp_set_contrast(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_contrast)
		count = effect_ops->get_contrast(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_contrast);

int isp_set_saturation(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_saturation)
		count = effect_ops->get_saturation(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_saturation);

static int isp_get_framectrl(struct isp_device *isp)
{
	struct isp_parm *iparm = &isp->parm;
	return (iparm->frame_rate == FRAME_RATE_AUTO) ? 1 : 0;
}

int isp_set_scene(struct isp_device *isp, int val)
{
	u8 Frcntrl_local;
	int Pregaincurrent[3];
	int Pregain_local[3];
	u8 i;

	struct isp_parm *iparm = &isp->parm;


	switch (val) {
		case SCENE_MODE_AUTO:
		case SCENE_MODE_NORMAL:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, iparm->focus_mode);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x02, 0x1c190);
		break;
		case SCENE_MODE_ACTION:
		case SCENE_MODE_STEADYPHOTO:
		case SCENE_MODE_SPORTS:
		case SCENE_MODE_PARTY:
			Frcntrl_local = 0;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x02, 0x1c190);
		break;
		case SCENE_MODE_PORTRAIT:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x88;
			isp_reg_writeb(isp, 0x02, 0x1c190);
		break;
		case SCENE_MODE_LANDSCAPE:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, FOCUS_MODE_INFINITY);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x03, 0x1c190);
		break;
		case SCENE_MODE_NIGHT:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x04, 0x1c190);
		break;
		case SCENE_MODE_NIGHT_PORTRAIT:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x84;
			isp_reg_writeb(isp, 0x04, 0x1c190);
		break;
		case SCENE_MODE_THEATRE:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, FOCUS_MODE_INFINITY);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x06, 0x1c190);
		break;
		case SCENE_MODE_BEACH:
		case SCENE_MODE_SUNSET:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x03, 0x1c190);
		break;
		case SCENE_MODE_SNOW:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			Pregaincurrent[0] = 0x90;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x03, 0x1c190);
		break;
		case SCENE_MODE_FIREWORKS:
			isp_set_vcm_range(isp, FOCUS_MODE_INFINITY);
			Frcntrl_local = 0;
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x04, 0x1c190);
		break;
		case SCENE_MODE_CANDLELIGHT:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			Pregaincurrent[0] = 0x90;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x88;
			isp_reg_writeb(isp, 0x03, 0x1c190);
		break;
		case SCENE_MODE_BARCODE:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, FOCUS_MODE_MACRO);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			isp_reg_writeb(isp, 0x02, 0x1c190);
		break;
		default:
			return -EINVAL;
	}


	isp_reg_writeb(isp, Frcntrl_local, 0x1c13a);

	for(i = 0; i < 3; i++) {
		Pregain_local[i] = 0x20 + isp->parm.brightness * 0x20;
		Pregain_local[i] = (Pregain_local[i] * Pregaincurrent[i] + 0x40) >> 7;
		if (Pregain_local[i] < 0)
			Pregain_local[i] = 0;
		else if (Pregain_local[i] > 225)
			Pregain_local[i] = 255;
		isp_reg_writeb(isp, Pregain_local[i], (0x1c1c5 + i));
	}

	return 0;
}
EXPORT_SYMBOL(isp_set_scene);

int isp_set_effect(struct isp_device *isp, int val)
{
	switch (val) {
	case EFFECT_NONE:
		ISP_WRITEB_ARRAY(isp_effect_none);
		break;
	case EFFECT_MONO:
		ISP_WRITEB_ARRAY(isp_effect_mono);
		break;
	case EFFECT_NEGATIVE:
		ISP_WRITEB_ARRAY(isp_effect_negative);
		break;
	case EFFECT_SOLARIZE:
		ISP_WRITEB_ARRAY(isp_effect_solarize);
		break;
	case EFFECT_SEPIA:
		ISP_WRITEB_ARRAY(isp_effect_sepia);
		break;
	case EFFECT_POSTERIZE:
		ISP_WRITEB_ARRAY(isp_effect_posterize);
		break;
	case EFFECT_WHITEBOARD:
		ISP_WRITEB_ARRAY(isp_effect_whiteboard);
		break;
	case EFFECT_BLACKBOARD:
		ISP_WRITEB_ARRAY(isp_effect_blackboard);
		break;
	case EFFECT_AQUA:
		ISP_WRITEB_ARRAY(isp_effect_aqua);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(isp_set_effect);

int isp_set_iso(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	struct isp_parm *iparm = &isp->parm;
	int ret = 0;
	int count = -1;
	void *vals;

	if (val == ISO_AUTO) {
		if (isp->running) {
			if ((iparm->auto_max_gain > 0) && (iparm->auto_min_gain > 0)) {
				isp_reg_writeb(isp, 0, ISP_GAIN_MAX_H);
				isp_reg_writeb(isp, iparm->auto_max_gain, ISP_GAIN_MAX_L);
				isp_reg_writeb(isp, 0, ISP_GAIN_MIN_H);
				isp_reg_writeb(isp, iparm->auto_min_gain, ISP_GAIN_MIN_L);
				iparm->max_gain = iparm->auto_max_gain;
				iparm->min_gain = iparm->auto_min_gain;

			}else {
				printk(KERN_WARNING "iso param is invalid \n");
			}
			ret = 0;
		}else {
			printk(KERN_WARNING "set iso auto\n");
			ret = 0;
		}
	}else {
		if (effect_ops->get_iso)
			count = effect_ops->get_iso(&vals, val);
		if (vals != NULL)
			isp_writeb_array(isp, (struct isp_regb_vals *)vals);
		else
			ret = -EINVAL;
		iparm->max_gain = isp_reg_readw(isp, ISP_GAIN_MAX_H);
		iparm->min_gain = isp_reg_readw(isp, ISP_GAIN_MIN_H);
	}
	return ret;
}
EXPORT_SYMBOL(isp_set_iso);

int isp_set_white_balance(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_white_balance)
		count = effect_ops->get_white_balance(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_white_balance);

int isp_set_sharpness(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_sharpness)
		count = effect_ops->get_sharpness(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_sharpness);

int isp_set_brightness(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_brightness)
		count = effect_ops->get_brightness(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_brightness);

int isp_set_aecgc_win(struct isp_device *isp, int width, int height, int meter_mode)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals;

	if (effect_ops->get_aecgc_win_setting)
		count = effect_ops->get_aecgc_win_setting(width, height, meter_mode, &vals);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);

	return ret;
}
EXPORT_SYMBOL(isp_set_aecgc_win);

int isp_set_flicker(struct isp_device *isp, int val)
{
	switch (val) {
	case FLICKER_AUTO:
		ISP_WRITEB_ARRAY(isp_flicker_auto);
		break;
	case FLICKER_50Hz:
		ISP_WRITEB_ARRAY(isp_flicker_50hz);
		break;
	case FLICKER_60Hz:
		ISP_WRITEB_ARRAY(isp_flicker_60hz);
		break;
	case FLICKER_OFF:
		ISP_WRITEB_ARRAY(isp_flicker_Off);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(isp_set_flicker);

int isp_check_meter_cap(struct isp_device *isp, int val)
{
	return 0;
}

EXPORT_SYMBOL(isp_check_meter_cap);

int isp_check_focus_cap(struct isp_device *isp, int val)
{
	return 0;
}
EXPORT_SYMBOL(isp_check_focus_cap);

int isp_set_hflip(struct isp_device *isp, int val)
{
	isp->parm.hflip = val;
	return 0;
}
EXPORT_SYMBOL(isp_set_hflip);

int isp_set_vflip(struct isp_device *isp, int val)
{
	isp->parm.vflip = val;
	return 0;
}
EXPORT_SYMBOL(isp_set_vflip);

int isp_set_capture_anti_shake(struct isp_device *isp, int on)
{
	unsigned int max_exp_preset, max_gain_preset, exp_preview;
	unsigned int gain_preview, ratio_convert, exposuret, gaint;
	unsigned int max_exp;
	unsigned long long texp;

	max_exp_preset = isp_reg_readw(isp, ISP_MAX_EXPOSURE);
	max_gain_preset = isp_reg_readw(isp, ISP_MAX_GAIN);

	exp_preview = isp_reg_readw(isp, 0x1c16a);
	gain_preview = isp_reg_readw(isp, 0x1c170);
	ratio_convert = isp_reg_readw(isp, ISP_EXPOSURE_RATIO);

	texp = (unsigned long long)(exp_preview) * gain_preview * ratio_convert;
	texp >>= 8;

	if (on) {
		exposuret = max_exp_preset / isp->anti_shake_parms->a;
		gaint = max_gain_preset;
	}
	else {
		exposuret = max_exp_preset / isp->anti_shake_parms->b;
		gaint = max_gain_preset * isp->anti_shake_parms->c / isp->anti_shake_parms->d;
	}

	if (div_u64(texp, exposuret << 4) < gaint) {
		max_exp = exposuret;
	}
	else {
		max_exp = max_exp_preset;
	}

	isp_reg_writew(isp, max_exp, 0x1e862);
	
	return 0;  	
}

int isp_get_aecgc_stable(struct isp_device *isp, int *stable)
{
	u16 max_gain, cur_gain;
	*stable = isp_reg_readb(isp, 0x1c60c); 	
	max_gain = isp_reg_readb(isp, 0x1c150);
	max_gain <<= 8;
	max_gain += isp_reg_readb(isp, 0x1c151);

	cur_gain = isp_reg_readb(isp, 0x1c170);
	cur_gain <<= 8;
	cur_gain += isp_reg_readb(isp, 0x1c171);

	if (!(*stable)) {
		if (cur_gain == max_gain) {
			*stable = 1;
		}
	}
	return 0;
}

int isp_get_brightness_level(struct isp_device *isp, int *level)
{
	u16 max_gain, cur_gain;
	int aecgc_stable = 0;
	u32 cur_mean, target_low;
	
	max_gain = isp_reg_readb(isp, 0x1c150);
	max_gain <<= 8;
	max_gain += isp_reg_readb(isp, 0x1c151);

	cur_gain = isp_reg_readb(isp, 0x1c170);
	cur_gain <<= 8;
	cur_gain += isp_reg_readb(isp, 0x1c171);

	cur_mean = isp_reg_readb(isp, 0x1c758);
	target_low = isp_reg_readb(isp, 0x1c146);
	
	aecgc_stable = isp_reg_readb(isp, 0x1c60c); 	

	if (max_gain == cur_gain
		&& !aecgc_stable
		&& (cur_mean * 100 < target_low * isp->flash_parms->mean_percent))
		*level = 0;
	else
		*level = 1;
	
	return 0;
}

int isp_get_iso(struct isp_device *isp, int *val)
{
	u16 iso = 0;

	iso = isp_reg_readb(isp, 0x1e912);
	iso <<= 8;
	iso += isp_reg_readb(isp, 0x1e913);

	*val = (int)iso;

	return 0;
}

/*
	return value: 	-2 should down frm use large step
					-1 should down frm use small step
				  	0  framerate should be steady
				  	1  should up frm use small step
				  	2  should up frm use large step
*/
int  isp_get_dynfrt_cond(struct isp_device *isp,
						int down_small_thres,
						int down_large_thres,
						int up_small_thres,
						int up_large_thres)
{
#if 0
	u16 max_gain, cur_gain;
	int aecgc_stable = 0;
	u32 cur_mean, target_low;
	int level = 0;


	max_gain = isp_reg_readw(isp, REG_ISP_MAX_GAIN);
	cur_gain = isp_reg_readw(isp, REG_ISP_PRE_GAIN);

	cur_mean = isp_reg_readb(isp, REG_ISP_MEAN);
	target_low = isp_reg_readb(isp, REG_ISP_TARGET_LOW);

	aecgc_stable = isp_reg_readb(isp, REG_ISP_AECGC_STABLE);
#if 0
	printk("cur_gain=0x%x max_gain=0x%x cur_mean=0x%x target_low=0x%x aecgc_stable=%d min_mean_percent=%d max_mean_percent=%d\n",
		cur_gain, max_gain, cur_mean, target_low, aecgc_stable, min_mean_percent, max_mean_percent);
#endif
	if (max_gain == cur_gain
		&& !aecgc_stable
		&& (cur_mean * 100 < target_low * down_large_thres)) {
		level = -2;
	}
	else if (cur_gain == max_gain
		&& !aecgc_stable
		&& (cur_mean * 100 < target_low * down_small_thres))
		level = -1;
	else if (cur_mean * 100 >= target_low * up_large_thres)
		level = 2;
	else if (cur_mean * 100 >= target_low * up_small_thres)
		level = 1;
	else
		level = 0;

	return level;
#endif

	return 0;
}

int isp_get_pre_gain(struct isp_device *isp, int *val)
{
	u16 gain = 0;
	if (isp->aecgc_control)
		gain = isp_reg_readw(isp, REG_ISP_PRE_GAIN1);
	else
		gain = isp_reg_readw(isp, REG_ISP_PRE_GAIN2);
	*val = gain;

	return 0;
}

int isp_get_pre_exp(struct isp_device *isp, int *val)
{
	u32 exp = 0;

	if (isp->aecgc_control)
		exp = isp_reg_readl(isp, REG_ISP_PRE_EXP1);
	else
		exp = isp_reg_readl(isp, REG_ISP_PRE_EXP2);
	*val = exp >> 4;

	return 0;
}

int isp_get_sna_gain(struct isp_device *isp, int *val)
{
	u16 gain = 0;

	if (isp->aecgc_control)
		return isp_get_pre_gain(isp, val);
	else {
		gain = isp_reg_readw(isp, REG_ISP_SNA_GAIN);
		*val = gain;
	}
	return 0;
}

int isp_get_sna_exp(struct isp_device *isp, int *val)
{
	u16 exp = 0;

	if (isp->aecgc_control)
		return isp_get_pre_exp(isp, val);
	else {
		exp = isp_reg_readw(isp, REG_ISP_SNA_EXP);
		*val = exp;
	}
	return 0;

}

int isp_set_max_exp(struct isp_device *isp, int max_exp)
{
	u32 tmp = (u32)max_exp;

	isp_reg_writel(isp, tmp, REG_ISP_MAX_EXP);
	return 0;
}

int isp_set_vts(struct isp_device *isp, int vts)
{
	u16 tmp = (u16)vts;

	isp_reg_writew(isp, tmp, REG_ISP_VTS);
	return 0;
}

int isp_set_min_gain(struct isp_device *isp, unsigned int min_gain)
{
	u16 tmp = (u16)min_gain;

	isp_reg_writew(isp, tmp, REG_ISP_MIN_GAIN);

	return 0;
}

int isp_set_max_gain(struct isp_device *isp, unsigned int max_gain)
{
	u16 tmp = (u16)max_gain;

	isp_reg_writew(isp, tmp, REG_ISP_MAX_GAIN);

	return 0;
}

int isp_enable_aecgc_writeback(struct isp_device *isp, int enable)
{
	isp_reg_writeb(isp, (u8)enable, REG_ISP_AECGC_WRITEBACK_ENABLE);
	return 0;
}


int isp_set_pre_gain(struct isp_device *isp, unsigned int gain)
{
	u16 tmp = (u16) gain;
	if (isp->aecgc_control)
		isp_reg_writew(isp, tmp, REG_ISP_PRE_GAIN1);
	else
		isp_reg_writew(isp, tmp, REG_ISP_PRE_GAIN2);

	return 0;
}

int isp_set_pre_exp(struct isp_device *isp, unsigned int exp)
{
	u32 tmp = (u32) exp;
	if (isp->aecgc_control)
		isp_reg_writel(isp, tmp, REG_ISP_PRE_EXP1);
	else
		isp_reg_writel(isp, tmp, REG_ISP_PRE_EXP2);

	return 0;
}

int isp_get_banding_step(struct isp_device *isp, int *val)
{
	u16 banding_step = 0;
	int freq = 0;

	freq = isp_reg_readb(isp, REG_ISP_BANDFILTER_FLAG);

	if(freq == 0x01)
		banding_step =isp_reg_readw(isp, ISP_PRE_BANDING_60HZ);
	else if(freq == 0x02)
		banding_step =isp_reg_readw(isp, ISP_PRE_BANDING_50HZ);

	*val = banding_step;

	return 0;
}

int isp_set_aecgc_enable(struct isp_device *isp, int enable)
{
	isp_reg_writeb(isp, (u8)(!enable), REG_ISP_AECGC_MANUAL_ENABLE);
	return 0;
}

