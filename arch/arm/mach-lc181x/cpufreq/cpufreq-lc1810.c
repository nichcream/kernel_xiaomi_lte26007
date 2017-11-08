/*
 * arch/arm/mach-comip/cpufreq.c
 *
 * Copyright (C) 2012 Leadcore, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>

#include <asm/smp_twd.h>
#include <asm/system.h>
#include <asm/setup.h>
#include <asm/sections.h>

#include <plat/hardware.h>
#include <plat/clock.h>
#include <plat/comip-pmic.h>
#include <plat/comip-setup.h>
#include <mach/suspend.h>

#define COMIP_CPUFREQ_DEBUG	0
#if COMIP_CPUFREQ_DEBUG
#define CPUFREQ_PR(fmt, args...)	printk(KERN_INFO fmt, ## args)
#else
#define CPUFREQ_PR(fmt, args...)
#endif

#define VOLT_STABLE_DURA_US 50
#define AP_PWR_DV_CTL AP_PWR_A9DV_CTL
#define VOLT_VALUE_ADDR io_p2v(AP_SEC_RAM_BASE)

#define VOLT_VALUE_IS_INVALID(val) \
		(((unsigned int)(val)) < 700 || ((unsigned int)(val)) > 2000)

/* core voltage(DVFS). */
static int vout_val_0 = 900;
static int vout_val_1 = 1100;
static int vout_val_2 = 1200;
static int vout_val_3 = 1200;

enum{
	VOLT_ADJ_LOW  = 1, /*dvfs(0)*/
	VOLT_ADJ_HIGH = 2, /*dvfs(1)*/
	VOLT_ADJ_DUAL = 3, /*dvfs(1) && dvfs(0)*/
} volt_adj_type;

enum{
	VOLT_TYPE_SW,
	VOLT_TYPE_TW,
};

enum{
	VOLT_LEVEL_0 = 0,
	VOLT_LEVEL_1 = 1,
	VOLT_LEVEL_2 = 2,
	VOLT_LEVEL_3 = 3,
};

enum{
	FREQ_LEVEL_HIGH = 0,
	FREQ_LEVEL_0    = 0,
	FREQ_LEVEL_1    = 1,
	FREQ_LEVEL_2    = 2,
	FREQ_LEVEL_3    = 3,
	FREQ_LEVEL_4    = 4,
	FREQ_LEVEL_5    = 5,
	FREQ_LEVEL_6    = 6,
	FREQ_LEVEL_7    = 7,
	FREQ_LEVEL_LOW  = 7,
	FREQ_LEVEL_MAX  = 8,
};

enum{
	FREQ_IN_IDLE,
	FREQ_IN_RUN,
};

struct voltage_table {
	unsigned int	index;	/* any */
	unsigned int	volt;	/* uV */
};

struct cpufreq_data_table {
	unsigned int freq; /* kHz */
	struct cpufreq_frequency_table *cluster_table;
	struct cpufreq_frequency_table *acp_table;
	struct voltage_table *volt_tables[4];
};

static struct voltage_table volt_table_1[] = {
	{ FREQ_LEVEL_0, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_1, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_2, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_3, VOLT_LEVEL_1 },

	{ FREQ_LEVEL_4, VOLT_LEVEL_0 },
	{ FREQ_LEVEL_5, VOLT_LEVEL_0 },
	{ FREQ_LEVEL_6, VOLT_LEVEL_0 },
	{ FREQ_LEVEL_7, VOLT_LEVEL_0 },
};

static struct voltage_table volt_table_2[] = {
	{ FREQ_LEVEL_0, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_1, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_2, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_3, VOLT_LEVEL_2 },

	{ FREQ_LEVEL_4, VOLT_LEVEL_0 },
	{ FREQ_LEVEL_5, VOLT_LEVEL_0 },
	{ FREQ_LEVEL_6, VOLT_LEVEL_0 },
	{ FREQ_LEVEL_7, VOLT_LEVEL_0 },
};

static struct voltage_table volt_table_3[] = {
	{ FREQ_LEVEL_0, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_1, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_2, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_3, VOLT_LEVEL_2 },

	{ FREQ_LEVEL_4, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_5, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_6, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_7, VOLT_LEVEL_1 },
};

static struct voltage_table volt_table_4[] = {
	{ FREQ_LEVEL_0, VOLT_LEVEL_3 },
	{ FREQ_LEVEL_1, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_2, VOLT_LEVEL_2 },
	{ FREQ_LEVEL_3, VOLT_LEVEL_2 },

