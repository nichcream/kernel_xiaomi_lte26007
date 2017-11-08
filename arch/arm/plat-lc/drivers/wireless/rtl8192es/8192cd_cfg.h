/*
 *	 Headler file defines some configure options and basic types
 *
 *	 $Id: 8192cd_cfg.h,v 1.59.2.26 2011/01/10 07:49:07 jerryko Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _8192CD_CFG_H_
#define _8192CD_CFG_H_

#if defined(CONFIG_RTL_ULINKER_BRSC)
#include "linux/ulinker_brsc.h"
#endif

//#define _LITTLE_ENDIAN_
//#define _BIG_ENDIAN_

//this is for WLAN HAL driver coexist with not HAL driver for code size reduce
#ifdef CONFIG_RTL_WLAN_HAL_NOT_EXIST
#define CONFIG_WLAN_NOT_HAL_EXIST 1
#else
#define CONFIG_WLAN_NOT_HAL_EXIST 0//96e_92e, 8881a_92e, 8881a_only, 96d_92er, is only HAL driver
#endif

#ifdef __MIPSEB__

#ifndef _BIG_ENDIAN_
	#define _BIG_ENDIAN_
#endif

#ifdef _LITTLE_ENDIAN_
#undef _LITTLE_ENDIAN_
#endif
//### add by sen_liu 2011.4.14 CONFIG_NET_PCI defined in V2.4 and CONFIG_PCI
// define now to replace it. However,some modules still use CONFIG_NET_PCI
#ifdef CONFIG_PCI
#define CONFIG_NET_PCI
#endif

//### end
#endif	//__MIPSEB__

#ifdef __KERNEL__
#include <linux/version.h>

#if LINUX_VERSION_CODE >= 0x020614 // linux 2.6.20
	#define LINUX_2_6_20_
#endif

#if LINUX_VERSION_CODE >= 0x020615 // linux 2.6.21
	#define LINUX_2_6_21_
#endif

#if LINUX_VERSION_CODE >= 0x020616 // linux 2.6.22
	#define LINUX_2_6_22_
#endif

#if LINUX_VERSION_CODE >= 0x020618 // linux 2.6.24
	#define LINUX_2_6_24_
#endif

#if LINUX_VERSION_CODE >= 0x02061B // linux 2.6.27
	#define LINUX_2_6_27_
#endif

#if LINUX_VERSION_CODE > 0x020600
	#define __LINUX_2_6__
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
	#define __LINUX_3_4__
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	#define __LINUX_3_10__
	#define CONFIG_RTL_PROC_NEW 
#endif

#if defined(LINUX_2_6_20_) || defined(__LINUX_3_4__)
#ifdef CPTCFG_CFG80211_MODULE
#include "../../../../../linux-3.10.32/include/generated/autoconf.h"
#else //CPTCFG_CFG80211_MODULE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#include <linux/kconfig.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif
#endif //CPTCFG_CFG80211_MODULE
#include <linux/jiffies.h>
#include <asm/param.h>
#else
#include <linux/config.h>
#endif
#endif // __KERNEL__

//-------------------------------------------------------------
// Type definition
//-------------------------------------------------------------
#include "typedef.h"

#ifdef __ECOS
	#include <pkgconf/system.h>
	#include <pkgconf/devs_eth_rltk_819x_wrapper.h>
	#include <pkgconf/devs_eth_rltk_819x_wlan.h>
	#include <sys/param.h>
#endif
#ifdef __LINUX_3_4__ // linux 2.6.29
#define NETDEV_NO_PRIV
#endif

/*
 *	Following Definition Sync 2.4/2.6 SDK Definitions
 */

#if !defined(__LINUX_2_6__) && !defined(__ECOS)

#if defined(CONFIG_RTL8196B) || defined(CONFIG_RTL8196C) || defined(CONFIG_RTL8198)
#define CONFIG_RTL_819X
#endif
#if defined(CONFIG_RTL8198)
#define CONFIG_RTL_8198
#endif
#if defined(CONFIG_RTL8196C)
#define CONFIG_RTL_8196C
#endif

#define BSP_PCIE0_D_CFG0 	PCIE0_D_CFG0
#define BSP_PCIE1_D_CFG0 	PCIE0_D_CFG0
#define BSP_PCIE0_H_CFG 	PCIE0_H_CFG
#define BSP_PCIE0_H_EXT 	PCIE0_RC_EXT_BASE
#define BSP_PCIE0_H_MDIO	(BSP_PCIE0_H_EXT + 0x00)
#define BSP_PCIE0_H_INTSTR	(BSP_PCIE0_H_EXT + 0x04)
#define BSP_PCIE0_H_PWRCR	(BSP_PCIE0_H_EXT + 0x08)
#define BSP_PCIE0_H_IPCFG	(BSP_PCIE0_H_EXT + 0x0C)
#define BSP_PCIE0_H_MISC	(BSP_PCIE0_H_EXT + 0x10)
#define BSP_PCIE1_H_CFG 	PCIE1_H_CFG
#define BSP_PCIE1_H_EXT 	PCIE1_RC_EXT_BASE
#define BSP_PCIE1_H_MDIO	(BSP_PCIE1_H_EXT + 0x00)
#define BSP_PCIE1_H_INTSTR	(BSP_PCIE1_H_EXT + 0x04)
#define BSP_PCIE1_H_PWRCR	(BSP_PCIE1_H_EXT + 0x08)
#define BSP_PCIE1_H_IPCFG	(BSP_PCIE1_H_EXT + 0x0C)
#define BSP_PCIE1_H_MISC	(BSP_PCIE1_H_EXT + 0x10)

#endif // !defined(__LINUX_2_6__)

#ifdef __ECOS
//add macro for Realsil WLAN modification
//move the macro definition to cdl file now
//#ifndef CONFIG_RTL_819X_ECOS
//#define CONFIG_RTL_819X_ECOS
//#endif
#endif

#if !defined(__ECOS) && !(defined(CONFIG_OPENWRT_SDK) && defined(__LINUX_3_10__)) //eric-sync ??
#define CONFIG_RTL_CUSTOM_PASSTHRU
#endif

#ifdef CONFIG_RTL_819X_ECOS
#define CONFIG_RTL_CUSTOM_PASSTHRU
#define CONFIG_RTL_CUSTOM_PASSTHRU_PPPOE
#endif

#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
//#define CONFIG_RTL_CUSTOM_PASSTHRU_PPPOE

#define IP6_PASSTHRU_MASK 0x1
#if	defined(CONFIG_RTL_CUSTOM_PASSTHRU_PPPOE)
#define PPPOE_PASSTHRU_MASK 0x1<<1
#endif

#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT)||defined (CONFIG_RTL_8881A)
#define WISP_WLAN_IDX_MASK 0xF0
#define WISP_WLAN_IDX_RIGHT_SHIFT 4
#endif

#endif	/* CONFIG_RTL_CUSTOM_PASSTHRU	*/

#if defined(CONFIG_USE_PCIE_SLOT_0) && defined(CONFIG_USE_PCIE_SLOT_1)
#if defined(CONFIG_RTL_92D_SUPPORT)
#define CONFIG_RTL_DUAL_PCIESLOT_BIWLAN_D 1
#else
#define CONFIG_RTL_DUAL_PCIESLOT_BIWLAN 1
#endif
#endif

#if defined(CONFIG_RTL_92D_DMDP) \
||  defined(CONFIG_RTL_DUAL_PCIESLOT_BIWLAN)\
|| (defined(CONFIG_USE_PCIE_SLOT_0)  && defined(CONFIG_WLAN_HAL_8881A)) \
|| (defined(CONFIG_USE_PCIE_SLOT_0) && defined(CONFIG_USE_PCIE_SLOT_1)) 
#define CONCURRENT_MODE

#if defined(CONFIG_RTL_92D_DMDP) && !defined(CONFIG_RTL_DUAL_PCIESLOT_BIWLAN) && !defined(CONFIG_RTL_92C_SUPPORT) && !defined(CONFIG_RTL_DUAL_PCIESLOT_BIWLAN_D)
#define DUALBAND_ONLY
#endif
#endif


#ifdef CONFIG_RTK_VOIP_BOARD
	#define	RTL_MAX_PCIE_SLOT_NUM	1
	#define	RTL_USED_PCIE_SLOT		0
#elif defined(CONFIG_RTL_8198)
	#define	RTL_MAX_PCIE_SLOT_NUM	2
	#ifdef CONFIG_SLOT_0_92D
		#define RTL_USED_PCIE_SLOT	0
	#else
		#define	RTL_USED_PCIE_SLOT	1
	#endif
#elif defined(CONFIG_RTL_8197D) || defined(CONFIG_RTL_8197DL) || defined(CONFIG_RTL_8198C)
	#define	RTL_MAX_PCIE_SLOT_NUM	2
	#ifdef CONFIG_SLOT_0_92D
		#define RTL_USED_PCIE_SLOT	0
	#elif defined(CONFIG_USE_PCIE_SLOT_1)
		#define	RTL_USED_PCIE_SLOT	1
	#else
		#define RTL_USED_PCIE_SLOT	0
	#endif
#else
	#define	RTL_MAX_PCIE_SLOT_NUM	1
	#define	RTL_USED_PCIE_SLOT		0
#endif

#ifdef CONFIG_RTK_MESH
#include "../mesh_ext/mesh_cfg.h"
#define RTL_MESH_TXCACHE
#define RTK_MESH_MANUALMETRIC
#define RTK_MESH_AODV_STANDALONE_TIMER
//#define RTK_MESH_REDIRECT_TO_ROOT
#define RTK_MESH_REMOVE_PATH_AFTER_AODV_TIMEOUT
#endif

#if defined(CONFIG_RTL8196B) || defined(CONFIG_RTL_819X)
	#if defined(CONFIG_RTL8196B_AP_ROOT) || defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL8196B_GW) || defined(CONFIG_RTL_8196C_GW) || defined(CONFIG_RTL_8198_GW) || defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196B_TLD) || defined(CONFIG_RTL8196C_AP_ROOT) || defined(CONFIG_RTL8196C_AP_HCM) || defined(CONFIG_RTL8198_AP_ROOT) || defined(CONFIG_RTL_8198_AP_ROOT) || defined(CONFIG_RTL8196C_CLIENT_ONLY) || defined(CONFIG_RTL_8198_NFBI_BOARD) || defined(CONFIG_RTL8196C_KLD) || defined(CONFIG_RTL8196C_EC) || defined(CONFIG_RTL_8196C_iNIC) || defined(CONFIG_RTL_8198_INBAND_AP) || defined(CONFIG_RTL_819XD) || defined(CONFIG_RTL_8196E) || defined(CONFIG_RTL_8198B) || defined(__ECOS) || defined(CONFIG_RTL_8198C)
		#define USE_RTL8186_SDK
	#endif
#endif

#if !defined(_BIG_ENDIAN_) && !defined(_LITTLE_ENDIAN_)
	#error "Please specify your ENDIAN type!\n"
#elif defined(_BIG_ENDIAN_) && defined(_LITTLE_ENDIAN_)
	#error "Only one ENDIAN should be specified\n"
#endif

#define PCI_CONFIG_COMMAND			(wdev->conf_addr+4)
#define PCI_CONFIG_LATENCY			(wdev->conf_addr+0x0c)
#define PCI_CONFIG_BASE0			(wdev->conf_addr+0x10)
#define PCI_CONFIG_BASE1			(wdev->conf_addr+0x18)

#define MAX_NUM(_x_, _y_)	(((_x_)>(_y_))? (_x_) : (_y_))
#define MIN_NUM(_x_, _y_)	(((_x_)<(_y_))? (_x_) : (_y_))

#define POWER_MIN_CHECK(a,b)            (((a) > (b)) ? (b) : (a))
#define POWER_RANGE_CHECK(val)		(((val) > 0x3f)? 0x3f : ((val < 0) ? 0 : val))
#define COUNT_SIGN_OFFSET(val, oft)	(((oft & 0x08) == 0x08)? (val - (0x10 - oft)) : (val + oft))

//-------------------------------------------------------------
// Driver version information
//-------------------------------------------------------------
#define DRV_VERSION_H	1
#define DRV_VERSION_L	7
#define DRV_RELDATE		"2014-09-19"
#define DRV_NAME		"Realtek WLAN driver"


//-------------------------------------------------------------
// Will check type for endian issue when access IO and memory
//-------------------------------------------------------------
#define CHECK_SWAP


