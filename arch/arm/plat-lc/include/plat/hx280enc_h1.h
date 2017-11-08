/*
 * Copyright (C) 2011 Wind River Systems, Inc.
 * Author: Kai Liu <kai.liu@windriver.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
*/
 /*******************************************************************************
* CR History:
********************************************************************************
*  1.0.0     EDEN_Req00000003       fuhaidong     2012-4-26    modify hardware  resource for 1810*
*********************************************************************************/

#ifndef _HX280ENC_H1_H_
#define _HX280ENC_H1_H_
#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_pfn_range
	SetPageReserved
	ClearPageReserved
*/
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>

/* request_irq(), free_irq() */
#include <linux/interrupt.h>

/* needed for virt_to_phys() */
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/ioctl.h>    /* needed for the _IOW etc stuff used later */

#include <asm/irq.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

//#define HX280ENC_H1_DEBUG

/*
 * Macros to help debugging
 */

#undef PDEBUG   /* undef it, just in case */
#ifdef HX280ENC_H1_DEBUG
#ifdef __KERNEL__
	/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk( KERN_INFO "hx280enc_h1: " fmt, ## args)
#else
	/* This one for user space */
#define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#endif
#else
#define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

#define HX280ENC_H1_CPU_FREQ  (416000000)
#define HX280ENC_H1_CPU_FREQ_MIN  (13000000)

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define HX280ENC_H1_IOC_MAGIC  'k'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
 /*
  * #define HX280ENC_H1_IOCGBUFBUSADDRESS _IOR(HX280ENC_H1_IOC_MAGIC,  1, unsigned long *)
  * #define HX280ENC_H1_IOCGBUFSIZE       _IOR(HX280ENC_H1_IOC_MAGIC,  2, unsigned int *)
  */
#define HX280ENC_H1_IOCGHWOFFSET      _IOR(HX280ENC_H1_IOC_MAGIC,  3, unsigned long *)
#define HX280ENC_H1_IOCGHWIOSIZE      _IOR(HX280ENC_H1_IOC_MAGIC,  4, unsigned int *)
#define HX280ENC_H1_IOC_CLI           _IO(HX280ENC_H1_IOC_MAGIC,  5)
#define HX280ENC_H1_IOC_STI           _IO(HX280ENC_H1_IOC_MAGIC,  6)
#define HX280ENC_H1_IOCXVIRT2BUS      _IOWR(HX280ENC_H1_IOC_MAGIC,  7, unsigned long *)

#define HX280ENC_H1_IOCHARDRESET      _IO(HX280ENC_H1_IOC_MAGIC, 8)   /* debugging tool */

#define HX280ENC_H1_IOC_MAXNR 8

#if defined(CONFIG_ARCH_LC181X)
typedef struct
{
	struct device *dev;
	char *buffer;
	unsigned int buffsize;
	unsigned long iobaseaddr;
	unsigned int iosize;
	volatile u8 *hwregs;
	unsigned int irq;
	struct fasync_struct *async_queue;
} hx280enc_h1_t;
#else
/* here's all the must remember stuff */
typedef struct
{
    char *buffer;
    unsigned int buffsize;
    unsigned long iobaseaddr;
    unsigned int iosize;
    volatile u8 *hwregs;
    unsigned int irq;
    struct fasync_struct *async_queue;
} hx280enc_h1_t;
#endif

#endif /* !_HX280ENC_H1_H_ */
