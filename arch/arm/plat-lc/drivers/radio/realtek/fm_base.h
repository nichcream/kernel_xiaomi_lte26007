#ifndef __FM_BASE_H
#define __FM_BASE_H



#include "foundation.h"




// Register entry
typedef struct
{
	int RegBitName;
	unsigned short RegStartAddr;
	unsigned char Msb;
	unsigned char Lsb;
}
FM_PRIMARY_REG_ENTRY;



typedef struct
{
	int IsAvailable;
	unsigned short RegStartAddr;
	unsigned char Msb;
	unsigned char Lsb;
}
FM_REG_ENTRY;



//Read ONLY Register entry
typedef struct
{
	int RegBitName;
	unsigned char DebugSignalBus;
	unsigned short RegStartAddr;
	unsigned char Msb;
	unsigned char Lsb;
}
FM_PRIMARY_READ_ONLY_REG_ENTRY;



typedef struct
{
	int IsAvailable;
	unsigned char DebugSignalBus;
	unsigned short RegStartAddr;
	unsigned char Msb;
	unsigned char Lsb;
}
FM_READ_ONLY_REG_ENTRY;





// Register table dependence
enum FM_REG_BIT_NAME
{
	FM_ENABLE,
	FM_SEEK,
	FM_SEEK_UP,
	FM_BASS,
	FM_FORCE_MUTE,
	FM_RDS_ENABLE,
	FM_CHANNEL_SPACE,
	FM_CHIP_ID,
	FM_BAND,
	FM_BAND0_START,
	FM_CHAN,
	FM_TUNE,
	
	// Item terminator
	FM_REG_BIT_NAME_ITEM_TERMINATOR,
};



enum FM_READ_ONLY_REG_BIT_NAME
{
	FM_SNR_AVE,
	FM_FSM_STATE,
	FM_BB_GAIN_STE_R,
	FM_RF_GAIN_STE_R,
	FM_RF_FREQUENCY_CODE,
	FM_IS_HIGH_SIDE,
	FM_CFO,
	FM_DAGC_GAIN_USE,

	// Item terminator
	FM_READ_ONLY_REG_BIT_NAME_ITEM_TERMINATOR,
};



// Register table length definitions
#define FM_REG_TABLE_LEN_MAX				FM_REG_BIT_NAME_ITEM_TERMINATOR
#define FM_READ_ONLY_REG_TABLE_LEN_MAX	FM_READ_ONLY_REG_BIT_NAME_ITEM_TERMINATOR


#define FM_DEBUG_SEL 0x0067
#define FM_ADDR_DEBUG_0	0x00fc
#define FM_ADDR_DEBUG_1	0x00fd
#define FM_ADDR_DEBUG_2	0x00fe
#define FM_ADDR_DEBUG_3	0x00ff


#define FM_RFC_WRITE_FLAG	0
#define FM_RFC_READ_FLAG	1


#define YES 1
#define NO 0




enum FM_CHANNEL_SPACE
{
	FM_CHANNEL_SPACE_50kHz,
	FM_CHANNEL_SPACE_100kHz,
	FM_CHANNEL_SPACE_200kHz,
};


// 1: 76~91MHz 2:76~108MHz 3: 87~108MHz
enum FM_BAND
{
	FM_BAND_REG_DEFINE,
	FM_BAND_76_91MHz,
	FM_BAND_76_108MHz,
	FM_BAND_87_108MHz,
};

enum FM_SEEK_DIR
{
	FM_SEEK_DIR_DOWN,
	FM_SEEK_DIR_UP,
};

//lory 
enum RTL_SEEK_DIR
{
	RTL_SEEK_DIR_DOWN,
	RTL_SEEK_DIR_UP,
};




enum FM_SEEK
{
	FM_SEEK_DISABLE,
	FM_SEEK_ENABLE,
};


typedef struct FM_MODULE_TAG FM_MODULE;





typedef int
(*FM_FP_SET_REG_BYTES)(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	const unsigned char *pWritingBytes,
	unsigned long ByteNum
	);



typedef int
(*FM_FP_GET_REG_BYTES)(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char *pReadingBytes,
	unsigned long ByteNum
	);



typedef int
(*FM_FP_SET_REG_MASK_BITS)(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned long WritingValue
	);



