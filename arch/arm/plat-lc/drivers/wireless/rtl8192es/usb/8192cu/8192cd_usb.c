/*
 *  RX handle routines
 *
 *  $Id: 8192cd_usb.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_USB_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "./8192cd.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"


#define USB_HIGH_SPEED_BULK_SIZE	512
#define USB_FULL_SPEED_BULK_SIZE	64


#if defined(CONFIG_VENDOR_REQ_RETRY) && defined(CONFIG_USB_VENDOR_REQ_MUTEX)
//vendor req retry should be in the situation when each vendor req is atomically submitted from others
#define MAX_USBCTRL_VENDORREQ_TIMES 10
#else
#define MAX_USBCTRL_VENDORREQ_TIMES 2 // 1
#endif


//#ifdef USB_LOCK_ENABLE
#define MAX_RETRY_COUNT1 500
#define MAX_RETRY_COUNT2 2000
#define USB_RETRY_DELAY 1 // ms
static void new_usb_api_blocking_completion(struct urb *urb)
{
	int *done=(int *)urb->context;
//	printk("%s %d *******************************\n",__FUNCTION__,__LINE__);
	*done = 1;
//	wmb(); //FIXME
}

int new_usb_control_msg(struct usb_device *dev, unsigned int pipe, __u8 request, __u8 requesttype,
			 __u16 value, __u16 index, void *data, __u16 size, int timeout)
{
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	volatile int done=0;
	unsigned long expire;
	int retry_cnt=0, retry_cnt2=0;	
	int status, retval, actual_length = 0;
	
	dr = kzalloc(sizeof(struct usb_ctrlrequest), GFP_ATOMIC);
	if (!dr)
		return -ENOMEM;

	dr->bRequestType = requesttype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16(value);
	dr->wIndex = cpu_to_le16(index);
	dr->wLength = cpu_to_le16(size);

#if defined(__LINUX_2_6__)
	urb = usb_alloc_urb(0, GFP_ATOMIC);
#else
	urb = usb_alloc_urb(0);
#endif
	if (!urb)
	{
		kfree(dr);
		return -ENOMEM;
	}

	usb_fill_control_urb(urb, dev, pipe, (unsigned char *)dr, data,
			     size, new_usb_api_blocking_completion, (void *)&done);

	urb->actual_length = 0;

#if defined( __LINUX_2_6__)
	status = usb_submit_urb(urb, GFP_ATOMIC);
#else
	status = usb_submit_urb(urb);
#endif
	if (unlikely(status))
	{
		printk("[%s,%d] usb_submit_urb error, status=%d\n", __func__, __LINE__, status);
		goto err_submit_urb;
	}


	if(timeout)		//expire = msecs_to_jiffies(timeout) + jiffies;
		expire = msecs_to_jiffies(timeout) + jiffies;
	else
		expire = msecs_to_jiffies(200) + jiffies; //cathy
 	
retry:

	if(done==1) goto out;
	if(time_after(jiffies, expire)) goto out;
	

#ifndef USB_LOCK_ENABLE
	if(in_atomic())
#endif
	{
		retry_cnt++;
		mdelay(USB_RETRY_DELAY);
		if(retry_cnt>MAX_RETRY_COUNT1) goto out;
	}
#ifndef USB_LOCK_ENABLE
	else
	{
		schedule();	
	}
#endif

	goto retry;

out:

	if (retry_cnt > MAX_RETRY_COUNT1)
	{
		printk("Timeout! requesttype=%x request=%x value=%x index=%x size=%x retry_cnt=%d\n",requesttype,request,value,index,size,retry_cnt);
	}
	
	if (0 == done)
	{
		retval = usb_unlink_urb(urb);
		
		if (-EINPROGRESS == retval) {
			while (done==0 && retry_cnt2++ <MAX_RETRY_COUNT2) { // polling until completion
				if(in_atomic())
					mdelay(USB_RETRY_DELAY);
				else
					schedule();
			}
			status = (urb->status == -ECONNRESET ? -ETIMEDOUT : urb->status);
			if ((0 != status) && (-ETIMEDOUT != status)) {
				printk("[%s,%d] urb->status = %d, retval = %d\n", __func__, __LINE__, urb->status, retval);
			}
		} else {
			printk("[%s %d] usb_unlink_urb() fail!(retval=%d, urb->status = %d)\n",
				__FUNCTION__, __LINE__, retval, urb->status);
			status = retval;
		}
	}
	else
	{
		//printk("[done] index=%x data=%x, urb->status = %d \n",index,*(u8*)data, urb->status); 
		status = urb->status;
		actual_length = urb->actual_length;
	}

err_submit_urb:
	usb_free_urb(urb);
	kfree(dr);

	if (status < 0)
		return status;
	else
		return actual_length;
}
//#endif // USB_LOCK_ENABLE

static int usbctrl_vendorreq(struct rtl8192cd_priv *priv, u8 request, u16 value, u16 index, void *pdata, u16 len, u8 requesttype)
{
	struct priv_shared_info *pshare = priv->pshare;
	struct usb_device *udev = pshare->pusbdev;
	unsigned int pipe;
	int status = 0;
	u8 reqtype;
	u8 *pIo_buf;
	int vendorreq_times = 0;

#ifndef CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
	#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
	u8 *tmp_buf;
	#else // use stack memory
	u8 tmp_buf[MAX_USB_IO_CTL_SIZE];
	#endif
	u32 tmp_buflen=0;
#endif

	//DBG_871X("%s %s:%d\n",__FUNCTION__, current->comm, current->pid);

	if(pshare->bSurpriseRemoved){
		printk("[%s] padapter->bSurpriseRemoved!!!\n", __FUNCTION__);
		status = -EPERM; 
		goto exit;
	}	

	if(len>MAX_VENDOR_REQ_CMD_SIZE){
		printk( "[%s] Buffer len error ,vendor request failed\n", __FUNCTION__ );
		status = -EINVAL;
		goto exit;
	}	

	#ifdef CONFIG_USB_VENDOR_REQ_MUTEX
	_enter_critical_mutex(&pshare->usb_vendor_req_mutex, NULL);
	#endif
	
	// Acquire IO memory for vendorreq
#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
	pIo_buf = pshare->usb_vendor_req_buf;
#else
	#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
	tmp_buflen =  (u32)len + ALIGNMENT_UNIT;
	tmp_buf = rtw_malloc(tmp_buflen);
	#else // use stack memory
	tmp_buflen = MAX_USB_IO_CTL_SIZE;
	#endif

	// Added by Albert 2010/02/09
	// For mstar platform, mstar suggests the address for USB IO should be 16 bytes alignment.
	// Trying to fix it here.
	//pIo_buf = (tmp_buf==NULL)?NULL:tmp_buf + ALIGNMENT_UNIT -((SIZE_PTR)(tmp_buf) & 0x0f );	
	pIo_buf = (u8*)ALIGN((SIZE_PTR)tmp_buf, ALIGNMENT_UNIT);
#endif

	if ( pIo_buf== NULL) {
		printk( "[%s] pIo_buf == NULL \n", __FUNCTION__ );
		status = -ENOMEM;
		goto release_mutex;
	}
	
	while(++vendorreq_times <= MAX_USBCTRL_VENDORREQ_TIMES)
	{
		memset(pIo_buf, 0, len);
		
		if (requesttype == 0x01)
		{
			pipe = usb_rcvctrlpipe(udev, 0);//read_in
			reqtype =  REALTEK_USB_VENQT_READ;		
		} 
		else 
		{
			pipe = usb_sndctrlpipe(udev, 0);//write_out
			reqtype =  REALTEK_USB_VENQT_WRITE;		
			memcpy( pIo_buf, pdata, len);
		}		
		
		if (in_atomic())
			status = new_usb_control_msg(udev, pipe, request, reqtype, value, index, pIo_buf, len, RTW_USB_CONTROL_MSG_TIMEOUT);
		else
			status = rtw_usb_control_msg(udev, pipe, request, reqtype, value, index, pIo_buf, len, RTW_USB_CONTROL_MSG_TIMEOUT);
		
		if (status == len)   // Success this control transfer.
		{
			rtw_reset_continual_urb_error(priv);
			if ( requesttype == 0x01 )
			{   // For Control read transfer, we have to copy the read data from pIo_buf to pdata.
				memcpy( pdata, pIo_buf, len );
			}
		}
		else { // error cases
			printk("reg 0x%x, usb %s %u fail, status:%d value=0x%x, vendorreq_times:%d\n",
				value, (requesttype==0x01)?"read":"write", len, status, *(u32*)pdata, vendorreq_times);
			
			if (status < 0) {
				if( status == -ESHUTDOWN || status == -ENODEV )
				{			
					pshare->bSurpriseRemoved = TRUE;
				}
			}
			else // status != len && status >= 0
			{
				if(status > 0) {
					if ( requesttype == 0x01 )
					{   // For Control read transfer, we have to copy the read data from pIo_buf to pdata.
						memcpy( pdata, pIo_buf, len );
					}
				}
			}

			if(rtw_inc_and_chk_continual_urb_error(priv) == TRUE ){
				pshare->bSurpriseRemoved = TRUE;
				break;
			}
	
		}
	
		// firmware download is checksumed, don't retry
		if( (value >= FW_8192C_START_ADDRESS && value <= FW_8192C_END_ADDRESS) || status == len )
			break;
	
	}
	
	// release IO memory used by vendorreq
	#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
	rtw_mfree(tmp_buf, tmp_buflen);
	#endif

release_mutex:
	#ifdef CONFIG_USB_VENDOR_REQ_MUTEX
	_exit_critical_mutex(&pshare->usb_vendor_req_mutex, NULL);
	#endif
exit:
	return status;

}


u8 usb_read8(struct rtl8192cd_priv *priv, u32 addr)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u32 data=0;

	request = 0x05;
	requesttype = 0x01;//read_in
	index = 0;//n/a

	wvalue = (u16)(addr&0x0000ffff);
	len = 1;	
	
	usbctrl_vendorreq(priv, request, wvalue, index, &data, len, requesttype);

	return (u8)(le32_to_cpu(data)&0x0ff);
		
}

u16 usb_read16(struct rtl8192cd_priv *priv, u32 addr)
{       
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u32 data=0;

	request = 0x05;
	requesttype = 0x01;//read_in
	index = 0;//n/a

	wvalue = (u16)(addr&0x0000ffff);
	len = 2;	
	
	usbctrl_vendorreq(priv, request, wvalue, index, &data, len, requesttype);

	return (u16)(le32_to_cpu(data)&0xffff);
	
}

u32 usb_read32(struct rtl8192cd_priv *priv, u32 addr)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u32 data=0;

	request = 0x05;
	requesttype = 0x01;//read_in
	index = 0;//n/a

	wvalue = (u16)(addr&0x0000ffff);
	len = 4;	
	
	usbctrl_vendorreq(priv, request, wvalue, index, &data, len, requesttype);

	return le32_to_cpu(data);
	
}

int usb_write8(struct rtl8192cd_priv *priv, u32 addr, u8 val)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u32 data;
	int ret;

	request = 0x05;
	requesttype = 0x00;//write_out
	index = 0;//n/a

	wvalue = (u16)(addr&0x0000ffff);
	len = 1;
	
	data = val;
	data = cpu_to_le32(data&0x000000ff);
	
	ret = usbctrl_vendorreq(priv, request, wvalue, index, &data, len, requesttype);
	
	return ret;
	
}

int usb_write16(struct rtl8192cd_priv *priv, u32 addr, u16 val)
{	
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u32 data;
	int ret;

	request = 0x05;
	requesttype = 0x00;//write_out
	index = 0;//n/a

	wvalue = (u16)(addr&0x0000ffff);
	len = 2;
	
	data = val;
	data = cpu_to_le32(data&0x0000ffff);
	
	ret = usbctrl_vendorreq(priv, request, wvalue, index, &data, len, requesttype);
	
	return ret;
	
}

int usb_write32(struct rtl8192cd_priv *priv, u32 addr, u32 val)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u32 data;
	int ret;

	request = 0x05;
	requesttype = 0x00;//write_out
	index = 0;//n/a

	wvalue = (u16)(addr&0x0000ffff);
	len = 4;
	data = cpu_to_le32(val);	
	

	ret = usbctrl_vendorreq(priv, request, wvalue, index, &data, len, requesttype);
	
	return ret;
	
}

#define VENDOR_CMD_MAX_DATA_LEN	254
int usb_writeN(struct rtl8192cd_priv *priv, u32 addr, u32 length, u8 *pdata)
{
	u8 request;
	u8 requesttype;
	u16 wvalue;
	u16 index;
	u16 len;
	u32 buf[(VENDOR_CMD_MAX_DATA_LEN+3)/4]={0};
	int ret;

	request = 0x05;
	requesttype = 0x00;//write_out
	index = 0;//n/a

	wvalue = (u16)(addr&0x0000ffff);
	len = length;
	memcpy(buf, pdata, len );
	
	ret = usbctrl_vendorreq(priv, request, wvalue, index, buf, len, requesttype);
	
	return ret;
	
}

/*
 *
 *    HAL
 *
 */



