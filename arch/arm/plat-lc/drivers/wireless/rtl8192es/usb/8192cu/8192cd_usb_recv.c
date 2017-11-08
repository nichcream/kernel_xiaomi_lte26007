/*
 *  USB RX handle routines
 *
 *  $Id: 8192cd_usb_recv.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_USB_RECV_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "./8192cd.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"


#ifdef CONFIG_RTL_92C_SUPPORT
#define INTERRUPT_MSG_FORMAT_LEN		sizeof(INTERRUPT_MSG_FORMAT_EX)
#elif defined(CONFIG_RTL_88E_SUPPORT)
#define INTERRUPT_MSG_FORMAT_LEN		60
#endif


#define _func_enter_
#define _func_exit_

static u32 usb_read_port(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *rmem);

union recv_frame *rtw_alloc_recvframe (_queue *pfree_recv_queue)
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
	union recv_frame *precvframe;
	int retval = FAIL;
	struct rx_desc *pdesc;
	struct rx_frinfo *pfrinfo;
	struct sk_buff *pskb;
	unsigned int cmd;
	unsigned int rtl8192cd_ICV, privacy;
	struct stat_info *pstat;
	unsigned char *pframe;
	
	struct recv_priv *precvpriv = &priv->recvpriv;
	_queue *pfree_recv_queue = &precvpriv->free_recv_queue;

	precvframe = (union recv_frame *)pcontext;
	
	pskb = (struct sk_buff *)(precvframe->u.hdr.pkt);

	pdesc = (struct rx_desc *)prxstat;
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
	
	pfrinfo->pktlen -= pfrinfo->rxbuf_shift;
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

	if ((get_desc(pdesc->Dword3)&RX_RxMcsMask) < 12) {
		pfrinfo->rx_rate = dot11_rate_table[(get_desc(pdesc->Dword3)&RX_RxMcsMask)];
	} else {
		pfrinfo->rx_rate = 0x80|((get_desc(pdesc->Dword3)&RX_RxMcsMask)-12);
	}

	if (!pfrinfo->physt) {
		pfrinfo->rssi = 0;
	} else {
#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
		if(IS_OUTSRC_CHIP(priv))
#endif
		{
			translate_rssi_sq_outsrc(priv, pfrinfo, (get_desc(pdesc->Dword3)&RX_RxMcsMask));
		}
#endif
	
#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
		if(!IS_OUTSRC_CHIP(priv))
#endif
		{
			translate_rssi_sq(priv, pfrinfo);
		}
#endif
	}

	SNMP_MIB_INC(dot11ReceivedFragmentCount, 1);

	#if defined(SW_ANT_SWITCH)
	if(priv->pshare->rf_ft_var.antSw_enable) {
		dm_SWAW_RSSI_Check(priv, pfrinfo);
	}
	#endif

	if (!validate_mpdu(priv, pfrinfo)) {
		precvframe->u.hdr.pkt = NULL;
	}
	
	retval = SUCCESS;
	
_exit_recv_func:
	rtw_free_recvframe(priv, precvframe, pfree_recv_queue);

	return retval;
}

s32 rtw_recv_entry(struct rtl8192cd_priv *priv, union recv_frame *precvframe, struct recv_stat *prxstat, struct phy_stat *pphy_info)
{
	return recv_func(priv, precvframe, prxstat, pphy_info);
}

int rtw_os_recv_resource_alloc(struct rtl8192cd_priv *priv, union recv_frame *precvframe)
{
	int	res=SUCCESS;
	
	precvframe->u.hdr.pkt_newalloc = precvframe->u.hdr.pkt = NULL;

	return res;
}

void rtw_os_recv_resource_free(struct recv_priv *precvpriv)
{

}

int rtw_os_recvbuf_resource_alloc(struct rtl8192cd_priv *priv, struct recv_buf *precvbuf)
{
#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	struct usb_device	*pusbd = priv->pshare->pusbdev;
#endif

	precvbuf->irp_pending = FALSE;
	precvbuf->purb = usb_alloc_urb(0, GFP_KERNEL);
	if (NULL == precvbuf->purb){
		return FAIL;
	}

	precvbuf->pskb = NULL;

	precvbuf->pallocated_buf = precvbuf->pbuf = NULL;

	precvbuf->pdata = precvbuf->phead = precvbuf->ptail = precvbuf->pend = NULL;

	precvbuf->transfer_len = 0;

	precvbuf->len = 0;

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	precvbuf->pallocated_buf = rtw_usb_buffer_alloc(pusbd, (size_t)precvbuf->alloc_sz, GFP_ATOMIC, &precvbuf->dma_transfer_addr);
	if (NULL == precvbuf->pallocated_buf)
		return FAIL;
	precvbuf->pbuf = precvbuf->pallocated_buf;
#endif // CONFIG_USE_USB_BUFFER_ALLOC_RX
	
	return SUCCESS;
}

//free os related resource in struct recv_buf
int rtw_os_recvbuf_resource_free(struct rtl8192cd_priv *priv, struct recv_buf *precvbuf)
{
	int ret = SUCCESS;

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX

	struct usb_device	*pusbd = priv->pshare->pusbdev;
	
	if (precvbuf->pallocated_buf) {
		rtw_usb_buffer_free(pusbd, (size_t)precvbuf->alloc_sz, precvbuf->pallocated_buf, precvbuf->dma_transfer_addr);
		precvbuf->pallocated_buf =  NULL;
		precvbuf->dma_transfer_addr = 0;
	}
#endif // CONFIG_USE_USB_BUFFER_ALLOC_RX

	if(precvbuf->purb)
	{
		//usb_kill_urb(precvbuf->purb);
		usb_free_urb(precvbuf->purb);
	}

	if(precvbuf->pskb)
		dev_kfree_skb_any(precvbuf->pskb);

	return ret;

}

void rtl8192cu_init_recvbuf(struct rtl8192cd_priv *priv, struct recv_buf *precvbuf)
{
	precvbuf->transfer_len = 0;
	precvbuf->len = 0;
	precvbuf->ref_cnt = 0;
	if(precvbuf->pbuf)
	{
		precvbuf->pdata = precvbuf->phead = precvbuf->ptail = precvbuf->pbuf;
		precvbuf->pend = precvbuf->pdata + MAX_RECVBUF_SZ;
	}
}

/* This function is the same as rtl8188eu_init_recv_priv */
int	rtl8192cu_init_recv_priv(struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;
	int	i, res = SUCCESS;
	struct recv_buf *precvbuf;

#ifdef CONFIG_RECV_THREAD_MODE	
	_rtw_init_sema(&precvpriv->recv_sema, 0);//will be removed
	_rtw_init_sema(&precvpriv->terminate_recvthread_sema, 0);//will be removed
#endif

	tasklet_init(&precvpriv->recv_tasklet,
	     (void(*)(unsigned long))rtl8192cu_recv_tasklet,
	     (unsigned long)priv);

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	precvpriv->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if(precvpriv->int_in_urb == NULL){
		printk("alloc_urb for interrupt in endpoint fail !!!!\n");
	}
	
	precvpriv->int_in_buf = rtw_zmalloc(INTERRUPT_MSG_FORMAT_LEN);
	if(precvpriv->int_in_buf == NULL){
		printk("alloc_mem for interrupt in endpoint fail !!!!\n");
	}
#endif

	//init recv_buf
	_rtw_init_queue(&precvpriv->free_recv_buf_queue);

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	_rtw_init_queue(&precvpriv->recv_buf_pending_queue);
#endif	// CONFIG_USE_USB_BUFFER_ALLOC_RX

	precvpriv->pallocated_recv_buf = rtw_zmalloc(NR_RECVBUFF *sizeof(struct recv_buf) + 4);
	if(precvpriv->pallocated_recv_buf==NULL){
		res= FAIL;
		printk("alloc recv_buf fail!(size %d)\n", (NR_RECVBUFF *sizeof(struct recv_buf) + 4));
		goto exit;
	}

	precvpriv->precv_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(precvpriv->pallocated_recv_buf), 4);

	precvbuf = (struct recv_buf*)precvpriv->precv_buf;

	for(i=0; i < NR_RECVBUFF ; i++)
	{
		_rtw_init_listhead(&precvbuf->list);

		_rtw_spinlock_init(&precvbuf->recvbuf_lock);

		precvbuf->alloc_sz = MAX_RECVBUF_SZ;

		res = rtw_os_recvbuf_resource_alloc(priv, precvbuf);
		if(res==FAIL){
			printk("alloc recvbuf resource fail!\n");
			break;
		}

		precvbuf->ref_cnt = 0;
		precvbuf->priv = priv;

		//rtw_list_insert_tail(&precvbuf->list, &(precvpriv->free_recv_buf_queue.queue));

		precvbuf++;
	}

	precvpriv->free_recv_buf_queue_cnt = NR_RECVBUFF;

	skb_queue_head_init(&precvpriv->rx_skb_queue);

