/*
 *  SDIO RX handle routines
 *
 *  $Id: 8188e_sdio_recv.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8188E_SDIO_RECV_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"


int rtw_enqueue_recvbuf(struct recv_buf *precvbuf, _queue *queue)
{
	_irqL irqL;	

	_enter_critical(&queue->lock, &irqL);
	
	rtw_list_insert_tail(&precvbuf->list, get_list_head(queue));
	
	++queue->qlen;
	
	_exit_critical(&queue->lock, &irqL);

	return SUCCESS;
	
}

struct recv_buf *rtw_dequeue_recvbuf(_queue *queue)
{
	_irqL irqL;
	struct recv_buf *precvbuf = NULL;
	_list *plist, *phead;
	
	plist = NULL;
	
	phead = get_list_head(queue);
	
	_enter_critical(&queue->lock, &irqL);
	
	if (rtw_is_list_empty(phead) == FALSE) {

		plist = get_next(phead);

		rtw_list_delete(plist);
		
		--queue->qlen;
	}

	_exit_critical(&queue->lock, &irqL);
	
	if (NULL != plist) {
		precvbuf = LIST_CONTAINOR(plist, struct recv_buf, list);
	}

	return precvbuf;
}

union recv_frame *rtw_alloc_recvframe(_queue *pfree_recv_queue)
{
	_irqL irqL;
	union recv_frame  *precvframe = NULL;
	_list *plist, *phead;
	
	plist = NULL;
	
	phead = get_list_head(pfree_recv_queue);
	
	_enter_critical_bh(&pfree_recv_queue->lock, &irqL);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		
		plist = get_next(phead);
		
		rtw_list_delete(plist);
		
		--pfree_recv_queue->qlen;
	}
	
	_exit_critical_bh(&pfree_recv_queue->lock, &irqL);
	
	if (NULL !=  plist) {
		precvframe = LIST_CONTAINOR(plist, union recv_frame, u);
	}
	
	return precvframe;
}

int rtw_free_recvframe(struct rtl8192cd_priv *priv, union recv_frame *precvframe, _queue *pfree_recv_queue)
{
	_irqL irqL;
	
	if(precvframe->u.hdr.pkt)
	{
		rtl_kfree_skb(priv, precvframe->u.hdr.pkt, _SKB_RX_);
		precvframe->u.hdr.pkt = NULL;
	}
	
	_enter_critical_bh(&pfree_recv_queue->lock, &irqL);
	
	rtw_list_insert_tail(&(precvframe->u.hdr.list), get_list_head(pfree_recv_queue));
	
	++pfree_recv_queue->qlen;
	
	_exit_critical_bh(&pfree_recv_queue->lock, &irqL);

	return SUCCESS;
}

int recv_func(struct rtl8192cd_priv *priv, void *pcontext, struct recv_stat *prxstat, struct phy_stat *pphy_info)
{
	union recv_frame *precvframe = (union recv_frame *)pcontext;
	int retval = FAIL;
	struct rx_desc *pdesc = (struct rx_desc *)prxstat;
	struct rx_frinfo *pfrinfo;
	struct sk_buff *pskb;
	unsigned int cmd;
	unsigned int rtl8192cd_ICV, privacy;
	unsigned char rx_rate;
	struct stat_info *pstat;
	unsigned char *pframe;
	struct recv_priv *precvpriv = &priv->recvpriv;

	pskb = (struct sk_buff *)(precvframe->u.hdr.pkt);

	cmd = get_desc(pdesc->Dword0);
	pfrinfo = get_pfrinfo(pskb);

	if (cmd & RX_CRC32) {
		/*printk("CRC32 happens~!!\n");*/
		rx_pkt_exception(priv, cmd);
		goto _exit_recv_func;
	}
	
	if (!IS_DRV_OPEN(priv)) {
		goto _exit_recv_func;
	}
	
	pfrinfo->pskb = pskb;
	
	if (cmd & RX_ICVERR) {
		rtl8192cd_ICV = privacy = 0;
		pstat = NULL;
		
		pframe = get_pframe(pfrinfo);
		#if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
		if (get_tofr_ds(pframe) == 3) {
			pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
		} else
		#endif
			{pstat = get_stainfo(priv, get_sa(pframe));}

		if (!pstat) {
			rtl8192cd_ICV++;
		} else {
			if (OPMODE & WIFI_AP_STATE) {
				#if defined(WDS) || defined(CONFIG_RTK_MESH)
				if (get_tofr_ds(pframe) == 3){
					#if defined(CONFIG_RTK_MESH)
					if(priv->pmib->dot1180211sInfo.mesh_enable) {
						privacy = (IS_MCAST(GetAddr1Ptr(pframe))) ? _NO_PRIVACY_ : priv->pmib->dot11sKeysTable.dot11Privacy;
					} else
					#endif
						{privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;}
				}
				else
				#endif	/*	defined(WDS) || defined(CONFIG_RTK_MESH)	*/
					{privacy = get_sta_encrypt_algthm(priv, pstat);}
			}
			#if defined(CLIENT_MODE)
			else {
					privacy = get_sta_encrypt_algthm(priv, pstat);
			}
			#endif
			
			if (privacy != _CCMP_PRIVACY_)
				rtl8192cd_ICV++;
		}

		if (rtl8192cd_ICV) {
			rx_pkt_exception(priv, cmd);
			goto _exit_recv_func;
		}
	}
	
	pfrinfo->pktlen = (cmd & RX_PktLenMask) - _CRCLNG_;
	pfrinfo->driver_info_size = ((cmd >> RX_DrvInfoSizeSHIFT) & RX_DrvInfoSizeMask)<<3;
	pfrinfo->rxbuf_shift = (cmd & (RX_ShiftMask << RX_ShiftSHIFT)) >> RX_ShiftSHIFT;
	pfrinfo->sw_dec = (cmd & RX_SwDec) >> 27;
	
