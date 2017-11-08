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
*  1.0.0		2013-12-20	xuxuefeng	created
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

#include <plat/clock.h>	
#include <plat/hardware.h>

#include "clock.h"
#ifdef CONFIG_USB_COMIP_HSIC
atomic_t usb_clk_cnt = ATOMIC_INIT(0);

static int clk_enable_usb(struct clk * clk)
{
	unsigned int val;

	if (!atomic_read(&usb_clk_cnt)) {
		
		if (clk->mclk_reg != 0) {
			val = (1 << clk->mclk_bit) | (1 << clk->mclk_we_bit);
			writel(val, clk->mclk_reg);
		} else if (clk->grclk_reg != 0) {
			val = (clk->grclk_mask << clk->grclk_we_bit)
				| (clk->grclk_val << clk->grclk_bit);
			writel(val, clk->grclk_reg);
		}

	}

	
	if (clk->ifclk_reg != 0) {
		val = (1 << clk->ifclk_bit) | (1 << clk->ifclk_we_bit);
		writel(val, clk->ifclk_reg);
	}

	if (clk->busclk_reg != 0) {
		val = (1 << clk->busclk_bit) | (1 << clk->busclk_we_bit);
		writel(val, clk->busclk_reg);
	}

	atomic_inc(&usb_clk_cnt);
	
	return 0;
}

static void clk_disable_usb(struct clk * clk)
{
	unsigned int val;

	if (atomic_dec_and_test(&usb_clk_cnt)) {

		if (clk->mclk_reg != 0) {
			val = (1 << clk->mclk_we_bit);
			writel(val, clk->mclk_reg);
		} else if (clk->grclk_reg != 0) {
			val = (clk->grclk_mask << clk->grclk_we_bit);
			writel(val, clk->grclk_reg);
		}
	}

	if (clk->ifclk_reg != 0) {
		val = (1 << clk->ifclk_we_bit);
		writel(val, clk->ifclk_reg);
	}
	if (clk->busclk_reg != 0) {
		val = (1 << clk->busclk_we_bit);
		writel(val, clk->busclk_reg);
	}
}
#endif
static int clk_enable_generic(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		val = (1 << clk->mclk_bit) | (1 << clk->mclk_we_bit);
		writel(val, clk->mclk_reg);
	} else if (clk->grclk_reg != 0) {
		val = (clk->grclk_mask << clk->grclk_we_bit)
			| (clk->grclk_val << clk->grclk_bit);
		writel(val, clk->grclk_reg);
	}

	if (clk->ifclk_reg != 0) {
		val = (1 << clk->ifclk_bit) | (1 << clk->ifclk_we_bit);
		writel(val, clk->ifclk_reg);
	}

	if (clk->busclk_reg != 0) {
		val = (1 << clk->busclk_bit) | (1 << clk->busclk_we_bit);
		writel(val, clk->busclk_reg);
	}

	return 0;
}

static void clk_disable_generic(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		val = (1 << clk->mclk_we_bit);
		writel(val, clk->mclk_reg);
	} else if (clk->grclk_reg != 0) {
		val = (clk->grclk_mask << clk->grclk_we_bit);
		writel(val, clk->grclk_reg);
	}

	if (clk->ifclk_reg != 0) {
		val = (1 << clk->ifclk_we_bit);
		writel(val, clk->ifclk_reg);
	}

	if (clk->busclk_reg != 0) {
		val = (1 << clk->busclk_we_bit);
		writel(val, clk->busclk_reg);
	}
}

static int clk_enable_nowe(struct clk *clk)
{
	unsigned int val;

	val = readl(clk->mclk_reg);
	val |= 1 << clk->mclk_bit;
	writel(val, clk->mclk_reg);

	return 0;
}

static void clk_disable_nowe(struct clk *clk)
{
	unsigned int val;

	val = readl(clk->mclk_reg);
	val &= ~(1 << clk->mclk_bit);
	writel(val, clk->mclk_reg);
}

