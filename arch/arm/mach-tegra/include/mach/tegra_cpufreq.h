/*
 * arch/arm/mach-tegra/include/mach/fb.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
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

#ifndef __MACH_TEGRA_CPUFREQ_H
#define __MACH_TEGRA_CPUFREQ_H

#if defined CONFIG_HAS_EARLYSUSPEND && defined CONFIG_CPU_FREQ
#define SET_CONSERVATIVE_GOVERNOR_UP_THRESHOLD 95
#define SET_CONSERVATIVE_GOVERNOR_DOWN_THRESHOLD 50

void cpufreq_save_default_governor(void);
void cpufreq_restore_default_governor(void);
void cpufreq_set_conservative_governor(void);
void cpufreq_set_conservative_governor_param(int up_th, int down_th);
#endif

#endif
