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
 *	Version		Date		 Author		Description
 *	 1.0.0		2012-12-28 yangshunlong	created
 *
 */

#include "comip_codecs_lc1132.h"
#include <plat/mfp.h>

#define OUT_SAMPLERATE_NUM 9+1

#define CONFIG_CODEC_LC1132_PRINTK_DEBUG
#ifdef CONFIG_CODEC_LC1132_PRINTK_DEBUG
#define CODEC_LC1132_PRINTK_DEBUG(fmt, args...)  printk(KERN_DEBUG"[CODEC_LC1132]" fmt,##args)
#else
#define CODEC_LC1132_PRINTK_DEBUG(fmt, args...)
#endif
#define CONFIG_CODEC_LC1132_DEBUG_ERR
#ifdef CONFIG_CODEC_LC1132_DEBUG_ERR
#define CODEC_LC1132_PRINTK_ERR(fmt, args...)   printk(KERN_ERR"[CODEC_LC1132]" fmt, ##args)
#else
#define CODEC_LC1132_PRINTK_ERR(fmt, args...)
#endif
//#define TEST_LC1132
#ifdef TEST_LC1132
#define CODEC_LC1132_PRINTK_TEST(fmt, args...) 			printk(KERN_ERR   "[CODEC_LC1132]" fmt, ##args)
#else
#define CODEC_LC1132_PRINTK_TEST(fmt, args...)
#endif

#define CONFIG_CODEC_LC1132_DEBUG_INFO
#ifdef CONFIG_CODEC_LC1132_DEBUG_INFO
#define CODEC_LC1132_PRINTK_INFO(fmt, args...)  printk(KERN_INFO"[CODEC_LC1132]" fmt,##args)
#else
#define CODEC_LC1132_PRINTK_INFO(fmt, args...)
#endif

#define    DD_SUCCESS		 ( 0)	//success
#define    DD_FAIL			 (-1)	//operation failed

#define OUT_REC   		0x1
#define OUT_SPK   		0x2
#define OUT_HP   		0x3
#define OUT_ALL_MUTE   	0x4
#define OUT_SPK_HP   	0x5

struct comip_lc1132_device_info_struct {
	const char *name;
	struct snd_soc_codec *pcodec;
	int sysclk;
	int dai_fmt;
	
	struct work_struct codec_irq_work;
	int init_flag;
	struct clk *mclk;
	struct mutex mclk_mutex;
	unsigned long  main_clk_freq;
	
	struct clk *ext_clk;
	struct mutex ext_mutex;
	unsigned long  ext_clk_freq;
	unsigned int clk_st;
	int main_clk_st;

	struct comip_1132_audio_struct *paudio;
	struct comip_1132_voice_struct *pvoice;
	struct comip_1132_device_struct *pdevice;
	

	int comip_codec_jackst;
	int outdevice;

	int nType_audio;
	int type_play;
	int type_record;
	int type_voice;
	int fm;
	int nPath_in;
	int mic_num;
	int mic1_mmic; // If hardware make mic1 as main mic
	int spk_use_aux;
	int (*comip_switch_isr)(int type);
	int irq_gpio;
	int irq;
	
};
struct comip_lc1132_device_info_struct* lc1132_codec_info = NULL;


/*
 * codec mclk set,audio mclk=13mhz,voice mclk=2mhz
 */
#define LC1132_MCLK_ALLON

#define LC1132_MCLK_RATE_AUDIO				11289600
//#define LC1132_MCLK_RATE_AUDIO				13000000
#define LC1132_MCLK_RATE_AUDIO_AND_VOICE	13000000

#define LC1132_VOICE_USE_2048000
#ifdef LC1132_VOICE_USE_2048000
#define LC1132_MCLK_RATE_VOICE				2048000
#else
#define LC1132_MCLK_RATE_VOICE				13000000
#endif


static const struct snd_kcontrol_new lc1132_snd_controls[] = {
	SOC_SINGLE("Master Playback Volume", 0, 0, 0x3F, 1),//sample
};

/******************************************************************************
*  Private Function Declare
******************************************************************************/

/* In-data addresses are hard-coded into the reg-cache values */
static u8 comip_lc1132_reg[COMIP_1132_REGS_NUM] = {

};

static const struct CODEC_I2C_DATA_T lc1132_init_data[]= {
	 {LC1132_R87, 0x80, 0x80, 0x32} // LDO power on
	,{LC1132_R96, 0xe7, 0xff, 0x00} //power on
	,{LC1132_R85, 0x53, 0x7b, 0x00} // mmic1&mmic2&hpmic set ,default  mmic   //ERROR!!!!
};
static const struct CODEC_I2C_DATA_T lc1132_deinit_data[]= {
	 {LC1132_R87, 0x00, 0x80, 0x32} // LDO power on
	,{LC1132_R96, 0x06, 0xff, 0x00} //power on
	,{LC1132_R84, 0x00, 0x02, 0x00}//Micbias enable
	/*under reg will be set no clk when deinit*/
	,{LC1132_R15, 0x30, 0x30, 0x00}
	,{LC1132_R29, 0x0f, 0x0f, 0x00}
	,{LC1132_R32, 0x0f, 0x0f, 0x00}
	,{LC1132_R45, 0x0f, 0x0f, 0x00}
	,{LC1132_R47, 0x00, 0x80, 0x00}
};

//1 /*-----------data1 struct  start---------------*/
//1 /*pcm interface,8k-16bit samplerate,use adc 1,mic2-hpmic*/
static const struct CODEC_I2C_DATA_T lc1132_voice_pcm_slave_data1[]= {
	{LC1132_R13, 0x80, 0xff, 0x00}// pcm
	//,{LC1132_R15, 0x61, 0xff, 0x00}// pcm    // PCM clock sel MCLK
	,{LC1132_R15, 0x51, 0xff, 0x00}// pcm // PCM clock sel EXT CLK
	,{LC1132_R01, 0x28, 0x28, 0x00}// mono DAC enable ,adc1 enable
	,{LC1132_R16, 0x0a, 0x1d, 0x00}// mono DAC sel PCM left
	//,{LC1132_R32, 0x36, 0x7f, 0x00}//MONO dac SEL PCM,SEL MCLK
	,{LC1132_R32, 0x35, 0x7f, 0x00} //MONO dac SEL PCM,SEL EXT CLK
	,{LC1132_R29, 0x00, 0x10, 0x00}//reset ADC1
	//,{LC1132_R29, 0x36, 0x7f, 0x00}//ADC1 OSR128,ADC1SEL PCM,ADC SEL MCLK
	,{LC1132_R29, 0x35, 0x7f, 0x00}//ADC1 OSR128,ADC1SEL PCM,ADC SEL EXT CLK
	,{LC1132_R19, 0xc0, 0xc0, 0x00}//ADC1 HP enable
	,{LC1132_R22, 0xe0, 0xe0, 0x00}//ADC1 HP CUT 
	//,{LC1132_R45, 0x36, 0x7f, 0x00}//adc2 from i2s,clk sel mclk //1132 BUG!!!!
	,{LC1132_R45, 0x35, 0x7f, 0x00}//adc2 from i2s,clk sel ext  clk //1132 BUG!!!!
	,{LC1132_R82, 0x04, 0x05, 0x00}// adc1 sel mmic2&hpmic
	,{LC1132_R0,  0x50, 0x70, 0x00}//Mic2 PAG enable
	,{LC1132_R84, 0x03, 0x03, 0x00}//Micbias enable
};

static const struct CODEC_I2C_DATA_T lc1132_voice_pcm_slave_data1_close[]= {
	{LC1132_R13, 0xbf, 0xff, 0x00}// pcm
	,{LC1132_R15, 0x72, 0xff, 0x00}// pcm
	,{LC1132_R01, 0x00, 0x28, 0x00}// mono DAC enable ,adc1 enable
	,{LC1132_R16, 0x1c, 0x1c, 0x00}// mono DAC sel PCM left,
	,{LC1132_R32, 0x0f, 0x0f, 0x00}//MONO dac SEL PCM,SEL MCLK
	,{LC1132_R29, 0x00, 0x10, 0x00}//reset ADC1
	,{LC1132_R29, 0x3f, 0x7f, 0x00}//ADC1 OSR128,ADC1SEL noClk,ADC SEL noClk
	,{LC1132_R19, 0x00, 0xc0, 0x00}//ADC1 HP disable
	,{LC1132_R22, 0x00, 0xe0, 0x00}//ADC1 HP CUT
	,{LC1132_R45, 0x3f, 0x7f, 0x00}//adc2 from i2s,clk sel mclk //1132 BUG!!!!
	,{LC1132_R82, 0x01, 0x05, 0x00}// adc1 un sel mmic2&hpmic
	//,{LC1132_R84, 0x02, 0x02, 0x00}//Micbias enable
};
/*-----------data1 struct  end---------------*/


//1 /*-----------data2 struct  start---------------*/
//1 /*pcm interface,8k-32bit samplerate,use adc1&adc2,mic1&mic2-hpmic*/
static const struct CODEC_I2C_DATA_T lc1132_voice_pcm_slave_data2[]= {
	{LC1132_R13, 0xc4, 0xff, 0x00}// pcm double-channel,sel adc1&adc2
	//,{LC1132_R15, 0x61, 0xff, 0x00}// pcm sel mclk ,16FS
	,{LC1132_R15, 0x53, 0xff, 0x00}//   PCM clock sel EXT CLK
	,{LC1132_R01, 0x38, 0x38, 0x00}// mono DAC enable ,adc1 enable,adc2 enable
	,{LC1132_R16, 0x0a, 0x1d, 0x00}// mono DAC sel PCM left,
	//,{LC1132_R32, 0x36, 0x7f, 0x00}//MONO dac SEL PCM,SEL MCLK
	,{LC1132_R32, 0x35, 0x7f, 0x00} //MONO dac SEL PCM,SEL EXT CLK
	,{LC1132_R29, 0x00, 0x10, 0x00}//reset ADC1
	//,{LC1132_R29, 0x36, 0x7f, 0x00}//ADC1 OSR128,ADC1 SEL PCM,ADC2 SEL MCLK
	,{LC1132_R29, 0x35, 0x7f, 0x00}//ADC1 OSR128,ADC1SEL PCM,ADC SEL EXT CLK
	,{LC1132_R19, 0xc0, 0xc0, 0x00}//ADC1 HP enable
	,{LC1132_R22, 0x60, 0xe0, 0x00}//ADC1 HP CUT 
	,{LC1132_R45, 0x00, 0x10, 0x00}//reset ADC2
	//,{LC1132_R45, 0x36, 0x7f, 0x00}//ADC2 OSR128,ADC2 SEL PCM,ADC2 SEL MCLK
	,{LC1132_R45, 0x35, 0x7f, 0x00}//adc2 from i2s,clk sel ext  clk 
	,{LC1132_R35, 0xc0, 0xc0, 0x00}//ADC2 HP enable
	,{LC1132_R38, 0xe0, 0xe0, 0x00}//ADC2 HP CUT 
	,{LC1132_R82, 0x04, 0x05, 0x00}// adc1 sel mmic2&hpmic 
	,{LC1132_R83, 0x02, 0x03, 0x00}// adc2 sel mmic1
	,{LC1132_R85, 0x73, 0x1b, 0x00} // mmic1&mmic2&hpmic set ,default  mmic  
	,{LC1132_R84, 0x03, 0x03, 0x00}//Micbias enable,Micbias switch enable
	,{LC1132_R0,  0x55, 0x77, 0x00}//Mic2 PGA enable,MIC1 PGA enable
};
static const struct CODEC_I2C_DATA_T lc1132_voice_pcm_slave_data2_close[]= {
	{LC1132_R13, 0xbf, 0xff, 0x00}// pcm double-channel,sel adc1&adc2
	,{LC1132_R15, 0x72, 0xff, 0x00}// pcm sel mclk ,16FS
	,{LC1132_R01, 0x00, 0x38, 0x00}// mono DAC enable ,adc1 enable,adc2 enable
	,{LC1132_R16, 0x78, 0x78, 0x00}// mono DAC sel PCM left, dac mute
	,{LC1132_R32, 0x0f, 0x0f, 0x00}//MONO dac SEL PCM,SEL MCLK
	,{LC1132_R29, 0x00, 0x10, 0x00}//reset ADC1
	,{LC1132_R29, 0x3f, 0x7f, 0x00}//ADC1 OSR128,ADC1 SEL PCM,ADC2 SEL MCLK
	,{LC1132_R19, 0x00, 0xc0, 0x00}//ADC1 HP disable 
	,{LC1132_R22, 0x00, 0xe0, 0x00}//ADC1 HP CUT 
	,{LC1132_R45, 0x00, 0x10, 0x00}//reset ADC2
	,{LC1132_R45, 0x3f, 0x7f, 0x00}//ADC2 OSR128,ADC2 SEL PCM,ADC2 SEL MCLK
	,{LC1132_R35, 0x00, 0xc0, 0x00}//ADC2 HP disable
	,{LC1132_R39, 0x80, 0x80, 0x00}//ADC2 mute
	,{LC1132_R38, 0x00, 0xe0, 0x00}//ADC2 HP CUT 
	,{LC1132_R82, 0x00, 0x04, 0x00}// adc1 sel mmic2&hpmic 
	,{LC1132_R83, 0x00, 0x02, 0x00}// adc2 sel mmic1
	//,{LC1132_R84, 0x03, 0x03, 0x00}//Micbias enable,Micbias switch enable
};
/*-----------data2 struct  end---------------*/
//1 /*-----------data3 struct  start---------------*/
//1 /*pcm ingerface,8k-16bit samplerate,use adc 1,mic1-hpmic*/
static const struct CODEC_I2C_DATA_T lc1132_voice_pcm_slave_data3[]= {
	{LC1132_R13, 0x80, 0xff, 0x00}// pcm
	//,{LC1132_R15, 0x61, 0xff, 0x00}// pcm    // PCM clock sel MCLK
	,{LC1132_R15, 0x51, 0xff, 0x00}// pcm // PCM clock sel EXT CLK
	,{LC1132_R01, 0x28, 0x28, 0x00}// mono DAC enable ,adc1 enable
	,{LC1132_R16, 0x0a, 0x1d, 0x00}// mono DAC sel PCM left
	//,{LC1132_R32, 0x36, 0x7f, 0x00}//MONO dac SEL PCM,SEL MCLK
	,{LC1132_R32, 0x35, 0x7f, 0x00} //MONO dac SEL PCM,SEL EXT CLK
	,{LC1132_R29, 0x00, 0x10, 0x00}//reset ADC1
	//,{LC1132_R29, 0x36, 0x7f, 0x00}//ADC1 OSR128,ADC1SEL PCM,ADC SEL MCLK
	,{LC1132_R29, 0x35, 0x7f, 0x00}//ADC1 OSR128,ADC1SEL PCM,ADC SEL EXT CLK
	,{LC1132_R19, 0xc0, 0xc0, 0x00}//ADC1 HP enable
	,{LC1132_R22, 0xe0, 0xe0, 0x00}//ADC1 HP CUT 
	//,{LC1132_R45, 0x36, 0x7f, 0x00}//adc2 from i2s,clk sel mclk //1132 BUG!!!!
	,{LC1132_R45, 0x35, 0x7f, 0x00}//adc2 from i2s,clk sel ext  clk //1132 BUG!!!!
	,{LC1132_R82, 0x02, 0x03, 0x00}// adc1 sel mmic1
	,{LC1132_R0,  0x05, 0x07, 0x00}//Mic1 PAG enable
	,{LC1132_R84, 0x03, 0x03, 0x00}//Micbias enable
};

static const struct CODEC_I2C_DATA_T lc1132_voice_pcm_slave_data3_close[]= {
	{LC1132_R13, 0xbf, 0xff, 0x00}// pcm
	,{LC1132_R15, 0x72, 0xff, 0x00}// pcm
	,{LC1132_R01, 0x00, 0x28, 0x00}// mono DAC enable ,adc1 enable
	,{LC1132_R16, 0x1c, 0x1c, 0x00}// mono DAC sel PCM left,
	,{LC1132_R32, 0x0f, 0x0f, 0x00}//MONO dac SEL PCM,SEL MCLK
	,{LC1132_R29, 0x00, 0x10, 0x00}//reset ADC1
	,{LC1132_R29, 0x3f, 0x7f, 0x00}//ADC1 OSR128,ADC1SEL noClk,ADC SEL noClk
	,{LC1132_R19, 0x00, 0xc0, 0x00}//ADC1 HP disable
	,{LC1132_R22, 0x00, 0xe0, 0x00}//ADC1 HP CUT
	,{LC1132_R45, 0x3f, 0x7f, 0x00}//adc2 from i2s,clk sel mclk //1132 BUG!!!!
	,{LC1132_R82, 0x01, 0x07, 0x00}// adc1 un sel mic1 & mmic2&hpmic
	//,{LC1132_R84, 0x02, 0x02, 0x00}//Micbias enable
};
/*-----------data1 struct  end---------------*/


