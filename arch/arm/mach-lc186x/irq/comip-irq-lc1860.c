/*
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2010-2019	LeadCoreTech Corp.
**
**	PURPOSE: This files contains irq driver. 
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2013-12-8	zhaoyuan		created
**
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/chained_irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <plat/hardware.h>
#include <mach/irqs.h>
#include <mach/comip-irq.h>
#define	APA7_INTR_STA_NUM		2
#define	APA7_INTR_SRC_BITS		32

static DEFINE_RAW_SPINLOCK(irq_cp_lock);

static inline int irq_set_wake_common(int irq_start, int irq_end,
	int irq_src, int trigger, int enable)
{
	struct irq_desc *desc;
	int irq;
	int ret = 0;

	for (irq  = irq_start; irq < irq_end; ++irq) {
		desc = irq_to_desc(irq);
		if (!desc || irq == irq_src)
			continue;

		if (irqd_is_wakeup_set(&desc->irq_data)) {
			ret = 1;
			break;
		}
	}

	if (ret)
		return 0;

	desc = irq_to_desc(trigger);
	if (desc && desc->irq_data.chip && desc->irq_data.chip->irq_set_wake)
		ret = desc->irq_data.chip->irq_set_wake(&desc->irq_data, enable);

	return ret;

}
static int comip_ap_pwr_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	if (!d)
		return -EINVAL;

	return irq_set_wake_common(IRQ_AP_PWR(0), IRQ_AP_PWR(NR_AP_PWR_IRQS),
			d->irq, INT_AP_PWR, enable);
}

/*
 * AP-CP interrupt manage
 */
static int comip_cp_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	if (!d)
		return -EINVAL;
	return irq_set_wake_common(IRQ_CP(0), IRQ_CP(NR_CP_IRQS),
			d->irq, INT_MAILBOX, enable);
}

static void comip_cp_irq_ack(struct irq_data *d)
{

}

static void comip_cp_irq_mask(struct irq_data *d)
{
	int cp_irq = IRQ_TO_CP(d->irq);
	unsigned long val;
	unsigned int addr;

	if(cp_irq < APA7_INTR_SRC_BITS)
		addr = TOP_MAIL_APA7_INTR_SRC_EN0;
	else{
		addr = TOP_MAIL_APA7_INTR_SRC_EN1;
		cp_irq -= APA7_INTR_SRC_BITS;
	}
	raw_spin_lock(&irq_cp_lock);
	val = readl(io_p2v(addr));
	val &= ~(1 << cp_irq);
	writel(val, io_p2v(addr));
	raw_spin_unlock(&irq_cp_lock);
}

static void comip_cp_irq_unmask(struct irq_data *d)
{
	int cp_irq = IRQ_TO_CP(d->irq);
	unsigned long val;
	unsigned int addr;

	if(cp_irq < APA7_INTR_SRC_BITS)
		addr = TOP_MAIL_APA7_INTR_SRC_EN0;
	else{
		addr = TOP_MAIL_APA7_INTR_SRC_EN1;
		cp_irq -= APA7_INTR_SRC_BITS;
	}
	raw_spin_lock(&irq_cp_lock);
	val = readl(io_p2v(addr));
	val |= (1 << cp_irq);
	writel(val, io_p2v(addr));
	raw_spin_unlock(&irq_cp_lock);

}

static struct irq_chip comip_cp_irq_chip = {
	.name		= "CTL CP",
	.irq_ack	= comip_cp_irq_ack,
	.irq_mask	= comip_cp_irq_mask,
	.irq_unmask	= comip_cp_irq_unmask,
	.irq_set_wake	= comip_cp_irq_set_wake,
	.irq_enable	= comip_cp_irq_unmask,
	.irq_disable	= comip_cp_irq_mask,
};

static void comip_cp_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned long status;
	int bit;
	int i;
	unsigned int addr;

	chained_irq_enter(chip, desc);

	for(i = 0; i < APA7_INTR_STA_NUM; i++){
		addr = TOP_MAIL_APA7_INTR_STA0 + 0x4 * i;
		status = readl(io_p2v(addr));
		for_each_set_bit(bit, &status, APA7_INTR_SRC_BITS) {
			/* Clear interrupt. */
			writel(1 << bit, io_p2v(addr));

			/* Check interrupt status. */
			while(readl(io_p2v(addr)) & (1 << bit)) {
				writel(1 << bit, io_p2v(addr));
				//printk(KERN_DEBUG "status(%lx):CP2AP unhandle irq(%d)!\n", status, bit);
			}

			/* Handle irq. */
			generic_handle_irq(IRQ_CP(bit + i * APA7_INTR_SRC_BITS));
		}
	}
	chained_irq_exit(chip, desc);
}

