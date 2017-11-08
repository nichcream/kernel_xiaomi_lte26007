/*
 * ARM big.LITTLE Platforms CPUFreq support
 *
 * Copyright (C) 2013 ARM Ltd.
 * Sudeep KarkadaNagesha <sudeep.karkadanagesha@arm.com>
 *
 * Copyright (C) 2013 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/opp.h>
#include <linux/slab.h>
#include <linux/topology.h>
#include <linux/types.h>
#include <asm/bL_switcher.h>
#include <linux/cpu.h>
#include <plat/hardware.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/mcpm.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <asm/setup.h>
#include <plat/comip-setup.h>
#include <mach/comip-dvfs.h>

#include "arm_big_little.h"

#ifdef CONFIG_BL_SWITCHER
bool bL_switching_enabled;
#endif

#define ACTUAL_FREQ(cluster, freq)	(freq)
#define VIRT_FREQ(cluster, freq)	(freq)
static struct clk *clk[MAX_CLUSTERS];
static struct voltage_table *voltage[MAX_CLUSTERS + 1];
static struct cpufreq_frequency_table *freq_table[MAX_CLUSTERS + 1];
static atomic_t cluster_usage[MAX_CLUSTERS + 1] = {ATOMIC_INIT(0),
	ATOMIC_INIT(0)};

static unsigned int clk_big_min;	/* (Big) clock frequencies */
static unsigned int clk_big_mid;
static unsigned int clk_little_max;	/* Maximum clock frequency (Little) */
static unsigned int in_suspend = 0;

static DEFINE_PER_CPU(unsigned int, physical_cluster);
static DEFINE_PER_CPU(unsigned int, cpu_last_req_freq);

static struct mutex cluster_lock[MAX_CLUSTERS];
static struct mutex voltage_freq;

static struct cpufreq_data_table freq_data_table[] = {
	{624000, sa7_freq_table_624MHz, sa7_voltage_624MHZ},
	{1196000, ha7_freq_table_1196MHz, ha7_voltage_1196MHZ},
	{1495000, ha7_freq_table_1495MHz, ha7_voltage_1495MHZ},
	{1599000, ha7_freq_table_1599MHz, ha7_voltage_1599MHZ},
	{1807000, ha7_freq_table_1807MHz, ha7_voltage_1807MHZ},
};

static BLOCKING_NOTIFIER_HEAD(cpufreq_hotplug_activation_notifier);

int cpufreq_hotplug_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&cpufreq_hotplug_activation_notifier, nb);
}
EXPORT_SYMBOL_GPL(cpufreq_hotplug_register_notifier);

int cpufreq_hotplug_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&cpufreq_hotplug_activation_notifier, nb);
}
EXPORT_SYMBOL_GPL(cpufreq_hotplug_unregister_notifier);

static int cpufreq_hotplug_activation_notify(unsigned long val)
{
	int ret;

	ret = blocking_notifier_call_chain(&cpufreq_hotplug_activation_notifier, val, NULL);
	if (ret)
		pr_err("cpufreq hotplug notify error!\n");

	return ret;
}

static int __init get_buck2_info(const struct tag *tag)
{
	struct tag_buck2_voltage *vol = (struct tag_buck2_voltage *)&tag->u;
	printk(KERN_INFO "CPU VOUT: %d/%d/%d/%d\n",
		vol->vout0, vol->vout1, vol->vout2, vol->vout3);
	return 0;
}

__tagtable(ATAG_BUCK2_VOLTAGE, get_buck2_info);

