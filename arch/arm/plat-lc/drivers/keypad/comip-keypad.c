/* drivers/input/keyboard/comip-keypad.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip keypad driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date		Author		Description
 *	1.0.0	2012-03-26	zouqiao		created
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/input/matrix_keypad.h>
#include <linux/slab.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/hardware.h>
#include <plat/keypad.h>

#define COMIP_KBS_DEBUG
#ifdef COMIP_KBS_DEBUG
#define KBS_PRINT(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define KBS_PRINT(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#endif

#define KBS_MATRIX_INTR		((1 << KBS_MTXKEY_PRESS_INTR) \
				| (1 << KBS_MTXKEY_RELEASE_INTR))

#define KBS_MATRIX_KEYS_MAX	(KBS_MATRIX_ROWS_MAX * KBS_MATRIX_COLS_MAX)
#define KBS_KEYS_MAX		(KBS_MATRIX_KEYS_MAX + KBS_DIRECT_KEYS_MAX)

#define KBS_MTXKEY_AS_REG(row)	(KBS_MTXKEY_ASREG0 + (((row) / 4) * 4))
#define KBS_MTXKEY_AS_BIT(row)	(((row) % 4) * 8)

struct comip_keypad {
	struct comip_keypad_platform_data *pdata;
	struct clk *clk;
	struct input_dev *input_dev;
	void __iomem *base;
	int irq;

	/* State row bits of each column scan. */
	unsigned char matrix_key_state[KBS_MATRIX_ROWS_MAX];

	/* Key codes. */
	unsigned short keycodes[KBS_KEYS_MAX];
};

static inline void comip_keypad_writel(struct comip_keypad *keypad,
		int idx, unsigned int val)
{
	writel(val, keypad->base + idx);
}

static inline unsigned int comip_keypad_readl(struct comip_keypad *keypad,
		int idx)
{
	return readl(keypad->base + idx);
}

static void comip_keypad_build_keycode(struct comip_keypad *keypad)
{
	struct comip_keypad_platform_data *pdata = keypad->pdata;
	struct input_dev *input_dev = keypad->input_dev;
	unsigned short keycode;
	int i;

	for (i = 0; i < pdata->matrix_key_map_size; i++) {
		unsigned int key = pdata->matrix_key_map[i];
		unsigned int row = KEY_ROW(key);
		unsigned int col = KEY_COL(key);
		unsigned int scancode = MATRIX_SCAN_CODE(row, col,
							 KBS_MATRIX_ROW_SHIFT);

		keycode = KEY_VAL(key);
		keypad->keycodes[scancode] = keycode;
		__set_bit(keycode, input_dev->keybit);
	}

	for (i = 0; i < pdata->direct_key_num; i++) {
		keycode = pdata->direct_key_map[i];
		keypad->keycodes[KBS_MATRIX_KEYS_MAX + i] = keycode;
		__set_bit(keycode, input_dev->keybit);
	}

	__clear_bit(KEY_RESERVED, input_dev->keybit);
}

static void comip_keypad_scan_matrix(struct comip_keypad *keypad)
{
	struct comip_keypad_platform_data *pdata = keypad->pdata;
	struct input_dev *input_dev = keypad->input_dev;
	unsigned char col_state[KBS_MATRIX_ROWS_MAX];
	unsigned int changed;
	unsigned int pressed;
	unsigned int val;
	unsigned int col, row;

	memset(col_state, 0, sizeof(col_state));

	for (row = 0; row < pdata->matrix_key_rows; row++) {
		val = comip_keypad_readl(keypad, KBS_MTXKEY_AS_REG(row));
		col_state[row] = (val >> (KBS_MTXKEY_AS_BIT(row)))
			& ((1 << pdata->matrix_key_cols) - 1);
	}

	for (row = 0; row < pdata->matrix_key_rows; row++) {
		changed = col_state[row] ^ keypad->matrix_key_state[row];
		if (!changed)
			continue;

		for (col = 0; col < pdata->matrix_key_cols; col++) {
			if (!(changed & (1 << col)))
				continue;

			pressed = col_state[row] & (1 << col);

			val = MATRIX_SCAN_CODE(row, col, KBS_MATRIX_ROW_SHIFT);

			KBS_PRINT("key %s, row: %d, col: %d, code : %d\n",
				pressed ? "pressed" : "released",
				row, col, keypad->keycodes[val]);

			input_event(input_dev, EV_MSC, MSC_SCAN, val);
			input_report_key(input_dev,
					keypad->keycodes[val], pressed);
		}
		input_sync(keypad->input_dev);
	}

	memcpy(keypad->matrix_key_state, col_state,
		sizeof(keypad->matrix_key_state));
}

