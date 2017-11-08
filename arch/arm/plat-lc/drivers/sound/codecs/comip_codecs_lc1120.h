/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2009  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the code driver headfile.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	 1.0.0		2012-02-04	tangyong	created
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
		  
#include <asm/io.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <plat/hardware.h>
#include <plat/audio.h>
#include <plat/switch.h>
#include <plat/comip_codecs_interface.h>

struct CODEC_I2C_DATA_T {
	u8 nreg;	/*codec register number*/
	u8 nvalue; 	/*codec register value*/
	u8 nmask;
	u8 ndelay_times;
};

#define MAX_GAIN_LEVEL 4

enum _COMIP_SWITCH {
	SWITCH_SUSPEND_NOEXIST,
	SWITCH_SUSPEND_EXIST,
};

extern int comip_1120_i2c_write(u8 value, u8 reg);
extern int comip_1120_i2c_read(u8 reg);

#define LC1120_RESERVE_R0    			0x00
#define LC1120_R01           			0x01
#define LC1120_R02    			        0x02
#define LC1120_R03          			0x03
#define LC1120_R04     	    			0x04
#define LC1120_R05           			0x05
#define LC1120_R06           			0x06
#define LC1120_R07           			0x07
#define LC1120_R08           			0x08
#define LC1120_R09           			0x09
#define LC1120_R10          			0x0a
#define LC1120_R11          			0x0b
#define LC1120_R12              		0x0c
#define LC1120_R13   			        0x0d
#define LC1120_R14          			0x0e
#define LC1120_R15          			0x0f
#define LC1120_R16          			0x10
#define LC1120_R17  		    		0x11
#define LC1120_R18          			0x12
#define LC1120_R19            			0x13
#define LC1120_R20  				    0x14
#define LC1120_R21 				        0x15
#define LC1120_R22 				        0x16
#define LC1120_R23    				    0x17
#define LC1120_R24    				    0x18
#define LC1120_R25		    			0x19
#define LC1120_R26    				    0x1a
#define LC1120_R27    				    0x1b
#define LC1120_R28    				    0x1c
#define LC1120_R29    				    0x1d
#define LC1120_R30    				    0x1e
#define LC1120_R31	    				0x1f
#define LC1120_R32				        0x20
#define LC1120_R33  				    0x21
#define LC1120_R34  				    0x22
#define LC1120_R35 				        0x23
#define LC1120_R36				        0x24
#define LC1120_R37 				        0x25
#define LC1120_R38    					0x26
#define LC1120_R39                  	0x27
#define LC1120_R40                  	0x28
#define LC1120_R41                  	0x29
#define LC1120_R42                  	0x2a
#define LC1120_R43                  	0x2b
#define LC1120_R44                  	0x2c
#define LC1120_R45                  	0x2d
#define LC1120_R46                  	0x2e
#define LC1120_R47                  	0x2f
#define LC1120_R48                  	0x30
#define LC1120_R49                  	0x31
#define LC1120_R50                  	0x32
#define LC1120_R51                  	0x33
#define LC1120_R52                  	0x34
#define LC1120_R53                  	0x35
#define LC1120_R54                  	0x36
#define LC1120_R55                  	0x37
#define LC1120_R56                  	0x38
#define LC1120_R57                  	0x39
#define LC1120_R58                  	0x3a
#define LC1120_R59                  	0x3b
#define LC1120_R60                  	0x3c
#define LC1120_R61                  	0x3d
#define LC1120_R62                  	0x3e
#define LC1120_R63                  	0x3f
#define LC1120_R64                  	0x40
#define LC1120_R65                  	0x41
#define LC1120_R66                  	0x42
#define LC1120_R67                  	0x43
#define LC1120_R68                  	0x44
#define LC1120_R69                  	0x45
#define LC1120_R70                  	0x46
#define LC1120_R71                  	0x47
#define LC1120_R72                  	0x48
#define LC1120_R73                  	0x49
#define LC1120_R74                  	0x4a
#define LC1120_R75                  	0x4b
#define LC1120_R76                  	0x4c
#define LC1120_R77                  	0x4d
#define LC1120_R78                  	0x4e
#define LC1120_R79                  	0x4f
#define LC1120_R80                  	0x50
#define LC1120_R81                  	0x51
#define LC1120_R82                  	0x52
#define LC1120_R83                  	0x53
#define LC1120_R84                  	0x54
#define LC1120_R85                  	0x55
#define LC1120_R86                  	0x56
#define LC1120_R87                  	0x57
#define LC1120_R88                  	0x58
#define LC1120_R89                  	0x59
#define LC1120_R90                  	0x5a
#define LC1120_R91                  	0x5b
#define LC1120_R92                  	0x5c
#define LC1120_R93                  	0x5d
#define LC1120_R94                  	0x5e
#define LC1120_R95                  	0x5f
#define LC1120_R96                  	0x60
#define LC1120_R97                 	 	0x61
#define LC1120_R98                  	0x62
#define LC1120_R99                  	0x63
#define LC1120_R100                  	0x64
#define LC1120_R101                  	0x65
#define comip_1120_REGS_NUM 			102

