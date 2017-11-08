/* arch/arm/mach-comip/comip-wdt-lc1813.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip hardware plarform's wdt driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2013-06-08	licheng		created
 *	1.0.1		2013-06-26	zhaoyuan		updated
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/sched.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <plat/hardware.h>
#include <plat/watchdog.h>

#define DRIVER_NAME "comip-wdt"

#ifdef WATCHDOG_DEBUG
#define wdt_debug(fmt, args...)		printk(KERN_INFO fmt, ##args)
#else
#define wdt_debug(fmt, args...)		do {} while (0)
#endif

/* Set WDT 10s timeout. */
#define WDT_TIMEOUT_VAL		(0x0d)
#define WDT_NUM				(4)
#define WDT_HRTIMER_TICK		(5)

#define WDT_TORR_TO_TIMEOUT(val)	((val) / 2)
#define WDT_TIMEOUT_TO_TORR(val)	((val) * 2)

enum {
	CPU_ID_0 = 0,
	CPU_ID_1 = 1,
	CPU_ID_2 = 2,
	CPU_ID_3 = 3,
};

struct comip_wdt_data {
	void __iomem *base;
	struct clk *clk;
	struct resource *res;
	struct hrtimer wdt_timer;
	unsigned long clk_rate;
	/* timeout is set to milliseconds*/
	unsigned long timeout;
	int wdt_enable;
	int clk_enable;
	int hrtimer_init;
	int hrtimer_start;
	unsigned int wdt_mode;
	int id;
};
static struct comip_wdt_data *comip_wdt_data[WDT_NUM];

static inline void set_watchdog_timeout_max(struct comip_wdt_data *data)
{
	/* Set a long timer to let the reboot happens. */
	writel(WDT_TIMEOUT_MAX, io_p2v(data->base  + WDT_TORR));
	writel(WDT_RESET_VAL, io_p2v(data->base + WDT_CRR));
}

static void comip_get_wdt_mode(struct comip_wdt_data *data)
{
	clk_enable(data->clk);
	data->wdt_mode = readl(io_p2v(data->base + WDT_CR));
	clk_disable(data->clk);
}

static inline void comip_reset_wachdog(struct comip_wdt_data *data)
{
	writel(WDT_TIMEOUT_VAL, io_p2v(data->base + WDT_TORR));
	writel(data->wdt_mode, io_p2v(data->base + WDT_CR));
	writel(WDT_RESET_VAL, io_p2v(data->base + WDT_CRR));
}

static inline void comip_kick_watchdog(struct comip_wdt_data * data)
{
	writel(WDT_RESET_VAL, io_p2v(data->base + WDT_CRR));
}

static inline void comip_disable_watchdog(struct comip_wdt_data * data)
{
	writel(0, io_p2v(data->base + WDT_CR));
}

static int comip_wdt_reboot_handler(struct notifier_block *this,
			unsigned long code, void *unused)
{
	int i = 0;
	struct comip_wdt_data *data;

	if ((code == SYS_POWER_OFF) || (code == SYS_HALT)) {
		for(i = 0; i < WDT_NUM; i++){
			data = comip_wdt_data[i];
			if (data && data->hrtimer_start)
				set_watchdog_timeout_max(data);
		}
	}
	return NOTIFY_OK;
}

static int comip_wdt_panic_handler(struct notifier_block *this,
			unsigned long event, void *ptr)
{
	int i = 0;
	struct comip_wdt_data *data;

	for (i = 0; i < WDT_NUM; i++) {
		data = comip_wdt_data[i];
		if(data && data->hrtimer_start)
			set_watchdog_timeout_max(data);
	}

	return NOTIFY_OK;
}

static struct notifier_block comip_wdt_reboot_notifier = {
	.notifier_call	= comip_wdt_reboot_handler,
};

static struct notifier_block comip_wdt_panic_notifier = {
	.notifier_call	= comip_wdt_panic_handler,
	.priority	= 150,
};

static enum hrtimer_restart wdt_timer_fn(struct hrtimer *hrtimer)
{
	struct comip_wdt_data *data =
		container_of(hrtimer, struct comip_wdt_data, wdt_timer);

	wdt_debug("Watchdog%d count is 0x%x, when hrtimer timeout.\n",
			data->id, readl(io_p2v(data->base + WDT_CCVR)));
	comip_kick_watchdog(data);
	hrtimer_forward_now(hrtimer, ktime_set(WDT_HRTIMER_TICK, 0));
	return HRTIMER_RESTART;
}

static inline void cpu_timer_start(struct comip_wdt_data *data)
{
	if(!data->hrtimer_start){
		wdt_debug("Watchdog%d count is 0x%x, when start hrtimer.\n",
				data->id, readl(io_p2v(data->base + WDT_CCVR)));
		clk_enable(data->clk);
		comip_reset_wachdog(data);
		hrtimer_start(&data->wdt_timer, ktime_set(WDT_HRTIMER_TICK, 0),
			HRTIMER_MODE_REL_PINNED);
		data->hrtimer_start = 1;
	}
}

