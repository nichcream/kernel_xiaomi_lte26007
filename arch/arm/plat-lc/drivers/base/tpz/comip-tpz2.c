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
 *	1.0.0		2014-06-02	xuxuefeng	created
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/sched.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <plat/hardware.h>
#include <plat/comip-memory.h>
#include <plat/tpz.h>

#define DRIVER_NAME "comip-tpz"

struct comip_tpz_data {
	void __iomem *base;
	struct clk *clk;
	struct resource *res;
	unsigned long *vaddr;
	dma_addr_t paddr;

	int irq;
};

static struct comip_tpz_data *comip_tpz_data;

static inline void tpz_writel(struct comip_tpz_data *tpz,
		int idx, unsigned int val)
{
	writel(val, tpz->base + idx);
}

static inline unsigned int tpz_readl(struct comip_tpz_data *tpz,
		int idx)
{
	return readl(tpz->base + idx);
}

static int tpz_irq_status(struct comip_tpz_data *tpz)
{
	int status;

	status = tpz_readl(tpz, TPZCTL_INTS);
	tpz_writel(tpz, TPZCTL_INTS, status);

	return status;
}

static void tpz_irq_enable(struct comip_tpz_data *tpz, int enable, int flags)
{
	int status;

	status = tpz_readl(tpz, TPZCTL_INTE);
	if (enable)
		status |= flags;
	else
		status &= ~flags;
	tpz_writel(tpz, TPZCTL_INTE, status );
}

static void tpz_irq_init(struct comip_tpz_data *tpz)
{
	tpz_irq_enable(tpz, 0, 0xffffffff);
	tpz_writel(tpz, TPZCTL_INTS, 0xffffffff);
	tpz_writel(tpz, TPZCTL_INTRAW, 0xffffffff);
}

int comip_tpz_enabe(int id, int enable)
{
	struct comip_tpz_data *tpz = comip_tpz_data;

	if (id >= TPZ_MAX)
		return -EINVAL;

	if (id >= TPZ_TOP0 && id <= TPZ_TOP3) {
		tpz_writel(tpz, TPZCTL_TOP_RGN_EN(id - TPZ_TOP0), !!enable);
		tpz_writel(tpz, TPZCTL_TOP_RGN_UPDT(id - TPZ_TOP0), 1);
		tpz_irq_enable(tpz, enable, (1 << TPZCTL_TOP_TPZ_INTR));

	} else if (id >= TPZ_CP0 && id <= TPZ_CP3) {
		tpz_writel(tpz, TPZCTL_CP_RGN_EN(id - TPZ_CP0), !!enable);
		tpz_writel(tpz, TPZCTL_CP_RGN_UPDT(id - TPZ_CP0), 1);
		tpz_irq_enable(tpz, enable, (1 << TPZCTL_CP_TPZ_INTR)
			| (1 << TPZCTL_CPA7_TPZ_INTR));
	} else {
		tpz_writel(tpz, TPZCTL_AP_RGN_EN(id - TPZ_AP0), !!enable);
		tpz_writel(tpz, TPZCTL_AP_RGN_UPDT(id - TPZ_AP0), 1);
		tpz_irq_enable(tpz, enable,
			(1 << TPZCTL_AP_CCI400_TPZ_INTR) | (1 << TPZCTL_AP_SW2_TPZ_INTR)
			| (1 << TPZCTL_AP_VIDEO_TPZ_INTR) | (1 << TPZCTL_AP_DSI_ISP_TPZ_INTR));
	}

	return 0;


}
EXPORT_SYMBOL(comip_tpz_enabe);

int comip_tpz_config(int id, int attr, int start, int end, int def_addr)
{
	struct comip_tpz_data *tpz = comip_tpz_data;
	int val;

	if (id >= TPZ_MAX)
		return -EINVAL;

	if (start >= TPZ_MAX_BLOCKS)
		return -EINVAL;

	if (end > TPZ_MAX_BLOCKS)
		end = TPZ_MAX_BLOCKS;

	if(start == end)
		return -EINVAL;


	if (def_addr < 0)
		def_addr = tpz->paddr;

	if (id >= TPZ_TOP0 && id <= TPZ_TOP3) {
		val = (start << 8) | (end << 20) | (attr & TPZ_ATTR_MASK);
		tpz_writel(tpz, TPZCTL_TOP_TPZ_CFG(id - TPZ_TOP0), val);
		tpz_writel(tpz, TPZCTL_CP_TPZ_DADDR, def_addr);
	} else	if (id >= TPZ_CP0 && id <= TPZ_CP3) {
		val = (start << 8) | (end << 20) | (attr & TPZ_ATTR_MASK);
		tpz_writel(tpz, TPZCTL_CP_TPZ_CFG(id - TPZ_CP0), val);
		tpz_writel(tpz, TPZCTL_CP_TPZ_DADDR, def_addr);
	} else {
		val = (start << 0) | (end << 16);

		tpz_writel(tpz, TPZCTL_AP_TPZ_CFG(id - TPZ_AP0), val);
		tpz_writel(tpz, TPZCTL_AP_TPZ_DADDR, def_addr);

		val = (attr & TPZ_ATTR_MASK);
		tpz_writel(tpz, TPZCTL_AP_TPZ_ATTR(id - TPZ_AP0), val);

	}

	return 0;
}
EXPORT_SYMBOL(comip_tpz_config);

