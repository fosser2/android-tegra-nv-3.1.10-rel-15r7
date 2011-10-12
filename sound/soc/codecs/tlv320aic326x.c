/*
 * linux/sound/soc/codecs/tlv320aic3262.c
 *
 * Copyright (C) 2011 Mistral Solutions Pvt Ltd.
 *
 * Based on sound/soc/codecs/tlv320aic3262.c
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * The TLV320AIC3262 is a flexible, low-power, low-voltage stereo audio
 * codec with digital microphone inputs and programmable outputs.
 *
 * History:
 *
 * Rev 0.1   ASoC driver support    Mistral         20-01-2011
 *
 *	 The AIC325x ASoC driver is ported for the codec AIC3262.
 * Rev 0.2   ASoC driver support    Mistral         21-03-2011
 *	 The AIC326x ASoC driver is updated abe changes.
 *
 * Rev 0.3   ASoC driver support    Mistral         12.09.2011
 *	fixed the compilation issues for Whistler support(compilation
 *	errors were identified by nVidia)
 *
 * Rev 0.4   ASoC driver support    Mistral         27.09.2011
 *	 The AIC326x driver ported for Nvidia cardhu.
 */

/*
 *****************************************************************************
 * INCLUDES
 *****************************************************************************
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include "tlv320aic326x.h"

/*#define DEBUG*/
#undef DEBUG

/*Select the macro to decide on the DAC master volume controls.
 *2 independent or one combined
 */
/*#define DAC_INDEPENDENT_VOL*/
#undef DAC_INDEPENDENT_VOL

#ifdef DEBUG
	#define DBG(x...)	printk(x)
#else
	#define DBG(x...) do {} while (0)
#endif

#ifdef AIC3262_TiLoad
	extern int aic3262_driver_init(struct snd_soc_codec *codec);
#endif

/*
 *****************************************************************************
 * Global Variable
 *****************************************************************************
 */
static u8 aic3262_reg_ctl;

u8 dac_reg = 0, adc_gain = 0, hpl = 0, hpr = 0;
u8 rec_amp = 0, rampr = 0, spk_amp = 0;
/* whenever aplay/arecord is run, aic3262_hw_params() function gets called.
 * This function reprograms the clock dividers etc. this flag can be used to
 * disable this when the clock dividers are programmed by pps config file
 */
static int soc_static_freq_config = 1;

/*
 *****************************************************************************
 * Structure Declaration
 *****************************************************************************
 */
static struct snd_soc_device *aic3262_socdev;
static struct snd_soc_codec *aic3262_codec;

/*
 *****************************************************************************
 * Macros
 *****************************************************************************
 */


/* ASoC Widget Control definition for a single Register based Control */
#define SOC_SINGLE_AIC3262(xname) \
{\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = __new_control_info, .get = __new_control_get,\
	.put = __new_control_put, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
}
#define SOC_SINGLE_N(xname, xreg, xshift, xmax, xinvert) \
{\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = n_control_info, .get = n_control_get,\
	.put = n_control_put, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.private_value =  ((unsigned long)&(struct soc_mixer_control)) \
	{.reg = xreg, .shift = xshift, .rshift = xshift, .max = xmax, \
	.invert = xinvert} }

/* ASoC Widget Control definition for a Double Register based Control */

#define SOC_DOUBLE_R_N(xname, reg_left, reg_right, xshift, xmax, xinvert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.info = snd_soc_info_volsw_2r_n, \
	.get = snd_soc_get_volsw_2r_n, .put = snd_soc_put_volsw_2r_n, \
	.private_value = (unsigned long)&(struct soc_mixer_control) \
		{.reg = reg_left, .rreg = reg_right, .shift = xshift, \
		.max = xmax, .invert = xinvert} }

#define SND_SOC_DAPM_SWITCH_N(wname, wreg, wshift, winvert) \
{	.id = snd_soc_dapm_switch, .name = wname, .reg = wreg, .shift = wshift,\
	.invert = winvert, .kcontrols = NULL, .num_kcontrols = 0}
/*
 *****************************************************************************
 * Function Prototype
 *****************************************************************************
 */
static int aic3262_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai);

static int aic3262_mute(struct snd_soc_dai *dai, int mute);

static int aic3262_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir);

static int aic3262_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt);

static int aic3262_set_bias_level(struct snd_soc_codec *codec,
						enum snd_soc_bias_level level);

static u8 aic3262_read(struct snd_soc_codec *codec, u16 reg);

static int __new_control_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo);

static int __new_control_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int __new_control_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);
static int aic3262_change_book(struct snd_soc_codec *codec, u8 new_book);

#ifdef DAC_INDEPENDENT_VOL
/*
 *----------------------------------------------------------------------------
 * Function : n_control_info
 * Purpose  : This function is to initialize data for new control required to
 *            program the AIC3262 registers.
 *
 *----------------------------------------------------------------------------
 */
static int n_control_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;

	if (max == 1)
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = shift == rshift ? 1 : 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : n_control_get
 * Purpose  : This function is to read data of new control for
 *            program the AIC3262 registers.
 *
 *----------------------------------------------------------------------------
 */
static int n_control_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 val;
	unsigned short mask, shift;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	if (!strcmp(kcontrol->id.name, "Left DAC Volume")) {
		mask = AIC3262_8BITS_MASK;
		shift = 0;
		val = aic3262_read(codec, mc->reg);
		ucontrol->value.integer.value[0] =
		    (val <= 48) ? (val + 127) : (val - 129);
	}
	if (!strcmp(kcontrol->id.name, "Right DAC Volume")) {
		mask = AIC3262_8BITS_MASK;
		shift = 0;
		val = aic3262_read(codec, mc->reg);
		ucontrol->value.integer.value[0] =
		    (val <= 48) ? (val + 127) : (val - 129);
	}

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put
 * Purpose  : new_control_put is called to pass data from user/application to
 *            the driver.
 *
 *----------------------------------------------------------------------------
 */
static int n_control_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u8 val, val_mask;
	int reg, err;
	unsigned int invert = mc->invert;
	int max = mc->max;
	DBG("n_control_put\n");
	reg = mc->reg;
	val = ucontrol->value.integer.value[0];
	if (invert)
		val = max - val;
	if (!strcmp(kcontrol->id.name, "Left DAC Volume")) {
		DBG("LDAC\n");
		val = (val >= 127) ? (val - 127) : (val + 129);
		val_mask = AIC3262_8BITS_MASK;
	}
	if (!strcmp(kcontrol->id.name, "Right DAC Volume")) {
		DBG("RDAC\n");
		val = (val >= 127) ? (val - 127) : (val + 129);
		val_mask = AIC3262_8BITS_MASK;
	}

	err = snd_soc_update_bits_locked(codec, reg, val_mask, val);
	if (err < 0) {
		printk(KERN_ERR "Error while updating bits\n");
		return err;
	}

	return 0;
}
#endif /*#ifdef DAC_INDEPENDENT_VOL*/
/*
 *------------------------------------------------------------------------------
 * snd_soc_info_volsw_2r_n - double mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a double mixer control that
 * spans 2 codec registers.
 *
 * Returns 0 for success.
 *------------------------------------------------------------------------------
 */
int snd_soc_info_volsw_2r_n(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max;

	if (max == 1)
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max;
	return 0;
}

/*
 *------------------------------------------------------------------------------
 * snd_soc_get_volsw_2r_n - double mixer get callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to get the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 *------------------------------------------------------------------------------
 */
int snd_soc_get_volsw_2r_n(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask;
	unsigned int invert = mc->invert;
	unsigned short val, val2;

	if (!strcmp(kcontrol->id.name, "PCM Playback Volume")) {
		mask = AIC3262_8BITS_MASK;
		shift = 0;
	} else if (!strcmp(kcontrol->id.name, "HP Driver Gain")) {
		mask = 0x3F;
		shift = 0;
	} else if (!strcmp(kcontrol->id.name, "PGA Capture Volume")) {
		mask = 0x7F;
		shift = 0;
	} else if (!strcmp(kcontrol->id.name, "REC Driver Volume")) {
		mask = 0x3F;
		shift = 0;
	} else if (!strcmp(kcontrol->id.name, "LO to HP Volume")) {
		mask = 0x7F;
		shift = 0;
	} else if (!strcmp(kcontrol->id.name, "MA Volume")) {
		mask = 0x7F;
		shift = 0;
	} else {
		printk(KERN_ERR "Invalid kcontrol name\n");
		return -1;
	}

	/* Read, update the corresponding Registers */
	val = (snd_soc_read(codec, reg) >> shift) & mask;
	val2 = (snd_soc_read(codec, reg2) >> shift) & mask;

	if (!strcmp(kcontrol->id.name, "PCM Playback Volume")) {
		ucontrol->value.integer.value[0] =
		    (val <= 48) ? (val + 127) : (val - 129);
		ucontrol->value.integer.value[1] =
		    (val2 <= 48) ? (val2 + 127) : (val2 - 129);
	} else if (!strcmp(kcontrol->id.name, "HP Driver Gain")) {
		ucontrol->value.integer.value[0] =
		    (val >= 57) ? (val - 57) : (val + 7);
		ucontrol->value.integer.value[1] =
		    (val2 >= 57) ? (val2 - 57) : (val2 + 7);
	} else if (!strcmp(kcontrol->id.name, "PGA Capture Volume")) {
		ucontrol->value.integer.value[0] =
		    (val <= 40) ? (val + 24) : (val - 104);
		ucontrol->value.integer.value[1] =
		    (val2 <= 40) ? (val2 + 24) : (val2 - 104);
	} else if (!strcmp(kcontrol->id.name, "REC Driver Volume")) {
		ucontrol->value.integer.value[0] = ((val >= 0) & (val <= 29)) ?
						 (val + 7) : (val - 57);
		ucontrol->value.integer.value[1] = ((val2 >= 0) &
		    (val2 <= 29)) ? (val2 + 7) : (val2 - 57);

	} else if (!strcmp(kcontrol->id.name, "LO to HP Volume")) {
		ucontrol->value.integer.value[0] = ((val >= 0) & (val <= 116)) ?
				 (val + 1) : ((val == 127) ? (0) : (117));
		ucontrol->value.integer.value[1] = ((val2 >= 0) & (val2 <= 116))
				 ? (val2 + 1) : ((val2 == 127) ? (0) : (117));

	} else if (!strcmp(kcontrol->id.name, "MA Volume")) {
		ucontrol->value.integer.value[0] = (val <= 40) ?
					 (41 - val) : (val = 0);
		ucontrol->value.integer.value[1] = (val2 <= 40) ?
					 (41 - val2) : (val2 = 0);
	}

	if (invert) {
		ucontrol->value.integer.value[0] =
			max - ucontrol->value.integer.value[0];
		ucontrol->value.integer.value[1] =
			max - ucontrol->value.integer.value[1];
	}

	return 0;
}
/*
 *------------------------------------------------------------------------------
 * snd_soc_put_volsw_2r_n - double mixer set callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to set the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 *------------------------------------------------------------------------------
 */
