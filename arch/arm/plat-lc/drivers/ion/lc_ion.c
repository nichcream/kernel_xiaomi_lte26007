/*
 * drivers/staging/android/ion/lc/lc_ion.c
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

#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/iommu.h>
#include <asm/cacheflush.h>

#include <plat/lcmem.h>
#include <plat/lc_ion.h>
#include <plat/comip-memory.h>
#include <ion/ion_priv.h>

#include "lc_ion_isp.h"

#define LC_ION_ORDER_POLICY 0
#define OLD_MMUHEAP 1

static struct ion_device *lc_ion_dev;
static int num_heaps;
static struct ion_heap **heaps;

#if  LC_ION_ORDER_POLICY
static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO |
						__GFP_NOWARN | __GFP_NORETRY | __GFP_NO_KSWAPD) &
					   ~__GFP_WAIT;
static gfp_t low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_ZERO |
					 __GFP_NOWARN);

static const unsigned int orders[] = {0};
#else
static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO |
						__GFP_NOWARN | __GFP_NORETRY | __GFP_COMP | __GFP_NO_KSWAPD) &
					   ~__GFP_WAIT;
static gfp_t low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_ZERO |
					 __GFP_NOWARN | __GFP_COMP);

static const unsigned int orders[] = {9, 8, 4, 0};
#endif
static const int num_orders = ARRAY_SIZE(orders);
static int order_to_index(unsigned int order)
{
	int i;
	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct lc_ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool **pools;
};

struct page_info {
	struct page *page;
	unsigned int order;
	struct list_head list;
};

struct ion_client *lc_ion_client_create(const char *name)
{
	if (!lc_ion_dev) {
		pr_err("lc_ion: client create failed, device is not initialized\n");
		return NULL;
	}

	return ion_client_create(lc_ion_dev, name);
}
EXPORT_SYMBOL(lc_ion_client_create);

static struct page *alloc_buffer_page(struct lc_ion_system_heap *heap,
					  struct ion_buffer *buffer,
					  unsigned long order)
{
	bool cached = ion_buffer_cached(buffer);
	struct ion_page_pool *pool = heap->pools[order_to_index(order)];
	struct page *page;

#if defined (CONFIG_ARCH_PHYS_ADDR_T_64BIT)
	if (buffer->flags & ION_HEAP_LC_2D_MASK)
	{
		gfp_t gfp_flags = low_order_gfp_flags;

		if (order > 4)
			gfp_flags = high_order_gfp_flags;

		gfp_flags &= ~__GFP_HIGHMEM;
		page = alloc_pages(gfp_flags, order);
		if (!page)
			return NULL;
		ion_pages_sync_for_device(NULL, page, PAGE_SIZE << order,
						DMA_BIDIRECTIONAL);
	}
	else
#endif
	{
		if (!cached) {
			page = ion_page_pool_alloc(pool);
		} else {
			gfp_t gfp_flags = low_order_gfp_flags;

			if (order > 4)
				gfp_flags = high_order_gfp_flags;
			page = alloc_pages(gfp_flags, order);
			if (!page)
				return NULL;
			ion_pages_sync_for_device(NULL, page, PAGE_SIZE << order,
							DMA_BIDIRECTIONAL);
		}
	}
	if (!page)
		return NULL;

	return page;
}

static void free_buffer_page(struct lc_ion_system_heap *heap,
				 struct ion_buffer *buffer, struct page *page,
				 unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
#if defined(CONFIG_ARCH_PHYS_ADDR_T_64BIT)
	if (buffer->flags & ION_HEAP_LC_2D_MASK) {
		__free_pages(page, order);
	}
	else
#endif
	{
		if (!cached && !(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)) {
			struct ion_page_pool *pool = heap->pools[order_to_index(order)];
			ion_page_pool_free(pool, page);
		} else {
			__free_pages(page, order);
		}
	}
}

static struct page_info *alloc_largest_available(struct lc_ion_system_heap *heap,
						 struct ion_buffer *buffer,
						 unsigned long size,
						 unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;

	info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
	if (!info)
		return NULL;

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i]);
		if (!page)
			continue;

		info->page = page;
		info->order = orders[i];
		INIT_LIST_HEAD(&info->list);
		return info;
	}
	kfree(info);

	return NULL;
}

static int lc_ion_system_heap_allocate(struct ion_heap *heap,
					 struct ion_buffer *buffer,
					 unsigned long size, unsigned long align,
					 unsigned long flags)
{
	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	int ret;
	struct list_head pages;
	struct page_info *info, *tmp_info;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];

	if (align > PAGE_SIZE)
		return -EINVAL;

	if (size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

	if(flags & ION_HEAP_LC_ISP_RESET_MASK)
	{
		reset_isp_buffer();
		lc_ion_isp_reset_buffer();
	}

	INIT_LIST_HEAD(&pages);
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining, max_order);
		if (!info)
			goto err;
		list_add_tail(&info->list, &pages);
		size_remaining -= (1 << info->order) * PAGE_SIZE;
		max_order = info->order;
		i++;
	}
	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, i, GFP_KERNEL);
	if (ret)
		goto err1;

	sg = table->sgl;
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		struct page *page = info->page;
		sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE, 0);
		sg = sg_next(sg);
		list_del(&info->list);
		kfree(info);
	}

	buffer->priv_virt = table;
	return 0;
err1:
	kfree(table);
err:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	return -ENOMEM;
}

static void lc_ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	bool cached = ion_buffer_cached(buffer);
	struct scatterlist *sg;
	LIST_HEAD(pages);
	int i;

	/* uncached pages come from the page pools, zero them before returning
	   for security purposes (other allocations are zerod at alloc time */
	if (!cached && !(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE))
		ion_heap_buffer_zero(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg->length));
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *lc_ion_system_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void lc_ion_system_heap_unmap_dma(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	return;
}

