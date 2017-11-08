/*------------------------------------------------------------------------
 *
 * Copyright ST-Ericsson, 2009 – All rights reserved.
 *
 * This information, source code and any compilation or derivative thereof
 * are the proprietary information of ST-Ericsson and/or its licensors and
 * are confidential in nature. Under no circumstances is this software to
 * be exposed to or placed under an Open Source License of any type without
 * the expressed written permission of ST-Ericsson.
 *
 * Although care has been taken to ensure the accuracy of the information
 * and the software, ST-Ericsson assumes no responsibility therefore.THE
 * INFORMATION AND SOFTWARE ARE PROVIDED "AS IS" AND "AS AVAILABLE".
 * ST-Ericsson MAKES NO REPRESENTATIONS OR WARRANTIES OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO WARRANTIES OR
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT OF
 * INTELLECTUAL PROPERTY RIGHTS, WITH RESPECT TO THE INFORMATION AND
 * SOFTWARE PROVIDED
 *--------------------------------------------------------------------------------*/
/**
 * \addtogroup Upper_MAC_Interface
 * \brief
 *
 */
/**
 * \file UMI_Api.h
 * - <b>PROJECT</b>		: WLAN HOST UMAC
 * - <b>FILE</b>		: UMI_Api.h
 * - <b>REVISION</b>	: 0.1
 * \brief
 * This is the header file for UMI API module. This file implements the interface as described in
 * WLAN_Host_UMAC_API.doc
 * \ingroup Upper_MAC_Interface
 * \date 19/12/08 11:37
 * \author Abhijit Uplenchwar [abhijit.uplenchwar@stericsson.com]
 * \note
 * Last person to change file : Abhijit Uplenchwar
 */

#ifndef _UMI_API_H
#define _UMI_API_H

#include "UMI_OsIf.h"

#define UMI_TX_POWER_LEVEL_CONTROLED_BY_DEVICE  0x00

#define ETH_HEADER_SIZE             14
#define ETH_MAX_DATA_SIZE           1500
#define ETH_MAX_PACKET_SIZE         ETH_HEADER_SIZE + ETH_MAX_DATA_SIZE
#define ETH_MIN_PACKET_SIZE         60

#define SCAN_INFO_BEACON            1
#define SCAN_INFO_PROBE_RESP        0

#define LMAC_HI_MSG_SIZE            24
#define MAX_802_11_HDR_SIZE         30
#define SNAP_HDR_SIZE               8
#define ENCRYPTION_HDR_SIZE         8
#define UMAC_INTERNAL_HDR_SIZE      36

/* If OS does not allow extra memory before 802.3 frame */
#define EXTRA_MEMORY_SIZE           (UMAC_INTERNAL_HDR_SIZE + LMAC_HI_MSG_SIZE \
                                     + MAX_802_11_HDR_SIZE + SNAP_HDR_SIZE \
                                     + ENCRYPTION_HDR_SIZE - ETH_HEADER_SIZE)

#define UMI_MAC_ADDRESS_SIZE                6
#define UMI_DOT11_COUNTRY_STRING_SIZE       3
#define UMI_MAX_SSID_SIZE                   32
#define UMI_MAX_ETHERNET_FRAME_SIZE         1500

/* Encryption Status */
#define UMI_ENC_STATUS_NO_ENCRYPTION      0
#define UMI_ENC_STATUS_WEP_SUPPORTED      (1<<0)
#define UMI_ENC_STATUS_TKIP_SUPPORTED     (1<<1)
#define UMI_ENC_STATUS_AES_SUPPORTED      (1<<2)
#define UMI_ENC_STATUS_MASK_ENC_TYPE      0x7  //three bits
#define UMI_ENC_STATUS_MASK_KEY_AVAIL     ((uint32) (1<<31))

/* For Add Key */
#define UMI_ADDKEY_KEYSIZE_WEP_40                 5
#define UMI_ADDKEY_KEYSIZE_WEP_104                13
#define UMI_ADDKEY_KEYSIZE_TKIP                   32
#define UMI_ADDKEY_KEYSIZE_AES                    16
#define UMI_ADDKEY_KEYIDX_TX_KEY                  ((uint32)(1<<31))  // 0x80000000
#define UMI_ADDKEY_KEYIDX_PAIRWISEKEY             (1<<30)            // 0x40000000
#define UMI_ADDKEY_KEYIDX_KEYRSC                  (1<<29)            // 0x20000000
#define UMI_ADDKEY_KEYIDX_FIRST_TXKEY_THEN_RXKEY  (1<<28)            // 0x10000000
#define UMI_ADDKEY_KEYIDX_KEYID_MASK              0x000000FF

