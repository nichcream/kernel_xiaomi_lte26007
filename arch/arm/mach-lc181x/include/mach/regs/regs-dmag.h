#ifndef __ASM_ARCH_REGS_AP_DMAG_H
#define __ASM_ARCH_REGS_AP_DMAG_H

#define DMAG_CH_STATUS 			(0x00)
#define DMAG_CH03_PRIOR			(0x04)
#define DMAG_INTR_EN_AP			(0x0C)
#define DMAG_INTR_STATUS_AP		(0x10)
#define DMAG_INTR_MASK_AP		(0x1C)
#define DMAG_CH_LP_EN	 		(0x4C)
#define DMAG_AHB_LP_EN	 		(0x50)

#define DMAG_CH_CTL(id)			(0x100 + (id) * 0x40)
#define DMAG_CH_SIZE(id)		(0x104 + (id) * 0x40)
#define DMAG_CH_PARA(id)		(0x108 + (id) * 0x40)
#define DMAG_CH_SRC_ADDR(id)		(0x10C + (id) * 0x40)
#define DMAG_CH_DST_ADDR(id)		(0x110 + (id) * 0x40)
#define DMAG_CH_INTR_EN(id)		(0x114 + (id) * 0x40)
#define DMAG_CH_INTR_STATUS(id)		(0x118 + (id) * 0x40)
#define DMAG_CH_INTR_RAW(id)		(0x11C + (id) * 0x40)
#define DMAG_CH_CUR_SRC_ADDR(id)	(0x120 + (id) * 0x40)
#define DMAG_CH_CUR_DST_ADDR(id)	(0x124 + (id) * 0x40)
#define DMAG_CH_SPACE(id)		(0x128 + (id) * 0x40)

/* DMAG_CH_CTL */
#define DMAG_FIFO_RESET			(2)
#define DMAG_CFG_MODE			(1)
#define DMAG_CH_EN			(0)

/* DMAG_CH_SIZE */
#define DMAG_Y_SIZE			(16)
#define DMAG_X_SIZE			(0)

/* DMAG_CH_PARA */
#define DMAG_SRC_TYPE			(2)
#define DMAG_DST_TYPE			(1)
#define DMAG_LINK_ADDR			(0)

#endif /* __ASM_ARCH_REGS_AP_DMAG_H */