//	pfrinfo->pktlen -= pfrinfo->rxbuf_shift;
	if ((pfrinfo->pktlen > 0x2000) || (pfrinfo->pktlen < 16)) {
		printk("pfrinfo->pktlen=%d, goto rx_reuse\n",pfrinfo->pktlen);
		goto _exit_recv_func;
	}

	pfrinfo->driver_info = (struct RxFWInfo *)pphy_info;
	pfrinfo->physt = (get_desc(pdesc->Dword0) & RX_PHYST)? 1:0;
	pfrinfo->faggr = (get_desc(pdesc->Dword1) & RX_FAGGR)? 1:0;
	pfrinfo->paggr = (get_desc(pdesc->Dword1) & RX_PAGGR)? 1:0;
	pfrinfo->rx_bw = (get_desc(pdesc->Dword3) & RX_BW)? 1:0;
	pfrinfo->rx_splcp = (get_desc(pdesc->Dword3) & RX_SPLCP)? 1:0;

	rx_rate = get_desc(pdesc->Dword3) & RX_RxMcsMask;
	if (rx_rate < 12) {
		pfrinfo->rx_rate = dot11_rate_table[rx_rate];
	} else {
		pfrinfo->rx_rate = HT_RATE_ID + (rx_rate - 12);
	}

	if (!pfrinfo->physt) {
		pfrinfo->rssi = 0;
	} else {
#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
		if (IS_OUTSRC_CHIP(priv))
#endif
		{
			translate_rssi_sq_outsrc(priv, pfrinfo, rx_rate);
		}
#endif
	
#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
		if (!IS_OUTSRC_CHIP(priv))
#endif
		{
			translate_rssi_sq(priv, pfrinfo);
		}
#endif
	}

	SNMP_MIB_INC(dot11ReceivedFragmentCount, 1);

	#if defined(SW_ANT_SWITCH)
	if (priv->pshare->rf_ft_var.antSw_enable) {
		dm_SWAW_RSSI_Check(priv, pfrinfo);
	}
	#endif

	if (!validate_mpdu(priv, pfrinfo)) {
		precvframe->u.hdr.pkt = NULL;
	}
	
	retval = SUCCESS;
	
_exit_recv_func:
	rtw_free_recvframe(priv, precvframe, &precvpriv->free_recv_queue);

	return retval;
}

