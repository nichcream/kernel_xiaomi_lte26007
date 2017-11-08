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
#define CODEC_SOC_PROC

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#ifdef CODEC_SOC_PROC
#include <linux/proc_fs.h>
#endif
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/asound.h>
#include <asm/io.h>
#include <plat/hardware.h>
#include <plat/audio.h>
#include <plat/comip-snd-notifier.h>

#define MIXER_OUTPUT_PATH         0x01
#define MIXER_OUTPUT_VOLUME       0x02
#define MIXER_INPUT_VOLUME        0x03
#define MIXER_OUTPUT_SAMPLERATE   0x04
#define MIXER_INPUT_SAMPLERATE    0x05
#define MIXER_INPUT_MUTE          0x06
#define MIXER_OUTPUT_MUTE         0x07
#define MIXER_INPUT_PATH          0x08
#define MIXER_ENABLE_VOICE        0x09
#define MIXER_ENABLE_FM           0x0a
#define MIXER_MP_POWER_SAVE       0x0b
#define MIXER_DOUBLE_MICS         0x0c
#define MIXER_ENABLE_ALC          0x0d



/*switch nType*/
enum {
	JACK_DETECT = 0x01,
	HOOK_DETECT = 0X02
};

/*headphone key */
enum {
	HOOK_KEY = 0,
	UP_KEY,
	DOWN_KEY,
	INVALID_KEY
};
/*jack status*/
enum {
	JACK_OUT = 0,
	JACK_IN_WITH_MIC,
	JACK_IN_WITHOUT_MIC
};
/*hook status*/
enum {
	HOOK_RELEASE = 0,
	HOOK_PRESS
};

enum {
 HOOKSWITCH_RELEASE=0,
 HOOKSWITCH_PRESS,
};

enum {
    JACK_IRS,
	HOOKSWITCH_IRS,
};


typedef int (*codec_func) (int,int);
#define MAX_GAIN_LEVEL 4


struct snd_leadcore_mixer_info {
       char codecName[16];
       int multiService;
};

struct codec_core_func_t {
	u8 szFuncName;
	codec_func pfunc;
};
struct comip_codec_ops {
	int (*set_output_path)(int nType,int nValue);
	int (*set_output_volume)(int nType,int nValue);
	int (*set_input_gain)(int nType,int nValue);
	int (*set_output_samplerate)(int nValue);
	int (*set_input_samplerate)(int nValue);
	int (*set_input_mute)(int nType,int nValue);
	int (*set_output_mute)(int nType,int nValue);
	int (*set_input_path)(int nType,int nValue);
	int (*enable_voice)(int nValue);
	int (*enable_fm)(int nValue);
	int (*enable_bluetooth)(int nValue);
	int (*mp_powersave)(int nType,int nValue);
	int (*fm_out_gain)(int nValue);
	int (*set_double_mics_mode)(int nValue);
	int (*enable_alc)(int nValue);
	ssize_t (*calibrate_data_show) (const char *buf);
	ssize_t (*calibrate_data_store)(const char *buf, size_t size);
	ssize_t (*comip_register_show) (const char *buf);
	ssize_t (*comip_register_store)(const char *buf, size_t size);
#ifdef CODEC_SOC_PROC
	ssize_t (*codec_proc_read)(char *page,int count,loff_t *eof);
	ssize_t (*codec_proc_write)(const char __user *buffer,unsigned long count);
	ssize_t (*codec_audio_proc_read)(char *page,int count,loff_t *eof);
	ssize_t (*codec_audio_proc_write)(const char __user *buffer,unsigned long count);
	ssize_t (*codec_eq_proc_read)(char*page,int count,loff_t*eof);
	ssize_t (*codec_eq_proc_write)(const char __user*buffer,unsigned long count);
#endif
	ssize_t (*codec_get_mixer_info)(struct snd_leadcore_mixer_info* _userInfo);
	/*switch */
	ssize_t (*codec_switch_mask)(u8 nType);
	ssize_t (*codec_switch_unmask)(u8 nType);
	ssize_t (*codec_hook_enable)(int nValue);
	ssize_t (*codec_irq_type_get)(void);
	ssize_t (*codec_switch_status_get)(u8 nType);
	ssize_t (*codec_clr_irq)(void);
	ssize_t (*codec_get_headphone_key_num)(void);
};


extern int leadcore_register_codec_info(struct snd_soc_codec_driver *soc_codec_dev,
                                        struct snd_soc_dai_driver  *soc_dai_dev, struct comip_codec_ops * comip_codec_ops_dev);
 
