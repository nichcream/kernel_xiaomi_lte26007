#ifndef __ASM_ARCH_COMIP_METS_H
#define __ASM_ARCH_COMIP_METS_H

#if defined(CONFIG_METS_COMIP) 
extern int comip_mets_config(int temperature_low, int temperature_high);
extern int mets_notifier_register(struct notifier_block *nb);
extern int mets_notifier_unregister(struct notifier_block *nb);
extern int comip_mets_get_temperature(void);
#else
static inline int comip_mets_get_temperature(){ return -1;}
static inline int comip_mets_config(int temperature_low, int temperature_high){ return -1;}
static inline int mets_notifier_register(struct notifier_block *nb){ return -1;}
static inline int mets_notifier_unregister(struct notifier_block *nb);
#endif

struct comip_mets_platform_data {
	int reserved;
};

extern void comip_set_mets_info(struct comip_mets_platform_data *info);

#endif /* __ASM_ARCH_COMIP_METS_H */