s32 rtw_recv_entry(struct rtl8192cd_priv *priv, union recv_frame *precvframe, struct recv_stat *prxstat, struct phy_stat *pphy_info)
{
	return recv_func(priv, precvframe, prxstat, pphy_info);
}

void rtl8188es_free_recv_priv(struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;

	_rtw_spinlock_free(&precvpriv->free_recv_buf_queue.lock);
	_rtw_spinlock_free(&precvpriv->recv_buf_pending_queue.lock);

	if (precvpriv->pallocated_recv_buf)
	{
		rtw_mfree(precvpriv->pallocated_recv_buf, NR_RECVBUFF * sizeof(struct recv_buf) + 4);
		precvpriv->pallocated_recv_buf = NULL;
		precvpriv->precv_buf = NULL;
	}
	
	if (precvpriv->recvbuf_mem_head) {
		rtw_mfree(precvpriv->recvbuf_mem_head, MAX_RECVBUF_MEM_SZ);
		precvpriv->recvbuf_mem_head = NULL;
	}
}

int rtl8188es_init_recv_priv(struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;
	struct recv_buf *precvbuf;
	int i;

	//init recv_buf
	_rtw_init_queue(&precvpriv->free_recv_buf_queue);
	_rtw_init_queue(&precvpriv->recv_buf_pending_queue);

	precvpriv->recvbuf_mem_head = rtw_malloc(MAX_RECVBUF_MEM_SZ);
	if (NULL == precvpriv->recvbuf_mem_head) {
		printk("alloc recvbuf_mem fail!(size %d)\n", MAX_RECVBUF_MEM_SZ);
		return FAIL;
	}
	precvpriv->recvbuf_mem_data = precvpriv->recvbuf_mem_tail = precvpriv->recvbuf_mem_head;
#ifdef RECVBUF_DEBUG
	precvpriv->recvbuf_mem_end = precvpriv->recvbuf_mem_head + MAX_RECVBUF_MEM_SZ -1;
	*precvpriv->recvbuf_mem_end = RECVBUF_POISON_END;
#else
	precvpriv->recvbuf_mem_end = precvpriv->recvbuf_mem_head + MAX_RECVBUF_MEM_SZ;
#endif
	
	precvpriv->pallocated_recv_buf = rtw_zmalloc(NR_RECVBUFF *sizeof(struct recv_buf) + 4);

	if (precvpriv->pallocated_recv_buf == NULL) {
		printk("alloc recv_buf fail!(size %d)\n", (NR_RECVBUFF *sizeof(struct recv_buf) + 4));
		goto exit;
	}

	precvpriv->precv_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(precvpriv->pallocated_recv_buf), 4);

	precvbuf = (struct recv_buf*)precvpriv->precv_buf;

	for (i=0 ; i < NR_RECVBUFF ; i++)
	{
		_rtw_init_listhead(&precvbuf->list);
		rtw_list_insert_tail(&precvbuf->list, &(precvpriv->free_recv_buf_queue.queue));
		precvbuf++;
	}
	
	precvpriv->free_recv_buf_queue.qlen = NR_RECVBUFF;

	//init tasklet
	tasklet_init(&precvpriv->recv_tasklet,
	     (void(*)(unsigned long))rtl8188es_recv_tasklet,
	     (unsigned long)priv);

	return SUCCESS;

exit:
	rtl8188es_free_recv_priv(priv);

	return FAIL;
}

