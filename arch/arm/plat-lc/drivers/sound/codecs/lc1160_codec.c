/*
 * lc1160_codec.c -- LC1160  ALSA SoC Audio driver
 *
 * Copyright (C) 2013 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <plat/lc1160_codec.h>

struct lc1160_codec {
	struct snd_soc_codec *codec;
	const char *name;
	struct clk *mclk;
	struct clk *eclk;
	struct snd_soc_jack *jack;
	bool jack_mic;
	int jack_polarity;
	int (*pcm_switch)(int enable);
	int (*codec_power_save)(int enable, int clk_id);
	int (*ear_switch)(int enable);
	int (*pa_enable)(int enable);
	int sysclk;
	int dai_fmt;
	int irq;
	bool choose_mic_capture;
	bool serial_port_state;
	struct delayed_work irq_work;
	struct mutex mutex;
};
/*
 * codec mclk set,audio mclk=11mhz,voice mclk=2mhz
 */

#define SWITCH_TO_GPIO		1
#define SWITCH_TO_CLK		0
#define MCLK_ID		0
#define EXTCLK_ID	1
#define LC1160_MCLK_RATE				11289600
#define LC1160_ECLK_RATE				2048000
#define CODEC_IRQ_DEBOUNCE_TIME 	(HZ / 100)

/* In-data addresses are hard-coded into the reg-cache values */
static u8 lc1160_codec_reg[LC1160_CACHEREGNUM] = {
	0x00,																					   /*R0*/
	0xAD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x72, 0xFA,   /*R01~R10*/
	0x3C, 0x72, 0x1F, 0xE7, 0x00, 0x28, 0x00, 0x32, 0x00, 0x3F,   /*R11~R20*/
	0x67, 0x3F, 0xE7, 0x00, 0x28, 0x00, 0x32, 0x00, 0x3F, 0xFF,   /*R21~R30*/
	0xE0, 0x60, 0x67, 0x67, 0x00, 0x07, 0x23, 0x7F, 0x00, 0x7F,   /*R31~R40*/
	0x00, 0x32, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x33, 0x00,   /*R41~R50*/
	0x07, 0x23, 0xE7, 0x67, 0x7F, 0x00, 0x7F, 0x00, 0x32, 0x00,   /*R51~R60*/
	0x1F, 0x07, 0x26, 0xD5, 0x8F, 0x82, 0x0A, 0x34, 0x2C, 0x00,   /*R61~R70*/
	0x1B, 0x1B, 0x19, 0x19, 0x07, 0x08, 0x27, 0x00, 0x00, 0x00,   /*R71~R80*/
	0x00,																						/*R81 Read Only*/
	0x4c, //*LDOA15 For Codec,addr:0x3b*								   /**/
	/* Jack & Hookswitch,addr:0xa7,0xa8,0xa9,0xaa,0xab*/
	0x00,0x04,0x04,0x04,0x00,
	0x00, /*DBB_PCM_SWITCH*/
	0x00, /*PA_ENABLE*/
};
/*
 * read lc1160 register cache
 */
static inline unsigned int lc1160_read_reg_cache(struct snd_soc_codec *codec,
						unsigned int reg)
{
	u8 *cache = codec->reg_cache;
	u8 value;
	if (reg > LC1160_CACHEREGNUM)
		return -EIO;

	if(reg < LC1160_LDO) {
		 lc1160_reg_read(reg+0xAC,&value);
		 return value;
	}else if (reg == LC1160_LDO) {
		lc1160_reg_read(0x3B,&value);
		return value;
	}else if (reg == DBB_PCM_SWITCH)
		return cache[DBB_PCM_SWITCH];
	else if (reg==PA_ENABLE)
		return cache[PA_ENABLE];
	else lc1160_reg_read(reg-0x53+0xa7,&value);

	cache[reg]=value;
	return value;
}
/*
 * write lc1160 register cache
 */
