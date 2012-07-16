/*
 * arch/arm/mach-tegra/board-smba9701-clocks.c
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
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/pda_power.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/i2c-tegra.h>
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
#include <mach/sdhci.h>
#include <mach/gpio.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
//#include <mach/tegra2_i2s.h>
#include <mach/system.h>
#include <linux/nvmap.h>

#include "board.h"
#include "board-smba9701.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

/* Be careful here: Most clocks have restrictions on parent and on
   divider/multiplier ratios. Check tegra2clocks.c before modifying
   this table ! */
static __initdata struct tegra_clk_init_table smba_clk_init_table[] = {
	/* name			parent				rate	enabled */
	/* always on clocks */
	/* 32khz system clock */
	{ "stat_mon",		"clk_m",		0,	true},		/*enabled by bootloader*/	

	/* Master clock */
	{ "cdev1",		NULL,			0,	true},		/* used as audio CODEC MCLK */
	{ "cdev2",		NULL,			0,	true},		/* probably used as USB clock - perhaps 24mhz ?*/

	{ "clk_m",		NULL,			0,	true},	 	/* must be always on - Frequency will be autodetected */
	{ "pcie_xclk",		"clk_m",		0,	false},		/* pcie controller */
	{ "afi",		"clk_m",		0,	false},		/* pcie controller */
	{ "pex",		"clk_m",		0,	false},		/* pcie controller */
	{ "csus",		"clk_m",		0,	false},		/* tegra_camera */
	{ "isp",		"clk_m",		0,	false},		/* tegra_camera */
	{ "usb3", 		"clk_m", 		0,	false},		/* tegra_ehci.2 */
	{ "usb2",		"clk_m",		0,	false},		/* tegra_ehci.1 */
	{ "usbd",		"clk_m", 		0,	false},		/* fsl-tegra-udc , utmip-pad , tegra_ehci.0 , tegra_otg */
	{ "disp2",  		"clk_m",		0, 	false},		/* tegradc.1 */	
	{ "tvdac",		"clk_m",		0,	false},	
	{ "hdmi",		"clk_m",	 12000000,	false},		/* tegra_dc.0, tegra_dc.1 */
	{ "cve",		"clk_m",		0,	false},	
	{ "tvo",		"clk_m",		0,	false},	
	{ "dvc",		"clk_m",	  3000000,	false},		/* tegra-i2c.3 */
	{ "i2c3",		"clk_m",	 12000000,	false},		/* tegra-i2c.2 */
	{ "i2c2",		"clk_m",	   800000,	false},		/* tegra-i2c.1 */
	{ "i2c1",		"clk_m",	   800000,	false},		/* tegra-i2c.0 */
	{ "mipi",		"clk_m",		0,	false},	
	{ "nor",		"clk_m",		0,	false},	
	{ "owr",		"clk_m",		0,	false},		/* tegra_w1 */
	{ "la",			"clk_m",		0,	false},	
	{ "bsev",		"clk_m",		0,	false},		/* tegra_aes */
	{ "bsea",		"clk_m",		0,	false},		/* tegra_avp */
	{ "vcp",		"clk_m",		0,	false},		/* tegra_avp */	
	{ "vfir",		"clk_m",		0,	false},
	{ "ide",		"clk_m",		0,	false},
	{ "sbc4",		"clk_m",		0,	false}, 	/* tegra_spi_slave.3 */
	{ "sbc3",		"clk_m",		0,	false}, 	/* tegra_spi_slave.2 */
	{ "sbc2",		"clk_m",		0,	false}, 	/* tegra_spi_slave.1 */
	{ "sbc1",		"clk_m",		0,	false}, 	/* tegra_spi_slave.0 */
	{ "twc",		"clk_m",		0,	false},
	{ "xio",		"clk_m",		0,	false},
	{ "spi",		"clk_m",		0,	false},
	{ "pwm",		"clk_m",		0,	true},		/* tegra-pwm.0 tegra-pwm.1 tegra-pwm.2 tegra-pwm.3*/
	{ "kfuse",		"clk_m",		0,	true},		/* kfuse-tegra */ /* always on - no init req */
	{ "timer",		"clk_m",		0,	true},		/* timer */ /* always on - no init req */
	{ "clk_d",		"clk_m",		0,	true},
	{ "pll_e",		"clk_m",		0,	true},		/*unknown*/

        /* pll_x */
	{ "pll_x",		"clk_m",		0,	true},		/* */
	{ "cclk",		"pll_x",		0,	true},
	{ "cpu",		"cclk",			0,	true},

	/* pll_u is a clock source for the USB bus */
	{ "pll_u",		"clk_m",		0,	false},		/* USB ulpi clock */

        /* pll_d */
	{ "pll_d",		"clk_m", 	  5000000,	true},			/* hdmi clock */
	{ "dsi",		"pll_d_out0",	   0,	false},		/* tegra_dc.0, tegra_dc.1 - bug on kernel 2.6.36*/
	{ "pll_d_out0",		"pll_d", 	   250000,	true}, 		/* hdmi clock */

	/* pll_s generates the master clock */
	{ "pll_p",		"clk_m",		0,	true},		/* must be always on */
	{ "host1x",		"pll_p",		0,	false},		/* tegra_grhost */
	{ "uarte",		"pll_p",		0,	false},		/* tegra_uart.4 uart.4 */
	{ "uartd",		"pll_p",	216000000,	false},		/* tegra_uart.3 uart.3 */
	{ "uartc",		"pll_p",	216000000,	false},		/* tegra_uart.2 uart.2 */
	{ "uartb",		"pll_p",	216000000,	false},		/* tegra_uart.1 uart.1 */
	{ "uarta",		"pll_p",		0,	true},		/* tegra_uart.0 uart.0 */
	{ "csite",		"pll_p",		0,	true},		/* csite - coresite */ /* always on */
	{ "sdmmc4",		"pll_p",	 52000000,	false},		/* sdhci-tegra.3 */
	{ "sdmmc3",		"pll_p",	 52000000,	false},		/* sdhci-tegra.2 */
	{ "sdmmc2",		"pll_p",	 52000000, 	false},		/* sdhci-tegra.1 */
	{ "sdmmc1",		"pll_p",	 48000000,	false},		/* sdhci-tegra.0 */
	{ "ndflash",		"pll_p",		0,	false},		/* tegra_nand -> should start disabled, but if we do, then nand stops working */
	{ "pll_p_out4",		"pll_p",		0,	true},		/* must be always on - USB ulpi */
	{ "pll_p_out3",		"pll_p",		0,	true},		/* must be always on - i2c, camera */
	{ "csi",		"pll_p_out3",		0,	true},		/* tegra_camera */
	{ "pll_p_out2",		"pll_p",		0,	true},		/* must be always on - usb perhaps ? unused ?*/
	{ "pll_p_out1",		"pll_p",		0,	true},		/* must be always on - audio clocks ...*/
	{ "pll_c",		"clk_m",		0,	true},		/* always on - graphics and camera clocks */
	{ "disp1",  		"pll_c",		0, 	false},		/* tegradc.0 */
	{ "vde", 		"pll_p",		0,	false},		/* tegra-avp */
	{ "pll_c_out1",		"pll_c",		0,	true},		/* must be always on - system clock */
	{ "sclk",		"pll_p_out2",		0,	true},		/* must be always on */
	{ "avp.sclk",		NULL,			0,	false},		/* must be always on */
	{ "cop",		"sclk",			0,	false},		 /* must be always on */
	{ "hclk",		"sclk",			0,	true},		/* must be always on */
	{ "pclk",		"hclk",			0,	true},		/* must be always on */
	{ "apbdma",		"pclk",			0,	true},		/* tegra-dma */

	{ "pll_m",		NULL,			0,	false},		/* always on - memory clocks */	
	{ "mpe",		"pll_m",		0,	false},		/* tegra_grhost */
	{ "epp",		"pll_m",		0,	false}, 	/* tegra_grhost */	
	{ "vi_sensor",		"pll_m",		0,	false},		/* tegra_camera */
	{ "vi",			"pll_m",		0,	false},		/* tegra_camera */
	{ "emc",		"pll_m",		0,	true},		/* always on */
	{ "pll_m_out1",		"pll_m",		0,	true},		/* always on - unused ?*/

	{ "clk_32k",		NULL,		    32768,	true},		/* always on */
	{ "rtc",		"clk_32k",		0,	true},		/* rtc-tegra : must be always on */
	{ "kbc",		"clk_32k",	    32768,	false},		/* tegra-kbc */
	{ "blink",		"clk_32k",	    32768,	true},		/* used for bluetooth */
	{ "pll_s",		"clk_32k",		0,	false},		/* must be always on */

#ifdef SMBA9701_48KHZ_AUDIO
	{ "i2s1",    "pll_a_out0",    2822400,  false},    /* i2s.0 */
	{ "i2s2",    "pll_a_out0",   12288000,  true},    /* i2s.1 */
	{ "pll_a",		"pll_p_out1",	 73728000,	true},		/* always on - audio clocks */
	{ "pll_a_out0", 	"pll_a",	 18432000,	true},		/* always on - i2s audio */
	{ "audio",		"pll_a_out0",	 0,	true},
	{ "audio_2x",		"audio",	 0,	true},
	{ "spdif_in",		"pll_p",	 36000000,	true},
	{ "spdif_out",		"pll_a_out0",	  6144000,	true},
#else
	{ "i2s1",    "pll_a_out0",    2822400,  false},    /* i2s.0 */
	{ "i2s2",    "pll_a_out0",   11289600,  true},    /* i2s.1 */
	{ "pll_a",		"pll_p_out1",	 56448000,	true},		/* always on - audio clocks */
	{ "pll_a_out0", 	"pll_a",	 11289600,	true},		/* always on - i2s audio */
	{ "audio",		"pll_a_out0",	 0,	true},
	{ "audio_2x",		"audio",	 0,	true},
	{ "spdif_in",		"pll_p",	 36000000,	true},
	{ "spdif_out",		"pll_a_out0",	  5644800,	true},
#endif
	{ NULL,			NULL,			0,		0},
};

void __init smba_clks_init(void)
{
	tegra_clk_init_from_table(smba_clk_init_table);
}

