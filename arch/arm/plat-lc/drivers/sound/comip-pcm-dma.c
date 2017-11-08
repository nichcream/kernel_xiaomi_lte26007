/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2009  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip 1860 sound pcm dma driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date			Author		Description
 *	 1.0.0	2015-02-27	yangzhou 	created
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <asm/dma.h>
#include <plat/hardware.h>
#include <plat/audio.h>
#include <plat/dmas.h>

#include "comip-pcm-dma.h"

static struct dmas_ch_cfg pcm_dmas_config[2];

static const struct snd_pcm_hardware comip_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_MMAP_VALID |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_PAUSE |
	SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
	SNDRV_PCM_FMTBIT_U16_LE |
	SNDRV_PCM_FMTBIT_S8 |
	SNDRV_PCM_FMTBIT_U8,
	.period_bytes_min	= 32,
	.period_bytes_max	= 128 * 1024,
	.periods_min		= 1,
	.periods_max		= 4,
	.buffer_bytes_max	= 128 * 1024,
	.fifo_size		= 64,
};

struct comip_runtime_data {
	int dma_ch;
	struct comip_pcm_dma_params *params;
	int	period_index;
	int	dmas1_index;
	int	off_index;
};

static void comip_pcm_dma_irq(int dma_ch,int type,void *dev_id)
{
	struct snd_pcm_substream *substream = dev_id;
	struct comip_runtime_data *prtd;
	struct snd_pcm_runtime *runtime;
	dma_addr_t paddr;
	int err = 0;

	struct dmas_ch_cfg *config = &pcm_dmas_config[0];

	if((config->tx_block_mode == DMAS_MULTI_BLOCK) &&
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)) {
		snd_pcm_period_elapsed(substream);
		return;
	}

	if(substream->runtime == NULL) {
		printk(KERN_DEBUG "[SND BT] comip_pcm_dma_irq substream->runtime == NULL\n");

		if(substream->stream)
			printk(KERN_DEBUG "[SND BT] comip_pcm_dma_irq substream->stream: %d\n", substream->stream);

		return;
	}

	runtime = substream->runtime;
	prtd = substream->runtime->private_data;
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		config = &pcm_dmas_config[1];
	}

	if ((++(prtd->period_index)) < runtime->periods) {
		paddr = runtime->dma_addr+runtime->period_size*(runtime->frame_bits / 8)*prtd->period_index;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			config->flags = DMAS_CFG_SRC_ADDR;
			config->src_addr = (unsigned int) paddr;

		} else {
			config->flags = DMAS_CFG_DST_ADDR;
			config->dst_addr = (unsigned int) paddr;

		}
	} else {
		prtd->period_index = 0;
		paddr = runtime->dma_addr;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			config->flags = DMAS_CFG_SRC_ADDR;
			config->src_addr = (unsigned int) paddr;

		} else {
			config->flags = DMAS_CFG_DST_ADDR;
			config->dst_addr = (unsigned int) paddr;

		}
	}
	err = comip_dmas_config(dma_ch,config);

	if (err < 0) {
		printk(KERN_ERR "[SND BT] comip_pcm_dma_irq stream comip_dmas_config err: -%d\n", -err);
		return ;
	}

	comip_dmas_start(dma_ch);
	snd_pcm_period_elapsed(substream);
}

static int comip_pcm_hw_params(struct snd_pcm_substream *substream,
                               struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct comip_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct comip_pcm_dma_params *dma = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	size_t totsize = params_buffer_bytes(params);
	int ret;

	/* return if this is a bufferless transfer e.g.
	 * codec <--> BT codec or GSM modem -- lg FIXME
	 */

	if (!dma)
		return 0;

	if (prtd->params == NULL) {
		prtd->params = dma;
		ret = comip_dmas_request(prtd->params->name, dma->channel);
		prtd->dma_ch = dma->channel;
	} else if (prtd->params != dma) {
		ret = comip_dmas_request(prtd->params->name, dma->channel);
		prtd->dma_ch = dma->channel;
	}

	printk(KERN_DEBUG "[SND BT] comip_pcm_hw_params stream: %d, bytes: %d, buffersize: %d, periods: %d, p_bytes: %d, p_size: %d prtd->dma_ch: 0x%x\n", substream->stream, params_buffer_bytes(params), params_buffer_size(params), params_periods(params), params_period_bytes(params), params_period_size(params), prtd->dma_ch);
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totsize;
	prtd->period_index = 0;
	prtd->dmas1_index = 0;
	prtd->off_index = 0;

	return 0;
}