static irqreturn_t comip_keypad_irq_handler(int irq, void *dev_id)
{
	struct comip_keypad *keypad = dev_id;
	unsigned long status;

	status = comip_keypad_readl(keypad, KBS_MTXROT_INT);

	/* Clear interrupt. */
	comip_keypad_writel(keypad, KBS_MTXROT_INT, ~0x0);

	if (status & KBS_MATRIX_INTR)
		comip_keypad_scan_matrix(keypad);

	return IRQ_HANDLED;
}

static void comip_keypad_start(struct comip_keypad *keypad)
{
	struct comip_keypad_platform_data *pdata = keypad->pdata;
	unsigned int col_mask, row_mask;
	unsigned int val;

	clk_enable(keypad->clk);

	/* Set matrix keys mask. */
	col_mask = (~((1 << pdata->matrix_key_cols) - 1)) & 0xff;
	row_mask = (~((1 << pdata->matrix_key_rows) - 1)) & 0xff;
	comip_keypad_writel(keypad, KBS_MASK, (col_mask << 8) | row_mask);

	/* Set debounce interval. */
	val = pdata->debounce_interval * 2;
	comip_keypad_writel(keypad, KBS_DEBOUNCE_INTVAL, val);

	/* Set detect interval. */
	val = pdata->detect_interval * 2;
	comip_keypad_writel(keypad, KBS_DETECT_INTVAL, (val << 8) | val);

	/* Enable matrix keys with automatic scan. */
	val = (1 << KBS_MTXKEY_AUTOSCAN_EN) | (1 << KBS_MTXKEY_EN);
	if (pdata->internal_pull_up)
		val |= (1 << KBS_MTXKEY_PULL_CFG);
	comip_keypad_writel(keypad, KBS_CTL, val);

	/* Enable interrupts. */
	comip_keypad_writel(keypad, KBS_MTXROT_INT_EN, KBS_MATRIX_INTR);
}

static void comip_keypad_stop(struct comip_keypad *keypad)
{
	/* Disable interrupts. */
	comip_keypad_writel(keypad, KBS_MTXROT_INT_EN, 0x0);

	/* Clear interrupts. */
	comip_keypad_writel(keypad, KBS_MTXROT_INT, ~0x0);

	clk_disable(keypad->clk);
}

static int comip_keypad_open(struct input_dev *dev)
{
	struct comip_keypad *keypad = input_get_drvdata(dev);

	comip_keypad_start(keypad);

	return 0;
}

static void comip_keypad_close(struct input_dev *dev)
{
	struct comip_keypad *keypad = input_get_drvdata(dev);

	comip_keypad_stop(keypad);
}

#ifdef CONFIG_PM
static int comip_keypad_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_keypad *keypad = platform_get_drvdata(pdev);
	struct input_dev *input_dev = keypad->input_dev;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		comip_keypad_stop(keypad);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int comip_keypad_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_keypad *keypad = platform_get_drvdata(pdev);
	struct input_dev *input_dev = keypad->input_dev;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		comip_keypad_start(keypad);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static const struct dev_pm_ops comip_keypad_pm_ops = {
	.suspend	= comip_keypad_suspend,
	.resume		= comip_keypad_resume,
};
#endif

