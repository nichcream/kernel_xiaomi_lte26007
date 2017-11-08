/* drivers/spi/spi_comip.c
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** Copyright (c) 2010-2019 LeadCoreTech Corp.
**
** PURPOSE: This files contains spi master driver.
**
** CHANGE HISTORY:
**
** Version	Date		Author		Description
** 1.0.0	2012-03-06	liuyong		created
**
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>
 
#include <plat/hardware.h>
#include <plat/dmas.h>
#include <plat/spi.h>

#define COMIP_SPI_DEBUG
#ifdef COMIP_SPI_DEBUG
#define SSI_DUMP_REGS(a, b, c)		comip_spi_dump_regs(a, b, c)
#define SSI_PRINT(fmt, args...)		printk(KERN_ERR fmt, ##args)
#else
#define SSI_DUMP_REGS(a, b, c)
#define SSI_PRINT(fmt, args...)		printk(KERN_DEBUG fmt, ##args)
#endif

/* Version. */
#define SSI_VERSION			"1.0.0"

/* Frame length. */
#define SSI_DFS_MIN			(4)
#define SSI_DFS_MAX			(16)

/* DMA transfer. */
#define SSI_DMA_MIN_BYTES		(512)

/* TX dummy data . */
#define SSI_TX_DUMMY_DATA		(0)

/* Tranfser timeout. */
#define SSI_TIMEOUT			msecs_to_jiffies(1000)

struct comip_ssi {
	/* Base. */
	struct comip_spi_platform_data *pdata;
	struct platform_device *pdev;
	struct spi_master *master;
	struct resource *res;
	void __iomem *base;
	struct clk *clk;
	unsigned int clk_input;
	int id;
	int irq;

	/* DMA. */
	int dma_supported;
	int dma_rx_channel;
	int dma_tx_channel;
	struct dmas_ch_cfg dma_rx_cfg;
	struct dmas_ch_cfg dma_tx_cfg;
	struct completion dma_rx_completion;
	struct completion dma_tx_completion;

	/* Driver message queue */
	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct list_head queue;
	spinlock_t lock;

	/* SPI config. */
	unsigned int speed_hz;
	unsigned char bits_per_word;
	unsigned char chip_select;
	unsigned char mode;
	unsigned int tx_dummy_data;
	unsigned int rtx_flag;
	unsigned int rx_len;
	int cs_level;
};

static inline void comip_spi_write_reg(struct comip_ssi *ssi,
		int idx, unsigned int val)
{
	writel(val, ssi->base + idx);
}

static inline unsigned int comip_spi_read_reg(struct comip_ssi *ssi, int idx)
{
	return readl(ssi->base + idx);
}