static inline void lc1160_write_reg_cache(struct snd_soc_codec *codec,
						u8 reg, u8 value)
{
	u8 *cache = codec->reg_cache;
	dev_dbg(codec->dev, "reg cache write : R%d=0x%x \n",reg,value);
	if (reg > LC1160_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the lc1160 register space
 */
static int lc1160_write(struct snd_soc_codec *codec,
			unsigned int reg, unsigned int value)
{
	struct comip_codec_platform_data *pdata = dev_get_platdata(codec->dev);
	int ret;
	int i;
	u8 *cache = codec->reg_cache;

	if (reg > LC1160_CACHEREGNUM)
		return -EIO;

	if(reg < LC1160_LDO){
		if(reg == LC1160_R06 && (value&0x20) && (!(cache[reg]&0x20))){
			ret = lc1160_reg_write(reg+0xAC,0x20);
		}else if(reg == LC1160_R07 && (value&0x20) && (!(cache[reg]&0x20))){
			ret = lc1160_reg_write(reg+0xAC,0x20);
			for(i=0; i<(value-0x20); i++){
				ret = lc1160_reg_write(LC1160_R06+0xAC,(0x20+i+1));
				ret = lc1160_reg_write(LC1160_R07+0xAC,(0x20+i+1));
				mdelay(1);
			}
		}else{
			ret = lc1160_reg_write(reg+0xAC,value);
		}
	}
	else if (reg == LC1160_LDO)
		ret = lc1160_reg_write(0x3B,value);
	else if ((reg == DBB_PCM_SWITCH) && pdata->pcm_switch) {
		ret = pdata->pcm_switch(value&0x01);
		dev_info(codec->dev,"pcm switch : value :%d \n",value);
	}else if ((reg == PA_ENABLE) && pdata->pa_enable) {
		if(!value)
			ret = pdata->pa_enable(value&0x01);
		else
			ret=0;
		dev_dbg(codec->dev,"pa enable : value :%d \n",value);
	} else
			ret = lc1160_reg_write(reg-0x53+0xa7,value);

	if(!ret)
		lc1160_write_reg_cache(codec, reg, value);
	else
		dev_err(codec->dev,"lc1160_write error!");
	return ret;
}

void codec_hs_jack_detect(struct snd_soc_codec *codec,
				struct snd_soc_jack *jack, int report)
{
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);

	lc1160->jack = jack;
	/*Jack interrupt enable*/
	if (lc1160->jack_polarity) {
		snd_soc_update_bits(codec, LC1160_JACKCR1, 0xf8, 0xc8);
		/* clear jack hook interrupt */
		snd_soc_write(codec,LC1160_JKHSIR,0xff);
	} else
		snd_soc_update_bits(codec, LC1160_JACKCR1, 0xf8, 0xc0);
}
EXPORT_SYMBOL_GPL(codec_hs_jack_detect);

static void lc1160_threshold_set(struct snd_soc_codec *codec)
{
	struct comip_codec_platform_data *pdata = dev_get_platdata(codec->dev);

	if(pdata != NULL) {
		snd_soc_update_bits(codec, LC1160_HS1CR, HS_VTH, pdata->vth1);
		snd_soc_update_bits(codec, LC1160_HS2CR, HS_VTH, pdata->vth2);
		snd_soc_update_bits(codec, LC1160_HS3CR, HS_VTH, pdata->vth3);
	} else
		dev_warn(codec->dev, "lc1160_threshold_set NULL !");
}
static void lc1160_micd_ctl(struct snd_soc_codec *codec,bool enable)
{
	if(enable) {
		lc1160_threshold_set(codec);
		snd_soc_dapm_force_enable_pin(&codec->dapm, "MICB2");
		snd_soc_dapm_sync(&codec->dapm);
		snd_soc_update_bits(codec, LC1160_HS1CR, HS_CTR_MASK|HS_ENABLE,HS_CTL_UNMASK|HS_ENABLE);
		snd_soc_update_bits(codec, LC1160_HS2CR, HS_CTR_MASK,HS_CTL_UNMASK);
		snd_soc_update_bits(codec, LC1160_HS3CR, HS_CTR_MASK,HS_CTL_UNMASK);
	} else {
		snd_soc_update_bits(codec, LC1160_HS1CR, HS_CTR_MASK|HS_ENABLE,~(HS_CTL_UNMASK|HS_ENABLE));
		snd_soc_update_bits(codec, LC1160_HS2CR, HS_CTR_MASK,~HS_CTL_UNMASK);
		snd_soc_update_bits(codec, LC1160_HS3CR, HS_CTR_MASK,~HS_CTL_UNMASK);
		snd_soc_dapm_disable_pin(&codec->dapm, "MICB2");
		snd_soc_dapm_sync(&codec->dapm);
	}
	msleep(10);
}
static void lc1160_micd(struct snd_soc_codec *codec)
{
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);
	int count = 10;
	u8 status;
	int report = 0;
	u8 status1;
	int count1 = 0;
	lc1160->jack_mic = false;

	if (lc1160->ear_switch)
		lc1160->ear_switch(1);

	do {
		status = snd_soc_read(codec, LC1160_HS1CR);
		msleep(5);
		status1 = snd_soc_read(codec, LC1160_HS1CR);
		if (status != status1) count = 10;
		count1++;
		if(count1 > 100) {
			dev_info(codec->dev,"Mic detect invalid,hs1ct: 0x%x, count: %d ", status, count);
			break;
		}
		msleep(2);
	} while (count--);

	if(count<0 && (status & MICD_VALID))
		lc1160->jack_mic = true;

	status = snd_soc_read(codec, LC1160_JACKCR1);
	if(!(status & JACK_INSERT)) {
		lc1160->jack_mic = false;
		lc1160_micd_ctl(codec,false);
		if (lc1160->ear_switch && lc1160->serial_port_state)
			lc1160->ear_switch(0);
		snd_soc_jack_report(lc1160->jack, 0, SND_JACK_HEADSET);
		dev_info(codec->dev,"Mic detected, but jack removed, jack: 0x%x\n",status);
		return;
	}

	if(lc1160->jack_mic)
		report |= SND_JACK_MICROPHONE;
	else {
		lc1160_micd_ctl(codec,false);
		report |= SND_JACK_HEADPHONE;
	}

	snd_soc_jack_report(lc1160->jack, report, SND_JACK_HEADSET);

	dev_info(codec->dev,"Jack in , report :%d", report);
}
static irqreturn_t lc1160_irq_handle(int irq, void* dev)
{
	struct snd_soc_codec *codec= (struct snd_soc_codec *)dev;
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);

	disable_irq_nosync(lc1160->irq);
	schedule_delayed_work(&lc1160->irq_work, CODEC_IRQ_DEBOUNCE_TIME);

	return IRQ_HANDLED;
}

static void lc1160_irq_work_handle(struct work_struct *work)
{
	struct lc1160_codec *lc1160 = container_of(work,
				struct lc1160_codec, irq_work.work);
	struct snd_soc_codec *codec= lc1160->codec;
	u8 status = 0;
	int report = 0;

	status = snd_soc_read(codec, LC1160_JKHSIR);
	/* clear jack hook interrupt */
	snd_soc_write(codec,LC1160_JKHSIR,0xff);
	dev_info(codec->dev,"lc1160_irq_work_handle: status = 0x%x\n", status);
	if(status&JACK_OUT_MASK)  {
		if (lc1160->ear_switch && lc1160->serial_port_state)
			lc1160->ear_switch(0);
		snd_soc_jack_report(lc1160->jack, 0, SND_JACK_HEADSET);
		lc1160->jack_mic = false;
		dev_info(codec->dev,"jack removed !!!");
	}
	if(status&JACK_IN_MASK) {
		msleep(500);
		lc1160_micd_ctl(codec,true);
		lc1160_micd(codec);
		goto out;
	}

	if(!lc1160->jack_mic) {
		lc1160_micd_ctl(codec,false);
		goto out;
	}

	switch(status&HS_DOWN_MASK) {
	case HS1_DOWN:
		report |= SND_JACK_BTN_0;
		break;
	case HS2_DOWN:
		report |= SND_JACK_BTN_1;
		break;
	case HS3_DOWN:
		report |= SND_JACK_BTN_2;
		break;
	default:
		break;
	}

	dev_info(codec->dev,"jack hook status :0x%x,report:0x%x\n",status,report);
	snd_soc_jack_report(lc1160->jack, report, SND_JACK_BTN_0|
												SND_JACK_BTN_1|SND_JACK_BTN_2);
out:
	snd_soc_write(codec,LC1160_JKHSIR,0xff);
	enable_irq(lc1160->irq);
}

static int lc1160_power(struct snd_soc_codec *codec, int on)
{
	int ret = 0;
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&lc1160->mutex);
	if (on) {
		snd_soc_update_bits(codec, LC1160_LDO, 0X80, 0X80);
		snd_soc_update_bits(codec, LC1160_R79, 0x01, 0x01);
	} else {
		snd_soc_update_bits(codec, LC1160_R79, 0x01, 0x00);
		snd_soc_update_bits(codec, LC1160_LDO, 0X80, 0X00);
	}
	mutex_unlock(&lc1160->mutex);
	return ret;
}

static void lc1160_shutdown(struct snd_pcm_substream *substream,
								struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "lc1160_shutdown function,dai:%d ,stream:%d,pActive:%d,cActive:%d \n",\
		dai->id,substream->stream,dai->playback_active,dai->capture_active);

	switch(dai->id) {
		case 0:
			if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				/* playback dac, sel no LRCK,sel no clk  */
				snd_soc_update_bits(codec, LC1160_R49, 0x0f, 0x0f);
			} else	{
				/* captuer adc1,  sel no LRCK,sel no clk */
				snd_soc_update_bits(codec, LC1160_R20, 0x0f, 0x0f);
			}
			if((!dai->playback_active)&&(!dai->capture_active)) {
				/* i2s sel no clk, and set i2s slave for power save*/
				snd_soc_update_bits(codec,LC1160_R09,0x30,0x30);
				//1 Need Chip Update
				/* i2s sel no LRCK*/
				//snd_soc_update_bits(codec,LC1160_R10,0xf0,0xf0);
				clk_disable(lc1160->mclk);
				lc1160->codec_power_save(SWITCH_TO_GPIO, 0);
			}
			break;
		case 1:
			/* voice adc1,	sel no LRCK,sel no clk	*/
			snd_soc_update_bits(codec, LC1160_R20, 0x0f, 0x0f);
			/* voice adc2,	sel no LRCK,sel no clk	*/
			snd_soc_update_bits(codec, LC1160_R29, 0x0f, 0x0f);
			/* mono dac, sel no LRCK,sel no clk  */
			snd_soc_update_bits(codec, LC1160_R22, 0x0f, 0x0f);
			/* PCM 256fs clock selection , and set pcm slave for power save */
			snd_soc_update_bits(codec, LC1160_R12,0x30, 0x30);

			clk_disable(lc1160->eclk);
			lc1160->codec_power_save(SWITCH_TO_GPIO, 1);
			break;
	}

}


