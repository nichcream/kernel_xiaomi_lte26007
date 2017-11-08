/*
 *  RX handle routines
 *
 *  $Id: 8192cd_usb_hw.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_USB_HW_C_

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"
#ifdef CONFIG_RTL_88E_SUPPORT
#include "Hal8188EPwrSeq.h"
#endif


extern struct _device_info_ wlan_device[];
extern int drv_registered;


extern int MDL_DEVINIT rtl8192cd_init_one(struct usb_interface *pusb_intf,
                  const struct usb_device_id *ent, struct _device_info_ *wdev, int vap_idx);
extern void rtl8192cd_deinit_one(struct rtl8192cd_priv *priv);


static int MDL_DEVINIT rtw_drv_init(struct usb_interface *pusb_intf, const struct usb_device_id *pdid)
{
	int ret;
#ifdef MBSSID
	int i;
#endif

	//2009.8.13, by Thomas
	// In this probe function, O.S. will provide the usb interface pointer to driver.
	// We have to increase the reference count of the usb device structure by using the usb_get_dev function.
	usb_get_dev(interface_to_usbdev(pusb_intf));
	
	ret = rtl8192cd_init_one(pusb_intf, pdid, &wlan_device[0], -1);
	if (ret)
		goto error;
	
#ifdef UNIVERSAL_REPEATER
	ret = rtl8192cd_init_one(pusb_intf, pdid, &wlan_device[0], -1);
	if (ret)
		goto error;
#endif

#ifdef MBSSID
	for (i = 0; i < RTL8192CD_NUM_VWLAN; i++) {
		ret = rtl8192cd_init_one(pusb_intf, pdid, &wlan_device[0], i);
		if (ret)
			goto error;
	}
#endif
	/*
	if(usb_dvobj_init(wlan_device[wlan_index].priv) != SUCCESS) {
		ret = -ENOMEM;
	}
	*/
	
	return 0;

error:
	if (NULL != wlan_device[0].priv) {
		rtl8192cd_deinit_one(wlan_device[0].priv);
		wlan_device[0].priv = NULL;
	}
	
	usb_put_dev(interface_to_usbdev(pusb_intf));//decrease the reference count of the usb device structure if driver fail on initialzation
	
	return ret;
}

static void MDL_DEVEXIT rtw_dev_remove(struct usb_interface *pusb_intf)
{
	struct net_device *dev = usb_get_intfdata(pusb_intf);
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = dev->priv;
#endif
	usb_set_intfdata(pusb_intf, NULL);

	if (priv) {
		priv->pshare->bDriverStopped = TRUE;
		if (TRUE == drv_registered)
			priv->pshare->bSurpriseRemoved = TRUE;
	}

	if (NULL != wlan_device[0].priv) {
		rtl8192cd_deinit_one(wlan_device[0].priv);
		wlan_device[0].priv = NULL;
	}

	usb_put_dev(interface_to_usbdev(pusb_intf));//decrease the reference count of the usb device structure when disconnect
}

static struct usb_device_id rtw_usb_id_tbl[] ={
#ifdef CONFIG_RTL_92C_SUPPORT
	/*=== Realtek demoboard ===*/		
	{USB_DEVICE(0x0BDA, 0x8191)},//Default ID

	/****** 8188CUS ********/
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8176)},//8188cu 1*1 dongole
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8170)},//8188CE-VAU USB minCard
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x817E)},//8188CE-VAU USB minCard
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x817A)},//8188cu Slim Solo
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x817B)},//8188cu Slim Combo
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x817D)},//8188RU High-power USB Dongle
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8754)},//8188 Combo for BC4
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x817F)},//8188RU
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x818A)},//RTL8188CUS-VL
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x018A)},//RTL8188CTV

	/****** 8192CUS ********/
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8177)},//8191cu 1*2
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8178)},//8192cu 2*2
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x817C)},//8192CE-VAU USB minCard
	
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8191)},//8192CU 2*2
	{USB_DEVICE(0x1058, 0x0631)},//Alpha, 8192CU

	/*=== Customer ID ===*/	
	/****** 8188CUS Dongle ********/
	{USB_DEVICE(0x2019, 0xED17)},//PCI - Edimax
	{USB_DEVICE(0x0DF6, 0x0052)},//Sitecom - Edimax
	{USB_DEVICE(0x7392, 0x7811)},//Edimax - Edimax
	{USB_DEVICE(0x07B8, 0x8189)},//Abocom - Abocom
	{USB_DEVICE(0x0EB0, 0x9071)},//NO Brand - Etop
	{USB_DEVICE(0x06F8, 0xE033)},//Hercules - Edimax
	{USB_DEVICE(0x103C, 0x1629)},//HP - Lite-On ,8188CUS Slim Combo
	{USB_DEVICE(0x2001, 0x3308)},//D-Link - Alpha
	{USB_DEVICE(0x050D, 0x1102)},//Belkin - Edimax
	{USB_DEVICE(0x2019, 0xAB2A)},//Planex - Abocom
	{USB_DEVICE(0x20F4, 0x648B)},//TRENDnet - Cameo
	{USB_DEVICE(0x4855, 0x0090)},// - Feixun
	{USB_DEVICE(0x13D3, 0x3357)},// - AzureWave
	{USB_DEVICE(0x0DF6, 0x005C)},//Sitecom - Edimax
	{USB_DEVICE(0x0BDA, 0x5088)},//Thinkware - CC&C
	{USB_DEVICE(0x4856, 0x0091)},//NetweeN - Feixun
	{USB_DEVICE(0x2019, 0x4902)},//Planex - Etop
	{USB_DEVICE(0x2019, 0xAB2E)},//SW-WF02-AD15 -Abocom

	/****** 8188 RU ********/
	{USB_DEVICE(0x0BDA, 0x317F)},//Netcore,Netcore
	
	/****** 8188CE-VAU ********/
	{USB_DEVICE(0x13D3, 0x3359)},// - Azwave
	{USB_DEVICE(0x13D3, 0x3358)},// - Azwave

	/****** 8188CUS Slim Solo********/
	{USB_DEVICE(0x04F2, 0xAFF7)},//XAVI - XAVI
	{USB_DEVICE(0x04F2, 0xAFF9)},//XAVI - XAVI
	{USB_DEVICE(0x04F2, 0xAFFA)},//XAVI - XAVI

	/****** 8188CUS Slim Combo ********/
	{USB_DEVICE(0x04F2, 0xAFF8)},//XAVI - XAVI
	{USB_DEVICE(0x04F2, 0xAFFB)},//XAVI - XAVI
	{USB_DEVICE(0x04F2, 0xAFFC)},//XAVI - XAVI
	{USB_DEVICE(0x2019, 0x1201)},//Planex - Vencer
	
	/****** 8192CUS Dongle ********/
	{USB_DEVICE(0x2001, 0x3307)},//D-Link - Cameo
	{USB_DEVICE(0x2001, 0x330A)},//D-Link - Alpha
	{USB_DEVICE(0x2001, 0x3309)},//D-Link - Alpha
	{USB_DEVICE(0x0586, 0x341F)},//Zyxel - Abocom
	{USB_DEVICE(0x7392, 0x7822)},//Edimax - Edimax
	{USB_DEVICE(0x2019, 0xAB2B)},//Planex - Abocom
	{USB_DEVICE(0x07B8, 0x8178)},//Abocom - Abocom
	{USB_DEVICE(0x07AA, 0x0056)},//ATKK - Gemtek
	{USB_DEVICE(0x4855, 0x0091)},// - Feixun
	{USB_DEVICE(0x2001, 0x3307)},//D-Link-Cameo
	{USB_DEVICE(0x050D, 0x2102)},//Belkin - Sercomm
	{USB_DEVICE(0x050D, 0x2103)},//Belkin - Edimax
	{USB_DEVICE(0x20F4, 0x624D)},//TRENDnet
	{USB_DEVICE(0x0DF6, 0x0061)},//Sitecom - Edimax
	{USB_DEVICE(0x0B05, 0x17AB)},//ASUS - Edimax
	{USB_DEVICE(0x0846, 0x9021)},//Netgear - Sercomm
	{USB_DEVICE(0x0E66, 0x0019)},//Hawking,Edimax 

	/****** 8192CE-VAU  ********/
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8186)},//Intel-Xavi( Azwave)
#endif // CONFIG_RTL_92C_SUPPORT

