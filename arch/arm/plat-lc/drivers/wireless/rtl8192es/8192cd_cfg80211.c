/*
 * Copyright (c) 2004-2011 Atheros Communications Inc.
 * Copyright (c) 2011-2012 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/delay.h>
#endif

#include "./8192cd_cfg.h"

#ifdef RTK_NL80211

#include <linux/kernel.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include <net/ieee80211_radiotap.h>
#include <linux/wireless.h>
#include <linux/device.h>
#include <linux/if_ether.h>
#include <linux/nl80211.h>



//#include "./nl80211_copy.h"

#ifdef __LINUX_2_6__
#include <linux/initrd.h>
#include <linux/syscalls.h>
#endif

#include "./8192cd.h"
#include "./8192cd_debug.h"
#include "./8192cd_cfg80211.h"
#include "./8192cd_headers.h"

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>
#include "./8192cd_net80211.h"  

#if 0
#define NLENTER	printk("\n[rtk_nl80211]%s +++\n", (char *)__FUNCTION__)
#define NLEXIT printk("[rtk_nl80211]%s ---\n\n", (char *)__FUNCTION__)
#define NLINFO printk("[rtk_nl80211]%s %d\n", (char *)__FUNCTION__, __LINE__)
#define NLNOT printk("[rtk_nl80211]%s !!! NOT implement YET !!!\n", (char *)__FUNCTION__)
#else
#define NLENTER
#define NLEXIT
#define NLINFO
#define NLNOT
#endif

#define RTK_MAX_WIFI_PHY 2
static int rtk_phy_idx=0;
struct rtknl *rtk_phy[RTK_MAX_WIFI_PHY]; 
static dev_t rtk_wifi_dev[RTK_MAX_WIFI_PHY]; 
static char *rtk_dev_name[RTK_MAX_WIFI_PHY]={"RTKWiFi0","RTKWiFi1"}; 

char rtk_fake_addr[6]={0x00,0xe0,0x4c,0x81,0x92,0xcc}; //mark_dual , FIXME if wlan_mac readable

#define MAX_5G_DIFF_NUM		14
#define PIN_LEN					8
#define SIGNATURE_LEN			4
#if 0
#ifdef CONFIG_RTL_HW_SETTING_OFFSET
#define HW_SETTING_OFFSET		CONFIG_RTL_HW_SETTING_OFFSET
#else
#define HW_SETTING_OFFSET		0x6000
#endif
#endif
extern unsigned int HW_SETTING_OFFSET; //mark_hw,from rtl819x_flash.c
#define HW_WLAN_SETTING_OFFSET	13

__PACK struct hw_wlan_setting {
	unsigned char macAddr[6] ;
	unsigned char macAddr1[6] ;
	unsigned char macAddr2[6] ;
	unsigned char macAddr3[6] ;
	unsigned char macAddr4[6] ;
	unsigned char macAddr5[6] ; 
	unsigned char macAddr6[6] ; 
	unsigned char macAddr7[6] ; 
	unsigned char pwrlevelCCK_A[MAX_2G_CHANNEL_NUM] ; 	
	unsigned char pwrlevelCCK_B[MAX_2G_CHANNEL_NUM] ; 
	unsigned char pwrlevelHT40_1S_A[MAX_2G_CHANNEL_NUM] ;	
	unsigned char pwrlevelHT40_1S_B[MAX_2G_CHANNEL_NUM] ;	
	unsigned char pwrdiffHT40_2S[MAX_2G_CHANNEL_NUM] ;	
	unsigned char pwrdiffHT20[MAX_2G_CHANNEL_NUM] ; 
	unsigned char pwrdiffOFDM[MAX_2G_CHANNEL_NUM] ; 
	unsigned char regDomain ; 	
	unsigned char rfType ; 
	unsigned char ledType ; // LED type, see LED_TYPE_T for definition	
	unsigned char xCap ;	
	unsigned char TSSI1 ;	
	unsigned char TSSI2 ;	
	unsigned char Ther ;	
	unsigned char Reserved1 ;
	unsigned char Reserved2 ;
	unsigned char Reserved3 ;
	unsigned char Reserved4 ;
	unsigned char Reserved5 ;	
	unsigned char Reserved6 ;
	unsigned char Reserved7 ;	
	unsigned char Reserved8 ;
	unsigned char Reserved9 ;
	unsigned char Reserved10 ;
	unsigned char pwrlevel5GHT40_1S_A[MAX_5G_CHANNEL_NUM] ;	
	unsigned char pwrlevel5GHT40_1S_B[MAX_5G_CHANNEL_NUM] ;	
	unsigned char pwrdiff5GHT40_2S[MAX_5G_CHANNEL_NUM] ; 
	unsigned char pwrdiff5GHT20[MAX_5G_CHANNEL_NUM] ;	
	unsigned char pwrdiff5GOFDM[MAX_5G_CHANNEL_NUM] ;

	
	unsigned char wscPin[PIN_LEN+1] ;	

#ifdef RTK_AC_SUPPORT
	unsigned char pwrdiff_20BW1S_OFDM1T_A[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_40BW2S_20BW2S_A[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_OFDM2T_CCK2T_A[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_40BW3S_20BW3S_A[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_4OFDM3T_CCK3T_A[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_40BW4S_20BW4S_A[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_OFDM4T_CCK4T_A[MAX_2G_CHANNEL_NUM] ;

	unsigned char pwrdiff_5G_20BW1S_OFDM1T_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_40BW2S_20BW2S_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_40BW3S_20BW3S_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_40BW4S_20BW4S_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_RSVD_OFDM4T_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW1S_160BW1S_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW2S_160BW2S_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW3S_160BW3S_A[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW4S_160BW4S_A[MAX_5G_DIFF_NUM] ;


	unsigned char pwrdiff_20BW1S_OFDM1T_B[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_40BW2S_20BW2S_B[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_OFDM2T_CCK2T_B[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_40BW3S_20BW3S_B[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_OFDM3T_CCK3T_B[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_40BW4S_20BW4S_B[MAX_2G_CHANNEL_NUM] ;
	unsigned char pwrdiff_OFDM4T_CCK4T_B[MAX_2G_CHANNEL_NUM] ;

	unsigned char pwrdiff_5G_20BW1S_OFDM1T_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_40BW2S_20BW2S_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_40BW3S_20BW3S_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_40BW4S_20BW4S_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_RSVD_OFDM4T_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW1S_160BW1S_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW2S_160BW2S_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW3S_160BW3S_B[MAX_5G_DIFF_NUM] ;
	unsigned char pwrdiff_5G_80BW4S_160BW4S_B[MAX_5G_DIFF_NUM] ;
#endif

}__WLAN_ATTRIB_PACK__;
__PACK struct param_header {
	unsigned char signature[SIGNATURE_LEN];  // Tag + version
	unsigned short len ;
} __WLAN_ATTRIB_PACK__;

void read_flash_hw_mac( unsigned char *mac,int idx)
{
	unsigned int offset;
	if(!mac)
		return;
	offset = HW_SETTING_OFFSET+ sizeof(struct param_header)+ HW_WLAN_SETTING_OFFSET + sizeof(struct hw_wlan_setting) * idx;
	offset |= 0xbd000000;
	memcpy(mac,(unsigned char *)offset,ETH_ALEN);
}

//#define CPTCFG_CFG80211_MODULE 1 // mark_com

#if 1 //_eric_nl event 

struct rtl8192cd_priv* realtek_get_priv(struct wiphy *wiphy, struct net_device *dev)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = NULL;

	if(dev)
	{
#ifdef NETDEV_NO_PRIV
		priv = ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv;
#else
		priv = (struct rtl8192cd_priv *)dev->priv;
#endif
	}
	else
		priv = rtk->priv;

	return priv;
}

struct ieee80211_channel *rtk_get_iee80211_channel(struct wiphy *wiphy, unsigned int channel)
{
	unsigned int  freq = 0;

	if(channel >= 34)
		freq = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_5GHZ);
	else
		freq = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_2GHZ);

	return ieee80211_get_channel(wiphy, freq);

}


void realtek_cfg80211_inform_bss(struct rtl8192cd_priv *priv)
{
	struct wiphy *wiphy = priv->rtk->wiphy;
	struct ieee80211_channel *channel = NULL;
	struct ieee80211_bss *bss = NULL;
	char tmpbuf[33];
	UINT8 *mac = NULL;
	unsigned long timestamp = 0;
	unsigned char ie[MAX_IE_LEN];
	unsigned char ie_len = 0;
	unsigned char wpa_ie_len = 0;
	unsigned char rsn_ie_len = 0;

	mac = priv->pmib->dot11Bss.bssid;
	wpa_ie_len = priv->rtk->clnt_info.wpa_ie.wpa_ie_len;
	rsn_ie_len = priv->rtk->clnt_info.rsn_ie.rsn_ie_len;
	
	channel = rtk_get_iee80211_channel(wiphy, priv->pmib->dot11Bss.channel);
		
	if(channel == NULL)
	{
		printk("Null channel!!\n");
		return;
	}
		
	timestamp = priv->pmib->dot11Bss.t_stamp[0] + (priv->pmib->dot11Bss.t_stamp[0]<<32);

	ie[0]= _SSID_IE_;
	ie[1]= priv->pmib->dot11Bss.ssidlen;
	memcpy(ie+2, priv->pmib->dot11Bss.ssid, priv->pmib->dot11Bss.ssidlen);
	ie_len += (priv->pmib->dot11Bss.ssidlen + 2);
			
	if((ie_len + wpa_ie_len + rsn_ie_len) < MAX_IE_LEN)
	{
		if(wpa_ie_len)
		{
			memcpy(ie+ie_len, priv->rtk->clnt_info.wpa_ie.data, wpa_ie_len);
			ie_len += wpa_ie_len;
		}

		if(rsn_ie_len)
		{
				memcpy(ie+ie_len, priv->rtk->clnt_info.rsn_ie.data, rsn_ie_len);
				ie_len += rsn_ie_len;
		}
	}
	else
		printk("ie_len too long !!!\n");
#if 1
	printk("[bss=%s] %03d 0x%08x 0x%02x %d %d %d\n", priv->pmib->dot11Bss.ssid, 
		channel->hw_value, timestamp, 
		priv->pmib->dot11Bss.capability, 
		priv->pmib->dot11Bss.beacon_prd, ie_len, 
		priv->pmib->dot11Bss.rssi);
#endif

	bss = cfg80211_inform_bss(wiphy, channel, mac, timestamp, 
			priv->pmib->dot11Bss.capability, 
			priv->pmib->dot11Bss.beacon_prd, 
			ie, ie_len, priv->pmib->dot11Bss.rssi, GFP_ATOMIC);

	if(bss)
		cfg80211_put_bss(wiphy, bss);
	else
		printk("bss = null\n");

}


void realtek_cfg80211_inform_ss_result(struct rtl8192cd_priv *priv)
{
	int i;
	struct wiphy *wiphy = priv->rtk->wiphy;
	struct ieee80211_channel *channel = NULL;
	struct ieee80211_bss *bss = NULL;

	printk("realtek_cfg80211_inform_ss_result count=%d\n", priv->site_survey->count);
	//printk("SSID                 BSSID        ch  prd cap  bsc  oper ss sq bd 40m\n");
	
	for(i=0; i<priv->site_survey->count; i++)
	{
		char tmpbuf[33];
		UINT8 *mac = priv->site_survey->bss[i].bssid;
		unsigned long timestamp = 0;
		unsigned char ie[MAX_IE_LEN];
		unsigned char ie_len = 0;
		unsigned char wpa_ie_len = priv->site_survey->wpa_ie[i].wpa_ie_len;
		unsigned char rsn_ie_len = priv->site_survey->rsn_ie[i].rsn_ie_len;
		unsigned char wps_ie_len = (priv->site_survey->wscie[i].data[1] + 2); //wrt-wps-clnt
		unsigned int  freq = 0;

		channel = rtk_get_iee80211_channel(wiphy, priv->site_survey->bss[i].channel);
		
		if(channel == NULL)
		{
			printk("Null channel!!\n");
			continue;
		}
		
		timestamp = priv->site_survey->bss[i].t_stamp[0] + (priv->site_survey->bss[i].t_stamp[0]<<32);

		ie[0]= _SSID_IE_;
		ie[1]= priv->site_survey->bss[i].ssidlen;
		memcpy(ie+2, priv->site_survey->bss[i].ssid, priv->site_survey->bss[i].ssidlen);
		ie_len += (priv->site_survey->bss[i].ssidlen + 2);
			
		if((ie_len + wpa_ie_len + rsn_ie_len) < MAX_IE_LEN)
		{
			if(wpa_ie_len)
			{
				memcpy(ie+ie_len, priv->site_survey->wpa_ie[i].data, wpa_ie_len);
				ie_len += wpa_ie_len;
			}

			if(rsn_ie_len)
			{
				memcpy(ie+ie_len, priv->site_survey->rsn_ie[i].data, rsn_ie_len);
				ie_len += rsn_ie_len;
			}
		}
		else
			printk("ie_len too long !!!\n");


#if 1 //wrt-wps-clnt
	if(wps_ie_len <= 2)
		wps_ie_len = 0;

	if((ie_len + wps_ie_len ) < MAX_IE_LEN)
	{
		if(wps_ie_len)
		{
			memcpy(ie+ie_len, priv->site_survey->wscie[i].data, wps_ie_len);
			ie_len += wps_ie_len;
		}
	
	}
	else
		printk("ie_len too long !!!\n");


#endif

#if 0
		printk("[%d=%s] %03d 0x%08x 0x%02x %d %d %d\n", i, priv->site_survey->bss[i].ssid, 
			channel->hw_value, timestamp, 
			priv->site_survey->bss[i].capability, 
			priv->site_survey->bss[i].beacon_prd, ie_len, 
			priv->site_survey->bss[i].rssi);
#endif

		bss = cfg80211_inform_bss(wiphy, channel, mac, timestamp, 
				priv->site_survey->bss[i].capability, 
				priv->site_survey->bss[i].beacon_prd, 
				ie, ie_len, priv->site_survey->bss[i].rssi, GFP_ATOMIC);

		if(bss)
			cfg80211_put_bss(wiphy, bss);
		else
			printk("bss = null\n");

	}
}


static void realtek_cfg80211_sscan_disable(struct rtl8192cd_priv *priv)
{
	//printk("realtek_cfg80211_sscan_disable +++ (TODO...)\n ");
	//priv->ss_req_ongoing = 0;	
	//cfg80211_sched_scan_stopped(wiphy);
}



#define HAPD_READY_RX_EVENT	5

int event_indicate_cfg80211(struct rtl8192cd_priv *priv, unsigned char *mac, int event, unsigned char *extra)
{
	struct net_device	*dev = (struct net_device *)priv->dev;
	struct stat_info	*pstat = NULL;
	struct station_info sinfo;
	struct wiphy *wiphy = priv->rtk->wiphy;

	NLENTER;

	if(event != CFG80211_SCAN_DONE) //eric-bb
	if( (OPMODE & WIFI_AP_STATE) && (priv->up_time <= HAPD_READY_RX_EVENT) ) 
	{
		printk("Not Ready, Do NOT report cfg event !! priv->up_time = %d \n", priv->up_time);
		return; 
	}

	if(mac)
		pstat = get_stainfo(priv, mac);

	if(1)//(OPMODE & WIFI_AP_STATE)
	{
		printk(" +++ [%s] cfg80211_event = %d\n", dev->name, event);

		if(event == CFG80211_CONNECT_RESULT)
		{
			struct cfg80211_bss *bss = NULL;
			struct ieee80211_channel *channel = NULL;
	
			channel = rtk_get_iee80211_channel(wiphy, priv->pmib->dot11Bss.channel);

			if(priv->receive_connect_cmd == 0)
			{
				printk("Not received connect cmd yet !! No report CFG80211_CONNECT_RESULT\n");
				return 0;
			}

			bss = cfg80211_get_bss(wiphy, 
					channel, priv->pmib->dot11Bss.bssid,
					priv->pmib->dot11Bss.ssid, priv->pmib->dot11Bss.ssidlen, 
					WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);

			if(bss==NULL)
			{
				printk("bss == NULL inform this bss !!\n");
				realtek_cfg80211_inform_bss(priv);
			}
	
			cfg80211_connect_result(priv->dev, BSSID,
					priv->rtk->clnt_info.assoc_req, priv->rtk->clnt_info.assoc_req_len,
					priv->rtk->clnt_info.assoc_rsp, priv->rtk->clnt_info.assoc_rsp_len,
					WLAN_STATUS_SUCCESS, GFP_KERNEL);
			
			return 0;
		}
		else if(event == CFG80211_ROAMED)
		{
				
			return 0;
		}
		else if(event == CFG80211_DISCONNECTED)
		{
			//_eric_nl ?? disconnect event no mac, for station mode only ??
			cfg80211_disconnected(priv->dev, 0, NULL, 0, GFP_KERNEL);
			return 0;
		}
		else if(event == CFG80211_IBSS_JOINED)
		{
			/*
				brian, fix compile error whilen migrate compatible wireless from 2013-11-05 to 2014-01-23.1
				cfg80211_ibss_joined(priv->dev, BSSID, GFP_KERNEL);
			*/
			cfg80211_ibss_joined(priv->dev, BSSID, NULL, GFP_KERNEL);
			return 0;
		}
		else if(event == CFG80211_NEW_STA)
		{	
			/* send event to application */
			memset(&sinfo, 0, sizeof(struct station_info));

			if(pstat == NULL)
			{
				printk("PSTA = NULL, MAC = %02x:%02x:%02x:%02x:%02x:%02x\n", 
					mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

				if(extra == NULL){
					printk("NO PSTA for CFG80211_NEW_STA\n");
					return -1;
				} else 
					pstat = extra;
			}
			
			/* TODO: sinfo.generation */
			sinfo.assoc_req_ies = pstat->wpa_ie;
			if(pstat->wpa_ie[1] > 0)
			sinfo.assoc_req_ies_len = pstat->wpa_ie[1]+2;
			else
				sinfo.assoc_req_ies_len = 0;
			
#if 1//wrt-wps
			if(pstat->wps_ie[1] > 0)
			{
				int wps_ie_len = pstat->wps_ie[1]+2;
					
				if(pstat->wpa_ie[1] > 0)
					memcpy(sinfo.assoc_req_ies+pstat->wpa_ie[1]+2, pstat->wps_ie, wps_ie_len);
				else
					memcpy(sinfo.assoc_req_ies, pstat->wps_ie, wps_ie_len);
					
				sinfo.assoc_req_ies_len += wps_ie_len;
			}
#endif

			if(sinfo.assoc_req_ies_len)
			sinfo.filled |= STATION_INFO_ASSOC_REQ_IES;
			
			printk("[%s][idx=%d] Rx assoc_req_ies_len = %d\n", 
				priv->dev->name, priv->dev->ifindex, sinfo.assoc_req_ies_len);
			printk("STA = %02x:%02x:%02x:%02x:%02x:%02x with wpa_ie: %02x %02x %02x ...\n", 
					mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] , 
					pstat->wpa_ie[0], pstat->wpa_ie[1], pstat->wpa_ie[2]);
			
			cfg80211_new_sta(priv->dev, mac, &sinfo, GFP_KERNEL);

			printk("cfg80211_new_sta --- \n");
			
			netif_wake_queue(priv->dev); //wrt-vap

			NLINFO;

			return 0;
		}
		else if(event == CFG80211_SCAN_DONE)
		{
			priv->ss_req_ongoing = 0;
			priv->site_survey->count_backup = priv->site_survey->count;
			memcpy(priv->site_survey->bss_backup, priv->site_survey->bss, sizeof(struct bss_desc)*priv->site_survey->count);
			
			if (priv->scan_req) {
				cfg80211_scan_done(priv->scan_req, false);
				priv->scan_req = NULL;
			}
		}
		else if(event == CFG80211_DEL_STA)
		{
			cfg80211_del_sta(priv->dev, mac, GFP_KERNEL);
		}
		else 
			printk("Unknown Event !!\n");
	}

	return -1;
}


