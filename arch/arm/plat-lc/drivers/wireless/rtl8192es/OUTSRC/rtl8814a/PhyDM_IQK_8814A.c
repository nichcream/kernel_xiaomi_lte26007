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

#if !defined(__ECOS) && !defined(CONFIG_COMPAT_WIRELESS)
#include "Mp_Precomp.h"
#else
#include "../Mp_Precomp.h"
#endif
#include "../odm_precomp.h"





VOID
_IQK_BackupMacBB_8814A(
	IN PDM_ODM_T	pDM_Odm,
	IN pu4Byte		MACBB_backup,
	IN pu4Byte		Backup_MACBB_REG, 
	IN u4Byte		MACBB_NUM
	)
{
    u4Byte i;
     //save MACBB default value
    for (i = 0; i < MACBB_NUM; i++){
        MACBB_backup[i] = ODM_Read4Byte(pDM_Odm, Backup_MACBB_REG[i]);
    }
    
    ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("BackupMacBB Success!!!!\n"));
}


VOID
_IQK_BackupRF_8814A(
	IN PDM_ODM_T	pDM_Odm,
	IN u4Byte		RF_backup[][4],
	IN pu4Byte		Backup_RF_REG, 
	IN u4Byte		RF_NUM
	)	
{
    u4Byte i;
    //Save RF Parameters
        for (i = 0; i < RF_NUM; i++){
            RF_backup[i][ODM_RF_PATH_A] = ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_A, Backup_RF_REG[i], bMaskDWord);
        RF_backup[i][ODM_RF_PATH_B] = ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_B, Backup_RF_REG[i], bMaskDWord);
        RF_backup[i][ODM_RF_PATH_C] = ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_C, Backup_RF_REG[i], bMaskDWord);
        RF_backup[i][ODM_RF_PATH_D] = ODM_GetRFReg(pDM_Odm, ODM_RF_PATH_D, Backup_RF_REG[i], bMaskDWord);
        }

    ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("BackupRF Success!!!!\n"));
}


VOID
_IQK_AFESetting_8814A(
	IN PDM_ODM_T		pDM_Odm,
	IN BOOLEAN			Do_IQK
	)
{
	u4Byte ii;
	if(Do_IQK)
	{
		// IQK AFE Setting RX_WAIT_CCA mode 
		ODM_Write4Byte(pDM_Odm, 0xc60, 0x0e808003); 
		ODM_Write4Byte(pDM_Odm, 0xe60, 0x0e808003);
		ODM_Write4Byte(pDM_Odm, 0x1860, 0x0e808003);
		ODM_Write4Byte(pDM_Odm, 0x1a60, 0x0e808003);
		ODM_SetBBReg(pDM_Odm, 0x90c, BIT(13), 0x1);
		 
		ODM_SetBBReg(pDM_Odm, 0x764, BIT(10)|BIT(9), 0x3);
		ODM_SetBBReg(pDM_Odm, 0x764, BIT(10)|BIT(9), 0x0);

		ODM_SetBBReg(pDM_Odm, 0x804, BIT(2), 0x1);
		ODM_SetBBReg(pDM_Odm, 0x804, BIT(2), 0x0);
		
		ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("AFE IQK mode Success!!!!\n"));
	}
	else
	{
		ODM_Write4Byte(pDM_Odm, 0xc60, 0x07808003);
		ODM_Write4Byte(pDM_Odm, 0xe60, 0x07808003);
		ODM_Write4Byte(pDM_Odm, 0x1860, 0x07808003);
		ODM_Write4Byte(pDM_Odm, 0x1a60, 0x07808003);
		ODM_SetBBReg(pDM_Odm, 0x90c, BIT(13), 0x1);
	
		ODM_SetBBReg(pDM_Odm, 0x764, BIT(10)|BIT(9), 0x3);
		ODM_SetBBReg(pDM_Odm, 0x764, BIT(10)|BIT(9), 0x0);

		ODM_SetBBReg(pDM_Odm, 0x804, BIT(2), 0x1);
		ODM_SetBBReg(pDM_Odm, 0x804, BIT(2), 0x0);
		
		ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("AFE Normal mode Success!!!!\n"));
	}
	
}


VOID
_IQK_RestoreMacBB_8814A(
	IN PDM_ODM_T		pDM_Odm,
	IN pu4Byte		MACBB_backup,
	IN pu4Byte		Backup_MACBB_REG, 
	IN u4Byte		MACBB_NUM
	)	
{
    u4Byte i;
    //Reload MacBB Parameters 
        for (i = 0; i < MACBB_NUM; i++){
            ODM_Write4Byte(pDM_Odm, Backup_MACBB_REG[i], MACBB_backup[i]);
        }
        ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RestoreMacBB Success!!!!\n"));
}