#ifdef CONFIG_PREALLOC_RECV_SKB
	{
		int i;
		SIZE_PTR tmpaddr=0;
		SIZE_PTR alignment=0;
		_pkt *pskb=NULL;

		skb_queue_head_init(&precvpriv->free_recv_skb_queue);

		for(i=0; i<NR_PREALLOC_RECV_SKB; i++)
		{

	#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)) // http://www.mail-archive.com/netdev@vger.kernel.org/msg17214.html
			pskb = dev_alloc_skb(MAX_RECVBUF_SZ + RECVBUFF_ALIGN_SZ);
	#else
			pskb = netdev_alloc_skb(priv->dev, MAX_RECVBUF_SZ + RECVBUFF_ALIGN_SZ);
	#endif

			if(pskb)
			{
				pskb->dev = priv->dev;

				tmpaddr = (SIZE_PTR)pskb->data;
				alignment = tmpaddr & (RECVBUFF_ALIGN_SZ-1);
				skb_reserve(pskb, (RECVBUFF_ALIGN_SZ - alignment));

				skb_queue_tail(&precvpriv->free_recv_skb_queue, pskb);
			}

			pskb=NULL;
		}
	}
#endif // CONFIG_PREALLOC_RECV_SKB

exit:

	return res;

}

void rtl8192cu_free_recv_priv (struct rtl8192cd_priv *priv)
{
	int	i;
	struct recv_buf *precvbuf;
	struct recv_priv *precvpriv = &priv->recvpriv;

	if (precvpriv->pallocated_recv_buf) {
		precvbuf = (struct recv_buf *)precvpriv->precv_buf;

		for (i=0; i < NR_RECVBUFF; i++) {
			rtw_os_recvbuf_resource_free(priv, precvbuf);
			precvbuf++;
		}
		rtw_mfree(precvpriv->pallocated_recv_buf, NR_RECVBUFF *sizeof(struct recv_buf) + 4);
	}

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	if(precvpriv->int_in_urb)
		usb_free_urb(precvpriv->int_in_urb);
	
	if(precvpriv->int_in_buf)
		rtw_mfree(precvpriv->int_in_buf, INTERRUPT_MSG_FORMAT_LEN);
#endif

	if (skb_queue_len(&precvpriv->rx_skb_queue)) {
		printk(KERN_WARNING "rx_skb_queue not empty\n");
	}

	skb_queue_purge(&precvpriv->rx_skb_queue);

#ifdef CONFIG_PREALLOC_RECV_SKB
	if (skb_queue_len(&precvpriv->free_recv_skb_queue)) {
		printk(KERN_WARNING "free_recv_skb_queue not empty, %d\n", skb_queue_len(&precvpriv->free_recv_skb_queue));
	}

	skb_queue_purge(&precvpriv->free_recv_skb_queue);
#endif
}

int rtw_enqueue_recvbuf(struct recv_buf *precvbuf, _queue *queue)
{
	_irqL irqL;	

	_enter_critical(&queue->lock, &irqL);
	
	rtw_list_insert_tail(&precvbuf->list, get_list_head(queue));
	
	++queue->qlen;
	
	_exit_critical(&queue->lock, &irqL);

	return SUCCESS;
	
}

struct recv_buf *rtw_dequeue_recvbuf (_queue *queue)
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