static int freq_table_init(u32 cluster, struct cpufreq_frequency_table **table)
{
       int ret, i;
       unsigned long cpu_clk_rate = 0;

       if (atomic_inc_return(&cluster_usage[cluster]) != 1)
               return 0;

       if(cluster == HA7_CLUSTER){
               clk[cluster] = clk_get(NULL, HA7_CLK_NAME);
       }else if (cluster == SA7_CLUSTER){
               clk[cluster] = clk_get(NULL, SA7_CLK_NAME);
       }

       if (!IS_ERR(clk[cluster])) {
	       cpu_clk_rate = clk_get_rate(clk[cluster]);
	       for (i = 0; i < ARRAY_SIZE(freq_data_table); i++) {
		       if (freq_data_table[i].max_freq == cpu_clk_rate / 1000) {
			       *table = freq_data_table[i].table;
			       voltage[cluster] = freq_data_table[i].volt_table;
			       break;
		       }
	       }
	       if(i == ARRAY_SIZE(freq_data_table)){
		       pr_err("Get cluster %d freq table ERROR !!\n ", cluster);
		       ret = -EINVAL;
		       goto out;
	       }
	       pr_debug("Get freq table OK, max freq is %d\n", freq_data_table[i].max_freq);
	       return 0;
       }

       ret = PTR_ERR(clk[cluster]);
out:
       atomic_dec(&cluster_usage[cluster]);
       pr_err("%s: Failed to get data for cluster: %d\n", __func__,
                       cluster);
       return ret;
}

static int _get_cluster_clk_and_freq_table(u32 cluster)
{
	int ret;

	ret = freq_table_init(cluster, &freq_table[cluster]);

	return ret;
}

static unsigned int find_cluster_maxfreq(int cluster)
{
	int j;
	u32 max_freq = 0, cpu_freq;

	for_each_online_cpu(j) {
		cpu_freq = per_cpu(cpu_last_req_freq, j);

		if ((cluster == per_cpu(physical_cluster, j)) &&
				(max_freq < cpu_freq))
			max_freq = cpu_freq;
	}

	pr_debug("%s: cluster: %d, max freq: %d\n", __func__, cluster,
			max_freq);

	return max_freq;
}

static unsigned int clk_get_cpu_rate(unsigned int cpu)
{
	u32 cur_cluster = per_cpu(physical_cluster, cpu);
	u32 rate = clk_get_rate(clk[cur_cluster]) / 1000;

	/* For switcher we use virtual A15 clock rates */
	if (is_bL_switching_enabled())
		rate = VIRT_FREQ(cur_cluster, rate);

	pr_debug("%s: cpu: %d, cluster: %d, freq: %u\n", __func__, cpu,
			cur_cluster, rate);

	return rate;
}

static unsigned int bL_cpufreq_get_rate(unsigned int cpu)
{
	if (is_bL_switching_enabled()) {
		pr_debug("%s: freq: %d\n", __func__, per_cpu(cpu_last_req_freq,
					cpu));

		return per_cpu(cpu_last_req_freq, cpu);
	} else {
		return clk_get_cpu_rate(cpu);
	}
}

static unsigned int get_cpufreq_gr(void){
	u32 val;

	val = __raw_readl(io_p2v(AP_PWR_HA7_CLK_CTL));
	return val & 0x1f;
}

static unsigned int get_voltage_from_rate(u32 cpu, u32 rate)
{
	int count;
	u32 cur_cluster = cpu_to_cluster(cpu);
	struct voltage_table *table = voltage[cur_cluster];
	struct cpufreq_frequency_table *freq_tbl = freq_table[cur_cluster];
	for (count = 0; freq_tbl[count].frequency!= CPUFREQ_TABLE_END; count++){
		if (rate == freq_tbl[count].frequency) {
			return table[count].volt;
		}
	}
	return -1;
}