#ifdef CONFIG_RTL_92D_SUPPORT
	/*=== Realtek demoboard ===*/
	/****** 8192DU ********/
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8193)},//8192DU-VC
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8194)},//8192DU-VS
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8111)},//Realtek 5G dongle for WiFi Display
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x0193)},//8192DE-VAU
	
	/*=== Customer ID ===*/
	/****** 8192DU-VC ********/
	{USB_DEVICE(0x2019, 0xAB2C)},//PCI - Abocm
	{USB_DEVICE(0x2019, 0x4903)},//PCI - ETOP
	{USB_DEVICE(0x2019, 0x4904)},//PCI - ETOP
	{USB_DEVICE(0x07B8, 0x8193)},//Abocom - Abocom

	/****** 8192DU-VS ********/
	{USB_DEVICE(0x20F4, 0x664B)},//TRENDnet

	/****** 8192DU-WiFi Display Dongle ********/
	{USB_DEVICE(0x2019, 0xAB2D)},//Planex - Abocom ,5G dongle for WiFi Display
#endif // CONFIG_RTL_92D_SUPPORT

#ifdef CONFIG_RTL_88E_SUPPORT
	/*=== Realtek demoboard ===*/
	{USB_DEVICE(USB_VENDER_ID_REALTEK, 0x8179)},/* Default ID */
#endif // CONFIG_RTL_88E_SUPPORT

	{}	/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, rtw_usb_id_tbl);

struct usb_driver rtl8192cd_usb_driver = {
	.name = (char*)DRV_NAME,
	.id_table = rtw_usb_id_tbl,
	.probe = rtw_drv_init,
	.disconnect = rtw_dev_remove,
};


#ifdef CONFIG_RTL_92C_SUPPORT
#define MAX_PAGE_SIZE				4096	// @ page : 4k bytes
#define MAX_REG_BOLCK_SIZE			196
#define MIN_REG_BOLCK_SIZE			8
#define FW_8192C_START_ADDRESS		0x1000

static int _BlockWrite(struct rtl8192cd_priv *priv, void *buffer, u32 size)
{
	int ret = 0;

#ifdef CONFIG_PCI_HCI
	u32			blockSize	= sizeof(u32);	// Use 4-byte write to download FW
	u8			*bufferPtr	= (u8 *)buffer;
	u32			*pu4BytePtr	= (u32 *)buffer;
	u32			i, offset, blockCount, remainSize;

	blockCount = size / blockSize;
	remainSize = size % blockSize;

	for(i = 0 ; i < blockCount ; i++){
		offset = i * blockSize;
		RTL_W32((FW_8192C_START_ADDRESS + offset), cpu_to_le32(*(pu4BytePtr + i)));
	}

	if(remainSize){
		offset = blockCount * blockSize;
		bufferPtr += offset;
		
		for(i = 0 ; i < remainSize ; i++){
			RTL_W8((FW_8192C_START_ADDRESS + offset + i), *(bufferPtr + i));
		}
	}
#else
  
#ifdef SUPPORTED_BLOCK_IO
	u32			blockSize	= MAX_REG_BOLCK_SIZE;	// Use 196-byte write to download FW	
	u32			blockSize2  = MIN_REG_BOLCK_SIZE;	
#else
	u32			blockSize	= sizeof(u32);	// Use 4-byte write to download FW
	u32*		pu4BytePtr	= (u32*)buffer;
	u32			blockSize2  = sizeof(u8);
#endif
	u8*			bufferPtr	= (u8*)buffer;
	u32			i, offset = 0, offset2, blockCount, remainSize, remainSize2;

	blockCount = size / blockSize;
	remainSize = size % blockSize;

	for(i = 0 ; i < blockCount ; i++){
		offset = i * blockSize;
		#ifdef SUPPORTED_BLOCK_IO
		ret = RTL_Wn((FW_8192C_START_ADDRESS + offset), blockSize, (bufferPtr + offset));
		#else
		ret = RTL_W32((FW_8192C_START_ADDRESS + offset), le32_to_cpu(*(pu4BytePtr + i)));
		#endif

		if (IS_ERR_VALUE(ret))
			goto exit;
	}

	if(remainSize){
		offset2 = blockCount * blockSize;		
		blockCount = remainSize / blockSize2;
		remainSize2 = remainSize % blockSize2;

		for(i = 0 ; i < blockCount ; i++){
			offset = offset2 + i * blockSize2;
			#ifdef SUPPORTED_BLOCK_IO
			ret = RTL_Wn((FW_8192C_START_ADDRESS + offset), blockSize2, (bufferPtr + offset));
			#else
			ret = RTL_W8((FW_8192C_START_ADDRESS + offset ), *(bufferPtr + offset));
			#endif

			if (IS_ERR_VALUE(ret))
				goto exit;
		}

		if(remainSize2)
		{
			offset = (blockCount ? (offset+blockSize2) : offset2);
			bufferPtr += offset;
			
			for(i = 0 ; i < remainSize2 ; i++){
				ret = RTL_W8((FW_8192C_START_ADDRESS + offset + i), *(bufferPtr + i));

				if (IS_ERR_VALUE(ret))
					goto exit;
			}
		}
	}
exit:
	
#endif

	return ret;
}