#ifdef COMIP_SPI_DEBUG
static void comip_spi_dump_regs(struct comip_ssi *ssi, int is_dma, int is_rx)
{
	SSI_PRINT("SSI%d driver version : %s\n", ssi->id, SSI_VERSION);
	SSI_PRINT("SSI CS level : %d\n", ssi->cs_level);
	SSI_PRINT("SSI_CTRL0 : 0x%08x\n", comip_spi_read_reg(ssi, SSI_CTRL0));
	SSI_PRINT("SSI_CTRL1 : 0x%08x\n", comip_spi_read_reg(ssi, SSI_CTRL1));
	SSI_PRINT("SSI_EN : 0x%08x\n", comip_spi_read_reg(ssi, SSI_EN));
	SSI_PRINT("SSI_SE : 0x%08x\n", comip_spi_read_reg(ssi, SSI_SE));
	SSI_PRINT("SSI_BAUD : 0x%08x\n", comip_spi_read_reg(ssi, SSI_BAUD));
	SSI_PRINT("SSI_TXFTL : 0x%08x\n", comip_spi_read_reg(ssi, SSI_TXFTL));
	SSI_PRINT("SSI_RXFTL : 0x%08x\n", comip_spi_read_reg(ssi, SSI_RXFTL));
	SSI_PRINT("SSI_TXFL : 0x%08x\n", comip_spi_read_reg(ssi, SSI_TXFL));
	SSI_PRINT("SSI_RXFL : 0x%08x\n", comip_spi_read_reg(ssi, SSI_RXFL));
	SSI_PRINT("SSI_STS : 0x%08x\n", comip_spi_read_reg(ssi, SSI_STS));
	SSI_PRINT("SSI_IE : 0x%08x\n", comip_spi_read_reg(ssi, SSI_IE));
	SSI_PRINT("SSI_IS : 0x%08x\n", comip_spi_read_reg(ssi, SSI_IS));
	SSI_PRINT("SSI_RIS : 0x%08x\n", comip_spi_read_reg(ssi, SSI_RIS));
	SSI_PRINT("SSI_DMAC : 0x%08x\n", comip_spi_read_reg(ssi, SSI_DMAC));
	SSI_PRINT("SSI_DMATDL : 0x%08x\n", comip_spi_read_reg(ssi, SSI_DMATDL));
	SSI_PRINT("SSI_DMARDL : 0x%08x\n", comip_spi_read_reg(ssi, SSI_DMARDL));
	if (is_dma) {
		if (is_rx) {
			SSI_PRINT("rx dma : %d\n", ssi->dma_rx_channel);
			comip_dmas_dump(ssi->dma_rx_channel);
		} else {
			SSI_PRINT("tx dma : %d\n", ssi->dma_tx_channel);
			comip_dmas_dump(ssi->dma_tx_channel);
		}
	}
}
#endif

static int comip_spi_for_reg_bit(struct comip_ssi *ssi, int idx,
			unsigned long bit)
{
	unsigned long timeout;

	timeout = jiffies + SSI_TIMEOUT;
	while (!(comip_spi_read_reg(ssi, idx) & bit)) {
		if (time_after(jiffies, timeout))
			return -1;
		cpu_relax();
	}

	return 0;
}

static void comip_spi_hw_init(struct comip_ssi *ssi)
{
	/* Disable all interrupts. */
	comip_spi_write_reg(ssi, SSI_IE, 0);

	/* Disable SSI. */
	comip_spi_write_reg(ssi, SSI_EN, 0);

	/* Config SSI. */
	comip_spi_write_reg(ssi, SSI_CTRL0, SSI_RTX | SSI_MOTOROLA_SPI);
	comip_spi_write_reg(ssi, SSI_CTRL1, 8);

	/* Set threshold of TX and RX. */
	comip_spi_write_reg(ssi, SSI_TXFTL, 0);
	comip_spi_write_reg(ssi, SSI_RXFTL, 0);

	if (ssi->dma_supported) {
		/* Disable DMA mode. */
		comip_spi_write_reg(ssi, SSI_DMAC, 0);

		/* Set DMA threshold of TX and RX. */
		comip_spi_write_reg(ssi, SSI_DMATDL, 4);
		comip_spi_write_reg(ssi, SSI_DMARDL, 0);
	}
}

static void comip_spi_hw_uninit(struct comip_ssi *ssi)
{
	/* Disable all interrupts. */
	comip_spi_write_reg(ssi, SSI_IE, 0);

	/* Disable SSI. */
	comip_spi_write_reg(ssi, SSI_EN, 0);
}

static void comip_spi_enable(struct comip_ssi *ssi, int enable)
{
	comip_spi_write_reg(ssi, SSI_EN, enable ? 1 : 0);
}

static void comip_spi_dma_enable(struct comip_ssi *ssi,
		int is_rx, int enable)
{
	unsigned int bit = is_rx ? 0 : 1;
	unsigned int val;

	val = comip_spi_read_reg(ssi, SSI_DMAC);
	if (enable)
		val |= (1 << bit);
	else
		val &= ~(1 << bit);
	comip_spi_write_reg(ssi, SSI_DMAC, val);
}

