/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *******************************************************************************/
#define _SDIO_OPS_LINUX_C_

#include <drv_types.h>

#include <linux/mmc/sdio_func.h>

#define SD_IO_TRY_CNT (8)
static u8 rtw_sdio_readb(struct sdio_func *func, unsigned int addr, int *err)
{
	u8 val;
	int err_ret = 0;

	val = sdio_readb(func, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x\n", __func__, err_ret, addr, val);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			val = sdio_readb(func, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);

	}
	if (err) {
		*err = err_ret;
	}

	return val;
}

static u16 rtw_sdio_readw(struct sdio_func *func, unsigned int addr, int *err)
{
	u16 val;
	int err_ret = 0;

	val = sdio_readw(func, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x\n", __func__, err_ret, addr, val);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			val = sdio_readw(func, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);

	}
	if (err) {
		*err = err_ret;
	}

	return val;
}

static u32 rtw_sdio_readl(struct sdio_func *func, unsigned int addr, int *err)
{
	u32 val;
	int err_ret = 0;

	val = sdio_readl(func, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x\n", __func__, err_ret, addr, val);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			val = sdio_readl(func, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);

	}
	if (err) {
		*err = err_ret;
	}

	return val;
}

static void rtw_sdio_writeb(struct sdio_func *func, u8 b, unsigned int addr, int *err)
{
	int err_ret = 0;

	sdio_writeb(func, b, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x\n", __func__, err_ret, addr, b);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			sdio_writeb(func, b, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);

	}
	if (err) {
		*err = err_ret;
	}
}

static void rtw_sdio_writew(struct sdio_func *func, u16 b, unsigned int addr, int *err)
{
	int err_ret = 0;

	sdio_writew(func, b, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x\n", __func__, err_ret, addr, b);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			sdio_writew(func, b, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);

	}
	if (err) {
		*err = err_ret;
	}
}

static void rtw_sdio_writel(struct sdio_func *func, u32 b, unsigned int addr, int *err)
{
	int err_ret = 0;

	sdio_writel(func, b, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x\n", __func__, err_ret, addr, b);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			sdio_writel(func, b, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);

	}
	if (err) {
		*err = err_ret;
	}
}

static unsigned char rtw_sdio_f0_readb(struct sdio_func *func, unsigned int addr,
	int *err)
{
	unsigned char val;
	int err_ret = 0;

	val = sdio_f0_readb(func, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x\n", __func__, err_ret, addr, val);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			val = sdio_f0_readb(func, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x, val=0x%x, try_cnt=%d\n", __func__, err_ret, addr, val, i);

	}
	if (err) {
		*err = err_ret;
	}

	return val;
}

static void rtw_sdio_f0_writeb(struct sdio_func *func, unsigned char b, unsigned int addr,
	int *err)
{
	int err_ret = 0;

	sdio_f0_writeb(func, b, addr, &err_ret);
	if (err_ret)
	{
		int i;

		DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x\n", __func__, err_ret, addr, b);

		err_ret = 0;
		for(i=0; i<SD_IO_TRY_CNT; i++)
		{
			sdio_f0_writeb(func, b, addr, &err_ret);
			if (err_ret == 0)
				break;
		}

		if (i==SD_IO_TRY_CNT)
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);
		else
			DBG_871X(KERN_ERR "%s: (%d) addr=0x%05x val=0x%08x, try_cnt=%d\n", __func__, err_ret, addr, b, i);

	}
	if (err) {
		*err = err_ret;
	}
}

//dont retry under memcpy
static int rtw_sdio_memcpy_fromio(struct sdio_func *func, void *dst,
	unsigned int addr, int count)
{
	return sdio_memcpy_fromio(func, dst, addr, count);
}

static int rtw_sdio_memcpy_toio(struct sdio_func *func, unsigned int addr,
	void *src, int count)
{
	return sdio_memcpy_toio(func, addr, src, count);
}

u8 sd_f0_read8(PSDIO_DATA psdio, u32 addr, s32 *err)
{
	u8 v;
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	v = rtw_sdio_f0_readb(func, addr, err);
	sdio_release_host(func);
	if (err && *err)
		DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

_func_exit_;

	return v;
}

void sd_f0_write8(PSDIO_DATA psdio, u32 addr, u8 v, s32 *err)
{
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	rtw_sdio_f0_writeb(func, v, addr, err);
	sdio_release_host(func);
	if (err && *err)
		DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, *err, addr, v);

_func_exit_;
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 _sd_cmd52_read(PSDIO_DATA psdio, u32 addr, u32 cnt, u8 *pdata)
{
	int err, i;
	struct sdio_func *func;

_func_enter_;

	err = 0;
	func = psdio->func;

	for (i = 0; i < cnt; i++) {
		pdata[i] = rtw_sdio_readb(func, addr+i, &err);
		if (err) {
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);
			break;
		}
	}

_func_exit_;

	return err;
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 sd_cmd52_read(PSDIO_DATA psdio, u32 addr, u32 cnt, u8 *pdata)
{
	int err, i;
	struct sdio_func *func;

_func_enter_;

	err = 0;
	func = psdio->func;

	sdio_claim_host(func);
	err = _sd_cmd52_read(psdio, addr, cnt, pdata);
	sdio_release_host(func);

_func_exit_;

	return err;
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 _sd_cmd52_write(PSDIO_DATA psdio, u32 addr, u32 cnt, u8 *pdata)
{
	int err, i;
	struct sdio_func *func;

_func_enter_;

	err = 0;
	func = psdio->func;

	for (i = 0; i < cnt; i++) {
		rtw_sdio_writeb(func, pdata[i], addr+i, &err);
		if (err) {
			DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, err, addr, pdata[i]);
			break;
		}
	}

_func_exit_;

	return err;
}

