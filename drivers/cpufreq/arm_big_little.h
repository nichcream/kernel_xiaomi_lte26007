/*
 * ARM big.LITTLE platform's CPUFreq header file
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
#ifndef CPUFREQ_ARM_BIG_LITTLE_H
#define CPUFREQ_ARM_BIG_LITTLE_H

#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/types.h>

/* Currently we support only two clusters */
#define A15_CLUSTER	1
#define A7_CLUSTER	0
#define HOTPLUG_ENABLE	0
#define HOTPLUG_DISABLE 1
#define HA7_CLUSTER	1
#define SA7_CLUSTER	0
#define MAX_CLUSTERS	2
#define HA7_CLK_NAME	"a7_clk"
#define SA7_CLK_NAME	"sa7_clk"

struct voltage_table {
	unsigned int index;
	unsigned int volt;	/* uV */
};

struct cpufreq_data_table {
	unsigned int max_freq; /* kHz */
	struct cpufreq_frequency_table *table;
	struct voltage_table *volt_table;
};

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

struct cpufreq_frequency_table ha7_freq_table_1807MHz[] = {
	{ 0,  1807000},
	{ 1,  1694063},
	{ 2,  1581125},
	{ 3,  1468188},
	{ 4,  1355250},
	{ 5,  1242313},
	{ 6,  1129375},
	{ 7,  1016438},
	{ 8,   903500},
	{ 9,   790563},
	{ 10,  677625},
	{ 11,  CPUFREQ_TABLE_END},
};

struct voltage_table ha7_voltage_1807MHZ[] = {
	{ 0,  VOLT_LEVEL_3 },
	{ 1,  VOLT_LEVEL_3 },
	{ 2,  VOLT_LEVEL_3 },
	{ 3,  VOLT_LEVEL_3 },
	{ 4,  VOLT_LEVEL_3 },
	{ 5,  VOLT_LEVEL_3 },
	{ 6,  VOLT_LEVEL_3 },
	{ 7,  VOLT_LEVEL_3 },
	{ 8,  VOLT_LEVEL_3 },
	{ 9,  VOLT_LEVEL_2 },
	{ 10, VOLT_LEVEL_2 },
	{ 11, CPUFREQ_TABLE_END},
};

struct cpufreq_frequency_table ha7_freq_table_1495MHz[] = {
	{ 0,  1495000},
	{ 1,  1401563},
	{ 2,  1308125},
	{ 3,  1214688},
	{ 4,  1121250},
	{ 5,  1027813},
	{ 6,   934375},
	{ 7,   840938},
	{ 8,   747500},
	{ 9,   654063},
	{10,   CPUFREQ_TABLE_END},
};

struct voltage_table ha7_voltage_1495MHZ[] = {
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
	{ 10, CPUFREQ_TABLE_END},
};

struct cpufreq_frequency_table ha7_freq_table_1599MHz[] = {
	{ 0,  1599000},
	{ 1,  1499063},
	{ 2,  1399125},
	{ 3,  1299188},
	{ 4,  1199250},
	{ 5,  1099313},
	{ 6,   999375},
	{ 7,   899438},
	{ 8,   799500},
	{ 9,   699563},
	{10,   CPUFREQ_TABLE_END},
};

struct voltage_table ha7_voltage_1599MHZ[] = {
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
	{ 10, CPUFREQ_TABLE_END},
};

struct cpufreq_frequency_table ha7_freq_table_1196MHz[] = {
	{ 0,  1196000},
	{ 1,  1121250},
	{ 2,  1046500},
	{ 3,   971750},
	{ 4,   897000},
	{ 5,   822250},
	{ 6,   747500},
	{ 7,   672750},
	{ 8,   CPUFREQ_TABLE_END},
};

struct voltage_table ha7_voltage_1196MHZ[] = {
	{ 0,  VOLT_LEVEL_3 },
	{ 1,  VOLT_LEVEL_3 },
	{ 2,  VOLT_LEVEL_3 },
	{ 3,  VOLT_LEVEL_3 },
	{ 4,  VOLT_LEVEL_3 },
	{ 5,  VOLT_LEVEL_2 },
	{ 6,  VOLT_LEVEL_2 },
	{ 7,  VOLT_LEVEL_2 },
	{ 8,  CPUFREQ_TABLE_END},
};

struct cpufreq_frequency_table sa7_freq_table_624MHz[] = {
	{ 0,  624000},
//	{ 1,  546000},
//	{ 2,  390000},
//	{ 3,  234000},
	{ 1, CPUFREQ_TABLE_END},
};

struct voltage_table sa7_voltage_624MHZ[] = {
	{ 0,  VOLT_LEVEL_2 },
//	{ 1,  VOLT_LEVEL_2 },
//	{ 2,  VOLT_LEVEL_2 },
//	{ 3,  VOLT_LEVEL_2 },
	{ 1,  CPUFREQ_TABLE_END},
};

#ifdef CONFIG_BL_SWITCHER
extern bool bL_switching_enabled;
#define is_bL_switching_enabled()		bL_switching_enabled
#define set_switching_enabled(x) 		(bL_switching_enabled = (x))
#else
#define is_bL_switching_enabled()		false
#define set_switching_enabled(x) 		do { } while (0)
#endif

struct cpufreq_arm_bL_ops {
	char name[CPUFREQ_NAME_LEN];
	int (*get_transition_latency)(struct device *cpu_dev);

	/*
	 * This must set opp table for cpu_dev in a similar way as done by
	 * of_init_opp_table().
	 */
	int (*init_opp_table)(struct device *cpu_dev);
};

static inline int cpu_to_cluster(int cpu)
{
	return is_bL_switching_enabled() ? MAX_CLUSTERS:
		topology_physical_package_id(cpu);
}
#endif /* CPUFREQ_ARM_BIG_LITTLE_H */