VOID
_IQK_RestoreRF_8814A(
	IN PDM_ODM_T			pDM_Odm,
	IN pu4Byte			Backup_RF_REG,
	IN u4Byte 			RF_backup[][4],
	IN u4Byte			RF_REG_NUM
	)
{   
    u4Byte i;
        for (i = 0; i < RF_REG_NUM; i++){
            ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, Backup_RF_REG[i], bRFRegOffsetMask, RF_backup[i][ODM_RF_PATH_A]);
        ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, Backup_RF_REG[i], bRFRegOffsetMask, RF_backup[i][ODM_RF_PATH_B]);
        ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, Backup_RF_REG[i], bRFRegOffsetMask, RF_backup[i][ODM_RF_PATH_C]);
        ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, Backup_RF_REG[i], bRFRegOffsetMask, RF_backup[i][ODM_RF_PATH_D]);
        }

    ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RestoreRF Success!!!!\n"));
    
}

VOID 
PHY_ResetIQKResult_8814A(
	IN	PDM_ODM_T	pDM_Odm
)
{
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
	ODM_Write4Byte(pDM_Odm, 0x1b38, 0x20000000);
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
	ODM_Write4Byte(pDM_Odm, 0x1b38, 0x20000000);
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
	ODM_Write4Byte(pDM_Odm, 0x1b38, 0x20000000);
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
	ODM_Write4Byte(pDM_Odm, 0x1b38, 0x20000000);
	ODM_Write4Byte(pDM_Odm, 0xc10, 0x100);
	ODM_Write4Byte(pDM_Odm, 0xe10, 0x100);
	ODM_Write4Byte(pDM_Odm, 0x1810, 0x100);
	ODM_Write4Byte(pDM_Odm, 0x1a10, 0x100);
}

VOID 
_IQK_ResetNCTL_8814A(
	IN PDM_ODM_T
	pDM_Odm
)
{ 
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
	ODM_Write4Byte(pDM_Odm, 0x1b80, 0x00000006);
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
	ODM_Write4Byte(pDM_Odm, 0x1b80, 0x00000002);
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("ResetNCTL Success!!!!\n"))
}

VOID 
_IQK_ConfigureMAC_8814A(
	IN PDM_ODM_T		pDM_Odm
	)
{
    // ========MAC register setting========
    ODM_Write1Byte(pDM_Odm, 0x522, 0x3f);
    ODM_SetBBReg(pDM_Odm, 0x550, BIT(11)|BIT(3), 0x0);
    ODM_Write1Byte(pDM_Odm, 0x808, 0x00);               //      RX ante off
    ODM_SetBBReg(pDM_Odm, 0x838, 0xf, 0xe);             //      CCA off
    ODM_SetBBReg(pDM_Odm, 0xa14, BIT(9)|BIT(8), 0x3);   //          CCK RX Path off
    ODM_Write4Byte(pDM_Odm, 0xcb0, 0x77777777);
    ODM_Write4Byte(pDM_Odm, 0xeb0, 0x77777777);
    ODM_Write4Byte(pDM_Odm, 0x18b4, 0x77777777);
    ODM_Write4Byte(pDM_Odm, 0x1ab4, 0x77777777);
    ODM_SetBBReg(pDM_Odm, 0x1abc, 0x0ff00000, 0x77);
}


#define DBG_Msg 0


