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
#define INT_PRI_BASE			(INT_GIC_BASE + 32)
#define INT_CTL				(INT_PRI_BASE + 0)
#define INT_AP_DMAG			(INT_PRI_BASE + 1)
#define INT_AUDIO_DMAS			(INT_PRI_BASE + 2)
#define INT_AP_DMAS			(INT_PRI_BASE + 3)
#define INT_SECURITY			(INT_PRI_BASE + 4)
#define INT_KBS				(INT_PRI_BASE + 5)

#if defined(CONFIG_CPU_LC1813)
#define INT_TIMER4			(INT_PRI_BASE + 6)
#define INT_WDT 			(INT_PRI_BASE + 7)
#define INT_WDT3			(INT_PRI_BASE + 8)
#else
#define INT_RTC				(INT_PRI_BASE + 6)
#define INT_TRACKBALL			(INT_PRI_BASE + 7)
#define INT_WDT				(INT_PRI_BASE + 8)
#endif

#define INT_TIMER0			(INT_PRI_BASE + 9)
#define INT_I2C0			(INT_PRI_BASE + 10)
#define INT_I2C1			(INT_PRI_BASE + 11)
#define INT_I2C2			(INT_PRI_BASE + 12)
#define INT_I2C3			(INT_PRI_BASE + 13)
#define INT_COM_I2C			(INT_PRI_BASE + 14)
#define INT_AP_PWR			(INT_PRI_BASE + 15)
#define INT_GPIO			(INT_PRI_BASE + 16)
#define INT_DSI				(INT_PRI_BASE + 17)

#if defined(CONFIG_CPU_LC1813)
#define INT_PMU0			(INT_PRI_BASE + 18)
#else
#define INT_HDMI			(INT_PRI_BASE + 18)
#endif

#define INT_GPU_GP			(INT_PRI_BASE + 19)
#define INT_GPU_PP0			(INT_PRI_BASE + 20)
#define INT_GPU_PP1			(INT_PRI_BASE + 21)
#define INT_GPU_GPMMU			(INT_PRI_BASE + 22)
#define INT_GPU_PP0MMU			(INT_PRI_BASE + 23)
#define INT_GPU_PP1MMU			(INT_PRI_BASE + 24)
#define INT_GPU_PMU			(INT_PRI_BASE + 25)
#define INT_I2S0			(INT_PRI_BASE + 26)
#define INT_I2S1			(INT_PRI_BASE + 27)
#define INT_SDIO0			(INT_PRI_BASE + 28)
#define INT_SDIO1			(INT_PRI_BASE + 29)
#define INT_SDIO2			(INT_PRI_BASE + 30)
#define INT_SSI0			(INT_PRI_BASE + 31)

/* Secondary Interrupt Controller */
#define INT_SEC_BASE			(INT_PRI_BASE + 32)
#define INT_SSI1			(INT_SEC_BASE + 0)
#define INT_SSI2			(INT_SEC_BASE + 1)

#if defined(CONFIG_CPU_LC1813)
#define INT_PMU1			(INT_SEC_BASE + 2)
#else
#define INT_COM_PCM			(INT_SEC_BASE + 2)
#endif

#define INT_UART0			(INT_SEC_BASE + 3)
#define INT_UART1			(INT_SEC_BASE + 4)
#define INT_UART2			(INT_SEC_BASE + 5)

#if defined(CONFIG_CPU_LC1813)
#define INT_TIMER5			(INT_SEC_BASE + 6)
#else
#define INT_DDR_INT			(INT_SEC_BASE + 6)
#endif
#define INT_COM_UART			(INT_SEC_BASE + 7)

#if defined(CONFIG_CPU_LC1813)
#define INT_PMU2			(INT_SEC_BASE + 8)
#else
#define INT_SDIO3			(INT_SEC_BASE + 8)
#endif

#define INT_ON2_DECODER			(INT_SEC_BASE + 9)
#define INT_ON2_ENCODER0		(INT_SEC_BASE + 10)
#define INT_ON2_ENCODER1		(INT_SEC_BASE + 11)
#define INT_USB_OTG			(INT_SEC_BASE + 12)

#if defined(CONFIG_CPU_LC1813)
#define INT_DMAC			(INT_SEC_BASE + 13)
#define INT_CTI3			(INT_SEC_BASE + 14)
#define INT_2DACC			(INT_CTI3)
#else
#define INT_USB_HOST			(INT_SEC_BASE + 13)
#define INT_USB_HSIC			(INT_SEC_BASE + 14)
#endif

#define INT_LCDC0			(INT_SEC_BASE + 15)

#if defined(CONFIG_CPU_LC1813)
#define INT_PMU3			(INT_SEC_BASE + 16)
#else
#define INT_LCDC1			(INT_SEC_BASE + 16)
#endif

#define INT_NFC				(INT_SEC_BASE + 17)
#define INT_ISP				(INT_SEC_BASE + 18)
#define INT_CIPHER			(INT_SEC_BASE + 19)

#if defined(CONFIG_CPU_LC1813)
#define INT_TIMER6			(INT_SEC_BASE + 20)
#else
#define INT_COM_UART1			(INT_SEC_BASE + 20)
#endif