static void comip_spi_cs_set(struct comip_ssi *ssi, int cs_active)
{
	int level = !((ssi->mode & SPI_CS_HIGH) ^ cs_active);

	ssi->cs_level = level;
	if (ssi->pdata->cs_state)
		ssi->pdata->cs_state(ssi->chip_select, level);
}

static inline void comip_spi_dma_tx_irq(int irq, int type, void *dev_id)
{
	struct comip_ssi *ssi = dev_id;

	comip_spi_dma_enable(ssi, 0, 0);
	complete(&ssi->dma_tx_completion);
}

static inline void comip_spi_dma_rx_irq(int irq, int type, void *dev_id)
{
	struct comip_ssi *ssi = dev_id;

	comip_spi_dma_enable(ssi, 1, 0);
	complete(&ssi->dma_rx_completion);
}

static int comip_spi_dma_init(struct comip_ssi *ssi)
{
	struct dmas_ch_cfg *cfg;
	int ret;

	cfg = &ssi->dma_tx_cfg;
	cfg->flags = DMAS_CFG_ALL;
	cfg->block_size = 0;
	cfg->src_addr = 0;
	cfg->dst_addr = (unsigned int)(ssi->res->start + SSI_DATA);
	cfg->priority = DMAS_CH_PRI_DEFAULT;
	cfg->bus_width = DMAS_DEV_WIDTH_8BIT;
	cfg->tx_trans_mode = DMAS_TRANS_NORMAL;
	cfg->tx_fix_value = 0;
	cfg->tx_block_mode = DMAS_SINGLE_BLOCK;
	cfg->irq_en = DMAS_INT_DONE;
	cfg->irq_handler = comip_spi_dma_tx_irq;
	cfg->irq_data = ssi;
	ret = comip_dmas_request((char *)ssi->res->name, ssi->dma_tx_channel);
	if (ret)
		return ret;

	ret = comip_dmas_config(ssi->dma_tx_channel, cfg);
	if (ret)
		return ret;

	cfg = &ssi->dma_rx_cfg;
	cfg->flags = DMAS_CFG_ALL;
	cfg->block_size = 0;
	cfg->src_addr = (unsigned int)(ssi->res->start + SSI_DATA);
	cfg->dst_addr = 0;
	cfg->priority = DMAS_CH_PRI_DEFAULT;
	cfg->bus_width = DMAS_DEV_WIDTH_8BIT;
	cfg->rx_trans_type = DMAS_TRANS_BLOCK;
	cfg->rx_timeout = 0;
	cfg->irq_en = DMAS_INT_DONE;
	cfg->irq_handler = comip_spi_dma_rx_irq;
	cfg->irq_data = ssi;
	ret = comip_dmas_request((char *)ssi->res->name, ssi->dma_rx_channel);
	if (ret)
		return ret;

	ret = comip_dmas_config(ssi->dma_rx_channel, cfg);
	if (ret)
		return ret;

	init_completion(&ssi->dma_rx_completion);
	init_completion(&ssi->dma_tx_completion);

	return ret;
}

static void comip_spi_dma_uninit(struct comip_ssi *ssi)
{
	comip_dmas_free(ssi->dma_tx_channel);
	comip_dmas_free(ssi->dma_rx_channel);
}

static void comip_spi_dma_start(struct comip_ssi *ssi,
		int is_rx, dma_addr_t buf, unsigned int count)
{
	struct dmas_ch_cfg *cfg;

	if (is_rx) {
		cfg = &ssi->dma_rx_cfg;
		cfg->flags = DMAS_CFG_BLOCK_SIZE | DMAS_CFG_DST_ADDR;
		cfg->dst_addr = buf;
		cfg->block_size = count;
		comip_dmas_config(ssi->dma_rx_channel, cfg);
		comip_dmas_start(ssi->dma_rx_channel);
	} else {
		cfg = &ssi->dma_tx_cfg;
		cfg->flags = DMAS_CFG_BLOCK_SIZE | DMAS_CFG_SRC_ADDR;
		cfg->src_addr = buf;
		cfg->block_size = count;
		comip_dmas_config(ssi->dma_tx_channel, cfg);
		comip_dmas_start(ssi->dma_tx_channel);
	}
}

