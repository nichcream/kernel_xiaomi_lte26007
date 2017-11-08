

#include <linux/mutex.h>
#include "fm_base.h"



extern struct mutex mutex;


int
fm_default_SetRegBytes(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	const unsigned char *pWritingBytes,
	unsigned long ByteNum
	)
{
	BASE_INTERFACE_MODULE *pBaseInterface;

	unsigned long i, j;

	unsigned char DeviceAddr;
	unsigned char WritingBuffer[I2C_BUFFER_LEN];
	unsigned long WritingByteNum, WritingByteNumMax, WritingByteNumRem;
	unsigned short RegWritingAddr;



	// Get base interface.
	pBaseInterface = pFm->pBaseInterface;


	// Get fm I2C device address.
	pFm->GetDeviceAddr(pFm, &DeviceAddr);


	// Calculate maximum writing byte number.
	WritingByteNumMax = pBaseInterface->I2cWritingByteNumMax - LEN_2_BYTE;


	// Set fm register bytes with writing bytes.
	// Note: Set fm register bytes considering maximum writing byte number.
	for(i = 0; i < ByteNum; i += WritingByteNumMax)
	{
		// Set register writing address.
		RegWritingAddr = (unsigned short)(RegStartAddr + i);

		// Calculate remainder writing byte number.
		WritingByteNumRem = ByteNum - i;

		// Determine writing byte number.
		WritingByteNum = (WritingByteNumRem > WritingByteNumMax) ? WritingByteNumMax : WritingByteNumRem;


		// Set writing buffer.
		// Note: The I2C format of fm register byte setting is as follows:
		//       start_bit + (DeviceAddr | writing_bit) + RegWritingAddrMsb + RegWritingAddrLsb +
		//       writing_bytes (WritingByteNum bytes) + stop_bit
		WritingBuffer[0] = (RegWritingAddr >> BYTE_SHIFT) & BYTE_MASK;
		WritingBuffer[1] = RegWritingAddr & BYTE_MASK;

		for(j = 0; j < WritingByteNum; j++)
			WritingBuffer[LEN_2_BYTE + j] = pWritingBytes[i + j];


		// Set fm register bytes with writing buffer.
		if(pBaseInterface->I2cWrite(pBaseInterface, DeviceAddr, WritingBuffer, WritingByteNum + LEN_2_BYTE) !=
			FUNCTION_SUCCESS)
			goto error_status;
	}


	return FUNCTION_SUCCESS;


error_status:


	return FUNCTION_ERROR;
}



int
fm_default_GetRegBytes(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char *pReadingBytes,
	unsigned long ByteNum
	)
{
	BASE_INTERFACE_MODULE *pBaseInterface;

	unsigned long i;
	unsigned char DeviceAddr;
	unsigned long ReadingByteNum, ReadingByteNumMax, ReadingByteNumRem;
	unsigned short RegReadingAddr;
	unsigned char WritingBuffer[LEN_2_BYTE];



	// Get base interface.
	pBaseInterface = pFm->pBaseInterface;


	// Get fm I2C device address.
	pFm->GetDeviceAddr(pFm, &DeviceAddr);


	// Calculate maximum reading byte number.
	ReadingByteNumMax = pBaseInterface->I2cReadingByteNumMax;


	// Get fm register bytes.
	// Note: Get fm register bytes considering maximum reading byte number.
	for(i = 0; i < ByteNum; i += ReadingByteNumMax)
	{
		// Set register reading address.
		RegReadingAddr = (unsigned short)(RegStartAddr + i);

		// Calculate remainder reading byte number.
		ReadingByteNumRem = ByteNum - i;

		// Determine reading byte number.
		ReadingByteNum = (ReadingByteNumRem > ReadingByteNumMax) ? ReadingByteNumMax : ReadingByteNumRem;


		// Set fm register reading address.
		// Note: The I2C format of fm register reading address setting is as follows:
		//       start_bit + (DeviceAddr | writing_bit) + RegReadingAddrMsb + RegReadingAddrLsb + stop_bit
		WritingBuffer[0] = (RegReadingAddr >> BYTE_SHIFT) & BYTE_MASK;
		WritingBuffer[1] = RegReadingAddr & BYTE_MASK;

		if(pBaseInterface->I2cWrite(pBaseInterface, DeviceAddr, WritingBuffer, LEN_2_BYTE) != FUNCTION_SUCCESS)
			goto error_status;

		// Get fm register bytes.
		// Note: The I2C format of fm register byte getting is as follows:
		//       start_bit + (DeviceAddr | reading_bit) + reading_bytes (ReadingByteNum bytes) + stop_bit
		if(pBaseInterface->I2cRead(pBaseInterface, DeviceAddr, &pReadingBytes[i], ReadingByteNum) != FUNCTION_SUCCESS)
			goto error_status;
	}


	return FUNCTION_SUCCESS;


error_status:


	return FUNCTION_ERROR;
}



