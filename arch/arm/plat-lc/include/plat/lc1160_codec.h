/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2013  LeadCoreTech Corp.
 *
 *  PURPOSE: This files contains the code driver headfile.
 *
 *  CHANGE HISTORY:
 *
 *  Version     Date        Author      Description
 *   1.0.0      2013-12-20  yangshunlong created
 *
 */
#ifndef __LC1160_CODEC_H__
#define __LC1160_CODEC_H__
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/jack.h>
#include <plat/hardware.h>
#include <plat/mfp.h>
#include <plat/comip-snd-notifier.h>
#include <plat/lc1160.h>

struct comip_codec_platform_data {
	int (*pcm_switch)(int enable);
	int (*codec_power_save)(int enable, int clk_id);
	int (*ear_switch)(int enable);
	int (*pa_enable)(int enable);
	int irq_gpio;
	int jack_polarity;
	const char *mclk_id;
	const char *eclk_id;
	u8 vth1;
	u8 vth2;
	u8 vth3;
};

#define LC1160_CACHEREGNUM  90

#define LC1160_R0           0x0
#define LC1160_R01          0x01
#define LC1160_R02          0x02
#define LC1160_R03          0x03
#define LC1160_R04          0x04
#define LC1160_R05          0x05
#define LC1160_R06          0x06
#define LC1160_R07          0x07
#define LC1160_R08          0x08
#define LC1160_R09          0x09
#define LC1160_R10          0x0a
#define LC1160_R11          0x0b
#define LC1160_R12          0x0c
#define LC1160_R13          0x0d
#define LC1160_R14          0x0e
#define LC1160_R15          0x0f
#define LC1160_R16          0x10
#define LC1160_R17          0x11
#define LC1160_R18          0x12
#define LC1160_R19          0x13
#define LC1160_R20          0x14
#define LC1160_R21          0x15
#define LC1160_R22          0x16
#define LC1160_R23          0x17
#define LC1160_R24          0x18
#define LC1160_R25          0x19
#define LC1160_R26          0x1a
#define LC1160_R27          0x1b
#define LC1160_R28          0x1c
#define LC1160_R29          0x1d
#define LC1160_R30          0x1e
#define LC1160_R31          0x1f
#define LC1160_R32          0x20
#define LC1160_R33          0x21
#define LC1160_R34          0x22
#define LC1160_R35          0x23
#define LC1160_R36          0x24
#define LC1160_R37          0x25
#define LC1160_R38          0x26
#define LC1160_R39          0x27
#define LC1160_R40          0x28
#define LC1160_R41          0x29
#define LC1160_R42          0x2a
#define LC1160_R43          0x2b
#define LC1160_R44          0x2c
#define LC1160_R45          0x2d
#define LC1160_R46          0x2e
#define LC1160_R47          0x2f
#define LC1160_R48          0x30
#define LC1160_R49          0x31
#define LC1160_R50          0x32
#define LC1160_R51          0x33
#define LC1160_R52          0x34
#define LC1160_R53          0x35
#define LC1160_R54          0x36
#define LC1160_R55          0x37
#define LC1160_R56          0x38
#define LC1160_R57          0x39
#define LC1160_R58          0x3a
#define LC1160_R59          0x3b
#define LC1160_R60          0x3c
#define LC1160_R61          0x3d
#define LC1160_R62          0x3e
#define LC1160_R63          0x3f
#define LC1160_R64          0x40
#define LC1160_R65          0x41
#define LC1160_R66          0x42
#define LC1160_R67          0x43
#define LC1160_R68          0x44
#define LC1160_R69          0x45
#define LC1160_R70          0x46
#define LC1160_R71          0x47
#define LC1160_R72          0x48
#define LC1160_R73          0x49
#define LC1160_R74          0x4a
#define LC1160_R75          0x4b
#define LC1160_R76          0x4c
#define LC1160_R77          0x4d
#define LC1160_R78          0x4e
#define LC1160_R79          0x4f
#define LC1160_R80          0x50
#define LC1160_R81          0x51
#define LC1160_LDO          0x52  //addr is 0x3b

/* Jack & Hookswitch,addr:0xa7,0xa8,0xa9,0xaa,0xab*/
#define LC1160_JACKCR1  0x53
#define LC1160_HS1CR      0x54
#define LC1160_HS2CR      0x55
#define LC1160_HS3CR      0x56
#define LC1160_JKHSIR     0x57

#define DBB_PCM_SWITCH    0x58

#define PA_ENABLE	0x59

#define HS_CTR_MASK          0xf0
#define HS_CTL_UNMASK      0xc0
#define HS_ENABLE               0x08
#define HS_VTH                      0x03
#define MICD_VALID         0x04
#define JACK_INSERT           0X04
#define JACK_IN_MASK       0x01
#define JACK_OUT_MASK   0x02

#define HS_DOWN_MASK  0x54
#define HS1_DOWN  0x54
#define HS2_DOWN  0x50
#define HS3_DOWN   0x40

#define COMIP_1160_RATES    SNDRV_PCM_RATE_8000_48000
#define COMIP_1160_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE |\
        SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S20_3LE)

void codec_hs_jack_detect(struct snd_soc_codec *codec,
                struct snd_soc_jack *jack, int report);
#endif
