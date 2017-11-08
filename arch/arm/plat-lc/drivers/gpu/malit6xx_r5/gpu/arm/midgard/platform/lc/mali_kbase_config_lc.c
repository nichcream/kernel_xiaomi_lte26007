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



#include <linux/ioport.h>

#include <asm/io.h>
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>
#ifdef CONFIG_UMP
#include <linux/ump-common.h>
#endif				/* CONFIG_UMP */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <mach/irqs.h>

#include <mach/comip-regs.h>
#include <mach/comip-dvfs.h>
#include <plat/comip-thermal.h>
#include <plat/clock.h>
#include <plat/hardware.h>

#include "mali_kbase_cpu_lc.h"
#include <mali_kbase_pm.h>

/* do not disable the clock in idle, just set the clock to a very low frequency */
#define LC_CLK_ALWAYS_ON	1
/* use vsync information for dvfs, need fb sync support */
#define DVFS_VSYNC_ENABLE	0
/* use temperature control */
#define TEMPERATURE_CTL_ENABLE  1

#define KBASE_LC_GPU_FREQ_KHZ_MAX               624000
#define KBASE_LC_GPU_FREQ_KHZ_MIN               13

#define KBASE_LC_POWER_MANAGEMENT_CALLBACKS     ((uintptr_t)&pm_callbacks)
#define KBASE_LC_PLATFORM_FUNC     				((uintptr_t)&lc_platform_func)
#define KBASE_LC_MEMORY_RESOURCE_DDR            ((uintptr_t)&lt_ddr)

#define KBASE_LC_PM_GPU_POWEROFF_TICK_NS		100000000 /* 100ms */
#define KBASE_LC_PM_POWEROFF_TICK_SHADER		1 /* 100 ms */
#define KBASE_LC_PM_PM_POWEROFF_TICK_GPU		5 /* 500 ms */

#define KBASE_LC_POWER_MANAGEMENT_DVFS_FREQ		150

/* for lc platform debug print */
#define LC_MALI_PRINT(level,...)				if(level<=gpu_dvfs_debuglevel) KBASE_LOG(0,lc_kbdev->dev,__VA_ARGS__)

#define HARD_RESET_AT_POWER_OFF 0

#define GPU_MHZ		1000000
#define GPU_FREQ_IDLE	(13)

#define GPU_CLKFREQ_LEVEL 7

#define GPU_CLKFREQ_ENABLE MALI_DVFS_ENABLE
#if defined(MALI_HIGH_FREQ)
#define GPU_MAX_CLKFREQ_INDEX 6
#define GPU_MIN_CLKFREQ_INDEX 2
#define GPU_BOOST_CLKFREQ_INDEX 4
#define GPU_BOOST_DURATION 8
#elif defined(MALI_DEFAULT_FREQ)
#define GPU_MAX_CLKFREQ_INDEX 5
#define GPU_MIN_CLKFREQ_INDEX 1
#define GPU_BOOST_CLKFREQ_INDEX 3
#define GPU_BOOST_DURATION 5
#elif defined(MALI_LOW_FREQ)
#define GPU_MAX_CLKFREQ_INDEX 4
#define GPU_MIN_CLKFREQ_INDEX 0
#define GPU_BOOST_CLKFREQ_INDEX 2
#define GPU_BOOST_DURATION 3
#endif
#define GPU_CLKFREQ_INITLEVEL 1    //init clock frequency

#define GPU_VOLT_ENABLE	MALI_VOLT_ENABLE

typedef enum lc_mali_power_mode_tag
{
	LC_MALI_POWER_OFF,  		/**< Power Mali off */
	LC_MALI_POWER_ON,          /**< Power Mali on */
} lc_mali_power_mode;

typedef enum lc_mali_debug_level_tag
{
	LC_MALI_LEVEL_ERROR,
	LC_MALI_LEVEL_WARN,
	LC_MALI_LEVEL_INFO,
	LC_MALI_LEVEL_DEBUG,
	LC_MALI_LEVEL_VERBOSE
} lc_mali_debug_level;

