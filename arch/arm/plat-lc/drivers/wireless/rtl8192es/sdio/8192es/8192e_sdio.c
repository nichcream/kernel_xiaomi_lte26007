/*
 *  SDIO core routines
 *
 *  $Id: 8192e_sdio.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192E_SDIO_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"
#include "WlanHAL/HalHeader/HalComReg.h"
#ifdef REVO_MIFI
#include "../../../../../../arch/arm/mach-balong/mmi.h"
#endif

#define SDIO_ERR_VAL8	0xEA
#define SDIO_ERR_VAL16	0xEAEA
#define SDIO_ERR_VAL32	0xEAEAEAEA

const u32 reg_freepage_thres[SDIO_TX_FREE_PG_QUEUE] = {
	REG_92E_TQPNT1+1, REG_92E_TQPNT1+3, REG_92E_TQPNT2+1, 0, REG_92E_TQPNT2+3
};


u8 sd_f0_read8(struct rtl8192cd_priv *priv, u32 addr, s32 *err)
{
	u8 v;
	struct sdio_func *func;
	
	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	v = sdio_f0_readb(func, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

	return v;
}

void sd_f0_write8(struct rtl8192cd_priv *priv, u32 addr, u8 v, s32 *err)
{
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	sdio_f0_writeb(func, v, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, *err, addr, v);
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 _sd_cmd52_read(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pdata)
{
	int err, i;
	struct sdio_func *func;

	err = 0;
	func = priv->pshare->psdio_func;

	for (i = 0; i < cnt; i++) {
		pdata[i] = sdio_readb(func, addr+i, &err);
		if (err) {
			printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);
			break;
		}
	}

	return err;
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 sd_cmd52_read(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pdata)
{
	int err;
	struct sdio_func *func;

	err = 0;
	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	err = _sd_cmd52_read(priv, addr, cnt, pdata);
	sdio_release_host(func);

	return err;
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 _sd_cmd52_write(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pdata)
{
	int err, i;
	struct sdio_func *func;

	err = 0;
	func = priv->pshare->psdio_func;

	for (i = 0; i < cnt; i++) {
		sdio_writeb(func, pdata[i], addr+i, &err);
		if (err) {
			printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, err, addr, pdata[i]);
			break;
		}
	}

	return err;
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 sd_cmd52_write(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pdata)
{
	int err;
	struct sdio_func *func;

	err = 0;
	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	err = _sd_cmd52_write(priv, addr, cnt, pdata);
	sdio_release_host(func);

	return err;
}

u8 _sd_read8(struct rtl8192cd_priv *priv, u32 addr, s32 *err)
{
	u8 v;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	//sdio_claim_host(func);
	v = sdio_readb(func, addr, err);
	//sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

	return v;
}

u8 sd_read8(struct rtl8192cd_priv *priv, u32 addr, s32 *err)
{
	u8 v;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	v = sdio_readb(func, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

	return v;
}

u16 sd_read16(struct rtl8192cd_priv *priv, u32 addr, s32 *err)
{
	u16 v;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	v = sdio_readw(func, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

	return  v;
}

u32 _sd_read32(struct rtl8192cd_priv *priv, u32 addr, s32 *err)
{
	u32 v;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	//sdio_claim_host(func);
	v = sdio_readl(func, addr, err);
	//sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

	return  v;
}

u32 sd_read32(struct rtl8192cd_priv *priv, u32 addr, s32 *err)
{
	u32 v;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	v = sdio_readl(func, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

	return  v;
}

void sd_write8(struct rtl8192cd_priv *priv, u32 addr, u8 v, s32 *err)
{
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	sdio_writeb(func, v, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, *err, addr, v);
}

void sd_write16(struct rtl8192cd_priv *priv, u32 addr, u16 v, s32 *err)
{
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	sdio_writew(func, v, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%04x\n", __func__, *err, addr, v);
}

void _sd_write32(struct rtl8192cd_priv *priv, u32 addr, u32 v, s32 *err)
{
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	//sdio_claim_host(func);
	sdio_writel(func, v, addr, err);
	//sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%08x\n", __func__, *err, addr, v);
}

void sd_write32(struct rtl8192cd_priv *priv, u32 addr, u32 v, s32 *err)
{
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	sdio_writel(func, v, addr, err);
	sdio_release_host(func);
	if (err && *err)
		printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%08x\n", __func__, *err, addr, v);
}

/*
 * Use CMD53 to read data from SDIO device.
 * This function MUST be called after sdio_claim_host() or
 * in SDIO ISR(host had been claimed).
 *
 * Parameters:
 *	psdio	pointer of SDIO_DATA
 *	addr	address to read
 *	cnt		amount to read
 *	pdata	pointer to put data, this should be a "DMA:able scratch buffer"!
 *
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 _sd_read(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, void *pdata)
{
	int err;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	if (unlikely((cnt==1) || (cnt==2)))
	{
		int i;
		u8 *pbuf = (u8*)pdata;

		for (i = 0; i < cnt; i++)
		{
			*(pbuf+i) = sdio_readb(func, addr+i, &err);

			if (err) {
				printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);
				break;
			}
		}
		return err;
	}

	err = sdio_memcpy_fromio(func, pdata, addr, cnt);
	if (err) {
		printk(KERN_ERR "%s: FAIL(%d)! ADDR=%#x Size=%d\n", __func__, err, addr, cnt);
	}

	return err;
}

/*
 * Use CMD53 to read data from SDIO device.
 *
 * Parameters:
 *	psdio	pointer of SDIO_DATA
 *	addr	address to read
 *	cnt		amount to read
 *	pdata	pointer to put data, this should be a "DMA:able scratch buffer"!
 *
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 sd_read(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, void *pdata)
{
	s32 err;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	err = _sd_read(priv, addr, cnt, pdata);
	sdio_release_host(func);

	return err;
}

/*
 * Use CMD53 to write data to SDIO device.
 * This function MUST be called after sdio_claim_host() or
 * in SDIO ISR(host had been claimed).
 *
 * Parameters:
 *	psdio	pointer of SDIO_DATA
 *	addr	address to write
 *	cnt		amount to write
 *	pdata	data pointer, this should be a "DMA:able scratch buffer"!
 *
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 _sd_write(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, void *pdata)
{
	int err;
	struct sdio_func *func;
	u32 size;

	func = priv->pshare->psdio_func;
//	size = sdio_align_size(func, cnt);

	if (unlikely((cnt==1) || (cnt==2)))
	{
		int i;
		u8 *pbuf = (u8*)pdata;

		for (i = 0; i < cnt; i++)
		{
			sdio_writeb(func, *(pbuf+i), addr+i, &err);
			if (err) {
				printk(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, err, addr, *(pbuf+i));
				break;
			}
		}

		return err;
	}

	size = cnt;
	err = sdio_memcpy_toio(func, addr, pdata, size);
	if (err) {
		printk(KERN_ERR "%s: FAIL(%d)! ADDR=%#x Size=%d(%d)\n", __func__, err, addr, cnt, size);
	}
	
	return err;
}

/*
 * Use CMD53 to write data to SDIO device.
 *
 * Parameters:
 *  psdio	pointer of SDIO_DATA
 *  addr	address to write
 *  cnt		amount to write
 *  pdata	data pointer, this should be a "DMA:able scratch buffer"!
 *
 * Return:
 *  0		Success
 *  others	Fail
 */
s32 sd_write(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, void *pdata)
{
	s32 err;
	struct sdio_func *func;

	func = priv->pshare->psdio_func;

	sdio_claim_host(func);
	err = _sd_write(priv, addr, cnt, pdata);
	sdio_release_host(func);

	return err;
}