int snd_soc_put_volsw_2r_n(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask;
	unsigned int invert = mc->invert;
	int err;
	unsigned short val, val2, val_mask;

	mask = 0x00FF;

	val = (ucontrol->value.integer.value[0] & mask);
	val2 = (ucontrol->value.integer.value[1] & mask);
	if (invert) {
		val = max - val;
		val2 = max - val2;
	}

	/* Check for the string name of the kcontrol */
	if (!strcmp(kcontrol->id.name, "PCM Playback Volume")) {
		val = (val >= 127) ? (val - 127) : (val + 129);
		val2 = (val2 >= 127) ? (val2 - 127) : (val2 + 129);
		val_mask = AIC3262_8BITS_MASK;	/* 8 bits */
	} else if ((!strcmp(kcontrol->id.name, "HP Driver Gain")) ||
		   (!strcmp(kcontrol->id.name, "LO Driver Gain"))) {
		val = (val <= 6) ? (val + 57) : (val - 7);
		val2 = (val2 <= 6) ? (val2 + 57) : (val2 - 7);
		val_mask = 0x3F;	/* 6 bits */
		DBG("val=%d, val2=%d", val, val2);
	} else if (!strcmp(kcontrol->id.name, "PGA Capture Volume")) {
		val = (val >= 24) ? ((val <= 64) ?
			(val-24) : (40)) : (val + 104);
		val2 = (val2 >= 24) ?
			((val2 <= 64) ? (val2 - 24) : (40)) : (val2 + 104);
		val_mask = 0x7F;	/* 7 bits */
	} else if (!strcmp(kcontrol->id.name, "LO to REC Volume")) {

		val = (val <= 116) ?
			 (val % 116) : ((val == 117) ? (127) : (117));
		val2 = (val2 <= 116) ?
			(val2 % 116) : ((val2 == 117) ? (127) : (117));
		val_mask = 0x7F;
	} else if (!strcmp(kcontrol->id.name, "REC Driver Volume")) {

		val = (val <= 7) ? (val + 57) : ((val < 36) ? (val - 7) : (29));
		val2 = (val2 <= 7) ?
				(val2 + 57) : ((val2 < 36) ? (val2 - 7) : (29));
		val_mask = 0x3F;
	} else if (!strcmp(kcontrol->id.name, "LO to HP Volume")) {

		val = ((val > 0) & (val <= 117)) ?
			 (val - 1) : ((val == 0) ? (127) : (116));
		val2 = ((val2 > 0) & (val2 <= 117)) ?
			 (val2 - 1) : ((val2 == 0) ? (127) : (116));
		val_mask = 0x7F;
	} else if (!strcmp(kcontrol->id.name, "MA Volume")) {

		val = ((val <= 41) & (val > 0)) ?
			 (41 - val) : ((val > 41) ? (val = 41) : (63));
		val2 = ((val2 <= 41) & (val2 > 0)) ?
			 (41 - val2) : ((val2 > 41) ? (val2 = 41) : (63));
		val_mask = 0x7F;
	} else {
		printk(KERN_ERR "Invalid control name\n");
		return -1;
	}

	val = val << shift;
	val2 = val2 << shift;

	err = snd_soc_update_bits_locked(codec, reg, val_mask, val);
	if (err < 0)
		return err;

	err = snd_soc_update_bits_locked(codec, reg2, val_mask, val2);
	return err;
}

static int __new_control_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65535;

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_get
 * Purpose  : This function is to read data of new control for
 *            program the AIC3262 registers.
 *
 *----------------------------------------------------------------------------
 */
static int __new_control_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u32 val;
	val = aic3262_read(codec, aic3262_reg_ctl);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : __new_control_put
 * Purpose  : new_control_put is called to pass data from user/application to
 *            the driver.
 *
 *----------------------------------------------------------------------------
 */
static int __new_control_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	u8 data[2];

	u32 data_from_user = ucontrol->value.integer.value[0];

	aic3262_change_book(codec, 0);
	aic3262_reg_ctl = data[0] = (u8) ((data_from_user & 0xFF00) >> 8);
	data[1] = (u8) ((data_from_user & 0x00FF));

	if (!data[0])
		aic3262->page_no = data[1];
	DBG("reg = %d val = %x\n", data[0], data[1]);
	if (codec->hw_write(codec->control_data, data, 2) != 2) {
		printk(KERN_ERR "Error in i2c write\n");
		return -EIO;


	}

	return 0;
}


/*
 *****************************************************************************
 * Structure Initialization
 *****************************************************************************
 */