typedef enum lc_mali_power_perf_level {
    LC_POWER_GPU_NO_CONTROL = 0,
    LC_POWER_GPU_LOW_PERFORMANCE_LEVEL = 1,
    LC_POWER_GPU_STANDARD_LEVEL,
    LC_POWER_GPU_HIGH_PERFORMANCE_LEVEL
} lc_mali_power_perf_level;

static u32 clkfreq_table[GPU_CLKFREQ_LEVEL] = {156, 234, 273, 312, 416, 546, 624};
#if MALI_VOLT_ENABLE
static u32 clkvolt_table[GPU_CLKFREQ_LEVEL] = {1, 1, 1, 1, 2, 3, 3};
#endif
static s32 current_clkfreq_index = -1;
static u32 enable_dvfs = 0;
static u32 max_clkfreq = 0;
static u32 max_clkfreq_index = 0;
static u32 min_clkfreq = 0;
static u32 min_clkfreq_index = 0;
static lc_mali_power_perf_level curr_perf_level = 0;
static u32 upthreshold_vsync[GPU_CLKFREQ_LEVEL] = {80, 80, 85, 90, 95, 95, 95};
static u32 downthreshold_vsync[GPU_CLKFREQ_LEVEL] = {55, 55, 55, 55, 65, 75, 85};
static u32 upthreshold_emerg_vsync[GPU_CLKFREQ_LEVEL] = {95, 95, 95, 95, 99, 99, 99};
static u32 downthreshold_emerg_vsync[GPU_CLKFREQ_LEVEL] = {30, 30, 30, 30, 40, 40, 70};
static u32 upthreshold_novsync[GPU_CLKFREQ_LEVEL] = {40, 40, 40, 50, 90, 98, 98};
static u32 downthreshold_novsync[GPU_CLKFREQ_LEVEL] = {15, 15, 15, 15, 25, 35, 90};
static u32 upthreshold_emerg_novsync[GPU_CLKFREQ_LEVEL] = {80, 80, 80, 90, 99, 99, 99};
static u32 downthreshold_emerg_novsync[GPU_CLKFREQ_LEVEL] = {10, 10, 10, 10, 20, 30, 40};
static u32 gpu_dvfs_debuglevel = 0;
static u32 gpu_perf_level = 0;

static u32 gpu_temp_control = 0;
static u32 gpu_boost_freq = 0;

static u32 last_clkfreq = 0;

spinlock_t dvfs_lock;

struct kbase_device *lc_kbdev;

#ifndef CONFIG_OF
static struct kbase_io_resources io_resources = {
	.job_irq_number = INT_GPU_JOB,
	.mmu_irq_number = INT_GPU_MMU,
	.gpu_irq_number = INT_GPU_GPU,
	.io_memory_region = {
			     .start = GPU_BASE,
			     .end = GPU_BASE + (4096 * 4) - 1}
};
#endif

static u32 bEnableClk = 0;
static u32 bMaliInited = 0;

static struct clk *mclk = NULL;
static struct clk *peri_pclk = NULL;
static struct clk *adb_mclk = NULL;

static u32 get_clkfreq_index(u32 freq);
static mali_bool mali_clk_get(void)
{
	if (mclk == NULL) {
		mclk = clk_get(NULL, "gpu_mclk");

		if (IS_ERR(mclk)) {
			LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "failed to get source mali clock\n");
			return MALI_FALSE;
		}
	}

	if (peri_pclk == NULL) {
		peri_pclk = clk_get(NULL, "peri_sw0_gpu_clk");

		if (IS_ERR(peri_pclk)) {
			LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "failed to get source peri_pclk\n");
			return MALI_FALSE;
		}
	}

	if (adb_mclk == NULL) {
		adb_mclk = clk_get(NULL, "gpu_adb_aclkm");

		if (IS_ERR(adb_mclk)) {
			LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "failed to get source adb_mclk\n");
			return MALI_FALSE;
		}
	}

	return MALI_TRUE;
}

static void mali_clk_put(void)
{
	if (adb_mclk) {
		clk_put(adb_mclk);
		adb_mclk = NULL;
	}

	if (peri_pclk) {
		clk_put(peri_pclk);
		peri_pclk = NULL;
	}

	if (mclk) {
		clk_put(mclk);
		mclk = NULL;
	}
}