static int lc1160_hw_params(struct snd_pcm_substream *substream,
								struct snd_pcm_hw_params *params,
								struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);
	unsigned int sample_rate;
	int err = 0;
	int multiple = 1;

	sample_rate = params_rate(params);
	dev_dbg(codec->dev, "lc1160_hw_params function,dai:%d stream :%d, sample rate:%d\n",dai->id,substream->stream,sample_rate);
	switch(dai->id){
		case 0:
			lc1160->codec_power_save(SWITCH_TO_CLK, 0);
			err = clk_set_rate(lc1160->mclk, LC1160_MCLK_RATE);
			if (err < 0) {
				dev_err(codec->dev, "lc1160 mclk set rate error %d\n",err);
				return err;
			}
			err = clk_enable(lc1160->mclk);
			if (err < 0) {
				dev_err(codec->dev, "lc1160 enable mclk error %d\n",err);
				return err;
			}
			if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				/* playback dac, over sampling rate 128x,sel i2s sync,sel mclk	*/
				snd_soc_write(codec, LC1160_R49, 0x32);
			} else	{
				/* for choose captuer adc, over sampling rate 128x,sel i2s sync,sel mclk	*/
				lc1160->choose_mic_capture=1;
				/* captuer adc1, over sampling rate 128x,sel i2s sync,sel mclk	*/
				snd_soc_write(codec, LC1160_R20, 0x22);
			}
			break;
		case 1:
			if(sample_rate == 16000)
				multiple = 2;
			lc1160->codec_power_save(SWITCH_TO_CLK, 1);
			err = clk_set_rate(lc1160->eclk, LC1160_ECLK_RATE *multiple);
			if (err < 0) {
				dev_err(codec->dev, "lc1160 eclk set rate error %d\n",err);
				return err;
			}
			err = clk_enable(lc1160->eclk);
			if (err < 0) {
				dev_err(codec->dev, "lc1160 enable eclk error %d\n",err);
				return err;
			}
			/* voice adc1, over sampling rate 128x,sel pcm sync,sel extclk	*/
			snd_soc_write(codec, LC1160_R20, 0x25);
			/* voice adc2, over sampling rate 128x,sel pcm sync,sel extclk	*/
			snd_soc_write(codec, LC1160_R29, 0x25);
			/* mono dac,over sampling rate 128x, sel pcm sync,sel extclk*/
			snd_soc_write(codec, LC1160_R22, 0x35);
			break;
	}
	return 0;
}

static int lc1160_set_dai_sysclk(struct snd_soc_dai *dai,
									 int clk_id, unsigned int freq, int dir)
{
	switch (dai->id) {
	case 0:
		break;
	case 1:
		break;
	default:
		BUG();
		return -EINVAL;
	}
	return 0;
}

static int lc1160_set_dai_fmt(struct snd_soc_dai *dai,
								  unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	dev_dbg(codec->dev, "lc1160_set_dai_fmt function,dai:%d \n",dai->id);

	switch (dai->id) {
	case 0:
		/*	i2s,clk sel mclk,32FS,master mode,
			  i2s word length set 16bit,letf justified	*/
		snd_soc_update_bits(codec,LC1160_R09,0x73,0x61);
		snd_soc_update_bits(codec,LC1160_R10,0x0f,0x01);
		break;
	case 1:
		/*	pcm,send data after one clk,sel extclk,
			  short frame sync, 16fs,master mode  */
		snd_soc_write(codec, LC1160_R12, 0x51);
		break;
	default:
		BUG();
		return -EINVAL;
	}
	return 0;
}

static int lc1160_set_bias_level(struct snd_soc_codec *codec,
									 enum snd_soc_bias_level level)
{
	dev_dbg(codec->dev, "lc1160_set_bias_level :level is %d\n",level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		lc1160_power(codec, 1);
		break;
	case SND_SOC_BIAS_OFF:
		lc1160_power(codec, 0);
		break;
	default:
		BUG();
	return -EINVAL;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static int notifier_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	if(!strcmp(w->name, "DACL")){
		switch (event) {
		case SND_SOC_DAPM_PRE_PMD:
			raw_notifier_call_chain(&audio_notifier_list,AUDIO_STOP,NULL);
			break;
		case SND_SOC_DAPM_POST_PMU:
			raw_notifier_call_chain(&audio_notifier_list,AUDIO_START,NULL);
			break;
		default:
			BUG();
			return -EINVAL;
		}
	}

	if(!strcmp(w->name, "MonoDAC")){
		dev_info(codec->dev, "Voice notifier_event :event is %d\n",event);
		switch (event) {
		case SND_SOC_DAPM_PRE_PMD:
			raw_notifier_call_chain(&call_notifier_list,VOICE_CALL_STOP,NULL);
			break;
		case SND_SOC_DAPM_POST_PMU:
			raw_notifier_call_chain(&call_notifier_list,VOICE_CALL_START,NULL);
			break;
		default:
			BUG();
			return -EINVAL;
		}
	}
	if( !strcmp(w->name, "LineL")||!strcmp(w->name, "LineR")){
		dev_info(codec->dev, "Line notifier_event :event is %d\n",event);
		switch (event) {
		case SND_SOC_DAPM_PRE_PMD:
			snd_soc_update_bits(codec, LC1160_R79, 0x04, 0x00);
				raw_notifier_call_chain(&fm_notifier_list,FM_STOP,NULL);
			break;
		case SND_SOC_DAPM_POST_PMU:
			snd_soc_update_bits(codec, LC1160_R79, 0x04, 0x04);
			raw_notifier_call_chain(&fm_notifier_list,FM_START,NULL);
			break;
		default:
			BUG();
			return -EINVAL;
		}
	}
		return 0;
}

static int classd_mixer_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec,LC1160_R77,0x3f,0x27);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(w->codec,LC1160_R77,0x3f,0x00);
		break;
	default:
		BUG();
		return -EINVAL;
	}
	return 0;
}
static int aux_pa_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);

	if(!lc1160->pa_enable)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		lc1160->pa_enable(0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		msleep(10);
		lc1160->pa_enable(1);
		break;
	default:
		BUG();
		return -EINVAL;
	}
		return 0;
}
static int cp_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(w->codec,LC1160_R76,0x07,0x07);
		msleep(5);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(w->codec,LC1160_R76,0x07,0x00);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}
	return 0;
}
static int adc_pga_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);
	if( !strcmp(w->name, "ADC1 Pga")){
		dev_info(codec->dev, "adc1_pga_event :event is %d\n",event);
		switch (event) {
		case SND_SOC_DAPM_PRE_PMD:
			if (lc1160->choose_mic_capture){
			snd_soc_update_bits(codec, LC1160_R20, 0x0f, 0x0f);
			lc1160->choose_mic_capture=0;
				}
			snd_soc_update_bits(codec, LC1160_R17, 0xc0, 0x00);
			snd_soc_update_bits(codec, LC1160_R14, 0x80, 0x80);
			snd_soc_update_bits(codec, LC1160_R23, 0x80, 0x80);
			break;
		case SND_SOC_DAPM_POST_PMU:
			if (lc1160->choose_mic_capture)
				snd_soc_update_bits(codec, LC1160_R20, 0xff, 0x32);
			snd_soc_update_bits(codec, LC1160_R17, 0xc0, 0xc0);
			snd_soc_update_bits(codec, LC1160_R20, 0x10, 0x10);
			msleep(50);
			snd_soc_update_bits(codec, LC1160_R14, 0x80, 0x00);
			snd_soc_update_bits(codec, LC1160_R23, 0x80, 0x00);
			break;
		default:
			BUG();
			return -EINVAL;
		}
	}
	if( !strcmp(w->name, "ADC2 Pga")){
		dev_info(codec->dev, "adc2_pga_event :event is %d\n",event);
		switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			if (lc1160->choose_mic_capture)
				snd_soc_update_bits(codec, LC1160_R29, 0xff, 0x32);
			
			snd_soc_update_bits(codec, LC1160_R26, 0xc0, 0xc0);
			snd_soc_update_bits(codec, LC1160_R29, 0x10, 0x10);
			msleep(50);
			snd_soc_update_bits(codec, LC1160_R23, 0x80, 0x00);

			break;
		case SND_SOC_DAPM_PRE_PMD:
			if (lc1160->choose_mic_capture){
				snd_soc_update_bits(codec, LC1160_R29, 0x0f, 0x0f);
				lc1160->choose_mic_capture=0;
				}
			snd_soc_update_bits(codec, LC1160_R26, 0xc0, 0x00);
			snd_soc_update_bits(codec, LC1160_R23, 0x80, 0x80);
			break;
		default:
			BUG();
			return -EINVAL;
		}
	}
	return 0;
}

