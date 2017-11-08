/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2009  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the pcm driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date			Author		Description
 *	 1.0.0	2015-02-27	yangzhou 	created
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/io.h>

#include <plat/hardware.h>
#include <plat/audio.h>
#include <plat/dmas.h>
#include "comip-pcm-dma.h"

#define COMIP_PCM_CLK_RATE		(25600000)

static struct comip_pcm_dma_params comip_pcm_stereo[2] = {
	{
		.name = "PCM Stereo out",
	},
	{
		.name = "PCM Stereo in",
	}
};

static inline void pcm_writel(struct comip_pcm_data *pcm,
			int offset, unsigned int val)
{
	writel(val, pcm->reg_base + offset);
}

static inline unsigned int pcm_readl(struct comip_pcm_data *pcm,
			int offset)
{
	return readl(pcm->reg_base + offset);
}

static int comip_pcm_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct comip_pcm_data *pcm = dev_get_drvdata(dai->dev);
	int ret = 0;

	pcm->clk = clk_get(dai->dev, "pcm_clk");
	if (IS_ERR(pcm->clk)) {
		dev_err(dai->dev, "get pcm clock failed\n");
		ret = PTR_ERR(pcm->clk);
		goto exit;
	}

	clk_set_rate(pcm->clk, COMIP_PCM_CLK_RATE);
	clk_enable(pcm->clk);

exit:
	return ret;
}

static int comip_pcm_set_fmt(struct snd_soc_dai *dai,
			unsigned int fmt)
{
	struct comip_pcm_data *pcm = dev_get_drvdata(dai->dev);
	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		pcm->fmt = 0;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		pcm->fmt = 1;
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		pcm->master = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		pcm->master = 0;
		break;
	default:
		break;
	}

	return 0;
}

static int comip_pcm_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	cpu_dai->playback_dma_data = &comip_pcm_stereo[0];
	cpu_dai->capture_dma_data = &comip_pcm_stereo[1];

	return 0;
}

static int comip_pcm_trigger(struct snd_pcm_substream *substream, int cmd,
			struct snd_soc_dai *dai)
{
	struct comip_pcm_data *pcm = dev_get_drvdata(dai->dev);
	int ret = 0;
	int val;
	unsigned long flags;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		spin_lock_irqsave(&pcm->lock, flags);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			val = pcm_readl(pcm, PCM_MODE);
			val |= (1 << PCM_MODE_FLUSH_TRAN_BUF);
			pcm_writel(pcm, PCM_MODE, val);

			val = pcm_readl(pcm, PCM_MODE);
			val &= ~(1 << PCM_MODE_FLUSH_TRAN_BUF);
			pcm_writel(pcm, PCM_MODE, val);

			val = pcm_readl(pcm, PCM_MODE);
			val |= (1 << PCM_MODE_PCM_SLOT_NUM) | (1 << PCM_MODE_TRAN_EN) |
				(1 << PCM_MODE_SLAVE_EN);
			pcm_writel(pcm, PCM_MODE, val);

			val = pcm_readl(pcm, PCM_EN);
			val |= (1 << 0);
			pcm_writel(pcm, PCM_EN, val);
		} else {
			val = pcm_readl(pcm, PCM_MODE);
			val |= (1 << PCM_MODE_FLUSH_REC_BUF);
			pcm_writel(pcm, PCM_MODE, val);

			val = pcm_readl(pcm, PCM_MODE);
			val &= ~(1 << PCM_MODE_FLUSH_REC_BUF);
			pcm_writel(pcm, PCM_MODE, val);

			val = pcm_readl(pcm, PCM_MODE);
			val |= (1 << PCM_MODE_PCM_SLOT_NUM) | (1 << PCM_MODE_REC_EN) |
				(1 << PCM_MODE_SLAVE_EN);
			pcm_writel(pcm, PCM_MODE, val);

			val = pcm_readl(pcm, PCM_EN);
			val |= (1 << 0);
			pcm_writel(pcm, PCM_EN, val);
		}
		spin_unlock_irqrestore(&pcm->lock, flags);
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_STOP:
		spin_lock_irqsave(&pcm->lock, flags);
		val = pcm_readl(pcm, PCM_MODE);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			val &= ~(1 << PCM_MODE_TRAN_EN);
		else
			val &= ~(1 << PCM_MODE_REC_EN);
		pcm_writel(pcm, PCM_MODE, val);
		spin_unlock_irqrestore(&pcm->lock, flags);
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void comip_pcm_shutdown(struct snd_pcm_substream *substream,
                                    struct snd_soc_dai *dai)
{
	struct comip_pcm_data *pcm = dev_get_drvdata(dai->dev);
	int val;
	unsigned long flags;

