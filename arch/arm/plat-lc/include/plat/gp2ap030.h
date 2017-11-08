/*
Copyright(C), 2013, leadcore Inc.
Description: Definitions for gp2ap030 light-prox chip.
History:
<author>  <time>       <version>   <desc>
hudie     2013/8/20      1.0       build this module
*/

#ifndef _GP2AP030_H
#define _GP2AP030_H

// Reg.
#define REG_ADR_00	0x00	// Read & Write
#define REG_ADR_01	0x01	// Read & Write
#define REG_ADR_02	0x02	// Read & Write
#define REG_ADR_03	0x03	// Read & Write
#define REG_ADR_04	0x04	// Read & Write
#define REG_ADR_05	0x05	// Read & Write
#define REG_ADR_06	0x06	// Read & Write
#define REG_ADR_07	0x07	// Read & Write
#define REG_ADR_08	0x08	// Read & Write
#define REG_ADR_09	0x09	// Read & Write
#define REG_ADR_0A	0x0A	// Read & Write
#define REG_ADR_0B	0x0B	// Read & Write
#define REG_ADR_0C	0x0C	// Read  Only
#define REG_ADR_0D	0x0D	// Read  Only
#define REG_ADR_0E	0x0E	// Read  Only
#define REG_ADR_0F	0x0F	// Read  Only
#define REG_ADR_10	0x10	// Read  Only
#define REG_ADR_11	0x11	// Read  Only
#define REG_ADR_16	0x16	// Read  Only
#define REG_ADR_17	0x17	// Read  Only

#define GP2AP030_ALS_CONF_REG		REG_ADR_01
#define GP2AP030_PROX_CONF_REG		REG_ADR_02
#define GP2AP030_INTR_CONF_REG		REG_ADR_03
#define GP2AP030_ALS_DATA_REG0		REG_ADR_0C
#define GP2AP030_ALS_DATA_REG1		REG_ADR_0E
#define GP2AP030_PS_DATA_REG		REG_ADR_10
#define GP2AP030_ID_REG			REG_ADR_17

/*************************************config********************************************/
#define gp2ap030_DEVICE_NAME                "gp2ap030"
#define gp2ap030_DEVICE_ID                  "gp2ap030"

#define ALS_AVG_READ_COUNT		( 10 )		/* sensor read count */
#define ALS_AVG_READ_INTERVAL		( 20 )		/* 20 ms */

#define PS_AVG_READ_COUNT		( 40 )		/* sensor read count */
#define PS_AVG_READ_INTERVAL		( 40 )		/* 40 ms */
#define PS_AVG_POLL_DELAY		( 2000 )	/* 2000 ms */

#define LOW_LUX_MODE			( 0 )
#define HIGH_LUX_MODE			( 1 )

#define CALIBRATION_ONOFF		( 1 )
#define SET_TH  			( 20 )

// Reg. 00H
#define	OP_SHUTDOWN		0x00	// OP3:0
#define	OP_RUN			0x80	// OP3:1
#define	OP_CONTINUOUS		0x40	// OP2:1
#define	OP_PS_ALS		0x00	// OP01:00
#define	OP_ALS			0x10	// OP01:01
#define	OP_PS			0x20	// OP01:10
#define	OP_COUNT		0x30	// OP01:11
#define	INT_NOCLEAR		0x0E	// PROX:1 FLAG_P:1 FLAG_A:1

// Reg. 01H
#define	PRST_1			0x00	// PRST:00
#define	PRST_4			0x40	// PRST:01
#define	PRST_8			0x80	// PRST:10
#define	PRST_16			0xC0	// PRST:11
#define	RES_A_14		0x20	// RES_A:100
#define	RES_A_16		0x18	// RES_A:011
#define	RANGE_A_8		0x03	// RANGE_A:011
#define	RANGE_A_32		0x05	// RANGE_A:101
#define	RANGE_A_64		0x06	// RANGE_A:110
#define	RANGE_A_128		0x07	// RANGE_A:111

// Reg. 02H
#define	INTTYPE_L		0x00	// INTTYPE:0
#define	INTTYPE_P		0x40	// INTTYPE:1
#define	RES_P_14		0x08	// RES_P:001
#define	RES_P_12		0x10	// RES_P:010
#define	RES_P_10		0x18	// RES_P:011
#define	RANGE_P_4		0x02	// RANGE_A:010
#define	RANGE_P_8		0x03	// RANGE_A:011

// Reg. 03H
#define	INTVAL_0		0x00	// INTVAL:00
#define	INTVAL_4		0x40	// INTVAL:01
#define	INTVAL_8		0x80	// INTVAL:10
#define	INTVAL_16		0xC0	// INTVAL:11
#define	IS_130			0x30	// IS:11
#define	PIN_INT			0x00	// PIN:00
#define	PIN_INT_ALS		0x04	// PIN:01
#define	PIN_INT_PS		0x08	// PIN:10
#define	PIN_DETECT		0x0C	// PIN:11
#define	FREQ_327_5		0x00	// FREQ:0
#define	FREQ_81_8		0x02	// FREQ:1
#define	RST			0x01	// RST:1

/* Reg1 PRST:01 RES_A:100 RANGE_A:011 */
#define GP2AP030_ALS_CONFIG	(PRST_4 | RES_A_14 | RANGE_A_8)
#define INT_MASK		(0x03 << 2)
#define OP_MODE_MASK		(0x03 << 4)
/*************************************config********************************************/

#define ALS_ENABLED	0x01
#define PROX_ENABLED	0x02

struct gp2ap030_platform_data {
	u16	prox_threshold_hi;
	u16 	prox_threshold_lo;
};
#endif
