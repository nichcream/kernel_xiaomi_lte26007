/*
 *  SDIO TX handle routines
 *
 *  $Id: 8188e_sdio_xmit.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8188E_SDIO_XMIT_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/tcp.h>
#endif

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"


#ifdef CONFIG_TCP_ACK_TX_AGGREGATION
static void rtl8192cd_tcpack_timer(unsigned long task_priv);
#endif
void rtl8188es_xmit_tasklet(unsigned long data);

int rtw_os_xmit_resource_alloc(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf, u32 alloc_sz)
{
	pxmitbuf->pallocated_buf = rtw_zmalloc(alloc_sz);
	if (NULL == pxmitbuf->pallocated_buf)
	{
		return FAIL;
	}

	pxmitbuf->pkt_head = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pxmitbuf->pallocated_buf), XMITBUF_ALIGN_SZ);
	pxmitbuf->pkt_end = pxmitbuf->pallocated_buf + alloc_sz;

	return SUCCESS;	
}

void rtw_os_xmit_resource_free(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf, u32 free_sz)
{
	if (pxmitbuf->pallocated_buf)
	{
		rtw_mfree(pxmitbuf->pallocated_buf, free_sz);
		pxmitbuf->pallocated_buf = NULL;
	}
}

int _rtw_init_xmit_priv(struct rtl8192cd_priv *priv)
{
	int i;
	struct priv_shared_info *pshare = priv->pshare;
	struct xmit_frame *pxframe;
	struct xmit_buf *pxmitbuf;
	
	for (i = 0; i < MAX_HW_TX_QUEUE; ++i) {
		_rtw_init_queue(&pshare->tx_pending_sta_queue[i]);
		_rtw_init_queue(&pshare->tx_xmitbuf_waiting_queue[i]);
	}
	_init_txservq(&pshare->pspoll_sta_queue, BE_QUEUE);
	pshare->use_hw_queue_bitmap = 0;
	
	// init xmit_frame
	_rtw_init_queue(&pshare->free_xmit_queue);
	
	pshare->pallocated_frame_buf = rtw_zvmalloc(NR_XMITFRAME * sizeof(struct xmit_frame) + 4);
	if (NULL == pshare->pallocated_frame_buf) {
		printk("alloc pallocated_frame_buf fail!(size %d)\n", (NR_XMITFRAME * sizeof(struct xmit_frame) + 4));
		goto exit;
	}
	
	pshare->pxmit_frame_buf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pshare->pallocated_frame_buf), 4);

	pxframe = (struct xmit_frame*) pshare->pxmit_frame_buf;

	for (i = 0; i < NR_XMITFRAME; i++)
	{
		_rtw_init_listhead(&(pxframe->list));
		
		pxframe->txinsn.fr_type = _RESERVED_FRAME_TYPE_;
		pxframe->txinsn.pframe = NULL;
		pxframe->txinsn.phdr = NULL;
 		
		rtw_list_insert_tail(&(pxframe->list), &(pshare->free_xmit_queue.queue));
		
		pxframe++;
	}

	pshare->free_xmit_queue.qlen = NR_XMITFRAME;
	
	// init xmit_buf
	_rtw_init_queue(&pshare->free_xmitbuf_queue);
	
	pshare->pallocated_xmitbuf = rtw_zvmalloc(NR_XMITBUFF * sizeof(struct xmit_buf) + 4);
	if (NULL == pshare->pallocated_xmitbuf) {
		printk("alloc pallocated_xmitbuf fail!(size %d)\n", (NR_XMITBUFF * sizeof(struct xmit_buf) + 4));
		goto exit;
	}
	
	pshare->pxmitbuf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pshare->pallocated_xmitbuf), 4);
	
	pxmitbuf = (struct xmit_buf*)pshare->pxmitbuf;
	
	for (i = 0; i < NR_XMITBUFF; i++)
	{
		_rtw_init_listhead(&pxmitbuf->list);
		_rtw_init_listhead(&pxmitbuf->tx_xmitbuf_list);
		
		pxmitbuf->ext_tag = FALSE;
		
		if(rtw_os_xmit_resource_alloc(priv, pxmitbuf, (MAX_XMITBUF_SZ + XMITBUF_ALIGN_SZ)) == FAIL) {
			printk("alloc xmit_buf resource fail!(size %d)\n", (MAX_XMITBUF_SZ + XMITBUF_ALIGN_SZ));
			goto exit;
		}
		
		rtw_list_insert_tail(&pxmitbuf->list, &(pshare->free_xmitbuf_queue.queue));
		
		pxmitbuf++;
	}
	
	pshare->free_xmitbuf_queue.qlen = NR_XMITBUFF;
	
	// init xmit extension buff
	_rtw_init_queue(&pshare->free_xmit_extbuf_queue);

	pshare->pallocated_xmit_extbuf = rtw_zvmalloc(NR_XMIT_EXTBUFF * sizeof(struct xmit_buf) + 4);
	if (NULL == pshare->pallocated_xmit_extbuf) {
		printk("alloc pallocated_xmit_extbuf fail!(size %d)\n", (NR_XMIT_EXTBUFF * sizeof(struct xmit_buf) + 4));
		goto exit;
	}

	pshare->pxmit_extbuf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pshare->pallocated_xmit_extbuf), 4);

	pxmitbuf = (struct xmit_buf*)pshare->pxmit_extbuf;

	for (i = 0; i < NR_XMIT_EXTBUFF; i++)
	{
		_rtw_init_listhead(&pxmitbuf->list);
		_rtw_init_listhead(&pxmitbuf->tx_xmitbuf_list);
		
		pxmitbuf->ext_tag = TRUE;
		
		if(rtw_os_xmit_resource_alloc(priv, pxmitbuf, MAX_XMIT_EXTBUF_SZ + XMITBUF_ALIGN_SZ) == FAIL) {
			printk("alloc xmit_extbuf resource fail!(size %d)\n", (MAX_XMIT_EXTBUF_SZ + XMITBUF_ALIGN_SZ));
			goto exit;
		}

		rtw_list_insert_tail(&pxmitbuf->list, &(pshare->free_xmit_extbuf_queue.queue));
		
		pxmitbuf++;
	}

	pshare->free_xmit_extbuf_queue.qlen = NR_XMIT_EXTBUFF;
	
	// init xmit_buf for beacon
	_rtw_init_queue(&pshare->free_bcn_xmitbuf_queue);

	pshare->pallocated_bcn_xmitbuf = rtw_zvmalloc(NR_BCN_XMITBUFF * sizeof(struct xmit_buf) + 4);
	if (NULL == pshare->pallocated_bcn_xmitbuf) {
		printk("alloc pallocated_bcn_xmitbuf fail!(size %d)\n", (NR_BCN_XMITBUFF * sizeof(struct xmit_buf) + 4));
		goto exit;
	}

	pshare->pbcn_xmitbuf = (u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pshare->pallocated_bcn_xmitbuf), 4);

	pxmitbuf = (struct xmit_buf*)pshare->pbcn_xmitbuf;

	for (i = 0; i < NR_BCN_XMITBUFF; ++i)
	{
		_rtw_init_listhead(&pxmitbuf->list);
		_rtw_init_listhead(&pxmitbuf->tx_xmitbuf_list);
		
		pxmitbuf->ext_tag = TRUE;
		
		if(rtw_os_xmit_resource_alloc(priv, pxmitbuf, MAX_BCN_XMITBUF_SZ + XMITBUF_ALIGN_SZ) == FAIL) {
			printk("alloc bcn_xmitbuf resource fail!(size %d)\n", (MAX_BCN_XMITBUF_SZ + XMITBUF_ALIGN_SZ));
			goto exit;
		}

		rtw_list_insert_tail(&pxmitbuf->list, &(pshare->free_bcn_xmitbuf_queue.queue));
		
		++pxmitbuf;
	}

	pshare->free_bcn_xmitbuf_queue.qlen = NR_BCN_XMITBUFF;
	
#ifdef CONFIG_TCP_ACK_TX_AGGREGATION
	// init tcp ack related materials
	_rtw_init_queue(&pshare->tcpack_queue);
	
	init_timer(&pshare->tcpack_timer);
	pshare->tcpack_timer.data = (unsigned long)priv;
	pshare->tcpack_timer.function = rtl8192cd_tcpack_timer;
#endif
	
	// init xmit tasklet
	tasklet_init(&pshare->xmit_tasklet, rtl8188es_xmit_tasklet, (unsigned long)priv);

	for (i = 0; i < MAX_HW_TX_QUEUE; ++i)
		pshare->ts_used[i] = 0;

	// init xmit thread
#ifdef CONFIG_SDIO_TX_MULTI_QUEUE
	for (i = 0; i < ARRAY_SIZE(pshare->tx_pending_queue); ++i)
		_rtw_init_queue(&pshare->tx_pending_queue[i]);
#else
	_rtw_init_queue(&pshare->pending_xmitbuf_queue);
#endif
	init_waitqueue_head(&pshare->xmit_waitqueue);
	pshare->xmit_thread = NULL;
	init_completion(&pshare->xmit_thread_done);

	__clear_bit(WAKE_EVENT_XMIT, &pshare->wake_event);

	return SUCCESS;
	
exit:
	_rtw_free_xmit_priv(priv);
	
	return FAIL;
}

void _rtw_free_xmit_priv(struct rtl8192cd_priv *priv)
{
	int i;
	struct priv_shared_info *pshare = priv->pshare;
	struct xmit_buf *pxmitbuf;

	for (i = 0; i < MAX_HW_TX_QUEUE; ++i) {
		_rtw_spinlock_free(&pshare->tx_pending_sta_queue[i].lock);
		_rtw_spinlock_free(&pshare->tx_xmitbuf_waiting_queue[i].lock);
	}

#ifdef CONFIG_SDIO_TX_MULTI_QUEUE
	for (i = 0; i < ARRAY_SIZE(pshare->tx_pending_queue); ++i)
		_rtw_spinlock_free(&pshare->tx_pending_queue[i].lock);
#else
	_rtw_spinlock_free(&pshare->pending_xmitbuf_queue.lock);
#endif

	// free xmit_frame
	_rtw_spinlock_free(&pshare->free_xmit_queue.lock);

	if(pshare->pallocated_frame_buf)
	{
		rtw_vmfree(pshare->pallocated_frame_buf, NR_XMITFRAME * sizeof(struct xmit_frame) + 4);
		pshare->pallocated_frame_buf = NULL;
	}
	
	// free xmit_buf
	_rtw_spinlock_free(&pshare->free_xmitbuf_queue.lock);
	
	if(pshare->pallocated_xmitbuf)
	{
		pxmitbuf = (struct xmit_buf *)pshare->pxmitbuf;
		for (i=0; i<NR_XMITBUFF; ++i) {
			rtw_os_xmit_resource_free(priv, pxmitbuf,(MAX_XMITBUF_SZ + XMITBUF_ALIGN_SZ));
			++pxmitbuf;
		}

		rtw_vmfree(pshare->pallocated_xmitbuf, NR_XMITBUFF * sizeof(struct xmit_buf) + 4);
		pshare->pallocated_xmitbuf = NULL;
	}

	// free xmit extension buff
	_rtw_spinlock_free(&pshare->free_xmit_extbuf_queue.lock);

	if(pshare->pallocated_xmit_extbuf)
	{
		pxmitbuf = (struct xmit_buf *)pshare->pxmit_extbuf;
		for(i=0; i<NR_XMIT_EXTBUFF; ++i) {
			rtw_os_xmit_resource_free(priv, pxmitbuf,(MAX_XMIT_EXTBUF_SZ + XMITBUF_ALIGN_SZ));
			++pxmitbuf;
		}
		
		rtw_vmfree(pshare->pallocated_xmit_extbuf, NR_XMIT_EXTBUFF * sizeof(struct xmit_buf) + 4);
		pshare->pallocated_xmit_extbuf = NULL;
	}
	
	// free xmit_buf for beacon
	_rtw_spinlock_free(&pshare->free_bcn_xmitbuf_queue.lock);
	
	if(pshare->pallocated_bcn_xmitbuf)
	{
		pxmitbuf = (struct xmit_buf *)pshare->pbcn_xmitbuf;
		for(i=0; i<NR_BCN_XMITBUFF; ++i) {
			rtw_os_xmit_resource_free(priv, pxmitbuf,(MAX_BCN_XMITBUF_SZ + XMITBUF_ALIGN_SZ));
			++pxmitbuf;
		}

		rtw_vmfree(pshare->pallocated_bcn_xmitbuf, NR_BCN_XMITBUFF * sizeof(struct xmit_buf) + 4);
		pshare->pallocated_bcn_xmitbuf = NULL;
	}
}

static inline void rtw_init_xmitbuf(struct xmit_buf *pxmitbuf, u8 q_num)
{
	pxmitbuf->pkt_tail = pxmitbuf->pkt_data = pxmitbuf->pkt_head;
	pxmitbuf->pkt_offset = DEFAULT_TXPKT_OFFSET;
	
	pxmitbuf->q_num = q_num;
	pxmitbuf->agg_num = 0;
	pxmitbuf->use_hw_queue = 0;
	pxmitbuf->flags = 0;
}

struct xmit_buf *rtw_alloc_xmitbuf_ext(struct rtl8192cd_priv *priv, u8 q_num)
{
	_irqL irqL;
	struct xmit_buf *pxmitbuf = NULL;
	_list *plist, *phead;
	_queue *pfree_queue = &priv->pshare->free_xmit_extbuf_queue;
	
	plist =  NULL;
	
	phead = get_list_head(pfree_queue);
	
	_enter_critical(&pfree_queue->lock, &irqL);
	
	if(rtw_is_list_empty(phead) == FALSE) {
		
		plist = get_next(phead);
		
		rtw_list_delete(plist);
		
		--pfree_queue->qlen;
	}
	
	_exit_critical(&pfree_queue->lock, &irqL);
	
	if (NULL !=  plist) {
		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
		rtw_init_xmitbuf(pxmitbuf, q_num);
	}
	
	return pxmitbuf;
}

s32 rtw_free_xmitbuf_ext(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf)
{
	_irqL irqL;
	_queue *pfree_queue;
	
	if (unlikely(NULL == pxmitbuf))
	{
		return FAIL;
	}
	
	pfree_queue = &priv->pshare->free_xmit_extbuf_queue;
	
	_enter_critical(&pfree_queue->lock, &irqL);
	
	rtw_list_insert_tail(&(pxmitbuf->list), get_list_head(pfree_queue));
	
	++pfree_queue->qlen;

	_exit_critical(&pfree_queue->lock, &irqL);
	
	return SUCCESS;
} 

struct xmit_buf *rtw_alloc_xmitbuf(struct rtl8192cd_priv *priv, u8 q_num)
{
	_irqL irqL;
	struct xmit_buf *pxmitbuf = NULL;
	_list *plist, *phead;
	_queue *pfree_queue = &priv->pshare->free_xmitbuf_queue;
	
	plist = NULL;
	
	phead = get_list_head(pfree_queue);
	
	_enter_critical(&pfree_queue->lock, &irqL);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		
		plist = get_next(phead);
		
		rtw_list_delete(plist);
		
		--pfree_queue->qlen;
	}
	
	_exit_critical(&pfree_queue->lock, &irqL);
	
	if (NULL !=  plist) {
		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
		rtw_init_xmitbuf(pxmitbuf, q_num);
	}
	
	return pxmitbuf;
}

s32 rtw_free_xmitbuf(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf)
{
	_irqL irqL;
	_queue *pfree_queue;
	
	if (unlikely(NULL == pxmitbuf))
	{
		return FAIL;
	}
	
	BUG_ON(pxmitbuf->use_hw_queue);

	if (pxmitbuf->ext_tag)
	{
		rtw_free_xmitbuf_ext(priv, pxmitbuf);
	}
	else
	{
		pfree_queue = &priv->pshare->free_xmitbuf_queue;
		
		_enter_critical(&pfree_queue->lock, &irqL);

		rtw_list_insert_tail(&(pxmitbuf->list), get_list_head(pfree_queue));

		++pfree_queue->qlen;
		
		_exit_critical(&pfree_queue->lock, &irqL);
	}

	return SUCCESS;	
} 

struct xmit_buf *rtw_alloc_beacon_xmitbuf(struct rtl8192cd_priv *priv)
{
	_irqL irqL;
	struct xmit_buf *pxmitbuf = NULL;
	_list *plist, *phead;
	_queue *pfree_queue = &priv->pshare->free_bcn_xmitbuf_queue;
	
	plist =  NULL;
	
	phead = get_list_head(pfree_queue);
	
	_enter_critical(&pfree_queue->lock, &irqL);
	
	if(rtw_is_list_empty(phead) == FALSE) {
		
		plist = get_next(phead);
		
		rtw_list_delete(plist);
		
		--pfree_queue->qlen;
	}
	
	_exit_critical(&pfree_queue->lock, &irqL);
	
	if (NULL !=  plist) {
		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
		rtw_init_xmitbuf(pxmitbuf, BEACON_QUEUE);
	}
	
	return pxmitbuf;
}

s32 rtw_free_beacon_xmitbuf(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf)
{	
	_irqL irqL;
	_queue *pfree_queue;
	
	if (unlikely(NULL == pxmitbuf))
	{
		return FAIL;
	}
	
	pfree_queue = &priv->pshare->free_bcn_xmitbuf_queue;

	_enter_critical(&pfree_queue->lock, &irqL);
	
	rtw_list_insert_tail(&(pxmitbuf->list), get_list_head(pfree_queue));
	
	++pfree_queue->qlen;

	_exit_critical(&pfree_queue->lock, &irqL);
	
	return SUCCESS;
}

void rtw_free_txinsn_resource(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	if ((NULL != txcfg->pframe) || (NULL != txcfg->phdr)) {
		if (_SKB_FRAME_TYPE_ == txcfg->fr_type) {
			rtl_kfree_skb(priv, (struct sk_buff *)txcfg->pframe, _SKB_TX_);
			if (NULL != txcfg->phdr)
				release_wlanllchdr_to_poll(priv, txcfg->phdr);
		}
		else if (_PRE_ALLOCMEM_ == txcfg->fr_type) {
			release_mgtbuf_to_poll(priv, txcfg->pframe);
			release_wlanhdr_to_poll(priv, txcfg->phdr);
		}
		else if (NULL != txcfg->phdr) {
			release_wlanhdr_to_poll(priv, txcfg->phdr);
		}
		
		txcfg->fr_type = _RESERVED_FRAME_TYPE_;
		txcfg->pframe = NULL;
		txcfg->phdr = NULL;
	}
}

struct xmit_frame *rtw_alloc_xmitframe(struct rtl8192cd_priv *priv)
{
	/*
		Please remember to use all the osdep_service api,
		and lock/unlock or _enter/_exit critical to protect 
		pfree_xmit_queue
	*/

	_irqL irqL;
	struct xmit_frame *pxframe = NULL;
	_list *plist, *phead;
	_queue *pfree_queue = &priv->pshare->free_xmit_queue;
	
	plist = NULL;
	
	phead = get_list_head(pfree_queue);
	
	_enter_critical_bh(&pfree_queue->lock, &irqL);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		plist = get_next(phead);
		rtw_list_delete(plist);
		
		--pfree_queue->qlen;
		if (STOP_NETIF_TX_QUEUE_THRESH == pfree_queue->qlen) {
#ifdef CONFIG_NETDEV_MULTI_TX_QUEUE
			if (BIT(_NETDEV_TX_QUEUE_ALL)-1 != priv->pshare->stop_netif_tx_queue) {
				priv->pshare->stop_netif_tx_queue = BIT(_NETDEV_TX_QUEUE_ALL)-1;
				rtl8192cd_tx_stopQueue(priv);
			}
#else
			if (0 == priv->pshare->stop_netif_tx_queue) {
				priv->pshare->stop_netif_tx_queue = 1;
				rtl8192cd_tx_stopQueue(priv);
			}
#endif // CONFIG_NETDEV_MULTI_TX_QUEUE
		}
	}
	
	_exit_critical_bh(&pfree_queue->lock, &irqL);
	
	if (likely(NULL != plist)) {
		pxframe = LIST_CONTAINOR(plist, struct xmit_frame, list);
		pxframe->priv = NULL;
		
		pxframe->txinsn.fr_type = _RESERVED_FRAME_TYPE_;
		pxframe->txinsn.pframe = NULL;
		pxframe->txinsn.phdr = NULL;
	} else {
		++priv->pshare->nr_out_of_xmitframe;
	}

	return pxframe;
}

