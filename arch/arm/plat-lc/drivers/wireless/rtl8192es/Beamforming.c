#include "8192cd.h"
#include "8192cd_cfg.h"
#include "8192cd_util.h"
#include "8192cd_headers.h"
#include "Beamforming.h"
#include "8812_reg.h"
#include "8812_vht_gen.h"



#ifdef BEAMFORMING_SUPPORT

VOID
PacketAppendData(
	IN	POCTET_STRING	packet,
	IN	OCTET_STRING	data
	)
{
	pu1Byte buf = packet->Octet + packet->Length;
	memcpy( buf, data.Octet, data.Length);
	packet->Length = packet->Length + data.Length;
}

VOID
Beamforming_GidPAid(
	struct rtl8192cd_priv *priv,
	struct stat_info	*pstat)
{

	if (OPMODE & WIFI_AP_STATE)
	{
		u2Byte	AID = (u2Byte) ((pstat->aid) & 0x1ff); 		//AID[0:8]
		u2Byte	bssid = 0;	

		pstat->g_id = 63;
		
		bssid = ((BSSID[5] & 0xf0) >> 4) ^ (BSSID[5] & 0xf);	// BSSID[44:47] xor BSSID[40:43]
		pstat->p_aid = (AID + bssid * 32) & 0x1ff;		// (dec(A) + dec(B)*32) mod 512		

	}
	else if (OPMODE & WIFI_ADHOC_STATE)
	{
		pstat->p_aid = REMAP_AID(pstat);
		pstat->g_id = 63;

	}
	else if (OPMODE & WIFI_STATION_STATE)
	{
		pstat->g_id = 0;		
		pstat->p_aid = ((int)(pstat->hwaddr[5])<<1) | (pstat->hwaddr[4]>>7);
	}	
}

BEAMFORMING_CAP
Beamforming_GetEntryBeamCapByMacId(
	struct rtl8192cd_priv *priv,
	IN	u1Byte		MacId
	)
{
	u1Byte	i = 0;
	PRT_BEAMFORMING_INFO	pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	BEAMFORMING_CAP			BeamformEntryCap = BEAMFORMING_CAP_NONE;
	
	for(i = 0; i < BEAMFORMING_ENTRY_NUM; i++)
	{
		if(	pBeamformingInfo->BeamformingEntry[i].bUsed && 
			(MacId == pBeamformingInfo->BeamformingEntry[i].MacId))
		{
			BeamformEntryCap =  pBeamformingInfo->BeamformingEntry[i].BeamformEntryCap;
			i = BEAMFORMING_ENTRY_NUM;
		}
	}

	return BeamformEntryCap;
}


PRT_BEAMFORMING_ENTRY
Beamforming_GetEntryByAddr(
	struct rtl8192cd_priv *priv,
	IN	pu1Byte		RA,
	OUT	pu1Byte		Idx
	)
{
	u1Byte	i = 0;
	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	
	for(i = 0; i < BEAMFORMING_ENTRY_NUM; i++)
	{
		if(	pBeamformingInfo->BeamformingEntry[i].bUsed && 
			0 == (memcmp(RA, pBeamformingInfo->BeamformingEntry[i].MacAddr, MACADDRLEN)))
		{
			*Idx = i;
			return &(pBeamformingInfo->BeamformingEntry[i]);
		}
	}
	return NULL;
}


PRT_BEAMFORMING_ENTRY
Beamforming_GetEntryByMacId(
	struct rtl8192cd_priv *priv,
	u1Byte		MacId,
	pu1Byte		Idx
	)
{
	u1Byte	i = 0;
	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	
	for(i = 0; i < BEAMFORMING_ENTRY_NUM; i++)
	{
		if(	pBeamformingInfo->BeamformingEntry[i].bUsed && 
			(MacId == pBeamformingInfo->BeamformingEntry[i].MacId))
		{
			*Idx = i;
			return &(pBeamformingInfo->BeamformingEntry[i]);
		}
	}

	return NULL;

}

PRT_BEAMFORMING_ENTRY
Beamforming_GetFreeEntry(
	struct rtl8192cd_priv *priv,
	OUT	pu1Byte		Idx
	)
{
	u1Byte	i = 0;
	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	
	for(i = 0; i < BEAMFORMING_ENTRY_NUM; i++)
	{
		if(pBeamformingInfo->BeamformingEntry[i].bUsed == FALSE)
		{
			*Idx = i;
			return &(pBeamformingInfo->BeamformingEntry[i]);
		}	
	}
	return NULL;
}

PRT_BEAMFORMING_ENTRY
Beamforming_AddEntry(
	struct rtl8192cd_priv *priv,
		pu1Byte				RA,
		u2Byte				AID,
		u2Byte				MacID,
		u1Byte				BW,
		BEAMFORMING_CAP		BeamformCap,
		pu1Byte				Idx
	)
{
	PRT_BEAMFORMING_ENTRY	pEntry;
	pEntry = Beamforming_GetFreeEntry(priv, Idx);

	if(pEntry != NULL)
	{	
		pEntry->bUsed = TRUE;
		pEntry->AID = AID;
		pEntry->MacId = MacID;
		pEntry->BW = BW;

		// AID -> P_AID
		if (OPMODE & WIFI_AP_STATE)
		{
			u2Byte bssid = ((GET_MY_HWADDR[5]>> 4) & 0x0f ) ^ 
							(GET_MY_HWADDR[5] & 0xf);				// BSSID[44:47] xor BSSID[40:43]
			pEntry->P_AID = (AID + (bssid <<5)) & 0x1ff;				// (dec(A) + dec(B)*32) mod 512	

		}
		else if (OPMODE & WIFI_ADHOC_STATE)
		{
//			pEntry->P_AID = AID;
			pEntry->P_AID = 0;
	
		}
		else if (OPMODE & WIFI_STATION_STATE) {
			pEntry->P_AID =  RA[5];						// BSSID[39:47]
			pEntry->P_AID = (pEntry->P_AID << 1) | (RA[4] >> 7 );
		}
		//
			
		memcpy(pEntry->MacAddr, RA, MACADDRLEN);
		pEntry->bTxBF = FALSE;
		pEntry->BeamformEntryCap = BeamformCap;	
		pEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;
		pEntry->LogSeq = 0xff;
		pEntry->LogRetryCnt = 0;
		pEntry->LogStatusFailCnt = 0;		
#ifdef MBSSID
		if(GET_CHIP_VER(priv) == VERSION_8812E)
			if (GET_MIB(GET_ROOT(priv))->miscEntry.vap_enable)
				rtl8192cd_set_mbssid(priv, RA, *Idx);
#endif
		return pEntry;
	}
	else
		return NULL;
}

