/*
 * arch/arm/mach-tegra/board-harmony.h
 *
 * Copyright (C) 2010 Google, Inc.
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

#ifndef _MACH_TEGRA_BOARD_HARMONY_H
#define _MACH_TEGRA_BOARD_HARMONY_H

#define HARMONY_GPIO_TPS6586X(_x_)      (TEGRA_NR_GPIOS + (_x_))
/* fixed voltage regulator enable/mode gpios */
#define TPS_GPIO_EN_1V5                 (HARMONY_GPIO_TPS6586X(0))
#define TPS_GPIO_EN_1V2                 (HARMONY_GPIO_TPS6586X(1))
#define TPS_GPIO_EN_1V05                (HARMONY_GPIO_TPS6586X(2))
#define TPS_GPIO_MODE_1V05              (HARMONY_GPIO_TPS6586X(3))

/* WLAN pwr and reset gpio */
#define TEGRA_GPIO_WLAN_PWR_LOW         TEGRA_GPIO_PK5
#define TEGRA_GPIO_WLAN_RST_LOW         TEGRA_GPIO_PK6

void harmony_pinmux_init(void);
int harmony_panel_init(void);
int harmony_sdhci_init(void);
int harmony_regulator_init(void);
void harmony_power_off_init(void);

#endif
