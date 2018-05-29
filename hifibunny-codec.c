/*
 * Driver for ES9028Q2M DAC controller
 *
 * Author: Satoru Kawase
 * Modified by: Jiang Li
 *      Copyright 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
//#include "hifibunny-codec.h"
//
#define VOLUME1			11557
#define VOLUME2			29772
#define GENERAL_SET		30974
#define INPUT_CONFIG	21133
#define DPLL			12135
#define M_MODE			22575
#define chipStatue		6201
static int hifibunny_codec_dac_mute(struct snd_soc_dai *dai, int mute);
/* hifibunny Q2M Codec Private Data */
struct hifibunny_codec_priv {
	struct regmap *regmap;
	unsigned int fmt;
};

/* hifibunny Q2M Default Register Value */
static const struct reg_default hifibunny_codec_reg_defaults[] = {
	{INPUT_CONFIG,0x8c},
	{GENERAL_SET,0x87},
	{51015,0x10},
	{M_MODE,0x02},
	{DPLL,0x9a},
	{42772,0x00},
	{VOLUME1,0xFF},
	{VOLUME2,0xFF},
	{13183,0x00},//Lch 2nd THD fine
	{16853,0x00},//Lch 2nd THD coarse
	{1300, 0x00},//Lch 3nd THD fine
	{17977,0x00},//Lch 3nd THD coarse
	{29577,0x00},
	{18362,0x00},
	{39786,0x00},
	{47915,0x00},
	{6100, 0x00},
	{22290,0x00},//Rch 2nd THD fine
	{2649, 0x00},//Rch 2nd THD coarse
	{38059,0x00},//Rch 3rd THD fine
	{57542,0x00},//Rch 3rd THD coarse
	{35091,0x01},//L/R sep. THD comp.
};

//A list of writeable reg
static bool hifibunny_codec_writeable(struct device *dev, unsigned int reg)
{
	return true;
}

//A list of readable reg
static bool hifibunny_codec_readable(struct device *dev, unsigned int reg)
{
	return true;
}

//A list of non-cache reg
static bool hifibunny_codec_volatile(struct device *dev, unsigned int reg)
{
	return true;
}


/* Volume Scale */
static const DECLARE_TLV_DB_SCALE(volume_tlv, -12750, 50, 1);

/* Filter Type */
static const char * const fir_filter_type_texts[] = {
	"Fast Roll-Off",
	"Slow Roll-Off",
	"Minimum Phase",
};
//Create the FIR filter control widget 
static SOC_ENUM_SINGLE_DECL(hifibunny_fir_filter_type_enum,
				GENERAL_SET, 5, fir_filter_type_texts);


/* IIR Filter Select */
static const char * const iir_filter_texts[] = {
	"47kHz",
	"50kHz",
	"60kHz",
	"70kHz",
};
//Create the IIR filter control widget 
static SOC_ENUM_SINGLE_DECL(hifibunny_iir_filter_enum,
				GENERAL_SET, 2, iir_filter_texts);


/* I2S / SPDIF Select */
static const char * const iis_spdif_sel_texts[] = {
	"I2S",
	"SPDIF",
	"reserved",
	"DSD",
};
//Create the input selection control widget 
static SOC_ENUM_SINGLE_DECL(hifibunny_iis_spdif_sel_enum,
				INPUT_CONFIG, 0, iis_spdif_sel_texts);

//THD compensation switch
static const char * const thd_enum_texts[] = 
{
	"Enable comp.",
	"Disable comp.",
};
//Create the THD compensation switch control widget 
static SOC_ENUM_SINGLE_DECL(hifibunny_thd_enum, 42772, 6, thd_enum_texts);

//R/L channel THD compensation seperately
static const char * const thd_ctrl_texts[] = 
{
	"Sep. comp.",
	"Non Sep. comp.",
};
static SOC_ENUM_SINGLE_DECL(hifibunny_thd_ctrl_enum, 35091, 0, thd_ctrl_texts);


static const char * const channel_mute_texts[] = 
{
	"Normal",
	"Mute Lch",
	"Mute Rch",
	"Mute both",
};
static SOC_ENUM_SINGLE_DECL(channel_mute_ctrl, GENERAL_SET, 0, channel_mute_texts);