static int comip_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct comip_runtime_data *prtd = substream->runtime->private_data;

	printk(KERN_DEBUG "[SND BT] comip_pcm_hw_free stream: %d, dma: %d\n", substream->stream, prtd->dma_ch);

	if (prtd->dma_ch) {
		comip_dmas_free(prtd->dma_ch);
		snd_pcm_set_runtime_buffer(substream, NULL);
		prtd->dma_ch = 0;
	}

	return 0;
}

static int comip_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct comip_pcm_dma_params *dma = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	struct dmas_ch_cfg *config = &pcm_dmas_config[0];
	int err = 0;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		config = &pcm_dmas_config[1];
	}
	printk(KERN_DEBUG "[SND BT] comip_pcm_prepare stream: %d, rt_dma_addr: 0x%x, runtime: 0x%x, dma_addr: 0x%x\n", substream->stream, (unsigned int)runtime->dma_addr, (unsigned int)runtime, (unsigned int)dma);

	memset(config, 0, sizeof(struct dmas_ch_cfg));
	config->block_size = runtime->period_size*(runtime->frame_bits / 8);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		config->src_addr = runtime->dma_addr;
		config->dst_addr = dma->daddr;
	} else {
		config->src_addr = dma->saddr;
		config->dst_addr = runtime->dma_addr;
	}
	if (runtime->frame_bits == 16) {
		config->bus_width = DMAS_DEV_WIDTH_16BIT;
	} else if (runtime->frame_bits == 32) {
		config->bus_width = DMAS_DEV_WIDTH_32BIT;
	} else if (runtime->frame_bits == 8) {
		config->bus_width = DMAS_DEV_WIDTH_8BIT;
	}

	config->priority = DMAS_CH_PRI_DEFAULT;
	config->flags = DMAS_CFG_ALL;
	config->tx_trans_mode = DMAS_TRANS_NORMAL;
	config->tx_fix_value = 0;
	config->tx_block_mode = DMAS_SINGLE_BLOCK;
	config->rx_trans_type = DMAS_TRANS_BLOCK;
	config->rx_timeout = 0;
	config->irq_en = DMAS_INT_DONE;
	config->irq_handler = comip_pcm_dma_irq;
	config->irq_data = substream;

	//writel(0x0, io_p2v(TOP_DMAS_BASE+DMAS_INT_EN0));
	err = comip_dmas_config(dma->channel,config);
	if (err < 0) {
		printk(KERN_ERR "[SND BT] comip_pcm_prepare stream  %d, err: -%d\n", substream->stream, -err);
		return err;
	}

	return 0;
}

static int comip_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct comip_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	printk(KERN_DEBUG "[SND BT] comip_pcm_trigger stream: %d, to enable or disable dma with cmd: %d, dma_ch: %d\n", substream->stream, cmd, prtd->dma_ch);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		comip_dmas_start(prtd->dma_ch);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		comip_dmas_stop(prtd->dma_ch);
		comip_dmas_intr_disable(prtd->dma_ch, DMAS_INT_DONE);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		comip_dmas_stop(prtd->dma_ch);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		comip_dmas_stop(prtd->dma_ch);
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
		comip_dmas_start(prtd->dma_ch);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		comip_dmas_start(prtd->dma_ch);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t