static int lc_ion_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
					int nr_to_scan)
{
	struct lc_ion_system_heap *sys_heap;
	int nr_total = 0;
	int i;

	sys_heap = container_of(heap, struct lc_ion_system_heap, heap);

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		nr_total += ion_page_pool_shrink(pool, gfp_mask, nr_to_scan);
	}

	return nr_total;
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = lc_ion_system_heap_allocate,
	.free = lc_ion_system_heap_free,
	.map_dma = lc_ion_system_heap_map_dma,
	.unmap_dma = lc_ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = lc_ion_system_heap_shrink,
};

static int lc_ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
					  void *unused)
{

	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	int i;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		seq_printf(s, "%d order %u highmem pages in pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	return 0;
}

struct ion_heap *lc_ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct lc_ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(struct lc_ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders,
				  GFP_KERNEL);
	if (!heap->pools)
		goto err_alloc_pools;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 4)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		heap->pools[i] = pool;
	}

	heap->heap.debug_show = lc_ion_system_heap_debug_show;
	return &heap->heap;
err_create_pool:
	for (i = 0; i < num_orders; i++)
		if (heap->pools[i])
			ion_page_pool_destroy(heap->pools[i]);
	kfree(heap->pools);
err_alloc_pools:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void lc_ion_system_heap_destroy(struct ion_heap *heap)
{
	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders; i++)
		ion_page_pool_destroy(sys_heap->pools[i]);
	kfree(sys_heap->pools);
	kfree(sys_heap);
}

static int lc_ion_lcmem_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	lcmem_alloc_type nflag = LCMEM_ALLOC_NORMAL;

	if (flags & ION_HEAP_LC_ISP_MASK) {
		buffer->priv_phys = (ion_phys_addr_t)get_isp_buffer(len);
	} else if (flags & ION_HEAP_LC_LCD_MASK) {
		if (FB_MEMORY_FIX)
			buffer->priv_phys = (ion_phys_addr_t)FB_MEMORY_BASE;
		else
			buffer->priv_phys = 0;
	} else {
		if (flags & ION_HEAP_LC_ISP_KERNEL_MASK) {
			nflag = LCMEM_ALLOC_ISP_MEM;
		}
		buffer->priv_phys = (ion_phys_addr_t)malloc_phys_lcmem(len, nflag);
	}
	if (!buffer->priv_phys)
		return -ENOMEM;

	buffer->flags = flags;
	return 0;
}

void lc_ion_lcmem_heap_free(struct ion_buffer *buffer)
{
	if(buffer->flags & ION_HEAP_LC_ISP_MASK) {
		release_isp_buffer((void*)buffer->priv_phys);
	} else {
		free_phys_lcmem((void*)buffer->priv_phys);
	}
}

static int lc_ion_lcmem_heap_phys(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   ion_phys_addr_t *addr, size_t *len)
{
	*addr = buffer->priv_phys;
	*len = buffer->size;
	return 0;
}

struct sg_table *lc_ion_lcmem_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, phys_to_page(buffer->priv_phys), buffer->size,
			offset_in_page(buffer->priv_phys));
	return table;
}

void lc_ion_lcmem_heap_unmap_dma(struct ion_heap *heap,
					  struct ion_buffer *buffer)
{
	if (buffer->sg_table) {
		sg_free_table(buffer->sg_table);
		kfree(buffer->sg_table);
	}
}

static void *lc_ion_lcmem_heap_map_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	int i;

	if (!pages)
		return 0;

	for (i = 0; i < npages; i++)
		pages[i] = phys_to_page(buffer->priv_phys + i * PAGE_SIZE);
	buffer->vaddr = vmap(pages, npages, VM_MAP, PAGE_KERNEL);
	vfree(pages);

	return buffer->vaddr;
}