//endpoint number 1,2,3,4,5
// bult in : 1
// bult out: 2 (High)
// bult out: 3 (Normal) for 3 out_ep, (Low) for 2 out_ep
// interrupt in: 4
// bult out: 5 (Low) for 3 out_ep


static void _OneOutEpMapping(HAL_INTF_DATA_TYPE *pHalData)
{
	//only endpoint number 0x02
	
	pHalData->Queue2EPNum[VO_QUEUE] = pHalData->RtBulkOutPipe[0];//VO
	pHalData->Queue2EPNum[VI_QUEUE] = pHalData->RtBulkOutPipe[0];//VI
	pHalData->Queue2EPNum[BE_QUEUE] = pHalData->RtBulkOutPipe[0];//BE
	pHalData->Queue2EPNum[BK_QUEUE] = pHalData->RtBulkOutPipe[0];//BK
	
	pHalData->Queue2EPNum[BEACON_QUEUE] = pHalData->RtBulkOutPipe[0];//BCN
	pHalData->Queue2EPNum[MGNT_QUEUE] = pHalData->RtBulkOutPipe[0];//MGT
	pHalData->Queue2EPNum[HIGH_QUEUE] = pHalData->RtBulkOutPipe[0];//HIGH
	pHalData->Queue2EPNum[TXCMD_QUEUE] = pHalData->RtBulkOutPipe[0];//TXCMD
}


