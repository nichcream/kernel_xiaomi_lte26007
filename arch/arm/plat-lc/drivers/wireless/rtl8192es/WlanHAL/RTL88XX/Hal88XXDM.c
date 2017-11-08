/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXDM.c

Abstract:
	Defined RTL88XX HAL common Function

Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2013-08-19 Filen            Create.
--*/

#include "../HalPrecomp.h"

#if CFG_HAL_MACDM

typedef enum _MACDM_STATE_CHANGE_{
    MACDM_STATE_CHANGE_NO = 0,
    MACDM_STATE_CHANGE_UP,
    MACDM_STATE_CHANGE_DOWN
} MACDM_STATE_CHANGE, *PMACDM_STATE_CHANGE;


static
RSSI_LVL_DM_88XX
TranslateRSSI88XX(
    u4Byte rssi
)
{
    // TODO:
    return RSSI_LVL_LOW;
}


static
BOOLEAN
LoadMACDMTable(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          MACDM_State,
    IN  u4Byte          rssi_lvl
)
{
    PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);
    u4Byte                      offset, value, tbl_idx = 0;

    while(1) {

        offset  = pHalData->MACDM_Table[MACDM_State][rssi_lvl][tbl_idx].offset;
        value   = pHalData->MACDM_Table[MACDM_State][rssi_lvl][tbl_idx].value;
    
        if (offset == 0xFFFF) {
            break;
        }

        HAL_RTL_W32(offset, value);
        tbl_idx++;

        // TODO: timeout machinsm, to avoid hanging here
    }

    return _TRUE;
}


VOID
InitMACDM88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    //PHAL_INTERFACE              pHalFunc = GET_HAL_INTERFACE(Adapter);
    PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);
    pu1Byte                     pRegFileStart;
    u4Byte                      RegFileLen;
    u4Byte                      stateThrs[MACDM_TP_THRS_MAX_NUM*RSSI_LVL_MAX_NUM+1]; //1: EOF
    u4Byte                      thrs_idx,rssi_idx;
    

    pHalData->MACDM_State = MACDM_TP_STATE_DEFAULT;

    // TODO: should load from mib
    pHalData->MACDM_Mode_Sel = MACDM_MODE_MAX_TP;

    pHalData->MACDM_preRssiLvl = RSSI_LVL_LOW;

    //Load Raise/Fall state criteria from the para.c (translation from txt file)
    HAL_memset(stateThrs, 0, sizeof(stateThrs));
    HAL_memset(pHalData->MACDM_stateThrs, 0, sizeof(pHalData->MACDM_stateThrs));

    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_CRITERIA_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_CRITERIA_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToOneParaTable(pRegFileStart, 
                        RegFileLen, 
                        (pu1Byte)stateThrs,
                        (MACDM_TP_THRS_MAX_NUM*RSSI_LVL_MAX_NUM+1));

    //one-way arrary translate to two-way arrary
    for(thrs_idx=0; thrs_idx<MACDM_TP_THRS_MAX_NUM; thrs_idx++) {
        for (rssi_idx=0; rssi_idx<RSSI_LVL_MAX_NUM; rssi_idx++){
            pHalData->MACDM_stateThrs[thrs_idx][rssi_idx] = stateThrs[thrs_idx*RSSI_LVL_MAX_NUM + rssi_idx];
        }
    }

#if 0   //Filen: for verification
    RT_TRACE(COMP_INIT, DBG_LOUD, ("pRegFileStart(0x%p), RegFileLen(0x%x)\n", pRegFileStart, RegFileLen) );
    RT_TRACE(COMP_INIT, DBG_LOUD, ("0: value(0x%x)\n", pHalData->MACDM_stateThrs[0]));
    RT_TRACE(COMP_INIT, DBG_LOUD, ("1: value(0x%x)\n", pHalData->MACDM_stateThrs[1]));
    RT_TRACE(COMP_INIT, DBG_LOUD, ("2: value(0x%x)\n", pHalData->MACDM_stateThrs[2]));
    RT_TRACE(COMP_INIT, DBG_LOUD, ("3: value(0x%x)\n", pHalData->MACDM_stateThrs[3]));
    RT_TRACE(COMP_INIT, DBG_LOUD, ("4: value(0x%x)\n", pHalData->MACDM_stateThrs[4]));
