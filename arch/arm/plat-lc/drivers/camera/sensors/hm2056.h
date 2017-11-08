#ifndef __HM2056_H__
#define __HM2056_H__

static struct v4l2_isp_parm hm2056_isp_parm = {
	.sensor_vendor = SENSOR_VENDOR_HIMAX,
	.sensor_main_type = HM_2056,
	.sensor_sub_type = 1,
};

static const struct v4l2_isp_regval hm2056_isp_setting[] = {};

#endif /* __HM2056_H__ */
