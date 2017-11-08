#ifndef __FOUNDATION_H
#define __FOUNDATION_H

/**

@file

@brief   Fundamental interface declaration

Fundamental interface contains base function pointers and some mathematics tools.

*/


#include "math_mpi.h"





// Definitions

// API version
#define REALTEK_NIM_API_VERSION		"Realtek NIM API 2013.01.07"



// Constants
#define INVALID_POINTER_VALUE		0
#define NO_USE						0

#define LEN_1_BYTE					1
#define LEN_2_BYTE					2
#define LEN_3_BYTE					3
#define LEN_4_BYTE					4
#define LEN_5_BYTE					5
#define LEN_6_BYTE					6
#define LEN_11_BYTE					11

#define LEN_1_BIT					1

#define BYTE_MASK					0xff
#define BYTE_SHIFT					8
#define HEX_DIGIT_MASK				0xf
#define BYTE_BIT_NUM				8
#define LONG_BIT_NUM				32

#define BIT_0_MASK					0x1
#define BIT_1_MASK					0x2
#define BIT_2_MASK					0x4
#define BIT_3_MASK					0x8
#define BIT_4_MASK					0x10
#define BIT_5_MASK					0x20
#define BIT_6_MASK					0x40
#define BIT_7_MASK					0x80
#define BIT_8_MASK					0x100

#define BIT_7_SHIFT					7
#define BIT_8_SHIFT					8



// I2C buffer length
// Note: I2C_BUFFER_LEN must be greater than I2cReadingByteNumMax and I2cWritingByteNumMax in BASE_INTERFACE_MODULE.
#define I2C_BUFFER_LEN				128



/// Function return status
enum FUNCTION_RETURN_STATUS
{
	FUNCTION_SUCCESS,			///<   Execute function successfully.
	FUNCTION_ERROR,				///<   Execute function unsuccessfully.
};








/// Base interface module alias
typedef struct BASE_INTERFACE_MODULE_TAG BASE_INTERFACE_MODULE;





/**

@brief   Basic I2C reading function pointer

Upper layer functions will use BASE_FP_I2C_READ() to read ByteNum bytes from I2C device to pReadingBytes buffer.


@param [in]    pBaseInterface   The base interface module pointer
@param [in]    DeviceAddr       I2C device address in 7-bit format
@param [out]   pReadingBytes    Buffer pointer to an allocated memory for storing reading bytes
@param [in]    ByteNum          Reading byte number


@retval   FUNCTION_SUCCESS   Read bytes from I2C device with reading byte number successfully.
@retval   FUNCTION_ERROR     Read bytes from I2C device unsuccessfully.


@note
	The requirements of BASE_FP_I2C_READ() function are described as follows:
	-# Follow the I2C format for BASE_FP_I2C_READ(). \n
	   start_bit + (DeviceAddr | reading_bit) + reading_byte * ByteNum + stop_bit
	-# Don't allocate memory on pReadingBytes.
	-# Upper layer functions should allocate memory on pReadingBytes before using BASE_FP_I2C_READ().
	-# Need to assign I2C reading funtion to BASE_FP_I2C_READ() for upper layer functions.



@par Example:
@code


#include "foundation.h"


// Implement I2C reading funciton for BASE_FP_I2C_READ function pointer.
int
CustomI2cRead(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	unsigned char *pReadingBytes,
	unsigned char ByteNum
	)
{
	...

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}


int main(void)
{
	BASE_INTERFACE_MODULE *pBaseInterface;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;
	unsigned char ReadingBytes[100];


	// Assign implemented I2C reading funciton to BASE_FP_I2C_READ in base interface module.
	BuildBaseInterface(&pBaseInterface, &BaseInterfaceModuleMemory, ..., ..., CustomI2cRead, ..., ...);

	...

	// Use I2cRead() to read 33 bytes from I2C device and store reading bytes to ReadingBytes.
	pBaseInterface->I2cRead(pBaseInterface, 0x20, ReadingBytes, 33);
	
	...

	return 0;
}


@endcode

*/
typedef int
(*BASE_FP_I2C_READ)(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	unsigned char *pReadingBytes,
	unsigned long ByteNum
	);