#endif

#if 0
void realtek_ap_calibration(struct rtl8192cd_priv	*priv)
{
	NLENTER;
	
#if 0
	unsigned char CCK_A[3] = {0x2a,0x2a,0x28};
	unsigned char CCK_B[3] = {0x2a,0x2a,0x28};
	unsigned char HT40_A[3] = {0x2b,0x2b,0x29};
	unsigned char HT40_B[3] = {0x2b,0x2b,0x29};
	unsigned char DIFF_HT40_2S[3] = {0x0,0x0,0x0};
	unsigned char DIFF_20[3] = {0x02,0x02,0x02};
	unsigned char DIFF_OFDM[3] = {0x04,0x04,0x04};
	unsigned int thermal = 0x19;
	unsigned int crystal = 32;
#else
	unsigned char CCK_A[3] = {0x2b,0x2a,0x29};
	unsigned char CCK_B[3] = {0x2b,0x2a,0x29};
	unsigned char HT40_A[3] = {0x2c,0x2b,0x2a};
	unsigned char HT40_B[3] = {0x2c,0x2b,0x2a};
	unsigned char DIFF_HT40_2S[3] = {0x0,0x0,0x0};
	unsigned char DIFF_20[3] = {0x02,0x02,0x02};
	unsigned char DIFF_OFDM[3] = {0x04,0x04,0x04};
	unsigned int thermal = 0x16;
	unsigned int crystal = 32;
#endif

	int tmp = 0;
	int tmp2 = 0;

	for(tmp = 0; tmp <=13; tmp ++)
	{
		if(tmp < 3)
			tmp2 = 0;
		else if(tmp < 9)
			tmp2 = 1;
		else
			tmp2 = 2;
	
		priv->pmib->dot11RFEntry.pwrlevelCCK_A[tmp] = CCK_A[tmp2];
		priv->pmib->dot11RFEntry.pwrlevelCCK_B[tmp] = CCK_B[tmp2];
		priv->pmib->dot11RFEntry.pwrlevelHT40_1S_A[tmp] = HT40_A[tmp2];
		priv->pmib->dot11RFEntry.pwrlevelHT40_1S_B[tmp] = HT40_B[tmp2];
		priv->pmib->dot11RFEntry.pwrdiffHT40_2S[tmp] = DIFF_HT40_2S[tmp2];
		priv->pmib->dot11RFEntry.pwrdiffHT20[tmp] = DIFF_20[tmp2];
		priv->pmib->dot11RFEntry.pwrdiffOFDM[tmp] = DIFF_OFDM[tmp2];
	}

	priv->pmib->dot11RFEntry.ther = thermal;
	priv->pmib->dot11RFEntry.xcap = crystal;

	NLEXIT;
}
#endif

//eric-vap
unsigned char is_WRT_scan_iface(unsigned char* if_name)
{
	if((strcmp(if_name, "tmp.wlan0")==0) || (strcmp(if_name, "tmp.wlan1")==0))
		return 1;
	else
		return 0;
}