void update_recvframe_attrib_88e(union recv_frame *precvframe, struct recv_stat *prxstat)
{
	struct rx_pkt_attrib	*pattrib;
	struct recv_stat	report;

	report.rxdw0 = le32_to_cpu(prxstat->rxdw0);
	report.rxdw1 = le32_to_cpu(prxstat->rxdw1);
	report.rxdw2 = le32_to_cpu(prxstat->rxdw2);
	report.rxdw3 = le32_to_cpu(prxstat->rxdw3);
	report.rxdw4 = le32_to_cpu(prxstat->rxdw4);
	report.rxdw5 = le32_to_cpu(prxstat->rxdw5);

	pattrib = &precvframe->u.hdr.attrib;
	memset(pattrib, 0, sizeof(struct rx_pkt_attrib));

	pattrib->crc_err = (u8)((report.rxdw0 >> 14) & 0x1);;//(u8)prxreport->crc32;

	// update rx report to recv_frame attribute
	pattrib->pkt_rpt_type = (u8)((report.rxdw3 >> 14) & 0x3);//prxreport->rpt_sel;

	if(pattrib->pkt_rpt_type == NORMAL_RX)//Normal rx packet
	{
		pattrib->pkt_len = (u16)(report.rxdw0 &0x00003fff);//(u16)prxreport->pktlen;
		pattrib->drvinfo_sz = (u8)((report.rxdw0 >> 16) & 0xf) * 8;//(u8)(prxreport->drvinfosize << 3);

		pattrib->physt =  (u8)((report.rxdw0 >> 26) & 0x1);//(u8)prxreport->physt;

		pattrib->bdecrypted = (report.rxdw0 & BIT(27))? 0:1;//(u8)(prxreport->swdec ? 0 : 1);
		pattrib->encrypt = (u8)((report.rxdw0 >> 20) & 0x7);//(u8)prxreport->security;

		pattrib->qos = (u8)((report.rxdw0 >> 23) & 0x1);//(u8)prxreport->qos;
		pattrib->priority = (u8)((report.rxdw1 >> 8) & 0xf);//(u8)prxreport->tid;

		pattrib->seq_num = (u16)(report.rxdw2 & 0x00000fff);//(u16)prxreport->seq;
		pattrib->frag_num = (u8)((report.rxdw2 >> 12) & 0xf);//(u8)prxreport->frag;
		pattrib->mfrag = (u8)((report.rxdw1 >> 27) & 0x1);//(u8)prxreport->mf;
		pattrib->mdata = (u8)((report.rxdw1 >> 26) & 0x1);//(u8)prxreport->md;

		pattrib->icv_err = (u8)((report.rxdw0 >> 15) & 0x1);//(u8)prxreport->icverr;
		//pattrib->shift_sz = (u8)prxreport->shift;
		//pattrib->shift_sz = (u8)((report.rxdw0 >> 24) & 0x3);

	}
	else if(pattrib->pkt_rpt_type == TX_REPORT1)//CCX
	{
		pattrib->pkt_len = TX_RPT1_PKT_LEN;
		pattrib->drvinfo_sz = 0;
	}
	else if(pattrib->pkt_rpt_type == TX_REPORT2)// TX RPT
	{
		pattrib->pkt_len =(u16)(report.rxdw0 & 0x3FF);//Rx length[9:0]
		pattrib->drvinfo_sz = 0;

		//
		// Get TX report MAC ID valid.
		//
		pattrib->MacIDValidEntry[0] = report.rxdw4;
		pattrib->MacIDValidEntry[1] = report.rxdw5;

	}
	else if(pattrib->pkt_rpt_type == HIS_REPORT)// USB HISR RPT
	{
		pattrib->pkt_len = (u16)(report.rxdw0 &0x00003fff);//(u16)prxreport->pktlen;
	}

}

