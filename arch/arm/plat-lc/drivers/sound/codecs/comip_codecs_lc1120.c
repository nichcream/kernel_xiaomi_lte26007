/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2009  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the codec driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2012-02-04	tangyong	created
 *
 */

#include "comip_codecs_lc1120.h"
#include <plat/mfp.h>

#define CODEC_DEBUG 1


//#define LC1120_REGLIST_PRINT

#define OUT_SAMPLERATE_NUM 9+1

#define CONFIG_CODEC_LC1120_DEBUG_A
#ifdef CONFIG_CODEC_LC1120_DEBUG_A
#define CODEC_LC1120_PRINTKA(fmt, args...)  printk(KERN_DEBUG "[CODEC_LC1120]" fmt, ##args)
#else
#define CODEC_LC1120_PRINTKA(fmt, args...)
#endif
//#define CONFIG_CODEC_LC1120_DEBUG_B
#ifdef CONFIG_CODEC_LC1120_DEBUG_B
#define CODEC_LC1120_PRINTKB(fmt, args...)  printk(KERN_DEBUG "[CODEC_LC1120]" fmt, ##args)
#else
#define CODEC_LC1120_PRINTKB(fmt, args...)
#endif

#define CONFIG_CODEC_LC1120_DEBUG_C
#ifdef CONFIG_CODEC_LC1120_DEBUG_C
#define CODEC_LC1120_PRINTKC(fmt, args...)  printk(KERN_INFO "[CODEC_LC1120]" fmt, ##args)
#else
#define CODEC_LC1120_PRINTKC(fmt, args...)
#endif
#define CODEC_PCM_SEL   0x1
#define BT_PCM_SEL   	0x0

#define OUT_REC   		0x1
#define OUT_SPK   		0x2
#define OUT_HP   		0x3
#define OUT_ALL_MUTE   	0x4
#define OUT_SPK_HP   	0x5


/******************************************************************************
*  Private Function Declare
******************************************************************************/

/* In-data addresses are hard-coded into the reg-cache values */
static u8 comip_lc1120_reg[comip_1120_REGS_NUM] = {
	0x00, 0x10, 0x9f, 0x9f, 0x9e, 0x99, 0x99, 0x99, 0xb4, 0x00,
	0x62, 0x0a, 0x00, 0x80, 0x80, 0x62, 0x62, 0x00, 0x07, 0x23,
	0xe7, 0x67, 0x7f, 0x00, 0x7f, 0x00, 0x32, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x12, 0x80, 0x00, 0x00, 0x00, 0x07, 0x03, 0x67,
	0x00, 0x28, 0x00, 0x32, 0x00, 0x32, 0x04, 0x80, 0x00, 0x67,
	0x67, 0x00, 0x07, 0x23, 0x7f, 0x00, 0x7f, 0x00, 0x32, 0x00,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x10, 0x00, 0x00,
	0x00, 0x32, 0x66, 0xe9, 0x26, 0x31, 0xe9, 0x26, 0x31, 0x80,
	0x80, 0x19, 0x19, 0x33, 0x74, 0x12, 0x18, 0x06, 0x48, 0x30,
	0xc6, 0x30, 0xd4, 0x0f, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};


static const struct CODEC_I2C_DATA_T lc1120_init_data[]= {
	{LC1120_R89, 0x80, 0x80, 0x32}
	,{LC1120_R87, 0xf6, 0xff, 0x00}
	,{LC1120_R86, 0x40, 0xc0, 0x00}
	,{LC1120_R87, 0xf7, 0xff, 0x00}
	,{LC1120_R93, 0x80, 0xff, 0x00}
	,{LC1120_R86, 0xc0, 0xc0, 0x00}
	,{LC1120_R92, 0x08, 0x08, 0x00}//Class D dead time 140ms 1120bug
};

static const struct CODEC_I2C_DATA_T lc1120_deinit_data[]= {
	{LC1120_R01, 0x00, 0xee, 0x00}
	,{LC1120_R02, 0x00, 0x40, 0x00}
	,{LC1120_R03, 0x00, 0x40, 0x00}
	,{LC1120_R04, 0x00, 0x60, 0x00}
	,{LC1120_R05, 0x00, 0x60, 0x00}
	,{LC1120_R47, 0x00, 0x80, 0x00}
	,{LC1120_R81, 0x00, 0x20, 0x00}
	,{LC1120_R82, 0x00, 0x20, 0x00}
	,{LC1120_R83, 0x00, 0x10, 0x00}
	,{LC1120_R91, 0x00, 0x40, 0x00}
	,{LC1120_R93, 0x0f, 0x8f, 0x01}
	,{LC1120_R89, 0x00, 0x41, 0x00}
	,{LC1120_R89, 0x00, 0x10, 0x00}
	,{LC1120_R86, 0x00, 0xff, 0x00}
	,{LC1120_R90, 0x00, 0x42, 0x00}
	,{LC1120_R92, 0x00, 0x08, 0x00}//Class D dead time 140ms 1120bug
	,{LC1120_R09, 0x0c, 0x0c, 0x00}//disable Class D irq
};


//default samplate value 44.1k
static const struct CODEC_I2C_DATA_T lc1120_playback_data_first[]= {
	{LC1120_R81, 0x20, 0x20, 0x00}//pll1
	,{LC1120_R79, 0x08, 0x08, 0x00}//pll1
	,{LC1120_R76, 0xd4, 0xff, 0x00}//pll1
	,{LC1120_R77, 0x8b, 0xff, 0x00}//pll1
	,{LC1120_R78, 0xf2, 0xff, 0x00}//pll1
	,{LC1120_R72, 0x06, 0x0f, 0x00}//pll1
	,{LC1120_R79, 0x92, 0xf7, 0x00}//pll1
	,{LC1120_R81, 0x18, 0x5c, 0x00}//pll1 44.1k
	,{LC1120_R10, 0x41, 0x7f, 0x00}//i2s
	,{LC1120_R11, 0x01, 0x3f, 0x00}//i2s
//	,{LC1120_R12, 0x00, 0xff, 0x00}//i2s

};

static const struct CODEC_I2C_DATA_T lc1120_playback_data_second[]= {
	{LC1120_R46, 0x04, 0xfc, 0x00}//dac
	,{LC1120_R71, 0x00, 0x0f, 0x00}//dac
	,{LC1120_R84, 0x60, 0xe0, 0x00}//ictr_dac
	,{LC1120_R01, 0xc0, 0xc0, 0x00}//DAC open
};

static const struct CODEC_I2C_DATA_T lc1120_playback_data_nopll_first[]= {
	{LC1120_R10, 0x61, 0x7f, 0x00}//i2s,from mclk
	,{LC1120_R11, 0x01, 0x3f, 0x00}//i2s
//	,{LC1120_R12, 0x00, 0xff, 0x00}//i2s
};

static const struct CODEC_I2C_DATA_T lc1120_playback_data_nopll_second[]= {
	{LC1120_R46, 0x04, 0xfc, 0x00}//dac
	,{LC1120_R71, 0x02, 0x0f, 0x00}//dac,from mclk
	,{LC1120_R84, 0x60, 0xe0, 0x00}//ictr_dac
	,{LC1120_R01, 0xc0, 0xc0, 0x00}//DAC open
};


static const struct CODEC_I2C_DATA_T lc1120_capture_data_first[]= {
	{LC1120_R81, 0x20, 0x20, 0x00}//pll1
	,{LC1120_R79, 0x08, 0x08, 0x00}//pll1
	,{LC1120_R76, 0xd4, 0xff, 0x00}//pll1
	,{LC1120_R77, 0x8b, 0xff, 0x00}//pll1
	,{LC1120_R78, 0xf2, 0xff, 0x00}//pll1
	,{LC1120_R72, 0x06, 0x0f, 0x00}//pll1
	,{LC1120_R79, 0x92, 0xf7, 0x00}//pll1
	,{LC1120_R81, 0x18, 0x5c, 0x00}//pll1 44.1k
	,{LC1120_R10, 0x41, 0x7f, 0x00}//i2s
	,{LC1120_R11, 0x01, 0x3f, 0x00}//i2s
	,{LC1120_R12, 0x00, 0xff, 0x00}//i2s
};

static const struct CODEC_I2C_DATA_T lc1120_capture_data_second[]= {
	{LC1120_R45, 0x00, 0x03, 0x00}//adc
	,{LC1120_R45, 0x30, 0x3c, 0x00}//adc
	,{LC1120_R83, 0x2b, 0xff, 0x00}//ADC open
	,{LC1120_R39, 0x6a, 0x7f, 0x00}//adc vol 0db
	,{LC1120_R40, 0x08, 0x3f, 0x00}//mic pga 10db
	,{LC1120_R01, 0x28, 0x38, 0x00}//micpga
	//,{LC1120_R85, 0x16, 0x1f, 0x00}//hp mic
	,{LC1120_R85, 0x03, 0x1f, 0x00}//main mic
	,{LC1120_R84, 0x16, 0x1e, 0x00}//micbias
};

static const struct CODEC_I2C_DATA_T lc1120_capture_data_nopll_first[]= {
	{LC1120_R10, 0x61, 0x7f, 0x00}//i2s,from mclk
	,{LC1120_R11, 0x01, 0x3f, 0x00}//i2s
	,{LC1120_R12, 0x00, 0xff, 0x00}//i2s
};

static const struct CODEC_I2C_DATA_T lc1120_capture_data_nopll_second[]= {
	{LC1120_R45, 0x02, 0x03, 0x00}//adc,from mclk
	,{LC1120_R45, 0x30, 0x3c, 0x00}//adc
	,{LC1120_R83, 0x2b, 0xff, 0x00}//ADC open
	,{LC1120_R39, 0x6a, 0x7f, 0x00}//adc vol 0db
	,{LC1120_R40, 0x08, 0x3f, 0x00}//mic pga 10db
	,{LC1120_R01, 0x28, 0x38, 0x00}//micpga
	,{LC1120_R85, 0x03, 0x1f, 0x00}//main mic
	,{LC1120_R84, 0x16, 0x1e, 0x00}//micbias
};
static const struct CODEC_I2C_DATA_T lc1120_capture_data_pdm_second[]= {
	{LC1120_R12, 0x90, 0xff, 0x00}//i2s sel from pdm L
	,{LC1120_R17, 0x30, 0xff, 0x00}//PDM HP enable
	,{LC1120_R32, 0x02, 0x0f, 0x00}//pdm sel i2s sync ,sel mclk
	,{LC1120_R20, 0x00, 0x80, 0x00}//64X over samplerate
	,{LC1120_R19, 0x80, 0x80, 0x00}//PDM falling edge
};
static const struct CODEC_I2C_DATA_T lc1120_capture_dmic_input_headset_data[]={
	{LC1120_R45, 0x02, 0x03, 0x00}//adc,from mclk
	,{LC1120_R45, 0x30, 0x3c, 0x00}//adc
	,{LC1120_R83, 0x2b, 0xff, 0x00}//ADC open
	,{LC1120_R39, 0x6a, 0x7f, 0x00}//adc vol 0db
	,{LC1120_R40, 0x08, 0x3f, 0x00}//mic pga 10db
	,{LC1120_R12, 0x00, 0xff, 0x00}//i2s
	,{LC1120_R01, 0x28, 0x38, 0x00}//micpga
	,{LC1120_R85, 0x03, 0x1f, 0x00}//main mic
	,{LC1120_R84, 0x16, 0x1e, 0x00}//micbias
	,{LC1120_R85, 0x14, 0x1f, 0x00}
	,{LC1120_R01, 0x28, 0x38, 0x00}//main MIC close
};

static const struct CODEC_I2C_DATA_T lc1120_voice_pcm_slave_data[]= {
	{LC1120_R73, 0x33, 0xff, 0x00}//pll2
	,{LC1120_R74, 0x1c, 0xff, 0x00}//pll2
	,{LC1120_R75, 0x15, 0xff, 0x00}//pll2
	,{LC1120_R72, 0xa0, 0xf0, 0x00}//pll2
	,{LC1120_R80, 0x9d, 0x07, 0x00}//pll2 8k
	,{LC1120_R82, 0x20, 0x20, 0x00}//pll2
	,{LC1120_R82, 0x19, 0x5c, 0x00}//pll2 config
	,{LC1120_R39, 0x00, 0xff, 0x00}//mute adc
	,{LC1120_R46, 0x90, 0xff, 0x00}//dac
	,{LC1120_R84, 0x60, 0xe0, 0x00}//vmic
	,{LC1120_R83, 0x2b, 0xff, 0x00}//ADC open
	,{LC1120_R01, 0x28, 0x38, 0x00}//micpga
	,{LC1120_R84, 0x16, 0x1e, 0x00}//main mic
	,{LC1120_R01, 0xe8, 0xe8, 0x00}
	,{LC1120_R14, 0x80, 0xff, 0x00}//pcm2
	,{LC1120_R16, 0x51, 0xff, 0x00}//pcm2 pll2
	,{LC1120_R35, 0xc0, 0xff, 0x00}//adc
	,{LC1120_R38, 0xe3, 0xff, 0x00}//adc
	,{LC1120_R45, 0x39, 0x3f, 0x00}//adc pll2
	,{LC1120_R71, 0x39, 0xff, 0x00}//dac pll2
};

static const struct CODEC_I2C_DATA_T lc1120_voice_dmic_pcm_slave_double_mics_data[]= {
	{LC1120_R73, 0x33, 0xff, 0x00}//pll2  // ACTION  0x3a
	,{LC1120_R74, 0x1c, 0xff, 0x00}//pll2
	,{LC1120_R75, 0x15, 0xff, 0x00}//pll2
	,{LC1120_R72, 0xa0, 0xf0, 0x00}//pll2
	,{LC1120_R80, 0x9d, 0x07, 0x00}//pll2 8k
	,{LC1120_R82, 0x20, 0x20, 0x00}//pll2
	,{LC1120_R82, 0x19, 0x5c, 0x00}//pll2 config
	,{LC1120_R46, 0x90, 0xff, 0x00}//dac sel from pcm2 left channel
	,{LC1120_R01, 0xc0, 0xc0, 0x00} //enable dac
	,{LC1120_R16, 0x51, 0xff, 0x00}//pcm2 pll2 ??
	,{LC1120_R71, 0x39, 0xff, 0x00}//dac pll2
	,{LC1120_R14, 0xd8, 0xff, 0x00}//pcm2 sel dmic
	,{LC1120_R32, 0x09, 0x0f, 0x00}//pdm sel pcm2 sync ,sel pll2
};
static const struct CODEC_I2C_DATA_T lc1120_voice_dmic_pcm_slave_single_mic_data[]= {
	{LC1120_R73, 0x33, 0xff, 0x00}//pll2  // ACTION  0x3a
	,{LC1120_R74, 0x1c, 0xff, 0x00}//pll2
	,{LC1120_R75, 0x15, 0xff, 0x00}//pll2
	,{LC1120_R72, 0xa0, 0xf0, 0x00}//pll2
	,{LC1120_R80, 0x9d, 0x07, 0x00}//pll2 8k
	,{LC1120_R82, 0x20, 0x20, 0x00}//pll2
	,{LC1120_R82, 0x19, 0x5c, 0x00}//pll2 config
	,{LC1120_R46, 0x90, 0xff, 0x00}//dac sel from pcm2 left channel
	,{LC1120_R01, 0xc0, 0xc0, 0x00} //enable dac
	,{LC1120_R16, 0x51, 0xff, 0x00}//pcm2 pll2 ??
	,{LC1120_R71, 0x39, 0xff, 0x00}//dac pll2
	,{LC1120_R14, 0xd8, 0xff, 0x00}//pcm2 sel dmic
	,{LC1120_R32, 0x09, 0x0f, 0x00}//pdm sel pcm2 sync ,sel pll2
};



static const struct CODEC_I2C_DATA_T lc1120_voice_pcm_sel_pdm_data[]= {
	 {LC1120_R84, 0x60, 0xe0, 0x00}//vmic   ?????del
	,{LC1120_R83, 0x23, 0xff, 0x00}//ADC open  //or 0x03
	,{LC1120_R84, 0x16, 0x1e, 0x00}//main mic  ?
//	,{LC1120_R01, 0xc0, 0xc0, 0x00} //enable dac
	//,{LC1120_R16, 0x53, 0xff, 0x00}//pcm2 pll2 ??
	,{LC1120_R17, 0x30, 0xff, 0x00}//PDM HP enable
	,{LC1120_R20, 0x80, 0x80, 0x00}//pdm over samplerate set 128
};

static const struct CODEC_I2C_DATA_T lc1120_voice_pcm_sel_adc_headset_data[]= {
	{LC1120_R84, 0x60, 0xe0, 0x00}//vmic
	,{LC1120_R83, 0x2b, 0xff, 0x00}//ADC open
	,{LC1120_R84, 0x16, 0x1e, 0x00}//main mic
	,{LC1120_R01, 0x38, 0x38, 0x00} //enable adc pga
	,{LC1120_R14, 0x80, 0xff, 0x00}//pcm2
	,{LC1120_R35, 0xc0, 0xff, 0x00}//adc
	,{LC1120_R38, 0xe3, 0xff, 0x00}//adc
	,{LC1120_R45, 0x39, 0x3f, 0x00}//adc pll2
	,{LC1120_R01, 0x00, 0x10, 0x00}
	,{LC1120_R85, 0x14, 0x1f, 0x00}//hp mic
};

static const struct CODEC_I2C_DATA_T lc1120_voice_pcm_slave_data_nopll[]= {
	{LC1120_R46, 0x90, 0xff, 0x00}//dac
	,{LC1120_R84, 0x60, 0xe0, 0x00}//vmic
	,{LC1120_R83, 0x2b, 0xff, 0x00}//ADC open
	,{LC1120_R01, 0x28, 0x38, 0x00}//micpga
	,{LC1120_R84, 0x16, 0x1e, 0x00}//main mic
	,{LC1120_R01, 0xe8, 0xe8, 0x00}
	,{LC1120_R14, 0x80, 0xff, 0x00}//pcm2
	,{LC1120_R16, 0x61, 0xff, 0x00}//pcm2,from mclk
	,{LC1120_R35, 0xc0, 0xff, 0x00}//adc
	,{LC1120_R38, 0xe3, 0xff, 0x00}//adc
	,{LC1120_R45, 0x3a, 0x3f, 0x00}//adc,from mclk
	,{LC1120_R71, 0x3a, 0x3f, 0x00}//dac,from mclk
};