/*----------Audio data1 struct  start---------------*/
/*i2s interface, 44.1k-32bit samplerate,use dac1&dac2*/
static struct CODEC_I2C_DATA_T lc1132_audio_i2s_slave_data1[]= {
	 {LC1132_R10, 0x00, 0x40, 0x00}//i2s,reset
	,{LC1132_R10, 0x61, 0x7f, 0x00}//i2s,clk sel mclk,32FS,master mode
	,{LC1132_R11, 0x01, 0x3f, 0x00}//i2s word length set 16bit,letf justified
};
static struct CODEC_I2C_DATA_T lc1132_audio_i2s_slave_data1_close[]= {
	{LC1132_R10, 0x00, 0x40, 0x00}//i2s,reset
	,{LC1132_R10, 0x72, 0x7f, 0x00}//i2s,set default
	,{LC1132_R11, 0x0a, 0x3f, 0x00}//i2s set default
};

static struct CODEC_I2C_DATA_T lc1132_playback_i2s_slave_data1[]= {
	 {LC1132_R12, 0x00, 0x0a, 0x00}//i2s
	,{LC1132_R46, 0x04, 0xfc, 0x00}//dac sel i2s left/right channel
	,{LC1132_R71, 0x00, 0x10, 0x00}//dac reset
	,{LC1132_R71, 0x32, 0xff, 0x00}//dac ovs 128x,from i2s,dac clk from mclk
	,{LC1132_R01, 0xc0, 0xc0, 0x00}//dac R/L enable
};
static struct CODEC_I2C_DATA_T lc1132_playback_i2s_slave_data1_close[]= {
	{LC1132_R12, 0x00, 0x0a, 0x00}//i2s
	,{LC1132_R46, 0xfc, 0xfc, 0x00}//dac sel i2s zero
	,{LC1132_R71, 0x00, 0x10, 0x00}//dac reset
	,{LC1132_R71, 0x3f, 0xfc, 0x00}//dac ovs 128x,from no clk,dac clk from mclk
	,{LC1132_R01, 0x00, 0xc0, 0x00}//dac R/L enable
};

static struct CODEC_I2C_DATA_T lc1132_capture_i2s_slave_data1[]= {
	 {LC1132_R12, 0x00, 0xf5, 0x00}//i2s L/R sel adc1
	,{LC1132_R29, 0x00, 0x10, 0x00}//adc1 reset
	,{LC1132_R29, 0x32, 0x7f, 0x00}//adc1 from i2s,clk sel mclk
	,{LC1132_R45, 0x32, 0x7f, 0x00}//adc2 from i2s,clk sel mclk //1132 BUG
	,{LC1132_R82, 0x04, 0x05, 0x00}//adc1 sel mic2&hpmic
	,{LC1132_R01, 0x08, 0x08, 0x00} //adc1 enable
	,{LC1132_R84, 0x03, 0x03, 0x00}//Micbias enable  AAAAAAAACCCCCCCTTTTTTTTIIIIIIIOOOONNN
	,{LC1132_R0,  0x50, 0x70, 0x00}//Mic2 PAG enable
};
static struct CODEC_I2C_DATA_T lc1132_capture_i2s_slave_data1_close[]= {
	 {LC1132_R12, 0xf0, 0xf5, 0x00}//i2s L/R sel  adc zero
	,{LC1132_R45, 0x00, 0x10, 0x00}//adc2 reset
	,{LC1132_R45, 0x3f, 0x7f, 0x00}//adc2 from i2s no LRCK,clk sel noclk
	,{LC1132_R83, 0x01, 0x05, 0x00}//adc2 unsel mic2&hpmic
	//,{LC1132_R84, 0x02, 0x02, 0x00}//Micbias enable	
	,{LC1132_R0, 0x20, 0x70, 0x00}//Mic2 PAG  dis able
};
/*----------Audio data1 struct  end---------------*/

/*----------Path data struct  start--------------*/
static struct CODEC_I2C_DATA_T lc1132_rec_output_path_open_data[]= {
	{LC1132_R91, 0x80, 0xc0, 0x00} //rec mixer enable ,rec mixer un mute
   ,{LC1132_R05, 0x60, 0xe0, 0x00} //Rec mute,EPZCEN,EP power on
};
static struct CODEC_I2C_DATA_T lc1132_rec_output_path_close_data[]= {
	{LC1132_R91, 0x40, 0xc0, 0x00} //rec mixer disable ,rec mixer mute
	,{LC1132_R05, 0x80, 0xe0, 0x00} //Rec mute,EPZCEN,EP power on
};

static struct CODEC_I2C_DATA_T lc1132_audio_rec_path_sel_data[]= {
	{LC1132_R91, 0x18, 0x18, 0x00} //dac L/R sel rec
};
static struct CODEC_I2C_DATA_T lc1132_audio_rec_path_unsel_data[]= {
	{LC1132_R91, 0x00, 0x18, 0x00} //dac L/R unsel rec
};

static struct CODEC_I2C_DATA_T lc1132_voice_rec_path_sel_data[]= {
	{LC1132_R91, 0x20, 0x20, 0x00} //mono dac  sel rec
};
static struct CODEC_I2C_DATA_T lc1132_voice_rec_path_unsel_data[]= {
	{LC1132_R91, 0x00, 0x20, 0x00} //mono dac  sel rec
};
//Receiver  end <<<<<<<<<<<<<<<<<<<<<<<<
static struct CODEC_I2C_DATA_T lc1132_spk_output_path_open_data[]= {
	{LC1132_R08, 0x40, 0xc0, 0x00} //Class D unmute,SPKZCEN
   ,{LC1132_R93, 0x00, 0xff, 0x00} //Class D CTL
   ,{LC1132_R95, 0xc4, 0xc4, 0x00} //Class D dead time set 13ns //bit 3 is 1132 bug  //c0 to c4
   ,{LC1132_R87, 0x40, 0x60, 0x00} // SPK Mixer Enable,SPK Mixer unmute
};
static struct CODEC_I2C_DATA_T lc1132_spk_output_path_close_data[]= {
	 {LC1132_R08, 0x80, 0xc0, 0x00} //Class D mute,SPKZCEN disable
	,{LC1132_R93, 0x0f, 0xff, 0x00} //Class D CTL
	,{LC1132_R95, 0x40, 0xc0, 0x00} //Class D dead time set 13ns
	,{LC1132_R87, 0x20, 0x60, 0x00} // SPK Mixer disable,SPK Mixer mute
};

static struct CODEC_I2C_DATA_T lc1132_audio_spk_path_sel_data[]= {
	{LC1132_R87, 0x03, 0x03, 0x00} //dac L/R sel spk
};
static struct CODEC_I2C_DATA_T lc1132_audio_spk_path_unsel_data[]= {
	{LC1132_R87, 0x00, 0x03, 0x00} //dac L/R unsel spk
};

static struct CODEC_I2C_DATA_T lc1132_voice_spk_path_sel_data[]= {
	{LC1132_R87, 0x04, 0x04, 0x00} //dac mono sel spk
};
static struct CODEC_I2C_DATA_T lc1132_voice_spk_path_unsel_data[]= {
	{LC1132_R87, 0x00, 0x04, 0x00} //mono dac  sel spk
};
static struct CODEC_I2C_DATA_T lc1132_line_spk_path_sel_data[]= {
	{LC1132_R87, 0x18, 0x18, 0x00} //line L/R sel spk
};
static struct CODEC_I2C_DATA_T lc1132_line_spk_path_unsel_data[]= {
	{LC1132_R87, 0x00, 0x18, 0x00} //line L/R sel spk
};
//SPK end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<
static struct CODEC_I2C_DATA_T lc1132_hp_output_path_open_data[]= {
	{LC1132_R89, 0x40, 0x60, 0x00} // hp L mixer enable ,unmute
	,{LC1132_R90, 0x40, 0x60, 0x00} // hp R mixer enable ,unmute
	,{LC1132_R92, 0xde, 0xff, 0x05}  // class G CTL
	,{LC1132_R06, 0x60, 0xe0, 0x00} //HP letf  unmute, zero crossing enable, power on
	,{LC1132_R07, 0x60, 0xe0, 0x00} //HP right unmute, zero crossing enable, power on

};
static struct CODEC_I2C_DATA_T lc1132_hp_output_path_close_data[]= {
	{LC1132_R06, 0x80, 0xe0, 0x00} //HP letf  mute, zero crossing disable, power down
	,{LC1132_R07, 0x80, 0xe0, 0x00} //HP right mute, zero crossing disable, power down
	,{LC1132_R92, 0xd0, 0xff, 0x00}  // class G CTL
	,{LC1132_R89, 0x20, 0x60, 0x00} // hp left mixer disable ,mute
	,{LC1132_R90, 0x20, 0x60, 0x00} // hp Right  mixer disable,mute
};

static struct CODEC_I2C_DATA_T lc1132_audio_hp_path_sel_data[]= {
	{LC1132_R89, 0x04, 0x04, 0x00} //dac L  sel hp L
   ,{LC1132_R90, 0x02, 0x02, 0x00} //dac R  sel hp R
};
static struct CODEC_I2C_DATA_T lc1132_audio_hp_path_unsel_data[]= {
	 {LC1132_R89, 0x00, 0x04, 0x00} //dac L  sel hp L
	,{LC1132_R90, 0x00, 0x02, 0x00} //dac R  sel hp R
};

static struct CODEC_I2C_DATA_T lc1132_voice_hp_path_sel_data[]= {
	{LC1132_R89, 0x08, 0x08, 0x00} //dac mono  sel hp L
	,{LC1132_R90, 0x08, 0x08, 0x00} //dac mono  sel hp R
};
static struct CODEC_I2C_DATA_T lc1132_voice_hp_path_unsel_data[]= {
	{LC1132_R89, 0x00, 0x08, 0x00} //dac mono  unsel hp L
	,{LC1132_R90, 0x00, 0x08, 0x00} //dac mono unsel hp R
};
static struct CODEC_I2C_DATA_T lc1132_line_hp_path_sel_data[]= {
	{LC1132_R89, 0x10, 0x10, 0x00} //line sel hp L
	,{LC1132_R90, 0x10, 0x10, 0x00} //line sel hp R
};
static struct CODEC_I2C_DATA_T lc1132_line_hp_path_unsel_data[]= {
	{LC1132_R89, 0x00, 0x10, 0x00} //line sel hp L
	,{LC1132_R90, 0x00, 0x10, 0x00} //line sel hp R
};
// headphone end <<<<<<<<<<<<<<<<<<<<<<<<<<<<
static struct CODEC_I2C_DATA_T lc1132_aux_output_path_open_data[]= {
	{LC1132_R86, 0x40, 0x60, 0x00} //Aux mixer enable, unmute
};
static struct CODEC_I2C_DATA_T lc1132_aux_output_path_close_data[]= {
	{LC1132_R86, 0x20, 0x60, 0x00} //Aux mixerdisable, mute
};

static struct CODEC_I2C_DATA_T lc1132_audio_aux_path_sel_data[]= {
	{LC1132_R86, 0x03, 0x03, 0x00} //dac L/R sel aux
};
static struct CODEC_I2C_DATA_T lc1132_audio_aux_path_unsel_data[]= {
	{LC1132_R86, 0x00, 0x03, 0x00} //dac L/R unsel aux
};

static struct CODEC_I2C_DATA_T lc1132_voice_aux_path_sel_data[]= {
	{LC1132_R86, 0x04, 0x04, 0x00} //dac mono  sel aux
};
static struct CODEC_I2C_DATA_T lc1132_voice_aux_path_unsel_data[]= {
	{LC1132_R86, 0x00, 0x04, 0x00} //dac mono  unsel aux
};
static struct CODEC_I2C_DATA_T lc1132_line_aux_path_sel_data[]= {
	{LC1132_R86, 0x18, 0x18, 0x00} //line L/R sel aux
};
static struct CODEC_I2C_DATA_T lc1132_line_aux_path_unsel_data[]= {
	{LC1132_R86, 0x00, 0x18, 0x00} //line L/R unsel aux
};
//AUX end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<
static struct CODEC_I2C_DATA_T lc1132_hpmic_input_path_sel_data[]= {
	{LC1132_R85, 0x04, 0x04, 0x00} // sel hp mic
};
static struct CODEC_I2C_DATA_T lc1132_mic2_input_path_sel_data[]= {
	{LC1132_R85, 0x00, 0x04, 0x00} // sel main mic
};

//1 Following two structs is for mic1 as main mic
static struct CODEC_I2C_DATA_T lc1132_mic1_to_hpmic_input_path_sel_data[]= {

	 {LC1132_R0, 0x50, 0x70, 0x00} // mic2 PGA enable
	,{LC1132_R82, 0x04, 0x07, 0x00} // adc1 sel mic2
	,{LC1132_R85, 0x04, 0x04, 0x00} // sel hp mic
};
static struct CODEC_I2C_DATA_T lc1132_hpmic_to_mic1_input_path_sel_data[]= {
	{LC1132_R84, 0x02, 0x02, 0x00} // micbais enable
	,{LC1132_R0, 0x05, 0x07, 0x00} // mic1 PGA enable
	,{LC1132_R82, 0x02, 0x07, 0x00} // adc1 sel mic1
};

/*----------Path data struct  end---------------*/
/*----------Gain data struct  start---------------*/
static struct CODEC_I2C_DATA_T lc1132_dac_stereo_output_gain_set_data[]= {
	{LC1132_R49, 0x00, 0x7f, 0x00}
	,{LC1132_R50, 0x00, 0x7f, 0x00}
};
static struct CODEC_I2C_DATA_T lc1132_dac_mono_output_gain_set_data[]= {
	{LC1132_R30, 0x00, 0x7f, 0x00}
};
static struct CODEC_I2C_DATA_T lc1132_line_in_output_gain_set_data[]= {
	{LC1132_R02, 0x00, 0x7f, 0x00}
	,{LC1132_R03, 0x00, 0x7f, 0x00}
};
static struct CODEC_I2C_DATA_T lc1132_rec_output_gain_set_data[]= {
	{LC1132_R05, 0x00, 0x1f, 0x00}
};
static struct CODEC_I2C_DATA_T lc1132_spk_output_gain_set_data[]= {
	{LC1132_R08, 0x00, 0x1f, 0x00}
};
static struct CODEC_I2C_DATA_T lc1132_hp_output_gain_set_data[]= {
	{LC1132_R06, 0x00, 0x1f, 0x00}
	,{LC1132_R07, 0x00, 0x1f, 0x00}
};
static struct CODEC_I2C_DATA_T lc1132_mic1_input_gain_set_data[]= {
	{LC1132_R24, 0x00, 0x3f, 0x00} //mic1
};

static struct CODEC_I2C_DATA_T lc1132_mic2_input_gain_set_data[]= {
	{LC1132_R40, 0x00, 0x3f, 0x00} //mic2
};
static struct CODEC_I2C_DATA_T lc1132_adc1_input_gain_set_data[]= {
	{LC1132_R23, 0x00, 0x7f, 0x00} //ADC1 voice
};
static struct CODEC_I2C_DATA_T lc1132_adc2_input_gain_set_data[]= {
	{LC1132_R39, 0x00, 0x7f, 0x00} //ADC2  capture
};

/*----------Gain data struct  end---------------*/
/*----------Line in struct start ---------------*/

static struct CODEC_I2C_DATA_T lc1132_fm_on_data[]= {
	{LC1132_R01, 0x06, 0x06, 0x00} //line enable
	,{LC1132_R02, 0x40, 0xc0, 0x00} //Line unmute, ZC enable
	,{LC1132_R03, 0x40, 0xc0, 0x00} //Line unmute, ZC enable
};
static struct CODEC_I2C_DATA_T lc1132_fm_off_data[]= {
	{LC1132_R01, 0x00, 0x06, 0x00} //line disable
	,{LC1132_R02, 0x80, 0xc0, 0x00} //Line mute, ZC disable
	,{LC1132_R03, 0x80, 0xc0, 0x00} //Line mute, ZC disable
};
/*----------Line in struct end---------------*/

static u8 lc1132_i2c_read(u8 reg, u8 *value)
{
	lc1132_reg_read(reg,value);
	return *value;
}

static int lc1132_i2c_write(u8 reg,u8 value)
{
	lc1132_reg_write(reg, value);
	return 0;
}

static int lc1132_i2c_bit_write(u8 reg, u8 mask,u8 value)
{
	CODEC_LC1132_PRINTK_TEST("reg:%d(mask 0x%02x) = 0x%02x \n", reg, mask, value);
	lc1132_reg_bit_write(reg, mask, value);
	return 0;
}
static u8 comip_lc1132_i2c_read(u8 reg)
{
	u8 value;
	lc1132_i2c_read(reg+0x9c,&value);
	return value;
}
static int comip_lc1132_i2c_write(u8 reg,u8 value)
{
	CODEC_LC1132_PRINTK_TEST("reg:%d = 0x%02x \n",reg,value);
	lc1132_i2c_write(reg+0x9c, value);
	return 0;
}
static int comip_lc1132_reg_bit_write(u8 reg,u8 bit, u8 nValue)
{
	u8 nData;
	nData = comip_lc1132_i2c_read(reg);
	nData &=(~bit);
	if(nValue)
		nData |= bit;
	comip_lc1132_i2c_write(reg, nData);
	return 0;
}
/*
 * The codec has no support for reading its registers except for peak level...
 */
