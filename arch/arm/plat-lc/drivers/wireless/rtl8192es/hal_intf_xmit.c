/*
 *  SDIO core routines
 *
 *  $Id: hal_intf_xmit.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _HAL_INTF_XMIT_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "./8192cd.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"
#include "WlanHAL/HalHeader/HalComTXDesc.h"


#ifdef TX_SHORTCUT
int get_tx_sc_index(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri)
{
	struct tx_sc_entry *ptxsc_entry;
	int i;

	ptxsc_entry = pstat->tx_sc_ent[pktpri];
	
	for (i=0; i<TX_SC_ENTRY_NUM; i++) {
#ifdef MCAST2UI_REFINE          
		if  ((OPMODE & WIFI_AP_STATE)
#ifdef WDS			
				&& !(pstat->state & WIFI_WDS)
#endif				
			) {		
			if (!memcmp(hdr+6, &ptxsc_entry[i].ethhdr.saddr, sizeof(struct wlan_ethhdr_t)-6)) 
				return i;							
		}
		else				
#endif
		{
			if (!memcmp(hdr, &ptxsc_entry[i].ethhdr, sizeof(struct wlan_ethhdr_t)))
				return i;
		}
	}

	return -1;
}

int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri)
{
	struct tx_sc_entry *ptxsc_entry;
	int i;

	i = get_tx_sc_index(priv, pstat, hdr, pktpri);
	if (i >= 0)
		return i;
	
	ptxsc_entry = pstat->tx_sc_ent[pktpri];
	
	for (i=0; i<TX_SC_ENTRY_NUM; i++) {
		if (ptxsc_entry[i].txcfg.fr_len == 0)
			return i;
	}
	
	// no free entry
	i = pstat->tx_sc_replace_idx[pktpri];
	pstat->tx_sc_replace_idx[pktpri] = (i+1) % TX_SC_ENTRY_NUM;
	return i;
}
#endif // TX_SHORTCUT

u32 rtw_is_tx_queue_empty(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	int empty = TRUE;
	
	if (txcfg->pstat) {
		if (tx_servq_len(&txcfg->pstat->tx_queue[txcfg->q_num]) > 0)
			empty = FALSE;
	} else {	// txcfg->pstat == NULL
		if (MGNT_QUEUE == txcfg->q_num) {
			if (tx_servq_len(&priv->tx_mgnt_queue) > 0)
				empty = FALSE;
		} else if (tx_servq_len(&priv->tx_mc_queue) > 0)
			empty = FALSE;
	}
	
	return empty;
}

int rtw_xmit_enqueue(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	struct xmit_frame *pxmitframe;
	
#ifdef CONFIG_SDIO_TX_FILTER_BY_PRI
	if (OPMODE & WIFI_AP_STATE) {
		if (priv->pshare->rf_ft_var.tx_filter_enable && !priv->pshare->iot_mode_enable) {
			int qlen = priv->pshare->free_xmit_queue.qlen;
			switch (txcfg->q_num) {
				case VO_QUEUE:
					if (1 > qlen)
						return FALSE;
					break;
				case VI_QUEUE:
					if (41 > qlen)
						return FALSE;
					break;
				case BE_QUEUE:
					if (81 > qlen)
						return FALSE;
					break;
				case BK_QUEUE:
					if (121 > qlen)
						return FALSE;
					break;
			}
		}
	}
#endif

	if (NULL == (pxmitframe = rtw_alloc_xmitframe(priv))) {
		DEBUG_WARN("No more xmitframe\n");
		return FALSE;
	}
	
	memcpy(&pxmitframe->txinsn, txcfg, sizeof(struct tx_insn));
	pxmitframe->priv = priv;
	
	if (rtw_enqueue_xmitframe(priv, pxmitframe, ENQUEUE_TO_TAIL) == FALSE) {
		DEBUG_WARN("Fail to enqueue xmitframe\n");
		rtw_free_xmitframe(priv, pxmitframe);
		return FALSE;
	}
	
	return TRUE;
}

void rtw_handle_xmit_fail(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	struct xmit_buf *pxmitbuf = txcfg->pxmitbuf;
	
	if (pxmitbuf) {
		// handle error case for the first user that hold xmitbuf
		if ((txcfg == pxmitbuf->agg_start_with) && pxmitbuf->use_hw_queue) {
			txcfg->pxmitbuf = NULL;
			
#ifdef CONFIG_SDIO_HCI
			if (pxmitbuf->flags & XMIT_BUF_FLAG_REUSE) {
				local_bh_disable();
				// Release the ownership of the HW TX queue
				clear_bit(pxmitbuf->q_num, &priv->pshare->use_hw_queue_bitmap);
				
				pxmitbuf->use_hw_queue = 0;
				/* Enqueue the pxmitbuf, and it will be dequeued by a xmit thread later */
#ifdef CONFIG_SDIO_TX_MULTI_QUEUE
				rtw_sdio_enqueue_xmitbuf(priv, pxmitbuf);
#else
				enqueue_pending_xmitbuf(priv, pxmitbuf);
#endif
				local_bh_enable();
			} else
#endif // CONFIG_SDIO_HCI
			{
				pxmitbuf->use_hw_queue = 0;
				// Release the ownership of the HW TX queue
				clear_bit(pxmitbuf->q_num, &priv->pshare->use_hw_queue_bitmap);
				
				rtw_free_xmitbuf(priv, pxmitbuf);

				
				// schedule xmit_tasklet to avoid the same source TX queue not be handled
				// because this queue had enqueued packet during this processing
				if (FALSE == priv->pshare->bDriverStopped)
				     	tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
			}
		}
	}
}

static int _rtw_xmit_decision(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	if (test_and_set_bit(txcfg->q_num, &priv->pshare->use_hw_queue_bitmap) == 0)
	{
		// Got the ownership of the corresponding HW TX queue
		if (NULL == txcfg->pxmitbuf) {
			struct xmit_buf *pxmitbuf;
			
			if (txcfg->fr_type == _SKB_FRAME_TYPE_) {
#if 0//def CONFIG_SDIO_HCI
				pxmitbuf = get_usable_pending_xmitbuf(priv, txcfg);
				if (NULL == pxmitbuf)
#endif
					pxmitbuf = rtw_alloc_xmitbuf(priv, (u8)txcfg->q_num);
			} else {
				pxmitbuf = rtw_alloc_xmitbuf_ext(priv, (u8)txcfg->q_num);
			}
			
			if (NULL == pxmitbuf) {
				// Release the ownership of the HW TX queue
				clear_bit(txcfg->q_num, &priv->pshare->use_hw_queue_bitmap);
				
				if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
					return XMIT_DECISION_STOP;
				}
				return XMIT_DECISION_ENQUEUE;
			}
			
			pxmitbuf->agg_start_with = txcfg;
			txcfg->pxmitbuf = pxmitbuf;
		} else {
			BUG_ON((txcfg->pxmitbuf->q_num != txcfg->q_num) ||txcfg->pxmitbuf->use_hw_queue);
		}
		
		txcfg->pxmitbuf->use_hw_queue = 1;
	}
	else if (NULL == txcfg->pxmitbuf) {
		if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
			return XMIT_DECISION_STOP;
		}
		return XMIT_DECISION_ENQUEUE;
	}
	else {
		BUG_ON(0 == txcfg->pxmitbuf->use_hw_queue);
	}
	
	return XMIT_DECISION_CONTINUE;
}

int rtw_xmit_decision(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	int res;
	
	local_bh_disable();
	res = _rtw_xmit_decision(priv, txcfg);
	local_bh_enable();
	
	return res;
}

int dz_queue(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_insn *txcfg)
{
	if (pstat)
	{
		if (0 == pstat->expire_to)
			return FALSE;
		
		if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
			return FALSE;
		}
		
#if defined(WIFI_WMM) && defined(WMM_APSD)
		if (_SKB_FRAME_TYPE_ == txcfg->fr_type) {
			if ((QOS_ENABLE) && (APSD_ENABLE) && (pstat->QosEnabled) && (pstat->apsd_bitmap & 0x0f)) {
				int pri = ((struct sk_buff*)txcfg->pframe)->cb[1];
				
				if ((((pri == 7) || (pri == 6)) && (pstat->apsd_bitmap & 0x01))
						|| (((pri == 5) || (pri == 4)) && (pstat->apsd_bitmap & 0x02))
						|| (((pri == 0) || (pri == 3)) && (pstat->apsd_bitmap & 0x08))
						|| (((pri == 1) || (pri == 2)) && (pstat->apsd_bitmap & 0x04)))
					pstat->apsd_pkt_buffering = 1;
			}
		}
#endif
		return TRUE;
	}
	else {	// Multicast or Broadcast or class 1 frame
		if (rtw_xmit_enqueue(priv, txcfg) == TRUE) {
			priv->pkt_in_dtimQ = 1;
			return TRUE;
		}
	}

	return FALSE;
}

#define MAX_FRAG_NUM		16
int update_txinsn_stage1(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct sk_buff 	*pskb=NULL;
	unsigned short  protocol;
	unsigned char   *da=NULL;
	struct stat_info	*pstat=NULL;
	int priority=0;
	
	if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE || txcfg->aggre_en == FG_AGGRE_MSDU_LAST)
		return TRUE;

	/*---frag_thrshld setting---plus tune---0115*/
#ifdef	WDS
	if (txcfg->wdsIdx >= 0){
		txcfg->frag_thrshld = 2346; // if wds, disable fragment
	}else
#endif
#ifdef CONFIG_RTK_MESH
	if(txcfg->is_11s){
		txcfg->frag_thrshld = 2346; // if Mesh case, disable fragment
	}else
#endif
	{
		txcfg->frag_thrshld = FRAGTHRSLD - _CRCLNG_;
	}
	/*---frag_thrshld setting---end*/

	txcfg->rts_thrshld  = RTSTHRSLD;
	
	txcfg->privacy = txcfg->iv = txcfg->icv = txcfg->mic = 0;
	txcfg->frg_num = 0;
	txcfg->need_ack = 1;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
	{
		pskb = ((struct sk_buff *)txcfg->pframe);
		txcfg->fr_len = pskb->len - WLAN_ETHHDR_LEN;
		
		protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));
		if ((protocol + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN)
			txcfg->llc = sizeof(struct llc_snap);
		
#ifdef MP_TEST
		if (OPMODE & WIFI_MP_STATE) {
			txcfg->hdr_len = WLAN_HDR_A3_LEN;
			txcfg->frg_num = 1;
			if (IS_MCAST(pskb->data))
				txcfg->need_ack = 0;
			return TRUE;
		}
#endif

#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			pstat = get_stainfo(priv, priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr);
			if (pstat == NULL) {
				DEBUG_ERR("TX DROP: %s: get_stainfo() for wds failed [%d]!\n", (char *)__FUNCTION__, txcfg->wdsIdx);
				return FALSE;
			}

			txcfg->privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
			switch (txcfg->privacy) {
				case _WEP_40_PRIVACY_:
				case _WEP_104_PRIVACY_:
					txcfg->iv = 4;
					txcfg->icv = 4;
					break;
				case _TKIP_PRIVACY_:
					txcfg->iv = 8;
					txcfg->icv = 4;
					txcfg->mic = 0;
					break;
				case _CCMP_PRIVACY_:
					txcfg->iv = 8;
					txcfg->icv = 0;
					txcfg->mic = 8;
					break;
			}
			txcfg->frg_num = 1;
			if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) {
				priority = get_skb_priority(priv, pskb, pstat);
#ifdef RTL_MANUAL_EDCA
				txcfg->q_num = PRI_TO_QNUM(priv, priority);
#else
				PRI_TO_QNUM(priority, txcfg->q_num, priv->pmib->dot11OperationEntry.wifi_specific);
#endif
			}
			
			txcfg->need_ack = 1;
			txcfg->pstat = pstat;
#ifdef WIFI_WMM
			if ((QOS_ENABLE) && (pstat->QosEnabled))
				txcfg->hdr_len = WLAN_HDR_A4_QOS_LEN;
			else
#endif
			txcfg->hdr_len = WLAN_HDR_A4_LEN;
		
			return TRUE;
		}
#endif // WDS

		if (OPMODE & WIFI_AP_STATE) {
#ifdef MCAST2UI_REFINE
			pstat = get_stainfo(priv, &pskb->cb[10]);
#else
			pstat = get_stainfo(priv, pskb->data);
#endif
#ifdef A4_STA
			if (pstat == NULL) {
				if (txcfg->pstat && (txcfg->pstat->state & WIFI_A4_STA)) 
					pstat = txcfg->pstat;
				else if (!IS_MCAST(pskb->data) &&  priv->pshare->rf_ft_var.a4_enable)
					pstat = a4_sta_lookup(priv, pskb->data);
				if (pstat) 
					da = pstat->hwaddr;
			}
#endif
		}
