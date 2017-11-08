/*
* ST-Ericsson ETF driver
*
* Copyright (c) ST-Ericsson SA 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

/**********************************************************************
* sh_ta.c
*
* This module interfaces with the Linux Kernel MMC/SDIO stack.
*
***********************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "dev_ioctl.h"
#include "hi_api.h"
#include "st90tds_if.h"
#include "driver.h"
#include "debug.h"

#define SUCCESS 0

#define NTSTATUS int
#define OUT_SIZE 200

Dev_t *device;
uint32 g_fwsze;
uint32 g_msgsze;
uint32 started;

NTSTATUS STLC_ReadAdapter(Dev_t *device, HI_MSG_HDR *msg, uint32 out_size);
NTSTATUS STLC_WriteAdapter(Dev_t *device, HI_MSG_HDR *msg, uint32 size);
NTSTATUS STLC_Request(Dev_t *device, st90tds_msg_t *msg,
	uint32 inp_size, uint32 out_size);
int STLC_InitializeAdapter_ChipDetect(void);

static int device_open(struct inode *inode, struct file *file)
{
	DEBUG(DBG_FUNC, "\thit %s\n", __func__);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
	DEBUG(DBG_FUNC, "\thit %s: Reboot to restart tests.\n", __func__);
	return 0;
}

static ssize_t device_read(struct file *file, char *buffer,
		size_t length, loff_t *offset) {
	DEBUG(DBG_FUNC, "\thit %s\n", __func__);
	return 0;
}

static ssize_t device_write(struct file *file, const char *buffer,
			size_t length, loff_t *offset) {
	DEBUG(DBG_FUNC, "\thit %s\n", __func__);
	return 0;
}
st90tds_msg_t *msgbuf = NULL;

#ifdef HAVE_UNLOCKED_IOCTL
long device_ioctl(struct file *file, uint32 ioctl_num,
	unsigned long ioctl_param) {
#else
int device_ioctl(struct inode *inode, struct file *file,
	uint32 ioctl_num, unsigned long ioctl_param) {
#endif
	NTSTATUS status = STATUS_SUCCESS;
	char *msg;
	unsigned out_buf_size;
	int retry = 0;
	out_buf_size = OUT_SIZE;

	DEBUG(DBG_FUNC, "%s: hit ioctl:%d\n", __func__, ioctl_num);

	switch (ioctl_num) {
	case IOCTL_ST90TDS_ADAP_READ:
		msg = (char *)ioctl_param;
		/* Re-entrance is an error in the application level.*/
		status = STLC_ReadAdapter(device, (HI_MSG_HDR *)msg,
				out_buf_size);
		break;

	case IOCTL_ST90TDS_ADAP_WRITE:
		retry = 0;
		do {
			msg = kmalloc(g_msgsze, GFP_KERNEL);
			DEBUG(DBG_FUNC,"IOCTL adap write:"
				"allocated buf of size %d, @ %p\n",
				g_msgsze, msg);
		} while (msg == NULL && retry++ < 3);
		if ((retry >= 3) && (msg == NULL)) {
			status = -1;
			break;
		}

		if (copy_from_user(msg, (char *)ioctl_param, g_msgsze))
			DEBUG(DBG_ERROR, "%s: copy_from_user failed\n",
				__func__);
		status = STLC_WriteAdapter(device,
				(HI_MSG_HDR *)msg, g_msgsze);
		DEBUG(DBG_FUNC, "freeing msg @%p\n", msg);
		kfree(msg);
		break;

	case IOCTL_ST90TDS_REQ_CNF_SIZE:
		g_fwsze = ioctl_param;
		DEBUG(DBG_FUNC, "IOCTL size of request = %d\n",
			g_fwsze);
		break;

	case IOCTL_ST90TDS_WRITE_CNF_SIZE:
		g_msgsze = ioctl_param;
		DEBUG(DBG_FUNC, "IOCTL size of message = %d\n",
			g_msgsze);
		break;

	case IOCTL_ST90TDS_REQ_CNF:
		printk("g_fwsze  =%d, ioctl_param = %lx \n", g_fwsze, ioctl_param);
		retry = 0;
		do {
			msg = kmalloc(g_fwsze, GFP_KERNEL);
			printk("IOCTL request: allocated"
				"buf of size %d, @ %p\n",
				g_fwsze, msg);
		} while (msg == NULL && retry++ < 3);
		if ((retry >= 3) && (msg == NULL)) {
			status = -1;
			break;
		}

		if (copy_from_user(msg, (void *)ioctl_param, g_fwsze))
			DEBUG(DBG_ERROR, "%s: copy_from_user failed\n",
				__func__);

		/*To keep track of userspace buf address for reply */
		msgbuf = (st90tds_msg_t *)ioctl_param;
		status = STLC_Request(device, (st90tds_msg_t *)msg,
				g_fwsze, out_buf_size);
		DEBUG(DBG_FUNC, "freeing msg @%p\n", msg);
		started = 1; /*unused*/
		kfree(msg);
		break;

	case IOCTL_ST90TDS_CHIP_VERSION:
		/* check if chip version has been detected or not */
		if (chip_version_global_int == FALSE) {
			status = STLC_InitializeAdapter_ChipDetect();
			if (status != 0) {
				DEBUG(DBG_ERROR,
					"STLC_InitializeAdapter_ChipDetect"
					" returned error\n");
				break;
			}

			DEBUG(DBG_MESSAGE, "%s: chip version: %d\n", __func__,
				chip_version_global_int);
		}

		msgbuf = (st90tds_msg_t *)ioctl_param;

		DEBUG(DBG_MESSAGE, "%s: chip version: %d",
				__func__, chip_version_global_int);
		status = copy_to_user((void *)ioctl_param,
				&chip_version_global_int,
				sizeof(chip_version_global_int));
                if (status) {
                        DEBUG(DBG_ERROR, "copy_to_user fails: %d\n", status);
                }
		else {
			DEBUG(DBG_MESSAGE, "copy_to_user done copying"
				" chip_version_global_int=%d\n",
				chip_version_global_int);
			status = STATUS_SUCCESS;
		}

		break;

	default:
		DEBUG(DBG_ERROR, "%s:ERROR:Wrong Ioctl(%d) sent from host!\n",
				__func__, ioctl_num);
	}
	return status;
}

struct file_operations Fops = {
	.read = device_read,
	.write = device_write,
#ifdef HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl = device_ioctl,
#else
	.ioctl = device_ioctl,
#endif
	.open = device_open,
	.release = device_release,
};

int drv_etf_ioctl_enter(void)
{
	int ret_val;

	if (debug_level > 15)
		debug_level = 15;
	else if (debug_level < 8)
		debug_level += DBG_ALWAYS;

	ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);
	if (ret_val < 0) {
		DEBUG(DBG_ERROR, "register chrdev failed with %d\n", ret_val);
		return ret_val;
	}
	DEBUG(DBG_ALWAYS, "The major device number is %d\n", MAJOR_NUM);
	DEBUG(DBG_ALWAYS, "Create device file by command:\n");
	DEBUG(DBG_ALWAYS, "mknod %s c %d 0 \n", DEVICE_NAME, MAJOR_NUM);

	return 0;
}

void drv_etf_ioctl_release(void)
{
	st90tds_msg_t *msg;
	msg = kmalloc(sizeof(st90tds_msg_t), GFP_KERNEL);
	msg->msg_id = ST90TDS_EXIT_ADAPTER;
	STLC_Request(device, msg, 4, 200);
	DEBUG(DBG_ALWAYS, "Exiting character driver\n");
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}