//
// Description:
//	The following mapping is for SDIO host local register space.
//
// Creadted by Roger, 2011.01.31.
//
static void HalSdioGetCmdAddr8723ASdio(struct rtl8192cd_priv *priv, u8 DeviceID, u32 Addr, u32* pCmdAddr)
{
	switch (DeviceID)
	{
		case SDIO_LOCAL_DEVICE_ID:
			*pCmdAddr = ((SDIO_LOCAL_DEVICE_ID << 13) | (Addr & SDIO_LOCAL_MSK));
			break;

		case WLAN_IOREG_DEVICE_ID:
			*pCmdAddr = ((WLAN_IOREG_DEVICE_ID << 13) | (Addr & WLAN_IOREG_MSK));
			break;

		case WLAN_TX_HIQ_DEVICE_ID:
			*pCmdAddr = ((WLAN_TX_HIQ_DEVICE_ID << 13) | (Addr & WLAN_FIFO_MSK));
			break;

		case WLAN_TX_MIQ_DEVICE_ID:
			*pCmdAddr = ((WLAN_TX_MIQ_DEVICE_ID << 13) | (Addr & WLAN_FIFO_MSK));
			break;

		case WLAN_TX_LOQ_DEVICE_ID:
			*pCmdAddr = ((WLAN_TX_LOQ_DEVICE_ID << 13) | (Addr & WLAN_FIFO_MSK));
			break;
			
		case WLAN_TX_EXQ_DEVICE_ID:
			// Note. TX_EXQ actually map to 0b[16], 111b[15:13]
			*pCmdAddr = ((WLAN_RX0FF_DEVICE_ID << 13) | (Addr & WLAN_FIFO_MSK));
			break;
			
		case WLAN_RX0FF_DEVICE_ID:
			*pCmdAddr = ((WLAN_RX0FF_DEVICE_ID << 13) | (Addr & WLAN_RX0FF_MSK));
			break;

		default:
			break;
	}
}

static u8 get_deviceid(u32 addr)
{
	u32 baseAddr;
	u8 devideId;

	baseAddr = addr & 0xffff0000;
	
	switch (baseAddr) {
	case SDIO_LOCAL_BASE:
		devideId = SDIO_LOCAL_DEVICE_ID;
		break;

	case WLAN_IOREG_BASE:
		devideId = WLAN_IOREG_DEVICE_ID;
		break;

//	case FIRMWARE_FIFO_BASE:
//		devideId = SDIO_FIRMWARE_FIFO;
//		break;

	case TX_HIQ_BASE:
		devideId = WLAN_TX_HIQ_DEVICE_ID;
		break;

	case TX_MIQ_BASE:
		devideId = WLAN_TX_MIQ_DEVICE_ID;
		break;

	case TX_LOQ_BASE:
		devideId = WLAN_TX_LOQ_DEVICE_ID;
		break;

#ifdef CONFIG_WLAN_HAL_8192EE
	case TX_EXQ_BASE:
		devideId = WLAN_TX_EXQ_DEVICE_ID;
		break;
#endif

	case RX_RX0FF_BASE:
		devideId = WLAN_RX0FF_DEVICE_ID;
		break;

	default:
//		devideId = (u8)((addr >> 13) & 0xF);
		devideId = WLAN_IOREG_DEVICE_ID;
		break;
	}

	return devideId;
}

/*
 * Ref:
 *	HalSdioGetCmdAddr8723ASdio()
 */
static u32 _cvrt2ftaddr(const u32 addr, u8 *pdeviceId, u16 *poffset)
{
	u8 deviceId;
	u16 offset;
	u32 ftaddr;


	deviceId = get_deviceid(addr);
	offset = 0;

	switch (deviceId)
	{
		case SDIO_LOCAL_DEVICE_ID:
			offset = addr & SDIO_LOCAL_MSK;
			break;

		case WLAN_TX_HIQ_DEVICE_ID:
		case WLAN_TX_MIQ_DEVICE_ID:
		case WLAN_TX_LOQ_DEVICE_ID:
#ifdef CONFIG_WLAN_HAL_8192EE
		case WLAN_TX_EXQ_DEVICE_ID:
#endif
			offset = addr & WLAN_FIFO_MSK;
			break;

		case WLAN_RX0FF_DEVICE_ID:
			offset = addr & WLAN_RX0FF_MSK;
			break;

		case WLAN_IOREG_DEVICE_ID:
		default:
			deviceId = WLAN_IOREG_DEVICE_ID;
			offset = addr & WLAN_IOREG_MSK;
			break;
	}
	ftaddr = (deviceId << 13) | offset;

	if (pdeviceId) *pdeviceId = deviceId;
	if (poffset) *poffset = offset;

	return ftaddr;
}

u8 _sdio_read8(struct rtl8192cd_priv *priv, u32 addr)
{
	u32 ftaddr;
	u8 val;
	
	ftaddr = _cvrt2ftaddr(addr, NULL, NULL);
	val = _sd_read8(priv, ftaddr, NULL);
	
	return val;
}

u8 sdio_read8(struct rtl8192cd_priv *priv, u32 addr)
{
	u32 ftaddr;
	u8 val;

	ftaddr = _cvrt2ftaddr(addr, NULL, NULL);
	val = sd_read8(priv, ftaddr, NULL);

	return val;
}

u16 sdio_read16(struct rtl8192cd_priv *priv, u32 addr)
{
	u32 ftaddr;
	u16 val;

	ftaddr = _cvrt2ftaddr(addr, NULL, NULL);
	sd_cmd52_read(priv, ftaddr, 2, (u8*)&val);
	val = le16_to_cpu(val);

	return val;
}

