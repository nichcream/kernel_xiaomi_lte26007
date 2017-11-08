/*  
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION      	DATE			AUTHOR        	Note
 * 1.0		2013-03-08		wuxuemin    	add Bridge Chip for Mipi2LVDSRGB
 *     
 *
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/earlysuspend.h>
#include <linux/workqueue.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <plat/mfp.h>
#include <mach/gpio.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <plat/comip-icn6201.h>
#include <plat/hardware.h>

#include "comipfb_dev.h"

#define ICN6201_DEBUG
#ifdef ICN6201_DEBUG
#define ICN6201_PRINT(fmt, args...) printk(KERN_ERR "[ICN6201]" fmt, ##args)
#else
#define ICN6201_PRINT(fmt, args...) printk(KERN_DEBUG "[ICN6201]" fmt, ##args)
#endif

static struct i2c_client *this_client;

struct icn6201_info {
	struct device *dev;
	struct i2c_client *client;
	struct lvds_icn6201_platform_data *pdata;
	struct early_suspend early_suspend;
	struct work_struct irq_event_work;
	struct workqueue_struct *irq_event_workqueue;
	struct clk *mclk_parent;
	struct clk *mclk;
	int irq;
};

static  struct icn6201_reg reg_table[] = {
		{0x20, 0x00},   {0x21, 0x20},   {0x22, 0x35},
		{0x23, 0x28},   {0x24, 0x14},   {0x25, 0x28},
		{0x26, 0x00},   {0x27, 0x1e},   {0x28, 0x01},
		{0x29, 0x07},   {0x34, 0x80},   {0x36, 0x28},
		{0xb5, 0xa0},   {0x5c, 0xff},   {0x13, 0x10},
		{0x56, 0x92},   {0x6b, 0x32},   {0x69, 0x4c},
		{0xb6, 0x20},   {0x51, 0x20},   {0x09, 0x10}
};

#define REG_NUM ARRAY_SIZE(reg_table)

static int icn6201_i2c_rxdata(unsigned char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		ICN6201_PRINT("i2c read error: %d\n", ret);
	
	return ret;
}

static int icn6201_i2c_txdata(unsigned char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		ICN6201_PRINT("write error: %d\n", ret);

	return ret;
}

static int icn6201_write_reg(unsigned char addr, unsigned char para)
{
	unsigned char buf[2];
	int ret = -1;

	buf[0] = addr;
	buf[1] = para;

	//printk("write: reg = %d, val = %d\n",buf[0], buf[1]);
	ret = icn6201_i2c_txdata(buf, 2);
	if (ret < 0) {
		ICN6201_PRINT("write reg failed! 0x%x ret: %d", buf[0], ret);
		return -1;
	}

	return 0;
}

static int icn6201_read_reg(unsigned char addr,unsigned char *pdata)
{
	int ret = 0;
	unsigned char data = addr;

	ret = icn6201_i2c_rxdata(&data, 1);
	if (ret < 0)
		ICN6201_PRINT("read reg failed! 0x%x ret: %d", data, ret);
	*pdata = data;

	return ret;
}

static int icn6201_write_reg_arry(struct icn6201_reg *reg, int num)
{
	int i, ret = 0;

	//unsigned char addr;
	//unsigned char data;
	//printk("icn6201_write_reg_arry\n");
	for (i = 0; i < num; i++) {
		ret = icn6201_write_reg(reg->addr, reg->val);
		if (ret < 0) 
			return ret;
		//mdelay(5);
		//icn6201_read_reg(reg->addr, &data);
		//printk("read: reg = 0x%x, val = 0x%x\n",reg->addr, data);
		reg++;
	}

	return ret;
}

static int icn6201_clk_enable(struct icn6201_info *info)
{
	struct lvds_icn6201_platform_data *pdata = info->pdata;
	struct clk *mclk_parent;
	int err = 0;

	if (!pdata->mclk_parent_name || !pdata->mclk_name) {
		ICN6201_PRINT("not pdata->mclk_parent, not pdata->mclk");
		return 0;
	}
	
	info->mclk_parent = clk_get(NULL, pdata->mclk_parent_name);
	if (IS_ERR(info->mclk_parent)) {
		ICN6201_PRINT("Cannot get  parent clock\n");
		err = PTR_ERR(info->mclk_parent);
		goto out;
	}

	info->mclk = clk_get(NULL, pdata->mclk_name);
	if (IS_ERR(info->mclk)) {
		ICN6201_PRINT("Cannot get mclk\n");
		err = PTR_ERR(info->mclk);
		goto out_clk_put;
	}

	clk_set_parent(info->mclk, info->mclk_parent);
	clk_set_rate(info->mclk, pdata->clk_rate);
	clk_enable(info->mclk);

	return 0;
out_clk_put:
	clk_put(info->mclk_parent);
out:
	return err;
}

void icn6201_chip_init(void *reg_table, int num)
{
	struct icn6201_reg *reg = NULL;
	reg = (struct icn6201_reg *)reg_table;

	icn6201_write_reg_arry(reg, num);
}
EXPORT_SYMBOL(icn6201_chip_init);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void icn6201_suspend(struct early_suspend *handler)
{
	struct icn6201_info *info;

	info = container_of(handler, struct icn6201_info, early_suspend);
	
	ICN6201_PRINT("enter icn6201_suspend \n");
	return 0;
}

static void icn6201_resume(struct early_suspend *handler)
{
	struct icn6201_info *info;

	info = container_of(handler, struct icn6201_info, early_suspend);

	ICN6201_PRINT("enter icn6201_resume \n");

	return 0;
}
#endif

static ssize_t lvds_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char val;

	return sprintf(buf, "lvds_test show\n");

	
}

static ssize_t lvds_write_store(struct device *dev, struct device_attribute *attr,
                                                                const char *buf, size_t count)
{
	unsigned char reg, val;

	sscanf(buf, "%d%d", &reg, &val);
	ICN6201_PRINT("lvds_test write reg %d, val %d\n",reg,val);

	icn6201_write_reg(reg, val);

	return count;
}

DEVICE_ATTR(lvds_test, S_IRUGO | S_IWUSR, lvds_read_show, lvds_write_store);

static void lvds_sysfs_init(struct icn6201_info *info)
{
	device_create_file(info->dev, &dev_attr_lvds_test);
}

static void lvds_sysfs_remove(struct icn6201_info *info)
{
	device_remove_file(info->dev, &dev_attr_lvds_test);
}

static irqreturn_t icn6201_irq_handle(int irq, void *dev_id)
{
	struct icn6201_info *info;

	info = (struct icn6201_info *)dev_id;

	disable_irq_nosync(info->irq);

	if (!work_pending(&info->irq_event_work))
		queue_work(info->irq_event_workqueue, &info->irq_event_work);

	return IRQ_HANDLED;	
}

static void icn6201_work_func(struct work_struct *work)
{
	struct icn6201_info *info = container_of(work, struct icn6201_info, irq_event_work);

	printk("icn6201 work func\n");
	enable_irq(info->irq);
}

static int icn6201_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct icn6201_info *info;
	struct lvds_icn6201_platform_data *pdata;
	int ret = 0;

	ICN6201_PRINT("icn6201_probe enter\n");
	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		ICN6201_PRINT("No platform data\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		return -ENODEV;
	}
	
	/* private data allocation */
	info = kzalloc(sizeof(struct icn6201_info), GFP_KERNEL);

	if (!info) {
		ICN6201_PRINT("Cannot allocate memory for icn6201_info\n");
		return -ENOMEM;
	}

	this_client = client;
	info->dev = &client->dev;
	info->pdata = pdata;
	info->client = client;
	info->irq = gpio_to_irq(pdata->gpio_irq);

	INIT_WORK(&info->irq_event_work, icn6201_work_func);
	info->irq_event_workqueue = create_singlethread_workqueue("icn6201_wq");

