//==================================================
//
//	This module is for BT related common code.
//
//		04/13/2011 created by Cosa
//
//==================================================

#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <osdep_intf.h>

#ifdef CONFIG_RTL8723B

//#include "../hal/BTCoexist/halbt_precomp.h"
#include "../hal/BTCoexist/HalBtCoexist.h"
#include "../hal/BTCoexist/HalBtcOutSrc.h"

u1Byte	testbuf[10];	// this is only for compile when BT_30_SUPPORT = 0

//#if(BT_30_SUPPORT == 1)
#ifdef CONFIG_BT_COEXIST

typedef struct _btcoexdbginfo
{
	u8 *info;
	u32 size; // buffer total size
	u32 len; // now used length
} BTCDBGINFO, *PBTCDBGINFO;

BTCDBGINFO GLBtcDbgInfo;

//static u1Byte	btDbgBuf[BT_TMP_BUF_SIZE];

static void DBG_BT_INFO_INIT(PBTCDBGINFO pinfo, u8 *pbuf, u32 size)
{
	if (NULL == pinfo) return;

	_rtw_memset(pinfo, 0, sizeof(BTCDBGINFO));

	if (pbuf && size) {
		pinfo->info = pbuf;
		pinfo->size = size;
	}
}

void DBG_BT_INFO(u8 *dbgmsg)
{
	PBTCDBGINFO pinfo;
	u32 msglen, buflen;
	u8 *pbuf;


	pinfo = &GLBtcDbgInfo;

	if (NULL == pinfo->info)
		return;

	msglen = strlen(dbgmsg);
	if (pinfo->len + msglen > pinfo->size)
		return;

	pbuf = pinfo->info + pinfo->len;
	_rtw_memcpy(pbuf, dbgmsg, msglen);
	pinfo->len += msglen;
}

//==================================================
//	extern function
//==================================================
//
// Make sure this function is called before enter IPS (rf not turn off yet) and
// after leave IPS(rf already turned on)
//
VOID
BT_IpsNotify(
	IN	PADAPTER	Adapter,
	IN	u1Byte		type
	)
{
	//PBT_CTRL_INFO	pBtCtrlInfo=&Adapter->MgntInfo.btCtrlInfo;
	PADAPTER		pDefaultAdapter=GetDefaultAdapter(Adapter);

	if(pDefaultAdapter != Adapter)
		return;

	if(!HALBT_IsBtExist(Adapter))
		return;

	//pBtCtrlInfo->notifyData.ipsType = type;
	BTDM_IpsNotify(Adapter,type);
	//PlatformScheduleWorkItem(&(pBtCtrlInfo->notifyWorkItem[NOTIFY_WIFI_IPS]));
}

VOID
BT_LpsNotify(
	IN	PADAPTER	Adapter,
	IN	u1Byte		type
	)
{
	//PBT_CTRL_INFO	pBtCtrlInfo=&Adapter->MgntInfo.btCtrlInfo;
	PADAPTER		pDefaultAdapter=GetDefaultAdapter(Adapter);

	if(pDefaultAdapter != Adapter)
		return;

	if(!HALBT_IsBtExist(Adapter))
		return;

	//pBtCtrlInfo->notifyData.lpsType = type;
	//PlatformScheduleWorkItem(&(pBtCtrlInfo->notifyWorkItem[NOTIFY_WIFI_LPS]));
	BTDM_LpsNotify(Adapter, type);
}

VOID
BT_ScanNotify(
	IN	PADAPTER 	Adapter,
	IN	u1Byte		scanType
	)
{
	//PBT_CTRL_INFO	pBtCtrlInfo=&Adapter->MgntInfo.btCtrlInfo;
	PADAPTER		pDefaultAdapter=GetDefaultAdapter(Adapter);

	if(pDefaultAdapter != Adapter)
		return;

	if(!HALBT_IsBtExist(Adapter))
		return;

#ifdef CONFIG_CONCURRENT_MODE
	if ((_FALSE == scanType) && (Adapter->pbuddy_adapter))
	{
		PADAPTER pbuddy = Adapter->pbuddy_adapter;
		if (check_fwstate(&pbuddy->mlmepriv, WIFI_SITE_MONITOR) == _TRUE)
			return;
	}
#endif

	//pBtCtrlInfo->notifyData.scanType = scanType;
	//PlatformScheduleWorkItem(&(pBtCtrlInfo->notifyWorkItem[NOTIFY_WIFI_SCAN]));
	BTDM_ScanNotify(Adapter,scanType);
}