static void _TwoOutEpMapping(BOOLEAN IsTestChip, HAL_INTF_DATA_TYPE *pHalData, BOOLEAN bWIFICfg)
{
	if(IsTestChip && bWIFICfg){ // test chip && wmm
		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  1, 	0, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};			
		//0:H(end_number=0x02), 1:L (end_number=0x03)
		
		pHalData->Queue2EPNum[VO_QUEUE] = pHalData->RtBulkOutPipe[0];//VO
		pHalData->Queue2EPNum[VI_QUEUE] = pHalData->RtBulkOutPipe[1];//VI
		pHalData->Queue2EPNum[BE_QUEUE] = pHalData->RtBulkOutPipe[0];//BE
		pHalData->Queue2EPNum[BK_QUEUE] = pHalData->RtBulkOutPipe[1];//BK
		
		pHalData->Queue2EPNum[BEACON_QUEUE] = pHalData->RtBulkOutPipe[0];//BCN
		pHalData->Queue2EPNum[MGNT_QUEUE] = pHalData->RtBulkOutPipe[0];//MGT
		pHalData->Queue2EPNum[HIGH_QUEUE] = pHalData->RtBulkOutPipe[0];//HIGH
		pHalData->Queue2EPNum[TXCMD_QUEUE] = pHalData->RtBulkOutPipe[0];//TXCMD
	}
	else if(!IsTestChip && bWIFICfg){ // Normal chip && wmm
		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  0, 	1, 	0, 	1, 	0, 	0, 	0, 	0, 		0	};
		//0:H(end_number=0x02), 1:L (end_number=0x03)
		
		pHalData->Queue2EPNum[VO_QUEUE] = pHalData->RtBulkOutPipe[1];//VO
		pHalData->Queue2EPNum[VI_QUEUE] = pHalData->RtBulkOutPipe[0];//VI
		pHalData->Queue2EPNum[BE_QUEUE] = pHalData->RtBulkOutPipe[1];//BE
		pHalData->Queue2EPNum[BK_QUEUE] = pHalData->RtBulkOutPipe[0];//BK
		
		pHalData->Queue2EPNum[BEACON_QUEUE] = pHalData->RtBulkOutPipe[0];//BCN
		pHalData->Queue2EPNum[MGNT_QUEUE] = pHalData->RtBulkOutPipe[0];//MGT
		pHalData->Queue2EPNum[HIGH_QUEUE] = pHalData->RtBulkOutPipe[0];//HIGH
		pHalData->Queue2EPNum[TXCMD_QUEUE] = pHalData->RtBulkOutPipe[0];//TXCMD
	}
	else{//typical setting
		
		//BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  1, 	1, 	0, 	0, 	0, 	0, 	0, 	0, 		0	};			
		//0:H(end_number=0x02), 1:L (end_number=0x03)
		
		pHalData->Queue2EPNum[VO_QUEUE] = pHalData->RtBulkOutPipe[0];//VO
		pHalData->Queue2EPNum[VI_QUEUE] = pHalData->RtBulkOutPipe[0];//VI
		pHalData->Queue2EPNum[BE_QUEUE] = pHalData->RtBulkOutPipe[1];//BE
		pHalData->Queue2EPNum[BK_QUEUE] = pHalData->RtBulkOutPipe[1];//BK
		
		pHalData->Queue2EPNum[BEACON_QUEUE] = pHalData->RtBulkOutPipe[0];//BCN
		pHalData->Queue2EPNum[MGNT_QUEUE] = pHalData->RtBulkOutPipe[0];//MGT
		pHalData->Queue2EPNum[HIGH_QUEUE] = pHalData->RtBulkOutPipe[0];//HIGH
		pHalData->Queue2EPNum[TXCMD_QUEUE] = pHalData->RtBulkOutPipe[0];//TXCMD
	}
	
}


static void _ThreeOutEpMapping(HAL_INTF_DATA_TYPE *pHalData, BOOLEAN bWIFICfg)
{
	if(bWIFICfg){//for WMM
		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  1, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};
		//0:H(end_number=0x02), 1:N(end_number=0x03), 2:L (end_number=0x05)
		
		pHalData->Queue2EPNum[VO_QUEUE] = pHalData->RtBulkOutPipe[0];//VO
		pHalData->Queue2EPNum[VI_QUEUE] = pHalData->RtBulkOutPipe[1];//VI
		pHalData->Queue2EPNum[BE_QUEUE] = pHalData->RtBulkOutPipe[2];//BE
		pHalData->Queue2EPNum[BK_QUEUE] = pHalData->RtBulkOutPipe[1];//BK
		
		pHalData->Queue2EPNum[BEACON_QUEUE] = pHalData->RtBulkOutPipe[0];//BCN
		pHalData->Queue2EPNum[MGNT_QUEUE] = pHalData->RtBulkOutPipe[0];//MGT
		pHalData->Queue2EPNum[HIGH_QUEUE] = pHalData->RtBulkOutPipe[0];//HIGH
		pHalData->Queue2EPNum[TXCMD_QUEUE] = pHalData->RtBulkOutPipe[0];//TXCMD
	}
	else{//typical setting
		
		//	BK, 	BE, 	VI, 	VO, 	BCN,	CMD,MGT,HIGH,HCCA 
		//{  2, 	2, 	1, 	0, 	0, 	0, 	0, 	0, 		0	};			
		//0:H(end_number=0x02), 1:N(end_number=0x03), 2:L (end_number=0x05)
		pHalData->Queue2EPNum[VO_QUEUE] = pHalData->RtBulkOutPipe[0];//VO
		pHalData->Queue2EPNum[VI_QUEUE] = pHalData->RtBulkOutPipe[1];//VI
		pHalData->Queue2EPNum[BE_QUEUE] = pHalData->RtBulkOutPipe[2];//BE
		pHalData->Queue2EPNum[BK_QUEUE] = pHalData->RtBulkOutPipe[2];//BK
		
		pHalData->Queue2EPNum[BEACON_QUEUE] = pHalData->RtBulkOutPipe[0];//BCN
		pHalData->Queue2EPNum[MGNT_QUEUE] = pHalData->RtBulkOutPipe[0];//MGT
		pHalData->Queue2EPNum[HIGH_QUEUE] = pHalData->RtBulkOutPipe[0];//HIGH
		pHalData->Queue2EPNum[TXCMD_QUEUE] = pHalData->RtBulkOutPipe[0];//TXCMD
	}

}

BOOLEAN _MappingOutEP(struct rtl8192cd_priv *priv, u8 NumOutPipe, BOOLEAN IsTestChip)
{
	HAL_INTF_DATA_TYPE* pHalData = GET_HAL_INTF_DATA(priv);
	BOOLEAN	 bWIFICfg = (priv->pmib->dot11OperationEntry.wifi_specific) ? TRUE : FALSE;
	
	BOOLEAN result = TRUE;

	switch(NumOutPipe)
	{
		case 2:
			_TwoOutEpMapping(IsTestChip, pHalData, bWIFICfg);
			break;
		case 3:
			// Test chip doesn't support three out EPs.
			if(IsTestChip){
				return FALSE;
			}			
			_ThreeOutEpMapping(pHalData, bWIFICfg);
			break;
		case 1:
			_OneOutEpMapping(pHalData);
			break;
		default:
			result = FALSE;
			break;
	}

	return result;
	
}

