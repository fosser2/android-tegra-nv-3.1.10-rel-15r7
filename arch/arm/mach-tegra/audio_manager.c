/*
 * linux/arch/arm/mach-tegra/audio_manager.c
 *
 * Audio Manager for tegra soc
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation
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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/err.h>

#include "clock.h"
#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <mach/tegra_das.h>
#include <mach/tegra_i2s.h>
#include <mach/tegra3_i2s.h>
#include <mach/spdif.h>
#include <mach/audio_manager.h>

#define  ENABLE_AM_DEBUG_PRINT	0

#if ENABLE_AM_DEBUG_PRINT
#define AM_DEBUG_PRINT(fmt, arg...) printk(fmt, ## arg)
#else
#define AM_DEBUG_PRINT(fmt, arg...) do {} while (0)
#endif

#define LINEAR_UNITY_GAIN 0x1000

static struct am_dev_fns init_am_dev_fns[] = {
[AUDIO_I2S_DEVICE] = {
	.aud_dev_suspend = i2s_suspend,
	.aud_dev_resume = i2s_resume,
	.aud_dev_set_stream_state = i2s_fifo_enable,
	.aud_dev_get_dma_requestor = i2s_get_dma_requestor,
	.aud_dev_free_dma_requestor = i2s_free_dma_requestor,
	.aud_dev_get_fifo_phy_base = i2s_get_fifo_phy_base,
	.aud_dev_set_fifo_attention = i2s_set_fifo_attention,
	.aud_dev_get_status = i2s_get_status,
	.aud_dev_clock_disable = i2s_clock_disable,
	.aud_dev_clock_enable = i2s_clock_enable,
	.aud_dev_clock_set_parent = i2s_clock_set_parent,
	.aud_dev_clock_set_rate = i2s_clock_set_rate,
	.aud_dev_deinit = i2s_close,
	},

[AUDIO_SPDIF_DEVICE] = {
	.aud_dev_suspend = spdif_suspend,
	.aud_dev_resume = spdif_resume,
	.aud_dev_set_stream_state = spdif_fifo_enable,
	.aud_dev_get_dma_requestor = spdif_get_dma_requestor,
	.aud_dev_free_dma_requestor = spdif_free_dma_requestor,
	.aud_dev_get_fifo_phy_base = spdif_get_fifo_phy_base,
	.aud_dev_set_fifo_attention = spdif_set_fifo_attention,
	.aud_dev_get_status = spdif_get_status,
	.aud_dev_clock_disable = spdif_clock_disable,
	.aud_dev_clock_enable = spdif_clock_enable,
	.aud_dev_clock_set_parent = spdif_clock_set_parent,
	.aud_dev_clock_set_rate = spdif_clock_set_rate,
	.aud_dev_deinit = spdif_close,
	},

};

#ifdef CONFIG_ARCH_TEGRA_2x_SOC

/* dummy calls */
int audio_switch_open(void) { return 0; }
int audio_switch_close(void) { return 0; }
int audio_switch_suspend(void) { return 0; }
int audio_switch_resume(void) { return 0; }
int audio_switch_enable_clock(void) { return 0; }
void audio_switch_disable_clock(void) {}

int am_suspend(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_suspend(devinfo->dev_id);
}

int am_resume(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_resume(devinfo->dev_id);
}

int am_set_stream_state(aud_dev_info* devinfo, bool enable)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_set_stream_state(
				devinfo->dev_id,
				devinfo->fifo_mode,
				((enable)? 1 : 0));
}