static int _PageWrite(struct rtl8192cd_priv *priv, u32 page, void *buffer, u32 size)
{
	u8 value8;
	u8 u8Page = (u8) (page & 0x07) ;

	value8 = (RTL_R8(REG_MCUFWDL+2)&0xF8) | u8Page ;
	RTL_W8(REG_MCUFWDL+2,value8);
	return _BlockWrite(priv, buffer, size);
}

#ifdef CONFIG_PCI_HCI
static void _FillDummy(u8 *pFwBuf, u32 *pFwLen)
{
	u32	FwLen = *pFwLen;
	u8	remain = (u8)(FwLen%4);
	
	if (remain) {
		remain = 4 - remain;
		
		do {
			pFwBuf[FwLen] = 0;
			FwLen++;
			remain--;
		} while (remain > 0);
	}

	*pFwLen = FwLen;
}
#endif

static int _WriteFW(struct rtl8192cd_priv *priv, void *buffer, u32 size)
{
	// Since we need dynamic decide method of dwonload fw, so we call this function to get chip version.
	// We can remove _ReadChipVersion from ReadAdapterInfo8192C later.

	int ret = 0;
	BOOLEAN	isNormalChip = !IS_TEST_CHIP(priv);
	
	if(isNormalChip){
		u32 	pageNums,remainSize ;
		u32 	page,offset;
		u8*		bufferPtr = (u8*)buffer;

#ifdef CONFIG_PCI_HCI
		// 20100120 Joseph: Add for 88CE normal chip. 
		// Fill in zero to make firmware image to dword alignment.
		_FillDummy(bufferPtr, &size);
#endif

		pageNums = size / MAX_PAGE_SIZE;
		//RT_ASSERT((pageNums <= 4), ("Page numbers should not greater then 4 \n"));
		remainSize = size % MAX_PAGE_SIZE;
		
		for(page = 0; page < pageNums;  page++)
		{
			offset = page * MAX_PAGE_SIZE;
			
			ret = _PageWrite(priv, page, (bufferPtr+offset), MAX_PAGE_SIZE);
			if (IS_ERR_VALUE(ret))
				goto exit;
		}
		if(remainSize)
		{
			offset = pageNums * MAX_PAGE_SIZE;
			page = pageNums;
			
			ret = _PageWrite(priv, page, (bufferPtr+offset), remainSize);
			if (IS_ERR_VALUE(ret))
				goto exit;
		}	
		//RT_TRACE(COMP_INIT, DBG_LOUD, ("_WriteFW Done- for Normal chip.\n"));
	}
	else
	{
		ret = _BlockWrite(priv, buffer, size);
		if (IS_ERR_VALUE(ret))
			goto exit;
		//RT_TRACE(COMP_INIT, DBG_LOUD, ("_WriteFW Done- for Test chip.\n"));
	}

exit:
	return ret;
}

int rtl8192cu_InitPowerOn(struct rtl8192cd_priv *priv)
{
	volatile unsigned char bytetmp;
	unsigned short retry;
	
	retry = 0;
	while (!(RTL_R8(APS_FSMCO) & PFM_ALDN)) {
		if (++retry > POLLING_READY_TIMEOUT_COUNT)
			return FALSE;
	}

//	For hardware power on sequence.

	// 0.	RSV_CTRL 0x1C[7:0] = 0x00			// unlock ISO/CLK/Power control register	
	RTL_W8(RSV_CTRL0, 0x00);
	// Power on when re-enter from IPS/Radio off/card disable
	// 1. SPS0_CTRL 0x11[7:0] = 0x2B
	RTL_W8(SPS0_CTRL, 0x2b);		// enable SPS into PWM
	
	delay_us(100);	//this is not necessary when initially power on

	bytetmp = RTL_R8(LDOV12D_CTRL);
	if (0 == (bytetmp & LDV12_EN)) {
		RTL_W8(LDOV12D_CTRL, bytetmp | LDV12_EN);
		delay_us(100);
		RTL_W8(SYS_ISO_CTRL, RTL_R8(SYS_ISO_CTRL) & ~ISO_MD2PP);
	}
	
	// auto enable WLAN
	// 4. APS_FSMCO 0x04[8] = 1;	// wait till 0x04[8] = 0
	RTL_W16(APS_FSMCO, RTL_R16(APS_FSMCO) | APFM_ONMAC);

	retry = 0;
	while (RTL_R16(APS_FSMCO) & APFM_ONMAC) {
		if (++retry > POLLING_READY_TIMEOUT_COUNT)
			return FALSE;
	}
	
	// Enable Radio off, GPIO, and LED function
	// 8. APS_FSMCO 0x04[15:0] = 0x0812
	RTL_W16(APS_FSMCO, 0x0812);			// when enable HWPDN
	
	// release RF digital isolation
	// 9. SYS_ISO_CTRL 0x01[1] = 0x0
	RTL_W16(SYS_ISO_CTRL, RTL_R16(SYS_ISO_CTRL) & ~ISO_DIOR);
	
	// Release MAC IO register reset
	RTL_W32(CR, RTL_R32(CR)|MACRXEN|MACTXEN|SCHEDULE_EN|PROTOCOL_EN
		|RXDMA_EN|TXDMA_EN|HCI_RXDMA_EN|HCI_TXDMA_EN);
	
	return SUCCESS;
}