#endif

    //Load Table parameter from the para.c (translation from txt file)
    HAL_memset(pHalData->MACDM_Table, 0, sizeof(pHalData->MACDM_Table));

    //(default, low)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_DEF_LOW_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_DEF_LOW_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen, 
                        pHalData->MACDM_Table[MACDM_TP_STATE_DEFAULT][RSSI_LVL_LOW],
                        MAX_MACDM_REG_NUM);

    //(default, normal)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_DEF_NORMAL_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_DEF_NORMAL_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen, 
                        pHalData->MACDM_Table[MACDM_TP_STATE_DEFAULT][RSSI_LVL_NORMAL],
                        MAX_MACDM_REG_NUM);

    //(default, high)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_DEF_HIGH_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_DEF_HIGH_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen,
                        pHalData->MACDM_Table[MACDM_TP_STATE_DEFAULT][RSSI_LVL_HIGH],
                        MAX_MACDM_REG_NUM);

    //(general, low)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_GEN_LOW_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_GEN_LOW_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen, 
                        pHalData->MACDM_Table[MACDM_TP_STATE_GENERAL][RSSI_LVL_LOW],
                        MAX_MACDM_REG_NUM);

    //(general, normal)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_GEN_NORMAL_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_GEN_NORMAL_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen, 
                        pHalData->MACDM_Table[MACDM_TP_STATE_GENERAL][RSSI_LVL_NORMAL],
                        MAX_MACDM_REG_NUM);

    //(general, high)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_GEN_HIGH_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_GEN_HIGH_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen,
                        pHalData->MACDM_Table[MACDM_TP_STATE_GENERAL][RSSI_LVL_HIGH],
                        MAX_MACDM_REG_NUM);

    //(txop, low)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_TXOP_LOW_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_TXOP_LOW_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen, 
                        pHalData->MACDM_Table[MACDM_TP_STATE_TXOP][RSSI_LVL_LOW],
                        MAX_MACDM_REG_NUM);

    //(general, normal)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_TXOP_NORMAL_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_TXOP_NORMAL_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen, 
                        pHalData->MACDM_Table[MACDM_TP_STATE_TXOP][RSSI_LVL_NORMAL],
                        MAX_MACDM_REG_NUM);

    //(general, high)
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_TXOP_HIGH_START, (pu1Byte)&pRegFileStart);
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_MACDM_TXOP_HIGH_SIZE, (pu1Byte)&RegFileLen);
    LoadFileToIORegTable(pRegFileStart, 
                            RegFileLen,
                        pHalData->MACDM_Table[MACDM_TP_STATE_TXOP][RSSI_LVL_HIGH],
                        MAX_MACDM_REG_NUM);


    #if 0   //Filen: for verification
    RT_TRACE(COMP_INIT, DBG_LOUD, ("pRegFileStart(0x%p), RegFileLen(0x%x)\n", pRegFileStart, RegFileLen) );
    RT_TRACE(COMP_INIT, DBG_LOUD, ("0: offset(0x%x), value(0x%x)\n", 
                                    pHalData->MACDM_Table[MACDM_TP_STATE_DEFAULT][RSSI_LVL_LOW][0].offset,
                                    pHalData->MACDM_Table[MACDM_TP_STATE_DEFAULT][RSSI_LVL_LOW][0].value)
                                    );

    RT_TRACE(COMP_INIT, DBG_LOUD, ("1: offset(0x%x), value(0x%x)\n", 
                                    pHalData->MACDM_Table[MACDM_TP_STATE_DEFAULT][RSSI_LVL_LOW][1].offset,
                                    pHalData->MACDM_Table[MACDM_TP_STATE_DEFAULT][RSSI_LVL_LOW][1].value)
                                    );
    #endif

    //Load MACDM table by initial value
    LoadMACDMTable(Adapter, pHalData->MACDM_State, pHalData->MACDM_preRssiLvl);
}


static
MACDM_STATE_CHANGE
CheckMACDMThrs(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          MACDM_State,
    IN  u4Byte          TP,
    IN  u4Byte          rssi_lvl
)
{
    PHAL_DATA_TYPE              pHalData    = _GET_HAL_DATA(Adapter);

    switch(MACDM_State) {
        case MACDM_TP_STATE_DEFAULT:
            if (TP > pHalData->MACDM_stateThrs[MACDM_TP_THRS_DEF_TO_GEN][rssi_lvl]) {
                return MACDM_STATE_CHANGE_UP;
            }
            break;
        case MACDM_TP_STATE_GENERAL:
            if (TP < pHalData->MACDM_stateThrs[MACDM_TP_THRS_GEN_TO_DEF][rssi_lvl]) {
                return MACDM_STATE_CHANGE_DOWN;
            }
            else if (TP > pHalData->MACDM_stateThrs[MACDM_TP_THRS_GEN_TO_TXOP][rssi_lvl]) {
                return MACDM_STATE_CHANGE_UP;
            }
            break;
        case MACDM_TP_STATE_TXOP:
            if (TP < pHalData->MACDM_stateThrs[MACDM_TP_THRS_TXOP_TO_GEN][rssi_lvl]) {
                return MACDM_STATE_CHANGE_DOWN;
            }
            break;
        default:
            break;
    }

    return MACDM_STATE_CHANGE_NO;
}