/* For PsMode */
#define UMI_PS_MODE_DISABLED                      0
#define UMI_PS_MODE_ENABLED                       1

/* Priority Mask */
#define UMI_PRIORITY_DEFAULT                      0x00
#define UMI_PRIORITY_0                            0x00
#define UMI_PRIORITY_1                            0x01
#define UMI_PRIORITY_2                            0x02
#define UMI_PRIORITY_3                            0x03
#define UMI_PRIORITY_4                            0x04
#define UMI_PRIORITY_5                            0x05
#define UMI_PRIORITY_6                            0x06
#define UMI_PRIORITY_7                            0x07

#define UMI_MEM_BLK_DWORDS                        (1024/4)

typedef void*	UL_HANDLE ;	  /* Standardized Name */

typedef void*	LL_HANDLE ;   /* Standardized Name */

//typedef void*	UMAC_HANDLE ; /* Standardized Name */

typedef void* DRIVER_HANDLE ; /* Standardized Name */

typedef void*  UMI_HANDLE; /* Standardized Name */

typedef enum UMI_STATUS_CODE_E
{
    UMI_STATUS_SUCCESS             ,
    UMI_STATUS_FAILURE             ,
    UMI_STATUS_PENDING             ,
    UMI_STATUS_BAD_PARAM           ,
    UMI_STATUS_OUT_OF_RESOURCES    ,
    UMI_STATUS_TX_LIFETIME_EXCEEDED,
    UMI_STATUS_RETRY_EXCEEDED      ,
    UMI_STATUS_LINK_LOST           ,
    UMI_STATUS_REQ_REJECTED        ,
    UMI_STATUS_AUTH_FAILED         ,
    UMI_STATUS_NO_MORE_DATA        ,
    UMI_STATUS_COUNTRY_INFO_MISMATCH,
    UMI_STATUS_COUNTRY_NOT_FOUND_IN_TABLE,
    UMI_STATUS_MISMATCH_ERROR      ,
    UMI_STATUS_CONF_ERROR          ,
    UMI_STATUS_UNKNOWN_OID_ERROR   ,
    UMI_STATUS_NOT_SUPPORTED       ,
    UMI_STATUS_UNSPECIFIED_ERROR   ,
    UMI_STATUS_MAX
} UMI_STATUS_CODE ;

typedef enum UMI_EVENTS_E
{
    UMI_EVT_IDLE              ,
    UMI_EVT_START_COMPLETED   ,
    UMI_EVT_STOP_COMPLETED    ,
    UMI_EVT_CONNECTING        ,
    UMI_EVT_CONNECTED         ,
    UMI_EVT_CONNECT_FAILED    ,
    UMI_EVT_DISCONNECTED      ,
    UMI_EVT_SCAN_COMPLETED    ,
    UMI_EVT_GENERAL_FAILURE   ,
    UMI_EVT_TX_RATE_CHANGED   ,
    UMI_EVT_RECONNECTED       ,
    UMI_EVT_MICFAILURE        ,
    UMI_EVT_RADAR             ,
    /* Add more events here */
    UMI_EVT_MAX
} UMI_EVENT ;


typedef struct UMI_EVT_DATA_MIC_FAILURE_S
{
    uint8     Rsc[6]          ;
    uint8     IsPairwiseKey   ;
    uint8     KeyIndex        ;
    uint32    MicFailureCount ;
    uint8     PeerAddress[6]  ;
    uint16    Reserved        ;
}UMI_EVT_DATA_MIC_FAILURE  ;


typedef union UMI_EVENT_DATA_U
{

    uint32                   Rate            ;/*UMI_EVT_TX_RATE_CHANGED*/
    UMI_EVT_DATA_MIC_FAILURE MicFailureData  ;/*UMI_EVT_MICFAILURE*/

}UMI_EVENT_DATA ;



