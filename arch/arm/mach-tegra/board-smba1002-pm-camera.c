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

#define DEBUG 1

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <generated/mach-types.h>
#include <mach/gpio.h>
#include "gpio-names.h"
#include "board.h"
#include "board-smba1002.h"
#include "clock.h"
#include <media/tegra_camera.h>

//static struct regulator *cam_ldo6 = NULL;
//static struct regulator *cam_ldo9 = NULL;

//#define TPS6586X_GPIO_BASE (TEGRA_NR_GPIOS)
//#define AVDD_DSI_CSI_ENB_GPIO (TPS6586X_GPIO_BASE + 1) //gpio2

#define S5K6AA_MCLK_FREQ 24000000
#define S5K6AA_PCLK_FREQ 24000000

void *tegra_camera_get_dev(void);
int tegra_camera_enable(void *d);
void tegra_camera_disable(void *d);
int tegra_camera_clk_set_info(void *d, struct tegra_camera_clk_info *info);

static struct tegra_camera_clk_info pclk_info = {
  .id = TEGRA_CAMERA_MODULE_VI,
  .clk_id = TEGRA_CAMERA_VI_CLK,
  .rate = S5K6AA_PCLK_FREQ,
  .flag = 0, //TEGRA_CAMERA_ENABLE_PD2VI_CLK,
};

static struct tegra_camera_clk_info mclk_info = {
  .id = TEGRA_CAMERA_MODULE_VI,
  .clk_id = TEGRA_CAMERA_VI_SENSOR_CLK,
  .rate = S5K6AA_MCLK_FREQ,
  .flag = 0,
};

static int smba_s5k6aa_power_on(void)
{
	void *dev = tegra_camera_get_dev();

	if(!dev) {
	  pr_err("%s: could not get pm device!\n", __func__);
	  return 0;
	}

	tegra_camera_enable(dev);
	tegra_camera_clk_set_info(dev, &mclk_info);
	tegra_camera_clk_set_info(dev, &pclk_info);

	// camera MCLK (vi_sensor clk)
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CSUS, TEGRA_TRI_NORMAL);
	// camera PCLK (vi clk, pixel clk for data, exported from sensor to t2) is an input

	return 0;
}

static int smba_s5k6aa_power_off(void)
{
	void *dev = tegra_camera_get_dev();

	if(!dev) {
	  pr_err("%s: could not get pm device!\n", __func__);
	  return 0;
	}

	// camera MCLK (vi_sensor clk)
	// camera PCLK (vi clk, pixel clk for data) is always an input
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CSUS, TEGRA_TRI_TRISTATE);
	tegra_camera_disable(dev);
	return 0;
}

int smba_s5k6aa_set_power(int enable)
{
  if(enable)
    return smba_s5k6aa_power_on();
  else
    return smba_s5k6aa_power_off();
}
EXPORT_SYMBOL(smba_s5k6aa_set_power);

static int smba_s5k6aa_reset(void)
{
	pr_info("s5k6aa reset\n");

	return 0;
}

enum s5k6aa_gpio_id {
	STBY,
	RST,
	GPIO_NUM,
};
