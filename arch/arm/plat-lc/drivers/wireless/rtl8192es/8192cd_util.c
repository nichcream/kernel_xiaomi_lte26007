/*
 *  Utility routines
 *
 *  $Id: 8192cd_util.c,v 1.52.2.24 2011/01/10 06:55:07 chuangsw Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_UTILS_C_

#ifdef __KERNEL__
#include <linux/circ_buf.h>
#include <linux/sched.h>
#include <linux/if_arp.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ndisc.h>
#include <linux/icmpv6.h>
#include <linux/vmalloc.h>
#elif defined(__ECOS)
#include <pkgconf/system.h>
#include <pkgconf/devs_eth_rltk_819x_wlan.h>
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_util.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"

#ifdef RTL8192CD_VARIABLE_USED_DMEM
#include "./8192cd_dmem.h"
#endif
#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
#ifdef __KERNEL__
#include <linux/if_vlan.h>
#endif
#endif
#if defined(CONFIG_RTL_FASTBRIDGE)
#include <net/rtl/features/fast_bridge.h>
#endif

#if defined(CONFIG_RTL_SIMPLE_CONFIG)
#include <linux/netdevice.h>
#include "./8192cd_profile.h"
extern int g_sc_debug;
extern unsigned char g_sc_ifname[32];
unsigned int g_sc_security_type=0;
#endif
#ifdef __ECOS
extern void rtl8192cd_beq_timer(void *task_priv);
extern void rtl8192cd_bkq_timer(void *task_priv);
extern void rtl8192cd_viq_timer(void *task_priv);
extern void rtl8192cd_voq_timer(void *task_priv);
#endif

#if defined(USE_PID_NOTIFY) && defined(LINUX_2_6_27_)
struct pid *_wlanapp_pid;
struct pid *_wlanwapi_pid;
#endif

UINT8 Realtek_OUI[]={0x00, 0xe0, 0x4c};
UINT8 dot11_rate_table[]={2,4,11,22,12,18,24,36,48,72,96,108,0}; // last element must be zero!!

#define WLAN_PKT_FORMAT_ENCAPSULATED	0x01
#define WLAN_PKT_FORMAT_CDP				0x06
#ifndef CONFIG_RTK_MESH  // mesh project moves these define to 8190n_headers.h
#define WLAN_PKT_FORMAT_SNAP_RFC1042	0x02
#define WLAN_PKT_FORMAT_SNAP_TUNNEL		0x03
#define WLAN_PKT_FORMAT_IPX_TYPE4		0x04
#define WLAN_PKT_FORMAT_APPLETALK		0x05
#define WLAN_PKT_FORMAT_OTHERS			0x07
#endif

#ifdef __ECOS
#ifndef VLAN_HLEN
#define VLAN_HLEN 4
#endif
#endif

unsigned char SNAP_ETH_TYPE_IPX[2] = {0x81, 0x37};
unsigned char SNAP_ETH_TYPE_APPLETALK_AARP[2] = {0x80, 0xf3};
unsigned char SNAP_ETH_TYPE_APPLETALK_DDP[2] = {0x80, 0x9B};
unsigned char SNAP_HDR_APPLETALK_DDP[3] = {0x08, 0x00, 0x07}; // Datagram Delivery Protocol

void mem_dump(unsigned char *ptitle, unsigned char *pbuf, int len)
{
	int i;
	if (ptitle) printk("%s", ptitle);
	for (i = 0; i < len; ++i ) {
		if (!(i & 0x0f))
			printk("\n%03X:\t", i);
		printk("%02X ", pbuf[i]);
	}
	printk("\n");
}

struct rtl_arphdr
{
	//for corss platform
    __be16          ar_hrd;         /* format of hardware address   */
    __be16          ar_pro;         /* format of protocol address   */
    unsigned char   ar_hln;         /* length of hardware address   */
    unsigned char   ar_pln;         /* length of protocol address   */
    __be16          ar_op;          /* ARP opcode (command)         */
};

#ifdef DBG_MEMORY_LEAK
#include <asm/atomic.h>
atomic_t _malloc_cnt = ATOMIC_INIT(0);
atomic_t _malloc_size = ATOMIC_INIT(0);
#endif /* DBG_MEMORY_LEAK */

#ifdef __KERNEL__
inline u8* _rtw_vmalloc(u32 sz)
{
	u8 *pbuf;

	pbuf = vmalloc(sz);

#ifdef DBG_MEMORY_LEAK
	if (pbuf != NULL) {
		atomic_inc(&_malloc_cnt);
		atomic_add(sz, &_malloc_size);
	}
#endif /* DBG_MEMORY_LEAK */

	return pbuf;
}

inline u8* _rtw_zvmalloc(u32 sz)
{
	u8 	*pbuf;

	pbuf = _rtw_vmalloc(sz);
	if (pbuf != NULL) {
		memset(pbuf, 0, sz);
	}

	return pbuf;
}

inline void _rtw_vmfree(const void *pbuf, u32 sz)
{
	if (pbuf)
	{
		vfree(pbuf);

#ifdef DBG_MEMORY_LEAK
		atomic_dec(&_malloc_cnt);
		atomic_sub(sz, &_malloc_size);
#endif /* DBG_MEMORY_LEAK */
	}
}
#endif // __KERNEL__

u8* _rtw_malloc(u32 sz)
{
	u8 *pbuf = NULL;

	pbuf = kmalloc(sz, /*GFP_KERNEL*/GFP_ATOMIC);

#ifdef DBG_MEMORY_LEAK
	if (pbuf != NULL) {
		atomic_inc(&_malloc_cnt);
		atomic_add(sz, &_malloc_size);
	}
#endif /* DBG_MEMORY_LEAK */

	return pbuf;
}

u8* _rtw_zmalloc(u32 sz)
{
	u8 *pbuf = _rtw_malloc(sz);

	if (pbuf != NULL) {
		memset(pbuf, 0, sz);
	}

	return pbuf;
}

void* rtw_malloc2d(int h, int w, int size)
{
	int j;

	void **a = (void **) rtw_zmalloc( h*sizeof(void *) + h*w*size );
	if(a == NULL)
	{
		return NULL;
	}

	for( j=0; j<h; j++ )
		a[j] = ((char *)(a+h)) + j*w*size;

	return a;
}

void rtw_mfree2d(void *pbuf, int h, int w, int size)
{
	rtw_mfree((u8 *)pbuf, h*sizeof(void*) + w*h*size);
}

void _rtw_mfree(const void *pbuf, u32 sz)
{
	if (pbuf)
	{
		kfree(pbuf);
		
#ifdef DBG_MEMORY_LEAK
		atomic_dec(&_malloc_cnt);
		atomic_sub(sz, &_malloc_size);
#endif /* DBG_MEMORY_LEAK */
	}
}

#ifndef _11s_TEST_MODE_
 static
#endif
__inline__ void release_buf_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf, struct list_head	*phead, unsigned int *count)
{
	struct list_head *plist;
#ifdef SMP_SYNC
	unsigned long flags;
#endif

	if (pbuf == NULL)
	{
		DEBUG_ERR("Release Null Buf!\n");
		return;
	}

#if 0
	if (*count >= PRE_ALLOCATED_HDR) {
		_DEBUG_ERR("over size free buf phead=%lX, *count=%d\n", (unsigned long)phead, *count);
		return;
	}
#endif

	plist = (struct list_head *)((unsigned long)pbuf - sizeof(struct list_head));

	SMP_LOCK_BUF(flags);
	
	*count = *count + 1;
	list_add_tail(plist, phead);
	
	SMP_UNLOCK_BUF(flags);
}

#ifndef _11s_TEST_MODE_
 static
#endif
__inline__ unsigned char *get_buf_from_poll(struct rtl8192cd_priv *priv, struct list_head *phead, unsigned int *count)
{
	unsigned char *buf;
	struct list_head *plist;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	if(priv)
	SMP_LOCK_BUF(flags);

	if (list_empty(phead)) {
		if(priv)
		SMP_UNLOCK_BUF(flags);
//		_DEBUG_ERR("phead=%lX buf is empty now!\n", (unsigned long)phead);
		return NULL;
	}

	if (*count == 0) {
		if(priv)
		SMP_UNLOCK_BUF(flags);
		_DEBUG_ERR("phead=%lX under-run!\n", (unsigned long)phead);
		return NULL;
	}

	*count = *count - 1;
	plist = phead->next;
	list_del_init(plist);

	if(priv)
	SMP_UNLOCK_BUF(flags);
	
	buf = (UINT8 *)((unsigned long)plist + sizeof (struct list_head));
	return buf;
}

__inline__ unsigned int orSTABitMap(STA_BITMAP *map)
{
    return (
        map->_staMap_
#if (NUM_STAT >32)
        || map->_staMap_ext_1
#if (NUM_STAT >64)		
        || map->_staMap_ext_2 || map->_staMap_ext_3
#endif
#endif
    );	
}

__inline__ unsigned int orForce20_Switch20Map(struct rtl8192cd_priv *priv)
{
    return (orSTABitMap(&priv->force_20_sta) || orSTABitMap(&priv->switch_20_sta));
}

/* return 1 or 0*/
unsigned char getSTABitMap(STA_BITMAP *map, int bitIdx)
{
    unsigned int ret;
    bitIdx--;

    if (bitIdx < 32)
        ret = map->_staMap_ & BIT(bitIdx);
#if (NUM_STAT >32)		
    else if (bitIdx <= 64)
        ret = map->_staMap_ext_1 & BIT(bitIdx - 32);
#if (NUM_STAT >64)		
    else if (bitIdx <= 96)
        ret  = map->_staMap_ext_2 & BIT(bitIdx - 64);
    else if (bitIdx <= 128)
        ret = map->_staMap_ext_3 & BIT(bitIdx -96);
#endif	
#endif	

    return (ret?1:0);
}

void setSTABitMap(STA_BITMAP *map, int bitIdx)
{
    bitIdx--;

    if (bitIdx < 32)
        map->_staMap_ |= BIT(bitIdx);
#if (NUM_STAT >32)		
    else if (bitIdx <= 64)
        map->_staMap_ext_1 |= BIT(bitIdx - 32);
#if (NUM_STAT >64)		
    else if (bitIdx <= 96)
        map->_staMap_ext_2 |= BIT(bitIdx - 64);
    else if (bitIdx <= 128)
        map->_staMap_ext_3 |= BIT(bitIdx -96);
#endif	
#endif	
}

void clearSTABitMap(STA_BITMAP* map, int bitIdx)
{
    bitIdx--;

    if (bitIdx < 32)
        map->_staMap_ &= ~ BIT(bitIdx);
#if (NUM_STAT >32)		
    else if (bitIdx < 64)
        map->_staMap_ext_1 &= ~ BIT(bitIdx - 32);
#if (NUM_STAT >64)		
    else if (bitIdx < 96)
        map->_staMap_ext_2 &= ~ BIT(bitIdx - 64);
    else if (bitIdx < 128)
        map->_staMap_ext_3 &= ~ BIT(bitIdx - 96);
#endif	
#endif	
}

#ifdef PRIV_STA_BUF
struct priv_obj_buf {
	unsigned char magic[8];
	struct list_head	list;
	struct aid_obj obj;
};

#if defined(WIFI_WMM) && defined(WMM_APSD)
struct priv_apsd_que {
	unsigned char magic[8];
	struct list_head	list;
	struct apsd_pkt_queue que;
};
#endif

#if defined(WIFI_WMM)
struct priv_dz_mgt_que {
	unsigned char magic[8];
	struct list_head	list;
	struct dz_mgmt_queue que;
};
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
struct priv_wpa_buf {
	unsigned char magic[8];
	struct list_head	list;
	WPA_STA_INFO wpa;
};
#endif

#define MAGIC_CODE_BUF			"8192"

#define MAX_PRIV_OBJ_NUM		NUM_STAT

#ifdef CONCURRENT_MODE
static struct priv_obj_buf obj_buf[NUM_WLAN_IFACE][MAX_PRIV_OBJ_NUM];
static struct list_head objbuf_list[NUM_WLAN_IFACE];
static int free_obj_buf_num[NUM_WLAN_IFACE];


#if defined(WIFI_WMM) && defined(WMM_APSD)
	#define MAX_PRIV_QUE_NUM	(MAX_PRIV_OBJ_NUM*4)
	static struct priv_apsd_que que_buf[NUM_WLAN_IFACE][MAX_PRIV_QUE_NUM];
	static struct list_head quebuf_list[NUM_WLAN_IFACE];
	static int free_que_buf_num[NUM_WLAN_IFACE];
#endif

#if defined(WIFI_WMM)
	#define MAX_MGT_QUE_NUM	(MAX_PRIV_OBJ_NUM)
	static struct priv_dz_mgt_que mgt_que_buf[NUM_WLAN_IFACE][MAX_MGT_QUE_NUM];
	static struct list_head mgt_quebuf_list[NUM_WLAN_IFACE];
	static int free_mgt_que_buf_num[NUM_WLAN_IFACE];
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	#define MAX_PRIV_WPA_NUM	MAX_PRIV_OBJ_NUM
	static struct priv_wpa_buf wpa_buf[NUM_WLAN_IFACE][MAX_PRIV_WPA_NUM];
	static struct list_head wpabuf_list[NUM_WLAN_IFACE];
	static int free_wpa_buf_num[NUM_WLAN_IFACE];
#endif

#else
static struct priv_obj_buf obj_buf[MAX_PRIV_OBJ_NUM];
static struct list_head objbuf_list;
static int free_obj_buf_num;


#if defined(WIFI_WMM) && defined(WMM_APSD)
	#define MAX_PRIV_QUE_NUM	(MAX_PRIV_OBJ_NUM*4)
	static struct priv_apsd_que que_buf[MAX_PRIV_QUE_NUM];
	static struct list_head quebuf_list;
	static int free_que_buf_num;
#endif

#if defined(WIFI_WMM)
	#define MAX_MGT_QUE_NUM	(MAX_PRIV_OBJ_NUM)
	static struct priv_dz_mgt_que mgt_que_buf[MAX_MGT_QUE_NUM];
	static struct list_head mgt_quebuf_list;
	static int free_mgt_que_buf_num;
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	#define MAX_PRIV_WPA_NUM	MAX_PRIV_OBJ_NUM
	static struct priv_wpa_buf wpa_buf[MAX_PRIV_WPA_NUM];
	static struct list_head wpabuf_list;
	static int free_wpa_buf_num;
#endif
#endif

void init_priv_sta_buf(struct rtl8192cd_priv *priv)
{
	int i;
#ifdef CONCURRENT_MODE
	int idx = priv->pshare->wlandev_idx;
	memset(&obj_buf[idx], '\0', sizeof(struct priv_obj_buf)*MAX_PRIV_OBJ_NUM);
	INIT_LIST_HEAD(&objbuf_list[idx]);
	for (i=0; i<MAX_PRIV_OBJ_NUM; i++)  {
		memcpy(obj_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&obj_buf[idx][i].list);
		list_add_tail(&obj_buf[idx][i].list, &objbuf_list[idx]);
	}
	free_obj_buf_num[idx] = i;

#if defined(WIFI_WMM) && defined(WMM_APSD)
	memset(&que_buf[idx], '\0', sizeof(struct priv_apsd_que)*MAX_PRIV_QUE_NUM);
	INIT_LIST_HEAD(&quebuf_list[idx]);
	for (i=0; i<MAX_PRIV_QUE_NUM; i++)  {
		memcpy(que_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&que_buf[idx][i].list);
		list_add_tail(&que_buf[idx][i].list, &quebuf_list[idx]);
	}
	free_que_buf_num[idx] = i;
#endif

#if defined(WIFI_WMM)
	memset(&mgt_que_buf[idx], '\0', sizeof(struct priv_dz_mgt_que)*MAX_MGT_QUE_NUM);
	INIT_LIST_HEAD(&mgt_quebuf_list[idx]);
	for (i=0; i<MAX_MGT_QUE_NUM; i++)  {
		memcpy(mgt_que_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&mgt_que_buf[idx][i].list);
		list_add_tail(&mgt_que_buf[idx][i].list, &mgt_quebuf_list[idx]);
	}
	free_mgt_que_buf_num[idx] = i;
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	memset(&wpa_buf[idx], '\0', sizeof(struct priv_wpa_buf)*MAX_PRIV_WPA_NUM);
	INIT_LIST_HEAD(&wpabuf_list[idx]);
	for (i=0; i<MAX_PRIV_WPA_NUM; i++)  {
		memcpy(wpa_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&wpa_buf[idx][i].list);
		list_add_tail(&wpa_buf[idx][i].list, &wpabuf_list[idx]);
	}
	free_wpa_buf_num[idx] = i;
#endif
#else	
	memset(obj_buf, '\0', sizeof(struct priv_obj_buf)*MAX_PRIV_OBJ_NUM);
	INIT_LIST_HEAD(&objbuf_list);
	for (i=0; i<MAX_PRIV_OBJ_NUM; i++)  {
		memcpy(obj_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&obj_buf[i].list);
		list_add_tail(&obj_buf[i].list, &objbuf_list);
	}
	free_obj_buf_num = i;

#if defined(WIFI_WMM) && defined(WMM_APSD)
	memset(que_buf, '\0', sizeof(struct priv_apsd_que)*MAX_PRIV_QUE_NUM);
	INIT_LIST_HEAD(&quebuf_list);
	for (i=0; i<MAX_PRIV_QUE_NUM; i++)  {
		memcpy(que_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&que_buf[i].list);
		list_add_tail(&que_buf[i].list, &quebuf_list);
	}
	free_que_buf_num = i;
#endif

#if defined(WIFI_WMM) 
	memset(&mgt_que_buf, '\0', sizeof(struct priv_dz_mgt_que)*MAX_MGT_QUE_NUM);
	INIT_LIST_HEAD(&mgt_quebuf_list);
	for (i=0; i<MAX_MGT_QUE_NUM; i++)  {
		memcpy(mgt_que_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&mgt_que_buf[i].list);
		list_add_tail(&mgt_que_buf[i].list, &mgt_quebuf_list);
	}
	free_mgt_que_buf_num = i;
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	memset(wpa_buf, '\0', sizeof(struct priv_wpa_buf)*MAX_PRIV_WPA_NUM);
	INIT_LIST_HEAD(&wpabuf_list);
	for (i=0; i<MAX_PRIV_WPA_NUM; i++)  {
		memcpy(wpa_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&wpa_buf[i].list);
		list_add_tail(&wpa_buf[i].list, &wpabuf_list);
	}
	free_wpa_buf_num = i;
#endif
#endif
}

struct aid_obj *alloc_sta_obj(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags=0;
#endif
	struct aid_obj *priv_obj;

    if(priv)
	    SAVE_INT_AND_CLI(flags);

#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
#ifdef CONCURRENT_MODE
	priv_obj = (struct aid_obj  *)get_buf_from_poll(priv, &objbuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_obj_buf_num[priv->pshare->wlandev_idx]);
#else	
	priv_obj = (struct aid_obj  *)get_buf_from_poll(priv, &objbuf_list, (unsigned int *)&free_obj_buf_num);
#endif
#else
#ifdef CONCURRENT_MODE
	priv_obj = (struct aid_obj  *)get_buf_from_poll(NULL, &objbuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_obj_buf_num[priv->pshare->wlandev_idx]);
#else	
	priv_obj = (struct aid_obj  *)get_buf_from_poll(NULL, &objbuf_list, (unsigned int *)&free_obj_buf_num);
#endif
#endif

   if(priv)
        RESTORE_INT(flags);

#ifdef __KERNEL__
	if (priv_obj == NULL)
		return ((struct aid_obj *)kmalloc(sizeof(struct aid_obj), GFP_ATOMIC));
	else
#endif		
		return priv_obj;
}

void free_sta_obj(struct rtl8192cd_priv *priv, struct aid_obj *obj)
{
	unsigned long offset = (unsigned long)(&((struct priv_obj_buf *)0)->obj);
	struct priv_obj_buf *priv_obj = (struct priv_obj_buf *)(((unsigned long)obj) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_obj->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_obj->obj) ==  ((unsigned long)obj)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)obj, &objbuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_obj_buf_num[priv->pshare->wlandev_idx]);
#else
		release_buf_to_poll(priv, (unsigned char *)obj, &objbuf_list, (unsigned int *)&free_obj_buf_num);
#endif
		RESTORE_INT(flags);
	}
	else
#ifdef __ECOS
		ASSERT(0);
#else
		kfree(obj);
#endif
}

#if defined(WIFI_WMM) && defined(WMM_APSD)
static struct apsd_pkt_queue *alloc_sta_que(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	struct apsd_pkt_queue *priv_que;
	SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
	priv_que = (struct apsd_pkt_queue*)get_buf_from_poll(priv, &quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_que_buf_num[priv->pshare->wlandev_idx]);
#else	
	priv_que = (struct apsd_pkt_queue*)get_buf_from_poll(priv, &quebuf_list, (unsigned int *)&free_que_buf_num);
#endif
	RESTORE_INT(flags);

#ifdef __KERNEL__
	if (priv_que == NULL)
		return ((struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC));
	else
#endif		
		return priv_que;
}

void free_sta_que(struct rtl8192cd_priv *priv, struct apsd_pkt_queue *que)
{
	unsigned long offset = (unsigned long)(&((struct priv_apsd_que *)0)->que);
	struct priv_apsd_que *priv_que = (struct priv_apsd_que *)(((unsigned long)que) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_que->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_que->que) ==  ((unsigned long)que)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)que, &quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_que_buf_num[priv->pshare->wlandev_idx]);
#else			
		release_buf_to_poll(priv, (unsigned char *)que, &quebuf_list, (unsigned int *)&free_que_buf_num);
#endif
		RESTORE_INT(flags);		
	}
	else
#ifdef __ECOS
		ASSERT(0);
#else
		kfree(que);
#endif
}
#endif // defined(WIFI_WMM) && defined(WMM_APSD)

#if defined(WIFI_WMM)
static struct dz_mgmt_queue *alloc_sta_mgt_que(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	struct dz_mgmt_queue *priv_que;

	SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
	priv_que = (struct dz_mgmt_queue*)get_buf_from_poll(priv, &mgt_quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_mgt_que_buf_num[priv->pshare->wlandev_idx]);
#else
	priv_que = (struct dz_mgmt_queue*)get_buf_from_poll(priv, &mgt_quebuf_list, (unsigned int *)&free_mgt_que_buf_num);
#endif
	RESTORE_INT(flags);

#ifdef __KERNEL__
	if (priv_que == NULL)
		return ((struct dz_mgmt_queue *)kmalloc(sizeof(struct dz_mgmt_queue), GFP_ATOMIC));
	else
#endif		
		return priv_que;
}

void free_sta_mgt_que(struct rtl8192cd_priv *priv, struct dz_mgmt_queue *que)
{
	unsigned long offset = (unsigned long)(&((struct priv_dz_mgt_que *)0)->que);
	struct priv_dz_mgt_que *priv_que = (struct priv_dz_mgt_que *)(((unsigned long)que) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_que->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_que->que) ==  ((unsigned long)que)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)que, &mgt_quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_mgt_que_buf_num[priv->pshare->wlandev_idx]);
#else
		release_buf_to_poll(priv, (unsigned char *)que, &mgt_quebuf_list, (unsigned int *)&free_mgt_que_buf_num);
#endif
		RESTORE_INT(flags);		
	}
	else
		kfree(que);
}
#endif // defined(WIFI_WMM) 


#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
static WPA_STA_INFO *alloc_wpa_buf(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	WPA_STA_INFO *priv_buf;
	SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
	priv_buf = (WPA_STA_INFO *)get_buf_from_poll(priv, &wpabuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_wpa_buf_num[priv->pshare->wlandev_idx]);
#else
	priv_buf = (WPA_STA_INFO *)get_buf_from_poll(priv, &wpabuf_list, (unsigned int *)&free_wpa_buf_num);
#endif
	RESTORE_INT(flags);

#ifdef __ECOS
	ASSERT(priv_buf != NULL);
	return priv_buf;

#else	
	if (priv_buf == NULL)
		return ((WPA_STA_INFO *)kmalloc(sizeof(WPA_STA_INFO), GFP_ATOMIC));
	else
		return priv_buf;
#endif		
}

void free_wpa_buf(struct rtl8192cd_priv *priv, WPA_STA_INFO *buf)
{
	unsigned long offset = (unsigned long)(&((struct priv_wpa_buf *)0)->wpa);
	struct priv_wpa_buf *priv_buf = (struct priv_wpa_buf *)(((unsigned long)buf) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_buf->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_buf->wpa) ==  ((unsigned long)buf)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)buf, &wpabuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_wpa_buf_num[priv->pshare->wlandev_idx]);
#else
		release_buf_to_poll(priv, (unsigned char *)buf, &wpabuf_list, (unsigned int *)&free_wpa_buf_num);
#endif
		RESTORE_INT(flags);
	}
	else
#ifdef __ECOS
		ASSERT(0);
#else	
		kfree(buf);
#endif
}
#endif // INCLUDE_WPA_PSK
#endif // PRIV_STA_BUF


#ifdef CONFIG_RTL8190_PRIV_SKB
#ifdef CONCURRENT_MODE
static struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size);
#else
static struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size);
#endif
#endif


int enque(struct rtl8192cd_priv *priv, int *head, int *tail, unsigned long ffptr, int ffsize, void *elm)
{
	// critical section!
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);
	if (CIRC_SPACE(*head, *tail, ffsize) == 0) {
		RESTORE_INT(flags);
		return FALSE;
	}

	*(unsigned long *)(ffptr + (*head)*(sizeof(void *))) = (unsigned long)elm;
	*head = (*head + 1) & (ffsize - 1);
	RESTORE_INT(flags);
	return TRUE;
}


void* deque(struct rtl8192cd_priv *priv, int *head, int *tail, unsigned long ffptr, int ffsize)
{
	// critical section!
	unsigned int  i;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	void *elm;
	
	SAVE_INT_AND_CLI(flags);
	if (CIRC_CNT(*head, *tail, ffsize) == 0) {
		RESTORE_INT(flags);
		return NULL;
	}

	i = *tail;
	*tail = (*tail + 1) & (ffsize - 1);
	elm = (void*)(*(unsigned long *)(ffptr + i*(sizeof(void *))));
	RESTORE_INT(flags);
	
	return elm;
}


void initque(struct rtl8192cd_priv *priv, int *head, int *tail)
{
	// critical section!
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);
	*head = *tail = 0;
	RESTORE_INT(flags);
}


int	isFFempty(int head, int tail)
{
	return (head == tail);
}

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
void _init_txservq(struct tx_servq *ptxservq, int q_num)
{
	_rtw_init_listhead(&ptxservq->tx_pending);
	_rtw_init_queue(&ptxservq->xframe_queue);
	ptxservq->q_num = q_num;
#ifdef CONFIG_SDIO_HCI
	ptxservq->ts_used = 0;
#endif
}

#ifdef CONFIG_TCP_ACK_TX_AGGREGATION
void _init_tcpack_servq(struct tcpack_servq *tcpackq, int q_num)
{
	_rtw_init_listhead(&tcpackq->tx_pending);
	_rtw_init_queue(&tcpackq->xframe_queue);
	tcpackq->q_num = q_num;
}
#endif
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI

#ifdef CONFIG_RTK_MESH
unsigned int find_rate_MP(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct ht_cap_elmt * peer_ht_cap,  int peer_ht_cap_len, char *peer_rate,int peer_rate_len,int mode, int isBasicRate)
{
	unsigned int len, i, hirate, lowrate, rate_limit, OFDM_only=0;
	unsigned char *rateset, *p;

	if ((get_rf_mimo_mode(priv)== MIMO_1T2R) || (get_rf_mimo_mode(priv)== MIMO_1T1R)) //eric-8814 ?? 3t3r ??
		rate_limit = 8;
	else if (get_rf_mimo_mode(priv)== MIMO_2T2R)
		rate_limit = 16;
	else if (get_rf_mimo_mode(priv)== MIMO_3T3R)
		rate_limit = 24; 
	else
		rate_limit = 16;

	if (pstat) {
		rateset = pstat->bssrateset;
		len = pstat->bssratelen;
	}
	else {
		rateset = peer_rate;
		len = peer_rate_len;
	}

	hirate = _1M_RATE_;
	lowrate = _54M_RATE_;
	if (priv->pshare->curr_band == BAND_5G)
		OFDM_only = 1;

	for(i=0,p=rateset; i<len; i++,p++)
	{
		if (*p == 0x00)
			break;

		if ((isBasicRate & 1) && !(*p & 0x80))
			continue;

		if ((isBasicRate & 2) && !is_CCK_rate(*p & 0x7f))
			continue;

		if ((*p & 0x7f) > hirate)
			if (!OFDM_only || !is_CCK_rate(*p & 0x7f))
				hirate = (*p & 0x7f);

		if ((*p & 0x7f) < lowrate)
			if (!OFDM_only || !is_CCK_rate(*p & 0x7f))
				lowrate = (*p & 0x7f);
	}

	if (pstat) {
		if ((mode == 1) && (isBasicRate == 0) && pstat->ht_cap_len) {
			for (i=0; i<rate_limit; i++)
			{
				if (pstat->ht_cap_buf.support_mcs[i/8] & BIT(i%8)) {
					hirate = i;
					hirate += HT_RATE_ID;
				}
			}
		}
	}
	else {
		if ((mode == 1) && (isBasicRate == 0) && priv->ht_cap_len && peer_ht_cap_len) {
			for (i=0; i<rate_limit; i++)
			{
				if (peer_ht_cap->support_mcs[i/8] & BIT(i%8)) {
					hirate = i;
					hirate += HT_RATE_ID;
				}
			}
		}
	}

	if (mode == 0)
		return lowrate;
	else
		return hirate;
}

#endif


// rateset: is the rateset for searching
// mode: 0: find the lowest rate, 1: find the highest rate
// isBasicRate: bit0-1: find from basic rate set, bit0-0: find from supported rate set. bit1-1: find CCK only
unsigned int find_rate(struct rtl8192cd_priv *priv, struct stat_info *pstat, int mode, int isBasicRate)
{
	unsigned int len, i, hirate, lowrate, rate_limit, OFDM_only=0;
	unsigned char *rateset, *p;
#ifdef CLIENT_MODE
	unsigned char totalrateset[32];
#endif

	if ((get_rf_mimo_mode(priv)== MIMO_1T2R) || (get_rf_mimo_mode(priv)== MIMO_1T1R)) //eric-8814 ?? 3t3r ??
		rate_limit = 8;
	else if (get_rf_mimo_mode(priv)== MIMO_2T2R)
		rate_limit = 16;
	else if (get_rf_mimo_mode(priv)== MIMO_3T3R)
		rate_limit = 24; 
	else
		rate_limit = 16;

	if (pstat) {
		rateset = pstat->bssrateset;
		len = pstat->bssratelen;
	} else {
#ifdef CLIENT_MODE
		if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->dot11Bss.supportrate) {
			int i=0;
			len = 0;
			for (i=0; dot11_rate_table[i]; i++) {
				if (priv->pmib->dot11Bss.supportrate & BIT(i)) {
					totalrateset[len] = dot11_rate_table[i];
					if (priv->pmib->dot11Bss.basicrate & BIT(i))
						totalrateset[len] |= 0x80;
					len++;
				}
			}
			rateset = totalrateset;
		} else
#endif
		{
			rateset = AP_BSSRATE;
			len = AP_BSSRATE_LEN;
		}
	}

	hirate = _1M_RATE_;
	lowrate = _54M_RATE_;
	if (priv->pshare->curr_band == BAND_5G
#if defined(RTK_5G_SUPPORT) 
		|| priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G
#endif
		)
		OFDM_only = 1;

	for(i=0,p=rateset; i<len; i++,p++)
	{
		if (*p == 0x00)
			break;

		if ((isBasicRate & 1) && !(*p & 0x80))
			continue;

		if ((isBasicRate & 2) && !is_CCK_rate(*p & 0x7f))
			continue;

		if ((*p & 0x7f) > hirate)
			if (!OFDM_only || !is_CCK_rate(*p & 0x7f))
				hirate = (*p & 0x7f);

		if ((*p & 0x7f) < lowrate)
			if (!OFDM_only || !is_CCK_rate(*p & 0x7f))
				lowrate = (*p & 0x7f);
	}

	if (pstat) {
		if ((mode == 1) && (isBasicRate == 0) && pstat->ht_cap_len && (!should_restrict_Nrate(priv, pstat))) {
			for (i=0; i<rate_limit; i++)
			{
				if (pstat->ht_cap_buf.support_mcs[i/8] & BIT(i&0x7)) {
					hirate = i;
					hirate += HT_RATE_ID;
				}
			}
		}
	}
	else {
		if ((mode == 1) && (isBasicRate == 0) && priv->ht_cap_len) {
			for (i=0; i<rate_limit; i++)
			{
				if (priv->ht_cap_buf.support_mcs[i/8] & BIT(i%8)) {
					hirate = i;
					hirate += HT_RATE_ID;
				}
			}
		}
	}

	if (mode == 0)
		return lowrate;
	else
		return hirate;
}


UINT8 get_rate_from_bit_value(int bit_val)
{
	int i;

	if (bit_val == 0)
		return 0;
	
#ifdef RTK_AC_SUPPORT 	//vht rate 
	if(bit_val & BIT(31)) {
		i = bit_val - BIT(31);

		if(i < VHT_RATE_NUM)
			return (VHT_RATE_ID + i);
		else
			return _NSS1_MCS0_RATE_; //unknown rate value 
	}
#endif

	i = 0;
	while ((bit_val & BIT(i)) == 0)
		i++;

	if (i < 12)
		return dot11_rate_table[i];
	else if (i < 28)
		return ((i - 12) + HT_RATE_ID);
	if(bit_val & BIT(28)) {
		i = bit_val - BIT(28);

		if((i+16) < HT_RATE_NUM)
			return (_MCS16_RATE_ + i);
		else
			return _MCS0_RATE_; //unknown rate value 
	}
	else
		return 0;
}


int get_rate_index_from_ieee_value(UINT8 val)
{
	int i;
	for (i=0; dot11_rate_table[i]; i++) {
		if (val == dot11_rate_table[i]) {
			return i;
		}
	}
	_DEBUG_ERR("Local error, invalid input rate for get_rate_index_from_ieee_value() [%d]!!\n", val);
	return 0;
}


int get_bit_value_from_ieee_value(UINT8 val)
{
	int i=0;
	while(dot11_rate_table[i] != 0) {
		if (dot11_rate_table[i] == val)
			return BIT(i);
		i++;
	}
	return 0;
}

bool CheckCts2SelfEnable(UINT8 rtsTxRate)
{
	return (rtsTxRate <= _11M_RATE_) ? 1 :0;
}

UINT8 find_rts_rate(struct rtl8192cd_priv *priv, UINT8 TxRate, bool bErpProtect)
{
	UINT8 rtsTxRate = _6M_RATE_;

	if(bErpProtect) // use CCK rate as RTS
	{
		rtsTxRate = _1M_RATE_;
	}
	else
	{
		switch (TxRate) 
		{
			case _NSS2_MCS9_RATE_:
			case _NSS2_MCS8_RATE_:
			case _NSS2_MCS7_RATE_:
			case _NSS2_MCS6_RATE_:
			case _NSS2_MCS5_RATE_:
			case _NSS2_MCS4_RATE_:
			case _NSS2_MCS3_RATE_:
			case _NSS1_MCS9_RATE_:
			case _NSS1_MCS8_RATE_:
			case _NSS1_MCS7_RATE_:
			case _NSS1_MCS6_RATE_:
			case _NSS1_MCS5_RATE_:
			case _NSS1_MCS4_RATE_:
			case _NSS1_MCS3_RATE_:
			case _MCS15_RATE_:
			case _MCS14_RATE_:
			case _MCS13_RATE_:
			case _MCS12_RATE_:
			case _MCS11_RATE_:
			case _MCS7_RATE_:
			case _MCS6_RATE_:
			case _MCS5_RATE_:
			case _MCS4_RATE_:
			case _MCS3_RATE_:
			case _54M_RATE_:
			case _48M_RATE_:
			case _36M_RATE_:
			case _24M_RATE_:		
				rtsTxRate = _24M_RATE_;
				break;
			case _NSS2_MCS2_RATE_:
			case _NSS2_MCS1_RATE_:
			case _NSS1_MCS2_RATE_:
			case _NSS1_MCS1_RATE_:
			case _MCS10_RATE_:
			case _MCS9_RATE_:
			case _MCS2_RATE_:
			case _MCS1_RATE_:
			case _18M_RATE_:
			case _12M_RATE_:
				rtsTxRate = _12M_RATE_;
				break;
			case _NSS2_MCS0_RATE_:
			case _NSS1_MCS0_RATE_:
			case _MCS8_RATE_:
			case _MCS0_RATE_:
			case _9M_RATE_:
			case _6M_RATE_:
				rtsTxRate = _6M_RATE_;
				break;
			case _11M_RATE_:
			case _5M_RATE_:
			case _2M_RATE_:
			case _1M_RATE_:
				rtsTxRate = _1M_RATE_;
				break;
			default:
				rtsTxRate = _6M_RATE_;
				break;
		}
	}

	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
	           if(rtsTxRate < _6M_RATE_)
	                     rtsTxRate = _6M_RATE_;
	}

	return rtsTxRate;
}

