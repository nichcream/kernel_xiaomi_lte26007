/* Lite-On LTR-558ALS Linux Driver
 *
 * Copyright (C) 2011 Lite-On Technology Corp (Singapore)
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _LTR55X_H
#define _LTR55X_H


/* LTR-55X Registers */
#define LTR55x_ALS_CONTR  0x80
#define LTR55x_PS_CONTR  0x81
#define LTR55x_PS_LED   0x82
#define LTR55x_PS_N_PULSES  0x83
#define LTR55x_PS_MEAS_RATE 0x84
#define LTR55x_ALS_MEAS_RATE 0x85
#define LTR55x_PART_ID      0x86
#define LTR55x_MANUFACTURER_ID 0x87

#define LTR55x_INTERRUPT  0x8F
#define LTR55x_PS_THRES_UP_0 0x90
#define LTR55x_PS_THRES_UP_1 0x91
#define LTR55x_PS_THRES_LOW_0 0x92
#define LTR55x_PS_THRES_LOW_1 0x93

#define LTR55x_ALS_THRES_UP_0 0x97
#define LTR55x_ALS_THRES_UP_1 0x98
#define LTR55x_ALS_THRES_LOW_0 0x99
#define LTR55x_ALS_THRES_LOW_1 0x9A

#define LTR55x_INTERRUPT_PERSIST 0x9E

/* 558's Read Only Registers */
#define LTR55x_ALS_DATA_CH1_0 0x88
#define LTR55x_ALS_DATA_CH1_1 0x89
#define LTR55x_ALS_DATA_CH0_0 0x8A
#define LTR55x_ALS_DATA_CH0_1 0x8B
#define LTR55x_ALS_PS_STATUS 0x8C
#define LTR55x_PS_DATA_0  0x8D
#define LTR55x_PS_DATA_1  0x8E


/* Basic Operating Modes */
/**************************LTR553**************************************/
/*ALS*/
#define LTR553_ALS_RANGE_64K	1
#define LTR553_ALS_RANGE_32K 	2
#define LTR553_ALS_RANGE_16K 	4
#define LTR553_ALS_RANGE_8K 	8
#define LTR553_ALS_RANGE_1300 48
#define LTR553_ALS_RANGE_600 	96

#define LTR553_MODE_ALS_ON_Range1	0x01  ///for als gain x1
#define LTR553_MODE_ALS_ON_Range2	0x05  ///for als  gain x2
#define LTR553_MODE_ALS_ON_Range3	0x09  ///for als  gain x4
#define LTR553_MODE_ALS_ON_Range4	0x0D  ///for als gain x8
#define LTR553_MODE_ALS_ON_Range5	0x19  ///for als gain x48
#define LTR553_MODE_ALS_ON_Range6	0x1D  ///for als gain x96

/*PS*/
#define LTR553_PS_RANGE16 	1
#define LTR553_PS_RANGE32	4
#define LTR553_PS_RANGE64 	8

#define LTR553_MODE_PS_ON_Gain16	0x03
#define LTR553_MODE_PS_ON_Gain32	0x0B
#define LTR553_MODE_PS_ON_Gain64	0x0F

#define LTR553_MODE_ALS_StdBy		0x00
#define LTR553_MODE_PS_StdBy		0x00


/**************************LTR558**************************************/
#define LTR558_MODE_ALS_ON_Range1  0x0B
#define LTR558_MODE_ALS_ON_Range2  0x03


#define MODE_PS_ON_Gain1  0x03
#define MODE_PS_ON_Gain2  0x07
#define MODE_PS_ON_Gain4  0x0B
#define MODE_PS_ON_Gain8  0x0C
/**********************************************************************/
/*common used*/
#define MODE_ALS_StdBy		0x00
#define MODE_PS_StdBy		0x00
#define MODE_PS_Active		0x03

/*********************************************************************/


#define PS_RANGE1  1
#define PS_RANGE2 2
#define PS_RANGE4  4
#define PS_RANGE8 8

#define ALS_RANGE1_320  1
#define ALS_RANGE2_64K  2

enum {
    LTR553 =1,
    LTR558 ,
    LTR55X=0xFF,
};
/*
 * Magic Number
 * ============
 * Refer to file ioctl-number.txt for allocation
 */
#define LTR55X_IOCTL_MAGIC      'c'

/* Power On response time in ms */
#define PON_DELAY 600
#define WAKEUP_DELAY 10

struct ltr55x_platform_data{
	int irq_gpio;
};
#endif
