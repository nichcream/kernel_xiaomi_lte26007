/* drivers/i2c/busses/i2c-comip.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement
 * under which you licensed source code. If you did not accept the terms of the
 * license agreement, you are not authorized to use source code. For the terms
 * of the license, please see the license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019 LeadCoreTech Corp.
 *
 * PURPOSE: This files contains i2c adapter driver.
 *
 * CHANGE HISTORY:
 *
 * Version	Date		Author		Description
 * 1.0.0	2012-3-12	lengyansong	created
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/mutex.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <plat/hardware.h>
#include <plat/i2c.h>

#define COMIP_I2C_DEBUG
#ifdef COMIP_I2C_DEBUG
#define I2C_PRINT(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define I2C_PRINT(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#endif

/* Timeout values. */
#define I2C_BUSY_TIMEOUT			(msecs_to_jiffies(1000))
#define I2C_RX_FIFO_WAIT_TIMEOUT		20000 /* us. */
#define I2C_TX_FIFO_WAIT_TIMEOUT		20000 /* us. */
#define I2C_SEND_STOP_TIMEOUT			20000 /* us. */

/* Error values. */
#define I2C_INTR_ERR				(I2C_INTR_TX_ABORT | I2C_INTR_TX_OVER | I2C_INTR_RX_OVER)
#define I2C_ERR_CODE(ret)			(int)(((~ret) + 1))
#define I2C_ERR_TX_ABORT			(0x00010000)
#define I2C_ERR_TX_OVER				(0x00020000)
#define I2C_ERR_RX_OVER				(0x00030000)

/* Speed. */
#define COMIP_I2C_STANDARD_SPEED		(100000)
#define COMIP_I2C_FAST_SPEED			(400000)
#define COMIP_I2C_HIGH_SPEED			(3400000)

struct comip_i2c {
	struct mutex muxlock;
	struct completion completion;
	struct i2c_msg *msg;
	unsigned short msg_ptr;
	int msg_num;
	int msg_idx;
	int msg_rd_bytes;

	struct i2c_adapter adap;
	struct clk *clk;
	unsigned long rate;

	void __iomem *reg_base;

	unsigned long iobase;
	unsigned long iosize;

	int irq;
	unsigned int use_pio;
	unsigned int flags;
};

#define COMIP_I2C_WRITE_REG32(value, offset)		\
		 writel(value, i2c->reg_base + offset)

#define COMIP_I2C_READ_REG32(offset)				\
		 readl(i2c->reg_base + offset)

static inline void i2c_comip_sch_scl(struct comip_i2c *i2c,
		int speed, int *psch, int *pscl)
{
	int sum = 0;

	sum = i2c->rate / speed;
	*psch = max((sum / 2 - 8), 6);
	*pscl = max((sum / 2 - 1), 8);
}

static void i2c_comip_init_hw(struct comip_i2c *i2c)
{
	unsigned char con = I2C_CON_NOMEANING
					| I2C_CON_RESTART_EN
					| I2C_CON_7BIT_ADDR
					| I2C_CON_MASTER_MODE;
	int speed = 0;
	int scl = 0;
	int sch = 0;

	if (COMIP_I2C_HIGH_SPEED_MODE & i2c->flags) {
		/* High speed mode. */
		con |= I2C_CON_HIGH_SPEED_MODE;
		speed = COMIP_I2C_HIGH_SPEED;
	} else if (COMIP_I2C_FAST_MODE & i2c->flags) {
		/* Fast mode. */
		con |= I2C_CON_FAST_MODE;
		speed = COMIP_I2C_FAST_SPEED;
	} else {
		/* Standard mode. */
		con |= I2C_CON_STANDARD_MODE;
		speed = COMIP_I2C_STANDARD_SPEED;
	}
	COMIP_I2C_WRITE_REG32(0, I2C_ENABLE);
	COMIP_I2C_WRITE_REG32(con, I2C_CON);


	/* Standard mode. */
	i2c_comip_sch_scl(i2c, COMIP_I2C_STANDARD_SPEED, &sch, &scl);
	COMIP_I2C_WRITE_REG32(sch, I2C_SS_SCL_HCNT);
	COMIP_I2C_WRITE_REG32(scl, I2C_SS_SCL_LCNT);

	/* Fast mode. */
	i2c_comip_sch_scl(i2c, COMIP_I2C_FAST_SPEED, &sch, &scl);
	COMIP_I2C_WRITE_REG32(sch, I2C_FS_SCL_HCNT);
	COMIP_I2C_WRITE_REG32(scl, I2C_FS_SCL_LCNT);

	/* High speed mode. */
	i2c_comip_sch_scl(i2c, COMIP_I2C_HIGH_SPEED, &sch, &scl);
	COMIP_I2C_WRITE_REG32(sch, I2C_HS_SCL_HCNT);
	COMIP_I2C_WRITE_REG32(scl, I2C_HS_SCL_LCNT);

	COMIP_I2C_WRITE_REG32(0, I2C_RX_TL);
	COMIP_I2C_WRITE_REG32(12, I2C_TX_TL);
	COMIP_I2C_WRITE_REG32(0, I2C_INTR_EN);

	I2C_PRINT("i2c-%d: clock: %ld, speed:%d\n",
		i2c->adap.nr, i2c->rate, speed);
}

