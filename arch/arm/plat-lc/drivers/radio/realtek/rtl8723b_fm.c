

//#include <linux/kernel.h>
#include "rtl8723b_fm.h"



void
BuildRtl8723bFmModule(
	FM_MODULE **ppFm,
	FM_MODULE *pFmModuleMemory,
	BASE_INTERFACE_MODULE *pBaseInterfaceModuleMemory,
	unsigned char DeviceAddr
	)
{
	FM_MODULE *pFm;



	// Set demod module pointer, 
	*ppFm = pFmModuleMemory;

	// Get demod module.
	pFm = *ppFm;

	// Set base interface module pointer.
	pFm->pBaseInterface = pBaseInterfaceModuleMemory;


	// Set demod I2C device address.
	pFm->DeviceAddr = DeviceAddr;


	// Initialize demod register table.
	rtl8723b_fm_InitRegTable(pFm);

	
	// I2C function pointers with default functions.
	pFm->SetRegMaskBits     = fm_default_SetRegMaskBits;
	pFm->GetRegMaskBits     = fm_default_GetRegMaskBits;
	pFm->SetRegBits         = fm_default_SetRegBits;
	pFm->GetRegBits         = fm_default_GetRegBits;
	pFm->GetReadOnlyRegBits         = fm_default_GetReadOnlyRegBits;
	pFm->GetReadOnlyRegMaskBits     = fm_default_GetReadOnlyRegMaskBits;

	pFm->SetRfcReg2Bytes = fm_default_SetRfcReg2Bytes;
	pFm->GetRfcReg2Bytes = fm_default_GetRfcReg2Bytes;
	pFm->SetRfcRegMaskBits     = fm_default_SetRfcRegMaskBits;
	pFm->GetRfcRegMaskBits     = fm_default_GetRfcRegMaskBits;
	
	pFm->GetFmType     = fm_default_GetFmType;
	pFm->GetDeviceAddr    = fm_default_GetDeviceAddr;

	return;
}