#ifdef CONFIG_RTL_88E_SUPPORT
void update_recvframe_attrib_88e(union recv_frame *precvframe, struct recv_stat *prxstat)
{
	struct rx_pkt_attrib	*pattrib;
	struct recv_stat	report;
	//PRXREPORT		prxreport;

	report.rxdw0 = le32_to_cpu(prxstat->rxdw0);
	report.rxdw1 = le32_to_cpu(prxstat->rxdw1);
	report.rxdw2 = le32_to_cpu(prxstat->rxdw2);
	report.rxdw3 = le32_to_cpu(prxstat->rxdw3);
	report.rxdw4 = le32_to_cpu(prxstat->rxdw4);
	report.rxdw5 = le32_to_cpu(prxstat->rxdw5);

	//prereport = (PRXREPORT)&report;

	pattrib = &precvframe->u.hdr.attrib;
	//_rtw_memset(pattrib, 0, sizeof(struct rx_pkt_attrib));
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

		pattrib->amsdu = (u8)((report.rxdw1 >> 13) & 0x1);//(u8)prxreport->amsdu;

		pattrib->seq_num = (u16)(report.rxdw2 & 0x00000fff);//(u16)prxreport->seq;
		pattrib->frag_num = (u8)((report.rxdw2 >> 12) & 0xf);//(u8)prxreport->frag;
		pattrib->mfrag = (u8)((report.rxdw1 >> 27) & 0x1);//(u8)prxreport->mf;
		pattrib->mdata = (u8)((report.rxdw1 >> 26) & 0x1);//(u8)prxreport->md;

		pattrib->mcs_rate = (u8)(report.rxdw3 & 0x3f);//(u8)prxreport->rxmcs;
		pattrib->rxht = (u8)((report.rxdw3 >> 6) & 0x1);//(u8)prxreport->rxht;

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

#ifdef CONFIG_SUPPORT_USB_INT
#ifdef CONFIG_INTERRUPT_BASED_TXBCN
static void rtl8188eu_bcnProc(struct rtl8192cd_priv *priv, unsigned int bcnInt,
	unsigned int bcnOk, unsigned int bcnErr, unsigned int status, unsigned int status_ext)
{
#ifdef MBSSID
	int i;
	struct rtl8192cd_priv *target_priv = NULL;
#endif
#ifdef UNIVERSAL_REPEATER
	struct rtl8192cd_priv *priv_root=NULL;
#endif

	/* ================================================================
			Process Beacon OK/ERROR interrupt
		================================================================ */
	if ( bcnOk || bcnErr)
	{
#ifdef UNIVERSAL_REPEATER
		if ((OPMODE & WIFI_STATION_STATE) && GET_VXD_PRIV(priv) &&
					(GET_VXD_PRIV(priv)->drv_state & DRV_STATE_VXD_AP_STARTED)) {
			priv_root = priv;
			priv = GET_VXD_PRIV(priv);
		}
#endif

		//
		// Statistics and LED counting
		//
		if (bcnOk) {
			// for SW LED
			if (priv->pshare->LED_cnt_mgn_pkt)
				priv->pshare->LED_tx_cnt++;
#ifdef MBSSID
			if (priv->pshare->bcnDOk_priv)
				priv->pshare->bcnDOk_priv->ext_stats.beacon_ok++;
#else
			priv->ext_stats.beacon_ok++;
#endif
			SNMP_MIB_INC(dot11TransmittedFragmentCount, 1);

			// disable high queue limitation
			if ((OPMODE & WIFI_AP_STATE) && (priv->pshare->bcnDOk_priv)) {
				if (*((unsigned char *)priv->pshare->bcnDOk_priv->beaconbuf + priv->pshare->bcnDOk_priv->timoffset + 4) & 0x01)  {
					RTL_W16(RD_CTRL, RTL_R16(RD_CTRL) | HIQ_NO_LMT_EN);
				}
			}
		} else if (bcnErr) {
#ifdef MBSSID
			if (priv->pshare->bcnDOk_priv)
				priv->pshare->bcnDOk_priv->ext_stats.beacon_er++;
#else
			priv->ext_stats.beacon_er++;
#endif
		}

#ifdef UNIVERSAL_REPEATER
		if (priv_root != NULL)
			priv = priv_root;
#endif
	}
	
	/* ================================================================
			Process Beacon interrupt
	    ================================================================ */
	//
	// Update beacon content
	//
	if (bcnInt) {
		unsigned char val8;
		if (status & HIMR_88E_BcnInt) {
#ifdef UNIVERSAL_REPEATER
			if ((OPMODE & WIFI_STATION_STATE) && GET_VXD_PRIV(priv) &&
					(GET_VXD_PRIV(priv)->drv_state & DRV_STATE_VXD_AP_STARTED)) {
				if (GET_VXD_PRIV(priv)->timoffset) {
					update_beacon(GET_VXD_PRIV(priv));
				}
			} else
#endif
			{
				if (priv->timoffset) {
					update_beacon(priv);
				}
			}
		}
#ifdef MBSSID
		else {
			if (priv->pmib->miscEntry.vap_enable) {
				for (i = 0; i < priv->pshare->nr_vap_bcn; ++i) {
					if (status_ext & (HIMRE_88E_BCNDMAINT1 << i)) {
						target_priv = priv->pshare->bcn_priv[i+1];
						if (target_priv->timoffset) {
							update_beacon(target_priv);
						}
					}
				}
			}
		}
#endif

		//
		// Polling highQ as there is multicast waiting for tx...
		//
#ifdef UNIVERSAL_REPEATER
		if ((OPMODE & WIFI_STATION_STATE) && GET_VXD_PRIV(priv) &&
			(GET_VXD_PRIV(priv)->drv_state & DRV_STATE_VXD_AP_STARTED)) {
			priv_root = priv;
			priv = GET_VXD_PRIV(priv);
		}
#endif

		if ((OPMODE & WIFI_AP_STATE)) {
			if (status & HIMR_88E_BcnInt) {
				val8 = *((unsigned char *)priv->beaconbuf + priv->timoffset + 4);
				if (val8 & 0x01) {
					if(RTL_R8(BCN_CTRL) & DIS_ATIM)
						RTL_W8(BCN_CTRL, (RTL_R8(BCN_CTRL) & (~DIS_ATIM)));
					process_mcast_dzqueue(priv);
					priv->pkt_in_dtimQ = 0;
				} else {
					if(!(RTL_R8(BCN_CTRL) & DIS_ATIM))
						RTL_W8(BCN_CTRL, (RTL_R8(BCN_CTRL) | DIS_ATIM));				
				}	
//#ifdef MBSSID
				priv->pshare->bcnDOk_priv = priv;
//#endif
			}
#ifdef MBSSID
			else if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
				for (i = 0; i < priv->pshare->nr_vap_bcn; ++i) {
					if (status_ext & (HIMRE_88E_BCNDMAINT1 << i)) {
						target_priv = priv->pshare->bcn_priv[i+1];
						val8 = *((unsigned char *)target_priv->beaconbuf + target_priv->timoffset + 4);
						if (val8 & 0x01) {
							if(RTL_R8(BCN_CTRL) & DIS_ATIM)
								RTL_W8(BCN_CTRL, (RTL_R8(BCN_CTRL) & (~DIS_ATIM)));
							process_mcast_dzqueue(target_priv);
							target_priv->pkt_in_dtimQ = 0;
						} else {
							if(!(RTL_R8(BCN_CTRL) & DIS_ATIM))
								RTL_W8(BCN_CTRL, (RTL_R8(BCN_CTRL) | DIS_ATIM));
						}

						priv->pshare->bcnDOk_priv = target_priv;
					}
				}
			}
#endif
		}

#ifdef UNIVERSAL_REPEATER
		if (priv_root != NULL)
			priv = priv_root;
#endif
	}

	
#ifdef CLIENT_MODE
	//
	// Ad-hoc beacon status
	//
	if (OPMODE & WIFI_ADHOC_STATE) {
		if (bcnOk)
			priv->ibss_tx_beacon = TRUE;
		if (bcnErr)
			priv->ibss_tx_beacon = FALSE;
	}
#endif
}
#endif //CONFIG_INTERRUPT_BASED_TXBCN

void interrupt_handler_8188eu(struct rtl8192cd_priv *priv, u16 pkt_len, u8 *pbuf)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

	if (pkt_len != INTERRUPT_MSG_FORMAT_LEN)
	{
		printk("[%s] Invalid interrupt content length (%d)!\n", __FUNCTION__, pkt_len);
		return ;
	}

	// HISR
	memcpy(&(pHalData->IntArray[0]), &(pbuf[USB_INTR_CONTENT_HISR_OFFSET]), 4);
	memcpy(&(pHalData->IntArray[1]), &(pbuf[USB_INTR_CONTENT_HISRE_OFFSET]), 4);

	#if 0 //DBG
	{
		u32 hisr=0 ,hisr_ex=0;
		_rtw_memcpy(&hisr,&(pHalData->IntArray[0]),4);
		hisr = le32_to_cpu(hisr);

		_rtw_memcpy(&hisr_ex,&(pHalData->IntArray[1]),4);
		hisr_ex = le32_to_cpu(hisr_ex);

		if((hisr != 0) || (hisr_ex!=0))
			DBG_871X("===> %s hisr:0x%08x ,hisr_ex:0x%08x \n",__FUNCTION__,hisr,hisr_ex);
	}
	#endif


#ifdef CONFIG_INTERRUPT_BASED_TXBCN
	{
		unsigned int caseBcnInt, caseBcnStatusOK, caseBcnStatusER, caseBcnDmaOK;
		
		if ((pHalData->IntArray[0] & HIMR_88E_BcnInt) ||
				(pHalData->IntArray[1] & (HIMRE_88E_BCNDMAINT1 | HIMRE_88E_BCNDMAINT2
				| HIMRE_88E_BCNDMAINT3 | HIMRE_88E_BCNDMAINT4 | HIMRE_88E_BCNDMAINT5
				| HIMRE_88E_BCNDMAINT6 | HIMRE_88E_BCNDMAINT7))) {
			caseBcnInt = 1;
		}
		
		if ((pHalData->IntArray[0] & HIMR_88E_BDOK) ||
				(pHalData->IntArray[1] & (HIMRE_88E_BCNDOK1 | HIMRE_88E_BCNDOK2
				| HIMRE_88E_BCNDOK3 | HIMRE_88E_BCNDOK4 | HIMRE_88E_BCNDOK5
				| HIMRE_88E_BCNDOK6 | HIMRE_88E_BCNDOK7))) {
			caseBcnDmaOK = 1;
		}

		if (pHalData->IntArray[0] & HIMR_88E_TBDOK)
			caseBcnStatusOK = 1;

		if (pHalData->IntArray[0] & HIMR_88E_TBDER)
			caseBcnStatusER = 1;

		if (caseBcnInt || caseBcnStatusOK || caseBcnStatusER || caseBcnDmaOK) {
	                rtl8188eu_bcnProc(priv, caseBcnInt, caseBcnStatusOK, caseBcnStatusER,
				pHalData->IntArray[0], pHalData->IntArray[1]);
		}
	}
#endif //CONFIG_INTERRUPT_BASED_TXBCN
	
#ifdef DBG_CONFIG_ERROR_DETECT_INT
	if(  pHalData->IntArray[1]  & HIMRE_88E_TXERR )
		printk("===> %s Tx Error Flag Interrupt Status \n",__FUNCTION__);
	if(  pHalData->IntArray[1]  & HIMRE_88E_RXERR )
		printk("===> %s Rx Error Flag INT Status \n",__FUNCTION__);
	if(  pHalData->IntArray[1]  & HIMRE_88E_TXFOVW )
		printk("===> %s Transmit FIFO Overflow \n",__FUNCTION__);
	if(  pHalData->IntArray[1]  & HIMRE_88E_RXFOVW )
		printk("===> %s Receive FIFO Overflow \n",__FUNCTION__);
