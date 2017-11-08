/* arch/arm/mach-comip/dmas.c
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2010 LeadCoreTech Corp.
**
**	PURPOSE: This files contains the dmas driver.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2012-03-12	zouqiao	created
**
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <asm/page.h>
#include <asm/dma.h>
#include <asm/fiq.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include <plat/hardware.h>
#include <plat/dmas.h>

#define COMIP_DMAS_DEBUG
#ifdef COMIP_DMAS_DEBUG
#define DMAS_PRINT(fmt, args...) printk(KERN_ERR "[DMAS]" fmt, ##args)
#else
#define DMAS_PRINT(fmt, args...) printk(KERN_DEBUG "[DMAS]" fmt, ##args)
#endif

#define DMAS_MODULE_CHANNEL 	(16)

#define DMAS_TIMEOUT_UNIT	(1) /* unit : us. */
#define DMAS_BLK_SIZE_MAX	(64 * 1024 *2) /* Special TOP DMAS 0 Unit:Word */

#define DMAS_INT_CHANNEL(bit)	(bit % 16)
#define DMAS_INT_TYPE(bit)	(1 << (bit / 16))

#define DMAS_BIT_VALUE(reg, mask, off, val) \
	reg = ((reg) & (~((mask) << (off)))) | (val << off)

#define DMAS_CH_REQUESTED	(0x00000001)
#define DMAS_CH_CONFIGURED	(0x00000002)
#define DMAS_CH_BLK_SIZE_UNIT_WORD	(0x00000004)

struct comip_dmas_ch {
	const char *name;
	unsigned int flags;
	unsigned int index;
	unsigned int base;
	unsigned int reg_ctl1;
	void (*irq_handler)(int, int, void *);
	void *irq_data;
};

struct comip_dmas_module {
	const char *name;
	const char *clk_name;
	unsigned long base;
	unsigned long start;
	unsigned long end;
	unsigned long int_en0_reg;
	unsigned long int0_reg;
	unsigned long int_en1_reg;
	unsigned long int1_reg;
	unsigned int irq;
	spinlock_t lock;
	struct comip_dmas_ch channel[DMAS_MODULE_CHANNEL];
};

#define DMAS_MODULE(ch)		(&comip_dmas[ch / DMAS_MODULE_CHANNEL])
#define DMAS_CHANNEL(ch)	(&DMAS_MODULE(ch)->channel[ch % DMAS_MODULE_CHANNEL])
#define DMAS_IS_RX_CHANNEL(ch)	((ch % DMAS_MODULE_CHANNEL) >= DMAS_CH8)

static struct comip_dmas_module comip_dmas[] = {
	{
		.name = "ap_dmas",
#if defined(CONFIG_ARCH_LC181X)
		.clk_name = "bus_mclk",
#elif defined(CONFIG_ARCH_LC186X)
		.clk_name = "bus_mclk1",
#else
		#error unknown arch
#endif
		.base = AP_DMAS_BASE,
		.start = DMAS_CH0,
		.end = DMAS_CH15,
		.int_en0_reg = DMAS_INT_EN0,
		.int0_reg = DMAS_INT0,
		.int_en1_reg = DMAS_INT_EN1,
		.int1_reg = DMAS_INT1,
		.irq = INT_AP_DMAS,
	},
#if defined(AUDIO_DMAS_BASE)
	{
		.name = "audio_dmas",
		.clk_name = "bus_mclk",
		.base = AUDIO_DMAS_BASE,
		.start = AUDIO_DMAS_CH0,
		.end = AUDIO_DMAS_CH15,
		.int_en0_reg = DMAS_INT_EN0,
		.int0_reg = DMAS_INT0,
		.int_en1_reg = DMAS_INT_EN1,
		.int1_reg = DMAS_INT1,
		.irq = INT_AUDIO_DMAS,
	},
#endif
#if defined(TOP_DMAS_BASE)
	{
		.name = "top_dmas",
		.clk_name = "top_bus_clk",
		.base = TOP_DMAS_BASE,
		.start = TOP_DMAS_CH0,
		.end = TOP_DMAS_CH15,
		.int_en0_reg = DMAS_INT_EN0_TOP,
		.int0_reg = DMAS_INT0_TOP,
		.int_en1_reg = DMAS_INT_EN1_TOP,
		.int1_reg = DMAS_INT1_TOP,
		.irq = INT_TOP_DMAS,
		.channel = {
			[0] = {
						.flags = DMAS_CH_BLK_SIZE_UNIT_WORD,
					},
		}
	},
#endif
};