	{ FREQ_LEVEL_4, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_5, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_6, VOLT_LEVEL_1 },
	{ FREQ_LEVEL_7, VOLT_LEVEL_1 },
};

static struct cpufreq_frequency_table freq_table_988MHz[] = {
	{ FREQ_LEVEL_0,  988000},
	{ FREQ_LEVEL_1,  864500},
	{ FREQ_LEVEL_2,  741000},
	{ FREQ_LEVEL_3,  617500},

	{ FREQ_LEVEL_4,  494000},
	{ FREQ_LEVEL_5,  370500},
	{ FREQ_LEVEL_6,  247000},
	{ FREQ_LEVEL_7,  123500},

	{ FREQ_LEVEL_MAX, CPUFREQ_TABLE_END },

};

/*acp_axi_clk/a9_clk = 1/8, 2/8, 4/8, default=2/8 */

static struct cpufreq_frequency_table freq_table_acp_988MHz[] = {

	{ FREQ_LEVEL_0,  247000},
	{ FREQ_LEVEL_1,  216125},
	{ FREQ_LEVEL_2,  185250},
	{ FREQ_LEVEL_3,  154375},

	{ FREQ_LEVEL_4,  247000},
	{ FREQ_LEVEL_5,  185250},
	{ FREQ_LEVEL_6,  123500},
	{ FREQ_LEVEL_7,   61750},

	{ FREQ_LEVEL_MAX, CPUFREQ_TABLE_END },

};

static struct cpufreq_frequency_table freq_table_1196MHz[] = {
	{ FREQ_LEVEL_0,  1196000},
	{ FREQ_LEVEL_1,  1046500},
	{ FREQ_LEVEL_2,   897000},
	{ FREQ_LEVEL_3,   747500},

	{ FREQ_LEVEL_4,   598000},
	{ FREQ_LEVEL_5,   448500},
	{ FREQ_LEVEL_6,   299000},
	{ FREQ_LEVEL_7,   149500},

	{ FREQ_LEVEL_MAX, CPUFREQ_TABLE_END },
};

/*acp_axi_clk/a9_clk = 1/8, 2/8, 4/8, default=2/8 */

static struct cpufreq_frequency_table freq_table_acp_1196MHz[] = {

	{ FREQ_LEVEL_0,  299000},
	{ FREQ_LEVEL_1,  261625},
	{ FREQ_LEVEL_2,  224250},
	{ FREQ_LEVEL_3,  186875},

	{ FREQ_LEVEL_4,  299000},
	{ FREQ_LEVEL_5,  224250},
	{ FREQ_LEVEL_6,  149500},
	{ FREQ_LEVEL_7,   74750},

	{ FREQ_LEVEL_MAX, CPUFREQ_TABLE_END },

};

static struct cpufreq_data_table freq_data_table[] = {
	{ 
		988000, freq_table_988MHz, freq_table_acp_988MHz,
		{0, volt_table_1, volt_table_2, volt_table_3}
	},
	{ 
		1196000, freq_table_1196MHz, freq_table_acp_1196MHz,
		{0, volt_table_1, volt_table_2, volt_table_4}
	},
};

static struct cpufreq_frequency_table *cpu_freq_table;
struct voltage_table *volt_table;
static struct clk *cluster_clk;
static unsigned int cpus_target_freq[NR_CPUS];
static bool cpufreq_is_suspended;
static unsigned int freq_level_curr = FREQ_LEVEL_HIGH;
static unsigned int freq_level_pre = FREQ_LEVEL_HIGH;
static DEFINE_MUTEX(comip_cpufreq_lock);

static struct cpufreq_frequency_table *freq_table_acp;
static struct clk *acp_clk;

static int cpufreq_acp_transition(struct notifier_block *nb,
	unsigned long state, void *data)
{
	struct cpufreq_freqs *freqs = data;

	if (CPU_ID_1 == freqs->cpu)
		return NOTIFY_OK;		

	if (CPUFREQ_POSTCHANGE == state) {
		clk_set_rate(acp_clk, freq_table_acp[freq_level_curr].frequency);
		dsb();
	}
	
	return NOTIFY_OK;
}

static struct notifier_block cpufreq_acp_nb = {
	.notifier_call = cpufreq_acp_transition,
};

static int cpufreq_acp_init(void)
{
	return cpufreq_register_notifier(&cpufreq_acp_nb,
		CPUFREQ_TRANSITION_NOTIFIER);
}