	spin_lock_irqsave(&pcm->lock, flags);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		val = pcm_readl(pcm, PCM_MODE);
		val &= ~(1 << PCM_MODE_TRAN_EN);
		pcm_writel(pcm, PCM_MODE, val);
	} else {
		val = pcm_readl(pcm, PCM_MODE);
		val &= ~(1 << PCM_MODE_REC_EN);
		pcm_writel(pcm, PCM_MODE, val);
	}
	spin_unlock_irqrestore(&pcm->lock, flags);

	clk_disable(pcm->clk);
}

#define COMIP_PCM_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 )

static struct snd_soc_dai_ops comip_pcm_dai_ops = {
	.trigger	= comip_pcm_trigger,
	.hw_params	= comip_pcm_hw_params,
	.set_fmt	= comip_pcm_set_fmt,
	.startup 	= comip_pcm_startup,
	.shutdown 	= comip_pcm_shutdown,
};

static struct snd_soc_dai_driver comip_pcm_dai = {
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_PCM_RATES,
		.formats = SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_PCM_RATES,
		.formats = SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &comip_pcm_dai_ops,
};

static const struct snd_soc_component_driver lc186x_pcm_component = {
	.name		= "comip-pcm",
};

static __init int comip_pcm_probe(struct platform_device *pdev)
{
	struct comip_pcm_data *pcm;
	struct resource *mmres;
	struct resource *dmarxres, *dmatxres;
	int ret = 0;

	dev_info(&pdev->dev, "comip_pcm_probe\n");

	pcm = kzalloc(sizeof(struct comip_pcm_data), GFP_KERNEL);
	if (!pcm) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto exit;
	}

	mmres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dmarxres = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	dmatxres = platform_get_resource(pdev, IORESOURCE_DMA, 1);

	if (!mmres || !dmarxres || !dmatxres) {
		dev_err(&pdev->dev, "get resource failed\n");
		ret = -ENODEV;
		goto exit_kfree;
	}

	pcm->reg_base = io_p2v(mmres->start);
	spin_lock_init(&pcm->lock);

	platform_set_drvdata(pdev, pcm);

	comip_pcm_stereo[0].channel = dmatxres->start;
	comip_pcm_stereo[0].daddr = mmres->start + PCM_TRAN_FIFO;

	comip_pcm_stereo[1].channel = dmarxres->start;
	comip_pcm_stereo[1].saddr = mmres->start + PCM_REC_FIFO;

	snd_soc_register_component(&pdev->dev, &lc186x_pcm_component,
					 &comip_pcm_dai, 1);

	return leadcore_asoc_pcm_dma_register(&pdev->dev);

exit_kfree:
	kfree(pcm);
exit:
	return ret;
}

static __exit int comip_pcm_remove(struct platform_device *pdev)
{
	struct comip_pcm_data *pcm = platform_get_drvdata(pdev);

	snd_soc_unregister_component(&pdev->dev);

	leadcore_asoc_pcm_dma_unregister(&pdev->dev);

	kfree(pcm);

	return 0;
}

static struct platform_driver comip_pcm_driver = {
	.remove = comip_pcm_remove,
	.driver = {
		.name = "comip-pcm",
		.owner = THIS_MODULE,
	},
};

static int __init comip_pcm_init(void)
{
	return platform_driver_probe(&comip_pcm_driver, comip_pcm_probe);
}

static void __exit comip_pcm_exit(void)
{
	platform_driver_unregister(&comip_pcm_driver);
}

module_init(comip_pcm_init);
module_exit(comip_pcm_exit);

MODULE_AUTHOR("yangzhou <yangzhou@leadcoretech.com>");
MODULE_DESCRIPTION("lc186x sound pcm driver");
MODULE_LICENSE("GPL");

