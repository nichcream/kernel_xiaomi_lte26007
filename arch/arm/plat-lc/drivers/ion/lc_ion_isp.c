
#include <linux/spinlock.h>

#include "lc_ion_isp.h"

static DEFINE_SPINLOCK(mem_lock);

static int isp_buffer_address = 0;
static int isp_buffer_size = 0;
static int isp_buffer_offset = 0;
static int isp_buffer_reset = 0;
static int is_isp_buffer_alloc = 0;

void lc_ion_isp_init_mem()
{
	spin_lock(&mem_lock);
	is_isp_buffer_alloc = 0;
	isp_buffer_address = 0;
	isp_buffer_size = 0;
	isp_buffer_offset = 0;
	spin_unlock(&mem_lock);
}

int lc_ion_isp_restore_mem(unsigned int phys_addr, unsigned int size)
{
	spin_lock(&mem_lock);

	if(!phys_addr || !size) {
		printk(KERN_DEBUG "lc_ion_isp_restore_mem invalid phys_addr %d or size %d", phys_addr, size);
		spin_unlock(&mem_lock);
		return -1;
	}

	if(is_isp_buffer_alloc) {
		printk(KERN_DEBUG "lc_ion_isp_restore_mem isp buffer already allocate");
		spin_unlock(&mem_lock);
		return 0;
	}

	is_isp_buffer_alloc = 1;
	isp_buffer_address = phys_addr;
	isp_buffer_size = size;
	isp_buffer_offset = 0;
	spin_unlock(&mem_lock);

	return 0;
}

int lc_ion_isp_remove_mem(unsigned int phys_addr)
{
	spin_lock(&mem_lock);
	is_isp_buffer_alloc = 0;
	isp_buffer_address = 0;
	isp_buffer_size = 0;
	isp_buffer_offset = 0;
	spin_unlock(&mem_lock);

	return 0;
}

void* lc_ion_isp_get_buffer(unsigned int size)
{
	int bus_addr = 0;

	spin_lock(&mem_lock);
	if(is_isp_buffer_alloc)	{
		if(isp_buffer_reset) {
			printk(KERN_DEBUG "lc_ion_isp_get_buffer reset\n");
			isp_buffer_reset = 0;
			isp_buffer_offset = 0;
		}

		if(isp_buffer_offset + size <= isp_buffer_size) {
			bus_addr = isp_buffer_address + isp_buffer_offset;
			isp_buffer_offset += size;
		} else {
			printk(KERN_DEBUG "lc_ion_isp_get_buffer out of range\n");
		}
	} else {
		printk(KERN_DEBUG "lc_ion_isp_get_buffer isp buffer not alloc\n");
	}
	spin_unlock(&mem_lock);

	return (void *)bus_addr;
}

int lc_ion_isp_release_buffer(void* phys_addr)
{
	spin_lock(&mem_lock);

	if(is_isp_buffer_alloc) {
		if(((unsigned int)phys_addr < isp_buffer_address) ||
			((unsigned int)phys_addr >= isp_buffer_address + isp_buffer_size)) {
			printk(KERN_DEBUG "release_isp_buffer out of range\n");
			spin_unlock(&mem_lock);
			return -1;
		}
		if(isp_buffer_reset) {
			isp_buffer_reset = 0;
			isp_buffer_offset = 0;
		}
	} else if(isp_buffer_reset) {
		isp_buffer_reset = 0;
		isp_buffer_offset = 0;
		printk(KERN_DEBUG "release_isp_buffer isp buffer not alloc\n");
	}
	spin_unlock(&mem_lock);

	return 0;
}

void lc_ion_isp_reset_buffer()
{
	spin_lock(&mem_lock);
	isp_buffer_reset = 1;
	spin_unlock(&mem_lock);

	printk(KERN_DEBUG "reset isp mem\n");
	return;
}

int lc_ion_isp_get_offset()
{
    //spin_lock(&mem_lock);
    if(isp_buffer_reset)
    {
        printk(KERN_DEBUG "lc_ion_isp_get_offset, reset isp_buffer_offset=%d isp_buffer_size=%d\n", isp_buffer_offset, isp_buffer_size);
        isp_buffer_reset = 0;
        isp_buffer_offset = 0;
    }
    //spin_unlock(&mem_lock);

	return isp_buffer_offset;
}