/**

@brief   Basic I2C writing function pointer

Upper layer functions will use BASE_FP_I2C_WRITE() to write ByteNum bytes from pWritingBytes buffer to I2C device.


@param [in]   pBaseInterface   The base interface module pointer
@param [in]   DeviceAddr       I2C device address in 7-bit format
@param [in]   pWritingBytes    Buffer pointer to writing bytes
@param [in]   ByteNum          Writing byte number


@retval   FUNCTION_SUCCESS   Write bytes to I2C device with writing bytes successfully.
@retval   FUNCTION_ERROR     Write bytes to I2C device unsuccessfully.


@note
	The requirements of BASE_FP_I2C_WRITE() function are described as follows:
	-# Follow the I2C format for BASE_FP_I2C_WRITE(). \n
	   start_bit + (DeviceAddr | writing_bit) + writing_byte * ByteNum + stop_bit
	-# Need to assign I2C writing funtion to BASE_FP_I2C_WRITE() for upper layer functions.



@par Example:
@code


#include "foundation.h"


// Implement I2C writing funciton for BASE_FP_I2C_WRITE function pointer.
int
CustomI2cWrite(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	const unsigned char *pWritingBytes,
	unsigned char ByteNum
	)
{
	...

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}


int main(void)
{
	BASE_INTERFACE_MODULE *pBaseInterface;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;
	unsigned char WritingBytes[100];


	// Assign implemented I2C writing funciton to BASE_FP_I2C_WRITE in base interface module.
	BuildBaseInterface(&pBaseInterface, &BaseInterfaceModuleMemory, ..., ..., ..., CustomI2cWrite, ...);

	...

	// Use I2cWrite() to write 33 bytes from WritingBytes to I2C device.
	pBaseInterface->I2cWrite(pBaseInterface, 0x20, WritingBytes, 33);
	
	...

	return 0;
}


@endcode

*/
typedef int
(*BASE_FP_I2C_WRITE)(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	const unsigned char *pWritingBytes,
	unsigned long ByteNum
	);





/**

@brief   Basic waiting function pointer

Upper layer functions will use BASE_FP_WAIT_MS() to wait WaitTimeMs milliseconds.


@param [in]   pBaseInterface   The base interface module pointer
@param [in]   WaitTimeMs       Waiting time in millisecond


@note
	The requirements of BASE_FP_WAIT_MS() function are described as follows:
	-# Need to assign a waiting function to BASE_FP_WAIT_MS() for upper layer functions.



@par Example:
@code


#include "foundation.h"


// Implement waiting funciton for BASE_FP_WAIT_MS function pointer.
void
CustomWaitMs(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned long WaitTimeMs
	)
{
	...

	return;
}


int main(void)
{
	BASE_INTERFACE_MODULE *pBaseInterface;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;


	// Assign implemented waiting funciton to BASE_FP_WAIT_MS in base interface module.
	BuildBaseInterface(&pBaseInterface, &BaseInterfaceModuleMemory, ..., ..., ..., ..., CustomWaitMs);

	...

	// Use WaitMs() to wait 30 millisecond.
	pBaseInterface->WaitMs(pBaseInterface, 30);
	
	...

	return 0;
}


@endcode

*/
typedef void
(*BASE_FP_WAIT_MS)(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned long WaitTimeMs
	);