static void lc_ion_lcmem_heap_unmap_kernel(struct ion_heap *heap,
				  struct ion_buffer *buffer)
{
	vunmap(buffer->vaddr);
}

int lc_ion_lcmem_heap_map_user(struct ion_heap *heap,
					struct ion_buffer *buffer,
					struct vm_area_struct *vma)
{
	unsigned long pfn = __phys_to_pfn(buffer->priv_phys);

	return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
				   vma->vm_end - vma->vm_start,
				   vma->vm_page_prot);
}

static struct ion_heap_ops lcmem_heap_ops = {
	.allocate = lc_ion_lcmem_heap_allocate,
	.free = lc_ion_lcmem_heap_free,
	.phys = lc_ion_lcmem_heap_phys,
	.map_dma = lc_ion_lcmem_heap_map_dma,
	.unmap_dma = lc_ion_lcmem_heap_unmap_dma,
	.map_kernel = lc_ion_lcmem_heap_map_kernel,
	.unmap_kernel = lc_ion_lcmem_heap_unmap_kernel,
	.map_user = lc_ion_lcmem_heap_map_user,
};

static struct ion_heap *lc_ion_lcmem_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &lcmem_heap_ops;
	heap->type = LC_ION_HEAP_TYPE_LCMEM;
	return heap;
}

static void lc_ion_lcmem_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

#ifdef CONFIG_COMIP_IOMMU

struct ion_iommu_priv_data {
	struct page **pages;
	int nrpages;
	unsigned long size;
	unsigned long map_phy_addr;
	size_t map_len;
};

static struct ion_buffer *isp_ion_buffer = NULL;

static int lc_ion_iommu_heap_allocate(struct ion_heap *heap, struct ion_buffer *buffer,
				unsigned long size, unsigned long align, unsigned long flags)
{
#if OLD_MMUHEAP
	struct ion_iommu_priv_data *p_data = NULL;
	struct ion_iommu_priv_data *data;
	struct scatterlist *sg;
	struct sg_table *table;
	unsigned int pfn;
	unsigned int i;
	int ret;

	if (unlikely(!buffer))
		return -EINVAL;

	if (flags & ION_HEAP_LC_ISP_MASK) {
		if (!isp_ion_buffer) {
			pr_warn("ion_iommu: state is out of order\n");
			return -EINVAL;
		}

		p_data = isp_ion_buffer->priv_virt;
		if (!p_data || p_data->size < size) {
			pr_warn("ion: request size is out of regien\n");
			return -EINVAL;
		}
	}

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->map_phy_addr = 0;
	data->map_len = 0;
	data->size = PFN_ALIGN(size);
	data->nrpages = data->size >> PAGE_SHIFT;
	data->pages = vzalloc(sizeof(struct page *) * data->nrpages);
	if (!data->pages) {
		ret = -ENOMEM;
		goto err1;
	}

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto err2;
	}

	ret = sg_alloc_table(table, data->nrpages, GFP_KERNEL);
	if (ret)
		goto err3;

	for_each_sg(table->sgl, sg, table->nents, i) {
		if (flags & ION_HEAP_LC_ISP_MASK) {
			pfn = lc_ion_isp_get_offset() >> PAGE_SHIFT;
			data->pages[i] = p_data->pages[pfn + i];
			if (!data->pages[i]) {
				ret = -EINVAL;
				goto err4;
			}
			get_page(data->pages[i]);
		} else {
#if defined(CONFIG_ARCH_PHYS_ADDR_T_64BIT)
			data->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
#else
			data->pages[i] = alloc_page(GFP_KERNEL | __GFP_HIGHMEM |  __GFP_ZERO);
#endif
			if (!data->pages[i]) {
				ret = -ENOMEM;
				goto err5;
			}
			arm_dma_ops.sync_single_for_device(NULL,
				pfn_to_dma(NULL, page_to_pfn(data->pages[i])),
				PAGE_SIZE, DMA_BIDIRECTIONAL);
		}
		sg_set_page(sg, data->pages[i], PAGE_SIZE, 0);
		sg_dma_address(sg) = sg_phys(sg);
	}

	buffer->priv_virt = data;
	buffer->flags = flags;
	buffer->sg_table = table;

	if (buffer->flags & ION_HEAP_LC_ISP_KERNEL_MASK) {
		isp_ion_buffer = buffer;
		lc_ion_isp_init_mem();
	}

	return 0;
err5:
	for (i = 0; i < data->nrpages; i++) {
		if (data->pages[i])
			__free_page(data->pages[i]);
	}
err4:
	sg_free_table(table);
err3:
	kfree(table);
err2:
	vfree(data->pages);
err1:
	kfree(data);

	return ret;
