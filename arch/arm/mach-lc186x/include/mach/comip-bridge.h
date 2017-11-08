/*
 * LC186X Bridge driver
 *
 * Copyright (C) 2013 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef COMIP_BRIDGE_H
#define COMIP_BRIDGE_H
#include <mach/comip-modem.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>

/* circular buffer */
struct bridge_buf {
	unsigned	buf_size;/*buf length*/
	char		*buf_buf;/*MEMORY start addr*/
	char		*buf_get;/*begin address for read*/
	char		*buf_put;/*begin address for write*/
};

struct buf_lbridge_head {
	u32		RcvDataOffset;/*begin addr for read*/
	u32		RcvIrqCount;
	u32		RcvDataCount;
	u32		RcvReserved;
	u32		SndDataOffset;/*begin addr for write*/
	u32		SndIrqCount;
	u32		SndDataCount;
	u32		SndReserved;
};

struct buf_hbridge_head {
	u32		RcvFrmOffset;/*read frame offset of receiver*/
	u32		RcvFrmIrqCount;/*frame irq count of receiver */
	u32		RcvFrmCount;/*frame count of receiver*/
	u32		RcvFCIrqCount;/*clear flow ctl irq count of receiver*/
	u32		SndFrmOffset;/*write frame offset of sender*/
	u32		SndFrmIrqCount;/*frame irq count of sender */
	u32		SndFrmCount;/*frame count of sender*/
	u32		SndFCIrqCount;/*received flow ctl clear irq count of sender*/
	u32		RcvNotBusy;/*flow ctl status of receiver*/
	u32		Reserved1;
	u32		Reserved2;
	u32		Reserved3;
};/*frame and flow ctl*/

struct bridge_frame {
	unsigned char	*pData;
	u32		Count;
};

struct bridge_rxdata{
	struct list_head	list;
	struct bridge_frame	frame;
	unsigned char		*vData;
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
	dma_addr_t		pData;
#endif
	void			*context;
};

struct bridge_info {
	u32			membase;
	u32			memlength;
	char			*pdata;/*data begin addr == buf_buf*/
	u32			length;/*data area length*/
	u32			pkt_num;
	u32			pkt_size;
	struct bridge_buf	circular_buf;
};

enum bridge_debug_cmd{
	start_dump_read_buf_cmd,
	start_dump_write_buf_cmd,
	start_write_to_read_cmd,
	start_read_to_write_cmd,
	stop_dump_read_buf_cmd,
	stop_dump_write_buf_cmd,
	stop_write_to_read_cmd,
	stop_read_to_write_cmd,
};

struct bridge_debug_info{
	u64 t;
	u32 count;
};