static unsigned int
bL_cpufreq_set_rate(u32 cpu, u32 old_cluster, u32 new_cluster, u32 rate)
{
	u32 new_rate, prev_rate, voltage_index, cur_index;
	int ret = 0, dvs = 0;
	bool bLs = is_bL_switching_enabled();

	mutex_lock(&voltage_freq);

	if (bLs) {
		prev_rate = per_cpu(cpu_last_req_freq, cpu);
		per_cpu(cpu_last_req_freq, cpu) = rate;
		per_cpu(physical_cluster, cpu) = new_cluster;

		new_rate = find_cluster_maxfreq(new_cluster);
		new_rate = ACTUAL_FREQ(new_cluster, new_rate);
	} else {
		new_rate = rate;
	}
	
	voltage_index = get_voltage_from_rate(cpu, new_rate);
	if(voltage_index == -1) {
		pr_err("ERROR new rate %d couldnot mach any voltage index !!\n", new_rate);
		mutex_unlock(&voltage_freq);
		return -1;
	}
	cur_index = get_cur_voltage_level(CPU_CORE);

	if (cur_index == voltage_index)
		dvs = 0;
	else if (cur_index > voltage_index)
		dvs = -1;
	else
		dvs = 1;

	pr_debug("%s: cpu: %d, old cluster: %d, new cluster: %d, freq: %d\n",
			__func__, cpu, old_cluster, new_cluster, new_rate);

	if (dvs == 1) {
		set_cur_voltage_level(CPU_CORE, VOLT_LEVEL_3);
		if (get_cur_voltage_level(CPU_CORE) != VOLT_LEVEL_3) {
			ret = -1;
			goto not_set_rate;
		}
	}
	/* check voltage */
	if (new_rate > clk_big_mid) {
		if (get_cur_voltage_level(CPU_CORE) != VOLT_LEVEL_3) {
			ret = -2;
			goto not_set_rate;
		}
	}
	ret = clk_set_rate(clk[new_cluster], new_rate * 1000);
not_set_rate:
	if (WARN_ON(ret)) {
		pr_err("clk_set_rate %d failed: %d, new cluster: %d\n", new_rate, ret,
				new_cluster);
		if (bLs) {
			per_cpu(cpu_last_req_freq, cpu) = prev_rate;
			per_cpu(physical_cluster, cpu) = old_cluster;
		}
		mutex_unlock(&voltage_freq);
		return ret;
	}

	/* Recalc freq for old cluster when switching clusters */
	if (old_cluster != new_cluster) {
		pr_debug("%s cpu: %d, old cluster: %d, new cluster: %d\n",
				__func__, cpu, old_cluster, new_cluster);
		mutex_unlock(&voltage_freq);
		/* Switch cluster */
		bL_switch_request(cpu, new_cluster);
#if 0
		mutex_lock(&cluster_lock[old_cluster]);
		/* Set freq of old cluster if there are cpus left on it */
		new_rate = find_cluster_maxfreq(old_cluster);
		new_rate = ACTUAL_FREQ(old_cluster, new_rate);

		if (new_rate) {
			pr_debug("%s: Updating rate of old cluster: %d, to freq: %d\n",
					__func__, old_cluster, new_rate);

			if (clk_set_rate(clk[old_cluster], new_rate * 1000))
				pr_err("%s: clk_set_rate failed: %d, old cluster: %d\n",
						__func__, ret, old_cluster);
		}
		mutex_unlock(&cluster_lock[old_cluster]);
#endif
	} else if (new_cluster == HA7_CLUSTER) {
		if (dvs == -1){
			if (get_cpufreq_gr() < 9)
				set_cur_voltage_level(CPU_CORE, VOLT_LEVEL_2);
		}
		mutex_unlock(&voltage_freq);
	}
	return 0;
}

/* Validate policy frequency range */
static int bL_cpufreq_verify_policy(struct cpufreq_policy *policy)
{
	u32 cur_cluster = cpu_to_cluster(policy->cpu);

	return cpufreq_frequency_table_verify(policy, freq_table[cur_cluster]);
}

