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
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/i2c.h>
#include <linux/earlysuspend.h>
#include <linux/workqueue.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <plat/mfp.h>
#include <mach/gpio.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <plat/za7783.h>
#include <plat/hardware.h>

#define CONFIG_REFCLK_LVDS 1

#define za7783_debug 1

#define ZA7783_DEBUG
#ifdef ZA7783_DEBUG
#define ZA7783_PRINT(fmt, args...) printk(KERN_ERR "[za7783]" fmt, ##args)
#else
#define ZA7783_PRINT(fmt, args...) printk(KERN_DEBUG "[za7783]" fmt, ##args)
#endif

#define ZA7783_SOFTRST_REG	0x09
#define ZA7783_SOFTRST_VAL	0x01
#define ZA7783_IRQ_EN_REG	0xE0
#define ZA7783_CLK_SEL_REG	0x0A
#define ZA7783_CLK_SEL_VAL	0x02 
#define ZA7783_DSI_CHANNEL_SEL_REG	0x0B
#define ZA7783_DSI_CHANNEL_SEL_VAL	0x01	

#define ZA7783_DSI_CLK_REG	0x12
#define ZA7783_DSI_CLK_VAL	0x1F
#define ZA7783_PLL_EN_REG	0x0D
#define ZA7783_PLL_EN_VAL	0x01
//#define LVDS_PROC

extern void lvds_mipi_poweroffon(void);
static index = 0;
struct za7783_info {
	struct i2c_client *client;
	struct za7783_platform_data *pdata;
	struct clk *mclk;
	struct mutex mutex;
	struct early_suspend	early_suspend;
	spinlock_t lock;
	int irq_enabled;
	int irq;
	struct work_struct sn_event_work;
	struct	workqueue_struct *sn_workqueue; 
};

struct za7783_reg
{
	u8 addr;
	u8 val;
};

static  struct za7783_reg reg_tbl[] = {
};

static  struct za7783_reg reg_tbx[] = {
		{0x00, 0x00},   {0x01, 0x00},   {0x02, 0x00},
		{0x03, 0x00},   {0x04, 0x00},   {0x05, 0x00},
		{0x06, 0x00},   {0x07, 0x00},   {0x08, 0x00},
		{0x09, 0x00},   {0x0a, 0x00},   {0x0b, 0x00},
		{0x0c, 0x00},   {0x0d, 0x00},   {0x0e, 0x00},
		{0x0f, 0x00},   {0x10, 0x00},   {0x11, 0x00},
		{0x12, 0x00},   {0x13, 0x00},   {0x14, 0x00},
		{0x15, 0x00},   {0x16, 0x00},   {0x17, 0x00},
		{0x18, 0x00},   {0x19, 0x00},   {0x1a, 0x00},
		{0x1b, 0x00},   {0x1c, 0x00},   {0x1d, 0x00},
		{0x1e, 0x00},   {0x1f, 0x00},   {0x20, 0x00},
		{0x21, 0x00},   {0x22, 0x00},   {0x23, 0x00},
		{0x24, 0x00},   {0x25, 0x00},   {0x26, 0x00},
		{0x27, 0x00},   {0x28, 0x00},   {0x29, 0x00},
		{0x2a, 0x00},   {0x2b, 0x00},   {0x2c, 0x00},
		{0x2d, 0x00},   {0x2e, 0x00},   {0x2f, 0x00},
		{0x30, 0x00},   {0x31, 0x00},   {0x32, 0x00},
		{0x33, 0x00},   {0x34, 0x00},   {0x35, 0x00},
		{0x36, 0x00},   {0x37, 0x00},   {0x38, 0x00},
		{0x39, 0x00},   {0x3a, 0x00},   {0x3b, 0x00},
		{0x3c, 0x00},   {0x3d, 0x00},   {0x3e, 0x00},
		{0x3f, 0x00},   {0x40, 0x00},   {0x41, 0x00},
		{0x42, 0x00},	{0xff, 0x00}
};

static struct i2c_client *this_client;

