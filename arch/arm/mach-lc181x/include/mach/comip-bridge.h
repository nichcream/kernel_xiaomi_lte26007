/*
 * LC1810 Bridge driver
 *
 * Copyright (C) 2011 Leadcore Corporation
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
/* circular buffer */
struct gs_buf {
	unsigned		buf_size;/*buf length*/
	char			*buf_buf;/*MEMORY start addr*/
	char			*buf_get;/*begin address for read*/
	char			*buf_put;/*begin address for write*/
};

struct GS_BUF_BRIDGE_HEAD {
	char			*ReceiverDataOffset;/*begin addr for read*/
	u32			ReceiverIrqNumber;
	u32			ReceiverDataCount;
	u32			ReceiverReserved;
	char			*SenderDataOffset;/*begin addr for write*/
	u32			SenderIrqNumber;
	u32			SenderDataCount;
	u32			SenderReserved;
};

struct BRIDGE_INFO
{
	void __iomem *membase;
	u32 memlength;
	u8 *pdata;/*data begin addr == buf_buf*/
	u32 length;/*data area length*/
	struct gs_buf  circular_buf;
};

typedef enum{
	start_dump_read_buf_cmd,
	start_dump_write_buf_cmd,
	start_write_to_read_cmd,
	start_read_to_write_cmd,
	stop_dump_read_buf_cmd,
	stop_dump_write_buf_cmd,
	stop_write_to_read_cmd,
	stop_read_to_write_cmd,
}Bridge_Debug_Cmd;

struct bridge_debug_info
{
	u64 t;
	u32 count;
};

struct comip_bridge_drvdata
{
	char               name[12];
	struct miscdevice bridge_dev;	/* misc device structure */
	struct mutex       read_sem;
	struct mutex       write_sem;
	spinlock_t op_lock;
	struct wake_lock wakelock;
	u32	rxirq;/*irq number for read*/
	u32	txirq;/*irq number for write*/
	u8	is_open;
	u32	debug_flg;/*ref Bridge_Debug_Cmd*/
	unsigned int __iomem *debug_info;
	struct file *write_file;/*file keep wirte data.*/
	struct file *read_file;/*file keep read data*/
	wait_queue_head_t  rxwait_queue;
	struct bridge_debug_info rx_int_dinfo;
	struct bridge_debug_info rx_dinfo;
	struct bridge_debug_info tx_dinfo;
	struct BRIDGE_INFO rxinfo;
	struct BRIDGE_INFO txinfo;
};

typedef enum BRIDGE_IOCTL_CMD{
	GET_BRIDGE_TX_LEN,
	GET_BRIDGE_RX_LEN,
}Bridge_Ioctl_Cmd;

extern struct miscdevice * register_comip_bridge(Modem_Bridge_Info *bridge_info,struct device *parent,struct file_operations *ops);
extern  void deregister_comip_bridge(Modem_Bridge_Info *bridge_info);

 #endif

