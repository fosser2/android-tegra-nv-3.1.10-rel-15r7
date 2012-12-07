/*
 * arch/arm/mach-tegra/board-smba1002-camera.c
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

#include <media/tegra_v4l2_camera.h>
#include <media/soc_camera.h>
#include <media/s5k6aa.h>

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

#define S5K6AA_POWER_PIN TEGRA_GPIO_PBB5
#define S5K6AA_RESET_PIN TEGRA_GPIO_PD2

// TODO: clean these up into a common header
#define S5K6AA_MCLK_FREQ 24000000

static int clink_s5k6aa_set_power(struct device *dev, int power_on) {
  return smba_s5k6aa_set_power(power_on);
}

struct s5k6aa_platform_data smba_s5k6aa_data = {
	.mclk_frequency = S5K6AA_MCLK_FREQ,
	.bus_type = V4L2_MBUS_PARALLEL,
	.gpio_stby = { 
		.gpio = S5K6AA_POWER_PIN, 
		.level = 0, // active-low
	},
	.gpio_reset = { 
		.gpio = S5K6AA_RESET_PIN,
		.level = 0, // active-low
	},
	.nlanes = 1,
	.horiz_flip = false,
	.vert_flip = false,
};

static struct i2c_board_info smba_i2c3_board_info_camera = {
  I2C_BOARD_INFO("S5K6AA",  0x3c),
  // platform data will get overwritten here with soc_camera_device
};

static struct soc_camera_link clink_s5k6aa = {
  .board_info = &smba_i2c3_board_info_camera,
  .i2c_adapter_id = 3,
  .power = &clink_s5k6aa_set_power,
  .priv = &smba_s5k6aa_data,
  // TODO: move s5k6aa regulators here
};

static struct platform_device smba_tegra_s5k6aa_device = {
  .name   = "soc-camera-pdrv",
  .id     = 0,
  .dev    = {
    .platform_data = &clink_s5k6aa,
  },
};

static struct platform_device tegra_camera_power_device = {
  // note the underscore
  .name   = "tegra_camera",
  .id     = 0,
};

static struct resource smba_camera_resources[] = {
	{
		.name	= "regs",
		.start	= TEGRA_VI_BASE,
		.end	= TEGRA_VI_BASE + TEGRA_VI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

/* In theory we might want to use this callback to reference the 
   tegra_camera driver from the soc_camera host driver instead of
   the i2c client driver */
static int smba_enable_camera(struct nvhost_device *ndev)
{
	// struct soc_camera_host *ici = to_soc_camera_host(&ndev->dev);

	dev_dbg(&ndev->dev, "%s\n", __func__);

	return 0;
}

static void smba_disable_camera(struct nvhost_device *ndev)
{
	dev_dbg(&ndev->dev, "%s\n", __func__);
}

static struct tegra_camera_platform_data smba_camera_pdata = {
  .enable_camera = &smba_enable_camera,
  .disable_camera = &smba_disable_camera,
  .flip_h = 0,
  .flip_v = 0,
};

int __init smba_camera_register_devices(void)
{
  int ret;

  tegra_camera_device.dev.platform_data = &smba_camera_pdata;

  ret = platform_device_register(&tegra_camera_power_device);
  if(ret)
    return ret;

  ret = platform_device_register(&smba_tegra_s5k6aa_device);
  if(ret)
    return ret;

  ret = nvhost_device_register(&tegra_camera_device);
  if(ret)
    return ret;

  return 0;
}
