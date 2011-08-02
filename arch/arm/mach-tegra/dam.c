/*
 * arch/arm/mach-tegra/dam.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/slab.h>

#include "clock.h"
#include <asm/io.h>
#include <mach/iomap.h>
#include <mach/audio.h>
#include <mach/audio_switch.h>

#define NR_DAM_IFC	3

#define check_dam_ifc(n, ...) if ((n) > NR_DAM_IFC) {			\
	pr_err("%s: invalid dam interface %d\n", __func__, (n));	\
	return __VA_ARGS__;						\
}

/* Offsets from TEGRA_DAM1_BASE, TEGRA_DAM2_BASE and TEGRA_DAM3_BASE */
#define DAM_CTRL_0			0
#define DAM_CLIP_0			4
#define DAM_CLIP_THRESHOLD_0		8
#define DAM_AUDIOCIF_OUT_CTRL_0		0x0C
#define DAM_CH0_CTRL_0			0x10
#define DAM_CH0_CONV_0			0x14
#define DAM_AUDIOCIF_CH0_CTRL_0		0x1C
#define DAM_CH1_CTRL_0			0x20
#define DAM_CH1_CONV_0			0x24
#define DAM_AUDIOCIF_CH1_CTRL_0		0x2C

#define DAM_CTRL_REGINDEX 	(DAM_AUDIOCIF_CH1_CTRL_0 >> 2)
#define DAM_CTRL_RSVD_6		6
#define DAM_CTRL_RSVD_10	10

#define DAM_NUM_INPUT_CHANNELS		2
#define DAM_FS_8KHZ  			0
#define DAM_FS_16KHZ 			1
#define DAM_FS_44KHZ 			2
#define DAM_FS_48KHZ 			3

/* DAM_CTRL_0 */

#define DAM_CTRL_0_SOFT_RESET_ENABLE	(1 << 31)

#define DAM_CTRL_0_FSOUT_SHIFT	4
#define DAM_CTRL_0_FSOUT_MASK	(0xf << DAM_CTRL_0_FSOUT_SHIFT)
#define DAM_CTRL_0_FSOUT_FS8	(DAM_FS_8KHZ << DAM_CTRL_0_FSOUT_SHIFT)
#define DAM_CTRL_0_FSOUT_FS16	(DAM_FS_16KHZ << DAM_CTRL_0_FSOUT_SHIFT)
#define DAM_CTRL_0_FSOUT_FS44	(DAM_FS_44KHZ << DAM_CTRL_0_FSOUT_SHIFT)
#define DAM_CTRL_0_FSOUT_FS48	(DAM_FS_48KHZ << DAM_CTRL_0_FSOUT_SHIFT)
#define DAM_CTRL_0_CG_EN	(1 << 1)
#define DAM_CTRL_0_DAM_EN	(1 << 0)

/* DAM_CLIP_0 */

#define DAM_CLIP_0_COUNTER_ENABLE	(1 << 31)
#define DAM_CLIP_0_COUNT_MASK		0x7fffffff

/* DAM_CLIP_THRESHOLD_0 */
#define DAM_CLIP_THRESHOLD_0_VALUE_SHIFT	8
#define DAM_CLIP_THRESHOLD_0_VALUE_MASK		\
		(0x7fffff << DAM_CLIP_THRESHOLD_0_VALUE_SHIFT)
#define DAM_CLIP_THRESHOLD_0_VALUE		(1 << 31)
#define DAM_CLIP_THRESHOLD_0_COUNT_SHIFT	0


#define STEP_RESET		1
#define DAM_DATA_SYNC		1
#define DAM_DATA_SYNC_SHIFT	4
#define DAM_GAIN		1
#define DAM_GAIN_SHIFT		0

