/*
 * arch/arm/mach-tegra/board-smba9701.h
 *
 * Copyright (C) 2011 Eduardo Jos� Tagle <ejtagle@tutopia.com>
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

#ifndef _MACH_TEGRA_BOARD_SMBA9701_H
#define _MACH_TEGRA_BOARD_SMBA9701_H

#define TPS6586X_INT_BASE		TEGRA_NR_IRQS

#define SMBA9701_3G_DISABLE		TEGRA_GPIO_PB0 /* 0 = disabled */

#define SMBA9701_KEY_VOLUMEUP 		TEGRA_GPIO_PQ5 	/* 0=pressed */
#define SMBA9701_KEY_VOLUMEDOWN 	TEGRA_GPIO_PQ4 	/* 0=pressed */
#define SMBA9701_KEY_POWER 		TEGRA_GPIO_PV2 	/* 0=pressed */
#define SMBA9701_KEY_BACK		TEGRA_GPIO_PQ1	/* 0=pressed */
#define SMBA9701_KEY_HOME		TEGRA_GPIO_PQ0	/* 0=pressed */
#define SMBA9701_KEY_MENU		TEGRA_GPIO_PQ2	/* 0=pressed */

#define SMBA9701_WAKE_KEY_POWER		TEGRA_WAKE_GPIO_PV2
#define SMBA9701_WAKE_KEY_RESUME	TEGRA_WAKE_GPIO_PV2

#define SMBA9701_ISL29023		TEGRA_GPIO_PV5
#define SMBA9701_LIS3LV02D		TEGRA_GPIO_PJ0

#define SMBA9701_CAMERA_POWER 		TEGRA_GPIO_PBB5	/* 1=powered on */
#define SMBA9701_CAMERA_RESET 		TEGRA_GPIO_PD2	/* 1=powered on */

#define SMBA9701_DOCK			TEGRA_GPIO_PH0

#define SMBA9701_NAND_WPN		TEGRA_GPIO_PC7	/* NAND flash write protect: 0=writeprotected */

#define SMBA9701_BL_ENB			TEGRA_GPIO_PD3 //LCD Backlight enable
#define SMBA9701_LVDS_SHUTDOWN		TEGRA_GPIO_PB2
#define SMBA9701_EN_VDD_PANEL		TEGRA_GPIO_PC6
#define SMBA9701_BL_VDD			TEGRA_GPIO_PW0
#define SMBA9701_BL_PWM			TEGRA_GPIO_PU3 /* PWM */
#define SMBA9701_HDMI_HPD		TEGRA_GPIO_PN7 /* 1=HDMI plug detected */

#define SMBA9701_BL_PWM_ID		0		/* PWM0 controls backlight */

#define SMBA9701_FB_PAGES		2		/* At least, 2 video pages */
#define SMBA9701_FB_HDMI_PAGES		2		/* At least, 2 video pages for HDMI */

// SMBA9701 memory is 2xSZ_512M
#define SMBA9701_MEM_SIZE 		SZ_512M		/* Total memory */
#define SMBA9701_MEM_BANKS		2

#define SMBA9701_GPU_MEM_SIZE 		SZ_128M		/* Memory reserved for GPU */

#define SMBA9701_FB1_MEM_SIZE 		SZ_8M		/* Memory reserved for Framebuffer 1: LCD */
#define SMBA9701_FB2_MEM_SIZE 		SZ_16M		/* Memory reserved for Framebuffer 2: HDMI out */

#define DYNAMIC_GPU_MEM			1		/* use dynamic memory for GPU */

#define SMBA9701_48KHZ_AUDIO   /* <- define this if you want 48khz audio sampling rate instead of 44100Hz */
#define SMBA9701_INT_MIC_EN		TEGRA_GPIO_PX0 /* 0 = disabled */

// TPS6586x GPIOs as registered 
#define PMU_GPIO_BASE			(TEGRA_NR_GPIOS)
#define PMU_GPIO0 			(PMU_GPIO_BASE)
#define PMU_GPIO1 			(PMU_GPIO_BASE + 1)
#define PMU_GPIO2 			(PMU_GPIO_BASE + 2)
#define PMU_GPIO3 			(PMU_GPIO_BASE + 3)

#define ALC5623_GPIO_BASE		(TEGRA_NR_GPIOS + 16)
#define ALC5623_GP0			(ALC5623_GPIO_BASE)

#define PMU_IRQ_BASE			(TEGRA_NR_IRQS)
#define PMU_IRQ_RTC_ALM1 		(TPS6586X_INT_BASE + TPS6586X_INT_RTC_ALM1)

#define	SMBA9701_ENABLE_VDD_VID		TEGRA_GPIO_PD1	/* 1=enabled.  Powers HDMI. Wait 500uS to let it stabilize before returning */