#endif//DBG_CONFIG_ERROR_DETECT_INT

	// C2H Event
	if (pbuf[0] != 0){
		memcpy(&(pHalData->C2hArray[0]), &(pbuf[USB_INTR_CONTENT_C2H_OFFSET]), 16);
		//rtw_c2h_wk_cmd(padapter); to do..
	}

}
#endif // CONFIG_SUPPORT_USB_INT
#endif // CONFIG_RTL_88E_SUPPORT

#ifdef CONFIG_RTL_92C_SUPPORT
void rtl8192c_query_rx_desc_status(union recv_frame *precvframe, struct recv_stat *pdesc)
{
	struct rx_pkt_attrib	*pattrib = &precvframe->u.hdr.attrib;

	//Offset 0
	pattrib->physt = (u8)((le32_to_cpu(pdesc->rxdw0) >> 26) & 0x1);
	pattrib->pkt_len =  (u16)(le32_to_cpu(pdesc->rxdw0)&0x00003fff);
	pattrib->drvinfo_sz = (u8)((le32_to_cpu(pdesc->rxdw0) >> 16) & 0xf) * 8;//uint 2^3 = 8 bytes

	pattrib->shift_sz = (u8)((le32_to_cpu(pdesc->rxdw0) >> 24) & 0x3);

	pattrib->crc_err = (u8)((le32_to_cpu(pdesc->rxdw0) >> 14) & 0x1);
	pattrib->icv_err = (u8)((le32_to_cpu(pdesc->rxdw0) >> 15) & 0x1);
	pattrib->qos = (u8)(( le32_to_cpu( pdesc->rxdw0 ) >> 23) & 0x1);// Qos data, wireless lan header length is 26
	pattrib->bdecrypted = (le32_to_cpu(pdesc->rxdw0) & BIT(27))? 0:1;

	//Offset 4
	pattrib->mfrag = (u8)((le32_to_cpu(pdesc->rxdw1) >> 27) & 0x1);//more fragment bit

	//Offset 8
	pattrib->frag_num = (u8)((le32_to_cpu(pdesc->rxdw2) >> 12) & 0xf);//fragmentation number

	//Offset 12
#ifdef CONFIG_TCP_CSUM_OFFLOAD_RX
	if ( le32_to_cpu(pdesc->rxdw3) & BIT(13)){
		pattrib->tcpchk_valid = 1; // valid
		if ( le32_to_cpu(pdesc->rxdw3) & BIT(11) ) {
			pattrib->tcp_chkrpt = 1; // correct
			//DBG_8192C("tcp csum ok\n");
		}
		else
			pattrib->tcp_chkrpt = 0; // incorrect

		if ( le32_to_cpu(pdesc->rxdw3) & BIT(12) )
			pattrib->ip_chkrpt = 1; // correct
		else
			pattrib->ip_chkrpt = 0; // incorrect
	}
	else {
		pattrib->tcpchk_valid = 0; // invalid
	}
#endif

	pattrib->mcs_rate=(u8)((le32_to_cpu(pdesc->rxdw3))&0x3f);
	pattrib->rxht=(u8)((le32_to_cpu(pdesc->rxdw3) >>6)&0x1);
	pattrib->sgi=(u8)((le32_to_cpu(pdesc->rxdw3) >>8)&0x1);
	//Offset 16
	//Offset 20

}
#endif // CONFIG_RTL_92C_SUPPORT

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18))
static void usb_read_interrupt_complete(struct urb *purb)
#else
static void usb_read_interrupt_complete(struct urb *purb, struct pt_regs *regs)
#endif
{
	int err;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)purb->context;
	
	if (priv->pshare->bSurpriseRemoved || priv->pshare->bDriverStopped || priv->pshare->bReadPortCancel)
	{
		printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d) OR bReadPortCancel(%d)\n", 
			__FUNCTION__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved, priv->pshare->bReadPortCancel);
		return;
	}
	
	if (purb->status==0)//SUCCESS
	{
		if (purb->actual_length > INTERRUPT_MSG_FORMAT_LEN)
		{
			printk("[%s] purb->actual_length(%d) > INTERRUPT_MSG_FORMAT_LEN\n",
				__FUNCTION__, purb->actual_length);
		}

		err = usb_submit_urb(purb, GFP_ATOMIC);
		if((err) && (err != (-EPERM)))
		{
			printk("cannot submit interrupt in URB (err=%d), urb_status=%d\n", err, purb->status);
		}
	}
	else
	{
		printk("[%s]: purb->status(%d) != 0 \n", __FUNCTION__, purb->status);

		switch (purb->status) {
		case -EINVAL:
		case -EPIPE:
		case -ENODEV:
		case -ESHUTDOWN:
			priv->pshare->bDriverStopped = TRUE;
			printk("[%s] bDriverStopped=TRUE\n", __FUNCTION__);
			break;
		case -ENOENT:
			priv->pshare->bSurpriseRemoved = TRUE;
			printk("[%s] bSurpriseRemoved=TRUE\n", __FUNCTION__);
			break;
		case -EPROTO:
			break;
		case -EINPROGRESS:
			printk("ERROR: URB IS IN PROGRESS!/n");
			break;
		default:
			break;
		}
	}

}

static u32 usb_read_interrupt(struct rtl8192cd_priv *priv, u32 addr)
{
	int err;
	unsigned int pipe;
	u32	ret = SUCCESS;
	struct recv_priv	*precvpriv = &priv->recvpriv;
	struct usb_device	*pusbd = priv->pshare->pusbdev;

_func_enter_;

	//translate DMA FIFO addr to pipehandle
	pipe = ffaddr2pipehdl(priv, addr);

	usb_fill_int_urb(precvpriv->int_in_urb, pusbd, pipe,
					precvpriv->int_in_buf,
            				INTERRUPT_MSG_FORMAT_LEN,
            				usb_read_interrupt_complete,
            				priv,
            				1);

	err = usb_submit_urb(precvpriv->int_in_urb, GFP_ATOMIC);
	if ((err) && (err != (-EPERM)))
	{
		printk("cannot submit interrupt in URB (err=%d), urb_status=%d\n",err, precvpriv->int_in_urb->status);
		ret = FAIL;
	}

_func_exit_;

	return ret;
}
#endif // CONFIG_USB_INTERRUPT_IN_PIPE