static int cpufreq_verify_policy(struct cpufreq_policy *policy)
{
	int ret;

	mutex_lock(&comip_cpufreq_lock);
	ret = cpufreq_frequency_table_verify(policy, cpu_freq_table);
	mutex_unlock(&comip_cpufreq_lock);

	return ret;
}

static inline unsigned int 
cpufreq_getspeed(void)
{
	return clk_get_rate(cluster_clk) / 1000;
}

static unsigned int cpufreq_freq_get(unsigned int cpu)
{
	return cpufreq_getspeed();
}

static void core_voltage_set(int type, int level)
{
	unsigned int reg;

	if (VOLT_TYPE_SW == type)
		type = AP_PWR_VOL_SW;
	else 
		type = AP_PWR_VOL_TW;

	reg = readl(io_p2v(AP_PWR_DV_CTL));
	reg &= ~(3 << type);
	reg |= (level & 3) << type;
	writel(reg, io_p2v(AP_PWR_DV_CTL));
	dsb();
}

static void core_voltage_read(int type, int *level)
{
	unsigned int reg;

	if (VOLT_TYPE_SW == type)
		type = AP_PWR_VOL_SW;
	else 
		type = AP_PWR_VOL_TW;

	reg = readl(io_p2v(AP_PWR_DV_CTL));
	reg = (reg >> type) & 3;
	if (level)
		*level = reg;
}

static int cpufreq_update_speed(unsigned int idx, int flag)
{
	struct cpufreq_freqs freqs;
	int target_volt;
	int curr_volt;
	int ret = 0;

	CPUFREQ_PR("cpufreq:cpu%d:old=%d --> new=%d, \n", 
		smp_processor_id(), freq_level_curr, idx);

	freqs.flags = 0;
	freqs.old = cpufreq_getspeed();
	freqs.new =  cpu_freq_table[idx].frequency;

	if (freqs.old != cpu_freq_table[freq_level_curr].frequency) 
		printk("cpufreq: old freq:%d is not matched level:%d\n", 
					freqs.old, freq_level_curr);

	if (freqs.old == freqs.new) {
		WARN(idx != freq_level_curr, 
			"cpufreq: new freq is not matched\n");
		return ret;
	}

	if (flag) {
		for_each_online_cpu(freqs.cpu)
			cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	}

	CPUFREQ_PR("cpufreq: transition: %u --> %u\n",
	       freqs.old, freqs.new);

	target_volt = volt_table[cpu_freq_table[idx].index].volt;
	core_voltage_read(VOLT_TYPE_SW, &curr_volt);

	if (freqs.new > freqs.old) {
		if (target_volt > curr_volt) {
			core_voltage_set(VOLT_TYPE_SW, target_volt);
			udelay(VOLT_STABLE_DURA_US);
		} else if (target_volt < curr_volt)
			printk(KERN_DEBUG "cpufreq-pre:volt is not matched\n");
	}

	ret = clk_set_rate(cluster_clk, freqs.new * 1000);

	if (!ret && freqs.new < freqs.old) {
		if (target_volt < curr_volt)
			core_voltage_set(VOLT_TYPE_SW, target_volt);
		else if (target_volt > curr_volt)
			printk(KERN_DEBUG "cpufreq-post:volt is not matched\n");
	}

	if (ret)
		pr_err("cpufreq: Failed to set cpu freq to %d kHz\n",
			freqs.new);
	else
		freq_level_curr = idx;

	if (flag) {
		if (ret)
			freqs.new = freqs.old;
		for_each_online_cpu(freqs.cpu)
			cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}

	return ret;
}

static inline void cpu_target_freq_set(int cpu, unsigned int freq)
{
	cpus_target_freq[cpu] = freq;
}

static unsigned int cpus_highest_freq(void)
{
	unsigned int rate = 0;
	int i;

	for_each_online_cpu(i)
		rate = max(rate, cpus_target_freq[i]);

	return rate;
}

