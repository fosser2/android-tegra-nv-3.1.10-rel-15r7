/*
 * arch/arm/mach-tegra/board-smba9701-panel.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/pwm_backlight.h>
#include <linux/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "devices.h"
#include "gpio-names.h"
#include "board.h"
#include "board-smba9701.h"

/*panel power on sequence timing*/
#define smba_pnl_to_lvds_ms	0
#define smba_lvds_to_bl_ms	200

#ifdef CONFIG_TEGRA_DC
static struct regulator *smba_hdmi_reg = NULL;
static struct regulator *smba_hdmi_pll = NULL;
#endif

static int smba_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(SMBA9701_BL_ENB, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(SMBA9701_BL_ENB, 1);
	if (ret < 0)
		gpio_free(SMBA9701_BL_ENB);
	else
		tegra_gpio_enable(SMBA9701_BL_ENB);

	return ret;
};

static void smba_backlight_exit(struct device *dev) {
	gpio_set_value(SMBA9701_BL_ENB, 0);
	gpio_free(SMBA9701_BL_ENB);
	tegra_gpio_disable(SMBA9701_BL_ENB);
}

static int smba_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(SMBA9701_BL_ENB, !!brightness);
	return brightness;
}

static int smba_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data smba_backlight_data = {
	.pwm_id		= SMBA9701_BL_PWM_ID,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= smba_backlight_init,
	.exit		= smba_backlight_exit,
	.notify		= smba_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb   = smba_disp1_check_fb,
};

static struct platform_device smba_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &smba_backlight_data,
	},
};

#ifdef CONFIG_TEGRA_DC
static int smba_panel_enable(void)
{
	gpio_set_value(SMBA9701_EN_VDD_PANEL, 1);
	mdelay(smba_pnl_to_lvds_ms);
	gpio_set_value(SMBA9701_LVDS_SHUTDOWN, 1);
	mdelay(smba_lvds_to_bl_ms);
	return 0;
}

static int smba_panel_disable(void)
{
	gpio_set_value(SMBA9701_LVDS_SHUTDOWN, 0);
	gpio_set_value(SMBA9701_EN_VDD_PANEL, 0);
	return 0;
}

static int smba_hdmi_enable(void)
{
	if (!smba_hdmi_reg) {
		smba_hdmi_reg = regulator_get(NULL, "avdd_hdmi"); /* LD07 */
		if (IS_ERR_OR_NULL(smba_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			smba_hdmi_reg = NULL;
			return PTR_ERR(smba_hdmi_reg);
		}
	}
	regulator_enable(smba_hdmi_reg);

	if (!smba_hdmi_pll) {
		smba_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll"); /* LD08 */
		if (IS_ERR_OR_NULL(smba_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			smba_hdmi_pll = NULL;
			regulator_disable(smba_hdmi_reg);
			smba_hdmi_reg = NULL;
			return PTR_ERR(smba_hdmi_pll);
		}
	}
	regulator_enable(smba_hdmi_pll);
	return 0;
}

static int smba_hdmi_disable(void)
{
	regulator_disable(smba_hdmi_reg);
	regulator_disable(smba_hdmi_pll);
	return 0;
}

static struct resource smba_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource smba_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode smba_panel_modes[] = {
	{
		.pclk = 100030000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 2,
		.h_sync_width = 320,
		.v_sync_width = 10,
		.h_back_porch = 480,
		.v_back_porch = 6,
		.h_active = 1024,
		.v_active = 768,
		.h_front_porch = 260,
		.v_front_porch = 16,
	},
};

static struct tegra_fb_data smba_fb_data = {
	.win		= 0,
	.xres		= 1024,
	.yres		= 768,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_fb_data smba_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1280,
	.yres		= 720,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out smba_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.depth		= 18,
	.dither		= TEGRA_DC_ORDERED_DITHER,

	.height         = 147, /* mm */
	.width          = 196, /* mm */

	.modes	 	= smba_panel_modes,
	.n_modes 	= ARRAY_SIZE(smba_panel_modes),

	.enable		= smba_panel_enable,
	.disable	= smba_panel_disable,
};

static struct tegra_dc_out smba_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= SMBA9701_HDMI_HPD,

	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= smba_hdmi_enable,
	.disable	= smba_hdmi_disable,
};

static struct tegra_dc_platform_data smba_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &smba_disp1_out,
	.fb		= &smba_fb_data,
};

