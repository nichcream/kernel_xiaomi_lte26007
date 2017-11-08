/* arch/arm/mach-comip/comip-ureg.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip user register driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date		Author		Description
 *	1.0.0	2012-03-06	zouqiao		created
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#include <asm/uaccess.h>
#include <plat/hardware.h>
#include <plat/comip-pmic.h>
#include <plat/mfp.h>

#include "comip-ureg.h"

#define COMIP_UREG_DEBUG
#ifdef COMIP_UREG_DEBUG
#define UREG_PRINT(fmt, args...) printk(KERN_ERR "[UREG]" fmt, ##args)
#else
#define UREG_PRINT(fmt, args...) printk(KERN_DEBUG "[UREG]" fmt, ##args)
#endif

struct ureg_range {
	unsigned int start;
	unsigned int end;
};

struct ureg_range ureg_range_list[] = {
	{0x00000000, 0xffffffff},
};

extern int read_reg_thr_isp(u16 reg, u8 *val);
extern int write_reg_thr_isp(u16 reg, u8 val, u8 mask);

static int ureg_check(unsigned int paddr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ureg_range_list); i++) {
		if ((paddr >= ureg_range_list[i].start)
			&& (paddr <= ureg_range_list[i].end))
			return 0;
	}

	return -ENXIO;
}

static int do_normal_reg_read(struct ureg_unit *n_reg)
{
	int ret = -ENXIO;

	if ((n_reg->paddr < ISP_BASE) || (n_reg->paddr > (ISP_BASE + 0x80000 - 1))) {
		if (n_reg->paddr & 0x03) {
			n_reg->paddr &= ~0x3;
			UREG_PRINT("Address aligned to 0x%08x\n", n_reg->paddr);
		}
	}
	ret = ureg_check(n_reg->paddr);
	if (ret < 0)
		return ret;

	if ((n_reg->paddr < ISP_BASE) || (n_reg->paddr > (ISP_BASE + 0x80000 - 1)))
		n_reg->val = readl(io_p2v(n_reg->paddr));
	else {
		n_reg->paddr |= ISP_BASE;
		n_reg->val = 0;
		n_reg->val = readb(io_p2v(n_reg->paddr));
		printk("ISP REG READ,  [0x%08X, 0x%02X]\n", n_reg->paddr, n_reg->val);
	}
	return 0;
}

static int do_normal_reg_write(struct ureg_unit *n_reg)
{
	int ret = -ENXIO;

	if ((n_reg->paddr < ISP_BASE) || (n_reg->paddr > (ISP_BASE + 0x80000 - 1))) {
		if (n_reg->paddr & 0x03) {
			n_reg->paddr &= ~0x3;
			UREG_PRINT("Address aligned to 0x%08x\n", n_reg->paddr);
		}
	}

	ret = ureg_check(n_reg->paddr);
	if (ret < 0)
		return ret;

	if ((n_reg->paddr < ISP_BASE) || (n_reg->paddr > (ISP_BASE + 0x80000 - 1)))
		writel(n_reg->val, io_p2v(n_reg->paddr));
	else {
		n_reg->paddr |= ISP_BASE;
		writeb(n_reg->val, io_p2v(n_reg->paddr));
		printk("ISP REG WRITE, [0x%08X, 0x%02X]\n", n_reg->paddr, n_reg->val);
	}

	return 0;
}

static inline int do_pmic_reg_read(struct ureg_unit *n_reg)
{
	int ret;
	u16 val;

	ret = pmic_reg_read((u16)n_reg->paddr, &val);
	if (ret)
		return ret;

	n_reg->val = (unsigned int)val;
	return 0;
}

static inline int do_pmic_reg_write(struct ureg_unit *n_reg)
{
	return pmic_reg_write((u16)(n_reg->paddr), (u16)(n_reg->val));
}

static inline int do_i2c_read(struct ureg_i2c *n_i2c)
{
	struct i2c_adapter *adapter;
	struct i2c_msg msg[2];
	unsigned char addr_buf[4];
	unsigned short addr_buf_len = 0;
	unsigned char val_buf[4];
	unsigned short val_buf_len = 0;
	int ret;
	int i;
	int byte_cnt;

	adapter = i2c_get_adapter(n_i2c->adapter_nr);

	if(n_i2c->flag & UREG_I2C_ADDR_8BIT)
		byte_cnt = 1;
	else if(n_i2c->flag & UREG_I2C_ADDR_16BIT)
		byte_cnt = 2;
	else if(n_i2c->flag & UREG_I2C_ADDR_24BIT)
		byte_cnt = 3;
	else if(n_i2c->flag & UREG_I2C_ADDR_32BIT)
		byte_cnt = 4;
	else
		byte_cnt = 1;

	for (i = 0; i < byte_cnt; i++) {
		addr_buf[byte_cnt - i - 1] = (n_i2c->addr >> (8 * i)) & 0xff;
		addr_buf_len++;
	}

	if(n_i2c->flag & UREG_I2C_VAL_8BIT)
		byte_cnt = 1;
	else if(n_i2c->flag & UREG_I2C_VAL_16BIT)
		byte_cnt = 2;
	else if(n_i2c->flag & UREG_I2C_VAL_24BIT)
		byte_cnt = 3;
	else if(n_i2c->flag & UREG_I2C_VAL_32BIT)
		byte_cnt = 4;
	else
		byte_cnt = 1;

	for (i = 0; i < byte_cnt; i++) {
		val_buf[byte_cnt - i - 1] = (n_i2c->val >> (8 * i)) & 0xff;
		val_buf_len++;
	}

	msg[0].addr = n_i2c->device_addr;
	msg[0].flags = 0;
	msg[0].len = addr_buf_len;
	msg[0].buf = addr_buf;

	msg[1].addr = n_i2c->device_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = val_buf_len;
	msg[1].buf = val_buf;

	ret = i2c_transfer(adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	n_i2c->val = 0;

	for (i = 0; i < val_buf_len; i++)
	  n_i2c->val |= (((int)val_buf[val_buf_len - i - 1]) << (8 * i));

	return ret;
}


static inline int do_i2c_write(struct ureg_i2c *n_i2c)
{
	struct i2c_adapter *adapter;
	struct i2c_msg msg;
	unsigned char buf[8];
	unsigned short buf_len = 0;
	unsigned short addr_len = 0;
	int ret;
	int i;
	int byte_cnt = 0;

	adapter = i2c_get_adapter(n_i2c->adapter_nr);

	if(n_i2c->flag & UREG_I2C_ADDR_8BIT)
		byte_cnt = 1;
	else if(n_i2c->flag & UREG_I2C_ADDR_16BIT)
		byte_cnt = 2;
	else if(n_i2c->flag & UREG_I2C_ADDR_24BIT)
		byte_cnt = 3;
	else if(n_i2c->flag & UREG_I2C_ADDR_32BIT)
		byte_cnt = 4;
	else
		byte_cnt = 1;

	for (i = 0; i < byte_cnt; i++) {
		buf[byte_cnt - i - 1] = (n_i2c->addr >> (8 * i)) & 0xff;
		buf_len++;
	}

	addr_len = buf_len;

	if(n_i2c->flag & UREG_I2C_VAL_8BIT)
		byte_cnt = 1;
	else if(n_i2c->flag & UREG_I2C_VAL_16BIT)
		byte_cnt = 2;
	else if(n_i2c->flag & UREG_I2C_VAL_24BIT)
		byte_cnt = 3;
	else if(n_i2c->flag & UREG_I2C_VAL_32BIT)
		byte_cnt = 4;
	else
		byte_cnt = 1;

	for (i = 0; i < byte_cnt; i++) {
		buf[byte_cnt - i - 1 + addr_len] = (n_i2c->val >> (8 * i)) & 0xff;
		buf_len++;
	}

	msg.addr = n_i2c->device_addr;
	msg.flags = 0;
	msg.len = buf_len;
	msg.buf = buf;

	ret = i2c_transfer(adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int do_gpio_reg_read(struct ureg_gpio *n_gpio)
{
	if (n_gpio->gpio_num < 0 ||n_gpio->gpio_num > 255)
		return -EINVAL;

	n_gpio->muxpin_status = comip_mfp_af_read(n_gpio->gpio_num);
	n_gpio->pull_down = comip_mfp_pull_down_read(n_gpio->gpio_num);
	n_gpio->pull_up = comip_mfp_pull_up_read(n_gpio->gpio_num);

	return 0;
}

static int do_gpio_reg_write(struct ureg_gpio *n_gpio)
{

	if (n_gpio->gpio_num < 0 || n_gpio->gpio_num > 255)
		return -EINVAL;
	if (n_gpio->muxpin_status != 7)
		comip_mfp_config(n_gpio->gpio_num, n_gpio->muxpin_status);
	if (n_gpio->flag != 7)
		comip_mfp_config_pull(n_gpio->gpio_num, n_gpio->flag);

	return 0;
}

static ssize_t ureg_read(struct file *file, char *buf,
				 size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t ureg_write(struct file *file, const char *buf,
				  size_t count, loff_t * ppos)
{
	return 0;
}

static int ureg_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ureg_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long ureg_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int ret;
	void __user *argp = (void __user *)arg;
	struct ureg_unit n_reg;
	struct ureg_i2c n_i2c;
	struct ureg_gpio n_gpio;

	switch (cmd) {
	case UREG_READ_REG:
		if (copy_from_user(&n_reg, argp, sizeof(n_reg)))
			return -EFAULT;

		ret = do_normal_reg_read(&n_reg);
		if (ret)
			return ret;

		ret = copy_to_user(argp, &n_reg, sizeof(n_reg));
		break;

	case UREG_WRITE_REG:
		if (copy_from_user(&n_reg, argp, sizeof(n_reg)))
			return -EFAULT;

		ret = do_normal_reg_write(&n_reg);
		break;

	case UREG_READ_PMIC:
		if (copy_from_user(&n_reg, argp, sizeof(n_reg)))
			return -EFAULT;

		ret = do_pmic_reg_read(&n_reg);
		if (ret)
			return ret;

		ret = copy_to_user(argp, &n_reg, sizeof(n_reg));
		break;

	case UREG_WRITE_PMIC:
		if (copy_from_user(&n_reg, argp, sizeof(n_reg)))
			return -EFAULT;

		ret = do_pmic_reg_write(&n_reg);
		break;

	case UREG_READ_CODEC:
	case UREG_WRITE_CODEC:

	case UREG_READ_I2C:
		if (copy_from_user(&n_i2c, argp, sizeof(n_i2c)))
			return -EFAULT;

		ret = do_i2c_read(&n_i2c);
		if (ret)
			return ret;

		ret = copy_to_user(argp, &n_i2c, sizeof(n_i2c));
		break;

	case UREG_WRITE_I2C:
		if (copy_from_user(&n_i2c, argp, sizeof(n_i2c)))
			return -EFAULT;

		ret = do_i2c_write(&n_i2c);
		if (ret)
			return ret;
		break;

	case UREG_READ_GPIO:
		if (copy_from_user(&n_gpio, argp, sizeof(n_gpio)))
			return -EFAULT;

		ret = do_gpio_reg_read(&n_gpio);
		if (ret)
			return ret;

		ret = copy_to_user(argp, &n_gpio, sizeof(n_gpio));
		break;
	case UREG_WRITE_GPIO:
		if (copy_from_user(&n_gpio, argp, sizeof(n_gpio)))
			return -EFAULT;

		ret = do_gpio_reg_write(&n_gpio);
		if (ret)
			return ret;

		ret = copy_to_user(argp, &n_gpio, sizeof(n_gpio));
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations ureg_fops = {
	.owner	= THIS_MODULE,
	.open	= ureg_open,
	.release	= ureg_release,
	.read	= ureg_read,
	.write	= ureg_write,
	.unlocked_ioctl	= ureg_ioctl,
	.llseek	= no_llseek,
};

static struct miscdevice ureg_misc_dev = {
	MISC_DYNAMIC_MINOR,
	UREG_DEV_NAME,
	&ureg_fops
};

static int ureg_probe(struct platform_device *pdev)
{
	int ret;

	ret = misc_register(&ureg_misc_dev);
	if (ret) {
		UREG_PRINT("Unable to register misc device.\n");
		return ret;
	}

	UREG_PRINT("Ureg Driver Registered\n");
	return 0;
}

static int ureg_remove(struct platform_device *pdev)
{
	misc_deregister(&ureg_misc_dev);
	return 0;
}

static struct platform_driver ureg_driver = {
	.probe = ureg_probe,
	.remove = ureg_remove,
	.driver = {
		.name = UREG_DEV_NAME,
	},
};

static int __init ureg_init(void)
{
	return platform_driver_register(&ureg_driver);
}

static void __exit ureg_exit(void)
{
	platform_driver_unregister(&ureg_driver);
}

module_init(ureg_init);
module_exit(ureg_exit);

MODULE_LICENSE("GPL");
