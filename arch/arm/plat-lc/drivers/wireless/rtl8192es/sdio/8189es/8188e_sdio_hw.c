/*
 *  SDIO core routines
 *
 *  $Id: 8188e_sdio_hw.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8188E_SDIO_HW_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"
#include "Hal8188EPwrSeq.h"


extern struct _device_info_ wlan_device[];
extern int drv_registered;


extern int MDL_DEVINIT rtl8192cd_init_one(struct sdio_func *psdio_func,
		const struct sdio_device_id *ent, struct _device_info_ *wdev, int vap_idx);
extern void rtl8192cd_deinit_one(struct rtl8192cd_priv *priv);


static int MDL_DEVINIT rtw_drv_init(struct sdio_func *psdio_func, const struct sdio_device_id *pdid)
{
	int ret;
#ifdef MBSSID
	int i;
#endif
	printk("%s: sdio_func_id is \"%s\"\n", __func__, sdio_func_id(psdio_func));
	
	ret = rtl8192cd_init_one(psdio_func, pdid, &wlan_device[0], -1);
	if (ret)
		goto error;
	
#ifdef UNIVERSAL_REPEATER
	ret = rtl8192cd_init_one(psdio_func, pdid, &wlan_device[0], -1);
	if (ret)
		goto error;
#endif

#ifdef MBSSID
	for (i = 0; i < RTL8192CD_NUM_VWLAN; i++) {
		ret = rtl8192cd_init_one(psdio_func, pdid, &wlan_device[0], i);
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

	return ret;
}

static void MDL_DEVEXIT rtw_dev_remove(struct sdio_func *psdio_func)
{
	struct net_device *dev = sdio_get_drvdata(psdio_func);
#ifdef NETDEV_NO_PRIV
	struct rtl8192cd_priv *priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	struct rtl8192cd_priv *priv = dev->priv;
#endif
	printk("%s: sdio_func_id is \"%s\"\n", __func__, sdio_func_id(psdio_func));
	
	sdio_set_drvdata(psdio_func, NULL);

	if (priv) {
		priv->pshare->bDriverStopped = TRUE;
		if (TRUE == drv_registered)
			priv->pshare->bSurpriseRemoved = TRUE;
	}

	if (NULL != wlan_device[0].priv) {
		rtl8192cd_deinit_one(wlan_device[0].priv);
		wlan_device[0].priv = NULL;
	}
}

static const struct sdio_device_id rtw_sdio_id_tbl[] = {
#ifdef CONFIG_RTL_88E_SUPPORT
	{ SDIO_DEVICE(0x024c, 0x8179) },
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
	{ SDIO_DEVICE(0x024c, 0x818b) },
#endif
	{}	/* Terminating entry */
};

MODULE_DEVICE_TABLE(sdio, rtw_sdio_id_tbl);

struct sdio_driver rtl8192cd_sdio_driver = {
	.name = (char*)DRV_NAME,
	.id_table = rtw_sdio_id_tbl,
	.probe = rtw_drv_init,
	.remove = rtw_dev_remove,
};


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


int rtl8192cd_stop_hw(struct rtl8192cd_priv *priv)
{
#ifdef TXREPORT
	RTL8188E_DisableTxReport(priv);
#endif
	
	RTL_W8(RCR, 0);
	RTL_W8(TXPAUSE, 0xff);				// Pause MAC TX queue
	RTL_W8(CR, 0x0);
	
	// Run LPS WL RFOFF flow
	if (FALSE == HalPwrSeqCmdParsing(priv, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
			PWR_INTF_SDIO_MSK, rtl8188E_enter_lps_flow)) {
		DEBUG_ERR("[%s %d] run RF OFF flow fail!\n", __FUNCTION__, __LINE__);
	}
	
	if (RTL_R8(MCUFWDL) & BIT(7)) { //8051 RAM code
		// Reset MCU 0x2[10]=0.
		RTL_W8(SYS_FUNC_EN+1, RTL_R8(SYS_FUNC_EN+1) & ~ BIT(2));
	}
	
	// MCUFWDL 0x80[1:0]=0
	// reset MCU ready status
	RTL_W8(MCUFWDL, 0);
	
	// Disable CMD53 R/W
	GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn = FALSE;
	
	// Card disable power action flow
	if (FALSE == HalPwrSeqCmdParsing(priv, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
			PWR_INTF_SDIO_MSK, rtl8188E_card_disable_flow)) {
		DEBUG_ERR("[%s %d] run CARD DISABLE flow fail!\n", __FUNCTION__, __LINE__);
	}
	
	// lock ISO/CLK/Power control register
	RTL_W8(RSV_CTRL0, 0x0e);
	
	return SUCCESS;
}