int
fm_default_SetRegMaskBits(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned long WritingValue
	)
{
	int i;

	unsigned char ReadingBytes[LEN_4_BYTE];
	unsigned char WritingBytes[LEN_4_BYTE];

	unsigned char ByteNum;
	unsigned long Mask;
	unsigned char Shift;

	unsigned long Value;


	// Calculate writing byte number according to MSB.
	ByteNum = Msb / BYTE_BIT_NUM + LEN_1_BYTE;


	// Generate mask and shift according to MSB and LSB.
	Mask = 0;

	for(i = Lsb; i < (unsigned char)(Msb + 1); i++)
		Mask |= 0x1 << i;

	Shift = Lsb;

	mutex_lock(&mutex);

	// Get fm register bytes according to register start adddress and byte number.
	if(fm_default_GetRegBytes(pFm, RegStartAddr, ReadingBytes, ByteNum) != FUNCTION_SUCCESS)
		goto error_status;


	// Combine reading bytes into an unsigned integer value.
	// Note: Put lower address byte on value LSB.
	//       Put upper address byte on value MSB.
	Value = 0;

	for(i = 0; i < ByteNum; i++)
		Value |= (unsigned long)ReadingBytes[i] << (BYTE_SHIFT * i);


	// Reserve unsigned integer value unmask bit with mask and inlay writing value into it.
	Value &= ~Mask;
	Value |= (WritingValue << Shift) & Mask;


	// Separate unsigned integer value into writing bytes.
	// Note: Pick up lower address byte from value LSB.
	//       Pick up upper address byte from value MSB.
	for(i = 0; i < ByteNum; i++)
		WritingBytes[i] = (unsigned char)((Value >> (BYTE_SHIFT * i)) & BYTE_MASK);


	// Write fm register bytes with writing bytes.
	if(fm_default_SetRegBytes(pFm, RegStartAddr, WritingBytes, ByteNum) != FUNCTION_SUCCESS)
		goto error_status;

	mutex_unlock(&mutex);
	

	return FUNCTION_SUCCESS;


error_status:

	mutex_unlock(&mutex);


	return FUNCTION_ERROR;
}



int
fm_default_GetRegMaskBits(
	FM_MODULE *pFm,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	)
{
	int i;

	unsigned char ReadingBytes[LEN_4_BYTE];

	unsigned char ByteNum;
	unsigned long Mask;
	unsigned char Shift;

	unsigned long Value;


	// Calculate writing byte number according to MSB.
	ByteNum = Msb / BYTE_BIT_NUM + LEN_1_BYTE;


	// Generate mask and shift according to MSB and LSB.
	Mask = 0;

	for(i = Lsb; i < (unsigned char)(Msb + 1); i++)
		Mask |= 0x1 << i;

	Shift = Lsb;

	mutex_lock(&mutex);

	// Get fm register bytes according to register start adddress and byte number.
	if(fm_default_GetRegBytes(pFm, RegStartAddr, ReadingBytes, ByteNum) != FUNCTION_SUCCESS)
		goto error_status;


	// Combine reading bytes into an unsigned integer value.
	// Note: Put lower address byte on value LSB.
	//       Put upper address byte on value MSB.
	Value = 0;

	for(i = 0; i < ByteNum; i++)
		Value |= (unsigned long)ReadingBytes[i] << (BYTE_SHIFT * i);


	// Get register bits from unsigned integaer value with mask and shift
	*pReadingValue = (Value & Mask) >> Shift;

	mutex_unlock(&mutex);
	

	return FUNCTION_SUCCESS;


error_status:

	mutex_unlock(&mutex);
	
	return FUNCTION_ERROR;
}



