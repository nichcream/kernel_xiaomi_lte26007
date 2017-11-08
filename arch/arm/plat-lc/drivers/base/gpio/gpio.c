/* arch/arm/mach-comip/gpio.c
**
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
** PURPOSE: This files contains the driver GPIO controller.
**
** CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2011-12-08	gaobing		created
**
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/irqchip/chained_irq.h>
#include <plat/hardware.h>

struct comip_gpio_chip {
	struct gpio_chip chip;
	spinlock_t lock;
	void __iomem *regbase;
	unsigned int irq;
	unsigned int irq_clksrc[4];
};
static int comip_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	struct comip_gpio_chip *comip;
	void __iomem *reg;
	unsigned int bit;
	unsigned int val;

	comip = container_of(chip, struct comip_gpio_chip, chip);
	reg = comip->regbase + GPIO_PORT_DDR(offset);
	bit = offset % 16;

	val = readl(reg) & (1 << bit);
	return !val;
}
static int comip_gpio_set_debounce(struct gpio_chip *chip,
					unsigned offset, unsigned debounce)
{
	struct comip_gpio_chip *comip;
	void __iomem *reg;
	unsigned int bit;
	unsigned int val;

	comip = container_of(chip, struct comip_gpio_chip, chip);
	reg = comip->regbase + GPIO_DEBOUNCE(offset);
	bit = offset % 16;

	val = (1 << (bit + 16));
	if(debounce)
		val |= (1 << bit);
	writel(val, reg);

	return 0;
}

static int comip_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct comip_gpio_chip *comip;
	void __iomem *reg;
	unsigned int bit;
	unsigned int val;

	comip = container_of(chip, struct comip_gpio_chip, chip);
	reg = comip->regbase + GPIO_EXT_PORT(offset);
	bit = offset % 32;

	val = readl(reg) & (1 << bit);
	return !!val;
}

static void comip_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct comip_gpio_chip *comip;
	void __iomem *reg;
	unsigned int bit;
	unsigned int val;

	comip = container_of(chip, struct comip_gpio_chip, chip);
	reg = comip->regbase + GPIO_PORT_DR(offset);
	bit = offset % 16;

	val = (1 << (bit + 16));
	if(value)
		val |= 1 << bit;
	writel(val, reg);
}

static int comip_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct comip_gpio_chip *comip;
	void __iomem *reg;
	unsigned int bit;
	unsigned int val;

	comip = container_of(chip, struct comip_gpio_chip, chip);
	reg = comip->regbase + GPIO_PORT_DDR(offset);
	bit = offset % 16;

	val = (1 << (bit + 16));
	writel(val, reg);

	return 0;
}

static int comip_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	struct comip_gpio_chip *comip;
	void __iomem *reg;
	unsigned int bit;
	unsigned int val;

	/* Set output value. */
	comip_gpio_set(chip, offset, value);

	/* Set direction. */
	comip = container_of(chip, struct comip_gpio_chip, chip);
	reg = comip->regbase + GPIO_PORT_DDR(offset);
	bit = offset % 16;

	val = (1 << (bit + 16)) | (1 << bit);
	writel(val, reg);

	return 0;
}

static struct comip_gpio_chip comip_gpio_chips[] = {
	[0] = {
		.regbase = io_p2v(GPIO_BANK0_BASE),
		.chip = {
			.label = "gpio-0",
			.get_direction = comip_gpio_get_direction,
			.direction_input = comip_gpio_direction_input,
			.direction_output = comip_gpio_direction_output,
			.get = comip_gpio_get,
			.set = comip_gpio_set,
			.set_debounce = comip_gpio_set_debounce,
			.base = 0,
			.ngpio = GPIO_BANK0_NUM,
		},
		.irq_clksrc = {0, 0, 0, 0},
		.irq = INT_GPIO,
	},
#ifdef GPIO_BANK1_NUM
	[1] = {
		.regbase = io_p2v(GPIO_BANK1_BASE),
		.chip = {
			.label = "gpio-1",
			.get_direction = comip_gpio_get_direction,
			.direction_input = comip_gpio_direction_input,
			.direction_output = comip_gpio_direction_output,
			.get = comip_gpio_get,
			.set = comip_gpio_set,
			.set_debounce = comip_gpio_set_debounce,
			.base = GPIO_BANK0_NUM,
			.ngpio = GPIO_BANK1_NUM,
		},
		.irq_clksrc = {0, 0, 0, 0},
		.irq = INT_GPIO1,
	},
#endif
};