VOID
_IQK_Tx_8814A(
	IN PDM_ODM_T		pDM_Odm,
	IN u1Byte chnlIdx
	)
{	
	u1Byte		delay_count, cal = 0, ii;
	u1Byte		cal0_retry = 0, cal1_retry = 0, cal2_retry = 0, cal3_retry = 0;
	int 		TX_IQC[8] = {0}, RX_IQC[8];		//TX_IQC = [TX0_X,TX0_Y,TX1_X,TX1_Y,TX2_X,TX2_Y,TX3_X,TX3_Y];
											//RX_IQC = [RX0_X,RX0_Y,RX1_X,RX1_Y,RX2_X,RX2_Y,RX3_X,RX3_Y];
	BOOLEAN 	TX0_fail = TRUE, RX0_fail = TRUE, TX0_notready = TRUE, RX0_notready = TRUE, LOK0_notready = TRUE;
	BOOLEAN 	TX1_fail = TRUE, RX1_fail = TRUE, TX1_notready = TRUE, RX1_notready = TRUE, LOK1_notready = TRUE;
	BOOLEAN 	TX2_fail = TRUE, RX2_fail = TRUE, TX2_notready = TRUE, RX2_notready = TRUE, LOK2_notready = TRUE;
	BOOLEAN 	TX3_fail = TRUE, RX3_fail = TRUE, TX3_notready = TRUE, RX3_notready = TRUE, LOK3_notready = TRUE, WBRXIQK_Enable = FALSE;
	u4Byte		LOK_temp1, LOK_temp2;
	u4Byte		kk=0, jj=0, ll = 0, IQK_Loop;
	PODM_RF_CAL_T  pRFCalibrateInfo = &(pDM_Odm->RFCalibrateInfo);
	
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("BandWidth = %d, ExtPA2G = %d\n", *pDM_Odm->pBandWidth, pDM_Odm->ExtPA));
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("Interface = %d, pBandType = %d\n", pDM_Odm->SupportInterface, *pDM_Odm->pBandType));

	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, 0xef, bRFRegOffsetMask, 0x2);
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, 0xef, bRFRegOffsetMask, 0x2);
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, 0xef, bRFRegOffsetMask, 0x2);
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, 0xef, bRFRegOffsetMask, 0x2);
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, 0x58, BIT(19), 0x1);
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, 0x58, BIT(19), 0x1);
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, 0x58, BIT(19), 0x1);
	ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, 0x58, BIT(19), 0x1);
	
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
	ODM_Write4Byte(pDM_Odm, 0x1bb8, 0x00000000);
	
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
	ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
	ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
	ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
	ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
	ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
	
	if(*pDM_Odm->pBandType == ODM_BAND_5G)
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000ff1);
	else
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000ef1);

	delay_ms(1);

	ODM_Write4Byte(pDM_Odm, 0x810, 0x20101063);
	ODM_Write4Byte(pDM_Odm, 0x90c, 0x0B00c000);
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("============ LOK ============\n"));
	

	while(1){
//2 S0 LOK
		if(LOK0_notready){
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S0 LOK ==========\n"));
			ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x0); 
			ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000011);
			delay_ms(3);
			delay_count = 0;
			LOK0_notready = TRUE;
			while(LOK0_notready){
				LOK0_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
				delay_count++;
				if(delay_count >= 10){
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 LOK timeout!!!\n"));
					_IQK_ResetNCTL_8814A(pDM_Odm);
					break;
				}
			}
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> delay_count = 0x%d\n", delay_count));
			if(!LOK0_notready){
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
				ODM_Write4Byte(pDM_Odm, 0x1bd4, 0x003f0001);
				LOK_temp2 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x003e0000)+0x10)&0x1f;
				LOK_temp1 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x0000003e)+0x10)&0x1f;
				/*
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1, LOK_temp2));
				
				for(ii = 0; ii<5; ii++){
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, 0x8, BIT(ii+10), (LOK_temp1 & BIT(4-ii))>>(4-ii));
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, 0x8, BIT(ii+15), (LOK_temp2 & BIT(4-ii))>>(4-ii));
					//DbgPrint("index1 = %d, index2 = %d\n", (LOK_temp1 & BIT(4-ii))>>(4-ii), (LOK_temp2 & BIT(4-ii))>>(4-ii));
				}
				DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_A, 0x8, bRFRegOffsetMask));
				*/
				for(ii = 1; ii<5; ii++){
					LOK_temp1 = LOK_temp1 + ((LOK_temp1 & BIT(4-ii))<<(ii*2));
					LOK_temp2 = LOK_temp2 + ((LOK_temp2 & BIT(4-ii))<<(ii*2));
					//DbgPrint("index1 = %x, index2 = %x\n", ((LOK_temp1 & BIT(4-ii))<<(ii*2)), ((LOK_temp2 & BIT(4-ii))<<(ii*2)));
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1>>4, LOK_temp2>>4));
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, 0x8, 0x07c00, LOK_temp1>>4);
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, 0x8, 0xf8000, LOK_temp2>>4);
				//DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_A, 0x8, bRFRegOffsetMask));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==>S0 fill LOK \n"));

			}
			else{
				cal0_retry++;
			}
		}
		

//2 S1 LOK
		if(LOK1_notready){
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S1 LOK ==========\n"));
			ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x1); 
			ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000021);
			delay_ms(1);
			delay_count = 0;
			LOK1_notready = TRUE;
			while(LOK1_notready){
				LOK1_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
				delay_count++;
				if(delay_count >= 10){
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 LOK timeout!!!\n"));
					_IQK_ResetNCTL_8814A(pDM_Odm);
					break;
				}
			}
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> delay_count = 0x%d\n", delay_count));
			if(!LOK1_notready){
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
				ODM_Write4Byte(pDM_Odm, 0x1bd4, 0x003f0001);
				LOK_temp2 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x003e0000)+0x10)&0x1f;
				LOK_temp1 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x0000003e)+0x10)&0x1f;
				/*
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1, LOK_temp2));
				for(ii = 0; ii<5; ii++){
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, 0x8, BIT(ii+10), (LOK_temp1 & BIT(4-ii))>>(4-ii));
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, 0x8, BIT(ii+15), (LOK_temp2 & BIT(4-ii))>>(4-ii));
					//DbgPrint("index1 = %d, index2 = %d\n", (LOK_temp1 & BIT(4-ii))>>(4-ii), (LOK_temp2 & BIT(4-ii))>>(4-ii));
				}
				DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_B, 0x8, bRFRegOffsetMask));
				*/
				for(ii = 1; ii<5; ii++){
					LOK_temp1 = LOK_temp1 + ((LOK_temp1 & BIT(4-ii))<<(ii*2));
					LOK_temp2 = LOK_temp2 + ((LOK_temp2 & BIT(4-ii))<<(ii*2));
					//DbgPrint("index1 = %x, index2 = %x\n", ((LOK_temp1 & BIT(4-ii))<<(ii*2)), ((LOK_temp2 & BIT(4-ii))<<(ii*2)));
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1>>4, LOK_temp2>>4));
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, 0x8, 0x07c00, LOK_temp1>>4);
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, 0x8, 0xf8000, LOK_temp2>>4);
				//DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_B, 0x8, bRFRegOffsetMask));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==>S1 fill LOK \n"));

			}
			else{
				cal1_retry++;
			}
		}

