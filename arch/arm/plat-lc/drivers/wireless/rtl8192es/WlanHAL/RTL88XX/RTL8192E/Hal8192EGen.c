/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8192EGen.c
	
Abstract:
	Defined RTL8192E HAL Function
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-04-16 Filen            Create.	
--*/


#include "../../HalPrecomp.h"


#include "../../../data_AGC_TAB_8192E.c"
#include "../../../data_MAC_REG_8192E.c"
#include "../../../data_PHY_REG_8192E.c"
//#include "../../../data_PHY_REG_1T_8192E.c"
#include "../../../data_PHY_REG_MP_8192E.c"
#include "../../../data_PHY_REG_PG_8192E.c"
#ifdef TXPWR_LMT_92EE
#include "../../../data_PHY_REG_PG_8192E_new.c"
#endif
#include "../../../data_RadioA_8192E.c"
#include "../../../data_RadioB_8192E.c"
#include "../../../data_rtl8192Efw.c"
//#include "data_RTL8192EFW_Test_T.c"

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
#ifdef PWR_BY_RATE_92E_HP			
#include "../../../data_PHY_REG_PG_8192Emp_hp.c"
#endif
#if CFG_HAL_HIGH_POWER_EXT_LNA
#include "../../../data_AGC_TAB_8192E_hp.c"
#include "../../../data_PHY_REG_8192E_hp.c"
#include "../../../data_RadioA_8192E_hp.c"
#include "../../../data_RadioB_8192E_hp.c"
#endif
#include "../../../data_AGC_TAB_8192E_extpa.c"
#include "../../../data_PHY_REG_8192E_extpa.c"
#include "../../../data_RadioA_8192E_extpa.c"
#include "../../../data_RadioB_8192E_extpa.c"
#endif
#if CFG_HAL_HIGH_POWER_EXT_LNA
#include "../../../data_AGC_TAB_8192E_extlna.c"
#include "../../../data_PHY_REG_8192E_extlna.c"
#include "../../../data_RadioA_8192E_extlna.c"
#include "../../../data_RadioB_8192E_extlna.c"
#endif
// B-cut
#include "data_MAC_REG_8192Eb.c"
#include "data_PHY_REG_8192Eb.c"

#ifdef CONFIG_SDIO_HCI
#include "data_RadioA_8192Es.c"
#include "data_RadioB_8192Es.c"

#include "data_AGC_TAB_8192Es.c"
#else
#include "data_RadioA_8192Eb.c"
#include "data_RadioB_8192Eb.c"
//

// MP
#include "data_RadioA_8192Emp.c"
#include "data_RadioB_8192Emp.c"
#include "data_RadioA_8192EmpA.c"
#include "data_RadioB_8192EmpA.c"
#include "data_AGC_TAB_8192Emp.c"
#endif

#include "data_MAC_REG_8192Emp.c"
#include "data_PHY_REG_8192Emp.c"
#include "data_PHY_REG_MP_8192Emp.c"
#include "data_PHY_REG_PG_8192Emp.c"
#include "data_rtl8192EfwMP.c"

// Power Tracking
#include "../../../data_TxPowerTrack_AP.c"


//3 MACDM
//default
#include "../../../data_MACDM_def_high_8192E.c"
#include "../../../data_MACDM_def_low_8192E.c"
#include "../../../data_MACDM_def_normal_8192E.c"
//general
#include "../../../data_MACDM_gen_high_8192E.c"
#include "../../../data_MACDM_gen_low_8192E.c"
#include "../../../data_MACDM_gen_normal_8192E.c"
//txop
#include "../../../data_MACDM_txop_high_8192E.c"
#include "../../../data_MACDM_txop_low_8192E.c"
#include "../../../data_MACDM_txop_normal_8192E.c"
//criteria
#include "../../../data_MACDM_state_criteria_8192E.c"




#define VAR_MAPPING(dst,src) \
	u1Byte *data_##dst##_start = &data_##src[0]; \
	u1Byte *data_##dst##_end   = &data_##src[sizeof(data_##src)];

VAR_MAPPING(AGC_TAB_8192E, AGC_TAB_8192E);
VAR_MAPPING(MAC_REG_8192E, MAC_REG_8192E);
VAR_MAPPING(PHY_REG_8192E, PHY_REG_8192E);
//VAR_MAPPING(PHY_REG_1T_8192E, PHY_REG_1T_8192E);
VAR_MAPPING(PHY_REG_PG_8192E, PHY_REG_PG_8192E);
VAR_MAPPING(PHY_REG_MP_8192E, PHY_REG_MP_8192E);
VAR_MAPPING(RadioA_8192E, RadioA_8192E);
VAR_MAPPING(RadioB_8192E, RadioB_8192E);
VAR_MAPPING(rtl8192Efw, rtl8192Efw);

