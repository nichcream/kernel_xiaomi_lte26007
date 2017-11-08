#ifndef __ASM_ARCH_REGS_PCM_H
#define __ASM_ARCH_REGS_PCM_H

#define PCM_MODE				(0x00)
#define PCM_SCLK_CFG				(0x04)
#define PCM_SYNC_CFG				(0x08)
#define PCM_EN					(0x0C)
#define PCM_FIFO_STA				(0x10)
#define PCM_REC_FIFO				(0x14)
#define PCM_TRAN_FIFO				(0x18)
#define PCM_INTR_STA_RAW			(0x20)
#define PCM_INTR_STA				(0x24)
#define PCM_INTR_EN 				(0x28)

/* PCM_MODE. */
#define PCM_MODE_PCM_SLOT_NUM			16
#define PCM_MODE_PCM_TIM_MODE			12
#define PCM_MODE_PCM_TRAN_POL			11
#define PCM_MODE_PCM_REC_POL			10
#define PCM_MODE_PCM_SYNC_FORMAT		9
#define PCM_MODE_PCM_DUAL_REC_CH_EN		8
#define PCM_MODE_SCLK_INVERT			7
#define PCM_MODE_DLY_MODE			6
#define PCM_MODE_FLUSH_TRAN_BUF			5
#define PCM_MODE_FLUSH_REC_BUF			4
#define PCM_MODE_REC_EN				2
#define PCM_MODE_TRAN_EN			1
#define PCM_MODE_SLAVE_EN			0

/* PCM_FIFO_STA */
#define PCM_FIFO_STA_IBUF_AF			7
#define PCM_FIFO_STA_IBUF_HF			6
#define PCM_FIFO_STA_IBUF_NE			5
#define PCM_FIFO_STA_IBUF_FL			4
#define PCM_FIFO_STA_OBUF_NF			3
#define PCM_FIFO_STA_OBUF_HE			2
#define PCM_FIFO_STA_OBUF_AE			1
#define PCM_FIFO_STA_OBUF_EM			0

/* PCM_INTR_EN&PCM_INTR_STA. */
#define PCM_IBUF_OV_INTR			1
#define PCM_OBUF_UF_INTR			0

#endif /* __ASM_ARCH_REGS_PCM_H */
