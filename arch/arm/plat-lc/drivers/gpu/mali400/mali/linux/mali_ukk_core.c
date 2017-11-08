/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2013 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <linux/fs.h>       /* file system operations */
#include <linux/slab.h>     /* memort allocation functions */
#include <asm/uaccess.h>    /* user space access */

#include "mali_ukk.h"
#include "mali_osk.h"
#include "mali_kernel_common.h"
#include "mali_session.h"
#include "mali_ukk_wrappers.h"
/* FIX L1810_Enh00000738 BEGIN DATE:2013-1-18 AUTHOR NAME ZHUXIAOPING */
#include "lc_pmm.h"
/* FIX L1810_Enh00000738 END DATE:2013-1-18 AUTHOR NAME ZHUXIAOPING */

int get_api_version_wrapper(struct mali_session_data *session_data, _mali_uk_get_api_version_s __user *uargs)
{
	_mali_uk_get_api_version_s kargs;
	_mali_osk_errcode_t err;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	if (0 != get_user(kargs.version, &uargs->version)) return -EFAULT;

	kargs.ctx = session_data;
	err = _mali_ukk_get_api_version(&kargs);
	if (_MALI_OSK_ERR_OK != err) return map_errcode(err);

	if (0 != put_user(kargs.version, &uargs->version)) return -EFAULT;
	if (0 != put_user(kargs.compatible, &uargs->compatible)) return -EFAULT;

	return 0;
}

int wait_for_notification_wrapper(struct mali_session_data *session_data, _mali_uk_wait_for_notification_s __user *uargs)
{
	_mali_uk_wait_for_notification_s kargs;
	_mali_osk_errcode_t err;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	kargs.ctx = session_data;
	err = _mali_ukk_wait_for_notification(&kargs);
	if (_MALI_OSK_ERR_OK != err) return map_errcode(err);

	if(_MALI_NOTIFICATION_CORE_SHUTDOWN_IN_PROGRESS != kargs.type) {
		kargs.ctx = NULL; /* prevent kernel address to be returned to user space */
		if (0 != copy_to_user(uargs, &kargs, sizeof(_mali_uk_wait_for_notification_s))) return -EFAULT;
	} else {
		if (0 != put_user(kargs.type, &uargs->type)) return -EFAULT;
	}

	return 0;
}

int post_notification_wrapper(struct mali_session_data *session_data, _mali_uk_post_notification_s __user *uargs)
{
	_mali_uk_post_notification_s kargs;
	_mali_osk_errcode_t err;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	kargs.ctx = session_data;

	if (0 != get_user(kargs.type, &uargs->type)) {
		return -EFAULT;
	}

	err = _mali_ukk_post_notification(&kargs);
	if (_MALI_OSK_ERR_OK != err) {
		return map_errcode(err);
	}

	return 0;
}

int get_user_settings_wrapper(struct mali_session_data *session_data, _mali_uk_get_user_settings_s __user *uargs)
{
	_mali_uk_get_user_settings_s kargs;
	_mali_osk_errcode_t err;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	kargs.ctx = session_data;
	err = _mali_ukk_get_user_settings(&kargs);
	if (_MALI_OSK_ERR_OK != err) {
		return map_errcode(err);
	}

	kargs.ctx = NULL; /* prevent kernel address to be returned to user space */
	if (0 != copy_to_user(uargs, &kargs, sizeof(_mali_uk_get_user_settings_s))) return -EFAULT;

	return 0;
}

int request_high_priority_wrapper(struct mali_session_data *session_data, _mali_uk_request_high_priority_s __user *uargs)
{
	_mali_uk_request_high_priority_s kargs;
	_mali_osk_errcode_t err;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	kargs.ctx = session_data;
	err = _mali_ukk_request_high_priority(&kargs);

	kargs.ctx = NULL;

	return map_errcode(err);
}
/* FIX L1810_Enh00000738 BEGIN DATE:2013-1-18 AUTHOR NAME ZHUXIAOPING */
int gpu_clk_set_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_clk_set_s __user *uargs)
{
	_mali_uk_pmm_clk_set_s kargs;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	if (0 != copy_from_user(&kargs, uargs, sizeof(_mali_uk_pmm_clk_set_s)))
	{
		return -EFAULT;
	}

	mali_gpu_clk_set(kargs.gpu_freq);

	return 0;
}

