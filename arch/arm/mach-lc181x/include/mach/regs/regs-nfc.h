#ifndef __ASM_ARCH_REGS_NFC_H
#define __ASM_ARCH_REGS_NFC_H

#define NFC_CTRLR0				(NFC_BASE + 0x00)
#define NFC_TIMR0				(NFC_BASE + 0x04)
#define NFC_TIMR1				(NFC_BASE + 0x08)
#define NFC_TX_BUF				(NFC_BASE + 0x0C)
#define NFC_RX_BUF				(NFC_BASE + 0x10)
#define NFC_STA_SEQ				(NFC_BASE + 0x14)
#define NFC_INTR_EN				(NFC_BASE + 0x18)
#define NFC_INTR_STAT				(NFC_BASE + 0x1C)
#define NFC_INTR_RAW_STAT			(NFC_BASE + 0x20)
#define NFC_ECC_CODE0				(NFC_BASE + 0x24)
#define NFC_ECC_CODE1				(NFC_BASE + 0x28)
#define NFC_ECC_CODE2				(NFC_BASE + 0x2C)
#define NFC_ECC_CODE3				(NFC_BASE + 0x30)
#define NFC_CS					(NFC_BASE + 0x38)
#define NFC_CMD					(NFC_BASE + 0x3C)
#define NFC_CMD_STA				(NFC_BASE + 0x40)
#define NFC_ADDR32				(NFC_BASE + 0x44)
#define NFC_ADDR8				(NFC_BASE + 0x48)
#define NFC_WLEN				(NFC_BASE + 0x4C)
#define NFC_RLEN				(NFC_BASE + 0x50)
#define NFC_NOP					(NFC_BASE + 0x54)
#define NFC_ADDR_NUM				(NFC_BASE + 0x58)
#define NFC_RB_STATUS				(NFC_BASE + 0x5C)
#define NFC_PARITY_EN				(NFC_BASE + 0x60)
#define NFC_BCH_CFG				(NFC_BASE + 0x64)
#define NFC_BCH_ERR_MODE			(NFC_BASE + 0x68)
#define NFC_BCH_RST				(NFC_BASE + 0x6C)
#define NFC_LP_CTRL				(NFC_BASE + 0x70)
#define NFC_NOP_INTVAL				(NFC_BASE + 0x74)
#define NFC_AUTO_EC_BASE_ADDR			(NFC_BASE + 0x78)
#define NFC_SEC_ADDR_32				(NFC_BASE + 0x7C)
#define NFC_SEC_ADDR_8				(NFC_BASE + 0x80)
#define NFC_SEC_ADDR_MASK_32			(NFC_BASE + 0x84)
#define NFC_SEC_ADDR_MASK_8			(NFC_BASE + 0x88)
#define NFC_SEC_EN				(NFC_BASE + 0x8C)
#define NFC_BCH_RAM				(NFC_BASE + 0x200)

/* Register CTRLR0. */
#define NFC_CTRLR0_HPI_BYPASS			(1 << 11)
#define NFC_CTRLR0_ECC_FIX_ERROR_EN		(1 << 10)
#define NFC_CTRLR0_ECC_TYPE			(1 << 8)
#define NFC_CTRLR0_ECC_EN			(1 << 7)
#define NFC_CTRLR0_NFC_WIDTH			(1 << 6)
#define NFC_CTRLR0_EN				(1 << 5)
#define NFC_CTRLR0_WP				(1 << 4)
#define NFC_CTRLR0_TFLUSH			(1 << 3)
#define NFC_CTRLR0_RFLUSH			(1 << 2)
#define NFC_CTRLR0_STASTP1			(1 << 1)
#define NFC_CTRLR0_STASTP0			(1 << 0)

/* Register TIMER0. */
#define NFC_TIMER0_TWCH				(1 << 6)
#define NFC_TIMER0_TCWS				(1 << 4)
#define NFC_TIMER0_TWH				(1 << 2)
#define NFC_TIMER0_TWP				(1 << 0)

/* Register TIMER1. */
#define NFC_TIMER0_TRCH				(1 << 6)
#define NFC_TIMER0_TCRS				(1 << 4)
#define NFC_TIMER0_TRH				(1 << 2)
#define NFC_TIMER0_TRP				(1 << 0)

/* Register STA_SEQ. */
#define NFC_STA_SEQ_SEQ_NUM			(1 << 8)
#define NFC_STA_SEQ_STA_UWP			(1 << 7)
#define NFC_STA_SEQ_STA_READY1			(1 << 6)
#define NFC_STA_SEQ_STA_READY0			(1 << 5)
#define NFC_STA_SEQ_STA_ERROR1			(1 << 1)
#define NFC_STA_SEQ_STA_ERROR0			(1 << 0)

/* Register INTR_EN, INTR_STAT, INTR_RAW_STAT. */
#define NFC_INT_NO_LEGAL_AC_EN			(1 << 22)
#define NFC_INT_ECC_BUS_ERR_EN			(1 << 21)
#define NFC_INT_RB_UP_EN			(1 << 20)
#define NFC_INT_STA_SEQ_VALID_EN		(1 << 19)
#define NFC_INT_BCH_DEC_DONE_EN			(1 << 18)
#define NFC_INT_BCH_ENC_DONE_EN			(1 << 17)
#define NFC_INT_ECC_VALID3			(1 << 16)
#define NFC_INT_ECC_VALID2			(1 << 15)
#define NFC_INT_ECC_VALID1			(1 << 14)
#define NFC_INT_ECC_VALID0			(1 << 13)
#define NFC_INT_NOP				(1 << 12)
#define NFC_INT_ST1				(1 << 11)
#define NFC_INT_ST0				(1 << 10)
#define NFC_INT_TNF				(1 << 9)
#define NFC_INT_THE				(1 << 8)
#define NFC_INT_TFU				(1 << 7)
#define NFC_INT_TEM				(1 << 6)
#define NFC_INT_TOV				(1 << 5)
#define NFC_INT_RNE				(1 << 4)
#define NFC_INT_RHE				(1 << 3)
#define NFC_INT_RFU				(1 << 2)
#define NFC_INT_REM				(1 << 1)
#define NFC_INT_ROV				(1 << 0)

/* Register CMD_STA. */
#define NFC_CMD_STA_RB_EN			(1 << 7)
#define NFC_CMD_STA_SEQ_NUM			(1 << 0)

/* Register NOP. */
#define NFC_NOP_IRQ				(1 << 8)
#define NFC_NOP_CLK_NUM				(1 << 0)

/* Register RB_STATUS. */
#define NFC_RB_STATUS_RDY			(1 << 0)

/* Register INTR_EN, INTR_STAT, INTR_RAW_STAT value. */
#define NFC_INT_ECC				(0xf << 13)

/* Register BCH_CFG. */
#define NFC_PAGE_SIZE				(1 << 8)
#define NFC_PARITY_MATCH			(1 << 0)

/* Register BCH_ERR_MODE. */
#define NFC_BCH_ERR_MODE_INVALID		(1 << 8)
#define NFC_BCH_ERR_MODE_BIT			(1 << 0)

/* Register NFC_LP_CTRL. */
#define NFC_IDLE_LIMIT				(1 << 8)
#define NFC_LP_EN				(1 << 1)
#define NFC_ECC_LP_EN				(1 << 0)

#endif /* __ASM_ARCH_REGS_NFC_H */
