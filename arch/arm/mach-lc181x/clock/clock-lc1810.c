/*
   arch/arm/mach-comip/comip-clocks.c
*
* Use of source code is subject to the terms of the LeadCore license agreement under
* which you licensed source code. If you did not accept the terms of the license agreement,
* you are not authorized to use source code. For the terms of the license, please see the
* license agreement between you and LeadCore.
*
* Copyright (c) 2010-2019  LeadCoreTech Corp.
*
*  PURPOSE: This files contains the comip hardware plarform's clock driver.
*
*  CHANGE HISTORY:
*
*  Version	Date		Author		Description
*  1.0.0		2011-12-31	wujingchao	created
*
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <mach/comip-multicore.h>
#include <plat/clock.h>	
#include <plat/hardware.h>

#include "clock.h"

static int comip_clk_enable_generic(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		if ((clk->grclk_mask != 0)
			&& (clk->divclk_reg == clk->mclk_reg)) {
			val = (1 << clk->mclk_bit) | (1 << clk->mclk_we_bit);
			val |= (clk->grclk_val << clk->grclk_bit)
				| (1 << clk->grclk_we_bit);
			writel(val, clk->mclk_reg);
		} else {
			val = (1 << clk->mclk_bit) | (1 << clk->mclk_we_bit);
			writel(val, clk->mclk_reg);
		}
	} else if (clk->grclk_reg != 0) {
		val = (1 << clk->grclk_we_bit)
			| (clk->grclk_val << clk->grclk_bit);
		writel(val, clk->grclk_reg);
	}

	if (clk->ifclk_reg != 0) {
		val = (1 << clk->ifclk_bit) | (1 << clk->ifclk_we_bit);
		writel(val, clk->ifclk_reg);
	}

	return 0;
}

static void comip_clk_disable_generic(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		val = (1 << clk->mclk_we_bit);
		writel(val, clk->mclk_reg);

		if ((clk->grclk_mask != 0)
			&& (clk->divclk_reg == clk->mclk_reg)) {
			val = (1 << clk->grclk_we_bit);
			writel(val, clk->mclk_reg);
		}
	} else if (clk->grclk_reg != 0) {
		val = (1 << clk->grclk_we_bit);
		writel(val, clk->grclk_reg);
	}

	if (clk->ifclk_reg != 0) {
		val = (1 << clk->ifclk_we_bit);
		writel(val, clk->ifclk_reg);
	}
}

static void comip_clk_init_generic(struct clk *clk)
{
	struct clk_range *range = (struct clk_range *)clk->cust;
	unsigned int gr;
	unsigned int div;
	unsigned int val;

	BUG_ON(!clk->divclk_reg);

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;
	clk->rate = clk->parent->rate / (div + range->offset);

	if (clk->grclk_mask != 0) {
		val = readl(clk->divclk_reg);
		gr = (val >> clk->grclk_bit) & clk->grclk_mask;
		if (!gr)
			gr = 8;
		clk->grclk_val = gr;
		clk->rate = clk->rate * gr / 8;
	}
}

static int comip_clk_set_rate_generic(struct clk *clk, unsigned long rate)
{
	struct clk_range *range = (struct clk_range *)clk->cust;
	unsigned long parent_rate = clk->parent->rate;
	unsigned long cal_rate;
	unsigned int gr;
	unsigned int div;
	unsigned int val;
	int ret = 0;

	if (rate > parent_rate)
		return -EINVAL;

	if (clk->grclk_mask != 0) {
		for (gr = 0; gr <= 8; gr++) {
			for (div = range->min; div <= range->max; div++) {
				cal_rate = (parent_rate / div) * gr / 8;
				if (cal_rate == rate) {
					clk->divclk_val = div;
					clk->grclk_val = gr;
					clk->rate = rate;
					val = (1 << clk->divclk_we_bit)
						| ((div - range->offset)
							<< clk->divclk_bit);
					val |= (1 << clk->grclk_we_bit)
						| (gr << clk->grclk_bit);
					writel(val, clk->divclk_reg);
					return 0;
				}
			}
		}

		ret = -EINVAL;
	} else {
		div = (parent_rate / rate) - range->offset;
		if ((div < range->min) || (div > range->max))
			return -EINVAL;

		clk->divclk_val = div;
		clk->rate = parent_rate / (div + range->offset);

		val = readl(clk->divclk_reg);
		val &= ~(clk->divclk_mask << clk->divclk_bit);
		val |= (clk->divclk_val << clk->divclk_bit);
		writel(val, clk->divclk_reg);
	}

	return ret;
}

static void comip_clk_init_by_table(struct clk *clk)
{
	unsigned int mul;
	unsigned int div;
	unsigned int val;

	if (clk->divclk_reg != 0) {
		val = readl(clk->divclk_reg);
		mul = (val >> 16) & 0xffff;
		div = val & 0xffff;
		clk->rate = (clk->parent->rate / div) * mul / 2;
	}

	if (clk->grclk_reg != 0) {
		val = readl(clk->grclk_reg);
		clk->grclk_val = (val >> clk->grclk_bit) & clk->grclk_mask;
		clk->rate = (clk->rate * clk->grclk_val / 8);
	}
}

static int comip_clk_set_rate_by_table(struct clk *clk,unsigned long rate)
{
	struct clk_table *tbl= (struct clk_table *)clk->cust;
	unsigned long parent_rate = clk->parent->rate;
	unsigned int find = 0;
	unsigned int val;

	if (rate > clk->parent->rate)
		return -EINVAL;

	while (tbl->rate && tbl->parent_rate) {
		if ((tbl->rate == rate)
			&& (tbl->parent_rate == parent_rate)) {
			find = 1;
			break;
		}
		tbl++;
	}

	if (!find)
		return -ENODEV;

	clk->rate = rate;
	if (clk->divclk_reg != 0)
		writel(((tbl->mul << 16) | tbl->div), clk->divclk_reg);

	if (clk->grclk_reg != 0) {
		clk->grclk_val = tbl->gr & clk->grclk_mask;
		clk->rate = (clk->rate * clk->grclk_val / 8);
		val = clk->grclk_val << clk->grclk_bit;
		val |= (1 << clk->grclk_we_bit);
		writel(val, clk->grclk_reg);
	}

	return 0;
}

static void comip_clk_pll_init(struct clk *clk)
{
	unsigned long parent_rate;
	unsigned int val;
	unsigned int nf;
	unsigned int nr;
	unsigned int no;

	parent_rate = clk->parent->rate;
	val = readl(clk->divclk_reg);
	nf = (((val >> 8) & 0x7f) + 1) * 2;
	nr = (val & 0x1f) + 1;
	no = 1 << ((val >> 16) & 0x03);

	clk->rate = parent_rate / (nr * no) * nf;
}

static void comip_clk_a9_init(struct clk *clk)
{
	unsigned int gr;
	unsigned int val;

	val = readl(clk->grclk_reg);
	gr = (val >> clk->grclk_bit) & clk->grclk_mask;
	if (!gr)
		gr = 8;
	clk->grclk_val = gr;
	clk->rate = clk->parent->rate / 8 * gr;
	printk("A9 rate: %ld\n", clk->rate);
}

static int comip_clk_a9_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int gr;
	unsigned int val;

	if (rate > clk->parent->rate)
		return -EINVAL;

	gr = rate / (clk->parent->rate / 8);
	if (gr > 8)
		return -EINVAL;

	clk->grclk_val = gr;
	clk->rate = clk->parent->rate / 8 * gr;
	val = (1 << clk->grclk_we_bit) | (gr << clk->grclk_bit);
	writel(val, clk->grclk_reg);

	return 0;
}

static void comip_clk_a9_peri_init(struct clk *clk)
{
	unsigned int gr;
	unsigned int val;

	val = readl(clk->grclk_reg);
	gr = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = gr;
	clk->rate = clk->parent->rate / 8 * (gr + 1);
}

static int comip_clk_a9_peri_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int gr;
	unsigned int val;

	if (rate > clk->parent->rate)
		return -EINVAL;

	gr = rate / (clk->parent->rate / 8);
	if ((gr < 1) || (gr > 4))
		return -EINVAL;

	clk->grclk_val = gr - 1;
	clk->rate = clk->parent->rate / 8 * gr;
	val = (1 << clk->grclk_we_bit) | ((gr - 1) << clk->grclk_bit);
	writel(val, clk->grclk_reg);

	return 0;
}

static void comip_clk_a9_acp_init(struct clk *clk)
{
	unsigned int gr;
	unsigned int val;

	val = readl(clk->grclk_reg);
	gr = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = gr;
	clk->rate = clk->parent->rate / 8 * (gr + 1);
}

static int comip_clk_a9_acp_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int gr;
	unsigned int val;

	if (rate > clk->parent->rate)
		return -EINVAL;

	gr = rate / (clk->parent->rate / 8);
	if ((gr != 1) || (gr != 2) || (gr != 4))
		return -EINVAL;

	clk->grclk_val = gr - 1;
	clk->rate = clk->parent->rate / 8 * gr;
	val = (1 << clk->grclk_we_bit) | ((gr - 1) << clk->grclk_bit);
	writel(val, clk->grclk_reg);

	return 0;
}

static void comip_clk_clkout_init(struct clk *clk)
{
	unsigned int val;

	BUG_ON(!clk->mclk_reg);
	BUG_ON(!clk->ifclk_reg);

	val = readl(clk->ifclk_reg);
	clk->parent_id = (val >> clk->ifclk_bit) & 0x7;
}

static int comip_clk_clkout_enable(struct clk *clk)
{
	unsigned int val;

	val = readl(clk->ifclk_reg);
	val &= ~(0x7 << clk->ifclk_bit);
	val |= (clk->parent_id << clk->ifclk_bit);
	writel(val, clk->ifclk_reg);

	return 0;
}

static void comip_clk_clkout_disable(struct clk *clk)
{
	unsigned int val;

	val = readl(clk->ifclk_reg);
	val &= ~(0x7 << clk->ifclk_bit);
	val |= (6 << clk->ifclk_bit);
	writel(val, clk->ifclk_reg);
}

static int comip_clk_clkout_set_rate(struct clk *clk, unsigned long rate)
{
	struct clk *parent = clk->parent;
	unsigned int index;
	unsigned int val;
	unsigned long parent_rate;
	unsigned long div;
	unsigned long bit;
	unsigned long mask;

	if (rate > clk->parent->rate)
		return -EINVAL;

	if (parent == &bus_mclk)
		index = 0;
	else if (parent == &pll1_mclk)
		index = 1;
	else if (parent == &pll1_mclk_div2)
		index = 2;
	else if (parent == &func_mclk)
		index = 3;
	else if (parent == &clk_32k) {
		if (clk->rate != parent->rate)
			index = 4;
		else
			index = 5;
	} else
		return -EINVAL;

	if (index < 0)
		return index;

	clk->parent_id = index;

	if (index == 5)
		return 0;

	if (index >= 3) {
		bit = 4 * (index - 3) + 24;
		mask = 0x0f;
	} else {
		bit = (8 * index);
		mask = 0xff;
	}

	parent_rate = clk->parent->rate;
	if (index == 1) {
		div = (parent_rate / rate) - 1;
		clk->rate = parent_rate / (div + 1);
	} else {
		div = ((parent_rate / rate) / 2) - 1;
		clk->rate = parent_rate / (2 * (div + 1));
	}

	if (likely(clk->mclk_reg != 0)) {
		val = readl(clk->mclk_reg);
		val &= ~(mask << bit);
		val |= (div << bit);
		writel(val, clk->mclk_reg);
	}

	return 0;
}

static int comip_clk_clkout_set_parent(struct clk *clk, struct clk *parent)
{
	if (!parent)
		return -EINVAL;

	clk->rate = 0;
	clk->parent = parent;

	return 0;
}

static void comip_clk_timer_init(struct clk *clk)
{
	unsigned int val;
	unsigned int *mask = (unsigned int *)clk->cust;

	BUG_ON(!clk->mclk_reg);
	BUG_ON(!clk->ifclk_reg);
	BUG_ON(!clk->cust);

	val = readl(clk->mclk_reg);
	clk->parent_id = (val >> clk->mclk_bit) & 0x7;
	*mask = 0;
}

static int comip_clk_timer_enable(struct clk *clk)
{
	unsigned int val;
	unsigned int *mask = (unsigned int *)clk->cust;

	/* Enable bus clock. */
	if (!(*mask)) {
		val = readl(clk->ifclk_reg);
		val |= (1 << (clk->ifclk_bit + 16));
		val |= (1 << clk->ifclk_bit);
		writel(val, clk->ifclk_reg);
	}
	*mask |= (1 << clk->id);

	/* Enable main clock. */
	val = readl(clk->mclk_reg);
	val &= ~(0x7 << clk->mclk_bit);
	val |= (clk->parent_id << clk->mclk_bit);
	writel(val, clk->mclk_reg);

	return 0;
}