// Register all amixer control widget
static const struct snd_kcontrol_new hifibunny_codec_controls[] = {
SOC_DOUBLE_R_TLV("Digital Playback Volume", VOLUME1, VOLUME2,
		 0, 255, 1, volume_tlv),
SOC_ENUM("Ch Mute", channel_mute_ctrl),
SOC_ENUM("FIR Filter", hifibunny_fir_filter_type_enum),
SOC_ENUM("IIR Filter", hifibunny_iir_filter_enum),
SOC_ENUM("input", hifibunny_iis_spdif_sel_enum),
SOC_SINGLE("Lch 2nd THD Coarse", 16853, 0, 255, 0),
SOC_SINGLE("LCh 2nd THD Fine", 13183, 0, 255, 0),
SOC_SINGLE("LCh 3rd THD Coarse", 17977, 0, 255, 0),
SOC_SINGLE("LCh 3rd THD Fine", 1300, 0, 255, 0),
SOC_SINGLE("RCh 2nd THD Coarse", 2649, 0, 255, 0),
SOC_SINGLE("RCh 2nd THD Fine", 22290, 0, 255, 0),
SOC_SINGLE("RCh 3rd THD Coarse", 57542, 0, 255, 0),
SOC_SINGLE("RCh 3rd THD Fine", 38059, 0, 255, 0),
};
static struct snd_soc_codec_driver hifibunny_codec_codec_driver = {
	.component_driver = {
		.controls         = hifibunny_codec_controls,
		.num_controls     = ARRAY_SIZE(hifibunny_codec_controls),
	}
};

//Support sampling rate list
static const u32 hifibunny_codec_dai_rates_slave[] = {
	8000, 11025, 16000, 22050, 32000,
	44100, 48000, 64000, 88200, 96000, 176400, 192000, 352800, 384000
};
//Register sampling rate list
static const struct snd_pcm_hw_constraint_list constraints_slave = {
	.list  = hifibunny_codec_dai_rates_slave,
	.count = ARRAY_SIZE(hifibunny_codec_dai_rates_slave),
};

//Codec startup helper
static int hifibunny_codec_dai_startup_slave(
		struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret;

	ret = snd_pcm_hw_constraint_list(substream->runtime,
			0, SNDRV_PCM_HW_PARAM_RATE, &constraints_slave);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to setup rates constraints: %d\n", ret);
	}

	return ret;
}
//Codec startup
static int hifibunny_codec_dai_startup(
		struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_soc_codec      * codec = dai->codec;
	struct hifibunny_codec_priv * hifibunny_codec
					= snd_soc_codec_get_drvdata(codec);
	hifibunny_codec_dac_mute(dai, 1);

	switch (hifibunny_codec->fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		return hifibunny_codec_dai_startup_slave(substream, dai);

	default:
		return (-EINVAL);
	}
}

//Codec shutdown
static void hifibunny_codec_dai_shutdown(struct snd_pcm_substream * substream, struct snd_soc_dai *dai)
{
	hifibunny_codec_dac_mute(dai, 1);
}

//Set audio format to codec
static int hifibunny_codec_hw_params(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	uint8_t iface = snd_soc_read(codec, INPUT_CONFIG) & 0x3f;
	switch (params_format(params)) {
		//16-bit
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x0;
			break;
		//24-bit
		case SNDRV_PCM_FORMAT_S24_LE:
			iface |= 0x80;
			break;
		//32-bit
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x80;
			break;
		default:
			return -EINVAL;
	}
	snd_soc_write(codec, INPUT_CONFIG, iface);
	//Change DPLL bandwidth based on the sampling rate
	switch(params_rate(params))
	{
		case 11025:
		case 22050:
		case 44100:
		case 88200:
			snd_soc_write(codec, DPLL, 0xFA); //bump up PLL BW to track RPI I2S jitter
			snd_soc_write(codec, M_MODE, 0x02);//default stop_div
			break;
		case 176400:
		case 352800:
			snd_soc_write(codec, DPLL, 0xFA);
			snd_soc_write(codec, M_MODE, 0x00);//Compensate for high FSR
			break;
		case 8000:
		case 16000:
		case 32000:
		case 48000:
		case 64000:
			snd_soc_write(codec, DPLL, 0x9A);//I2S jitter is less
			snd_soc_write(codec, M_MODE, 0x02);
			break;
		case 96000:
		case 192000:
		case 384000:
			snd_soc_write(codec, DPLL, 0x9A);
			snd_soc_write(codec, M_MODE, 0x00);
			break;
		default:
			snd_soc_write(codec, DPLL, 0x9A);
			snd_soc_write(codec, M_MODE, 0x02);
	}
	return 0;
}
//Set audio data format to I2S
static int hifibunny_codec_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec      *codec = dai->codec;
	struct hifibunny_codec_priv *hifibunny_codec
					= snd_soc_codec_get_drvdata(codec);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
	default:
		return (-EINVAL);
	}

	/* clock inversion */
	if ((fmt & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_NB_NF) {
		return (-EINVAL);
	}

	/* Set Audio Data Format */
	hifibunny_codec->fmt = fmt;

	return 0;
}