static int recvbuf2recvframe(struct rtl8192cd_priv *priv, struct recv_buf *precvbuf)
{
	u8	*pbuf;
	u8	shift_sz = 0;
	u32	pkt_offset, skb_len, alloc_sz;
	struct recv_stat	*prxstat;
	struct phy_stat		*pphy_info = NULL;
	_pkt			*pkt_copy = NULL;
	union recv_frame	*precvframe = NULL;
	struct rx_pkt_attrib	*pattrib = NULL;
	struct recv_priv	*precvpriv = &priv->recvpriv;
	_queue			*pfree_recv_queue = &precvpriv->free_recv_queue;
	int nr_recvframe = 0;

	pbuf = precvbuf->pdata;

	do {
		prxstat = (struct recv_stat *)pbuf;

		precvframe = rtw_alloc_recvframe(pfree_recv_queue);
		if (precvframe == NULL)
		{
			printk("%s()-%d: rtw_alloc_recvframe() failed! RX Drop!\n", __FUNCTION__, __LINE__);
			precvpriv->nr_out_of_recvframe++;
			goto _exit_recvbuf2recvframe;
		}

		update_recvframe_attrib_88e(precvframe, prxstat);
		
		pattrib = &precvframe->u.hdr.attrib;

		if (pattrib->physt
#ifdef CONFIG_RTL_88E_SUPPORT
		  && (pattrib->pkt_rpt_type == NORMAL_RX)
#endif
		   )
		{
			pphy_info = (struct phy_stat *)(pbuf + RXDESC_OFFSET);
		}

		pkt_offset = RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz + pattrib->pkt_len;

		if ((pbuf + pkt_offset) > precvbuf->ptail)
		{	
			printk("%s()-%d: RX Warning! next pkt len exceed ptail!\n", __FUNCTION__, __LINE__);
			rtw_free_recvframe(priv, precvframe, pfree_recv_queue);
			goto _exit_recvbuf2recvframe;
		}

		//	Modified by Albert 20101213
		//	For 8 bytes IP header alignment.
		if (pattrib->qos)	//	Qos data, wireless lan header length is 26
		{
			shift_sz = 6;
		}
		else
		{
			shift_sz = 0;
		}

		skb_len = pattrib->pkt_len;

		// for first fragment packet, driver need allocate 1536+drvinfo_sz+RXDESC_SIZE to defrag packet.
		// modify alloc_sz for recvive crc error packet by thomas 2011-06-02
		if ((pattrib->mfrag == 1) && (pattrib->frag_num == 0)) {
			//alloc_sz = 1664;	//1664 is 128 alignment.
			if(skb_len <= 1650)
				alloc_sz = 1664;
			else
				alloc_sz = skb_len + 14;
		}
		else {
			alloc_sz = skb_len;
			//	6 is for IP header 8 bytes alignment in QoS packet case.
			//	8 is for skb->data 4 bytes alignment.
			alloc_sz += 14;
		}

		alloc_sz += sizeof(struct rx_frinfo);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)) // http://www.mail-archive.com/netdev@vger.kernel.org/msg17214.html
		pkt_copy = dev_alloc_skb(alloc_sz);
#else			
		pkt_copy = netdev_alloc_skb(priv->dev, alloc_sz);
#endif		
		if (pkt_copy)
		{
			pkt_copy->dev = priv->dev;
			precvframe->u.hdr.pkt = pkt_copy;
			skb_reserve( pkt_copy, sizeof(struct rx_frinfo));
			skb_reserve( pkt_copy, 8 - ((SIZE_PTR)( pkt_copy->data ) & 7 ));//force pkt_copy->data at 8-byte alignment address
			skb_reserve( pkt_copy, shift_sz );//force ip_hdr at 8-byte alignment address according to shift_sz.
			memcpy(pkt_copy->data, (pbuf + RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz), skb_len);
		}
		else
		{
			printk("recvbuf2recvframe:can not allocate memory for skb copy\n");

			precvframe->u.hdr.pkt = NULL;
			rtw_free_recvframe(priv, precvframe, pfree_recv_queue);

			goto _exit_recvbuf2recvframe;
		}
		
#ifdef ENABLE_RTL_SKB_STATS
		rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif

		if (pattrib->pkt_rpt_type == NORMAL_RX)//Normal rx packet
		{
			nr_recvframe++;
			if (rtw_recv_entry(priv, precvframe, prxstat, pphy_info) != SUCCESS)
			{
				//printk("recvbuf2recvframe: rtw_recv_entry(precvframe) != SUCCESS\n");
			}
		}
		else { // pkt_rpt_type == TX_REPORT1-CCX, TX_REPORT2-TX RTP, HIS_REPORT-USB HISR RTP

			//enqueue recvframe to txrtp queue
			if (unlikely(pattrib->pkt_rpt_type == TX_REPORT1)) {
				printk("rx CCX\n");
			}
			else if (pattrib->pkt_rpt_type == TX_REPORT2) {
				//printk("rx TX RPT\n");
#ifdef TXREPORT
#ifdef RATEADAPTIVE_BY_ODM
				ODM_RA_TxRPT2Handle_8188E(
							ODMPTR,
							pkt_copy->data, //precvframe->u.hdr.rx_data,
							pattrib->pkt_len,
							pattrib->MacIDValidEntry[0],
							pattrib->MacIDValidEntry[1]
							);
#else
				{
					struct rx_frinfo *pfrinfo = get_pfrinfo(pkt_copy);
					pfrinfo->pktlen = pattrib->pkt_len;
					
					RTL8188E_TxReportHandler(priv, pkt_copy, pattrib->MacIDValidEntry[0],
						pattrib->MacIDValidEntry[1], (struct rx_desc *)prxstat);
				}
#endif
#endif
			}

			rtw_free_recvframe(priv, precvframe, pfree_recv_queue);
		}

		// Page size of receive package is 128 bytes alignment => DMA AGG
		// refer to _InitTransferPageSize()
		pkt_offset = _RND128(pkt_offset);
		precvbuf->pdata += pkt_offset;
		pbuf = precvbuf->pdata;
		precvframe = NULL;
		pkt_copy = NULL;

	} while(pbuf < precvbuf->ptail);
	
	priv->pshare->nr_recvframe_in_recvbuf = nr_recvframe;

