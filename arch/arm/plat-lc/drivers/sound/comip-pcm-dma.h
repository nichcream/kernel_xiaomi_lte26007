/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2009  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the dma driver headfile.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date			Author		Description
 *	 1.0.0	2015-02-27	yangzhou 	created
 *
 */
#ifndef __COMIP_PCM_DMA_H__
#define __COMIP_PCM_DMA_H__

#include <sound/pcm.h>

struct comip_pcm_dma_params {
	char *name;			/* stream identifier */
	int channel;			/* DMA channel */
	u32 saddr;			/* device source physical address for DMA  (pcm rx fifo address)*/
	u32 daddr;			/* device destination physical address for DMA (pcm tx fifo address) */
};

struct comip_pcm_data {
	spinlock_t	lock;
	void __iomem *reg_base;
	struct clk *clk;
	int master;
	u32 fmt;
	u32 baddr;			/* physical adderss to write  playback data*/
};

extern int leadcore_asoc_pcm_dma_register(struct device *dev);
extern void leadcore_asoc_pcm_dma_unregister(struct device *dev);

#endif
