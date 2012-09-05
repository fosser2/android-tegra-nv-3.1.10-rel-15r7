/*
 * arch/arm/mach-tegra/board-smba1002-audio.c
 *
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* All configurations related to audio */
/*#define ALC5623_IS_MASTER*/
 
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <sound/alc5623.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>
#include <asm/io.h>

#include <mach/tegra_alc5623_pdata.h>
#include <mach/io.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/gpio.h>
#include <mach/spdif.h>
#include <mach/audio.h>  

#include <mach/system.h>

#include "board.h"
#include "board-smba1002.h"
#include "gpio-names.h"
#include "devices.h"

/* Default music path: I2S1(DAC1)<->Dap1<->HifiCodec
   Bluetooth to codec: I2S2(DAC2)<->Dap4<->Bluetooth
*/
/* For SMBA1002, 
	Codec is ALC5623
	Codec I2C Address = 0x34(includes R/W bit), i2c #0
	Codec MCLK = APxx DAP_MCLK1
*/


static struct tegra_audio_platform_data tegra_spdif_pdata = {
	.dma_on			= true,  /* use dma by default */
	.mask			= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
	.stereo_capture = true,
};

static struct tegra_audio_platform_data tegra_audio_pdata[] = {
	/* For I2S1 - Hifi */
	[0] = {
#ifdef ALC5623_IS_MASTER
		.i2s_master		= false,	/* CODEC is master for audio */
		.dma_on			= true,  	/* use dma by default */
		.i2s_clk_rate 	= 2822400,
		.dap_clk	  	= "cdev1",
		.audio_sync_clk = "audio_2x",
		.mode			= I2S_BIT_FORMAT_I2S,
		.fifo_fmt		= I2S_FIFO_16_LSB,
		.bit_size		= I2S_BIT_SIZE_16,
#else
		.i2s_master		= true,		/* CODEC is slave for audio */
		.dma_on			= true,  	/* use dma by default */
#ifdef SMBA1002_48KHZ_AUDIO						
		.i2s_master_clk = 48000,
		.i2s_clk_rate 	= 12288000,
#else
		.i2s_master_clk = 44100,
		.i2s_clk_rate 	= 11289600,
#endif
		.dap_clk	  	= "cdev1",
		.audio_sync_clk = "audio_2x",
		.mode			= I2S_BIT_FORMAT_I2S,
		.fifo_fmt		= I2S_FIFO_PACKED,
		.bit_size		= I2S_BIT_SIZE_16,
		.i2s_bus_width	= 32,
#endif
		.mask			= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
		.stereo_capture = true,
	},
	/* For I2S2 - Bluetooth */
	[1] = {
		.i2s_master		= false,	/* bluetooth is master always */
		.dma_on			= true,  /* use dma by default */
		.i2s_master_clk = 8000,
		.dsp_master_clk = 8000,
		.i2s_clk_rate	= 2000000,
		.dap_clk		= "cdev1",
		.audio_sync_clk = "audio_2x",
		.mode			= I2S_BIT_FORMAT_DSP,
		.fifo_fmt		= I2S_FIFO_16_LSB,
		.bit_size		= I2S_BIT_SIZE_16,
		.i2s_bus_width 	= 32,
		.dsp_bus_width 	= 16,
		.mask			= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
		.stereo_capture = true,
	}
}; 

static struct alc5623_platform_data smba_alc5623_pdata = {
	.add_ctrl	= 0xD300,
	.jack_det_ctrl	= 0,
	.avdd_mv		= 3300,	/* Analog vdd in millivolts */

	.mic1bias_mv		= 2475,	/* MIC1 bias voltage */
	.mic1boost_db    	= 20,  /* MIC1 gain boost */
	.mic2boost_db   	= 20,  /* MIC2 gain boost */

	.default_is_mic2 	= false,	/* SMBA1002 uses MIC1 as the default capture source */

};

static struct i2c_board_info __initdata smba_i2c_bus0_board_info[] = {
	{
		I2C_BOARD_INFO("alc5623", 0x1a),
		.platform_data = &smba_alc5623_pdata,
	},
};


static struct tegra_alc5623_platform_data smba_audio_pdata = {
        .gpio_spkr_en           = -2,
        .gpio_hp_det            = SMBA1002_HP_DETECT,
	.gpio_int_mic_en 	= SMBA1002_INT_MIC_EN,
	.hifi_codec_datafmt = SND_SOC_DAIFMT_I2S,	/* HiFi codec data format */
#ifdef ALC5623_IS_MASTER
	.hifi_codec_master  = true,					/* If Hifi codec is master */
#else
	.hifi_codec_master  = false,				/* If Hifi codec is master */
#endif
	.bt_codec_datafmt   = SND_SOC_DAIFMT_DSP_A,	/* Bluetooth codec data format */
	.bt_codec_master    = true,					/* If bt codec is master */

};

static struct platform_device tegra_generic_codec = {
	.name = "tegra-generic-codec",
	.id   = -1,
};

static struct platform_device smba_audio_device = {
	.name = "tegra-snd-alc5623",
	.id   = 0,
        .dev    = {
                .platform_data  = &smba_audio_pdata,
        },

};

static struct platform_device *smba_i2s_devices[] __initdata = {
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_das_device,
	&spdif_dit_device,
	&tegra_pcm_device,
	&tegra_generic_codec,
	&smba_audio_device, /* this must come last, as we need the DAS to be initialized to access the codec registers ! */
};


int  __init smba_audio_register_devices(void)
{
	int ret;

        pr_info("%s++ \n", __func__);

	/* Patch in the platform data */
	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata[0];
	tegra_i2s_device2.dev.platform_data = &tegra_audio_pdata[1];
	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;

        ret = i2c_register_board_info(0, smba_i2c_bus0_board_info,
                ARRAY_SIZE(smba_i2c_bus0_board_info));
        if (ret)
                return ret;

        return platform_add_devices(smba_i2s_devices, ARRAY_SIZE(smba_i2s_devices));
}


