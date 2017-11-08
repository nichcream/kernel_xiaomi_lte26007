#ifndef __ASM_ARCH_MACH_GPIO_H
#define __ASM_ARCH_MACH_GPIO_H

#include <linux/init.h>
#include <mach/irqs.h>

#define gpio_get_value			__gpio_get_value
#define gpio_set_value			__gpio_set_value
#define gpio_cansleep			__gpio_cansleep

#define gpio_to_irq(gpio)		IRQ_GPIO(gpio)
#define irq_to_gpio(irq)		IRQ_TO_GPIO(irq)
#define gpio_to_bank(gpio)		((gpio) >> 7)

#define GPIO_BANK0_BASE			(GPIO0_BASE)
#define GPIO_BANK1_BASE			(GPIO1_BASE)

#define GPIO_BANK0_NUM			(128)
#define GPIO_BANK1_NUM			(115)
#define GPIO_NUM			(GPIO_BANK0_NUM + GPIO_BANK1_NUM)

#define GPIO_IRQ_CLKSRC_32K		(0)
#define GPIO_IRQ_CLKSRC_PCLK		(1)

extern void gpio_set_irq_clksrc(unsigned int gpio, int clksrc);
extern int gpio_get_irq_clksrc(unsigned int gpio);

#endif /* __ASM_ARCH_MACH_GPIO_H */
