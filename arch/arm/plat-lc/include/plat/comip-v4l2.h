#ifndef __COMIP_V4L2_H__
#define __COMIP_V4L2_H__

#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

/* External v4l2 format info. */
#define V4L2_I2C_REG_MAX		(120)
#define V4L2_I2C_ADDR_16BIT		(0x0002)
#define V4L2_I2C_DATA_16BIT		(0x0004)

struct v4l2_i2c_reg {
	unsigned short addr;
	unsigned short data;
};

struct v4l2_aecgc_parm {
	int	gain;
	int exp;
	int vts;
};

struct v4l2_fmt_data {
	struct v4l2_aecgc_parm aecgc_parm;
	struct v4l2_i2c_reg reg[V4L2_I2C_REG_MAX];
	unsigned short hts;
	unsigned short vts;
	unsigned short exposure;
	unsigned short gain;
	unsigned short max_gain_dyn_frm;
	unsigned short min_gain_dyn_frm;
	unsigned short max_gain_fix_frm;
	unsigned short min_gain_fix_frm;
	unsigned short framerate;
	unsigned short framerate_div;
	unsigned short min_framerate;
	unsigned char  vts_gain_ctrl_1;
	unsigned char  vts_gain_ctrl_2;
	unsigned char  vts_gain_ctrl_3;
	unsigned short gain_ctrl_1;
	unsigned short gain_ctrl_2;
	unsigned short gain_ctrl_3;
	unsigned short spot_meter_win_width;
	unsigned short spot_meter_win_height;
	unsigned short binning;
	unsigned short flags;
	unsigned short sensor_vts_address1;
	unsigned short sensor_vts_address2;
	unsigned short mipi_clk; /* Mbps. */
	unsigned char slave_addr;
	unsigned char reg_num;
	unsigned char viv_section;
	unsigned char viv_addr;
	unsigned char viv_len;
	unsigned int  exp_ratio;
};

struct v4l2_mbus_framefmt_ext {
	__u32			width;
	__u32			height;
	__u32			code;
	__u32			field;
	__u32			colorspace;
	__u32			reserved[150];
};

struct v4l2_flash_enable {
	unsigned short on;
	unsigned short mode;
};

struct v4l2_vm_area{
	void *vaddr;
	unsigned int length;
};

struct v4l2_flash_duration {
	unsigned int redeye_off_duration;
	unsigned int redeye_on_duration;
	unsigned int snapshot_pre_duration;
	unsigned int aecgc_stable_duration;
	unsigned int torch_off_duration;
	unsigned int flash_on_ramp_time;
};

struct v4l2_isp_regval {
	unsigned int reg;
	unsigned char val;
};

struct v4l2_isp_setting {
	const struct v4l2_isp_regval *setting;
	unsigned int size;
};

struct v4l2_isp_firmware_setting {
	const unsigned char *setting;
	unsigned int size;
};

struct v4l2_iso_config {
	int	iso;
	unsigned int min_gain;
	unsigned int max_gain;
};

struct v4l2_isp_parm {
	unsigned short min_exposure;
	unsigned short default_snapshot_exposure_ratio;
	unsigned short default_preview_exposure_ratio;
	unsigned short meter_light_roi;
	unsigned char sensor_vendor;
	unsigned char sensor_main_type;
	unsigned char sensor_sub_type;
	unsigned char mirror;
	unsigned char flip;
	unsigned char dvp_vsync_polar;
	unsigned short saturation_ct_threshold_d_right;
	unsigned short saturation_ct_threshold_c_left;
	unsigned short saturation_ct_threshold_c_right;
	unsigned short saturation_ct_threshold_a_left;
	unsigned short dyn_lens_threshold_indoor;
	unsigned short dyn_lens_threshold_outdoor;
};

struct v4l2_query_index {
	int camera_id;
	int index;
};

#define CAMERA_ID_BACK		(0x01)
#define CAMERA_ID_FRONT		(0x02)

enum {
	SENSOR_VENDOR_ANY = 0,
	SENSOR_VENDOR_OMNIVISION,
	SENSOR_VENDOR_SAMSUMG,
	SENSOR_VENDOR_GALAXYCORE,
	SENSOR_VENDOR_HYNIX,
	SENSOR_VENDOR_HIMAX,
	SENSOR_VENDOR_BYD,
	SENSOR_VENDOR_SUPERPIX,
};

/*sensor main type*/
/*for ov sensors*/
enum {
	OV_ANY = 0,
	OV_5647,
	OV_5648,
	OV_8825,
	OV_2680,
};