s32 rtw_free_xmitframe(struct rtl8192cd_priv *priv, struct xmit_frame *pxmitframe)
{	
	_irqL irqL;
	_queue *pfree_queue;
#ifdef CONFIG_NETDEV_MULTI_TX_QUEUE
	int i;
#endif
	
	if (unlikely(NULL == pxmitframe)) {
		goto exit;
	}
	
	pfree_queue = &priv->pshare->free_xmit_queue;
	
	_enter_critical_bh(&pfree_queue->lock, &irqL);
	
	rtw_list_insert_tail(&pxmitframe->list, get_list_head(pfree_queue));
	
	++pfree_queue->qlen;
#ifdef CONFIG_NETDEV_MULTI_TX_QUEUE
	if (priv->pshare->iot_mode_enable) {
		// If no consider this case, original flow will cause driver almost frequently restart and stop queue 0(VO),
		// and rarely restart other queue (especially for BE queue) during massive traffic loading.
		// Obviously, we will see serious ping timeout happen no matter ping packet size.
		// [Conclusion] If not in WMM process, we must restart all queues when reaching upper threshold.
		if ((WAKE_NETIF_TX_QUEUE_THRESH <= pfree_queue->qlen)
				&& (priv->pshare->stop_netif_tx_queue)) {
			priv->pshare->stop_netif_tx_queue = 0;
			rtl8192cd_tx_restartQueue(priv, _NETDEV_TX_QUEUE_ALL);
		}
	} else {
		for (i = 0; i < _NETDEV_TX_QUEUE_ALL; ++i) {
			if (WAKE_NETIF_TX_QUEUE_THRESH*(i+1) <= pfree_queue->qlen) {
				if (priv->pshare->stop_netif_tx_queue & BIT(i)) {
					priv->pshare->stop_netif_tx_queue &= ~ BIT(i);
					rtl8192cd_tx_restartQueue(priv, i);
				}
			} else
				break;
		}
	}
#else
	if ((WAKE_NETIF_TX_QUEUE_THRESH == pfree_queue->qlen)
			&& (priv->pshare->stop_netif_tx_queue)) {
		priv->pshare->stop_netif_tx_queue = 0;
		rtl8192cd_tx_restartQueue(priv);
	}
#endif // CONFIG_NETDEV_MULTI_TX_QUEUE
	
	_exit_critical_bh(&pfree_queue->lock, &irqL);
	
exit:
	
	return SUCCESS;
}

