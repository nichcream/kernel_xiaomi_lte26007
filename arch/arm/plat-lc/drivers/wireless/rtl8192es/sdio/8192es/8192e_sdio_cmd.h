/*
 *  Header files defines some SDIO CMD inline routines
 *
 *  $Id: 8192e_sdio_cmd.h,v 1.4.4.5 2010/12/10 06:11:55 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _8192E_SDIO_CMD_H_
#define _8192E_SDIO_CMD_H_

#ifdef __KERNEL__
#include <linux/kthread.h>	// for kthread_run()
#endif

#include "./osdep_service.h"

struct timer_event_entry {
	_list list;
	void (*function)(unsigned long);
	unsigned long data;
};

#define INIT_TIMER_EVENT_ENTRY(_entry, _func, _data) \
	do { \
		_rtw_init_listhead(&(_entry)->list); \
		(_entry)->data = (_data); \
		(_entry)->function = (_func); \
	} while (0)

int _rtw_init_cmd_priv(struct rtl8192cd_priv *priv);
void _rtw_free_cmd_priv(struct rtl8192cd_priv *priv);

int rtw_enqueue_timer_event(struct rtl8192cd_priv *priv, struct timer_event_entry *entry, int insert_tail);
void timer_event_timer_fn(unsigned long __data);

void rtw_flush_cmd_queue(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void rtw_flush_rx_mgt_queue(struct rtl8192cd_priv *priv);

int rtw_cmd_thread(void *context);
#ifdef REVO_MIFI
int rtw_wps_wake_thread(void *context);
#endif
void notify_update_sta_RATid(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void notify_del_sta(struct rtl8192cd_priv *priv, struct stat_info *pstat);

void notify_40M_RRSR_SC_change(struct rtl8192cd_priv *priv);
void notify_NAV_prot_len_change(struct rtl8192cd_priv *priv);
void notify_slot_time_change(struct rtl8192cd_priv *priv, int use_short);
void notify_NBI_filter_change(struct rtl8192cd_priv *priv);
#ifdef TXREPORT
void notify_request_tx_report(struct rtl8192cd_priv *priv);
#endif
#ifdef SW_ANT_SWITCH
void notify_antenna_switch(struct rtl8192cd_priv *priv, u8 nextAntenna);
#endif
void notify_mp_ctx_background(struct rtl8192cd_priv *priv);
void notify_IOT_EDCA_switch(struct rtl8192cd_priv *priv, u32 be_edca, u32 vi_edca, u16 disable_cfe);
void notify_check_DIG_by_rssi(struct rtl8192cd_priv *priv, u8 rssi);
void notify_set_key(struct net_device *dev, DOT11_SET_KEY *pSetKey, unsigned char *pKey);
void notify_disconnect_sta(struct net_device *dev, DOT11_DISCONNECT_REQ *pReq);
void notify_indicate_MIC_failure(struct net_device *dev, struct stat_info *pstat);
void notify_indicate_MIC_failure_clnt(struct rtl8192cd_priv *priv, unsigned char *sa);
void notify_recv_mgnt_frame(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo);
#ifdef CONFIG_RTL_WAPI_SUPPORT
void notify_recv_wai_frame(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo);
#endif
void notify_macid_pause_change(struct rtl8192cd_priv *priv, u16 macid, u16 pause);
void notify_update_sta_msr(struct rtl8192cd_priv *priv, struct stat_info *pstat, u8 opmode);

enum rtw_cmd_code
{
	_CMD_UPDATE_STA_RATID = 0,
	_CMD_DEL_STA,
	
	_CMD_40M_RRSR_SC_CHANGE,
	_CMD_NAV_PROT_LEN_CHANGE,
	_CMD_SLOT_TIME_CHANGE,
	_CMD_NBI_FILTER_CHANGE,
#ifdef TXREPORT
	_CMD_REQUEST_TX_REPORT,
#endif
#ifdef SW_ANT_SWITCH
	_CMD_ANTENNA_SWITCH,
#endif
	_CMD_MP_CTX_BACKGROUND,
	_CMD_IOT_EDCA_SWITCH,
	_CMD_CHECK_DIG_BY_RSSI,
	_CMD_SET_KEY,
	_CMD_DISCONNECT_STA,
	_CMD_INDICATE_MIC_FAILURE,
	_CMD_INDICATE_MIC_FAILURE_CLNT,
	_CMD_RECV_MGNT_FRAME,
#ifdef CONFIG_RTL_WAPI_SUPPORT
	_CMD_RECV_WAI_FRAME,
#endif
	_CMD_MACID_PAUSE_CHANGE,
	_CMD_UPDATE_STA_MSR,
#ifdef SDIO_AP_PS
        _CMD_AP_OFFLOAD,
        _CMD_AP_UNOFFLOAD,
#endif
	MAX_RTW_CMD_CODE
};

#endif // _8192E_SDIO_CMD_H_

