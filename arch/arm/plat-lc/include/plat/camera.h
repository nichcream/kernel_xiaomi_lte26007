#ifndef __ASM_ARCH_PLAT_CAMERA_H
#define __ASM_ARCH_PLAT_CAMERA_H

#include <linux/i2c.h>
#include "comip-v4l2.h"

/* Camera flags. */
#define CAMERA_USE_ISP_I2C		(0x00000001)
#define CAMERA_USE_HIGH_BYTE		(0x00000002)
#define CAMERA_I2C_PIO_MODE		(0x00010000)
#define CAMERA_I2C_STANDARD_SPEED	(0x00020000)
#define CAMERA_I2C_FAST_SPEED		(0x00040000)
#define CAMERA_I2C_HIGH_SPEED		(0x00080000)

/* Client flags. */
#define CAMERA_CLIENT_CLK_EXT		(0x00000001)
#define CAMERA_CLIENT_FRAMERATE_DYN     (0x00000002)
#define CAMERA_CLIENT_IF_MIPI		(0x00000100)
#define CAMERA_CLIENT_IF_DVP		(0x00000200)
#define CAMERA_CLIENT_YUV_DATA		(0x00010000)
#define CAMERA_CLIENT_INDEP_I2C		(0x01000000)
#define CAMERA_CLIENT_ISP_CLK_HIGH	(0x10000000)
#define CAMERA_CLIENT_ISP_CLK_MIDDLE	(0x20000000)
#define CAMERA_CLIENT_ISP_CLK_HHIGH	(0x40000000)

/* obsolete caps */
#define CAMERA_CAP_FOCUS_AUTO			(0)
#define CAMERA_CAP_FOCUS_INFINITY		(0)
#define CAMERA_CAP_FOCUS_MACRO			(0)
#define CAMERA_CAP_FOCUS_FIXED			(0)
#define CAMERA_CAP_FOCUS_EDOF			(0)
#define CAMERA_CAP_FOCUS_CONTINUOUS_VIDEO	(0)
#define CAMERA_CAP_FOCUS_CONTINUOUS_PICTURE	(0)
#define CAMERA_CAP_FOCUS_CONTINUOUS_AUTO	(0)
#define CAMERA_CAP_METER_DOT			(0)
#define CAMERA_CAP_METER_MATRIX			(0)
#define CAMERA_CAP_METER_CENTER			(0)
#define CAMERA_CAP_FACE_DETECT			(0)
#define CAMERA_CAP_ANTI_SHAKE_CAPTURE		(0)


#define CAMERA_CAP_HDR_CAPTURE			(0x00000001)
#define CAMERA_CAP_FLASH				(0x00000002)
#define CAMERA_CAP_SW_HDR_CAPTURE		(0x00000004)
#define CAMERA_CAP_VIV_CAPTURE			(0x00000008)
#define CAMERA_CAP_ROTATION_0			(0x00000010)
#define CAMERA_CAP_ROTATION_90			(0x00000020)
#define CAMERA_CAP_ROTATION_180			(0x00000040)
#define CAMERA_CAP_ROTATION_270			(0x00000080)


/* Camera sub-device platform data. */
#define CAMERA_SUBDEV_FLAG_MIRROR	(0x00000001)
#define CAMERA_SUBDEV_FLAG_FLIP		(0x00000002)

#define COMIP_CAMERA_AECGC_INVALID_VALUE	(0)

enum camera_led_mode {
	FLASH = 0,
	TORCH = 1,
};

struct comip_camera_client {
	int i2c_adapter_id;
	struct i2c_board_info *board_info;
	unsigned long flags;
	unsigned long caps;
	int if_id;
	int mipi_lane_num;
	int camera_id;
	const char* mclk_parent_name;
	const char* mclk_name;
	unsigned long mclk_rate;
	int (*power)(int);
	int (*reset)(void);
	int (*flash)(enum camera_led_mode, int);
};

struct comip_camera_anti_shake_parms {
	int a;//ExposureT=MaxExpPreset/12
	int b;//ExposureT=MaxExpPreset/3
	int c;//GainT=MaxGainPreset*3/4
	int d;//GainT=MaxGainPreset*3/4
};

struct comip_camera_flash_parms {
	unsigned int redeye_off_duration;
	unsigned int redeye_on_duration;
	unsigned int snapshot_pre_duration;
	unsigned int aecgc_stable_duration;
	unsigned int torch_off_duration;
	unsigned int flash_on_ramp_time;
	unsigned int mean_percent;
	int flash_brightness;
	unsigned int brightness_ratio;
};

struct comip_camera_mem_delayed_release {
	unsigned int delayed_release_duration;//in sec unit
	unsigned int memfree_threshold;//in MB unit,system total free phys mem threshold,when sys total free phys mem <= memfree_threshold delayed release is ignored
};

struct comip_camera_platform_data {
	int i2c_adapter_id;
	unsigned long flags;
	struct comip_camera_anti_shake_parms anti_shake_parms;
	struct comip_camera_flash_parms flash_parms;
	struct comip_camera_mem_delayed_release mem_delayed_release;
	unsigned int client_num;
	struct comip_camera_client *client;
};

struct comip_camera_subdev_platform_data {
	unsigned long flags;
};

#endif /* __ASM_ARCH_PLAT_CAMERA_H */