#else
	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	struct ion_iommu_priv_data *p_data = NULL;
	struct ion_iommu_priv_data *data;
	struct sg_table *table;
	struct scatterlist *sg;
	int ret;
	struct list_head pages;
	struct page_info *info, *tmp_info;
	unsigned int pfn;
	int i = 0;
	long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];

	if (unlikely(!buffer))
		return -EINVAL;

	if (flags & ION_HEAP_LC_ISP_MASK) {
		if (!isp_ion_buffer) {
			pr_warn("ion_iommu: state is out of order\n");
			return -EINVAL;
		}

		p_data = isp_ion_buffer->priv_virt;
		if (!p_data || p_data->size < size) {
			pr_warn("ion: request size is out of regien\n");
			return -EINVAL;
		}
	}

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->map_phy_addr = 0;
	data->map_len = 0;
	data->size = PFN_ALIGN(size);

	if (flags & ION_HEAP_LC_ISP_MASK) {
		data->nrpages = data->size >> PAGE_SHIFT;
		data->pages = vzalloc(sizeof(struct page *) * data->nrpages);
		if (!data->pages) {
			ret = -ENOMEM;
			goto err;
		}

		table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (!table) {
			ret = -ENOMEM;
			goto err1;
		}

		ret = sg_alloc_table(table, data->nrpages, GFP_KERNEL);
		if (ret)
			goto err2;

		for_each_sg(table->sgl, sg, table->nents, i) {
			pfn = lc_ion_isp_get_offset() >> PAGE_SHIFT;
			data->pages[i] = p_data->pages[pfn + i];
			if (!data->pages[i]) {
				ret = -EINVAL;
				goto err3;
			}
			get_page(data->pages[i]);
			sg_set_page(sg, data->pages[i], PAGE_SIZE, 0);
			sg_dma_address(sg) = sg_phys(sg);
		}
		pfn = lc_ion_isp_get_offset() >> PAGE_SHIFT;
		data->pages[i] = p_data->pages[pfn + i];
		if (!data->pages[i]) {
			ret = -EINVAL;
			goto err4;
		}
		get_page(data->pages[i]);
	} else {
		int tmpind = 0;

		INIT_LIST_HEAD(&pages);

		if (buffer->flags & ION_HEAP_LC_ISP_KERNEL_MASK)
			max_order = 0;

		while (size_remaining > 0) {
			info = alloc_largest_available(sys_heap, buffer, size_remaining, max_order);
			if (!info)
				goto err4;
			list_add_tail(&info->list, &pages);
			size_remaining -= (1 << info->order) * PAGE_SIZE;
			max_order = info->order;
			i++;
		}

		data->nrpages = i;
		data->pages = vzalloc(sizeof(struct page *) * data->nrpages);
		if (!data->pages) {
			ret = -ENOMEM;
			goto err5;
		}

		table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (!table)
			goto err6;

		ret = sg_alloc_table(table, i, GFP_KERNEL);
		if (ret)
			goto err7;

		sg = table->sgl;

		list_for_each_entry_safe(info, tmp_info, &pages, list) {
			struct page *page = info->page;
			data->pages[tmpind++] = page;

			sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE, 0);
			sg = sg_next(sg);
			list_del(&info->list);
			kfree(info);
		}
	}

	buffer->priv_virt = data;
	buffer->flags = flags;
	buffer->sg_table = table;

	if (buffer->flags & ION_HEAP_LC_ISP_KERNEL_MASK) {
		isp_ion_buffer = buffer;
		lc_ion_isp_init_mem();
	}

	return 0;

err7:
	kfree(table);
err6:
	vfree(data->pages);
err5:
	list_for_each_entry(info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
err4:
	kfree(data);
	return -ENOMEM;

err3:
	sg_free_table(table);
err2:
	kfree(table);
err1:
	vfree(data->pages);
err:
	kfree(data);
	return ret;
#endif
}