void dump_mac(struct rtl8192cd_priv *priv, unsigned char *mac)
{
	if(mac)
		printk(" %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

//mark_swc	
static void rtk_set_phy_channel(struct rtl8192cd_priv *priv,unsigned int channel,unsigned int bandwidth,unsigned int chan_offset)
{
	//priv , share  part
	priv->pshare->CurrentChannelBW = bandwidth;
	priv->pshare->offset_2nd_chan =chan_offset ;

	// wifi chanel  hw settting  API
	SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
	SwChnl(priv, channel, priv->pshare->offset_2nd_chan);
	//printk("rtk_set_phy_channel end !!!\n  chan=%d \n",channel );

}



static void rtk_get_band_capa(struct rtl8192cd_priv *priv,bool *band_2gig ,bool *band_5gig)
{
	*band_2gig = true;				
	*band_5gig = false;
	
	if (GET_CHIP_VER(priv) == VERSION_8812E) 
	{
		*band_2gig = false;	
		*band_5gig = true;
	}	
	else if (GET_CHIP_VER(priv) == VERSION_8192D) 
	{
		*band_2gig = false;	
		*band_5gig = true;
	}
	else if (GET_CHIP_VER(priv) == VERSION_8881A)
	{
#if defined(CONFIG_RTL_8881A_SELECTIVE)
		//8881A selective mode
		*band_2gig = true;
		*band_5gig = true;
#else
		//use pcie slot 0 for 2.4G 88E/92E, 8881A is 5G now
		*band_2gig = false;
		*band_5gig = true;
#endif
	}	
	//mark_sel
	//if 881a , then it is possible to  *band_2gig = true ,*band_5gig = true in selective mode(FLAG?)
	//FIXME 
}

void realtek_ap_default_config(struct rtl8192cd_priv *priv) 
{

#if 0  //move to set_channel
	bool band_2gig = false, band_5gig = false;

	rtk_get_band_capa(priv,&band_2gig,&band_5gig);

	if(band_2gig)
		priv->pmib->dot11BssType.net_work_type = WIRELESS_11B|WIRELESS_11G;
	if(band_5gig)
		priv->pmib->dot11BssType.net_work_type = WIRELESS_11A;

	priv->pmib->dot11BssType.net_work_type |= WIRELESS_11N;
#endif	
	
	//short GI default
	priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M = 1;
	priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M = 1;

	//APMDU
	priv->pmib->dot11nConfigEntry.dot11nAMPDU = 1;	

#ifdef MBSSID
	if(IS_ROOT_INTERFACE(priv))
	{
		priv->pmib->miscEntry.vap_enable = 1; //eric-vap //eric-brsc
	}
	else
	{	
#if 0
		if(IS_VAP_INTERFACE(priv))
		{
			struct rtl8192cd_priv *priv_root = GET_ROOT(priv);
			struct rtl8192cd_priv *priv_vxd = GET_VXD_PRIV(priv_root);
			unsigned char is_vxd_running = 0; 

			if(priv_vxd)
				is_vxd_running = netif_running(priv_vxd->dev);
			
			if(priv_root->pmib->miscEntry.vap_enable == 0)
			{
				priv_root->pmib->miscEntry.vap_enable = 1;

				if(is_vxd_running)
				rtl8192cd_close(priv_vxd->dev);
				
				rtl8192cd_close(priv_root->dev);
				rtl8192cd_open(priv_root->dev);	

				if(is_vxd_running)
				rtl8192cd_open(priv_vxd->dev);	

			}
		}
#endif	
		//vif copy settings from root
		priv->pmib->dot11BssType.net_work_type = GET_ROOT(priv)->pmib->dot11BssType.net_work_type;
		priv->pmib->dot11RFEntry.phyBandSelect = GET_ROOT(priv)->pmib->dot11RFEntry.phyBandSelect;
		priv->pmib->dot11RFEntry.dot11channel = GET_ROOT(priv)->pmib->dot11RFEntry.dot11channel; 
		priv->pmib->dot11nConfigEntry.dot11nUse40M = GET_ROOT(priv)->pmib->dot11nConfigEntry.dot11nUse40M;
		priv->pmib->dot11nConfigEntry.dot11n2ndChOffset = GET_ROOT(priv)->pmib->dot11nConfigEntry.dot11n2ndChOffset;
	}
#endif

}

//mark_priv
#define RTK_PRIV_BW_5M 1 
#define RTK_PRIV_BW_10M 2
#define RTK_PRIV_BW_80M_MINUS 3
#define RTK_PRIV_BW_80M_PLUS 4

static inline int is_hw_vht_support(struct rtl8192cd_priv *priv)
{	
	int support=0;
	
	if (GET_CHIP_VER(priv) == VERSION_8812E) 
		support=1;
	else if (GET_CHIP_VER(priv) == VERSION_8881A) 
		support=1;

	return support;
}
//priv low bandwidth
static inline int is_hw_lbw_support(struct rtl8192cd_priv *priv)
{	
	int support=0;
	
	if (GET_CHIP_VER(priv) == VERSION_8812E) 
		support=1;
#if defined(CONFIG_WLAN_HAL_8192EE)
	if ((GET_CHIP_VER(priv) == VERSION_8192E) && (_GET_HAL_DATA(priv)->cutVersion == ODM_CUT_C))
		support=1;
#endif
	if(!support)
		printk("This IC NOT support 5M10M !! \n");

	return support;
}

static inline int convert_privBW(char *str_bw) //mark_priv
{
	int priv_bw=0;

    if(!strcmp(str_bw,"5M"))
		priv_bw = RTK_PRIV_BW_5M;
    else if(!strcmp(str_bw,"10M"))	
		priv_bw = RTK_PRIV_BW_10M;
    //future 160M 
   	
    return priv_bw;
}

int check_5M10M_config(struct rtl8192cd_priv *priv)
{

	int priv_bw=0;	
	int ret = 0;

	priv_bw = convert_privBW(priv->pshare->rf_ft_var.rtk_uci_PrivBandwidth);
	//printk("rtk_set_channel_mode , priv_band= %s , val=%d \n", priv->pshare->rf_ft_var.rtk_uci_PrivBandwidth, priv_bw);

	//first check if priv_band is set			  
	if(priv_bw)
	{
		//check 5/10M	
		if( (priv_bw == RTK_PRIV_BW_10M) && is_hw_lbw_support(priv))
		{
			priv->pmib->dot11nConfigEntry.dot11nUse40M = HT_CHANNEL_WIDTH_10;
			priv->pmib->dot11nConfigEntry.dot11n2ndChOffset = HT_2NDCH_OFFSET_DONTCARE;
			ret = 1;
			printk("Force config bandwidth=10M\n");
		}
		else if( (priv_bw == RTK_PRIV_BW_5M) && is_hw_lbw_support(priv))			
		{
			priv->pmib->dot11nConfigEntry.dot11nUse40M = HT_CHANNEL_WIDTH_5;
			priv->pmib->dot11nConfigEntry.dot11n2ndChOffset = HT_2NDCH_OFFSET_DONTCARE;
			ret = 1;
			printk("Force config bandwidth=5M\n");
		}
		else			
			printk("No such priv channel type !!!\n");
		
	}

	return ret;

}

static void rtk_set_band_mode(struct rtl8192cd_priv *priv,enum ieee80211_band band ,enum nl80211_chan_width channel_width) 
{
	if(band == IEEE80211_BAND_2GHZ)
	{
		priv->pmib->dot11BssType.net_work_type = WIRELESS_11B|WIRELESS_11G;
		priv->pmib->dot11RFEntry.phyBandSelect = PHY_BAND_2G;
	}
	else if(band == IEEE80211_BAND_5GHZ)
	{
		priv->pmib->dot11BssType.net_work_type = WIRELESS_11A;
		priv->pmib->dot11RFEntry.phyBandSelect = PHY_BAND_5G;
	}
    if(channel_width != NL80211_CHAN_WIDTH_20_NOHT)
		priv->pmib->dot11BssType.net_work_type |= WIRELESS_11N;

	//printk("rtk_set_band_mode , ac_enable =%d \n", (channel_width = NL80211_CHAN_WIDTH_80)? 1:0);
	if(channel_width == NL80211_CHAN_WIDTH_80)
		priv->pmib->dot11BssType.net_work_type |= WIRELESS_11AC;

#ifdef UNIVERSAL_REPEATER
	if(IS_ROOT_INTERFACE(priv) && priv->pvxd_priv)
	{
		priv->pvxd_priv->pmib->dot11BssType.net_work_type = priv->pmib->dot11BssType.net_work_type;
		priv->pvxd_priv->pmib->dot11RFEntry.phyBandSelect = priv->pmib->dot11RFEntry.phyBandSelect;
	}
#endif
}

static void rtk_set_channel_mode(struct rtl8192cd_priv *priv, struct cfg80211_chan_def *chandef)
{
	int config_BW5m10m=0;

	config_BW5m10m = check_5M10M_config(priv);

	//printk("rtk_set_channel_mode , priv_band= %s , val=%d \n", priv->pshare->rf_ft_var.rtk_uci_PrivBandwidth, priv_bw);

	//first check if priv_band is set  			  
	if(!config_BW5m10m)
	{
		//normal channel setup path from cfg80211
		if(chandef->width == NL80211_CHAN_WIDTH_40) {
            //printk("NL80211_CHAN_WIDTH_40\n");
			if (chandef->center_freq1 > chandef->chan->center_freq) {
                //printk("NL80211_CHAN_WIDTH_40-PLUS\n");
				priv->pmib->dot11nConfigEntry.dot11nUse40M = HT_CHANNEL_WIDTH_20_40;
				priv->pmib->dot11nConfigEntry.dot11n2ndChOffset = HT_2NDCH_OFFSET_ABOVE; //above
			} else {
				//printk("NL80211_CHAN_WIDTH_40-MINUS\n");
				priv->pmib->dot11nConfigEntry.dot11nUse40M = HT_CHANNEL_WIDTH_20_40;
				priv->pmib->dot11nConfigEntry.dot11n2ndChOffset = HT_2NDCH_OFFSET_BELOW; //below
			}
		} else if(chandef->width == NL80211_CHAN_WIDTH_80) {
			//printk("NL80211_CHAN_WIDTH_80\n");
			priv->pmib->dot11nConfigEntry.dot11nUse40M = HT_CHANNEL_WIDTH_80;
			priv->pmib->dot11nConfigEntry.dot11n2ndChOffset = HT_2NDCH_OFFSET_DONTCARE; //dontcare
		} else {
#if 0
			if(chandef->width == NL80211_CHAN_WIDTH_20 || chandef->width == NL80211_CHAN_WIDTH_20_NOHT)
				printk("NL80211_CHAN_WIDTH_20\/NL80211_CHAN_WIDTH_20_NOHT\n");
            else
                printk("Unknown bandwidth: %d, use 20Mhz be default\n", chandef->width);
#endif
			priv->pmib->dot11nConfigEntry.dot11nUse40M = HT_CHANNEL_WIDTH_20;
			priv->pmib->dot11nConfigEntry.dot11n2ndChOffset = HT_2NDCH_OFFSET_DONTCARE;
		}
	}
}

void realtek_ap_config_apply(struct rtl8192cd_priv	*priv)
{
	NLENTER;
	rtl8192cd_close(priv->dev);
	rtl8192cd_open(priv->dev);	

#ifdef UNIVERSAL_REPEATER
	if(IS_ROOT_INTERFACE(priv) && priv->pvxd_priv)
	{
		rtl8192cd_close(priv->pvxd_priv->dev);
		rtl8192cd_open(priv->pvxd_priv->dev);	
	}
#endif

}

int realtek_cfg80211_ready(struct rtl8192cd_priv	*priv)
{

	if (netif_running(priv->dev))
		return 1;
	else
		return 0;
}


void realtek_auth_none(struct rtl8192cd_priv *priv)
{

	NLENTER; 
	priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm = 0;
	priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = 0;
	priv->pmib->dot1180211AuthEntry.dot11EnablePSK = 0;
	priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm = 0;
	priv->pmib->dot1180211AuthEntry.dot11WPACipher = 0;
	priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher = 0;
	

}

void realtek_auth_wep(struct rtl8192cd_priv *priv, int cipher)
{
	//_eric_nl ?? wep auto/shared/open ??
	NLENTER;
	priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm = 2;
	priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = cipher;
	priv->pmib->dot1180211AuthEntry.dot11EnablePSK = 0;
	priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm = 0;
	priv->pmib->dot1180211AuthEntry.dot11WPACipher = 0;
	priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher = 0;

}

void realtek_auth_wpa(struct rtl8192cd_priv *priv, int wpa, int psk, int cipher)
{
	int wpa_cipher;

	// bit0-wep64, bit1-tkip, bit2-wrap,bit3-ccmp, bit4-wep128
	if(cipher & _TKIP_PRIVACY_)
		wpa_cipher |= BIT(1);
	if(cipher & _CCMP_PRIVACY_)
		wpa_cipher |= BIT(3);
	
	NLENTER;
	//printk("wpa=%d psk=%d wpa_cipher=0x%x\n", wpa, psk, wpa_cipher);
	priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm = 2;
	priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = 2;

	if(psk)
	priv->pmib->dot1180211AuthEntry.dot11EnablePSK = wpa;
	
	priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm = 1;

	if(wpa& BIT(0))
	priv->pmib->dot1180211AuthEntry.dot11WPACipher = wpa_cipher;
	if(wpa& BIT(1))
	priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher = wpa_cipher;

}


void realtek_set_security(struct rtl8192cd_priv *priv, struct rtknl *rtk, struct cfg80211_crypto_settings crypto)
{
	int wpa = 0;
	int psk = 0;
	int cipher = 0;
	int i = 0;

 	realtek_auth_none(priv);
	//printk("n_akm_suites=%d n_ciphers_pairwise=%d \n", crypto.n_akm_suites, crypto.n_ciphers_pairwise);
	for (i = 0; i < crypto.n_akm_suites; i++) {
		switch (crypto.akm_suites[i]) {
		case WLAN_AKM_SUITE_8021X:
			psk = 0;
			if (crypto.wpa_versions & NL80211_WPA_VERSION_1)
				wpa |= BIT(0);
			if (crypto.wpa_versions & NL80211_WPA_VERSION_2)
				wpa |= BIT(1);
			break;
		case WLAN_AKM_SUITE_PSK:
			psk = 1;
			if (crypto.wpa_versions & NL80211_WPA_VERSION_1)
				wpa |= BIT(0);
			if (crypto.wpa_versions & NL80211_WPA_VERSION_2)
				wpa |= BIT(1);
			break;
		}
	}
	
//_eric_nl ?? multiple ciphers ??
	for (i = 0; i < crypto.n_ciphers_pairwise; i++) {
		switch (crypto.ciphers_pairwise[i]) {
		case WLAN_CIPHER_SUITE_WEP40:
			rtk->cipher = WLAN_CIPHER_SUITE_WEP40;
			realtek_auth_wep(priv, _WEP_40_PRIVACY_);
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			rtk->cipher = WLAN_CIPHER_SUITE_WEP104;
			realtek_auth_wep(priv, _WEP_104_PRIVACY_);
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			rtk->cipher |= WLAN_CIPHER_SUITE_TKIP;
			cipher |= _TKIP_PRIVACY_;
			//printk("TKIP(%d)\n", i);
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			rtk->cipher |= WLAN_CIPHER_SUITE_CCMP;
			cipher |= _CCMP_PRIVACY_;
			//printk("CCMP(%d)\n", i);
			break;
		}
	}

	if(wpa)
		realtek_auth_wpa(priv, wpa, psk, cipher);

#if 1
	switch (crypto.cipher_group) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			//printk("WEP GROUP\n");
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			//printk("TKIP GROUP\n");
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			//printk("CCMP GROUP\n");
			break;
		case WLAN_CIPHER_SUITE_SMS4:
			//printk("WAPI GROUP\n");
			break;
		default:
			//printk("NONE GROUP\n");
			break;
	}
#endif
}

unsigned int realtek_get_key_from_sta(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct key_params *params)
{
	unsigned int cipher = 0; 
	struct Dot11EncryptKey *pEncryptKey;

	//_eric_cfg ?? key len data seq for get_key ??
	if(pstat == NULL)
	{
		cipher = priv->pmib->dot11GroupKeysTable.dot11Privacy;
		pEncryptKey = &priv->pmib->dot11GroupKeysTable.dot11EncryptKey;
	}
	else
	{
		cipher = pstat->dot11KeyMapping.dot11Privacy;
		pEncryptKey = &pstat->dot11KeyMapping.dot11EncryptKey;
	}

	switch (cipher) {
	case _WEP_40_PRIVACY_:
		params->cipher = WLAN_CIPHER_SUITE_WEP40;
		params->key_len = 5;
		memcpy(params->key, pEncryptKey->dot11TTKey.skey, pEncryptKey->dot11TTKeyLen);
	case _WEP_104_PRIVACY_:
		params->cipher = WLAN_CIPHER_SUITE_WEP104;
		params->key_len = 10;
		memcpy(params->key, pEncryptKey->dot11TTKey.skey, pEncryptKey->dot11TTKeyLen);
	case _CCMP_PRIVACY_:
		params->cipher = WLAN_CIPHER_SUITE_CCMP;
		params->key_len = 32;
		memcpy(params->key, pEncryptKey->dot11TTKey.skey, pEncryptKey->dot11TTKeyLen);
		memcpy(params->key+16, pEncryptKey->dot11TMicKey1.skey, pEncryptKey->dot11TMicKeyLen);
	case _TKIP_PRIVACY_:
		params->cipher = WLAN_CIPHER_SUITE_TKIP;
		params->key_len = 32;
		memcpy(params->key, pEncryptKey->dot11TTKey.skey, pEncryptKey->dot11TTKeyLen);
		memcpy(params->key+16, pEncryptKey->dot11TMicKey1.skey, pEncryptKey->dot11TMicKeyLen);
		memcpy(params->key+24, pEncryptKey->dot11TMicKey2.skey, pEncryptKey->dot11TMicKeyLen);
	default:
		return -ENOTSUPP;
	}
}


#if 1 //wrt-wps
void clear_wps_ies(struct rtl8192cd_priv *priv)
{
	priv->pmib->wscEntry.wsc_enable = 0;
	
	priv->pmib->wscEntry.beacon_ielen = 0;
	priv->pmib->wscEntry.probe_rsp_ielen = 0;
	priv->pmib->wscEntry.probe_req_ielen = 0;
	priv->pmib->wscEntry.assoc_ielen = 0;
}


void copy_wps_ie(struct rtl8192cd_priv *priv, unsigned char *wps_ie, unsigned char mgmt_type)
{
	unsigned int wps_ie_len = (wps_ie[1] + 2);

	if(wps_ie_len > 256)
	{
		printk("WPS_IE length > 256 !! Can NOT copy !!\n");
		return;
	}

	if(OPMODE & WIFI_AP_STATE)
		priv->pmib->wscEntry.wsc_enable = 2; //Enable WPS for AP mode
	else if(OPMODE & WIFI_STATION_STATE)
		priv->pmib->wscEntry.wsc_enable = 1;

	if (mgmt_type == MGMT_BEACON) {
		printk("WSC: set beacon IE\n");
		priv->pmib->wscEntry.beacon_ielen = wps_ie_len;
		memcpy((void *)priv->pmib->wscEntry.beacon_ie, wps_ie, wps_ie_len);
	}
	else if (mgmt_type == MGMT_PROBERSP) {
		printk("WSC: set probe response IE\n");
		priv->pmib->wscEntry.probe_rsp_ielen = wps_ie_len;
		memcpy((void *)priv->pmib->wscEntry.probe_rsp_ie, wps_ie, wps_ie_len);
	}
	else if ((mgmt_type == MGMT_ASSOCRSP) || (mgmt_type == MGMT_ASSOCREQ)) { //wrt-wps-clnt
		printk("WSC: set association IE\n");
		priv->pmib->wscEntry.assoc_ielen = wps_ie_len;
		memcpy((void *)priv->pmib->wscEntry.assoc_ie, wps_ie, wps_ie_len);
	}
}

void dump_ies(struct rtl8192cd_priv *priv, 
					unsigned char *pies, unsigned int ies_len, unsigned char mgmt_type)
{
	unsigned char *pie = pies;
	unsigned int len, total_len = 0;
	int i = 0;

	while(1)
	{
		len = pie[1];

		total_len += (len+2);	
		if(total_len > ies_len)
		{
			printk("Exceed !!\n");
			break;
		}

		if(pie[0] == _WPS_IE_)
			copy_wps_ie(priv, pie, mgmt_type);
		
		printk("[Tag=0x%02x Len=%d(0x%x)]\n", pie[0], len, len);
		pie+=2; 
		
#if 0		
		for(i=0; i<len; i++)
		{
			if((i%10) == 9)
				printk("\n");
			
			printk("%02x ", pie[i]);
		}

		printk("\n");
#endif

		pie+=len;
		
		if(total_len == ies_len)
		{
			//printk("Done \n");
			break;
		}
		
	}

}
#endif



void realtek_set_ies(struct rtl8192cd_priv *priv, struct cfg80211_beacon_data *info)
{

	clear_wps_ies(priv);

	if(info->beacon_ies)
	{
		printk("beacon_ies_len = %d \n", info->beacon_ies_len);
		dump_ies(priv, info->beacon_ies, info->beacon_ies_len, MGMT_BEACON);
	}
	if(info->proberesp_ies)
	{
		printk("proberesp_ies_len = %d \n", info->proberesp_ies_len);
		dump_ies(priv, info->proberesp_ies, info->proberesp_ies_len, MGMT_PROBERSP);
	}
	if(info->assocresp_ies)
	{
		printk("assocresp_ies_len = %d \n", info->assocresp_ies_len);
		dump_ies(priv, info->assocresp_ies, info->assocresp_ies_len, MGMT_ASSOCRSP);
	}

}

static int realtek_set_ssid(struct rtl8192cd_priv *priv, struct cfg80211_ap_settings *info)
{
	//printk("SSID = %s \n", info->ssid);

	memcpy(SSID, info->ssid, info->ssid_len);	
	SSID_LEN = info->ssid_len;
	
	switch(info->hidden_ssid)
	{	
		case NL80211_HIDDEN_SSID_NOT_IN_USE:
			HIDDEN_AP=0;
			break;
		case NL80211_HIDDEN_SSID_ZERO_CONTENTS:
			HIDDEN_AP=1;
			break;
		case NL80211_HIDDEN_SSID_ZERO_LEN:/*firmware doesn't support this type of hidden ssid*/
		default:
			return -EOPNOTSUPP;  
	}

	return 0;
}

static int realtek_set_auth_type(struct rtl8192cd_priv *priv, enum nl80211_auth_type auth_type)
{
	//printk("%s: 0x%x\n", __func__, auth_type);
	
	switch (auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		//printk("%s: 0x%x NL80211_AUTHTYPE_OPEN_SYSTEM \n", __func__, auth_type);
			break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		//printk("%s: 0x%x NL80211_AUTHTYPE_SHARED_KEY \n", __func__, auth_type);
			break;
	case NL80211_AUTHTYPE_NETWORK_EAP:
		//printk("%s: 0x%x NL80211_AUTHTYPE_NETWORK_EAP \n", __func__, auth_type);
			break;
	case NL80211_AUTHTYPE_AUTOMATIC:
		//printk("%s: 0x%x NL80211_AUTHTYPE_AUTOMATIC \n", __func__, auth_type);
			break;

	default:
		printk("%s: auth_type=0x%x not supported\n", __func__, auth_type);
		return -ENOTSUPP;
		}

	return 0;
}



static int realtek_change_beacon(struct wiphy *wiphy, struct net_device *dev,
				struct cfg80211_beacon_data *beacon)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	
	if (!realtek_cfg80211_ready(priv))
		return -EIO;

	if ((OPMODE & WIFI_AP_STATE) == 0)
		return -EOPNOTSUPP;

	realtek_set_ies(priv, beacon);

	NLEXIT;

	return 0; 

	}