/* DAM_CH0_CTRL_0 */
#define DAM_CH0_CTRL_0_FSIN_SHIFT	8
#define DAM_CH0_CTRL_0_STEP_SHIFT	16
#define DAM_CH0_CTRL_0_STEP_MASK	(0xffff << 16)
#define DAM_CH0_CTRL_0_STEP_RESET	(STEP_RESET << 16)
#define DAM_CH0_CTRL_0_FSIN_MASK	(0xf << 8)
#define DAM_CH0_CTRL_0_FSIN_FS8		(DAM_FS_8KHZ << 8)
#define DAM_CH0_CTRL_0_FSIN_FS16	(DAM_FS_16KHZ << 8)
#define DAM_CH0_CTRL_0_FSIN_FS44	(DAM_FS_44KHZ << 8)
#define DAM_CH0_CTRL_0_FSIN_FS48	(DAM_FS_48KHZ << 8)
#define DAM_CH0_CTRL_0_DATA_SYNC_MASK	(0xf << DAM_DATA_SYNC_SHIFT)
#define DAM_CH0_CTRL_0_DATA_SYNC	(DAM_DATA_SYNC << DAM_DATA_SYNC_SHIFT)
#define DAM_CH0_CTRL_0_EN		(1 << 0)


/* DAM_CH0_CONV_0 */
#define DAM_CH0_CONV_0_GAIN		(DAM_GAIN << DAM_GAIN_SHIFT)

/* DAM_CH1_CTRL_0 */
#define DAM_CH1_CTRL_0_DATA_SYNC_MASK	(0xf << DAM_DATA_SYNC_SHIFT)
#define DAM_CH1_CTRL_0_DATA_SYNC	(DAM_DATA_SYNC << DAM_DATA_SYNC_SHIFT)
#define DAM_CH1_CTRL_0_EN		(1 << 0)

/* DAM_CH1_CONV_0 */
#define DAM_CH1_CONV_0_GAIN		(DAM_GAIN << DAM_GAIN_SHIFT)

#define DAM_OUT_CHANNEL		0
#define DAM_IN_CHANNEL_0	1
#define DAM_IN_CHANNEL_1	2

#define  ENABLE_DAM_DEBUG_PRINT	0

#if ENABLE_DAM_DEBUG_PRINT
#define DAM_DEBUG_PRINT(fmt, arg...) printk(fmt, ## arg)
#else
#define DAM_DEBUG_PRINT(fmt, arg...) do {} while (0)
#endif

/* FIXME: move this control to audio_manager later if needed */
struct dam_context {
	int		outsamplerate;
	bool		ch_alloc[DAM_NUM_INPUT_CHANNELS];
	bool		ch_inuse[DAM_NUM_INPUT_CHANNELS];
	int		ch_insamplerate[DAM_NUM_INPUT_CHANNELS];
	int		ctrlreg_cache[DAM_CTRL_REGINDEX];
	struct clk	*dam_clk;
	int		clk_refcnt;
	int		dma_ch[dam_ch_maxnum];
	int		in_use;
};

struct dam_context dam_cont_info[NR_DAM_IFC];

static char* damclk_info[NR_DAM_IFC] = {
	"dam0",
	"dam1",
	"dam2"
};

struct dam_src_step_table {
	int insample;
	int outsample;
	int stepreset;
};
/* FIXME : recheck all these values */
struct dam_src_step_table  step_table[] = {
	{ 8000, 44100, 80 },
	{ 8000, 48000, 0 },  /* 0 worked for A01 - 1 is suggested for A02 */
	{ 16000, 44100, 160 },
	{ 16000, 48000, 1 },
	{ 44100, 8000, 441 },
	{ 48000, 8000, 6 },
	{ 44100, 16000, 441 },
	{ 48000, 16000, 0 },
};

struct dam_module_context {
	int refcnt;
};

static struct dam_module_context *dam_info = NULL;

static void *dam_base[NR_DAM_IFC] = {
	IO_ADDRESS(TEGRA_DAM0_BASE),
	IO_ADDRESS(TEGRA_DAM1_BASE),
	IO_ADDRESS(TEGRA_DAM2_BASE),
};

/* Internal calls */
void dam_dump_registers(int ifc);
void dam_ch0_enable(int ifc,int on);
void dam_ch1_enable(int ifc,int on);
void dam_set_input_samplerate(int ifc,int fsin);
void dam_set_output_samplerate(int ifc,int fsout);
void dam_ch0_set_step(int ifc,int step);
void dam_ch0_set_datasync(int ifc,int datasync);
void dam_ch0_set_gain(int ifc,int gain);
void dam_ch1_set_datasync(int ifc,int datasync);
void dam_ch1_set_gain(int ifc,int gain);