int
fm_default_SetRegBits(
	FM_MODULE *pFm,
	int RegBitName,
	const unsigned long WritingValue
	)
{
	unsigned short RegStartAddr;
	unsigned char Msb;
	unsigned char Lsb;


	// Check if register bit name is available.
	if(pFm->RegTable[RegBitName].IsAvailable == NO)
		goto error_status;


	// Get register start address, MSB, and LSB from register table with register bit name key.
	RegStartAddr = pFm->RegTable[RegBitName].RegStartAddr;
	Msb          = pFm->RegTable[RegBitName].Msb;
	Lsb          = pFm->RegTable[RegBitName].Lsb;


	// Set register mask bits.
	if(pFm->SetRegMaskBits(pFm, RegStartAddr, Msb, Lsb, WritingValue) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}



int
fm_default_GetRegBits(
	FM_MODULE *pFm,
	int RegBitName,
	unsigned long *pReadingValue
	)
{
	unsigned short RegStartAddr;
	unsigned char Msb;
	unsigned char Lsb;


	// Check if register bit name is available.
	if(pFm->RegTable[RegBitName].IsAvailable == NO)
		goto error_status;


	// Get register start address, MSB, and LSB from register table with register bit name key.
	RegStartAddr = pFm->RegTable[RegBitName].RegStartAddr;
	Msb          = pFm->RegTable[RegBitName].Msb;
	Lsb          = pFm->RegTable[RegBitName].Lsb;


	// Get register mask bits.
	if(pFm->GetRegMaskBits(pFm, RegStartAddr, Msb, Lsb, pReadingValue) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;

error_status:
	return FUNCTION_ERROR;
}



int
fm_default_GetReadOnlyRegMaskBits(
	FM_MODULE *pFm,
	unsigned char DebugSignalBus,
	unsigned short RegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	)
{
	int i;

	unsigned char ReadingBytes[LEN_4_BYTE];

	unsigned char ByteNum;
	unsigned long Mask;
	unsigned char Shift;

	unsigned long Value;


	// Calculate writing byte number according to MSB.
	ByteNum = Msb / BYTE_BIT_NUM + LEN_1_BYTE;


	// Generate mask and shift according to MSB and LSB.
	Mask = 0;

	for(i = Lsb; i < (unsigned char)(Msb + 1); i++)
		Mask |= 0x1 << i;

	Shift = Lsb;

	mutex_lock(&mutex);

	if(fm_default_SetRegBytes(pFm, FM_DEBUG_SEL, &DebugSignalBus, LEN_1_BYTE) != FUNCTION_SUCCESS)
		goto error_status;

	if(fm_default_GetRegBytes(pFm, RegStartAddr, ReadingBytes, ByteNum) != FUNCTION_SUCCESS)
		goto error_status;


	// Combine reading bytes into an unsigned integer value.
	// Note: Put lower address byte on value LSB.
	//       Put upper address byte on value MSB.
	Value = 0;

	for(i = 0; i < ByteNum; i++)
		Value |= (unsigned long)ReadingBytes[i] << (BYTE_SHIFT * i);


	// Get register bits from unsigned integaer value with mask and shift
	*pReadingValue = (Value & Mask) >> Shift;

	mutex_unlock(&mutex);


	return FUNCTION_SUCCESS;


error_status:

	mutex_unlock(&mutex);
	
	return FUNCTION_ERROR;
}



int
fm_default_GetReadOnlyRegBits(
	FM_MODULE *pFm,
	int RegBitName,
	unsigned long *pReadingValue
	)
{
	unsigned char DebugSignalBus;
	unsigned short RegStartAddr;
	unsigned char Msb;
	unsigned char Lsb;


	// Check if register bit name is available.
	if(pFm->ReadOnlyRegTable[RegBitName].IsAvailable == NO)
		goto error_status;


	// Get register start address, MSB, and LSB from register table with register bit name key.
	DebugSignalBus = pFm->ReadOnlyRegTable[RegBitName].DebugSignalBus;
	RegStartAddr = pFm->ReadOnlyRegTable[RegBitName].RegStartAddr;
	Msb          = pFm->ReadOnlyRegTable[RegBitName].Msb;
	Lsb          = pFm->ReadOnlyRegTable[RegBitName].Lsb;


	// Get register mask bits.
	if(pFm->GetReadOnlyRegMaskBits(pFm, DebugSignalBus, RegStartAddr, Msb, Lsb, pReadingValue) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;

error_status:
	return FUNCTION_ERROR;
}



int
fm_default_SetRfcReg2Bytes(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	const unsigned long WritingValue
	)
{

	unsigned char WritingBuffer[LEN_2_BYTE];


	WritingBuffer[0] = (unsigned char)(WritingValue & BYTE_MASK);
	WritingBuffer[1] = (unsigned char)((WritingValue>>BYTE_SHIFT) & BYTE_MASK);

	mutex_lock(&mutex);

	if(fm_default_SetRegBytes(pFm, 0x0063, WritingBuffer, LEN_2_BYTE) != FUNCTION_SUCCESS)
		goto error_status; 

	WritingBuffer[0] = RfcRegStartAddr;
	WritingBuffer[1] = FM_RFC_WRITE_FLAG;
	
	if(fm_default_SetRegBytes(pFm, 0x0065, WritingBuffer, LEN_2_BYTE) != FUNCTION_SUCCESS)
		goto error_status;

	mutex_unlock(&mutex);

	return FUNCTION_SUCCESS;

error_status:

	mutex_unlock(&mutex);
	
	return FUNCTION_ERROR;
}



int
fm_default_GetRfcReg2Bytes(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned long *pReadingValue
	)
{

	unsigned char WritingBuffer[LEN_2_BYTE];
	unsigned char ReadingBytes[LEN_2_BYTE];

		
	WritingBuffer[0] = RfcRegStartAddr;
	WritingBuffer[1] = FM_RFC_READ_FLAG;
	
	mutex_lock(&mutex);
	
	if(fm_default_SetRegBytes(pFm, 0x0065, WritingBuffer, LEN_2_BYTE) != FUNCTION_SUCCESS)
		goto error_status;

	if(fm_default_GetRegBytes(pFm, 0x00fa, ReadingBytes, LEN_2_BYTE) != FUNCTION_SUCCESS)
		goto error_status;

	mutex_unlock(&mutex);

	*pReadingValue= (unsigned long)(ReadingBytes[0] | (ReadingBytes[1]<<BYTE_SHIFT) );

	return FUNCTION_SUCCESS;

error_status:

	mutex_unlock(&mutex);
	
		return FUNCTION_ERROR;

}




int
fm_default_SetRfcRegMaskBits(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	const unsigned long WritingValue
	)
{
	int i;

	unsigned long Mask;
	unsigned char Shift;

	unsigned long Value;

	// Generate mask and shift according to MSB and LSB.
	Mask = 0;

	for(i = Lsb; i < (unsigned char)(Msb + 1); i++)
		Mask |= 0x1 << i;

	Shift = Lsb;

	Value = 0;

	if(pFm->GetRfcReg2Bytes(pFm, RfcRegStartAddr, &Value) != FUNCTION_SUCCESS)
		goto error_status;


	// Reserve unsigned integer value unmask bit with mask and inlay writing value into it.
	Value &= ~Mask;
	Value |= (WritingValue << Shift) & Mask;

	if(pFm->SetRfcReg2Bytes(pFm, RfcRegStartAddr, Value) != FUNCTION_SUCCESS)
		goto error_status;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}



int
fm_default_GetRfcRegMaskBits(
	FM_MODULE *pFm,
	unsigned char RfcRegStartAddr,
	unsigned char Msb,
	unsigned char Lsb,
	unsigned long *pReadingValue
	)
{
	int i;
	
	unsigned long Mask;
	unsigned char Shift;

	unsigned long Value;


	// Generate mask and shift according to MSB and LSB.
	Mask = 0;

	for(i = Lsb; i < (unsigned char)(Msb + 1); i++)
		Mask |= 0x1 << i;

	Shift = Lsb;
	

	Value = 0;
	
	if(pFm->GetRfcReg2Bytes(pFm, RfcRegStartAddr, &Value) != FUNCTION_SUCCESS)
		goto error_status;

	// Get register bits from unsigned integaer value with mask and shift
	*pReadingValue = (Value & Mask) >> Shift;


	return FUNCTION_SUCCESS;


error_status:
	return FUNCTION_ERROR;
}





void
fm_default_GetFmType(
	FM_MODULE *pFm,
	int *pFmType
	)
{
	*pFmType = pFm->FmType;


	return;
}



void
fm_default_GetDeviceAddr(
	FM_MODULE *pFm,
	unsigned char *pDeviceAddr
	)
{
	*pDeviceAddr = pFm->DeviceAddr;


	return;
}