int
rtl8723b_fm_Initialize(
	FM_MODULE *pFm
	)
{
	// Initializing table entry only used in Initialize()
	typedef struct
	{
		unsigned short RegStartAddr;
		unsigned char WritingValue;
	}
	INIT_TABLE_ENTRY;

	typedef struct
	{
		unsigned char RegStartAddr;
		unsigned long WritingValue;
	}
	INIT_RFC_TABLE_ENTRY;

	static const INIT_TABLE_ENTRY InitRegTable[FM_INIT_REG_TABLE_LEN] =
	{
		// RegStartAddr,	WritingValue
		{0x0004,			0x23},
		{0x0007,			0x0},
		{0x0008,			0x83},
		{0x0009,			0xc0},
		{0x000a,			0x3},
		{0x000f,			0x5},
		{0x0010,			0x18},
		{0x0012,			0x7f},		
		{0x001d,			0x17},
		{0x0020,			0x20},
		{0x0022,			0xff},
		{0x0023,			0x1},
		{0x002b,			0x22},
		{0x002f,			0xc6},
		{0x0030,			0x00},
		{0x0035,			0x09},		
		{0x0036,			0xff},
		{0x0040,			0xc0},
		{0x004a,			0x0},
		{0x0056,			0x0c},		
		{0x0057,			0x20},
		{0x005a,			0x0c},
		{0x005b,			0x58},
		{0x005c,			0xc},
		{0x0060,			0x10},
		{0x0062,			0x2},
		{0x006b,			0x00},
		{0x006f,			0x19},
		{0x0070,			0x19},
		{0x0071,			0x1f},
		{0x0072,			0x1f},
		{0x0073,			0xf2},
		{0x0074,			0x1f},
		{0x0075,			0x0},
		{0x008b,			0x4a},
		{0x008e,			0x30},
		{0x008f,			0x4a},
		{0x0092,			0xc},
		{0x0093,			0x4a},
		{0x0096,			0xc},
		{0x0097,			0x4a},
		{0x009a,			0x8},
		{0x009b,			0xca},
		{0x009e,			0x8},
		{0x009f,			0x95},
		{0x00a0,			0x29},
		{0x00a1,			0x22},
		{0x00a2,			0x6},
		{0x00a3,			0x34},
		{0x00a5,			0x41},
		{0x00a6,			0x6},
		{0x00a7,			0x14},
		{0x00aa,			0x12},
		{0x00ab,			0x12},
		{0x00ac,			0x12},
		{0x00ad,			0x12},
		{0x00ae,			0x12},
		{0x00af,			0x12},
		{0x00b0,			0x12},
		{0x00b1,			0x17},
		{0x00b2,			0x17},
		{0x00b3,			0x1d},
		{0x00b4,			0x1d},
		{0x00bf,			0x0},
		{0x00c5,			0x1},	
		//{0x0001,			0x1},
		//{0x0002,			0x0},		
	};


	static const INIT_RFC_TABLE_ENTRY  RfcInitRegTable[RFC_INIT_TABLE_LEN]=
	{
		{0x7,		0x21AB},
		{0x8,		0xCF00},
		{0x9,		0x0622},
		{0xa,		0xCFF4},  //{0xa,		0x4034},
		{0xb,		0xFFC0},
		{0xc,		0x8000},
		{0x18,		0x555E},
		{0x1d,		0xACCD},
		{0x20,		0x0800},
		{0x21,		0xCD53},
		{0x24,		0xFD00},
		{0x26,		0x6D7E},
		{0x30,		0x8840},
		{0x32,		0x4000},
		{0x33,		0xCE69},
		{0x38,		0x6018},
		{0x50,		0x0802},
		{0x51,		0x0020},
	};

	int i;

	unsigned short RegStartAddr;
	unsigned char WritingValue;
	
	unsigned char RfcRegStartAddr;
	unsigned long RfcWritingValue;	
	
	unsigned long GainVal;
	unsigned long GainSum;


	if(pFm->SetRegMaskBits(pFm, 0x0061, 2, 0, 4) != FUNCTION_SUCCESS)
		goto error_status;

	//if(pFm->SetRegMaskBits(pFm, 0x005f, 0, 0, 0) != FUNCTION_SUCCESS)
	//	goto error_status;

	//if(pFm->SetRegMaskBits(pFm, 0x010C, 7, 0, 0x75) != FUNCTION_SUCCESS)
	//	goto error_status;	

	//pFm->pBaseInterface->WaitMs(pFm->pBaseInterface, 50);

	for(i = 0; i < FM_INIT_REG_TABLE_LEN; i++)
	{
		RegStartAddr = InitRegTable[i].RegStartAddr;
		WritingValue = InitRegTable[i].WritingValue;

		if(pFm->SetRegMaskBits(pFm, RegStartAddr, 7, 0, WritingValue) != FUNCTION_SUCCESS)
			goto error_status;
	}

	pFm->pBaseInterface->WaitMs(pFm->pBaseInterface, 10);

	for(i = 0; i < RFC_INIT_TABLE_LEN; i++)
	{
		RfcRegStartAddr = RfcInitRegTable[i].RegStartAddr;
		RfcWritingValue = RfcInitRegTable[i].WritingValue;

		if(pFm->SetRfcReg2Bytes(pFm, RfcRegStartAddr, RfcWritingValue) != FUNCTION_SUCCESS)
			goto error_status;			

	}
	
	//set lna mixer gain
	if(pFm->SetRfcRegMaskBits(pFm, 0x0f, 14, 14, 1)!= FUNCTION_SUCCESS)
		goto error_status;

	if(pFm->SetRfcRegMaskBits(pFm, 0x0e, 14, 14, 1)!= FUNCTION_SUCCESS)
		goto error_status;

	for(i = 0; i < 8; i++)
	{
		if(pFm->SetRfcRegMaskBits(pFm, 0x13, 10, 8, i)!= FUNCTION_SUCCESS)
			goto error_status;

		if(i==7)
		{
			if(pFm->SetRfcRegMaskBits(pFm, 0x14, 3, 0, 0x0f)!= FUNCTION_SUCCESS)
				goto error_status;			

			if(pFm->SetRfcReg2Bytes(pFm, 0x15, 0x3801) != FUNCTION_SUCCESS)
				goto error_status;			

		}
		else if (i==6)
		{
			if(pFm->SetRfcRegMaskBits(pFm, 0x14, 3, 0, 0x0e)!= FUNCTION_SUCCESS)
				goto error_status;			

			if(pFm->SetRfcReg2Bytes(pFm, 0x15, 0x00c1) != FUNCTION_SUCCESS)
				goto error_status;
		}
		else if (i==5)
		{
			if(pFm->SetRfcRegMaskBits(pFm, 0x14, 3, 0, 0x08)!= FUNCTION_SUCCESS)
				goto error_status;			

			if(pFm->SetRfcReg2Bytes(pFm, 0x15, 0x0061) != FUNCTION_SUCCESS)
				goto error_status;
		}
		else if (i==4)
		{
			if(pFm->SetRfcRegMaskBits(pFm, 0x14, 3, 0, 0x04)!= FUNCTION_SUCCESS)
				goto error_status;			

			if(pFm->SetRfcReg2Bytes(pFm, 0x15, 0x0fe1) != FUNCTION_SUCCESS)
				goto error_status;
		}
		else
		{
			if(pFm->SetRfcRegMaskBits(pFm, 0x14, 3, 0, 0x01)!= FUNCTION_SUCCESS)
				goto error_status;			

			if(pFm->SetRfcReg2Bytes(pFm, 0x15, 0x0fe1) != FUNCTION_SUCCESS)
				goto error_status;
		}

	}

	if(pFm->SetRfcRegMaskBits(pFm, 0x0f, 14, 14, 0)!= FUNCTION_SUCCESS)
		goto error_status;



	if(pFm->SetRfcRegMaskBits(pFm, 0x0f, 12, 12, 1)!= FUNCTION_SUCCESS)
		goto error_status;

	if(pFm->SetRfcRegMaskBits(pFm, 0x0e, 13, 13, 1)!= FUNCTION_SUCCESS)
		goto error_status;

	for(i = 0; i < 32; i++)
	{
		if(pFm->SetRfcRegMaskBits(pFm, 0x13, 15, 11, i)!= FUNCTION_SUCCESS)
			goto error_status;

		GainSum = i*2;
		if(GainSum<20)
			GainVal = (0<<6) + (((GainSum-0)/2)<<2) + 0;
		else if (GainSum>=20 && GainSum<26)
			GainVal = (1<<6) + (((GainSum-6)/2)<<2) + 0;
		else if (GainSum>=26 && GainSum<32)
			GainVal = (2<<6) + (((GainSum-12)/2)<<2) + 0;
		else if (GainSum>=32 && GainSum<38)
			GainVal = (3<<6) + (((GainSum-18)/2)<<2) + 0;
		else if (GainSum>=38 && GainSum<44)
			GainVal = (3<<6) + (((GainSum-18-6)/2)<<2) + (1<<0);
		else if (GainSum>=44 && GainSum<50)
			GainVal = (3<<6) + (((GainSum-18-12)/2)<<2) + (2<<0);
		else if (GainSum>=50 && GainSum<=54)
			GainVal = (3<<6) + (((GainSum-18-18)/2)<<2) + (3<<0);
		else
			GainVal = (3<<6) + (9<<2) + (3<<0);

		if(pFm->SetRfcRegMaskBits(pFm, 0x14, 15, 8, GainVal)!= FUNCTION_SUCCESS)
			goto error_status;
	}

	if(pFm->SetRfcRegMaskBits(pFm, 0x0f, 12, 12, 0)!= FUNCTION_SUCCESS)
		goto error_status;


	if(pFm->SetRegMaskBits(pFm, 0x0001, 7, 0, 0x1) != FUNCTION_SUCCESS)
		goto error_status;

	if(pFm->SetRegMaskBits(pFm, 0x0002, 7, 0, 0x0) != FUNCTION_SUCCESS)
		goto error_status;

	//disable audio agc
	if(pFm->SetRegMaskBits(pFm, 0x0021, 4, 4, 0)!= FUNCTION_SUCCESS)
		goto error_status;

	pFm->pBaseInterface->WaitMs(pFm->pBaseInterface, 50);

	return FUNCTION_SUCCESS;

error_status:
	return FUNCTION_ERROR;

}

