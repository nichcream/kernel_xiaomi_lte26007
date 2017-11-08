#ifndef __ASM_ARCH_PLAT_I2C_H
#define __ASM_ARCH_PLAT_I2C_H

#define COMIP_I2C_STANDARD_MODE		(1 << 0)
#define COMIP_I2C_FAST_MODE		(1 << 1)
#define COMIP_I2C_HIGH_SPEED_MODE	(1 << 2)

struct comip_i2c_platform_data {
	unsigned int class;
	unsigned int use_pio;
	unsigned int flags;
};

#endif /* __ASM_ARCH_PLAT_I2C_H */