typedef enum UMI_DEVICE_OID_E
{
    UMI_DEVICE_OID_802_11_STATION_ID               = 0x0000, /* gets the MAC address set operation not supported */
    UMI_DEVICE_OID_802_11_COUNTRY_STRING           = 0x0001, /* Gets the country string                          */
    UMI_DEVICE_OID_802_11_BSSID_LIST_SCAN          = 0x0002, /* BSS_LIST_SCAN - Scan Request, get operation is not
                                                                supported
                                                             */
    UMI_DEVICE_OID_802_11_BSSID_LIST               = 0x0003, /* BSS_CACHE_QUERY - Gets the current stored
                                                                scan list, There is no set operation  corresponding to this
                                                             */
    UMI_DEVICE_OID_802_11_BSSID                    = 0x0004, /* BSS - Set operation specifies the AP/IBSS to join/start
                                                                Get operation returns the current BSS info. the device has
                                                                joined
                                                             */
    /* Windows defined OID's */
    UMI_DEVICE_OID_802_11_ADD_KEY                  = 0x0005,
    UMI_DEVICE_OID_802_11_AUTHENTICATION_MODE      = 0x0006,
    UMI_DEVICE_OID_802_11_ASSOCIATION_INFORMATION  = 0x0007,
    UMI_DEVICE_OID_802_11_REMOVE_KEY               = 0x0008,
    UMI_DEVICE_OID_802_11_DISASSOCIATE             = 0x0009,
    UMI_DEVICE_OID_802_11_RSSI                     = 0x000A,
    UMI_DEVICE_OID_802_11_RSSI_TRIGGER             = 0x000B,
    UMI_DEVICE_OID_802_11_MEDIA_STREAM_MODE        = 0x000C,
    UMI_DEVICE_OID_802_11_TX_ANTENNA_SELECTED      = 0x000D,
    UMI_DEVICE_OID_802_11_RX_ANTENNA_SELECTED      = 0x000E,
    UMI_DEVICE_OID_802_11_SUPPORTED_DATA_RATES     = 0x000F,
    UMI_DEVICE_OID_802_11_TX_POWER_LEVEL           = 0x0010,
    UMI_DEVICE_OID_802_11_NETWORK_TYPE_IN_USE      = 0x0011,
    UMI_DEVICE_OID_802_11_INFRASTRUCTURE_MODE      = 0x0012,
    UMI_DEVICE_OID_802_11_SSID                     = 0x0013,
    UMI_DEVICE_OID_802_11_ENCRYPTION_STATUS        = 0x0014,

    UMI_DEVICE_OID_802_11_PRIVACY_FILTER           = 0x0020,
    UMI_DEVICE_OID_802_11_STATISTICS               = 0x0030,
    UMI_DEVICE_OID_802_11_POWER_MODE               = 0x0031,
    /* Windows defined OID's */
    UMI_DEVICE_OID_802_11_BLOCK_ACK_POLICY         = 0x0032,
    UMI_DEVICE_OID_802_11_UPDATE_EPTA_CONFIG_DATA  = 0x0033,
    UMI_DEVICE_OID_802_11_SET_AUTO_CALIBRATION_MODE= 0x0034,
    UMI_DEVICE_OID_802_11_CONFIGURE_IBSS           = 0x0035,
    UMI_DEVICE_OID_802_11_SET_UAPSD                = 0x0036,


/*  0: Enable BG scan;  1: Disable BG scan*/
    UMI_DEVICE_OID_802_11_DISABLE_BG_SCAN          = 0x0040,
    UMI_DEVICE_OID_802_11_BLACK_LIST_ADDR          = 0x0041,
    UMI_DEVICE_OID_802_11_OPERATIONAL_POWER_MODE   = 0x0042,
    UMI_DEVICE_OID_802_11_SET_DATA_TYPE_FILTER     = 0x0043


} UMI_DEVICE_OID ;

typedef enum UMI_REGULATORY_CLASS_E
{
    UMI_REG_CLASS_CEPT_1_3                  ,
    UMI_REG_CLASS_FCC_1_4                   ,
    UMI_REG_CLASS_MPHPT_1_6                 ,
    UMI_REG_CLASS_MPHPT_7_11                ,
    UMI_REG_CLASS_MPHPT_12_15               ,
    UMI_REG_CLASS_MPHPT_16_20               ,
    UMI_REG_CLASS_MAX_VALUE
} UMI_REGULATORY_CLASS ;

typedef enum UMI_INFRASTRUCTURE_MODE_E
{
    UMI_802_IBSS                           ,
    UMI_802_INFRASTRUCTURE                 ,
    UMI_802_AUTO_UNKNOWN                   ,
    UMI_802_INFRASTRUCTURE_MODE_MAX_VALUE
} UMI_INFRASTRUCTURE_MODE ;


/* Authentication modes */
typedef enum UMI_AUTHENTICATION_MODE_E
{
    UMI_AUTH_MODE_OPEN      ,
    UMI_AUTH_MODE_SHARED    ,
    UMI_AUTH_MODE_WPA       ,
    UMI_AUTH_MODE_WPA_PSK   ,
    UMI_AUTH_MODE_WPA_NONE  ,
    UMI_AUTH_MODE_WPA_2     ,
    UMI_AUTH_MODE_WPA_2_PSK
} UMI_AUTHENTICATION_MODE ;


typedef struct UMI_DOT11_COUNTRY_STRING_S  // 4 byte aligned
{
    uint8 dot11CountryString[ UMI_DOT11_COUNTRY_STRING_SIZE ];
} UMI_DOT11_COUNTRY_STRING ;

