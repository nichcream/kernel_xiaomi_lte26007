/*
 * This file is based on the drivers/i2c/chips/comip_1120.c
 *
 * Copyright (c) 2010  LeadCoreTech Corp.
 *
 * The Free Software Foundation; version 2 of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/i2c.h>

#include <plat/comip-pmic.h>

#define comip_1120_ADDRESS			0x1a 

/* Each client has this additional data */
struct comip_1120_data {
	struct i2c_client client;
	struct mutex update_lock;

	u32 valid;
};

static struct i2c_client *comip_1120_client = NULL;

int comip_1120_i2c_read(u8 reg)
{
	int ret;
	ret = i2c_smbus_read_byte_data(comip_1120_client, reg);
	return ret;
}

int comip_1120_i2c_write(u8 value, u8 reg)
{
	int ret;
	ret = i2c_smbus_write_byte_data(comip_1120_client, reg, value);
	//udelay(500);

	return ret;
}

int comip_1120_i2c_getbit(u8 reg, u8 bit)
{
	int value;
	value = i2c_smbus_read_byte_data(comip_1120_client, reg);
	return (value & (1 << bit));
}

int comip_1120_i2c_setbit(u8 reg, u8 bit)
{
	int ret;
	int value;
	value = i2c_smbus_read_byte_data(comip_1120_client, reg);
	value |= (1 << bit);
	ret = i2c_smbus_write_byte_data(comip_1120_client, reg, value);
	udelay(500);
	return ret;
}

int comip_1120_i2c_clrbit(u8 reg, u8 bit)
{
	int ret;
	int value;
	value = i2c_smbus_read_byte_data(comip_1120_client, reg);
	value &= ~(1 << bit);
	ret = i2c_smbus_write_byte_data(comip_1120_client, reg, value);
	return ret;
}

//extern int lc1100l_reg_read8(u8 reg, u8 *value);
/* This function is called by i2c_probe */
static int comip_1120_probe(struct i2c_client *client,
				const struct i2c_device_id *id)

{
	struct comip_1120_data *data;

	printk(KERN_INFO"comip_1120 probe\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA
				     | I2C_FUNC_SMBUS_READ_BYTE))
		return 0;

	if (!(data = kzalloc(sizeof(struct comip_1120_data), GFP_KERNEL)))
		return -ENOMEM;
	comip_1120_client = client;
	/* Init real i2c_client */
	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	return 0;
}

/* Will be called for both the real client and the fake client */
static int comip_1120_remove(struct i2c_client *client)

{
	struct comip_1120_data *data = i2c_get_clientdata(client);

	if (data)		/* real client */
		kfree(data);

	return 0;
}

static const struct i2c_device_id comip_1120_id[] = {
    { "comip_1120", 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, comip_1120_id);

/* This is the driver that will be inserted */
static struct i2c_driver comip_1120_driver = {
	.driver = {
		   .name = "comip_1120",
		   },
	.probe = comip_1120_probe,
	.remove = comip_1120_remove,
	.id_table = comip_1120_id,
};

static int __init comip_1120_i2c_init(void)
{
	return i2c_add_driver(&comip_1120_driver);
}

static void __exit comip_1120_i2c_exit(void)
{
	i2c_del_driver(&comip_1120_driver);
}

EXPORT_SYMBOL(comip_1120_i2c_read);
EXPORT_SYMBOL(comip_1120_i2c_write);
EXPORT_SYMBOL(comip_1120_i2c_setbit);
EXPORT_SYMBOL(comip_1120_i2c_getbit);
EXPORT_SYMBOL(comip_1120_i2c_clrbit);


MODULE_AUTHOR("Jerry Huang <jerry.huang@windriver.com>");
MODULE_DESCRIPTION("comip_1120 i2c driver");
MODULE_LICENSE("GPL");

module_init(comip_1120_i2c_init);
module_exit(comip_1120_i2c_exit);
