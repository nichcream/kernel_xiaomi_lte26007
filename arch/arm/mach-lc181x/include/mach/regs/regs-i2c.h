#ifndef __ASM_ARCH_REGS_I2C_H
#define __ASM_ARCH_REGS_I2C_H

#define I2C_CON 				(0x00)
#define I2C_TAR 				(0x04)
#define I2C_DATA_CMD				(0x10)
#define I2C_SS_SCL_HCNT 			(0x14)
#define I2C_SS_SCL_LCNT 			(0x18)
#define I2C_FS_SCL_HCNT 			(0x1c)
#define I2C_FS_SCL_LCNT 			(0x20)
#define I2C_HS_SCL_HCNT 			(0x24)
#define I2C_HS_SCL_LCNT 			(0x28)
#define I2C_INTR_STAT				(0x2c)
#define I2C_INTR_EN 				(0x30)
#define I2C_RAW_INTR_STAT			(0x34)
#define I2C_RX_TL				(0x38)
#define I2C_TX_TL				(0x3c)
#define I2C_CLR_INTR				(0x40)
#define I2C_CLR_RX_UNDER			(0x44)
#define I2C_CLR_RX_OVER 			(0x48)
#define I2C_CLR_TX_OVER 			(0x4c)
#define I2C_CLR_RD_REQ				(0x50)
#define I2C_CLR_TX_ABRT 			(0x54)
#define I2C_CLR_ACTIVITY			(0x5c)
#define I2C_CLR_STOP_DET			(0x60)
#define I2C_CLR_START_DET			(0x64)
#define I2C_CLR_GEN_CALL			(0x68)
#define I2C_ENABLE				(0x6c)
#define I2C_STATUS				(0x70)
#define I2C_TXFLR				(0x74)
#define I2C_RXFLR				(0x78)
#define I2C_SDA_HOLD				(0x7c)
#define I2C_TX_ABRT_SOURCE			(0x80)

/* I2C_DATA_CMD. */
#define I2C_DATA_CMD_RESTART			(1 << 10)
#define I2C_DATA_CMD_STOP			(1 << 9)
#define I2C_DATA_CMD_READ			(1 << 8)

/* I2C_CON. */
#define I2C_CON_NOMEANING			(1 << 6)
#define I2C_CON_RESTART_EN			(1 << 5)
#define I2C_CON_10BIT_ADDR			(1 << 4)
#define I2C_CON_7BIT_ADDR			(0 << 4)
#define I2C_CON_HIGH_SPEED_MODE			(3 << 1)
#define I2C_CON_FAST_MODE			(2 << 1)
#define I2C_CON_STANDARD_MODE			(1 << 1)
#define I2C_CON_MASTER_MODE			(1 << 0)

/* I2C_STATUS. */
#define I2C_STATUS_RX_FIFO_FULL			(1 << 4)
#define I2C_STATUS_RX_FIFO_NOT_EMPTY		(1 << 3)
#define I2C_STATUS_TX_FIFO_EMPTY		(1 << 2)
#define I2C_STATUS_TX_FIFO_NOT_FULL		(1 << 1)
#define I2C_STATUS_ACTIVITY			(1 << 0)

/* I2C_INTR_EN & I2C_INTR_STAT & I2C_RAW_INTR_STAT. */
#define I2C_INTR_GEN_CALL			(1 << 11)
#define I2C_INTR_START_DET			(1 << 10)
#define I2C_INTR_STOP_DET			(1 << 9)
#define I2C_INTR_ACTIVITY			(1 << 8)
#define I2C_INTR_TX_ABORT			(1 << 6)
#define I2C_INTR_TX_EMPTY			(1 << 4)
#define I2C_INTR_TX_OVER			(1 << 3)
#define I2C_INTR_RX_FULL			(1 << 2)
#define I2C_INTR_RX_OVER			(1 << 1)
#define I2C_INTR_RX_UNDER			(1 << 0)

#endif /* __ASM_ARCH_REGS_I2C_H */
