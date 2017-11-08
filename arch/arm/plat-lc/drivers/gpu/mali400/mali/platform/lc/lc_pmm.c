/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
/*******************************************************************************
* CR History:
********************************************************************************
*  1.0.0     L1810_Bug00000063  	   zhuxiaoping     2012-5-30   fix gpu clk bug
*  1.0.1     L1810_Enh00000367  	   zhuxiaoping     2012-10-26  add gpu dynamic frequency scaling
*  1.0.2     L1810_Enh00000738  	   zhuxiaoping     2013-1-18   add dvfs interface for enable/disable
********************************************************************************/

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include <linux/mali/mali_utgard.h>
#include "lc_pmm.h"
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <plat/comip-thermal.h>
#include <plat/clock.h>
#include <plat/hardware.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#endif
#include "linux/mali/mali_utgard.h"

#define MAX(a, b)       ((a) > (b) ?  (a) : (b))
#define MIN(a, b)       ((a) < (b) ?  (a) : (b))
#if defined(CONFIG_ARCH_LC181X) && defined(DDR_AXI_GPV_BASE)
#define DDR_AXI_GPV_MAX_OT_GPU	(DDR_AXI_GPV_BASE + 0x46110)
#endif
#define USING_INPUT_EVENT 1
#define INPUT_EVENT_FREQ_INDEX  3

#if USING_MALI_PMU
#define DISABLE_CLK_IN_LIGHTSLEEP 1
#else
#define DISABLE_CLK_IN_LIGHTSLEEP 0
#endif

#define GPU_MHZ		1000000

#define GPU_FREQ_MIN	(26)

/* FIX L1810_Enh00000367 BEGIN DATE:2012-10-26 AUTHOR NAME ZHUXIAOPING */
/* FIX L1810_Enh00000738 BEGIN DATE:2013-1-18 AUTHOR NAME ZHUXIAOPING */
#define GPU_CLKFREQ_LEVEL 6
#define GPU_CLKFREQ_INITLEVEL 2    //init clock frequency

#define GPU_CLKFREQ_ENABLE 1
#define GPU_MAX_CLKFREQ_INDEX 4
#define GPU_MIN_CLKFREQ_INDEX 1

static struct clk *mclk = NULL;
static struct clk *peri_pclk = NULL;

static u32 bEnableClk = 0;
static u32 bMaliInited = 0;
static u32 bDVFSRegister = 0;

static u32 clkfreq_table[GPU_CLKFREQ_LEVEL] = {156, 208, 260, 312, 364, 416};
static s32 current_clkfreq_index = -1;
static u32 enable_dvfs = 0;
static u32 max_clkfreq = 0;
static u32 max_clkfreq_index = 0;
static u32 min_clkfreq = 0;
static u32 upthreshold[GPU_CLKFREQ_LEVEL] = {92, 92, 92, 94, 94, 94};
static u32 downthreshold[GPU_CLKFREQ_LEVEL] = {72, 72, 72, 76, 82, 84};
static u32 upthreshold_emerg[GPU_CLKFREQ_LEVEL] = {94, 94, 96, 96, 96, 96};
static u32 downthreshold_emerg[GPU_CLKFREQ_LEVEL] = {60, 60, 60, 60, 70, 80};
static u32 gpu_dvfs_debuglevel = 0;
static u32 last_clkfreq = 0;

static u32 read_outstanding[GPU_CLKFREQ_LEVEL] = {3, 3, 4, 5, 5, 6};

static u32 is_on2_enable = 0;
static u32 read_outstanding_with_on2[GPU_CLKFREQ_LEVEL] = {1, 2, 3, 3, 4, 4};

static _mali_osk_spinlock_t *clk_freq_lock = NULL;

/* for core scaling */
static int num_cores_total;
static int num_cores_enabled;
static u32 core_upthreshold = 60;
static u32 core_downthreshold = 50;

static struct work_struct wq_work;

static void set_num_cores(struct work_struct *work);
static void enable_one_core(void);
static void disable_one_core(void);

#if USING_INPUT_EVENT
static void enable_max_num_cores(void);
#endif

static u32 hasInputEvent = 0;
#if USING_INPUT_EVENT
struct malifreq_dvfs_inputopen {
	struct input_handle *handle;
	struct work_struct inputopen_work;
};