BOOLEAN
Beamforming_RemoveEntry(
	struct rtl8192cd_priv *priv,
	IN	pu1Byte		RA,
	OUT	pu1Byte		Idx
	)
{
	PRT_BEAMFORMING_ENTRY	pEntry = Beamforming_GetEntryByAddr(priv, RA, Idx);

	if(pEntry != NULL)
	{	
		pEntry->bUsed = FALSE;
		pEntry->BeamformEntryCap = BEAMFORMING_CAP_NONE;
		pEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;
		return TRUE;
	}
	else
		return FALSE;
}

BOOLEAN
Beamforming_InitEntry(
	struct rtl8192cd_priv	*priv,
	struct stat_info		*pSTA,
	pu1Byte					Idx	
	)
{

	PRT_BEAMFORMING_ENTRY	pBeamformEntry = NULL;
	pu1Byte					RA; 
	u2Byte					AID, MacID;
	u1Byte					WirelessMode;
	u1Byte					BW = HT_CHANNEL_WIDTH_20;
	BEAMFORMING_CAP			BeamformCap = BEAMFORMING_CAP_NONE;	

	// The current setting does not support Beaforming
	if (priv->pmib->dot11RFEntry.txbf == 0)
	{
		return FALSE;
	}

	// IBSS, AP mode
	if(pSTA != NULL)
	{
		AID = pSTA->aid;
		RA  = pSTA->hwaddr;
		MacID = pSTA->aid;

		WirelessMode = pSTA->WirelessMode;
		BW = pSTA->tx_bw;
	}
	else	// Client mode
	{
		return FALSE;
	}

	if( WirelessMode < WIRELESS_MODE_N_24G)
		return FALSE;

	else 
	{

// BIT 4 implies capable of sending NDPA (BFER),
// BIT 3 implies capable of receiving NDPA (BFEE),
	if(pSTA->ht_cap_len && (cpu_to_le32(pSTA->ht_cap_buf.txbf_cap) & 0x8)&& (priv->pmib->dot11RFEntry.txbfer == 1)){
		BeamformCap |=BEAMFORMER_CAP_HT_EXPLICIT;
//		panic_printk("[%d] BeamformCap = BEAMFORMER_CAP_HT_EXPLICIT \n",__LINE__);
	}
	if (pSTA->ht_cap_len && (cpu_to_le32(pSTA->ht_cap_buf.txbf_cap) & 0x10)&& (priv->pmib->dot11RFEntry.txbfee == 1)){
		BeamformCap |=BEAMFORMEE_CAP_HT_EXPLICIT;
//		panic_printk("[%d] BeamformCap = BEAMFORMEE_CAP_HT_EXPLICIT \n",__LINE__);
	}

#ifdef RTK_AC_SUPPORT			
		if(WirelessMode == WIRELESS_MODE_AC_5G)
		{
			if(pSTA->vht_cap_len && (cpu_to_le32(pSTA->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)) && (priv->pmib->dot11RFEntry.txbfer == 1))  // ACÁÙ¨S¨Ì¾Úbfer or bfee¾×
				BeamformCap |=BEAMFORMER_CAP_VHT_SU;
			if(pSTA->vht_cap_len && (cpu_to_le32(pSTA->vht_cap_buf.vht_cap_info) & BIT(SU_BFER_S)) && (priv->pmib->dot11RFEntry.txbfee == 1))
				BeamformCap |=BEAMFORMEE_CAP_VHT_SU;			
		}
#endif		
	}

	if(BeamformCap == BEAMFORMING_CAP_NONE)
		return FALSE;
	

	pBeamformEntry = Beamforming_GetEntryByAddr(priv, RA, Idx);
	if(pBeamformEntry == NULL)
	{
		pBeamformEntry = Beamforming_AddEntry(priv, RA, AID, MacID, BW, BeamformCap, Idx);
		if(pBeamformEntry == FALSE){
			return FALSE;
		}else
			pBeamformEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_INITIALIZEING;
	}	
	else
	{
		// Entry has been created. If entry is initialing or progressing then errors occur.
		if(	pBeamformEntry->BeamformEntryState != BEAMFORMING_ENTRY_STATE_INITIALIZED && 
			pBeamformEntry->BeamformEntryState != BEAMFORMING_ENTRY_STATE_PROGRESSED)
		{
			panic_printk("Error State of Beamforming");
			return FALSE;
		}	
		else
			pBeamformEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_INITIALIZEING;
	}

	pBeamformEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_INITIALIZED;
	Beamforming_AutoTest(priv, *Idx, pBeamformEntry);

	return TRUE;
}

VOID
Beamforming_DeInitEntry(
	struct rtl8192cd_priv *priv,
	pu1Byte			RA
	)
{
	u1Byte					Idx = 0;

//	panic_printk("%d%s \n",__LINE__,__FUNCTION__);
	
	if(Beamforming_RemoveEntry(priv, RA, &Idx) == TRUE)
	{
#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv) == VERSION_8812E) {
			SetBeamformLeave8812(priv, Idx);	
#ifdef MBSSID
			if (GET_MIB(GET_ROOT(priv))->miscEntry.vap_enable)
				rtl8192cd_clear_mbssid(priv, Idx);
#endif
		}
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
		if(GET_CHIP_VER(priv) == VERSION_8192E){
			SetBeamformLeave92E(priv,Idx);
//			panic_printk("[%d][%s]\n",__LINE__,__FUNCTION__);
		}
#endif
	}

}

VOID
BeamformingReset(
	struct rtl8192cd_priv *priv
	)
{
	u1Byte		Idx = 0;
	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);

	for(Idx = 0; Idx < BEAMFORMING_ENTRY_NUM; Idx++)
	{
		if(pBeamformingInfo->BeamformingEntry[Idx].bUsed == TRUE)
		{
			pBeamformingInfo->BeamformingEntry[Idx].bUsed = FALSE;
			pBeamformingInfo->BeamformingEntry[Idx].BeamformEntryCap = BEAMFORMING_CAP_NONE;
			pBeamformingInfo->BeamformingEntry[Idx].BeamformEntryState = BEAMFORMING_ENTRY_STATE_UNINITIALIZE;

#ifdef CONFIG_RTL_8812_SUPPORT
			if(GET_CHIP_VER(priv) == VERSION_8812E)
				SetBeamformLeave8812(priv, Idx);
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
			if(GET_CHIP_VER(priv) == VERSION_8192E)			
				SetBeamformLeave92E(priv, Idx);
#endif			
		}
	}

}



#define FillOctetString(_os,_octet,_len)		\
	(_os).Octet=(pu1Byte)(_octet);			\
	(_os).Length=(_len);