static void _ConfigTestChipOutEP(struct rtl8192cd_priv *priv, u8 NumOutPipe)
{
	u8 value8, txqsele;
	HAL_INTF_DATA_TYPE* pHalData = GET_HAL_INTF_DATA(priv);

	pHalData->OutEpQueueSel = 0;
	pHalData->OutEpNumber = 0;

	value8 = RTL_R8(REG_TEST_SIE_OPTIONAL);
	value8 = (value8 & USB_TEST_EP_MASK) >> USB_TEST_EP_SHIFT;
	
	switch(value8)
	{
		case 0:		// 2 bulk OUT, 1 bulk IN
		case 3:		
			pHalData->OutEpQueueSel = TX_SELE_HQ | TX_SELE_LQ;
			pHalData->OutEpNumber	= 2;
			//RT_TRACE(COMP_INIT,  DBG_LOUD, ("EP Config: 2 bulk OUT, 1 bulk IN\n"));
			break;
		case 1:		// 1 bulk IN/OUT => map all endpoint to Low queue
		case 2:		// 1 bulk IN, 1 bulk OUT => map all endpoint to High queue
			txqsele = RTL_R8(REG_TEST_USB_TXQS);
			if(txqsele & 0x0F){//map all endpoint to High queue
				pHalData->OutEpQueueSel  = TX_SELE_HQ;
			}
			else if(txqsele&0xF0){//map all endpoint to Low queue
				pHalData->OutEpQueueSel  =  TX_SELE_LQ;
			}
			pHalData->OutEpNumber	= 1;
			//RT_TRACE(COMP_INIT,  DBG_LOUD, ("%s\n", ((1 == value8) ? "1 bulk IN/OUT" : "1 bulk IN, 1 bulk OUT")));
			break;
		default:
			break;
	}

	// TODO: Error recovery for this case
	//RT_ASSERT((NumOutPipe == pHalData->OutEpNumber), ("Out EP number isn't match! %d(Descriptor) != %d (SIE reg)\n", (u4Byte)NumOutPipe, (u4Byte)pHalData->OutEpNumber));

}



static void _ConfigNormalChipOutEP(struct rtl8192cd_priv *priv, u8 NumOutPipe)
{
#ifdef CONFIG_RTL_92C_SUPPORT
	u8			value8;
#endif
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

#ifdef CONFIG_RTL_92C_SUPPORT
	pHalData->OutEpQueueSel = 0;
	pHalData->OutEpNumber	= 0;
		
	// Normal and High queue
	value8 = RTL_R8(REG_NORMAL_SIE_EP + 1);
	
	if(value8 & USB_NORMAL_SIE_EP_MASK){
		pHalData->OutEpQueueSel |= TX_SELE_HQ;
		pHalData->OutEpNumber++;
	}
	
	if((value8 >> USB_NORMAL_SIE_EP_SHIFT) & USB_NORMAL_SIE_EP_MASK){
		pHalData->OutEpQueueSel |= TX_SELE_NQ;
		pHalData->OutEpNumber++;
	}
	
	// Low queue
	value8 = RTL_R8(REG_NORMAL_SIE_EP + 2);
	if(value8 & USB_NORMAL_SIE_EP_MASK){
		pHalData->OutEpQueueSel |= TX_SELE_LQ;
		pHalData->OutEpNumber++;
	}

	// TODO: Error recovery for this case
	//RT_ASSERT((NumOutPipe == pHalData->OutEpNumber), ("Out EP number isn't match! %d(Descriptor) != %d (SIE reg)\n", (u4Byte)NumOutPipe, (u4Byte)pHalData->OutEpNumber));
	
#elif defined(CONFIG_RTL_88E_SUPPORT)
	switch(NumOutPipe){
		case 	3:
			pHalData->OutEpQueueSel=TX_SELE_HQ| TX_SELE_LQ|TX_SELE_NQ;
			pHalData->OutEpNumber=3;
			break;
		case 	2:
			pHalData->OutEpQueueSel=TX_SELE_HQ| TX_SELE_NQ;
			pHalData->OutEpNumber=2;
			break;
		case 	1:
			pHalData->OutEpQueueSel=TX_SELE_HQ;
			pHalData->OutEpNumber=1;
			break;
		default:
			break;

	}
#endif
}

static BOOLEAN HalUsbSetQueuePipeMapping8192CUsb(struct rtl8192cd_priv *priv, u8 NumInPipe, u8 NumOutPipe)
{
	HAL_INTF_DATA_TYPE	*pHalData = GET_HAL_INTF_DATA(priv);
	BOOLEAN			isNormalChip;

	//may be update UPHY Parameter == georgia 
	

	// ReadAdapterInfo8192C also call _ReadChipVersion too.
	// Since we need dynamic config EP mapping, so we call this function to get chip version.
	// We can remove _ReadChipVersion from ReadAdapterInfo8192C later.
	//pHalData->VersionID = rtl8192c_ReadChipVersion(pAdapter);

	isNormalChip = !IS_TEST_CHIP(priv);

	if(isNormalChip){
		_ConfigNormalChipOutEP(priv, NumOutPipe);
	}
	else{
		_ConfigTestChipOutEP(priv, NumOutPipe);
	}

	// Normal chip with one IN and one OUT doesn't have interrupt IN EP.
	if(isNormalChip && (1 == pHalData->OutEpNumber)){
		if(1 != NumInPipe){
			return FALSE;
		}
	}

	// All config other than above support one Bulk IN and one Interrupt IN.
	//if(2 != NumInPipe){
	//	return FALSE;
	//}

	// family_mark: Move _MappingOutEP() from xxx_init_one() to xxx_open() to let dynamically change OutEP mapping
	//result = _MappingOutEP(priv, NumOutPipe, !isNormalChip);
	
	return TRUE;

}

void rtl8192cu_interface_configure(struct rtl8192cd_priv *priv)
{
	struct priv_shared_info *pshare = priv->pshare;
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

	if (pshare->ishighspeed == TRUE)
	{
		pHalData->UsbBulkOutSize = USB_HIGH_SPEED_BULK_SIZE;//512 bytes
	}
	else
	{
		pHalData->UsbBulkOutSize = USB_FULL_SPEED_BULK_SIZE;//64 bytes
	}

	pHalData->interfaceIndex = pshare->InterfaceNumber;
	pHalData->RtBulkInPipe = pshare->ep_num[0];
	pHalData->RtBulkOutPipe[0] = pshare->ep_num[1];
	pHalData->RtBulkOutPipe[1] = pshare->ep_num[2];
	pHalData->RtIntInPipe = pshare->ep_num[3];
	pHalData->RtBulkOutPipe[2] = pshare->ep_num[4];

#ifdef CONFIG_USB_TX_AGGREGATION
	pHalData->UsbTxAggMode		= 1;
	pHalData->UsbTxAggDescNum	= 0x6;	// only 4 bits
#endif

#ifdef CONFIG_USB_RX_AGGREGATION
	pHalData->UsbRxAggMode			= USB_RX_AGG_DMA;// USB_RX_AGG_DMA;
	pHalData->UsbRxAggBlockCount	= 8; //unit : 512b
	pHalData->UsbRxAggBlockTimeout	= 0x6;
	pHalData->UsbRxAggPageCount		= 48; //uint :128 b //0x0A;	// 10 = MAX_RX_DMA_BUFFER_SIZE/2/pHalData->UsbBulkOutSize
	pHalData->UsbRxAggPageTimeout	= 0x4; //6, absolute time = 34ms/(2^6)
#endif

	HalUsbSetQueuePipeMapping8192CUsb(priv, pshare->RtNumInPipes, pshare->RtNumOutPipes);

}

