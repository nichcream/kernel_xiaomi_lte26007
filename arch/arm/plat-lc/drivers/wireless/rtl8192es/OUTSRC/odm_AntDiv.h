/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *                                        
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
 
#ifndef	__ODMANTDIV_H__
#define    __ODMANTDIV_H__



#define ANT1_2G 0 // = ANT2_5G
#define ANT2_2G 1 // = ANT1_5G

//Antenna Diversty Control Type
#define	ODM_AUTO_ANT	0
#define	ODM_FIX_MAIN_ANT	1
#define	ODM_FIX_AUX_ANT	2
#define	ODM_STAY_CURR_ANT	 3

#if (DM_ODM_SUPPORT_TYPE != ODM_AP)
#define ODM_RTL8881A 0   //Just for windows driver to jointly use ODM-driver
#endif

#define ODM_ANTDIV_SUPPORT			(ODM_RTL8188E|ODM_RTL8192E|ODM_RTL8723B|ODM_RTL8821|ODM_RTL8881A|ODM_RTL8812)
#define ODM_N_ANTDIV_SUPPORT		(ODM_RTL8188E|ODM_RTL8192E|ODM_RTL8723B)
#define ODM_AC_ANTDIV_SUPPORT		(ODM_RTL8821|ODM_RTL8881A|ODM_RTL8812)
#define ODM_SMART_ANT_SUPPORT		(ODM_RTL8188E|ODM_RTL8192E)

#define ODM_ANTDIV_2G_SUPPORT_IC			(ODM_RTL8188E|ODM_RTL8192E|ODM_RTL8723B|ODM_RTL8881A)
#define ODM_ANTDIV_5G_SUPPORT_IC			(ODM_RTL8821|ODM_RTL8881A|ODM_RTL8812)

#define ODM_EVM_ENHANCE_ANTDIV_SUPPORT_IC	(ODM_RTL8192E)

#define ODM_ANTDIV_2G	BIT0
#define ODM_ANTDIV_5G	BIT1

#define ANTDIV_ON 1
#define ANTDIV_OFF 0

#define FAT_ON 1
#define FAT_OFF 0

#define TX_DESC 1
#define REG 0

#define RSSI_METHOD 0
#define EVM_METHOD 1
#define CRC32_METHOD 2



#define CRC32_FAIL 1
#define CRC32_OK 0

#define Evm_RSSI_TH_High 25
#define Evm_RSSI_TH_Low 20

#define NORMAL_STATE_MIAN 1
#define NORMAL_STATE_AUX 2
#define TRAINING_STATE 3

#define FORCE_RSSI_DIFF 10

#define CSI_ON 1
#define CSI_OFF 0

#define DIVON_CSIOFF 1
#define DIVOFF_CSION 2

#define BDC_DIV_TRAIN_STATE 0
#define BDC_BFer_TRAIN_STATE 1
#define BDC_DECISION_STATE 2
#define BDC_BF_HOLD_STATE 3
#define BDC_DIV_HOLD_STATE 4

#define BDC_MODE_1 1
#define BDC_MODE_2 2
#define BDC_MODE_3 3
#define BDC_MODE_4 4
#define BDC_MODE_NULL 0xff

//----------------------------------------------------------------
#define ODM_AP_AntDiv_Printk( fmt,...)		\
		if(pDM_Odm->antdiv_rssi)			\
		{								\
			panic_printk(fmt);				\
		}
#define ODM_WIN_AntDiv_Printk(  fmt,...)		\
		 ODM_RT_TRACE(pDM_Odm, ODM_COMP_ANT_DIV,ODM_DBG_LOUD,fmt);\
		 			 							
#if (DM_ODM_SUPPORT_TYPE == ODM_AP)
	#define  ODM_AntDiv_Printk(  fmt)   ODM_AP_AntDiv_Printk( fmt)	
#else
	#define  ODM_AntDiv_Printk( fmt)   ODM_WIN_AntDiv_Printk(  fmt)
#endif
//----------------------------------------------------------------

#define SwAntDivRestAfterLink	ODM_SwAntDivRestAfterLink
VOID ODM_SwAntDivRestAfterLink(	IN	PDM_ODM_T	pDM_Odm);

#if (defined(CONFIG_HW_ANTENNA_DIVERSITY))

VOID
ODM_UpdateRxIdleAnt(
	IN	 	PDM_ODM_T 		pDM_Odm, 
	IN		 u1Byte			Ant
);

#if(RTL8723B_SUPPORT == 1|| RTL8821A_SUPPORT == 1) 
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
VOID
ODM_SW_AntDiv_Callback(
	IN 	PRT_TIMER			pTimer
);

VOID
ODM_SW_AntDiv_WorkitemCallback(
    	IN 	PVOID            pContext
);
#endif //#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
#endif



#ifdef ODM_EVM_ENHANCE_ANTDIV
VOID
odm_EVM_FastAntTrainingCallback(
	IN		PDM_ODM_T		pDM_Odm
);
#endif

VOID
odm_HW_AntDiv(
	IN		PDM_ODM_T		pDM_Odm
);

//#if(RTL8188E_SUPPORT == 1  || RTL8192E_SUPPORT == 1)
//#if ( !(DM_ODM_SUPPORT_TYPE == ODM_CE))
#if( defined(CONFIG_5G_CG_SMART_ANT_DIVERSITY) ) ||( defined(CONFIG_2G_CG_SMART_ANT_DIVERSITY) )
VOID
odm_FastAntTraining(
	IN		PDM_ODM_T		pDM_Odm
);

VOID
odm_FastAntTrainingCallback(
	IN		PDM_ODM_T		pDM_Odm
);

VOID
odm_FastAntTrainingWorkItemCallback(
	IN		PDM_ODM_T		pDM_Odm
);
#endif
//#endif
//#endif

VOID
ODM_AntDivInit(
	IN		 PDM_ODM_T		pDM_Odm 
);

VOID
ODM_AntDiv(
	IN		PDM_ODM_T		pDM_Odm
);

VOID
odm_AntselStatistics(
	IN		PDM_ODM_T		pDM_Odm,
	IN		u1Byte			antsel_tr_mux,
	IN		u4Byte			MacId,
	IN		u4Byte			utility,
	IN            u1Byte			method
);

VOID
ODM_Process_RSSIForAntDiv(	
	IN OUT	PDM_ODM_T					pDM_Odm,
	IN		PODM_PHY_INFO_T				pPhyInfo,
	IN		PODM_PACKET_INFO_T			pPktinfo
);

#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN|ODM_CE))
VOID
ODM_SetTxAntByTxInfo(
	IN		PDM_ODM_T		pDM_Odm,
	IN		pu1Byte			pDesc,
	IN		u1Byte			macId	
);

#elif(DM_ODM_SUPPORT_TYPE == ODM_AP)

VOID
ODM_SetTxAntByTxInfo(
	//IN		PDM_ODM_T		pDM_Odm,
	struct	rtl8192cd_priv		*priv,
	struct 	tx_desc			*pdesc,
	struct	tx_insn			*txcfg,
	unsigned short			aid	
);

VOID
ODM_AP_AntDiv_Config(
	IN		PDM_ODM_T		pDM_Odm
);

#endif

#endif //#if (defined(CONFIG_HW_ANTENNA_DIVERSITY))
#endif //#ifndef	__ODMANTDIV_H__
