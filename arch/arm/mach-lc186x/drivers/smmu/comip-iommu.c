#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/iommu.h>

#include <plat/lc_ion.h>

struct vminfo {
	struct rb_node rb_node;
	struct list_head list;
	unsigned long vstart;
	unsigned long vend;
	unsigned long flag;
};

#define IOMMU_VM_START	0x100000
#define IOMMU_VM_END	0xffffffff

static DEFINE_SPINLOCK(vm_splk);
static LIST_HEAD(vm_area_list);

static struct domain_data {
	struct iommu_domain *io_domain[2][4];
	atomic_t ref[2][4];
} g_domain;

static struct rb_root area_root = RB_ROOT;
static struct rb_node *search_vm = NULL;
static unsigned long hole_size = 0;

static size_t sgtable_len(struct sg_table *sgt)
{
	size_t total = 0;
	struct scatterlist *sg;
	unsigned int i;

	if (!sgt)
		return 0;

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		total += sg->length;

	return total;
}

static struct vminfo *__find_vmap_area(unsigned long addr)
{
	struct rb_node *n = area_root.rb_node;
	struct vminfo *va;

	while (n) {
		va = rb_entry(n, struct vminfo, rb_node);
		if (addr < va->vstart)
			n = n->rb_left;
		else if (addr > va->vstart)
			n = n->rb_right;
		else
			return va;
	}

	return NULL;
}

static void __free_vm_area(struct vminfo *va)
{
	if (!va)
		return ;
	
	if (search_vm) {
		if (va->vend < IOMMU_VM_START) {
			search_vm = NULL;
		}
		else {
			struct vminfo *area;
			area = rb_entry(search_vm, struct vminfo, rb_node);
			if (va->vstart <= area->vstart)
				search_vm = rb_prev(&va->rb_node);
		}
	}
	rb_erase(&va->rb_node, &area_root);
	RB_CLEAR_NODE(&va->rb_node);
	list_del(&va->list);
	kfree(va);
}

static size_t free_vm_addr(unsigned long va)
{
	struct vminfo *vm_area = NULL;
	size_t size;
	
	spin_lock(&vm_splk);
	vm_area = __find_vmap_area(va);
	if (!vm_area) {
		spin_unlock(&vm_splk);
		return 0;
	}
	size = vm_area->vend - vm_area->vstart;
	__free_vm_area(vm_area);
	spin_unlock(&vm_splk);
	
	return size;
}

static void __insert_vm_area(struct vminfo *va)
{
	struct rb_node **p = &area_root.rb_node;
	struct rb_node *parent = NULL;
	struct rb_node *tmp;
	struct vminfo *tmp_va;

	while (*p) {
		parent = *p;
		tmp_va = rb_entry(parent, struct vminfo, rb_node);
		if (va->vstart < tmp_va->vend)
			p = &(*p)->rb_left;
		else if (va->vend > tmp_va->vstart)
			p = &(*p)->rb_right;
		else
			BUG();
	}

	rb_link_node(&va->rb_node, parent, p);
	rb_insert_color(&va->rb_node, &area_root);
	/* address-sort this list */
	tmp = rb_prev(&va->rb_node);
	if (tmp) {
		struct vminfo *prev;
		prev = rb_entry(tmp, struct vminfo, rb_node);
		list_add(&va->list, &prev->list);
	} else
		list_add(&va->list, &vm_area_list);
}

static struct vminfo *get_iommu_vm_area(unsigned long total, unsigned long flag)
{
	unsigned long size, addr;
	struct vminfo *area, *tmp, *first;
	struct rb_node *n;
	
	size = PAGE_ALIGN(total);
	if (unlikely(!size))
		return NULL;

	//size = size + PAGE_SIZE; //set guard
	area = kzalloc(sizeof(struct vminfo), GFP_KERNEL);
	if (unlikely(!area))
		return NULL;

	spin_lock(&vm_splk);
	if (!search_vm || (size < hole_size)) {
		hole_size = 0;
		search_vm = NULL;
	}

	//find a search point
	if (search_vm) {
		first = rb_entry(search_vm, struct vminfo, rb_node);
		addr = PAGE_ALIGN(first->vend);
		if ((addr < IOMMU_VM_START) || (addr + size < addr))
			goto out;
	}
	else {
		n = area_root.rb_node;
		addr = IOMMU_VM_START;
		first = NULL;
		if (addr + size < addr)
			goto out;
		
		while (n) {
			tmp = rb_entry(n, struct vminfo, rb_node);
			if (tmp->vend >= addr) {
				first = tmp;
				if (tmp->vstart <= addr)
					break;
				n = n->rb_left;
			}
			else {
				n = n->rb_right; 
			}
		}

		if (!first)
			goto found;
	}

	while (addr + size > first->vstart && addr + size <= IOMMU_VM_END) {
		if (addr + hole_size < first->vstart)
			hole_size = first->vstart - addr;
		addr = PAGE_ALIGN(first->vend);
		if (addr + size < addr)
			goto out;
		if (list_is_last(&first->list, &vm_area_list))
			goto found;
		first = list_entry(first->list.next,
				struct vminfo, list);
	}

found:
	if (addr + size > IOMMU_VM_END)
		goto out;
	area->vstart = addr;
	area->vend = addr + size;
	area->flag = 0;
	__insert_vm_area(area);
	search_vm = &area->rb_node;
	spin_unlock(&vm_splk);
	
	return area;

out:
	spin_unlock(&vm_splk);
	kfree(area);
	
	return NULL;
}