int rtl8192cd_stop_hw(struct rtl8192cd_priv *priv)
{
	u8 value8=0;
	BOOLEAN bWithoutHWSM = priv->pshare->bCardDisableWOHSM;
	
	// 1.) (Only for AP Series)Stop receiving Interrupt quickly		
//	RTL_W32(HIMR, 0);
//	RTL_W16(HIMRE, 0);
//	RTL_W16(HIMRE+2, 0);
	RTL_W32(CR, (RTL_R32(CR) & ~(NETYPE_Mask << NETYPE_SHIFT)) | ((NETYPE_NOLINK & NETYPE_Mask) << NETYPE_SHIFT));

	// 2.) ==== RF Off Sequence ====
	phy_InitBBRFRegisterDefinition(priv);		// preparation for read/write RF register
	
	RTL_W8(TXPAUSE, 0xff);								// Pause MAC TX queue
	PHY_SetRFReg(priv, RF92CD_PATH_A, 0x00, bMask20Bits, 0x00);	// disable RF
	RTL_W8(APSD_CTRL, 0x40);
	RTL_W8(SYS_FUNC_EN, 0x16);		// reset BB state machine
	RTL_W8(SYS_FUNC_EN, 0x14);		// reset BB state machine

	// 3.) ==== Reset digital sequence ====
	printk( "_\n" );
	if (RTL_R8(MCUFWDL) & BIT(1)) {
		//Make sure that Host Recovery Interrupt is handled by 8051 ASAP.
		RTL_W32(FSIMR, 0);				// clear FSIMR
		RTL_W32(FWIMR, 0x20);			// clear FWIMR except HRCV_INT
		RTL_W32(FTIMR, 0);				// clear FTIMR
		
		FirmwareSelfReset(priv);

		//Clear FWIMR to guarantee if 8051 runs in ROM, it is impossible to run FWISR Interrupt handler
		RTL_W32(FWIMR, 0x0);			// clear All FWIMR
	} else {
		//Critical Error.
		//the operation that reset 8051 is necessary to be done by 8051
		printk("%s %d ERROR: (RTL_R8(MCUFWDL) & BIT(1))=0\n", __FUNCTION__, __LINE__);
		printk("%s %d ERROR: the operation that reset 8051 is necessary to be done by 8051\n", __FUNCTION__, __LINE__);
	}
	RTL_W8(SYS_FUNC_EN+1, 0x54);			// reset MCU, MAC register, DCORE

	// Clear rpwm value for initial toggle bit trigger.
	RTL_W8(REG_USB_HRPWM, 0x00);
	
	if(bWithoutHWSM){
	/*****************************
		Without HW auto state machine
	g.	SYS_CLKR 0x08[15:0] = 0x30A3			//disable MAC clock
	h.	AFE_PLL_CTRL 0x28[7:0] = 0x80			//disable AFE PLL
	i.	AFE_XTAL_CTRL 0x24[15:0] = 0x880F		//gated AFE DIG_CLOCK
	j.	SYS_ISO_CTRL 0x00[7:0] = 0xF9			// isolated digital to PON
	******************************/	
		//RTL_W16(SYS_CLKR, 0x30A3);
		RTL_W16(SYS_CLKR, 0x70A3);//modify to 0x70A3 by Scott.
		RTL_W8(AFE_PLL_CTRL, 0x80);
		RTL_W16(AFE_XTAL_CTRL, 0x880F);
		RTL_W8(SYS_ISO_CTRL, 0xF9);
	} else {		
		// Disable all RF/BB power 
		RTL_W8(REG_RF_CTRL, 0x00);
	}

	//  ==== Reset digital sequence   ======
	if(bWithoutHWSM){
	 	RTL_W16(SYS_CLKR, 0x70a3); //modify to 0x70a3 by Scott.
	 	RTL_W8(SYS_ISO_CTRL+1, 0x82); //modify to 0x82 by Scott.
	}
	
	// 4.) ==== Disable analog sequence ====
	if(bWithoutHWSM){
	/*****************************
	n.	LDOA15_CTRL 0x20[7:0] = 0x04		// disable A15 power
	o.	LDOV12D_CTRL 0x21[7:0] = 0x54		// disable digital core power
	r.	When driver call disable, the ASIC will turn off remaining clock automatically 
	******************************/
	
		RTL_W8(LDOA15_CTRL, 0x04);
		//RTL_W8(LDOV12D_CTRL, 0x54);		
		
		RTL_W8(LDOV12D_CTRL, RTL_R8(LDOV12D_CTRL) & ~LDV12_EN);
	}
	
/*****************************
h.	SPS0_CTRL 0x11[7:0] = 0x23			//enter PFM mode
i.	APS_FSMCO 0x04[15:0] = 0x4802		// set USB suspend 
******************************/

	value8 = 0x23;
	if (IS_UMC_B_CUT_88C(priv))
		value8 |= BIT(3);

	RTL_W8(SPS0_CTRL, value8);
	RTL_W16(APS_FSMCO, 0x4802);

	RTL_W8(RSV_CTRL0, 0x0e);

	return SUCCESS;
}
#endif // CONFIG_RTL_92C_SUPPORT

#ifdef CONFIG_RTL_88E_SUPPORT
#define FW_8188E_SIZE				0x4000 //16384,16k
#define FW_8188E_START_ADDRESS		0x1000
#define FW_8188E_END_ADDRESS		0x1FFF //0x5FFF

#define MAX_PAGE_SIZE			4096	// @ page : 4k bytes
#define MAX_REG_BOLCK_SIZE	196

static int _BlockWrite(struct rtl8192cd_priv *priv, void *buffer, u32 buffSize)
{
	int ret = 0;

	u32			blockSize_p1 = 4;	// (Default) Phase #1 : PCI muse use 4-byte write to download FW
	u32			blockCount_p1 = 0;
	u32			remainSize_p1 = 0;
	u8			*bufferPtr	= (u8*)buffer;
	u32			i=0;
	
#ifdef CONFIG_PCI_HCI
	u8			remainFW[4] = {0, 0, 0, 0};
	u8			*p = NULL;

	blockCount_p1 = buffSize / blockSize_p1;
	remainSize_p1 = buffSize % blockSize_p1;
	
	for (i = 0; i < blockCount_p1; i++) {
		RTL_W32((FW_8188E_START_ADDRESS + i * blockSize_p1), le32_to_cpu(*((u32*)(bufferPtr + i * blockSize_p1))));
	}
	
	p = (u8*)((u32*)(bufferPtr + blockCount_p1 * blockSize_p1));
	if (remainSize_p1) {
		switch (remainSize_p1) {
		case 3:
			remainFW[2]=*(p+2);
		case 2:
			remainFW[1]=*(p+1);
		case 1:
			remainFW[0]=*(p);
			RTL_W32((FW_8188E_START_ADDRESS + blockCount_p1 * blockSize_p1),
				 le32_to_cpu(*(u32*)remainFW));
		}
	}
	
#else // !CONFIG_PCI_HCI
	u32			blockSize_p2 = 8;	// Phase #2 : Use 8-byte, if Phase#1 use big size to write FW.
	u32			blockSize_p3 = 1;	// Phase #3 : Use 1-byte, the remnant of FW image.
	u32			blockCount_p2 = 0, blockCount_p3 = 0;
	u32			remainSize_p2 = 0;
	u32			offset=0;

#ifdef CONFIG_USB_HCI
	blockSize_p1 = MAX_REG_BOLCK_SIZE;
#endif
	
	//3 Phase #1
	blockCount_p1 = buffSize / blockSize_p1;
	remainSize_p1 = buffSize % blockSize_p1;
	
	for (i = 0; i < blockCount_p1; i++)
	{
#ifdef CONFIG_USB_HCI
		ret = RTL_Wn((FW_8188E_START_ADDRESS + i * blockSize_p1), blockSize_p1, (bufferPtr + i * blockSize_p1));
#else
		ret = RTL_W32((FW_8188E_START_ADDRESS + i * blockSize_p1), le32_to_cpu(*((u32*)(bufferPtr + i * blockSize_p1))));
#endif
		if (IS_ERR_VALUE(ret))
			goto exit;
	}

	//3 Phase #2
	if (remainSize_p1)
	{
		offset = blockCount_p1 * blockSize_p1;

		blockCount_p2 = remainSize_p1/blockSize_p2;
		remainSize_p2 = remainSize_p1%blockSize_p2;

#ifdef CONFIG_USB_HCI
		for (i = 0; i < blockCount_p2; i++) {
			ret = RTL_Wn((FW_8188E_START_ADDRESS + offset + i*blockSize_p2), blockSize_p2, (bufferPtr + offset + i*blockSize_p2));
			if (IS_ERR_VALUE(ret))
				goto exit;
		}
#endif
	}

	//3 Phase #3
	if (remainSize_p2)
	{
		offset = (blockCount_p1 * blockSize_p1) + (blockCount_p2 * blockSize_p2);

		blockCount_p3 = remainSize_p2 / blockSize_p3;

		for(i = 0 ; i < blockCount_p3 ; i++){
			ret = RTL_W8((FW_8188E_START_ADDRESS + offset + i), *(bufferPtr + offset + i));
			if (IS_ERR_VALUE(ret))
				goto exit;
		}
	}

exit:
#endif // CONFIG_PCI_HCI

	return ret;
}