void init_stainfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct wifi_mib	*pmib = priv->pmib;
	unsigned long	offset;
	int i, j;
#if !defined(SMP_SYNC) && defined(CONFIG_RTL_WAPI_SUPPORT)
	unsigned long		flags;
#endif

#ifdef WDS
	static unsigned char bssrateset[32];
	unsigned int	bssratelen=0;
	unsigned int	current_tx_rate=0;
#endif
	unsigned short	bk_aid;
	unsigned char		bk_hwaddr[MACADDRLEN];


	// init linked list header
	// BUT do NOT init hash_list
	INIT_LIST_HEAD(&pstat->asoc_list);
	INIT_LIST_HEAD(&pstat->auth_list);
	INIT_LIST_HEAD(&pstat->sleep_list);
	INIT_LIST_HEAD(&pstat->defrag_list);
	INIT_LIST_HEAD(&pstat->wakeup_list);
	INIT_LIST_HEAD(&pstat->frag_list);

#ifdef CONFIG_PCI_HCI
	// to avoid add RAtid fail
	INIT_LIST_HEAD(&pstat->addRAtid_list);
	INIT_LIST_HEAD(&pstat->addrssi_list);
#endif

#ifdef CONFIG_RTK_MESH
	INIT_LIST_HEAD(&pstat->mesh_mp_ptr);
#endif	// CONFIG_RTK_MESH

#ifdef A4_STA
	INIT_LIST_HEAD(&pstat->a4_sta_list);
#endif
#if defined(CONFIG_PCI_HCI)
	skb_queue_head_init(&pstat->dz_queue);
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	INIT_LIST_HEAD(&pstat->pspoll_list);

	for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
		_init_txservq(&pstat->tx_queue[i], i);
#ifdef CONFIG_TCP_ACK_TX_AGGREGATION
		_init_tcpack_servq(&pstat->tcpack_queue[i], i);
#endif
	}
	
	pstat->pending_cmd = 0;
	pstat->asoc_list_refcnt = 0;
#ifdef __ECOS
	cyg_flag_init(&pstat->asoc_unref_done);
#else
	init_completion(&pstat->asoc_unref_done);
#endif
#endif

#ifdef SW_TX_QUEUE
	init_timer(&pstat->swq.beq_timer);
    init_timer(&pstat->swq.bkq_timer);
    init_timer(&pstat->swq.viq_timer);
    init_timer(&pstat->swq.voq_timer);

	skb_queue_head_init(&pstat->swq.be_queue);
    skb_queue_head_init(&pstat->swq.bk_queue);
    skb_queue_head_init(&pstat->swq.vi_queue);
	skb_queue_head_init(&pstat->swq.vo_queue);
	for(i=BK_QUEUE;i<HIGH_QUEUE;i++) {	
		pstat->swq.q_aggnum[i] = 1;	
	}
#endif

	// we do NOT reset MAC here

#if defined(WIFI_WMM)
#ifdef DZ_ADDBA_RSP
	pstat->dz_addba.used = 0;
#endif
#endif

#ifdef WDS
	if (pstat->state & WIFI_WDS) {
		bssratelen = pstat->bssratelen;
		memcpy(bssrateset, pstat->bssrateset, bssratelen);
		current_tx_rate = pstat->current_tx_rate;
	}
#endif

	// zero out all the rest
	bk_aid = pstat->aid;
	memcpy(bk_hwaddr, pstat->hwaddr, MACADDRLEN);

	offset = (unsigned long)(&((struct stat_info *)0)->auth_seq);
	memset((void *)((unsigned long)pstat + offset), 0, sizeof(struct stat_info)-offset);
	
	pstat->aid = bk_aid;
	memcpy(pstat->hwaddr, bk_hwaddr, MACADDRLEN);

#ifdef WDS
	if (bssratelen) {
		pstat->bssratelen = bssratelen;
		memcpy(pstat->bssrateset, bssrateset, bssratelen);
		pstat->current_tx_rate = current_tx_rate;
		pstat->state |= WIFI_WDS;
	}
#endif

	// some variables need initial value
	pstat->ieee8021x_ctrlport = pmib->dot118021xAuthEntry.dot118021xDefaultPort;
	pstat->expire_to = priv->expire_to;
	pstat->idle_count = 0;
	for (i=0; i<8; i++)
		for (j=0; j<TUPLE_WINDOW; j++)
			pstat->tpcache[i][j] = 0xffff;
			// Stanldy mesh: pstat->tpcache[i][j] = j+1 is best solution, because its a hash table, fill slot[i] with i+1 can prevent collision,fix the packet loss of first unicast
	pstat->tpcache_mgt = 0xffff;
#ifdef CLIENT_MODE
	pstat->tpcache_mcast = 0xffff;
#endif
#ifdef GBWC
	for (i=0; i<priv->pmib->gbwcEntry.GBWCNum; i++) {
		if (!memcmp(pstat->hwaddr, priv->pmib->gbwcEntry.GBWCAddr[i], MACADDRLEN)) {
			pstat->GBWC_in_group = TRUE;
			break;
		}
	}
#endif

// button 2009.05.21
#ifdef INCLUDE_WPA_PSK
	pstat->wpa_sta_info->clientHndshkProcessing = pstat->wpa_sta_info->clientHndshkDone = FALSE;
#endif

#ifdef CONFIG_RTK_MESH
	pstat->mesh_neighbor_TBL.BSexpire_LLSAperiod = jiffies + MESH_EXPIRE_TO;
#endif	//CONFIG_RTK_MESH

	memset(pstat->rc_entry, 0, sizeof(pstat->rc_entry));

#ifdef SUPPORT_TX_AMSDU
	for (i=0; i<8; i++)
		skb_queue_head_init(&pstat->amsdu_tx_que[i]);
#endif

#if defined (HW_ANT_SWITCH) && (defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT))
	pstat->CurAntenna = priv->pshare->rf_ft_var.CurAntenna;
#endif

#ifdef CONFIG_RTL_WAPI_SUPPORT
//	if (priv->pmib->wapiInfo.wapiType!=wapiDisable)
	{
		SAVE_INT_AND_CLI(flags);
//		wapiAssert(pstat->wapiInfo!=NULL);
		if (pstat->wapiInfo==NULL)
		{
			pstat->wapiInfo = (wapiStaInfo*)kmalloc(sizeof(wapiStaInfo), GFP_ATOMIC);
			if (pstat->wapiInfo==NULL)
			{
				printk("Err: kmalloc wapiStaInfo fail!\n");
			}
		}
		if (pstat->wapiInfo!=NULL)
		{
			pstat->wapiInfo->priv = priv;
			wapiStationInit(pstat);
			pstat->wapiInfo->wapiType = priv->pmib->wapiInfo.wapiType;
		}
		RESTORE_INT(flags);
	}
#endif

#ifdef USE_OUT_SRC		
#ifdef _OUTSRC_COEXIST
	if(IS_OUTSRC_CHIP(priv))
#endif
	ODM_CmnInfoPtrArrayHook(ODMPTR , ODM_CMNINFO_STA_STATUS, pstat->aid, pstat);
#endif

#ifdef MULTI_MAC_CLONE
	if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable) 
	{
		pstat->mclone_id = ACTIVE_ID;
		memcpy(pstat->sa_addr, GET_MY_HWADDR, MACADDRLEN);		
	}
#endif
}

#if defined(CONFIG_RTL_ETH_PRIV_SKB_DEBUG)
void dump_sta_dz_queue_num(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#if defined(WIFI_WMM) && defined(WMM_APSD)
	int				hd, tl;
#endif

	// free all skb in dz_queue
	
	printk("---------------------------------------\n");
#if defined(CONFIG_PCI_HCI)
	printk("pstat->dz_queue:%d\n",skb_queue_len(&pstat->dz_queue));
#endif

#ifdef SW_TX_QUEUE
	printk("swq.be_queue:%d\n",skb_queue_len(&pstat->swq.be_queue));
	printk("swq.bk_queue:%d\n",skb_queue_len(&pstat->swq.bk_queue));
	printk("swq.vi_queue:%d\n",skb_queue_len(&pstat->swq.vi_queue));
	printk("swq.vo_queue:%d\n",skb_queue_len(&pstat->swq.vo_queue));
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
	hd = pstat->VO_dz_queue->head;
	tl = pstat->VO_dz_queue->tail;
	printk("VO_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
	hd = pstat->VI_dz_queue->head;
	tl = pstat->VI_dz_queue->tail;
	printk("VI_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
	hd = pstat->BE_dz_queue->head;
	tl = pstat->BE_dz_queue->tail;
	printk("BE_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
	hd = pstat->BK_dz_queue->head;
	tl = pstat->BK_dz_queue->tail;
	printk("BK_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
#endif

#if defined(WIFI_WMM)
	hd = pstat->MGT_dz_queue->head;
	tl = pstat->MGT_dz_queue->tail;
	printk("BK_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_DZ_MGT_QUEUE));
#endif
	
	return;

}
#endif

#ifdef CONFIG_PCI_HCI
void free_sta_tx_skb(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef WIFI_WMM
	int				hd, tl;
#endif
	struct sk_buff	*pskb;

	// free all skb in dz_queue
	while (skb_queue_len(&pstat->dz_queue)) {
		pskb = skb_dequeue(&pstat->dz_queue);
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
	}

#ifdef SW_TX_QUEUE
        while (skb_queue_len(&pstat->swq.be_queue)) {
                pskb = skb_dequeue(&pstat->swq.be_queue);
                rtl_kfree_skb(priv, pskb, _SKB_TX_);
        }
	while (skb_queue_len(&pstat->swq.bk_queue)) {
                pskb = skb_dequeue(&pstat->swq.bk_queue);
                rtl_kfree_skb(priv, pskb, _SKB_TX_);
        }
	while (skb_queue_len(&pstat->swq.vi_queue)) {
                pskb = skb_dequeue(&pstat->swq.vi_queue);
                rtl_kfree_skb(priv, pskb, _SKB_TX_);
        }
	while (skb_queue_len(&pstat->swq.vo_queue)) {
                pskb = skb_dequeue(&pstat->swq.vo_queue);
                rtl_kfree_skb(priv, pskb, _SKB_TX_);
        }
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
	hd = pstat->VO_dz_queue->head;
	tl = pstat->VO_dz_queue->tail;
	while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
		pskb = pstat->VO_dz_queue->pSkb[tl];
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
		tl++;
		tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
	}
	pstat->VO_dz_queue->head = 0;
	pstat->VO_dz_queue->tail = 0;

	hd = pstat->VI_dz_queue->head;
	tl = pstat->VI_dz_queue->tail;
	while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
		pskb = pstat->VI_dz_queue->pSkb[tl];
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
		tl++;
		tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
	}
	pstat->VI_dz_queue->head = 0;
	pstat->VI_dz_queue->tail = 0;

	hd = pstat->BE_dz_queue->head;
	tl = pstat->BE_dz_queue->tail;
	while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
		pskb = pstat->BE_dz_queue->pSkb[tl];
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
		tl++;
		tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
	}
	pstat->BE_dz_queue->head = 0;
	pstat->BE_dz_queue->tail = 0;

	hd = pstat->BK_dz_queue->head;
	tl = pstat->BK_dz_queue->tail;
	while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
		pskb = pstat->BK_dz_queue->pSkb[tl];
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
		tl++;
		tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
	}
	pstat->BK_dz_queue->head = 0;
	pstat->BK_dz_queue->tail = 0;
#endif
#if defined(WIFI_WMM)
	hd = pstat->MGT_dz_queue->head;
	tl = pstat->MGT_dz_queue->tail;
	while (CIRC_CNT(hd, tl, NUM_DZ_MGT_QUEUE)) {
		struct tx_insn *ptx_insn = pstat->MGT_dz_queue->ptx_insn[tl];
		release_mgtbuf_to_poll(priv, ptx_insn->pframe);
		release_wlanhdr_to_poll(priv, ptx_insn->phdr);
		kfree(ptx_insn);
		tl++;
		tl = tl & (NUM_DZ_MGT_QUEUE - 1);
	}
	pstat->MGT_dz_queue->head = 0;
	pstat->MGT_dz_queue->tail = 0;

	
#ifdef DZ_ADDBA_RSP
	pstat->dz_addba.used = 0;
#endif
#endif
}
#endif // CONFIG_PCI_HCI

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
void free_sta_tx_skb(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i;
	for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
#ifdef CONFIG_TCP_ACK_TX_AGGREGATION
		rtw_tcpack_servq_flush(priv, &pstat->tcpack_queue[i]);
#endif
		rtw_txservq_flush(priv, &pstat->tx_queue[i]);
	}
}
#endif

void free_sta_skb(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int	 i, j;
	struct list_head frag_list;
	struct sk_buff	*pskb;
	unsigned long flags;
	
	free_sta_tx_skb(priv,pstat);
	
	// free all skb in frag_list
	INIT_LIST_HEAD(&frag_list);

	DEFRAG_LOCK(flags);
	list_splice_init(&pstat->frag_list, &frag_list);
	DEFRAG_UNLOCK(flags);
	
	unchainned_all_frag(priv, &frag_list);

	// free all skb in rc queue
	SMP_LOCK_REORDER_CTRL(flags);
	for (i=0; i<8; i++) {
		pstat->rc_entry[i].start_rcv = FALSE;
		for (j=0; j<128; j++) {
			if (pstat->rc_entry[i].packet_q[j]) {
				pskb = pstat->rc_entry[i].packet_q[j];
				rtl_kfree_skb(priv, pskb, _SKB_RX_);
				pstat->rc_entry[i].packet_q[j] = NULL;
			}
		}
	}
	SMP_UNLOCK_REORDER_CTRL(flags);
}


void release_stainfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int				i;
	unsigned long		flags;

	if (priv->pshare->is_40m_bw && (pstat->IOTPeer == HT_IOT_PEER_MARVELL))

	{
	    clearSTABitMap(&priv->pshare->marvellMapBit, pstat->aid);

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8881A)
		if((GET_CHIP_VER(priv)==VERSION_8812E)||(GET_CHIP_VER(priv)==VERSION_8881A)){
		}
        else
#endif
		if ((orSTABitMap(&priv->pshare->marvellMapBit) == 0) &&
			 (priv->pshare->Reg_RRSR_2 != 0) && (priv->pshare->Reg_81b != 0))
		{
#if defined(CONFIG_PCI_HCI)
			RTL_W8(RRSR+2, priv->pshare->Reg_RRSR_2);
			RTL_W8(0x81b, priv->pshare->Reg_81b);
			priv->pshare->Reg_RRSR_2 = 0;
			priv->pshare->Reg_81b = 0;
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			notify_40M_RRSR_SC_change(priv);
#endif
		}
	}

	update_intel_sta_bitmap(priv, pstat, 1);
#if defined(WIFI_11N_2040_COEXIST_EXT)
	update_40m_staMap(priv, pstat, 1);
#endif

	// flush the stainfo cache
	//if (!memcmp(pstat->hwaddr, priv->stainfo_cache.hwaddr, MACADDRLEN))
	//	memset(&(priv->stainfo_cache), 0, sizeof(priv->stainfo_cache));
	if (pstat == priv->pstat_cache)
		priv->pstat_cache = NULL;

	// free all queued skb
	free_sta_skb(priv, pstat);

	// delete all list
	// BUT do NOT delete hash list
	asoc_list_del(priv, pstat);
	auth_list_del(priv, pstat);
	sleep_list_del(priv, pstat);
	wakeup_list_del(priv, pstat);

	DEFRAG_LOCK(flags);
	if (!list_empty(&(pstat->defrag_list)))
		list_del_init(&(pstat->defrag_list));
	DEFRAG_UNLOCK(flags);

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	rtw_pspoll_sta_delete(priv, pstat);
#endif

#ifdef CONFIG_PCI_HCI
	// to avoid add RAtid fail
	if (!list_empty(&(pstat->addRAtid_list)))
		list_del_init(&(pstat->addRAtid_list));

	if (!list_empty(&(pstat->addrssi_list)))
		list_del_init(&(pstat->addrssi_list));
#endif

#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
	#if defined(CONFIG_USB_HCI)
	rtw_flush_h2c_cmd_queue(priv, pstat);
	#endif
#elif defined(CONFIG_RTL_88E_SUPPORT)
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	rtw_flush_cmd_queue(priv, pstat);
	#endif
#endif

#ifdef SW_TX_QUEUE
#ifdef SMP_SYNC		
	int locked=0;
	SMP_TRY_LOCK_XMIT(flags,locked);
#endif
	if (timer_pending(&pstat->swq.beq_timer))
                del_timer(&pstat->swq.beq_timer);
	if (timer_pending(&pstat->swq.bkq_timer))
                del_timer(&pstat->swq.bkq_timer);
	if (timer_pending(&pstat->swq.viq_timer))
                del_timer(&pstat->swq.viq_timer);
	if (timer_pending(&pstat->swq.voq_timer))
                del_timer(&pstat->swq.voq_timer);
#ifdef SMP_SYNC				
	if(locked)
		SMP_UNLOCK_XMIT(flags);
#endif

#endif

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
		_rtw_spinlock_free(&(pstat->tx_queue[i].xframe_queue.lock));
#ifdef CONFIG_TCP_ACK_TX_AGGREGATION
		_rtw_spinlock_free(&(pstat->tcpack_queue[i].xframe_queue.lock));
#endif
	}
#endif

#ifdef CONFIG_RTK_MESH
	SMP_LOCK_MESH_MP_HDR(flags);
	if (!list_empty(&(pstat->mesh_mp_ptr)))
		list_del_init(&(pstat->mesh_mp_ptr));
	SMP_UNLOCK_MESH_MP_HDR(flags);

	pstat->mesh_neighbor_TBL.State = MP_UNUSED;	// reset state (clean is high priority)

//yschen 2009-03-04
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID) // 1.Proxy_table in root interface NOW!! 2.Spare for Mesh work with Multiple AP (Please see Mantis 0000107 for detail)
    if(IS_ROOT_INTERFACE(priv))
#endif
	{
        remove_proxy_table(priv, pstat->hwaddr);
        clear_route_info(priv, pstat->hwaddr); 
	}
#endif

#ifdef A4_STA
	if (!list_empty(&pstat->a4_sta_list))
		list_del_init(&pstat->a4_sta_list);	
#endif

	// remove key in CAM
	if (pstat->dot11KeyMapping.keyInCam == TRUE) {
		if (GET_ROOT(priv)->drv_state & DRV_STATE_OPEN) {
			if (CamDeleteOneEntry(priv, pstat->hwaddr, 0, 0)) {
				pstat->dot11KeyMapping.keyInCam = FALSE;
				priv->pshare->CamEntryOccupied--;
			}
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
			/*	for wapi, one state take two cam entry	*/
			if (CamDeleteOneEntry(priv, pstat->hwaddr, 0, 0)) {
				pstat->dot11KeyMapping.keyInCam = FALSE;
				priv->pshare->CamEntryOccupied--;
			}
#endif
		}
	}

	SMP_LOCK_REORDER_CTRL(flags);
	for (i=0; i<RC_TIMER_NUM; i++)
		if (priv->pshare->rc_timer[i].pstat == pstat)
			priv->pshare->rc_timer[i].pstat = NULL;
	SMP_UNLOCK_REORDER_CTRL(flags);

#ifdef WDS
	pstat->state &= WIFI_WDS;
#else
	pstat->state = 0;
#endif

#ifdef TX_SHORTCUT
	memset(pstat->tx_sc_ent, 0, sizeof(pstat->tx_sc_ent));
#endif

#ifdef RX_SHORTCUT
	for (i=0; i<RX_SC_ENTRY_NUM; i++)
		pstat->rx_sc_ent[i].rx_payload_offset = 0;
#endif

#ifdef INCLUDE_WPA_PSK
	if (timer_pending(&pstat->wpa_sta_info->resendTimer)) {
		del_timer(&pstat->wpa_sta_info->resendTimer);
	}
#endif

	release_remapAid(priv, pstat);

#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
	if(IS_OUTSRC_CHIP(priv))
#endif
	ODM_CmnInfoPtrArrayHook(ODMPTR , ODM_CMNINFO_STA_STATUS, pstat->aid, 0);
#endif
#ifdef SUPPORT_TX_AMSDU
	for (i=0; i<8; i++)
		free_skb_queue(priv, &pstat->amsdu_tx_que[i]);
#endif

#ifdef BEAMFORMING_SUPPORT
	if (priv->pmib->dot11RFEntry.txbf == 1 && (GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8192E) )
		Beamforming_DeInitEntry(priv, pstat->hwaddr);
#endif
#if 1
#if defined(BR_SHORTCUT) && defined(RTL_CACHED_BR_STA)
	release_brsc_cache(pstat->hwaddr);
#endif
#if defined(CONFIG_RTL_FASTBRIDGE)
		rtl_fb_del_entry(pstat->hwaddr);
#endif
#else
#ifdef BR_SHORTCUT
	clear_shortcut_cache();
#endif
#endif
#ifdef CONFIG_RTL_WAPI_SUPPORT
	free_sta_wapiInfo(priv, pstat);
#endif
}


struct	stat_info *alloc_stainfo(struct rtl8192cd_priv *priv, unsigned char *hwaddr, int id)
{
	unsigned long	flags;
	unsigned int	i,index;
	struct list_head	*phead, *plist;
	struct stat_info	*pstat;

	SAVE_INT_AND_CLI(flags);

	if (id < 0) { // not from FAST_RECOVERY
	// any free sta info?
		for(i=0; i<NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == FALSE))
			{
				priv->pshare->aidarray[i]->priv = priv;
				priv->pshare->aidarray[i]->used = TRUE;
				pstat = &(priv->pshare->aidarray[i]->station);
				memcpy(pstat->hwaddr, hwaddr, MACADDRLEN);
				init_stainfo(priv, pstat);
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
				if (GET_CHIP_VER(priv)==VERSION_8188E) {
#ifdef RATEADAPTIVE_BY_ODM
					ODM_RAInfo_Init(ODMPTR, pstat->aid);
#else
					priv->pshare->RaInfo[pstat->aid].pstat = pstat;			
					RateAdaptiveInfoInit(&priv->pshare->RaInfo[pstat->aid]);
#endif			
				}
#endif

				// insert to hash list
				hash_list_add(priv, pstat);

				RESTORE_INT(flags);
				return pstat;
			}
		}

		// allocate new sta info
		for(i=0; i<NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] == NULL)
				break;
		}
	}
	else
		i = id;

	if (i < NUM_STAT) {
#ifdef RTL8192CD_VARIABLE_USED_DMEM
			priv->pshare->aidarray[i] = (struct aid_obj *)rtl8192cd_dmem_alloc(AID_OBJ, &i);
#else
#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i] = alloc_sta_obj(priv);
#else
			priv->pshare->aidarray[i] = (struct aid_obj *)kmalloc(sizeof(struct aid_obj), GFP_ATOMIC);
#endif
#endif
			if (priv->pshare->aidarray[i] == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i], 0, sizeof(struct aid_obj));

#ifdef CONFIG_PCI_HCI
#if defined(WIFI_WMM) && defined(WMM_APSD)
#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i]->station.VO_dz_queue = alloc_sta_que(priv);
#else
			priv->pshare->aidarray[i]->station.VO_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
			if (priv->pshare->aidarray[i]->station.VO_dz_queue == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i]->station.VO_dz_queue, 0, sizeof(struct apsd_pkt_queue));

#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i]->station.VI_dz_queue = alloc_sta_que(priv);
#else
			priv->pshare->aidarray[i]->station.VI_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
			if (priv->pshare->aidarray[i]->station.VI_dz_queue == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i]->station.VI_dz_queue, 0, sizeof(struct apsd_pkt_queue));

#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i]->station.BE_dz_queue = alloc_sta_que(priv);
#else
			priv->pshare->aidarray[i]->station.BE_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
			if (priv->pshare->aidarray[i]->station.BE_dz_queue == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i]->station.BE_dz_queue, 0, sizeof(struct apsd_pkt_queue));

#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i]->station.BK_dz_queue = alloc_sta_que(priv);
#else
			priv->pshare->aidarray[i]->station.BK_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
			if (priv->pshare->aidarray[i]->station.BK_dz_queue == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i]->station.BK_dz_queue, 0, sizeof(struct apsd_pkt_queue));
#endif

#if defined(WIFI_WMM)
#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i]->station.MGT_dz_queue = alloc_sta_mgt_que(priv);
#else
			priv->pshare->aidarray[i]->station.MGT_dz_queue = (struct dz_mgmt_queue *)kmalloc(sizeof(struct dz_mgmt_queue), GFP_ATOMIC);
#endif
			if (priv->pshare->aidarray[i]->station.MGT_dz_queue == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i]->station.MGT_dz_queue, 0, sizeof(struct dz_mgmt_queue));
#endif
#endif // CONFIG_PCI_HCI

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i]->station.wpa_sta_info = alloc_wpa_buf(priv);
#else
			priv->pshare->aidarray[i]->station.wpa_sta_info = (WPA_STA_INFO *)kmalloc(sizeof(WPA_STA_INFO), GFP_ATOMIC);
#endif
			if (priv->pshare->aidarray[i]->station.wpa_sta_info == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i]->station.wpa_sta_info, 0, sizeof(WPA_STA_INFO));
#endif

#if defined(WIFI_HAPD) || defined(RTK_NL80211) || defined (WIFI_WPAS)
			memset(priv->pshare->aidarray[i]->station.wpa_ie, 0, 256);
#ifndef HAPD_DRV_PSK_WPS
			memset(priv->pshare->aidarray[i]->station.wps_ie, 0, 256);
#endif
#endif

			priv->pshare->aidarray[i]->priv = priv;
			INIT_LIST_HEAD(&(priv->pshare->aidarray[i]->station.hash_list));
			priv->pshare->aidarray[i]->station.aid = i + 1; //aid 0 is reserved for AP
			priv->pshare->aidarray[i]->used = TRUE;
			pstat = &(priv->pshare->aidarray[i]->station);
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			if (GET_CHIP_VER(priv)==VERSION_8188E) {

#ifdef RATEADAPTIVE_BY_ODM
				ODM_RAInfo_Init(ODMPTR, pstat->aid);
#else
				priv->pshare->RaInfo[i + 1].pstat = pstat;
				RateAdaptiveInfoInit(&priv->pshare->RaInfo[i + 1]);
#endif
			}
#endif

#if defined(CONFIG_RTL_WAPI_SUPPORT)
			{
				wapiAssert(pstat->wapiInfo==NULL);
				if (pstat->wapiInfo==NULL)
				{
					pstat->wapiInfo = (wapiStaInfo*)kmalloc(sizeof(wapiStaInfo), GFP_ATOMIC);
					if (pstat->wapiInfo==NULL)
					{
						goto no_free_memory;
					}
				}
			}
#endif
			memcpy(pstat->hwaddr, hwaddr, MACADDRLEN);
			init_stainfo(priv, pstat);

			// insert to hash list
			hash_list_add(priv, pstat);

			RESTORE_INT(flags);
			return pstat;
	}

	// no more free sta info, check idle sta
	SMP_LOCK_ASOC_LIST(flags);
	
	phead = &priv->asoc_list;
	plist = phead->next;

	while(plist != phead)
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);
		if ((pstat->expire_to == 0)
#ifdef WDS
#ifdef LAZY_WDS
			&& ((pstat->state & WIFI_WDS_LAZY) ||
				(!(pstat->state & WIFI_WDS_LAZY) && !(pstat->state & WIFI_WDS)))
#else
			&& !(pstat->state & WIFI_WDS)
#endif
#endif
		)
		{
			SMP_UNLOCK_ASOC_LIST(flags);
			
			i = pstat->aid - 1;
			release_stainfo(priv, pstat);
			hash_list_del(priv, pstat);

			priv->pshare->aidarray[i]->used = TRUE;
			priv->pshare->aidarray[i]->priv = priv;
			memcpy(pstat->hwaddr, hwaddr, MACADDRLEN);
			init_stainfo(priv, pstat);
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			if (GET_CHIP_VER(priv)==VERSION_8188E)
#ifdef RATEADAPTIVE_BY_ODM
				ODM_RAInfo_Init(ODMPTR, pstat->aid);
#else
				RateAdaptiveInfoInit(&priv->pshare->RaInfo[pstat->aid]);
#endif
#endif
			// insert to hash list
			hash_list_add(priv, pstat);

			RESTORE_INT(flags);
			return pstat;
		}
		else
			plist = plist->next;
	}

	SMP_UNLOCK_ASOC_LIST(flags);
	RESTORE_INT(flags);
	DEBUG_ERR("AID buf is not enough\n");
	return	(struct stat_info *)NULL;

no_free_memory:

	if (priv->pshare->aidarray[i]) {
#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
		if (priv->pshare->aidarray[i]->station.wpa_sta_info)
#ifdef PRIV_STA_BUF
			free_wpa_buf(priv, priv->pshare->aidarray[i]->station.wpa_sta_info);
#else
			kfree(priv->pshare->aidarray[i]->station.wpa_sta_info);
#endif
#endif

#ifdef CONFIG_PCI_HCI
#if defined(WIFI_WMM) && defined(WMM_APSD)
#ifdef PRIV_STA_BUF
		if (priv->pshare->aidarray[i]->station.VO_dz_queue)
			free_sta_que(priv, priv->pshare->aidarray[i]->station.VO_dz_queue);
		if (priv->pshare->aidarray[i]->station.VI_dz_queue)
			free_sta_que(priv, priv->pshare->aidarray[i]->station.VI_dz_queue);
		if (priv->pshare->aidarray[i]->station.BE_dz_queue)
			free_sta_que(priv, priv->pshare->aidarray[i]->station.BE_dz_queue);
		if (priv->pshare->aidarray[i]->station.BK_dz_queue)
			free_sta_que(priv, priv->pshare->aidarray[i]->station.BK_dz_queue);
#else
		if (priv->pshare->aidarray[i]->station.VO_dz_queue)
			kfree(priv->pshare->aidarray[i]->station.VO_dz_queue);
		if (priv->pshare->aidarray[i]->station.VI_dz_queue)
			kfree(priv->pshare->aidarray[i]->station.VI_dz_queue);
		if (priv->pshare->aidarray[i]->station.BE_dz_queue)
			kfree(priv->pshare->aidarray[i]->station.BE_dz_queue);
		if (priv->pshare->aidarray[i]->station.BK_dz_queue)
			kfree(priv->pshare->aidarray[i]->station.BK_dz_queue);
#endif
#endif

#if defined(WIFI_WMM)
#ifdef PRIV_STA_BUF
		if (priv->pshare->aidarray[i]->station.MGT_dz_queue)
			free_sta_mgt_que(priv, priv->pshare->aidarray[i]->station.MGT_dz_queue);
#else
		if (priv->pshare->aidarray[i]->station.MGT_dz_queue)
			kfree(priv->pshare->aidarray[i]->station.MGT_dz_queue);

#endif
#endif
#endif // CONFIG_PCI_HCI

#ifdef RTL8192CD_VARIABLE_USED_DMEM
		rtl8192cd_dmem_free(AID_OBJ, &i);
#else
#ifdef PRIV_STA_BUF
		free_sta_obj(priv, priv->pshare->aidarray[i]);
#else
		kfree(priv->pshare->aidarray[i]);
#endif
#endif
		priv->pshare->aidarray[i] = NULL;
	}

	RESTORE_INT(flags);
	DEBUG_ERR("No free memory to allocate station info\n");
	return NULL;
}


int	free_stainfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long	flags;
#endif
	unsigned int	i;

	if (pstat == (struct stat_info *)NULL)
	{
		DEBUG_ERR("illegal free an NULL stat obj\n");
		return FAIL;
	}

	for(i=0; i<NUM_STAT; i++)
	{
		if (priv->pshare->aidarray[i] &&
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			(priv->pshare->aidarray[i]->priv == priv) &&
#endif
			(priv->pshare->aidarray[i]->used == TRUE) &&
			(&(priv->pshare->aidarray[i]->station) == pstat))
		{
			DEBUG_INFO("free station info of %02X%02X%02X%02X%02X%02X\n",
				pstat->hwaddr[0], pstat->hwaddr[1], pstat->hwaddr[2],
				pstat->hwaddr[3], pstat->hwaddr[4], pstat->hwaddr[5]);

			SAVE_INT_AND_CLI(flags);
#ifdef WDS
#ifdef LAZY_WDS
			if (!(pstat->state & WIFI_WDS) || (pstat->state & WIFI_WDS_LAZY))
#else
			if (!(pstat->state & WIFI_WDS))
#endif
#endif
			{
				priv->pshare->aidarray[i]->used = FALSE;
				// remove from hash_list
				hash_list_del(priv, pstat);
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT) && !defined(RATEADAPTIVE_BY_ODM)
				if (GET_CHIP_VER(priv)==VERSION_8188E)
					priv->pshare->RaInfo[pstat->aid].pstat = NULL;
#endif
			}

			release_stainfo(priv, pstat);
			RESTORE_INT(flags);
			return SUCCESS;
		}
	}

#if defined(CONFIG_RTL_WAPI_SUPPORT)
	wapiAssert(pstat->wapiInfo==NULL);
#endif
	DEBUG_ERR("pstat can not be freed \n");
	return	FAIL;
}


/* any station allocated can be searched by hash list */
__MIPS16
__IRAM_IN_865X
struct stat_info *get_stainfo(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct list_head	*phead, *plist;
	struct stat_info	*pstat;
	unsigned int	index;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

#ifdef RTK_NL80211
	if(hwaddr == NULL)
		return (struct stat_info *)NULL;
#endif

	//if (!memcmp(hwaddr, priv->stainfo_cache.hwaddr, MACADDRLEN) &&  priv->stainfo_cache.pstat)
	pstat = priv->pstat_cache;

#ifdef MULTI_MAC_CLONE
	if ((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) && MCLONE_NUM > 0) {
	    if (pstat && !memcmp(hwaddr, pstat->hwaddr, MACADDRLEN) && pstat->mclone_id == ACTIVE_ID)
		return pstat;
	}
	else
#endif
	{
    if (pstat && !memcmp(hwaddr, pstat->hwaddr, MACADDRLEN))
		return pstat;
	}

	index = wifi_mac_hash(hwaddr);
	phead = &priv->stat_hash[index];
	
	SMP_LOCK_HASH_LIST(flags);

	plist = phead->next;
	while (plist != phead)
	{
		pstat = list_entry(plist, struct stat_info ,hash_list);
		plist = plist->next;
		
		if (!(memcmp((void *)pstat->hwaddr, (void *)hwaddr, MACADDRLEN))) { // if found the matched address
#ifdef MULTI_MAC_CLONE
			if (!(priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) ||
				((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) &&
										!priv->pmib->ethBrExtInfo.macclone_enable) ||
					((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) &&
						priv->pmib->ethBrExtInfo.macclone_enable & (pstat->mclone_id == ACTIVE_ID))) 
#endif		
			{
				priv->pstat_cache = pstat;
				goto exit;
			}
		}
#ifdef CONFIG_PCI_HCI
		if (plist == plist->next)
			break;
#endif
	}
	
	pstat = NULL;
	
exit:
	SMP_UNLOCK_HASH_LIST(flags);
	
	return pstat;
}


#ifdef HW_FILL_MACID
__MIPS16
__IRAM_IN_865X
struct stat_info *get_HW_mapping_sta(struct rtl8192cd_priv *priv, unsigned char macID)
{
    struct aid_obj  *obj;
    
    if(macID > 0)
    {
        if((macID >= 0x7E))
            return (struct stat_info *)NULL;
        else
            return &(priv->pshare->aidarray[macID-1]->station);

        /*
        obj = priv->pshare->aidarray[macID-1];

        if(obj->priv == priv)
            return &(priv->pshare->aidarray[macID-1]->station);
        else
        {
            printk("obj error at macID %x \n",macID);
            return (struct stat_info *)NULL;
        }*/
        
    }
}
#endif // #ifdef HW_FILL_MACID

/* aid is only meaningful for assocated stations... */
struct stat_info *get_aidinfo(struct rtl8192cd_priv *priv, unsigned int aid)
{
	struct list_head	*plist, *phead;
	struct stat_info	*pstat = NULL;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	if (aid == 0)
		return (struct stat_info *)NULL;
	
	SMP_LOCK_ASOC_LIST(flags);

	phead = &priv->asoc_list;
	plist = phead->next;

	while (plist != phead)
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;
		if (pstat->aid == aid)
			goto exit;
	}
	pstat = NULL;
exit:
	SMP_UNLOCK_ASOC_LIST(flags);
	
	return pstat;
}

#if defined(TXREPORT)
struct stat_info *get_macidinfo(struct rtl8192cd_priv *priv, unsigned int aid)
{
	struct list_head	*plist, *phead;
	struct stat_info	*pstat = NULL;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	if (aid == 0)
		return (struct stat_info *)NULL;
	
	SMP_LOCK_ASOC_LIST(flags);

	phead = &priv->asoc_list;
	plist = phead->next;

	while (plist != phead)
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;
		if (REMAP_AID(pstat) == aid)
			goto exit;
	}
	pstat = NULL;