void rtw_free_xmitframe_queue(struct rtl8192cd_priv *priv, _queue *pframequeue)
{
	_irqL irqL;
	_list	*plist, *phead, xmit_list;
	struct xmit_frame *pxmitframe;
	
	phead = &xmit_list;
	
clean_queue:
	
	_rtw_init_listhead(phead);
	
	_enter_critical_bh(&(pframequeue->lock), &irqL);
	
	rtw_list_splice(get_list_head(pframequeue), phead);
	
	pframequeue->qlen = 0;
	
	_exit_critical_bh(&(pframequeue->lock), &irqL);
	
	plist = get_next(phead);
	
	while (rtw_end_of_queue_search(phead, plist) == FALSE)
	{
		pxmitframe = LIST_CONTAINOR(plist, struct xmit_frame, list);
		
		plist = get_next(plist);
		
		rtw_free_txinsn_resource(pxmitframe->priv, &pxmitframe->txinsn);
		
		rtw_free_xmitframe(priv, pxmitframe);
	}
	
	if (rtw_is_list_empty(&pframequeue->queue) == FALSE)
		goto clean_queue;
}

void rtw_txservq_flush(struct rtl8192cd_priv *priv, struct tx_servq *ptxservq)
{
	_irqL irqL;
	_queue *sta_queue;
	
	if (_rtw_queue_empty(&ptxservq->xframe_queue) == TRUE)
		return;
	
	sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num];
	
	_enter_critical_bh(&sta_queue->lock, &irqL);
	
	if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
		rtw_list_delete(&ptxservq->tx_pending);
		--sta_queue->qlen;
	}
	
	_exit_critical_bh(&sta_queue->lock, &irqL);
	
	rtw_free_xmitframe_queue(priv, &ptxservq->xframe_queue);
}

struct xmit_frame* rtw_txservq_dequeue(struct rtl8192cd_priv *priv, struct tx_servq *ptxservq)
{
	_queue *xframe_queue, *sta_queue;
	_list *phead, *plist;
	_irqL irqL;
	
	struct xmit_frame *pxmitframe = NULL;
	
	xframe_queue = &ptxservq->xframe_queue;
	
	phead = get_list_head(xframe_queue);
	plist = NULL;
	
	_enter_critical_bh(&xframe_queue->lock, &irqL);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		plist = get_next(phead);
		rtw_list_delete(plist);
		--xframe_queue->qlen;
	}
	
	if (0 == xframe_queue->qlen) {
		sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num];
		
		_rtw_spinlock(&sta_queue->lock);
		
		if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
			rtw_list_delete(&ptxservq->tx_pending);
			--sta_queue->qlen;
			ptxservq->ts_used = 0;
		}
		
		if (MCAST_QNUM == ptxservq->q_num)
			ptxservq->q_num = BE_QUEUE;
		
		_rtw_spinunlock(&sta_queue->lock);
	}
	
	_exit_critical_bh(&xframe_queue->lock, &irqL);
	
	if (plist) {
		if (ptxservq == &priv->pshare->pspoll_sta_queue) {
			struct stat_info *pstat;
			pstat = LIST_CONTAINOR(plist, struct stat_info, pspoll_list);
			pxmitframe = rtw_txservq_dequeue(priv, &pstat->tx_queue[BE_QUEUE]);

			if (NULL != pxmitframe)
				pxmitframe->txinsn.is_pspoll = 1;
		} else {
			pxmitframe = LIST_CONTAINOR(plist, struct xmit_frame, list);
		}
	}
		
	return pxmitframe;
}

void rtw_pspoll_sta_enqueue(struct rtl8192cd_priv *priv, struct stat_info *pstat, int insert_tail)
{
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;

	ptxservq = &priv->pshare->pspoll_sta_queue;

	xframe_queue = &ptxservq->xframe_queue;

	_enter_critical_bh(&xframe_queue->lock, &irqL);

	if (TRUE == rtw_is_list_empty(&pstat->pspoll_list)) {
		if (ENQUEUE_TO_TAIL == insert_tail)
			rtw_list_insert_tail(&pstat->pspoll_list, &xframe_queue->queue);
		else
			rtw_list_insert_head(&pstat->pspoll_list, &xframe_queue->queue);
		++xframe_queue->qlen;

		if (1 == xframe_queue->qlen)
		{
			sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num]; // polling packets use BE_QUEUE
				
			_rtw_spinlock(&sta_queue->lock);
				
			if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
				rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
				++sta_queue->qlen;
			}
				
			_rtw_spinunlock(&sta_queue->lock);
		}
	}
		
	_exit_critical_bh(&xframe_queue->lock, &irqL);
}

void rtw_pspoll_sta_delete(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;

	ptxservq = &priv->pshare->pspoll_sta_queue;
	
	xframe_queue = &ptxservq->xframe_queue;
	
	_enter_critical_bh(&xframe_queue->lock, &irqL);
	
	if (FALSE == rtw_is_list_empty(&pstat->pspoll_list)) {
		rtw_list_delete(&pstat->pspoll_list);
		--xframe_queue->qlen;
	}

	if (0 == xframe_queue->qlen) {
		sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num]; // polling packets use BE_QUEUE

		_rtw_spinlock(&sta_queue->lock);
		
		if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
			rtw_list_delete(&ptxservq->tx_pending);
			--sta_queue->qlen;
		}

		_rtw_spinunlock(&sta_queue->lock);
	}

	_exit_critical_bh(&xframe_queue->lock, &irqL);
}