static inline void cpu_timer_stop(struct comip_wdt_data *data)
{
	if(data->hrtimer_start) {
		wdt_debug("Watchdog%d count is 0x%x, when stop hrtimer.\n",
				data->id, readl(io_p2v(data->base + WDT_CCVR)));
		comip_kick_watchdog(data);
		comip_disable_watchdog(data);
		clk_disable(data->clk);
		hrtimer_cancel(&data->wdt_timer);
		data->hrtimer_start = 0;
	}
}

static void wdt_hrtimer_init(void *info)
{
	struct comip_wdt_data *data = (struct comip_wdt_data *)info;

	if(data && !data->hrtimer_init){
		clk_enable(data->clk);
		comip_reset_wachdog(data);
		hrtimer_init(&data->wdt_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
		data->wdt_timer.function = wdt_timer_fn;
		hrtimer_start(&data->wdt_timer, ktime_set(WDT_HRTIMER_TICK, 0),
			HRTIMER_MODE_REL_PINNED);
		data->hrtimer_init = 1;
		data->hrtimer_start = 1;
		printk(KERN_INFO "Watchdog%d is enabled.\n", data->id);
	}
}

static int watchdog_cpu_notify(struct notifier_block *self, unsigned long action, void *hcpu)
{
	int scpu = (long)hcpu;
	struct comip_wdt_data *data = comip_wdt_data[scpu];

	switch (action) {
	case CPU_STARTING:
	case CPU_STARTING_FROZEN:
		if(data->hrtimer_init)
			cpu_timer_start(data);
		else
			wdt_hrtimer_init((void *)data);
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_DYING:
	case CPU_DYING_FROZEN:
		if(data->hrtimer_init)
			cpu_timer_stop(data);
		break;
#endif
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block  wdt_hrtimers_nb = {
	.notifier_call = watchdog_cpu_notify,
};

static int comip_wdt_probe(struct platform_device *pdev)
{
	struct comip_wdt_data *data;
	struct resource *res;
	int ret, cpu_id;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev, "failed to get resource\n");
		ret = -ENOENT;
		goto exit;
	}
	if (!(data = kzalloc(sizeof(struct comip_wdt_data), GFP_KERNEL))) {
		dev_err(&pdev->dev, "wdt kzalloc error\n");
		ret = -ENOMEM;
		goto exit;
	}

	data->id = pdev->id;
	data->base = (void __iomem *)res->start;
	data->clk = clk_get(&pdev->dev, "wdt_clk");
	if (IS_ERR(data->clk)) {
		ret = PTR_ERR(data->clk);
		goto exit_kfree;
	}
	data->clk_rate = clk_get_rate(data->clk);
	comip_get_wdt_mode(data);
	data->wdt_enable = !!(data->wdt_mode & WDT_ENABLE);

	cpu_id = data->id;
	if (data->wdt_enable) {
		/* Create hrtimer for CPUs. */
		if (cpu_online(cpu_id))
			smp_call_function_single(cpu_id, &wdt_hrtimer_init, data, false);

		/*Register callback function once.*/
		if (cpu_id == CPU_ID_0) {
			register_cpu_notifier(&wdt_hrtimers_nb);
			register_reboot_notifier(&comip_wdt_reboot_notifier);
			atomic_notifier_chain_register(&panic_notifier_list,
					&comip_wdt_panic_notifier);
		}
	} else
		printk(KERN_INFO "Watchdog%d is disabled\n", data->id);

	comip_wdt_data[cpu_id] = data;
	platform_set_drvdata(pdev, data);

	return 0;

exit_kfree:
	kfree(data);
exit:
	return ret;
}

static int comip_wdt_remove(struct platform_device *pdev)
{
	struct comip_wdt_data *data = platform_get_drvdata(pdev);
	if (data) {
		if (data->hrtimer_start){
			hrtimer_cancel(&data->wdt_timer);
			clk_disable(data->clk);
			data->hrtimer_init = 0;
			data->hrtimer_start= 0;
		}

		comip_wdt_data[data->id] = NULL;
		clk_put(data->clk);
		kfree(data);
		platform_set_drvdata(pdev, NULL);
	}
	return 0;
}

static struct platform_driver comip_wdt_drv = {
	.driver = {
		.name = "comip-wdt",
	},
	.probe	 = comip_wdt_probe,
	.remove  = comip_wdt_remove,
};

static int __init comip_wdt_init(void)
{
	int ret;

	ret = platform_driver_register(&comip_wdt_drv);
	if (ret) {
		printk(KERN_ERR "comip-wdt: watchdog driver register error.\n");
		return ret;
	}

	return 0;
}

static void __exit comip_wdt_exit(void)
{
	platform_driver_unregister(&comip_wdt_drv);
}

core_initcall(comip_wdt_init);
module_exit(comip_wdt_exit);

MODULE_AUTHOR("licheng <licheng@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP Watchdog Driver");
MODULE_LICENSE("GPL");

