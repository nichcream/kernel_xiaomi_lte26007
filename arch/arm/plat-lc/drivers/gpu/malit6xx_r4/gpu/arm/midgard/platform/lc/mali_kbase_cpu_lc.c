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





#include <linux/io.h>
#include <mali_kbase.h>
#include "mali_kbase_cpu_lc.h"

#define HZ_IN_MHZ					    (1000000)

#define CPU_CLOCK_SPEED_UNDEFINED 0
#define CPU_CLOCK_SPEED_LC 1500

static u32 cpu_clock_speed = CPU_CLOCK_SPEED_UNDEFINED;

//static DEFINE_RAW_SPINLOCK(syscfg_lock);
/**
 * kbase_get_vendor_specific_cpu_clock_speed
 * @brief  Retrieves the CPU clock speed.
 *         The implementation is platform specific.
 * @param[out]    cpu_clock - the value of CPU clock speed in MHz
 * @return        0 on success, 1 otherwise
*/
int kbase_get_lc_cpu_clock_speed(u32 *cpu_clock)
{
	//todo
	int result = CPU_CLOCK_SPEED_LC;
	cpu_clock_speed = CPU_CLOCK_SPEED_LC;
	return result;
}