VOID
BT_WiFiConnectNotify(
	IN	PADAPTER	Adapter,
	IN	u1Byte		actionType
	)
{
	// action :
	// _TRUE = wifi connection start
	// _FALSE = wifi connection finished
	//PBT_CTRL_INFO	pBtCtrlInfo=&Adapter->MgntInfo.btCtrlInfo;
	PADAPTER		pDefaultAdapter=GetDefaultAdapter(Adapter);

	if(pDefaultAdapter != Adapter)
		return;

	if(!HALBT_IsBtExist(Adapter))
		return;

#ifdef CONFIG_CONCURRENT_MODE
	if ((_FALSE == actionType) && Adapter->pbuddy_adapter)
	{
		PADAPTER pbuddy = Adapter->pbuddy_adapter;
		if (check_fwstate(&pbuddy->mlmepriv, WIFI_UNDER_LINKING) == _TRUE)
			return;
	}
#endif

	BTDM_ConnectNotify(Adapter,actionType);
}

VOID
BT_WifiMediaStatusNotify(
	IN	PADAPTER			Adapter,
	IN	RT_MEDIA_STATUS		mstatus
	)
{
	//PBT_CTRL_INFO	pBtCtrlInfo=&Adapter->MgntInfo.btCtrlInfo;
	PADAPTER		pDefaultAdapter=GetDefaultAdapter(Adapter);

	if(pDefaultAdapter != Adapter)
		return;

	if(!HALBT_IsBtExist(Adapter))
		return;

#ifdef CONFIG_CONCURRENT_MODE
	if ((0 == mstatus) && (Adapter->pbuddy_adapter))
	{
		PADAPTER pbuddy = Adapter->pbuddy_adapter;
		if (check_fwstate(&pbuddy->mlmepriv, WIFI_ASOC_STATE) == _TRUE)
			return;
	}
#endif

	//pBtCtrlInfo->notifyData.mediaStatus = (u1Byte)mstatus;
	//PlatformScheduleWorkItem(&(pBtCtrlInfo->notifyWorkItem[NOTIFY_WIFI_MEDIA_STATUS]));
	BTDM_MediaStatusNotify(Adapter, mstatus);
}
VOID
BT_SpecialPacketNotify(
	IN	PADAPTER	Adapter,
	IN	u1Byte		pktType
	)
{
	//PBT_CTRL_INFO	pBtCtrlInfo=&Adapter->MgntInfo.btCtrlInfo;
	PADAPTER		pDefaultAdapter=GetDefaultAdapter(Adapter);

	if(pDefaultAdapter != Adapter)
		return;

	if(!HALBT_IsBtExist(Adapter))
		return;

	BTDM_SpecialPacketNotify(Adapter,pktType);
	//pBtCtrlInfo->notifyData.pktType = pktType;
	//if(pBtCtrlInfo->notifyData.pktType == PACKET_DHCP)
	//{
		//PlatformScheduleWorkItem(&(pBtCtrlInfo->notifyWorkItem[NOTIFY_WIFI_SPECIAL_PACKET]));
	//}
}

VOID
BT_IQKNotify(
	IN	PADAPTER	padapter,
	IN	u8			state)
{
	BTDM_IQKNotify(padapter, state);
}

VOID
BT_BtInfoNotify(
	IN	PADAPTER	Adapter,
	IN	pu1Byte		tmpBuf,
	IN	u1Byte		length
	)
{
	//PBT_CTRL_INFO	pBtCtrlInfo=&Adapter->MgntInfo.btCtrlInfo;
	PADAPTER		pDefaultAdapter=GetDefaultAdapter(Adapter);

	if(pDefaultAdapter != Adapter)
		return;

	if(!HALBT_IsBtExist(Adapter))
		return;

	if(length <= BT_CTRL_BTINFO_BUF)
	{
		//pBtCtrlInfo->notifyData.btInfoLen = length;
		//PlatformMoveMemory(&pBtCtrlInfo->notifyData.btInfo[0], tmpBuf, length);
		//PlatformScheduleWorkItem(&(pBtCtrlInfo->notifyWorkItem[NOTIFY_BT_INFO]));
		BTDM_BtInfoNotify(Adapter, tmpBuf, length);
	}
}