static int __devinit comip_keypad_probe(struct platform_device *pdev)
{
	struct comip_keypad_platform_data *pdata = pdev->dev.platform_data;
	struct comip_keypad *keypad;
	struct input_dev *input_dev;
	struct resource *res;
	int irq, error;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	if (!pdata->matrix_key_map || !pdata->matrix_key_map_size
		|| (pdata->matrix_key_cols > KBS_MATRIX_COLS_MAX)
		|| (pdata->matrix_key_rows > KBS_MATRIX_ROWS_MAX)) {
		dev_err(&pdev->dev, "invalid platform data\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get keypad irq\n");
		return -ENXIO;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get I/O memory\n");
		return -ENXIO;
	}

	keypad = kzalloc(sizeof(struct comip_keypad), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!keypad || !input_dev) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		error = -ENOMEM;
		goto failed_free;
	}

	keypad->pdata = pdata;
	keypad->input_dev = input_dev;
	keypad->irq = irq;

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to request I/O memory\n");
		error = -EBUSY;
		goto failed_free;
	}

	keypad->base = ioremap(res->start, resource_size(res));
	if (keypad->base == NULL) {
		dev_err(&pdev->dev, "failed to remap I/O memory\n");
		error = -ENXIO;
		goto failed_free_mem;
	}

	keypad->clk = clk_get(&pdev->dev, "kbs_clk");
	if (IS_ERR(keypad->clk)) {
		dev_err(&pdev->dev, "failed to get keypad clock\n");
		error = PTR_ERR(keypad->clk);
		goto failed_free_io;
	}

	input_dev->name = pdev->name;
	input_dev->id.bustype = BUS_HOST;
	input_dev->open = comip_keypad_open;
	input_dev->close = comip_keypad_close;
	input_dev->dev.parent = &pdev->dev;

	input_dev->keycode = keypad->keycodes;
	input_dev->keycodesize = sizeof(keypad->keycodes[0]);
	input_dev->keycodemax = ARRAY_SIZE(keypad->keycodes);

	input_set_drvdata(input_dev, keypad);

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(input_dev, EV_MSC, MSC_SCAN);

	comip_keypad_build_keycode(keypad);

	error = request_irq(irq, comip_keypad_irq_handler, IRQF_DISABLED,
			    pdev->name, keypad);
	if (error) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		goto failed_put_clk;
	}

	/* Register the input device. */
	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev, "failed to register input device\n");
		goto failed_free_irq;
	}

	platform_set_drvdata(pdev, keypad);

	comip_keypad_open(input_dev);

	return 0;

failed_free_irq:
	free_irq(irq, pdev);
failed_put_clk:
	clk_put(keypad->clk);
failed_free_io:
	iounmap(keypad->base);
failed_free_mem:
	release_mem_region(res->start, resource_size(res));
failed_free:
	input_free_device(input_dev);
	kfree(keypad);
	return error;
}

static int __devexit comip_keypad_remove(struct platform_device *pdev)
{
	struct comip_keypad *keypad = platform_get_drvdata(pdev);
	struct resource *res;

	free_irq(keypad->irq, pdev);
	clk_put(keypad->clk);

	input_unregister_device(keypad->input_dev);
	iounmap(keypad->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	platform_set_drvdata(pdev, NULL);
	kfree(keypad);

	return 0;
}

static struct platform_driver comip_keypad_driver = {
	.probe		= comip_keypad_probe,
	.remove		= __devexit_p(comip_keypad_remove),
	.driver		= {
		.name	= "comip-keypad",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &comip_keypad_pm_ops,
#endif
	},
};

static int __init comip_keypad_init(void)
{
	return platform_driver_register(&comip_keypad_driver);
}

static void __exit comip_keypad_exit(void)
{
	platform_driver_unregister(&comip_keypad_driver);
}

module_init(comip_keypad_init);
module_exit(comip_keypad_exit);

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP Keypad Controller Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:comip-keypad");