static inline unsigned int comip_1132_read_reg_cache(struct snd_soc_codec *codec,
                u8 reg)
{
	u8 *cache = codec->reg_cache;

	if (reg >= COMIP_1132_REGS_NUM)
		return -1;
	return cache[reg];
}

/*
 * Write the register cache
 */
static inline void comip_1132_write_reg_cache(struct snd_soc_codec *codec,
                u8 reg, unsigned int value)
{
	u8 *cache = codec->reg_cache;

	if (reg >= COMIP_1132_REGS_NUM)
		return;
	cache[reg] = value;
}

static int comip_1132_write(struct snd_soc_codec *codec, unsigned int reg,
                            unsigned int value)
{
	int ret=0;
	CODEC_LC1132_PRINTK_TEST("reg: %02X, value:%02X\n",reg, value);

	if (reg >= COMIP_1132_REGS_NUM) {
		CODEC_LC1132_PRINTK_ERR( "unknown register: reg: %u", reg);
		return -EINVAL;
	}

	if(codec == NULL) {
		CODEC_LC1132_PRINTK_ERR( "unknown codec");
		return 0;
	}
	ret = comip_lc1132_i2c_write((u8)value, (u8)reg);
	if (ret != 0)
		return -EIO;

	return 0;
}

int comip_lc1132_registers_write(struct CODEC_I2C_DATA_T *pInitHandle,int nLength)
{
	int nRst,i;
	u8 nData;

	for( i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( "comip_lc1132_registers_write read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;

		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( "comip_lc1132_registers_write: write reg error\n");
		}
	}
	return 0;
}
int lc1132_audio_clk_enable(int nValue)
{
	int err = 0;
	mutex_lock(&lc1132_codec_info->mclk_mutex);

	if(nValue) {
		if(lc1132_codec_info->main_clk_freq != LC1132_MCLK_RATE_AUDIO) {
			CODEC_LC1132_PRINTK_TEST("1132 CLK RATE SET : 11.2986 \n");
			err = clk_set_rate(lc1132_codec_info->mclk, LC1132_MCLK_RATE_AUDIO);
			if (err < 0) {
				CODEC_LC1132_PRINTK_ERR( "lc1132_clk_set_rate error %d\n",err);
				return err;
			}
		}
		
		CODEC_LC1132_PRINTK_TEST("1132 CLK RATE SET : 11.2986 enable \n");
		err = clk_enable(lc1132_codec_info->mclk);
		if (err < 0) {
			CODEC_LC1132_PRINTK_ERR( "lc1132_clk_enable error %d\n",err);
			return err;
		}
		mdelay(5);
	} else {
		lc1132_codec_info->main_clk_freq = 0;
		clk_disable(lc1132_codec_info->mclk);
	}

	mutex_unlock(&lc1132_codec_info->mclk_mutex);

	return 0;
}
int lc1132_voice_clk_enable(int nValue)
{
	int err;
	mutex_lock(&lc1132_codec_info->ext_mutex);
	comip_mfp_config(MFP_PIN_GPIO(88), MFP_PIN_MODE_1);

	if(nValue) {
		if(lc1132_codec_info->ext_clk_freq != LC1132_MCLK_RATE_VOICE) {
			printk("1132 CLK RATE SET : 11.2986 \n");
			err = clk_set_rate(lc1132_codec_info->ext_clk, LC1132_MCLK_RATE_VOICE);
			if (err < 0) {
				CODEC_LC1132_PRINTK_ERR( "lc1132_clk_set_rate error %d\n",err);
				return err;
			}
		}
		
		CODEC_LC1132_PRINTK_TEST("1132 CLK RATE SET : 11.2986 enable \n");
		err = clk_enable(lc1132_codec_info->ext_clk);
		if (err < 0) {
			CODEC_LC1132_PRINTK_ERR( "lc1132_clk_enable error %d\n",err);
			return err;
		}
		mdelay(5);
	} else {
		lc1132_codec_info->ext_clk_freq = 0;
		clk_disable(lc1132_codec_info->ext_clk);
	}
	mutex_unlock(&lc1132_codec_info->ext_mutex);

	return 0;
}
static int lc1132_init(void)
{
	struct CODEC_I2C_DATA_T *pInitHandle;
	int nRst = 0;
	int nLength =0;
	pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_init_data;
	nLength =  ARRAY_SIZE(lc1132_init_data);
	
	CODEC_LC1132_PRINTK_TEST("init start\n");
	
	if(lc1132_codec_info->init_flag)
		return nRst;
	nRst = comip_lc1132_registers_write(pInitHandle,ARRAY_SIZE(lc1132_init_data));

	lc1132_codec_info->init_flag = 1;
	return nRst;
}
static int lc1132_deinit(void)
{
	struct CODEC_I2C_DATA_T *pInitHandle;
	int nRst;
	int nLength =0;
	lc1132_codec_info->init_flag = 0;
	pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_deinit_data;
	nLength =  ARRAY_SIZE(lc1132_deinit_data);

	CODEC_LC1132_PRINTK_TEST("lc1132_deinit deinit\n");
	nRst = comip_lc1132_registers_write(pInitHandle,ARRAY_SIZE(lc1132_deinit_data));
	return nRst;

}
static int lc1132_audio_init(void)
{
	
	struct CODEC_I2C_DATA_T *pInitHandle;
	int nRst = 0;
	pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_audio_i2s_slave_data1;
	nRst = comip_lc1132_registers_write(pInitHandle,ARRAY_SIZE(lc1132_audio_i2s_slave_data1));
	CODEC_LC1132_PRINTK_TEST("lc1132_audio_init end\n");
	return nRst;
}
static int lc1132_codec_open(int nType)
{
	struct CODEC_I2C_DATA_T *pInitHandle = NULL;
	int nRst,nLength = 0;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_open: nType=%d,play:%d,voice:%d,capture:%d,fm:%d,jack:%d,init:%d;\n",\
		nType,lc1132_codec_info->type_play,lc1132_codec_info->type_voice,lc1132_codec_info->type_record,\
		lc1132_codec_info->fm,lc1132_codec_info->comip_codec_jackst,lc1132_codec_info->init_flag);

	lc1132_init();

	if(VOICE_TYPE == nType) {
		if(lc1132_codec_info->mic_num == SINGLE_MIC) {/*2.048M mclk,8ksamplerate,single mic*/
			if(lc1132_codec_info->mic1_mmic) {
				pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_voice_pcm_slave_data3;
				nLength = ARRAY_SIZE(lc1132_voice_pcm_slave_data3);
			} else {
				pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_voice_pcm_slave_data1;
				nLength = ARRAY_SIZE(lc1132_voice_pcm_slave_data1);
			}
		} else if(lc1132_codec_info->mic_num == DOUBLE_MICS) {/*2.048M mclk,8ksamplerate,double mics*/
			pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_voice_pcm_slave_data2;
			nLength = ARRAY_SIZE(lc1132_voice_pcm_slave_data2);
		}
	} else if(AUDIO_TYPE == nType) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_playback_i2s_slave_data1;
		nLength = ARRAY_SIZE(lc1132_playback_i2s_slave_data1);
	} else if (AUDIO_CAPTURE_TYPE == nType) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_capture_i2s_slave_data1;
		nLength = ARRAY_SIZE(lc1132_capture_i2s_slave_data1);
	} else {
		CODEC_LC1132_PRINTK_ERR("lc1132_codec_open:error \n");
		return 0;
	}
	nRst = comip_lc1132_registers_write(pInitHandle,nLength);

	CODEC_LC1132_PRINTK_TEST("nType=%d,end\n",nType);
	return nRst;
}
static int lc1132_codec_close(int nType)
{
	struct CODEC_I2C_DATA_T *pInitHandle = NULL;
	int nRst,nLength = 0;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_close: nType=%d,play:%d,voice:%d,capture:%d,fm:%d,jack:%d;\n",\
		nType,lc1132_codec_info->type_play,lc1132_codec_info->type_voice,lc1132_codec_info->type_record,\
		lc1132_codec_info->fm,lc1132_codec_info->comip_codec_jackst);

	lc1132_codec_output_path_set(nType,DD_OUTMUTE);

	if(VOICE_TYPE == nType) {
		if(lc1132_codec_info->mic_num == SINGLE_MIC) {/*2.048M mclk,8ksamplerate,single mic*/
			if(lc1132_codec_info->mic1_mmic) {
				pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_voice_pcm_slave_data3_close;
				nLength = ARRAY_SIZE(lc1132_voice_pcm_slave_data3_close);
			} else {
				pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_voice_pcm_slave_data1_close;
				nLength = ARRAY_SIZE(lc1132_voice_pcm_slave_data1_close);
			}
		} else if(lc1132_codec_info->mic_num == DOUBLE_MICS) {/*2.048M mclk,8ksamplerate,double mics*/
			pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_voice_pcm_slave_data2_close;
			nLength = ARRAY_SIZE(lc1132_voice_pcm_slave_data2_close);
		}
	} else if(AUDIO_TYPE == nType) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_playback_i2s_slave_data1_close;
		nLength = ARRAY_SIZE(lc1132_playback_i2s_slave_data1_close);
	} else if (AUDIO_CAPTURE_TYPE == nType) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_capture_i2s_slave_data1_close;
		nLength = ARRAY_SIZE(lc1132_capture_i2s_slave_data1_close);
	} else if (FM_TYPE == nType) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_fm_off_data;
		nLength = ARRAY_SIZE(lc1132_fm_off_data);
	}
	nRst = comip_lc1132_registers_write(pInitHandle,nLength);

	if((lc1132_codec_info->type_voice == 0)&&(lc1132_codec_info->type_record == 0)){
		comip_lc1132_i2c_write(LC1132_R0,0x22); //mute & disable  mic1/mic2 PGA
		comip_lc1132_i2c_write(LC1132_R23,0x80); //adc1 MUTE
	}
	if((lc1132_codec_info->type_play == 0) &&(lc1132_codec_info->type_record == 0)) {
		comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_audio_i2s_slave_data1_close,\
			ARRAY_SIZE(lc1132_audio_i2s_slave_data1_close));
	}
	if(!(lc1132_codec_info->type_play||lc1132_codec_info->type_record||lc1132_codec_info->type_voice|| \
				lc1132_codec_info->fm||lc1132_codec_info->comip_codec_jackst)) {
		lc1132_deinit();
	}
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_close: nType=%d,end\n",nType);
	return nRst;
}
static int lc1132_device_is_sel(int path_ctl,int check_device)
{
	if((path_ctl && (lc1132_codec_info->outdevice & check_device)) || ((!path_ctl) && (!(lc1132_codec_info->outdevice & check_device))))
		return DEVICE_SEL;
	if(!path_ctl) {
		if((lc1132_codec_info->paudio->dac_stereo_sel & check_device)||(lc1132_codec_info->pvoice->dac_mono_sel & check_device)||\
			(lc1132_codec_info->paudio->line_in_sel & check_device)) {
			return DEVICE_SEL;
		}else {
			return DEVICE_UNSEL;
		}
	}
	return DEVICE_UNSEL;
}
static int lc1132_codec_rec_output_set(int path_ctl)
{
	int nRst,nLength;
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_rec_output_set:path_ctl=%d,out_device=0x%x,start\n",path_ctl,lc1132_codec_info->outdevice);
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_rec_output_set:audio_sel=0x%x\n",lc1132_codec_info->paudio->dac_stereo_sel&0xf);

	if(lc1132_device_is_sel(path_ctl,OUT_DEVICE_REC))
		return 0;
	
	if(path_ctl){
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_rec_output_path_open_data;
		nLength = ARRAY_SIZE(lc1132_rec_output_path_open_data);
	}else{
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_rec_output_path_close_data;
		nLength = ARRAY_SIZE(lc1132_rec_output_path_close_data);
	}
	nRst = comip_lc1132_registers_write(pInitHandle,nLength);

	if(path_ctl){
		lc1132_codec_info->outdevice |= OUT_DEVICE_REC;
	}
	else{
		lc1132_codec_info->outdevice &= (~OUT_DEVICE_REC);
	}
	return nRst;
}
static int lc1132_codec_spk_output_set(int path_ctl)
{
	int nRst,nLength;
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("path_ctl=%d,out_device=0x%x,start\n",path_ctl,lc1132_codec_info->outdevice);

	if(lc1132_device_is_sel(path_ctl,OUT_DEVICE_SPK))
		return 0;

	if(lc1132_codec_info->spk_use_aux) {
		lc1132_codec_aux_output_set(path_ctl);
		return 0;
	}

	if(path_ctl) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_spk_output_path_open_data;
		nLength = ARRAY_SIZE(lc1132_spk_output_path_open_data);
	}else {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_spk_output_path_close_data;
		nLength = ARRAY_SIZE(lc1132_spk_output_path_close_data);
	}
	nRst = comip_lc1132_registers_write(pInitHandle,nLength);
	if(nRst)
		CODEC_LC1132_PRINTK_ERR("registers write error\n");

	if(path_ctl){
		lc1132_codec_info->outdevice |= OUT_DEVICE_SPK;
	} else {
		lc1132_codec_info->outdevice &= (~OUT_DEVICE_SPK);
	}
	return nRst;
}
static int lc1132_codec_hp_output_set(int path_ctl)
{
	int nRst,nLength;
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("path_ctl=%d,out_device=0x%x,start\n",path_ctl,lc1132_codec_info->outdevice);
	if(lc1132_device_is_sel(path_ctl,OUT_DEVICE_HP))
		return 0;

	if(path_ctl) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_hp_output_path_open_data;
		nLength = ARRAY_SIZE(lc1132_hp_output_path_open_data);
	}else {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_hp_output_path_close_data;
		nLength = ARRAY_SIZE(lc1132_hp_output_path_close_data);
	}
	nRst = comip_lc1132_registers_write(pInitHandle,nLength);

	if(path_ctl){
		lc1132_codec_info->outdevice |= OUT_DEVICE_HP;		
	} else {
		lc1132_codec_info->outdevice &= (~OUT_DEVICE_HP);
	}
	return nRst;
}
static int comip_pa_gpio_control(int nValue)
{
	gpio_direction_output(lc1132_codec_info->spk_use_aux,nValue);
	/* notice:If the external PA is controlled by two gpio pins, please add the code here */
	if(nValue) {
		mdelay(15);
	}
	return 0;
}

static int lc1132_codec_aux_output_set(int path_ctl)
{
	int nRst,nLength;
	struct CODEC_I2C_DATA_T *pInitHandle;

	comip_pa_gpio_control(path_ctl);
	if(path_ctl) {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_aux_output_path_open_data;
		nLength = ARRAY_SIZE(lc1132_aux_output_path_open_data);
	}else {
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_aux_output_path_close_data;
		nLength = ARRAY_SIZE(lc1132_aux_output_path_close_data);
	}
	nRst = comip_lc1132_registers_write(pInitHandle,nLength);

	if(path_ctl){
		lc1132_codec_info->outdevice |= OUT_DEVICE_SPK;
	}
	else{
		lc1132_codec_info->outdevice &= (~OUT_DEVICE_SPK);
	}
	return nRst;
}
static void lc1132_codec_dac_stereo_sel_outpath(int nPath)
{
	struct comip_1132_audio_struct *audio_info = (lc1132_codec_info->paudio);
	//dac stereo unsel
	if(((DD_RECEIVER == nPath)||(DD_HEADPHONE== nPath)||(DD_OUTMUTE== nPath))&&(audio_info->dac_stereo_sel & OUT_DEVICE_SPK)) {
			if(lc1132_codec_info->spk_use_aux)
				comip_lc1132_registers_write((struct CODEC_I2C_DATA_T*)&lc1132_audio_aux_path_unsel_data,ARRAY_SIZE(lc1132_audio_aux_path_unsel_data));
			else
				comip_lc1132_registers_write((struct CODEC_I2C_DATA_T*)&lc1132_audio_spk_path_unsel_data,ARRAY_SIZE(lc1132_audio_spk_path_unsel_data));
			audio_info->dac_stereo_sel &= (~OUT_DEVICE_SPK);
	}
	if(((DD_SPEAKER == nPath)||(DD_HEADPHONE== nPath)||(DD_SPEAKER_HEADPHONE == nPath)||(DD_OUTMUTE == nPath))&&(audio_info->dac_stereo_sel & OUT_DEVICE_REC)) {
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T*)&lc1132_audio_rec_path_unsel_data,ARRAY_SIZE(lc1132_audio_rec_path_unsel_data));
			audio_info->dac_stereo_sel &= (~OUT_DEVICE_REC);		
	}
	if(((DD_SPEAKER == nPath)||(DD_RECEIVER == nPath)||(DD_OUTMUTE == nPath))&&(audio_info->dac_stereo_sel & OUT_DEVICE_HP)) {
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_audio_hp_path_unsel_data,ARRAY_SIZE(lc1132_audio_hp_path_unsel_data));
			audio_info->dac_stereo_sel &= (~OUT_DEVICE_HP);	
		}
	//dac stereo sel 
	if((DD_RECEIVER == nPath)&&(!(audio_info->dac_stereo_sel & OUT_DEVICE_REC))) {
		comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_audio_rec_path_sel_data,ARRAY_SIZE(lc1132_audio_rec_path_sel_data));
		audio_info->dac_stereo_sel |= OUT_DEVICE_REC;
	}
	if(((DD_SPEAKER == nPath)||(DD_SPEAKER_HEADPHONE == nPath))&&(!(audio_info->dac_stereo_sel & OUT_DEVICE_SPK))) {
		if(lc1132_codec_info->spk_use_aux)	
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_audio_aux_path_sel_data,ARRAY_SIZE(lc1132_audio_aux_path_sel_data));
		else
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_audio_spk_path_sel_data,ARRAY_SIZE(lc1132_audio_spk_path_sel_data));
		audio_info->dac_stereo_sel |= OUT_DEVICE_SPK;
	}
	if(((DD_HEADPHONE == nPath)||(DD_SPEAKER_HEADPHONE == nPath))&&(!(audio_info->dac_stereo_sel & OUT_DEVICE_HP))) {
		comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_audio_hp_path_sel_data,ARRAY_SIZE(lc1132_audio_hp_path_sel_data));
		audio_info->dac_stereo_sel |= OUT_DEVICE_HP;
	}
}

