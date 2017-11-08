#ifndef __ISP2_CTRL_H__
#define __ISP2_CTRL_H__

#include "comip-isp2.h"

struct isp_regb_vals {
	unsigned int reg;
	unsigned char value;
};

extern int isp_set_exposure(struct isp_device *isp, int val);
extern int isp_set_iso(struct isp_device *isp, int val);
extern int isp_set_contrast(struct isp_device *isp, int val);
extern int isp_set_saturation(struct isp_device *isp, int val);
extern int isp_set_scene(struct isp_device *isp, int val);
extern int isp_set_effect(struct isp_device *isp, int val);
extern int isp_set_white_balance(struct isp_device *isp, int val);
extern int isp_set_brightness(struct isp_device *isp, int val);
extern int isp_set_sharpness(struct isp_device *isp, int val);
extern int isp_set_flicker(struct isp_device *isp, int val);
extern int isp_check_focus_cap(struct isp_device *isp, int val);
extern int isp_set_hflip(struct isp_device *isp, int val);
extern int isp_set_vflip(struct isp_device *isp, int val);
extern int isp_check_meter_cap(struct isp_device *isp, int val);
extern int isp_set_capture_anti_shake(struct isp_device *isp, int on);
extern int isp_set_vcm_range(struct isp_device *isp, int focus_mode);
extern int isp_get_aecgc_stable(struct isp_device *isp, int *stable);
extern int isp_get_brightness_level(struct isp_device *isp, int *level);
extern int isp_get_iso(struct isp_device *isp, int *val);
extern int isp_get_pre_gain(struct isp_device *isp, int *val);
extern int isp_get_pre_exp(struct isp_device *isp, int *val);
extern int isp_get_sna_gain(struct isp_device *isp, int *val);
extern int isp_get_sna_exp(struct isp_device *isp, int *val);
extern int isp_get_dynfrt_cond(struct isp_device *isp, int down_small_thres, int down_large_thres, int up_small_thres, int up_large_thres);
extern int isp_set_max_exp(struct isp_device *isp, int max_exp);
extern int isp_set_vts(struct isp_device *isp, int vts);
extern int isp_set_min_gain(struct isp_device *isp, unsigned int min_gain);
extern int isp_set_max_gain(struct isp_device *isp, unsigned int max_gain);
extern int isp_enable_aecgc_writeback(struct isp_device *isp, int enable);
extern void isp_get_func(struct isp_device *isp, struct isp_effect_ops **);
extern int isp_set_aecgc_win(struct isp_device *isp, int width, int height, int meter_mode);
extern int isp_get_banding_step(struct isp_device *isp, int *val);
extern int isp_set_pre_gain(struct isp_device *isp, unsigned int gain);
extern int isp_set_pre_exp(struct isp_device *isp, unsigned int exp);
extern int isp_set_aecgc_enable(struct isp_device *isp, int enable);

#endif/*__ISP2_CTRL_H__*/