static inline void dam_writel(int ifc, u32 val, u32 reg)
{
	__raw_writel(val, dam_base[ifc] + reg);
	DAM_DEBUG_PRINT("dam write 0x%p: %08x\n", dam_base[ifc] + reg, val);
}

static inline u32 dam_readl(int ifc, u32 reg)
{
	u32 val = __raw_readl(dam_base[ifc] + reg);
	DAM_DEBUG_PRINT("dam read offset 0x%x: %08x\n", reg, val);
	return val;
}

void dam_dump_registers(int ifc)
{
	check_dam_ifc(ifc);

	pr_info("%s: \n",__func__);

	dam_readl(ifc, DAM_CTRL_0);
	dam_readl(ifc, DAM_CLIP_0);
	dam_readl(ifc, DAM_CLIP_THRESHOLD_0);
	dam_readl(ifc, DAM_AUDIOCIF_OUT_CTRL_0);
	dam_readl(ifc, DAM_CH0_CTRL_0);
	dam_readl(ifc, DAM_CH0_CONV_0);
	dam_readl(ifc, DAM_AUDIOCIF_CH0_CTRL_0);
	dam_readl(ifc, DAM_CH1_CTRL_0);
	dam_readl(ifc, DAM_CH1_CONV_0);
	dam_readl(ifc, DAM_AUDIOCIF_CH1_CTRL_0);
}

void dam_enable(int ifc, int on, int chtype)
{
	u32 val;
	struct dam_context *ch = &dam_cont_info[ifc];

	check_dam_ifc(ifc);

	if (chtype == dam_ch_in0) {
		dam_ch0_enable(ifc, on);
		ch->ch_inuse[dam_ch_in0] = (on)? true : false;
	} else if (chtype == dam_ch_in1) {
		dam_ch1_enable(ifc, on);
		ch->ch_inuse[dam_ch_in1] = (on)? true : false;
	}

	val = dam_readl(ifc, DAM_CTRL_0);

	val &= ~DAM_CTRL_0_DAM_EN;
	val |= ((ch->ch_inuse[dam_ch_in0] || ch->ch_inuse[dam_ch_in1])) ?
		DAM_CTRL_0_DAM_EN : 0;

	dam_writel(ifc, val, DAM_CTRL_0);
}

void dam_enable_clip_counter(int ifc, int on)
{
	u32 val;

	check_dam_ifc(ifc);

	val = dam_readl(ifc, DAM_CLIP_0);

	val &= ~ DAM_CLIP_0_COUNTER_ENABLE;
	val |= on ?  DAM_CLIP_0_COUNTER_ENABLE : 0;

	dam_writel(ifc, val, DAM_CLIP_0);
}

int dam_set_step_reset(int ifc, int insample, int outsample)
{
	int step_reset = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(step_table); i++) {
		if ((insample == step_table[i].insample) &&
			(outsample == step_table[i].outsample))
			step_reset = step_table[i].stepreset;
	}

	dam_ch0_set_step(ifc, step_reset);
	return 0;
}

int dam_set_gain(int ifc, int chtype, int gain)
{

	switch (chtype) {
	case dam_ch_in0:
		dam_ch0_set_gain(ifc, gain);
		break;
	case dam_ch_in1:
		dam_ch1_set_gain(ifc, gain);
		break;
	default:
		break;
	}
	return 0;
}

void dam_set_samplerate(int ifc, int chtype, int samplerate)
{
	struct dam_context *ch = &dam_cont_info[ifc];

	switch (chtype) {
	case dam_ch_in0:
		dam_set_input_samplerate(ifc, samplerate);
		ch->ch_insamplerate[dam_ch_in0] = samplerate;
		dam_set_step_reset(ifc, samplerate, ch->outsamplerate);
		break;
	case dam_ch_in1:
		ch->ch_insamplerate[dam_ch_in1] = samplerate;
		break;
	case dam_ch_out:
		dam_set_output_samplerate(ifc, samplerate);
		ch->outsamplerate = samplerate;
		break;
	default:
		break;
	}
}