static void clk_top_bus_init(struct clk *clk)
{
	unsigned int div;
	unsigned int val;
	unsigned int mul;

	val = readl(clk->divclk_reg);
	div = (val >> 0) & 0xF;
	mul = (val >> 8) & 0xF;

	clk->rate = clk->parent->rate / (div + 1) * (mul + 1) / 16;
}

static int clk_top_bus_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	unsigned long cal_rate;
	unsigned int div;
	unsigned int mul;
	unsigned int val;
	unsigned int gap;

	if (rate > parent_rate)
		return -EINVAL;

	gap = rate;	/*for calculate the close rate*/

	for (mul = 0; mul <= 0xf; mul++) {
		for (div = 0; div <= 0xf; div++) {
			cal_rate = parent_rate  / (div + 1) / 16 * (mul + 1);
			if (cal_rate == rate) {
				val = (0xf << 16) | (mul << 8) | (0xf << 24) | (div << 0);
				writel(val, clk->divclk_reg);
				return 0;
			} else {
				if (abs(rate - cal_rate) < gap) {
					val = (0xf << 16) | (mul << 8) | (0xf << 24) | (div << 0);
					gap = abs(rate - cal_rate);
					clk->rate = cal_rate;
				}
			}
		}
	}

	writel(val, clk->divclk_reg);

	printk("set %s rate %ld\n",  clk->name, clk->rate);

	return -EINVAL;

}

static void sdiv_init(struct clk *clk)
{
	unsigned int div;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;
	clk->rate = clk->parent->rate / (div + 1);
}

static void sdiv_g16plus1_init(struct clk *clk)
{
	unsigned int div;
	unsigned int grp;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;

	clk->rate = clk->parent->rate / (div + 1);

	val = readl(clk->grclk_reg);
	grp = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = grp;

	clk->rate = clk->rate / 16 * (grp + 1);
}

static void sa7_init(struct clk *clk)
{
	unsigned int div;
	unsigned int grp;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;

	clk->rate = clk->parent->rate / (div + 1);

	val = readl(clk->grclk_reg);
	grp = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = grp;

	clk->rate = clk->rate /16 * (grp + 1) ;
}

static int sdiv_g16plus1_set_rate_noen(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	unsigned long cal_rate;
	unsigned int div;
	unsigned int grp;
	unsigned int val;
	unsigned int gap;

	if (rate > parent_rate)
		return -EINVAL;

	gap = rate;	/*for calculate the close rate*/

	for (grp = 0; grp <= clk->grclk_mask; grp++) {
		for (div = 1; div <= clk->divclk_mask; div++) {
			cal_rate = parent_rate / (div + 1) / 16 * (grp + 1);
			if (cal_rate == rate) {
				clk->divclk_val = div;
				clk->grclk_val = grp;
				clk->rate = rate;
				val = (clk->divclk_mask << clk->divclk_we_bit) | (clk->divclk_val << clk->divclk_bit)
					| (clk->grclk_mask << clk->grclk_we_bit) | (clk->grclk_val << clk->grclk_bit);

				writel(val, clk->divclk_reg);
				udelay(2);
				return  0;
			} else {
				if (abs(rate - cal_rate) < gap) {
					clk->divclk_val = div;
					clk->grclk_val = grp;
					clk->rate = cal_rate;
					val = (clk->divclk_mask << clk->divclk_we_bit) | (clk->divclk_val << clk->divclk_bit)
						| (clk->grclk_mask << clk->grclk_we_bit) | (clk->grclk_val << clk->grclk_bit);			
					gap = abs(rate - cal_rate);
				}
			}
		}
	}

	writel(val, clk->divclk_reg);
	udelay(2);

	printk("set %s rate %ld\n",  clk->name, clk->rate);

	return  -EINVAL;

}

static void g8plus1noen_init(struct clk *clk)
{
	unsigned int val;
	unsigned int div;

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;
	clk->rate = clk->parent->rate * (div + 1) / 8;
}