VOID
ConstructHTNDPAPacket(
	struct rtl8192cd_priv *priv,
	pu1Byte				RA,
	pu1Byte				Buffer,
	pu4Byte				pLength,
	u1Byte			 	BW

	)
{
	u2Byte					Duration= 0;
	OCTET_STRING			pNDPAFrame, ActionContent;
	u1Byte					ActionHdr[4] = {ACT_CAT_VENDOR, 0x00, 0xe0, 0x4c};
	int aSifsTime = ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && (priv->pshare->ht_sta_num)) ? 0x10 : 10;


	SET_80211_HDR_FRAME_CONTROL(Buffer,0);
	SET_80211_HDR_ORDER(Buffer, 1);
	SET_80211_HDR_TYPE_AND_SUBTYPE(Buffer,Type_Action_No_Ack);

	memcpy((void *)GetAddr1Ptr(Buffer), RA, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr(Buffer), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr(Buffer), BSSID, MACADDRLEN);

	Duration = 2*aSifsTime + 40;
	
	if(BW== HT_CHANNEL_WIDTH_20_40)
		Duration+= 87;
	else	
		Duration+= 180;

	SET_80211_HDR_DURATION(Buffer, Duration);

	//HT control field
	SET_HT_CTRL_CSI_STEERING(Buffer+sMacHdrLng, 3);
	SET_HT_CTRL_NDP_ANNOUNCEMENT(Buffer+sMacHdrLng, 1);
	
	FillOctetString(pNDPAFrame, Buffer, sMacHdrLng+sHTCLng);

	FillOctetString(ActionContent, ActionHdr, 4);
	PacketAppendData(&pNDPAFrame, ActionContent);	

	*pLength = 32;
}


BOOLEAN
SendHTNDPAPacket(
	struct rtl8192cd_priv *priv,
		pu1Byte				RA,
		u1Byte 				BW

	)
{
	BOOLEAN					ret = TRUE;
	unsigned char *pbuf 		= get_wlanllchdr_from_poll(priv);
	u4Byte PacketLength;
	DECLARE_TXINSN(txinsn);	

	if(pbuf) 
	{
		memset(pbuf, 0, sizeof(struct wlan_hdr));
		ConstructHTNDPAPacket(
				priv, 
				RA,
				pbuf,
				&PacketLength,
				BW
				);
		
		txinsn.q_num = MGNT_QUEUE;	
		txinsn.fr_type = _PRE_ALLOCLLCHDR_;				

		txinsn.phdr = pbuf;
		txinsn.hdr_len = PacketLength;
		txinsn.fr_len = 0;
		txinsn.lowest_tx_rate = txinsn.tx_rate = _MCS8_RATE_; //_MCS8_RATE_;
		txinsn.fixed_rate = 1;	
		txinsn.ndpa = 1;

		if (rtl8192cd_wlantx(priv, &txinsn) == CONGESTED) {		
			netif_stop_queue(priv->dev);		
			priv->ext_stats.tx_drops++; 	
//			panic_printk("TX DROP: Congested!\n");
			if (txinsn.phdr)
				release_wlanhdr_to_poll(priv, txinsn.phdr); 			
			if (txinsn.pframe)
				release_mgtbuf_to_poll(priv, txinsn.pframe);			
			return 0;	
		}
	}
	else
		ret = FALSE;

	return ret;
}




VOID
ConstructVHTNDPAPacket(
	struct rtl8192cd_priv *priv,
	pu1Byte			RA,
	u2Byte			AID,
	pu1Byte			Buffer,
	pu4Byte			pLength,
	u1Byte 			BW
	)
{
	u2Byte					Duration= 0;
	u1Byte					Sequence = 0;
	pu1Byte					pNDPAFrame = Buffer;
	u2Byte					tmp16;
	
	RT_NDPA_STA_INFO		STAInfo;
	int aSifsTime = ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && (priv->pshare->ht_sta_num)) ? 0x10 : 10;

	// Frame control.
	SET_80211_HDR_FRAME_CONTROL(pNDPAFrame, 0);
	SET_80211_HDR_TYPE_AND_SUBTYPE(pNDPAFrame, Type_NDPA);

	memcpy((void *)GetAddr1Ptr(pNDPAFrame), RA, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr(pNDPAFrame), GET_MY_HWADDR, MACADDRLEN);

	Duration = 2*aSifsTime + 44;
	
	if(BW == HT_CHANNEL_WIDTH_80)
		Duration += 40;
	else if(BW == HT_CHANNEL_WIDTH_20_40)
		Duration+= 87;
	else	
		Duration+= 180;

	SetDuration(pNDPAFrame, Duration);
	Sequence = GET_HW(priv)->sounding_seq<<2;
	GET_HW(priv)->sounding_seq =  (GET_HW(priv)->sounding_seq+1) & 0xfff;
	 
	memcpy(pNDPAFrame+16, &Sequence,1);

	if (OPMODE & WIFI_ADHOC_STATE)
		AID = 0;

	STAInfo.AID = AID;

	STAInfo.FeedbackType = 0;
	STAInfo.NcIndex = 0;
	
	memcpy(&tmp16, (pu1Byte)&STAInfo, 2);
	tmp16 = cpu_to_le16(tmp16);

	memcpy(pNDPAFrame+17, &tmp16, 2);

	*pLength = 19;
}


BOOLEAN
SendVHTNDPAPacket(
	struct rtl8192cd_priv *priv,
	IN	pu1Byte			RA,
	IN	u2Byte			AID,
	u1Byte 				BW
	)
{
	BOOLEAN					ret = TRUE;
	u4Byte 					PacketLength;
	unsigned char *pbuf 	= get_wlanllchdr_from_poll(priv);
	DECLARE_TXINSN(txinsn);	

	if(pbuf)
	{
		memset(pbuf, 0, sizeof(struct wlan_hdr));

		ConstructVHTNDPAPacket	(
			priv, 
			RA,
			AID,
			pbuf,
			&PacketLength,
			BW
			);

		txinsn.q_num = MANAGE_QUE_NUM;
		txinsn.fr_type = _PRE_ALLOCLLCHDR_;		
		txinsn.phdr = pbuf;
		txinsn.hdr_len = PacketLength;
		txinsn.fr_len = 0;
		txinsn.fixed_rate = 1;	
		txinsn.lowest_tx_rate = txinsn.tx_rate = _NSS2_MCS0_RATE_;
		txinsn.ndpa = 1;

		if (rtl8192cd_wlantx(priv, &txinsn) == CONGESTED) {		
			netif_stop_queue(priv->dev);		
			priv->ext_stats.tx_drops++; 	
//			panic_printk("TX DROP: Congested!\n");
			if (txinsn.phdr)
				release_wlanhdr_to_poll(priv, txinsn.phdr); 			
			if (txinsn.pframe)
				release_mgtbuf_to_poll(priv, txinsn.pframe);
			return 0;	
		}
	}
	else
		ret = FALSE;

	return ret;
}



