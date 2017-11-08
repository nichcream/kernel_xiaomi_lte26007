/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Allocate memory blocks
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: lcmem.c,v $
--  $Date: 2012/11/2 06:28:46 $
--  $Revision: 1.20 $
--------------------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range */
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>
/* this header files wraps some common module-space operations ...
   here we use mem_map_reserve() macro */

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/list.h>
/* for current pid */
#include <linux/sched.h>

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

/* Our header */
#include <plat/lcmem.h>
#include <plat/comip-memory.h>

/*
 * on2 mem address Macros
 */
#define ON2_PHY_MEM_INFO_BASE 	(ON2_MEMORY_BASE)
#define ON2_PHY_MEM_INFO_SIZE   (PAGE_SIZE)
#define ON2_PHY_MEM_BASE 	(ON2_MEMORY_BASE+ON2_PHY_MEM_INFO_SIZE)

#define ON2_MEM_TEST

/* module description */
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Hantro Products Oy");
MODULE_DESCRIPTION("RAM allocation");

#define ON2MEM_ASSERT(x)
//#undef PDEBUG
//#undef printk
//#define PDEBUG(x,...) printk("[%s, %s LINE%d]: "x,__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__);
////////////////////////
#undef PDEBUG
#define PDEBUG(fmt, args...)  /* not debugging: nothing */

#define MAX_OPEN 32
#define ID_UNUSED 0xFF
static int lcmem_major = 0;  /* dynamic */
static spinlock_t mem_lock;
static struct class *lcmem_class;

/*
* should be adjusted according to memory application
*/
#define CHUNK_CLASS_NUMBERS  6  //

#define CHUNK_CLASS1_PAGENUMBERS    1  // 1 memory page in chunk
#define CHUNK_CLASS2_PAGENUMBERS    8  // 8 memory pages in chunk
#define CHUNK_CLASS3_PAGENUMBERS    16
#define CHUNK_CLASS4_PAGENUMBERS    32
#define CHUNK_CLASS5_PAGENUMBERS    48
#define CHUNK_CLASS6_PAGENUMBERS    64

#define CHUNK_CLASS1_NUMBERS    ((ON2_MEMORY_SIZE/PAGE_SIZE)-1)  // numbers of 1 page's chunk
#define CHUNK_CLASS2_NUMBERS    0 // numbers of 8 pages's chunk
#define CHUNK_CLASS3_NUMBERS    0  // numbers of 16 pages's
#define CHUNK_CLASS4_NUMBERS    0  // numbers of 32 pages
#define CHUNK_CLASS5_NUMBERS    0  // numbers of 48 pages
#define CHUNK_CLASS6_NUMBERS    0 // numbers of 64 pages

#define CHUNK_TOTAL_NUMBERS  ( CHUNK_CLASS1_NUMBERS+\
                               CHUNK_CLASS2_NUMBERS+\
                               CHUNK_CLASS3_NUMBERS+\
                               CHUNK_CLASS4_NUMBERS+\
                               CHUNK_CLASS5_NUMBERS+\
                               CHUNK_CLASS6_NUMBERS)

static int chunk_page_numbers[]= {
	CHUNK_CLASS1_PAGENUMBERS,
	CHUNK_CLASS2_PAGENUMBERS,
	CHUNK_CLASS3_PAGENUMBERS,
	CHUNK_CLASS4_PAGENUMBERS,
	CHUNK_CLASS5_PAGENUMBERS,
	CHUNK_CLASS6_PAGENUMBERS
};

static int *chunk_class = NULL;

/*allocated or free mem buffer list in BLOCK.
  NOTE: mem buffer is composed of one or more chunks.
        If mem buffer has one chunks, the on2_mem_list object just would be chunk*/
typedef struct on2_mem_list {
	int chunkNumbers;    // numbers of CONTINUOUS chunks of this mem buffer
	unsigned int bus_addr;        // phy memory address of this mem buffer, which is page address aligned
	int file_id;
	struct on2_mem_list *next;  // pointer to next mem buffer
	struct on2_mem_list *prev;  // pointer to prev mem buffer
	int begin_chunk_index;
#ifdef ON2_MEM_TEST
	int req_size;  // actual allocated size in the chunk
	int flag;  // 0: free;  1: allocated, composed of one one chunk or the first chunk of mem buffer;  >1: allocated, composed of more chunk;
#endif
}ON2_MEM_LIST_ST;


typedef struct on2_mem_block {
	int startChunkIndex;   // begin index number of this block
	int sizePerChunk;  // bytes of one chunk in this block = (pageNumbers in this chunk)*(PAGE_SIZE)
	int chunks;        // chunk numbers in this block
	int maxFreeChunks;        // max number of CONTINUOUS free chunks in this block
	ON2_MEM_LIST_ST *allocatedMemListPtr;
	ON2_MEM_LIST_ST *freeMemListPtr;
}ON2_MEM_BLOCK_ST;

typedef struct lcmem_private_data {
	int id;
	struct fasync_struct *async_queue;
}LCMEM_PRIVATE_DATA;


/*************************************
* static globle variable
************************************/
static ON2_MEM_LIST_ST *hlina_chunks = NULL; // memery alloc unit
static ON2_MEM_BLOCK_ST hlina_blocks[CHUNK_CLASS_NUMBERS];  // we have 7 blocks

LCMEM_PRIVATE_DATA *logon2mem=NULL;

LCMEM_PRIVATE_DATA lcmem_private_data[MAX_OPEN + 2];

static int isp_buffer_address = 0;
static int isp_buffer_size = 0;
static int isp_buffer_offset = 0;
static int isp_buffer_reset = 0;
static int is_isp_buffer_alloc = 0;

/*************************************
* function declare
************************************/
static int malloc_in_block(ON2_MEM_BLOCK_ST *pMemBlock, int req_size);
static int free_in_block(ON2_MEM_BLOCK_ST *pMemBlock,unsigned long busaddr);
static int search_in_block(ON2_MEM_BLOCK_ST *pMemBlock,unsigned long busaddr);
static int AllocMemory_l(unsigned *busaddr, unsigned int size, struct file *filp);
static int FreeMemory_l(unsigned long busaddr);
static void ResetMems_l(void);
static int closeDevice_l(struct file *filp);
void lcmem_print_mem_info(long memAllocSize, long memFreeAddr, \
			bool refresh_enable, bool signal_enable, bool kmsglog_enable);
