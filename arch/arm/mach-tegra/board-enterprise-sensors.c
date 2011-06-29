/*
 * arch/arm/mach-tegra/board-enterprise-sensors.c
 *
 * Copyright (c) 2011, NVIDIA, All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mpu.h>
#include <linux/nct1008.h>
#include <linux/regulator/consumer.h>

#include <mach/gpio.h>

#include <media/ar0832_main.h>
#include <media/ov9726.h>
#include "cpu-tegra.h"
#include "gpio-names.h"
#include "board-enterprise.h"

static struct nct1008_platform_data enterprise_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = false,
	.conv_rate = 0x08,
	.offset = 0,
	.hysteresis = 5,
	.shutdown_ext_limit = 75,
	.shutdown_local_limit = 75,
	.throttling_ext_limit = 90,
	.alarm_fn = tegra_throttling_enable,
};

static struct i2c_board_info enterprise_i2c4_nct1008_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH7),
		.platform_data = &enterprise_nct1008_pdata,
	}
};

static inline void enterprise_msleep(u32 t)
{
	/*
	If timer value is between ( 10us - 20ms),
	usleep_range() is recommended.
	Please read Documentation/timers/timers-howto.txt.
	*/
	usleep_range(t*1000, t*1000 + 500);
}

static void enterprise_nct1008_init(void)
{
	int ret;

	tegra_gpio_enable(TEGRA_GPIO_PH7);
	ret = gpio_request(TEGRA_GPIO_PH7, "temp_alert");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(TEGRA_GPIO_PH7);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(TEGRA_GPIO_PH7);
		return;
	}

	i2c_register_board_info(4, enterprise_i2c4_nct1008_board_info,
				ARRAY_SIZE(enterprise_i2c4_nct1008_board_info));
}

#define SENSOR_MPU_NAME "mpu3050"
static struct mpu3050_platform_data mpu3050_data = {
	.int_config  = 0x10,
	/* Orientation matrix for MPU on enterprise */
	.orientation = { -1, 0, 0, 0, -1, 0, 0, 0, 1 },
	.level_shifter = 0,

	.accel = {
		.get_slave_descr = get_accel_slave_descr,
		.adapt_num   = 0,
		.bus         = EXT_SLAVE_BUS_SECONDARY,
		.address     = 0x0F,
		/* Orientation matrix for Kionix on enterprise */
		.orientation = { 0, 1, 0, -1, 0, 0, 0, 0, 1 },

	},

	.compass = {
		.get_slave_descr = get_compass_slave_descr,
		.adapt_num   = 0,
		.bus         = EXT_SLAVE_BUS_PRIMARY,
		.address     = 0x0C,
		/* Orientation matrix for AKM on enterprise */
		.orientation = { 0, 1, 0, -1, 0, 0, 0, 0, 1 },
	},
};

static struct i2c_board_info __initdata mpu3050_i2c0_boardinfo[] = {
	{
		I2C_BOARD_INFO(SENSOR_MPU_NAME, 0x68),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH4),
		.platform_data = &mpu3050_data,
	},
};

static void enterprise_mpuirq_init(void)
{
	int ret = 0;

	tegra_gpio_enable(TEGRA_GPIO_PH4);
	ret = gpio_request(TEGRA_GPIO_PH4, SENSOR_MPU_NAME);
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(TEGRA_GPIO_PH4);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(TEGRA_GPIO_PH4);
		return;
	}

	i2c_register_board_info(0, mpu3050_i2c0_boardinfo,
				ARRAY_SIZE(mpu3050_i2c0_boardinfo));
}

static struct i2c_board_info enterprise_i2c0_isl_board_info[] = {
	{
		I2C_BOARD_INFO("isl29028", 0x44),
	}
};

static void enterprise_isl_init(void)
{
	i2c_register_board_info(0, enterprise_i2c0_isl_board_info,
				ARRAY_SIZE(enterprise_i2c0_isl_board_info));
}

typedef enum {
	CAM_REAR_LEFT,
	CAM_REAR_RIGHT,
	CAM_FRONT,
	NUM_OF_CAM
} CAMERA_INDEX;

