/*
 * arch/arm/mach-tegra/board-smba1007-usb.c
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

/* All configurations related to USB */

/* Misc notes on USB bus on Tegra2 systems (extracted from several conversations
held regarding USB devices not being recognized)

  For additional power saving, the tegra ehci USB driver supports powering
down the phy on bus suspend when it is used, for example, to connect an 
internal device that use an out-of-band remote wakeup mechanism (e.g. a 
gpio).

  With power_down_on_bus_suspend = 1, the board fails to recognize newly
attached devices, i.e. only devices connected during boot are accessible.
But it doesn't cause problems with the devices themselves. The main cause
seems to be that power_down_on_bus_suspend = 1 will stop the USB ehci clock
, so we dont get hotplug events.

  Seems that it is needed to keep the IP clocked even if phy is powered 
down on bus suspend, since otherwise we don't get hotplug events for
hub-less systems.

*/
 
#include <linux/kobject.h>
#include <linux/console.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/i2c-tegra.h>
#include <linux/mfd/tps6586x.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
#include <mach/system.h>

#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/usb/f_accessory.h>
#include <linux/fsl_devices.h>

#include "board.h"
#include "board-smba1007.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

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
                .xcvr_setup = 9,
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
        .op_mode = TEGRA_USB_OPMODE_HOST,
        .u_data.host = {
                .vbus_gpio = SMBA1007_USB0_VBUS,
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
                .xcvr_setup = 9,
                .xcvr_lsfslew = 2,
                .xcvr_lsrslew = 2,
        },
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
        .port_otg = false,
        .has_hostpc = false,
        .phy_intf = TEGRA_USB_PHY_INTF_UTMI,
        .op_mode        = TEGRA_USB_OPMODE_HOST,
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
                .xcvr_setup = 9,
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
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	platform_device_register(&tegra_udc_device);

	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);
};

int __init smba_usb_register_devices(void)
{
	smba_usb_init();
	return 0;
}