static const char *lc1160_stereo_dac_input_texts[] = {
	"I2SL", "I2SR", "PCML", "PCMR","ZERO"
};
static const char *lc1160_mono_dac_input_texts[] = {
	"I2SL", "I2SR", "PCML", "PCMR","ADC1","ADC2","ZERO"
};

static const char *lc1160_interface_input_texts[]= {
	"ADC1", "ADC2","PDML","PDMR"
};

static const char *lc1160_mic1Hp_mux_texts[] = {
	"MIC1", "HPMIC"
};

static const char *lc1160_mic2Hp_mux_texts[] = {
	"MIC2", "HPMIC"
};

static const char *lc1160_sideTn_mux_texts[] = {
	"MIC1PGA", "MIC2PGA"
};

static DECLARE_TLV_DB_SCALE(mic_pag_tlv, -1000, 75, 0);
static DECLARE_TLV_DB_SCALE(spk_pag_tlv, -2600, 100, 0);
static DECLARE_TLV_DB_SCALE(hp_rec_pag_tlv, -2500, 100, 0);
static DECLARE_TLV_DB_SCALE(line_pag_tlv, -4650, 150, 0);
static DECLARE_TLV_DB_SCALE(digital_gain_tlv, -7650, 75, 1);
static DECLARE_TLV_DB_SCALE(sideTn_pga_tlv, -3000, 100, 0);

static const struct snd_kcontrol_new lc1160_spk_mixer_controls[] = {
	SOC_DAPM_SINGLE("Right Playback Switch", LC1160_R72, 0, 1, 1),
	SOC_DAPM_SINGLE("Left Playback Switch", LC1160_R72, 1, 1, 1),
	SOC_DAPM_SINGLE("Mono Voice Switch", LC1160_R72, 2, 1, 1),
	SOC_DAPM_SINGLE("Right Line Switch", LC1160_R72, 3, 1, 1),
	SOC_DAPM_SINGLE("Left Line Switch", LC1160_R72, 4, 1, 1),
};
static const struct snd_kcontrol_new lc1160_aux_mixer_controls[] = {
	SOC_DAPM_SINGLE("Right Playback Switch", LC1160_R71, 0, 1, 1),
	SOC_DAPM_SINGLE("Left Playback Switch", LC1160_R71, 1, 1, 1),
	SOC_DAPM_SINGLE("Mono Voice Switch", LC1160_R71, 2, 1, 1),
	SOC_DAPM_SINGLE("Right Line Switch", LC1160_R71, 3, 1, 1),
	SOC_DAPM_SINGLE("Left Line Switch", LC1160_R71, 4, 1, 1),
};
static const struct snd_kcontrol_new lc1160_rec_mixer_controls[] = {
	SOC_DAPM_SINGLE("Side Tone Switch", LC1160_R75, 0, 1, 1),
	SOC_DAPM_SINGLE("Right Playback Switch", LC1160_R75, 1, 1, 1),
	SOC_DAPM_SINGLE("Left Playback Switch", LC1160_R75, 2, 1, 1),
	SOC_DAPM_SINGLE("Mono Voice Switch", LC1160_R75, 3, 1, 1),
};
static const struct snd_kcontrol_new lc1160_hpl_mixer_controls[] = {
	SOC_DAPM_SINGLE("Side Tone Switch", LC1160_R73, 0, 1, 1),
	SOC_DAPM_SINGLE("Right Playback Switch", LC1160_R73, 1, 1, 1),
	SOC_DAPM_SINGLE("Left Playback Switch", LC1160_R73, 2, 1, 1),
	SOC_DAPM_SINGLE("Mono Voice Switch", LC1160_R73, 3, 1, 1),
	SOC_DAPM_SINGLE("Left Line Switch", LC1160_R73, 4, 1, 1),
};
static const struct snd_kcontrol_new lc1160_hpr_mixer_controls[] = {
	SOC_DAPM_SINGLE("Side Tone Switch", LC1160_R74, 0, 1, 1),
	SOC_DAPM_SINGLE("Right Playback Switch", LC1160_R74, 1, 1, 1),
	SOC_DAPM_SINGLE("Left Playback Switch", LC1160_R74, 2, 1, 1),
	SOC_DAPM_SINGLE("Mono Voice Switch", LC1160_R74, 3, 1, 1),
	SOC_DAPM_SINGLE("Right Line Switch", LC1160_R74, 4, 1, 1),
};
static const struct snd_kcontrol_new lc1160_adc1_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic1 Input Switch", LC1160_R68, 1, 1, 1),
	SOC_DAPM_SINGLE("Line input Switch", LC1160_R68, 2, 1, 1),
	SOC_DAPM_SINGLE("DAC left Switch", LC1160_R68, 4, 1, 1),
	SOC_DAPM_SINGLE("Mono Voice Switch", LC1160_R68, 5, 1, 1),
};
static const struct snd_kcontrol_new lc1160_adc2_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic2 Input Switch", LC1160_R69, 1, 1, 1),
	SOC_DAPM_SINGLE("Line input Switch", LC1160_R69, 2, 1, 1),
	SOC_DAPM_SINGLE("DAC left Switch", LC1160_R69, 4, 1, 1),
	SOC_DAPM_SINGLE("Mono Voice Switch", LC1160_R69, 5, 1, 1),
};