u1Byte
beamforming_SoundingIdx(
	PRT_BEAMFORMING_INFO 			pBeamInfo
	)
{
	u1Byte							Idx = 0;
	PRT_BEAMFORMING_PERIOD_INFO	pBeamPeriodInfo = &(pBeamInfo->BeamformingPeriodInfo);

	if(	pBeamPeriodInfo->Mode == SOUNDING_SW_VHT_TIMER ||pBeamPeriodInfo->Mode == SOUNDING_SW_HT_TIMER ||
		pBeamPeriodInfo->Mode == SOUNDING_HW_VHT_TIMER ||pBeamPeriodInfo->Mode == SOUNDING_HW_HT_TIMER)
		Idx = pBeamPeriodInfo->Idx;
	else
		Idx = 0;

	return Idx;
}


BOOLEAN
beamfomring_bSounding(
	PRT_BEAMFORMING_INFO 	pBeamInfo,
	u1Byte					Idx
	)
{
	BOOLEAN							bSounding = FALSE;
	RT_BEAMFORMING_ENTRY			Entry = pBeamInfo->BeamformingEntry[Idx];
	RT_BEAMFORMING_PERIOD_INFO		BeamPeriodInfo = pBeamInfo->BeamformingPeriodInfo;

	if((pBeamInfo->BeamformCap & BEAMFORMER_CAP) == 0)
		bSounding = FALSE;
	else if(BeamPeriodInfo.Mode == SOUNDING_STOP_All_TIMER)
		bSounding = FALSE;
	else if(	((Entry.BeamformEntryCap & BEAMFORMER_CAP_VHT_SU) == 0) &&
			((Entry.BeamformEntryCap & BEAMFORMER_CAP_HT_EXPLICIT) == 0))
		bSounding = FALSE;
	else if(BeamPeriodInfo.Mode == SOUNDING_STOP_OID_TIMER && Entry.bTxBF == FALSE)
		bSounding = FALSE;
	else 
		bSounding = TRUE;

	return bSounding;
}


SOUNDING_MODE
beamforming_SoundingMode(
	PRT_BEAMFORMING_INFO 	pBeamInfo,
	u1Byte					Idx
	)
{
	RT_BEAMFORMING_PERIOD_INFO		BeamPeriodInfo = pBeamInfo->BeamformingPeriodInfo;	
	SOUNDING_MODE					Mode = BeamPeriodInfo.Mode;
	RT_BEAMFORMING_ENTRY			Entry = pBeamInfo->BeamformingEntry[Idx];

	if(	BeamPeriodInfo.Mode == SOUNDING_SW_VHT_TIMER || BeamPeriodInfo.Mode == SOUNDING_SW_HT_TIMER ||
		BeamPeriodInfo.Mode == SOUNDING_HW_VHT_TIMER || BeamPeriodInfo.Mode == SOUNDING_HW_HT_TIMER )
		Mode = BeamPeriodInfo.Mode;
	else	if(Entry.BeamformEntryCap & BEAMFORMER_CAP_VHT_SU)
		Mode = SOUNDING_AUTO_VHT_TIMER;
	else	if(Entry.BeamformEntryCap & BEAMFORMER_CAP_HT_EXPLICIT)
		Mode = SOUNDING_AUTO_HT_TIMER;
//	else 
//		printk("%s %d Mode %d\n", __FUNCTION__, Idx, Mode);

	return Mode;
}


u2Byte
beamforming_SoundingTime(
	PRT_BEAMFORMING_INFO 	pBeamInfo,
	SOUNDING_MODE			Mode
	)
{
	u2Byte							SoundingTime = 0xffff;
	RT_BEAMFORMING_PERIOD_INFO		BeamPeriodInfo = pBeamInfo->BeamformingPeriodInfo;

	if(Mode == SOUNDING_HW_HT_TIMER || Mode == SOUNDING_HW_VHT_TIMER)
		SoundingTime = BeamPeriodInfo.BeamPeriod * 32;
	else	if(Mode == SOUNDING_SW_HT_TIMER || Mode == SOUNDING_SW_VHT_TIMER)
		SoundingTime = BeamPeriodInfo.BeamPeriod ;
	else
		SoundingTime = 20*32;

	return SoundingTime;
}


u1Byte
beamforming_SoundingBW(
	PRT_BEAMFORMING_INFO 	pBeamInfo,
	SOUNDING_MODE			Mode,
	u1Byte					Idx
	)
{
	u1Byte							SoundingBW = HT_CHANNEL_WIDTH_20;
	RT_BEAMFORMING_ENTRY			Entry = pBeamInfo->BeamformingEntry[Idx];
	RT_BEAMFORMING_PERIOD_INFO		BeamPeriodInfo = pBeamInfo->BeamformingPeriodInfo;

	if(Mode == SOUNDING_HW_HT_TIMER || Mode == SOUNDING_HW_VHT_TIMER)
		SoundingBW = BeamPeriodInfo.BW;
	else	if(Mode == SOUNDING_SW_HT_TIMER || Mode == SOUNDING_SW_VHT_TIMER)
		SoundingBW = BeamPeriodInfo.BW;
	else 
		SoundingBW = Entry.BW;

	return SoundingBW;
}


VOID
beamforming_StartPeriod(
	struct rtl8192cd_priv *priv,
	u1Byte				Idx
	)
{

	PRT_BEAMFORMING_INFO 		pBeamInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_TIMER_INFO	pBeamTimerInfo = &(pBeamInfo->BeamformingTimerInfo[Idx]);	


//	pBeamTimerInfo->Idx = Idx;
	pBeamTimerInfo->Mode = beamforming_SoundingMode(pBeamInfo, Idx);
	pBeamTimerInfo->BW = beamforming_SoundingBW(pBeamInfo, pBeamTimerInfo->Mode, Idx);
	pBeamTimerInfo->BeamPeriod = beamforming_SoundingTime(pBeamInfo, pBeamTimerInfo->Mode);

	if(pBeamTimerInfo->Mode == SOUNDING_SW_VHT_TIMER || pBeamTimerInfo->Mode == SOUNDING_SW_HT_TIMER) {

		ODM_SetTimer(ODMPTR, &pBeamInfo->BeamformingTimer, pBeamTimerInfo->BeamPeriod);
	} else	{
#ifdef CONFIG_WLAN_HAL_8192EE
		if(GET_CHIP_VER(priv)== VERSION_8192E){			
			HW_VAR_HW_REG_TIMER_STOP_92E(priv);
			HW_VAR_HW_REG_TIMER_INIT_92E(priv, pBeamTimerInfo->BeamPeriod);
			HW_VAR_HW_REG_TIMER_START_92E(priv);
		}
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv)== VERSION_8812E){
			HW_VAR_HW_REG_TIMER_STOP_8812(priv);
			HW_VAR_HW_REG_TIMER_INIT_8812(priv, pBeamTimerInfo->BeamPeriod);
			HW_VAR_HW_REG_TIMER_START_8812(priv);
		}
