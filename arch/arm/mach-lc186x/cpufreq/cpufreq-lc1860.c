/*
 * arch/arm/mach-lc181x/cpufreq/cpufreq-lc1860.c
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
#include <plat/comip-thermal.h>
#include <mach/suspend.h>

#define COMIP_CPUFREQ_DEBUG	0
#if COMIP_CPUFREQ_DEBUG
#define CPUFREQ_PR(fmt, args...)	printk(KERN_INFO fmt, ## args)
#else
#define CPUFREQ_PR(fmt, args...)
#endif

#define VOLT_STABLE_DURA_US 100
#define AP_PWR_DV_CTL DDR_PWR_PMU_CTL
#define VOLT_VALUE_ADDR io_p2v(AP_SEC_RAM_BASE)

#define VOLT_VALUE_IS_INVALID(val) \
		(((unsigned int)(val)) < 700 || ((unsigned int)(val)) > 2000)

/* core voltage(DVFS). */
static int vout_val_0 = 900;
static int vout_val_1 = 1200;
static int vout_val_2 = 1250;
static int vout_val_3 = 1250;

enum{
	VOLT_ADJ_LOW  = 1, /*dvfs(0)*/
	VOLT_ADJ_HIGH = 2, /*dvfs(1)*/
	VOLT_ADJ_DUAL = 3, /*dvfs(1) && dvfs(0)*/
} volt_adj_type;

enum{
	VOLT_TYPE_SW,
	VOLT_TYPE_TW,
};

/*
 *for single dvfs pin, the map relation show as below:
 * VOLT_LEVEL_0                          : vout_val_2
 * VOLT_LEVEL_1(VOLT_LEVEL_2) : vout_val_1
 */
enum{
	VOLT_LEVEL_0 = 0,
	VOLT_LEVEL_1 = 1,
	VOLT_LEVEL_2 = 2,
	VOLT_LEVEL_3 = 3,
};

enum{
	FREQ_LEVEL_HIGH		= 0,
	FREQ_LEVEL_RESUME	= 7,
	FREQ_LEVEL_SUSPEND	= 12,
	FREQ_LEVEL_LOW		= 15,
	FREQ_LEVEL_MAX		= 16,
};

enum{
	FREQ_IN_IDLE,
	FREQ_IN_RUN,
};

struct voltage_table {
	unsigned int	index;
	unsigned int	volt;	/* uV */
};

struct cpufreq_data_table {
	unsigned int freq; /* kHz */
	struct cpufreq_frequency_table *cluster_table;
	struct cpufreq_frequency_table *acp_table;
	struct voltage_table *volt_tables[4];
};

static struct voltage_table volt_table_1[] = {
	{ 0,  VOLT_LEVEL_0 },
	{ 1,  VOLT_LEVEL_0 },
	{ 2,  VOLT_LEVEL_0 },
	{ 3,  VOLT_LEVEL_0 },
	{ 4,  VOLT_LEVEL_0 },
	{ 5,  VOLT_LEVEL_0 },
	{ 6,  VOLT_LEVEL_0 },
	{ 7,  VOLT_LEVEL_0 },
	{ 8,  VOLT_LEVEL_1 },
	{ 9,  VOLT_LEVEL_1 },
	{ 10, VOLT_LEVEL_1 },
	{ 11, VOLT_LEVEL_1 },
	{ 12, VOLT_LEVEL_1 },
	{ 13, VOLT_LEVEL_1 },
	{ 14, VOLT_LEVEL_1 },
	{ 15, VOLT_LEVEL_1 },
};

static struct voltage_table volt_table_2[] = {
	{ 0,  VOLT_LEVEL_0 },
	{ 1,  VOLT_LEVEL_0 },
	{ 2,  VOLT_LEVEL_0 },
	{ 3,  VOLT_LEVEL_0 },
	{ 4,  VOLT_LEVEL_0 },
	{ 5,  VOLT_LEVEL_0 },
	{ 6,  VOLT_LEVEL_0 },
	{ 7,  VOLT_LEVEL_0 },
	{ 8,  VOLT_LEVEL_2 },
	{ 9,  VOLT_LEVEL_2 },
	{ 10, VOLT_LEVEL_2 },
	{ 11, VOLT_LEVEL_2 },
	{ 12, VOLT_LEVEL_2 },
	{ 13, VOLT_LEVEL_2 },
	{ 14, VOLT_LEVEL_2 },
	{ 15, VOLT_LEVEL_2 },
};