#ifdef CONFIG_THERMAL_COMIP
static int mali_hotctl_callback(struct notifier_block *p, unsigned long event, void *data)
{
    struct thermal_data *tmpdata = (struct thermal_data *)data;
    LC_MALI_PRINT(LC_MALI_LEVEL_DEBUG, "mali_hotctl_callback event %ld temperature %d\n", event, tmpdata->temperature);

    if(gpu_temp_control)
    {
        if(event == THERMAL_NORMAL) {
            max_clkfreq = clkfreq_table[max_clkfreq_index];
        }
        else if(event == THERMAL_WARM) {
            max_clkfreq = clkfreq_table[MAX(get_clkfreq_index(min_clkfreq),max_clkfreq_index-1)];
        }
        else if(event == THERMAL_HOT) {
            max_clkfreq = clkfreq_table[MAX(get_clkfreq_index(min_clkfreq),max_clkfreq_index-2)];
        }
        else if(event == THERMAL_CRITICAL) {
            max_clkfreq = min_clkfreq;
        }
        else if(event == THERMAL_FATAL)	{
            max_clkfreq = min_clkfreq;
        }
    }

	return NOTIFY_DONE;
}

static struct notifier_block malifreq_hot_notifier_block =
{
	.notifier_call = mali_hotctl_callback,
};
#endif

static int mali_power_change(lc_mali_power_mode power_mode)
{
	int result = 1;
	int reg_val;
	int val;
	int check_count = 100;
	unsigned long flags;

	spin_lock_irqsave(&dvfs_lock, flags);
	switch (power_mode) {
		case LC_MALI_POWER_ON:
		    if(bEnableClk == 0) {
				set_cur_voltage_level(GPU_CORE, clkvolt_table[current_clkfreq_index]);
#if LC_CLK_ALWAYS_ON
				clk_set_rate(mclk, clkfreq_table[current_clkfreq_index] * GPU_MHZ);
#endif
		        clk_enable(mclk);
		        clk_enable(peri_pclk);
		        clk_enable(adb_mclk);

				/* power up whole GPU */
				reg_val = (1<<AP_PWR_CORE_WK_UP_WE) | (1<<AP_PWR_CORE_WK_UP);
				writel(reg_val ,(void __iomem *)io_p2v(AP_PWR_GPU_PD_CTL));

				do {
					val = readl(io_p2v(AP_PWR_PDFSM_ST2));
					if((val & 0x7) == 0)
					{
						bEnableClk = 1;
						result = 0;
						break;
					}
					/* delay for a while to check next value */
					udelay(10);
				} while(check_count--);
		    }
			else
				result = 0;
			break;
		case LC_MALI_POWER_OFF:
			if(bEnableClk) {
                bEnableClk = 0;

				/* power off whole GPU */
				reg_val = (1<<AP_PWR_CORE_PD_MK_WE) | (1<<AP_PWR_CORE_PD_EN_WE);
				reg_val |= (1<<AP_PWR_CORE_PD_EN);
				reg_val &= ~(1<<AP_PWR_CORE_PD_MK);
				writel(reg_val ,(void __iomem *)io_p2v(AP_PWR_GPU_PD_CTL));

				do {
					val = readl(io_p2v(AP_PWR_PDFSM_ST2));
					if((val & 0x7) == 0x7)
					{
						result = 0;
						break;
					}
					/* delay for a while to check next value */
					udelay(10);
				} while(check_count--);

				clk_disable(adb_mclk);
				clk_disable(peri_pclk);
#if LC_CLK_ALWAYS_ON
				clk_set_rate(mclk, GPU_FREQ_IDLE * GPU_MHZ);
#else
				clk_disable(mclk);
#endif
				/* set gpu buck3 voltage */
				set_cur_voltage_level(GPU_CORE, clkvolt_table[0]);
			}
			else
				result = 0;
			break;
		default:
			break;
	}
	spin_unlock_irqrestore(&dvfs_lock, flags);
    LC_MALI_PRINT(LC_MALI_LEVEL_VERBOSE, "mali_power_change power_mode %d result %d\n", power_mode, result);
	return result;
}

