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
#include <drv_conf.h>
#include <osdep_service.h>
#include <drv_types.h>
#include <rtw_byteorder.h>

#include <hal_intf.h>
#include "HalVerDef.h"
#include <hal_com.h>
#include "../hal/OUTSRC/odm_precomp.h"


#define _HAL_INIT_C_

void dump_chip_info(HAL_VERSION	ChipVersion)
{
	if(IS_81XXC(ChipVersion)){
		DBG_871X("Chip Version Info: %s_",IS_92C_SERIAL(ChipVersion)?"CHIP_8192C":"CHIP_8188C");
	}
	else if(IS_92D(ChipVersion)){
		DBG_871X("Chip Version Info: CHIP_8192D_");
	}
	else if(IS_8723_SERIES(ChipVersion)){
		DBG_871X("Chip Version Info: CHIP_8723A_");
	}
	else if(IS_8188E(ChipVersion)){
		DBG_871X("Chip Version Info: CHIP_8188E_");
	}

	DBG_871X("%s_",IS_NORMAL_CHIP(ChipVersion)?"Normal_Chip":"Test_Chip");
	DBG_871X("%s_",IS_CHIP_VENDOR_TSMC(ChipVersion)?"TSMC":(IS_CHIP_VENDOR_SMIC(ChipVersion)?"SMIC":"UMC"));
	if(IS_A_CUT(ChipVersion)) DBG_871X("A_CUT_");
	else if(IS_B_CUT(ChipVersion)) DBG_871X("B_CUT_");
	else if(IS_C_CUT(ChipVersion)) DBG_871X("C_CUT_");
	else if(IS_D_CUT(ChipVersion)) DBG_871X("D_CUT_");
	else if(IS_E_CUT(ChipVersion)) DBG_871X("E_CUT_");
	else DBG_871X("UNKNOWN_CUT(%d)_",ChipVersion.CUTVersion);

	if(IS_1T1R(ChipVersion))	DBG_871X("1T1R_");
	else if(IS_1T2R(ChipVersion))	DBG_871X("1T2R_");
	else if(IS_2T2R(ChipVersion))	DBG_871X("2T2R_");
	else DBG_871X("UNKNOWN_RFTYPE(%d)_",ChipVersion.RFType);


	DBG_871X("RomVer(%d)\n",ChipVersion.ROMVer);
}


#define	EEPROM_CHANNEL_PLAN_BY_HW_MASK	0x80

u8	//return the final channel plan decision
hal_com_get_channel_plan(
	IN	PADAPTER	padapter,
	IN	u8			hw_channel_plan,	//channel plan from HW (efuse/eeprom)
	IN	u8			sw_channel_plan,	//channel plan from SW (registry/module param)
	IN	u8			def_channel_plan,	//channel plan used when the former two is invalid
	IN	BOOLEAN		AutoLoadFail
	)
{
	u8 swConfig;
	u8 chnlPlan;

	swConfig = _TRUE;
	if (!AutoLoadFail)
	{
		if (!rtw_is_channel_plan_valid(sw_channel_plan))
			swConfig = _FALSE;
		if (hw_channel_plan & EEPROM_CHANNEL_PLAN_BY_HW_MASK)
			swConfig = _FALSE;
	}

	if (swConfig == _TRUE)
		chnlPlan = sw_channel_plan;
	else
		chnlPlan = hw_channel_plan & (~EEPROM_CHANNEL_PLAN_BY_HW_MASK);

	if (!rtw_is_channel_plan_valid(chnlPlan))
		chnlPlan = def_channel_plan;

	return chnlPlan;
}