static int __comip_dmas_config(unsigned int ch, struct dmas_ch_cfg *cfg)
{
	struct comip_dmas_module *module = DMAS_MODULE(ch);
	struct comip_dmas_ch *dmas = DMAS_CHANNEL(ch);
	unsigned int reg_ctl1 = dmas->reg_ctl1;
	unsigned int block_size = cfg->block_size;
	unsigned int flags = cfg->flags;
	unsigned int int_en;

	if (flags & DMAS_CFG_BLOCK_SIZE) {
		if (DMAS_IS_RX_CHANNEL(ch) && (block_size >= DMAS_BLK_SIZE_MAX))
			block_size = 0;
		if (dmas->flags & DMAS_CH_BLK_SIZE_UNIT_WORD)
			block_size /= 4;
		writel(block_size, DMAS_CH_CTL0(dmas));
	}

	if (flags & DMAS_CFG_SRC_ADDR)
		writel(cfg->src_addr, DMAS_CH_SAR(dmas));

	if (flags & DMAS_CFG_DST_ADDR)
		writel(cfg->dst_addr, DMAS_CH_DAR(dmas));

#if defined(CONFIG_DMAS2_COMIP)
	if (flags & DMAS_CFG_MATCH_ADDR)
		writel(cfg->match_addr, DMAS_CH_INTA(dmas));
#endif

	if (flags & DMAS_CFG_BUS_WIDTH)
		reg_ctl1 = (reg_ctl1 & (~0x3)) | cfg->bus_width;

	if (flags & DMAS_CFG_PRIORITY)
		DMAS_BIT_VALUE(reg_ctl1, 0xf, 8, cfg->priority);

	if (DMAS_IS_RX_CHANNEL(ch)) {
		if (flags & DMAS_CFG_RX_TRANS_TYPE)
			DMAS_BIT_VALUE(reg_ctl1, 0x1, 3, cfg->rx_trans_type);

		if (cfg->rx_trans_type == DMAS_TRANS_WRAP) {
			if (flags & DMAS_CFG_RX_TIMEOUT)
				DMAS_BIT_VALUE(reg_ctl1, 0xfff, 16, cfg->rx_timeout);
			reg_ctl1 |= (0x1 << 2);
		} else {
			reg_ctl1 &= ~(0x1 << 2);
			reg_ctl1 &= ~(0xfff << 16);
		}
	} else {
		if (flags & DMAS_CFG_TX_BLOCK_MODE)
			DMAS_BIT_VALUE(reg_ctl1, 0x1, 16, cfg->tx_block_mode);

		if (flags & DMAS_CFG_TX_TRANS_MODE)
			DMAS_BIT_VALUE(reg_ctl1, 0x1, 17, cfg->tx_trans_mode);

		if ((cfg->tx_trans_mode == DMAS_TRANS_SPECIAL)
			&& (flags & DMAS_CFG_TX_FIX_VALUE))
			writel(cfg->tx_fix_value, DMAS_CH_WD(module, dmas->index));
	}

	/* For CP will also write this reg. */
	dmas->reg_ctl1 = readl(DMAS_CH_CTL1(dmas));
	if (reg_ctl1 != dmas->reg_ctl1) {
		dmas->reg_ctl1 = reg_ctl1;
		writel(reg_ctl1, DMAS_CH_CTL1(dmas));
	}

	if (flags & DMAS_CFG_IRQ_EN) {
		if (cfg->irq_en & (DMAS_INT_DONE | DMAS_INT_HBLK_FLUSH)) {
			int_en = readl(DMAS_REG(module, module->int_en0_reg));
			if (cfg->irq_en & DMAS_INT_DONE)
				int_en |= (1 << dmas->index);
			if (cfg->irq_en & DMAS_INT_HBLK_FLUSH)
				int_en |= (1 << (dmas->index + 16));
			writel(int_en, DMAS_REG(module, module->int_en0_reg));
		}

#if defined(CONFIG_DMAS2_COMIP)
		if (cfg->irq_en & DMAS_INT_MATCH) {
			int_en = readl(DMAS_REG(module, module->int_en1_reg));
			if (cfg->irq_en & DMAS_INT_MATCH)
				int_en |= (1 << dmas->index);
			writel(int_en, DMAS_REG(module, module->int_en1_reg));
		}
#endif
	}

	if (flags & DMAS_CFG_IRQ_HANDLER)
		dmas->irq_handler = cfg->irq_handler;

	if (flags & DMAS_CFG_IRQ_DATA)
		dmas->irq_data = cfg->irq_data;

	return 0;
}

