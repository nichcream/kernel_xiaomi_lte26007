#ifndef __ASM_ARCH_PLAT_DMAS_H
#define __ASM_ARCH_PLAT_DMAS_H

/* IRQ types. */
/* Block transfer complete. */
#define DMAS_INT_DONE			(0x00000001)

/*
 * In send mode : half block transfer complete.
 * In receive mode : flush complete.
 */
#define DMAS_INT_HBLK_FLUSH		(0x00000002)

#if defined(CONFIG_DMAS2_COMIP)
#define DMAS_INT_MATCH			(0x00000004)
#endif

/* Config flag values. */
#define DMAS_CFG_BLOCK_SIZE		(0x00000001)
#define DMAS_CFG_SRC_ADDR		(0x00000002)
#define DMAS_CFG_DST_ADDR		(0x00000004)
#define DMAS_CFG_PRIORITY		(0x00000008)
#define DMAS_CFG_BUS_WIDTH		(0x00000010)
#define DMAS_CFG_TX_TRANS_MODE		(0x00000020)
#define DMAS_CFG_TX_FIX_VALUE		(0x00000040)
#define DMAS_CFG_TX_BLOCK_MODE		(0x00000080)
#define DMAS_CFG_RX_TRANS_TYPE		(0x00000100)
#define DMAS_CFG_RX_TIMEOUT		(0x00000200)
#define DMAS_CFG_IRQ_EN			(0x00000400)
#define DMAS_CFG_IRQ_HANDLER		(0x00000800)
#define DMAS_CFG_IRQ_DATA		(0x00001000)
#if defined(CONFIG_DMAS2_COMIP)
#define DMAS_CFG_MATCH_ADDR		(0x00002000)
#endif
#define DMAS_CFG_ALL			(0x0000ffff)

/* Channel ID. */
enum {
	DMAS_CH0 = 0,
	DMAS_CH1,
	DMAS_CH2,
	DMAS_CH3,
	DMAS_CH4,
	DMAS_CH5,
	DMAS_CH6,
	DMAS_CH7,
	DMAS_CH8,
	DMAS_CH9,
	DMAS_CH10,
	DMAS_CH11,
	DMAS_CH12,
	DMAS_CH13,
	DMAS_CH14,
	DMAS_CH15,
#if defined(AUDIO_DMAS_BASE)
	AUDIO_DMAS_CH0,
	AUDIO_DMAS_CH1,
	AUDIO_DMAS_CH2,
	AUDIO_DMAS_CH3,
	AUDIO_DMAS_CH4,
	AUDIO_DMAS_CH5,
	AUDIO_DMAS_CH6,
	AUDIO_DMAS_CH7,
	AUDIO_DMAS_CH8,
	AUDIO_DMAS_CH9,
	AUDIO_DMAS_CH10,
	AUDIO_DMAS_CH11,
	AUDIO_DMAS_CH12,
	AUDIO_DMAS_CH13,
	AUDIO_DMAS_CH14,
	AUDIO_DMAS_CH15,
#endif
#if defined(TOP_DMAS_BASE)
	TOP_DMAS_CH0,
	TOP_DMAS_CH1,
	TOP_DMAS_CH2,
	TOP_DMAS_CH3,
	TOP_DMAS_CH4,
	TOP_DMAS_CH5,
	TOP_DMAS_CH6,
	TOP_DMAS_CH7,
	TOP_DMAS_CH8,
	TOP_DMAS_CH9,
	TOP_DMAS_CH10,
	TOP_DMAS_CH11,
	TOP_DMAS_CH12,
	TOP_DMAS_CH13,
	TOP_DMAS_CH14,
	TOP_DMAS_CH15,
#endif
	DMAS_CH_MAX
};

/* DMAS channel priority. */
enum {
	DMAS_CH_PRI_DEFAULT = 2,
	DMAS_CH_PRI_2 = DMAS_CH_PRI_DEFAULT,
	DMAS_CH_PRI_3,
	DMAS_CH_PRI_4,
	DMAS_CH_PRI_5,
	DMAS_CH_PRI_6,
	DMAS_CH_PRI_7,
	DMAS_CH_PRI_8,
	DMAS_CH_PRI_9,
	DMAS_CH_PRI_10,
	DMAS_CH_PRI_11,
	DMAS_CH_PRI_12,
	DMAS_CH_PRI_13,
	DMAS_CH_PRI_14,
	DMAS_CH_PRI_15,
	DMAS_CH_PRI_MAX
};

/* DMAS bit width of peripheral. */
enum {
	DMAS_DEV_WIDTH_8BIT = 0,
	DMAS_DEV_WIDTH_16BIT,
	DMAS_DEV_WIDTH_32BIT,
	DMAS_DEV_WIDTH_MAX
};

/* Tx Transfer block mode. */
enum {
	DMAS_SINGLE_BLOCK = 0,
	DMAS_MULTI_BLOCK,
	DMAS_BLOCK_MODE_MAX
};

/* Tx transfer mode.*/
enum {
	DMAS_TRANS_NORMAL = 0,
	DMAS_TRANS_SPECIAL,
	DMAS_TRANS_MODE_MAX
};

/* Rx transfer type.*/
enum {
	DMAS_TRANS_BLOCK = 0,
	DMAS_TRANS_WRAP,
	DMAS_TRANS_TYPE_MAX
};

struct dmas_ch_cfg {
	/* Config flags. */
	unsigned int flags;

	/*
	 * Max size is 64KB
	 * if this channel's transfer type is DMAS_TRANS_BLOCK,
	 * this size means transfer size; if DMAS_TRANS_UNDEF,
	 * and circle buf is enable, it means circle buf size,
	 * and it should be align with 64Byte.
	 */
	unsigned int block_size;

	/* Source address which should be align with 4-byte. */
	unsigned int src_addr;

	/* Destination address which should be align with 4-byte. */
	unsigned int dst_addr;

#if defined(CONFIG_DMAS2_COMIP)
	/* Match address which should be align with 4-byte. */
	unsigned int match_addr;
#endif

	/* Channel priority. */
	int priority;

	/* Transfer burst width, which should be equal to FIFO width. */
	int bus_width;

	/* For tx channel only. Transfer mode, normal or special. */
	int tx_trans_mode;

	/* For tx channel only. Transfer fix value in special transfer mode. */
	int tx_fix_value;

	/* For tx channel only. Transfer block mode, single or multi. */
	int tx_block_mode;

	/* For rx channel only. Transfer type, block or undefined size. */
	int rx_trans_type;

	/*
	 * For rx channel only. Timeout counter base on 1us, when timeout, DMAS will
	 * do flush; if this value==0 that means disable
	 * timeout. MAX=0xFFF.
	 */
	int rx_timeout;

	/* IRQ. */
	int irq_en;
	void (*irq_handler)(int ch, int type, void *data);
	void *irq_data;
};

/* DMAS Function Declarations. */
extern int comip_dmas_request(const char *name, unsigned int ch);
extern int comip_dmas_free(unsigned int ch);
extern int comip_dmas_config(unsigned int ch, struct dmas_ch_cfg *cfg);
extern int comip_dmas_get(unsigned int ch, unsigned int *addr);
extern int comip_dmas_state(unsigned int ch);
extern int comip_dmas_start(unsigned int ch);
extern int comip_dmas_stop(unsigned int ch);
extern int comip_dmas_intr_enable(unsigned int ch, unsigned int intr);
extern int comip_dmas_intr_disable(unsigned int ch, unsigned int intr);
extern void comip_dmas_dump(unsigned int ch);

#endif /* __ASM_ARCH_PLAT_DMAS_H */
