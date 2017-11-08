/*
 *  SDIO cmd handle routines
 *
 *  $Id: 8192e_sdio_cmd.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192E_SDIO_CMD_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"
#ifdef REVO_MIFI
#include "../../../../../../arch/arm/mach-balong/mmi.h"
#endif
RT_STATUS SetMACIDSleep88XX(IN  HAL_PADAPTER Adapter, IN  BOOLEAN bSleep, IN  u4Byte aid);
void UpdateHalMSRRPT88XX(IN HAL_PADAPTER Adapter, HAL_PSTAINFO pEntry, u1Byte opmode);


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

struct macid_msr_parm {
	struct cmd_header header;
	u8 opmode;
};

u8 callback_update_sta_RATid(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_del_sta(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);

u8 callback_40M_RRSR_SC_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_NAV_prot_len_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_slot_time_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_NBI_filter_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
#ifdef TXREPORT
u8 callback_request_tx_report(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
#endif
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
u8 callback_macid_pause_change(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);
u8 callback_update_sta_msr(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr);

#ifdef SDIO_AP_PS
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
#ifdef TXREPORT
	{ _CMD_REQUEST_TX_REPORT, callback_request_tx_report },
#endif
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
	{ _CMD_MACID_PAUSE_CHANGE, callback_macid_pause_change },
	{ _CMD_UPDATE_STA_MSR, callback_update_sta_msr },
#ifdef SDIO_AP_PS
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
#ifdef REVO_MIFI 
		if (&priv->pshare->beacon_timer_event == entry)
		{	
			printk("beacon evt miss\n");
			_rtw_init_listhead(&priv->pshare->beacon_timer_event.list);
			mod_timer(&priv->pshare->beacon_timer, jiffies + 
			RTL_MILISECONDS_TO_JIFFIES(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod));
		}
#endif
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
#ifdef REVO_MIFI
extern int BSP_PWRCTRL_WIFI_LowPowerExit(void);
int rtw_wps_wake_thread(void *context)
{
	struct rtl8192cd_priv *priv;
	struct priv_shared_info *pshare;
	priv = (struct rtl8192cd_priv*) context;
	pshare = priv->pshare;
	msleep(2000);
	while(1)
	{
		//wait wps long press
		printk("before wait_for_completion rtw_wps_wake_thread!\n");
		wait_for_completion(&g_wps_key_wait);
		printk("after wait_for_completion rtw_wps_wake_thread!\n");
		if ((TRUE == pshare->bDriverStopped) ||(TRUE == pshare->bSurpriseRemoved)) {
				printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
					__FUNCTION__, pshare->bDriverStopped, pshare->bSurpriseRemoved);
				goto out;
		}
		BSP_PWRCTRL_WIFI_LowPowerExit();
		kobject_uevent(&((priv->dev)->dev.kobj), KOBJ_MOVE);
		if ( priv->pshare->pwr_state == RTW_STS_SUSPEND ) {
          		priv->pshare->pwr_state = RTW_STS_NORMAL;
         		priv->pshare->ps_ctrl = RTW_ACT_IDLE;
	 		schedule_work(&priv->ap_cmd_queue);
	 	  	mod_timer(&priv->pshare->ps_timer, jiffies + POWER_DOWN_T0);
		 } 
	}
out:
	printk("exit rtw_wps_wake_thread\n");
	complete_and_exit(&pshare->wps_wake_thread_done, 0);
}
#endif

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
	
	GET_HAL_INTERFACE(priv)->UpdateHalRAMaskHandler(priv, pstat, 3);
	
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

#ifdef TXREPORT
void notify_request_tx_report(struct rtl8192cd_priv *priv)
{
	struct cmd_header *pcmdhdr;
	
	if (test_and_set_bit(_CMD_REQUEST_TX_REPORT, priv->pshare->pending_cmd) != 0)
		return;
	
	pcmdhdr = (struct cmd_header *)rtw_zmalloc(sizeof(struct cmd_header));
	if (NULL == pcmdhdr) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	pcmdhdr->cmd_code = _CMD_REQUEST_TX_REPORT;
	pcmdhdr->cmd_len = sizeof(struct cmd_header);
	pcmdhdr->priv = priv;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_request_tx_report(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	clear_bit(_CMD_REQUEST_TX_REPORT, priv->pshare->pending_cmd);
	
	requestTxReport88XX(priv);
	
	return TRUE;
}
#endif // TXREPORT

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
	
	SetMACIDSleep88XX(priv, param->pause, param->macid);
	
	return TRUE;
}

void notify_update_sta_msr(struct rtl8192cd_priv *priv, struct stat_info *pstat, u8 opmode)
{
	struct macid_msr_parm *param;
	struct cmd_header *pcmdhdr;
	
	param = (struct macid_msr_parm *)rtw_zmalloc(sizeof(struct macid_msr_parm));
	if (NULL == param) {
		++priv->pshare->nr_cmd_miss;
		DEBUG_ERR("[%s] out of memory!!\n", __FUNCTION__);
		return;
	}
	
	// Initialize command header
	pcmdhdr = &param->header;
	pcmdhdr->cmd_code = _CMD_UPDATE_STA_MSR;
	pcmdhdr->cmd_len = sizeof(*param);
	pcmdhdr->priv = priv;
	pcmdhdr->pstat = pstat;
	
	// Initialize command-specific parameter
	param->opmode = opmode;
	
	rtw_enqueue_cmd(priv, &priv->pshare->cmd_queue, pcmdhdr);
}

u8 callback_update_sta_msr(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	struct macid_msr_parm *param = (struct macid_msr_parm *)pcmdhdr;
	
	UpdateHalMSRRPT88XX(priv, pcmdhdr->pstat, param->opmode);
	
	return TRUE;
}

#ifdef SDIO_AP_PS
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
#ifdef MBSSID_OFFLOAD
#define AP_NUM RTL8192CD_NUM_VWLAN+1
#else
#define AP_NUM 2
#endif
u8 callback_ap_offload(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	unsigned char      loc_bcn[AP_NUM];
	unsigned char      loc_probe[AP_NUM];

	clear_bit(_CMD_AP_OFFLOAD, priv->pshare->pending_cmd);

        if ( priv->offload_function_ctrl == 0 )
                return TRUE;
	printk("===>callback_ap_offload!!\n");
 #ifdef MBSSID_OFFLOAD
	if(1)
 	{
 		unsigned char i=0;
		for(i=0;i<RTL8192CD_NUM_VWLAN+1;i++)
		{
			loc_bcn[i]=0xEA+4*i;
			loc_probe[i]=0xEA+4*i+2;
			printk("loc_bcn[%d]= %x loc_probe[%d]= %x\n", i,loc_bcn[i],i, loc_probe[i]);
		}
 	}
 #else
        loc_bcn[0] = 0xf6;
        loc_probe[0] = 0xf8;

        loc_bcn[1] = 0xfa;
        loc_probe[1] = 0xfc;
#endif
	
   //     printk("loc_bcn[0]= %x loc_bcn[1]= %x\n", loc_bcn[0], loc_bcn[1]);
//        printk("loc_probe[0]= %x loc_probe[1]= %x\n", loc_probe[0], loc_probe[1]);
//	RTL_W8(0x526,7);
#if 0                      //debug for download page 
	{
		u32 temp_L=0,temp_H=0,i=0,j=0;
		u16 temp=0;
		for(j=0;j<(RTL8192CD_NUM_VWLAN+1)*4;j++)
		{
			printk("tx buffer page:%d\n",0xEA+j);
			for(i=0;i<32;i++)
			{
				RTL_W8(0x106,0x69); 
				temp=(0xEA+j);
				temp=temp<<5;
				RTL_W32(0x140,(temp+i)&0x0000ffff);
				temp_L =RTL_R32(0x144);
				temp_H =RTL_R16(0x148);
				printk("0x%08x 0x%08x \n",temp_L,temp_H);			
			}
		}
		
		
	}
#endif
	if (timer_pending(&priv->expire_timer)) 
	{	
		del_timer_sync(&priv->expire_timer); 
	}
	RTL_W8(0x1c , (RTL_R8(0x1c) & (~(BIT(1)|BIT(0)))));   //zyj
	 priv->pshare->hw_seq_ctrl_backup =RTL_R8(0x423);
	 RTL_W8(0x423, 0xff);  //zyj
	 msleep(10);
        /* en, nr_ap, hidden, deny */
	if(HIDDEN_AP)
		GET_HAL_INTERFACE(priv)->SetAPOffloadHandler(priv, 1, AP_NUM, 1, 0, loc_bcn, loc_probe);
	else
		GET_HAL_INTERFACE(priv)->SetAPOffloadHandler(priv, 1, AP_NUM, 0, 0, loc_bcn, loc_probe);