int codec_set_output_path(int nType,int nValue);
int codec_set_output_volume(int nType,int nValue);
int codec_set_input_gain(int nType,int nValue);
int codec_set_output_samplerate(int nType,int nValue);
int codec_set_input_samplerate(int nType,int nValue);
int codec_set_input_mute(int nType,int nValue);
int codec_set_output_mute(int nType,int nValue);
int codec_set_input_path(int nType,int nValue);
int codec_enable_voice(int nType,int nValue);
int codec_enable_fm(int nType,int nValue);
int codec_enable_bluetooth(int nType,int nValue);
int codec_mp_powersave(int nType,int nValue);
int codec_fm_out_gain(int nType,int nValue);
int codec_set_double_mics_mode(int nType,int nValue);
int codec_enable_alc(int nType,int nValue);

int codec_interface_switch_mask(u8 nType);
int codec_interface_switch_unmask(u8 nType);

int codec_interface_hook_enable(int nValue);
int codec_interface_isr_type_get(void);
int codec_interface_switch_status_get(u8 nType);
int codec_interface_clr_irq(void);
int codec_interface_get_headphone_key_num(void);

#define	CODEC_PT_AUDIO_ULINK_SIZE 332
#define	CODEC_PT_AUDIO_DLINK_SIZE 136
#define CODEC_CAL_TBL_SIZE		512
#define	CODEC_PT_GAIN_ITEMNUM 	16
#define	CODEC_PT_GAIN_MAXVAL		128

#define VOICE_TYPE   		0x01
#define AUDIO_TYPE   		0x02
#define AUDIO_CAPTURE_TYPE  0x03
#define FM_TYPE				0x04
#define BLUETOOTH_TYPE		0x08

#define CODEC_PCM_SEL   0x1
#define BT_PCM_SEL   	0x0

#define DMIC_BOARD_DOUBLE_MICS_MODE 1
#define DMIC_BOARD_SINGLE_MIC_MODE 2

#define SNDRV_CTL_IOCTL_LEADCORE_READ  _IOWR('U', 0xda, struct snd_leadcore_mixer_info)
#define SNDRV_CTL_IOCTL_LEADCORE_READ  _IOWR('U', 0xda, struct snd_leadcore_mixer_info)

#define SNDRV_CTL_IOCTL_LEADCORE_WRITE _IOWR('U', 0xdb, struct snd_leadcore_mixer)


struct codec_voice_tbl {
	u32 flag;
	u16 length;
	u16 ratflag;
	u16 channelflag;
	u16 sceneflag;
	u16 sidetone_reserve_1;
	u16 sidetone_reserve_2;
	u8  audio_uplink[CODEC_PT_AUDIO_ULINK_SIZE];
	u8  aduio_downlink[CODEC_PT_AUDIO_DLINK_SIZE];
	u8  gain_in_level0;			/* For analog gain */
	u8  gain_in_level1;			/* For digital gain */
	u8  gain_in_level2;
	u8  gain_in_level3;
	u8  gain_out_level0_class0;
	u8  gain_out_level0_class1;
	u8  gain_out_level0_class2;
	u8  gain_out_level0_class3;
	u8  gain_out_level0_class4;
	u8  gain_out_level0_class5;
	u8  gain_out_level1;
	u8  gain_out_level2;
	u8  gain_out_level3;
	u8  gain_out_level4;
	u8  gain_out_level5;
	u8  gain_out_level6;
	u32 reserved;
	u32 protected;
} ;

struct comip_codec_interface_platform_data {
	int (*pcm_switch)(int enable);
	int jack_detect_level;
	int aux_pa_gpio;
	int main_mic_flag; // if flag == 1, codec lc1132 use mic1 as main mic,then mic2 as main mic
};

struct snd_leadcore_mixer {
       int nCmd;
       int nType;
       int nValue;
};

/* codec path out enum */
typedef enum _DD_CODEC_PATHOUT_ {
       DD_OUTMUTE = 0,
       DD_RECEIVER,
       DD_SPEAKER,
       DD_HEADPHONE,
       DD_BLUETOOTH,
    DD_SPEAKER_HEADPHONE
} DD_CODEC_PATHOUT;



/* codec path in enum */
typedef enum _DD_CODEC_PATHIN_ {
       DD_MIC = 1,
       DD_HEADSET,
} DD_CODEC_PATHIN;



extern struct comip_codec_interface_platform_data *pintdata;

