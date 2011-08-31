 /*
  * tegra_soc_wm8753.c  --  SoC audio for tegra
  *
  * Copyright  2010-2011 Nvidia Graphics Pvt. Ltd.
  *
  * Author: Sachin Nikam
  *             snikam@nvidia.com
  *  http://www.nvidia.com
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
#include "../codecs/wm8753.h"
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

#define WM8753_SRATE1_SRMODE	(1<<8)

#define WM8753_GPIO2_GP2M_2	(1<<5)
#define WM8753_GPIO2_GP2M_1 	1<<4
#define WM8753_GPIO2_GP2M_0 	1<<3

#define WM8753_GPIO1_INTCON_1 	1<<8
#define WM8753_GPIO1_INTCON_0 	1<<7

#define WM8753_INTPOL_GPIO4IPOL 1<<4

#define WM8753_INTEN_MICSHTEN	1<<0
#define WM8753_INTEN_MICDETEN	1<<1
#define WM8753_INTEN_GPIO3IEN	1<<3
#define WM8753_INTEN_GPIO4IEN	1<<4
#define WM8753_INTEN_GPIO5IEN	1<<5
#define WM8753_INTEN_HPSWIEN	1<<6
#define WM8753_INTEN_TSDIEN 	1<<7

/* Board Specific GPIO configuration for Whistler */
#define TEGRA_GPIO_PW3		(179)

enum headset_state {
	BIT_NO_HEADSET = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};

static struct wm8753_headphone_jack {
	struct snd_jack *jack;
	int gpio;
	struct work_struct work;
	struct snd_soc_codec* pcodec;
};

static struct wm8753_headphone_jack* wm8753_jack = NULL;

static struct platform_device *tegra_snd_device;
static struct regulator* wm8753_reg;

extern struct snd_soc_dai tegra_i2s_dai[];
extern struct snd_soc_dai tegra_spdif_dai;
extern struct snd_soc_dai tegra_generic_codec_dai[];
extern struct snd_soc_platform tegra_soc_platform;

static bool vdac_enabled;

static int tegra_hifi_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct tegra_audio_data* audio_data = rtd->socdev->codec_data;
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

static int tegra_hifi_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}


static int tegra_voice_hw_params(struct snd_pcm_substream *substream,
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
		vdac_enabled = true;
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
	err = snd_soc_dai_set_sysclk(codec_dai, WM8753_PCMCLK, sys_clk,
							SND_SOC_CLOCK_IN);
	if (err < 0) {
		pr_err("cpu_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(cpu_dai, 0, sys_clk, SND_SOC_CLOCK_IN);
	if (err < 0) {
		pr_err("cpu_dai clock not set\n");
		return err;
	}

	if ((!strcmp(rtd->dai->stream_name, "Tegra Voice Call")) &&
		(master_port == true)) {
		/*default set to 8k */
		int pcmdiv = WM8753_PCM_DIV_6;
		snd_soc_dai_set_pll(codec_dai, WM8753_PLL2, 0,
			sys_clk, 12288000);
		if (params_rate(params) == 16000) {
			pcmdiv = WM8753_PCM_DIV_3;
			/* BB expecting 2048Khz bclk*/
			snd_soc_dai_set_clkdiv(codec_dai, WM8753_VXCLKDIV,
							WM8753_VXCLK_DIV_2);
		}
		snd_soc_dai_set_clkdiv(codec_dai, WM8753_PCMDIV, pcmdiv);
	}

	if (vdac_enabled == true) {
		int value = snd_soc_read(codec_dai->codec, WM8753_SRATE1);
		value |= (WM8753_SRATE1_SRMODE);
		snd_soc_write(codec_dai->codec, WM8753_SRATE1, value);
	}
	return 0;
}

static int tegra_voice_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE) &&
	    (vdac_enabled == true)) {
		int value = snd_soc_read(codec_dai->codec,
				WM8753_SRATE1);
		value &= ~(WM8753_SRATE1_SRMODE);
		snd_soc_write(codec_dai->codec, WM8753_SRATE1, value);
		vdac_enabled = false;
	}
	return 0;
}

static int tegra_spdif_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	return 0;
}

int tegra_codec_startup(struct snd_pcm_substream *substream)
{
	tegra_das_power_mode(true);

	return 0;
}

void tegra_codec_shutdown(struct snd_pcm_substream *substream)
{
	tegra_das_power_mode(false);
}

int tegra_soc_suspend_pre(struct platform_device *pdev, pm_message_t state)
{
	disable_irq(gpio_to_irq(wm8753_jack->gpio));
	return 0;
}

int tegra_soc_suspend_post(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct tegra_audio_data* audio_data = socdev->codec_data;

	clk_disable(audio_data->dap_mclk);

	return 0;
}