struct ent_camera_regulators {
	struct regulator	*cam_reg;
	struct regulator	*csi_reg;
	bool			cam_pwr_state[NUM_OF_CAM];
};

static struct ent_camera_regulators ent_cam_pwr;

static int enterprise_cam_pwr(CAMERA_INDEX cam, bool pwr_on)
{
	int ret = 0;
	bool *pwr_ptr = ent_cam_pwr.cam_pwr_state;

	/*
	* SW must turn on 1.8V first then 2.8V
	* SW must turn off 2.8V first then 1.8V
	*/
	if (pwr_on) {
		if (!pwr_ptr[CAM_REAR_LEFT] &&
		    !pwr_ptr[CAM_REAR_RIGHT] &&
		    !pwr_ptr[CAM_REAR_RIGHT])	{
			if (ent_cam_pwr.csi_reg == NULL) {
			    ent_cam_pwr.csi_reg = regulator_get(NULL, "avdd_dsi_csi");
			    if (IS_ERR_OR_NULL(ent_cam_pwr.csi_reg)) {
				    pr_err("%s: get csi pwr err\n", __func__);
				    ret = PTR_ERR(ent_cam_pwr.csi_reg);
				    goto enterprise_cam_pwr_fail;
			    }
			}

			ret = regulator_enable(ent_cam_pwr.csi_reg);
			if (ret) {
				pr_err("%s: enable csi pwr err\n", __func__);
				goto enterprise_cam_pwr_fail;
			}

			if (ent_cam_pwr.cam_reg == NULL) {
			    ent_cam_pwr.cam_reg = regulator_get(NULL, "vddio_cam");
			    if (IS_ERR_OR_NULL(ent_cam_pwr.cam_reg)) {
				    pr_err("%s: get cam pwr err\n", __func__);
				    ret = PTR_ERR(ent_cam_pwr.cam_reg);
				    goto enterprise_cam_pwr_fail;
			    }
			}

			ret = regulator_enable(ent_cam_pwr.cam_reg);
			if (ret) {
				pr_err("%s: enable cam pwr err\n", __func__);
				goto enterprise_cam_pwr_fail;
			}
		}

		// turn on powers
		gpio_set_value(CAM_LDO_1V8_EN_L_GPIO, 1);
		enterprise_msleep(20);
		gpio_set_value(CAM_LDO_2V8_EN_L_GPIO, 1);
		pwr_ptr[cam] = true;
	}
	else {
		pwr_ptr[cam] = false;
		if (!pwr_ptr[CAM_REAR_LEFT] &&
		    !pwr_ptr[CAM_REAR_RIGHT] &&
		    !pwr_ptr[CAM_REAR_RIGHT]) {
			if (ent_cam_pwr.cam_reg)
			    regulator_disable(ent_cam_pwr.cam_reg);

			if (ent_cam_pwr.csi_reg)
			    regulator_disable(ent_cam_pwr.csi_reg);

			// turn powers off
			gpio_set_value(CAM_LDO_2V8_EN_L_GPIO, 0);
			enterprise_msleep(20);
			gpio_set_value(CAM_LDO_1V8_EN_L_GPIO, 0);
		}
	}
	return 0;

enterprise_cam_pwr_fail:
	if (!IS_ERR_OR_NULL(ent_cam_pwr.cam_reg))
		regulator_put(ent_cam_pwr.cam_reg);
	ent_cam_pwr.cam_reg = NULL;

	if (!IS_ERR_OR_NULL(ent_cam_pwr.csi_reg))
		regulator_put(ent_cam_pwr.csi_reg);
	ent_cam_pwr.csi_reg = NULL;

	return ret;
}

