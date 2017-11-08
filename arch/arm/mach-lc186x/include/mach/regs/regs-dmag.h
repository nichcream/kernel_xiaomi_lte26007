#ifndef __ASM_ARCH_REGS_AP_DMAG_H
#define __ASM_ARCH_REGS_AP_DMAG_H

#define DMAG_CH_STATUS 			(0x00)
#define DMAG_CH_PRIOR0			(0x20)
#define DMAG_CH_PRIOR1			(0x24)
#define DMAG_CH_PRIOR2			(0x28)
#define DMAG_INTR_EN0			(0x40)
#define DMAG_INTR_MASK0			(0x44)
#define DMAG_INTR_STATUS0		(0x48)
#define DMAG_CH_LP0	 		(0x80)
#define DMAG_BUS_LP0	 		(0x88)

#define DMAG_RAM_ADDR_BOUNDARY(id)	(0xA0 + (id) * 0x04)
#define DMAG_CH_CTL(id)			(0x100 + (id) * 0x40)
#define DMAG_CH_CONFIG(id)		(0x104 + (id) * 0x40)
#define DMAG_CH_SRC_ADDR(id)		(0x108 + (id) * 0x40)
#define DMAG_CH_DST_ADDR(id)		(0x110 + (id) * 0x40)
#define DMAG_CH_SIZE(id)		(0x118 + (id) * 0x40)
#define DMAG_CH_LINK_ADDR(id)		(0x124 + (id) * 0x40)
#define DMAG_CH_LINK_NUM(id)		(0x128 + (id) * 0x40)
#define DMAG_CH_INTR_EN(id)		(0x12C + (id) * 0x40)
#define DMAG_CH_INTR_STATUS(id)		(0x130 + (id) * 0x40)
#define DMAG_CH_INTR_RAW(id)		(0x134 + (id) * 0x40)
#define DMAG_CH_MONITOR_CTRL(id)	(0x138 + (id) * 0x40)
#define DMAG_CH_MONITOR_OUT(id)		(0x13C + (id) * 0x40)

/* DMAG_CH_CTL */
#define DMAG_FIFO_RESET			(3)
#define DMAG_PAUSE			(2)
#define DMAG_CLOSE			(1)
#define DMAG_ENABLE			(0)

/* DMAG_CH_SIZE */
#define DMAG_X_SIZE			(0)

/* DMAG_CH_CONFIG */
#define DMAG_SRC_TYPE			(2)
#define DMAG_MODE			(0)

#endif /* __ASM_ARCH_REGS_AP_DMAG_H */