#endif
	}	

//	panic_printk ("%s Idx %d Mode %d BW %d Period %d\n", __FUNCTION__, 
//			Idx, pBeamTimerInfo->Mode, pBeamTimerInfo->BW, pBeamTimerInfo->BeamPeriod);
}


VOID
beamforming_EndPeriod(
		struct rtl8192cd_priv *priv,
		u1Byte		Idx
	)
{
//	u1Byte						Idx = 0;
	PRT_BEAMFORMING_ENTRY		pBeamformEntry;
	PRT_BEAMFORMING_INFO 		pBeamInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_TIMER_INFO	pBeamTimerInfo = &(pBeamInfo->BeamformingTimerInfo[Idx]);	

	if(pBeamTimerInfo->Mode == SOUNDING_SW_VHT_TIMER || pBeamTimerInfo->Mode == SOUNDING_SW_HT_TIMER) {

		ODM_CancelTimer(ODMPTR, &pBeamInfo->BeamformingTimer);
	}  else {

#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv) == VERSION_8812E)
			HW_VAR_HW_REG_TIMER_STOP_8812(priv);
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
		if(GET_CHIP_VER(priv) == VERSION_8192E)
			HW_VAR_HW_REG_TIMER_STOP_92E(priv);
#endif
	}


	for(Idx = 0; Idx < BEAMFORMING_ENTRY_NUM; Idx++)
	{
		pBeamformEntry = pBeamInfo->BeamformingEntry+Idx;
	
		if(pBeamformEntry->bUsed)
		{
			/*	
			*	If End procedure is 
			*	1. Between (Send NDPA, C2H packet return), reset state to initialized.
			*	After C2H packet return , status bit will be set to zero. 
			*
			*	2. After C2H packet, then reset state to initialized and clear status bit.
			*/ 
			if(pBeamformEntry->BeamformEntryState == BEAMFORMING_ENTRY_STATE_PROGRESSING)
				Beamforming_End(priv, 0);
			else if(pBeamformEntry->BeamformEntryState == BEAMFORMING_ENTRY_STATE_PROGRESSED)
			{
				pBeamformEntry->BeamformEntryState  = BEAMFORMING_ENTRY_STATE_INITIALIZED;
#ifdef CONFIG_WLAN_HAL_8192EE
				if(GET_CHIP_VER(priv)== VERSION_8192E)
					SetBeamformStatus92E(priv, Idx);		
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
				if(GET_CHIP_VER(priv)== VERSION_8812E)
					SetBeamformStatus8812(priv, Idx);
#endif
			}	
		}
	}
}

struct rtl8192cd_priv* getBeamEntryDev(struct rtl8192cd_priv *priv, PRT_BEAMFORMING_ENTRY pEntry)
{
	struct stat_info *pstat;
	struct rtl8192cd_priv *vxd_priv;	
	int j;

	pstat = get_stainfo(priv, pEntry->MacAddr);
	if(pstat)
		return priv;
	
#ifdef MBSSID
	  if ((OPMODE & WIFI_AP_STATE) && priv->pmib->miscEntry.vap_enable) {
		for (j=0; j<RTL8192CD_NUM_VWLAN; j++) {
			if ((priv->pvap_priv[j]->assoc_num > 0) && IS_DRV_OPEN(priv->pvap_priv[j])) {
				pstat = get_stainfo(priv->pvap_priv[j], pEntry->MacAddr);
				if(pstat)
					return priv->pvap_priv[j];

			}
		}
	}
#endif			
#ifdef UNIVERSAL_REPEATER
	vxd_priv = GET_VXD_PRIV(priv);
	if((OPMODE & WIFI_STATION_STATE) && (vxd_priv->assoc_num > 0) && IS_DRV_OPEN(vxd_priv)	) { 	
		pstat = get_stainfo(vxd_priv, pEntry->MacAddr);
		if(pstat)
			return vxd_priv;
	}
#endif
	return NULL;

}


