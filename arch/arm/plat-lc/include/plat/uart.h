#ifndef __ASM_ARCH_PLAT_UART_H
#define __ASM_ARCH_PLAT_UART_H

#define COMIP_UART_TX_USE_DMA				0x00000001
#define COMIP_UART_RX_USE_DMA				0x00000002
#define COMIP_UART_SUPPORT_IRDA				0x00000004
#define COMIP_UART_SUPPORT_MCTRL			0x00000008
#define COMIP_UART_DISABLE_MSI				0x00000010
#define COMIP_UART_USE_WORKQUEUE			0x00000100
#define COMIP_UART_RX_USE_GPIO_IRQ			0x00010000

struct comip_uart_platform_data {
	unsigned int flags;
	unsigned int rx_gpio;
	int (*power)(void *param, int onoff);
	void (*pin_switch)(int enable);
};

#endif/* __ASM_ARCH_PLAT_UART_H */