#ifdef CLIENT_MODE
		else if (OPMODE & WIFI_STATION_STATE)
			pstat = get_stainfo(priv, BSSID);
		else if (OPMODE & WIFI_ADHOC_STATE)
			pstat = get_stainfo(priv, pskb->data);
#endif

#ifdef WIFI_WMM
		if ((pstat) && (QOS_ENABLE) && (pstat->QosEnabled)) {
			txcfg->hdr_len = WLAN_HDR_A3_QOS_LEN;
		}
		else
#endif
		{
			txcfg->hdr_len = WLAN_HDR_A3_LEN;
		}

#ifdef CONFIG_RTK_MESH
		if(txcfg->is_11s)
		{
			txcfg->hdr_len = WLAN_HDR_A4_QOS_LEN ;
			da = txcfg->nhop_11s;
		}
		else
#endif

#ifdef A4_STA
		if (!da)
#endif
#ifdef MCAST2UI_REFINE
			da = &pskb->cb[10];
#else
			da = pskb->data;
#endif

		//check if da is associated, if not, just drop and return false
		if (!IS_MCAST(da)
#ifdef CLIENT_MODE
			|| (OPMODE & WIFI_STATION_STATE)
#endif
#ifdef A4_STA
			|| (pstat && (pstat->state & WIFI_A4_STA))
#endif
			)
		{
			if ((pstat == NULL) || (!(pstat->state & WIFI_ASOC_STATE)))
			{
				DEBUG_ERR("TX DROP: Non asoc tx request!\n");
				return FALSE;
			}
#ifdef A4_STA
			if (pstat->state & WIFI_A4_STA)
				txcfg->hdr_len += WLAN_ADDR_LEN;
#endif

			if (((protocol == 0x888E) && ((GET_UNICAST_ENCRYP_KEYLEN == 0)
#ifdef WIFI_SIMPLE_CONFIG
				|| (pstat->state & WIFI_WPS_JOIN)
#endif
				))
#ifdef CONFIG_RTL_WAPI_SUPPORT
				 || (protocol == ETH_P_WAPI)
#endif
#ifdef BEAMFORMING_SUPPORT
				|| (txcfg->ndpa) 
#endif
				)
				txcfg->privacy = 0;
			else
				txcfg->privacy = get_privacy(priv, pstat, &txcfg->iv, &txcfg->icv, &txcfg->mic);
			
			if ((OPMODE & WIFI_AP_STATE) && !IS_MCAST(da) && (isDHCPpkt(pskb) == TRUE))
				txcfg->is_dhcp = 1;
			
			if (txcfg->aggre_en <= FG_AGGRE_MSDU_FIRST) {
#ifdef CONFIG_RTK_MESH
				priority = get_skb_priority3(priv, pskb, txcfg->is_11s, pstat);
#else
				priority = get_skb_priority(priv, pskb, pstat);
#endif
#ifdef RTL_MANUAL_EDCA
				txcfg->q_num = PRI_TO_QNUM(priv, priority);
#else
				PRI_TO_QNUM(priority, txcfg->q_num, priv->pmib->dot11OperationEntry.wifi_specific);
#endif
			}

#ifdef CONFIG_RTL_WAPI_SUPPORT
			if (protocol==ETH_P_WAPI)
			{
				txcfg->q_num = MGNT_QUEUE;
			}
#endif
		}
		else
		{
			// if group key not set yet, don't let unencrypted multicast go to air
			if (priv->pmib->dot11GroupKeysTable.dot11Privacy) {
				if (GET_GROUP_ENCRYP_KEYLEN == 0) {
					DEBUG_ERR("TX DROP: group key not set yet!\n");
					return FALSE;
				}
			}

			txcfg->privacy = get_mcast_privacy(priv, &txcfg->iv, &txcfg->icv, &txcfg->mic);
			
			txcfg->q_num = priv->tx_mc_queue.q_num;

			if ((*da) == 0xff)	// broadcast
				txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
			else {				// multicast
				if (priv->pmib->dot11StationConfigEntry.lowestMlcstRate)
					txcfg->tx_rate = get_rate_from_bit_value(priv->pmib->dot11StationConfigEntry.lowestMlcstRate);
				else
					txcfg->tx_rate = find_rate(priv, NULL, 1, 1);
			}

#ifdef CONFIG_RTK_MESH
			mesh_debug_tx4(priv, txcfg);
#endif

			txcfg->lowest_tx_rate = txcfg->tx_rate;
			txcfg->fixed_rate = 1;
		}
	}
#ifdef _11s_TEST_MODE_	/*---11s mgt frame---*/
	else if (txcfg->is_11s)
		mesh_debug_tx6(priv, txcfg);
#endif
	
	if (!da)
	{
		// This is non data frame, no need to frag.
#ifdef CONFIG_RTK_MESH
		if(txcfg->is_11s)
			da = GetAddr1Ptr(txcfg->phdr);
		else
#endif
			da = get_da((unsigned char *) (txcfg->phdr));
		
#ifdef CLIENT_MODE
		if ((OPMODE & WIFI_AP_STATE) || (OPMODE & WIFI_ADHOC_STATE))
#else
		if (OPMODE & WIFI_AP_STATE)
#endif
		{
			if (IS_MCAST(da) == FALSE) {
				pstat = get_stainfo(priv, da);
			}
		}
#ifdef CLIENT_MODE
		else if (OPMODE & WIFI_STATION_STATE)
		{
			pstat = get_stainfo(priv, BSSID);
		}
#endif

		txcfg->frg_num = 1;

		if (IS_MCAST(da))
			txcfg->need_ack = 0;
		else
			txcfg->need_ack = 1;

		if (GetPrivacy(txcfg->phdr))
		{
			// only auth with legacy wep...
			txcfg->iv = 4;
			txcfg->icv = 4;
			txcfg->privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;
		}

#ifdef DRVMAC_LB
		if (GetFrameType(txcfg->phdr) == WIFI_MGT_TYPE)
#endif
		if (txcfg->fr_len != 0)	//for mgt frame
			txcfg->hdr_len += WLAN_HDR_A3_LEN;
	}
	
	txcfg->is_mcast = IS_MCAST(da);
	
#ifdef CLIENT_MODE
	if ((OPMODE & WIFI_AP_STATE) || (OPMODE & WIFI_ADHOC_STATE))
#else
	if (OPMODE & WIFI_AP_STATE)
#endif
	{
		if (IS_MCAST(da))
		{
			txcfg->frg_num = 1;
			txcfg->need_ack = 0;
			txcfg->rts_thrshld = 10000;
		}
		else
		{
			txcfg->pstat = pstat;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE)
	{
		txcfg->pstat = pstat;
	}
#endif

#ifdef BEAMFORMING_SUPPORT
	if((priv->pmib->dot11RFEntry.txbf == 1) && (pstat) &&
		((pstat->ht_cap_len && (pstat->ht_cap_buf.txbf_cap)) 
#ifdef RTK_AC_SUPPORT		
		||(pstat->vht_cap_len && (cpu_to_le32(pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)))
#endif		
		))
		Beamforming_GidPAid(priv, pstat);
#endif

	txcfg->frag_thrshld -= (txcfg->mic + txcfg->iv + txcfg->icv + txcfg->hdr_len);

	if (txcfg->frg_num == 0)
	{
		if (txcfg->aggre_en > 0)
			txcfg->frg_num = 1;
		else {
			// how many mpdu we need...
			int size;
			
			size = txcfg->fr_len + txcfg->llc + ((_TKIP_PRIVACY_ == txcfg->privacy) ? 8 : 0);
			txcfg->frg_num = (size + txcfg->frag_thrshld -1) / txcfg->frag_thrshld;
			if (unlikely(txcfg->frg_num > MAX_FRAG_NUM)) {
				txcfg->frag_thrshld = 2346;
				txcfg->frg_num = 1;
			}
		}
	}

	return TRUE;
}

int update_txinsn_stage2(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct sk_buff 	*pskb=NULL;
	unsigned short  protocol;
	struct stat_info	*pstat=NULL;
	int priority=0;

	if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE || txcfg->aggre_en == FG_AGGRE_MSDU_LAST)
		return TRUE;
	
	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
	{
#ifdef MP_TEST
		if (OPMODE & WIFI_MP_STATE) {
			return TRUE;
		}
#endif
		pskb = ((struct sk_buff *)txcfg->pframe);
		pstat = txcfg->pstat;

#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			if (txcfg->privacy == _TKIP_PRIVACY_)
				txcfg->fr_len += 8;	// for Michael padding.
				
			txcfg->tx_rate = get_tx_rate(priv, pstat);
			txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
			if (priv->pmib->dot11WdsInfo.entry[pstat->wds_idx].txRate)
				txcfg->fixed_rate = 1;

			if (txcfg->aggre_en == 0) {
				if ((pstat->aggre_mthd == AGGRE_MTHD_MPDU) && is_MCS_rate(txcfg->tx_rate))
					txcfg->aggre_en = FG_AGGRE_MPDU;
			}

			return TRUE;
		}
#endif

		//check if da is associated, if not, just drop and return false
		if (!txcfg->is_mcast
#ifdef CLIENT_MODE
			|| (OPMODE & WIFI_STATION_STATE)
#endif
#ifdef A4_STA
			|| (pstat && (pstat->state & WIFI_A4_STA))
#endif		
			)
		{
			protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));

			if ((protocol == 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
				||(protocol == ETH_P_WAPI)
#endif
				|| txcfg->is_dhcp) {
				// use basic rate to send EAP packet for sure
				txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
				txcfg->lowest_tx_rate = txcfg->tx_rate;
				txcfg->fixed_rate = 1;
			} else {
				txcfg->tx_rate = get_tx_rate(priv, pstat);
				txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
				if (!is_auto_rate(priv, pstat)&&
					!(should_restrict_Nrate(priv, pstat) && is_fixedMCSTxRate(priv, pstat)))
					txcfg->fixed_rate = 1;
			}

			if (txcfg->aggre_en == 0
#ifdef SUPPORT_TX_MCAST2UNI
					&& (priv->pshare->rf_ft_var.mc2u_disable || (pskb->cb[2] != (char)0xff))
#endif
				) {
				if ((pstat->aggre_mthd == AGGRE_MTHD_MPDU) &&
				/*	is_MCS_rate(txcfg->tx_rate) &&*/ (protocol != 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
					&& (protocol != ETH_P_WAPI)
#endif
					&& !txcfg->is_dhcp)
					txcfg->aggre_en = FG_AGGRE_MPDU;
			}

			if (
#ifdef RTK_AC_SUPPORT //FOR_VHT5G_PF
				(txcfg->aggre_en && txcfg->pstat && 
				((txcfg->pstat->aggre_mthd == AGGRE_MTHD_MPDU_AMSDU) || (txcfg->pstat->aggre_mthd == AGGRE_MTHD_MPDU))) ||
#endif
				(txcfg->aggre_en >= FG_AGGRE_MPDU && txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST))
			{
				priority = pskb->cb[1];
				if (!pstat->ADDBA_ready[priority]) {
					if ((pstat->ADDBA_req_num[priority] < 5) && !pstat->ADDBA_sent[priority]) {
						pstat->ADDBA_req_num[priority]++;
						issue_ADDBAreq(priv, pstat, priority);
						pstat->ADDBA_sent[priority]++;
					}
				}
			}

			if (txcfg->is_pspoll && (tx_servq_len(&pstat->tx_queue[BE_QUEUE]) > 0)) {
				SetMData(txcfg->phdr);
			}
		} else {
			if ((OPMODE & WIFI_AP_STATE) && !list_empty(&priv->sleep_list)
					&& (tx_servq_len(&priv->tx_mc_queue) > 0)) {
				SetMData(txcfg->phdr);
			}
		}
	}

	if (txcfg->privacy == _TKIP_PRIVACY_)
		txcfg->fr_len += 8;	// for Michael padding.

	if (txcfg->aggre_en > 0) {
		txcfg->frg_num = 1;
		txcfg->frag_thrshld = 2346;
	}
	
	return TRUE;
}