typedef struct UMI_802_11_SSID_S  // 4 byte aligned
{
    uint32    ssidLength                ;
    uint8     ssid[ UMI_MAX_SSID_SIZE ] ;
} UMI_802_11_SSID ;


typedef struct UMI_BSS_LIST_SCAN_S  // 4 byte aligned
{
    uint32    flags       ; /* Bit    0: auto band  Bit 1: 2.4G band
                               Bit    2: 4.9G band  Bit 3: 5G band
                               Bit    4 - 31 : Reserved
                               Value  0: stop scan
                            */
    uint32    powerLevel  ; /* 0 – the transmit power level for active scan
                               is determined by the device.Otherwise,
                               the transmit power in dBm.
                            */
    uint16    ssidLength  ;
    uint8     ssid[2]     ; /* SSID may follow in the following locations,
                               if its bigger than 2 bytes
                            */
} UMI_BSS_LIST_SCAN ;

typedef struct UMI_OID_802_11_ASSOCIATION_INFORMATION_S  // 4 byte aligned
{
    uint16    capabilitiesReq                           ;
    uint16    listenInterval                            ;
    uint8     currentApAddress[UMI_MAC_ADDRESS_SIZE ]   ;
    uint16    capabilitiesResp                          ;
    uint16    statusCode                                ; /* Status code return in assoc/reassoc response */
    uint16    associationId                             ;
    uint16    variableIELenReq                          ;
    uint16    variableIELenRsp                          ;
} UMI_OID_802_11_ASSOCIATION_INFORMATION ;

/***********************************************************************
 * Add-Key Request                                                     *
 ***********************************************************************/

typedef union UMI_PRIVACY_KEY_DATA_U
{
    struct
    {
        uint8     peerAddress[6];
        uint8     reserved;
        uint8     keyLength;
        uint8     keyData[16];
    }WepPairwiseKey;

    struct
    {
        uint8     keyId;
        uint8     keyLength;
        uint8     reserved[2];
        uint8     keyData[16];
    }WepGroupKey;

    struct
    {
        uint8     peerAddress[6];
        uint8     reserved[2];
        uint8     tkipKeyData[16];
        uint8     rxMicKey[8];
        uint8     txMicKey[8];
    }TkipPairwiseKey;

    struct
    {
        uint8     tkipKeyData[16];
        uint8     rxMicKey[8];
        uint8     keyId;
        uint8     reserved[3];
        uint8     rxSequenceCounter[8];
    }TkipGroupKey;

    struct
    {
        uint8     peerAddress[6];
        uint8     reserved[2];
        uint8     aesKeyData[16];
    }AesPairwiseKey;

    struct
    {
        uint8     aesKeyData[16];
        uint8     keyId;
        uint8     reserved[3];
        uint8     rxSequenceCounter[8];
    }AesGroupKey;

    struct
    {
        uint8     peerAddress[6];
        uint8     keyId;
        uint8     reserved;
        uint8     wapiKeyData[16];
        uint8     micKeyData[16];
    }WapiPairwiseKey;

    struct
    {
        uint8     wapiKeyData[16];
        uint8     micKeyData[16];
        uint8     keyId;
        uint8     reserved[3];
    }WapiGroupKey;

} UMI_PRIVACY_KEY_DATA;

typedef struct UMI_OID_802_11_KEY_S  // 4 byte aligned
{
    uint8                 keyType       ;
    uint8                 entryIndex    ;
    uint16                reserved      ;
    UMI_PRIVACY_KEY_DATA  Key           ;

} UMI_OID_802_11_KEY ;

typedef struct UMI_OID_802_11_REMOVE_KEY_S
{
    uint8   EntryIndex  ;
    uint8   Reserved[3] ;
}UMI_OID_802_11_REMOVE_KEY  ;

typedef struct UMI_OID_802_11_STATISTICS_S  // 4 byte aligned
{
    uint32    countPlcpErrors             ;
    uint32    countFcsErrors              ;
    uint32    countTxPackets              ;
    uint32    countRxPackets              ;
    uint32    countRxPacketErrors         ;
    uint32    countRxDecryptionFailures   ;
    uint32    countRxMicFailures          ;
    uint32    countRxNoKeyFailures        ;
    uint32    countTxMulticastFrames      ;
    uint32    countTxFramesSuccess        ;
    uint32    countTxFrameFailures        ;
    uint32    countTxFramesRetried        ;
    uint32    countTxFramesMultiRetried   ;
    uint32    countRxFrameDuplicates      ;
    uint32    countRtsSuccess             ;
    uint32    countRtsFailures            ;
    uint32    countAckFailures            ;
    uint32    countRxMulticastFrames      ;
    uint32    countRxFramesSuccess        ;
} UMI_OID_802_11_STATISTICS ;

