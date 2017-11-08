/* arch/arm/mach-comip/comip-pwm.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip pwm driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date		Author		Description
 *	1.0.0	2012-03-28	tangyong	created
 *	1.0.1	2013-08-20	fanjiangang	updated
 *	2.0.0	2013-12-26	tuzhiqiang	rework
 *
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <plat/pwm.h>
#include <plat/hardware.h>

struct comip_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	void __iomem *base;
	struct resource *res;
};

#define NS_IN_HZ	(1000000000UL)
#define NS_TO_HZ(x)	(1000000000UL / (x))

#define to_comip_pwm_chip(_chip) container_of(_chip, struct comip_pwm_chip, chip)

static inline void pwm_writel(struct comip_pwm_chip *comip,
	struct pwm_device *pwm, unsigned int offset, unsigned int val)
{
	writel(val, comip->base + pwm->hwpwm * 0x40 + offset);
}

static inline u32 pwm_readl(struct comip_pwm_chip *comip,
	struct pwm_device *pwm, unsigned int offset)
{
	return readl(comip->base + pwm->hwpwm * 0x40 + offset);
}

static int comip_pwm_request(struct pwm_chip *chip,
			struct pwm_device *pwm)
{
	struct comip_pwm_platform_data *pdata = chip->dev->platform_data;

	pdata->dev_init(pwm->hwpwm);

	return 0;
}

static int comip_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct comip_pwm_chip *comip = to_comip_pwm_chip(chip);

	clk_enable(comip->clk);
	pwm_writel(comip, pwm, PWM_EN, 0x1);

	return 0;
}

static void comip_pwm_disable(struct pwm_chip *chip,
	struct pwm_device *pwm)
{
	struct comip_pwm_chip *comip = to_comip_pwm_chip(chip);

	pwm_writel(comip, pwm, PWM_EN, 0x0);
	clk_disable(comip->clk);
}

static int comip_pwm_config(struct pwm_chip *chip,
	struct pwm_device *pwm, int duty_ns, int period_ns)
{
	struct comip_pwm_chip *comip = to_comip_pwm_chip(chip);
	u64 tmp;
	unsigned long period;
	unsigned long ocpy;

	if (period_ns > NS_IN_HZ || duty_ns > NS_IN_HZ)
		return -ERANGE;

	if (pwm && !test_bit(PWMF_ENABLED, &pwm->flags))
		clk_enable(comip->clk);

	tmp = (u64)clk_get_rate(comip->clk);
	do_div(tmp, NS_TO_HZ(period_ns) * 100);
	period = tmp - 1;
	ocpy = duty_ns * 100 / period_ns;

	pwm_writel(comip, pwm, PWM_EN, 0x0);
	pwm_writel(comip, pwm, PWM_P, period);
	pwm_writel(comip, pwm, PWM_OCPY, ocpy);
	pwm_writel(comip, pwm, PWM_UP, 0x1);

	if (pwm && test_bit(PWMF_ENABLED, &pwm->flags))
		pwm_writel(comip, pwm, PWM_EN, 0x1);

	return 0;
}

static const struct pwm_ops comip_pwm_ops = {
	.request = comip_pwm_request,
	.config = comip_pwm_config,
	.enable = comip_pwm_enable,
	.disable = comip_pwm_disable,
	.owner = THIS_MODULE,
};

static int comip_pwm_probe(struct platform_device *pdev)
{
	struct comip_pwm_chip *comip;
	struct resource *r;
	struct clk *clk;
	int ret = 0;

	dev_info(&pdev->dev, "comip_pwm_probe\n");

	comip = kzalloc(sizeof(*comip), GFP_KERNEL);
	if (!comip) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	clk = clk_get(&pdev->dev, "pwm_clk");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto err_free;
	}

	comip->chip.dev = &pdev->dev;
	comip->chip.ops = &comip_pwm_ops;
	comip->chip.base = COMIP_PWM_BASE;
	comip->chip.npwm = COMIP_PWM_NUM;
	comip->clk = clk;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no memory resource defined\n");
		ret = -ENODEV;
		goto err_free_clk;
	}

	r = request_mem_region(r->start, resource_size(r), pdev->name);
	if (r == NULL) {
		dev_err(&pdev->dev, "failed to request memory resource\n");
		ret = -EBUSY;
		goto err_free_clk;
	}

	comip->base = ioremap(r->start, resource_size(r));
	if (!comip->base) {
		dev_err(&pdev->dev, "failed to ioremap registers\n");
		ret = -ENODEV;
		goto err_free_mem;
	}

	comip->res = r;

	ret = pwmchip_add(&comip->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add pwm chip %d\n", ret);
		goto err_add_pwmchip;
	}

	platform_set_drvdata(pdev, comip);

	return 0;

err_add_pwmchip:
	iounmap(comip->base);
err_free_mem:
	release_mem_region(r->start, resource_size(r));
err_free_clk:
	clk_put(clk);
err_free:
	kfree(comip);
	return ret;
}

static int __exit comip_pwm_remove(struct platform_device *pdev)
{
	struct comip_pwm_chip *comip = platform_get_drvdata(pdev);

	if (!comip)
		return -ENODEV;

	pwmchip_remove(&comip->chip);
	iounmap(comip->base);
	release_mem_region(comip->res->start, resource_size(comip->res));
	clk_put(comip->clk);
	kfree(comip);
	return 0;
}

static struct platform_driver comip_pwm_driver = {
	.driver = {
		.name = "comip-pwm",
		.owner = THIS_MODULE,
	},
	.probe = comip_pwm_probe,
	.remove = comip_pwm_remove,
};

static int __init comip_pwm_init(void)
{
	return platform_driver_register(&comip_pwm_driver);
}

static void __exit comip_pwm_exit(void)
{
	platform_driver_unregister(&comip_pwm_driver);
}

subsys_initcall(comip_pwm_init);
module_exit(comip_pwm_exit);