VOID
BT_RfStatusNotify(
	IN	PADAPTER				Adapter,
	IN	rt_rf_power_state		StateToSet,
	IN	u4Byte	ChangeSource
	)
{
}

VOID
BT_StackOperationNotify(
	IN	PADAPTER	Adapter,
	IN	u1Byte		opType
	)
{
}

VOID
BT_HaltNotify(
	IN	PADAPTER	Adapter
	)
{
	EXhalbtcoutsrc_HaltNotify(&GLBtCoexist);
}

void BT_SwitchGntBt(PADAPTER padapter)
{
	EXhalbtcoutsrc_SwitchGntBt(&GLBtCoexist);
}

VOID
BT_PnpNotify(
	IN	PADAPTER	Adapter,
	IN	u1Byte		pnpState
	)
{
	if (pnpState)
		EXhalbtcoutsrc_PnpNotify(&GLBtCoexist, BTC_WIFI_PNP_SLEEP);
	else
		EXhalbtcoutsrc_PnpNotify(&GLBtCoexist, BTC_WIFI_PNP_WAKE_UP);
}

VOID
BT_DownloadFwNotify(
	IN	PADAPTER	Adapter,
	IN	u1Byte		Begain
	)
{
	if (Begain)
		EXhalbtcoutsrc_DownloadFwNotify(&GLBtCoexist, BTC_WIFI_DLFW_BEGAIN);
	else
		EXhalbtcoutsrc_DownloadFwNotify(&GLBtCoexist, BTC_WIFI_DLFW_END);
}

BOOLEAN
BT_AnyClientConnectToAp(
	IN	PADAPTER	Adapter
	)
{
	//PMGNT_INFO		pMgntInfo = &Adapter->MgntInfo;
	//PADAPTER		pExtAdapter=NULL, pDefaultAdapter=Adapter;
	//PMGNT_INFO		pExtMgntInfo;
	BOOLEAN			bCurStaAssociated = _FALSE;
	struct sta_priv *pstapriv = &Adapter->stapriv;

	// client is connected to AP mode(vwifi or softAp)
	if((pstapriv->asoc_sta_count > 2))
		bCurStaAssociated = _TRUE;
	return bCurStaAssociated;
}

BOOLEAN
BT_Operation(
	IN	PADAPTER	Adapter
	)
{
	return _FALSE;
}

VOID
BT_SetManualControl(
	IN	PADAPTER	Adapter,
	IN	BOOLEAN		Flag
	)
{
	BTDM_SetManualControl(Flag);
}

VOID
BT_InitializeVariables(
	IN	PADAPTER	Adapter
	)
{
	BTDM_InitlizeVariables(Adapter);
}

//===============================================================
//===============================================================
VOID
BT_InitHalVars(
	IN	PADAPTER			Adapter
	)
{
	u1Byte	antNum = 2;

	// HALBT_InitHalVars() must be called first
	BTDM_InitHalVars(Adapter);

	// called after HALBT_InitHalVars()
	BTDM_SetBtExist(Adapter);
	BTDM_SetBtChipType(Adapter);
	antNum = HALBT_GetPgAntNum(Adapter);
	BTDM_SetBtCoexAntNum(Adapter, BT_COEX_ANT_TYPE_PG, antNum);
}

BOOLEAN
BT_IsBtExist(
	IN	PADAPTER	Adapter
	)
{
	return HALBT_IsBtExist(Adapter);
}
u1Byte
BT_BtChipType(
	IN	PADAPTER	Adapter
	)
{
	return HALBT_BTChipType(Adapter);
}
VOID
BT_InitHwConfig(
	IN	PADAPTER	Adapter
	)
{
	if(!BT_IsBtExist(Adapter))
		return;
	//HALBT_InitHwConfig(Adapter);
	EXhalbtcoutsrc_InitHwConfig(&GLBtCoexist);
	EXhalbtcoutsrc_InitCoexDm(&GLBtCoexist);
}

