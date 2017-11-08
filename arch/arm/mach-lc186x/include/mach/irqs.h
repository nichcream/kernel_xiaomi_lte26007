/*
 * arch/arm/mach-comip/include/mach/irqs.h
 *
 ** This software is licensed under the terms of the GNU General Public
 ** License version 2, as published by the Free Software Foundation, and
 ** may be copied, distributed, and modified under those terms.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** Copyright (c) 2010-2019 LeadCoreTech Corp.
 **
 ** PURPOSE: irq.
 **
 ** CHANGE HISTORY:
 **
 ** Version	Date		Author		Description
 ** 1.0.0		2012-03-06	gaojian		created
 **
 */

#ifndef __MACH_COMIP_IRQS_H
#define __MACH_COMIP_IRQS_H

#define INT_GIC_BASE			0

#define IRQ_LOCALTIMER                  29
/* Primary Interrupt Controller */
#define	INT_PRI_BASE			(INT_GIC_BASE + 32)
#define INT_USB_CTL			(INT_PRI_BASE + 0)
#define INT_AP_DMAG			(INT_PRI_BASE + 1)
#define INT_AP_DMAS			(INT_PRI_BASE + 2)
#define	INT_AP_DMAC			(INT_PRI_BASE + 3)
#define	INT_AP_PWR			(INT_PRI_BASE + 4)
#define	INT_DDR_PWR			(INT_PRI_BASE + 5)
#define	INT_DSI				(INT_PRI_BASE + 6)
#define	INT_SMMU0			(INT_PRI_BASE + 7)
#define	INT_SMMU1			(INT_PRI_BASE + 8)
#define	INT_TIMER7			(INT_PRI_BASE + 9)
#define	INT_2DACC			(INT_PRI_BASE + 10)
#define	INT_GPU_JOB			(INT_PRI_BASE + 11)
#define	INT_GPU_MMU			(INT_PRI_BASE + 12)
#define	INT_GPU_GPU			(INT_PRI_BASE + 13)
#define	INT_ON2_CODEC			(INT_PRI_BASE + 14)
#define	INT_ON2_ENCODER1		(INT_PRI_BASE + 16)
#define	INT_USB_OTG			(INT_PRI_BASE + 17)
#define	INT_USB_HSIC			(INT_PRI_BASE + 18)
#define	INT_LCDC0			(INT_PRI_BASE + 19)
#define	INT_LCDC1			(INT_PRI_BASE + 20)
#define	INT_ISP				(INT_PRI_BASE + 21)
#define	INT_SECURITY			(INT_PRI_BASE + 22)
#define	INT_KBS				(INT_PRI_BASE + 23)
#define	INT_NFC				(INT_PRI_BASE + 24)
#define	INT_CIPHER			(INT_PRI_BASE + 25)
#define	INT_TPZCTL			(INT_PRI_BASE + 26)
#define	INT_WDT1			(INT_PRI_BASE + 27)
#define	INT_WDT2			(INT_PRI_BASE + 28)
#define	INT_WDT3			(INT_PRI_BASE + 29)
#define	INT_I2C0			(INT_PRI_BASE + 30)
#define	INT_I2C1			(INT_PRI_BASE + 31)
#define	INT_I2C2			(INT_PRI_BASE + 32)
#define	INT_I2C3			(INT_PRI_BASE + 33)
#define	INT_SDIO0			(INT_PRI_BASE + 34)
#define	INT_SDIO1			(INT_PRI_BASE + 35)
#define	INT_SDIO2			(INT_PRI_BASE + 36)
#define	INT_SSI0			(INT_PRI_BASE + 37)
#define	INT_SSI1			(INT_PRI_BASE + 38)
#define	INT_SSI2			(INT_PRI_BASE + 39)
#define	INT_UART0			(INT_PRI_BASE + 40)
#define	INT_UART1			(INT_PRI_BASE + 41)
#define	INT_UART2			(INT_PRI_BASE + 42)
#define	INT_TIMER0			(INT_PRI_BASE + 43)
#define	INT_TIMER1			(INT_PRI_BASE + 44)
#define	INT_TIMER2			(INT_PRI_BASE + 45)
#define	INT_TIMER3			(INT_PRI_BASE + 46)
#define	INT_TIMER4			(INT_PRI_BASE + 47)
#define	INT_TIMER5			(INT_PRI_BASE + 48)
#define	INT_TIMER6			(INT_PRI_BASE + 49)
#define	INT_CTI_1CORE			(INT_PRI_BASE + 50)
#define	INT_CTI_4CORE_0			(INT_PRI_BASE + 51)
#define	INT_CTI_4CORE_1			(INT_PRI_BASE + 52)
#define	INT_CTI_4CORE_2			(INT_PRI_BASE + 53)
#define	INT_CTI_4CORE_3			(INT_PRI_BASE + 54)
#define	INT_GPIO			(INT_PRI_BASE + 55)
#define	INT_COM_I2C			(INT_PRI_BASE + 56)
#define	INT_COM_UART			(INT_PRI_BASE + 57)
#define	INT_COM_I2S			(INT_PRI_BASE + 58)
#define	INT_AP_I2S			(INT_PRI_BASE + 59)
#define	INT_PCM				(INT_PRI_BASE + 60)
#define	INT_TOP_DMAS			(INT_PRI_BASE + 61)
#define	INT_MAILBOX			(INT_PRI_BASE + 62)
#define	INT_TOP_DMAG			(INT_PRI_BASE + 63)
#define	INT_AP_HA7_PMU0			(INT_PRI_BASE + 64)
#define	INT_AP_SA7_PMU			(INT_PRI_BASE + 65)
#define	INT_WDT0			(INT_PRI_BASE + 66)
#define	INT_HA7_PMU1			(INT_PRI_BASE + 67)
#define	INT_HA7_PMU2			(INT_PRI_BASE + 68)
#define	INT_HA7_PMU3			(INT_PRI_BASE + 69)
#define	INT_SA7_AXI_ERR			(INT_PRI_BASE + 70)
#define	INT_HA7_AXI_ERR			(INT_PRI_BASE + 71)
#define	INT_CCI400_ERR			(INT_PRI_BASE + 72)
#define	INT_TOPMAILBOX_METS		(INT_PRI_BASE + 73)
#define NR_COMIP_IRQS			(106)

/* For AP_PWR IRQs. */
#define NR_AP_PWR_IRQS			(32)
#define IRQ_AP_PWR(x) 			(NR_COMIP_IRQS + (x))
#define IRQ_TO_AP_PWR(x)		((x) - IRQ_AP_PWR(0))

/* For GPIO IRQs. */
#define NR_GPIO_IRQS			(256)
#define IRQ_GPIO(x) 			(IRQ_AP_PWR(NR_AP_PWR_IRQS) + (x))
#define IRQ_TO_GPIO(x)			((x) - IRQ_GPIO(0))

/* For CP IRQs. */
#define NR_CP_IRQS			(64)
#define IRQ_CP(x)			(IRQ_GPIO(NR_GPIO_IRQS) + (x))
#define IRQ_TO_CP(x)			((x) - IRQ_CP(0))

#define NR_IRQS				(NR_COMIP_IRQS + NR_AP_PWR_IRQS + NR_GPIO_IRQS + NR_CP_IRQS)

#endif /* __MACH_COMIP_IRQS_H */
