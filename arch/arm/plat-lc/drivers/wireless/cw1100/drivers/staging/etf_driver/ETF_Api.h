/*
* ST-Ericsson ETF driver
*
* Copyright (c) ST-Ericsson SA 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
/****************************************************************************
* ETF_Api.h
*
* This is the header file for ETF API module.
*
****************************************************************************/

#ifndef _ETF_API_H
#define _ETF_API_H

#include "stddefs.h"

#define IN
#define IN_OUT
#define OUT

#define ETF_TX_POWER_LEVEL_CONTROLED_BY_DEVICE 0x00

#define ETH_HEADER_SIZE 14
#define ETH_MAX_DATA_SIZE 1500
#define ETH_MAX_PACKET_SIZE (ETH_HEADER_SIZE + ETH_MAX_DATA_SIZE)
#define ETH_MIN_PACKET_SIZE 60

#define SCAN_INFO_BEACON 1
#define SCAN_INFO_PROBE_RESP 0

#define MAX_802_11_HDR_SIZE 30
#define SNAP_HDR_SIZE 8
#define ENCRYPTION_HDR_SIZE 8
#define ETF_INTERNAL_HDR_SIZE 36
#define ETF_MAC_ADDRESS_SIZE 6
#define ETF_DOT11_COUNTRY_STRING_SIZE 3
#define ETF_MAX_SSID_SIZE 32
#define ETF_MAX_ETHERNET_FRAME_SIZE 1500

/* Encryption Status */
#define ETF_ENC_STATUS_NO_ENCRYPTION 0
#define ETF_ENC_STATUS_WEP_SUPPORTED (1<<0)
#define ETF_ENC_STATUS_TKIP_SUPPORTED (1<<1)
#define ETF_ENC_STATUS_AES_SUPPORTED (1<<2)
#define ETF_ENC_STATUS_MASK_ENC_TYPE 0x7 /*three bits*/
#define ETF_ENC_STATUS_MASK_KEY_AVAIL ((uint32) (1<<31))

/* For Add Key */
#define ETF_ADDKEY_KEYSIZE_WEP_40 5
#define ETF_ADDKEY_KEYSIZE_WEP_104 13
#define ETF_ADDKEY_KEYSIZE_TKIP 32
#define ETF_ADDKEY_KEYSIZE_AES 16
#define ETF_ADDKEY_KEYIDX_TX_KEY ((uint32)(1<<31)) /* 0x80000000*/
#define ETF_ADDKEY_KEYIDX_PAIRWISEKEY (1<<30) /* 0x40000000*/
#define ETF_ADDKEY_KEYIDX_KEYRSC (1<<29) /* 0x20000000*/
#define ETF_ADDKEY_KEYIDX_FIRST_TXKEY_THEN_RXKEY (1<<28) /* 0x10000000*/
#define ETF_ADDKEY_KEYIDX_KEYID_MASK 0x000000FF

/* For PsMode */
#define ETF_PS_MODE_DISABLED 0
#define ETF_PS_MODE_ENABLED 1

/* Priority Mask */
#define ETF_PRIORITY_DEFAULT 0x00
#define ETF_PRIORITY_0 0x00
#define ETF_PRIORITY_1 0x01
#define ETF_PRIORITY_2 0x02
#define ETF_PRIORITY_3 0x03
#define ETF_PRIORITY_4 0x04
#define ETF_PRIORITY_5 0x05
#define ETF_PRIORITY_6 0x06
#define ETF_PRIORITY_7 0x07

#define ETF_MEM_BLK_DWORDS (1024/4)

typedef void *UL_HANDLE; /* Standardized Name */

typedef void *LL_HANDLE; /* Standardized Name */

typedef void *DRIVER_HANDLE; /* Standardized Name */

typedef void *ETF_HANDLE; /* Standardized Name */