// Before enqueue xmitframe, the fr_type, pframe, pstat, q_num field in txinsn must be available 
int rtw_enqueue_xmitframe(struct rtl8192cd_priv *priv, struct xmit_frame *pxmitframe, int insert_tail)
{
	struct tx_insn *txcfg;
	struct stat_info *pstat;
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;
	
	txcfg = &pxmitframe->txinsn;
	pstat = txcfg->pstat;
	
	if (pstat)
	{
		ptxservq = &pstat->tx_queue[txcfg->q_num];
		
		xframe_queue = &ptxservq->xframe_queue;
		
		_enter_critical_bh(&xframe_queue->lock, &irqL);
		
		if (insert_tail)
			rtw_list_insert_tail(&pxmitframe->list, &xframe_queue->queue);
		else
			rtw_list_insert_head(&pxmitframe->list, &xframe_queue->queue);
		
		++xframe_queue->qlen;
		
		if (1 == xframe_queue->qlen)
		{
			sta_queue = &priv->pshare->tx_pending_sta_queue[txcfg->q_num];
			
			_rtw_spinlock(&sta_queue->lock);
			
			if ((rtw_is_list_empty(&ptxservq->tx_pending) == TRUE)
					&& !(pstat->state & WIFI_SLEEP_STATE)) {
				rtw_list_insert_tail(&ptxservq->tx_pending, &sta_queue->queue);
				++sta_queue->qlen;
			}
			
			_rtw_spinunlock(&sta_queue->lock);
		}
		
		_exit_critical_bh(&xframe_queue->lock, &irqL);
	}
	else if (MGNT_QUEUE == txcfg->q_num)	// class 1 frame
	{
		ptxservq = &priv->tx_mgnt_queue;
		
		xframe_queue = &ptxservq->xframe_queue;
		
		_enter_critical_bh(&xframe_queue->lock, &irqL);
		
		if (insert_tail)
			rtw_list_insert_tail(&pxmitframe->list, &xframe_queue->queue);
		else
			rtw_list_insert_head(&pxmitframe->list, &xframe_queue->queue);
		
		++xframe_queue->qlen;
		
		if (1 == xframe_queue->qlen)
		{
			sta_queue = &priv->pshare->tx_pending_sta_queue[MGNT_QUEUE];
			
			_rtw_spinlock(&sta_queue->lock);
			
			if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
				rtw_list_insert_tail(&ptxservq->tx_pending, &sta_queue->queue);
				++sta_queue->qlen;
			}
			
			_rtw_spinunlock(&sta_queue->lock);
		}
		
		_exit_critical_bh(&xframe_queue->lock, &irqL);
	}
	else	// enqueue MC/BC
	{
		ptxservq = &priv->tx_mc_queue;
		
		xframe_queue = &ptxservq->xframe_queue;
		
		_enter_critical_bh(&xframe_queue->lock, &irqL);
		
		if (insert_tail)
			rtw_list_insert_tail(&pxmitframe->list, &xframe_queue->queue);
		else
			rtw_list_insert_head(&pxmitframe->list, &xframe_queue->queue);
		
		++xframe_queue->qlen;
		
		if (1 == xframe_queue->qlen)
		{
			sta_queue = &priv->pshare->tx_pending_sta_queue[BE_QUEUE];
			
			_rtw_spinlock(&sta_queue->lock);
			
			if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
				if (list_empty(&priv->sleep_list)) {
					ptxservq->q_num = BE_QUEUE;
					rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
					++sta_queue->qlen;
				} else {
					ptxservq->q_num = MCAST_QNUM;
				}
			}
			
			priv->release_mcast = 0;
			
			_rtw_spinunlock(&sta_queue->lock);
		}
		
		_exit_critical_bh(&xframe_queue->lock, &irqL);
	}
	
	return TRUE;
}

struct xmit_frame* rtw_dequeue_xmitframe(struct rtl8192cd_priv *priv, int q_num)
{
	struct tx_servq *ptxservq;
	_queue *sta_queue;
	_list *phead, *plist;
	_irqL irqL;
	
	struct xmit_frame *pxmitframe;
	
	sta_queue = &priv->pshare->tx_pending_sta_queue[q_num];
	
	phead = get_list_head(sta_queue);
	
	do {
		ptxservq = NULL;
		
		_enter_critical_bh(&sta_queue->lock, &irqL);
		
#ifdef CONFIG_SDIO_TX_AGGREGATION
		if (rtw_is_list_empty(phead) == FALSE) {
			plist = get_next(phead);
			ptxservq = LIST_CONTAINOR(plist, struct tx_servq, tx_pending);
		}
#else // !CONFIG_SDIO_TX_AGGREGATION
		while (rtw_is_list_empty(phead) == FALSE) {
			plist = get_next(phead);
			ptxservq = LIST_CONTAINOR(plist, struct tx_servq, tx_pending);
			
			if ((ptxservq == &priv->tx_mc_queue) || (ptxservq == &priv->pshare->pspoll_sta_queue))
				break;
			
			// check remaining timeslice
			if (ptxservq->ts_used < STA_TS_LIMIT)
				break;
			
			rtw_list_delete(plist);
			rtw_list_insert_tail(plist, phead);
			ptxservq->ts_used= 0;
			
			ptxservq = NULL;
		}
#endif // CONFIG_SDIO_TX_AGGREGATION
		
		_exit_critical_bh(&sta_queue->lock, &irqL);
		
		if (NULL == ptxservq)
			return NULL;
		
		pxmitframe = rtw_txservq_dequeue(priv, ptxservq);
		
	} while (NULL == pxmitframe);
	
	return pxmitframe;
}

#ifdef CONFIG_TCP_ACK_TX_AGGREGATION
#ifdef CONFIG_TCP_ACK_MERGE
int rtw_merge_tcpack(struct rtl8192cd_priv *priv, struct list_head *tcpack_list)
{
	_list *phead, *plist;
	
	struct xmit_frame *pxmitframe;
	struct xmit_frame *pxmitframe2;
	struct tx_insn *txcfg;
	struct sk_buff *skb1, *skb2;
	struct iphdr *iph1, *iph2;
	struct tcphdr *tcph1, *tcph2;
	int num;
	
	phead = tcpack_list;
	plist = phead->prev;
	
	if (plist == phead)
		return 0;
	
	num = 0;
	do {
		pxmitframe = LIST_CONTAINOR(plist, struct xmit_frame, list);
		skb1 = (struct sk_buff *) pxmitframe->txinsn.pframe;
		iph1 = (struct iphdr *)(skb1->data + ETH_HLEN);
		tcph1 = (struct tcphdr *)((u8*)iph1 + iph1->ihl*4);
		
		plist = plist->prev;
		
		while (plist != phead) {
			pxmitframe2 = LIST_CONTAINOR(plist, struct xmit_frame, list);
			plist = plist->prev;
			
			txcfg = &pxmitframe2->txinsn;
			skb2 = (struct sk_buff *) txcfg->pframe;
			iph2 = (struct iphdr *)(skb2->data + ETH_HLEN);
			tcph2 = (struct tcphdr *)((u8*)iph2 + iph2->ihl*4);
			
			if ((iph1->saddr == iph2->saddr) && (iph1->daddr == iph2->daddr) 
					&& (tcph1->source == tcph2->source) && (tcph1->dest == tcph2->dest)
					&& (tcph1->ack_seq != tcph2->ack_seq)) {
				rtw_list_delete(&pxmitframe2->list);
				rtw_free_txinsn_resource(pxmitframe2->priv, txcfg);
				rtw_free_xmitframe(priv, pxmitframe2);
			}
		}
		
		num++;
		plist = pxmitframe->list.prev;
	} while (plist != phead);
	
	return num;
}
#endif // CONFIG_TCP_ACK_MERGE

void rtw_migrate_tcpack(struct rtl8192cd_priv *priv, struct tcpack_servq *tcpackq)
{
	int q_num;
	int nr_tcpack;
	struct stat_info *pstat;
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;
	
	struct list_head tcpack_list;
	
	INIT_LIST_HEAD(&tcpack_list);
	
	// move all xframes on tcpackq  to temporary list "tcpack_list"
	xframe_queue = &tcpackq->xframe_queue;
	
	_enter_critical_bh(&xframe_queue->lock, &irqL);
	
	list_splice_init(&xframe_queue->queue, &tcpack_list);
	nr_tcpack = xframe_queue->qlen;
	xframe_queue->qlen = 0;
	
	_exit_critical_bh(&xframe_queue->lock, &irqL);
	
#ifdef CONFIG_TCP_ACK_MERGE
	if (priv->pshare->rf_ft_var.tcpack_merge && (nr_tcpack > 1)) {
		nr_tcpack = rtw_merge_tcpack(priv, &tcpack_list);
	}
#endif
	
	// next, move all xframes on "tcpack_list" to pstat->tx_queue
	q_num = tcpackq->q_num;
	pstat = (struct stat_info *)((char *)tcpackq - FIELD_OFFSET(struct stat_info, tcpack_queue)
		- q_num* sizeof(struct tcpack_servq));
	
	ptxservq = &pstat->tx_queue[q_num];
	
	xframe_queue = &ptxservq->xframe_queue;
	
	_enter_critical_bh(&xframe_queue->lock, &irqL);
	
	list_splice_tail(&tcpack_list, &xframe_queue->queue);
	
	xframe_queue->qlen += nr_tcpack;
		
	if (xframe_queue->qlen == nr_tcpack)
	{
		sta_queue = &priv->pshare->tx_pending_sta_queue[q_num];
		
		_rtw_spinlock(&sta_queue->lock);
		
		if ((rtw_is_list_empty(&ptxservq->tx_pending) == TRUE)
				&& !(pstat->state & WIFI_SLEEP_STATE)) {
			rtw_list_insert_tail(&ptxservq->tx_pending, &sta_queue->queue);
			++sta_queue->qlen;
			
			tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
		}
		
		_rtw_spinunlock(&sta_queue->lock);
	}
	
	_exit_critical_bh(&xframe_queue->lock, &irqL);
}

int rtw_enqueue_tcpack_xmitframe(struct rtl8192cd_priv *priv, struct xmit_frame *pxmitframe)
{
	struct tx_insn *txcfg;
	struct stat_info *pstat;
	struct tcpack_servq *tcpackq;
	_queue *xframe_queue, *tcpack_queue;
	_irqL irqL;
	int qlen;
	
	txcfg = &pxmitframe->txinsn;
	pstat = txcfg->pstat;
	
	tcpackq = &pstat->tcpack_queue[txcfg->q_num];
	xframe_queue = &tcpackq->xframe_queue;
	
	_enter_critical_bh(&xframe_queue->lock, &irqL);
	
	rtw_list_insert_tail(&pxmitframe->list, &xframe_queue->queue);
	++xframe_queue->qlen;
	qlen = xframe_queue->qlen;
	
	_exit_critical_bh(&xframe_queue->lock, &irqL);
	
	if (1 == qlen) {
		tcpackq->start_time = jiffies;
		tcpack_queue = &priv->pshare->tcpack_queue;
		
		_enter_critical_bh(&tcpack_queue->lock, &irqL);
		
		rtw_list_insert_tail(&tcpackq->tx_pending, &tcpack_queue->queue);
		++tcpack_queue->qlen;
		
		_exit_critical_bh(&tcpack_queue->lock, &irqL);
	} else if (MAX_TCP_ACK_AGG == qlen) {
		
		tcpack_queue = &priv->pshare->tcpack_queue;
		
		_enter_critical_bh(&tcpack_queue->lock, &irqL);
		
		if (rtw_is_list_empty(&tcpackq->tx_pending) == FALSE) {
			rtw_list_delete(&tcpackq->tx_pending);
			--tcpack_queue->qlen;
		}
		
		_exit_critical_bh(&tcpack_queue->lock, &irqL);
		
		rtw_migrate_tcpack(priv, tcpackq);
	}
	
	return TRUE;
}

