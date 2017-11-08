/*
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_LC_ION_H
#define _LINUX_LC_ION_H

#include <linux/mutex.h>
#include <ion/ion.h>

enum {
	LC_ION_HEAP_TYPE_LCMEM = ION_HEAP_TYPE_CUSTOM + 1,  /* use lcmem allocator */
	LC_ION_HEAP_TYPE_SYSTEM,
	LC_ION_HEAP_TYPE_DMA,
	LC_ION_HEAP_TYPE_SYSTEM_MMU,	/* system allocator with smmu */
};

#define ION_HEAP_SYSTEM_ID 0
#define LC_ION_HEAP_LCMEM_ID 8
#define LC_ION_HEAP_SYSTEM_ID 9
#define LC_ION_HEAP_DMA_ID 10
#define LC_ION_HEAP_SYSTEM_MMU_ID 11
#define LC_ION_HEAP_LCMEM_ALIAS_ID 12

/* heap mask is to select the suitable heap allocator with the same heap id */
#define LC_ION_HEAP_LCMEM_MASK		(1 << LC_ION_HEAP_LCMEM_ID)
#define LC_ION_HEAP_SYSTEM_MASK		(1 << LC_ION_HEAP_SYSTEM_ID)
#define LC_ION_HEAP_DMA_MASK		(1 << LC_ION_HEAP_DMA_ID)
#define LC_ION_HEAP_SYSTEM_MMU_MASK	(1 << LC_ION_HEAP_SYSTEM_MMU_ID)
#define LC_ION_HEAP_LCMEM_ALIAS_MASK	(1 << LC_ION_HEAP_LCMEM_ALIAS_ID)

/* flag to indicate the device that use the data */
#define ION_HEAP_LC_VIDEO_MASK		(1 << 24)
#define ION_HEAP_LC_2D_MASK		(1 << 25)
#define ION_HEAP_LC_ISP_MASK		(1 << 26)
#define ION_HEAP_LC_LCD_MASK		(1 << 27)
#define ION_HEAP_LC_ISP_RESET_MASK	(1 << 28)
#define ION_HEAP_LC_ISP_KERNEL_MASK	(1 << 29)

/* struct ion_phys_data - data passed to ion for get physical address
 *
 * @fd:	share fd
 * @paddr:      physical address of the data
 * @len:		size of the data
 *
 */
struct ion_phys_data {
	int fd;
	unsigned int paddr;
	size_t len;
};

/* struct ion_cache_data - data passed to ion for cache operation
 *
 * @fd:	share fd
 * @vaddr:      virtual address of the data
 * @len:		size of the data
 *
 */
struct ion_cache_data {
	int fd;
	unsigned long vaddr;
	size_t len;
};

#define ION_IOC_LC_MAGIC 'L'

/**
 * DOC: ION_IOC_GET_PHYS - get physical address
 *
 * Get physical address of buffer
 */
#define ION_IOC_GET_PHYS	    	_IOWR(ION_IOC_LC_MAGIC, 0, \
                                                struct ion_phys_data)

/**
 * DOC: ION_IOC_CLEAN_CACHE - clean ion cache
 *
 * Flush cache of ion buffer
 */
#define ION_IOC_CLEAN_CACHE	    	_IOR(ION_IOC_LC_MAGIC, 1, \
                                                struct ion_cache_data)

/**
 * DOC: ION_IOC_INV_CACHE - invalid ion cache
 *
 * Invalid cache of ion buffer
 */
#define ION_IOC_INV_CACHE	    	_IOR(ION_IOC_LC_MAGIC, 2, \
                                                struct ion_cache_data)

/**
 * DOC: ION_IOC_CLEAN_INV_CACHE - clean and invalid ion cache
 *
 * Flush and invalid cache of ion buffer
 */
#define ION_IOC_CLEAN_INV_CACHE	    _IOR(ION_IOC_LC_MAGIC, 3, \
                                                struct ion_cache_data)

extern struct ion_client *lc_ion_client_create(const char *name);

#ifdef CONFIG_COMIP_IOMMU
extern int ion_iommu_map(struct sg_table *sg_table, unsigned long *addr, size_t *len, unsigned long flags);
extern int ion_iommu_unmap(unsigned long *addr, size_t *len, unsigned long flags);
#endif
#endif /* _LINUX_LC_ION_H */