static struct malifreq_dvfs_inputopen inputopen = {0};
static struct workqueue_struct *down_wq = NULL;

/*
 * Pulsed boost on input event raises GPU to hispeed_freq and lets
 * usual algorithm of min_sample_time  decide when to allow speed
 * to drop.
 */
static void malifreq_dvfs_input_event(struct input_handle *handle,
					    unsigned int type,
					    unsigned int code, int value)
{
	if (type == EV_SYN && code == SYN_REPORT) {
		if(enable_dvfs)	{
			mali_gpu_set_clk_index(MAX(current_clkfreq_index, INPUT_EVENT_FREQ_INDEX));
			enable_max_num_cores();
			hasInputEvent = 1;
		}
	}
}

static void malifreq_dvfs_input_open(struct work_struct *w)
{
	struct malifreq_dvfs_inputopen *io =
		container_of(w, struct malifreq_dvfs_inputopen,
			     inputopen_work);
	int error;
    MALI_DEBUG_PRINT(1,("%s line %d\n", __func__,__LINE__));

	error = input_open_device(io->handle);
	if (error)
		input_unregister_handle(io->handle);
}

static int malifreq_dvfs_input_connect(struct input_handler *handler,
					     struct input_dev *dev,
					     const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

    MALI_DEBUG_PRINT(1,("%s: connect to %s\n", __func__, dev->name));

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "malifreq_dvfs";

	error = input_register_handle(handle);
	if (error)
		goto err;

	inputopen.handle = handle;
	queue_work(down_wq, &inputopen.inputopen_work);

	return 0;
err:
	kfree(handle);
	return error;
}

static void malifreq_dvfs_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id malifreq_dvfs_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			    BIT_MASK(ABS_MT_POSITION_X) |
			    BIT_MASK(ABS_MT_POSITION_Y) },
	}, /* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			    BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	}, /* touchpad */
	{ },
};

static struct input_handler malifreq_dvfs_input_handler = {
	.event          = malifreq_dvfs_input_event,
	.connect        = malifreq_dvfs_input_connect,
	.disconnect     = malifreq_dvfs_input_disconnect,
	.name           = "malifreq_dvfs",
	.id_table       = malifreq_dvfs_ids,
};
#endif

static mali_bool mali_clk_get(void)
{
	if (mclk == NULL) {
		mclk = clk_get(NULL, "gpu_clk");

		if (IS_ERR(mclk)) {
			MALI_PRINT_ERROR( ("failed to get source mali clock\n"));
			return MALI_FALSE;
		}
	}

	if (peri_pclk == NULL) {
		peri_pclk = clk_get(NULL, "peri_gpu_pclk");

		if (IS_ERR(peri_pclk)) {
			MALI_PRINT_ERROR( ("failed to get source peri_pclk\n"));
			return MALI_FALSE;
		}
	}

	return MALI_TRUE;
}

static void mali_clk_put(void)
{
	if (peri_pclk) {
		clk_put(peri_pclk);
		peri_pclk = NULL;
	}

	if (mclk) {
		clk_put(mclk);
		mclk = NULL;
	}
}