#ifdef CONFIG_EXT_CLK_26M
#define REG_APE_PLL_CTRL_EXT  0x002c
void _InitClockTo26MHz(struct rtl8192cd_priv *priv)
{
	u8 u1temp = 0;
	
	// EnableGpio5ClockReq(priv, FALSE, 1);

	u1temp = RTL_R8(REG_APE_PLL_CTRL_EXT);
	u1temp = (u1temp & 0xF0) | 0x05;
	RTL_W8(REG_APE_PLL_CTRL_EXT, u1temp); 

	printk("acli: set 26M\n");
}
#endif

/*
 * Description:
 *	Call power on sequence to enable card
 */
u8 _CardEnable(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

#ifdef CONFIG_EXT_CLK_26M
	u8 val8;
#endif
	
	if (FALSE == pHalData->bMacPwrCtrlOn)
	{
		// RSV_CTRL 0x1C[7:0] = 0x00
		// unlock ISO/CLK/Power control register
		RTL_W8(REG_RSV_CTRL, 0x0);

#ifdef CONFIG_EXT_CLK_26M
		// _InitClockTo26MHz(priv);
		val8 =  RTL_R8(0x4); // APS_FSMCO
		val8 = val8 & ~BIT(5);
		RTL_W8(0x4, val8);
#endif

		if (FALSE == HalPwrSeqCmdParsing(priv, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
				PWR_INTF_SDIO_MSK, rtl8188E_card_enable_flow)) 	{
			printk(KERN_ERR "%s: run power on flow fail\n", __func__);
			return FAIL;
		}
		
		pHalData->bMacPwrCtrlOn = TRUE;
	}
	
	return SUCCESS;
}

int rtl8188es_InitPowerOn(struct rtl8192cd_priv *priv)
{
	u16 value16;

	if (_CardEnable(priv) == FAIL) {
		return FAIL;
	}

	// Enable power down and GPIO interrupt
	value16 = RTL_R16(REG_APS_FSMCO);
	value16 |= PDN_EN; // Enable HW power down and RF on
	RTL_W16(REG_APS_FSMCO, value16);

	// Reset TX/RX DMA before LLT init to avoid TXDMA error "LLT_NULL_PG"
	RTL_W8(CR, 0x0);
	
	// Enable MAC DMA/WMAC/SCHEDULE/SEC block
	value16 = RTL_R16(CR);
	value16 |= (HCI_TXDMA_EN | HCI_RXDMA_EN | TXDMA_EN | RXDMA_EN
				| PROTOCOL_EN | SCHEDULE_EN | CALTMR_EN);
	// for SDIO - Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.

	RTL_W16(CR, value16);
	
	return SUCCESS;

}

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
		camData[0] = MBIDCAM_POLL | MBIDWRITE_EN | MBIDCAM_VALID | (macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		for (j=1; j>=0; j--) {
			RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
		}

		// clear the rest area of CAM
		camData[1] = 0;
		for (i=1; i<8; i++) {
			camData[0] = MBIDCAM_POLL | MBIDWRITE_EN | (i&MBIDCAM_ADDR_Mask)<<MBIDCAM_ADDR_SHIFT;
			for (j=1; j>=0; j--) {
				RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
			}
		}
		
		priv->pshare->nr_vap_bcn = 0;
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		priv->pshare->bcn_priv[0] = priv;
		priv->pshare->inter_bcn_space = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * NET80211_TU_TO_US;
#endif

		// set MBIDCTRL & MBID_BCN_SPACE by cmd
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
	}
	else if (IS_VAP_INTERFACE(priv))
	{
		priv->vap_init_seq = priv->vap_id +1;

		camData[0] = MBIDCAM_POLL | MBIDWRITE_EN | MBIDCAM_VALID |
				(priv->vap_init_seq&MBIDCAM_ADDR_Mask)<<MBIDCAM_ADDR_SHIFT |
				(macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		for (j=1; j>=0; j--) {
			RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
		}
		
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
		}
		
		priv->pshare->nr_vap_bcn = 0;
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		priv->pshare->bcn_priv[0] = priv;
		priv->pshare->inter_bcn_space = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * NET80211_TU_TO_US;
#endif

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
		camData[0] = MBIDCAM_POLL | MBIDWRITE_EN |
			(priv->vap_init_seq&MBIDCAM_ADDR_Mask)<<MBIDCAM_ADDR_SHIFT;
		for (j=1; j>=0; j--) {
			RTL_W32((MBIDCAMCFG+4)-4*j, camData[j]);
		}
		
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