static void lc1132_codec_dac_mono_sel_outpath(int nPath)
{

	struct comip_1132_voice_struct *voice_info = (lc1132_codec_info->pvoice);
	//dac mono unsel
	if(((DD_RECEIVER == nPath)||(DD_HEADPHONE== nPath)||(DD_OUTMUTE == nPath))&&(voice_info->dac_mono_sel & OUT_DEVICE_SPK)) {
		if(lc1132_codec_info->spk_use_aux)
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_aux_path_unsel_data,ARRAY_SIZE(lc1132_voice_aux_path_unsel_data));
		else
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_spk_path_unsel_data,ARRAY_SIZE(lc1132_voice_spk_path_unsel_data));
		voice_info->dac_mono_sel &= (~OUT_DEVICE_SPK);
	} 
	if(((DD_SPEAKER == nPath)||(DD_HEADPHONE== nPath)||(DD_SPEAKER_HEADPHONE == nPath)||(DD_OUTMUTE == nPath))&&(voice_info->dac_mono_sel & OUT_DEVICE_REC)) {
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_rec_path_unsel_data,ARRAY_SIZE(lc1132_voice_rec_path_unsel_data));
			voice_info->dac_mono_sel &= (~OUT_DEVICE_REC);
	}
	if(((DD_SPEAKER == nPath)||(DD_RECEIVER == nPath)||(DD_OUTMUTE == nPath))&&(voice_info->dac_mono_sel & OUT_DEVICE_HP)) {
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_hp_path_unsel_data,ARRAY_SIZE(lc1132_voice_hp_path_unsel_data));
			voice_info->dac_mono_sel &= (~OUT_DEVICE_HP);
	}
	//dac stereo sel 
	if((DD_RECEIVER == nPath)&&(!(voice_info->dac_mono_sel & OUT_DEVICE_REC))) {
		comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_rec_path_sel_data,ARRAY_SIZE(lc1132_voice_rec_path_sel_data));
		voice_info->dac_mono_sel |= OUT_DEVICE_REC;			
	} else if(((DD_SPEAKER == nPath)||(DD_SPEAKER_HEADPHONE == nPath))&&(!(voice_info->dac_mono_sel & OUT_DEVICE_SPK))) {
		if(lc1132_codec_info->spk_use_aux)
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_aux_path_sel_data,ARRAY_SIZE(lc1132_voice_aux_path_sel_data));
		else
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_spk_path_sel_data,ARRAY_SIZE(lc1132_voice_spk_path_sel_data));
		voice_info->dac_mono_sel |= OUT_DEVICE_SPK;
	} else if(((DD_HEADPHONE == nPath)||(DD_SPEAKER_HEADPHONE == nPath))&&(!(voice_info->dac_mono_sel & OUT_DEVICE_HP))) {
		comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_voice_hp_path_sel_data,ARRAY_SIZE(lc1132_voice_hp_path_sel_data));
		voice_info->dac_mono_sel |= OUT_DEVICE_HP;
	}
}
static void lc1132_codec_line_sel_outpath(int nPath)
{
	struct comip_1132_audio_struct *audio_info = (lc1132_codec_info->paudio);
	//line unsel
	if(((DD_RECEIVER == nPath)||(DD_HEADPHONE== nPath)||(DD_OUTMUTE == nPath))&&(audio_info->line_in_sel & OUT_DEVICE_SPK)) {
		if(lc1132_codec_info->spk_use_aux)
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_line_aux_path_unsel_data,ARRAY_SIZE(lc1132_line_aux_path_unsel_data));
		else			
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_line_spk_path_unsel_data,ARRAY_SIZE(lc1132_line_spk_path_unsel_data));
		audio_info->line_in_sel &= (~OUT_DEVICE_SPK);
	} 
	if(((DD_SPEAKER == nPath)||(DD_RECEIVER == nPath)||(DD_OUTMUTE == nPath))&&(audio_info->line_in_sel & OUT_DEVICE_HP)) {
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_line_hp_path_unsel_data,ARRAY_SIZE(lc1132_line_hp_path_unsel_data));
			audio_info->line_in_sel &= (~OUT_DEVICE_HP);
	}
	if(((DD_SPEAKER == nPath)||(DD_SPEAKER_HEADPHONE == nPath))&&(!(audio_info->line_in_sel & OUT_DEVICE_SPK))) {
		if(lc1132_codec_info->spk_use_aux)
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_line_aux_path_sel_data,ARRAY_SIZE(lc1132_line_aux_path_sel_data));
		else
			comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_line_spk_path_sel_data,ARRAY_SIZE(lc1132_line_spk_path_sel_data));
		audio_info->line_in_sel |= OUT_DEVICE_SPK;
	} 
	if(((DD_HEADPHONE == nPath)||(DD_SPEAKER_HEADPHONE == nPath))&&(!(audio_info->line_in_sel & OUT_DEVICE_HP))) {
		comip_lc1132_registers_write((struct CODEC_I2C_DATA_T *)&lc1132_line_hp_path_sel_data,ARRAY_SIZE(lc1132_line_hp_path_sel_data));
		audio_info->line_in_sel |= OUT_DEVICE_HP;
	}
}


static int lc1132_codec_output_path_set(int nType, int nPath)
{
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_output_path_set nPath=%d,nType=0%d,start\n",nPath,nType);

	if(nPath == DD_BLUETOOTH)
		nPath = DD_OUTMUTE;
	if(nPath != DD_OUTMUTE) {
		switch(nType) {
		case AUDIO_TYPE:
			if(!lc1132_codec_info->type_play)
				return 0;
			break;
		case VOICE_TYPE:
			if(!lc1132_codec_info->type_voice)
				return 0;
			break;
		case FM_TYPE:
			if(!lc1132_codec_info->fm)
				return 0;
			break;
		default:
			break;
		}
	}

	if(AUDIO_TYPE == nType) {
		lc1132_codec_dac_stereo_sel_outpath(nPath);
	} else if(VOICE_TYPE == nType) {
		lc1132_codec_dac_mono_sel_outpath(nPath);
	} else if(FM_TYPE == nType) {
		lc1132_codec_line_sel_outpath(nPath);
	} else {
		return 0;
	}


	if(DD_RECEIVER == nPath){
		lc1132_codec_spk_output_set(PATH_CLOSE);
		lc1132_codec_hp_output_set(PATH_CLOSE);
		lc1132_codec_rec_output_set(PATH_OPEN);
	} else if(DD_SPEAKER == nPath) {
		lc1132_codec_rec_output_set(PATH_CLOSE);
		lc1132_codec_hp_output_set(PATH_CLOSE);
		lc1132_codec_spk_output_set(PATH_OPEN);
	} else if(DD_HEADPHONE == nPath) {
		lc1132_codec_rec_output_set(PATH_CLOSE);
		lc1132_codec_spk_output_set(PATH_CLOSE);
		lc1132_codec_hp_output_set(PATH_OPEN);
	} else if(DD_SPEAKER_HEADPHONE == nPath) {
		lc1132_codec_rec_output_set(PATH_CLOSE);
		lc1132_codec_spk_output_set(PATH_OPEN);
		lc1132_codec_hp_output_set(PATH_OPEN);
	} else if((DD_BLUETOOTH == nPath)||(DD_OUTMUTE == nPath)) {
		lc1132_codec_rec_output_set(PATH_CLOSE);
		lc1132_codec_spk_output_set(PATH_CLOSE);
		lc1132_codec_hp_output_set(PATH_CLOSE);
		nPath = DD_OUTMUTE;
	}
	//update audio cali param
	if((AUDIO_TYPE == nType)||(FM_TYPE == nType))
		lc1132_i2s_ogain_to_hw();
	if(VOICE_TYPE == nType)
		lc1132_pcm_tbl_to_hw(lc1132_codec_info->pvoice->pvoice_table,1);
	
	CODEC_LC1132_PRINTK_TEST("nPath=%d,nType=0%d,end\n",nPath,nType);
	return 0;
}
static int lc1132_codec_input_path_set(int nType, int nPath)
{
	int nRst,nLength;
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("nPath=%d,nType=0%d,start\n",nPath,nType);

	if(DD_MIC == nPath) {
		if(lc1132_codec_info->mic1_mmic) {
			pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_hpmic_to_mic1_input_path_sel_data ;
			nLength = ARRAY_SIZE(lc1132_hpmic_to_mic1_input_path_sel_data);
		} else {
			pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_mic2_input_path_sel_data ;
			nLength = ARRAY_SIZE(lc1132_mic2_input_path_sel_data);
		}
	}else {//DD_HEADSET
		if(lc1132_codec_info->mic1_mmic) {
			pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_mic1_to_hpmic_input_path_sel_data ;
			nLength = ARRAY_SIZE(lc1132_mic1_to_hpmic_input_path_sel_data);
		} else {
			pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_hpmic_input_path_sel_data ;
			nLength = ARRAY_SIZE(lc1132_hpmic_input_path_sel_data);
		}
	}
	lc1132_codec_info->nPath_in = nPath;
	nRst = comip_lc1132_registers_write(pInitHandle,nLength);

	if(((lc1132_codec_info->type_record == 1)||(lc1132_codec_info->type_play == 1)||(lc1132_codec_info->fm == 1))\
		&&(lc1132_codec_info->type_voice == 0)) {
		lc1132_i2s_igain_to_hw();
	}
	return nRst;
}
static int lc1132_codec_set_input_mute(int nType,int nMute)
{
	CODEC_LC1132_PRINTK_TEST("nMute=%d,nType=0%d \n",nMute,nType);

	if(SINGLE_MIC == lc1132_codec_info->mic_num)
		comip_lc1132_reg_bit_write(LC1132_R23,LC1132_BIT_7, nMute); //ADC1 MUTE
	else {
		comip_lc1132_reg_bit_write(LC1132_R23,LC1132_BIT_7, nMute); //ADC1 MUTE
		comip_lc1132_reg_bit_write(LC1132_R39,LC1132_BIT_7,nMute); //ADC2 MUTE
	}
	return DD_SUCCESS;
}
static int lc1132_codec_set_output_mute(int nType,int nMute)
{
	u8 nData = 0x0;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_set_output_mute:nMute=%d,nType=0%d \n",nMute,nType);
	if(FM_TYPE == nType) {
		comip_lc1132_reg_bit_write(LC1132_R02,LC1132_BIT_7,nMute);
		comip_lc1132_reg_bit_write(LC1132_R03,LC1132_BIT_7,nMute);//line in mute
	}
	if(VOICE_TYPE == nType)
		comip_lc1132_reg_bit_write(LC1132_R16,LC1132_BIT_1, nMute);//MONO ADC1 mute
	if(AUDIO_TYPE == nType){
		nData = comip_lc1132_i2c_read(LC1132_R46);
		nData&=0xfc;
		if(nMute)
			nData|=0x03;
		comip_lc1132_i2c_write(LC1132_R46,nData); //DAC L/R MUTE
	}
	return DD_SUCCESS;
}
static int lc1132_codec_dac_output_gain_set(int nType,int nGain)
{
	u8 nData = 0x0;
	int i,nLength,nRst;
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_dac_output_gain_set:nGain=%d,nType=0%d,start\n",nGain,nType);
	
	if(VOICE_TYPE == nType) {
		pInitHandle = lc1132_dac_mono_output_gain_set_data;
		nLength = ARRAY_SIZE(lc1132_dac_mono_output_gain_set_data);
	}
	if(AUDIO_TYPE == nType){
		pInitHandle = lc1132_dac_stereo_output_gain_set_data;
		nLength = ARRAY_SIZE(lc1132_dac_stereo_output_gain_set_data);
	}

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nData |=((u8)( nGain*LC1132_DAC_OUTPUT_GAIN_RANGE/LC1132_DAC_OUTPUT_GAIN_MAX)\
				+ LC1132_DAC_OUTPUT_GAIN_MIN)& pInitHandle[i].nmask;
		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( ": write reg error\n");
		}
	}
	return DD_SUCCESS;
}
static int lc1132_codec_line_in_mute_set(int nMute)
{
	CODEC_LC1132_PRINTK_INFO("lc1132_codec_line_in_mute_set:nMute=%d \n",nMute);
	comip_lc1132_reg_bit_write(LC1132_R02,LC1132_BIT_7,nMute);
	comip_lc1132_reg_bit_write(LC1132_R03,LC1132_BIT_7,nMute);
	return 0;
}
static int lc1132_codec_line_in_output_gain_set(int nGain)
{
	u8 nData = 0x0;
	int i,nRst;	
	struct CODEC_I2C_DATA_T *pInitHandle;
	int nLength;
	CODEC_LC1132_PRINTK_INFO("lc1132_codec_line_in_output_gain_set: nGain=%d,start\n",nGain);

	if(nGain == LC1132_LINE_OUTPUT_GAIN_MIN) {
		lc1132_codec_line_in_mute_set(1);
		return DD_SUCCESS;
	}
	pInitHandle=lc1132_line_in_output_gain_set_data;
	nLength=ARRAY_SIZE(lc1132_line_in_output_gain_set_data);

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nData |=((u8)(nGain*LC1132_LINE_OUTPUT_GAIN_RANGE/LC1132_LINE_OUTPUT_GAIN_MAX)\
			+ LC1132_LINE_OUTPUT_GAIN_MIN)& pInitHandle[i].nmask;
	
		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( ": write reg error\n");
		}
	}

	lc1132_codec_line_in_mute_set(0);
	return DD_SUCCESS;
}
static int lc1132_codec_outdevice_output_gain_set(int nPath,int nGain)
{
	u8 nData = 0x0;
	int i,nRst,nLength;	
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_outdevice_output_gain_set:nGain=%d,nPath=0%d,start\n",nGain,nPath);

	if(DD_RECEIVER == nPath) {
		pInitHandle = lc1132_rec_output_gain_set_data;
		nLength =  ARRAY_SIZE(lc1132_rec_output_gain_set_data);
	} else if (DD_SPEAKER == nPath) {
		pInitHandle = lc1132_spk_output_gain_set_data;
		nLength =  ARRAY_SIZE(lc1132_spk_output_gain_set_data);
	} else if(DD_HEADPHONE == nPath) {
		pInitHandle = lc1132_hp_output_gain_set_data;
		nLength =  ARRAY_SIZE(lc1132_hp_output_gain_set_data);
	} else {
		CODEC_LC1132_PRINTK_DEBUG("lc1132_codec_outdevice_output_gain_set:NONE path=%d \n",nPath);
		return 0;
	}
	for(i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		
		nData |=((u8)( nGain*LC1132_OUT_DEVICE_OUTPUT_GAIN_RANGE/LC1132_OUT_DEVICE_OUTPUT_GAIN_MAX)\
				+ LC1132_OUT_DEVICE_OUTPUT_GAIN_MIN)& pInitHandle[i].nmask;
		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " write reg error\n");
		}
	}
	return DD_SUCCESS;
}
static int lc1132_codec_mic1_input_gain_set(int nGain)
{
	u8 nData = 0x0;
	int i,nRst,nLength;	
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_mic1_input_gain_set:nGain=%d \n",nGain);

	pInitHandle = lc1132_mic1_input_gain_set_data;
	nLength =  ARRAY_SIZE(lc1132_mic1_input_gain_set_data);

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nData |=((u8)( nGain*LC1132_MIC_INPUT_GAIN_RANGE/LC1132_MIC_INPUT_GAIN_MAX)\
				+ LC1132_MIC_INPUT_GAIN_MIN)& pInitHandle[i].nmask;
		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " write reg error\n");
		}
	}
	return DD_SUCCESS;
}