BOOLEAN
BeamformingStart_V2(
	struct rtl8192cd_priv *priv,
	u1Byte			Idx,
	u1Byte			Mode, 
	u1Byte			BW
	)
{
	pu1Byte					RA = NULL;
	PRT_BEAMFORMING_ENTRY	pEntry;
	BOOLEAN					ret = TRUE;
	PRT_BEAMFORMING_INFO 	pBeamformingInfo = &(priv->pshare->BeamformingInfo);

	pEntry = &(pBeamformingInfo->BeamformingEntry[Idx]);

	priv = getBeamEntryDev(priv, pEntry);
	if( !priv)
		return FALSE;

//panic_printk("%d", Idx);	
	if(pEntry->bUsed == FALSE)
	{
		pEntry->bBeamformingInProgress = FALSE;
//		panic_printk("%s, %d, \n", __FUNCTION__, __LINE__);
		return FALSE;
	}
	else
	{
		if(pEntry->bBeamformingInProgress)
			return FALSE;
		pEntry->bBeamformingInProgress = TRUE;			
	
		RA = pEntry->MacAddr;
		
		if(Mode == SOUNDING_SW_HT_TIMER || Mode == SOUNDING_HW_HT_TIMER || Mode == SOUNDING_AUTO_HT_TIMER)
		{	
			if(!(pEntry->BeamformEntryCap & BEAMFORMER_CAP_HT_EXPLICIT))
			{
//				pBeamformingInfo->bBeamformingInProgress = FALSE;
				pEntry->bBeamformingInProgress = FALSE;

//				panic_printk("%s, %d, %d\n", __FUNCTION__, __LINE__, pEntry->BeamformEntryCap);
				return FALSE;
			}
		}
		else if(Mode == SOUNDING_SW_VHT_TIMER || Mode == SOUNDING_HW_VHT_TIMER || Mode == SOUNDING_AUTO_VHT_TIMER)
		{
			if(!(pEntry->BeamformEntryCap & BEAMFORMER_CAP_VHT_SU))
			{
//				pBeamformingInfo->bBeamformingInProgress = FALSE;
				pEntry->bBeamformingInProgress = FALSE;

//				panic_printk("%s, %d, %d\n", __FUNCTION__, __LINE__, pEntry->BeamformEntryCap);
				return FALSE;
			}
		}
		if(	pEntry->BeamformEntryState != BEAMFORMING_ENTRY_STATE_INITIALIZED &&
			pEntry->BeamformEntryState != BEAMFORMING_ENTRY_STATE_PROGRESSED)
		{
//			pBeamformingInfo->bBeamformingInProgress = FALSE;
			pEntry->bBeamformingInProgress = FALSE;
//			panic_printk("%s, %d, %d\n", __FUNCTION__, __LINE__, pEntry->BeamformEntryState);
			return FALSE;
		}	
		else
			pEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_PROGRESSING;
	}

	pEntry->BW = BW;
	pBeamformingInfo->BeamformingCurIdx = Idx;
	

#ifdef CONFIG_WLAN_HAL_8192EE
	if(GET_CHIP_VER(priv)== VERSION_8192E){
		Beamforming_NDPARate_92E(priv, Mode, BW, 0);
		SetBeamformStatus92E(priv, (pBeamformingInfo->BeamformingCurIdx));
	}		
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
	if(GET_CHIP_VER(priv)== VERSION_8812E) {
		Beamforming_NDPARate_8812(priv, Mode, BW, priv->pshare->rf_ft_var.ndparate);  //
		SetBeamformStatus8812(priv, (pBeamformingInfo->BeamformingCurIdx));
	}
#endif

//  debug
	if(!priv->pshare->rf_ft_var.soundingEnable)
		return TRUE;

#ifdef CONFIG_WLAN_HAL_8192EE
	if ((OPMODE & WIFI_AP_STATE) && (priv->pshare->soundingLock))
		return TRUE;
#endif	
//	
	if(Mode == SOUNDING_SW_HT_TIMER || Mode == SOUNDING_HW_HT_TIMER || Mode == SOUNDING_AUTO_HT_TIMER)
		ret = SendHTNDPAPacket(priv,RA, BW);	
	else {

		if(priv->pshare->rf_ft_var.ndpaaid != 0xff)
			ret = SendVHTNDPAPacket(priv,RA, priv->pshare->rf_ft_var.ndpaaid, BW);
		else
			ret = SendVHTNDPAPacket(priv,RA, pEntry->AID, BW);
	}

	if(ret == FALSE)
	{
//		panic_printk("Beamforming_RemoveEntry because of failure sending NDPA for addr =\n");
//		Beamforming_RemoveEntry(priv, RA, &Idx);
		Beamforming_Leave(priv, RA);
//		pBeamformingInfo->bBeamformingInProgress = FALSE;
		pEntry->bBeamformingInProgress = FALSE;
		return FALSE;
	}

	return TRUE;
}


VOID
Beamforming_Notify(
	struct rtl8192cd_priv *priv, 
	u1Byte		Idx
	)
{
//	u1Byte						Idx;
	BOOLEAN						bSounding = FALSE;
	PRT_BEAMFORMING_INFO 		pBeamInfo = &(priv->pshare->BeamformingInfo);	

//	Idx = beamforming_SoundingIdx(pBeamInfo);
	bSounding = beamfomring_bSounding(pBeamInfo, Idx);

//panic_printk ("%s bSounding=%d, state=%d\n", __FUNCTION__, bSounding, pBeamInfo->BeamformState);

	if(pBeamInfo->BeamformState == BEAMFORMING_STATE_IDLE)
	{
		if(bSounding)
		{			
			beamforming_StartPeriod(priv, Idx);
			pBeamInfo->BeamformState = BEAMFORMING_STATE_START;
		}
	}
	else if(pBeamInfo->BeamformState == BEAMFORMING_STATE_START)
	{
		if(bSounding){
			beamforming_StartPeriod(priv, Idx);
		}
		else
		{
			beamforming_EndPeriod(priv, Idx);
			pBeamInfo->BeamformState = BEAMFORMING_STATE_END;
		}
	}
	else if(pBeamInfo->BeamformState == BEAMFORMING_STATE_END)
	{
		if(bSounding)
		{
			beamforming_StartPeriod(priv, Idx);
			pBeamInfo->BeamformState = BEAMFORMING_STATE_START;
		}
	}
//	else
//		panic_printk ("%s BeamformState %d\n", __FUNCTION__, pBeamInfo->BeamformState);
	

}


VOID
Beamforming_AutoTest(
	struct rtl8192cd_priv *priv,
	u1Byte					Idx, 
	PRT_BEAMFORMING_ENTRY	pBeamformEntry
	)
{
	SOUNDING_MODE					Mode;

	BEAMFORMING_CAP				BeamformCap = pBeamformEntry->BeamformEntryCap;
	PRT_BEAMFORMING_INFO 		pBeamInfo = &(priv->pshare->BeamformingInfo);	
	PRT_BEAMFORMING_PERIOD_INFO pBeamPeriodInfo = &(pBeamInfo->BeamformingPeriodInfo);

//	if((OPMODE & WIFI_AP_STATE) != WIFI_AP_STATE)
//		return;
//	else
	if(BeamformCap & BEAMFORMER_CAP_VHT_SU)
		Mode = SOUNDING_SW_VHT_TIMER;
	else if(BeamformCap & BEAMFORMER_CAP_HT_EXPLICIT){
#ifdef CONFIG_WLAN_HAL_8192EE
		if(GET_CHIP_VER(priv)== VERSION_8192E)
			Mode = SOUNDING_SW_HT_TIMER;
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv)== VERSION_8812E)
			Mode = SOUNDING_HW_HT_TIMER;
#endif
	}
	else 
		return;

	pBeamPeriodInfo->Idx = Idx;
	pBeamPeriodInfo->Mode = Mode;
	pBeamPeriodInfo->BW = pBeamformEntry->BW;

	pBeamPeriodInfo->BeamPeriod = priv->pshare->rf_ft_var.soundingPeriod;


}


VOID
Beamforming_End(
	struct rtl8192cd_priv *priv,
	BOOLEAN			Status	
	)
{

	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_ENTRY	pEntry = &(pBeamformingInfo->BeamformingEntry[pBeamformingInfo->BeamformingCurIdx]);


	if(pEntry->BeamformEntryState != BEAMFORMING_ENTRY_STATE_PROGRESSING)
	{
		return;
	}

	if(Status == 1)
	{
		pEntry->LogStatusFailCnt = 0;
		pEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_PROGRESSED;
#ifdef CONFIG_WLAN_HAL_8192EE
		if(GET_CHIP_VER(priv)== VERSION_8192E)
			SetBeamformStatus92E(priv, (pBeamformingInfo->BeamformingCurIdx));		
#endif		
#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv)== VERSION_8812E)
			SetBeamformStatus8812(priv, (pBeamformingInfo->BeamformingCurIdx));
#endif
	}	
	else {
		pEntry->LogStatusFailCnt++;
		pEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_INITIALIZED;
	}
//	Beamforming_SetTxBFen(priv, pEntry->MacId, Status);
//	pBeamformingInfo->bBeamformingInProgress = FALSE;
	pEntry->bBeamformingInProgress = FALSE;


	if(pEntry->LogStatusFailCnt > 50)
		Beamforming_DeInitEntry(priv, pEntry->MacAddr); 	



}	