static int _PageWrite(struct rtl8192cd_priv *priv, u32 page, void *buffer, u32 size)
{
	u8 value8;
	u8 u8Page = (u8) (page & 0x07) ;

	value8 = (RTL_R8(REG_MCUFWDL+2)&0xF8) | u8Page ;
	RTL_W8(REG_MCUFWDL+2,value8);
	return _BlockWrite(priv, buffer, size);
}

#ifdef CONFIG_PCI_HCI
static void _FillDummy(u8 *pFwBuf, u32 *pFwLen)
{
	u32	FwLen = *pFwLen;
	u8	remain = (u8)(FwLen%4);
	
	if (remain) {
		remain = 4 - remain;
		
		do {
			pFwBuf[FwLen] = 0;
			FwLen++;
			remain--;
		} while (remain > 0);
	}

	*pFwLen = FwLen;
}
#endif

static int _WriteFW(struct rtl8192cd_priv *priv, void *buffer, u32 size)
{
	// Since we need dynamic decide method of dwonload fw, so we call this function to get chip version.
	// We can remove _ReadChipVersion from ReadpadapterInfo8192C later.
	int ret = 0;
	u32 	pageNums,remainSize ;
	u32 	page, offset;
	u8		*bufferPtr = (u8*)buffer;

#ifdef CONFIG_PCI_HCI
	// 20100120 Joseph: Add for 88CE normal chip.
	// Fill in zero to make firmware image to dword alignment.
//		_FillDummy(bufferPtr, &size);
#endif

	pageNums = size / MAX_PAGE_SIZE ;
	//RT_ASSERT((pageNums <= 4), ("Page numbers should not greater then 4 \n"));
	remainSize = size % MAX_PAGE_SIZE;

	for (page = 0; page < pageNums; page++) {
		offset = page * MAX_PAGE_SIZE;
		
		ret = _PageWrite(priv, page, bufferPtr+offset, MAX_PAGE_SIZE);
		if (IS_ERR_VALUE(ret))
			goto exit;
	}
	if (remainSize) {
		offset = pageNums * MAX_PAGE_SIZE;
		page = pageNums;
		
		ret = _PageWrite(priv, page, bufferPtr+offset, remainSize);
		if (IS_ERR_VALUE(ret))
			goto exit;
	}

exit:
	return ret;
}

void _8051Reset88E(struct rtl8192cd_priv *priv)
{
	u8 u1bTmp;

	u1bTmp = RTL_R8(REG_SYS_FUNC_EN+1);
	RTL_W8(REG_SYS_FUNC_EN+1, u1bTmp&(~BIT2));
	RTL_W8(REG_SYS_FUNC_EN+1, u1bTmp|(BIT2));
}

int Load_88E_Firmware(struct rtl8192cd_priv *priv)
{
	int fw_len, wait_cnt=0;
#ifdef CONFIG_PCI_HCI
	unsigned int CurPtr=0;
	unsigned int WriteAddr;
	unsigned int Temp;
#endif
	unsigned char *ptmp;
	u8 value8;
	int ret = TRUE;

#ifdef CONFIG_RTL8672
	printk("val=%x\n", RTL_R8(0x80));
#endif

#ifdef MP_TEST
	if (priv->pshare->rf_ft_var.mp_specific)
		return TRUE;
#endif

	printk("===> %s\n", __FUNCTION__);

	ptmp = Array_8188E_FW_AP + 32;
	fw_len = ArrayLength_8188E_FW_AP - 32;

	// Disable SIC
	RTL_W8(0x41, 0x40);
	delay_ms(1);

#ifdef CONFIG_RTL8672
	RTL_W8(0x04, RTL_R8(0x04) | 0x02);
	delay_ms(1);  //czyao
#endif

	// Load SRAM
	RTL_W8(MCUFWDL, RTL_R8(MCUFWDL) | MCUFWDL_EN);
	delay_ms(1);
	
	RTL_W32(MCUFWDL, RTL_R32(MCUFWDL) & 0xfff0ffff);
	delay_ms(1);

#ifdef CONFIG_PCI_HCI
	WriteAddr = 0x1000;
	while (CurPtr < fw_len) {
		if ((CurPtr+4) > fw_len) {
			// Reach the end of file.
			while (CurPtr < fw_len) {
				Temp = *(ptmp + CurPtr);
				RTL_W8(WriteAddr, (unsigned char)Temp);
				WriteAddr++;
				CurPtr++;
			}
		} else {
			// Write FW content to memory.
			Temp = *((unsigned int *)(ptmp + CurPtr));
			Temp = cpu_to_le32(Temp);
			RTL_W32(WriteAddr, Temp);
			WriteAddr += 4;

			if((IS_TEST_CHIP(priv)==0) && (WriteAddr == 0x2000)) {
				unsigned char tmp = RTL_R8(MCUFWDL+2);
				tmp += 1;
				WriteAddr = 0x1000;
				RTL_W8(MCUFWDL+2, tmp) ;
				delay_ms(10);
//				printk("\n[CurPtr=%x, 0x82=%x]\n", CurPtr, RTL_R8(0x82));
			}
			CurPtr += 4;
		}
	}
#else
	if (IS_ERR_VALUE(_WriteFW(priv, ptmp, fw_len))) {
		printk("WriteFW FAIL !\n");
		ret = FALSE;
	}
#endif

	// MCU firmware download disable.
	RTL_W8(MCUFWDL, RTL_R8(MCUFWDL) & 0xfe);
	delay_ms(1);
	// Reserved for fw extension.
	RTL_W8(0x81, 0x00);
	
	if (FALSE == ret)
		return FALSE;
	
	value8 = RTL_R8(MCUFWDL);
	value8 |= MCUFWDL_RDY;
	value8 &= ~WINTINI_RDY;
	RTL_W8(MCUFWDL, value8);
	
	// Enable MCU
	value8 = RTL_R8(REG_SYS_FUNC_EN+1);
	RTL_W8(REG_SYS_FUNC_EN+1, value8 & (~BIT2));
	RTL_W8(REG_SYS_FUNC_EN+1, value8|BIT2);

	printk("<=== %s\n", __FUNCTION__);

	// check if firmware is ready
	while (!(RTL_R8(MCUFWDL) & WINTINI_RDY)) {
		if (++wait_cnt > 10) {
			printk("8188e firmware not ready\n");
			return FALSE;
		}
		
		delay_ms(1);
	}
#ifdef CONFIG_RTL8672
	printk("val=%x\n",RTL_R8(MCUFWDL));
#endif

	return TRUE;
}
#endif // CONFIG_RTL_88E_SUPPORT

