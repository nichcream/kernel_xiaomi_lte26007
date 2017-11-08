#ifndef __ASM_ARCH_PLAT_MMC_H
#define __ASM_ARCH_PLAT_MMC_H

#include <linux/mmc/host.h>
#include <linux/interrupt.h>

#define MMCF_IGNORE_PM		(0x00000001)
#define MMCF_DMA_COHERENT	(0x00000002)
#define MMCF_8BIT_DATA		(0x00000004)
#define MMCF_USE_POLL		(0x00000008)
#define MMCF_DETECT_WQ		(0x00000010)
#define MMCF_MONITOR_TIMER	(0x00000020)
#define MMCF_SUSPEND_HOST	(0x00000040)
#define MMCF_CMD_WORKQUEUE	(0x00000080)
#define MMCF_ERROR_RECOVERY	(0x00000100)
#define MMCF_KEEP_POWER		(0x00000200)
#define MMCF_SUSPEND_DEVICE	(0x00000400)
#define MMCF_SET_SEG_BLK_SIZE   (0x00000800)
#define MMCF_USE_CMD23   (0x00001000)
#define MMCF_SUSPEND_SDIO  (0x00002000) /* sdio suspend for wifi. */
#define MMCF_CAP2_HS200  (0x00004000) /* support JEDEC 4.5 HS200. */
#define MMCF_UHS_I			(0x00008000)
#define MMCF_IO_POWER_SUPPORT			(0x00010000)
#define MMCF_AUDIO_ON  (0x00010000) /* support for mp3 low power. */
#define MMCF_CAP2_PACKED_CMD  (0x00020000) /* support for EMMC packed cmd. */

struct comip_mmc_platform_data {
	int flags;
	int clk_rate_max;
	int clk_rate;
	int clk_read_delay;
	int clk_write_delay;
	int detect_gpio;	/* detect pin */
	unsigned int ocr_mask;	/* available voltages */
	unsigned long detect_delay;	/* delay in jiffies before detecting cards after interrupt */
	int (*init) (struct device *, irq_handler_t, void *);
	int (*get_ro) (struct device *);
	int (*get_cd) (struct device *);
	void (*setpower) (struct device *, unsigned int);
	void (*exit) (struct device *, void *);
	int (*setflags)(void);

	/* for sdio. */
	irq_handler_t detect_handle;
	void *detect_data;
	int cd;
	unsigned int max_seg_size;	/* see blk_queue_max_segment_size */
	unsigned int max_blk_size;	/* maximum size of one mmc block */
};

#endif/*__ASM_ARCH_PLAT_MMC_H */

