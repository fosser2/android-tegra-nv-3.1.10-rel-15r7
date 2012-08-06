/* OK */
/*
 * arch/arm/mach-tegra/board-smba9701-keyboard.c
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

#include <linux/platform_device.h>
#include <linux/input.h>

#include <linux/gpio_keys.h>
//#include <linux/gpio_shortlong_key.h>
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

#include "board-smba9701.h"
#include "gpio-names.h"
#include "wakeups-t2.h"
#include "fuse.h"

#define GPIO_KEY(_id, _gpio, _iswake)           \
        {                                       \
                .code = _id,                    \
                .gpio = TEGRA_GPIO_##_gpio,     \
                .active_low = 1,                \
                .desc = #_id,                   \
                .type = EV_KEY,                 \
                .wakeup = _iswake,              \
                .debounce_interval = 10,        \
        }

static struct gpio_keys_button smba_keys[] = {
        [0] = GPIO_KEY(KEY_HOME, PQ0, 0),
        [1] = GPIO_KEY(KEY_BACK, PQ1, 0),
        [2] = GPIO_KEY(KEY_VOLUMEUP, PQ5, 0),
        [3] = GPIO_KEY(KEY_VOLUMEDOWN, PQ4, 0),
        [4] = GPIO_KEY(KEY_POWER, PV2, 1),
        [5] = GPIO_KEY(KEY_MENU, PQ2, 0),
};

#define PMC_WAKE_STATUS 0x14

static int smba_wakeup_key(void)
{
        unsigned long status =
                readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

        return status & TEGRA_WAKE_GPIO_PV2 ? KEY_POWER : KEY_RESERVED;
}

static struct gpio_keys_platform_data smba_keys_platform_data = {
        .buttons        = smba_keys,
        .nbuttons       = ARRAY_SIZE(smba_keys),
        .wakeup_key     = smba_wakeup_key,
};

static struct platform_device smba_keys_device = {
        .name   = "gpio-keys",
        .id     = 0,
        .dev    = {
                .platform_data  = &smba_keys_platform_data,
        },
};

static void smba_keys_init(void)
{
        int i;

        for (i = 0; i < ARRAY_SIZE(smba_keys); i++)
                tegra_gpio_enable(smba_keys[i].gpio);
}


static struct platform_device *smba_devices[] __initdata = {
	&smba_keys_device,
};

/* Register all keyboard devices */
int __init smba_keyboard_register_devices(void)
{

	platform_add_devices(smba_devices, ARRAY_SIZE(smba_devices));
	smba_keys_init();
}