typedef struct UMI_BSS_CACHE_INFO_IND_S  // 4 byte aligned
{
    uint8     cacheLine                     ;
    uint8     flags                         ;  /* b0 is set => beacon, clear => probe resp */
    uint8     bssId[ UMI_MAC_ADDRESS_SIZE ] ;
    sint8     rssi                          ;
    uint8     rcpi                          ;
    uint16    channelNumber                 ;
    uint32    freqKhz                       ;
/* From Beacon Body - 802.11 frame endianness (little endian) */
    uint64    timeStamp                     ;
    uint16    beaconInterval                ;
    uint16    capability                    ;
    uint16    ieLength                      ;   /* Note: Cpu endianness -(not from 802.11 frame) */
    uint8     ieElements[ 2 ]               ; /* IE elements may follow, if its larger than 2 bytes */
} UMI_BSS_CACHE_INFO_IND ;

typedef struct UMI_BEACON_CAPABILITIY_S
{
    uint16  ess                : 1 ; /* Bit 00 */
    uint16  ibss               : 1 ; /* Bit 01 */
    uint16  cfPollable         : 1 ; /* Bit 02 */
    uint16  cfPollRequest      : 1 ; /* Bit 03 */
    uint16  privacy            : 1 ; /* Bit 04 */
    uint16  shortPreamble      : 1 ; /* Bit 05 */
    uint16  pbcc               : 1 ; /* Bit 06 */
    uint16  channelAgility     : 1 ; /* Bit 07 */
    uint16  spectrumMgmt       : 1 ; /* Bit 08 */
    uint16  qos                : 1 ; /* Bit 09 */
    uint16  shortSlotTime      : 1 ; /* Bit 10 */
    uint16  apsd               : 1 ; /* Bit 11 */
    uint16  reserved           : 1 ; /* Bit 12 */
    uint16  dsssOFDM           : 1 ; /* Bit 13 */
    uint16  delayedBlockAck    : 1 ; /* Bit 14 */
    uint16  immdediateBlockAck : 1 ; /* Bit 15 */
} UMI_BEACON_CAPABILITY ;

/* List of Beacon and Probe Response */
typedef struct  UMI_BSS_CACHE_INFO_IND_LIST_S
{
    uint32                  numberOfItems       ;
    UMI_BSS_CACHE_INFO_IND  bssCacheInfoInd[1]  ;
} UMI_BSS_CACHE_INFO_IND_LIST ;

typedef struct UMI_BLOCK_ACK_POLICY_S
{
    /*Bits 7:0 Correspond to TIDs 7:0 respectively*/
    uint8              blockAckTxTidPolicy;
    uint8              reserved1;
    /*Bits 7:0 Correspond to TIDs 7:0 respectively*/
    uint8              blockAckRxTidPolicy;
    uint8              reserved2;
}UMI_BLOCK_ACK_POLICY;

typedef struct UMI_UPDATE_EPTA_CONFIG_DATA_S
{
    uint32             ConfigurationParameters ;
    uint32             OccupancyLimits         ;
    uint32             PriorityParameters      ;
    uint32             PeriodParameters        ;
    uint32             LinkExpiryPeriod        ;
}UMI_UPDATE_EPTA_CONFIG_DATA;

typedef struct  UMI_TX_DATA_S
{
    uint8*  pExtraBuffer      ;
    uint8*  pEthHeader        ;
    uint32  ethPayloadLen     ;
    uint8*  pEthPayloadStart  ;
    void*   pDriverInfo       ;
} UMI_TX_DATA ;

typedef struct  UMI_GET_TX_DATA_S
{
    uint32  bufferLength      ;
    uint8*	pTxDesc     		  ;
    uint8*	pDot11Frame    	  ;
    void*   pDriverInfo       ;
} UMI_GET_TX_DATA ;

typedef struct  UMI_GET_PARAM_STATUS_S
{
    uint16				    oid		  ; /* The OID number that associated with
                                                               the parameter. */
    UMI_STATUS_CODE	  status	; /* Status code for the “get” operation. */
    uint16		        length	; /* Number of bytes in the data. */
    void*             pValue  ; /* The parameter data. */
} UMI_GET_PARAM_STATUS ;

typedef struct UMI_HI_DPD_S  // 4 byte aligned
{
    uint16  length        ;     /* length in bytes of the DPD struct */
    uint16  version       ;     /* version of the DPD record */
    uint8   macAddress[6] ;     /* Mac addr of the device */
    uint16  flags         ;     /* record options */
    uint32  sddData[1]    ;     /* 1st 4 bytes of the Static & Dynamic Data */
    /* rest of the Static & Dynamic Data */
} UMI_HI_DPD ;


