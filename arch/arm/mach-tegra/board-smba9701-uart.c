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

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/w1.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>

#include "board.h"
#include "board-smba9701.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

#include <linux/tegra_uart.h>

static struct tegra_uart_platform_data smba_uart_pdata;

static struct platform_device *smba_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

int __init smba_uart_register_devices(void)
{
//  !!!!!!!!!!!! Need fix clocks
	tegra_uarta_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartb_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartc_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartd_device.dev.platform_data = &smba_uart_pdata;
	tegra_uarte_device.dev.platform_data = &smba_uart_pdata;

	return platform_add_devices(smba_uart_devices, ARRAY_SIZE(smba_uart_devices));
}
