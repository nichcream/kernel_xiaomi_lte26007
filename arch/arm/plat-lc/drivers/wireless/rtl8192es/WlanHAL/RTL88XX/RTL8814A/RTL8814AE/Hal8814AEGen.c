/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8814AEGen.c
	
Abstract:
	Defined RTL8814AE HAL Function
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2013-05-28 Filen            Create.	
--*/

#ifndef __ECOS
#include "HalPrecomp.h"
#else
#include "../../../HalPrecomp.h"
#endif

RT_STATUS
InitPON8814AE(
    IN  HAL_PADAPTER Adapter,
    IN  u4Byte     	ClkSel        
)
{
    u32     bytetmp;
    u32     retry;
    
    RT_TRACE_F( COMP_INIT, DBG_LOUD, ("\n"));

    // TODO: Filen, first write IO will fail, don't know the root cause
    printk("0: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));
	HAL_RTL_W8(REG_RSV_CTRL, 0x00);
    printk("1: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));
	HAL_RTL_W8(REG_RSV_CTRL, 0x00);
    printk("2: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));

    // TODO: Filen, check 8814A setting
	if(ClkSel == XTAL_CLK_SEL_25M) {
	} else if (ClkSel == XTAL_CLK_SEL_40M){
	}	

	if (!HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
			PWR_INTF_PCI_MSK, rtl8814A_card_enable_flow))
    {
        RT_TRACE( COMP_INIT, DBG_SERIOUS, ("%s %d, HalPwrSeqCmdParsing init fail!!!\n", __FUNCTION__, __LINE__));
        return RT_STATUS_FAILURE;
    }

    printk("3: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));

#ifdef RTL_8814A_MP_TEMP

    HAL_RTL_W32(REG_BD_RWPTR_CLR,0xffffffff);

    HAL_RTL_W32(0x1000, HAL_RTL_R32(0x1000)|BIT16|BIT17);
    printk("%s(%d): 0x1000:0x%x \n", __FUNCTION__, __LINE__, HAL_RTL_R32(0x1000));
#endif

    return  RT_STATUS_SUCCESS;
}


RT_STATUS
StopHW8814AE(
    IN  HAL_PADAPTER Adapter
)
{
    // TODO:

    return RT_STATUS_SUCCESS;
}


RT_STATUS
ResetHWForSurprise8814AE(
    IN  HAL_PADAPTER Adapter
)
{
    // TODO: Filen, necessary to be added code here

    return RT_STATUS_SUCCESS;
}


RT_STATUS	
hal_Associate_8814AE(
    HAL_PADAPTER        Adapter,
    BOOLEAN             IsDefaultAdapter
)
{
    PHAL_INTERFACE              pHalFunc = GET_HAL_INTERFACE(Adapter);
    PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);

    //
    //Initialization Related
    //
    pHalData->AccessSwapCtrl        = HAL_ACCESS_SWAP_MEM;
    pHalFunc->InitPONHandler        = InitPON8814AE;
    pHalFunc->InitMACHandler        = InitMAC88XX;
    pHalFunc->InitFirmwareHandler   = InitMIPSFirmware88XX;
    pHalFunc->InitHCIDMAMemHandler  = InitHCIDMAMem88XX;
    pHalFunc->InitHCIDMARegHandler  = InitHCIDMAReg88XX;    
#if CFG_HAL_SUPPORT_MBSSID    
    pHalFunc->InitMBSSIDHandler     = InitMBSSID88XX;
#endif  //CFG_HAL_SUPPORT_MBSSID
    pHalFunc->InitVAPIMRHandler     = InitVAPIMR88XX;
    pHalFunc->InitLLT_TableHandler  = InitLLT_Table88XX_V1;
#if CFG_HAL_HW_FILL_MACID
    pHalFunc->InitMACIDSearchHandler    = InitMACIDSearch88XX;            
    pHalFunc->CheckHWMACIDResultHandler = CheckHWMACIDResult88XX;            
#endif //CFG_HAL_HW_FILL_MACID

    //
    //Stop Related
    //
#if CFG_HAL_SUPPORT_MBSSID        
    pHalFunc->StopMBSSIDHandler     = StopMBSSID88XX;
#endif  //CFG_HAL_SUPPORT_MBSSID
    pHalFunc->StopHWHandler         = StopHW88XX;
    pHalFunc->StopSWHandler         = StopSW88XX;
    pHalFunc->DisableVXDAPHandler   = DisableVXDAP88XX;
    pHalFunc->ResetHWForSurpriseHandler     = ResetHWForSurprise8814AE;


    //
    //ISR Related
    //
    pHalFunc->InitIMRHandler                    = InitIMR88XX;
    pHalFunc->EnableIMRHandler                  = EnableIMR88XX;
    pHalFunc->InterruptRecognizedHandler        = InterruptRecognized88XX;
    pHalFunc->GetInterruptHandler               = GetInterrupt88XX;
    pHalFunc->AddInterruptMaskHandler           = AddInterruptMask88XX;
    pHalFunc->RemoveInterruptMaskHandler        = RemoveInterruptMask88XX;
    pHalFunc->DisableRxRelatedInterruptHandler  = DisableRxRelatedInterrupt88XX;
    pHalFunc->EnableRxRelatedInterruptHandler   = EnableRxRelatedInterrupt88XX;


    //
    //Tx Related
    //
    pHalFunc->PrepareTXBDHandler            = PrepareTXBD88XX;    
    pHalFunc->FillTxHwCtrlHandler           = FillTxHwCtrl88XX;
    pHalFunc->SyncSWTXBDHostIdxToHWHandler  = SyncSWTXBDHostIdxToHW88XX;
    pHalFunc->TxPollingHandler              = TxPolling88XX;
    pHalFunc->SigninBeaconTXBDHandler       = SigninBeaconTXBD88XX;
    pHalFunc->SetBeaconDownloadHandler      = SetBeaconDownload88XX;
    pHalFunc->FillBeaconDescHandler         = FillBeaconDesc88XX;
    pHalFunc->GetTxQueueHWIdxHandler        = GetTxQueueHWIdx88XX;
    pHalFunc->MappingTxQueueHandler         = MappingTxQueue88XX;
    pHalFunc->QueryTxConditionMatchHandler  = QueryTxConditionMatch88XX;
#if CFG_HAL_TX_SHORTCUT
//    pHalFunc->GetShortCutTxDescHandler      = GetShortCutTxDesc88XX;
//    pHalFunc->ReleaseShortCutTxDescHandler  = ReleaseShortCutTxDesc88XX;
    pHalFunc->GetShortCutTxBuffSizeHandler  = GetShortCutTxBuffSize88XX;
    pHalFunc->SetShortCutTxBuffSizeHandler  = SetShortCutTxBuffSize88XX;
    pHalFunc->CopyShortCutTxDescHandler     = CopyShortCutTxDesc88XX;
    pHalFunc->FillShortCutTxHwCtrlHandler   = FillShortCutTxHwCtrl88XX;    
    pHalFunc->ReleaseOnePacketHandler       = ReleaseOnePacket88XX;                  
#endif // CFG_HAL_TX_SHORTCUT

    //
    //Rx Related
    //
    pHalFunc->PrepareRXBDHandler            = PrepareRXBD88XX;
    pHalFunc->QueryRxDescHandler            = QueryRxDesc88XX;
    pHalFunc->UpdateRXBDInfoHandler         = UpdateRXBDInfo88XX;
    pHalFunc->UpdateRXBDHWIdxHandler        = UpdateRXBDHWIdx88XX;
    pHalFunc->UpdateRXBDHostIdxHandler      = UpdateRXBDHostIdx88XX;    

    //
    // General operation
    //
    pHalFunc->GetChipIDMIMOHandler          =   GetChipIDMIMO88XX;
    pHalFunc->SetHwRegHandler               =   SetHwReg88XX;
    pHalFunc->GetHwRegHandler               =   GetHwReg88XX;
    pHalFunc->SetMACIDSleepHandler          =   SetMACIDSleep88XX;
	pHalFunc->CheckHangHandler              =   CheckHang88XX;
    pHalFunc->GetMACIDQueueInTXPKTBUFHandler=   GetMACIDQueueInTXPKTBUF88XX;

    //
    // Timer Related
    //
    pHalFunc->Timer1SecHandler              =   Timer1Sec88XX;


    //
    // Security Related     
    //
    pHalFunc->CAMReadMACConfigHandler       =   CAMReadMACConfig88XX;
    pHalFunc->CAMEmptyEntryHandler          =   CAMEmptyEntry88XX;
    pHalFunc->CAMFindUsableHandler          =   CAMFindUsable88XX;
    pHalFunc->CAMProgramEntryHandler        =   CAMProgramEntry88XX;


    //
    // PHY/RF Related
    //
    pHalFunc->PHYSetCCKTxPowerHandler       = PHYSetCCKTxPower88XX_AC;
    pHalFunc->PHYSetOFDMTxPowerHandler      = PHYSetOFDMTxPower88XX_AC;
    pHalFunc->PHYSwBWModeHandler            = SwBWMode88XX_AC;
    pHalFunc->PHYUpdateBBRFValHandler       = UpdateBBRFVal88XX_AC;
    // TODO: 8814A Power Tracking should be done
    pHalFunc->TXPowerTrackingHandler        = TXPowerTracking_ThermalMeter_Tmp8814A;
    pHalFunc->PHYSSetRFRegHandler           = PHY_SetRFReg_88XX_AC;
    pHalFunc->PHYQueryRFRegHandler          = PHY_QueryRFReg_88XX_AC;
    pHalFunc->IsBBRegRangeHandler           = IsBBRegRange88XX_V1;


    //
    // Firmware CMD IO related
    //
    pHalData->H2CBufPtr88XX     = 0;
    pHalData->bFWReady          = _FALSE;
    // TODO: code below should be sync with new 3081 FW
    pHalFunc->FillH2CCmdHandler             = FillH2CCmd88XX;
    pHalFunc->UpdateHalRAMaskHandler        = UpdateHalRAMask8814A;
    pHalFunc->UpdateHalMSRRPTHandler        = UpdateHalMSRRPT88XX;
    pHalFunc->SetAPOffloadHandler           = HalGeneralDummy;
    pHalFunc->SetRsvdPageHandler	        = HalGeneralDummy;
    pHalFunc->GetRsvdPageLocHandler	        = HalGeneralDummy;
    pHalFunc->DownloadRsvdPageHandler	    = HalGeneralDummy;
    pHalFunc->C2HHandler                    = HalGeneralDummy;
    pHalFunc->C2HPacketHandler              = C2HPacket88XX;    
    pHalFunc->GetTxRPTHandler               = GetTxRPTBuf88XX;
    pHalFunc->SetTxRPTHandler               = SetTxRPTBuf88XX;    
#if CFG_HAL_HW_FILL_MACID
    pHalFunc->SetCRC5ToRPTBufferHandler     = SetCRC5ToRPTBuffer88XX;        
#endif //#if CFG_HAL_HW_FILL_MACID
    pHalFunc->DumpRxBDescTestHandler        = DumpRxBDesc88XX;
    pHalFunc->DumpTxBDescTestHandler        = DumpTxBDesc88XX;
    
    return  RT_STATUS_SUCCESS;    
}


void 
InitMAC8814AE(
    IN  HAL_PADAPTER Adapter
)
{














    
}