u8 rtw_init_intf_priv(struct rtl8192cd_priv *priv)
{
	u8 rst = SUCCESS;
	struct priv_shared_info *pshare = priv->pshare;

	pshare->pHalData = kzalloc(sizeof(HAL_INTF_DATA_TYPE), GFP_ATOMIC);
	if (NULL == pshare->pHalData) return FAIL;
	
	#ifdef CONFIG_USB_VENDOR_REQ_MUTEX
	_rtw_mutex_init(&pshare->usb_vendor_req_mutex, NULL);
	#endif

#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
	pshare->usb_alloc_vendor_req_buf = kzalloc(MAX_USB_IO_CTL_SIZE, GFP_ATOMIC);

	if (NULL == pshare->usb_alloc_vendor_req_buf){
		printk("alloc usb_vendor_req_buf failed... /n");
		rst = FAIL;
		goto exit;
	}
	pshare->usb_vendor_req_buf  = 
		(u8 *)N_BYTE_ALIGMENT((SIZE_PTR)(pshare->usb_alloc_vendor_req_buf), ALIGNMENT_UNIT);
exit:
#endif // CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC

	if (FAIL == rst)
	{
		if (pshare->pHalData) {
			kfree(pshare->pHalData);
			pshare->pHalData = NULL;
		}
	}

	return rst;
	
}

u8 rtw_deinit_intf_priv(struct rtl8192cd_priv *priv)
{
	u8 rst = SUCCESS;
	struct priv_shared_info *pshare = priv->pshare;

#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
	if(pshare->usb_vendor_req_buf) {
		kfree(pshare->usb_alloc_vendor_req_buf);
	}
#endif // CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
	
#ifdef CONFIG_USB_VENDOR_REQ_MUTEX
	_rtw_mutex_free(&pshare->usb_vendor_req_mutex, NULL);
#endif
	
	if (pshare->pHalData) {
		kfree(pshare->pHalData);
		pshare->pHalData = NULL;
	}
	
	return rst;
}

u32 usb_dvobj_init(struct rtl8192cd_priv *priv)
{
	int	i;
	int	status = SUCCESS;
	struct priv_shared_info *pshare = priv->pshare;
	struct usb_device_descriptor		*pdev_desc;
	struct usb_host_config			*phost_conf;
	struct usb_config_descriptor		*pconf_desc;
	struct usb_host_interface		*phost_iface;
	struct usb_interface_descriptor	*piface_desc;
	struct usb_host_endpoint		*phost_endp;
	struct usb_endpoint_descriptor	*pendp_desc;
	struct usb_device				*pusbd = pshare->pusbdev;
	struct usb_interface			*pusb_interface = pshare->pusbintf;

	pshare->RtNumInPipes = 0;
	pshare->RtNumOutPipes = 0;

	pdev_desc = &pusbd->descriptor;

#if 0
	printk("\nusb_device_descriptor:\n");
	printk("bLength=%x\n", pdev_desc->bLength);
	printk("bDescriptorType=%x\n", pdev_desc->bDescriptorType);
	printk("bcdUSB=%x\n", pdev_desc->bcdUSB);
	printk("bDeviceClass=%x\n", pdev_desc->bDeviceClass);
	printk("bDeviceSubClass=%x\n", pdev_desc->bDeviceSubClass);
	printk("bDeviceProtocol=%x\n", pdev_desc->bDeviceProtocol);
	printk("bMaxPacketSize0=%x\n", pdev_desc->bMaxPacketSize0);
	printk("idVendor=%x\n", pdev_desc->idVendor);
	printk("idProduct=%x\n", pdev_desc->idProduct);
	printk("bcdDevice=%x\n", pdev_desc->bcdDevice);
	printk("iManufacturer=%x\n", pdev_desc->iManufacturer);
	printk("iProduct=%x\n", pdev_desc->iProduct);
	printk("iSerialNumber=%x\n", pdev_desc->iSerialNumber);
	printk("bNumConfigurations=%x\n", pdev_desc->bNumConfigurations);
#endif
	
	phost_conf = pusbd->actconfig;
	pconf_desc = &phost_conf->desc;

#if 0
	printk("\nusb_configuration_descriptor:\n");
	printk("bLength=%x\n", pconf_desc->bLength);
	printk("bDescriptorType=%x\n", pconf_desc->bDescriptorType);
	printk("wTotalLength=%x\n", pconf_desc->wTotalLength);
	printk("bNumInterfaces=%x\n", pconf_desc->bNumInterfaces);
	printk("bConfigurationValue=%x\n", pconf_desc->bConfigurationValue);
	printk("iConfiguration=%x\n", pconf_desc->iConfiguration);
	printk("bmAttributes=%x\n", pconf_desc->bmAttributes);
	printk("bMaxPower=%x\n", pconf_desc->bMaxPower);
#endif

	//printk("\n/****** num of altsetting = (%d) ******/\n", pusb_interface->num_altsetting);

	phost_iface = &pusb_interface->altsetting[0];
	piface_desc = &phost_iface->desc;

#if 0
	printk("\nusb_interface_descriptor:\n");
	printk("bLength=%x\n", piface_desc->bLength);
	printk("bDescriptorType=%x\n", piface_desc->bDescriptorType);
	printk("bInterfaceNumber=%x\n", piface_desc->bInterfaceNumber);
	printk("bAlternateSetting=%x\n", piface_desc->bAlternateSetting);
	printk("bNumEndpoints=%x\n", piface_desc->bNumEndpoints);
	printk("bInterfaceClass=%x\n", piface_desc->bInterfaceClass);
	printk("bInterfaceSubClass=%x\n", piface_desc->bInterfaceSubClass);
	printk("bInterfaceProtocol=%x\n", piface_desc->bInterfaceProtocol);
	printk("iInterface=%x\n", piface_desc->iInterface);
#endif

	pshare->NumInterfaces = pconf_desc->bNumInterfaces;
	pshare->InterfaceNumber = piface_desc->bInterfaceNumber;
	pshare->nr_endpoint = piface_desc->bNumEndpoints;

	//printk("\ndump usb_endpoint_descriptor:\n");

	for (i = 0; i < pshare->nr_endpoint; i++)
	{
		phost_endp = phost_iface->endpoint + i;
		if (phost_endp)
		{
			pendp_desc = &phost_endp->desc;

			printk("\nusb_endpoint_descriptor(%d):\n", i);
			printk("bLength=%x\n",pendp_desc->bLength);
			printk("bDescriptorType=%x\n",pendp_desc->bDescriptorType);
			printk("bEndpointAddress=%x\n",pendp_desc->bEndpointAddress);
			//printk("bmAttributes=%x\n",pendp_desc->bmAttributes);
			//printk("wMaxPacketSize=%x\n",pendp_desc->wMaxPacketSize);
			printk("wMaxPacketSize=%x\n",le16_to_cpu(pendp_desc->wMaxPacketSize));
			printk("bInterval=%x\n",pendp_desc->bInterval);
			//printk("bRefresh=%x\n",pendp_desc->bRefresh);
			//printk("bSynchAddress=%x\n",pendp_desc->bSynchAddress);

			if (usb_endpoint_is_bulk_in(pendp_desc))
			{
				printk("usb_endpoint_is_bulk_in = %x\n", usb_endpoint_num(pendp_desc));
				pshare->RtNumInPipes++;
			}
			else if (usb_endpoint_is_int_in(pendp_desc))
			{
				printk("usb_endpoint_is_int_in = %x, Interval = %x\n", usb_endpoint_num(pendp_desc), pendp_desc->bInterval);
				pshare->RtNumInPipes++;
			}
			else if (usb_endpoint_is_bulk_out(pendp_desc))
			{
				printk("usb_endpoint_is_bulk_out = %x\n", usb_endpoint_num(pendp_desc));
				pshare->RtNumOutPipes++;
			}
			pshare->ep_num[i] = usb_endpoint_num(pendp_desc);
		}
	}
	
	printk("nr_endpoint=%d, in_num=%d, out_num=%d\n\n", pshare->nr_endpoint, pshare->RtNumInPipes, pshare->RtNumOutPipes);

	if (pusbd->speed == USB_SPEED_HIGH)
	{
		pshare->ishighspeed = TRUE;
		printk("USB_SPEED_HIGH\n");
	}
	else
	{
		pshare->ishighspeed = FALSE;
		printk("NON USB_SPEED_HIGH\n");
	}

	//.2
	if((rtw_init_intf_priv(priv) )== FAIL)
	{
//		RT_TRACE(_module_os_intfs_c_,_drv_err_,("\n Can't INIT rtw_init_intf_priv\n"));
		status = FAIL;
	}
	
	rtw_reset_continual_urb_error(priv);
	
#ifdef CONFIG_RTL_88E_SUPPORT
	priv->pshare->version_id = VERSION_8188E;
#endif

	check_chipID_MIMO(priv);

	//.4 usb endpoint mapping
	rtl8192cu_interface_configure(priv);

	return status;
}