/* Set clock frequency */
static int bL_cpufreq_set_target(struct cpufreq_policy *policy,
		unsigned int target_freq, unsigned int relation)
{
	struct cpufreq_freqs freqs;
	u32 cpu = policy->cpu, freq_tab_idx, cur_cluster, new_cluster,
	    actual_cluster;
	int ret = 0;
	int need_hotplug = 0;

	cur_cluster = cpu_to_cluster(cpu);
	new_cluster = actual_cluster = per_cpu(physical_cluster, cpu);

	freqs.old = bL_cpufreq_get_rate(cpu);

	/* Determine valid target frequency using freq_table */
	cpufreq_frequency_table_target(policy, freq_table[cur_cluster],
			target_freq, relation, &freq_tab_idx);
	freqs.new = freq_table[cur_cluster][freq_tab_idx].frequency;

	pr_debug("set before %s: cpu: %d, cluster: %d, oldfreq: %d, target freq: %d, new freq: %d\n",
			__func__, cpu, cur_cluster, freqs.old, target_freq,
			freqs.new);

	if (freqs.old == freqs.new)
		return 0;
	if (is_bL_switching_enabled()) {
		/* new clock < big_min, only cpu0 run */
		if (freqs.new < clk_big_min && (num_online_cpus() == 1)
			&& (actual_cluster == HA7_CLUSTER)){
			if (!in_suspend && !cpufreq_hotplug_activation_notify(HOTPLUG_DISABLE)){
				if (num_online_cpus() == 1) {
					/* Check if SA7 is powerdown */
					if(!mcpm_check_cpu_powered_down(0, SA7_CLUSTER))
						new_cluster = SA7_CLUSTER;
					else {
						pr_err("WARNING !! SA7 is powered !\n");
						need_hotplug = 1;
					}
				} else
						need_hotplug = 1;
			}
		/* new clock > clk little, sa7 run */
		}else if ((freqs.new > clk_little_max) && (actual_cluster == SA7_CLUSTER)){
			if(!mcpm_check_cpu_powered_down(0, HA7_CLUSTER)) {
				new_cluster = HA7_CLUSTER;
				need_hotplug = 1;
			} else {
				pr_err("WARNING !! HA7 is powered\n");
				goto out;
			}
		}
	}

	if ((freqs.new < clk_big_min) && (actual_cluster == HA7_CLUSTER)
		&& (new_cluster != SA7_CLUSTER))
			goto out;

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	ret = bL_cpufreq_set_rate(cpu, actual_cluster, new_cluster, freqs.new);
	if (ret)
		freqs.new = freqs.old;

	policy->cur = freqs.new;

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);

out:
	if (need_hotplug){
		if (cpufreq_hotplug_activation_notify(HOTPLUG_ENABLE))
			pr_err("Enable HOTPLUG ERROR!!\n");
	}

	return ret;
}

static inline u32 get_voltage_count(struct voltage_table *table)
{
	int count;

	for(count = 0; table[count].volt != CPUFREQ_TABLE_END; count++)
		;

	return count;
}

static inline u32 get_table_count(struct cpufreq_frequency_table *table)
{
	int count;

	for (count = 0; table[count].frequency != CPUFREQ_TABLE_END; count++)
		;

	return count;
}

/* get the minimum frequency in the cpufreq_frequency_table */
static inline u32 get_table_min(struct cpufreq_frequency_table *table)
{
	int i;
	uint32_t min_freq = ~0;
	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency < min_freq)
			min_freq = table[i].frequency;
	return min_freq;
}

/* get the maximum frequency in the cpufreq_frequency_table */
static inline u32 get_table_max(struct cpufreq_frequency_table *table)
{
	int i;
	uint32_t max_freq = 0;
	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++)
		if (table[i].frequency > max_freq)
			max_freq = table[i].frequency;
	return max_freq;
}

