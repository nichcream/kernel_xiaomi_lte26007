/*
* arch/arm/mach-comip/clock.c
*
* Use of source code is subject to the terms of the LeadCore license agreement under
* which you licensed source code. If you did not accept the terms of the license agreement,
* you are not authorized to use source code. For the terms of the license, please see the
* license agreement between you and LeadCore.
*
* Copyright (c) 2010-2019 LeadCoreTech Corp.
*
*  PURPOSE: This files contains the clock interface for drivers.
*
*  CHANGE HISTORY:
*
*  Version		Date			Author		 	Description
*  1.0.0		2011-12-31		wujingchao	 	created
*
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <plat/clock.h>

static LIST_HEAD(clocks);
static DEFINE_SPINLOCK(clocks_lock);
static DEFINE_SPINLOCK(clocks_list_lock);


static struct clk_functions *arch_clock;

struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *p, *clk = ERR_PTR(-ENOENT);
	int idno;

	if (dev == NULL)
		idno = -1;
	else
		idno = to_platform_device(dev)->id;

	spin_lock(&clocks_list_lock);

	list_for_each_entry(p, &clocks, node) {
		if ((p->id == idno) &&
		    strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			goto found;
		}
	}

	list_for_each_entry(p, &clocks, node) {
		if (strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}

found:
	spin_unlock(&clocks_list_lock);

	return clk;
}
EXPORT_SYMBOL(clk_get);

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret = 0;

	if (clk == NULL || IS_ERR(clk) || (clk->flag & CLK_ALWAYS_ENABLED))
		return -EINVAL;

	spin_lock_irqsave(&clocks_lock, flags);
	if (arch_clock->clk_enable)
		ret = arch_clock->clk_enable(clk);
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	if (clk == NULL || IS_ERR(clk) || (clk->flag & CLK_ALWAYS_ENABLED))
		return;

	WARN_ON(clk->user == 0);
	spin_lock_irqsave(&clocks_lock, flags);
	if (arch_clock->clk_disable)
		arch_clock->clk_disable(clk);
	spin_unlock_irqrestore(&clocks_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

int clk_get_usecount(struct clk *clk)
{
	unsigned long flags;
	int ret = 0;

	if (clk == NULL || IS_ERR(clk))
		return 0;

	spin_lock_irqsave(&clocks_lock, flags);
	ret = clk->user;
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_usecount);

unsigned long clk_get_rate(struct clk *clk)
{
	unsigned long flags;
	unsigned long ret = 0;

	if (clk == NULL || IS_ERR(clk))
		return 0;

	spin_lock_irqsave(&clocks_lock, flags);
	ret = clk->rate;
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_rate);

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);

}
EXPORT_SYMBOL(clk_put);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	long ret = 0;

	if (clk == NULL || IS_ERR(clk))
		return ret;

	spin_lock_irqsave(&clocks_lock, flags);
	if (arch_clock->clk_round_rate)
		ret = arch_clock->clk_round_rate(clk, rate);
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	int ret = -EINVAL;

	if (clk == NULL || IS_ERR(clk) || (clk->flag & CLK_RATE_FIXED))
		return ret;

	spin_lock_irqsave(&clocks_lock, flags);
	if (arch_clock->clk_set_rate)
		ret = arch_clock->clk_set_rate(clk, rate);
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	unsigned long flags;
	int ret = -EINVAL;

	if (clk == NULL || IS_ERR(clk) || parent == NULL || IS_ERR(parent))
		return ret;

	spin_lock_irqsave(&clocks_lock, flags);
	if (arch_clock->clk_set_parent)
		ret =  arch_clock->clk_set_parent(clk, parent);
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	unsigned long flags;
	struct clk * ret = NULL;

	if (clk == NULL || IS_ERR(clk))
		return ret;

	spin_lock_irqsave(&clocks_lock, flags);
	ret = clk->parent;
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_get_parent);

int clk_register(struct clk *clk)
{
	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;

	spin_lock(&clocks_list_lock);
	list_add(&clk->node, &clocks);
	if (clk->init)
		clk->init(clk);
	spin_unlock(&clocks_list_lock);

	if (clk->flag & CLK_INIT_DISABLED) {
		if (clk->disable)
			clk->disable(clk);
	} else if (clk->flag & (CLK_INIT_ENABLED | CLK_ALWAYS_ENABLED)) {
		if (clk->enable)
			clk->enable(clk);
	}

	/* If rate is already set, use it. Otherwise, default to parent rate. */
	if (!clk->rate && clk->parent) {
		if (clk->fix_div && (clk->flag & CLK_RATE_DIV_FIXED))
			clk->rate = clk->parent->rate / clk->fix_div;
		else
			clk->rate = clk->parent->rate;
	}

	return 0;
}
EXPORT_SYMBOL(clk_register);

void clk_unregister(struct clk *clk)
{
	if (clk == NULL || IS_ERR(clk))
		return;

	spin_lock(&clocks_list_lock);
	list_del(&clk->node);
	spin_unlock(&clocks_list_lock);
}
EXPORT_SYMBOL(clk_unregister);

int __init clk_init(struct clk_functions * custom_clocks)
{
	if (!custom_clocks) {
		printk(KERN_ERR "No custom clock functions registered\n");
		BUG();
	}

	arch_clock = custom_clocks;

	return 0;
}