static const struct CODEC_I2C_DATA_T lc1120_sleep_data[]= {
	{LC1120_R84, 0x03,0x03,0x00}
};

static const struct CODEC_I2C_DATA_T lc1120_resume_data[]= {
	{LC1120_R84, 0x03,0x03,0x00}
};
static struct CODEC_I2C_DATA_T lc1120_rec_output_path_set_data[]= {
	{LC1120_R91, 0x50, 0x70, 0x32}
	,{LC1120_R05, 0x60, 0xe0, 0x00}//REC
};
static struct CODEC_I2C_DATA_T lc1120_spk_output_path_set_data[]= {
	{LC1120_R66, 0x07, 0x07, 0x00}
	,{LC1120_R95, 0x04, 0x04, 0x00}
	,{LC1120_R89, 0x46, 0x66, 0x00}
	,{LC1120_R08,  0x40, 0xc0, 0x00}
	,{LC1120_R93, 0x00, 0x7f, 0x00}//SPK
};
static struct CODEC_I2C_DATA_T lc1120_spk_aux_output_path_set_data[]= {
	{LC1120_R66, 0x07, 0x07, 0x00}
	,{LC1120_R95, 0x04, 0x04, 0x00}
	,{LC1120_R89, 0x46, 0x66, 0x00}
	,{LC1120_R08,  0x40, 0xc0, 0x00}
	,{LC1120_R93, 0x00, 0x7f, 0x00}//SPK
	,{LC1120_R86, 0x23, 0x33, 0x00}
};

static struct CODEC_I2C_DATA_T lc1120_aux_output_path_set_data[]= {
	{LC1120_R86, 0x23, 0x33, 0x00}
};

static struct CODEC_I2C_DATA_T lc1120_hp_output_path_open_data[]= {
	{LC1120_R07, 0x80, 0x80, 0x00}
	,{LC1120_R06, 0x80, 0x80, 0x00}
	,{LC1120_R06, 0x00, 0x40, 0x00}
	,{LC1120_R07, 0x00, 0x40, 0x00}
	,{LC1120_R06, 0x20, 0x20, 0x00}//channl:mute cross powerOn
	,{LC1120_R07, 0x20, 0x20, 0x00}//channl:mute cross powerOn
	,{LC1120_R89, 0x01, 0x01, 0x00}
	,{LC1120_R90, 0x8c, 0x8c, 0x00} //mixer: mute mute enalbe //ce to 8c
	,{LC1120_R90, 0x00, 0x84, 0x00}//mixer: Umute
	,{LC1120_R90, 0x21, 0x21, 0x00} //dac sel hp
	,{LC1120_R06, 0x00, 0x80, 0x00} //channl: Umute
	,{LC1120_R07, 0x00, 0x80, 0x00}//channl: Umute
};

static struct CODEC_I2C_DATA_T lc1120_hp_output_path_close_data[]= {
	{LC1120_R90, 0x00, 0x84, 0x00}//mixer: Umute
	,{LC1120_R07, 0x00, 0x80, 0x00}//channl: Umute
	,{LC1120_R06, 0x00, 0x80, 0x00} //channl: Umute
	,{LC1120_R90, 0x21, 0x21, 0x00} //dac sel hp
	,{LC1120_R90, 0x8c, 0x8c, 0x00} //mixer: mute mute enalbe //ce to 8c
	,{LC1120_R07, 0x60, 0x60, 0x00}//channl:mute cross powerOn
	,{LC1120_R06, 0x60, 0x60, 0x00} //channl:mute cross powerOn
	,{LC1120_R89, 0x01, 0x01, 0x00}
};



static const struct CODEC_I2C_DATA_T lc1120_fm_on_data[]= {
	{LC1120_R02, 0x40, 0x40, 0x00} //LINLMU LINLZCEN
	,{LC1120_R01, 0x02, 0x02, 0x00}
	,{LC1120_R03, 0x40, 0x40, 0x00} //LINRMU LINRZCEN
	,{LC1120_R01, 0x04, 0x04, 0x00}
	,{LC1120_R89, 0x18, 0x18, 0x00}//SPK-line
	,{LC1120_R90, 0x40, 0x40, 0x00}//HPL
	,{LC1120_R90, 0x02, 0x02, 0x00}//HPL
	,{LC1120_R02, 0x1f, 0x3f, 0x00}//HPL
	,{LC1120_R03, 0x1f, 0x3f, 0x00}//HPL
	,{LC1120_R86, 0x0c, 0x0c, 0x00}//AUX
	,{LC1120_R02, 0x00, 0x80, 0x00} //LINL uMU
	,{LC1120_R03, 0x00, 0x80, 0x00} //LINR uMU

};

static const struct CODEC_I2C_DATA_T lc1120_fm_off_data[]= {
	{LC1120_R02, 0x80, 0x80, 0x00} //LINL uMU
	,{LC1120_R03, 0x80, 0x80, 0x00} //LINR uMU
	,{LC1120_R02, 0x00, 0x40, 0x00} //LINLMU LINLZCEN
	,{LC1120_R01, 0x00, 0x02, 0x00}
	,{LC1120_R03, 0x00, 0x40, 0x00} //LINRMU LINRZCEN
	,{LC1120_R01, 0x00, 0x04, 0x00}
	,{LC1120_R89, 0x00, 0x18, 0x00}//SPK-line
	,{LC1120_R90, 0x00, 0x40, 0x00}//HPL
	,{LC1120_R90, 0x00, 0x02, 0x00}//HPL
	,{LC1120_R86, 0x00, 0x0c, 0x00}//AUX
};

static const struct CODEC_I2C_DATA_T lc1120_input_path_set_data[]= {
	{LC1120_R85, 0x00, 0x1f, 0x00}
	,{LC1120_R01, 0x00, 0x38, 0x00}
};

static u8  lc1120_input_path_table[2][2]= {
	{0x03, 0x28}//main MIC open
	,{0x14, 0x28}//main MIC close
};

static const struct CODEC_I2C_DATA_T lc1120_frist_output_gain_set_data[]= {
	{LC1120_R49, 0x00, 0x7f, 0x00}//DAC_LEVEL_L
	,{LC1120_R50, 0x00, 0x7f, 0x00}//DAC_LEVEL_R
};
static const struct CODEC_I2C_DATA_T lc1120_left_pdm_gain_set_data[]= {
	{LC1120_R20, 0x00, 0x7f, 0x00}
};
static const struct CODEC_I2C_DATA_T lc1120_right_pdm_gain_set_data[]= {
	{LC1120_R21, 0x00, 0x7f, 0x00}
};

static const struct CODEC_I2C_DATA_T lc1120_second_output_gain_set_data[]= {
	{LC1120_R05, 0x00, 0x1f, 0x00}//REC
	,{LC1120_R08, 0x00, 0x1f, 0x00}//SPK
	,{LC1120_R06, 0x00, 0x1f, 0x00}
	,{LC1120_R07, 0x00, 0x1f, 0x00}//HP
};

static const struct CODEC_I2C_DATA_T lc1120_fm_output_gain_set_data[]= {
	{LC1120_R02, 0x00, 0x3f, 0x00}//LINLVOL
	,{LC1120_R03, 0x00, 0x3f, 0x00}//LINRVOL
};

static const struct CODEC_I2C_DATA_T lc1120_fm_output_mute_set_data[]= {
	{LC1120_R02, 0x80, 0x80, 0x00}//LINLVOL
	,{LC1120_R03, 0x80, 0x80, 0x00}//LINRVOL
};

static const struct CODEC_I2C_DATA_T lc1120_frist_input_gain_set_data[]= {
	{LC1120_R40, 0x00,0x3f,0x00}
	,{LC1120_R20, 0x00, 0x7f, 0x00} // pdm left
};

static const struct CODEC_I2C_DATA_T lc1120_second_input_gain_set_data[]= {
	{LC1120_R39, 0x00,0x7f,0x00}
	,{LC1120_R21, 0x00, 0x7f, 0x00} // pdm right
};

static const struct CODEC_I2C_DATA_T lc1120_output_mute_set_data[]= {
	{LC1120_R01, 0xc0, 0xc0, 0x00}//disable dac
};

static const struct CODEC_I2C_DATA_T lc1120_output_samplerate_set_data[]= {
	{LC1120_R76, 0x00, 0xff, 0x00}
	,{LC1120_R77, 0x00, 0xff, 0x00}
	,{LC1120_R78, 0x00, 0xff, 0x00}
	,{LC1120_R72, 0x00, 0x0f, 0x00}
	,{LC1120_R79, 0x00, 0x07, 0x00}
};

static const u8  lc1120_output_sample_table[10][5]= {
	{0x33, 0x1c, 0x15, 0x0a, 0x07}//8k
	,{0xd4, 0x8b, 0xf2, 0x06, 0x04}//11.025k
	,{0x26, 0xd5, 0x8f, 0x07, 0x04}//12k
	,{0x33, 0x1c, 0x15, 0x0a, 0x04}//16k
	,{0xd4, 0x8b, 0xf2, 0x06, 0x03}//22.05k
	,{0x00, 0x00, 0x00, 0x00, 0x00}//NONE
	,{0x26, 0xd5, 0x8f, 0x07, 0x03}//24k
	,{0x33, 0x1c, 0x15, 0x0a, 0x03}//32k
	,{0xd4, 0x8b, 0xf2, 0x06, 0x02}//44.1k
	,{0x26, 0xd5, 0x8f, 0x07, 0x02}//48k
};

static const struct CODEC_I2C_DATA_T lc1120_input_samplerate_set_data[]= {
	{LC1120_R76, 0x00, 0xff, 0x00}
	,{LC1120_R77, 0x00, 0xff, 0x00}
	,{LC1120_R15, 0x00, 0xff, 0x00}
	,{LC1120_R72, 0x00, 0x0f, 0x00}
	,{LC1120_R79, 0x00, 0x07, 0x00}
};

//check it
static const u8  lc1120_input_sample_table[2][5]= {
	{0x00,	0x00,	0x00,	0x00,	0x00}
	,{0x00,	0x00,	0x00,	0x00,	0x00}
};

static const struct CODEC_I2C_DATA_T lc1120_hp_detpol_set_data[]= {
	{LC1120_R85, 0x20, 0x20, 0x00}//JACKDET low indicate Jack insertion
};

static const struct snd_kcontrol_new lc1120_snd_controls[] = {
	SOC_SINGLE("Master Playback Volume", 0, 0, 0x3F, 1),//sample
};

struct comip_lc1120_st_priv {
	struct clk *mclk;
	unsigned long freq;
	unsigned int clk_st;
	int main_clk_st;
	int comip_codec_clk_freq;
	int comip_codec_audio_st;
	int comip_codec_jackst;
	int outdevice;
	struct mutex mutex;
	int nType_audio;
	int type_play;
	int type_record;
	int type_voice;
	int fm;
	int nPath_out;
	int nPath_in;
	int nrecord_input_gain;
	int nplay_output_gain;
	unsigned char adc_regval;
	int pdm_flag;
	int spk_use_aux;
	int (*comip_switch_isr)(int type);
	int irq_gpio;
	int irq;
};
struct comip_1120_priv* pcodec_pri_data = NULL;
struct comip_lc1120_st_priv* comip_lc1120_st_pri_data = NULL;
static int comip_clk_init_st = 0;



/*
 * codec mclk set,audio mclk=13mhz,voice mclk=2mhz
 */
#define LC1120_MCLK_ALLON

#define LC1120_MCLK_RATE_AUDIO				11289600
//#define LC1120_MCLK_RATE_AUDIO				13000000
#define LC1120_MCLK_RATE_AUDIO_AND_VOICE	13000000

//#define LC1120_VOICE_USE_2048000
#ifdef LC1120_VOICE_USE_2048000
#define LC1120_MCLK_RATE_VOICE				2048000
#else
#define LC1120_MCLK_RATE_VOICE				13000000
#endif


int comip_lc1120_st_init(void)
{
	int err=0;
	comip_lc1120_st_pri_data = kzalloc(sizeof(struct comip_lc1120_st_priv), GFP_KERNEL);
	if (comip_lc1120_st_pri_data == NULL) {
		CODEC_LC1120_PRINTKC( "comip_lc1120_st_init error %d\n",-ENOMEM);
		return -ENOMEM;
	}
	comip_lc1120_st_pri_data->clk_st= 0;
	comip_lc1120_st_pri_data->main_clk_st= 0;
	comip_lc1120_st_pri_data->comip_codec_clk_freq= 0;
	comip_lc1120_st_pri_data->comip_codec_audio_st= 0;
	comip_lc1120_st_pri_data->comip_codec_jackst= 0;
	comip_lc1120_st_pri_data->nType_audio = AUDIO_TYPE;
	comip_lc1120_st_pri_data->nPath_out = DD_SPEAKER;
	comip_lc1120_st_pri_data->nPath_in = DD_MIC;
	comip_lc1120_st_pri_data->fm = 0;
	comip_lc1120_st_pri_data->type_play = 0;
	comip_lc1120_st_pri_data->type_record = 0;
	comip_lc1120_st_pri_data->type_voice = 0;
	comip_lc1120_st_pri_data->adc_regval = 0;
	comip_lc1120_st_pri_data->pdm_flag = 0;

	comip_lc1120_st_pri_data->mclk = clk_get(NULL, "clkout3_clk");
	if (IS_ERR(comip_lc1120_st_pri_data->mclk)) {
		CODEC_LC1120_PRINTKC( "[SND CODEC] Cannot get codec input clock\n");
		err = PTR_ERR(comip_lc1120_st_pri_data->mclk);
		goto out;
	}
	mutex_init(&comip_lc1120_st_pri_data->mutex);
	return 0;

out:
	kfree(comip_lc1120_st_pri_data);
	return err;
}

int lc1120_clk_enable(void)
{
	int err = 0;
	if(comip_clk_init_st == 0) {
		err = comip_lc1120_st_init();
		if(err < 0)
			return -1;
		else
		comip_clk_init_st = 1;
	}

	mutex_lock(&comip_lc1120_st_pri_data->mutex);
	if(comip_lc1120_st_pri_data->type_voice&&(comip_lc1120_st_pri_data->type_play||comip_lc1120_st_pri_data->type_record))
		comip_lc1120_st_pri_data->freq = LC1120_MCLK_RATE_AUDIO_AND_VOICE;
	else if(comip_lc1120_st_pri_data->type_voice)
		comip_lc1120_st_pri_data->freq = LC1120_MCLK_RATE_VOICE;		
	else if(comip_lc1120_st_pri_data->type_play|comip_lc1120_st_pri_data->type_record)
		comip_lc1120_st_pri_data->freq = LC1120_MCLK_RATE_AUDIO;	
	else
		comip_lc1120_st_pri_data->freq = LC1120_MCLK_RATE_AUDIO;

	if(comip_lc1120_st_pri_data->comip_codec_clk_freq != comip_lc1120_st_pri_data->freq) {
		err = clk_set_rate(comip_lc1120_st_pri_data->mclk, comip_lc1120_st_pri_data->freq);
		if (err < 0) {
			CODEC_LC1120_PRINTKC( "lc1120_clk_set_rate error %d\n",err);
			return err;
		}
		comip_lc1120_st_pri_data->comip_codec_clk_freq = comip_lc1120_st_pri_data->freq;
		CODEC_LC1120_PRINTKA("lc1120_clk_enable clk_set_rate freq: %d\n",(unsigned int)comip_lc1120_st_pri_data->freq);

	}

	if(comip_lc1120_st_pri_data->main_clk_st == 0){
		CODEC_LC1120_PRINTKB("lc1120_clk_enable clk_enable freq: %d\n",(unsigned int)comip_lc1120_st_pri_data->freq);

		err = clk_enable(comip_lc1120_st_pri_data->mclk);
		if (err < 0) {
			CODEC_LC1120_PRINTKC( "lc1120_clk_enable error %d\n",err);
			return err;
		}
		mdelay(5);

	}
#ifdef LC1120_MCLK_ALLON
	comip_lc1120_st_pri_data->main_clk_st = 1;
#else
	comip_lc1120_st_pri_data->main_clk_st ++;
#endif
	CODEC_LC1120_PRINTKB("lc1120_clk_enable main_clk_st: %d\n",comip_lc1120_st_pri_data->main_clk_st);

	mutex_unlock(&comip_lc1120_st_pri_data->mutex);

	return 0;

}


int lc1120_clk_disable(void)
{
	if(comip_clk_init_st == 0)
		return -1;

	mutex_lock(&comip_lc1120_st_pri_data->mutex);

#ifdef LC1120_MCLK_ALLON
	//comip_lc1120_st_pri_data->main_clk_st = 1;
#else
	comip_lc1120_st_pri_data->main_clk_st --;
#endif

	CODEC_LC1120_PRINTKB("lc1120_clk_disable main_clk_st: %d\n",comip_lc1120_st_pri_data->main_clk_st);

	if((comip_lc1120_st_pri_data->main_clk_st == 0)||(comip_lc1120_st_pri_data->main_clk_st < 0)) {
		CODEC_LC1120_PRINTKB("lc1120_clk_disable clk_disable\n");

		clk_disable(comip_lc1120_st_pri_data->mclk);
		//comip_lc1120_st_pri_data->comip_codec_clk_freq = 0;
	}

	mutex_unlock(&comip_lc1120_st_pri_data->mutex);
	return 0;
}

static void dump_reg(void)
{
	u8 nReg =0;
	u8 nData=0;
	lc1120_clk_enable();
	for(nReg=0; nReg<comip_1120_REGS_NUM; nReg++) {
		nData = 0;
		nData = comip_1120_i2c_read(nReg);
		CODEC_LC1120_PRINTKC("comip_1120 reg: %d 0x%x=0x%x\n",nReg,nReg,nData);

	}
	lc1120_clk_disable();
}

/*
 * The codec has no support for reading its registers except for peak level...
 */