//-------------------------------------------------------------
// Defined when include proc file system
//-------------------------------------------------------------
#define INCLUDE_PROC_FS
#if defined(CONFIG_PROC_FS) && defined(INCLUDE_PROC_FS)
	#define _INCLUDE_PROC_FS_
#endif


//-------------------------------------------------------------
// Debug function
//-------------------------------------------------------------
#define _DEBUG_RTL8192CD_		// defined when debug print is used
#define _IOCTL_DEBUG_CMD_		// defined when read/write register/memory command is used in ioctl


//-------------------------------------------------------------
// Defined when internal DRAM is used for sw encryption/decryption
//-------------------------------------------------------------
#ifdef __MIPSEB__
	// disable internal ram for nat speedup
	//#define _USE_DRAM_
#endif


//-------------------------------------------------------------
// Support 8188C/8192C test chip
//-------------------------------------------------------------
#if !defined(CONFIG_RTL8196B_GW_8M) //&&!defined(CONFIG_RTL_92D_SUPPORT)
#define TESTCHIP_SUPPORT
#endif

//-------------------------------------------------------------
// Support software tx queue
// ------------------------------------------------------------
#ifdef CONFIG_PCI_HCI
#define SW_TX_QUEUE
#endif

//-------------------------------------------------------------
// RF MIMO SWITCH
// ------------------------------------------------------------
#ifdef CONFIG_PCI_HCI
#define RF_MIMO_SWITCH
#define IDLE_T0			(10*HZ)
#endif

//-------------------------------------------------------------
// Support Tx Report
//-------------------------------------------------------------
#define TXREPORT
#ifdef TXREPORT
#define DETECT_STA_EXISTANCE
#endif
//#define LEAVESTADETECT


//-------------------------------------------------------------
// PCIe power saving function
//-------------------------------------------------------------
#ifdef CONFIG_PCIE_POWER_SAVING
#if !defined(CONFIG_NET_PCI) && !defined(CONFIG_RTL_8196CS) &&  !defined(CONFIG_RTL_88E_SUPPORT)
#define PCIE_POWER_SAVING
#endif

#endif

#ifdef _SINUX_
#define PCIE_POWER_SAVING
#endif

#ifdef PCIE_POWER_SAVING
	#define GPIO_WAKEPIN
	#define FIB_96C
//	#define PCIE_POWER_SAVING_DEBUG
//	#define ASPM_ENABLE
#ifdef PCIE_POWER_SAVING_DEBUG
	#define PCIE_L2_ENABLE
#endif

#define CONFIG_SLOT0H	0xb8b00000
#define CONFIG_SLOT0S	0xb8b10000
#define CONFIG_SLOT1H	0xb8b20000
#define CONFIG_SLOT1S	0xb8b30000


#if defined(CONFIG_RTL_8198) || defined(CONFIG_RTL_819XD) || defined(CONFIG_RTL_8196E)
#define CFG_92C_SLOTH		CONFIG_SLOT0H
#define CFG_92C_SLOTS		CONFIG_SLOT0S
#if (RTL_USED_PCIE_SLOT==1)
#define CFG_92D_SLOTH		CONFIG_SLOT1H
#define CFG_92D_SLOTS		CONFIG_SLOT1S
#else
#define CFG_92D_SLOTH		CONFIG_SLOT0H
#define CFG_92D_SLOTS		CONFIG_SLOT0S
#endif
#elif defined(CONFIG_RTL_8196C)
#define CFG_92C_SLOTH		CONFIG_SLOT0H
#define CFG_92C_SLOTS		CONFIG_SLOT0S
#define CFG_92D_SLOTH		CONFIG_SLOT0H
#define CFG_92D_SLOTS		CONFIG_SLOT0S
#endif


#endif


//-------------------------------------------------------------
// WDS function support
//-------------------------------------------------------------
#if defined(CONFIG_RTL_WDS_SUPPORT)
#define WDS
//	#define LAZY_WDS
#endif


//-------------------------------------------------------------
// Pass EAP packet by event queue
//-------------------------------------------------------------
#define EAP_BY_QUEUE
#undef EAPOLSTART_BY_QUEUE	// jimmylin: don't pass eapol-start up
							// due to XP compatibility issue
//#define USE_CHAR_DEV
#define USE_PID_NOTIFY


//-------------------------------------------------------------
// WPA Supplicant 
//-------------------------------------------------------------
//#define WIFI_WPAS

#ifdef WIFI_WPAS
#ifndef WIFI_HAPD
#define WIFI_HAPD
#endif

#ifndef CLIENT_MODE
#define CLIENT_MODE
#endif
#endif

#ifdef WIFI_WPAS_CLI
#define WIFI_WPAS
#define RF_MIMO_PS
//#define IDLE_T0 (3*HZ)

#ifndef CLIENT_MODE
#define CLIENT_MODE
#endif
#endif

//-------------------------------------------------------------
// Client mode function support
//-------------------------------------------------------------
#if defined(CONFIG_RTL_CLIENT_MODE_SUPPORT)
#define CLIENT_MODE
#endif

#ifdef CLIENT_MODE
	#define RTK_BR_EXT		// Enable NAT2.5 and MAC clone support
	#define CL_IPV6_PASS	// Enable IPV6 pass-through. RTK_BR_EXT must be defined
#if defined(__ECOS) && defined(CONFIG_SDIO_HCI) 
	#undef RTK_BR_EXT
	#undef CL_IPV6_PASS
#endif
#endif

#ifdef CONFIG_RTL_SUPPORT_MULTI_PROFILE
	#define SUPPORT_MULTI_PROFILE		// support multiple AP profile
#endif


//-------------------------------------------------------------
// Defined when WPA2 is used
//-------------------------------------------------------------
#define RTL_WPA2
#define RTL_WPA2_PREAUTH


//-------------------------------------------------------------
// MP test
//-------------------------------------------------------------
#if (!defined(CONFIG_RTL8196B_GW_8M) || defined(CONFIG_RTL8196B_GW_MP))
#define MP_TEST
#ifdef CONFIG_RTL_92D_SUPPORT
#endif
#endif


//-------------------------------------------------------------
// MIC error test
//-------------------------------------------------------------
//#define MICERR_TEST


//-------------------------------------------------------------
// Log event
//-------------------------------------------------------------
#define EVENT_LOG


//-------------------------------------------------------------
// Tx/Rx data path shortcut
//-------------------------------------------------------------
#define TX_SHORTCUT
#define RX_SHORTCUT
#if defined(CONFIG_RTK_MESH) && defined(RX_SHORTCUT)
#define RX_RL_SHORTCUT
#endif
#ifdef __ECOS
#ifdef RTLPKG_DEVS_ETH_RLTK_819X_BRSC
#define BR_SHORTCUT
#endif
#elif !defined(CONFIG_RTL_FASTBRIDGE)
#ifdef CONFIG_RTL8672
#ifndef CONFIG_RTL8672_BRIDGE_FASTPATH
//#define BR_SHORTCUT
#endif
#else
#define BR_SHORTCUT //mark_apo
#if 0	//jwj
#if !defined(CONFIG_RTL_8198B) && !defined(CONFIG_RTL8196C_KLD)
#define BR_SHORTCUT_C2
#define BR_SHORTCUT_C3
#define BR_SHORTCUT_C4
#endif
#endif
#endif
#endif
#if defined(CONFIG_RTK_MESH) && defined(TX_SHORTCUT)
	#define MESH_TX_SHORTCUT
#endif

#define TX_SC_ENTRY_NUM		4
#define RX_SC_ENTRY_NUM		4

//Filen
//#define SHORTCUT_STATISTIC

//-------------------------------------------------------------
// Mininal Memory usage
//-------------------------------------------------------------
#if defined(CONFIG_MEM_LIMITATION) || defined(CONFIG_RTL_NFJROM_MP)
#define WIFI_LIMITED_MEM
#endif

#ifdef WIFI_LIMITED_MEM
#define WIFI_MIN_IMEM_USAGE	// Mininal IMEM usage
#undef TESTCHIP_SUPPORT
#endif


//-------------------------------------------------------------
// back to back test
//-------------------------------------------------------------
//#define B2B_TEST


//-------------------------------------------------------------
// enable e-fuse read write
//-------------------------------------------------------------
#ifdef CONFIG_ENABLE_EFUSE
#define EN_EFUSE
#endif


//-------------------------------------------------------------
// new Auto channel
//-------------------------------------------------------------
#define CONFIG_RTL_NEW_AUTOCH
#define MAC_RX_COUNT_THRESHOLD		200
#ifdef CONFIG_SDIO_HCI
#define AUTOCH_SS_SPEEDUP
#endif

//-------------------------------------------------------------
// new IQ calibration for 92c / 88c
//-------------------------------------------------------------
#define CONFIG_RTL_NEW_IQK


//-------------------------------------------------------------
// noise control
//-------------------------------------------------------------
#ifdef CONFIG_RTL_92C_SUPPORT
//#define CONFIG_RTL_NOISE_CONTROL_92C
#endif


//-------------------------------------------------------------
// Universal Repeater (support AP + Infra client concurrently)
//-------------------------------------------------------------
#if defined(CONFIG_RTL_REPEATER_MODE_SUPPORT)
#define UNIVERSAL_REPEATER
#define SMART_REPEATER_MODE
#define	SWITCH_CHAN

#endif

//-------------------------------------------------------------
// Check hangup for Tx queue
//-------------------------------------------------------------
#ifdef CONFIG_PCI_HCI
#define CHECK_HANGUP
#endif

#ifdef CHECK_HANGUP
	#define CHECK_TX_HANGUP
	#define CHECK_RX_DMA_ERROR
	#define CHECK_RX_TAG_ERROR
	#define CHECK_LX_DMA_ERROR
	#define FAST_RECOVERY
#endif


//-------------------------------------------------------------
// DFS
//-------------------------------------------------------------
#ifdef CONFIG_RTL_DFS_SUPPORT
#define DFS
#endif


//-------------------------------------------------------------
// Driver based WPA PSK feature
//-------------------------------------------------------------
#define INCLUDE_WPA_PSK


//eric-sync ?? bind with openwrt-sdk ??
//-------------------------------------------------------------
// NL80211
//-------------------------------------------------------------
//
#ifdef CONFIG_OPENWRT_SDK
#define NETDEV_NO_PRIV 1  //mark_wrt
#undef EVENT_LOG //mark_wrt

#if defined(CPTCFG_CFG80211_MODULE)
#define RTK_NL80211 1
#endif

#ifdef RTK_NL80211
//#define CONFIG_NET_PCI
#undef EAP_BY_QUEUE
#undef INCLUDE_WPA_PSK

#undef BR_SHORTCUT_C2
#undef BR_SHORTCUT_C3
#undef BR_SHORTCUT_C4

#ifndef CONFIG_RTL_VAP_SUPPORT
#define CONFIG_RTL_VAP_SUPPORT 1
#endif

#ifndef CONFIG_RTL_REPEATER_MODE_SUPPORT
#define CONFIG_RTL_REPEATER_MODE_SUPPORT 1
#endif

#endif //RTK_NL80211
#endif //CONFIG_OPENWRT_SDK

//-------------------------------------------------------------
// RF Fine Tune
//-------------------------------------------------------------
//#define RF_FINETUNE


//-------------------------------------------------------------
// Wifi WMM
//-------------------------------------------------------------
#define WIFI_WMM
#ifdef WIFI_WMM
	#define WMM_APSD	// WMM Power Save
	#ifndef WIFI_LIMITED_MEM
	#define RTL_MANUAL_EDCA		// manual EDCA parameter setting
	#endif
#endif

//-------------------------------------------------------------
// Hotspot 2.0
//-------------------------------------------------------------
#ifdef CONFIG_RTL_HS2_SUPPORT
#define HS2_SUPPORT
//#define HS2_CLIENT_TEST
#endif

//-------------------------------------------------------------
// Hostapd 
//-------------------------------------------------------------
#ifdef CONFIG_RTL_HOSTAPD_SUPPORT 
#define WIFI_HAPD
#endif
#ifdef CONFIG_RTL_P2P_SUPPORT
#define P2P_SUPPORT  //  support for WIFI_Direct
//#define P2P_DEBUGMSG
#endif

#ifdef CONFIG_PACP_SUPPORT
#define SUPPORT_MONITOR			// for packet capture function
#ifndef CONFIG_RTL_COMAPI_WLTOOLS
#define CONFIG_RTL_COMAPI_WLTOOLS
#endif
#endif

#ifdef CONFIG_RTL_TDLS_SUPPORT
#define TDLS_SUPPORT 
#endif

