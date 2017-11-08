#ifndef __HAL88XX_FIRMWARE_H__
#define __HAL88XX_FIRMWARE_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXFirmware.h
	
Abstract:
	Defined HAL 88XX Firmware data structure & Define
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-04-11 Filen            Create.	
--*/


/**********************/
// Mapping C2H callback function


typedef struct _TXRPT_
{
    u1Byte RPT_MACID;
    u2Byte RPT_TXOK;    
    u2Byte RPT_TXFAIL;        
    u1Byte RPT_InitialRate;  
}__attribute__ ((packed)) TXRPT,*PTXRPT ;


typedef struct _APREQTXRPT_
{
    TXRPT txrpt[2];
}__attribute__ ((packed)) APREQTXRPT,*PAPREQTXRPT ;


#define GEN_FW_CMD_HANDLER(size, cmd)	{size, cmd##Handler},


//void	h2csetdsr(void);

struct cmdobj {
	u4Byte	        parmsize;
	VOID            (*c2hfuns)(IN HAL_PADAPTER Adapter,u1Byte *pbuf);	
};



//-----------------------------------------------------
//
//	0x1200h ~ 0x12FFh	DDMA CTRL
//
//-----------------------------------------------------
#define DDMA_LEN_MASK               0x0001FFFF
#define DDMA_CH_CHKSUM_CNT          BIT(24)
#define DDMA_RST_CHKSUM_STS         BIT(25)
#define DDMA_MODE_BLOCK_CPU         BIT(26)
#define DDMA_CHKSUM_FAIL            BIT(27)
#define DDMA_DA_W_DISABLE           BIT(28)
#define DDMA_CHKSUM_EN              BIT(29)
#define DDMA_CH_OWN                 BIT(31)




typedef struct _RTL88XX_FW_HDR_ 
{
    u2Byte      signature;
    u1Byte       category;
    u1Byte       function;

    u2Byte      version;
    u1Byte       subversion;
    u1Byte       rsvd1;

    u1Byte       month;      //human easy reading format
    u1Byte       day;        //human easy reading format
    u1Byte       hour;       //human easy reading format
    u1Byte       minute;     //human easy reading format

    u2Byte      ram_code_size;
    u1Byte       Foundry;  //0: TSMC,  1:UMC, 2:SMIC
    u1Byte       rsvd3;
    u4Byte        svnidx;
    u4Byte        rsvd5;
    u4Byte        rsvd6;
    u4Byte        rsvd7;
    
}RTL88XX_FW_HDR, *PRTL88XX_FW_HDR;

typedef struct _RTL88XX_MIPS_FW_HDR_ 
{
    //offset0
    u2Byte  signature;
    u1Byte  category;
    u1Byte  function;
    u2Byte  version;
    u1Byte  Subversion;
    u1Byte  sub_index;
    //offset8
    u4Byte  SVN_index;
    u4Byte  rsvd1;
    //offset16
    u1Byte  Month;
    u1Byte  Date;
    u1Byte  Hour;
    u1Byte  Min;
    u2Byte  Year;
    u1Byte  Foundry;
    u1Byte  rsvd2;
    //offset24
    u1Byte  MEM_USAGE__DL_from:1;
    u1Byte  MEM_USAGE__BOOT_from:1;
    u1Byte  MEM_USAGE__BOOT_LOADER:1;
    u1Byte  MEM_USAGE__IRAM:1;
    u1Byte  MEM_USAGE__ERAM:1;
    u1Byte  MEM_USAGE__rsvd4:3;
    u1Byte  rsvd3;
    u2Byte  BOOT_LOADER_SZ;
    u4Byte  rsvd5;
    //offset32
    u4Byte  TOTAL_DMEM_SZ;
    u2Byte  FW_CFG_SZ;
    u2Byte  FW_ATTR_SZ;
    //offset40
    u4Byte  IROM_SZ;
    u4Byte  EROM_SZ;           
    //offset 48
    u4Byte  IRAM_SZ;
    u4Byte  ERAM_SZ;
    //offset 56
    u4Byte  rsvd6;
    u4Byte  rsvd7;
}RTL88XX_MIPS_FW_HDR, *PRTL88XX_MIPS_FW_HDR;

