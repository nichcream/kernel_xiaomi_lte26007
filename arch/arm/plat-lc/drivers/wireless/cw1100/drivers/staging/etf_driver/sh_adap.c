/*
* ST-Ericsson ETF driver
*
* Copyright (c) ST-Ericsson SA 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
/****************************************************************************
* sh_adap.c
*
* This module interfaces with the Linux Kernel MMC/SDIO stack.
*
****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/mmc/sdio_func.h>
#include <plat/ste-cw1200.h>

#include "driver.h"
#include "hi_api.h"
#include "ETF_Api.h"
#include "st90tds_if.h"
#include "cw1200_common.h"
#include "sbus_wrapper.h"
#include "debug.h"

#define STATUS_SUCCESS 0
#define ERROR -1
#define NTSTATUS int
#define DEFAULT_DPLL_INIT_VAL 0x0EC4F121

/*WBF - SPI Register Addresses */
#define ST90TDS_ADDR_ID_BASE 0x0000
#define ST90TDS_CONFIG_REG_ID 0x0000 /* 16/32 bits*/
#define ST90TDS_CONTROL_REG_ID 0x0001 /* 16/32 bits*/
#define ST90TDS_IN_OUT_QUEUE_REG_ID 0x0002 /* 16 bits, Q mode W/R*/
#define ST90TDS_AHB_DPORT_REG_ID 0x0003 /* 32 bits, AHB bus R/W*/
#define ST90TDS_SRAM_BASE_ADDR_REG_ID 0x0004 /* 16/32 bits*/
#define ST90TDS_SRAM_DPORT_REG_ID 0x0005 /* 32 bits, APB bus R/W*/
#define ST90TDS_TSET_GEN_R_W_REG_ID 0x0006 /* 32 bits, t_settle/general*/
#define ST90TDS_FRAME_OUT_REG_ID 0x0007 /* 16 bits, Q mode read, no length*/
#define ST90TDS_ADDR_ID_MAX ST90TDS_FRAME_OUT_REG_ID

#define SDIO_READ 1
#define SDIO_WRITE 2
#define BIT_16_REG 2 /*2 Bytes */
#define BIT_32_REG 4

#define DOWNLOAD_BLOCK_SIZE 1024

#define ETF_ADAPTER_IO_DRIVE_STRENGTH_2MA 0

struct CW1200_priv priv;
wait_queue_head_t detect_waitq;
int detect_waitq_cond_flag;

struct sk_buff *_buf_h_;
int _rxlen_;
uint32 _fwsize_;
int g_exit = TRUE;
int g_rmmod = FALSE;
volatile int g_enter = FALSE;
volatile int waiting_rx = 0;

int chip_version_global_int = 0;
uint32 DPLL_INIT_VAL = DEFAULT_DPLL_INIT_VAL;
uint32 hif_strength = ETF_ADAPTER_IO_DRIVE_STRENGTH_2MA;

CW1200_STATUS_E SBUS_SDIOReadWrite_QUEUE_Reg(struct CW1200_priv *priv,
		uint8 *rw_buf, uint16 length, int read_write);
ETF_STATUS_CODE ETF_CB_Destroy (LL_HANDLE LowerHandle);
ETF_STATUS_CODE ETF_CB_Stop (LL_HANDLE LowerHandle);
ETF_STATUS_CODE ETF_CB_Devstop(LL_HANDLE LowerHandle);
LL_HANDLE ETF_CB_Create(ETF_HANDLE ETFHandle, UL_HANDLE ulHandle);
CW1200_STATUS_E SBUS_Sleep_Device(struct CW1200_priv *priv);
ETF_STATUS_CODE SBUS_ChipDetect (LL_HANDLE LowerHandle);
ETF_STATUS_CODE ETF_CB_Start (LL_HANDLE LowerHandle,
		uint32_t FmLength, void *FirmwareImage,uint32_t BlLength, void *bootloader);

CW1200_STATUS_E SBUS_SDIO_RW_Reg(struct CW1200_priv *priv,
	uint32 addr, uint8 *rw_buf,
	uint16 length, int read_write);
CW1200_STATUS_E Parse_SDD_File(char *sddcontent, uint32 length);

