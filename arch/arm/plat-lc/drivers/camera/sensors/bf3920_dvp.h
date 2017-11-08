#ifndef __BF3920_H__
#define __BF3920_H__

static struct v4l2_isp_parm bf3920_isp_parm = {
	.sensor_sub_type = 1,
	.dvp_vsync_polar = 1,
	.mirror = 0,
	.flip = 0,
};

static const struct v4l2_isp_regval bf3920_isp_setting[] = {

};

#endif /* __BF3920_H__ */