u32 _sdio_read32(struct rtl8192cd_priv *priv, u32 addr)
{
	u8 deviceId;
	u16 offset;
	u32 ftaddr;
	u8 shift;
	u32 val;
	s32 err;

	ftaddr = _cvrt2ftaddr(addr, &deviceId, &offset);

	if (((deviceId == WLAN_IOREG_DEVICE_ID) && (offset < 0x100))
		|| (FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
        {
		err = _sd_cmd52_read(priv, ftaddr, 4, (u8*)&val);
#ifdef SDIO_DEBUG_IO
		if (err) {
			printk(KERN_ERR "%s: Mac Power off, Read FAIL(%d)! addr=0x%x\n", __func__, err, addr);
			return SDIO_ERR_VAL32;
		}
#endif
		val = le32_to_cpu(val);
		return val;
	}

	// 4 bytes alignment
	shift = ftaddr & 0x3;
	if (shift == 0) {
		val = _sd_read32(priv, ftaddr, NULL);
	} else {
		u8 tmpbuf[8];

		ftaddr &= ~0x3;
		err = _sd_read(priv, ftaddr, 8, tmpbuf);
		memcpy(&val, tmpbuf+shift, 4);
		val = le32_to_cpu(val);
	}

	return val;
}

u32 sdio_read32(struct rtl8192cd_priv *priv, u32 addr)
{
	u8 deviceId;
	u16 offset;
	u32 ftaddr;
	u8 shift;
	u32 val;
	s32 err;
	
	ftaddr = _cvrt2ftaddr(addr, &deviceId, &offset);

	if (((deviceId == WLAN_IOREG_DEVICE_ID) && (offset < 0x100))
		|| (FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		err = sd_cmd52_read(priv, ftaddr, 4, (u8*)&val);
#ifdef SDIO_DEBUG_IO
		if (err) {
			printk(KERN_ERR "%s: Mac Power off, Read FAIL(%d)! addr=0x%x\n", __func__, err, addr);
			return SDIO_ERR_VAL32;
		}
#endif
		val = le32_to_cpu(val);
		return val;
	}

	// 4 bytes alignment
	shift = ftaddr & 0x3;
	if (shift == 0) {
		val = sd_read32(priv, ftaddr, NULL);
	} else {
		u8 tmpbuf[8];

		ftaddr &= ~0x3;
		err = sd_read(priv, ftaddr, 8, tmpbuf);
		memcpy(&val, tmpbuf+shift, 4);
		val = le32_to_cpu(val);
	}

	return val;
}

s32 sdio_readN(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pbuf)
{
	u8 deviceId;
	u16 offset;
	u32 ftaddr;
	u8 shift;
	s32 err;
	
	err = 0;
	ftaddr = _cvrt2ftaddr(addr, &deviceId, &offset);

	if (((deviceId == WLAN_IOREG_DEVICE_ID) && (offset < 0x100))
		|| (FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		err = sd_cmd52_read(priv, ftaddr, cnt, pbuf);
		return err;
	}

	// 4 bytes alignment
	shift = ftaddr & 0x3;
	if (shift == 0) {
		err = sd_read(priv, ftaddr, cnt, pbuf);
	} else {
		u8 *ptmpbuf;
		u32 n;

		ftaddr &= ~0x3;
		n = cnt + shift;
		ptmpbuf = rtw_malloc(n);
		if (NULL == ptmpbuf) return -ENOMEM;
		
		err = sd_read(priv, ftaddr, n, ptmpbuf);
		if (!err)
			memcpy(pbuf, ptmpbuf+shift, cnt);
		
		rtw_mfree(ptmpbuf, n);
	}

	return err;
}

s32 sdio_write8(struct rtl8192cd_priv *priv, u32 addr, u8 val)
{
	u32 ftaddr;
	s32 err;

	ftaddr = _cvrt2ftaddr(addr, NULL, NULL);
	sd_write8(priv, ftaddr, val, &err);

	return err;
}

s32 sdio_write16(struct rtl8192cd_priv *priv, u32 addr, u16 val)
{
	u32 ftaddr;
	s32 err;

	ftaddr = _cvrt2ftaddr(addr, NULL, NULL);
	val = cpu_to_le16(val);
	err = sd_cmd52_write(priv, ftaddr, 2, (u8*)&val);

	return err;
}

s32 _sdio_write32(struct rtl8192cd_priv *priv, u32 addr, u32 val)
{
	u8 deviceId;
	u16 offset;
	u32 ftaddr;
	u8 shift;
	s32 err;
	
	err = 0;
	ftaddr = _cvrt2ftaddr(addr, &deviceId, &offset);

	if (((deviceId == WLAN_IOREG_DEVICE_ID) && (offset < 0x100))
		|| (FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		val = cpu_to_le32(val);
		err = _sd_cmd52_write(priv, ftaddr, 4, (u8*)&val);
		return err;
	}

	// 4 bytes alignment
	shift = ftaddr & 0x3;
#if 1
	if (shift == 0)
	{
		_sd_write32(priv, ftaddr, val, &err);
	}
	else
	{
		val = cpu_to_le32(val);
		err = _sd_cmd52_write(priv, ftaddr, 4, (u8*)&val);
	}
#else
	if (shift == 0) {
		sd_write32(priv, ftaddr, val, &err);
	} else {
		u8 *ptmpbuf;

		ptmpbuf = (u8*)rtw_malloc(8);
		if (NULL == ptmpbuf) return -ENOMEM;

		ftaddr &= ~0x3;
		err = sd_read(priv, ftaddr, 8, ptmpbuf);
		if (err) {
			_rtw_mfree(ptmpbuf, 8);
			return err;
		}
		val = cpu_to_le32(val);
		memcpy(ptmpbuf+shift, &val, 4);
		err = sd_write(priv, ftaddr, 8, ptmpbuf);

		rtw_mfree(ptmpbuf, 8);
	}
#endif
	
	return err;
}

s32 sdio_write32(struct rtl8192cd_priv *priv, u32 addr, u32 val)
{
	u8 deviceId;
	u16 offset;
	u32 ftaddr;
	u8 shift;
	s32 err;
	
	err = 0;
	ftaddr = _cvrt2ftaddr(addr, &deviceId, &offset);

	if (((deviceId == WLAN_IOREG_DEVICE_ID) && (offset < 0x100))
		|| (FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		val = cpu_to_le32(val);
		err = sd_cmd52_write(priv, ftaddr, 4, (u8*)&val);
		return err;
	}

	// 4 bytes alignment
	shift = ftaddr & 0x3;
#if 1
	if (shift == 0)
	{
		sd_write32(priv, ftaddr, val, &err);
	}
	else
	{
		val = cpu_to_le32(val);
		err = sd_cmd52_write(priv, ftaddr, 4, (u8*)&val);
	}
#else
	if (shift == 0) {
		sd_write32(priv, ftaddr, val, &err);
	} else {
		u8 *ptmpbuf;

		ptmpbuf = (u8*)rtw_malloc(8);
		if (NULL == ptmpbuf) return -ENOMEM;

		ftaddr &= ~0x3;
		err = sd_read(priv, ftaddr, 8, ptmpbuf);
		if (err) {
			rtw_mfree(ptmpbuf, 8);
			return err;
		}
		val = cpu_to_le32(val);
		memcpy(ptmpbuf+shift, &val, 4);
		err = sd_write(priv, ftaddr, 8, ptmpbuf);

		rtw_mfree(ptmpbuf, 8);
	}
#endif

	return err;
}

s32 sdio_writeN(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8* pbuf)
{
	u8 deviceId;
	u16 offset;
	u32 ftaddr;
	u8 shift;
	s32 err;
	
	err = 0;
	ftaddr = _cvrt2ftaddr(addr, &deviceId, &offset);

	if (((deviceId == WLAN_IOREG_DEVICE_ID) && (offset < 0x100))
		|| (FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		err = sd_cmd52_write(priv, ftaddr, cnt, pbuf);
		return err;
	}

	shift = ftaddr & 0x3;
	if (shift == 0) {
		err = sd_write(priv, ftaddr, cnt, pbuf);
	} else {
		u8 *ptmpbuf;
		u32 n;

		ftaddr &= ~0x3;
		n = cnt + shift;
		ptmpbuf = rtw_malloc(n);
		if (NULL == ptmpbuf) return -ENOMEM;
		err = sd_read(priv, ftaddr, 4, ptmpbuf);
		if (err) {
			rtw_mfree(ptmpbuf, n);
			return err;
		}
		memcpy(ptmpbuf+shift, pbuf, cnt);
		err = sd_write(priv, ftaddr, n, ptmpbuf);
		rtw_mfree(ptmpbuf, n);
	}

	return err;
}

/*
 * Description:
 *	Read from RX FIFO
 *	Round read size to block size,
 *	and make sure data transfer will be done in one command.
 *
 * Parameters:
 *	pintfhdl	a pointer of intf_hdl
 *	addr		port ID
 *	cnt			size to read
 *	rmem		address to put data
 *
 * Return:
 *	= 0		Success
 *	!= 0		Fail
 */
u32 sdio_read_port(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *mem)
{
	struct priv_shared_info *pshare = priv->pshare;
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	s32 err;

	HalSdioGetCmdAddr8723ASdio(priv, addr, pHalData->SdioRxFIFOCnt++, &addr);


	cnt = _RND4(cnt);
	if (cnt > pshare->block_transfer_len)
		cnt = _RND(cnt, pshare->block_transfer_len);

//	cnt = sdio_align_size(cnt);

	err = _sd_read(priv, addr, cnt, mem);
	//err = sd_read(priv, addr, cnt, mem);
	
	return err;
}

/*
 * Description:
 *	Write to TX FIFO
 *	Align write size block size,
 *	and make sure data could be written in one command.
 *
 * Parameters:
 *	pintfhdl	a pointer of intf_hdl
 *	addr		port ID
 *	cnt			size to write
 *	wmem		data pointer to write
 *
 * Return:
 *	= 0		Success
 *	!= 0		Fail
 */
u32 sdio_write_port(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *mem)
{
	s32 err;
	struct priv_shared_info *pshare = priv->pshare;
	struct xmit_buf *xmitbuf = (struct xmit_buf *)mem;

	cnt = _RND4(cnt);
	HalSdioGetCmdAddr8723ASdio(priv, addr, cnt >> 2, &addr);

	if (cnt > pshare->block_transfer_len)
		cnt = _RND(cnt, pshare->block_transfer_len);
//	cnt = sdio_align_size(cnt);

	err = sd_write(priv, addr, cnt, xmitbuf->pkt_data);
	xmitbuf->status = err;

//	rtw_sctx_done_err(&xmitbuf->sctx,
//		err ? RTW_SCTX_DONE_WRITE_PORT_ERR : RTW_SCTX_DONE_SUCCESS);

	if (err) {
		printk("%s, error=%d\n", __func__, err);
	}
	
	return err;
}

/*
 * Todo: align address to 4 bytes.
 */
s32 _sdio_local_read(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pbuf)
{
	s32 err;
	u8 *ptmpbuf;
	u32 n;

	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);

	if ((FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
//		|| (_TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		err = _sd_cmd52_read(priv, addr, cnt, pbuf);
		return err;
	}

        n = _RND4(cnt);
	ptmpbuf = (u8*)rtw_malloc(n);
	if (NULL == ptmpbuf)
		return -ENOMEM;

	err = _sd_read(priv, addr, n, ptmpbuf);
	if (!err)
		memcpy(pbuf, ptmpbuf, cnt);

	rtw_mfree(ptmpbuf, n);

	return err;
}

/*
 * Todo: align address to 4 bytes.
 */
s32 sdio_local_read(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pbuf)
{
	s32 err;
	u8 *ptmpbuf;
	u32 n;

	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);

	if ((FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		err = sd_cmd52_read(priv, addr, cnt, pbuf);
		return err;
	}

        n = _RND4(cnt);
	ptmpbuf = (u8*)rtw_malloc(n);
	if (NULL == ptmpbuf)
		return -ENOMEM;

	err = sd_read(priv, addr, n, ptmpbuf);
	if (!err)
		memcpy(pbuf, ptmpbuf, cnt);

	rtw_mfree(ptmpbuf, n);

	return err;
}

/*
 * Todo: align address to 4 bytes.
 */
s32 _sdio_local_write(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pbuf)
{
	s32 err;
	u8 *ptmpbuf;

	if (addr & 0x3)
		printk("%s, address must be 4 bytes alignment\n", __FUNCTION__);

	if (cnt & 0x3)
		printk("%s, size must be the multiple of 4 \n", __FUNCTION__);

	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);

	if ((FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
//		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		err = _sd_cmd52_write(priv, addr, cnt, pbuf);
		return err;
	}

        ptmpbuf = (u8*)rtw_malloc(cnt);
	if (NULL == ptmpbuf)
		return -ENOMEM;

	memcpy(ptmpbuf, pbuf, cnt);

	err = _sd_write(priv, addr, cnt, ptmpbuf);

	rtw_mfree(ptmpbuf, cnt);

	return err;
}

/*
 * Todo: align address to 4 bytes.
 */
s32 sdio_local_write(struct rtl8192cd_priv *priv, u32 addr, u32 cnt, u8 *pbuf)
{
	s32 err;
	u8 *ptmpbuf;

	if (addr & 0x3)
		printk("%s, address must be 4 bytes alignment\n", __FUNCTION__);

	if (cnt & 0x3)
		printk("%s, size must be the multiple of 4 \n", __FUNCTION__);

	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);

	if ((FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		err = sd_cmd52_write(priv, addr, cnt, pbuf);
		return err;
	}

        ptmpbuf = (u8*)rtw_malloc(cnt);
	if (NULL == ptmpbuf)
		return -ENOMEM;

	memcpy(ptmpbuf, pbuf, cnt);

	err = sd_write(priv, addr, cnt, ptmpbuf);

	rtw_mfree(ptmpbuf, cnt);

	return err;
}

u8 SdioLocalCmd52Read1Byte(struct rtl8192cd_priv *priv, u32 addr)
{
	u8 val = 0;

	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);
	sd_cmd52_read(priv, addr, 1, &val);

	return val;
}

u16 SdioLocalCmd52Read2Byte(struct rtl8192cd_priv *priv, u32 addr)
{
	u16 val = 0;

	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);
	sd_cmd52_read(priv, addr, 2, (u8*)&val);

	val = le16_to_cpu(val);

	return val;
}

u32 SdioLocalCmd52Read4Byte(struct rtl8192cd_priv *priv, u32 addr)
{
	u32 val = 0;

	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);
	sd_cmd52_read(priv, addr, 4, (u8*)&val);

	val = le32_to_cpu(val);

	return val;
}

u32 SdioLocalCmd53Read4Byte(struct rtl8192cd_priv *priv, u32 addr)
{
	u32 val;

	val = 0;
	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);
	if ((FALSE == GET_HAL_INTF_DATA(priv)->bMacPwrCtrlOn)
#ifdef CONFIG_LPS_LCLK
		|| (TRUE == padapter->pwrctrlpriv.bFwCurrentInPSMode)
#endif
		)
	{
		sd_cmd52_read(priv, addr, 4, (u8*)&val);
		val = le32_to_cpu(val);
	}
	else
		val = sd_read32(priv, addr, NULL);

	return val;
}

void SdioLocalCmd52Write1Byte(struct rtl8192cd_priv *priv, u32 addr, u8 v)
{
	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);
	sd_cmd52_write(priv, addr, 1, &v);
}

void SdioLocalCmd52Write2Byte(struct rtl8192cd_priv *priv, u32 addr, u16 v)
{
	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);
	v = cpu_to_le16(v);
	sd_cmd52_write(priv, addr, 2, (u8*)&v);
}

void SdioLocalCmd52Write4Byte(struct rtl8192cd_priv *priv, u32 addr, u32 v)
{
	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);
	v = cpu_to_le32(v);
	sd_cmd52_write(priv, addr, 4, (u8*)&v);
}