typedef enum ETF_STATUS_CODE_E {
	ETF_STATUS_SUCCESS,
	ETF_STATUS_FAILURE,
	ETF_STATUS_UNKNOWN_CHIP,
	ETF_STATUS_PENDING,
	ETF_STATUS_BAD_PARAM,
	ETF_STATUS_OUT_OF_RESOURCES,
	ETF_STATUS_TX_LIFETIME_EXCEEDED,
	ETF_STATUS_RETRY_EXCEEDED,
	ETF_STATUS_LINK_LOST,
	ETF_STATUS_REQ_REJECTED,
	ETF_STATUS_AUTH_FAILED,
	ETF_STATUS_NO_MORE_DATA,
	ETF_STATUS_COUNTRY_INFO_MISMATCH,
	ETF_STATUS_COUNTRY_NOT_FOUND_IN_TABLE,
	ETF_STATUS_MISMATCH_ERROR,
	ETF_STATUS_CONF_ERROR,
	ETF_STATUS_UNKNOWN_OID_ERROR,
	ETF_STATUS_NOT_SUPPORTED,
	ETF_STATUS_UNSPECIFIED_ERROR,
	ETF_STATUS_MAX
} ETF_STATUS_CODE;

typedef enum ETF_EVENTS_E {
	ETF_EVT_IDLE,
	ETF_EVT_START_COMPLETED,
	ETF_EVT_STOP_COMPLETED,
	ETF_EVT_CONNECTING,
	ETF_EVT_CONNECTED,
	ETF_EVT_CONNECT_FAILED,
	ETF_EVT_DISCONNECTED,
	ETF_EVT_SCAN_COMPLETED,
	ETF_EVT_GENERAL_FAILURE,
	ETF_EVT_TX_RATE_CHANGED,
	ETF_EVT_RECONNECTED,
	ETF_EVT_MICFAILURE,
	ETF_EVT_RADAR,
	/* Add more events here */
	ETF_EVT_MAX
} ETF_EVENT;

typedef struct ETF_EVT_DATA_MIC_FAILURE_S {
	uint8 Rsc[6];
	uint8 IsPairwiseKey;
	uint8 KeyIndex;
	uint32 MicFailureCount;
	uint8 PeerAddress[6];
	uint16 Reserved;
} ETF_EVT_DATA_MIC_FAILURE;

typedef enum ETF_REGULATORY_CLASS_E {
	ETF_REG_CLASS_CEPT_1_3,
	ETF_REG_CLASS_FCC_1_4,
	ETF_REG_CLASS_MPHPT_1_6,
	ETF_REG_CLASS_MPHPT_7_11,
	ETF_REG_CLASS_MPHPT_12_15,
	ETF_REG_CLASS_MPHPT_16_20,
	ETF_REG_CLASS_MAX_VALUE
} ETF_REGULATORY_CLASS;

typedef enum ETF_INFRASTRUCTURE_MODE_E {
	ETF_802_IBSS,
	ETF_802_INFRASTRUCTURE,
	ETF_802_AUTO_UNKNOWN,
	ETF_802_INFRASTRUCTURE_MODE_MAX_VALUE
} ETF_INFRASTRUCTURE_MODE;

/* Authentication modes */
typedef enum ETF_AUTHENTICATION_MODE_E {
	ETF_AUTH_MODE_OPEN,
	ETF_AUTH_MODE_SHARED,
	ETF_AUTH_MODE_WPA,
	ETF_AUTH_MODE_WPA_PSK,
	ETF_AUTH_MODE_WPA_NONE,
	ETF_AUTH_MODE_WPA_2,
	ETF_AUTH_MODE_WPA_2_PSK
} ETF_AUTHENTICATION_MODE;

typedef struct ETF_DOT11_COUNTRY_STRING_S /* 4 byte aligned*/ {
	uint8 dot11CountryString[ETF_DOT11_COUNTRY_STRING_SIZE];
} ETF_DOT11_COUNTRY_STRING;