static int lc1132_codec_mic2_input_gain_set(int nGain)
{
	u8 nData = 0x0;
	int i,nRst,nLength;	
	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_mic2_input_gain_set:nGain=%d start\n",nGain);

	pInitHandle = lc1132_mic2_input_gain_set_data;
	nLength =  ARRAY_SIZE(lc1132_mic2_input_gain_set_data);

	for(i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nData |=((u8)( nGain*LC1132_MIC_INPUT_GAIN_RANGE/LC1132_MIC_INPUT_GAIN_MAX)\
				+ LC1132_MIC_INPUT_GAIN_MIN)& pInitHandle[i].nmask;
		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " write reg error\n");
		}
	}
	return DD_SUCCESS;
}
static int lc1132_codec_adc1_input_gain_set(int nType,int nGain)
{
	u8 nData = 0x0;
	int i,nRst,nLength;	

	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_adc1_input_gain_set:nGain=%d,nType=%d start\n",nGain,nType);

	if(VOICE_TYPE == nType) {
		pInitHandle = lc1132_adc1_input_gain_set_data;
		nLength =  ARRAY_SIZE(lc1132_adc1_input_gain_set_data);
	}else if(AUDIO_CAPTURE_TYPE == nType){
		pInitHandle = lc1132_adc1_input_gain_set_data;
		nLength =  ARRAY_SIZE(lc1132_adc1_input_gain_set_data);
	}
	for(i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nData |=((u8)( nGain*LC1132_ADC_INPUT_GAIN_RANGE/LC1132_ADC_INPUT_GAIN_MAX)\
				+ LC1132_ADC_INPUT_GAIN_MIN)& pInitHandle[i].nmask;
		
		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " write reg error\n");
		}
	}
	return DD_SUCCESS;
}

static int lc1132_codec_adc2_input_gain_set(int nType,int nGain)
{
	u8 nData = 0x0;
	int i,nRst,nLength;	

	struct CODEC_I2C_DATA_T *pInitHandle;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_adc2_input_gain_set:nGain=%d,nType=%d start\n",nGain,nType);

	if(VOICE_TYPE == nType) {
		pInitHandle = lc1132_adc2_input_gain_set_data;
		nLength =  ARRAY_SIZE(lc1132_adc2_input_gain_set_data);
	}
	for(i=0; i<nLength; i++) {
		nRst = comip_lc1132_i2c_read(pInitHandle[i].nreg);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " read (0x%x)failed \n",pInitHandle[i].nreg);
			return nRst;
		} else
			nData = nRst;
		if(pInitHandle[i].nmask) {
			nData&= (~pInitHandle[i].nmask);
		}
		nData = pInitHandle[i].nvalue|nData;
		nData |=((u8)( nGain*LC1132_ADC_INPUT_GAIN_RANGE/LC1132_ADC_INPUT_GAIN_MAX)\
				+ LC1132_ADC_INPUT_GAIN_MIN)& pInitHandle[i].nmask;
		
		nRst =comip_lc1132_i2c_write(pInitHandle[i].nreg, nData);
		mdelay(pInitHandle[i].ndelay_times);
		if(nRst<0) {
			CODEC_LC1132_PRINTK_ERR( " write reg error\n");
		}
	}
	return DD_SUCCESS;
}
static void lc1132_i2s_igain_to_hw(void)
{
	u8 frist_input_gain = 0, second_input_gain = 0;
	struct comip_1132_audio_tbl *audio_gain_tbl = (lc1132_codec_info->paudio->paudio_table);

	CODEC_LC1132_PRINTK_TEST("lc1132_i2s_igain_to_hw start\n");
	if(lc1132_codec_info->nPath_in == DD_MIC) {
		frist_input_gain = audio_gain_tbl->i2s_igain_mic_pgagain;
		second_input_gain = audio_gain_tbl->i2s_igain_mic_adclevel;
	}else if(lc1132_codec_info->nPath_in == DD_HEADSET) {
		frist_input_gain = audio_gain_tbl->i2s_igain_headsetmic_pgagain;
		second_input_gain = audio_gain_tbl->i2s_igain_headsetmic_adclevel;
	}
	lc1132_codec_adc1_input_gain_set(AUDIO_CAPTURE_TYPE,frist_input_gain);//mic_gain
	if(lc1132_codec_info->mic1_mmic) {
		lc1132_codec_mic1_input_gain_set(second_input_gain);//for mic1
		lc1132_codec_mic2_input_gain_set(second_input_gain);//for hp mic
	} else {
		lc1132_codec_mic2_input_gain_set(second_input_gain);//for mic2
	}

	lc1132_codec_set_input_mute(AUDIO_CAPTURE_TYPE,0);
}

static void lc1132_i2s_ogain_to_hw(void)
{
	u8 frist_output_gain = 0, second_output_gain = 0;
	int out_path = 0;
	struct comip_1132_audio_tbl *audio_gain_tbl = (lc1132_codec_info->paudio->paudio_table);
	struct comip_1132_audio_struct *audio_info = (lc1132_codec_info->paudio);
	CODEC_LC1132_PRINTK_TEST("lc1132_i2s_ogain_to_hw :start\n");
	if(audio_info->dac_stereo_sel == OUT_DEVICE_REC) {
		frist_output_gain = audio_gain_tbl->i2s_ogain_receiver_daclevel;
		second_output_gain = audio_gain_tbl->i2s_ogain_receiver_epvol;
		out_path = DD_RECEIVER;
	}else if(audio_info->dac_stereo_sel == OUT_DEVICE_SPK) {
		frist_output_gain = audio_gain_tbl->i2s_ogain_speaker_daclevel;
		second_output_gain = audio_gain_tbl->i2s_ogain_speaker_cdvol;
		out_path = DD_SPEAKER;
	}else if(audio_info->dac_stereo_sel == OUT_DEVICE_HP) {
		frist_output_gain = audio_gain_tbl->i2s_ogain_headset_daclevel;
		second_output_gain = audio_gain_tbl->i2s_ogain_headset_hpvol;
		out_path = DD_HEADPHONE;
	}else if(audio_info->dac_stereo_sel == (OUT_DEVICE_SPK|OUT_DEVICE_HP)) {
		frist_output_gain = audio_gain_tbl->i2s_ogain_spk_hp_daclevel;
		second_output_gain = audio_gain_tbl->i2s_ogain_spk_hp_hpvol;
		lc1132_codec_outdevice_output_gain_set(DD_SPEAKER,audio_gain_tbl->i2s_ogain_spk_hp_cdvol);//output_gain
		out_path = DD_HEADPHONE;
	}
	if((audio_info->line_in_sel & OUT_DEVICE_SPK)&&(!(audio_info->dac_stereo_sel & OUT_DEVICE_SPK)))
		lc1132_codec_outdevice_output_gain_set(DD_SPEAKER,audio_gain_tbl->i2s_ogain_speaker_cdvol);//output_gain
	if((audio_info->line_in_sel & OUT_DEVICE_HP)&&(!(audio_info->dac_stereo_sel & OUT_DEVICE_HP)))
		lc1132_codec_outdevice_output_gain_set(DD_HEADPHONE,audio_gain_tbl->i2s_ogain_headset_hpvol);//output_gain

	lc1132_codec_dac_output_gain_set(AUDIO_TYPE,frist_output_gain);//dac_gain
	lc1132_codec_outdevice_output_gain_set(out_path,second_output_gain);//output_gain
	CODEC_LC1132_PRINTK_TEST("lc1132_i2s_ogain_to_hw:end\n");
}

int lc1132_codec_output_gain_set(int nType, int nGain)
{
	int nRst =0;
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_output_gain_set :nType=%d, nGain=%d start\n",nType,nGain);

	if(VOICE_TYPE == nType) {	
		lc1132_codec_info->pvoice->cur_volume = nGain;
		if(lc1132_codec_info->type_voice == 1) {
			lc1132_pcm_tbl_to_hw(lc1132_codec_info->pvoice->pvoice_table,0);
			nRst = 0;
		}
	} else if(FM_TYPE == nType) {
		lc1132_codec_line_in_output_gain_set(nGain);
	}
	return nRst;
}

int lc1132_codec_fm_enable(int nOnOff)
{
	struct CODEC_I2C_DATA_T *pInitHandle;
	int nLength=0;
	CODEC_LC1132_PRINTK_INFO("lc1132_codec_fm_enable :nOnOff=%d start\n",nOnOff);
	lc1132_codec_info->fm=nOnOff;

	if(nOnOff == 1) {
		lc1132_init();
		pInitHandle = (struct CODEC_I2C_DATA_T *)&lc1132_fm_on_data;
		nLength = ARRAY_SIZE(lc1132_fm_on_data);
		comip_lc1132_registers_write(pInitHandle,nLength);
	}else {
		lc1132_codec_close(FM_TYPE);
	}
	return DD_SUCCESS;
}

int codec_lc1132_enable_alc(int nValue)
{
	int i;
	u8 reg,val;
	struct comip_1132_audio_tbl *audio_gain_tbl = (lc1132_codec_info->paudio->paudio_table);
	
	if(nValue&&lc1132_codec_info->type_play){
		CODEC_LC1132_PRINTK_TEST(KERN_INFO"lc1132_codec_update_alc\n");
		for(i=0;i<14;i++){ //r51-64
			reg = LC1132_R51 + i;
			val = *((u8*)&audio_gain_tbl->alc_reg51 + i);
			comip_lc1132_i2c_write(reg,val);
		}
	} else {
		/*HP disable*/
		comip_lc1132_i2c_write(LC1132_R51,0x00);
		/*ALC disable*/
		comip_lc1132_i2c_write(LC1132_R52,0x00);
		/*EQ disable*/
		comip_lc1132_i2c_write(LC1132_R60,0x02);
		comip_lc1132_i2c_write(LC1132_R61,0x02);
		comip_lc1132_i2c_write(LC1132_R62,0x02);
		comip_lc1132_i2c_write(LC1132_R63,0x02);
		comip_lc1132_i2c_write(LC1132_R64,0x02);
	}
	return 0;
}

int lc1132_codec_enable_voice(int nValue)
{
	int nRst =0;
	CODEC_LC1132_PRINTK_INFO("lc1132_enable_voice %d,play %d,voice %d,record %d,fm %d,mic_num %d\n",\
		nValue,lc1132_codec_info->type_play,lc1132_codec_info->type_voice,lc1132_codec_info->type_record,lc1132_codec_info->fm,lc1132_codec_info->mic_num);

	if(nValue ==1) {		
		lc1132_codec_info->nType_audio = VOICE_TYPE;
		lc1132_codec_info->type_voice = 1;
		lc1132_voice_clk_enable(1);
		lc1132_init();
		nRst = lc1132_codec_open(lc1132_codec_info->nType_audio);
	} else {
		lc1132_codec_info->nType_audio = AUDIO_TYPE;
		lc1132_codec_info->type_voice = 0;
		lc1132_codec_close(VOICE_TYPE);
		lc1132_codec_output_path_set(VOICE_TYPE,DD_BLUETOOTH);
		lc1132_codec_set_output_mute(VOICE_TYPE,1);
		lc1132_voice_clk_enable(0);
	}
	return nRst;
}
static ssize_t lc1132_codec_comip_register_show(const char *buf)
{
	int len;
	u8 nReg =0;
	u8 nData=0;
	len = sprintf((char *)buf, "----dump 1132 reg----\n");
	for(nReg=0; nReg<COMIP_1132_REGS_NUM; nReg++) {
		nData = 0;
		nData = comip_lc1132_i2c_read(nReg);
		CODEC_LC1132_PRINTK_INFO("comip_1132 reg: %d 0x%x=0x%x\n",nReg,nReg,nData);
	}
	return len;
}
static ssize_t lc1132_codec_comip_register_store( const char *buf, size_t size)
{
	int long ngtest =0 ;
	u8 nReg =0;
	u8 nData=0;

	ngtest = simple_strtoul(buf, NULL, 10);
	nReg = (ngtest &0xff);
	nData = ((ngtest>>0x08) &0xff);
	CODEC_LC1132_PRINTK_INFO( "1132_register_store Reg0x%x = 0x%x",nReg,nData);
	comip_1132_write(lc1132_codec_info->pcodec,nReg, nData);
	return size;
}
static int lc1132_codec_set_double_mics_mode(int nValue)
{
	CODEC_LC1132_PRINTK_DEBUG("nValue = %d",nValue);
	lc1132_codec_info->mic_num = nValue;

	return 0;
}
static int lc1132_codec_hp_detpol_set(int detpol)
{
	lc1132_i2c_bit_write(LC1132_REG_JKHSIRQST,LC1132_BIT_7,!detpol);
	CODEC_LC1132_PRINTK_DEBUG("detpol = %d",detpol);
	return 0;
}
static int lc1132_codec_mp_power_save(int nType,int nValue)
{
	u8 value;
	CODEC_LC1132_PRINTK_DEBUG("lc1132_codec_mp_power_save:nType=%d,val=%d\n",nType,nValue);

	if(nType==AUDIO_TYPE){
		if(nValue==1 && (!lc1132_codec_info->comip_codec_jackst)){
			value=comip_lc1132_i2c_read(LC1132_R93);
			value |= 0x07;
			comip_lc1132_i2c_write(LC1132_R93, value);

			value=comip_lc1132_i2c_read(LC1132_R87);
			value &=0x7f;
			comip_lc1132_i2c_write(LC1132_R87, value);

			value=comip_lc1132_i2c_read(LC1132_R96);
			value &=0xfe;
			comip_lc1132_i2c_write(LC1132_R96, value);
		}else {
			if(lc1132_codec_info->outdevice&OUT_DEVICE_SPK){
				value=comip_lc1132_i2c_read(LC1132_R93);
				value &=0xf8;
				comip_lc1132_i2c_write(LC1132_R93, value);
			}
			value=comip_lc1132_i2c_read(LC1132_R87);
			value |=0x80;
			comip_lc1132_i2c_write(LC1132_R87, value);

			value=comip_lc1132_i2c_read(LC1132_R96);
			value |=0x01;
			comip_lc1132_i2c_write(LC1132_R96, value);
		}
	}
	return 0;
}