static int comip_spi_config(struct spi_device *spi,
		struct spi_message *msg, struct spi_transfer *xfer)
{
	struct comip_ssi *ssi = spi_master_get_devdata(spi->master);
	unsigned int rtx_flag = SSI_RTX;
	unsigned int enable = 0;
	unsigned int val = 0;
	unsigned int div = 0;

	if (xfer) {
		if ((msg->is_dma_mapped || (xfer->len >= SSI_DMA_MIN_BYTES))
			&& ssi->dma_supported) {
			if (!xfer->tx_buf)
				rtx_flag = SSI_RX;
			else if (!xfer->rx_buf)
				rtx_flag = SSI_TX;
		}

		if ((!xfer->bits_per_word || (xfer->bits_per_word == ssi->bits_per_word))
			&& (!xfer->speed_hz || (xfer->speed_hz == ssi->speed_hz))
			&& (rtx_flag == ssi->rtx_flag)
			&& !((rtx_flag == SSI_RX) && (xfer->len != ssi->rx_len)))
			return 0;

		ssi->rtx_flag = rtx_flag;
		if (xfer->bits_per_word)
			ssi->bits_per_word = xfer->bits_per_word;
		if (xfer->speed_hz)
			ssi->speed_hz = xfer->speed_hz;
	} else {
		if (spi->chip_select >= ssi->pdata->num_chipselect) {
			dev_err(&ssi->pdev->dev, "invalid chip select %d\n", spi->chip_select);
			return -EINVAL;
		}

		if ((spi->bits_per_word == ssi->bits_per_word)
			&& (spi->max_speed_hz == ssi->speed_hz)
			&& (spi->chip_select == ssi->chip_select)
			&& (spi->mode == ssi->mode))
			return 0;

		ssi->mode = spi->mode;
		ssi->chip_select = spi->chip_select;
		ssi->bits_per_word = spi->bits_per_word;
		ssi->speed_hz = spi->max_speed_hz;
	}

	if ((ssi->bits_per_word < SSI_DFS_MIN)
		|| (ssi->bits_per_word > SSI_DFS_MAX)) {
		dev_err(&ssi->pdev->dev, "invalid word length %d\n", ssi->bits_per_word);
		return -EINVAL;
	}

	if (ssi->speed_hz) {
		div = ssi->clk_input / ssi->speed_hz;
		div = ((div + 1) / 2) * 2;
	}

	/* Save the SSI enable flag and disable SSI. */
	enable = comip_spi_read_reg(ssi, SSI_EN);
	comip_spi_write_reg(ssi, SSI_EN, 0);

	/* Set the buadrate. */
	comip_spi_write_reg(ssi, SSI_BAUD, div);

	/* CS. */
	comip_spi_write_reg(ssi, SSI_SE, 1 << ssi->chip_select);

	val = comip_spi_read_reg(ssi, SSI_CTRL0);

	/* Word length. */
	val &= ~(SSI_DFS_MASK << SSI_DFS_BIT);
	val |= (ssi->bits_per_word - 1) << SSI_DFS_BIT;

	/* RTX. */
	if (xfer) {
		if (rtx_flag == SSI_RX) {
			ssi->rx_len = xfer->len;
			comip_spi_write_reg(ssi, SSI_CTRL1, ssi->rx_len - 1);
		} else {
			ssi->rx_len = 0;
			comip_spi_write_reg(ssi, SSI_CTRL1, 0);
		}

		val &= ~(SSI_RTX_MASK << SSI_RTX_BIT);
		val |= rtx_flag;
	}

	/* Set SPI mode. */
	if (spi->mode & SPI_CPOL)
		val |= SSI_CPOL;
	else
		val &= ~SSI_CPOL;

	if (spi->mode & SPI_CPHA)
		val |= SSI_CPHA;
	else
		val &= ~SSI_CPHA;

	if (spi->mode & SPI_LOOP)
		val |= SSI_SRL;
	else
		val &= ~SSI_SRL;