#ifdef WIFI_HAPD

//#define HAPD_DRV_PSK_WPS

#ifndef CONFIG_WEXT_PRIV
#define CONFIG_WEXT_PRIV
#endif

#ifndef CONFIG_RTL_COMAPI_WLTOOLS
#define CONFIG_RTL_COMAPI_WLTOOLS
#endif

#ifndef HAPD_DRV_PSK_WPS
#undef EAP_BY_QUEUE
#undef INCLUDE_WPA_PSK
#endif

#endif


//-------------------------------------------------------------
// IO mapping access
//-------------------------------------------------------------
//#define IO_MAPPING


//-------------------------------------------------------------
// Wifi Simple Config support
//-------------------------------------------------------------
#define WIFI_SIMPLE_CONFIG
/* WPS2DOTX   */
#define WPS2DOTX
#define OUI_LEN					4

#ifdef WPS2DOTX
#define SUPPORT_PROBE_REQ_REASSEM	//for AP mode
#define SUPPORT_PROBE_RSP_REASSEM	// for STA mode
//#define WPS2DOTX_DEBUG
#endif

#ifdef	WPS2DOTX_DEBUG	  //0614 for wps2.0  trace
#define SECU_DEBUG(fmt, args...)  printk("[secu]%s %d:"fmt, __FUNCTION__,__LINE__, ## args)
#define SME_DEBUG(fmt, args...) printk("[sme]%s %d:"fmt,__FUNCTION__ , __LINE__ , ## args)

#else
#define SECU_DEBUG(fmt, args...)
#define SME_DEBUG(fmt, args...) 
#endif
/* WPS2DOTX   */

//-------------------------------------------------------------
// Support Multiple BSSID
//-------------------------------------------------------------
#if defined(CONFIG_RTL_VAP_SUPPORT)
#define MBSSID
#endif

#ifndef NOT_RTK_BSP
#ifdef MBSSID
#if !defined(CONFIG_RTL_SDRAM_GE_32M) && defined(CONFIG_RTL_8196E)
#define RTL8192CD_NUM_VWLAN  1
#else
#define RTL8192CD_NUM_VWLAN  4
#endif
#else
#define RTL8192CD_NUM_VWLAN  0
#endif
#endif // !NOT_RTK_BSP


#if defined(CLIENT_MODE) && defined(CONFIG_RTL_MULTI_CLONE_SUPPORT) && defined(MBSSID)
	#define MULTI_MAC_CLONE // Enable mac clone to multiple Ethernet MAC address
#endif
#ifdef MULTI_MAC_CLONE
#define REPEATER_TO		RTL_SECONDS_TO_JIFFIES(10)
#endif


//-------------------------------------------------------------
// Support Tx Descriptor Reservation for each interface
//-------------------------------------------------------------
#ifdef CONFIG_RTL_TX_RESERVE_DESC
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
#define RESERVE_TXDESC_FOR_EACH_IF
#endif
#endif


//-------------------------------------------------------------
// Group BandWidth Control
//-------------------------------------------------------------
#ifdef CONFIG_PCI_HCI
#define GBWC
#endif

//-------------------------------------------------------------
// Support add or remove ACL list at run time
//-------------------------------------------------------------
#define D_ACL

//-------------------------------------------------------------
// Support 802.11 SNMP MIB
//-------------------------------------------------------------
//#define SUPPORT_SNMP_MIB


//-------------------------------------------------------------
// Driver-MAC loopback
//-------------------------------------------------------------
//#define DRVMAC_LB


//-------------------------------------------------------------
// Use perfomance profiling
//-------------------------------------------------------------
//#define PERF_DUMP
#ifdef PERF_DUMP
//3 Definition
//Check List Selection
#define PERF_DUMP_INIT_ORI          0
#define PERF_DUMP_INIT_WLAN_TRX     1

//CP3 of MIPS series count event format selection
#define PERF_DUMP_CP3_OLD     0
#define PERF_DUMP_CP3_NEW     1

//3 Control Setting
#define PERF_DUMP_INIT_SELECT   PERF_DUMP_INIT_WLAN_TRX

// TODO: Filen, I am not sure that which type these are for 97D/96E/96D
#if defined(CONFIG_RTL_8881A)
#define PERF_DUMP_CP3_SELECT    PERF_DUMP_CP3_NEW
#else
// 96C / ...
#define PERF_DUMP_CP3_SELECT    PERF_DUMP_CP3_OLD
#endif

//Dual Counter mode, only new chip support
#if (PERF_DUMP_CP3_SELECT == PERF_DUMP_CP3_NEW)
#define PERF_DUMP_CP3_DUAL_COUNTER_EN
#else
#undef PERF_DUMP_CP3_DUAL_COUNTER_EN
#endif
#endif //PERF_DUMP


//-------------------------------------------------------------
// 1x1 Antenna Diversity
//-------------------------------------------------------------
#if defined (CONFIG_ANT_SWITCH) || defined(CONFIG_RTL_8881A_ANT_SWITCH) || defined(CONFIG_SLOT_0_ANT_SWITCH) || defined(CONFIG_SLOT_1_ANT_SWITCH)
//#define SW_ANT_SWITCH
#define HW_ANT_SWITCH
//#define GPIO_ANT_SWITCH
#ifdef HW_ANT_SWITCH
#define HW_DIV_ENABLE	(priv->pshare->rf_ft_var.antHw_enable&1)
#endif
#ifdef SW_ANT_SWITCH
#define SW_DIV_ENABLE	(priv->pshare->rf_ft_var.antSw_enable&1)
#endif
#endif


//-------------------------------------------------------------
// WPAI performance issue
//-------------------------------------------------------------
#ifdef CONFIG_RTL_WAPI_SUPPORT
	//#define IRAM_FOR_WIRELESS_AND_WAPI_PERFORMANCE
	#if defined(CONFIG_RTL8192CD)
	// if CONFIG_RTL_HW_WAPI_SUPPORT defined, { SWCRYPTO=1: sw wapi; SWCRYPTO=0: hw wapi. }
	// if CONFIG_RTL_HW_WAPI_SUPPORT not defined, { SWCRYPTO=1: sw wapi;  SWCRYPTO=0: should not be used! }
	#define	CONFIG_RTL_HW_WAPI_SUPPORT		1
	#endif
#endif


//-------------------------------------------------------------
// Use local ring for pre-alloc Rx buffer.
// If no defined, will use kernel skb que
//-------------------------------------------------------------
#ifndef __ECOS
#define RTK_QUE
#endif


//-------------------------------------------------------------
//Support IP multicast->unicast
//-------------------------------------------------------------
#ifndef __ECOS
#define SUPPORT_TX_MCAST2UNI
#define MCAST2UI_REFINE
#endif

#ifdef CONFIG_RTL_819X_ECOS
#define SUPPORT_TX_MCAST2UNI
//#define MCAST2UI_REFINE
#endif

#ifdef CLIENT_MODE
#define SUPPORT_RX_UNI2MCAST
#endif

/* for cameo feature*/
#ifdef CONFIG_RTL865X_CMO
	#define IGMP_FILTER_CMO
#endif

// Support  IPV6 multicast->unicast
#ifdef	SUPPORT_TX_MCAST2UNI
	#define	TX_SUPPORT_IPV6_MCAST2UNI
#endif


//-------------------------------------------------------------
// Support  USB tx rate adaptive
//-------------------------------------------------------------
// define it always for object code release
#if defined(CONFIG_USB) && !defined(NOT_RTK_BSP)
	#define USB_PKT_RATE_CTRL_SUPPORT
#endif


//-------------------------------------------------------------
// Support Tx AMSDU
//-------------------------------------------------------------
//#define SUPPORT_TX_AMSDU


//-------------------------------------------------------------
// Mesh Network
//-------------------------------------------------------------
#ifdef CONFIG_RTK_MESH
#define _MESH_ACL_ENABLE_

/*need check Tx AMSDU dependency ; 8196B no support now */
#ifdef	SUPPORT_TX_AMSDU
#define MESH_AMSDU
#endif
//#define MESH_ESTABLISH_RSSI_THRESHOLD
//#define MESH_BOOTSEQ_AUTH
#endif // CONFIG_RTK_MESH


//-------------------------------------------------------------
// Realtek proprietary wake up on wlan mode
//-------------------------------------------------------------
//#define RTK_WOW


//-------------------------------------------------------------
// Use static buffer for STA private buffer
//-------------------------------------------------------------
#if !defined(CONFIG_RTL8196B_GW_8M) && !defined(CONFIG_WIRELESS_LAN_MODULE) && !defined(WIFI_LIMITED_MEM)
#define PRIV_STA_BUF
#endif

//-------------------------------------------------------------
// Do not drop packet immediately when rx buffer empty
//-------------------------------------------------------------

#ifdef __ECOS
#ifdef CONFIG_RTL_DELAY_REFILL
#define DELAY_REFILL_RX_BUF
#endif
#endif


#ifdef CONFIG_RTL8190_PRIV_SKB
	#define DELAY_REFILL_RX_BUF
#endif


//-------------------------------------------------------------
// WiFi 11n 20/40 coexistence
//-------------------------------------------------------------
#define WIFI_11N_2040_COEXIST
#define WIFI_11N_2040_COEXIST_EXT

//-------------------------------------------------------------
// Add TX power by command
//-------------------------------------------------------------
#define ADD_TX_POWER_BY_CMD


//-------------------------------------------------------------
// Do Rx process in tasklet
//-------------------------------------------------------------
//#define RX_TASKLET


//-------------------------------------------------------------
// Support external high power PA
//-------------------------------------------------------------
#if defined(CONFIG_SLOT_0_EXT_PA) || defined(CONFIG_SLOT_1_EXT_PA)
#define HIGH_POWER_EXT_PA
#endif


//-------------------------------------------------------------
// Support external LNA
//-------------------------------------------------------------
#if defined(CONFIG_SLOT_0_EXT_LNA) || defined(CONFIG_SLOT_1_EXT_LNA)
#define HIGH_POWER_EXT_LNA
#endif


//-------------------------------------------------------------
// Cache station info for bridge
//-------------------------------------------------------------
#define RTL_CACHED_BR_STA


//-------------------------------------------------------------
// Bridge shortcut for AP to AP case
//-------------------------------------------------------------
#define AP_2_AP_BRSC

//-------------------------------------------------------------
// Use default keys of WEP (instead of keymapping keys)
//-------------------------------------------------------------
//#define USE_WEP_DEFAULT_KEY


//-------------------------------------------------------------
// Auto test support
//-------------------------------------------------------------
//#ifdef CONFIG_RTL_92C_SUPPORT
#define AUTO_TEST_SUPPORT
//#endif


//-------------------------------------------------------------
// to prevent broadcast storm attacks
//-------------------------------------------------------------
#define PREVENT_BROADCAST_STORM	1

#ifdef PREVENT_BROADCAST_STORM
/*
 *	NOTE: The driver will skip the other broadcast packets if the system free memory is less than FREE_MEM_LOWER_BOUND 
 *		   and the broadcast packet amount is larger than BROADCAST_STORM_THRESHOLD in one second period.
 */

#define BROADCAST_STORM_THRESHOLD		16
#define FREE_MEM_LOWER_BOUND			800 //uint: KBytes
#endif


//-------------------------------------------------------------
// Video streaming refine
//-------------------------------------------------------------
#ifdef CONFIG_RTK_VLC_SPEEDUP_SUPPORT
	#define VIDEO_STREAMING_REFINE
#endif


//-------------------------------------------------------------
// Rx buffer gather feature
//-------------------------------------------------------------
#define RX_BUFFER_GATHER

//-------------------------------------------------------------
// A4 client support
//-------------------------------------------------------------
//#define A4_STA

#if defined(A4_STA) && defined(WDS)
	#error "A4_STA and WDS can't be used together\n"
#endif


//-------------------------------------------------------------
// Avoid deadlock for SMP(Symmetrical Multi-Processing) architecture
//-------------------------------------------------------------
#if defined(CONFIG_SMP) && !defined(SMP_SYNC)
#define SMP_SYNC
#endif


//-------------------------------------------------------------
// NOT use Realtek specified BSP
//-------------------------------------------------------------
//#define NOT_RTK_BSP


//-------------------------------------------------------------
// No padding between members in the structure for specified CPU
//-------------------------------------------------------------
//#define PACK_STRUCTURE


//-------------------------------------------------------------
// customers proprietary info display
//-------------------------------------------------------------
//#define TLN_STATS


