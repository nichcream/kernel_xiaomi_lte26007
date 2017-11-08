/* arch/arm/mach-comip/comip-irq.c
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
**	1.0.0		2011-12-8	gaobing		created
**
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/hardware/gic.h>
#include <plat/hardware.h>
#include <mach/irqs.h>
#include <mach/comip-irq.h>

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
/*
 * AP-CP interrupt manage
 */
static void comip_cp_irq_ack(struct irq_data *d)
{

}

static void comip_cp_irq_mask(struct irq_data *d)
{
	int cp_irq = IRQ_TO_CP(d->irq);
	unsigned long val;

	val = readl(io_p2v(CTL_ARM02A9_INTR_EN));
	val &= ~(1 << cp_irq);
	writel(val, io_p2v(CTL_ARM02A9_INTR_EN));
}

static void comip_cp_irq_unmask(struct irq_data *d)
{
	int cp_irq = IRQ_TO_CP(d->irq);
	unsigned long val;

	val = readl(io_p2v(CTL_ARM02A9_INTR_EN));
	val |= (1 << cp_irq);	
	writel(val, io_p2v(CTL_ARM02A9_INTR_EN));

}

static int comip_cp_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	if (!d)
		return -EINVAL;

	return irq_set_wake_common(IRQ_CP(0), IRQ_CP(NR_CP_IRQS),
			d->irq, INT_CTL, enable);
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

	chained_irq_enter(chip, desc);

	status = readl(io_p2v(CTL_ARM02A9_INTR_STA));
	for_each_set_bit(bit, &status, NR_CP_IRQS) {
		/* Clear interrupt. */
		writel(1 << bit, io_p2v(CTL_ARM02A9_INTR_STA));

		/* Check interrupt status. */
		while(readl(io_p2v(CTL_ARM02A9_INTR_STA)) & (1 << bit)) {
			writel(1 << bit, io_p2v(CTL_ARM02A9_INTR_STA));
			//printk(KERN_DEBUG "status(%lx):CP2AP unhandle irq(%d)!\n", status, bit);
		}

		/* Handle irq. */
		generic_handle_irq(IRQ_CP(bit));
	}

	chained_irq_exit(chip, desc);
}

static void comip_cp_regs_init(void)
{
	/* Use default priority. */
#if 0
	writel(0, io_p2v(CTL_ISP_ICM_PRIOR));
	writel(0, io_p2v(CTL_LCDC_ICM_PRIOR));
	writel(0, io_p2v(CTL_DATA_APB_ICM_PRIOR));
	writel(0, io_p2v(CTL_TOPBRG_ICM_PRIOR));
	writel(0, io_p2v(CTL_NFC_ICM_PRIOR));
	writel(0, io_p2v(CTL_M0_IRAM_ICM_PRIOR));
	writel(0, io_p2v(CTL_ICM_PRIOR));
	writel(0, io_p2v(CTL_AP_DMA_ICM_PRIOR));
	writel(0, io_p2v(CTL_APB_ICM_PRIOR));
	writel(0, io_p2v(CTL_ACP_MST0_ICM_PRIOR));
	writel(0, io_p2v(CTL_ACP_CIPHER_SDIO1_ICM_PRIOR));
	writel(0, io_p2v(CTL_ACP_MST3_ICM_PRIOR));
	writel(0, io_p2v(CTL_ACP_SDIO23_ICM_PRIOR));
	writel(0xffff, io_p2v(CTL_ACP_MH2X_CH_PRIOR));
	writel(0xfff, io_p2v(CTL_USB_MH2X_PRIOR));

	/* Enable ctl low power mode. */
	writel(0xff01, io_p2v(CTL_LP_MODE_CTRL));

	/* Enable dmac low power mode. */
	writel(0x1, io_p2v(CTL_AP_DMAC_LP_EN));

	/* Enable MH2X low power mode. */
	writel(0x7f, io_p2v(CTL_MH2X_CH_LP_EN));
#endif
}

static void comip_cp_irq_init(void)
{
	int irq;

	/* Enable CP->A9 interrupt. */
	writel((1 << 16), io_p2v(CTL_ARM02A9_INTR_EN));

	for (irq  = IRQ_CP(0); irq < IRQ_CP(NR_CP_IRQS); irq++) {
		irq_set_chip_and_handler(irq, &comip_cp_irq_chip,
					 handle_simple_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	irq_set_chained_handler(INT_CTL, comip_cp_irq_handler);

	/* Set priority. */
	comip_cp_regs_init();
}

/* A9 send irq to CP */
void comip_trigger_cp_irq(int irq)
{
	irq = IRQ_TO_CP(irq);
	writel(irq, io_p2v(CTL_AP2ARM0_INTR_SET));
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

	val = readl(io_p2v(AP_PWR_INTEN_A9));
	val &= ~(1 << pwr_irq);
	writel(val, io_p2v(AP_PWR_INTEN_A9));
}

static void comip_ap_pwr_irq_unmask(struct irq_data *d)
{
	int pwr_irq = IRQ_TO_AP_PWR(d->irq);
	unsigned long val;

	val = readl(io_p2v(AP_PWR_INTEN_A9));
	val |= (1 << pwr_irq);
	writel(val, io_p2v(AP_PWR_INTEN_A9));

}

static int comip_ap_pwr_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	if (!d)
		return -EINVAL;

	return irq_set_wake_common(IRQ_AP_PWR(0), IRQ_AP_PWR(NR_AP_PWR_IRQS),
			d->irq, INT_AP_PWR, enable);
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

	status = readl(io_p2v(AP_PWR_INTST_A9));

	for_each_set_bit(bit, &status, NR_AP_PWR_IRQS) {
		/* Clear interrupt. */
		writel(1 << bit, io_p2v(AP_PWR_INTST_A9));

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
}

static int comip_gic_set_wake(struct irq_data *d, unsigned int on)
{
	unsigned int irq;
	unsigned int val;
	void __iomem *addr;

	if (!d || d->irq < INT_PRI_BASE || d->irq > NR_COMIP_IRQS)
		return -EINVAL;

	irq = d->irq - INT_PRI_BASE;
	if (irq < 32)
		addr = io_p2v(AP_PWR_A9INTIN_MK0);
	else
		addr = io_p2v(AP_PWR_A9INTIN_MK1);

	val = readl(addr);
	if (on)
		val &= ~(1 << irq % 32);
	else
		val |= (1 << irq % 32);
	writel(val, addr);

	return 0;
}

static void comip_extn_gic_init(void)
{
	writel(0xffffffff, io_p2v(AP_PWR_A9INTIN_MK0));
	writel(0xffffffff, io_p2v(AP_PWR_A9INTIN_MK1));

	gic_arch_extn.irq_set_wake = comip_gic_set_wake;
}

void __init comip_irq_init(void)
{
	gic_init(0, 29, __io_address(GIC_DIST_BASE),
	__io_address(CPU_CORE_BASE + 0x100));

	comip_extn_gic_init();
	comip_cp_irq_init();
	comip_ap_pwr_irq_init();
}