typedef struct UMI_CONFIG_REQ_S  // 4 byte aligned
{
    uint32          dot11MaxTransmitMsduLifeTime ;
    uint32          dot11MaxReceiveLifeTime      ;
    uint32          dot11RtsThreshold            ;
    UMI_HI_DPD      dpdData                      ;
} UMI_CONFIG_REQ ;

typedef struct UMI_MEM_READ_REQ_S
{
    uint32      address;                  /* Address to read data   */
    uint16      length;                   /* Length of data to read */
    uint16      flags;
}UMI_MEM_READ_REQ;

typedef struct UMI_MEM_READ_CNF_S
{
    uint32      length;                   /*Length of Data*/
    uint32      result;                   /* Status */
    uint32      data[UMI_MEM_BLK_DWORDS];
}UMI_MEM_READ_CNF;

typedef struct UMI_MEM_WRITE_REQ_S
{
    uint32      address;
    uint16      length;                   /* Length of data */
    uint16      flags;
    uint32      data[UMI_MEM_BLK_DWORDS];
}UMI_MEM_WRITE_REQ;

typedef struct UMI_OID_802_11_CONFIGURE_IBSS_S // 4 bytes aligned
{
    uint8           channelNum;
    uint8           enableWMM;
    uint8           enableWEP;
    uint8           networkTypeInUse;
    uint16          atimWinSize;
    uint16          beaconInterval;
}UMI_OID_802_11_CONFIGURE_IBSS;

typedef struct UMI_OID_802_11_SET_UAPSD_S  // 4 bytes aligned
{
    uint16          uapsdFlags;
    uint16          reserved;
    uint16          autoTriggerInterval[4];
}UMI_OID_802_11_SET_UAPSD;


/*********************************************************************************/
/********          Callback Function Type Definition                      ********/
/*********************************************************************************/

typedef void (*IndicateEvent) (UL_HANDLE		    ulHandle,
                               UMI_STATUS_CODE	statusCode,
                               UMI_EVENT		    umiEvent,
                               uint16			      eventDataLength,
                               void 			      *pEventData
    ) ;

typedef void (*TxComplete) (
    UL_HANDLE		  ulHandle,
    UMI_STATUS_CODE  statusCode,
    UMI_TX_DATA      *pTxData
    ) ;

typedef UMI_STATUS_CODE (*DataReceived) (
    UL_HANDLE			 ulHandle,
    UMI_STATUS_CODE	 status,
    uint16			 length,
    void               *pFrame,
    void               *pDriverInfo
    ) ;

typedef void (*ScanInfo) (UL_HANDLE	                ulHandle,
                          uint32		                cacheInfoLen,
                          UMI_BSS_CACHE_INFO_IND		*pCacheInfo
    ) ;

typedef void (*GetParameterComplete) (UL_HANDLE		    ulHandle,
                                      uint16			    oid,
                                      UMI_STATUS_CODE	status,
                                      uint16			    length,
                                      void *			    pValue
    ) ;

typedef void (*SetParameterComplete) (UL_HANDLE			  ulHandle,
                                      uint16			    oid,
                                      UMI_STATUS_CODE	status
    ) ;

typedef void (*ConfigReqComplete) (UL_HANDLE			    ulHandle,
                                   UMI_STATUS_CODE		status
    ) ;

typedef void (*MemoryReadReqComplete) (UL_HANDLE			    ulHandle,
                                       UMI_MEM_READ_CNF		*pMemReadCnf
    ) ;

typedef void (*MemoryWriteReqComplete) (UL_HANDLE			    ulHandle,
                                        UMI_STATUS_CODE		status
    ) ;

typedef void (*Schedule) (UL_HANDLE		ulHandle) ;


typedef UMI_STATUS_CODE (* ScheduleTx) (LL_HANDLE	lowerHandle);

typedef void (* RxComplete) (LL_HANDLE	lowerHandle,
                             void *		pFrame
    );

typedef LL_HANDLE  (* Create) (UMI_HANDLE 	umiHandle,
                               DRIVER_HANDLE driverHandle
    );

typedef UMI_STATUS_CODE  (* Start) (LL_HANDLE lowerHandle,
                                    uint32		fwLength,
                                    void *		pFirmwareImage
    );

typedef UMI_STATUS_CODE  (* Stop) (LL_HANDLE  	lowerHandle);

typedef UMI_STATUS_CODE  (* Destroy) (LL_HANDLE	lowerHandle);


/*********************************************************************************/
/********          External Data Structure                                ********/
/*********************************************************************************/