static void comip_clk_timer_disable(struct clk *clk)
{
	unsigned int val;
	unsigned int *mask = (unsigned int *)clk->cust;

	/* Disable main clock. */
	val = readl(clk->mclk_reg);
	val &= ~(0x7 << clk->mclk_bit);
	val |= (6 << clk->mclk_bit);
	writel(val, clk->mclk_reg);

	/* Disable bus clock. */
	*mask &= ~(1 << clk->id);
	if (!(*mask)) {
		val = readl(clk->ifclk_reg);
		val |= (1 << (clk->ifclk_bit + 16));
		val &= ~(1 << clk->ifclk_bit);
		writel(val, clk->ifclk_reg);
	}
}

static int comip_clk_timer_set_rate(struct clk *clk, unsigned long rate)
{
	struct clk *parent = clk->parent;
	unsigned int index;
	unsigned int val;
	unsigned long parent_rate;
	unsigned long div;
	unsigned long bit;
	unsigned long mask;

	if (rate > clk->parent->rate)
		return -EINVAL;

	if (parent == &bus_mclk)
		index = 0;
	else if (parent == &pll1_mclk)
		index = 1;
	else if (parent == &ctl_pclk)
		index = 2;
	else if (parent == &func_mclk) {
		if (rate != parent->rate)
			index = 3;
		else
			index = 4;
	} else if (parent == &clk_32k)
		index = 5;
	else
		return -EINVAL;

	if (index < 0)
		return index;

	clk->parent_id = index;

	if (index >= 4)
		return 0;

	if (index >= 3) {
		bit = 4 * (index - 3) + 24;
		mask = 0x0f;
	} else {
		bit = (8 * index);
		mask = 0xff;
	}

	parent_rate = clk->parent->rate;
	div = ((parent_rate / rate) / 2) - 1;
	clk->rate = parent_rate / (2 * (div + 1));

	val = readl(clk->mclk_reg);
	val &= ~(mask << bit);
	val |= (div << bit);
	writel(val, clk->mclk_reg);

	return 0;
}