void usb_dvobj_deinit(struct rtl8192cd_priv *priv)
{
	rtw_deinit_intf_priv(priv);
}

void rtw_dev_unload(struct rtl8192cd_priv *priv)
{
	struct priv_shared_info *pshare;
	
	pshare = priv->pshare;
	pshare->bDriverStopped = TRUE;
	
	//cancel in irp
	rtl8192cu_inirp_deinit(priv);
	
	//cancel out irp
	usb_write_port_cancel(priv);
	
#ifdef SMART_REPEATER_MODE
	if (!pshare->switch_chan_rp)
#endif
	if (pshare->cmd_thread) {
		if (test_and_set_bit(WAKE_EVENT_CMD, &pshare->wake_event) == 0)
			wake_up_process(pshare->cmd_thread);
		printk("[%s] cmd_thread", __FUNCTION__);
		wait_for_completion(&pshare->cmd_thread_done);
		printk(" terminate\n");
		pshare->cmd_thread = NULL;
	}
	
	if (FALSE == pshare->bSurpriseRemoved) {
		rtl8192cd_stop_hw(priv);
		pshare->bSurpriseRemoved = TRUE;
	}
}

void _InitQueueReservedPage(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE	*pHalData	= GET_HAL_INTF_DATA(priv);
#ifdef CONFIG_RTL_92C_SUPPORT
	BOOLEAN			isNormalChip = !IS_TEST_CHIP(priv);
#endif
	
//	u32			outEPNum	= (u32)pHalData->OutEpNumber;
	u32			numHQ		= 0;
	u32			numLQ		= 0;
	u32			numNQ		= 0;
	u32			numPubQ;
	u32			value32;
	u8			value8;
	BOOLEAN		bWiFiConfig	= ((priv->pmib->dot11OperationEntry.wifi_specific) ? TRUE : FALSE);
	
#ifdef CONFIG_RTL_92C_SUPPORT
	//RT_ASSERT((outEPNum>=2), ("for WMM ,number of out-ep must more than or equal to 2!\n"));
	
	numPubQ = (isNormalChip)? ((bWiFiConfig)? WMM_NORMAL_PAGE_NUM_PUBQ : NORMAL_PAGE_NUM_PUBQ)
							: WMM_TEST_PAGE_NUM_PUBQ;		
	
	if(pHalData->OutEpQueueSel & TX_SELE_HQ){
		numHQ = (isNormalChip)? ((bWiFiConfig)? WMM_NORMAL_PAGE_NUM_HPQ : NORMAL_PAGE_NUM_HPQ)
							: WMM_TEST_PAGE_NUM_HPQ;
	}
	
	if(pHalData->OutEpQueueSel & TX_SELE_LQ){
		numLQ = (isNormalChip)? ((bWiFiConfig)? WMM_NORMAL_PAGE_NUM_LPQ : NORMAL_PAGE_NUM_LPQ)
							: WMM_TEST_PAGE_NUM_LPQ;
	}
	
	// NOTE: This step shall be proceed before writting REG_RQPN.
	if(isNormalChip){			
		if(pHalData->OutEpQueueSel & TX_SELE_NQ){
			numNQ = (bWiFiConfig)? WMM_NORMAL_PAGE_NUM_NPQ : NORMAL_PAGE_NUM_NPQ;
		}
		value8 = (u8)_NPQ(numNQ);
		RTL_W8(RQPN_NPQ, value8);
	}
	
	// TX DMA
	value32 = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;	
	RTL_W32(RQPN, value32);
	
#elif defined(CONFIG_RTL_88E_SUPPORT)
	if (bWiFiConfig)
	{
		if (pHalData->OutEpQueueSel & TX_SELE_HQ)
		{
			numHQ =  0x29;
		}

		if (pHalData->OutEpQueueSel & TX_SELE_LQ)
		{
			numLQ = 0x1C;
		}

		// NOTE: This step shall be proceed before writting REG_RQPN.
		if (pHalData->OutEpQueueSel & TX_SELE_NQ) {
			numNQ = 0x1C;
		}
		value8 = (u8)_NPQ(numNQ);
		RTL_W8(RQPN_NPQ, value8);

		numPubQ = 0xA8 - numHQ - numLQ - numNQ;

		// TX DMA
		value32 = _HPQ(numHQ) | _LPQ(numLQ) | _PUBQ(numPubQ) | LD_RQPN;
		RTL_W32(RQPN, value32);
	}
	else
	{
		RTL_W16(RQPN_NPQ, 0x0000);//Just follow MP Team,??? Georgia 03/28
		RTL_W16(RQPN_NPQ, 0x0d);
		RTL_W32(RQPN, 0x808E000d);//reserve 7 page for LPS
	}
#endif
}

static void _InitNormalChipRegPriority(struct rtl8192cd_priv *priv,
	u16 beQ, u16 bkQ, u16 viQ, u16 voQ, u16 mgtQ, u16 hiQ)
{
	u16 value16 = (RTL_R16(TRXDMA_CTRL) & 0x7);

	value16 |=	_TXDMA_BEQ_MAP(beQ) 	| _TXDMA_BKQ_MAP(bkQ) |
				_TXDMA_VIQ_MAP(viQ) 	| _TXDMA_VOQ_MAP(voQ) |
				_TXDMA_MGQ_MAP(mgtQ)| _TXDMA_HIQ_MAP(hiQ);
	
	RTL_W16(TRXDMA_CTRL, value16);
}

static void _InitNormalChipOneOutEpPriority(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	
	u16	value = 0;
	switch(pHalData->OutEpQueueSel)
	{
		case TX_SELE_HQ:
			value = QUEUE_HIGH;
			break;
		case TX_SELE_LQ:
			value = QUEUE_LOW;
			break;
		case TX_SELE_NQ:
			value = QUEUE_NORMAL;
			break;
		default:
			//RT_ASSERT(FALSE,("Shall not reach here!\n"));
			break;
	}
	
	_InitNormalChipRegPriority(priv, value, value, value, value, value, value);

}

