 /*
 * arch/arm/mach-tegra/board-smba1002.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2012 NVIDIA Corporation.
 * Copyright (C) 2013 Andrew Foss <fosser2@teamdrh.com>
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
 
 
#ifndef _MACH_TEGRA_BOARD_SMBA1002_H
#define _MACH_TEGRA_BOARD_SMBA1002_H

// SMBA1002 GPIOs

// Input
/* GPS and Magnetic sensor share the same enabling IO line */
#define SMBA1002_KEY_VOLUMEUP 		TEGRA_GPIO_PV4 	/* 0=pressed */
#define SMBA1002_KEY_VOLUMEDOWN 	TEGRA_GPIO_PD4 	/* 0=pressed */
#define SMBA1002_KEY_POWER 			TEGRA_GPIO_PV2 	/* 0=pressed */
#define SMBA1002_KEY_HOMEPAGE   	TEGRA_GPIO_PS2  /* 0=pressed */
#define SMBA1002_KEYBOARD			TEGRA_GPIO_PV6
#define SMBA1002_HDMI_HPD			TEGRA_GPIO_PN7  /* 1=HDMI plug detected */
#define SMBA1002_TS_IRQ				TEGRA_GPIO_PJ7
#define SMBA1002_HP_DETECT			TEGRA_GPIO_PW2 	/* HeadPhone detect for audio codec: 1=Headphone plugged */
#define SMBA1002_TEMP_ALERT			TEGRA_GPIO_PN6
#define SMBA1002_LIS3LV02D 			TEGRA_GPIO_PJ0  /*Accelerometer */
#define SMBA1002_BT_IRQ 			TEGRA_GPIO_PU6
#define SMBA1002_ISL29023  			TEGRA_GPIO_PV5  /* Light Sensor */
#define SMBA1002_AC_PRESENT    		TEGRA_GPIO_PH2
#define SMBA1002_DOCK           	TEGRA_GPIO_PH0


// Outputs
#define SMBA1002_GPSMAG_DISABLE  	TEGRA_GPIO_PV3 	/* 0=disabled */
#define SMBA1002_3G_DISABLE			TEGRA_GPIO_PJ2  /* 0=disabled */
#define SMBA1002_CAMERA_POWER 		TEGRA_GPIO_PBB5 /* 1=powered on */
#define SMBA1002_NAND_WPN			TEGRA_GPIO_PC7	/* NAND flash write protect: 0=writeprotected */
#define SMBA1002_BL_ENB				TEGRA_GPIO_PD3  /* LCD Backlight enable */
#define SMBA1002_LVDS_SHUTDOWN		TEGRA_GPIO_PB2
#define SMBA1002_EN_VDD_PANEL		TEGRA_GPIO_PC6 
#define SMBA1002_BL_VDD				TEGRA_GPIO_PW0
#define SMBA1002_BL_PWM				TEGRA_GPIO_PU3  /* PWM */
#define	SMBA1002_ENABLE_VDD_VID		TEGRA_GPIO_PD1	/* 1=enabled.  Powers HDMI. Wait 500uS to let it stabilize before returning */
#define SMBA1002_SDHC_CD			TEGRA_GPIO_PI5  /* External SD Card Detect */
#define SMBA1002_SDHC_WP			-1				/*1=Write Protected */
#define SMBA1002_WL_BT_POWER 		TEGRA_GPIO_PK5
#define SMBA1002_WLAN_RESET 		TEGRA_GPIO_PK6
#define SMBA1002_BT_RESET 			TEGRA_GPIO_PU0  /* 0=reset asserted */
#define SMBA1002_BT_WAKEUP			TEGRA_GPIO_PU5
#define SMBA1002_LOW_BATT			TEGRA_GPIO_PW3  /*(0=low battery)*/
#define SMBA1002_TS_RESET			TEGRA_GPIO_PH1
#define SMBA1002_TS_POWER			TEGRA_GPIO_PK2
#define SMBA1002_SDINT_POWER       	TEGRA_GPIO_PI6
#define SMBA1002_SDHC_POWER			TEGRA_GPIO_PD0  /* External SD Power On */
#define SMBA1002_CAMERA_RESET   	TEGRA_GPIO_PD2  /* 1=powered on */
#define SMBA1002_CHARGING_DISABLE  	TEGRA_GPIO_PK7
#define SMBA1002_INT_MIC_EN    		TEGRA_GPIO_PX0  /* 0 = disabled */
#define SMBA1002_USB0_VBUS			TEGRA_GPIO_PB1	/* 1= VBUS usb0 */

#define SMBA1002_WAKE_KEY_POWER 	TEGRA_WAKE_GPIO_PV2
#define SMBA1002_WAKE_KEY_RESUME	TEGRA_WAKE_GPIO_PV2

// SMBA1002 memory is 1xSZ_512M
#define SMBA1002_MEM_SIZE 			SZ_512M			/* Total memory */
#define SMBA1002_MEM_BANKS			1

#define SMBA1002_GPU_MEM_SIZE  			(SZ_1M*128)			/* Memory reserved for GPU */
#define SMBA1002_FB1_MEM_SIZE 	SZ_8M			/* Memory reserved for Framebuffer 1: LCD <-- Is this actually the size of the framebuffer?*/
#define SMBA1002_FB2_MEM_SIZE 	SZ_16M			/* Memory reserved for Framebuffer 2: HDMI out <-- Is this actually the size of the framebuffer?*/
#define SMBA1002_TOTAL_GPU_MEM_SIZE (SMBA1002_GPU_MEM_SIZE+SMBA1002_FB1_MEM_SIZE+SMBA1002_FB2_MEM_SIZE)

/* TPS6586X gpios */
#define TPS6586X_GPIO_BASE	TEGRA_NR_GPIOS
#define PMU_IRQ_BASE				(TEGRA_NR_IRQS)
#define TPS6586X_GPIO(_x_)	(TPS6586X_GPIO_BASE + (_x_))
#define TPS6586X_NR_GPIOS	4
#define AVDD_DSI_CSI_ENB_GPIO	TPS6586X_GPIO(1) /* gpio2 */
#define TPS6586X_GPIO_END	TPS6586X_GPIO(TPS6586X_NR_GPIOS - 1)

extern int  smba_pinmux_init(void);
extern void smba_clks_init(void);
extern int smba_usb_register_devices(void);
extern int smba_audio_register_devices(void);
extern int smba_panel_init(void);
extern int smba_uart_register_devices(void);
extern int smba_spi_register_devices(void);
extern int smba_i2c_register_devices(void);
extern int smba_keys_init(void);
extern int smba_touch_register_devices(void);
extern int smba_sdhci_init(void);
extern int smba_sensors_register_devices(void);
extern void smba_setup_bluesleep(void);
extern int smba_nand_register_devices(void);
extern int smba_camera_register_devices(void);
extern int smba_charge_init(void);
extern int smba_regulator_init(void);
extern int smba_s5k6aa_set_power(int enable);

#endif