//2 S2 LOK
		if(LOK2_notready){
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S2 LOK ==========\n"));
			ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x2); 
			ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000041);
			delay_ms(1);
			delay_count = 0;
			LOK2_notready = TRUE;
			while(LOK2_notready){
				LOK2_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
				delay_count++;
				if(delay_count >= 10){
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 LOK timeout!!!\n"));
					_IQK_ResetNCTL_8814A(pDM_Odm);
					break;
				}
			}
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> delay_count = 0x%d\n", delay_count));
			if(!LOK2_notready){
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
				ODM_Write4Byte(pDM_Odm, 0x1bd4, 0x003f0001);
				LOK_temp2 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x003e0000)+0x10)&0x1f;
				LOK_temp1 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x0000003e)+0x10)&0x1f;
				/*
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1, LOK_temp2));
				for(ii = 0; ii<5; ii++){
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, 0x8, BIT(ii+10), (LOK_temp1 & BIT(4-ii))>>(4-ii));
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, 0x8, BIT(ii+15), (LOK_temp2 & BIT(4-ii))>>(4-ii));
					//DbgPrint("index1 = %d, index2 = %d\n", (LOK_temp1 & BIT(4-ii))>>(4-ii), (LOK_temp2 & BIT(4-ii))>>(4-ii));
				}
				DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_C, 0x8, bRFRegOffsetMask));
				*/
				for(ii = 1; ii<5; ii++){
					LOK_temp1 = LOK_temp1 + ((LOK_temp1 & BIT(4-ii))<<(ii*2));
					LOK_temp2 = LOK_temp2 + ((LOK_temp2 & BIT(4-ii))<<(ii*2));
					//DbgPrint("index1 = %x, index2 = %x\n", ((LOK_temp1 & BIT(4-ii))<<(ii*2)), ((LOK_temp2 & BIT(4-ii))<<(ii*2)));
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1>>4, LOK_temp2>>4));
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, 0x8, 0x07c00, LOK_temp1>>4);
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, 0x8, 0xf8000, LOK_temp2>>4);
				//DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_C, 0x8, bRFRegOffsetMask));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==>S2 fill LOK \n"));

			}
			else{
				cal2_retry++;
			}
		}

//2 S3 LOK
		if(LOK3_notready){
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S3 LOK ==========\n"));
			ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x3);
			ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000081);
			delay_ms(1);
			delay_count = 0;
			LOK3_notready = TRUE;
			while(LOK3_notready){
				LOK3_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
				delay_count++;
				if(delay_count >= 10){
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 LOK timeout!!!\n"));
					_IQK_ResetNCTL_8814A(pDM_Odm);
					break;
				}
			}
			ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> delay_count = 0x%d\n", delay_count));
			if(!LOK3_notready){
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
				ODM_Write4Byte(pDM_Odm, 0x1bd4, 0x003f0001);
				LOK_temp2 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x003e0000)+0x10)&0x1f;
				LOK_temp1 = (ODM_GetBBReg(pDM_Odm, 0x1bfc, 0x0000003e)+0x10)&0x1f;
				/*
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1, LOK_temp2));
				for(ii = 0; ii<5; ii++){
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, 0x8, BIT(ii+10), (LOK_temp1 & BIT(4-ii))>>(4-ii));
					ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, 0x8, BIT(ii+15), (LOK_temp2 & BIT(4-ii))>>(4-ii));
					//DbgPrint("index1 = %d, index2 = %d\n", (LOK_temp1 & BIT(4-ii))>>(4-ii), (LOK_temp2 & BIT(4-ii))>>(4-ii));
				}
				DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_D, 0x8, bRFRegOffsetMask));
				*/
				for(ii = 1; ii<5; ii++){
					LOK_temp1 = LOK_temp1 + ((LOK_temp1 & BIT(4-ii))<<(ii*2));
					LOK_temp2 = LOK_temp2 + ((LOK_temp2 & BIT(4-ii))<<(ii*2));
					//DbgPrint("index1 = %x, index2 = %x\n", ((LOK_temp1 & BIT(4-ii))<<(ii*2)), ((LOK_temp2 & BIT(4-ii))<<(ii*2)));
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK_temp1 = 0x%x, LOK_temp2 = 0x%x\n", LOK_temp1>>4, LOK_temp2>>4));
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, 0x8, 0x07c00, LOK_temp1>>4);
				ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, 0x8, 0xf8000, LOK_temp2>>4);
				//DbgPrint("RF 0x8 = %x\n", ODM_GetRFReg( pDM_Odm, ODM_RF_PATH_D, 0x8, bRFRegOffsetMask));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==>S3 fill LOK \n"));

			}
			else{
				cal3_retry++;
			}
		}
		kk++;
#if DBG_Msg
		DbgPrint("kk = %d\n", kk);