static int sdiv_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	unsigned long cal_rate;
	unsigned long last_rate;
	unsigned int div;
	unsigned int last_div;
	unsigned int val;
	unsigned int en = 0;

	if (rate > parent_rate)
		return -EINVAL;

	/*for sdiv disable clk first*/
	if(clk->mclk_reg) {
		val = readl(clk->mclk_reg);
		en = (val >> clk->mclk_bit) & 0x1;
		if(en) {
			if(clk->disable)
				clk->disable(clk);

			udelay(1);
		}
	}

	div = parent_rate / rate - 1;

	if (div < 1 || div > clk->divclk_mask)
		return -EINVAL;

	last_rate = parent_rate / (div + 1);
	last_div = div;

	if (rate != last_rate) {
		div++;
		cal_rate = parent_rate / (div + 1);
		if (abs(rate - cal_rate) < abs(rate - last_rate)) {
			clk->divclk_val = div;
			clk->rate = cal_rate;
		} else {
			clk->divclk_val = last_div;
			clk->rate = last_rate;
		}

		printk("set %s rate %ld\n",  clk->name, clk->rate);

	} else {
		clk->divclk_val = last_div;
		clk->rate = last_rate;
	}

	if (clk->divclk_we_bit)
		val = (clk->divclk_mask << clk->divclk_we_bit)
		| (clk->divclk_val << clk->divclk_bit);
	else
		val = clk->divclk_val << clk->divclk_bit;

	writel(val, clk->divclk_reg);

	/*restore enable status*/
	if(clk->mclk_reg) {
		if(en) {
			if(clk->enable)
				clk->enable(clk);

		}
	}

	return 0;
}

static void fdiv_init_by_table(struct clk *clk)
{
	unsigned int mul;
	unsigned int div;
	unsigned int val;

	if (clk->divclk_reg != 0) {
		val = readl(clk->divclk_reg);
		mul = (val >> 16) & 0xffff;
		div = val & 0xffff;
		clk->rate = clk->parent->rate / div / 2 * mul;
	}

	if (clk->grclk_reg != 0) {
		val = readl(clk->grclk_reg);
		clk->grclk_val = ((val >> clk->grclk_bit) & clk->grclk_mask) + 1;
		clk->rate = (clk->rate * clk->grclk_val / 8);
	}
}

static int fdiv_set_rate_by_table(struct clk *clk,unsigned long rate)
{
	struct clk_table *tbl= (struct clk_table *)clk->cust;
	unsigned long parent_rate = clk->parent->rate;
	unsigned int find = 0;
	unsigned int val;
	unsigned int en = 0;

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

	/*for fdiv disable clk first*/
	if(clk->mclk_reg) {
		val = readl(clk->mclk_reg);
		en = (val >> clk->mclk_bit) & 0x1;
		if(en) {
			if(clk->disable)
				clk->disable(clk);

			udelay(1);
		}
	}

	clk->rate = rate;
	if (clk->divclk_reg != 0)
		writel(((tbl->mul << 16) | tbl->div), clk->divclk_reg);

	if (clk->grclk_reg != 0) {
		clk->rate = (clk->rate * clk->grclk_val / 8);
		val = (clk->grclk_val - 1) << clk->grclk_bit;
		val |= (clk->grclk_mask << clk->grclk_we_bit);
		writel(val, clk->grclk_reg);
	}

	/*restore enable status*/
	if(clk->mclk_reg) {
		if(en) {
			if(clk->enable)
				clk->enable(clk);

		}
	}

	return 0;
}
static void clk_pllx_init(struct clk *clk)
{
	unsigned long parent_rate;
	unsigned int val;
	unsigned int refdiv;
	unsigned int fbdiv;
	unsigned int postdiv1;
	unsigned int postdiv2;

	parent_rate = clk->parent->rate;
	val = readl(clk->divclk_reg);
	refdiv = val & 0x3f;
	fbdiv = (val >> 8) & 0xfff;
	postdiv1 = (val >> 20) & 0x7;
	postdiv2 = (val >> 24) & 0x7;

	clk->rate = parent_rate / refdiv / postdiv1 / postdiv2 * fbdiv;

	printk("%s rate:%ld\n", clk->name, clk->rate);
}