extern void WDEV_CB_RX(struct sk_buff *skb, int rxstatus);
extern void WDEV_CB_TX_Queue_Available(void);
int Device_SleepStatus(struct CW1200_priv *priv, int *sleep_status);

struct stlc_firmware fw_ptr;
struct bootloader_1260 bt_ptr;

static NTSTATUS STLC_InitializeAdapter_DownloadFw_AHB (Dev_t *device,
	st90tds_start_t *start_msg, unsigned inp_size);
NTSTATUS STLC_Request(Dev_t *device, st90tds_msg_t *own_msg, uint32 inp_size,
	uint32 out_size);
NTSTATUS STLC_ReadAdapter(Dev_t *device, HI_MSG_HDR *msg, uint32 out_size);
NTSTATUS STLC_WriteAdapter(Dev_t *device, HI_MSG_HDR *msg, uint32 size);
void STLC_StopAdapter(Dev_t *device, st90tds_start_t *msg);

extern int ste_wifi_set_carddetect(int on); //05022013

/**
* Sh_adap_init - release inbound mailbox message service
* @func: pointer to sdio_func structure
*
* Initialises synchronisation variables and waitqueue.
*/
void Sh_adap_init(struct sdio_func *func)
{
	priv.func = func;
	func->num = 1;
	priv.pdev = &(func->dev);

	init_completion(&priv.comp);

	mutex_init(&priv.rx_list_lock);

	INIT_LIST_HEAD(&priv.rx_list);

	DEBUG(DBG_FUNC, "%s, func and func_num initialised\n", __func__);
	/*Initialise data structures*/
	priv.max_num_buffs_supp = 0;
	priv.out_seq_num = 0;
	priv.in_seq_num = 0;
	priv.num_unprocessed_buffs_in_device = 0;
	priv.max_size_supp = 0;
	priv.num_unprocessed_buffs_in_device = 0;
	priv.hw_type = 0;
	return;
}

int HifTests_SendCustomCommand(Dev_t *device,
	st90tds_custom_cmd_t *req_resp)
{
	DEBUG(DBG_FUNC, "%s: hit", __func__);
	return 0;
}

int HifTests_WriteAhaReg(Dev_t *device, void *req)
{
	DEBUG(DBG_FUNC, "%s: hit\n", __func__);
	return 0;
}

int HifTests_ReadAhaReg(Dev_t *device, void *req)
{
	DEBUG(DBG_FUNC, "%s: hit\n", __func__);
	return 0;
}

int pr_dmp_msg(char *buf, uint32 len)
{
	int i;
	DEBUG(DBG_MESSAGE, "dump starts:\n");
	for (i = 1; i <= len; i++) {
		DEBUG(DBG_MESSAGE, "0x%2x ", buf[i-1]);
		if ((i % 8) == 0)
			DEBUG(DBG_MESSAGE, "\n");
	}
	DEBUG(DBG_MESSAGE, "dump ends\n");
	return 0;
}