static struct voltage_table volt_table_3[] = {
	{ 0,  VOLT_LEVEL_3 },
	{ 1,  VOLT_LEVEL_3 },
	{ 2,  VOLT_LEVEL_3 },
	{ 3,  VOLT_LEVEL_3 },
	{ 4,  VOLT_LEVEL_3 },
	{ 5,  VOLT_LEVEL_3 },
	{ 6,  VOLT_LEVEL_3 },
	{ 7,  VOLT_LEVEL_3 },
	{ 8,  VOLT_LEVEL_3 },
	{ 9,  VOLT_LEVEL_3 },
	{ 10, VOLT_LEVEL_3 },
	{ 11, VOLT_LEVEL_3 },
	{ 12, VOLT_LEVEL_3 },
	{ 13, VOLT_LEVEL_3 },
	{ 14, VOLT_LEVEL_3 },
	{ 15, VOLT_LEVEL_3 },
};

static struct voltage_table volt_table_4[] = {
	{ 0,  VOLT_LEVEL_3 },
	{ 1,  VOLT_LEVEL_3 },
	{ 2,  VOLT_LEVEL_3 },
	{ 3,  VOLT_LEVEL_3 },
	{ 4,  VOLT_LEVEL_3 },
	{ 5,  VOLT_LEVEL_3 },
	{ 6,  VOLT_LEVEL_3 },
	{ 7,  VOLT_LEVEL_3 },
	{ 8,  VOLT_LEVEL_2 },
	{ 9,  VOLT_LEVEL_2 },
	{ 10, VOLT_LEVEL_2 },
	{ 11, VOLT_LEVEL_2 },
	{ 12, VOLT_LEVEL_2 },
	{ 13, VOLT_LEVEL_2 },
	{ 14, VOLT_LEVEL_2 },
	{ 15, VOLT_LEVEL_2 },
};

static struct cpufreq_frequency_table freq_table_988MHz[] = {
	{ 0,   988000},
	{ 1,   926250},
	{ 2,   864500},
	{ 3,   802750},
	{ 4,   741000},
	{ 5,   679250},
	{ 6,   617500},
	{ 7,   555750},
	{ 8,   494000},
	{ 9,   432250},
	{ 10,  370500},
	{ 11,  308750},
	{ 12,  247000},
	{ 13,  185250},
	{ 14,  123500},
	{ 15,  61750},

	{ FREQ_LEVEL_MAX, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_1092MHz[] = {
	{ 0,  1092000},
	{ 1,  1023750},
	{ 2,   955500},
	{ 3,   887250},
	{ 4,   819000},
	{ 5,   750750},
	{ 6,   682500},
	{ 7,   614250},
	{ 8,   546000},
	{ 9,   477750},
	{ 10,  409500},
	{ 11,  341250},
	{ 12,  273000},
	{ 13,  204750},
	{ 14,  136500},
	{ 15,  68250},

	{ FREQ_LEVEL_MAX, CPUFREQ_TABLE_END },
};

static struct cpufreq_frequency_table freq_table_1196MHz[] = {
	{ 0,  1196000},
	{ 1,  1121250},
	{ 2,  1046500},
	{ 3,   971750},
	{ 4,   897000},
	{ 5,   822250},
	{ 6,   747500},
	{ 7,   672750},
	{ 8,   598000},
	{ 9,   523250},
	{ 10,  448500},
	{ 11,  373750},
	{ 12,  299000},
	{ 13,  224250},
	{ 14,  149500},
	{ 15,  74750},

	{ FREQ_LEVEL_MAX, CPUFREQ_TABLE_END },
};

static struct cpufreq_data_table freq_data_table[] = {
	{ 
		988000, freq_table_988MHz, 0,
		{0, volt_table_1, volt_table_2, volt_table_3}
	},
	{ 
		1092000, freq_table_1092MHz, 0,
		{0, volt_table_1, volt_table_2, volt_table_4}
	},
	{
		1196000, freq_table_1196MHz, 0,
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
		type = VOL_SW_A7;
	else 
		type = VOL_TW_A7;

	reg = readl(io_p2v(DDR_PWR_PMU_CTL));
	reg &= ~(3 << type);
	reg |= (level & 3) << type;
	writel(reg, io_p2v(AP_PWR_DV_CTL));
	dsb();
}

static void core_voltage_read(int type, int *level)
{
	unsigned int reg;

	if (VOLT_TYPE_SW == type)
		type = VOL_SW_A7;
	else 
		type = VOL_TW_A7;

	reg = readl(io_p2v(AP_PWR_DV_CTL));
	reg = (reg >> type) & 3;
	if (level)
		*level = reg;
}

static int cpufreq_update_speed(struct cpufreq_policy *policy, unsigned int idx, int flag)
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
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
	}

	CPUFREQ_PR("cpufreq: transition: %u --> %u\n",
	       freqs.old, freqs.new);

	target_volt = volt_table[cpu_freq_table[idx].index].volt;
	core_voltage_read(VOLT_TYPE_SW, &curr_volt);

	if (freqs.new > freqs.old) {
		if (target_volt != curr_volt) {
			core_voltage_set(VOLT_TYPE_SW, target_volt);
			udelay(VOLT_STABLE_DURA_US);
		}
	}

	ret = clk_set_rate(cluster_clk, freqs.new * 1000);

	if (!ret && freqs.new < freqs.old) {
		if (target_volt != curr_volt)
			core_voltage_set(VOLT_TYPE_SW, target_volt);
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
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
	}

	return ret;
}

static inline void cpu_target_freq_set(int cpu, unsigned int freq)
{
	cpus_target_freq[cpu] = freq;
}

static unsigned int cpus_highest_freq(void)
{
	unsigned int rate = FREQ_LEVEL_LOW;
	int i;

	for_each_online_cpu(i)
		rate = min(rate, cpus_target_freq[i]);

	return rate;
}

static int cpufreq_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	unsigned int idx;
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

