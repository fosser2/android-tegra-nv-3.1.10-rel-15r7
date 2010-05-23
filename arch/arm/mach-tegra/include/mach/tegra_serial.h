/*
 * arch/arm/mach-tegra/include/mach/tegra_serial.h
 *
 * Copyright (c) 2009-2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_SERIAL_H
#define __MACH_TEGRA_SERIAL_H

#include <linux/serial_core.h>

/* Switch off the clock of the uart controller. */
void tegra_uart_request_clock_off(struct uart_port *uport);

/* Switch on the clock of the uart controller */
void tegra_uart_request_clock_on(struct uart_port *uport);

/* Set the modem control signals state of uart controller. */
void tegra_uart_set_mctrl(struct uart_port *uport, unsigned int mctrl);

/* Return the status of the transmit fifo whether empty or not.
 * Return 0 if tx fifo is not empty.
 * Return TIOCSER_TEMT if tx fifo is empty */
int tegra_uart_is_tx_empty(struct uart_port *uport);

#endif