//-------------------------------------------------------------
// Tx early mode
//-------------------------------------------------------------
#ifdef CONFIG_RTL_TX_EARLY_MODE_SUPPORT
#ifdef CONFIG_RTL_88E_SUPPORT
	#define TX_EARLY_MODE
#endif
#endif

//-------------------------------------------------------------
// Dynamically switch LNA for 97d High Power in MP mode to pass Rx Test
//-------------------------------------------------------------
#if defined(CONFIG_RTL_819XD) && !defined(CONFIG_RTL_8196D) && defined(CONFIG_HIGH_POWER_EXT_PA) //for 97d High Power only
//#define MP_SWITCH_LNA
#endif


//-------------------------------------------------------------
// RTL8188E GPIO Control
//-------------------------------------------------------------
#define RTLWIFINIC_GPIO_CONTROL

//-------------------------------------------------------------
// General interface of add user defined IE.
//-------------------------------------------------------------
//#define USER_ADDIE


//-------------------------------------------------------------
// Tx power limit function
//-------------------------------------------------------------
#ifdef CONFIG_TXPWR_LMT
#define TXPWR_LMT
#ifdef CONFIG_RTL_8812_SUPPORT
#define TXPWR_LMT_8812
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
#define TXPWR_LMT_88E
#endif
#ifdef CONFIG_WLAN_HAL_8881A
#define TXPWR_LMT_8881A
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
#define TXPWR_LMT_92EE
#endif

#if defined(TXPWR_LMT_8812) || defined(TXPWR_LMT_88E) || defined(CONFIG_WLAN_HAL)
#define TXPWR_LMT_NEWFILE
#endif

#endif // CONFIG_TXPWR_LMT

#if 0
#define PWR_BY_RATE_92E_HP
#endif

//-------------------------------------------------------------
// RADIUS Accounting supportive functions
//-------------------------------------------------------------
//#define RADIUS_ACCOUNTING

//-------------------------------------------------------------
// Client mixed mode security
//-------------------------------------------------------------
#ifdef CLIENT_MODE

#ifdef CONFIG_SUPPORT_CLIENT_MIXED_SECURITY
#define SUPPORT_CLIENT_MIXED_SECURITY
#endif

#ifdef RTK_NL80211
#ifdef SUPPORT_CLIENT_MIXED_SECURITY
#undef SUPPORT_CLIENT_MIXED_SECURITY //because 8192cd_psk_hapd.c not patched this fun yet
#endif
#endif

#endif

//-------------------------------------------------------------
// FW Test Function
//-------------------------------------------------------------
#ifdef CONFIG_WLAN_HAL
#define CONFIG_OFFLOAD_FUNCTION
#endif

//-------------------------------------------------------------
// 8814 AP MAC function verification
//-------------------------------------------------------------
#ifdef CONFIG_WLAN_HAL_8814AE
#define HW_FILL_MACID
#define HW_DETEC_POWER_STATE
//#define RTL8814_FPGA_TEMP
//#define DISABLE_BB_RF
//#define CONFIG_8814_AP_MAC_VERI
//#define VERIFY_AP_FAST_EDCA
#endif

//-------------------------------------------------------------
// WLAN HAL
//-------------------------------------------------------------
#ifdef CONFIG_WLAN_HAL

// Add by Eric and Pedro, this compile flag is temp for 8814 FPGA tunning 
// Must remove after driver ready
#ifdef RTL8814_FPGA_TEMP
#define DISABLE_BB_RF
#endif

#ifdef CONFIG_RTL_8198C
#define TRXBD_CACHABLE_REGION
#endif
//#undef CHECK_HANGUP
//#undef FAST_RECOVERY
//#undef SW_TX_QUEUE
//#undef DELAY_REFILL_RX_BUF
#undef RESERVE_TXDESC_FOR_EACH_IF
//#undef CONFIG_NET_PCI
#undef TX_EARLY_MODE
//#undef CONFIG_OFFLOAD_FUNCTION
//#undef PREVENT_BROADCAST_STORM
//#undef DETECT_STA_EXISTANCE
//#undef RX_BUFFER_GATHER
//#undef GBWC
#define _TRACKING_TABLE_FILE
#define AVG_THERMAL_NUM_88XX    4
//#define WLANHAL_MACDM
#endif

#if defined(CONFIG_WLAN_HAL_8814AE) && defined(TX_SHORTCUT)
//#define WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
//#define WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
// TODO: temporary for only turn on WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
//#define WLAN_HAL_HW_TX_SHORTCUT_DISABLE_REUSE_TXDESC_FOR_DEBUG

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
// TODO: turn on REUSE + HDR_CONV
// TODO: recycle problem (sw desc) for HDR_CONV
#define WLAN_HAL_HW_SEQ
#define WLAN_HAL_HW_AES_IV
#undef  TX_SC_ENTRY_NUM
#define TX_SC_ENTRY_NUM			1
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
#endif // defined(CONFIG_WLAN_HAL_8814AE) && defined(TX_SHORTCUT)


#ifdef CONFIG_RTL_8881A
#define WLAN_HAL_TXDESC_CHECK_ADDR_LEN    1
#endif

#ifdef CONFIG_WLAN_HAL_8192EE
//#define BEAMFORMING_SUPPORT                     1
#endif

//#define WMM_DSCP_C42

/*********************************************************************/
/* some definitions in 8192cd driver, we set them as NULL definition */
/*********************************************************************/
#ifdef USE_RTL8186_SDK
#if defined(CONFIG_WIRELESS_LAN_MODULE) || defined(CONFIG_RTL_8198C)
#define __DRAM_IN_865X
#define __IRAM_IN_865X
#else
#define __DRAM_IN_865X		__attribute__ ((section(".dram-rtkwlan")))
#define __IRAM_IN_865X		__attribute__ ((section(".iram-rtkwlan")))
#endif

#define RTL8190_DIRECT_RX						/* For packet RX : directly receive the packet instead of queuing it */
#define RTL8190_ISR_RX							/* process RXed packet in interrupt service routine: It become useful only when RTL8190_DIRECT_RX is defined */

#ifndef CONFIG_WIRELESS_LAN_MODULE
#ifndef CONCURRENT_MODE
#define RTL8192CD_VARIABLE_USED_DMEM			/* Use DMEM for some critical variables */
#endif
#endif

#else // not USE_RTL8186_SDK

#define __DRAM_IN_865X
#define __IRAM_IN_865X
#endif


#undef __MIPS16
#ifdef __ECOS
#if defined(RTLPKG_DEVS_ETH_RLTK_819X_USE_MIPS16) || !defined(CONFIG_RTL_8198C) 
#define __MIPS16			__attribute__ ((mips16))
#else
#define __MIPS16
#endif
#else
#if defined(CONFIG_WIRELESS_LAN_MODULE) || defined(CONFIG_RTL_8198C) 
#define __MIPS16
#else
#define __MIPS16			__attribute__ ((mips16))
#endif
#endif

#ifdef IRAM_FOR_WIRELESS_AND_WAPI_PERFORMANCE
#ifdef CONFIG_RTL_8198C
#define __IRAM_WLAN_HI		//__attribute__ ((section(".iram-wapi")))
#define __DRAM_WLAN_HI		//__attribute__ ((section(".dram-wapi")))
#else
#define __IRAM_WLAN_HI		__attribute__ ((section(".iram-wapi")))
#define __DRAM_WLAN_HI		__attribute__ ((section(".dram-wapi")))
#endif
#endif


//-------------------------------------------------------------
// Kernel 2.6 specific config
//-------------------------------------------------------------
#ifdef __LINUX_2_6__

#define USE_RLX_BSP
#ifndef CONCURRENT_MODE
#define RTL8192CD_VARIABLE_USED_DMEM
#endif

#ifndef RX_TASKLET
	#define	RX_TASKLET
#endif

#endif


#if 0
//-------------------------------------------------------------
// TR define flag
//-------------------------------------------------------------
#if defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL8196C_EC)

#ifndef INCLUDE_WPA_PSK
	#define INCLUDE_WPA_PSK
#endif

#ifdef UNIVERSAL_REPEATER
	#undef UNIVERSAL_REPEATER
#endif

#ifdef CLIENT_MODE
	#undef CLIENT_MODE
	#undef RTK_BR_EXT
#endif

#ifndef WIFI_SIMPLE_CONFIG
	#define WIFI_SIMPLE_CONFIG
#endif

#ifdef GBWC
	#undef GBWC
#endif

#ifdef SUPPORT_SNMP_MIB
	#undef SUPPORT_SNMP_MIB
#endif

#endif // CONFIG_RTL8196B_TR


//-------------------------------------------------------------
// AC define flag
//-------------------------------------------------------------
#ifdef CONFIG_RTL865X_AC

#ifndef INCLUDE_WPA_PSK
	#define INCLUDE_WPA_PSK
#endif

#ifdef UNIVERSAL_REPEATER
	#undef UNIVERSAL_REPEATER
#endif

#ifdef CLIENT_MODE
	#undef CLIENT_MODE
	#undef RTK_BR_EXT
#endif

#ifndef WIFI_SIMPLE_CONFIG
	#define WIFI_SIMPLE_CONFIG
#endif

#ifdef GBWC
	#undef GBWC
#endif

#ifdef SUPPORT_SNMP_MIB
	#undef SUPPORT_SNMP_MIB
#endif

#endif // CONFIG_RTL865X_AC
#endif //#if 0


//-------------------------------------------------------------
// Config Little Endian CPU
//-------------------------------------------------------------
#ifdef _LITTLE_ENDIAN_

#ifndef NOT_RTK_BSP
#define NOT_RTK_BSP
#endif

#ifndef CONFIG_WIRELESS_LAN_MODULE
#define CONFIG_WIRELESS_LAN_MODULE
#endif

#ifdef __MIPSEB__
	#undef __MIPSEB__
#endif

#ifdef _BIG_ENDIAN_
	#undef _BIG_ENDIAN_
#endif

#endif //_LITTLE_ENDIAN_

//-------------------------------------------------------------
// Config if NOT use Realtek specified BSP
//-------------------------------------------------------------
#ifdef NOT_RTK_BSP

#if defined(CONFIG_PCI_HCI) && !defined(CONFIG_NET_PCI)
#define CONFIG_NET_PCI
#endif

#ifndef __LINUX_2_6__
#ifndef __ECOS
#define del_timer_sync del_timer
#endif
#endif

#ifdef __KERNEL__
#if LINUX_VERSION_CODE >= 0x02061D // linux 2.6.29
#define NETDEV_NO_PRIV
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))
#define CONFIG_COMPAT_NET_DEV_OPS
#endif
#endif

#ifdef CONFIG_RTL_CUSTOM_PASSTHRU
#undef CONFIG_RTL_CUSTOM_PASSTHRU
#endif

#ifdef CONFIG_RTL_CUSTOM_PASSTHRU_PPPOE
#undef CONFIG_RTL_CUSTOM_PASSTHRU_PPPOE
#endif

#ifdef _USE_DRAM_
	#undef _USE_DRAM_
#endif

#ifdef _BIG_ENDIAN_
#ifndef CHECK_SWAP
	#define CHECK_SWAP
#endif
#else // !_BIG_ENDIAN_
#ifdef CHECK_SWAP
	#undef CHECK_SWAP
#endif
#endif

#ifdef EVENT_LOG
	#undef EVENT_LOG
#endif

#ifdef BR_SHORTCUT
	#undef BR_SHORTCUT
#endif

#if defined(NOT_RTK_BSP) && defined(BR_SHORTCUT_SUPPORT)
#define BR_SHORTCUT
#endif // NOT_RTK_BSP && BR_SHORTCUT_SUPPORT

//#ifdef RTK_BR_EXT
//	#undef RTK_BR_EXT
//#endif

//#ifdef UNIVERSAL_REPEATER
//	#undef UNIVERSAL_REPEATER
//#endif

#ifdef GBWC
	#undef GBWC
#endif

#ifdef USE_IO_OPS
	#undef USE_IO_OPS
#endif

#ifdef IO_MAPPING
	#undef IO_MAPPING
#endif

#ifdef RTK_QUE
	#undef RTK_QUE
#endif

#ifdef USE_RLX_BSP
#undef USE_RLX_BSP
#endif

#ifdef __ECOS
#ifdef RTLWIFINIC_GPIO_CONTROL
#undef RTLWIFINIC_GPIO_CONTROL
#endif
#endif

