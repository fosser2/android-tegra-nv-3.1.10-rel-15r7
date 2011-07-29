/*
 * ALSA SoC Generic Codec driver
 *
 * This Codec can be used for the following purposes
 * S/PDIF/HDMI audio where there is no codec.
 * Audio over MOST (where CPLD logic is used)
 * Complex SOCs shipping their Drivers without Codec Support
 *
 * Author:      Nitin Pai, <npai@nvidia.com>
 * Copyright (c) 2009-2010, NVIDIA Corporation.
 *
 *
 * Based on code copyright/by:
 *
 * Author:      Steve Chen,  <schen@mvista.com>
 * Copyright:   (C) 2009 MontaVista Software, Inc., <source@mvista.com>
 * Copyright:   (C) 2009  Texas Instruments, India
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#include "generic_codec.h"

MODULE_LICENSE("GPL");

#define STUB_RATES	SNDRV_PCM_RATE_8000_96000
#define STUB_FORMATS	SNDRV_PCM_FMTBIT_S16_LE

static struct snd_soc_codec *generic_dit_codec;

static int generic_dit_codec_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret;

	if (generic_dit_codec == NULL) {
		dev_err(&pdev->dev, "Codec device not registered\n");
		return -ENODEV;
	}

	socdev->card->codec = generic_dit_codec;
	codec = generic_dit_codec;

	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(codec->dev, "failed to create pcms: %d\n", ret);
		goto err_create_pcms;
	}

	return 0;

err_create_pcms:
	return ret;
}

static int generic_dit_codec_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	snd_soc_free_pcms(socdev);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_generic_dit = {
	.probe		= generic_dit_codec_probe,
	.remove		= generic_dit_codec_remove,
}; EXPORT_SYMBOL_GPL(soc_codec_dev_generic_dit);

struct snd_soc_dai generic_dit_stub_dai = {
	.name		= "GENERIC-DIT",
	.playback	= {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 384,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
	.capture	= {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 384,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
};
EXPORT_SYMBOL_GPL(generic_dit_stub_dai);

static int generic_dit_probe(struct platform_device *pdev)
{
	struct snd_soc_codec *codec;
	int ret;

	if (generic_dit_codec) {
		dev_err(&pdev->dev, "Another Codec is registered\n");
		ret = -EINVAL;
		goto err_reg_codec;
	}

	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	codec->dev = &pdev->dev;

	mutex_init(&codec->mutex);

	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name = "generic-dit";
	codec->owner = THIS_MODULE;
	codec->dai = &generic_dit_stub_dai;
	codec->num_dai = 1;

	generic_dit_codec = codec;

	ret = snd_soc_register_codec(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to register codec: %d\n", ret);
		goto err_reg_codec;
	}

	generic_dit_stub_dai.dev = &pdev->dev;
	ret = snd_soc_register_dai(&generic_dit_stub_dai);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to register dai: %d\n", ret);
		goto err_reg_dai;
	}

	return 0;

err_reg_dai:
	snd_soc_unregister_codec(codec);
err_reg_codec:
	kfree(generic_dit_codec);
	return ret;
}

static int generic_dit_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&generic_dit_stub_dai);
	snd_soc_unregister_codec(generic_dit_codec);
	kfree(generic_dit_codec);
	generic_dit_codec = NULL;
	return 0;
}

static struct platform_driver generic_dit_driver = {
	.probe		= generic_dit_probe,
	.remove		= generic_dit_remove,
	.driver		= {
		.name	= "generic-dit",
		.owner	= THIS_MODULE,
	},
};

static int __init generic_dit_modinit(void)
{
	return platform_driver_register(&generic_dit_driver);
}

static void __exit generic_dit_exit(void)
{
	platform_driver_unregister(&generic_dit_driver);
}

module_init(generic_dit_modinit);
module_exit(generic_dit_exit);