exit:
	SMP_UNLOCK_ASOC_LIST(flags);
	
	return pstat;
}
#endif

int IS_BSSID(struct rtl8192cd_priv *priv, unsigned char *da)
{
	unsigned char *bssid;
	bssid = priv->pmib->dot11StationConfigEntry.dot11Bssid;

	if (!memcmp(da, bssid, 6))
		return TRUE;
	else
		return FALSE;
}


int IS_MCAST(unsigned char *da)
{
	if ((*da) & 0x01)
		return TRUE;
	else
		return FALSE;
}

int IS_BCAST2(unsigned char *da)
{
     if ((*da) == 0xff)
         return TRUE;
     else
         return FALSE;
}


 UINT8 oui_rfc1042[] = {0x00, 0x00, 0x00};
 UINT8 oui_8021h[] = {0x00, 0x00, 0xf8};
 UINT8 oui_cisco[] = {0x00, 0x00, 0x0c};
int p80211_stt_findproto(UINT16 proto)
{
	/* Always return found for now.	This is the behavior used by the */
	/*  Zoom Win95 driver when 802.1h mode is selected */
	/* TODO: If necessary, add an actual search we'll probably
		 need this to match the CMAC's way of doing things.
		 Need to do some testing to confirm.
	*/

	if (proto == 0x80f3 ||   /* APPLETALK */
		proto == 0x8137 ) /* DIX II IPX */
		return 1;

	return 0;
}


void eth_2_llc(struct wlan_ethhdr_t *pethhdr, struct llc_snap *pllc_snap)
{
	pllc_snap->llc_hdr.dsap=pllc_snap->llc_hdr.ssap=0xAA;
	pllc_snap->llc_hdr.ctl=0x03;

	if (p80211_stt_findproto(ntohs(pethhdr->type))) {
		memcpy((void *)pllc_snap->snap_hdr.oui, oui_8021h, WLAN_IEEE_OUI_LEN);
	}
	else {
		memcpy((void *)pllc_snap->snap_hdr.oui, oui_rfc1042, WLAN_IEEE_OUI_LEN);
	}
	pllc_snap->snap_hdr.type = pethhdr->type;
}


void eth2_2_wlanhdr(struct rtl8192cd_priv *priv, struct wlan_ethhdr_t *pethhdr, struct tx_insn *txcfg)
{
	unsigned char *pframe = txcfg->phdr;
	unsigned int to_fr_ds = get_tofr_ds(pframe);

	switch (to_fr_ds)
	{
		case 0x00:
			memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
			memcpy(GetAddr2Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
			memcpy(GetAddr3Ptr(pframe), BSSID, WLAN_ADDR_LEN);
			break;
		case 0x01:
			{
#ifdef MCAST2UI_REFINE
                                if (txcfg->fr_type == _SKB_FRAME_TYPE_)
					memcpy(GetAddr1Ptr(pframe), (const void *) &((struct sk_buff *)txcfg->pframe)->cb[10], WLAN_ADDR_LEN);
                                else
#endif
				memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
				memcpy(GetAddr2Ptr(pframe), BSSID, WLAN_ADDR_LEN);
				memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
			}
			break;
		case 0x02:
			{
				memcpy(GetAddr1Ptr(pframe), BSSID, WLAN_ADDR_LEN);
				memcpy(GetAddr2Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
				memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
			}
			break;
		case 0x03:
#ifdef WDS
#ifdef MP_TEST
			if (OPMODE & WIFI_MP_STATE)
				memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
			else
#endif
#ifdef CONFIG_RTK_MESH
			if(txcfg->is_11s)
				memcpy(GetAddr1Ptr(pframe), txcfg->nhop_11s, WLAN_ADDR_LEN);
			else
#endif
				memcpy(GetAddr1Ptr(pframe), priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr, WLAN_ADDR_LEN);

#ifdef MP_TEST
			if (OPMODE & WIFI_MP_STATE)
				memcpy(GetAddr2Ptr(pframe), priv->dev->dev_addr, WLAN_ADDR_LEN);
			else
#endif

#ifdef __DRAYTEK_OS__
				memcpy(GetAddr2Ptr(pframe), priv->dev->dev_addr, WLAN_ADDR_LEN);
#else
#ifdef CONFIG_RTK_MESH
			if(txcfg->is_11s)
				memcpy(GetAddr2Ptr(pframe), GET_MY_HWADDR, WLAN_ADDR_LEN);
			else
#endif // CONFIG_RTK_MESH
				memcpy(GetAddr2Ptr(pframe), priv->wds_dev[txcfg->wdsIdx]->dev_addr , WLAN_ADDR_LEN);
#endif
			memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
			memcpy(GetAddr4Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);

#ifdef CONFIG_RTK_MESH
			if(txcfg->is_11s && is_qos_data(pframe))
				memset( pframe+WLAN_HDR_A4_LEN, 0, 2); // qos
#endif	// CONFIG_RTK_MESH

#else // not WDS
#ifdef CONFIG_RTK_MESH
			if(txcfg->is_11s) {
				memcpy(GetAddr1Ptr(pframe), txcfg->nhop_11s, WLAN_ADDR_LEN);
				memcpy(GetAddr2Ptr(pframe), GET_MY_HWADDR, WLAN_ADDR_LEN);
				memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
				memcpy(GetAddr4Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);

				//if((*pframe) & 0x80) //if qos is enable, the bit 7 of frame control will set to 1
				if(is_qos_data(pframe))
					memset(pframe+WLAN_HDR_A4_LEN, 0, 2); // qos
			} else
#endif // CONFIG_RTK_MESH

#ifdef A4_STA
			if (priv->pshare->rf_ft_var.a4_enable && txcfg->pstat && 
											(txcfg->pstat->state & WIFI_A4_STA)) {
				memcpy(GetAddr1Ptr(pframe), txcfg->pstat->hwaddr, WLAN_ADDR_LEN);
				memcpy(GetAddr2Ptr(pframe), GET_MY_HWADDR, WLAN_ADDR_LEN);
				memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
				memcpy(GetAddr4Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);				
			}
			else
#endif
			{
			DEBUG_ERR("no support for WDS!\n");
			memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
			memcpy(GetAddr2Ptr(pframe), (const void *)BSSID, WLAN_ADDR_LEN);
			memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
			} // else of if(txcfg->is_11s)
#endif	// WDS
			break;
	}
}


int skb_p80211_to_ether(struct net_device *dev, int wep_mode, struct rx_frinfo *pfrinfo)
{
	UINT	to_fr_ds;
	INT		payload_length;
	INT		payload_offset, trim_pad;
	UINT8	daddr[WLAN_ETHADDR_LEN];
	UINT8	saddr[WLAN_ETHADDR_LEN];
	UINT8	*pframe;
#ifdef CONFIG_RTK_MESH
	INT 	mesh_header_len=0;
#endif
	struct wlan_hdr *w_hdr;
	struct wlan_ethhdr_t   *e_hdr;
	struct wlan_llc_t      *e_llc;
	struct wlan_snap_t     *e_snap;
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv	   *priv = (struct rtl8192cd_priv *)dev->priv;
#endif
	int wlan_pkt_format;
	struct sk_buff *skb = get_pskb(pfrinfo);

#ifdef RX_SHORTCUT
	extern int get_rx_sc_free_entry(struct stat_info *pstat, unsigned char *pframe);
	int privacy, idx=0;
	struct rx_sc_entry *prxsc_entry = NULL;
	struct wlan_hdr wlanhdr;

#ifndef MESH_AMSDU
	struct stat_info 	*pstat = get_stainfo(priv, GetAddr2Ptr(skb->data));
#else
	struct stat_info 	*pstat;

	if (pfrinfo->is_11s & 8)
		pstat = NULL;
	else
		pstat = get_stainfo(priv, GetAddr2Ptr(skb->data));
#endif // MESH_AMSDU
#endif // RX_SHORTCUT

	pframe = get_pframe(pfrinfo);
	to_fr_ds = get_tofr_ds(pframe);
#ifndef MESH_AMSDU
	payload_offset = get_hdrlen(priv, pframe);
#else
	// Get_hdrlen needs to access pframe+32
	// When is_11s=8, the length of a frame might be less than 32 bytes, so we need to protect it
	if( pfrinfo->is_11s &8 )
		payload_offset = 0;
	else
		payload_offset = get_hdrlen(priv, pframe);
#endif // MESH_AMSDU
	trim_pad = 0; // _CRCLNG_ has beed subtracted in isr
	w_hdr = (struct wlan_hdr *)pframe;

#ifdef MESH_AMSDU
	if( pfrinfo->is_11s &8 )
	{
		struct wlan_ethhdr_t eth;
		struct  MESH_HDR* mhdr = (struct MESH_HDR*) (pframe+sizeof(struct wlan_ethhdr_t));
		const short mlen =  (mhdr->mesh_flag &1) ? 16 : 4;
		memcpy( &eth, pframe, sizeof(struct wlan_ethhdr_t));
		if( mlen &16 )
			memcpy(&eth, mhdr->DestMACAddr, WLAN_ETHADDR_LEN<<1 );
		memcpy(skb_pull(skb, mlen), &eth, sizeof(struct wlan_ethhdr_t));
		return SUCCESS;
	}
#endif

	if ( to_fr_ds == 0x00) {
		memcpy(daddr, (const void *)w_hdr->addr1, WLAN_ETHADDR_LEN);
		memcpy(saddr, (const void *)w_hdr->addr2, WLAN_ETHADDR_LEN);
	}
	else if( to_fr_ds == 0x01) {
		memcpy(daddr, (const void *)w_hdr->addr1, WLAN_ETHADDR_LEN);
		memcpy(saddr, (const void *)w_hdr->addr3, WLAN_ETHADDR_LEN);
	}
	else if( to_fr_ds == 0x02) {
		memcpy(daddr, (const void *)w_hdr->addr3, WLAN_ETHADDR_LEN);
		memcpy(saddr, (const void *)w_hdr->addr2, WLAN_ETHADDR_LEN);
	}
	else {
#ifdef CONFIG_RTK_MESH
		// WIFI_11S_MESH = WIFI_QOS_DATA
		if(pfrinfo->is_11s &1) {
			memcpy(daddr, (const void *)pfrinfo->mesh_header.DestMACAddr, WLAN_ETHADDR_LEN);
			memcpy(saddr, (const void *)pfrinfo->mesh_header.SrcMACAddr, WLAN_ETHADDR_LEN);
			mesh_header_len = 16;				
		}
		else
#endif // CONFIG_RTK_MESH
		{
			memcpy(daddr, (const void *)w_hdr->addr3, WLAN_ETHADDR_LEN);
			memcpy(saddr, (const void *)w_hdr->addr4, WLAN_ETHADDR_LEN);
		}
	}

	if (GetPrivacy(pframe)) {
#ifdef CONFIG_RTL_WAPI_SUPPORT
		if ((wep_mode == _WAPI_SMS4_)) {
			payload_offset += (WAPI_EXT_LEN-WAPI_ALIGNMENT_OFFSET);
			trim_pad += (SMS4_MIC_LEN);
		} else
#endif
		if (((wep_mode == _WEP_40_PRIVACY_) || (wep_mode == _WEP_104_PRIVACY_))) {
			payload_offset += 4;
			trim_pad += 4;
		}
		else if ((wep_mode == _TKIP_PRIVACY_)) {
			payload_offset += 8;
			trim_pad += (8 + 4);
		}
		else if ((wep_mode == _CCMP_PRIVACY_)) {
			payload_offset += 8;
			trim_pad += 8;
		}
		else {
			DEBUG_ERR("drop pkt due to unallowed wep_mode privacy=%d\n", wep_mode);
			return FAIL;
		}
	}

#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
	skb->len -= WAPI_ALIGNMENT_OFFSET;
#endif

	payload_length = skb->len - payload_offset - trim_pad;

#ifdef CONFIG_RTK_MESH
	  payload_length -=	mesh_header_len;
#endif

	if (payload_length <= 0) {
		DEBUG_ERR("drop pkt due to payload_length<=0\n");
		return FAIL;
	}

	e_hdr = (struct wlan_ethhdr_t *) (pframe + payload_offset);
	e_llc = (struct wlan_llc_t *) (pframe + payload_offset);
	e_snap = (struct wlan_snap_t *) (pframe + payload_offset + sizeof(struct wlan_llc_t));

	if ((e_llc->dsap==0xaa) && (e_llc->ssap==0xaa) && (e_llc->ctl==0x03))
	{
		if (!memcmp(e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN)) {
			wlan_pkt_format = WLAN_PKT_FORMAT_SNAP_RFC1042;
			if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_IPX, 2))
				wlan_pkt_format = WLAN_PKT_FORMAT_IPX_TYPE4;
			else if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_AARP, 2))
				wlan_pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		}
		else if (!memcmp(e_snap->oui, SNAP_HDR_APPLETALK_DDP, WLAN_IEEE_OUI_LEN) &&
				 !memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_DDP, 2))
			wlan_pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		else if (!memcmp(e_snap->oui, oui_8021h, WLAN_IEEE_OUI_LEN))
			wlan_pkt_format = WLAN_PKT_FORMAT_SNAP_TUNNEL;
		else if (!memcmp(e_snap->oui, oui_cisco, WLAN_IEEE_OUI_LEN))
			wlan_pkt_format = WLAN_PKT_FORMAT_CDP;
		else {
			DEBUG_ERR("drop pkt due to invalid frame format!\n");
			return FAIL;
		}
	}
	else if ((memcmp(daddr, e_hdr->daddr, WLAN_ETHADDR_LEN) == 0) &&
			 (memcmp(saddr, e_hdr->saddr, WLAN_ETHADDR_LEN) == 0))
		wlan_pkt_format = WLAN_PKT_FORMAT_ENCAPSULATED;
	else
		wlan_pkt_format = WLAN_PKT_FORMAT_OTHERS;

	DEBUG_INFO("Convert 802.11 to 802.3 in format %d\n", wlan_pkt_format);

	if ((wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
		(wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL) ||
		(wlan_pkt_format == WLAN_PKT_FORMAT_CDP)) {
		/* Test for an overlength frame */
		payload_length = payload_length - sizeof(struct wlan_llc_t) - sizeof(struct wlan_snap_t);

		if ((payload_length+WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("SNAP frame too large (%d>%d)\n",
				(payload_length+WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

#ifdef RX_SHORTCUT
		if (!priv->pmib->dot11OperationEntry.disable_rxsc && pstat) {
#ifdef CONFIG_RTK_MESH
			if (pfrinfo->is_11s)
				privacy = get_sta_encrypt_algthm(priv, pstat);
			else
#endif // CONFIG_RTK_MESH
#ifdef WDS
			if (pfrinfo->to_fr_ds == 3)
				privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
			else
#endif
				privacy = get_sta_encrypt_algthm(priv, pstat);
			if ((GetFragNum(pframe)==0) &&
#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
                (!pfrinfo->is_11s || !IS_MCAST(pfrinfo->da)) &&
#endif
				((privacy == 0) ||
#ifdef CONFIG_RTL_WAPI_SUPPORT
				(privacy==_WAPI_SMS4_) ||
#endif
				(!UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe))))))
			{
				idx = get_rx_sc_free_entry(pstat, pframe);
				prxsc_entry = &pstat->rx_sc_ent[idx];
				memcpy((void *)&wlanhdr, pframe, pfrinfo->hdr_len);
			}
		}
#endif // RX_SHORTCUT


		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

		if ((wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
			(wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL))
		{
			/* chop llc header from skb. */
			skb_pull(skb, sizeof(struct wlan_llc_t));

			/* chop snap header from skb. */
			skb_pull(skb, sizeof(struct wlan_snap_t));
		}

#ifdef CONFIG_RTK_MESH
		/* chop mesh header from skb. */
		skb_pull(skb, mesh_header_len);
#endif

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		if (wlan_pkt_format == WLAN_PKT_FORMAT_CDP)
			e_hdr->type = payload_length;
		else
			e_hdr->type = e_snap->type;
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length + WLAN_ETHHDR_LEN);

#ifdef RX_SHORTCUT
		if (prxsc_entry) {
			if ((e_hdr->type != htons(0x888e)) && // for WIFI_SIMPLE_CONFIG
#ifdef CONFIG_RTL_WAPI_SUPPORT
				(e_hdr->type != htons(ETH_P_WAPI)) &&
#endif
				(e_hdr->type != htons(ETH_P_ARP)) &&
				(wlan_pkt_format != WLAN_PKT_FORMAT_CDP)) {
				memcpy((void *)&prxsc_entry->rx_wlanhdr, &wlanhdr, pfrinfo->hdr_len);
				memcpy((void *)&prxsc_entry->rx_ethhdr, (const void *)e_hdr, sizeof(struct wlan_ethhdr_t));
				prxsc_entry->rx_payload_offset = payload_offset;
				prxsc_entry->rx_trim_pad = trim_pad;
				pstat->rx_privacy = GetPrivacy(pframe);

#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
				if ( (pfrinfo->is_11s &3) && (pfrinfo->mesh_header.mesh_flag &1))
				{
					memcpy((void *)&prxsc_entry->rx_wlanhdr.meshhdr.DestMACAddr, (const void *)pfrinfo->mesh_header.DestMACAddr, WLAN_ETHADDR_LEN);
					memcpy((void *)&prxsc_entry->rx_wlanhdr.meshhdr.SrcMACAddr, (const void *)pfrinfo->mesh_header.SrcMACAddr, WLAN_ETHADDR_LEN);
				}
#endif
			}
		}
#endif
	}
	else if ((wlan_pkt_format == WLAN_PKT_FORMAT_OTHERS) ||
			 (wlan_pkt_format == WLAN_PKT_FORMAT_APPLETALK) ||
			 (wlan_pkt_format == WLAN_PKT_FORMAT_IPX_TYPE4)) {

		/* Test for an overlength frame */
		if ( (payload_length + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("IPX/AppleTalk frame too large (%d>%d)\n",
				(payload_length + WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

#ifdef CONFIG_RTK_MESH
		/* chop mesh header from skb. */
		skb_pull(skb, mesh_header_len);
#endif

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);
		e_hdr->type = htons(payload_length);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length+WLAN_ETHHDR_LEN);
	}
	else if (wlan_pkt_format == WLAN_PKT_FORMAT_ENCAPSULATED) {

		if ( payload_length > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("Encapsulated frame too large (%d>%d)\n",
				payload_length, WLAN_MAX_ETHFRM_LEN);
		}

		/* Chop off the 802.11 header. */
		skb_pull(skb, payload_offset);

#ifdef CONFIG_RTK_MESH
		/* chop mesh header from skb. */
		skb_pull(skb, mesh_header_len);
#endif

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length);
	}

#ifdef __KERNEL__
#ifdef LINUX_2_6_22_
	skb_reset_mac_header(skb);
#else
	skb->mac.raw = (unsigned char *) skb->data; /* new MAC header */
#endif
#endif

	return SUCCESS;
}


int strip_amsdu_llc(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat)
{
	INT		payload_length;
	INT		payload_offset;
	UINT8	daddr[WLAN_ETHADDR_LEN];
	UINT8	saddr[WLAN_ETHADDR_LEN];
	struct wlan_ethhdr_t	*e_hdr;
	struct wlan_llc_t		*e_llc;
	struct wlan_snap_t		*e_snap;
	int		pkt_format;

	memcpy(daddr, skb->data, MACADDRLEN);
	memcpy(saddr, skb->data+MACADDRLEN, MACADDRLEN);
	payload_length = skb->len - WLAN_ETHHDR_LEN;
	payload_offset = WLAN_ETHHDR_LEN;

	e_hdr = (struct wlan_ethhdr_t *) (skb->data + payload_offset);
	e_llc = (struct wlan_llc_t *) (skb->data + payload_offset);
	e_snap = (struct wlan_snap_t *) (skb->data + payload_offset + sizeof(struct wlan_llc_t));

	if ((e_llc->dsap==0xaa) && (e_llc->ssap==0xaa) && (e_llc->ctl==0x03))
	{
		if (!memcmp(e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN)) {
			pkt_format = WLAN_PKT_FORMAT_SNAP_RFC1042;
			if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_IPX, 2))
				pkt_format = WLAN_PKT_FORMAT_IPX_TYPE4;
			else if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_AARP, 2))
				pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		}
		else if (!memcmp(e_snap->oui, SNAP_HDR_APPLETALK_DDP, WLAN_IEEE_OUI_LEN) &&
				 !memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_DDP, 2))
			pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		else if (!memcmp(e_snap->oui, oui_8021h, WLAN_IEEE_OUI_LEN))
			pkt_format = WLAN_PKT_FORMAT_SNAP_TUNNEL;
		else if (!memcmp(e_snap->oui, oui_cisco, WLAN_IEEE_OUI_LEN))
			pkt_format = WLAN_PKT_FORMAT_CDP;
		else {
			DEBUG_ERR("drop pkt due to invalid frame format!\n");
			return FAIL;
		}
	}
	else if ((memcmp(daddr, e_hdr->daddr, WLAN_ETHADDR_LEN) == 0) &&
			 (memcmp(saddr, e_hdr->saddr, WLAN_ETHADDR_LEN) == 0))
		pkt_format = WLAN_PKT_FORMAT_ENCAPSULATED;
	else
		pkt_format = WLAN_PKT_FORMAT_OTHERS;

	DEBUG_INFO("Convert 802.11 to 802.3 in format %d\n", pkt_format);

	if ((pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
		(pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL) ||
		(pkt_format == WLAN_PKT_FORMAT_CDP)) {
		/* Test for an overlength frame */
		payload_length = payload_length - sizeof(struct wlan_llc_t) - sizeof(struct wlan_snap_t);

		if ((payload_length+WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("SNAP frame too large (%d>%d)\n",
				(payload_length+WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

		if ((pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
			(pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL))
		{
			/* chop llc header from skb. */
			skb_pull(skb, sizeof(struct wlan_llc_t));

			/* chop snap header from skb. */
			skb_pull(skb, sizeof(struct wlan_snap_t));
		}

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		if (pkt_format == WLAN_PKT_FORMAT_CDP)
			e_hdr->type = payload_length;
		else
			e_hdr->type = e_snap->type;
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length + WLAN_ETHHDR_LEN);
	}
	else if ((pkt_format == WLAN_PKT_FORMAT_OTHERS) ||
			 (pkt_format == WLAN_PKT_FORMAT_APPLETALK) ||
			 (pkt_format == WLAN_PKT_FORMAT_IPX_TYPE4)) {

		/* Test for an overlength frame */
		if ( (payload_length + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("IPX/AppleTalk frame too large (%d>%d)\n",
				(payload_length + WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);
		e_hdr->type = htons(payload_length);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length+WLAN_ETHHDR_LEN);
	}
	else if (pkt_format == WLAN_PKT_FORMAT_ENCAPSULATED) {

		if ( payload_length > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("Encapsulated frame too large (%d>%d)\n",
				payload_length, WLAN_MAX_ETHFRM_LEN);
		}

		/* Chop off the 802.11 header. */
		skb_pull(skb, payload_offset);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length);
	}

#ifdef __KERNEL__
#ifdef LINUX_2_6_22_
	skb_reset_mac_header(skb);
#else
	skb->mac.raw = (unsigned char *) skb->data; /* new MAC header */
#endif
#endif

	return SUCCESS;
}


unsigned int get_sta_encrypt_algthm(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned int privacy = 0;

#ifdef CONFIG_RTK_MESH
	if( isMeshPoint(pstat) )
		return priv->pmib->dot11sKeysTable.dot11Privacy ;
#endif

#ifdef CONFIG_RTL_WAPI_SUPPORT
	if (pstat&&pstat->wapiInfo&&pstat->wapiInfo->wapiType!=wapiDisable)
	{
		return _WAPI_SMS4_;
	}
	else
#endif
	{
	if (priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm) {
		if (pstat)
			privacy = pstat->dot11KeyMapping.dot11Privacy;
		else
			DEBUG_ERR("pstat == NULL\n");
	}
		else
		{
		// legacy system
		privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm; //could be wep40 or wep104
	}
	}

	return privacy;
}


unsigned int get_mcast_encrypt_algthm(struct rtl8192cd_priv *priv)
{
	unsigned int privacy;

#ifdef CONFIG_RTL_WAPI_SUPPORT
	if (priv->pmib->wapiInfo.wapiType!=wapiDisable)
	{
		return _WAPI_SMS4_;
	} else
#endif
	{
	if (priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm) {
		// check station info
		privacy = priv->pmib->dot11GroupKeysTable.dot11Privacy;
	}
	else {	// legacy system
		privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;//must be wep40 or wep104
	}
	}
	return privacy;
}


unsigned int get_privacy(struct rtl8192cd_priv *priv, struct stat_info *pstat,
				unsigned int *iv, unsigned int *icv, unsigned int *mic)
{
	unsigned int privacy;
	*iv = 0;
	*icv = 0;
	*mic = 0;

	privacy = get_sta_encrypt_algthm(priv, pstat);

	switch (privacy)
	{
#ifdef CONFIG_RTL_WAPI_SUPPORT
	case _WAPI_SMS4_:
		*iv = WAPI_PN_LEN+2;
		*icv = 0;
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
		if(!(UseSwCrypto(priv, pstat, (pstat ? FALSE : TRUE))))
			*mic = 0;	//HW will take care of the mic
		else
		*mic = SMS4_MIC_LEN;
#else
		*mic = SMS4_MIC_LEN;
#endif
		break;
#endif
	case _NO_PRIVACY_:
		*iv  = 0;
		*icv = 0;
		*mic = 0;
		break;
	case _WEP_40_PRIVACY_:
	case _WEP_104_PRIVACY_:
		*iv = 4;
		*icv = 4;
		*mic = 0;
		break;
	case _TKIP_PRIVACY_:
		*iv = 8;
		*icv = 4;
		*mic = 0;	// mic of TKIP is msdu based
		break;
	case _CCMP_PRIVACY_:
		*iv = 8;
		*icv = 0;
		*mic = 8;
		break;
	default:
		DEBUG_WARN("un-awared encrypted type %d\n", privacy);
		*iv = *icv = *mic = 0;
		break;
	}

	return privacy;
}


unsigned int get_mcast_privacy(struct rtl8192cd_priv *priv, unsigned int *iv, unsigned int *icv,
				unsigned int *mic)
{
	unsigned int privacy;
	*iv  = 0;
	*icv = 0;
	*mic = 0;

	privacy = get_mcast_encrypt_algthm(priv);

	switch (privacy)
	{
#ifdef CONFIG_RTL_WAPI_SUPPORT
	case _WAPI_SMS4_:
		*iv = WAPI_PN_LEN+2;
		*icv = 0;
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
		if(!(UseSwCrypto(priv, NULL, TRUE)))
			*mic = 0;	//HW will take care of the mic
		else
		*mic = SMS4_MIC_LEN;
#else
		*mic = SMS4_MIC_LEN;
#endif
		break;
#endif
	case _NO_PRIVACY_:
		*iv = 0;
		*icv = 0;
		*mic = 0;
		break;
	case _WEP_40_PRIVACY_:
	case _WEP_104_PRIVACY_:
		*iv = 4;
		*icv = 4;
		*mic = 0;
		break;
	case _TKIP_PRIVACY_:
		*iv = 8;
		*icv = 4;
		*mic = 0; // mic of TKIP is msdu based
		break;
	case _CCMP_PRIVACY_:
		*iv = 8;
		*icv = 0;
		*mic = 8;
		break;
	default:
		DEBUG_WARN("un-awared encrypted type %d\n", privacy);
		*iv = 0;
		*icv = 0;
		*mic = 0;
		break;
	}

	return privacy;
}

unsigned char * get_da(unsigned char *pframe)
{
	unsigned char 	*da;
	unsigned int	to_fr_ds	= (GetToDs(pframe) << 1) | GetFrDs(pframe);

	switch (to_fr_ds) {
		case 0x00:	// ToDs=0, FromDs=0
			da = GetAddr1Ptr(pframe);
			break;
		case 0x01:	// ToDs=0, FromDs=1
			da = GetAddr1Ptr(pframe);
			break;
		case 0x02:	// ToDs=1, FromDs=0
			da = GetAddr3Ptr(pframe);
			break;
		default:	// ToDs=1, FromDs=1
			da = GetAddr3Ptr(pframe);
			break;
	}

	return da;
}


unsigned char * get_sa(unsigned char *pframe)
{
	unsigned char 	*sa;
	unsigned int	to_fr_ds	= (GetToDs(pframe) << 1) | GetFrDs(pframe);

	switch (to_fr_ds) {
		case 0x00:	// ToDs=0, FromDs=0
			sa = GetAddr2Ptr(pframe);
			break;
		case 0x01:	// ToDs=0, FromDs=1
			sa = GetAddr3Ptr(pframe);
			break;
		case 0x02:	// ToDs=1, FromDs=0
			sa = GetAddr2Ptr(pframe);
			break;
		default:	// ToDs=1, FromDs=1
			sa = GetAddr4Ptr(pframe);
			break;
	}

	return sa;
}


__MIPS16
__IRAM_IN_865X
unsigned char get_hdrlen(struct rtl8192cd_priv *priv, UINT8 *pframe)
{
	if (GetFrameType(pframe) == WIFI_DATA_TYPE)
	{
#ifdef CONFIG_RTK_MESH
		if ((get_tofr_ds(pframe) == 0x03) && ( (GetFrameSubType(pframe) == WIFI_11S_MESH) || (GetFrameSubType(pframe) == WIFI_11S_MESH_ACTION)))
		{
			if(GetFrameSubType(pframe) == WIFI_11S_MESH)  // DATA frame, qos might be on (TRUE on 8186)
			{
					return WLAN_HDR_A4_QOS_LEN;
			} // WIFI_11S_MESH
			else // WIFI_11S_MESH_ACTION frame, although qos flag is on, the qos field(2bytes) is not used for 8186
			{
				if(is_mesh_6addr_format_without_qos(pframe)) {
					return WLAN_HDR_A6_MESH_DATA_LEN;
				} else {
					return WLAN_HDR_A4_MESH_DATA_LEN;
				}
			}
		} // end of get_tofr_ds == 0x03 & (MESH DATA or MESH ACTION)
		else
#endif // CONFIG_RTK_MESH
		if (is_qos_data(pframe)) {
			if (get_tofr_ds(pframe) == 0x03)
				return WLAN_HDR_A4_QOS_LEN;
			else
				return WLAN_HDR_A3_QOS_LEN;
		}
		else {
			if (get_tofr_ds(pframe) == 0x03)
				return WLAN_HDR_A4_LEN;
			else
				return WLAN_HDR_A3_LEN;
		}
	}
	else if (GetFrameType(pframe) == WIFI_MGT_TYPE)
		return 	WLAN_HDR_A3_LEN;
	else if (GetFrameType(pframe) == WIFI_CTRL_TYPE)
	{
		if (GetFrameSubType(pframe) == WIFI_PSPOLL)
			return 16;
		else if (GetFrameSubType(pframe) == WIFI_BLOCKACK_REQ)
			return 16;
		else if (GetFrameSubType(pframe) == WIFI_BLOCKACK)
			return 16;
		else
		{
#ifdef _DEBUG_RTL8192CD_
			printk("unallowed control pkt type! 0x%04X\n", GetFrameSubType(pframe));
#endif
			return 0;
		}
	}
	else
	{
#ifdef _DEBUG_RTL8192CD_
		printk("unallowed pkt type! 0x%04X\n", GetFrameType(pframe));
#endif
		return 0;
	}
}


unsigned char *get_mgtbuf_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);
	
	ret = get_buf_from_poll(priv, &priv->pshare->wlanbuf_list, (unsigned int *)&priv->pshare->pwlanbuf_poll->count);

	RESTORE_INT(flags);
	return ret;
}


void release_mgtbuf_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanbuf_list, (unsigned int *)&priv->pshare->pwlanbuf_poll->count);

	RESTORE_INT(flags);
}


unsigned char *get_wlanhdr_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *pbuf;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	pbuf = get_buf_from_poll(priv, &priv->pshare->wlan_hdrlist, (unsigned int *)&priv->pshare->pwlan_hdr_poll->count);
#ifdef TX_EARLY_MODE
	pbuf += 8;
#endif

	RESTORE_INT(flags);
	return pbuf;
}


void release_wlanhdr_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (pbuf == NULL)
	{
		DEBUG_ERR("Err: Free Null Buf!\n");
		return;
	}
	
	SAVE_INT_AND_CLI(flags);

#ifdef TX_EARLY_MODE
	pbuf -= 8;
#endif
	release_buf_to_poll(priv, pbuf, &priv->pshare->wlan_hdrlist, (unsigned int *)&priv->pshare->pwlan_hdr_poll->count);

	RESTORE_INT(flags);
}


//__MIPS16
__IRAM_IN_865X
unsigned char *get_wlanllchdr_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *pbuf;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	pbuf = get_buf_from_poll(priv, &priv->pshare->wlanllc_hdrlist, (unsigned int *)&priv->pshare->pwlanllc_hdr_poll->count);
#ifdef TX_EARLY_MODE
	pbuf += 8;
#endif

	RESTORE_INT(flags);
	return pbuf;
}


void release_wlanllchdr_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

#ifdef TX_EARLY_MODE
	pbuf -= 8;
#endif
	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanllc_hdrlist, (unsigned int *)&priv->pshare->pwlanllc_hdr_poll->count);

	RESTORE_INT(flags);
}


unsigned char *get_icv_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	ret = get_buf_from_poll(priv, &priv->pshare->wlanicv_list, (unsigned int *)&priv->pshare->pwlanicv_poll->count);

	RESTORE_INT(flags);
	return ret;
}


void release_icv_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanicv_list, (unsigned int *)&priv->pshare->pwlanicv_poll->count);

	RESTORE_INT(flags);
}


unsigned char *get_mic_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	ret = get_buf_from_poll(priv, &priv->pshare->wlanmic_list, (unsigned int *)&priv->pshare->pwlanmic_poll->count);

	RESTORE_INT(flags);
	return ret;
}


void release_mic_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanmic_list, (unsigned int *)&priv->pshare->pwlanmic_poll->count);

	RESTORE_INT(flags);
}


unsigned short get_pnl(union PN48 *ptsc)
{
	return (((ptsc->_byte_.TSC1) << 8) | (ptsc->_byte_.TSC0));
}


unsigned int get_pnh(union PN48 *ptsc)
{
	return 	(((ptsc->_byte_.TSC5) << 24) |
			 ((ptsc->_byte_.TSC4) << 16) |
			 ((ptsc->_byte_.TSC3) << 8) |
			  (ptsc->_byte_.TSC2));
}


int UseSwCrypto(struct rtl8192cd_priv *priv, struct stat_info *pstat, int isMulticast)
{
	if (SWCRYPTO)
		return 1;
	else // hw crypto
	{
#ifdef CONFIG_RTK_MESH
	if( isMeshPoint(pstat) )
		return 0;
//		return	(pstat->dot11KeyMapping.keyInCam || isMulticast) ? 0 : 1;

#endif

#ifdef WDS
		if (pstat && (pstat->state & WIFI_WDS) && !(pstat->state & WIFI_ASOC_STATE)) {
#ifndef CONFIG_RTL8186_KB
			if (!pstat->dot11KeyMapping.keyInCam)
				return 1;
			else
#endif
			return 0;
		}
#endif

			if (priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm
#ifdef CONFIG_RTL_WAPI_SUPPORT
			 || (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == 0 
				&& priv->pmib->wapiInfo.wapiType > 0)
			
#endif
			) {
				if (isMulticast) { // multicast
					if (!priv->pmib->dot11GroupKeysTable.keyInCam)
						return 1;
					else
						return 0;
				}
				else {
				if (!pstat->dot11KeyMapping.keyInCam)
					return 1;
				else // key is in CAM
					return 0;
				}
			}
			else { // legacy 802.11 auth (wep40 || wep104)
#ifdef MBSSID
				if (GET_ROOT(priv)->pmib->miscEntry.vap_enable)
				{
					if (GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) {
						if (isMulticast)
							return 1;
						else {
						if (!pstat->dot11KeyMapping.keyInCam)
							return 1;
						else // key is in CAM
							return 0;
						}
					}
				}
#endif

#ifdef USE_WEP_DEFAULT_KEY
				if (GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE)
                {
                    if (pstat && (pstat->state & WIFI_ASOC_STATE))
                        return 0;
                }

				if (isMulticast && 	!priv->pmib->dot11GroupKeysTable.keyInCam)
					return 1;
#else			
				if (isMulticast) {
					if (!priv->pmib->dot11GroupKeysTable.keyInCam)
						return 1;
				}
				else {
					if (!pstat->dot11KeyMapping.keyInCam)
						return 1;				
				}			
#endif			
				return 0;
			}
		}
	}


void check_protection_shortslot(struct rtl8192cd_priv *priv)
{
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8881A)
	if((GET_CHIP_VER(priv)== VERSION_8812E)||(GET_CHIP_VER(priv)== VERSION_8881A))
		return;