typedef int
(*FM_FP_GET_REG_MASK_BITS)(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	);



typedef int
(*FM_FP_SET_REG_BITS)(
	FM_MODULE *pFm,
	int RegBitName,
	const unsigned long WritingValue
	);



typedef int
(*FM_FP_GET_REG_BITS)(
	FM_MODULE *pFm,
	int RegBitName,
	unsigned long *pReadingValue
	);



typedef int
(*FM_FP_GET_READ_ONLY_REG_MASK_BITS)(
	FM_MODULE *pFm,
	unsigned char DebugSignalBus,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	);



typedef int
(*FM_FP_GET_READ_ONLY_REG_BITS)(
	FM_MODULE *pFm,
	int RegBitName,
	unsigned long *pReadingValue
	);



typedef int
(*FM_FP_SET_RFC_REG_2_BYTES)(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	const unsigned long WritingValue
	);



typedef int
(*FM_FP_GET_RFC_REG_2_BYTES)(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned long *pReadingValue
	);



typedef int
(*FM_FP_SET_RFC_REG_MASK_BITS)(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned long WritingValue
	);



typedef int
(*FM_FP_GET_RFC_REG_MASK_BITS)(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	);





typedef void
(*FM_FP_GET_FM_TYPE)(
	FM_MODULE *pFm,
	int *pFmType
	);



typedef void
(*FM_FP_GET_DEVICE_ADDR)(
	FM_MODULE *pFm,
	unsigned char *pDeviceAddr
	);







///FM module structure
struct FM_MODULE_TAG
{
	// Private variables
	int           FmType;
	unsigned char DeviceAddr;


	BASE_INTERFACE_MODULE *pBaseInterface;


	//register table
	FM_REG_ENTRY RegTable[FM_REG_TABLE_LEN_MAX];
	FM_READ_ONLY_REG_ENTRY ReadOnlyRegTable[FM_READ_ONLY_REG_TABLE_LEN_MAX];

	//I2C function pointers
	FM_FP_SET_REG_MASK_BITS        SetRegMaskBits;
	FM_FP_GET_REG_MASK_BITS        GetRegMaskBits;
	FM_FP_SET_REG_BITS             SetRegBits;
	FM_FP_GET_REG_BITS             GetRegBits;
	
	FM_FP_GET_READ_ONLY_REG_MASK_BITS        GetReadOnlyRegMaskBits;	
	FM_FP_GET_READ_ONLY_REG_BITS             GetReadOnlyRegBits;

	FM_FP_SET_RFC_REG_2_BYTES		SetRfcReg2Bytes;
	FM_FP_GET_RFC_REG_2_BYTES		GetRfcReg2Bytes;
	
	FM_FP_SET_RFC_REG_MASK_BITS        SetRfcRegMaskBits;
	FM_FP_GET_RFC_REG_MASK_BITS        GetRfcRegMaskBits;

	// Demod manipulating function pointers
	FM_FP_GET_FM_TYPE        GetFmType;
	FM_FP_GET_DEVICE_ADDR       GetDeviceAddr;

};





int
fm_default_SetRegMaskBits(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned long WritingValue
	);



int
fm_default_GetRegMaskBits(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	);



int
fm_default_SetRegBits(
	FM_MODULE *pFm,
	int RegBitName,
	const unsigned long WritingValue
	);



int
fm_default_GetRegBits(
	FM_MODULE *pFm,
	int RegBitName,
	unsigned long *pReadingValue
	);



int
fm_default_GetReadOnlyRegMaskBits(
	FM_MODULE *pFm,
	unsigned char DebugSignalBus,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	);



int
fm_default_GetReadOnlyRegBits(
	FM_MODULE *pFm,
	int RegBitName,
	unsigned long *pReadingValue
	);



int
fm_default_SetRfcReg2Bytes(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	const unsigned long WritingValue
	);



int
fm_default_GetRfcReg2Bytes(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned long *pReadingValue
	);



int
fm_default_SetRfcRegMaskBits(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned long WritingValue
	);



int
fm_default_GetRfcRegMaskBits(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	);





void
fm_default_GetFmType(
	FM_MODULE *pFm,
	int *pFmType
	);



void
fm_default_GetDeviceAddr(
	FM_MODULE *pFm,
	unsigned char *pDeviceAddr
	);





#endif
