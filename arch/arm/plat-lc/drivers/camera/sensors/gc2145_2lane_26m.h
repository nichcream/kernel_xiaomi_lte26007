#ifndef __GC2145_H__
#define __GC2145_H__

static struct v4l2_isp_parm gc2145_isp_parm = {
	.sensor_vendor = SENSOR_VENDOR_GALAXYCORE,
	.sensor_main_type = GC_2145,
	.sensor_sub_type = 0,
	.mirror = 0,
	.flip = 0,
};

static const struct v4l2_isp_regval gc2145_isp_setting[] = {};

#endif /* __GC2145_MIPI_H__ */