	comip_spi_write_reg(ssi, SSI_CTRL0, val);

	/* Restore the SSI enable flag. */
	comip_spi_write_reg(ssi, SSI_EN, enable);

	return 0;
}

static int comip_spi_transfer_dma(struct comip_ssi *ssi,
		struct spi_message *msg, struct spi_transfer *xfer)
{
	unsigned long timeout;
	unsigned int count;
	int ret;
	u8 *rx;
	const u8 *tx;

	count = xfer->len;
	rx = xfer->rx_buf;
	tx = xfer->tx_buf;
	ret = count;

	if (rx != NULL) {
		INIT_COMPLETION(ssi->dma_rx_completion);
		comip_spi_dma_enable(ssi, 1, 1);
		comip_spi_dma_start(ssi, 1, xfer->rx_dma, count);
	}

	if (tx != NULL) {
		INIT_COMPLETION(ssi->dma_tx_completion);
		comip_spi_dma_enable(ssi, 0, 1);
		comip_spi_dma_start(ssi, 0, xfer->tx_dma, count);
	} else {
		/* RX_ONLY mode needs dummy data in TX reg. */
		comip_spi_write_reg(ssi, SSI_DATA, ssi->tx_dummy_data);
	}

	if (tx != NULL) {
		timeout = wait_for_completion_timeout(&ssi->dma_tx_completion, SSI_TIMEOUT);
		if (!timeout && !ssi->dma_tx_completion.done) {
			dev_err(&ssi->pdev->dev, "DMA TXS timed out\n");
			SSI_DUMP_REGS(ssi, 1, 0);
			comip_spi_dma_enable(ssi, 0, 0);
			comip_dmas_stop(ssi->dma_tx_channel);
			ret = -EIO;
		}

		if (!msg->is_dma_mapped)
			dma_unmap_single(NULL, xfer->tx_dma, count, DMA_TO_DEVICE);
	}

	if (rx != NULL) {
		if (ret == count) {
			timeout = wait_for_completion_timeout(&ssi->dma_rx_completion, SSI_TIMEOUT);
			if (!timeout && !ssi->dma_rx_completion.done) {
				dev_err(&ssi->pdev->dev, "DMA RXS timed out\n");
				SSI_DUMP_REGS(ssi, 1, 1);
				comip_spi_dma_enable(ssi, 1, 0);
				comip_dmas_stop(ssi->dma_rx_channel);
				ret = -EIO;
			}
		} else {
			comip_spi_dma_enable(ssi, 1, 0);
			comip_dmas_stop(ssi->dma_rx_channel);
		}

		if (!msg->is_dma_mapped)
			dma_unmap_single(NULL, xfer->rx_dma, count, DMA_FROM_DEVICE);
	}

	return ret;
}

static int comip_spi_transfer_pio(struct comip_ssi *ssi,
		struct spi_message *msg, struct spi_transfer *xfer)
{
	unsigned int count;
	unsigned int c;
	unsigned int val;
	unsigned int step;
	u8 *rx;
	const u8 *tx;

	count = xfer->len;
	rx = xfer->rx_buf;
	tx = xfer->tx_buf;
	c = count;

	if (ssi->bits_per_word <= 8)
		step = 1;
	else if (ssi->bits_per_word <= 16)
		step = 2;
	else
		return -EINVAL;

	while (c) {
		if (tx != NULL) {
			memcpy(&val, tx, step);
			tx += step;
		} else {
			val = ssi->tx_dummy_data;
		}

		if (comip_spi_for_reg_bit(ssi, SSI_STS, SSI_TFNF) < 0) {
			dev_err(&ssi->pdev->dev, "TXS timed out\n");
			SSI_DUMP_REGS(ssi, 0, 0);
			goto out;
		}

		comip_spi_write_reg(ssi, SSI_DATA, val);

		if (comip_spi_for_reg_bit(ssi, SSI_STS, SSI_RFNE) < 0) {
			dev_err(&ssi->pdev->dev, "RXS timed out\n");
			SSI_DUMP_REGS(ssi, 0, 1);
			goto out;
		}

		val = comip_spi_read_reg(ssi, SSI_DATA);

		if (rx != NULL) {
			memcpy(rx, &val, step);
			rx += step;
		}

		c -= step;
	}