u8	MRateToHwRate(u8 rate)
{
	u8	ret = DESC_RATE1M;

	switch(rate)
	{
		// CCK and OFDM non-HT rates
	case IEEE80211_CCK_RATE_1MB:	ret = DESC_RATE1M;	break;
	case IEEE80211_CCK_RATE_2MB:	ret = DESC_RATE2M;	break;
	case IEEE80211_CCK_RATE_5MB:	ret = DESC_RATE5_5M;	break;
	case IEEE80211_CCK_RATE_11MB:	ret = DESC_RATE11M;	break;
	case IEEE80211_OFDM_RATE_6MB:	ret = DESC_RATE6M;	break;
	case IEEE80211_OFDM_RATE_9MB:	ret = DESC_RATE9M;	break;
	case IEEE80211_OFDM_RATE_12MB:	ret = DESC_RATE12M;	break;
	case IEEE80211_OFDM_RATE_18MB:	ret = DESC_RATE18M;	break;
	case IEEE80211_OFDM_RATE_24MB:	ret = DESC_RATE24M;	break;
	case IEEE80211_OFDM_RATE_36MB:	ret = DESC_RATE36M;	break;
	case IEEE80211_OFDM_RATE_48MB:	ret = DESC_RATE48M;	break;
	case IEEE80211_OFDM_RATE_54MB:	ret = DESC_RATE54M;	break;

		// HT rates since here
	//case MGN_MCS0:		ret = DESC_RATEMCS0;	break;
	//case MGN_MCS1:		ret = DESC_RATEMCS1;	break;
	//case MGN_MCS2:		ret = DESC_RATEMCS2;	break;
	//case MGN_MCS3:		ret = DESC_RATEMCS3;	break;
	//case MGN_MCS4:		ret = DESC_RATEMCS4;	break;
	//case MGN_MCS5:		ret = DESC_RATEMCS5;	break;
	//case MGN_MCS6:		ret = DESC_RATEMCS6;	break;
	//case MGN_MCS7:		ret = DESC_RATEMCS7;	break;

	default:		break;
	}

	return ret;
}

void	HalSetBrateCfg(
	IN PADAPTER		Adapter,
	IN u8			*mBratesOS,
	OUT u16			*pBrateCfg)
{
	u8	i, is_brate, brate;

	for(i=0;i<NDIS_802_11_LENGTH_RATES_EX;i++)
	{
		is_brate = mBratesOS[i] & IEEE80211_BASIC_RATE_MASK;
		brate = mBratesOS[i] & 0x7f;

		if( is_brate )
		{
			switch(brate)
			{
				case IEEE80211_CCK_RATE_1MB:	*pBrateCfg |= RATE_1M;	break;
				case IEEE80211_CCK_RATE_2MB:	*pBrateCfg |= RATE_2M;	break;
				case IEEE80211_CCK_RATE_5MB:	*pBrateCfg |= RATE_5_5M;break;
				case IEEE80211_CCK_RATE_11MB:	*pBrateCfg |= RATE_11M;	break;
				case IEEE80211_OFDM_RATE_6MB:	*pBrateCfg |= RATE_6M;	break;
				case IEEE80211_OFDM_RATE_9MB:	*pBrateCfg |= RATE_9M;	break;
				case IEEE80211_OFDM_RATE_12MB:	*pBrateCfg |= RATE_12M;	break;
				case IEEE80211_OFDM_RATE_18MB:	*pBrateCfg |= RATE_18M;	break;
				case IEEE80211_OFDM_RATE_24MB:	*pBrateCfg |= RATE_24M;	break;
				case IEEE80211_OFDM_RATE_36MB:	*pBrateCfg |= RATE_36M;	break;
				case IEEE80211_OFDM_RATE_48MB:	*pBrateCfg |= RATE_48M;	break;
				case IEEE80211_OFDM_RATE_54MB:	*pBrateCfg |= RATE_54M;	break;
			}
		}
	}
}

static VOID
_OneOutPipeMapping(
	IN	PADAPTER	pAdapter
	)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
	pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];//VI
	pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[0];//BE
	pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[0];//BK

	pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
	pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
	pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
	pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD
}