int STLC_InitializeAdapter_ChipDetect(void)
{
	int retval = ETF_STATUS_SUCCESS;
	void *p_ret = NULL;
	int32_t status = 0;

	DEBUG(DBG_FUNC, "Entering %s\n", __func__);

	/*sdio_register_driver (&sdio_driver);*/
	init_waitqueue_head(&detect_waitq);
	/* Free if used by anyone else.This is WLAN GPIO */
	DEBUG(DBG_MESSAGE, "about to hit wpd in %s\n", __func__);
	wpd = cw1200_get_platform_data();
	DEBUG(DBG_MESSAGE, "wpd mmc id:%s\n", wpd->mmc_id);
	DEBUG(DBG_MESSAGE, "wpd irq name:%s\n", wpd->irq->name);
	DEBUG(DBG_MESSAGE, "wpd irq start:%d\n", wpd->irq->start);
	DEBUG(DBG_MESSAGE, "wpd reset name:%s\n", wpd->reset->name);
	DEBUG(DBG_MESSAGE, "wpd reset start:%d\n",wpd->reset->start);

	gpio_free(wpd->reset->start);
	DEBUG(DBG_MESSAGE, "gpio free done\n");

	/* Request WLAN Enable GPIO */
	status = gpio_request(wpd->reset->start, wpd->reset->name);
	DEBUG(DBG_MESSAGE, "%s, gpio_request called, status=%d\n",
		__func__, status);

	if (status) {
		DEBUG(DBG_ERROR, "INIT : Unable to request GPIO\n");
		return -1;
	}
	gpio_direction_output(wpd->reset->start, 1);
	gpio_set_value(wpd->reset->start, 1);
	mdelay(10);
	DEBUG(DBG_MESSAGE, "GPIO settings over, calling SDIO card detect\n");
	ste_wifi_set_carddetect(1); //05022013
	cw1200_detect_card();
	DEBUG(DBG_MESSAGE, "Going to sleep for initialization\n");
	detect_waitq_cond_flag=0;
	/* sleep for initializing the card, up when probe rcvd*/
	status = wait_event_interruptible_timeout(detect_waitq,
			detect_waitq_cond_flag, 20*HZ);
	DEBUG(DBG_MESSAGE, "woken up detect waitq\n");

	if (status > 0) {
		DEBUG(DBG_MESSAGE, "%s: Calling ETF_CB_Create\n", __func__);
		p_ret = ETF_CB_Create(NULL, &priv);

		DEBUG(DBG_MESSAGE, "%s: Calling SBUS_ChipDetect\n", __func__);
		retval = SBUS_ChipDetect(&priv);
		if (retval != ETF_STATUS_SUCCESS)
			DEBUG(DBG_ERROR, "SBUS_ChipDetect returned error\n");

	}else {
		DEBUG(DBG_ERROR, "timed out wait_event_interruptible_timeout\n");
		retval = ETF_STATUS_FAILURE;
	} // end if (status>0)

	DEBUG(DBG_MESSAGE, "%s: stopping adapter\n", __func__);
	g_exit = TRUE;
	g_enter = TRUE;
	STLC_StopAdapter(NULL, NULL);
	g_enter = FALSE;

	DEBUG(DBG_FUNC, "%s:returning status:%d to caller\n",
			__func__, retval);
	return retval;
}

static NTSTATUS STLC_InitializeAdapter_DownloadFw_AHB(Dev_t *device,
	st90tds_start_t *start_msg, unsigned inp_size)
{
	int retval = 0;
	void *p_ret = NULL;

	/*sdio_register_driver (&sdio_driver);*/
	int32_t status = 0;
	init_waitqueue_head(&detect_waitq);
	/* Free if used by anyone else.This is WLAN GPIO */
	DEBUG(DBG_MESSAGE, "about to hit wpd in %s\n", __func__);
	wpd = cw1200_get_platform_data();
	DEBUG(DBG_SBUS, "wpd mmc id:%s \n", wpd->mmc_id);
	DEBUG(DBG_SBUS, "wpd irq name:%s \n", wpd->irq->name);
	DEBUG(DBG_SBUS, "wpd irq start:%d \n", wpd->irq->start);
	DEBUG(DBG_SBUS, "wpd reset name:%s \n", wpd->reset->name);
	DEBUG(DBG_SBUS, "wpd reset start:%d \n",wpd->reset->start);

	gpio_free(wpd->reset->start);
	DEBUG(DBG_MESSAGE, "gpio free done\n");

	/* Request WLAN Enable GPIO */
	status = gpio_request(wpd->reset->start, wpd->reset->name);
	DEBUG(DBG_SBUS, "%s:%d, gpio_request called, status:%d\n",
		__func__, __LINE__, status);

	if (status) {
		DEBUG(DBG_ERROR,
			"INIT : Unable to request GPIO GPIO_WLAN_ENABLE \n");
		return -1;
	}
	gpio_direction_output(wpd->reset->start, 1);
	/* It is not stated in the datasheet, but at least some of devices
	 * have problems with reset if this stage is omited. */
	msleep(500);
	gpio_direction_output(wpd->reset->start, 0);
	/* A valid reset shall be obtained by maintaining WRESETN
	 * active (low) for at least two cycles of LP_CLK after VDDIO
	 * is stable within it operating range. */
	usleep_range(1000, 20000);
	gpio_set_value(wpd->reset->start, 1);
	/* The host should wait 32 ms after the WRESETN release
	 * for the on-chip LDO to stabilize */
	msleep(500);

	DEBUG(DBG_SBUS, "GPIO settings over, calling SDIO card detect\n");
	ste_wifi_set_carddetect(1); //05022013
	//cw1200_detect_card();
	DEBUG(DBG_ALWAYS, "Going to sleep for initialization\n");
	detect_waitq_cond_flag=0;
	/* sleep for initializing the card, up when probe rcvd*/
	status = wait_event_interruptible_timeout(detect_waitq,
			detect_waitq_cond_flag, 2*HZ);
	DEBUG(DBG_ALWAYS, "woken up detect waitq\n");

	if (status>0){
		DEBUG(DBG_FUNC, "%s: DownloadFw Silicon: size = %d\n",
			__func__, start_msg->image_size);
		if (start_msg->image_size == 0) {
			return -1;
		} else{
			fw_ptr.length = start_msg->image_size;
			fw_ptr.firmware = (char *)(start_msg+1);
			p_ret = ETF_CB_Create(NULL, &priv);
			retval = ETF_CB_Start(&priv, fw_ptr.length,
					fw_ptr.firmware,bt_ptr.length,
					bt_ptr.bootloader);
			if (retval!= ETF_STATUS_SUCCESS) {

				g_exit = TRUE;
				ETF_CB_Devstop((void *)&priv);

				if(g_enter)
					complete(&priv.comp);

				g_enter = FALSE;
			}
		}
	}else {
		DEBUG(DBG_ERROR,
			"timed out waiting for interrupt from device\n");
		g_exit = TRUE;
		ETF_CB_Devstop((void *)&priv);
		if(g_enter){
			complete(&priv.comp);
			g_enter = FALSE;
		}
		retval = ETF_STATUS_FAILURE;
	}
	DEBUG(DBG_FUNC, "%s:returning status:%d to caller\n",
			__func__, retval);
	return retval;
}

