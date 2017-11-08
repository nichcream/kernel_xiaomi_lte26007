/*
 *  Header file defines some common inline funtions
 *
 *  $Id: osdep_service.h,v 1.10.2.4 2010/11/09 09:10:03 button Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __OSDEP_SERVICE_H_
#define __OSDEP_SERVICE_H_

#ifdef __KERNEL__
#include <linux/version.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif

#ifdef CONFIG_USB_HCI
#include <linux/usb.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21))
#include <linux/usb_ch9.h>
#else
#include <linux/usb/ch9.h>
#endif
#endif // CONFIG_USB_HCI

#ifdef CONFIG_SDIO_HCI
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#endif // CONFIG_SDIO_HCI

#ifdef CONFIG_PCI_HCI
#include <linux/pci.h>
#endif

#include <linux/spinlock.h>
#include <linux/circ_buf.h>
#endif // __KERNEL__
#include <linux/wakelock.h>   //zyj
typedef struct semaphore _sema;
typedef spinlock_t	_lock;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
typedef struct mutex 		_mutex;
#else
typedef struct semaphore	_mutex;
#endif

typedef	struct sk_buff	_pkt;
typedef struct list_head _list;
struct __queue {
	_list queue;
	int qlen;
	_lock lock;
};
typedef struct __queue _queue;
typedef unsigned long _irqL;
typedef struct timer_list _timer;
typedef struct	net_device * _nic_hdl;

#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field)	((int)&(((type *)0)->field))
#endif

#define _RND(sz, r) ((((sz)+(r)-1)/(r))*(r))

__inline static u32 _RND4(u32 sz)
{
	return ((sz + 3) & ~3);
}

__inline static u32 _RND8(u32 sz)
{
	return ((sz + 7) & ~7);
}

__inline static u32 _RND128(u32 sz)
{
	return ((sz + 127) & ~127);
}

__inline static u32 _RND256(u32 sz)
{
	return ((sz + 255) & ~255);
}

__inline static u32 _RND512(u32 sz)
{
	return ((sz + 511) & ~511);
}

#define LIST_CONTAINOR(ptr, type, member) \
        ((type *)((char *)(ptr)-(SIZE_T)(&((type *)0)->member)))


__inline static void _rtw_mutex_init(_mutex *pmutex)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	mutex_init(pmutex);
#else
	init_MUTEX(pmutex);
#endif
}

__inline static void _rtw_mutex_free(_mutex *pmutex)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	mutex_destroy(pmutex);
#else
#endif
}

__inline static void _enter_critical_mutex(_mutex *pmutex, _irqL *pirqL)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	mutex_lock(pmutex);
#else
	down(pmutex);
#endif
}

__inline static void _exit_critical_mutex(_mutex *pmutex, _irqL *pirqL)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	mutex_unlock(pmutex);
#else
	up(pmutex);
#endif
}

__inline static void _rtw_spinlock_init(_lock *plock)
{
	spin_lock_init(plock);
}

__inline static void _rtw_spinlock_free(_lock *plock) {}

__inline static void _rtw_spinlock(_lock	*plock)
{
	spin_lock(plock);
}

__inline static void _rtw_spinunlock(_lock *plock)
{
	spin_unlock(plock);
}
__inline static void _rtw_spinlock_ex(_lock	*plock)
{
	spin_lock(plock);
}

__inline static void _rtw_spinunlock_ex(_lock *plock)
{
	spin_unlock(plock);
}

__inline static void _enter_critical(_lock *plock, _irqL *pirqL)
{
	spin_lock_irqsave(plock, *pirqL);
}

__inline static void _exit_critical(_lock *plock, _irqL *pirqL)
{
	spin_unlock_irqrestore(plock, *pirqL);
}

__inline static void _enter_critical_ex(_lock *plock, _irqL *pirqL)
{
	spin_lock_irqsave(plock, *pirqL);
}

__inline static void _exit_critical_ex(_lock *plock, _irqL *pirqL)
{
	spin_unlock_irqrestore(plock, *pirqL);
}

__inline static void _enter_critical_bh(_lock *plock, _irqL *pirqL)
{
	spin_lock_bh(plock);
}

__inline static void _exit_critical_bh(_lock *plock, _irqL *pirqL)
{
	spin_unlock_bh(plock);
}

__inline static void _rtw_init_listhead(_list *list)
{
        INIT_LIST_HEAD(list);
}

__inline static u32	rtw_is_list_empty(_list *phead)
{
	return (list_empty(phead) ? TRUE: FALSE);
}

__inline static void rtw_list_insert_head(_list *plist, _list *phead)
{
	list_add(plist, phead);
}

__inline static void rtw_list_insert_tail(_list *plist, _list *phead)
{
	list_add_tail(plist, phead);
}

__inline static void rtw_list_delete(_list *plist)
{
	list_del_init(plist);
}

__inline static void rtw_list_splice(_list *plist, _list *phead)
{
	list_splice_init(plist, phead);
}

__inline static _list *get_next(_list *list)
{
	return list->next;
}

__inline static _list *get_list_head(_queue *queue)
{
	return (&(queue->queue));
}

__inline static void _rtw_init_queue(_queue *pqueue)
{
	_rtw_init_listhead(&(pqueue->queue));
	_rtw_spinlock_init(&(pqueue->lock));
	pqueue->qlen = 0;
}

__inline static u32 _rtw_queue_empty(_queue	*pqueue)
{
	return (rtw_is_list_empty(&(pqueue->queue)));
}

__inline static u32 rtw_end_of_queue_search(_list *head, _list *plist)
{
	return ((head == plist)? TRUE : FALSE);
}

__inline static void _init_timer(_timer *ptimer, _nic_hdl nic_hdl,void *pfunc,void* cntx)
{
	//setup_timer(ptimer, pfunc,(u32)cntx);	
	init_timer(ptimer);
	ptimer->function = pfunc;
	ptimer->data = (unsigned long)cntx;
}

__inline static void _set_timer(_timer *ptimer,u32 delay_time)
{	
	mod_timer(ptimer , jiffies+msecs_to_jiffies(delay_time));
}

__inline static void _cancel_timer(_timer *ptimer,u8 *bcancelled)
{
	del_timer_sync(ptimer); 	
	*bcancelled = TRUE;//TRUE ==1; FALSE==0
}

__inline static unsigned char _cancel_timer_ex(_timer *ptimer)
{
	return del_timer_sync(ptimer);
}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21))
static inline unsigned char *skb_end_pointer(const struct sk_buff *skb)
{
	return skb->end;
}
static inline unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}
static inline void skb_reset_tail_pointer(struct sk_buff *skb)
{
	skb->tail = skb->data;
}

static inline void skb_set_tail_pointer(struct sk_buff *skb, const int offset)
{
	skb->tail = skb->data + offset;
}
#endif


#endif // __OSDEP_SERVICE_H_

