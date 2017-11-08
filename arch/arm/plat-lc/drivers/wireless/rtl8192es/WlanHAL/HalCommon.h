#ifndef __HALCOMMON_H__
#define __HALCOMMON_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	HalCommon.h
	
Abstract:
	Defined HAL Common
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-05-18  Lun-Wu            Create.	
--*/

// Total 32bytes, we need control in 8bytes

VOID
HalGeneralDummy(
	IN	HAL_PADAPTER    Adapter
);


RT_STATUS 
HAL_ReadTypeID(
	INPUT	HAL_PADAPTER	Adapter
);

VOID
ResetHALIndex(
    VOID
);

VOID
DecreaseHALIndex(
    VOID
);

RT_STATUS
HalAssociateNic(
    HAL_PADAPTER        Adapter,
    BOOLEAN			IsDefaultAdapter    
);

RT_STATUS
HalDisAssociateNic(
    HAL_PADAPTER        Adapter,
    BOOLEAN			    IsDefaultAdapter    
);

VOID 
SoftwareCRC32 (
    IN  pu1Byte     pBuf,
    IN  u2Byte      byteNum,
    OUT pu4Byte     pCRC32
);


u1Byte 
GetXorResultWithCRC (
    IN  u1Byte      a,
    IN  u1Byte      b
);

u1Byte
CRC5 (
    IN pu1Byte      dwInput,
    IN u1Byte       len
);

VOID 
SoftwareCRC32_RXBuffGather (
    IN  pu1Byte     pPktBufAddr,
    IN  pu2Byte     pPktBufLen,  
    IN  u2Byte      pktNum,
    OUT pu4Byte     pCRC32
);

RT_STATUS 
GetTxRPTBuf88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u4Byte              macID,
    IN  u1Byte              variable,    
    OUT pu1Byte             val
);

RT_STATUS 
SetTxRPTBuf88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u4Byte              macID,
    IN  u1Byte              variable,
    IN  pu1Byte             val    
);

VOID
SetCRC5ToRPTBuffer88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u1Byte              val,
    IN	u4Byte              macID,
    IN  u1Byte              bValid
    
);

RT_STATUS
ReleaseOnePacket88XX(
    IN  HAL_PADAPTER        Adapter,
    IN  u1Byte              macID
);

BOOLEAN
LoadFileToIORegTable(
    IN  pu1Byte     pRegFileStart,
    IN  u4Byte      RegFileLen,
    OUT pu1Byte     pTableStart,
    IN  u4Byte      TableEleNum
);

BOOLEAN
LoadFileToOneParaTable(
    IN  pu1Byte     pFileStart,
    IN  u4Byte      FileLen,
    OUT pu1Byte     pTableStart,
    IN  u4Byte      TableEleNum
);

#endif // __HALCOMMON_H__