void dam_set_output_samplerate(int ifc,int fsout)
{
	u32 val;

	check_dam_ifc(ifc);

	val = dam_readl(ifc, DAM_CTRL_0);
	val &=~DAM_CTRL_0_FSOUT_MASK;

	switch (fsout){

	case AUDIO_SAMPLERATE_8000:
		val |= DAM_CTRL_0_FSOUT_FS8;
		break;
	case AUDIO_SAMPLERATE_16000:
		val |= DAM_CTRL_0_FSOUT_FS16;
		break;
	case AUDIO_SAMPLERATE_44100:
		val |= DAM_CTRL_0_FSOUT_FS44;
		break;
	case AUDIO_SAMPLERATE_48000:
		val |= DAM_CTRL_0_FSOUT_FS48;
		break;
	default:
		break;
	}

	dam_writel(ifc, val, DAM_CTRL_0);
}

void dam_ch0_enable(int ifc,int on)
{
	u32 val;

	val = dam_readl(ifc, DAM_CH0_CTRL_0);

	val &= ~DAM_CH0_CTRL_0_EN;
	val |= on ? DAM_CH0_CTRL_0_EN : 0;

	dam_writel(ifc, val, DAM_CH0_CTRL_0);
}

void dam_set_input_samplerate(int ifc,int fsin)
{
	u32 val;

	check_dam_ifc(ifc);

	val = dam_readl(ifc, DAM_CH0_CTRL_0);
	val &=~DAM_CH0_CTRL_0_FSIN_MASK;

	switch (fsin) {

	case AUDIO_SAMPLERATE_8000:
		val |= DAM_CH0_CTRL_0_FSIN_FS8;
		break;
	case AUDIO_SAMPLERATE_16000:
		val |= DAM_CH0_CTRL_0_FSIN_FS16;
		break;
	case AUDIO_SAMPLERATE_44100:
		val |= DAM_CH0_CTRL_0_FSIN_FS44;
		break;
	case AUDIO_SAMPLERATE_48000:
		val |= DAM_CH0_CTRL_0_FSIN_FS48;
		break;
	default:
		break;
	}

	dam_writel(ifc, val, DAM_CH0_CTRL_0);
}

void dam_ch0_set_step(int ifc,int step)
{
	u32 val;

	check_dam_ifc(ifc);

	val = dam_readl(ifc, DAM_CH0_CTRL_0);

	val &= ~DAM_CH0_CTRL_0_STEP_MASK;
	val |= step << DAM_CH0_CTRL_0_STEP_SHIFT;

	dam_writel(ifc, val, DAM_CH0_CTRL_0);
}

void dam_ch0_set_datasync(int ifc,int datasync)
{
	u32 val;

	check_dam_ifc(ifc);

	val = dam_readl(ifc, DAM_CH0_CTRL_0);
	val &= ~DAM_CH0_CTRL_0_DATA_SYNC_MASK;

	val |= datasync << DAM_DATA_SYNC_SHIFT;

	dam_writel(ifc, val, DAM_CH0_CTRL_0);
}

void dam_ch0_set_gain(int ifc, int gain)
{
	check_dam_ifc(ifc);
	dam_writel(ifc, gain, DAM_CH0_CONV_0);
}

void dam_ch1_enable(int ifc,int on)
{
	u32 val;

	val = dam_readl(ifc, DAM_CH1_CTRL_0);

	val &= ~DAM_CH1_CTRL_0_EN;
	val |= on ? DAM_CH1_CTRL_0_EN : 0;

	dam_writel(ifc, val, DAM_CH1_CTRL_0);
}

void dam_ch1_set_datasync(int ifc,int datasync)
{
	u32 val;

	check_dam_ifc(ifc);

	val = dam_readl(ifc, DAM_CH1_CTRL_0);
	val &= ~DAM_CH1_CTRL_0_DATA_SYNC_MASK;

	val |= datasync << DAM_DATA_SYNC_SHIFT;

	dam_writel(ifc, val, DAM_CH1_CTRL_0);
}

