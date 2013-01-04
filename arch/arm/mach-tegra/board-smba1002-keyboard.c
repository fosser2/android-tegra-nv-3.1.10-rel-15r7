/* OK */
/*
 * arch/arm/mach-tegra/board-smba1002-keyboard.c
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

#include <linux/platform_device.h>
#include <linux/input.h>

#include <linux/gpio_keys.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include <linux/gpio.h>
#include <asm/mach-types.h>

#include "board-smba1002.h"
#include "gpio-names.h"
#include "wakeups-t2.h"
#include "fuse.h"

static struct gpio_keys_button smba_keys[] = {
	[0] = {
		.gpio = SMBA1002_KEY_VOLUMEUP,
		.active_low = true,
		.debounce_interval = 10,
		.wakeup = false,		
		.code = KEY_VOLUMEUP,
		.type = EV_KEY,		
		.desc = "volume up",
	},
	[1] = {
		.gpio = SMBA1002_KEY_VOLUMEDOWN,
		.active_low = true,
		.debounce_interval = 10,
		.wakeup = false,		
		.code = KEY_VOLUMEDOWN,
		.type = EV_KEY,		
		.desc = "volume down",
	},
	[2] = {
		.gpio = SMBA1002_KEY_POWER,
		.active_low = true,
		.debounce_interval = 10,
		.wakeup = true,		
		.code = KEY_POWER,
		.type = EV_KEY,		
		.desc = "power",
	},
};

#define PMC_WAKE_STATUS 0x14

static int smba_wakeup_key(void)
{
	unsigned long status =
		readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

	return (status & (1 << SMBA1002_KEY_POWER)) ?
		KEY_POWER : KEY_RESERVED;	
}

static struct gpio_keys_platform_data smba_keys_platform_data = {
	.buttons	= smba_keys,
	.nbuttons	= ARRAY_SIZE(smba_keys),
	.wakeup_key	= smba_wakeup_key,
};

static struct platform_device smba_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &smba_keys_platform_data,
	},
};


static struct platform_device *smba_pmu_devices[] __initdata = {
	&smba_keys_device,
};

/* Register all keyboard devices */
int __init smba_keys_init(void)
{
	return platform_add_devices(smba_pmu_devices, ARRAY_SIZE(smba_pmu_devices));
}