static void mali_gpu_clk_set(u32 gpu_freq)
{
    if(mclk) {
		if(bEnableClk) {
		#if GPU_VOLT_ENABLE
			if(gpu_freq < last_clkfreq) {
				clk_set_rate(mclk, gpu_freq * GPU_MHZ);
				set_cur_voltage_level(GPU_CORE, clkvolt_table[get_clkfreq_index(gpu_freq)]);
			} else {
				set_cur_voltage_level(GPU_CORE, clkvolt_table[get_clkfreq_index(gpu_freq)]);
				clk_set_rate(mclk, gpu_freq * GPU_MHZ);
			}
		#else
			clk_set_rate(mclk, gpu_freq * GPU_MHZ);
		#endif
		}

        LC_MALI_PRINT(LC_MALI_LEVEL_DEBUG, "clk_set_rate gpu_freq %dMHz last_clkfreq %dMHz\n", gpu_freq, last_clkfreq);
		last_clkfreq = gpu_freq;
    }
    return;
}

static u32 get_clkfreq_index(u32 freq)
{
    int i = 0;
    if(freq >= clkfreq_table[GPU_CLKFREQ_LEVEL - 1]) {
        return GPU_CLKFREQ_LEVEL - 1;
    }

    while(freq > clkfreq_table[i++]);

    return i-1;
}

static ssize_t show_temp_ctrl(struct device *dev, struct device_attribute *attr, char *const buf)
{
	return sprintf(buf, "%u\n", gpu_temp_control);
}

static ssize_t set_temp_ctrl(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long unsigned int val;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	gpu_temp_control = val;

	return count;
}

DEVICE_ATTR(temperature_control, 0660, show_temp_ctrl, set_temp_ctrl);

static ssize_t show_dvfs_ctrl(struct device *dev, struct device_attribute *attr, char *const buf)
{
	return sprintf(buf, "%u\n", enable_dvfs);
}

static ssize_t set_dvfs_ctrl(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long unsigned int val;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	enable_dvfs = val;

	return count;
}

DEVICE_ATTR(dvfs_control, 0660, show_dvfs_ctrl, set_dvfs_ctrl);

static ssize_t show_clk_freq(struct device *dev, struct device_attribute *attr, char *const buf)
{
	return sprintf(buf, "%u\n", clkfreq_table[current_clkfreq_index]);
}

static ssize_t set_clk_freq(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long unsigned int val;
    u32 target_clk_index;
	unsigned long flags;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	target_clk_index = get_clkfreq_index(val);
	target_clk_index = MAX(MIN(target_clk_index, get_clkfreq_index(max_clkfreq)), get_clkfreq_index(min_clkfreq));

	spin_lock_irqsave(&dvfs_lock, flags);
	if(current_clkfreq_index != target_clk_index) {
		current_clkfreq_index = target_clk_index;
		mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
	}
	spin_unlock_irqrestore(&dvfs_lock, flags);

	return count;
}

DEVICE_ATTR(clk_frequency, 0660, show_clk_freq, set_clk_freq);

static ssize_t show_debug_level(struct device *dev, struct device_attribute *attr, char *const buf)
{
	return sprintf(buf, "%u\n", gpu_dvfs_debuglevel);
}

static ssize_t set_debug_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long unsigned int val;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	gpu_dvfs_debuglevel = val;

	return count;
}

DEVICE_ATTR(debug_level, 0660, show_debug_level, set_debug_level);

static ssize_t set_boost_freq(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long unsigned int val;
    u32 target_clk_index;
	unsigned long flags;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	if(enable_dvfs) {
        target_clk_index = MAX(MIN((val==1)?GPU_BOOST_CLKFREQ_INDEX:(GPU_BOOST_CLKFREQ_INDEX-1), get_clkfreq_index(max_clkfreq)), min_clkfreq_index);
        spin_lock_irqsave(&dvfs_lock, flags);
        if(current_clkfreq_index != target_clk_index) {
            current_clkfreq_index = target_clk_index;
            mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
        }
        gpu_boost_freq = GPU_BOOST_DURATION;
        spin_unlock_irqrestore(&dvfs_lock, flags);
	}

	return count;
}