static
BOOLEAN
MACDM_Core88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          TP,         // Mbps
    IN  u4Byte          rssi,
    IN  u4Byte          CurTxRate  //Data Rate Index
)
{
    PHAL_DATA_TYPE              pHalData    = _GET_HAL_DATA(Adapter);
    u4Byte                      rssi_lvl    = TranslateRSSI88XX(rssi);
    BOOLEAN                     StateChange = _FALSE;
    MACDM_STATE_CHANGE          stateChange;

    stateChange = CheckMACDMThrs(Adapter, pHalData->MACDM_State, TP, rssi_lvl);
    
    switch(pHalData->MACDM_State) {
        case MACDM_TP_STATE_DEFAULT:
            if (MACDM_STATE_CHANGE_UP == stateChange) {
                pHalData->MACDM_State = MACDM_TP_STATE_GENERAL;
                LoadMACDMTable(Adapter, MACDM_TP_STATE_GENERAL, rssi_lvl);
                RT_TRACE_F(COMP_DBG, DBG_LOUD, ("state(%d -> %d)\n", 
                                                MACDM_TP_STATE_DEFAULT, 
                                                pHalData->MACDM_State));
                StateChange = _TRUE;
            }
            else if (MACDM_STATE_CHANGE_NO == stateChange) {
                if (pHalData->MACDM_preRssiLvl != rssi_lvl) {
                    LoadMACDMTable(Adapter, MACDM_TP_STATE_DEFAULT, rssi_lvl);
                }
            }
            else {
                //Error, impossible
            }
            break;

        case MACDM_TP_STATE_GENERAL:

            if (MACDM_STATE_CHANGE_UP == stateChange) {
                pHalData->MACDM_State = MACDM_TP_STATE_TXOP;
                LoadMACDMTable(Adapter, MACDM_TP_STATE_TXOP, rssi_lvl);
                RT_TRACE_F(COMP_DBG, DBG_LOUD, ("state(%d -> %d)\n", 
                                                MACDM_TP_STATE_GENERAL, 
                                                pHalData->MACDM_State));
                StateChange = _TRUE;
            }
            else if (MACDM_STATE_CHANGE_DOWN == stateChange) {
                pHalData->MACDM_State = MACDM_TP_STATE_DEFAULT;
                LoadMACDMTable(Adapter, MACDM_TP_STATE_DEFAULT, rssi_lvl);
                RT_TRACE_F(COMP_DBG, DBG_LOUD, ("state(%d -> %d)\n", 
                                                MACDM_TP_STATE_GENERAL, 
                                                pHalData->MACDM_State));
                StateChange = _TRUE;
            }
            else {
                //MACDM_STATE_CHANGE_NO
                if (pHalData->MACDM_preRssiLvl != rssi_lvl) {
                    LoadMACDMTable(Adapter, MACDM_TP_STATE_GENERAL, rssi_lvl);
                }
            }
            break;

        case MACDM_TP_STATE_TXOP:
            if (MACDM_STATE_CHANGE_DOWN == stateChange) {
                pHalData->MACDM_State = MACDM_TP_STATE_GENERAL;
                LoadMACDMTable(Adapter, MACDM_TP_STATE_GENERAL, rssi_lvl);
                RT_TRACE_F(COMP_DBG, DBG_LOUD, ("state(%d -> %d)\n", 
                                                MACDM_TP_STATE_TXOP, 
                                                pHalData->MACDM_State));
                StateChange = _TRUE;
            }
            else if (MACDM_STATE_CHANGE_NO == stateChange) {
                if (pHalData->MACDM_preRssiLvl != rssi_lvl) {
                    LoadMACDMTable(Adapter, MACDM_TP_STATE_TXOP, rssi_lvl);
                }
            }
            else {
                //Error, impossible
            }

            break;

        default:
            //impossible
            break;
    };

    //Record last one RSSI Level
    pHalData->MACDM_preRssiLvl = rssi_lvl;

    return StateChange;
}


