/*
 * arch/arm/mach-tegra/board-p1852-sdhci.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Copyright (C) 2011 NVIDIA Corporation
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

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mmc/host.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/sdhci.h>

#include "gpio-names.h"
#include "board.h"
#include "board-p1852.h"
#include "devices.h"

#define P1852_SD1_CD TEGRA_GPIO_PV2
#define P1852_SD3_CD TEGRA_GPIO_PV3

#define P1852_SD1_WP TEGRA_GPIO_PD3
#define P1852_SD3_WP TEGRA_GPIO_PD4


static struct tegra_sdhci_platform_data tegra_sdhci_platform_data1 = {
	.clk_id = NULL,
	.force_hs = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 6,
	.is_voltage_switch_supported = true,
	.vdd_rail_name = NULL,
	.slot_rail_name = NULL,
	.vdd_max_uv = -1,
	.vdd_min_uv = -1,
	.max_clk = 48000000,
	.is_8bit_supported = false,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data2 = {
	.clk_id = NULL,
	.force_hs = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 6,
	.is_voltage_switch_supported = false,
	.vdd_rail_name = NULL,
	.slot_rail_name = NULL,
	.vdd_max_uv = -1,
	.vdd_min_uv = -1,
	.max_clk = 48000000,
	.is_8bit_supported = true,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data3 = {
	.clk_id = NULL,
	.force_hs = 1,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 6,
	.is_voltage_switch_supported = true,
	.vdd_rail_name = NULL,
	.slot_rail_name = NULL,
	.vdd_max_uv = -1,
	.vdd_min_uv = -1,
	.max_clk = 48000000,
	.is_8bit_supported = false,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data4 = {
	.clk_id = NULL,
	.force_hs = 0,
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 6,
	.is_voltage_switch_supported = false,
	.vdd_rail_name = NULL,
	.slot_rail_name = NULL,
	.vdd_max_uv = -1,
	.vdd_min_uv = -1,
	.max_clk = 48000000,
	.is_8bit_supported = true,
};

static int p1852_sd1_cd_gpio_init(void)
{
	unsigned int rc = 0;

	rc = gpio_request(P1852_SD1_CD, "card_detect");
	if (rc) {
		pr_err("Card detect gpio request failed:%d\n", rc);
		return rc;
	}

	tegra_gpio_enable(P1852_SD1_CD);

	rc = gpio_direction_input(P1852_SD1_CD);
	if (rc) {
		pr_err("Unable to configure direction for card detect gpio:%d\n", rc);
		return rc;
	}

	return 0;
}

static int p1852_sd1_wp_gpio_init(void)
{
	unsigned int rc = 0;

	rc = gpio_request(P1852_SD1_WP, "write_protect");
	if (rc) {
		pr_err("Write protect gpio request failed:%d\n", rc);
		return rc;
	}

	tegra_gpio_enable(P1852_SD1_WP);

	rc = gpio_direction_input(P1852_SD1_WP);
	if (rc) {
		pr_err("Unable to configure direction for write protect gpio:%d\n", rc);
		return rc;
	}

	return 0;
}

static int p1852_sd3_wp_gpio_init(void)
{
	unsigned int rc = 0;

	rc = gpio_request(P1852_SD3_WP, "write_protect");
	if (rc) {
		pr_err("Write protect gpio request failed:%d\n", rc);
		return rc;
	}

	tegra_gpio_enable(P1852_SD3_WP);

	rc = gpio_direction_input(P1852_SD3_WP);
	if (rc) {
		pr_err("Unable to configure direction for write protect gpio:%d\n", rc);
		return rc;
	}

	return 0;
}

int __init p1852_sdhci_init(void)
{
	unsigned int rc = 0;
	rc = p1852_sd1_wp_gpio_init();
	if (!rc) {
		tegra_sdhci_platform_data1.wp_gpio = P1852_SD1_WP;
		tegra_sdhci_platform_data1.wp_gpio_polarity = 1;
	}

	rc = p1852_sd3_wp_gpio_init();
	if (!rc) {
		tegra_sdhci_platform_data3.wp_gpio = P1852_SD3_WP;
		tegra_sdhci_platform_data3.wp_gpio_polarity = 1;
	}

	/* Fix ME: The gpios have to enabled for hot plug support */
	rc = p1852_sd1_cd_gpio_init();
	if (!rc) {
		tegra_sdhci_platform_data1.cd_gpio = P1852_SD1_CD;
		tegra_sdhci_platform_data1.cd_gpio_polarity = 0;
	}

	tegra_sdhci_device1.dev.platform_data = &tegra_sdhci_platform_data1;
	tegra_sdhci_device2.dev.platform_data = &tegra_sdhci_platform_data2;
	tegra_sdhci_device3.dev.platform_data = &tegra_sdhci_platform_data3;
	tegra_sdhci_device4.dev.platform_data = &tegra_sdhci_platform_data4;

	platform_device_register(&tegra_sdhci_device1);
	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device4);

	return 0;
}