#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
static int recvbuf2recvframe(struct rtl8192cd_priv *priv, struct recv_buf *precvbuf)
{
	u8	*pbuf;
	u8	shift_sz = 0;
	u16	pkt_cnt;
	u32	pkt_offset, skb_len, alloc_sz;
	s32	transfer_len;
	struct recv_stat	*prxstat;
	struct phy_stat	*pphy_info = NULL;
	_pkt				*pkt_copy = NULL;
	union recv_frame	*precvframe = NULL;
	struct rx_pkt_attrib	*pattrib = NULL;
	HAL_INTF_DATA_TYPE		*pHalData = GET_HAL_INTF_DATA(priv);
	struct recv_priv	*precvpriv = &priv->recvpriv;
	_queue			*pfree_recv_queue = &precvpriv->free_recv_queue;

	transfer_len = (s32)precvbuf->transfer_len;
	pbuf = precvbuf->pbuf;

	prxstat = (struct recv_stat *)pbuf;	
	pkt_cnt = (le32_to_cpu(prxstat->rxdw2)>>16) & 0xff;
	
	do{
		prxstat = (struct recv_stat *)pbuf;
		//printk("recvbuf2recvframe: rxdesc=offsset 0:0x%08x, 4:0x%08x, 8:0x%08x, C:0x%08x\n",
		//		prxstat->rxdw0, prxstat->rxdw1, prxstat->rxdw2, prxstat->rxdw3);

		precvframe = rtw_alloc_recvframe(pfree_recv_queue);
		if(precvframe==NULL)
		{
			printk("%s()-%d: rtw_alloc_recvframe() failed! RX Drop!\n", __FUNCTION__, __LINE__);
			goto _exit_recvbuf2recvframe;
		}

		precvframe->u.hdr.precvbuf = NULL;	//can't access the precvbuf for new arch.
		precvframe->u.hdr.len = 0;

#ifdef CONFIG_RTL_92C_SUPPORT
		rtl8192c_query_rx_desc_status(precvframe, prxstat);
#elif defined(CONFIG_RTL_88E_SUPPORT)
		update_recvframe_attrib_88e(precvframe, prxstat);
#endif
		pattrib = &precvframe->u.hdr.attrib;

		if(pattrib->physt
#ifdef CONFIG_RTL_88E_SUPPORT
		  && (pattrib->pkt_rpt_type == NORMAL_RX)
#endif
		  )
		{
			pphy_info = (struct phy_stat *)(pbuf + RXDESC_OFFSET);
		}

		pkt_offset = RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz + pattrib->pkt_len;

		if((pattrib->pkt_len<=0) || (pkt_offset>transfer_len))
		{	
			printk("recvbuf2recvframe: pkt_len<=0\n");
			printk("%s()-%d: RX Warning!\n", __FUNCTION__, __LINE__);
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
		if((pattrib->mfrag == 1)&&(pattrib->frag_num == 0)){
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
		if(pkt_copy)
		{
			pkt_copy->dev = priv->dev;
			precvframe->u.hdr.pkt = pkt_copy;
			precvframe->u.hdr.rx_end = pkt_copy->data + alloc_sz;
			skb_reserve( pkt_copy, sizeof(struct rx_frinfo));
			skb_reserve( pkt_copy, 8 - ((SIZE_PTR)( pkt_copy->data ) & 7 ));//force pkt_copy->data at 8-byte alignment address
			skb_reserve( pkt_copy, shift_sz );//force ip_hdr at 8-byte alignment address according to shift_sz.
			memcpy(pkt_copy->data, (pbuf + RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz), skb_len);
			precvframe->u.hdr.rx_head = precvframe->u.hdr.rx_data = precvframe->u.hdr.rx_tail = pkt_copy->data;
		}
		else
		{
			printk("recvbuf2recvframe:can not allocate memory for skb copy\n");
			//precvframe->u.hdr.pkt = skb_clone(pskb, GFP_ATOMIC);
			//precvframe->u.hdr.rx_head = precvframe->u.hdr.rx_data = precvframe->u.hdr.rx_tail = pbuf;
			//precvframe->u.hdr.rx_end = pbuf + (pkt_offset>1612?pkt_offset:1612);

			precvframe->u.hdr.pkt = NULL;
			rtw_free_recvframe(priv, precvframe, pfree_recv_queue);

			goto _exit_recvbuf2recvframe;
		}
		
#ifdef ENABLE_RTL_SKB_STATS
		rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif

		recvframe_put(precvframe, skb_len);
		//recvframe_pull(precvframe, drvinfo_sz + RXDESC_SIZE);	

//t		rtl8192c_translate_rx_signal_stuff(precvframe, pphy_info);

#ifdef CONFIG_USB_RX_AGGREGATION
		switch(pHalData->UsbRxAggMode)
		{
			case USB_RX_AGG_DMA:
			case USB_RX_AGG_MIX:
				pkt_offset = (u16)_RND128(pkt_offset);
				break;
			case USB_RX_AGG_USB:
				pkt_offset = (u16)_RND4(pkt_offset);
				break;
			case USB_RX_AGG_DISABLE:
			default:
				break;
		}
#endif

#ifdef CONFIG_RTL_92C_SUPPORT
		if(rtw_recv_entry(priv, precvframe, prxstat, pphy_info) != SUCCESS)
		{
			//printk("recvbuf2recvframe: rtw_recv_entry(precvframe) != SUCCESS\n");
		}
#elif defined(CONFIG_RTL_88E_SUPPORT)
		if(pattrib->pkt_rpt_type == NORMAL_RX)//Normal rx packet
		{
			if(rtw_recv_entry(priv, precvframe, prxstat, pphy_info) != SUCCESS)
			{
				//printk("recvbuf2recvframe: rtw_recv_entry(precvframe) != SUCCESS\n");
			}
		}
		else { // pkt_rpt_type == TX_REPORT1-CCX, TX_REPORT2-TX RTP, HIS_REPORT-USB HISR RTP

			//enqueue recvframe to txrtp queue
			if(unlikely(pattrib->pkt_rpt_type == TX_REPORT1)){
				printk("rx CCX\n");
			}
			else if(pattrib->pkt_rpt_type == TX_REPORT2){
//				printk("rx TX RPT\n");
#ifdef TXREPORT
#ifdef RATEADAPTIVE_BY_ODM
				ODM_RA_TxRPT2Handle_8188E(
							ODMPTR,
							precvframe->u.hdr.rx_data,
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
			else if(pattrib->pkt_rpt_type == HIS_REPORT)
			{
//				printk("[%s %d] rx USB HISR \n", __FUNCTION__, __LINE__);
				#ifdef CONFIG_SUPPORT_USB_INT
				interrupt_handler_8188eu(priv, pattrib->pkt_len, precvframe->u.hdr.rx_data);
				#endif
			}

			rtw_free_recvframe(priv, precvframe, pfree_recv_queue);
		}
#endif

		pkt_cnt--;
		transfer_len -= pkt_offset;
		pbuf += pkt_offset;	
		precvframe = NULL;
		pkt_copy = NULL;

		if(transfer_len>0 && pkt_cnt==0)
			pkt_cnt = (le32_to_cpu(prxstat->rxdw2)>>16) & 0xff;

	}while((transfer_len>0) && (pkt_cnt>0));

_exit_recvbuf2recvframe:

	return SUCCESS;
}

void rtw_flush_recvbuf_pending_queue(struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;
	struct recv_buf *precvbuf;
	
	while (NULL != (precvbuf = rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue))) {
	}
}

void rtl8192cu_recv_tasklet(void *p)
{	
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)p;
	struct recv_priv *precvpriv = &priv->recvpriv;
	struct recv_buf *precvbuf;

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
		
		recvbuf2recvframe(priv, precvbuf);

		usb_read_port(priv, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
	} while (1);
	
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18))
static void usb_read_port_complete(struct urb *purb)
#else
static void usb_read_port_complete(struct urb *purb, struct pt_regs *regs)
#endif
{
	struct recv_buf *precvbuf = (struct recv_buf *)purb->context;	
	struct rtl8192cd_priv *priv = precvbuf->priv;
	struct recv_priv *precvpriv = &priv->recvpriv;
	
	//printk("usb_read_port_complete!!!\n");
	
	precvpriv->rx_pending_cnt --;
	
	if(priv->pshare->bSurpriseRemoved || priv->pshare->bDriverStopped || priv->pshare->bReadPortCancel)
	{
		printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d) OR bReadPortCancel(%d)\n", 
			__FUNCTION__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved, priv->pshare->bReadPortCancel);
		goto exit;
	}

	if(purb->status==0)//SUCCESS
	{
		if ((purb->actual_length > MAX_RECVBUF_SZ) || (purb->actual_length < RXDESC_SIZE))
		{
			printk("usb_read_port_complete: (purb->actual_length > MAX_RECVBUF_SZ) || (purb->actual_length < RXDESC_SIZE)\n");

			usb_read_port(priv, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		}
		else
		{
			rtw_reset_continual_urb_error(priv);
			
			precvbuf->transfer_len = purb->actual_length;

			//rtw_enqueue_rx_transfer_buffer(precvpriv, rx_transfer_buf);			
			rtw_enqueue_recvbuf(precvbuf, &precvpriv->recv_buf_pending_queue);

			tasklet_schedule(&precvpriv->recv_tasklet);
		}
	}
	else
	{
		printk("[%s]: purb->status(%d) != 0 \n", __FUNCTION__, purb->status);

		if(rtw_inc_and_chk_continual_urb_error(priv) == TRUE ){
			priv->pshare->bSurpriseRemoved = TRUE;
		}

		switch(purb->status) {
		case -EINVAL:
		case -EPIPE:
		case -ENODEV:
		case -ESHUTDOWN:
			priv->pshare->bDriverStopped = TRUE;
			printk("[%s] bDriverStopped=TRUE\n", __FUNCTION__);
			break;
		case -ENOENT:
			priv->pshare->bSurpriseRemoved = TRUE;
			printk("[%s] bSurpriseRemoved=TRUE\n", __FUNCTION__);
			break;
		case -EPROTO:
			#ifdef DBG_CONFIG_ERROR_DETECT
			{	
//				HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
//				pHalData->srestpriv.Wifi_Error_Status = USB_READ_PORT_FAIL;			
			}
			#endif
			usb_read_port(priv, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);			
			break;
		case -EINPROGRESS:
			printk("ERROR: URB IS IN PROGRESS!/n");
			break;
		default:
			break;
		}
	}

exit:	
	
_func_exit_;
	
}

