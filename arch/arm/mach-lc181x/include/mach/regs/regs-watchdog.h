#ifndef __ASM_ARCH_REGS_WATCHDOG_H
#define __ASM_ARCH_REGS_WATCHDOG_H

#define WDT_CR			(0x00)
#define WDT_TORR		(0x04)
#define WDT_CCVR 		(0x08)
#define WDT_CRR			(0x0C)
#define WDT_STAT		(0x10)
#define WDT_ICR			(0x14)

/* WDT_CR. */
#define WDT_ENABLE		(1 << 0)

/* WDT_TORR. */
#define WDT_TIMEOUT_MAX		(0x0f)

/* WDT_CRR. */
#define WDT_RESET_VAL		(0x76)

#endif /* __ASM_ARCH_REGS_WATCHDOG_H */
