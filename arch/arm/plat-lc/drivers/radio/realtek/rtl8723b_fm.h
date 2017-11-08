#ifndef __FM_RTL8723B_H
#define __FM_RTL8723B_H


#include <linux/kernel.h>


#include "fm_base.h"


//#define REALTEK_FM_I2C_DEBUG 0

#ifdef REALTEK_FM_I2C_DEBUG
#define dbg_i2c_info(args...) printk(args)
#else
#define dbg_i2c_info(args...)
#endif





// Definitions

// Initializing
#define FM_INIT_REG_TABLE_LEN						65
#define RFC_INIT_TABLE_LEN							18


#define RTL8723B_FM_SNR_AVE_BIT_NUM				16
#define RTL8723B_FM_CFO_BIT_NUM				18


void
BuildRtl8723bFmModule(
	FM_MODULE **ppFm,
	FM_MODULE *pFmModuleMemory,
	BASE_INTERFACE_MODULE *pBaseInterfaceModuleMemory,
	unsigned char DeviceAddr
	);



int
rtl8723b_fm_Initialize(
	FM_MODULE *pFm
	);



int
rtl8723b_fm_SetTune_Hz(
	FM_MODULE *pFm,
	int RfFreqHz
	);



int
rtl8723b_fm_Set_Channel_Space(
	FM_MODULE *pFm,
	int ChannelSpace
	);



int
rtl8723b_fm_Seek(
	FM_MODULE *pFm,
	int bSeekEnable
	);



int
rtl8723b_fm_SetSeekDirection(
	FM_MODULE *pFm,
	int bSeekUp
	);



int
rtl8723b_fm_SetBassBoost(
	FM_MODULE *pFm,
	int bBassEnable
	);



int
rtl8723b_fm_SetMute(
	FM_MODULE *pFm,
	int bForceMute
	);



int
rtl8723b_fm_SetUserDefineBand(
	FM_MODULE *pFm,
	int StartHz,
	int EndHz
	);



int
rtl8723b_fm_SetBand(
	FM_MODULE *pFm,
	unsigned long FmBandType
	);



int
rtl8723b_fm_SetRDS(
	FM_MODULE *pFm,
	int bRDSEnable
	);



int
rtl8723b_fm_GetFrequencyHz(
	FM_MODULE *pFm,
	int *pFrequencyHz
	);



int
rtl8723b_fm_GetSNR(
	FM_MODULE *pFm,
	long *pSnrDbNum,
	long *pSnrDbDen
	);



int
rtl8723b_fm_GetCFO(
	FM_MODULE *pFm,
	long *pCfo
	);



int
rtl8723b_fm_GetRSSI(
	FM_MODULE *pFm,
	int *pRssi
	);



int
rtl8723b_fm_GetFSM(
	FM_MODULE *pFm,
	unsigned long *pFsmState
	);



void
rtl8723b_fm_InitRegTable(
	FM_MODULE *pFm
	);



int rtl8723b_fm_Get_Chipid(
        FM_MODULE *pFm,
        unsigned long *id
        );












#endif
