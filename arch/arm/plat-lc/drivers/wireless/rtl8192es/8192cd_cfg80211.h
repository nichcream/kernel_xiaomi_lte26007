/*
 * Copyright (c) 2011 Atheros Communications Inc.
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

#ifndef RTK_NL80211
#define RTK_NL80211
#endif

#ifdef RTK_NL80211

#define MAX_PROBED_SSIDS 32

#define VIF_NUM				RTL8192CD_NUM_VWLAN //eric-vap
#define IF_NUM				(VIF_NUM+2)  //#vap + root + vxd
#define VIF_NAME_SIZE		10

#define MAX_IE_LEN 			300

#define MAX_ASSOC_REQ_LEN 	512
#define MAX_ASSOC_RSP_LEN 	512


#define RATETAB_ENT(_rate, _rateid, _flags) {   \
	.bitrate    = (_rate),                  \
	.flags      = (_flags),                 \
	.hw_value   = (_rateid),                \
}


static struct ieee80211_rate realtek_rates[] = {
	RATETAB_ENT(10, 0x1, 0),
	RATETAB_ENT(20, 0x2, 0),
	RATETAB_ENT(55, 0x4, 0),
	RATETAB_ENT(110, 0x8, 0),
	RATETAB_ENT(60, 0x10, 0),
	RATETAB_ENT(90, 0x20, 0),
	RATETAB_ENT(120, 0x40, 0),
	RATETAB_ENT(180, 0x80, 0),
	RATETAB_ENT(240, 0x100, 0),
	RATETAB_ENT(360, 0x200, 0),
	RATETAB_ENT(480, 0x400, 0),
	RATETAB_ENT(540, 0x800, 0),
};


#define realtek_g_rates     (realtek_rates + 0)
#define realtek_g_rates_size    12

#define realtek_a_rates     (realtek_rates + 4)
#define realtek_a_rates_size    8

#define realtek_g_htcap (IEEE80211_HT_CAP_SUP_WIDTH_20_40 | \
			IEEE80211_HT_CAP_SGI_20		 | \
			IEEE80211_HT_CAP_SGI_40)

#define realtek_a_htcap (IEEE80211_HT_CAP_SUP_WIDTH_20_40 | \
			IEEE80211_HT_CAP_SGI_20		 | \
			IEEE80211_HT_CAP_SGI_40)

#define realtek_a_vhtcap (IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_80MHZ)


/* WMI_CONNECT_CMDID  */
enum network_type {
	INFRA_NETWORK = 0x01,
	ADHOC_NETWORK = 0x02,
	ADHOC_CREATOR = 0x04,
	AP_NETWORK = 0x10,
};

enum mgmt_type {
	MGMT_BEACON = 0,
	MGMT_PROBERSP = 1,
	MGMT_ASSOCRSP = 2,
	MGMT_ASSOCREQ = 3,
};


static const u32 cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	//CCKM_KRK_CIPHER_SUITE,
	//WLAN_CIPHER_SUITE_SMS4,
};

#define CHAN2G(_channel, _freq, _flags) {   \
	.band           = IEEE80211_BAND_2GHZ,  \
	.hw_value       = (_channel),           \
	.center_freq    = (_freq),              \
	.flags          = (_flags),             \
	.max_antenna_gain   = 0,                \
	.max_power      = 30,                   \
}

#define CHAN5G(_channel, _flags) {                  \
        .band           = IEEE80211_BAND_5GHZ,      \
        .hw_value       = (_channel),               \
        .center_freq    = 5000 + (5 * (_channel)),  \
        .flags          = (_flags),                 \
        .max_antenna_gain   = 0,                    \
        .max_power      = 30,                       \
} 
   

static struct ieee80211_channel realtek_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	//CHAN2G(14, 2484, 0),
};