static void g16plus0_init(struct clk *clk)
{
	unsigned int gr;
	unsigned int val;

	val = readl(clk->grclk_reg);
	gr = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = gr;
	clk->rate = clk->parent->rate / 16 * gr;
	printk("A7 rate: %ld, gr:%d\n", clk->rate, gr);
}


static int ha7_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int gr, cur_gr;
	unsigned int val;
	unsigned int dvf;

	if (rate > clk->parent->rate)
		return -EINVAL;

	gr = rate / (clk->parent->rate / 16);
	if (gr > 16)
		return -EINVAL;

	cur_gr = readl(clk->grclk_reg);
	cur_gr &= 0x1f;

	if ((cur_gr < 9) && (gr >= 9)) {
		dvf = readl(io_p2v(DDR_PWR_PMU_CTL));
		dvf = (dvf >> 12) & 0x3;
		if (dvf < 3) {
			return -EINVAL;
		}
	}

	if(gr == cur_gr) {
		return 0;
	} else if (gr > cur_gr) {
		/* Step rasie freq */
		if ((gr - cur_gr) % 2)
			cur_gr += 1;
		else
			cur_gr += 2;

		while (cur_gr <= gr) {
			val = (clk->grclk_mask << clk->grclk_we_bit) | (cur_gr << clk->grclk_bit);
			writel(val, clk->grclk_reg);
			udelay(2);
			if (unlikely(cur_gr == gr))
				break;
			cur_gr += 2;
		}
	} else if (gr < cur_gr) {
		/* Step low freq */
		if ((cur_gr - gr) % 2)
			cur_gr -= 1;
		else
			cur_gr -= 2;

		while (cur_gr >= gr) {
			val = (clk->grclk_mask << clk->grclk_we_bit) | (cur_gr << clk->grclk_bit);
			writel(val, clk->grclk_reg);
			udelay(2);
			if (unlikely(cur_gr == gr))
				break;
			cur_gr -= 2;
		}
	}

	clk->grclk_val = gr;
	clk->rate = clk->parent->rate / 16 * gr;

	if (clk->rate % 1000)
		clk->rate += (1000 - clk->rate % 1000);

	return 0;
}

static int sa7_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	unsigned int div, cur_grp, grp, val;

	if (rate > parent_rate)
		return -EINVAL;

	div = readl(clk->divclk_reg);
	cur_grp = (div >> 8) & 0xf;
	div = (div >> 4) & 0x7;
	grp = rate / (parent_rate / ((div + 1) * 16)) - 1;
	if(grp > 0xf)
		return -EINVAL;
	if (grp == cur_grp) {
		return 0;
	} else if (grp > cur_grp) {
		/* Step rasie freq */
		if ((grp - cur_grp) % 2)
			cur_grp += 1;
		else
			cur_grp += 2;

		while (cur_grp <= grp) {
			val = (clk->grclk_mask << clk->grclk_we_bit) | (cur_grp << clk->grclk_bit);
			writel(val, clk->grclk_reg);
			udelay(2);
			if (unlikely(cur_grp == grp))
				break;
			cur_grp += 2;
		}
	} else if (grp < cur_grp) {
		/* Step low freq */
		if ((cur_grp - grp) % 2)
			cur_grp -= 1;
		else
			cur_grp -= 2;

		while (cur_grp >= grp) {
			val = (clk->grclk_mask << clk->grclk_we_bit) | (cur_grp << clk->grclk_bit);
			writel(val, clk->grclk_reg);
			udelay(2);
			if (unlikely(cur_grp == grp))
				break;
			cur_grp -= 2;
		}
	}

	clk->rate = clk->parent->rate / (div + 1)  / 16 * (grp + 1);
	if (clk->rate % 1000)
		clk->rate += (1000 - clk->rate % 1000);

	return 0;
}

static void gdiv_init(struct clk *clk)
{
	unsigned int gr;
	unsigned int val;

	val = readl(clk->grclk_reg);
	gr = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = gr;
	clk->rate = clk->parent->rate / 8 * (gr + 1);
}