#ifdef CONFIG_RTL_88E_SUPPORT
int rtl8188eu_InitPowerOn(struct rtl8192cd_priv *priv)
{
	u16 value16;
	
	// unlock ISO/CLK/Power control register
	RTL_W8(RSV_CTRL0, 0x00);
	
	// HW Power on sequence
	if (FALSE == HalPwrSeqCmdParsing(priv, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, 
			PWR_INTF_USB_MSK, rtl8188E_power_on_flow)) {
		panic_printk("%s %d, HalPwrSeqCmdParsing init fail!!!\n", __FUNCTION__, __LINE__);
		return FALSE;
	}
	
	// Enable MAC DMA/WMAC/SCHEDULE/SEC block
	// Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.
	RTL_W16(CR, 0x00);  //suggseted by zhouzhou, by page, 20111230
	
	// Enable MAC DMA/WMAC/SCHEDULE/SEC block
	value16 = RTL_R16(CR);
	value16 |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
				| PROTOCOL_EN | SCHEDULE_EN | CALTMR_EN);
	// for SDIO - Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.
	
	RTL_W16(CR, value16);
	
	return SUCCESS;
}
#endif // CONFIG_RTL_88E_SUPPORT

#ifdef CONFIG_RTL_88E_SUPPORT
int rtl8192cd_stop_hw(struct rtl8192cd_priv *priv)
{
#ifdef TXREPORT
	RTL8188E_DisableTxReport(priv);
#endif
	RTL_W32(REG_88E_HIMR, 0);
	RTL_W32(REG_88E_HIMRE, 0);
	
	RTL_W8(RCR, 0);
	RTL_W8(TXPAUSE, 0xff);				// Pause MAC TX queue
	RTL_W8(CR, 0x0);
	
	// Run LPS WL RFOFF flow
	if (FALSE == HalPwrSeqCmdParsing(priv, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
			PWR_INTF_USB_MSK, rtl8188E_enter_lps_flow)) {
		DEBUG_ERR("[%s %d] run RF OFF flow fail!\n", __FUNCTION__, __LINE__);
	}
	
	if (RTL_R8(MCUFWDL) & BIT(7)) { //8051 RAM code
		// Reset MCU 0x2[10]=0.
		RTL_W8(SYS_FUNC_EN+1, RTL_R8(SYS_FUNC_EN+1) & ~ BIT(2));
	}
	
	// MCUFWDL 0x80[1:0]=0
	// reset MCU ready status
	RTL_W8(MCUFWDL, 0);
	
	//Disable 32k
	RTL_W8(REG_32K_CTRL, RTL_R8(REG_32K_CTRL) & ~BIT0);
	
	// Card disable power action flow
	if (FALSE == HalPwrSeqCmdParsing(priv, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
			PWR_INTF_USB_MSK, rtl8188E_card_disable_flow)) {
		DEBUG_ERR("[%s %d] run CARD DISABLE flow fail!\n", __FUNCTION__, __LINE__);
	}
	
	// Reset MCU IO Wrapper
	RTL_W8(RSV_CTRL0+1, RTL_R8(RSV_CTRL0+1) & ~ BIT(3));
	RTL_W8(RSV_CTRL0+1, RTL_R8(RSV_CTRL0+1) | BIT(3));
	
	// lock ISO/CLK/Power control register
	RTL_W8(RSV_CTRL0, 0x0e);
	
	return SUCCESS;
}
#endif // CONFIG_RTL_88E_SUPPORT

