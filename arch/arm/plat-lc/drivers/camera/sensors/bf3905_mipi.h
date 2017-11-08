#ifndef __BF3905_H__
#define __BF3905_H__

static struct v4l2_isp_parm bf3905_isp_parm = {
	//.sensor_vendor = SENSOR_VENDOR_GALAXYCORE,
	.sensor_vendor = SENSOR_VENDOR_BYD,
	.sensor_main_type = BYD_BF3905,
	.sensor_sub_type = 0,
	.mirror = 0,
	.flip = 0,
};

static const struct v4l2_isp_regval bf3905_isp_setting[] = {};

#endif /* __BF3905_H__ */