static int merge_cluster_tables(void)
{
	int i, j, k = 0, count = 1;
	struct cpufreq_frequency_table *table;

	for (i = 0; i < MAX_CLUSTERS; i++)
		count += get_table_count(freq_table[i]);

	table = kzalloc(sizeof(*table) * count, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	freq_table[MAX_CLUSTERS] = table;

	/* Add in reverse order to get freqs in increasing order */
	for (i = MAX_CLUSTERS - 1; i >= 0; i--) {
		for (j = 0; freq_table[i][j].frequency != CPUFREQ_TABLE_END;
				j++) {
			table[k].frequency = VIRT_FREQ(i,
					freq_table[i][j].frequency);
			pr_debug("%s: index: %d, freq: %d\n", __func__, k,
					table[k].frequency);
			k++;
		}
	}

	table[k].index = k;
	table[k].frequency = CPUFREQ_TABLE_END;

	pr_debug("%s: End, table: %p, count: %d\n", __func__, table, k);

	return 0;
}

static int merge_volate_table(void)
{
	int i, j, k = 0, count = 1;
	struct voltage_table *table;

	for (i = 0; i < MAX_CLUSTERS; i++)
		count += get_voltage_count(voltage[i]);

	table = kzalloc(sizeof(*table) * count, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	voltage[MAX_CLUSTERS] = table;
	/* Add in reverse order to get freqs in increasing order */
	for (i = MAX_CLUSTERS - 1; i >= 0; i--) {
		for (j = 0; voltage[i][j].volt != CPUFREQ_TABLE_END;
				j++) {
			table[k].volt = voltage[i][j].volt;
			pr_debug("%s: index: %d, voltage: %d\n", __func__, k,
					table[k].volt);
			k++;
		}
	}

	table[k].index = k;
	table[k].volt = CPUFREQ_TABLE_END;

	pr_debug("voltage %s: End, table: %p, count: %d\n", __func__, table, k);
	return 0;
}
static void _put_cluster_clk_and_freq_table(u32 cluster)
{

	if (!atomic_dec_return(&cluster_usage[cluster])) {
		clk_put(clk[cluster]);
		if (freq_table[cluster])
			freq_table[cluster] = NULL;
		pr_err("%s: cluster: %d\n", __func__, cluster);
	}
}

static void put_cluster_clk_and_freq_table(struct device *cpu_dev)
{
	u32 cluster = cpu_to_cluster(cpu_dev->id);
	int i;

	if (cluster < MAX_CLUSTERS)
		return _put_cluster_clk_and_freq_table(cluster);

	if (atomic_dec_return(&cluster_usage[MAX_CLUSTERS]))
		return;

	for (i = 0; i < MAX_CLUSTERS; i++) {
		_put_cluster_clk_and_freq_table(i);
	}
}

static int get_cluster_clk_and_freq_table(struct device *cpu_dev)
{
	u32 cluster = cpu_to_cluster(cpu_dev->id);
	int i, ret;

	if (cluster < MAX_CLUSTERS)
		return _get_cluster_clk_and_freq_table(cluster);

	if (atomic_inc_return(&cluster_usage[MAX_CLUSTERS]) != 1)
		return 0;

	/*
	 * Get data for all clusters and fill virtual cluster with a merge of
	 * both
	 */
	for (i = 0; i < MAX_CLUSTERS; i++) {
		ret = _get_cluster_clk_and_freq_table(i);
		if (ret)
			goto put_clusters;
	}

	ret = merge_cluster_tables();
	if (ret)
		goto put_clusters;

	ret = merge_volate_table();
	if(ret)
		goto put_clusters;
	/* Assuming 2 cluster, set clk_big_min and clk_little_max */
	clk_big_mid = get_table_max(freq_table[1]) / 2;
	clk_big_min = get_table_min(freq_table[1]);
	clk_little_max = get_table_max(freq_table[0]);

	pr_debug("%s: cluster: %d, clk_big_min: %d, clk_little_max: %d, clk_big_mid: %d\n",
			__func__, cluster, clk_big_min, clk_little_max, clk_big_mid);

	return 0;

put_clusters:
	while (i--)
		_put_cluster_clk_and_freq_table(i);

	if (voltage[MAX_CLUSTERS])
		kfree(voltage[MAX_CLUSTERS]);

	if (freq_table[MAX_CLUSTERS])
		kfree(freq_table[MAX_CLUSTERS]);

	atomic_dec(&cluster_usage[MAX_CLUSTERS]);

	return ret;
}

/* Per-CPU initialization */
static int bL_cpufreq_init(struct cpufreq_policy *policy)
{
	u32 cur_cluster = cpu_to_cluster(policy->cpu);
	struct device *cpu_dev;
	cpumask_t *cmask;
	int ret;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
				policy->cpu);
		return -ENODEV;
	}

	ret = get_cluster_clk_and_freq_table(cpu_dev);
	if (ret)
		return ret;

	ret = cpufreq_frequency_table_cpuinfo(policy, freq_table[cur_cluster]);
	if (ret) {
		dev_err(cpu_dev, "CPU %d, cluster: %d invalid freq table\n",
				policy->cpu, cur_cluster);
		put_cluster_clk_and_freq_table(cpu_dev);
		return ret;
	}

	cpufreq_frequency_table_get_attr(freq_table[cur_cluster], policy->cpu);

	if (cur_cluster < MAX_CLUSTERS) {
		cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));

		per_cpu(physical_cluster, policy->cpu) = cur_cluster;
	} else {
		/* Assumption: during init, we are always running on Ha7 */
		per_cpu(physical_cluster, policy->cpu) = HA7_CLUSTER;
	}

	policy->cpuinfo.transition_latency = 300 * 1000;

	policy->cur = clk_get_cpu_rate(policy->cpu);

	if (is_bL_switching_enabled())
		per_cpu(cpu_last_req_freq, policy->cpu) = policy->cur;

	cmask = topology_core_cpumask(policy->cpu);
	cpumask_copy(policy->related_cpus, cmask);
	cpumask_and(cmask, cmask, cpu_online_mask);
	cpumask_copy(policy->cpus, cmask);

	pr_debug("%s: CPU %d initialized\n", __func__, policy->cpu);
	return 0;
}