#ifdef SOFTAP_PS_DURATION
//	GET_HAL_INTERFACE(priv)->SetSapPsHandler(priv, 1, 70); 
#endif
	msleep(10);
#ifdef PLATFORM_ARM_BALONG
	extern int BSP_PWRCTRL_WIFI_LowPowerEnter(void);
	BSP_PWRCTRL_WIFI_LowPowerEnter();
#endif
#ifdef CONFIG_POWER_SUSPEND
	wake_unlock(&priv->pshare->rtw_suspend_lock);
	printk("wake_unlock(&rtw_suspend_lock)\n");
#endif
        return TRUE; 
}

void handle_ap_cmd( struct work_struct *work )
{ 
	struct rtl8192cd_priv *priv = container_of(work, struct rtl8192cd_priv, ap_cmd_queue);
	struct rtl8192cd_hw	*phw = GET_HW(priv);
#ifdef CONFIG_POWER_SUSPEND
	wake_lock(&priv->pshare->rtw_suspend_lock);
#endif
	if(TRUE==priv->pshare->bDriverStopped)
		return;
	// clear_bit(_CMD_AP_OFFLOAD, priv->pshare->pending_cmd);
#ifdef SOFTAP_PS_DURATION
	GET_HAL_INTERFACE(priv)->SetSapPsHandler(priv, 0, 0);
#endif
	msleep(10);
#ifdef CONFIG_92ES_1T1R
printk("handle_ap_cmd for 1T1R\n");
RTL_W8(0x800,RTL_R8(0x800)|BIT1);
RTL_W8(0xc04,0x13);
RTL_W8(0x879,RTL_R8(0x879)&~BIT4);
RTL_W8(0x826,(RTL_R8(0x826)&~0x70)|0x30);
RTL_W8(0x878,(RTL_R8(0x878)&~0x78)|0x08);
#endif


	 RTL_W8(0x423,  priv->pshare->hw_seq_ctrl_backup);  //zyj
	 printk("handle_ap_cmd hw_seq_ctrl_backup:%x\n",priv->pshare->hw_seq_ctrl_backup);
	atomic_set(&phw->seq ,RTL_R16(0x4DA));

        GET_HAL_INTERFACE(priv)->SetAPOffloadHandler(priv, 0, 0, 0, 0, 0, 0);
        GET_ROOT(priv)->offload_function_ctrl = 0;
        RTL_W8(0x286 , (RTL_R8(0x286) & (~BIT(2))));

        if ( priv->pshare->pwr_state !=  RTW_STS_REP )
            priv->pshare->pwr_state = RTW_STS_NORMAL;	

		mod_timer(&priv->expire_timer, jiffies + EXPIRE_TO);
        printk("ap cmd wakeup\n");	

	return; 
}