#endif
		if(kk > 100)
			break;
		if((!LOK0_notready) && (!LOK1_notready) && (!LOK2_notready) && (!LOK3_notready))
			break;
		if(cal0_retry >= 3 || cal1_retry >= 3 || cal2_retry >= 3 || cal3_retry >= 3)
			break;
		
	}

	if(LOK0_notready)
		ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_A, 0x8, bRFRegOffsetMask, 0x08400);
	if(LOK1_notready)
		ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_B, 0x8, bRFRegOffsetMask, 0x08400);
	if(LOK2_notready)
		ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_C, 0x8, bRFRegOffsetMask, 0x08400);
	if(LOK3_notready)
		ODM_SetRFReg(pDM_Odm, ODM_RF_PATH_D, 0x8, bRFRegOffsetMask, 0x08400);
	
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("LOK0_notready = %d, LOK1_notready = %d, LOK2_notready = %d, LOK3_notready = %d\n", LOK0_notready, LOK1_notready, LOK2_notready, LOK3_notready));
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("cal0_retry = %d, cal1_retry = %d, cal2_retry = %d, cal3_retry = %d\n", cal0_retry, cal1_retry, cal2_retry, cal3_retry));


	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("============ TXIQK ============\n"));
	cal0_retry = 0;
	cal1_retry = 0;
	cal2_retry = 0;
	cal3_retry = 0;
	while(1){
		if(*pDM_Odm->pBandWidth != ODM_BW80M){
//1 Normal TX IQK
			if( TX0_fail){
//2 S0 TXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S0 TXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x0); 
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000111);
				delay_ms(7);
				delay_count = 0;
				TX0_notready = TRUE;
				while(TX0_notready){
					TX0_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX0_notready){
						TX0_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 TXIQK timeout!!!\n"));
						ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> delay_count = 0x%d\n", delay_count));
				if(!TX0_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
					
					TX_IQC[0] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[1] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX0_X = 0x%x, TX0_Y = 0x%x\n", TX_IQC[0], TX_IQC[1]));
				}
				else{
					cal0_retry++;
				}
			}

			if(TX1_fail){
//2 S1 TXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S1 TXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x1); 
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000121);
				delay_ms(7);
				delay_count = 0;
				TX1_notready = TRUE;
				while(TX1_notready){
					TX1_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX1_notready){
						TX1_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 TXIQK timeout!!!\n"));
						ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> delay_count = 0x%d\n", delay_count));
				if(!TX1_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
					
					TX_IQC[2] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[3] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX1_X = 0x%x, TX1_Y = 0x%x\n", TX_IQC[2], TX_IQC[3]));
				}
				else{
					cal1_retry++;
				}
			}

			if(TX2_fail){
//2 S2 TXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S2 TXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x2);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000141);
				delay_ms(7);
				delay_count = 0;
				TX2_notready = TRUE;
				while(TX2_notready){
					TX2_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX2_notready){
						TX2_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 TXIQK timeout!!!\n"));
						ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> delay_count = 0x%d\n", delay_count));
				if(!TX2_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
					
					TX_IQC[4] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[5] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX2_X = 0x%x, TX2_Y = 0x%x\n", TX_IQC[4], TX_IQC[5]));
				}
				else{
					cal2_retry++;
				}
			}

			if(TX3_fail){
//2 S3 TXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S3 TXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x3);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000181);
				delay_ms(7);
				delay_count = 0;
				TX3_notready = TRUE;
				while(TX3_notready){
					TX3_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX3_notready){
						TX3_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 TXIQK timeout!!!\n"));
						ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> delay_count = 0x%d\n", delay_count));
				if(!TX3_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					ODM_Write4Byte( pDM_Odm, 0x1bd4, 0x000f0001);	
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("0x1bfc = 0x%x\n", ODM_Read4Byte( pDM_Odm, 0x1bfc)));
					
					TX_IQC[6] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[7] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX3_X = 0x%x, TX3_Y = 0x%x\n", TX_IQC[6], TX_IQC[7]));
				}
				else{
					cal3_retry++;
				}
			}
		}
		else{
//1 WideBand TXIQK
			if(TX0_fail){
//2 S0 WBTXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S0 WBTXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x0); 
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000511);
				delay_ms(18);

				delay_count = 0;
				TX0_notready = TRUE;
				while(TX0_notready){
					TX0_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX0_notready){
						TX0_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 WBTXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> delay_count = 0x%d\n", delay_count));
				if(!TX0_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78003000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					TX_IQC[0] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[1] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX0_X = 0x%x, TX0_Y = 0x%x\n", TX_IQC[0], TX_IQC[1]));
				}
				else{
					cal0_retry++;
				}
			}
			
			if(TX1_fail){
//2 S1 WBTXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S1 WBTXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x1); 
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000521);
				delay_ms(18);

				delay_count = 0;
				TX1_notready = TRUE;
				while(TX1_notready){
					TX1_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX1_notready){
						TX1_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 WBTXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> delay_count = 0x%d\n", delay_count));
				if(!TX1_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78003000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					TX_IQC[2] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[3] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX1_X = 0x%x, TX1_Y = 0x%x\n", TX_IQC[2], TX_IQC[3]));
				}
				else{
					cal1_retry++;
				}
			}

			if(TX2_fail){
//2 S2 WBTXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S2 WBTXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x2); 
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000541);
				delay_ms(18);

				delay_count = 0;
				TX2_notready = TRUE;
				while(TX2_notready){
					TX2_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX2_notready){
						TX2_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 WBTXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> delay_count = 0x%d\n", delay_count));
				if(!TX2_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78003000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					TX_IQC[4] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[5] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX2_X = 0x%x, TX2_Y = 0x%x\n", TX_IQC[4], TX_IQC[5]));
				}
				else{
					cal2_retry++;
				}
			}

			if(TX3_fail){
//2 S3 WBTXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S3 WBTXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x3); 
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000581);
				delay_ms(18);

				delay_count = 0;
				TX3_notready = TRUE;
				while(TX3_notready){
					TX3_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!TX3_notready){
						TX3_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 WBTXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> delay_count = 0x%d\n", delay_count));
				if(!TX3_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78003000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					TX_IQC[6] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x7ff00000);
					TX_IQC[7] = ODM_GetBBReg(pDM_Odm, 0x1b38, 0x0007ff00);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX3_X = 0x%x, TX3_Y = 0x%x\n", TX_IQC[6], TX_IQC[7]));
				}
				else{
					cal3_retry++;
				}
			}
		}

		jj++;
#if DBG_Msg
		DbgPrint("jj = %d\n", jj);
#endif
		if(jj > 100)
			break;
		if((!TX0_fail)&& (!TX1_fail) && (!TX2_fail) && (!TX3_fail))
			break;
		if(cal0_retry >= 3 || cal1_retry >= 3 || cal2_retry >= 3 || cal3_retry >= 3)
			break;

	}

	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("TX0_fail = %d, TX1_fail = %d, TX2_fail = %d, TX3_fail = %d\n", TX0_fail, TX1_fail, TX2_fail, TX3_fail));
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("cal0_retry = %d, cal1_retry = %d, cal2_retry = %d, cal3_retry = %d\n", cal0_retry, cal1_retry, cal2_retry, cal3_retry));