VOID
Timer1SecDM88XX(
    IN  HAL_PADAPTER Adapter
)
{
	HAL_DATA_TYPE	    *pHalData   = _GET_HAL_DATA(Adapter);
    u4Byte              idx         = 0;
    HAL_PSTAINFO        pEntry      = findNextSTA(Adapter, &idx);

    //RT_TRACE_F(COMP_DBG, DBG_LOUD, ("mode(%d)\n", pHalData->MACDM_Mode_Sel));

    switch(pHalData->MACDM_Mode_Sel) {
        case MACDM_MODE_MAX_TP:
        {
            u4Byte              tmpTotal_tp     = 0;
            u4Byte              highest_tp      = 0;
            HAL_PSTAINFO        pstat_highest   = NULL;
            
            while(pEntry) {
                if(pEntry && pEntry->expire_to) {
                    tmpTotal_tp = (pEntry->tx_avarage /*+ pEntry->rx_avarage*/)>>17; //unit: bytes -> Mb

                    if (tmpTotal_tp > highest_tp) {
            			highest_tp      = tmpTotal_tp;
            			pstat_highest   = pEntry;
            		}
                }
                
                pEntry = findNextSTA(Adapter, &idx);
            };

            // TODO: current_tx_rate is not correct, we should get this value from FW
            // TODO: transform current_tx_rate format
            if (pstat_highest) {
                MACDM_Core88XX(Adapter, 
                                highest_tp, 
                                pstat_highest->rssi, 
                                pstat_highest->current_tx_rate
                                );
            }
        }
            break;

        case MACDM_MODE_AVERAGE:
        {
            u4Byte              tmpTotal_tp     = 0, Total_tp = 0, average_tp  = 0;
            u4Byte              LinkedStaNum    = 0;
            u4Byte              Total_rssi      = 0, average_rssi = 0; 
            u4Byte              Min_CurTxRate   = 0xFFFFFFFF;

            while(pEntry) {

                if(pEntry && pEntry->expire_to) {

                    LinkedStaNum++;
                    
                    //accumulate throughput
                    tmpTotal_tp = (pEntry->tx_avarage /*+ pEntry->rx_avarage*/)>>17;//unit: bytes -> Mb
                    Total_tp    += tmpTotal_tp;

                    //accumulate RSSI
                    Total_rssi += pEntry->rssi;

                    //find lowest rate of all linked sta
                    if (pEntry->current_tx_rate < Min_CurTxRate) {
                        Min_CurTxRate = pEntry->current_tx_rate;
                    }
                }
                
                pEntry = findNextSTA(Adapter, &idx);
            };

            if (LinkedStaNum != 0) {
                average_tp      = Total_tp / LinkedStaNum;
                average_rssi    = Total_rssi / LinkedStaNum;

                // TODO: current_tx_rate is not correct, we should get this value from FW
                // TODO: transform current_tx_rate format
                MACDM_Core88XX(Adapter, 
                                average_tp, 
                                average_rssi, 
                                Min_CurTxRate
                                );
            }
        }
            break;

        case MACDM_MODE_MIN_TP:
        {
            u4Byte              tmpTotal_tp     = 0;
            u4Byte              lowest_tp       = 0xffffffff;
            HAL_PSTAINFO        pstat_lowest    = NULL;
            
            while(pEntry) {
                if(pEntry && pEntry->expire_to) {
                    tmpTotal_tp = (pEntry->tx_avarage /*+ pEntry->rx_avarage*/)>>17; //unit: bytes -> Mb

                    //find lowest sta
                    if (tmpTotal_tp < lowest_tp) {
            			lowest_tp       = tmpTotal_tp;
            			pstat_lowest    = pEntry;
            		}
                }
                
                pEntry = findNextSTA(Adapter, &idx);
            };

            // TODO: current_tx_rate is not correct, we should get this value from FW
            // TODO: transform current_tx_rate format
            if (pstat_lowest) {
                MACDM_Core88XX(Adapter, 
                                lowest_tp, 
                                pstat_lowest->rssi, 
                                pstat_lowest->current_tx_rate
                                );
            }
        }
            break;

        case MACDM_MODE_STOP:
        default:
            break;
    }

}

#endif //#if CFG_HAL_MACDM