static irqreturn_t comip_dmas_intr_handle(int irq, void *dev_id)
{
	struct comip_dmas_module *module = (struct comip_dmas_module *)dev_id;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	unsigned long status;
	unsigned int type;
	unsigned int index;
	unsigned int ch;
	unsigned int i;

	if (!module)
		return IRQ_HANDLED;

	spin_lock_irqsave(&module->lock, flags);

	status = readl(DMAS_REG(module, module->int0_reg));

	for_each_set_bit(i, &status, 32) {
		/* Clear interrupt. */
		writel(i, DMAS_REG(module, DMAS_INT_CLR0));

		index = DMAS_INT_CHANNEL(i);
		type = DMAS_INT_TYPE(i);
		ch = index + module->start;
		dmas = &module->channel[index];
		spin_unlock_irqrestore(&module->lock, flags);
		if(dmas->irq_handler)
			dmas->irq_handler(ch, type, dmas->irq_data);
		else
			DMAS_PRINT("Spurious irq for dmas channel%d\n", ch);
		spin_lock_irqsave(&module->lock, flags);
	}

#if defined(CONFIG_DMAS2_COMIP)
	status = readl(DMAS_REG(module, module->int1_reg));

	for_each_set_bit(i, &status, 16) {
		/* Clear interrupt. */
		writel(i, DMAS_REG(module, DMAS_INT_CLR1));

		index = DMAS_INT_CHANNEL(i);
		type = DMAS_INT_MATCH;
		ch = index + module->start;
		dmas = &module->channel[index];
		spin_unlock_irqrestore(&module->lock, flags);
		if(dmas->irq_handler)
			dmas->irq_handler(ch, type, dmas->irq_data);
		else
			DMAS_PRINT("Spurious irq for dmas channel%d\n", ch);
		spin_lock_irqsave(&module->lock, flags);
	}
#endif

	spin_unlock_irqrestore(&module->lock, flags);

	return IRQ_HANDLED;
}