/*for glaxycore sensors*/
enum {
	GC_ANY = 0,
	GC_0329,
	GC_2035,
	GC_2235,
	GC_2355,
	GC_2145,
	GC_5004,
	GC_0310,
	GC_2755,
};

/*for samsung sensors */
enum {
	SAMSUNG_ANY = 0,
	SAMSUNG_S5K4E1,
	SAMSUNG_S5K3H2,
};

/*for himax sensors*/
enum{
	HM_ANY=0,
	HM_5065,
	HM_5040,
	HM_2058,
	HM_2056,
};

/*for byd sensors*/
enum {
	BYD_ANY = 0,
	BYD_BF3905,
	BYD_BF3920,
	BYD_BF3A50,
	BYD_BF3A03,
	BYD_BF3703,
	BYD_BF3A20_MIPI,
};

/*for SP sensors*/
enum {
	SP_ANY = 0,
	SP_2508,
	SP_5408,
	SP_2529,
	SP_5409,
	SP_0A20,
};

#define v4l2_get_fmt_data(fmt)		((struct v4l2_fmt_data *)(&(fmt)->reserved[0]))

/* External control IDs. */
#define V4L2_CID_ISO					(V4L2_CID_PRIVACY + 1)
#define V4L2_CID_EFFECT					(V4L2_CID_PRIVACY + 2)
#define V4L2_CID_PHOTOMETRY				(V4L2_CID_PRIVACY + 3)
#define V4L2_CID_FLASH_MODE				(V4L2_CID_PRIVACY + 4)
#define V4L2_CID_EXT_FOCUS				(V4L2_CID_PRIVACY + 5)
#define V4L2_CID_SCENE					(V4L2_CID_PRIVACY + 6)
#define V4L2_CID_SET_SENSOR				(V4L2_CID_PRIVACY + 7)
#define V4L2_CID_FRAME_RATE				(V4L2_CID_PRIVACY + 8)
#define V4L2_CID_SET_FOCUS				(V4L2_CID_PRIVACY + 9)	//set on or off
#define V4L2_CID_AUTO_FOCUS_RESULT		(V4L2_CID_PRIVACY + 10)
#define V4L2_CID_SET_FOCUS_RECT			(V4L2_CID_PRIVACY + 11)
#define V4L2_CID_SET_METER_RECT			(V4L2_CID_PRIVACY + 12)
#define V4L2_CID_GET_CAPABILITY 		(V4L2_CID_PRIVACY + 13)
#define V4L2_CID_SET_METER_MODE			(V4L2_CID_PRIVACY + 14)
#define V4L2_CID_SET_ANTI_SHAKE_CAPTURE	(V4L2_CID_PRIVACY + 15)
#define V4L2_CID_SET_HDR_CAPTURE		(V4L2_CID_PRIVACY + 16)
#define V4L2_CID_SET_FLASH				(V4L2_CID_PRIVACY + 17)
#define V4L2_CID_GET_AECGC_STABLE		(V4L2_CID_PRIVACY + 18)
#define V4L2_CID_GET_BRIGHTNESS_LEVEL	(V4L2_CID_PRIVACY + 19)
#define V4L2_CID_GET_PREFLASH_DURATION	(V4L2_CID_PRIVACY + 20)
#define V4L2_CID_ISP_SETTING			(V4L2_CID_PRIVACY + 21)
#define V4L2_CID_ISP_PARM				(V4L2_CID_PRIVACY + 22)
#define V4L2_CID_MIN_GAIN				(V4L2_CID_PRIVACY + 23)
#define V4L2_CID_MAX_GAIN				(V4L2_CID_PRIVACY + 24)
#define V4L2_CID_SNAPSHOT_GAIN			(V4L2_CID_PRIVACY + 25)
#define V4L2_CID_SHUTTER_SPEED			(V4L2_CID_PRIVACY + 26)
#define V4L2_CID_SNAPSHOT				(V4L2_CID_PRIVACY + 27)
#define V4L2_CID_NO_DATA_DEBUG			(V4L2_CID_PRIVACY + 28)
#define V4L2_CID_PRE_GAIN				(V4L2_CID_PRIVACY + 29)
#define V4L2_CID_PRE_EXP				(V4L2_CID_PRIVACY + 30)
#define V4L2_CID_SNA_GAIN				(V4L2_CID_PRIVACY + 31)
#define V4L2_CID_SNA_EXP				(V4L2_CID_PRIVACY + 32)
#define V4L2_CID_AECGC_PARM				(V4L2_CID_PRIVACY + 33)
#define V4L2_CID_DOWNFRM_QUERY			(V4L2_CID_PRIVACY + 34)
#define V4L2_CID_MAX_EXP				(V4L2_CID_PRIVACY + 35)
#define V4L2_CID_VTS					(V4L2_CID_PRIVACY + 36)
#define V4L2_CID_GET_EFFECT_FUNC			(V4L2_CID_PRIVACY + 37)
#define V4L2_CID_SET_VIV_MODE				(V4L2_CID_PRIVACY + 38)
#define V4L2_CID_SET_VIV_WIN_POSITION			(V4L2_CID_PRIVACY + 39)
#define V4L2_CID_READ_REGISTER			(V4L2_CID_PRIVACY + 40)
#define V4L2_CID_CACHE_FLUSH			(V4L2_CID_PRIVACY + 42)
#define V4L2_CID_CACHE_INVALIDATE		(V4L2_CID_PRIVACY + 43)
#define V4L2_CID_QUERY_CAMERA_INDEX		(V4L2_CID_PRIVACY + 44)
#define V4L2_CID_GET_ISP_FIRMWARE		(V4L2_CID_PRIVACY + 45)
#define V4L2_CID_SET_SENSOR_MIRROR		(V4L2_CID_PRIVACY + 46)
#define V4L2_CID_SET_SENSOR_FLIP		(V4L2_CID_PRIVACY + 47)
#define V4L2_CID_SET_SNA_EXP_MODE		(V4L2_CID_PRIVACY + 48)
#define V4L2_CID_SENSOR_ESD_RESET		(V4L2_CID_PRIVACY + 49)
#ifdef CONFIG_CAMERA_DRIVING_RECODE