#ifdef REGISTER_IRQ
	ret = request_irq(info->irq, icn6201_irq_handle,IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_SHARED, "icn6201", info);
#endif	
	i2c_set_clientdata(client, info);
	
	if (pdata->power)
		pdata->power(1);
	if (pdata->lvds_en)
		pdata->lvds_en(1);

	icn6201_clk_enable(info);

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = icn6201_suspend;
	info->early_suspend.resume	= icn6201_resume;
	register_early_suspend(&info->early_suspend);
#endif

	/* Proc fs init */
	lvds_sysfs_init(info);

	printk("icn2601 finish\n");

	return 0;

err_icn6201_power:
	i2c_set_clientdata(client, NULL);
	kfree(info);

	return ret;
}

static int icn6201_remove(struct i2c_client *client)
{
	struct icn6201_info *info = i2c_get_clientdata(client);
	
	i2c_set_clientdata(client, NULL);
	lvds_sysfs_remove(info);
	destroy_workqueue(info->irq_event_workqueue);

	kfree(info);
	
	return 0;
}

static const struct i2c_device_id icn6201_id[] = {
	{ ICN6201_NAME, 0 },
	{ },
};

static struct i2c_driver icn6201_driver = {
	.driver = {
		.name = ICN6201_NAME,
	},
	.probe		= icn6201_probe,
	.remove 	= icn6201_remove,
	.id_table	= icn6201_id,
	
};

static int __init icn6201_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&icn6201_driver);
	if (ret) {
		printk(KERN_WARNING "icn6201: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit icn6201_exit(void)
{
	i2c_del_driver(&icn6201_driver);
}

fs_initcall(icn6201_init);
module_exit(icn6201_exit);

MODULE_DESCRIPTION("mipi to Lvds inc6201 chip driver");
MODULE_LICENSE("GPL");