// 2013/07/04, Lucien, Enable RX_BUFFER_GATHER to have lower RX_BUF_LEN
// Because pci_map_single/pci_unmap_single has higher time consumption in some non-RTK platforms.
//#ifdef RX_BUFFER_GATHER
//#undef RX_BUFFER_GATHER
//#endif
#define POWER_PERCENT_ADJUSTMENT

// If the CPU's crystal is shared with WIFI, unmark this line
//#define DONT_DISABLE_XTAL_ON_CLOSE

// use seq_file to display big file to avoid buffer overrun when using create_proc_entry
#define CONFIG_RTL_PROC_NEW

#endif //NOT_RTK_BSP


//-------------------------------------------------------------
// Define flag of EC system
//-------------------------------------------------------------
#ifdef CONFIG_RTL8196C_EC

#ifndef USE_WEP_DEFAULT_KEY
	#define USE_WEP_DEFAULT_KEY
#endif

#ifdef TESTCHIP_SUPPORT
	#undef TESTCHIP_SUPPORT
#endif

#endif 


//-------------------------------------------------------------
// MSC define flag
//-------------------------------------------------------------
#ifdef _SINUX_
#define CONFIG_MSC 
#endif

#ifdef CONFIG_MSC
#define INCLUDE_WPS

#ifdef CHECK_HANGUP
	#undef CHECK_HANGUP
#endif
#ifndef USE_WEP_DEFAULT_KEY         //2010.4.23 Fred Fu open it to support wep key index 1 2 3
	#define USE_WEP_DEFAULT_KEY
#endif

#ifdef WIFI_SIMPLE_CONFIG
#ifdef	INCLUDE_WPS
#define USE_PORTING_OPENSSL
#endif
#endif

#endif

//-------------------------------------------------------------
// Define flag of 8672 system
//-------------------------------------------------------------
#ifdef CONFIG_RTL8672

#ifndef RX_TASKLET
	#define RX_TASKLET
#endif

#ifdef RTL8190_DIRECT_RX
	#undef RTL8190_DIRECT_RX
#endif

#ifdef RTL8190_ISR_RX
	#undef RTL8190_ISR_RX
#endif

#if defined(USE_RLX_BSP) && !defined(LINUX_2_6_22_)
	#undef USE_RLX_BSP
#endif

#undef TX_SC_ENTRY_NUM
#define TX_SC_ENTRY_NUM		2

#ifdef __DRAM_IN_865X
	#undef __DRAM_IN_865X
#endif
#ifdef CONFIG_RTL_8198C
#define __DRAM_IN_865X		//__attribute__ ((section(".dram-rtkwlan")))
#else
#define __DRAM_IN_865X		__attribute__ ((section(".dram-rtkwlan")))
#endif
#ifdef __IRAM_IN_865X
	#undef __IRAM_IN_865X
#endif
#ifdef CONFIG_RTL_8198C
#define __IRAM_IN_865X		//__attribute__ ((section(".iram-rtkwlan")))
#else
#define __IRAM_IN_865X		__attribute__ ((section(".iram-rtkwlan")))
#endif

#ifdef __IRAM_IN_865X_HI
	#undef __IRAM_IN_865X_HI
#endif
#ifdef CONFIG_RTL_8198C
#define __IRAM_IN_865X_HI	//__attribute__ ((section(".iram-tx")))
#else
#define __IRAM_IN_865X_HI	__attribute__ ((section(".iram-tx")))
#endif

//#define USE_TXQUEUE
#ifdef USE_TXQUEUE
	#define TXQUEUE_SIZE	512
#endif

// Support dynamically adjust TXOP in low throughput feature
#define LOW_TP_TXOP

// Support four different AC stream
#define WMM_VIBE_PRI

// Resist interference 
#ifdef CONFIG_RTL_92C_SUPPORT
	#define INTERFERENCE_CONTROL
#endif

#ifdef PCIE_POWER_SAVING
	#undef PCIE_POWER_SAVING
#endif

#ifdef CONFIG_RTL_8196C
	#undef CONFIG_RTL_8196C
#endif
#ifdef CONFIG_RTL8196C_REVISION_B
	#undef CONFIG_RTL8196C_REVISION_B
#endif

#ifdef RTL_MANUAL_EDCA
	#undef RTL_MANUAL_EDCA
#endif

#if defined(CONFIG_RTL_8812_SUPPORT) && defined(CONFIG_WLAN_HAL_8192EE)
#define RX_LOOP_LIMIT
#endif

#define RTLWIFINIC_GPIO_CONTROL

#ifdef CONFIG_RTL8686
#define _FULLY_WIFI_IGMP_SNOOPING_SUPPORT_
#endif

// use seq_file to display big file to avoid buffer overrun when using create_proc_entry
#define CONFIG_RTL_PROC_NEW

#endif // CONFIG_RTL8672


//-------------------------------------------------------------
// Define flag of rtl8192d features
//-------------------------------------------------------------
#ifdef CONFIG_RTL_92D_SUPPORT
#define SW_LCK_92D
#define DPK_92D

#define RX_GAIN_TRACK_92D
//#define CONFIG_RTL_NOISE_CONTROL
#ifdef CONFIG_RTL_92D_DMDP
//#define NON_INTR_ANTDIV
#endif

//#ifdef CONFIG_RTL_92D_INT_PA
#define RTL8192D_INT_PA
//#endif

#ifdef RTL8192D_INT_PA
//Use Gain Table with suffix '_new'  for purpose
//1. refine the large gap between power index 39 &40
//#define RTL8192D_INT_PA_GAIN_TABLE_NEW //for both Non-USB & USB Power

//Use Gain Table with suffix '_new1' for purpose
//1. refine the large gap between power index 39 &40
//2. increase tx power 
//#define RTL8192D_INT_PA_GAIN_TABLE_NEW1 //for USB Power only
#endif

#endif


//-------------------------------------------------------------
// Define flag of RTL8188E features
//-------------------------------------------------------------
#ifdef CONFIG_RTL_88E_SUPPORT
/* RTL8188E test chip support*/
#define SUPPORT_RTL8188E_TC
#ifndef CALIBRATE_BY_ODM

//for 8188E IQK
#define IQK_BB_REG_NUM		9		

//for 88e tx power tracking
#define	index_mapping_NUM_88E	15
#define AVG_THERMAL_NUM_88E		4
#define IQK_Matrix_Settings_NUM	1+24+21
#define	AVG_THERMAL_NUM 8
#define	HP_THERMAL_NUM 8

#define	RF_T_METER_88E			0x42
//===
#endif

#endif

#if 0
//-------------------------------------------------------------
// TLD define flag
//-------------------------------------------------------------
#ifdef CONFIG_RTL8196B_TLD

#ifdef GBWC
	#undef GBWC
#endif

#ifdef SUPPORT_SNMP_MIB
	#undef SUPPORT_SNMP_MIB
#endif

#ifdef DRVMAC_LB
	#undef DRVMAC_LB
#endif

#ifdef HIGH_POWER_EXT_PA
	#undef HIGH_POWER_EXT_PA
#endif

#ifdef ADD_TX_POWER_BY_CMD
	#undef ADD_TX_POWER_BY_CMD
#endif

#endif // CONFIG_RTL8196B_TLD
#endif



//-------------------------------------------------------------
// KLD define flag
//-------------------------------------------------------------
#if defined(CONFIG_RTL8196C_KLD)

#ifndef INCLUDE_WPA_PSK
	#define INCLUDE_WPA_PSK
#endif

#ifdef UNIVERSAL_REPEATER
	#undef UNIVERSAL_REPEATER
	#undef SMART_REPEATER_MODE
#endif

#ifndef WIFI_SIMPLE_CONFIG
	#define WIFI_SIMPLE_CONFIG
#endif

#ifdef GBWC
	#undef GBWC
#endif

#ifdef SUPPORT_SNMP_MIB
	#undef SUPPORT_SNMP_MIB
#endif

#ifdef DRVMAC_LB
	#undef DRVMAC_LB
#endif

#ifdef MBSSID
	#undef RTL8192CD_NUM_VWLAN
	#define RTL8192CD_NUM_VWLAN  1
#endif

//#ifdef HIGH_POWER_EXT_PA
//	#undef HIGH_POWER_EXT_PA
//#endif

//#ifdef ADD_TX_POWER_BY_CMD
//	#undef ADD_TX_POWER_BY_CMD
//#endif

#endif // CONFIG_RTL8196C_KLD


//-------------------------------------------------------------
// eCos define flag
//-------------------------------------------------------------
#ifdef __ECOS
	#undef USE_PID_NOTIFY
	//#undef EAP_BY_QUEUE
#ifdef RTK_SYSLOG_SUPPORT
	#define EVENT_LOG
#else
//	#undef EVENT_LOG
	#define EVENT_LOG
#endif
	//#undef SUPPORT_TX_MCAST2UNI	//support m2u hx
	//#undef AUTO_TEST_SUPPORT
	//#undef SUPPORT_RX_UNI2MCAST
#if !defined(UNIVERSAL_REPEATER) && !defined(MBSSID)
	#define USE_WEP_DEFAULT_KEY
#endif	
#ifndef RX_TASKLET
	#define	RX_TASKLET
#endif

#ifdef RTL8192CD_NUM_VWLAN
	#undef RTL8192CD_NUM_VWLAN
#ifdef CONFIG_RTL_VAP_SUPPORT
	#define RTL8192CD_NUM_VWLAN	RTLPKG_DEVS_ETH_RLTK_819X_WLAN_MBSSID_NUM
#else
	#define RTL8192CD_NUM_VWLAN 0
#endif
#endif
	
#if defined(CYGSEM_HAL_IMEM_SUPPORT) && !defined(CONFIG_RTL_8198C)
	#define __IRAM_IN_865X		__attribute__ ((section(".iram-rtkwlan")))
#else
	#define __IRAM_IN_865X
#endif
#if defined(CYGSEM_HAL_DMEM_SUPPORT) && !defined(CONFIG_RTL_8198C)
	#define __DRAM_IN_865X		__attribute__ ((section(".dram-rtkwlan")))
#else
	#define __DRAM_IN_865X
#endif
	#define dev_kfree_skb_any(skb)	wlan_dev_kfree_skb_any(skb)
#endif /* __ECOS */


//-------------------------------------------------------------
// Dependence check of define flag
//-------------------------------------------------------------
#if defined(B2B_TEST) && !defined(MP_TEST)
	#error "Define flag error, MP_TEST is not defined!\n"
#endif


#if defined(UNIVERSAL_REPEATER) && !defined(CLIENT_MODE)
	#error "Define flag error, CLIENT_MODE is not defined!\n"
#endif


#if defined(TX_EARLY_MODE) && !defined(SW_TX_QUEUE)
	#error "Define flag error, SW_TX_QUEUE is not defined!\n"
#endif


/*=============================================================*/
/*------ Compiler Portability Macros --------------------------*/
/*=============================================================*/
#ifdef EVENT_LOG
#ifdef RTK_SYSLOG_SUPPORT
	extern int wlanlog_printk(const char *fmt, ...);
#else
	extern int scrlog_printk(const char * fmt, ...);
#endif
#ifdef CONFIG_RTK_MESH
/*
 *	NOTE: dot1180211sInfo.log_enabled content from webpage MIB_LOG_ENABLED (bitmap) (in AP/goahead-2.1.1/LINUX/fmmgmt.c  formSysLog)
 */
#ifndef RTK_SYSLOG_SUPPORT
	#define _LOG_MSG(fmt, args...)	if (1 & GET_MIB(priv)->dot1180211sInfo.log_enabled) scrlog_printk(fmt, ## args)
#else
	#define _LOG_MSG(fmt, args...)	if (1 & GET_MIB(priv)->dot1180211sInfo.log_enabled) wlanlog_printk(fmt, ## args)
#endif
	
	#define LOG_MESH_MSG(fmt, args...)	if (16 & GET_MIB(priv)->dot1180211sInfo.log_enabled) _LOG_MSG("%s: " fmt, priv->mesh_dev->name, ## args)
#else
#ifndef RTK_SYSLOG_SUPPORT
#ifdef __ECOS
	#define _LOG_MSG(fmt, args...)	diag_printf(fmt, ## args)
#else
	#define _LOG_MSG(fmt, args...)	scrlog_printk(fmt, ## args)
#endif
#else
	#define _LOG_MSG(fmt, args...)	wlanlog_printk(fmt, ## args)
