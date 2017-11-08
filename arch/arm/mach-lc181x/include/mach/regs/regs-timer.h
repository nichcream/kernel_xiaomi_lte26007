#ifndef __ASM_ARCH_REGS_TIMER_H
#define __ASM_ARCH_REGS_TIMER_H

/* Timer number. */
#if defined(CONFIG_CPU_LC1810)
#define COMIP_TIMER_NUM				(4)
#elif defined(CONFIG_CPU_LC1813)
#define COMIP_TIMER_NUM				(7)
#endif

/* Timer registers. */
#define COMIP_TIMER_TLC(nr)			(TIMER_BASE + (nr) * 0x14)
#define COMIP_TIMER_TCV(nr)			(TIMER_BASE + 0x04 + (nr) * 0x14)
#define COMIP_TIMER_TCR(nr)			(TIMER_BASE + 0x08 + (nr) * 0x14)
#define COMIP_TIMER_TIC(nr)			(TIMER_BASE + 0x0C + (nr) * 0x14)
#define COMIP_TIMER_TIS(nr)			(TIMER_BASE + 0x10 + (nr) * 0x14)

/* TIMERx_TCR bits. */
#define TIMER_TIM				(1 << 2)
#define TIMER_TMS				(1 << 1)
#define TIMER_TES				(1 << 0)

#endif /* __ASM_ARCH_REGS_TIMER_H */