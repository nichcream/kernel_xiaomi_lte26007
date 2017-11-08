/*
 * Copyright (C) 2012 LeadCore, Inc.
 * Author: Haidong Fu<fuhaidong@leadcoretech.com>
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
* Change History:
********************************************************************************
*  1.0.0     L1810_Bug00000243       fuhaidong     2012-6-14    original version, add on2 psm
*********************************************************************************/
#ifndef _ON2PSM_H_
#define _ON2PSM_H_
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

#include <linux/semaphore.h>

//#define ON2PSM_DEBUG

/*
 * Macros to help debugging
 */
#undef PDEBUG   /* undef it, just in case */
#ifdef ON2PSM_DEBUG
#  ifdef __KERNEL__
	/* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_INFO "on2psm: " fmt, ## args)
#  else
	/* This one for user space */
#    define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define ON2PSM_IOC_MAGIC  'k'

/*for lock*/
#define  ON2_MUTEX_LOCK      _IO(ON2PSM_IOC_MAGIC,1)
#define  ON2_MUTEX_UNLOCK    _IO(ON2PSM_IOC_MAGIC,2)

/*for on2 pwr clk*/
#define  ON2_PWR_G1_DEC_CLK_ENABLE  _IO(ON2PSM_IOC_MAGIC,  3) 
#define  ON2_PWR_7280_ENC_CLK_ENABLE  _IO(ON2PSM_IOC_MAGIC,  4) 
#define  ON2_PWR_H1_ENC_CLK_ENABLE  _IO(ON2PSM_IOC_MAGIC,  5) 
#define  ON2_PWR_G1_DEC_CLK_DISABLE  _IO(ON2PSM_IOC_MAGIC,  6) 
#define  ON2_PWR_7280_ENC_CLK_DISABLE  _IO(ON2PSM_IOC_MAGIC,  7) 
#define  ON2_PWR_H1_ENC_CLK_DISABLE  _IO(ON2PSM_IOC_MAGIC, 8) 

/*for power*/
#define  ON2POWER_DOWN_UPDATEFLAG  _IO(ON2PSM_IOC_MAGIC, 9) 
#define  ON2POWER_UP_UPDATEFLAG  _IO(ON2PSM_IOC_MAGIC, 10) 

#define ON2_RESET_FLAG _IO(ON2PSM_IOC_MAGIC,11) 

#define ON2PSM_IOC_MAXNR 11

#if defined(CONFIG_ARCH_LC181X)
typedef struct
{
	struct device *dev;

	int on2_pwr_dec_clk_flag;
	int on2_pwr_enc0_clk_flag;
	int on2_pwr_enc1_clk_flag;
	int on2_pwr_enc_bus_clk_flag;
	int on2_pwr_power_on_flag;

	int on2_ddr_axi_config_flag;
	
	int irq;
	struct fasync_struct *async_queue;/*async notify*/
	
} on2psm_t;
#elif defined(CONFIG_ARCH_LC186X)
typedef struct
{
	struct device *dev;

	int on2_pwr_codec_clk_flag; /*G1 & 7280*/
	int on2_pwr_sw1_codec_clk_flag;
	int on2_pwr_peri_sw1_codec_clk_flag;
	int on2_pwr_enc_clk_flag; /*H1*/
	int on2_pwr_sw1_enc_clk_flag;
	int on2_pwr_peri_sw1_enc_clk_flag;

	int on2_pwr_power_on_flag;

	int irq;
	struct fasync_struct *async_queue;/*async notify*/
} on2psm_t;
#endif

#endif /* !_ON2PSM_H_ */
