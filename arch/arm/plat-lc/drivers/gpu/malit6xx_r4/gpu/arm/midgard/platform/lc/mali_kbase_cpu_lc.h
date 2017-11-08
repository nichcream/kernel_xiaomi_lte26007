/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





#ifndef _KBASE_CPU_LC_H_
#define _KBASE_CPU_LC_H_
/**
 * Versatile lc implementation of @ref kbase_cpuprops_clock_speed_function.
 */
int kbase_get_lc_cpu_clock_speed(u32 *cpu_clock);

int kbase_get_lc_gpu_clock_speed(u32 *clock_speed);


typedef struct mali_gpu_config
{
    u32 utilization_timeout;
    u32 sleep_timeout;
    u32 upthreshold;
    u32 downthreshold;
}mali_gpu_config;

void mali_gpu_configuration_get(mali_gpu_config *gpu_config);
void mali_gpu_configuration_set(mali_gpu_config gpu_config);
void mali_gpu_dvfs_debuglevel_set(u32 debug_level);
void mali_gpu_set_clk_index(u32 clk_index);
void mali_gpu_get_clk_index(u32 *clk_index);

mali_error kbase_pmm_lc_call(struct kbase_device *kbdev, void * const args, u32 args_size);
#endif				/* _KBASE_CPU_VEXPRESS_H_ */
