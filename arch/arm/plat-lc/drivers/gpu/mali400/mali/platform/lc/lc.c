/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012-2013 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"

#include "lc_pmm.h"

#include <plat/comip-memory.h>

#define GPU_RUNTIME_SUSPEND_DELAY 1000

static void mali_platform_device_release(struct device *device);
static int mali_os_suspend(struct device *device);
static int mali_os_resume(struct device *device);
static int mali_os_freeze(struct device *device);
static int mali_os_thaw(struct device *device);
#ifdef CONFIG_PM_RUNTIME
static int mali_runtime_suspend(struct device *device);
static int mali_runtime_resume(struct device *device);
static int mali_runtime_idle(struct device *device);
#endif

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);

static struct dev_pm_ops mali_gpu_device_type_pm_ops =
{
	.suspend = mali_os_suspend,
	.resume = mali_os_resume,
	.freeze = mali_os_freeze,
	.thaw = mali_os_thaw,
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = mali_runtime_suspend,
	.runtime_resume = mali_runtime_resume,
	.runtime_idle = mali_runtime_idle,
#endif
};

static struct device_type mali_gpu_device_device_type =
{
	.pm = &mali_gpu_device_type_pm_ops,
};

static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.dev.release = mali_platform_device_release,
	.dev.coherent_dma_mask = DMA_BIT_MASK(32),	
	/*
	 * We temporarily make use of a device type so that we can control the Mali power
	 * from within the mali.ko (since the default platform bus implementation will not do that).
	 * Ideally .dev.pm_domain should be used instead, as this is the new framework designed
	 * to control the power of devices.
	 */
	.dev.type = &mali_gpu_device_device_type, /* We should probably use the pm_domain instead of type on newer kernels */
};

#define MALI_BASE_IRQ		32

#define MALI_GP_IRQ		(MALI_BASE_IRQ + 19)
#define MALI_PP0_IRQ		(MALI_BASE_IRQ + 20)
#define MALI_PP1_IRQ		(MALI_BASE_IRQ + 21)
#define MALI_GP_MMU_IRQ		(MALI_BASE_IRQ + 22)
#define MALI_PP0_MMU_IRQ	(MALI_BASE_IRQ + 23)
#define MALI_PP1_MMU_IRQ	(MALI_BASE_IRQ + 24)

static struct resource mali_gpu_resources_m400_mp2[] =
{
	MALI_GPU_RESOURCES_MALI400_MP2_PMU(0xA2010000,
					MALI_GP_IRQ, MALI_GP_MMU_IRQ,
					MALI_PP0_IRQ, MALI_PP0_MMU_IRQ,
					MALI_PP1_IRQ, MALI_PP1_MMU_IRQ)
};

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = 1024 * 1024 * 1024, /* 1024MB just for memery map */
	.fb_start = 0,
	.fb_size = 2UL * 1024 * 1024 * 1024,
	.max_job_runtime = 60000, /* 60 seconds */
	.utilization_interval = 400, /* 400ms */
	.utilization_callback = mali_gpu_utilization_callback,
	.pmu_switch_delay = 0xFF, /* do not have to be this high on FPGA, but it is good for testing to have a delay */
};

int mali_platform_device_register(void)
{
	int err = -1;
	int num_pp_cores = 0;

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));

	err = platform_device_add_resources(&mali_gpu_device, mali_gpu_resources_m400_mp2, sizeof(mali_gpu_resources_m400_mp2) / sizeof(mali_gpu_resources_m400_mp2[0]));
	if (0 == err) {
		if (FB_MEMORY_FIX) {
			mali_gpu_data.fb_start = FB_MEMORY_BASE;
			mali_gpu_data.fb_size = FB_MEMORY_SIZE * FB_MEMORY_NUM;
		} else {
			if (KERNEL_MEMORY2_SIZE > 0) {
				mali_gpu_data.fb_start = KERNEL_MEMORY_BASE;
				mali_gpu_data.fb_size = KERNEL_MEMORY2_BASE +
					KERNEL_MEMORY2_SIZE - KERNEL_MEMORY_BASE;
			} else {
				mali_gpu_data.fb_start = KERNEL_MEMORY_BASE;
				mali_gpu_data.fb_size = KERNEL_MEMORY_SIZE;
			}
		}
		err = platform_device_add_data(&mali_gpu_device, &mali_gpu_data, sizeof(mali_gpu_data));
		MALI_DEBUG_PRINT(4, ("platform_device_add_data error %d\n", err));
		if (0 == err) {
			/* Register the platform device */
			err = platform_device_register(&mali_gpu_device);
			MALI_DEBUG_PRINT(4, ("platform_device_register error %d\n", err));
			if (0 == err) {
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
				pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), GPU_RUNTIME_SUSPEND_DELAY);
				pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
				pm_runtime_enable(&(mali_gpu_device.dev));
#endif
				mali_platform_init();

				num_pp_cores = 2;
				mali_core_scaling_init(num_pp_cores);

				return 0;
			}
		}

		platform_device_unregister(&mali_gpu_device);
	}

	return err;
}

void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	mali_core_scaling_term();
	mali_platform_deinit();
	platform_device_unregister(&mali_gpu_device);
}

static void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

static int mali_os_suspend(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_suspend() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->suspend(device);
	}

	mali_platform_power_mode_change(MALI_POWER_MODE_DEEP_SLEEP);

	return ret;
}

static int mali_os_resume(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_resume() called\n"));

	mali_platform_power_mode_change(MALI_POWER_MODE_ON);

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->resume(device);
	}

	return ret;
}

static int mali_os_freeze(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_freeze() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->freeze)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->freeze(device);
	}

	return ret;
}

static int mali_os_thaw(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_os_thaw() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->thaw)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->thaw(device);
	}

	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int mali_runtime_suspend(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_runtime_suspend() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_suspend)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_suspend(device);
	}

	mali_platform_power_mode_change(MALI_POWER_MODE_LIGHT_SLEEP);

	return ret;
}

static int mali_runtime_resume(struct device *device)
{
	int ret = 0;

	MALI_DEBUG_PRINT(4, ("mali_runtime_resume() called\n"));

	mali_platform_power_mode_change(MALI_POWER_MODE_ON);

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_resume)
	{
		/* Need to notify Mali driver about this event */
		ret = device->driver->pm->runtime_resume(device);
	}

	return ret;
}

static int mali_runtime_idle(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_runtime_idle() called\n"));

	if (NULL != device->driver &&
	    NULL != device->driver->pm &&
	    NULL != device->driver->pm->runtime_idle)
	{
		/* Need to notify Mali driver about this event */
		int ret = device->driver->pm->runtime_idle(device);
		if (0 != ret)
		{
			return ret;
		}
	}

	pm_runtime_suspend(device);

	return 0;
}
#endif

