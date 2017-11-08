/*
 * (C) Copyright Leadcore
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __COMIP_MMC_H_
#define __COMIP_MMC_H_



#define   MMC_TRANSFER_MASK          ((1<<SDMMC_DRTO_INTR)  | \
                                      (1<<SDMMC_FRUN_INTR)   | \
                                      (1<<SDMMC_SBE_INTR) | \
                                      (1<<SDMMC_DTO_INTR))

#define   MMC_TRANSFER_ERROR_MASK     ((1<<SDMMC_DRTO_INTR)  | \
                                      (1<<SDMMC_FRUN_INTR)   | \
                                      (1<<SDMMC_SBE_INTR) )




struct mmc_host {
	unsigned int version;	/* SDHCI spec. version */
	unsigned int clock;	/* Current clock (MHz) */
	unsigned int base;	/* Base address, SDMMC1/2/3/4 */
	int pwr_gpio;		/* Power GPIO */
	int cd_gpio;		/* Change Detect GPIO */
};

int comip_mmc_init(int dev_index, int bus_width, int pwr_gpio, int cd_gpio);

#endif	/* __COMIP_MMC_H_ */