static int cpufreq_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	unsigned int idx;
	unsigned int freq;
	int ret;

	mutex_lock(&comip_cpufreq_lock);

	smp_rmb();
	if (cpufreq_is_suspended) {
		ret = -EBUSY;
		goto out;
	}

	ret = cpufreq_frequency_table_target(policy, cpu_freq_table, target_freq,
		relation, &idx);
	if (unlikely(ret)) {
		ret = -ENODEV;
		goto out;
	}

	freq = cpu_freq_table[idx].frequency;
	cpu_target_freq_set(policy->cpu, freq);
	freq = cpus_highest_freq();

	ret = cpufreq_frequency_table_target(policy, cpu_freq_table, freq,
		relation, &idx);
	if (unlikely(ret)) {
		ret = -ENODEV;
		goto out;
	}

	ret = cpufreq_update_speed(idx, FREQ_IN_RUN);
out:
	mutex_unlock(&comip_cpufreq_lock);

	return ret;
}

static int cpufreq_pm_call(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	mutex_lock(&comip_cpufreq_lock);

	if (event == PM_SUSPEND_PREPARE) {
		CPUFREQ_PR("cpufreq:cpu%d: suspend pre\n", smp_processor_id());
		cpufreq_is_suspended = true;
		smp_wmb();
		cpufreq_update_speed(FREQ_LEVEL_LOW, FREQ_IN_RUN);

		if (volt_adj_type == VOLT_ADJ_LOW)
			pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 1, vout_val_0);
		else if (volt_adj_type == VOLT_ADJ_HIGH)
			pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 2, vout_val_0);

	} else if (event == PM_POST_SUSPEND) {
		CPUFREQ_PR("cpufreq:cpu%d: suspend exit\n", smp_processor_id());

		if (volt_adj_type == VOLT_ADJ_LOW)
			pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 1, vout_val_2);
		else if (volt_adj_type == VOLT_ADJ_HIGH)
			pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 2, vout_val_2);

		cpufreq_update_speed(FREQ_LEVEL_HIGH, FREQ_IN_RUN);
		cpufreq_is_suspended = false;
		smp_wmb();
	}
	
	mutex_unlock(&comip_cpufreq_lock);

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_pm_notifier = {
	.notifier_call = cpufreq_pm_call,
};

static int cpufreq_driver_init(struct cpufreq_policy *policy)
{
	if (policy->cpu >= NR_CPUS)
		return -EINVAL;

	/* cpu core clock*/
	cluster_clk = clk_get(NULL, CLUSTER_CLK_NAME);
	if (IS_ERR(cluster_clk))
		return PTR_ERR(cluster_clk);

	acp_clk = clk_get(NULL, "a9_acp_axi_clk");
	if (IS_ERR(acp_clk))
		return PTR_ERR(acp_clk);	

	cpufreq_frequency_table_cpuinfo(policy, cpu_freq_table);
	cpufreq_frequency_table_get_attr(cpu_freq_table, policy->cpu);
	policy->cur = cpufreq_getspeed();
	cpu_target_freq_set(policy->cpu, policy->cur);

	/* FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 300 * 1000;
	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;

	/*the only policy managers all cpus*/
	cpumask_copy(policy->related_cpus, cpu_possible_mask);
	cpumask_copy(policy->cpus, cpu_online_mask);

	if (policy->cpu == CPU_ID_0) {
		register_pm_notifier(&cpufreq_pm_notifier);
	}

	return 0;
}

static int cpufreq_driver_exit(struct cpufreq_policy *policy)
{
	if (policy->cpu == CPU_ID_0) {
		pr_err("cpufreq:cpu0 MUST NOT be hot-plugged out!\n");
		return -EINVAL;
	}
	cpu_target_freq_set(policy->cpu, 0);
	cpufreq_frequency_table_cpuinfo(policy, cpu_freq_table);
	clk_put(acp_clk);
	clk_put(cluster_clk);

	return 0;
}

static int cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int cpufreq_resume(struct cpufreq_policy *policy)
{
	return 0;
}

static struct freq_attr *cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver comip_cpufreq_driver = {
	.flags	= CPUFREQ_STICKY,
	.name	= "comip-cpufreq",
	.attr	= cpufreq_attr,
	.verify	= cpufreq_verify_policy,
	.target	= cpufreq_target,
	.get	= cpufreq_freq_get,
	.init	= cpufreq_driver_init,
	.exit	= cpufreq_driver_exit,
	.suspend = cpufreq_suspend,
	.resume  = cpufreq_resume,
};