/*
 *
 *    HAL
 *
 */
 
void InitSdioInterrupt(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData;
	
	pHalData = GET_HAL_INTF_DATA(priv);
	pHalData->sdio_himr = SDIO_HIMR_RX_REQUEST_MSK
#ifdef CONFIG_SDIO_TX_INTERRUPT
			| SDIO_HIMR_AVAL_MSK
#endif
			;
}
 
void EnableSdioInterrupt(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData;
	u32 himr;

	pHalData = GET_HAL_INTF_DATA(priv);
	himr = cpu_to_le32(pHalData->sdio_himr);
	sdio_local_write(priv, SDIO_REG_HIMR, 4, (u8*)&himr);

	//
	// <Roger_Notes> There are some C2H CMDs have been sent before system interrupt is enabled, e.g., C2H, CPWM.
	// So we need to clear all C2H events that FW has notified, otherwise FW won't schedule any commands anymore.
	// 2011.10.19.
	//
	RTL_W8(C2H_SYNC_BYTE, C2H_EVT_HOST_CLOSE);
}

void DisableSdioInterrupt(struct rtl8192cd_priv *priv)
{
	u32 himr;

	himr = cpu_to_le32(SDIO_HIMR_DISABLED);
	sdio_local_write(priv, SDIO_REG_HIMR, 4, (u8*)&himr);
}