u1Byte
BT_GetPgAntNum(
	IN	PADAPTER	Adapter
	)
{
	return HALBT_GetPgAntNum(Adapter);
}

BOOLEAN
BT_SupportRfStatusNotify(
	IN	PADAPTER	Adapter
	)
{
	return HALBT_SupportRfStatusNotify(Adapter);
}
VOID
BT_SetRtsCtsNoLenLimit(
	IN	PADAPTER	Adapter
	)
{
	HALBT_SetRtsCtsNoLenLimit(Adapter);
}

VOID
BT_ActUpdateRaMask(
	IN	PADAPTER	Adapter,
	IN	u1Byte		macId
	)
{
	rtw_hal_update_ra_mask(Adapter, 0, DM_RATR_STA_HIGH);
}
//=======================================
//	BTDM module
//=======================================
BOOLEAN
BT_IsP2pGoConnected(
	IN	PADAPTER	pAdapter
	)
{
#if 0
	PP2P_INFO pP2PInfo=NULL;

	if(!P2P_ENABLED(GET_P2P_INFO(pAdapter)))
			return FALSE;

	pP2PInfo = GET_P2P_INFO(pAdapter);

	if(pAdapter->MgntInfo.bMediaConnect && P2P_GO == pP2PInfo->Role)
		return TRUE;
	else
#endif
		return FALSE;
}

BOOLEAN
BT_IsP2pClientConnected(
	IN	PADAPTER	pAdapter
	)
{
#if 0
	PP2P_INFO pP2PInfo=NULL;

	if(!P2P_ENABLED(GET_P2P_INFO(pAdapter)))
			return FALSE;

	pP2PInfo = GET_P2P_INFO(pAdapter);

	if(pAdapter->MgntInfo.bMediaConnect && P2P_CLIENT == pP2PInfo->Role)
		return TRUE;
	else
#endif
		return FALSE;
}

BOOLEAN
BT_IsBtBusy(
	IN	PADAPTER	Adapter
	)
{
	return BTDM_IsBtBusy(Adapter);
}
BOOLEAN
BT_IsLimitedDig(
	IN	PADAPTER	Adapter
	)
{
	return BTDM_IsLimitedDig(Adapter);
}
VOID
BT_CoexistMechanism(
	IN	PADAPTER	Adapter
	)
{
#ifdef CONFIG_CONCURRENT_MODE
	if (Adapter->adapter_type != PRIMARY_ADAPTER)
		return;
#endif //CONFIG_CONCURRENT_MODE

	BTDM_Coexist(Adapter);
}
s4Byte
BT_GetHsWifiRssi(
	IN		PADAPTER	Adapter
	)
{
	return BTDM_GetHsWifiRssi(Adapter);
}

BOOLEAN
BT_IsDisableEdcaTurbo(
	IN	PADAPTER	Adapter
	)
{
	return BTDM_IsDisableEdcaTurbo(Adapter);
}
u1Byte
BT_AdjustRssiForCoex(
	IN	PADAPTER	Adapter
	)
{
	return BTDM_AdjustRssiForCoex(Adapter);
}
VOID
BT_SetBtCoexAntNum(
	IN	PADAPTER	Adapter,
	IN	u1Byte		type,
	IN	u1Byte		antNum
	)
{
	BTDM_SetBtCoexAntNum(Adapter, type, antNum);
}

//=======================================
//	BTCoex module
//=======================================
VOID
BT_UpdateProfileInfo(
	)
{
	EXhalbtcoutsrc_StackUpdateProfileInfo();
}
VOID
BT_UpdateMinBtRssi(
	IN	s1Byte	btRssi
	)
{
	EXhalbtcoutsrc_UpdateMinBtRssi(btRssi);
}
VOID
BT_SetHciVersion(
	IN	u2Byte	hciVersion
	)
{
	EXhalbtcoutsrc_SetHciVersion(hciVersion);
}
VOID
BT_SetBtPatchVersion(
	IN	u2Byte	btHciVersion,
	IN	u2Byte	btPatchVersion
	)
{
	EXhalbtcoutsrc_SetBtPatchVersion(btHciVersion, btPatchVersion);
}
//=======================================
//	Other module
//=======================================