static void lc_ion_iommu_heap_free(struct ion_buffer *buffer)
{
#if OLD_MMUHEAP
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	int i;

	if (unlikely(!data))
		return;

	buffer->priv_virt = NULL;

	if(buffer->flags & ION_HEAP_LC_ISP_MASK)
		lc_ion_isp_release_buffer((void*)data->map_phy_addr);
	else if (data->map_len)
		ion_iommu_unmap(&(data->map_phy_addr), &(data->map_len), buffer->flags);

	if (buffer->sg_table) {
		sg_free_table(buffer->sg_table);
		kfree(buffer->sg_table);
		buffer->sg_table = NULL;
	}

	if (buffer->flags & ION_HEAP_LC_ISP_KERNEL_MASK) {
		WARN_ON(buffer != isp_ion_buffer);
		lc_ion_isp_remove_mem(data->map_phy_addr);
		isp_ion_buffer = NULL;
	}

	for (i = 0; i < data->nrpages; i++)
		__free_page(data->pages[i]);

	vfree(data->pages);
	data->map_len = 0;
	data->map_phy_addr = 0;
	kfree(data);
#else
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	int i;
	struct ion_heap *heap = buffer->heap;
	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	bool cached = ion_buffer_cached(buffer);
	struct scatterlist *sg;
	LIST_HEAD(pages);

	if (unlikely(!data))
		return;

	buffer->priv_virt = NULL;

	if(buffer->flags & ION_HEAP_LC_ISP_MASK)
		lc_ion_isp_release_buffer((void*)data->map_phy_addr);
	else if (data->map_len)
		ion_iommu_unmap(&(data->map_phy_addr), &(data->map_len), buffer->flags);

	if(!(buffer->flags & ION_HEAP_LC_ISP_MASK)) {
		/* uncached pages come from the page pools, zero them before returning
		   for security purposes (other allocations are zerod at alloc time */
		if (!cached)
			ion_heap_buffer_zero(buffer);

		for_each_sg(table->sgl, sg, table->nents, i)
			free_buffer_page(sys_heap, buffer, sg_page(sg),
					get_order(sg_dma_len(sg)));
	} else {
		for (i = 0; i < data->nrpages; i++)
			__free_page(data->pages[i]);
	}
	sg_free_table(table);
	kfree(table);

	if (buffer->flags & ION_HEAP_LC_ISP_KERNEL_MASK) {
		WARN_ON(buffer != isp_ion_buffer);
		lc_ion_isp_remove_mem(data->map_phy_addr);
		isp_ion_buffer = NULL;
	}

	vfree(data->pages);
	data->map_len = 0;
	data->map_phy_addr = 0;
	kfree(data);
#endif
}

static int lc_ion_iommu_heap_phys(struct ion_heap *heap, struct ion_buffer *buffer,
							ion_phys_addr_t *addr, size_t *len)
{
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	int ret = 0;

	if (unlikely(!data))
		return -EINVAL;

	if(buffer->flags & ION_HEAP_LC_ISP_MASK) {
		data->map_phy_addr = (unsigned long)lc_ion_isp_get_buffer(data->size);
		data->map_len = data->size;
		*addr = data->map_phy_addr;
		*len = data->map_len;
		return 0;
	}

	if (data->map_len == 0) {
		ret = ion_iommu_map(buffer->sg_table, addr, len, buffer->flags);
		if (ret == 0) {
			data->map_phy_addr = *addr;
			data->map_len = *len;
		} else {
			data->map_phy_addr = 0;
			data->map_len = 0;
		}
	} else {
		*addr = data->map_phy_addr;
		*len = data->map_len;
	}

	if (buffer->flags & ION_HEAP_LC_ISP_KERNEL_MASK)
		lc_ion_isp_restore_mem(data->map_phy_addr, data->map_len);

	return ret;
}

static struct sg_table *lc_ion_iommu_heap_map_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	struct ion_iommu_priv_data *data = buffer->priv_virt;

	if (unlikely(!data))
		return NULL;

	return buffer->sg_table;
}

static void lc_ion_iommu_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	return;
}

static void *lc_ion_iommu_heap_map_kernel(struct ion_heap *heap, struct ion_buffer *buffer)
{
	struct ion_iommu_priv_data *data = buffer->priv_virt;

	if (unlikely(!data))
		return NULL;

#if OLD_MMUHEAP
	buffer->vaddr = vmap(data->pages, data->nrpages, VM_IOREMAP, PAGE_KERNEL);
#else
	buffer->vaddr = ion_heap_map_kernel(heap, buffer);
#endif
	return buffer->vaddr;
}

void lc_ion_iommu_heap_unmap_kernel(struct ion_heap *heap,
									struct ion_buffer *buffer)
{
	if (!buffer->vaddr)
		return;

#if OLD_MMUHEAP
	vunmap(buffer->vaddr);
#else
	ion_heap_unmap_kernel(heap, buffer);
#endif
	buffer->vaddr = NULL;
}

static int lc_ion_iommu_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
							   struct vm_area_struct *vma)
{
#if OLD_MMUHEAP
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	unsigned long curr_addr;
	int i;

	if (unlikely(!data))
		return -EINVAL;

	curr_addr = vma->vm_start;
	for (i = 0; i < data->nrpages && curr_addr < vma->vm_end; i++) {
		if (vm_insert_page(vma, curr_addr, data->pages[i])) {
			/*
			* This will fail the mmap which will
			* clean up the vma space properly.
			*/
			return -EINVAL;
		}
		curr_addr += PAGE_SIZE;
	}

	return 0;
#else
	return ion_heap_map_user(heap, buffer, vma);
#endif
}

