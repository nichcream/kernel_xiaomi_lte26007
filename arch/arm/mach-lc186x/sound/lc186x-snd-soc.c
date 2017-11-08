/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 *	Copyright (c) 2013	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip 1810 sound driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version 	Date		Author		Description
 *	 1.0.0		2012-02-01	tangyong	created
 *	 2.0.0		2013-04-18	tuzhiqiang	optimized
 *
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input.h>

extern void codec_hs_jack_detect(struct snd_soc_codec *codec,
				struct snd_soc_jack *jack, int report);

static struct platform_device *comip_snd_soc_device;

static int comip_snd_soc_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void comip_snd_soc_shutdown(struct snd_pcm_substream *substream)
{
}

static int comip_snd_soc_hw_params(struct snd_pcm_substream *substream,
								struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	if(codec_dai->id != 2) {
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
										SND_SOC_DAIFMT_CBM_CFM);
		if (ret < 0)
			return ret;
	}
	if(codec_dai->id != 1) {
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
									SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			return ret;
	}
	return ret;
}
static struct snd_soc_jack comip_headset;

static int comip_soc_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	int ret;

	ret = snd_soc_jack_new(codec, "Headset",
					SND_JACK_HEADSET |SND_JACK_BTN_0 |
					SND_JACK_BTN_1 | SND_JACK_BTN_2,
				   &comip_headset);

	if (ret < 0)
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);

	ret = snd_jack_set_key(comip_headset.jack, SND_JACK_BTN_0,
							KEY_MEDIA);

	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_MEDIA: %d\n", ret);

	ret = snd_jack_set_key(comip_headset.jack, SND_JACK_BTN_1,
							KEY_VOLUMEUP);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEUP: %d\n", ret);

	ret = snd_jack_set_key(comip_headset.jack, SND_JACK_BTN_2,
							KEY_VOLUMEDOWN);

	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEDOWN: %d\n", ret);

	codec_hs_jack_detect(codec, &comip_headset,
						SND_JACK_HEADSET |SND_JACK_BTN_0 |
						SND_JACK_BTN_1 | SND_JACK_BTN_2);

	return 0;
}

static struct snd_soc_ops comip_snd_soc_ops = {
	.startup = comip_snd_soc_startup,
	.shutdown = comip_snd_soc_shutdown,
	.hw_params = comip_snd_soc_hw_params,
};

static struct snd_soc_dai_link comip_snd_soc_dai_link[] = {
	{
		.name = "comip_audio",
		.stream_name = "comip_au",
		.codec_name = "comip_codec",
		.codec_dai_name = "comip_hifi",
		.cpu_dai_name = "comip-i2s.0",
		.platform_name	= "comip-i2s.0",
		.ops = &comip_snd_soc_ops,
		.init = comip_soc_init,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_CBM_CFM,
	},
	{
		.name = "comip_voice",
		.stream_name = "comip_vx",
		.codec_name = "comip_codec",
		.codec_dai_name = "comip_voice",
		.cpu_dai_name = "virtual-pcm",
		.ops = &comip_snd_soc_ops,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_CBM_CFM,
	},
#if defined(CONFIG_SND_COMIP_PCM)
	{
		.name = "comip_voip",
		.stream_name = "voip",
		.codec_name = "comip_codec",
		.codec_dai_name = "virtual_codec",
		.cpu_dai_name = "comip-pcm.0",
		.platform_name = "comip-pcm.0",
		.ops = &comip_snd_soc_ops,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_CBM_CFM,
	},
	{
		.name = "voice_link",
		.stream_name = "voice_loop",
		.codec_name = "comip_codec",
		.codec_dai_name = "comip_voice",
		.cpu_dai_name = "comip-pcm.0",
		.platform_name = "comip-pcm.0",
		.ops = &comip_snd_soc_ops,
		.ignore_suspend = 1,
		.dai_fmt = SND_SOC_DAIFMT_CBM_CFM,
	}
#endif
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
		dev_err(&pdev->dev, "allocate platform device failed\n");
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
	.probe	= comip_snd_soc_probe,
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

MODULE_AUTHOR("Yang Shunlong <yangshunlong@leadcoretech.com>");
MODULE_DESCRIPTION("comip sound soc driver");
MODULE_LICENSE("GPL");