#if CONFIG_THERMAL_COMIP
static int mali_hotctl_callback(struct notifier_block *p, unsigned long event, void *data)
{
	struct thermal_data *tmpdata = (struct thermal_data *)data;
    MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1,("mali_hotctl_callback event %ld temperature %d\n", event, tmpdata->temperature));

	if(enable_dvfs) {
		if(event == THERMAL_NORMAL) {
			max_clkfreq = clkfreq_table[max_clkfreq_index];
		}
		else if(event == THERMAL_WARM) {
			max_clkfreq = clkfreq_table[MAX(get_clkfreq_index(min_clkfreq),max_clkfreq_index-1)];
		}
		else if(event == THERMAL_HOT) {
			max_clkfreq = clkfreq_table[MIN(get_clkfreq_index(min_clkfreq)+1,max_clkfreq_index)];
		}
		else if(event == THERMAL_CRITICAL) {
			max_clkfreq = min_clkfreq;
		}
		else if(event == THERMAL_FATAL) {
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

_mali_osk_errcode_t mali_platform_init(void)
{
	if(bMaliInited) {
		MALI_SUCCESS;
	}

	if (!mali_clk_get()) {
		MALI_PRINT_ERROR(("Error: Failed to get Mali clock\n"));
		goto err_clk;
	}

    current_clkfreq_index = GPU_CLKFREQ_INITLEVEL;
	last_clkfreq = clkfreq_table[GPU_CLKFREQ_INITLEVEL];
    mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);

    max_clkfreq = clkfreq_table[GPU_MAX_CLKFREQ_INDEX];
	min_clkfreq = clkfreq_table[GPU_MIN_CLKFREQ_INDEX];

	max_clkfreq_index = GPU_MAX_CLKFREQ_INDEX;

    /* default to enable dvfs */
    enable_dvfs = GPU_CLKFREQ_ENABLE;

	gpu_dvfs_debuglevel = 3;

	clk_freq_lock = _mali_osk_spinlock_init(_MALI_OSK_LOCKFLAG_ORDERED, _MALI_OSK_LOCK_ORDER_UTILIZATION);
	if (NULL == clk_freq_lock) {
		MALI_PRINT_ERROR(("Error: Failed to init clk_freq_lock\n"));
		goto err_clk;
	}

    MALI_DEBUG_PRINT(1,("mali init clkfreq=%dMHz max_clkfreq=%dMHz min_clkfreq=%dMHz\n", clkfreq_table[current_clkfreq_index], max_clkfreq, min_clkfreq));

	bMaliInited = 1;
	bDVFSRegister = 0;

	is_on2_enable = 0;

	mali_platform_power_mode_change(MALI_POWER_MODE_ON);

#if USING_INPUT_EVENT
	hasInputEvent = 0;
	inputopen.handle = NULL;

	if(!bDVFSRegister) {
		int ret;
		down_wq = alloc_workqueue("mali_dvfs_down", 0, 1);
		if (down_wq) {
			INIT_WORK(&inputopen.inputopen_work, malifreq_dvfs_input_open);

			ret = input_register_handler(&malifreq_dvfs_input_handler);
			if (ret) {
				MALI_PRINT_ERROR(("Error: failed to register input handler ret %d\n",ret));
				destroy_workqueue(down_wq);
				down_wq = NULL;
			} else {
				bDVFSRegister = 1;
			}
		} else {
			MALI_PRINT_ERROR(("Error: Failed to alloc_workqueue\n"));
		}
	}
#endif

#if CONFIG_THERMAL_COMIP
	thermal_notifier_register(&malifreq_hot_notifier_block);
#endif

	MALI_SUCCESS;
err_clk:
	mali_clk_put();

	MALI_ERROR(_MALI_OSK_ERR_FAULT);
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	if(bMaliInited) {
		mali_platform_power_mode_change(MALI_POWER_MODE_DEEP_SLEEP);

#if USING_INPUT_EVENT
		if(bDVFSRegister) {
			input_unregister_handler(&malifreq_dvfs_input_handler);

			destroy_workqueue(down_wq);
			down_wq = NULL;
		}
#endif
		mali_clk_put();

		_mali_osk_spinlock_term(clk_freq_lock);
		clk_freq_lock = NULL;

		bMaliInited = 0;
	    MALI_DEBUG_PRINT(1,("mali deinit\n"));
	}
	MALI_SUCCESS;
}

void mali_core_scaling_init(int num_pp_cores)
{
	INIT_WORK(&wq_work, set_num_cores);

	num_cores_total   = num_pp_cores;
	num_cores_enabled = num_pp_cores;

	/* NOTE: Mali is not fully initialized at this point. */
}

void mali_core_scaling_term(void)
{
	flush_scheduled_work();
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
	switch (power_mode) {
		case MALI_POWER_MODE_ON:
            if(bEnableClk == 0) {
                //clk_enable(mclk);
                //clk_enable(peri_pclk);
                //mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
                clk_set_rate(mclk, clkfreq_table[current_clkfreq_index] * GPU_MHZ);
#if USING_MALI_PMU
				mali_pmu_powerup();
#endif
				_mali_osk_spinlock_lock(clk_freq_lock);
				bEnableClk = 1;
				_mali_osk_spinlock_unlock(clk_freq_lock);

				MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("mali enable clk power up\n"));
			}

			MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1,("mali power up\n"));
			break;
		case MALI_POWER_MODE_LIGHT_SLEEP:
#if DISABLE_CLK_IN_LIGHTSLEEP
            if(bEnableClk) {
				_mali_osk_spinlock_lock(clk_freq_lock);
                bEnableClk = 0;
				_mali_osk_spinlock_unlock(clk_freq_lock);
#if USING_MALI_PMU
                mali_pmu_powerdown();
#endif
                //mali_gpu_clk_set(GPU_FREQ_MIN);
                clk_set_rate(mclk, GPU_FREQ_MIN * GPU_MHZ);

                //clk_disable(mclk);
                //clk_disable(peri_pclk);
                MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("mali light sleep disable clk\n"));
            }
