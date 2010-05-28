/*
 * arch/arm/mach-tegra/board-nvodm.c
 *
 * Board registration for ODM-kit generic Tegra boards
 *
 * Copyright (c) 2009, NVIDIA Corporation.
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
#include <linux/platform_device.h>
#include <mach/kbc.h>
#include <linux/tegra_devices.h>

#include "nvodm_query_discovery.h"
#include "nvodm_kbc.h"
#include "nvodm_query_kbc.h"
#include "nvodm_kbc_keymapping.h"

#ifdef CONFIG_KEYBOARD_GPIO
#include "nvodm_query_gpio.h"
#include <linux/gpio_keys.h>
#include <linux/input.h>
#endif

#ifdef CONFIG_KEYBOARD_TEGRA
struct tegra_kbc_plat *tegra_kbc_odm_to_plat(void)
{
	struct tegra_kbc_plat *pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	const NvOdmPeripheralConnectivity *conn;
	NvOdmPeripheralSearch srch_attr = NvOdmPeripheralSearch_IoModule;
	const struct NvOdmKeyVirtTableDetail **vkeys;
	NvU32 srch_val = NvOdmIoModule_Kbd;
	NvU32 temp;
	NvU64 guid;
	NvU32 i, j, k;
	NvU32 cols=0;
	NvU32 rows=0;
	NvU32 *wake_row;
	NvU32 *wake_col;
	NvU32 wake_num;
	NvU32 vnum;

	if (!pdata) return NULL;
	pdata->keymap = kzalloc(sizeof(*pdata->keymap)*KBC_MAX_KEY, GFP_KERNEL);
	if (!pdata->keymap) {
		kfree(pdata->keymap);
		return NULL;
	}
	if (NvOdmKbcIsSelectKeysWkUpEnabled(&wake_row, &wake_col, &wake_num)) {
	 	BUG_ON(!wake_num || wake_num>=KBC_MAX_KEY);
		pdata->wake_cfg = kzalloc(sizeof(*pdata->wake_cfg)*wake_num,
			GFP_KERNEL);
		if (pdata->wake_cfg) {
			pdata->wake_cnt = (int)wake_num;
			for (i=0; i<wake_num; i++) {
				pdata->wake_cfg[i].row=wake_row[i];
				pdata->wake_cfg[i].col=wake_col[i];
			}
		} else
			pr_err("disabling wakeup key filtering due to "
				"out-of-memory error\n");
	}

	NvOdmKbcGetParameter(NvOdmKbcParameter_DebounceTime, 1, &temp);

	/* debounce time is reported from ODM in terms of clock ticks. */
	pdata->debounce_cnt = temp;

	/* repeat cycle is reported from ODM in milliseconds,
	 * but needs to be specified in 32KHz ticks */
	NvOdmKbcGetParameter(NvOdmKbcParameter_RepeatCycleTime, 1, &temp);
	pdata->repeat_cnt = temp * 32;

	temp = NvOdmPeripheralEnumerate(&srch_attr, &srch_val, 1, &guid, 1);
	if (!temp) {
		kfree(pdata);
		return NULL;
	}
	conn = NvOdmPeripheralGetGuid(guid);
	if (!conn) {
		kfree(pdata);
		return NULL;
	}

	for (i=0; i<conn->NumAddress; i++) {
		NvU32 addr = conn->AddressList[i].Address;

		if (conn->AddressList[i].Interface!=NvOdmIoModule_Kbd) continue;

		if (conn->AddressList[i].Instance) {
			pdata->pin_cfg[addr].num = cols++;
		 	pdata->pin_cfg[addr].is_col = true;
		} else {
			pdata->pin_cfg[addr].num = rows++;
			pdata->pin_cfg[addr].is_row = true;
		}
	}

	for (i=0; i<KBC_MAX_KEY; i++)
		pdata->keymap[i] = -1;

	vnum = NvOdmKbcKeyMappingGetVirtualKeyMappingList(&vkeys);

	for (i=0; i<rows; i++) {
		for (j=0; j<cols; j++) {
			NvU32 sc = NvOdmKbcGetKeyCode(i, j, rows, cols);
			for (k=0; k<vnum; k++) {
				if (sc >= vkeys[k]->StartScanCode &&
				    sc <= vkeys[k]->EndScanCode) {
					sc -= vkeys[k]->StartScanCode;
					sc = vkeys[k]->pVirtualKeyTable[sc];
					if (!sc) continue;
					pdata->keymap[kbc_indexof(i,j)]=sc;
				}

                        }
		}
	}

	return pdata;
}
#endif

#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_platform_data tegra_button_data;
static struct platform_device tegra_button_device = {
	.name   = "gpio-keys",
	.id     = 3,
	.dev    = {
		.platform_data  = &tegra_button_data,
	}
};
static char *gpio_key_names = "gpio_keys";
struct platform_device *get_gpio_key_platform_data(void)
{
	struct gpio_keys_button *tegra_buttons = NULL;
	int ngpiokeys = 0;
	const NvOdmGpioPinInfo *gpio_key_info;
	int i;
	NvOdmGpioPinKeyInfo *gpio_pin_info = NULL;

	gpio_key_info = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_keypadMisc, 0, &ngpiokeys);

	if (!ngpiokeys) {
		pr_info("No gpio is configured as buttons\n");
		goto end;
	}
	tegra_buttons = kzalloc(ngpiokeys * sizeof(struct gpio_keys_button), GFP_KERNEL);
	if (!tegra_buttons) {
		pr_err("Memory allocation failed for tegra_buttons\n");
		return NULL;
	}

	for (i = 0; i < ngpiokeys; ++i) {
		tegra_buttons[i].gpio =
			(int)(gpio_key_info[i].Port*8 +	gpio_key_info[i].Pin);

		gpio_pin_info = gpio_key_info[i].GpioPinSpecificData;
		tegra_buttons[i].code = (int)gpio_pin_info->Code;
		tegra_buttons[i].desc = gpio_key_names;

		if (gpio_key_info[i].activeState == NvOdmGpioPinActiveState_Low)
			tegra_buttons[i].active_low = 1;
		else
			tegra_buttons[i].active_low = 0;

		tegra_buttons[i].type = EV_KEY;
		tegra_buttons[i].wakeup = (gpio_pin_info->Wakeup)? 1: 0;
		tegra_buttons[i].debounce_interval =
				gpio_pin_info->DebounceTimeMs;
	}

end:
	tegra_button_data.buttons = tegra_buttons;
	tegra_button_data.nbuttons = ngpiokeys;
	return &tegra_button_device;
}
#endif
