/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013 LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the lc1160 hardware plarform's i2c driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2013-03-20	fanjiangang		created
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <plat/lc1160.h>
#include <plat/lc1160-pmic.h>
#include <plat/mfp.h>

static struct i2c_client *lc1160_client;
static DEFINE_MUTEX(lc1160_reg_lock);

int lc1160_reg_read(u8 reg, u8 *value)
{
	int ret;

	if (!lc1160_client)
		return -1;

	ret = i2c_smbus_read_byte_data(lc1160_client, reg);
	if(ret < 0)
		return ret;

	*value = (u8)ret;

	return 0;
}
EXPORT_SYMBOL(lc1160_reg_read);

int lc1160_reg_write(u8 reg, u8 value)
{
	int ret;

	if (!lc1160_client)
		return -1;

	ret = i2c_smbus_write_byte_data(lc1160_client, reg, value);
	if(ret > 0)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(lc1160_reg_write);

int lc1160_reg_bit_write(u8 reg, u8 mask, u8 value)
{
	u8 valo, valn;
	int ret;

	if (!mask)
		return -EINVAL;

	mutex_lock(&lc1160_reg_lock);

	ret = lc1160_reg_read(reg, &valo);
	if (!ret) {
		valn = valo & ~mask;
		valn |= (value << (ffs(mask) - 1));
		ret = lc1160_reg_write(reg, valn);
	}

	mutex_unlock(&lc1160_reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1160_reg_bit_write);

int lc1160_reg_bit_set(u8 reg, u8 value)
{
	u8 val;
	int ret;

	mutex_lock(&lc1160_reg_lock);

	ret = lc1160_reg_read(reg, &val);
	if (!ret) {
		val |= value;
		ret = lc1160_reg_write(reg, val);
	}

	mutex_unlock(&lc1160_reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1160_reg_bit_set);

int lc1160_reg_bit_clr(u8 reg, u8 value)
{
	u8 val;
	int ret;

	mutex_lock(&lc1160_reg_lock);

	ret = lc1160_reg_read(reg, &val);
	if (!ret) {
		val &= ~value;
		ret = lc1160_reg_write(reg, val);
	}

	mutex_unlock(&lc1160_reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1160_reg_bit_clr);

int lc1160_chip_version_get(void)
{
	u8 val, chip_version;

	lc1160_reg_read(LC1160_REG_REGBAK_CONTROL, &val);
	lc1160_reg_write(LC1160_REG_REGBAK_CONTROL, 1);
	lc1160_reg_read(LC1160_REG_CODEC_R1_BAK, &chip_version);
	lc1160_reg_write(LC1160_REG_REGBAK_CONTROL, val);

	/* return 0.  LC1160 version
	 *  return 1.  LC1160B version */
	 if(chip_version & (1 << 5))
		chip_version = LC1160_ECO1;
	 else
		chip_version = LC1160_ECO0;

	return	chip_version;
}
EXPORT_SYMBOL(lc1160_chip_version_get);

static int lc1160_check(void)
{
	int ret = 0;
	u8 val;

	ret = lc1160_reg_read(LC1160_REG_VERSION, &val);
	if (ret)
		return 0;

	return 1;
}

static struct platform_device lc1160_pmic_device = {
	.name = "lc1160-pmic",
	.id   = -1,
};

static struct platform_device lc1160_battery_device = {
	.name = "lc1160-battery",
	.id   = -1,
};

static int lc1160_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct lc1160_platform_data *pdata;
	int err = 0;

	pdata = client->dev.platform_data;
	if (!pdata || !pdata->pmic_data) {
		dev_dbg(&client->dev, "No platform data\n");
		return -EINVAL;
	}

	lc1160_client = client;
	if ((pdata->flags & LC1160_FLAGS_DEVICE_CHECK) && !lc1160_check()) {
		printk(KERN_ERR "LC1160: Can't find the device\n");
		return -ENODEV;
	}

	lc1160_pmic_device.dev.platform_data = pdata->pmic_data;
	err = platform_device_register(&lc1160_pmic_device);
	if (err) {
		printk(KERN_ERR "LC1160: Register pmic device failed\n");
		goto exit;
	}

	if (!(pdata->flags & LC1160_FLAGS_NO_BATTERY_DEVICE)) {
		err = platform_device_register(&lc1160_battery_device);
		if (err) {
			printk(KERN_ERR "LC1160: Register battery device failed\n");
			goto exit_unregister_pmic_device;
		}
	}

	return 0;

exit_unregister_pmic_device:
	platform_device_unregister(&lc1160_pmic_device);
exit:
	lc1160_client = NULL;
	return err;
}

static int __exit lc1160_remove(struct i2c_client *client)
{
	struct lc1160_platform_data *pdata = client->dev.platform_data;

	if (!(pdata->flags & LC1160_FLAGS_NO_BATTERY_DEVICE))
		platform_device_unregister(&lc1160_battery_device);

	platform_device_unregister(&lc1160_pmic_device);

	lc1160_client = NULL;

	return 0;
}

static void lc1160_shutdown(struct i2c_client *client)
{
#if defined(CONFIG_OTG_GPIO_CTL)
    int otg_gpio = MFP_PIN_GPIO(160);
    gpio_direction_output(otg_gpio, 1);
    gpio_set_value(otg_gpio,0);

    gpio_free(otg_gpio);
#endif
    return;
}

static const struct i2c_device_id lc1160_id[] = {
	{ "lc1160", 0 },
	{ },
};

static struct i2c_driver lc1160_driver = {
	.driver = {
		   .name = "lc1160",
	},
	.probe		= lc1160_probe,
	.remove 	= __exit_p(lc1160_remove),
       .shutdown = lc1160_shutdown,
	.id_table	= lc1160_id,
};

static int __init lc1160_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&lc1160_driver);
	if (ret) {
		printk(KERN_WARNING "lc1160: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit lc1160_exit(void)
{
	i2c_del_driver(&lc1160_driver);
}

subsys_initcall(lc1160_init);
module_exit(lc1160_exit);

MODULE_AUTHOR("fanjiangang <fanjiangang@leadcoretech.com>");
MODULE_DESCRIPTION("LC1160 driver");
MODULE_LICENSE("GPL");