static void comip_dmas_module_init(struct comip_dmas_module *module)
{
	struct clk *clk;
	unsigned long freq;
	unsigned long val;
	unsigned int index;
	int ret;

	/* Disable all channels. */
	for (index = 0; index <= (module->end - module->start); index++) {
		module->channel[index].index = index;
		module->channel[index].base = DMAS_CH_REG(module, index);
		writel(index, DMAS_REG(module, DMAS_CLR));
	}

	/* Clear the DMAS interrupt. */
	writel(0xffffffff, DMAS_REG(module, DMAS_INT_RAW0));
	writel(0xffffffff, DMAS_REG(module, DMAS_INT_CLR0));
#if defined(CONFIG_DMAS2_COMIP)
	writel(0xffffffff, DMAS_REG(module, DMAS_INT_RAW1));
	writel(0xffffffff, DMAS_REG(module, DMAS_INT_CLR1));
#endif

	/* Get DMAS mclk frequency. */
	clk = clk_get(NULL, module->clk_name);
	if (IS_ERR(clk)) {
		freq = 208000000;
		DMAS_PRINT("failed to get bus clock. Use default %ld\n", freq);
	} else
		freq = clk_get_rate(clk);
	clk_put(clk);

	/* Timeout unit is 1us. */
	val = ((freq / 1000000) * DMAS_TIMEOUT_UNIT);
	if (val > 255)
		val = 255;
	else if (val > 0)
		val--;
	writel(val, DMAS_REG(module, DMAS_INTV_UNIT));

	ret = request_irq(module->irq, comip_dmas_intr_handle,
		IRQF_DISABLED, module->name, (void*)module);
	if (ret)
		DMAS_PRINT("Wow! Can't register IRQ for DMAS\n");
}

int comip_dmas_request(const char *name, unsigned int ch)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	int ret = 0;

	if (ch >= DMAS_CH_MAX) {
		ret = -EINVAL;
		goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	if (dmas->flags & DMAS_CH_REQUESTED) {
		ret = -EEXIST;
		goto err_unlock;
	}

	dmas->name = name;
	dmas->flags |= DMAS_CH_REQUESTED;

err_unlock:
	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Request channel%d failed(%d)\n", ch, ret);

	return ret;
}

int comip_dmas_free(unsigned int ch)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	int ret = 0;

	if (ch >= DMAS_CH_MAX) {
		ret = -EINVAL;
		goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	if (!(dmas->flags & DMAS_CH_REQUESTED)) {
		ret = -EEXIST;
		goto err_unlock;
	}

	writel(dmas->index, DMAS_REG(module, DMAS_CLR));

	dmas->flags &= ~(DMAS_CH_REQUESTED | DMAS_CH_CONFIGURED);

err_unlock:
	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Free channel%d failed(%d)\n", ch, ret);

	return ret;
}

int comip_dmas_config(unsigned int ch, struct dmas_ch_cfg *cfg)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	int ret = -EINVAL;

	if ((ch >= DMAS_CH_MAX) || !cfg)
		goto err;

	if (cfg->block_size > DMAS_BLK_SIZE_MAX)
		goto err;

	if ((cfg->src_addr & 0x3) || (cfg->dst_addr & 0x3))
		goto err;

	if (DMAS_IS_RX_CHANNEL(ch)) {
		if (cfg->rx_trans_type >= DMAS_TRANS_TYPE_MAX)
			goto err;

		if (cfg->rx_trans_type == DMAS_TRANS_WRAP)
			if ((cfg->block_size & 0x3f) || (cfg->rx_timeout > 0xfff))
				goto err;
	} else {
		if (cfg->block_size >= DMAS_BLK_SIZE_MAX)
			goto err;

		if (cfg->tx_trans_mode >= DMAS_BLOCK_MODE_MAX)
			goto err;

		if (cfg->tx_block_mode >= DMAS_TRANS_MODE_MAX)
			goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	if (!(dmas->flags & DMAS_CH_REQUESTED)) {
		ret = -EEXIST;
		goto err_unlock;
	}

	if (readl(DMAS_REG(module, DMAS_STA)) & (1 << dmas->index)) {
		writel(dmas->index, DMAS_REG(module, DMAS_CLR));
		DMAS_PRINT("Trying to config active channel%d\n", ch);
	}

	ret = __comip_dmas_config(ch, cfg);
	if (ret)
		goto err_unlock;

	dmas->flags |= DMAS_CH_CONFIGURED;

err_unlock:
	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Config channel%d failed(%d)\n", ch, ret);

	return ret;
}

int comip_dmas_get(unsigned int ch, unsigned int *addr)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	int ret = 0;

	if ((ch >= DMAS_CH_MAX) || !addr) {
		ret = -EINVAL;
		goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	*addr = readl(DMAS_CH_CA(dmas));

	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Get channel%d failed(%d)\n", ch, ret);

	return ret;
}