/* The following data structure defines the callback interface the upper and
   lower layer is supposed to register, inorder to be notified as and when
   the corresponding events occures.
*/
typedef struct UL_CB_S
{
    IndicateEvent           indicateEvent_Cb          ;
    TxComplete              txComplete_Cb             ;
    DataReceived            dataReceived_Cb           ;
    ScanInfo                scanInfo_Cb               ;
    GetParameterComplete    getParameterComplete_Cb   ;
    SetParameterComplete    setParameterComplete_Cb   ;
    ConfigReqComplete       configReqComplete_Cb      ;
    MemoryReadReqComplete   memoryReadReqComplete_Cb  ;
    MemoryWriteReqComplete  memoryWriteReqComplete_Cb ;
    Schedule                schedule_Cb               ;
} UL_CB ;

typedef struct LL_CB_S
{
    Create				create_Cb				;
    Start				  start_Cb				;
    Destroy				destroy_Cb			;
    Stop				  stop_Cb					;
    RxComplete	  rxComplete_Cb		;
    ScheduleTx    scheduleTx_Cb   ;
} LL_CB ;

typedef struct  UMI_CREATE_IN_S
{
    uint32				apiVersion	;
    uint32				flags		    ;
    UL_HANDLE			ulHandle	  ;
    UL_CB				  ulCallbacks	;
    LL_CB				  llCallbacks	;
} UMI_CREATE_IN ;

typedef struct  UMI_CREATE_OUT_S
{
    UMI_HANDLE	umiHandle   ; /* Handle for UMI  Module */
    LL_HANDLE	  llHandle	  ; /* Handle for Lower Layer Driver Instance */
    uint32		mtu           ;
} UMI_CREATE_OUT ;

typedef struct  UMI_START_T_S
{
    uint8*    pFirmware ; /* The firmware image to be downloaded to the device */
    uint32    fmLength  ; /* The number of bytes in the firmware image.        */
    uint32*   pDPD      ; /* Pointer to Device Production Data.                */
    uint32    dpdLength ; /* The number of bytes in the DPD block.             */
} UMI_START_T ;

/*-----------------------------------------------------------------------*
 *                     Externally Visible Functions                      *
 *-----------------------------------------------------------------------*/

/*************************************************************************
 *                          HOST INTERFACE LAYER			                     *
 **************************************************************************/

/*********************************************************************************
 * NAME:	UMI_Create
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This is the first API that needs to be called by the Host device.It creates a UMI
 * object and initializes the UMI module.
 * \param pIn     -   A structure that contains all the input parameters to this function.
 * \param pOut    -   A structure that contains all the output parameters to this function.
 * \returns UMI_STATUS_CODE     A value of 0 indicates a success.
 */
UMI_STATUS_CODE  UMI_Create(IN UMI_CREATE_IN* pIn,
                            IN_OUT UMI_CREATE_OUT* pOut);

/*********************************************************************************
 * NAME:	UMI_Start
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This needs to be called by the Host device.This function starts the UMI module.
 * The caller should wait for UL_CB::IndicateEvent() to return an event that
 * indicates the start operation is completed.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param pStart      -   Pointer to the start-request parameters.
 * \returns UMI_STATUS_CODE     A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_Start(IN UMI_HANDLE umiHandle, IN UMI_START_T* pStart);

/*********************************************************************************
 * NAME:	UMI_Destroy
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function deletes a UMI object and free all resources owned by this object. The
 * caller should call UMI_Stop() and wait for UMI returning a stop-event before removing
 * the UMI object.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \returns UMI_STATUS_CODE     A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_Destroy(IN UMI_HANDLE umiHandle);

/*********************************************************************************
 * NAME:	UMI_Stop
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function requests to stop the UMI module. The caller should wait for
 * UMI_CB::IndicateEvent() to return an event that indicates the stop operation is
 * completed.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \returns UMI_STATUS_CODE     A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_Stop( IN UMI_HANDLE umiHandle );

/*********************************************************************************
 * NAME:	UMI_Transmit
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function indicates to the UMI module the driver has 802.3 frames to be sent to
 * the device.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param priority    -   Priority of the frame.
 * \param flags       -   b0 set indicates more frames waiting
 *                      to be serviced.[b1-b31 : reserved]
 * \param pTxData     -   pointer to the structure which
*                         include frame to transmit.
 * \returns UMI_STATUS_CODE     A value of 0 indicates a success.
 */
UMI_STATUS_CODE  UMI_Transmit(IN UMI_HANDLE         umiHandle   ,
                              IN uint8              priority    ,
                              IN uint32             flags       ,
                              IN UMI_TX_DATA*       pTxData
    );