u8 callback_ap_unoffload(struct rtl8192cd_priv *priv, struct cmd_header *pcmdhdr)
{
	u32 val;
	struct rtl8192cd_hw	*phw = GET_HW(priv);
	clear_bit(_CMD_AP_UNOFFLOAD, priv->pshare->pending_cmd);
	//begin add by zyj
#ifdef SOFTAP_PS_DURATION
	GET_HAL_INTERFACE(priv)->SetSapPsHandler(priv, 0, 0);
#endif
	msleep(10);
#ifdef CONFIG_92ES_1T1R
	RTL_W8(0xc04,0x13);  //  2T2R ->1T1R
#endif
	 RTL_W8(0x423,  priv->pshare->hw_seq_ctrl_backup);  //zyj
	 printk("callback_ap_unoffload hw_seq_ctrl_backup:%x\n",priv->pshare->hw_seq_ctrl_backup);
	 atomic_set(&phw->seq ,RTL_R16(0x4DA));	
	GET_HAL_INTERFACE(priv)->SetAPOffloadHandler(priv, 0, 0, 0, 0, 0, 0);
	GET_ROOT(priv)->offload_function_ctrl = 0;
	RTL_W8(0x286 , (RTL_R8(0x286) & (~BIT(2))));

	if ( priv->pshare->pwr_state !=  RTW_STS_REP )
            priv->pshare->pwr_state = RTW_STS_NORMAL;

	mod_timer(&priv->expire_timer, jiffies + EXPIRE_TO);
        printk("ap wakeup test\n");
	
        return TRUE;
}	      
#endif