static int enterprise_ar0832_power_on(void)
{
	int ret = 0;
#if 0
	csi_reg = regulator_get(NULL, "avdd_dsi_csi");
	if (IS_ERR_OR_NULL(csi_reg)) {
		pr_err("%s: get csi pwr err\n", __func__);
		return PTR_ERR(cam_reg);
	}

	ret = regulator_enable(csi_reg);
	if (ret) {
		pr_err("%s: enable csi pwr err\n", __func__);
		goto fail_regulator_csi_reg;
	}

	cam_reg = regulator_get(NULL, "vddio_cam");
	if (IS_ERR_OR_NULL(cam_reg)) {
		pr_err("%s: get cam pwr err\n", __func__);
		return PTR_ERR(cam_reg);
	}

	ret = regulator_enable(cam_reg);
	if (ret) {
		pr_err("%s: enable cam pwr err\n", __func__);
		goto fail_regulator_cam_reg;
	}

	pr_info("%s: enable 1.8V...\n", __func__);
	gpio_set_value(CAM_LDO_1V8_EN_L_GPIO, 1);
	enterprise_msleep(20);
	pr_info("%s: enable 2.8V...\n", __func__);
	gpio_set_value(CAM_LDO_2V8_EN_L_GPIO, 1);
#else
	ret = enterprise_cam_pwr(CAM_REAR_RIGHT, true);
	if (ret)
		return ret;
#endif

	gpio_set_value(CAM1_PWDN_GPIO, 1);
	enterprise_msleep(5);
	gpio_set_value(CAM1_RST_L_GPIO, 1);

	// switch mipi mux to rear camera
	gpio_set_value(CAM_CSI_MUX_SEL_GPIO, CAM_CSI_MUX_SEL_REAR);

	/*
	It takes 2400 EXTCLK for ar0832 to be ready for I2c.
	EXTCLK is 10 ~ 24MCK. 1 ms should be enough to cover
	at least 2400 EXTCLK within frequency range.
	*/
	enterprise_msleep(1);
	return 0;
#if 0
fail_regulator_cam_reg:
	regulator_put(cam_reg);
fail_regulator_csi_reg:
	regulator_put(csi_reg);

	return ret;
#endif
}

static int enterprise_ar0832_power_off(void)
{
#if 0
	if (cam_reg) {
		regulator_disable(cam_reg);
		regulator_put(cam_reg);
	}
	if (csi_reg) {
		regulator_disable(csi_reg);
		regulator_put(csi_reg);
	}

	gpio_set_value(CAM_LDO_2V8_EN_L_GPIO, 0);
	mdelay(20);
	gpio_set_value(CAM_LDO_1V8_EN_L_GPIO, 0);
#else
	enterprise_cam_pwr(CAM_REAR_RIGHT, true);
#endif

	return 0;
}

struct enterprise_cam_gpio {
	int gpio;
	const char *label;
	int value;
};

#define TEGRA_CAMERA_GPIO(_gpio, _label, _value)	\
	{						\
		.gpio = _gpio,				\
		.label = _label,				\
		.value = _value,			\
	}

static struct enterprise_cam_gpio enterprise_cam_gpio_data[] = {
	[0] = TEGRA_CAMERA_GPIO(CAM_LDO_1V8_EN_L_GPIO, "cam_ldo_1v8", 0),
	[1] = TEGRA_CAMERA_GPIO(CAM_LDO_2V8_EN_L_GPIO, "cam_ldo_2v8", 0),
	[2] = TEGRA_CAMERA_GPIO(CAM_CSI_MUX_SEL_GPIO, "cam_csi_sel", 1),

	[3] = TEGRA_CAMERA_GPIO(CAM1_RST_L_GPIO, "cam1_rst_lo", 0),
	[4] = TEGRA_CAMERA_GPIO(CAM1_PWDN_GPIO, "cam1_pwdn", 1),

	[5] = TEGRA_CAMERA_GPIO(CAM2_RST_L_GPIO, "cam2_rst_lo", 0),
	[6] = TEGRA_CAMERA_GPIO(CAM2_PWDN_GPIO, "cam2_pwdn", 1),

	[7] = TEGRA_CAMERA_GPIO(CAM3_RST_L_GPIO, "cam3_rst_lo", 0),
	[8] = TEGRA_CAMERA_GPIO(CAM3_PWDN_GPIO, "cam3_pwdn", 1),
};