static struct tegra_dc_platform_data smba_disp2_pdata = {
	.flags		= 0,
	.default_out	= &smba_disp2_out,
	.fb		= &smba_hdmi_fb_data,
};

static struct nvhost_device smba_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= smba_disp1_resources,
	.num_resources	= ARRAY_SIZE(smba_disp1_resources),
	.dev = {
		.platform_data = &smba_disp1_pdata,
	},
};

static int smba_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &smba_disp1_device.dev;
}

static struct nvhost_device smba_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= smba_disp2_resources,
	.num_resources	= ARRAY_SIZE(smba_disp2_resources),
	.dev = {
		.platform_data = &smba_disp2_pdata,
	},
};
#else
static int smba_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return 0;
}
#endif

static struct nvmap_platform_carveout smba_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data smba_nvmap_data = {
	.carveouts	= smba_carveouts,
	.nr_carveouts	= ARRAY_SIZE(smba_carveouts),
};

static struct platform_device smba_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &smba_nvmap_data,
	},
};

static struct platform_device *smba_gfx_devices[] __initdata = {
	&smba_nvmap_device,
	&tegra_pwfm0_device,
	&smba_backlight_device,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend smba_panel_early_suspender;

static void smba_panel_early_suspend(struct early_suspend *h)
{
	/* power down LCD, add use a black screen for HDMI */
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
	if (num_registered_fb > 1)
		fb_blank(registered_fb[1], FB_BLANK_NORMAL);
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_save_default_governor();
	cpufreq_set_conservative_governor();
	cpufreq_set_conservative_governor_param("up_threshold",
			SET_CONSERVATIVE_GOVERNOR_UP_THRESHOLD);

	cpufreq_set_conservative_governor_param("down_threshold",
			SET_CONSERVATIVE_GOVERNOR_DOWN_THRESHOLD);

	cpufreq_set_conservative_governor_param("freq_step",
		SET_CONSERVATIVE_GOVERNOR_FREQ_STEP);
#endif
}

static void smba_panel_late_resume(struct early_suspend *h)
{
	unsigned i;
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_restore_default_governor();
#endif
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_UNBLANK);
}
#endif

int __init smba_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;

	gpio_request(SMBA9701_EN_VDD_PANEL, "pnl_pwr_enb");
	gpio_direction_output(SMBA9701_EN_VDD_PANEL, 1);
	tegra_gpio_enable(SMBA9701_EN_VDD_PANEL);

	gpio_request(SMBA9701_BL_VDD, "bl_vdd");
	gpio_direction_output(SMBA9701_BL_VDD, 1);
	tegra_gpio_enable(SMBA9701_BL_VDD);

	gpio_request(SMBA9701_LVDS_SHUTDOWN, "lvds_shdn");
	gpio_direction_output(SMBA9701_LVDS_SHUTDOWN, 1);
	tegra_gpio_enable(SMBA9701_LVDS_SHUTDOWN);

//	tegra_gpio_enable(SMBA9701_HDMI_ENB);
//	gpio_request(SMBA9701_HDMI_ENB, "hdmi_5v_en");
//	gpio_direction_output(SMBA9701_HDMI_ENB, 1);

	tegra_gpio_enable(SMBA9701_HDMI_HPD);
	gpio_request(SMBA9701_HDMI_HPD, "hdmi_hpd");
	gpio_direction_input(SMBA9701_HDMI_HPD);

#ifdef CONFIG_HAS_EARLYSUSPEND
	smba_panel_early_suspender.suspend = smba_panel_early_suspend;
	smba_panel_early_suspender.resume = smba_panel_late_resume;
	smba_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&smba_panel_early_suspender);
#endif

#if defined(CONFIG_TEGRA_NVMAP)
	smba_carveouts[1].base = tegra_carveout_start;
	smba_carveouts[1].size = tegra_carveout_size;
#endif

#ifdef CONFIG_TEGRA_GRHOST
	err = nvhost_device_register(&tegra_grhost_device);
	if (err)
		return err;
#endif

	err = platform_add_devices(smba_gfx_devices,
				   ARRAY_SIZE(smba_gfx_devices));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = nvhost_get_resource_byname(&smba_disp1_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	res = nvhost_get_resource_byname(&smba_disp2_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
#endif

	/* Copy the bootloader fb to the fb. */
	tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
		min(tegra_fb_size, tegra_bootloader_fb_size));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err)
		err = nvhost_device_register(&smba_disp1_device);

	if (!err)
		err = nvhost_device_register(&smba_disp2_device);
#endif

	return err;
}