static u32 usb_read_port(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *rmem)
{
	int err;
	unsigned int pipe;
	u32 ret = SUCCESS;
	struct urb *purb = NULL;
	struct recv_buf	*precvbuf = (struct recv_buf *)rmem;
	struct recv_priv	*precvpriv = &priv->recvpriv;
	struct usb_device	*pusbd = priv->pshare->pusbdev;

_func_enter_;
	
	if(priv->pshare->bDriverStopped || priv->pshare->bSurpriseRemoved)
	{
		printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
			__FUNCTION__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved);
		return FAIL;
	}

	rtl8192cu_init_recvbuf(priv, precvbuf);

	if(precvbuf->pbuf)
	{
		precvpriv->rx_pending_cnt++;
	
		purb = precvbuf->purb;

		//translate DMA FIFO addr to pipehandle
		pipe = ffaddr2pipehdl(priv, addr);	

		usb_fill_bulk_urb(purb, pusbd, pipe, 
						precvbuf->pbuf,
						MAX_RECVBUF_SZ,
						usb_read_port_complete,
						precvbuf);//context is precvbuf

		purb->transfer_dma = precvbuf->dma_transfer_addr;
		purb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		err = usb_submit_urb(purb, GFP_ATOMIC);
		if((err) && (err != (-EPERM)))
		{
			printk("usb_read_port, status=%d\n", err);
			ret = FAIL;
		}
	}

