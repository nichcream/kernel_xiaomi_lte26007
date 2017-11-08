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

#ifndef _HX170DEC_H_
#define _HX170DEC_H_
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

//#define HX170DEC_DEBUG

/*
 * Macros to help debugging
 */
#undef PDEBUG   /* undef it, just in case */
#ifdef HX170DEC_DEBUG
#  ifdef __KERNEL__
	/* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "hx170dec: " fmt, ## args)
#  else
	/* This one for user space */
#    define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

#define HX170DEC_CPU_FREQ  (416000000)
#define HX170DEC_CPU_FREQ_MIN  (13000000)

struct core_desc
{
    __u32 id; /* id of the core */
    __u32 *regs; /* pointer to user registers */
    __u32 size; /* size of register space */
};

#if defined(CONFIG_ARCH_LC181X)
typedef struct
{
	struct device *dev;
	char *buffer;
	unsigned long iobaseaddr;
	unsigned int iosize;
	volatile u8 *hwregs;
	int irq;
	struct fasync_struct *async_queue_dec;
	struct fasync_struct *async_queue_pp;
} hx170dec_t;
#else
#define HXDEC_MAX_CORES                 4

#define HANTRO_DEC_REGS                 60
#define HANTRO_PP_REGS                  41

#define HANTRO_DEC_FIRST_REG            0
#define HANTRO_DEC_LAST_REG             59
#define HANTRO_PP_FIRST_REG             60
#define HANTRO_PP_LAST_REG              100

/* here's all the must remember stuff */
typedef struct
{
    char *buffer;
    unsigned int iosize;
    volatile u8 *hwregs[HXDEC_MAX_CORES];
    int irq;
    int cores;
    struct fasync_struct *async_queue_dec;
    struct fasync_struct *async_queue_pp;
} hx170dec_t;
#endif

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define HX170DEC_IOC_MAGIC  'k'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

#define HX170DEC_PP_INSTANCE       _IO(HX170DEC_IOC_MAGIC, 1)   /* the client is pp instance */
#define HX170DEC_HW_PERFORMANCE    _IO(HX170DEC_IOC_MAGIC, 2)   /* decode/pp time for HW performance */
#define HX170DEC_IOCGHWOFFSET      _IOR(HX170DEC_IOC_MAGIC,  3, unsigned long *)
#define HX170DEC_IOCGHWIOSIZE      _IOR(HX170DEC_IOC_MAGIC,  4, unsigned int *)

#define HX170DEC_IOC_CLI           _IO(HX170DEC_IOC_MAGIC,  5)
#define HX170DEC_IOC_STI           _IO(HX170DEC_IOC_MAGIC,  6)

#define HX170DEC_IOC_MC_OFFSETS    _IOR(HX170DEC_IOC_MAGIC, 7, unsigned long *)
#define HX170DEC_IOC_MC_CORES      _IOR(HX170DEC_IOC_MAGIC, 8, unsigned int *)


#define HX170DEC_IOCS_DEC_PUSH_REG  _IOW(HX170DEC_IOC_MAGIC, 9, struct core_desc *)
#define HX170DEC_IOCS_PP_PUSH_REG   _IOW(HX170DEC_IOC_MAGIC, 10, struct core_desc *)

#define HX170DEC_IOCH_DEC_RESERVE   _IO(HX170DEC_IOC_MAGIC, 11)
#define HX170DEC_IOCT_DEC_RELEASE   _IO(HX170DEC_IOC_MAGIC, 12)
#define HX170DEC_IOCQ_PP_RESERVE    _IO(HX170DEC_IOC_MAGIC, 13)
#define HX170DEC_IOCT_PP_RELEASE    _IO(HX170DEC_IOC_MAGIC, 14)

#define HX170DEC_IOCX_DEC_WAIT      _IOWR(HX170DEC_IOC_MAGIC, 15, struct core_desc *)
#define HX170DEC_IOCX_PP_WAIT       _IOWR(HX170DEC_IOC_MAGIC, 16, struct core_desc *)

#define HX170DEC_IOCS_DEC_PULL_REG  _IOWR(HX170DEC_IOC_MAGIC, 17, struct core_desc *)
#define HX170DEC_IOCS_PP_PULL_REG   _IOWR(HX170DEC_IOC_MAGIC, 18, struct core_desc *)

#define HX170DEC_IOCG_CORE_WAIT     _IOR(HX170DEC_IOC_MAGIC, 19, int *)

#define HX170DEC_IOX_ASIC_ID        _IOWR(HX170DEC_IOC_MAGIC, 20, __u32 *)

#define HX170DEC_DEBUG_STATUS       _IO(HX170DEC_IOC_MAGIC, 29)

#define HX170DEC_IOC_MAXNR 29

#define DWL_MPEG2_E         31  /* 1 bit */
#define DWL_VC1_E           29  /* 2 bits */
#define DWL_JPEG_E          28  /* 1 bit */
#define DWL_MPEG4_E         26  /* 2 bits */
#define DWL_H264_E          24  /* 2 bits */
#define DWL_VP6_E           23  /* 1 bit */
#define DWL_RV_E            26  /* 2 bits */
#define DWL_VP8_E           23  /* 1 bit */
#define DWL_VP7_E           24  /* 1 bit */
#define DWL_WEBP_E          19  /* 1 bit */
#define DWL_AVS_E           22  /* 1 bit */
#define DWL_PP_E            16  /* 1 bit */

#define HX170_IRQ_STAT_DEC          1
#define HX170_IRQ_STAT_DEC_OFF      (HX170_IRQ_STAT_DEC * 4)
#define HX170_IRQ_STAT_PP           60
#define HX170_IRQ_STAT_PP_OFF       (HX170_IRQ_STAT_PP * 4)

#define HX170PP_SYNTH_CFG           100
#define HX170PP_SYNTH_CFG_OFF       (HX170PP_SYNTH_CFG * 4)
#define HX170DEC_SYNTH_CFG          50
#define HX170DEC_SYNTH_CFG_OFF      (HX170DEC_SYNTH_CFG * 4)
#define HX170DEC_SYNTH_CFG_2        54
#define HX170DEC_SYNTH_CFG_2_OFF    (HX170DEC_SYNTH_CFG_2 * 4)


#define HX170_DEC_E                 0x01
#define HX170_PP_E                  0x01
#define HX170_DEC_ABORT             0x20
#define HX170_DEC_IRQ_DISABLE       0x10
#define HX170_PP_IRQ_DISABLE        0x10
#define HX170_DEC_IRQ               0x100
#define HX170_PP_IRQ                0x100

#endif /* !_HX170DEC_H_ */