static inline unsigned int comip_1120_read_reg_cache(struct snd_soc_codec *codec,
                u8 reg)
{
	u8 *cache = codec->reg_cache;

	if (reg >= comip_1120_REGS_NUM)
		return -1;
	return cache[reg];
}

/*
 * Write the register cache
 */
static inline void comip_1120_write_reg_cache(struct snd_soc_codec *codec,
                u8 reg, unsigned int value)
{
	u8 *cache = codec->reg_cache;

	if (reg >= comip_1120_REGS_NUM)
		return;
	cache[reg] = value;
}


int comip_lc1120_i2c_read(u8 reg)
{
	int ret;
	ret = comip_1120_i2c_read(reg);
	return ret;
}

int comip_lc1120_i2c_write(u8 reg,u8 value)
{
	int ret;
	ret = comip_1120_i2c_write(value,reg);
	return ret;
}


/*
 * Write to the lc1120 registers
 *
 */
static int comip_1120_write(struct snd_soc_codec *codec, unsigned int reg,
                            unsigned int value)
{
	int ret=0;
	CODEC_LC1120_PRINTKC("%s reg: %02X, value:%02X\n", __func__, reg, value);

	if (reg >= comip_1120_REGS_NUM) {
		CODEC_LC1120_PRINTKC( "%s unknown register: reg: %u",__func__, reg);
		return -EINVAL;
	}

	if(codec == NULL) {
		CODEC_LC1120_PRINTKC( "unknown codec");
		return 0;
	}

//	comip_1120_write_reg_cache(codec, reg, value);

	ret = comip_1120_i2c_write((u8)value, (u8)reg);
	if (ret != 0)
		return -EIO;

	return 0;
}

static int lc1120_init(void)
{
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int i=0;
	int nLength=0;
	int nRst =0;
	u8 nData=0;

	CODEC_LC1120_PRINTKA("lc1120_init\n");
	//lc1120_codec_output_path_set(4);
	nLength =  ARRAY_SIZE(lc1120_init_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_init_data;
	lc1120_clk_enable();
	for( i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);

		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;

		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_init: write reg error\n");
		}
	}
	lc1120_clk_disable();

	return 0;
}
static int lc1120_deinit(void)
{
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int i=0;
	int nLength=0;
	int nRst =0;
	u8 nData=0;

	CODEC_LC1120_PRINTKB("lc1120_deinit\n");
	lc1120_clk_enable();

	nLength =  ARRAY_SIZE(lc1120_deinit_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_deinit_data;

	for( i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_deinit: write reg error\n");
		}
	}

	return 0;
}

static int lc1120_playback_second(void)
{
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int i=0;
	int nLength=0;
	int nRst =0;
	u8 nData=0;

	CODEC_LC1120_PRINTKB("lc1120_playback_second\n");
	//nLength =  ARRAY_SIZE(lc1120_playback_data_second);
	//pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_playback_data_second;
	nLength =  ARRAY_SIZE(lc1120_playback_data_nopll_second);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_playback_data_nopll_second;

	for( i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_playback_second: write reg error\n");
		}
	}

	return 0;
}

static int lc1120_capture_second(void)
{
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int i=0;
	int nLength=0;
	int nRst =0;
	u8 nData=0;

	CODEC_LC1120_PRINTKB("lc1120_capture_second\n");
	//nLength =  ARRAY_SIZE(lc1120_capture_data_second);
	//pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_capture_data_second;
	if((comip_lc1120_st_pri_data->pdm_flag)&&(comip_lc1120_st_pri_data->nPath_in == DD_MIC) ){
		nLength =  ARRAY_SIZE(lc1120_capture_data_pdm_second);
		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_capture_data_pdm_second;
	} else {
		nLength =  ARRAY_SIZE(lc1120_capture_data_nopll_second);
		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_capture_data_nopll_second;
	}
	for( i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_capture_second: write reg error\n");
		}
	}

	return 0;
}
static int lc1120_capture_dmic_input_headset(void)
{
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int i=0;
	int nLength=0;
	int nRst =0;
	u8 nData=0;

	CODEC_LC1120_PRINTKB("lc1120_capture_dmic_input_headset\n");

	nLength =  ARRAY_SIZE(lc1120_capture_dmic_input_headset_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_capture_dmic_input_headset_data;
	for( i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_capture_dmic_input_headset: write reg error\n");
		}
	}

	return 0;
}



int lc1120_codec_open(int nType)
{
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int i=0;
	int nLength=0;
	int nRst =0;
	u8 data=0;

 	CODEC_LC1120_PRINTKB("lc1120_codec_open nType=%d\n",nType);
	if(nType == VOICE_TYPE) { //voice type
		if(comip_lc1120_st_pri_data->pdm_flag == DMIC_BOARD_DOUBLE_MICS_MODE) {
			pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_voice_dmic_pcm_slave_double_mics_data;
			nLength = ARRAY_SIZE(lc1120_voice_dmic_pcm_slave_double_mics_data);
		}else if(comip_lc1120_st_pri_data->pdm_flag == DMIC_BOARD_SINGLE_MIC_MODE) {
			pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_voice_dmic_pcm_slave_single_mic_data;
			nLength = ARRAY_SIZE(lc1120_voice_dmic_pcm_slave_single_mic_data);
		}else {
			if((comip_lc1120_st_pri_data->type_play==0)&&(comip_lc1120_st_pri_data->type_record==0)){
#ifdef	LC1120_VOICE_USE_2048000
				pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_voice_pcm_slave_data_nopll;
				nLength = ARRAY_SIZE(lc1120_voice_pcm_slave_data_nopll);
#else
				pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_voice_pcm_slave_data;
				nLength = ARRAY_SIZE(lc1120_voice_pcm_slave_data);
#endif
			}else {
				pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_voice_pcm_slave_data;
				nLength = ARRAY_SIZE(lc1120_voice_pcm_slave_data);
				//lc1120_codec_input_mute_set(1);
			}
		}
	} else if(nType == AUDIO_TYPE) {
		if(comip_lc1120_st_pri_data->type_voice==0){
			pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_playback_data_nopll_first;
			nLength = ARRAY_SIZE(lc1120_playback_data_nopll_first);
		}else {
			pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_playback_data_first;
			nLength = ARRAY_SIZE(lc1120_playback_data_first);
		}
#if 0
	} else if(nType == FM_TYPE) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_fm_data;
		nLength = ARRAY_SIZE(lc1120_fm_data);
#endif
	} else if(nType ==AUDIO_CAPTURE_TYPE) {
		if(comip_lc1120_st_pri_data->type_voice==0){
			pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_capture_data_nopll_first;
			nLength = ARRAY_SIZE(lc1120_capture_data_nopll_first);
		}else {
			pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_capture_data_first;
			nLength = ARRAY_SIZE(lc1120_capture_data_first);
		}
	} else {
		nRst=-1;//unsupported device type
		goto out;
	}

	for( i=0; i<nLength; i++) {
		nRst= comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			data=0;
			return nRst;
		} else
			data = nRst;
		if(pInitHandle[i].nmask) {
			data &= (~pInitHandle[i].nmask) ;
		}
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, pInitHandle[i].nvalue|data);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_open: write reg error\n");
		}
	}
	//lc1120_codec_output_mute_set(0);
	nRst =0;
out:
	return nRst;
}

static int lc1120_codec_hook_enable(int onoff)
{
	unsigned char codec_reg;
	lc1120_clk_enable();

	if(onoff==1) {
		comip_lc1120_st_pri_data->comip_codec_jackst = 1;

		codec_reg = comip_lc1120_i2c_read(LC1120_R89);
		codec_reg = codec_reg | 0x80;
		comip_lc1120_i2c_write(LC1120_R89, codec_reg);
	
		codec_reg = comip_lc1120_i2c_read(LC1120_R86);
		codec_reg = codec_reg | 0xc0;
		comip_lc1120_i2c_write(LC1120_R86, codec_reg);
		
		codec_reg = comip_lc1120_i2c_read(LC1120_R87);
		codec_reg = codec_reg | 0xf1;
		comip_lc1120_i2c_write(LC1120_R87, codec_reg);

	} else {
		comip_lc1120_st_pri_data->comip_codec_jackst = 0;

		if((comip_lc1120_st_pri_data->type_record == 0)&&(comip_lc1120_st_pri_data->type_voice == 0)\
			&&(comip_lc1120_st_pri_data->type_play == 0)&&(comip_lc1120_st_pri_data->fm == 0)) {
			codec_reg = comip_lc1120_i2c_read(LC1120_R89);
			codec_reg = codec_reg & 0x7f;
			comip_lc1120_i2c_write(LC1120_R89, codec_reg);

			codec_reg = comip_lc1120_i2c_read(LC1120_R86);
			codec_reg = codec_reg & 0x3f;
			comip_lc1120_i2c_write(LC1120_R86, codec_reg);
			
			codec_reg = comip_lc1120_i2c_read(LC1120_R87);
			codec_reg = codec_reg & 0x0e;
			comip_lc1120_i2c_write(LC1120_R87, codec_reg);
		}
	}
	codec_reg = comip_lc1120_i2c_read(LC1120_R01);
	if(onoff==1) {
		codec_reg = codec_reg | 0x01;
	} else {
		codec_reg = codec_reg & 0xfe;
	}
	comip_lc1120_i2c_write(LC1120_R01, codec_reg);
	codec_reg = comip_lc1120_i2c_read(LC1120_R84);
	if(onoff==1) {
		codec_reg = codec_reg | 0x3;
	} else {
		codec_reg = codec_reg & 0xfe;
	}
	comip_lc1120_i2c_write(LC1120_R84, codec_reg);
	codec_reg = comip_lc1120_i2c_read(LC1120_R91);
	if(onoff==1) {
		codec_reg = codec_reg | 0x3;
	} else {
		codec_reg = codec_reg & 0xfc;
	}
	comip_lc1120_i2c_write(LC1120_R91, codec_reg);
	lc1120_clk_disable();
	return 0;
}

int lc1120_codec_fm_enable(int nOnOff)
{
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int i=0;
	int nLength=0;
	int nRst =0;
	u8 data=0;

	if(nOnOff == 1) {

		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_fm_on_data;
		nLength = ARRAY_SIZE(lc1120_fm_on_data);
	} else {

		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_fm_off_data;
		nLength = ARRAY_SIZE(lc1120_fm_off_data);
	}

	for( i=0; i<nLength; i++) {
		nRst= comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			data=0;
			return nRst;
		} else
			data = nRst;
		if(pInitHandle[i].nmask) {
			data &= (~pInitHandle[i].nmask) ;
		}
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, pInitHandle[i].nvalue|data);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_codec_fm_enable: write reg error\n");
		}
	}

	return nRst;
}

int lc1120_codec_set_output_mute(int nMute)
{
	u8 nData=0;
	int nRst =0;

	CODEC_LC1120_PRINTKB("lc1120_codec_set_output_mute,nMute=%d,outdevice=0x%x\n",nMute,comip_lc1120_st_pri_data->outdevice);

	if(nMute){
		if(comip_lc1120_st_pri_data->outdevice & 0x4){// hp
			nData = comip_lc1120_i2c_read(LC1120_R06);
			nData |= 0x80;
			comip_lc1120_i2c_write(LC1120_R06,nData);
			comip_lc1120_i2c_write(LC1120_R07,nData);
		}
		if(comip_lc1120_st_pri_data->outdevice & 0x1){//REC
			nData = comip_lc1120_i2c_read(LC1120_R05);
			nData |= 0x80;
			comip_lc1120_i2c_write(LC1120_R05,nData);
		}
		if(comip_lc1120_st_pri_data->outdevice & 0x2 ){//SPK
			if(comip_lc1120_st_pri_data->spk_use_aux) {
				gpio_direction_output(comip_lc1120_st_pri_data->spk_use_aux,0);
			} else {
				nData = comip_lc1120_i2c_read(LC1120_R08);
				nData |= 0x80;
				comip_lc1120_i2c_write(LC1120_R08,nData);
			}
		}
	}else{
		if(comip_lc1120_st_pri_data->outdevice & 0x4){
				nData = comip_lc1120_i2c_read(LC1120_R06);
				nData &= 0x7f;
				comip_lc1120_i2c_write(LC1120_R06,nData);
				comip_lc1120_i2c_write(LC1120_R07,nData);
		}
		if(comip_lc1120_st_pri_data->outdevice & 0x1){
				nData = comip_lc1120_i2c_read(LC1120_R05);
				nData &= 0x7f;
				comip_lc1120_i2c_write(LC1120_R05,nData);
		}
		if(comip_lc1120_st_pri_data->outdevice & 0x2){
			if(comip_lc1120_st_pri_data->spk_use_aux){
				gpio_direction_output(comip_lc1120_st_pri_data->spk_use_aux,1);
				mdelay(15);
			} else {
				nData = comip_lc1120_i2c_read(LC1120_R08);
				nData &= 0x7f;
				comip_lc1120_i2c_write(LC1120_R08,nData);
			}
		}
		}

	nRst =0;
	return nRst;
}
int lc1120_codec_set_mp_output_mute(int nMute)
{  
	u8 nData=0;
	int nRst =0;

	CODEC_LC1120_PRINTKB("lc1120_codec_set_mp_output_mute enter nMute=%d,nPath_out=%d \n",nMute,comip_lc1120_st_pri_data->nPath_out);
	if(nMute){
		lc1120_codec_hook_enable(0);
        if(comip_lc1120_st_pri_data->nPath_out == DD_HEADPHONE){
		nData =comip_lc1120_i2c_read(LC1120_R06);
		nData &= 0x5f;
		nData |= 0x80;
		nRst =comip_lc1120_i2c_write(LC1120_R06, nData);

		nData =comip_lc1120_i2c_read(LC1120_R07);
		nData &=0x5f;
		nData |= 0x80;
		nRst =comip_lc1120_i2c_write(LC1120_R07, nData);

		nData =comip_lc1120_i2c_read(LC1120_R46);
		nData &=0xfc;
		nData |=0x03;
		nRst =comip_lc1120_i2c_write(LC1120_R46, nData);

		nData =comip_lc1120_i2c_read(LC1120_R01);
		nData &=0x3f;
		nRst =comip_lc1120_i2c_write(LC1120_R01, nData);

		nData =comip_lc1120_i2c_read(LC1120_R89);
		nData &=0x7f;
		nRst =comip_lc1120_i2c_write(LC1120_R89, nData);

        }else if(comip_lc1120_st_pri_data->nPath_out == DD_SPEAKER){
	 	nData =comip_lc1120_i2c_read(LC1120_R08);
		nData &=0x7f;	  
	      	nData |= 0x80;
		nRst =comip_lc1120_i2c_write(LC1120_R08, nData);	
		
		nData =comip_lc1120_i2c_read(LC1120_R93);
		nData &=0xf0;	  
		nData |= 0x0f;
		nRst =comip_lc1120_i2c_write(LC1120_R93, nData);

		nData =comip_lc1120_i2c_read(LC1120_R46);
		nData &=0xfc;
		nData |=0x03;
		nRst =comip_lc1120_i2c_write(LC1120_R46, nData);

		nData =comip_lc1120_i2c_read(LC1120_R01);
		nData &=0x3f;
		nRst =comip_lc1120_i2c_write(LC1120_R01, nData);

		nData =comip_lc1120_i2c_read(LC1120_R89);
		nData |=0x20;
		nData &=0x3f;  
		nRst =comip_lc1120_i2c_write(LC1120_R89, nData);
		if(comip_lc1120_st_pri_data->spk_use_aux) {
			nData =comip_lc1120_i2c_read(LC1120_R86);
			nData &=0xcf;
			nData |= 0x10;
			nRst =comip_lc1120_i2c_write(LC1120_R86, nData);
			gpio_direction_output(comip_lc1120_st_pri_data->spk_use_aux,0);
		}
        	}
	}else{
		if(comip_lc1120_st_pri_data->nPath_out == DD_HEADPHONE){
		nData =comip_lc1120_i2c_read(LC1120_R89);
		nData &= 0x7f;
		nData |= 0x80;
		nRst =comip_lc1120_i2c_write(LC1120_R89, nData);

		nData =comip_lc1120_i2c_read(LC1120_R01);
		nData |=0xc0;
		nRst =comip_lc1120_i2c_write(LC1120_R01, nData);

		nData =comip_lc1120_i2c_read(LC1120_R46);
		nData &=0xfc;
		nRst =comip_lc1120_i2c_write(LC1120_R46, nData);

		nData =comip_lc1120_i2c_read(LC1120_R06);
		nData &=0x7f;
		nData |= 0x20;
		nRst =comip_lc1120_i2c_write(LC1120_R06, nData);

		nData =comip_lc1120_i2c_read(LC1120_R07);
		nData &=0x7f;
		nData |= 0x20;
		nRst =comip_lc1120_i2c_write(LC1120_R07, nData);

		msleep(50);
		lc1120_codec_hook_enable(1);
		}else if(comip_lc1120_st_pri_data->nPath_out == DD_SPEAKER){
		nData =comip_lc1120_i2c_read(LC1120_R89);
		nData &= 0x1f;
		nData |= 0xc0;
		nRst =comip_lc1120_i2c_write(LC1120_R89, nData);

		nData =comip_lc1120_i2c_read(LC1120_R01);
		nData |=0xc0;
		nRst =comip_lc1120_i2c_write(LC1120_R01, nData);

		nData =comip_lc1120_i2c_read(LC1120_R46);
		nData &=0xfc;
		nRst =comip_lc1120_i2c_write(LC1120_R46, nData);

		nData =comip_lc1120_i2c_read(LC1120_R93);
		nData &=0xf0;	  
		nRst =comip_lc1120_i2c_write(LC1120_R93, nData);

		nData =comip_lc1120_i2c_read(LC1120_R08);
		nData &=0x7f;	 
		nRst =comip_lc1120_i2c_write(LC1120_R08, nData);

		if(comip_lc1120_st_pri_data->spk_use_aux) {
			nData =comip_lc1120_i2c_read(LC1120_R86);
			nData &=0xcf;
			nData |= 0x20;
			nRst =comip_lc1120_i2c_write(LC1120_R86, nData);
			gpio_direction_output(comip_lc1120_st_pri_data->spk_use_aux,1);
			mdelay(15);
		}
	    }

	}

	nRst =0;
	return nRst;
}
int lc1120_codec_fm_output_mute_set(int nMute)
{
	u8 nData=0;
	int nRst =0;
	u8 i=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;


	CODEC_LC1120_PRINTKB("lc1120_codec_output_mute_set,nMute=%d\n",nMute);
	nLength = ARRAY_SIZE(lc1120_fm_output_mute_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_fm_output_mute_set_data;

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		nData &= (~pInitHandle[i].nmask);

		if(nMute) {
			nData |= pInitHandle[i].nvalue;
		} else {
			nData &= (~pInitHandle[i].nvalue);
		}
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_output_mute_set: write reg error\n");
		}
	}
	nRst =0;
	return nRst;
}