#if !OLD_MMUHEAP
static int lc_ion_iommu_heap_shrink(struct shrinker *shrinker,
				  struct shrink_control *sc) {

	struct ion_heap *heap = container_of(shrinker, struct ion_heap,
						 shrinker);
	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	int nr_total = 0;
	int nr_freed = 0;
	int i;

	if (sc->nr_to_scan == 0)
		goto end;

	/* shrink the free list first, no point in zeroing the memory if
	   we're just going to reclaim it */
	nr_freed += ion_heap_freelist_drain(heap, sc->nr_to_scan * PAGE_SIZE) /
		PAGE_SIZE;

	if (nr_freed >= sc->nr_to_scan)
		goto end;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		nr_freed += ion_page_pool_shrink(pool, sc->gfp_mask,
						 sc->nr_to_scan);
		if (nr_freed >= sc->nr_to_scan)
			break;
	}

end:
	/* total number of items is whatever the page pools are holding
	   plus whatever's in the freelist */
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		nr_total += ion_page_pool_shrink(pool, sc->gfp_mask, 0);
	}
	nr_total += ion_heap_freelist_size(heap) / PAGE_SIZE;
	return nr_total;

}

static int lc_ion_iommu_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
					  void *unused)
{

	struct lc_ion_system_heap *sys_heap = container_of(heap,
							struct lc_ion_system_heap,
							heap);
	int i;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		seq_printf(s, "%d order %u highmem pages in pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	return 0;
}
#endif

static struct ion_heap_ops iommu_heap_ops = {
	.allocate	= lc_ion_iommu_heap_allocate,
	.free		= lc_ion_iommu_heap_free,
	.phys		= lc_ion_iommu_heap_phys,
	.map_dma	= lc_ion_iommu_heap_map_dma,
	.unmap_dma	= lc_ion_iommu_heap_unmap_dma,
	.map_kernel	= lc_ion_iommu_heap_map_kernel,
	.unmap_kernel	= lc_ion_iommu_heap_unmap_kernel,
	.map_user	= lc_ion_iommu_heap_map_user,
};

static struct ion_heap *lc_ion_iommu_heap_create(struct ion_platform_heap *unused)
{
#if OLD_MMUHEAP
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &iommu_heap_ops;
	heap->type = LC_ION_HEAP_TYPE_SYSTEM_MMU;

	return heap;
#else
	struct lc_ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(struct lc_ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &iommu_heap_ops;
	heap->heap.type = LC_ION_HEAP_TYPE_SYSTEM_MMU;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders,
				  GFP_KERNEL);
	if (!heap->pools)
		goto err_alloc_pools;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 4)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		heap->pools[i] = pool;
	}

	heap->heap.shrinker.shrink = lc_ion_iommu_heap_shrink;
	heap->heap.shrinker.seeks = DEFAULT_SEEKS;
	heap->heap.shrinker.batch = 0;
	register_shrinker(&heap->heap.shrinker);
	heap->heap.debug_show = lc_ion_iommu_heap_debug_show;
	return &heap->heap;
err_create_pool:
	for (i = 0; i < num_orders; i++)
		if (heap->pools[i])
			ion_page_pool_destroy(heap->pools[i]);
	kfree(heap->pools);
err_alloc_pools:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
#endif
}

static void lc_ion_iommu_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

#endif

static int lc_ion_dma_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	dma_addr_t phy_addr;

	buffer->vaddr = dma_alloc_writecombine(NULL, len, &phy_addr, GFP_KERNEL);

	buffer->priv_phys = (ion_phys_addr_t)phy_addr;

	if (!buffer->vaddr)
		return -ENOMEM;

	buffer->flags = flags;
	return 0;
}

void lc_ion_dma_heap_free(struct ion_buffer *buffer)
{
	dma_free_writecombine(NULL, buffer->size, buffer->vaddr, buffer->priv_phys);
}

static int lc_ion_dma_heap_phys(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   ion_phys_addr_t *addr, size_t *len)
{
	*addr = buffer->priv_phys;
	*len = buffer->size;
	return 0;
}

struct sg_table *lc_ion_dma_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, phys_to_page(buffer->priv_phys), buffer->size,
			offset_in_page(buffer->priv_phys));
	return table;
}

void lc_ion_dma_heap_unmap_dma(struct ion_heap *heap,
					  struct ion_buffer *buffer)
{
	if (buffer->sg_table) {
		sg_free_table(buffer->sg_table);
		kfree(buffer->sg_table);
	}
}

static void *lc_ion_dma_heap_map_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	return buffer->vaddr;
}

static void lc_ion_dma_heap_unmap_kernel(struct ion_heap *heap,
				  struct ion_buffer *buffer)
{
	return;
}

