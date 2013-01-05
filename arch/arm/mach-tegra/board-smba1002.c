/*
 * arch/arm/mach-tegra/board-smba1002.c
 *
 * Copyright (c) 2010-2011 NVIDIA Corporation.
 * Copyright (c) 2012, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c/at168_ts.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/mfd/tps6586x.h>
#include <linux/memblock.h>
#include <linux/tegra_uart.h>
#include <linux/rfkill-gpio.h>

#include <sound/alc5623.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/tegra_alc5623_pdata.h>
#include <mach/audio.h>
#include <mach/usb_phy.h>
#include <mach/memory.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include "board.h"
#include "clock.h"
#include "board-smba1002.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "wakeups-t2.h"
#include "pm.h"


static struct rfkill_gpio_platform_data smba_bt_rfkill_pdata[] = {
	{
		.name           = "bt_rfkill",
		.shutdown_gpio  = -1,
		.reset_gpio     = SMBA1002_BT_RESET,
		.type           = RFKILL_TYPE_BLUETOOTH,
	},
};

static struct platform_device smba_bt_rfkill_device = {
	.name = "rfkill_gpio",
	.id             = -1,
	.dev = {
		.platform_data  = smba_bt_rfkill_pdata,
	},
};

static void __init smba_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", smba_bt_rfkill_device.name, \
				"blink", NULL);
	return;
}

static struct resource smba_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = SMBA1002_BT_IRQ,
			.end    = SMBA1002_BT_IRQ,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "gpio_ext_wake",
			.start  = SMBA1002_BT_WAKEUP,
			.end    = SMBA1002_BT_WAKEUP,
			.flags  = IORESOURCE_IO,
	},
	[2] = {
		.name = "host_wake",
			.start  = TEGRA_GPIO_TO_IRQ(SMBA1002_BT_IRQ),
			.end    = TEGRA_GPIO_TO_IRQ(SMBA1002_BT_IRQ),
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device smba_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(smba_bluesleep_resources),
	.resource       = smba_bluesleep_resources,
};

static void __init smba_setup_bluesleep(void)
{
	platform_device_register(&smba_bluesleep_device);
	return;
}

static __initdata struct tegra_clk_init_table smba_clk_init_table[] = {
	/* name			parent		 rate		enabled */
	{ "cdev1",		NULL,		     0, 	true },
	{ "blink",	"clk_32k",		 32768,		false},
	{ "pll_p_out4",	"pll_p",  24000000,		true },
	{ "pwm",	"clk_32k",		 32768,		false},
	{ "i2s1",	"pll_a_out0",		 0,		false},
	{ "i2s2",	"pll_a_out0",		 0,		false},
	{ "spdif_out",	"pll_a_out0",	 0,		false},	
	{ NULL,			NULL,		     0,			0},
};

static struct tegra_i2c_platform_data smba_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data smba_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 2,
	.bus_clk_rate	= { 10000, 10000 },
	//FIX ME
	.bus_mux	= { &i2c2_ddc, &i2c2_gen2 },
	.bus_mux_len	= { 1, 1 },
};

static struct tegra_i2c_platform_data smba_i2c3_platform_data = {
	.adapter_nr     = 3,
	.bus_count      = 1,
	.bus_clk_rate   = { 100000, 0 },
};

static struct tegra_i2c_platform_data smba_dvc_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_dvc		= true,
};

static void smba_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &smba_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &smba_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &smba_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &smba_dvc_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static struct platform_device *smba_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

/* Prefer lower clocks if possible */
static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"}, 	//  12000000
	[1] = {.name = "pll_p"},	// 216000000
	[2] = {.name = "pll_c"},	// 600000000
};

