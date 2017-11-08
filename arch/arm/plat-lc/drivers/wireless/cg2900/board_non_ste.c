// #include "board.h"
#include "board_non_ste.h"
#ifndef GPIO_AMBA_DEVICE


#define CG29XX_GPIO_INPUT 	0
#define CG29XX_GPIO_OUTPUT 	1

/*
* SAMPLE ONLY: Based on GPIO model

 Enable:
	CTS: input : PULL Up
	RTS: Output: high
	RXD: input : Pull up
	TXD: Output: High

 Disable:
CTS: input : PULL Up
RTS: Output: high
RXD: input : Pull up
TXD: Output: Low
*/


int dcg2900_enable_uart (struct cg2900_chip_dev *dev)
{
	//printk("++ %s \n",__func__);
	comip_mfp_config(CG29XX_UART_RTS_GPIO, MFP_GPIO_UART_RTS);
	comip_mfp_config(CG29XX_UART_CTS_GPIO, MFP_GPIO_UART_CTS);
	comip_mfp_config(CG29XX_UART_RXD_GPIO, MFP_GPIO_UART_RXD);
	comip_mfp_config(CG29XX_UART_TXD_GPIO, MFP_GPIO_UART_TXD);
	//printk("-- %s \n",__func__);
	return 0;
}

int dcg2900_disable_uart(struct cg2900_chip_dev *dev)
{
	//printk("++ %s \n",__func__);
	//stephen_debug + move TX to first
	comip_mfp_config(CG29XX_UART_TXD_GPIO, MFP_PIN_MODE_GPIO);
	gpio_request(CG29XX_UART_TXD, "UART TX");
	gpio_direction_output(CG29XX_UART_TXD, 0);
	gpio_free(CG29XX_UART_TXD);
	//printk("HOST_TX= %d, \n", gpio_get_value(CG29XX_UART_TXD_GPIO));

	comip_mfp_config(CG29XX_UART_RTS_GPIO, MFP_PIN_MODE_GPIO);
	gpio_request(CG29XX_UART_RTS, "UART RTS");
	gpio_direction_output(CG29XX_UART_RTS, 1);
	gpio_free(CG29XX_UART_RTS);
	//printk("HOST_RTS= %d, \n", gpio_get_value(CG29XX_UART_RTS_GPIO));


	comip_mfp_config(CG29XX_UART_CTS_GPIO, MFP_PIN_MODE_GPIO);
	gpio_request(CG29XX_UART_CTS, "UART CTS");
	gpio_direction_input(CG29XX_UART_CTS);
	gpio_free(CG29XX_UART_CTS);
	//printk("HOST_CTS= %d, \n", gpio_get_value(CG29XX_UART_CTS_GPIO));

	comip_mfp_config(CG29XX_UART_RXD_GPIO, MFP_PIN_MODE_GPIO);
	gpio_request(CG29XX_UART_RXD, "UART RX");
	gpio_direction_input(CG29XX_UART_RXD);
	gpio_free(CG29XX_UART_RXD);
	//printk("HOST_RX= %d, \n", gpio_get_value(CG29XX_UART_RXD_GPIO));

	//printk("-- %s \n",__func__);
	return 0;
}

#endif