static int gdiv_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int grp;
	unsigned int val;

	if (rate > clk->parent->rate)
		return -EINVAL;

	grp = rate / (clk->parent->rate / 8) - 1;
	clk->grclk_val = grp;
	clk->rate = clk->parent->rate / 8 * (grp + 1);
	val = (clk->grclk_mask << clk->grclk_we_bit) | (grp << clk->grclk_bit);
	writel(val, clk->grclk_reg);

	return 0;
}
static void clk_memctl_phy_init(struct clk *clk)
{
	unsigned int div;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;
	clk->rate = clk->parent->rate / (div + 1);
}

static int clk_memctl_phy_enable(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		val = (1 << clk->mclk_bit) | (1 << clk->mclk_we_bit);
		writel(val, clk->mclk_reg);
	}

	if (clk->ifclk_reg != 0) {
		val = (1 << clk->ifclk_bit) | (1 << clk->ifclk_we_bit);
		writel(val, clk->ifclk_reg);
	}

	return 0;
}

static void clk_memctl_phy_disable(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		val = (1 << clk->mclk_we_bit);
		writel(val, clk->mclk_reg);
	}

	if (clk->ifclk_reg != 0) {
		val = 1 << clk->ifclk_we_bit;
		writel(val, clk->ifclk_reg);
	}

}

static int clk_memctl_phy_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	unsigned long cal_rate;
	unsigned int div;
	unsigned int val;

	if (rate > parent_rate)
		return -EINVAL;

	for (div = 1; div <= clk->divclk_mask; div += 2) {
		cal_rate = parent_rate / (div + 1);
		if (cal_rate == rate) {
			clk->divclk_val = div;
			clk->rate = rate;
			val = div << clk->divclk_bit;
			writel(val, clk->divclk_reg);
			return 0;
		}
	}

	return -EINVAL;
}

static void clk_clkout_init(struct clk *clk)
{
	unsigned int val;

	val = readl(clk->ifclk_reg);
	clk->parent_id = (val >> clk->ifclk_bit) & 0x7;
}

static int clk_clkout_enable(struct clk *clk)
{
	unsigned int val;

	val = readl(clk->ifclk_reg);
	val &= ~(0x7 << clk->ifclk_bit);
	val |= (clk->parent_id << clk->ifclk_bit);
	writel(val, clk->ifclk_reg);

	return 0;
}

static void clk_clkout_disable(struct clk *clk)
{
	unsigned int val;

	val = readl(clk->ifclk_reg);
	val &= ~(0x7 << clk->ifclk_bit);
	val |= (6 << clk->ifclk_bit);
	writel(val, clk->ifclk_reg);
}

static int clk_clkout_set_rate(struct clk *clk, unsigned long rate)
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

	if (parent == &pll0_div13_clk)
		index = 0;
	else if (parent == &pll1_mclk)
		index = 1;
	else if (parent == &pll1_mclk)
		index = 2;
	else if (parent == &osc_clk)
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

static int clk_clkout_set_parent(struct clk *clk, struct clk *parent)
{
	if (!parent)
		return -EINVAL;

	clk->rate = 0;
	clk->parent = parent;

	return 0;
}

static void clk_timer_init(struct clk *clk)
{
	unsigned int val;
	unsigned int *mask = (unsigned int *)clk->cust;

	val = readl(clk->mclk_reg);
	clk->parent_id = (val >> clk->mclk_bit) & 0x7;
	*mask = 0;
}

static int clk_timer_enable(struct clk *clk)
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

static void clk_timer_disable(struct clk *clk)
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