void STLC_StopAdapter(Dev_t *device, st90tds_start_t *msg)
{
	int status;
	DEBUG(DBG_FUNC, "\tstop adapter called \n");

	status = ETF_CB_Devstop(&priv);
	if (!status)
		DEBUG(DBG_FUNC, "STLC stop adapter success\n\n");

	/*test extra down_lock*/
	DEBUG(DBG_MESSAGE, "complete from stop adapter\n");
	complete(&priv.comp);

	/* wake_up(&rx_waitq); */
	/*to send response to blocking ioctl call to rcv thread */
	DEBUG(DBG_FUNC, "woke up rx_waitq in stop_adapter\n");
	msleep(100);/*Debug delay*/

	return;
}

NTSTATUS STLC_Request(Dev_t *device, st90tds_msg_t *own_msg,
	uint32 inp_size, uint32 out_size)
{
	uint32 msg_id;
	NTSTATUS status = STATUS_SUCCESS;
	st90tds_iomem_t *msg;/* use it to get msg_id*/
	st90tds_start_t *start_msg;
	st90tds_set_device_opt_t *optmsg;
	msg = (st90tds_iomem_t *)own_msg;
	msg_id = msg->msg_id;
	DEBUG(DBG_FUNC, "%s: hit msg_id = %d\n", __func__, msg_id);

	switch (msg_id) {
	case ST90TDS_START_ADAPTER:
		/*Download for CW1200 Silicon */
		g_exit = FALSE;
		DEBUG(DBG_MESSAGE, "\n\tcase ST90TDS_START_ADAPTER\n");
		status = STLC_InitializeAdapter_DownloadFw_AHB(device,
				(st90tds_start_t *) msg, inp_size);
			kfree(bt_ptr.bootloader);
		break;

	case ST90TDS_START_BOOT_LOADER:
		start_msg = (st90tds_start_t *) msg;
		DEBUG(DBG_MESSAGE,"bootloader file size :%d \n",
					start_msg->image_size);
		bt_ptr.bootloader = kmalloc(start_msg->image_size,
					GFP_KERNEL);
		bt_ptr.length = start_msg->image_size;
		memcpy(bt_ptr.bootloader,(uint8 *)(start_msg+1),
					bt_ptr.length);
		break;

	case ST90TDS_SEND_SDD:
		DEBUG(DBG_MESSAGE, "\n\tcase ST90TDS_SEND_SDD\n");
		DEBUG(DBG_MESSAGE, "\nmsg size:%d\n",inp_size);
		status = Parse_SDD_File(((char *)msg)+8, inp_size);
		break;

	case ST90TDS_SET_DEVICE_OPTION:
		DEBUG(DBG_MESSAGE, "\n\tcase ST90TDS_SET_DEVICE_OPTION\n");
		optmsg = (st90tds_set_device_opt_t *)msg;
		/*sleep device*/
		if(optmsg->opt_id == ST90TDS_DEVICE_OPT_SLEEP_MODE){
			if(optmsg->opt.sleep_mode.sleep_enable == 1){
				DEBUG(DBG_MESSAGE, "Clearing WUP Bit...\n");
				status = SBUS_Sleep_Device(&priv);
			}
		}
		/*set hif_strength*/
		if(optmsg->opt_id == ST90TDS_DEVICE_OPT_HIF_DRIVE_STRENGTH)
			hif_strength =
				(uint32)optmsg->opt.hif_strength.hif_strength;
		break;

	case ST90TDS_STOP_ADAPTER:
		if(g_exit){
			DEBUG(DBG_MESSAGE, "\nfirmware already stopped\n");
			break;
		}
		g_exit = TRUE;
		DEBUG(DBG_MESSAGE, "\n\tcase ST90TDS_STOP_ADAPTER\n");
		STLC_StopAdapter(device, (st90tds_start_t *)msg);
		g_enter = FALSE;
		break;

	case ST90TDS_EXIT_ADAPTER:
		if(g_exit){
			DEBUG(DBG_MESSAGE, "\nfirmware already stopped\n");
			break;
		}
		g_rmmod = TRUE;
		g_exit = TRUE;
		DEBUG(DBG_MESSAGE, "\n\tcase ST90TDS_STOP_ADAPTER\n");
		STLC_StopAdapter(device, (st90tds_start_t *)msg);
		g_enter = FALSE;
		break;

	case ST90TDS_CONFIG_ADAPTER:

	case ST90TDS_CONFIG_SPI_FREQ:

	case ST90TDS_CONFIG_SDIO_FREQ:
		break;

	case ST90TDS_SBUS_READ:
		{
			int config_reg_val = 0xffffffff;
			int retval;
			retval = SBUS_SDIO_RW_Reg(&priv, msg->offset,
						(char *)&config_reg_val,
						msg->length, SDIO_READ);
			DEBUG(DBG_MESSAGE, "read val %0x\n", config_reg_val);
			break;
		}

	case ST90TDS_SBUS_WRITE:
		{
			int retval = 0;
			retval = SBUS_SDIO_RW_Reg(&priv, msg->offset,
					(char *)(msg + 1), msg->length,
					SDIO_WRITE);
			DEBUG(DBG_MESSAGE, "value written retval = %d\n",
					retval);
			break;
		}

        case ST90TDS_SLEEP_CNF:
		{
			int sleep_status;
			int retval = 0;
			extern st90tds_msg_t *msgbuf;
			status = Device_SleepStatus(&priv, &sleep_status);
			retval = copy_to_user(&(msgbuf->msg_id), &sleep_status,
					sizeof(st90tds_msg_t));
			if (retval) {
				DEBUG(DBG_ERROR, "copy_to_user fails: %d\n",
					retval);
			}
			break;
		}
	default:
		break;
	}

	DEBUG(DBG_FUNC, "%s:returning status:%d to caller\n",
			__func__, status);
	return status;
}

