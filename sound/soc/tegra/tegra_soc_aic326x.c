/*
* tegra_soc_aic326x.c
*
* Copyright (c) 2010-2011, NVIDIA Corporation.
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "tegra_soc.h"
#include "../codecs/tlv320aic326x.h"
#include <sound/soc-dapm.h>
#include <linux/regulator/consumer.h>

#include <linux/types.h>
#include <sound/jack.h>
#include <linux/switch.h>
#include <mach/gpio.h>
#include <mach/audio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

/* Board Specific GPIO configuration for Whistler */
#define TEGRA_GPIO_PW3		179

static struct aic326x_headphone_jack
{
	struct snd_jack *jack;
	int gpio;
	struct work_struct work;
	struct snd_soc_codec *pcodec;
};

static struct aic326x_headphone_jack *aic326x_jack;

static struct platform_device *tegra_snd_device;
static struct regulator *aic326x_reg;

extern struct snd_soc_dai tegra_i2s_dai[];
extern struct snd_soc_dai tegra_spdif_dai;
extern struct snd_soc_dai tegra_generic_codec_dai[];
extern struct snd_soc_platform tegra_soc_platform;

static int aic326x_hifi_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct tegra_audio_data *audio_data = rtd->socdev->codec_data;
	enum dac_dap_data_format data_fmt;
	int dai_flag = 0, sys_clk;
	int err;

	if (tegra_das_is_port_master(tegra_audio_codec_type_hifi))
		dai_flag |= SND_SOC_DAIFMT_CBM_CFM;
	else
		dai_flag |= SND_SOC_DAIFMT_CBS_CFS;

	data_fmt = tegra_das_get_codec_data_fmt(tegra_audio_codec_type_hifi);

	/* We are supporting DSP and I2s format for now */
	if (data_fmt & dac_dap_data_format_i2s)
		dai_flag |= SND_SOC_DAIFMT_I2S;
	else
		dai_flag |= SND_SOC_DAIFMT_DSP_A;

	err = snd_soc_dai_set_fmt(codec_dai, dai_flag);
	if (err < 0) {
		pr_err("codec_dai fmt not set\n");
		return err;
	}


	err = snd_soc_dai_set_fmt(cpu_dai, dai_flag);
	if (err < 0) {
		pr_err("cpu_dai fmt not set\n");
		return err;
	}

	sys_clk = clk_get_rate(audio_data->dap_mclk);

	err = snd_soc_dai_set_sysclk(codec_dai, 0, sys_clk, SND_SOC_CLOCK_IN);
	if (err < 0) {
		pr_err("codec_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(cpu_dai, 0, sys_clk, SND_SOC_CLOCK_IN);
	if (err < 0) {
		pr_err("cpu_dai clock not set\n");
		return err;
	}

	return 0;
}

static int aic326x_hifi_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int aic326x_voice_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct tegra_audio_data* audio_data = rtd->socdev->codec_data;
	enum dac_dap_data_format data_fmt;
	int dai_flag = 0, sys_clk;
	int err;
	bool master_port = false;

	if (!strcmp(rtd->dai->stream_name, "Tegra BT Voice Call")) {
		master_port = tegra_das_is_port_master(
				tegra_audio_codec_type_bluetooth);

		data_fmt = tegra_das_get_codec_data_fmt(
				tegra_audio_codec_type_bluetooth);
	}
	else if (!strcmp(rtd->dai->stream_name, "Tegra Voice Call")) {
		master_port = tegra_das_is_port_master(
				tegra_audio_codec_type_voice);

		data_fmt = tegra_das_get_codec_data_fmt(
				tegra_audio_codec_type_baseband);
	}
	else {/* Tegra BT-SCO Voice */
		master_port = tegra_das_is_port_master(
				tegra_audio_codec_type_bluetooth);

		data_fmt = tegra_das_get_codec_data_fmt
			(tegra_audio_codec_type_bluetooth);
	}

	if (master_port == true)
		dai_flag |= SND_SOC_DAIFMT_CBM_CFM;
	else
		dai_flag |= SND_SOC_DAIFMT_CBS_CFS;

	/* We are supporting DSP and I2s format for now */
	if (data_fmt & dac_dap_data_format_dsp)
		dai_flag |= SND_SOC_DAIFMT_DSP_A;
	else
		dai_flag |= SND_SOC_DAIFMT_I2S;

	err = snd_soc_dai_set_fmt(codec_dai, dai_flag);
	if (err < 0) {
		pr_err("codec_dai fmt not set \n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai, dai_flag);
	if (err < 0) {
		pr_err("cpu_dai fmt not set \n");
		return err;
	}

	sys_clk = clk_get_rate(audio_data->dap_mclk);
	err = snd_soc_dai_set_sysclk(codec_dai, 0, sys_clk, SND_SOC_CLOCK_IN);
	if (err < 0) {
		pr_err("cpu_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(cpu_dai, 0, sys_clk, SND_SOC_CLOCK_IN);
	if (err < 0) {
		pr_err("cpu_dai clock not set\n");
		return err;
	}

	return 0;
}

static int aic326x_voice_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int aic326x_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	return 0;
}

int aic326x_codec_startup(struct snd_pcm_substream *substream)
{
	tegra_das_power_mode(true);
	return 0;
}

void aic326x_codec_shutdown(struct snd_pcm_substream *substream)
{
	tegra_das_power_mode(false);
}

int aic326x_soc_suspend_pre(struct platform_device *pdev, pm_message_t state)
{
#if 0
	disable_irq(gpio_to_irq(aic326x_jack->gpio));
#endif
	return 0;
}

int aic326x_soc_suspend_post(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct tegra_audio_data *audio_data = socdev->codec_data;
	clk_disable(audio_data->dap_mclk);

	return 0;
}

int aic326x_soc_resume_pre(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct tegra_audio_data *audio_data = socdev->codec_data;
	clk_enable(audio_data->dap_mclk);

	return 0;
}

int aic326x_soc_resume_post(struct platform_device *pdev)
{
#if 0
	enable_irq(gpio_to_irq(aic326x_jack->gpio));
#endif
	return 0;
}

static struct snd_soc_ops aic326x_hifi_ops = {
	.hw_params = aic326x_hifi_hw_params,
	.hw_free   = aic326x_hifi_hw_free,
	.startup = aic326x_codec_startup,
	.shutdown = aic326x_codec_shutdown,
};

static struct snd_soc_ops aic326x_voice_ops = {
	.hw_params = aic326x_voice_hw_params,
	.hw_free   = aic326x_voice_hw_free,
	.startup = aic326x_codec_startup,
	.shutdown = aic326x_codec_shutdown,
};

static struct snd_soc_ops aic326x_spdif_ops = {
	.hw_params = aic326x_spdif_hw_params,
};

void tegra_ext_control(struct snd_soc_codec *codec, int new_con)
{
	struct tegra_audio_data *audio_data = codec->socdev->codec_data;

	/* Disconnect old codec routes and connect new routes*/
	if (new_con & TEGRA_HEADPHONE)
		snd_soc_dapm_enable_pin(codec, "Headphone");
	else
		snd_soc_dapm_disable_pin(codec, "Headphone");

	if (new_con & TEGRA_EAR_SPK)
		snd_soc_dapm_enable_pin(codec, "EarPiece");
	else
		snd_soc_dapm_disable_pin(codec, "EarPiece");

	if (new_con & TEGRA_SPK)
		snd_soc_dapm_enable_pin(codec, "SPK out");
	else
		snd_soc_dapm_disable_pin(codec, "SPK out");

	if (new_con & TEGRA_EXT_MIC)
		snd_soc_dapm_enable_pin(codec, "Ext Mic");
	else
		snd_soc_dapm_disable_pin(codec, "Ext Mic");

	if (new_con & TEGRA_LINEIN)
		snd_soc_dapm_enable_pin(codec, "Linein");
	else
		snd_soc_dapm_disable_pin(codec, "Linein");

	if (new_con & TEGRA_HEADSET_OUT)
		snd_soc_dapm_enable_pin(codec, "Headset Out");
	else
		snd_soc_dapm_disable_pin(codec, "Headset Out");

	if (new_con & TEGRA_HEADSET_IN)
		snd_soc_dapm_enable_pin(codec, "Headset In");
	else
		snd_soc_dapm_disable_pin(codec, "Headset In");

	/* set to headphone as default */
	snd_soc_dapm_enable_pin(codec, "Headphone");

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);
	audio_data->codec_con = new_con;
}

/*tegra machine dapm widgets */
static const struct snd_soc_dapm_widget aic326x_tegra_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_HP("EarPiece", NULL),
	SND_SOC_DAPM_HP("Headset Out", NULL),
	SND_SOC_DAPM_MIC("Headset In", NULL),
	SND_SOC_DAPM_SPK("SPK out", NULL),
	SND_SOC_DAPM_MIC("Ext Mic", NULL),
	SND_SOC_DAPM_LINE("Linein", NULL),
};

/* Tegra machine audio map (connections to the codec pins) */
static const struct snd_soc_dapm_route aic326x_audio_map[] = {

	/* headphone*/
	{"Headphone", NULL, "HPL"},
	{"Headphone", NULL, "HPR"},

	/* earpiece */
	{"EarPiece", NULL, "LOL"},
	{"EarPiece", NULL, "LOR"},

	/* headset Jack */
	{"Headset Out", NULL, "HPL"},
	{"Headset Out", NULL, "HPR"},
	{"IN2L", NULL, "Headset In"},

	/* build-in speaker */
	{"SPK out", NULL, "SPKL"},
	{"SPK out", NULL, "SPKR"},

	/* external mic is stero */
	{"IN2L", NULL, "Ext Mic"},
	{"IN2R", NULL, "Ext Mic"},

	/* Line in */
	{"IN2L", NULL, "Linein"},
	{"IN2R", NULL, "Linein"},
};

#if 0
static void aic326x_intr_work(struct work_struct *work)
{
	unsigned int value;

	/* Do something here */
	mutex_lock(&aic326x_jack->pcodec->mutex);

	/* GPIO4 interrupt disable (also disable other interrupts) */
	/* Invert GPIO4 interrupt polarity */
	/* GPIO4 interrupt enable */

	mutex_unlock(&aic326x_jack->pcodec->mutex);
}

static irqreturn_t aic326x_irq(int irq, void *data)
{
	schedule_work(&aic326x_jack->work);
	return IRQ_HANDLED;
}
#endif

static int aic326x_codec_init(struct snd_soc_codec *codec)
{
	struct tegra_audio_data *audio_data = codec->socdev->codec_data;
	int ret = 0;

	if (!audio_data->init_done) {
		audio_data->dap_mclk = tegra_das_get_dap_mclk();
		if (!audio_data->dap_mclk) {
			pr_err("Failed to get dap mclk\n");
			ret = -ENODEV;
			return ret;
		}
		clk_enable(audio_data->dap_mclk);

		/* Add tegra specific widgets */
		snd_soc_dapm_new_controls(codec, aic326x_tegra_dapm_widgets,
					ARRAY_SIZE(aic326x_tegra_dapm_widgets));

		/* Set up tegra specific audio path audio_map */
		snd_soc_dapm_add_routes(codec, aic326x_audio_map,
					ARRAY_SIZE(aic326x_audio_map));

		/* Add jack detection */
		/*ret = tegra_jack_init(codec);
		if (ret < 0) {
			pr_err("Failed in jack init\n");
			return ret;
		}*/

		/* Default to OFF */
		tegra_ext_control(codec, TEGRA_AUDIO_OFF);

		ret = tegra_controls_init(codec);
		if (ret < 0) {
			pr_err("Failed in controls init\n");
			return ret;
		}

#if 0
		if (!aic326x_jack) {
			unsigned int value;
			aic326x_jack = kzalloc(sizeof(*aic326x_jack),
							 GFP_KERNEL);
			if (!aic326x_jack) {
				pr_err("failed to allocate aic326x-jack\n");
				return -ENOMEM;
			}

			aic326x_jack->gpio = TEGRA_GPIO_PW3;
			aic326x_jack->pcodec = codec;

			INIT_WORK(&aic326x_jack->work, aic326x_intr_work);

			ret = snd_jack_new(codec->card,
				"Headphone Jack", SND_JACK_HEADPHONE,
				&aic326x_jack->jack);
			if (ret < 0)
				goto failed;

			ret = gpio_request(aic326x_jack->gpio,
					"headphone-detect-gpio");
			if (ret)
				goto failed;

			ret = gpio_direction_input(aic326x_jack->gpio);
			if (ret)
				goto gpio_failed;

			tegra_gpio_enable(aic326x_jack->gpio);

			ret = request_irq(gpio_to_irq(aic326x_jack->gpio),
					aic326x_irq,
					IRQF_TRIGGER_FALLING,
					"aic326x",
					aic326x_jack);

			if (ret)
				goto gpio_failed;

			/* Configure GPIO pin to generate the interrupt */
			/* Active low Interrupt */
			/* interupt when low Headphone connected */
			/* Enable GPIO interrupt,disable other interrupts */

		}
#endif
		audio_data->codec = codec;
		audio_data->init_done = 1;
	}

	return ret;
#if 0
gpio_failed:
	gpio_free(aic326x_jack->gpio);
failed:
	kfree(aic326x_jack);
	aic326x_jack = NULL;
	return ret;
#endif
}

static struct snd_soc_dai_link tegra_soc_dai[] = {
	{
		.name = "tlv320aic3262",
		.stream_name = "tlv320aic3262 HiFi",
		.cpu_dai = &tegra_i2s_dai[0],
		.codec_dai = &tlv320aic3262_dai,
		.init = aic326x_codec_init,
		.ops = &aic326x_hifi_ops,
	},
	{
		.name = "Tegra-Voice",
		.stream_name = "Tegra BT-SCO Voice",
		.cpu_dai = &tegra_i2s_dai[1],
		.codec_dai = &tegra_generic_codec_dai[TEGRA_BT_CODEC_ID],
		.init = aic326x_codec_init,
		.ops = &aic326x_voice_ops,
	},
	{
		.name = "Tegra-spdif",
		.stream_name = "Tegra Spdif",
		.cpu_dai = &tegra_spdif_dai,
		.codec_dai = &tegra_generic_codec_dai[TEGRA_SPDIF_CODEC_ID],
		.init = aic326x_codec_init,
		.ops = &aic326x_spdif_ops,
	},
	{
		.name = "Tegra-voice-call",
		.stream_name = "Tegra Voice Call",
		.cpu_dai = &tegra_generic_codec_dai[TEGRA_BB_CODEC_ID],
		.codec_dai = &tlv320aic3262_dai,
		.init = aic326x_codec_init,
		.ops = &aic326x_voice_ops,
	},

};

static struct tegra_audio_data aic326x_audio_data = {
	.init_done = 0,
	.play_device = TEGRA_AUDIO_DEVICE_NONE,
	.capture_device = TEGRA_AUDIO_DEVICE_NONE,
	.is_call_mode = false,
	.codec_con = TEGRA_AUDIO_OFF,
};

static struct snd_soc_card aic326x_tegra_snd_soc = {
	.name = "tegra",
	.platform = &tegra_soc_platform,
	.dai_link = tegra_soc_dai,
	.num_links = ARRAY_SIZE(tegra_soc_dai),
	.suspend_pre = aic326x_soc_suspend_pre,
	.suspend_post = aic326x_soc_suspend_post,
	.resume_pre = aic326x_soc_resume_pre,
	.resume_post = aic326x_soc_resume_post,
};

static struct snd_soc_device aic326x_tegra_snd_devdata = {
	.card = &aic326x_tegra_snd_soc,
	.codec_dev = &soc_codec_dev_aic3262,
	.codec_data = &aic326x_audio_data,
};

static int __init aic326x_tegra_init(void)
{
	int ret = 0;

	tegra_snd_device = platform_device_alloc("soc-audio", -1);
	if (!tegra_snd_device) {
		pr_err("failed to allocate soc-audio\n");
		return ENOMEM;
	}

	platform_set_drvdata(tegra_snd_device, &aic326x_tegra_snd_devdata);
	aic326x_tegra_snd_devdata.dev = &tegra_snd_device->dev;
	ret = platform_device_add(tegra_snd_device);
	if (ret) {
		pr_err("audio device could not be added\n");
		goto fail;
	}

#if 0
	aic326x_reg = regulator_get(NULL, "avddio_audio");
	if (IS_ERR(aic326x_reg)) {
		ret = PTR_ERR(aic326x_reg);
		pr_err("unable to get aic326x regulator\n");
		goto fail;
	}

	ret = regulator_enable(aic326x_reg);
	if (ret) {
		pr_err("aic326x regulator enable failed\n");
		goto err_put_regulator;
	}
#endif
	return 0;

fail:
	if (tegra_snd_device) {
		platform_device_put(tegra_snd_device);
		tegra_snd_device = 0;
	}

	return ret;

#if 0
err_put_regulator:
	regulator_put(aic326x_reg);
	return ret;
#endif
}

static void __exit aic326x_tegra_exit(void)
{
	platform_device_unregister(tegra_snd_device);
#if 0
	regulator_disable(aic326x_reg);
	regulator_put(aic326x_reg);
#endif

	if (aic326x_jack) {
		gpio_free(aic326x_jack->gpio);
		kfree(aic326x_jack);
		aic326x_jack = NULL;
	}
}

module_init(aic326x_tegra_init);
module_exit(aic326x_tegra_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra ALSA SoC");
MODULE_LICENSE("GPL");

