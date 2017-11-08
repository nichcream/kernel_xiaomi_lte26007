/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the pmic driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2012-03-06	zouqiao		created
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <plat/comip-battery.h>
#include <plat/comip-pmic.h>

static struct pmic_ops *g_pmic_ops = NULL;

/*****************************************************************************
 *			Operation of PMIC					 *
 *****************************************************************************/
static int pmic_ops_check(void)
{
	if (!g_pmic_ops) {
		printk(KERN_WARNING "No pmic_ops registered!\n");
		return -EINVAL;
	} else
		return 0;
}

int pmic_ops_set(struct pmic_ops *ops)
{
	if (g_pmic_ops != NULL) {
		printk(KERN_ERR "set pmic_ops when pmic_ops is not NULL\n");
		return -EFAULT;
	}

	g_pmic_ops = ops;
	INIT_LIST_HEAD(&g_pmic_ops->list);
	spin_lock_init(&g_pmic_ops->cb_lock);
	INIT_LIST_HEAD(&g_pmic_ops->ep_list);
	spin_lock_init(&g_pmic_ops->ep_lock);

	return 0;
}
EXPORT_SYMBOL(pmic_ops_set);

int pmic_event_handle(int event, void* pargs)
{
	int ret;
	unsigned long flags;
	struct pmic_callback *pmic_cb;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&g_pmic_ops->cb_lock, flags);
	list_for_each_entry(pmic_cb, &g_pmic_ops->list, list) {
		spin_unlock_irqrestore(&g_pmic_ops->cb_lock, flags);
		if ((pmic_cb->event == event) && (pmic_cb->func))
			pmic_cb->func(event, pmic_cb->puser, pargs);
		spin_lock_irqsave(&g_pmic_ops->cb_lock, flags);
	}
	spin_unlock_irqrestore(&g_pmic_ops->cb_lock, flags);

	return 0;
}
EXPORT_SYMBOL(pmic_event_handle);

const char *pmic_name_get(void)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return NULL;

	return g_pmic_ops->name;
}
EXPORT_SYMBOL(pmic_name_get);

/* Called by external power drivers. */
int pmic_ext_power_register(struct pmic_ext_power *ep)
{
	int ret;
	unsigned long flags;
	struct pmic_ext_power *ext_power;

	might_sleep();

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	ext_power = kzalloc(sizeof(*ext_power), GFP_KERNEL);
	if (!ext_power)
		return -ENOMEM;

	INIT_LIST_HEAD(&ext_power->list);
	ext_power->module = ep->module;
	ext_power->voltage_get = ep->voltage_get;
	ext_power->voltage_set = ep->voltage_set;

	spin_lock_irqsave(&g_pmic_ops->ep_lock, flags);
	list_add(&ext_power->list, &g_pmic_ops->ep_list);
	spin_unlock_irqrestore(&g_pmic_ops->ep_lock, flags);

	return 0;
}
EXPORT_SYMBOL(pmic_ext_power_register);

int pmic_ext_power_unregister(struct pmic_ext_power *ep)
{
	unsigned long flags;
	struct pmic_ext_power *ext_power, *next;

	spin_lock_irqsave(&g_pmic_ops->ep_lock, flags);
	list_for_each_entry_safe(ext_power, next, &g_pmic_ops->ep_list, list) {
		if (ext_power->module == ep->module) {
			list_del_init(&ext_power->list);
			kfree(ext_power);
		}
	}
	spin_unlock_irqrestore(&g_pmic_ops->ep_lock, flags);

	return 0;
}
EXPORT_SYMBOL(pmic_ext_power_unregister);

/* Register pmic callback */
int pmic_callback_register(int event, void *puser, void (*func)(int event, void *puser, void* pargs))
{
	int ret;
	unsigned long flags;
	struct pmic_callback *pmic_cb;

	might_sleep();

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	pmic_cb = kzalloc(sizeof(*pmic_cb), GFP_KERNEL);
	if (!pmic_cb)
		return -ENOMEM;

	INIT_LIST_HEAD(&pmic_cb->list);
	pmic_cb->event = event;
	pmic_cb->puser = puser;
	pmic_cb->func = func;

	spin_lock_irqsave(&g_pmic_ops->cb_lock, flags);
	list_add(&pmic_cb->list, &g_pmic_ops->list);
	spin_unlock_irqrestore(&g_pmic_ops->cb_lock, flags);

	return 0;
}
EXPORT_SYMBOL(pmic_callback_register);