static int comip_clk_timer_set_parent(struct clk *clk, struct clk *parent)
{
	if (!parent)
		return -EINVAL;

	clk->rate = 0;
	clk->parent = parent;

	return 0;
}

static void comip_clk_usb_init(struct clk *clk)
{
	unsigned int div;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div = val & 0x7;
	clk->rate = clk->parent->rate / (div + 1);
}

static int comip_clk_usb_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int div;

	if (rate > clk->parent->rate)
		return -EINVAL;

	div = clk->parent->rate / rate - 1;
	if ((div < 1) || (div > 7))
		return -EINVAL;

	clk->rate = clk->parent->rate / (div + 1);

	writel(div, clk->divclk_reg);

	return 0;
}

static void comip_clk_ssi_init(struct clk *clk)
{
	unsigned int gr;
	unsigned int div;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;
	clk->rate = clk->parent->rate / (div + 1);

	val = readl(clk->grclk_reg);
	gr = (val >> clk->grclk_bit) & clk->grclk_mask;
	if (!gr)
		gr = 8;
	clk->grclk_val = gr;
	clk->rate = clk->rate * gr / 8;
}

static int comip_clk_ssi_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int grs[] = {1, 2, 4, 8};
	unsigned int val;
	int div;
	int i;

	if (rate > clk->parent->rate)
		return -EINVAL;

	for (i = (ARRAY_SIZE(grs) - 1); i >= 0; i--) {
		div = clk->parent->rate / (rate * 8 / grs[i]) - 1;
		if (div < 1)
			return -EINVAL;
		else if (div <= 15)
			break;
	}

	if (i < 0)
		return -EINVAL;

	clk->grclk_val = grs[i];
	clk->divclk_val = div;
	clk->rate = clk->parent->rate / (div + 1) * grs[i] / 8;
	val = (1 << clk->grclk_we_bit) | (grs[i] << clk->grclk_bit);
	writel(val, clk->grclk_reg);
	val = (1 << clk->divclk_we_bit) | (div << clk->divclk_bit);
	writel(val, clk->divclk_reg);

	return 0;
}