int comip_dmas_state(unsigned int ch)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	int ret = 0;

	if (ch >= DMAS_CH_MAX) {
		DMAS_PRINT("Invalid channel(%d)\n", ch);
		return -EINVAL;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	ret = !!(readl(DMAS_REG(module, DMAS_STA)) & (1 << dmas->index));

	spin_unlock_irqrestore(&module->lock, flags);

	return ret;
}

int comip_dmas_start(unsigned int ch)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	int ret = 0;

	if (ch >= DMAS_CH_MAX) {
		ret = -EINVAL;
		goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	if (!(dmas->flags & DMAS_CH_REQUESTED)) {
		ret = -EEXIST;
		goto err_unlock;
	}

	if (!(dmas->flags & DMAS_CH_CONFIGURED)) {
		ret = -ENODEV;
		goto err_unlock;
	}

	writel(dmas->index, DMAS_REG(module, DMAS_EN));

err_unlock:
	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Start channel%d failed(%d)\n", ch, ret);

	return ret;
}

int comip_dmas_stop(unsigned int ch)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	int ret = 0;

	if (ch >= DMAS_CH_MAX) {
		ret = -EINVAL;
		goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	if (!(dmas->flags & DMAS_CH_REQUESTED)) {
		ret = -EEXIST;
		goto err_unlock;
	}

	if (!(dmas->flags & DMAS_CH_CONFIGURED)) {
		ret = -ENODEV;
		goto err_unlock;
	}

	writel(dmas->index, DMAS_REG(module, DMAS_CLR));

err_unlock:
	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Stop channel%d failed(%d)\n", ch, ret);

	return ret;
}

int comip_dmas_intr_enable(unsigned int ch, unsigned int intr)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	unsigned int int_en;
	int ret = 0;

	if (ch >= DMAS_CH_MAX) {
		ret = -EINVAL;
		goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	if (!(dmas->flags & DMAS_CH_REQUESTED)) {
		ret = -EEXIST;
		goto err_unlock;
	}

	if (!(dmas->flags & DMAS_CH_CONFIGURED)) {
		ret = -ENODEV;
		goto err_unlock;
	}

	if (intr & (DMAS_INT_DONE | DMAS_INT_HBLK_FLUSH)) {
		int_en = readl(DMAS_REG(module, module->int_en0_reg));
		if (intr & DMAS_INT_DONE)
			int_en |= (1 << dmas->index);
		if (intr & DMAS_INT_HBLK_FLUSH)
			int_en |= (1 << (dmas->index + 16));
		writel(int_en, DMAS_REG(module, module->int_en0_reg));
	}

#if defined(CONFIG_DMAS2_COMIP)
	if (intr & DMAS_INT_MATCH) {
		int_en = readl(DMAS_REG(module, module->int_en1_reg));
		if (intr & DMAS_INT_MATCH)
			int_en |= (1 << dmas->index);
		writel(int_en, DMAS_REG(module, module->int_en1_reg));
	}
#endif

err_unlock:
	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Enable channel%d interrupt failed(%d)\n", ch, ret);

	return ret;
}

int comip_dmas_intr_disable(unsigned int ch, unsigned int intr)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;
	unsigned long flags;
	unsigned int int_en;
	int ret = 0;

	if (ch >= DMAS_CH_MAX) {
		ret = -EINVAL;
		goto err;
	}

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	spin_lock_irqsave(&module->lock, flags);

	if (!(dmas->flags & DMAS_CH_REQUESTED)) {
		ret = -EEXIST;
		goto err_unlock;
	}

	if (!(dmas->flags & DMAS_CH_CONFIGURED)) {
		ret = -ENODEV;
		goto err_unlock;
	}

	if (intr & (DMAS_INT_DONE | DMAS_INT_HBLK_FLUSH)) {
		int_en = readl(DMAS_REG(module, module->int_en0_reg));
		if (intr & DMAS_INT_DONE)
			int_en &= ~(1 << dmas->index);
		if (intr & DMAS_INT_HBLK_FLUSH)
			int_en &= ~(1 << (dmas->index + 16));
		writel(int_en, DMAS_REG(module, module->int_en0_reg));
	}