static const struct snd_kcontrol_new aic3262_snd_controls[] = {
	/* Output */
	#ifndef DAC_INDEPENDENT_VOL
	/* sound new kcontrol for PCM Playback volume control */
	SOC_DOUBLE_R_N("PCM Playback Volume", DAC_LVOL, DAC_RVOL, 0, 0xAf, 0),
	#endif
	/* sound new kcontrol for HP driver gain */
	SOC_DOUBLE_R_N("HP Driver Gain", HPL_VOL, HPR_VOL, 0, 21, 0),
	/* sound new kcontrol for SPK driver gain*/
	SOC_DOUBLE("SPK Driver Gain", SPK_AMP_CNTL_R4, 0, 4, 5, 0),
	/* Reveiver driver Gain volume*/
	SOC_DOUBLE_R_N("REC Driver Volume",
			REC_AMP_CNTL_R5, RAMPR_VOL, 0, 36, 0),
	/* sound new kcontrol for PGA capture volume */
	SOC_DOUBLE_R_N("PGA Capture Volume", LADC_VOL, RADC_VOL, 0, 0x3F,
			     0),
	SOC_DOUBLE("ADC Fine Gain ", ADC_FINE_GAIN, 4, 0, 4, 1),
	SOC_DOUBLE_R("PGA MIC Volume", MICL_PGA, MICR_PGA, 0, 95, 0),

	SOC_DOUBLE_R("LO to REC Volume", RAMP_CNTL_R1, RAMP_CNTL_R2, 0, 116, 1),
	SOC_DOUBLE_R_N("LO to HP Volume",
		 HP_AMP_CNTL_R2, HP_AMP_CNTL_R3, 0, 117, 0),
	SOC_DOUBLE_R("LO to SPK Volume",
		 SPK_AMP_CNTL_R2, SPK_AMP_CNTL_R3, 0, 116, 1),
	SOC_DOUBLE("ADC channel mute", ADC_FINE_GAIN, 7, 3, 1, 0),

	SOC_DOUBLE("DAC MUTE", DAC_MVOL_CONF, 2, 3, 1, 1),

	/* sound new kcontrol for Programming the registers from user space */
	SOC_SINGLE_AIC3262("Program Registers"),

	SOC_SINGLE("RESET", RESET_REG, 0 , 1, 0),

	SOC_SINGLE("DAC VOL SOFT STEPPING", DAC_MVOL_CONF, 0, 2, 0),

	#ifdef DAC_INDEPENDENT_VOL
	SOC_SINGLE_N("Left DAC Volume", DAC_LVOL, 0, 0xAF, 0),
	SOC_SINGLE_N("Right DAC Volume", DAC_RVOL, 0, 0xAF, 0),
	#endif

	SOC_SINGLE("DAC AUTO MUTE CONTROL", DAC_MVOL_CONF, 4, 7, 0),
	SOC_SINGLE("RIGHT MODULATOR SETUP", DAC_MVOL_CONF, 7, 1, 0),

	SOC_SINGLE("ADC Volume soft stepping", ADC_CHANNEL_POW, 0, 3, 0),

	SOC_DOUBLE_R_N("MA Volume",
		 LADC_PGA_MAL_VOL, RADC_PGA_MAR_VOL, 0, 42, 0),

	SOC_SINGLE("Mic Bias ext independent enable", MIC_BIAS_CNTL, 7, 1, 0),
	SOC_SINGLE("MICBIAS_EXT ON", MIC_BIAS_CNTL, 6, 1, 0),
	SOC_SINGLE("MICBIAS EXT Power Level", MIC_BIAS_CNTL, 4, 3, 0),

	SOC_SINGLE("MICBIAS_INT ON", MIC_BIAS_CNTL, 2, 1, 0),
	SOC_SINGLE("MICBIAS INT Power Level", MIC_BIAS_CNTL, 0, 3, 0),

	SOC_DOUBLE("DRC_EN_CTL", DRC_CNTL_R1, 6, 5, 1, 0),
	SOC_SINGLE("DRC_THRESHOLD_LEVEL", DRC_CNTL_R1, 2, 7, 1),
	SOC_SINGLE("DRC_HYSTERISIS_LEVEL", DRC_CNTL_R1, 0, 7, 0),

	SOC_SINGLE("DRC_HOLD_LEVEL", DRC_CNTL_R2, 3, 0x0F, 0),
	SOC_SINGLE("DRC_GAIN_RATE", DRC_CNTL_R2, 0, 4, 0),
	SOC_SINGLE("DRC_ATTACK_RATE", DRC_CNTL_R3, 4, 0x0F, 1),
	SOC_SINGLE("DRC_DECAY_RATE", DRC_CNTL_R3, 0, 0x0F, 1),

	SOC_SINGLE("BEEP_GEN_EN", BEEP_CNTL_R1, 7, 1, 0),
	SOC_DOUBLE_R("BEEP_VOL_CNTL", BEEP_CNTL_R1, BEEP_CNTL_R2, 0, 0x0F, 1),
	SOC_SINGLE("BEEP_MAS_VOL", BEEP_CNTL_R2, 6, 3, 0),

	SOC_DOUBLE_R("AGC_EN", LAGC_CNTL, RAGC_CNTL, 7, 1, 0),
	SOC_DOUBLE_R("AGC_TARGET_LEVEL", LAGC_CNTL, RAGC_CNTL, 4, 7, 1),

	SOC_DOUBLE_R("AGC_GAIN_HYSTERESIS", LAGC_CNTL, RAGC_CNTL, 0, 3, 0),
	SOC_DOUBLE_R("AGC_HYSTERESIS", LAGC_CNTL_R2, RAGC_CNTL_R2, 6, 3, 0),
	SOC_DOUBLE_R("AGC_NOISE_THRESHOLD",
		 LAGC_CNTL_R2, RAGC_CNTL_R2, 1, 31, 1),

	SOC_DOUBLE_R("AGC_MAX_GAIN", LAGC_CNTL_R3, RAGC_CNTL_R3, 0, 116, 0),
	SOC_DOUBLE_R("AGC_ATCK_TIME", LAGC_CNTL_R4, RAGC_CNTL_R4, 3, 31, 0),
	SOC_DOUBLE_R("AGC_ATCK_SCALE_FACTOR",
		 LAGC_CNTL_R4, RAGC_CNTL_R4, 0, 7, 0),

	SOC_DOUBLE_R("AGC_DECAY_TIME", LAGC_CNTL_R5, RAGC_CNTL_R5, 3, 31, 0),
	SOC_DOUBLE_R("AGC_DECAY_SCALE_FACTOR",
		 LAGC_CNTL_R5, RAGC_CNTL_R5, 0, 7, 0),
	SOC_DOUBLE_R("AGC_NOISE_DEB_TIME",
		 LAGC_CNTL_R6, RAGC_CNTL_R6, 0, 31, 0),

	SOC_DOUBLE_R("AGC_SGL_DEB_TIME",
		 LAGC_CNTL_R7, RAGC_CNTL_R7, 0, 0x0F, 0),

	SOC_SINGLE("DAC PRB Selection", DAC_PRB, 0, 25, 0),

	SOC_SINGLE("INTERRUPT FLAG - Read only", 46, 0, 255, 0),
	SOC_SINGLE("INTERRUPT STICKY FLAG - Read only", 44, 0, 255, 0),
	SOC_SINGLE("INT1 CONTROL", 48, 0, 255, 0),
	SOC_SINGLE("GPIO1 CONTROL", (PAGE_4 + 86), 0, 255, 0),
	SOC_SINGLE("HP_DEPOP", HP_DEPOP, 0, 255, 0),

	#if defined(FULL_IN_CNTL)

	SOC_SINGLE("IN1L_2_LMPGA_P_CTL", LMIC_PGA_PIN, 6, 3, 0),
	SOC_SINGLE("IN2L_2_LMPGA_P_CTL", LMIC_PGA_PIN, 4, 3, 0),
	SOC_SINGLE("IN3L_2_LMPGA_P_CTL", LMIC_PGA_PIN, 2, 3, 0),
	SOC_SINGLE("IN1R_2_LMPGA_P_CTL", LMIC_PGA_PIN, 0, 3, 0),

	SOC_SINGLE("IN4L_2_LMPGA_P_CTL", LMIC_PGA_PM_IN4, 5, 1, 0),
	SOC_SINGLE("IN4R_2_LMPGA_M_CTL", LMIC_PGA_PM_IN4, 4, 1, 0),

	SOC_SINGLE("CM1_2_LMPGA_M_CTL", LMIC_PGA_MIN, 6, 3, 0),
	SOC_SINGLE("IN2R_2_LMPGA_M_CTL", LMIC_PGA_MIN, 4, 3, 0),
	SOC_SINGLE("IN3R_2_LMPGA_M_CTL", LMIC_PGA_MIN, 2, 3, 0),
	SOC_SINGLE("CM2_2_LMPGA_M_CTL", LMIC_PGA_MIN, 0, 3, 0),

	SOC_SINGLE("IN1R_2_RMPGA_P_CTL", RMIC_PGA_PIN, 6, 3, 0),
	SOC_SINGLE("IN2R_2_RMPGA_P_CTL", RMIC_PGA_PIN, 4, 3, 0),
	SOC_SINGLE("IN3R_2_RMPGA_P_CTL", RMIC_PGA_PIN, 2, 3, 0),
	SOC_SINGLE("IN2L_2_RMPGA_P_CTL", RMIC_PGA_PIN, 0, 3, 0),

	SOC_SINGLE("IN4R_2_RMPGA_P_CTL", RMIC_PGA_PM_IN4, 5, 1, 0),
	SOC_SINGLE("IN4L_2_RMPGA_M_CTL", RMIC_PGA_PM_IN4, 4, 1, 0),

	SOC_SINGLE("CM1_2_RMPGA_M_CTL", RMIC_PGA_MIN, 6, 3, 0),
	SOC_SINGLE("IN1L_2_RMPGA_M_CTL", RMIC_PGA_MIN, 4, 3, 0),
	SOC_SINGLE("IN3L_2_RMPGA_M_CTL", RMIC_PGA_MIN, 2, 3, 0),
	SOC_SINGLE("CM2_2_RMPGA_M_CTL", RMIC_PGA_MIN, 0, 3, 0),

	#endif

	SOC_DOUBLE("MA_EN_CNTL", MA_CNTL, 3, 2, 1, 0),
	SOC_DOUBLE("IN1 LO DIRECT BYPASS", MA_CNTL, 5, 4, 1, 0),
	SOC_DOUBLE("IN1 LO BYPASS VOLUME" , LINE_AMP_CNTL_R2, 3, 0, 3, 1),
	SOC_DOUBLE("MA LO BYPASS EN",  LINE_AMP_CNTL_R2, 7, 6, 1, 0),

};