typedef struct ETF_802_11_SSID_S /* 4 byte aligned*/ {
	uint32 ssidLength;
	uint8 ssid[ETF_MAX_SSID_SIZE];
} ETF_802_11_SSID;

typedef struct ETF_BSS_LIST_SCAN_S /* 4 byte aligned*/ {
	uint32 flags; /*Bit 0: auto band
			Bit 1: 2.4G band
			Bit 2: 4.9G band
			Bit 3: 5G band
			Bit 4 - 31 : Reserved
			Value 0: stop scan */
	uint32 powerLevel;      /* 0 the transmit power level for active scan
				is determined by the device.Otherwise,
				the transmit power in dBm. */
	uint16 ssidLength;
	/* SSID may follow in the following locations,
	if its bigger than 2 bytes */
	uint8 ssid[2];
} ETF_BSS_LIST_SCAN;

typedef struct ETF_OID_802_11_ASSOCIATION_INFORMATION_S /* 4 byte aligned*/ {
	uint16 capabilitiesReq;
	uint16 listenInterval;
	uint8 currentApAddress[ETF_MAC_ADDRESS_SIZE];
	uint16 capabilitiesResp;
	uint16 statusCode; /* Status code return in assoc/reassoc response */
	uint16 associationId;
	uint16 variableIELenReq;
	uint16 variableIELenRsp;
} ETF_OID_802_11_ASSOCIATION_INFORMATION;

/* Add-Key Request */

typedef union ETF_PRIVACY_KEY_DATA_U {
	struct {
		uint8 peerAddress[6];
		uint8 reserved;
		uint8 keyLength;
		uint8 keyData[16];
	} WepPairwiseKey;

	struct {
		uint8 keyId;
		uint8 keyLength;
		uint8 reserved[2];
		uint8 keyData[16];
	} WepGroupKey;

	struct {
		uint8 peerAddress[6];
		uint8 reserved[2];
		uint8 tkipKeyData[16];
		uint8 rxMicKey[8];
		uint8 txMicKey[8];
	} TkipPairwiseKey;

	struct {
		uint8 tkipKeyData[16];
		uint8 rxMicKey[8];
		uint8 keyId;
		uint8 reserved[3];
		uint8 rxSequenceCounter[8];
	} TkipGroupKey;

	struct {
		uint8 peerAddress[6];
		uint8 reserved[2];
		uint8 aesKeyData[16];
	} AesPairwiseKey;

	struct {
		uint8 aesKeyData[16];
		uint8 keyId;
		uint8 reserved[3];
		uint8 rxSequenceCounter[8];
	} AesGroupKey;

	struct {
		uint8 peerAddress[6];
		uint8 keyId;
		uint8 reserved;
		uint8 wapiKeyData[16];
		uint8 micKeyData[16];
	} WapiPairwiseKey;

	struct {
		uint8 wapiKeyData[16];
		uint8 micKeyData[16];
		uint8 keyId;
		uint8 reserved[3];
	} WapiGroupKey;
} ETF_PRIVACY_KEY_DATA;

typedef struct ETF_OID_802_11_KEY_S /* 4 byte aligned*/ {
	uint8 keyType;
	uint8 entryIndex;
	uint16 reserved;
	ETF_PRIVACY_KEY_DATA Key;
} ETF_OID_802_11_KEY;

typedef struct ETF_OID_802_11_REMOVE_KEY_S {
	uint8 EntryIndex;
	uint8 Reserved[3];
} ETF_OID_802_11_REMOVE_KEY;

typedef struct ETF_OID_802_11_STATISTICS_S /* 4 byte aligned*/ {
	uint32 countPlcpErrors;
	uint32 countFcsErrors;
	uint32 countTxPackets;
	uint32 countRxPackets;
	uint32 countRxPacketErrors;
	uint32 countRxDecryptionFailures;
	uint32 countRxMicFailures;
	uint32 countRxNoKeyFailures;
	uint32 countTxMulticastFrames;
	uint32 countTxFramesSuccess;
	uint32 countTxFrameFailures;
	uint32 countTxFramesRetried;
	uint32 countTxFramesMultiRetried;
	uint32 countRxFrameDuplicates;
	uint32 countRtsSuccess;
	uint32 countRtsFailures;
	uint32 countAckFailures;
	uint32 countRxMulticastFrames;
	uint32 countRxFramesSuccess;
} ETF_OID_802_11_STATISTICS;

