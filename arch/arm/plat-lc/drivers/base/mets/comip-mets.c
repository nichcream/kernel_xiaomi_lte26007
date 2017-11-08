/* arch/arm/mach-comip/comip-tpz.c
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
 *	1.0.0		2014 -06 -7	xuxuefeng	created
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <plat/hardware.h>
#include <plat/cpu.h>

#define DRIVER_NAME "comip-mets"
#define METS_CLK	(9000000)
#define METS_CLK_ECO2	(1000000)
#define TEMP_LOW_MAX	(-40)
#define TEMP_HIGH_MAX	(125)

struct comip_mets_data {
	void __iomem *base;
	struct clk *clk;
	struct resource *res;
	int irq;
	struct delayed_work monitor_work;
	int temp;
};

static struct comip_mets_data *comip_mets_data;

static inline void mets_writel(struct comip_mets_data *mets,
		int idx, unsigned int val)
{
	writel(val, mets->base + idx);
}

static inline unsigned int mets_readl(struct comip_mets_data *mets,
		int idx)
{
	return readl(mets->base + idx);
}

int regs2temp(int regs_val)
{
	if (cpu_is_lc1860_eco2())
		return ((regs_val * 353) / 4096 - 108);
	else
		return ((regs_val * 82) / 1000 - 83);
}

int temp2regs(int temp_val)
{
	if (cpu_is_lc1860_eco2())
		return ((temp_val + 108) * 4096 / 353);
	else
		return ((temp_val + 83) * 1000 / 82);

}

int comip_mets_get_temperature(void)
{
	struct comip_mets_data *mets = comip_mets_data;
	int val;

	val = mets_readl(mets, TEMP_VALUE);

	return regs2temp(val);
}
EXPORT_SYMBOL(comip_mets_get_temperature);


void mets_hw_init(void)
{
	struct comip_mets_data *mets = comip_mets_data;
	int val;

	val = (0x30 << METS_CTRL_POWER_UP_CNT_VAL) |
		(0x08 << METS_CTRL_POWER_DN_CNT_VAL ) | (0x04 << METS_CTRL_INIT_CNT_VAL);

	mets_writel(mets, METS_CTRL, val);

}

void mets_enable(int enable)
{
	struct comip_mets_data *mets = comip_mets_data;

	mets_writel(mets, TEMP_MON_EN, !!enable);
}

static int mets_irq_status(struct comip_mets_data *mets)
{
	int status;

	status = mets_readl(mets, TEMP_MON_INTR);
	mets_writel(mets, TEMP_MON_INTR, status);

	return status;
}

static void mets_irq_enable(struct comip_mets_data *mets, int enable, int flags)
{
	int status;

	status = mets_readl(mets, TEMP_MON_INTR_EN);
	if (enable)
		status |= flags;
	else
		status &= ~flags;

	mets_writel(mets, TEMP_MON_INTR_EN, status);
}

static void mets_irq_init(struct comip_mets_data *mets)
{
	mets_irq_enable(mets, 0, 0xffffffff);
	mets_writel(mets, TEMP_MON_INTR, 0xffffffff);
}

static irqreturn_t comip_mets_irq_handle(int irq, void* dev_id)
{
	struct comip_mets_data *mets = dev_id;
	int status = mets_irq_status(mets);

	if(status & (1 << TEMP_MON_INTR_DN))
		printk(KERN_DEBUG "mets down interrupt\n");

	if(status & (1 << TEMP_MON_INTR_UP))
		printk(KERN_DEBUG "mets up interrupt\n");

	schedule_delayed_work(&mets->monitor_work, 0 * HZ);

	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(mets_notifier_list);

/* external function*/
int mets_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mets_notifier_list, nb);
}
EXPORT_SYMBOL(mets_notifier_register);

int mets_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mets_notifier_list, nb);
}
EXPORT_SYMBOL(mets_notifier_unregister);

int mets_notify(int temperature)
{
	return blocking_notifier_call_chain(&mets_notifier_list, 0, (void *)&temperature);
}
EXPORT_SYMBOL(mets_notify);

