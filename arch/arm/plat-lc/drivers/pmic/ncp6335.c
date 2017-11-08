
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <plat/comip-pmic.h>

#define NCP6335_I2C_ADDR			(0x1c)

/* Register address. */
#define NCP6335_REG_INTACK			(0x00)
#define NCP6335_REG_INTSEN			(0x01)
#define NCP6335_REG_INTMASK			(0x02)
#define NCP6335_REG_PID				(0x03)
#define NCP6335_REG_RID				(0x04)
#define NCP6335_REG_FID				(0x05)
#define NCP6335_REG_PROGVSEL1			(0x10)
#define NCP6335_REG_PROGVSEL0			(0x11)
#define NCP6335_REG_PGOOD			(0x12)
#define NCP6335_REG_TIME			(0x13)
#define NCP6335_REG_COMMAND			(0x14)
#define NCP6335_REG_MODULE			(0x15)
#define NCP6335_REG_LIMCONF			(0x16)

/* Register bit. */
/* NCP6335_REG_PROGVSEL* & NCP6335_REG_RESERVED* */
#define NCP6335_MASK_PROGVSEL_ENVSEL		(0x80)
#define NCP6335_MASK_PROGVSEL_VSEL		(0x7f)

struct ncp6335_reg {
	u8 reg;
	u8 val;
	u8 mask;
};

static struct i2c_client *ncp6335_client;

static int ncp6335_reg_read(u8 reg, u8 *value)
{
	int ret;

	if (!ncp6335_client)
		return -1;

	ret = i2c_smbus_read_byte_data(ncp6335_client, reg);
	if(ret < 0)
		return ret;

	*value = (u8)ret;

	return 0;
}

static int ncp6335_reg_write(u8 reg, u8 value)
{
	int ret;

	if (!ncp6335_client)
		return -1;

	ret = i2c_smbus_write_byte_data(ncp6335_client, reg, value);
	if(ret > 0)
		ret = 0;

	return ret;
}

static u8 ncp6335_vsel_get(int mv)
{
	u8 vsel;

	vsel = (mv - 600) * 100 / 625;
	if (vsel > NCP6335_MASK_PROGVSEL_VSEL)
		vsel = NCP6335_MASK_PROGVSEL_VSEL;

	return vsel;
}

static int ncp6335_vout_get(u8 vsel)
{
	int mv;

	if (vsel > NCP6335_MASK_PROGVSEL_VSEL)
		vsel = NCP6335_MASK_PROGVSEL_VSEL;

	mv = ((int)vsel * 625) / 100 + 600;

	return mv;
}

static int ncp6335_voltage_get(u8 module, u8 param, int *pmv)
{
	u8 power_id = PMIC_DIRECT_POWER_ID(module);
	u8 vsel = 0;
	int ret = -EINVAL;

	if (power_id == PMIC_DCDC2) {
		switch (param) {
		case 0:
			ret = ncp6335_reg_read(NCP6335_REG_PROGVSEL0, &vsel);
			break;
		case 1:
			ret = ncp6335_reg_read(NCP6335_REG_PROGVSEL1, &vsel);
			break;
		default:
			return -EINVAL;
		}

		if (!ret)
			*pmv = ncp6335_vout_get(vsel & NCP6335_MASK_PROGVSEL_VSEL);
	}

	return ret;
}

static int ncp6335_voltage_set(u8 module, u8 param, int mv)
{
	u8 power_id = PMIC_DIRECT_POWER_ID(module);
	u8 vsel;
	u8 reg;
	int ret = -EINVAL;

	if (power_id == PMIC_DCDC2) {
		vsel = ncp6335_vsel_get(mv);

		switch (param) {
		case 0:
			reg = NCP6335_REG_PROGVSEL0;
			break;
		case 1:
			reg = NCP6335_REG_PROGVSEL1;
			break;
		default:
			return -EINVAL;
		}

		ret = ncp6335_reg_write(reg, NCP6335_MASK_PROGVSEL_ENVSEL | vsel);
	}

	return ret;
}

static int ncp6335_check(void)
{
	int ret;
	u8 val;

	ret = ncp6335_reg_read(NCP6335_REG_FID, &val);
	if (!ret) {
		printk(KERN_ERR "NCP6335: fid 0x%02x\n", val);

		ret = ncp6335_reg_read(NCP6335_REG_PROGVSEL0, &val);
		if (ret)
			goto no_dev;

		if (!(val & NCP6335_MASK_PROGVSEL_ENVSEL)) {
			printk(KERN_ERR "NCP6335: disabled\n");
			return 0;
		}

		return 1;
	}

no_dev:
	printk(KERN_ERR "NCP6335: no device\n");
	return 0;
}

#ifdef CONFIG_PM
static int ncp6335_suspend(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static int ncp6335_resume(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static struct dev_pm_ops ncp6335_pm_ops = {
	.suspend = ncp6335_suspend,
	.resume = ncp6335_resume,
};
#endif

static struct pmic_ext_power ncp6335_ep_power = {
	.module = PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2),
	.voltage_get = ncp6335_voltage_get,
	.voltage_set = ncp6335_voltage_set,
};

static int ncp6335_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int ret = 0;

	ncp6335_client = client;

	if (!ncp6335_check())
		return -ENODEV;

	ret = pmic_ext_power_register(&ncp6335_ep_power);
	if (ret)
		printk(KERN_ERR "NCP6335: register external power failed!\n");

	return ret;
}

static int __exit ncp6335_remove(struct i2c_client *client)
{
	pmic_ext_power_unregister(&ncp6335_ep_power);

	ncp6335_client = NULL;

	return 0;
}

static const struct i2c_device_id ncp6335_id[] = {
	{ "ncp6335d", 0 },
	{ "ncp6335f", 0 },
	{ },
};

static struct i2c_driver ncp6335_driver = {
	.driver = {
		.name = "ncp6335",
#ifdef CONFIG_PM
		.pm = &ncp6335_pm_ops,
#endif
	},
	.probe		= ncp6335_probe,
	.remove 	= __exit_p(ncp6335_remove),
	.id_table	= ncp6335_id,
};

static int __init ncp6335_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&ncp6335_driver);
	if (ret) {
		printk(KERN_WARNING "NCP6335: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit ncp6335_exit(void)
{
	i2c_del_driver(&ncp6335_driver);
}

subsys_initcall(ncp6335_init);
module_exit(ncp6335_exit);

MODULE_AUTHOR("zouqiao <zouqiao@leadcoretech.com>");
MODULE_DESCRIPTION("ncp6335 driver");
MODULE_LICENSE("GPL");