DEVICE_ATTR(boost_frequency, 0220, NULL, set_boost_freq);

static ssize_t show_perf_level(struct device *dev, struct device_attribute *attr, char *const buf)
{
	return sprintf(buf, "%u\n", gpu_perf_level);
}

static ssize_t set_perf_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long unsigned int val;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	gpu_perf_level = val;

    switch(gpu_perf_level) {
        case LC_POWER_GPU_HIGH_PERFORMANCE_LEVEL:
            max_clkfreq_index = MIN(GPU_MAX_CLKFREQ_INDEX+1, GPU_CLKFREQ_LEVEL-1);
            min_clkfreq_index = MIN(GPU_MIN_CLKFREQ_INDEX+1, max_clkfreq_index);

            max_clkfreq = clkfreq_table[max_clkfreq_index];
            min_clkfreq = clkfreq_table[min_clkfreq_index];
            break;

        case LC_POWER_GPU_STANDARD_LEVEL:
            max_clkfreq_index = GPU_MAX_CLKFREQ_INDEX;
            min_clkfreq_index = GPU_MIN_CLKFREQ_INDEX;

            max_clkfreq = clkfreq_table[max_clkfreq_index];
            min_clkfreq = clkfreq_table[min_clkfreq_index];
            break;

        case LC_POWER_GPU_LOW_PERFORMANCE_LEVEL:
            min_clkfreq_index = MAX(GPU_MIN_CLKFREQ_INDEX, 1) - 1;
            max_clkfreq_index = MAX(GPU_MAX_CLKFREQ_INDEX, min_clkfreq_index+1) - 1;

            max_clkfreq = clkfreq_table[max_clkfreq_index];
            min_clkfreq = clkfreq_table[min_clkfreq_index];
            break;
    }

	LC_MALI_PRINT(LC_MALI_LEVEL_INFO, "set_perf_level gpu_perf_level %d\n", gpu_perf_level);

	return count;
}

DEVICE_ATTR(perf_level, 0660, show_perf_level, set_perf_level);

static mali_bool lc_platform_init(struct kbase_device *kbdev)
{
	LC_MALI_PRINT(LC_MALI_LEVEL_INFO, "mali init\n");

	if(bMaliInited) {
		return MALI_TRUE;
	}

	lc_kbdev = kbdev;

	bEnableClk = 0;

	if (!mali_clk_get()) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Failed to get Mali clock\n");
		goto err_clk;
	}

	if (device_create_file(kbdev->dev, &dev_attr_temperature_control)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Couldn't create temperature_control sysfs file\n");
		goto err_clk;
	}

	if (device_create_file(kbdev->dev, &dev_attr_dvfs_control)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Couldn't create dvfs_control sysfs file\n");
		goto temp_fs;
	}

	if (device_create_file(kbdev->dev, &dev_attr_clk_frequency)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Couldn't create clk_frequency sysfs file\n");
		goto dvfs_fs;
	}

	if (device_create_file(kbdev->dev, &dev_attr_debug_level)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Couldn't create debug_level sysfs file\n");
		goto clkfreq_fs;
	}

	if (device_create_file(kbdev->dev, &dev_attr_boost_frequency)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Couldn't create boost_frequency sysfs file\n");
		goto debug_fs;
	}

	if (device_create_file(kbdev->dev, &dev_attr_perf_level)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Couldn't create perf_level sysfs file\n");
		goto boost_fs;
	}

	last_clkfreq = clkfreq_table[GPU_CLKFREQ_INITLEVEL];
	current_clkfreq_index = GPU_CLKFREQ_INITLEVEL;
#if LC_CLK_ALWAYS_ON
    mali_gpu_clk_set(GPU_FREQ_IDLE);
#else
    mali_gpu_clk_set(clkfreq_table[GPU_CLKFREQ_INITLEVEL]);