static VOID
_TwoOutPipeMapping(
	IN	PADAPTER	pAdapter,
	IN	BOOLEAN	 	bWIFICfg
	)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if(bWIFICfg){ //WMM

		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA
		//{  0, 	1, 	0, 	1, 	0, 	0, 	0, 	0, 		0	};
		//0:H, 1:L

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[1];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[1];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[0];//BK

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD

	}
	else{//typical setting


		//BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA
		//{  1, 	1, 	0, 	0, 	0, 	0, 	0, 	0, 		0	};
		//0:H, 1:L

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[0];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[1];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];//BK

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD

	}

}

static VOID _ThreeOutPipeMapping(
	IN	PADAPTER	pAdapter,
	IN	BOOLEAN	 	bWIFICfg
	)
{
	struct dvobj_priv	*pdvobjpriv = adapter_to_dvobj(pAdapter);

	if(bWIFICfg){//for WMM

		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA
		//{  1, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};
		//0:H, 1:N, 2:L

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[1];//BK

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD

	}
	else{//typical setting


		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA
		//{  2, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};
		//0:H, 1:N, 2:L

		pdvobjpriv->Queue2Pipe[0] = pdvobjpriv->RtOutPipe[0];//VO
		pdvobjpriv->Queue2Pipe[1] = pdvobjpriv->RtOutPipe[1];//VI
		pdvobjpriv->Queue2Pipe[2] = pdvobjpriv->RtOutPipe[2];//BE
		pdvobjpriv->Queue2Pipe[3] = pdvobjpriv->RtOutPipe[2];//BK

		pdvobjpriv->Queue2Pipe[4] = pdvobjpriv->RtOutPipe[0];//BCN
		pdvobjpriv->Queue2Pipe[5] = pdvobjpriv->RtOutPipe[0];//MGT
		pdvobjpriv->Queue2Pipe[6] = pdvobjpriv->RtOutPipe[0];//HIGH
		pdvobjpriv->Queue2Pipe[7] = pdvobjpriv->RtOutPipe[0];//TXCMD
	}

}

BOOLEAN
Hal_MappingOutPipe(
	IN	PADAPTER	pAdapter,
	IN	u8		NumOutPipe
	)
{
	struct registry_priv *pregistrypriv = &pAdapter->registrypriv;

	BOOLEAN	 bWIFICfg = (pregistrypriv->wifi_spec) ?_TRUE:_FALSE;

	BOOLEAN result = _TRUE;

	switch(NumOutPipe)
	{
		case 2:
			_TwoOutPipeMapping(pAdapter, bWIFICfg);
			break;
		case 3:
			_ThreeOutPipeMapping(pAdapter, bWIFICfg);
			break;
		case 1:
			_OneOutPipeMapping(pAdapter);
			break;
		default:
			result = _FALSE;
			break;
	}

	return result;

}

