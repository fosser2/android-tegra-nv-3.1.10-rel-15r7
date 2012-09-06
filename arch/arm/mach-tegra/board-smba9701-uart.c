/*
 * arch/arm/mach-tegra/board-smba9701-uart.c
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/memblock.h>
#include <linux/clk.h>
#include <linux/tegra_uart.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/w1.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/iomap.h>
#include <mach/clk.h>

#include "board.h"
#include "board-smba9701.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"
#include "pm.h"

static struct platform_device *smba_uart_devices[] __initdata = {
//	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "clk_m"},
};

static struct tegra_uart_platform_data smba_uart_pdata;
static unsigned long debug_uart_port_clk_rate;
static void __init uart_debug_init(void)
{
        unsigned long rate;
        struct clk *c;
        
        /* UARTD is the debug port. */
        pr_info("Selecting UARTA as the debug console\n");
        smba_uart_devices[0] = &debug_uarta_device;
        debug_uart_port_base = ((struct plat_serial8250_port *)(
                        debug_uarta_device.dev.platform_data))->mapbase;
        debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
        
        /* Clock enable for the debug channel */
        if (!IS_ERR_OR_NULL(debug_uart_clk)) {
                rate = ((struct plat_serial8250_port *)(
                        debug_uarta_device.dev.platform_data))->uartclk;
                pr_info("The debug console clock name is %s\n",
                                                debug_uart_clk->name);
                c = tegra_get_clock_by_name("pll_p");
                if (IS_ERR_OR_NULL(c))
                        pr_err("Not getting the parent clock pll_p\n");
                else
                        clk_set_parent(debug_uart_clk, c);
                
                clk_enable(debug_uart_clk);
                clk_set_rate(debug_uart_clk, rate);
        } else {
                pr_err("Not getting the clock %s for debug console\n",
                                        debug_uart_clk->name);
        }
}

int __init smba_uart_register_devices(void)
{
	struct clk *c;
	int i;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	smba_uart_pdata.parent_clk_list = uart_parent_clk;
	smba_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uartb_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartc_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartd_device.dev.platform_data = &smba_uart_pdata;
	tegra_uarte_device.dev.platform_data = &smba_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	return platform_add_devices(smba_uart_devices,
				 ARRAY_SIZE(smba_uart_devices));
}