static const struct soc_enum lc1160_dac_right_input_switch_enum =
	SOC_ENUM_SINGLE(LC1160_R30,  2, 5, lc1160_stereo_dac_input_texts);

static const struct soc_enum lc1160_dac_left_input_switch_enum =
	SOC_ENUM_SINGLE(LC1160_R30,  5, 5, lc1160_stereo_dac_input_texts);

static const struct soc_enum lc1160_mono_dac_input_switch_enum =
	SOC_ENUM_SINGLE(LC1160_R13, 2, 7, lc1160_mono_dac_input_texts);

static const struct soc_enum pcm_left_enum =
	SOC_ENUM_SINGLE(LC1160_R11, 4, 4, lc1160_interface_input_texts);
static const struct soc_enum pcm_right_enum =
	SOC_ENUM_SINGLE(LC1160_R11, 2, 4, lc1160_interface_input_texts);

static const struct snd_kcontrol_new pcm_out_left_mux =
	SOC_DAPM_ENUM("PCM Out Left Sel", pcm_left_enum);
static const struct snd_kcontrol_new pcm_out_right_mux =
	SOC_DAPM_ENUM("PCM Out Right Sel", pcm_right_enum);

static const struct soc_enum i2s_left_enum =
	SOC_ENUM_SINGLE(LC1160_R10, 4, 4, lc1160_interface_input_texts);
static const struct soc_enum i2s_right_enum =
	SOC_ENUM_SINGLE(LC1160_R10, 6, 4, lc1160_interface_input_texts);

static const struct snd_kcontrol_new i2s_out_left_mux =
	SOC_DAPM_ENUM("I2S Out Left Sel", i2s_left_enum);
static const struct snd_kcontrol_new i2s_out_right_mux =
	SOC_DAPM_ENUM("I2S Out Right Sel", i2s_right_enum);

static const struct soc_enum mic1Pga_mux_enum =
	SOC_ENUM_SINGLE(LC1160_R71, 6, 2, lc1160_mic1Hp_mux_texts);
static const struct soc_enum mic2Pga_mux_enum =
	SOC_ENUM_SINGLE(LC1160_R70, 2, 2, lc1160_mic2Hp_mux_texts);
static const struct soc_enum sideTn_mux_enum =
	SOC_ENUM_SINGLE(LC1160_R70, 7, 2, lc1160_sideTn_mux_texts);
static const struct snd_kcontrol_new mic1Pga_mux_kctl =
	SOC_DAPM_ENUM("mic1Pga mux kctl", mic1Pga_mux_enum);
static const struct snd_kcontrol_new mic2Pga_mux_kctl =
	SOC_DAPM_ENUM("mic2Pga mux kctl", mic2Pga_mux_enum);
static const struct snd_kcontrol_new sideTn_mux_kctl =
	SOC_DAPM_ENUM("sideTn mux kctl", sideTn_mux_enum);