//Mute DAC
static int hifibunny_codec_dac_mute(struct snd_soc_dai *dai, int mute)
{
	uint8_t genSet = snd_soc_read(dai->codec, GENERAL_SET);
	if(mute)
	{
		snd_soc_write(dai->codec, GENERAL_SET, genSet | 0x03);
	}
	return 0;
}
//Unmute DAC
static int hifibunny_codec_dac_unmute(struct snd_soc_dai *dai)
{
	snd_soc_write(dai->codec, GENERAL_SET, genSet & 0xFC);
	return 0;
}

//Call back function that will execute during playback preparation 
static int hifibunny_codec_dai_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	hifibunny_codec_dac_unmute(dai);
	return 0;
}

//Call back function that will execute when a defined event happens
static int hifibunny_codec_dai_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	int ret = 0;
	switch(cmd)
	{
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			break;
		//Try to mute DAC when the user asks to stop
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			hifibunny_codec_dac_mute(dai, 1);
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}
//Register all the previous defined call back functions by their names.
static const struct snd_soc_dai_ops hifibunny_codec_dai_ops = {
	.startup      = hifibunny_codec_dai_startup,
	.hw_params    = hifibunny_codec_hw_params,
	.set_fmt      = hifibunny_codec_set_fmt,
	.digital_mute = hifibunny_codec_dac_mute,
	.shutdown = hifibunny_codec_dai_shutdown,
	.prepare = hifibunny_codec_dai_prepare,
	.trigger = hifibunny_codec_dai_trigger,
};
//Register HW info and related operations
static struct snd_soc_dai_driver hifibunny_codec_dai = {
	.name = "hifibunny-codec-dai",
	.playback = {
		.stream_name  = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats      = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
		    SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &hifibunny_codec_dai_ops,
};

//Internal register info
static const struct regmap_config hifibunny_codec_regmap = {
	//16-bit Address
	.reg_bits         = 16,
	//8-bit data
	.val_bits         = 8,
	.max_register     = 65535,
	//default register values
	.reg_defaults     = hifibunny_codec_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(hifibunny_codec_reg_defaults),

	.writeable_reg    = hifibunny_codec_writeable,
	.readable_reg     = hifibunny_codec_readable,
	.volatile_reg     = hifibunny_codec_volatile,

	.cache_type       = REGCACHE_RBTREE,
};

//Register codec driver
static int hifibunny_codec_probe(struct device *dev, struct regmap *regmap)
{
	struct hifibunny_codec_priv *hifibunny_codec;
	int ret;
	hifibunny_codec = devm_kzalloc(dev, sizeof(*hifibunny_codec), GFP_KERNEL);
	if (!hifibunny_codec) {
		dev_err(dev, "devm_kzalloc");
		return (-ENOMEM);
	}
	printk("Registering hifibunny-codec \n");
	hifibunny_codec->regmap = regmap;

	dev_set_drvdata(dev, hifibunny_codec);

	ret = snd_soc_register_codec(dev,
			&hifibunny_codec_codec_driver, &hifibunny_codec_dai, 1);
	if (ret != 0) {
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
		return ret;
	}
	return 0;
}
//Unregister codec driver
static void hifibunny_codec_remove(struct device *dev)
{
	snd_soc_unregister_codec(dev);
}

//Register i2c control 
static int hifibunny_codec_i2c_probe(
		struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct regmap *regmap;

	regmap = devm_regmap_init_i2c(i2c, &hifibunny_codec_regmap);
	if (IS_ERR(regmap)) {
		return PTR_ERR(regmap);
	}

	return hifibunny_codec_probe(&i2c->dev, regmap);
}
//Unregister i2c control 
static int hifibunny_codec_i2c_remove(struct i2c_client *i2c)
{
	hifibunny_codec_remove(&i2c->dev);

	return 0;
}

//id string used for machine driver matching
static const struct i2c_device_id hifibunny_codec_i2c_id[] = {
	{ "hifibunny-codec", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hifibunny_codec_i2c_id);
static const struct of_device_id hifibunny_codec_of_match[] = {
	{ .compatible = "tuxiong,hifibunny-codec", },
	{ }
};
MODULE_DEVICE_TABLE(of, hifibunny_codec_of_match);
static struct i2c_driver hifibunny_codec_i2c_driver = {
	.driver = {
		.name           = "hifibunny-codec-i2c",
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(hifibunny_codec_of_match),
	},
	.probe    = hifibunny_codec_i2c_probe,
	.remove   = hifibunny_codec_i2c_remove,
	.id_table = hifibunny_codec_i2c_id,
};
module_i2c_driver(hifibunny_codec_i2c_driver);


MODULE_DESCRIPTION("ASoC Hifibunny Q2M codec driver");
MODULE_AUTHOR("Jiang Li");
MODULE_LICENSE("GPL");
