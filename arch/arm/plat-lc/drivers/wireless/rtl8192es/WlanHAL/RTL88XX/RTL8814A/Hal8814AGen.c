/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8814AGen.c
	
Abstract:
	Defined RTL8814A HAL Function
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-05-28 Filen            Create.	
--*/

#ifndef __ECOS
#include "HalPrecomp.h"
#else
#include "../../HalPrecomp.h"
#endif

// TestChip
#include "data_AGC_TAB_8814A.c"
#include "data_MAC_REG_8814A.c"
#include "data_PHY_REG_8814A.c"
#include "data_PHY_REG_MP_8814A.c"
#include "data_PHY_REG_PG_8814A.c"
#include "data_RadioA_8814A.c"
#include "data_RadioB_8814A.c"
#include "data_RadioC_8814A.c"
#include "data_RadioD_8814A.c"
#include "data_rtl8814Afw.c"

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
#include "data_AGC_TAB_8814A_hp.c"
#include "data_PHY_REG_8814A_hp.c"
#include "data_RadioA_8814A_hp.c"
#include "data_RadioB_8814A_hp.c"
#include "data_RadioC_8814A_hp.c"
#include "data_RadioD_8814A_hp.c"
#endif

// Power Tracking
#include "data_TxPowerTrack_AP_8814A.c"


//3 MACDM
//default
#include "data_MACDM_def_high_8814A.c"
#include "data_MACDM_def_low_8814A.c"
#include "data_MACDM_def_normal_8814A.c"
//general
#include "data_MACDM_gen_high_8814A.c"
#include "data_MACDM_gen_low_8814A.c"
#include "data_MACDM_gen_normal_8814A.c"
//txop
#include "data_MACDM_txop_high_8814A.c"
#include "data_MACDM_txop_low_8814A.c"
#include "data_MACDM_txop_normal_8814A.c"
//criteria
#include "data_MACDM_state_criteria_8814A.c"


#define VAR_MAPPING(dst,src) \
	u1Byte *data_##dst##_start = &data_##src[0]; \
	u1Byte *data_##dst##_end   = &data_##src[sizeof(data_##src)];

VAR_MAPPING(AGC_TAB_8814A, AGC_TAB_8814A);
VAR_MAPPING(MAC_REG_8814A, MAC_REG_8814A);
VAR_MAPPING(PHY_REG_8814A, PHY_REG_8814A);
//VAR_MAPPING(PHY_REG_1T_8814A, PHY_REG_1T_8814A);
VAR_MAPPING(PHY_REG_PG_8814A, PHY_REG_PG_8814A);
VAR_MAPPING(PHY_REG_MP_8814A, PHY_REG_MP_8814A);
VAR_MAPPING(RadioA_8814A, RadioA_8814A);
VAR_MAPPING(RadioB_8814A, RadioB_8814A);
VAR_MAPPING(RadioC_8814A, RadioC_8814A);
VAR_MAPPING(RadioD_8814A, RadioD_8814A);
VAR_MAPPING(rtl8814Afw, rtl8814Afw);

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
VAR_MAPPING(AGC_TAB_8814A_hp, AGC_TAB_8814A_hp);
VAR_MAPPING(PHY_REG_8814A_hp, PHY_REG_8814A_hp);
VAR_MAPPING(RadioA_8814A_hp, RadioA_8814A_hp);
VAR_MAPPING(RadioB_8814A_hp, RadioB_8814A_hp);
VAR_MAPPING(RadioC_8814A_hp, RadioC_8814A_hp);
VAR_MAPPING(RadioD_8814A_hp, RadioD_8814A_hp);
#endif

// Power Tracking
VAR_MAPPING(TxPowerTrack_AP_8814A, TxPowerTrack_AP_8814A);


//3 MACDM
VAR_MAPPING(MACDM_def_high_8814A, MACDM_def_high_8814A);
VAR_MAPPING(MACDM_def_low_8814A, MACDM_def_low_8814A);
VAR_MAPPING(MACDM_def_normal_8814A, MACDM_def_normal_8814A);

VAR_MAPPING(MACDM_gen_high_8814A, MACDM_gen_high_8814A);
VAR_MAPPING(MACDM_gen_low_8814A, MACDM_gen_low_8814A);
VAR_MAPPING(MACDM_gen_normal_8814A, MACDM_gen_normal_8814A);

VAR_MAPPING(MACDM_txop_high_8814A, MACDM_txop_high_8814A);
VAR_MAPPING(MACDM_txop_low_8814A, MACDM_txop_low_8814A);
VAR_MAPPING(MACDM_txop_normal_8814A, MACDM_txop_normal_8814A);

VAR_MAPPING(MACDM_state_criteria_8814A, MACDM_state_criteria_8814A);


//MP Chip
#include "data_AGC_TAB_8814Amp.c"
#include "data_MAC_REG_8814Amp.c"
#include "data_PHY_REG_8814Amp.c"
#include "data_PHY_REG_MP_8814Amp.c"
#include "data_PHY_REG_PG_8814Amp.c"
#include "data_RadioA_8814Amp.c"
#include "data_RadioB_8814Amp.c"
#include "data_RadioC_8814Amp.c"
#include "data_RadioD_8814Amp.c"
#include "data_rtl8814AfwMP.c"

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
#include "data_AGC_TAB_8814Amp_hp.c"
#include "data_PHY_REG_8814Amp_hp.c"
#include "data_RadioA_8814Amp_hp.c"
#include "data_RadioB_8814Amp_hp.c"
#include "data_RadioC_8814Amp_hp.c"
#include "data_RadioD_8814Amp_hp.c"
#endif

// Power Tracking
#include "data_TxPowerTrack_AP_8814Amp.c"


VAR_MAPPING(AGC_TAB_8814Amp, AGC_TAB_8814Amp);
VAR_MAPPING(MAC_REG_8814Amp, MAC_REG_8814Amp);
VAR_MAPPING(PHY_REG_8814Amp, PHY_REG_8814Amp);
VAR_MAPPING(PHY_REG_PG_8814Amp, PHY_REG_PG_8814Amp);
VAR_MAPPING(PHY_REG_MP_8814Amp, PHY_REG_MP_8814Amp);
VAR_MAPPING(RadioA_8814Amp, RadioA_8814Amp);
VAR_MAPPING(RadioB_8814Amp, RadioB_8814Amp);
VAR_MAPPING(RadioC_8814Amp, RadioC_8814Amp);
VAR_MAPPING(RadioD_8814Amp, RadioD_8814Amp);
VAR_MAPPING(rtl8814AfwMP, rtl8814AfwMP);

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
VAR_MAPPING(AGC_TAB_8814Amp_hp, AGC_TAB_8814Amp_hp);
VAR_MAPPING(PHY_REG_8814Amp_hp, PHY_REG_8814Amp_hp);
VAR_MAPPING(RadioA_8814Amp_hp, RadioA_8814Amp_hp);
VAR_MAPPING(RadioB_8814Amp_hp, RadioB_8814Amp_hp);
VAR_MAPPING(RadioC_8814Amp_hp, RadioC_8814Amp_hp);
VAR_MAPPING(RadioD_8814Amp_hp, RadioD_8814Amp_hp);
#endif

// Power Tracking
VAR_MAPPING(TxPowerTrack_AP_8814Amp, TxPowerTrack_AP_8814Amp);