static int realtek_cfg80211_del_beacon(struct wiphy *wiphy, struct net_device *dev)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;

	if (OPMODE & WIFI_AP_STATE == 0)
		return -EOPNOTSUPP;
	if (priv->assoc_num == 0)
		return -ENOTCONN;

	rtl8192cd_close(priv->dev);

	NLEXIT;
	return 0;
}


static int realtek_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	int ret = 0;

	NLENTER;

	ret = realtek_cfg80211_del_beacon(wiphy, dev);

	NLEXIT;
	return ret;
}

#if 0
static int realtek_cfg80211_add_beacon(struct wiphy *wiphy, struct net_device *dev,
				struct beacon_parameters *info)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	
	realtek_ap_beacon(wiphy, dev, info, true);
	realtek_ap_config_apply(priv);

	NLEXIT;
	return 0;
}

//_eric_nl ?? what's the diff between st & add beacon??
static int realtek_cfg80211_set_beacon(struct wiphy *wiphy, struct net_device *dev,
				struct beacon_parameters *info)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;

	realtek_ap_beacon(wiphy, dev, info, false);
	
	NLEXIT;
	return 0;

}

//_eric_nl ?? what's the purpose of del_beacon ??
static int realtek_cfg80211_del_beacon(struct wiphy *wiphy, struct net_device *dev)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;

	if (OPMODE & WIFI_AP_STATE == 0)
		return -EOPNOTSUPP;
	if (priv->assoc_num == 0)
		return -ENOTCONN;

	rtl8192cd_close(priv->dev);

	NLEXIT;
	return 0;
}
#endif


static int realtek_cfg80211_set_channel(struct wiphy *wiphy, struct net_device *dev,
			      struct cfg80211_chan_def *chandef)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);
	int channel = 0;

	NLENTER;

	channel = ieee80211_frequency_to_channel(chandef->chan->center_freq);
	
	//printk("%s: center_freq=%u channel=%d hw_value=%u bandwidth =%d\n", __func__, 
		//chandef->chan->center_freq, channel, chandef->chan->hw_value, chandef->width);

	priv->pmib->dot11RFEntry.dot11channel = channel;

	rtk_set_band_mode(priv,chandef->chan->band , chandef->width);
	rtk_set_channel_mode(priv,chandef);

	realtek_ap_default_config(priv);
	realtek_ap_config_apply(priv);

	return 0;
}


static int realtek_start_ap(struct wiphy *wiphy, struct net_device *dev,
			   struct cfg80211_ap_settings *info)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	int ret = 0;

	NLENTER;

	if (!realtek_cfg80211_ready(priv))
		return -EIO;

	if ((OPMODE & (WIFI_AP_STATE | WIFI_ADHOC_STATE)) == 0) //wrt-adhoc
		return -EOPNOTSUPP;

	if (info->ssid == NULL)
		return -EINVAL;

	realtek_cfg80211_set_channel(wiphy, dev, &info->chandef);

	realtek_set_ies(priv, &info->beacon);

	ret = realtek_set_ssid(priv, info);

	ret = realtek_set_auth_type(priv, info->auth_type);
	
	realtek_set_security(priv, rtk, info->crypto);

	realtek_ap_default_config(priv);
	
	realtek_ap_config_apply(priv);

	NLEXIT;
	
	return ret;

}



//Not in ath6k
static int realtek_cfg80211_change_bss(struct wiphy *wiphy,
				struct net_device *dev,
				struct bss_parameters *params)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	
	unsigned char dot11_rate_table[]={2,4,11,22,12,18,24,36,48,72,96,108,0};

	NLENTER;

#if 0
if (params->use_cts_prot >= 0) {
	sdata->vif.bss_conf.use_cts_prot = params->use_cts_prot;
	changed |= BSS_CHANGED_ERP_CTS_PROT;
}
#endif

	priv->pmib->dot11RFEntry.shortpreamble = params->use_short_preamble;

#if 0
if (params->use_short_slot_time >= 0) {
	sdata->vif.bss_conf.use_short_slot =
		params->use_short_slot_time;
	changed |= BSS_CHANGED_ERP_SLOT;
}
#endif

	if (params->basic_rates) {
		int i, j;
		u32 rates = 0;

		//printk("rate = ");
		for (i = 0; i < params->basic_rates_len; i++) {
			int rate = params->basic_rates[i];
			//printk("%d ", rate);

			for (j = 0; j < 13; j++) {
				if ((dot11_rate_table[j]) == rate)
				{
					//printk("BIT(%d) ", j);
					rates |= BIT(j);
				}

			}
		}
		//printk("\n");
		priv->pmib->dot11StationConfigEntry.dot11BasicRates = rates;
	}

	return 0;
}



#if 0

static int realtek_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
			     u8 key_idx, const u8 *mac_addr,
			     struct key_params *params)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv	*priv = rtk->priv;

	NLENTER;
	return 0;

}


static int realtek_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
			     u8 key_idx, const u8 *mac_addr)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv	*priv = rtk->priv;

	NLENTER;
	return 0;
}

static int realtek_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
			     u8 key_idx, const u8 *mac_addr, void *cookie,
			     void (*callback)(void *cookie,
					      struct key_params *params))
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv	*priv = rtk->priv;

	NLENTER;
	return 0;
}

#else


void set_pairwise_key_for_ibss(struct rtl8192cd_priv *priv, union iwreq_data *wrqu)
{
	int i = 0;
	struct stat_info *pstat = NULL;
	struct ieee80211req_key *wk = (struct ieee80211req_key *)wrqu->data.pointer;

	printk("set_pairwise_key_for_ibss +++ \n");
	
	for(i=0; i<NUM_STAT; i++)
	{
		if(priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == TRUE)) 
		{
			pstat = get_stainfo(priv, priv->pshare->aidarray[i]->station.hwaddr);

			if(pstat)
			{
				memcpy(wk->ik_macaddr, priv->pshare->aidarray[i]->station.hwaddr, ETH_ALEN);
				rtl_net80211_setkey(priv->dev, NULL, wrqu, NULL);
			}
		}
	}
}


#define TOTAL_CAM_ENTRY(priv) (priv->pshare->total_cam_entry)
#ifdef CPTCFG_CFG80211_MODULE
static int realtek_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr,
				   struct key_params *params)			   
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	union iwreq_data wrqu;
	struct ieee80211req_key wk;

	NLENTER;

	if (!realtek_cfg80211_ready(priv))
		return -EIO;

	if (key_index > TOTAL_CAM_ENTRY(priv)) {
		printk("%s: key index %d out of bounds\n", __func__, key_index);
		return -ENOENT;
	}

#if 0
	if(mac_addr == NULL) {
		printk("NO MAC Address !!\n");
		return -ENOENT;;
	}
#endif

	memset(&wk, 0, sizeof(struct ieee80211req_key));

	wk.ik_keyix = key_index;

	if(mac_addr != NULL)
		memcpy(wk.ik_macaddr, mac_addr, ETH_ALEN);
	else
		memset(wk.ik_macaddr, 0, ETH_ALEN);

#if 1
	if (!pairwise) //in rtl_net80211_setkey(), group identification is by mac address
	{
		unsigned char	MULTICAST_ADD[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

		memcpy(wk.ik_macaddr, MULTICAST_ADD, ETH_ALEN);
	}
#endif

	switch (params->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		wk.ik_type = IEEE80211_CIPHER_WEP;
		//printk("WEP !!\n");
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		wk.ik_type = IEEE80211_CIPHER_TKIP;
		//printk("TKIP !!\n");
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		wk.ik_type = IEEE80211_CIPHER_AES_CCM;
		//printk("AES !!\n");
		break;
	default:
		return -EINVAL;
	}

#if 0
	switch (rtk->cipher) { //_eric_cfg ?? mixed mode ?? 
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		wk.ik_type = IEEE80211_CIPHER_WEP;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		wk.ik_type = IEEE80211_CIPHER_TKIP;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		wk.ik_type = IEEE80211_CIPHER_AES_CCM;
		break;
	default:
		return -ENOTSUPP;
	}
#endif
	wk.ik_keylen = params->key_len;
	memcpy(wk.ik_keydata, params->key, params->key_len);

#if 0
{
	int tmp = 0;
	printk("keylen = %d: ", wk.ik_keylen);
	for(tmp = 0; tmp < wk.ik_keylen; tmp ++)		
		printk("%02x ", wk.ik_keydata[tmp]);
	printk("\n");
}

	//_eric_cfg ?? key seq is not used ??
	
	printk("[%s] add keyid = %d, mac = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			priv->dev->name , wk.ik_keyix, wk.ik_macaddr[0], wk.ik_macaddr[1], wk.ik_macaddr[2], 
				wk.ik_macaddr[3], wk.ik_macaddr[4], wk.ik_macaddr[5]);
	printk("type = 0x%x, flags = 0x%x, keylen = 0x%x \n"
			, wk.ik_type, wk.ik_flags, wk.ik_keylen);
#endif


	wrqu.data.pointer = &wk;
	
	rtl_net80211_setkey(priv->dev, NULL, &wrqu, NULL);

#if 1 //wrt-adhoc
	if(OPMODE & WIFI_ADHOC_STATE)
	{
		if(!pairwise)
			set_pairwise_key_for_ibss(priv, &wrqu);	//or need to apply set_default_key
	}
#endif

	NLEXIT;
	return 0;

}

static int realtek_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr)			   
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	union iwreq_data wrqu;
	struct ieee80211req_del_key wk;

	NLENTER;

	if(!pairwise)
	{
		printk("No need to delete Groupe key !!\n");
		return 0;	
	}


	if (!realtek_cfg80211_ready(priv))
		return -EIO;

	if (key_index > TOTAL_CAM_ENTRY(priv)) {
		printk("%s: key index %d out of bounds\n", __func__, key_index);
		return -ENOENT;
	}

	
 	memset(&wk, 0, sizeof(struct ieee80211req_del_key));

	wk.idk_keyix = key_index;

	if(mac_addr != NULL)
		memcpy(wk.idk_macaddr, mac_addr, ETH_ALEN);
	else
		memset(wk.idk_macaddr, 0, ETH_ALEN);

#if 0
	if (!pairwise) //in rtl_net80211_delkey(), group identification is by mac address
	{
		unsigned char	MULTICAST_ADD[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
		unsigned char	GROUP_ADD[6]={0x0,0x0,0x0,0x0,0x0,0x0};

		if(OPMODE & WIFI_AP_STATE)
			memcpy(wk->idk_macaddr, GROUP_ADD, ETH_ALEN);
	}

	printk("keyid = %d, mac = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n"
			, wk.idk_keyix, wk.idk_macaddr[0], wk.idk_macaddr[1], wk.idk_macaddr[2], 
				wk.idk_macaddr[3], wk.idk_macaddr[4], wk.idk_macaddr[5]);
#endif


	wrqu.data.pointer = &wk;
	
	rtl_net80211_delkey(priv->dev, NULL, &wrqu, NULL);


	NLEXIT;
	return 0;

}