static void _InitNormalChipTwoOutEpPriority(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u16	beQ,bkQ,viQ,voQ,mgtQ,hiQ;
	
	u16	valueHi = 0;
	u16	valueLow = 0;
	
	switch(pHalData->OutEpQueueSel)
	{
		case (TX_SELE_HQ | TX_SELE_LQ):
			valueHi = QUEUE_HIGH;
			valueLow = QUEUE_LOW;
			break;
		case (TX_SELE_NQ | TX_SELE_LQ):
			valueHi = QUEUE_NORMAL;
			valueLow = QUEUE_LOW;
			break;
		case (TX_SELE_HQ | TX_SELE_NQ):
			valueHi = QUEUE_HIGH;
			valueLow = QUEUE_NORMAL;
			break;
		default:
			//RT_ASSERT(FALSE,("Shall not reach here!\n"));
			break;
	}

	if(!priv->pmib->dot11OperationEntry.wifi_specific){
		beQ		= valueLow;
		bkQ		= valueLow;
		viQ		= valueHi;
		voQ		= valueHi;
		mgtQ	= valueHi; 
		hiQ		= valueHi;
	}
	else{//for WMM ,CONFIG_OUT_EP_WIFI_MODE
		beQ		= valueLow;
		bkQ		= valueHi;
		viQ		= valueHi;
		voQ		= valueLow;
		mgtQ	= valueHi;
		hiQ		= valueHi;
	}
	
	_InitNormalChipRegPriority(priv,beQ,bkQ,viQ,voQ,mgtQ,hiQ);

}

static void _InitNormalChipThreeOutEpPriority(struct rtl8192cd_priv *priv)
{
	u16			beQ,bkQ,viQ,voQ,mgtQ,hiQ;

	if(!priv->pmib->dot11OperationEntry.wifi_specific){// typical setting
		beQ		= QUEUE_LOW;
		bkQ 		= QUEUE_LOW;
		viQ 		= QUEUE_NORMAL;
		voQ 		= QUEUE_HIGH;
		mgtQ 	= QUEUE_HIGH;
		hiQ 		= QUEUE_HIGH;
	}
	else{// for WMM
		beQ		= QUEUE_LOW;
		bkQ 		= QUEUE_NORMAL;
		viQ 		= QUEUE_NORMAL;
		voQ 		= QUEUE_HIGH;
		mgtQ 	= QUEUE_HIGH;
		hiQ 		= QUEUE_HIGH;
	}
	
	_InitNormalChipRegPriority(priv,beQ,bkQ,viQ,voQ,mgtQ,hiQ);
}

static void _InitNormalChipQueuePriority(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

	switch(pHalData->OutEpNumber)
	{
		case 1:
			_InitNormalChipOneOutEpPriority(priv);
			break;
		case 2:
			_InitNormalChipTwoOutEpPriority(priv);
			break;
		case 3:
			_InitNormalChipThreeOutEpPriority(priv);
			break;
		default:
			//RT_ASSERT(FALSE,("Shall not reach here!\n"));
			break;
	}
}

static void _InitTestChipQueuePriority(struct rtl8192cd_priv *priv) 
{
	u8	hq_sele ;
	HAL_INTF_DATA_TYPE* pHalData = GET_HAL_INTF_DATA(priv);
	
	switch(pHalData->OutEpNumber)
	{
		case 2:	// (TX_SELE_HQ|TX_SELE_LQ)
			if(!priv->pmib->dot11OperationEntry.wifi_specific)//typical setting
				hq_sele =  HPQ_SEL_VOQ | HPQ_SEL_VIQ | HPQ_SEL_MGQ | HPQ_SEL_HIQ ;
			else	//for WMM
				hq_sele = HPQ_SEL_VOQ | HPQ_SEL_BEQ | HPQ_SEL_MGQ | HPQ_SEL_HIQ ;
			break;
		case 1:
			if(TX_SELE_LQ == pHalData->OutEpQueueSel){//map all endpoint to Low queue
				 hq_sele = 0;
			}
			else if(TX_SELE_HQ == pHalData->OutEpQueueSel){//map all endpoint to High queue
				hq_sele =  HPQ_SEL_VOQ | HPQ_SEL_VIQ | HPQ_SEL_BEQ | HPQ_SEL_BKQ | HPQ_SEL_MGQ | HPQ_SEL_HIQ ;
			}		
			break;
		default:
			//RT_ASSERT(FALSE,("Shall not reach here!\n"));
			break;
	}
	RTL_W8((TRXDMA_CTRL+1), hq_sele);
}

void _InitQueuePriority(struct rtl8192cd_priv *priv)
{
	if(IS_TEST_CHIP(priv)){
		_InitTestChipQueuePriority(priv);
	}
	else{
		_InitNormalChipQueuePriority(priv);
	}
}

static void usb_AggSettingTxUpdate(struct rtl8192cd_priv *priv)
{
#ifdef CONFIG_USB_TX_AGGREGATION
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u32			value32;

// family_mark: need to confirm
//	if(priv->pmib->dot11OperationEntry.wifi_specific)
//		pHalData->UsbTxAggMode = FALSE;

	if(pHalData->UsbTxAggMode){
		value32 = RTL_R32(TDECTRL);
		value32 = value32 & ~(BLK_DESC_NUM_Mask << BLK_DESC_NUM_SHIFT);
		value32 |= ((pHalData->UsbTxAggDescNum & BLK_DESC_NUM_Mask) << BLK_DESC_NUM_SHIFT);
		
		RTL_W32(TDECTRL, value32);
	}
	
#endif
}

static void usb_AggSettingRxUpdate(struct rtl8192cd_priv *priv)
{
#ifdef CONFIG_USB_RX_AGGREGATION
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u8			valueDMA;
	u8			valueUSB;

	valueDMA = RTL_R8(TRXDMA_CTRL);
	valueUSB = RTL_R8(REG_USB_SPECIAL_OPTION);

	switch(pHalData->UsbRxAggMode)
	{
		case USB_RX_AGG_DMA:
			valueDMA |= RXDMA_AGG_EN;
			valueUSB &= ~USB_AGG_EN;
			break;
		case USB_RX_AGG_USB:
			valueDMA &= ~RXDMA_AGG_EN;
			valueUSB |= USB_AGG_EN;
			break;
		case USB_RX_AGG_MIX:
			valueDMA |= RXDMA_AGG_EN;
			valueUSB |= USB_AGG_EN;
			break;
		case USB_RX_AGG_DISABLE:
		default:
			valueDMA &= ~RXDMA_AGG_EN;
			valueUSB &= ~USB_AGG_EN;
			break;
	}

	RTL_W8(TRXDMA_CTRL, valueDMA);
	RTL_W8(REG_USB_SPECIAL_OPTION, valueUSB);

	switch(pHalData->UsbRxAggMode)
	{
		case USB_RX_AGG_DMA:
			RTL_W8(RXDMA_AGG_PG_TH, pHalData->UsbRxAggPageCount);
#ifdef CONFIG_RTL_92C_SUPPORT
			RTL_W8(REG_USB_DMA_AGG_TO, pHalData->UsbRxAggPageTimeout);
#elif defined(CONFIG_RTL_88E_SUPPORT)
			RTL_W8(RXDMA_AGG_PG_TH+1, pHalData->UsbRxAggPageTimeout);
#endif
			break;
		case USB_RX_AGG_USB:
			RTL_W8(REG_USB_AGG_TH, pHalData->UsbRxAggBlockCount);
			RTL_W8(REG_USB_AGG_TO, pHalData->UsbRxAggBlockTimeout);
			break;
		case USB_RX_AGG_MIX:
			RTL_W8(RXDMA_AGG_PG_TH, pHalData->UsbRxAggPageCount);
#ifdef CONFIG_RTL_92C_SUPPORT
			RTL_W8(REG_USB_DMA_AGG_TO, pHalData->UsbRxAggPageTimeout);
#elif defined(CONFIG_RTL_88E_SUPPORT)
			RTL_W8(RXDMA_AGG_PG_TH+1, pHalData->UsbRxAggPageTimeout);
#endif
			RTL_W8(REG_USB_AGG_TH, pHalData->UsbRxAggBlockCount);
			RTL_W8(REG_USB_AGG_TO, pHalData->UsbRxAggBlockTimeout);
			break;
		case USB_RX_AGG_DISABLE:
		default:
			// TODO: 
			break;
	}
#endif
}