#ifdef CONFIG_SDIO_HCI
static inline void update_remaining_timeslot(struct rtl8192cd_priv *priv, struct tx_desc_info *pdescinfo, unsigned int q_num, unsigned char is_40m_bw)
{
	const int RATE_20M_2SS_LGI[] = {
		13, 26, 39, 52, 78, 104, 117, 130,	// 1SS
		26, 52, 78, 104, 156, 208, 234, 260	// 2SS
	};
	const int RATE_40M_2SS_LGI[] = {
		27, 54, 81, 108, 162, 216, 243, 270,	// 1SS
		54, 108, 162, 216, 324, 432, 486, 540	// 2SS
	};

	unsigned int txRate = 0;
	unsigned int ts_consumed = 0;
	struct tx_servq *ptxservq;
	struct stat_info *pstat = pdescinfo->pstat;

	if (pstat) {
		ptxservq = &pstat->tx_queue[q_num];
	} else if (MGNT_QUEUE == q_num) {
		ptxservq = &priv->tx_mgnt_queue;
	} else {
		return; // skip checking tx_mc_queue
	}

	if (rtw_is_list_empty(&ptxservq->tx_pending))
		return;

	if (is_MCS_rate(pdescinfo->rate)) {
		if (is_40m_bw)
			txRate = RATE_40M_2SS_LGI[pdescinfo->rate - HT_RATE_ID];
		else
			txRate = RATE_20M_2SS_LGI[pdescinfo->rate - HT_RATE_ID];
	} else {
		txRate = pdescinfo->rate;
	}

	ts_consumed = ((pdescinfo->buf_len)*16)/(txRate); // unit: microsecond (us)

	priv->pshare->ts_used[q_num] += ts_consumed;

	ptxservq->ts_used += ts_consumed;
}
#endif

void rtl8192cd_hal_fill_fwinfo(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, struct tx_desc  *pdesc, unsigned int frag_idx)
{
	char n_txshort = 0, bg_txshort = 0;
	//int erp_protection = 0, n_protection = 0;
	//unsigned char rate;
	unsigned char txRate = 0;
#ifdef DRVMAC_LB
	static unsigned int rate_select = 0;
#endif
	bool			bRtsEnable = FALSE;
	bool			bErpProtect = FALSE;
	bool			bNProtect = FALSE;
	bool			bHwRts = FALSE;
	bool			bCts2SelfEnable = FALSE;
	unsigned char RtsRate;
	unsigned int	value;

#ifdef MP_TEST
	if (OPMODE & WIFI_MP_STATE) {
#ifdef RTK_AC_SUPPORT
		if (is_VHT_rate(txcfg->tx_rate)) {
			txRate = (txcfg->tx_rate - VHT_RATE_ID) + 44;
		} else
#endif
		if (is_MCS_rate(txcfg->tx_rate)) {	// HT rates
			txRate = (txcfg->tx_rate - HT_RATE_ID) + 12;
		}
		else{
			txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
		}

		if (priv->pshare->CurrentChannelBW) {
			if (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20_40)
				pdesc->Dword5 |= set_desc((1 << TX_DW5_DATA_BW_SH)
					| (0 << TX_DW5_DATA_SC_SH) | (0 << TX_DW5_RTS_SC_SH));
			else if (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_80)
				pdesc->Dword5 |= set_desc((2 << TX_DW5_DATA_BW_SH)
					| (0 << TX_DW5_DATA_SC_SH) | (0 << TX_DW5_RTS_SC_SH));
			
			if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M)
				n_txshort = 1;
		}
		else {
			if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M)
				n_txshort = 1;
		}

		if (txcfg->retry)
			pdesc->Dword4 |= set_desc((1 << TX_DW4_RTY_LMT_EN_SH)
				| ((txcfg->retry & TX_DW4_DATA_RT_LMT_MSK) << TX_DW4_DATA_RT_LMT_SH));

		if (n_txshort == 1)
			pdesc->Dword5 |= set_desc(1 << TX_DW5_DATA_SHORT_SH);

		pdesc->Dword4 |= set_desc((txRate & TX_DW4_DATARATE_MSK) << TX_DW4_DATARATE_SH);
		
#ifdef RTK_5G_SUPPORT
		if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
			pdesc->Dword4 |= set_desc((4 & TX_DW4_RTSRATE_MSK) << TX_DW4_RTSRATE_SH);
		}
#endif

		return;
	}
#endif

	if (is_MCS_rate(txcfg->tx_rate))	// HT rates
	{
#ifdef RTK_AC_SUPPORT
		if (is_VHT_rate(txcfg->tx_rate))
			txRate = (txcfg->tx_rate - VHT_RATE_ID) + 44;
		else
#endif
			txRate = (txcfg->tx_rate - HT_RATE_ID) + 12;

#ifdef RTK_AC_SUPPORT
		if (priv->pshare->is_40m_bw==HT_CHANNEL_WIDTH_80)
		{
			if(txcfg->pstat && (txcfg->pstat->tx_bw == HT_CHANNEL_WIDTH_80
				&& (is_auto_rate(priv, txcfg->pstat) || is_fixedVHTTxRate(priv, txcfg->pstat))
				) )
			{
				pdesc->Dword5 |= set_desc((2 << TX_DW5_DATA_BW_SH)
					| (0 << TX_DW5_DATA_SC_SH) | (0 << TX_DW5_RTS_SC_SH));
				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M && //todo, add shortGI 80M option
						txcfg->pstat && (txcfg->pstat->vht_cap_buf.vht_cap_info & cpu_to_le32(BIT(5))))
					n_txshort = 1;
			} // TODO: Pedro, in 8812: is_MCS_rate(txcfg->tx_rate)
			else if(txcfg->pstat && (txcfg->pstat->tx_bw >= HT_CHANNEL_WIDTH_20_40
				&& (is_auto_rate(priv, txcfg->pstat) || is_fixedMCSTxRate(priv, txcfg->pstat))        
				) )
			{
				pdesc->Dword5 |= set_desc((1 << TX_DW5_DATA_BW_SH)
					| ((priv->pshare->txsc_40 & TX_DW5_DATA_SC_MSK) << TX_DW5_DATA_SC_SH)
					| ((priv->pshare->txsc_40 & TX_DW5_RTS_SC_MSK) << TX_DW5_RTS_SC_SH));
				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
						txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
					n_txshort = 1;
			}
			else
			{
				pdesc->Dword5 |= set_desc((0 << TX_DW5_DATA_BW_SH)
					| ((priv->pshare->txsc_20 & TX_DW5_DATA_SC_MSK) << TX_DW5_DATA_SC_SH)
					| ((priv->pshare->txsc_20 & TX_DW5_RTS_SC_MSK) << TX_DW5_RTS_SC_SH));
				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
						txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
					n_txshort = 1;
			}
		}
		else
#endif
		if (priv->pshare->is_40m_bw) {
			if (txcfg->pstat && (txcfg->pstat->tx_bw == HT_CHANNEL_WIDTH_20_40)
#ifdef WIFI_11N_2040_COEXIST
				&& !(priv->pmib->dot11nConfigEntry.dot11nCoexist && (((OPMODE & WIFI_AP_STATE) && 
				(priv->bg_ap_timeout ||orForce20_Switch20Map(priv)
				))
#ifdef CLIENT_MODE
				|| ((OPMODE & WIFI_STATION_STATE) && priv->coexist_connection && 
				(txcfg->pstat->ht_ie_len) && !(txcfg->pstat->ht_ie_buf.info0 & _HTIE_STA_CH_WDTH_))
#endif
				))
#endif

				) {
				pdesc->Dword5 |= set_desc((1 << TX_DW5_DATA_BW_SH) | (3 << TX_DW5_DATA_SC_SH));

				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
						txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
					n_txshort = 1;
			}
			else {
				if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW)
					pdesc->Dword5 |= set_desc((2 << TX_DW5_DATA_SC_SH) | (2 << TX_DW5_RTS_SC_SH));
				else
					pdesc->Dword5 |= set_desc((1 << TX_DW5_DATA_SC_SH) | (1 << TX_DW5_RTS_SC_SH));

				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
					txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
					n_txshort = 1;
			}
		} else {
			if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
				txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
				n_txshort = 1;
		}

		if ((AMSDU_ENABLE && AMPDU_ENABLE && txcfg->aggre_en)
				|| ((txcfg->aggre_en >= FG_AGGRE_MPDU) && (txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST))) {
			int TID = ((struct sk_buff *)txcfg->pframe)->cb[1];
			if (txcfg->pstat->ADDBA_ready[TID] && !txcfg->pstat->low_tp_disable_ampdu) {
				pdesc->Dword2 |= set_desc(1 << TX_DW2_AGG_EN_SH);
				
				// assign aggr density
				if (txcfg->privacy) {
					pdesc->Dword2 |= set_desc(7 << TX_DW2_AMPDU_DENSITY_SH); 
				}
				else {
					value = (txcfg->pstat->ht_cap_buf.ampdu_para & _HTCAP_AMPDU_SPC_MASK_) >> _HTCAP_AMPDU_SPC_SHIFT_;
					pdesc->Dword2 |= set_desc((value & TX_DW2_AMPDU_DENSITY_MSK) << TX_DW2_AMPDU_DENSITY_SH);
				}
				value = ((1<<(txcfg->pstat->ht_cap_buf.ampdu_para & 0x03))*5) >>1;
				pdesc->Dword3 |= set_desc((value & TX_DW3_MAX_AGG_NUM_MSK) << TX_DW3_MAX_AGG_NUM_SH);
			}
			//set Break
			if ((txcfg->q_num >=1 && txcfg->q_num <=4)){
				if ((txcfg->pstat != priv->pshare->CurPstat[txcfg->q_num-1])) {
					pdesc->Dword2 |= set_desc(1 << TX_DW2_BK_SH);
					priv->pshare->CurPstat[txcfg->q_num-1] = txcfg->pstat;
				}
			} else {
				pdesc->Dword2 |= set_desc(1 << TX_DW2_BK_SH);
			}
		}

		// for STBC
	        if (priv->pmib->dot11nConfigEntry.dot11nSTBC && (txcfg->pstat)
				&& ((txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_RX_STBC_CAP_)) 
#ifdef RTK_AC_SUPPORT			
				|| (txcfg->pstat->vht_cap_buf.vht_cap_info & cpu_to_le32(_VHTCAP_RX_STBC_CAP_))
#endif
				)){
#ifdef BEAMFORMING_SUPPORT
			u1Byte					Idx = 0;
			PRT_BEAMFORMING_ENTRY	pEntry; 
			pEntry = Beamforming_GetEntryByMacId(priv, txcfg->pstat->aid, &Idx);
			if (pEntry == NULL)
#endif
			if ((get_rf_mimo_mode(priv) == MIMO_2T2R) || (get_rf_mimo_mode(priv) == MIMO_3T3R))
           			pdesc->Dword5 |= set_desc(1 << TX_DW5_DATA_STBC_SH);
	        }

		// LDPC
#ifdef BEAMFORMING_SUPPORT
		if (!txcfg->ndpa) 
#endif		
		if ((priv->pmib->dot11nConfigEntry.dot11nLDPC) && (txcfg->pstat) && (!txcfg->pstat->disable_ldpc)
				&& ((txcfg->pstat->ht_cap_len && cpu_to_le16(txcfg->pstat->ht_cap_buf.ht_cap_info) & _HTCAP_SUPPORT_RX_LDPC_)
#ifdef RTK_AC_SUPPORT		
				|| (txcfg->pstat->vht_cap_len && (cpu_to_le32(txcfg->pstat->vht_cap_buf.vht_cap_info) & BIT(RX_LDPC_E)))
#endif		
				)) {
			pdesc->Dword5 |= set_desc(1 << TX_DW5_DATA_LDPC_SH);
		}
	}
	else	// legacy rate
	{
		txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
		if (is_CCK_rate(txcfg->tx_rate) && (txcfg->tx_rate != 2)) {
			if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
					(priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
				; // txfw->txshort = 0
			else {
				if (txcfg->pstat)
					bg_txshort = (priv->pmib->dot11RFEntry.shortpreamble) &&
									(txcfg->pstat->useShortPreamble);
				else
					bg_txshort = priv->pmib->dot11RFEntry.shortpreamble;
			}
		}

#ifdef RTK_AC_SUPPORT
		if (priv->pshare->is_40m_bw==2) {
			pdesc->Dword5 |= set_desc(((priv->pshare->txsc_20 & TX_DW5_DATA_SC_MSK) << TX_DW5_DATA_SC_SH)
				| ((priv->pshare->txsc_20 & TX_DW5_RTS_SC_MSK) << TX_DW5_RTS_SC_SH));
		}
		else
#endif
		if (priv->pshare->is_40m_bw) {
			if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW) {
				pdesc->Dword5 |= set_desc((2 << TX_DW5_DATA_SC_SH) | (2 << TX_DW5_RTS_SC_SH));
			} else {
				pdesc->Dword5 |= set_desc((1 << TX_DW5_DATA_SC_SH) | (1 << TX_DW5_RTS_SC_SH));
			}
		}

		if (bg_txshort)
			pdesc->Dword5 |= set_desc(1 << TX_DW5_DATA_SHORT_SH);
	}

	if (txcfg->need_ack) { // unicast
		if (frag_idx == 0) 
			RtsCheck(priv, txcfg, &bRtsEnable, &bCts2SelfEnable, &bHwRts, &bErpProtect, &bNProtect);
	}

	RtsRate = find_rts_rate(priv, txcfg->tx_rate, bErpProtect);
	if (bRtsEnable && CheckCts2SelfEnable(RtsRate))
	{
		bRtsEnable = FALSE;
		bCts2SelfEnable = TRUE;
	}

