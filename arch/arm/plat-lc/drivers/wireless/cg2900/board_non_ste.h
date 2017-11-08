#ifndef __Board_H
#define __Board_H


#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <plat/clock.h>
#include "include/cg2900.h"

#include <plat/mfp.h>

/* Sleep timeout in milli seconds */
#define CG29XX_SLEEP_TIMEOUT_VALUE  200 // 200msec (0 is to disable sleep)

#ifndef NON_STE_PLATFORM
#define NON_STE_PLATFORM
#endif


/* + GPIOs has to be changed as per board specification */
/* + Power Enable for the chip through GPIO
*/
#define GPIO_TO_IRQ_CONV_FN 	gpio_to_irq 
#define CG29XX_HOST_UART_HIGH_BAUD_RATE 	 3000000

/* + Reset the chip through GPIO  */
#define CG29XX_GPIO_GBF_RESET  mfp_to_gpio(MFP_PIN_GPIO(1))
#define CG29XX_GPIO_PMU_EN	mfp_to_gpio(MFP_PIN_GPIO(2))
#define CG29XX_GPIO_WAKE_UP	mfp_to_gpio(MFP_PIN_GPIO(10))

/* UART resources */
#define CG29XX_UART_RTS_GPIO   MFP_PIN_GPIO(157)
#define CG29XX_UART_CTS_GPIO   MFP_PIN_GPIO(156)
#define CG29XX_UART_TXD_GPIO   MFP_PIN_GPIO(154)
#define CG29XX_UART_RXD_GPIO   MFP_PIN_GPIO(155)

#define MFP_GPIO_UART_RTS MFP_GPIO157_UART1_RTS
#define MFP_GPIO_UART_CTS MFP_GPIO156_UART1_CTS
#define MFP_GPIO_UART_TXD MFP_GPIO154_UART1_TX
#define MFP_GPIO_UART_RXD MFP_GPIO155_UART1_RX

/* CTS pin though GPIO: used for interrupt  	*/
#define CG29XX_BT_CTS_GPIO   mfp_to_gpio(CG29XX_UART_CTS_GPIO)
#define CG29XX_BT_CTS_IRQ   gpio_to_irq(mfp_to_gpio(CG29XX_UART_CTS_GPIO))

/* + UART pins: Sample platform uses normal GPIO mode to disable */
#define CG29XX_UART_RTS   mfp_to_gpio(CG29XX_UART_RTS_GPIO)
#define CG29XX_UART_CTS   mfp_to_gpio(CG29XX_UART_CTS_GPIO)
#define CG29XX_UART_TXD   mfp_to_gpio(CG29XX_UART_TXD_GPIO)
#define CG29XX_UART_RXD   mfp_to_gpio(CG29XX_UART_RXD_GPIO)


int dcg2900_disable_uart(struct cg2900_chip_dev *dev);
int dcg2900_enable_uart(struct cg2900_chip_dev *dev);
#define  IRQF_TRIGGER_LEVEL IRQF_TRIGGER_LOW

#endif /* __Board_H*/