void dam_ch1_set_gain(int ifc,int gain)
{
	check_dam_ifc(ifc);
	dam_writel(ifc, gain, DAM_CH1_CONV_0);
}

void dam_save_ctrl_registers(int ifc)
{
	int i = 0;
	struct dam_context *ch;
	ch = &dam_cont_info[ifc];

	for (i = 0; i <= DAM_CTRL_REGINDEX; i++) {
		if ((i == DAM_CTRL_RSVD_6) || (i == DAM_CTRL_RSVD_10))
			continue;

		ch->ctrlreg_cache[i] = dam_readl(ifc, (i << 2));
	}
}

void dam_restore_ctrl_registers(int ifc)
{
	int i = 0;
	struct dam_context *ch;
	ch = &dam_cont_info[ifc];

	for (i = 0; i <= DAM_CTRL_REGINDEX; i++) {
		if ((i == DAM_CTRL_RSVD_6) || (i == DAM_CTRL_RSVD_10))
			continue;

		dam_writel(ifc, ch->ctrlreg_cache[i], (i << 2));
	}
}

int dam_suspend(int ifc)
{
	struct dam_context *ch;
	ch = &dam_cont_info[ifc];

	dam_save_ctrl_registers(ifc);

	dam_disable_clock(ifc);

	return 0;
}

int dam_resume(int ifc)
{
	struct dam_context *ch;
	ch = &dam_cont_info[ifc];

	dam_enable_clock(ifc);

	dam_restore_ctrl_registers(ifc);

	return 0;
}

int dam_set_clock_rate(int rate)
{
	/* FIXME: to complete */
	return 0;
}

int dam_set_clock_parent(int ifc, int parent)
{
	/* FIXME ; set parent based on need */
	struct dam_context *ch;
	struct clk *mclk_source = clk_get_sys(NULL, "pll_a_out0");

	ch =  &dam_cont_info[ifc];
	clk_set_parent(ch->dam_clk, mclk_source);
	return 0;
}

void dam_disable_clock(int ifc)
{
	struct dam_context *ch;

	ch =  &dam_cont_info[ifc];

	if (ch->clk_refcnt > 0) {
		ch->clk_refcnt--;
		if ((ch->clk_refcnt == 0)  &&
			(ch->dam_clk)) {
			clk_disable(ch->dam_clk);
		}
	}

	audio_switch_disable_clock();
	DAM_DEBUG_PRINT("%s dev%d clk cnt %d\n", __func__,
					ifc, ch->clk_refcnt);
}

int dam_enable_clock(int ifc)
{
	int err = 0;
	struct dam_context *ch;

	ch =  &dam_cont_info[ifc];

	err = audio_switch_enable_clock();
	if (err)
		return err;

	if (!ch->clk_refcnt) {
		if (clk_enable(ch->dam_clk)) {
			err = PTR_ERR(ch->dam_clk);
			goto fail_dam_clock;
		}
	}

	ch->clk_refcnt++;

	DAM_DEBUG_PRINT("%s dev%d clk cnt %d\n", __func__,
				ifc, ch->clk_refcnt);
	return err;

fail_dam_clock:

	dam_disable_clock(ifc);
	return err;
}

int dam_set_acif(int ifc, int chtype, struct audio_cif *cifInfo)
{
	unsigned int reg_addr = (unsigned int)dam_base[ifc];
	bool found = true;

	switch (chtype) {
	case dam_ch_out:
		reg_addr += DAM_AUDIOCIF_OUT_CTRL_0;
		break;
	case dam_ch_in0:
		/*always Mono channel*/
		cifInfo->client_channels = AUDIO_CHANNEL_1;
		reg_addr += DAM_AUDIOCIF_CH0_CTRL_0;
		break;
	case dam_ch_in1:
		reg_addr += DAM_AUDIOCIF_CH1_CTRL_0;
		break;
	default:
		found = false;
		break;
	}

	if (found == true)
		audio_switch_set_acif(reg_addr, cifInfo);

	return 0;
}

