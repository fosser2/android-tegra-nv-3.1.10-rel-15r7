/* arch/arm/mach-tegra/board-harmony-kbc.c
 * Key arrangements on Chicony keyboard and platform dependent info for HARMONY
 *
 * Copyright (C) 2011 NVIDIA, Inc.
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


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/device.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/kbc.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#define HARMONY_ROW_COUNT	16
#define HARMONY_COL_COUNT	8

static int plain_kbd_keycode[] = {
	/*
	Row 0-> Unused, Unused, 'W',     'S',
				'A',     'Z',       Unused, Function,
	Row 1 ->Unused, Unused, Unused,  Unused,
				Unused,  unused,     Unused, WIN_SPECIAL
	Row 2 ->Unused, Unused, Unused,  Unused,
				Unused,  unused,     Alt,    Alt2
	Row 3 ->'5',    '4',    'R',     'E',
				'F',     'D',        'X',    Unused,
	Row 4 ->'7',    '6',    'T',     'H',
				'G',     'V',        'C',    SPACEBAR,
	Row 5 ->'9',    '8',    'U',     'Y',
				'J',     'N',        'B',    '|\',
	Row 6 ->Minus,  '0',    'O',     'I',
				'L',     'K',        '<',    M,
	Row 7 ->unused, '+',    '}]',    '#',
				Unused,  Unused,     Unused, WinSpecial,
	Row 8 ->Unused, Unused, Unused,  Unused,
				SHIFT,   SHIFT,      UnUsed, Unused ,
	Row 9 ->Unused, Unused, Unused,  Unused,
				unused,  Ctrl,       UnUsed, Control,
	Row A ->Unused, Unused, Unused,  Unused,
				unused,  unused,     UnUsed, Unused,
	Row B ->'{[',   'P',    '"',     ':;',
				'/?,      '>',       UnUsed, Unused,
	Row C ->'F10',  'F9',   'BckSpc','3',
				'2',     'Up,        Prntscr,Pause
	Row D ->INS,    DEL,    Unused,  Pgup,
				PgDn,    right,      Down,   Left,
	Row E ->F11,    F12,    F8,      'Q',
				F4,      F3,         '1',    F7,
	Row F ->ESC,    '~',     F5,      TAB,
				F1,      F2,         CAPLOCK,F6,*/
	KEY_RESERVED,	KEY_RESERVED,	KEY_W,		KEY_S,
		KEY_A,	 KEY_Z,	 KEY_RESERVED,	KEY_FN,
	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
		KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_MENU,
	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
		KEY_RESERVED,	KEY_RESERVED,	KEY_LEFTALT,	KEY_RIGHTALT,
	KEY_5,		KEY_4,		KEY_R,		KEY_E,
		KEY_F,	 KEY_D,	 KEY_X,	 KEY_RESERVED,
	KEY_7,	 KEY_6,	 KEY_T,		KEY_H,
		KEY_G,	 KEY_V,	 KEY_C,	 KEY_SPACE,
	KEY_9,	 KEY_8,	 KEY_U,		KEY_Y,
		KEY_J,	 KEY_N,	 KEY_B,	 KEY_BACKSLASH,
	KEY_MINUS,	KEY_0,	 KEY_O,		KEY_I,
		KEY_L,	 KEY_K,	 KEY_COMMA,	KEY_M,
	KEY_RESERVED,	KEY_EQUAL,	KEY_RIGHTBRACE,	KEY_ENTER,
		KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_MENU,
	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
		KEY_LEFTSHIFT,	KEY_RIGHTSHIFT, KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
		KEY_RESERVED,	KEY_LEFTCTRL, KEY_RESERVED,	KEY_RIGHTCTRL,
	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED,
		KEY_RESERVED,	KEY_RESERVED,	KEY_RESERVED, KEY_RESERVED,
	KEY_LEFTBRACE, KEY_P,	 KEY_APOSTROPHE,	KEY_SEMICOLON,
		KEY_SLASH,	KEY_DOT,	KEY_RESERVED,	KEY_RESERVED,
	KEY_F10,	KEY_F9,	 KEY_BACKSPACE,	KEY_3,
		KEY_2,		KEY_UP,	 KEY_PRINT,	KEY_PAUSE,
	KEY_INSERT,	KEY_DELETE,	KEY_RESERVED,	KEY_PAGEUP,
		KEY_PAGEDOWN,	KEY_RIGHT,	KEY_DOWN,	KEY_LEFT,
	KEY_F11,	KEY_F12,	KEY_F8,	 KEY_Q,
		KEY_F4,	 KEY_F3,	 KEY_1,		KEY_F7,
	KEY_ESC,	KEY_GRAVE,	KEY_F5,	 KEY_TAB,
		KEY_F1,	 KEY_F2,	 KEY_CAPSLOCK ,	KEY_F6

};