#endif
            MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1,("mali light sleep\n"));
            break;
		case MALI_POWER_MODE_DEEP_SLEEP:
            if(bEnableClk) {
				_mali_osk_spinlock_lock(clk_freq_lock);
                bEnableClk = 0;
				_mali_osk_spinlock_unlock(clk_freq_lock);

#if USING_MALI_PMU
                mali_pmu_powerdown();
#endif
                //mali_gpu_clk_set(GPU_FREQ_MIN);
                clk_set_rate(mclk, GPU_FREQ_MIN * GPU_MHZ);
                //clk_disable(mclk);
                //clk_disable(peri_pclk);

                MALI_DEBUG_PRINT(gpu_dvfs_debuglevel-1,("mali deep sleep disable clk\n"));
            }
            MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("mali deep sleep\n"));
	    	break;
		default:
		break;

   	}

	MALI_SUCCESS;
}
/* FIX L1810_Bug00000063 END DATE:2012-5-30 AUTHOR NAME ZHUXIAOPING */

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
	/* FIX L1810_Enh00000367 BEGIN DATE:2012-10-26 AUTHOR NAME ZHUXIAOPING */
    u32 util_percent_gpu = 100*data->utilization_gpu/255;
    u32 util_percent_pp = 100*data->utilization_pp/255;
    u32 util_percent_gp = 100*data->utilization_gp/255;

	MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1, ("Utilization: (%3d, %3d, %3d), cores enabled: %d/%d current_clkfreq_index %d\n", util_percent_gpu, util_percent_gp, util_percent_pp,
		num_cores_enabled, num_cores_total, current_clkfreq_index));

	_mali_osk_spinlock_lock(clk_freq_lock);
	if(bEnableClk) {
		if(enable_dvfs) {
			if(current_clkfreq_index >  get_clkfreq_index(max_clkfreq)) {
	        	current_clkfreq_index = get_clkfreq_index(max_clkfreq);
	        	mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
	        }
	        else {
	            if((util_percent_gpu >= upthreshold[current_clkfreq_index]) && (current_clkfreq_index < get_clkfreq_index(max_clkfreq))) {
					/* up the clock frequency */
	                if(util_percent_gpu >= upthreshold_emerg[current_clkfreq_index]) {
						if(util_percent_gpu == 100)	{
		                    current_clkfreq_index = get_clkfreq_index(max_clkfreq);
						} else {
							/* if gpu utilization is much higher, change the frequency up to 2 level */
		                    current_clkfreq_index += 2;
						}
	                } else {
	                    current_clkfreq_index += 1;
	                }
                    current_clkfreq_index = MIN(current_clkfreq_index, get_clkfreq_index(max_clkfreq));
	                mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
	            }
	            else if ((util_percent_gpu <= downthreshold[current_clkfreq_index]) && (current_clkfreq_index > get_clkfreq_index(min_clkfreq))) {
					/* down the clock frequency */
					if(!hasInputEvent) {
						/* if has input event, not take down scale operation */
		                if(util_percent_gpu <= downthreshold_emerg[current_clkfreq_index]) {
							if(util_percent_gpu <= 10) {
								current_clkfreq_index = get_clkfreq_index(min_clkfreq);
							} else {
								/* if gpu utilization is much lower, change the frequency down to 2 level */
								current_clkfreq_index -= 2;
							}
 		                } else {
		                    current_clkfreq_index -= 1;
		                }
						current_clkfreq_index = MAX(current_clkfreq_index,get_clkfreq_index(min_clkfreq));
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

		if(current_clkfreq_index == get_clkfreq_index(min_clkfreq) && util_percent_pp < core_downthreshold) {
			disable_one_core();
		} else if(util_percent_pp > core_upthreshold) {
			enable_one_core();
		}
	}
	/* FIX L1810_Enh00000367 END DATE:2012-10-26 AUTHOR NAME ZHUXIAOPING */
	_mali_osk_spinlock_unlock(clk_freq_lock);

	#if USING_INPUT_EVENT
	hasInputEvent = 0;
	#endif
}


void mali_gpu_clk_set(u32 gpu_freq)
{
	u32 reg_val;
    if(mclk) {
		if(gpu_freq < last_clkfreq) {
		#ifdef DDR_AXI_GPV_MAX_OT_GPU
			reg_val = readl((void __iomem *)io_p2v(DDR_AXI_GPV_MAX_OT_GPU));
			reg_val &= ~(0x3f<<24);
			reg_val |= ((is_on2_enable?read_outstanding_with_on2[get_clkfreq_index(gpu_freq)]:read_outstanding[get_clkfreq_index(gpu_freq)])<<24);
			/* set ddr axi gpv gpu max read outstanding */
			writel(reg_val ,(void __iomem *)io_p2v(DDR_AXI_GPV_MAX_OT_GPU));
		#endif
			clk_set_rate(mclk, gpu_freq * GPU_MHZ);
		} else {
			/* set clock rate before up the outstanding */
			clk_set_rate(mclk, gpu_freq * GPU_MHZ);
		#ifdef DDR_AXI_GPV_MAX_OT_GPU
			reg_val = readl((void __iomem *)io_p2v(DDR_AXI_GPV_MAX_OT_GPU));
			reg_val &= ~(0x3f<<24);
			reg_val |= ((is_on2_enable?read_outstanding_with_on2[get_clkfreq_index(gpu_freq)]:read_outstanding[get_clkfreq_index(gpu_freq)])<<24);
			/* set ddr axi gpv gpu max read outstanding */
			writel(reg_val ,(void __iomem *)io_p2v(DDR_AXI_GPV_MAX_OT_GPU));
		#endif
		}

        MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("clk_set_rate gpu_freq %dMHz last_clkfreq %dMHz reg_val %#x\n", gpu_freq, last_clkfreq, reg_val));
		last_clkfreq = gpu_freq;
    }
    return;
}

/* FIX L1810_Enh00000738 BEGIN DATE:2013-1-18 AUTHOR NAME ZHUXIAOPING */
void mali_gpu_dvfs_set(u32 enable, u32 maxfreq)
{
    MALI_DEBUG_PRINT(2,("mali_gpu_dvfs_set dvfs %s maxfreq %dMHz\n", enable?"enable":"disable", maxfreq));

    enable_dvfs = enable;
    max_clkfreq = MAX(maxfreq, min_clkfreq);
	max_clkfreq_index = get_clkfreq_index(max_clkfreq);
    return;
}

void mali_gpu_dvfs_get(u32 *enable, u32 *maxfreq)
{
    MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("mali_gpu_dvfs_get dvfs %s maxfreq %dMHz\n", enable_dvfs?"enable":"disable", max_clkfreq));

    *enable = enable_dvfs;
    *maxfreq = clkfreq_table[max_clkfreq_index];
    return;
}

extern u32 mali_utilization_timeout;
void mali_gpu_configuration_get(mali_gpu_config *gpu_config)
{
    gpu_config->utilization_timeout = mali_utilization_timeout;
    gpu_config->upthreshold = upthreshold[0];
    gpu_config->downthreshold = downthreshold[0];

    MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("mali_gpu_configuration_get utilization_timeout %d sleep_timeout %d upthreshold %d downthreshold %d temperature_threshold[0] %d [1] %d\n",
        gpu_config->utilization_timeout, gpu_config->sleep_timeout, gpu_config->upthreshold, gpu_config->downthreshold));

    return;
}

