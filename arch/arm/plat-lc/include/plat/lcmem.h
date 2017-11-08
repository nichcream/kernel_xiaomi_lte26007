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
--  Abstract :
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: lcmem.h,v $
--  $Date: 2007/03/27 11:06:42 $
--  $Revision: 1.1 $
--
------------------------------------------------------------------------------*/


#ifndef _HMP4ENC_H_
#define _HMP4ENC_H_

#include <linux/ioctl.h>    /* needed for the _IOW etc stuff used later */

/*
 * Macros to help debugging
 */
#undef PDEBUG   /* undef it, just in case */
#ifdef LCMEM_DEBUG
#  ifdef __KERNEL__
    /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "lcmem: " fmt, ## args)
#  else
    /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif
/*
 * Ioctl definitions
 */
/* Use 'k' as magic number */
#define LCMEM_IOC_MAGIC  'k'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
#define LCMEM_IOCXGETBUFFER         _IOWR(LCMEM_IOC_MAGIC,  1, unsigned long)
#define LCMEM_IOCSFREEBUFFER        _IOW(LCMEM_IOC_MAGIC,  2, unsigned long)

#define LCMEM_IOCTMEMINFO   _IOWR(LCMEM_IOC_MAGIC,3,unsigned long)

/* ... more to come */
#define LCMEM_IOCHARDRESET       _IO(LCMEM_IOC_MAGIC, 15) /* debugging tool */
#define LCMEM_IOC_MAXNR 15

typedef enum
{
	LCMEM_ALLOC_NORMAL = 0,
	LCMEM_ALLOC_ISP_MEM,
} lcmem_alloc_type;


/*device name*/
#define LC_MEM_DEV_NAME "lcmem"

#ifndef ON2MAP_MODULE_PATH
#define ON2MAP_MODULE_PATH          "/dev/on2map"
#endif

#ifndef LCMEM_MODULE_PATH
#define LCMEM_MODULE_PATH          "/dev/lcmem"
#endif

typedef struct {
    unsigned busAddress;
    unsigned size;
}MemallocParams;

/* FIX L1810_Bug00001936 BEGIN DATE:2012-10-29 AUTHOR NAME ZHUXIAOPING */
void* malloc_lcmem(unsigned int* phys_addr, unsigned int size, lcmem_alloc_type flag);
void free_lcmem(void *phys_addr, void* virt);
/* FIX L1810_Bug00001936 END DATE:2012-10-29 AUTHOR NAME ZHUXIAOPING */
void* malloc_phys_lcmem(unsigned int size, lcmem_alloc_type flag);
void free_phys_lcmem(void* phys_addr);

void* get_isp_buffer(unsigned int size);
void release_isp_buffer(void* phys_addr);
void reset_isp_buffer(void);

#endif /* _HMP4ENC_H_ */