static struct tegra_uart_platform_data smba_uart_pdata;
static unsigned long debug_uart_port_clk_rate;
static void __init uart_debug_init(void)
{
	int debug_port_id;

	debug_port_id = get_tegra_uart_debug_port_id();
	if (debug_port_id < 0)
		debug_port_id = 3;

	switch (debug_port_id) {
	case 0:
		/* UARTA is the debug port. */
		pr_info("Selecting UARTA as the debug console\n");
		smba_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		debug_uart_port_clk_rate = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->uartclk; 
			
		break;

	case 1:
		/* UARTB is the debug port. */
		pr_info("Selecting UARTB as the debug console\n");
		smba_uart_devices[1] = &debug_uartb_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartb");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->mapbase;
		debug_uart_port_clk_rate = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->uartclk; 
			
		break;

	case 2:
		/* UARTC is the debug port. */
		pr_info("Selecting UARTC as the debug console\n");
		smba_uart_devices[2] = &debug_uartc_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartc");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartc_device.dev.platform_data))->mapbase;
		debug_uart_port_clk_rate = ((struct plat_serial8250_port *)(
			debug_uartc_device.dev.platform_data))->uartclk; 
			
		break;

	case 3:
		/* UARTD is the debug port. */
		pr_info("Selecting UARTD as the debug console\n");
		smba_uart_devices[3] = &debug_uartd_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uartd");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
		debug_uart_port_clk_rate = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->uartclk; 
			
		break;

	default:
		pr_info("The debug console id %d is invalid, Assuming UARTA",
			debug_port_id);
		smba_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		debug_uart_port_clk_rate = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->uartclk; 
			
		break;
	}
}

static void __init smba_uart_init(void)
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
	
	tegra_uarta_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartb_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartc_device.dev.platform_data = &smba_uart_pdata;
	tegra_uartd_device.dev.platform_data = &smba_uart_pdata;
	tegra_uarte_device.dev.platform_data = &smba_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs()) {
	
		uart_debug_init();
		
		/* Clock enable for the debug channel */
		if (!IS_ERR_OR_NULL(debug_uart_clk)) {
			
			pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
			c = tegra_get_clock_by_name("pll_p");
			if (IS_ERR_OR_NULL(c))
				pr_err("Not getting the parent clock pll_p\n");
			else
				clk_set_parent(debug_uart_clk, c);

			clk_enable(debug_uart_clk);
			clk_set_rate(debug_uart_clk, debug_uart_port_clk_rate);
		} else {
			pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
		}
	}

	platform_add_devices(smba_uart_devices,
				ARRAY_SIZE(smba_uart_devices));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static struct gpio_keys_button smba_keys[] = {
	[0] = {
		.gpio = SMBA1002_KEY_VOLUMEUP,
		.active_low = true,
		.debounce_interval = 10,
		.wakeup = false,		
		.code = KEY_VOLUMEUP,
		.type = EV_KEY,		
		.desc = "volume up",
	},
	[1] = {
		.gpio = SMBA1002_KEY_VOLUMEDOWN,
		.active_low = true,
		.debounce_interval = 10,
		.wakeup = false,		
		.code = KEY_VOLUMEDOWN,
		.type = EV_KEY,		
		.desc = "volume down",
	},
	[2] = {
		.gpio = SMBA1002_KEY_POWER,
		.active_low = true,
		.debounce_interval = 10,
		.wakeup = true,		
		.code = KEY_POWER,
		.type = EV_KEY,		
		.desc = "power",
	},
};

#define PMC_WAKE_STATUS 0x14

static int smba_wakeup_key(void)
{
	unsigned long status =
		readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

	return (status & (1 << SMBA1002_KEY_POWER)) ?
		KEY_POWER : KEY_RESERVED;
}

static struct gpio_keys_platform_data smba_keys_platform_data = {
	.buttons	= smba_keys,
	.nbuttons	= ARRAY_SIZE(smba_keys),
	.wakeup_key	= smba_wakeup_key,
};

static struct platform_device smba_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &smba_keys_platform_data,
	},
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Default music path: I2S1(DAC1)<->Dap1<->HifiCodec
   Bluetooth to codec: I2S2(DAC2)<->Dap4<->Bluetooth
*/
/* For SMBA1002, 
	Codec is ALC5623
	Codec I2C Address = 0x34(includes R/W bit), i2c #0
	Codec MCLK = APxx DAP_MCLK1
*/


static struct tegra_audio_platform_data tegra_spdif_pdata = {
	.dma_on			= true,  /* use dma by default */
	.mask			= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
	.stereo_capture = true,
};