static int clk_timer_set_rate(struct clk *clk, unsigned long rate)
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

	if (parent == &pll0_div13_clk)
		index = 0;
	else if (parent == &pll1_mclk)
		index = 1;
	else if (parent == &pll1_mclk)
		index = 2;
	else if (parent == &osc_clk) {
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

static int clk_timer_set_parent(struct clk *clk, struct clk *parent)
{
	if (!parent)
		return -EINVAL;

	clk->rate = 0;
	clk->parent = parent;

	return 0;
}
static void sdiv_phdiv_init(struct clk *clk)
{
	unsigned int div0;
	unsigned int div1;
	unsigned int val;

	val = readl(clk->divclk_reg);
	div1 = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div1;
	clk->rate = clk->parent->rate / (2 * (div1 + 1));

	val = readl(clk->grclk_reg);
	div0 = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = div0 + 1;
	clk->rate = clk->rate / (div0 + 1);
}
static int sdiv_phdiv_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int val;
	unsigned long cal_rate;
	unsigned long parent_rate = clk->parent->rate;
	int div0;
	int div1;
	unsigned int en = 0;
	unsigned int gap;

	if (rate > clk->parent->rate)
		return -EINVAL;

	/*for sdiv disable clk first*/
	if(clk->mclk_reg) {
		val = readl(clk->mclk_reg);
		en = (val >> clk->mclk_bit) & 0x1;
		if(en) {
			if(clk->disable)
				clk->disable(clk);
		}
	}

	gap = rate;	/*for calculate the close rate*/

	for (div0 = 1; div0 <= clk->divclk_mask; div0++ ) {
		for (div1 = 0; div1 <= clk->grclk_mask; div1++ ) {
				cal_rate = parent_rate / (div0 + 1) / (2 * (div1 + 1));
				if (cal_rate == rate) {
					clk->grclk_val = div0;
					clk->divclk_val = div1;
					clk->rate = rate;

					val = (clk->divclk_mask << clk->divclk_we_bit)
						| (clk->divclk_val << clk->divclk_bit);
					writel(val, clk->divclk_reg);
					val = (clk->grclk_mask << clk->grclk_we_bit)
						| (clk->grclk_val << clk->grclk_bit);
					writel(val, clk->grclk_reg);

					/*restore enable status*/
					if(clk->mclk_reg) {
						if(en) {
							if(clk->enable)
								clk->enable(clk);
						}
					}

					return 0;
				} else {
					if (abs(rate - cal_rate) < gap) {
						clk->grclk_val = div0;
						clk->divclk_val = div1;
						clk->rate = cal_rate;
						gap = abs(rate - cal_rate);
					}
				}
		}
	}

	val = (clk->divclk_mask << clk->divclk_we_bit)
		| (clk->divclk_val << clk->divclk_bit);
	writel(val, clk->divclk_reg);
	val = (clk->grclk_mask << clk->grclk_we_bit)
		| (clk->grclk_val << clk->grclk_bit);
	writel(val, clk->grclk_reg);

	/*restore enable status*/
	if(clk->mclk_reg) {
		if(en) {
			if(clk->enable)
				clk->enable(clk);
		}
	}

	printk("set %s rate %ld\n",  clk->name, clk->rate);

	return -EINVAL;
}

static void g8plus1_sdiv_init(struct clk *clk)
{
	unsigned int div;
	unsigned int grp;
	unsigned int val;

	val = readl(clk->grclk_reg);
	grp = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = grp;
	clk->rate = clk->parent->rate / 8 * (grp + 1);

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;

	clk->rate = clk->rate / (div + 1);
}

