/*
 * tegra_soc_generic_codec.c  --  SoC audio for tegra
 *
 * (c) 2010-2011 Nvidia Graphics Pvt. Ltd.
 *  http://www.nvidia.com
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
#include "../codecs/generic_codec.h"

static struct platform_device *tegra_snd_device;

extern struct snd_soc_dai tegra_i2s_dai[];
extern struct snd_soc_platform tegra_soc_platform;

static int tegra_hifi_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int dai_flag = 0, sys_clk;
	int err;

	enum dac_dap_data_format data_fmt;

	/* Program the cpu_dai only */

	data_fmt = dac_dap_data_format_i2s;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	data_fmt = tegra_das_get_codec_data_fmt(tegra_audio_codec_type_hifi);
	if (tegra_das_is_port_master(tegra_audio_codec_type_hifi))
		dai_flag |= SND_SOC_DAIFMT_CBM_CFM;
	else
#endif
		dai_flag |= SND_SOC_DAIFMT_CBS_CFS;

	if ((data_fmt & dac_dap_data_format_tdm))
		dai_flag |= SND_SOC_DAIFMT_DSP_A;
	else if ((data_fmt & dac_dap_data_format_rjm))
		dai_flag |= SND_SOC_DAIFMT_RIGHT_J;
	else if ((data_fmt & dac_dap_data_format_i2s))
		dai_flag |= SND_SOC_DAIFMT_I2S;
	else if ((data_fmt & dac_dap_data_format_ljm))
		dai_flag |= SND_SOC_DAIFMT_LEFT_J;


	err = snd_soc_dai_set_fmt(cpu_dai, dai_flag);
	if (err < 0) {
		pr_err("cpu_dai fmt not set\n");
		return err;
	}

	sys_clk = 48000 * 512;
	err = snd_soc_dai_set_sysclk(cpu_dai, 0, sys_clk, SND_SOC_CLOCK_IN);
	if (err < 0) {
		pr_err("cpu_dai clock not set\n");
		return err;
	}

	return 0;
}

void tegra_ext_control(struct snd_soc_codec *codec, int new_con)
{
	return;
}

int tegra_codec_startup(struct snd_pcm_substream *substream)
{
	tegra_das_power_mode(true);
	return 0;
}

void tegra_codec_shutdown(struct snd_pcm_substream *substream)
{
}

int tegra_soc_suspend_pre(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int tegra_soc_suspend_post(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

int tegra_soc_resume_pre(struct platform_device *pdev)
{
	return 0;
}

int tegra_soc_resume_post(struct platform_device *pdev)
{
	return 0;
}

static struct snd_soc_ops tegra_hifi_ops = {
	.hw_params = tegra_hifi_hw_params,
	.startup = tegra_codec_startup,
	.shutdown = tegra_codec_shutdown,
};

static int tegra_codec_init(struct snd_soc_codec *codec)
{
	struct tegra_audio_data *audio_data = codec->socdev->codec_data;
	int err = 0;

	if (!audio_data->init_done) {

		err = tegra_das_open();
		if (err) {
			pr_err("Failed get dap mclk\n");
			err = -ENODEV;
			goto generic_codec_init_fail;
		}

		err = tegra_das_enable_mclk();
		if (err) {
			pr_err("Failed to enable dap mclk\n");
			err = -ENODEV;
			goto generic_codec_init_fail;
		}

		err = tegra_controls_init(codec);
		if (err < 0) {
			pr_err("Failed in controls init\n");
			goto generic_codec_init_fail;
		}

		audio_data->codec = codec;
		audio_data->init_done = 1;
	}

	return err;

generic_codec_init_fail:

	tegra_das_disable_mclk();
	tegra_das_close();
	return err;
}

#define TEGRA_CREATE_SOC_DAI_LINK(xname, xstreamname, \
			xcpudai, xcodecdai, xops) \
{							\
	.name = xname,					\
	.stream_name = xstreamname,     \
	.cpu_dai = xcpudai,             \
	.codec_dai = xcodecdai,         \
	.init = tegra_codec_init,       \
	.ops = xops,                    \
}

/* Currently both the DAI link to the same CODEC dai
 * as only one of them can be active at the same time
 * */
static struct snd_soc_dai_link tegra_soc_dai[] = {
	TEGRA_CREATE_SOC_DAI_LINK("Generic DIT Codec", "Multi-8",
				  &tegra_i2s_dai[0], &generic_dit_stub_dai,
				  &tegra_hifi_ops),

	TEGRA_CREATE_SOC_DAI_LINK("Generic DIT Codec", "Multi-16",
				  &tegra_i2s_dai[1],
				  &generic_dit_stub_dai, &tegra_hifi_ops),
};

static struct tegra_audio_data audio_data = {
	.init_done = 0,
	.play_device = TEGRA_AUDIO_DEVICE_NONE,
	.capture_device = TEGRA_AUDIO_DEVICE_NONE,
	.is_call_mode = false,
	.codec_con = TEGRA_AUDIO_OFF,
};

static struct snd_soc_card tegra_snd_soc = {
	.name = "tegra-generic",
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
	.codec_data = &audio_data,
	.codec_dev = &soc_codec_dev_generic_dit,
};

static int __init tegra_init(void)
{
	int ret = 0;

	tegra_snd_device = platform_device_alloc("soc-audio", -1);
	if (!tegra_snd_device) {
		pr_err("failed to allocate soc-audio\n");
		return -ENOMEM;
	}

	platform_set_drvdata(tegra_snd_device, &tegra_snd_devdata);
	tegra_snd_devdata.dev = &tegra_snd_device->dev;

	ret = platform_device_add(tegra_snd_device);
	if (ret) {
		pr_err("audio device could not be added\n");
		goto fail;
	}

	return 0;

fail:
	if (tegra_snd_device) {
		platform_device_put(tegra_snd_device);
		tegra_snd_device = 0;
	}

	return ret;
}

static void __exit tegra_exit(void)
{
	platform_device_unregister(tegra_snd_device);
}

module_init(tegra_init);
module_exit(tegra_exit);

/* Module information */
MODULE_DESCRIPTION("Tegra ALSA SoC for Generic Codec");
MODULE_LICENSE("GPL");