struct bridge_drvdata;
struct comip_bridge_operations {
	int (*drvinit)(struct bridge_drvdata *, struct modem_bridge_info*);
	int (*drvuninit)(struct bridge_drvdata *);
	int (*rxsavebuf)(struct bridge_drvdata *);
	int (*rxstorebuf)(struct bridge_drvdata *);
	int (*open)(struct bridge_drvdata *);
	int (*close)(struct bridge_drvdata *);
	unsigned int (*poll)(struct bridge_drvdata *);
	ssize_t (*xmit)(struct bridge_drvdata *, const char __user*,	size_t, bool, bool);
	ssize_t (*recv_raw)(struct bridge_drvdata *, char __user *,	size_t);
	ssize_t (*recv_pkt)(struct bridge_drvdata *, u32 *,	size_t, bool);
	void (*irq_en)(unsigned int);
	void (*irq_dis)(unsigned int);
	int (*set_fc)(struct bridge_drvdata *);
	int (*clr_fc)(struct bridge_drvdata *);
	ssize_t (*info_show)(struct bridge_drvdata*, char *);
	ssize_t (*reg_show)(struct bridge_drvdata*, char *);
	bool (*test_rcvbusy)(struct bridge_drvdata *);
};
struct bridge_drvdata {
	char					name[12];
	Bridge_Type				type;
	Bridge_Pro				property;
	struct miscdevice			misc_dev;	/* misc device structure */
	struct net_device			*net_dev;
	struct mutex				read_sem;
	struct mutex				write_sem;
	spinlock_t				op_lock;
	struct wake_lock			wakelock;
	u32					rxirq;/*irq number for read*/
	u32					txirq;/*irq number for write*/
	u32					rxctl_irq;
	u32					txctl_irq;
	u32					is_open;
	u32					f_open;/*first open flag*/
	u32					unit;
	u32					tx_timeout;
	u32					baseaddr;
	wait_queue_head_t			rxwait_queue;
	wait_queue_head_t			txwait_queue;
	struct bridge_debug_info		rx_int_dinfo;
	struct bridge_debug_info		rx_dinfo;
	struct bridge_debug_info		tx_dinfo;
	struct bridge_debug_info		tx_ctl_dinfo;
	struct bridge_info			rxinfo;
	struct bridge_info			txinfo;
	struct list_head			rxbuf_list;
	struct list_head			rxdata_list;
	struct list_head			rx_list;
	struct info				dbg_info;
	char *					rx_save_buf;
	int (*alloc_rcvbuf)(struct bridge_drvdata *, struct bridge_rxdata *,u32);
	void (*free_rcvbuf)(struct bridge_rxdata *);
	irqreturn_t (*rxhandle)(struct bridge_drvdata *);
	irqreturn_t (*ctlhandle)(struct bridge_drvdata *);
	const struct comip_bridge_operations *ops;
};

#define GET_BRIDGE_TX_LEN	_IO('B', 1)
#define GET_BRIDGE_RX_LEN 	_IO('B', 2)
#define SET_BRIDGE_FLOW_CTL 	_IO('B', 3)
#define CLEAR_BRIDGE_FLOW_CTL	_IO('B', 4)

#define BRIDGE_BUF_COUNT		(10)
#define NO_FRAME_UNIT	(1)
#define BRIDGE_SND_WAIT_TIME	(50)
#define BRIDGE_SND_WAITRDY_TIME		(500)

#define COMIP_BRIDGE_DEBUG	1
#ifdef COMIP_BRIDGE_DEBUG
#define BRIDGE_PRT(format, args...)	printk(KERN_DEBUG "bridge: " format, ## args)
#define BRIDGE_INFO(format, args...)	printk(KERN_DEBUG "bridge: " format, ## args)
#define BRIDGE_ERR(format, args...)	printk(KERN_ERR "bridge: " format, ## args)
#else
#define BRIDGE_PRT(x...)  do{} while(0)
#define BRIDGE_INFO(x...)  do{} while(0)
#define BRIDGE_ERR(x...)  do{} while(0)
#endif

static inline int check_para(struct bridge_drvdata *drvdata)
{
	int status = 0;
	if (!drvdata) {
		BRIDGE_ERR("drvdata NULL!\n");
		status = -EFAULT;
	}

	if (!drvdata->is_open) {
		BRIDGE_ERR("%s:not open!\n",drvdata->name);
		status = -ENXIO;
	}
	return status;
}

extern int register_comip_bridge(struct modem_bridge_info *bridge_info,struct device *parent);
extern  void deregister_comip_bridge(struct modem_bridge_info *bridge_info);
extern int save_rxbuf_comip_bridge(struct modem_bridge_info *bridge_info);
extern int restore_rxbuf_comip_bridge(struct modem_bridge_info *bridge_info);
extern const struct comip_bridge_operations *get_bridge_ops(void);
extern int bridge_device_init(struct bridge_drvdata *drvdata, struct device *parent);
extern int bridge_device_uninit(struct bridge_drvdata *drvdata);
extern int bridge_net_device_init(struct bridge_drvdata *drvdata, struct device *parent);
extern int bridge_net_device_uninit(struct bridge_drvdata *drvdata);
extern struct bridge_drvdata * get_bridge_net_drvdata(struct net_device	*dev);
extern u64 get_clock(void);
#endif