/* the sturcture contains the different values for mclk */
static const struct aic3262_rate_divs aic3262_divs[] = {
/*
 * mclk, rate, p_val, pll_j, pll_d, dosr, ndac, mdac, aosr, nadc, madc, blck_N,
 * codec_speficic_initializations
 */
	/* 8k rate */
	{12000000, 8000, 1, 8, 1920, 128, 12, 8, 128, 8, 6, 4,
	 {{0, 60, 1}, {0, 61, 1} } },
	{12288000, 8000, 1, 1, 3333, 128, 12, 8, 128, 8, 6, 4,
	{ {0, 60, 1}, {0, 61, 1} } },
	{24000000, 8000, 1, 4, 96, 128, 12, 8, 128, 12, 8, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	/* 11.025k rate */
	{12000000, 11025, 1, 1, 8816, 1024, 8, 2, 128, 8, 2, 48,
	{{0, 60, 1}, {0, 61, 1} } },
	{12288000, 11025, 1, 1, 8375, 1024, 8, 2, 128, 8, 2, 48,
	{{0, 60, 1}, {0, 61, 1} } },
	{24000000, 11025, 1, 3, 7632, 128, 8, 8, 128, 8, 8, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	/* 16k rate */
	{12000000, 16000, 1, 8, 1920, 128, 8, 6, 128, 8, 6, 4,
	 {{0, 60, 1}, {0, 61, 1} } },
	{12288000, 16000, 1, 2, 6667, 128, 8, 6, 128, 8, 6, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{24000000, 16000, 1, 4, 96, 128, 8, 6, 128, 8, 6, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	/* 22.05k rate */
	{12000000, 22050, 1, 3, 7632, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{12288000, 22050, 1, 3, 675, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{24000000, 22050, 1, 3, 7632, 128, 8, 3, 128, 8, 3, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	/* 32k rate */
	{12000000, 32000, 1, 5, 4613, 128, 8, 2, 128, 8, 2, 4,
	 {{0, 60, 1}, {0, 61, 1} } },
	{12288000, 32000, 1, 5, 3333, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{24000000, 32000, 1, 4, 96, 128, 6, 4, 128, 6, 4, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	/* 44.1k rate */
	{12000000, 44100, 1, 7, 5264, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{12288000, 44100, 1, 7, 3548, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{24000000, 44100, 1, 3, 7632, 128, 4, 4, 64, 4, 4, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	/* 48k rate */
	{12000000, 48000, 1, 8, 1920, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{12288000, 48000, 1, 8, 52, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	{24000000, 48000, 1, 4, 960, 128, 4, 4, 128, 4, 4, 4,
	{{0, 60, 1}, {0, 61, 1} } },
	/*96k rate */
	{12000000, 96000, 1, 16, 3840, 128, 8, 2, 128, 8, 2 , 4,
	{{0, 60, 7}, {0, 61, 7} } },
	{24000000, 96000, 1, 4, 960, 128, 4, 2, 128, 4, 2, 2,
	{{0, 60, 7}, {0, 61, 7} } },
	/*192k */
	{12000000, 192000, 1, 32, 7680, 128, 8, 2, 128, 8, 2, 4,
	{{0, 60, 17}, {0, 61, 13} } },
	{24000000, 192000, 1, 4, 960, 128, 2, 2, 128, 2, 2, 4,
	{{0, 60, 17}, {0, 61, 13} } },
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz.,
 *          playback and capture, DAI runtime information viz. state of DAI
 *			and pop wait state, and DAI private data.
 *          The AIC3262 rates ranges from 8k to 192k
 *          The PCM bit format supported are 16, 20, 24 and 32 bits
 *----------------------------------------------------------------------------
 */
struct snd_soc_dai_ops aic3262_dai_ops = {
		.hw_params = aic3262_hw_params,
		.digital_mute = aic3262_mute,
		.set_sysclk = aic3262_set_dai_sysclk,
		.set_fmt = aic3262_set_dai_fmt,
		};

struct snd_soc_dai tlv320aic3262_dai = {
	.name = "aic3262",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = AIC3262_RATES,
		     .formats = AIC3262_FORMATS,},
	.capture = {
		    .stream_name = "Capture",
		    .channels_min = 1,
		    .channels_max = 2,
		    .rates = AIC3262_RATES,
		    .formats = AIC3262_FORMATS,},
	.ops = &aic3262_dai_ops,
};
EXPORT_SYMBOL_GPL(tlv320aic3262_dai);

/*
 *****************************************************************************
 * Initializations
 *****************************************************************************
 */
/*
 * AIC3262 register cache
 * We are caching the registers here.
 * There is no point in caching the reset register.
 *
 * NOTE: In AIC3262, there are 127 registers supported in both page0 and page1
 *       The following table contains the page0 and page 1 and page 3
 *       registers values.
 */
static const u8 aic3262_reg[AIC3262_CACHEREGNUM] = {
	0x00, 0x00, 0x10, 0x00,	/* 0 */
	0x03, 0x40, 0x11, 0x08,	/* 4 */
	0x00, 0x00, 0x00, 0x82,	/* 8 */
	0x88, 0x00, 0x80, 0x02,	/* 12 */
	0x00, 0x08, 0x01, 0x01,	/* 16 */
	0x80, 0x01, 0x00, 0x04,	/* 20 */
	0x00, 0x00, 0x01, 0x00,	/* 24 */
	0x00, 0x00, 0x01, 0x00,	/* 28 */
	0x00, 0x00, 0x00, 0x00,	/* 32 */
	0x00, 0x00, 0x00, 0x00,	/* 36 */
	0x00, 0x00, 0x00, 0x00,	/* 40 */
	0x00, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x00,	/* 48 */
	0x00, 0x42, 0x02, 0x02,	/* 52 */
	0x42, 0x02, 0x02, 0x02,	/* 56 */
	0x00, 0x00, 0x00, 0x01,	/* 60 */
	0x01, 0x00, 0x14, 0x00,	/* 64 */
	0x0C, 0x00, 0x00, 0x00,	/* 68 */
	0x00, 0x00, 0x00, 0xEE,	/* 72 */
	0x10, 0xD8, 0x10, 0xD8,	/* 76 */
	0x00, 0x00, 0x88, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00,	/* 84 */
	0x7F, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 92 */
	0x7F, 0x00, 0x00, 0x00,	/* 96 */
	0x00, 0x00, 0x00, 0x00,	/* 100 */
	0x00, 0x00, 0x00, 0x00,	/* 104 */
	0x00, 0x00, 0x00, 0x00,	/* 108 */
	0x00, 0x00, 0x00, 0x00,	/* 112 */
	0x00, 0x00, 0x00, 0x00,	/* 116 */
	0x00, 0x00, 0x00, 0x00,	/* 120 */
	0x00, 0x00, 0x00, 0x00,	/* 124 - PAGE0 Registers(127) ends here */
	0x01, 0x00, 0x08, 0x00,	/* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00,	/* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x10,	/* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00,	/* 140, PAGE1-12 */
	0x40, 0x40, 0x40, 0x40,	/* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00,	/* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00,	/* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00,	/* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00,	/* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00,	/* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00,	/* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00,	/* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00,	/* 176, PAGE1-48 */
	0x00, 0x00, 0x00, 0x00,	/* 180, PAGE1-52 */
	0x00, 0x00, 0x00, 0x80,	/* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00,	/* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00,	/* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00,	/* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00,	/* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00,	/* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00,	/* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00,	/* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00,	/* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00,	/* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00,	/* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00,	/* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00,	/* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00,	/* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00,	/* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00,	/* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00,	/* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00,	/* 252, PAGE1-124 Page 1 Registers Ends Here */
	0x00, 0x00, 0x00, 0x00,	/* 256, PAGE2-0  */
	0x00, 0x00, 0x00, 0x00,	/* 260, PAGE2-4  */
	0x00, 0x00, 0x00, 0x00,	/* 264, PAGE2-8  */
	0x00, 0x00, 0x00, 0x00,	/* 268, PAGE2-12 */
	0x00, 0x00, 0x00, 0x00,	/* 272, PAGE2-16 */
	0x00, 0x00, 0x00, 0x00,	/* 276, PAGE2-20 */
	0x00, 0x00, 0x00, 0x00,	/* 280, PAGE2-24 */
	0x00, 0x00, 0x00, 0x00,	/* 284, PAGE2-28 */
	0x00, 0x00, 0x00, 0x00,	/* 288, PAGE2-32 */
	0x00, 0x00, 0x00, 0x00,	/* 292, PAGE2-36 */
	0x00, 0x00, 0x00, 0x00,	/* 296, PAGE2-40 */
	0x00, 0x00, 0x00, 0x00,	/* 300, PAGE2-44 */
	0x00, 0x00, 0x00, 0x00,	/* 304, PAGE2-48 */
	0x00, 0x00, 0x00, 0x00,	/* 308, PAGE2-52 */
	0x00, 0x00, 0x00, 0x00,	/* 312, PAGE2-56 */
	0x00, 0x00, 0x00, 0x00,	/* 316, PAGE2-60 */
	0x00, 0x00, 0x00, 0x00,	/* 320, PAGE2-64 */
	0x00, 0x00, 0x00, 0x00,	/* 324, PAGE2-68 */
	0x00, 0x00, 0x00, 0x00,	/* 328, PAGE2-72 */
	0x00, 0x00, 0x00, 0x00,	/* 332, PAGE2-76 */
	0x00, 0x00, 0x00, 0x00,	/* 336, PAGE2-80 */
	0x00, 0x00, 0x00, 0x00,	/* 340, PAGE2-84 */
	0x00, 0x00, 0x00, 0x00,	/* 344, PAGE2-88 */
	0x00, 0x00, 0x00, 0x00,	/* 348, PAGE2-92 */
	0x00, 0x00, 0x00, 0x00,	/* 352, PAGE2-96 */
	0x00, 0x00, 0x00, 0x00,	/* 356, PAGE2-100 */
	0x00, 0x00, 0x00, 0x00,	/* 360, PAGE2-104 */
	0x00, 0x00, 0x00, 0x00,	/* 364, PAGE2-108 */
	0x00, 0x00, 0x00, 0x00,	/* 368, PAGE2-112*/
	0x00, 0x00, 0x00, 0x00,	/* 372, PAGE2-116*/
	0x00, 0x00, 0x00, 0x00,	/* 376, PAGE2-120*/
	0x00, 0x00, 0x00, 0x00,	/* 380, PAGE2-124 Page 2 Registers Ends Here */
	0x00, 0x00, 0x00, 0x00,	/* 384, PAGE3-0  */
	0x00, 0x00, 0x00, 0x00,	/* 388, PAGE3-4  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-8  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-12  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-16  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-20  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-24  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-28  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-32  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-36  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-40  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-44  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-48  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-52  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-56  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-60  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-64  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE3-68  */
	0x00, 0x00, 0x00, 0x00,	/* 328, PAGE3-72 */
	0x00, 0x00, 0x00, 0x00,	/* 332, PAGE3-76 */
	0x00, 0x00, 0x00, 0x00,	/* 336, PAGE3-80 */
	0x00, 0x00, 0x00, 0x00,	/* 340, PAGE3-84 */
	0x00, 0x00, 0x00, 0x00,	/* 344, PAGE3-88 */
	0x00, 0x00, 0x00, 0x00,	/* 348, PAGE3-92 */
	0x00, 0x00, 0x00, 0x00,	/* 352, PAGE3-96 */
	0x00, 0x00, 0x00, 0x00,	/* 356, PAGE3-100 */
	0x00, 0x00, 0x00, 0x00,	/* 360, PAGE3-104 */
	0x00, 0x00, 0x00, 0x00,	/* 364, PAGE3-108 */
	0x00, 0x00, 0x00, 0x00,	/* 368, PAGE3-112*/
	0x00, 0x00, 0x00, 0x00,	/* 372, PAGE3-116*/
	0x00, 0x00, 0x00, 0x00,	/* 376, PAGE3-120*/
	0x00, 0x00, 0x00, 0x00,	/* 380, PAGE3-124 Page 3 Registers Ends Here */
	0x00, 0x00, 0x00, 0x00,	/* 384, PAGE4-0  */
	0x00, 0x00, 0x00, 0x00,	/* 388, PAGE4-4  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-8  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-12  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-16  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-20  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-24  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-28  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-32  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-36  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-40  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-44  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-48  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-52  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-56  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-60  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-64  */
	0x00, 0x00, 0x00, 0x00,	/* 392, PAGE4-68  */
	0x00, 0x00, 0x00, 0x00,	/* 328, PAGE4-72 */
	0x00, 0x00, 0x00, 0x00,	/* 332, PAGE4-76 */
	0x00, 0x00, 0x00, 0x00,	/* 336, PAGE4-80 */
	0x00, 0x00, 0x00, 0x00,	/* 340, PAGE4-84 */
	0x00, 0x00, 0x00, 0x00,	/* 344, PAGE4-88 */
	0x00, 0x00, 0x00, 0x00,	/* 348, PAGE4-92 */
	0x00, 0x00, 0x00, 0x00,	/* 352, PAGE4-96 */
	0x00, 0x00, 0x00, 0x00,	/* 356, PAGE4-100 */
	0x00, 0x00, 0x00, 0x00,	/* 360, PAGE4-104 */
	0x00, 0x00, 0x00, 0x00,	/* 364, PAGE4-108 */
	0x00, 0x00, 0x00, 0x00,	/* 368, PAGE4-112*/
	0x00, 0x00, 0x00, 0x00,	/* 372, PAGE4-116*/
	0x00, 0x00, 0x00, 0x00,	/* 376, PAGE4-120*/
	0x00, 0x00, 0x00, 0x00,	/* 380, PAGE4-124 Page 2 Registers Ends Here */

};

/*
 *------------------------------------------------------------------------------
 * aic3262 initialization data
 * This structure initialization contains the initialization required for
 * AIC326x.
 * These registers values (reg_val) are written into the respective AIC3262
 * register offset (reg_offset) to  initialize AIC326x.
 * These values are used in aic3262_init() function only.
 *------------------------------------------------------------------------------
 */
static const struct aic3262_configs aic3262_reg_init[] = {
	/* CLOCKING */

	{0, RESET_REG, 1},
	{0, RESET_REG, 0},

	{0, PASI_DAC_DP_SETUP,  0xc0},	/*DAC */
	{0, DAC_MVOL_CONF,  0x00},	/*DAC un-muted*/
	/* set default volumes */
	{0, DAC_LVOL, 0x01},
	{0, DAC_RVOL, 0x01},
	{0, HPL_VOL,  0x3a},
	{0, HPR_VOL,  0x3a},
	{0, SPK_AMP_CNTL_R2, 0x14},
	{0, SPK_AMP_CNTL_R3, 0x14},
	{0, SPK_AMP_CNTL_R4, 0x33},
	{0, REC_AMP_CNTL_R5, 0x82},
	{0, RAMPR_VOL, 20},
	{0, RAMP_CNTL_R1, 70},
	{0, RAMP_CNTL_R2, 70},

	/* DRC Defaults */
	{0, DRC_CNTL_R1, 0x6c},
	{0, DRC_CNTL_R2, 16},

	/* DEPOP SETTINGS */
	{0, HP_DEPOP, 0x14},
	{0, RECV_DEPOP, 0x14},

	{0, POWER_CONF, 0x00},	 /* Disconnecting AVDD-DVD weak link*/
	{0, REF_PWR_DLY, 0x01},
	{0, CM_REG, 0x00},	/*CM - default*/
	{0, LDAC_PTM, 0},	/*LDAC_PTM - default*/
	{0, RDAC_PTM, 0},	/*RDAC_PTM - default*/
	{0, HP_CTL, 0x30},	/*HP output percentage - at 75%*/
	{0, LADC_VOL, 0x01},	/*LADC volume*/
	{0, RADC_VOL, 0x01},	/*RADC volume*/

	{0, DAC_ADC_CLKIN_REG, 0x33},	/*DAC ADC CLKIN*/
	{0, PLL_CLKIN_REG, 0x00},	/*PLL CLKIN*/
	{0, PLL_PR_POW_REG, 0x11},	/*PLL Power=0-down, P=1, R=1 vals*/
	{0, 0x3d, 1},

	{0, LMIC_PGA_PIN, 0x55},	/*IN1_L select - - 10k -LMICPGA_P*/
	{0, LMIC_PGA_MIN, 0x40},	/*CM to LMICPGA-M*/
	{0, RMIC_PGA_PIN, 0x55},	/*IN1_R select - - 10k -RMIC_PGA_P*/
	{0, RMIC_PGA_MIN, 0x40},	/*CM to RMICPGA_M*/
	{0, (PAGE_1 + 0x79), 33},	/*LMIC-PGA-POWERUP-DELAY - default*/
	{0, (PAGE_1 + 0x7a), 1},	/*FIXMELATER*/


	{0, ADC_CHANNEL_POW, 0xc2}, /*ladc, radc ON , SOFT STEP disabled*/
	{0, ADC_FINE_GAIN, 0x00},   /*ladc - unmute, radc - unmute*/
	{0, MICL_PGA, 0x4f},
	{0, MICR_PGA, 0x4f},
	{0, MIC_BIAS_CNTL, 0xFC},
	/*   ASI1 Configuration */
	{0, ASI1_BUS_FMT, 0},
	{0, ASI1_BWCLK_CNTL_REG, 0x00},		/* originaly 0x24*/
	{0, ASI1_BCLK_N_CNTL, 1},
	{0, ASI1_BCLK_N, 0x84},

	{0, MA_CNTL, 0},			/* Mixer Amp disabled */
	{0, LINE_AMP_CNTL_R2, 0x00},		/* Line Amp Cntl disabled */

	{0, BEEP_CNTL_R1, 0x05},
	{0, BEEP_CNTL_R2, 0x04},

	/* Interrupt config for headset detection */
	{0, INT1_CNTL, 0x81},
	{0, TIMER_REG, 0x8c},
	{0, INT_FMT, 0x40},
	{0, GPIO1_IO_CNTL, 0x14},
	{0, HP_DETECT, 0x94},


};

static int reg_init_size =
	sizeof(aic3262_reg_init) / sizeof(struct aic3262_configs);

static const struct snd_kcontrol_new aic3262_snd_controls2[] = {

	SOC_DOUBLE_R("IN1 MPGA Route", LMIC_PGA_PIN, RMIC_PGA_PIN, 6, 3, 0),
	SOC_DOUBLE_R("IN2 MPGA Route", LMIC_PGA_PIN, RMIC_PGA_PIN, 4, 3, 0),
	SOC_DOUBLE_R("IN3 MPGA Route", LMIC_PGA_PIN, RMIC_PGA_PIN, 2, 3, 0),
	SOC_DOUBLE_R("IN4 MPGA Route",
		 LMIC_PGA_PM_IN4, RMIC_PGA_PM_IN4, 5, 1, 0),
};
static const struct snd_soc_dapm_widget aic3262_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("Left DAC", "Playback", PASI_DAC_DP_SETUP, 7, 0),
	SND_SOC_DAPM_DAC("Right DAC", "Playback", PASI_DAC_DP_SETUP, 6, 0),

	SND_SOC_DAPM_SWITCH_N("LDAC_2_HPL", HP_AMP_CNTL_R1, 5, 0),
	SND_SOC_DAPM_SWITCH_N("RDAC_2_HPR", HP_AMP_CNTL_R1, 4, 0),

	SND_SOC_DAPM_PGA("HPL Driver", HP_AMP_CNTL_R1, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPR Driver", HP_AMP_CNTL_R1, 0, 0, NULL, 0),

	SND_SOC_DAPM_SWITCH_N("LDAC_2_LOL", LINE_AMP_CNTL_R1, 7, 0),
	SND_SOC_DAPM_SWITCH_N("RDAC_2_LOR", LINE_AMP_CNTL_R1, 6, 0),

	SND_SOC_DAPM_PGA("LOL Driver", LINE_AMP_CNTL_R1, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("LOR Driver", LINE_AMP_CNTL_R1, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA("SPKL Driver", SPK_AMP_CNTL_R1, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPKR Driver", SPK_AMP_CNTL_R1, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA("RECL Driver", REC_AMP_CNTL_R5, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("RECR Driver", REC_AMP_CNTL_R5, 6, 0, NULL, 0),

	SND_SOC_DAPM_ADC("Left ADC", "Capture", ADC_CHANNEL_POW, 7, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Capture", ADC_CHANNEL_POW, 6, 0),

	SND_SOC_DAPM_SWITCH("IN1L Route",
		 LMIC_PGA_PIN, 6, 0, &aic3262_snd_controls2[0]),
	SND_SOC_DAPM_SWITCH("IN2L Route",
		 LMIC_PGA_PIN, 4, 0, &aic3262_snd_controls2[1]),
	SND_SOC_DAPM_SWITCH("IN3L Route",
		 LMIC_PGA_PIN, 2, 0, &aic3262_snd_controls2[2]),
	SND_SOC_DAPM_SWITCH("IN4L Route",
		 LMIC_PGA_PM_IN4, 5, 0, &aic3262_snd_controls2[3]),
	SND_SOC_DAPM_SWITCH("IN1R Route",
		 RMIC_PGA_PIN, 6, 0, &aic3262_snd_controls2[4]),
	SND_SOC_DAPM_SWITCH("IN2R Route",
		 RMIC_PGA_PIN, 4, 0, &aic3262_snd_controls2[5]),
	SND_SOC_DAPM_SWITCH("IN3R Route",
		 RMIC_PGA_PIN, 2, 0, &aic3262_snd_controls2[6]),
	SND_SOC_DAPM_SWITCH("IN4R Route",
		 RMIC_PGA_PM_IN4, 5, 0, &aic3262_snd_controls2[7]),

	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("LOL"),
	SND_SOC_DAPM_OUTPUT("LOR"),
	SND_SOC_DAPM_OUTPUT("SPKL"),
	SND_SOC_DAPM_OUTPUT("SPKR"),
	SND_SOC_DAPM_OUTPUT("RECL"),
	SND_SOC_DAPM_OUTPUT("RECR"),

	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("IN3L"),
	SND_SOC_DAPM_INPUT("IN4L"),

	SND_SOC_DAPM_INPUT("IN1R"),
	SND_SOC_DAPM_INPUT("IN2R"),
	SND_SOC_DAPM_INPUT("IN3R"),
	SND_SOC_DAPM_INPUT("IN4R"),
};

static const struct snd_soc_dapm_route aic3262_dapm_routes[] = {
	{"LDAC_2_HPL", NULL, "Left DAC"},
	{"HPL Driver", NULL, "LDAC_2_HPL"},
	{"HPL", NULL, "HPL Driver"},
	{"RDAC_2_HPR", NULL, "Right DAC"},
	{"HPR Driver", NULL, "RDAC_2_HPR"},
	{"HPR", NULL, "HPR Driver"},

	{"LDAC_2_LOL", NULL, "Left DAC"},
	{"LOL Driver", NULL, "LDAC_2_LOL"},
	{"LOL", NULL, "LOL Driver"},
	{"RDAC_2_LOR", NULL, "Right DAC"},
	{"LOR Driver", NULL, "RDAC_2_LOR"},
	{"LOR", NULL, "LOR Driver"},

	{"SPKL Driver", NULL, "LOL"},
	{"SPKL", NULL, "SPKL Driver"},
	{"SPKR Driver", NULL, "LOR"},
	{"SPKR", NULL, "SPKR Driver"},

	{"RECL Driver", NULL, "LOL"},
	{"RECL", NULL, "RECL Driver"},
	{"RECR Driver", NULL, "LOR"},
	{"RECR", NULL, "RECR Driver"},

	{"Left ADC", "IN1L Route", "IN1L"},
	{"Left ADC", "IN2L Route", "IN2L"},
	{"Left ADC", "IN3L Route", "IN3L"},
	{"Left ADC", "IN4L Route", "IN4L"},

	{"Right ADC", "IN1R Route", "IN1R"},
	{"Right ADC", "IN2R Route", "IN2R"},
	{"Right ADC", "IN3R Route", "IN3R"},
	{"Right ADC", "IN4R Route", "IN4R"},
/*
	{"LOL Driver", NULL, "IN1L"},
	{"LOR Driver", NULL, "IN1R"},
*/
};
#define AIC3262_DAPM_ROUTE_NUM \
	(sizeof(aic3262_dapm_routes)/sizeof(struct snd_soc_dapm_route))

/*
 *****************************************************************************
 * Function Definitions
 *****************************************************************************
 */


/*
 *----------------------------------------------------------------------------
 * Function : aic3262_change_page
 * Purpose  : This function is to switch between page 0 and page 1.
 *
 *----------------------------------------------------------------------------
 */
int aic3262_change_page(struct snd_soc_codec *codec, u8 new_page)
{
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	u8 data[2];

	data[0] = 0;
	data[1] = new_page;
	aic3262->page_no = new_page;

	if (codec->hw_write(codec->control_data, data, 2) != 2) {
		printk(KERN_ERR "Error in changing page to %d\n", new_page);
		return -1;
	}
	DBG("# Changing page to %d\r\n", new_page);

	return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : aic3262_change_book
 * Purpose  : This function is to switch between books
 *
 *----------------------------------------------------------------------------
 */
int aic3262_change_book(struct snd_soc_codec *codec, u8 new_book)
{
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	u8 data[2];

	data[0] = 0x7F;
	data[1] = new_book;
	aic3262->book_no = new_book;
	aic3262_change_page(codec, 0);
	if (codec->hw_write(codec->control_data, data, 2) != 2) {
		printk(KERN_ERR "Error in changing Book\n");
		return -1;
	}
	DBG("# Changing book to %d\r\n", new_book);

	return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : aic3262_write_reg_cache
 * Purpose  : This function is to write aic3262 register cache
 *
 *----------------------------------------------------------------------------
 */
void aic3262_write_reg_cache(struct snd_soc_codec *codec,
					   u16 reg, u8 value)
{
	u8 *cache = codec->reg_cache;

	if (reg >= AIC3262_CACHEREGNUM)
		return;

	cache[reg] = value;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_read
 * Purpose  : This function is to read the aic3262 register space.
 *
 *----------------------------------------------------------------------------
 */
u8 aic3262_read(struct snd_soc_codec *codec, u16 reg)
{
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	u8 value;
	u8 page = reg / 128;

	reg = reg % 128;

	if (aic3262->page_no != page)
		aic3262_change_page(codec, page);

	i2c_master_send(codec->control_data, (char *)&reg, 1);
	i2c_master_recv(codec->control_data, &value, 1);
	DBG("r %2x %02x\r\n", reg, value);
	return value;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_write
 * Purpose  : This function is to write to the aic3262 register space.
 *
 *----------------------------------------------------------------------------
 */
int aic3262_write(struct snd_soc_codec *codec, u16 reg, u8 value)
{
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	u8 data[2];
	u8 page;

	page = reg / 128;
	data[AIC3262_REG_OFFSET_INDEX] = reg % 128;
	if (aic3262->page_no != page)
		aic3262_change_page(codec, page);

	/* data is
	 *   D15..D8 aic3262 register offset
	 *   D7...D0 register data
	 */
	data[AIC3262_REG_DATA_INDEX] = value & AIC3262_8BITS_MASK;
#if defined(EN_REG_CACHE)
	if ((page >= 0) & (page <= 4))
		aic3262_write_reg_cache(codec, reg, value);

#endif
	if (!data[AIC3262_REG_OFFSET_INDEX]) {
		/* if the write is to reg0 update aic3262->page_no */
		aic3262->page_no = value;
	}

	DBG("w %2x %02x\r\n",
		data[AIC3262_REG_OFFSET_INDEX], data[AIC3262_REG_DATA_INDEX]);
	if (codec->hw_write(codec->control_data, data, 2) != 2) {
		printk(KERN_ERR "Error in i2c write\n");
		return -EIO;
	}

	return 0;
}

/*
 *------------------------------------------------------------------------------
 * Function : aic3262_write__
 * Purpose  : This function is to write to the aic3262 register space.
 *            (low level).
 *------------------------------------------------------------------------------
 */

int aic3262_write__(struct i2c_client *client, const char *buf, int count)
{
	u8 data[3];
	int ret;
	data[0] = *buf;
	data[1] = *(buf+1);
	data[2] = *(buf+2);
	DBG("w %2x %02x\r\n",
		data[AIC3262_REG_OFFSET_INDEX], data[AIC3262_REG_DATA_INDEX]);
	ret = i2c_master_send(client, data, 2);
	if (ret < 2) {
		printk(
		KERN_ERR"I2C write Error : bytes written = %d\n\n", ret);
		return -EIO;
	}

	return ret;
}
/*
 *----------------------------------------------------------------------------
 * Function : aic3262_reset_cache
 * Purpose  : This function is to reset the cache.
 *----------------------------------------------------------------------------
 */
int aic3262_reset_cache(struct snd_soc_codec *codec)
{
#if defined(EN_REG_CACHE)
	if (codec->reg_cache) {
	    memcpy(codec->reg_cache, aic3262_reg, sizeof(aic3262_reg));
	    return 0;
	}

	codec->reg_cache = kmemdup(aic3262_reg,
			sizeof(aic3262_reg), GFP_KERNEL);
	if (!codec->reg_cache) {
	    printk(KERN_ERR "aic32x4: kmemdup failed\n");
	    return -ENOMEM;
	}
#endif
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_get_divs
 * Purpose  : This function is to get required divisor from the "aic3262_divs"
 *            table.
 *
 *----------------------------------------------------------------------------
 */
static inline int aic3262_get_divs(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic3262_divs); i++) {
		if ((aic3262_divs[i].rate == rate)
		    && (aic3262_divs[i].mclk == mclk)) {
			return i;
		}
	}
	printk(KERN_ERR "Master clock and sample rate is not supported\n");
	return -EINVAL;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_add_controls
 * Purpose  : This function is to add non dapm kcontrols.  The different
 *            controls are in "aic3262_snd_controls" table.
 *            The following different controls are supported
 *                # PCM Playback volume control
 *				  # PCM Playback Volume
 *				  # HP Driver Gain
 *				  # HP DAC Playback Switch
 *				  # PGA Capture Volume
 *				  # Program Registers
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(aic3262_snd_controls); i++) {
		err =
		    snd_ctl_add(codec->card,
				snd_soc_cnew(&aic3262_snd_controls[i], codec,
					     NULL));
		if (err < 0) {
			printk(KERN_ERR "Invalid control\n");
			return err;
		}
	}
	for (i = 0; i < ARRAY_SIZE(aic3262_snd_controls2); i++) {
		err =
		    snd_ctl_add(codec->card,
				snd_soc_cnew(&aic3262_snd_controls2[i], codec,
					     NULL));
		if (err < 0) {
			printk(KERN_ERR "Invalid control\n");
			return err;
		}
	}
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_add_widgets
 * Purpose  : This function is to add the dapm widgets
 *            The following are the main widgets supported
 *                # Left DAC to Left Outputs
 *                # Right DAC to Right Outputs
 *		  # Left Inputs to Left ADC
 *		  # Right Inputs to Right ADC
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_add_widgets(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic3262_dapm_widgets); i++)
		snd_soc_dapm_new_control(codec, &aic3262_dapm_widgets[i]);

	/* set up audio path interconnects */
	DBG("#Completed adding new dapm widget controls size=%d\n",
		ARRAY_SIZE(aic3262_dapm_widgets));
	snd_soc_dapm_add_routes(codec, aic3262_dapm_routes,
				AIC3262_DAPM_ROUTE_NUM);
	DBG("#Completed adding DAPM routes\n");
	snd_soc_dapm_new_widgets(codec);
	DBG("#Completed updating dapm\n");
	return 0;
}
/*
 *----------------------------------------------------------------------------
 * Function : reg_def_conf
 * Purpose  : This function is to reset the codec book 0 registers
 *
 *----------------------------------------------------------------------------
 */
int reg_def_conf(struct snd_soc_codec *codec)
{
	int i = 0, ret;
	aic3262_change_page(codec, 0);
	aic3262_change_book(codec, 0);

	/* Configure the Codec with the default Initialization Values */
	for (i = 0; i < reg_init_size; i++) {
		ret = aic3262_write(codec, aic3262_reg_init[i].reg_offset,
			aic3262_reg_init[i].reg_val);
	}
	return ret;
}
/*
 *----------------------------------------------------------------------------
 * Function : aic3262_hw_params
 * Purpose  : This function is to set the hardware parameters for AIC3262.
 *            The functions set the sample rate and audio serial data word
 *            length.
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	int i, j;
	u8 data;

	aic3262_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	i = aic3262_get_divs(aic3262->sysclk, params_rate(params));

	if (i < 0) {
		printk(KERN_ERR "sampling rate not supported\n");
		return i;
	}

	if (soc_static_freq_config) {
		/*fix R value to 1 and will make P & J=K.D as varialble */

		/* Setting P & R values are set to 1 and 1 at init*/

		/* J value */
		aic3262_write(codec, PLL_J_REG, aic3262_divs[i].pll_j);

		/* MSB & LSB for D value */

		aic3262_write(codec, PLL_D_MSB, (aic3262_divs[i].pll_d >> 8));
		aic3262_write(codec, PLL_D_LSB,
			      (aic3262_divs[i].pll_d & AIC3262_8BITS_MASK));

		/* NDAC divider value */
		data = aic3262_read(codec, NDAC_DIV_POW_REG);
		DBG(KERN_INFO "# reading NDAC = %d , NDAC_DIV_POW_REG = %x\n",
			aic3262_divs[i].ndac, data);
		aic3262_write(codec, NDAC_DIV_POW_REG,
			 ((data & 0x80)|(aic3262_divs[i].ndac)));
		DBG(KERN_INFO "# writing NDAC = %d , NDAC_DIV_POW_REG = %x\n",
			aic3262_divs[i].ndac,
			((data & 0x80)|(aic3262_divs[i].ndac)));

		/* MDAC divider value */
		data = aic3262_read(codec, MDAC_DIV_POW_REG);
		DBG(KERN_INFO "# reading MDAC = %d , MDAC_DIV_POW_REG = %x\n",
			aic3262_divs[i].mdac, data);
		aic3262_write(codec, MDAC_DIV_POW_REG,
			 ((data & 0x80)|(aic3262_divs[i].mdac)));
		DBG(KERN_INFO "# writing MDAC = %d , MDAC_DIV_POW_REG = %x\n",
		aic3262_divs[i].mdac, ((data & 0x80)|(aic3262_divs[i].mdac)));

		/* DOSR MSB & LSB values */
		aic3262_write(codec, DOSR_MSB_REG, aic3262_divs[i].dosr >> 8);
		DBG(KERN_INFO "# writing DOSR_MSB_REG = %d\n",
			(aic3262_divs[i].dosr >> 8));
		aic3262_write(codec, DOSR_LSB_REG,
			      aic3262_divs[i].dosr & AIC3262_8BITS_MASK);
		DBG(KERN_INFO "# writing DOSR_LSB_REG = %d\n",
			(aic3262_divs[i].dosr & AIC3262_8BITS_MASK));

		/* NADC divider value */
		data = aic3262_read(codec, NADC_DIV_POW_REG);
		aic3262_write(codec, NADC_DIV_POW_REG,
			 ((data & 0x80)|(aic3262_divs[i].nadc)));
		DBG(KERN_INFO "# writing NADC_DIV_POW_REG = %d\n",
			aic3262_divs[i].nadc);

		/* MADC divider value */
		data = aic3262_read(codec, MADC_DIV_POW_REG);
		aic3262_write(codec, MADC_DIV_POW_REG,
			((data & 0x80)|(aic3262_divs[i].madc)));
		DBG(KERN_INFO "# writing MADC_DIV_POW_REG = %d\n",
			aic3262_divs[i].madc);

		/* AOSR value */
		aic3262_write(codec, AOSR_REG, aic3262_divs[i].aosr);
		DBG(KERN_INFO "# writing AOSR = %d\n", aic3262_divs[i].aosr);
	}


	data = aic3262_read(codec, ASI1_BUS_FMT);

	data = data & 0xe7;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data = data | 0x00;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (0x08);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (0x10);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (0x18);
		break;
	}

	/* configure the respective Registers for the above configuration */
	aic3262_write(codec, ASI1_BUS_FMT, data);

	for (j = 0; j < NO_FEATURE_REGS; j++) {
		aic3262_write(codec,
			      aic3262_divs[i].codec_specific_regs[j].reg_offset,
			      aic3262_divs[i].codec_specific_regs[j].reg_val);
	}

	aic3262_set_bias_level(codec, SND_SOC_BIAS_ON);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_mute
 * Purpose  : This function is to mute or unmute the left and right DAC
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	DBG(KERN_INFO "#aic3262 codec : mute started\n");
	if (mute) {
		DBG("Mute if part\n");
		dac_reg = aic3262_read(codec, DAC_MVOL_CONF);
		adc_gain = aic3262_read(codec, ADC_FINE_GAIN);
		hpl = aic3262_read(codec, HPL_VOL);
		hpr = aic3262_read(codec, HPL_VOL);
		rec_amp = aic3262_read(codec, REC_AMP_CNTL_R5);
		rampr = aic3262_read(codec, RAMPR_VOL);
		spk_amp = aic3262_read(codec, SPK_AMP_CNTL_R4);
		DBG("spk_reg = %2x\n\n", spk_amp);

		aic3262_write(codec, DAC_MVOL_CONF, ((dac_reg & 0xF3) | 0x0C));
		aic3262_write(codec, ADC_FINE_GAIN, ((adc_gain & 0x77) | 0x88));
		aic3262_write(codec, HPL_VOL, 0xB9);
		aic3262_write(codec, HPL_VOL, 0xB9);
		aic3262_write(codec, REC_AMP_CNTL_R5, 0x39);
		aic3262_write(codec, RAMPR_VOL, 0x39);
		aic3262_write(codec, SPK_AMP_CNTL_R4, 0x00);
	} else {
		DBG("Mute else part\n");
		aic3262_write(codec, DAC_MVOL_CONF, (dac_reg & 0xF3));
		mdelay(5);
		aic3262_write(codec, ADC_FINE_GAIN, (adc_gain & 0x77));
		mdelay(5);
		aic3262_write(codec, HPL_VOL, hpl);
		mdelay(5);
		aic3262_write(codec, HPL_VOL, hpr);
		mdelay(5);
		aic3262_write(codec, REC_AMP_CNTL_R5, rec_amp);
		mdelay(5);
		aic3262_write(codec, RAMPR_VOL, rampr);
		mdelay(5);
		aic3262_write(codec, SPK_AMP_CNTL_R4, spk_amp);
		mdelay(5);
	}
	DBG(KERN_INFO "#aic3262 codec : mute ended\n");
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_set_dai_sysclk
 * Purpose  : This function is to set the DAI system clock
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);

	switch (freq) {
	case AIC3262_FREQ_12000000:
	case AIC3262_FREQ_12288000:
		aic3262->sysclk = freq;
		return 0;
	case AIC3262_FREQ_24000000:
		aic3262->sysclk = freq;
		return 0;
		break;
	}
	printk(KERN_ERR "Invalid frequency to set DAI system clock\n");
	return -EINVAL;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_set_dai_fmt
 * Purpose  : This function is to set the DAI format
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	u8 iface_reg, clk_reg;
	iface_reg = aic3262_read(codec, ASI1_BUS_FMT);
	/*
	iface_reg = iface_reg & ~(3 << 6 | 3 << 2);
	*/

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		DBG("setdai_fmt : SND_SOC_DAIFMT_CBM_CFM : master=1\n");
		aic3262->master = 1;
		/*Test added */
		clk_reg = aic3262_read(codec, ASI1_BWCLK_CNTL_REG);
		clk_reg = (clk_reg & 0x03);
		aic3262_write(codec, ASI1_BWCLK_CNTL_REG,
				      (clk_reg | 0x24));
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		DBG("setdai_fmt : SND_SOC_DAIFMT_CBS_CFS : master=0\n");
		aic3262->master = 0;
		/*Test added */
		clk_reg = aic3262_read(codec, ASI1_BWCLK_CNTL_REG);
		aic3262_write(codec, ASI1_BWCLK_CNTL_REG,
				      (clk_reg & 0x03));

		break;
	case SND_SOC_DAIFMT_CBS_CFM: /*new case..just for debugging*/
		DBG("SND_SOC_DAIFMT_CBS_CFM\n");
		aic3262->master = 0;
		break;
	default:
		printk(KERN_ERR "Invalid DAI master/slave interface\n");
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface_reg = (iface_reg & 0x1f);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_reg = (iface_reg & 0x1f) | 0x20;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg = (iface_reg & 0x1f) | 0x40;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg = (iface_reg & 0x1f) | 0x60;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface_reg = (iface_reg & 0x1f) | 0x80;
		break;
	default:
		printk(KERN_ERR "Invalid DAI interface format\n");
		return -EINVAL;
	}

	aic3262_write(codec, ASI1_BUS_FMT, iface_reg);
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_set_bias_level
 * Purpose  : This function is to get triggered when dapm events occurs.
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	u8 value;

	switch (level) {
		/* full On */
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		/* all power is driven by DAPM system */
		DBG(KERN_INFO "#aic3262 codec : set_bias_on started\n");

		/* Switch on PLL */
		value = aic3262_read(codec, PLL_PR_POW_REG);
		aic3262_write(codec, PLL_PR_POW_REG, ((value | 0x80)));

		/* Switch on NDAC Divider */
		value = aic3262_read(codec, NDAC_DIV_POW_REG);
		aic3262_write(codec, NDAC_DIV_POW_REG,
			((value & 0x7f) | (0x80)));

		/* Switch on MDAC Divider */
		value = aic3262_read(codec, MDAC_DIV_POW_REG);
		aic3262_write(codec, MDAC_DIV_POW_REG,
			((value & 0x7f) | (0x80)));

		/* Switch on NADC Divider */
		value = aic3262_read(codec, NADC_DIV_POW_REG);
		aic3262_write(codec, NADC_DIV_POW_REG,
			((value & 0x7f) | (0x80)));

		/* Switch on MADC Divider */
		value = aic3262_read(codec, MADC_DIV_POW_REG);
		aic3262_write(codec, MADC_DIV_POW_REG,
			((value & 0x7f) | (0x80)));


		aic3262_write(codec, ADC_CHANNEL_POW, 0xc2);
		aic3262_write(codec, ADC_FINE_GAIN, 0x00);

		DBG("#aic3262 codec : set_bias_on complete\n");

		break;

		/* partial On */
/*	case SND_SOC_BIAS_PREPARE:
		break;
*/
		/* Off, with power */
	case SND_SOC_BIAS_STANDBY:
		/*
		 * all power is driven by DAPM system,
		 * so output power is safe if bypass was set
		 */

		DBG(KERN_INFO "#aic3262 codec : set_bias_stby started\n");

		DBG("#aic3262 codec : set_bias_stby inside if condn\n");

		/* Switch off NDAC Divider */
		value = aic3262_read(codec, NDAC_DIV_POW_REG);
		aic3262_write(codec, NDAC_DIV_POW_REG,
			(value & 0x7f));

		/* Switch off MDAC Divider */
		value = aic3262_read(codec, MDAC_DIV_POW_REG);
		aic3262_write(codec, MDAC_DIV_POW_REG,
			(value & 0x7f));

		/* Switch off NADC Divider */
		value = aic3262_read(codec, NADC_DIV_POW_REG);
		aic3262_write(codec, NADC_DIV_POW_REG,
			(value & 0x7f));

		/* Switch off MADC Divider */
		value = aic3262_read(codec, MADC_DIV_POW_REG);
		aic3262_write(codec, MADC_DIV_POW_REG,
			(value & 0x7f));

		/* Switch off PLL */
		value = aic3262_read(codec, PLL_PR_POW_REG);
		aic3262_write(codec, PLL_PR_POW_REG, (value & 0x7f));

		DBG("#aic3262 codec : set_bias_stby complete\n");

		break;

		/* Off, without power */
	case SND_SOC_BIAS_OFF:
		/* force all power off */

		/* Switch off PLL */
		value = aic3262_read(codec, PLL_PR_POW_REG);
		aic3262_write(codec,
			PLL_PR_POW_REG, (value & ~(0x01 << 7)));

		/* Switch off NDAC Divider */
		value = aic3262_read(codec, NDAC_DIV_POW_REG);
		aic3262_write(codec, NDAC_DIV_POW_REG,
			(value & ~(0x01 << 7)));

		/* Switch off MDAC Divider */
		value = aic3262_read(codec, MDAC_DIV_POW_REG);
		aic3262_write(codec, MDAC_DIV_POW_REG,
			(value & ~(0x01 << 7)));

		/* Switch off NADC Divider */
		value = aic3262_read(codec, NADC_DIV_POW_REG);
		aic3262_write(codec, NADC_DIV_POW_REG,
			(value & ~(0x01 << 7)));

		/* Switch off MADC Divider */
		value = aic3262_read(codec, MADC_DIV_POW_REG);
		aic3262_write(codec, MADC_DIV_POW_REG,
			(value & ~(0x01 << 7)));
		value = aic3262_read(codec, ASI1_BCLK_N);

		break;
	}
	codec->bias_level = level;
	DBG(KERN_INFO "#aic3262 codec : set_bias exiting\n");
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_suspend
 * Purpose  : This function is to suspend the AIC3262 driver.
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	aic3262_set_bias_level(codec, SNDRV_CTL_POWER_D3cold);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_resume
 * Purpose  : This function is to resume the AIC3262 driver
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	int i;
	u8 data[2];
	u8 *cache = codec->reg_cache;

	aic3262_change_page(codec, 0);
	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(aic3262_reg); i++) {
		data[0] = i % 128;
		data[1] = cache[i];
		codec->hw_write(codec->control_data, data, 2);
	}
	aic3262_change_page(codec, 0);
	aic3262_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_hw_read
 * Purpose  : This is a low level harware read function.
 *
 *----------------------------------------------------------------------------
 */
unsigned int aic3262_hw_read(struct snd_soc_codec *codec, unsigned int count)
{
	struct i2c_client *client = codec->control_data;
	unsigned int buf;

	if (count > (sizeof(unsigned int)))
	    return 0;

	i2c_master_recv(client, (char *)&buf, count);
	return buf;
}





#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/*
 *----------------------------------------------------------------------------
 * Function : aic3262_codec_probe
 * Purpose  : This function attaches the i2c client and initializes
 *				AIC3262 CODEC.
 *            NOTE:
 *            This function is called from i2c core when the I2C address is
 *            valid.
 *            If the i2c layer weren't so broken, we could pass this kind of
 *            data around
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_codec_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	int ret;

	struct snd_soc_codec *codec;
	struct aic3262_priv *aic3262;

	aic3262 = kzalloc(sizeof(struct aic3262_priv), GFP_KERNEL);

	if (!aic3262)
		return -ENOMEM;


	codec = aic3262_codec = &aic3262->codec;

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	DBG(KERN_INFO " in codec_probe entered\n");
	codec->control_data = i2c;
	codec->dev = &i2c->dev;
	codec->name = "tlv320aic3262";
	codec->owner = THIS_MODULE;
	codec->read = aic3262_read;
	codec->write = aic3262_write;
	codec->set_bias_level = aic3262_set_bias_level;
	codec->dai = &tlv320aic3262_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = sizeof(aic3262_reg);
	codec->reg_cache =
	    kmemdup(aic3262_reg, sizeof(aic3262_reg), GFP_KERNEL);

	if (!codec->reg_cache) {
		printk(KERN_ERR "aic3262: kmemdup failed\n");
		return -ENOMEM;
	}

	codec->hw_write = (hw_write_t) aic3262_write__;
	codec->hw_read = aic3262_hw_read;

	snd_soc_codec_set_drvdata(codec, aic3262);
	i2c_set_clientdata(i2c, codec);


	aic3262->page_no = 0;
	aic3262->book_no = 0;


	tlv320aic3262_dai.dev = &i2c->dev;

	reg_def_conf(codec);

	aic3262_codec = codec;
	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register codec: %d\n", ret);
		goto err_irq;
	}

	ret = snd_soc_register_dai(&tlv320aic3262_dai);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register DAI: %d\n", ret);
		goto err_codec;
	}

	return ret;
err_codec:
	snd_soc_unregister_codec(codec);

err_irq:
	if (i2c->irq)
		free_irq(i2c->irq, aic3262);

	return ret;

}
#endif /*#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)*/

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_probe
 * Purpose  : This is first driver function called by the SoC core driver.
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = aic3262_codec;
	int ret = 0;

	if (socdev == NULL) {
		printk(KERN_ERR
		"\nAIC3262_Probe: platform_get_drvdata returned NULL\r\n");
		return -ENODEV;
	}
	socdev->card->codec = codec;

	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
	    printk(KERN_ERR "aic3262: failed to create pcms\n");
	    goto pcm_err;
	}

	aic3262_socdev = socdev;


	/* off, with power on */
	aic3262_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	aic3262_add_controls(codec);
	aic3262_add_widgets(codec);
	aic3262_write(codec, MIC_BIAS_CNTL, 0x66);

	return ret;

pcm_err:
	kfree(codec->reg_cache);
	printk(KERN_INFO "aic3262.c : aic3262_init : function end error\n\n");
	return ret;

}

/*
 *----------------------------------------------------------------------------
 * Function : aic3262_remove
 * Purpose  : to remove aic3262 soc device
 *
 *----------------------------------------------------------------------------
 */
static int aic3262_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	/* power down chip */
	if (codec->control_data)
		aic3262_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
	kfree(snd_soc_codec_get_drvdata(codec));
	kfree(codec);

	return 0;
}


/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic3262_probe(), aic3262_remove(),
 *          aic3262_suspend() and aic3262_resume()
 *----------------------------------------------------------------------------
 */
struct snd_soc_codec_device soc_codec_dev_aic3262 = {
	.probe = aic3262_probe,
	.remove = aic3262_remove,
	.suspend = aic3262_suspend,
	.resume = aic3262_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_aic3262);


/*
 *----------------------------------------------------------------------------
 * Function : aic3262_i2c_remove
 * Purpose  : This function removes the i2c client and uninitializes
 *                              AIC3262 CODEC.
 *            NOTE:
 *            This function is called from i2c core
 *            If the i2c layer weren't so broken, we could pass this kind of
 *            data around
 *
 *----------------------------------------------------------------------------
 */

static int __exit aic3262_i2c_remove(struct i2c_client *i2c)
{
	put_device(&i2c->dev);
	return 0;
}

static const struct i2c_device_id tlv320aic3262_id[] = {
	{"tlv320aic3262", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tlv320aic3262_id);

static struct i2c_driver tlv320aic3262_i2c_driver = {
	.driver = {
		.name = "tlv320aic3262",
	},
	.probe = aic3262_codec_probe,
	.remove = __exit_p(aic3262_i2c_remove),
	.id_table = tlv320aic3262_id,
};

static int __init tlv320aic3262_modinit(void)
{
	int ret = i2c_add_driver(&tlv320aic3262_i2c_driver);

	return ret;

}

module_init(tlv320aic3262_modinit);

static void __exit tlv320aic3262_exit(void)
{
	i2c_del_driver(&tlv320aic3262_i2c_driver);

}

module_exit(tlv320aic3262_exit);

MODULE_DESCRIPTION("ASoC TLV320AIC3262 codec driver");
MODULE_AUTHOR("Barani Prashanth<gvbarani@mistralsolutions.com>");
MODULE_LICENSE("GPL");