static int g8plus1_sdiv_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	unsigned long cal_rate;
	unsigned int div;
	unsigned int grp;
	unsigned int val;
	unsigned int en = 0;
	unsigned int gap;

	if (rate > parent_rate)
		return -EINVAL;

	/*for sdiv disable clk first*/
	if(clk->mclk_reg) {
		val = readl(clk->mclk_reg);
		en = (val >> clk->mclk_bit) & 0x1;
		if(en) {
			if(clk->disable)
				clk->disable(clk);
		}
	}

	gap = rate;	/*for calculate the close rate*/

	for (div = 1; div <= clk->divclk_mask; div++) {
		for (grp = 0; grp <= clk->grclk_mask; grp++) {
			cal_rate = parent_rate / (div + 1) / 8 * (grp + 1);
			if (cal_rate == rate) {
				clk->divclk_val = div;
				clk->grclk_val = grp;
				clk->rate = rate;
				val = (clk->divclk_mask << clk->divclk_we_bit)
					| (clk->divclk_val << clk->divclk_bit);
				writel(val, clk->divclk_reg);
				val = (clk->grclk_mask << clk->grclk_we_bit)
					| (clk->grclk_val << clk->grclk_bit);
				writel(val, clk->grclk_reg);

				/*restore enable status*/
				if(clk->mclk_reg) {
					if(en) {
						if(clk->enable)
							clk->enable(clk);
					}
				}

				return	0;
			} else {
				if (abs(rate - cal_rate) < gap) {
					clk->divclk_val = div;
					clk->grclk_val = grp;
					clk->rate = cal_rate;
					gap = abs(rate - cal_rate);
				}

			}
		}
	}

	val = (clk->divclk_mask << clk->divclk_we_bit)
		| (clk->divclk_val << clk->divclk_bit);
	writel(val, clk->divclk_reg);
	val = (clk->grclk_mask << clk->grclk_we_bit)
		| (clk->grclk_val << clk->grclk_bit);
	writel(val, clk->grclk_reg);

	/*restore enable status*/
	if(clk->mclk_reg) {
		if(en) {
			if(clk->enable)
				clk->enable(clk);
		}
	}

	printk("set %s rate %ld\n",  clk->name, clk->rate);

	return	-EINVAL;
}

static void g16plus1_sdiv_init(struct clk *clk)
{
	unsigned int div;
	unsigned int grp;
	unsigned int val;

	val = readl(clk->grclk_reg);
	grp = (val >> clk->grclk_bit) & clk->grclk_mask;
	clk->grclk_val = grp;
	clk->rate = clk->parent->rate / 16 * (grp + 1);

	val = readl(clk->divclk_reg);
	div = (val >> clk->divclk_bit) & clk->divclk_mask;
	clk->divclk_val = div;

	clk->rate = clk->rate / (div + 1);
}

static int g16plus1_sdiv_set_rate_noen(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = clk->parent->rate;
	unsigned long cal_rate;
	unsigned int div;
	unsigned int grp;
	unsigned int val;
	unsigned int gap = 0;

	if (rate > parent_rate)
		return -EINVAL;

	gap = rate;	/*for calculate the close rate*/

	for (div = 1; div <= clk->divclk_mask; div++) {
		for (grp = 0; grp <= clk->grclk_mask; grp++) {
			cal_rate = parent_rate / (div + 1) / 16 * (grp + 1);
			if (cal_rate == rate) {
				clk->divclk_val = div;
				clk->grclk_val = grp;
				clk->rate = rate;
				val = (clk->divclk_mask << clk->divclk_we_bit)
					| (clk->divclk_val << clk->divclk_bit);
				val |= (clk->grclk_mask << clk->grclk_we_bit)
					| (clk->grclk_val << clk->grclk_bit);
				writel(val, clk->divclk_reg);

				return	0;
			} else {
				if (abs(rate - cal_rate) < gap) {
					clk->divclk_val = div;
					clk->grclk_val = grp;
					clk->rate = cal_rate;
					gap = abs(rate - cal_rate);
				}
			}
		}
	}

	val = (clk->divclk_mask << clk->divclk_we_bit)
		| (clk->divclk_val << clk->divclk_bit);
	val |= (clk->grclk_mask << clk->grclk_we_bit)
		| (clk->grclk_val << clk->grclk_bit);
	writel(val, clk->divclk_reg);

	return	-EINVAL;
}

static int mets_clk_enable(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		val = (1 << clk->mclk_bit) | (1 << clk->mclk_we_bit) | clk->divclk_val;
		writel(val, clk->mclk_reg);
	} 

	return 0;
}
static void mets_clk_disable(struct clk * clk)
{
	unsigned int val;

	if (clk->mclk_reg != 0) {
		val = (0 << clk->mclk_bit) | (1 << clk->mclk_we_bit) | clk->divclk_val;
		writel(val, clk->mclk_reg);
	} 
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
}