static void comip_cp_irq_init(void)
{
	int irq;

	/* Enable CP->A7 interrupt. */
	writel((1 << 0), io_p2v(TOP_MAIL_APA7_INTR_EN));

	for (irq  = IRQ_CP(0); irq < IRQ_CP(NR_CP_IRQS); irq++) {
		irq_set_chip_and_handler(irq, &comip_cp_irq_chip,
					 handle_simple_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	irq_set_chained_handler(INT_MAILBOX, comip_cp_irq_handler);

	if(irq_set_irq_wake(INT_MAILBOX, 1))
		pr_err("set irq %d wake up error\n", INT_MAILBOX);
}
void comip_cp_irq_enable(void)
{
	/* Enable A7->CP interrupt. */
	writel((1 << 0), io_p2v(TOP_MAIL_CPA7_INTR_EN));
	/* Enable cp irq 0*/
	writel((1 << 0), io_p2v(TOP_MAIL_CPA7_INTR_SRC_EN0));
}
EXPORT_SYMBOL(comip_cp_irq_enable);

/* A7 send irq to CP/DSP */
void comip_trigger_cp_irq(int irq)
{
	irq = IRQ_TO_CP(irq);
#ifdef CONFIG_BRIDGE_DEBUG_LOOP_TEST
	writel(irq, io_p2v(TOP_MAIL_APA7_INTR_SET));
#else
	writel(irq, io_p2v(TOP_MAIL_CPA7_DSP_INTR_SET));
#endif
}
EXPORT_SYMBOL(comip_trigger_cp_irq);
/*
 * AP_PWR interrupt manage
 */
static void comip_ap_pwr_irq_ack(struct irq_data *d)
{

}

static void comip_ap_pwr_irq_mask(struct irq_data *d)
{
	int pwr_irq = IRQ_TO_AP_PWR(d->irq);
	unsigned long val;
	val = readl(io_p2v(AP_PWR_INTEN_A7));
	val &= ~(1 << pwr_irq);
	writel(val, io_p2v(AP_PWR_INTEN_A7));
}

static void comip_ap_pwr_irq_unmask(struct irq_data *d)
{
	int pwr_irq = IRQ_TO_AP_PWR(d->irq);
	unsigned long val;

	val = readl(io_p2v(AP_PWR_INTEN_A7));
	val |= (1 << pwr_irq);
	writel(val, io_p2v(AP_PWR_INTEN_A7));

}

static struct irq_chip comip_ap_pwr_irq_chip = {
	.name		= "AP PWR",
	.irq_ack	= comip_ap_pwr_irq_ack,
	.irq_mask	= comip_ap_pwr_irq_mask,
	.irq_unmask	= comip_ap_pwr_irq_unmask,
	.irq_set_wake	= comip_ap_pwr_irq_set_wake,
	.irq_enable	= comip_ap_pwr_irq_unmask,
	.irq_disable	= comip_ap_pwr_irq_mask,
};

static void comip_ap_pwr_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned long status;
	int bit;

	chained_irq_enter(chip, desc);

	status = readl(io_p2v(AP_PWR_INTST_A7));

	for_each_set_bit(bit, &status, NR_AP_PWR_IRQS) {
		/* Clear interrupt. */
		writel(1 << bit, io_p2v(AP_PWR_INTST_A7));

		/* Handle irq. */
		generic_handle_irq(IRQ_AP_PWR(bit));
	}

	chained_irq_exit(chip, desc);
}

static void comip_ap_pwr_irq_init(void)
{
	int irq;

	for (irq  = IRQ_AP_PWR(0); irq < IRQ_AP_PWR(NR_AP_PWR_IRQS); irq++) {
		irq_set_chip_and_handler(irq, &comip_ap_pwr_irq_chip,
					 handle_simple_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	irq_set_chained_handler(INT_AP_PWR, comip_ap_pwr_irq_handler);

	if(irq_set_irq_wake(INT_AP_PWR, 1))
		pr_err("set irq %d wake up error\n", INT_AP_PWR);
}
static int comip_gic_set_wake(struct irq_data *d, unsigned int on)
{
	return 0;
}
void __init comip_irq_init(void)
{
#ifdef CONFIG_BL_SWITCHER
	gic_set_dist_physaddr(AP_GICD_BASE);
#endif
	gic_init(0, 29, __io_address(AP_GICD_BASE),
		__io_address(AP_GICC_BASE));
	gic_arch_extn.irq_set_wake = comip_gic_set_wake;
	comip_cp_irq_init();
	comip_ap_pwr_irq_init();
}