/* Export freq_table to sysfs */
static struct freq_attr *bL_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver bL_cpufreq_driver = {
	.name			= "arm-big-little",
	.flags			= CPUFREQ_STICKY,
	.verify			= bL_cpufreq_verify_policy,
	.target			= bL_cpufreq_set_target,
	.get			= bL_cpufreq_get_rate,
	.init			= bL_cpufreq_init,
	.have_governor_per_policy = true,
	.attr			= bL_cpufreq_attr,
};

static int bL_pm_notifier_callback(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		in_suspend = 1;
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		in_suspend = 0;
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block bL_pm_notifier_block = {
	.notifier_call = bL_pm_notifier_callback
};

static int bL_cpufreq_switcher_notifier(struct notifier_block *nfb,
					unsigned long action, void *_arg)
{
	pr_debug("%s: action: %ld\n", __func__, action);

	switch (action) {
	case BL_NOTIFY_PRE_ENABLE:
	case BL_NOTIFY_PRE_DISABLE:
		cpufreq_unregister_driver(&bL_cpufreq_driver);
		break;

	case BL_NOTIFY_POST_ENABLE:
		set_switching_enabled(true);
		cpufreq_register_driver(&bL_cpufreq_driver);
		break;

	case BL_NOTIFY_POST_DISABLE:
		set_switching_enabled(false);
		cpufreq_register_driver(&bL_cpufreq_driver);
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block bL_switcher_notifier = {
	.notifier_call = bL_cpufreq_switcher_notifier,
};

int bL_cpufreq_register(void)
{
	int ret, i;

	ret = bL_switcher_get_enabled();
	set_switching_enabled(ret);
	register_pm_notifier(&bL_pm_notifier_block);

	for (i = 0; i < MAX_CLUSTERS; i++)
		mutex_init(&cluster_lock[i]);
	mutex_init(&voltage_freq);
	ret = cpufreq_register_driver(&bL_cpufreq_driver);
	if (ret) {
		pr_info("%s: Failed registering platform driver err: %d\n",
				__func__, ret);
	} else {
		ret = bL_switcher_register_notifier(&bL_switcher_notifier);
		if (ret) {
			cpufreq_unregister_driver(&bL_cpufreq_driver);
		} else {
			pr_info("%s: Registered platform driver\n",
					__func__);
		}
	}

	bL_switcher_put_enabled();
	return ret;
}
EXPORT_SYMBOL_GPL(bL_cpufreq_register);

void bL_cpufreq_unregister(void)
{

	bL_switcher_get_enabled();
	bL_switcher_unregister_notifier(&bL_switcher_notifier);
	cpufreq_unregister_driver(&bL_cpufreq_driver);
	bL_switcher_put_enabled();
	pr_info("%s: Un-registered platform driver\n", __func__);

	/* For saving table get/put on every cpu in/out */
	if (is_bL_switching_enabled()) {
		put_cluster_clk_and_freq_table(get_cpu_device(0));
	} else {
		int i;

		for (i = 0; i < MAX_CLUSTERS; i++) {
			struct device *cdev = get_cpu_device(i);
			if (!cdev) {
				pr_err("%s: failed to get cpu%d device\n",
						__func__, i);
				return;
			}

			put_cluster_clk_and_freq_table(cdev);
		}
	}

}

#ifndef CONFIG_COMIP_HOTPLUG
#error "Need define CONFIG_COMIP_HOTPLUG"
#endif
EXPORT_SYMBOL_GPL(bL_cpufreq_unregister);

late_initcall(bL_cpufreq_register);
