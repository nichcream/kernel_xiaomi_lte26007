
#include "isp-ctrl.h"
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

static struct isp_regb_vals isp_exposure_l3[] = {
	{0x1c146, 0x14},
	{0x1c5a0, 0x14},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_l2[] = {
	{0x1c146, 0x1d},
	{0x1c5a0, 0x1d},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_l1[] = {
	{0x1c146, 0x26},
	{0x1c5a0, 0x26},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_h0[] = {
	{0x1c146, 0x40},
	{0x1c5a0, 0x40},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_h1[] = {
	{0x1c146, 0x5d},
	{0x1c5a0, 0x5d},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_h2[] = {
	{0x1c146, 0x6a},
	{0x1c5a0, 0x6a},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_exposure_h3[] = {
	{0x1c146, 0x78},
	{0x1c5a0, 0x78},
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
	{0x1c4c0, 0x1a},
	{0x1c4c1, 0x2d},
	{0x1c4c2, 0x3c},
	{0x1c4c3, 0x48},
	{0x1c4c4, 0x53},
	{0x1c4c5, 0x5e},
	{0x1c4c6, 0x69},
	{0x1c4c7, 0x74},
	{0x1c4c8, 0x80},
	{0x1c4c9, 0x8d},
	{0x1c4ca, 0x9a},
	{0x1c4cb, 0xa9},
	{0x1c4cc, 0xb9},
	{0x1c4cd, 0xcd},
	{0x1c4ce, 0xe4},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_l2[] = {
	{0x1c4c0, 0x1c},
	{0x1c4c1, 0x2f},
	{0x1c4c2, 0x3e},
	{0x1c4c3, 0x49},
	{0x1c4c4, 0x54},
	{0x1c4c5, 0x5e},
	{0x1c4c6, 0x68},
	{0x1c4c7, 0x73},
	{0x1c4c8, 0x7e},
	{0x1c4c9, 0x8a},
	{0x1c4ca, 0x97},
	{0x1c4cb, 0xa6},
	{0x1c4cc, 0xb6},
	{0x1c4cd, 0xca},
	{0x1c4ce, 0xe2},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_l1[] = {
	{0x1c4c0, 0x1d},
	{0x1c4c1, 0x31},
	{0x1c4c2, 0x40},
	{0x1c4c3, 0x4a},
	{0x1c4c4, 0x54},
	{0x1c4c5, 0x5e},
	{0x1c4c6, 0x67},
	{0x1c4c7, 0x71},
	{0x1c4c8, 0x7c},
	{0x1c4c9, 0x87},
	{0x1c4ca, 0x94},
	{0x1c4cb, 0xa3},
	{0x1c4cc, 0xb3},
	{0x1c4cd, 0xc7},
	{0x1c4ce, 0xe0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h0[] = {
	{0x1c4c0, 0x1f},
	{0x1c4c1, 0x33},
	{0x1c4c2, 0x42},
	{0x1c4c3, 0x4c},
	{0x1c4c4, 0x55},
	{0x1c4c5, 0x5e},
	{0x1c4c6, 0x67},
	{0x1c4c7, 0x70},
	{0x1c4c8, 0x7a},
	{0x1c4c9, 0x85},
	{0x1c4ca, 0x91},
	{0x1c4cb, 0xa0},
	{0x1c4cc, 0xb0},
	{0x1c4cd, 0xc5},
	{0x1c4ce, 0xdf},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h1[] = {
	{0x1c4c0, 0x20},
	{0x1c4c1, 0x34},
	{0x1c4c2, 0x43},
	{0x1c4c3, 0x4c},
	{0x1c4c4, 0x55},
	{0x1c4c5, 0x5d},
	{0x1c4c6, 0x66},
	{0x1c4c7, 0x6e},
	{0x1c4c8, 0x78},
	{0x1c4c9, 0x83},
	{0x1c4ca, 0x8e},
	{0x1c4cb, 0x9d},
	{0x1c4cc, 0xad},
	{0x1c4cd, 0xc3},
	{0x1c4ce, 0xdd},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h2[] = {
	{0x1c4c0, 0x21},
	{0x1c4c1, 0x35},
	{0x1c4c2, 0x44},
	{0x1c4c3, 0x4d},
	{0x1c4c4, 0x55},
	{0x1c4c5, 0x5d},
	{0x1c4c6, 0x65},
	{0x1c4c7, 0x6d},
	{0x1c4c8, 0x76},
	{0x1c4c9, 0x81},
	{0x1c4ca, 0x8c},
	{0x1c4cb, 0x9b},
	{0x1c4cc, 0xab},
	{0x1c4cd, 0xc1},
	{0x1c4ce, 0xdc},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_contrast_h3[] = {
	{0x1c4c0, 0x22},
	{0x1c4c1, 0x36},
	{0x1c4c2, 0x45},
	{0x1c4c3, 0x4e},
	{0x1c4c4, 0x56},
	{0x1c4c5, 0x5d},
	{0x1c4c6, 0x65},
	{0x1c4c7, 0x6c},
	{0x1c4c8, 0x75},
	{0x1c4c9, 0x7f},
	{0x1c4ca, 0x8a},
	{0x1c4cb, 0x99},
	{0x1c4cc, 0xa9},
	{0x1c4cd, 0xbf},
	{0x1c4ce, 0xdb},
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
	{0x66206, 0x0d},
	{0x66207, 0x0d},
	{0x66208, 0x0f},
	{0x66209, 0x71},
	{0x6620a, 0x61},
	{0x6620b, 0xd8},
	{0x6620c, 0xa0},
	{0x6620d, 0x40},
	{0x6620e, 0x34},
	{0x6620f, 0x65},
	{0x66210, 0x55},
	{0x66201, 0x52},

	{0x1c5cd, 0x01},
	{0x1c5ce, 0x00},
	{0x1c5cf, 0xf0},
	{0x1c5d0, 0x01},
	{0x1c5d1, 0x20},
	{0x1c5d2, 0x03},
	{0x1c5d3, 0x00},
	{0x1c5d4, 0x50},
	{0x1c5d5, 0xc0},
	{0x1c5d6, 0x88},
	{0x1c5d7, 0xd8},
	{0x1c5d8, 0x40},
	{0x1c1c2, 0x00},
	{0x1c1c3, 0x20},
	{0x1c17c, 0x01},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_daylight[] = {
	{0x1c4f0, 0x00},
	{0x1c4f1, 0xa4},
	{0x1c4f2, 0x00},
	{0x1c4f3, 0x80},
	{0x1c4f4, 0x00},
	{0x1c4f5, 0xc1},
	{0x1c17c, 0x03},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_cloudy_daylight[] = {
	{0x1c4f6, 0x00},
	{0x1c4f7, 0xb0},
	{0x1c4f8, 0x00},
	{0x1c4f9, 0x80},
	{0x1c4fa, 0x00},
	{0x1c4fb, 0xbd},
	{0x1c17c, 0x04},
	{ISP_REG_END, 0x00},
};
static struct isp_regb_vals isp_white_balance_incandescent[] = {
	{0x1c4fc, 0x01},
	{0x1c4fd, 0x3a},
	{0x1c4fe, 0x00},
	{0x1c4ff, 0x8a},
	{0x1c500, 0x00},
	{0x1c501, 0x80},
	{0x1c17c, 0x05},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_fluorescent[] = {
	{0x1c502, 0x01},
	{0x1c503, 0x12},
	{0x1c504, 0x00},
	{0x1c505, 0x80},
	{0x1c506, 0x00},
	{0x1c507, 0xbb},
	{0x1c17c, 0x06},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_warm_fluorescent[] = {
	{0x1c508, 0x01},
	{0x1c509, 0x02},
	{0x1c50a, 0x00},
	{0x1c50b, 0x80},
	{0x1c50c, 0x00},
	{0x1c50d, 0xa0},
	{0x1c17c, 0x07},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_twilight[] = {
	{0x1c50e, 0x00},
	{0x1c50f, 0xa8},
	{0x1c510, 0x00},
	{0x1c511, 0x80},
	{0x1c512, 0x00},
	{0x1c513, 0xd0},
	{0x1c17c, 0x08},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_white_balance_shade[] = {
	{0x1c514, 0x00},
	{0x1c515, 0x98},
	{0x1c516, 0x00},
	{0x1c517, 0x80},
	{0x1c518, 0x00},
	{0x1c519, 0xc8},
	{0x1c17c, 0x09},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_l3[] = {
	{0x1c5ac, 0x20},
	{0x1c5ad, 0x20},
	{0x1c5ae, 0x20},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_l2[] = {
	{0x1c5ac, 0x40},
	{0x1c5ad, 0x40},
	{0x1c5ae, 0x40},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_l1[] = {
	{0x1c5ac, 0x60},
	{0x1c5ad, 0x60},
	{0x1c5ae, 0x60},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h0[] = {
	{0x1c5ac, 0x80},
	{0x1c5ad, 0x80},
	{0x1c5ae, 0x80},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h1[] = {
	{0x1c5ac, 0xa0},
	{0x1c5ad, 0xa0},
	{0x1c5ae, 0xa0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h2[] = {
	{0x1c5ac, 0xc0},
	{0x1c5ad, 0xc0},
	{0x1c5ae, 0xc0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_brightness_h3[] = {
	{0x1c5ac, 0xe0},
	{0x1c5ad, 0xe0},
	{0x1c5ae, 0xe0},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_l2[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x0b},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_l1[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x06},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_sharpness_h0[] = {
	{0x6560c, 0x00},
	{0x6560d, 0x10},
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
	{0x1c13f, 0x02},
	{ISP_REG_END, 0x00},

};

static struct isp_regb_vals isp_flicker_auto[] = {
	{0x1c140, 0x01},
	{ISP_REG_END, 0x00},
};

static struct isp_regb_vals isp_flicker_Off[] = {
	{0x1c140, 0x00},
	{ISP_REG_END, 0x00},
};

static int default_get_exposure(void **vals, int val)
{
	switch (val) {
	case EXPOSURE_L6:
	case EXPOSURE_L5:
	case EXPOSURE_L4:
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
	case EXPOSURE_H4:
	case EXPOSURE_H5:
	case EXPOSURE_H6:
		*vals = isp_exposure_h3;
		return ARRAY_SIZE(isp_exposure_h3);
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
	case CONTRAST_H4:
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
	case BRIGHTNESS_H4:
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
	void *vals = NULL;

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
	void *vals = NULL;

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
	void *vals = NULL;

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
	void *vals = NULL;

	if (effect_ops->get_saturation)
		count = effect_ops->get_saturation(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_saturation);

int isp_set_saturation_hdr(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals = NULL;

	if (effect_ops->get_saturation_hdr)
		count = effect_ops->get_saturation_hdr(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_saturation_hdr);

static int isp_get_framectrl(struct isp_device *isp)
{
	struct isp_parm *iparm = &isp->parm;
	return (iparm->frame_rate == FRAME_RATE_AUTO) ? 1 : 0;
}


int isp_set_scene(struct isp_device *isp, int val)
{
	u8 i;
	u8 Frcntrl_local;
	int Pregaincurrent[3];
	int Pregain_local[3];
	int min_gain = 0;
	int ae_target_low = 0,ae_target_high = 0;
	int min_saturation= 0,max_saturation = 0;
	struct isp_parm *iparm = &isp->parm;

	switch (val) {
		case SCENE_MODE_AUTO:
		case SCENE_MODE_NORMAL:
			Frcntrl_local = isp_get_framectrl(isp);
			isp_set_vcm_range(isp, iparm->focus_mode);
			isp_set_exposure(isp, iparm->exposure);
			isp_set_saturation(isp, iparm->saturation);
			isp_set_iso(isp, iparm->iso);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
		break;
		case SCENE_MODE_ACTION:
		case SCENE_MODE_STEADYPHOTO:
		case SCENE_MODE_SPORTS:
			Frcntrl_local = 0;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			isp_set_exposure(isp, iparm->exposure);
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x40;
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
			break;
		case SCENE_MODE_PARTY:
			Frcntrl_local = 0;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			isp_set_exposure(isp, iparm->exposure);
			isp_set_saturation(isp, iparm->saturation);
			isp_set_iso(isp, iparm->iso);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
		break;
		case SCENE_MODE_PORTRAIT:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			isp_set_exposure(isp, iparm->exposure);
			max_saturation = 0x73;
			min_saturation = 0x65;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x90;
			Pregaincurrent[1] = 0x84;
			Pregaincurrent[2] = 0x84;
		break;
		case SCENE_MODE_LANDSCAPE:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_INFINITY);
			ae_target_low  = 0x3d;
			ae_target_high = 0x46;
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x84;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
		break;
		case SCENE_MODE_NIGHT:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			isp_set_exposure(isp, iparm->exposure);
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x84;
			Pregaincurrent[2] = 0x84;
		break;
		case SCENE_MODE_NIGHT_PORTRAIT:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			isp_set_exposure(isp, iparm->exposure);
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x9c;
			Pregaincurrent[1] = 0x90;
			Pregaincurrent[2] = 0x90;
		break;
		case SCENE_MODE_THEATRE:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_INFINITY);
			isp_set_exposure(isp, iparm->exposure);
			isp_set_saturation(isp, iparm->saturation);
			isp_set_iso(isp, iparm->iso);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
		break;
		case SCENE_MODE_BEACH:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			ae_target_low  = 0x43;
			ae_target_high = 0x4d;
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x88;
			Pregaincurrent[2] = 0x88;
		break;
		case SCENE_MODE_SUNSET:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			ae_target_low  = 0x1c;
			ae_target_high = 0x20;
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0xc0;
			Pregaincurrent[2] = 0xff;
		break;
		case SCENE_MODE_SNOW:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			ae_target_low  = 0x49;
			ae_target_high = 0x54;
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x90;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
		break;
		case SCENE_MODE_FIREWORKS:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			ae_target_low  = 0x2d;
			ae_target_high = 0x33;
			max_saturation = 0x80;
			min_saturation = 0x70;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0xb0;
			Pregaincurrent[2] = 0xe0;
		break;
		case SCENE_MODE_CANDLELIGHT:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			isp_set_exposure(isp, iparm->exposure);
			isp_set_saturation(isp, iparm->saturation);
			isp_set_iso(isp, iparm->iso);
			Pregaincurrent[0] = 0x90;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x88;
		break;
		case SCENE_MODE_BARCODE:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_MACRO);
			isp_set_exposure(isp, iparm->exposure);
			isp_set_saturation(isp, iparm->saturation);
			isp_set_iso(isp, iparm->iso);
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x80;
			Pregaincurrent[2] = 0x80;
		break;
		case SCENE_MODE_FLOWERS:
			Frcntrl_local = 1;
			isp_set_vcm_range(isp, FOCUS_MODE_AUTO);
			isp_set_exposure(isp, iparm->exposure);
			max_saturation = 0x99;
			min_saturation = 0x86;
			min_gain = 0x10;
			Pregaincurrent[0] = 0x80;
			Pregaincurrent[1] = 0x84;
			Pregaincurrent[2] = 0x84;
		break;
		default:
			return -EINVAL;
	}

	isp_set_dynamic_framerate_enable(isp, Frcntrl_local);

	if(ae_target_low && ae_target_high) {
		isp_reg_writeb(isp, ae_target_low, REG_ISP_TARGET_LOW);
		isp_reg_writeb(isp, ae_target_high, REG_ISP_TARGET_HIGH);
	}
	if(max_saturation && min_saturation){
		isp_reg_writeb(isp, max_saturation, REG_ISP_MAX_SATURATION);
		isp_reg_writeb(isp, min_saturation, REG_ISP_MIN_SATURATION);
	}
	if(min_gain)
		isp_reg_writew(isp, min_gain, REG_ISP_MIN_GAIN);

	for(i = 0; i < 3; i++) {
		if(isp->parm.brightness>=BRIGHTNESS_H0)
			Pregain_local[i]=0x80;
		else
			Pregain_local[i] = 0x20 + isp->parm.brightness * 0x20;

		Pregain_local[i] = (Pregain_local[i] * Pregaincurrent[i] + 0x40) >> 7;

		if (Pregain_local[i] < 0)
			Pregain_local[i] = 0;
		else if (Pregain_local[i] > 225)
			Pregain_local[i] = 255;
		isp_reg_writeb(isp, Pregain_local[i], (0x1c5ac + i));
	}

	return 0;
}
EXPORT_SYMBOL(isp_set_scene);

struct isp_effect {
	u8 sde_enable;
	u8 r_sde_ctrl00;
	u8 u_offset;
	u8 v_offset;
	u8 y_offset;
	u8 color_sel_enable;
	u8 yuv_thre00;
	u8 yuv_thre01;
	u16 ygain;
	u8 sign_bit;
	u8 yoffset;
	u16 uvmatrix00;
	u16 uvmatrix01;
	u16 uvmatrix10;
	u16 uvmatrix11;
	u8 uvoffset0;
	u8 uvoffset1;
	u8 vector_shift;
	u8 ne_enable;
	u8 pe_enable;
	u8 hthre;
	u8 hgain;
};

static struct isp_effect none_effect = {
	.sde_enable = 0x0,
	.r_sde_ctrl00 = 0x0,
	.u_offset = 0x40,
	.v_offset = 0x0,
	.y_offset = 0x80,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x80,
	.sign_bit = 0x0,
	.yoffset = 0x0,
	.uvmatrix00 = 0x40,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x40,
	.uvoffset0 = 0x0,
	.uvoffset1 =0x0,
	.vector_shift =0x0,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x4
};

static struct isp_effect mono_effect = {
	.sde_enable = 0x0,
	.r_sde_ctrl00 = 0x0,
	.u_offset = 0x0,
	.v_offset = 0x0,
	.y_offset = 0x0,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x80,
	.sign_bit = 0x0,
	.yoffset = 0x0,
	.uvmatrix00 = 0x0,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x0,
	.uvoffset0 = 0x0,
	.uvoffset1 =0x0,
	.vector_shift =0x0,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x4
};

static struct isp_effect negative_effect = {
	.sde_enable = 0x0,
	.r_sde_ctrl00 = 0x0,
	.u_offset = 0x0,
	.v_offset = 0x0,
	.y_offset = 0x0,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x280,
	.sign_bit = 0x0,
	.yoffset = 0xff,
	.uvmatrix00 = 0x240,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x240,
	.uvoffset0 = 0x0,
	.uvoffset1 =0x0,
	.vector_shift =0x0,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x84
};

static struct isp_effect solarize_effect = {
	.sde_enable = 0x0,
	.r_sde_ctrl00 = 0x0,
	.u_offset = 0x0,
	.v_offset = 0x0,
	.y_offset = 0x0,
	.color_sel_enable = 0x1,
	.yuv_thre00 = 0x80,
	.yuv_thre01 = 0xff,
	.ygain = 0x280,
	.sign_bit = 0x0,
	.yoffset = 0xff,
	.uvmatrix00 = 0x240,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x240,
	.uvoffset0 = 0x0,
	.uvoffset1 =0x0,
	.vector_shift =0x0,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x84
};

static struct isp_effect sepia_effect = {
	.sde_enable = 0x1,
	.r_sde_ctrl00 = 0x18,
	.u_offset = 0x71,
	.v_offset = 0x8e,
	.y_offset = 0x0,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x80,
	.sign_bit = 0x0,
	.yoffset = 0x0,
	.uvmatrix00 = 0x0,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x0,
	.uvoffset0 = 0x8f,
	.uvoffset1 =0x0e,
	.vector_shift =0x0,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x4
};

static struct isp_effect posterize_effect = {
	.sde_enable = 0x0,
	.r_sde_ctrl00 = 0x0,
	.u_offset = 0x0,
	.v_offset = 0x0,
	.y_offset = 0x0,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x80,
	.sign_bit = 0x0,
	.yoffset = 0x0,
	.uvmatrix00 = 0x40,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x40,
	.uvoffset0 = 0x0,
	.uvoffset1 =0x0,
	.vector_shift =0x4,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x4
};

static struct isp_effect whiteboard_effect = {
	.sde_enable = 0x0,
	.r_sde_ctrl00 = 0x0,
	.u_offset = 0x0,
	.v_offset = 0x0,
	.y_offset = 0x0,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x80,
	.sign_bit = 0x0,
	.yoffset = 0x0,
	.uvmatrix00 = 0x0,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x0,
	.uvoffset0 = 0x0,
	.uvoffset1 =0x0,
	.vector_shift =0x7,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0xf,
};

static struct isp_effect blackboard_effect = {
	.sde_enable = 0x0,
	.r_sde_ctrl00 = 0x0,
	.u_offset = 0x0,
	.v_offset = 0x0,
	.y_offset = 0x0,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x280,
	.sign_bit = 0x0,
	.yoffset = 0x0,
	.uvmatrix00 = 0x0,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x0,
	.uvoffset0 = 0x0,
	.uvoffset1 =0x0,
	.vector_shift =0x7,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x8f,
};

static struct isp_effect aqua_effect = {
	.sde_enable = 0x1,
	.r_sde_ctrl00 = 0x18,
	.u_offset = 0xa1,
	.v_offset = 0x27,
	.y_offset = 0x0,
	.color_sel_enable = 0x0,
	.yuv_thre00 = 0x0,
	.yuv_thre01 = 0xff,
	.ygain = 0x80,
	.sign_bit = 0x0,
	.yoffset = 0x0,
	.uvmatrix00 = 0x0,
	.uvmatrix01 =0x0,
	.uvmatrix10 =0x0,
	.uvmatrix11 =0x0,
	.uvoffset0 = 0x21,
	.uvoffset1 =0x9,
	.vector_shift =0x0,
	.ne_enable =0x0,
	.pe_enable =0x0,
	.hthre =0x0,
	.hgain =0x4,
};

static int do_set_effect(struct isp_device *isp, struct isp_effect *effect)
{
	u8 reg;
	u8 ret;

	//enable sde
	reg = isp_reg_readb(isp, 0x65002);
	if (effect->sde_enable)
		reg |= (1 << 2);
	else
		reg &= ~(1<<2);
	isp_reg_writeb(isp, reg, 0x65002);

	isp_reg_writeb(isp, effect->r_sde_ctrl00, 0x65b00);
	isp_reg_writeb(isp, effect->u_offset, 0x65b03);
	isp_reg_writeb(isp, effect->v_offset, 0x65b04);
	isp_reg_writeb(isp, effect->y_offset, 0x65b05);

	//enable the color selection
	reg = isp_reg_readb(isp, 0x65d0a);
	if(effect->color_sel_enable)
		reg |= (1 << 5);
	else
		reg &= ~(1 <<5);
	isp_reg_writeb(isp, reg, 0x65d0a);

	// yuv thre
	isp_reg_writeb(isp, effect->yuv_thre00, 0x65d00);
	isp_reg_writeb(isp, effect->yuv_thre01, 0x65d01);

	//ygain
	reg = isp_reg_readb(isp, 0x65d0c);
	ret = (effect->ygain>>8)&0x3;
	if (ret & 0x1)
		reg |= 0x1;
	else if (ret & 0x2)
		reg |= 0x2;
	else
		reg &= ~0x3;
	isp_reg_writeb(isp, reg, 0x65d0c);
	isp_reg_writeb(isp, effect->ygain&0xff, 0x65d0d);

	//sign bit of yoffset
	reg = isp_reg_readb(isp, 0x65d19);
	if (effect->sign_bit)
		reg |= (1<<6);
	else
		reg &= ~(1<<6);
	isp_reg_writeb(isp, reg, 0x65d19);

	//yoffset
	isp_reg_writeb(isp, effect->yoffset&0xff, 0x65d16);

	//uvmatrix 01~11
	reg =  isp_reg_readb(isp, 0x65d0e);
	ret = (effect->uvmatrix00>>8)&0x3;
	if (ret & 0x1)
		reg |= 0x1;
	else if (ret & 0x2)
		reg |= 0x2;
	else
		reg &= ~0x3;
	isp_reg_writeb(isp, reg, 0x65d0e);
	isp_reg_writeb(isp, effect->uvmatrix00&0xff, 0x65d0f);

	reg =  isp_reg_readb(isp, 0x65d10);
	ret = (effect->uvmatrix01>>8)&0x3;
	if (ret & 0x1)
		reg |= 0x1;
	else if (ret & 0x2)
		reg |= 0x2;
	else
		reg &= ~0x3;
	isp_reg_writeb(isp, reg, 0x65d10);
	isp_reg_writeb(isp, effect->uvmatrix01&0xff, 0x65d11);

	reg =  isp_reg_readb(isp, 0x65d12);
	ret = (effect->uvmatrix10>>8)&0x3;
	if (ret & 0x1)
		reg |= 0x1;
	else if (ret & 0x2)
		reg |= 0x2;
	else
		reg &= ~0x3;
	isp_reg_writeb(isp, reg, 0x65d12);
	isp_reg_writeb(isp, effect->uvmatrix10&0xff, 0x65d13);

	reg =  isp_reg_readb(isp, 0x65d14);
	ret = (effect->uvmatrix11>>8)&0x3;
	if (ret & 0x1)
		reg |= 0x1;
	else if (ret & 0x2)
		reg |= 0x2;
	else
		reg &= ~0x3;
	isp_reg_writeb(isp, reg, 0x65d14);
	isp_reg_writeb(isp, effect->uvmatrix11&0xff, 0x65d15);

	//uvoffset 0~1
	isp_reg_writeb(isp, effect->uvoffset0, 0x65d17);
	isp_reg_writeb(isp, effect->uvoffset0, 0x65d18);

	//vector shift
	reg = isp_reg_readb(isp, 0x65d19);
	if (effect->vector_shift) {
		reg &= 0xf0;
		reg |= effect->vector_shift;
	} else
		reg &= ~0x0f;
	if (effect->ne_enable)
		reg |= (1<<4);
	else
		reg &= ~(1<<4);
	if(effect->pe_enable)
		reg |= (1<<5);
	else
		reg &= ~(1<<5);
	isp_reg_writeb(isp, reg, 0x65d19);

	//hthre
	reg = isp_reg_readb(isp, 0x65d1a);
	if (effect->hthre) {
		reg &= 0xc0;
		reg |= effect->hthre;
	}else
		reg &= ~0x3f;
	isp_reg_writeb(isp, reg, 0x65d1a);

	//hgain
	isp_reg_writeb(isp, effect->hgain, 0x65d1b);

	return 0;
}

int isp_set_effect(struct isp_device *isp, int val)
{
	switch (val) {
	case EFFECT_NONE:
		do_set_effect(isp, &none_effect);
		break;
	case EFFECT_MONO:
		do_set_effect(isp, &mono_effect);
		break;
	case EFFECT_NEGATIVE:
		do_set_effect(isp, &negative_effect);
		break;
	case EFFECT_SOLARIZE:
		do_set_effect(isp, &solarize_effect);
		break;
	case EFFECT_SEPIA:
		do_set_effect(isp, &sepia_effect);
		break;
	case EFFECT_POSTERIZE:
		do_set_effect(isp, &posterize_effect);
		break;
	case EFFECT_WHITEBOARD:
		do_set_effect(isp, &whiteboard_effect);
		break;
	case EFFECT_BLACKBOARD:
		do_set_effect(isp, &blackboard_effect);
		break;
	case EFFECT_AQUA:
		do_set_effect(isp, &aqua_effect);
		break;
	//leadcore add
	case EFFECT_WATER_GREEN:
	case EFFECT_WATER_BLUE:
	case EFFECT_BROWN:
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
	void *vals = NULL;

	if (val == ISO_AUTO) {
		if (isp->running) {
			if ((iparm->auto_max_gain > 0) && (iparm->auto_min_gain > 0)) {
				isp_reg_writeb(isp, 0, ISP_GAIN_MAX_H);
				isp_reg_writeb(isp, iparm->auto_max_gain, ISP_GAIN_MAX_L);
				isp_reg_writeb(isp, 0, ISP_GAIN_MIN_H);
				isp_reg_writeb(isp, iparm->auto_min_gain, ISP_GAIN_MIN_L);
				isp_reg_writew(isp, (u16)iparm->auto_gain_ctrl_1, REG_ISP_GAIN_CTRL_1);
				isp_reg_writew(isp, (u16)iparm->auto_gain_ctrl_2, REG_ISP_GAIN_CTRL_2);
				isp_reg_writew(isp, (u16)iparm->auto_gain_ctrl_3, REG_ISP_GAIN_CTRL_3);
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
	void *vals = NULL;

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
	void *vals = NULL;

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
	void *vals = NULL;

	if (effect_ops->get_brightness)
		count = effect_ops->get_brightness(&vals, val);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);
	else
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(isp_set_brightness);

int isp_set_win_setting(struct isp_device *isp, int width, int height)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals = NULL;

	if (effect_ops->get_isp_win_setting)
		count = effect_ops->get_isp_win_setting(width, height, &vals);
	if (vals != NULL)
		isp_writeb_array(isp, (struct isp_regb_vals *)vals);

	return ret;
}
EXPORT_SYMBOL(isp_set_win_setting);

int isp_set_aecgc_win(struct isp_device *isp, int width, int height, int meter_mode)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	int ret = 0;
	int count = -1;
	void *vals = NULL;

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

	isp_reg_writew(isp, max_exp, 0x1e934);

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

	cur_mean = isp_reg_readb(isp, 0x1c75e);
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

	iso = isp_reg_readb(isp, 0x1e95e);
	iso <<= 8;
	iso += isp_reg_readb(isp, 0x1e95f);

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
#ifdef CONFIG_CAMERA_DRIVING_RECODE

int isp_get_pre_brightness(struct isp_device *isp, int *val)
{
	u16 gain = 0;
	if (isp->aecgc_control)
		gain = isp_reg_readw(isp, REG_ISP_PRE_GAIN1);
	else
		gain = isp_reg_readw(isp, REG_ISP_PRE_GAIN2);
	*val = gain;

	return 0;
}
#endif
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
	u16 tmp = (u16)max_exp;

	isp_reg_writew(isp, tmp, REG_ISP_MAX_EXP);
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
	u16 tmp = (u16) min_gain;

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

int isp_set_dynamic_framerate_enable(struct isp_device *isp, int enable)
{
	isp_reg_writeb(isp, enable, REG_ISP_DYN_FRAMERATE);
	if(isp->exp_table_enable)
		isp_reg_writeb(isp, enable, REG_ISP_EXP_TABLE_ENABLE);
	return 0;
}

static int isp_get_ct(struct isp_device *isp)
{
	int b_gain = 0, r_gain = 0, ct = 0;

	b_gain = (int)isp_reg_readw(isp, REG_ISP_AWB_BGAIN);
	r_gain = (int)isp_reg_readw(isp, REG_ISP_AWB_RGAIN);
	ct = (b_gain << 8) / r_gain;

	return ct;
}

int isp_set_lens_threshold(struct isp_device *isp)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;
	struct isp_parm *iparm = &isp->parm;
	struct isp_regb_vals *vals = NULL;
	int exp = 0, gain = 0, brightness = 0;
	int in_width = iparm->in_width;
	int in_height = iparm->in_height;

	isp_get_pre_exp(isp, &exp);
	isp_get_pre_gain(isp, &gain);
	brightness = (exp * gain) >> 2;

	if (effect_ops && effect_ops->get_lens_threshold) {
		if (brightness <= isp->sd_isp_parm->dyn_lens_threshold_outdoor)
			effect_ops->get_lens_threshold((void**)&vals, in_width, in_height, 1);
		else if (brightness >= isp->sd_isp_parm->dyn_lens_threshold_indoor)
			effect_ops->get_lens_threshold((void**)&vals, in_width, in_height, 0);
	}

	if (vals != NULL)
		isp_writeb_array(isp, vals);
	return 0;
}

int isp_set_ct_saturation(struct isp_device *isp, int val)
{
	struct isp_effect_ops *effect_ops = isp->effect_ops;

	int count = -1;
	unsigned int ct = 0;
	int y1 = 0, y2 = 0;
	struct isp_regb_vals *a = NULL, *d = NULL, *c = NULL;
	struct isp_regb_vals s[3];
//	static struct isp_regb_vals s_last[2];
	int d_right = 0, c_left = 0, c_right = 0, a_left = 0;

	if (effect_ops->get_ct_saturation)
		count = effect_ops->get_ct_saturation((void**)&a,
										     (void**)&c,
										     (void**)&d,val);

	if (a == NULL || d == NULL || c == NULL)
		return -1;

	ct = isp_get_ct(isp);
	d_right = isp->sd_isp_parm->saturation_ct_threshold_d_right;
	c_left = isp->sd_isp_parm->saturation_ct_threshold_c_left;
	c_right = isp->sd_isp_parm->saturation_ct_threshold_c_right;
	a_left = isp->sd_isp_parm->saturation_ct_threshold_a_left;
	if (d_right == 0 || c_left == 0 || c_right == 0 || a_left == 0)
		return -1;

	if (ct <= d_right)
		memcpy(s, d, sizeof(s));
	else if (ct >d_right && ct < c_left) {
		memcpy(s, d, sizeof(s));
		y1 = d[0].value;
		y2 = c[0].value;
		s[0].value =(unsigned char)((y2 - y1) * (ct - d_right) / (c_left - d_right) + y1);
		y1 = d[1].value;
		y2 = c[1].value;
		s[1].value =(unsigned char)((y2 - y1) * (ct - d_right) / (c_left - d_right) + y1);
	} else if (ct >= c_left && ct <= c_right)
		memcpy(s, c, sizeof(s));
	else if (ct > c_right && ct < a_left) {
		memcpy(s, c, sizeof(s));
		y1 = c[0].value;
		y2 = a[0].value;
		s[0].value =(unsigned char)((y2 - y1) * (ct - c_right) / (a_left - c_right) + y1);
		y1 = c[1].value;
		y2 = a[1].value;
		s[1].value =(unsigned char)((y2 - y1) * (ct - c_right) / (a_left - c_right) + y1);
	} else
		memcpy(s, a, sizeof(s));

//	if (s_last[0].reg != s[0].reg || s_last[0].value != s[0].value ||
//		s_last[1].reg != s[1].reg || s_last[1].value != s[1].value) {
		isp_writeb_array(isp, s);
//		s_last[0].reg = s[0].reg;
//		s_last[0].value = s[0].value;
//		s_last[1].reg = s[1].reg;
//		s_last[1].value = s[1].value;
//	}

	return 0;
}

int isp_trigger_caf(struct isp_device *isp)
{
	isp_reg_writeb(isp, 0x01, REG_ISP_FOCUS_RESTART);
	return 0;
}