int rtw_xmit_enqueue_tcpack(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	struct xmit_frame *pxmitframe;

	if (NULL == (pxmitframe = rtw_alloc_xmitframe(priv))) {
		DEBUG_WARN("No more xmitframe\n");
		return FALSE;
	}
	
	memcpy(&pxmitframe->txinsn, txcfg, sizeof(struct tx_insn));
	pxmitframe->priv = priv;
	
	if (rtw_enqueue_tcpack_xmitframe(priv, pxmitframe) == FALSE) {
		DEBUG_WARN("Fail to enqueue xmitframe\n");
		rtw_free_xmitframe(priv, pxmitframe);
		return FALSE;
	}
	
	return TRUE;
}

void rtw_tcpack_servq_flush(struct rtl8192cd_priv *priv, struct tcpack_servq *tcpackq)
{
	_irqL irqL;
	_queue *tcpack_queue;
	
	if (_rtw_queue_empty(&tcpackq->xframe_queue) == TRUE)
		return;
	
	tcpack_queue = &priv->pshare->tcpack_queue;
	
	_enter_critical_bh(&tcpack_queue->lock, &irqL);
	
	if (rtw_is_list_empty(&tcpackq->tx_pending) == FALSE) {
		rtw_list_delete(&tcpackq->tx_pending);
		--tcpack_queue->qlen;
	}
	
	_exit_critical_bh(&tcpack_queue->lock, &irqL);
	
	rtw_free_xmitframe_queue(priv, &tcpackq->xframe_queue);
}

static void rtl8192cd_tcpack_timer(unsigned long task_priv)
{
	_list *phead, *plist;
	
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct priv_shared_info *pshare = priv->pshare;
	
	struct tcpack_servq *tcpackq;
	_queue *tcpack_queue;
	_irqL irqL;
	
	if ((pshare->bDriverStopped) || (pshare->bSurpriseRemoved))
		return;
	
	tcpack_queue = &priv->pshare->tcpack_queue;
	phead = get_list_head(tcpack_queue);
	
	if (rtw_is_list_empty(phead))
		goto out;
	
	do {
		tcpackq = NULL;
		
		_enter_critical_bh(&tcpack_queue->lock, &irqL);
		
		if (rtw_is_list_empty(phead) == FALSE) {
			plist = get_next(phead);
			tcpackq = LIST_CONTAINOR(plist, struct tcpack_servq, tx_pending);
			if (time_after_eq(jiffies, tcpackq->start_time+TCP_ACK_TIMEOUT)) {
				rtw_list_delete(&tcpackq->tx_pending);
				--tcpack_queue->qlen;
			} else {
				// don't process tcpackq before TCP_ACK_TIMEOUT
				tcpackq = NULL;
			}
		}
		
		_exit_critical_bh(&tcpack_queue->lock, &irqL);
		
		if (tcpackq) {
			rtw_migrate_tcpack(priv, tcpackq);
		}
	} while (tcpackq);
out:
	mod_timer(&pshare->tcpack_timer, jiffies+1);
}
#endif // CONFIG_TCP_ACK_TX_AGGREGATION

int rtw_send_xmitframe(struct xmit_frame *pxmitframe)
{
	struct rtl8192cd_priv *priv;
	struct tx_insn* txcfg;
	
	priv = pxmitframe->priv;
	txcfg = &pxmitframe->txinsn;
	
	if (MCAST_QNUM == txcfg->q_num) {
		if (priv->release_mcast && (tx_servq_len(&priv->tx_mc_queue) == 0))
			priv->release_mcast = 0;
	}
	
	switch (txcfg->next_txpath) {
	case TXPATH_HARD_START_XMIT:
		__rtl8192cd_usb_start_xmit(priv, txcfg);
		break;
		
	case TXPATH_SLOW_PATH:
		{
			struct net_device *wdsDev = NULL;
#ifdef WDS
			if (txcfg->wdsIdx >= 0) {
				wdsDev = priv->wds_dev[txcfg->wdsIdx];
			}
#endif
			
			rtl8192cd_tx_slowPath(priv, (struct sk_buff*)txcfg->pframe, txcfg->pstat,
					priv->dev, wdsDev, &pxmitframe->txinsn);
		}
		break;
		
	case TXPATH_FIRETX:
		if (__rtl8192cd_firetx(priv, txcfg) == CONGESTED) {
			rtw_free_txinsn_resource(priv, txcfg);
		}
		rtw_handle_xmit_fail(priv, txcfg);
		break;
	}
	
	rtw_free_xmitframe(priv, pxmitframe);

	return 0;
}

void rtl8188es_xmit_tasklet(unsigned long data)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv*)data;
	struct priv_shared_info *pshare = priv->pshare;
	struct xmit_frame *pxmitframe;
	struct xmit_buf *pxmitbuf;
	
	const u8 q_priority[] = {HIGH_QUEUE, MGNT_QUEUE, VO_QUEUE, VI_QUEUE, BE_QUEUE, BK_QUEUE};
	const int ts_limit[]  = {     65536,          0,    65536,    49152,    32768,    16384}; // unit: microsecond
	int i, q_num=-1;
	
	struct tx_insn* txcfg;

	while(1)
	{
		if ((pshare->bDriverStopped == TRUE)||(pshare->bSurpriseRemoved== TRUE))
		{
			printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
				__FUNCTION__, pshare->bDriverStopped, pshare->bSurpriseRemoved);
			return;
		}
		
		pxmitframe  = NULL;
		
		for (i = 0; i < ARRAY_SIZE(q_priority); ++i) {
			q_num = q_priority[i];
			
			if (MGNT_QUEUE == q_num) {
				if (0 == pshare->free_xmit_extbuf_queue.qlen)
					continue;
			} else {
				if (0 == pshare->free_xmitbuf_queue.qlen)
					continue;

				// check whether timeslice quota goes beyond the limit
				if (pshare->ts_used[q_num] > ts_limit[i])
					continue;
			}

			if (test_and_set_bit(q_num, &pshare->use_hw_queue_bitmap) == 0) {
				if (NULL != (pxmitframe = rtw_dequeue_xmitframe(priv, q_num)))
					break;
				
				clear_bit(q_num, &pshare->use_hw_queue_bitmap);
			}
		}
		
		if (NULL == pxmitframe) {
			unsigned char reset_flag = 0;
			
			for (i = 0; i < MAX_HW_TX_QUEUE; ++i) {
				if (0 != pshare->ts_used[i]) {
					reset_flag = 1;
					pshare->ts_used[i] = 0;
				}
			}

			if (reset_flag)
				continue;
			else
				break;
		}
		
		txcfg = &pxmitframe->txinsn;
		
		// re-assign q_num to avoid txcfg->q_num is not equal tx_servq.q_num for tx_mc_queue.
		// Because q_num of tx_mc_queue will switch to MCAST_QNUM once any STA sleeps.
		// This action is redundent for other queues.
		txcfg->q_num = q_num;
		
		if (_SKB_FRAME_TYPE_ == txcfg->fr_type) {
			pxmitbuf = get_usable_pending_xmitbuf(priv, txcfg);
			if (NULL == pxmitbuf)
				pxmitbuf = rtw_alloc_xmitbuf(priv, (u8)q_num);
		} else {
			pxmitbuf = rtw_alloc_xmitbuf_ext(priv, (u8)q_num);
		}
		
		if (NULL == pxmitbuf) {
			if (txcfg->is_pspoll) {
				txcfg->is_pspoll = 0;
				rtw_pspoll_sta_enqueue(priv, txcfg->pstat, ENQUEUE_TO_HEAD);
			}
			if (rtw_enqueue_xmitframe(pxmitframe->priv, pxmitframe, ENQUEUE_TO_HEAD) == FALSE) {
				rtw_free_txinsn_resource(pxmitframe->priv, txcfg);
				rtw_free_xmitframe(priv, pxmitframe);
			}
			// Release the ownership of the HW TX queue
			clear_bit(q_num, &pshare->use_hw_queue_bitmap);
			continue;
		}
		
		pxmitbuf->agg_start_with = txcfg;
		pxmitbuf->use_hw_queue = 1;
		txcfg->pxmitbuf = pxmitbuf;
		
		rtw_send_xmitframe(pxmitframe);
	}
}

int rtw_txinsn_require_bufsize(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	int sw_encrypt = UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE));
	
	return txcfg->llc + txcfg->fr_len + ((_TKIP_PRIVACY_== txcfg->privacy)? 8 : 0) +
		((txcfg->hdr_len + txcfg->iv + (sw_encrypt ? (txcfg->icv + txcfg->mic) : 0)
		+ TXDESC_SIZE + TXAGG_DESC_ALIGN_SZ)*txcfg->frg_num) - TXAGG_DESC_ALIGN_SZ;
}