typedef struct ETF_BSS_CACHE_INFO_IND_S /* 4 byte aligned*/ {
	uint8 cacheLine;
	uint8 flags; /* b0 is set => beacon, clear => probe resp */
	uint8 bssId[ETF_MAC_ADDRESS_SIZE];
	sint8 rssi;
	uint8 rcpi;
	uint16 channelNumber;
	uint32 freqKhz;
	/* From Beacon Body - 802.11 frame endianness (little endian) */
	uint64 timeStamp;
	uint16 beaconInterval;
	uint16 capability;
	uint16 ieLength; /* Note: Cpu endianness -(not from 802.11 frame) */
	/* IE elements may follow, if its larger than 2 bytes */
	uint8 ieElements[2];
} ETF_BSS_CACHE_INFO_IND;

typedef struct ETF_BEACON_CAPABILITIY_S {
	uint16 ess:1; /* Bit 00 */
	uint16 ibss:1; /* Bit 01 */
	uint16 cfPollable:1; /* Bit 02 */
	uint16 cfPollRequest:1; /* Bit 03 */
	uint16 privacy:1; /* Bit 04 */
	uint16 shortPreamble:1; /* Bit 05 */
	uint16 pbcc:1; /* Bit 06 */
	uint16 channelAgility:1; /* Bit 07 */
	uint16 spectrumMgmt:1; /* Bit 08 */
	uint16 qos:1; /* Bit 09 */
	uint16 shortSlotTime:1; /* Bit 10 */
	uint16 apsd:1; /* Bit 11 */
	uint16 reserved:1; /* Bit 12 */
	uint16 dsssOFDM:1; /* Bit 13 */
	uint16 delayedBlockAck:1; /* Bit 14 */
	uint16 immdediateBlockAck:1; /* Bit 15 */
} ETF_BEACON_CAPABILITY;

/* List of Beacon and Probe Response */
typedef struct ETF_BSS_CACHE_INFO_IND_LIST_S {
	uint32 numberOfItems;
	ETF_BSS_CACHE_INFO_IND bssCacheInfoInd[1];
} ETF_BSS_CACHE_INFO_IND_LIST;

typedef struct ETF_BLOCK_ACK_POLICY_S {
	/*Bits 7:0 Correspond to TIDs 7:0 respectively*/
	uint8 blockAckTxTidPolicy;
	uint8 reserved1;
	/*Bits 7:0 Correspond to TIDs 7:0 respectively*/
	uint8 blockAckRxTidPolicy;
	uint8 reserved2;
} ETF_BLOCK_ACK_POLICY;

typedef struct ETF_UPDATE_EPTA_CONFIG_DATA_S {
	uint32 ConfigurationParameters;
	uint32 OccupancyLimits;
	uint32 PriorityParameters;
	uint32 PeriodParameters;
	uint32 LinkExpiryPeriod;
} ETF_UPDATE_EPTA_CONFIG_DATA;

typedef struct ETF_TX_DATA_S {
	uint8 *pExtraBuffer;
	uint8 *pEthHeader;
	uint32 ethPayloadLen;
	uint8 *pEthPayloadStart;
	void *pDriverInfo;
} ETF_TX_DATA;

typedef struct ETF_GET_TX_DATA_S {
	uint32 bufferLength;
	uint8 *pTxDesc;
	uint8 *pDot11Frame;
	void *pDriverInfo;
} ETF_GET_TX_DATA;