_exit_recvbuf2recvframe:

	return SUCCESS;
}

void rtw_flush_recvbuf_pending_queue(struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;
	struct recv_buf *precvbuf;
	
	while (NULL != (precvbuf = rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue))) {
		precvpriv->recvbuf_mem_data = precvbuf->pend;
		rtw_enqueue_recvbuf(precvbuf, &precvpriv->free_recv_buf_queue);
	}
}

void rtl8188es_recv_tasklet(void *p)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)p;
	struct recv_priv *precvpriv = &priv->recvpriv;
	struct recv_buf *precvbuf;
	int nr_recvbuf = 0;

	do {
		if ((priv->pshare->bDriverStopped == TRUE) || (priv->pshare->bSurpriseRemoved == TRUE))
		{
			printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
				__FUNCTION__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved);
			break;
		}

		precvbuf = rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue);
		if (NULL == precvbuf)
			break;

		nr_recvbuf++;
		recvbuf2recvframe(priv, precvbuf);

		precvpriv->recvbuf_mem_data = precvbuf->pend;
		rtw_enqueue_recvbuf(precvbuf, &precvpriv->free_recv_buf_queue);
	} while (1);
	
	priv->pshare->nr_recvbuf_handled_in_tasklet = nr_recvbuf;
}

struct recv_buf* sd_recv_rxfifo(struct rtl8192cd_priv *priv, u32 size)
{
	int err;
	u8 *preadbuf;
	struct recv_priv *precvpriv = &priv->recvpriv;
	struct recv_buf *precvbuf = NULL;

	u32 allocsize;
	SIZE_PTR tmpaddr = 0;
	SIZE_PTR alignment = 0;
	
	BUG_ON(size > MAX_RECVBUF_SZ);

	precvbuf = rtw_dequeue_recvbuf(&precvpriv->free_recv_buf_queue);
	if (NULL == precvbuf) {
		DEBUG_INFO("%s: alloc recvbuf FAIL!\n", __FUNCTION__);
		precvpriv->nr_out_of_recvbuf++;
		goto err_exit;
	}
	
	// prepare buffer size to meet sdio_read_port() requirement
	allocsize = _RND(size, priv->pshare->block_transfer_len);
	
	// assign buffer memory to recvbuf
	preadbuf = NULL;
	if (precvpriv->recvbuf_mem_tail >= precvpriv->recvbuf_mem_data) {
		if ((unsigned long)(precvpriv->recvbuf_mem_end - precvpriv->recvbuf_mem_tail) >= allocsize)
			preadbuf = precvpriv->recvbuf_mem_tail;
		else if ((unsigned long)(precvpriv->recvbuf_mem_data - precvpriv->recvbuf_mem_head) > allocsize)
			preadbuf = precvpriv->recvbuf_mem_head;
	} else {
		if ((unsigned long)(precvpriv->recvbuf_mem_data - precvpriv->recvbuf_mem_tail) > allocsize)
			preadbuf = precvpriv->recvbuf_mem_tail;
	}
	if (NULL == preadbuf) {
		DEBUG_INFO("%s: alloc recvbuf mem FAIL!\n", __FUNCTION__);
		precvpriv->nr_out_of_recvbuf_mem++;
		goto err_exit;
	}
	