extern int printk(const char *fmt, ...);


/*************************************
* function implement
************************************/
static long lcmem_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg)
/* FIX EDEN_Req00000001 END DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
{
	int err = 0;
	int ret;

	PDEBUG("ioctl cmd 0x%08x, arg 0x%x\n", cmd, (unsigned int)arg);

/* FIX EDEN_Req00000001 BEGIN DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
	if(/*inode == NULL ||*/ filp == NULL || arg == 0) {
		return -EFAULT;
	}
 /* FIX EDEN_Req00000001 END DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */


/*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
	if(_IOC_TYPE(cmd) != LCMEM_IOC_MAGIC)
		return -ENOTTY;
	if(_IOC_NR(cmd) > LCMEM_IOC_MAXNR)
		return -ENOTTY;

	if(_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
	if(err)
		return -EFAULT;

	switch (cmd) {
	case LCMEM_IOCHARDRESET:
	{
		PDEBUG("HARDRESET\n");
		ResetMems_l();

		break;
	}
	case LCMEM_IOCXGETBUFFER:
	{
		int result;
		int ret;
		MemallocParams memparams;

		spin_lock(&mem_lock);
		ret = __copy_from_user(&memparams, (const void *) arg, sizeof(memparams));
		result = AllocMemory_l(&memparams.busAddress, memparams.size, filp);
		ret = __copy_to_user((void *) arg, &memparams, sizeof(memparams));
		spin_unlock(&mem_lock);
		return result;
	}
	case LCMEM_IOCSFREEBUFFER:
	{
		unsigned long busaddr;

		spin_lock(&mem_lock);
		__get_user(busaddr, (unsigned long *) arg);
		ret =  FreeMemory_l(busaddr);
		if( -1 == ret)
			printk("free memory(addr 0x%x) failure\n", (unsigned int)busaddr);

		spin_unlock(&mem_lock);
		return ret;
	}
	case LCMEM_IOCTMEMINFO:
	{/*only used by logon2mem process, in order to printf on2 mem infomation*/
		unsigned int pMemInfoBuffer_phy=(unsigned int)ON2_PHY_MEM_INFO_BASE;

		logon2mem = filp->private_data;	 // we must evaluate it immediately

		PDEBUG("ioctl cmd 0x%08x, arg pointer 0x%x, arg=%d \n", cmd, (unsigned int)arg,*(bool*)arg);
		lcmem_print_mem_info(-1, -1, true, 0, *(bool*)arg);
		__put_user((unsigned int)pMemInfoBuffer_phy,(unsigned long *)arg);

		break;
	}
	default:
		return -ENOTTY;
	}
	return 0;
}

int lcmem_fasync(int fd, struct file *filp, int mode)

{
	struct lcmem_private_data *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int lcmem_open(struct inode *inode, struct file *filp)
{
	int i = 0;

	for(i = 0; i < MAX_OPEN + 1; i++) {
		if(i == MAX_OPEN)
			return -1;
		if(lcmem_private_data[i].id == ID_UNUSED) {
			lcmem_private_data[i].id = i;
			filp->private_data = lcmem_private_data + i;
			break;
		}
	}
	printk("dev %d opened\n",i);
	return 0;
}

static int lcmem_release(struct inode *inode, struct file *filp)
{
#ifdef PDEBUG
	LCMEM_PRIVATE_DATA *dev_private_data = (LCMEM_PRIVATE_DATA*) (filp->private_data);
#endif

	spin_lock(&mem_lock);
	if( 0 != closeDevice_l(filp)) {
		spin_unlock(&mem_lock);
		printk("close dev %d error!\n", dev_private_data->id);
		return -1;
	}
	spin_unlock(&mem_lock);
	lcmem_fasync(-1, filp, 0);

	printk("close dev %d success\n", dev_private_data->id);
	return 0;
}


/* VFS methods */
static struct file_operations lcmem_fops = {
	open:lcmem_open,
	release:lcmem_release,

	/* FIX EDEN_Req00000001 BEGIN DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
	unlocked_ioctl:lcmem_ioctl,
	//ioctl:lcmem_ioctl,
	/* FIX EDEN_Req00000001 END DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
	.fasync = lcmem_fasync,
};

int __init lcmem_probe(struct platform_device *pdev)
{
	int result;
	int i = 0;


	printk("lcmem_probe in: module init\n");
	printk("lcmem: Linear Memory Allocator, %s \n", "$Revision: 1.10 $");
	printk("lcmem: linear memory base = 0x%x \n", (unsigned int)ON2_PHY_MEM_BASE);

	spin_lock_init(&mem_lock);

	result = register_chrdev(lcmem_major, LC_MEM_DEV_NAME, &lcmem_fops);
	if(result < 0) {
		printk("lcmem: unable to get major %d\n", lcmem_major);
		goto err;
	}
	else if(result != 0) {/* this is for dynamic major */
		lcmem_major = result;
		lcmem_class = class_create(THIS_MODULE, LC_MEM_DEV_NAME);
		if (IS_ERR(lcmem_class)) {
			printk("lcmem: class_create failed\n");
			goto error_class_create;
		}
		device_create(lcmem_class, NULL, MKDEV(lcmem_major, 0), NULL, LC_MEM_DEV_NAME);

		printk(KERN_INFO "lcmem: probe successfully. Major = %d\n", lcmem_major);
	}

	hlina_chunks = kcalloc(CHUNK_TOTAL_NUMBERS,sizeof(ON2_MEM_LIST_ST),GFP_KERNEL);
	if(!hlina_chunks) {
		printk("lcmem: hlina_chunks kcalloc failed\n");
		goto error_hlina_chunks;
	}

	chunk_class = kmalloc(CHUNK_CLASS_NUMBERS*sizeof(int),GFP_KERNEL);
	if(!chunk_class) {
		printk("lcmem: chunk_class kmalloc failed\n");
		goto error_chunk_class;
	}

	{
	int *tmp = chunk_class;
	*tmp++ = CHUNK_CLASS1_NUMBERS;  // numbers of 1 page's chunk
	*tmp++ = CHUNK_CLASS2_NUMBERS;  // numbers of 8 pages's chunk
	*tmp++ = CHUNK_CLASS3_NUMBERS;  // numbers of 16 pages's
	*tmp++ = CHUNK_CLASS4_NUMBERS;  // numbers of 32 pages's
	*tmp++ = CHUNK_CLASS5_NUMBERS;  // numbers of 48 pages's
	*tmp++ = CHUNK_CLASS6_NUMBERS;  // numbers of 64 pages's
	}

	ResetMems_l();

	/* We keep a register of out customers, reset it */
	for(i = 0; i < MAX_OPEN; i++) {
		lcmem_private_data[i].id = ID_UNUSED;
		lcmem_private_data[i].async_queue = NULL;
	}

	lcmem_private_data[MAX_OPEN].id = MAX_OPEN;
	lcmem_private_data[MAX_OPEN].async_queue = NULL;

	lcmem_private_data[MAX_OPEN+1].id = MAX_OPEN+1;
	lcmem_private_data[MAX_OPEN].async_queue = NULL;

	return 0;
error_chunk_class:
	kfree(hlina_chunks);
error_hlina_chunks:
	device_destroy(lcmem_class, MKDEV(lcmem_major, 0));
	class_destroy(lcmem_class);
error_class_create:
	unregister_chrdev(lcmem_major, LC_MEM_DEV_NAME);
err:
	printk("lcmem: module not inserted\n");
	return result;
}