//	if(bErpProtect)
//		priv->pshare->phw->bErpProtection = TRUE;

	if (bRtsEnable)
		pdesc->Dword3 |= set_desc(1 << TX_DW3_RTSEN_SH);
	if (bCts2SelfEnable)
		pdesc->Dword3 |= set_desc(1 << TX_DW3_CTS2SELF_SH);
	if (bHwRts)
		pdesc->Dword3 |=set_desc(1 << TX_DW3_HW_RTS_EN_SH);
		
	if (bRtsEnable || bCts2SelfEnable) {
		unsigned int rtsTxRateIdx  = get_rate_index_from_ieee_value(RtsRate);
		if (bErpProtect) {
			unsigned char  rtsShort = 0;
			if (is_CCK_rate(RtsRate) && (RtsRate != 2)) {
				if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
						(priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
					rtsShort = 0; // do nothing
				else {
					if (txcfg->pstat)
						rtsShort = (priv->pmib->dot11RFEntry.shortpreamble) &&
										(txcfg->pstat->useShortPreamble);
					else
						rtsShort = priv->pmib->dot11RFEntry.shortpreamble;
				}
			}
			pdesc->Dword5 |= (rtsShort == 1)? set_desc(1 << TX_DW5_RTS_SHORT_SH): 0;
		} 
		
		pdesc->Dword4 |= set_desc((rtsTxRateIdx & TX_DW4_RTSRATE_MSK) << TX_DW4_RTSRATE_SH);
		pdesc->Dword4 |= set_desc((0xf & TX_DW4_RTS_RATEFB_LMT_MSK) << TX_DW4_RTS_RATEFB_LMT_SH);
	}

	if (n_txshort == 1 && txcfg->pstat && (txcfg->pstat->sta_in_firmware == 1))
		pdesc->Dword5 |= set_desc(1 << TX_DW5_DATA_SHORT_SH);

#ifdef DRVMAC_LB
	if (priv->pmib->miscEntry.drvmac_lb && (priv->pmib->miscEntry.lb_mlmp == 4)) {
		txRate = rate_select;
		if (rate_select++ > 0x1b)
			rate_select = 0;

		pdesc->Dword3 |= set_desc(1 << TX_DW3_USERATE_SH);
		pdesc->Dword3 |= set_desc(1 << TX_DW3_DISDATAFB_SH);
		pdesc->Dword3 |= set_desc(1 << TX_DW3_DISRTSFB_SH);// disable RTS fall back
	}
#endif

	if ((priv->pshare->rf_ft_var.txforce != 0xff)
#ifdef BEAMFORMING_SUPPORT
			&& (!txcfg->ndpa) 
#endif
			) {
		pdesc->Dword3 |= set_desc(1 << TX_DW3_USERATE_SH);
		pdesc->Dword4 |= set_desc((priv->pshare->rf_ft_var.txforce & TX_DW4_DATARATE_MSK) << TX_DW4_DATARATE_SH);
	} else {
		pdesc->Dword4 |= set_desc((txRate & TX_DW4_DATARATE_MSK) << TX_DW4_DATARATE_SH);
	}
	
	if (priv->pshare->rf_ft_var.rts_init_rate) {
		pdesc->Dword4 |= set_desc((priv->pshare->rf_ft_var.rts_init_rate & TX_DW4_RTSRATE_MSK) << TX_DW4_RTSRATE_SH);
	}
	if ((priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G) && 
			((get_desc(pdesc->Dword4) >> TX_DW4_RTSRATE_SH) & TX_DW4_RTSRATE_MSK)<4) {
		pdesc->Dword4 &= ~ set_desc(TX_DW4_RTSRATE_MSK << TX_DW4_RTSRATE_SH);
		pdesc->Dword4 |= set_desc(4 << TX_DW4_RTSRATE_SH);
	}

	if (txcfg->need_ack) {
		// give retry limit to management frame
		if (txcfg->q_num == MANAGE_QUE_NUM) {
			pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
			if (GetFrameSubType(txcfg->phdr) == WIFI_PROBERSP) {
				;	// 0 no need to set
			}
#ifdef WDS
			else if ((GetFrameSubType(txcfg->phdr) == WIFI_PROBEREQ) && (txcfg->pstat->state & WIFI_WDS)) {
				pdesc->Dword4 |= set_desc(2 << TX_DW4_DATA_RT_LMT_SH);
			}
#endif
			else {
				pdesc->Dword4 |= set_desc(6 << TX_DW4_DATA_RT_LMT_SH);
			}
		}
#ifdef WDS
		else if (txcfg->wdsIdx >= 0) {
			if (txcfg->pstat->rx_avarage == 0) {
				pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
				pdesc->Dword4 |= set_desc(3 << TX_DW4_DATA_RT_LMT_SH);
			}
		}
#endif
		else if (txcfg->pstat) {
			if (txcfg->pstat->rssi && (txcfg->pstat->rssi < 30)) {
				if ((txcfg->pstat->current_tx_rate <= _6M_RATE_) || (_MCS0_RATE_ == txcfg->pstat->current_tx_rate)) {
					pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
					pdesc->Dword4 |= set_desc((0x04 & TX_DW4_DATA_RT_LMT_MSK) << TX_DW4_DATA_RT_LMT_SH);
				} else {
					pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
					pdesc->Dword4 |= set_desc((0x0c & TX_DW4_DATA_RT_LMT_MSK) << TX_DW4_DATA_RT_LMT_SH);
				}
			} else
			if (is_MCS_rate(txcfg->pstat->current_tx_rate) && (txcfg->pstat->IOTPeer==HT_IOT_PEER_INTEL) && (txcfg->pstat->retry_inc)
					&& !(txcfg->pstat->leave) && priv->pshare->intel_rty_lmt) {
				pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
				pdesc->Dword4 |= set_desc((priv->pshare->intel_rty_lmt & TX_DW4_DATA_RT_LMT_MSK) << TX_DW4_DATA_RT_LMT_SH);
			}
			else if ((txcfg->pstat->IOTPeer==HT_IOT_PEER_BROADCOM) && (txcfg->pstat->retry_inc) && !(txcfg->pstat->leave)) {
				pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
				pdesc->Dword4 |= set_desc(0x20 << TX_DW4_DATA_RT_LMT_SH);
			}
		}

		// High power mechanism
		if (priv->pshare->rf_ft_var.tx_pwr_ctrl && txcfg->pstat && (txcfg->fr_type == _SKB_FRAME_TYPE_)) {
			if (txcfg->pstat->hp_level == 1) {
#if defined(CONFIG_WLAN_HAL_8192EE) && defined(HIGH_POWER_EXT_PA)
				if (GET_CHIP_VER(priv)==VERSION_8192E && priv->pshare->rf_ft_var.use_ext_pa)
					pdesc->Dword5 |= set_desc(3 << TX_DW5_TXPWR_OFSET_SH); // -11 dB
				else
#endif
					pdesc->Dword5 |= set_desc(2 << TX_DW5_TXPWR_OFSET_SH); // -7 dB
			}
		}
		
#ifdef BEAMFORMING_SUPPORT
		if (txcfg->ndpa) {
			unsigned char *pwlhdr = (unsigned char*)txcfg->phdr;
			if (IsCtrlNDPA(pwlhdr) || IsMgntNDPA(pwlhdr)) {
				pdesc->Dword4 |= set_desc(5 << TX_DW4_DATA_RT_LMT_SH);
				pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
				if(IsMgntNDPA(pwlhdr)) {		//0xe0
					pdesc->Dword3 |= set_desc(1 << TX_DW3_NDPA_SH);
				} else {		// 0x54
					if(!IS_TEST_CHIP(priv)) {
						pdesc->Dword3 |= set_desc(2 << TX_DW3_NDPA_SH);
					} else {
						pdesc->Dword3 |= set_desc(1 << TX_DW3_NDPA_SH);
					}
				}
			}
		}
#endif
	}
}


// Now we didn't consider of FG_AGGRE_MSDU_FIRST case.
// If you want to support AMSDU method, you must modify this function.
int rtl8192cd_signin_txdesc(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, struct wlan_ethhdr_t *pethhdr)
{
	struct tx_desc		*pdesc = NULL, desc_backup;
	struct tx_desc_info	*pdescinfo = NULL;
	unsigned int 		fr_len, tx_len, i, keyid;
	int				q_num;
	int				sw_encrypt;
	unsigned char		*da, *pbuf, *pwlhdr, *pmic, *picv;
	unsigned char		 q_select;
#ifdef TX_SHORTCUT
#ifdef CONFIG_RTL_WAPI_SUPPORT
	struct wlan_ethhdr_t ethhdr_data;
#endif
	int				idx=0;
	struct tx_sc_entry *ptxsc_entry;
	unsigned char		pktpri;
#endif
	
	struct wlan_hdr wlanhdr;
	u32 icv_data[2], mic_data[2];
	
	struct xmit_buf *pxmitbuf;
	u8 *mem_start, *pkt_start, *write_ptr;
	u32 pkt_len, hdr_len;

	keyid = 0;
	q_select = 0;
	sw_encrypt = UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE));
	
	pmic = (unsigned char *) mic_data;
	picv = (unsigned char *) icv_data;

	if (txcfg->tx_rate == 0) {
		DEBUG_ERR("tx_rate=0!\n");
		txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
	}

	q_num = txcfg->q_num;
	tx_len = txcfg->fr_len;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_) {
		pbuf = ((struct sk_buff *)txcfg->pframe)->data;
#ifdef TX_SHORTCUT
		if ((NULL == pethhdr) && (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST)) {
			pethhdr = (struct wlan_ethhdr_t *)(pbuf - sizeof(struct wlan_ethhdr_t));
#ifdef CONFIG_RTL_WAPI_SUPPORT
			if ((_WAPI_SMS4_ == txcfg->privacy) && sw_encrypt) {
				// backup ethhdr because SecSWSMS4Encryption() will overwrite it via LLC header for SW WAPI
				memcpy(&ethhdr_data, pethhdr, sizeof(struct wlan_ethhdr_t));
				pethhdr = &ethhdr_data;
			}
#endif
		}
#endif
	} else {
		pbuf = (unsigned char*)txcfg->pframe;
	}
	
	da = get_da((unsigned char *)txcfg->phdr);

	// in case of default key, then find the key id
	if (GetPrivacy((txcfg->phdr)))
	{
#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			if (txcfg->pstat)
				keyid = txcfg->pstat->keyid;
			else
				keyid = 0;
		}
		else
#endif

#ifdef __DRAYTEK_OS__
		if (!IEEE8021X_FUN)
			keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		else {
			if (IS_MCAST(GetAddr1Ptr ((unsigned char *)txcfg->phdr)) || !txcfg->pstat)
				keyid = priv->pmib->dot11GroupKeysTable.keyid;
			else
				keyid = txcfg->pstat->keyid;
		}
#else

		if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_40_PRIVACY_ ||
			priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_104_PRIVACY_) {
			if(IEEE8021X_FUN && txcfg->pstat) {
#ifdef A4_STA
				if (IS_MCAST(da) && !(txcfg->pstat->state & WIFI_A4_STA))
#else
				if(IS_MCAST(da))
#endif					
					keyid = 0;
				else
					keyid = txcfg->pstat->keyid;
			}
			else
				keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		}