_func_exit_;

	return ret;
}
#else	// CONFIG_USE_USB_BUFFER_ALLOC_RX
static int recvbuf2recvframe(struct rtl8192cd_priv *priv, _pkt *pskb)
{
	u8	*pbuf;
	u8	shift_sz = 0;
	u16	pkt_cnt;
	u32	pkt_offset, skb_len, alloc_sz;
	s32	transfer_len;
	struct recv_stat	*prxstat;
	struct phy_stat	*pphy_info = NULL;
	_pkt				*pkt_copy = NULL;
	union recv_frame	*precvframe = NULL;
	struct rx_pkt_attrib	*pattrib = NULL;
	HAL_INTF_DATA_TYPE		*pHalData = GET_HAL_INTF_DATA(priv);
	struct recv_priv	*precvpriv = &priv->recvpriv;
	_queue			*pfree_recv_queue = &precvpriv->free_recv_queue;
	
	transfer_len = (s32)pskb->len;
	pbuf = pskb->data;

	prxstat = (struct recv_stat *)pbuf;
	pkt_cnt = (le32_to_cpu(prxstat->rxdw2)>>16) & 0xff;
	
	do{
		prxstat = (struct recv_stat *)pbuf;
		//printk("recvbuf2recvframe: rxdesc=offsset 0:0x%08x, 4:0x%08x, 8:0x%08x, C:0x%08x\n",
		//		prxstat->rxdw0, prxstat->rxdw1, prxstat->rxdw2, prxstat->rxdw3);

		precvframe = rtw_alloc_recvframe(pfree_recv_queue);
		if(precvframe==NULL)
		{
			printk("%s()-%d: rtw_alloc_recvframe() failed! RX Drop!\n", __FUNCTION__, __LINE__);	
			goto _exit_recvbuf2recvframe;
		}

		precvframe->u.hdr.precvbuf = NULL;	//can't access the precvbuf for new arch.
		precvframe->u.hdr.len = 0;

#ifdef CONFIG_RTL_92C_SUPPORT
		rtl8192c_query_rx_desc_status(precvframe, prxstat);
#elif defined(CONFIG_RTL_88E_SUPPORT)
		update_recvframe_attrib_88e(precvframe, prxstat);
#endif
		pattrib = &precvframe->u.hdr.attrib;

		if(pattrib->physt
#ifdef CONFIG_RTL_88E_SUPPORT
		  && (pattrib->pkt_rpt_type == NORMAL_RX)
#endif
		  )
		{
			pphy_info = (struct phy_stat *)(pbuf + RXDESC_OFFSET);
		}

		pkt_offset = RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz + pattrib->pkt_len;

		if((pattrib->pkt_len<=0) || (pkt_offset>transfer_len))
		{	
			printk("recvbuf2recvframe: pkt_len<=0\n");
			printk("%s()-%d: RX Warning!\n", __FUNCTION__, __LINE__);	
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
		if((pattrib->mfrag == 1)&&(pattrib->frag_num == 0)){
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
		if(pkt_copy)
		{
			pkt_copy->dev = priv->dev;
			precvframe->u.hdr.pkt = pkt_copy;
			precvframe->u.hdr.rx_end = pkt_copy->data + alloc_sz;
			skb_reserve( pkt_copy, sizeof(struct rx_frinfo));
			skb_reserve( pkt_copy, 8 - ((SIZE_PTR)( pkt_copy->data ) & 7 ));//force pkt_copy->data at 8-byte alignment address
			skb_reserve( pkt_copy, shift_sz );//force ip_hdr at 8-byte alignment address according to shift_sz.
			memcpy(pkt_copy->data, (pbuf + RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz), skb_len);
			precvframe->u.hdr.rx_head = precvframe->u.hdr.rx_data = precvframe->u.hdr.rx_tail = pkt_copy->data;
		}
		else
		{
			if((pattrib->mfrag == 1)&&(pattrib->frag_num == 0))
			{
				printk("recvbuf2recvframe: alloc_skb fail , drop frag frame \n");
				rtw_free_recvframe(priv, precvframe, pfree_recv_queue);
				goto _exit_recvbuf2recvframe;
			}
			
			precvframe->u.hdr.pkt = skb_clone(pskb, GFP_ATOMIC);
			if(precvframe->u.hdr.pkt)
			{
				struct sk_buff *pkt_clone = precvframe->u.hdr.pkt;
				
				pkt_clone->data = pbuf + RXDESC_SIZE + pattrib->drvinfo_sz + pattrib->shift_sz;
				precvframe->u.hdr.rx_head = precvframe->u.hdr.rx_data = precvframe->u.hdr.rx_tail
					= pkt_clone->data;
				precvframe->u.hdr.rx_end = pkt_clone->data + skb_len;
			}
			else
			{
				printk("recvbuf2recvframe: skb_clone fail\n");
				rtw_free_recvframe(priv, precvframe, pfree_recv_queue);
				goto _exit_recvbuf2recvframe;
			}
		}
		
#ifdef ENABLE_RTL_SKB_STATS
		rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif

		recvframe_put(precvframe, skb_len);
		//recvframe_pull(precvframe, drvinfo_sz + RXDESC_SIZE);	

//t		rtl8192c_translate_rx_signal_stuff(precvframe, pphy_info);

#ifdef CONFIG_USB_RX_AGGREGATION
		switch(pHalData->UsbRxAggMode)
		{
			case USB_RX_AGG_DMA:
			case USB_RX_AGG_MIX:
				pkt_offset = (u16)_RND128(pkt_offset);
				break;
			case USB_RX_AGG_USB:
				pkt_offset = (u16)_RND4(pkt_offset);
				break;
			case USB_RX_AGG_DISABLE:
			default:
				break;
		}
#endif

#ifdef CONFIG_RTL_92C_SUPPORT
		if(rtw_recv_entry(priv, precvframe, prxstat, pphy_info) != SUCCESS)
		{
			//printk("recvbuf2recvframe: rtw_recv_entry(precvframe) != SUCCESS\n");
		}
#elif defined(CONFIG_RTL_88E_SUPPORT)
		if(pattrib->pkt_rpt_type == NORMAL_RX)//Normal rx packet
		{
			if(rtw_recv_entry(priv, precvframe, prxstat, pphy_info) != SUCCESS)
			{
				//printk("recvbuf2recvframe: rtw_recv_entry(precvframe) != SUCCESS\n");
			}
		}
		else { // pkt_rpt_type == TX_REPORT1-CCX, TX_REPORT2-TX RTP, HIS_REPORT-USB HISR RTP

			//enqueue recvframe to txrtp queue
			if(unlikely(pattrib->pkt_rpt_type == TX_REPORT1)){
				printk("rx CCX\n");
			}
			else if(pattrib->pkt_rpt_type == TX_REPORT2){
//				printk("rx TX RPT\n");
#ifdef TXREPORT
#ifdef RATEADAPTIVE_BY_ODM
				ODM_RA_TxRPT2Handle_8188E(
							ODMPTR,
							precvframe->u.hdr.rx_data,
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
			else if(pattrib->pkt_rpt_type == HIS_REPORT)
			{
//				printk("[%s %d] rx USB HISR \n", __FUNCTION__, __LINE__);
				#ifdef CONFIG_SUPPORT_USB_INT
				interrupt_handler_8188eu(priv, pattrib->pkt_len, precvframe->u.hdr.rx_data);
				#endif
			}

			rtw_free_recvframe(priv, precvframe, pfree_recv_queue);
		}
#endif

		pkt_cnt--;
		transfer_len -= pkt_offset;
		pbuf += pkt_offset;	
		precvframe = NULL;
		pkt_copy = NULL;

		if(transfer_len>0 && pkt_cnt==0)
			pkt_cnt = (le32_to_cpu(prxstat->rxdw2)>>16) & 0xff;

	}while((transfer_len>0) && (pkt_cnt>0));

_exit_recvbuf2recvframe:

	return SUCCESS;
}

void rtw_flush_recvbuf_pending_queue(struct rtl8192cd_priv *priv)
{
	skb_queue_purge(&priv->recvpriv.rx_skb_queue);
}

void rtl8192cu_recv_tasklet(void *p)
{
	_pkt *pskb;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)p;
	struct recv_priv *precvpriv = &priv->recvpriv;
	
	while (NULL != (pskb = skb_dequeue(&precvpriv->rx_skb_queue)))
	{
		if ((priv->pshare->bDriverStopped == TRUE)||(priv->pshare->bSurpriseRemoved== TRUE))
		{
			printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
				__FUNCTION__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved);
			dev_kfree_skb_any(pskb);
			break;
		}
		
		recvbuf2recvframe(priv, pskb);

#ifdef CONFIG_PREALLOC_RECV_SKB

		skb_reset_tail_pointer(pskb);
		
		pskb->len = 0;
		
		skb_queue_tail(&precvpriv->free_recv_skb_queue, pskb);
		
#else
		dev_kfree_skb_any(pskb);
#endif
		
	}
	
}


#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18))
static void usb_read_port_complete(struct urb *purb)
#else
static void usb_read_port_complete(struct urb *purb, struct pt_regs *regs)
#endif
{
	struct recv_buf *precvbuf = (struct recv_buf *)purb->context;	
	struct rtl8192cd_priv *priv = precvbuf->priv;
	struct recv_priv *precvpriv = &priv->recvpriv;
	
	//printk("usb_read_port_complete!!!\n");
	
	//_enter_critical(&precvpriv->lock, &irqL);
	//precvbuf->irp_pending=_FALSE;
	//precvpriv->rx_pending_cnt --;
	//_exit_critical(&precvpriv->lock, &irqL);
	
	precvpriv->rx_pending_cnt --;
	
	if(priv->pshare->bSurpriseRemoved || priv->pshare->bDriverStopped || priv->pshare->bReadPortCancel)
	{
		printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d) OR bReadPortCancel(%d)\n", 
			__FUNCTION__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved, priv->pshare->bReadPortCancel);

#ifdef CONFIG_PREALLOC_RECV_SKB
		//precvbuf->reuse = _TRUE;
#else
		if(precvbuf->pskb) {
			printk("==> free skb(%p)\n",precvbuf->pskb);
			dev_kfree_skb_any(precvbuf->pskb);
		}
#endif
		goto exit;
	}

	if(purb->status==0)//SUCCESS
	{
		if ((purb->actual_length > MAX_RECVBUF_SZ) || (purb->actual_length < RXDESC_SIZE))
		{
			//printk("usb_read_port_complete: (purb->actual_length(%d) > MAX_RECVBUF_SZ) || (purb->actual_length(%d) < RXDESC_SIZE)\n", purb->actual_length, purb->actual_length);
			usb_read_port(priv, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		}
		else
		{
			rtw_reset_continual_urb_error(priv);
			
			precvbuf->transfer_len = purb->actual_length;
			skb_put(precvbuf->pskb, purb->actual_length);	
			skb_queue_tail(&precvpriv->rx_skb_queue, precvbuf->pskb);

			if (skb_queue_len(&precvpriv->rx_skb_queue) == 1)
				tasklet_schedule(&precvpriv->recv_tasklet);

			precvbuf->pskb = NULL;
			usb_read_port(priv, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);			
		}
	}
	else
	{
		printk("[%s] purb->status(%d) != 0 \n", __FUNCTION__, purb->status);

		if(rtw_inc_and_chk_continual_urb_error(priv) == TRUE ){
			priv->pshare->bSurpriseRemoved = TRUE;
		}

		switch(purb->status) {
		case -EINVAL:
		case -EPIPE:
		case -ENODEV:
		case -ESHUTDOWN:
			priv->pshare->bDriverStopped = TRUE;
			printk("[%s] bDriverStopped=TRUE\n", __FUNCTION__);
			break;
		case -ENOENT:
			priv->pshare->bSurpriseRemoved = TRUE;
			printk("[%s] bSurpriseRemoved=TRUE\n", __FUNCTION__);
			break;
		case -EPROTO:
			#ifdef DBG_CONFIG_ERROR_DETECT
			{	
//				HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
//				pHalData->srestpriv.Wifi_Error_Status = USB_READ_PORT_FAIL;			
			}
			#endif
			usb_read_port(priv, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);			
			break;
		case -EINPROGRESS:
			printk("ERROR: URB IS IN PROGRESS!/n");
			break;
		default:
			break;
		}
	}

exit:	
	
_func_exit_;
	
}

static u32 usb_read_port(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *rmem)
{
	int err;
	unsigned int pipe;
	SIZE_PTR tmpaddr=0;
	SIZE_PTR alignment=0;
	u32 ret = SUCCESS;
	struct urb *purb = NULL;
	struct recv_buf	*precvbuf = (struct recv_buf *)rmem;
	struct recv_priv	*precvpriv = &priv->recvpriv;
	struct usb_device	*pusbd = priv->pshare->pusbdev;

_func_enter_;
	
	if(priv->pshare->bDriverStopped || priv->pshare->bSurpriseRemoved)
	{
		printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
			__FUNCTION__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved);
		return FAIL;
	}

#ifdef CONFIG_PREALLOC_RECV_SKB
	if (NULL == precvbuf->pskb)
	{
		precvbuf->pskb = skb_dequeue(&precvpriv->free_recv_skb_queue);
	}
#endif
	
	rtl8192cu_init_recvbuf(priv, precvbuf);

	//re-assign for linux based on skb
	if (NULL == precvbuf->pskb)
	{
		//precvbuf->pskb = alloc_skb(MAX_RECVBUF_SZ, GFP_ATOMIC);//don't use this after v2.6.25
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)) // http://www.mail-archive.com/netdev@vger.kernel.org/msg17214.html
		precvbuf->pskb = dev_alloc_skb(MAX_RECVBUF_SZ + RECVBUFF_ALIGN_SZ);
