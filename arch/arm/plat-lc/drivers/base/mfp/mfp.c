/*
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2010 LeadCoreTech Corp.
**
**	PURPOSE: This files contains the mfp config.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2011-11-22	lengyansong	created
**
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <plat/hardware.h>
#include <plat/mfp.h>

static unsigned long mfp_regs[] = {
	MUX_PIN_MUX0,			/* 0. */
	MUX_PIN_MUX1,			/* 1. */
	MUX_PIN_MUX2,			/* 2. */
	MUX_PIN_MUX3,			/* 3. */
	MUX_PIN_MUX4,			/* 4. */
	MUX_PIN_MUX5,			/* 5. */
	MUX_PIN_MUX6,			/* 6. */
	MUX_PIN_MUX7,			/* 7. */
	MUX_PIN_MUX8,			/* 8. */
	MUX_PIN_MUX9,			/* 9. */
	MUX_PIN_MUX10,			/* 10. */
	MUX_PIN_MUX11,			/* 11. */
	MUX_PIN_MUX12,			/* 12. */
	MUX_PIN_MUX13,			/* 13. */
	MUX_PIN_MUX14,			/* 14. */
	MUX_PIN_MUX15,			/* 15. */
};

static unsigned long mfp_pull_regs[] = {
	MUX_PIN_PULL_EN0,		/* 0. */
	MUX_PIN_PULL_EN1,		/* 1. */
	MUX_PIN_PULL_EN2,		/* 2. */
	MUX_PIN_PULL_EN3,		/* 3. */
	MUX_PIN_PULL_EN4,		/* 4. */
	MUX_PIN_PULL_EN5,		/* 5. */
	MUX_PIN_PULL_EN6,		/* 6. */
	MUX_PIN_PULL_EN7,		/* 7. */
	MUX_PIN_SDMMC0_PAD_CTRL,	/* 8. */
	MUX_PIN_SDMMC1_PAD_CTRL,	/* 9. */
	MUX_PIN_SDMMC2_PAD_CTRL,	/* 10. */
	MUX_PIN_SDMMC3_PAD_CTRL,	/* 11. */
};

static DEFINE_SPINLOCK(mfp_lock);

int comip_mfp_af_read(mfp_pin id)
{
	return 0;
}

int comip_mfp_pull_down_read(mfp_pin id)
{
	return 0;
}

int comip_mfp_pull_up_read(mfp_pin id)
{
	return 0;
}

int comip_mfp_config(mfp_pin id, mfp_pin_mode mode)
{
	struct mfp *mfp;
	unsigned long flags;
	unsigned long reg;
	unsigned int reg_id;
	unsigned int reg_bit;
	unsigned int val;

	if (unlikely((id >= MFP_PIN_MAX)
		|| (mode >= MFP_PIN_MODE_MAX)))
		return -EINVAL;

	mfp = (struct mfp *)&mfp_pin_list;
	if (unlikely(mfp[id].flags & MFP_PIN_GPIO_ONLY))
		return 0;

	if (unlikely(mode == mfp[id].val))
		return -EINVAL;

	if (unlikely((mode == MFP_PIN_MODE_GPIO)
		&& (mfp[id].val >= MFP_PIN_MODE_MAX)))
		return -EINVAL;

	reg_id = mfp[id].mux / 16;
	reg_bit = (mfp[id].mux % 16) << 1;
	if (unlikely(reg_id > ARRAY_SIZE(mfp_regs)))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	reg = mfp_regs[reg_id];
	val = readl(io_p2v(reg));
	val &= ~(0x3 << reg_bit);
	if (mode == MFP_PIN_MODE_GPIO)
		val |= (mfp[id].val << reg_bit);
	else
		val |= (mode << reg_bit);
	writel(val, io_p2v(reg));

	spin_unlock_irqrestore(&mfp_lock, flags);

	return 0;
}

int comip_mfp_config_pull(mfp_pull_id id, mfp_pull_mode mode)
{
	struct mfp_pull *pull;
	unsigned long flags;
	unsigned long reg;
	unsigned int val;

	if (unlikely((id >= mfp_pull_list_size)
		|| (mode > MFP_PULL_MODE_MAX)))
		return -EINVAL;

	pull = &mfp_pull_list[id];
	if (unlikely((pull->id != id) || !(pull->caps & (1 << mode))))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	if ((mode != MFP_PULL_DISABLE)
		&& ((pull->caps & MFP_PULL_ALL_CAP) == MFP_PULL_ALL_CAP)) {
		reg = mfp_pull_regs[pull->pull_reg_id];
		val = readl(io_p2v(reg));
		if (mode == MFP_PULL_UP)
			val &= ~(1 << pull->pull_bit);
		else
			val |= (1 << pull->pull_bit);
		writel(val, io_p2v(reg));
	}

	reg = mfp_pull_regs[pull->en_reg_id];
	val = readl(io_p2v(reg));
	if (mode == MFP_PULL_DISABLE)
		val |= (1 << pull->en_bit);
	else
		val &= ~(1 << pull->en_bit);
	writel(val, io_p2v(reg));

	spin_unlock_irqrestore(&mfp_lock, flags);

	return 0;
}

int comip_mfp_config_array(struct mfp_pin_cfg *pin, int size)
{
	int i;

	if (unlikely(!pin || !size))
		return -EINVAL;

	for (i = 0; i < size; i++)
		comip_mfp_config(pin[i].id, pin[i].mode);

	return 0;
}

int comip_mfp_config_pull_array(struct mfp_pull_cfg *pull, int size)
{
	int i;

	if (unlikely(!pull || !size))
		return -EINVAL;

	for (i = 0; i < size; i++)
		comip_mfp_config_pull(pull[i].id, pull[i].mode);

	return 0;
}

int comip_pvdd_vol_ctrl_config(int reg, int volt)
{

	return 0;
}

EXPORT_SYMBOL(comip_pvdd_vol_ctrl_config);
EXPORT_SYMBOL(comip_mfp_config);
EXPORT_SYMBOL(comip_mfp_config_pull);
EXPORT_SYMBOL(comip_mfp_config_array);
EXPORT_SYMBOL(comip_mfp_config_pull_array);