#endif
	}
	
	pxmitbuf = txcfg->pxmitbuf;
	
	mem_start = pxmitbuf->pkt_tail;
	
	for (i=0; i < txcfg->frg_num; i++)
	{
		mem_start = PTR_ALIGN(mem_start, TXAGG_DESC_ALIGN_SZ);
		pdesc = (struct tx_desc*) mem_start;
		pdescinfo = &pxmitbuf->txdesc_info[pxmitbuf->agg_num];
		
		pkt_start = mem_start + TXDESC_SIZE;
		if (0 == pxmitbuf->agg_num)
		{
			pkt_start += (pxmitbuf->pkt_offset * PACKET_OFFSET_SZ);
		}
		
		write_ptr = pkt_start;
		
		/*------------------------------------------------------------*/
		/*           fill descriptor of header + iv + llc             */
		/*------------------------------------------------------------*/

		if (i != 0)
		{
			// we have to allocate wlan_hdr
			pwlhdr = (UINT8 *)&wlanhdr;
			
			// other MPDU will share the same seq with the first MPDU
			memcpy((void *)pwlhdr, (void *)(txcfg->phdr), txcfg->hdr_len); // data pkt has 24 bytes wlan_hdr
//			pdesc->Dword3 |= set_desc((GetSequence(txcfg->phdr) & TX_SeqMask)  << TX_SeqSHIFT);
			memcpy(pdesc, &desc_backup, sizeof(*pdesc));
			goto init_desc;
		}
		else
		{
			//clear all bits
			memset(pdesc, 0, TXDESC_SIZE);
			
#ifdef WIFI_WMM
			if (txcfg->pstat && (is_qos_data(txcfg->phdr))) {
				if ((GetFrameSubType(txcfg->phdr) & (WIFI_DATA_TYPE | BIT(6) | BIT(7))) == (WIFI_DATA_TYPE | BIT(7))) {
					unsigned char tempQosControl[2];
					memset(tempQosControl, 0, 2);
					tempQosControl[0] = ((struct sk_buff *)txcfg->pframe)->cb[1];
#ifdef WMM_APSD
					if (
#ifdef CLIENT_MODE
						(OPMODE & WIFI_AP_STATE) &&
#endif
						(APSD_ENABLE) && (txcfg->pstat->state & WIFI_SLEEP_STATE) &&
						(!GetMData(txcfg->phdr)) &&
						((((tempQosControl[0] == 7) || (tempQosControl[0] == 6)) && (txcfg->pstat->apsd_bitmap & 0x01)) ||
						 (((tempQosControl[0] == 5) || (tempQosControl[0] == 4)) && (txcfg->pstat->apsd_bitmap & 0x02)) ||
						 (((tempQosControl[0] == 3) || (tempQosControl[0] == 0)) && (txcfg->pstat->apsd_bitmap & 0x08)) ||
						 (((tempQosControl[0] == 2) || (tempQosControl[0] == 1)) && (txcfg->pstat->apsd_bitmap & 0x04))))
						tempQosControl[0] |= BIT(4);
#endif
					if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
						tempQosControl[0] |= BIT(7);

					if (priv->pmib->dot11nConfigEntry.dot11nTxNoAck)
						tempQosControl[0] |= BIT(5);

					memcpy((void *)GetQosControl((txcfg->phdr)), tempQosControl, 2);
				}
			}
#endif

#ifdef BEAMFORMING_SUPPORT
			if(!txcfg->ndpa)
#endif	
			assign_wlanseq(GET_HW(priv), txcfg->phdr, txcfg->pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
				, txcfg->is_11s
#endif
				);
			pdesc->Dword9 |= set_desc((GetSequence(txcfg->phdr) & TX_DW9_SEQ_MSK)  << TX_DW9_SEQ_SH);
			pwlhdr = txcfg->phdr;
		}
		
#ifdef BEAMFORMING_SUPPORT
		if(!txcfg->ndpa)
#endif	
		SetDuration(pwlhdr, 0);

		rtl8192cd_hal_fill_fwinfo(priv, txcfg, pdesc, i);
		
#ifdef CLIENT_MODE
		if (OPMODE & WIFI_STATION_STATE) {
			if (GetFrameSubType(txcfg->phdr) == WIFI_PSPOLL)
				pdesc->Dword3 |= set_desc(1 << TX_DW3_NAVUSEHDR_SH);

			if (priv->ps_state)
				SetPwrMgt(pwlhdr);
			else
				ClearPwrMgt(pwlhdr);
		}
#endif
#ifdef BEAMFORMING_SUPPORT
		if (txcfg->ndpa)
			pdesc->Dword3 |= set_desc(1 << TX_DW3_NAVUSEHDR_SH);
#endif

//		pdesc->Dword0 |= set_desc(TX_OWN | TX_FirstSeg | TX_LastSeg);
		pdesc->Dword0 |= set_desc(TXDESC_SIZE << TX_DW0_OFFSET_SH); // tx_desc size

		if (IS_MCAST(GetAddr1Ptr((unsigned char *)txcfg->phdr)))
			pdesc->Dword0 |= set_desc(1 << TX_DW0_BMC_SH);
		
		switch (q_num) {
		case HIGH_QUEUE:
#ifdef MBSSID
			if (IS_VAP_INTERFACE(priv)) {
				// set MBSSID index
				pdesc->Dword1 |= set_desc((priv->vap_init_seq & TX_DW1_MACID_MSK) << TX_DW1_MACID_SH);
				pdesc->Dword6 |= set_desc((priv->vap_init_seq & TX_DW6_MBSSID_MSK) << TX_DW6_MBSSID_SH);
			}
#endif
			pdesc->Dword1 |= set_desc(1 << TX_DW1_MOREDATA_SH);
			q_select = TXDESC_QSEL_HIGH;
			break;
		case MGNT_QUEUE:
			q_select = TXDESC_QSEL_MGT;
			break;
#if defined(DRVMAC_LB) && defined(WIFI_WMM)
		case BE_QUEUE:
			q_select = TXDESC_QSEL_TID0;
			break;
#endif
		default:
			// data packet
#ifdef RTL_MANUAL_EDCA
			if (priv->pmib->dot11QosEntry.ManualEDCA) {
				switch (q_num) {
				case VO_QUEUE:
					q_select = TXDESC_QSEL_TID6;
					break;
				case VI_QUEUE:
					q_select = TXDESC_QSEL_TID4;
					break;
				case BE_QUEUE:
					q_select = TXDESC_QSEL_TID0;
					break;
				default:
					q_select = TXDESC_QSEL_TID1;
					break;
				}
			}
			else
#endif
			q_select = ((struct sk_buff *)txcfg->pframe)->cb[1];
			break;
		}
		pdesc->Dword1 |= set_desc((q_select & TX_DW1_QSEL_MSK)<< TX_DW1_QSEL_SH);

		// Set RateID
		if (txcfg->pstat) {
			if (txcfg->pstat->aid != MANAGEMENT_AID)	{
				pdesc->Dword1 |= set_desc((txcfg->pstat->ratr_idx & TX_DW1_RATE_ID_MSK) << TX_DW1_RATE_ID_SH);			
				// Set MacID
				if (q_select != TXDESC_QSEL_HIGH)
					pdesc->Dword1 |= set_desc(REMAP_AID(txcfg->pstat) << TX_DW1_MACID_SH);
				
#ifdef BEAMFORMING_SUPPORT
				if((priv->pmib->dot11RFEntry.txbf == 1)
						&& ((txcfg->pstat->ht_cap_len && (txcfg->pstat->ht_cap_buf.txbf_cap)) 
#ifdef RTK_AC_SUPPORT
						||(txcfg->pstat->vht_cap_len && (cpu_to_le32(txcfg->pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)))
#endif
						)) {
					pdesc->Dword2 |= set_desc((txcfg->pstat->p_aid & TX_DW2_P_AID_MSK)<<TX_DW2_P_AID_SH);
					pdesc->Dword2 |= set_desc((txcfg->pstat->g_id & TX_DW2_G_ID_MSK)<<TX_DW2_G_ID_SH);
				}
#endif
			}
		} else {
 			pdesc->Dword1 |= set_desc(ARFR_BMC << TX_DW1_RATE_ID_SH);
#ifdef BEAMFORMING_SUPPORT
			if (priv->pmib->dot11RFEntry.txbf == 1)
				pdesc->Dword2 |= set_desc(1 << TX_DW2_G_ID_SH);
#endif
		}

		pdesc->Dword4 |= set_desc((0x1f & TX_DW4_DATA_RATEFB_LMT_MSK) << TX_DW4_DATA_RATEFB_LMT_SH);
		if (txcfg->fixed_rate)
			pdesc->Dword3 |= set_desc((1 << TX_DW3_DISDATAFB_SH) | (1 << TX_DW3_DISRTSFB_SH)
					| (1 << TX_DW3_USERATE_SH));

		if (txcfg->pstat && (txcfg->pstat->sta_in_firmware != 1))
			pdesc->Dword3 |= set_desc(1 << TX_DW3_USERATE_SH);

init_desc:

		if (i != (txcfg->frg_num - 1))
		{
			if (i == 0) {
				memcpy(&desc_backup, pdesc, sizeof(*pdesc));
				fr_len = (txcfg->frag_thrshld - txcfg->llc);
			} else {
				fr_len = txcfg->frag_thrshld;
			}
			tx_len -= fr_len;
			
			SetMFrag(pwlhdr);
			pdesc->Dword2 |= set_desc(1 << TX_DW2_MOREFRAG_SH);
		}
		else	// last seg, or the only seg (frg_num == 1)
		{
			fr_len = tx_len;
			ClearMFrag(pwlhdr);
		}
		SetFragNum((pwlhdr), i);
		
		// consider the diff between the first frag and the other frag in rtl8192cd_fill_fwinfo()
		if ((txcfg->need_ack) && (i != 0)) {
			pdesc->Dword3 &= ~ set_desc((1 << TX_DW3_HW_RTS_EN_SH) | (1 << TX_DW3_RTSEN_SH) | (1 << TX_DW3_CTS2SELF_SH));
		}
		
		hdr_len = txcfg->hdr_len + txcfg->iv;
		if ((i == 0) && (txcfg->fr_type == _SKB_FRAME_TYPE_))
			hdr_len += txcfg->llc;
		
		pkt_len = hdr_len + fr_len;
		if ((txcfg->privacy) && sw_encrypt)
			pkt_len += (txcfg->icv + txcfg->mic);

		pdesc->Dword0 |= set_desc(pkt_len << TX_DW0_TXPKSIZE_SH);

//		if (!txcfg->need_ack && txcfg->privacy && sw_encrypt)
//			pdesc->Dword1 &= set_desc( ~(TX_SecTypeMask<< TX_SecTypeSHIFT));

		if ((txcfg->privacy) && !sw_encrypt) {
			// hw encrypt
			switch(txcfg->privacy) {
			case _WEP_104_PRIVACY_:
			case _WEP_40_PRIVACY_:
				wep_fill_iv(priv, pwlhdr, txcfg->hdr_len, keyid);
				pdesc->Dword1 |= set_desc(TXDESC_SECTYPE_WEP40_OR_TKIP << TX_DW1_SECTYPE_SH);
				break;

			case _TKIP_PRIVACY_:
				tkip_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
				pdesc->Dword1 |= set_desc(TXDESC_SECTYPE_WEP40_OR_TKIP << TX_DW1_SECTYPE_SH);
				break;
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
			case _WAPI_SMS4_:
				pdesc->Dword1 |= set_desc(TXDESC_SECTYPE_WAPI << TX_DW1_SECTYPE_SH);
				break;
#endif
			case _CCMP_PRIVACY_:
				//michal also hardware...
				aes_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
				pdesc->Dword1 |= set_desc(TXDESC_SECTYPE_AES << TX_DW1_SECTYPE_SH);
				break;

			default:
				DEBUG_ERR("Unknow privacy\n");
				break;
			}
		}
		
		if (pxmitbuf->agg_num != 0) rtl8192cd_usb_cal_txdesc_chksum(pdesc);
		
		if (txcfg->fr_len == 0) {
			goto fill_usb_data;
		}

		/*------------------------------------------------------------*/
		/*              fill descriptor of frame body                 */
		/*------------------------------------------------------------*/
		
		// retrieve H/W MIC and put in payload
#ifdef CONFIG_RTL_WAPI_SUPPORT
		if (txcfg->privacy == _WAPI_SMS4_)
		{
			SecSWSMS4Encryption(priv, txcfg);
		}
#endif

#ifndef NOT_RTK_BSP //_Eric??
		if ((txcfg->privacy == _TKIP_PRIVACY_) &&
			(priv->pshare->have_hw_mic) &&
			!(priv->pmib->dot11StationConfigEntry.swTkipMic) &&
			(i == (txcfg->frg_num-1)) )	// last segment
		{
			register unsigned long int l,r;
			unsigned char *mic;
			volatile int i;

			while ((*(volatile unsigned int *)GDMAISR & GDMA_COMPIP) == 0)
				for (i=0; i<10; i++)
					;

			l = *(volatile unsigned int *)GDMAICVL;
			r = *(volatile unsigned int *)GDMAICVR;

			mic = ((struct sk_buff *)txcfg->pframe)->data + txcfg->fr_len - 8;
			mic[0] = (unsigned char)(l & 0xff);
			mic[1] = (unsigned char)((l >> 8) & 0xff);
			mic[2] = (unsigned char)((l >> 16) & 0xff);
			mic[3] = (unsigned char)((l >> 24) & 0xff);
			mic[4] = (unsigned char)(r & 0xff);
			mic[5] = (unsigned char)((r >> 8) & 0xff);
			mic[6] = (unsigned char)((r >> 16) & 0xff);
			mic[7] = (unsigned char)((r >> 24) & 0xff);

#ifdef MICERR_TEST
			if (priv->micerr_flag) {
				mic[7] ^= mic[7];
				priv->micerr_flag = 0;
			}
#endif
		}
#endif // !NOT_RTK_BSP

		/*------------------------------------------------------------*/
		/*                insert sw encrypt here!                     */
		/*------------------------------------------------------------*/
		if (txcfg->privacy && sw_encrypt)
		{
			if (txcfg->privacy == _TKIP_PRIVACY_ ||
				txcfg->privacy == _WEP_40_PRIVACY_ ||
				txcfg->privacy == _WEP_104_PRIVACY_)
			{
				if (i == 0)
					tkip_icv(picv,
						pwlhdr + txcfg->hdr_len + txcfg->iv, txcfg->llc,
						pbuf, fr_len);
				else
					tkip_icv(picv,
						NULL, 0,
						pbuf, fr_len);

				if ((i == 0) && (txcfg->llc != 0)) {
					if (txcfg->privacy == _TKIP_PRIVACY_)
						tkip_encrypt(priv, pwlhdr, txcfg->hdr_len,
							pwlhdr + txcfg->hdr_len + 8, sizeof(struct llc_snap),
							pbuf, fr_len, picv, txcfg->icv);
					else
						wep_encrypt(priv, pwlhdr, txcfg->hdr_len,
							pwlhdr + txcfg->hdr_len + 4, sizeof(struct llc_snap),
							pbuf, fr_len, picv, txcfg->icv,
							txcfg->privacy);
				}
				else { // not first segment or no snap header
					if (txcfg->privacy == _TKIP_PRIVACY_)
						tkip_encrypt(priv, pwlhdr, txcfg->hdr_len, NULL, 0,
							pbuf, fr_len, picv, txcfg->icv);
					else
						wep_encrypt(priv, pwlhdr, txcfg->hdr_len, NULL, 0,
							pbuf, fr_len, picv, txcfg->icv,
							txcfg->privacy);
				}
			}
			else if (txcfg->privacy == _CCMP_PRIVACY_)
			{
				// then encrypt all (including ICV) by AES
				if ((i == 0) && (txcfg->llc != 0)) // encrypt 3 segments ==> llc, mpdu, and mic
				{
					aesccmp_encrypt(priv, pwlhdr, pwlhdr + txcfg->hdr_len + 8,
						pbuf, fr_len, pmic);
				}
				else // encrypt 2 segments ==> mpdu and mic
					aesccmp_encrypt(priv, pwlhdr, NULL,
						pbuf, fr_len, pmic);
			}
		}
		
fill_usb_data:
		
		// Fill data of header + iv + llc
		memcpy(write_ptr, pwlhdr, hdr_len);
		write_ptr += hdr_len;
		
		if (fr_len) {
			// Fill data of frame payload without llc
			memcpy(write_ptr, pbuf, fr_len);
			write_ptr += fr_len;
			
			// Fill data of icv/mic
			if (txcfg->privacy && sw_encrypt) {
				if (txcfg->privacy == _TKIP_PRIVACY_ ||
					txcfg->privacy == _WEP_40_PRIVACY_ ||
					txcfg->privacy == _WEP_104_PRIVACY_)
				{
					memcpy(write_ptr, picv, txcfg->icv);
					write_ptr += txcfg->icv;
				}
				else if (txcfg->privacy == _CCMP_PRIVACY_)
				{
					memcpy(write_ptr, pmic, txcfg->mic);
					write_ptr += txcfg->mic;
				}
#ifdef CONFIG_RTL_WAPI_SUPPORT
				else if (txcfg->privacy == _WAPI_SMS4_)
				{
					memcpy(write_ptr, pbuf + fr_len, txcfg->mic);
					write_ptr += txcfg->mic;
				}
#endif
			}
		}

		if ((i == (txcfg->frg_num - 1))
#ifdef BEAMFORMING_SUPPORT
				&& !txcfg->ndpa
#endif
				)
			pdescinfo->type = txcfg->fr_type;
		else
			pdescinfo->type = _RESERVED_FRAME_TYPE_;

#if defined(CONFIG_RTK_MESH) && defined(MESH_USE_METRICOP)
		if( (txcfg->fr_type == _PRE_ALLOCMEM_) && (txcfg->is_11s & 128)) // for 11s link measurement frame
			pdescinfo->type =_RESERVED_FRAME_TYPE_;
#endif
		pdescinfo->pframe = txcfg->pframe;
		pdescinfo->buf_ptr = mem_start;
		pdescinfo->buf_len = (u32)(write_ptr - mem_start);
		
		pdescinfo->pstat = txcfg->pstat;
		pdescinfo->rate = txcfg->tx_rate;
		pdescinfo->priv = priv;
#ifdef CONFIG_SDIO_HCI
		update_remaining_timeslot(priv, pdescinfo, q_num,
			(get_desc(pdesc->Dword5) >> TX_DW5_DATA_BW_SH) & TX_DW5_DATA_BW_MSK);
#endif
		mem_start = write_ptr;
		++pxmitbuf->agg_num;
		
		pbuf += fr_len;
	}
	
	pxmitbuf->pkt_tail = mem_start;
	