/*
 * Return:
 *	0		Success
 *	others	Fail
 */
s32 sd_cmd52_write(PSDIO_DATA psdio, u32 addr, u32 cnt, u8 *pdata)
{
	int err, i;
	struct sdio_func *func;

_func_enter_;

	err = 0;
	func = psdio->func;

	sdio_claim_host(func);
	err = _sd_cmd52_write(psdio, addr, cnt, pdata);
	sdio_release_host(func);

_func_exit_;

	return err;
}

u8 _sd_read8(PSDIO_DATA psdio, u32 addr, s32 *err)
{
	u8 v;
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	//sdio_claim_host(func);
	v = rtw_sdio_readb(func, addr, err);
	//sdio_release_host(func);
	if (err && *err)
		DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

_func_exit_;

	return v;
}

u8 sd_read8(PSDIO_DATA psdio, u32 addr, s32 *err)
{
	u8 v;
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	v = rtw_sdio_readb(func, addr, err);
	sdio_release_host(func);
	if (err && *err)
		DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

_func_exit_;

	return v;
}

u16 sd_read16(PSDIO_DATA psdio, u32 addr, s32 *err)
{
	u16 v;
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	v = rtw_sdio_readw(func, addr, err);
	sdio_release_host(func);
	if (err && *err)
		DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, *err, addr);

_func_exit_;

	return  v;
}

u32 _sd_read32(PSDIO_DATA psdio, u32 addr, s32 *err)
{
	u32 v;
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	//sdio_claim_host(func);
	v = rtw_sdio_readl(func, addr, err);
	//sdio_release_host(func);

_func_exit_;

	return  v;
}

u32 sd_read32(PSDIO_DATA psdio, u32 addr, s32 *err)
{
	u32 v;
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	v = rtw_sdio_readl(func, addr, err);
	sdio_release_host(func);

_func_exit_;

	return  v;
}

void sd_write8(PSDIO_DATA psdio, u32 addr, u8 v, s32 *err)
{
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	rtw_sdio_writeb(func, v, addr, err);
	sdio_release_host(func);
	if (err && *err)
		DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, *err, addr, v);

_func_exit_;
}

void sd_write16(PSDIO_DATA psdio, u32 addr, u16 v, s32 *err)
{
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	rtw_sdio_writew(func, v, addr, err);
	sdio_release_host(func);
	if (err && *err)
		DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%04x\n", __func__, *err, addr, v);

_func_exit_;
}

void _sd_write32(PSDIO_DATA psdio, u32 addr, u32 v, s32 *err)
{
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	//sdio_claim_host(func);
	rtw_sdio_writel(func, v, addr, err);
	//sdio_release_host(func);

_func_exit_;
}

void sd_write32(PSDIO_DATA psdio, u32 addr, u32 v, s32 *err)
{
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	sdio_claim_host(func);
	rtw_sdio_writel(func, v, addr, err);
	sdio_release_host(func);

_func_exit_;
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
s32 _sd_read(PSDIO_DATA psdio, u32 addr, u32 cnt, void *pdata)
{
	int err;
	struct sdio_func *func;

_func_enter_;

	func = psdio->func;

	if (unlikely((cnt==1) || (cnt==2)))
	{
		int i;
		u8 *pbuf = (u8*)pdata;

		for (i = 0; i < cnt; i++)
		{
			*(pbuf+i) = rtw_sdio_readb(func, addr+i, &err);

			if (err) {
				DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);
				break;
			}
		}
		return err;
	}

	err = rtw_sdio_memcpy_fromio(func, pdata, addr, cnt);
	if (err) {
		DBG_871X(KERN_ERR "%s: FAIL(%d)! ADDR=%#x Size=%d\n", __func__, err, addr, cnt);
	}

_func_exit_;

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
s32 sd_read(PSDIO_DATA psdio, u32 addr, u32 cnt, void *pdata)
{
	s32 err;
	struct sdio_func *func;


	func = psdio->func;

	sdio_claim_host(func);
	err = _sd_read(psdio, addr, cnt, pdata);
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
s32 _sd_write(PSDIO_DATA psdio, u32 addr, u32 cnt, void *pdata)
{
	int err;
	struct sdio_func *func;
	u32 size;

_func_enter_;

	func = psdio->func;
//	size = sdio_align_size(func, cnt);

	if (unlikely((cnt==1) || (cnt==2)))
	{
		int i;
		u8 *pbuf = (u8*)pdata;

		for (i = 0; i < cnt; i++)
		{
			rtw_sdio_writeb(func, *(pbuf+i), addr+i, &err);
			if (err) {
				DBG_871X(KERN_ERR "%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, err, addr, *(pbuf+i));
				break;
			}
		}

		return err;
	}

	size = cnt;
	err = rtw_sdio_memcpy_toio(func, addr, pdata, size);
	if (err) {
		DBG_871X(KERN_ERR "%s: FAIL(%d)! ADDR=%#x Size=%d(%d)\n", __func__, err, addr, cnt, size);
	}

_func_exit_;

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
s32 sd_write(PSDIO_DATA psdio, u32 addr, u32 cnt, void *pdata)
{
	s32 err;
	struct sdio_func *func;


	func = psdio->func;

	sdio_claim_host(func);
	err = _sd_write(psdio, addr, cnt, pdata);
	sdio_release_host(func);

	return err;
}

