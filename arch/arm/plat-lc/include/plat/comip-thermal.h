/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the temperature monitor.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2012-06-29	xuxuefeng		created
 *
 */
#ifndef __COMIP_THERMAL_H__
#define __COMIP_THERMAL_H__

#define MONITOR_SAMPLE_UNIT			   (10)	/*10s*/

enum thermal_level {
	THERMAL_NORMAL = 0,
	THERMAL_WARM,
	THERMAL_HOT,
	THERMAL_TORRID,
	THERMAL_CRITICAL,
	THERMAL_FATAL,
	THERMAL_ERROR,
	THERMAL_END,
};

enum sample_rate {
	SAMPLE_RATE_SLOW = 12 * MONITOR_SAMPLE_UNIT,
	SAMPLE_RATE_NORMAL = 6 * MONITOR_SAMPLE_UNIT,
	SAMPLE_RATE_FAST = 3 * MONITOR_SAMPLE_UNIT,
	SAMPLE_RATE_EMERGENT = MONITOR_SAMPLE_UNIT,
	SAMPLE_RATE_ERROR = 0,
};


struct sample_rate_member {
	short temp_level_bottom;
	short temp_level_top;
	enum  sample_rate rate;
	enum  thermal_level level;
};

struct comip_thermal_platform_data {
	unsigned short channel;
	unsigned short sample_table_size;
	struct sample_rate_member *sample_table;
};


struct defeedback {
	int enable;
	int  limit_percent; /*lower limit percent , unit 1/10000, eg. 625 mean 625/1000 = 1/16 */
	int  target_temperature;
};

struct thermal_data {
	int temperature;
	struct defeedback dfb_cpu;
	struct defeedback dfb_gpu;
};

#ifdef CONFIG_THERMAL_COMIP
extern int thermal_notifier_register(struct notifier_block *nb);
extern int thermal_notifier_unregister(struct notifier_block *nb);
extern void comip_set_thermal_info(struct comip_thermal_platform_data *info);
#else
static inline int thermal_notifier_register(struct notifier_block *nb) { return 0; };
static inline int thermal_notifier_unregister(struct notifier_block *nb) { return 0; };
static inline void  comip_set_thermal_info(struct comip_thermal_platform_data *info) { return; };
#endif

#endif/* __COMIP_THERMAL_H__ */