int lc1120_codec_rec_output_set(u8 pathOpen)
{
	u8 nData=0;
	int nRst =0;
	u8 i=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;
	if((pathOpen && (comip_lc1120_st_pri_data->outdevice & 0x1)) || ((!pathOpen) && (!(comip_lc1120_st_pri_data->outdevice &0x1)))){
		return 0;
	}

	nLength =  ARRAY_SIZE(lc1120_rec_output_path_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_rec_output_path_set_data;

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;

		if(pathOpen) {
			nData &=(~pInitHandle[i].nmask);
			nData |=(pInitHandle[i].nvalue);
		} else { //pathClose
			nData |=(pInitHandle[i].nmask);
			nData &=(~pInitHandle[i].nvalue);
		}

		nRst = comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_rec_output_set: write reg error\n");
		}

	}
	if(pathOpen)
		comip_lc1120_st_pri_data->outdevice |= 0x1;
	else
		comip_lc1120_st_pri_data->outdevice &=0x6;
	return nRst;
}

int lc1120_codec_spk_output_set(u8 pathOpen)
{
	u8 nData=0;
	int nRst =0;
	u8 i=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	if((pathOpen && (comip_lc1120_st_pri_data->outdevice & 0x2)) || ((!pathOpen) && (!(comip_lc1120_st_pri_data->outdevice &0x2)))){
		return 0;
	}

	if(comip_lc1120_st_pri_data->spk_use_aux) {
		nLength =  ARRAY_SIZE(lc1120_aux_output_path_set_data);
		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_aux_output_path_set_data;
		gpio_direction_output(comip_lc1120_st_pri_data->spk_use_aux,pathOpen);
		if(pathOpen)
			mdelay(15);
	} else {
		nLength =  ARRAY_SIZE(lc1120_spk_output_path_set_data);
		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_spk_output_path_set_data;
	}
	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;

		if(pathOpen) {
			nData &=(~pInitHandle[i].nmask);
			nData |=(pInitHandle[i].nvalue);
		} else { //pathClose
			nData |=(pInitHandle[i].nmask);
			nData &=(~pInitHandle[i].nvalue);
		}

		nRst = comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_spk_output_set: write reg error\n");
		}
	}
	if(pathOpen)
		comip_lc1120_st_pri_data->outdevice |= 0x2;
	else
		comip_lc1120_st_pri_data->outdevice &= 0x5;
	return nRst;
}
int lc1120_codec_hp_output_set(u8 pathOpen)
{
	u8 nData=0;
	int nRst =0;
	u8 i=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	if((pathOpen && (comip_lc1120_st_pri_data->outdevice & 0x4)) || ((!pathOpen) && (!(comip_lc1120_st_pri_data->outdevice &0x4)))){
		return 0;
	}

	if(pathOpen) {
		nLength =  ARRAY_SIZE(lc1120_hp_output_path_open_data);
		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_hp_output_path_open_data;
		comip_lc1120_st_pri_data->outdevice |= 0x4;
	} else { //pathClose
		comip_lc1120_st_pri_data->outdevice &= 0x3;
		nLength =  ARRAY_SIZE(lc1120_hp_output_path_close_data);
		pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_hp_output_path_close_data;
	}

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;

		if(pathOpen) {
			nData &=(~pInitHandle[i].nmask);
			nData |=(pInitHandle[i].nvalue);
		} else { //pathClose
			nData |=(pInitHandle[i].nmask);
			nData &=(~pInitHandle[i].nvalue);
		}

		nRst = comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_hp_output_set: write reg error\n");
		}

	}
	if(comip_lc1120_st_pri_data->fm && pathOpen) {
		nData = comip_lc1120_i2c_read(LC1120_R90);
		nData = nData|0x42;
		comip_lc1120_i2c_write(LC1120_R90, nData);
	}
	return nRst;
}

int lc1120_codec_output_path_set(int nPath)
{
	u8 pathClose = 0;
	u8 pathOpen = 1;
	
	if(nPath == DD_RECEIVER) {
	    lc1120_codec_spk_output_set(pathClose);
		lc1120_codec_hp_output_set(pathClose);
		lc1120_codec_rec_output_set(pathOpen);
	} else if(nPath == DD_SPEAKER) {
		lc1120_codec_rec_output_set(pathClose);
		lc1120_codec_hp_output_set(pathClose);
		lc1120_codec_spk_output_set(pathOpen);
	} else if(nPath == DD_HEADPHONE) {
		lc1120_codec_rec_output_set(pathClose);
		lc1120_codec_spk_output_set( pathClose);
		lc1120_codec_hp_output_set( pathOpen);
	} else if(nPath == DD_SPEAKER_HEADPHONE) {
		lc1120_codec_rec_output_set( pathClose);
		lc1120_codec_spk_output_set( pathOpen);
		lc1120_codec_hp_output_set( pathOpen);
	} else if(nPath == DD_BLUETOOTH) {
		lc1120_codec_rec_output_set( pathClose);
		lc1120_codec_spk_output_set( pathClose);
		lc1120_codec_hp_output_set( pathClose);
	} else {
		CODEC_LC1120_PRINTKC("lc1120_codec_output_path_set: error output path = %d\n",nPath);
		return 0;
	}
	CODEC_LC1120_PRINTKC("lc1120_codec_output_path_set: output path = %d\n",nPath);


	return 0;

}

int lc1120_codec_frist_output_gain_set(int nGain)
{
	int nRst =0;
	u8 nData=0;
	u8 i=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_frist_output_gain_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_frist_output_gain_set_data;

	if (nGain == DD_MIXER_AUDIO_OUTPUT_MIN) {
		goto out;
	}

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask) ;
		}

		nData |=((u8)( nGain*LC1120_FRIST_OUTPUT_GAIN_RANGE/ (LC1120_FRIST_OUTPUT_GAIN_MAX))\
		        + LC1120_FRIST_OUTPUT_GAIN_MIN)& pInitHandle[i].nmask;

		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_frist_output_gain_set: write reg error\n");
		}

	}

	nRst =0;

out:
	return nRst;
}

int lc1120_codec_second_output_gain_set(int nGain)
{
	int nRst =0;
	u8 nData=0;
	u8 i=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	//if (nGain == DD_MIXER_AUDIO_OUTPUT_MIN) {
	//	goto out;
	//}

	nLength = ARRAY_SIZE(lc1120_second_output_gain_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_second_output_gain_set_data;

	if(comip_lc1120_st_pri_data->nPath_out == DD_RECEIVER) {
		nData = comip_lc1120_i2c_read(LC1120_R05);
		nData &= 0xbf;
		comip_lc1120_i2c_write(LC1120_R05,nData);
		udelay(10);
		nData |= 0x40;
		comip_lc1120_i2c_write(LC1120_R05,nData);
		udelay(10);
	} else if(comip_lc1120_st_pri_data->nPath_out == DD_SPEAKER) {
		nData = comip_lc1120_i2c_read(LC1120_R08);
		nData &= 0xbf;
		comip_lc1120_i2c_write(LC1120_R08,nData);
		udelay(10);
		nData |= 0x40;
		comip_lc1120_i2c_write(LC1120_R08,nData);
		udelay(10);
	}else if(comip_lc1120_st_pri_data->nPath_out == DD_HEADPHONE){
		nData = comip_lc1120_i2c_read(LC1120_R06);
		nData &= 0xbf;
		comip_lc1120_i2c_write(LC1120_R06,nData);
		comip_lc1120_i2c_write(LC1120_R07,nData);
		udelay(10);
		nData |= 0x40;
		comip_lc1120_i2c_write(LC1120_R06,nData);
		comip_lc1120_i2c_write(LC1120_R07,nData);
		udelay(10);
	}else{
        CODEC_LC1120_PRINTKC("lc1120_codec_second_output_gain_set output_path  nPath_out=%d\n",comip_lc1120_st_pri_data->nPath_out);
	}

	for(i=0; i<nLength; i++) {

		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask) ;
		}
		nData |=((u8)( nGain*LC1120_SECOND_OUTPUT_GAIN_RANGE / (LC1120_SECOND_OUTPUT_GAIN_MAX))\
		        + LC1120_SECOND_OUTPUT_GAIN_MIN)& pInitHandle[i].nmask;

		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_second_output_gain_set: write reg error\n");
		}
	}

	return nRst;
}

int lc1120_codec_fm_output_gain_set(int nGain)
{
	int nRst =0;
	u8 nData=0;
	u8 i=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_fm_output_gain_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_fm_output_gain_set_data;

	//nGain = ( (nGain>>((lc1120_gain_parameter.nvolume_level-1)*0x8))&0xff);

	if (nGain == DD_MIXER_AUDIO_OUTPUT_MIN) {
		nRst = lc1120_codec_fm_output_mute_set(1);
		goto out;
	}

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask) ;
		}
		nData |=((u8)( nGain*LC1120_FM_OUTPUT_GAIN_RANGE/ (LC1120_FM_OUTPUT_GAIN_MAX))\
		        + LC1120_FM_OUTPUT_GAIN_MIN)& pInitHandle[i].nmask;

		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_second_output_gain_set: write reg error\n");
		}

	}

	lc1120_codec_fm_output_mute_set(0);

	nRst =0;

out:
	return nRst;
}

int lc1120_codec_output_gain_set(int nType,int nPath,int nGain)
{
	u8 frist_output_gain, second_output_gain;
	frist_output_gain = (nGain&0xff);
	second_output_gain = ((nGain >> 0x8)&0xff);

	CODEC_LC1120_PRINTKB("lc1120_codec_set_output_gain,nGain=0x%x,frist_output_gain=0x%x,second_output_gain=0x%x\n",
		nGain,frist_output_gain,second_output_gain);

	if(nType == FM_TYPE) {
		//lc1120_codec_fm_output_gain_set(frist_output_gain);
	} else {
		lc1120_codec_frist_output_gain_set(frist_output_gain);
	}

	lc1120_codec_second_output_gain_set(second_output_gain);
	return 0;
}

int lc1120_codec_frist_input_gain_set(int nGain)
{
	int nRst =0;
	u8 i=0;
	u8 nData=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_frist_input_gain_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_frist_input_gain_set_data;

	//if (nGain == (u8)DD_MIXER_AUDIO_INPUT_MIN) {
	//	goto out;
	//}
	lc1120_clk_enable();
	nData = comip_lc1120_i2c_read(LC1120_R01);
	nData &= 0xdf;
	comip_lc1120_i2c_write(LC1120_R01,nData);
	udelay(10);
	nData |= 0x20;
	comip_lc1120_i2c_write(LC1120_R01,nData);
	udelay(10);
	for(i=0; i<nLength; i++) {

		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask) ;
		}
		if(i == 0) {
			nData |=((u8)( nGain*LC1120_FRIST_INPUT_GAIN_RANGE/LC1120_FRIST_INPUT_GAIN_MAX)\
		        + LC1120_FRIST_INPUT_GAIN_MIN)& pInitHandle[i].nmask;
		}else {
			nData |=((u8)( nGain*LC1120_SECOND_INPUT_GAIN_RANGE/LC1120_SECOND_INPUT_GAIN_MAX)\
		        + LC1120_SECOND_INPUT_GAIN_MIN)&pInitHandle[i].nmask;
		}

		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_frist_input_gain_set: write reg error\n");
		}

	}

	return nRst;
}

int lc1120_codec_second_input_gain_set(int nGain)
{
	int nRst =0;
	u8 i=0;
	u8 nData=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_second_input_gain_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_second_input_gain_set_data;

	//if (nGain == (u8)DD_MIXER_AUDIO_INPUT_MIN) {
	//	goto out;
	//}

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask) ;
		}
		nData |=((u8)( nGain*LC1120_SECOND_INPUT_GAIN_RANGE/LC1120_SECOND_INPUT_GAIN_MAX)\
		        + LC1120_SECOND_INPUT_GAIN_MIN)&pInitHandle[i].nmask;

		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_second_input_gain_set: write reg error\n");
		}
	}

	return nRst;

}


int lc1120_codec_input_gain_set(int nGain)
{
	u8 frist_input_gain, second_input_gain;
	frist_input_gain = (nGain&0xff);
	second_input_gain = ((nGain >> 0x8)&0xff);
	CODEC_LC1120_PRINTKB("lc1120_codec_set_input_gain,nGain=0x%x,frist_input_gain=0x%x,second_input_gain=0x%x\n",
		nGain,frist_input_gain,second_input_gain);
	lc1120_codec_frist_input_gain_set(frist_input_gain);
	lc1120_codec_second_input_gain_set(second_input_gain);
	return 0;
}

int lc1120_codec_output_samplerate_set(int nSamplerate)
{
	int nRst=0;
	u8 nData=0;
	u8 i=0;
	u8 nindex=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;
	nLength = ARRAY_SIZE(lc1120_output_samplerate_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_output_samplerate_set_data;

	for(i=0; i<OUT_SAMPLERATE_NUM; i++) {
		if(!(nSamplerate&(0x01<<i))) {
			continue;
		} else {
			nindex=i;
			break;
		}
	}
	nindex = 8;

	for(i=0; i<nLength; i++) {

		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask);
		}

		nData |=lc1120_output_sample_table[nindex][i];
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_output_samplerate_set: write reg error\n");
		}

	}

	return nRst;
}


int lc1120_codec_input_samplerate_set(int nSamplerate)
{

	int nRst=0;
#if 0
	u8 nData=0;
	u8 i=0;
	u8 nindex=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_input_samplerate_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_input_samplerate_set_data;

	for(i=0; i<2; i++) {
		if(!(nSamplerate&(0x01<<i))) {
			continue;
		} else {
			nindex=i;
			break;
		}
	}
	nindex = 0;

	for(i=0; i<nLength; i++) {

		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask);
		}

		nData |=lc1120_input_sample_table[nindex][i];
		nRst =comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_set_input_samplerate: write reg error\n");
		}

	}
#endif
	return nRst;
}

int lc1120_codec_set_input_mute(int nValue)
{
	int result=0;

	if(nValue == 1) {
		comip_lc1120_st_pri_data->adc_regval = comip_lc1120_i2c_read(LC1120_R39);
		comip_lc1120_i2c_write(LC1120_R39, 0x0);

	}else {

		comip_lc1120_i2c_write(LC1120_R39, comip_lc1120_st_pri_data->adc_regval);

	}
	return result;

}
int lc1120_codec_input_dmic_pdm_set(void)
{
	int nRst =0;
	u8 i=0;
	u8 nData=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_voice_pcm_sel_pdm_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_voice_pcm_sel_pdm_data;

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_codec_input_dmic_pdm_set read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask);
		}
		nRst = comip_lc1120_i2c_write(pInitHandle[i].nreg, pInitHandle[i].nvalue|nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_input_dmic_pdm_set: write reg error\n");
		}
	}

}
int lc1120_codec_input_dmic_headset_set(void)
{
	int nRst =0;
	u8 i=0;
	u8 nData=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_voice_pcm_sel_adc_headset_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_voice_pcm_sel_adc_headset_data;

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_codec_input_dmic_headset_set read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask);
		}
		nRst = comip_lc1120_i2c_write(pInitHandle[i].nreg,  pInitHandle[i].nvalue|nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_input_dmic_headset_set: write reg error\n");
		}
	}

}


int lc1120_codec_input_path_set(int nPath)
{
	int nRst =0;
	u8 i=0;
	u8 nData=0;
	struct CODEC_I2C_DATA_T *pInitHandle =NULL;
	int nLength=0;

	nLength = ARRAY_SIZE(lc1120_input_path_set_data);
	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_input_path_set_data;

	for(i=0; i<nLength; i++) {

		nRst = comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "comip_lc1120_i2c_read read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData &= (~pInitHandle[i].nmask);
		}

		nData |=lc1120_input_path_table[nPath-1][i];
		nRst = comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC("lc1120_codec_input_path_set: write reg error\n");
		}
	}

	nRst =0;
	return nRst;
}
int lc1120_codec_dac_disable(void)
{
	unsigned char codec_reg;
	codec_reg = comip_lc1120_i2c_read(LC1120_R01);
	codec_reg = codec_reg & 0x3f;  // dac disable
	comip_lc1120_i2c_write(LC1120_R01, codec_reg);
}
int lc1120_codec_adc_disable(void)
{
	unsigned char codec_reg;
	codec_reg = comip_lc1120_i2c_read(LC1120_R83);
	codec_reg = codec_reg & 0xf7;  // ADC disable
	comip_lc1120_i2c_write(LC1120_R83, codec_reg);
}