// TODO: Filen, check below
typedef enum _RTL88XX_H2C_CMD 
{
//	H2C_88XX_RSVDPAGE               = 0,
	H2C_88XX_MSRRPT             = 0x1,	
//	H2C_88XX_KEEP_ALIVE_CTRL    = 0x3,
//	H2C_88XX_WO_WLAN            = 0x5,	// Wake on Wlan.
//	H2C_88XX_REMOTE_WAKEUP      = 0x7, 
	H2C_88XX_AP_OFFLOAD         = 0x8,
	H2C_88XX_BCN_RSVDPAGE       = 0x9,
	H2C_88XX_PROBE_RSVDPAGE     = 0xa,
    H2C_88XX_WAKEUP_PIN          = 0x13,	
//	H2C_88XX_SETPWRMODE         = 0x20,		
//	H2C_88XX_P2P_PS_MODE        = 0x24,

#ifdef SOFTAP_PS_DURATION
	H2C_88XX_SAP_PS        = 0x26,
#endif

	H2C_88XX_RA_MASK            = 0x40,
	H2C_88XX_RSSI_REPORT        = 0x42,
	H2C_88XX_AP_REQ_TXREP		= 0x43,
	H2C_88XX_NHM				= 0xC1,
	MAX_88XX_H2CCMD
}RTL88XX_H2C_CMD;


typedef enum _RTL88XX_C2H_CMD 
{
//	C2H_88XX_DBG                = 0,
//	C2H_88XX_C2H_LB             = 0x1,	
//	C2H_88XX_SND_TXBF           = 0x2,
//	C2H_88XX_CCXRPT             = 0x3,
	C2H_88XX_APREQTXRPT         = 0x4,
//	C2H_88XX_INITIALRATE        = 0x5,
//	C2H_88XX_PSD_RPT            = 0x6,
//	C2H_88XX_SCAN_COMPLETE      = 0x7, 
//	C2H_88XX_PSD_CONTROL        = 0x8,
//	C2H_88XX_BT_INFO            = 0x9,
//	C2H_88XX_BT_LOOPBACK        = 0xa,	
	MAX_88XX_C2HCMD
}RTL88XX_C2H_CMD;


VOID
ReadMIPSFwHdr88XX(
    IN  HAL_PADAPTER    Adapter
);

RT_STATUS
InitMIPSFirmware88XX(
    IN  HAL_PADAPTER    Adapter
);


RT_STATUS
InitFirmware88XX(
    IN  HAL_PADAPTER    Adapter
);

#if 0
typedef struct _H2C_CONTENT_
{
    u4Byte  content;
    u2Byte  ext_content;
}H2C_CONTENT, *PH2C_CONTENT;



BOOLEAN
IsH2CBufOccupy88XX(
    IN  HAL_PADAPTER    Adapter
);


BOOLEAN
SigninH2C88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PH2C_CONTENT    pH2CContent
);
#else
BOOLEAN
CheckFwReadLastH2C88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  u1Byte          BoxNum
);

RT_STATUS
FillH2CCmd88XX(
	IN  HAL_PADAPTER    Adapter,
	IN	u1Byte 		    ElementID,
	IN	u4Byte 		    CmdLen,
	IN	pu1Byte		    pCmdBuffer
);
#endif



VOID
UpdateHalRAMask88XX(
	IN HAL_PADAPTER         Adapter,	
	HAL_PSTAINFO            pEntry,
	u1Byte				    rssi_level
);

void
UpdateHalMSRRPT88XX(
	IN HAL_PADAPTER     Adapter,
	HAL_PSTAINFO        pEntry,
	u1Byte              opmode
);

void
SetAPOffload88XX(
	IN HAL_PADAPTER     Adapter,
	u1Byte              bEn,
	u1Byte              numOfAP,
	u1Byte              bHidden,	
	u1Byte              bDenyAny,
	pu1Byte             loc_bcn,
	pu1Byte             loc_probe
);

#ifdef SOFTAP_PS_DURATION
 VOID 
SetSAPPS88XX
(    
	IN HAL_PADAPTER     Adapter,
        u1Byte en,
        u1Byte duration
);
#endif

VOID
SetRsvdPage88XX
( 
	IN  IN HAL_PADAPTER     Adapter,
    IN  pu1Byte             prsp,
    IN  pu4Byte             beaconbuf,    
    IN  u4Byte              pktLen,  
    IN  u4Byte              bigPktLen        
);

u4Byte
GetRsvdPageLoc88XX
( 
	IN  IN HAL_PADAPTER     Adapter,
    IN  u4Byte              frlen,
    OUT pu1Byte             loc_page
);

BOOLEAN
DownloadRsvdPage88XX
( 
	IN HAL_PADAPTER     Adapter,
    IN  pu4Byte         beaconbuf,    
    IN  u4Byte          beaconPktLen,
    IN  u1Byte          bReDownload    
);

void C2HHandler88XX(
    IN HAL_PADAPTER     Adapter
);



#endif  //__HAL88XX_FIRMWARE_H__