static struct ieee80211_channel realtek_5ghz_a_channels[] = {
        CHAN5G(34, 0), CHAN5G(36, 0),
        CHAN5G(38, 0), CHAN5G(40, 0),
        CHAN5G(42, 0), CHAN5G(44, 0),
        CHAN5G(46, 0), CHAN5G(48, 0),
        CHAN5G(52, 0), CHAN5G(56, 0),
        CHAN5G(60, 0), CHAN5G(64, 0),
        CHAN5G(100, 0), CHAN5G(104, 0),
        CHAN5G(108, 0), CHAN5G(112, 0),
        CHAN5G(116, 0), CHAN5G(120, 0),
        CHAN5G(124, 0), CHAN5G(128, 0),
        CHAN5G(132, 0), CHAN5G(136, 0),
        CHAN5G(140, 0), CHAN5G(149, 0),
        CHAN5G(153, 0), CHAN5G(157, 0),
        CHAN5G(161, 0), CHAN5G(165, 0),
        CHAN5G(184, 0), CHAN5G(188, 0),
        CHAN5G(192, 0), CHAN5G(196, 0),
        CHAN5G(200, 0), CHAN5G(204, 0),
        CHAN5G(208, 0), CHAN5G(212, 0),
        CHAN5G(216, 0),
};


static struct ieee80211_supported_band realtek_band_2ghz = {
	.n_channels = ARRAY_SIZE(realtek_2ghz_channels),
	.channels = realtek_2ghz_channels,
	.n_bitrates = realtek_g_rates_size,
	.bitrates = realtek_g_rates,
	.ht_cap.cap = realtek_g_htcap,
	.ht_cap.ht_supported = true,
};

static struct ieee80211_supported_band realtek_band_5ghz = {
        .n_channels = ARRAY_SIZE(realtek_5ghz_a_channels),
        .channels = realtek_5ghz_a_channels,
        .n_bitrates = realtek_a_rates_size,
        .bitrates = realtek_a_rates,
        .ht_cap.cap = realtek_a_htcap,
        .ht_cap.ht_supported = true,
};
struct rtk_clnt_info {
	struct wpa_ie_info	wpa_ie;
	struct rsn_ie_info	rsn_ie;
	unsigned char assoc_req[MAX_ASSOC_REQ_LEN];
	unsigned char assoc_req_len;
	unsigned char assoc_rsp[MAX_ASSOC_RSP_LEN];
	unsigned char assoc_rsp_len;
};

struct rtk_iface_info {
	unsigned char used;
	unsigned char ndev_name[32];
	struct rtl8192cd_priv *priv;
};

struct rtknl {
	struct class *cl;
	struct device *dev;
	struct wiphy *wiphy;
	struct rtl8192cd_priv *priv;
	struct net_device *ndev_add;
	struct rtk_clnt_info clnt_info;
	unsigned char num_vif;
	int 		idx_vif;
	unsigned char num_vap;
	unsigned char num_vxd;
	unsigned int  vif_flag;
	unsigned char wiphy_registered;
	unsigned int  cipher;
	unsigned char ndev_name[VIF_NUM][VIF_NAME_SIZE];
	unsigned char ndev_name_vxd[VIF_NAME_SIZE];
	unsigned char root_ifname[VIF_NAME_SIZE];
	unsigned char root_mac[ETH_ALEN];
	struct rtl8192cd_priv *priv_root;
	struct rtl8192cd_priv *priv_vxd;
	struct rtk_iface_info rtk_iface[VIF_NUM+2];
	//for survey_dump
	unsigned int chbusytime;
	unsigned int rx_time;
	unsigned int tx_time;
	//openwrt_psd
	unsigned int psd_chnl;
	unsigned int psd_bw;
	unsigned int psd_pts;
	unsigned int psd_fft_info[1040];
};

unsigned char is_WRT_scan_iface(unsigned char* if_name); //eric-vap
void realtek_cfg80211_inform_ss_result(struct rtl8192cd_priv *priv);
struct rtknl *realtek_cfg80211_create(void); 
int realtek_rtknl_init(struct rtknl *rtk);
int realtek_cfg80211_init(struct rtknl *rtk,struct rtl8192cd_priv *priv); 
int realtek_interface_add(struct rtl8192cd_priv *priv, struct rtknl *rtk, const char *name, 
								enum nl80211_iftype type, u8 fw_vif_idx, u8 nw_type);
int event_indicate_cfg80211(struct rtl8192cd_priv *priv, unsigned char *mac, int event, unsigned char *extra);
void close_vxd_vap(struct rtl8192cd_priv *priv_root);
int check_5M10M_config(struct rtl8192cd_priv *priv);



#endif /* RTK_NL80211 */