if((pDM_Odm->SupportInterface == ODM_ITRF_USB && *pDM_Odm->pBandType == ODM_BAND_5G)||(pDM_Odm->RFEType == 2 && pDM_Odm->SupportInterface == ODM_ITRF_PCIE)){
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("============ RXIQK ============\n"));
	cal0_retry = 0;
	cal1_retry = 0;
	cal2_retry = 0;
	cal3_retry = 0;
	
	while(1){
		if(!WBRXIQK_Enable)
		{
//1 Normal RX IQK
			if( RX0_fail)
			{
//2 S0 RXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S0 RXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x0);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000211);
				delay_ms(12);

				delay_count = 0;
				RX0_notready = TRUE;
				while(RX0_notready){
					RX0_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX0_notready){
						RX0_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 RXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> delay_count = 0x%d\n", delay_count));
				if(!RX0_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					RX_IQC[0] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[1] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					//ODM_SetBBReg( pDM_Odm, 0xc10, 0x000003ff, RX_IQC[0]>>1);
					//ODM_SetBBReg( pDM_Odm, 0xc10, 0x03ff0000, RX_IQC[1]>>1);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX0_X = 0x%x, RX0_Y = 0x%x\n", RX_IQC[0], RX_IQC[1]));
				}
				else{
					cal0_retry++;
				}
			}

			if(RX1_fail){
//2 S1 RXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S1 RXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x1);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000221);
				delay_ms(12);

				delay_count = 0;
				RX1_notready = TRUE;
				while(RX1_notready){
					RX1_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX1_notready){
						RX1_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 RXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> delay_count = 0x%d\n", delay_count));
				if(!RX1_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					RX_IQC[2] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[3] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					//ODM_SetBBReg( pDM_Odm, 0xe10, 0x000003ff, RX_IQC[2]>>1);
					//ODM_SetBBReg( pDM_Odm, 0xe10, 0x03ff0000, RX_IQC[3]>>1);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX1_X = 0x%x, RX1_Y = 0x%x\n", RX_IQC[2], RX_IQC[3]));
				}
				else{
					cal1_retry++;
				}
			}

			if(RX2_fail){
//2 S2 RXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S2 RXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x2);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000241);
				delay_ms(12);

				delay_count = 0;
				RX2_notready = TRUE;
				while(RX2_notready){
					RX2_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX2_notready){
						RX2_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 TXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> delay_count = 0x%d\n", delay_count));
				if(!RX2_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					RX_IQC[4] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[5] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					//ODM_SetBBReg( pDM_Odm, 0x1810, 0x000003ff, RX_IQC[4]>>1);
					//ODM_SetBBReg( pDM_Odm, 0x1810, 0x03ff0000, RX_IQC[5]>>1);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX2_X = 0x%x, RX2_Y = 0x%x\n", RX_IQC[4], RX_IQC[5]));
				}
				else{
					cal2_retry++;
				}
			}

			if(RX3_fail){
//2 S3 RXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S3 RXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x3);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000281);
				delay_ms(12);

				delay_count = 0;
				RX3_notready = TRUE;
				while(RX3_notready){
					RX3_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX3_notready){
						RX3_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 RXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> delay_count = 0x%d\n", delay_count));
				if(!RX3_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
					ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
					RX_IQC[6] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[7] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					//ODM_SetBBReg( pDM_Odm, 0x1a10, 0x000003ff, RX_IQC[6]>>1);
					//ODM_SetBBReg( pDM_Odm, 0x1a10, 0x03ff0000, RX_IQC[7]>>1);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX3_X = 0x%x, RX3_Y = 0x%x\n", RX_IQC[6], RX_IQC[7]));
				}
				else{
					cal3_retry++;
				}
			}
		}
		else
		{
			ODM_Write4Byte(pDM_Odm, 0xc10, 0x100);
			ODM_Write4Byte(pDM_Odm, 0xe10, 0x100);
			ODM_Write4Byte(pDM_Odm, 0x1810, 0x100);
			ODM_Write4Byte(pDM_Odm, 0x1a10, 0x100);
			//1 WideBand RX IQK
			if( RX0_fail)
			{
//2 S0 WBRXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S0 WBRXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x0);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000711);
				delay_ms(19);

				delay_count = 0;
				RX0_notready = TRUE;
				while(RX0_notready){
					RX0_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX0_notready){
						RX0_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 WBRXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S0 ==> delay_count = 0x%d\n", delay_count));
				if(!RX0_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
					RX_IQC[0] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[1] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78001000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					ODM_Write4Byte(pDM_Odm, 0x1b3c, 0x20000000);					
					//ODM_Write4Byte(pDM_Odm, 0xc10, 0x100);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX0_X = 0x%x, RX0_Y = 0x%x\n", RX_IQC[0], RX_IQC[1]));
				}
		else{
					cal0_retry++;
				}
			}

			if(RX1_fail){
//2 S1 WBRXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S1 WBRXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x1);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000721);
				delay_ms(19);

				delay_count = 0;
				RX1_notready = TRUE;
				while(RX1_notready){
					RX1_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX1_notready){
						RX1_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 WBRXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S1 ==> delay_count = 0x%d\n", delay_count));
				if(!RX1_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
					RX_IQC[2] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[3] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78001000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					ODM_Write4Byte(pDM_Odm, 0x1b3c, 0x20000000);					
					//ODM_Write4Byte(pDM_Odm, 0xe10, 0x100);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX1_X = 0x%x, RX1_Y = 0x%x\n", RX_IQC[2], RX_IQC[3]));
				}
				else{
					cal1_retry++;
				}
			}

			if(RX2_fail){
//2 S2 WBRXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S2 WBRXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x2);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000741);
				delay_ms(19);

				delay_count = 0;
				RX2_notready = TRUE;
				while(RX2_notready){
					RX2_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX2_notready){
						RX2_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 WBRXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S2 ==> delay_count = 0x%d\n", delay_count));
				if(!RX2_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
					RX_IQC[4] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[5] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78001000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					ODM_Write4Byte(pDM_Odm, 0x1b3c, 0x20000000);					
					//ODM_Write4Byte(pDM_Odm, 0x1810, 0x100);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX2_X = 0x%x, RX2_Y = 0x%x\n", RX_IQC[4], RX_IQC[5]));
				}
				else{
					cal2_retry++;
				}
			}

			if(RX3_fail){
//2 S3 WBRXIQK
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("==========S3 WBRXK ==========\n"));
				ODM_SetBBReg(pDM_Odm, 0x9a4, BIT(21)|BIT(20), 0x3);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
				//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000003);
				ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000781);
				delay_ms(19);

				delay_count = 0;
				RX3_notready = TRUE;
				while(RX3_notready){
					RX3_notready = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b00, BIT(0));
					if(!RX3_notready){
						RX3_fail = (BOOLEAN) ODM_GetBBReg(pDM_Odm, 0x1b08, BIT(26));
						break;
					}
					delay_count++;
					if(delay_count >= 10){
						ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 WBRXIQK timeout!!!\n"));
						_IQK_ResetNCTL_8814A(pDM_Odm);
						break;
					}
				}
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b00 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b00)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> 0x1b08 = 0x%x\n", ODM_Read4Byte(pDM_Odm, 0x1b08)));
				ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("S3 ==> delay_count = 0x%d\n", delay_count));
				if(!RX3_fail){
					ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
					RX_IQC[6] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x7ff00000);
					RX_IQC[7] = ODM_GetBBReg(pDM_Odm, 0x1b3c, 0x0007ff00);
					ODM_Write4Byte(pDM_Odm, 0x1b0c, 0x78001000);
					//ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
					ODM_Write4Byte(pDM_Odm, 0x1b3c, 0x20000000);					
					//ODM_Write4Byte(pDM_Odm, 0x1a10, 0x100);
					ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX3_X = 0x%x, RX3_Y = 0x%x\n", RX_IQC[6], RX_IQC[7]));
				}
				else
				{
					cal3_retry++;
				}
			}
		}

		ll++;
#if DBG_Msg
		DbgPrint("ll = %d\n",ll);
#endif
		if(ll > 100)
			break;
		if((!RX0_fail) && (!RX1_fail) && (!RX2_fail) && (!RX3_fail))
			break;
		if(cal0_retry >= 3 || cal1_retry >= 3 || cal2_retry >= 3 || cal3_retry >= 3)
			break;

		}

	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("RX0_fail = %d, RX1_fail = %d, RX2_fail = %d, RX3_fail = %d\n", RX0_fail, RX1_fail, RX2_fail, RX3_fail));
	ODM_RT_TRACE(pDM_Odm, ODM_COMP_CALIBRATION, ODM_DBG_LOUD, ("cal0_retry = %d, cal1_retry = %d, cal2_retry = %d, cal3_retry = %d\n", cal0_retry, cal1_retry, cal2_retry, cal3_retry));
		}

	if(!TX0_fail){
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
		ODM_Write4Byte( pDM_Odm, 0x1b38, (TX_IQC[0] << 20) + (TX_IQC[1] << 8));
	}
	if(!TX1_fail){
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
		ODM_Write4Byte( pDM_Odm, 0x1b38, (TX_IQC[2] << 20) + (TX_IQC[3] << 8));
	}
	if(!TX2_fail){
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
		ODM_Write4Byte( pDM_Odm, 0x1b38, (TX_IQC[4] << 20) + (TX_IQC[5] << 8));
	}
	if(!TX3_fail){
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
		ODM_Write4Byte( pDM_Odm, 0x1b38, (TX_IQC[6] << 20) + (TX_IQC[7] << 8));
	}

	if (*pDM_Odm->pBandWidth == ODM_BW80M){
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000000);
		ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
		ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000002);
		ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
		ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000004);
		ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
		ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
		ODM_Write4Byte(pDM_Odm, 0x1b00, 0xf8000006);
		ODM_Write4Byte(pDM_Odm, 0x1b2c, 0x20000007);
		ODM_SetBBReg( pDM_Odm, 0x1b0c, 0x00003000, 0x3);
	}

}