static int za7783_i2c_rxdata(char *rxdata, int length)
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
		ZA7783_PRINT("i2c read error: %d\n", ret);
	
	return ret;
}

static int za7783_i2c_txdata(char *txdata, int length)
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
		ZA7783_PRINT("write error: %d\n", ret);

	return ret;
}

static int za7783_write_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = za7783_i2c_txdata(buf, 2);
    if (ret < 0) {
        ZA7783_PRINT("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }
    
    return 0;
}

static int za7783_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[2];
	u8 data = 0;

	buf[0] = addr;    //register address
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;
	msgs[1].addr = this_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &data;

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		ZA7783_PRINT("i2c read error: %d\n", ret);

	*pdata = data;
	return ret;
}

int za7783_clk_enable(struct i2c_client *client)
{
	struct za7783_info *data = i2c_get_clientdata(client);
	struct za7783_platform_data *pdata = data->pdata;
	struct clk* mclk_parent;
	int err = 0;

	if (!pdata->mclk_parent_name || !pdata->mclk_name) {
		dev_info(&data->client->dev, " za7783 not set mclk, will use external clock\n");
		return 0;
	}
	
	mclk_parent = clk_get(&data->client->dev, pdata->mclk_parent_name);
	if (IS_ERR(mclk_parent)) {
		dev_err(&data->client->dev, "Cannot get pmu input parent clock \"%s\"\n",
			pdata->mclk_parent_name);
		err = PTR_ERR(mclk_parent);
		goto out;
	}

	data->mclk = clk_get(&data->client->dev, pdata->mclk_name);
	if (IS_ERR(data->mclk)) {
		dev_err(&data->client->dev, "Cannot get pum input clock \"%s\"\n",
			pdata->mclk_name);
		err = PTR_ERR(data->mclk);
		goto out_clk_put;
	}

	clk_set_parent(data->mclk, mclk_parent);
	clk_set_rate(data->mclk, pdata->mclk_rate);
	clk_enable(data->mclk);
out_clk_put:
	clk_put(mclk_parent);
out:
	return err;
}

#if 1//za7783_debug
static void za7783_test(void)
{
	int i=0;
	u8 tstvalue;

	while(reg_tbl[i].addr != 0xff)
	{
		za7783_read_reg(reg_tbl[i].addr, &tstvalue);
		
		ZA7783_PRINT("za7783: reg =0x%x; val=0x%x\n", reg_tbl[i].addr, tstvalue);
		i+=1;
		tstvalue = 0;
	}
}

static void za7783_testx(void)
{
	int i=0;
	u8 tstvalue;

	while(reg_tbx[i].addr != 0xff)
	{
		za7783_read_reg(reg_tbx[i].addr, &tstvalue);
		
		ZA7783_PRINT("zhongxinwei: reg =0x%x; val=0x%x\n", reg_tbx[i].addr, tstvalue);
		i+=1;
		tstvalue = 0;
	}
}
#endif