void rtl8192es_interface_configure(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);

	// The following settings must match TRX DMA Queue Mapping (Reg0x10C)

#if (TXDMA_VOQ_MAP_SEL == TXDMA_MAP_EXTRA)
	pHalData->Queue2Pipe[VO_QUEUE] = WLAN_TX_EXQ_DEVICE_ID;
#elif (TXDMA_VOQ_MAP_SEL == TXDMA_MAP_LOW)
	pHalData->Queue2Pipe[VO_QUEUE] = WLAN_TX_LOQ_DEVICE_ID;
#elif (TXDMA_VOQ_MAP_SEL == TXDMA_MAP_NORMAL)
	pHalData->Queue2Pipe[VO_QUEUE] = WLAN_TX_MIQ_DEVICE_ID;
#elif (TXDMA_VOQ_MAP_SEL == TXDMA_MAP_HIGH)
	pHalData->Queue2Pipe[VO_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;
#endif

#if (TXDMA_VIQ_MAP_SEL == TXDMA_MAP_EXTRA)
	pHalData->Queue2Pipe[VI_QUEUE] = WLAN_TX_EXQ_DEVICE_ID;
#elif (TXDMA_VIQ_MAP_SEL == TXDMA_MAP_LOW)
	pHalData->Queue2Pipe[VI_QUEUE] = WLAN_TX_LOQ_DEVICE_ID;
#elif (TXDMA_VIQ_MAP_SEL == TXDMA_MAP_NORMAL)
	pHalData->Queue2Pipe[VI_QUEUE] = WLAN_TX_MIQ_DEVICE_ID;
#elif (TXDMA_VIQ_MAP_SEL == TXDMA_MAP_HIGH)
	pHalData->Queue2Pipe[VI_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;
#endif

#if (TXDMA_BEQ_MAP_SEL == TXDMA_MAP_EXTRA)
	pHalData->Queue2Pipe[BE_QUEUE] = WLAN_TX_EXQ_DEVICE_ID;
#elif (TXDMA_BEQ_MAP_SEL == TXDMA_MAP_LOW)
	pHalData->Queue2Pipe[BE_QUEUE] = WLAN_TX_LOQ_DEVICE_ID;
#elif (TXDMA_BEQ_MAP_SEL == TXDMA_MAP_NORMAL)
	pHalData->Queue2Pipe[BE_QUEUE] = WLAN_TX_MIQ_DEVICE_ID;
#elif (TXDMA_BEQ_MAP_SEL == TXDMA_MAP_HIGH)
	pHalData->Queue2Pipe[BE_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;
#endif

#if (TXDMA_BKQ_MAP_SEL == TXDMA_MAP_EXTRA)
	pHalData->Queue2Pipe[BK_QUEUE] = WLAN_TX_EXQ_DEVICE_ID;
#elif (TXDMA_BKQ_MAP_SEL == TXDMA_MAP_LOW)
	pHalData->Queue2Pipe[BK_QUEUE] = WLAN_TX_LOQ_DEVICE_ID;
#elif (TXDMA_BKQ_MAP_SEL == TXDMA_MAP_NORMAL)
	pHalData->Queue2Pipe[BK_QUEUE] = WLAN_TX_MIQ_DEVICE_ID;
#elif (TXDMA_BKQ_MAP_SEL == TXDMA_MAP_HIGH)
	pHalData->Queue2Pipe[BK_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;
#endif

	pHalData->Queue2Pipe[BEACON_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;

#if (TXDMA_MGQ_MAP_SEL == TXDMA_MAP_EXTRA)
	pHalData->Queue2Pipe[MGNT_QUEUE] = WLAN_TX_EXQ_DEVICE_ID;
#elif (TXDMA_MGQ_MAP_SEL == TXDMA_MAP_LOW)
	pHalData->Queue2Pipe[MGNT_QUEUE] = WLAN_TX_LOQ_DEVICE_ID;
#elif (TXDMA_MGQ_MAP_SEL == TXDMA_MAP_NORMAL)
	pHalData->Queue2Pipe[MGNT_QUEUE] = WLAN_TX_MIQ_DEVICE_ID;
#elif (TXDMA_MGQ_MAP_SEL == TXDMA_MAP_HIGH)
	pHalData->Queue2Pipe[MGNT_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;
#endif

#if (TXDMA_HIQ_MAP_SEL == TXDMA_MAP_EXTRA)
	pHalData->Queue2Pipe[HIGH_QUEUE] = WLAN_TX_EXQ_DEVICE_ID;
#elif (TXDMA_HIQ_MAP_SEL == TXDMA_MAP_LOW)
	pHalData->Queue2Pipe[HIGH_QUEUE] = WLAN_TX_LOQ_DEVICE_ID;
#elif (TXDMA_HIQ_MAP_SEL == TXDMA_MAP_NORMAL)
	pHalData->Queue2Pipe[HIGH_QUEUE] = WLAN_TX_MIQ_DEVICE_ID;
#elif (TXDMA_HIQ_MAP_SEL == TXDMA_MAP_HIGH)
	pHalData->Queue2Pipe[HIGH_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;
#endif

#if (TXDMA_CMQ_MAP_SEL == TXDMA_MAP_EXTRA)
	pHalData->Queue2Pipe[TXCMD_QUEUE] = WLAN_TX_EXQ_DEVICE_ID;
#elif (TXDMA_CMQ_MAP_SEL == TXDMA_MAP_LOW)
	pHalData->Queue2Pipe[TXCMD_QUEUE] = WLAN_TX_LOQ_DEVICE_ID;
#elif (TXDMA_CMQ_MAP_SEL == TXDMA_MAP_NORMAL)
	pHalData->Queue2Pipe[TXCMD_QUEUE] = WLAN_TX_MIQ_DEVICE_ID;
#elif (TXDMA_CMQ_MAP_SEL == TXDMA_MAP_HIGH)
	pHalData->Queue2Pipe[TXCMD_QUEUE] = WLAN_TX_HIQ_DEVICE_ID;
#endif
}

#ifdef CONFIG_SDIO_TX_IN_INTERRUPT
s32 sd_tx_rx_hdl(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	struct priv_shared_info *pshare = priv->pshare;
	
	struct recv_buf *precvbuf = NULL;
	s32 err = 0;
	int nr_recvbuf = 0;
	int nr_xmitbuf = 0;
	int nr_handle_recvbuf = 0;
	int loop;
	int handle_tx;
	u16 oqtFreeSpace;
	
	do {
		if ((TRUE == pshare->bDriverStopped) || (TRUE == pshare->bSurpriseRemoved))
			break;

		tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
		
		handle_tx = 0;
		
		//Sometimes rx length will be zero. driver need to use cmd53 read again.
		if (pHalData->SdioRxFIFOSize == 0)
		{
			u32 buf[2];
			u8* data = (u8*) buf;
			
			if (pHalData->SdioTxIntStatus & BIT(SDIO_TX_INT_SETUP_TH)) {
				u32 sdio_hisr;
				
				err = _sdio_local_read(priv, SDIO_REG_HISR, 8, data);
				if (err)
					break;

				sdio_hisr = le32_to_cpu(*(u32*)data);
				pHalData->SdioRxFIFOSize = le16_to_cpu(*(u16*)&data[4]);
				
				if (sdio_hisr & SDIO_HISR_AVAL)
				{
					oqtFreeSpace = le16_to_cpu(*(u16*)&data[6]);
					memcpy(pHalData->SdioTxOQTFreeSpace, &oqtFreeSpace, MAX_TXOQT_TYPE);
					
					if (test_and_clear_bit(SDIO_TX_INT_SETUP_TH, &pHalData->SdioTxIntStatus)) {
						set_bit(SDIO_TX_INT_WORKING, &pHalData->SdioTxIntStatus);
						
						// Invalidate TX Free Page Threshold
						RTL_W8(reg_freepage_thres[pHalData->SdioTxIntQIdx], 0xFF);
						
						HalQueryTxBufferStatus8192ESdio2(priv);
						
						for (loop = priv->pmib->miscEntry.max_handle_xmitbuf; loop > 0; --loop) {
							if (rtl8192es_dequeue_writeport(priv, SDIO_TX_ISR) == FAIL)
								break;
							++nr_xmitbuf;
						}
						++handle_tx;
					}
				}
			} else {
				err = _sdio_local_read(priv, SDIO_REG_RX0_REQ_LEN, 4, data);
				if (err)
					break;

				pHalData->SdioRxFIFOSize = le16_to_cpu(*(u16*)data);
				
				if ((0 == pHalData->SdioRxFIFOSize) || (nr_handle_recvbuf >= priv->pmib->miscEntry.max_handle_recvbuf))
				if ((pHalData->SdioTxIntStatus & BIT(SDIO_TX_INT_WORKING))
						&& (_rtw_queue_empty(&pshare->pending_xmitbuf_queue) == FALSE))
				{
					oqtFreeSpace = le16_to_cpu(*(u16*)&data[2]);
					memcpy(pHalData->SdioTxOQTFreeSpace, &oqtFreeSpace, MAX_TXOQT_TYPE);
					
					for (loop = priv->pmib->miscEntry.max_handle_xmitbuf; loop > 0; --loop) {
						if (rtl8192es_dequeue_writeport(priv, SDIO_RX_ISR) == FAIL)
							break;
						++nr_xmitbuf;
					}
					++handle_tx;
					nr_handle_recvbuf = 0;
				}
			}
		}

		if (pHalData->SdioRxFIFOSize)
		{
			precvbuf = sd_recv_rxfifo(priv, pHalData->SdioRxFIFOSize);
			
			if (precvbuf) {
				nr_recvbuf++;
				nr_handle_recvbuf++;
				sd_rxhandler(priv, precvbuf);
			}
			
			pHalData->SdioRxFIFOSize = 0;
		} else if (0 == handle_tx) {
			break;
		}
	} while (1);
	
	if (test_and_clear_bit(SDIO_TX_INT_WORKING, &pHalData->SdioTxIntStatus)
			&& !(pHalData->SdioTxIntStatus & BIT(SDIO_TX_INT_SETUP_TH))) {
		if (_rtw_queue_empty(&pshare->pending_xmitbuf_queue) == FALSE) {
			if (test_and_set_bit(WAKE_EVENT_XMIT, &pshare->wake_event) == 0)
				wake_up_process(pshare->xmit_thread);
		}
	}
	
	priv->pshare->nr_recvbuf_handled_in_irq = nr_recvbuf;
	priv->pshare->nr_xmitbuf_handled_in_irq += nr_xmitbuf;
	
	return err;
}

#else // !CONFIG_SDIO_TX_IN_INTERRUPT
s32 sd_rx_request_hdl(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	struct priv_shared_info *pshare = priv->pshare;
	
	struct recv_buf *precvbuf = NULL;
	s32 err = 0;
	int nr_recvbuf = 0;
	int nr_handle_recvbuf = 0;
	
	do {
		if ((TRUE == pshare->bDriverStopped) || (TRUE == pshare->bSurpriseRemoved))
			break;
		
		//Sometimes rx length will be zero. driver need to use cmd53 read again.
		if (pHalData->SdioRxFIFOSize == 0)
		{
			u32 buf[1];
			u8* data = (u8*) buf;
			
			err = _sdio_local_read(priv, SDIO_REG_RX0_REQ_LEN, 4, data);
			if (err)
				break;

			pHalData->SdioRxFIFOSize = le16_to_cpu(*(u16*)data);
		}

		if (pHalData->SdioRxFIFOSize)
		{
			precvbuf = sd_recv_rxfifo(priv, pHalData->SdioRxFIFOSize);
			
			if (precvbuf) {
				nr_recvbuf++;
				sd_rxhandler(priv, precvbuf);
			}
			
			pHalData->SdioRxFIFOSize = 0;
		} else {
			break;
		}
	} while (1);
	
	priv->pshare->nr_recvbuf_handled_in_irq = nr_recvbuf;
	
	return err;
}
#endif // CONFIG_SDIO_TX_IN_INTERRUPT

void sd_int_dpc(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	struct priv_shared_info *pshare = priv->pshare;

	if (pHalData->sdio_hisr & SDIO_HISR_CPWM1)
	{
		unsigned char state;

		_sdio_local_read(priv, SDIO_REG_HCPWM1, 1, &state);
	}

	if (pHalData->sdio_hisr & SDIO_HISR_TXERR)
	{
		u8 *status;
		u32 addr;

		status = _rtw_malloc(4);
		if (status)
		{
			addr = TXDMA_STATUS;
			HalSdioGetCmdAddr8723ASdio(priv, WLAN_IOREG_DEVICE_ID, addr, &addr);
			_sd_read(priv, addr, 4, status);
			_sd_write(priv, addr, 4, status);
			printk("%s: SDIO_HISR_TXERR (0x%08x)\n", __func__, le32_to_cpu(*(u32*)status));
			priv->pshare->tx_dma_err++;
			_rtw_mfree(status, 4);
		} else {
			printk("%s: SDIO_HISR_TXERR, but can't allocate memory to read status!\n", __func__);
		}
	}

#ifdef CONFIG_INTERRUPT_BASED_TXBCN

	if (pHalData->sdio_hisr & SDIO_HISR_BCNERLY_INT)
	{
		if (priv->timoffset)
			update_beacon(priv);
	}
	
	#ifdef  CONFIG_INTERRUPT_BASED_TXBCN_BCN_OK_ERR
	if (pHalData->sdio_hisr & (SDIO_HISR_TXBCNOK|SDIO_HISR_TXBCNERR))
	{
	}
	#endif
#endif //CONFIG_INTERRUPT_BASED_TXBCN

	if (pHalData->sdio_hisr & SDIO_HISR_C2HCMD)
	{
		printk("%s: C2H Command\n", __func__);
	}

#ifdef CONFIG_SDIO_TX_IN_INTERRUPT
	if (pHalData->sdio_hisr & (SDIO_HISR_AVAL|SDIO_HISR_RX_REQUEST))
	{
		if (sd_tx_rx_hdl(priv) == -ENOMEDIUM) {
			pshare->bSurpriseRemoved = TRUE;
			return;
		}
	}

#else // !CONFIG_SDIO_TX_IN_INTERRUPT
#ifdef CONFIG_SDIO_TX_INTERRUPT
	if (pHalData->sdio_hisr & SDIO_HISR_AVAL)
	{
		if (test_and_clear_bit(SDIO_TX_INT_SETUP_TH, &pHalData->SdioTxIntStatus)) {
			// Invalidate TX Free Page Threshold
			RTL_W8(reg_freepage_thres[pHalData->SdioTxIntQIdx], 0xFF);
			
			if (test_and_set_bit(WAKE_EVENT_XMIT, &pshare->wake_event) == 0)
				wake_up_process(pshare->xmit_thread);
		}
	}
#endif
	
	if (pHalData->sdio_hisr & SDIO_HISR_RX_REQUEST)
	{
		pHalData->sdio_hisr ^= SDIO_HISR_RX_REQUEST;
		
		if (sd_rx_request_hdl(priv) == -ENOMEDIUM) {
			pshare->bSurpriseRemoved = TRUE;
			return;
		}
	}
#endif // CONFIG_SDIO_TX_IN_INTERRUPT
}

static void sd_sync_int_hdl(struct sdio_func *func)
{
	struct net_device *dev;
	struct rtl8192cd_priv *priv;
	struct priv_shared_info *pshare;
	HAL_INTF_DATA_TYPE *pHalData;
	u8 data[6];
	
	dev = sdio_get_drvdata(func);
	// check if surprise removal occurs ? If yes, return right now.
	if (NULL == dev)
		return;
	
#ifdef NETDEV_NO_PRIV
	priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
	priv = dev->priv;
#endif
	pshare = priv->pshare;
	
	if ((TRUE == pshare->bDriverStopped) || (TRUE == pshare->bSurpriseRemoved))
		return;
	
	++pshare->nr_interrupt;

	if (_sdio_local_read(priv, SDIO_REG_HISR, 6, data))
		return;

	pHalData = GET_HAL_INTF_DATA(priv);
	pHalData->sdio_hisr = le32_to_cpu(*(u32*)data);
	pHalData->SdioRxFIFOSize = le16_to_cpu(*(u16*)&data[4]);

	if (pHalData->sdio_hisr & pHalData->sdio_himr)
	{
		u32 v32;
		pHalData->sdio_hisr &= pHalData->sdio_himr;
		
		// Reduce the frequency of RX Request Interrupt during RX handling
		DisableSdioInterrupt(priv);

		// clear HISR
		v32 = pHalData->sdio_hisr & MASK_SDIO_HISR_CLEAR;
		if (v32) {
			v32 = cpu_to_le32(v32);
			_sdio_local_write(priv, SDIO_REG_HISR, 4, (u8*)&v32);
		}

		sd_int_dpc(priv);
		
		EnableSdioInterrupt(priv);
	}
	else
	{
		DEBUG_WARN("%s: HISR(0x%08x) and HIMR(0x%08x) not match!\n",
				__FUNCTION__, pHalData->sdio_hisr, pHalData->sdio_himr);
	}
}

int sdio_alloc_irq(struct rtl8192cd_priv *priv)
{
	struct sdio_func *func;
	int err;

	func = priv->pshare->psdio_func;
	
	sdio_claim_host(func);
	
	err = sdio_claim_irq(func, &sd_sync_int_hdl);
	
	sdio_release_host(func);
	
	if (err)
		printk(KERN_CRIT "%s: sdio_claim_irq FAIL(%d)!\n", __func__, err);
	
	return err;
}

int sdio_free_irq(struct rtl8192cd_priv *priv)
{
	struct sdio_func *func;
	int err;

	func = priv->pshare->psdio_func;
	
	sdio_claim_host(func);
	
	err = sdio_release_irq(func);
	
	sdio_release_host(func);
	
	if (err)
		printk(KERN_CRIT "%s: sdio_release_irq FAIL(%d)!\n", __func__, err);
	
	return err;
}

static int sdio_init(struct rtl8192cd_priv *priv)
{
	struct priv_shared_info *pshare = priv->pshare;
	struct sdio_func *func = pshare->psdio_func;
	int err;
	
	sdio_claim_host(func);

	err = sdio_enable_func(func);
	if (err) {
		printk(KERN_CRIT "%s: sdio_enable_func FAIL(%d)!\n", __func__, err);
		goto release;
	}

	err = sdio_set_block_size(func, 512);
	if (err) {
		printk(KERN_CRIT "%s: sdio_set_block_size FAIL(%d)!\n", __func__, err);
		goto release;
	}
	pshare->block_transfer_len = 512;
	pshare->tx_block_mode = 1;
	pshare->rx_block_mode = 1;

release:
	sdio_release_host(func);
	return err;
}

static void sdio_deinit(struct rtl8192cd_priv *priv)
{
	struct sdio_func *func;
	int err;

	func = priv->pshare->psdio_func;

	if (func) {
		sdio_claim_host(func);
		
		err = sdio_disable_func(func);
		if (err)
			printk(KERN_ERR "%s: sdio_disable_func(%d)\n", __func__, err);
/*
		err = sdio_release_irq(func);
		if (err)
			printk(KERN_ERR "%s: sdio_release_irq(%d)\n", __func__, err);
*/
		sdio_release_host(func);
	}
}

int sdio_dvobj_init(struct rtl8192cd_priv *priv)
{
	int err = 0;
	
	priv->pshare->pHalData = (HAL_INTF_DATA_TYPE *) rtw_zmalloc(sizeof(HAL_INTF_DATA_TYPE));
	if (NULL == priv->pshare->pHalData)
		return -ENOMEM;
	
	priv->pshare->version_id = VERSION_8192E;
	
#ifdef  CONFIG_WLAN_HAL
	if (RT_STATUS_SUCCESS != HalAssociateNic(priv, TRUE)) {
		err = -ENOMEM;
		goto fail;
	}
#endif
	
	if ((err = sdio_init(priv)) != 0)
		goto fail;

	check_chipID_MIMO(priv);
	
	rtl8192es_interface_configure(priv);

	return 0;
	
fail:
	rtw_mfree(priv->pshare->pHalData, sizeof(HAL_INTF_DATA_TYPE));
	priv->pshare->pHalData = NULL;
	
	return err;
}

void sdio_dvobj_deinit(struct rtl8192cd_priv *priv)
{
	sdio_deinit(priv);

	if (priv->pshare->pHalData) {
		rtw_mfree(priv->pshare->pHalData, sizeof(HAL_INTF_DATA_TYPE));
		priv->pshare->pHalData = NULL;
	}
}

void rtw_dev_unload(struct rtl8192cd_priv *priv)
{
	struct priv_shared_info *pshare;
	
	pshare = priv->pshare;
	pshare->bDriverStopped = TRUE;

	if (FALSE == pshare->bSurpriseRemoved) {
		DisableSdioInterrupt(priv);
		sdio_free_irq(priv);
	}
	
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
	
	if (pshare->xmit_thread) {
		if (test_and_set_bit(WAKE_EVENT_XMIT, &pshare->wake_event) == 0)
			wake_up_process(pshare->xmit_thread);
		printk("[%s] xmit_thread", __FUNCTION__);
		wait_for_completion(&pshare->xmit_thread_done);
		printk(" terminate\n");
		pshare->xmit_thread = NULL;
	}
#ifdef REVO_MIFI
	if (pshare->wps_wake_thread) {
		complete(&g_wps_key_wait);
		wake_up_process(pshare->wps_wake_thread);
		printk("[%s] wps_wake_thread", __FUNCTION__);
		wait_for_completion(&pshare->wps_wake_thread_done);
		printk(" terminate\n");
		pshare->wps_wake_thread =NULL;
	}

#endif
	if (FALSE == pshare->bSurpriseRemoved) {
		GET_HAL_INTERFACE(priv)->StopHWHandler(priv);
		pshare->bSurpriseRemoved = TRUE;
	}
}

static void HalRxAggr8192ESdio(struct rtl8192cd_priv *priv)
{
	u8	valueDMATimeout;
	u8	valueDMAPageCount;
	
	valueDMATimeout = 0x01;
#ifdef PLATFORM_ARM_BALONG
	valueDMAPageCount = 0x04;// 0xf->0x4
#else
	valueDMAPageCount = 0x0f;
#endif
	printk("HalRxAggr8192ESdio 4K  new version!!\n");
	
	RTL_W8(RXDMA_AGG_PG_TH+1, valueDMATimeout);
	RTL_W8(RXDMA_AGG_PG_TH, valueDMAPageCount);
}

void sdio_AggSettingRxUpdate(struct rtl8192cd_priv *priv)
{
	u8 valueDMA;
	
	valueDMA = RTL_R8(TRXDMA_CTRL);
	valueDMA |= RXDMA_AGG_EN;
	RTL_W8(TRXDMA_CTRL, valueDMA);
	
	RTL_W32(REG_RXDMA_MODE, 0x2);
}

void _initSdioAggregationSetting(struct rtl8192cd_priv *priv)
{
	// Tx aggregation setting
	//sdio_AggSettingTxUpdate(priv);

	// Rx aggregation setting
	HalRxAggr8192ESdio(priv);
	sdio_AggSettingRxUpdate(priv);
}

//
//	Description:
//		Query SDIO Local register to get the current number of TX OQT Free Space.
//
u8 HalQueryTxOQTBufferStatus8192ESdio(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u16 oqtFreeSpace;
	
	oqtFreeSpace = SdioLocalCmd52Read2Byte(priv, SDIO_REG_OQT_FREE_SPACE);
	memcpy(pHalData->SdioTxOQTFreeSpace, &oqtFreeSpace, MAX_TXOQT_TYPE);
	
	return TRUE;
}

//
//	Description:
//		Query SDIO Local register to get the current number of Free TxPacketBuffer page.
//
s32 HalQueryTxBufferStatus8192ESdio(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u8 NumOfFreePage[SDIO_TX_FREE_PG_QUEUE];
	u32 addr;
	s32 err;

//	NumOfFreePage = SdioLocalCmd53Read4Byte(priv, SDIO_REG_FREE_TXPG);
	addr = SDIO_REG_FREE_TXPG;
//	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr);  //zyj
//	err = sd_read(priv, addr, SDIO_TX_FREE_PG_QUEUE, NumOfFreePage);  //zyj
	err=sdio_local_read(priv, addr, SDIO_TX_FREE_PG_QUEUE, NumOfFreePage);// since the size of  SDIO_TX_FREE_PG_QUEUE is 5,sd_read 5 bytes will error ,modify into sdio_local_read  zyj
	if (err)
		return err;

	memcpy(pHalData->SdioTxFIFOFreePage, NumOfFreePage, SDIO_TX_FREE_PG_QUEUE);
	DEBUG_INFO("%s: Free page for HIQ(%#x),MIQ(%#x),LOQ(%#x),EXQ(%#x),PUBQ(%#x)\n",
			__FUNCTION__,
			pHalData->SdioTxFIFOFreePage[HI_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[MID_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[LOW_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[EXTRA_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[PUBLIC_QUEUE_IDX]);
	
	return 0;
}

#ifdef CONFIG_SDIO_TX_IN_INTERRUPT
s32 HalQueryTxBufferStatus8192ESdio2(struct rtl8192cd_priv *priv)
{
	HAL_INTF_DATA_TYPE *pHalData = GET_HAL_INTF_DATA(priv);
	u8 NumOfFreePage[SDIO_TX_FREE_PG_QUEUE];
	u32 addr;
	s32 err;
	
	_queue *pqueue = &priv->pshare->pending_xmitbuf_queue;
	_irqL irql;
	int update = 0;
	
//	NumOfFreePage = SdioLocalCmd53Read4Byte(priv, SDIO_REG_FREE_TXPG);
	addr = SDIO_REG_FREE_TXPG;
//	HalSdioGetCmdAddr8723ASdio(priv, SDIO_LOCAL_DEVICE_ID, addr, &addr); //zyj
//	err = sd_read(priv, addr, SDIO_TX_FREE_PG_QUEUE, NumOfFreePage);   //zyj
	err=_sdio_local_read(priv, addr, SDIO_TX_FREE_PG_QUEUE, NumOfFreePage);

	if (err)
		return err;
	
	_enter_critical_bh(&pqueue->lock, &irql);
	if (!priv->pshare->freepage_updated) {
		memcpy(pHalData->SdioTxFIFOFreePage, NumOfFreePage, SDIO_TX_FREE_PG_QUEUE);
		update = 1;
	}
	_exit_critical_bh(&pqueue->lock, &irql);
	
	DEBUG_INFO("%s: Free page for HIQ(%#x),MIQ(%#x),LOQ(%#x),EXQ(%#x),PUBQ(%#x),Update(%d)\n",
			__FUNCTION__,
			pHalData->SdioTxFIFOFreePage[HI_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[MID_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[LOW_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[EXTRA_QUEUE_IDX],
			pHalData->SdioTxFIFOFreePage[PUBLIC_QUEUE_IDX],
			update);
	
	return 0;
}
#endif

static void pre_rtl8192es_beacon_timer(unsigned long task_priv)
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
static void rtl8192es_beacon_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct priv_shared_info *pshare = priv->pshare;
	u32 beacon_interval, timestamp;
	u32 cur_tick, time_offset;
#ifdef MBSSID
	u32 inter_beacon_space;
	int nr_vap, idx, bcn_idx;
#endif
	u8 val8, late=0;
	
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
#ifdef MBSSID
		if (nr_vap) {
			if (priv->vap_init_seq & 0x1) {
				// Use BCNQ1 for VAP1/VAP3/VAP5/VAP7
				RTL_W8(REG_DWBCN1_CTRL+2, RTL_R8(REG_DWBCN1_CTRL+2) | BIT4);
			} else {
				// Use BCNQ0 for Root/VAP2/VAP4/VAP6
				RTL_W8(REG_DWBCN1_CTRL+2, RTL_R8(REG_DWBCN1_CTRL+2) & ~BIT4);
			}
		}
#endif
		update_beacon(priv);
		
		// handle any buffered BC/MC frames
		// Don't dynamically change DIS_ATIM due to HW will auto send ACQ after HIQ empty.
		val8 = *((unsigned char *)priv->beaconbuf + priv->timoffset + 4);
		if (val8 & 0x01) {
			process_mcast_dzqueue(priv);
			priv->pkt_in_dtimQ = 0;
		}
	}
}

u8 rtw_init_drv_sw(struct rtl8192cd_priv *priv)
{
#ifdef REVO_MIFI
	init_completion(&priv->pshare->wps_wake_thread_done);
	priv->pshare->wps_wake_thread = NULL;
#endif
	if (_rtw_init_cmd_priv(priv) == FAIL)
		goto cmd_fail;
	
	if (_rtw_init_xmit_priv(priv) == FAIL)
		goto xmit_fail;

	if (_rtw_init_recv_priv(priv) == FAIL)
		goto recv_fail;
	
	init_timer(&priv->pshare->beacon_timer);
	priv->pshare->beacon_timer.data = (unsigned long)priv;
	priv->pshare->beacon_timer.function = pre_rtl8192es_beacon_timer;
	INIT_TIMER_EVENT_ENTRY(&priv->pshare->beacon_timer_event,
		rtl8192es_beacon_timer, (unsigned long)priv);
	
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