#define V4L2_CID_GET_MANUAL_FOUCS_SHARPNESS            (V4L2_CID_PRIVACY + 100)
#define V4L2_CID_GET_PREVIEW_PARM              (V4L2_CID_PRIVACY + 101)
#endif
#define V4L2_CID_DENOISE  				(V4L2_CID_PRIVACY + 50)
#define V4L2_CID_RESTART_FOCUS			(V4L2_CID_PRIVACY + 51)

//capability
struct v4l2_privacy_cap
{
	unsigned char buf[8];
};

enum v4l2_scene_mode_e {
	SCENE_MODE_AUTO = 0,
	SCENE_MODE_ACTION,
	SCENE_MODE_PORTRAIT,
	SCENE_MODE_LANDSCAPE,
	SCENE_MODE_NIGHT,
	SCENE_MODE_NIGHT_PORTRAIT,
	SCENE_MODE_THEATRE,
	SCENE_MODE_BEACH,
	SCENE_MODE_SNOW,
	SCENE_MODE_SUNSET,
	SCENE_MODE_STEADYPHOTO,
	SCENE_MODE_FIREWORKS,
	SCENE_MODE_SPORTS,
	SCENE_MODE_PARTY,
	SCENE_MODE_CANDLELIGHT,
	SCENE_MODE_BARCODE,
	SCENE_MODE_NORMAL,
	SCENE_MODE_FLOWERS,
	SCENE_MODE_MAX,
};

enum v4l2_flash_mode {
	FLASH_MODE_OFF = 0,
	FLASH_MODE_AUTO,
	FLASH_MODE_ON,
	FLASH_MODE_RED_EYE,
	FLASH_MODE_TORCH,
	FLASH_MODE_MAX,
};

enum v4l2_wb_mode {
	WHITE_BALANCE_AUTO = 0,
	WHITE_BALANCE_INCANDESCENT,
	WHITE_BALANCE_FLUORESCENT,
	WHITE_BALANCE_WARM_FLUORESCENT,
	WHITE_BALANCE_DAYLIGHT,
	WHITE_BALANCE_CLOUDY_DAYLIGHT,
	WHITE_BALANCE_TWILIGHT,
	WHITE_BALANCE_SHADE,
	WHITE_BALANCE_MAX,
};

enum v4l2_effect_mode {
	EFFECT_NONE = 0,
	EFFECT_MONO,
	EFFECT_NEGATIVE,
	EFFECT_SOLARIZE,
	EFFECT_SEPIA,
	EFFECT_POSTERIZE,
	EFFECT_WHITEBOARD,
	EFFECT_BLACKBOARD,
	EFFECT_AQUA,
	EFFECT_WATER_GREEN,
	EFFECT_WATER_BLUE,
	EFFECT_BROWN,
	EFFECT_MAX,
};

enum v4l2_iso_mode {
	ISO_AUTO = 0,
	ISO_100,
	ISO_200,
	ISO_400,
	ISO_800,
	ISO_MAX,
};