/*********************************************************************************
 * NAME:	UMI_GetParameter
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function request to get a parameter from the UMI module or the WLAN device.
 * The caller should wait for UL_CB::GetParameterComplete() to retrieve the data.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param oid         -   The OID number that is associated with the parameter.
 * \returns UMI_GET_PARAM_STATUS If the Status filled in UMI_GET_PARAM_STATUS
 *                       structure is set to UMI_STATUS_SUCCESS than caller
 *                       will get parameter data in pValue filed. If Status
 *                       filled is set to UMI_STATUS_PENDING than caller
 *                       should wait for UL_CB::GetParameterComplete() to
 *                       retrieve the data.
 */
UMI_GET_PARAM_STATUS* UMI_GetParameter (
    IN UMI_HANDLE umiHandle,
    IN uint16 oid
    );

/*********************************************************************************
 * NAME:	UMI_SetParameter
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function request to set a parameter to the UMI module or the WLAN device.
 * This API is used to issue WLAN commands to the device.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param oid         -   The OID number that is associated with the parameter.
 * \param length      -   Number of bytes in the data.
 * \param pValue      -   The parameter data
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE  UMI_SetParameter(IN UMI_HANDLE umiHandle,
                                  IN uint16  oid,
                                  IN uint16  length,
                                  IN void*   pValue
    );

/*********************************************************************************
 * NAME:	UMI_Worker
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function executes the UMI state machine in a exclusive context scheduled
 * by the driver software.The function UL_CB::Schedule() indicate driver that
 * UMAC need context to process imminent events. If driver is free, than it will
 * call this function.This function shall return as soon as it has processed all
 * imminent events. It is not a thread function.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_Worker(IN UMI_HANDLE umiHandle);

/*********************************************************************************
 * NAME:	UMI_ConfigurationRequest
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function will format and send a Configuration request down to the Device.
 * Before calling this function the UMI module has to be initialized and started.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param length      -   Number of bytes in the data.
 * \param pValue      -   The parameter data.
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_ConfigurationRequest(IN UMI_HANDLE      umiHandle,
                                         IN uint16          length,
                                         IN UMI_CONFIG_REQ* pValue
    ) ;

/*********************************************************************************
 * NAME:	UMI_MemoryReadRequest
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function will format and send a memory read request down to the Device.
 * \param umiHandle - A UMI instance returned by UMI_Create().
 * \param length - Number of bytes in the data.
 * \param pValue - The parameter data.
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_MemoryReadRequest( IN UMI_HANDLE        umiHandle,
                                       IN uint16            length,
                                       IN UMI_MEM_READ_REQ* pValue
    ) ;

/*********************************************************************************
 * NAME:	UMI_MemoryWriteRequest
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function will format and send a memory write request down to the Device.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param length      -   Number of bytes in the data.
 * \param pValue - The parameter data.
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_MemoryWriteRequest(  IN UMI_HANDLE         umiHandle,
                                         IN uint16             length,
                                         IN UMI_MEM_WRITE_REQ* pValue
    ) ;

/*********************************************************************************
 * NAME:	UMI_RegisterEvents
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * Through this API upper layer will register event of their interest.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param eventMask   -   Bit mask of registered events.
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_RegisterEvents(IN UMI_HANDLE umiHandle,
                                   IN uint32 eventMask
    );

/*********************************************************************************
 * NAME:	UMI_RxFrmComplete
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function is called when the receiving of a frame is complete by the upper
 * layer driver so that the memory associated with the frame can be freed.
 * \param umiHandle   -   A UMI instance returned by UMI_Create().
 * \param pFrame      -   Pointer to the received frame.
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_RxFrmComplete( IN UMI_HANDLE  umiHandle,
                                   IN void*       pFrame
    ) ;



/*************************************************************************
 *                         DEVICE INTERFACE LAYER			                   *
 **************************************************************************/

/*********************************************************************************
 * NAME:	UMI_GetTxFrame
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * Pulls the frame queued in the Device Interface Layer.
 * \param UmiHandle   -   A UMI instance returned by UMI_Create().
 * \param pTxData     -   A structure that contains all the input-output parameters
 * to this function.
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE UMI_GetTxFrame(IN  UMI_HANDLE umiHandle,
                               OUT UMI_GET_TX_DATA **pTxData
    );

/*********************************************************************************
 * NAME:	UMI_ReceiveFrame
 *-------------------------------------------------------------------------------*/
/**
 * \brief
 * This function will receive frame buffer from lower layer driver.
 * \param umiHandle       -   A UMI instance returned by UMI_Create().
 * \param pFrameBuffer    -   The frame buffer form the Device.
 * \param pDriverInfo     -   Pointer to the Driver Info.
 * \returns UMI_STATUS_CODE A value of 0 indicates a success.
 */
UMI_STATUS_CODE  UMI_ReceiveFrame(
    IN UMI_HANDLE 	umiHandle,
    IN void* pFrameBuffer,
    IN void* pDriverInfo
    );

#endif /* _UMI_API_H */