/* Unregister pmic callback */
int pmic_callback_unregister(int event)
{
	unsigned long flags;
	struct pmic_callback *pmic_cb, *next;

	spin_lock_irqsave(&g_pmic_ops->cb_lock, flags);
	list_for_each_entry_safe(pmic_cb, next, &g_pmic_ops->list, list) {
		if (pmic_cb->event == event) {
			list_del_init(&pmic_cb->list);
			kfree(pmic_cb);
		}
	}
	spin_unlock_irqrestore(&g_pmic_ops->cb_lock, flags);

	return 0;
}
EXPORT_SYMBOL(pmic_callback_unregister);

int pmic_callback_mask(int event)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->event_mask)
		return g_pmic_ops->event_mask(event);

	return 0;
}
EXPORT_SYMBOL(pmic_callback_mask);

int pmic_callback_unmask(int event)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->event_unmask)
		return g_pmic_ops->event_unmask(event);

	return 0;
}
EXPORT_SYMBOL(pmic_callback_unmask);

int pmic_reg_write(u16 reg, u16 val)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->reg_write)
		return g_pmic_ops->reg_write(reg, val);

	return 0;
}
EXPORT_SYMBOL(pmic_reg_write);

int pmic_reg_read(u16 reg, u16 *pval)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->reg_read)
		return g_pmic_ops->reg_read(reg, pval);

	return 0;
}
EXPORT_SYMBOL(pmic_reg_read);

int pmic_voltage_get(u8 module, u8 param, int *pmv)
{
	struct pmic_ext_power *ext_power;
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	list_for_each_entry(ext_power, &g_pmic_ops->ep_list, list) {
		if ((ext_power->module == module) && (ext_power->voltage_get)) {
			return ext_power->voltage_get(module, param, pmv);
		}
	}

	if (g_pmic_ops->voltage_get)
		return g_pmic_ops->voltage_get(module, param, pmv);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_voltage_get);

int pmic_voltage_set(u8 module, u8 param, int mv)
{
	struct pmic_ext_power *ext_power;
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	list_for_each_entry(ext_power, &g_pmic_ops->ep_list, list) {
		if ((ext_power->module == module) && (ext_power->voltage_set)) {
			return ext_power->voltage_set(module, param, mv);
		}
	}

	if (g_pmic_ops->voltage_set)
		return g_pmic_ops->voltage_set(module, param, mv);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_voltage_set);

int pmic_rtc_time_get(struct rtc_time *tm)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->rtc_time_get)
		return g_pmic_ops->rtc_time_get(tm);
	else
		return -EINVAL;

}
EXPORT_SYMBOL(pmic_rtc_time_get);

int pmic_rtc_time_set(struct rtc_time *tm)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->rtc_time_set)
		return g_pmic_ops->rtc_time_set(tm);
	else
		return -EINVAL;

}
EXPORT_SYMBOL(pmic_rtc_time_set);

int pmic_rtc_alarm_get(u8 id, struct rtc_wkalrm *alrm)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->rtc_alarm_get)
		return g_pmic_ops->rtc_alarm_get(id, alrm);
	else
		return -EINVAL;

}
EXPORT_SYMBOL(pmic_rtc_alarm_get);

int pmic_rtc_alarm_set(u8 id, struct rtc_wkalrm *alrm)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->rtc_alarm_set)
		return g_pmic_ops->rtc_alarm_set(id, alrm);
	else
		return -EINVAL;

}
EXPORT_SYMBOL(pmic_rtc_alarm_set);

int pmic_comp_polarity_set(u8 id, u8 pol)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->comp_polarity_set)
		return g_pmic_ops->comp_polarity_set(id, pol);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_comp_polarity_set);

int pmic_comp_state_get(u8 id)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->comp_state_get)
		return g_pmic_ops->comp_state_get(id);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_comp_state_get);

int pmic_reboot_type_set(u8 type)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->reboot_type_set)
		return g_pmic_ops->reboot_type_set(type);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_reboot_type_set);

int pmic_power_key_state_get(void)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->power_key_state_get)
		return g_pmic_ops->power_key_state_get();
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_power_key_state_get);

int pmic_vchg_input_state_get(void)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->vchg_input_state_get)
		return g_pmic_ops->vchg_input_state_get();
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_vchg_input_state_get);

int pmic_power_ctrl_set(u8 module, u8 param, u8 ctrl)
{
	int ret;

	ret = pmic_ops_check();
	if (ret < 0)
		return ret;

	if (g_pmic_ops->power_ctrl_set)
		return g_pmic_ops->power_ctrl_set(module, param, ctrl);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(pmic_power_ctrl_set);

struct comip_android_battery_info *battery_property = NULL;
int comip_battery_property_register(struct comip_android_battery_info *di)
{
    if(battery_property != NULL){
        printk(KERN_ERR "init battery_property when battery_property is not NULL\n");
        return -EFAULT;
    }
    battery_property = di;
    return 0;
}
EXPORT_SYMBOL(comip_battery_property_register);