#endif
	if (priv->pmib->dot11ErpInfo.nonErpStaNum == 0 &&
			priv->pmib->dot11ErpInfo.olbcDetected == 0)
	{
		if (priv->pmib->dot11ErpInfo.protection) {
			priv->pmib->dot11ErpInfo.protection = 0;
			//priv->pshare->phw->RTSInitRate_Candidate = 0x8;	// 24Mbps
		}
	}
	else
	{
		if (!priv->pmib->dot11StationConfigEntry.protectionDisabled &&
					priv->pmib->dot11ErpInfo.protection == 0) {
			priv->pmib->dot11ErpInfo.protection = 1;
			//priv->pshare->phw->RTSInitRate_Candidate = 0x3; // 11Mbps
		}
	}

	if (priv->pmib->dot11ErpInfo.nonErpStaNum == 0)
	{
		if (priv->pmib->dot11ErpInfo.shortSlot == 0)
		{
			priv->pmib->dot11ErpInfo.shortSlot = 1;
#ifdef MBSSID
			if ((IS_ROOT_INTERFACE(priv))
#ifdef UNIVERSAL_REPEATER
				|| (IS_VXD_INTERFACE(priv))
#endif
				)
#endif
			set_slot_time(priv, priv->pmib->dot11ErpInfo.shortSlot);
			SET_SHORTSLOT_IN_BEACON_CAP;
			DEBUG_INFO("set short slot time\n");
		}
	}
	else
	{
		if (priv->pmib->dot11ErpInfo.shortSlot)
		{
			priv->pmib->dot11ErpInfo.shortSlot = 0;
#ifdef MBSSID
			if ((IS_ROOT_INTERFACE(priv))
#ifdef UNIVERSAL_REPEATER
				|| (IS_VXD_INTERFACE(priv))
#endif
				)
#endif
			set_slot_time(priv, priv->pmib->dot11ErpInfo.shortSlot);
			RESET_SHORTSLOT_IN_BEACON_CAP;
			DEBUG_INFO("reset short slot time\n");
		}
	}
}


void check_sta_characteristic(struct rtl8192cd_priv *priv, struct stat_info *pstat, int act)
{
	if (act == INCREASE) {
		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) && !isErpSta(pstat)) {
			priv->pmib->dot11ErpInfo.nonErpStaNum++;
			check_protection_shortslot(priv);

			if (!pstat->useShortPreamble)
				priv->pmib->dot11ErpInfo.longPreambleStaNum++;
		}

		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && (pstat->ht_cap_len == 0))
			priv->ht_legacy_sta_num++;
	}
	else {
		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) && !isErpSta(pstat)) {
			priv->pmib->dot11ErpInfo.nonErpStaNum--;
			check_protection_shortslot(priv);

			if (!pstat->useShortPreamble && priv->pmib->dot11ErpInfo.longPreambleStaNum > 0)
				priv->pmib->dot11ErpInfo.longPreambleStaNum--;
		}

		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && (pstat->ht_cap_len == 0))
			priv->ht_legacy_sta_num--;
	}
}

int should_forbid_Nmode(struct rtl8192cd_priv *priv)
{
	if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11N))
		return 0;

	if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_)
		return 0;

	if (!(priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(3)))
		return 0;

	// if pure TKIP, change N mode to G mode
	if (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(1))	{
                if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK == 0
                        && priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm == 1) {
                        if (priv->pmib->dot1180211AuthEntry.dot11WPACipher == 2
                                || priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 2)
                                return 1;
                }
		else if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK == 1) {
			if (priv->pmib->dot1180211AuthEntry.dot11WPACipher == 2)
				return 1;
		}
		else if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK == 2) {
			if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 2)
				return 1;
		}
		else if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK == 3) {
			if ((priv->pmib->dot1180211AuthEntry.dot11WPACipher == 2) &&
				(priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 2))
				return 1;
		}
	}
	
	// if WEP, forbid  N mode
	if ((priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(0)) &&
		 (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_40_PRIVACY_ ||
		  priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_104_PRIVACY_) )
		return 1;

	return 0;
}

#ifdef RTK_AC_SUPPORT //for 11ac logo
int is_mixed_mode(struct rtl8192cd_priv *priv)
{
	if((priv->pmib->dot1180211AuthEntry.dot11EnablePSK == 3)
		&& (priv->pmib->dot1180211AuthEntry.dot11WPACipher & BIT(1))
		&& (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher & BIT(1))
		&& (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == 2))
	{
		return 1;
	}
	else
		return 0;
}
#endif

int should_restrict_Nrate(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (OPMODE & WIFI_AP_STATE)
	{
		if (pstat->is_legacy_encrpt == 1) {
			if (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(1)) {
				if (!pstat->is_realtek_sta || (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(2)))
					return 1;
			}
		}
		else if (pstat->is_legacy_encrpt == 2) {
			if (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(0)) {
				if (!pstat->is_realtek_sta || (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(2)))
					return 1;
			}
		}

#ifdef RTK_AC_SUPPORT //for 11ac logo  // Cheat for mixed mode
	if(AC_SIGMA_MODE != AC_SIGMA_NONE) {
		if(is_mixed_mode(priv))
			return 1;
	}
#endif
		
#ifdef WDS
		else if (pstat->state & WIFI_WDS) {
			if ((priv->pmib->dot11WdsInfo.wdsPrivacy == _WEP_40_PRIVACY_) ||
				(priv->pmib->dot11WdsInfo.wdsPrivacy == _WEP_104_PRIVACY_) ||
				(priv->pmib->dot11WdsInfo.wdsPrivacy == _TKIP_PRIVACY_))
				return 1;
		}
#endif
	}
// Client mode IOT issue, Button 2009.07.17
#ifdef CLIENT_MODE
	else if(OPMODE & WIFI_STATION_STATE)
	{

		if(!pstat->is_realtek_sta && (pstat->IOTPeer != HT_IOT_PEER_MARVELL) && pstat->is_legacy_encrpt)


		return 1;
	}
#endif

	return 0;
}


#ifdef WDS
int getWdsIdxByDev(struct rtl8192cd_priv *priv, struct net_device *dev)
{
	int i;

#ifdef LAZY_WDS
	int max_num;
	if (priv->pmib->dot11WdsInfo.wdsEnabled == WDS_LAZY_ENABLE) 
		max_num = NUM_WDS;
	else
		max_num = priv->pmib->dot11WdsInfo.wdsNum;

	for (i=0; i<max_num; i++) {	
#else

	for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
#endif		

		if (dev == priv->wds_dev[i])
			return i;
	}
	return -1;
}


struct net_device *getWdsDevByAddr(struct rtl8192cd_priv *priv, unsigned char *addr)
{
	int i;

#ifdef LAZY_WDS
	int max_num;
	if (priv->pmib->dot11WdsInfo.wdsEnabled == WDS_LAZY_ENABLE) 
		max_num = NUM_WDS;
	else
		max_num = priv->pmib->dot11WdsInfo.wdsNum;

	for (i=0; i<max_num; i++) {	
#else

	for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
#endif		
			if (!memcmp(priv->pmib->dot11WdsInfo.entry[i].macAddr, addr, 6))
				return priv->wds_dev[i];
	}
	return NULL;
}
#endif // WDS


void validate_oper_rate(struct rtl8192cd_priv *priv)
{
	unsigned int supportedRates;
	unsigned int basicRates;

	if (OPMODE & WIFI_AP_STATE)
	{
		supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
		basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

#ifndef CONFIG_RTL8186_KB
		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
			if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11G)) {
				// if use B only, mask G high rate
				supportedRates &= 0xf;
				basicRates &= 0xf;
			}
		}
		else {
			// if use A or G mode, mask B low rate
			supportedRates &= 0xff0;
			basicRates &= 0xff0;
		}

		if (supportedRates == 0) {
			if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11G | WIRELESS_11A))
				supportedRates = 0xff0;
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)
				supportedRates |= 0xf;

			PRINT_INFO("invalid supproted rate, use default value [%x]!\n", supportedRates);
		}

		if (basicRates == 0) {
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A)
				//basicRates = 0x1f0;
				//11a basic rate is 6/12/24M 
				basicRates = 0x150;
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)
				basicRates = 0xf;
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B))
					basicRates = 0x1f0;
			}

			PRINT_INFO("invalid basic rate, use default value [%x]!\n", basicRates);
		}

		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) {
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
				if ((basicRates & 0xf) == 0)		// if no CCK rates. jimmylin 2004/12/02
					basicRates |= 0xf;
				if ((supportedRates & 0xf) == 0)	// if no CCK rates. jimmylin 2004/12/02
					supportedRates |= 0xf;
			}
			if ((supportedRates & 0xff0) == 0) {	// no ERP rate existed
				supportedRates |= 0xff0;

				PRINT_INFO("invalid supported rate for 11G, use default value [%x]!\n",
																	supportedRates);
			}
		}
#endif // !CONFIG_RTL8186_KB

		priv->supported_rates = supportedRates;
		priv->basic_rates = basicRates;

		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) {
			if (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS == 0)
				priv->pmib->dot11nConfigEntry.dot11nSupportedMCS = 0xffff;
		}
	}
#ifdef CLIENT_MODE
	else
	{
		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A) {
			if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11B | WIRELESS_11G))
				priv->dual_band = 1;
			else
				priv->dual_band = 0;
		}
		else
			priv->dual_band = 0;

		if (priv->dual_band) {
			// for 2.4G band
			supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
			basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11G)) {
					supportedRates &= 0xf;
					basicRates &= 0xf;
				}
				if ((supportedRates & 0xf) == 0)
					supportedRates |= 0xf;
				if ((basicRates & 0xf) == 0)
					basicRates |= 0xf;
			}
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)) {
					supportedRates &= 0xff0;
					basicRates &= 0xff0;
				}
				if ((supportedRates & 0xff0) == 0)
					supportedRates |= 0xff0;
				if ((basicRates & 0xff0) == 0)
					basicRates |= 0x1f0;
			}

			priv->supported_rates = supportedRates;
			priv->basic_rates = basicRates;

			// for 5G band
			supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
			basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

			supportedRates &= 0xff0;
			basicRates &= 0xff0;
			if (supportedRates == 0)
				supportedRates |= 0xff0;
			if (basicRates == 0)
				basicRates |= 0x1f0;

			priv->supported_rates_alt = supportedRates;
			priv->basic_rates_alt = basicRates;
		}
		else {
			supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
			basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11G)) {
					supportedRates &= 0xf;
					basicRates &= 0xf;
				}
				if ((supportedRates & 0xf) == 0)
					supportedRates |= 0xf;
				if ((basicRates & 0xf) == 0)
					basicRates |= 0xf;
			}
			if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11G | WIRELESS_11A)) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)) {
					supportedRates &= 0xff0;
					basicRates &= 0xff0;
				}
				if ((supportedRates & 0xff0) == 0)
					supportedRates |= 0xff0;
				if ((basicRates & 0xff0) == 0)
					basicRates |= 0x1f0;
			}

			priv->supported_rates = supportedRates;
			priv->basic_rates = basicRates;
		}

		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) {
			if (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS == 0)
				priv->pmib->dot11nConfigEntry.dot11nSupportedMCS = 0xffff;
		}
	}
#endif
#if defined(RTK_AC_SUPPORT)
	if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) {
		
		if (IS_TEST_CHIP(priv))	{
			if(get_rf_mimo_mode(priv) == MIMO_1T1R) {
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x0ff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfffc;
			} else {
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x3fcff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfff0;
			}					
		} else {
			if(get_rf_mimo_mode(priv) == MIMO_1T1R) {
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x3ff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT |= 0xfffc;

				if(!priv->pmib->dot11acConfigEntry.dot11VHT_TxMap)
					priv->pmib->dot11acConfigEntry.dot11VHT_TxMap = 0x3ff;
				if(priv->pmib->dot11acConfigEntry.dot11SupportedVHT == 0xffff)
					priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfffe;
			} else if (get_rf_mimo_mode(priv) == MIMO_2T2R) { //eric_8814
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0xfffff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT |= 0xfff0;

				if(!priv->pmib->dot11acConfigEntry.dot11VHT_TxMap)
					priv->pmib->dot11acConfigEntry.dot11VHT_TxMap = 0xfffff;
				if(priv->pmib->dot11acConfigEntry.dot11SupportedVHT == 0xffff)
					priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfffa;
			} else if ((get_rf_mimo_mode(priv) == MIMO_3T3R) ||(get_rf_mimo_mode(priv) == MIMO_4T4R)) {  //eric_8814
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x3fffffff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT |= 0xffc0;
				
				if(!priv->pmib->dot11acConfigEntry.dot11VHT_TxMap)
					priv->pmib->dot11acConfigEntry.dot11VHT_TxMap = 0x3fffffff;
				if(priv->pmib->dot11acConfigEntry.dot11SupportedVHT == 0xffff)
					priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xffda;
			}			
		}
	} 

#endif	
}


void get_oper_rate(struct rtl8192cd_priv *priv)
{
	unsigned int supportedRates=0;
	unsigned int basicRates=0;
	unsigned char val;
	int i, idx=0;

	memset(AP_BSSRATE, 0, sizeof(AP_BSSRATE));
	AP_BSSRATE_LEN = 0;

	if (OPMODE & WIFI_AP_STATE) {
		supportedRates = priv->supported_rates;
		basicRates = priv->basic_rates;
	}
#ifdef CLIENT_MODE
	else {
		if (priv->dual_band && (priv->pshare->curr_band == BAND_5G)) {
			supportedRates = priv->supported_rates_alt;
			basicRates = priv->basic_rates_alt;
		}
		else {
			supportedRates = priv->supported_rates;
			basicRates = priv->basic_rates;
		}
	}
#endif

	for (i=0; dot11_rate_table[i]; i++) {
		int bit_mask = 1 << i;
		if (supportedRates & bit_mask) {
			val = dot11_rate_table[i];

#ifdef SUPPORT_SNMP_MIB
			SNMP_MIB_ASSIGN(dot11SupportedDataRatesSet[i], ((unsigned int)val));
			SNMP_MIB_ASSIGN(dot11OperationalRateSet[i], ((unsigned char)val));
#endif

			if (basicRates & bit_mask)
				val |= 0x80;

			AP_BSSRATE[idx] = val;
			AP_BSSRATE_LEN++;
			idx++;
		}
	}

#ifdef SUPPORT_SNMP_MIB
	SNMP_MIB_ASSIGN(dot11SupportedDataRatesNum, ((unsigned char)AP_BSSRATE_LEN));
#endif

}


// bssrate_ie: _SUPPORTEDRATES_IE_ get supported rate set
// bssrate_ie: _EXT_SUPPORTEDRATES_IE_ get extended supported rate set
int get_bssrate_set(struct rtl8192cd_priv *priv, int bssrate_ie, unsigned char **pbssrate, int *bssrate_len)
{
	int i;

#ifdef CONFIG_RTL_92D_SUPPORT
	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
#else
	if ((priv->pshare->curr_band == BAND_5G)||(priv->pmib->dot11nConfigEntry.dot11nUse40M == HT_CHANNEL_WIDTH_80)) //AC2G_256QAM
#endif
	{

#ifdef P2P_SUPPORT			
		if(bssrate_ie == _SUPPORTED_RATES_NO_CCK_ ){
				*pbssrate = &dot11_rate_table[4];
				*bssrate_len = 8;
				return TRUE;
		}
#endif			
	
		if (bssrate_ie == _SUPPORTEDRATES_IE_ 	)
		{
			for(i=0; i<AP_BSSRATE_LEN; i++)
				if (!is_CCK_rate(AP_BSSRATE[i] & 0x7f))
					break;

			if (i == AP_BSSRATE_LEN)
				return FALSE;
			else {
				*pbssrate = &AP_BSSRATE[i];
				*bssrate_len = AP_BSSRATE_LEN - i;
				return TRUE;
			}
		}
		else
			return FALSE;
	}
	else
	{
		if (bssrate_ie == _SUPPORTEDRATES_IE_)
		{
			*pbssrate = AP_BSSRATE;
			if (AP_BSSRATE_LEN > 8)
				*bssrate_len = 8;
			else
				*bssrate_len = AP_BSSRATE_LEN;
			return TRUE;
		}
#ifdef P2P_SUPPORT
		else if( bssrate_ie == _SUPPORTED_RATES_NO_CCK_){
				*pbssrate = &dot11_rate_table[4];
				*bssrate_len = 8;
				return TRUE;
		}
#endif		
		else
		{
			if (AP_BSSRATE_LEN > 8) {
				*pbssrate = &AP_BSSRATE[8];
				*bssrate_len = AP_BSSRATE_LEN - 8;
				return TRUE;
			}
			else
				return FALSE;
		}
	}
}


struct channel_list{
	unsigned char	channel[31];
	unsigned char	len;
};
static struct channel_list reg_channel_2_4g[] = {
	/* FCC */		{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* IC */		{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* ETSI */		{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* SPAIN */		{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* FRANCE */	{{10,11,12,13},4},
	/* MKK */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* ISRAEL */	{{3,4,5,6,7,8,9,10,11,12,13},11},
	/* MKK1 */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* MKK2 */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* MKK3 */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* NCC (Taiwan) */	{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* RUSSIAN */	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* CN */		{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* Global */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* World_wide */	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* Test */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
};

#ifdef DFS
static struct channel_list reg_channel_5g_full_band[] = {
	/* FCC */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},20},
	/* IC */		{{36,40,44,48,52,56,60,64,149,153,157,161},12},
	/* ETSI */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* SPAIN */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* FRANCE */	{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* MKK */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* ISRAEL */	{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* MKK1 */		{{34,38,42,46},4},
	/* MKK2 */		{{36,40,44,48},4},
	/* MKK3 */		{{36,40,44,48,52,56,60,64},8},
	/* NCC (Taiwan) */	{{56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},15},
	/* RUSSIAN */	{{36,40,44,48,52,56,60,64,132,136,140,149,153,157,161,165},16},
	/* CN */		{{36,40,44,48,52,56,60,64,149,153,157,161,165},13},
	/* Global */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},20},
	/* World_wide */	{{36,40,44,48,52,56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},20},
	/* Test */		{{36,40,44,48, 52,56,60,64, 100,104,108,112, 116,120,124,128, 132,136,140,144, 149,153,157,161, 165,169,173,177}, 28},
	/* 5M10M */		{{146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170}, 25},
};

struct channel_list reg_channel_5g_not_dfs_band[] = {
	/* FCC */		{{36,40,44,48,149,153,157,161,165},9},
	/* IC */		{{36,40,44,48,149,153,157,161},8},
	/* ETSI */		{{36,40,44,48},4},
	/* SPAIN */		{{36,40,44,48},4},
	/* FRANCE */	{{36,40,44,48},4},
	/* MKK */		{{36,40,44,48},4},
	/* ISRAEL */	{{36,40,44,48},4},
	/* MKK1 */		{{34,38,42,46},4},
	/* MKK2 */		{{36,40,44,48},4},
	/* MKK3 */		{{36,40,44,48},4},
	/* NCC (Taiwan) */	{{56,60,64,149,153,157,161,165},8},
	/* RUSSIAN */	{{36,40,44,48,149,153,157,161,165},9},
	/* CN */		{{36,40,44,48,149,153,157,161,165},9},
	/* Global */		{{36,40,44,48,149,153,157,161,165},9},
	/* World_wide */	{{36,40,44,48,149,153,157,161,165},9},
	/* Test */		{{36,40,44,48, 149,153,157,161, 165,169,173,177}, 12},
	/* 5M10M */		{{146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170}, 25},
};
#else

// Exclude DFS channels
static struct channel_list reg_channel_5g_full_band[] = {
	/* FCC */		{{36,40,44,48,149,153,157,161,165},9},
	/* IC */		{{36,40,44,48,149,153,157,161},8},
	/* ETSI */		{{36,40,44,48},4},
	/* SPAIN */		{{36,40,44,48},4},
	/* FRANCE */	{{36,40,44,48},4},
	/* MKK */		{{36,40,44,48},4},
	/* ISRAEL */	{{36,40,44,48},4},
	/* MKK1 */		{{34,38,42,46},4},
	/* MKK2 */		{{36,40,44,48},4},
	/* MKK3 */		{{36,40,44,48},4},
	/* NCC (Taiwan) */	{{56,60,64,149,153,157,161,165},8},
	/* RUSSIAN */	{{36,40,44,48,149,153,157,161,165},9},
	/* CN */		{{36,40,44,48,149,153,157,161,165},9},
	/* Global */		{{36,40,44,48,149,153,157,161,165},9},
	/* World_wide */	{{36,40,44,48,149,153,157,161,165},9},
	/* Test */		{{36,40,44,48, 52,56,60,64, 100,104,108,112, 116,120,124,128, 132,136,140,144, 149,153,157,161, 165,169,173,177}, 28},
	/* 5M10M */		{{146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170}, 25},
};
#endif


int is_available_channel(struct rtl8192cd_priv *priv, unsigned char channel) {
	
	if(priv->pmib->dot11DFSEntry.disable_DFS == 1) {
		if((channel >= 52 && channel <= 140))
			return 0;
	}
	
	if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_1) &&
				(channel >= 36 && channel <= 48))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_2) &&
				(channel >= 52 && channel <= 64))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_3) &&
				(channel >= 100 && channel <= 144))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_4) &&
				(channel >= 149 && channel <= 177))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_4) &&
				(channel >= 146 && channel <= 177) && 
				(priv->pmib->dot11nConfigEntry.dot11nUse40M == 4 ||priv->pmib->dot11nConfigEntry.dot11nUse40M == 5))
		return 1;
	else
		return 0;
}


int is_available_NonDFS_channel(struct rtl8192cd_priv *priv, unsigned char channel)
{
	if((priv->pmib->dot11RFEntry.band5GSelected == PHY_BAND_5G_3))
		return 0;
	else
		return 1;
}


#define MAX_NUM_80M_CH 7
unsigned int CH_80m[MAX_NUM_80M_CH]={36,52,100,116,132,149,165};

int is80MChannel(unsigned int chnl_list[], unsigned int chnl_num,unsigned int channel)
{
	int idx;
	int chNO;
	int baseCH=0;
	idx = -1;

	if( (CH_80m[MAX_NUM_80M_CH-1] <= channel) && ((CH_80m[MAX_NUM_80M_CH-1]+8) >= channel))
		baseCH = CH_80m[MAX_NUM_80M_CH-1];
	else
	{
		for(chNO=0;chNO<MAX_NUM_80M_CH-1;chNO++) {
			if(CH_80m[chNO] <= channel && CH_80m[chNO+1] > channel) {
				baseCH = CH_80m[chNO];			
				break;
			}
		}
	}

	if(baseCH == 0)
		_DEBUG_ERR("Channel is out of scope\n");
	
	for(idx=0;idx<chnl_num;idx++) {
		// available_chnl is sorted.
		if(chnl_list[idx] == baseCH)
			break;
	}

	if(idx == chnl_num || idx + 3 >= chnl_num)
		return 0;

	if(chnl_list[idx+1] == baseCH + 4 &&
	   chnl_list[idx+2] == baseCH + 8 &&
	   chnl_list[idx+3] == baseCH + 12)
	   return 1;
	else
	   return 0;
}


#define MAX_NUM_40M_CH 14
unsigned int CH_40m[MAX_NUM_40M_CH]={36,44,52,60,100,108,116,124,132,140,149,157,165,173};

int is40MChannel(unsigned int chnl_list[], unsigned int chnl_num,unsigned int channel)
{
	int idx;
	int chNO;
	int baseCH=0;
	idx = -1;

	if( (CH_40m[MAX_NUM_40M_CH-1] <= channel) && ((CH_40m[MAX_NUM_40M_CH-1]+4) >= channel))
		baseCH = CH_40m[MAX_NUM_40M_CH-1];
	else
	{
		for(chNO=0;chNO<MAX_NUM_40M_CH-1;chNO++) {
			if(CH_40m[chNO] <= channel && CH_40m[chNO+1] > channel) {
				baseCH = CH_40m[chNO];			
				break;
			}
		}
	}

	if(baseCH == 0)
		_DEBUG_ERR("Channel is out of scope\n");
	
	for(idx=0;idx<chnl_num;idx++) {
		// available_chnl is sorted.
		if(chnl_list[idx] == baseCH)
			break;
	}

	if(idx == chnl_num || idx + 1 >= chnl_num)
		return 0;

	if(chnl_list[idx+1] == baseCH + 4)
	   return 1;
	else
	   return 0;
}


int find80MChannel(unsigned int chnl_list[], unsigned int chnl_num) {
	int i;	
	unsigned int random;
	unsigned int num;
#ifdef __ECOS
	{
		unsigned char random_buf[4];
		get_random_bytes(random_buf, 4);
		random = random_buf[3];
	}
#else
		get_random_bytes(&random, 4);
#endif
		num = random % chnl_num;
	for(i=num;i<chnl_num+num;i++) {
		if (i>=chnl_num)
			i=i-chnl_num;
		if(is80MChannel(chnl_list,chnl_num,chnl_list[i]))
			return chnl_list[i];
	}
	return -1;
}


int find40MChannel(unsigned int chnl_list[], unsigned int chnl_num)
{
	int i;
	unsigned int random;
	unsigned int num;
#ifdef __ECOS
	{
		unsigned char random_buf[4];
		get_random_bytes(random_buf, 4);
		random = random_buf[3];
	}
#else
		get_random_bytes(&random, 4);
#endif
		num = random % chnl_num;
	
	for(i=num;i<chnl_num+num;i++) {
		if (i>=chnl_num)
			i=i-chnl_num;
		if(is40MChannel(chnl_list,chnl_num,chnl_list[i]))
			return chnl_list[i];
	}
	return -1;
}


int get_available_channel(struct rtl8192cd_priv *priv)
{
	int i, reg;
	struct channel_list *ch_5g_lst=NULL;

	priv->available_chnl_num = 0;
	reg = priv->pmib->dot11StationConfigEntry.dot11RegDomain;

	if ((reg < DOMAIN_FCC) || (reg >= DOMAIN_MAX))
		return FAIL;

	if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11B | WIRELESS_11G) || 
		((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
			!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11A))) {
#if 0 //def AC2G_256QAM
		if(is_ac2g(priv)) //if 2.4G + 11ac mode, force available channels = 1, 5, 9, 13
		{
				priv->available_chnl[0]=1;
				priv->available_chnl[1]=5;
				priv->available_chnl[2]=9;
				priv->available_chnl[3]=13;
				priv->available_chnl_num=4;
		}
		else
#endif
		{
			for (i=0; i<reg_channel_2_4g[reg-1].len; i++)
				priv->available_chnl[i] = reg_channel_2_4g[reg-1].channel[i];
			priv->available_chnl_num += reg_channel_2_4g[reg-1].len;
		}
	}

	if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A) {

		ch_5g_lst = reg_channel_5g_full_band;

		for (i=0; i<ch_5g_lst[reg-1].len; i++) {
				if(is_available_channel(priv,ch_5g_lst[reg-1].channel[i]))
					priv->available_chnl[priv->available_chnl_num++] = ch_5g_lst[reg-1].channel[i];
		}
		
		//for (i=0; i<ch_5g_lst[reg-1].len; i++)
		//	priv->available_chnl[priv->available_chnl_num+i] = ch_5g_lst[reg-1].channel[i];
		//priv->available_chnl_num += ch_5g_lst[reg-1].len;

#ifdef DFS
		/* remove the blocked channels from available_chnl[32] */
		if (priv->NOP_chnl_num)
			for (i=0; i<priv->NOP_chnl_num; i++)
				RemoveChannel(priv, priv->available_chnl, &priv->available_chnl_num, priv->NOP_chnl[i]);

		priv->Not_DFS_chnl_num = 0;
		for (i=0; i<reg_channel_5g_not_dfs_band[reg-1].len; i++) {
			if(is_available_NonDFS_channel(priv,reg_channel_5g_not_dfs_band[reg-1].channel[i]))
				priv->Not_DFS_chnl[priv->Not_DFS_chnl_num++] = reg_channel_5g_not_dfs_band[reg-1].channel[i];
			
		}
		
		
#endif
	}

// add by david ---------------------------------------------------
	if (priv->pmib->dot11RFEntry.dot11ch_low ||  priv->pmib->dot11RFEntry.dot11ch_hi) {
		unsigned int tmpbuf[100];
		int num=0;
		for (i=0; i<priv->available_chnl_num; i++) {
			if ( (priv->pmib->dot11RFEntry.dot11ch_low &&
					priv->available_chnl[i] < priv->pmib->dot11RFEntry.dot11ch_low) ||
				(priv->pmib->dot11RFEntry.dot11ch_hi &&
					priv->available_chnl[i] > priv->pmib->dot11RFEntry.dot11ch_hi))
				continue;
			else
				tmpbuf[num++] = priv->available_chnl[i];
		}
		if (num) {
			memcpy(priv->available_chnl, tmpbuf, num*4);
			priv->available_chnl_num = num;
		}
	}
//------------------------------------------------------ 2007-04-14

	return SUCCESS;
}


void cnt_assoc_num(struct rtl8192cd_priv *priv, struct stat_info *pstat, int act, char *func)
{
#ifdef CONFIG_RTL_92D_SUPPORT
	int i;
#endif

	if (act == INCREASE) {
		if (priv->assoc_num <= NUM_STAT) {
			priv->assoc_num++;
#ifdef REVO_MIFI 
			printk("+assoc_num=%d\n", priv->assoc_num);
			kobject_uevent(&((priv->dev)->dev.kobj), KOBJ_CHANGE);
#endif
#ifdef TLN_STATS
			if (priv->assoc_num > priv->wifi_stats.max_sta) {
				priv->wifi_stats.max_sta = priv->assoc_num;
				priv->wifi_stats.max_sta_timestamp = priv->up_time;
			}
#endif
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			if (GET_CHIP_VER(priv) == VERSION_8188E) {
				priv->pshare->total_assoc_num++;
	#if defined(CONFIG_PCI_HCI)
				RTL8188E_AssignTxReportMacId(priv);
				if (priv->pshare->total_assoc_num == 1)
					RTL8188E_ResumeTxReport(priv);
	#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				notify_tx_report_change(priv);
	#endif
			}
#elif defined(SDIO_AP_PS)
            priv->pshare->total_assoc_num++;			
#endif
#if 0
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			if (IS_ROOT_INTERFACE(priv))
#endif
			{
				if (priv->assoc_num > 1)
					check_DIG_by_rssi(priv, 0);	// force DIG temporary off for association after the fist one
			}
#endif
			if (pstat->ht_cap_len) {
				priv->pshare->ht_sta_num++;
				if (priv->pshare->iot_mode_enable && (priv->pshare->ht_sta_num == 1)
#ifdef RTL_MANUAL_EDCA
						&& (priv->pmib->dot11QosEntry.ManualEDCA == 0)
#endif
						) {
#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
						if(IS_OUTSRC_CHIP(priv))
#endif
						ODM_IotEdcaSwitch(ODMPTR, priv->pshare->iot_mode_enable);
#endif
#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
						if(!IS_OUTSRC_CHIP(priv))
#endif
						IOT_EDCA_switch(priv, priv->pmib->dot11BssType.net_work_type, priv->pshare->iot_mode_enable);
#endif
				}

#ifdef WIFI_11N_2040_COEXIST
				if (priv->pmib->dot11nConfigEntry.dot11nCoexist && priv->pshare->is_40m_bw &&
					(priv->pmib->dot11BssType.net_work_type & (WIRELESS_11N|WIRELESS_11G))) {
					if (pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_40M_INTOLERANT_)) {
						if (OPMODE & WIFI_AP_STATE) {
                            setSTABitMap(&priv->force_20_sta, pstat->aid);
#if defined(WIFI_11N_2040_COEXIST_EXT)
                            clearSTABitMap(&priv->pshare->_40m_staMap, pstat->aid);

#endif
						} 
					}
				}
#endif

				check_NAV_prot_len(priv, pstat, 0);
			}
#ifdef CONFIG_RTL_8812_SUPPORT				
			if (GET_CHIP_VER(priv) == VERSION_8812E) {
				UpdateHalMSRRPT8812(priv, pstat, INCREASE);			
				RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
			}
#endif
#if defined(CONFIG_WLAN_HAL_8881A) || defined(CONFIG_WLAN_HAL_8192EE)
            if ((GET_CHIP_VER(priv) == VERSION_8881A) || (GET_CHIP_VER(priv) == VERSION_8192E)) {
               if(pstat->aid <128)
               {
					GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);				
					GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0, REMAP_AID(pstat));
					pstat->bDrop = 0; 
				}
            }
#endif

#if defined(CONFIG_WLAN_HAL_8814AE)
#ifdef HW_FILL_MACID
        // set Drop = 0;
        if (GET_CHIP_VER(priv) == VERSION_8814A)  {
//         if(pstat->aid <128)
//            {
//                 GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
//                 pstat->bDrop = 0;                          
//            }

        //Init fill mac address,Init fill CRC5
        GET_HAL_INTERFACE(priv)->SetTxRPTHandler(priv, REMAP_AID(pstat), TXRPT_VAR_MAC_ADDRESS, &pstat->hwaddr);                         
        GET_HAL_INTERFACE(priv)->SetCRC5ToRPTBufferHandler(priv,CRC5(&pstat->hwaddr,6), REMAP_AID(pstat),1);                           
        }       
#endif //#ifdef HW_FILL_MACID            
#endif


		} else {
			DEBUG_ERR("Association Number Error (%d)!\n", NUM_STAT);
		}
	} else {
		if (priv->assoc_num > 0) {
			priv->assoc_num--;
			printk("-assoc_num=%d\n",priv->assoc_num);
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			if (GET_CHIP_VER(priv) == VERSION_8188E) {
				priv->pshare->total_assoc_num--;
	#if defined(CONFIG_PCI_HCI)
				if (!priv->pshare->total_assoc_num)
					RTL8188E_SuspendTxReport(priv);
				else
					RTL8188E_AssignTxReportMacId(priv);
	#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				notify_tx_report_change(priv);
	#endif
			}
#elif defined(SDIO_AP_PS)
            priv->pshare->total_assoc_num--;		
#endif

#if 0
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			if (IS_ROOT_INTERFACE(priv))
#endif
				if (!priv->assoc_num) {
#ifdef INTERFERENCE_CONTROL
					if (priv->pshare->rf_ft_var.nbi_filter_enable)
						check_NBI_by_rssi(priv, 0xFF);	// force NBI on while no station associated
#else
	#if defined(CONFIG_PCI_HCI)
					check_DIG_by_rssi(priv, 0);	// force DIG off while no station associated
	#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
					notify_check_DIG_by_rssi(priv, 0);
	#endif
#endif
				}
#endif
			if (pstat->ht_cap_len) {
				if (--priv->pshare->ht_sta_num < 0) {
					printk("ht_sta_num error\n");  // this should not happen
				} else {
					if (priv->pshare->iot_mode_enable && !priv->pshare->ht_sta_num
#ifdef RTL_MANUAL_EDCA
							&& (priv->pmib->dot11QosEntry.ManualEDCA == 0)
#endif
							) {
#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
							if(IS_OUTSRC_CHIP(priv))
#endif
							ODM_IotEdcaSwitch(ODMPTR, priv->pshare->iot_mode_enable);
#endif

#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
							if(!IS_OUTSRC_CHIP(priv))
#endif
							IOT_EDCA_switch(priv, priv->pmib->dot11BssType.net_work_type, priv->pshare->iot_mode_enable);
#endif
					}
#ifdef WIFI_11N_2040_COEXIST
					if (priv->pmib->dot11nConfigEntry.dot11nCoexist && priv->pshare->is_40m_bw &&
						(priv->pmib->dot11BssType.net_work_type & (WIRELESS_11N|WIRELESS_11G))) {
						if (pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_40M_INTOLERANT_)) {
							if (OPMODE & WIFI_AP_STATE) {
                                clearSTABitMap(&priv->force_20_sta, pstat->aid);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
                                update_RAMask_to_FW(priv, 0);
#endif
							}
						}
					}
#endif

					check_NAV_prot_len(priv, pstat, 1);
				}
			}
#ifdef CONFIG_RTL_8812_SUPPORT				
			if (GET_CHIP_VER(priv) == VERSION_8812E) {
				UpdateHalMSRRPT8812(priv, pstat, DECREASE);
			}
#endif
#if defined(CONFIG_WLAN_HAL_8881A) || defined(CONFIG_WLAN_HAL_8192EE)
            if ((GET_CHIP_VER(priv) == VERSION_8881A) || (GET_CHIP_VER(priv) == VERSION_8192E)) {
                if(pstat->aid <127)
                {
                    GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, DECREASE);
                    pstat->bDrop = 1;                      
                }
            }
#endif
#if defined(CONFIG_WLAN_HAL_8814AE)
#ifdef HW_FILL_MACID
        if (GET_CHIP_VER(priv) == VERSION_8814A)  {
           // if(pstat->aid <127)
           // {
           //     GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, DECREASE);
           //     pstat->bDrop = 1;                      
           // }
                               // Set hwaddr zero
        u1Byte hwaddr = {0x00,0x00,0x00,0x00,0x00,0x00};
        GET_HAL_INTERFACE(priv)->SetTxRPTHandler(priv, REMAP_AID(pstat), TXRPT_VAR_MAC_ADDRESS, &hwaddr);                         
        GET_HAL_INTERFACE(priv)->SetCRC5ToRPTBufferHandler(priv,CRC5(&hwaddr,6), REMAP_AID(pstat),0); 
        }