static inline struct comip_gpio_chip *comip_gpio_to_chip(unsigned gpio)
{
	if (gpio >= GPIO_NUM)
		return 0;

	return &comip_gpio_chips[gpio_to_bank(gpio)];
}

void gpio_set_irq_clksrc(unsigned gpio, int clksrc)
{
	struct comip_gpio_chip *comip = comip_gpio_to_chip(gpio);
	unsigned int offset = gpio - comip->chip.base;
	unsigned long flags;

	if (gpio >= GPIO_NUM)
		return;

	spin_lock_irqsave(&comip->lock, flags);

	if (clksrc)
		comip->irq_clksrc[offset / 32] |= (1 << (offset % 32));
	else
		comip->irq_clksrc[offset / 32] &= ~(1 << (offset % 32));

	spin_unlock_irqrestore(&comip->lock, flags);
}
EXPORT_SYMBOL(gpio_set_irq_clksrc);

int gpio_get_irq_clksrc(unsigned gpio)
{
	struct comip_gpio_chip *comip = comip_gpio_to_chip(gpio);
	unsigned int offset = gpio - comip->chip.base;
	unsigned long flags;
	int clksrc;

	if (gpio >= GPIO_NUM)
		return 0;

	spin_lock_irqsave(&comip->lock, flags);

	clksrc = comip->irq_clksrc[offset / 32] & (1 << (offset % 32));

	spin_unlock_irqrestore(&comip->lock, flags);

	return !!clksrc;
}
EXPORT_SYMBOL(gpio_get_irq_clksrc);

static void comip_gpio_irq_ack(struct irq_data *d)
{
	int gpio = irq_to_gpio(d->irq);
	struct comip_gpio_chip *comip = comip_gpio_to_chip(gpio);
	unsigned int offset = gpio - comip->chip.base;
	unsigned int val = (1 << (offset % 32));
	void __iomem *reg = (comip->regbase + GPIO_INTR_CLR(offset));

	writel(val, reg);
}

static void comip_gpio_irq_mask(struct irq_data *d)
{
	int gpio = irq_to_gpio(d->irq);
	struct comip_gpio_chip *comip = comip_gpio_to_chip(gpio);
	unsigned int offset = gpio - comip->chip.base;
	unsigned int bit = offset % 16;
	unsigned int val = (1 << (bit + 16)) | (1 << bit);
	void __iomem *reg = comip->regbase + GPIO_INTR_MASK(offset);

	writel(val, reg);
}

static void comip_gpio_irq_unmask(struct irq_data *d)
{
	int gpio = irq_to_gpio(d->irq);
	struct comip_gpio_chip *comip = comip_gpio_to_chip(gpio);
	unsigned int offset = gpio - comip->chip.base;
	unsigned int bit = offset % 16;
	unsigned int val = (1 << (bit + 16));
	void __iomem *reg = comip->regbase + GPIO_INTR_MASK(offset);

	writel(val, reg);
}

static int comip_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	int gpio = irq_to_gpio(d->irq);
	struct comip_gpio_chip *comip = comip_gpio_to_chip(gpio);
	unsigned int offset = gpio - comip->chip.base;
	unsigned int trigger = -1;
	unsigned int add_bit = 0;
	unsigned int bit;
	unsigned int val;
	void __iomem *reg;
	unsigned long flags;

	if (gpio >= GPIO_NUM)
		return 0;


	spin_lock_irqsave(&comip->lock, flags);

	if (comip->irq_clksrc[offset / 32] & (1 << (offset % 32)))
		add_bit = 3;

	if ((type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
		trigger = 4 + add_bit;
	else if (type & IRQ_TYPE_EDGE_RISING)
		trigger = 2 + add_bit;
	else if (type & IRQ_TYPE_EDGE_FALLING)
		trigger = 3 + add_bit;
	else if (type & IRQ_TYPE_LEVEL_HIGH)
		trigger = 0;
	else if (type & IRQ_TYPE_LEVEL_LOW)
		trigger = 1;

	if (trigger >= 0) {
		reg = comip->regbase + GPIO_INTR_CTRL(offset);
		bit = offset % 4;
		val = (0x1 << (16 + bit)) | (((trigger << 1) + 1) << bit * 4);
		writel(val, reg);
	}

	spin_unlock_irqrestore(&comip->lock, flags);

	return 0;
}

static int comip_gpio_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	struct irq_desc *desc;
	struct comip_gpio_chip *comip;
	int irq;
	int group;
	int ret = 0;

	if (!d)
		return -EINVAL;

	if (d->irq < gpio_to_irq(GPIO_BANK0_NUM)) {
		group = gpio_to_irq(GPIO_BANK0_NUM);
		irq = gpio_to_irq(0);
	} else if (d->irq < gpio_to_irq(GPIO_NUM)) {
		group = gpio_to_irq(GPIO_NUM);
		irq = gpio_to_irq(GPIO_BANK0_NUM);
	} else
		return -EINVAL;

	for (; irq < group; ++irq) {
		desc = irq_to_desc(irq);
		if (!desc || irq == d->irq)
			continue;

		if (irqd_is_wakeup_set(&desc->irq_data)) {
			ret = 1;
			break;
		}
	}

	if (ret)
		return 0;

	comip = d->chip_data;
	desc = irq_to_desc(comip->irq);
	if (desc && desc->irq_data.chip && desc->irq_data.chip->irq_set_wake)
		ret = desc->irq_data.chip->irq_set_wake(&desc->irq_data, enable);

	return ret;
}