NTSTATUS STLC_ReadAdapter(Dev_t *device, HI_MSG_HDR *msg, uint32 out_size)
{
	char *_rxbuf_;
	int retval = 0;
	HI_MSG_HDR hi_msg;
	struct cw1200_rx_buf *rx_buf = NULL, *next = NULL;

	if (!g_enter) {
		/* sleep introduced to break context of receiver thread and
		* avoid busyloop here */
		msleep(1);

		return 0;/*warning fix*/
	}
	DEBUG(DBG_MESSAGE, "hit rx_list_lock\n");
	mutex_lock(&priv.rx_list_lock);
	DEBUG(DBG_MESSAGE, "locked mutex\n");
	if(list_empty(&priv.rx_list)) {
		mutex_unlock(&priv.rx_list_lock);
		DEBUG(DBG_MESSAGE, "Calling wait_for_completion_timeout()\n");
		if(wait_for_completion_timeout(&priv.comp, 100*HZ)==0){
			DEBUG(DBG_MESSAGE, "%s:Timed out \n", __func__);
			goto end;
		}
		DEBUG(DBG_MESSAGE, "completion rcvd\n");
		mutex_lock(&priv.rx_list_lock);
	} else {
		DEBUG(DBG_MESSAGE, "Calling wait_for_completion_timeout()\n");
		if(wait_for_completion_timeout(&priv.comp, 100*HZ)==0){
			DEBUG(DBG_MESSAGE, "%s:Timed out \n", __func__);
			goto end;
		}
		DEBUG(DBG_MESSAGE, "completion rcvd\n");
	}

	if (g_exit) {
		DEBUG(DBG_SBUS, "%s:g_exit set found \n", __func__);
		goto end;
	}

	if (g_rmmod) {
		hi_msg.MsgLen = 4;
		hi_msg.MsgId = 0xfffe; /*stop adapter*/
		retval = copy_to_user(msg, &hi_msg, 4);
		if (retval)
			DEBUG(DBG_ERROR, "copy_to_user fails: %d\n", retval);

		DEBUG(DBG_MESSAGE, "%s:g_rmmod set found \n",
			__func__);
		goto end;
	}
	if (g_exit) {
		hi_msg.MsgLen = 4;
		hi_msg.MsgId = 0xffff; /*stop adapter*/
		retval = copy_to_user(msg, &hi_msg, 4);
		DEBUG(DBG_MESSAGE, "%s:g_exit set found \n", __func__);
		goto end;
	}

	list_for_each_entry_safe(rx_buf, next, &priv.rx_list, list) {
		DEBUG(DBG_MESSAGE, "process queue\n");
		if(NULL == rx_buf) {
			DEBUG(DBG_ERROR, "%s, rx_buf == NULL\n", __func__);
			mutex_unlock(&priv.rx_list_lock);
			goto end;
		}
		_rxlen_ = ((struct sk_buff *)rx_buf->skb)->len;
		_buf_h_ = ((struct sk_buff *)rx_buf->skb);
		list_del(&rx_buf->list);
		break;
	}
	mutex_unlock(&priv.rx_list_lock);

	DEBUG(DBG_MESSAGE, "going to free rxbuf:%p\n",rx_buf);
	kfree(rx_buf);

	if(NULL == _buf_h_) {
		DEBUG(DBG_ERROR, "%s, _buf_h_ == NULL\n", __func__);
		goto end;
	}

	_rxbuf_ = _buf_h_->data;
	DEBUG(DBG_MESSAGE, "%s: woken up Id%04x len:%04x\n",
		__func__, ((HI_MSG_HDR *)_rxbuf_)->MsgId,
		((HI_MSG_HDR *)_rxbuf_)->MsgLen);

	retval = copy_to_user(msg, _rxbuf_, _rxlen_);

	if (retval) {
		DEBUG(DBG_ERROR, "copy_to_user fails: %d\n", retval);
		pr_dmp_msg((char *)_rxbuf_, _rxlen_);
	}

	DEBUG(DBG_MESSAGE, "%s:skb %p and data %p of len %d freed\n",
		__func__, _buf_h_,_buf_h_->data, _buf_h_->len);

	dev_kfree_skb_any(_buf_h_);
	_buf_h_ = NULL;

end:
	return 0;
}

