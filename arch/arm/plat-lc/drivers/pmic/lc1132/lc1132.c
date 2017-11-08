/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013 LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the lc1132 hardware plarform's i2c driver.
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
#include <plat/lc1132.h>
#include <plat/lc1132-pmic.h>

static struct i2c_client *lc1132_client;
static DEFINE_MUTEX(lc1132_reg_lock);

int lc1132_reg_read(u8 reg, u8 *value)
{
	int ret;

	if (!lc1132_client)
		return -1;

	ret = i2c_smbus_read_byte_data(lc1132_client, reg);
	if(ret < 0)
		return ret;

	*value = (u8)ret;

	return 0;
}
EXPORT_SYMBOL(lc1132_reg_read);

int lc1132_reg_write(u8 reg, u8 value)
{
	int ret;

	if (!lc1132_client)
		return -1;

	ret = i2c_smbus_write_byte_data(lc1132_client, reg, value);
	if(ret > 0)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(lc1132_reg_write);

int lc1132_reg_bit_write(u8 reg, u8 mask, u8 value)
{
	u8 valo, valn;
	int ret;

	if (!mask)
		return -EINVAL;

	mutex_lock(&lc1132_reg_lock);

	ret = lc1132_reg_read(reg, &valo);
	if (!ret) {
		valn = valo & ~mask;
		valn |= (value << (ffs(mask) - 1));
		ret = lc1132_reg_write(reg, valn);
	}

	mutex_unlock(&lc1132_reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1132_reg_bit_write);

int lc1132_reg_bit_set(u8 reg, u8 value)
{
	u8 val;
	int ret;

	mutex_lock(&lc1132_reg_lock);

	ret = lc1132_reg_read(reg, &val);
	if (!ret) {
		val |= value;
		ret = lc1132_reg_write(reg, val);
	}

	mutex_unlock(&lc1132_reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1132_reg_bit_set);

int lc1132_reg_bit_clr(u8 reg, u8 value)
{
	u8 val;
	int ret;

	mutex_lock(&lc1132_reg_lock);

	ret = lc1132_reg_read(reg, &val);
	if (!ret) {
		val &= ~value;
		ret = lc1132_reg_write(reg, val);
	}

	mutex_unlock(&lc1132_reg_lock);

	return ret;
}
EXPORT_SYMBOL(lc1132_reg_bit_clr);

static int lc1132_check(void)
{
	int ret = 0;
	u8 val;

	ret = lc1132_reg_read(LC1132_REG_VERSION, &val);
	if (ret)
		return 0;

	return 1;
}

static struct platform_device lc1132_pmic_device = {
	.name = "lc1132-pmic",
	.id   = -1,
};

static struct platform_device lc1132_battery_device = {
	.name = "lc1132-battery",
	.id   = -1,
};

static int __init lc1132_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct lc1132_platform_data *pdata;
	int err = 0;

	pdata = client->dev.platform_data;
	if (!pdata || !pdata->pmic_data) {
		dev_dbg(&client->dev, "No platform data\n");
		return -EINVAL;
	}

	lc1132_client = client;
	if ((pdata->flags & LC1132_FLAGS_DEVICE_CHECK) && !lc1132_check()) {
		printk(KERN_ERR "LC1132: Can't find the device\n");
		return -ENODEV;
	}

	lc1132_pmic_device.dev.platform_data = pdata->pmic_data;
	err = platform_device_register(&lc1132_pmic_device);
	if (err) {
		printk(KERN_ERR "LC1132: Register pmic device failed\n");
		goto exit;
	}

	if (!(pdata->flags & LC1132_FLAGS_NO_BATTERY_DEVICE)) {
		err = platform_device_register(&lc1132_battery_device);
		if (err) {
			printk(KERN_ERR "LC1132: Register battery device failed\n");
			goto exit_unregister_pmic_device;
		}
	}

	return 0;

exit_unregister_pmic_device:
	platform_device_unregister(&lc1132_pmic_device);
exit:
	lc1132_client = NULL;
	return err;
}

static int __exit lc1132_remove(struct i2c_client *client)
{
	struct lc1132_platform_data *pdata = client->dev.platform_data;

	if (!(pdata->flags & LC1132_FLAGS_NO_BATTERY_DEVICE))
		platform_device_unregister(&lc1132_battery_device);

	platform_device_unregister(&lc1132_pmic_device);

	lc1132_client = NULL;

	return 0;
}

static const struct i2c_device_id lc1132_id[] = {
	{ "lc1132", 0 },
	{ },
};

static struct i2c_driver lc1132_driver = {
	.driver = {
		   .name = "lc1132",
	},
	.probe		= lc1132_probe,
	.remove 	= __exit_p(lc1132_remove),
	.id_table	= lc1132_id,
};

static int __init lc1132_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&lc1132_driver);
	if (ret) {
		printk(KERN_WARNING "lc1132: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit lc1132_exit(void)
{
	i2c_del_driver(&lc1132_driver);
}

subsys_initcall(lc1132_init);
module_exit(lc1132_exit);

MODULE_AUTHOR("fanjiangang <fanjiangang@leadcoretech.com>");
MODULE_DESCRIPTION("LC1132 driver");
MODULE_LICENSE("GPL");