static void comip_clk_sdmmc_init(struct clk *clk)
{
	unsigned int gr;
	unsigned int div;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;
	clk->rate = clk->parent->rate / (2 * (div + 1));

	val = readl(clk->grclk_reg);
	gr = (val >> clk->grclk_bit) & clk->grclk_mask;
	if (!gr)
		gr = 8;
	clk->grclk_val = gr;
	clk->rate = clk->rate * gr / 8;
}

static int comip_clk_sdmmc_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int grs[] = {1, 2, 4, 8};
	unsigned int val;
	int div;
	int i;

	if (rate > clk->parent->rate)
		return -EINVAL;

	for (i = (ARRAY_SIZE(grs) - 1); i >= 0; i--) {
		div = clk->parent->rate / (rate * 16 / grs[i]) - 1;
		if (div < 0)
			return -EINVAL;
		else if (div <= 7)
			break;
	}

	if (i < 0)
		return -EINVAL;

	clk->grclk_val = grs[i];
	clk->divclk_val = div;
	clk->rate = (clk->parent->rate / (2 * (div + 1))) * grs[i] / 8;

	val = (1 << clk->divclk_we_bit) | (div << clk->divclk_bit);
	writel(val, clk->divclk_reg);
	val = (1 << clk->grclk_we_bit) | (grs[i] << clk->grclk_bit);
	writel(val, clk->grclk_reg);

	return 0;
}