int dam_get_controller(int chtype)
{
	int i = 0, err = 0;
	struct dam_context *ch = NULL;

	if (!dam_info)
		return -ENOENT;

	for (i = 0; i < NR_DAM_IFC; i++) {
		ch =  &dam_cont_info[i];

		if (ch->in_use == false) {

			err = dam_enable_clock(i);
			if (err) {
				pr_err("unable to enable the dam channel\n");
				return -ENOENT;
			}
			ch->in_use = true;
			ch->ch_alloc[chtype] = true;
			return i;
		} else {
			if (ch->ch_alloc[chtype] == false) {
				ch->ch_alloc[chtype] = true;
				return i;
			}
		}
	}
	return -ENOENT;
}

int dam_free_controller(int ifc, int chtype)
{
	struct dam_context *ch = NULL;

	check_dam_ifc(ifc , -EINVAL);

	ch =  &dam_cont_info[ifc];
	DAM_DEBUG_PRINT("%s\n", __func__);

	if (!dam_info)
		return -ENOENT;

	ch->ch_alloc[chtype] = false;

	if (ch->ch_alloc[dam_ch_in0] == false &&
		ch->ch_alloc[dam_ch_in1] == false) {

		dam_disable_clock(ifc);

		if (ch->clk_refcnt == 0)
			ch->in_use = false;
	}

	return 0;
}

int dam_get_dma_requestor(int ifc, int chtype, int fifo_mode)
{
	int dma_index = -1;
	struct dam_context *ch;
	int regIndex  = ifc + ahubrx0_dam0 + chtype;

	if (fifo_mode == AUDIO_RX_MODE)
		regIndex = ifc + ahubtx_dam0;

	dma_index = apbif_get_channel(regIndex, fifo_mode);

	if (dma_index >= 0) {
		ch = &dam_cont_info[ifc];
		ch->dma_ch[chtype] = dma_index-1;
	}

	return dma_index;
}

int dam_free_dma_requestor(int ifc, int chtype, int fifo_mode)
{
	struct dam_context *ch;
	ch = &dam_cont_info[ifc];
	if (ch->dma_ch[chtype] >= 0) {
		audio_apbif_free_channel(ch->dma_ch[chtype], fifo_mode);
		DAM_DEBUG_PRINT("free dam ch %d\n", ch->dma_ch[chtype]);
		ch->dma_ch[chtype] = -ENOENT;
	}
	return 0;
}

int dam_open(void)
{
	int err = 0, i, j;

	DAM_DEBUG_PRINT("%s ++\n", __func__);

	if (!dam_info) {
		struct dam_context *ch;

		dam_info =
		kzalloc(sizeof(struct dam_module_context), GFP_KERNEL);

		if (!dam_info)
			return -ENOMEM;

		memset(dam_cont_info, 0, sizeof(dam_cont_info));

		for (i = 0; i < NR_DAM_IFC; i++) {
			ch = &dam_cont_info[i];
			memset(ch, 0, sizeof(struct dam_context));
			ch->dam_clk = tegra_get_clock_by_name(damclk_info[i]);
			if (!ch->dam_clk) {
				pr_err("unable to get dam%d clock\n", i);
				err = -ENOENT;
				goto fail_dam_open;
			}
			dam_set_clock_parent(i, 0);

			for (j = 0; j < dam_ch_maxnum; j++)
				ch->dma_ch[j] = -1;
		}
	}
	dam_info->refcnt++;

	DAM_DEBUG_PRINT("%s-- refcnt 0x%x\n", __func__ , dam_info->refcnt);

	return 0;

fail_dam_open:

	dam_close();
	return err;
}

int dam_close(void)
{
	struct dam_context *ch;
	int i = 0;

	DAM_DEBUG_PRINT("%s\n", __func__);
	if (dam_info) {
		dam_info->refcnt--;

		if (dam_info->refcnt == 0) {
			for (i = 0; i < NR_DAM_IFC; i++) {
				ch = &dam_cont_info[i];
				dam_disable_clock(i);

				if (ch->dam_clk)
					clk_put(ch->dam_clk);
			}
			kfree(dam_info);
			dam_info = NULL;
		}
	}

	return 0;
}
