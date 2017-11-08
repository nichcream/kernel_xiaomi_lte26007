#if !defined(__ASM_ARCH_RTK_FM_H)
#define __ASM_ARCH_RTK_FM_H

struct rtk_fm_platform_data {
	int fm_en;
	void (*powersave)(int on);
};

#endif /* __ASM_ARCH_RTK_FM_H */
