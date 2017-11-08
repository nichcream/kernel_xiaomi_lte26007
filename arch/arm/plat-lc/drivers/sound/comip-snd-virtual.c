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
 *	Version 	Date		Author		Description
 *	 1.0.0		2013-12-14	yangshunlong	created
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/io.h>

#include <plat/audio.h>

#define COMIP_PCM_RATES 	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)

static int comip_pcm_virtual_dai_hw_params(
									struct snd_pcm_substream *substream,
									struct snd_pcm_hw_params *params,
									struct snd_soc_dai *dai)
{
	return 0;
}

static int comip_pcm_virtual_hw_params(
									struct snd_pcm_substream *substream,
									struct snd_pcm_hw_params *params)
{
	return 0;
}

static struct snd_soc_dai_ops comip_pcm_virtual_dai_ops = {
	.hw_params	= comip_pcm_virtual_dai_hw_params,
};

static struct snd_soc_dai_driver comip_pcm_virtual_dai = {
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_PCM_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_PCM_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &comip_pcm_virtual_dai_ops,
};

static const struct snd_soc_component_driver comip_pcm_virtual_component = {
	.name = "virtual-pcm",
};

static int comip_pcm_virtual_new(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static int comip_pcm_virtual_fn(struct snd_pcm_substream *substream)
{
	return 0;
}

static snd_pcm_uframes_t comip_pcm_virtual_pointer(
	struct snd_pcm_substream *substream)
{
	return 0;
}

static int comip_pcm_virtual_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static void comip_pcm_virtual_free_dma_buffers(struct snd_pcm *pcm)
{
}

static int comip_pcm_virtual_mmap(struct snd_pcm_substream *substream,
						  struct vm_area_struct *vma)
{
	return 0;
}

struct snd_pcm_ops comip_pcm_virtual_ops = {
	.open		= comip_pcm_virtual_fn,
	.close		= comip_pcm_virtual_fn,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= comip_pcm_virtual_hw_params,
	.hw_free	= comip_pcm_virtual_fn,
	.prepare	= comip_pcm_virtual_fn,
	.trigger	= comip_pcm_virtual_trigger,
	.pointer	= comip_pcm_virtual_pointer,
	.mmap		= comip_pcm_virtual_mmap,
};

static struct snd_soc_platform_driver leadcore_asoc_platform = {
	.ops		= &comip_pcm_virtual_ops,
	.pcm_new	= comip_pcm_virtual_new,
	.pcm_free	= comip_pcm_virtual_free_dma_buffers,
};

static __init int comip_pcm_virtual_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "comip_pcm_virtual_probe\n");

	snd_soc_register_component(&pdev->dev, &comip_pcm_virtual_component,
					&comip_pcm_virtual_dai, 1);

	snd_soc_register_platform(&pdev->dev, &leadcore_asoc_platform);

	return 0;
}

static __exit int comip_pcm_virtual_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);

	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static struct platform_driver comip_pcm_virtual_driver = {
	.remove = comip_pcm_virtual_remove,
	.driver = {
		.name = "virtual-pcm",
		.owner = THIS_MODULE,
	},
};

static int __init comip_snd_virtual_init(void)
{
	platform_driver_probe(&comip_pcm_virtual_driver, comip_pcm_virtual_probe);

	return 0;
}

static void __exit comip_snd_virtual_exit(void)
{
	platform_driver_unregister(&comip_pcm_virtual_driver);
}

module_init(comip_snd_virtual_init);
module_exit(comip_snd_virtual_exit);

MODULE_AUTHOR("yangshunlong <yangshunlong@leadcoretech.com>");
MODULE_DESCRIPTION("virtual sound pcm driver");
MODULE_LICENSE("GPL");