#endif

    max_clkfreq = clkfreq_table[GPU_MAX_CLKFREQ_INDEX];
	min_clkfreq = clkfreq_table[GPU_MIN_CLKFREQ_INDEX];

	max_clkfreq_index = GPU_MAX_CLKFREQ_INDEX;
	min_clkfreq_index = GPU_MIN_CLKFREQ_INDEX;

    curr_perf_level = LC_POWER_GPU_STANDARD_LEVEL;

    enable_dvfs = GPU_CLKFREQ_ENABLE;
	gpu_dvfs_debuglevel = LC_MALI_LEVEL_INFO;

    gpu_temp_control = TEMPERATURE_CTL_ENABLE;
    gpu_boost_freq = 0;

    gpu_perf_level = LC_POWER_GPU_STANDARD_LEVEL;

	spin_lock_init(&dvfs_lock);

	bMaliInited = 1;

#ifdef CONFIG_THERMAL_COMIP
	thermal_notifier_register(&malifreq_hot_notifier_block);
#endif

	return MALI_TRUE;
boost_fs:
  	device_remove_file(kbdev->dev, &dev_attr_boost_frequency);
debug_fs:
  	device_remove_file(kbdev->dev, &dev_attr_debug_level);
clkfreq_fs:
	device_remove_file(kbdev->dev, &dev_attr_clk_frequency);
dvfs_fs:
	device_remove_file(kbdev->dev, &dev_attr_dvfs_control);
temp_fs:
   	device_remove_file(kbdev->dev, &dev_attr_temperature_control);
err_clk:
	mali_clk_put();

	return MALI_FALSE;
}

static void lc_platform_deinit(struct kbase_device *kbdev)
{
	LC_MALI_PRINT(LC_MALI_LEVEL_INFO, "mali deinit\n");
	if(bMaliInited) {
		mali_clk_put();

        device_remove_file(kbdev->dev, &dev_attr_temperature_control);
        device_remove_file(kbdev->dev, &dev_attr_dvfs_control);
        device_remove_file(kbdev->dev, &dev_attr_clk_frequency);
        device_remove_file(kbdev->dev, &dev_attr_debug_level);
        device_remove_file(kbdev->dev, &dev_attr_boost_frequency);
        device_remove_file(kbdev->dev, &dev_attr_perf_level);

#ifdef CONFIG_THERMAL_COMIP
	    thermal_notifier_unregister(&malifreq_hot_notifier_block);
#endif
		bMaliInited = 0;
	}
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	if (mali_power_change(LC_MALI_POWER_ON)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Failed to power on\n");
		return 1;
	}

	return 0;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	if (mali_power_change(LC_MALI_POWER_OFF)) {
		LC_MALI_PRINT(LC_MALI_LEVEL_ERROR, "Failed to power off\n");
	}

#if HARD_RESET_AT_POWER_OFF
	/* Cause a GPU hard reset to test whether we have actually idled the GPU
	 * and that we properly reconfigure the GPU on power up.
	 * Usually this would be dangerous, but if the GPU is working correctly it should
	 * be completely safe as the GPU should not be active at this point.
	 * However this is disabled normally because it will most likely interfere with
	 * bus logging etc.
	 */
	KBASE_TRACE_ADD(kbdev, CORE_GPU_HARD_RESET, NULL, NULL, 0u, 0);
	kbase_os_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET);
#endif
}

static struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback  = NULL,
	.power_resume_callback = NULL,
};

static struct kbase_platform_funcs_conf lc_platform_func = {
	.platform_init_func = lc_platform_init,
	.platform_term_func = lc_platform_deinit,
};

/* Please keep table config_attributes in sync with config_attributes_hw_issue_8408 */
static struct kbase_attribute config_attributes[] = {
	{
        KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
        1000},

	{
		KBASE_CONFIG_ATTR_PM_GPU_POWEROFF_TICK_NS,
		KBASE_LC_PM_GPU_POWEROFF_TICK_NS},

	{
		KBASE_CONFIG_ATTR_PM_POWEROFF_TICK_SHADER,
		KBASE_LC_PM_POWEROFF_TICK_SHADER},

	{
		KBASE_CONFIG_ATTR_PM_POWEROFF_TICK_GPU,
		KBASE_LC_PM_PM_POWEROFF_TICK_GPU},

	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
		KBASE_LC_POWER_MANAGEMENT_CALLBACKS},

	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		KBASE_LC_PLATFORM_FUNC},

	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_DVFS_FREQ,
		KBASE_LC_POWER_MANAGEMENT_DVFS_FREQ},

	{
		KBASE_CONFIG_ATTR_END,
		0}
};