#endif	
#endif
#if defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL8196C_EC)
	#define _NOTICE	"NOTICElog_num:13;msg:"
	#define _DROPT	"DROPlog_num:13;msg:"
	#define _SYSACT "SYSACTlog_num:13;msg:"

	#define LOG_MSG_NOTICE(fmt, args...) _LOG_MSG("%s" fmt, _NOTICE, ## args)
	#define LOG_MSG_DROP(fmt, args...) _LOG_MSG("%s" fmt, _DROPT, ## args)
	#define LOG_MSG_SYSACT(fmt, args...) _LOG_MSG("%s" fmt, _SYSACT, ## args)
	#define LOG_MSG(fmt, args...)	{}

	#define LOG_START_MSG() { \
			char tmpbuf[10]; \
			LOG_MSG_NOTICE("Access Point: %s started at channel %d;\n", \
				priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, \
				priv->pmib->dot11RFEntry.dot11channel); \
			if (priv->pmib->dot11StationConfigEntry.autoRate) \
				strcpy(tmpbuf, "best"); \
			else \
				sprintf(tmpbuf, "%d", get_rate_from_bit_value(priv->pmib->dot11StationConfigEntry.fixedTxRate)/2); \
			LOG_MSG_SYSACT("AP 2.4GHz mode Ready. Channel : %d TxRate : %s SSID : %s;\n", \
				priv->pmib->dot11RFEntry.dot11channel, \
				tmpbuf, priv->pmib->dot11StationConfigEntry.dot11DesiredSSID); \
	}

#elif defined(CONFIG_RTL865X_AC) || defined(CONFIG_RTL865X_KLD) || defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196C_KLD)
	#define _NOTICE	"NOTICElog_num:13;msg:"
	#define _DROPT	"DROPlog_num:13;msg:"
	#define _SYSACT "SYSACTlog_num:13;msg:"

	#define LOG_MSG_NOTICE(fmt, args...) _LOG_MSG("%s" fmt, _NOTICE, ## args)
	#define LOG_MSG_DROP(fmt, args...) _LOG_MSG("%s" fmt, _DROPT, ## args)
	#define LOG_MSG_SYSACT(fmt, args...) _LOG_MSG("%s" fmt, _SYSACT, ## args)
	#define LOG_MSG(fmt, args...)	{}

	#define LOG_START_MSG() { \
			char tmpbuf[10]; \
			LOG_MSG_NOTICE("Access Point: %s started at channel %d;\n", \
				priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, \
				priv->pmib->dot11RFEntry.dot11channel); \
			if (priv->pmib->dot11StationConfigEntry.autoRate) \
				strcpy(tmpbuf, "best"); \
			else \
				sprintf(tmpbuf, "%d", get_rate_from_bit_value(priv->pmib->dot11StationConfigEntry.fixedTxRate)/2); \
			LOG_MSG_SYSACT("AP 2.4GHz mode Ready. Channel : %d TxRate : %s SSID : %s;\n", \
				priv->pmib->dot11RFEntry.dot11channel, \
				tmpbuf, priv->pmib->dot11StationConfigEntry.dot11DesiredSSID); \
	}
#elif defined(CONFIG_RTL8196B_TLD)
	#define LOG_MSG_DEL(fmt, args...)	_LOG_MSG(fmt, ## args)
	#define LOG_MSG(fmt, args...)	{}
#else
	#define LOG_MSG(fmt, args...)	_LOG_MSG("%s: "fmt, priv->dev->name, ## args)
#endif
#else
	#if defined(__GNUC__) || defined(GREEN_HILL)
		#define LOG_MSG(fmt, args...)	{}
	#else
		#define LOG_MSG
	#endif
#endif // EVENT_LOG

#ifdef _USE_DRAM_
	#define DRAM_START_ADDR		0x81000000	// start address of internal data ram
#endif

#ifndef __ECOS
#ifdef __GNUC__
#define __WLAN_ATTRIB_PACK__		__attribute__ ((packed))
#define __PACK
#endif
#endif

#ifdef __arm
#define __WLAN_ATTRIB_PACK__
#define __PACK	__packed
#endif

#if defined(GREEN_HILL) || defined(__ECOS)
#define __WLAN_ATTRIB_PACK__
#define __PACK
#endif


/*=============================================================*/
/*-----------_ Driver module flags ----------------------------*/
/*=============================================================*/
#if defined(CONFIG_WIRELESS_LAN_MODULE) || defined(CONFIG_RTL_ULINKER_WLAN_DELAY_INIT)
	#define	MODULE_NAME		"Realtek WirelessLan Driver"
#ifndef NOT_RTK_BSP
	#define	MODULE_VERSION	"v1.00"
#endif

	#define MDL_DEVINIT
	#define MDL_DEVEXIT
	#define MDL_INIT
	#define MDL_EXIT
	#define MDL_DEVINITDATA
#else
#ifdef CONFIG_RTL_8198C
	#define MDL_DEVINIT		//__devinit
	#define MDL_DEVEXIT		//__devexit
	#define MDL_INIT		//__init
	#define MDL_EXIT		//__exit
	#define MDL_DEVINITDATA	//__devinitdata
#else
	#define MDL_DEVINIT		__devinit
	#define MDL_DEVEXIT		__devexit
	#define MDL_INIT		__init
	#define MDL_EXIT		__exit
	#define MDL_DEVINITDATA	__devinitdata
#endif	
#endif


/*=============================================================*/
/*----------- System configuration ----------------------------*/
/*=============================================================*/
#if defined(CONFIG_RTL8196B_GW_8M)
#define NUM_TX_DESC		200
#else
#if defined(CONFIG_RTL_8198) || defined(CONFIG_RTL_819XD)  || defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8196E) || defined(CONFIG_RTL_8198B)  || (defined(CONFIG_LUNA_DUAL_LINUX) || defined(CONFIG_ARCH_LUNA_SLAVE)) || defined(CONFIG_RTL8672)
#if defined(CONFIG_RTL_DUAL_PCIESLOT_BIWLAN_D)
#define NUM_TX_DESC		640
#elif defined(__ECOS) && defined(CYGNUM_RAM_SIZE_0x00800000) 
#define NUM_TX_DESC 	256
#else
#ifdef CONFIG_RTL_8812_SUPPORT
	#ifdef CONFIG_RTL_8812AR_VN_SUPPORT    
		#define NUM_TX_DESC	480
	#else
		#define NUM_TX_DESC	768
	#endif
#elif  defined(CONFIG_RTL_8196E)
	#define NUM_TX_DESC		300//256	
#elif defined(CONFIG_WLAN_HAL_8881A)
	#define NUM_TX_DESC		480
#elif defined(CONFIG_WLAN_HAL_8814AE)
    #define NUM_TX_DESC    768
#else
#define NUM_TX_DESC		512
#endif
#endif
#elif defined(NOT_RTK_BSP)
#if defined(CONFIG_WLAN_HAL_8814AE)
#define NUM_TX_DESC		2176 //512		// kmalloc max size issue
#else
#define NUM_TX_DESC		512
#endif
#else
#define NUM_TX_DESC		256		// kmalloc max size issue
#endif
#endif

//#define NUM_TX_DESC_HQ    64
#if defined (CONFIG_SLOT_0_TX_BEAMFORMING) || defined (CONFIG_SLOT_1_TX_BEAMFORMING)
#define BEAMFORMING_SUPPORT  1
#endif

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8881A) || defined(CONFIG_WLAN_HAL_8814AE)
#define RTK_AC_SUPPORT
#endif

#ifdef CONFIG_RTL_NFJROM_MP
	#define NUM_RX_DESC		64
#elif defined(CONFIG_RTL_8198) || defined(CONFIG_RTL_819XD) || defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8196E) || (defined(CONFIG_LUNA_DUAL_LINUX) || defined(CONFIG_ARCH_LUNA_SLAVE)) || defined(CONFIG_RTL8672)
	#if defined(CONFIG_RTL_DUAL_PCIESLOT_BIWLAN_D)
		#define NUM_RX_DESC		256
	#elif defined(__ECOS) && defined(CYGNUM_RAM_SIZE_0x00800000) 
		#define NUM_RX_DESC 	320
	#else
		#ifdef CONFIG_RTL_8812_SUPPORT
			#ifdef CONFIG_RTL_8812AR_VN_SUPPORT 
			#define NUM_RX_DESC 	256 
			#else
			#define NUM_RX_DESC 	512 
			#endif
		#elif defined(CONFIG_RTL_8196E) 
            #ifdef MULTI_MAC_CLONE
			#define NUM_RX_DESC 	512 	            
            #else
			#define NUM_RX_DESC 	256 	
            #endif
		#elif  defined(CONFIG_WLAN_HAL_8881A)
			#define NUM_RX_DESC		480	//256 
        #elif  defined(CONFIG_WLAN_HAL_8814AE)
            #define NUM_RX_DESC     512 //256 
		#elif defined(CONFIG_RTL_8196D)
			#define NUM_RX_DESC		512
		#else
			#define NUM_RX_DESC		128
		#endif	
	#endif
#elif defined(CONFIG_RTL_92D_SUPPORT)
	#define NUM_RX_DESC		80
#elif defined(__ECOS)
	#define NUM_RX_DESC		64
	#undef NUM_TX_DESC
	#define NUM_TX_DESC		512
#elif defined(NOT_RTK_BSP)
	#ifdef CONFIG_WLAN_HAL_8814AE
		#define NUM_RX_DESC		2048
	#elif defined(CONFIG_RTL_8812_SUPPORT)
		#define NUM_RX_DESC		2048
	#else
		#define NUM_RX_DESC		128
	#endif
#else
	#ifdef RX_BUFFER_GATHER
		#define NUM_RX_DESC		64
	#else
		#define NUM_RX_DESC		32
	#endif
#endif

#define CURRENT_NUM_TX_DESC	priv->pshare->current_num_tx_desc
#if defined (CONFIG_RTL_92D_SUPPORT) && defined(CONFIG_RTL_92D_DMDP)
#define MAX_NUM_TX_DESC_DMDP		256
#endif

#ifdef DELAY_REFILL_RX_BUF
	#define REFILL_THRESHOLD	NUM_RX_DESC
#endif


#ifdef CONFIG_RTL_88E_SUPPORT
#ifdef __ECOS
#define RTL8188E_NUM_STAT 32
#else
#define RTL8188E_NUM_STAT 64
#endif
#endif

/* do not modify this*/
#define RTL8192CD_NUM_STAT         32         //92c / 92d // 88c
#define FW_NUM_STAT               128         // 8812 / 8192E / 8881A / Other new ic  FW_NUM_STAT

#if (defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196C_KLD)) && defined(MBSSID)
#define NUM_CMD_DESC	2
#else
#define NUM_CMD_DESC	16
#endif

#ifdef HW_DETEC_POWER_STATE
#define HW_MACID_SEARCH_NOT_READY   0x7E
#define HW_MACID_SEARCH_FAIL        0x7F
#define HW_MACID_SEARCH_SUPPORT_NUM (HW_MACID_SEARCH_FAIL-1)
#endif

#ifdef CONFIG_RTL_NFJROM_MP
    #define NUM_STAT		8
#elif defined(MULTI_MAC_CLONE) && defined(CONFIG_WLAN_HAL_8192EE)
    #define NUM_STAT        63
#elif defined(CONFIG_RTL_88E_SUPPORT)
    #define NUM_STAT		(RTL8188E_NUM_STAT - 1)
#else
    #define NUM_STAT		31 //127
#endif


#define MAX_GUEST_NUM   NUM_STAT

#define NUM_TXPKT_QUEUE			64
#define NUM_APSD_TXPKT_QUEUE	32
#define NUM_DZ_MGT_QUEUE	16

#ifdef CONFIG_WLAN_HAL
#define PRE_ALLOCATED_HDR		(NUM_TX_DESC*2)
#else
#define PRE_ALLOCATED_HDR		NUM_TX_DESC
#endif

#ifdef DRVMAC_LB
#define PRE_ALLOCATED_MMPDU		32
#define PRE_ALLOCATED_BUFSIZE	(2048/4)		// 600 bytes long should be enough for mgt! Declare as unsigned int
#else
#define PRE_ALLOCATED_MMPDU		64
#define PRE_ALLOCATED_BUFSIZE	((600+128)/4)		// 600 bytes long should be enough for mgt! Declare as unsigned int
#endif

#ifdef RTK_NL80211  //eric-sync ??
#define MAX_BSS_NUM		32
#else
#define MAX_BSS_NUM		64
#endif

#define MAX_NUM_WLANIF		4
#define WLAN_MISC_MAJOR		13

#define MAX_FRAG_COUNT		10

#define NUM_MP_SKB		32