void rtw_xmitbuf_aggregate(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf, struct stat_info *pstat, int q_num)
{
	struct tx_servq *ptxservq;
	struct xmit_frame *pxmitframe;
	struct tx_insn *txcfg;
	
	_queue *sta_queue;
	_list *phead, *plist;
	_irqL irqL;
	
	BUG_ON(0 == pxmitbuf->use_hw_queue);
	
	if (pstat) {
		ptxservq = &pstat->tx_queue[q_num];
	} else if (MGNT_QUEUE == q_num) {
		ptxservq = &priv->tx_mgnt_queue;
	} else {
		ptxservq = &priv->tx_mc_queue;
	}
	
	sta_queue = &priv->pshare->tx_pending_sta_queue[q_num];
	phead = get_list_head(sta_queue);
	
	while (1) {
		
		if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
			// re-select an another valid tx servq
			pxmitframe = NULL;
		} else {
			// check remaining timeslice
			if ((ptxservq == &priv->tx_mc_queue) || (ptxservq == &priv->pshare->pspoll_sta_queue)
					|| (ptxservq->ts_used < STA_TS_LIMIT)) {
				pxmitframe = rtw_txservq_dequeue(priv, ptxservq);
			} else {
				_enter_critical_bh(&sta_queue->lock, &irqL);

				if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
					rtw_list_delete(&ptxservq->tx_pending);
					rtw_list_insert_tail(&ptxservq->tx_pending, phead);
				}
				ptxservq->ts_used = 0;

				_exit_critical_bh(&sta_queue->lock, &irqL);
				
				pxmitframe = NULL;
			}
		}
		
		while (NULL == pxmitframe) {
			
			plist = NULL;
			
			_enter_critical_bh(&sta_queue->lock, &irqL);
			
			if (rtw_is_list_empty(phead) == FALSE) {
				plist = get_next(phead);
			}
			
			_exit_critical_bh(&sta_queue->lock, &irqL);
			
			if (NULL == plist) return;
			
			ptxservq= LIST_CONTAINOR(plist, struct tx_servq, tx_pending);
			
			pxmitframe = rtw_txservq_dequeue(priv, ptxservq);
		}
		
		txcfg = &pxmitframe->txinsn;
		
		if (((pxmitbuf->agg_num + txcfg->frg_num) > MAX_TX_AGG_PKT_NUM)
				|| ((rtw_txinsn_require_bufsize(pxmitframe->priv, txcfg)+
				PTR_ALIGN(pxmitbuf->pkt_tail, TXAGG_DESC_ALIGN_SZ)) > pxmitbuf->pkt_end))
		{
			if (txcfg->is_pspoll) {
				rtw_pspoll_sta_enqueue(priv, txcfg->pstat, ENQUEUE_TO_HEAD);
				txcfg->is_pspoll = 0;
			}
			if (rtw_enqueue_xmitframe(priv, pxmitframe, ENQUEUE_TO_HEAD) == FALSE) {
				rtw_free_txinsn_resource(pxmitframe->priv, txcfg);
				rtw_free_xmitframe(priv, pxmitframe);
			}
			break;
		}
		
		txcfg->pxmitbuf = pxmitbuf;
		
		// re-assign q_num to avoid txcfg->q_num is not equal tx_servq.q_num for tx_mc_queue.
		// Because q_num of tx_mc_queue will switch to MCAST_QNUM once any STA sleeps.
		// This action is redundent for other queues.
		txcfg->q_num = q_num;
		
		rtw_send_xmitframe(pxmitframe);
		
		if (pxmitbuf->agg_num >= priv->pmib->miscEntry.max_xmitbuf_agg)
			break;
	}
}

int rtl8192cd_usb_tx_recycle(struct rtl8192cd_priv *priv, struct tx_desc_info* pdescinfo)
{
	int needRestartQueue = 0;

	if (pdescinfo->type == _SKB_FRAME_TYPE_)
	{
		struct sk_buff *skb = (struct sk_buff *)(pdescinfo->pframe);
#ifdef MP_TEST
		if (OPMODE & WIFI_MP_CTX_BACKGROUND) {
			skb->data = skb->head;
			skb->tail = skb->data;
			skb->len = 0;
			priv->pshare->skb_tail = (priv->pshare->skb_tail + 1) & (NUM_MP_SKB - 1);
		}
		else
#endif
		{
#ifdef __LINUX_2_6__
			rtl_kfree_skb(pdescinfo->priv, skb, _SKB_TX_IRQ_);
#endif
			needRestartQueue = 1;
		}
	}
	else if (pdescinfo->type == _PRE_ALLOCMEM_)
	{
		release_mgtbuf_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
	}
	else if (pdescinfo->type == _RESERVED_FRAME_TYPE_)
	{
		// the chained skb, no need to release memory
	}
	else
	{
		DEBUG_ERR("Unknown tx frame type %d\n", pdescinfo->type);
	}

	pdescinfo->type = _RESERVED_FRAME_TYPE_;

	return needRestartQueue;
}

void sdio_recycle_xmitbuf(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf)
{
	struct priv_shared_info *pshare = priv->pshare;
	_queue *xmitbuf_queue;
	_irqL irqL;
	int i;

	if (rtw_is_list_empty(&pxmitbuf->tx_xmitbuf_list) == FALSE) {
		xmitbuf_queue = &pshare->tx_xmitbuf_waiting_queue[pxmitbuf->q_num];
		_enter_critical_bh(&xmitbuf_queue->lock, &irqL);

		rtw_list_delete(&pxmitbuf->tx_xmitbuf_list);
		--xmitbuf_queue->qlen;

		_exit_critical_bh(&xmitbuf_queue->lock, &irqL);
	}
	
#ifndef CONFIG_TX_RECYCLE_EARLY
	for (i = 0; i < pxmitbuf->agg_num; ++i) {
		rtl8192cd_usb_tx_recycle(priv, &pxmitbuf->txdesc_info[i]);
	}
#endif
	
	if (BEACON_QUEUE == pxmitbuf->q_num) {
		if (0 == pxmitbuf->status)
			++priv->ext_stats.beacon_ok;
		else
			++priv->ext_stats.beacon_er;
		
		rtw_free_beacon_xmitbuf(priv, pxmitbuf);
	} else {
		rtw_free_xmitbuf(priv, pxmitbuf);
	}

#ifdef MP_TEST
	if ((OPMODE & (WIFI_MP_STATE | WIFI_MP_CTX_BACKGROUND | WIFI_MP_CTX_BACKGROUND_STOPPING)) ==
		(WIFI_MP_STATE | WIFI_MP_CTX_BACKGROUND) )
	{
		notify_mp_ctx_background(priv);
	}
#endif
}

static inline u32 ffaddr2deviceId(struct rtl8192cd_priv *priv, u32 addr)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

	return pHalData->Queue2Pipe[addr];
}

static inline u8 rtw_sdio_get_tx_pageidx(u32 addr)
{
	u8 idx;

	switch (addr)
	{
		case WLAN_TX_HIQ_DEVICE_ID:
			idx = HI_QUEUE_IDX;
			break;
		case WLAN_TX_MIQ_DEVICE_ID:
			idx = MID_QUEUE_IDX;
			break;
		case WLAN_TX_LOQ_DEVICE_ID:
			idx = LOW_QUEUE_IDX;
			break;
		default:
			printk("get_txfifo_pageidx(): wrong TX addr %x\n", addr);
			idx = 0;
			break;
	}

	return idx;
}

static u8 rtw_sdio_wait_enough_TxOQT_space(struct rtl8192cd_priv *priv, u8 agg_num)
{
	u32 n = 0;
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

	while (pHalData->SdioTxOQTFreeSpace < agg_num) {
		if (priv->pshare->bSurpriseRemoved == TRUE) {
			printk("%s: bSurpriseRemoved (wait TxOQT)\n", __func__);
			return FALSE;
		}

		HalQueryTxOQTBufferStatus8189ESdio(priv);
		if ((++n % 60) == 0) {
			printk("%s(%d): QOT does not have enough space(%d), it needs: %d\n",
 				__func__, n, pHalData->SdioTxOQTFreeSpace, agg_num);
			msleep(1);
			yield();
		}
	}

	pHalData->SdioTxOQTFreeSpace -= agg_num;
	
	if (n > 1)
		++priv->pshare->nr_out_of_txoqt_space;

	return TRUE;
}

#if defined(CONFIG_SDIO_TX_MULTI_QUEUE) || defined(CONFIG_SDIO_TX_INTERRUPT)
static s32 rtw_sdio_check_tx_freepage(struct rtl8192cd_priv *priv, u8 page_idx, u8 page_num)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u8 *free_tx_page = pHalData->SdioTxFIFOFreePage;

	if ((free_tx_page[page_idx] + free_tx_page[PUBLIC_QUEUE_IDX]) > (page_num + 1)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static void rtw_sdio_update_tx_freepage(struct rtl8192cd_priv *priv, u8 page_idx, u8 page_num)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u8 *free_tx_page = pHalData->SdioTxFIFOFreePage;
	u8 requiredPublicPage = 0;

	if (page_num <= free_tx_page[page_idx]) {
		free_tx_page[page_idx] -= page_num;
	} else if ((free_tx_page[page_idx] + free_tx_page[PUBLIC_QUEUE_IDX]) > (page_num + 1)) {
		// The number of page which public page included is available.
		requiredPublicPage = page_num - free_tx_page[page_idx];
		free_tx_page[page_idx] = 0;
		free_tx_page[PUBLIC_QUEUE_IDX] -= requiredPublicPage;
	} else {
		printk("update_tx_freepage(): page error !!!!!!\n");
	}
}
#endif // CONFIG_SDIO_TX_MULTI_QUEUE || CONFIG_SDIO_TX_INTERRUPT

struct xmit_buf* get_usable_pending_xmitbuf(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	_list *phead, *plist;
	_irqL irql;
	struct xmit_buf *pxmitbuf, *pxmitbuf_usable = NULL;
	_queue *pqueue;
	
	pqueue = &priv->pshare->pending_xmitbuf_queue;
	phead = get_list_head(pqueue);
	
	_enter_critical_bh(&pqueue->lock, &irql);
	
	plist = phead->prev;
	while (plist != phead) {
		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
		plist = plist->prev;
		
		if (pxmitbuf->q_num == txcfg->q_num) {
			if (((pxmitbuf->agg_num + txcfg->frg_num) <= MAX_TX_AGG_PKT_NUM)
					&& ((pxmitbuf->agg_num + txcfg->frg_num) <= priv->pmib->miscEntry.max_xmitbuf_agg)
					&& ((rtw_txinsn_require_bufsize(priv, txcfg)+
					PTR_ALIGN(pxmitbuf->pkt_tail, TXAGG_DESC_ALIGN_SZ)) <= pxmitbuf->pkt_end)) {
				rtw_list_delete(&pxmitbuf->list);
				--pqueue->qlen;
				
				pxmitbuf_usable = pxmitbuf;
				pxmitbuf_usable->flags |= XMIT_BUF_FLAG_REUSE;
			}
			break;
		}
	}
	
	_exit_critical_bh(&pqueue->lock, &irql);
	
	return pxmitbuf_usable;
}

#ifdef CONFIG_SDIO_TX_MULTI_QUEUE
s32 rtw_sdio_xmitbuf_pending(struct rtl8192cd_priv *priv)
{
	return ((_rtw_queue_empty(&priv->pshare->tx_pending_queue[HI_QUEUE_IDX]) == FALSE) ||
		(_rtw_queue_empty(&priv->pshare->tx_pending_queue[MID_QUEUE_IDX]) == FALSE) ||
		(_rtw_queue_empty(&priv->pshare->tx_pending_queue[LOW_QUEUE_IDX]) == FALSE));
}