static int realtek_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr, void *cookie,
				   void (*callback) (void *cookie,
						     struct key_params *))						     
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	struct key_params params;
	struct stat_info	*pstat = NULL;
	unsigned int cipher = 0;

	NLENTER;

	if(mac_addr)
		pstat = get_stainfo(priv, mac_addr);

	printk("%s: key_index %d\n", __func__, key_index);


	if (!realtek_cfg80211_ready(priv))
		return -EIO;

	if (key_index > TOTAL_CAM_ENTRY(priv)) {
		printk("%s: key index %d out of bounds\n", __func__, key_index);
		return -ENOENT;
	}

#if 0
	if(pairwise)
	{
		pstat = get_stainfo(priv, mac_addr);
		if (pstat == NULL)
			return -ENOENT;
	}
#endif

	memset(&params, 0, sizeof(params));
	realtek_get_key_from_sta(priv, pstat, &params);

	//_eric_cfg ?? key seq is not used ??
#if 0
	params.seq_len = key->seq_len;
	params.seq = key->seq;
#endif

	callback(cookie, &params);

	NLEXIT;

	return 0;
}

#endif

static int realtek_cfg80211_set_default_key(struct wiphy *wiphy,
					   struct net_device *dev,
					   u8 key_index, bool unicast,
					   bool multicast)						
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	printk("defaukt key_index = %d unicast = %d multicast = %d \n", 
				key_index, unicast, multicast);
	return 0;
}

static int realtek_cfg80211_set_default_mgmt_key(struct wiphy *wiphy,
					     struct net_device *dev,
					     u8 key_idx)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;
}

//not in ath6k
static int realtek_cfg80211_auth(struct wiphy *wiphy, struct net_device *dev,
			  struct cfg80211_auth_request *req)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;
}

static int realtek_cfg80211_assoc(struct wiphy *wiphy, struct net_device *dev,
			   struct cfg80211_assoc_request *req)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;
}

static int realtek_cfg80211_deauth(struct wiphy *wiphy, struct net_device *dev,
			    struct cfg80211_deauth_request *req,
			    void *cookie)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;
}

static int realtek_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev,
			      struct cfg80211_disassoc_request *req,
			      void *cookie)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;		
	return 0;
}

//Not in ath6k
static int realtek_cfg80211_add_station(struct wiphy *wiphy, struct net_device *dev,
				 u8 *mac, struct station_parameters *params)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;

	NLEXIT;
	return 0;
}


void realtek_del_station(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned long	flags;
	issue_disassoc(priv, pstat->hwaddr, _RSON_AUTH_NO_LONGER_VALID_);

	SAVE_INT_AND_CLI(flags);
	if (!list_empty(&pstat->asoc_list))
	{
		list_del_init(&pstat->asoc_list);
		if (pstat->expire_to > 0)
		{
			cnt_assoc_num(priv, pstat, DECREASE, (char *)__FUNCTION__);
			check_sta_characteristic(priv, pstat, DECREASE);
			LOG_MSG("A STA is rejected by nl80211 - %02X:%02X:%02X:%02X:%02X:%02X\n",
					pstat->hwaddr[0],pstat->hwaddr[1],pstat->hwaddr[2],pstat->hwaddr[3],pstat->hwaddr[4],pstat->hwaddr[5]);
		}
	}
	free_stainfo(priv, pstat);
	RESTORE_INT(flags);

}

//eric ?? can apply to disconnect ??
static int realtek_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev,
				 u8 *mac)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	struct stat_info	*pstat;

	NLENTER;
	
	pstat = get_stainfo(priv, mac);

	if (pstat == NULL)
		return (-1);
		
	printk("nl80211 issue disassoc sta:%02X%02X%02X%02X%02X%02X reason:%d\n",
		mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], _RSON_AUTH_NO_LONGER_VALID_);

	realtek_del_station(priv, pstat);
	
	NLEXIT;
	return 0;
}

static int realtek_cfg80211_change_station(struct wiphy *wiphy,
				    struct net_device *dev,
				    u8 *mac,
				    struct station_parameters *params)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	struct stat_info *pstat = NULL;
	union iwreq_data wrqu;
	struct ieee80211req_mlme mlme;
	int err = 0;

	NLENTER;

	if(mac)
	{
		dump_mac(priv, mac);
		pstat = get_stainfo(priv, mac);
	}

	if(pstat == NULL)
		return 0;
		
#if 0
	if ((OPMODE & WIFI_AP_STATE) == 0)
	{
		return -EOPNOTSUPP;
	}
#endif

	memcpy(mlme.im_macaddr, mac, ETH_ALEN);

#if 1
	err = cfg80211_check_station_change(wiphy, params,
						CFG80211_STA_AP_MLME_CLIENT);

	if (err)
	{
		printk("cfg80211_check_station_change error !! \n");
		return err;
	}
#else
	/* Use this only for authorizing/unauthorizing a station */
	if (!(params->sta_flags_mask & BIT(NL80211_STA_FLAG_AUTHORIZED)))
		return -EOPNOTSUPP;
#endif

	if (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED))
		mlme.im_op = IEEE80211_MLME_AUTHORIZE;
	else
		mlme.im_op = IEEE80211_MLME_UNAUTHORIZE;

	wrqu.data.pointer = &mlme;

#if 0
	printk("NO SET PORT !!\n");
#else
	if(mlme.im_op == IEEE80211_MLME_AUTHORIZE)
		printk("IEEE80211_MLME_AUTHORIZE\n");
	else
		printk("IEEE80211_MLME_UNAUTHORIZE\n");
	
	if(priv->pmib->dot1180211AuthEntry.dot11EnablePSK || priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm)//OPENWRT_RADIUS 
	rtl_net80211_setmlme(priv->dev, NULL, &wrqu, NULL);
#endif


	NLEXIT;
	return 0;
}


static int realtek_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
				 u8 *mac, struct station_info *sinfo)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	struct stat_info *pstat = NULL;

	long left;
	bool sgi;
	int ret;
	u8 mcs;

	//NLENTER;

	if(mac)
		pstat = get_stainfo(priv, mac);

	if(pstat==NULL)
		return -ENOENT;
#if 0
	else
		dump_mac(priv, mac);
#endif

	sinfo->filled = 0;

	sinfo->filled |= STATION_INFO_INACTIVE_TIME;
	sinfo->inactive_time = pstat->idle_count*1000;


	sinfo->filled |= STATION_INFO_CONNECTED_TIME;
	sinfo->connected_time = pstat->link_time;

	if (pstat->rx_bytes) {
		sinfo->rx_bytes = pstat->rx_bytes;
		sinfo->filled |= STATION_INFO_RX_BYTES64;
		sinfo->rx_packets = pstat->rx_pkts;
		sinfo->filled |= STATION_INFO_RX_PACKETS;
	}

	if (pstat->tx_bytes) {
		sinfo->tx_bytes = pstat->tx_bytes;
		sinfo->filled |= STATION_INFO_TX_BYTES64;
		sinfo->tx_packets = pstat->tx_pkts;
		sinfo->filled |= STATION_INFO_TX_PACKETS;
	}

	sinfo->signal = pstat->rssi;
	sinfo->filled |= STATION_INFO_SIGNAL;

	//_eric_nl ?? VHT rates ??
	if (is_MCS_rate(pstat->current_tx_rate)) {
		sinfo->txrate.mcs = pstat->current_tx_rate&0xf;
		sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;

		if(pstat->ht_current_tx_info&BIT(0))
			sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;

		if(pstat->ht_current_tx_info&BIT(1))
			sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
			
	}
	else
	{
		//sinfo->txrate.legacy = rate / 100;
		sinfo->txrate.legacy = (pstat->current_tx_rate&0x7f)/2;
	}

	sinfo->filled |= STATION_INFO_TX_BITRATE;


	//_eric_nl ?? VHT rates ??
	if (is_MCS_rate(pstat->rx_rate)) {
		sinfo->rxrate.mcs = pstat->rx_rate&0xf;
		sinfo->rxrate.flags |= RATE_INFO_FLAGS_MCS;

		if(pstat->rx_bw)
			sinfo->rxrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;

		if(pstat->rx_splcp)
			sinfo->rxrate.flags |= RATE_INFO_FLAGS_SHORT_GI;

	}
	else
	{
		sinfo->txrate.legacy = (pstat->rx_rate&0x7f)/2;
	}

	sinfo->filled |= STATION_INFO_RX_BITRATE;
	

#if 0 //_eric_nl ?? sinfo->bss_param ??
	if(OPMODE & WIFI_STATION_STATE)
	{
		sinfo->filled |= STATION_INFO_BSS_PARAM;
		sinfo->bss_param.flags = 0;
		sinfo->bss_param.dtim_period = priv->pmib->dot11Bss.dtim_prd;
		sinfo->bss_param.beacon_interval = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod;
	}
#endif 

	//NLEXIT;
	return 0;
}


static int realtek_cfg80211_dump_station(struct wiphy *wiphy, struct net_device *dev,
				 int idx, u8 *mac, struct station_info *sinfo)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	int num = 0;
	struct list_head *phead, *plist;
	struct stat_info *pstat;
	int ret = -ENOENT;

	//NLENTER;

	//printk("try dump sta[%d]\n", idx);

	if(idx >= priv->assoc_num)
		return -ENOENT;
	
	phead = &priv->asoc_list;
	if (!netif_running(priv->dev) || list_empty(phead)) {
		return -ENOENT;
	}

	plist = phead->next;
	
	while (plist != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);

		if(num == idx){
			if(mac)
				memcpy(mac, pstat->hwaddr, ETH_ALEN);
			else
				mac = pstat->hwaddr;
			
			ret = realtek_cfg80211_get_station(wiphy, dev, pstat->hwaddr, sinfo);
			break;
		}
		num++;
		plist = plist->next;
	}

	//NLEXIT; 
	return ret;
}

#if 0
//not in ath6k
static int realtek_cfg80211_set_txq_params(struct wiphy *wiphy,
				    struct ieee80211_txq_params *params)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);

	NLENTER;
	NLNOT; 
	
	printk("queue = %d\n", params->queue);

	return 0;

}
#endif

static int realtek_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);

	NLENTER;

	if (changed & WIPHY_PARAM_RTS_THRESHOLD)
		priv->pmib->dot11OperationEntry.dot11RTSThreshold = wiphy->rts_threshold;
	if (changed & WIPHY_PARAM_RETRY_SHORT)
		priv->pmib->dot11OperationEntry.dot11ShortRetryLimit = wiphy->retry_short;
	if (changed & WIPHY_PARAM_RETRY_LONG)
		priv->pmib->dot11OperationEntry.dot11LongRetryLimit = wiphy->retry_long;
	
	NLEXIT;
	return 0;
}

static int realtek_cfg80211_set_tx_power(struct wiphy *wiphy,
				  enum nl80211_tx_power_setting type, int mbm) 			  
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);
	int dbm = MBM_TO_DBM(mbm);

	NLENTER;
	return 0;
	
}

struct rtl8192cd_priv* get_priv_from_wdev(struct rtknl *rtk, struct wireless_dev *wdev)
{
	struct rtl8192cd_priv *priv = NULL;
	int tmp = 0;

	for(tmp = 0; tmp<(IF_NUM); tmp++)
	{
		if(rtk->rtk_iface[tmp].priv)
		if(wdev == &(rtk->rtk_iface[tmp].priv->wdev))
		{
			priv = rtk->rtk_iface[tmp].priv;
			break;
		}
	}

	//printk("wdev = 0x%x priv = 0x%x \n", wdev, priv);

	return priv;
}


static int realtek_cfg80211_get_tx_power(struct wiphy *wiphy, 
				  struct wireless_dev *wdev, int *dbm)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = get_priv_from_wdev(rtk, wdev);

	//NLENTER;
	//printk("13dBm");
	*dbm = 13;
	return 0;

}


#endif


#if 1

//_eric_nl ?? suspend/resume use open/close ??
static int realtek_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);

	NLENTER;
	NLNOT;
	
	return 0;
}

static int realtek_cfg80211_resume(struct wiphy *wiphy)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);

	NLENTER;
	NLNOT;

	return 0;
}

static int realtek_cfg80211_scan(struct wiphy *wiphy,
			  struct cfg80211_scan_request *request)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);
	int ret = 0, i=0;

	NLENTER;

	priv = get_priv_from_wdev(rtk, request->wdev);

	if (!netif_running(priv->dev) || priv->ss_req_ongoing)
		ret = -1;
	else
		ret = 0;


#if 0
{
	unsigned char i;
	if (request->n_ssids && request->ssids[0].ssid_len) {
		for (i = 0; i < request->n_ssids; i++)
		{
			printk("[%d]len=%d,%s\n",i,
				 	request->ssids[i].ssid_len,
					request->ssids[i].ssid);
		}
	}

	if (request->ie) {
		printk("request->ie = ");
		for(i=0; i<request->ie_len; i++)
		{
			printk("0x%02x", request->ie);
		}
		printk("\n");
	}

	if (request->n_channels > 0) {
		unsigned char n_channels = 0;
		n_channels = request->n_channels;
		for (i = 0; i < n_channels; i++)
			printk("channel[%d]=%d\n", i, 
			ieee80211_frequency_to_channel(request->channels[i]->center_freq));
	}
}
#endif

	if (!ret)	// now, let's start site survey
	{
		if(!IS_ROOT_INTERFACE(priv))
			priv = GET_ROOT(priv);
	
		realtek_cfg80211_sscan_disable(wiphy);

		if (!netif_running(priv->dev))
			rtl8192cd_open(priv->dev);	
	
		if(priv->ss_req_ongoing)
		{
			printk("already under scanning, please wait...\n");
			return -1;
		}
	
		priv->ss_ssidlen = 0;
		//printk("start_clnt_ss, trigger by %s, ss_ssidlen=0\n", (char *)__FUNCTION__);

#if 0//def WIFI_SIMPLE_CONFIG
		if (len == 2)
			priv->ss_req_ongoing = 2;	// WiFi-Simple-Config scan-req
		else
#endif
			priv->ss_req_ongoing = 1;


		priv->scan_req = request;

		get_available_channel(priv);
		start_clnt_ss(priv);
	}
		
	NLEXIT;
	return ret;
}


