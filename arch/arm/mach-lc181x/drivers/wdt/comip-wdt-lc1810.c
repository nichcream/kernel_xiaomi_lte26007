/* arch/arm/mach-comip/comip-wdt-lc1810.c
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
 *	1.0.0		2012-10-23	zouqiao		created
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

#include <asm/irq.h>
#include <asm/io.h>
#include <plat/hardware.h>
#include <plat/watchdog.h>

#define WDT_TORR_TO_TIMEOUT(val)	((val) / 2)
#define WDT_TIMEOUT_TO_TORR(val)	((val) * 2)

struct comip_wdt_data {
	void __iomem *base;
	struct clk *clk;
	struct resource *res;
	struct hrtimer wdt_timer;
	unsigned long clk_rate;
	/* timeout is set to milliseconds*/
	unsigned long timeout;
	int enable;
	int reg_cr;
};

static struct comip_wdt_data *comip_wdt_data;

static int comip_wdt_check(struct comip_wdt_data *data)
{
	data->reg_cr = readl(data->base + WDT_CR);

	return !!(data->reg_cr & WDT_ENABLE);
}

static int comip_wdt_enable(struct comip_wdt_data *data, int enable)
{
	unsigned long val;

	val = readl(data->base + WDT_CR);
	if (enable)
		val |= WDT_ENABLE;
	else
		val &= ~WDT_ENABLE;
	writel(val, data->base + WDT_CR);

	return 0;
}

static void comip_wdt_kick(struct comip_wdt_data *data)
{
	writel(WDT_RESET_VAL, data->base + WDT_CRR);
}

static unsigned long comip_wdt_timeout(struct comip_wdt_data *data)
{
	unsigned long val;
	unsigned long timeout;

	val = readl(data->base + WDT_TORR);
	if (val > WDT_TIMEOUT_MAX)
		return 0;

	timeout = ((1 << (val + 16)) - 1)/ data->clk_rate;
	return WDT_TORR_TO_TIMEOUT(timeout * 1000);
}

static enum hrtimer_restart wdt_timeout_func(struct hrtimer *timer)
{
	struct comip_wdt_data *data =
		container_of(timer, struct comip_wdt_data, wdt_timer);

	comip_wdt_kick(data);
	hrtimer_forward_now(timer, ktime_set((data->timeout / 1000),
		(data->timeout % 1000  * NSEC_PER_MSEC)));
	return HRTIMER_RESTART;
}

static ssize_t comip_wdt_show_enable(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct comip_wdt_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enable);
}