void za7783_chip_rest(void)
{
	int ret, i, u1;
	u1 = 0;
	u8 tstvalue;

#if 1 //lvds_interface
	za7783_write_reg(0x12, 0x00);
	za7783_write_reg(0x13, 0xe4);
	za7783_write_reg(0x14, 0x02);
	za7783_write_reg(0x15, 0x02);
	za7783_write_reg(0x16, 0x14);
	za7783_write_reg(0x17, 0x01);//00
	za7783_write_reg(0x18, 0x21);
	za7783_write_reg(0x24, 0x0d);//0c:4 lanes; 
	za7783_write_reg(0x25, 0x00);//0x0
	za7783_write_reg(0x26, 0xe4);
	za7783_write_reg(0x27, 0x00);
	za7783_write_reg(0x29, 0x01);
	za7783_write_reg(0x2B, 0x03); // 增加可靠性
	za7783_write_reg(0x42, 0x02);
	msleep(50);
	za7783_write_reg(0x00, 0x01);
	za7783_write_reg(0x10, 0x00);
	za7783_write_reg(0x11, 0x0f);
	za7783_write_reg(0x20, 0x08);//00
	za7783_write_reg(0x21, 0x00);
	za7783_write_reg(0x22, 0x03);
	za7783_write_reg(0x23, 0x01);

	//za7783_write_reg(0x08, 0x00);
	//za7783_write_reg(0x41, 0x06);
#else //rgb config
	za7783_write_reg(0x08, 0x00);//PADO_DPI_OEN,Enable DPI I/F
	za7783_write_reg(0x12, 0x00);//MIPI D-PHY Data Lane Dp/Dn Exchange
	za7783_write_reg(0x13, 0xe4);//MIPI-PHY Data Lane Reorder
	za7783_write_reg(0x14, 0x02);//MIPI D-PHY Clock Lane Timing Parameter: TIME_CLK_SETTLE
	za7783_write_reg(0x15, 0x00);//MIPI D-PHY Data Lane Timing Parameter: TIME_D_TERM_EN
	za7783_write_reg(0x16, 0x11);//MIPI D-PHY Data Lane Timing Parameter: TIME_HS_SETTLE
	za7783_write_reg(0x17, 0x00);//Dither Enable
msleep(50);
	za7783_write_reg(0x18, 0x21);
	za7783_write_reg(0x41, 0x06);//PADO_DPI_ADJ
	za7783_write_reg(0x42, 0x00);
	za7783_write_reg(0x00, 0x01);//Global Software Reset,reset all digital logic except i2c slave and software register, low active
	za7783_write_reg(0x10, 0x00);//MIPI D-PHY Power Down Control
	za7783_write_reg(0x11, 0x0f);//MIPI D-PHY Data Lane Reset Control
#endif
}