#ifdef TX_SHORTCUT
	if (!priv->pmib->dot11OperationEntry.disable_txsc && txcfg->pstat
			&& (txcfg->fr_type == _SKB_FRAME_TYPE_)
			&& (txcfg->frg_num == 1)
			&& ((txcfg->privacy == 0)
#ifdef CONFIG_RTL_WAPI_SUPPORT
			|| (txcfg->privacy == _WAPI_SMS4_)
#endif
			|| (!sw_encrypt))
			&& !GetMData(txcfg->phdr)
			&& (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST)
			/*&& (!IEEE8021X_FUN ||
				(IEEE8021X_FUN && (txcfg->pstat->ieee8021x_ctrlport == 1)
				&& (pethhdr->type != htons(0x888e)))) */
			&& (pethhdr->type != htons(0x888e))
#ifdef CONFIG_RTL_WAPI_SUPPORT
			&& (pethhdr->type != htons(ETH_P_WAPI))
#endif
			&& !txcfg->is_dhcp
#ifdef A4_STA
			&& ((txcfg->pstat && txcfg->pstat->state & WIFI_A4_STA) 
				||!IS_MCAST((unsigned char *)pethhdr))
#else
			&& !IS_MCAST((unsigned char *)pethhdr)
#endif
			) {
		pktpri = ((struct sk_buff *)txcfg->pframe)->cb[1];
		idx = get_tx_sc_free_entry(priv, txcfg->pstat, (u8*)pethhdr, pktpri);
		ptxsc_entry = &txcfg->pstat->tx_sc_ent[pktpri][idx];
		
		memcpy((void *)&ptxsc_entry->ethhdr, pethhdr, sizeof(struct wlan_ethhdr_t));
		desc_copy(&ptxsc_entry->hwdesc1, pdesc);
		
		// For convenient follow PCI rule to let Dword7[15:0] of Tx desc backup store TxBuffSize.
		// Do this, we can use the same condition to determine if go through TX shortcut path
		// (Note: for WAPI SW encryption, PCIE IF contain a extra SMS4_MIC_LEN)
		ptxsc_entry->hwdesc1.Dword7 &= set_desc(~TX_TxBufSizeMask);	// Remove checksum
		ptxsc_entry->hwdesc1.Dword7 |= set_desc(txcfg->fr_len & TX_TxBufSizeMask);
		
		descinfo_copy(&ptxsc_entry->swdesc1, pdescinfo);
		ptxsc_entry->sc_keyid = keyid;
		
		memcpy(&ptxsc_entry->txcfg, txcfg, FIELD_OFFSET(struct tx_insn, pxmitframe));
		memcpy((void *)&ptxsc_entry->wlanhdr, txcfg->phdr, sizeof(struct wlanllc_hdr));
		
		txcfg->pstat->protection = priv->pmib->dot11ErpInfo.protection;
		txcfg->pstat->ht_protection = priv->ht_protection;
	}
#endif // TX_SHORTCUT
	
	if ((_SKB_FRAME_TYPE_ == txcfg->fr_type)
#ifdef BEAMFORMING_SUPPORT
				|| txcfg->ndpa
#endif
			) {
		release_wlanllchdr_to_poll(priv, txcfg->phdr);
	} else {
		release_wlanhdr_to_poll(priv, txcfg->phdr);
	}
	txcfg->phdr = NULL;
	
#ifdef CONFIG_TX_RECYCLE_EARLY
	rtl8192cd_usb_tx_recycle(priv, pdescinfo);
#endif
	
#ifdef CONFIG_USB_HCI
#ifdef CONFIG_USB_TX_AGGREGATION
	if (pxmitbuf->ext_tag) {
		usb_submit_xmitbuf_async(priv, pxmitbuf);
	} else {
		if (pxmitbuf->agg_num == txcfg->frg_num) {
			rtw_xmitbuf_aggregate(priv, pxmitbuf, txcfg->pstat, q_num);
			usb_submit_xmitbuf_async(priv, pxmitbuf);
		}
	}
#else
	usb_submit_xmitbuf_async(priv, pxmitbuf);
#endif
#endif // CONFIG_USB_HCI

#ifdef CONFIG_SDIO_HCI
#ifdef CONFIG_SDIO_TX_AGGREGATION
	if (pxmitbuf->ext_tag) {
		sdio_submit_xmitbuf(priv, pxmitbuf);
	} else {
		if (pxmitbuf->agg_start_with == txcfg) {
			rtw_xmitbuf_aggregate(priv, pxmitbuf, txcfg->pstat, q_num);
			sdio_submit_xmitbuf(priv, pxmitbuf);
		}
	}
#else
	sdio_submit_xmitbuf(priv, pxmitbuf);
#endif
#endif // CONFIG_SDIO_HCI
	
	txcfg->pxmitbuf = NULL;

	return 0;
}

#ifdef TX_SHORTCUT
int rtl8192cd_signin_txdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct tx_sc_entry *ptxsc_entry)
{
	struct tx_desc *pdesc;
	struct tx_desc_info *pdescinfo;
	int q_num;
	struct stat_info *pstat;
	struct sk_buff *pskb;
	
	struct xmit_buf *pxmitbuf;
	u8 *mem_start, *pkt_start, *write_ptr;
	u32 pkt_len, hdr_len, fr_len;
	
	pstat = txcfg->pstat;
	pskb = (struct sk_buff *)txcfg->pframe;

	q_num = txcfg->q_num;
	
	pxmitbuf = txcfg->pxmitbuf;
	
	mem_start = pxmitbuf->pkt_tail;
	mem_start = PTR_ALIGN(mem_start, TXAGG_DESC_ALIGN_SZ);
	
	pdesc = (struct tx_desc*) mem_start;
	pdescinfo = &pxmitbuf->txdesc_info[pxmitbuf->agg_num];
	
	pkt_start = mem_start + TXDESC_SIZE;
	if (0 == pxmitbuf->agg_num)
		pkt_start += (pxmitbuf->pkt_offset * PACKET_OFFSET_SZ);
		
	write_ptr = pkt_start;
	
	hdr_len = txcfg->hdr_len + txcfg->iv + txcfg->llc;
	fr_len = txcfg->fr_len;
#if defined(CONFIG_RTL_WAPI_SUPPORT)
	if (txcfg->privacy == _WAPI_SMS4_)
		fr_len += txcfg->mic;		// For WAPI software encryption, we must consider txcfg->mic
#endif
	pkt_len  = hdr_len + fr_len;
	
	/*------------------------------------------------------------*/
	/*           fill descriptor of header + iv + llc             */
	/*------------------------------------------------------------*/
	desc_copy(pdesc, &ptxsc_entry->hwdesc1);

	assign_wlanseq(GET_HW(priv), txcfg->phdr, pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
		, txcfg->is_11s
#endif
		);

	pdesc->Dword9 &= ~ set_desc(TX_DW9_SEQ_MSK << TX_DW9_SEQ_SH);
	pdesc->Dword9 |= set_desc((GetSequence(txcfg->phdr) & TX_DW9_SEQ_MSK)  << TX_DW9_SEQ_SH);

	//set Break
	if((q_num >= BK_QUEUE) && (q_num <= VO_QUEUE)){
		if((pstat != priv->pshare->CurPstat[q_num-BK_QUEUE])) {
			pdesc->Dword2 |= set_desc(1 << TX_DW2_BK_SH);
			priv->pshare->CurPstat[q_num-BK_QUEUE] = pstat;
		} else {
			pdesc->Dword2 &= ~ set_desc(1 << TX_DW2_BK_SH);
		}
	} else {
		pdesc->Dword2 |= set_desc(1 << TX_DW2_BK_SH);
	}
	
	if ((pstat->IOTPeer == HT_IOT_PEER_INTEL) && (pstat->retry_inc)) {
		if (is_MCS_rate(pstat->current_tx_rate) && !(pstat->leave)
				&& priv->pshare->intel_rty_lmt) {
			pdesc->Dword4 |= set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
			
			pdesc->Dword4 &= ~ set_desc(TX_DW4_DATA_RT_LMT_MSK << TX_DW4_DATA_RT_LMT_SH);
			pdesc->Dword4 |= set_desc((priv->pshare->intel_rty_lmt & TX_DW4_DATA_RT_LMT_MSK) << TX_DW4_DATA_RT_LMT_SH);
		} else {
			pdesc->Dword4 &= ~ set_desc(1 << TX_DW4_RTY_LMT_EN_SH);
			pdesc->Dword4 &= ~ set_desc(TX_DW4_DATA_RT_LMT_MSK << TX_DW4_DATA_RT_LMT_SH);
		}
	}

	if((priv->pshare->rf_ft_var.txforce != 0xff)
#ifdef BEAMFORMING_SUPPORT
		&& (!txcfg->ndpa) 
#endif
	){
		pdesc->Dword4 &= ~ set_desc(TX_DW4_DATARATE_MSK << TX_DW4_DATARATE_SH);
		pdesc->Dword4 |= set_desc((priv->pshare->rf_ft_var.txforce & TX_DW4_DATARATE_MSK) << TX_DW4_DATARATE_SH);
		
		pdesc->Dword3 |= set_desc(1 << TX_DW3_USERATE_SH);
		pdesc->Dword3 |= set_desc(1 << TX_DW3_DISDATAFB_SH);
		pdesc->Dword3 |= set_desc(1 << TX_DW3_DISRTSFB_SH);
	}

#ifdef BEAMFORMING_SUPPORT
        if (priv->pmib->dot11nConfigEntry.dot11nSTBC && (txcfg->pstat)
			&& ((txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_RX_STBC_CAP_)) 
#ifdef RTK_AC_SUPPORT			
			|| (txcfg->pstat->vht_cap_buf.vht_cap_info & cpu_to_le32(_VHTCAP_RX_STBC_CAP_))
#endif
			)){
		u1Byte					Idx = 0;
		PRT_BEAMFORMING_ENTRY	pEntry; 
		pEntry = Beamforming_GetEntryByMacId(priv, txcfg->pstat->aid, &Idx);
		if(((get_rf_mimo_mode(priv) == MIMO_2T2R) || (get_rf_mimo_mode(priv) == MIMO_3T3R)) && (pEntry == NULL))
			pdesc->Dword5 |= set_desc(1 << TX_DW5_DATA_STBC_SH);
        }