static void monitor_work_fn(struct work_struct *work)
{
	struct comip_mets_data *mets
		= container_of((struct delayed_work *)work, struct comip_mets_data, monitor_work);

	int temp = comip_mets_get_temperature();

	mets->temp = temp;
	mets_notify(temp);
}

int comip_mets_config(int temperature_low, int temperature_high)
{
	int low;
	int high;
	struct comip_mets_data *mets = comip_mets_data;

	if (temperature_low <= TEMP_LOW_MAX || temperature_high >= TEMP_HIGH_MAX)
		return -EINVAL;

	low = temp2regs(temperature_low);
	high = temp2regs(temperature_high);

	mets_writel(mets, TEMP_THRES_CFG,
		(low << TEMP_THRES_CFG_LOW) | (high << TEMP_THRES_CFG_HIGH));
	mets_irq_enable(mets, 1,
		(1 << TEMP_MON_INTR_EN_DN) | (1 << TEMP_MON_INTR_EN_UP));

	return 0;
}
EXPORT_SYMBOL(comip_mets_config);


static int comip_mets_probe(struct platform_device *pdev)
{
	struct resource *res;
	int irq;
	int ret;
	static struct comip_mets_data *mets;


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get resource\n");
		return -ENOENT;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get mets irq\n");
		return -ENXIO;
	}

	if (!(mets = kzalloc(sizeof(struct comip_mets_data), GFP_KERNEL))) {
		dev_err(&pdev->dev, "mets kzalloc failed\n");
		ret = -ENOMEM;
		goto exit;
	}


	if (!request_mem_region(res->start, resource_size(res), res->name)) {
		dev_err(&pdev->dev, "failed to request memory region\n");
		return -ENOMEM;
		goto exit_kfree;
	}

	comip_mets_data = mets;
	mets->res = res;
	mets->irq = irq;
	mets->base = ioremap(res->start, resource_size(res));
	if (!mets->base) {
		ret = -EIO;
		goto exit_kfree;
	}

	mets_hw_init();

	mets->clk = clk_get(&pdev->dev, "mets_clk");
	if (IS_ERR(mets->clk)) {
		ret = PTR_ERR(mets->clk);
		goto exit_unmap;
	}

	if (cpu_is_lc1860_eco2())
		clk_set_rate(mets->clk, METS_CLK_ECO2);
	else
		clk_set_rate(mets->clk, METS_CLK);

	clk_enable(mets->clk);

	mets_enable(1);

	mets_irq_init(mets);

	/* request irq */
	ret = request_irq(irq,
			comip_mets_irq_handle,
			IRQF_DISABLED, DRIVER_NAME, mets);
	if(ret < 0) {
		dev_err(&pdev->dev, "mets_probe: request irq failed\n");
		goto exit_clk_disable;
	}

	INIT_DELAYED_WORK(&mets->monitor_work, monitor_work_fn);
	platform_set_drvdata(pdev, mets);

	return 0;

exit_clk_disable:
	clk_disable(mets->clk);
	clk_put(mets->clk);
exit_unmap:
	iounmap(mets->base);
exit_kfree:
	kfree(mets);
exit:
	return ret;
}

static int comip_mets_remove(struct platform_device *pdev)
{
	struct comip_mets_data *mets = platform_get_drvdata(pdev);

	mets_enable(0);
	clk_disable(mets->clk);
	clk_put(mets->clk);
	iounmap(mets->base);
	kfree(mets);

	return 0;
}

static struct platform_driver comip_mets_drv = {
	.driver = {
		.name = "comip-mets",
	},
	.probe	 = comip_mets_probe,
	.remove  = comip_mets_remove,
};

static int __init comip_mets_init(void)
{
	int ret;

	ret = platform_driver_register(&comip_mets_drv);
	if (ret) {
		printk(KERN_ERR "comip-mets: mets driver register error.\n");
		return ret;
	}

	return 0;
}

static void __exit comip_mets_exit(void)
{
	platform_driver_unregister(&comip_mets_drv);
}

core_initcall(comip_mets_init);
module_exit(comip_mets_exit);

MODULE_AUTHOR("xuxuefeng<xuxuefeng@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP METS Driver");
MODULE_LICENSE("GPL");

