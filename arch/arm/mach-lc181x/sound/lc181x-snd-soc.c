/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 *	Copyright (c) 2013  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip 1810 sound driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2012-02-01	tangyong	created
 *	 2.0.0		2013-04-18	tuzhiqiang	optimized
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <sound/soc.h>

static struct platform_device *comip_snd_soc_device;

static int comip_snd_soc_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void comip_snd_soc_shutdown(struct snd_pcm_substream *substream)
{
	return;
}

static int comip_snd_soc_hw_params(struct snd_pcm_substream *substream,
                                struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
								SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
								SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	return ret;
}

static struct snd_soc_ops comip_snd_soc_ops = {
	.startup = comip_snd_soc_startup,
	.shutdown = comip_snd_soc_shutdown,
	.hw_params = comip_snd_soc_hw_params,
};

static struct snd_soc_dai_link comip_snd_soc_dai_link[] = {
	{
		.name = "comip",
		.stream_name = "comip",
		.codec_name = "comip_codec",
		.codec_dai_name = "comip_hifi",
		.cpu_dai_name = "comip-i2s.0",
		.ops = &comip_snd_soc_ops,
		.platform_name	= "comip-i2s.0",
	}
};

static struct snd_soc_card comip_snd_soc = {
	.name = "comip_snd_soc",
	.dai_link = comip_snd_soc_dai_link,
	.num_links = ARRAY_SIZE(comip_snd_soc_dai_link),
};

static int comip_snd_soc_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "comip_snd_soc_probe\n");

	comip_snd_soc_device = platform_device_alloc("soc-audio", -1);
	if (!comip_snd_soc_device) {
		dev_err(&pdev->dev, "alloc platform device failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(comip_snd_soc_device, &comip_snd_soc);
	ret = platform_device_add(comip_snd_soc_device);
	if (ret) {
		dev_err(&pdev->dev, "add platform device failed\n");
		goto err_add_soc_device;
	}

	return 0;

err_add_soc_device:
	platform_device_put(comip_snd_soc_device);

	return ret;
}

static int comip_snd_soc_remove(struct platform_device *pdev)
{
	platform_device_unregister(comip_snd_soc_device);
	return 0;
}

static struct platform_driver comip_snd_soc_driver = {
	.probe  = comip_snd_soc_probe,
	.remove = comip_snd_soc_remove,
	.driver = {
		.name = "comip_snd_soc",
		.owner = THIS_MODULE,
	},
};

static int __init comip_snd_soc_init(void)
{
	return platform_driver_register(&comip_snd_soc_driver);
}

static void __exit comip_snd_soc_exit(void)
{
	platform_driver_unregister(&comip_snd_soc_driver);
}

module_init(comip_snd_soc_init);
module_exit(comip_snd_soc_exit);

MODULE_AUTHOR("Peter Tang <tangyong@leadcoretech.com>");
MODULE_DESCRIPTION("comip sound soc driver");
MODULE_LICENSE("GPL");

