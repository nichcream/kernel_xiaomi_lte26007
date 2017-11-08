#ifndef __ASM_ARCH_COMIP_MULTICORE_H
#define __ASM_ARCH_COMIP_MULTICORE_H

/*
	CPU0:  AP CPU0
	CPU1:  AP CPU1, we will not use this , use spinlock instead
	CPU2:  CP ARM0
	CPU3:  CP ZSP0
	CPU4:  CP ZSP1
*/
enum {
    MTC_TYPE_MFP,
	MTC_TYPE_DDR,
    MTC_TYPE_MAX
};

extern void comip_mtc_down(int type);
extern void comip_mtc_up(int type);

#endif /* __ASM_ARCH_COMIP_MULTICORE_H */
