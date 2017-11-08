/* arch/arm/plat-lc/drivers/pmic/lc1160/lc1160_pwm.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the lc1160 pwm driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date			Author		Description
 *	1.0.0	2014-02-28	fanjiangang	created
 *
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <plat/pwm.h>
#include <plat/hardware.h>
#include <plat/comip-pmic.h>
#include <plat/lc1160.h>
#include <plat/lc1160-pwm.h>

struct lc1160_pwm_chip {
	struct pwm_chip chip;
};

#define NS_IN_HZ	(1000000000UL)
#define NS_TO_HZ(x)	(1000000000UL / (x))
#define PWMCLK	(3000000UL)

#define to_lc1160_pwm_chip(_chip) container_of(_chip, struct lc1160_pwm_chip, chip)

static int lc1160_pwm_request(struct pwm_chip *chip,
			struct pwm_device *pwm)
{
	struct lc1160_pwm_platform_data *pdata = chip->dev->platform_data;

	pdata->dev_init(pwm->pwm);

	return 0;
}

static int lc1160_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	u8 mask;

	mask = (pwm->pwm % 2) ? LC1160_REG_BITMASK_PWM1_EN : LC1160_REG_BITMASK_PWM0_EN;
	lc1160_reg_bit_write(LC1160_REG_PWM_CTL, mask, 0x01);

	return 0;
}

static void lc1160_pwm_disable(struct pwm_chip *chip,
	struct pwm_device *pwm)
{
	u8 mask;

	mask = (pwm->pwm % 2) ? LC1160_REG_BITMASK_PWM1_EN : LC1160_REG_BITMASK_PWM0_EN;
	lc1160_reg_bit_write(LC1160_REG_PWM_CTL, mask, 0x00);
}

static int lc1160_pwm_config(struct pwm_chip *chip,
	struct pwm_device *pwm, int duty_ns, int period_ns)
{
	u64 tmp;
	u8 mask;
	u8 reg;
	unsigned long period;
	unsigned long ocpy;

	if (period_ns > NS_IN_HZ || duty_ns > NS_IN_HZ)
		return -ERANGE;

	tmp = (u64)PWMCLK;
	do_div(tmp, NS_TO_HZ(period_ns) * 50);
	period = tmp - 1;
	ocpy = duty_ns * 100 / period_ns;

	reg = LC1160_REG_PWM_CTL;
	mask = (pwm->pwm % 2) ? LC1160_REG_BITMASK_PWM1_EN : LC1160_REG_BITMASK_PWM0_EN;
	lc1160_reg_bit_write(reg, mask, 0x00);
	reg = (pwm->pwm % 2) ? LC1160_REG_PWM1_P : LC1160_REG_PWM0_P;
	mask = LC1160_REG_BITMASK_PWM0_PERIOD;
	lc1160_reg_bit_write(reg, mask, period);
	reg = (pwm->pwm % 2) ? LC1160_REG_PWM1_OCPY : LC1160_REG_PWM0_OCPY;
	mask = LC1160_REG_BITMASK_PWM0_OCPY_RATIO;
	lc1160_reg_bit_write(reg, mask, ocpy);
	reg = LC1160_REG_PWM_CTL;
	mask = (pwm->pwm % 2) ? LC1160_REG_BITMASK_PWM1_UPDATE : LC1160_REG_BITMASK_PWM0_UPDATE;
	lc1160_reg_bit_write(reg, mask, 0x01);

	if (pwm && test_bit(PWMF_ENABLED, &pwm->flags)) {
		reg = LC1160_REG_PWM_CTL;
		mask = (pwm->pwm % 2) ? LC1160_REG_BITMASK_PWM1_EN : LC1160_REG_BITMASK_PWM0_EN;
		lc1160_reg_bit_write(reg, mask, 0x01);
	}

	return 0;
}

static const struct pwm_ops lc1160_pwm_ops = {
	.request = lc1160_pwm_request,
	.config = lc1160_pwm_config,
	.enable = lc1160_pwm_enable,
	.disable = lc1160_pwm_disable,
	.owner = THIS_MODULE,
};

static int lc1160_pwm_probe(struct platform_device *pdev)
{
	struct lc1160_pwm_chip *pwm_c;
	int ret = 0;

	dev_info(&pdev->dev, "lc1160_pwm_probe\n");

	pwm_c = kzalloc(sizeof(*pwm_c), GFP_KERNEL);
	if (!pwm_c) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	pwm_c->chip.dev = &pdev->dev;
	pwm_c->chip.ops = &lc1160_pwm_ops;
	pwm_c->chip.base = LC1160_PWM_BASE;
	pwm_c->chip.npwm = LC1160_PWM_NUM;

	ret = pwmchip_add(&pwm_c->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add pwm chip %d\n", ret);
		goto err_free;
	}

	platform_set_drvdata(pdev, pwm_c);

	return 0;

err_free:
	kfree(pwm_c);
	return ret;
}

static int __exit lc1160_pwm_remove(struct platform_device *pdev)
{
	struct lc1160_pwm_chip *pwm_c = platform_get_drvdata(pdev);

	if (!pwm_c)
		return -ENODEV;

	pwmchip_remove(&pwm_c->chip);
	kfree(pwm_c);
	return 0;
}

static struct platform_driver lc1160_pwm_driver = {
	.driver = {
		.name = "lc1160-pwm",
		.owner = THIS_MODULE,
	},
	.probe = lc1160_pwm_probe,
	.remove = lc1160_pwm_remove,
};

static int __init lc1160_pwm_init(void)
{
	return platform_driver_register(&lc1160_pwm_driver);
}

static void __exit lc1160_pwm_exit(void)
{
	platform_driver_unregister(&lc1160_pwm_driver);
}

subsys_initcall(lc1160_pwm_init);
module_exit(lc1160_pwm_exit);