int comip_cpufreq_idle(int on)
{
	unsigned int target_level = FREQ_LEVEL_LOW;
	int status = 0;
	
	if(!cluster_clk){
		status = -ENODEV;
		goto out;
	}
	smp_rmb();
	if(cpufreq_is_suspended){
		status = -EBUSY;
		goto out;
	}
	
	if(on){
		freq_level_pre = freq_level_curr;
		if (target_level != freq_level_curr)
			status = cpufreq_update_speed(target_level, FREQ_IN_IDLE);
	} else {
		if (target_level != freq_level_pre)
			status = cpufreq_update_speed(freq_level_pre, FREQ_IN_IDLE);
	}

out:
	return status;
}

static int __init cpufreq_table_init(int *index)
{
	struct clk *cpu_clk = NULL;
	unsigned long cpu_clk_rate = 0;
	int i;

	cpu_clk = clk_get(NULL, CLUSTER_CLK_NAME);
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);
	cpu_clk_rate = clk_get_rate(cpu_clk);
	clk_put(cpu_clk);

	for (i = 0; i < ARRAY_SIZE(freq_data_table); i++) {
		if (freq_data_table[i].freq == cpu_clk_rate / 1000) {
			break;
		}
	}
	if (i == ARRAY_SIZE(freq_data_table)) {
		printk(KERN_ALERT "comip_cpufreq_init: fail to find cpufreq table");
		return -EINVAL;
	}

	cpu_freq_table = freq_data_table[i].cluster_table;
	freq_table_acp = freq_data_table[i].acp_table;

	if (index)
		*index = i;
	return 0;
}

static int __init cpufreq_voltage_init(int index)
{
	struct voltage_table **ptr;
	int val;

	ptr = freq_data_table[index].volt_tables;
	core_voltage_read(VOLT_TYPE_SW, &val);
	if (val == 0x01) {
		core_voltage_set(VOLT_TYPE_TW, VOLT_LEVEL_1);
		volt_table = ptr[1];
		volt_adj_type = VOLT_ADJ_LOW;
	} else if (val == 0x02) {
		core_voltage_set(VOLT_TYPE_TW, VOLT_LEVEL_2);
		volt_table = ptr[2];
		volt_adj_type = VOLT_ADJ_HIGH;
	} else if (val == 0x03) {
		core_voltage_set(VOLT_TYPE_TW, VOLT_LEVEL_0);
		volt_table = ptr[3];
		volt_adj_type = VOLT_ADJ_DUAL;
	} else {
		pr_err("cpufreq:VOLT_TYPE_SW is NOT correct!\n");
		return -EINVAL;
	}

	printk(KERN_INFO "cpufreq:volt_adj_type:%d\n", volt_adj_type);

	if (VOLT_ADJ_DUAL == volt_adj_type) {
		printk(KERN_INFO "cpufreq:CPU VOUT: %d/%d/%d/%d\n",
			vout_val_0, vout_val_1,
			vout_val_2, vout_val_3);

		if (VOLT_VALUE_IS_INVALID(vout_val_0) ||
			VOLT_VALUE_IS_INVALID(vout_val_1) ||
			VOLT_VALUE_IS_INVALID(vout_val_2) ||
			VOLT_VALUE_IS_INVALID(vout_val_3) )
				return -EINVAL;

	} else {
		printk(KERN_INFO "cpufreq:CPU VOUT: %d/%d/%d\n",
			vout_val_0, vout_val_1, vout_val_2);

		if (VOLT_VALUE_IS_INVALID(vout_val_0) ||
			VOLT_VALUE_IS_INVALID(vout_val_1) ||
			VOLT_VALUE_IS_INVALID(vout_val_2) )
				return -EINVAL;

		pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 0, vout_val_1);
	}

	return 0;
}

static int __init comip_parse_tag_buck2_voltage(const struct tag *tag)
{
	struct tag_buck2_voltage *vol = (struct tag_buck2_voltage *)&tag->u;
	vout_val_0 = vol->vout0;
	vout_val_1 = vol->vout1;
	vout_val_2 = vol->vout2;
	vout_val_3 = vol->vout3;
	return 0;
}
__tagtable(ATAG_BUCK2_VOLTAGE, comip_parse_tag_buck2_voltage);

static int __init comip_cpufreq_init(void)
{
	int index = 0;

	if (cpufreq_table_init(&index))
		return -EPERM;
	if (cpufreq_voltage_init(index))
		return -EPERM;

	cpufreq_acp_init();
	return cpufreq_register_driver(&comip_cpufreq_driver);
}

static void __exit comip_cpufreq_exit(void)
{
	cpufreq_unregister_driver(&comip_cpufreq_driver);
}

module_init(comip_cpufreq_init);
module_exit(comip_cpufreq_exit);