#define SUPPORT_CH_NUM	59

// unit of time out: 10 msec
#define AUTH_TO			RTL_SECONDS_TO_JIFFIES(5)
#define ASSOC_TO		RTL_SECONDS_TO_JIFFIES(5)
#define FRAG_TO			RTL_SECONDS_TO_JIFFIES(20)
#define SS_TO			RTL_MILISECONDS_TO_JIFFIES(50)
#define SS_PSSV_TO		RTL_MILISECONDS_TO_JIFFIES(120)			// passive scan for 120 ms



#define P2P_SEARCH_TIME_V   200
#define P2P_SEARCH_TIME		RTL_MILISECONDS_TO_JIFFIES(P2P_SEARCH_TIME_V)



#ifdef HS2_SUPPORT
#define CU_TO           RTL_MILISECONDS_TO_JIFFIES(210)         // 200ms to calculate bbp channel load
#define CU_Intval       200
#endif

#ifdef CONFIG_RTL_NEW_AUTOCH
#define SS_AUTO_CHNL_TO		RTL_MILISECONDS_TO_JIFFIES(200)
#define SS_AUTO_CHNL_NHM_TO		RTL_MILISECONDS_TO_JIFFIES(100)
#endif

#ifdef CONFIG_RTK_MESH
//GANTOE for automatic site survey 2008/12/10
#define SS_RAND_DEFER		300
#if defined(RTK_MESH_AODV_STANDALONE_TIMER)
#define MESH_AODV_EXPIRE_TO	RTL_MILISECONDS_TO_JIFFIES(100)
#endif
#endif
#ifdef LINUX_2_6_22_
#define EXPIRE_TO		RTL_SECONDS_TO_JIFFIES(1)
#else
#define EXPIRE_TO		RTL_SECONDS_TO_JIFFIES(1)
#endif
#define REAUTH_TO		RTL_SECONDS_TO_JIFFIES(5)
#define REASSOC_TO		RTL_SECONDS_TO_JIFFIES(5)
#define REAUTH_LIMIT	6
#define REASSOC_LIMIT	6

#define DEFAULT_OLBC_EXPIRE		60

#define GBWC_TO			RTL_MILISECONDS_TO_JIFFIES(250)

#ifdef __DRAYTEK_OS__
#define SS_COUNT		2
#else
#define SS_COUNT		3
#endif

#define TUPLE_WINDOW	64

#define RC_TIMER_NUM	64
#define RC_ENTRY_NUM	128
#define AMSDU_TIMER_NUM	64
#define AMPDU_TIMER_NUM	64

#define ROAMING_DECISION_PERIOD_INFRA	5
#define ROAMING_DECISION_PERIOD_ADHOC	10
#define ROAMING_DECISION_PERIOD_ARRAY 	(MAX_NUM(ROAMING_DECISION_PERIOD_ADHOC,ROAMING_DECISION_PERIOD_INFRA)+1)
#define ROAMING_THRESHOLD		1	// roaming will be triggered when rx
									// beacon percentage is less than the value
#define FAST_ROAMING_THRESHOLD	40

/* below is for security.h  */
#define MAXDATALEN		1560
#define MAXQUEUESIZE	8	//WPS2DOTX
#define MAXRSNIELEN		128
#define E_DOT11_2LARGE	-1
#define E_DOT11_QFULL	-2
#define E_DOT11_QEMPTY	-3
#ifdef WIFI_SIMPLE_CONFIG
#define PROBEIELEN		260
#endif

// for SW LED
#define LED_MAX_PACKET_CNT_B	400
#define LED_MAX_PACKET_CNT_AG	1200
#define LED_MAX_SCALE			100
#define LED_NOBLINK_TIME		RTL_SECONDS_TO_JIFFIES(15)/10	// time more than watchdog interval
#define LED_INTERVAL_TIME		RTL_MILISECONDS_TO_JIFFIES(500)	// 500ms
#define LED_ON_TIME				RTL_MILISECONDS_TO_JIFFIES(40)	// 40ms
#define LED_ON					0
#define LED_OFF					1
#define LED_0 					0
#define LED_1 					1
#define LED_2					2

// for counting association number
#define INCREASE		1
#define DECREASE		0

// DFS
#define CH_AVAIL_CHK_TO			RTL_SECONDS_TO_JIFFIES(62)	 // 62 seconds
#define CH_AVAIL_CHK_TO_CE              RTL_SECONDS_TO_JIFFIES(602)      // 602 seconds


/*adjusted for support AMSDU*/
#if defined(RTK_AC_SUPPORT) //&& !defined(CONFIG_RTL_8198B)

#ifdef CONFIG_RTL_AC2G_256QAM
#define AC2G_256QAM 
#endif

#ifdef __ECOS
#define MAX_SKB_BUF     MCLBYTES //2048
#define MAX_RX_BUF_LEN	(MAX_SKB_BUF -sizeof(struct skb_shared_info) - 32)
#define MIN_RX_BUF_LEN  MAX_RX_BUF_LEN
#else
#ifdef RX_BUFFER_GATHER
#ifdef CONFIG_RTL_8812_SUPPORT
#define MAX_RX_BUF_LEN  3000
#define MIN_RX_BUF_LEN  3000
#else
#define MAX_RX_BUF_LEN  2600
#define MIN_RX_BUF_LEN  2600
#endif
#else
#define MAX_RX_BUF_LEN  12000
#define MIN_RX_BUF_LEN  4600
#endif
#endif

#else // ! RTK_AC_SUPPORT

#ifdef RX_BUFFER_GATHER
#ifdef __LINUX_2_6__
#define MAX_SKB_BUF     2280
#elif defined(__ECOS)
#define MAX_SKB_BUF     MCLBYTES //2048
#else
#define MAX_SKB_BUF     2048
#endif

#ifdef __ECOS
#define MAX_RX_BUF_LEN  (MAX_SKB_BUF -sizeof(struct skb_shared_info) - 32)
#else
#define MAX_RX_BUF_LEN  (MAX_SKB_BUF -sizeof(struct skb_shared_info) - 128)
#endif
#define MIN_RX_BUF_LEN MAX_RX_BUF_LEN 
#else //#ifdef RX_BUFFER_GATHER
#if defined(__ECOS) //mark_ecos
#define MAX_SKB_BUF     MCLBYTES //2048
#define MAX_RX_BUF_LEN	(MAX_SKB_BUF -sizeof(struct skb_shared_info) - 32)
#define MIN_RX_BUF_LEN  MAX_RX_BUF_LEN
#else //
#define MAX_RX_BUF_LEN	8400
#define MIN_RX_BUF_LEN	4400
#endif
#endif //#ifdef RX_BUFFER_GATHER
#endif

/* for RTL865x suspend mode */
#define TP_HIGH_WATER_MARK 55 //80 /* unit: Mbps */
#define TP_LOW_WATER_MARK 35 //40 /* unit: Mbps */

#define FW_BOOT_SIZE	400
#define FW_MAIN_SIZE	52000
#define FW_DATA_SIZE	850

#ifdef CONFIG_WLAN_HAL_8881A
#define AGC_TAB_SIZE    2400
#else
#define AGC_TAB_SIZE	1600
#endif
#define PHY_REG_SIZE	2048
//#define MAC_REG_SIZE	1200
#define MAC_REG_SIZE	1420
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
#define PHY_REG_PG_SIZE 2560
#else
#define PHY_REG_PG_SIZE 256
#endif

#ifdef CONFIG_WLAN_HAL_8814AE
// TODO: temporary for 8814AE file size
#define AGC_TAB_SIZE	5120
#define PHY_REG_SIZE	18432
#define MAC_REG_SIZE	2048
#define PHY_REG_PG_SIZE 9216
#endif 

#define PHY_REG_1T2R	256
#define PHY_REG_1T1R	256
#define FW_IMEM_SIZE	40*(1024)
#define FW_EMEM_SIZE	50*(1024)
#define FW_DMEM_SIZE	48

// for PCIe power saving
#define POWER_DOWN_T0	(60*HZ)
#define PKT_PAGE_SZ 	128
#define TX_DESC_SZ 		32


#ifdef SUPPORT_TX_MCAST2UNI
#define MAX_IP_MC_ENTRY		8
#define MAX_FLOODING_MAC_NUM 32
#endif

#ifndef CALIBRATE_BY_ODM
#define IQK_ADDA_REG_NUM	16
#define MAX_TOLERANCE		5
#if defined(HIGH_POWER_EXT_PA) && defined(CONFIG_RTL_92C_SUPPORT)
#define	IQK_DELAY_TIME		20		//ms
#else
#define	IQK_DELAY_TIME		1		//ms
#endif
#define IQK_MAC_REG_NUM		4
#endif


#define SKIP_MIC_NUM	300


// for dynamic mechanism of reserving tx desc
#ifdef RESERVE_TXDESC_FOR_EACH_IF
#define IF_TXDESC_UPPER_LIMIT	70	// percentage
#ifdef USE_TXQUEUE
#define IF_TXQ_UPPER_LIMIT		85	// percentage
#endif
#endif

// for dynamic mechanism of retry count
#define RETRY_TRSHLD_H	3750000
#define RETRY_TRSHLD_L	3125000
#define MP_PSD_SUPPORT 1
//-------------------------------------------------------------
// Define flag for 8M gateway configuration
//-------------------------------------------------------------
#if defined(CONFIG_RTL8196B_GW_8M)

#ifdef MBSSID
	#undef RTL8192CD_NUM_VWLAN
	#define RTL8192CD_NUM_VWLAN  1
#endif

#undef NUM_STAT
#define NUM_STAT		16

#endif // CONFIG_RTL8196B_GW_8M


#if defined(CONFIG_RTL8196C_AP_ROOT) || defined(CONFIG_RTL8198_AP_ROOT)

#ifdef DOT11D
	#undef DOT11D
#endif


#ifdef MBSSID
	#undef RTL8192CD_NUM_VWLAN
	#define RTL8192CD_NUM_VWLAN  1
#endif

#undef NUM_STAT
#define NUM_STAT		16


#endif //defined(CONFIG_RTL8196C_AP_ROOT)
#ifdef CONFIG_RTL_8198_NFBI_RTK_INBAND_AP //mark_nfbi_inband_ap
#ifdef MBSSID
        #undef RTL8192CD_NUM_VWLAN
        #define RTL8192CD_NUM_VWLAN  7
#endif
#endif


#if defined(CONFIG_RTL8196C_CLIENT_ONLY)

#ifdef DOT11D
	#undef DOT11D
#endif

#endif

#ifdef CONCURRENT_MODE
#define NUM_WLAN_IFACE 2
#endif

#ifdef CONFIG_NET_PCI
#ifndef NUM_WLAN_IFACE
#define NUM_WLAN_IFACE 2
#endif
#endif

#if defined(CONFIG_RTL_92D_SUPPORT) && defined(CONFIG_USB_POWER_BUS)
#define USB_POWER_SUPPORT
#endif

#ifdef WIFI_SIMPLE_CONFIG
#define MAX_WSC_IE_LEN		(256+128)
#define MAX_WSC_PROBE_STA	10
#endif

#define MAX_PROBE_REQ_STA	32

#ifdef CONFIG_WIRELESS_LAN_MODULE //BE_MODULE
#undef __DRAM_IN_865X
#undef __IRAM_IN_865X
#undef __MIPS16

#define __DRAM_IN_865X  
#define __IRAM_IN_865X  
#define __MIPS16 
#ifndef RTK_NL80211
#undef BR_SHORTCUT
#endif
#if defined(NOT_RTK_BSP) && defined(BR_SHORTCUT_SUPPORT)
#define BR_SHORTCUT
#endif // NOT_RTK_BSP && BR_SHORTCUT_SUPPORT
#undef CONFIG_RTL865X_ETH_PRIV_SKB
#undef CONFIG_RTL_ETH_PRIV_SKB
#undef CONFIG_RTK_VLAN_SUPPORT
#endif
#if defined(CONFIG_RTL8672) && defined(LINUX_2_6_22_) && defined(WIFI_LIMITED_MEM)
	#undef NUM_STAT
	#define NUM_STAT	16
#endif

//-------------------------------------------------------------
// Option: Use kernel thread to execute Tx Power Tracking function.
//-------------------------------------------------------------
#ifdef CONFIG_RTL_TPT_THREAD
#define TPT_THREAD
#endif

//-------------------------------------------------------------
// OUT SOURCE
//-------------------------------------------------------------
#ifdef CONFIG_RTL_ODM_WLAN_DRIVER
#define USE_OUT_SRC					1
#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT)
#define _OUTSRC_COEXIST
#endif
#endif