int tegra_soc_resume_pre(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct tegra_audio_data* audio_data = socdev->codec_data;

	clk_enable(audio_data->dap_mclk);

	return 0;
}

int tegra_soc_resume_post(struct platform_device *pdev)
{
	enable_irq(gpio_to_irq(wm8753_jack->gpio));
	return 0;
}

static struct snd_soc_ops tegra_hifi_ops = {
	.hw_params = tegra_hifi_hw_params,
	.hw_free   = tegra_hifi_hw_free,
	.startup = tegra_codec_startup,
	.shutdown = tegra_codec_shutdown,
};

static struct snd_soc_ops tegra_voice_ops = {
	.hw_params = tegra_voice_hw_params,
	.hw_free   = tegra_voice_hw_free,
	.startup = tegra_codec_startup,
	.shutdown = tegra_codec_shutdown,
};

static struct snd_soc_ops tegra_spdif_ops = {
	.hw_params = tegra_spdif_hw_params,
};

void tegra_ext_control(struct snd_soc_codec *codec, int new_con)
{
	struct tegra_audio_data* audio_data = codec->socdev->codec_data;

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
		snd_soc_dapm_enable_pin(codec, "Int Spk");
	else
		snd_soc_dapm_disable_pin(codec, "Int Spk");

	if (new_con & TEGRA_INT_MIC)
		snd_soc_dapm_enable_pin(codec, "Int Mic");
	else
		snd_soc_dapm_disable_pin(codec, "Int Mic");

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

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);
	audio_data->codec_con = new_con;
}

/*tegra machine dapm widgets */
static const struct snd_soc_dapm_widget tegra_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_HP("EarPiece", NULL),
	SND_SOC_DAPM_HP("Headset Out", NULL),
	SND_SOC_DAPM_MIC("Headset In", NULL),
	SND_SOC_DAPM_SPK("Int Spk", NULL),
	SND_SOC_DAPM_MIC("Ext Mic", NULL),
	SND_SOC_DAPM_MIC("Int Mic", NULL),
	SND_SOC_DAPM_LINE("Linein", NULL),
};