static int realtek_cfg80211_join_ibss(struct wiphy *wiphy, struct net_device *dev,
			       struct cfg80211_ibss_params *ibss_param)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;

	if (!realtek_cfg80211_ready(priv))
		return -EIO;

	if (OPMODE & WIFI_ADHOC_STATE == 0)
		return -EOPNOTSUPP;

	//printk("Ad-Hoc join [%s] \n", ibss_param->ssid);
		
	memcpy(SSID, ibss_param->ssid, ibss_param->ssid_len);	
	SSID_LEN = ibss_param->ssid_len;

	realtek_auth_none(priv);

	if (ibss_param->privacy) 
	{
		realtek_auth_wep(priv, _WEP_40_PRIVACY_);
	}

#if 0
	if (ibss_param->chandef.chan)
	{
		realtek_cfg80211_set_channel(wiphy, dev, ibss_param->chandef.chan, ibss_param->channel_type);
	}
#endif

#if 0	

	if (ibss_param->channel_fixed) {
		/*
		 * TODO: channel_fixed: The channel should be fixed, do not
		 * search for IBSSs to join on other channels. Target
		 * firmware does not support this feature, needs to be
		 * updated.
		 */
		return -EOPNOTSUPP;
	}

	memset(vif->req_bssid, 0, sizeof(vif->req_bssid));
	if (ibss_param->bssid && !is_broadcast_ether_addr(ibss_param->bssid))
		memcpy(vif->req_bssid, ibss_param->bssid,
			sizeof(vif->req_bssid));
#endif

	if(IS_VXD_INTERFACE(priv))
	{
		//printk("launch vxd_ibss_beacon timer !!\n");
		construct_ibss_beacon(priv);
		issue_beacon_ibss_vxd(priv);
	}

	priv->join_res = STATE_Sta_No_Bss;
	start_clnt_lookup(priv, 1);

	NLEXIT;

	return 0;
}

static int realtek_cfg80211_leave_ibss(struct wiphy *wiphy, struct net_device *dev)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;
}


static int realtek_cfg80211_set_wds_peer(struct wiphy *wiphy, struct net_device *dev,
				  u8 *addr)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;
}

static void realtek_cfg80211_rfkill_poll(struct wiphy *wiphy)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, NULL);

	NLENTER;
	return 0;
}


static int realtek_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev,
				    bool enabled, int timeout)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;
}

//not in ath6k
static int realtek_cfg80211_set_bitrate_mask(struct wiphy *wiphy,
				      struct net_device *dev,
				      const u8 *addr,
				      const struct cfg80211_bitrate_mask *mask)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	NLNOT;

	//printk("fixed=%d, maxrate=%d\n", mask->fixed, mask->maxrate);  //mark_com

	return 0;
}



#endif




void copy_bss_ie(struct rtl8192cd_priv *priv, int ix)
{
	int wpa_ie_len = priv->site_survey->wpa_ie_backup[ix].wpa_ie_len;
	int rsn_ie_len = priv->site_survey->rsn_ie_backup[ix].rsn_ie_len;
	
	priv->rtk->clnt_info.wpa_ie.wpa_ie_len = wpa_ie_len;
	memcpy(priv->rtk->clnt_info.wpa_ie.data, priv->site_survey->wpa_ie_backup[ix].data, wpa_ie_len);

	priv->rtk->clnt_info.rsn_ie.rsn_ie_len = rsn_ie_len;
	memcpy(priv->rtk->clnt_info.rsn_ie.data, priv->site_survey->rsn_ie_backup[ix].data, rsn_ie_len);

}

int get_bss_by_bssid(struct rtl8192cd_priv *priv, unsigned char* bssid)
{
	int ix = 0, found = 0;

	printk("count = %d ", priv->site_survey->count_backup);
	dump_mac(priv, bssid);

	for(ix = 0 ; ix < priv->site_survey->count_backup ; ix++) //_Eric ?? will bss_backup be cleaned?? -> Not found in  codes
	{	
		if(!memcmp(priv->site_survey->bss_backup[ix].bssid , bssid, 6))
		{
			found = 1;
			copy_bss_ie(priv, ix);
			break;
		}
	}

	if(found == 0)
	{	
		printk("BSSID NOT Found !!\n");
		return -EINVAL;
	}
	else
		return ix;
	
}


int get_bss_by_ssid(struct rtl8192cd_priv *priv, unsigned char* ssid, int ssid_len)
{
	int ix = 0, found = 0;
	printk("count = %d ssid = %s\n", priv->site_survey->count_backup, ssid);

	for(ix = 0 ; ix < priv->site_survey->count_backup ; ix++) //_Eric ?? will bss_backup be cleaned?? -> Not found in  codes
	{	
		if(!memcmp(priv->site_survey->bss_backup[ix].ssid , ssid, ssid_len))
		{
			found = 1;
			copy_bss_ie(priv, ix);
			break;
		}
	}

	if(found == 0)
	{	
		printk("SSID NOT Found !!\n");
		return -EINVAL;
	}
	else
		return ix;
	
}

void vxd_copy_ss_result_from_root(struct rtl8192cd_priv *priv)
{
	struct rtl8192cd_priv *priv_root = GET_ROOT(priv);
	
	priv->site_survey->count_backup = priv_root->site_survey->count_backup;
	memcpy(priv->site_survey->bss_backup, priv_root->site_survey->bss_backup, sizeof(struct bss_desc)*priv_root->site_survey->count_backup);
}

static int realtek_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
					  struct cfg80211_connect_params *sme)
{

	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);
	int status = 0;
	int bss_num = 0;
	int ret = 0;

	NLENTER;

	if(dev)
	printk("[%s] GET_MY_HWADDR = ", dev->name);
	dump_mac(priv, GET_MY_HWADDR);

#ifdef  UNIVERSAL_REPEATER //wrt-vxd
	if((!IS_ROOT_INTERFACE(priv)) && (!IS_VXD_INTERFACE(priv)))
#else
	if(!IS_ROOT_INTERFACE(priv))
#endif
	{
		printk("vap can not connect, switch to root\n");
		priv = GET_ROOT(priv);
	}
	
	if (!realtek_cfg80211_ready(priv))
		return -EIO;
	
#if 1 //wrt_clnt
	if((OPMODE & WIFI_STATION_STATE) == 0)
	{
		printk("NOT in Client Mode, can NOT Associate !!!\n");
		return -1;
	}
#endif

	realtek_cfg80211_sscan_disable(priv);

	priv->receive_connect_cmd = 1;

#if 1 //wrt-wps-clnt
	priv->pmib->wscEntry.assoc_ielen = 0;
	priv->pmib->wscEntry.wsc_enable = 0;
	
	if (sme->ie && (sme->ie_len > 0)) {
		printk("Connect ie_len = %d\n", sme->ie_len);
		dump_ies(priv, sme->ie, sme->ie_len, MGMT_ASSOCREQ);
	}

	if(priv->pmib->wscEntry.wsc_enable)
		priv->wps_issue_join_req = 1;
#endif	

//=== check parameters
	if((sme->bssid == NULL) && (sme->ssid == NULL))
	{
		printk("No bssid&ssid from request !!!\n");
		return -1;
	}

//=== search from ss list 
#ifdef  UNIVERSAL_REPEATER //wrt-vxd
	if(IS_VXD_INTERFACE(priv))
		vxd_copy_ss_result_from_root(priv);
#endif

	if(sme->bssid)
		bss_num = get_bss_by_bssid(priv, sme->bssid);
	else if(sme->ssid) //?? channel parameter check ??
		bss_num = get_bss_by_ssid(priv, sme->ssid, sme->ssid_len);

	if(bss_num < 0)
	{
		printk("Can not found this bss from SiteSurvey result!!\n");
		return -1;
	}

	priv->ss_req_ongoing = 0; //found bss, no need to scan ...

//=== set security 
	realtek_set_security(priv, rtk, sme->crypto);

	if(priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
		psk_init(priv);
	
//=== set key (for wep only)
	if((sme->key_len) && 
		((rtk->cipher == WLAN_CIPHER_SUITE_WEP40)||(rtk->cipher == WLAN_CIPHER_SUITE_WEP104)))
	{
		printk("Set wep key to connect ! \n");
		
		if(rtk->cipher == WLAN_CIPHER_SUITE_WEP40)
		{
			priv->pmib->dot11GroupKeysTable.dot11Privacy = DOT11_ENC_WEP40;

		}
		else if(rtk->cipher == WLAN_CIPHER_SUITE_WEP104)
		{
			priv->pmib->dot11GroupKeysTable.dot11Privacy = DOT11_ENC_WEP104;

		}
		
		memcpy(&priv->pmib->dot11DefaultKeysTable.keytype[sme->key_idx].skey[0], sme->key, sme->key_len);

		priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen = sme->key_len;
		memcpy(&priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKey.skey[0], sme->key, sme->key_len);
	}

	syncMulticastCipher(priv, &priv->site_survey->bss_target[bss_num]);

//=== connect
	ret = rtl_wpas_join(priv, bss_num);

	NLEXIT;
	return ret;
}


static int realtek_cfg80211_disconnect(struct wiphy *wiphy,
						  struct net_device *dev, u16 reason_code)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	realtek_cfg80211_sscan_disable(priv);
	return 0;

}

static int realtek_cfg80211_channel_switch(struct wiphy *wiphy, 
			struct net_device *dev, struct cfg80211_csa_settings *params)

{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = realtek_get_priv(wiphy, dev);

	NLENTER;
	return 0;

}



static int realtek_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
			struct ieee80211_channel *chan,	unsigned int duration,	u64 *cookie)

{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = get_priv_from_wdev(rtk, wdev);

	NLENTER;
	return 0;

}


static int realtek_cancel_remain_on_channel(struct wiphy *wiphy,
			struct wireless_dev *wdev,	u64 cookie)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = get_priv_from_wdev(rtk, wdev);

	NLENTER;
	return 0;
}

static int realtek_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,	
		struct ieee80211_channel *chan, bool offchan, unsigned int wait, 
		const u8 *buf, size_t len,			  bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = get_priv_from_wdev(rtk, wdev);

	NLENTER;
	return 0;

}


static void realtek_mgmt_frame_register(struct wiphy *wiphy,
				       struct  wireless_dev *wdev,
				       u16 frame_type, bool reg)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = get_priv_from_wdev(rtk, wdev);

	NLENTER;
	return 0;
}




static struct device_type wiphy_type = {
	.name	= "wlan",
};


int register_netdevice_name_rtk(struct net_device *dev)
{
	int err;

	if (strchr(dev->name, '%')) {
		err = dev_alloc_name(dev, dev->name);
		if (err < 0)
			return err;
	}
	
	return register_netdevice(dev);
}

#if 1 //wrt-vap

static int realtek_nliftype_to_drv_iftype(enum nl80211_iftype type, u8 *nw_type)
{
	switch (type) {
	case NL80211_IFTYPE_STATION:
	//case NL80211_IFTYPE_P2P_CLIENT:
		*nw_type = INFRA_NETWORK;
		break;
	case NL80211_IFTYPE_ADHOC:
		*nw_type = ADHOC_NETWORK;
		break;
	case NL80211_IFTYPE_AP:
	//case NL80211_IFTYPE_P2P_GO:
		*nw_type = AP_NETWORK;
		break;
	default:
		printk("invalid interface type %u\n", type);
		return -ENOTSUPP;
	}

	return 0;
}

static bool realtek_is_valid_iftype(struct rtknl *rtk, enum nl80211_iftype type,
				   u8 *if_idx, u8 *nw_type)
{
	int i;

	if (realtek_nliftype_to_drv_iftype(type, nw_type))
		return false;

	if (type == NL80211_IFTYPE_AP || type == NL80211_IFTYPE_STATION || type == NL80211_IFTYPE_ADHOC) //wrt-adhoc
		return true;

	return false;
}

char check_vif_existed(struct rtl8192cd_priv *priv, struct rtknl *rtk, unsigned char *name)
{
	char tmp = 0;
	
	for(tmp =0; tmp<= VIF_NUM; tmp++)
	{
		if(!strcmp(name, rtk->ndev_name[tmp]))
		{
			printk("%s = %s, existed in vif[%d]\n", name, rtk->ndev_name[tmp]);
			return 1;
		}
	}

	return 0;
}

unsigned char check_vif_type_match(struct rtl8192cd_priv *priv, unsigned char is_vxd)
{
	unsigned char ret = 0;

	printk("check_vif_type for priv = 0x%x (root=%d vxd=%d vap=%d) +++ \n", 
		priv, IS_ROOT_INTERFACE(priv), IS_VXD_INTERFACE(priv), IS_VAP_INTERFACE(priv));

	printk("proot_priv = 0x%x, vap_id = %d \n", priv->proot_priv, priv->vap_id);
	
	if(is_vxd && IS_VXD_INTERFACE(priv))
		ret = 1;
	
	if((!is_vxd) && IS_VAP_INTERFACE(priv))
		ret = 1;

	if(ret)
		printk("is_vxd = %d, type OK \n", is_vxd);
	else
		printk("is_vxd = %d, type NOT match \n", is_vxd);

	return ret;
}

void rtk_change_netdev_name(struct rtl8192cd_priv *priv, unsigned char *name)
{
#if 0
	printk("rtk_change_netdev_name for priv = 0x%x (root=%d vxd=%d vap=%d) +++ \n", 
		priv, IS_ROOT_INTERFACE(priv), IS_VXD_INTERFACE(priv), IS_VAP_INTERFACE(priv));
	printk("from %s to %s \n", priv->dev->name, name);
#endif

	dev_change_name(priv->dev, name); //Need to modify kernel code to export this API
}

struct rtl8192cd_priv* get_priv_vxd_from_rtk(struct rtknl *rtk)
{
	struct rtl8192cd_priv *priv = NULL;
	int tmp = 0;

	for(tmp = 0; tmp<(IF_NUM); tmp++)
	{
		if(rtk->rtk_iface[tmp].priv)
		if(IS_VXD_INTERFACE(rtk->rtk_iface[tmp].priv))
		{
			priv = rtk->rtk_iface[tmp].priv;
			break;
		}
	}

	//printk("name = %s priv_vxd = 0x%x \n", priv->dev->name, priv);

	return priv;
}

struct rtl8192cd_priv* get_priv_from_rtk(struct rtknl *rtk, unsigned char *name)
{
	struct rtl8192cd_priv *priv = NULL;
	int tmp = 0;

	for(tmp = 0; tmp<(IF_NUM); tmp++)
	{
		if(rtk->rtk_iface[tmp].priv)
		if(!strcmp(rtk->rtk_iface[tmp].priv->dev->name, name))
		{
			priv = rtk->rtk_iface[tmp].priv;
			break;
		}
	}

#if 0
	if(priv) //rtk_vap
	printk("get_priv_from_rtk name = %s priv = 0x%x %s\n", name, priv, priv->dev->name);
	else
		printk("get_priv_from_rtk = NULL !!\n");
#endif

