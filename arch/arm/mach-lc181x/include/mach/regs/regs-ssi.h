#ifndef __ASM_ARCH_REGS_SSI_H
#define __ASM_ARCH_REGS_SSI_H

#define SSI_CTRL0			(0x00)
#define SSI_CTRL1			(0x04)
#define SSI_EN				(0x08)
#define SSI_SE				(0x10)
#define SSI_BAUD			(0x14)
#define SSI_TXFTL			(0x18)
#define SSI_RXFTL			(0x1c)
#define SSI_TXFL			(0x20)
#define SSI_RXFL			(0x24)
#define SSI_STS				(0x28)
#define SSI_IE				(0x2c)
#define SSI_IS				(0x30)
#define SSI_RIS				(0x34)
#define SSI_TXOIC			(0x38)
#define SSI_RXOIC			(0x3c)
#define SSI_RXUIC			(0x40)
#define SSI_IC				(0x48)
#define SSI_DMAC			(0x4c)
#define SSI_DMATDL			(0x50)
#define SSI_DMARDL			(0x54)
#define SSI_DATA			(0x60)

/* SSI_CTRL0. */
#define SSI_SRL				(1 << 11)
#define SSI_RTX				(0 << 8)
#define SSI_TX				(1 << 8)
#define SSI_RX				(2 << 8)
#define SSI_CPOL			(1 << 7)
#define SSI_CPHA			(1 << 6)
#define SSI_MOTOROLA_SPI		(0 << 4)
#define SSI_TI_SSP			(1 << 4)
#define SSI_RTX_BIT			(8)
#define SSI_RTX_MASK			(0x3)
#define SSI_DFS_BIT			(0)
#define SSI_DFS_MASK			(0xf)

/* SSI_RIS. */
#define SSI_RFF				(1 << 4)
#define SSI_RFNE			(1 << 3)
#define SSI_TFE				(1 << 2)
#define SSI_TFNF			(1 << 1)
#define SSI_BUSY			(1 << 0)

#endif /* __ASM_ARCH_REGS_SSI_H */