/**

@brief   User defined data pointer setting function pointer

One can use BASE_FP_SET_USER_DEFINED_DATA_POINTER() to set user defined data pointer of base interface structure for
custom basic function implementation.


@param [in]   pBaseInterface     The base interface module pointer
@param [in]   pUserDefinedData   Pointer to user defined data


@note
	One can use BASE_FP_GET_USER_DEFINED_DATA_POINTER() to get user defined data pointer of base interface structure for
	custom basic function implementation.



@par Example:
@code


#include "foundation.h"


// Implement I2C reading funciton for BASE_FP_I2C_READ function pointer.
int
CustomI2cRead(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	unsigned char *pReadingBytes,
	unsigned char ByteNum
	)
{
	CUSTOM_USER_DEFINED_DATA *pUserDefinedData;


	// Get user defined data pointer of base interface structure for custom I2C reading function.
	pBaseInterface->GetUserDefinedDataPointer(pBaseInterface, (void **)&pUserDefinedData);

	...

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}


int main(void)
{
	BASE_INTERFACE_MODULE *pBaseInterface;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;
	unsigned char ReadingBytes[100];

	CUSTOM_USER_DEFINED_DATA UserDefinedData;


	// Assign implemented I2C reading funciton to BASE_FP_I2C_READ in base interface module.
	BuildBaseInterface(&pBaseInterface, &BaseInterfaceModuleMemory, ..., ..., CustomI2cRead, ..., ...);

	...

	// Set user defined data pointer of base interface structure for custom basic functions.
	pBaseInterface->SetUserDefinedDataPointer(pBaseInterface, &UserDefinedData);

	// Use I2cRead() to read 33 bytes from I2C device and store reading bytes to ReadingBytes.
	pBaseInterface->I2cRead(pBaseInterface, 0x20, ReadingBytes, 33);
	
	...

	return 0;
}


@endcode

*/
typedef void
(*BASE_FP_SET_USER_DEFINED_DATA_POINTER)(
	BASE_INTERFACE_MODULE *pBaseInterface,
	void *pUserDefinedData
	);





/**

@brief   User defined data pointer getting function pointer

One can use BASE_FP_GET_USER_DEFINED_DATA_POINTER() to get user defined data pointer of base interface structure for
custom basic function implementation.


@param [in]   pBaseInterface      The base interface module pointer
@param [in]   ppUserDefinedData   Pointer to user defined data pointer


@note
	One can use BASE_FP_SET_USER_DEFINED_DATA_POINTER() to set user defined data pointer of base interface structure for
	custom basic function implementation.



@par Example:
@code


#include "foundation.h"


// Implement I2C reading funciton for BASE_FP_I2C_READ function pointer.
int
CustomI2cRead(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	unsigned char *pReadingBytes,
	unsigned char ByteNum
	)
{
	CUSTOM_USER_DEFINED_DATA *pUserDefinedData;


	// Get user defined data pointer of base interface structure for custom I2C reading function.
	pBaseInterface->GetUserDefinedDataPointer(pBaseInterface, (void **)&pUserDefinedData);

	...

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}


int main(void)
{
	BASE_INTERFACE_MODULE *pBaseInterface;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;
	unsigned char ReadingBytes[100];

	CUSTOM_USER_DEFINED_DATA UserDefinedData;


	// Assign implemented I2C reading funciton to BASE_FP_I2C_READ in base interface module.
	BuildBaseInterface(&pBaseInterface, &BaseInterfaceModuleMemory, ..., ..., CustomI2cRead, ..., ...);

	...

	// Set user defined data pointer of base interface structure for custom basic functions.
	pBaseInterface->SetUserDefinedDataPointer(pBaseInterface, &UserDefinedData);

	// Use I2cRead() to read 33 bytes from I2C device and store reading bytes to ReadingBytes.
	pBaseInterface->I2cRead(pBaseInterface, 0x20, ReadingBytes, 33);
	
	...

	return 0;
}


@endcode

*/
typedef void
(*BASE_FP_GET_USER_DEFINED_DATA_POINTER)(
	BASE_INTERFACE_MODULE *pBaseInterface,
	void **ppUserDefinedData
	);





/// Base interface module structure
struct BASE_INTERFACE_MODULE_TAG
{
	// Variables and function pointers
	unsigned long I2cReadingByteNumMax;
	unsigned long I2cWritingByteNumMax;

	BASE_FP_I2C_READ    I2cRead;
	BASE_FP_I2C_WRITE   I2cWrite;
	BASE_FP_WAIT_MS     WaitMs;

	BASE_FP_SET_USER_DEFINED_DATA_POINTER   SetUserDefinedDataPointer;
	BASE_FP_GET_USER_DEFINED_DATA_POINTER   GetUserDefinedDataPointer;


	// User defined data
	void *pUserDefinedData;
};