typedef struct ETF_GET_PARAM_STATUS_S {
	uint16 oid; /* The OID number that associated with the parameter. */
	ETF_STATUS_CODE status; /* Status code for the get operation. */
	uint16 length; /* Number of bytes in the data. */
	void *pValue; /* The parameter data. */
} ETF_GET_PARAM_STATUS;

typedef struct ETF_HI_DPD_S /* 4 byte aligned*/ {
	uint16 length; /* length in bytes of the DPD struct */
	uint16 version; /* version of the DPD record */
	uint8 macAddress[6]; /* Mac addr of the device */
	uint16 flags; /* record options */
	uint32 sddData[1]; /* 1st 4 bytes of the Static & Dynamic Data */
	/* rest of the Static & Dynamic Data */
} ETF_HI_DPD;

typedef struct ETF_CONFIG_REQ_S /* 4 byte aligned*/ {
	uint32 dot11MaxTransmitMsduLifeTime;
	uint32 dot11MaxReceiveLifeTime;
	uint32 dot11RtsThreshold;
	ETF_HI_DPD dpdData;
} ETF_CONFIG_REQ;

typedef struct ETF_MEM_READ_REQ_S {
	uint32 address; /* Address to read data */
	uint16 length; /* Length of data to read */
	uint16 flags;
} ETF_MEM_READ_REQ;

typedef struct ETF_MEM_READ_CNF_S {
	uint32 length; /*Length of Data*/
	uint32 result; /* Status */
	uint32 data[ETF_MEM_BLK_DWORDS];
} ETF_MEM_READ_CNF;

typedef struct ETF_MEM_WRITE_REQ_S {
	uint32 address;
	uint16 length; /* Length of data */
	uint16 flags;
	uint32 data[ETF_MEM_BLK_DWORDS];
} ETF_MEM_WRITE_REQ;

typedef struct ETF_OID_802_11_CONFIGURE_IBSS_S /* 4 bytes aligned*/ {
	uint8 channelNum;
	uint8 enableWMM;
	uint8 enableWEP;
	uint8 networkTypeInUse;
	uint16 atimWinSize;
	uint16 beaconInterval;
} ETF_OID_802_11_CONFIGURE_IBSS;

typedef struct ETF_OID_802_11_SET_UAPSD_S /* 4 bytes aligned*/ {
	uint16 uapsdFlags;
	uint16 reserved;
	uint16 autoTriggerInterval[4];
} ETF_OID_802_11_SET_UAPSD;

typedef void (*IndicateEvent) (UL_HANDLE ulHandle,
		ETF_STATUS_CODE statusCode,
		ETF_EVENT umiEvent,
		uint16 eventDataLength,
		void *pEventData);

typedef void (*TxComplete) (UL_HANDLE ulHandle,
		ETF_STATUS_CODE statusCode,
		ETF_TX_DATA * pTxData);

typedef ETF_STATUS_CODE (*DataReceived) (UL_HANDLE ulHandle,
		ETF_STATUS_CODE status,
		uint16 length,
		void *pFrame,
		void *pDriverInfo);

typedef void (*ScanInfo) (UL_HANDLE ulHandle,
		uint32 cacheInfoLen,
		ETF_BSS_CACHE_INFO_IND * pCacheInfo);

typedef void (*GetParameterComplete) (UL_HANDLE ulHandle,
		uint16 oid,
		ETF_STATUS_CODE status,
		uint16 length,
		void *pValue);

typedef void (*SetParameterComplete) (UL_HANDLE ulHandle,
		uint16 oid,
		ETF_STATUS_CODE status);

typedef void (*ConfigReqComplete) (UL_HANDLE ulHandle,
		ETF_STATUS_CODE status);

typedef void (*MemoryReadReqComplete) (UL_HANDLE ulHandle,
		ETF_MEM_READ_CNF * pMemReadCnf);

typedef void (*MemoryWriteReqComplete) (UL_HANDLE ulHandle,
		ETF_STATUS_CODE status);

typedef void (*Schedule) (UL_HANDLE ulHandle);