#ifdef CONFIG_RTL_88E_SUPPORT
#ifdef USE_OUT_SRC
#define RATEADAPTIVE_BY_ODM			1
#define CALIBRATE_BY_ODM			1
#endif
// Support four different AC stream
#ifndef WMM_VIBE_PRI
#define WMM_VIBE_PRI
#endif
#define WMM_BEBK_PRI
#endif


#ifdef CONFIG_WLAN_HAL

#endif

//-------------------------------------------------------------
// Define flag of RTL8812 features
//-------------------------------------------------------------
#ifdef CONFIG_RTL_8812_SUPPORT

//#undef DETECT_STA_EXISTANCE

#define USE_OUT_SRC					1

//#ifdef CONFIG_RTL_92C_SUPPORT
//#define _OUTSRC_COEXIST
//#endif

//for 11ac logo +++
//#define BEAMFORMING_SUPPORT			1
#ifndef __ECOS
//#define SUPPORT_TX_AMSDU //disable this, because aput only needs rx amsdu. (but testbed needs both rx & tx amsdu)
#endif
//for 11ac logo ---

#define _TRACKING_TABLE_FILE
#define TX_PG_8812

#endif

//-------------------------------------------------------------
// SKB NUM
//-------------------------------------------------------------
#ifdef CONFIG_RTL8190_PRIV_SKB
		#ifdef DELAY_REFILL_RX_BUF
			#if defined(CONFIG_RTL8196B_GW_8M)
				#define MAX_SKB_NUM		100
			#elif defined(CONFIG_RTL8672)
				#if defined(WIFI_LIMITED_MEM)
					#if defined(LINUX_2_6_22_)
						#define MAX_SKB_NUM		96
					#else
						#define MAX_SKB_NUM		160
					#endif
				#else
					#define MAX_SKB_NUM		768
				#endif
			#elif defined(CONFIG_RTL_8196E) 
				#if defined(CONFIG_WLAN_HAL_8192EE)
    		        #ifdef MULTI_MAC_CLONE
					#define MAX_SKB_NUM 	(NUM_RX_DESC + 64)
		            #else
					#define MAX_SKB_NUM 	400 //256 	
		            #endif				
				#else
					#define MAX_SKB_NUM	256
				#endif
			#elif defined(CONFIG_WLAN_HAL_8881A)						
					#define MAX_SKB_NUM	480		
			#elif defined(CONFIG_RTL_8198C)
					#define MAX_SKB_NUM     512
			#elif defined(CONFIG_RTL_8198_GW) || defined(CONFIG_RTL_8198_AP_ROOT) || defined(CONFIG_RTL_819XD)
				#ifdef CONFIG_RTL_8812_SUPPORT					
					#ifdef CONFIG_RTL_8812AR_VN_SUPPORT    		
				    	#define MAX_SKB_NUM	480
					#else
					#define MAX_SKB_NUM	768
					#endif
                #elif defined(CONFIG_WLAN_HAL_8814AE)
                    #define MAX_SKB_NUM	768
				#else
					#define MAX_SKB_NUM	480//256	
				#endif		
			#elif defined(CONFIG_RTL_92D_SUPPORT)
				#ifdef CONFIG_RTL_8198_AP_ROOT
					#define MAX_SKB_NUM		210
				#else
					#define MAX_SKB_NUM		256
				#endif
			#elif defined( __ECOS)
				#define MAX_SKB_NUM		256
			#else
				#ifdef UNIVERSAL_REPEATER
					#define MAX_SKB_NUM		256
				#else
					#define MAX_SKB_NUM		160
				#endif
			#endif			
		#else
			#define MAX_SKB_NUM		580
		#endif


#endif


//-------------------------------------------------------------
// NFJROM CONFIG
//-------------------------------------------------------------
#ifdef CONFIG_RTL_NFJROM_MP
#undef NUM_TX_DESC
#undef NUM_RX_DESC
#undef NUM_STAT
#undef NUM_TXPKT_QUEUE
#undef NUM_APSD_TXPKT_QUEUE
#undef NUM_DZ_MGT_QUEUE
#ifdef __ECOS
/*Ecos's skb buffer is limited to cluster size.using the value in MP the same as in normal driver*/
#else
#undef MAX_RX_BUF_LEN  
#undef MIN_RX_BUF_LEN  
#endif

#define NUM_TX_DESC				64
#define NUM_RX_DESC				64
#define NUM_STAT				1
#define NUM_TXPKT_QUEUE		8
#define NUM_APSD_TXPKT_QUEUE	1
#define NUM_DZ_MGT_QUEUE		1
#ifdef __ECOS
/*Ecos's skb buffer is limited to cluster size. using the value in MP the same as in normal driver*/
#else
#define MAX_RX_BUF_LEN  			3000
#define MIN_RX_BUF_LEN  			3000
#endif
#endif

#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8881A) || defined(CONFIG_WLAN_HAL_8814AE)
#define RTK_5G_SUPPORT
#endif

#ifdef RTK_5G_SUPPORT
#define MAX_CHANNEL_NUM		76
#else
#define MAX_CHANNEL_NUM		MAX_2G_CHANNEL_NUM
#endif

#define NUM_TX_DESC_HQ 	(NUM_TX_DESC>>3)


//-------------------------------------------------------------
//  802.11h TPC
//-------------------------------------------------------------
#ifdef RTK_5G_SUPPORT
#define DOT11H
#endif

//-------------------------------------------------------------
// Support 802.11d
//-------------------------------------------------------------
#if defined(RTK_5G_SUPPORT) || defined(CONFIG_RTL_80211D_SUPPORT)
#define DOT11D
#endif

//#define CHK_RX_ISR_TAKES_TOO_LONG

//This is for LUNA SDK - Apollo to config 8812 in slave CPU and shift mem 33M
#if defined(CONFIG_ARCH_LUNA_SLAVE) && !defined(CONFIG_WLAN_HAL)
#define CONFIG_LUNA_SLAVE_PHYMEM_OFFSET CONFIG_RTL8686_DSP_MEM_BASE
#else
#define CONFIG_LUNA_SLAVE_PHYMEM_OFFSET 0x0
#endif

#if !defined(CONFIG_LUNA_DUAL_LINUX) && defined(CONFIG_ARCH_LUNA_SLAVE)
#define CONFIG_LUNA_DUAL_LINUX
#endif

#ifdef CONFIG_USB_HCI
#define USB_INTERFERENCE_ISSUE // this should be checked in all usb interface
//#define USB_LOCK_ENABLE
//#define CONFIG_USB_VENDOR_REQ_MUTEX
#define CONFIG_VENDOR_REQ_RETRY
//#define CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
#define CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
#define SUPPORTED_BLOCK_IO
#define CONFIG_USE_VMALLOC

// USB TX
#define CONFIG_USB_TX_AGGREGATION
#define CONFIG_TCP_ACK_TX_AGGREGATION
#define CONFIG_TCP_ACK_MERGE
#define CONFIG_NETDEV_MULTI_TX_QUEUE
//#define CONFIG_TX_RECYCLE_EARLY

// USB RX
//#define CONFIG_USE_USB_BUFFER_ALLOC_RX
#define CONFIG_PREALLOC_RECV_SKB
#define CONFIG_USB_RX_AGGREGATION
#define DBG_CONFIG_ERROR_DETECT
//#define CONFIG_USB_INTERRUPT_IN_PIPE		// Has bug on 92C
#ifdef CONFIG_RTL_88E_SUPPORT
//#define CONFIG_SUPPORT_USB_INT
#define CONFIG_INTERRUPT_BASED_TXBCN // Tx Beacon when driver BCN_OK ,BCN_ERR interrupt occurs
#endif

#undef TX_SC_ENTRY_NUM
#define TX_SC_ENTRY_NUM		3

#undef RX_SC_ENTRY_NUM
#define RX_SC_ENTRY_NUM		3

#undef SW_TX_QUEUE
#define RTL8190_DIRECT_RX
#undef RTL8190_ISR_RX
#define RX_TASKLET
#undef RX_BUFFER_GATHER	// this is a tip. You must define/undef it in above to make it available
#endif // CONFIG_USB_HCI

#ifdef CONFIG_SDIO_HCI
#undef RTL_MANUAL_EDCA
#ifdef __KERNEL__
#define CONFIG_USE_VMALLOC
#endif
#define DONT_COUNT_PROBE_PACKET
#ifdef CONFIG_WLAN_HAL_8192EE
#define CONFIG_1RCCA_RF_POWER_SAVING
#endif

// SDIO TX
#define CONFIG_SDIO_TX_AGGREGATION
//#define CONFIG_TCP_ACK_TX_AGGREGATION
//#define CONFIG_TCP_ACK_MERGE
//#define CONFIG_SDIO_TX_MULTI_QUEUE
#define CONFIG_SDIO_TX_INTERRUPT
#define CONFIG_SDIO_TX_IN_INTERRUPT
#define CONFIG_SDIO_RESERVE_MASSIVE_PUBLIC_PAGE
#define CONFIG_NETDEV_MULTI_TX_QUEUE
#define CONFIG_TX_RECYCLE_EARLY
#ifdef __ECOS
#define CONFIG_SDIO_TX_FILTER_BY_PRI
#endif

// SDIO RX
//#define SHARE_RECV_BUF_MEM 1   //this compile flag is decided use shared 64K recv mem for 8 RX_recv buffers
#if !defined(SHARE_RECV_BUF_MEM)
#define CONFIG_SDIO_RECV_SKB_PREALLOC
#endif
#define RTL8190_DIRECT_RX
#undef RTL8190_ISR_RX
#define RX_TASKLET
#undef RX_BUFFER_GATHER	// this is a tip. You must define/undef it in above to make it available

#undef TX_SC_ENTRY_NUM
#define TX_SC_ENTRY_NUM		3

#undef RX_SC_ENTRY_NUM
#define RX_SC_ENTRY_NUM		3
//#define PLATFORM_ARM_BALONG 1
#if defined(PLATFORM_ARM_BALONG)
//#define REVO_MIFI  1
#define CONFIG_92ES_1T1R 1
#endif
#define SDIO_AP_PS   1     
#define SOFTAP_PS_DURATION 1
#if defined(SDIO_AP_PS)
#define CONFIG_POWER_SUSPEND  1
#define MBSSID_OFFLOAD 1
#endif
// Validate SDIO flag combination
#if defined(CONFIG_SDIO_TX_MULTI_QUEUE) && defined(CONFIG_SDIO_TX_INTERRUPT)
#error "CONFIG_SDIO_TX_MULTI_QUEUE NOT support TX INT mode"
#endif
#if defined(CONFIG_SDIO_TX_IN_INTERRUPT) && !defined(CONFIG_SDIO_TX_INTERRUPT)
#error "CONFIG_SDIO_TX_IN_INTERRUPT must under TX INT mode"
#endif
#endif // CONFIG_SDIO_HCI

// select_queue (of net_device) is available above Linux 2.6.27
#if defined(CONFIG_NETDEV_MULTI_TX_QUEUE) && !defined(LINUX_2_6_27_)
#undef CONFIG_NETDEV_MULTI_TX_QUEUE
#endif

#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
#undef __DRAM_IN_865X
#undef __IRAM_IN_865X
#undef __MIPS16

#define __DRAM_IN_865X  
#define __IRAM_IN_865X  
#define __MIPS16 
#undef BR_SHORTCUT
#undef CONFIG_RTL865X_ETH_PRIV_SKB
#undef CONFIG_RTL_ETH_PRIV_SKB
#undef CONFIG_RTK_VLAN_SUPPORT
#endif




//#ifdef BR_SHORTCUT
#define MAX_REPEATER_SC_NUM 2
#define MAX_BRSC_NUM 8
//#endif

#ifdef RTK_NL80211

#ifdef CPTCFG_CFG80211_MODULE
#define RTK_NL80211_DMA //eric-sync ?? backfire comapt
#endif

#ifndef RTK_NL80211_DMA
#undef NUM_RX_DESC
#undef NUM_TX_DESC
#undef RTL8192CD_NUM_VWLAN
 
#define NUM_RX_DESC    128
#define NUM_TX_DESC    128
#define RTL8192CD_NUM_VWLAN  4
#else
#define RTL8192CD_NUM_VWLAN  (8+1) //eric-vap, add one more for open-wrt scan iface
#endif

#endif //RTK_NL80211

#endif // _8192CD_CFG_H_