/**

@brief   Base interface builder

Use BuildBaseInterface() to build base interface for module functions to access basic functions.


@param [in]   ppBaseInterface              Pointer to base interface module pointer
@param [in]   pBaseInterfaceModuleMemory   Pointer to an allocated base interface module memory
@param [in]   I2cReadingByteNumMax         Maximum I2C reading byte number for basic I2C reading function
@param [in]   I2cWritingByteNumMax         Maximum I2C writing byte number for basic I2C writing function
@param [in]   I2cRead                      Basic I2C reading function pointer
@param [in]   I2cWrite                     Basic I2C writing function pointer
@param [in]   WaitMs                       Basic waiting function pointer


@note
	-# One should build base interface before using module functions.
	-# The I2C reading format is described as follows:
	   start_bit + (device_addr | reading_bit) + reading_byte * byte_num + stop_bit
	-# The I2cReadingByteNumMax is the maximum byte_num of the I2C reading format.
	-# The I2C writing format is described as follows:
	   start_bit + (device_addr | writing_bit) + writing_byte * byte_num + stop_bit
	-# The I2cWritingByteNumMax is the maximum byte_num of the I2C writing format.



@par Example:
@code


#include "foundation.h"


// Implement I2C reading funciton for BASE_FP_I2C_READ function pointer.
int
CustomI2cRead(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	unsigned char *pReadingBytes,
	unsigned char ByteNum
	)
{
	...

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}


// Implement I2C writing funciton for BASE_FP_I2C_WRITE function pointer.
int
CustomI2cWrite(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned char DeviceAddr,
	const unsigned char *pWritingBytes,
	unsigned char ByteNum
	)
{
	...

	return FUNCTION_SUCCESS;

error_status:

	return FUNCTION_ERROR;
}


// Implement waiting funciton for BASE_FP_WAIT_MS function pointer.
void
CustomWaitMs(
	BASE_INTERFACE_MODULE *pBaseInterface,
	unsigned long WaitTimeMs
	)
{
	...

	return;
}


int main(void)
{
	BASE_INTERFACE_MODULE *pBaseInterface;
	BASE_INTERFACE_MODULE BaseInterfaceModuleMemory;


	// Build base interface with the following settings.
	//
	// 1. Assign 9 to maximum I2C reading byte number.
	// 2. Assign 8 to maximum I2C writing byte number.
	// 3. Assign CustomI2cRead() to basic I2C reading function pointer.
	// 4. Assign CustomI2cWrite() to basic I2C writing function pointer.
	// 5. Assign CustomWaitMs() to basic waiting function pointer.
	//
	BuildBaseInterface(
		&pBaseInterface,
		&BaseInterfaceModuleMemory,
		9,
		8,
		CustomI2cRead,
		CustomI2cWrite,
		CustomWaitMs
		);

	...

	return 0;
}


@endcode

*/
void
BuildBaseInterface(
	BASE_INTERFACE_MODULE **ppBaseInterface,
	BASE_INTERFACE_MODULE *pBaseInterfaceModuleMemory,
	unsigned long I2cReadingByteNumMax,
	unsigned long I2cWritingByteNumMax,
	BASE_FP_I2C_READ I2cRead,
	BASE_FP_I2C_WRITE I2cWrite,
	BASE_FP_WAIT_MS WaitMs
	);





// User data pointer of base interface structure setting and getting functions
void
base_interface_SetUserDefinedDataPointer(
	BASE_INTERFACE_MODULE *pBaseInterface,
	void *pUserDefinedData
	);

void
base_interface_GetUserDefinedDataPointer(
	BASE_INTERFACE_MODULE *pBaseInterface,
	void **ppUserDefinedData
	);





// Math functions

// Binary and signed integer converter
unsigned long
SignedIntToBin(
	long Value,
	unsigned char BitNum
	);

long
BinToSignedInt(
	unsigned long Binary,
	unsigned char BitNum
	);



// Arithmetic
unsigned long
DivideWithCeiling(
	unsigned long Dividend,
	unsigned long Divisor
	);











/**

@mainpage Realtek demod Source Code Manual

@note
	-# The Realtek demod API source code is designed for demod IC driver porting.
	-# The API source code is written in C language without floating-point arithmetic.
	-# One can use the API to manipulate Realtek demod IC.
	-# The API will call custom underlayer functions through API base interface module.


@par Important:
	-# Please assign API base interface module with custom underlayer functions instead of modifying API source code.
	-# Please see the example code to understand the relation bewteen API and custom system.

*/

















#endif
