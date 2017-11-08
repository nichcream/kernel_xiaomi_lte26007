/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2009  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the i2s driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2012-02-04	tangyong	created
 *	 2.0.0		2013-12-23	tuzhiqiang	optimized
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
#include "comip-snd-dma.h"

#define COMIP_I2S_ONLINE_CAPTURE_SIZE	(4096)
#define COMIP_I2S_CLK_RATE		(19500000)

enum {
	COMIP_I2S_ST_NORMAL = 0,
	COMIP_I2S_ST_ONLINE_CAPTURE = 1,
};


static struct comip_pcm_dma_params comip_i2s_pcm_stereo[2] = {
	{
		.name = "I2S PCM Stereo out",
	},
	{
		.name = "I2S PCM Stereo in",
	}
};

static inline void i2s_writel(struct comip_i2s_data *i2s,
			int offset, unsigned int val)
{
	writel(val, i2s->reg_base + offset);
}

static inline unsigned int i2s_readl(struct comip_i2s_data *i2s,
			int offset)
{
	return readl(i2s->reg_base + offset);
}

static int comip_i2s_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct comip_i2s_data *i2s = dev_get_drvdata(dai->dev);
	int ret = 0;
	unsigned long flags;

	i2s->clk = clk_get(dai->dev, "i2s_clk");
	if (IS_ERR(i2s->clk)) {
		dev_err(dai->dev, "get i2s clock failed\n");
		ret = PTR_ERR(i2s->clk);
		goto exit;
	}

	spin_lock_irqsave(&i2s->lock, flags);
	clk_set_rate(i2s->clk, COMIP_I2S_CLK_RATE);
	clk_enable(i2s->clk);
	spin_unlock_irqrestore(&i2s->lock, flags);

exit:
	return ret;
}

static int comip_i2s_set_fmt(struct snd_soc_dai *dai,
			unsigned int fmt)
{
	struct comip_i2s_data *i2s = dev_get_drvdata(dai->dev);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		i2s->fmt = 0;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		i2s->fmt = 1;
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		i2s->master = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		i2s->master = 0;
		break;
	default:
		break;
	}

	return 0;
}

static int comip_i2s_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct comip_i2s_data *i2s = dev_get_drvdata(dai->dev);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	size_t totsize = params_buffer_bytes(params);

	if (substream->stream && totsize == COMIP_I2S_ONLINE_CAPTURE_SIZE) {
		i2s->state = COMIP_I2S_ST_ONLINE_CAPTURE;
		return 0;
	}

	cpu_dai->playback_dma_data = &comip_i2s_pcm_stereo[0];
	cpu_dai->capture_dma_data = &comip_i2s_pcm_stereo[1];

	return 0;
}

static int comip_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			struct snd_soc_dai *dai)
{
	struct comip_i2s_data *i2s = dev_get_drvdata(dai->dev);
	int ret = 0;
	int val;
	unsigned long flags;

	if (substream->stream && (i2s->state == COMIP_I2S_ST_ONLINE_CAPTURE))
		return 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		spin_lock_irqsave(&i2s->lock, flags);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			val = i2s_readl(i2s, I2S_MODE);
			val |= (1 << I2S_MODE_FLUSH_TRAN_BUF);
			i2s_writel(i2s, I2S_MODE, val);

			val = i2s_readl(i2s, I2S_MODE);
			val &= ~(1 << I2S_MODE_FLUSH_TRAN_BUF);
			i2s_writel(i2s, I2S_MODE, val);

			val = i2s_readl(i2s, I2S_MODE);
			val |= (1 << I2S_MODE_DLY_MODE) | (1 << I2S_MODE_I2S0_EN) |
				(1 << I2S_MODE_TRAN_EN) | (1 << I2S_MODE_SLAVE_EN);
			i2s_writel(i2s, I2S_MODE, val);
		} else {
			val = i2s_readl(i2s, I2S_MODE);
			val |= (1 << I2S_MODE_FLUSH_REC_BUF);
			i2s_writel(i2s, I2S_MODE, val);

			val = i2s_readl(i2s, I2S_MODE);
			val &= ~(1 << I2S_MODE_FLUSH_REC_BUF);
			i2s_writel(i2s, I2S_MODE, val);

			val = i2s_readl(i2s, I2S_MODE);
			val |= (1 << I2S_MODE_DLY_MODE) | (1 << I2S_MODE_I2S0_EN) |
				(1 << I2S_MODE_REC_EN) | (1 << I2S_MODE_SLAVE_EN);
			i2s_writel(i2s, I2S_MODE, val);
		}
		spin_unlock_irqrestore(&i2s->lock, flags);
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_STOP:
		spin_lock_irqsave(&i2s->lock, flags);
		val = i2s_readl(i2s, I2S_MODE);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			val &= ~(1 << I2S_MODE_TRAN_EN);
		else
			val &= ~(1 << I2S_MODE_REC_EN);
		i2s_writel(i2s, I2S_MODE, val);
		spin_unlock_irqrestore(&i2s->lock, flags);
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void comip_i2s_shutdown(struct snd_pcm_substream *substream,
                                    struct snd_soc_dai *dai)
{
	struct comip_i2s_data *i2s = dev_get_drvdata(dai->dev);
	int val;
	unsigned long flags;