void rtw_sdio_enqueue_xmitbuf(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf)
{
	_irqL irql;
	_queue *pqueue = NULL;

	pqueue = &priv->pshare->tx_pending_queue[pxmitbuf->tx_page_idx];

	_enter_critical_bh(&pqueue->lock, &irql);

	rtw_list_insert_tail(&pxmitbuf->list, get_list_head(pqueue));
	++pqueue->qlen;

	_exit_critical_bh(&pqueue->lock, &irql);

	if (test_and_set_bit(WAKE_EVENT_XMIT, &priv->pshare->wake_event) == 0)
		wake_up_process(priv->pshare->xmit_thread);
}

struct xmit_buf* rtw_sdio_dequeue_xmitbuf(struct rtl8192cd_priv *priv, u8 page_idx)
{
	_irqL irql;
	_list *plist = NULL, *phead = NULL;
	struct xmit_buf *pxmitbuf = NULL;
	_queue *pqueue = &priv->pshare->tx_pending_queue[page_idx];

	_enter_critical_bh(&pqueue->lock, &irql);

	if (_rtw_queue_empty(pqueue) == FALSE)
	{
		phead = get_list_head(pqueue);
		plist = get_next(phead);
		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
		
		if (rtw_sdio_check_tx_freepage(priv, page_idx, pxmitbuf->pg_num) == TRUE) {
			rtw_sdio_update_tx_freepage(priv, page_idx, pxmitbuf->pg_num);
			rtw_list_delete(&pxmitbuf->list);
			--pqueue->qlen;
		} else {
			pxmitbuf = NULL;
		}
	}

	_exit_critical_bh(&pqueue->lock, &irql);

	return pxmitbuf;
}

s32 rtw_sdio_xmit_buf_handler(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	struct xmit_buf *pxmitbuf = NULL;
	u8 pageIndex = 0;
	u32 n = 0;

	if (!rtw_sdio_xmitbuf_pending(priv))
		return FAIL;

	do {
		pxmitbuf = rtw_sdio_dequeue_xmitbuf(priv, pageIndex);

		if (NULL == pxmitbuf) {
			if (!rtw_sdio_xmitbuf_pending(priv))
				return FAIL;

			if ((priv->pshare->bSurpriseRemoved == TRUE) || (priv->pshare->bDriverStopped == TRUE))
				return FAIL;
			
			pageIndex++;
			if (pageIndex == pHalData->OutEpNumber) {
				// if go throught all pending queue, check if need query or back to the first queue.
				pageIndex = 0;

				if ((++n % 60) == 0) {
					DEBUG_INFO("%s: FIFO starvation!\n", __func__);
					msleep(1);
					yield();
				}
				
				HalQueryTxBufferStatus8189ESdio(priv);
			}
			continue;
		} else {
			n = 0;
		}

		if (priv->pshare->bSurpriseRemoved == TRUE) {
			printk("%s: bSurpriseRemoved (write port)\n", __func__);
			goto free_xmitbuf;
		}

		if (rtw_sdio_wait_enough_TxOQT_space(priv, pxmitbuf->agg_num) == FALSE) {
			goto free_xmitbuf;
		}

		sdio_write_port(priv, ffaddr2deviceId(priv, pxmitbuf->q_num), pxmitbuf->len, (u8 *)pxmitbuf);

free_xmitbuf:
		sdio_recycle_xmitbuf(priv, pxmitbuf);

		tasklet_hi_schedule(&priv->pshare->xmit_tasklet);

	} while (1);

	return SUCCESS;
}

void rtw_flush_xmit_pending_queue(struct rtl8192cd_priv *priv)
{
	int i;
	_irqL irql;
	_list *plist = NULL, *phead = NULL;
	struct xmit_buf *pxmitbuf = NULL;
	_queue *pqueue = NULL;

	for (i = 0; i < ARRAY_SIZE(priv->pshare->tx_pending_queue); ++i) {
		pqueue = &priv->pshare->tx_pending_queue[i];

		_enter_critical_bh(&pqueue->lock, &irql);

		phead = get_list_head(pqueue);
		while (rtw_is_list_empty(phead) == FALSE) {

			plist = get_next(phead);
			rtw_list_delete(plist);

			pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
			sdio_recycle_xmitbuf(priv, pxmitbuf);
		}
		pqueue->qlen = 0;

		_exit_critical_bh(&pqueue->lock, &irql);
	}
}

#else // CONFIG_SDIO_TX_MULTI_QUEUE

void enqueue_pending_xmitbuf(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf)
{
	_irqL irql;
	_queue *pqueue = &priv->pshare->pending_xmitbuf_queue;

	_enter_critical_bh(&pqueue->lock, &irql);

	rtw_list_insert_tail(&pxmitbuf->list, get_list_head(pqueue));
	++pqueue->qlen;

	_exit_critical_bh(&pqueue->lock, &irql);

#ifdef CONFIG_SDIO_TX_INTERRUPT
	if (GET_HAL_INTF_DATA(priv)->SdioTxIntStatus)
		return;
#endif
	if (test_and_set_bit(WAKE_EVENT_XMIT, &priv->pshare->wake_event) == 0)
		wake_up_process(priv->pshare->xmit_thread);
}

struct xmit_buf* dequeue_pending_xmitbuf(struct rtl8192cd_priv *priv)
{
	_list *phead = NULL, *plist = NULL;
	_irqL irql;
	struct xmit_buf *pxmitbuf = NULL;
	_queue *pqueue = &priv->pshare->pending_xmitbuf_queue;
	
	phead = get_list_head(pqueue);

	_enter_critical_bh(&pqueue->lock, &irql);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		plist = get_next(phead);
		rtw_list_delete(plist);
		--pqueue->qlen;
	}

	_exit_critical_bh(&pqueue->lock, &irql);
	
	if (plist)
		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);

	return pxmitbuf;
}

#ifdef CONFIG_SDIO_TX_INTERRUPT
struct xmit_buf* try_to_dequeue_pending_xmitbuf(struct rtl8192cd_priv *priv, int from)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	_list *phead, *plist = NULL;
	_irqL irql;
	struct xmit_buf *pxmitbuf;
	_queue *pqueue;
	
#ifdef CONFIG_SDIO_TX_IN_INTERRUPT
	if ((SDIO_TX_THREAD == from) && (pHalData->SdioTxIntStatus & BIT(SDIO_TX_INT_WORKING)))
		return NULL;
#endif
	
	pqueue = &priv->pshare->pending_xmitbuf_queue;
	phead = get_list_head(pqueue);
	
do_check:
	
	pxmitbuf = NULL;
	
	_enter_critical_bh(&pqueue->lock, &irql);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		plist = get_next(phead);
		pxmitbuf = LIST_CONTAINOR(plist, struct xmit_buf, list);
		
		if (rtw_sdio_check_tx_freepage(priv, pxmitbuf->tx_page_idx, pxmitbuf->pg_num) == TRUE) {
			rtw_sdio_update_tx_freepage(priv, pxmitbuf->tx_page_idx, pxmitbuf->pg_num);
			rtw_list_delete(plist);
			--pqueue->qlen;
			set_bit(from, &priv->pshare->freepage_updated);
		}
	}

	_exit_critical_bh(&pqueue->lock, &irql);
	
	if (pxmitbuf) {
		if (!priv->pshare->freepage_updated) {
			// If free page is NOT enough, then update current FIFO status and check again
#ifdef CONFIG_SDIO_TX_IN_INTERRUPT
			HalQueryTxBufferStatus8189ESdio2(priv);
#else
			HalQueryTxBufferStatus8189ESdio(priv);
#endif
			if (rtw_sdio_check_tx_freepage(priv, pxmitbuf->tx_page_idx, pxmitbuf->pg_num) == TRUE)
				goto do_check;
			
			if (test_and_set_bit(SDIO_TX_INT_SETUP_TH, &pHalData->SdioTxIntStatus))
				return NULL;
			pHalData->SdioTxIntQIdx = pxmitbuf->tx_page_idx;
			RTL_W8(reg_freepage_thres[pxmitbuf->tx_page_idx], pxmitbuf->pg_num+2);
			
#ifdef CONFIG_SDIO_TX_IN_INTERRUPT
			HalQueryTxBufferStatus8189ESdio2(priv);
#else
			HalQueryTxBufferStatus8189ESdio(priv);
#endif
			// check if free page is available to avoid race condition from SDIO ISR to cause NO interrupt issue
			if ((rtw_sdio_check_tx_freepage(priv, pxmitbuf->tx_page_idx, pxmitbuf->pg_num) == TRUE)
					|| (plist != get_next(phead))) {
				if (test_and_clear_bit(SDIO_TX_INT_SETUP_TH, &pHalData->SdioTxIntStatus)) {
					// Invalidate TX Free Page Threshold
					RTL_W8(reg_freepage_thres[pHalData->SdioTxIntQIdx], 0xFF);
					goto do_check;
				}
			}
			DEBUG_INFO("[%s %d] tx_page_idx=%d, pg_num=%d, TxIntStatus=%ld\n",
				__FUNCTION__, __LINE__, pxmitbuf->tx_page_idx, pxmitbuf->pg_num, pHalData->SdioTxIntStatus);
			
			pxmitbuf = NULL;
		}
	}
	
	return pxmitbuf;
	
}