#ifdef MBSSID
void rtl8192cd_init_mbssid(struct rtl8192cd_priv *priv)
{
	int i, j;
	unsigned int camData[2];
	unsigned char *macAddr = GET_MY_HWADDR;
	unsigned int vap_bcn_offset;
	int nr_vap;
	_irqL irqL;

	SMP_LOCK_MBSSID(&irqL);

	if (IS_ROOT_INTERFACE(priv))
	{
		//camData[0] = 0x00800000 | (macAddr[5] << 8) | macAddr[4];
		camData[0] = MBIDCAM_POLL | MBIDWRITE_EN | MBIDCAM_VALID | (macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
//		for (j=0; j<2; j++) {
		for (j=1; j>=0; j--) {
			//RTL_W32((_MBIDCAMCONTENT_+4)-4*j, camData[j]);
			RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
		}
		//RTL_W8(_MBIDCAMCFG_, BIT(7) | BIT(6));

		// clear the rest area of CAM
		//camData[0] = 0;
		camData[1] = 0;
		for (i=1; i<8; i++) {
			camData[0] = MBIDCAM_POLL | MBIDWRITE_EN | (i&MBIDCAM_ADDR_Mask)<<MBIDCAM_ADDR_SHIFT;
//			for (j=0; j<2; j++) {
			for (j=1; j>=0; j--) {
				RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
			}
//			RTL_W8(_MBIDCAMCFG_, BIT(7) | BIT(6) | (unsigned char)i);
		}
		
		priv->pshare->nr_vap_bcn = 0;
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		priv->pshare->bcn_priv[0] = priv;
		priv->pshare->inter_bcn_space = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * NET80211_TU_TO_US;
#endif

		// set MBIDCTRL & MBID_BCN_SPACE by cmd
//		set_fw_reg(priv, 0xf1000101, 0, 0);
		RTL_W32(MBSSID_BCN_SPACE,
			(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BCN_SPACE2_Mask)<<BCN_SPACE2_SHIFT
			|(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BCN_SPACE1_Mask)<<BCN_SPACE1_SHIFT);

		RTL_W8(BCN_CTRL, 0);
		RTL_W8(0x553, 1);

#ifdef CONFIG_RTL_92C_SUPPORT
		if (IS_TEST_CHIP(priv))
			RTL_W8(BCN_CTRL, EN_BCN_FUNCTION |EN_MBSSID| DIS_SUB_STATE | DIS_TSF_UPDATE|EN_TXBCN_RPT);
		else
#endif
			RTL_W8(BCN_CTRL, EN_BCN_FUNCTION | DIS_SUB_STATE_N | DIS_TSF_UPDATE_N|EN_TXBCN_RPT);

		RTL_W32(RCR, RTL_R32(RCR) | RCR_MBID_EN);	// MBSSID enable
/*
#ifdef CLIENT_MODE
		if ((OPMODE & WIFI_STATION_STATE) || (OPMODE & WIFI_ADHOC_STATE))
			RTL_W32(RCR, RTL_R32(RCR) | RCR_CBSSID);
#endif
*/
	}
	else if (IS_VAP_INTERFACE(priv))
	{
//		priv->vap_init_seq = (RTL_R8(_MBIDCTRL_) & (BIT(4) | BIT(5) | BIT(6))) >> 4;
//		priv->vap_init_seq++;
//		set_fw_reg(priv, 0xf1000001 | ((priv->vap_init_seq + 1)&0xffff)<<8, 0, 0);
		
		priv->vap_init_seq = priv->vap_id +1;

#if defined(CONFIG_RTL_92E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT) //eric_8812 ??
//		printk("init swq=%d\n", priv->vap_init_seq);
		if(GET_CHIP_VER(priv)==VERSION_8192E|| GET_CHIP_VER(priv)==VERSION_8812E)
		{
		switch (priv->vap_init_seq)
		{
			case 1:
				RTL_W16(REG_92E_ATIMWND1, 0x03); //0x3C);		
				RTL_W8(REG_92E_DTIM_COUNT_VAP1, priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod);
				break;
			case 2:
				RTL_W8(REG_92E_ATIMWND2, 0x3C);
				RTL_W8(REG_92E_DTIM_COUNT_VAP2, priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod);
				break;
			case 3:
				RTL_W8(REG_92E_ATIMWND3, 0x3C);
				RTL_W8(REG_92E_DTIM_COUNT_VAP3, priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod);
				break;
			case 4:
				RTL_W8(REG_92E_ATIMWND4, 0x3C);
				RTL_W8(REG_92E_DTIM_COUNT_VAP4, priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod);
				break;
			case 5:
				RTL_W8(REG_92E_ATIMWND5, 0x3C);
				RTL_W8(REG_92E_DTIM_COUNT_VAP5, priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod);
				break;
			case 6:
				RTL_W8(REG_92E_ATIMWND6, 0x3C);
				RTL_W8(REG_92E_DTIM_COUNT_VAP6, priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod);
				break;
			case 7:
				RTL_W8(REG_92E_ATIMWND7, 0x3C);
				RTL_W8(REG_92E_DTIM_COUNT_VAP7, priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod);
				break;
		}
		}
#endif
		
		camData[0] = MBIDCAM_POLL | MBIDWRITE_EN | MBIDCAM_VALID |
				(priv->vap_init_seq&MBIDCAM_ADDR_Mask)<<MBIDCAM_ADDR_SHIFT |
				(macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		for (j=1; j>=0; j--) {
			RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
		}
//		RTL_W8(_MBIDCAMCFG_, BIT(7) | BIT(6) | ((unsigned char)priv->vap_init_seq & 0x1f));
		
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		priv->pshare->bcn_priv[priv->pshare->nr_vap_bcn+1] = priv;
#endif
		nr_vap = ++priv->pshare->nr_vap_bcn;

#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E) {
			vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod-
				((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod*nr_vap)/(nr_vap+1))-1;

			if (vap_bcn_offset > 200)
				vap_bcn_offset = 200;
			RTL_W32(MBSSID_BCN_SPACE, (vap_bcn_offset & BCN_SPACE2_Mask)<<BCN_SPACE2_SHIFT
				|(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BCN_SPACE1_Mask)<<BCN_SPACE1_SHIFT);
		} else
#endif
#if defined(CONFIG_RTL_92E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT)
		if ((GET_CHIP_VER(priv)==VERSION_8192E) || (GET_CHIP_VER(priv)==VERSION_8812E)) {
			vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod-
				((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod*nr_vap)/(nr_vap+1))-1;

			if (vap_bcn_offset > 200)
				vap_bcn_offset = 200;
			RTL_W32(MBSSID_BCN_SPACE, (vap_bcn_offset & BCN_SPACE2_Mask)<<BCN_SPACE2_SHIFT
				|((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod/(nr_vap+1)) & BCN_SPACE1_Mask)
				<<BCN_SPACE1_SHIFT);
		} else
#endif
		{
			vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod-
				((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod*nr_vap)/(nr_vap+1));
			
			RTL_W32(MBSSID_BCN_SPACE, ((vap_bcn_offset & BCN_SPACE2_Mask) << BCN_SPACE2_SHIFT)
				|((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod/(nr_vap+1)) & BCN_SPACE1_Mask)
				<<BCN_SPACE1_SHIFT);
		}
		
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		priv->pshare->inter_bcn_space = vap_bcn_offset * NET80211_TU_TO_US;
#endif
		
		RTL_W8(BCN_CTRL, 0);
		RTL_W8(0x553, 1);

#ifdef CONFIG_RTL_92C_SUPPORT
		if (IS_TEST_CHIP(priv))
			RTL_W8(BCN_CTRL, EN_BCN_FUNCTION |EN_MBSSID| DIS_SUB_STATE | DIS_TSF_UPDATE|EN_TXBCN_RPT);
		else
#endif
			RTL_W8(BCN_CTRL, EN_BCN_FUNCTION | DIS_SUB_STATE_N | DIS_TSF_UPDATE_N|EN_TXBCN_RPT);

#if defined(CONFIG_RTL_92E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT) //eric_8812 ??
		if(GET_CHIP_VER(priv)==VERSION_8192E || GET_CHIP_VER(priv)==VERSION_8812E || GET_CHIP_VER(priv)==VERSION_8188E)
			RTL_W8(MBID_NUM, (RTL_R8(MBID_NUM) & ~MBID_BCN_NUM_Mask) | (nr_vap & MBID_BCN_NUM_Mask));
		else
#endif
			RTL_W8(MBID_NUM, nr_vap & MBID_BCN_NUM_Mask);
		
		RTL_W32(RCR, RTL_R32(RCR) & ~RCR_MBID_EN);
		RTL_W32(RCR, RTL_R32(RCR) | RCR_MBID_EN);	// MBSSID enable
	}
	
	SMP_UNLOCK_MBSSID(&irqL);
}

void rtl8192cd_stop_mbssid(struct rtl8192cd_priv *priv)
{
	int i, j;
	unsigned int camData[2];
	unsigned int vap_bcn_offset;
	int nr_vap;
	_irqL irqL;
	
	camData[1] = 0;
	
	SMP_LOCK_MBSSID(&irqL);

	if (IS_ROOT_INTERFACE(priv))
	{
		// clear the rest area of CAM
		for (i=0; i<8; i++) {
			camData[0] = MBIDCAM_POLL | MBIDWRITE_EN | (i&MBIDCAM_ADDR_Mask)<<MBIDCAM_ADDR_SHIFT;
			for (j=1; j>=0; j--) {
				RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
			}
//			RTL_W8(_MBIDCAMCFG_, BIT(7) | BIT(6) | (unsigned char)i);
		}
		
		priv->pshare->nr_vap_bcn = 0;
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		priv->pshare->bcn_priv[0] = priv;
		priv->pshare->inter_bcn_space = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * NET80211_TU_TO_US;
#endif

//		set_fw_reg(priv, 0xf1000001, 0, 0);
		RTL_W32(RCR, RTL_R32(RCR) & ~RCR_MBID_EN);	// MBSSID disable
		RTL_W32(MBSSID_BCN_SPACE,
			(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BCN_SPACE1_Mask)<<BCN_SPACE1_SHIFT);

		RTL_W8(BCN_CTRL, 0);
		RTL_W8(0x553, 1);
#ifdef CONFIG_RTL_92C_SUPPORT
		if (IS_TEST_CHIP(priv))
			RTL_W8(BCN_CTRL, EN_BCN_FUNCTION | DIS_SUB_STATE | DIS_TSF_UPDATE| EN_TXBCN_RPT);
		else
#endif
			RTL_W8(BCN_CTRL, EN_BCN_FUNCTION | DIS_SUB_STATE_N | DIS_TSF_UPDATE_N| EN_TXBCN_RPT);

	}
	else if (IS_VAP_INTERFACE(priv) && (priv->vap_init_seq >= 0))
	{
//		set_fw_reg(priv, 0xf1000001 | (((RTL_R8(_MBIDCTRL_) & (BIT(4) | BIT(5) | BIT(6))) >> 4)&0xffff)<<8, 0, 0);
		camData[0] = MBIDCAM_POLL | MBIDWRITE_EN |
			(priv->vap_init_seq&MBIDCAM_ADDR_Mask)<<MBIDCAM_ADDR_SHIFT;
		for (j=1; j>=0; j--) {
			RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
		}
//		RTL_W8(_MBIDCAMCFG_, BIT(7) | BIT(6) | ((unsigned char)priv->vap_init_seq & 0x1f));
		
		if (priv->pshare->nr_vap_bcn) {
			nr_vap = --priv->pshare->nr_vap_bcn;
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			for (i = 0; i <= nr_vap; ++i) {
				if (priv->pshare->bcn_priv[i] == priv) {
					priv->pshare->bcn_priv[i] = priv->pshare->bcn_priv[nr_vap+1];
					break;
				}
			}
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI
			
#if defined(CONFIG_RTL_92E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT) //eric_8812 ??
			if(GET_CHIP_VER(priv)==VERSION_8192E || GET_CHIP_VER(priv)==VERSION_8812E || GET_CHIP_VER(priv)==VERSION_8188E)
				RTL_W8(MBID_NUM, (RTL_R8(MBID_NUM) & ~MBID_BCN_NUM_Mask) | (nr_vap & MBID_BCN_NUM_Mask));
			else
#endif
				RTL_W8(MBID_NUM, nr_vap & MBID_BCN_NUM_Mask);
			
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv)==VERSION_8188E) {
				vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod-
					((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod*nr_vap)/(nr_vap+1))-1;

				if (vap_bcn_offset > 200)
					vap_bcn_offset = 200;
				RTL_W32(MBSSID_BCN_SPACE, (vap_bcn_offset & BCN_SPACE2_Mask)<<BCN_SPACE2_SHIFT
					|(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BCN_SPACE1_Mask)<<BCN_SPACE1_SHIFT);
			} else
#endif
#if defined(CONFIG_RTL_92E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT)
			if ((GET_CHIP_VER(priv)==VERSION_8192E) || (GET_CHIP_VER(priv)==VERSION_8812E)) {
				vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod-
					((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod*nr_vap)/(nr_vap+1))-1;

				if (vap_bcn_offset > 200)
					vap_bcn_offset = 200;
				RTL_W32(MBSSID_BCN_SPACE, (vap_bcn_offset & BCN_SPACE2_Mask)<<BCN_SPACE2_SHIFT
					|((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod/(nr_vap+1)) & BCN_SPACE1_Mask)
					<<BCN_SPACE1_SHIFT);
			} else
#endif
			{
				vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod-
					((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod*nr_vap)/(nr_vap+1));
				
				RTL_W32(MBSSID_BCN_SPACE, ((vap_bcn_offset & BCN_SPACE2_Mask) << BCN_SPACE2_SHIFT)
					|((priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod/(nr_vap+1)) & BCN_SPACE1_Mask)
					<<BCN_SPACE1_SHIFT);
			}
			
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			priv->pshare->inter_bcn_space = vap_bcn_offset * NET80211_TU_TO_US;
#endif
			
			RTL_W8(BCN_CTRL, 0);
			RTL_W8(0x553, 1);
#ifdef CONFIG_RTL_92C_SUPPORT
			if (IS_TEST_CHIP(priv))
				RTL_W8(BCN_CTRL, EN_BCN_FUNCTION | DIS_SUB_STATE | DIS_TSF_UPDATE| EN_TXBCN_RPT);
			else
#endif
				RTL_W8(BCN_CTRL, EN_BCN_FUNCTION | DIS_SUB_STATE_N | DIS_TSF_UPDATE_N| EN_TXBCN_RPT);

		}
		RTL_W32(RCR, RTL_R32(RCR) & ~RCR_MBID_EN);
		RTL_W32(RCR, RTL_R32(RCR) | RCR_MBID_EN);
		priv->vap_init_seq = -1;
	}
	
	SMP_UNLOCK_MBSSID(&irqL);
}
#endif // MBSSID