#endif
		
#ifdef CLIENT_MODE
	if (OPMODE & WIFI_STATION_STATE) {
		if (GetFrameSubType(txcfg->phdr) == WIFI_PSPOLL)
			pdesc->Dword3 |= set_desc(1 << TX_DW3_NAVUSEHDR_SH);

		if (priv->ps_state)
			SetPwrMgt(txcfg->phdr);
		else
			ClearPwrMgt(txcfg->phdr);
	}
#endif
	
	pdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & ~TX_DW0_TXPKSIZE_MSK) | pkt_len);
	
	if (pxmitbuf->agg_num != 0) rtl8192cd_usb_cal_txdesc_chksum(pdesc);

	descinfo_copy(pdescinfo, &ptxsc_entry->swdesc1);

	if (txcfg->one_txdesc)
		goto fill_usb_data;

	/*------------------------------------------------------------*/
	/*              fill descriptor of frame body                 */
	/*------------------------------------------------------------*/
	
#if defined(CONFIG_RTL_WAPI_SUPPORT)
	if (txcfg->privacy == _WAPI_SMS4_)
	{
		SecSWSMS4Encryption(priv, txcfg);
	}
#endif

#ifndef NOT_RTK_BSP //_Eric??
	if ((txcfg->privacy == _TKIP_PRIVACY_) &&
		(priv->pshare->have_hw_mic) &&
		!(priv->pmib->dot11StationConfigEntry.swTkipMic))
	{
		register unsigned long int l,r;
		unsigned char *mic;
		int delay = 18;

		while ((*(volatile unsigned int *)GDMAISR & GDMA_COMPIP) == 0) {
			delay_us(delay);
			delay = delay / 2;
		}

		l = *(volatile unsigned int *)GDMAICVL;
		r = *(volatile unsigned int *)GDMAICVR;

		mic = ((struct sk_buff *)txcfg->pframe)->data + txcfg->fr_len - 8;
		mic[0] = (unsigned char)(l & 0xff);
		mic[1] = (unsigned char)((l >> 8) & 0xff);
		mic[2] = (unsigned char)((l >> 16) & 0xff);
		mic[3] = (unsigned char)((l >> 24) & 0xff);
		mic[4] = (unsigned char)(r & 0xff);
		mic[5] = (unsigned char)((r >> 8) & 0xff);
		mic[6] = (unsigned char)((r >> 16) & 0xff);
		mic[7] = (unsigned char)((r >> 24) & 0xff);

#ifdef MICERR_TEST
		if (priv->micerr_flag) {
			mic[7] ^= mic[7];
			priv->micerr_flag = 0;
		}
#endif
	}
#endif // !NOT_RTK_BSP
	
fill_usb_data:
	
	// Fill data of header + iv + llc
	memcpy(write_ptr, txcfg->phdr, hdr_len);
	write_ptr += hdr_len;
	
	// Fill data of frame payload without llc
	memcpy(write_ptr, pskb->data, fr_len);
	write_ptr += fr_len;
	
//	pdescinfo->type = _SKB_FRAME_TYPE_;
	pdescinfo->pframe = pskb;
	pdescinfo->buf_ptr = mem_start;
	pdescinfo->buf_len = (u32)(write_ptr - mem_start);
	
	pdescinfo->pstat = pstat;
	pdescinfo->priv = priv;
#ifdef CONFIG_SDIO_HCI
	update_remaining_timeslot(priv, pdescinfo, q_num,
		(get_desc(pdesc->Dword5) >> TX_DW5_DATA_BW_SH) & TX_DW5_DATA_BW_MSK);
#endif
	++pxmitbuf->agg_num;
	
	pxmitbuf->pkt_tail = write_ptr;
	
	release_wlanllchdr_to_poll(priv, txcfg->phdr);
	txcfg->phdr = NULL;
	
#ifdef CONFIG_TX_RECYCLE_EARLY
	rtl8192cd_usb_tx_recycle(priv, pdescinfo);
#endif
	
#ifdef CONFIG_USB_HCI
#ifdef CONFIG_USB_TX_AGGREGATION
	if (pxmitbuf->ext_tag) {
		usb_submit_xmitbuf_async(priv, pxmitbuf);
	} else {
		if (pxmitbuf->agg_num == txcfg->frg_num) {
			rtw_xmitbuf_aggregate(priv, pxmitbuf, pstat, q_num);
			usb_submit_xmitbuf_async(priv, pxmitbuf);
		}
	}
#else
	usb_submit_xmitbuf_async(priv, pxmitbuf);
#endif
#endif // CONFIG_USB_HCI

#ifdef CONFIG_SDIO_HCI
#ifdef CONFIG_SDIO_TX_AGGREGATION
	if (pxmitbuf->ext_tag) {
		sdio_submit_xmitbuf(priv, pxmitbuf);
	} else {
		if (pxmitbuf->agg_start_with == txcfg) {
			rtw_xmitbuf_aggregate(priv, pxmitbuf, pstat, q_num);
			sdio_submit_xmitbuf(priv, pxmitbuf);
		}
	}
#else
	sdio_submit_xmitbuf(priv, pxmitbuf);
#endif
#endif // CONFIG_SDIO_HCI
	
	txcfg->pxmitbuf = NULL;

#ifdef SUPPORT_SNMP_MIB
	if (txcfg->rts_thrshld <= get_mpdu_len(txcfg, txcfg->fr_len))
		SNMP_MIB_INC(dot11RTSSuccessCount, 1);
#endif

	return 0;
}
#endif // TX_SHORTCUT


void hal_fill_bcn_desc(struct rtl8192cd_priv *priv, struct tx_desc *pdesc, void *dat_content, unsigned short txLength, char forceUpdate)
{
	if (txLength != priv->tx_beacon_len || forceUpdate)
	{
		forceUpdate = 1;
		memset(pdesc, 0, TXDESC_SIZE);
		
		pdesc->Dword0 |= set_desc((1 << TX_DW0_BMC_SH) | (TXDESC_SIZE << TX_DW0_OFFSET_SH));
		pdesc->Dword0 |= set_desc((txLength & TX_DW0_TXPKSIZE_MSK) << TX_DW0_TXPKSIZE_SH);
		pdesc->Dword1 |= set_desc(TXDESC_QSEL_BCN << TX_DW1_QSEL_SH);

#ifdef MBSSID
		if (IS_VAP_INTERFACE(priv)) {
			// set MBSSID index
			pdesc->Dword1 |= set_desc((priv->vap_init_seq & TX_DW1_MACID_MSK) << TX_DW1_MACID_SH);
			pdesc->Dword6 |= set_desc((priv->vap_init_seq & TX_DW6_MBSSID_MSK) << TX_DW6_MBSSID_SH);
		}
#endif
		pdesc->Dword9 = set_desc((GetSequence(dat_content) & TX_DW9_SEQ_MSK) << TX_DW9_SEQ_SH);
		pdesc->Dword3 |= set_desc((1 << TX_DW3_DISDATAFB_SH) | (1 << TX_DW3_USERATE_SH));

		priv->tx_beacon_len = txLength;
	}
	else
	{
		pdesc->Dword9 = set_desc((GetSequence(dat_content) & TX_DW9_SEQ_MSK) << TX_DW9_SEQ_SH);
	}
#if 1
	pdesc->Dword8 |= set_desc(1<<TX_DW8_EN_HWSEQ_SH);
 	RTL_W16(0x4DA, (GetSequence(dat_content) & TX_DW9_SEQ_MSK));
	pdesc->Dword3 |= set_desc((1<<TX_DW3_HW_SSN_SEL_SH));
#endif		
	// Group Bit Control
	pdesc->Dword9 |= set_desc(((priv->timoffset-24) & TX_DW9_GROUPBIT_IE_OFFSET_MSK) << TX_DW9_GROUPBIT_IE_OFFSET_SH);
	// Auto set bitmap control by HW
	if (OPMODE & WIFI_ADHOC_STATE) {
		pdesc->Dword9 |= set_desc(0 << TX_DW9_GROUPBIT_IE_ENABLE_SH);
	} else {
		pdesc->Dword9 |= set_desc(1 << TX_DW9_GROUPBIT_IE_ENABLE_SH);
	}

#ifdef P2P_SUPPORT  // 2013
	if ((OPMODE & WIFI_P2P_SUPPORT) && (P2PMODE==P2P_TMP_GO || P2PMODE==P2P_PRE_GO)) {
		pdesc->Dword4 |= set_desc(4 << TX_DW4_RTSRATE_SH);    // RTS rate set to 6M
		pdesc->Dword4 |= set_desc(4 << TX_DW4_DATARATE_SH);  // use 6M to beacon rate
	}
#endif
	
	/*
	* Intel IOT, dynamic enhance beacon tx AGC
	*/
	if ((priv->bcnTxAGC_bak != priv->bcnTxAGC) || forceUpdate)
	{
		pdesc->Dword5 &= ~ set_desc(TX_DW5_TXPWR_OFSET_MSK << TX_DW5_TXPWR_OFSET_SH);
		if (priv->bcnTxAGC == 1) {
			pdesc->Dword5 |= set_desc(4 << TX_DW5_TXPWR_OFSET_SH);
		} else if (priv->bcnTxAGC == 2) {
			pdesc->Dword5 |= set_desc(5 << TX_DW5_TXPWR_OFSET_SH);
		}
		priv->bcnTxAGC_bak = priv->bcnTxAGC;
	}

	if ((priv->pshare->is_40m_bw != priv->pshare->is_40m_bw_bak) || forceUpdate) {
		pdesc->Dword5 &= ~ set_desc(TX_DW5_DATA_SC_MSK << TX_DW5_DATA_SC_SH);

		if (priv->pshare->is_40m_bw) {
			if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW)
				pdesc->Dword5 |= set_desc(2 << TX_DW5_DATA_SC_SH);
			else
				pdesc->Dword5 |= set_desc(1 << TX_DW5_DATA_SC_SH);
		}
		priv->pshare->is_40m_bw_bak = priv->pshare->is_40m_bw;
	}
}


