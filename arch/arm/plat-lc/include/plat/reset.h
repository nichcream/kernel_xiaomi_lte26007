#ifndef __ARCH_ARM_PLAT_RESET_H
#define __ARCH_ARM_PLAT_RESET_H

/* Reset flags */
#define RST_HARDWARE_CLEAR	(1 << 0)
#define RST_WRITE_ENABLE	(1 << 1)

struct rst {
	const char *name;
	unsigned int flags;
	void __iomem *rst_reg;
	unsigned int rst_bit;
};

#define RST_VAR(n, f, reg, bit) \
	{ \
		.name = "n", \
		.flags = f, \
		.rst_reg = (void __iomem *)io_p2v(reg), \
		.rst_bit = bit, \
	}

#define RST_MODULE_VAR(n, regid, bit) \
	{ \
		.name = "n", \
		.flags = RST_HARDWARE_CLEAR, \
		.rst_reg = (void __iomem *)io_p2v(AP_PWR_MOD_RSTCTL##regid), \
		.rst_bit = bit, \
	}

extern int comip_reset(const char *name);

#endif /* __ARCH_ARM_PLAT_RESET_H */