int
rtl8723b_fm_RDSResample(
	FM_MODULE *pFm
	)
{
	BASE_INTERFACE_MODULE *pBaseInterface;
	long ALOTarget, Out;
	MPI MPI_ALOTarget, MPIConst;
	MPI MPIDivM, MPIDivL;
	MPI MPI_2ALOTarget;
	MPI MPIRemder;
	MPI MPIOutDend, MPIOutDsor;
	unsigned long RfFreqCode;


	pBaseInterface = pFm->pBaseInterface;

	pBaseInterface->WaitMs(pBaseInterface, 200);
	//
	//***rds_resample_compen***//
	//freq_code = show_rf_freq_code;
	//LO_target = 59.775+freq_code*0.05;
	//DIV_M = round(3600/LO_target/4);
	//IQGEN = 4;
	//VCO_target = LO_target * DIV_M * IQGEN;
	//DIV_L = round(VCO_target/2/40);
	//BB_CLK = VCO_target/DIV_L/2;
	//
	//out = 9.7656/2.375*BB_CLK/40;
	//
	//out = floor(out*2^11);
	//
	//==================>>>>>>
	//LO_target = 59.775+freq_code*0.05
	//A = 1000*LO = 59775+freq_code*50
	//DIV_M = floor((18*10^5+A)/2A)
	//VCO_target = (A* DIV_M/250)
	//DIV_L =( A * DIV_M + 10000 ) / 20000
	//out = (97656 * A * DIV_M * 2^11 ) / (23750 * 20000 * DIV_L)

	if(pFm->GetReadOnlyRegMaskBits(pFm, 11, FM_ADDR_DEBUG_0, 10, 0, &RfFreqCode)) goto error_status;

	ALOTarget = 59775 + RfFreqCode * 50;

	MpiSetValue(&MPI_ALOTarget, ALOTarget);
	MpiSetValue(&MPIConst, 1800000);
	MpiAdd(&MPIDivM, MPIConst, MPI_ALOTarget);
	MpiAdd(&MPI_2ALOTarget, MPI_ALOTarget, MPI_ALOTarget);
	MpiDiv(&MPIDivM, &MPIRemder, MPIDivM, MPI_2ALOTarget);

	MpiMul(&MPIDivL, MPI_ALOTarget, MPIDivM);
	MpiSetValue(&MPIConst, 10000);
	MpiAdd(&MPIDivL, MPIDivL, MPIConst);
	MpiSetValue(&MPIConst, 20000);
	MpiDiv(&MPIDivL, &MPIRemder, MPIDivL, MPIConst);

	MpiSetValue(&MPIConst, 97656);
	MpiMul(&MPIOutDend, MPIConst, MPI_ALOTarget);
	MpiMul(&MPIOutDend, MPIOutDend, MPIDivM);
	MpiLeftShift(&MPIOutDend, MPIOutDend, 11);
	MpiSetValue(&MPIConst, 475000000);
	MpiMul(&MPIOutDsor, MPIConst, MPIDivL);
	MpiDiv(&MPIOutDend, &MPIRemder, MPIOutDend, MPIOutDsor);

	MpiGetValue(MPIOutDend, &Out);

	if(pFm->SetRegMaskBits(pFm, 0x00b8, 13, 0, Out)) goto error_status;

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;

}

