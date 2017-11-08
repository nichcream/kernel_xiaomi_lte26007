#ifndef __ASM_ARCH_REGS_GPIO_H
#define __ASM_ARCH_REGS_GPIO_H

#define GPIO_REG(base, gpio, num)	(base + ((gpio) / num) * 4)

/* Registers. */
#define GPIO_PORT_DR(gpio) 		GPIO_REG(0x00, gpio, 16)
#define GPIO_PORT_DDR(gpio) 		GPIO_REG(0x40, gpio, 16)
#define GPIO_EXT_PORT(gpio)		GPIO_REG(0x80, gpio, 32)
#define GPIO_DEBOUNCE(gpio)		GPIO_REG(0x1A0, gpio, 16)
#define GPIO_INTR_CTRL(gpio)		GPIO_REG(0xA0, gpio, 4)
#define GPIO_INTR_RAW(gpio)		GPIO_REG(0x1E0, gpio, 32)
#define GPIO_INTR_CLR(gpio)		GPIO_REG(0x200, gpio, 32)
#define GPIO_INTR_MASK(gpio)		GPIO_REG(0x340, gpio, 16)
#define GPIO_INTR_STATUS(gpio)		GPIO_REG(0x380, gpio, 32)

#endif	/* __ASM_ARCH_REGS_GPIO_H */
