#ifndef __ASM_ARCH_PLAT_SPI_H
#define __ASM_ARCH_PLAT_SPI_H

#define COMIP_SPI_CS_MAX		(2)

struct comip_spi_platform_data {
	unsigned short num_chipselect;
	int (*cs_state)(int chipselect, int level);
};

#endif/*__ASM_ARCH_PLAT_SPI_H */