static void i2c_comip_enable_int(struct comip_i2c *i2c, unsigned int flags)
{
	unsigned int val;

	val = COMIP_I2C_READ_REG32(I2C_INTR_EN);
	val |= flags;
	COMIP_I2C_WRITE_REG32(val, I2C_INTR_EN);
}

static void i2c_comip_disable_int(struct comip_i2c *i2c, unsigned int flags)
{
	unsigned int val;

	val = COMIP_I2C_READ_REG32(I2C_INTR_EN);
	val &= ~flags;
	COMIP_I2C_WRITE_REG32(val, I2C_INTR_EN);
}

static inline void i2c_comip_start_message(struct comip_i2c *i2c)
{
	COMIP_I2C_READ_REG32(I2C_TX_ABRT_SOURCE);
	COMIP_I2C_READ_REG32(I2C_CLR_INTR);
	COMIP_I2C_WRITE_REG32(i2c->msg->addr & 0x7f, I2C_TAR);
	COMIP_I2C_WRITE_REG32(1, I2C_ENABLE);
}

static inline void i2c_comip_stop_message(struct comip_i2c *i2c)
{
	COMIP_I2C_WRITE_REG32(0, I2C_ENABLE);

	if (!i2c->use_pio)
		i2c_comip_disable_int(i2c, 0xffffffff);
}

static inline int i2c_comip_check_status(struct comip_i2c *i2c, unsigned long flag)
{
	return !!(COMIP_I2C_READ_REG32(I2C_STATUS) & flag);
}

static int i2c_comip_wait_rx_fifo_not_empty(struct comip_i2c *i2c, unsigned long timeout)
{
	while (timeout-- && !i2c_comip_check_status(i2c, I2C_STATUS_RX_FIFO_NOT_EMPTY))
		udelay(1);

	if (!i2c_comip_check_status(i2c, I2C_STATUS_RX_FIFO_NOT_EMPTY))
		return -ETIMEDOUT;

	return 0;
}

static int i2c_comip_wait_tx_fifo_not_full(struct comip_i2c *i2c, unsigned long timeout)
{
	while (timeout-- && !(COMIP_I2C_READ_REG32(I2C_RAW_INTR_STAT) & I2C_INTR_TX_EMPTY))
		udelay(1);

	if (!(COMIP_I2C_READ_REG32(I2C_RAW_INTR_STAT) & I2C_INTR_TX_EMPTY))
		return -ETIMEDOUT;

	return 0;
}

static int i2c_comip_wait_stop_flag(struct comip_i2c *i2c, unsigned long timeout)
{
	while (timeout-- && !(COMIP_I2C_READ_REG32(I2C_RAW_INTR_STAT) & I2C_INTR_STOP_DET))
		udelay(1);

	if (!(COMIP_I2C_READ_REG32(I2C_RAW_INTR_STAT) & I2C_INTR_STOP_DET))
		return -ETIMEDOUT;

	return 0;
}

static int i2c_comip_check_stop_flag(struct comip_i2c *i2c)
{
	if (COMIP_I2C_READ_REG32(I2C_RAW_INTR_STAT) & I2C_INTR_STOP_DET)
		I2C_PRINT("i2c-%d-0x%02x: invalid stop flag.\n", i2c->adap.nr, i2c->msg->addr);

	return 0;
}