void signin_beacon_desc(struct rtl8192cd_priv *priv, unsigned int *beaconbuf, unsigned int frlen)
{
	struct tx_desc *pdesc;
	struct tx_desc_info	*pdescinfo;
	struct xmit_buf *pxmitbuf;
	u8 *mem_start, *pkt_start;
	u32 tx_len, pkt_offset;
	
	pdesc = &priv->tx_descB;
	
	// only one that hold the ownership of the HW TX queue can signin beacon
	// because there is only one reserved beacon block in HW to store beacon content
	if (test_and_set_bit(BEACON_QUEUE, &priv->pshare->use_hw_queue_bitmap) != 0)
		return;
	
	if (NULL == (pxmitbuf = rtw_alloc_beacon_xmitbuf(priv))) {
		// Release the ownership of the HW TX queue
		clear_bit(BEACON_QUEUE, &priv->pshare->use_hw_queue_bitmap);
		
		printk("[%s] alloc xmitbuf fail\n", __FUNCTION__);
		return;
	}
	pxmitbuf->use_hw_queue = 1;
	
	hal_fill_bcn_desc(priv, pdesc, (void*)beaconbuf, frlen, 0);
	
	mem_start = pxmitbuf->pkt_data;
	
#if defined(CONFIG_USB_HCI)
#ifndef CONFIG_USE_USB_BUFFER_ALLOC_TX
	pkt_start = mem_start + TXDESC_SIZE;
	
	tx_len = frlen + TXDESC_SIZE;
	if ((tx_len % GET_HAL_INTF_DATA(priv)->UsbBulkOutSize) == 0) {
		pkt_offset = pxmitbuf->pkt_offset * PACKET_OFFSET_SZ;
		tx_len += pkt_offset;
		pkt_start += pkt_offset;
	} else {
		pxmitbuf->pkt_offset = 0;
	}
	
#else
	pkt_offset = pxmitbuf->pkt_offset * PACKET_OFFSET_SZ;
	pkt_start = mem_start + TXDESC_SIZE + pkt_offset;
	tx_len = frlen + TXDESC_SIZE + pkt_offset;
#endif // !CONFIG_USE_USB_BUFFER_ALLOC_TX
#else // CONFIG_SDIO_HCI
	pkt_offset = pxmitbuf->pkt_offset * PACKET_OFFSET_SZ;
	pkt_start = mem_start + TXDESC_SIZE + pkt_offset;
	tx_len = frlen + TXDESC_SIZE + pkt_offset;
#endif
	
	memcpy(mem_start, pdesc, TXDESC_SIZE);
	memcpy(pkt_start, beaconbuf, frlen);

	pxmitbuf->pkt_tail = mem_start + tx_len;
	
	pdescinfo = &pxmitbuf->txdesc_info[0];
	pdescinfo->type = _RESERVED_FRAME_TYPE_;
	pdescinfo->pframe = NULL;
	pdescinfo->priv = priv;

	pdescinfo->buf_ptr = mem_start;
	pdescinfo->buf_len = tx_len;
	pxmitbuf->agg_num = 1;
	
#if defined(CONFIG_USB_HCI)
	usb_submit_xmitbuf_async(priv, pxmitbuf);
#else // CONFIG_SDIO_HCI
	sdio_submit_xmitbuf(priv, pxmitbuf);
#endif
}


int rtl8192cd_firetx(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	int retval;

#ifdef DFS
	if (!priv->pmib->dot11DFSEntry.disable_DFS &&
		GET_ROOT(priv)->pmib->dot11DFSEntry.disable_tx) {
		DEBUG_ERR("TX DROP: DFS probation period\n");
		return CONGESTED;
	}
#endif

	if (update_txinsn_stage1(priv, txcfg) == FALSE) {
		return CONGESTED;
	}
	
	txcfg->next_txpath = TXPATH_FIRETX;
	local_bh_disable();
	if (rtw_is_tx_queue_empty(priv, txcfg) == FALSE) {
		if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
			local_bh_enable();
			return CONGESTED;
		}
		local_bh_enable();
		return SUCCESS;
	}
	local_bh_enable();
	
	retval = __rtl8192cd_firetx(priv, txcfg);
	
	rtw_handle_xmit_fail(priv, txcfg);
	
	return retval;
}

#ifndef CONFIG_NETDEV_MULTI_TX_QUEUE
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv)
{
	priv = GET_ROOT(priv);

	if (IS_DRV_OPEN(priv)) {
		netif_wake_queue(priv->dev);
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		netif_wake_queue(GET_VXD_PRIV(priv)->dev);
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				netif_wake_queue(priv->pvap_priv[bssidIdx]->dev);
			}
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_wake_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_wake_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}

void rtl8192cd_tx_stopQueue(struct rtl8192cd_priv *priv)
{
	priv = GET_ROOT(priv);
	++priv->pshare->nr_stop_netif_tx_queue;

	if (IS_DRV_OPEN(priv)) {
		netif_stop_queue(priv->dev);
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		netif_stop_queue(GET_VXD_PRIV(priv)->dev);
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				netif_stop_queue(priv->pvap_priv[bssidIdx]->dev);
			}
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_stop_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_stop_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}

#else // CONFIG_NETDEV_MULTI_TX_QUEUE
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv, unsigned int index)
{
	priv = GET_ROOT(priv);

	if (IS_DRV_OPEN(priv)) {
		if (unlikely(_NETDEV_TX_QUEUE_ALL == index))
			netif_tx_wake_all_queues(priv->dev);
		else
			netif_tx_wake_queue(netdev_get_tx_queue(priv->dev, index));
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		if (unlikely(_NETDEV_TX_QUEUE_ALL == index))
			netif_tx_wake_all_queues(GET_VXD_PRIV(priv)->dev);
		else
			netif_tx_wake_queue(netdev_get_tx_queue(GET_VXD_PRIV(priv)->dev, index));
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				if (unlikely(_NETDEV_TX_QUEUE_ALL == index))
					netif_tx_wake_all_queues(priv->pvap_priv[bssidIdx]->dev);
				else
					netif_tx_wake_queue(netdev_get_tx_queue(priv->pvap_priv[bssidIdx]->dev, index));
			}
		}
	}
#endif

	if ((_NETDEV_TX_QUEUE_ALL != index) && (_NETDEV_TX_QUEUE_BE != index))
		return;

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_wake_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_wake_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}

void rtl8192cd_tx_stopQueue(struct rtl8192cd_priv *priv)
{
	priv = GET_ROOT(priv);
	++priv->pshare->nr_stop_netif_tx_queue;

	if (IS_DRV_OPEN(priv)) {
		netif_tx_stop_all_queues(priv->dev);
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		netif_tx_stop_all_queues(GET_VXD_PRIV(priv)->dev);
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				netif_tx_stop_all_queues(priv->pvap_priv[bssidIdx]->dev);
			}
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_stop_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_stop_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}
#endif // !CONFIG_NETDEV_MULTI_TX_QUEUE


static int activate_mc_queue_xmit(struct rtl8192cd_priv *priv, u8 force_tx)
{
	_irqL irqL;
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	int active = 0;
	
	ptxservq = &priv->tx_mc_queue;
	xframe_queue = &ptxservq->xframe_queue;
	
	_enter_critical_bh(&xframe_queue->lock, &irqL);
	
	if (_rtw_queue_empty(xframe_queue) == FALSE) {
		sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num];
		
		_rtw_spinlock(&sta_queue->lock);
		
		if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
			rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
			++sta_queue->qlen;
			active = 1;
		}
		priv->release_mcast = force_tx;
		
		_rtw_spinunlock(&sta_queue->lock);
	}
	
	_exit_critical_bh(&xframe_queue->lock, &irqL);
	
	return active;
}

void stop_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i;
	
	struct tx_servq *ptxservq;
	_queue *sta_queue;
	_irqL irqL;
	
	for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
		ptxservq = &pstat->tx_queue[i];
		sta_queue = &priv->pshare->tx_pending_sta_queue[i];
		
		_enter_critical_bh(&sta_queue->lock, &irqL);
		
		if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
			rtw_list_delete(&ptxservq->tx_pending);
			--sta_queue->qlen;
		}
		
		_exit_critical_bh(&sta_queue->lock, &irqL);
	}
	
	// for BC/MC frames
	ptxservq = &priv->tx_mc_queue;
	sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num];
	
	_enter_critical_bh(&sta_queue->lock, &irqL);
	
	if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
		rtw_list_delete(&ptxservq->tx_pending);
		--sta_queue->qlen;
	}
	ptxservq->q_num = MCAST_QNUM;
	
	_exit_critical_bh(&sta_queue->lock, &irqL);
	
	if (priv->release_mcast) {
		activate_mc_queue_xmit(priv, priv->release_mcast);
	}
}

void wakeup_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i;
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;
	
	for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
		ptxservq = &pstat->tx_queue[i];
		xframe_queue = &ptxservq->xframe_queue;
		
		_enter_critical_bh(&xframe_queue->lock, &irqL);
		
		if (_rtw_queue_empty(xframe_queue) == FALSE) {
			sta_queue = &priv->pshare->tx_pending_sta_queue[i];
			
			_rtw_spinlock(&sta_queue->lock);
			
			if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
				rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
				++sta_queue->qlen;
			}
			
			_rtw_spinunlock(&sta_queue->lock);
		}
		
		_exit_critical_bh(&xframe_queue->lock, &irqL);
	}
	
#if defined(WIFI_WMM) && defined(WMM_APSD)
	if ((QOS_ENABLE) && (APSD_ENABLE) && (pstat->QosEnabled) && (pstat->apsd_pkt_buffering)) {
		if ((!(pstat->apsd_bitmap & 0x01) || (tx_servq_len(&pstat->tx_queue[VO_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x02) || (tx_servq_len(&pstat->tx_queue[VI_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x04) || (tx_servq_len(&pstat->tx_queue[BK_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x08) || (tx_servq_len(& pstat->tx_queue[BE_QUEUE]) == 0)))
			pstat->apsd_pkt_buffering = 0;
	}
#endif
	
	// for BC/MC frames
	if (list_empty(&priv->sleep_list)) {
		activate_mc_queue_xmit(priv, 0);
	}

#ifdef __ECOS
	triggered_wlan_tx_tasklet(priv);
#else
	tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
#endif
}

void process_dzqueue(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat;
	struct list_head *phead = &priv->wakeup_list;
	struct list_head *plist;
	unsigned long flags;
	
	while(1)
	{
		plist = NULL;
		
		SAVE_INT_AND_CLI(flags);
		SMP_LOCK_WAKEUP_LIST(flags);

		if (!list_empty(phead)) {
			plist = phead->next;
			list_del_init(plist);
		}

		SMP_UNLOCK_WAKEUP_LIST(flags);
		RESTORE_INT(flags);
		
		if (NULL == plist) break;
		
		pstat = list_entry(plist, struct stat_info, wakeup_list);
		
		DEBUG_INFO("Del fr wakeup_list %02X%02X%02X%02X%02X%02X\n",
			pstat->hwaddr[0],pstat->hwaddr[1],pstat->hwaddr[2],pstat->hwaddr[3],pstat->hwaddr[4],pstat->hwaddr[5]);
		
		wakeup_sta_xmit(priv, pstat);
	}
}

void process_mcast_dzqueue(struct rtl8192cd_priv *priv)
{
	priv->tx_mc_pkt_num = 0;
	
	if (activate_mc_queue_xmit(priv, 1)) {
#ifdef __ECOS
		triggered_wlan_tx_tasklet(priv);
#else
		tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
#endif
	}
}

void process_APSD_dz_queue(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned short tid)
{
	int mapping_q_num[4] = {VO_QUEUE, VI_QUEUE, BK_QUEUE, BE_QUEUE};
	int q_num;
	int offset, mask;
	
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;

	if ((((tid == 7) || (tid == 6)) && !(pstat->apsd_bitmap & 0x01))
			|| (((tid == 5) || (tid == 4)) && !(pstat->apsd_bitmap & 0x02))
			|| (((tid == 3) || (tid == 0)) && !(pstat->apsd_bitmap & 0x08))
			|| (((tid == 2) || (tid == 1)) && !(pstat->apsd_bitmap & 0x04))) {
		DEBUG_INFO("RcvQosNull legacy ps tid=%d", tid);
		return;
	}
	
	if (pstat->apsd_pkt_buffering == 0)
		goto sendQosNull;
	
	if ((!(pstat->apsd_bitmap & 0x01) || (tx_servq_len(&pstat->tx_queue[VO_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x02) || (tx_servq_len(&pstat->tx_queue[VI_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x04) || (tx_servq_len(&pstat->tx_queue[BK_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x08) || (tx_servq_len(&pstat->tx_queue[BE_QUEUE]) == 0))) {
		pstat->apsd_pkt_buffering = 0;
		//send QoS Null packet
sendQosNull:
		SendQosNullData(priv, pstat->hwaddr);
		DEBUG_INFO("sendQosNull  tid=%d\n", tid);
		return;
	}
	
	for (offset = 0; offset < ARRAY_SIZE(mapping_q_num); ++offset) 	{
		mask = 1 << offset;
		
		if (pstat->apsd_bitmap & mask) {
			q_num = mapping_q_num[offset];
			
			ptxservq = &pstat->tx_queue[q_num];
			xframe_queue = &ptxservq->xframe_queue;
			
			_enter_critical_bh(&xframe_queue->lock, &irqL);
			
			if (_rtw_queue_empty(xframe_queue) == FALSE) {
				sta_queue = &priv->pshare->tx_pending_sta_queue[q_num];
				
				_rtw_spinlock(&sta_queue->lock);
				
				if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
					rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
					++sta_queue->qlen;
				}
				
				_rtw_spinunlock(&sta_queue->lock);
			}
			
			_exit_critical_bh(&xframe_queue->lock, &irqL);
		}
	}
	
	// family_add: Need to send packet directly ???
}