void mali_gpu_configuration_set(mali_gpu_config gpu_config)
{
    MALI_DEBUG_PRINT(2,("mali_gpu_configuration_set utilization_timeout %d sleep_timeout %d upthreshold %d downthreshold %d temperature_threshold[0] %d [1] %d\n",
        gpu_config.utilization_timeout, gpu_config.sleep_timeout, gpu_config.upthreshold, gpu_config.downthreshold));

    mali_utilization_timeout = gpu_config.utilization_timeout;
    upthreshold[0] = gpu_config.upthreshold;
    downthreshold[0] = gpu_config.downthreshold;

    return;
}

void mali_gpu_dvfs_debuglevel_set(u32 debug_level)
{
    MALI_DEBUG_PRINT(2,("mali_gpu_dvfs_debuglevel_set set debug_level %d\n", debug_level));

	gpu_dvfs_debuglevel = debug_level;

    return;
}
/* FIX L1810_Enh00000738 END DATE:2013-1-18 AUTHOR NAME ZHUXIAOPING */

void mali_gpu_set_clk_index(u32 clk_index)
{
	u32 cur_clk = MAX(MIN(clk_index, get_clkfreq_index(max_clkfreq)), get_clkfreq_index(min_clkfreq));
	_mali_osk_spinlock_lock(clk_freq_lock);
	if(current_clkfreq_index != cur_clk) {
		current_clkfreq_index = cur_clk;
		mali_gpu_clk_set(clkfreq_table[current_clkfreq_index]);
	    MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("mali_gpu_set_clk_index gpu_freq %dMHz\n", clkfreq_table[cur_clk]));
	}
	_mali_osk_spinlock_unlock(clk_freq_lock);

	return;
}