static void i2c_comip_master_complete(struct comip_i2c *i2c, int ret)
{
	i2c->msg_ptr = 0;
	i2c->msg = NULL;
	i2c->msg_num = 0;
	i2c->msg_rd_bytes = 0;
	if (ret < 0)
		i2c->msg_idx = ret;
}

static int i2c_comip_xfer_data(struct comip_i2c *i2c)
{
	while (i2c->msg_idx < i2c->msg_num) {
		while (i2c->msg_ptr < i2c->msg->len) {
			if (!(COMIP_I2C_READ_REG32(I2C_RAW_INTR_STAT) & I2C_INTR_TX_EMPTY))
				return 0;

			/* Write data to I2C FIFO. We send stop signal after last byte. */
			if ((i2c->msg_idx == (i2c->msg_num - 1)) && (i2c->msg_ptr == (i2c->msg->len - 1))){
				if (i2c->msg->flags & I2C_M_RD)
					COMIP_I2C_WRITE_REG32(I2C_DATA_CMD_READ | I2C_DATA_CMD_STOP, I2C_DATA_CMD);
				else
					COMIP_I2C_WRITE_REG32(i2c->msg->buf[i2c->msg_ptr] | I2C_DATA_CMD_STOP, I2C_DATA_CMD);
			} else {
				if (i2c->msg->flags & I2C_M_RD)
					COMIP_I2C_WRITE_REG32(0x100, I2C_DATA_CMD);
				else
					COMIP_I2C_WRITE_REG32(i2c->msg->buf[i2c->msg_ptr], I2C_DATA_CMD);
			}

			i2c->msg_ptr++;

			/* Read data from I2C FIFO. */
			if (i2c->msg->flags & I2C_M_RD) {
				while (i2c_comip_check_status(i2c, I2C_STATUS_RX_FIFO_NOT_EMPTY)) {
					if ((i2c->msg_rd_bytes >= i2c->msg->len)) {
						printk(KERN_ERR "i2c-%d: received %d, wanted %d!\n",
							i2c->adap.nr,
							i2c->msg_rd_bytes,
							i2c->msg->len);
						return -EINVAL;
					}

					i2c->msg->buf[i2c->msg_rd_bytes++] = (u8)COMIP_I2C_READ_REG32(I2C_DATA_CMD);
				}
			}
		}

		i2c->msg_ptr = 0;
		i2c->msg_idx++;
		i2c->msg++;
	}

	return i2c->msg_idx;
}

static irqreturn_t i2c_comip_handle_irq(int this_irq, void *dev_id)
{
	struct comip_i2c *i2c = dev_id;
	unsigned int status;
	unsigned int ret = 0;

	status = COMIP_I2C_READ_REG32(I2C_INTR_STAT);

	if (!status)
		return IRQ_NONE;

	if (status & I2C_INTR_TX_ABORT) {
		ret = I2C_ERR_CODE(COMIP_I2C_READ_REG32(I2C_TX_ABRT_SOURCE) | I2C_ERR_TX_ABORT);
		COMIP_I2C_READ_REG32(I2C_CLR_TX_ABRT);
	}

	if (status & I2C_INTR_TX_OVER) {
		ret = I2C_ERR_CODE(I2C_ERR_TX_OVER);
		COMIP_I2C_READ_REG32(I2C_CLR_TX_OVER);
	}

	if (status & I2C_INTR_RX_OVER) {
		ret = I2C_ERR_CODE(I2C_ERR_RX_OVER);
		COMIP_I2C_READ_REG32(I2C_CLR_RX_OVER);
	}

	if (status & I2C_INTR_STOP_DET)
		COMIP_I2C_READ_REG32(I2C_CLR_STOP_DET);

	if (!ret && i2c->msg && (status & I2C_INTR_TX_EMPTY)) {
		ret = i2c_comip_check_stop_flag(i2c);
		if (!ret)
			ret = i2c_comip_xfer_data(i2c);
	}

	if (ret != 0) {
		i2c_comip_disable_int(i2c, 0xffffffff);
		if (ret < 0)
			i2c->msg_idx = ret;
		complete(&i2c->completion);
	}

	return IRQ_HANDLED;
}

