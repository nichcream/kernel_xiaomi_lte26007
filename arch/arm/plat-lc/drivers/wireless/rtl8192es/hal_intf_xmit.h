/*
 *  Header files defines some SDIO inline routines
 *
 *  $Id: 8188e_sdio.h,v 1.4.4.5 2010/12/10 06:11:55 button Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _HAL_INTF_XMIT_H_
#define _HAL_INTF_XMIT_H_

struct tx_insn;
#ifdef TX_SHORTCUT
struct tx_sc_entry;
#endif
//#include "./8192cd.h"

enum {
	XMIT_DECISION_CONTINUE = 0,
	XMIT_DECISION_ENQUEUE,
	XMIT_DECISION_STOP
};

#ifdef TX_SHORTCUT
int get_tx_sc_index(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri);
int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri);
#endif

u32 rtw_is_tx_queue_empty(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);
int rtw_xmit_enqueue(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);
void rtw_handle_xmit_fail(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);
int rtw_xmit_decision(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);

int dz_queue(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_insn *txcfg);
int update_txinsn_stage1(struct rtl8192cd_priv *priv, struct tx_insn* txcfg);
int update_txinsn_stage2(struct rtl8192cd_priv *priv, struct tx_insn* txcfg);
int rtl8192cd_signin_txdesc(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, struct wlan_ethhdr_t *pethhdr);
#ifdef TX_SHORTCUT
int rtl8192cd_signin_txdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct tx_sc_entry *ptxsc_entry);
#endif

#ifdef CONFIG_NETDEV_MULTI_TX_QUEUE
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv, unsigned int index);
#else
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv);
#endif
void rtl8192cd_tx_stopQueue(struct rtl8192cd_priv *priv);

void stop_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void wakeup_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void process_APSD_dz_queue(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned short tid);


#endif // _HAL_INTF_XMIT_H_