int am_get_dma_requestor(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_get_dma_requestor(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_free_dma_requestor(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_free_dma_requestor(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

phys_addr_t am_get_fifo_phy_base(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_get_fifo_phy_base(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_set_fifo_attention(aud_dev_info* devinfo, int buffersize)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_set_fifo_attention(
				devinfo->dev_id,
				buffersize,
				devinfo->fifo_mode);
}

u32 am_get_status(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_get_status(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_clock_disable(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_disable(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_clock_enable(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_enable(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_clock_set_parent(aud_dev_info* devinfo, int parent)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_set_parent(
				devinfo->dev_id,
				devinfo->fifo_mode,
				parent);
}

int am_clock_set_rate(aud_dev_info* devinfo, int rate)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_set_rate(
				devinfo->dev_id,
				devinfo->fifo_mode,
				rate);
}

int am_device_deinit(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_deinit(devinfo->dev_id);
}

int am_set_stream_format(aud_dev_info* devinfo, am_stream_format_info *format)
{
	int dev_id = devinfo->dev_id;

	if (devinfo->dev_type == AUDIO_I2S_DEVICE) {
		i2s_set_bit_size(dev_id, format->bitsize);
		i2s_set_samplerate(dev_id, format->samplerate);
		i2s_set_channels(dev_id, format->channels);
		i2s_set_fifo_attention(dev_id,
			devinfo->fifo_mode, format->buffersize);

	} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {
		spdif_set_bit_mode(dev_id, format->bitsize);
		/* fixme - move to appropriate locn later */
		spdif_set_fifo_packed(dev_id, 1);

		spdif_set_sample_rate(dev_id,
			devinfo->fifo_mode, format->samplerate);
		spdif_set_fifo_attention(dev_id,
			devinfo->fifo_mode, format->buffersize);
	}
	return 0;
}

int am_set_device_format(aud_dev_info* devinfo, am_dev_format_info *format)
{
	if (devinfo->dev_type == AUDIO_I2S_DEVICE) {
		i2s_set_loopback(devinfo->dev_id, format->loopmode);
		i2s_set_master(devinfo->dev_id, format->mastermode);
		i2s_set_bit_format(devinfo->dev_id, format->audiomode);
		i2s_set_left_right_control_polarity(
				devinfo->dev_id,
				format->polarity);

	} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {

	}
	return 0;
}

int am_device_init(aud_dev_info* devinfo, void *dev_fmt, void  *strm_fmt)
{
	am_stream_format_info  *sfmt = (am_stream_format_info*)strm_fmt;
	am_dev_format_info *dfmt = (am_dev_format_info*)dev_fmt;

	if (devinfo->dev_type == AUDIO_I2S_DEVICE) {
		struct tegra_i2s_property i2sprop;


		memset(&i2sprop, 0, sizeof(i2sprop));

		if (sfmt) {
			i2sprop.bit_size = sfmt->bitsize;
			i2sprop.sample_rate = sfmt->samplerate;
		}

		if (dfmt) {
			i2sprop.master_mode = dfmt->mastermode;
			i2sprop.audio_mode = dfmt->audiomode;
			i2sprop.clk_rate = dfmt->clkrate;
			i2sprop.fifo_fmt = dfmt->fifofmt;
		}

		return i2s_init(devinfo->dev_id, &i2sprop);

	} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {

		struct tegra_spdif_property spdifprop;
		memset(&spdifprop, 0, sizeof(spdifprop));

		if (dfmt) {
			spdifprop.clk_rate = dfmt->clkrate;

			return spdif_init(
				devinfo->base,
				devinfo->phy_base,
				devinfo->fifo_mode,
				&spdifprop);
		}
	}
	return 0;
}

#else

struct am_ch_info {
	bool inuse;
	int outsample;
	int ahubrx_index;
	int ahubtx_index;
	int damch[AUDIO_FIFO_CNT];
	int dmach[AUDIO_FIFO_CNT];
	int dmach_refcnt[AUDIO_FIFO_CNT];
	am_stream_format_info sfmt;
	struct audio_cif inacif;
	struct audio_cif outcif;
};

struct audio_manager_context {
	struct clk *mclk;
	struct clk *pmc_clk;
	int mclk_refcnt;
	int mclk_rate;
	int mclk_parent;
	struct am_ch_info i2s_ch[NR_I2S_IFC];
	struct am_ch_info spdif_ch;
};


struct audio_manager_context *aud_manager;

int default_music_connection(struct am_ch_info *ch, int ifc, int fifo_mode)
{
	/* get unused dam channel first */
	ch->damch[fifo_mode] = dam_get_controller();

	if (ch->damch[fifo_mode] == -ENOENT) {
		pr_err("unable to get the dam channel\n");
		return -ENOENT;
	}

	if (fifo_mode == AUDIO_TX_MODE)
		ch->ahubtx_index = ahubtx_dam0 + ch->damch[fifo_mode];
	else if (fifo_mode == AUDIO_RX_MODE)
		ch->ahubrx_index = ahubrx1_dam0 + (ch->damch[fifo_mode] << 1);

	/* Enable the clock if needed */

	/* FIXME: Take the property from the destination device
	 - it shoule the stream format also*/
	dam_set_samplerate(ch->damch[fifo_mode],
			 dam_ch_out, ch->sfmt.samplerate);

	/*ch 0 expecting as 8Khz */
	dam_set_samplerate(ch->damch[fifo_mode],
			 dam_ch_in0, AUDIO_SAMPLERATE_8000);

	/*Apbif dma going through DAM */
	ch->dmach[fifo_mode] = dam_get_dma_requestor(ch->damch[fifo_mode],
				dam_ch_in1, fifo_mode);

	if (ch->dmach[fifo_mode] == -ENOENT)
		goto conn_fail;

	audio_switch_set_rx_port(ch->ahubrx_index, ch->ahubtx_index);

	ch->inacif.audio_channels = ch->sfmt.channels;
	ch->inacif.client_channels = ch->sfmt.channels;
	ch->inacif.audio_bits = ch->sfmt.bitsize;
	ch->inacif.client_bits = ch->sfmt.bitsize;

	dam_set_acif(ch->damch[fifo_mode], dam_ch_out, &ch->inacif);
	dam_set_acif(ch->damch[fifo_mode], dam_ch_in1, &ch->inacif);

	/* set unity gain to damch1 */
	dam_set_gain(ch->damch[fifo_mode], dam_ch_in1, LINEAR_UNITY_GAIN);
	return 0;

conn_fail:

	dam_free_controller(ch->damch[fifo_mode]);
	return -ENOENT;
}

int test_free_dam_connection(struct am_ch_info *ch, int ifc, int fifo_mode)
{
	dam_free_dma_requestor(ch->damch[fifo_mode], dam_ch_in1, fifo_mode);

	audio_switch_clear_rx_port(ch->ahubrx_index);

	dam_free_controller(ch->damch[fifo_mode]);

	ch->damch[fifo_mode] = -1;
	return 0;
}

int am_suspend(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_suspend(devinfo->dev_id);
}

int am_resume(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_resume(devinfo->dev_id);
}

int am_set_stream_state(aud_dev_info* devinfo, bool enable)
{
	struct am_ch_info *ch = NULL;
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	int dev_id = devinfo->dev_id;
	int fifo_mode = devinfo->fifo_mode;
	int on_off = (enable) ? 1 : 0;

	if (devinfo->dev_type == AUDIO_I2S_DEVICE)
		ch = &aud_manager->i2s_ch[dev_id];
	else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE)
		ch = &aud_manager->spdif_ch;

	if (ch->damch[fifo_mode] != -1)
		dam_enable(ch->damch[fifo_mode], on_off, dam_ch_in1);

	return am_fn->aud_dev_set_stream_state(dev_id, fifo_mode, on_off);
}

int am_get_dma_requestor(aud_dev_info* devinfo)
{
	int err = 0;
	struct am_ch_info *ch = NULL;
	int ahubindex = 0;
	int dev_id = devinfo->dev_id;
	int fifo_mode = devinfo->fifo_mode;

	if (devinfo->dev_type == AUDIO_I2S_DEVICE) {
		ch = &aud_manager->i2s_ch[dev_id];
		ahubindex  = dev_id + ahubrx_i2s0;

		if (fifo_mode == AUDIO_RX_MODE)
			ahubindex = dev_id + ahubtx_i2s0;

	} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {
		ch = &aud_manager->spdif_ch;
		ahubindex = ahubrx0_spdif;

		if (fifo_mode == AUDIO_RX_MODE)
			ahubindex = ahubtx0_spdif;
	}

	if (ch->inuse == false) {

		ch->ahubrx_index = ahubindex;

		if (fifo_mode == AUDIO_RX_MODE)
			ch->ahubtx_index = ahubindex;

		err = default_music_connection(ch, dev_id, fifo_mode);
		if (err)
			goto fail_conn;

		if (devinfo->dev_type == AUDIO_I2S_DEVICE) {
			i2s_set_dma_channel(dev_id,
				fifo_mode, (ch->dmach[fifo_mode] - 1));
			i2s_set_acif(dev_id, fifo_mode, &ch->inacif);
		} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {
			spdif_set_dma_channel(dev_id,
				fifo_mode, (ch->dmach[fifo_mode] - 1));
			spdif_set_acif(dev_id, fifo_mode, (void *)&ch->inacif);
		}
	}

	if (!err) {
		ch->inuse = true;
		ch->dmach_refcnt[fifo_mode]++;
		return ch->dmach[fifo_mode];
	}

	return err;

fail_conn:
	/*free connection */
	return err;
}

int am_free_dma_requestor(aud_dev_info* devinfo)
{
	struct am_ch_info *ch = NULL;
	int dev_id = devinfo->dev_id;
	int fifo_mode = devinfo->fifo_mode;

	if (devinfo->dev_type == AUDIO_I2S_DEVICE)
		ch = &aud_manager->i2s_ch[dev_id];
	else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE)
		ch = &aud_manager->spdif_ch;

	AM_DEBUG_PRINT("%s++ %d  refcnt %d\n", __func__,
			ch->dmach[fifo_mode],
			ch->dmach_refcnt[fifo_mode]);

	if (ch->dmach_refcnt[fifo_mode] > 0)
		ch->dmach_refcnt[fifo_mode]--;

	if (ch->dmach_refcnt[devinfo->fifo_mode] == 0) {
		test_free_dam_connection(ch, dev_id, fifo_mode);
		ch->inuse = false;
	}
	AM_DEBUG_PRINT("%s++ %d  refcnt %d\n", __func__,
			ch->dmach[fifo_mode],
			ch->dmach_refcnt[fifo_mode]);
	return 0;

}

phys_addr_t am_get_fifo_phy_base(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_get_fifo_phy_base(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_set_fifo_attention(aud_dev_info* devinfo, int buffersize)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_set_fifo_attention(
				devinfo->dev_id,
				buffersize,
				devinfo->fifo_mode);
}

u32 am_get_status(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_get_status(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_clock_disable(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_disable(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_clock_enable(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_enable(
				devinfo->dev_id,
				devinfo->fifo_mode);
}

int am_clock_set_parent(aud_dev_info* devinfo, int parent)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_set_parent(
				devinfo->dev_id,
				devinfo->fifo_mode,
				parent);
}

int am_clock_set_rate(aud_dev_info* devinfo, int rate)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_clock_set_rate(
				devinfo->dev_id,
				devinfo->fifo_mode,
				rate);
}

int am_device_deinit(aud_dev_info* devinfo)
{
	struct am_dev_fns* am_fn = &init_am_dev_fns[devinfo->dev_type];
	return am_fn->aud_dev_deinit(devinfo->dev_id);
}

int am_set_stream_format(aud_dev_info* devinfo, am_stream_format_info *format)
{
	int dev_id = devinfo->dev_id;
	struct am_ch_info *ch = NULL;

	if (devinfo->dev_type == AUDIO_I2S_DEVICE) {
		ch = &aud_manager->i2s_ch[dev_id];
		i2s_set_bit_size(dev_id, format->bitsize);
		i2s_set_samplerate(dev_id, format->samplerate);
		i2s_set_channels(dev_id, format->channels);
		i2s_set_fifo_attention(dev_id,
			devinfo->fifo_mode, format->buffersize);

	} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {
		ch = &aud_manager->spdif_ch;
		spdif_set_bit_mode(dev_id, format->bitsize);
		spdif_set_sample_rate(dev_id,
			devinfo->fifo_mode,
			format->samplerate);
		spdif_set_fifo_attention(dev_id,
			devinfo->fifo_mode, format->buffersize);
	}

	memcpy(&ch->sfmt, format, sizeof(am_stream_format_info));

	if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {
		switch (format->bitsize) {
		case SPDIF_BIT_MODE_MODE24BIT:
			ch->sfmt.bitsize = AUDIO_BIT_SIZE_24;
			break;
		case SPDIF_BIT_MODE_MODERAW:
			ch->sfmt.bitsize = AUDIO_BIT_SIZE_32;
			break;
		default:
			ch->sfmt.bitsize = AUDIO_BIT_SIZE_16;
			break;
		}
	}
	return 0;
}

int am_set_device_format(aud_dev_info* devinfo, am_dev_format_info *format)
{
	if (devinfo->dev_type == AUDIO_I2S_DEVICE) {
		i2s_set_loopback(devinfo->dev_id, format->loopmode);
		i2s_set_master(devinfo->dev_id, format->mastermode);
		i2s_set_bit_format(devinfo->dev_id, format->audiomode);
		i2s_set_left_right_control_polarity(
				devinfo->dev_id,
				format->polarity);

	} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {

	}
	return 0;
}

int am_device_init(aud_dev_info* devinfo, void *dev_fmt, void  *strm_fmt)
{
	am_stream_format_info  *sfmt = (am_stream_format_info*)strm_fmt;
	am_dev_format_info *dfmt = (am_dev_format_info*)dev_fmt;

	if (devinfo->dev_type == AUDIO_I2S_DEVICE) {

		struct tegra_i2s_property i2sprop;
		memset(&i2sprop, 0, sizeof(i2sprop));

		if (sfmt) {
			i2sprop.bit_size = sfmt->bitsize;
			i2sprop.sample_rate = sfmt->samplerate;
		}

		if (dfmt) {
			i2sprop.master_mode = dfmt->mastermode;
			i2sprop.audio_mode = dfmt->audiomode;
			i2sprop.clk_rate = dfmt->clkrate;
			i2sprop.fifo_fmt = dfmt->fifofmt;
		}

		return i2s_init(devinfo->dev_id, &i2sprop);

	} else if (devinfo->dev_type == AUDIO_SPDIF_DEVICE) {

		struct tegra_spdif_property spdifprop;
		memset(&spdifprop, 0, sizeof(spdifprop));

		if (dfmt) {
			spdifprop.clk_rate = dfmt->clkrate;

			return spdif_init(
				devinfo->base,
				devinfo->phy_base,
				devinfo->fifo_mode,
				&spdifprop);
		}
	}

	return 0;
}

int tegra_das_set_connection(enum tegra_das_port_con_id new_con_id)
{
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_set_connection);

int tegra_das_get_connection(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_get_connection);

/* if is_normal is true then power mode is normal else tristated */
int tegra_das_power_mode(bool is_normal)
{
	int err = 0;
	/*
	if (is_normal == true) {
		err = tegra_das_enable_mclk();
	} else {
		err = tegra_das_disable_mclk();
	}*/

	return err;
}
EXPORT_SYMBOL_GPL(tegra_das_power_mode);

int tegra_das_open(void)
{
	int err = 0;

	aud_manager = kzalloc(sizeof(struct audio_manager_context), GFP_KERNEL);
	if (!aud_manager)
		return -ENOMEM;

	aud_manager->mclk = tegra_get_clock_by_name("extern1");

	if (!aud_manager->mclk) {
		pr_err(" err in getting mclk \n");
		err = -ENODEV;
		goto fail_clock;
	}

	aud_manager->pmc_clk  = clk_get_sys("clk_out_1", "extern1");
	if (IS_ERR_OR_NULL(aud_manager->pmc_clk))
	{
		pr_err("%s can't get pmc clock\n", __func__);
		err = -ENODEV;
		aud_manager->pmc_clk = 0;
		goto fail_clock;
	}

	return err;

fail_clock:

	tegra_das_close();
	return err;
}
EXPORT_SYMBOL_GPL(tegra_das_open);

int tegra_das_close(void)
{
	if (aud_manager->mclk)
		clk_put(aud_manager->mclk);

	if (aud_manager->pmc_clk)
		clk_put(aud_manager->pmc_clk);

	kfree(aud_manager);
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_close);

void tegra_das_get_all_regs(struct das_regs_cache* regs)
{

}
EXPORT_SYMBOL_GPL(tegra_das_get_all_regs);

void tegra_das_set_all_regs(struct das_regs_cache* regs)
{

}
EXPORT_SYMBOL_GPL(tegra_das_set_all_regs);

int tegra_das_set_mclk_parent(int parent)
{
	/* FIXME ; set parent based on need */
	struct clk *mclk_source = clk_get_sys(NULL, "pll_a_out0");

	clk_set_parent(aud_manager->mclk, mclk_source);
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_set_mclk_parent);

int tegra_das_enable_mclk(void)
{
	int err = 0;

	if (!aud_manager->mclk_refcnt) {
		if (aud_manager->mclk && aud_manager->pmc_clk) {
			tegra_das_set_mclk_parent(0);
			if (clk_enable(aud_manager->mclk)) {
				err = PTR_ERR(aud_manager->mclk);
				return err;
			}

			if (clk_enable(aud_manager->pmc_clk)) {
				clk_disable(aud_manager->mclk);
				err = PTR_ERR(aud_manager->pmc_clk);
				return err;
			}
		}
	}

	aud_manager->mclk_refcnt++;
	return err;
}
EXPORT_SYMBOL_GPL(tegra_das_enable_mclk);

int tegra_das_disable_mclk(void)
{
	int err = 0;

	if (aud_manager->mclk_refcnt > 0) {
		aud_manager->mclk_refcnt--;
		if (aud_manager->mclk_refcnt == 0) {
			if (aud_manager->mclk)
				clk_disable(aud_manager->mclk);

			if (aud_manager->pmc_clk)
				clk_disable(aud_manager->pmc_clk);
		}
	}
	return err;
}
EXPORT_SYMBOL_GPL(tegra_das_disable_mclk);

int tegra_das_set_mclk_rate(int rate)
{
	/* FIXME: change the clock after disabling it if needed */
	aud_manager->mclk_rate = rate;
	clk_set_rate(aud_manager->mclk, rate);
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_das_set_mclk_rate);

int tegra_das_get_mclk_rate(void)
{
	return clk_get_rate(aud_manager->mclk);
}
EXPORT_SYMBOL_GPL(tegra_das_get_mclk_rate);

MODULE_LICENSE("GPL");
#endif