#endif //#ifdef HW_FILL_MACID                                                  
#endif

		} else {
			DEBUG_ERR("Association Number Error (0)!\n");
		}
	}

#ifdef CONFIG_RTL_92D_SUPPORT
	if (GET_CHIP_VER(priv) == VERSION_8192D){
		for (i=NUM_STAT-1; i>=0 ; i--){
			if (priv->pshare->aidarray[i]!=NULL){
				if (priv->pshare->aidarray[i]->used)
						break;
			}
		}
		priv->pshare->max_fw_macid = priv->pshare->aidarray[i]->station.aid+1; // fw check macid num from 1~32, so we add 1 to index.
		if (priv->pshare->max_fw_macid > (NUM_STAT+1))
			priv->pshare->max_fw_macid = (NUM_STAT+1);
	}
#endif

	DEBUG_INFO("assoc_num%s(%d) in %s %02X%02X%02X%02X%02X%02X\n",
		act?"++":"--",
		priv->assoc_num,
		func,
		pstat->hwaddr[0],
		pstat->hwaddr[1],
		pstat->hwaddr[2],
		pstat->hwaddr[3],
		pstat->hwaddr[4],
		pstat->hwaddr[5]);
}


/*
 * Use this function to get the number of associated station, no matter
 * it is expired or not. And don't count WDS peers in.
 */
int get_assoc_sta_num(struct rtl8192cd_priv *priv, int mode)
{
	struct list_head *phead, *plist;
	struct stat_info *pstat;
	int sta_num;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	sta_num = 0;
	phead = &priv->asoc_list;
	
	SMP_LOCK_ASOC_LIST(flags);
	
	plist = phead->next;
	while (plist != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;

#ifdef CONFIG_RTK_MESH        
		if(mode == 1) {
			if(isPossibleNeighbor(pstat))
			continue;
		}
#endif

		if (pstat->state & WIFI_ASOC_STATE) { 
			sta_num++;
		} 
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	return sta_num;
}


void event_indicate(struct rtl8192cd_priv *priv, unsigned char *mac, int event)
{
#ifdef __KERNEL__
#ifdef USE_CHAR_DEV
	if (priv->pshare->chr_priv && priv->pshare->chr_priv->asoc_fasync)
		kill_fasync(&priv->pshare->chr_priv->asoc_fasync, SIGIO, POLL_IN);
#endif
#ifdef USE_PID_NOTIFY
	if (priv->pshare->wlanapp_pid > 0)
#ifdef LINUX_2_6_27_
	{
		kill_pid(_wlanapp_pid, SIGIO, 1);
	}
#else
		kill_proc(priv->pshare->wlanapp_pid, SIGIO, 1);
#endif
#endif
#endif

#ifdef __DRAYTEK_OS__
	if (event == 2)
		cb_disassoc_indicate(priv->dev, mac);
#endif

#ifdef GREEN_HILL
	extern void indicate_to_upper(int reason, unsigned char *addr);
	if (event > 0)
		indicate_to_upper(event, mac);
#endif

#ifdef __ECOS
#ifdef RTLPKG_DEVS_ETH_RLTK_819X_IWCONTROL
    extern cyg_flag_t iw_flag;
    cyg_flag_setbits(&iw_flag, 0x1);

#else
#ifdef RTLPKG_DEVS_ETH_RLTK_819X_WLAN_WPS
	extern cyg_flag_t wsc_flag;
	cyg_flag_setbits(&wsc_flag, 0x1);
#endif
#endif
#endif
}

#ifdef WIFI_HAPD
int event_indicate_hapd(struct rtl8192cd_priv *priv, unsigned char *mac, int event, unsigned char *extra)
{
	struct net_device	*dev = (struct net_device *)priv->dev;
	union iwreq_data wreq;

	if(OPMODE & WIFI_AP_STATE)
	{

		printk("event_indicate_hapd +++, event =0x%x\n", event);

	memset(&wreq, 0, sizeof(wreq));

	if(event == HAPD_EXIRED)
		{
			memcpy(wreq.addr.sa_data, mac, 6);
			wireless_send_event(dev, IWEVEXPIRED, &wreq, NULL);
			return 0;
		}
	else if(event == HAPD_REGISTERED)
		{
			memcpy(wreq.addr.sa_data, mac, 6);
			wireless_send_event(dev, IWEVREGISTERED, &wreq, NULL);
			return 0;
		}
	else if(event == HAPD_MIC_FAILURE)
		{
			char buf[6];
			memcpy(buf, mac, 6);
			wreq.data.flags = event;
			wreq.data.length = 6;
			wireless_send_event(dev, IWEVCUSTOM, &wreq, buf);
			return 0;
		}
	else if(event == HAPD_WPS_PROBEREQ)
		{
			wreq.data.flags = event;
			wreq.data.length = sizeof(struct _DOT11_PROBE_REQUEST_IND);
			wireless_send_event(dev, IWEVGENIE, &wreq, extra); //IW_CUSTOM_MAX is 256, can NOT afford  _DOT11_PROBE_REQUEST_IND
					return 0;
		}		
	else{
			//Not used yet
			wreq.data.flags = event;
			wireless_send_event(dev, IWEVCUSTOM, &wreq, extra);
			return 0;
		}
	}

	return -1;
}
#endif

#ifdef WIFI_WPAS
int event_indicate_wpas(struct rtl8192cd_priv *priv, unsigned char *mac, int event, unsigned char *extra)
{
	struct net_device	*dev = (struct net_device *)priv->dev;
	union iwreq_data wreq;

	if(OPMODE & WIFI_STATION_STATE)
	{
		printk("event_indicate_wpas +++ event = 0x%x\n", event);
		
		memset(&wreq, 0, sizeof(wreq));

		if(event == WPAS_EXIRED)
			{
				memcpy(wreq.addr.sa_data, mac, 6);
				wireless_send_event(dev, IWEVEXPIRED, &wreq, NULL);
				return 0;
			}
		else if(event == WPAS_REGISTERED)
			{
				memcpy(wreq.addr.sa_data, mac, 6);
				wireless_send_event(dev, SIOCGIWAP, &wreq, NULL);
				return 0;
			}
		else if(event == WPAS_MIC_FAILURE)
			{
				char buf[6];
				memcpy(buf, mac, 6);
				wreq.data.flags = event;
				wreq.data.length = 6;
				wireless_send_event(dev, IWEVCUSTOM, &wreq, buf);
				return 0;
			}
		else if(event == WPAS_ASSOC_INFO)
			{
				wreq.data.flags = event;
				wreq.data.length = sizeof(struct _WPAS_ASSOCIATION_INFO);
				wireless_send_event(dev, IWEVCUSTOM, &wreq, extra); //IW_CUSTOM_MAX is 256, can NOT afford  _DOT11_PROBE_REQUEST_IND
				return 0;
			}	
		else if(event == WPAS_SCAN_DONE)
			{
				wireless_send_event(dev, SIOCGIWSCAN, &wreq, NULL);
				return 0;
			}	
#ifdef WIFI_WPAS_CLI 		
		else if (event == WPAS_DISCON ) 
		     {
			         wreq.ap_addr.sa_family = ARPHRD_ETHER;
			         memset(wreq.ap_addr.sa_data, 0, ETH_ALEN);
			         wireless_send_event(dev, SIOCGIWAP, &wreq, NULL);			
			         return 0;
		     }
#endif		
		else
			{
				//Not used yet
				wreq.data.flags = event;
				wireless_send_event(dev, IWEVCUSTOM, &wreq, extra);
				return 0;
			}
	}

	return -1;
}
#endif


#ifdef CONFIG_RTL_WAPI_SUPPORT
void wapi_event_indicate(struct rtl8192cd_priv *priv)
{
	#ifdef LINUX_2_6_27_
	struct pid *pid;
	#endif

	if (priv->pshare->wlanwapi_pid > 0)
	{
#ifdef LINUX_2_6_27_
		kill_pid(_wlanwapi_pid, SIGIO, 1);
#else
		kill_proc(priv->pshare->wlanwapi_pid, SIGIO, 1);
#endif
	}
}
#endif

#ifdef USE_WEP_DEFAULT_KEY
void init_DefaultKey_Enc(struct rtl8192cd_priv *priv, unsigned char *key, int algorithm)
{
	unsigned char defaultmac[4][6];
	int i;

	memset(defaultmac, 0, sizeof(defaultmac));
	for(i=0; i<4; i++)
		defaultmac[i][5] = i;

	for(i=0; i<4; i++)
	{
		CamDeleteOneEntry(priv, defaultmac[i], i, 1);
		if (key == NULL)
			CamAddOneEntry(priv, defaultmac[i], i,
					(priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm)<<2,
					1, priv->pmib->dot11DefaultKeysTable.keytype[i].skey);
		else
			CamAddOneEntry(priv, defaultmac[i], i,
					algorithm<<2,
					1, key);
	}
	priv->pshare->CamEntryOccupied += 4;
}
#endif


#ifdef UNIVERSAL_REPEATER
//
// Disable AP function in virtual interface
//
void disable_vxd_ap(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if ((priv==NULL) || !(priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE))
		return;

	if (!(priv->drv_state & DRV_STATE_VXD_AP_STARTED))
		return;
	else
		priv->drv_state &= ~DRV_STATE_VXD_AP_STARTED;

	DEBUG_INFO("Disable vxd AP\n");

	if (IS_DRV_OPEN(priv))
		rtl8192cd_close(priv->dev);

	SAVE_INT_AND_CLI(flags);

#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
    	GET_HAL_INTERFACE(priv)->DisableVXDAPHandler(priv);
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
#if defined(CONFIG_PCI_HCI) || (defined(CONFIG_USB_HCI) && defined(CONFIG_SUPPORT_USB_INT) && defined(CONFIG_INTERRUPT_BASED_TXBCN))
#ifdef CONFIG_RTL_88E_SUPPORT
	if (GET_CHIP_VER(priv)==VERSION_8188E) {
		priv->pshare->InterruptMask &= ~(HIMR_88E_BcnInt | HIMR_88E_TBDOK | HIMR_88E_TBDER);
		RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask);
	} else
#endif
	{
		RTL_W32(HIMR, RTL_R32(HIMR) & ~(HIMR_BCNDOK0));
	}
#endif

	//RTL_W16(ATIMWND, 2);
	RTL_W32(CR, (RTL_R32(CR) & ~(NETYPE_Mask << NETYPE_SHIFT)) | ((NETYPE_NOLINK & NETYPE_Mask) << NETYPE_SHIFT));
	}

	RESTORE_INT(flags);
}


//
// Enable AP function in virtual interface
//
void enable_vxd_ap(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if ((priv==NULL) || !(priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) ||
		!(priv->drv_state & DRV_STATE_VXD_INIT))
		return;

	if (priv->drv_state & DRV_STATE_VXD_AP_STARTED)
		return;
	else
		priv->drv_state |= DRV_STATE_VXD_AP_STARTED;

	DEBUG_INFO("Enable vxd AP\n");

	priv->pmib->dot11RFEntry.dot11channel = GET_ROOT(priv)->pmib->dot11Bss.channel;
	//priv->pmib->dot11BssType.net_work_type = GET_ROOT_PRIV(priv)->oper_band;
	priv->pmib->dot11BssType.net_work_type = GET_ROOT(priv)->pmib->dot11BssType.net_work_type &
		GET_ROOT(priv)->pmib->dot11Bss.network;

	if (!IS_DRV_OPEN(priv))
		rtl8192cd_open(priv->dev);
	else {
		//priv->oper_band = priv->pmib->dot11BssType.net_work_type;
		validate_oper_rate(priv);
		get_oper_rate(priv);
	}

	memcpy(priv->pmib->dot11StationConfigEntry.dot11Bssid, GET_ROOT(priv)->pmib->dot11OperationEntry.hwaddr, MACADDRLEN);
	memcpy(GET_MY_HWADDR, priv->pmib->dot11StationConfigEntry.dot11Bssid, MACADDRLEN);
	memcpy(priv->pmib->dot11Bss.bssid, priv->pmib->dot11StationConfigEntry.dot11Bssid, MACADDRLEN);

	SAVE_INT_AND_CLI(flags);
	priv->ht_cap_len = 0;
	init_beacon(priv);

#if defined(CONFIG_PCI_HCI) || (defined(CONFIG_USB_HCI) && defined(CONFIG_SUPPORT_USB_INT) && defined(CONFIG_INTERRUPT_BASED_TXBCN))
#ifdef CONFIG_RTL_88E_SUPPORT
	if (GET_CHIP_VER(priv)==VERSION_8188E) {
		priv->pshare->InterruptMask |= HIMR_88E_BcnInt | HIMR_88E_TBDOK | HIMR_88E_TBDER;
		RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask);
	} else
#endif
	{
		RTL_W32(HIMR, RTL_R32(HIMR) | HIMR_BCNDOK0);
	}
#endif

	//RTL_W16(ATIMWND, 0x0030);
	RTL_W32(CR, (RTL_R32(CR) & ~(NETYPE_Mask << NETYPE_SHIFT)) | ((NETYPE_AP & NETYPE_Mask) << NETYPE_SHIFT));
#ifdef CONFIG_RTL_92C_SUPPORT
	if (!IS_TEST_CHIP(priv))
#endif
	{
		RTL_W8(0x422, RTL_R8(0x422) | BIT(6));
		RTL_W8(BCN_CTRL, 0); 
		RTL_W8(0x553, 1); 
		RTL_W8(BCN_CTRL, DIS_TSF_UPDATE_N| EN_BCN_FUNCTION | DIS_SUB_STATE_N | EN_TXBCN_RPT);
	}

	RESTORE_INT(flags);
}
#endif // UNIVERSAL_REPEATER


#ifdef GBWC
void rtl8192cd_GBWC_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct sk_buff *pskb;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	if (priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_DISABLE)
		return;

	priv->GBWC_consuming_Q = 1;

	// clear bandwidth control counter
	priv->GBWC_tx_count = 0;
	priv->GBWC_rx_count = 0;

	// consume Tx queue
	while(1)
	{
		pskb = (struct sk_buff *)deque(priv, &(priv->GBWC_tx_queue.head), &(priv->GBWC_tx_queue.tail),
			(unsigned long)(priv->GBWC_tx_queue.pSkb), NUM_TXPKT_QUEUE);

		if (pskb == NULL)
			break;

#ifdef ENABLE_RTL_SKB_STATS
		rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif

		if (rtl8192cd_start_xmit_noM2U(pskb, pskb->dev))
			rtl_kfree_skb(priv, pskb, _SKB_TX_);
	}

	// consume Rx queue
	while(1)
	{
		pskb = (struct sk_buff *)deque(priv, &(priv->GBWC_rx_queue.head), &(priv->GBWC_rx_queue.tail),
			(unsigned long)(priv->GBWC_rx_queue.pSkb), NUM_TXPKT_QUEUE);

		if (pskb == NULL)
			break;

		rtl_netif_rx(priv, pskb, (struct stat_info *)*(unsigned int *)&(pskb->cb[4]));
	}

	priv->GBWC_consuming_Q = 0;

	mod_timer(&priv->GBWC_timer, jiffies + GBWC_TO);
}
#endif


unsigned char fw_was_full(struct rtl8192cd_priv *priv)
{
    struct list_head *phead;
    struct list_head *plist;
    struct stat_info *pstat;
#ifdef SMP_SYNC
    unsigned long flags = 0;
#endif
    unsigned char is_full;

    phead = &priv->asoc_list;
    if(list_empty(phead))
        return 0;

    is_full = 0;

    SMP_LOCK_ASOC_LIST(flags);

    plist = phead->next;
    while (plist != phead) {
        pstat = list_entry(plist, struct stat_info, asoc_list);
        plist = plist->next;
        if(	pstat->sta_in_firmware != 1) 
        {
            is_full = 1;
            goto exit;
        }
    }

exit:
    SMP_UNLOCK_ASOC_LIST(flags);
    return is_full;
}


int realloc_RATid(struct rtl8192cd_priv *priv)
{
    struct list_head *phead;
    struct list_head *plist;
    struct stat_info *pstat =NULL, *pstat_chosen = NULL;
    unsigned int max_through_put = 0;
    unsigned int have_chosen = 0;
#ifdef SMP_SYNC
    unsigned long flags = 0;
#endif

    phead = &priv->asoc_list;
    if(list_empty(phead))
        return 0;

    SMP_LOCK_ASOC_LIST(flags);

    plist = phead->next;
    while (plist != phead) {
        int temp_through_put ;
        pstat = list_entry(plist, struct stat_info, asoc_list);
        plist = plist->next;

        if (pstat->sta_in_firmware == 1)// STA has rate adaptive
            continue;

        temp_through_put =  pstat->tx_avarage + pstat->rx_avarage;

        if (temp_through_put >= max_through_put){
            pstat_chosen = pstat;
            max_through_put = temp_through_put;
            have_chosen = 1;
        }
    }

    SMP_UNLOCK_ASOC_LIST(flags);

    if (have_chosen == 0)
    return 0;

#if defined(CONFIG_PCI_HCI)
#ifdef CONFIG_RTL_88E_SUPPORT
    if (GET_CHIP_VER(priv)==VERSION_8188E) {
#ifdef TXREPORT
        add_RATid(priv, pstat);
#endif
    } else
#endif
    {	
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)	
        if(CHIP_VER_92X_SERIES(priv))
            add_update_RATid(priv, pstat_chosen);
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
        if(GET_CHIP_VER(priv)== VERSION_8812E){
            UpdateHalRAMask8812(priv, pstat_chosen, 3);			
        }
#endif
#ifdef  CONFIG_WLAN_HAL
        if (IS_HAL_CHIP(priv)){
            GET_HAL_INTERFACE(priv)->UpdateHalRAMaskHandler(priv, pstat_chosen, 3);
        }
#endif
    }
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
    update_STA_RATid(priv, pstat_chosen);
#endif

    return 1;
}


void update_remapAid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int array_idx;

    if((pstat->sta_in_firmware == 1) || (pstat->sta_in_firmware == -1))
        return; /*already exists, just return*/

    if(priv->pshare->fw_free_space) {
        /*find an empty slot*/
    	for (array_idx = 0; array_idx < NUM_STAT; array_idx++) {
    		if (priv->pshare->remapped_aidarray[array_idx] == 0)
    			break;
    	}

    	if (array_idx == NUM_STAT) {
            /*WARNING:  THIS SHOULD NOT HAPPEN*/
            printk("add AID fail!!\n");
            BUG();
    		return;
        }
        
    
        pstat->remapped_aid = array_idx + 1;
        priv->pshare->remapped_aidarray[array_idx] = pstat->aid; 
        pstat->sta_in_firmware = 1; // this value will updated in expire_timer
        priv->pshare->fw_free_space --;
    }
    else { /* free space run out, share the same aid*/
        pstat->remapped_aid = priv->pshare->fw_support_sta_num;
        pstat->sta_in_firmware = 0;
    }
}


void release_remapAid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
    int i;
    if (pstat->sta_in_firmware == 1)
    {
        for(i = 0; i < NUM_STAT; i++) {
            if(priv->pshare->remapped_aidarray[i] == pstat->aid){
                priv->pshare->remapped_aidarray[i] = 0;                
                break;
            }
        }        
        priv->pshare->fw_free_space ++;
        pstat->sta_in_firmware = -1;
        DEBUG_INFO("Remove id %d from ratr\n", pstat->aid);        
    }
}


unsigned int is_h2c_buf_occupy(struct rtl8192cd_priv *priv)
{
	 unsigned int occupied = 0;

	if (
#ifdef CONFIG_RTL_92C_SUPPORT
		(IS_TEST_CHIP(priv) && RTL_R8(0x1c0+priv->pshare->fw_q_fifo_count)) ||
#endif
		(RTL_R8(0x1cc) & BIT(priv->pshare->fw_q_fifo_count)))
		occupied++;

	return occupied;
}

#if defined(UNIVERSAL_REPEATER)
int under_apmode_repeater(struct rtl8192cd_priv *priv)
{
	int ret = 0;

	if(IS_ROOT_INTERFACE(priv))
	{
		if(IS_DRV_OPEN(GET_VXD_PRIV(priv))) 
		{
			if((priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
				(GET_VXD_PRIV(priv)->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE))
					ret = 1;
		}
	}
	else if(IS_VXD_INTERFACE(priv))
	{
		if(IS_DRV_OPEN(priv)) 
		{
			if((GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
				(priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE))
					ret = 1;
		}
	}

	return ret;
}
#endif

short signin_h2c_cmd(struct rtl8192cd_priv *priv, unsigned int content, unsigned short ext_content)
{
	int c=0;

#ifdef MP_TEST
	if (priv->pshare->rf_ft_var.mp_specific)
		goto SigninFAIL;
#endif

	/*
	 *	Check if h2c cmd signin buffer is occupied, 
	 *	for Power Saving related functions only
	 */
	if ((content & 0x7f) < H2C_CMD_RSSI) {
		while (is_h2c_buf_occupy(priv)) {
		delay_us(10);
		if(++c ==30)
			goto SigninFAIL;
	}
	}

	/*
		 * signin reg in order to fit hw requirement
		 */
		if(content & BIT(7))
			RTL_W16(0x88+(priv->pshare->fw_q_fifo_count*2), ext_content);

	RTL_W32(HMEBOX_0+(priv->pshare->fw_q_fifo_count*4), content);

	//printk("(smcc) sign in h2c %x\n", HMEBOX_0+(priv->pshare->fw_q_fifo_count*4));

#if defined(TESTCHIP_SUPPORT) && defined(CONFIG_RTL_92C_SUPPORT)
	/*
	 * set own bit
	 */
	if(IS_TEST_CHIP(priv))
	RTL_W8(0x1c0+priv->pshare->fw_q_fifo_count, 1);
#endif

	/*
	 * rollover ring buffer count
	 */
	if (++priv->pshare->fw_q_fifo_count > 3)
		priv->pshare->fw_q_fifo_count = 0;

	return 0;
	
SigninFAIL:
	return 1;
}


void set_ps_cmd(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	unsigned int content = 0;

	if(! CHIP_VER_92X_SERIES(priv))
		return;

	SAVE_INT_AND_CLI(flags);

	/*
	 * set ps state
	 */
	 if (pstat->state & WIFI_SLEEP_STATE)
	 	content |= BIT(24);

	/*
	 * set macid
	 */
	content |= REMAP_AID(pstat) << 8;

	/*
	 * set cmd id
	 */
	 content |= H2C_CMD_PS;

	signin_h2c_cmd(priv, content, 0);

	RESTORE_INT(flags);
}


#ifdef CONFIG_PCI_HCI
void add_ps_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct stat_info *pstat = NULL;
	unsigned int set_timer = 0;
	unsigned long flags = 0;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	if (timer_pending(&priv->add_ps_timer))
		del_timer_sync(&priv->add_ps_timer);

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1))
			return;
#endif

	if (!list_empty(&priv->addps_list)) {
		pstat = list_entry(priv->addps_list.next, struct stat_info, addps_list);
		if (!pstat)
	return ;

		if (!is_h2c_buf_occupy(priv)) {
			set_ps_cmd(priv, pstat);
			if (!list_empty(&pstat->addps_list)) {
				SAVE_INT_AND_CLI(flags);
				SMP_LOCK(flags);
				list_del_init(&pstat->addps_list);
				RESTORE_INT(flags);
				SMP_UNLOCK(flags);
			}

			if (!list_empty(&priv->addps_list))
				set_timer++;
		} else {
			set_timer++;
		}
	}

	if (set_timer)
		mod_timer(&priv->add_ps_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
}


#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
void add_update_ps(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
//#ifdef CONFIG_RTL_8812_SUPPORT
	if(! CHIP_VER_92X_SERIES(priv))
		return;
//#endif		
	if (is_h2c_buf_occupy(priv)) {
		if (list_empty(&pstat->addps_list)) {
			SAVE_INT_AND_CLI(flags);
			list_add_tail(&(pstat->addps_list), &(priv->addps_list));
			RESTORE_INT(flags);

			if (!timer_pending(&priv->add_ps_timer))
				mod_timer(&priv->add_ps_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
		}
	} else {
		set_ps_cmd(priv, pstat);
	}
}
#endif
#endif // CONFIG_PCI_HCI


void update_intel_sta_bitmap(struct rtl8192cd_priv *priv, struct stat_info *pstat, int release)
{
	if (pstat->IOTPeer != HT_IOT_PEER_INTEL)
		return;
	
	if (release) {
		clearSTABitMap(&priv->pshare->intel_sta_bitmap, pstat->aid);
	} else {	// join
		setSTABitMap(&priv->pshare->intel_sta_bitmap, pstat->aid);
	}
}

#if defined(WIFI_11N_2040_COEXIST_EXT)

void update_40m_staMap(struct rtl8192cd_priv *priv, struct stat_info *pstat, int release)
{
	if(pstat) {
		if(release || (pstat->tx_bw == HT_CHANNEL_WIDTH_20))
			clearSTABitMap(&priv->pshare->_40m_staMap, pstat->aid);
		else
			setSTABitMap(&priv->pshare->_40m_staMap, pstat->aid);

	}
}

void checkBandwidth(struct rtl8192cd_priv *priv)
{
	int i;
	unsigned int _40m_stamap = orSTABitMap(&priv->pshare->_40m_staMap);
	
	int FA_counter = priv->pshare->FA_total_cnt;

	if(!priv->pshare->rf_ft_var.bws_enable)
		return;

#ifdef MP_TEST
	if ( (OPMODE & WIFI_MP_STATE)|| priv->pshare->rf_ft_var.mp_specific)
		return ;
#endif

	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
		return ;

#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled)
		return;
#endif

	if (!(OPMODE & WIFI_AP_STATE))
		return;

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
		return;
#endif		

    if (timer_pending(&priv->ss_timer))
        return;

#if	defined(USE_OUT_SRC)
	if(IS_OUTSRC_CHIP(priv))
		FA_counter = ODMPTR->FalseAlmCnt.Cnt_all;
#endif

	if ((priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20_40)  && (!_40m_stamap) ) {
		if (priv->pmib->dot11nConfigEntry.dot11nUse40M == HT_CHANNEL_WIDTH_10)
			priv->pshare->CurrentChannelBW = HT_CHANNEL_WIDTH_10;
		else if (priv->pmib->dot11nConfigEntry.dot11nUse40M == HT_CHANNEL_WIDTH_5)
			priv->pshare->CurrentChannelBW = HT_CHANNEL_WIDTH_5;
		else
			priv->pshare->CurrentChannelBW = HT_CHANNEL_WIDTH_20;		
		SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
		SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
		update_RAMask_to_FW(priv, 1);
#endif					
	} 
	if( priv->pmib->dot11nConfigEntry.dot11nCoexist) {
		if((FA_counter> priv->pshare->rf_ft_var.bws_Thd)&&((RTL_R8(0xc50) & 0x7f) >= 0x32)) {
			if(priv->pshare->is_40m_bw != HT_CHANNEL_WIDTH_20) {
				priv->pshare->is_40m_bw = HT_CHANNEL_WIDTH_20;
				priv->ht_cap_len = 0;	// reconstruct ie
				if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
					construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);				
			}
		} else {
			if(priv->pshare->is_40m_bw != priv->pmib->dot11nConfigEntry.dot11nUse40M) {
				priv->pshare->is_40m_bw = priv->pmib->dot11nConfigEntry.dot11nUse40M;
				priv->ht_cap_len = 0;				// reconstruct ie
				if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
					construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);
			}
		}		
	}		
	if (!priv->ss_req_ongoing)
	if( _40m_stamap && (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20)  &&
		((priv->pmib->dot11nConfigEntry.dot11nUse40M != HT_CHANNEL_WIDTH_20))) {
			if(priv->pmib->dot11nConfigEntry.dot11nCoexist) {
				priv->pshare->is_40m_bw = priv->pmib->dot11nConfigEntry.dot11nUse40M;
				priv->ht_cap_len = 0;				// reconstruct ie
				if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
					construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);				
			}
			priv->pshare->CurrentChannelBW = priv->pmib->dot11nConfigEntry.dot11nUse40M;
			SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
			SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
			update_RAMask_to_FW(priv, 1);
#endif
	}
}
#endif


#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
#ifdef CONFIG_PCI_HCI
void add_RATid_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct stat_info *pstat = NULL;
	unsigned int set_timer = 0;
	unsigned long flags = 0;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	if (timer_pending(&priv->add_RATid_timer))
		del_timer_sync(&priv->add_RATid_timer);

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1))
			return;
#endif

	if (!list_empty(&priv->addRAtid_list)) {
		pstat = list_entry(priv->addRAtid_list.next, struct stat_info, addRAtid_list);
		if (!pstat)
			return;

		if (!is_h2c_buf_occupy(priv)) {
			add_RATid(priv, pstat);
			if (!list_empty(&pstat->addRAtid_list)) {
				SAVE_INT_AND_CLI(flags);
				SMP_LOCK(flags);
				list_del_init(&pstat->addRAtid_list);
				RESTORE_INT(flags);
				SMP_UNLOCK(flags);
			}

			if (!list_empty(&priv->addRAtid_list))
				set_timer++;
		} else {
			set_timer++;
		}
	}

	if (set_timer)
		mod_timer(&priv->add_RATid_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
}


void add_update_RATid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (is_h2c_buf_occupy(priv)) {
		if (list_empty(&pstat->addRAtid_list)) {
			SAVE_INT_AND_CLI(flags);
			list_add_tail(&(pstat->addRAtid_list), &(priv->addRAtid_list));
			RESTORE_INT(flags);

			if (!timer_pending(&priv->add_RATid_timer))
				mod_timer(&priv->add_RATid_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
		}
	} else {
			add_RATid(priv, pstat);
	}
}
#endif // CONFIG_PCI_HCI
#endif

#ifdef SDIO_AP_PS
int set_ap_ps_mode(struct rtl8192cd_priv *priv, unsigned char *data)
{
	int mode = _atoi(data, 16);

	if (strlen(data) == 0) {
		return 0;
	}	    

	if ( mode == 0x00 ) {
		priv->pshare->dis_ps_mode = 0;
	} else {
		priv->pshare->dis_ps_mode = 1;    
		if ( priv->pshare->pwr_state == RTW_STS_SUSPEND ) {
			GET_ROOT(priv)->offload_function_ctrl = 0;
			cmd_set_ap_offload(priv, 0);
			priv->pshare->pwr_state = RTW_STS_NORMAL;
		}
	}

	mod_timer(&priv->pshare->ps_timer, jiffies + POWER_DOWN_T0);
	return;
}
#endif

void send_h2c_cmd_detect_wps_gpio(struct rtl8192cd_priv *priv, unsigned int gpio_num, unsigned int enable, unsigned int high_active)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	unsigned int content = 0;

	SAVE_INT_AND_CLI(flags);
	
	content = gpio_num << 16;
	
	/*
	 * enable firmware to detect wps gpio
	 */
	if (enable)
		content |= BIT(8);
	
	/*
	 * rising edge trigger
	 */
	if (high_active)
		content |= BIT(9);

	/*
	 * set cmd id
	 */
	content |= H2C_CMD_AP_WPS_CTRL;

	signin_h2c_cmd(priv, content, 0);
	printk("signin ap_wps_ctrl h2c: 0x%08X\n", content);
	
	RESTORE_INT(flags);
}

#ifdef RTK_QUE
void rtk_queue_init(struct ring_que *que)
{
	memset(que, '\0', sizeof(struct ring_que));
	que->qmax = MAX_PRE_ALLOC_SKB_NUM;
}

static int rtk_queue_tail(struct rtl8192cd_priv *priv, struct ring_que *que, struct sk_buff *skb)
{
	int next;
	unsigned long x;

	SAVE_INT_AND_CLI(x);
	SMP_LOCK_SKB(x);

	if (que->head == que->qmax)
		next = 0;
	else
		next = que->head + 1;

	if (que->qlen >= que->qmax || next == que->tail) {
		printk("%s: ring-queue full!\n", __FUNCTION__);
		RESTORE_INT(x);
		SMP_UNLOCK_SKB(x);
		return 0;
	}

	que->ring[que->head] = skb;
	que->head = next;
	que->qlen++;

	RESTORE_INT(x);
	SMP_UNLOCK_SKB(x);
	return 1;
}


__IRAM_IN_865X
static struct sk_buff *rtk_dequeue(struct rtl8192cd_priv *priv, struct ring_que *que)
{
	struct sk_buff *skb;
	unsigned long x;

	SAVE_INT_AND_CLI(x);
	SMP_LOCK_SKB(x);

	if (que->qlen <= 0 || que->tail == que->head) {
		RESTORE_INT(x);
		SMP_UNLOCK_SKB(x);
		return NULL;
	}

	skb = que->ring[que->tail];

	if (que->tail == que->qmax)
		que->tail  = 0;
	else
		que->tail++;

	que->qlen--;

	RESTORE_INT(x);
	SMP_UNLOCK_SKB(x);
	return (struct sk_buff *)skb;
}


void free_rtk_queue(struct rtl8192cd_priv *priv, struct ring_que *skb_que)
{
	struct sk_buff *skb;

	while (skb_que->qlen > 0) {
		skb = rtk_dequeue(priv, skb_que);
		if (skb == NULL)
			break;
		dev_kfree_skb_any(skb);
	}
}
#endif // RTK_QUE

#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
    extern int refill_rx_ring_88XX(struct rtl8192cd_priv * priv, struct sk_buff * skb, unsigned char * data, unsigned int q_num, PHCI_RX_DMA_QUEUE_STRUCT_88XX cur_q);
#endif
extern int refill_rx_ring(struct rtl8192cd_priv *priv, struct sk_buff *skb, unsigned char *data);
#endif

#ifdef __ECOS
#ifdef DELAY_REFILL_RX_BUF
int __MIPS16 rtk_wifi_delay_refill(struct sk_buff  *pskb)
{
	struct rtl8192cd_priv *priv;	
	struct rtl8192cd_hw *phw;
	struct sk_buff *skb=(struct sk_buff *)pskb;
	int ret=0;
	if(!(pskb && pskb->priv))
		return ret;
	
	priv=(struct rtl8192cd_priv *)pskb->priv;
 	phw=GET_HW(priv);

#ifdef CONFIG_WLAN_HAL
	unsigned int					q_num;
	PHCI_RX_DMA_MANAGER_88XX		prx_dma;
	PHCI_RX_DMA_QUEUE_STRUCT_88XX	cur_q;
#endif // CONFIG_WLAN_HAL

#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
        if (!(priv->drv_state & DRV_STATE_OPEN)){
            /* return 0 means can't refill (because interface be closed or not opened yet) to rx ring but relesae to skb_poll*/            
            ret=0;
        }else{        
    	    q_num   = 0;
    	    prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
    	    cur_q   = &(prx_dma->rx_queue[q_num]);
    	    ret = refill_rx_ring_88XX(priv, NULL, NULL, q_num, cur_q);
    	    GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
    	    cur_q->rxbd_ok_cnt = 0;
        }
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{
		if (phw->cur_rx_refill != phw->cur_rx) {
		   	ret=refill_rx_ring(priv, NULL, NULL); 
		}
	}
	return ret;
}
#endif
#endif

#if !(defined(__ECOS) && defined(CONFIG_SDIO_HCI))
void refill_skb_queue(struct rtl8192cd_priv *priv)
{
	struct sk_buff *skb;
#ifdef DELAY_REFILL_RX_BUF
 	struct rtl8192cd_hw *phw=GET_HW(priv);

#ifdef CONFIG_WLAN_HAL
    unsigned int                    q_num;
    PHCI_RX_DMA_MANAGER_88XX        prx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q;

    if (IS_HAL_CHIP(priv)) {
        q_num   = 0;
        prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
        cur_q   = &(prx_dma->rx_queue[q_num]);
    }
#endif // CONFIG_WLAN_HAL
#endif

#ifdef NOT_RTK_BSP
	while (skb_queue_len(&priv->pshare->skb_queue) < MAX_PRE_ALLOC_SKB_NUM) 
#else
	while (priv->pshare->skb_queue.qlen < MAX_PRE_ALLOC_SKB_NUM) 
#endif
	{

	#ifdef CONFIG_RTL8190_PRIV_SKB
			skb = dev_alloc_skb_priv(priv, RX_BUF_LEN);
	#else
			skb = dev_alloc_skb(RX_BUF_LEN);
	#endif

		if (skb == NULL) {
//			DEBUG_ERR("dev_alloc_skb() failed!\n");
			return;
		}
#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
		if (IS_HAL_CHIP(priv)) {
	        if (cur_q->cur_host_idx != ((cur_q->host_idx + cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num)) {
	            refill_rx_ring_88XX(priv, skb, NULL, q_num, cur_q);
				continue;
		  	}
		} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif // CONFIG_WLAN_HAL
		{//not HAL
			if (phw->cur_rx_refill != phw->cur_rx) {
				refill_rx_ring(priv, skb, NULL); 
				continue;
			}
		}
#endif

#ifdef RTK_QUE
		rtk_queue_tail(priv, &priv->pshare->skb_queue, skb);
#else
#ifdef __ECOS
		skb_queue_tail(&priv->pshare->skb_queue, skb);
#else
		__skb_queue_tail(&priv->pshare->skb_queue, skb);
#endif
#endif
	}
#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
        GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
        cur_q->rxbd_ok_cnt = 0;
	}
#endif // CONFIG_WLAN_HAL
#endif 
}