BOOLEAN
BT_IsBtControlLps(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return FALSE;

	if(GLBtCoexist.btInfo.bBtDisabled)
		return FALSE;

	if(GLBtCoexist.btInfo.bBtCtrlLps)
		return TRUE;
	else
		return FALSE;
}

BOOLEAN
BT_IsBtLpsOn(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return FALSE;

	if(GLBtCoexist.btInfo.bBtDisabled)
		return FALSE;

	if(GLBtCoexist.btInfo.bBtLpsOn)
		return TRUE;
	else
		return FALSE;
}

BOOLEAN
BT_IsLowPwrDisable(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return FALSE;

	if(GLBtCoexist.btInfo.bBtDisabled)
		return FALSE;

	if(GLBtCoexist.btInfo.bBtDisableLowPwr)
		return TRUE;
	else
		return FALSE;
}

BOOLEAN
BT_1Ant(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return _FALSE;

	if(GLBtCoexist.boardInfo.btdmAntNum == 1)
		return _TRUE;
	else
		return _FALSE;
}

u1Byte
BT_CoexLpsVal(
	IN	PADAPTER	Adapter
	)
{
	return GLBtCoexist.btInfo.lpsVal;
}

u1Byte
BT_CoexRpwmVal(
	IN	PADAPTER	Adapter
	)
{
	return GLBtCoexist.btInfo.rpwmVal;
}

VOID
BT_RecordPwrMode(
	IN	PADAPTER	Adapter,
	IN	pu1Byte		pCmdBuf,
	IN	u1Byte		cmdLen
	)
{
	BTC_PRINT(BTC_MSG_ALGORITHM, ALGO_TRACE_FW_EXEC, ("[BTCoex], FW write pwrModeCmd=0x%04x%08x\n",
		pCmdBuf[0]<<8|pCmdBuf[1],
		pCmdBuf[2]<<24|pCmdBuf[3]<<16|pCmdBuf[4]<<8|pCmdBuf[5]));

	_rtw_memcpy(&GLBtCoexist.pwrModeVal[0], pCmdBuf, cmdLen);
}

BOOLEAN
BT_IsBtCoexManualControl(
	IN	PADAPTER	Adapter
	)
{
	if(GLBtCoexist.bManualControl)
		return _TRUE;
	else
		return _FALSE;
}