	/* for TX_ONLY mode, be sure all words have shifted out */
	if (xfer->rx_buf == NULL) {
		if (comip_spi_for_reg_bit(ssi, SSI_STS, SSI_TFE) < 0)
			dev_err(&ssi->pdev->dev, "TXS timed out\n");
	}

out:
	return count - c;
}

static void comip_spi_msg_handle(struct comip_ssi *ssi,
		struct spi_message *msg)
{
	struct spi_transfer *xfer = NULL;
	unsigned int count;
	int	cs_active = 0;
	int ret = 0;

	comip_spi_enable(ssi, 1);

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (xfer->tx_buf == NULL && xfer->rx_buf == NULL && xfer->len) {
			ret = -EINVAL;
			break;
		}

		comip_spi_config(msg->spi, msg, xfer);

		if (!cs_active) {
			comip_spi_cs_set(ssi, 1);
			cs_active = 1;
		}

		if (xfer->len) {
			if ((msg->is_dma_mapped
				|| (xfer->len >= SSI_DMA_MIN_BYTES))
					&& ssi->dma_supported)
				count = comip_spi_transfer_dma(ssi, msg, xfer);
			else
				count = comip_spi_transfer_pio(ssi, msg, xfer);
			msg->actual_length += count;

			if (count != xfer->len) {
				ret = -EIO;
				break;
			}
		}

		if (xfer->delay_usecs)
			udelay(xfer->delay_usecs);

		if (xfer->cs_change) {
			comip_spi_cs_set(ssi, 0);
			cs_active = 0;
		}
	}

	if (cs_active)
		comip_spi_cs_set(ssi, 0);

	comip_spi_enable(ssi, 0);

	msg->status = ret;

	if (msg->complete)
		msg->complete(msg->context);
}

static void comip_spi_work(struct work_struct *work)
{
	struct comip_ssi *ssi = container_of(work, struct comip_ssi, work);
	unsigned long flags;

	spin_lock_irqsave(&ssi->lock, flags);

	while (!list_empty(&ssi->queue)) {
		struct spi_message *msg;

		msg = container_of(ssi->queue.next, struct spi_message,
				 queue);

		list_del_init(&msg->queue);

		spin_unlock_irqrestore(&ssi->lock, flags);

		comip_spi_msg_handle(ssi, msg);

		spin_lock_irqsave(&ssi->lock, flags);
	}

	spin_unlock_irqrestore(&ssi->lock, flags);
}

static int __init comip_spi_queue_init(struct comip_ssi *ssi)
{
	INIT_LIST_HEAD(&ssi->queue);
	spin_lock_init(&ssi->lock);

	INIT_WORK(&ssi->work, comip_spi_work);
	ssi->workqueue = create_singlethread_workqueue(
					dev_name(ssi->master->dev.parent));
	if (!ssi->workqueue)
		return -EBUSY;

	return 0;
}

static void comip_spi_queue_uninit(struct comip_ssi *ssi)
{
	destroy_workqueue(ssi->workqueue);
	ssi->workqueue = NULL;
}

static int comip_spi_transfer(struct spi_device *spi,
		struct spi_message *msg)
{
	unsigned long flags;
	struct comip_ssi *ssi = spi_master_get_devdata(spi->master);
	struct spi_transfer *xfer = NULL;
	unsigned int count;
	u8 *rx;
	const u8 *tx;

	msg->actual_length = 0;
	msg->status = 0;

	/* Reject invalid messages and transfers. */
	if (list_empty(&msg->transfers) || !msg->complete)
		return -EINVAL;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		tx = xfer->tx_buf;
		rx = xfer->rx_buf;
		count = xfer->len;

