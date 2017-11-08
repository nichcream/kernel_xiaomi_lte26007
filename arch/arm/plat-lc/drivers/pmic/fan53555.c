
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <plat/comip-pmic.h>

#define FAN53555_I2C_ADDR			(0x60)

/* Register address. */
#define FAN53555_REG_VSEL0			(0x00)
#define FAN53555_REG_VSEL1			(0x01)
#define FAN53555_REG_CONTROL			(0x02)
#define FAN53555_REG_ID1			(0x03)
#define FAN53555_REG_ID2			(0x04)
#define FAN53555_REG_MONITOR			(0x05)

/* Register bit. */
/* FAN53555_REG_VSEL0* & FAN53555_REG_VSEL1* */
#define FAN53555_MASK_BUCK_EN		(0x80)
#define FAN53555_MASK_MODE		(0x40)
#define FAN53555_MASK_NSEL		(0x3f)

/* FAN53555_REG_CONTROL */
#define FAN53555_MASK_OUTPUT_DISCHARGE		(0x80)
#define FAN53555_MASK_SLEW			(0x70)

struct fan53555_reg {
	u8 reg;
	u8 val;
	u8 mask;
};

static struct i2c_client *fan53555_client;

static int fan53555_reg_read(u8 reg, u8 *value)
{
	int ret;

	if (!fan53555_client)
		return -1;

	ret = i2c_smbus_read_byte_data(fan53555_client, reg);
	if(ret < 0)
		return ret;

	*value = (u8)ret;

	return 0;
}

static int fan53555_reg_write(u8 reg, u8 value)
{
	int ret;

	if (!fan53555_client)
		return -1;

	ret = i2c_smbus_write_byte_data(fan53555_client, reg, value);
	if(ret > 0)
		ret = 0;

	return ret;
}

static int fan53555_reg_bit_write(u8 reg, u8 mask, u8 value)
{
	u8 valo, valn;
	int ret;

	if (!mask)
		return -1;

	ret = fan53555_reg_read(reg, &valo);
	if (!ret) {
		valn = valo & ~mask;
		valn |= (value << (ffs(mask) - 1));
		ret = fan53555_reg_write(reg, valn);
	}

	return ret;
}

static u8 fan53555_vsel_get(int mv)
{
	u8 vsel;

	vsel = (mv - 603) * 1000 / 12826;
	if (vsel > FAN53555_MASK_NSEL)
		vsel = FAN53555_MASK_NSEL;

	return vsel;
}

static int fan53555_vout_get(u8 vsel)
{
	int mv;

	if (vsel > FAN53555_MASK_NSEL)
		vsel = FAN53555_MASK_NSEL;

	mv = ((int)vsel * 12826) / 1000 + 603;

	return mv;
}

static int fan53555_voltage_get(u8 module, u8 param, int *pmv)
{
	u8 power_id = PMIC_DIRECT_POWER_ID(module);
	u8 vsel = 0;
	int ret = -EINVAL;

	if (power_id == PMIC_DCDC2) {
		switch (param) {
		case 0:
			ret = fan53555_reg_read(FAN53555_REG_VSEL0, &vsel);
			break;
		case 1:
			ret = fan53555_reg_read(FAN53555_REG_VSEL1, &vsel);
			break;
		default:
			return -EINVAL;
		}

		if (!ret)
			*pmv = fan53555_vout_get(vsel & FAN53555_MASK_NSEL);
	}

	return ret;
}

static int fan53555_voltage_set(u8 module, u8 param, int mv)
{
	u8 power_id = PMIC_DIRECT_POWER_ID(module);
	u8 vsel;
	u8 reg;
	int ret = -EINVAL;

	if (power_id == PMIC_DCDC2) {
		vsel = fan53555_vsel_get(mv);

		switch (param) {
		case 0:
			reg = FAN53555_REG_VSEL0;
			break;
		case 1:
			reg = FAN53555_REG_VSEL1;
			break;
		default:
			return -EINVAL;
		}

		ret = fan53555_reg_bit_write(reg, FAN53555_MASK_NSEL, vsel);
	}

	return ret;
}

static int fan53555_check(void)
{
	int ret;
	u8 val;

	ret = fan53555_reg_read(FAN53555_REG_ID1, &val);
	if (!ret) {
		printk(KERN_ERR "FAN53555: id1 0x%02x\n", val);
		return 1;
	}

	return 0;
}

#ifdef CONFIG_PM
static int fan53555_suspend(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static int fan53555_resume(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static struct dev_pm_ops fan53555_pm_ops = {
	.suspend = fan53555_suspend,
	.resume = fan53555_resume,
};
#endif

static struct pmic_ext_power fan53555_ep_power = {
	.module = PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2),
	.voltage_get = fan53555_voltage_get,
	.voltage_set = fan53555_voltage_set,
};

static int __devinit fan53555_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int ret = 0;

	fan53555_client = client;

	if (!fan53555_check()) {
		printk(KERN_ERR "FAN53555: no device\n");
		return -ENODEV;
	}

	ret = pmic_ext_power_register(&fan53555_ep_power);
	if (ret)
		printk(KERN_ERR "FAN53555: register external power failed!\n");

	return ret;
}

static int __devexit fan53555_remove(struct i2c_client *client)
{
	pmic_ext_power_unregister(&fan53555_ep_power);

	fan53555_client = NULL;

	return 0;
}

static const struct i2c_device_id fan53555_id[] = {
	{ "fan53555", 0 },
	{ },
};

static struct i2c_driver fan53555_driver = {
	.driver = {
		.name = "fan53555",
#ifdef CONFIG_PM
		.pm = &fan53555_pm_ops,
#endif
	},
	.probe		= fan53555_probe,
	.remove 	= __devexit_p(fan53555_remove),
	.id_table	= fan53555_id,
};

static int __init fan53555_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&fan53555_driver);
	if (ret) {
		printk(KERN_WARNING "FAN53555: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit fan53555_exit(void)
{
	i2c_del_driver(&fan53555_driver);
}

subsys_initcall(fan53555_init);
module_exit(fan53555_exit);

MODULE_AUTHOR("fanjiangang <fanjiangang@leadcoretech.com>");
MODULE_DESCRIPTION("fan53555 driver");
MODULE_LICENSE("GPL");