	return priv;
}


struct rtl8192cd_priv* get_priv_from_ndev(struct rtknl *rtk, struct net_device *ndev)
{
	struct rtl8192cd_priv *priv = NULL;
	int tmp = 0;

	for(tmp = 0; tmp<(IF_NUM); tmp++)
	{
		if(rtk->rtk_iface[tmp].priv)
		if(ndev == rtk->rtk_iface[tmp].priv->dev)
		{
			priv = rtk->rtk_iface[tmp].priv;
			break;
		}
	}

	//printk("ndev = 0x%x priv = 0x%x \n", ndev, priv);

	return priv;
}

void rtk_add_priv(struct rtl8192cd_priv *priv_add, struct rtknl *rtk)
{
	
	int tmp = 0;

	for(tmp = 0; tmp<(IF_NUM); tmp++)
	{
		if(rtk->rtk_iface[tmp].priv == NULL)
		{
			rtk->rtk_iface[tmp].priv = priv_add;
			strcpy(rtk->rtk_iface[tmp].ndev_name, priv_add->dev->name);
			break;
		}
	}

}

void rtk_del_priv(struct rtl8192cd_priv *priv_del, struct rtknl *rtk)
{

	int tmp = 0;

	for(tmp = 0; tmp<(IF_NUM); tmp++)
	{
		if(rtk->rtk_iface[tmp].priv == priv_del)
		{
			rtk->rtk_iface[tmp].priv = NULL;
			memset(rtk->rtk_iface[tmp].ndev_name, 0, 32);
			break;
		}
	}

}

unsigned char find_ava_vif_idx(struct rtknl *rtk)
{
	unsigned char idx = 0;

	for(idx = 0; idx < VIF_NUM; idx ++)
	{
		if(rtk->ndev_name[idx][0] == 0)
			return idx;
	}

	return -1;
}

unsigned char get_vif_idx(struct rtknl *rtk, unsigned char *name)
{
	unsigned char idx = 0;

	for(idx = 0; idx < VIF_NUM; idx ++)
	{
		if(rtk->ndev_name[idx][0] != 0)
		if(strcmp(name, rtk->ndev_name[idx])==0)
			return idx;
	}

	return -1;
}


void realtek_create_vap_iface(struct rtknl *rtk, unsigned char *name)
{
	struct rtl8192cd_priv *priv = rtk->priv;

	if(check_vif_existed(priv, rtk, name))
	{
		printk("vif interface already existed !! \n");
		return 0;
	}

	if (rtk->num_vif == VIF_NUM) 
	{
		printk("Reached maximum number of supported vif\n");
		return -1;
	}

	rtk->idx_vif = find_ava_vif_idx(rtk);

	printk("rtk->idx_vif = %d\n", rtk->idx_vif);

	if(rtk->idx_vif < 0)
	{
		printk("rtk->idx_vif < 0 \n");
		return;
	}

	if(name){
		if(dev_valid_name(name))
			strcpy(rtk->ndev_name[rtk->idx_vif], name);
	}
	else
	{
		printk("No interface name !!\n");
		return -1;
	}

	rtl8192cd_init_one_cfg80211(rtk);
	rtk->num_vif++;

}

#endif

int realtek_interface_add(struct rtl8192cd_priv *priv, 
					  struct rtknl *rtk, const char *name,
					  enum nl80211_iftype type,
					  u8 fw_vif_idx, u8 nw_type)
{

 	struct net_device *ndev;
	struct ath6kl_vif *vif;

	NLENTER;

	printk("name = %s, type = %d\n", name, type);

	ndev = priv->dev;

	dump_mac(priv, ndev->dev_addr);
	
	if (!ndev)
	{
		printk("ndev = NULL !!\n");
		free_netdev(ndev);
		return -1;
	}

	strcpy(ndev->name, name);

	dev_net_set(ndev, wiphy_net(rtk->wiphy));

	priv->wdev.wiphy = rtk->wiphy;	

	ndev->ieee80211_ptr = &priv->wdev;

	SET_NETDEV_DEV(ndev, wiphy_dev(rtk->wiphy));
	
	priv->wdev.netdev = ndev;	
	priv->wdev.iftype = type;
	
	SET_NETDEV_DEVTYPE(ndev, &wiphy_type);


	register_netdev(ndev);

#if 0
	if(IS_ROOT_INTERFACE(priv))
		register_netdev(ndev);
#ifdef UNIVERSAL_REPEATER //wrt-vxd
	else if(IS_VXD_INTERFACE(priv))
		register_netdev(ndev);
#endif
	else
		register_netdevice_name_rtk(ndev);
#endif

	rtk->ndev_add = ndev;

	printk("add priv = 0x%x wdev = 0x%x ndev = 0x%x\n", priv, &priv->wdev, ndev);
	rtk_add_priv(priv, rtk);

	NLEXIT;

	return 0;

}

void realtek_change_iftype(struct rtl8192cd_priv *priv ,enum nl80211_iftype type)
{
	OPMODE &= ~(WIFI_STATION_STATE|WIFI_ADHOC_STATE|WIFI_AP_STATE);

	switch (type) {
	case NL80211_IFTYPE_STATION:
	//case NL80211_IFTYPE_P2P_CLIENT:
		OPMODE = WIFI_STATION_STATE;
		priv->wdev.iftype = type;
		break;
	case NL80211_IFTYPE_ADHOC:
		OPMODE = WIFI_ADHOC_STATE;
		priv->wdev.iftype = type;
		break;
	case NL80211_IFTYPE_AP:
	//case NL80211_IFTYPE_P2P_GO:
		OPMODE = WIFI_AP_STATE;
		priv->wdev.iftype = type;
		break;
	default:
		printk("invalid interface type %u\n", type);
		OPMODE = WIFI_AP_STATE;
		return -EOPNOTSUPP;
	}

	printk("type =%d, OPMODE = 0x%x\n", type, OPMODE);

	if(IS_ROOT_INTERFACE(priv) && IS_DRV_OPEN(priv))
		rtl8192cd_close(priv->dev);

}

void type_to_name(type, type_name)
{

	switch (type) {
	case NL80211_IFTYPE_STATION:
		strcpy(type_name, "NL80211_IFTYPE_STATION");
		break;
	case NL80211_IFTYPE_ADHOC:
		strcpy(type_name, "NL80211_IFTYPE_ADHOC");
		break;
	case NL80211_IFTYPE_AP:
		strcpy(type_name, "NL80211_IFTYPE_AP");
		break;
	default:
		strcpy(type_name, "NOT SUPPORT TYPE");
		return -EOPNOTSUPP;
	}

}

static struct wireless_dev *realtek_cfg80211_add_iface(struct wiphy *wiphy,
						      const char *name,
						      enum nl80211_iftype type,
						      u32 *flags,
						      struct vif_params *params)
{
	struct rtknl *rtk = wiphy_priv(wiphy); //return &wiphy->priv;
	struct rtl8192cd_priv	*priv = rtk->priv;
	struct rtl8192cd_priv *priv_add = NULL;
	u8 if_idx, nw_type;
	unsigned char type_name[32];

	NLENTER;
	
	type_to_name(type, type_name);
	printk("add_iface type=%d [%s] name=%s\n", type, type_name, name);

#ifdef WDS
	if(params)
	{
		printk("use_4addr = %d \n", params->use_4addr);
	}
#endif

	if((strcmp(name, "wlan0")==0) || (strcmp(name, "wlan1")==0))
	{
		printk("root interface, just change type\n");
		realtek_change_iftype(priv, type);
		return &rtk->priv->wdev;
	}	

	priv_add = get_priv_from_rtk(rtk, name);

	if(priv_add)
	{
		unsigned char type_match = 0; 
		unsigned char is_vxd = 0;

		if(is_WRT_scan_iface(name))
		{
			printk("Add Scan interface, do nothing\n");
			return &priv_add->wdev;
		}	

		if(type == NL80211_IFTYPE_AP)
		{
			is_vxd = 0;
			rtk->num_vap ++ ;
		}
		else
		{
			is_vxd = 1;
			rtk->num_vxd = 1;
		}
	
		type_match = check_vif_type_match(priv_add, is_vxd);

		if(!type_match)
		{
			unsigned char name_vxd[32];
			unsigned char name_vap[32];
			unsigned char name_tmp[32];
			struct rtl8192cd_priv *priv_vxd = NULL;
			struct rtl8192cd_priv *priv_vap = NULL;
			struct rtl8192cd_priv *priv_tmp = NULL;
			
			printk("Type NOT Match !!! need to change name\n");

			if(is_vxd)
			{
				priv_vap = priv_add;
				priv_vxd = get_priv_vxd_from_rtk(rtk);
			}
			else
			{
				sprintf(name_vap, "%s-%d", rtk->priv->dev->name, (RTL8192CD_NUM_VWLAN));
				priv_vap = get_priv_from_rtk(rtk, name_vap);
				priv_vxd = priv_add;
			}

			sprintf(name_tmp, "%s-%d", rtk->priv->dev->name, (RTL8192CD_NUM_VWLAN+10));
			
			strcpy(name_vap, priv_vap->dev->name);
			strcpy(name_vxd, priv_vxd->dev->name);
			
#if 1
			printk(" [BEFORE] +++ \n");
			printk("VAP = 0x%x(0x%x) name=%s \n", priv_vap, priv_vap->dev, priv_vap->dev->name);
			printk("VXD = 0x%x(0x%x) name=%s \n", priv_vxd, priv_vxd->dev, priv_vxd->dev->name);
#endif
			
			rtk_change_netdev_name(priv_vap, name_tmp);
			rtk_change_netdev_name(priv_vxd, name_vap);
			rtk_change_netdev_name(priv_vap, name_vxd);
	
#if 1
			printk(" [AFTER] --- \n");
			printk("VAP = 0x%x(0x%x) name=%s \n", priv_vap, priv_vap->dev, priv_vap->dev->name);
			printk("VXD = 0x%x(0x%x) name=%s \n", priv_vxd, priv_vxd->dev, priv_vxd->dev->name);
#endif


			if(is_vxd)
			{
#if 1 //wrt-adhoc
				{
					printk("VXD change type to %d \n", type);
					realtek_change_iftype(priv_vxd, type);
				}
#endif

				return &priv_vxd->wdev;
			}
			else
				return &priv_vap->wdev;

		}
		else
		{
			printk("Type OK, do nothing\n");

#if 1 //wrt-adhoc
			if(is_vxd)
			{
				printk("VXD change type to %d \n", type);
				realtek_change_iftype(priv_add, type);
			}
#endif

			return &priv_add->wdev;
		}

	}
	else 
	{
		printk("Can not find correspinding priv for %s !!\n", name);
		return -1;
	}

	NLEXIT;
	
	return &rtk->priv->wdev;

}


void close_vxd_vap(struct rtl8192cd_priv *priv_root)
{
	int i = 0;

#ifdef UNIVERSAL_REPEATER
	if(IS_DRV_OPEN(priv_root->pvxd_priv))
		rtl8192cd_close(priv_root->pvxd_priv->dev);
#endif
	
#ifdef MBSSID
	for (i=0; i<RTL8192CD_NUM_VWLAN; i++) { 
		if(IS_DRV_OPEN(priv_root->pvap_priv[i]))
			rtl8192cd_close(priv_root->pvap_priv[i]->dev);
	}
#endif

}


static int realtek_cfg80211_del_iface(struct wiphy *wiphy,
				     struct wireless_dev *wdev)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv	*priv = get_priv_from_wdev(rtk, wdev);
	unsigned char *name = NULL;

	NLENTER;

	if(priv) 
	{
		name = priv->dev->name;
		printk("def_iface name= %s priv = 0x%x wdev =0x%x\n", name, priv, wdev);
	}
	else
	{
		printk("Can NOT find priv from wdev = 0x%x", wdev);
		return -1;
	}	

	printk("Just close this interface\n");

#ifdef MBSSID
	if(IS_ROOT_INTERFACE(priv))
	{
		close_vxd_vap(priv);
	}

#ifdef UNIVERSAL_REPEATER
	if(IS_VXD_INTERFACE(priv))
		rtk->num_vxd = 0;
#endif

	if(IS_VAP_INTERFACE(priv))
		rtk->num_vap --;
#endif
	
	priv->receive_connect_cmd = 0;
	
	rtl8192cd_close(priv->dev);

	NLEXIT;
	
	return 0;
}

//survey_dump
static int realtek_dump_survey(struct wiphy *wiphy, 
								struct net_device *dev,	
								int idx, 
								struct survey_info *survey)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv *priv = rtk->priv;
	int freq;

	//NLENTER;

	if(idx != 0)
	{
		//printk("survey idx:%d\n", idx);
		return -1;
	}

	if(priv->pmib->dot11RFEntry.dot11channel >= 34)
		freq = ieee80211_channel_to_frequency(priv->pmib->dot11RFEntry.dot11channel, IEEE80211_BAND_5GHZ);
	else
		freq = ieee80211_channel_to_frequency(priv->pmib->dot11RFEntry.dot11channel, IEEE80211_BAND_2GHZ);
		
	survey->channel = ieee80211_get_channel(wiphy, freq);
	
	survey->channel_time = 1000;
#if 1
	if(priv->rtk->chbusytime > priv->rtk->rx_time)
	{
		survey->channel_time_busy = priv->rtk->chbusytime + priv->rtk->tx_time;
		survey->channel_time_rx = priv->rtk->rx_time;
	}
	else
	{
		survey->channel_time_busy = priv->rtk->rx_time + priv->rtk->tx_time;
		survey->channel_time_rx = priv->rtk->rx_time - priv->rtk->chbusytime;
	}
	survey->channel_time_tx = priv->rtk->tx_time;
#else	
	survey->channel_time_busy = priv->rtk->chbusytime;
	survey->channel_time_rx = priv->rtk->rx_time;
	survey->channel_time_tx = priv->rtk->tx_time;
#endif
	//printk("chbusytime:%d rx_time:%d tx_time:%d\n",
	//	priv->rtk->chbusytime, priv->rtk->rx_time, priv->rtk->tx_time);

	//printk("channel_time_busy:%d channel_time_rx:%d channel_time_tx:%d\n",
	//	survey->channel_time_busy, survey->channel_time_rx, survey->channel_time_tx);

	//survey->channel_time_busy = 100;
	//survey->channel_time_ext_busy = 100;
	//survey->channel_time_rx = 100;
	//survey->channel_time_tx = 100;
	survey->filled = SURVEY_INFO_CHANNEL_TIME|SURVEY_INFO_CHANNEL_TIME_BUSY | SURVEY_INFO_CHANNEL_TIME_RX|SURVEY_INFO_CHANNEL_TIME_TX;
	//survey->noise = 6;

	//NLEXIT;
	
	return 0;
}