static ssize_t comip_wdt_store_enable(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf,
			  size_t count)
{
	struct comip_wdt_data *data = dev_get_drvdata(dev);
	char *endp;
	int value;
	int enable;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	enable = !!value;
	if (data->enable == enable)
		return count;

	data->enable = enable;
	if (data->enable) {
		clk_enable(data->clk);
		data->timeout = comip_wdt_timeout(data);
		if (!data->timeout) {
			data->enable = 0;
			comip_wdt_enable(data, data->enable);
			printk(KERN_DEBUG "watchdog timeout is invalid\n");
		} else {
			hrtimer_init(&data->wdt_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			data->wdt_timer.function = wdt_timeout_func;
			hrtimer_start(&data->wdt_timer, ktime_set((data->timeout / 1000),
					((data->timeout % 1000) * NSEC_PER_MSEC)), HRTIMER_MODE_REL);
		}
	} else {
		hrtimer_cancel(&data->wdt_timer);
		comip_wdt_enable(data, data->enable);
		clk_disable(data->clk);
	}

	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO, comip_wdt_show_enable,
		   comip_wdt_store_enable);

static ssize_t comip_wdt_show_torr(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct comip_wdt_data *data = dev_get_drvdata(dev);
	int value;

	if (!data->enable) {
		clk_enable(data->clk);
		value = readl(data->base + WDT_TORR);
		clk_disable(data->clk);
	} else {
		value = readl(data->base + WDT_TORR);
	}

	return sprintf(buf, "%d\n", value);
}

static ssize_t comip_wdt_store_torr(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf,
			  size_t count)
{
	struct comip_wdt_data *data = dev_get_drvdata(dev);
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	if (!data->enable) {
		clk_enable(data->clk);
		writel(value, data->base + WDT_TORR);
		clk_disable(data->clk);
	}

	return count;
}

static DEVICE_ATTR(torr, S_IWUSR | S_IRUGO, comip_wdt_show_torr,
		   comip_wdt_store_torr);

static int comip_wdt_reboot_handler(struct notifier_block *this,
			unsigned long code, void *unused)
{
	struct comip_wdt_data *data = comip_wdt_data;

	if ((code == SYS_POWER_OFF) || (code == SYS_HALT)) {
		if (data && data->enable) {
			/* Set a long timer to let the reboot happens. */
			writel(0xf, data->base + WDT_TORR);
			writel(WDT_RESET_VAL, data->base + WDT_CRR);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block comip_wdt_reboot_notifier = {
	.notifier_call	= comip_wdt_reboot_handler,
};

static int comip_wdt_panic_handler(struct notifier_block *this,
			unsigned long event, void *ptr)
{
	struct comip_wdt_data *data = comip_wdt_data;

	if (data && data->enable) {
		/* Set a long timer to save panic log. */
		writel(0xf, data->base + WDT_TORR);
		writel(WDT_RESET_VAL, data->base + WDT_CRR);
	}

	return NOTIFY_OK;
}

static struct notifier_block comip_wdt_panic_notifier = {
	.notifier_call	= comip_wdt_panic_handler,
	.priority	= 150,
};

static int comip_wdt_probe(struct platform_device *pdev)
{
	struct comip_wdt_data *data;
	struct resource *res;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get resource\n");
		ret = -ENOENT;
		goto exit;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "failed to get request memory\n");
		ret = -EBUSY;
		goto exit;
	}

	if (!(data = kzalloc(sizeof(struct comip_wdt_data), GFP_KERNEL))) {
		dev_err(&pdev->dev, "wdt kzalloc error\n");
		ret = -ENOMEM;
		goto exit_release_mem;
	}

	data->res = res;
	data->base = ioremap(res->start, resource_size(res));
	if (!data->base) {
		dev_err(&pdev->dev, "failed to ioremap\n");
		ret = -ENOMEM;
		goto exit_kfree;
	}

	data->clk = clk_get(&pdev->dev, "wdt_clk");
	if (IS_ERR(data->clk)) {
		ret = PTR_ERR(data->clk);
		goto exit_iounmap;
	}

	data->clk_rate = clk_get_rate(data->clk);
	clk_enable(data->clk);

	ret = device_create_file(&pdev->dev, &dev_attr_enable);
	if (ret) {
		dev_err(&pdev->dev, "failed to create enable file\n");
		goto exit_clk_disable;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_torr);
	if (ret) {
		dev_err(&pdev->dev, "failed to create torr file\n");
		goto exit_clk_disable;
	}

	data->enable = comip_wdt_check(data);
	if (data->enable) {
		comip_wdt_kick(data);

		data->timeout = comip_wdt_timeout(data);
		if (!data->timeout) {
			data->enable = 0;
			comip_wdt_enable(data, data->enable);
			dev_warn(&pdev->dev, "watchdog timeout is invalid\n");
		} else {
			hrtimer_init(&data->wdt_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			data->wdt_timer.function = wdt_timeout_func;
			hrtimer_start(&data->wdt_timer, ktime_set((data->timeout / 1000),
					((data->timeout % 1000) * NSEC_PER_MSEC)), HRTIMER_MODE_REL);
		}
	} else {
		clk_disable(data->clk);
	}

	printk(KERN_ERR "watchdog %s, clock rate: %ld, timeout: %ld\n",
				data->enable ? "enable" : "disable",
				data->clk_rate, data->timeout);

	comip_wdt_data = data;
	register_reboot_notifier(&comip_wdt_reboot_notifier);
	atomic_notifier_chain_register(&panic_notifier_list,
			&comip_wdt_panic_notifier);

	platform_set_drvdata(pdev, data);

	return 0;

exit_clk_disable:
	clk_disable(data->clk);
	clk_put(data->clk);
exit_iounmap:
	iounmap(data->base);
exit_kfree:
	kfree(data);
exit_release_mem:
	release_mem_region(res->start, resource_size(res));
exit:
	return ret;
}

static int comip_wdt_remove(struct platform_device *pdev)
{
	struct comip_wdt_data *data = platform_get_drvdata(pdev);

	if (data) {
		platform_set_drvdata(pdev, NULL);

		atomic_notifier_chain_unregister(&panic_notifier_list,
					&comip_wdt_panic_notifier);
		unregister_reboot_notifier(&comip_wdt_reboot_notifier);

		device_remove_file(&pdev->dev, &dev_attr_enable);
		device_remove_file(&pdev->dev, &dev_attr_torr);

		if (data->enable)
			hrtimer_cancel(&data->wdt_timer);

		clk_disable(data->clk);
		clk_put(data->clk);

		iounmap(data->base);
		release_mem_region(data->res->start, resource_size(data->res));
		kfree(data);
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

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP Watchdog Driver");
MODULE_LICENSE("GPL");

