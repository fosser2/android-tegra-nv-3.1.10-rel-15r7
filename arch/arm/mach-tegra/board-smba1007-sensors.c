/*
 * arch/arm/mach-tegra/board-smba1007-sensors.c
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/adt7461.h>

#include "board-smba1007.h"
#include "gpio-names.h"
#include "cpu-tegra.h"

static void smba_isl29023_init(void)
{
	gpio_request(SMBA1007_ISL29023, "isl29023");
	gpio_direction_input(SMBA1007_ISL29023);
}

static void smba_lis3lv02d_init(void)
{
	gpio_request(SMBA1007_LIS3LV02D, "lis3lv02d");
	gpio_direction_input(SMBA1007_LIS3LV02D);
}

static void smba_so340010_kbd_init(void)
{
	gpio_request(SMBA1007_KEYBOARD, "so340010_kbd");
	gpio_direction_input(SMBA1007_KEYBOARD);
}

static void smba_adt7461_init(void)
{
	gpio_request(SMBA1007_TEMP_ALERT, "adt7461");
	gpio_direction_input(SMBA1007_TEMP_ALERT);
}

static struct i2c_board_info __initdata smba_i2c_bus0_sensor_info[] = {
	{
		I2C_BOARD_INFO("smba-battery", 0x0B),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA1007_AC_PRESENT),
	},
	{
		I2C_BOARD_INFO("so340010_kbd", 0x2c),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA1007_KEYBOARD),
	},	
	{
		I2C_BOARD_INFO("lis3lv02d", 0x1C),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA1007_LIS3LV02D),
	},
	{
		I2C_BOARD_INFO("isl29023", 0x44),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA1007_ISL29023),

	},   
};

static struct i2c_board_info __initdata smba_i2c_bus3_sensor_info[] = {
	
};

static struct adt7461_platform_data smba_adt7461_pdata = {
	.supported_hwrev = true,
	.ext_range = false,
	.therm2 = true,
	.conv_rate = 0x05,
	.offset = 0,
	.hysteresis = 0,
	.shutdown_ext_limit = 115,
	.shutdown_local_limit = 120,
	.throttling_ext_limit = 90,
	.alarm_fn = tegra_throttling_enable,
};


static struct i2c_board_info __initdata smba_i2c_bus4_sensor_info[] = {
	{
		I2C_BOARD_INFO("adt7461", 0x4C),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA1007_TEMP_ALERT),
		.platform_data = &smba_adt7461_pdata,
	},
};

int __init smba_sensors_register_devices(void)
{
	smba_isl29023_init();
	smba_lis3lv02d_init();
	smba_so340010_kbd_init();
	smba_adt7461_init();

	i2c_register_board_info(0, smba_i2c_bus0_sensor_info,
		ARRAY_SIZE(smba_i2c_bus0_sensor_info));
		
	i2c_register_board_info(4, smba_i2c_bus4_sensor_info,
		ARRAY_SIZE(smba_i2c_bus4_sensor_info));
			
	i2c_register_board_info(3, smba_i2c_bus3_sensor_info,
		ARRAY_SIZE(smba_i2c_bus3_sensor_info));
								   
	return 0;
}