enum v4l2_exposure_mode {
	EXPOSURE_L6 = -4,
	EXPOSURE_L5 = -3,
	EXPOSURE_L4 = -2,
	EXPOSURE_L3 = -1,
	EXPOSURE_L2 = 0,
	EXPOSURE_L1,
	EXPOSURE_H0,
	EXPOSURE_H1,
	EXPOSURE_H2,
	EXPOSURE_H3,
	EXPOSURE_H4,
	EXPOSURE_H5,
	EXPOSURE_H6,
	EXPOSURE_MAX,
};

enum v4l2_contrast_mode {
	CONTRAST_L4 = -1,
	CONTRAST_L3 = 0,
	CONTRAST_L2,
	CONTRAST_L1,
	CONTRAST_H0,
	CONTRAST_H1,
	CONTRAST_H2,
	CONTRAST_H3,
	CONTRAST_H4,
	CONTRAST_MAX,
};

enum v4l2_saturation_mode {
	SATURATION_L2 = 0,
	SATURATION_L1,
	SATURATION_H0,
	SATURATION_H1,
	SATURATION_H2,
	SATURATION_MAX,
};

enum v4l2_sharpness_mode {
	SHARPNESS_L2 = 0,
	SHARPNESS_L1,
	SHARPNESS_H0,
	SHARPNESS_H1,
	SHARPNESS_H2,
	SHARPNESS_H3,
	SHARPNESS_H4,
	SHARPNESS_H5,
	SHARPNESS_H6,
	SHARPNESS_H7,
	SHARPNESS_H8,
	SHARPNESS_MAX,
};

enum v4l2_brightness_mode {
	BRIGHTNESS_L4 = -1,
	BRIGHTNESS_L3 = 0,
	BRIGHTNESS_L2,
	BRIGHTNESS_L1,
	BRIGHTNESS_H0,
	BRIGHTNESS_H1,
	BRIGHTNESS_H2,
	BRIGHTNESS_H3,
	BRIGHTNESS_H4,
	BRIGHTNESS_MAX,
};

enum v4l2_flicker_mode {
	FLICKER_AUTO = 0,
	FLICKER_50Hz,
	FLICKER_60Hz,
	FLICKER_OFF,
	FLICKER_MAX,
};

enum v4l2_zoom_level {
	ZOOM_LEVEL_0 = 0,
	ZOOM_LEVEL_1,
	ZOOM_LEVEL_2,
	ZOOM_LEVEL_3,
	ZOOM_LEVEL_4,
	ZOOM_LEVEL_5,
	ZOOM_LEVEL_MAX,
};

enum v4l2_focus_mode {
	FOCUS_MODE_AUTO = 0,
	FOCUS_MODE_INFINITY,
	FOCUS_MODE_MACRO,
	FOCUS_MODE_FIXED,
	FOCUS_MODE_EDOF,
	FOCUS_MODE_CONTINUOUS_VIDEO,
	FOCUS_MODE_CONTINUOUS_PICTURE,
	FOCUS_MODE_CONTINUOUS_AUTO,
	FOCUS_MODE_MAX,
};

enum v4l2_meter_mode {
	METER_MODE_CENTER = 0,
	METER_MODE_DOT,
	METER_MODE_MATRIX,
	METER_MODE_CENTRAL_WEIGHT,
	METER_MODE_MAX,
};

enum v4l2_focus_status{
    AUTO_FOCUS_OFF = 0,
    AUTO_FOCUS_FORCE_OFF,
    AUTO_FOCUS_ON,
    AUTO_FOCUS_ON_CLEAR_RESULT,
};

enum v4l2_sna_exp_mode {
	SNA_NORMAL = 0,
	SNA_NORMAL_FLASH,
	SNA_ZSL_NORMAL,
	SNA_ZSL_FLASH,
};

enum v4l2_frame_rate {
	FRAME_RATE_AUTO = 0,
	FRAME_RATE_DEFAULT,
	FRAME_RATE_7    = 7,
	FRAME_RATE_15   = 15,
	FRAME_RATE_30   = 30,
	FRAME_RATE_60   = 60,
	FRAME_RATE_120  = 120,
	FRAME_RATE_MAX,
};

enum v4l2_viv_sub_window {
	VIV_LL = 0,
	VIV_LR,
	VIV_UL,
	VIV_UR,
};

enum v4l2_viv_status {
	VIV_OFF = 0,
	VIV_ON,
};

//IOCTLS CODES
#define SUBDEVIOC_MODULE_FACTORY	_IO('S', 0)

#endif /*__COMIP_V4L2_H__*/