		if ((xfer->speed_hz > (ssi->clk_input / 2))
				|| (count && !(rx || tx))
				|| (xfer->bits_per_word &&
					(xfer->bits_per_word < SSI_DFS_MIN
					|| xfer->bits_per_word > SSI_DFS_MAX))) {
			dev_err(&spi->dev, "transfer: %d Hz, %d %s%s, %d bpw\n",
					xfer->speed_hz,
					count,
					tx ? "tx" : "",
					rx ? "rx" : "",
					xfer->bits_per_word);
			return -EINVAL;
		}

		if (xfer->speed_hz && (xfer->speed_hz < (ssi->clk_input / 65534))) {
			dev_err(&spi->dev, "%d Hz max exceeds %d\n",
					xfer->speed_hz,
					(ssi->clk_input / 65534));
			return -EINVAL;
		}

		if (msg->is_dma_mapped || (count < SSI_DMA_MIN_BYTES)
			|| !ssi->dma_supported)
			continue;

		if (tx != NULL) {
			xfer->tx_dma = dma_map_single(&spi->dev, (void *)tx,
					count, DMA_TO_DEVICE);
			if (dma_mapping_error(&spi->dev, xfer->tx_dma)) {
				dev_dbg(&spi->dev, "dma %cX %d bytes error\n",
						'T', count);
				return -EINVAL;
			}
		}

		if (rx != NULL) {
			xfer->rx_dma = dma_map_single(&spi->dev, rx, count,
					DMA_FROM_DEVICE);
			if (dma_mapping_error(&spi->dev, xfer->rx_dma)) {
				dev_dbg(&spi->dev, "dma %cX %d bytes error\n",
						'R', count);
				if (tx != NULL)
					dma_unmap_single(NULL, xfer->tx_dma,
							count, DMA_TO_DEVICE);
				return -EINVAL;
			}
		}
	}

	spin_lock_irqsave(&ssi->lock, flags);
	list_add_tail(&msg->queue, &ssi->queue);
	if (ssi->workqueue)
		queue_work(ssi->workqueue, &ssi->work);
	spin_unlock_irqrestore(&ssi->lock, flags);

	return 0;
}

static int comip_spi_setup(struct spi_device *spi)
{
	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	return comip_spi_config(spi, NULL, NULL);
}

static void comip_spi_cleanup(struct spi_device *spi)
{

}