struct ar0832_platform_data enterprise_ar0832_data = {
	.power_on = enterprise_ar0832_power_on,
	.power_off = enterprise_ar0832_power_off,
};

static int enterprise_ov9726_power_on(void)
{
	pr_info("ov9726 power on\n");

	enterprise_cam_pwr(CAM_FRONT, true);
	enterprise_msleep(1);
	gpio_set_value(CAM3_PWDN_GPIO, CAM3_PWDN_FALSE);               // turn on ov9726

	enterprise_msleep(5);

	gpio_set_value(CAM3_RST_L_GPIO, CAM3_RST_L_TRUE);
	enterprise_msleep(5);
	gpio_set_value(CAM3_RST_L_GPIO, CAM3_RST_L_FALSE);
	enterprise_msleep(20);

	// switch mipi mux to front camera
	gpio_set_value(CAM_CSI_MUX_SEL_GPIO, CAM_CSI_MUX_SEL_FRONT);

	pr_info("cs0n: %x %x %x\n",
		gpio_get_value(CAM3_PWDN_GPIO),
		gpio_get_value(CAM_LDO_1V8_EN_L_GPIO),
		gpio_get_value(CAM_CSI_MUX_SEL_GPIO));

	return 0;
}

static int enterprise_ov9726_power_off(void)
{
	pr_info("ov9726 power off\n");

	gpio_set_value(CAM3_RST_L_GPIO, CAM3_RST_L_TRUE);     // pull low the RST pin of ov9726
	enterprise_msleep(1);
	gpio_set_value(CAM3_PWDN_GPIO, CAM3_PWDN_TRUE);      // turn off ov9726
	enterprise_msleep(1);
	enterprise_cam_pwr(CAM_FRONT, false);
	pr_info("cs0n: %x %x %x\n",
		gpio_get_value(CAM3_PWDN_GPIO),
		gpio_get_value(CAM_LDO_1V8_EN_L_GPIO),
		gpio_get_value(CAM_CSI_MUX_SEL_GPIO));

	return 0;
}

struct ov9726_platform_data enterprise_ov9726_data = {
    .power_on = enterprise_ov9726_power_on,
    .power_off = enterprise_ov9726_power_off,
};

static struct i2c_board_info enterprise_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO("ar0832", 0x36),
		.platform_data = &enterprise_ar0832_data,
	},
	{
		I2C_BOARD_INFO("ar0832_focuser", 0x36),
	},
	{
		I2C_BOARD_INFO("ov9726", OV9726_I2C_ADDR >> 1 ),
		.platform_data = &enterprise_ov9726_data,
	},
};

static int enterprise_cam_init(void)
{
	int ret;
	int i;

	pr_info("%s:++\n", __func__);

	memset(&ent_cam_pwr, sizeof(struct ent_camera_regulators), 0);

	for (i = 0; i < ARRAY_SIZE(enterprise_cam_gpio_data); i++) {
		ret = gpio_request(enterprise_cam_gpio_data[i].gpio,
				   enterprise_cam_gpio_data[i].label);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail_free_gpio;
		}
		gpio_direction_output(enterprise_cam_gpio_data[i].gpio,
				      enterprise_cam_gpio_data[i].value);
		gpio_export(enterprise_cam_gpio_data[i].gpio, false);
		tegra_gpio_enable(enterprise_cam_gpio_data[i].gpio);
	}

	i2c_register_board_info(2, enterprise_i2c2_board_info,
				ARRAY_SIZE(enterprise_i2c2_board_info));

	return 0;

fail_free_gpio:
	pr_err("%s enterprise_cam_init failed!\n", __func__);
	while (i--)
		gpio_free(enterprise_cam_gpio_data[i].gpio);
	return ret;
}

int __init enterprise_sensors_init(void)
{
	int ret;
	enterprise_isl_init();
	enterprise_nct1008_init();
	enterprise_mpuirq_init();
	ret = enterprise_cam_init();

	return ret;
}