int shortenSoundingPeriod(struct rtl8192cd_priv *priv)
{
	PRT_BEAMFORMING_INFO 		pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_ENTRY		pEntry = &(pBeamformingInfo->BeamformingEntry[0]);
	struct stat_info 			*pstat;
	struct rtl8192cd_priv 		*vxd_priv;	
	int idx, j, ret=0;
	pBeamformingInfo->BeamformingPeriodState = 0;     //

	for(idx=0 ; idx<BEAMFORMING_ENTRY_NUM; idx++)  {
		pEntry = &(pBeamformingInfo->BeamformingEntry[idx]);
		if( pEntry->bUsed)  {
			pstat = get_stainfo(priv, pEntry->MacAddr);
			if(pstat) {
				if(pstat->tx_avarage > (1<<16)){   // 0.5Mbps
					++ret;
					if(idx == 0)                                                                        //
						pBeamformingInfo->BeamformingPeriodState+=1;        //
					else                                                                                  //
						pBeamformingInfo->BeamformingPeriodState+=2;        //
				}
			}

#ifdef MBSSID
		  if ((OPMODE & WIFI_AP_STATE) && priv->pmib->miscEntry.vap_enable) {
			for (j=0; j<RTL8192CD_NUM_VWLAN; j++) {
				if ((priv->pvap_priv[j]->assoc_num > 0) && IS_DRV_OPEN(priv->pvap_priv[j])) {
					pstat = get_stainfo(priv->pvap_priv[j], pEntry->MacAddr);
					if(pstat) {
						if(pstat->tx_avarage > (1<<16))  // 0.5Mbps
							++ret;
					}
				}
			}
		}
#endif		
#ifdef UNIVERSAL_REPEATER
		vxd_priv = GET_VXD_PRIV(priv);
		if((OPMODE & WIFI_STATION_STATE) && (vxd_priv->assoc_num > 0) && IS_DRV_OPEN(vxd_priv) 	) {		
			pstat = get_stainfo(vxd_priv, pEntry->MacAddr);
			if(pstat) {
				if(pstat->tx_avarage >  (1<<16))  // 0.5Mbps
					++ret;
			}
		}
#endif
			
		}
	}
//	panic_printk("BeamformPeriodState = %d\n", pBeamformingInfo->BeamformingPeriodState);
	return ret;
}

u1Byte
getBFeeStaNum(
	struct rtl8192cd_priv *priv
	)
{
	PRT_BEAMFORMING_INFO 		pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	int idx;
	u1Byte BFee_STA_Num = 0;
	for(idx=0 ; idx<BEAMFORMING_ENTRY_NUM; idx++){
		if(pBeamformingInfo->BeamformingEntry[idx].bUsed)
			BFee_STA_Num++;
	}
	return BFee_STA_Num;	
}


