/*
 *  SDIO core routines
 *
 *  $Id: 8192e_sdio_hw.c,v 1.27.2.31 2010/12/31 08:37:43 family Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192E_SDIO_HW_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "8192cd.h"
#include "8192cd_headers.h"
#include "8192cd_debug.h"
#include "WlanHAL/HalHeader/HalComBit.h"
#include "WlanHAL/HalHeader/HalComReg.h"


extern struct _device_info_ wlan_device[];
extern int drv_registered;


extern int MDL_DEVINIT rtl8192cd_init_one(struct sdio_func *psdio_func,
		const struct sdio_device_id *ent, struct _device_info_ *wdev, int vap_idx);
extern void rtl8192cd_deinit_one(struct rtl8192cd_priv *priv);

extern VOID SetDTIM(IN  HAL_PADAPTER Adapter);


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
#ifdef CONFIG_POWER_SUSPEND
static int rtw_sdio_suspend(struct device *dev)
{
	struct sdio_func *func =dev_to_sdio_func(dev);
	struct dvobj_priv *psdpriv = sdio_get_drvdata(func);
	int ret = 0;
	printk("===>rtw_sdio_suspend\n");
	mmc_pm_flag_t pm_flag = 0;
	pm_flag = sdio_get_host_pm_caps(func);
	if (!(pm_flag & MMC_PM_KEEP_POWER))
	{
			printk("%s: cannot remain alive while host is suspended\n", sdio_func_id(func));
			pm_flag |= MMC_PM_KEEP_POWER;
			sdio_set_host_pm_flags(func, pm_flag);
	}
	return ret;
}

static int rtw_sdio_resume(struct device *dev)
{
	struct sdio_func *func =dev_to_sdio_func(dev);
	struct dvobj_priv *psdpriv = sdio_get_drvdata(func);
	 int ret = 0;
	 printk("===>rtw_sdio_resume\n");
	if (func)
	{
		mmc_pm_flag_t pm_flag = 0;
		pm_flag = sdio_get_host_pm_caps(func);
		if (pm_flag & MMC_PM_KEEP_POWER)
		{
			pm_flag &= ~MMC_PM_KEEP_POWER;
			sdio_set_host_pm_flags(func, pm_flag);
		}
	}
	return ret;
}
#endif
MODULE_DEVICE_TABLE(sdio, rtw_sdio_id_tbl);
#ifdef CONFIG_POWER_SUSPEND
//#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
static const struct dev_pm_ops rtw_sdio_pm_ops = {
	.suspend	= rtw_sdio_suspend,
	.resume	= rtw_sdio_resume,
};
//#endif
#endif
struct sdio_driver rtl8192cd_sdio_driver = {
	.name = (char*)DRV_NAME,
	.id_table = rtw_sdio_id_tbl,
	.probe = rtw_drv_init,
	.remove = rtw_dev_remove,
//#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
#ifdef CONFIG_POWER_SUSPEND
	.drv = {
		.pm = &rtw_sdio_pm_ops,
	}
#endif
//#endif
};

u8 _CardEnable(struct rtl8192cd_priv *priv)
{
	unsigned char errorFlag ;

	if ( RT_STATUS_SUCCESS != GET_HAL_INTERFACE(priv)->InitPONHandler(priv, XTAL_CLK_SEL_40M) ) {
		GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_DRV_DBG, (unsigned char *)&errorFlag);
		errorFlag |= DRV_ER_INIT_PON;
		GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv, HW_VAR_DRV_DBG, (unsigned char *)&errorFlag);
		panic_printk("InitPON Failed\n");
		return FAIL;
	} else {
		printk("InitPON OK\n");
	}
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
		camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | BIT_MBIDCAM_VALID | (macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		for (j=1; j>=0; j--) {
			RTL_W32((REG_MBIDCAMCFG_1+4)-4*j, camData[j]);
		}

		// clear the rest area of CAM
		camData[1] = 0;
		for (i=1; i<8; i++) {
			camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (i & BIT_MASK_MBIDCAM_ADDR)<<BIT_SHIFT_MBIDCAM_ADDR;
			for (j=1; j>=0; j--) {
				RTL_W32((REG_MBIDCAMCFG_1+4)-4*j, camData[j]);
			}
		}
		
		priv->pshare->nr_vap_bcn = 0;
		priv->pshare->bcn_priv[0] = priv;
		priv->pshare->inter_bcn_space = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * NET80211_TU_TO_US;

		RTL_W32(REG_MBSSID_BCN_SPACE,
			(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BIT_MASK_BCN_SPACE1)<<BIT_SHIFT_BCN_SPACE1
			|(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BIT_MASK_BCN_SPACE0)<<BIT_SHIFT_BCN_SPACE0);

		RTL_W8(REG_BCN_CTRL, 0);
		RTL_W8(REG_DUAL_TSF_RST, 1);

		RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION|BIT_DIS_TSF_UDT|BIT_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);

		RTL_W32(REG_RCR, RTL_R32(REG_RCR) | BIT_ENMBID);	// MBSSID enable
		
		// SW control use BCNQ0 for Root-only
		RTL_W32(REG_DWBCN1_CTRL, (RTL_R32(REG_DWBCN1_CTRL) & ~BIT20) | BIT17);
	}
	else if (IS_VAP_INTERFACE(priv))
	{
		priv->vap_init_seq = priv->vap_id +1;
		
		SetDTIM(priv);

		camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | BIT_MBIDCAM_VALID |
				(priv->vap_init_seq & BIT_MASK_MBIDCAM_ADDR)<<BIT_SHIFT_MBIDCAM_ADDR |
				(macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		for (j=1; j>=0; j--) {
			RTL_W32((REG_MBIDCAMCFG_1+4)-4*j, camData[j]);
		}
		
		priv->pshare->bcn_priv[priv->pshare->nr_vap_bcn+1] = priv;
		nr_vap = ++priv->pshare->nr_vap_bcn;
		vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod/(nr_vap+1);
		
		if (vap_bcn_offset > 200)
			vap_bcn_offset = 200;
		RTL_W32(REG_MBSSID_BCN_SPACE, (vap_bcn_offset & BIT_MASK_BCN_SPACE1)<<BIT_SHIFT_BCN_SPACE1
			|(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BIT_MASK_BCN_SPACE0)<<BIT_SHIFT_BCN_SPACE0);
		
		priv->pshare->inter_bcn_space = vap_bcn_offset * NET80211_TU_TO_US;
		
		RTL_W8(REG_BCN_CTRL, 0);
		RTL_W8(REG_DUAL_TSF_RST, 1);

		RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT|BIT_EN_TXBCN_RPT |BIT_DIS_RX_BSSID_FIT);
		RTL_W8(REG_MBID_NUM, (RTL_R8(REG_MBID_NUM) & ~BIT_MASK_MBID_BCN_NUM) | (nr_vap & BIT_MASK_MBID_BCN_NUM));
		RTL_W8(REG_MBSSID_CTRL, (RTL_R8(REG_MBSSID_CTRL) | (1 << priv->vap_init_seq)));
		
		RTL_W32(REG_RCR, RTL_R32(REG_RCR) & ~BIT_ENMBID);
		RTL_W32(REG_RCR, RTL_R32(REG_RCR) | BIT_ENMBID);	// MBSSID enable
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
			camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (i & BIT_MASK_MBIDCAM_ADDR)<<BIT_SHIFT_MBIDCAM_ADDR;
			for (j=1; j>=0; j--) {
				RTL_W32((REG_MBIDCAMCFG_1+4)-4*j, camData[j]);
			}
		}
		
		priv->pshare->nr_vap_bcn = 0;
		priv->pshare->bcn_priv[0] = priv;
		priv->pshare->inter_bcn_space = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * NET80211_TU_TO_US;

		RTL_W32(REG_RCR, RTL_R32(REG_RCR) & ~BIT_ENMBID);	// MBSSID disable
	        RTL_W32(REG_MBSSID_BCN_SPACE,
			(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BIT_MASK_BCN_SPACE0)<<BIT_SHIFT_BCN_SPACE0);

	        RTL_W8(REG_BCN_CTRL, 0);
	        RTL_W8(REG_DUAL_TSF_RST, 1);
	        RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT| BIT_EN_TXBCN_RPT);
	}
	else if (IS_VAP_INTERFACE(priv) && (priv->vap_init_seq >= 0))
	{
		camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN |
			(priv->vap_init_seq & BIT_MASK_MBIDCAM_ADDR)<<BIT_SHIFT_MBIDCAM_ADDR;
		for (j=1; j>=0; j--) {
			RTL_W32((REG_MBIDCAMCFG_1+4)-4*j, camData[j]);
		}
		
		if (priv->pshare->nr_vap_bcn) {
			nr_vap = --priv->pshare->nr_vap_bcn;
			for (i = 0; i <= nr_vap; ++i) {
				if (priv->pshare->bcn_priv[i] == priv) {
					priv->pshare->bcn_priv[i] = priv->pshare->bcn_priv[nr_vap+1];
					break;
				}
			}
			
			RTL_W8(REG_MBID_NUM, nr_vap & BIT_MASK_MBID_BCN_NUM);
			RTL_W8(REG_MBSSID_CTRL, (RTL_R8(REG_MBSSID_CTRL) & ~(1 << priv->vap_init_seq)));
			
			vap_bcn_offset = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod/(nr_vap+1);
			if (vap_bcn_offset > 200)
				vap_bcn_offset = 200;
			RTL_W32(REG_MBSSID_BCN_SPACE, (vap_bcn_offset & BIT_MASK_BCN_SPACE1)<<BIT_SHIFT_BCN_SPACE1
				|(priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod & BIT_MASK_BCN_SPACE0)<<BIT_SHIFT_BCN_SPACE0);
			
			priv->pshare->inter_bcn_space = vap_bcn_offset * NET80211_TU_TO_US;
			
			RTL_W8(REG_BCN_CTRL, 0);
			RTL_W8(REG_DUAL_TSF_RST, 1);
			RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT| BIT_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
			
			if (0 == nr_vap) {
				// Use BCNQ0 for Root-only
				RTL_W8(REG_DWBCN1_CTRL+2, RTL_R8(REG_DWBCN1_CTRL+2) & ~BIT4);
			}
		}
		
		RTL_W32(REG_RCR, RTL_R32(REG_RCR) & ~BIT_ENMBID);
		RTL_W32(REG_RCR, RTL_R32(REG_RCR) | BIT_ENMBID);
		priv->vap_init_seq = -1;
	}
	
	SMP_UNLOCK_MBSSID(&irqL);
}
#endif // MBSSID

