/*
 *  SDIO cmd handle routines
 *
 *  $Id: 8188e_sdio_cmd.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8188E_SDIO_CMD_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"


struct cmd_header {
	_list list;
	struct rtl8192cd_priv *priv;
	struct stat_info *pstat;
	u16 cmd_code;
	u16 cmd_len;
};

struct slot_time_parm {
	struct cmd_header header;
	u16 use_short;
};

struct switch_edca_parm {
	struct cmd_header header;
	u32 be_edca;
	u32 vi_edca;
	u16 disable_txop_cfe;
};

struct check_dig_parm {
	struct cmd_header header;
	u8 rssi;
};

struct set_key_parm {
	struct cmd_header header;
	struct net_device *dev;
	unsigned long KeyIndex;
//	unsigned long KeyLen;
	unsigned char KeyType;
	unsigned char EncType;
        unsigned char MACAddr[MACADDRLEN];
	unsigned char Key[32];
};

struct disconnect_sta_parm {
	struct cmd_header header;
	struct net_device *dev;
	DOT11_DISCONNECT_REQ DisconnectReq;
};

struct indicate_mic_failure_parm {
	struct cmd_header header;
	struct net_device *dev;
	unsigned char MACAddr[MACADDRLEN];
	unsigned char indicate_upper_layer;
};

struct recv_mgnt_frame_parm {
	struct cmd_header header;
	struct rx_frinfo *pfrinfo;
};

struct macid_pause_parm {
	struct cmd_header header;
        u16 macid;
	u16 pause;
};

struct macid_nolink_parm {
	struct cmd_header header;
        u16 macid;
	u16 nolink;
};

u8 callback_update_sta_RATid(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_del_sta(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);

u8 callback_40M_RRSR_SC_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_NAV_prot_len_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_slot_time_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_NBI_filter_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
#ifdef SW_ANT_SWITCH
u8 callback_antenna_switch(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
#endif
u8 callback_mp_ctx_background(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_IOT_EDCA_switch(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_check_DIG_by_rssi(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_set_key(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_disconnect_sta(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_indicate_MIC_failure(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_indicate_MIC_failure_clnt(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_recv_mgnt_frame(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
#ifdef CONFIG_RTL_WAPI_SUPPORT
u8 callback_recv_wai_frame(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
#endif
u8 callback_tx_report_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_tx_report_interval_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_macid_pause_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_macid_no_link_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);

#ifdef SDIO_AP_OFFLOAD
u8 callback_ap_offload(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_ap_unoffload(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
#endif

typedef u8 (*cmd_callback)(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);

struct _cmd_callback {
	u32 cmd_code;
	cmd_callback callback;
};

struct _cmd_callback rtw_cmd_callback[] = {
	{ _CMD_UPDATE_STA_RATID, callback_update_sta_RATid },
	{ _CMD_DEL_STA, callback_del_sta },
	
	{ _CMD_40M_RRSR_SC_CHANGE, callback_40M_RRSR_SC_change },
	{ _CMD_NAV_PROT_LEN_CHANGE, callback_NAV_prot_len_change },
	{ _CMD_SLOT_TIME_CHANGE, callback_slot_time_change },
	{ _CMD_NBI_FILTER_CHANGE, callback_NBI_filter_change },
#ifdef SW_ANT_SWITCH
	{ _CMD_ANTENNA_SWITCH, callback_antenna_switch },
#endif
	{ _CMD_MP_CTX_BACKGROUND, callback_mp_ctx_background },
	{ _CMD_IOT_EDCA_SWITCH, callback_IOT_EDCA_switch },
	{ _CMD_CHECK_DIG_BY_RSSI, callback_check_DIG_by_rssi },
	{ _CMD_SET_KEY, callback_set_key },
	{ _CMD_DISCONNECT_STA, callback_disconnect_sta },
	{ _CMD_INDICATE_MIC_FAILURE, callback_indicate_MIC_failure },
	{ _CMD_INDICATE_MIC_FAILURE_CLNT, callback_indicate_MIC_failure_clnt },
	{ _CMD_RECV_MGNT_FRAME, callback_recv_mgnt_frame },
#ifdef CONFIG_RTL_WAPI_SUPPORT
	{ _CMD_RECV_WAI_FRAME, callback_recv_wai_frame },
#endif
	{ _CMD_TX_REPORT_CHANGE, callback_tx_report_change },
	{ _CMD_TX_REPORT_INTERVAL_CHANGE, callback_tx_report_interval_change },
	{ _CMD_MACID_PAUSE_CHANGE, callback_macid_pause_change },
	{ _CMD_MACID_NO_LINK_CHANGE, callback_macid_no_link_change },
#ifdef SDIO_AP_OFFLOAD
	{ _CMD_AP_OFFLOAD, callback_ap_offload },
	{ _CMD_AP_UNOFFLOAD, callback_ap_unoffload },
#endif
};

int _rtw_init_cmd_priv(struct rtl8192cd_priv *priv)
{
	struct priv_shared_info *pshare;
	
	WARN_ON(ARRAY_SIZE(rtw_cmd_callback) != MAX_RTW_CMD_CODE);
	
	pshare = priv->pshare;
	
	init_waitqueue_head(&pshare->waitqueue);
	init_completion(&pshare->cmd_thread_done);
	pshare->cmd_thread = NULL;
	__clear_bit(WAKE_EVENT_CMD, &pshare->wake_event);
	
	_rtw_init_queue(&pshare->cmd_queue);
	_rtw_init_queue(&pshare->rx_mgt_queue);
	_rtw_init_queue(&pshare->timer_evt_queue);
	
	return SUCCESS;
}

void _rtw_free_cmd_priv(struct rtl8192cd_priv *priv)
{
	_rtw_spinlock_free(&priv->pshare->timer_evt_queue.lock);
	_rtw_spinlock_free(&priv->pshare->rx_mgt_queue.lock);
	_rtw_spinlock_free(&priv->pshare->cmd_queue.lock);
}

static int rtw_enqueue_cmd(struct rtl8192cd_priv *priv, _queue *cmd_queue, struct cmd_header *pcmdhdr)
{
	_irqL irqL;
	
	_enter_critical_bh(&cmd_queue->lock, &irqL);
	
	rtw_list_insert_tail(&pcmdhdr->list, &cmd_queue->queue);
	
	++cmd_queue->qlen;
	
	_exit_critical_bh(&cmd_queue->lock, &irqL);
	
	if (test_and_set_bit(WAKE_EVENT_CMD, &priv->pshare->wake_event) == 0)
		wake_up_process(priv->pshare->cmd_thread);
	
	return TRUE;
}

static struct cmd_header* rtw_dequeue_cmd(struct rtl8192cd_priv *priv, _queue *cmd_queue)
{
	_list *phead, *plist;
	_irqL irqL;
	
	struct cmd_header *cmd_obj = NULL;
	
	plist = NULL;
	
	phead = get_list_head(cmd_queue);
	
	_enter_critical_bh(&cmd_queue->lock, &irqL);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		plist = get_next(phead);
		rtw_list_delete(plist);
		--cmd_queue->qlen;
	}
	
	_exit_critical_bh(&cmd_queue->lock, &irqL);
	
	if (plist)
		cmd_obj = LIST_CONTAINOR(plist, struct cmd_header, list);
	
	return cmd_obj;
}

int rtw_enqueue_timer_event(struct rtl8192cd_priv *priv, struct timer_event_entry *entry, int insert_tail)
{
	_irqL irqL;
	_queue *pqueue = &priv->pshare->timer_evt_queue;
	int insert = 0;
	
	_enter_critical_bh(&pqueue->lock, &irqL);
	
	if (rtw_is_list_empty(&entry->list) == TRUE) {
		if (ENQUEUE_TO_TAIL == insert_tail)
			rtw_list_insert_tail(&entry->list, &pqueue->queue);
		else
			rtw_list_insert_head(&entry->list, &pqueue->queue);
		++pqueue->qlen;
		insert = 1;
	}
	
	_exit_critical_bh(&pqueue->lock, &irqL);
	
	if (insert) {
		if (test_and_set_bit(WAKE_EVENT_CMD, &priv->pshare->wake_event) == 0)
			wake_up_process(priv->pshare->cmd_thread);
	} else {
		++priv->pshare->nr_timer_evt_miss;
	}
	
	return TRUE;
}

struct timer_event_entry* rtw_dequeue_timer_event(struct rtl8192cd_priv *priv)
{
	_list *phead, *plist;
	_irqL irqL;
	struct timer_event_entry *entry = NULL;
	_queue *pqueue = &priv->pshare->timer_evt_queue;
	
	plist = NULL;
	
	phead = get_list_head(pqueue);
	
	_enter_critical_bh(&pqueue->lock, &irqL);
	
	if (rtw_is_list_empty(phead) == FALSE) {
		plist = get_next(phead);
		rtw_list_delete(plist);
		--pqueue->qlen;
	}
	
	_exit_critical_bh(&pqueue->lock, &irqL);
	
	if (plist)
		entry = LIST_CONTAINOR(plist, struct timer_event_entry, list);
	
	return entry;
}

void timer_event_timer_fn(unsigned long __data)
{
	struct timer_event_entry *entry = (struct timer_event_entry *)__data;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)entry->data;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	rtw_enqueue_timer_event(priv, entry, ENQUEUE_TO_TAIL);
}

static void rtw_split_cmd_queue(struct rtl8192cd_priv *priv, struct stat_info *pstat,
	_queue *from_queue, _list *to_list)
{
	_list *phead, *plist;
	_irqL irqL;
	
	struct cmd_header *cmd_obj;
	
	phead = get_list_head(from_queue);
	
	_enter_critical_bh(&from_queue->lock, &irqL);
	
	plist = get_next(phead);
	
	while (rtw_end_of_queue_search(phead, plist) == FALSE) {
		cmd_obj = LIST_CONTAINOR(plist, struct cmd_header, list);
		plist = get_next(plist);
		
		if ((cmd_obj->priv == priv) && (cmd_obj->pstat == pstat)) {
			rtw_list_delete(&cmd_obj->list);
			rtw_list_insert_tail(&cmd_obj->list, to_list);
			--from_queue->qlen;
		}
	}
	
	_exit_critical_bh(&from_queue->lock, &irqL);
}

void rtw_flush_cmd_queue(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	_list cmd_list;
	_list *phead, *plist;
	struct cmd_header *cmd_obj;
	
	if ((NULL != pstat) && (0 == pstat->pending_cmd))
		return;
	
	phead = &cmd_list;
	
	_rtw_init_listhead(phead);
	
	rtw_split_cmd_queue(priv, pstat, &priv->pshare->cmd_queue, phead);
	
	plist = get_next(phead);
	
	while (rtw_end_of_queue_search(phead, plist) == FALSE) {
		cmd_obj = LIST_CONTAINOR(plist, struct cmd_header, list);
		plist = get_next(plist);
		
		if (cmd_obj->pstat)
			clear_bit(cmd_obj->cmd_code, &cmd_obj->pstat->pending_cmd);
		rtw_mfree((u8*)cmd_obj, cmd_obj->cmd_len);
	}
}

void rtw_flush_rx_mgt_queue(struct rtl8192cd_priv *priv)
{
	_list cmd_list;
	_list *phead, *plist;
	struct cmd_header *cmd_obj;
	struct recv_mgnt_frame_parm *param;
	
	phead = &cmd_list;
	
	_rtw_init_listhead(phead);
	
	rtw_split_cmd_queue(priv, NULL, &priv->pshare->rx_mgt_queue, phead);
	
	plist = get_next(phead);
	
	while (rtw_end_of_queue_search(phead, plist) == FALSE) {
		cmd_obj = LIST_CONTAINOR(plist, struct cmd_header, list);
		plist = get_next(plist);
		
		param = (struct recv_mgnt_frame_parm *)cmd_obj;
		rtl_kfree_skb(priv, param->pfrinfo->pskb, _SKB_RX_);
		rtw_mfree((u8*)cmd_obj, cmd_obj->cmd_len);
	}
}

int rtw_cmd_thread(void *context)
{
	struct rtl8192cd_priv *priv;
	struct priv_shared_info *pshare;
	struct cmd_header* pcmd;
	cmd_callback cmd_handler;
	
	priv = (struct rtl8192cd_priv*) context;
	pshare = priv->pshare;
	
//	set_user_nice(current, -10);
	
	while (1)
	{
		wait_event(pshare->waitqueue, test_and_clear_bit(WAKE_EVENT_CMD, &pshare->wake_event));
		
		while (1)
		{
			if ((TRUE == pshare->bDriverStopped) ||(TRUE == pshare->bSurpriseRemoved)) {
				// avoid to continue calling wake_up_process() when cmd_thread is NULL
				set_bit(WAKE_EVENT_CMD, &pshare->wake_event);
				
				printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
					__FUNCTION__, pshare->bDriverStopped, pshare->bSurpriseRemoved);
				goto out;
			}
			
			pcmd = NULL;
			
			if (_rtw_queue_empty(&pshare->timer_evt_queue) == FALSE) {
				struct timer_event_entry *timer_event;
				
				timer_event = rtw_dequeue_timer_event(priv);
				if (NULL != timer_event) {
					++pshare->nr_timer_evt;
					timer_event->function(timer_event->data);
					continue;
				}
			}
			
			if (_rtw_queue_empty(&pshare->rx_mgt_queue) == FALSE) {
				pcmd = rtw_dequeue_cmd(priv, &pshare->rx_mgt_queue);
				if (NULL != pcmd) {
					++pshare->nr_rx_mgt_cmd;
					goto handle_cmd;
				}
			}
			
			pcmd = rtw_dequeue_cmd(priv, &pshare->cmd_queue);
			if (NULL != pcmd) {
				++pshare->nr_cmd;
				goto handle_cmd;
			}
			
			break;
			
handle_cmd:
			if (pcmd->cmd_code < ARRAY_SIZE(rtw_cmd_callback)) {
				cmd_handler = rtw_cmd_callback[pcmd->cmd_code].callback;
				cmd_handler(pcmd->priv, pcmd);
			}
			
			rtw_mfree((u8*)pcmd, pcmd->cmd_len);
		}
	}
	
out:
	
	complete_and_exit(&pshare->cmd_thread_done, 0);
}

void notify_update_sta_RATid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_UPDATE_STA_RATID, &pstat->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		clear_bit(_CMD_UPDATE_STA_RATID, &pstat->pending_cmd);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_UPDATE_STA_RATID;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	pcmdhdr->pstat = pstat;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_update_sta_RATid(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct stat_info *pstat = pcmdhdr->pstat;
	
	clear_bit(_CMD_UPDATE_STA_RATID, &pstat->pending_cmd);
	
	add_RATid(priv, pstat);
	
	return TRUE;
}

void notify_del_sta(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_DEL_STA, &pstat->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		clear_bit(_CMD_DEL_STA, &pstat->pending_cmd);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_DEL_STA;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	pcmdhdr->pstat = pstat;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_del_sta(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct stat_info *pstat = pcmdhdr->pstat;
	
	if (test_and_clear_bit(_CMD_DEL_STA, &pstat->pending_cmd)) {
		unsigned char tmpbuf[16];
		
		sprintf((char *)tmpbuf, "%02x%02x%02x%02x%02x%02x",
			pstat->hwaddr[0], pstat->hwaddr[1], pstat->hwaddr[2],
			pstat->hwaddr[3], pstat->hwaddr[4], pstat->hwaddr[5]);
		del_sta(pcmdhdr->priv, tmpbuf);
	}
	
	return TRUE;
}

void notify_40M_RRSR_SC_change(struct rtl8192cd_priv *priv)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_40M_RRSR_SC_CHANGE, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_40M_RRSR_SC_CHANGE;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_40M_RRSR_SC_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	clear_bit(_CMD_40M_RRSR_SC_CHANGE, priv->pshare->pending_cmd);
	
	if (priv->pshare->is_40m_bw) {
		if (orSTABitMap(&priv->pshare->marvellMapBit) == 0) {
			if (0 != priv->pshare->Reg_RRSR_2) {
				RTL_W8(RRSR+2, priv->pshare->Reg_RRSR_2);
				RTL_W8(0x81b, priv->pshare->Reg_81b);
				priv->pshare->Reg_RRSR_2 = 0;
				priv->pshare->Reg_81b = 0;
			}
		} else {
			if (0 == priv->pshare->Reg_RRSR_2) {
				priv->pshare->Reg_RRSR_2 = RTL_R8(RRSR+2);
				priv->pshare->Reg_81b = RTL_R8(0x81b);
				RTL_W8(RRSR+2, priv->pshare->Reg_RRSR_2 | 0x60);
				RTL_W8(0x81b, priv->pshare->Reg_81b | 0x0E); 
			}
		}
	}
	
	return TRUE;
}

void notify_NAV_prot_len_change(struct rtl8192cd_priv *priv)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_NAV_PROT_LEN_CHANGE, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_NAV_PROT_LEN_CHANGE;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_NAV_prot_len_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	clear_bit(_CMD_NAV_PROT_LEN_CHANGE, priv->pshare->pending_cmd);
	
	if (orSTABitMap(&priv->pshare->mimo_ps_dynamic_sta)) {
		RTL_W8(NAV_PROT_LEN, 0x40);
	} else {
		RTL_W8(NAV_PROT_LEN, 0x20);
	}
	
	return TRUE;
}

void notify_slot_time_change(struct rtl8192cd_priv *priv, int use_short)
{
	struct slot_time_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct slot_time_parm *)rtw_zmalloc(sizeof(struct slot_time_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_SLOT_TIME_CHANGE;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->use_short = use_short;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_slot_time_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct slot_time_parm *param = (struct slot_time_parm *)pcmdhdr;
	
	if (param->use_short)
		RTL_W8(SLOT_TIME, 0x09);
	else
		RTL_W8(SLOT_TIME, 0x14);
	
	return TRUE;
}

void notify_NBI_filter_change(struct rtl8192cd_priv *priv)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_NBI_FILTER_CHANGE, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_NBI_FILTER_CHANGE;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_NBI_filter_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	clear_bit(_CMD_NBI_FILTER_CHANGE, priv->pshare->pending_cmd);
	
	if (0 == priv->pshare->phw->nbi_filter_on) {
		RTL_W16(rOFDM0_RxDSP, RTL_R16(rOFDM0_RxDSP) & ~ BIT(9));	// NBI off
	} else {
		RTL_W16(rOFDM0_RxDSP, RTL_R16(rOFDM0_RxDSP) | BIT(9));		// NBI on
	}
	
	return TRUE;
}

#ifdef SW_ANT_SWITCH
void notify_antenna_switch(struct rtl8192cd_priv *priv, u8 nextAntenna)
{
	struct cmd_header *pcmdhdr;
	
	priv->pshare->DM_SWAT_Table.NextAntenna = nextAntenna;
	
	if (test_and_set_bit(_CMD_ANTENNA_SWITCH, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_ANTENNA_SWITCH;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_antenna_switch(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	u8 nextAntenna;
	
	clear_bit(_CMD_ANTENNA_SWITCH, priv->pshare->pending_cmd);

	nextAntenna = priv->pshare->DM_SWAT_Table.NextAntenna;
	
#ifdef GPIO_ANT_SWITCH
	PHY_SetBBReg(priv, GPIO_PIN_CTRL, 0x3000, nextAntenna);	
#else		
	if (!priv->pshare->rf_ft_var.antSw_select)
		PHY_SetBBReg(priv, rFPGA0_XA_RFInterfaceOE, 0x300, nextAntenna);
	else
		PHY_SetBBReg(priv, rFPGA0_XB_RFInterfaceOE, 0x300, nextAntenna);
#endif
	
	priv->pshare->DM_SWAT_Table.CurAntenna = nextAntenna;
	
	return TRUE;
}
#endif // SW_ANT_SWITCH

void notify_mp_ctx_background(struct rtl8192cd_priv *priv)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_MP_CTX_BACKGROUND, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_MP_CTX_BACKGROUND;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_mp_ctx_background(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	clear_bit(_CMD_MP_CTX_BACKGROUND, priv->pshare->pending_cmd);
	
//	if ((OPMODE & (WIFI_MP_STATE | WIFI_MP_CTX_BACKGROUND | WIFI_MP_CTX_BACKGROUND_STOPPING)) ==
//		(WIFI_MP_STATE | WIFI_MP_CTX_BACKGROUND) )
	mp_ctx(priv, (unsigned char *)"tx-isr");
	
	return TRUE;
}

void notify_IOT_EDCA_switch(struct rtl8192cd_priv *priv, u32 be_edca, u32 vi_edca, u16 disable_cfe)
{
	struct switch_edca_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct switch_edca_parm *)rtw_zmalloc(sizeof(struct switch_edca_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_IOT_EDCA_SWITCH;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->be_edca = be_edca;
	param->vi_edca = vi_edca;
	param->disable_txop_cfe = disable_cfe;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_IOT_EDCA_switch(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct switch_edca_parm *param = (struct switch_edca_parm *)pcmdhdr;
	
	RTL_W32(EDCA_BE_PARA, param->be_edca);
	
	if ((u32)-1 != param->vi_edca)
		RTL_W32(EDCA_VI_PARA, param->vi_edca);
	
	if ((u16)-1 != param->disable_txop_cfe) {
		if (param->disable_txop_cfe)
			RTL_W16(RD_CTRL, RTL_R16(RD_CTRL) | DIS_TXOP_CFE);
		else
			RTL_W16(RD_CTRL, RTL_R16(RD_CTRL) & ~(DIS_TXOP_CFE));
	}
	
	return TRUE;
}

void notify_check_DIG_by_rssi(struct rtl8192cd_priv *priv, u8 rssi)
{
	struct check_dig_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct check_dig_parm *)rtw_zmalloc(sizeof(struct check_dig_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_CHECK_DIG_BY_RSSI;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->rssi = rssi;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_check_DIG_by_rssi(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct check_dig_parm *param = (struct check_dig_parm *)pcmdhdr;
	
	check_DIG_by_rssi(priv, param->rssi);
	
	return TRUE;
}

void notify_set_key(struct net_device *dev, DOT11_SET_KEY *pSetKey, unsigned char *pKey)
{
	struct set_key_parm *param;
	struct cmd_header *pcmdhdr;
	
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;
#endif
	
	param = (struct set_key_parm *)rtw_zmalloc(sizeof(struct set_key_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_SET_KEY;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->dev = dev;
	memcpy(param->MACAddr, pSetKey->MACAddr, MACADDRLEN);
	param->KeyType = pSetKey->KeyType;
	param->EncType = pSetKey->EncType;
	param->KeyIndex = pSetKey->KeyIndex;
	if (pKey)
		memcpy(param->Key, pKey, sizeof(param->Key));
	else
		memcpy(param->Key, pSetKey->KeyMaterial, sizeof(param->Key));
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_set_key(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct set_key_parm *param = (struct set_key_parm *)pcmdhdr;
	DOT11_SET_KEY Set_Key;
	
	memset((char *)&Set_Key, 0, sizeof(Set_Key));
	memcpy(Set_Key.MACAddr, param->MACAddr, MACADDRLEN);
	Set_Key.KeyType = param->KeyType;
	Set_Key.EncType = param->EncType;
	Set_Key.KeyIndex = param->KeyIndex;
	
	DOT11_Process_Set_Key(param->dev, NULL, &Set_Key, param->Key);
	
	return TRUE;
}

void notify_disconnect_sta(struct net_device *dev, DOT11_DISCONNECT_REQ *pReq)
{
	struct disconnect_sta_parm *param;
	struct cmd_header *pcmdhdr;
	
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;
#endif
	
	param = (struct disconnect_sta_parm *)rtw_zmalloc(sizeof(struct disconnect_sta_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_DISCONNECT_STA;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->dev = dev;
	memcpy(&param->DisconnectReq, pReq, sizeof(DOT11_DISCONNECT_REQ));
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_disconnect_sta(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct disconnect_sta_parm *param = (struct disconnect_sta_parm *)pcmdhdr;
	struct iw_point wrq;
	
	wrq.pointer = (caddr_t)&param->DisconnectReq;
	wrq.length = sizeof(DOT11_DISCONNECT_REQ);
	
	__DOT11_Process_Disconnect_Req(param->dev, &wrq);
	
	return TRUE;
}

void notify_indicate_MIC_failure(struct net_device *dev, struct stat_info *pstat)
{
	struct indicate_mic_failure_parm *param;
	struct cmd_header *pcmdhdr;
	
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;
#endif
	
	param = (struct indicate_mic_failure_parm *)rtw_zmalloc(sizeof(struct indicate_mic_failure_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_INDICATE_MIC_FAILURE;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->dev = dev;
	if (pstat) {
		param->indicate_upper_layer = 1;
		memcpy(param->MACAddr, pstat->hwaddr, MACADDRLEN);
	} else {
		param->indicate_upper_layer = 0;
	}
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_indicate_MIC_failure(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct indicate_mic_failure_parm *param = (struct indicate_mic_failure_parm *)pcmdhdr;
	struct stat_info *pstat = NULL;
	
	if (param->indicate_upper_layer) {
		pstat = get_stainfo(priv, param->MACAddr); // indicate MIC failure whether STA exist.
	}
	
	__DOT11_Indicate_MIC_Failure(param->dev, pstat);
	
	return TRUE;
}

void notify_indicate_MIC_failure_clnt(struct rtl8192cd_priv *priv, unsigned char *sa)
{
	struct indicate_mic_failure_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct indicate_mic_failure_parm *)rtw_zmalloc(sizeof(struct indicate_mic_failure_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_INDICATE_MIC_FAILURE_CLNT;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	memcpy(param->MACAddr, sa, MACADDRLEN);
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
#ifdef WIFI_WPAS
	event_indicate_wpas(priv, sa, WPAS_MIC_FAILURE, NULL);
#endif
}

u8 callback_indicate_MIC_failure_clnt(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct indicate_mic_failure_parm *param = (struct indicate_mic_failure_parm *)pcmdhdr;
	
	__DOT11_Indicate_MIC_Failure_Clnt(priv, param->MACAddr);
	
	return TRUE;
}

void notify_recv_mgnt_frame(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	struct recv_mgnt_frame_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct recv_mgnt_frame_parm *)rtw_zmalloc(sizeof(struct recv_mgnt_frame_parm));
	if (NULL == param) {
		++priv->pshare->nr_rx_mgt_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_RECV_MGNT_FRAME;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->pfrinfo = pfrinfo;
	
	rtw_enqueue_cmd(priv, &priv->pshare->rx_mgt_queue, pcmdhdr);
}

u8 callback_recv_mgnt_frame(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct recv_mgnt_frame_parm *param = (struct recv_mgnt_frame_parm *)pcmdhdr;
	
	rtl8192cd_rx_mgntframe(priv, NULL, param->pfrinfo);
	
	return TRUE;
}

#ifdef CONFIG_RTL_WAPI_SUPPORT
void notify_recv_wai_frame(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	struct recv_mgnt_frame_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct recv_mgnt_frame_parm *)rtw_zmalloc(sizeof(struct recv_mgnt_frame_parm));
	if (NULL == param) {
		++priv->pshare->nr_rx_mgt_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_RECV_WAI_FRAME;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->pfrinfo = pfrinfo;
	
	rtw_enqueue_cmd(priv, &priv->pshare->rx_mgt_queue, pcmdhdr);
}

u8 callback_recv_wai_frame(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct recv_mgnt_frame_parm *param = (struct recv_mgnt_frame_parm *)pcmdhdr;
	struct rx_frinfo *pfrinfo = param->pfrinfo;
	unsigned char *pframe = get_pframe(pfrinfo);
	struct stat_info *pstat;

#ifdef WDS
	if (pfrinfo->to_fr_ds == 3) {
		pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
	} else
#endif
	{
#ifdef A4_STA
		if (pfrinfo->to_fr_ds == 3 &&  priv->pshare->rf_ft_var.a4_enable)
			pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
		else
#endif
	                pstat = get_stainfo(priv, pfrinfo->sa);
	}
	
	if (wapiHandleRecvPacket(pfrinfo, pstat) == FAILED) {
		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
	}
	
	return TRUE;
}
#endif // CONFIG_RTL_WAPI_SUPPORT

void notify_tx_report_change(struct rtl8192cd_priv *priv)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_TX_REPORT_CHANGE, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_TX_REPORT_CHANGE;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_tx_report_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	clear_bit(_CMD_TX_REPORT_CHANGE, priv->pshare->pending_cmd);
	
	if (0 == priv->pshare->total_assoc_num)
		RTL8188E_SuspendTxReport(priv);
	else
		RTL8188E_AssignTxReportMacId(priv);
	
	return TRUE;
}

void notify_tx_report_interval_change(struct rtl8192cd_priv *priv, u16 interval)
{
	struct cmd_header *pcmdhdr;
	
	priv->pshare->txRptTime = interval;
	
	if (test_and_set_bit(_CMD_TX_REPORT_INTERVAL_CHANGE, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_TX_REPORT_INTERVAL_CHANGE;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_tx_report_interval_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	clear_bit(_CMD_TX_REPORT_INTERVAL_CHANGE, priv->pshare->pending_cmd);
	
	RTL_W16(REG_88E_TXRPT_TIM, priv->pshare->txRptTime);
	
	return TRUE;
}

void notify_macid_pause_change(struct rtl8192cd_priv *priv, u16 macid, u16 pause)
{
	struct macid_pause_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct macid_pause_parm *)rtw_zmalloc(sizeof(struct macid_pause_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_MACID_PAUSE_CHANGE;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->macid = macid;
	param->pause = pause;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_macid_pause_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct macid_pause_parm *param = (struct macid_pause_parm *)pcmdhdr;
	
	__RTL8188E_MACID_PAUSE(priv, param->pause, param->macid);
	
	return TRUE;
}

void notify_macid_no_link_change(struct rtl8192cd_priv *priv, u16 macid, u16 nolink)
{
	struct macid_nolink_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct macid_nolink_parm *)rtw_zmalloc(sizeof(struct macid_nolink_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_MACID_NO_LINK_CHANGE;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	
	// Initialize command-specific parameter
	param->macid = macid;
	param->nolink = nolink;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_macid_no_link_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct macid_nolink_parm *param = (struct macid_nolink_parm *)pcmdhdr;
	
	__RTL8188E_MACID_NOLINK(priv, param->nolink, param->macid);
	
	return TRUE;
}

#ifdef SDIO_AP_OFFLOAD
void cmd_set_ap_offload(struct rtl8192cd_priv *priv, u8 en)
{
  struct cmd_header *pcmdhdr;
		
	if ( en ) {
	   if (test_and_set_bit(_CMD_AP_OFFLOAD, priv->pshare->pending_cmd) != 0)
		   return;
	} else {
	   if (test_and_set_bit(_CMD_AP_UNOFFLOAD, priv->pshare->pending_cmd) != 0)
		   return;
  }
  
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	if ( en )
	    pcmdhdr->cmd_code = _CMD_AP_OFFLOAD;
	else
	    pcmdhdr->cmd_code = _CMD_AP_UNOFFLOAD;
		  
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);  
}

struct ap_off_res_page ap_off_page[4];

u8 callback_ap_offload(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
#if 0	
  int bcn_off = 0xA1; // + (priv->bcn_len / 128 );
  int prob_off = bcn_off + 3 ; //(priv->bcn_len / 128 );
  int bcn_off1 = prob_off + 2;
  int prob_off1 = bcn_off1 + 3 ;
#endif

  int bcn_off   = 0xA1;

#if 0  
  int prob_off  = bcn_off + ap_off_page[0].bea_sz;
  int bcn_off1  = prob_off + ap_off_page[0].prob_sz + 1; 
  int prob_off1 = bcn_off1 + ap_off_page[1].bea_sz + 1;
#else
  int prob_off  = bcn_off + ap_off_page[0].prob_sz;
  int bcn_off1  = bcn_off + ap_off_page[1].bea_sz;
  int prob_off1 = bcn_off + ap_off_page[1].prob_sz;
#endif

  clear_bit(_CMD_AP_OFFLOAD, priv->pshare->pending_cmd);

  printk("acli: offset %x %x %x %x\n", bcn_off, prob_off, bcn_off1, prob_off1);
  /* enable hw beaconing */  
  RTL_W8(REG_MBID_NUM, RTL_R8(REG_MBID_NUM) | BIT(3) );
  
  set_wakeup_pin(priv, 1, 1, 1, 1, 7);
  delay_us(10);
  set_bcn_resv_page(priv, bcn_off, bcn_off1, 0);
  delay_us(10);
  set_probe_res_resv_page(priv, prob_off, prob_off1, 0);
  delay_us(10);
  set_ap_offload(priv, 0, 0, 1);

  // rtl8188es_set_softap_ps_cmd(priv, 1, 30);

#ifdef PLATFORM_ARM_BALONG
  extern void balong_wifi_devote(int element);
  balong_wifi_devote(1);
#endif
  
  return TRUE;    
}

u8 callback_ap_unoffload(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
   clear_bit(_CMD_AP_UNOFFLOAD, priv->pshare->pending_cmd);

   // rtl8188es_set_softap_ps_cmd(priv, 0, 0);
  
   RTL_W8(REG_MBID_NUM, RTL_R8(REG_MBID_NUM)& (~BIT(3)));
   set_ap_offload(priv, 0, 0, 0);

   RTL_W8(RXPKT_NUM+2, RTL_R8(RXPKT_NUM+2)& (~BIT(2)));

   if ( priv->pwr_state != 2 ) // RTW_STS_REP  
       priv->pwr_state = 1; // RTW_STS_NORMAL
       
   return TRUE;
}
#endif
