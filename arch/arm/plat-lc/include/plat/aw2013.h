/*
 *
 */
#ifndef __AW2013_H__
#define __AW2013_H__

/* register map */
#define LEDGCR			0x01
#define LEDSEL			0x30
#define LED0CTRL 		0x31
#define LED1CTRL 		0x32
#define LED2CTRL 		0x33
#define LED0LEVEL		0x34
#define LED1LEVEL		0x35
#define LED2LEVEL		0x36
#define LED0T0			0x37
#define LED0T1			0x38
#define LED0T2			0x39
#define LED1T0			0x3A
#define LED1T1			0x3B
#define LED1T2			0x3C
#define LED2T0			0x3D
#define LED2T1			0x3E
#define LED2T2			0x3F

/*
 *	Red->0, Green->1, Blue->2.
 */

#define LED_SWITCH(index)	(0x1 << index)
#define LED_CTRL(index)		(LED0CTRL + index)
#define LED_LEVEL(index)	(LED0LEVEL + index)
#define LED_T0(index)		(LED0T0 + index * 3)
#define LED_T1(index)		(LED0T1 + index * 3)
#define LED_T2(index)		(LED0T2 + index * 3)

#endif