int lc1120_codec_close(void)
{
	unsigned char codec_reg;
	
	CODEC_LC1120_PRINTKA(" lc1120_codec_close,jackst %d\n",comip_lc1120_st_pri_data->comip_codec_jackst);
	lc1120_codec_set_output_mute(1);
	lc1120_codec_output_path_set(OUT_ALL_MUTE);
	lc1120_deinit();
	if(comip_lc1120_st_pri_data->comip_codec_jackst == 0) {
		CODEC_LC1120_PRINTKB(" lc1120_codec_close R89 LDO disable\n");
		codec_reg = comip_lc1120_i2c_read(LC1120_R84);
		codec_reg = codec_reg & 0xfc;
		comip_lc1120_i2c_write(LC1120_R84, codec_reg);

		//codec_reg = comip_lc1120_i2c_read(LC1120_R86);
		//codec_reg = 0x10;
		comip_lc1120_i2c_write(LC1120_R86, 0x10);

		codec_reg = comip_lc1120_i2c_read(LC1120_R87);
		codec_reg = codec_reg & 0x0e;
		comip_lc1120_i2c_write(LC1120_R87, codec_reg);
		codec_reg = comip_lc1120_i2c_read(LC1120_R89);
		codec_reg = codec_reg & 0x7f;
		comip_lc1120_i2c_write(LC1120_R89, codec_reg);


	}

	return 0;
}
int lc1120_codec_hp_detpol_set(int detpol)
{
	unsigned char nData = 0;
	unsigned char i = 0;
	int nRst = 0;
	int nLength = 0;
	struct CODEC_I2C_DATA_T *pInitHandle = NULL;

	pInitHandle = (struct CODEC_I2C_DATA_T *)lc1120_hp_detpol_set_data;
	nLength = ARRAY_SIZE(lc1120_hp_detpol_set_data);
	lc1120_clk_enable();
	for(i=0; i<nLength; i++) {
		nRst= comip_lc1120_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_codec_hp_detpol_set read (0x%x)failed \n",pInitHandle[i].nreg);
			nData=0;
		} else
			nData = nRst;
		nData &= (~pInitHandle[i].nmask);
		if( GPIO_LEVEL_HIGH == detpol) {
			nData &= (~pInitHandle[i].nvalue);
		} else if( GPIO_LEVEL_LOW == detpol) {
			nData |= pInitHandle[i].nvalue;
		} else {
			CODEC_LC1120_PRINTKC( "lc1120_codec_hp_detpol_set: polarity error\n");
		}
		nRst = comip_lc1120_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1120_PRINTKC( "lc1120_codec_hp_detpol_set: write reg error\n");
		}

	}
	lc1120_clk_disable();
	return nRst;
}

int lc1120_codec_hp_detpol_get(void)
{
	unsigned char codec_reg;
	int lc1120_jack_pol;

	codec_reg = comip_lc1120_i2c_read(LC1120_R85);

	if(codec_reg & 0x20) {
		lc1120_jack_pol = GPIO_LEVEL_LOW;
	} else {
		lc1120_jack_pol = GPIO_LEVEL_HIGH;
	}

	return lc1120_jack_pol;
}

static int lc1120_codec_hp_status_get(int *hp_status)
{
	unsigned char codec_reg;
	int level;
	lc1120_clk_enable();
	codec_reg = comip_lc1120_i2c_read(LC1120_R100);
	level = lc1120_codec_hp_detpol_get();

	if(codec_reg & 0x20) {
		*hp_status = 1;
	} else {
		*hp_status = 0;
	}
#if 0
	if(HEADPHONE_DETECT_LEVEL != level) {
		if(JACK_IN == lc1120_hp_status) {
			lc1120_hp_status = JACK_OUT;
		} else {
			lc1120_hp_status = JACK_IN;
		}
	}
#endif

	lc1120_clk_disable();	

	return 0;
}

static int lc1120_codec_hook_status_get(int *hookswitch_status)
{
    unsigned char codec_reg;

	lc1120_clk_enable();
	codec_reg = comip_lc1120_i2c_read(LC1120_R100);

	if(codec_reg & 0x80) {
		*hookswitch_status = SWITCH_BUTTON_PRESS;
	} else {
		*hookswitch_status = SWITCH_BUTTON_RELEASE;
	}

	return 0;
}

static int lc1120_codec_jack_mask(int nMask)
{
	unsigned char codec_reg;
	lc1120_clk_enable();
	if(nMask) {
		codec_reg = comip_lc1120_i2c_read(LC1120_R95);
		codec_reg = codec_reg | 0x80;
		comip_lc1120_i2c_write(LC1120_R95, codec_reg);
		codec_reg = comip_lc1120_i2c_read(LC1120_R85);
		codec_reg = codec_reg & 0xbf;
		comip_lc1120_i2c_write(LC1120_R85, codec_reg);
	} else { //unMask
		codec_reg = comip_lc1120_i2c_read(LC1120_R85);
		codec_reg = codec_reg | 0x40;
		comip_lc1120_i2c_write(LC1120_R85, codec_reg);
		codec_reg = comip_lc1120_i2c_read(LC1120_R95);
		codec_reg = codec_reg & 0x7f;
		comip_lc1120_i2c_write(LC1120_R95, codec_reg);
	}
	lc1120_clk_disable();
	return 0;
}
static int lc1120_codec_hook_mask(int nMask)
{
	unsigned char codec_reg;
	lc1120_clk_enable();
	if(nMask) {
		codec_reg = comip_lc1120_i2c_read(LC1120_R95);
		codec_reg = codec_reg | 0x40;
		comip_lc1120_i2c_write(LC1120_R95, codec_reg);
		codec_reg = comip_lc1120_i2c_read(LC1120_R84);
		codec_reg = codec_reg & 0xfe;
		comip_lc1120_i2c_write(LC1120_R84, codec_reg);
	} else { // unmask
		codec_reg = comip_lc1120_i2c_read(LC1120_R84);
		codec_reg = codec_reg | 0x01;
		comip_lc1120_i2c_write(LC1120_R84, codec_reg);
		codec_reg = comip_lc1120_i2c_read(LC1120_R95);
		codec_reg = codec_reg & 0xbf;
		comip_lc1120_i2c_write(LC1120_R95, codec_reg);
	}
	lc1120_clk_disable();
	return 0;
}

static int lc1120_codec_irq_type_get()
{
	unsigned char codec_reg=0,codec_reg1=0,codec_reg2=0,codec_reg3=0,codec_reg4=0;
	unsigned char hookswitch_irs=0;
	unsigned char jack_irs=0;
	unsigned char jack_cmp=0;
	unsigned char irs_type = 0;
	lc1120_clk_enable();

	codec_reg = comip_lc1120_i2c_read(LC1120_R100);
	codec_reg1 = comip_lc1120_i2c_read(LC1120_R101);
	codec_reg2 = comip_lc1120_i2c_read(LC1120_R95);
	codec_reg3 = comip_lc1120_i2c_read(LC1120_R85);
	codec_reg4 = comip_lc1120_i2c_read(LC1120_R84);

	hookswitch_irs = codec_reg & 0x40;
	jack_irs = codec_reg & 0x10;
	jack_cmp = codec_reg & 0x20;

	if (jack_irs)
		irs_type = COMIP_SWITCH_INT_JACK;
	else if (hookswitch_irs) {
		if (jack_cmp == 0)
			irs_type = COMIP_SWITCH_INT_JACK;
		else
			irs_type = COMIP_SWITCH_INT_BUTTON;
	}

	CODEC_LC1120_PRINTKA( "switch irs_type=%d,Reg%d=0x%x,Reg%d=0x%x,Reg%d=0x%x,Reg%d=0x%x,Reg84=0x%x\n",\
		irs_type,LC1120_R100,codec_reg,LC1120_R101,codec_reg1,LC1120_R95,codec_reg2,LC1120_R85,codec_reg3,codec_reg4);

	lc1120_clk_disable();

	return irs_type;
}

static int lc1120_codec_clr_irq(void)
{
	lc1120_clk_enable();

	comip_lc1120_i2c_write(LC1120_R09, 0x0f);
	comip_lc1120_i2c_write(LC1120_R09, 0x0c);
	comip_lc1120_i2c_write(LC1120_R09, 0x02);//clear hook irq
	comip_lc1120_i2c_write(LC1120_R09, 0x0c);
	lc1120_clk_disable();

	return 0;
}

static int lc1120_codec_get_headphone_key_num(int *button_type)
{
	*button_type = SWITCH_KEY_HOOK;
	return 0;
}

static ssize_t codec_lc1120_comip_register_show(const char *buf)
{
	int len;
	len = sprintf((char *)buf, "----dump 1120 reg----\n");
	dump_reg();
	return len;
}

static ssize_t codec_lc1120_comip_register_store( const char *buf, size_t size)
{
	int long ngtest =0 ;
	u8 nReg =0;
	u8 nData=0;

	ngtest = simple_strtoul(buf, NULL, 10);
	nReg = (ngtest &0xff);
	nData = ((ngtest>>0x08) &0xff);
	CODEC_LC1120_PRINTKC( "1120_register_store Reg0x%x = 0x%x",nReg,nData);
	lc1120_clk_enable();
	comip_1120_write(pcodec_pri_data->pcodec,nReg, nData);
	lc1120_clk_disable();
	return size;
}

static void lc1120_i2s_ogain_to_hw(void)
{
	u8 frist_output_gain = 0, second_output_gain = 0;
	if(comip_lc1120_st_pri_data->nPath_out == DD_RECEIVER) {
		frist_output_gain = pcodec_pri_data->i2s_ogain_receiver_daclevel;
		second_output_gain = pcodec_pri_data->i2s_ogain_receiver_epvol;
	}else if(comip_lc1120_st_pri_data->nPath_out == DD_SPEAKER) {
		frist_output_gain = pcodec_pri_data->i2s_ogain_speaker_daclevel;
		second_output_gain = pcodec_pri_data->i2s_ogain_speaker_cdvol;

	}else if(comip_lc1120_st_pri_data->nPath_out == DD_HEADPHONE) {
		frist_output_gain = pcodec_pri_data->i2s_ogain_headset_daclevel;
		second_output_gain = pcodec_pri_data->i2s_ogain_headset_hpvol;

	}else if(comip_lc1120_st_pri_data->nPath_out == DD_SPEAKER_HEADPHONE) {
		frist_output_gain = pcodec_pri_data->i2s_ogain_spk_hp_daclevel;
		second_output_gain = pcodec_pri_data->i2s_ogain_spk_hp_hpvol;
	}

	//if(comip_lc1120_st_pri_data->fm == 1) {
	//	lc1120_codec_fm_output_gain_set(pcodec_pri_data->i2s_igain_line_linvol);//line_gain
	//} 
	
	lc1120_codec_frist_output_gain_set(frist_output_gain);//dac_gain
	lc1120_codec_second_output_gain_set(second_output_gain);//output_gain

}