static int realtek_cfg80211_change_iface(struct wiphy *wiphy,
					struct net_device *ndev,
					enum nl80211_iftype type, u32 *flags,
					struct vif_params *params)
{
	struct rtknl *rtk = wiphy_priv(wiphy);
	struct rtl8192cd_priv	*priv = get_priv_from_ndev(rtk, ndev); //rtk->priv;
	int i;
	unsigned char type_name[32];

	NLENTER;

	type_to_name(type, type_name);
	printk("change_iface type=%d [%s] name=%s\n", type, type_name, priv->dev->name);
	realtek_change_iftype(priv, type);

	NLEXIT;
	return 0;
}




#if 0
static int realtek_cfg80211_sscan_start(struct wiphy *wiphy,
			struct net_device *dev,
			struct cfg80211_sched_scan_request *request)
{

	return 0;
}

static int realtek_cfg80211_sscan_stop(struct wiphy *wiphy,
				      struct net_device *dev)
{

	return 0;
}

static int realtek_cfg80211_set_bitrate(struct wiphy *wiphy,
				       struct net_device *dev,
				       const u8 *addr,
				       const struct cfg80211_bitrate_mask *mask)
{

	return 0;
}

static int realtek_cfg80211_set_txe_config(struct wiphy *wiphy,
					  struct net_device *dev,
					  u32 rate, u32 pkts, u32 intvl)
{

	return 0;
}
#endif


struct cfg80211_ops realtek_cfg80211_ops = {
	.add_virtual_intf = realtek_cfg80211_add_iface,
	.del_virtual_intf = realtek_cfg80211_del_iface,
	.change_virtual_intf = realtek_cfg80211_change_iface,
	.add_key = realtek_cfg80211_add_key,
	.del_key = realtek_cfg80211_del_key,
	.get_key = realtek_cfg80211_get_key,
	.set_default_key = realtek_cfg80211_set_default_key,
	.set_default_mgmt_key = realtek_cfg80211_set_default_mgmt_key,
	//.add_beacon = realtek_cfg80211_add_beacon,
	//.set_beacon = realtek_cfg80211_set_beacon,
	//.del_beacon = realtek_cfg80211_del_beacon,
	.add_station = realtek_cfg80211_add_station,
	.del_station = realtek_cfg80211_del_station,
	.change_station = realtek_cfg80211_change_station,
	.get_station = realtek_cfg80211_get_station,
	.dump_station = realtek_cfg80211_dump_station,
#if 0//def CONFIG_MAC80211_MESH
		.add_mpath = realtek_cfg80211_add_mpath,
		.del_mpath = realtek_cfg80211_del_mpath,
		.change_mpath = realtek_cfg80211_change_mpath,
		.get_mpath = realtek_cfg80211_get_mpath,
		.dump_mpath = realtek_cfg80211_dump_mpath,
		.set_mesh_params = realtek_cfg80211_set_mesh_params,
		.get_mesh_params = realtek_cfg80211_get_mesh_params,
#endif
	.change_bss = realtek_cfg80211_change_bss,
	//.set_txq_params = realtek_cfg80211_set_txq_params,
	//.set_channel = realtek_cfg80211_set_channel,
	.suspend = realtek_cfg80211_suspend,
	.resume = realtek_cfg80211_resume,
	.scan = realtek_cfg80211_scan,
#if 0
	.auth = realtek_cfg80211_auth,
	.assoc = realtek_cfg80211_assoc,
	.deauth = realtek_cfg80211_deauth,
	.disassoc = realtek_cfg80211_disassoc,
#endif
	.join_ibss = realtek_cfg80211_join_ibss,
	.leave_ibss = realtek_cfg80211_leave_ibss,
	.set_wiphy_params = realtek_cfg80211_set_wiphy_params,
	.set_tx_power = realtek_cfg80211_set_tx_power,
	.get_tx_power = realtek_cfg80211_get_tx_power,
	.set_power_mgmt = realtek_cfg80211_set_power_mgmt,
	.set_wds_peer = realtek_cfg80211_set_wds_peer,
	.rfkill_poll = realtek_cfg80211_rfkill_poll,
	//CFG80211_TESTMODE_CMD(ieee80211_testmode_cmd)
	.set_bitrate_mask = realtek_cfg80211_set_bitrate_mask,
	.connect = realtek_cfg80211_connect,
	.disconnect = realtek_cfg80211_disconnect,
	//.remain_on_channel = realtek_remain_on_channel,
	//.cancel_remain_on_channel = realtek_cancel_remain_on_channel,
	.channel_switch = realtek_cfg80211_channel_switch,
	.mgmt_tx = realtek_mgmt_tx,
	.mgmt_frame_register = realtek_mgmt_frame_register,
	//.dump_survey = realtek_dump_survey,//survey_dump
	.start_ap = realtek_start_ap,
	.change_beacon = realtek_change_beacon,
	.stop_ap = realtek_stop_ap,
#if 0
	.sched_scan_start = realtek_cfg80211_sscan_start,
	.sched_scan_stop = realtek_cfg80211_sscan_stop,
	.set_bitrate_mask = realtek_cfg80211_set_bitrate,
	.set_cqm_txe_config = realtek_cfg80211_set_txe_config,
#endif
};

static void  rtk_create_dev(struct rtknl *rtk,int idx)
{
	/* define class here */
	unsigned char zero[] = {0, 0, 0, 0, 0, 0};
    rtk->cl = class_create(THIS_MODULE, rtk_dev_name[idx]); 
 
    /* create first device */
    rtk->dev = device_create(rtk->cl, NULL, rtk_wifi_dev[idx], NULL, rtk_dev_name[idx]);

 	dev_set_name(rtk->dev, rtk_dev_name[idx]);
  	printk("Device Name = %s \n", dev_name(rtk->dev));

	printk("VIF_NUM=%d\n", VIF_NUM);
	memset(rtk->ndev_name, 0, VIF_NUM*VIF_NAME_SIZE);

	//init rtk phy root name
	sprintf(rtk->root_ifname, "wlan%d", idx);

	//mark_dual ,init with fake mac for diff phy
	rtk_fake_addr[3] += ((unsigned char)idx) ;
#ifdef EN_EFUSE
	memcpy(rtk->root_mac, rtk_fake_addr, ETH_ALEN);
#else
	read_flash_hw_mac(rtk->root_mac,idx);
	if((memcmp(rtk->root_mac,zero,ETH_ALEN) == 0) || IS_MCAST(rtk->root_mac)){
		memcpy(rtk->root_mac, rtk_fake_addr, ETH_ALEN); 
	}
#endif


}


//struct rtknl *realtek_cfg80211_create(struct rtl8192cd_priv *priv)
struct rtknl *realtek_cfg80211_create(void) 
{
	struct wiphy *wiphy;
	struct rtknl *rtk;

	NLENTER;

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&realtek_cfg80211_ops, sizeof(struct rtknl));

	if (!wiphy) {
		printk("couldn't allocate wiphy device\n"); 
		return NULL;
	}

	rtk = wiphy_priv(wiphy);
	rtk->wiphy = wiphy;
	//rtk->priv = priv;	 //mark_dual2
	
	//sync to global rtk_phy
	if(rtk_phy_idx > RTK_MAX_WIFI_PHY)
	{		
		printk("ERROR!! rtk_phy_idx >  RTK_MAX_WIFI_PHY\n");
		wiphy_free(wiphy);
		return NULL;
	}
	rtk_create_dev(rtk,rtk_phy_idx);	
	rtk_phy[rtk_phy_idx] = rtk;
	rtk_phy_idx++;

	//priv->rtk = rtk ; //mark_dual2

	NLEXIT;
	return rtk;
}

int realtek_cfg80211_init(struct rtknl *rtk,struct rtl8192cd_priv *priv)  
{
	struct wiphy *wiphy = rtk->wiphy;
	bool band_2gig = false, band_5gig = false;
	int ret;
#ifdef EN_EFUSE
	char efusemac[ETH_ALEN];
	char zero[ETH_ALEN] = {0,0,0,0,0,0};
#endif
	NLENTER;
	rtk->priv = priv;  //mark_dual	
	//wiphy->mgmt_stypes = realtek_mgmt_stypes; //_eric_cfg ??
	//wiphy->max_remain_on_channel_duration = 5000;

	/* set device pointer for wiphy */

#if 1//rtk_nl80211 ??
	//printk("set_wiphy_dev +++ \n");
	set_wiphy_dev(wiphy, rtk->dev); //return wiphy->dev.parent;
	//printk("set_wiphy_dev --- \n");
#endif
#ifdef EN_EFUSE
	memset(efusemac,0,ETH_ALEN);
	extern read_efuse_mac_address(struct rtl8192cd_priv * priv,char * efusemac);
	read_efuse_mac_address(priv,efusemac);
	if( memcmp(efusemac,zero,ETH_ALEN) && !IS_MCAST(efusemac))
		memcpy(rtk->root_mac,efusemac,ETH_ALEN);
#endif
	memcpy(wiphy->perm_addr, rtk->root_mac, ETH_ALEN); 
	memcpy(priv->pmib->dot11Bss.bssid, wiphy->perm_addr, ETH_ALEN);

	wiphy->interface_modes = BIT(NL80211_IFTYPE_AP)|
							BIT(NL80211_IFTYPE_STATION) | //_eric_cfg station mandatory ??
							BIT(NL80211_IFTYPE_ADHOC); //wrt-adhoc
	
	/* max num of ssids that can be probed during scanning */
	//wiphy->max_scan_ssids = MAX_PROBED_SSIDS;
	/* max num of ssids that can be matched after scan */
	//wiphy->max_match_sets = MAX_PROBED_SSIDS;

	//wiphy->max_scan_ie_len = 1000; /* FIX: what is correct limit? */
	
	rtk_get_band_capa(priv,&band_2gig,&band_5gig);

	/*
	 * Even if the fw has HT support, advertise HT cap only when
	 * the firmware has support to override RSN capability, otherwise
	 * 4-way handshake would fail.
	 */
	if(band_2gig)
	{
	realtek_band_2ghz.ht_cap.mcs.rx_mask[0] = 0xff;
	realtek_band_2ghz.ht_cap.mcs.rx_mask[1] = 0xff;
	wiphy->bands[IEEE80211_BAND_2GHZ] = &realtek_band_2ghz;
	}	

	if(band_5gig)
	{
		realtek_band_5ghz.ht_cap.mcs.rx_mask[0] = 0xff;
		realtek_band_5ghz.ht_cap.mcs.rx_mask[1] = 0xff;
		wiphy->bands[IEEE80211_BAND_5GHZ] = &realtek_band_5ghz;
#ifdef RTK_AC_SUPPORT
		{
			extern void input_value_32(unsigned long *p, unsigned char start, unsigned char end, unsigned int value);

			wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_supported = true;
	        input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, MAX_MPDU_LENGTH_S, MAX_MPDU_LENGTH_E, 0x1);
			//Support 80MHz bandwidth only
			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, CHL_WIDTH_S, CHL_WIDTH_E, 0);

			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, SHORT_GI80M_S, SHORT_GI80M_E, 1);
			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, SHORT_GI160M_S, SHORT_GI160M_E, 0);
			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, RX_STBC_S, RX_STBC_E, 1);
			if (get_rf_mimo_mode(priv) == MIMO_2T2R)
				input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, TX_STBC_S, TX_STBC_E, 1);

			if (GET_CHIP_VER(priv) == VERSION_8812E)
				input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, RX_LDPC_S, RX_LDPC_E, 1);
			else
				input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, RX_LDPC_S, RX_LDPC_E, 0);
			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, SU_BFER_S, SU_BFER_E, 1);
			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, SU_BFEE_S, SU_BFEE_E, 1);
			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, MAX_ANT_SUPP_S, MAX_ANT_SUPP_E, 1);
			input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, SOUNDING_DIMENSIONS_S, SOUNDING_DIMENSIONS_E, 1);
            input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, HTC_VHT_S, HTC_VHT_E, 1);
            input_value_32(&wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.cap, MAX_RXAMPDU_FACTOR_S, MAX_RXAMPDU_FACTOR_E, 7);
			if (get_rf_mimo_mode(priv) == MIMO_2T2R) {
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(0xfffa);
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(0xfffa);
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.rx_highest = 1733;
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.tx_highest = 1733;
			} else {
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.rx_mcs_map = cpu_to_le16(0xfffe);
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.tx_mcs_map = cpu_to_le16(0xfffe);
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.rx_highest = 867;
				wiphy->bands[IEEE80211_BAND_5GHZ]->vht_cap.vht_mcs.tx_highest = 867;
			}
		}
#endif
	}	

	//wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM; //mark_priv
	wiphy->signal_type=CFG80211_SIGNAL_TYPE_UNSPEC;

	wiphy->cipher_suites = cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites);

#if 0//def CONFIG_PM
	wiphy->wowlan.flags = WIPHY_WOWLAN_MAGIC_PKT |
			      WIPHY_WOWLAN_DISCONNECT |
			      WIPHY_WOWLAN_GTK_REKEY_FAILURE  |
			      WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
			      WIPHY_WOWLAN_EAP_IDENTITY_REQ   |
			      WIPHY_WOWLAN_4WAY_HANDSHAKE;
	wiphy->wowlan.n_patterns = WOW_MAX_FILTERS_PER_LIST;
	wiphy->wowlan.pattern_min_len = 1;
	wiphy->wowlan.pattern_max_len = WOW_PATTERN_SIZE;

	wiphy->max_sched_scan_ssids = MAX_PROBED_SSIDS;

	//_eric_cfg ?? support these features ??
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM |
			    	WIPHY_FLAG_HAVE_AP_SME |
			    	WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
			    	WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD;

	wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
	
	wiphy->features |= NL80211_FEATURE_INACTIVITY_TIMER;
	wiphy->probe_resp_offload =
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2 |
		NL80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P;
#endif

	//printk("wiphy_register +++ \n");
	ret = wiphy_register(wiphy);
	//printk("wiphy_register --- \n");
	
	if (ret < 0) {
		printk("couldn't register wiphy device\n");
		return ret;
	}

	rtk->wiphy_registered = true;

	NLEXIT;
	return 0;
}

#endif //RTK_NL80211