static struct tegra_audio_platform_data tegra_audio_pdata[] = {
	/* For I2S1 - Hifi */
	[0] = {
#ifdef ALC5623_IS_MASTER
		.i2s_master		= false,	/* CODEC is master for audio */
		.dma_on			= true,  	/* use dma by default */
		.i2s_clk_rate 	= 2822400,
		.dap_clk	  	= "cdev1",
		.audio_sync_clk = "audio_2x",
		.mode			= I2S_BIT_FORMAT_I2S,
		.fifo_fmt		= I2S_FIFO_16_LSB,
		.bit_size		= I2S_BIT_SIZE_16,
#else
		.i2s_master		= true,		/* CODEC is slave for audio */
		.dma_on			= true,  	/* use dma by default */
#ifdef SMBA1002_48KHZ_AUDIO						
		.i2s_master_clk = 48000,
		.i2s_clk_rate 	= 12288000,
#else
		.i2s_master_clk = 44100,
		.i2s_clk_rate 	= 11289600,
#endif
		.dap_clk	  	= "cdev1",
		.audio_sync_clk = "audio_2x",
		.mode			= I2S_BIT_FORMAT_I2S,
		.fifo_fmt		= I2S_FIFO_PACKED,
		.bit_size		= I2S_BIT_SIZE_16,
		.i2s_bus_width	= 32,
#endif
		.mask			= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
		.stereo_capture = true,
	},
	/* For I2S2 - Bluetooth */
	[1] = {
		.i2s_master		= false,	/* bluetooth is master always */
		.dma_on			= true,  /* use dma by default */
		.i2s_master_clk = 8000,
		.dsp_master_clk = 8000,
		.i2s_clk_rate	= 2000000,
		.dap_clk		= "cdev1",
		.audio_sync_clk = "audio_2x",
		.mode			= I2S_BIT_FORMAT_DSP,
		.fifo_fmt		= I2S_FIFO_16_LSB,
		.bit_size		= I2S_BIT_SIZE_16,
		.i2s_bus_width 	= 32,
		.dsp_bus_width 	= 16,
		.mask			= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
		.stereo_capture = true,
	}
}; 

static struct alc5623_platform_data smba_alc5623_pdata = {
	.add_ctrl	= 0xD300,
	.jack_det_ctrl	= 0,
	.avdd_mv		= 3300,	/* Analog vdd in millivolts */

	.mic1bias_mv		= 2475,	/* MIC1 bias voltage */
	.mic1boost_db    	= 20,  /* MIC1 gain boost */
	.mic2boost_db   	= 20,  /* MIC2 gain boost */

	.default_is_mic2 	= false,	/* SMBA1002 uses MIC1 as the default capture source */

};

static struct i2c_board_info __initdata smba_i2c_bus0_board_info[] = {
	{
		I2C_BOARD_INFO("alc5623", 0x1a),
		.platform_data = &smba_alc5623_pdata,
	},
};


static struct tegra_alc5623_platform_data smba_audio_pdata = {
        .gpio_spkr_en           = -2,
        .gpio_hp_det            = SMBA1002_HP_DETECT,
	.gpio_int_mic_en 	= SMBA1002_INT_MIC_EN,
	.hifi_codec_datafmt = SND_SOC_DAIFMT_I2S,	/* HiFi codec data format */
#ifdef ALC5623_IS_MASTER
	.hifi_codec_master  = true,					/* If Hifi codec is master */
#else
	.hifi_codec_master  = false,				/* If Hifi codec is master */
#endif
	.bt_codec_datafmt   = SND_SOC_DAIFMT_DSP_A,	/* Bluetooth codec data format */
	.bt_codec_master    = true,					/* If bt codec is master */

};

static struct platform_device tegra_generic_codec = {
	.name = "tegra-generic-codec",
	.id   = -1,
};

static struct platform_device smba_audio_device = {
	.name = "tegra-snd-alc5623",
	.id   = 0,
        .dev    = {
                .platform_data  = &smba_audio_pdata,
        },

};