__MIPS16
__IRAM_IN_865X
struct sk_buff *alloc_skb_from_queue(struct rtl8192cd_priv *priv)
{
	struct sk_buff *skb=NULL;

#ifdef NOT_RTK_BSP
	if (skb_queue_len(&priv->pshare->skb_queue) < 2)
#else
	if (priv->pshare->skb_queue.qlen == 0) 
#endif
	{
//		struct sk_buff *skb;
#ifdef CONFIG_RTL8190_PRIV_SKB
#ifdef CONCURRENT_MODE
		skb = dev_alloc_skb_priv(priv, RX_BUF_LEN);
#else
		skb = dev_alloc_skb_priv(priv, RX_BUF_LEN);
#endif
#else
		skb = dev_alloc_skb(RX_BUF_LEN);
#endif
		if (skb == NULL) {
			DEBUG_ERR("dev_alloc_skb() failed!\n");
		}

		return skb;
	}
#ifdef RTK_QUE
	skb = rtk_dequeue(priv, &priv->pshare->skb_queue);
#else
#ifdef __ECOS
	skb = skb_dequeue(&priv->pshare->skb_queue);
#else
	skb = __skb_dequeue(&priv->pshare->skb_queue);
#endif
#endif
	if (skb == NULL) {
		DEBUG_ERR("skb_dequeue() failed!\n");
	}

	return skb;
}


void free_skb_queue(struct rtl8192cd_priv *priv, struct sk_buff_head	*skb_que)
{
	struct sk_buff *skb;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	while (skb_que->qlen > 0) {
// 2009.09.08
		SAVE_INT_AND_CLI(flags);
		skb = __skb_dequeue(skb_que);
		RESTORE_INT(flags);
		if (skb == NULL)
			break;
		dev_kfree_skb_any(skb);
	}
}
#endif // !(__ECOS && CONFIG_SDIO_HCI)

#ifdef FAST_RECOVERY
struct backup_info {
	struct aid_obj *sta[NUM_STAT];
	struct Dot11KeyMappingsEntry gkey;
#ifdef WDS
	struct wds_info	wds;
#endif
};

void *backup_sta(struct rtl8192cd_priv *priv)
{
	int i;
	struct backup_info *pBackup;

	pBackup = (struct backup_info *)kmalloc((sizeof(struct backup_info)), GFP_ATOMIC);
	if (pBackup == NULL) {
		printk("%s: kmalloc() failed!\n", __FUNCTION__);
		return NULL;
	}
	memset(pBackup, '\0', sizeof(struct backup_info));
	for (i=0; i<NUM_STAT; i++) {
		if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used) {
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			if (priv !=  priv->pshare->aidarray[i]->priv)
				continue;
#endif
			pBackup->sta[i] = (struct aid_obj *)kmalloc((sizeof(struct aid_obj)), GFP_ATOMIC);
			if (pBackup->sta[i] == NULL) {
				printk("%s: kmalloc(sta) failed!\n", __FUNCTION__);
				for (--i; i>=0; --i) {
					if (pBackup->sta[i]) {
#ifdef CONFIG_RTL_WAPI_SUPPORT
						free_sta_wapiInfo(priv, &pBackup->sta[i]->station);
#endif
						kfree(pBackup->sta[i]);
					}
				}
				kfree(pBackup);
				return NULL;
			}
			memcpy(pBackup->sta[i], priv->pshare->aidarray[i], sizeof(struct aid_obj));
#ifdef CONFIG_RTL_WAPI_SUPPORT
			// prevent backup station.wapiInfo from being freed during recovery preiod
			priv->pshare->aidarray[i]->station.wapiInfo = NULL;
#endif
		}
	}

#ifdef WDS
	memcpy(&pBackup->wds, &priv->pmib->dot11WdsInfo, sizeof(struct wds_info));
#endif
	memcpy(&pBackup->gkey, &priv->pmib->dot11GroupKeysTable, sizeof(struct Dot11KeyMappingsEntry));

	return (void *)pBackup;
}


void restore_backup_sta(struct rtl8192cd_priv *priv, void *pInfo)
{
	unsigned int i, offset;
	struct stat_info *pstat;
	unsigned char	key_combo[32];
	struct backup_info *pBackup=(struct backup_info *)pInfo;
#ifdef CONFIG_RTK_MESH
	unsigned char	is_11s_MP = FALSE;
	unsigned long flags;
#endif
	int retVal;

	for (i=0; i<NUM_STAT; i++) {
		if (pBackup->sta[i]) {

#ifdef CONFIG_RTK_MESH	// Restore Establish MP ONLY
			if ((1 == GET_MIB(priv)->dot1180211sInfo.mesh_enable) && !isSTA2(pBackup->sta[i]->station)) {
				UINT8 State = pBackup->sta[i]->station.mesh_neighbor_TBL.State;

				if ((State == MP_SUPERORDINATE_LINK_UP) || (State == MP_SUBORDINATE_LINK_UP)
						|| (State == MP_SUPERORDINATE_LINK_DOWN) || (State == MP_SUBORDINATE_LINK_DOWN_E))
					is_11s_MP = TRUE;
				else	// is MP, but not establish, Give up.
				{
					kfree(pBackup->sta[i]);
					continue;
				}
			}
#endif

			pstat = alloc_stainfo(priv, pBackup->sta[i]->station.hwaddr, i);
			if (!pstat) {
				printk("%s: alloc_stainfo() failed!\n", __FUNCTION__);
				for (; i<NUM_STAT; i++) {
					if (pBackup->sta[i]) {
#ifdef CONFIG_RTL_WAPI_SUPPORT
						free_sta_wapiInfo(priv, &pBackup->sta[i]->station);
#endif
						kfree(pBackup->sta[i]);
					}
				}
				kfree(pBackup);
				return;
			}

#ifdef CONFIG_RTL_WAPI_SUPPORT
			// free new allocated wapiInfo before restore backup wapiInfo
			if (pstat->wapiInfo) free_sta_wapiInfo(priv, pstat);
#endif
			offset = (unsigned long)(&((struct stat_info *)0)->aid);
			memcpy(((unsigned char *)pstat)+offset,
				((unsigned char *)&pBackup->sta[i]->station)+offset, sizeof(struct stat_info)-offset);
			asoc_list_add(priv, pstat);

#ifdef CONFIG_RTK_MESH
			if (TRUE == is_11s_MP) {
				is_11s_MP = FALSE;
				SMP_LOCK_MESH_MP_HDR(flags);
				list_add_tail(&pstat->mesh_mp_ptr, &priv->mesh_mp_hdr);
				SMP_UNLOCK_MESH_MP_HDR(flags);
				mesh_cnt_ASSOC_PeerLink_CAP(priv, pstat, INCREASE);
			}
#endif

#ifdef WDS
			if (!(pstat->state & WIFI_WDS))
#endif
			if (pstat->expire_to > 0) 
				cnt_assoc_num(priv, pstat, INCREASE, (char *)__FUNCTION__);

			if ((pstat->expire_to > 0) 
#ifdef WDS
				|| (pstat->state & WIFI_WDS)
#endif
			) {
			check_sta_characteristic(priv, pstat, INCREASE);
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
				construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);

#ifndef USE_WEP_DEFAULT_KEY
			set_keymapping_wep(priv, pstat);
#endif
			if (!SWCRYPTO && pstat->dot11KeyMapping.keyInCam == TRUE) {
#ifdef CONFIG_RTL_HW_WAPI_SUPPORT
				if (pstat->wapiInfo && (pstat->wapiInfo->wapiType != wapiDisable)) {
					wapiStaInfo *wapiInfo = pstat->wapiInfo;
					
					retVal = CamAddOneEntry(priv, 
							pstat->hwaddr, 
							wapiInfo->wapiUCastKeyId,	/* keyid */ 
							DOT11_ENC_WAPI<<2,	/* type */
							0,	/* use default key */
							wapiInfo->wapiUCastKey[wapiInfo->wapiUCastKeyId].dataKey);
					if (retVal) {
						priv->pshare->CamEntryOccupied++;
						
						retVal = CamAddOneEntry(priv, 
								pstat->hwaddr, 
								wapiInfo->wapiUCastKeyId,	/* keyid */
								DOT11_ENC_WAPI<<2,	/* type */
								1,	/* use default key */
								wapiInfo->wapiUCastKey[wapiInfo->wapiUCastKeyId].micKey);
						if (retVal) {
							//pstat->dot11KeyMapping.keyInCam = TRUE;
							priv->pshare->CamEntryOccupied++;
						} else {
							retVal = CamDeleteOneEntry(priv, pstat->hwaddr, wapiInfo->wapiUCastKeyId, 0);
							if (retVal) {
								priv->pshare->CamEntryOccupied--;
								pstat->dot11KeyMapping.keyInCam = FALSE;
							}
						}
					} else {
						pstat->dot11KeyMapping.keyInCam = FALSE;
					}
				} else
#endif // CONFIG_RTL_HW_WAPI_SUPPORT
				if (pstat->dot11KeyMapping.dot11Privacy) {
				memcpy(key_combo,
					pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKey.skey,
					pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen);
				memcpy(&key_combo[pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen],
					pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKey1.skey,
					pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKeyLen);

#ifdef MULTI_MAC_CLONE
				if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable) 
					retVal = CamAddOneEntry(priv, pstat->sa_addr, pstat->dot11KeyMapping.keyid,
						pstat->dot11KeyMapping.dot11Privacy<<2, 0, key_combo);
				else
#endif
					retVal = CamAddOneEntry(priv, pstat->hwaddr, pstat->dot11KeyMapping.keyid,
						pstat->dot11KeyMapping.dot11Privacy<<2, 0, key_combo);
					
					if (retVal)
						priv->pshare->CamEntryOccupied++;
					else
						pstat->dot11KeyMapping.keyInCam = FALSE;
				}
			}
			}
			// to avoid add RAtid fail
#if defined(CONFIG_PCI_HCI)
#ifdef CONFIG_WLAN_HAL
			if (IS_HAL_CHIP(priv)) {
				GET_HAL_INTERFACE(priv)->UpdateHalRAMaskHandler(priv, pstat, 3);
			} else
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
			if(GET_CHIP_VER(priv)== VERSION_8812E) {
				UpdateHalRAMask8812(priv, pstat, 3);
			} else
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv)==VERSION_8188E) {
#ifdef TXREPORT
				add_RATid(priv, pstat);
#endif
			} else
#endif
			{
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
				add_update_RATid(priv, pstat);
#endif
			}
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			update_STA_RATid(priv, pstat);
#endif
			kfree(pBackup->sta[i]);

			if (priv->pshare->is_40m_bw && (pstat->IOTPeer == HT_IOT_PEER_MARVELL))

			{
			    setSTABitMap(&priv->pshare->marvellMapBit, pstat->aid);

#if defined(CONFIG_RTL_8812_SUPPORT)||defined(CONFIG_WLAN_HAL)
				if((GET_CHIP_VER(priv)== VERSION_8812E)||(IS_HAL_CHIP(priv))){
				}
                else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
				{//not HAL
				if (priv->pshare->Reg_RRSR_2 == 0 && priv->pshare->Reg_81b == 0){
					priv->pshare->Reg_RRSR_2 = RTL_R8(RRSR+2);
					priv->pshare->Reg_81b = RTL_R8(0x81b);
					RTL_W8(RRSR+2, priv->pshare->Reg_RRSR_2 | 0x60);
					RTL_W8(0x81b, priv->pshare->Reg_81b | 0x0E); 
				} 
			}
			}
			update_intel_sta_bitmap(priv, pstat, 0);
#if defined(WIFI_11N_2040_COEXIST_EXT)
			update_40m_staMap(priv, pstat, 0);
#endif
		}
	}

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
	update_RAMask_to_FW(priv, 1);
#endif
	memcpy(&priv->pmib->dot11GroupKeysTable, &pBackup->gkey, sizeof(struct Dot11KeyMappingsEntry));
		if (!SWCRYPTO && priv->pmib->dot11GroupKeysTable.keyInCam) {
#ifdef CONFIG_RTL_HW_WAPI_SUPPORT
		if (priv->pmib->wapiInfo.wapiType != wapiDisable)
		{
			const uint8	CAM_CONST_BCAST[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
			retVal = CamAddOneEntry(priv, 
								CAM_CONST_BCAST, 
								priv->wapiMCastKeyId<<1,		/* keyid */ 
								DOT11_ENC_WAPI<<2, 	/* type */
								0,						/* use default key */
								priv->wapiMCastKey[priv->wapiMCastKeyId].dataKey);
			if (retVal) {
				retVal = CamAddOneEntry(priv, 
								CAM_CONST_BCAST, 
								(priv->wapiMCastKeyId<<1)+1,		/* keyid */ 
								DOT11_ENC_WAPI<<2, 	/* type */
								1,						/* use default key */
								priv->wapiMCastKey[priv->wapiMCastKeyId].micKey);
				if (retVal) {
					priv->pshare->CamEntryOccupied++;
//					priv->pmib->dot11GroupKeysTable.keyInCam = TRUE;
				} else {
					retVal = CamDeleteOneEntry(priv, CAM_CONST_BCAST, 1, 0);
					if (retVal)
						priv->pmib->dot11GroupKeysTable.keyInCam = FALSE;
			}
			} else {
				priv->pmib->dot11GroupKeysTable.keyInCam = FALSE;
		}
		} else
#endif // CONFIG_RTL_HW_WAPI_SUPPORT
		{
			memcpy(key_combo,
				priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKey.skey,
				priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen);

			memcpy(&key_combo[priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen],
				priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKey1.skey,
				priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKeyLen);

			retVal = CamAddOneEntry(priv, (unsigned char *)"\xff\xff\xff\xff\xff\xff", priv->pmib->dot11GroupKeysTable.keyid,
				priv->pmib->dot11GroupKeysTable.dot11Privacy<<2, 0, key_combo);
			
			if (retVal)
				priv->pshare->CamEntryOccupied++;
			else
				priv->pmib->dot11GroupKeysTable.keyInCam = FALSE;
		}
	}

#ifdef WDS
	memcpy(&priv->pmib->dot11WdsInfo, &pBackup->wds, sizeof(struct wds_info));
#endif
	kfree(pInfo);
}
#endif // FAST_RECOVERY

#ifdef CONFIG_RTL8190_PRIV_SKB
	#if defined(CONFIG_RTL8196B_GW_8M) || defined(CONFIG_RTL8196C_AP_ROOT) || defined(CONFIG_RTL8196C_CLIENT_ONLY) || defined(CONFIG_RTL_8198_AP_ROOT) || defined(__ECOS)
#ifdef __LINUX_2_6__
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128+128)
#else
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128)
#endif
	#else
#ifdef __LINUX_2_6__
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128+128)
#else
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128)
#endif
	#endif

#define MAGIC_CODE		"8190"

struct priv_skb_buf {
	unsigned char magic[4];
	unsigned int buf_pointer;
#ifdef CONCURRENT_MODE
	struct rtl8192cd_priv *root_priv;
#endif
	struct list_head	list;
	unsigned char buf[SKB_BUF_SIZE];
};


#ifdef DUALBAND_ONLY
	#define REAL_MAX_SKB	(MAX_SKB_NUM/2)
#else
	#define REAL_MAX_SKB	(MAX_SKB_NUM)
#endif

#ifdef CONCURRENT_MODE
static struct priv_skb_buf skb_buf[NUM_WLAN_IFACE][REAL_MAX_SKB];
static struct list_head skbbuf_list[NUM_WLAN_IFACE];
#ifdef CONFIG_WIRELESS_LAN_MODULE
static int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB, REAL_MAX_SKB};
#else
int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB, REAL_MAX_SKB};
#endif

#else
static struct priv_skb_buf skb_buf[REAL_MAX_SKB];
static struct list_head skbbuf_list;
static struct rtl8192cd_priv *root_priv;
#ifdef CONFIG_WIRELESS_LAN_MODULE
static int skb_free_num = REAL_MAX_SKB;
#else
int skb_free_num = REAL_MAX_SKB;
#endif
#endif


extern struct sk_buff *dev_alloc_8190_skb(unsigned char *data, int size);


void init_priv_skb_buf(struct rtl8192cd_priv *priv)
{

        panic_printk("\n\n#######################################################\n");
        panic_printk("SKB_BUF_SIZE=%d MAX_SKB_NUM=%d\n",SKB_BUF_SIZE,MAX_SKB_NUM);
        panic_printk("#######################################################\n\n");


	int i;
#ifdef CONCURRENT_MODE
	int idx = priv->pshare->wlandev_idx;
	memset(skb_buf[idx], '\0', sizeof(struct priv_skb_buf)*REAL_MAX_SKB);

	INIT_LIST_HEAD(&skbbuf_list[idx]);

	for (i=0; i<REAL_MAX_SKB; i++)  {
		memcpy(skb_buf[idx][i].magic, MAGIC_CODE, 4);
		skb_buf[idx][i].root_priv = priv;
		skb_buf[idx][i].buf_pointer = (unsigned int)&skb_buf[idx][i];
		INIT_LIST_HEAD(&skb_buf[idx][i].list);
		list_add_tail(&skb_buf[idx][i].list, &skbbuf_list[idx]);
	}
#else
	memset(skb_buf, '\0', sizeof(struct priv_skb_buf)*REAL_MAX_SKB);

	INIT_LIST_HEAD(&skbbuf_list);

	for (i=0; i<REAL_MAX_SKB; i++)  {
		memcpy(skb_buf[i].magic, MAGIC_CODE, 4);
		skb_buf[i].buf_pointer = (unsigned int)&skb_buf[i];				
		INIT_LIST_HEAD(&skb_buf[i].list);
		list_add_tail(&skb_buf[i].list, &skbbuf_list);
	}
	root_priv = priv;
#endif
}


#ifdef CONCURRENT_MODE
static __inline__ unsigned char *get_priv_skb_buf(struct rtl8192cd_priv *priv)
{
	int i;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	unsigned char *data;

	SAVE_INT_AND_CLI(flags);

	i = priv->pshare->wlandev_idx;
	data = get_buf_from_poll(priv, &skbbuf_list[i], (unsigned int *)&skb_free_num[i]);

	RESTORE_INT(flags);		
	return data;
}

#else

static __inline__ unsigned char *get_priv_skb_buf(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	ret = get_buf_from_poll(root_priv, &skbbuf_list, (unsigned int *)&skb_free_num);

	RESTORE_INT(flags);	
	return ret;
}

#endif

#if defined(DUALBAND_ONLY) && defined(CONFIG_RTL8190_PRIV_SKB)
extern u32 if_priv[];
void merge_pool(struct rtl8192cd_priv *priv)
{
	unsigned char *buf;
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf;	
	int next_idx;
	int idx = priv->pshare->wlandev_idx;

	if (idx == 0)
		next_idx = 1;
	else
		next_idx = 0;

	while (1) {
		if (skb_free_num[idx] >= REAL_MAX_SKB*2)
			break;

		buf = get_priv_skb_buf((struct rtl8192cd_priv *)if_priv[next_idx]);
		if (buf == NULL)
			break;

		priv_buf = (struct priv_skb_buf *)(((unsigned long)buf) - offset);
		priv_buf->root_priv = priv;
		release_buf_to_poll(priv, buf, &skbbuf_list[idx], (unsigned int *)&skb_free_num[idx]);
	}
}

void split_pool(struct rtl8192cd_priv *priv)
{
	unsigned char *buf;	
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf;	
	
	int next_idx;
	int idx = priv->pshare->wlandev_idx;

	if (idx == 0)
		next_idx = 1;
	else
		next_idx = 0;

	while (1) {		
		if (skb_free_num[idx] <= REAL_MAX_SKB)
			break;
		
		buf = get_priv_skb_buf(priv);
		if (buf == NULL)
			break;

		priv_buf = (struct priv_skb_buf *)(((unsigned long)buf) - offset);
		priv_buf->root_priv = (struct rtl8192cd_priv *)if_priv[next_idx];
		release_buf_to_poll((struct rtl8192cd_priv *)if_priv[next_idx], 
			buf, 	&skbbuf_list[next_idx], (unsigned int *)&skb_free_num[next_idx]);
	}
}
#endif

__IRAM_IN_865X
#ifdef CONCURRENT_MODE
static struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size)
{
	struct sk_buff *skb;

	unsigned char *data = get_priv_skb_buf(priv);
	if (data == NULL) {
//		_DEBUG_ERR("wlan: priv skb buffer empty!\n");
		return NULL;
	}

	skb = dev_alloc_8190_skb(data, size);
	if (skb == NULL) {
		free_rtl8190_priv_buf(data);
		return NULL;
	}
	return skb;
}
#else
static struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size)
{
	struct sk_buff *skb;

	unsigned char *data = get_priv_skb_buf(priv);
	if (data == NULL) {
//		_DEBUG_ERR("wlan: priv skb buffer empty!\n");
		return NULL;
	}

	skb = dev_alloc_8190_skb(data, size);
	if (skb == NULL) {
		free_rtl8190_priv_buf(data);
		return NULL;
	}
	return skb;
}
#endif

int is_rtl8190_priv_buf(unsigned char *head)
{
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf = (struct priv_skb_buf *)(((unsigned long)head) - offset);

	if (!memcmp(priv_buf->magic, MAGIC_CODE, 4) &&
			(priv_buf->buf_pointer == (unsigned int)priv_buf))
		return 1;
	else
		return 0;
}

__IRAM_IN_865X
void free_rtl8190_priv_buf(unsigned char *head)
{

#ifdef CONCURRENT_MODE
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf = (struct priv_skb_buf *)(((unsigned long)head) - offset);
	struct rtl8192cd_priv *priv = priv_buf->root_priv;
	int i = priv->pshare->wlandev_idx;
#ifndef SMP_SYNC
	unsigned long x;
#endif	
#ifdef DELAY_REFILL_RX_BUF
	int ret;
#ifdef SMP_SYNC		
	unsigned long flags;
	int locked, cpuid;
#endif
#ifdef CONFIG_WLAN_HAL
    unsigned int                    q_num;
    PHCI_RX_DMA_MANAGER_88XX        prx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q;
#endif // CONFIG_WLAN_HAL
#ifdef SMP_SYNC
	locked = 0;		
	SMP_TRY_LOCK_RECV(flags,locked);
#endif	
	SAVE_INT_AND_CLI(x);
#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
        if (!(priv->drv_state & DRV_STATE_OPEN)){
            /* return 0 means can't refill (because interface be closed or not opened yet) to rx ring but relesae to skb_poll*/            
            ret=0;
        }else{
            q_num   = 0;
            prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
            cur_q   = &(prx_dma->rx_queue[q_num]);
                        
            ret = refill_rx_ring_88XX(priv, NULL, head, q_num, cur_q);
            GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
            cur_q->rxbd_ok_cnt = 0;
        }
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
		ret = refill_rx_ring(priv, NULL, head);
	}
	
	if (ret) {
#ifdef SMP_SYNC		
		if(locked)
			SMP_UNLOCK_RECV(flags);
#endif			
		RESTORE_INT(x);
		return;
	}
	else {
    	release_buf_to_poll(priv, head, &skbbuf_list[i], (unsigned int *)&skb_free_num[i]);
    }
#ifdef SMP_SYNC
    if(locked)
			SMP_UNLOCK_RECV(flags);
#endif			
	RESTORE_INT(x);
#else // ! DELAY_REFILL_RX_BUF
	SAVE_INT_AND_CLI(x);
	release_buf_to_poll(priv, head, &skbbuf_list[i], (unsigned int *)&skb_free_num[i]);
	RESTORE_INT(x);
#endif

#else // ! CONCURRENT_MODE

	unsigned long x;
	struct rtl8192cd_priv *priv = root_priv;
#if 0
	if (!is_rtl8190_priv_buf(head)) {
		printk("wlan: free invalid priv skb buf!\n");
		return;
	}
#endif

#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
    unsigned int                    q_num;
    PHCI_RX_DMA_MANAGER_88XX        prx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q;
#endif
	int ret;

#ifdef SMP_SYNC
	unsigned long flags;
	int locked, cpuid;
  	cpuid = smp_processor_id();
	locked = 0;		
	SMP_TRY_LOCK_RECV(flags,locked,cpuid);
#endif
	SAVE_INT_AND_CLI(x);
#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
	    // Currently, only one queue for rx...    
        if (!(priv->drv_state & DRV_STATE_OPEN)){
            /* return 0 means can't refill (because interface be closed or not opened yet) to rx ring but relesae to skb_poll*/            
            ret=0;
        }else{        
    	    q_num   = 0;
    	    prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
    	    cur_q   = &(prx_dma->rx_queue[q_num]); 	
    	    ret = refill_rx_ring_88XX(priv, NULL, head, q_num, cur_q);
    	    GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
    	    cur_q->rxbd_ok_cnt = 0;
        }
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
		ret = refill_rx_ring(priv, NULL, head);
	}
	
	if (ret) {
#ifdef SMP_SYNC
		if(locked)
			SMP_UNLOCK_RECV(flags);
#endif	
		RESTORE_INT(x);
		return;
	} else {
		release_buf_to_poll(root_priv, head, &skbbuf_list, (unsigned int *)&skb_free_num);
	}
#ifdef SMP_SYNC
		if(locked)
			SMP_UNLOCK_RECV(flags);
#endif	
	RESTORE_INT(x);
#else // ! DELAY_REFILL_RX_BUF

	SAVE_INT_AND_CLI(x);
	release_buf_to_poll(root_priv, head, &skbbuf_list, (unsigned int *)&skb_free_num);
	RESTORE_INT(x);	

#endif

#endif
}
#endif //CONFIG_RTL8190_PRIV_SKB



/*
unsigned int set_fw_reg(struct rtl8192cd_priv *priv, unsigned int cmd, unsigned int val, unsigned int with_val)
{
	static unsigned int delay_count;

	delay_count = 10;

	do {
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);
	delay_count = 10;

	if (with_val == 1)
		RTL_W32(0x2c4, val);

	RTL_W32(0x2c0, cmd);

	do {
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);

	return 0;
}


void set_fw_A2_entry(struct rtl8192cd_priv *priv, unsigned int cmd, unsigned char *addr)
{
	unsigned int delay_count = 10;

	do{
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);
	delay_count = 10;

	RTL_W32(0x2c4, addr[3]<<24 | addr[2]<<16 | addr[1]<<8 | addr[0]);
	RTL_W32(0x2c8, addr[5]<<8 | addr[4]);
	RTL_W32(0x2c0, cmd);

	do{
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);
}
*/

//#if defined(TXREPORT) || defined(SW_ANT_SWITCH) || defined(USE_OUT_SRC)
#if 1
struct stat_info* findNextSTA(struct rtl8192cd_priv *priv, int *idx)
{
    int i;
    for(i= *idx; i<NUM_STAT; i++) {
        if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used == TRUE) {
            *idx = (i+1);
            if (priv->pshare->aidarray[i]->station.sta_in_firmware != 1)
                continue;

            return &(priv->pshare->aidarray[i]->station);
        }
    }
    return NULL;
}
#endif

int is_DFS_channel(int channelVal)
{
    if( channelVal >= 52  && channelVal <= 140 ){    
        return 1;
    }else{
        return 0;
    }
}

int is_passive_channel(struct rtl8192cd_priv *priv , int domain, int chan)
{

#ifdef DFS    
	/*during DFS channel , do passive scan*/
    if( (chan >= 52  && chan <= 140) && !priv->pmib->dot11DFSEntry.disable_DFS){  
        return 1;
    }
#endif    	
    #if 0/*when the mib "w52_passive_scan" enabled , do passive scan in ch W52(ch 36 40 44 48)*/
	else if(((chan >= 36) && (chan <= 48)) 
        && priv->pmib->dot11StationConfigEntry.w52_passive_scan ){
		return 1;
    }
    #endif
    #if 0
	else if ((chan >= 12 && chan <= 14) && (domain == DOMAIN_GLOBAL || domain == DOMAIN_WORLD_WIDE)){
		return 1;
    }
    #endif
	return 0;
}
#if defined(TXREPORT)
void requestTxReport(struct rtl8192cd_priv *priv)
{
	int h2ccmd, counter=20;
	struct stat_info *sta;

	if( priv->pshare->sta_query_idx == -1)
		return;
	
	while(is_h2c_buf_occupy(priv)) {
		delay_ms(2);
		if(--counter==0)
			break;
	}
	if(!counter) 
		return;

	h2ccmd= AP_REQ_RPT;
	
	sta = findNextSTA(priv, &priv->pshare->sta_query_idx);
	if(sta)
		h2ccmd |= (REMAP_AID(sta)<<24);
	else {
		priv->pshare->sta_query_idx = -1;
		return;		
	}
	sta = findNextSTA(priv, &priv->pshare->sta_query_idx);
	if(sta)	{	
		h2ccmd |= (REMAP_AID(sta)<<16);
	} else {
		priv->pshare->sta_query_idx = -1;	
	}

	signin_h2c_cmd(priv, h2ccmd , 0);
	DEBUG_INFO("signin h2c:%x\n", h2ccmd);

}

/*
inital tx rate report from fw
---------------------------------------------------------
0 -> cck 1		  12 -> MCS0	44 -> 1NSS-MCS0 
1 -> cck 2        13 -> MCS1    45 -> 1NSS-MCS1 
2 -> cck 5.5      14 -> MCS2    46 -> 1NSS-MCS2 
3 -> cck 11       15 -> MCS3    47 -> 1NSS-MCS3 
------------      16 -> MCS4    48 -> 1NSS-MCS4 
4 ->  ofdm 6      17 -> MCS5    49 -> 1NSS-MCS5 
5 ->  ofdm 9      18 -> MCS6    50 -> 1NSS-MCS6 
6 ->  ofdm 12     19 -> MCS7    51 -> 1NSS-MCS7 
7 ->  ofdm 18     20 -> MCS8    52 -> 1NSS-MCS8 
8 ->  ofdm 24     21 -> MCS9    53 -> 1NSS-MCS9 
9 ->  ofdm 36     22 -> MCS10   54 -> 2NSS-MCS0 
10 -> ofdm 48     23 -> MCS11   55 -> 2NSS-MCS1 
11 -> ofdm 54     24 -> MCS12   56 -> 2NSS-MCS2 
                  25 -> MCS13   57 -> 2NSS-MCS3 
                  26 -> MCS14   58 -> 2NSS-MCS4 
                  27 -> MCS15   59 -> 2NSS-MCS5 
                                60 -> 2NSS-MCS6 
                                61 -> 2NSS-MCS7 
                                62 -> 2NSS-MCS8 
                                63 -> 2NSS-MCS9 
---------------------------------------------------------                                
*/
#ifdef FOR_DISPLAY_RATE
void get_inital_tx_rate2string(unsigned char txrate ){
	static unsigned char rateStr[16];
    if(txrate>=44 && txrate<=53){
		printk("VHT 1SS-MCS%d\n",txrate-44);
	}
	else if(txrate>=54 && txrate<=63){
		printk("VHT 2SS-MCS%d\n",txrate-54);
	}
	else if(txrate>=12 && txrate<=27){
		printk("MCS%d\n",txrate-12);
	}
	else if(txrate>=0 && txrate<=3){
		if(txrate==0)
			printk("CCK-1\n");
		else if(txrate==1)
			printk("CCK-2\n");		
		else if(txrate==2)
			printk("CCK-5.5\n");
		else if(txrate==3)
			printk("CCK-11\n");
	}
	else if(txrate>=4 && txrate<=11){
		if(txrate==4)
			printk("OFDM-6\n");
		else if(txrate==5)
			printk("OFDM-9\n");
		else if(txrate==6)
			printk("OFDM-12\n");
		else if(txrate==7)
			printk("OFDM-18\n");
		else if(txrate==8)
			printk("OFDM-24\n");
		else if(txrate==9)
			printk("OFDM-36\n");
		else if(txrate==10)
			printk("OFDM-48\n");
		else if(txrate==11)
			printk("OFDM-54\n");		

	}

}

//#define FDEBUG(fmt, args...) panic_printk("[%s %d]"fmt,__FUNCTION__,__LINE__,## args)
#endif

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)

void update_RAMask_to_FW(struct rtl8192cd_priv *priv, int forceUpdate)
{
	int idx = 0;
	struct stat_info *pstat = NULL;

	if( !IS_HAL_CHIP(priv) && GET_CHIP_VER(priv)!= VERSION_8812E)
		return;


	if (!forceUpdate && !( priv->pshare->is_40m_bw 
#ifdef WIFI_11N_2040_COEXIST
			&& !((((GET_MIB(priv))->dot11OperationEntry.opmode) & WIFI_AP_STATE) 
			&& priv->pmib->dot11nConfigEntry.dot11nCoexist 
			&& (priv->bg_ap_timeout || orForce20_Switch20Map(priv)
			))
#endif
	))
		return;

	pstat = findNextSTA(priv, &idx);

	while(pstat) {		
		if(forceUpdate) {
#ifdef CONFIG_RTL_8812_SUPPORT
			if (GET_CHIP_VER(priv) == VERSION_8812E) 
				UpdateHalRAMask8812(priv, pstat, 3);
			 else
#endif				
			{
#ifdef CONFIG_WLAN_HAL
				GET_HAL_INTERFACE(priv)->UpdateHalRAMaskHandler(priv, pstat, 3);
#endif
			}
		} else {
				if(!pstat->tx_bw_fw && pstat->tx_bw) {
#ifdef CONFIG_RTL_8812_SUPPORT
					if (GET_CHIP_VER(priv) == VERSION_8812E)
						UpdateHalRAMask8812(priv, pstat, 3);
					else
#endif
					{
#ifdef CONFIG_WLAN_HAL                                       
						GET_HAL_INTERFACE(priv)->UpdateHalRAMaskHandler(priv, pstat, 3);
#endif
					}
				}
		}
		pstat = findNextSTA(priv, &idx);

	}		

}

// TODO: Filen, check 8192E code below
void txrpt_handler_8812(struct rtl8192cd_priv *priv, struct tx_rpt *report, struct stat_info	*pstat)
{
    static unsigned char initial_rate = 0x7f;
    static unsigned char legacyRA =0 ;
    static unsigned int autoRate1=0;

    /*under auto rate case , pstat->current_tx_rate just for display but it'll be changed, 
        so, take care! if under fixed rate case don't enter below block*/ 		

    if(!pstat)
        return;
    if(pstat->sta_in_firmware == 1)
    {
        if( should_restrict_Nrate(priv, pstat) && is_fixedMCSTxRate(priv, pstat)){
            legacyRA = 1;
        }

        autoRate1= is_auto_rate(priv, pstat);

        if(	!(legacyRA || autoRate1) )
            return;

        //FDEBUG("STA[%02x%02x%02x:%02x%02x%02x]auto rate ,txfail=%d , txok=%d , rate=",
        //	pstat->hwaddr[0],pstat->hwaddr[1],pstat->hwaddr[2],pstat->hwaddr[3],pstat->hwaddr[4],pstat->hwaddr[5],
        //	report->txfail, report->txok );
        //get_inital_tx_rate2string(report->initil_tx_rate&0x3f);


        initial_rate = report->initil_tx_rate ; 			
        if ((initial_rate & 0x7f) == 0x7f)
            return;

        if ((initial_rate&0x3f) < 12) {
            pstat->current_tx_rate = dot11_rate_table[initial_rate&0x3f];

            pstat->ht_current_tx_info &= ~TX_USE_SHORT_GI;				
        } else {
            if((initial_rate&0x3f) >= 44){
                pstat->current_tx_rate = VHT_RATE_ID+((initial_rate&0x3f) -44);
            }else{
                pstat->current_tx_rate = HT_RATE_ID+((initial_rate&0x3f) -12);
            }

            if (initial_rate & BIT(7))
                pstat->ht_current_tx_info |= TX_USE_SHORT_GI;
            else
                pstat->ht_current_tx_info &= ~TX_USE_SHORT_GI;
        }

        priv->pshare->current_tx_rate    = pstat->current_tx_rate;
        priv->pshare->ht_current_tx_info = pstat->ht_current_tx_info;

    } 
}
#endif
#ifdef CONFIG_WLAN_HAL
void APReqTXRptHandler(
    struct rtl8192cd_priv   *priv,
    pu1Byte                  pbuf
)
{
    PAPREQTXRPT pparm = (PAPREQTXRPT)pbuf;
  	struct tx_rpt rpt1;
	unsigned char MacID = 0xff;        
    unsigned char idx = 0;
    int j;
    {
        for (j = 0; j < 2; j++) {

            MacID = pparm->txrpt[j].RPT_MACID;
            if (MacID == 0xff)
                continue;

            rpt1.macid =  MacID & (0x1f);

            if (rpt1.macid) { 
                rpt1.txok = le16_to_cpu(pparm->txrpt[j].RPT_TXOK);
                rpt1.txfail = le16_to_cpu(pparm->txrpt[j].RPT_TXFAIL);                  
                rpt1.initil_tx_rate = pparm->txrpt[j].RPT_InitialRate;
              
                txrpt_handler(priv, &rpt1); // add inital tx rate handle for 8812E
            }
            idx += 6;
        }
    }
}
#endif