NTSTATUS STLC_WriteAdapter(Dev_t *device, HI_MSG_HDR *msg, uint32 size)
{
	int retval = 0, tmp = 0;
	int bufflen = 0;

	DEBUG(DBG_FUNC, "%s: hit\n", __func__);
	DEBUG(DBG_MESSAGE, "MsgId:%04x MsgLen:%d\n", msg->MsgId, msg->MsgLen);
	if (!(priv.num_unprocessed_buffs_in_device <
			priv.max_num_buffs_supp)) {
		DEBUG(DBG_ERROR, "%s: cant send message unprocessed"
		"buff exceed\n", __func__);
		goto err_out;
	}
	/*Update HI Message Seq Number */
	priv.num_unprocessed_buffs_in_device++;
	/*remove any seq no if from application*/
	DEBUG(DBG_MESSAGE, "msg seq no from app:%d\n",(msg->MsgId)>>13);
	msg->MsgId = (msg->MsgId) & (0x1fff);
	HI_PUT_MSG_SEQ(msg->MsgId, (priv.out_seq_num & HI_MSG_SEQ_RANGE));
	DEBUG(DBG_MESSAGE, "msg seq no from driver:%d\n",(msg->MsgId)>>13);
	priv.out_seq_num++;
#ifdef MOP_WORKAROUND
	tmp = msg->MsgLen;
	bufflen = tmp%DOWNLOAD_BLOCK_SIZE ?
	(tmp + DOWNLOAD_BLOCK_SIZE - tmp % DOWNLOAD_BLOCK_SIZE) : tmp;

	retval = SBUS_SDIOReadWrite_QUEUE_Reg (&priv,
			(uint8 *)msg, bufflen, SDIO_WRITE);
	if (retval) {
		DEBUG(DBG_ERROR, "SBUS_bh ():Device Data Queue Read/Write"
			"Error \n");
		kfree (msg);
		goto err_out;
	}
#else
	retval = SBUS_SDIOReadWrite_QUEUE_Reg (&priv, msg,
			msg->MsgLen, SDIO_WRITE);
	if (retval) {
		DEBUG(DBG_ERROR, "SBUS_bh ():Device Data Queue Read/Write"
			"Error \n");
		goto err_out;
	}
#endif
	mdelay(10);

err_out:
	if (retval)
		DEBUG(DBG_ERROR, "SBUS RW returns: %d", retval);
	return 0;
}