static struct platform_device *smba_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_gart_device,
	&tegra_aes_device,
	&smba_keys_device,
	&tegra_spi_device1,
	&tegra_spi_device2,
	&tegra_spi_device3,
	//&tegra_spi_device4,
	&tegra_wdt_device,
	&tegra_avp_device,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_das_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&smba_bt_rfkill_device,
	&tegra_pcm_device,
	&tegra_generic_codec,
	&smba_audio_device, /* this must come last, as we need the DAS to be initialized to access the codec registers ! */
};


int  __init smba_audio_register_devices(void)
{
	int ret;

        pr_info("%s++ \n", __func__);

	/* Patch in the platform data */
	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata[0];
	tegra_i2s_device2.dev.platform_data = &tegra_audio_pdata[1];
	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;

        ret = i2c_register_board_info(0, smba_i2c_bus0_board_info,
                ARRAY_SIZE(smba_i2c_bus0_board_info));
        if (ret)
                return ret;

        return platform_add_devices(smba_devices, ARRAY_SIZE(smba_devices));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct at168_i2c_ts_platform_data at168_pdata = {
	.gpio_reset = SMBA1002_TS_RESET,
	.gpio_power = SMBA1002_TS_POWER,
};

static struct i2c_board_info __initdata smba_i2c_bus0_touch_info_at168[] = {
	{
		I2C_BOARD_INFO("at168_touch", 0x5c),
		.irq = TEGRA_GPIO_TO_IRQ(SMBA1002_TS_IRQ),
		.platform_data = &at168_pdata,
	},
};


static int __init smba_touch_init_at168(void)
{
	tegra_gpio_enable(SMBA1002_TS_IRQ);
	gpio_request(SMBA1002_TS_IRQ, "at168_touch");
	gpio_direction_input(SMBA1002_TS_IRQ);
	
	i2c_register_board_info(0, smba_i2c_bus0_touch_info_at168, 1);

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* ADD ME LATERRRRR
static int __init smba_gps_init(void)
{
	struct clk *clk32 = clk_get_sys(NULL, "blink");
	if (!IS_ERR(clk32)) {
		clk_set_rate(clk32,clk32->parent->rate);
		clk_enable(clk32);
	}

	return 0;
}
*/

static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = 0,
		.vbus_gpio = -1,
		.charging_supported = false,
		.remote_wakeup_supported = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = SMBA1002_USB0_VBUS,
		.vbus_reg = NULL,
		.hot_plug = true,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 9,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.vbus_reg = NULL,
		.hot_plug = true,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 9,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};

static void smba_usb_init(void)
{
	/* OTG should be the first to be registered */
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	platform_device_register(&tegra_udc_device);

	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);
}

static void __init tegra_smba_init(void)
{
	tegra_clk_init_from_table(smba_clk_init_table);
	smba_pinmux_init();
	smba_i2c_init();
	smba_uart_init();
	tegra_ram_console_debug_init();
	smba_nand_register_devices();
	smba_sdhci_init();
	smba_charge_init();
	smba_regulator_init();
	smba_charger_init();
	smba_touch_init_at168();
	smba_usb_init();
	//smba_gps_init();
	smba_panel_init();
	smba_audio_register_devices();
	smba_sensors_register_devices();
	smba_bt_rfkill();
	smba_setup_bluesleep();
	tegra_release_bootloader_fb();
}

void __init tegra_smba_reserve(void)
{
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	/*tegra_reserve(SZ_256M, SZ_8M + SZ_1M, SZ_16M);
	tegra_ram_console_debug_reserve(SZ_1M); WHAT DOES THIS DO?*/ 
}

static void __init tegra_smba_fixup(struct machine_desc *desc,
	struct tag *tags, char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = SMBA1002_MEM_BANKS;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size  = SMBA1002_MEM_SIZE - SMBA1002_GPU_MEM_SIZE;
}	

MACHINE_START(HARMONY, "harmony")
.boot_params = 0x00000100,
.fixup	= tegra_smba_fixup,
.map_io = tegra_map_common_io,
.reserve = tegra_smba_reserve,
.init_early	= tegra_init_early,
.init_irq = tegra_init_irq,
.timer = &tegra_timer,
.init_machine = tegra_smba_init,
MACHINE_END

