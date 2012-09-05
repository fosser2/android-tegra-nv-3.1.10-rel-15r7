/*
 * arch/arm/mach-tegra/board-smba1002-bt.c
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/memblock.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/iomap.h>

#include "board.h"
#include "board-smba1002.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

static struct resource smba_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = SMBA1002_BT_RESET,
		.end    = SMBA1002_BT_RESET,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device smba_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(smba_bcm4329_rfkill_resources),
	.resource       = smba_bcm4329_rfkill_resources,
};

void __init smba_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", smba_bcm4329_rfkill_device.name, \
				"blink", NULL);
	
	platform_device_register(&smba_bcm4329_rfkill_device);
	return;
}

static struct resource smba_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = SMBA1002_BT_IRQ,
			.end    = SMBA1002_BT_IRQ,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "host_wake",
			.start  = TEGRA_GPIO_TO_IRQ(SMBA1002_BT_IRQ),
			.end    = TEGRA_GPIO_TO_IRQ(SMBA1002_BT_IRQ),
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device smba_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(smba_bluesleep_resources),
	.resource       = smba_bluesleep_resources,
};

void __init smba_setup_bluesleep(void)
{
	platform_device_register(&smba_bluesleep_device);
	tegra_gpio_enable(SMBA1002_BT_IRQ);
	tegra_gpio_enable(SMBA1002_BT_RESET);
	return;
}