	err = sdio_read_port(priv, WLAN_RX0FF_DEVICE_ID, size, preadbuf);
	if (0 != err) {
		printk("%s: read port FAIL! (err=%d)\n", __FUNCTION__, err);
		goto err_exit;
	}
	
#ifdef RECVBUF_DEBUG
	if (*precvpriv->recvbuf_mem_end != RECVBUF_POISON_END) {
		printk("[%s] BUG!! recvbuf_mem overwritten!!\n", __FUNCTION__);
		*precvpriv->recvbuf_mem_end = RECVBUF_POISON_END;
	}
#endif
	
	precvbuf->phead = preadbuf;
	precvbuf->pdata = preadbuf;
	precvbuf->ptail = preadbuf + size;
	
	tmpaddr = (SIZE_PTR)precvbuf->ptail;
	alignment = tmpaddr & (RECVBUFF_ALIGN_SZ - 1);
	if (alignment)
		alignment = RECVBUFF_ALIGN_SZ - alignment;
	precvbuf->pend = precvbuf->ptail + alignment;
	if (precvbuf->pend > precvpriv->recvbuf_mem_end)
		precvbuf->pend = precvpriv->recvbuf_mem_end;
	
	precvpriv->recvbuf_mem_tail = precvbuf->pend;

	return precvbuf;

err_exit:

	if (precvbuf) {
		rtw_enqueue_recvbuf(precvbuf, &precvpriv->free_recv_buf_queue);
	}

	return NULL;
}

void sd_rxhandler(struct rtl8192cd_priv *priv, struct recv_buf *precvbuf)
{
	struct recv_priv *precvpriv = &priv->recvpriv;
	
	rtw_enqueue_recvbuf(precvbuf, &precvpriv->recv_buf_pending_queue);
	
	// Considering TP, do every one without checking if recv_buf_pending_queue.qlen == 1
	tasklet_schedule(&precvpriv->recv_tasklet);
}

int _rtw_init_recv_priv(struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;
	union recv_frame *precvframe;
	int i;

	_rtw_spinlock_init(&precvpriv->lock);

	//init recv_frame
	_rtw_init_queue(&precvpriv->free_recv_queue);
	_rtw_init_queue(&precvpriv->recv_pending_queue);

	precvpriv->pallocated_frame_buf = rtw_zvmalloc(NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ);
	
	if (NULL == precvpriv->pallocated_frame_buf) {
		printk("alloc recv_frame fail!(size %d)\n", (NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ));
		return FAIL;
	}

	precvpriv->precv_frame_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(precvpriv->pallocated_frame_buf), RXFRAME_ALIGN_SZ);

	precvframe = (union recv_frame*) precvpriv->precv_frame_buf;

	for (i=0; i < NR_RECVFRAME ; i++)
	{
		_rtw_init_listhead(&(precvframe->u.list));

		rtw_list_insert_tail(&(precvframe->u.list), &(precvpriv->free_recv_queue.queue));

		precvframe->u.hdr.pkt = NULL;

		precvframe++;
	}

	precvpriv->free_recv_queue.qlen = NR_RECVFRAME;

	if (FAIL == rtl8188es_init_recv_priv(priv)) {
		rtw_vmfree(precvpriv->pallocated_frame_buf, NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ);
		
		precvpriv->pallocated_frame_buf = NULL;
		precvpriv->precv_frame_buf = NULL;

		return FAIL;
	}

	return SUCCESS;
}

void _rtw_free_recv_priv (struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;

	_rtw_spinlock_free(&precvpriv->lock);

	_rtw_spinlock_free(&precvpriv->free_recv_queue.lock);
	_rtw_spinlock_free(&precvpriv->recv_pending_queue.lock);

	if (precvpriv->pallocated_frame_buf) {
		rtw_vmfree(precvpriv->pallocated_frame_buf, NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ);

		precvpriv->pallocated_frame_buf = NULL;
		precvpriv->precv_frame_buf = NULL;
	}

	rtl8188es_free_recv_priv(priv);
}