void hw_var_port_switch(_adapter *adapter)
{
#ifdef CONFIG_CONCURRENT_MODE
#ifdef CONFIG_RUNTIME_PORT_SWITCH
/*
0x102: MSR
0x550: REG_BCN_CTRL
0x551: REG_BCN_CTRL_1
0x55A: REG_ATIMWND
0x560: REG_TSFTR
0x568: REG_TSFTR1
0x570: REG_ATIMWND_1
0x610: REG_MACID
0x618: REG_BSSID
0x700: REG_MACID1
0x708: REG_BSSID1
*/

	int i;
	u8 msr;
	u8 bcn_ctrl;
	u8 bcn_ctrl_1;
	u8 atimwnd[2];
	u8 atimwnd_1[2];
	u8 tsftr[8];
	u8 tsftr_1[8];
	u8 macid[6];
	u8 bssid[6];
	u8 macid_1[6];
	u8 bssid_1[6];

	u8 iface_type;

	msr = rtw_read8(adapter, MSR);
	bcn_ctrl = rtw_read8(adapter, REG_BCN_CTRL);
	bcn_ctrl_1 = rtw_read8(adapter, REG_BCN_CTRL_1);

	for (i=0; i<2; i++)
		atimwnd[i] = rtw_read8(adapter, REG_ATIMWND+i);
	for (i=0; i<2; i++)
		atimwnd_1[i] = rtw_read8(adapter, REG_ATIMWND_1+i);

	for (i=0; i<8; i++)
		tsftr[i] = rtw_read8(adapter, REG_TSFTR+i);
	for (i=0; i<8; i++)
		tsftr_1[i] = rtw_read8(adapter, REG_TSFTR1+i);

	for (i=0; i<6; i++)
		macid[i] = rtw_read8(adapter, REG_MACID+i);

	for (i=0; i<6; i++)
		bssid[i] = rtw_read8(adapter, REG_BSSID+i);

	for (i=0; i<6; i++)
		macid_1[i] = rtw_read8(adapter, REG_MACID1+i);

	for (i=0; i<6; i++)
		bssid_1[i] = rtw_read8(adapter, REG_BSSID1+i);

#ifdef DBG_RUNTIME_PORT_SWITCH
	DBG_871X(FUNC_ADPT_FMT" before switch\n"
		"msr:0x%02x\n"
		"bcn_ctrl:0x%02x\n"
		"bcn_ctrl_1:0x%02x\n"
		"atimwnd:0x%04x\n"
		"atimwnd_1:0x%04x\n"
		"tsftr:%llu\n"
		"tsftr1:%llu\n"
		"macid:"MAC_FMT"\n"
		"bssid:"MAC_FMT"\n"
		"macid_1:"MAC_FMT"\n"
		"bssid_1:"MAC_FMT"\n"
		, FUNC_ADPT_ARG(adapter)
		, msr
		, bcn_ctrl
		, bcn_ctrl_1
		, *((u16*)atimwnd)
		, *((u16*)atimwnd_1)
		, *((u64*)tsftr)
		, *((u64*)tsftr_1)
		, MAC_ARG(macid)
		, MAC_ARG(bssid)
		, MAC_ARG(macid_1)
		, MAC_ARG(bssid_1)
	);
#endif /* DBG_RUNTIME_PORT_SWITCH */

	/* disable bcn function, disable update TSF  */
	rtw_write8(adapter, REG_BCN_CTRL, (bcn_ctrl & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);
	rtw_write8(adapter, REG_BCN_CTRL_1, (bcn_ctrl_1 & (~EN_BCN_FUNCTION)) | DIS_TSF_UDT);

	/* switch msr */
	msr = (msr&0xf0) |((msr&0x03) << 2) | ((msr&0x0c) >> 2);
	rtw_write8(adapter, MSR, msr);

	/* write port0 */
	rtw_write8(adapter, REG_BCN_CTRL, bcn_ctrl_1 & ~EN_BCN_FUNCTION);
	for (i=0; i<2; i++)
		rtw_write8(adapter, REG_ATIMWND+i, atimwnd_1[i]);
	for (i=0; i<8; i++)
		rtw_write8(adapter, REG_TSFTR+i, tsftr_1[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_MACID+i, macid_1[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_BSSID+i, bssid_1[i]);

	/* write port1 */
	rtw_write8(adapter, REG_BCN_CTRL_1, bcn_ctrl & ~EN_BCN_FUNCTION);
	for (i=0; i<2; i++)
		rtw_write8(adapter, REG_ATIMWND_1+1, atimwnd[i]);
	for (i=0; i<8; i++)
		rtw_write8(adapter, REG_TSFTR1+i, tsftr[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_MACID1+i, macid[i]);
	for (i=0; i<6; i++)
		rtw_write8(adapter, REG_BSSID1+i, bssid[i]);

	/* write bcn ctl */
	rtw_write8(adapter, REG_BCN_CTRL, bcn_ctrl_1);
	rtw_write8(adapter, REG_BCN_CTRL_1, bcn_ctrl);

	if (adapter->iface_type == IFACE_PORT0) {
		adapter->iface_type = IFACE_PORT1;
		adapter->pbuddy_adapter->iface_type = IFACE_PORT0;
		DBG_871X_LEVEL(_drv_always_, "port switch - port0("ADPT_FMT"), port1("ADPT_FMT")\n",
			ADPT_ARG(adapter->pbuddy_adapter), ADPT_ARG(adapter));
	} else {
		adapter->iface_type = IFACE_PORT0;
		adapter->pbuddy_adapter->iface_type = IFACE_PORT1;
		DBG_871X_LEVEL(_drv_always_, "port switch - port0("ADPT_FMT"), port1("ADPT_FMT")\n",
			ADPT_ARG(adapter), ADPT_ARG(adapter->pbuddy_adapter));
	}

#ifdef DBG_RUNTIME_PORT_SWITCH
	msr = rtw_read8(adapter, MSR);
	bcn_ctrl = rtw_read8(adapter, REG_BCN_CTRL);
	bcn_ctrl_1 = rtw_read8(adapter, REG_BCN_CTRL_1);

	for (i=0; i<2; i++)
		atimwnd[i] = rtw_read8(adapter, REG_ATIMWND+i);
	for (i=0; i<2; i++)
		atimwnd_1[i] = rtw_read8(adapter, REG_ATIMWND_1+i);

	for (i=0; i<8; i++)
		tsftr[i] = rtw_read8(adapter, REG_TSFTR+i);
	for (i=0; i<8; i++)
		tsftr_1[i] = rtw_read8(adapter, REG_TSFTR1+i);

	for (i=0; i<6; i++)
		macid[i] = rtw_read8(adapter, REG_MACID+i);

	for (i=0; i<6; i++)
		bssid[i] = rtw_read8(adapter, REG_BSSID+i);

	for (i=0; i<6; i++)
		macid_1[i] = rtw_read8(adapter, REG_MACID1+i);

	for (i=0; i<6; i++)
		bssid_1[i] = rtw_read8(adapter, REG_BSSID1+i);

	DBG_871X(FUNC_ADPT_FMT" after switch\n"
		"msr:0x%02x\n"
		"bcn_ctrl:0x%02x\n"
		"bcn_ctrl_1:0x%02x\n"
		"atimwnd:%u\n"
		"atimwnd_1:%u\n"
		"tsftr:%llu\n"
		"tsftr1:%llu\n"
		"macid:"MAC_FMT"\n"
		"bssid:"MAC_FMT"\n"
		"macid_1:"MAC_FMT"\n"
		"bssid_1:"MAC_FMT"\n"
		, FUNC_ADPT_ARG(adapter)
		, msr
		, bcn_ctrl
		, bcn_ctrl_1
		, *((u16*)atimwnd)
		, *((u16*)atimwnd_1)
		, *((u64*)tsftr)
		, *((u64*)tsftr_1)
		, MAC_ARG(macid)
		, MAC_ARG(bssid)
		, MAC_ARG(macid_1)
		, MAC_ARG(bssid_1)
	);
#endif /* DBG_RUNTIME_PORT_SWITCH */

#endif /* CONFIG_RUNTIME_PORT_SWITCH */
#endif /* CONFIG_CONCURRENT_MODE */
}

void SetHwReg(PADAPTER padapter, u8 variable, u8 *val)
{
	PHAL_DATA_TYPE	pHalData = GET_HAL_DATA(padapter);

_func_enter_;

	switch (variable) {
	case HW_VAR_PORT_SWITCH:
		hw_var_port_switch(padapter);
		break;
	default:
		//DBG_871X_LEVEL(_drv_always_, FUNC_ADPT_FMT" variable(%d) not defined!\n",
		//	FUNC_ADPT_ARG(padapter), variable);
		break;
	}

_func_exit_;
}

void GetHwReg(PADAPTER padapter, u8 variable, u8 *val)
{
	PHAL_DATA_TYPE pHalData = GET_HAL_DATA(padapter);

_func_enter_;

	switch (variable) {
	default:
		//DBG_871X_LEVEL(_drv_always_, FUNC_ADPT_FMT" variable(%d) not defined!\n",
		//	FUNC_ADPT_ARG(padapter), variable);
		break;
	}

_func_exit_;
}