typedef ETF_STATUS_CODE (*ScheduleTx) (LL_HANDLE lowerHandle);

typedef void (*RxComplete) (LL_HANDLE lowerHandle,
		void *pFrame);

typedef LL_HANDLE (*Create) (ETF_HANDLE umiHandle,
		DRIVER_HANDLE driverHandle);

typedef ETF_STATUS_CODE (*Start) (LL_HANDLE lowerHandle,
		uint32 fwLength,
		void *pFirmwareImage);

typedef ETF_STATUS_CODE (*Stop) (LL_HANDLE lowerHandle);

typedef ETF_STATUS_CODE (*Destroy) (LL_HANDLE lowerHandle);

/* The following data structure defines the callback interface the upper and
lower layer is supposed to register, inorder to be notified as and when
the corresponding events occures.
*/
typedef struct UL_CB_S {
	IndicateEvent indicateEvent_Cb;
	TxComplete txComplete_Cb;
	DataReceived dataReceived_Cb;
	ScanInfo scanInfo_Cb;
	GetParameterComplete getParameterComplete_Cb;
	SetParameterComplete setParameterComplete_Cb;
	ConfigReqComplete configReqComplete_Cb;
	MemoryReadReqComplete memoryReadReqComplete_Cb;
	MemoryWriteReqComplete memoryWriteReqComplete_Cb;
	Schedule schedule_Cb;
} UL_CB;

typedef struct LL_CB_S {
	Create create_Cb;
	Start start_Cb;
	Destroy destroy_Cb;
	Stop stop_Cb;
	RxComplete rxComplete_Cb;
	ScheduleTx scheduleTx_Cb;
} LL_CB;

typedef struct ETF_CREATE_IN_S {
	uint32 apiVersion;
	uint32 flags;
	UL_HANDLE ulHandle;
	UL_CB ulCallbacks;
	LL_CB llCallbacks;
} ETF_CREATE_IN;

typedef struct ETF_CREATE_OUT_S {
	ETF_HANDLE umiHandle; /* Handle for ETF Module */
	LL_HANDLE llHandle; /* Handle for Lower Layer Driver Instance */
	uint32 mtu;
} ETF_CREATE_OUT;

typedef struct ETF_START_T_S {
	uint8 *pFirmware; /* The firmware image to be downloaded to device */
	uint32 fmLength; /* The number of bytes in the firmware image. */
	uint32 *pDPD; /* Pointer to Device Production Data. */
	uint32 dpdLength; /* The number of bytes in the DPD block. */
} ETF_START_T;

/*********************************************************************************
* NAME: ETF_Create
*-------------------------------------------------------------------------------*/
/**
* \brief
* This is the first API that needs to be called by the Host device.It creates a ETF
* object and initializes the ETF module.
* \param pIn - A structure that contains all the input parameters to this function.
* \param pOut - A structure that contains all the output parameters to this function.
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_Create(IN ETF_CREATE_IN * pIn,
		IN_OUT ETF_CREATE_OUT * pOut);

/*********************************************************************************
* NAME: ETF_Start
*-------------------------------------------------------------------------------*/
/**
* \brief
* This needs to be called by the Host device.This function starts the ETF module.
* The caller should wait for UL_CB::IndicateEvent() to return an event that
* indicates the start operation is completed.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param pStart - Pointer to the start-request parameters.
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_Start(IN ETF_HANDLE umiHandle, IN ETF_START_T * pStart);

/*********************************************************************************
* NAME: ETF_Destroy
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function deletes a ETF object and free all resources owned by this object. The
* caller should call ETF_Stop() and wait for ETF returning a stop-event before removing
* the ETF object.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_Destroy(IN ETF_HANDLE umiHandle);

/*********************************************************************************
* NAME: ETF_Stop
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function requests to stop the ETF module. The caller should wait for
* ETF_CB::IndicateEvent() to return an event that indicates the stop operation is
* completed.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_Stop(IN ETF_HANDLE umiHandle);

/*********************************************************************************
* NAME: ETF_Transmit
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function indicates to the ETF module the driver has 802.3 frames to be sent to
* the device.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param priority - Priority of the frame.
* \param flags - b0 set indicates more frames waiting
* to be serviced.[b1-b31 : reserved]
* \param pTxData - pointer to the structure which
* include frame to transmit.
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_Transmit(IN ETF_HANDLE umiHandle,
		IN uint8 priority,
		IN uint32 flags,
		IN ETF_TX_DATA * pTxData);

/*********************************************************************************
* NAME: ETF_GetParameter
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function request to get a parameter from the ETF module or the WLAN device.
* The caller should wait for UL_CB::GetParameterComplete() to retrieve the data.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param oid - The OID number that is associated with the parameter.
* \returns ETF_GET_PARAM_STATUS If the Status filled in ETF_GET_PARAM_STATUS
* structure is set to ETF_STATUS_SUCCESS than caller
* will get parameter data in pValue filed. If Status
* filled is set to ETF_STATUS_PENDING than caller
* should wait for UL_CB::GetParameterComplete() to
* retrieve the data.
*/
ETF_GET_PARAM_STATUS *ETF_GetParameter(IN ETF_HANDLE umiHandle,
		IN uint16 oid);