static ssize_t lc1132_codec_calibrate_data_show( const char *buf)
{
	int len = 0;

	len = sprintf((char *)buf, "gain_in_level0: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level0);
	len += sprintf((char *)buf + len, "gain_in_level1: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level1);
	len += sprintf((char *)buf + len, "gain_in_level2: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level2);
	len += sprintf((char *)buf + len, "gain_in_level3: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level3);
	len += sprintf((char *)buf + len, "gain_out_level0_class0: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class0);
	len += sprintf((char *)buf + len, "gain_out_level0_class1: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class1);
	len += sprintf((char *)buf + len, "gain_out_level0_class2: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class2);
	len += sprintf((char *)buf + len, "gain_out_level0_class3: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class3);
	len += sprintf((char *)buf + len, "gain_out_level0_class4: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class4);
	len += sprintf((char *)buf + len, "gain_out_level0_class5: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class5);
	len += sprintf((char *)buf + len, "gain_out_level1: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level1);
	len += sprintf((char *)buf + len, "gain_out_level2: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level2);
	len += sprintf((char *)buf + len, "gain_out_level3: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level3);
	len += sprintf((char *)buf + len, "gain_out_level4: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level4);
	len += sprintf((char *)buf + len, "gain_out_level5: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level5);
	len += sprintf((char *)buf + len, "gain_out_level6: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level6);

	len += sprintf((char *)buf + len, "sidetone_reserve_1: %u\n", lc1132_codec_info->pvoice->pvoice_table->sidetone_reserve_1);
	len += sprintf((char *)buf + len, "sidetone_reserve_2: %u\n", lc1132_codec_info->pvoice->pvoice_table->sidetone_reserve_2);

	return len;
}

static ssize_t codec_lc1132_calibrate_data_print( const char *buf,size_t size)
{
	int len = 0;
	CODEC_LC1132_PRINTK_DEBUG("gain_in_level0: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level0);
	CODEC_LC1132_PRINTK_DEBUG("gain_in_level1: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level1);
	CODEC_LC1132_PRINTK_DEBUG("gain_in_level2: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level2);
	CODEC_LC1132_PRINTK_DEBUG("gain_in_level3: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_in_level3);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level0_class0: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class0);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level0_class1: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class1);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level0_class2: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class2);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level0_class3: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class3);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level0_class4: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class4);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level0_class5: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level0_class5);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level1: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level1);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level2: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level2);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level3: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level3);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level4: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level4);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level5: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level5);
	CODEC_LC1132_PRINTK_DEBUG("gain_out_level6: %u\n", lc1132_codec_info->pvoice->pvoice_table->gain_out_level6);

	CODEC_LC1132_PRINTK_DEBUG("sidetone_reserve_1: %u\n", lc1132_codec_info->pvoice->pvoice_table->sidetone_reserve_1);
	CODEC_LC1132_PRINTK_DEBUG("sidetone_reserve_2: %u\n", lc1132_codec_info->pvoice->pvoice_table->sidetone_reserve_2);
	CODEC_LC1132_PRINTK_DEBUG("cur_volume: %u\n", lc1132_codec_info->pvoice->cur_volume);
	return len;

}

static void lc1132_pcm_tbl_to_hw(struct codec_voice_tbl *ptbl,unsigned char volumeset)
{
	u8 gain;
	int out_path = 0;
	//struct comip_1132_voice_struct *voice_info = &(lc1132_codec_info->pvoice);
	int dac_mono_sel= lc1132_codec_info->pvoice->dac_mono_sel;
	unsigned int cur_volume = lc1132_codec_info->pvoice->cur_volume;


	if(volumeset == 1) {
		/*
		 * gain to analog input
		 */
		/*  -- input, the first step (analog)  64 levels*/
		if(lc1132_codec_info->mic1_mmic) {
			lc1132_codec_mic1_input_gain_set(ptbl->gain_in_level0);
			lc1132_codec_mic2_input_gain_set(ptbl->gain_in_level0);
		} else {
			lc1132_codec_mic2_input_gain_set(ptbl->gain_in_level0);
		}

		/*
		 * gain to digital input
		 */
		/* ADC -- input, the second step (digital)  128 levels*/

		lc1132_codec_adc1_input_gain_set(VOICE_TYPE,ptbl->gain_in_level1);

		if(lc1132_codec_info->mic_num == DOUBLE_MICS) {
			/*	-- input, the first step (analog)  64 levels*/
			lc1132_codec_mic1_input_gain_set(ptbl->gain_in_level2);
			/*
			 * gain to digital input
			 */
			/* ADC -- input, the second step (digital)	128 levels*/
			
			lc1132_codec_adc2_input_gain_set(VOICE_TYPE,ptbl->gain_in_level3);
		}


		/*
		 * gain to digital output
		 */
		/* user's volume, DAC 128 levels -- output, the first step (digital) */
		gain = (*((u8 *)&ptbl->gain_out_level0_class0 + cur_volume));
		lc1132_codec_dac_output_gain_set(VOICE_TYPE, gain);
		/* -- output, the second step (digital) 32 levels */
		if(dac_mono_sel & OUT_DEVICE_REC)
			out_path = DD_RECEIVER;
		else if(dac_mono_sel & OUT_DEVICE_SPK)
			out_path = DD_SPEAKER;
		else if (dac_mono_sel & OUT_DEVICE_HP)
			out_path = DD_HEADPHONE;
		lc1132_codec_outdevice_output_gain_set(out_path, ptbl->gain_out_level1);
	} else {
		/* gain to digital output */
		/* user's volume, DAC 128 levels -- output, the first step (digital) */
		printk("lc1132_pcm_tbl_to_hw: level0 addr:0x%p,level0=%d\n",(&ptbl->gain_out_level0_class0),ptbl->gain_out_level0_class0);
		gain = (*((u8 *)&ptbl->gain_out_level0_class0 + cur_volume));
		lc1132_codec_dac_output_gain_set(VOICE_TYPE, gain);
	}
	lc1132_codec_set_input_mute(VOICE_TYPE,0);
}

static ssize_t lc1132_codec_calibrate_data_store( const char *buf, size_t size)
{
	unsigned int i;
	struct codec_voice_tbl *ptbl;
//	lc1132_clk_enable();
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_calibrate_data_store: START \n");
	if (buf == NULL || size > CODEC_CAL_TBL_SIZE) {
		CODEC_LC1132_PRINTK_ERR("pcm table is updated with error\n");
		return -1;
	}
	ptbl = (struct codec_voice_tbl *)buf;
	if (ptbl->flag != 0xAAAAAAAA) {
		CODEC_LC1132_PRINTK_ERR("pcm table is updated with magic error ptbl->flag = 0x%x,load default value\n",ptbl->flag);
		lc1132_pcm_tbl_to_hw(lc1132_codec_info->pvoice->pvoice_table,1);
		codec_lc1132_calibrate_data_print(buf,size);
		return -1;
	}
	for (i = 0; i < CODEC_PT_GAIN_ITEMNUM; i++) {
		if (*((u8 *)&ptbl->gain_in_level0 + i) > CODEC_PT_GAIN_MAXVAL) {
			CODEC_LC1132_PRINTK_ERR("pcm table is updated with item error\n");
			return -1;
		}
	}
	memcpy(lc1132_codec_info->pvoice->pvoice_table, buf, size);
	lc1132_pcm_tbl_to_hw(lc1132_codec_info->pvoice->pvoice_table,1);
	codec_lc1132_calibrate_data_print(buf,size);
	CODEC_LC1132_PRINTK_TEST("lc1132_codec_calibrate_data_store: END \n");

	return size;
}

#ifdef CODEC_SOC_PROC
static ssize_t lc1132_proc_read(char *page,int count, loff_t *eof)
{
	char *buf  = page;
	char *next = buf;
	int  t, i;
	u8 regval;
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
	t = scnprintf(next, size, "codec_lc1132_log_print,nType=%d,play=%d,record=%d,voice=%d,fm=%d,audio_sel=0x%x,voice_sel=0x%x,nPath_in=%d,cur_volume=%d\n",\
		lc1132_codec_info->nType_audio,lc1132_codec_info->type_play,\
		lc1132_codec_info->type_record,lc1132_codec_info->type_voice,lc1132_codec_info->fm,\
		lc1132_codec_info->paudio->dac_stereo_sel,lc1132_codec_info->pvoice->dac_mono_sel,\
		lc1132_codec_info->nPath_in,lc1132_codec_info->pvoice->cur_volume);

	size -= t;
	next += t;

	t = scnprintf(next, size, "lc1132 regs: \n");
	size -= t;
	next += t;

//	lc1132_clk_enable();
	for (i = 0; i < COMIP_1132_REGS_NUM; i++) {
		regval = comip_lc1132_i2c_read(i);
		//t = scnprintf(next, size, "[%d 0x%02x]=0x%02x  \n",i , i, regval);
		t = scnprintf(next, size, "[%d 0x%02x]=0x%02x  \n",i , i, regval);
		size -= t;
		next += t;
	}
	lc1132_i2c_read(LC1132_REG_JKHSIRQST,&regval);
	t = scnprintf(next, size, "JKHSIRQST regs=0x%02x \n",regval);
	size -= t;
	next += t;

	lc1132_i2c_read(LC1132_REG_JKHSMSK,&regval);
	t = scnprintf(next, size, "JKHSMSK regs=0x%02x \n",regval);
	size -= t;
	next += t;

	*eof = 1;
	return count - size;
}

static int lc1132_proc_write(const char __user *buffer,unsigned long count)
{
	static char kbuf[200];
	char *buf = kbuf;
	unsigned int val, reg, val2;
	char cmd;
	u8 nData=0;

	if (count >= 200)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	//sscanf(buf, "%c 0x%x 0x%x", &cmd, &reg, &val);
	sscanf(buf, "%c %d 0x%x", &cmd, &reg, &val);

	if ('r' == cmd) {
		val = comip_lc1132_i2c_read(reg);
		CODEC_LC1132_PRINTK_ERR( "Read reg:[%d]=0x%2x\n", reg, val);

	} else if ('w' == cmd) {
		if (reg > 0xff) {
			CODEC_LC1132_PRINTK_ERR( "invalid value!\n");
			goto error;
		}
		comip_lc1132_i2c_write(reg, val);
		val2 = comip_lc1132_i2c_read(reg);
		scnprintf(buf, count, "%c read back %d 0x%x", cmd, reg, val2);
		CODEC_LC1132_PRINTK_ERR("write reg:%d<==0x[%2x], read back 0x%2x\n",reg, val, val2);

	} else if ('o' == cmd) {
		val = comip_lc1132_i2c_read(reg);
		CODEC_LC1132_PRINTK_INFO( "Read reg:[%d]=0x%08x\n", reg, val);

	} else if ('i' == cmd) {
		val2 = comip_lc1132_i2c_read(reg);
		CODEC_LC1132_PRINTK_INFO( "Read reg:[%d]=0x%08x\n", reg, val2);

		comip_lc1132_i2c_write(reg, val);
		CODEC_LC1132_PRINTK_INFO( "Write reg:[%d]=0x%08x\n", reg, val);
	}else if ('a' == cmd) {//main  mic -> receiver  (both lc1120 and lc1132 use cmd A)
		lc1132_init();
		if(lc1132_codec_info->mic1_mmic){
			nData = comip_lc1132_i2c_read(LC1132_R0);
			nData&=0xf8;
			nData|=0x05;
			comip_lc1132_i2c_write(LC1132_R0,nData);//mic1 zcen;mic1 pga unmute,mic1 pga enable
			comip_lc1132_i2c_write(LC1132_R24,0x4f);//set mic1 pga gain to default  value
			comip_lc1132_i2c_write(LC1132_R84,0x76);

			comip_lc1132_i2c_read(LC1132_R85);
			nData &=0x1f;
			nData |=0x60;
			comip_lc1132_i2c_write(LC1132_R85,nData);//sidetone sel mic1

			nData=comip_lc1132_i2c_read(LC1132_R91);
			nData |=0x04;
			comip_lc1132_i2c_write(LC1132_R91,nData);//sidetone sel REC
			lc1132_codec_rec_output_set(PATH_OPEN);//open REC outpath

			comip_lc1132_i2c_write(LC1132_R05,0x7f);
			comip_lc1132_i2c_write(LC1132_R04,0x7e);
			comip_lc1132_i2c_write(LC1132_R24,0xf0);

		}else{
			nData = comip_lc1132_i2c_read(LC1132_R0);
			nData &= 0x8f;
			nData |= 0x50; // mic2 zcen; mic2 pga unmute; mic2 pga enable
			comip_lc1132_i2c_write(LC1132_R0, nData);
			comip_lc1132_i2c_write(LC1132_R40, 0x4f);// mic2 pga gain
			comip_lc1132_i2c_write(LC1132_R84, 0x76);// VMIC SEL,MICBEN
			nData = comip_lc1132_i2c_read(LC1132_R85);
			nData &= 0x7b;
			nData |= 0x83; // Sidetone sel MIC2,SEL HPmic
			comip_lc1132_i2c_write(LC1132_R85, nData);

			nData = comip_lc1132_i2c_read(LC1132_R91);
			nData |= 0x04;
			comip_lc1132_i2c_write(LC1132_R91, nData); // Sidetone sel REC
			lc1132_codec_rec_output_set(PATH_OPEN);

			comip_lc1132_i2c_write(LC1132_R05, 0x7f); // Rec PGA
			comip_lc1132_i2c_write(LC1132_R04, 0x7e); // Stone PGA
			comip_lc1132_i2c_write(LC1132_R40, 0xf0); // MIC2  PGA
		}


		CODEC_LC1132_PRINTK_INFO( "audio loopback, mic -> receiver\n");
		}else if ('c' == cmd) {//headsetmic -> hp   (both  1120 and 1132 use cmd C )
		lc1132_init();
		nData = comip_lc1132_i2c_read(LC1132_R0);
		nData &= 0x8f;
		nData |= 0x50; // mic2 zcen; mic2 pga unmute; mic2 pga enable
		comip_lc1132_i2c_write(LC1132_R0, nData);
		comip_lc1132_i2c_write(LC1132_R40, 0x4f);// mic2 pga gain
		comip_lc1132_i2c_write(LC1132_R84, 0x77);// VMIC SEL,MICBEN,MICBSWEN
		nData = comip_lc1132_i2c_read(LC1132_R85);
		nData &= 0x7b;
		nData |= 0x9c; // Sidetone sel MIC2,SEL HPmic
		comip_lc1132_i2c_write(LC1132_R85, nData);


		nData = comip_lc1132_i2c_read(LC1132_R90);
		nData |= 0x01;
		comip_lc1132_i2c_write(LC1132_R90, nData); // Sidetone sel HP
		nData = comip_lc1132_i2c_read(LC1132_R89);
		nData |= 0x01;
		comip_lc1132_i2c_write(LC1132_R89, nData); // Sidetone sel HP
		lc1132_codec_hp_output_set(PATH_OPEN);

		comip_lc1132_i2c_write(LC1132_R04, 0x7e); // Stone PGA

		CODEC_LC1132_PRINTK_INFO( "audio loopback, headsetmic -> headphone\n");
	}	else if ('h' == cmd) {//headsetmic -> receiver
		lc1132_init();
		nData = comip_lc1132_i2c_read(LC1132_R0);
		nData &= 0x8f;
		nData |= 0x50; // mic2 zcen; mic2 pga unmute; mic2 pga enable
		comip_lc1132_i2c_write(LC1132_R0, nData);
		comip_lc1132_i2c_write(LC1132_R40, 0x4f);// mic2 pga gain
		comip_lc1132_i2c_write(LC1132_R84, 0x77);// VMIC SEL,MICBEN,MICBSWEN
		nData = comip_lc1132_i2c_read(LC1132_R85);
		nData &= 0x7b;
		nData |= 0x9c; // Sidetone sel MIC2,SEL HPmic
		comip_lc1132_i2c_write(LC1132_R85, nData);

		nData = comip_lc1132_i2c_read(LC1132_R91);
		nData |= 0x04;
		comip_lc1132_i2c_write(LC1132_R91, nData); // Sidetone sel REC
		lc1132_codec_rec_output_set(PATH_OPEN);

		comip_lc1132_i2c_write(LC1132_R04, 0x7e); // Stone PGA
		CODEC_LC1132_PRINTK_INFO( "audio loopback, headsetmic -> receiver\n");

	} else if ('m' == cmd) {//mic -> headset
		lc1132_init();
		if(lc1132_codec_info->mic1_mmic){
			nData =comip_lc1132_i2c_read(LC1132_R0);
			nData &=0xf8;
			nData |=0x05;// mic1 zcen; mic1 pga unmute; mic1 pga enable
			comip_lc1132_i2c_write(LC1132_R0,nData);
			comip_lc1132_i2c_write(LC1132_R24,0x7f);
			comip_lc1132_i2c_write(LC1132_R84, 0x76);
			nData =comip_lc1132_i2c_read(LC1132_R85);
			nData &=0x1f;
			nData |=0x60;
			comip_lc1132_i2c_write(LC1132_R85,nData);

			comip_lc1132_reg_bit_write(LC1132_R90, 0x01, 1);
			comip_lc1132_reg_bit_write(LC1132_R89, 0x01, 1);
			lc1132_codec_hp_output_set(PATH_OPEN);

			comip_lc1132_i2c_write(LC1132_R04,0x7e);
			CODEC_LC1132_PRINTK_INFO( "audio loopback, mic1 -> headset\n");
		}else{
			nData = comip_lc1132_i2c_read(LC1132_R0);
			nData &= 0x8f;
			nData |= 0x50; // mic2 zcen; mic2 pga unmute; mic2 pga enable
			comip_lc1132_i2c_write(LC1132_R0, nData);
			comip_lc1132_i2c_write(LC1132_R40, 0x4f);// mic2 pga gain
			comip_lc1132_i2c_write(LC1132_R84, 0x76);// VMIC SEL,MICBEN
			nData = comip_lc1132_i2c_read(LC1132_R85);
			nData &= 0x7b;
			nData |= 0x83; // Sidetone sel MIC2,SEL HPmic
			comip_lc1132_i2c_write(LC1132_R85, nData);

			nData = comip_lc1132_i2c_read(LC1132_R90);
			nData |= 0x01;
			comip_lc1132_i2c_write(LC1132_R90, nData); // Sidetone sel HP
			nData = comip_lc1132_i2c_read(LC1132_R89);
			nData |= 0x01;
			comip_lc1132_i2c_write(LC1132_R89, nData); // Sidetone sel HP
			lc1132_codec_hp_output_set(PATH_OPEN);

			comip_lc1132_i2c_write(LC1132_R04, 0x7e); // Stone PGA
			CODEC_LC1132_PRINTK_INFO( "audio loopback, mic2 -> headset\n");
		}
	}	else if ('g' == cmd) {//another  mic -> headset
		lc1132_init();
		if(lc1132_codec_info->mic1_mmic){
			nData =comip_lc1132_i2c_read(LC1132_R0);
			nData &=0x8f;
			nData |=0x50;// mic2 zcen; mic2 pga unmute; mic2 pga enable
			comip_lc1132_i2c_write(LC1132_R0,nData);
			comip_lc1132_i2c_write(LC1132_R40,0x4f);
			comip_lc1132_i2c_write(LC1132_R84,0x76);
			nData =comip_lc1132_i2c_read(LC1132_R85);
			nData &=0x78;
			nData |=0x83;
			comip_lc1132_i2c_write(LC1132_R85,nData);

			comip_lc1132_reg_bit_write(LC1132_R90,0x01,1);
			comip_lc1132_reg_bit_write(LC1132_R89,0x01,1);
			lc1132_codec_hp_output_set(PATH_OPEN);

			comip_lc1132_i2c_write(LC1132_R04,0x7e);
			CODEC_LC1132_PRINTK_INFO( "audio loopback, mic2 -> headset\n");
		}else{
			nData = comip_lc1132_i2c_read(LC1132_R0);
			nData &= 0xf8;
			nData |= 0x05; // mic1 zcen; mic1 pga unmute; mic1 pga enable
			comip_lc1132_i2c_write(LC1132_R0, nData);
			comip_lc1132_i2c_write(LC1132_R24, 0x4f);// mic1 pga gain
			comip_lc1132_i2c_write(LC1132_R84, 0x76);// VMIC SEL,MICBEN
			nData = comip_lc1132_i2c_read(LC1132_R85);
			nData &= 0x1f;
			nData |= 0x60; // Sidetone sel MIC1,SEL mic1
			comip_lc1132_i2c_write(LC1132_R85, nData);

			nData = comip_lc1132_i2c_read(LC1132_R90);
			nData |= 0x01;
			comip_lc1132_i2c_write(LC1132_R90, nData); // Sidetone sel HP
			nData = comip_lc1132_i2c_read(LC1132_R89);
			nData |= 0x01;
			comip_lc1132_i2c_write(LC1132_R89, nData); // Sidetone sel HP
			lc1132_codec_hp_output_set(PATH_OPEN);
			comip_lc1132_i2c_write(LC1132_R04, 0x7e); // Stone PGA
			CODEC_LC1132_PRINTK_INFO( "audio loopback, mic1  -> headset\n");
		}

	}else if ('x' == cmd) {//colse audio loopback
		lc1132_codec_hp_output_set(PATH_CLOSE);
		lc1132_codec_rec_output_set(PATH_CLOSE);
		lc1132_codec_spk_output_set(PATH_CLOSE);

		comip_lc1132_i2c_write(LC1132_R0, 0x22); // ADC mute and  disable
		comip_lc1132_i2c_write(LC1132_R84, 0x74);// VMIC SEL,MICB disable

		nData = comip_lc1132_i2c_read(LC1132_R89);
		nData &= 0xfe;
		comip_lc1132_i2c_write(LC1132_R89, nData);// HPL Side Tone unselect
		nData = comip_lc1132_i2c_read(LC1132_R90);
		nData &= 0xfe;
		comip_lc1132_i2c_write(LC1132_R90, nData);// HPR Side Tone unselect

		nData = comip_lc1132_i2c_read(LC1132_R91);
		nData &= 0xf4;
		comip_lc1132_i2c_write(LC1132_R91, nData);// REC Side Tone unselect
		lc1132_deinit();
		CODEC_LC1132_PRINTK_INFO( "colse audio loopback\n");
	} else {
		CODEC_LC1132_PRINTK_ERR( "unknow opt!\n");
		goto error;
	}

	return count;

error:
	CODEC_LC1132_PRINTK_ERR( "r/w index(0x%%2x) value(0x%%2x)\n");
	return count;
}

static ssize_t lc1132_audio_proc_read(char *page,int count,loff_t *eof)
{

	char *buf  = page;
	char *next = buf;
	int  t;
	unsigned size = count;
	struct comip_1132_audio_tbl *audio_gain_tbl = (lc1132_codec_info->paudio->paudio_table);

	t = scnprintf(next, size, "codec i2s gain value:\n");
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_speaker: adc: %d pga: %d\n", audio_gain_tbl->i2s_ogain_speaker_daclevel, audio_gain_tbl->i2s_ogain_speaker_cdvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_receiver: adc: %d pga: %d\n", audio_gain_tbl->i2s_ogain_receiver_daclevel, audio_gain_tbl->i2s_ogain_receiver_epvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_headset: adc: %d pga: %d\n", audio_gain_tbl->i2s_ogain_headset_daclevel, audio_gain_tbl->i2s_ogain_headset_hpvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_ogain_speaker_headset: adc: %d hp_pga: %d, spk_pga: %d\n", audio_gain_tbl->i2s_ogain_spk_hp_daclevel, audio_gain_tbl->i2s_ogain_spk_hp_hpvol, audio_gain_tbl->i2s_ogain_spk_hp_cdvol);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_igain_main_mic: adc: %d pga: %d\n", audio_gain_tbl->i2s_igain_mic_adclevel, audio_gain_tbl->i2s_igain_mic_pgagain);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_igain_headset_mic: adc: %d pga: %d\n", audio_gain_tbl->i2s_igain_headsetmic_adclevel, audio_gain_tbl->i2s_igain_headsetmic_pgagain);
	size -= t;
	next += t;
	t = scnprintf(next, size, "i2s_igain_line: pga: %d\n", audio_gain_tbl->i2s_igain_line_linvol);
	size -= t;
	next += t;

	*eof = 1;
	return count - size;


}

static int lc1132_audio_proc_write(const char __user *buffer,unsigned long count)
{
	static char kbuf[200];
	char *buf = kbuf;
	unsigned int gain1, gain2, gain3;
	static char cbuf[200];
	char *cmdstring = cbuf;

	struct comip_1132_audio_tbl *audio_gain_tbl = (lc1132_codec_info->paudio->paudio_table);

	if (count >= 200)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	sscanf(buf, "%s %d %d %d", cmdstring, &gain1, &gain2, &gain3);
	CODEC_LC1132_PRINTK_ERR("audio gain %s write gain1:%d, gain2:%d, gain3:%d\n",cmdstring, gain1, gain2, gain3);

	if (strcmp("spk",cmdstring) == 0) {
		audio_gain_tbl->i2s_ogain_speaker_daclevel = gain1;
		audio_gain_tbl->i2s_ogain_speaker_cdvol = gain2;
	} else if (strcmp("hp",cmdstring) == 0) {
		audio_gain_tbl->i2s_ogain_headset_daclevel = gain1;
		audio_gain_tbl->i2s_ogain_headset_hpvol = gain2;
	} else if (strcmp("rec",cmdstring) == 0) {
		audio_gain_tbl->i2s_ogain_receiver_daclevel = gain1;
		audio_gain_tbl->i2s_ogain_receiver_epvol = gain2;
	} else if (strcmp("spk_hp",cmdstring) == 0) {
			
		audio_gain_tbl->i2s_ogain_spk_hp_daclevel = gain1;
		audio_gain_tbl->i2s_ogain_spk_hp_hpvol = gain2;
		audio_gain_tbl->i2s_ogain_spk_hp_cdvol = gain3;
	} else if (strcmp("main_mic",cmdstring) == 0) {
				
		audio_gain_tbl->i2s_igain_mic_adclevel = gain1;
		audio_gain_tbl->i2s_igain_mic_pgagain = gain2;

	} else if (strcmp("hp_mic",cmdstring) == 0) {
					
		audio_gain_tbl->i2s_igain_headsetmic_adclevel = gain1;
		audio_gain_tbl->i2s_igain_headsetmic_pgagain = gain2;
	} else if (strcmp("line",cmdstring) == 0) {
		audio_gain_tbl->i2s_igain_line_linvol = gain1;

	} else if (strcmp("update",cmdstring) == 0) {
		lc1132_i2s_ogain_to_hw();
		lc1132_i2s_igain_to_hw();

	} else {
		CODEC_LC1132_PRINTK_ERR( "unknow opt!\n");
		goto error;
	}
	return count;

error:
	CODEC_LC1132_PRINTK_ERR( "r/w index(0x%%2x) value(0x%%2x)\n");
	return count;
}


static int lc1132_codec_eq_proc_read(char* page,int count,loff_t*eof)
{
	char* next =page;
	int t;
	int size=count;
	struct comip_1132_audio_tbl *audio_gain_tbl = (lc1132_codec_info->paudio->paudio_table);

	t=scnprintf(next,size,"lc1132 codec reg51 value is: %d\n",audio_gain_tbl->alc_reg51);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg52 value is: %d\n",audio_gain_tbl->alc_reg52);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg53 value is: %d\n",audio_gain_tbl->alc_reg53);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg54 value is: %d\n",audio_gain_tbl->alc_reg54);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg55 value is: %d\n",audio_gain_tbl->alc_reg55);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg56 value is: %d\n",audio_gain_tbl->alc_reg56);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg57 value is: %d\n",audio_gain_tbl->alc_reg57);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg58 value is: %d\n",audio_gain_tbl->alc_reg58);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg59 value is: %d\n",audio_gain_tbl->alc_reg59);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg60 value is: %d\n",audio_gain_tbl->alc_reg60);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg61 value is: %d\n",audio_gain_tbl->alc_reg61);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg62 value is: %d\n",audio_gain_tbl->alc_reg62);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg63 value is: %d\n",audio_gain_tbl->alc_reg63);
	size-=t;
	next+=t;
	t=scnprintf(next,size,"lc1132 codec reg64 value is: %d\n",audio_gain_tbl->alc_reg64);
	size-=t;
	next+=t;

	*eof=1;
	return count-size;
}


static int lc1132_codec_eq_proc_write(const char __user*buffer,unsigned long count)			//ylz amended 13/6/19
{
	static char kbuf[200];
	static char cbuf[200];
	char* buf=kbuf;
	char* reg_name=cbuf;
	unsigned int  value_set;
	struct comip_1132_audio_tbl *audio_gain_tbl = (lc1132_codec_info->paudio->paudio_table);

	if(count>=200)
		return -EINVAL;
	if(copy_from_user(buf,buffer,count))
		return -EFAULT;
	sscanf(buf,"%s %x",reg_name,&value_set);
	printk(KERN_INFO" lc1132_codec_eq_proc_write: Register name:%s,Register value:0x%x\n",reg_name,value_set);

	if(strcmp("eq_r51",reg_name)==0){

		audio_gain_tbl->alc_reg51= (u8)value_set;

	}else if(strcmp("eq_r52",reg_name)==0){

		audio_gain_tbl->alc_reg52= (u8)value_set;

	}else if(strcmp("eq_r53",reg_name)==0){

		audio_gain_tbl->alc_reg53= (u8)value_set;

	}else if(strcmp("eq_r54",reg_name)==0){

		audio_gain_tbl->alc_reg54= (u8)value_set;

	}else if(strcmp("eq_r55",reg_name)==0){

		audio_gain_tbl->alc_reg55= (u8)value_set;

	}else if(strcmp("eq_r56",reg_name)==0){

		audio_gain_tbl->alc_reg56= (u8)value_set;

	}else if(strcmp("eq_r57",reg_name)==0){

		audio_gain_tbl->alc_reg57= (u8)value_set;

	}else if(strcmp("eq_r58",reg_name)==0){

		audio_gain_tbl->alc_reg58= (u8)value_set;

	}else if(strcmp("eq_r59",reg_name)==0){

		audio_gain_tbl->alc_reg59= (u8)value_set;
	
	}else if(strcmp("eq_r60",reg_name)==0){

		audio_gain_tbl->alc_reg60= (u8)value_set;

	}else if(strcmp("eq_r61",reg_name)==0){

		audio_gain_tbl->alc_reg61= (u8)value_set;

	}else if(strcmp("eq_r62",reg_name)==0){

		audio_gain_tbl->alc_reg62= (u8)value_set;

	}else if(strcmp("eq_r63",reg_name)==0){

		audio_gain_tbl->alc_reg63= (u8)value_set;

	}else if(strcmp("eq_r64",reg_name)==0){

		audio_gain_tbl->alc_reg64= (u8)value_set;

	}else if(strcmp("update",reg_name)==0){

		codec_lc1132_enable_alc(1);

	}else{
		printk("eq proc write: unknown opt!!!\n");
		goto error;
	}
	return count;
	
	error:
		return count;
}



#endif
static int codec_lc1132_playback_init(void)
{
	int nRst =0;
	CODEC_LC1132_PRINTK_TEST("lc1132_playback_init,play %d,voice %d,record %d,fm %d\n",\
		lc1132_codec_info->type_play,lc1132_codec_info->type_voice,lc1132_codec_info->type_record,lc1132_codec_info->fm);

	lc1132_codec_info->nType_audio = AUDIO_TYPE;
	lc1132_codec_info->type_play = 1;

	nRst = lc1132_codec_open(lc1132_codec_info->nType_audio);
	return nRst;
}
static int codec_lc1132_playback_deinit(void)
{
	int nRst =0;
	CODEC_LC1132_PRINTK_TEST("codec_lc1132_playback_deinit:play %d,voice %d,record %d,fm %d\n",\
		lc1132_codec_info->type_play,lc1132_codec_info->type_voice,lc1132_codec_info->type_record,lc1132_codec_info->fm);

	lc1132_codec_info->nType_audio = AUDIO_TYPE;
	lc1132_codec_info->type_play = 0;
	nRst = lc1132_codec_close(lc1132_codec_info->nType_audio);
return nRst;
}
static int codec_lc1132_capture_init(void)
{
	int nRst =0;
	CODEC_LC1132_PRINTK_TEST("lc1132_capture_init,play %d,voice %d,record %d,fm %d\n",\
		lc1132_codec_info->type_play,lc1132_codec_info->type_voice,lc1132_codec_info->type_record,lc1132_codec_info->fm);
	lc1132_codec_info->nType_audio = AUDIO_CAPTURE_TYPE;
	lc1132_codec_info->type_record = 1;
	nRst = lc1132_codec_open(lc1132_codec_info->nType_audio);
	return nRst;
}
static int codec_lc1132_capture_deinit(void)
{
	int nRst =0;
	CODEC_LC1132_PRINTK_TEST("lc1132_capture_deinit,play %d,voice %d,record %d,fm %d\n",\
		lc1132_codec_info->type_play,lc1132_codec_info->type_voice,lc1132_codec_info->type_record,lc1132_codec_info->fm);
	lc1132_codec_info->nType_audio = AUDIO_CAPTURE_TYPE;
	lc1132_codec_info->type_record = 0;
	nRst = lc1132_codec_close(lc1132_codec_info->nType_audio);
	return nRst;
}

static int comip_1132_startup(struct snd_pcm_substream *substream,
                              struct snd_soc_dai *dai)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_codec *codec =rtd->codec;
	CODEC_LC1132_PRINTK_TEST("comip_1132_startup function\n");
	//<---------------------------------------------------->
	lc1132_audio_clk_enable(1);
	lc1132_init();
	lc1132_audio_init();
	return 0;
}

static void comip_1132_shutdown(struct snd_pcm_substream *substream,
                                struct snd_soc_dai *dai)
{
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			codec_lc1132_playback_deinit();
	} else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			codec_lc1132_capture_deinit();
	}
//	lc1132_audio_clk_enable(0);
}
static int comip_1132_hw_params(struct snd_pcm_substream *substream,
                                struct snd_pcm_hw_params *params,
                                struct snd_soc_dai *dai)
{
	CODEC_LC1132_PRINTK_TEST("comip_1132_hw_params function\n");

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		codec_lc1132_playback_init();
	} else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		codec_lc1132_capture_init();
	}

	return 0;
}

static void lc1132_load_default_pcmtbl(struct codec_voice_tbl *ptbl)
{
	CODEC_LC1132_PRINTK_TEST("lc1132_load_default_pcmtbl start\n");

	if (!ptbl)
		return;

	ptbl->gain_in_level0 = 3 * 8;
	ptbl->gain_in_level1 = 96;
	ptbl->gain_in_level2 = 0x00;			/* no use for lc1132 */
	ptbl->gain_in_level3 = 0x00;			/* no use for lc1132 */

	ptbl->gain_out_level0_class0 = 32 * 2;
	ptbl->gain_out_level0_class1 = 39 * 2;
	ptbl->gain_out_level0_class2 = 45 * 2;
	ptbl->gain_out_level0_class3 = 51 * 2;
	ptbl->gain_out_level0_class4 = 57 * 2;
	ptbl->gain_out_level0_class5 = 63 * 2;
	ptbl->gain_out_level1 = 96;
	ptbl->gain_out_level2 = 0x00;			/* no use for lc1132 */
	ptbl->gain_out_level3 = 0x00;			/* no use for lc1132 */
	ptbl->gain_out_level4 = 0x00;			/* no use for lc1132 */
	ptbl->gain_out_level5 = 0x00;			/* no use for lc1132 */
	ptbl->gain_out_level6 = 0x00;			/* no use for lc1132 */
	
	ptbl->sidetone_reserve_1 = 0;			/* close hard sidetone at default */
	
}

static int comip_1132_set_dai_sysclk(struct snd_soc_dai *codec_dai,
                                     int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct comip_lc1132_device_info_struct *comip_1132_codec = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s clk_id: %d, freq: %u, dir: %d\n", __func__,
	         clk_id, freq, dir);

	/* Anything between 256fs*8Khz and 512fs*48Khz should be acceptable
	   because the codec is slave. Of course limitations of the clock
	   master (the IIS controller) apply.
	   We'll error out on set_hw_params if it's not OK */
	if ((freq >= (256 * 8000)) && (freq <= (512 * 48000))) {
		comip_1132_codec->sysclk = freq;
		return 0;
	}

	printk(KERN_DEBUG "%s unsupported sysclk\n", __func__);
	return -EINVAL;
}

static int comip_1132_set_dai_fmt(struct snd_soc_dai *codec_dai,
                                  unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct comip_lc1132_device_info_struct *comip_1132_codec = snd_soc_codec_get_drvdata(codec);

//	CODEC_LC1132_PRINTK("comip_1132_set_dai_fmt fmt: 0x%08X\n",fmt);


	/* We can't setup DAI format here as it depends on the word bit num */
	/* so let's just store the value for later */
	comip_1132_codec->dai_fmt = fmt;

	return 0;
}

static int comip_1132_set_bias_level(struct snd_soc_codec *codec,
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

static struct snd_soc_dai_ops comip_1132_dai_ops = {
	.startup		= comip_1132_startup,
	.shutdown		= comip_1132_shutdown,
	.hw_params		= comip_1132_hw_params,
//	.digital_mute	= comip_1132_mute,
	.set_sysclk		= comip_1132_set_dai_sysclk,
	.set_fmt		= comip_1132_set_dai_fmt,
};

static struct snd_soc_dai_driver comip_1810_dai = {
	.name = "comip_hifi",
	/* playback capabilities */
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1132_RATES,
		.formats = COMIP_1132_FORMATS,
	},
	/* capture capabilities */
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1132_RATES,
		.formats = COMIP_1132_FORMATS,
	},
	/* I2S operations */
	.ops = &comip_1132_dai_ops,
};

static void load_default_calibration_data(struct comip_1132_voice_struct* pdata)
{

	lc1132_load_default_pcmtbl(pdata->pvoice_table);
}


static int lc1132_codec_jack_mask(int nMask)
{
	u8 nData=0x00;

	if(nMask) {
		lc1132_i2c_read(LC1132_REG_JKHSMSK,&nData);
		nData = (nData&0xcc)|0x03; //JACK in/out   interrupt disable,interrupt mask
		lc1132_i2c_write(LC1132_REG_JKHSMSK,nData);
	} else {
		lc1132_i2c_read(LC1132_REG_JKHSMSK,&nData);
		nData = (nData&0xcc)|0x30; //JACK in/out  interrupt disable,interrupt mask
		lc1132_i2c_write(LC1132_REG_JKHSMSK,nData);
	}
	return 0;
}

static int lc1132_codec_hook_mask(int nMask)
{
	u8 nData;
	CODEC_LC1132_PRINTK_DEBUG("lc1132_codec_hook_mask :%d \n", nMask);

	if(nMask) {
		lc1132_i2c_read(LC1132_REG_JKHSMSK,&nData);
		nData = (nData&0x33)|0x0c; //hook up/down interrupt disable,interrupt mask
		lc1132_i2c_write(LC1132_REG_JKHSMSK,nData);
	} else {
		lc1132_i2c_read(LC1132_REG_JKHSIRQST,&nData);
		nData |= 0x0f;
		lc1132_i2c_write(LC1132_REG_JKHSIRQST,nData); //clr irq
		nData &= 0xf0;
		lc1132_i2c_write(LC1132_REG_JKHSIRQST,nData); //clr irq

		lc1132_i2c_read(LC1132_REG_JKHSMSK,&nData);
		nData = (nData&0x33)|0xc0; //hook up/down   interrupt disable,interrupt mask
		lc1132_i2c_write(LC1132_REG_JKHSMSK,nData);
	}
	return 0;
}


static int lc1132_codec_hook_enable(int nValue)
{
	u8 nData;
	
	CODEC_LC1132_PRINTK_DEBUG("lc1132_codec_hook_enable : %d\n",nValue);
	if(nValue) {
		comip_lc1132_reg_bit_write(LC1132_R87,LC1132_BIT_7,1); // open LDO support for LDO
		nData =	comip_lc1132_i2c_read(LC1132_R96);
		nData |= 0xe1;
		comip_lc1132_i2c_write(LC1132_R96,nData); //open VMID
		comip_lc1132_reg_bit_write(LC1132_R84,LC1132_BIT_1|LC1132_BIT_0,1);// enable MICBIAS & MICBIASSW
		//1 Del Bit 0
		lc1132_i2c_read(LC1132_REG_JKHSIRQST,&nData);
		nData |= 0x40;
		lc1132_i2c_write(LC1132_REG_JKHSIRQST,nData);
	} else {
		//Reg 0x9a: bit6 hook disable;
		lc1132_i2c_read(LC1132_REG_JKHSIRQST,&nData);
		nData &= 0xbf;
		lc1132_i2c_write(LC1132_REG_JKHSIRQST,nData);
		if(!(lc1132_codec_info->type_play||lc1132_codec_info->type_record||lc1132_codec_info->type_voice||\
			lc1132_codec_info->fm)) {
			comip_lc1132_reg_bit_write(LC1132_R87,LC1132_BIT_7,0); // open LDO support for LDO
			nData =	comip_lc1132_i2c_read(LC1132_R96);
			nData &= 0x1f;
			comip_lc1132_i2c_write(LC1132_R96,nData); //open VMID
		}
		if(!(lc1132_codec_info->type_record || lc1132_codec_info->type_voice)) {
			comip_lc1132_reg_bit_write(LC1132_R84,LC1132_BIT_1,0);// disable MICBIAS 

			if(lc1132_codec_info->nPath_in == DD_HEADSET)
				comip_lc1132_reg_bit_write(LC1132_R84,LC1132_BIT_1|LC1132_BIT_0,0);// disable MICBIAS 
		}
		if(!(lc1132_codec_info->type_play||lc1132_codec_info->type_record||lc1132_codec_info->type_voice||\
			lc1132_codec_info->fm))
			lc1132_deinit();
	}
	lc1132_codec_info->comip_codec_jackst = nValue;
	return 0;
}
int lc1132_codec_irq_type_get(void)
{
	int irs_type = 0;
	u8 nData1,nData2;
	lc1132_i2c_read(LC1132_REG_JKHSIRQST,&nData1);
	lc1132_i2c_read(LC1132_REG_JKHSMSK,&nData2);
	if(nData1&0x03) {
		irs_type = COMIP_SWITCH_INT_JACK;
	} else if (nData1&0x0c) {
		irs_type = COMIP_SWITCH_INT_BUTTON;
	} else {
		irs_type = -1;
		return irs_type;
	}
	CODEC_LC1132_PRINTK_INFO("irqSt=0x%x,irqMsk=0x%x, irs_type=%d\n",nData1,nData2,irs_type);
	nData1 |= 0x0f;
	lc1132_i2c_write(LC1132_REG_JKHSIRQST,nData1); //clr irq
	nData1 &= 0xf0;
	lc1132_i2c_write(LC1132_REG_JKHSIRQST,nData1); //clr irq

	return irs_type;
}

int lc1132_codec_jack_status_get(int *hp_status)
{
	u8 nData;
	*hp_status = 0;
	lc1132_i2c_read(LC1132_REG_JKHSIRQST,&nData);

	if(nData&0x10)
		*hp_status = 1;

	//printk("get jack status:JKHSIRQST=0x%x,jack=%d,hook=%d\n",nData,jack_status,hook_status);
	return 0;
}
int lc1132_codec_hook_status_get(int *hookswitch_status)
{
	u8 nData;

	lc1132_i2c_read(LC1132_REG_JKHSIRQST,&nData);
	if(nData&0x20) {
		*hookswitch_status = SWITCH_BUTTON_RELEASE;
	} else {
		*hookswitch_status = SWITCH_BUTTON_PRESS;
	}
	//printk("get hook status :JKHSIRQST=0x%x\n",nData);

	return 0;
}

static int lc1132_codec_clr_irq(void)
{
	return 0;
}
static int lc1132_codec_get_headphone_key_num(int *button_type)
{
	*button_type = SWITCH_KEY_HOOK;
	return 0;
}

static int lc1132_micbias_power(int onoff)
{
	return 0;
}

static int lc1132_button_active_level_set(int level)
{
	return 0;
}

static irqreturn_t lc1132_codec_isr(int irq, void* dev)
{
	schedule_work(&lc1132_codec_info->codec_irq_work);
	return IRQ_NONE;  /* notice: make sure irq can call pmic isr ,so return IRQ_NONE*/
}
static void lc1132_codec_irq_work(struct work_struct *work)
{
	int irq_type;
	irq_type = lc1132_codec_irq_type_get();
	if(irq_type < 0)
		return ;
	disable_irq_nosync(lc1132_codec_info->irq);
	lc1132_codec_info->comip_switch_isr(irq_type);
}
static int lc1132_switch_irq_handle_done(int type)
{
	lc1132_codec_clr_irq();
	enable_irq(lc1132_codec_info->irq);
	return 0;
}

static int lc1132_switch_irq_init(struct comip_switch_hw_info *info)
{
	int ret;
	lc1132_codec_info->comip_switch_isr =  info->comip_switch_isr;
	lc1132_codec_info->irq_gpio = info->irq_gpio;

	ret = gpio_direction_input(info->irq_gpio);
	if (ret < 0) {
		printk("%s: Failed to detect GPIO input.\n", __func__);
		goto failed;
	}
	lc1132_codec_info->irq = gpio_to_irq(info->irq_gpio);

	/* Request irq. */
	ret = request_irq(lc1132_codec_info->irq, lc1132_codec_isr,
			  IRQF_TRIGGER_HIGH | IRQF_SHARED, "comip_switch irq", info);

	if (ret) {
		printk("%s: IRQ already in use.\n", __func__);
		goto failed;
	}
	INIT_WORK(&lc1132_codec_info->codec_irq_work, lc1132_codec_irq_work);

	
	enable_irq_wake(lc1132_codec_info->irq);

	return 0;

failed:
	gpio_free(info->irq_gpio);
	return ret;
}

static int lc1132_switch_deinit(void)
{
	free_irq(lc1132_codec_info->irq, NULL);
	gpio_free(lc1132_codec_info->irq_gpio);
	return 0;
}

struct comip_switch_ops lc1132_codec_switch_ops = {
	.hw_init = lc1132_switch_irq_init,
	.hw_deinit = lc1132_switch_deinit,
	.jack_status_get = lc1132_codec_jack_status_get,
	.button_status_get = lc1132_codec_hook_status_get,
	.jack_int_mask = lc1132_codec_jack_mask,
	.button_int_mask = lc1132_codec_hook_mask,
	.button_enable = lc1132_codec_hook_enable,
	.micbias_power = lc1132_micbias_power,
	.jack_active_level_set = lc1132_codec_hp_detpol_set,
	.button_active_level_set = lc1132_button_active_level_set,
	.button_type_get = lc1132_codec_get_headphone_key_num,
	.irq_handle_done = lc1132_switch_irq_handle_done,
};

static int comip_1132_soc_probe(struct snd_soc_codec *codec)
{
	int err=0;
	CODEC_LC1132_PRINTK_INFO("comip_1132_soc_probe \n");

	lc1132_codec_info = kzalloc(sizeof(struct comip_lc1132_device_info_struct), GFP_KERNEL);
	if (lc1132_codec_info == NULL)
		return -ENOMEM;

	lc1132_codec_info->paudio= kzalloc(sizeof(struct comip_1132_audio_struct), GFP_KERNEL);
	if (lc1132_codec_info->paudio == NULL)
		return -ENOMEM;

	lc1132_codec_info->pvoice= kzalloc(sizeof(struct comip_1132_voice_struct), GFP_KERNEL);
	if (lc1132_codec_info->pvoice == NULL)
		return -ENOMEM;

	lc1132_codec_info->pvoice->pvoice_table = kzalloc(CODEC_CAL_TBL_SIZE, GFP_KERNEL);
	if (lc1132_codec_info->pvoice->pvoice_table == NULL)
		return -ENOMEM;

	lc1132_codec_info->paudio->paudio_table = kzalloc(sizeof(struct comip_1132_audio_tbl), GFP_KERNEL);
	if (lc1132_codec_info->paudio->paudio_table == NULL)
		return -ENOMEM;
	lc1132_codec_info->paudio->dac_stereo_sel = 0;

	lc1132_codec_info->mclk = clk_get(NULL, "clkout3_clk");
	if (IS_ERR(lc1132_codec_info->mclk)) {
		CODEC_LC1132_PRINTK_ERR( "Cannot get codec input clock\n");
		err = PTR_ERR(lc1132_codec_info->mclk);
		return 0;
	}
	#if 1
	lc1132_codec_info->ext_clk = clk_get(NULL, "clkout4_clk");
	if (IS_ERR(lc1132_codec_info->ext_clk)) {
		CODEC_LC1132_PRINTK_ERR( "Cannot get codec clkout4_clk clock\n");
		err = PTR_ERR(lc1132_codec_info->ext_clk);
		return 0;
	}
	#endif
	mutex_init(&lc1132_codec_info->mclk_mutex);
	mutex_init(&lc1132_codec_info->ext_mutex);

	comip_1132_set_bias_level(codec,SND_SOC_BIAS_OFF);

	lc1132_codec_info->pcodec = codec;
	load_default_calibration_data(lc1132_codec_info->pvoice);
	snd_soc_codec_set_drvdata(codec,lc1132_codec_info);

	lc1132_codec_info->pvoice->cur_volume = 3;

	comip_1132_set_bias_level(codec,SND_SOC_BIAS_PREPARE);
	lc1132_codec_info->mic_num = SINGLE_MIC;

	lc1132_codec_info->mic1_mmic = pintdata->main_mic_flag;

	lc1132_codec_info->spk_use_aux = pintdata->aux_pa_gpio;

	if(lc1132_codec_info->spk_use_aux) {
		comip_mfp_config(lc1132_codec_info->spk_use_aux ,MFP_PIN_MODE_GPIO);
		gpio_request(lc1132_codec_info->spk_use_aux ,"codec_pa_gpio");
		gpio_direction_output(lc1132_codec_info->spk_use_aux,0);
		/* notice:If the external PA is controlled by two gpio pins, please add the code below */
	}	

	lc1132_deinit();

	#if defined(CONFIG_SWITCH_COMIP_CODEC)
	comip_switch_register_ops(&lc1132_codec_switch_ops);
	#endif
	return 0;
}

/* power down chip */
static int comip_1132_soc_remove(struct snd_soc_codec *codec) 
{
	struct comip_lc1132_device_info_struct *comip_1132_codec;
	comip_1132_codec = snd_soc_codec_get_drvdata(codec);
	if(comip_1132_codec != NULL) {
		if(NULL !=comip_1132_codec->pvoice->pvoice_table) {
			kfree(comip_1132_codec->pvoice->pvoice_table);
			comip_1132_codec->pvoice->pvoice_table = NULL;
		}
		kfree(comip_1132_codec);
	}
	comip_1132_set_bias_level(codec,SND_SOC_BIAS_OFF);
	if(lc1132_codec_info->mclk)
		clk_put(lc1132_codec_info->mclk);

	if(lc1132_codec_info) {
		kfree(lc1132_codec_info);
		lc1132_codec_info = NULL;
	}

	return 0;
}

#if defined(CONFIG_PM)
static int comip_1132_soc_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int comip_1132_soc_resume(struct snd_soc_codec *codec)
{
	return 0;
}
#else
#define comip_1132_soc_suspend NULL
#define comip_1132_soc_resume NULL
#endif /* CONFIG_PM */

static int codec_lc1132_get_mixer_info(struct snd_leadcore_mixer_info* _userInfo)
{
	strcpy(_userInfo->codecName,"lc1132");
	_userInfo->multiService = 1;
	CODEC_LC1132_PRINTK_DEBUG("name= %s,ser=%d \n",_userInfo->codecName,_userInfo->multiService);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_1132 = {
	.probe =        comip_1132_soc_probe,
	.remove =       comip_1132_soc_remove,
	.suspend =      comip_1132_soc_suspend,
	.resume =       comip_1132_soc_resume,
	.reg_cache_size = sizeof(comip_lc1132_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = comip_lc1132_reg,
	.reg_cache_step = 1,
	//.read = comip_lc1132_i2c_read,
	//.write = comip_lc1132_i2c_write,
	.set_bias_level = comip_1132_set_bias_level,
};
struct comip_codec_ops comip_codec_1132_ops = {
	.set_output_path = lc1132_codec_output_path_set,
	.set_output_volume = lc1132_codec_output_gain_set,
	.set_input_gain = NULL,
	.set_output_samplerate = NULL,
	.set_input_samplerate = NULL,
	.set_input_mute = lc1132_codec_set_input_mute,
	.set_output_mute = lc1132_codec_set_output_mute,
	.set_input_path = lc1132_codec_input_path_set,
	.enable_voice = lc1132_codec_enable_voice,
	.enable_fm = lc1132_codec_fm_enable,
	.mp_powersave = lc1132_codec_mp_power_save,
	.calibrate_data_show = lc1132_codec_calibrate_data_show,
	.calibrate_data_store = lc1132_codec_calibrate_data_store,
	.comip_register_show = lc1132_codec_comip_register_show,
	.comip_register_store = lc1132_codec_comip_register_store,
	.set_double_mics_mode = lc1132_codec_set_double_mics_mode,
	.enable_alc = codec_lc1132_enable_alc,
#ifdef CODEC_SOC_PROC
	.codec_proc_read = lc1132_proc_read,
	.codec_proc_write = lc1132_proc_write,
	.codec_audio_proc_read = lc1132_audio_proc_read,
	.codec_audio_proc_write = lc1132_audio_proc_write,
	.codec_eq_proc_read = lc1132_codec_eq_proc_read,
	.codec_eq_proc_write = lc1132_codec_eq_proc_write,
#endif
	.codec_get_mixer_info = codec_lc1132_get_mixer_info,
};

static int __init comip_1132_codec_init(void)
{
	CODEC_LC1132_PRINTK_INFO("comip_1132_codec_init \n");
	leadcore_register_codec_info(&soc_codec_dev_1132,&comip_1810_dai,&comip_codec_1132_ops);
	return 0;
}

static void __exit comip_1132_codec_exit(void)
{
	CODEC_LC1132_PRINTK_INFO("comip_1132_codec_exit \n");
}

rootfs_initcall(comip_1132_codec_init);
module_exit(comip_1132_codec_exit);

MODULE_AUTHOR("yangshunlong <yangshunlong@leadcoretech.com>");
MODULE_DESCRIPTION("comip 1132 codec driver");
MODULE_LICENSE("GPL");