static void lc1120_i2s_igain_to_hw(void)
{
	u8 frist_input_gain = 0, second_input_gain = 0;
	if(comip_lc1120_st_pri_data->nPath_in == DD_MIC) {
		frist_input_gain = pcodec_pri_data->i2s_igain_mic_pgagain;
		second_input_gain = pcodec_pri_data->i2s_igain_mic_adclevel;
	}else if(comip_lc1120_st_pri_data->nPath_in == DD_HEADSET) {
		frist_input_gain = pcodec_pri_data->i2s_igain_headsetmic_pgagain;
		second_input_gain = pcodec_pri_data->i2s_igain_headsetmic_adclevel;
	}

	lc1120_codec_frist_input_gain_set(frist_input_gain);//mic_gain
	lc1120_codec_second_input_gain_set(second_input_gain);//adc_gain
}
static ssize_t codec_lc1120_calibrate_data_print( const char *buf,size_t size)
{
	int len = 0;
	CODEC_LC1120_PRINTKA("gain_in_level0: %u\n", pcodec_pri_data->pvoice_table->gain_in_level0);
	CODEC_LC1120_PRINTKA("gain_in_level1: %u\n", pcodec_pri_data->pvoice_table->gain_in_level1);
	CODEC_LC1120_PRINTKA("gain_in_level2: %u\n", pcodec_pri_data->pvoice_table->gain_in_level2);
	CODEC_LC1120_PRINTKA("gain_in_level3: %u\n", pcodec_pri_data->pvoice_table->gain_in_level3);
	CODEC_LC1120_PRINTKA("gain_out_level0_class0: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class0);
	CODEC_LC1120_PRINTKA("gain_out_level0_class1: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class1);
	CODEC_LC1120_PRINTKA("gain_out_level0_class2: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class2);
	CODEC_LC1120_PRINTKA("gain_out_level0_class3: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class3);
	CODEC_LC1120_PRINTKA("gain_out_level0_class4: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class4);
	CODEC_LC1120_PRINTKA("gain_out_level0_class5: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class5);
	CODEC_LC1120_PRINTKA("gain_out_level1: %u\n", pcodec_pri_data->pvoice_table->gain_out_level1);
	CODEC_LC1120_PRINTKA("gain_out_level2: %u\n", pcodec_pri_data->pvoice_table->gain_out_level2);
	CODEC_LC1120_PRINTKA("gain_out_level3: %u\n", pcodec_pri_data->pvoice_table->gain_out_level3);
	CODEC_LC1120_PRINTKA("gain_out_level4: %u\n", pcodec_pri_data->pvoice_table->gain_out_level4);
	CODEC_LC1120_PRINTKA("gain_out_level5: %u\n", pcodec_pri_data->pvoice_table->gain_out_level5);
	CODEC_LC1120_PRINTKA("gain_out_level6: %u\n", pcodec_pri_data->pvoice_table->gain_out_level6);

	CODEC_LC1120_PRINTKA("sidetone_reserve_1: %u\n", pcodec_pri_data->pvoice_table->sidetone_reserve_1);
	CODEC_LC1120_PRINTKA("sidetone_reserve_2: %u\n", pcodec_pri_data->pvoice_table->sidetone_reserve_2);
	CODEC_LC1120_PRINTKA("cur_volume: %u\n", pcodec_pri_data->cur_volume);
	return len;

}

static ssize_t codec_lc1120_calibrate_data_show( const char *buf)
{
	int len = 0;

	len = sprintf((char *)buf, "gain_in_level0: %u\n", pcodec_pri_data->pvoice_table->gain_in_level0);
	len += sprintf((char *)buf + len, "gain_in_level1: %u\n", pcodec_pri_data->pvoice_table->gain_in_level1);
	len += sprintf((char *)buf + len, "gain_in_level2: %u\n", pcodec_pri_data->pvoice_table->gain_in_level2);
	len += sprintf((char *)buf + len, "gain_in_level3: %u\n", pcodec_pri_data->pvoice_table->gain_in_level3);
	len += sprintf((char *)buf + len, "gain_out_level0_class0: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class0);
	len += sprintf((char *)buf + len, "gain_out_level0_class1: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class1);
	len += sprintf((char *)buf + len, "gain_out_level0_class2: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class2);
	len += sprintf((char *)buf + len, "gain_out_level0_class3: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class3);
	len += sprintf((char *)buf + len, "gain_out_level0_class4: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class4);
	len += sprintf((char *)buf + len, "gain_out_level0_class5: %u\n", pcodec_pri_data->pvoice_table->gain_out_level0_class5);
	len += sprintf((char *)buf + len, "gain_out_level1: %u\n", pcodec_pri_data->pvoice_table->gain_out_level1);
	len += sprintf((char *)buf + len, "gain_out_level2: %u\n", pcodec_pri_data->pvoice_table->gain_out_level2);
	len += sprintf((char *)buf + len, "gain_out_level3: %u\n", pcodec_pri_data->pvoice_table->gain_out_level3);
	len += sprintf((char *)buf + len, "gain_out_level4: %u\n", pcodec_pri_data->pvoice_table->gain_out_level4);
	len += sprintf((char *)buf + len, "gain_out_level5: %u\n", pcodec_pri_data->pvoice_table->gain_out_level5);
	len += sprintf((char *)buf + len, "gain_out_level6: %u\n", pcodec_pri_data->pvoice_table->gain_out_level6);

	len += sprintf((char *)buf + len, "sidetone_reserve_1: %u\n", pcodec_pri_data->pvoice_table->sidetone_reserve_1);
	len += sprintf((char *)buf + len, "sidetone_reserve_2: %u\n", pcodec_pri_data->pvoice_table->sidetone_reserve_2);

	return len;
}

static void lc1120_pcm_tbl_to_hw(struct codec_voice_tbl *ptbl,unsigned char volumeset)
{
	u8 gain;


	if(volumeset == 1) {
		/*
		 * gain to analog input
		 */
		/*  -- input, the first step (analog)  64 levels*/
		lc1120_codec_frist_input_gain_set(ptbl->gain_in_level0);
		/*
		 * gain to digital input
		 */
		/* ADC -- input, the second step (digital)  128 levels*/

		lc1120_codec_second_input_gain_set(ptbl->gain_in_level1);

		/*
		 * gain to digital output
		 */
		/* user's volume, DAC 128 levels -- output, the first step (digital) */
		gain = (*((u8 *)&ptbl->gain_out_level0_class0 + pcodec_pri_data->cur_volume));
		lc1120_codec_frist_output_gain_set(gain);
		/* -- output, the second step (digital) 32 levels */
		lc1120_codec_second_output_gain_set(ptbl->gain_out_level1);
	} else {
		/* gain to digital output */
		/* user's volume, DAC 128 levels -- output, the first step (digital) */
		gain = (*((u8 *)&ptbl->gain_out_level0_class0 + pcodec_pri_data->cur_volume));
		lc1120_codec_frist_output_gain_set(gain);

	}

}

static void lc1120_load_default_pcmtbl(struct codec_voice_tbl *ptbl)
{
	if (!ptbl)
		return;

	ptbl->gain_in_level0 = 3 * 8;
	ptbl->gain_in_level1 = 96;
	ptbl->gain_in_level2 = 0x00;			/* no use for lc1120 */
	ptbl->gain_in_level3 = 0x00;			/* no use for lc1120 */

	ptbl->gain_out_level0_class0 = 32 * 2;
	ptbl->gain_out_level0_class1 = 39 * 2;
	ptbl->gain_out_level0_class2 = 45 * 2;
	ptbl->gain_out_level0_class3 = 51 * 2;
	ptbl->gain_out_level0_class4 = 57 * 2;
	ptbl->gain_out_level0_class5 = 63 * 2;
	ptbl->gain_out_level1 = 96;
	ptbl->gain_out_level2 = 0x00;			/* no use for lc1120 */
	ptbl->gain_out_level3 = 0x00;			/* no use for lc1120 */
	ptbl->gain_out_level4 = 0x00;			/* no use for lc1120 */
	ptbl->gain_out_level5 = 0x00;			/* no use for lc1120 */
	ptbl->gain_out_level6 = 0x00;			/* no use for lc1120 */

	ptbl->sidetone_reserve_1 = 0;			/* close hard sidetone at default */

}
static ssize_t codec_lc1120_calibrate_data_store( const char *buf, size_t size)
{
	unsigned int i;
	struct codec_voice_tbl *ptbl;
	lc1120_clk_enable();

	if (buf == NULL || size > CODEC_CAL_TBL_SIZE) {
		CODEC_LC1120_PRINTKC("pcm table is updated with error\n");
		lc1120_clk_disable();
		return -1;
	}

	ptbl = (struct codec_voice_tbl *)buf;
	if (ptbl->flag != 0xAAAAAAAA) {
		CODEC_LC1120_PRINTKC("pcm table is updated with magic error ptbl->flag = 0x%x,load default value\n",ptbl->flag);
		lc1120_pcm_tbl_to_hw(pcodec_pri_data->pvoice_table,1);
		codec_lc1120_calibrate_data_print(buf,size);
		lc1120_clk_disable();
		return -1;
	}
	for (i = 0; i < CODEC_PT_GAIN_ITEMNUM; i++) {
		if (*((u8 *)&ptbl->gain_in_level0 + i) > CODEC_PT_GAIN_MAXVAL) {
			CODEC_LC1120_PRINTKC("pcm table is updated with item error\n");
			lc1120_clk_disable();
			return -1;
		}
	}
	memcpy(pcodec_pri_data->pvoice_table, buf, size);
	CODEC_LC1120_PRINTKC("pcm table is updated with calibrate value\n");
	lc1120_pcm_tbl_to_hw(pcodec_pri_data->pvoice_table,1);
	codec_lc1120_calibrate_data_print(buf,size);
	lc1120_clk_disable();

	return size;
}

static int codec_lc1120_deinit()
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("codec_lc1120_deinit,play %d,voice %d,record %d,fm %d\n",\
		comip_lc1120_st_pri_data->type_play,comip_lc1120_st_pri_data->type_voice,comip_lc1120_st_pri_data->type_record,comip_lc1120_st_pri_data->fm);

	if((comip_lc1120_st_pri_data->type_record == 0)&&(comip_lc1120_st_pri_data->type_voice == 0)\
		&&(comip_lc1120_st_pri_data->fm == 0)&&(comip_lc1120_st_pri_data->type_play == 0)) {
		nRst = lc1120_codec_close();
	} else {
		if((comip_lc1120_st_pri_data->type_voice == 0)&&(comip_lc1120_st_pri_data->type_play == 0)&&\
			(comip_lc1120_st_pri_data->fm == 0)) {
			lc1120_codec_set_output_mute(1);
			lc1120_codec_output_path_set(OUT_ALL_MUTE);
		}

		if ((comip_lc1120_st_pri_data->type_voice == 0)&&(comip_lc1120_st_pri_data->type_play == 0))
			lc1120_codec_dac_disable();

		if((comip_lc1120_st_pri_data->type_record == 0)&&(comip_lc1120_st_pri_data->type_voice == 0))
			lc1120_codec_adc_disable();

		if(comip_lc1120_st_pri_data->fm == 0)
			lc1120_codec_fm_enable(0);
	}

	return 0;
}
static int codec_lc1120_playback_deinit(void)
{

	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_playback_deinit,play %d,voice %d,record %d,fm %d\n",\
		comip_lc1120_st_pri_data->type_play,comip_lc1120_st_pri_data->type_voice,comip_lc1120_st_pri_data->type_record,comip_lc1120_st_pri_data->fm);

	comip_lc1120_st_pri_data->type_play = 0;

	codec_lc1120_deinit();
	lc1120_clk_disable();
	return nRst;
}

static int codec_lc1120_capture_deinit(void)
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_capture_deinit,play %d,voice %d,record %d,fm %d\n",\
		comip_lc1120_st_pri_data->type_play,comip_lc1120_st_pri_data->type_voice,comip_lc1120_st_pri_data->type_record,comip_lc1120_st_pri_data->fm);

	comip_lc1120_st_pri_data->type_record = 0;

	codec_lc1120_deinit();
	lc1120_clk_disable();
	return nRst;
}
static int codec_lc1120_playback_init(void)
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_playback_init,play %d,voice %d,record %d,fm %d\n",\
		comip_lc1120_st_pri_data->type_play,comip_lc1120_st_pri_data->type_voice,comip_lc1120_st_pri_data->type_record,comip_lc1120_st_pri_data->fm);

	comip_lc1120_st_pri_data->nType_audio = AUDIO_TYPE;
	comip_lc1120_st_pri_data->type_play = 1;
	lc1120_clk_enable();
	nRst = lc1120_codec_open(comip_lc1120_st_pri_data->nType_audio);
	if(comip_lc1120_st_pri_data->type_voice == 0)
		lc1120_playback_second();
	return nRst;
}

static int codec_lc1120_capture_init(void)
{

	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_capture_init,play %d,voice %d,record %d,fm %d\n",\
		comip_lc1120_st_pri_data->type_play,comip_lc1120_st_pri_data->type_voice,comip_lc1120_st_pri_data->type_record,comip_lc1120_st_pri_data->fm);
	comip_lc1120_st_pri_data->nType_audio = AUDIO_CAPTURE_TYPE;
	comip_lc1120_st_pri_data->type_record = 1;
	lc1120_clk_enable();	
	nRst = lc1120_codec_open(comip_lc1120_st_pri_data->nType_audio);
	if(comip_lc1120_st_pri_data->type_voice == 0)
		lc1120_capture_second();
	return nRst;
}



static void comip_pcm0_sel(int sel)
{
	#if 0
	int val=0;

	if(sel == CODEC_PCM_SEL) {
		val = readl(io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
		val &= ~0x10;//select pcm0
		writel(val,io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
	} else if(sel == BT_PCM_SEL) {
		val = readl(io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
		val |= 0x10;//select pcm1
		writel(val,io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
	}
	#endif
}

int codec_lc1120_enable_bluetooth(int nValue)
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_enable_bluetooth nValue=%d\n",nValue);
	lc1120_clk_enable();
	if(nValue) {
		comip_pcm0_sel(BT_PCM_SEL);
		lc1120_codec_output_path_set(4);
	} else {
		comip_pcm0_sel(CODEC_PCM_SEL);
	}
	lc1120_clk_disable();
	return nRst;
}

int codec_lc1120_fm_line_gain(int nGain)
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_fm_line_gain nGain=%d\n",nGain);
	lc1120_clk_enable();
	lc1120_codec_fm_output_gain_set(nGain);//line_gain
	lc1120_clk_disable();
	return nRst;
}

int codec_lc1120_enable_fm(int nValue)
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_enable_fm nValue=%d\n",nValue);
	lc1120_clk_enable();
	if(nValue == 1) {
		lc1120_init();
		nRst = lc1120_codec_fm_enable(1);
		comip_lc1120_st_pri_data->fm = 1;

	}else {
    	comip_lc1120_st_pri_data->fm = 0;    
		codec_lc1120_deinit();
	}
	lc1120_clk_disable();
	return nRst;
}

int codec_lc1120_set_input_path(int nType,int nPath)
{
	int nRst =0;

	comip_lc1120_st_pri_data->nPath_in = nPath;
	CODEC_LC1120_PRINTKA("lc1120_set_input_path,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x\n",
		comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in);
	lc1120_clk_enable();

	if(comip_lc1120_st_pri_data->pdm_flag && comip_lc1120_st_pri_data->type_voice) {
		if(DD_HEADSET == nPath) {
			// pcm sel adc /open adc / open hpmic
			lc1120_codec_input_dmic_headset_set();
		}else if (DD_MIC == nPath) {
			//pcm sel pdm / open pdm
			lc1120_codec_input_dmic_pdm_set();
		}
	} else if(comip_lc1120_st_pri_data->pdm_flag && comip_lc1120_st_pri_data->type_record){
		if(DD_MIC == nPath) {
			lc1120_capture_second();
		}else if(DD_HEADSET == nPath) {
			lc1120_capture_dmic_input_headset();
		}
	}else{
	nRst = lc1120_codec_input_path_set(comip_lc1120_st_pri_data->nPath_in);
	}
	if(((comip_lc1120_st_pri_data->type_record == 1)||(comip_lc1120_st_pri_data->type_play == 1)||(comip_lc1120_st_pri_data->fm == 1))\
		&&(comip_lc1120_st_pri_data->type_voice == 0)) {
		lc1120_i2s_igain_to_hw();
	}
	lc1120_clk_disable();
	return nRst;
}
int codec_lc1120_set_output_path(int nType,int nPath)
{

	int nRst =0;
	int inputpath;

	if(nPath == DD_OUTMUTE) //action: lc1132 need this,bug 1120 not need;
		return 0;
	if(!(comip_lc1120_st_pri_data->type_play |comip_lc1120_st_pri_data->fm | comip_lc1120_st_pri_data->type_voice))
		return 0; //action: android 4.2, framework set output path before  pcm open ,it make pop 

	comip_lc1120_st_pri_data->nPath_out = nPath;
	CODEC_LC1120_PRINTKA("lc1120_set_output_path,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x\n",
	                     comip_lc1120_st_pri_data->nType_audio,nPath,comip_lc1120_st_pri_data->nPath_in);
	lc1120_clk_enable();

	nRst = lc1120_codec_output_path_set(comip_lc1120_st_pri_data->nPath_out);
	if(((comip_lc1120_st_pri_data->type_record == 1)||(comip_lc1120_st_pri_data->type_play == 1)||(comip_lc1120_st_pri_data->fm == 1))\
		&&(comip_lc1120_st_pri_data->type_voice == 0)) {
		lc1120_i2s_ogain_to_hw();
	}
	#if 0
	if(comip_lc1120_st_pri_data->type_voice == 1) {
		if(nPath == 3)
			inputpath = 2;
		else
			inputpath = 1;
		nRst = codec_lc1120_set_input_path(inputpath);
	}
	#endif
	lc1120_clk_disable();

	return nRst;
}

int codec_lc1120_set_output_mute(int nType,int nMute)
{
	int nRst =0;
	CODEC_LC1120_PRINTKB("lc1120_set_output_mute,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x,nMute=0x%x\n",
	                     comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,nMute);
	nRst = lc1120_codec_set_output_mute(nMute);
	return nRst;
}
int codec_lc1120_mp_powersave(int nMute)
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_mp_powersave,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x,nMute=0x%x\n",
	                     comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,nMute);
	nRst = lc1120_codec_set_mp_output_mute(nMute);
	return nRst;
}

int codec_lc1120_enable_alc(int nValue)
{
	int i;
	u8 reg,val;
	if(nValue&&comip_lc1120_st_pri_data->type_play){
		CODEC_LC1120_PRINTKA(KERN_INFO"lc1120_codec_update_alc\n");
		for(i=0;i<14;i++){ //r51-64
			reg = LC1120_R51 + i;
			val = *((u8*)&pcodec_pri_data->alc_reg51 + i);
			comip_lc1120_i2c_write(reg,val);
			printk("lc1120_codec_update_alc: reg:%d,val:0x%x\n",reg,val);
		}
	} else {
		/*HP disable*/
		comip_lc1120_i2c_write(LC1120_R51,0x00);
		/*ALC disable*/
		comip_lc1120_i2c_write(LC1120_R52,0x00);
		/*EQ disable*/
		comip_lc1120_i2c_write(LC1120_R60,0x02);
		comip_lc1120_i2c_write(LC1120_R61,0x02);
		comip_lc1120_i2c_write(LC1120_R62,0x02);
		comip_lc1120_i2c_write(LC1120_R63,0x02);
		comip_lc1120_i2c_write(LC1120_R64,0x02);
	}
}
int codec_lc1120_set_output_gain(int nType,int nGain)
{
	int nRst =0;
	//nGain = 0x307f00|nGain;
	CODEC_LC1120_PRINTKA("lc1120_set_output_gain,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x,nGain=0x%x\n",
	                     comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,nGain);
	lc1120_clk_enable();

	if(nType == FM_TYPE) {
		codec_lc1120_fm_line_gain(nGain);
		return 0;
	}
	//if(comip_lc1120_st_pri_data->type_play == 1)
	//	comip_lc1120_st_pri_data->nplay_output_gain = nGain;
	pcodec_pri_data->cur_volume = nGain;
	if(comip_lc1120_st_pri_data->type_voice == 1) {
		lc1120_pcm_tbl_to_hw(pcodec_pri_data->pvoice_table,0);
		nRst = 0;
	} //else {
	//	nRst = lc1120_codec_output_gain_set(comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,nGain);
	//}
	lc1120_clk_disable();
	return nRst;
}

int codec_lc1120_set_output_samplerate(int nSamplerate)
{
	int nRst=0;
	//CODEC_LC1120_PRINTKB("codec_lc1120_set_output_samplerate,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x,nSamplerate=0x%x\n",
	//                     comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,nSamplerate);

	//nRst = lc1120_codec_output_samplerate_set(nSamplerate);
	return nRst;
}


int codec_lc1120_set_input_gain(int nType,int nGain)
{
	int nRst =0;
	//if(comip_lc1120_st_pri_data->type_record == 1)
	//	comip_lc1120_st_pri_data->nrecord_input_gain = nGain;
	//CODEC_LC1120_PRINTKB("codec_lc1120_set_input_gain,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x,nGain=0x%x\n",
	//                     comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,nGain);
	//nRst = lc1120_codec_input_gain_set(nGain);
	return nRst;
}

int codec_lc1120_set_input_samplerate(int nSamplerate)
{

	int nRst=0;

	//CODEC_LC1120_PRINTKB("codec_lc1120_set_input_samplerate,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x,nSamplerate=0x%x\n",
	//                     comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,nSamplerate);
	//nRst = lc1120_codec_input_samplerate_set(nSamplerate);
	return nRst;
}
int codec_lc1120_set_input_mute(int nType,int nValue)
{
	int result=0;
	//CODEC_LC1120_PRINTKA("codec_lc1120_set_input_mute,nType=0x%x,nPath_out=0x%x,nPath_in=0x%x,nValue=0x%x\n",
	//                     comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,nValue);
	//lc1120_clk_enable();
	//result = lc1120_codec_set_input_mute(nValue);
	//lc1120_clk_disable();
	return result;

}


int codec_lc1120_enable_voice(int nValue)
{
	int nRst =0;
	CODEC_LC1120_PRINTKA("lc1120_enable_voice %d,play %d,voice %d,record %d,fm %d,pdm %d\n",\
		nValue,comip_lc1120_st_pri_data->type_play,comip_lc1120_st_pri_data->type_voice,comip_lc1120_st_pri_data->type_record,comip_lc1120_st_pri_data->fm,comip_lc1120_st_pri_data->pdm_flag);

	lc1120_clk_enable();
	if(nValue ==1) {
		comip_lc1120_st_pri_data->nType_audio = VOICE_TYPE;
		comip_lc1120_st_pri_data->type_voice = 1;
		comip_pcm0_sel(CODEC_PCM_SEL);
		lc1120_init();
		nRst = lc1120_codec_open(comip_lc1120_st_pri_data->nType_audio);
		//raw_notifier_call_chain(&CALL_STATE_CHANGE,VOICE_CALL_START,NULL);
	} else {
	//      raw_notifier_call_chain(&CALL_STATE_CHANGE,VOICE_CALL_STOP,NULL);
		comip_lc1120_st_pri_data->nType_audio = AUDIO_TYPE;
		comip_lc1120_st_pri_data->type_voice = 0;
		//nRst = lc1120_deinit();
		if((comip_lc1120_st_pri_data->type_record == 0)&&(comip_lc1120_st_pri_data->type_play == 0)&&(comip_lc1120_st_pri_data->fm == 0))
			lc1120_codec_close();
		else {
			if(comip_lc1120_st_pri_data->type_play == 1) {
				lc1120_playback_second();
				//codec_lc1120_set_output_gain(comip_lc1120_st_pri_data->nplay_output_gain);
				lc1120_i2s_ogain_to_hw();
				codec_lc1120_set_output_path(0,comip_lc1120_st_pri_data->nPath_out);
			}

			if(comip_lc1120_st_pri_data->type_record == 1) {
				lc1120_capture_second();
				//codec_lc1120_set_input_gain(comip_lc1120_st_pri_data->nrecord_input_gain);
				lc1120_i2s_igain_to_hw();
				codec_lc1120_set_input_path(0,comip_lc1120_st_pri_data->nPath_in);
			}
			codec_lc1120_deinit();
		}
	}
	lc1120_clk_disable();
	return nRst;
}

#ifdef CODEC_SOC_PROC
static ssize_t lc1120_proc_read(char *page,int count,int *eof)
{
	char *buf  = page;
	char *next = buf;
	int  t, i, regval;
	unsigned size = count;
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	t = scnprintf(next, size, "system time: (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	size -= t;
	next += t;
	t = scnprintf(next, size, "codec_lc1120_log_print,nType=%d,play=%d,record=%d,voice=%d,fm=%d,naudio_Path_out=%d,nPath_in=%d,cur_volume=%d\n",\
		comip_lc1120_st_pri_data->nType_audio,comip_lc1120_st_pri_data->type_play,
		comip_lc1120_st_pri_data->type_record,comip_lc1120_st_pri_data->type_voice,comip_lc1120_st_pri_data->fm,\
		comip_lc1120_st_pri_data->nPath_out,comip_lc1120_st_pri_data->nPath_in,pcodec_pri_data->cur_volume);

	size -= t;
	next += t;

	t = scnprintf(next, size, "lc1120 regs: \n");
	size -= t;
	next += t;

	lc1120_clk_enable();
	for (i = 0; i < comip_1120_REGS_NUM; i++) {
		regval = comip_lc1120_i2c_read(i);
		//t = scnprintf(next, size, "[%d 0x%02x]=0x%02x  \n",i , i, regval);
		t = scnprintf(next, size, "[%d 0x%02x]=0x%02x  \n",i , i, regval);
		size -= t;
		next += t;
	}

	*eof = 1;
	lc1120_clk_disable();
	return count - size;
}

static int lc1120_proc_write(const char __user *buffer,unsigned long count)
{
	static char kbuf[200];
	char *buf = kbuf;
	unsigned int val, reg, val2;
	char cmd;

	if (count >= 200)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	//sscanf(buf, "%c 0x%x 0x%x", &cmd, &reg, &val);
	sscanf(buf, "%c %d 0x%x", &cmd, &reg, &val);

	lc1120_clk_enable();

	if ('r' == cmd) {
		val = comip_lc1120_i2c_read(reg);
		CODEC_LC1120_PRINTKC( "Read reg:[%d]=0x%2x\n", reg, val);

	} else if ('w' == cmd) {
		if (reg > 0xff) {
			CODEC_LC1120_PRINTKC( "invalid value!\n");
			goto error;
		}
		comip_lc1120_i2c_write(reg, val);
		val2 = comip_lc1120_i2c_read(reg);
		scnprintf(buf, count, "%c read back %d 0x%x", cmd, reg, val2);
		CODEC_LC1120_PRINTKC("write reg:%d<==0x[%2x], read back 0x%2x\n",reg, val, val2);

	} else if ('o' == cmd) {
		val = comip_lc1120_i2c_read(reg);
		CODEC_LC1120_PRINTKC( "Read reg:[%d]=0x%08x\n", reg, val);

	} else if ('i' == cmd) {
		val2 = comip_lc1120_i2c_read(reg);
		CODEC_LC1120_PRINTKC( "Read reg:[%d]=0x%08x\n", reg, val2);

		comip_lc1120_i2c_write(reg, val);
		CODEC_LC1120_PRINTKC( "Write reg:[%d]=0x%08x\n", reg, val);
	} else if ('h' == cmd) {//headsetmic -> receiver
		comip_lc1120_i2c_write(LC1120_R89, 0x80);
		comip_lc1120_i2c_write(LC1120_R87, 0xf6);
		comip_lc1120_i2c_write(LC1120_R86, 0xc0);
		comip_lc1120_i2c_write(LC1120_R93, 0x80);

		val2 = comip_lc1120_i2c_read(LC1120_R85);
		val2 = (val2&0xe0)|0x14;
		comip_lc1120_i2c_write(LC1120_R85, val2);

		comip_lc1120_i2c_write(LC1120_R01, 0x28);
		comip_lc1120_i2c_write(LC1120_R91, 0x44);
		comip_lc1120_i2c_write(LC1120_R04, 0x7e);
		comip_lc1120_i2c_write(LC1120_R05, 0x79);
		comip_lc1120_i2c_write(LC1120_R06, 0x80);
		comip_lc1120_i2c_write(LC1120_R07, 0x80);
		comip_lc1120_i2c_write(LC1120_R84, 0x77);
		comip_lc1120_i2c_write(LC1120_R40, 0x1f);
		CODEC_LC1120_PRINTKC( "audio loopback, headsetmic -> receiver\n");

	} else if ('m' == cmd) {//mic -> headset
		comip_lc1120_i2c_write(LC1120_R89, 0x81);
		comip_lc1120_i2c_write(LC1120_R87, 0xf6);
		comip_lc1120_i2c_write(LC1120_R86, 0xc0);
		comip_lc1120_i2c_write(LC1120_R93, 0x80);

		val2 = comip_lc1120_i2c_read(LC1120_R85);
		val2 = (val2&0xe0)|0x03;
		comip_lc1120_i2c_write(LC1120_R85, val2);

		comip_lc1120_i2c_write(LC1120_R01, 0x28);
		comip_lc1120_i2c_write(LC1120_R90, 0x18);
		comip_lc1120_i2c_write(LC1120_R91, 0x80);
		comip_lc1120_i2c_write(LC1120_R04, 0x7e);
		comip_lc1120_i2c_write(LC1120_R06, 0x79);
		comip_lc1120_i2c_write(LC1120_R07, 0x79);
		comip_lc1120_i2c_write(LC1120_R84, 0x77);
		comip_lc1120_i2c_write(LC1120_R40, 0x1f);
		CODEC_LC1120_PRINTKC( "audio loopback, mic -> headset\n");
	}else if( 'a' == cmd) { //mic1 -> receiver

		comip_lc1120_i2c_write(LC1120_R89, 0x81);
		comip_lc1120_i2c_write(LC1120_R87, 0xf6);
		comip_lc1120_i2c_write(LC1120_R86, 0xc0);
		comip_lc1120_i2c_write(LC1120_R93, 0x80);

		val2 = comip_lc1120_i2c_read(LC1120_R85);
		val2 = (val2&0xe0)|0x03;
		comip_lc1120_i2c_write(LC1120_R85, val2);

		comip_lc1120_i2c_write(LC1120_R01, 0x28); //mic pga unmute
		//comip_lc1120_i2c_write(LC1120_R90, 0x18); //STone sel HP
		//comip_lc1120_i2c_write(LC1120_R91, 0x80);
		comip_lc1120_i2c_write(LC1120_R91, 0x44);
		comip_lc1120_i2c_write(LC1120_R05, 0x7f);
		comip_lc1120_i2c_write(LC1120_R04, 0x7e); // Stone PGA
		//comip_lc1120_i2c_write(LC1120_R06, 0x79);
		//comip_lc1120_i2c_write(LC1120_R07, 0x79);
		comip_lc1120_i2c_write(LC1120_R84, 0x77); //micbais enable
		comip_lc1120_i2c_write(LC1120_R40, 0x1f); //mic pga gain
		CODEC_LC1120_PRINTKC( "audio loopback, mic1 -> receiver\n");

	}else if('b' == cmd) { //mic2 -> spk
		#if 0
		lc1120_init();
		lc1120_codec_open(VOICE_TYPE);
		#endif
		codec_lc1120_enable_voice(1);
		//digital Tone
		comip_lc1120_i2c_write(LC1120_R47, 0x95);
		comip_lc1120_i2c_write(LC1120_R48, 0x15);
		lc1120_codec_output_path_set(DD_SPEAKER);
		lc1120_codec_input_path_set(1);
	//	lc1120_codec_input_gain_set(90);
		comip_lc1120_i2c_write(LC1120_R39,0xf0);
		comip_lc1120_i2c_write(LC1120_R40, 0x00); //mic pga gain
		lc1120_codec_set_output_mute(0);
		CODEC_LC1120_PRINTKC( "audio loopback, mic2 -> spk\n");

	}else if('c' == cmd) { //hp mic -> hp
		comip_lc1120_i2c_write(LC1120_R89, 0x81);
		comip_lc1120_i2c_write(LC1120_R87, 0xf6);
		comip_lc1120_i2c_write(LC1120_R86, 0xc0);
		comip_lc1120_i2c_write(LC1120_R93, 0x80);

		val2 = comip_lc1120_i2c_read(LC1120_R85);
		val2 = (val2&0xe0)|0x14;
		comip_lc1120_i2c_write(LC1120_R85, val2);

		comip_lc1120_i2c_write(LC1120_R01, 0x28);
		comip_lc1120_i2c_write(LC1120_R90, 0x18);
		comip_lc1120_i2c_write(LC1120_R91, 0x80);
		comip_lc1120_i2c_write(LC1120_R04, 0x7e);
		comip_lc1120_i2c_write(LC1120_R06, 0x79);
		comip_lc1120_i2c_write(LC1120_R07, 0x79);
		comip_lc1120_i2c_write(LC1120_R84, 0x77);
		comip_lc1120_i2c_write(LC1120_R40, 0x1f);
		CODEC_LC1120_PRINTKC( "audio loopback, hp mic -> headset\n");
	}else if('d' == cmd) {//mic1 -> hp
		comip_lc1120_i2c_write(LC1120_R89, 0x81);
		comip_lc1120_i2c_write(LC1120_R87, 0xf6);
		comip_lc1120_i2c_write(LC1120_R86, 0xc0);
		comip_lc1120_i2c_write(LC1120_R93, 0x80);

		val2 = comip_lc1120_i2c_read(LC1120_R85);
		val2 = (val2&0xe0)|0x03;
		comip_lc1120_i2c_write(LC1120_R85, val2);

		comip_lc1120_i2c_write(LC1120_R01, 0x28);
		comip_lc1120_i2c_write(LC1120_R90, 0x18);
		comip_lc1120_i2c_write(LC1120_R91, 0x80);
		comip_lc1120_i2c_write(LC1120_R04, 0x7e);
		comip_lc1120_i2c_write(LC1120_R06, 0x79);
		comip_lc1120_i2c_write(LC1120_R07, 0x79);
		comip_lc1120_i2c_write(LC1120_R84, 0x77);
		comip_lc1120_i2c_write(LC1120_R40, 0x1f);
		CODEC_LC1120_PRINTKC( "audio loopback, mic1 -> headset\n");

	}else if('e' ==cmd) {//mic2 ->hp
		comip_lc1120_i2c_write(LC1120_R89, 0x81);
		comip_lc1120_i2c_write(LC1120_R87, 0xf6);
		comip_lc1120_i2c_write(LC1120_R86, 0xc0);
		comip_lc1120_i2c_write(LC1120_R93, 0x80);

		val2 = comip_lc1120_i2c_read(LC1120_R85);
		val2 = (val2&0xe0)|0x03;
		comip_lc1120_i2c_write(LC1120_R85, val2);

		comip_lc1120_i2c_write(LC1120_R01, 0x28);
		comip_lc1120_i2c_write(LC1120_R90, 0x18);
		comip_lc1120_i2c_write(LC1120_R91, 0x80);
		comip_lc1120_i2c_write(LC1120_R04, 0x7e);
		comip_lc1120_i2c_write(LC1120_R06, 0x79);
		comip_lc1120_i2c_write(LC1120_R07, 0x79);
		comip_lc1120_i2c_write(LC1120_R84, 0x77);
		comip_lc1120_i2c_write(LC1120_R40, 0x1f);
		CODEC_LC1120_PRINTKC( "audio loopback, mic2 -> headset\n");
	}else if('f' ==cmd){ //mic2 -> receiver
		#if 1
 		comip_lc1120_i2c_write(LC1120_R89, 0x81);
		comip_lc1120_i2c_write(LC1120_R87, 0xf6);
		comip_lc1120_i2c_write(LC1120_R86, 0xc0);
		comip_lc1120_i2c_write(LC1120_R93, 0x80);

		val2 = comip_lc1120_i2c_read(LC1120_R85);
		val2 = (val2&0xe0)|0x03;
		comip_lc1120_i2c_write(LC1120_R85, val2);

		comip_lc1120_i2c_write(LC1120_R01, 0x28); //mic pga unmute
		//comip_lc1120_i2c_write(LC1120_R90, 0x18); //STone sel HP
		//comip_lc1120_i2c_write(LC1120_R91, 0x80);
		comip_lc1120_i2c_write(LC1120_R91, 0x44);
		comip_lc1120_i2c_write(LC1120_R05, 0x7f);
		comip_lc1120_i2c_write(LC1120_R04, 0x7e); // Stone PGA
		//comip_lc1120_i2c_write(LC1120_R06, 0x79);
		//comip_lc1120_i2c_write(LC1120_R07, 0x79);
		comip_lc1120_i2c_write(LC1120_R84, 0x77); //micbais enable
		comip_lc1120_i2c_write(LC1120_R40, 0x1f); //mic pga gain
		CODEC_LC1120_PRINTKC( "audio loopback, mic2 -> receiver\n");
		#endif
		}	else if ('x' == cmd) {//colse audio loopback
			codec_lc1120_enable_voice(0);
			/*close digital sitTone*/
			comip_lc1120_i2c_write(LC1120_R47, 0x00);
			comip_lc1120_i2c_write(LC1120_R48, 0x00);
			/*close digital sitTone*/

			comip_lc1120_i2c_write(LC1120_R04, 0x80);
			comip_lc1120_i2c_write(LC1120_R05, 0x80);
			comip_lc1120_i2c_write(LC1120_R06, 0x80);
			comip_lc1120_i2c_write(LC1120_R07, 0x80);

			/*close hp  rec side tone*/
			val = comip_lc1120_i2c_read(LC1120_R90);
			val &= 0xef;
			comip_lc1120_i2c_write(LC1120_R90, val);
			val = comip_lc1120_i2c_read(LC1120_R91);
			val &= 0x7b;/*close rec side tone*/
			comip_lc1120_i2c_write(LC1120_R91, val);
			/*close hp side tone*/
			CODEC_LC1120_PRINTKC( "colse audio loopback\n");
	}else {
		CODEC_LC1120_PRINTKC( "unknow opt!\n");
		goto error;
	}
	lc1120_clk_disable();

	return count;

error:
	CODEC_LC1120_PRINTKC( "r/w index(0x%%2x) value(0x%%2x)\n");
	lc1120_clk_disable();
	return count;
}
static ssize_t lc1120_audio_proc_read(char *page,int count,int *eof)
{

	char *buf  = page;
	char *next = buf;
	int  t;
	unsigned size = count;

	t = scnprintf(next, size, "codec i2s gain value:\n");
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_speaker: adc: %d pga: %d\n", pcodec_pri_data->i2s_ogain_speaker_daclevel, pcodec_pri_data->i2s_ogain_speaker_cdvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_receiver: adc: %d pga: %d\n", pcodec_pri_data->i2s_ogain_receiver_daclevel, pcodec_pri_data->i2s_ogain_receiver_epvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_headset: adc: %d pga: %d\n", pcodec_pri_data->i2s_ogain_headset_daclevel, pcodec_pri_data->i2s_ogain_headset_hpvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_speaker_headset: adc: %d hp_pga: %d, spk_pga: %d\n", pcodec_pri_data->i2s_ogain_spk_hp_daclevel, pcodec_pri_data->i2s_ogain_spk_hp_hpvol, pcodec_pri_data->i2s_ogain_spk_hp_cdvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_igain_main_mic: adc: %d pga: %d\n", pcodec_pri_data->i2s_igain_mic_adclevel, pcodec_pri_data->i2s_igain_mic_pgagain);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_igain_headset_mic: adc: %d pga: %d\n", pcodec_pri_data->i2s_igain_headsetmic_adclevel, pcodec_pri_data->i2s_igain_headsetmic_pgagain);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_igain_line: pga: %d\n", pcodec_pri_data->i2s_igain_line_linvol);
	size -= t;
	next += t;

	*eof = 1;
	return count - size;


}

static int lc1120_audio_proc_write(const char __user *buffer,unsigned long count)
{
	static char kbuf[200];
	char *buf = kbuf;
	unsigned int gain1, gain2, gain3;
	static char cbuf[200];
	char *cmdstring = cbuf;

	if (count >= 200)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	sscanf(buf, "%s %d %d %d", cmdstring, &gain1, &gain2, &gain3);
	CODEC_LC1120_PRINTKC("audio gain %s write gain1:%d, gain2:%d, gain3:%d\n",cmdstring, gain1, gain2, gain3);

	if (strcmp("spk",cmdstring) == 0) {
		pcodec_pri_data->i2s_ogain_speaker_daclevel = gain1;
		pcodec_pri_data->i2s_ogain_speaker_cdvol = gain2;
	} else if (strcmp("hp",cmdstring) == 0) {

		pcodec_pri_data->i2s_ogain_headset_daclevel = gain1;
		pcodec_pri_data->i2s_ogain_headset_hpvol = gain2;
	} else if (strcmp("rec",cmdstring) == 0) {
		
		pcodec_pri_data->i2s_ogain_receiver_daclevel = gain1;
		pcodec_pri_data->i2s_ogain_receiver_epvol = gain2;
	} else if (strcmp("spk_hp",cmdstring) == 0) {
			
		pcodec_pri_data->i2s_ogain_spk_hp_daclevel = gain1;
		pcodec_pri_data->i2s_ogain_spk_hp_hpvol = gain2;
		pcodec_pri_data->i2s_ogain_spk_hp_cdvol = gain3;
	} else if (strcmp("main_mic",cmdstring) == 0) {
				
		pcodec_pri_data->i2s_igain_mic_adclevel = gain1;
		pcodec_pri_data->i2s_igain_mic_pgagain = gain2;

	} else if (strcmp("hp_mic",cmdstring) == 0) {
					
		pcodec_pri_data->i2s_igain_headsetmic_adclevel = gain1;
		pcodec_pri_data->i2s_igain_headsetmic_pgagain = gain2;
	} else if (strcmp("line",cmdstring) == 0) {
						
		pcodec_pri_data->i2s_igain_line_linvol = gain1;

	} else if (strcmp("update",cmdstring) == 0) {

		lc1120_i2s_ogain_to_hw();

	}else {
		CODEC_LC1120_PRINTKC( "unknow opt!\n");
		goto error;
	}

	return count;

error:
	CODEC_LC1120_PRINTKC( "r/w index(0x%%2x) value(0x%%2x)\n");
	return count;
}


static int lc1120_codec_eq_proc_read(char* page,int count,int*eof)
{
	char* next =page;
	int t;
	int size=count;

	t=scnprintf(next,size,"lc1120 codec reg51 value is: %d\n",pcodec_pri_data->alc_reg51);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg52 value is: %d\n",pcodec_pri_data->alc_reg52);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg53 value is: %d\n",pcodec_pri_data->alc_reg53);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg54 value is: %d\n",pcodec_pri_data->alc_reg54);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg55 value is: %d\n",pcodec_pri_data->alc_reg55);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg56 value is: %d\n",pcodec_pri_data->alc_reg56);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg57 value is: %d\n",pcodec_pri_data->alc_reg57);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg58 value is: %d\n",pcodec_pri_data->alc_reg58);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg59 value is: %d\n",pcodec_pri_data->alc_reg59);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg60 value is: %d\n",pcodec_pri_data->alc_reg60);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg61 value is: %d\n",pcodec_pri_data->alc_reg61);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg62 value is: %d\n",pcodec_pri_data->alc_reg62);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg63 value is: %d\n",pcodec_pri_data->alc_reg63);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1120 codec reg64 value is: %d\n",pcodec_pri_data->alc_reg64);
	size-=t;
	next+=t;

	*eof=1;
	return count-size;
}


static int lc1120_codec_eq_proc_write(const char __user*buffer,unsigned long count)			//ylz amended 13/6/19
{
	static char kbuf[200];
	static char cbuf[200];
	char* buf=kbuf;
	char* reg_name=cbuf;
	u8 value_set;

	if(count>=200)
		return -EINVAL;
	if(copy_from_user(buf,buffer,count))
		return -EFAULT;
	sscanf(buf,"%s %x",reg_name,&value_set);
	printk(KERN_INFO" lc1120_codec_eq_proc_write: Register name:%s,Register value:0x%x\n",reg_name,value_set);

	if(strcmp("eq_r51",reg_name)==0){

		pcodec_pri_data->alc_reg51=value_set;

	}else if(strcmp("eq_r52",reg_name)==0){

		pcodec_pri_data->alc_reg52=value_set;

	}else if(strcmp("eq_r53",reg_name)==0){

		pcodec_pri_data->alc_reg53=value_set;

	}else if(strcmp("eq_r54",reg_name)==0){

		pcodec_pri_data->alc_reg54=value_set;

	}else if(strcmp("eq_r55",reg_name)==0){

		pcodec_pri_data->alc_reg55=value_set;

	}else if(strcmp("eq_r56",reg_name)==0){

		pcodec_pri_data->alc_reg56=value_set;

	}else if(strcmp("eq_r57",reg_name)==0){

		pcodec_pri_data->alc_reg57=value_set;

	}else if(strcmp("eq_r58",reg_name)==0){

		pcodec_pri_data->alc_reg58=value_set;

	}else if(strcmp("eq_r59",reg_name)==0){

		pcodec_pri_data->alc_reg59=value_set;
	
	}else if(strcmp("eq_r60",reg_name)==0){

		pcodec_pri_data->alc_reg60=value_set;

	}else if(strcmp("eq_r61",reg_name)==0){

		pcodec_pri_data->alc_reg61=value_set;

	}else if(strcmp("eq_r62",reg_name)==0){

		pcodec_pri_data->alc_reg62=value_set;

	}else if(strcmp("eq_r63",reg_name)==0){

		pcodec_pri_data->alc_reg63=value_set;

	}else if(strcmp("eq_r64",reg_name)==0){

		pcodec_pri_data->alc_reg64=value_set;

	}else if(strcmp("update",reg_name)==0){

		codec_lc1120_enable_alc(1);

	}else{
		printk("eq proc write: unknown opt!!!\n");
		goto error;
	}
	return count;
	
	error:
		return count;
}

#endif


static int codec_lc1120_set_double_mics_mode(int nValue)
{
	CODEC_LC1120_PRINTKC("codec_lc1120_set_double_mics_mode: nValue = %d",nValue);
	if(comip_lc1120_st_pri_data->pdm_flag == DMIC_BOARD_DOUBLE_MICS_MODE)
		nValue = DMIC_BOARD_DOUBLE_MICS_MODE;
	comip_lc1120_st_pri_data->pdm_flag = nValue;
}
static int codec_lc1120_get_mixer_info(struct snd_leadcore_mixer_info* _userInfo)
{
	strcpy(_userInfo->codecName,"lc1120");
	_userInfo->multiService = 0;
	CODEC_LC1120_PRINTKC("name= %s,ser=%d \n",_userInfo->codecName,_userInfo->multiService);
	return 0;
}
struct comip_codec_ops comip_codec_1120_ops = {
	.set_output_path = codec_lc1120_set_output_path,
	.set_output_volume = codec_lc1120_set_output_gain,
	.set_input_gain = codec_lc1120_set_input_gain,
	.set_output_samplerate = codec_lc1120_set_output_samplerate,
	.set_input_samplerate = codec_lc1120_set_input_samplerate,
	.set_input_mute = codec_lc1120_set_input_mute,
	.set_output_mute = codec_lc1120_set_output_mute,
	.set_input_path = codec_lc1120_set_input_path,
	.enable_voice = codec_lc1120_enable_voice,
	.enable_fm = codec_lc1120_enable_fm,
	.enable_bluetooth = codec_lc1120_enable_bluetooth,
	.mp_powersave = codec_lc1120_mp_powersave,
	.fm_out_gain = codec_lc1120_fm_line_gain,
	.calibrate_data_show = codec_lc1120_calibrate_data_show,
	.calibrate_data_store = codec_lc1120_calibrate_data_store,
	.comip_register_show = codec_lc1120_comip_register_show,
	.comip_register_store = codec_lc1120_comip_register_store,
	.set_double_mics_mode = codec_lc1120_set_double_mics_mode,
	.enable_alc = codec_lc1120_enable_alc,
#ifdef CODEC_SOC_PROC
	.codec_proc_read = lc1120_proc_read,
	.codec_proc_write = lc1120_proc_write,
	.codec_audio_proc_read = lc1120_audio_proc_read,
	.codec_audio_proc_write = lc1120_audio_proc_write,
	.codec_eq_proc_read = lc1120_codec_eq_proc_read,
	.codec_eq_proc_write = lc1120_codec_eq_proc_write,
#endif
	.codec_get_mixer_info = codec_lc1120_get_mixer_info,
};

static int lc1120_micbias_power(int onoff)
{
	return 0;
}

static int lc1120_button_active_level_set(int level)
{
	return 0;
}

static irqreturn_t lc1120_codec_isr()
{
	printk("lc1120_codec_isr >>.");
	disable_irq_nosync(comip_lc1120_st_pri_data->irq);
	int irq_type;
	irq_type = lc1120_codec_irq_type_get();
	comip_lc1120_st_pri_data->comip_switch_isr(irq_type);
	return IRQ_HANDLED;
}

static int lc1120_switch_irq_handle_done(int type)
{
	lc1120_codec_clr_irq();

	enable_irq(comip_lc1120_st_pri_data->irq);

	return 0;
}

static int lc1120_switch_irq_init(struct comip_switch_hw_info *info)
{
	int ret;
	comip_lc1120_st_pri_data->comip_switch_isr =  info->comip_switch_isr;
	comip_lc1120_st_pri_data->irq_gpio = info->irq_gpio;
	printk("SWITCH  %s:gpio:%d\n", __func__, info->irq_gpio);

	ret = gpio_request(info->irq_gpio, "comip_switch irq");
	if (ret < 0) {
		printk("%s: Failed to request GPIO.\n", __func__);
		return ret;
	}
	printk("irq gpio = %d\n",info->irq_gpio);
	ret = gpio_direction_input(info->irq_gpio);
	if (ret < 0) {
		printk("%s: Failed to detect GPIO input.\n", __func__);
		goto failed;
	}

	comip_lc1120_st_pri_data->irq = gpio_to_irq(info->irq_gpio);

	/* Request irq. */
	ret = request_irq(comip_lc1120_st_pri_data->irq, lc1120_codec_isr,
			  IRQF_TRIGGER_HIGH, "comip_switch irq", NULL);
	if (ret) {
		printk("%s: IRQ already in use.\n", __func__);
		goto failed;
	}
	enable_irq_wake(comip_lc1120_st_pri_data->irq);

	return 0;

failed:
	gpio_free(info->irq_gpio);
	return ret;
}

static int lc1120_switch_deinit()
{
	free_irq(comip_lc1120_st_pri_data->irq, NULL);
	gpio_free(comip_lc1120_st_pri_data->irq_gpio);

	return 0;
}

struct comip_switch_ops lc1120_codec_switch_ops = {
	.hw_init = lc1120_switch_irq_init,
	.hw_deinit = lc1120_switch_deinit,
	.jack_status_get = lc1120_codec_hp_status_get,
	.button_status_get = lc1120_codec_hook_status_get,
	.jack_int_mask = lc1120_codec_jack_mask,
	.button_int_mask = lc1120_codec_hook_mask,
	.button_enable = lc1120_codec_hook_enable,
	.micbias_power = lc1120_micbias_power,
	.jack_active_level_set = lc1120_codec_hp_detpol_set,
	.button_active_level_set = lc1120_button_active_level_set,
	.button_type_get = lc1120_codec_get_headphone_key_num,
	.irq_handle_done = lc1120_switch_irq_handle_done,
};

static int comip_1120_startup(struct snd_pcm_substream *substream,
                              struct snd_soc_dai *dai)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_codec *codec =rtd->codec;
	CODEC_LC1120_PRINTKB("comip_1120_startup function\n");
	//<---------------------------------------------------->
	//init audio registers
	if((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)&&(comip_lc1120_st_pri_data->fm == 0)) {
		lc1120_codec_output_path_set(4);
	}
	lc1120_init();
	return 0;
}

