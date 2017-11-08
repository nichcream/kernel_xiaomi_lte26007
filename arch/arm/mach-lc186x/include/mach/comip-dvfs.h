#ifndef __ASM_ARCH_LC186X_DVFS_H
#define __ASM_ARCH_LC186X_DVFS_H
#define DVS_DELAY 100
enum{
	GPU_CORE,
	CPU_CORE,
};

extern void set_cur_voltage_level(unsigned int core, unsigned int level);
extern  unsigned int get_cur_voltage_level(unsigned int core);
#endif
