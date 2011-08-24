/*
 * arch/arm/mach-tegra/include/mach/dam.h
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_DAM_H
#define __MACH_TEGRA_DAM_H

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

/* dam apis */

void dam_enable(int ifc, int on, int chtype);
void dam_enable_clip_counter(int ifc, int on);
void dam_set_samplerate(int ifc, int chtype, int samplerate);

void dam_save_ctrl_registers(int ifc);
void dam_restore_ctrl_registers(int ifc);
int dam_suspend(int ifc);
int dam_resume(int ifc);

int dam_set_clock_rate(int rate);
int dam_set_clock_parent(int ifc, int parent);
void dam_disable_clock(int ifc);
int dam_enable_clock(int ifc);
int dam_set_acif(int ifc, int chtype, struct audio_cif *cifInfo);
int dam_get_controller(int chtype);
int dam_free_controller(int ifc, int chtype);
int dam_set_gain(int ifc, int chtype, int gain);
int dam_get_dma_requestor(int ifc, int chtype, int fifo_mode);
int dam_free_dma_requestor(int ifc, int chtype, int fifo_mode);

int dam_open(void);
int dam_close(void);

#endif