static const struct snd_soc_dapm_widget lc1160_dapm_widgets[] = {

SND_SOC_DAPM_MIC("MIC1", NULL),
SND_SOC_DAPM_MIC("MIC2", NULL),

SND_SOC_DAPM_LINE("LineL In", NULL),
SND_SOC_DAPM_LINE("LineR In", NULL),

SND_SOC_DAPM_MIC("MIC1P", NULL),
SND_SOC_DAPM_MIC("MIC1N", NULL),
SND_SOC_DAPM_MIC("MIC2P", NULL),
SND_SOC_DAPM_MIC("MIC2N", NULL),
SND_SOC_DAPM_MIC("HPMICP", NULL),
SND_SOC_DAPM_MIC("HPMICN", NULL),
SND_SOC_DAPM_MUX("mic2Pga mux", SND_SOC_NOPM, 0, 0, &mic2Pga_mux_kctl),
SND_SOC_DAPM_MUX("sideTn mux", SND_SOC_NOPM, 0, 0, &sideTn_mux_kctl),
SND_SOC_DAPM_MUX("mic1Pga mux", SND_SOC_NOPM, 0, 0, &mic1Pga_mux_kctl),

//SND_SOC_DAPM_DAC("DACL", "Playback", LC1160_R01, 6, 0),
SND_SOC_DAPM_DAC_E("DACL", "Playback", LC1160_R01, 6, 0,
				notifier_event, SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_DAC("DACR", "Playback", LC1160_R01, 7, 0),
SND_SOC_DAPM_DAC_E("MonoDAC", "VxDL", LC1160_R01, 5, 0,
				notifier_event, SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_ADC_E("ADC1 Pga", "Capture & VxUL", LC1160_R01, 3, 0,
                           adc_pga_event, SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_ADC_E("ADC2 Pga", "Capture & VxUL", LC1160_R01, 4, 0,
                           adc_pga_event, SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_SUPPLY("LDOA15", LC1160_LDO, 7, 0, NULL,0),

SND_SOC_DAPM_SUPPLY_S("Charge Pump", 2, LC1160_R76, 2, 0, cp_event,
							SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_MICBIAS("MICB1", LC1160_R77, 7, 0),
SND_SOC_DAPM_MICBIAS("MICB2", LC1160_R77, 6, 0),

SND_SOC_DAPM_PGA_E("LineL", LC1160_R01, 1, 0, NULL, 0,\
						notifier_event, SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_PGA_E("LineR", LC1160_R01, 2, 0, NULL, 0,\
						notifier_event, SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_PGA("MIC1 Pga", LC1160_R0, 0, 0, NULL, 0),
SND_SOC_DAPM_PGA("MIC2 Pga", LC1160_R0, 4, 0, NULL, 0),
SND_SOC_DAPM_PGA("SIDETn Pga", LC1160_R04, 6, 0, NULL, 0),

SND_SOC_DAPM_MUX("PcmL Out", SND_SOC_NOPM,0, 0, &pcm_out_left_mux),
SND_SOC_DAPM_MUX("PcmR Out", SND_SOC_NOPM,0, 0, &pcm_out_right_mux),

SND_SOC_DAPM_MUX("I2sL Out", SND_SOC_NOPM,0, 0, &i2s_out_left_mux),
SND_SOC_DAPM_MUX("I2sR Out", SND_SOC_NOPM,0, 0, &i2s_out_right_mux),

/*Path Domain Widgets*/
SND_SOC_DAPM_MIXER_E("Spk Mixer",LC1160_R72,5,0,\
			&lc1160_spk_mixer_controls[0],ARRAY_SIZE(lc1160_spk_mixer_controls),\
			classd_mixer_event,SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_MIXER("Aux Mixer", LC1160_R71, 5, 0,\
			&lc1160_aux_mixer_controls[0],ARRAY_SIZE(lc1160_aux_mixer_controls)),

SND_SOC_DAPM_MIXER("Rec Mixer", LC1160_R75, 4, 0,\
	&lc1160_rec_mixer_controls[0], ARRAY_SIZE(lc1160_rec_mixer_controls)),

SND_SOC_DAPM_MIXER("Hpl Mixer", LC1160_R73, 5, 0,\
	&lc1160_hpl_mixer_controls[0], ARRAY_SIZE(lc1160_hpl_mixer_controls)),
SND_SOC_DAPM_MIXER("Hpr Mixer", LC1160_R74, 5, 0,\
	&lc1160_hpr_mixer_controls[0], ARRAY_SIZE(lc1160_hpr_mixer_controls)),

SND_SOC_DAPM_MIXER("Adc1 Mixer", SND_SOC_NOPM, 0, 0,\
	&lc1160_adc1_mixer_controls[0], ARRAY_SIZE(lc1160_adc1_mixer_controls)),
SND_SOC_DAPM_MIXER("Adc2 Mixer", SND_SOC_NOPM, 0, 0,\
	&lc1160_adc2_mixer_controls[0], ARRAY_SIZE(lc1160_adc2_mixer_controls)),

SND_SOC_DAPM_PGA("HeadphoneL PGA", LC1160_R06, 5, 0,  NULL, 0),
SND_SOC_DAPM_PGA("HeadphoneR PGA", LC1160_R07, 5, 0,  NULL, 0),

SND_SOC_DAPM_PGA_E("Speaker PGA", SND_SOC_NOPM, 0, 0, NULL, 0,\
			aux_pa_event,SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_PGA_E("Aux PGA", SND_SOC_NOPM, 0, 0, NULL, 0,\
			aux_pa_event,SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_PRE_PMD),

/* Outputs */
SND_SOC_DAPM_SPK("SPKOUT", NULL),
SND_SOC_DAPM_SPK("AUXOUT", NULL),
SND_SOC_DAPM_LINE("RECOUT", NULL),
SND_SOC_DAPM_HP("HPLOUT", NULL),
SND_SOC_DAPM_HP("HPROUT", NULL),
};

static const struct snd_soc_dapm_widget lc1160_diff_dapm_widgets[] = {
	SND_SOC_DAPM_PGA("Receiver PGA", LC1160_R05, 5, 1,	NULL, 0),
};
static const struct snd_soc_dapm_widget lc1160b_diff_dapm_widgets[] = {
	SND_SOC_DAPM_PGA("Receiver PGA", LC1160_R05, 5, 0,	NULL, 0),
};

static const struct snd_kcontrol_new lc1160_snd_controls[] = {
SOC_ENUM("DACR Input", lc1160_dac_right_input_switch_enum),
SOC_ENUM("DACL Input", lc1160_dac_left_input_switch_enum),

SOC_DOUBLE("Stereo DAC Mute",LC1160_R30,0,1,1,0),

SOC_ENUM("Mono DAC input",lc1160_mono_dac_input_switch_enum),
SOC_SINGLE("Mono DAC Mute", LC1160_R13, 1, 1, 0),

SOC_SINGLE_TLV("Speaker Volume", LC1160_R08, 0, 31, 0, spk_pag_tlv),
SOC_DOUBLE_R_TLV("Headphone Volume", LC1160_R06, LC1160_R07, 0, 31, 0, hp_rec_pag_tlv),
SOC_SINGLE_TLV("Receiver Volume", LC1160_R05, 0, 31, 0, hp_rec_pag_tlv),
SOC_SINGLE_TLV("sideTn Volume", LC1160_R04, 0, 30, 0, sideTn_pga_tlv),

SOC_DOUBLE_R_TLV("Line Volume", LC1160_R02, LC1160_R03, 0, 39, 0, line_pag_tlv),

SOC_SINGLE_TLV("Mono DAC Gain", LC1160_R21, 0, 127, 0, digital_gain_tlv),
SOC_DOUBLE_R_TLV("Stereo DAC Gain", LC1160_R33, LC1160_R34, 0, 127, 0, digital_gain_tlv),

SOC_SINGLE_TLV("ADC1 Gain", LC1160_R14, 0,127 , 0, digital_gain_tlv),
SOC_SINGLE_TLV("ADC2 Gain", LC1160_R23, 0,127 , 0, digital_gain_tlv),

SOC_SINGLE_TLV("MIC1 Volume", LC1160_R15, 0,40 , 0, mic_pag_tlv),
SOC_SINGLE_TLV("MIC2 Volume", LC1160_R24, 0,40 , 0, mic_pag_tlv),

SOC_SINGLE("MIC1-HS Switch", LC1160_R71, 6, 1, 0),
SOC_SINGLE("MIC2-HS Switch", LC1160_R70, 2, 1, 0),

SOC_SINGLE("I2S Loop Back", LC1160_R09, 7, 1, 0),
SOC_SINGLE("MIC1 Boost", LC1160_R15, 6, 1, 0),
SOC_SINGLE("MIC2 Boost", LC1160_R24, 6, 1, 0),

SOC_SINGLE("Pcm To Codec", DBB_PCM_SWITCH, 0, 1, 0),
SOC_SINGLE("Dual-Mic Switch", LC1160_R11, 6, 1, 0),
SOC_SINGLE("Pcm32Bclk Switch", LC1160_R12, 1, 1, 0),
SOC_SINGLE("Pa Enable", PA_ENABLE, 0, 1, 0),
};
static const struct snd_soc_dapm_route intercon[] = {

	 /* Left headphone output mixer */
	{"Hpl Mixer",  "Left Playback Switch",	"DACL"},
	{"Hpl Mixer",  "Mono Voice Switch",   "MonoDAC"},
	{"Hpl Mixer",  "Left Line Switch",	"LineL"},
	{"Hpl Mixer",  "Side Tone Switch",  "SIDETn Pga"},

	/* Right headphone output mixer */
	{"Hpr Mixer", "Right Playback Switch",	"DACR"},
	{"Hpr Mixer", "Mono Voice Switch",	"MonoDAC"},
	{"Hpr Mixer", "Right Line Switch",	"LineR"},
	{"Hpr Mixer",  "Side Tone Switch",  "SIDETn Pga"},

	/* Left speaker output mixer */
	{"Spk Mixer", "Left Playback Switch",  "DACL"},
	{"Spk Mixer", "Left Line Switch",  "LineL"},
	{"Spk Mixer", "Mono Voice Switch",	 "MonoDAC"},

	/* Right speaker output mixer */
	{"Spk Mixer", "Right Playback Switch",	"DACR"},
	{"Spk Mixer", "Right Line Switch",	"LineR"},

	/* Left aux output mixer */
	{"Aux Mixer", "Left Playback Switch",  "DACL"},
	{"Aux Mixer", "Left Line Switch",  "LineL"},
	{"Aux Mixer", "Mono Voice Switch",	"MonoDAC"},

	/* Right aux output mixer */
	{"Aux Mixer", "Right Playback Switch",	"DACR"},
	{"Aux Mixer", "Right Line Switch",	"LineR"},

	/* Earpiece/Receiver output mixer */
	{"Rec Mixer", "Left Playback Switch",	 "DACL"},
	{"Rec Mixer", "Right Playback Switch",	"DACR"},
	{"Rec Mixer", "Mono Voice Switch",	 "MonoDAC"},
	{"Rec Mixer", "Side Tone Switch",   "SIDETn Pga"},

	/*ADC1 UpLink */
	{"Adc1 Mixer", "Line input Switch",  "LineL"},
	{"Adc1 Mixer", "Line input Switch",  "LineR"},
	{"Adc1 Mixer", "Mic1 Input Switch",  "MIC1 Pga"},
	{"ADC1 Pga", NULL,	"Adc1 Mixer"},


	{"PcmL Out", "ADC1",  "ADC1 Pga"},
	{"I2sL Out",  "ADC1",  "ADC1 Pga"},

	/*ADC2 UpLink */
	{"Adc2 Mixer","Mic2 Input Switch",	"MIC2 Pga"},
	{"ADC2 Pga",NULL,  "Adc2 Mixer"},
	{"PcmR Out", "ADC2",  "ADC2 Pga"},
	{"I2sR Out",  "ADC2",  "ADC2 Pga"},

	/* output  path*/
	{"HeadphoneL PGA", NULL,  "Hpl Mixer"},
	{"HeadphoneR PGA", NULL,  "Hpr Mixer"},
	{"HeadphoneL PGA", NULL,  "Charge Pump"},
	{"HeadphoneR PGA", NULL,  "Charge Pump"},
	{"Speaker PGA", NULL,  "Spk Mixer"},
	{"Aux PGA", NULL,  "Aux Mixer"},
	{"Receiver PGA", NULL,	"Rec Mixer"},

	{"LineL", NULL,  "LineL In"},
	{"LineR", NULL,  "LineR In"},

	{"MIC1 Pga", NULL,	"MICB1"},
	{"MIC2 Pga", NULL,	"MICB1"},

	{"MICB1", NULL,  "MIC1"},
	{"MICB1", NULL,  "MIC2"},

	{"HPLOUT", NULL,  "HeadphoneL PGA"},
	{"HPROUT", NULL,  "HeadphoneR PGA"},
	{"SPKOUT", NULL,  "Speaker PGA"},
	{"AUXOUT", NULL,  "Aux PGA"},
	{"RECOUT", NULL,  "Receiver PGA"},

	{"mic2Pga mux", "MIC2", "MIC2P"},
	{"mic2Pga mux", "MIC2", "MIC2N"},
	{"mic2Pga mux", "HPMIC", "HPMICP"},
	{"mic2Pga mux", "HPMIC", "HPMICN"},
	{"MIC2 Pga", NULL, "mic2Pga mux"},
	{"mic1Pga mux", "MIC1", "MIC1P"},
	{"mic1Pga mux", "MIC1", "MIC1N"},
	{"mic1Pga mux", "HPMIC", "HPMICP"},
	{"mic1Pga mux", "HPMIC", "HPMICN"},
	{"MIC1 Pga", NULL, "mic1Pga mux"},
	{"sideTn mux", "MIC1PGA", "MIC1 Pga"},
	{"sideTn mux", "MIC2PGA", "MIC2 Pga"},
	{"SIDETn Pga", NULL, "sideTn mux"},
};

static ssize_t lc1160_reg_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	u8 reg, val;

	sscanf(buf, "0x%02x  0x%02x", (int *)&reg, (int *)&val);
	lc1160_reg_write(reg,val);
	lc1160_reg_read(reg, &val);

	dev_info(dev, "lc1160_reg_store:read reg0x%02x = 0x%02x\n", reg,val);
	return count;
}

static ssize_t lc1160_reg_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	u8 val;
	int i;
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	lc1160_reg_read(0x3b, &val);
	str += scnprintf(str, end - str, "Reg 0x3b : 0x%02x \n", val);

	for (i = 172;i < 253; i++) {
		lc1160_reg_read(i, &val);
		str += scnprintf(str, end - str, "Reg 0x%02x : 0x%02x \n", i, val);
	}
	lc1160_reg_read(0xa7, &val);
	str += scnprintf(str, end - str, "Reg 0xa7 : 0x%02x \n", val);
	lc1160_reg_read(0xa8, &val);
	str += scnprintf(str, end - str, "Reg 0xa8 : 0x%02x \n", val);
	lc1160_reg_read(0xa9, &val);
	str += scnprintf(str, end - str, "Reg 0xa9 : 0x%02x \n", val);
	lc1160_reg_read(0xaa, &val);
	str += scnprintf(str, end - str, "Reg 0xaa : 0x%02x \n", val);
	lc1160_reg_read(0xab, &val);
	str += scnprintf(str, end - str, "Reg 0xab : 0x%02x \n", val);
	lc1160_reg_read(0x8c, &val);
	str += scnprintf(str, end - str, "Reg 0x8c : 0x%02x \n", val);
	if (str > buf)
		str--;

	str += scnprintf(str, end - str, "\n");
	return (str - buf);
}

static DEVICE_ATTR(lc1160_reg, S_IRUGO|S_IWUSR,
			lc1160_reg_show,lc1160_reg_store);

static struct attribute *lc1160_core_reg_attributes[] = {
	&dev_attr_lc1160_reg.attr,
	NULL,
};

static const struct attribute_group lc1160_core_attributes = {
	.attrs = lc1160_core_reg_attributes,
};

static ssize_t serial_port_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int value;
	struct lc1160_codec *lc1160 = dev_get_drvdata(dev);

	sscanf(buf, "%d", &value);
	if (lc1160->ear_switch){
		if(value){
			lc1160->ear_switch(0);
			lc1160->serial_port_state=1;
		}else{
			lc1160->ear_switch(1);
			lc1160->serial_port_state=0;
		}
	}
	return count;
}

static ssize_t serial_port_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct lc1160_codec *lc1160 = dev_get_drvdata(dev);

	return sprintf(buf, "serial port is %s\n", (lc1160->serial_port_state)? "open":"close");
}

static DEVICE_ATTR(serial_port, S_IRUGO|S_IWUSR,
			serial_port_show,serial_port_store);

static struct attribute *serial_port_state_attributes[] = {
	&dev_attr_serial_port.attr,
	NULL,
};

static const struct attribute_group serial_port_attributes = {
	.attrs = serial_port_state_attributes,
};

static int lc1160_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct comip_codec_platform_data *pdata = dev_get_platdata(codec->dev);
	struct lc1160_codec *lc1160;
	int ret = 0;
	u8 chip_version;

	lc1160 = kzalloc(sizeof(struct lc1160_codec), GFP_KERNEL);
	if (lc1160 == NULL)
		return -ENOMEM;

	lc1160->codec = codec;
	snd_soc_codec_set_drvdata(codec,lc1160);

	chip_version =lc1160_chip_version_get();

	if(chip_version == LC1160_ECO1) { /* lc1160b */
		snd_soc_dapm_new_controls(dapm, lc1160b_diff_dapm_widgets, ARRAY_SIZE(lc1160b_diff_dapm_widgets));
	} else {
		snd_soc_dapm_new_controls(dapm, lc1160_diff_dapm_widgets, ARRAY_SIZE(lc1160_diff_dapm_widgets));
	}

	dapm->idle_bias_off = true;
	codec->cache_only = false;
	dapm->bias_level = SND_SOC_BIAS_OFF;

	mutex_init(&lc1160->mutex);
	lc1160_set_bias_level(codec,SND_SOC_BIAS_OFF);

	if((pdata->mclk_id == NULL) || (pdata->eclk_id == NULL)) {
		dev_err(codec->dev, "pdata clk id null !!!\n");
		ret = -EINVAL;
		goto mclk_err;
	}

	lc1160->mclk = clk_get(NULL, pdata->mclk_id);
	if (IS_ERR(lc1160->mclk)) {
		dev_err(codec->dev, "Cannot get master clock\n");
		ret = -EINVAL;
		goto mclk_err;
	}
	lc1160->eclk = clk_get(NULL, pdata->eclk_id);
	if (IS_ERR(lc1160->eclk)) {
		dev_err(codec->dev, "Cannot get ext clock\n");
		ret = -EINVAL;
		goto extclk_err;
	}
	lc1160->codec_power_save = pdata->codec_power_save;
	lc1160->ear_switch = pdata->ear_switch;
	lc1160->pa_enable = pdata->pa_enable;
	lc1160->jack_polarity = pdata->jack_polarity;
	lc1160->irq = gpio_to_irq(pdata->irq_gpio);
	lc1160->serial_port_state=0;

	lc1160->codec_power_save(SWITCH_TO_GPIO, MCLK_ID);
	lc1160->codec_power_save(SWITCH_TO_GPIO, EXTCLK_ID);
	ret = gpio_request(pdata->irq_gpio, " LC1160 Codec Irq");
	if (ret < 0) {
		dev_err(codec->dev, "LC1160: Failed to request GPIO.\n");
		return ret;
	}
	ret = gpio_direction_input(pdata->irq_gpio);
	if (ret) {
		dev_err(codec->dev, "%s: Failed to detect GPIO input.\n", __func__);
		goto extclk_err;
	}

	/* Request irq. */
	ret = request_irq(lc1160->irq, lc1160_irq_handle,
			  IRQF_TRIGGER_HIGH, "lc1160 Codec Irq", codec);
	if (ret) {
		dev_err(codec->dev, "%s: IRQ already in use.\n", __func__);
		goto extclk_err;
	}

	disable_irq(lc1160->irq);
	INIT_DELAYED_WORK(&lc1160->irq_work, lc1160_irq_work_handle);
	irq_set_irq_wake(lc1160->irq, 1);
	enable_irq(lc1160->irq);

	/* change default value  */
	snd_soc_update_bits(codec,LC1160_R68,0x36,0x36);
	snd_soc_update_bits(codec,LC1160_R69,0x2e,0x2e);
	snd_soc_update_bits(codec,LC1160_R71,0x04,0x04);
	snd_soc_update_bits(codec,LC1160_R72,0x04,0x04);
	snd_soc_update_bits(codec,LC1160_R73,0x06,0x06);
	snd_soc_update_bits(codec,LC1160_R74,0x06,0x06);
	snd_soc_update_bits(codec,LC1160_R75,0x08,0x08);
	snd_soc_update_bits(codec, LC1160_R31, 0x80, 0x00);

	if (lc1160->ear_switch){
		if(lc1160->serial_port_state)
			lc1160->ear_switch(0);
		else
			lc1160->ear_switch(1);
	}

	if (lc1160->pa_enable)
		lc1160->pa_enable(0);

	dev_info(codec->dev, "codec probe success, chip version is %s", (chip_version)? "LC1160B":"LC1160");

	return 0;

extclk_err:
		clk_put(lc1160->mclk);
mclk_err:
		kfree(lc1160);
	return ret;
}
static int lc1160_remove(struct snd_soc_codec *codec)
{
	struct lc1160_codec *lc1160 = snd_soc_codec_get_drvdata(codec);

	lc1160_power(codec, 0);
	free_irq(lc1160->irq, codec);
	kfree(lc1160);
	return 0;
}

static struct snd_soc_dai_ops lc1160_dai_ops = {
	.shutdown		= lc1160_shutdown,
	.hw_params		= lc1160_hw_params,
	.set_sysclk 	= lc1160_set_dai_sysclk,
	.set_fmt		= lc1160_set_dai_fmt,
};
static struct snd_soc_dai_driver lc1160_dai[] = {
{
	.name = "comip_hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1160_RATES,
		.formats = COMIP_1160_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1160_RATES,
		.formats = COMIP_1160_FORMATS,
	},
	.ops = &lc1160_dai_ops,
},
{
	.name = "comip_voice",
	.playback = {
		.stream_name = "VxDL",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1160_RATES,
		.formats = COMIP_1160_FORMATS,
	},
	.capture = {
		.stream_name = "VxUL",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1160_RATES,
		.formats = COMIP_1160_FORMATS,
	},
	.ops = &lc1160_dai_ops,
},
{
	.name = "virtual_codec",
	.playback = {
		.stream_name = "Play",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1160_RATES,
		.formats = COMIP_1160_FORMATS,
	},
	.capture = {
		.stream_name = "Cap",
		.channels_min = 1,
		.channels_max = 2,
		.rates = COMIP_1160_RATES,
		.formats = COMIP_1160_FORMATS,
	},
}
};
static struct snd_soc_codec_driver soc_codec_dev_lc1160 = {
	.probe =	lc1160_probe,
	.remove =	lc1160_remove,
	.read = lc1160_read_reg_cache,
	.write = lc1160_write,
	.set_bias_level = lc1160_set_bias_level,
	.reg_cache_size = sizeof(lc1160_codec_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = lc1160_codec_reg,
	.ignore_pmdown_time = true,
	.controls = lc1160_snd_controls,
	.num_controls = ARRAY_SIZE(lc1160_snd_controls),
	.dapm_widgets = lc1160_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(lc1160_dapm_widgets),
	.dapm_routes = intercon,
	.num_dapm_routes = ARRAY_SIZE(intercon),
};
static int __init lc1160_codec_probe(struct platform_device *pdev)
{
	int ret;
	ret = sysfs_create_group(&pdev->dev.kobj, &lc1160_core_attributes);
	if(ret)
		dev_err(&pdev->dev, "lc1160_codec_probe,create group error");

	ret = sysfs_create_group(&pdev->dev.kobj, &serial_port_attributes);
	if(ret)
		dev_err(&pdev->dev, "lc1160_codec_probe,create group error");

	return snd_soc_register_codec(&pdev->dev,&soc_codec_dev_lc1160,
					lc1160_dai, ARRAY_SIZE(lc1160_dai));
}

static int __exit lc1160_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver lc1160_codec_driver = {
	.driver = {
		.name = "comip_codec",
		.owner = THIS_MODULE,
	},
	.remove = __exit_p(lc1160_codec_remove),
};

static int __init lc1160_codec_init(void)
{
	return platform_driver_probe(&lc1160_codec_driver, lc1160_codec_probe);
}

static void __exit lc1160_codec_exit(void)
{
	platform_driver_unregister(&lc1160_codec_driver);
}

rootfs_initcall(lc1160_codec_init);
module_exit(lc1160_codec_exit);

MODULE_DESCRIPTION("ASoC lc1160 codec driver");
MODULE_AUTHOR("yangshunlong <yangshunlong@leadcoretech.com>");
MODULE_LICENSE("GPL");