int lc_ion_dma_heap_map_user(struct ion_heap *heap,
					struct ion_buffer *buffer,
					struct vm_area_struct *vma)
{
	unsigned long pfn = __phys_to_pfn(buffer->priv_phys);

	return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
				   vma->vm_end - vma->vm_start,
				   vma->vm_page_prot);
}

static struct ion_heap_ops dma_heap_ops = {
	.allocate = lc_ion_dma_heap_allocate,
	.free = lc_ion_dma_heap_free,
	.phys = lc_ion_dma_heap_phys,
	.map_dma = lc_ion_dma_heap_map_dma,
	.unmap_dma = lc_ion_dma_heap_unmap_dma,
	.map_kernel = lc_ion_dma_heap_map_kernel,
	.unmap_kernel = lc_ion_dma_heap_unmap_kernel,
	.map_user = lc_ion_dma_heap_map_user,
};

static struct ion_heap *lc_ion_dma_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &dma_heap_ops;
	heap->type = LC_ION_HEAP_TYPE_DMA;
	return heap;
}

static void lc_ion_dma_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

static struct ion_heap *lc_ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
	case LC_ION_HEAP_TYPE_LCMEM:
		heap = lc_ion_lcmem_heap_create(heap_data);
		break;
	case LC_ION_HEAP_TYPE_SYSTEM:
		heap = lc_ion_system_heap_create(heap_data);
		break;
#ifdef CONFIG_COMIP_IOMMU
	case LC_ION_HEAP_TYPE_SYSTEM_MMU:
		heap = lc_ion_iommu_heap_create(heap_data);
		break;
#endif
	case LC_ION_HEAP_TYPE_DMA:
		heap = lc_ion_dma_heap_create(heap_data);
		break;
	default:
		return ion_heap_create(heap_data);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %lu size %u\n",
			   __func__, heap_data->name, heap_data->type,
			   heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;

	return heap;
}

void lc_ion_heap_destroy(struct ion_heap *heap)
{
	if (!heap)
		return;

	switch ((int)heap->type) {
	case LC_ION_HEAP_TYPE_LCMEM:
		lc_ion_lcmem_heap_destroy(heap);
		break;
	case LC_ION_HEAP_TYPE_SYSTEM:
		lc_ion_system_heap_destroy(heap);
		break;
#ifdef CONFIG_COMIP_IOMMU
	case LC_ION_HEAP_TYPE_SYSTEM_MMU:
		lc_ion_iommu_heap_destroy(heap);
		break;
#endif
	case LC_ION_HEAP_TYPE_DMA:
		lc_ion_dma_heap_destroy(heap);
		break;
	default:
		ion_heap_destroy(heap);
	}
}