/* Tegra machine audio map (connections to the codec pins) */
static const struct snd_soc_dapm_route audio_map[] = {

	/* headphone connected to LHPOUT1, RHPOUT1 */
	{"Headphone", NULL, "ROUT1"},
	{"Headphone", NULL, "LOUT1"},

	/* earpiece */
	{"EarPiece", NULL, "ROUT2"},
	{"EarPiece", NULL, "LOUT2"},

	/* headset Jack */
	{"Headset Out", NULL, "ROUT1"},
	{"Headset Out", NULL, "LOUT1"},
	{"MIC1", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Headset In"},

	/* build-in speaker */
	{"Int Spk", NULL, "ROUT1"},
	{"Int Spk", NULL, "LOUT1"},

	/* internal mic is mono */
	{"MIC1", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Int Mic"},

	/* external mic is stero */
	{"MIC2", NULL, "Mic Bias"},
	{"MIC2N", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Ext Mic"},

	/* Line in */
	{"LINE1", NULL, "Linein"},
	{"LINE2", NULL, "Linein"},
};

static void wm8753_intr_work(struct work_struct *work)
{
	unsigned int value;

	/* Do something here */
	mutex_lock(&wm8753_jack->pcodec->mutex);

	/* GPIO4 interrupt disable (also disable other interrupts) */
	value = snd_soc_read(wm8753_jack->pcodec, WM8753_INTEN);
	value &= ~(WM8753_INTEN_MICSHTEN | WM8753_INTEN_MICDETEN |
			  WM8753_INTEN_GPIO3IEN | WM8753_INTEN_HPSWIEN |
			  WM8753_INTEN_GPIO5IEN | WM8753_INTEN_TSDIEN |
			  WM8753_INTEN_GPIO4IEN);
	snd_soc_write(wm8753_jack->pcodec, WM8753_INTEN, value);

	/* Invert GPIO4 interrupt polarity */
	value = snd_soc_read(wm8753_jack->pcodec, WM8753_INTPOL);
	value &= WM8753_INTPOL_GPIO4IPOL;
	if ((value) & ((WM8753_INTPOL_GPIO4IPOL))) {
		tegra_switch_set_state(BIT_HEADSET_NO_MIC);
		snd_jack_report(wm8753_jack->jack, SND_JACK_HEADPHONE);
		value &= ~(WM8753_INTPOL_GPIO4IPOL);
	}
	else {
		tegra_switch_set_state(BIT_NO_HEADSET);
		snd_jack_report(wm8753_jack->jack, 0);
		value |= (WM8753_INTPOL_GPIO4IPOL);
	}
	snd_soc_write(wm8753_jack->pcodec, WM8753_INTPOL, value);

	/* GPIO4 interrupt enable */
	value = snd_soc_read(wm8753_jack->pcodec, WM8753_INTEN);
	value |= (WM8753_INTEN_GPIO4IEN);
	value &= ~(WM8753_INTEN_MICSHTEN | WM8753_INTEN_MICDETEN |
			  WM8753_INTEN_GPIO3IEN | WM8753_INTEN_HPSWIEN |
			  WM8753_INTEN_GPIO5IEN | WM8753_INTEN_TSDIEN);
	snd_soc_write(wm8753_jack->pcodec, WM8753_INTEN, value);

	mutex_unlock(&wm8753_jack->pcodec->mutex);
}

static irqreturn_t wm8753_irq(int irq, void *data)
{
	schedule_work(&wm8753_jack->work);
	return IRQ_HANDLED;
}

static int tegra_codec_init(struct snd_soc_codec *codec)
{
	struct tegra_audio_data* audio_data = codec->socdev->codec_data;
	int ret = 0;
	unsigned int value;

	if (!audio_data->init_done) {
		audio_data->dap_mclk = tegra_das_get_dap_mclk();
		if (!audio_data->dap_mclk) {
			pr_err("Failed to get dap mclk \n");
			ret = -ENODEV;
			return ret;
		}
		clk_enable(audio_data->dap_mclk);

		/* Add tegra specific widgets */
		snd_soc_dapm_new_controls(codec, tegra_dapm_widgets,
					ARRAY_SIZE(tegra_dapm_widgets));

		/* Set up tegra specific audio path audio_map */
		snd_soc_dapm_add_routes(codec, audio_map,
					ARRAY_SIZE(audio_map));

		/* Add jack detection */
		ret = tegra_jack_init(codec);
		if (ret < 0) {
			pr_err("Failed in jack init \n");
			return ret;
		}

		/* Default to OFF */
		tegra_ext_control(codec, TEGRA_AUDIO_OFF);

		ret = tegra_controls_init(codec);
		if (ret < 0) {
			pr_err("Failed in controls init \n");
			return ret;
		}

		if (!wm8753_jack) {
			wm8753_jack = kzalloc(sizeof(*wm8753_jack), GFP_KERNEL);
			if (!wm8753_jack) {
				pr_err("failed to allocate wm8753-jack\n");
				return -ENOMEM;
			}

			wm8753_jack->gpio = TEGRA_GPIO_PW3;
			wm8753_jack->pcodec = codec;

			INIT_WORK(&wm8753_jack->work, wm8753_intr_work);

			ret = snd_jack_new(codec->card, "Headphone Jack", SND_JACK_HEADPHONE,
				&wm8753_jack->jack);
			if (ret < 0)
				goto failed;

			ret = gpio_request(wm8753_jack->gpio, "headphone-detect-gpio");
			if (ret)
				goto failed;

			ret = gpio_direction_input(wm8753_jack->gpio);
			if (ret)
				goto gpio_failed;

			tegra_gpio_enable(wm8753_jack->gpio);

			ret = request_irq(gpio_to_irq(wm8753_jack->gpio),
					wm8753_irq,
					IRQF_TRIGGER_FALLING,
					"wm8753",
					wm8753_jack);

			if (ret)
				goto gpio_failed;

			/* Configure GPIO2 pin to generate the interrupt */
			value = snd_soc_read(codec, WM8753_GPIO2);
			value |= (WM8753_GPIO2_GP2M_0 | WM8753_GPIO2_GP2M_1);
			value &= ~(WM8753_GPIO2_GP2M_2);
			snd_soc_write(codec, WM8753_GPIO2, value);

			/* Active low Interrupt */
			value = snd_soc_read(codec, WM8753_GPIO1);
			value |= (WM8753_GPIO1_INTCON_1 | WM8753_GPIO1_INTCON_0);
			snd_soc_write(codec, WM8753_GPIO1, value);

			/* GPIO4 interrupt polarity -- interupt when low i.e Headphone connected */
			value = snd_soc_read(codec, WM8753_INTPOL);
			value |= (WM8753_INTPOL_GPIO4IPOL);
			snd_soc_write(codec, WM8753_INTPOL, value);

			/* GPIO4 interrupt enable and disable other interrupts */
			value = snd_soc_read(codec, WM8753_INTEN);
			value |= (WM8753_INTEN_GPIO4IEN);
			value &= ~(WM8753_INTEN_MICSHTEN | WM8753_INTEN_MICDETEN |
				      WM8753_INTEN_GPIO3IEN | WM8753_INTEN_HPSWIEN |
				      WM8753_INTEN_GPIO5IEN | WM8753_INTEN_TSDIEN);
			snd_soc_write(codec, WM8753_INTEN, value);
		}

		audio_data->codec = codec;
		audio_data->init_done = 1;
	}

	return ret;

gpio_failed:
	gpio_free(wm8753_jack->gpio);
failed:
	kfree(wm8753_jack);
	wm8753_jack = NULL;
	return ret;
}

static struct snd_soc_dai_link tegra_soc_dai[] = {
	{
		.name = "WM8753",
		.stream_name = "WM8753 HiFi",
		.cpu_dai = &tegra_i2s_dai[0],
		.codec_dai = &wm8753_dai[WM8753_DAI_HIFI],
		.init = tegra_codec_init,
		.ops = &tegra_hifi_ops,
	},
	{
		.name = "Tegra-Voice",
		.stream_name = "Tegra BT-SCO Voice",
		.cpu_dai = &tegra_i2s_dai[1],
		.codec_dai = &tegra_generic_codec_dai[TEGRA_BT_CODEC_ID],
		.init = tegra_codec_init,
		.ops = &tegra_voice_ops,
	},
	{
		.name = "Tegra-spdif",
		.stream_name = "Tegra Spdif",
		.cpu_dai = &tegra_spdif_dai,
		.codec_dai = &tegra_generic_codec_dai[TEGRA_SPDIF_CODEC_ID],
		.init = tegra_codec_init,
		.ops = &tegra_spdif_ops,
	},
	{
		.name = "Tegra-voice-call",
		.stream_name = "Tegra Voice Call",
		.cpu_dai = &tegra_generic_codec_dai[TEGRA_BB_CODEC_ID],
		.codec_dai = &wm8753_dai[WM8753_DAI_VOICE],
		.init = tegra_codec_init,
		.ops = &tegra_voice_ops,
	},
	{
		.name = "Tegra-bt-voice-call",
		.stream_name = "Tegra BT Voice Call",
		.cpu_dai = &tegra_generic_codec_dai[TEGRA_BB_CODEC_ID],
		.codec_dai = &tegra_generic_codec_dai[TEGRA_BT_CODEC_ID],
		.init = tegra_codec_init,
		.ops = &tegra_voice_ops,
	},
};

static struct tegra_audio_data audio_data = {
	.init_done = 0,
	.play_device = TEGRA_AUDIO_DEVICE_NONE,
	.capture_device = TEGRA_AUDIO_DEVICE_NONE,
	.is_call_mode = false,
	.codec_con = TEGRA_AUDIO_OFF,
};

static struct snd_soc_card tegra_snd_soc = {
	.name = "tegra",
	.platform = &tegra_soc_platform,
	.dai_link = tegra_soc_dai,
	.num_links = ARRAY_SIZE(tegra_soc_dai),
	.suspend_pre = tegra_soc_suspend_pre,
	.suspend_post = tegra_soc_suspend_post,
	.resume_pre = tegra_soc_resume_pre,
	.resume_post = tegra_soc_resume_post,
};

static struct snd_soc_device tegra_snd_devdata = {
	.card = &tegra_snd_soc,
	.codec_dev = &soc_codec_dev_wm8753,
	.codec_data = &audio_data,
};

static int __init tegra_init(void)
{
	int ret = 0;

	tegra_snd_device = platform_device_alloc("soc-audio", -1);
	if (!tegra_snd_device) {
		pr_err("failed to allocate soc-audio \n");
		return ENOMEM;
	}

	platform_set_drvdata(tegra_snd_device, &tegra_snd_devdata);
	tegra_snd_devdata.dev = &tegra_snd_device->dev;
	ret = platform_device_add(tegra_snd_device);
	if (ret) {
		pr_err("audio device could not be added \n");
		goto fail;
	}

	wm8753_reg = regulator_get(NULL, "avddio_audio");
	if (IS_ERR(wm8753_reg)) {
		ret = PTR_ERR(wm8753_reg);
		pr_err("unable to get wm8753 regulator\n");
		goto fail;
	}

	ret = regulator_enable(wm8753_reg);
	if (ret) {
		pr_err("wm8753 regulator enable failed\n");
		goto err_put_regulator;
	}

	return 0;

fail:
	if (tegra_snd_device) {
		platform_device_put(tegra_snd_device);
		tegra_snd_device = 0;
	}

	return ret;

err_put_regulator:
	regulator_put(wm8753_reg);
	return ret;
}

static void __exit tegra_exit(void)
{
	platform_device_unregister(tegra_snd_device);
	regulator_disable(wm8753_reg);
	regulator_put(wm8753_reg);

	if (wm8753_jack) {
		gpio_free(wm8753_jack->gpio);
		kfree(wm8753_jack);
		wm8753_jack = NULL;
	}
}

module_init(tegra_init);
module_exit(tegra_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra ALSA SoC");
MODULE_LICENSE("GPL");