s32 rtl8188es_dequeue_writeport(struct rtl8192cd_priv *priv, int from)
{
	struct xmit_buf *pxmitbuf;
	u32 deviceId;

	pxmitbuf = try_to_dequeue_pending_xmitbuf(priv, from);
	if (NULL == pxmitbuf)
		return FAIL;
	
	// statistics only for data frame
	if (MGNT_QUEUE != pxmitbuf->q_num) {
		priv->pshare->xmitbuf_agg_num = pxmitbuf->agg_num;
	}

	// translate queue index to sdio fifo addr
	deviceId = ffaddr2deviceId(priv, pxmitbuf->q_num);
	
	if (priv->pshare->bSurpriseRemoved == TRUE)
	{
		printk("%s: bSurpriseRemoved (write port)\n", __func__);
		goto free_xmitbuf;
	}
	
	if (rtw_sdio_wait_enough_TxOQT_space(priv, pxmitbuf->agg_num) == FALSE) {
		goto free_xmitbuf;
	}

	sdio_write_port(priv, deviceId, pxmitbuf->len, (u8 *)pxmitbuf);
	clear_bit(from, &priv->pshare->freepage_updated);

free_xmitbuf:

	sdio_recycle_xmitbuf(priv, pxmitbuf);
	// Considering TP, do every one in dequeue_writeport  is better than do only one in xmit_buf_handler
	tasklet_hi_schedule(&priv->pshare->xmit_tasklet);

	return SUCCESS;
}

s32 rtl8188es_xmit_buf_handler(struct rtl8192cd_priv *priv, int from)
{
	u32 cnt = 0;
	
	while (rtl8188es_dequeue_writeport(priv, from) == SUCCESS) {
		if ((priv->pshare->bSurpriseRemoved == TRUE) || (priv->pshare->bDriverStopped == TRUE))
			break;
		++cnt;
	}
	
	if (SDIO_TX_THREAD == from)
		priv->pshare->nr_xmitbuf_handled_in_thread += cnt;

	return SUCCESS;
}

#else // !CONFIG_SDIO_TX_INTERRUPT
s32 rtl8188es_dequeue_writeport(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u8 *freePage = pHalData->SdioTxFIFOFreePage;
	struct xmit_buf *pxmitbuf;
	u32 deviceId;
	u32 requiredPage;
	u8 PageIdx;
	u32 n = 0;

	pxmitbuf = dequeue_pending_xmitbuf(priv);
	if (NULL == pxmitbuf)
		return FAIL;

	requiredPage = pxmitbuf->pg_num;

	// translate queue index to sdio fifo addr
	deviceId = ffaddr2deviceId(priv, pxmitbuf->q_num);

	// translate sdio fifo addr to tx fifo page index
	PageIdx = pxmitbuf->tx_page_idx;

	// check if hardware tx fifo page is enough
	do {
		if (requiredPage <= freePage[PageIdx]) {
			freePage[PageIdx] -= requiredPage;
			break;
		}
		// The number of page which public page included is available.
		if ((freePage[PageIdx] + freePage[PUBLIC_QUEUE_IDX]) > (requiredPage + 1))
		{
			u8 requiredPublicPage;

			requiredPublicPage = requiredPage - freePage[PageIdx];
			freePage[PageIdx] = 0;
			freePage[PUBLIC_QUEUE_IDX] -= requiredPublicPage;
			break;
		}

		if ((priv->pshare->bSurpriseRemoved == TRUE) || (priv->pshare->bDriverStopped == TRUE))
		{
			printk("%s: bSurpriseRemoved OR bDriverStopped (update TX FIFO page)\n", __func__);
			goto free_xmitbuf;
		}

		if ((++n % 60) == 0) {//or 80
			//printk("%s: FIFO starvation!(%d) len=%d agg=%d page=(R)%d(A)%d\n",
			//	__func__, n, pxmitbuf->len, pxmitbuf->agg_num, pxmitbuf->pg_num, freePage[PageIdx] + freePage[PUBLIC_QUEUE_IDX]);
			DEBUG_INFO("%s: FIFO starvation!\n", __func__);
			msleep(1);
			yield();
		}

		// Total number of page is NOT available, so update current FIFO status
		HalQueryTxBufferStatus8189ESdio(priv);

	} while (1);

	if (priv->pshare->bSurpriseRemoved == TRUE)
	{
		printk("%s: bSurpriseRemoved (write port)\n", __func__);
		goto free_xmitbuf;
	}


	if (rtw_sdio_wait_enough_TxOQT_space(priv, pxmitbuf->agg_num) == FALSE) {
		goto free_xmitbuf;
	}

	sdio_write_port(priv, deviceId, pxmitbuf->len, (u8 *)pxmitbuf);

free_xmitbuf:

	sdio_recycle_xmitbuf(priv, pxmitbuf);

	tasklet_hi_schedule(&priv->pshare->xmit_tasklet);

	return SUCCESS;
}

s32 rtl8188es_xmit_buf_handler(struct rtl8192cd_priv *priv)
{
	while (rtl8188es_dequeue_writeport(priv) == SUCCESS) {
		if ((priv->pshare->bSurpriseRemoved == TRUE) || (priv->pshare->bDriverStopped == TRUE))
			break;
	}

	return SUCCESS;
}
#endif // CONFIG_SDIO_TX_INTERRUPT

void rtw_flush_xmit_pending_queue(struct rtl8192cd_priv *priv)
{
	struct xmit_buf *pxmitbuf = NULL;

	while (NULL != (pxmitbuf = dequeue_pending_xmitbuf(priv))) {
		sdio_recycle_xmitbuf(priv, pxmitbuf);
	}
}
#endif // CONFIG_SDIO_TX_MULTI_QUEUE

void rtl8192cd_usb_cal_txdesc_chksum(struct tx_desc *ptxdesc)
{
	u16 *usPtr = (u16*)ptxdesc;
	u32 count = 16;		// (32 bytes / 2 bytes per XOR) => 16 times
	u32 index;
	u16 checksum = 0;

	//Clear first
	ptxdesc->Dword7 &= cpu_to_le32(0xffff0000);
	
	for(index = 0 ; index < count ; index++){
		checksum = checksum ^ le16_to_cpu(*(usPtr + index));
	}
	
	ptxdesc->Dword7 |= cpu_to_le32(0x0000ffff&checksum);
}

u32 sdio_submit_xmitbuf(struct rtl8192cd_priv *priv, struct xmit_buf *pxmitbuf)
{
	_queue *xmitbuf_queue;
	_irqL irqL;
	struct tx_desc *pdesc;
	int i;
	const int q_num = pxmitbuf->q_num;
	
	BUG_ON(pxmitbuf->pkt_tail > pxmitbuf->pkt_end);
	
	pdesc = (struct tx_desc*) pxmitbuf->pkt_data;
	pxmitbuf->len = (u32)(pxmitbuf->pkt_tail - pxmitbuf->pkt_data);
	
	pxmitbuf->pg_num = 0;
	for (i = 0; i < pxmitbuf->agg_num; ++i) {
		pxmitbuf->pg_num += (pxmitbuf->txdesc_info[i].buf_len + 127) / 128;
	}
	
	// translate sdio fifo addr to tx fifo page index
	pxmitbuf->tx_page_idx = rtw_sdio_get_tx_pageidx(ffaddr2deviceId(priv, q_num));

	if (pxmitbuf->pkt_offset) {
		pdesc->Dword1 |= set_desc(pxmitbuf->pkt_offset << TX_PktOffsetSHIFT);
	}
	
	if (pxmitbuf->agg_num > 1) {
		// clear USB_TXAGG_NUM field to avoid erroneous settings from re-used pending xmitbuf
		pdesc->Dword5 &= ~ set_desc(TX_UsbAggNumMask << TX_UsbAggNumSHIFT);
		pdesc->Dword5 |= set_desc(pxmitbuf->agg_num << TX_UsbAggNumSHIFT);
	}
	
	rtl8192cd_usb_cal_txdesc_chksum(pdesc);

	if (BEACON_QUEUE == q_num) {
		sdio_write_port(priv, ffaddr2deviceId(priv, q_num), pxmitbuf->len, (u8 *)pxmitbuf);

		pxmitbuf->use_hw_queue = 0;
		// Release the ownership of the HW TX queue
		clear_bit(q_num, &priv->pshare->use_hw_queue_bitmap);

		sdio_recycle_xmitbuf(priv, pxmitbuf);
	} else {
		if (!(pxmitbuf->flags & XMIT_BUF_FLAG_REUSE)) {
			xmitbuf_queue = &priv->pshare->tx_xmitbuf_waiting_queue[q_num];
			_enter_critical_bh(&xmitbuf_queue->lock, &irqL);

			rtw_list_insert_tail(&pxmitbuf->tx_xmitbuf_list, &xmitbuf_queue->queue);
			++xmitbuf_queue->qlen;

			_exit_critical_bh(&xmitbuf_queue->lock, &irqL);
		}

		pxmitbuf->use_hw_queue = 0;
		/* Enqueue the pxmitbuf, and it will be dequeued by a xmit thread later */
#ifdef CONFIG_SDIO_TX_MULTI_QUEUE
		rtw_sdio_enqueue_xmitbuf(priv, pxmitbuf);
#else
		enqueue_pending_xmitbuf(priv, pxmitbuf);
#endif
		// Release the ownership of the HW TX queue
		clear_bit(q_num, &priv->pshare->use_hw_queue_bitmap);
	}
	
	return 0;
}

int rtw_xmit_thread(void *context)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv*) context;
	struct priv_shared_info *pshare = priv->pshare;
	
	while (1)
	{
		wait_event(pshare->xmit_waitqueue, test_and_clear_bit(WAKE_EVENT_XMIT, &pshare->wake_event));

		if ((priv->pshare->bSurpriseRemoved == TRUE) || (priv->pshare->bDriverStopped == TRUE)) {
			// avoid to continue calling wake_up_process() when xmit_thread is NULL
			set_bit(WAKE_EVENT_XMIT, &priv->pshare->wake_event);
			
			printk("%s: bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
				__func__, priv->pshare->bDriverStopped, priv->pshare->bSurpriseRemoved);
			goto out;
		}
		
		pshare->nr_xmit_thread_run++;
		
#ifdef CONFIG_SDIO_TX_MULTI_QUEUE
		rtw_sdio_xmit_buf_handler(priv);
#else
#ifdef CONFIG_SDIO_TX_INTERRUPT
		if (GET_HAL_INTF_DATA(priv)->SdioTxIntStatus)
			continue;
		
		rtl8188es_xmit_buf_handler(priv, SDIO_TX_THREAD);
#else
		rtl8188es_xmit_buf_handler(priv);
#endif
#endif
	}
	
out:
	complete_and_exit(&pshare->xmit_thread_done, 0);
}

