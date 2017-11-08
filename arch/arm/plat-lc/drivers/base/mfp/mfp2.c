/*
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2014 LeadCoreTech Corp.
**
**	PURPOSE: This files contains the mfp config.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2013-12-09	zouqiao		created
**
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <plat/hardware.h>
#include <plat/mfp.h>

static DEFINE_SPINLOCK(mfp_lock);
int comip_mfp_af_read(mfp_pin id)
{
	unsigned long flags;
	unsigned long reg;
	unsigned int val;

	if (unlikely(id >= MFP_PIN_MAX))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	reg = MFP_REG(id);
	val = readl(io_p2v(reg));
	val = (val & (MFP_AF_MASK));

	spin_unlock_irqrestore(&mfp_lock, flags);

	return val;
}

int comip_mfp_pull_down_read(mfp_pin id)
{
	unsigned long flags;
	unsigned long reg;
	unsigned int val;

	if (unlikely(id >= MFP_PIN_MAX))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	reg = MFP_REG(id);
	val = readl(io_p2v(reg));
	val = (val & (MFP_PULL_DOWN_MASK));

	spin_unlock_irqrestore(&mfp_lock, flags);

	return !!val;
}

int comip_mfp_pull_up_read(mfp_pin id)
{
	unsigned long flags;
	unsigned long reg;
	unsigned int val;

	if (unlikely(id >= MFP_PIN_MAX))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	reg = MFP_REG(id);
	val = readl(io_p2v(reg));
	val = (val & (MFP_PULL_UP_MASK));

	spin_unlock_irqrestore(&mfp_lock, flags);

	return !!val;
}

int comip_mfp_config(mfp_pin id, mfp_pin_mode mode)
{
	unsigned long flags;
	unsigned long reg;
	unsigned int val;

	if (unlikely((id >= MFP_PIN_MAX)
		|| (mode >= MFP_PIN_MODE_MAX)))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	reg = MFP_REG(id);
	val = readl(io_p2v(reg));
	val = (val & (~MFP_AF_MASK)) | MFP_AF(mode);
	writel(val, io_p2v(reg));

	spin_unlock_irqrestore(&mfp_lock, flags);

	return 0;
}

int comip_mfp_config_pull(mfp_pin id, mfp_pull_mode mode)
{
	unsigned long flags;
	unsigned long reg;
	unsigned int val;

	if (unlikely((id >= MFP_PIN_MAX)
		|| (mode > MFP_PULL_MODE_MAX)))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	reg = MFP_REG(id);
	val = readl(io_p2v(reg));
	val = (val & (~MFP_PULL_MASK)) | MFP_PULL(mode);
	writel(val, io_p2v(reg));

	spin_unlock_irqrestore(&mfp_lock, flags);

	return 0;
}

int comip_mfp_config_ds(mfp_pin id, mfp_ds_mode mode)
{
	unsigned long flags;
	unsigned long reg;
	unsigned int val;

	if (unlikely((id >= MFP_PIN_MAX)
		|| (mode > MFP_DS_MODE_MAX)))
		return -EINVAL;

	spin_lock_irqsave(&mfp_lock, flags);

	reg = MFP_REG(id);
	val = readl(io_p2v(reg));
	val = (val & (~MFP_DS_MASK)) | MFP_DS(mode);
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

int comip_mfp_config_ds_array(struct mfp_ds_cfg *ds, int size)
{
	int i;

	if (unlikely(!ds || !size))
		return -EINVAL;

	for (i = 0; i < size; i++)
		comip_mfp_config_ds(ds[i].id, ds[i].mode);

	return 0;
}

int comip_pvdd_vol_ctrl_config(int reg, int volt)
{
	unsigned int val = 0;

	if(reg < MUXPIN_PVDD_VOL_CTRL_BASE){
		return -EINVAL;
	}

	if(volt <= PVDDx_VOL_CTRL_1_8V){
		val = 0;
	}else if((volt > PVDDx_VOL_CTRL_1_8V)&&(volt <= PVDDx_VOL_CTRL_2_5V)){
		val = 1;
	}else if((volt > PVDDx_VOL_CTRL_2_5V)&&(volt <= PVDDx_VOL_CTRL_3_3V)){
		val = 2;
	}else{
		val = 2;
	}

	__raw_writel(val, io_p2v(reg));

	return 0;
}

EXPORT_SYMBOL(comip_pvdd_vol_ctrl_config);
EXPORT_SYMBOL(comip_mfp_config);
EXPORT_SYMBOL(comip_mfp_config_pull);
EXPORT_SYMBOL(comip_mfp_config_ds);
EXPORT_SYMBOL(comip_mfp_config_array);
EXPORT_SYMBOL(comip_mfp_config_pull_array);
EXPORT_SYMBOL(comip_mfp_config_ds_array);
