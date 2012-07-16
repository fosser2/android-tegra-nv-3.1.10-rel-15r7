/*
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com> 
	 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
	 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/adt7461.h>
#include <linux/power/bq20z75.h>
#include <linux/lis3lv02d.h>

#include "board-smba9701.h"
#include "gpio-names.h"
#include "cpu-tegra.h"

static struct i2c_board_info __initdata smba_i2c_bus0_sensor_info[] = {
	{
		I2C_BOARD_INFO("bq20z75-battery", 0x0B),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA9701_AC_PRESENT_IRQ),
	},
};

static struct lis3lv02d_platform_data smba_lis3lv02d_pdata = {
	.axis_x = LIS3_DEV_Y,
	.axis_y = LIS3_DEV_X,
	.axis_z = LIS3_INV_DEV_Z,
};


static struct i2c_board_info __initdata smba_i2c_bus1_sensor_info[] = {
	{
		I2C_BOARD_INFO("lis3lv02d", 0x1C),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA9701_LIS3LV02D),
		.platform_data = &smba_lis3lv02d_pdata,
	},
	{
		I2C_BOARD_INFO("isl29023", 0x44),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA9701_ISL29023),

	},   
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
		.irq = TEGRA_GPIO_TO_IRQ(SMBA9701_TEMP_ALERT),
		.platform_data = &smba_adt7461_pdata,
	},
};

int __init smba_sensors_register_devices(void)
{
	tegra_gpio_enable(SMBA9701_ISL29023);
	gpio_request(SMBA9701_ISL29023, "isl29023_irq");
	gpio_direction_input(SMBA9701_ISL29023);

	tegra_gpio_enable(SMBA9701_AC_PRESENT_IRQ);
	gpio_request(SMBA9701_AC_PRESENT_IRQ, "ac_present_irq");
	gpio_direction_input(SMBA9701_AC_PRESENT_IRQ);

	tegra_gpio_enable(SMBA9701_LIS3LV02D);
	gpio_request(SMBA9701_LIS3LV02D, "lis33de_irq");
	gpio_direction_input(SMBA9701_LIS3LV02D);

	tegra_gpio_enable(SMBA9701_TEMP_ALERT);
	gpio_request(SMBA9701_TEMP_ALERT, "adt7461_temp_alert_irq");
	gpio_direction_input(SMBA9701_TEMP_ALERT);

	i2c_register_board_info(0, smba_i2c_bus0_sensor_info,
	                        ARRAY_SIZE(smba_i2c_bus0_sensor_info));
	i2c_register_board_info(4, smba_i2c_bus4_sensor_info,
	                        ARRAY_SIZE(smba_i2c_bus4_sensor_info));
	return i2c_register_board_info(2, smba_i2c_bus1_sensor_info,
	                               ARRAY_SIZE(smba_i2c_bus1_sensor_info));
}