static void i2c_comip_handle_data(struct comip_i2c *i2c)
{
	unsigned long timeout;
	int ret = 0;

	do {
		ret = i2c_comip_xfer_data(i2c);
		if (ret < 0) {
			goto out;
		} else if (ret > 0)
			break;

		if (i2c->use_pio) {
			ret = i2c_comip_wait_tx_fifo_not_full(i2c, I2C_TX_FIFO_WAIT_TIMEOUT);
			if (ret) {
				printk(KERN_ERR "i2c-%d: Wait tx fifo timeout!\n",
						i2c->adap.nr);
				goto out;
			}
		} else {
			INIT_COMPLETION(i2c->completion);
			i2c_comip_enable_int(i2c, I2C_INTR_TX_EMPTY | I2C_INTR_ERR);

			timeout = wait_for_completion_timeout(&i2c->completion, HZ);
			if (!timeout && !i2c->completion.done) {
				i2c_comip_disable_int(i2c, 0xffffffff);
				ret = -ETIMEDOUT;
				goto out;
			}

			ret = i2c->msg_idx;
			if (ret < 0)
				goto out;
		}
	} while (ret == 0);

	/* Read the remain data. */
	i2c->msg--;
	if (i2c->msg->flags & I2C_M_RD) {
		while (i2c->msg_rd_bytes < i2c->msg->len) {
			ret = i2c_comip_wait_rx_fifo_not_empty(i2c, I2C_RX_FIFO_WAIT_TIMEOUT);
			if (ret) {
				I2C_PRINT("i2c-%d: wait rx timeout!\n", i2c->adap.nr);
				goto out;
			}

			while (i2c_comip_check_status(i2c, I2C_STATUS_RX_FIFO_NOT_EMPTY)) {
				if (i2c->msg_rd_bytes >= i2c->msg->len) {
					I2C_PRINT("i2c-%d: received %d, wanted %d!\n",
						i2c->adap.nr,
						i2c->msg_rd_bytes,
						i2c->msg->len);
					ret = -EINVAL;
					goto out;
				}

				i2c->msg->buf[i2c->msg_rd_bytes++] = (u8)COMIP_I2C_READ_REG32(I2C_DATA_CMD);
			}
		}
	}

	/* Wait stop flag. We should wait until transfer finished.  */
	ret = i2c_comip_wait_stop_flag(i2c, I2C_SEND_STOP_TIMEOUT);
	if (ret)
		I2C_PRINT("i2c-%d: wait stop flag failed(%d)\n", i2c->adap.nr, ret);

out:
	if (!i2c->use_pio) {
		COMIP_I2C_READ_REG32(I2C_CLR_INTR);
	}
	i2c_comip_master_complete(i2c, ret);
}

static int i2c_comip_do_xfer(struct comip_i2c *i2c, struct i2c_msg *msg,
					int num)
{
	int ret = 0;

	if (!msg || !num) {
		I2C_PRINT("i2c-%d: no msg need read or write.\n", i2c->adap.nr);
		return 0;
	}

	if (!msg->len || !msg->buf) {
		I2C_PRINT("i2c-%d-0x%02x: invalid msg.\n", i2c->adap.nr, msg->addr);
		return 0;
	}

	mutex_lock(&i2c->muxlock);

	i2c->msg = msg;
	i2c->msg_num = num;
	i2c->msg_ptr = 0;
	i2c->msg_idx = 0;
	i2c->msg_rd_bytes = 0;

	i2c_comip_start_message(i2c);

	i2c_comip_handle_data(i2c);

	i2c_comip_stop_message(i2c);

	mutex_unlock(&i2c->muxlock);

	/* Return code in i2c->msg_idx. */
	ret = i2c->msg_idx;
	if (ret < 0)
		I2C_PRINT("i2c-%d-0x%02x: xfer error %d.\n", i2c->adap.nr, msg->addr, ret);

	return ret;
}

static int i2c_comip_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
					int num)
{
	struct comip_i2c *i2c = adap->algo_data;

	return i2c_comip_do_xfer(i2c, msgs, num);
}

static u32 i2c_comip_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm i2c_comip_algorithm = {
	.master_xfer = i2c_comip_xfer,
	.functionality = i2c_comip_functionality,
};