static void comip_1120_shutdown(struct snd_pcm_substream *substream,
                                struct snd_soc_dai *dai)
{

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		//if((comip_lc1120_st_pri_data->type_play == 0)&&(comip_lc1120_st_pri_data->fm == 0)) {
			codec_lc1120_playback_deinit();
		//}
	} else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		//if(comip_lc1120_st_pri_data->type_record == AUDIO_CAPTURE_TYPE) {
			codec_lc1120_capture_deinit();
		//}
	}

}


static int comip_1120_hw_params(struct snd_pcm_substream *substream,
                                struct snd_pcm_hw_params *params,
                                struct snd_soc_dai *dai)
{

	CODEC_LC1120_PRINTKB("comip_1120_hw_params function\n");

	//<---------------------------------------------------->
	//init input or output system registers
//    CODEC_LC1120_PRINTK("comip_1120_hw_params stream type %d\n",substream->stream);
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		codec_lc1120_playback_init();
	} else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		codec_lc1120_capture_init();
	}

	return 0;


}

static int comip_1120_set_dai_sysclk(struct snd_soc_dai *codec_dai,
                                     int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct comip_1120_priv *comip_1120 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s clk_id: %d, freq: %u, dir: %d\n", __func__,
	         clk_id, freq, dir);

	/* Anything between 256fs*8Khz and 512fs*48Khz should be acceptable
	   because the codec is slave. Of course limitations of the clock
	   master (the IIS controller) apply.
	   We'll error out on set_hw_params if it's not OK */
	if ((freq >= (256 * 8000)) && (freq <= (512 * 48000))) {
		comip_1120->sysclk = freq;
		return 0;
	}

	printk(KERN_DEBUG "%s unsupported sysclk\n", __func__);
	return -EINVAL;
}