/* map 'sglist' to a contiguous mmu virtual area and return 'va' */
static int vmap_sg(struct iommu_domain *domain, struct sg_table *sgt,
	unsigned long *addr, size_t *len, int is_vm)
{
	unsigned long va;
	size_t total, bytes;
	unsigned int i;
	int err;
	struct scatterlist *sg;
	struct vminfo *v_new;
	phys_addr_t pa;

	total = sgtable_len(sgt);
	if (!total)
		return -EINVAL;

	if (is_vm)
		va = *addr;
	else {
		v_new = get_iommu_vm_area(total, 0);
		if (!v_new)
			return -ENOMEM;
		va = v_new->vstart;
		*addr = v_new->vstart;
	}

	*len = total;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		pa = sg_phys(sg);
		bytes = sg->length;
		err = iommu_map(domain, va, pa, bytes,
			IOMMU_READ | IOMMU_WRITE | IOMMU_EXEC);
		if (err)
			goto err_out;
		va += bytes;
	}

	return 0;
err_out:
	if (!is_vm)
		__free_vm_area(v_new);

	printk("ion_iommu: vmap_sg faile\n");
	return -EINVAL;
}

static int vunmap_sg(struct iommu_domain *domain,
	unsigned long *va, size_t *len, int is_vm)
{
	unsigned long vaddr = *va;
	size_t size;

	if (!is_vm) {
		size = free_vm_addr(vaddr);
		if (size < *len)
			printk(KERN_WARNING "vm free size =%d, len = %d is wrong %s\n",
				size, *len, __func__);
	} else
		size = *len;

	return iommu_unmap(domain, vaddr, size);
}

static int inline domain_map_id(unsigned long flags)
{
	if (flags & (ION_HEAP_LC_VIDEO_MASK | ION_HEAP_LC_2D_MASK
		| ION_HEAP_LC_ISP_RESET_MASK | ION_HEAP_LC_ISP_MASK))
		return 0;
	else if (flags & (ION_HEAP_LC_LCD_MASK | ION_HEAP_LC_ISP_KERNEL_MASK))
		return 1;
	else
		return -EINVAL;
}

static int inline vm_is_resident(unsigned long flags)
{
	if (flags & ION_HEAP_LC_ISP_MASK)
		return 1;
	else
		return 0;
}

static int inline domain_is_share(unsigned long flags)
{
	if (flags & ION_HEAP_LC_ISP_KERNEL_MASK)
		return 1;
	else
		return 0;
}

int ion_iommu_map(struct sg_table *sg_table, unsigned long *addr,
	size_t *len, unsigned long flags)
{
	int dev_id = domain_map_id(flags);
	int ret = 0;

	if (dev_id < 0) {
		printk("ion_iommu: error heap id\n");
		return dev_id;
	}

	ret = vmap_sg(g_domain.io_domain[dev_id][0], sg_table, addr, len,
			vm_is_resident(flags));
	if (domain_is_share(flags))
		ret |= vmap_sg(g_domain.io_domain[dev_id ^ 1][0], sg_table,
			addr, len, 1);
	if (ret)
		printk("ion_iommu: dev %08x map error: %08x\n",
			(unsigned int)flags, ret);

	return ret;
}

int ion_iommu_unmap(unsigned long *addr, size_t *len, unsigned long flags)
{
	int dev_id = domain_map_id(flags);

	if (dev_id < 0) {
		printk("ion_iommu: error heap id\n");
		return dev_id;
	}

	if (domain_is_share(flags))
		vunmap_sg(g_domain.io_domain[dev_id ^ 1][0], addr, len, 1);

	return  vunmap_sg(g_domain.io_domain[dev_id][0], addr, len,
			vm_is_resident(flags));

}

EXPORT_SYMBOL(ion_iommu_map);
EXPORT_SYMBOL(ion_iommu_unmap);

static int dev_smmu_id(struct device *dev, int *dev_id, int *s_id)
{
        struct platform_device *pdev;
	int id;

	pdev = container_of(dev, struct platform_device, dev);
	id = pdev->archdata.dev_id;
	if (id) {
		if (dev_id)
			*dev_id = --id;
		if (s_id)
			*s_id = 0; //pdev->archdata.s_id;
		return 0;
	}

	return -ENODEV;
}

int ion_iommu_attach_dev(struct device *dev)
{
	int dev_id;
	int s_id;
	int ret;

	if (dev_smmu_id(dev, &dev_id, &s_id))
		return -ENODEV;

	if (!atomic_read(&g_domain.ref[dev_id][s_id])) {
		g_domain.io_domain[dev_id][s_id] = iommu_domain_alloc(&platform_bus_type);
	}

	ret = iommu_attach_device(g_domain.io_domain[dev_id][s_id], dev);
	if (!ret)
		atomic_inc(&g_domain.ref[dev_id][s_id]);

	return ret;
}

int ion_iommu_detach_dev(struct device *dev)
{
	int dev_id;
	int s_id;

	if (dev_smmu_id(dev, &dev_id, &s_id))
		return -ENODEV;

	if (!atomic_read(&g_domain.ref[dev_id][s_id])) {
		printk("ion_iommu: try to detach un-attached dev\n");
		return -ENODEV;
	}

	iommu_detach_device(g_domain.io_domain[dev_id][s_id], dev);

	if (atomic_dec_and_test(&g_domain.ref[dev_id][s_id]))
		iommu_domain_free(g_domain.io_domain[dev_id][s_id]);

	return 0;
}

EXPORT_SYMBOL(ion_iommu_attach_dev);
EXPORT_SYMBOL(ion_iommu_detach_dev);

