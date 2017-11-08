#ifndef __BF3A20_MIPI_H__
#define __BF3A20_MIPI_H__

static const struct v4l2_isp_parm bf3a20_mipi_isp_parm = {
	//.sensor_vendor = SENSOR_VENDOR_GALAXYCORE,
	.sensor_vendor = SENSOR_VENDOR_BYD,
	.sensor_main_type = BYD_BF3A20_MIPI,
	.sensor_sub_type = 0,
};

static const struct v4l2_isp_regval bf3a20_mipi_isp_setting[] = {};

#endif /* __BF3A20_MIPI_H__ */