#define MACBB_REG_NUM_8814 15
#define RF_REG_NUM_8814 1

VOID	
phy_IQCalibrate_8814A(
	IN PDM_ODM_T		pDM_Odm,
	IN u1Byte		Channel
	)
{
    
    u4Byte  MACBB_backup[MACBB_REG_NUM_8814], RF_backup[RF_REG_NUM_8814][4];
    u4Byte  Backup_MACBB_REG[MACBB_REG_NUM_8814] = {0x520, 0x550, 0xa14, 0x808, 0x838, 0x90c, 0x810, 0xcb0, 0xeb0,
        0x18b4, 0x1ab4, 0x1abc, 0x8ac, 0x9a4, 0x764}; 
    u4Byte  Backup_RF_REG[RF_REG_NUM_8814] = {0x0}; 
    u1Byte  chnlIdx = ODM_GetRightChnlPlaceforIQK(Channel);
    
    
    _IQK_BackupMacBB_8814A(pDM_Odm, MACBB_backup, Backup_MACBB_REG, MACBB_REG_NUM_8814);
    _IQK_AFESetting_8814A(pDM_Odm,TRUE);
    _IQK_BackupRF_8814A(pDM_Odm, RF_backup, Backup_RF_REG, RF_REG_NUM_8814);
    _IQK_ConfigureMAC_8814A(pDM_Odm);
    _IQK_Tx_8814A(pDM_Odm, chnlIdx);
    _IQK_AFESetting_8814A(pDM_Odm,FALSE);
	_IQK_RestoreMacBB_8814A(pDM_Odm, MACBB_backup, Backup_MACBB_REG, MACBB_REG_NUM_8814);
  	_IQK_RestoreRF_8814A(pDM_Odm, Backup_RF_REG, RF_backup, RF_REG_NUM_8814);

}


VOID
PHY_IQCalibrate_8814A(
	IN 	PDM_ODM_T	pDM_Odm,
	IN	BOOLEAN 	bReCovery
	)
{

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
	PADAPTER 		pAdapter = pDM_Odm->Adapter;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(pAdapter);	

	#if (MP_DRIVER == 1)
		#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)	
			PMPT_CONTEXT	pMptCtx = &(pAdapter->MptCtx);	
		#else// (DM_ODM_SUPPORT_TYPE == ODM_CE)
			PMPT_CONTEXT	pMptCtx = &(pAdapter->mppriv.MptCtx);		
		#endif	
	#endif//(MP_DRIVER == 1)
	#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN|ODM_CE) )
		if (ODM_CheckPowerStatus(pAdapter) == FALSE)
			return;
	#endif

	#if MP_DRIVER == 1	
		if( pMptCtx->bSingleTone || pMptCtx->bCarrierSuppression )
			return;
	#endif
	
#endif	

	phy_IQCalibrate_8814A(pDM_Odm, *pDM_Odm->pChannel);
	
}