static int comip_1120_set_dai_fmt(struct snd_soc_dai *codec_dai,
                                  unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct comip_1120_priv *comip_1120 = snd_soc_codec_get_drvdata(codec);

//	CODEC_LC1120_PRINTK("comip_1120_set_dai_fmt fmt: 0x%08X\n",fmt);


	/* We can't setup DAI format here as it depends on the word bit num */
	/* so let's just store the value for later */
	comip_1120->dai_fmt = fmt;

	return 0;
}

static int comip_1120_set_bias_level(struct snd_soc_codec *codec,
                                     enum snd_soc_bias_level level)
{


	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}
static struct snd_soc_dai_ops comip_1120_dai_ops = {
	.startup		= comip_1120_startup,
	.shutdown		= comip_1120_shutdown,
	.hw_params		= comip_1120_hw_params,
//	.digital_mute	= comip_1120_mute,
	.set_sysclk		= comip_1120_set_dai_sysclk,
	.set_fmt		= comip_1120_set_dai_fmt,
};

static struct snd_soc_dai_driver comip_1810_dai = {
	.name = "comip_hifi",
	/* playback capabilities */
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1120_RATES,
		.formats = COMIP_1120_FORMATS,
	},
	/* capture capabilities */
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1120_RATES,
		.formats = COMIP_1120_FORMATS,
	},
	/* I2S operations */
	.ops = &comip_1120_dai_ops,
};

static void load_default_calibration_data(struct comip_1120_priv* pdata)
{
	struct codec_voice_tbl *pvoice_table = NULL;
	pvoice_table = kzalloc(sizeof(struct codec_voice_tbl), GFP_KERNEL);
	if (pvoice_table == NULL) {
		CODEC_LC1120_PRINTKC( "pvoice_table kzalloc failed\n");

	}
	pdata->pvoice_table = pvoice_table;
	lc1120_load_default_pcmtbl(pvoice_table);
}

static int comip_1120_soc_probe(struct snd_soc_codec *codec)
{

	int ret;
	CODEC_LC1120_PRINTKC("comip_1120_soc_probe \n");

	lc1120_deinit();//ensure codec Reg in a right state
//	lc1120_codec_output_path_set(4);

	comip_1120_set_bias_level(codec,SND_SOC_BIAS_OFF);

	pcodec_pri_data = kzalloc(sizeof(struct comip_1120_priv), GFP_KERNEL);
	if (pcodec_pri_data == NULL)
		return -ENOMEM;
	pcodec_pri_data->pcodec = codec;
	pcodec_pri_data->i2s_ogain_speaker_daclevel = 110;
	pcodec_pri_data->i2s_ogain_speaker_cdvol = 115;
	pcodec_pri_data->i2s_ogain_headset_daclevel = 100;
	pcodec_pri_data->i2s_ogain_headset_hpvol = 100;
	pcodec_pri_data->i2s_ogain_receiver_daclevel = 100;
	pcodec_pri_data->i2s_ogain_receiver_epvol = 100;
	pcodec_pri_data->i2s_ogain_spk_hp_daclevel = 100;
	pcodec_pri_data->i2s_ogain_spk_hp_hpvol = 100;
	pcodec_pri_data->i2s_ogain_spk_hp_cdvol = 100;
	pcodec_pri_data->i2s_igain_mic_adclevel = 110;
	pcodec_pri_data->i2s_igain_mic_pgagain = 110;
	pcodec_pri_data->i2s_igain_headsetmic_adclevel = 110;
	pcodec_pri_data->i2s_igain_headsetmic_pgagain = 110;
	pcodec_pri_data->i2s_igain_line_linvol = 70;
	load_default_calibration_data(pcodec_pri_data);
	snd_soc_codec_set_drvdata(codec, pcodec_pri_data);

	#if 0
	ret = snd_soc_add_controls(codec, lc1120_snd_controls,	ARRAY_SIZE(lc1120_snd_controls));

	if (ret < 0) {
		CODEC_LC1120_PRINTKC( "comip 1120: failed to register controls\n");
		kfree(pcodec_pri_data);
		return ret;
	}
	#endif
	pcodec_pri_data->cur_volume = 3;
	//lc1120_codec_hp_detpol_set(HEADPHONE_DETECT_LEVEL);
	comip_1120_set_bias_level(codec,SND_SOC_BIAS_PREPARE);
	comip_lc1120_st_pri_data->spk_use_aux = pintdata->aux_pa_gpio;
	if(comip_lc1120_st_pri_data->spk_use_aux) {
		comip_mfp_config(comip_lc1120_st_pri_data->spk_use_aux ,MFP_PIN_MODE_GPIO);
		gpio_request(comip_lc1120_st_pri_data->spk_use_aux ,"codec_pa");
		gpio_direction_output(comip_lc1120_st_pri_data->spk_use_aux,0);
	}	
	#if defined(CONFIG_SWITCH_COMIP_CODEC)
	comip_switch_register_ops(&lc1120_codec_switch_ops);
	#endif
	return 0;
}

/* power down chip */
static int comip_1120_soc_remove(struct snd_soc_codec *codec)
{
	struct comip_1120_priv *comip_1120_codec;
	comip_1120_codec = snd_soc_codec_get_drvdata(codec);
	if(comip_1120_codec != NULL) {
		if(NULL !=comip_1120_codec->pvoice_table) {
			kfree(comip_1120_codec->pvoice_table);
			comip_1120_codec->pvoice_table = NULL;
		}
		kfree(comip_1120_codec);
	}
	comip_1120_set_bias_level(codec,SND_SOC_BIAS_OFF);
	if(comip_lc1120_st_pri_data->mclk)
		clk_put(comip_lc1120_st_pri_data->mclk);
	if(pcodec_pri_data) {
		kfree(pcodec_pri_data);
		pcodec_pri_data = NULL;
	}
	if(comip_lc1120_st_pri_data) {
		kfree(comip_lc1120_st_pri_data);
		comip_lc1120_st_pri_data = NULL;
	}
	gpio_free(comip_lc1120_st_pri_data->spk_use_aux);
	return 0;
}

#if defined(CONFIG_PM)
static int comip_1120_soc_suspend(struct snd_soc_codec *codec,
                                  pm_message_t state)
{
	#if 0
	if((comip_lc1120_st_pri_data->type_voice == 0)&&(comip_lc1120_st_pri_data->fm == 0)&&(comip_lc1120_st_pri_data->comip_codec_jackst == 0)){
		comip_1120_set_bias_level(codec,SND_SOC_BIAS_STANDBY);
		clk_disable(comip_lc1120_st_pri_data->mclk);
	}
	#endif
	return 0;
}

static int comip_1120_soc_resume(struct snd_soc_codec *codec)
{
	#if 0
	if((comip_lc1120_st_pri_data->type_voice == 0)&&(comip_lc1120_st_pri_data->fm == 0)&&(comip_lc1120_st_pri_data->comip_codec_jackst == 0)){

		comip_1120_set_bias_level(codec,SND_SOC_BIAS_ON);
		clk_enable(comip_lc1120_st_pri_data->mclk);
	}
	#endif
	return 0;
}
#else
#define comip_1120_soc_suspend NULL
#define comip_1120_soc_resume NULL
#endif /* CONFIG_PM */

static struct snd_soc_codec_driver soc_codec_dev_1120 = {
	.probe =        comip_1120_soc_probe,
	.remove =       comip_1120_soc_remove,
	.suspend =      comip_1120_soc_suspend,
	.resume =       comip_1120_soc_resume,
	.reg_cache_size = sizeof(comip_lc1120_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = comip_lc1120_reg,
	.reg_cache_step = 1,
	.read = comip_1120_read_reg_cache,
	.write = comip_1120_write,
	.set_bias_level = comip_1120_set_bias_level,
};

static int __init comip_1120_codec_init(void)
{
	CODEC_LC1120_PRINTKC("comip_1120_codec_init \n");
	leadcore_register_codec_info(&soc_codec_dev_1120,&comip_1810_dai,&comip_codec_1120_ops);
	return 0;
}

static void __exit comip_1120_codec_exit(void)
{
	CODEC_LC1120_PRINTKC("comip_1120_codec_exit \n");
}
rootfs_initcall(comip_1120_codec_init);

//early_initcall(comip_1120_codec_init);
module_exit(comip_1120_codec_exit);

MODULE_AUTHOR("Peter Tang <tangyong@leadcoretech.com>");
MODULE_DESCRIPTION("comip 1120 codec driver");
MODULE_LICENSE("GPL");