static struct kbase_platform_config versatile_platform_config = {
	.attributes = config_attributes,
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &versatile_platform_config;
}

int kbase_platform_early_init(void)
{
    printk("mali ddk %s early init\n", MALI_RELEASE_NAME);

	/* Nothing needed at this stage */
	return 0;
}

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation, u32 util_gl_share, u32 util_cl_share[2])
{
	unsigned long flags;
	u32 *upthreshold,*downthreshold,*upthreshold_emerg,*downthreshold_emerg;

	spin_lock_irqsave(&dvfs_lock, flags);

	if(utilisation == 0) {
		gpu_boost_freq = 0;
	}

	if(bEnableClk)
	{
	    LC_MALI_PRINT(LC_MALI_LEVEL_VERBOSE, "kbase_platform_dvfs_event utilisation %d vsync_hit %d\n", utilisation, kbdev->pm.metrics.vsync_hit);

		if (DVFS_VSYNC_ENABLE && !kbdev->pm.metrics.vsync_hit) {
			/* VSync is being missed */
			upthreshold = upthreshold_novsync;
			downthreshold = downthreshold_novsync;
			upthreshold_emerg = upthreshold_emerg_novsync;
			downthreshold_emerg = downthreshold_emerg_novsync;
		} else {
			/* VSync is being met */
			upthreshold = upthreshold_vsync;
			downthreshold = downthreshold_vsync;
			upthreshold_emerg = upthreshold_emerg_vsync;
			downthreshold_emerg = downthreshold_emerg_vsync;
		}

		if(enable_dvfs) {
			if(current_clkfreq_index > get_clkfreq_index(max_clkfreq)) {
				current_clkfreq_index = get_clkfreq_index(max_clkfreq);
				mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
			}
			else if(current_clkfreq_index < get_clkfreq_index(min_clkfreq)) {
				current_clkfreq_index = get_clkfreq_index(min_clkfreq);
				mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
			}
	        else {
	            if((utilisation >= upthreshold[current_clkfreq_index]) && (current_clkfreq_index != get_clkfreq_index(max_clkfreq))) {
					/* up the clock frequency */
	                if(utilisation >= upthreshold_emerg[current_clkfreq_index]) {
						if(utilisation == 100)	{
		                    current_clkfreq_index = get_clkfreq_index(max_clkfreq);
						} else {
							/* if gpu utilization is much higher, change the frequency up to 2 level */
		                    current_clkfreq_index += 2;
						}
	                } else {
	                    current_clkfreq_index += 1;
	                }
                    current_clkfreq_index = MIN(current_clkfreq_index, (s32)get_clkfreq_index(max_clkfreq));
	                mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
	            }
	            else if ((utilisation <= downthreshold[current_clkfreq_index]) && (current_clkfreq_index != get_clkfreq_index(min_clkfreq))) {
                    /* down the clock frequency */
                    if(!gpu_boost_freq) {
                        /* if has boost operation, not take down scale this time */
                        if(utilisation <= downthreshold_emerg[current_clkfreq_index]) {
                            if(utilisation == 0) {
                                current_clkfreq_index = get_clkfreq_index(min_clkfreq);
                            } else {
                                /* if gpu utilization is much lower, change the frequency down to 2 level */
                                current_clkfreq_index -= 2;
                            }
                        } else {
                            current_clkfreq_index -= 1;
                        }
                        current_clkfreq_index = MAX(current_clkfreq_index,(s32)get_clkfreq_index(min_clkfreq));

                        mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
                    }
	            }
	        }
	    } else {
	        /* if dvfs is disable, set clock to max frequency */
	        if(current_clkfreq_index != get_clkfreq_index(max_clkfreq)) {
	            current_clkfreq_index = get_clkfreq_index(max_clkfreq);
	            mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
	        }
	    }
	}

	gpu_boost_freq -= gpu_boost_freq?1:0;

	spin_unlock_irqrestore(&dvfs_lock, flags);

    return 0;
}