	cpu_target_freq_set(policy->cpu, idx);
	idx = cpus_highest_freq();

	ret = cpufreq_update_speed(policy, idx, FREQ_IN_RUN);
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
		cpufreq_update_speed(cpufreq_cpu_get(0), FREQ_LEVEL_SUSPEND, FREQ_IN_RUN);

		if (volt_adj_type == VOLT_ADJ_LOW || volt_adj_type == VOLT_ADJ_HIGH)
			pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 0, vout_val_0);

	} else if (event == PM_POST_SUSPEND) {
		CPUFREQ_PR("cpufreq:cpu%d: suspend exit\n", smp_processor_id());

		if (volt_adj_type == VOLT_ADJ_LOW || volt_adj_type == VOLT_ADJ_HIGH)
			pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 0, vout_val_2);

		cpufreq_update_speed(cpufreq_cpu_get(0), FREQ_LEVEL_RESUME, FREQ_IN_RUN);
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

	cpufreq_frequency_table_cpuinfo(policy, cpu_freq_table);
	cpufreq_frequency_table_get_attr(cpu_freq_table, policy->cpu);
	policy->cur = cpufreq_getspeed();
	cpu_target_freq_set(policy->cpu, FREQ_LEVEL_LOW);

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
	cpu_target_freq_set(policy->cpu, FREQ_LEVEL_MAX);
	cpufreq_frequency_table_cpuinfo(policy, cpu_freq_table);
	cpufreq_frequency_table_put_attr(policy->cpu);
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
			status = cpufreq_update_speed(cpufreq_cpu_get(0), target_level, FREQ_IN_IDLE);
	} else {
		if (target_level != freq_level_pre)
			status = cpufreq_update_speed(cpufreq_cpu_get(0), freq_level_pre, FREQ_IN_IDLE);
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

	for (i = 0; i < NR_CPUS; i++)
		cpus_target_freq[i] = FREQ_LEVEL_MAX;

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

	if (index)
		*index = i;
	return 0;
}

static void __init single_dvfs_init(void)
{
	pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 0, vout_val_2);
	core_voltage_set(VOLT_TYPE_SW, VOLT_LEVEL_0);
	if (VOLT_ADJ_LOW == volt_adj_type)
		pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 1, vout_val_1);
	else if (VOLT_ADJ_HIGH == volt_adj_type)
		pmic_voltage_set(PMIC_TO_DIRECT_POWER_ID(PMIC_DCDC2), 2, vout_val_1);
}

static int __init cpufreq_voltage_init(int index)
{
	struct voltage_table **ptr;
	int val;

	ptr = freq_data_table[index].volt_tables;
	core_voltage_read(VOLT_TYPE_SW, &val);
	if (val == 0x01) {
		volt_table = ptr[1];
		volt_adj_type = VOLT_ADJ_LOW;
	} else if (val == 0x02) {
		volt_table = ptr[2];
		volt_adj_type = VOLT_ADJ_HIGH;
	} else if (val == 0x03) {
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

		single_dvfs_init();
	}

	core_voltage_set(VOLT_TYPE_TW, VOLT_LEVEL_0);

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

	return cpufreq_register_driver(&comip_cpufreq_driver);
}

static void __exit comip_cpufreq_exit(void)
{
	cpufreq_unregister_driver(&comip_cpufreq_driver);
}

module_init(comip_cpufreq_init);
module_exit(comip_cpufreq_exit);