static int func_kbd_keycode[] = {
	/*
	Row 0-> Unused, Unused, 'W',     'S',
				'A',     'Z',       Unused, Function,
	Row 1 ->WINSPECIAL, Unused, Unused,  Unused,
				Unused,  unused,     Unused, Win_special
	Row 2 ->Unused, Unused, Unused,  Unused,
				Unused,  unused,     Alt,    Alt2
	Row 3 ->'5',    '4',    'R',     'E',
				'F',     'D',        'X',    Unused,
	Row 4 ->'7',    '6',    'T',     'H',
				'G',     'V',        'C',    SPACEBAR,
	Row 5 ->'9',    '8',    'U',     'Y',
				'J',     'N',        'B',    '|\',
	Row 6 ->Minus,  '0',    'O',     'I',
				'L',     'K',        '<',    M,
	Row 7 ->unused, '+',    '}]',    '#',
				Unused,  Unused,     Unused, WinSpecial,
	Row 8 ->Unused, Unused, Unused,  Unused,
				SHIFT,   SHIFT,      UnUsed, Unused ,
	Row 9 ->Unused, Unused, Unused,  Unused,
				unused,  Ctrl,       UnUsed, Control,
	Row A ->Unused, Unused, Unused,  Unused,
				unused,  unused,     UnUsed, Unused,
	Row B ->'{[',   'P',    '"',     ':;',
				'/?,      '>',       UnUsed, Unused,
	Row C ->'F10',  'F9',   'BckSpc','3',
				'2',     'Up,        Prntscr,Pause
	Row D ->INS,    DEL,    Unused,  Pgup,
				PgDn,    right,      Down,   Left,
	Row E ->F11,    F12,    F8,      'Q',
				F4,      F3,         '1',    F7,
	Row F ->ESC,    '~',     F5,      TAB,
				F1,      F2,         CAPLOCK,F6,*/

	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_7,	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_9,	KEY_8,	KEY_4,	KEY_RESERVED,  KEY_1,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_SLASH,	KEY_6,	KEY_5,	KEY_3,
		KEY_2,	KEY_RESERVED,  KEY_0,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_KPASTERISK,  KEY_RESERVED,  KEY_KPMINUS,
		KEY_KPPLUS,  KEY_DOT,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED, KEY_VOLUMEUP,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_HOME,	KEY_END,
		KEY_BRIGHTNESSUP,  KEY_VOLUMEDOWN,  KEY_BRIGHTNESSDOWN,
	KEY_NUMLOCK, KEY_SCROLLLOCK,  KEY_MUTE,  KEY_RESERVED,  KEY_RESERVED,
		KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
	KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED,
		KEY_QUESTION,  KEY_RESERVED,  KEY_RESERVED,  KEY_RESERVED
};

static struct tegra_kbc_wake_key harmony_wake_cfg[] = {
	[0] = {
		.row = 1,
		.col = 7,
	},
	[1] = {
		.row = 15,
		.col = 0,
	},

};

static struct tegra_kbc_platform_data harmony_kbc_platform_data = {
	.debounce_cnt = 20,
	.repeat_cnt = 50 * 32,
	.scan_timeout_cnt = 3000 * 32,
	.plain_keycode = plain_kbd_keycode,
	.fn_keycode = func_kbd_keycode,
	.is_filter_keys = false,
	.is_wake_on_any_key = false,
	.wake_key_cnt = 2,
	.wake_cfg = harmony_wake_cfg,
};

static struct resource harmony_kbc_resources[] = {
	[0] = {
		.start = TEGRA_KBC_BASE,
		.end   = TEGRA_KBC_BASE + TEGRA_KBC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_KBC,
		.end   = INT_KBC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device harmony_kbc_device = {
	.name = "tegra-kbc",
	.id = -1,
	.dev = {
		.platform_data = &harmony_kbc_platform_data,
	},
	.resource = harmony_kbc_resources,
	.num_resources = ARRAY_SIZE(harmony_kbc_resources),
};

int __init harmony_kbc_init(void)
{
	struct tegra_kbc_platform_data *data = &harmony_kbc_platform_data;
	int i;

	pr_info("KBC: harmony_kbc_init\n");

	/* Setup the pin configuration information. */
	for (i = 0; i < KBC_MAX_GPIO; i++) {
		data->pin_cfg[i].num = 0;
		data->pin_cfg[i].pin_type = kbc_pin_unused;
	}
	for (i = 0; i < HARMONY_ROW_COUNT; i++) {
		data->pin_cfg[i].num = i;
		data->pin_cfg[i].pin_type = kbc_pin_row;
	}

	for (i = 0; i < HARMONY_COL_COUNT; i++) {
		data->pin_cfg[i + HARMONY_ROW_COUNT].num = i;
		data->pin_cfg[i + HARMONY_ROW_COUNT].pin_type = kbc_pin_col;
	}
	platform_device_register(&harmony_kbc_device);
	return 0;
}