int
rtl8723b_fm_BefSetTune(
	FM_MODULE *pFm,
	int RfFreqHz
	)
{

	BASE_INTERFACE_MODULE *pBaseInterface;
	unsigned long	Space;
	unsigned long	ChannSpace;
	unsigned long	Band;
	unsigned long	ChInit;
	unsigned long	InitFreqCode;
	unsigned long	ChanVal;


	ChInit = 0;
	ChannSpace = 0;

	pBaseInterface = pFm->pBaseInterface;

	if(pFm->SetRegBits(pFm, FM_TUNE, 0) != FUNCTION_SUCCESS)
		goto error_status;
	
	if(pFm->GetRegBits(pFm, FM_CHANNEL_SPACE, &Space) != FUNCTION_SUCCESS)
		goto error_status;
	
	switch(Space)
	{
		case FM_CHANNEL_SPACE_50kHz:	ChannSpace = 20; 	break;
		case FM_CHANNEL_SPACE_100kHz:	ChannSpace = 10;		break;	
		case FM_CHANNEL_SPACE_200kHz:	ChannSpace = 5;		break;
	}


	if(pFm->GetRegBits(pFm, FM_BAND, &Band) != FUNCTION_SUCCESS)
		goto error_status;

	Band = FM_BAND_87_108MHz;
	switch(Band)
	{
		case FM_BAND_REG_DEFINE:
			if(pFm->GetRegBits(pFm, FM_BAND0_START, &InitFreqCode) != FUNCTION_SUCCESS)
				goto error_status;
			ChInit = (InitFreqCode* ChannSpace) *50000 + 60 * ChannSpace * 1000000;
			break;
	
		case FM_BAND_76_91MHz:	
			ChInit = 76 * ChannSpace* 1000000;		break;
			
		case FM_BAND_76_108MHz:
			ChInit = 76 * ChannSpace * 1000000;		break;
			
		case FM_BAND_87_108MHz:
		default:
			ChInit = 87 * ChannSpace * 1000000;		break;
	}

	ChanVal = RfFreqHz * ChannSpace - ChInit;
	ChanVal = (ChanVal + 500000)/1000000;

	if(pFm->SetRegBits(pFm, FM_CHAN, ChanVal) != FUNCTION_SUCCESS)
		goto error_status;

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}

