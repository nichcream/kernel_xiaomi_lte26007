/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the 32k clock calibration driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2013-05-29	zouqiao		created
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/clocksource.h>
#include <linux/platform_device.h>
#include <plat/hardware.h>
#include <plat/comip-calib.h>
#include <asm/io.h>

#define COMIP_CALIB_DEBUG
#ifdef COMIP_CALIB_DEBUG
#define CALIB_PRINT(fmt, args...) printk(KERN_ERR "[CALIB]" fmt, ##args)
#else
#define CALIB_PRINT(fmt, args...) printk(KERN_DEBUG "[CALIB]" fmt, ##args)
#endif

#define CALIB_INTERVAL			(4 * HZ)
#define CALIB_32K_FERQ			(32768)
#define CALIB_32K_FERQ_DELTA		(8)
#define CALIB_32K_FERQ_MAX		(CALIB_32K_FERQ + CALIB_32K_FERQ_DELTA)
#define CALIB_32K_FERQ_MIN		(CALIB_32K_FERQ - CALIB_32K_FERQ_DELTA)

BLOCKING_NOTIFIER_HEAD(calibration_notifier_list);

struct calib_info {
	struct delayed_work calib_work;
	spinlock_t slock;
	cycle_t val_32k_start;
	cycle_t val_32k_stop;
	cycle_t val_hiclk_start;
	cycle_t val_hiclk_stop;
	unsigned long hiclk_rate;
};

static struct calib_info *calib_info;

static cycle_t calib_clkhi_read(void)
{
	return readl(io_p2v(COMIP_TIMER_TCV(COMIP_CLOCKSROUCE_TIMER_ID)));
}

static cycle_t calib_clk32k_read(void)
{
	return readl(io_p2v(AP_PWR_TM_CUR_VAL));
}

static int comip_calib_get(unsigned int ref_cnt, unsigned int *freq_32k, unsigned int *rem)
{
	struct calib_info *info = calib_info;
	cycle_t freq;

	if (!info)
		return -ENODEV;

	if (!ref_cnt || !freq_32k || !rem)
		return -EINVAL;

	freq = (info->val_32k_start - info->val_32k_stop) * info->hiclk_rate * ref_cnt;
	do_div(freq, (info->val_hiclk_start - info->val_hiclk_stop));

	*rem = do_div(freq, ref_cnt);
	*freq_32k = freq;

	if ((freq > CALIB_32K_FERQ_MAX) || (freq < CALIB_32K_FERQ_MIN)) {
		CALIB_PRINT("%lld, %lld, %lld\n",
			info->val_32k_start,
			info->val_32k_stop,
			(info->val_32k_start - info->val_32k_stop));
		CALIB_PRINT("%lld, %lld, %lld\n",
			info->val_hiclk_start,
			info->val_hiclk_stop,
			(info->val_hiclk_start - info->val_hiclk_stop));
		CALIB_PRINT("ref: %d, 32k freq: %d, rem: %d\n", ref_cnt, *freq_32k, *rem);

		return -ERANGE;
	}

	return 0;
}

static void comip_calib_work(struct work_struct *work)
{
	struct calib_info *info = container_of(work,
		struct calib_info, calib_work.work);
	unsigned long flags;

	spin_lock_irqsave(&info->slock, flags);
	info->val_32k_stop = calib_clk32k_read();
	info->val_hiclk_stop = calib_clkhi_read();
	spin_unlock_irqrestore(&info->slock, flags);

	blocking_notifier_call_chain(&calibration_notifier_list, 0, comip_calib_get);
}

static int comip_calib_probe(struct platform_device *pdev)
{
	struct calib_info *info;
	unsigned long flags;
	struct clk *clk;
	int ret = 0;

	info = kzalloc(sizeof(struct calib_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	clk = clk_get(NULL, COMIP_CLOCKSROUCE_TIMER_NAME);
	if (!clk) {
		printk(KERN_ERR "Can't get clock : %s!\n", COMIP_CLOCKSROUCE_TIMER_NAME);
		ret = PTR_ERR(clk);
		goto err;
	}

	info->hiclk_rate = clk_get_rate(clk);
	clk_put(clk);

	calib_info = info;

	spin_lock_irqsave(&info->slock, flags);
	info->val_32k_start = calib_clk32k_read();
	info->val_hiclk_start = calib_clkhi_read();
	spin_unlock_irqrestore(&info->slock, flags);

	INIT_DELAYED_WORK(&info->calib_work, comip_calib_work);
	schedule_delayed_work(&info->calib_work, CALIB_INTERVAL);

	platform_set_drvdata(pdev, info);

	return 0;

err:
	kfree(info);
	return ret;
}

static int comip_calib_remove(struct platform_device *pdev)
{
	struct calib_info *info = platform_get_drvdata(pdev);

	if (info) {
		platform_set_drvdata(pdev, NULL);
		kfree(info);
		calib_info = NULL;
	}

	return 0;
}

static struct platform_device comip_calib_device = {
	.name = "comip-calib",
	.id = -1,
};

struct platform_driver comip_calib_driver = {
	.probe = comip_calib_probe,
	.remove = comip_calib_remove,
	.driver = {
		.name = "comip-calib",
	},
};

static int __init calib_init(void)
{
	platform_device_register(&comip_calib_device);
	return platform_driver_register(&comip_calib_driver);
}

static void __exit calib_exit(void)
{
	platform_device_unregister(&comip_calib_device);
	platform_driver_unregister(&comip_calib_driver);
}

module_init(calib_init);
module_exit(calib_exit);