void InitUsbAggregationSetting(struct rtl8192cd_priv *priv)
{
	// Tx aggregation setting
	usb_AggSettingTxUpdate(priv);

	// Rx aggregation setting
	usb_AggSettingRxUpdate(priv);
}

#if defined(CONFIG_RTL_92C_SUPPORT) || (!defined(CONFIG_SUPPORT_USB_INT) || !defined(CONFIG_INTERRUPT_BASED_TXBCN))
static void pre_rtl8192cu_beacon_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct priv_shared_info *pshare = priv->pshare;
	
	if ((pshare->bDriverStopped) || (pshare->bSurpriseRemoved)) {
		printk("[%s] bDriverStopped(%d) OR bSurpriseRemoved(%d)\n",
			__FUNCTION__, pshare->bDriverStopped, pshare->bSurpriseRemoved);
		return;
	}
	
	rtw_enqueue_timer_event(priv, &pshare->beacon_timer_event, ENQUEUE_TO_HEAD);
}

#define BEACON_EARLY_TIME		20	// unit:TU
static void rtl8192cu_beacon_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct priv_shared_info *pshare = priv->pshare;
	u32 beacon_interval, timestamp;
	u32 cur_tick, time_offset;
#ifdef MBSSID
	u32 inter_beacon_space;
	int nr_vap, idx, bcn_idx;
#endif
	u8 val8, reg_val8, late=0;
	
	beacon_interval = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * NET80211_TU_TO_US;
	if (0 == beacon_interval) {
		printk("[%s] ERROR: beacon interval = 0\n", __FUNCTION__);
		return;
	}
	
	timestamp = RTL_R32(TSFTR);
	cur_tick = timestamp % beacon_interval;
	
#ifdef MBSSID
	nr_vap = pshare->nr_vap_bcn;
	if (nr_vap) {
		inter_beacon_space = pshare->inter_bcn_space;//beacon_interval / (nr_vap+1);
		idx = cur_tick / inter_beacon_space;
		if (idx < nr_vap)	// if (idx < (nr_vap+1))
			bcn_idx = idx +1;	// bcn_idx = (idx + 1) % (nr_vap+1);
		else
			bcn_idx = 0;
		priv = pshare->bcn_priv[bcn_idx];
		if (((idx+2 == nr_vap+1) && (idx < nr_vap+1)) || (0 == bcn_idx)) {
			time_offset = beacon_interval - cur_tick - BEACON_EARLY_TIME * NET80211_TU_TO_US;
			if ((s32)time_offset < 0) {
				time_offset += inter_beacon_space;
			}
		} else {
			time_offset = (idx+2)*inter_beacon_space - cur_tick - BEACON_EARLY_TIME * NET80211_TU_TO_US;
			if (time_offset > (inter_beacon_space+(inter_beacon_space >> 1))) {
				time_offset -= inter_beacon_space;
				late = 1;
			}
		}
	} else
#endif // MBSSID
	{
		time_offset = 2*beacon_interval - cur_tick - BEACON_EARLY_TIME * NET80211_TU_TO_US;
		if (time_offset > (beacon_interval+(beacon_interval >> 1))) {
			time_offset -= beacon_interval;
			late = 1;
		}
	}
	
	BUG_ON((s32)time_offset < 0);
	
	mod_timer(&pshare->beacon_timer, jiffies+usecs_to_jiffies(time_offset));
	
#ifdef UNIVERSAL_REPEATER
	if (IS_ROOT_INTERFACE(priv)) {
		if ((OPMODE & WIFI_STATION_STATE) && GET_VXD_PRIV(priv) &&
				(GET_VXD_PRIV(priv)->drv_state & DRV_STATE_VXD_AP_STARTED)) {
			priv = GET_VXD_PRIV(priv);
		}
	}
#endif
	
	if (late)
		++priv->ext_stats.beacon_er;
	
	if (priv->timoffset) {
		update_beacon(priv);
		
		// handle any buffered BC/MC frames
		reg_val8 = RTL_R8(BCN_CTRL);
		val8 = *((unsigned char *)priv->beaconbuf + priv->timoffset + 4);
		
		if (val8 & 0x01) {
			if(reg_val8 & DIS_ATIM)
				RTL_W8(BCN_CTRL, (reg_val8 & (~DIS_ATIM)));
			process_mcast_dzqueue(priv);
			priv->pkt_in_dtimQ = 0;
		} else {
			if(!(reg_val8 & DIS_ATIM))
				RTL_W8(BCN_CTRL, (reg_val8 | DIS_ATIM));
		}
	}
}
#endif // CONFIG_RTL_92C_SUPPORT || (!CONFIG_SUPPORT_USB_INT || !CONFIG_INTERRUPT_BASED_TXBCN)

u8 rtw_init_drv_sw(struct rtl8192cd_priv *priv)
{
	if (_rtw_init_cmd_priv(priv) == FAIL)
		goto cmd_fail;
	
	if (_rtw_init_xmit_priv(priv) == FAIL)
		goto xmit_fail;

	if (_rtw_init_recv_priv(priv) == FAIL)
		goto recv_fail;
	
#if defined(CONFIG_RTL_92C_SUPPORT) || (!defined(CONFIG_SUPPORT_USB_INT) || !defined(CONFIG_INTERRUPT_BASED_TXBCN))
	init_timer(&priv->pshare->beacon_timer);
	priv->pshare->beacon_timer.data = (unsigned long)priv;
	priv->pshare->beacon_timer.function = pre_rtl8192cu_beacon_timer;
	INIT_TIMER_EVENT_ENTRY(&priv->pshare->beacon_timer_event,
		rtl8192cu_beacon_timer, (unsigned long)priv);
#endif
	
	return SUCCESS;

recv_fail:
	_rtw_free_xmit_priv(priv);
xmit_fail:
	_rtw_free_cmd_priv(priv);
cmd_fail:
	
	return FAIL;
}

u8 rtw_free_drv_sw(struct rtl8192cd_priv *priv)
{
	_rtw_free_recv_priv(priv);
	_rtw_free_xmit_priv(priv);
	_rtw_free_cmd_priv(priv);
	
	return SUCCESS;
}

unsigned int ffaddr2pipehdl(struct rtl8192cd_priv *priv, u32 addr)
{
	unsigned int pipe=0;
	int ep_num=0;
	struct usb_device *pusbd = priv->pshare->pusbdev;	
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

	if(addr == RECV_BULK_IN_ADDR)
	{	
		pipe=usb_rcvbulkpipe(pusbd, 0x01);

		return pipe;
	}

	if(addr == RECV_INT_IN_ADDR)
	{
		pipe=usb_rcvintpipe(pusbd, 0x04);

		return pipe;
	}

	if(addr < HW_QUEUE_ENTRY)
	{
		ep_num = (pHalData->Queue2EPNum[(u8)addr] & 0x0f);
		
		pipe = usb_sndbulkpipe(pusbd, ep_num);

		return pipe;
	}

	return pipe;

}