static int __init comip_spi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct comip_spi_platform_data *pdata = dev->platform_data;
	struct spi_master *master;
	struct comip_ssi *ssi;
	struct resource *r;
	int dma_tx_channel;
	int dma_rx_channel;
	int irq;
	int ret;

	if (!pdata || (pdata->num_chipselect > COMIP_SPI_CS_MAX)) {
		dev_err(&pdev->dev, "Invalid platform data.\n");
		return -EINVAL;
	}

	r = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!r)
		return -ENXIO;
	dma_rx_channel = r->start;

	r = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!r)
		return -ENXIO;
	dma_tx_channel = r->start;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!r || irq < 0)
		return -ENXIO;

	r = request_mem_region(r->start, r->end - r->start + 1, r->name);
	if (!r)
		return -EBUSY;

	/* Allocate master with space for drv_data and null dma buffer. */
	master = spi_alloc_master(dev, sizeof(struct comip_ssi));
	if (!master) {
		dev_err(&pdev->dev, "cannot alloc spi_master\n");
		ret = -ENOMEM;
		goto out;
	}

	master->bus_num = pdev->id;
	master->num_chipselect = pdata->num_chipselect;
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_LOOP;
	master->cleanup = comip_spi_cleanup;
	master->setup = comip_spi_setup;
	master->transfer = comip_spi_transfer;

	ssi = spi_master_get_devdata(master);
	ssi->master = master;
	ssi->pdata = pdata;
	ssi->pdev = pdev;
	ssi->res = r;
	ssi->irq = irq;
	ssi->id = pdev->id;
	ssi->speed_hz = 0;
	ssi->bits_per_word = 0;
	ssi->chip_select = 0;
	ssi->mode = 0;
	ssi->rx_len = 0;
	ssi->rtx_flag = 0;
	ssi->tx_dummy_data = SSI_TX_DUMMY_DATA;
	ssi->dma_tx_channel = dma_tx_channel;
	ssi->dma_rx_channel = dma_rx_channel;
	ssi->base = ioremap(r->start, r->end - r->start + 1);
	if (!ssi->base) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "cannot remap io 0x%08x\n", r->start);
		goto out_ioremap;
	}

	if ((dma_tx_channel >= DMAS_CH_MAX) || (dma_rx_channel >= DMAS_CH_MAX))
		ssi->dma_supported = 0;
	else
		ssi->dma_supported = 1;

	ssi->clk = clk_get(&pdev->dev, "ssi_clk");
	if (IS_ERR(ssi->clk)) {
		ret = PTR_ERR(ssi->clk);
		ssi->clk = NULL;
		dev_err(&pdev->dev, "cannot get ssi clk\n");
		goto out_clk;
	}

	ssi->clk_input = clk_get_rate(ssi->clk);

	/* Enable SSI clock. */
	clk_enable(ssi->clk);

	/* Load default SSI configuration. */
	comip_spi_hw_init(ssi);

	/* Initial SSI DMA. */
	if (ssi->dma_supported) {
		ret = comip_spi_dma_init(ssi);
		if (ret)
			goto out_master;
	}

	/* Initial and start queue. */
	ret = comip_spi_queue_init(ssi);
	if (ret) {
		dev_err(&pdev->dev, "problem initializing queue\n");
		goto out_master;
	}

	/* Register with the SPI framework. */
	platform_set_drvdata(pdev, ssi);
	ret = spi_register_master(master);
	if (ret) {
		dev_err(&pdev->dev, "problem registering ssi master\n");
		goto out_master;
	}

	return ret;

out_master:
	clk_disable(ssi->clk);
	clk_put(ssi->clk);
out_clk:
	iounmap(ssi->base);
out_ioremap:
	spi_master_put(master);
out:
	release_mem_region(r->start, r->end - r->start + 1);
	return ret;
}

static int comip_spi_remove(struct platform_device *pdev)
{
	struct comip_ssi *ssi = platform_get_drvdata(pdev);

	if (!ssi)
		return 0;

	/* Remove the queue */
	comip_spi_queue_uninit(ssi);

	/* Release SSI hardware. */
	comip_spi_hw_uninit(ssi);

	/* Release IO. */
	iounmap(ssi->base);

	/* Release CLK. */
	clk_disable(ssi->clk);
	clk_put(ssi->clk);

	/* Release DMA. */
	if (ssi->dma_supported)
		comip_spi_dma_uninit(ssi);

	/* Disconnect from the SPI framework. */
	spi_unregister_master(ssi->master);

	/* Prevent double remove. */
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int comip_spi_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_ssi *ssi = platform_get_drvdata(pdev);

	clk_disable(ssi->clk);

	return 0;
}

static int comip_spi_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_ssi *ssi = platform_get_drvdata(pdev);

	clk_enable(ssi->clk);

	return 0;
}

static struct dev_pm_ops comip_spi_pm_ops = {
	.suspend_noirq = comip_spi_suspend_noirq,
	.resume_noirq = comip_spi_resume_noirq,
};
#endif

static struct platform_driver comip_spi_driver = {
	.driver = {
		.name = "comip-spi",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &comip_spi_pm_ops,
#endif
	},
	.remove = comip_spi_remove,
};

static int __init comip_spi_init(void)
{
	return platform_driver_probe(&comip_spi_driver, comip_spi_probe);
}

static void __exit comip_spi_exit(void)
{
	platform_driver_unregister(&comip_spi_driver);
}

subsys_initcall(comip_spi_init);
module_exit(comip_spi_exit);

MODULE_AUTHOR("liuyong <liuyong4@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP SSP SPI Controller");
MODULE_LICENSE("GPL");