static int lc_ion_no_pages_cache_ops(struct ion_client *client,
			struct ion_handle *handle,
			void *vaddr,
			unsigned int length,
			unsigned int cmd)
{
	unsigned long size_to_vmap, total_size;
	int i, j, ret;
	void *ptr = NULL;
	ion_phys_addr_t buff_phys = 0;
	ion_phys_addr_t buff_phys_start = 0;
	size_t buf_length = 0;

	ret = ion_phys(client, handle, &buff_phys_start, &buf_length);
	if (ret)
		return -EINVAL;

	buff_phys = buff_phys_start;

	if (!vaddr) {
		/*
		 * Split the vmalloc space into smaller regions in
		 * order to clean and/or invalidate the cache.
		 */
		size_to_vmap = ((VMALLOC_END - VMALLOC_START)/8);
		total_size = buf_length;

		for (i = 0; i < total_size; i += size_to_vmap) {
			size_to_vmap = min(size_to_vmap, total_size - i);
			for (j = 0; j < 10 && size_to_vmap; ++j) {
				ptr = ioremap(buff_phys, size_to_vmap);
				if (ptr) {
					switch (cmd) {
					case ION_IOC_CLEAN_CACHE:
						dmac_map_area(ptr, size_to_vmap, DMA_TO_DEVICE);
						break;
					case ION_IOC_INV_CACHE:
						dmac_map_area(ptr, size_to_vmap, DMA_FROM_DEVICE);
						break;
					case ION_IOC_CLEAN_INV_CACHE:
						dmac_flush_range(ptr, ptr + size_to_vmap);
						break;
					default:
						return -EINVAL;
					}
					buff_phys += size_to_vmap;
					break;
				} else {
					size_to_vmap >>= 1;
				}
			}
			if (!ptr) {
				pr_err("Couldn't io-remap the memory\n");
				return -EINVAL;
			}
			iounmap(ptr);
		}
	} else {
		switch (cmd) {
		case ION_IOC_CLEAN_CACHE:
			dmac_map_area(vaddr, length, DMA_TO_DEVICE);
			break;
		case ION_IOC_INV_CACHE:
			dmac_map_area(vaddr, length, DMA_FROM_DEVICE);
			break;
		case ION_IOC_CLEAN_INV_CACHE:
			dmac_flush_range(vaddr, vaddr + length);
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int lc_ion_pages_cache_ops(struct ion_client *client,
			struct ion_handle *handle,
			void *vaddr, unsigned int length,
			unsigned int cmd)
{
	struct sg_table *table = NULL;

	table = ion_sg_table(client, handle);
	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	switch (cmd) {
	case ION_IOC_CLEAN_CACHE:
		if (!vaddr)
			dma_sync_sg_for_device(NULL, table->sgl,
				table->nents, DMA_TO_DEVICE);
		else
			dmac_map_area(vaddr, length, DMA_TO_DEVICE);
		break;
	case ION_IOC_INV_CACHE:
		if (!vaddr)
			dma_sync_sg_for_cpu(NULL, table->sgl,
				table->nents, DMA_FROM_DEVICE);
		else
			dmac_map_area(vaddr, length, DMA_FROM_DEVICE);
		break;
	case ION_IOC_CLEAN_INV_CACHE:
		if (!vaddr) {
			dma_sync_sg_for_device(NULL, table->sgl,
				table->nents, DMA_TO_DEVICE);
			dma_sync_sg_for_cpu(NULL, table->sgl,
				table->nents, DMA_FROM_DEVICE);
		} else {
			dmac_flush_range(vaddr, vaddr + length);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

long lc_ion_ioctl(struct ion_client *client, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case ION_IOC_GET_PHYS:
	{
		struct ion_phys_data data;
		struct ion_handle *handle;
		ion_phys_addr_t paddr;
		size_t len;
		int ret;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;

		handle = ion_import_dma_buf(client, data.fd);
		if (IS_ERR(handle)) {
			pr_info("%s: Could not import handle: %d\n", __func__, (int)handle);
			return -EINVAL;
		}

		ret = ion_phys(client, handle, &paddr, &len);
		if (ret) {
			ion_free(client, handle);
			return ret;
		}

		ion_free(client, handle);

		data.paddr = paddr;
		data.len = len;

		if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
			return -EFAULT;
		}
		break;
	}
	case ION_IOC_CLEAN_CACHE:
	case ION_IOC_INV_CACHE:
	case ION_IOC_CLEAN_INV_CACHE:
	{
		struct ion_cache_data data;
		struct ion_handle *handle;
		unsigned long flags;
		struct sg_table *table;
		struct page *page;
		int ret;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;

		handle = ion_import_dma_buf(client, data.fd);
		if (IS_ERR(handle)) {
			pr_info("%s: Could not import handle: %d\n", __func__, (int)handle);
			return -EINVAL;
		}

		ret = ion_handle_get_flags(client, handle, &flags);
		if (IS_ERR(handle)) {
			pr_info("%s: Could not get flags: %d\n", __func__, ret);
			ion_free(client, handle);
			return -EINVAL;
		}

		if(!(flags & ION_FLAG_CACHED)) {
			ion_free(client, handle);
			break;
		}

		table = ion_sg_table(client, handle);

		if (IS_ERR_OR_NULL(table)) {
			ion_free(client, handle);
			return PTR_ERR(table);
		}

		page = sg_page(table->sgl);

		if (page)
			ret = lc_ion_pages_cache_ops(client, handle, (void *)data.vaddr,
						data.len, cmd);
		else
			ret = lc_ion_no_pages_cache_ops(client, handle, (void *)data.vaddr,
						data.len, cmd);

		ion_free(client, handle);
		break;
	}
	default:
		pr_err("%s: Unknown custom ioctl\n", __func__);
		return -ENOTTY;
	}
	return 0;
}

static int lc_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int err;
	int i;

	dev_info(&pdev->dev, "lc_ion_probe\n");

	num_heaps = pdata->nr;
	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	lc_ion_dev = ion_device_create(lc_ion_ioctl);
	if (IS_ERR_OR_NULL(lc_ion_dev)) {
		kfree(heaps);
		return PTR_ERR(lc_ion_dev);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		heaps[i] = lc_ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(lc_ion_dev, heaps[i]);
	}
	platform_set_drvdata(pdev, lc_ion_dev);
	return 0;
err:
	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			lc_ion_heap_destroy(heaps[i]);
	}
	kfree(heaps);
	return err;
}

static int lc_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < num_heaps; i++)
		lc_ion_heap_destroy(heaps[i]);

	ion_device_destroy(idev);
	kfree(heaps);
	return 0;
}

static struct platform_driver ion_driver = {
	.probe = lc_ion_probe,
	.remove = lc_ion_remove,
	.driver = { .name = "ion-lc" }
};

module_platform_driver(ion_driver);