/*********************************************************************************
* NAME: ETF_SetParameter
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function request to set a parameter to the ETF module or the WLAN device.
* This API is used to issue WLAN commands to the device.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param oid - The OID number that is associated with the parameter.
* \param length - Number of bytes in the data.
* \param pValue - The parameter data
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_SetParameter(IN ETF_HANDLE umiHandle,
		IN uint16 oid,
		IN uint16 length,
		IN void *pValue);

/*********************************************************************************
* NAME: ETF_Worker
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function executes the ETF state machine in a exclusive context scheduled
* by the driver software.The function UL_CB::Schedule() indicate driver that
* ETF need context to process imminent events. If driver is free, than it will
* call this function.This function shall return as soon as it has processed all
* imminent events. It is not a thread function.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_Worker(IN ETF_HANDLE umiHandle);

/*********************************************************************************
* NAME: ETF_ConfigurationRequest
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function will format and send a Configuration request down to the Device.
* Before calling this function the ETF module has to be initialized and started.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param length - Number of bytes in the data.
* \param pValue - The parameter data.
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_ConfigurationRequest(IN ETF_HANDLE umiHandle,
		IN uint16 length,
		IN ETF_CONFIG_REQ * pValue);

/*********************************************************************************
* NAME: ETF_MemoryReadRequest
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function will format and send a memory read request down to the Device.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param length - Number of bytes in the data.
* \param pValue - The parameter data.
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_MemoryReadRequest(IN ETF_HANDLE umiHandle,
		IN uint16 length,
		IN ETF_MEM_READ_REQ * pValue);

/*********************************************************************************
* NAME: ETF_MemoryWriteRequest
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function will format and send a memory write request down to the Device.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param length - Number of bytes in the data.
* \param pValue - The parameter data.
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_MemoryWriteRequest(IN ETF_HANDLE umiHandle, IN uint16 length,
		IN ETF_MEM_WRITE_REQ * pValue);

/*********************************************************************************
* NAME: ETF_ReceiveFrame
*-------------------------------------------------------------------------------*/
/**
* \brief
* This function will receive frame buffer from lower layer driver.
* \param umiHandle - A ETF instance returned by ETF_Create().
* \param pFrameBuffer - The frame buffer form the Device.
* \param pDriverInfo - Pointer to the Driver Info.
* \returns ETF_STATUS_CODE A value of 0 indicates a success.
*/
ETF_STATUS_CODE ETF_ReceiveFrame(IN ETF_HANDLE umiHandle,
		IN void *pFrameBuffer,
		IN void *pDriverInfo);

#endif /* _ETF_API_H */