void __exit lcmem_remove(void)
{
	device_destroy(lcmem_class, MKDEV(lcmem_major, 0));
	class_destroy(lcmem_class);

	unregister_chrdev(lcmem_major, LC_MEM_DEV_NAME);

	if(hlina_chunks) {
		kfree(hlina_chunks);
		hlina_chunks = NULL;
	}
	if(chunk_class) {
		kfree(chunk_class);
		chunk_class = NULL;
	}

	printk("lcmem: module removed\n");
	return;
}

static struct platform_device lcmem_device = {
	.name = LC_MEM_DEV_NAME,
	.id = -1,
};

static struct platform_driver lcmem_driver = {
	.driver = {
		.name = LC_MEM_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.remove = __exit_p(lcmem_remove),
};

static int __init lcmem_init(void)
{
	platform_device_register(&lcmem_device);
	return platform_driver_probe(&lcmem_driver, lcmem_probe);
}

static void __exit lcmem_exit(void)
{
	platform_driver_unregister(&lcmem_driver);
	platform_device_unregister(&lcmem_device);
}

module_init(lcmem_init);
module_exit(lcmem_exit);

// reset on2 memory area
static void ResetMems_l(void)
{
	int i;
#ifdef ON2_MEM_TEST
	int j;
#endif
	int chunkIndex=0;
	unsigned int ba = (int)ON2_PHY_MEM_BASE;

	for(i=0;i<CHUNK_CLASS_NUMBERS;i++) {
		hlina_blocks[i].sizePerChunk = chunk_page_numbers[i]*PAGE_SIZE/*4096*/;
		hlina_blocks[i].chunks = chunk_class[i];
		hlina_blocks[i].maxFreeChunks = chunk_class[i];

		hlina_blocks[i].allocatedMemListPtr = NULL;

		// init freeMemListPtr
		if(i>0)
			chunkIndex += chunk_class[i-1];  // index of the first chunk in BLOCK

		#ifdef ON2_MEM_TEST
		for(j=0; j< hlina_blocks[i].chunks; j++) {
			hlina_chunks[chunkIndex+j].req_size = 0;
			hlina_chunks[chunkIndex+j].flag = 0;
			hlina_chunks[chunkIndex+j].begin_chunk_index = -1;
		}
		#endif

		hlina_chunks[chunkIndex].chunkNumbers = chunk_class[i];
		hlina_chunks[chunkIndex].bus_addr = ba;
		hlina_chunks[chunkIndex].file_id = ID_UNUSED;
		hlina_chunks[chunkIndex].next = NULL;
		hlina_chunks[chunkIndex].prev = NULL;
		hlina_chunks[chunkIndex].begin_chunk_index = chunkIndex;

		hlina_blocks[i].freeMemListPtr = &hlina_chunks[chunkIndex];
		hlina_blocks[i].startChunkIndex = chunkIndex;

		ba += hlina_blocks[i].sizePerChunk * hlina_blocks[i].chunks;
	}

	isp_buffer_offset = 0;
	isp_buffer_reset = 0;
	is_isp_buffer_alloc = 0;
	isp_buffer_address = 0;
	isp_buffer_size = 0;
}

/*  first, try malloc in appropriate BLOCK which chunk size is more than request size, a little more is best;
 *  second, try malloc in prev BLOCK, and find appropriate free mem buffer area(min size area which can be satisfied with request size) in freeMemList;
 *  third, try malloc in foregoinger BLOCK,....; and so on, untill malloc the request size or not.
 */
static int AllocMemory_l(unsigned *busaddr, unsigned int size, struct file *filp)
{
	int i = 0;
	LCMEM_PRIVATE_DATA *dev_private_data = (LCMEM_PRIVATE_DATA*) (filp->private_data);

	if(dev_private_data->id == MAX_OPEN + 1) {
		if(is_isp_buffer_alloc) {
			printk("isp buffer already alloc");
			*busaddr = isp_buffer_address;
			return 0;
		}
	}

	*busaddr = 0;

	for(i=0;i<CHUNK_CLASS_NUMBERS;i++) {
		if( hlina_blocks[i].sizePerChunk >= (int)size && hlina_blocks[i].maxFreeChunks>=1 )
			goto _DO_MALLOC_IN_BLOCK;
	}

	// we only can but search free chunk list
	for(i=CHUNK_CLASS_NUMBERS-1; i>=0; i--)	{
		if(hlina_blocks[i].maxFreeChunks * hlina_blocks[i].sizePerChunk >= (int)size)
			goto _DO_MALLOC_IN_BLOCK;
	}

	// have no memory, malloc failure
	PDEBUG("to malloc memory %d bytes failure: have no free memory!\n", size);
	goto _RETURN;

_DO_MALLOC_IN_BLOCK:
	// we get it, malloc mem in this block.
	if( (i = malloc_in_block(&hlina_blocks[i], size)) >= 0  &&  i < CHUNK_TOTAL_NUMBERS) {
		*busaddr = hlina_chunks[i].bus_addr;
		//hlina_chunks[i].file_id = *((int *) (filp->private_data));
		hlina_chunks[i].file_id = dev_private_data->id;
	} else
		ON2MEM_ASSERT(0); // not find chunk index

	if(!busaddr)
		ON2MEM_ASSERT(0);  // should not happen
_RETURN:
	if(*busaddr == 0) {
		printk("lcmem: Allocation FAILED: size = %d id[%d]!\n", size,dev_private_data->id);
		lcmem_print_mem_info(size, -1, true,true,true);
		return -1;
	} else {
		if(dev_private_data->id == MAX_OPEN + 1) {
			is_isp_buffer_alloc = 1;
			isp_buffer_address = *busaddr;
			isp_buffer_size = size;
			isp_buffer_offset = 0;
		}

		PDEBUG("LCMEM OK: busaddr 0x%p, size %d, dev ID %d\n",(void*)*busaddr,size, dev_private_data->id);
		return 0;
	}
}


static int FreeMemory_l(unsigned long busaddr)
{
	int i=0, ret=-1;
	int found = 0;
	unsigned long block_ba_end = (unsigned long)ON2_PHY_MEM_BASE;


	for(i=0;i<CHUNK_CLASS_NUMBERS;i++) {
		block_ba_end += hlina_blocks[i].sizePerChunk * hlina_blocks[i].chunks;

		if(block_ba_end > busaddr) {
			found = 1;
			break;
		}
	}
	if(!found) {
		printk("to free memory(addr 0x%x) error: cann't find this buffer!\n", (unsigned int)busaddr);
		lcmem_print_mem_info(-1,busaddr,true,true,true);
		ret = -1;
		ON2MEM_ASSERT(0);
	} else {
		int index = search_in_block(&hlina_blocks[i], busaddr);
		if(index < 0) {
			printk("FreeMemory failed can't find chunk");
			return -1;
		}
		PDEBUG("FreeMemory OK: busaddr 0x%p, dev ID %d\n",(void*)busaddr, hlina_chunks[index].file_id);

		if(hlina_chunks[index].file_id == MAX_OPEN + 1)
		{
			is_isp_buffer_alloc = 0;
			isp_buffer_address = 0;
			isp_buffer_size = 0;
			isp_buffer_offset = 0;
		}
		// we had fount mem BLOCK, which index is i
		ret=free_in_block(&hlina_blocks[i], busaddr);
	}

	return ret;
}


/* search blocks and free all memory which is allocated by device pointer to filp*/
static int closeDevice_l( struct file *filp)
{
	LCMEM_PRIVATE_DATA *dev_private_data = (LCMEM_PRIVATE_DATA*) (filp->private_data);
	int i;
	ON2_MEM_LIST_ST *pAllocatedMemListNext =NULL;

	if(NULL == filp) {
		printk("input parameter(struct file *filp = %p) is wrong\n", (void*)filp);
		return -1;
	}
	for(i=0;i<CHUNK_CLASS_NUMBERS;i++) {
		pAllocatedMemListNext = hlina_blocks[i].allocatedMemListPtr;

		while(pAllocatedMemListNext) {
			// search allocated mem list of this block
			if( pAllocatedMemListNext->file_id ==  (dev_private_data->id) ) {
				// find it, so free this buffer in the block
				if(-1 == free_in_block(&hlina_blocks[i], pAllocatedMemListNext->bus_addr)) {
					PDEBUG("free mem 0x%x in BLOCK[%d] failure\n",pAllocatedMemListNext->bus_addr,i);
					lcmem_print_mem_info(-1,pAllocatedMemListNext->bus_addr,true,true,true);
					ON2MEM_ASSERT(0);
				}
				//  free_in_block will change allocated list head of block, so we must update list pointer
				pAllocatedMemListNext = hlina_blocks[i].allocatedMemListPtr;
			} else
				pAllocatedMemListNext = pAllocatedMemListNext->next;
		}
	}// end:for

	dev_private_data->id = ID_UNUSED;

	return 0;
}



//=======================================
/* malloc req_size in the block, and refresh allocated/free list of this block
 * find the least available mem_list and allocate mem
 * return: element index of globle variable hlina_chunks[]
 */
static int malloc_in_block(ON2_MEM_BLOCK_ST *pMemBlock, int req_size)
{
	ON2_MEM_LIST_ST *pFreeMemListNext=NULL, *pAllocatedMemListNext=NULL;
	ON2_MEM_LIST_ST *pFreeMemListFound=NULL,*pAllocateMemList=NULL;
	int freeChunkNumbers=0, usedChunkNumbers;

#ifdef ON2_MEM_TEST
	int i;
#endif
	//check
	if(!pMemBlock) {
		ON2MEM_ASSERT(0);
		return -1;
	}

	pFreeMemListNext = pMemBlock->freeMemListPtr;
	ON2MEM_ASSERT(pFreeMemListNext);
	freeChunkNumbers = pMemBlock->maxFreeChunks;

	// serch pointer to appropriate(the smallest continuous chunk numbers) free mem list
	usedChunkNumbers =  req_size / (pMemBlock->sizePerChunk);
	usedChunkNumbers = !(req_size%pMemBlock->sizePerChunk) ? usedChunkNumbers:(usedChunkNumbers+1); // (req_size+ (!(req_size%pMemBlock->sizePerChunk)?0:(pMemBlock->sizePerChunk))  ) / (pMemBlock->sizePerChunk); // needed chunk numbers

	while(pFreeMemListNext) {
		if( (pFreeMemListNext->chunkNumbers >= usedChunkNumbers) && \
			(pFreeMemListNext->chunkNumbers <= freeChunkNumbers)) {
			// refresh the most appropriate mem list
			freeChunkNumbers = pFreeMemListNext->chunkNumbers;
			pFreeMemListFound = pFreeMemListNext;
		}

		pFreeMemListNext =  pFreeMemListNext->next;
	}

	ON2MEM_ASSERT(pFreeMemListFound);  // we should find mem list for allocating mem

	// we find the node, so allocate mem
	pAllocateMemList = pFreeMemListFound;
	pAllocateMemList->bus_addr = pFreeMemListFound->bus_addr;
	pAllocateMemList->chunkNumbers = usedChunkNumbers;

#ifdef ON2_MEM_TEST
	for(i=0; i<usedChunkNumbers; i++) {
		(pAllocateMemList+i)->req_size = pMemBlock->sizePerChunk;
		(pAllocateMemList+i)->flag = i+1;
		(pAllocateMemList+i)->begin_chunk_index = (int)(pAllocateMemList - hlina_chunks);
	}
	(pAllocateMemList+usedChunkNumbers-1)->req_size = (!(req_size%pMemBlock->sizePerChunk)) ? (pMemBlock->sizePerChunk):(req_size%pMemBlock->sizePerChunk);
#endif
	pAllocateMemList->begin_chunk_index = (int)(pAllocateMemList - hlina_chunks);


	//refresh free mem list
	if(usedChunkNumbers < freeChunkNumbers) {   // we have new free mem list,and insert it into free mem list of block
		pFreeMemListFound += usedChunkNumbers; // now, pFreeMemListFound is  pointer to a new free mem list
		pFreeMemListFound->bus_addr = pAllocateMemList->bus_addr + usedChunkNumbers*pMemBlock->sizePerChunk;
		pFreeMemListFound->chunkNumbers = freeChunkNumbers-usedChunkNumbers;
		pFreeMemListFound->file_id = ID_UNUSED;

		pFreeMemListFound->prev = pAllocateMemList->prev;  // pAllocateMemList pointer to original free mem buffer list
		pFreeMemListFound->next = pAllocateMemList->next;
		if(pAllocateMemList->prev) {
			pAllocateMemList->prev->next = pFreeMemListFound;
		} else
			pMemBlock->freeMemListPtr = pFreeMemListFound;

		if(pAllocateMemList->next)
			pAllocateMemList->next->prev = pFreeMemListFound;

		pFreeMemListFound->begin_chunk_index = (int)(pFreeMemListFound - hlina_chunks);
	} else if (usedChunkNumbers == freeChunkNumbers) {
		// exactly meet req size, so we have no new free mem list, and just refresh original free list
		// and delete this node from free list
		if(pAllocateMemList->prev) {// pAllocateMemList pointer to original free mem buffer list
			pAllocateMemList->prev->next = pAllocateMemList->next;
		} else
			pMemBlock->freeMemListPtr = pAllocateMemList->next;

		if(pAllocateMemList->next)
			pAllocateMemList->next->prev = pAllocateMemList->prev;
	}
	else
		ON2MEM_ASSERT(0); //should not happen

	// refresh max free chunk numbers
	pFreeMemListNext = pMemBlock->freeMemListPtr;
	if(pFreeMemListNext) {
		pMemBlock->maxFreeChunks = pFreeMemListNext->chunkNumbers;
		while(pFreeMemListNext)	{
			freeChunkNumbers = pFreeMemListNext->chunkNumbers;
			if(freeChunkNumbers > pMemBlock->maxFreeChunks)
				pMemBlock->maxFreeChunks = freeChunkNumbers;
			pFreeMemListNext = pFreeMemListNext->next;
		}
	}
	else
		pMemBlock->maxFreeChunks = 0;


	// refresh allocated mem list: insert allocated mem buffer (namely pAllocateMemList) into head allocated list of block
	pAllocatedMemListNext = pMemBlock->allocatedMemListPtr;
	pMemBlock->allocatedMemListPtr = pAllocateMemList;
	pAllocateMemList->prev = NULL;
	pAllocateMemList->next = pAllocatedMemListNext;
	if(pAllocatedMemListNext)
		pAllocatedMemListNext->prev = pAllocateMemList;

	return (int)(pAllocateMemList - hlina_chunks);
}

/* free memory pointer to  busaddr in the block, and refresh allocated/free list of this block
 * input:
 *        pMemBlock   pointer to BLOCK
 *        busaddr     memory addr which would be released
 * return: if success, 0;  else, -1
 */
static int free_in_block(ON2_MEM_BLOCK_ST *pMemBlock,unsigned long busaddr)
{
	ON2_MEM_LIST_ST *pFreeMemList=NULL, *pAllocatedMemListNext=NULL;
	ON2_MEM_LIST_ST *pMemListFound=NULL, *pfreeMemElement=NULL;
	int maxFreeChunkNumbers=-1;

#ifdef ON2_MEM_TEST
	int i;
#endif

	PDEBUG("will free memory(addr 0x%x)...\n", (unsigned int)busaddr);

	if(!pMemBlock || !(pMemBlock->allocatedMemListPtr)) {
		printk("input patameter error!\n");
		return -1;
	}

	// search allocated mem list
	pAllocatedMemListNext = pMemBlock->allocatedMemListPtr;
	while(pAllocatedMemListNext) {
		if(pAllocatedMemListNext->bus_addr == busaddr) {
			pfreeMemElement = pAllocatedMemListNext;
			break; // so, we find it
		}

		pAllocatedMemListNext = pAllocatedMemListNext->next;
	}

	/**********check**********/
	if(!pfreeMemElement) {
		printk("to free memory(addr 0x%x) in BLOCK(id=%d) error: cann't find this buffer!\n", (unsigned int)busaddr, (int)(pMemBlock-hlina_blocks)/sizeof(ON2_MEM_BLOCK_ST) );
		ON2MEM_ASSERT(0);
		return -1;
	}

	// refresh allocated mem list of BLOCK
	if(pfreeMemElement->prev)
		pfreeMemElement->prev->next = pfreeMemElement->next;
	else
		pMemBlock->allocatedMemListPtr = pfreeMemElement->next;

	if(pfreeMemElement->next)
		pfreeMemElement->next->prev = pfreeMemElement->prev;

	PDEBUG(" we find memory(addr 0x%x),used by devID %d\n", (unsigned int)busaddr, pfreeMemElement->file_id);
	// refresh free mem list of BLOCK
	pfreeMemElement->file_id = ID_UNUSED;

#ifdef ON2_MEM_TEST
	for(i=0; i<pfreeMemElement->chunkNumbers; i++) {
		(pfreeMemElement+i)->req_size = 0;
		(pfreeMemElement+i)->flag = 0;
		(pfreeMemElement+i)->begin_chunk_index=(int)(pfreeMemElement - hlina_chunks);
	}
#endif
	pfreeMemElement->begin_chunk_index = (int)(pfreeMemElement - hlina_chunks);

	//
	pFreeMemList = pMemBlock->freeMemListPtr;

	if(!pFreeMemList) {
		// original free list is empty in this BLOCK
		pfreeMemElement->next = pfreeMemElement->prev = NULL;
		pMemBlock->freeMemListPtr = pfreeMemElement;
		pMemBlock->maxFreeChunks =  pfreeMemElement->chunkNumbers;

		PDEBUG(" free memory(addr 0x%x) success\n", (unsigned int)busaddr);
		return 0;
	}

	// first, search old free list ,which end chunk is adjacent to pfreeMemElement
	// |origin freeList(pFreeMemList) | our free buffer |
	pMemListFound = NULL;
	while(pFreeMemList) {
		if( pFreeMemList + pFreeMemList->chunkNumbers ==  pfreeMemElement ) {
			// just only increment chunknumbers
			pFreeMemList->chunkNumbers += pfreeMemElement->chunkNumbers;
			maxFreeChunkNumbers = pFreeMemList->chunkNumbers;
			pMemListFound = pFreeMemList;  // save
		#ifdef ON2_MEM_TEST
			for(i=0; i<pfreeMemElement->chunkNumbers; i++)
				(pfreeMemElement+i)->begin_chunk_index = (int)(pFreeMemList - hlina_chunks);
		#endif
			pfreeMemElement->begin_chunk_index = (int)(pFreeMemList - hlina_chunks);

			break;
		}
		pFreeMemList = pFreeMemList->next;
	}

	// second, search free list, check whether one/anotherlist's chunks is adjacent
	// | our free buffer | origin freeList(pFreeMemList) |
	pFreeMemList = pMemBlock->freeMemListPtr;
	while(pFreeMemList) {
		if( (pfreeMemElement + pfreeMemElement->chunkNumbers) == pFreeMemList) {
			pfreeMemElement->chunkNumbers += pFreeMemList->chunkNumbers; // merge free chunks
			// delete old node, and insert new node pfreeMemElement
			if(pFreeMemList->next)
				pFreeMemList->next->prev = pfreeMemElement;

			pfreeMemElement->next = pFreeMemList->next;

			pfreeMemElement->prev = pFreeMemList->prev;
			if(pFreeMemList->prev)
				pFreeMemList->prev->next = pfreeMemElement;
			else
				pMemBlock->freeMemListPtr = pfreeMemElement;

			maxFreeChunkNumbers = pfreeMemElement->chunkNumbers;

		#ifdef ON2_MEM_TEST
			for(i=0; i<pfreeMemElement->chunkNumbers; i++)
				(pfreeMemElement+i)->begin_chunk_index = (int)(pfreeMemElement - hlina_chunks);
		#endif
			pfreeMemElement->begin_chunk_index = (int)(pfreeMemElement - hlina_chunks);

			if(pMemListFound) {// we need merge again
				pMemListFound->chunkNumbers += pFreeMemList->chunkNumbers;
				maxFreeChunkNumbers = pMemListFound->chunkNumbers;

				//  need only delete pfreeMemElement node
			#ifdef ON2_MEM_TEST
				for(i=0; i<pfreeMemElement->chunkNumbers; i++)
					(pfreeMemElement+i)->begin_chunk_index = (int)(pMemListFound - hlina_chunks);
			#endif
				pfreeMemElement->begin_chunk_index = (int)(pMemListFound - hlina_chunks);

				if(pfreeMemElement->prev)
					pfreeMemElement->prev->next = pfreeMemElement->next;
				else
					pMemBlock->freeMemListPtr = pfreeMemElement->next;

				if(pfreeMemElement->next)
					pfreeMemElement->next->prev = pfreeMemElement->prev;
			}
			break;
		}
		pFreeMemList = pFreeMemList->next;
	}

	if(maxFreeChunkNumbers == -1) {
		// means it is independent free buffer, so we just insert into head of free list
		pFreeMemList = pMemBlock->freeMemListPtr;
		pMemBlock->freeMemListPtr = pfreeMemElement;
		pfreeMemElement->prev = NULL;
		pfreeMemElement->next = pFreeMemList;
			if(pFreeMemList)
		pFreeMemList->prev = pfreeMemElement;

		maxFreeChunkNumbers = pfreeMemElement->chunkNumbers;
	}

	// refresh max CONTINUOUS free chunk numbers
	if(pMemBlock->maxFreeChunks < maxFreeChunkNumbers)
		pMemBlock->maxFreeChunks = maxFreeChunkNumbers;

	PDEBUG(" free memory(addr 0x%x) success\n", (unsigned int)busaddr);
	return 0;
}

/* search  memory pointer to  busaddr in the block
 * input:
 *        pMemBlock   pointer to BLOCK
 *        busaddr     memory addr which would be released
 * return: if success, 0;  else, -1
 */
static int search_in_block(ON2_MEM_BLOCK_ST *pMemBlock,unsigned long busaddr)
{
	ON2_MEM_LIST_ST *pAllocatedMemListNext=NULL;
	ON2_MEM_LIST_ST *pfreeMemElement=NULL;

	if(!pMemBlock || !(pMemBlock->allocatedMemListPtr) ) {
		printk("input patameter error!\n");
		return -1;
	}

	// search allocated mem list
	pAllocatedMemListNext = pMemBlock->allocatedMemListPtr;
	while(pAllocatedMemListNext) {
		if(pAllocatedMemListNext->bus_addr == busaddr) {
			pfreeMemElement = pAllocatedMemListNext;
			break; // so, we find it
		}

		pAllocatedMemListNext = pAllocatedMemListNext->next;
	}

	/**********check**********/
	if(!pfreeMemElement) {
		printk("to free memory(addr 0x%x) in BLOCK(id=%d) error: cann't find this buffer!\n", (unsigned int)busaddr, (int)(pMemBlock-hlina_blocks)/sizeof(ON2_MEM_BLOCK_ST) );
		return -1;
	}

	return (int)(pfreeMemElement - hlina_chunks);
}

/*only used by logon2mem process, in order to printf on2 mem infomation.
  input parameter:
    memAllocSize: generally, it is -1. only when malloc operation failure, it is valued bytes of request buffer.
    memFreeAddr: generally, it is -1. only when free operation failure, it is valued pointer to buffer which want to be released.
    refresh_enable: if 1, we will get the newest infomation; if 0, reserve last infomation(note: we will get infomation when
                   this function be firstly called.).
    signal_enable: if it is true(generally, it is called when some error be happened,
                   such as malloc/free failure), we will send signal to logon2mem process,
                   then logon2mem can printf info.
    kmsglog_enable: if 1, save mem info into kmsg.log file; if 0, not save into kmsg.log file
  */
void lcmem_print_mem_info(long memAllocSize, long memFreeAddr,\
					bool refresh_enable, bool signal_enable, bool kmsglog_enable)
{
	static bool flagPrint=0;

	int i,infolen=0;
	ON2_MEM_LIST_ST *pAllocatedMemListNext = NULL, *pFreeMemListNext=NULL;
	char* pMemInfoBuffer=NULL; // virtual addr

	phys_addr_t pMemInfoBuffer_phy = ON2_PHY_MEM_INFO_BASE;

	if(!refresh_enable && flagPrint)// if need not refresh, just to use old mem info log
		goto SEND_SIGNAL_TO_LOGON2MEM_PROCESS;

	flagPrint = 1;

	pMemInfoBuffer = ioremap((phys_addr_t)pMemInfoBuffer_phy, ON2_PHY_MEM_INFO_SIZE);
	//PDEBUG("virtual addr of on2_mem_info area=0x%x \n",(unsigned int)pMemInfoBuffer);

	pMemInfoBuffer[ON2_PHY_MEM_INFO_SIZE-1]=0;

	//PDEBUG("the end bytes of on2_mem_info area=0x%x set to 0 \n",(unsigned int)pMemInfoBuffer);
	if(-1 != memAllocSize)
		infolen += snprintf(pMemInfoBuffer+infolen, ON2_PHY_MEM_INFO_SIZE-infolen-1, \
			"ERROR: try to malloc %d B memory buffer!\n\n", (int)memAllocSize);

	if(-1 != memFreeAddr)
		infolen += snprintf(pMemInfoBuffer+infolen, ON2_PHY_MEM_INFO_SIZE-infolen-1, \
			"ERROR: try to free 0x%x memory buffer!\n\n", (unsigned int)memFreeAddr);


	for(i=0;i<CHUNK_CLASS_NUMBERS;i++) {
		infolen += snprintf(pMemInfoBuffer+infolen, ON2_PHY_MEM_INFO_SIZE-infolen-1, "BLOCK[%d]: bytes/chunk %d B, max free continuous chunks number %d(total chunks %d)!\n",\
					i,\
					hlina_blocks[i].sizePerChunk,\
					hlina_blocks[i].maxFreeChunks,\
					hlina_blocks[i].chunks);
		if( infolen < 0 || (infolen == (ON2_PHY_MEM_INFO_SIZE-1)) )
			break;

		// log memory which had allocated
		pAllocatedMemListNext = hlina_blocks[i].allocatedMemListPtr;
		if(!pAllocatedMemListNext)
			infolen += snprintf(pMemInfoBuffer+infolen,ON2_PHY_MEM_INFO_SIZE-infolen-1,"    no allocated memory.\n");
		if( infolen < 0 || (infolen == (ON2_PHY_MEM_INFO_SIZE-1)))
			break;

		while(pAllocatedMemListNext) {
		#ifdef ON2_MEM_TEST
			int user_req_size=0, m;
			for(m=0;m < (pAllocatedMemListNext->chunkNumbers);m++)
				user_req_size += hlina_chunks[pAllocatedMemListNext->begin_chunk_index+m].req_size;

			infolen += snprintf(pMemInfoBuffer+infolen,ON2_PHY_MEM_INFO_SIZE-infolen-1,"    used mem[0x%x]: begin chunk id[%d], %d chunks. devID[%d]. Request mem size[%d B].\n",\
			pAllocatedMemListNext->bus_addr,\
			pAllocatedMemListNext->begin_chunk_index-hlina_blocks[i].startChunkIndex, \
			pAllocatedMemListNext->chunkNumbers,\
			pAllocatedMemListNext->file_id,\
			user_req_size);
		#else
			infolen += snprintf(pMemInfoBuffer+infolen,ON2_PHY_MEM_INFO_SIZE-infolen-1,"    used mem[0x%x]: begin chunk id[%d], %d chunks. devID[%d].\n",\
			pAllocatedMemListNext->bus_addr,\
			pAllocatedMemListNext->begin_chunk_index-hlina_blocks[i].startChunkIndex, \
			pAllocatedMemListNext->chunkNumbers,\
			pAllocatedMemListNext->file_id);
		#endif
			if( infolen < 0 || (infolen == (ON2_PHY_MEM_INFO_SIZE-1)))
				break;

			pAllocatedMemListNext = pAllocatedMemListNext->next;
		}

		// log memory which is available
		pFreeMemListNext = hlina_blocks[i].freeMemListPtr;
		if(!pFreeMemListNext)
			infolen += snprintf(pMemInfoBuffer+infolen, ON2_PHY_MEM_INFO_SIZE-infolen-1, "    no free memory.\n");
		if( infolen < 0 || (infolen == (ON2_PHY_MEM_INFO_SIZE-1)))
			break;

		while(pFreeMemListNext) {
			infolen += snprintf(pMemInfoBuffer+infolen, ON2_PHY_MEM_INFO_SIZE-infolen-1, "    free mem[0x%x]: begin chunk id[%d], %d chunks. devID[%d].\n",\
			pFreeMemListNext->bus_addr, \
			pFreeMemListNext->begin_chunk_index-hlina_blocks[i].startChunkIndex, \
			pFreeMemListNext->chunkNumbers,\
			pFreeMemListNext->file_id);
			if( infolen < 0 || (infolen == (ON2_PHY_MEM_INFO_SIZE-1)))
				break;

			pFreeMemListNext = pFreeMemListNext->next;
		}
	}
	iounmap(pMemInfoBuffer);

	if(kmsglog_enable) {
		// put log into kmsg.log file
		for(i=0;i<CHUNK_CLASS_NUMBERS;i++) {
			printk("%s LINE%d: BLOCK[%d]: max free continuous chunks number is %d(total chunks %d)!\n",__FUNCTION__,__LINE__,i, hlina_blocks[i].maxFreeChunks,hlina_blocks[i].chunks);

			// log memory which had allocated
			pAllocatedMemListNext = hlina_blocks[i].allocatedMemListPtr;
			if(!pAllocatedMemListNext)
				printk(" BLOCK[%d]: no allocated memory.\n",i);
			while(pAllocatedMemListNext) {
				printk(" BLOCK[%d]:used mem[0x%x]: begin chunk id[%d], chunk numbers[%d], bytes per chunk[%d].\n",i, pAllocatedMemListNext->bus_addr,pAllocatedMemListNext->begin_chunk_index, pAllocatedMemListNext->chunkNumbers,hlina_blocks[i].sizePerChunk);
				pAllocatedMemListNext = pAllocatedMemListNext->next;
			}

			// log memory which is available
			pFreeMemListNext = hlina_blocks[i].freeMemListPtr;
			if(!pFreeMemListNext)
				printk(" BLOCK[%d]: no free memory.\n",i);

			while(pFreeMemListNext) {
				printk(" BLOCK[%d]:free mem[0x%x]: begin chunk id[%d], chunk numbers[%d], bytes per chunk[%d].\n",i, pFreeMemListNext->bus_addr, pFreeMemListNext->begin_chunk_index, pFreeMemListNext->chunkNumbers,hlina_blocks[i].sizePerChunk);
				pFreeMemListNext = pFreeMemListNext->next;
			}
		}
	}
SEND_SIGNAL_TO_LOGON2MEM_PROCESS:
	if (logon2mem && logon2mem->async_queue && signal_enable) {
		kill_fasync(&logon2mem->async_queue, SIGIO, POLL_IN);
	}
	return;
}

/* FIX L1810_Bug00001936 BEGIN DATE:2012-10-29 AUTHOR NAME ZHUXIAOPING */
void* malloc_lcmem(unsigned int* phys_addr, unsigned int size, lcmem_alloc_type flag)
{
	int ret;
	struct file filp;
	void* virt_addr = NULL;

	filp.private_data = lcmem_private_data + MAX_OPEN; //special id for  external calling
	if(flag == LCMEM_ALLOC_ISP_MEM) {
		filp.private_data = lcmem_private_data + MAX_OPEN + 1;
	}

	spin_lock(&mem_lock);
	ret = AllocMemory_l(phys_addr, size, &filp);
	spin_unlock(&mem_lock);

	if(ret == 0) {
		virt_addr = ioremap(*phys_addr, size);
		if(virt_addr == NULL) {
			printk("malloc_lcmem failed due to ioremap");
			spin_lock(&mem_lock);
			FreeMemory_l((unsigned long)(*phys_addr));
			spin_unlock(&mem_lock);
			*phys_addr = 0;
		}
	}
	PDEBUG("malloc_lcmem phys_addr %p virt_addr %p size %d\n", *phys_addr, virt_addr, size);

	return virt_addr;
}

void free_lcmem(void* phys_addr, void* virt_addr)
{

	PDEBUG("free_lcmem phys_addr %p virt_addr %p\n", phys_addr, virt_addr);

	iounmap(virt_addr);

	spin_lock(&mem_lock);
	FreeMemory_l((unsigned long)phys_addr);
	spin_unlock(&mem_lock);
}
/* FIX L1810_Bug00001936 END DATE:2012-10-29 AUTHOR NAME ZHUXIAOPING */
void* malloc_phys_lcmem(unsigned int size, lcmem_alloc_type flag)
{
	int ret;
	struct file filp;
	unsigned int phys_addr = 0;

	filp.private_data = lcmem_private_data + MAX_OPEN; //special id for  external calling
	if(flag == LCMEM_ALLOC_ISP_MEM) {
		filp.private_data = lcmem_private_data + MAX_OPEN + 1;
	}

	spin_lock(&mem_lock);
	ret = AllocMemory_l(&phys_addr, size, &filp);
	spin_unlock(&mem_lock);

	PDEBUG("malloc_lcmem phys_addr 0x%x size %d\n", phys_addr, size);

	return (void *)phys_addr;
}

void free_phys_lcmem(void* phys_addr)
{
	PDEBUG("free_lcmem phys_addr %p\n", phys_addr);

	spin_lock(&mem_lock);
	FreeMemory_l((unsigned long)phys_addr);
	spin_unlock(&mem_lock);
}

void* get_isp_buffer(unsigned int size)
{
	int bus_addr = 0;

	spin_lock(&mem_lock);

	if(is_isp_buffer_alloc)	{
		if(isp_buffer_reset) {
			printk("get_isp_buffer reset\n");
			isp_buffer_reset = 0;
			isp_buffer_offset = 0;
		}

		PDEBUG("isp_buffer_offset:%d, size:%d.\n", isp_buffer_offset, size);

		if(isp_buffer_offset + size <= isp_buffer_size) {
			bus_addr = isp_buffer_address + isp_buffer_offset;
			isp_buffer_offset += size;
		} else {
			printk("get_isp_buffer out of range\n");
		}
	} else {
		printk("get_isp_buffer isp buffer not alloc\n");
	}

	spin_unlock(&mem_lock);

	return (void *)bus_addr;
}

void release_isp_buffer(void* phys_addr)
{
	spin_lock(&mem_lock);

	if(is_isp_buffer_alloc)	{
		if(((unsigned int)phys_addr < isp_buffer_address) || ((unsigned int)phys_addr >= isp_buffer_address + isp_buffer_size)) {
			printk("release_isp_buffer out of range\n");
		}
		if(isp_buffer_reset) {
			isp_buffer_reset = 0;
			isp_buffer_offset = 0;
		}
	} else if(isp_buffer_reset) {
		isp_buffer_reset = 0;
		isp_buffer_offset = 0;
		printk("release_isp_buffer isp buffer not alloc\n");
	}

	spin_unlock(&mem_lock);

	return;
}

void reset_isp_buffer()
{
	spin_lock(&mem_lock);
	isp_buffer_reset = 1;
	printk("reset isp mem\n");
	spin_unlock(&mem_lock);

	return;
}