static int i2c_comip_probe(struct platform_device *dev)
{
	struct comip_i2c *i2c;
	struct resource *res;
	struct comip_i2c_platform_data *plat = dev->dev.platform_data;
	int ret;
	int irq;

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(dev, 0);
	if (res == NULL || irq < 0)
		return -ENODEV;

	if (!request_mem_region(res->start, res->end - res->start + 1, res->name))
		return -ENOMEM;

	i2c = kzalloc(sizeof(struct comip_i2c), GFP_KERNEL);
	if (!i2c) {
		ret = -ENOMEM;
		goto emalloc;
	}

	i2c->adap.owner = THIS_MODULE;
	i2c->adap.retries = 5;

	mutex_init(&i2c->muxlock);
	init_completion(&i2c->completion);

	/*
	 * If "dev->id" is negative we consider it as zero.
	 * The reason to do so is to avoid sysfs names that only make
	 * sense when there are multiple adapters.
	 */
	i2c->adap.nr = dev->id != -1 ? dev->id : 0;
	snprintf(i2c->adap.name, sizeof(i2c->adap.name), "comip_i2c-i2c.%u",
		 i2c->adap.nr);

	i2c->clk = clk_get(&dev->dev, "i2c_clk");
	if (IS_ERR(i2c->clk)) {
		ret = PTR_ERR(i2c->clk);
		goto eclk;
	}

	i2c->rate = clk_get_rate(i2c->clk);
	i2c->irq = irq;
	i2c->iobase = res->start;
	i2c->iosize = res->end - res->start + 1;
	i2c->reg_base = ioremap(res->start, i2c->iosize);
	if (!i2c->reg_base) {
		ret = -EIO;
		goto eremap;
	}

	clk_enable(i2c->clk);

	if (plat) {
		i2c->adap.class = plat->class;
		i2c->use_pio = plat->use_pio;
		i2c->flags = plat->flags;
	}
	i2c->adap.algo = &i2c_comip_algorithm;

	i2c_comip_init_hw(i2c);

	if (!i2c->use_pio) {
		ret = request_irq(irq, i2c_comip_handle_irq, IRQF_SHARED,
				  i2c->adap.name, i2c);
		if (ret)
			goto eadapt;
	}

	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &dev->dev;

	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0) {
		I2C_PRINT("i2c-%d: Failed to add bus\n", i2c->adap.nr);
		goto eadapt;
	}

	platform_set_drvdata(dev, i2c);

	return 0;

eadapt:
	if (!i2c->use_pio)
		free_irq(irq, i2c);
	clk_disable(i2c->clk);
	iounmap(i2c->reg_base);
eremap:
	clk_put(i2c->clk);
eclk:
	kfree(i2c);
emalloc:
	release_mem_region(res->start, res->end - res->start + 1);
	mutex_destroy(&i2c->muxlock);
	return ret;
}

static int __exit i2c_comip_remove(struct platform_device *dev)
{
	struct comip_i2c *i2c = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	i2c_del_adapter(&i2c->adap);
	if (!i2c->use_pio)
		free_irq(i2c->irq, i2c);

	clk_disable(i2c->clk);
	clk_put(i2c->clk);

	iounmap(i2c->reg_base);
	release_mem_region(i2c->iobase, i2c->iosize);
	mutex_destroy(&i2c->muxlock);
	kfree(i2c);

	return 0;
}

#ifdef CONFIG_PM
static int i2c_comip_suspend_noirq(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static int i2c_comip_resume_noirq(struct device *dev)
{
	/* Do nothing. */
	return 0;
}

static struct dev_pm_ops i2c_comip_pm_ops = {
	.suspend_noirq = i2c_comip_suspend_noirq,
	.resume_noirq = i2c_comip_resume_noirq,
};
#endif

static struct platform_driver i2c_comip_driver = {
	.probe = i2c_comip_probe,
	.remove = __exit_p(i2c_comip_remove),
	.driver = {
		.name = "comip-i2c",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &i2c_comip_pm_ops,
#endif
	},
};

static int __init i2c_comip_init(void)
{
	return platform_driver_register(&i2c_comip_driver);
}

static void __exit i2c_comip_exit(void)
{
	platform_driver_unregister(&i2c_comip_driver);
}

subsys_initcall(i2c_comip_init);
module_exit(i2c_comip_exit);

MODULE_AUTHOR("lengyansong <lengyansong@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP I2C bus adapter");
MODULE_LICENSE("GPL");