static int comip_clk_lcd_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int div;
	unsigned int val;
	if (rate > clk->parent->rate)
		return -EINVAL;

	div = clk->parent->rate / rate;
	if ((div < 1) || (div > 39))
		return -EINVAL;
	if (div <= 31) {
		val = ((div - 1) << clk->divclk_bit) | (1 << clk->divclk_we_bit);
		writel(val, clk->divclk_reg);
		val = (0x08 | 0x01 << clk->grclk_we_bit);
		writel(val, clk->divclk_reg);
		clk->rate = clk->parent->rate / div;
		clk->divclk_val = div - 1;
		clk->grclk_val = 0x08;
	}else if (div > 31 ) {
		val = 31 * 8 / div;
		clk->grclk_val = val;
		clk->divclk_val = 30;
		val = (val | 0x01 << clk->grclk_we_bit);
		writel(val, clk->divclk_reg);
		val = (30 << clk->divclk_bit) | (1 << clk->divclk_we_bit);
		writel(val, clk->divclk_reg);
		clk->rate = clk->parent->rate * clk->grclk_val / 8 / 31;
	}

	return 0;
}



static int comip_clk_enable(struct clk *clk)
{
	int ret = 0;

	if (clk->user++ == 0) {
		if (clk->enable)
			ret = clk->enable(clk);

		if (unlikely(ret != 0))
			clk->user--;
	}

	return ret;
}

static void comip_clk_disable(struct clk *clk)
{
	if (clk->user > 0 && !(--clk->user)) {
		if (clk->disable)
			clk->disable(clk);
	}
}

static int comip_clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EINVAL;

	if (clk->flag & CLK_RATE_FIXED)
		return 0;

	if (rate == 0)
		return ret;

	if (clk->set_rate)
		ret = clk->set_rate(clk, rate);

	return ret;
}

static int comip_clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = -ENODEV;

	if (!clk->set_parent)
		goto out;

	ret = clk->set_parent(clk, parent);
	if (!ret)
		clk->parent = parent;

out:
	return ret;
}

static struct clk_functions comip_clk_funcs = {
	.clk_enable		= comip_clk_enable,
	.clk_disable		= comip_clk_disable,
	.clk_set_rate		= comip_clk_set_rate,
	.clk_set_parent		= comip_clk_set_parent,
};

void __init comip_clks_init(void)
{
	struct clk ** clkp;
	clk_init(&comip_clk_funcs);

	for (clkp = ddr_pwr_clks;
		clkp < ddr_pwr_clks + ARRAY_SIZE(ddr_pwr_clks); clkp++) {
		(*clkp)->owner = THIS_MODULE;
		clk_register(*clkp);
	}

	for (clkp = ap_pwr_clks;
		clkp < ap_pwr_clks + ARRAY_SIZE(ap_pwr_clks); clkp++) {
		(*clkp)->owner = THIS_MODULE;
		clk_register(*clkp);
	}

#if 0
{
	unsigned int val;
	void __iomem *clk_reg;

	/* Close unused clocks on system clock bus 0. */
	clk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN0);
	val = readl(clk_reg);
	val |= ((1 << 24) | (1 << 25) | (1 << 26) | (1 << 27) | (1 << 30) | (1 << 31));
	val &= ~((1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 14) | (1 << 15));
	writel(val, clk_reg);

	/* Close unused clocks on system clock bus 1. */
	clk_reg = (void __iomem *)io_p2v(AP_PWR_SYSCLK_EN1);
	val = readl(clk_reg);
	val |= ((1 << 21) | (1 << 22));
	val &= ~((1 << 8) | (1 << 9));
	writel(val, clk_reg);
}
#endif
}