#ifdef TXPWR_LMT_92EE
VAR_MAPPING(PHY_REG_PG_8192E_new, PHY_REG_PG_8192E_new);
#endif
// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
#ifdef PWR_BY_RATE_92E_HP			
VAR_MAPPING(PHY_REG_PG_8192Emp_hp, PHY_REG_PG_8192Emp_hp);
#endif
#if CFG_HAL_HIGH_POWER_EXT_LNA
VAR_MAPPING(AGC_TAB_8192E_hp, AGC_TAB_8192E_hp);
VAR_MAPPING(PHY_REG_8192E_hp, PHY_REG_8192E_hp);
VAR_MAPPING(RadioA_8192E_hp, RadioA_8192E_hp);
VAR_MAPPING(RadioB_8192E_hp, RadioB_8192E_hp);
#endif
VAR_MAPPING(AGC_TAB_8192E_extpa, AGC_TAB_8192E_extpa);
VAR_MAPPING(PHY_REG_8192E_extpa, PHY_REG_8192E_extpa);
VAR_MAPPING(RadioA_8192E_extpa, RadioA_8192E_extpa);
VAR_MAPPING(RadioB_8192E_extpa, RadioB_8192E_extpa);
#endif

#if CFG_HAL_HIGH_POWER_EXT_LNA
VAR_MAPPING(AGC_TAB_8192E_extlna, AGC_TAB_8192E_extlna);
VAR_MAPPING(PHY_REG_8192E_extlna, PHY_REG_8192E_extlna);
VAR_MAPPING(RadioA_8192E_extlna, RadioA_8192E_extlna);
VAR_MAPPING(RadioB_8192E_extlna, RadioB_8192E_extlna);
#endif

// B-cut
VAR_MAPPING(MAC_REG_8192Eb, MAC_REG_8192Eb);
VAR_MAPPING(PHY_REG_8192Eb, PHY_REG_8192Eb);

#ifdef CONFIG_SDIO_HCI
VAR_MAPPING(RadioA_8192Es, RadioA_8192Es);
VAR_MAPPING(RadioB_8192Es, RadioB_8192Es);

VAR_MAPPING(AGC_TAB_8192Es, AGC_TAB_8192Es);
#else
VAR_MAPPING(RadioA_8192Eb, RadioA_8192Eb);
VAR_MAPPING(RadioB_8192Eb, RadioB_8192Eb);

// MP
VAR_MAPPING(RadioA_8192Emp, RadioA_8192Emp);
VAR_MAPPING(RadioB_8192Emp, RadioB_8192Emp);
VAR_MAPPING(RadioA_8192EmpA, RadioA_8192EmpA);
VAR_MAPPING(RadioB_8192EmpA, RadioB_8192EmpA);

VAR_MAPPING(AGC_TAB_8192Emp, AGC_TAB_8192Emp);
#endif

VAR_MAPPING(MAC_REG_8192Emp, MAC_REG_8192Emp);
VAR_MAPPING(PHY_REG_8192Emp, PHY_REG_8192Emp);
VAR_MAPPING(PHY_REG_PG_8192Emp, PHY_REG_PG_8192Emp);
VAR_MAPPING(PHY_REG_MP_8192Emp, PHY_REG_MP_8192Emp);
VAR_MAPPING(rtl8192EfwMP, rtl8192EfwMP);

// Power Tracking
VAR_MAPPING(TxPowerTrack_AP, TxPowerTrack_AP);

#ifdef TXPWR_LMT_92EE
#include "../../../data_TXPWR_LMT_92EE_new.c"
VAR_MAPPING(TXPWR_LMT_92EE_new,TXPWR_LMT_92EE_new);
#ifdef PWR_BY_RATE_92E_HP
#if CFG_HAL_HIGH_POWER_EXT_PA
#include "../../../data_TXPWR_LMT_92EE_hp.c"
VAR_MAPPING(TXPWR_LMT_92EE_hp,TXPWR_LMT_92EE_hp);
#endif
#endif
#endif

//3 MACDM
VAR_MAPPING(MACDM_def_high_8192E, MACDM_def_high_8192E);
VAR_MAPPING(MACDM_def_low_8192E, MACDM_def_low_8192E);
VAR_MAPPING(MACDM_def_normal_8192E, MACDM_def_normal_8192E);

VAR_MAPPING(MACDM_gen_high_8192E, MACDM_gen_high_8192E);
VAR_MAPPING(MACDM_gen_low_8192E, MACDM_gen_low_8192E);
VAR_MAPPING(MACDM_gen_normal_8192E, MACDM_gen_normal_8192E);

VAR_MAPPING(MACDM_txop_high_8192E, MACDM_txop_high_8192E);
VAR_MAPPING(MACDM_txop_low_8192E, MACDM_txop_low_8192E);
VAR_MAPPING(MACDM_txop_normal_8192E, MACDM_txop_normal_8192E);

VAR_MAPPING(MACDM_state_criteria_8192E, MACDM_state_criteria_8192E);