static void comip_tpz_memory_monitor(void)
{
	comip_tpz_config(TPZ_AP3, TPZ_ATTR_HW,
			COMIP_MEMORY_SIZE / TPZ_BLOCK_SIZE,
			TPZ_MAX_BLOCKS, -1);
	comip_tpz_enabe(TPZ_AP3, 1);

	comip_tpz_config(TPZ_CP0, TPZ_ATTR_HW,
			KERNEL_MEMORY_BASE/ TPZ_BLOCK_SIZE,
			TPZ_MAX_BLOCKS, -1);
	comip_tpz_enabe(TPZ_CP0, 1);
}

static irqreturn_t comip_tpz_irq_handle(int irq, void* dev_id)
{
	struct comip_tpz_data *tpz = dev_id;
	int status = tpz_irq_status(tpz);

	if(status & (1 << TPZCTL_TOP_TPZ_INTR))
		printk(KERN_DEBUG "cp tpz interrupt\n");

	if(status & (1 << TPZCTL_CP_TPZ_INTR))
		printk(KERN_DEBUG "cp tpz interrupt\n");

	if(status & (1 << TPZCTL_CPA7_TPZ_INTR))
		printk(KERN_DEBUG "cpa7 tpz interrupt\n");

	if(status & (1 << TPZCTL_AP_CCI400_TPZ_INTR))
		printk(KERN_DEBUG "ap cci400 tpz interrupt\n");

	if(status & (1 << TPZCTL_AP_SW2_TPZ_INTR))
		printk(KERN_DEBUG "ap sw2 tpz interrupt\n");

	if(status & (1 << TPZCTL_AP_VIDEO_TPZ_INTR))
		printk(KERN_DEBUG "ap video tpz interrupt\n");

	if(status & (1 << TPZCTL_AP_DSI_ISP_TPZ_INTR))
		printk(KERN_DEBUG "ap dsi tpz interrupt\n");

	return IRQ_HANDLED;
}
static int comip_tpz_probe(struct platform_device *pdev)
{
	struct comip_tpz_data *tpz;
	struct resource *res;
	int irq;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get resource\n");
		return -ENOENT;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get tpz irq\n");
		return -ENXIO;
	}

	if (!request_mem_region(res->start, resource_size(res), res->name)) {
		dev_err(&pdev->dev, "failed to request memory region\n");
		return -ENOMEM;
		goto exit;
	}

	if (!(tpz = kzalloc(sizeof(struct comip_tpz_data), GFP_KERNEL))) {
		dev_err(&pdev->dev, "tpz kzalloc failed\n");
		ret = -ENOMEM;
		goto exit;
	}

	tpz->vaddr = dma_alloc_writecombine(&pdev->dev,
				sizeof(unsigned long), &tpz->paddr, GFP_KERNEL);
	if (!tpz->vaddr) {
		dev_err(&pdev->dev, "tpz dma_alloc_writecombine failed\n");
		ret = -ENOMEM;
		goto exit_kfree;
	}

	comip_tpz_data = tpz;
	*tpz->vaddr = 0x12345678;
	tpz->res = res;
	tpz->irq = irq;
	tpz->base = ioremap(res->start, resource_size(res));
	if (!tpz->base) {
		ret = -EIO;
		goto exit_dma_free;
	}

	tpz->clk = clk_get(&pdev->dev, "tpz_clk");
	if (IS_ERR(tpz->clk)) {
		ret = PTR_ERR(tpz->clk);
		goto exit_unmap;
	}

	clk_enable(tpz->clk);

	tpz_irq_init(tpz);

	/* request irq */
	ret = request_irq(irq,
			comip_tpz_irq_handle,
			IRQF_DISABLED, DRIVER_NAME, tpz);
	if(ret < 0) {
		dev_err(&pdev->dev, "tpz_probe: request irq failed\n");
		goto exit_clk_disable;
	}

	platform_set_drvdata(pdev, tpz);

	comip_tpz_memory_monitor();

	return 0;

exit_clk_disable:
	clk_disable(tpz->clk);
	clk_put(tpz->clk);
exit_unmap:
	iounmap(tpz->base);
exit_dma_free:
	dma_free_writecombine(&pdev->dev, sizeof(unsigned long),
			tpz->vaddr, tpz->paddr);
exit_kfree:
	kfree(tpz);
exit:
	return ret;
}

static int comip_tpz_remove(struct platform_device *pdev)
{
	struct comip_tpz_data *tpz = platform_get_drvdata(pdev);

	clk_disable(tpz->clk);
	clk_put(tpz->clk);
	iounmap(tpz->base);
	dma_free_writecombine(&pdev->dev, sizeof(unsigned long),
			tpz->vaddr, tpz->paddr);
	kfree(tpz);

	return 0;
}

static struct platform_driver comip_tpz_drv = {
	.driver = {
		.name = "comip-tpz",
	},
	.probe	 = comip_tpz_probe,
	.remove  = comip_tpz_remove,
};

static int __init comip_tpz_init(void)
{
	int ret;

	ret = platform_driver_register(&comip_tpz_drv);
	if (ret) {
		printk(KERN_ERR "comip-tpz: tpz driver register error.\n");
		return ret;
	}

	return 0;
}

static void __exit comip_tpz_exit(void)
{
	platform_driver_unregister(&comip_tpz_drv);
}

core_initcall(comip_tpz_init);
module_exit(comip_tpz_exit);

MODULE_AUTHOR("xuxuefeng<xuxuefeng@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP TPZ Driver");
MODULE_LICENSE("GPL");