VOID
Beamforming_TimerCallback(
	struct rtl8192cd_priv *priv
	)
{
	BOOLEAN						ret = FALSE;//, timer_set=FALSE;
	PRT_BEAMFORMING_INFO 		pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	int 							idx = pBeamformingInfo->BeamformingCurIdx;		
	PRT_BEAMFORMING_ENTRY		pEntry = &(pBeamformingInfo->BeamformingEntry[idx]);
	PRT_BEAMFORMING_TIMER_INFO	pBeamformingTimerInfo = &(pBeamformingInfo->BeamformingTimerInfo[idx]);	
	u1Byte						BFee_STA_Num = 0;
	u1Byte						index = 0;

	BFee_STA_Num = getBFeeStaNum(priv);

#if 0
	for(idx=0 ; idx<BEAMFORMING_ENTRY_NUM; idx++)  
	{
#else
	{		

		if(BFee_STA_Num == 2){
			if(pBeamformingInfo->BeamformingPeriodState == 0 || pBeamformingInfo->BeamformingPeriodState == 3){
				if(pBeamformingInfo->BeamformingEntry[idx^1].bUsed)
					idx ^=1;
			}
			else if(pBeamformingInfo->BeamformingPeriodState == 2){
				idx = 1;
			}
			else
				idx = 0;
	
		}	
		else{  
			
			if(pBeamformingInfo->BeamformingEntry[0].bUsed)
				idx = 0;
			else if(pBeamformingInfo->BeamformingEntry[1].bUsed)
				idx = 1;
		}
		
		pBeamformingInfo->BeamformingCurIdx = idx;

#endif
		pEntry = &(pBeamformingInfo->BeamformingEntry[idx]);
		pBeamformingTimerInfo = &(pBeamformingInfo->BeamformingTimerInfo[idx]);
		
		if(pEntry->bBeamformingInProgress) {
			Beamforming_End(priv, 0);
			pEntry->bBeamformingInProgress = FALSE;
		}
		if( pEntry->bUsed) 
		{
			ret = BeamformingStart_V2( priv, idx,
				pBeamformingTimerInfo->Mode,
				pEntry->BW );

		}

//		if(ret && !timer_set)
//		if(ret)
		if(pBeamformingInfo->BeamformingEntry[0].bUsed || pBeamformingInfo->BeamformingEntry[1].bUsed)
		{		
			if(pBeamformingInfo->BeamformState == BEAMFORMING_STATE_START)
			{
				if(pBeamformingTimerInfo->Mode == SOUNDING_SW_VHT_TIMER || pBeamformingTimerInfo->Mode == SOUNDING_SW_HT_TIMER)  {
					if(shortenSoundingPeriod(priv)){
						if(pBeamformingInfo->BeamformingPeriodState == 1 || pBeamformingInfo->BeamformingPeriodState == 2)
							ODM_SetTimer(ODMPTR, &pBeamformingInfo->BeamformingTimer, pBeamformingTimerInfo->BeamPeriod/100);
						else   // pBeamformingInfo->BeamformingPeriodState == 3
							ODM_SetTimer(ODMPTR, &pBeamformingInfo->BeamformingTimer, pBeamformingTimerInfo->BeamPeriod/200);
					}
					else  
						ODM_SetTimer(ODMPTR, &pBeamformingInfo->BeamformingTimer, pBeamformingTimerInfo->BeamPeriod);
				}
				else{

					int BeamPeriod = priv->pshare->rf_ft_var.soundingPeriod;

					if(pBeamformingTimerInfo->Mode == SOUNDING_HW_VHT_TIMER || pBeamformingTimerInfo->Mode == SOUNDING_HW_HT_TIMER)
						BeamPeriod *=32;
	
					if(shortenSoundingPeriod(priv))
					{
						if(pBeamformingInfo->BeamformingPeriodState == 1 || pBeamformingInfo->BeamformingPeriodState == 2) //only one entry is in used
							BeamPeriod /= 100;
						else //two entries are in used
							BeamPeriod /= 200;
					}
#ifdef CONFIG_WLAN_HAL_8192EE
					if(GET_CHIP_VER(priv)== VERSION_8192E){
						if(pBeamformingTimerInfo->BeamPeriod != BeamPeriod) {
							pBeamformingTimerInfo->BeamPeriod = BeamPeriod;
							HW_VAR_HW_REG_TIMER_STOP_92E(priv);
							HW_VAR_HW_REG_TIMER_INIT_92E(priv, pBeamformingTimerInfo->BeamPeriod);
						}
						HW_VAR_HW_REG_TIMER_START_92E(priv);
					}
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
					if(GET_CHIP_VER(priv)== VERSION_8812E){
						if(pBeamformingTimerInfo->BeamPeriod != BeamPeriod) {
							pBeamformingTimerInfo->BeamPeriod = BeamPeriod;
							HW_VAR_HW_REG_TIMER_STOP_8812(priv);
							HW_VAR_HW_REG_TIMER_INIT_8812(priv, pBeamformingTimerInfo->BeamPeriod);
						}
						HW_VAR_HW_REG_TIMER_START_8812(priv);
					}
#endif

					
			
				}
//				timer_set = 1;
			}
		}	

	
	}

}

VOID Beamforming_SWTimerCallback(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	Beamforming_TimerCallback(priv);
//	mod_timer(&priv->txbf_swtimer, jiffies + priv->pshare->rf_ft_var.soundingPeriod);
}

VOID
Beamforming_Init(
	struct rtl8192cd_priv *priv
	)
{
	PRT_BEAMFORMING_INFO pBeamInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_PERIOD_INFO	pBeamPeriodInfo = &(pBeamInfo->BeamformingPeriodInfo);

	pBeamInfo->BeamformingPeriodState = 0;
	pBeamPeriodInfo->Mode = SOUNDING_STOP_OID_TIMER;

	init_timer(&pBeamInfo->BeamformingTimer);
	pBeamInfo->BeamformingTimer.function = Beamforming_SWTimerCallback;
	pBeamInfo->BeamformingTimer.data = (unsigned long)priv;
}

VOID
Beamforming_Release(
	struct rtl8192cd_priv *priv
	)
{
	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	ODM_CancelTimer(ODMPTR, &pBeamformingInfo->BeamformingTimer);
}		



VOID
Beamforming_Enter(
	struct rtl8192cd_priv *priv,
	struct stat_info	*pstat
)
{
	u1Byte	Idx = 0xff;

	if(Beamforming_InitEntry(priv, pstat, &Idx)){
#ifdef CONFIG_WLAN_HAL_8192EE
		if (GET_CHIP_VER(priv)== VERSION_8192E)	
			SetBeamformEnter92E(priv, Idx);
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
		if (GET_CHIP_VER(priv)== VERSION_8812E)	
			SetBeamformEnter8812(priv, Idx);
#endif

		Beamforming_Notify(priv, Idx);
	}	
}


VOID
Beamforming_Leave(
	struct rtl8192cd_priv *priv,
	pu1Byte			RA
	)
{
	u1Byte		Idx = 0;
	PRT_BEAMFORMING_INFO 	pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	Beamforming_GetEntryByAddr(priv, RA, &Idx);

	if(RA == NULL)
		BeamformingReset(priv);
	else
		Beamforming_DeInitEntry(priv, RA);

	Beamforming_Notify(priv, Idx);

//	RT_DISP(FBEAM, FBEAM_FUN, ("%s\n", __FUNCTION__));
}


VOID
Beamforming_SetTxBFen(
	struct rtl8192cd_priv *priv,
	u1Byte			MacId,
	BOOLEAN			bTxBF
	)
{
	u1Byte					Idx = 0;
//	PRT_BEAMFORMING_INFO 	pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_ENTRY	pEntry;

	pEntry = Beamforming_GetEntryByMacId(priv, MacId, &Idx);

	if(pEntry == NULL)
		return;
	else
		pEntry->bTxBF = bTxBF;

	Beamforming_Notify(priv, Idx);
}


VOID
Beamforming_GetNDPAFrame(
	struct rtl8192cd_priv *priv,
	pu1Byte 					pNDPAFrame
	)
{
	pu1Byte						TA ;
	u1Byte						Idx, Sequence;

	PRT_BEAMFORMING_ENTRY		pBeamformEntry = NULL;
	if (GET_CHIP_VER(priv) != VERSION_8812E)
		return;
	if(IsCtrlNDPA(pNDPAFrame) == FALSE)
		return;

	TA = GetAddr2Ptr(pNDPAFrame);
	// Remove signaling TA. 
	TA[0] = TA[0] & 0xFE; 
	pBeamformEntry = Beamforming_GetEntryByAddr(priv, TA, &Idx);
	if(pBeamformEntry == NULL)
		return;
	else if(!(pBeamformEntry->BeamformEntryCap & BEAMFORMEE_CAP_VHT_SU))
		return;
	else if(pBeamformEntry->LogSuccessCnt > 1)
		return;
	Sequence = (pNDPAFrame[16]) >> 2;
	if(pBeamformEntry->LogSeq != Sequence)
	{
		/* Previous frame doesn't retry when meet new sequence number */
		if(pBeamformEntry->LogSeq != 0xff && pBeamformEntry->LogRetryCnt == 0)
			pBeamformEntry->LogSuccessCnt++;
		pBeamformEntry->LogSeq = Sequence;
		pBeamformEntry->LogRetryCnt = 0;
	}
	else
	{
#ifdef CONFIG_RTL_8812_SUPPORT	
		if(pBeamformEntry->LogRetryCnt == 3)
			_Beamforming_CLK(priv);
#endif		
		pBeamformEntry->LogRetryCnt++;
	}
//	RT_DISP(FBEAM, FBEAM_FUN, ("%s LogSeq %d LogRetryCnt %d LogSuccessCnt %d\n", 
//			__FUNCTION__, pBeamformEntry->LogSeq, pBeamformEntry->LogRetryCnt, pBeamformEntry->LogSuccessCnt));
}

#endif

