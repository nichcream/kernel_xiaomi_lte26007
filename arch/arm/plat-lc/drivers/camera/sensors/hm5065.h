#ifndef __HM5065_H__
#define __HM5065_H__

static const struct v4l2_isp_parm hm5065_isp_parm = {
	.sensor_vendor = SENSOR_VENDOR_HIMAX,
	.sensor_main_type = HM_5065,
	.sensor_sub_type = 1,
	.dvp_vsync_polar = 1,
};

static const struct v4l2_isp_regval hm5065_isp_setting[] = {};

#endif /* __HM5065_H__ */