#if defined(CONFIG_DMAS2_COMIP)
	if (intr & DMAS_INT_MATCH) {
		int_en = readl(DMAS_REG(module, module->int_en1_reg));
		if (intr & DMAS_INT_MATCH)
			int_en &= ~(1 << dmas->index);
		writel(int_en, DMAS_REG(module, module->int_en1_reg));
	}
#endif

err_unlock:
	spin_unlock_irqrestore(&module->lock, flags);

err:
	if (ret)
		DMAS_PRINT("Disable channel%d interrupt failed(%d)\n", ch, ret);

	return ret;
}

void comip_dmas_dump(unsigned int ch)
{
	struct comip_dmas_module *module;
	struct comip_dmas_ch *dmas;

	if (ch >= DMAS_CH_MAX)
		return;

	module = DMAS_MODULE(ch);
	dmas = DMAS_CHANNEL(ch);

	DMAS_PRINT("Channle%d\n", ch);
	DMAS_PRINT("DMAS_STA: 0x%08x\n",
		readl(DMAS_REG(module, DMAS_STA)));
	DMAS_PRINT("DMAS_INT_RAW0: 0x%08x\n",
		readl(DMAS_REG(module, DMAS_INT_RAW0)));
	DMAS_PRINT("DMAS_INT_EN0: 0x%08x\n",
		readl(DMAS_REG(module, module->int_en0_reg)));
	DMAS_PRINT("DMAS_INT0: 0x%08x\n",
		readl(DMAS_REG(module, module->int0_reg)));
#if defined(CONFIG_DMAS2_COMIP)
	DMAS_PRINT("DMAS_INT_RAW1: 0x%08x\n",
		readl(DMAS_REG(module, DMAS_INT_RAW1)));
	DMAS_PRINT("DMAS_INT_EN1: 0x%08x\n",
		readl(DMAS_REG(module, module->int_en1_reg)));
	DMAS_PRINT("DMAS_INT1: 0x%08x\n",
		readl(DMAS_REG(module, module->int1_reg)));
#endif
	DMAS_PRINT("DMAS_INTV_UNIT: 0x%08x\n",
		readl(DMAS_REG(module, DMAS_INTV_UNIT)));
	DMAS_PRINT("DMAS_CH_CA: 0x%08x\n", readl(DMAS_CH_CA(dmas)));
	DMAS_PRINT("DMAS_CH_SAR: 0x%08x\n", readl(DMAS_CH_SAR(dmas)));
	DMAS_PRINT("DMAS_CH_DAR: 0x%08x\n", readl(DMAS_CH_DAR(dmas)));
	DMAS_PRINT("DMAS_CH_CTL0: 0x%08x\n", readl(DMAS_CH_CTL0(dmas)));
	DMAS_PRINT("DMAS_CH_CTL1: 0x%08x\n", readl(DMAS_CH_CTL1(dmas)));
}

void __init comip_dmas_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(comip_dmas); i++) {
		spin_lock_init(&comip_dmas[i].lock);
		comip_dmas_module_init(&comip_dmas[i]);
	}
}

EXPORT_SYMBOL(comip_dmas_request);
EXPORT_SYMBOL(comip_dmas_free);
EXPORT_SYMBOL(comip_dmas_config);
EXPORT_SYMBOL(comip_dmas_get);
EXPORT_SYMBOL(comip_dmas_state);
EXPORT_SYMBOL(comip_dmas_start);
EXPORT_SYMBOL(comip_dmas_stop);
EXPORT_SYMBOL(comip_dmas_intr_enable);
EXPORT_SYMBOL(comip_dmas_intr_disable);
EXPORT_SYMBOL(comip_dmas_dump);