// TODO: Find whether there are any definitions for these?
#define SMBA9701_SDIO0_CD		TEGRA_GPIO_PI5
#define SMBA9701_SDIO0_POWER		TEGRA_GPIO_PD0	/* SDIO0 and SDIO2 power */

#define SMBA9701_SDHC_CD		TEGRA_GPIO_PI5
#define SMBA9701_SDHC_WP		-1	/*1=Write Protected */
#define SMBA9701_SDHC_POWER		TEGRA_GPIO_PD0

#define SMBA9701_TS_IRQ			TEGRA_GPIO_PJ7
#define SMBA9701_TS_RESET		TEGRA_GPIO_PH1
#define SMBA9701_TS_POWER		TEGRA_GPIO_PN5

//#define SMBA9701_FB_NONROTATE TEGRA_GPIO_PH1 /*1 = screen rotation locked */

#define SMBA9701_WLAN_POWER 		TEGRA_GPIO_PK5
#define SMBA9701_WLAN_RESET 		TEGRA_GPIO_PK6

#define SMBA9701_BT_RESET 		TEGRA_GPIO_PU0	/* 0= reset asserted */
#define SMBA9701_BT_WAKEUP 		TEGRA_GPIO_PU5
#define SMBA9701_BT_IRQ 		TEGRA_GPIO_PU6


#define SMBA9701_LOW_BATT		TEGRA_GPIO_PW3	/*(0=low battery)*/
#define SMBA9701_AC_PRESENT_IRQ		TEGRA_GPIO_PH2

#define SET_USBD_RST 22
#define CLR_USBD_RST 22
#define CLK_RST_CONTROLLER_RST_DEV_L_SET_0 0x300
#define CLK_RST_CONTROLLER_RST_DEV_L_CLR_0 0x304
#define SMBA9701_USB0_VBUS		TEGRA_GPIO_PB1	/* 1= VBUS usb0 */
//#define SMBA9701_USB1_RESET		TEGRA_GPIO_PV1	/* 0= reset */

#define SMBA9701_HP_DETECT		TEGRA_GPIO_PW2 	/* HeadPhone detect for audio codec: 1=Hedphone plugged */

//#define SMBA9701_NVEC_REQ		TEGRA_GPIO_PD0	/* Set to 0 to send a command to the NVidia Embedded controller */
//#define SMBA9701_NVEC_I2C_ADDR		0x8a 		/* I2C address of Tegra, when acting as I2C slave */


#define SMBA9701_TEMP_ALERT	TEGRA_GPIO_PN6

/* The switch used to indicate rotation lock */
//#define SW_ROTATION_LOCK 	(SW_MAX-1)

extern void smba_gps_mag_poweron(void);
extern void smba_gps_mag_poweroff(void);
extern void smba_gps_mag_init(void);
extern int smba_bt_wifi_gpio_set(bool on);
extern int smba_bt_wifi_gpio_init(void);

extern void smba_wifi_set_cd(int val);

extern void smba_init_emc(void);
extern int smba_pinmux_init(void);
extern void smba_clks_init(void);

extern int smba_usb_register_devices(void);
extern int smba_audio_register_devices(void);
extern int smba_jack_register_devices(void);
extern int smba_gpu_register_devices(void);
extern int smba_uart_register_devices(void);
extern int smba_spi_register_devices(void);
extern int smba_aes_register_devices(void);
extern int smba_wdt_register_devices(void);
extern int smba_i2c_register_devices(void);
extern int smba_power_register_devices(void);
extern int smba_keyboard_register_devices(void);
extern int smba_touch_register_devices(void);
extern int smba_sdhci_register_devices(void);
extern int smba_sensors_register_devices(void);
extern int smba_wlan_pm_register_devices(void);
extern int smba_gps_pm_register_devices(void);
extern int smba_gsm_pm_register_devices(void);
//extern int smba_bt_pm_register_devices(void);
extern void smba_setup_bluesleep(void);
extern void smba_bt_rfkill(void);
extern int smba_nand_register_devices(void);
extern int smba_camera_register_devices(void);

/* Autocalculate framebuffer sizes */

#define TEGRA_ROUND_ALLOC(x) (((x) + 4095) & ((unsigned)(-4096)))
#define SMBA9701_FB_SIZE TEGRA_ROUND_ALLOC(1024*768*(32/8)*SMBA9701_FB_PAGES)

//#define SMBA9701_1920x1080HDMI
#if defined(SMBA9701_1920x1080HDMI)
#define SMBA9701_FB_HDMI_SIZE TEGRA_ROUND_ALLOC(1920*1080*(16/8)*2)
#else
#define SMBA9701_FB_HDMI_SIZE TEGRA_ROUND_ALLOC(1280*720*(16/8)*2)
#endif


#endif