#else
		precvbuf->pskb = netdev_alloc_skb(priv->dev, MAX_RECVBUF_SZ + RECVBUFF_ALIGN_SZ);
#endif			
		if (NULL == precvbuf->pskb)
		{
			printk("usb_read_port: alloc_skb fail!\n");
			return FAIL;
		}

		tmpaddr = (SIZE_PTR)precvbuf->pskb->data;
		alignment = tmpaddr & (RECVBUFF_ALIGN_SZ-1);
		skb_reserve(precvbuf->pskb, (RECVBUFF_ALIGN_SZ - alignment));
	}
	
	precvbuf->phead = precvbuf->pskb->head;
	precvbuf->pdata = precvbuf->pskb->data;
	precvbuf->ptail = skb_tail_pointer(precvbuf->pskb);		
	precvbuf->pend = skb_end_pointer(precvbuf->pskb);
	
	precvbuf->pbuf = precvbuf->pskb->data;

	//_enter_critical(&precvpriv->lock, &irqL);
	//precvpriv->rx_pending_cnt++;
	//precvbuf->irp_pending = _TRUE;
	//_exit_critical(&precvpriv->lock, &irqL);

	precvpriv->rx_pending_cnt++;

	purb = precvbuf->purb;

	//translate DMA FIFO addr to pipehandle
	pipe = ffaddr2pipehdl(priv, addr);

	usb_fill_bulk_urb(purb, pusbd, pipe, 
					precvbuf->pbuf,
					MAX_RECVBUF_SZ,
					usb_read_port_complete,
					precvbuf);//context is precvbuf

	err = usb_submit_urb(purb, GFP_ATOMIC);
	if((err) && (err != (-EPERM)))
	{
		printk("usb_read_port, status=%d\n", err);
		ret = FAIL;
	}

_func_exit_;

	return ret;
}
#endif	// CONFIG_USE_USB_BUFFER_ALLOC_RX
static void usb_read_port_cancel(struct rtl8192cd_priv *priv)
{
	int i;	
	struct recv_buf *precvbuf = (struct recv_buf *)priv->recvpriv.precv_buf;	

	printk("usb_read_port_cancel \n");

	priv->pshare->bReadPortCancel = TRUE;

	for(i=0; i < NR_RECVBUFF ; i++)
	{
		if(precvbuf->purb)	
		{
			//DBG_8192C("usb_read_port_cancel : usb_kill_urb \n");
			usb_kill_urb(precvbuf->purb);
		}

		precvbuf++;
	}

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	if (priv->recvpriv.int_in_urb)
		usb_kill_urb(priv->recvpriv.int_in_urb);
#endif
}

unsigned int rtl8192cu_inirp_init(struct rtl8192cd_priv *priv)
{	
	u8 i;
	struct recv_buf *precvbuf;
	uint	status;
	struct recv_priv *precvpriv = &(priv->recvpriv);

_func_enter_;

	status = SUCCESS;

	printk("===> usb_inirp_init \n");	
		
	precvpriv->ff_hwaddr = RECV_BULK_IN_ADDR;

	//issue Rx irp to receive data
	precvbuf = (struct recv_buf *)precvpriv->precv_buf;	
	for(i=0; i<NR_RECVBUFF; i++)
	{
		if(usb_read_port(priv, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf) == FALSE )
		{
			printk("usb_rx_init: usb_read_port error \n");
			status = FAIL;
			goto exit;
		}
		
		precvbuf++;
		precvpriv->free_recv_buf_queue_cnt--;
	}

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	if(usb_read_interrupt(priv, RECV_INT_IN_ADDR) == FALSE )
	{
		printk("usb_rx_init: usb_read_interrupt error \n");
		status = FAIL;
	}
#endif

exit:
	
	printk("<=== usb_inirp_init \n");

_func_exit_;

	return status;

}

unsigned int rtl8192cu_inirp_deinit(struct rtl8192cd_priv *priv)
{	
	printk("\n ===> usb_rx_deinit \n");
	
	usb_read_port_cancel(priv);

	printk("\n <=== usb_rx_deinit \n");

	return SUCCESS;
}

int _rtw_init_recv_priv(struct rtl8192cd_priv *priv)
{
	int i;
	struct recv_priv *precvpriv = &priv->recvpriv;
	union recv_frame *precvframe;
	int res = SUCCESS;

_func_enter_;

	// We don't need to memset padapter->XXX to zero, because adapter is allocated by rtw_zvmalloc().
	//_rtw_memset((unsigned char *)precvpriv, 0, sizeof (struct  recv_priv));

	_rtw_spinlock_init(&precvpriv->lock);

	_rtw_init_queue(&precvpriv->free_recv_queue);
	_rtw_init_queue(&precvpriv->recv_pending_queue);

	precvpriv->pallocated_frame_buf = rtw_zvmalloc(NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ);
	
	if (NULL == precvpriv->pallocated_frame_buf) {
		printk("alloc recv_frame fail!(size %d)\n", (NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ));
		res= FAIL;
		goto exit;
	}

	precvpriv->precv_frame_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(precvpriv->pallocated_frame_buf), RXFRAME_ALIGN_SZ);

	precvframe = (union recv_frame*) precvpriv->precv_frame_buf;

	for(i=0; i < NR_RECVFRAME ; i++)
	{
		_rtw_init_listhead(&(precvframe->u.list));

		rtw_list_insert_tail(&(precvframe->u.list), &(precvpriv->free_recv_queue.queue));

		res = rtw_os_recv_resource_alloc(priv, precvframe);

		precvframe++;
	}

	precvpriv->free_recv_queue.qlen = NR_RECVFRAME;

	precvpriv->rx_pending_cnt=1;

	sema_init(&precvpriv->allrxreturnevt, 0);

	res = rtl8192cu_init_recv_priv(priv);

exit:

_func_exit_;

	if (FALSE == res)
		_rtw_free_recv_priv(priv);

	return res;

}

void rtw_mfree_recv_priv_lock(struct recv_priv *precvpriv)
{
	_rtw_spinlock_free(&precvpriv->lock);
#ifdef CONFIG_RECV_THREAD_MODE	
	_rtw_free_sema(&precvpriv->recv_sema);
	_rtw_free_sema(&precvpriv->terminate_recvthread_sema);
#endif

	_rtw_spinlock_free(&precvpriv->free_recv_queue.lock);
	_rtw_spinlock_free(&precvpriv->recv_pending_queue.lock);

	_rtw_spinlock_free(&precvpriv->free_recv_buf_queue.lock);

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
	_rtw_spinlock_free(&precvpriv->recv_buf_pending_queue.lock);
#endif	// CONFIG_USE_USB_BUFFER_ALLOC_RX
}

void _rtw_free_recv_priv (struct rtl8192cd_priv *priv)
{
	struct recv_priv *precvpriv = &priv->recvpriv;

_func_enter_;

	rtw_mfree_recv_priv_lock(precvpriv);

	rtw_os_recv_resource_free(precvpriv);

	if(precvpriv->pallocated_frame_buf) {
		rtw_vmfree(precvpriv->pallocated_frame_buf, NR_RECVFRAME * sizeof(union recv_frame) + RXFRAME_ALIGN_SZ);
	}

	rtl8192cu_free_recv_priv(priv);

_func_exit_;

}