/*
* fn static CW1200_STATUS_E Parse_SDD_File(struct cw1200_firmware *frmwr)
* brief This function is called to Parse the SDD file
* to send information from SDD to ETF
*/
CW1200_STATUS_E Parse_SDD_File(char *sddcontent, uint32 length)
{
	uint8_t* SDDATA = (uint8_t*)sddcontent;
	CW1200_SDD* pElement;
	uint32_t ParsedLength = 0;
	uint32_t clock_present = FALSE;
	pElement = (CW1200_SDD *)SDDATA;
	pElement = (CW1200_SDD *)((uint8_t*)pElement +
				FIELD_OFFSET(CW1200_SDD, data) +
				pElement->length);
	ParsedLength += (FIELD_OFFSET(CW1200_SDD, data) + pElement->length);

	while(ParsedLength <= length) {
		switch(pElement->id) {
		case SDD_REFERENCE_FREQUENCY_ELT_ID:
			clock_present = TRUE;
			DEBUG(DBG_MESSAGE,
				"Clock Frequency Element found(hex value):"
				"%dKHz\n", *((uint16_t*)pElement->data));

			switch(*((uint16_t*)pElement->data)) {
			case 0x32C8:
				DPLL_INIT_VAL = 0x1D89D241;
				break;
			case 0x3E80:
				DPLL_INIT_VAL = 0x1E1;
				break;
			case 0x41A0:
				DPLL_INIT_VAL = 0x124931C1;
				break;
			case 0x4B00:
				DPLL_INIT_VAL = 0x191;
				break;
			case 0x5DC0:
				DPLL_INIT_VAL = 0x141;
				break;
			case 0x6590:
				DPLL_INIT_VAL = 0x0EC4F121;
				break;
			case 0x8340:
				DPLL_INIT_VAL = 0x92490E1;
				break;
			case 0x9600:
				DPLL_INIT_VAL = 0x100010C1;
				break;
			case 0x9C40:
				DPLL_INIT_VAL = 0xC1;
				break;
			case 0xBB80:
				DPLL_INIT_VAL = 0xA1;
				break;
			case 0xCB20:
				DPLL_INIT_VAL = 0x7627091;
				break;
			default:
				DEBUG(DBG_ERROR, "Unknown Reference clock"
					"frequency found. Going to stop"
					"Adapter\n");
				break;
			}

			break;
		default:
			break;
		}
		pElement = (CW1200_SDD *)((uint8_t*)pElement +
				FIELD_OFFSET(CW1200_SDD, data) +
				pElement->length);
		ParsedLength += (FIELD_OFFSET(CW1200_SDD, data) +
				pElement->length);
	}
	if(FALSE == clock_present) {
		DEBUG(DBG_ERROR, "Reference clock frequency not found."
				"Setting default value:26MHz.\n");
		DPLL_INIT_VAL = 0x0EC4F121;
	}
	return SUCCESS;
}