#define COMIP_1120_RATES SNDRV_PCM_RATE_8000_48000
#define COMIP_1120_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S20_3LE)

#define LC1120_FRIST_OUTPUT_GAIN_RANGE     128
#define LC1120_FRIST_OUTPUT_GAIN_MIN	      0
#define LC1120_FRIST_OUTPUT_GAIN_MAX       128

#define LC1120_SECOND_OUTPUT_GAIN_RANGE     32
#define LC1120_SECOND_OUTPUT_GAIN_MIN	      0
#define LC1120_SECOND_OUTPUT_GAIN_MAX       128

#define LC1120_FM_OUTPUT_GAIN_RANGE     40
#define LC1120_FM_OUTPUT_GAIN_MIN	      0
#define LC1120_FM_OUTPUT_GAIN_MAX       128

#define LC1120_FRIST_INPUT_GAIN_RANGE     40
#define LC1120_FRIST_INPUT_GAIN_MIN	      0
#define LC1120_FRIST_INPUT_GAIN_MAX       120

#define LC1120_SECOND_INPUT_GAIN_RANGE     128
#define LC1120_SECOND_INPUT_GAIN_MIN	      0
#define LC1120_SECOND_INPUT_GAIN_MAX       128

#define RECEIVER 0x01
#define SPEAKER  0x02
#define EARPHONE 0x03
#if 0
#define HP_DETECT 1
#define HOOKSWITCH_DETECT 2
#define JACK_IN 1
#define JACK_OUT 0
#define HOOKSWITCH_PRESS 1
#define HOOKSWITCH_RELEASE 0
#define JACK_IRS 7
#define HOOKSWITCH_IRS 8

#define JACK_IN_WITH_MIC   1
#define JACK_IN_WITHOUT_MIC  2

#endif


#define GPIO_LEVEL_LOW 0
#define GPIO_LEVEL_HIGH 1
//#define HEADPHONE_DETECT_LEVEL 2

/* output gain range 0--127 */
#define DD_MIXER_AUDIO_OUTPUT_MIN 0
#define DD_MIXER_AUDIO_OUTPUT_MAX 127
/* input gain range 0--127 */
#define DD_MIXER_AUDIO_INPUT_MIN 0
#define DD_MIXER_AUDIO_INPUT_MAX 127

#define DD_MIXER_VOICE_SIDETONE_MIN 0
#define DD_MIXER_VOICE_SIDETONE_MAX 63




struct comip_1120_priv {
	struct codec_voice_tbl *pvoice_table;
	struct snd_soc_codec *pcodec;
	int sysclk;
	int dai_fmt;
	unsigned int cur_volume;
	unsigned char i2s_ogain_speaker_daclevel;
	unsigned char i2s_ogain_speaker_cdvol;
	unsigned char i2s_ogain_headset_daclevel;
	unsigned char i2s_ogain_headset_hpvol;
	unsigned char i2s_ogain_receiver_daclevel;
	unsigned char i2s_ogain_receiver_epvol;
	unsigned char i2s_ogain_spk_hp_daclevel;
	unsigned char i2s_ogain_spk_hp_hpvol;
	unsigned char i2s_ogain_spk_hp_cdvol;
	unsigned char i2s_igain_mic_adclevel;
	unsigned char i2s_igain_mic_pgagain;
	unsigned char i2s_igain_headsetmic_adclevel;
	unsigned char i2s_igain_headsetmic_pgagain;
	unsigned char i2s_igain_line_linvol;

	u8 alc_reg51;
	u8 alc_reg52;
	u8 alc_reg53;
	u8 alc_reg54;
	u8 alc_reg55;
	u8 alc_reg56;
	u8 alc_reg57;
	u8 alc_reg58;
	u8 alc_reg59;
	u8 alc_reg60;
	u8 alc_reg61;
	u8 alc_reg62;
	u8 alc_reg63;
	u8 alc_reg64;
};