int gpu_dvfs_set_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_dvfs_set_s __user *uargs)
{
	_mali_uk_pmm_dvfs_set_s kargs;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	if (0 != copy_from_user(&kargs, uargs, sizeof(_mali_uk_pmm_dvfs_set_s)))
	{
		return -EFAULT;
	}

	mali_gpu_dvfs_set(kargs.dvfs_enable, kargs.max_freq);

	return 0;
}

int gpu_dvfs_get_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_dvfs_set_s __user *uargs)
{
	_mali_uk_pmm_dvfs_set_s kargs;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	mali_gpu_dvfs_get(&kargs.dvfs_enable, &kargs.max_freq);

	kargs.ctx = NULL; /* prevent kernel address to be returned to user space */
	if (0 != copy_to_user(uargs, &kargs, sizeof(_mali_uk_pmm_dvfs_set_s)))
	{
		return -EFAULT;
	}

	return 0;
}

int gpu_config_get_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_config_set_s __user *uargs)
{
	_mali_uk_pmm_config_set_s kargs;
    mali_gpu_config gpu_config;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	mali_gpu_configuration_get(&gpu_config);

    kargs.utilization_timeout = gpu_config.utilization_timeout;
    kargs.sleep_timeout = gpu_config.sleep_timeout;
    kargs.upthreshold = gpu_config.upthreshold;
    kargs.downthreshold = gpu_config.downthreshold;

	kargs.ctx = NULL; /* prevent kernel address to be returned to user space */
	if (0 != copy_to_user(uargs, &kargs, sizeof(_mali_uk_pmm_config_set_s)))
	{
		return -EFAULT;
	}

	return 0;
}

int gpu_config_set_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_config_set_s __user *uargs)
{
	_mali_uk_pmm_config_set_s kargs;
    mali_gpu_config gpu_config;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	if (0 != copy_from_user(&kargs, uargs, sizeof(_mali_uk_pmm_config_set_s)))
	{
		return -EFAULT;
	}

    gpu_config.utilization_timeout = kargs.utilization_timeout;
    gpu_config.sleep_timeout = kargs.sleep_timeout;
    gpu_config.upthreshold = kargs.upthreshold;
    gpu_config.downthreshold = kargs.downthreshold;

	mali_gpu_configuration_set(gpu_config);

	return 0;
}

int gpu_dvfs_debuglevel_set_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_dvfs_debuglevel_set_s __user *uargs)
{
	_mali_uk_pmm_dvfs_debuglevel_set_s kargs;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	if (0 != copy_from_user(&kargs, uargs, sizeof(_mali_uk_pmm_dvfs_debuglevel_set_s)))
	{
		return -EFAULT;
	}

	mali_gpu_dvfs_debuglevel_set(kargs.debug_level);

	return 0;
}

int gpu_set_clk_index_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_setclk_idx_s __user *uargs)
{
	_mali_uk_pmm_setclk_idx_s kargs;
    u32 clk_index;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	if (0 != copy_from_user(&kargs, uargs, sizeof(_mali_uk_pmm_setclk_idx_s)))
	{
		return -EFAULT;
	}

    clk_index = kargs.clk_index;

	mali_gpu_set_clk_index(clk_index);

	return 0;
}

int gpu_get_clk_index_wrapper(struct mali_session_data *session_data, _mali_uk_pmm_setclk_idx_s __user *uargs)
{
	_mali_uk_pmm_setclk_idx_s kargs;
    u32 clk_index;

	MALI_CHECK_NON_NULL(uargs, -EINVAL);

	mali_gpu_get_clk_index(&clk_index);

    kargs.clk_index = clk_index;

	kargs.ctx = NULL; /* prevent kernel address to be returned to user space */
	if (0 != copy_to_user(uargs, &kargs, sizeof(_mali_uk_pmm_setclk_idx_s)))
	{
		return -EFAULT;
	}

	return 0;
}
/* FIX L1810_Enh00000738 END DATE:2013-1-18 AUTHOR NAME ZHUXIAOPING */