#define INT_TPZCTL			(INT_SEC_BASE + 21)
#define INT_SSI3			(INT_SEC_BASE + 22)
#define INT_TIMER1			(INT_SEC_BASE + 23)
#define INT_TIMER2			(INT_SEC_BASE + 24)
#define INT_TIMER3			(INT_SEC_BASE + 25)
#define INT_COM_PCM1			(INT_SEC_BASE + 26)
#define INT_DDR_PWR			(INT_SEC_BASE + 27)
#define INT_GPIO1			(INT_SEC_BASE + 28)

#if defined(CONFIG_CPU_LC1813)
#define INT_WDT1			(INT_SEC_BASE + 29)
#define INT_WDT2			(INT_SEC_BASE + 30)
#define INT_AXIERRIRQ			(INT_SEC_BASE + 31)
#else
#define INT_A9_L2C_L2CCINTR		(INT_SEC_BASE + 29)
#define INT_CTL_SEC			(INT_SEC_BASE + 30)
#endif

#define NR_COMIP_IRQS			(96)

/* AP PWR Interrupt Controller. */
#if defined(CONFIG_CPU_LC1813)
#define INT_A7_CPU1_PU			IRQ_AP_PWR(0)
#define INT_A7_CPU2_PU			IRQ_AP_PWR(1)
#define INT_A7_DBG_PU			IRQ_AP_PWR(2)
#define INT_A7_PU			IRQ_AP_PWR(3)
#define INT_ON2_PU			IRQ_AP_PWR(4)
#define INT_ISP_PU			IRQ_AP_PWR(5)
#define INT_A7_CPU3_PU			IRQ_AP_PWR(0)
#define INT_A7_CPU1_PD			IRQ_AP_PWR(8)
#define INT_A7_CPU2_PD			IRQ_AP_PWR(9)
#define INT_A7_DBG_PD			IRQ_AP_PWR(10)
#define INT_A7_PD			IRQ_AP_PWR(11)
#define INT_ON2_PD			IRQ_AP_PWR(12)
#define INT_ISP_PD			IRQ_AP_PWR(13)
#define INT_A7_CPU3_PD			IRQ_AP_PWR(14)
#define INT_AP_PWR_TM			IRQ_AP_PWR(16)
#define INT_CSPWRREQ			IRQ_AP_PWR(18)
#else
#define INT_NONE0_PU			IRQ_AP_PWR(0)
#define INT_NONE1_PU			IRQ_AP_PWR(1)
#define INT_PTM_PU			IRQ_AP_PWR(2)
#define INT_A9_PU			IRQ_AP_PWR(3)
#define INT_ON2_PU			IRQ_AP_PWR(4)
#define INT_ISP_PU			IRQ_AP_PWR(5)
#define INT_NONE0_PD			IRQ_AP_PWR(8)
#define INT_NONE1_PD			IRQ_AP_PWR(9)
#define INT_PTM_PD			IRQ_AP_PWR(10)
#define INT_A9_PD			IRQ_AP_PWR(11)
#define INT_ON2_PD			IRQ_AP_PWR(12)
#define INT_ISP_PD			IRQ_AP_PWR(13)
#define INT_AP_PWR_TM			IRQ_AP_PWR(16)
#define INT_PLL2			IRQ_AP_PWR(17)
#define INT_CSPWRREQ			IRQ_AP_PWR(18)
#define INT_MO_WDT			IRQ_AP_PWR(20)
#endif
/* For AP_PWR IRQs. */
#define NR_AP_PWR_IRQS			(21)
#define IRQ_AP_PWR(x) 			(NR_COMIP_IRQS + (x))
#define IRQ_TO_AP_PWR(x)		((x) - IRQ_AP_PWR(0))

/* For GPIO IRQs. */
#define NR_GPIO0_IRQS			(128)
#define NR_GPIO1_IRQS			(115)
#define NR_GPIO_IRQS			(NR_GPIO0_IRQS + NR_GPIO1_IRQS)
#define IRQ_GPIO(x) 			(IRQ_AP_PWR(NR_AP_PWR_IRQS) + (x))
#define IRQ_TO_GPIO(x)			((x) - IRQ_GPIO(0))

/* For AP-CP IRQs. */
#if defined(CONFIG_CPU_LC1810)
#define NR_CP_IRQS			(8)
#elif defined(CONFIG_CPU_LC1813)
#define NR_CP_IRQS			(32)
#endif
#define IRQ_CP(x)			(IRQ_GPIO(NR_GPIO_IRQS) + (x))
#define IRQ_TO_CP(x)			((x) - IRQ_CP(0))

/* NR_IRQS*/
#define NR_IRQS				(NR_COMIP_IRQS + NR_AP_PWR_IRQS + NR_GPIO_IRQS + NR_CP_IRQS)

#define IRQ_LCDC0			INT_LCDC0

#if defined(CONFIG_CPU_LC1813)
#else
#define IRQ_LCDC1			INT_LCDC1
#endif
#endif /* __MACH_COMIP_IRQS_H */
