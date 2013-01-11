/*
 * arch/arm/mach-tegra/board-smba9701-clocks.c
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

#include <linux/console.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/pda_power.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/i2c-tegra.h>
#include <linux/memblock.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/w1.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/sdhci.h>
#include <mach/gpio.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
//#include <mach/tegra2_i2s.h>
#include <mach/system.h>
#include <linux/nvmap.h>

#include "board.h"
#include "board-smba9701.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

static __initdata struct tegra_clk_init_table smba_clk_init_table[] = {
        /* name			parent		rate		enabled */
	{ "cdev1",		NULL,		0,		true},
        { "blink",		"clk_32k",	32768,		true},
	{ "pll_p_out4", 	"pll_p",  	24000000,	true},
        { "pwm",		"clk_32k",	32768,		false},
        { "i2s1",		"pll_a_out0",	0,		false},
        { "i2s2",		"pll_a_out0",	0,		false},
	{ "audio",		"pll_a_out0",	0,		true},
        { "spdif_out",		"pll_a_out0",	0,		false},
	{ "pll_m",		NULL,		0,		false},         /* always on - memory clocks */
	{ "csus",		"clk_m",	0,		false},         /* tegra_camera */
	{ "isp",		"clk_m",	0,		false},         /* tegra_camera? */
	{ "vi_sensor",		"pll_m",	20000000,	false},         /* tegra_camera */
	{ "vi",			"pll_m",	21000000,	false},         /* tegra_camera */
	{ NULL,			NULL,		0,		0},
};

void __init smba_clks_init(void)
{
	tegra_clk_init_from_table(smba_clk_init_table);
}