BOOLEAN
BT_ForceExecPwrCmd(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return FALSE;

	if(GLBtCoexist.btInfo.bBtDisabled)
		return FALSE;

	if(GLBtCoexist.btInfo.bBtCtrlLps)
	{
		if(GLBtCoexist.btInfo.bBtLpsOn)
		{
			if(GLBtCoexist.btInfo.lpsVal != GLBtCoexist.pwrModeVal[5])
			{
				return TRUE;
			}
			if(GLBtCoexist.btInfo.rpwmVal != GLBtCoexist.pwrModeVal[4])
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOLEAN
BT_IsBtDisabled(
	IN	PADAPTER	Adapter
	)
{
	if(BTDM_IsBtDisabled(Adapter))
		return _TRUE;
	else
		return _FALSE;
}

BOOLEAN
BT_BtCtrlAggregateBufSize(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return FALSE;

	if(GLBtCoexist.btInfo.bBtCtrlAggBufSize)
		return TRUE;
	else
		return FALSE;
}

u1Byte
BT_GetAdjustAggregateBufSize(
	IN	PADAPTER	Adapter
	)
{
	return GLBtCoexist.btInfo.aggBufSize;
}

void BT_LowWiFiTrafficNotify(PADAPTER padapter)
{
	//BTC_PRINT(BTC_MSG_ALGORITHM, ALGO_TRACE_FW_EXEC, ("BT_LowWiFiTrafficNotify()\n"));
	return;
}

u4Byte
BT_GetRaMask(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return 0;
	if(GLBtCoexist.btInfo.bBtDisabled)
		return 0;
	if(GLBtCoexist.boardInfo.btdmAntNum != 1)
		return 0;

	return GLBtCoexist.btInfo.raMask;
}

BOOLEAN
BT_IncreaseScanDeviceNum(
	IN	PADAPTER	Adapter
	)
{
	if(!HALBT_IsBtExist(Adapter))
		return FALSE;

	if(GLBtCoexist.btInfo.bIncreaseScanDevNum)
		return TRUE;
	else
		return FALSE;
}

void hal_btcoex_DisplayBtCoexInfo(PADAPTER padapter, u8 *pbuf, u32 bufsize)
{
	PBTCDBGINFO pinfo;


	pinfo = &GLBtcDbgInfo;
	DBG_BT_INFO_INIT(pinfo, pbuf, bufsize);
	//EXhalbtcoutsrc_DisplayBtCoexInfo(&GLBtCoexist);
	BTDM_DisplayBtCoexInfo(padapter);
	DBG_BT_INFO_INIT(pinfo, NULL, 0);
}

void hal_btcoex_SetDBG(PADAPTER padapter, u32 *pDbgModule)
{
	u32 i;


	if (NULL == pDbgModule)
		return;

	for (i=0; i<BTC_MSG_MAX; i++)
		GLBtcDbgType[i] = pDbgModule[i];
}

u32 hal_btcoex_GetDBG(PADAPTER padapter, u8 *pStrBuf, u32 bufSize)
{
	s32 count;
	u8 *pstr;
	u32 leftSize;


	if ((NULL == pStrBuf) || (0 == bufSize))
		return 0;

	count = 0;
	pstr = pStrBuf;
	leftSize = bufSize;
//	DBG_871X(FUNC_ADPT_FMT ": bufsize=%d\n", FUNC_ADPT_ARG(padapter), bufSize);

	count = rtw_sprintf(pstr, leftSize, "#define DBG\t%d\n", DBG);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

	count = rtw_sprintf(pstr, leftSize, "BTCOEX Debug Setting:\n");
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

	count = rtw_sprintf(pstr, leftSize,
		"INTERFACE / ALGORITHM: 0x%08X / 0x%08X\n\n",
		GLBtcDbgType[BTC_MSG_INTERFACE],
		GLBtcDbgType[BTC_MSG_ALGORITHM]);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

	count = rtw_sprintf(pstr, leftSize, "INTERFACE Debug Setting Definition:\n");
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[0]=%d for INTF_INIT\n",
		GLBtcDbgType[BTC_MSG_INTERFACE]&INTF_INIT?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[2]=%d for INTF_NOTIFY\n\n",
		GLBtcDbgType[BTC_MSG_INTERFACE]&INTF_NOTIFY?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

	count = rtw_sprintf(pstr, leftSize, "ALGORITHM Debug Setting Definition:\n");
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[0]=%d for BT_RSSI_STATE\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_BT_RSSI_STATE?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[1]=%d for WIFI_RSSI_STATE\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_WIFI_RSSI_STATE?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[2]=%d for BT_MONITOR\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_BT_MONITOR?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[3]=%d for TRACE\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_TRACE?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[4]=%d for TRACE_FW\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_TRACE_FW?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[5]=%d for TRACE_FW_DETAIL\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_TRACE_FW_DETAIL?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[6]=%d for TRACE_FW_EXEC\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_TRACE_FW_EXEC?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[7]=%d for TRACE_SW\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_TRACE_SW?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[8]=%d for TRACE_SW_DETAIL\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_TRACE_SW_DETAIL?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;
	count = rtw_sprintf(pstr, leftSize, "\tbit[9]=%d for TRACE_SW_EXEC\n",
		GLBtcDbgType[BTC_MSG_ALGORITHM]&ALGO_TRACE_SW_EXEC?1:0);
	if ((count < 0) || (count >= leftSize))
		goto exit;
	pstr += count;
	leftSize -= count;

exit:
	count = pstr - pStrBuf;
//	DBG_871X(FUNC_ADPT_FMT ": usedsize=%d\n", FUNC_ADPT_ARG(padapter), count);

	return count;
}

void BT_DisplayBtCoexInfo(PADAPTER padapter, u8 *pbuf, u32 bufsize)
{
	hal_btcoex_DisplayBtCoexInfo(padapter, pbuf, bufsize);
}

void BT_SetDBG(PADAPTER padapter, u32 *pDbgModule)
{
	hal_btcoex_SetDBG(padapter, pDbgModule);
}

u32 BT_GetDBG(PADAPTER padapter, u8 *pStrBuf, u32 bufSize)
{
	return hal_btcoex_GetDBG(padapter, pStrBuf, bufSize);
}

#endif
#endif
