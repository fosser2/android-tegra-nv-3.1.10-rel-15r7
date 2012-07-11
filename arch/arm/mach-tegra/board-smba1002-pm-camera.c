/*
 * arch/arm/mach-tegra/board-smba1002-pm-camera.c
 *
 * Copyright (C) 2012, Wanlida Group Co;Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <generated/mach-types.h>
#include <mach/gpio.h>
#include <media/s5k6aa.h>
#include "gpio-names.h"
#include "board.h"
#include "board-smba1002.h"

static struct regulator *cam_ldo6 = NULL;
static struct regulator *cam_ldo9 = NULL;

#define S5K6AA_POWER_PIN TEGRA_GPIO_PBB5
#define S5K6AA_RESET_PIN TEGRA_GPIO_PD2
//#define TPS6586X_GPIO_BASE (TEGRA_NR_GPIOS)
//#define AVDD_DSI_CSI_ENB_GPIO (TPS6586X_GPIO_BASE + 1) //gpio2

static int smba_s5k6aa_power_on(void)
{
	gpio_direction_output(S5K6AA_POWER_PIN, 1);
	mdelay(10);
	gpio_direction_output(S5K6AA_RESET_PIN, 1);
	mdelay(5);
	gpio_direction_output(S5K6AA_RESET_PIN, 0);
	mdelay(5);
	gpio_direction_output(S5K6AA_RESET_PIN, 1);
	mdelay(5);
	return 0;
}

static int smba_s5k6aa_power_off(void)
{
	gpio_direction_output(S5K6AA_POWER_PIN, 0);
	gpio_direction_output(S5K6AA_RESET_PIN, 1);
	mdelay(10);
	return 0;
}

static int smba_s5k6aa_reset(void)
{
	gpio_direction_output(S5K6AA_POWER_PIN, 0);
	mdelay(5);
	gpio_direction_output(S5K6AA_POWER_PIN, 1);
	mdelay(5);
	gpio_direction_output(S5K6AA_RESET_PIN, 1);
	mdelay(5);
	gpio_direction_output(S5K6AA_RESET_PIN, 0);
	mdelay(5);
	gpio_direction_output(S5K6AA_RESET_PIN, 1);
	mdelay(5);
	return 0;
}

struct s5k6aa_platform_data smba_s5k6aa_data = {
	.power_on = smba_s5k6aa_power_on,
	.power_off = smba_s5k6aa_power_off,
	.reset = smba_s5k6aa_reset,
};

static struct i2c_board_info smba_i2c3_board_info_camera[] = {
	{
		I2C_BOARD_INFO("s5k6aa",  0x3e),
		.platform_data = &smba_s5k6aa_data,
	},
};
struct tegra_camera_gpios {
	const char *name;
	int gpio;
	int enabled;
};

#define TEGRA_CAMERA_GPIO(_name, _gpio, _enabled) \
	{ \
		.name = _name, \
		.gpio = _gpio, \
		.enabled = _enabled, \
	}

static struct tegra_camera_gpios tegra_camera_gpio_keys[] = {
//	TEGRA_CAMERA_GPIO("en_avdd_csi", AVDD_DSI_CSI_ENB_GPIO, 1),
	TEGRA_CAMERA_GPIO("s5k6aa_power_down", S5K6AA_POWER_PIN, 0),
	TEGRA_CAMERA_GPIO("s5k6aa_resest", S5K6AA_RESET_PIN, 1),
};

static int init_camera_gpio(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(tegra_camera_gpio_keys); i++) {
		tegra_gpio_enable(tegra_camera_gpio_keys[i].gpio);
		ret = gpio_request(tegra_camera_gpio_keys[i].gpio,
				tegra_camera_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d (%s)\n",
				__func__, tegra_camera_gpio_keys[i].gpio,
				tegra_camera_gpio_keys[i].name);
			continue;
		}
		gpio_direction_output(tegra_camera_gpio_keys[i].gpio,
				tegra_camera_gpio_keys[i].enabled);
		gpio_export(tegra_camera_gpio_keys[i].gpio, false);
	}

	return 0;
}

int __init smba_camera_init(void)
{
	int i, ret;
	struct i2c_client *client;
	struct i2c_adapter *adapter;

	pr_info("smba camera init!\n");

	init_camera_gpio();

//	cam_ldo6 = regulator_get(NULL, "vdd_ldo6");
//	if (IS_ERR_OR_NULL(cam_ldo6)) {
//		pr_err("camera : can't get vdd_ldo6\n");
//		return -EFAULT;
//	}
//	cam_ldo9 = regulator_get(NULL, "vdd_ldo9");
//	if (IS_ERR_OR_NULL(cam_ldo9)) {
//		pr_err("camera : can't get vdd_ldo6");
//		return -EFAULT;
//	}

	adapter = i2c_get_adapter(3);
	if (adapter == NULL) {
		pr_err("camera : can't get i2c bus #3 adapter\n");
		return -EFAULT;
	}
	for (i = 0; i < ARRAY_SIZE(smba_i2c3_board_info_camera); i++) {
		client = i2c_new_device(adapter,
				&smba_i2c3_board_info_camera[i]);
		if (client == NULL) {
			pr_err("camera : register camera #%d failed\n", i);
			return -EFAULT;
		}
	}
	i2c_put_adapter(adapter);

	return ret;
}
late_initcall(smba_camera_init);