static void za7783_irq_work(struct work_struct *work)
{
	struct za7783_info *info = container_of(work, struct za7783_info, sn_event_work);
	u8 u1 = 0;
	int ret;	
	
	index=index+1;
	ret = za7783_read_reg(0xE5, &u1);
	if(ret < 0)
		ZA7783_PRINT("%s za7783_PLL_DISABLE =%d\n", __func__, ret);
	//clear E5	
	ret = za7783_write_reg(0xE5,0xff);
	if(ret < 0)
		ZA7783_PRINT("%s za7783_PLL_DISABLE =%d\n", __func__, ret);

	enable_irq(info->irq);	
}
static irqreturn_t za7783_interrupt(int irq, void *dev_id)
{
	struct za7783_info *info = dev_id;

	disable_irq_nosync(info->irq);
	if(!work_pending(&info->sn_event_work))	
		queue_work(info->sn_workqueue, &info->sn_event_work);
	
	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void za7783_suspend(struct early_suspend *handler)
{
	ZA7783_PRINT("enter za7783_suspend \n");
	int ret;
	u8 u1 = 0;	

	struct za7783_info *info;
        struct za7783_platform_data *pdata;
	info = container_of(handler, struct za7783_info, early_suspend);
	
	return 0;
}

static void za7783_resume(struct early_suspend *handler)
{
	ZA7783_PRINT("enter za7783_resume \n");

	int ret, i;
	u8 tstvalue;

	struct za7783_info *info;
        struct za7783_platform_data *pdata;
	info = container_of(handler, struct za7783_info, early_suspend);

	za7783_chip_rest();
	
	return 0;
}
#endif


#ifdef LVDS_PROC
//cat lvds
static ssize_t lvds_proc_read(char *page, char **start, off_t off,
							int count, int *eof, void *data)
{
	char *buf  = page;
	char *next = buf;
	int  t, i;
	u8 reg = 0;
	unsigned size = count;

	t = scnprintf(next, size, "lvds regs: \n");
	size -= t;
	next += t;
	//1TODO: Maybe need define register max num
	for (i = 0; i < 0xff; i++) {
		za7783_read_reg(i, &reg);
		t = scnprintf(next, size, "[0x%02x]=0x%02x\n", i, reg);
		size -= t;
		next += t;
	}
	*eof = 1;
	return count - size;
}

// echo w addr val > lvds 
static int lvds_proc_write(struct file *file, const char __user *buffer,
						unsigned long count, void *data)
{
	static char kbuf[4096];
	char *buf = kbuf;
	struct lvds_data *lvds_data = (struct lvds_data*)data;
	char cmd;

	if (count >= 4096)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	cmd = buf[0];
	printk("%s:%s\n", __func__, buf);

	if ('w' == cmd) {
		/* Read raw data to register */
		int reg, value;
		sscanf(buf, "%c %x %x", &cmd, &reg, &value);
		printk("Write register: reg[0x%02x], data[0x%02x]\n", reg, value);
		za7783_write_reg((u8)reg, (u8)value);
	} else {
		printk("unknow command!\n");
	}
	return count;
}
static void lvds_proc_init(struct za7783_info *info)
{
	struct proc_dir_entry *lvds_proc_entry = NULL;
	lvds_proc_entry = create_proc_entry("driver/lvds", 0, NULL);
	if (lvds_proc_entry) {
		lvds_proc_entry->data = info;
		lvds_proc_entry->read_proc = lvds_proc_read;
		lvds_proc_entry->write_proc = lvds_proc_write;
	}
}
#endif

static int za7783_probe(struct i2c_client *client, const struct i2c_device_id *id)
{


	struct za7783_info *info;
	struct za7783_platform_data *pdata;
	int ret = 0;
	int err =0;

	ZA7783_PRINT("za7783_probe enter\n");
	pdata = client->dev.platform_data; 
	if (pdata == NULL) {
		dev_err(&client->dev, "No platform data\n");
		return -EINVAL;
	}	

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}
	
	/* private data allocation */
	info = kzalloc(sizeof(struct za7783_info), GFP_KERNEL);

	if (!info) {
		dev_err(&client->dev,
			"Cannot allocate memory for pn544_info.\n");
		ret = -ENOMEM;
		goto err_info_alloc;
	}

	this_client = client;
	info->pdata = pdata;
	info->client = client;
	info->irq = this_client->irq;
	
	i2c_set_clientdata(client, info);
	
	mutex_init(&info->mutex);

	ret = pdata->power(1);
	if (ret != 0 )
		goto err_za7783_power;

#if CONFIG_REFCLK_LVDS
        za7783_clk_enable(client);
#endif

	za7783_chip_rest();
	msleep(100);

	za7783_testx();	
	//za7783_testx();
	spin_lock_init(&info->lock);

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = za7783_suspend;
	info->early_suspend.resume	= za7783_resume;
	register_early_suspend(&info->early_suspend);
#endif

#ifdef LVDS_PROC
	/* Proc fs init */
	lvds_proc_init(info);
#endif
	printk("za7783 finish.\n");
	return 0;

err_za7783_power:
	i2c_set_clientdata(client, NULL);
	kfree(info);
err_info_alloc:
exit_check_functionality_failed:
exit_irq_request_failed:
	return ret;
}

static int /*__devexit*/ za7783_remove(struct i2c_client *client)
{
	struct za7783_info *info = i2c_get_clientdata(client);
	
	i2c_set_clientdata(client, NULL);
	kfree(info);
	
	return 0;
}

static const struct i2c_device_id za7783_id[] = {
	{ ZA7783_NAME, 0 },
	{ },
};

static struct i2c_driver za7783_driver = {
	.driver = {
		.name = ZA7783_NAME,
	},
	.probe		= za7783_probe,
//	.remove 	= __devexit_p(za7783_remove),
	.remove 	= za7783_remove,
	.id_table	= za7783_id,
	
};

static int __init za7783_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&za7783_driver);
	if (ret) {
		printk(KERN_WARNING "za7783: Driver registration failed, \
					module not inserted.\n");
		return ret;
	}

	return ret;
}

static void __exit za7783_exit(void)
{
	i2c_del_driver(&za7783_driver);
}

EXPORT_SYMBOL(za7783_chip_rest);

fs_initcall(za7783_init);
module_exit(za7783_exit);

MODULE_AUTHOR("<liujunwei@leadcore.com>");
MODULE_DESCRIPTION("Lvds za7783 chip driver");
MODULE_LICENSE("GPL");