comip_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct comip_runtime_data *prtd = runtime->private_data;
	u32 ptr;
	dma_addr_t dma_ptr;
	snd_pcm_uframes_t x;

	comip_dmas_get(prtd->dma_ch, &ptr);
	dma_ptr = (dma_addr_t)ptr & ~((u32)0);
	x = bytes_to_frames(runtime, dma_ptr - runtime->dma_addr);
	if (x == runtime->buffer_size)
		x = 0;

	return x;
}

static int comip_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct comip_runtime_data *prtd;
	int ret;

	snd_soc_set_runtime_hwparams(substream, &comip_pcm_hardware);
	printk(KERN_DEBUG "[SND BT] comip_pcm_open stream: %d\n", substream->stream);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	prtd = kzalloc(sizeof(struct comip_runtime_data), GFP_KERNEL);
	if (prtd == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	runtime->private_data = prtd;

	return 0;

out:
	return ret;
}

static int comip_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct comip_runtime_data *prtd = runtime->private_data;

	printk(KERN_DEBUG "[SND BT] comip_pcm_close stream: %d\n", substream->stream);

	kfree(prtd);

	return 0;
}

static int comip_pcm_mmap(struct snd_pcm_substream *substream,
                          struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int status;

	status = dma_mmap_writecombine(substream->pcm->card->dev, vma,
		                               runtime->dma_area,
		                               runtime->dma_addr,
		                               runtime->dma_bytes);

	printk(KERN_DEBUG "[SND BT] comip_pcm_mmap, dma_area: 0x%x, dma_addr: 0x%x, dma_bytes: %d\n", (unsigned int)runtime->dma_area, (unsigned int)runtime->dma_addr, runtime->dma_bytes);
	return status;
}

struct snd_pcm_ops comip_pcm_dma_ops = {
	.open		= comip_pcm_open,
	.close		= comip_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= comip_pcm_hw_params,
	.hw_free	= comip_pcm_hw_free,
	.prepare	= comip_pcm_prepare,
	.trigger	= comip_pcm_trigger,
	.pointer	= comip_pcm_pointer,
	.mmap		= comip_pcm_mmap,
};

static int comip_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = comip_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;

	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
		                                   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	printk(KERN_DEBUG "[SND BT] comip_pcm_preallocate_dma_buffer addr: 0x%x, area: 0x%x, bytes: %d\n", (unsigned int)buf->addr, (unsigned int)buf->area, (unsigned int)buf->bytes);
	return 0;
}

static void comip_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	printk(KERN_DEBUG "[SND BT] comip_pcm_free_dma_buffers\n");
	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
			                      buf->area, buf->addr);
		buf->area = NULL;
	}
}

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
#define COMIP_DMA_BIT_MASK	(DMA_BIT_MASK(64))
#else
#define COMIP_DMA_BIT_MASK	(DMA_BIT_MASK(32))
#endif

static u64 comip_pcm_dmamask = COMIP_DMA_BIT_MASK;

int comip_pcm_dma_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_soc_dai *dai = rtd->codec_dai;

	printk(KERN_DEBUG "[SND BT] comip_pcm_new\n");

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &comip_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = COMIP_DMA_BIT_MASK;

	if (dai->driver->playback.channels_min) {
		ret = comip_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->driver->capture.channels_min) {
		ret = comip_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}

out:
	return ret;
}

static struct snd_soc_platform_driver leadcore_asoc_platform = {
	.ops		= &comip_pcm_dma_ops,
	.pcm_new	= comip_pcm_dma_new,
	.pcm_free	= comip_pcm_free_dma_buffers,
};

int leadcore_asoc_pcm_dma_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &leadcore_asoc_platform);
}
EXPORT_SYMBOL_GPL(leadcore_asoc_pcm_dma_register);

void leadcore_asoc_pcm_dma_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(leadcore_asoc_pcm_dma_unregister);

MODULE_AUTHOR("yangzhou<yangzhou@leadcoretech.com>");
MODULE_DESCRIPTION("lc186x sound pcm dma driver");
MODULE_LICENSE("GPL");

