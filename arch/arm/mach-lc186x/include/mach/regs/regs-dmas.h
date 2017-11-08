#ifndef __ASM_ARCH_REGS_DMAS_H
#define __ASM_ARCH_REGS_DMAS_H

#define DMAS_EN 			(0x00)
#define DMAS_CLR			(0x04)
#define DMAS_STA			(0x08)
#define DMAS_INT_RAW0		 	(0x0c)
#define DMAS_INT_EN0			(0x10)
#define DMAS_INT_EN0_TOP		(0x14)
#define DMAS_INT0			(0x1c)
#define DMAS_INT0_TOP			(0x20)
#define DMAS_INT_CLR0		 	(0x28)
#define DMAS_INTV_UNIT		 	(0x2c)
#define DMAS_INT_RAW1			(0x30c)
#define DMAS_INT_EN1			(0x310)
#define DMAS_INT_EN1_TOP		(0x314)
#define DMAS_INT1			(0x31c)
#define DMAS_INT1_TOP			(0x320)
#define DMAS_INT_CLR1			(0x328)
#define DMAS_LP_CTL 		 	(0x3fc)

#define DMAS_CH_REG(module, index)	(module->base + 0x40 + (index) * 0x20)
#define DMAS_CH_SAR(dmas)		io_p2v(dmas->base)
#define DMAS_CH_DAR(dmas)		io_p2v(dmas->base + 0x4)
#define DMAS_CH_CTL0(dmas)		io_p2v(dmas->base + 0x8)
#define DMAS_CH_CTL1(dmas)		io_p2v(dmas->base + 0xC)
#define DMAS_CH_CA(dmas)		io_p2v(dmas->base + 0x10)
#define DMAS_CH_INTA(dmas)		io_p2v(dmas->base + 0x14)
#define DMAS_CH_WD(module, index)	io_p2v(module->base + 0x240 + (index) * 4)

#define DMAS_REG(module, offset)	io_p2v(module->base + offset)

#endif /* __ASM_ARCH_REGS_DMAS_H */