static struct irq_chip comip_gpio_irq_chip = {
	.name		= "GPIO",
	.irq_enable = comip_gpio_irq_unmask,
	.irq_disable = comip_gpio_irq_mask,
	.irq_ack	= comip_gpio_irq_ack,
	.irq_mask	= comip_gpio_irq_mask,
	.irq_unmask	= comip_gpio_irq_unmask,
	.irq_set_type	= comip_gpio_irq_set_type,
	.irq_set_wake	= comip_gpio_irq_set_wake,
};

static void comip_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct comip_gpio_chip *comip;
	unsigned long status_val;
	void __iomem *status_reg;
	void __iomem *clear_reg;
	void __iomem *reg;
	unsigned int unmasked = 0;
	unsigned int offset;
	unsigned int type;
	unsigned int i, j;

	chained_irq_enter(chip, desc);

	comip = irq_get_handler_data(irq);
	for (i = 0; i < DIV_ROUND_UP(GPIO_BANK0_NUM, 32); i++) {
		status_reg = comip->regbase + GPIO_INTR_STATUS(0) + (i * 4);
		clear_reg = comip->regbase + GPIO_INTR_CLR(0) + (i * 4);
		status_val = readl(status_reg);
		for_each_set_bit(j, &status_val, 32) {
			writel(1 << j, clear_reg);
			offset = i * 32 + j;

			/* if gpio is edge triggered, clear condition
			 * before executing the hander so that we don't
			 * miss edges
			 */
			reg = comip->regbase + GPIO_INTR_CTRL(offset);
			type = (readl(reg) >> ((offset % 4) + 1)) & 0x7;
			if (type >= 2) {
				unmasked = 1;
				chained_irq_exit(chip, desc);
			}

			generic_handle_irq(gpio_to_irq(comip->chip.base + offset));
		}
	}

	if (!unmasked)
		chained_irq_exit(chip, desc);
}

/* This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key gpio_lock_class;

void __init comip_gpio_init(int enable)
{
	struct comip_gpio_chip *comip;
	unsigned int i, j;
	void __iomem *reg;
	int irq;

	for (i = 0 ; i < ARRAY_SIZE(comip_gpio_chips); i++) {
		comip = &comip_gpio_chips[i];

		/* Mask all gpio interrupt. */
		for (j = 0; j < DIV_ROUND_UP(GPIO_BANK0_NUM, 16); j++) {
			reg = comip->regbase + GPIO_INTR_MASK(0) + (j * 4);
			writel(~0x0, reg);
		}

		/* Disable all gpio interrupt. */
		for (j = 0; j < DIV_ROUND_UP(GPIO_BANK0_NUM, 4); j++) {
			reg = comip->regbase + GPIO_INTR_CTRL(0) + (j * 4);
			writel(0x0, reg);
		}

		/* Add chips. */
		gpiochip_add(&comip->chip);
		spin_lock_init(&comip->lock);
		irq_set_chained_handler(comip->irq, comip_gpio_irq_handler);
		irq_set_handler_data(comip->irq, comip);
		/* enable gpio irq wakeup */
		if(enable)
			if(irq_set_irq_wake(comip->irq, 1))
				pr_err("set irq %d wake up error\n", comip->irq);
	}

	for (irq  = gpio_to_irq(0); irq < gpio_to_irq(GPIO_NUM); irq++) {
		irq_set_lockdep_class(irq, &gpio_lock_class);
		irq_set_chip_data(irq, comip_gpio_to_chip(irq_to_gpio(irq)));
		irq_set_chip_and_handler(irq, &comip_gpio_irq_chip,
					 handle_simple_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
}