void _txrpt_handler(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_rpt *report)
{
#ifdef DONT_COUNT_PROBE_PACKET
	if (pstat->tx_probe_rsp_pkts) {
		if (pstat->tx_probe_rsp_pkts >= report->txok) {
			pstat->tx_probe_rsp_pkts -= report->txok;
			report->txok = 0;
		} else {
			report->txok -= pstat->tx_probe_rsp_pkts;
			pstat->tx_probe_rsp_pkts = 0;
		}
	}
#endif // DONT_COUNT_PROBE_PACKET

	if ((0 == report->txok) && (0 == report->txfail))
		return;

	priv->net_stats.tx_errors += report->txfail;
	pstat->tx_fail += report->txfail;
	pstat->tx_pkts += report->txok+report->txfail;

	DEBUG_INFO("debug[%02X%02X%02X%02X%02X%02X]:id=%d,ok=%d,fail=%d\n", 
		pstat->hwaddr[0],pstat->hwaddr[1],pstat->hwaddr[2],pstat->hwaddr[3],pstat->hwaddr[4],pstat->hwaddr[5],
		report->macid, report->txok, report->txfail);
	
#ifdef DETECT_STA_EXISTANCE
#ifdef CONFIG_WLAN_HAL
	if(IS_HAL_CHIP(priv))
	{
		DetectSTAExistance88XX(priv, report, pstat);
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
		// Check for STA existance; added by Annie, 2010-08-10.Not support now
#if (defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT)|| defined(CONFIG_RTL_8812_SUPPORT))
		if (CHIP_VER_92X_SERIES(priv) || (GET_CHIP_VER(priv)== VERSION_8812E))
			DetectSTAExistance(priv, report, pstat);
#endif
	}
#endif // DETECT_STA_EXISTANCE

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
	if( GET_CHIP_VER(priv)== VERSION_8812E || IS_HAL_CHIP(priv))
		txrpt_handler_8812(priv, report, pstat);
#endif
}

void txrpt_handler(struct rtl8192cd_priv *priv, struct tx_rpt *report)
{
	struct stat_info	*pstat;	
#ifdef MBSSID
	int i;
#endif
	pstat = get_macidinfo(priv, report->macid);
	if(pstat) {
		_txrpt_handler(priv, pstat, report);
		return;
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))	{
		pstat = get_macidinfo(GET_VXD_PRIV(priv), report->macid);
		if(pstat) {
			_txrpt_handler(GET_VXD_PRIV(priv), pstat, report);
			return;
		}
	}
#endif
#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) 		{
		for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
			if (IS_DRV_OPEN(priv->pvap_priv[i])) {
				pstat = get_macidinfo(priv->pvap_priv[i], report->macid);
				if(pstat) {
					_txrpt_handler(priv->pvap_priv[i], pstat, report);
					return;
				}
			}
		}
	}
#endif
}

void C2H_isr(struct rtl8192cd_priv *priv)
{
	struct tx_rpt rpt1;
	int j, tmp32, idx=0x1a2;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	SAVE_INT_AND_CLI(flags);
	tmp32 = RTL_R16(0x1a0);
	if( (tmp32&0xff)==0xc2 ) {
		for(j=0; j<2; j++) {
			rpt1.macid= (0x1f) & RTL_R8(idx+4);
			if(rpt1.macid) {
#ifdef _BIG_ENDIAN_
				rpt1.txok = le16_to_cpu(RTL_R16(idx+2));
				rpt1.txfail = le16_to_cpu(RTL_R16(idx));
#else
				rpt1.txok = be16_to_cpu(RTL_R16(idx+2));
				rpt1.txfail = be16_to_cpu(RTL_R16(idx));
#endif
				txrpt_handler(priv, &rpt1);
			}			
			idx+=6;		
		}
	}
	RTL_W8( 0x1af, 0);
	requestTxReport(priv);
	RESTORE_INT(flags);
}


#endif


static int _is_hex(char c)
{
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'A') && (c <= 'F')) ||
            ((c >= 'a') && (c <= 'f')));
}


int rtl_string_to_hex(char *string, unsigned char *key, int len)
{
	char tmpBuf[4];
	int idx, ii=0;
	for (idx=0; idx<len; idx+=2) {
		tmpBuf[0] = string[idx];
		tmpBuf[1] = string[idx+1];
		tmpBuf[2] = 0;
		if ( !_is_hex(tmpBuf[0]) || !_is_hex(tmpBuf[1]))
			return 0;

		key[ii++] = (unsigned char) _atoi(tmpBuf,16);
	}
	return 1;
}


#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
#ifdef __ECOS
INT32 rtl_isWlanPassthruFrame(UINT8 *data)
#else
INT32 rtl_isPassthruFrame(UINT8 *data)
#endif
{
	int	ret;

#ifdef __ECOS
	ret = FAILED;
#else
	ret = FAIL;
#endif
	if (passThruStatusWlan)
	{
		if (passThruStatusWlan&IP6_PASSTHRU_MASK)
		{
			if ((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_IPV6)) ||
				((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_8021Q))&&(*((UINT16*)(data+(ETH_ALEN<<1)+VLAN_HLEN))==__constant_htons(ETH_P_IPV6))))
			{
				ret = SUCCESS;
			}
		}
		#if defined(CONFIG_RTL_CUSTOM_PASSTHRU_PPPOE)
		if (passThruStatusWlan&PPPOE_PASSTHRU_MASK)
		{
			if (((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_PPP_SES))||(*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_PPP_DISC))) ||
				((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_8021Q))&&((*((UINT16*)(data+(ETH_ALEN<<1)+VLAN_HLEN))==__constant_htons(ETH_P_PPP_SES))||(*((UINT16*)(data+(ETH_ALEN<<1)+VLAN_HLEN))==__constant_htons(ETH_P_PPP_DISC)))))
			{
				ret = SUCCESS;
			}
		}
		#endif
	}

	return ret;
}
#endif

#ifdef HS2_SUPPORT
void calcu_sta_v6ip(struct stat_info *pstat)
{
	struct in6_addr addrp;
	addrp.s6_addr[0] = 0xfe;
	addrp.s6_addr[1] = 0x80;
	addrp.s6_addr[2] = 0x00;
	addrp.s6_addr[3] = 0x00;
	addrp.s6_addr[4] = 0x00;
	addrp.s6_addr[5] = 0x00;
	addrp.s6_addr[6] = 0x00;
	addrp.s6_addr[7] = 0x00;
	addrp.s6_addr[8] = pstat->hwaddr[0] | 0x02;
	addrp.s6_addr[9] = pstat->hwaddr[1];
	addrp.s6_addr[10] = pstat->hwaddr[2];
	addrp.s6_addr[11] = 0xff;
	addrp.s6_addr[12] = 0xfe;
	addrp.s6_addr[13] = pstat->hwaddr[3];
    addrp.s6_addr[14] = pstat->hwaddr[4];
    addrp.s6_addr[15] = pstat->hwaddr[5];

	ipv6_addr_copy(&pstat->sta_v6ip, &addrp);
}