int
rtl8723b_fm_SetTune_Hz(
	FM_MODULE *pFm,
	int RfFreqHz
	)
{

	BASE_INTERFACE_MODULE *pBaseInterface;
	unsigned long bRdsEn;


	pBaseInterface = pFm->pBaseInterface;

	if(rtl8723b_fm_BefSetTune(pFm, RfFreqHz)!= FUNCTION_SUCCESS)
		goto error_status;

	if(pFm->SetRegBits(pFm, FM_TUNE, 1) != FUNCTION_SUCCESS)
		goto error_status;

	pBaseInterface->WaitMs(pBaseInterface, 50);

	if(pFm->GetRegMaskBits(pFm, 0x00bf, 0, 0, &bRdsEn) != FUNCTION_SUCCESS)
		goto error_status;

	if(bRdsEn == 1)
	{
		rtl8723b_fm_RDSResample(pFm);
	}
	
	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}

int
rtl8723b_fm_Set_Channel_Space(
	FM_MODULE *pFm,
	int ChannelSpace
	)
{

	if(pFm->SetRegBits(pFm, FM_CHANNEL_SPACE, ChannelSpace) != FUNCTION_SUCCESS)
		goto error_status;	

	
	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}

int
rtl8723b_fm_Get_Chipid(
	FM_MODULE *pFm,
	unsigned long *id
	)
{
	if(pFm->GetRegBits(pFm, FM_CHIP_ID, id) != FUNCTION_SUCCESS)
		return FUNCTION_ERROR;
	
	return FUNCTION_SUCCESS;
}