void mali_gpu_get_clk_index(u32 *clk_index)
{
	_mali_osk_spinlock_lock(clk_freq_lock);
	*clk_index = current_clkfreq_index;
	_mali_osk_spinlock_unlock(clk_freq_lock);
    MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1,("mali_gpu_get_clk_index gpu_freq %dMHz\n", *clk_index));

	return;
}

u32 get_clkfreq_index(u32 freq)
{
    int i = 0;
    if(freq >= clkfreq_table[GPU_CLKFREQ_LEVEL - 1]) {
        return GPU_CLKFREQ_LEVEL - 1;
    }

    while(freq > clkfreq_table[i++]);

    return i-1;
}

void mali_on2_status_set(u32 enable) {
#ifdef DDR_AXI_GPV_MAX_OT_GPU
	u32 reg_val;
    if(mclk) {
		_mali_osk_spinlock_lock(clk_freq_lock);

		is_on2_enable = enable;

		reg_val = readl((void __iomem *)io_p2v(DDR_AXI_GPV_MAX_OT_GPU));
		reg_val &= ~(0x3f<<24);
		reg_val |= ((is_on2_enable?read_outstanding_with_on2[current_clkfreq_index]:read_outstanding[current_clkfreq_index])<<24);
		/* set ddr axi gpv gpu max read outstanding */
		writel(reg_val ,(void __iomem *)io_p2v(DDR_AXI_GPV_MAX_OT_GPU));

		_mali_osk_spinlock_unlock(clk_freq_lock);
	    MALI_DEBUG_PRINT(gpu_dvfs_debuglevel,("mali_on2_status_set is_on2_enable %d reg_val %#x \n", is_on2_enable, reg_val));
    }
#endif
}

static void set_num_cores(struct work_struct *work)
{
	int err = mali_perf_set_num_pp_cores(num_cores_enabled);
	MALI_DEBUG_ASSERT(0 == err);
	MALI_IGNORE(err);
}

static void enable_one_core(void)
{
	if (num_cores_enabled < num_cores_total) {
		++num_cores_enabled;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1, ("Core scaling: Enabling one more core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
}

static void disable_one_core(void)
{
	if (1 < num_cores_enabled) {
		--num_cores_enabled;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1, ("Core scaling: Disabling one core\n"));
	}

	MALI_DEBUG_ASSERT(              1 <= num_cores_enabled);
	MALI_DEBUG_ASSERT(num_cores_total >= num_cores_enabled);
}

#if USING_INPUT_EVENT
static void enable_max_num_cores(void)
{
	if (num_cores_enabled < num_cores_total) {
		num_cores_enabled = num_cores_total;
		schedule_work(&wq_work);
		MALI_DEBUG_PRINT(gpu_dvfs_debuglevel+1, ("Core scaling: Enabling maximum number of cores\n"));
	}

	MALI_DEBUG_ASSERT(num_cores_total == num_cores_enabled);
}
#endif

