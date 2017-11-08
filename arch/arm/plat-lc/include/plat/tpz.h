#ifndef __ASM_ARCH_COMIP_TPZ_H
#define __ASM_ARCH_COMIP_TPZ_H

#define TPZ_MAX_BLOCKS	(2048 - 1)
#define TPZ_BLOCK_SIZE	(0x100000)

enum {
#if defined(CONFIG_ARCH_LC181X)
	TPZ_CP,
#elif defined(CONFIG_ARCH_LC186X)
	TPZ_TOP0,
	TPZ_TOP1,
	TPZ_TOP2,
	TPZ_TOP3,
	TPZ_CP0,
	TPZ_CP1,
	TPZ_CP2,
	TPZ_CP3,
#else
	#error unknown arch
#endif
	TPZ_AP0,
	TPZ_AP1,
	TPZ_AP2,
	TPZ_AP3,
	TPZ_MAX
};

#define TPZ_ATTR_WRITE	(1 << 0)
#define TPZ_ATTR_READ	(1 << 1)
#define TPZ_ATTR_HW	(1 << 2)
#define TPZ_ATTR_MASK	(0x7)

#if defined(CONFIG_TPZ_COMIP) || defined(CONFIG_TPZ2_COMIP)
extern int comip_tpz_enabe(int id, int enable);
extern int comip_tpz_config(int id, int attr, int start, int end, int def_addr);
#else
static inline int comip_tpz_enabe(int id, int enable){return -1;}
static inline int comip_tpz_config(int id, int attr, int start, int end, int def_addr){return -1;}
#endif

struct comip_tpz_platform_data {
	int reserved;
};

extern void comip_set_tpz_info(struct comip_tpz_platform_data *info);

#endif /* __ASM_ARCH_COMIP_TPZ_H */