int
rtl8723b_fm_Seek(
	FM_MODULE *pFm,
	int bSeekEnable
	)
{

	BASE_INTERFACE_MODULE *pBaseInterface;



	// Get base interface.
	pBaseInterface = pFm->pBaseInterface;


	if(pFm->SetRegBits(pFm, FM_SEEK, 0) != FUNCTION_SUCCESS)
		goto error_status;

	// Wait time in millisecond.
	//pBaseInterface->WaitMs(pBaseInterface, 50);
	
	if(pFm->SetRegBits(pFm, FM_SEEK, bSeekEnable) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_SetSeekDirection(
	FM_MODULE *pFm,
	int bSeekUp
	)
{
	
	if(pFm->SetRegBits(pFm, FM_SEEK_UP, bSeekUp) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_SetBassBoost(
	FM_MODULE *pFm,
	int bBassEnable
	)
{
	
	if(pFm->SetRegBits(pFm, FM_BASS, bBassEnable) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_SetMute(
	FM_MODULE *pFm,
	int bForceMute
	)
{
	
	if(pFm->SetRegBits(pFm, FM_FORCE_MUTE, bForceMute) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_SetUserDefineBand(
	FM_MODULE *pFm,
	int StartHz,
	int EndHz
	)
{
	unsigned long StartHzbuf;
	unsigned long EndHzbuf;

	StartHzbuf = (StartHz*20 -1200*1000000) / 1000000;
	EndHzbuf = (EndHz*20 -StartHz*20) / 1000000;

	if(pFm->SetRegBits(pFm, FM_BAND, FM_BAND_REG_DEFINE) != FUNCTION_SUCCESS)
		goto error_status;

	if(pFm->SetRegMaskBits(pFm, 0x000b, 10, 0, StartHzbuf) != FUNCTION_SUCCESS)
		goto error_status;

	if(pFm->SetRegMaskBits(pFm, 0x0009, 10, 0, EndHzbuf) != FUNCTION_SUCCESS)
		goto error_status;	


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;


}





int
rtl8723b_fm_SetBand(
	FM_MODULE *pFm,
	unsigned long FmBandType
	)
{
	if(FmBandType == FM_BAND_REG_DEFINE) 
		return FUNCTION_ERROR;
	
	if(pFm->SetRegBits(pFm, FM_BAND, FmBandType) != FUNCTION_SUCCESS)
		goto error_status;

	{
		unsigned long Band;
		pFm->GetRegBits(pFm, FM_BAND, &Band);
		//dbg_info("%s, FmBandType:%d, %d\n", __func__, FmBandType, Band);
	}
	
	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_SetRDS(
	FM_MODULE *pFm,
	int bRDSEnable
	)
{

	if(pFm->SetRegBits(pFm, FM_RDS_ENABLE, bRDSEnable) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_GetFrequencyHz(
	FM_MODULE *pFm,
	int *pFrequencyHz
	)
{

	unsigned long RfFreqCode;
	unsigned long bIsHiSide;
	unsigned long Band;
	unsigned long InitFreqCode;
	unsigned long ChInit;


	RfFreqCode = 0;
	ChInit = 0;
	
	if(pFm->GetReadOnlyRegBits(pFm, FM_RF_FREQUENCY_CODE, &RfFreqCode) != FUNCTION_SUCCESS)
		goto error_status;	

	if(pFm->GetReadOnlyRegBits(pFm, FM_IS_HIGH_SIDE, &bIsHiSide) != FUNCTION_SUCCESS)
		goto error_status;	

	if(pFm->GetRegBits(pFm, FM_BAND, &Band) != FUNCTION_SUCCESS)
		goto error_status;	

	switch(Band)
	{
		case 0:
			if(pFm->GetRegBits(pFm, FM_BAND0_START, &InitFreqCode) != FUNCTION_SUCCESS)
				goto error_status;
			ChInit = InitFreqCode*50000 + 60*1000000;
			break;

		case 1:
			ChInit = 76*1000000;
			InitFreqCode = 320;
			break;	
		case 2:
			ChInit = 76*1000000;
			InitFreqCode = 320;
			break;

		case 3:
			ChInit = 87*1000000;
			InitFreqCode = 540;
			break;
	}

	if (bIsHiSide == 0)
		*pFrequencyHz = ChInit + (RfFreqCode - InitFreqCode)*50000;
	else
		*pFrequencyHz = ChInit + (RfFreqCode - InitFreqCode)*50000 -450000;

	
	return FUNCTION_SUCCESS;


error_status:
	//dbg_info("<-----------Error:%s()\n", __func__);
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_GetSNR(
	FM_MODULE *pFm,
	long *pSnrDbNum,
	long *pSnrDbDen
	)
{

	unsigned long SNRBinary;



	if(pFm->GetReadOnlyRegBits(pFm, FM_SNR_AVE, &SNRBinary) != FUNCTION_SUCCESS)
		goto error_status;	

	*pSnrDbNum = BinToSignedInt(SNRBinary, RTL8723B_FM_SNR_AVE_BIT_NUM);	

	*pSnrDbDen = 256;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_GetCFO(
	FM_MODULE *pFm,
	long *pCfo
	)
{

	unsigned long ulCfo;
	long	lCfo;
	MPI MpiCfo, MpiConst, MpiOut;



	if(pFm->GetReadOnlyRegBits(pFm, FM_CFO, &ulCfo) != FUNCTION_SUCCESS)
		goto error_status;	

	lCfo = BinToSignedInt(ulCfo, RTL8723B_FM_CFO_BIT_NUM);	

	MpiSetValue(&MpiCfo, lCfo);
	MpiSetValue(&MpiConst, 312500);
	MpiMul(&MpiOut, MpiCfo, MpiConst);		
	MpiRightShift(&MpiOut, MpiOut, 21);
	MpiGetValue(MpiOut, pCfo);

	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_GetRSSI(
	FM_MODULE *pFm,
	int *pRssi
	)
{

	unsigned long RfGainSte;
	unsigned long BbGainSte;
	unsigned long	RfGain;
	unsigned long	DagcGainUse;
	MPI MpiDGU, MpiConst, MpiOut, MpiQ, MpiRed;
	unsigned long ulDGC;
	int TargetPower;


	if(pFm->GetReadOnlyRegBits(pFm, FM_RF_GAIN_STE_R, &RfGainSte) != FUNCTION_SUCCESS)
		goto error_status;	

	if(pFm->GetReadOnlyRegBits(pFm, FM_BB_GAIN_STE_R, &BbGainSte) != FUNCTION_SUCCESS)
		goto error_status;		

	if(pFm->GetReadOnlyRegBits(pFm, FM_DAGC_GAIN_USE, &DagcGainUse) != FUNCTION_SUCCESS)
		goto error_status;	

	MpiSetValue(&MpiDGU, DagcGainUse);
	MpiSetValue(&MpiConst, 1000);
	MpiMul(&MpiOut, MpiDGU, MpiConst);
	MpiLog2(&MpiOut, MpiOut, 20);
	MpiSetValue(&MpiConst, 19568292);	
	MpiSub(&MpiOut, MpiOut, MpiConst);
	MpiSetValue(&MpiConst, 3401);
	MpiDiv(&MpiQ, &MpiRed, MpiOut, MpiConst);
	MpiGetValue(MpiQ, &ulDGC);
	
	switch(RfGainSte)
	{
	    case 0:
	    default:
	        RfGain = 8;	break;
	    case 1:
	        RfGain = 8;	break;
	    case 2:
	        RfGain = 8;	break;
	    case 3:
	        RfGain = 8;	break;
	    case 4:
	        RfGain = 17;	break;
	    case 5:
	        RfGain = 24;	break;
	    case 6:
	        RfGain = 30;	break;
	    case 7:
	        RfGain = 38;	break;
	}

	TargetPower = 5;
	*pRssi =(int) (( TargetPower - RfGain - BbGainSte*2 )*1024 - 20*ulDGC)/1024;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





int
rtl8723b_fm_GetFSM(
	FM_MODULE *pFm,
	unsigned long *pFsmState
	)
{

	if(pFm->GetReadOnlyRegBits(pFm, FM_FSM_STATE, pFsmState) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}










void
rtl8723b_fm_InitRegTable(
	FM_MODULE *pFm
	)
{
	static const FM_PRIMARY_REG_ENTRY  PrimaryRegTable[FM_REG_TABLE_LEN_MAX] =
	{
		// RegBitName,						RegStartAddr,	MSB,	LSB
		{FM_ENABLE,							0x0001,		0,		0},
		{FM_SEEK,							0x0007,		0,		0},
		{FM_SEEK_UP,						0x0008,		0,		0},
		{FM_BASS,							0x0008,		7,		7},
		{FM_FORCE_MUTE,						0x0008,		5,		5},
		{FM_RDS_ENABLE,						0x00bf,		0,		0},
		{FM_CHANNEL_SPACE,					0x0008,		2,		1},
		{FM_CHIP_ID,							0x010c,		7,		4},
		{FM_BAND,							0x0003,		1,		0},
		{FM_BAND0_START,					0x000b,		11,		0},
		{FM_CHAN,							0x0004,		11,		0},
		{FM_TUNE,							0x0006,		0,		0},

	};


	static const FM_PRIMARY_READ_ONLY_REG_ENTRY  PrimaryReadOnlyRegTable[FM_READ_ONLY_REG_TABLE_LEN_MAX] =
	{
		// RegBitName,			DebugSignalBus	RegStartAddr,			MSB,	LSB
		{FM_SNR_AVE,			9,				FM_ADDR_DEBUG_0,	15,		0},
		{FM_FSM_STATE,			11,				FM_ADDR_DEBUG_3,	2,		0},
		{FM_RF_GAIN_STE_R,		21,				FM_ADDR_DEBUG_0,	2,		0},
		{FM_BB_GAIN_STE_R,		21,				FM_ADDR_DEBUG_0,	7,		3},
		{FM_RF_FREQUENCY_CODE,	11,				FM_ADDR_DEBUG_0,	11,		0},
		{FM_IS_HIGH_SIDE,		10,				FM_ADDR_DEBUG_1,	2,		2},
		{FM_CFO,				1,				FM_ADDR_DEBUG_1,	21,		4},
		{FM_DAGC_GAIN_USE,		2,				FM_ADDR_DEBUG_0,	16,		0},
		
	};


	int i;
	int RegBitName;



	for(i = 0; i < FM_REG_TABLE_LEN_MAX; i++)
		pFm->RegTable[i].IsAvailable  = NO;

	for(i = 0; i < FM_REG_TABLE_LEN_MAX; i++)
	{
		RegBitName =  PrimaryRegTable[i].RegBitName;

		pFm->RegTable[RegBitName].IsAvailable  = YES;
		pFm->RegTable[RegBitName].RegStartAddr =  PrimaryRegTable[i].RegStartAddr;
		pFm->RegTable[RegBitName].Msb          =  PrimaryRegTable[i].Msb;
		pFm->RegTable[RegBitName].Lsb          =  PrimaryRegTable[i].Lsb;
	}

	for(i = 0; i < FM_READ_ONLY_REG_TABLE_LEN_MAX; i++)
		pFm->ReadOnlyRegTable[i].IsAvailable  = NO;

	for(i = 0; i < FM_READ_ONLY_REG_TABLE_LEN_MAX; i++)
	{
		RegBitName =  PrimaryReadOnlyRegTable[i].RegBitName;

		pFm->ReadOnlyRegTable[RegBitName].IsAvailable  = YES;
		pFm->ReadOnlyRegTable[RegBitName].DebugSignalBus =  PrimaryReadOnlyRegTable[i].DebugSignalBus;		
		pFm->ReadOnlyRegTable[RegBitName].RegStartAddr =  PrimaryReadOnlyRegTable[i].RegStartAddr;
		pFm->ReadOnlyRegTable[RegBitName].Msb          =  PrimaryReadOnlyRegTable[i].Msb;
		pFm->ReadOnlyRegTable[RegBitName].Lsb          =  PrimaryReadOnlyRegTable[i].Lsb;
	}
	

	return;
}