	if (substream->stream && (i2s->state == COMIP_I2S_ST_ONLINE_CAPTURE)) {
		i2s->state = COMIP_I2S_ST_NORMAL;
		return;
	}

	spin_lock_irqsave(&i2s->lock, flags);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		val = i2s_readl(i2s, I2S_MODE);
		val &= ~(1 << I2S_MODE_TRAN_EN);
		i2s_writel(i2s, I2S_MODE, val);
	} else {
		val = i2s_readl(i2s, I2S_MODE);
		val &= ~(1 << I2S_MODE_REC_EN);
		i2s_writel(i2s, I2S_MODE, val);
	}
	spin_unlock_irqrestore(&i2s->lock, flags);

	clk_disable(i2s->clk);
}

#ifdef CONFIG_PM

static int comip_i2s_suspend(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int comip_i2s_resume(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

#endif

#define COMIP_I2S_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

static struct snd_soc_dai_ops comip_i2s_dai_ops = {
	.trigger	= comip_i2s_trigger,
	.hw_params	= comip_i2s_hw_params,
	.set_fmt	= comip_i2s_set_fmt,
	.startup 	= comip_i2s_startup,
	.shutdown 	= comip_i2s_shutdown,
};

static struct snd_soc_dai_driver comip_i2s_dai = {
#ifdef CONFIG_PM
	.suspend = comip_i2s_suspend,
	.resume = comip_i2s_resume,
#endif
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &comip_i2s_dai_ops,
};

static const struct snd_soc_component_driver lc181x_i2s_component = {
	.name		= "comip_i2s",
};

static __init int comip_i2s_probe(struct platform_device *pdev)
{
	struct comip_i2s_data *i2s;
	struct resource *mmres, *ramres;
	struct resource *dmarxres, *dmatxres;
	int ret = 0;

	dev_info(&pdev->dev, "comip_i2s_probe\n");

	i2s = kzalloc(sizeof(struct comip_i2s_data), GFP_KERNEL);
	if (!i2s) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto exit;
	}

	mmres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ramres = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	dmarxres = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	dmatxres = platform_get_resource(pdev, IORESOURCE_DMA, 1);

	if (!mmres || !dmarxres || !dmatxres) {
		dev_err(&pdev->dev, "get resource failed\n");
		ret = -ENODEV;
		goto exit_kfree;
	}
	if(ramres)
		i2s->baddr = ramres->start;
	i2s->reg_base = io_p2v(mmres->start);
	i2s->state = COMIP_I2S_ST_NORMAL;
	spin_lock_init(&i2s->lock);

	platform_set_drvdata(pdev, i2s);

	comip_i2s_pcm_stereo[0].channel = dmatxres->start;
	comip_i2s_pcm_stereo[0].daddr = mmres->start + I2S_TRAN_FIFO;

	comip_i2s_pcm_stereo[1].channel = dmarxres->start;
	comip_i2s_pcm_stereo[1].saddr = mmres->start + I2S_REC_FIFO;

	snd_soc_register_component(&pdev->dev, &lc181x_i2s_component,
					 &comip_i2s_dai, 1);

	return leadcore_asoc_dma_platform_register(&pdev->dev);

exit_kfree:
	kfree(i2s);
exit:
	return ret;
}

static __exit int comip_i2s_remove(struct platform_device *pdev)
{
	struct comip_i2s_data *i2s = platform_get_drvdata(pdev);

	snd_soc_unregister_component(&pdev->dev);

	leadcore_asoc_dma_platform_unregister(&pdev->dev);

	kfree(i2s);

	return 0;
}

static struct platform_driver comip_i2s_driver = {
	.remove = comip_i2s_remove,
	.driver = {
		.name = "comip-i2s",
		.owner = THIS_MODULE,
	},
};

static int __init comip_i2s_init(void)
{
	return platform_driver_probe(&comip_i2s_driver, comip_i2s_probe);
}

static void __exit comip_i2s_exit(void)
{
	platform_driver_unregister(&comip_i2s_driver);
}

module_init(comip_i2s_init);
module_exit(comip_i2s_exit);

MODULE_AUTHOR("Peter Tang <tangyong@leadcoretech.com>");
MODULE_DESCRIPTION("lc181x sound i2s driver");
MODULE_LICENSE("GPL");