void staip_snooping_byarp(struct sk_buff *pskb, struct stat_info *pstat)
{
	struct arphdr *arp = (struct arphdr *)(pskb->data + ETH_HLEN);
	unsigned char *arp_ptr = (unsigned char *)(arp + 1);
	if((arp->ar_pro == __constant_htons(ETH_P_IP)) && (arp->ar_op == htons(ARPOP_REQUEST))) {
		//find sender ip
		arp_ptr += arp->ar_hln;
		//backup sender ip
		if ((*arp_ptr == 0) && (*(arp_ptr+1) == 0) && (*(arp_ptr+2) == 0) && (*(arp_ptr+3) == 0))
			return;
		else {
			memcpy(pstat->sta_ip, arp_ptr, 4);
			panic_printk("ARP cache ip=%d.%d.%d.%d\n", pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
		}
	}
}

void stav6ip_snooping_bynsolic(struct sk_buff *pskb, struct stat_info *pstat)
{
	struct ipv6hdr *hdr = (struct ipv6hdr *)(pskb->data+ETH_HLEN);
    struct icmp6hdr *icmphdr;
	int pkt_len, type;
	if (hdr->version != 6)
        return;

	if (hdr->hop_limit != 255)
		return;

	pkt_len = ntohs(hdr->payload_len);
	if (pkt_len>0)
    {
        icmphdr = (struct icmp6hdr *)(pskb->data+ETH_HLEN+sizeof(*hdr));
        type = icmphdr->icmp6_type;
		if (type == NDISC_NEIGHBOUR_SOLICITATION)
		{
			if ((hdr->saddr.s6_addr32[0] == 0) && (hdr->saddr.s6_addr32[1] == 0) && (hdr->saddr.s6_addr32[2] == 0) && (hdr->saddr.s6_addr32[3] == 0)) {
				struct in6_addr *target = (struct in6_addr *) (icmphdr + 1);
				if ((target->s6_addr32[0] == 0) && (target->s6_addr32[1] == 0) && (target->s6_addr32[2] == 0) && (target->s6_addr32[3] == 0)) {
					return;
				}
				else {
					printk("rcv multicast ns duplicate addr\n");
					ipv6_addr_copy(&pstat->sta_v6ip, target);
				}
			}
			else {
				printk("rcv multicast ns\n");
				ipv6_addr_copy(&pstat->sta_v6ip, &hdr->saddr);
			}
		}
	}
}

#if 0
void stav6ip_snooping_bynadvert(struct sk_buff *pskb, struct stat_info *pstat)
{
    struct ipv6hdr *hdr = (struct ipv6hdr *)(pskb->data+ETH_HLEN);
    struct icmp6hdr *icmphdr;
    int pkt_len, type;
    if (hdr->version != 6)
        return;

	if (hdr->hop_limit != 255)
		return;

    pkt_len = ntohs(hdr->payload_len);
    if (pkt_len>0)
    {
        icmphdr = (struct icmp6hdr *)(pskb->data+ETH_HLEN+sizeof(*hdr));
        type = icmphdr->icmp6_type;
        if (type == NDISC_NEIGHBOUR_ADVERTISEMENT)
        {
            if ((hdr->saddr.s6_addr32[0] == 0) && (hdr->saddr.s6_addr32[1] == 0) && (hdr->saddr.s6_addr32[2] == 0) && (hdr->saddr.s6_addr32[3] == 0)) {
				return;
			}
			else {
				printk("rcv unsolicited neighbor advert multicast\n");
                ipv6_addr_copy(&pstat->sta_v6ip, &hdr->saddr);
            }
        }
    }
}
#endif

void staip_snooping_bydhcp(struct sk_buff *pskb, struct rtl8192cd_priv *priv) //struct stat_info *pstat)
{
	#define DHCP_MAGIC 0x63825363

struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	        __u8    ihl:4,
	                version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	        __u8    version:4,
	                ihl:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	        __u8    tos;
	        __u16   tot_len;
	        __u16   id;
	        __u16   frag_off;
	        __u8    ttl;
	        __u8    protocol;
#if 0
	        __u16   check;
	        __u32   saddr;
	        __u32   daddr;
#endif
};

struct udphdr {
	        __u16   source;
	        __u16   dest;
	        __u16   len;
	        __u16   check;
};

struct dhcpMessage {
		u_int8_t op;
		u_int8_t htype;
		u_int8_t hlen;
		u_int8_t hops;
		u_int32_t xid;
		u_int16_t secs;
		u_int16_t flags;
		u_int32_t ciaddr;
		u_int32_t yiaddr;
		u_int32_t siaddr;
		u_int32_t giaddr;
		u_int8_t chaddr[16];
		u_int8_t sname[64];
		u_int8_t file[128];
		u_int32_t cookie;
#if 0
		u_int8_t options[308]; /* 312 - cookie */
#endif
};

	struct stat_info *pstat;
	struct iphdr* iph;
	struct udphdr *udph;
	struct dhcpMessage *dhcph;
	struct list_head *phead, *plist;
	
	iph = (struct iphdr *)(pskb->data + ETH_HLEN);
	udph = (struct udphdr *)((unsigned int)iph + (iph->ihl << 2));
	dhcph = (struct dhcpMessage *)((unsigned int)udph + sizeof(struct udphdr));

    phead = &priv->asoc_list;
    plist = phead->next;
    while (phead && (plist != phead))
    {
        pstat = list_entry(plist, struct stat_info, asoc_list);
        plist = plist->next;

        if (!memcmp(pstat->hwaddr, &dhcph->chaddr[0], 6)) {
			if (dhcph->op == 2) //dhcp reply
			{
				if (dhcph->yiaddr == 0) 
					return;
				else {
					memcpy(pstat->sta_ip, &dhcph->yiaddr, 4);
					printk("dhcp give yip=%d.%d.%d.%d\n", pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
				}
			}
			return;
		}
	}
}

extern __MIPS16 __IRAM_IN_865X int __rtl8192cd_start_xmit_out(struct sk_buff *skb, struct stat_info *pstat);

int check_nei_advt(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	struct ipv6hdr *hdr = (struct ipv6hdr *)(skb->data+ETH_HLEN);
	struct icmp6hdr *icmpv6;
	unsigned int pkt_len;
    int type;

	pkt_len = ntohs(hdr->payload_len);
	if (pkt_len>0)
    {
        icmpv6 = (struct icmp6hdr *)(skb->data+ETH_HLEN+sizeof(*hdr));
        type = icmpv6->icmp6_type;
        //printk("pkt len=%d,type=%d\n", pkt_len, type);
        if (type == NDISC_NEIGHBOUR_ADVERTISEMENT)
		{
			printk("drop nei advr\n");
			return 1;	
		}
		else
			return 0;
	}
	return 0;
}

int proxy_icmpv6_ndisc(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	struct sk_buff *newskb = NULL;
	struct in6_addr *addrp;
	struct ipv6hdr *hdr = (struct ipv6hdr *)(skb->data+ETH_HLEN);
	struct ipv6hdr *replyhdr;
	struct icmp6hdr *icmpv6_nsolic;
	struct icmp6hdr *icmpv6_nadvt;	
	struct stat_info *pstat;
    struct list_head *phead, *plist;
	unsigned int pkt_len;
	int type;

	HS2_DEBUG_TRACE(2, "proxy_icmpv6_ndisc\n");
	if (hdr->version != 6)
		return 0;
	
	pkt_len = ntohs(hdr->payload_len);

	if (pkt_len>0)
	{
		icmpv6_nsolic = (struct icmp6hdr *)(skb->data+ETH_HLEN+sizeof(*hdr));
		type = icmpv6_nsolic->icmp6_type;
		//printk("pkt len=%d,type=%d\n", pkt_len, type);
		if (type == NDISC_NEIGHBOUR_SOLICITATION)
        {
        	HS2_DEBUG_TRACE(2, "NDISC_NEIGHBOUR_SOLICITATION\n");
			if (!memcmp(skb->data+ETH_ALEN, priv->pmib->dot11StationConfigEntry.dot11Bssid, ETH_ALEN))
	        {
		        HS2_DEBUG_TRACE(2, "v6:arp req src mac=BSSID\n");
			    return 0;
	        }
			if (ipv6_addr_loopback(&hdr->daddr))
			{
				HS2_DEBUG_TRACE(2, "v6:loopback\n");
		        return 0;
			}

			//search target ip mapping pstat mac
			phead = &priv->asoc_list;
	        plist = phead->next;
		    while (phead && (plist != phead))
			{
	            pstat = list_entry(plist, struct stat_info, asoc_list);
		        plist = plist->next;

				addrp = (struct in6_addr *)(icmpv6_nsolic+1);
		        if (ipv6_addr_equal(&pstat->sta_v6ip, addrp))
			    {
#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
				    extern struct sk_buff *priv_skb_copy(struct sk_buff *skb);
	                newskb = priv_skb_copy(skb);
#else
		            newskb = skb_copy(skb, GFP_ATOMIC);
#endif
					//printk("compare ok!!\n");
					if (newskb == NULL)
	                {
		                priv->ext_stats.tx_drops++;
			            HS2_DEBUG_ERR("alloc icmpv6 neighbor advertisement skb null!!\n");
				        rtl_kfree_skb(priv, skb, _SKB_TX_);
					    return 1;
					}
					else
					{
						unsigned char *opt;
						int len;
						//da
						memcpy(newskb->data, newskb->data+ETH_ALEN, 6);
						//sa
						memcpy(newskb->data+ETH_ALEN, pstat->hwaddr, 6);
						
						replyhdr = (struct ipv6hdr *)(newskb->data+ETH_HLEN);
						ipv6_addr_copy(&replyhdr->saddr, &pstat->sta_v6ip);
						ipv6_addr_copy(&replyhdr->daddr, &hdr->saddr);

						icmpv6_nadvt = (struct icmp6hdr *)(newskb->data+ETH_HLEN+sizeof(*hdr));
				        icmpv6_nadvt->icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT;
						icmpv6_nadvt->icmp6_solicited = 1;
						icmpv6_nadvt->icmp6_override = 0;
						
						//target ip
						opt = (unsigned char *)(newskb->data+ETH_HLEN+sizeof(*replyhdr)+sizeof(struct icmp6hdr));
						ipv6_addr_copy((struct in6_addr *)opt, &pstat->sta_v6ip);
						opt += sizeof(struct in6_addr);
						//option
						opt[0] = 2; // Type: Target link-layer addr
						opt[1] = 1; // Length: 1 (8 bytes)
						memcpy(opt+2, pstat->hwaddr, ETH_ALEN);
						icmpv6_nadvt->icmp6_cksum = 0;
						len = sizeof(struct icmp6hdr)+sizeof(struct in6_addr)+8;
						
						icmpv6_nadvt->icmp6_cksum = csum_ipv6_magic(&replyhdr->saddr, &replyhdr->daddr, len, 
							IPPROTO_ICMPV6, csum_partial(icmpv6_nadvt, len, 0));
						
						rtl_kfree_skb(priv, skb, _SKB_TX_);
						
						if (ipv6_addr_equal(&replyhdr->saddr, &replyhdr->daddr))
                        {
                            printk("v6:tip=sip!!\n");
                            dev_kfree_skb_any(newskb);
                            return 1;
                        }

						if ((pstat = get_stainfo(priv, newskb->data)) != NULL)
	                    {
	                        int i;
							HS2_DEBUG_TRACE(2, "v6:da in wlan\n");
							newskb->cb[2] = (char)0xff;         // not do aggregation
							memcpy(newskb->cb+10,newskb->data,6);
							HS2_DEBUG_INFO("data=");
							for(i=0;i<ETH_HLEN+sizeof(*replyhdr)+sizeof(struct icmp6hdr)+sizeof(struct in6_addr)+8;i++) {
								HS2_DEBUG_INFO("%02x ",newskb->data[i]);
							}
							HS2_DEBUG_INFO("\n");
							//dev_kfree_skb_any(newskb);
	                        __rtl8192cd_start_xmit(newskb, priv->dev, 1);
						}
						else
						{				
							HS2_DEBUG_TRACE(2, "v6:da in lan\n");
	                        if (newskb->dev)
#ifdef __LINUX_2_6__
		                        newskb->protocol = eth_type_trans(newskb, newskb->dev);
			                else
#endif
				                newskb->protocol = eth_type_trans(newskb, priv->dev);

#if defined(__LINUX_2_6__) && defined(RX_TASKLET) && !defined(CONFIG_RTL8672) && !defined(__LINUX_3_10__)
					        netif_receive_skb(newskb);
#else
						    netif_rx(newskb);
#endif		
						}
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

int proxy_arp_handle(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	struct sk_buff *newskb = NULL;
	struct arphdr *arp = (struct arphdr *)(skb->data + ETH_HLEN);
	unsigned char *arp_ptr = (unsigned char *)(arp + 1), *psender, *ptarget, *psender_bak;
	struct stat_info *pstat;
	struct list_head *phead, *plist;
	int k;
	unsigned char *tmp = (unsigned char *)(skb->data);
	
	/*if((arp->ar_pro == __constant_htons(ETH_P_IP)) && (arp->ar_op == htons(ARPOP_REQUEST)||arp->ar_op == htons(ARPOP_REPLY)))
	{
		arp_ptr += arp->ar_hln;
		psender_bak = arp_ptr;
		//target ip
		arp_ptr += (arp->ar_hln + arp->ar_pln);
		ptarget = arp_ptr;
		if (!memcmp(psender_bak,ptarget,4))
		{
			printk("gratuitous ARP Request or Reply\n");
			return 0;
		}
	}*/
	HS2_DEBUG_TRACE(2, "Proxy ARP handle\n");
	if((arp->ar_pro == __constant_htons(ETH_P_IP)) && (arp->ar_op == htons(ARPOP_REQUEST)))
	{
		//{
		//	int j;
		//	printk("orin==>");
		//	for(j=0;j<skb->len;j++)
		//		printk("0x%02x:",*(skb->data+j));
		//	printk("\n");
		//}
		//sender ip
		arp_ptr += arp->ar_hln;
		psender_bak = arp_ptr;
		//target ip
		arp_ptr += (arp->ar_hln + arp->ar_pln);
		ptarget = arp_ptr;
	
		if (!memcmp(skb->data+ETH_ALEN, priv->pmib->dot11StationConfigEntry.dot11Bssid, ETH_ALEN))
		{
			HS2_DEBUG_TRACE(1, "arp req src mac=BSSID\n");
			return 0;
		}

		if (ipv4_is_loopback(ptarget) || ipv4_is_multicast(ptarget))
		{
			HS2_DEBUG_TRACE(1, "loopback or muticast!!\n");
	        return 0;
		}

		//search target ip mapping pstat mac
		phead = &priv->asoc_list;
		plist = phead->next;
		while (phead && (plist != phead)) 
		{
			pstat = list_entry(plist, struct stat_info, asoc_list);
			plist = plist->next;
			HS2_DEBUG_INFO("Proxy ARP: Find Destination in Assocation List, sta_ip=%d.%d.%d.%d\n",pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
			if (!memcmp(pstat->sta_ip, ptarget, 4))
			{
				panic_printk("Proxy ARP: Find Destination in Assocation List\n");
#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
				extern struct sk_buff *priv_skb_copy(struct sk_buff *skb);
				newskb = priv_skb_copy(skb);
#else
				newskb = skb_copy(skb, GFP_ATOMIC);
#endif
				if (newskb == NULL) 
				{
					priv->ext_stats.tx_drops++;
					HS2_DEBUG_ERR("alloc arp rsp skb null!!\n");
					rtl_kfree_skb(priv, skb, _SKB_TX_);
					return 1;
				}	
				else
				{
					// ======================
					// build new arp response
					// ======================
					//da
					memcpy(newskb->data, newskb->data+ETH_ALEN, 6);
					//memcpy(newskb->data, priv->pmib->dot11StationConfigEntry.dot11Bssid, 6);
					//sa
					memcpy(newskb->data+ETH_ALEN, pstat->hwaddr, 6);
					//memcpy(newskb->data+ETH_ALEN, priv->pmib->dot11StationConfigEntry.dot11Bssid, 6);
					//arp response			
					arp = (struct arphdr *)(newskb->data + ETH_HLEN);
					arp_ptr = (unsigned char *)(arp + 1);
					arp->ar_op = htons(ARPOP_REPLY);
					//sender mac and ip
					memcpy(arp_ptr, pstat->hwaddr, 6);
					arp_ptr += arp->ar_hln;
					psender = (unsigned char *)arp_ptr;
					memcpy(psender, ptarget, 4);
					//printk("sender mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",pstat->hwaddr[0],pstat->hwaddr[1],pstat->hwaddr[2],pstat->hwaddr[3],pstat->hwaddr[4],pstat->hwaddr[5],ptarget[0],ptarget[1],ptarget[2],ptarget[3]);
					//target mac and ip
					arp_ptr += arp->ar_pln;
					memcpy(arp_ptr, newskb->data, 6);
					//memcpy(newskb->data, priv->pmib->dot11StationConfigEntry.dot11Bssid, 6);

					arp_ptr += arp->ar_hln;
					ptarget = arp_ptr;
					memcpy((unsigned char *)ptarget, (unsigned char *)psender_bak, 4);
					//printk("target mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",newskb->data[0],newskb->data[1],newskb->data[2],newskb->data[3],newskb->data[4],newskb->data[5],ptarget[0],ptarget[1],ptarget[2],ptarget[3]);
							
					rtl_kfree_skb(priv, skb, _SKB_TX_);
					
					if (!memcmp(ptarget, psender, 4))
					{
						HS2_DEBUG_TRACE(2, "target ip = sender ip!!\n");
						dev_kfree_skb_any(newskb);
						return 1;
					}

					if ((pstat = get_stainfo(priv, newskb->data)) != NULL)
					{
						//struct sk_buff_head *pqueue;
					    //struct timer_list *ptimer;
					    //void (*timer_hook)(unsigned long task_priv);

						//if (newskb->dev)
//#ifdef __LINUX_2_6__
//							newskb->protocol = eth_type_trans(newskb, newskb->dev);
//						else
//#endif
//							newskb->protocol = eth_type_trans(newskb, priv->dev);

//						printk("enq\n");
//						pqueue = &pstat->swq.be_queue;
//			            ptimer = &pstat->swq.beq_timer;
//						timer_hook = rtl8192cd_beq_timer;

//					    skb_queue_tail(pqueue, newskb);
//					    ptimer->data = (unsigned long)pstat;
//						ptimer->function = timer_hook; //rtl8190_tmp_timer;
//			            mod_timer(ptimer, jiffies + 1);

//						 SAVE_INT_AND_CLI(x);
						//pstat = get_stainfo(priv, newskb->data);
						HS2_DEBUG_TRACE(1, "da in wlan\n");
				        //__rtl8192cd_start_xmit_out(newskb, pstat);
						newskb->cb[2] = (char)0xff;         // not do aggregation
						memcpy(newskb->cb+10,newskb->data,6);
                        __rtl8192cd_start_xmit(newskb, priv->dev, 1);
//				        RESTORE_INT(x);
	
					}
					else
					{		
						HS2_DEBUG_TRACE(1, "da in lan\n");
						if (newskb->dev)
#ifdef __LINUX_2_6__
							newskb->protocol = eth_type_trans(newskb, newskb->dev);
	                    else
#endif
		                    newskb->protocol = eth_type_trans(newskb, priv->dev);
			
#if defined(__LINUX_2_6__) && defined(RX_TASKLET) && !defined(CONFIG_RTL8672)
						netif_receive_skb(newskb);
#else
						netif_rx(newskb);
#endif
					}
					return 1;
				}			
			}
		}
	}
	
	//drop packet
	return 0;		
}

void rtl8192cd_cu_cntdwn_timer(unsigned long task_priv)
{
    struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	int val;

    if (timer_pending(&priv->cu_cntdwn_timer))
	{
        del_timer_sync(&priv->cu_cntdwn_timer);
    }
	
	if ((val = read_bbp_ch_load(priv)) == -1)
    {
		//printk("bbp not ready!!!\n");
		mod_timer(&priv->cu_cntdwn_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(10));	
	}
	else
	{
        priv->chbusytime += val;
		priv->cu_cntdwn--;
		if (priv->cu_cntdwn == 0)
	    {
			priv->channel_utilization = (priv->chbusytime*255)/(priv->pmib->hs2Entry.channel_utili_beaconIntval * priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * 1024);
			//printk("ch=%d\n", priv->channel_utilization);
			priv->chbusytime = 0;
			priv->cu_cntdwn = priv->cu_initialcnt;
		}
		start_bbp_ch_load(priv);
		mod_timer(&priv->cu_cntdwn_timer, jiffies + CU_TO);
    }
}
#endif

#ifdef USE_TXQUEUE
int init_txq_pool(struct list_head *head, unsigned char **ppool)
{
	unsigned char *ptr;
	unsigned int i;
	struct txq_node *pnode;

	INIT_LIST_HEAD(head);
	
	ptr = kmalloc(TXQUEUE_SIZE * sizeof(struct txq_node), GFP_ATOMIC);
	if (!ptr) {
		printk("ERRORL: %s failed\n", __FUNCTION__);
		*ppool = NULL;
		return -1;
	}

	pnode = (struct txq_node *)ptr;
	for (i=0; i<TXQUEUE_SIZE; i++)
	{
		pnode[i].skb = NULL;
		pnode[i].dev = NULL;
		list_add_tail(&(pnode[i].list), head);
	}

	*ppool = ptr;
	return 0;
}

void free_txq_pool(struct list_head *head, unsigned char *ppool)
{
	if (ppool)
		kfree(ppool);
	INIT_LIST_HEAD(head);
}

void append_skb_to_txq_head(struct txq_list_head *head, struct rtl8192cd_priv *priv, struct sk_buff *skb, struct net_device *dev, struct list_head *pool)
{
	struct txq_node *pnode = NULL;

	if (list_empty(pool))
	{
		DEBUG_ERR("%s: No unused node in pool, this should not happend, fix me.\n", __FUNCTION__);
		rtl_kfree_skb(priv, skb, _SKB_TX_);
		DEBUG_ERR("TX DROP: exceed the tx queue!\n");
		priv->ext_stats.tx_drops++;		
		return;
	}

	pnode = (struct txq_node *)pool->next;
	list_del(pool->next);
	pnode->skb = skb;
	pnode->dev = dev;
	
	add_txq_head(head, pnode);
}

void append_skb_to_txq_tail(struct txq_list_head *head, struct rtl8192cd_priv *priv, struct sk_buff *skb, struct net_device *dev, struct list_head *pool)
{
	struct txq_node *pnode = NULL;

	if (list_empty(pool))
	{
		DEBUG_ERR("%s: No unused node in pool, this should not happend, fix me.\n", __FUNCTION__);
		rtl_kfree_skb(priv, skb, _SKB_TX_);
		DEBUG_ERR("TX DROP: exceed the tx queue!\n");
		priv->ext_stats.tx_drops++;		
		return;
	}

	pnode = (struct txq_node *)pool->next;
	list_del(pool->next);
	pnode->skb = skb;
	pnode->dev = dev;
	
	add_txq_tail(head, pnode);
}

void remove_skb_from_txq(struct txq_list_head *head, struct sk_buff **pskb, struct net_device **pdev, struct list_head *pool)
{
	struct txq_node *pnode = NULL;

	if (is_txq_empty(head))
	{
		*pskb = NULL;
		*pdev = NULL;
		return;
	}

	pnode = deq_txq(head);
	*pskb = pnode->skb;
	*pdev = pnode->dev;
	pnode->skb = NULL;
	pnode->dev = NULL;

	list_add_tail(&pnode->list, pool);
}

#endif


#ifdef TLN_STATS
void stats_conn_rson_counts(struct rtl8192cd_priv *priv, unsigned int reason)
{
	switch (reason) {
	case _RSON_UNSPECIFIED_:
		priv->ext_wifi_stats.rson_UNSPECIFIED_1++;
		break;
	case _RSON_AUTH_NO_LONGER_VALID_:
		priv->ext_wifi_stats.rson_AUTH_INVALID_2++;
		break;
	case _RSON_DEAUTH_STA_LEAVING_:
		priv->ext_wifi_stats.rson_DEAUTH_STA_LEAVING_3++;
		break;
	case _RSON_INACTIVITY_:
		priv->ext_wifi_stats.rson_INACTIVITY_4++;
		break;
	case _RSON_UNABLE_HANDLE_:
		priv->ext_wifi_stats.rson_RESOURCE_INSUFFICIENT_5++;
		break;
	case _RSON_CLS2_:
		priv->ext_wifi_stats.rson_UNAUTH_CLS2FRAME_6++;
		break;
	case _RSON_CLS3_:
		priv->ext_wifi_stats.rson_UNAUTH_CLS3FRAME_7++;
		break;
	case _RSON_DISAOC_STA_LEAVING_:
		priv->ext_wifi_stats.rson_DISASSOC_STA_LEAVING_8++;
		break;
	case _RSON_ASOC_NOT_AUTH_:
		priv->ext_wifi_stats.rson_ASSOC_BEFORE_AUTH_9++;
		break;
	case _RSON_INVALID_IE_:
		priv->ext_wifi_stats.rson_INVALID_IE_13++;
		break;
	case _RSON_MIC_FAILURE_:
		priv->ext_wifi_stats.rson_MIC_FAILURE_14++;
		break;
	case _RSON_4WAY_HNDSHK_TIMEOUT_:
		priv->ext_wifi_stats.rson_4WAY_TIMEOUT_15++;
		break;
	case _RSON_GROUP_KEY_UPDATE_TIMEOUT_:
		priv->ext_wifi_stats.rson_GROUP_KEY_TIMEOUT_16++;
		break;
	case _RSON_DIFF_IE_:
		priv->ext_wifi_stats.rson_DIFF_IE_17++;
		break;
	case _RSON_MLTCST_CIPHER_NOT_VALID_:
		priv->ext_wifi_stats.rson_MCAST_CIPHER_INVALID_18++;
		break;
	case _RSON_UNICST_CIPHER_NOT_VALID_:
		priv->ext_wifi_stats.rson_UCAST_CIPHER_INVALID_19++;
		break;
	case _RSON_AKMP_NOT_VALID_:
		priv->ext_wifi_stats.rson_AKMP_INVALID_20++;
		break;
	case _RSON_UNSUPPORT_RSNE_VER_:
		priv->ext_wifi_stats.rson_UNSUPPORT_RSNIE_VER_21++;
		break;
	case _RSON_INVALID_RSNE_CAP_:
		priv->ext_wifi_stats.rson_RSNIE_CAP_INVALID_22++;
		break;
	case _RSON_IEEE_802DOT1X_AUTH_FAIL_:
		priv->ext_wifi_stats.rson_802_1X_AUTH_FAIL_23++;
		break;
	default:
		priv->ext_wifi_stats.rson_OUT_OF_SCOPE++;
		/*panic_printk("incorrect reason(%d) for statistics\n", reason);*/
		break;
	}

	priv->wifi_stats.rejected_sta++;
}


void stats_conn_status_counts(struct rtl8192cd_priv *priv, unsigned int status)
{
	switch (status) {
	case _STATS_SUCCESSFUL_:
		priv->wifi_stats.connected_sta++;
		break;
	case _STATS_FAILURE_:
		priv->ext_wifi_stats.status_FAILURE_1++;
		break;
	case _STATS_CAP_FAIL_:
		priv->ext_wifi_stats.status_CAP_FAIL_10++;
		break;
	case _STATS_NO_ASOC_:
		priv->ext_wifi_stats.status_NO_ASSOC_11++;
		break;
	case _STATS_OTHER_:
		priv->ext_wifi_stats.status_OTHER_12++;
		break;
	case _STATS_NO_SUPP_ALG_:
		priv->ext_wifi_stats.status_NOT_SUPPORT_ALG_13++;
		break;
	case _STATS_OUT_OF_AUTH_SEQ_:
		priv->ext_wifi_stats.status_OUT_OF_AUTH_SEQ_14++;
		break;
	case _STATS_CHALLENGE_FAIL_:
		priv->ext_wifi_stats.status_CHALLENGE_FAIL_15++;
		break;
	case _STATS_AUTH_TIMEOUT_:
		priv->ext_wifi_stats.status_AUTH_TIMEOUT_16++;
		break;
	case _STATS_UNABLE_HANDLE_STA_:
		priv->ext_wifi_stats.status_RESOURCE_INSUFFICIENT_17++;
		break;
	case _STATS_RATE_FAIL_:
		priv->ext_wifi_stats.status_RATE_FAIL_18++;
		break;
	default:
		priv->ext_wifi_stats.status_OUT_OF_SCOPE++;
		/*panic_printk("incorrect status(%d) for statistics\n", status);*/
		break;
	}

	if (status != _STATS_SUCCESSFUL_)
		priv->wifi_stats.rejected_sta++;
}
#endif


#ifdef SW_TX_QUEUE
void adjust_swq_setting(struct rtl8192cd_priv *priv, struct stat_info *pstat, int i, int mode)
{
	int thd;
	
	if(pstat->swq.q_used[i]) {
		if (mode == CHECK_DEC_AGGN) {
			if (pstat->swq.q_aggnum[i] <= 2)
				thd = priv->pshare->rf_ft_var.timeout_thd;
			else if (pstat->swq.q_aggnum[i] <= 4)
				thd = priv->pshare->rf_ft_var.timeout_thd2;
			else
				thd = priv->pshare->rf_ft_var.timeout_thd3;
		
			if ((pstat->swq.q_TOCount[i] >= thd)&& ((pstat->swq.q_TOCount[i] % thd) == 0)) {
    			pstat->swq.q_aggnum[i] = pstat->swq.q_aggnum[i]-1; 
    			if (pstat->swq.q_aggnum[i] <= 0)
    				pstat->swq.q_aggnum[i] = 1;
    			if (++pstat->swq.q_aggnumIncSlow[i] >= MAX_BACKOFF_CNT)
    				pstat->swq.q_aggnumIncSlow[i] = MAX_BACKOFF_CNT;
				DEBUG_INFO("dec,aid:%d,cnt:%d\n", pstat->aid, pstat->swq.q_TOCount[i]);
    		}
    	}
    	else {
    		if (pstat->swq.q_aggnum[i] <= 2)
				thd = priv->pshare->rf_ft_var.timeout_thd-10;
			else if (pstat->swq.q_aggnum[i] <= 4)
				thd = priv->pshare->rf_ft_var.timeout_thd2-30;
			else
				thd = priv->pshare->rf_ft_var.timeout_thd3-50;

			if(pstat->swq.q_TOCount[i]< thd) {
				pstat->swq.q_aggnum[i] = pstat->swq.q_aggnum[i]+1; 
				if (pstat->swq.q_aggnum[i] > priv->pshare->rf_ft_var.swq_aggnum)
					pstat->swq.q_aggnum[i] = priv->pshare->rf_ft_var.swq_aggnum;
				DEBUG_INFO("inc,aid:%d,cnt:%d,%d\n", pstat->aid, pstat->swq.q_TOCount[i], pstat->swq.q_aggnum[BE_QUEUE]);
			}
		}
	}
}
#endif

#if defined(CONFIG_RTL_ULINKER)
int get_wlan_opmode(struct net_device *dev)
{
	int opmode = -1;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;

	if (netif_running(dev)) {
		if ((priv->pmib->dot11OperationEntry.opmode) & WIFI_AP_STATE)
			opmode = 0;
		else
			opmode = 1;
	}

	return opmode;
}
#endif



#if defined(RF_MIMO_SWITCH) || defined(RF_MIMO_PS)

void Do_BB_Reset(struct rtl8192cd_priv *priv)
{
	unsigned char tmp_reg2 = 0;
	tmp_reg2 = RTL_R8(0x2);
			
	tmp_reg2 &= (~BIT(0));
	RTL_W8(0x2, tmp_reg2);
	tmp_reg2 |= BIT(0);
	RTL_W8(0x2, tmp_reg2);

}

void RF_MIMO_check_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	int i=0, assoc_num = priv->assoc_num;
	if(get_rf_mimo_mode(priv) != MIMO_2T2R)
		return;

    if(priv->auto_channel || timer_pending(&priv->ss_timer)) {
#ifdef CONFIG_PCI_HCI
		goto end;
#else
		return;
#endif
    }
    
#ifdef MP_TEST
	if (((OPMODE & WIFI_MP_STATE) || priv->pshare->rf_ft_var.mp_specific))
		return;
#endif	
		if(0 
#ifdef WDS
		|| (priv->pmib->dot11WdsInfo.wdsEnabled)
#endif
#ifdef UNIVERSAL_REPEATER
		|| (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
#endif
		) {
			if(MIMO_mode_switch(priv, MIMO_2T2R)) {
#if defined(WIFI_11N_2040_COEXIST_EXT)
				if((priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20) && (priv->pmib->dot11nConfigEntry.dot11nUse40M != HT_CHANNEL_WIDTH_20)) {
					priv->pshare->CurrentChannelBW = priv->pmib->dot11nConfigEntry.dot11nUse40M;
					SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
					SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
					update_RAMask_to_FW(priv, 1);		
#endif	
				}
#endif			
			}
			return;
		}

	if(priv->pshare->rf_ft_var.rf_mode ==0) {
#ifdef MBSSID
		if (priv->pmib->miscEntry.vap_enable){
			for (i=0; i<RTL8192CD_NUM_VWLAN; ++i)
				assoc_num += priv->pvap_priv[i]-> assoc_num;
		}
#endif
		if(assoc_num) {			
			if(MIMO_mode_switch(priv, MIMO_2T2R)) {
#if defined(WIFI_11N_2040_COEXIST_EXT)
				if((priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20) && (priv->pmib->dot11nConfigEntry.dot11nUse40M != HT_CHANNEL_WIDTH_20)) {
					priv->pshare->CurrentChannelBW = priv->pmib->dot11nConfigEntry.dot11nUse40M;
					SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
					SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
					update_RAMask_to_FW(priv, 1);
#endif						
				}
#endif		
				// reset bb for 8812 to avoid rx hang
				if (GET_CHIP_VER(priv) == VERSION_8812E)
				Do_BB_Reset(priv);

			}
		}
		else {
#if defined(WIFI_11N_2040_COEXIST_EXT)
			if((priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20_40)|| (priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_80)) {
				priv->pshare->CurrentChannelBW = HT_CHANNEL_WIDTH_20;
				SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
				SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
			}
#endif					
			if(MIMO_mode_switch(priv, MIMO_1T1R)) {	
				
				// reset bb for 8812 to avoid rx hang
				if (GET_CHIP_VER(priv) == VERSION_8812E)
				Do_BB_Reset(priv);
				
			}	
		}
	} else if (priv->pshare->rf_ft_var.rf_mode == 2) {
		if(MIMO_mode_switch(priv, MIMO_2T2R)) {
#if defined(WIFI_11N_2040_COEXIST_EXT)
			if((priv->pshare->CurrentChannelBW == HT_CHANNEL_WIDTH_20) && (priv->pmib->dot11nConfigEntry.dot11nUse40M != HT_CHANNEL_WIDTH_20)) {
				priv->pshare->CurrentChannelBW = priv->pmib->dot11nConfigEntry.dot11nUse40M;
				SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
				SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
				update_RAMask_to_FW(priv, 1);
#endif			
			}
#endif			
		}
	} else 	if (priv->pshare->rf_ft_var.rf_mode == 1) {
		MIMO_mode_switch(priv, MIMO_1T1R);
	}

#ifdef CONFIG_PCI_HCI
end:
	mod_timer(&priv->ps_timer, jiffies + IDLE_T0);
#endif
}

int assign_MIMO_TR_Mode(struct rtl8192cd_priv *priv, unsigned char *data)
{
#define dprintf printk
	int mode = _atoi(data, 16);
	if (strlen(data) == 0) {
		printk("tr mode.\n");
		printk("mimo 1: swith to 1T\n");
		printk("mimo 2: switch to 2T\n");
		printk("mimo 0: auto\n");
		return 0;
	}
	if (mode == 0x01)	{
		MIMO_mode_switch(priv, MIMO_1T1R);
		priv->pshare->rf_ft_var.rf_mode = 1;
	} else if (mode == 0x02)	 {		
		MIMO_mode_switch(priv, MIMO_2T2R);
		priv->pshare->rf_ft_var.rf_mode = 2;
	} else {
		priv->pshare->rf_ft_var.rf_mode = 0;
	}
	return 0;
}
#endif

#ifdef MULTI_MAC_CLONE
int pack_to_profile(struct rtl8192cd_priv *priv)
{
	struct ap_profile profile;
	int i;

	priv->pmib->ap_profile.profile_num = 0;
	memset(priv->pmib->ap_profile.profile, '\0', sizeof(profile)*PROFILE_NUM);

	if (netif_running(priv->dev))
	{
		memset(&profile, 0, sizeof(struct ap_profile));
		strcpy(profile.ssid, priv->pmib->dot11StationConfigEntry.dot11DesiredSSID);
		profile.auth_type = priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm;
		if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_NO_PRIVACY_)
			profile.encryption = 0;
		else if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_40_PRIVACY_) {
			profile.encryption = 1;
		}
		else if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_104_PRIVACY_) {
			profile.encryption = 2;
		}
		else if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher!=0) {
			profile.encryption = 4;
		}
		else if (priv->pmib->dot1180211AuthEntry.dot11WPACipher!=0) {
			profile.encryption = 3;
		}

		memset(&priv->pmib->ap_profile.profile[priv->pmib->ap_profile.profile_num], '\0', sizeof(profile));	
		memcpy(&priv->pmib->ap_profile.profile[priv->pmib->ap_profile.profile_num], &profile, sizeof(profile));	
		priv->pmib->ap_profile.profile_num++;
		priv->pmib->ap_profile.enable_profile = 1;
	}

	return 0;
}
#endif	// SUPPORT_MULTI_PROFILE

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
struct list_head* asoc_list_get_next(struct rtl8192cd_priv *priv, struct list_head *plist)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	struct list_head *phead, *plist_next;
	struct stat_info *pstat;
	
	phead = &priv->asoc_list;
	
	SMP_LOCK_ASOC_LIST(flags);
	
	plist_next = plist->next;
	if (plist_next != phead) {
		pstat = list_entry(plist_next, struct stat_info, asoc_list);
		pstat->asoc_list_refcnt++;
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	
	if (plist != phead) {
		pstat =  list_entry(plist, struct stat_info, asoc_list);
		asoc_list_unref(priv, pstat);
	}
	
	return plist_next;
}

void asoc_list_unref(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int unlink = 0;
	
	SMP_LOCK_ASOC_LIST(flags);
	
	BUG_ON(0 == pstat->asoc_list_refcnt);
	
	pstat->asoc_list_refcnt--;
	if (0 == pstat->asoc_list_refcnt) {
		list_del_init(&pstat->asoc_list);
		unlink = 1;
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	
	if (unlink) {
#ifdef __ECOS
		cyg_flag_setbits(&pstat->asoc_unref_done, 0x1);
#else
		complete(&pstat->asoc_unref_done);
#endif
	}
}

// The function returns whether it had linked to asoc_list before removing.
int asoc_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0, wait = 0;
	
#ifdef __KERNEL__
	might_sleep();
#endif

	SMP_LOCK_ASOC_LIST(flags);
	
	if (0 != pstat->asoc_list_refcnt) {
		pstat->asoc_list_refcnt--;
		if (0 ==pstat->asoc_list_refcnt) {
//			if (!list_empty(&pstat->asoc_list)) {
				list_del_init(&pstat->asoc_list);
				ret = 1;
//			}
		} else {
			wait = 1;
		}
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	
	if (wait) {
#ifdef __ECOS
		cyg_flag_wait(&pstat->asoc_unref_done, 0x01, CYG_FLAG_WAITMODE_OR | CYG_FLAG_WAITMODE_CLR);
#else
		wait_for_completion(&pstat->asoc_unref_done);
#endif
		ret = 1;
	}
	
	return ret;
}

int asoc_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;
	
	SMP_LOCK_ASOC_LIST(flags);
	
	if (list_empty(&pstat->asoc_list)) {
		list_add_tail(&pstat->asoc_list, &priv->asoc_list);
		pstat->asoc_list_refcnt = 1;
		ret = 1;
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	
	return ret;
}
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI

//return value: Auto site-survey level
//0(SS_LV_WSTA), STA connect to root AP/VAP
//1(SS_LV_WOSTA), No STA connected to root AP/VAP
//2(SS_LV_ROOTFUNCOFF), root AP only and func_off=1
int get_ss_level(struct rtl8192cd_priv *priv)
{
	int idx=0;
  
	if( GET_ROOT(priv)->pmib->miscEntry.func_off ){

        #ifdef MBSSID                            
        if(GET_ROOT(priv)->pmib->miscEntry.vap_enable==0)
    		return  SS_LV_ROOTFUNCOFF;
        else{
			for (idx=0; idx<RTL8192CD_NUM_VWLAN; idx++) {
				if (IS_DRV_OPEN(GET_ROOT(priv)->pvap_priv[idx])&& 
                    (GET_ROOT(priv)->pvap_priv[idx]->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
                    (GET_ROOT(priv)->pvap_priv[idx]->assoc_num)){
					return SS_LV_WSTA;
				}
			}
        }
        #else
    		return SS_LV_ROOTFUNCOFF;        
        #endif
       
	}
    else{
        
		if((GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE)  &&  GET_ROOT(priv)->assoc_num)
			return SS_LV_WSTA;

        #ifdef MBSSID                    
		if(GET_ROOT(priv)->pmib->miscEntry.vap_enable==1) {
			for (idx=0; idx<RTL8192CD_NUM_VWLAN; idx++) {
				if (IS_DRV_OPEN(GET_ROOT(priv)->pvap_priv[idx])&& 
                    (GET_ROOT(priv)->pvap_priv[idx]->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
                    (GET_ROOT(priv)->pvap_priv[idx]->assoc_num))
                {
					return SS_LV_WSTA;
				}
			}
		}
        #endif
	}

    //default
    return SS_LV_WOSTA;

}

void syncMulticastCipher(struct rtl8192cd_priv *priv, struct bss_desc *bss_target)
{
	int mcipher = 1;
	// set Multicast Cipher as same as AP's
	if (priv->pmib->dot11RsnIE.rsnie[0] == _RSN_IE_1_) {
		if(bss_target->t_stamp[0] & BIT(4))
			mcipher = 4;
		else if(bss_target->t_stamp[0] & BIT(2))
			mcipher = 2;									
		priv->pmib->dot11RsnIE.rsnie[11] = mcipher;
		priv->wpa_global_info->AuthInfoBuf[11] = mcipher;
	} else if(priv->pmib->dot11RsnIE.rsnie[0] == _RSN_IE_2_) {
		if(bss_target->t_stamp[0] & BIT(20))
			mcipher = 4;
		else if(bss_target->t_stamp[0] & BIT(18))
			mcipher = 2;		
		priv->pmib->dot11RsnIE.rsnie[7] = mcipher;
		priv->wpa_global_info->AuthInfoBuf[7] = mcipher;								
	}			
}

unsigned int isDHCPpkt(struct sk_buff *pskb)
{
#define DHCP_MAGIC 0x63825363

	struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	        __u8    ihl:4,
	                version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	        __u8    version:4,
	                ihl:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	        __u8    tos;
	        __u16   tot_len;
	        __u16   id;
	        __u16   frag_off;
	        __u8    ttl;
	        __u8    protocol;
#if 0
	        __u16   check;
	        __u32   saddr;
	        __u32   daddr;
#endif
	};

	struct udphdr {
	        __u16   source;
	        __u16   dest;
	        __u16   len;
	        __u16   check;
	};

	struct dhcpMessage {
		u_int8_t op;
		u_int8_t htype;
		u_int8_t hlen;
		u_int8_t hops;
		u_int32_t xid;
		u_int16_t secs;
		u_int16_t flags;
		u_int32_t ciaddr;
		u_int32_t yiaddr;
		u_int32_t siaddr;
		u_int32_t giaddr;
		u_int8_t chaddr[16];
		u_int8_t sname[64];
		u_int8_t file[128];
		u_int32_t cookie;
#if 0
		u_int8_t options[308]; /* 312 - cookie */
#endif
	};

	unsigned short protocol = 0;
	struct iphdr* iph;
	struct udphdr *udph;
	struct dhcpMessage *dhcph;

	protocol = *((unsigned short *)(pskb->data + 2 * ETH_ALEN));

	if(protocol == __constant_htons(ETH_P_IP)) { /* IP */
		iph = (struct iphdr *)(pskb->data + ETH_HLEN);

		if(iph->protocol == 17) { /* UDP */
			udph = (struct udphdr *)((unsigned long)iph + (iph->ihl << 2));
			dhcph = (struct dhcpMessage *)((unsigned long)udph + sizeof(struct udphdr));

			if ((unsigned long)dhcph & 0x03) { //not 4-byte alignment
				u_int32_t cookie;
				char *pdhcphcookie;
				char *pcookie = (char *)&cookie;

				pdhcphcookie = (char *)&dhcph->cookie;
				pcookie[0] = pdhcphcookie[0];
				pcookie[1] = pdhcphcookie[1];
				pcookie[2] = pdhcphcookie[2];
				pcookie[3] = pdhcphcookie[3];
				if(cookie == htonl(DHCP_MAGIC))
					return TRUE;
			}
			else {
				if(dhcph->cookie == htonl(DHCP_MAGIC))
					return TRUE;
			}
		}
	}

	return FALSE;
}

#ifdef UNIVERSAL_REPEATER
#if defined (__ECOS) || defined(RTK_NL80211) || (defined(CONFIG_OPENWRT_SDK) && defined(__LINUX_3_10__))//wrt-dhcp  //TBD //eric-sync ?? __LINUX_3_10__
int send_arp_response(struct rtl8192cd_priv *priv, unsigned int *dip, unsigned int *sip, unsigned char *dmac, unsigned char *smac)
{
	return -1;
}
#else
int send_arp_response(struct rtl8192cd_priv *priv, unsigned int *dip, unsigned int *sip, unsigned char *dmac, unsigned char *smac)
{
	unsigned int ret = -1;
	struct sk_buff *arp_skb = NULL;
	struct rtl_arphdr *arp=NULL;
	unsigned char *ptr;
	int hlen, tlen;

	hlen = LL_RESERVED_SPACE(priv->dev);
	tlen = priv->dev->needed_tailroom;
	arp_skb = __alloc_skb(arp_hdr_len(priv->dev) + hlen + tlen, GFP_ATOMIC, 0, -1);

	if (arp_skb == NULL)
		goto err_out;

	skb_reserve(arp_skb, LL_RESERVED_SPACE(priv->dev));
	skb_reset_network_header(arp_skb);
	arp = (struct rtl_arphdr *) skb_put(arp_skb, arp_hdr_len(priv->dev));
	arp_skb->dev = priv->dev;
	arp_skb->protocol = htons(ETH_P_ARP);
#if 0
	//without consideration of VLAN
//#ifdef CONFIG_RTK_VLAN_SUPPORT
	arp_skb->tag = arp_tag;
#if defined(CONFIG_RTK_VLAN_NEW_FEATURE)
	arp_skb->src_info = arp_info;
#endif
#endif
	if (dev_hard_header(arp_skb, priv->dev, htons(ETH_P_ARP), dmac, smac, arp_skb->len) < 0) {
		kfree_skb(arp_skb);
		goto err_out;
	}
	// ======================
	// build new arp response
	// ======================
	//arp response
	ptr = (unsigned char *)(arp + 1);
	arp->ar_op = htons(ARPOP_REPLY);
	arp->ar_hrd = htons(priv->dev->type);
	arp->ar_pro = htons(ETH_P_IP);
	arp->ar_hln = priv->dev->addr_len;;
	arp->ar_pln = 4;	//for ipv4
	//sender mac and ip
	memcpy(ptr, smac, 6);
	ptr += arp->ar_hln;
	memcpy(ptr, sip, 4);
	//printk("sender mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",pstat->hwaddr[0],pstat->hwaddr[1],pstat->hwaddr[2],pstat->hwaddr[3],pstat->hwaddr[4],pstat->hwaddr[5],pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
	//target mac and ip
	ptr += arp->ar_pln;
	memcpy(ptr, dmac, MACADDRLEN);
	ptr += arp->ar_hln;
	memcpy(ptr, dip, 4);
	//printk("target mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",newskb->data[0],newskb->data[1],newskb->data[2],newskb->data[3],newskb->data[4],newskb->data[5],ptarget[0],ptarget[1],ptarget[2],ptarget[3]);

	if(arp_skb){
		int i=0;       
		arp_skb->cb[2] = (char)0xff;         // not do aggregation
		memcpy(arp_skb->cb+10,arp_skb->data,6);
		SMP_LOCK_XMIT(x);
		if(__rtl8192cd_start_xmit(arp_skb, priv->dev, 1)) {
			DEBUG_ERR("%s %d ARP response sent failed\n",__func__,__LINE__);
		} else {
			ret = 0;
			DEBUG_INFO("%s %d ARP response for %02x:%02x:%02x:%02x:%02x:%02x to %s sent\n",__func__,__LINE__,
				smac[0],smac[1],smac[2],smac[3],smac[4],smac[5],priv->dev->name);
			for(i=0;i<arp_skb->len;i++){
				DEBUG_INFO("%02x",arp_skb->data[i]);
				if(i/16 && ((i%16) == 0))
					DEBUG_INFO("\n");
			}
			DEBUG_INFO("\n");
		}
		SMP_UNLOCK_XMIT(x);
	} else {
		DEBUG_ERR("%s %d Alloc skb failed\n",__func__,__LINE__);
	}

	return ret;

err_out:
	return ret;
}
#endif

void snoop_STA_IP(struct sk_buff *pskb, struct rtl8192cd_priv *priv)
{
	#define DHCP_MAGIC 0x63825363

struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	        __u8    ihl:4,
	                version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	        __u8    version:4,
	                ihl:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	        __u8    tos;
	        __u16   tot_len;
	        __u16   id;
	        __u16   frag_off;
	        __u8    ttl;
	        __u8    protocol;
#if 0
	        __u16   check;
	        __u32   saddr;
	        __u32   daddr;
#endif
};

struct udphdr {
	        __u16   source;
	        __u16   dest;
	        __u16   len;
	        __u16   check;
};

struct dhcpMessage {
		u_int8_t op;
		u_int8_t htype;
		u_int8_t hlen;
		u_int8_t hops;
		u_int32_t xid;
		u_int16_t secs;
		u_int16_t flags;
		u_int32_t ciaddr;
		u_int32_t yiaddr;
		u_int32_t siaddr;
		u_int32_t giaddr;
		u_int8_t chaddr[16];
		u_int8_t sname[64];
		u_int8_t file[128];
		u_int32_t cookie;
#if 0
		u_int8_t options[308]; /* 312 - cookie */
#endif
};
	struct rtl8192cd_priv *ap_priv;
	struct stat_info *pstat;
	struct iphdr* iph;
	struct udphdr *udph;
	struct dhcpMessage *dhcph;
	struct list_head *phead, *plist;
	
	iph = (struct iphdr *)(pskb->data + ETH_HLEN);
	udph = (struct udphdr *)((unsigned int)iph + (iph->ihl << 2));
	dhcph = (struct dhcpMessage *)((unsigned int)udph + sizeof(struct udphdr));

	if(IS_VXD_INTERFACE(priv)) {
        ap_priv = GET_ROOT(priv);
    } else {
		DEBUG_INFO("Receive DHCP response but interface is not VXD\n");
        return ;
	}

	//dhcp reply only
	if(ap_priv && IS_DRV_OPEN(ap_priv) && (dhcph->op == 2)) {
        unsigned char sta_ip[4];

		memcpy(sta_ip,&dhcph->yiaddr,4);
		DEBUG_INFO("[%s]External DHCP Server give IP[%d.%d.%d.%d]\n",priv->dev->name,sta_ip[0],sta_ip[1],sta_ip[2],sta_ip[3]);
		if( send_arp_response(priv,&dhcph->siaddr,&dhcph->yiaddr,pskb->data+MACADDRLEN,&dhcph->chaddr[0]) )
			DEBUG_ERR("Send ARP failed\n");

		return;
	}
}
#endif	//UNIVERSAL_REPEATER

/* Do defered channel scan when,
   1.There is not any station connected to active AP interafce(Root AP/VAP)
   2.Scan is not requested by wscd
   3.Scan is not requested by Station mode without any VAP
*/
int should_defer_ss(struct rtl8192cd_priv *priv)
{
	int ret=0;

	if(GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & (WIFI_STATION_STATE|WIFI_ADHOC_STATE)) {
		ret = 0;
	} else {
		if(get_ss_level(priv) < SS_LV_ROOTFUNCOFF) {
			if((priv->site_survey->ss_channel == 100) && (priv->pmib->miscEntry.ss_delay)) {
				ret=1;
				DEBUG_INFO("%s Sitesurvey defered\n",priv->dev->name);
			}
		}
	}

	if(priv->ss_req_ongoing == SSFROM_WSC)
		ret = 0;

	return ret;
}
#ifdef USE_OUT_SRC
unsigned char *Get_Adaptivity_Version(void)
{
	return ADAPTIVITY_VERSION;
}
#endif
#if defined(CONFIG_RTL_SIMPLE_CONFIG)
int rtk_sc_start_simple_config(struct rtl8192cd_priv *priv)
{
	int i=0;
	unsigned char null_mac[]={0,0,0,0,0,0};
	
	strcpy(g_sc_ifname, priv->dev->name);
	g_sc_debug = priv->pmib->dot11StationConfigEntry.sc_debug;
	if(priv->pmib->dot11StationConfigEntry.sc_debug)
		panic_printk("Start simple config now!\n");
	rtk_sc_clean_profile_value();
	
#ifdef UNIVERSAL_REPEATER	
	if(IS_VXD_INTERFACE(priv))
	{
		if(priv->simple_config_status == 0)
			memcpy(priv->pmib->dot11Bss.bssid, null_mac, MACADDRLEN);
	}
#endif

	if(priv->simple_config_status == 0)
		priv->simple_config_status = 1;

	RTL_W32(RCR, RTL_R32(RCR) | RCR_AAP);	// Accept Destination Address packets.

	priv->pmib->dot11StationConfigEntry.sc_status = 1;
	priv->pmib->dot11StationConfigEntry.sc_vxd_rescan_time = CHECK_VXD_SC_TIMEOUT;
	
	return 0;
}

int rtk_sc_start_connect_target(struct rtl8192cd_priv *priv)
{
	int i=0;
#ifdef UNIVERSAL_REPEATER		
	if(IS_VXD_INTERFACE(priv))
	{
#ifdef MBSSID
		if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
			for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
				if (IS_DRV_OPEN(priv->pvap_priv[i]))
					priv->pvap_priv[i]->pmib->dot11OperationEntry.keep_rsnie = 1;
			}
		}
#endif
	}
#endif
	RTL_W32(RCR, RTL_R32(RCR) & ~RCR_AAP);	// Disable Accept Destination Address packets.
	RTL_W8(TXPAUSE, 0xff);
	rtl8192cd_close(priv->dev);
#ifdef UNIVERSAL_REPEATER	
	if( (priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root == 1) && IS_VXD_INTERFACE(priv))
	{
		GET_ROOT(priv)->pmib->dot11StationConfigEntry.sc_enabled = 0;
		rtl8192cd_close(GET_ROOT(priv)->dev);
		rtl8192cd_open(GET_ROOT(priv)->dev);
		while(!netif_running(GET_ROOT(priv)->dev))
			delay_ms(10);
	}
#endif
	rtl8192cd_open(priv->dev);
	RTL_W8(TXPAUSE, 0x00);
	priv->simple_config_status = 5;

	return 0;
}
#ifdef UNIVERSAL_REPEATER	
int rtk_sc_sync_vxd_to_root(struct rtl8192cd_priv * priv)
{
	if(IS_VXD_INTERFACE(priv))
	{
		if(priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root == 1)
		{
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11AuthAlgrthm = priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11EnablePSK = priv->pmib->dot1180211AuthEntry.dot11EnablePSK;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
			GET_ROOT(priv)->pmib->dot118021xAuthEntry.dot118021xAlgrthm = priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11WPA2Cipher = priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11WPACipher = priv->pmib->dot1180211AuthEntry.dot11WPACipher;
			strcpy(GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11PassPhrase, priv->pmib->dot1180211AuthEntry.dot11PassPhrase);
			strcpy(&(GET_ROOT(priv)->pmib->dot11DefaultKeysTable.keytype[0]), &(priv->pmib->dot11DefaultKeysTable.keytype[0]));
			strcpy(GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11DesiredSSID, priv->pmib->dot11StationConfigEntry.dot11DesiredSSID);
			strcpy(GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11SSIDtoScan, priv->pmib->dot11StationConfigEntry.dot11SSIDtoScan);
			GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen= priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen;
			GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11SSIDtoScanLen= priv->pmib->dot11StationConfigEntry.dot11SSIDtoScanLen;
		}
	}

	return 0;
}
#else
int rtk_sc_sync_vxd_to_root(struct rtl8192cd_priv * priv)
{
	return 0;
}

#endif

int rtk_sc_stop_simple_config(struct rtl8192cd_priv *priv)
{
	int i=0, ack_num=0;
	int sc_security_type = rtk_sc_get_security_type();

	if (OPMODE & WIFI_ASOC_STATE)
	{
		rtk_sc_set_value(SC_ENABLED, 0);
		rtk_sc_set_value(SC_PRIV_STATUS, 0);
		rtk_sc_set_value(SC_STATUS, 10+sc_security_type);
	}
	else
		rtk_sc_set_value(SC_STATUS, -1);

	return 0;
}

struct net_device * rtl_get_dev_by_wlan_name(char *name)
{
	return __dev_get_by_name(&init_net, name);
}

int rtk_sc_set_value(unsigned int id, unsigned int value)
{
	struct net_device *dev;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;
#endif

	switch(id)
	{
		case SC_ENABLED:
			priv->pmib->dot11StationConfigEntry.sc_enabled = value;
			break;
		case SC_DURATION_TIME:
			priv->pmib->dot11StationConfigEntry.sc_duration_time = value;
			break;
		case SC_GET_SYNC_TIME:
			priv->pmib->dot11StationConfigEntry.sc_get_sync_time = value;
			break;
		case SC_GET_PROFILE_TIME:
			priv->pmib->dot11StationConfigEntry.sc_get_profile_time = value;
			break;
		case SC_VXD_RESCAN_TIME:
			priv->pmib->dot11StationConfigEntry.sc_vxd_rescan_time = value;
			break;
		case SC_PIN_ENABLED:
			priv->pmib->dot11StationConfigEntry.sc_pin_enabled = value;
			break;
		case SC_STATUS:
			priv->pmib->dot11StationConfigEntry.sc_status = value;
			break;
		case SC_DEBUG:
			priv->pmib->dot11StationConfigEntry.sc_debug = value;
			break;	
		case SC_CHECK_LINK_TIME:
			priv->pmib->dot11StationConfigEntry.sc_check_link_time = value;
			break;			
		case SC_SYNC_VXD_TO_ROOT:
			priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root = value;
			break;			
		case SC_ACK_ROUND:
			priv->pmib->dot11StationConfigEntry.sc_ack_round = value;
			break;			
		case SC_CONTROL_IP:
			priv->pmib->dot11StationConfigEntry.sc_control_ip = value;
			break;	
		case SC_PRIV_STATUS:
			priv->simple_config_status = value;
			break;
		case SC_CONFIG_TIME:
			priv->simple_config_time = value;
			break;
		default:
			break;
	}
	return 0;
}

int rtk_sc_get_value(unsigned int id)
{
	struct net_device *dev;
	int value=0;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;
#endif

	switch(id)
	{
		case SC_ENABLED:
			value = priv->pmib->dot11StationConfigEntry.sc_enabled;
			break;
		case SC_DURATION_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_duration_time;
			break;
		case SC_GET_SYNC_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_get_sync_time;
			break;
		case SC_GET_PROFILE_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_get_profile_time;
			break;
		case SC_VXD_RESCAN_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_vxd_rescan_time;
			break;
		case SC_PIN_ENABLED:
			value = priv->pmib->dot11StationConfigEntry.sc_pin_enabled;
			break;
		case SC_STATUS:
			value = priv->pmib->dot11StationConfigEntry.sc_status;
			break;
		case SC_DEBUG:
			value = priv->pmib->dot11StationConfigEntry.sc_debug;
			break;	
		case SC_CHECK_LINK_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_check_link_time;
			break;			
		case SC_SYNC_VXD_TO_ROOT:
			value = priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root;
			break;			
		case SC_ACK_ROUND:
			value = priv->pmib->dot11StationConfigEntry.sc_ack_round;
			break;			
		case SC_CONTROL_IP:
			value = priv->pmib->dot11StationConfigEntry.sc_control_ip;
			break;	
		case SC_PRIV_STATUS:
			value = priv->simple_config_status;
			break;
		case SC_CONFIG_TIME:
			value = priv->simple_config_time;
			break;
		default:
			break;
	}
	return value;
}

int rtk_sc_set_string_value(unsigned int id, unsigned char *value)
{
	struct net_device *dev;
	int i=0;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;
#endif
	switch(id)
	{
		case SC_PIN:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_pin, value);
			break;
		case SC_DEFAULT_PIN:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_default_pin, value);
			break;
		case SC_PASSWORD:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_passwd, value);
			break;
		case SC_DEVICE_NAME:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_device_name, value);
			break;
		case SC_DEVICE_TYPE:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_device_type, value);
			break;
		case SC_SSID:
			strcpy(priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, value);
			strcpy(priv->pmib->dot11StationConfigEntry.dot11SSIDtoScan, value);
			priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen= strlen(value);
			priv->pmib->dot11StationConfigEntry.dot11SSIDtoScanLen= strlen(value);
			break;
		case SC_BSSID:
			for(i=0; i<6; i++)
			{
				priv->pmib->dot11StationConfigEntry.dot11DesiredBssid[i] = value[i];
			}
			break;
		default:
			panic_printk("RTL Simple Config don't support this mib setting now!\n");
	}
	return 0;
}

int rtk_sc_get_string_value(unsigned int id, unsigned char *value)
{
	struct net_device *dev;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;
#endif
	switch(id)
	{
		case SC_PIN:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_pin);
			break;
		case SC_DEFAULT_PIN:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_default_pin);
			break;
		case SC_PASSWORD:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_passwd);
			break;
		case SC_DEVICE_NAME:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_device_name);
			break;
		case SC_DEVICE_TYPE:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_device_type);
			break;
		default:
			panic_printk("RTL Simple Config don't support this mib setting now!\n");
			break;
	}
	return 0;
}

int rtk_sc_get_security_type()
{
	return g_sc_security_type;
}

int set_wep_key(struct rtl8192cd_priv *priv, int type)
{
	int i, length;
	unsigned char value;
	unsigned char wep_key[26]={0};
	if(type == 0)
		length=10;
	else if(type == 1)
		length=26;
	for(i=0; i<length; i++)
	{
		value = priv->pmib->dot11StationConfigEntry.sc_passwd[i];
		if((value >=0x30) && (value <=0x39))
			value -= 0x30;
		else if((value >= 'a')&&(value <= 'z'))
			value -= 0x57;
		else if((value >= 'A')&&(value <= 'Z'))
			value -= 0x37;
		value &= 0xf;
		if(i%2 == 0)
			wep_key[i/2] = value<<4;
		else
			wep_key[i/2] += value;
	}
	memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), wep_key, length/2);
}

int rtk_sc_check_security(struct rtl8192cd_priv * priv,struct bss_desc * bss)
{
	int length=0, i=0;
	int status = rtk_sc_get_value(SC_STATUS);
	length = strlen(priv->pmib->dot11StationConfigEntry.sc_passwd);
	if ((bss->capability&BIT(4)) == 0)
	{
		GET_MIB(priv)->dot1180211AuthEntry.dot11AuthAlgrthm = 0;
		GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm=_NO_PRIVACY_;
		GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 0;
		rtk_sc_set_value(SC_CHECK_LINK_TIME, 2);
		g_sc_security_type = 0;
	}
	else if((bss->capability&BIT(4)))
	{
		if(bss->t_stamp[0] == 0)
		{
			GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 0;
			if((length == 5) || (length == 10))
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm = _WEP_40_PRIVACY_;
				if(length == 5)
				{
					memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), priv->pmib->dot11StationConfigEntry.sc_passwd, 5);
					g_sc_security_type = 1;
				}
				else if(length == 10)
				{
					set_wep_key(priv, 0);
					g_sc_security_type = 2;
				}
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyKeyIndex = 0;
			}
			else if((length == 13) || (length == 26))
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm = _WEP_104_PRIVACY_;
				if(length == 13)
				{
					memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), priv->pmib->dot11StationConfigEntry.sc_passwd, 5);
					g_sc_security_type = 3;
				}
				else
				{
					set_wep_key(priv, 1);
					g_sc_security_type = 4;
				}
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyKeyIndex = 0;
			}
			rtk_sc_set_value(SC_CHECK_LINK_TIME, 2);
		}
		else
		{
			GET_MIB(priv)->dot1180211AuthEntry.dot11AuthAlgrthm = 2;
			GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm = 2;
			strcpy((char *)GET_MIB(priv)->dot1180211AuthEntry.dot11PassPhrase, priv->pmib->dot11StationConfigEntry.sc_passwd);
			if(bss->t_stamp[0] & 0xffff0000)
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 2;
				if(((bss->t_stamp[0] & 0xf0000000) >> 28) == 0x4)
				{
					GET_MIB(priv)->dot118021xAuthEntry.dot118021xAlgrthm = 1;
				}
				if(((bss->t_stamp[0] & 0x0f000000) >> 24) == 0x5)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPA2Cipher = 8;
					g_sc_security_type = 5;
				}
				else if(((bss->t_stamp[0] & 0x0f000000) >> 24) == 0x4)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPA2Cipher = 8;
					g_sc_security_type = 5;
				}
				else if(((bss->t_stamp[0] & 0x0f000000) >> 24) == 0x1)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPA2Cipher = 2;
					g_sc_security_type = 6;
				}
			}
			else if(bss->t_stamp[0] & 0x0000ffff)
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 1;
				if(((bss->t_stamp[0] & 0x0000f000) >> 12) == 0x4)
				{
					GET_MIB(priv)->dot118021xAuthEntry.dot118021xAlgrthm = 0;
				}
				if(((bss->t_stamp[0] & 0x00000f00) >> 8) == 0x5)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPACipher = 8;	
					g_sc_security_type = 7;
				}
				else if(((bss->t_stamp[0] & 0x00000f00) >> 8) == 0x4)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPACipher = 8;	
					g_sc_security_type = 7;
				}
				else if(((bss->t_stamp[0] & 0x00000f00) >> 8) == 0x1)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPACipher = 2;	
					g_sc_security_type = 8;
				}
			}
		}
	}
	if(status == 3)
	{
		rtk_sc_set_value(SC_PRIV_STATUS, 4);
		rtk_sc_set_value(SC_STATUS, 4);
	}
	rtk_sc_sync_vxd_to_root(priv);
	rtk_sc_set_value(SC_CONFIG_TIME, 0);
	rtk_sc_start_connect_target(priv);
	return 0;
}
#endif
