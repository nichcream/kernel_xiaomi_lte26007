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
--  Abstract : 6280/7280/8270/8290/H1 Encoder device driver (kernel module)

--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: hx280enc_h1.c,v $
--  $Date: 2009/03/31 10:15:57 $
--  $Revision: 1.6 $
--
------------------------------------------------------------------------------*/
 /*******************************************************************************
* CR History:
********************************************************************************
*  1.0.0     EDEN_Req00000003       fuhaidong     2012-4-26    modify hardware  resource for 1810*
*  1.0.1     L1810_Bug00000243       fuhaidong     2012-6-14    add on2 psm, here to abandon clk operation
*  1.0.2     L1810_Bug00000393    zhangziming    2012-6-19   modify 7280 and h1 isr mode to polling for vt*
*********************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range
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

#include <linux/moduleparam.h>
/* request_irq(), free_irq() */
#include <linux/interrupt.h>
#include <linux/sched.h>

/* needed for virt_to_phys() */
#include <asm/io.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>

#include <asm/irq.h>

#include <linux/version.h>


#include <plat/hx280enc_h1.h>
#include <asm/signal.h>
#include <asm-generic/siginfo.h>

#include <plat/clock.h>
#include <mach/irqs.h>
#include <mach/comip-regs.h>
#include <plat/comip-smmu.h>

#if defined(CONFIG_ARCH_LC186X)
#include <linux/delay.h>
#include <plat/hardware.h>
#endif

#if defined(CONFIG_ARCH_LC181X)
#define ENC_IO_BASE                 ENCODER1_BASE
#define ENC_IO_SIZE                 (164 * 4)    /* bytes */
#define ENC_IRQ                     INT_ON2_ENCODER1
#elif defined(CONFIG_ARCH_LC186X)
#define ENC_IO_BASE                 ON2_ENCODER1_BASE
#define ENC_IO_SIZE                 (300 * 4)    /* bytes */
#define ENC_IRQ                     INT_ON2_ENCODER1
#endif

/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int hx280enc_h1_major = 0;

static struct class *hx280enc_h1_class;

/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
#if defined(CONFIG_ARCH_LC181X)
static struct clk *on2_encode_clk = NULL;
static struct clk *on2_e1_clk = NULL;
static struct clk *on2_ebus_clk = NULL;
#elif defined(CONFIG_ARCH_LC186X)
static struct clk *on2_enc_mclk = NULL;
static struct clk *ap_sw1_on2enc_clk = NULL;
static struct clk *ap_peri_sw1_on2enc_clk = NULL;
#endif
/*FIX EDEN_Req00000003 END DATE:2012-4-26 AUTHOR NAME FUHAIDONG */

static hx280enc_h1_t hx280enc_h1_data;

#if !defined(CONFIG_ARCH_LC181X)
/* CONFIG_ARCH_LC186X */
#define ENC_HW_ID1                  0x62800000
#define ENC_HW_ID2                  0x72800000
#define ENC_HW_ID3                  0x82700000
#define ENC_HW_ID4                  0x82900000
#define ENC_HW_ID5                  0x48310000

static unsigned long base_port = ENC_IO_BASE;
static int irq = ENC_IRQ;

/* module_param(name, type, perm) */
module_param(base_port, ulong, 0);
module_param(irq, int, 0);

static int ReserveIO(void);
static void ReleaseIO(void);
static void ResetAsic(hx280enc_h1_t * dev);

#ifdef HX280ENC_DEBUG
static void dump_regs(unsigned long data);
#endif

/* IRQ handler */
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
static irqreturn_t hx280enc_h1_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t hx280enc_h1_isr(int irq, void *dev_id);
#endif

/* VM operations */
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28))
static struct page *hx280enc_h1_vm_nopage(struct vm_area_struct *vma,
                                       unsigned long address, int *type)
{
    PDEBUG("hx280enc_h1_vm_nopage: problem with mem access\n");
    return NOPAGE_SIGBUS;   /* send a SIGBUS */
}
#else
static int hx280enc_h1_vm_fault(struct vm_area_struct *vma,
	                         struct vm_fault *vmf)
{
	PDEBUG("hx280enc_h1_vm_fault: problem with mem access\n");
	return VM_FAULT_SIGBUS;   /* send a SIGBUS */
}
#endif

static void hx280enc_h1_vm_open(struct vm_area_struct *vma)
{
	PDEBUG("hx280enc_h1_vm_open:\n");
}

static void hx280enc_h1_vm_close(struct vm_area_struct *vma)
{
	PDEBUG("hx280enc_h1_vm_close:\n");
}

static struct vm_operations_struct hx280enc_h1_vm_ops = {
  open:hx280enc_h1_vm_open,
  close:hx280enc_h1_vm_close,
#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28))
  nopage:hx280enc_h1_vm_nopage,
#else
  fault:hx280enc_h1_vm_fault,
#endif
};

/* the device's mmap method. The VFS has kindly prepared the process's
 * vm_area_struct for us, so we examine this to see what was requested.
 */
static int hx280enc_h1_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int result = -EINVAL;

	result = -EINVAL;

	vma->vm_ops = &hx280enc_h1_vm_ops;

	return result;
}

//remove"struct inode *inode, " for new kernel
//static int hx280enc_h1_ioctl(struct inode *inode, struct file *filp,
//	                      unsigned int cmd, unsigned long arg)
static long hx280enc_h1_ioctl(struct file *filp,
	                      unsigned int cmd, unsigned long arg)
{
	int err = 0;

	PDEBUG("ioctl cmd 0x%08ux\n", cmd);
	/*
	 * extract the type and number bitfields, and don't encode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if(_IOC_TYPE(cmd) != HX280ENC_H1_IOC_MAGIC)
	    return -ENOTTY;
	if(_IOC_NR(cmd) > HX280ENC_H1_IOC_MAXNR)
	    return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if(_IOC_DIR(cmd) & _IOC_READ)
	    err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	    err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
	if(err)
	    return -EFAULT;

	switch (cmd)
	{
	case HX280ENC_H1_IOCGHWOFFSET:
	    __put_user(hx280enc_h1_data.iobaseaddr, (unsigned long *) arg);
	    break;

	case HX280ENC_H1_IOCGHWIOSIZE:
	    __put_user(hx280enc_h1_data.iosize, (unsigned int *) arg);
	    break;
	}
	return 0;
}

static int hx280enc_h1_open(struct inode *inode, struct file *filp)
{
    int result = 0;
    hx280enc_h1_t *dev = &hx280enc_h1_data;

    filp->private_data = (void *) dev;

    PDEBUG("dev opened\n");
    return result;
}

static int hx280enc_h1_fasync(int fd, struct file *filp, int mode)
{
	hx280enc_h1_t *dev = (hx280enc_h1_t *) filp->private_data;

	PDEBUG("fasync called\n");

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int hx280enc_h1_release(struct inode *inode, struct file *filp)
{
    /* remove this filp from the asynchronusly notified filp's */
    hx280enc_h1_fasync(-1, filp, 0);

	PDEBUG("dev closed\n");
	return 0;
}

/* VFS methods */
static struct file_operations hx280enc_h1_fops = {
  mmap:hx280enc_h1_mmap,
  open:hx280enc_h1_open,
  release:hx280enc_h1_release,
  unlocked_ioctl:hx280enc_h1_ioctl,
  fasync:hx280enc_h1_fasync,
};

int __init hx280enc_h1_probe(struct platform_device *pdev)
{
	int result;
	unsigned long reg_val = 0;
	int timeout = 100;

	printk("hx280enc_h1_probe\n");

	hx280enc_h1_data.iobaseaddr = ENC_IO_BASE;
	hx280enc_h1_data.iosize = ENC_IO_SIZE;
	hx280enc_h1_data.irq = ENC_IRQ;
	hx280enc_h1_data.async_queue = NULL;
	hx280enc_h1_data.hwregs = NULL;

	on2_enc_mclk = clk_get(&pdev->dev, "on2_enc_mclk");
	if (IS_ERR(on2_enc_mclk)) {
	    printk(KERN_ERR "get on2_enc_mclk failed\n");
	    return -1;
	}
	clk_set_rate(on2_enc_mclk, HX280ENC_H1_CPU_FREQ_MIN);

	ap_sw1_on2enc_clk = clk_get(&pdev->dev, "ap_sw1_on2enc_clk");
	if (IS_ERR(ap_sw1_on2enc_clk)) {
	    printk(KERN_ERR "get ap_sw1_on2enc_clk failed\n");
	    return -1;
	}
	clk_enable(ap_sw1_on2enc_clk);
	ap_peri_sw1_on2enc_clk = clk_get(&pdev->dev, "ap_peri_sw1_on2enc_clk");
	if (IS_ERR(ap_peri_sw1_on2enc_clk)) {
	    printk(KERN_ERR "get ap_peri_sw1_on2enc_clk failed\n");
	    return -1;
	}
	clk_enable(ap_peri_sw1_on2enc_clk);

	/* power up */
	if((readl(io_p2v(AP_PWR_PDFSM_ST1)) & (0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)) != 0)
	{
		reg_val = readl(io_p2v(AP_PWR_ON2_PD_CTL));
		reg_val |= (1 << AP_PWR_WK_UP_WE) | (1 << AP_PWR_WK_UP);
		writel(reg_val ,io_p2v(AP_PWR_ON2_PD_CTL));
		while((readl(io_p2v(AP_PWR_PDFSM_ST1)) &
				(0x7 << AP_PWR_PDFSM_ST1_ON2_PD_ST)) != 0 && timeout-- > 0)
			udelay(10);
	}

	result = register_chrdev(hx280enc_h1_major, "hx280enc_h1", &hx280enc_h1_fops);
	if(result < 0) {
	    printk(KERN_ERR "hx280enc_h1: register_chrdev failed\n");
	    goto error_register_chrdev;
	}
	hx280enc_h1_major = result;

	result = ReserveIO();
	if(result < 0) {
	    goto error_reserve_io;
	}

	/* reset hardware */
	ResetAsic(&hx280enc_h1_data);

    /* get the IRQ line */
    if(irq != -1)
    {
        result = request_irq(irq, hx280enc_h1_isr,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
                             SA_INTERRUPT | SA_SHIRQ,
#else
                             IRQF_DISABLED | IRQF_SHARED,
#endif
                             "hx280enc_h1", (void *) &hx280enc_h1_data);
        if(result == -EINVAL)
        {
            printk(KERN_ERR "hx280enc_h1: Bad irq number or handler\n");
	        goto error_request_irq;
        }
        else if(result == -EBUSY)
        {
            printk(KERN_ERR "hx280enc_h1: IRQ <%d> busy, change your config\n",
                   hx280enc_h1_data.irq);
	        goto error_request_irq;
        }
    }
    else
    {
        printk(KERN_INFO "hx280enc_h1: IRQ not in use!\n");
    }

	hx280enc_h1_class = class_create(THIS_MODULE, "hx280enc_h1");
	if (IS_ERR(hx280enc_h1_class)) {
	    printk(KERN_ERR "hx280enc_h1: class_create failed\n");
	    goto error_class_create;
	}
	device_create(hx280enc_h1_class, NULL, MKDEV(hx280enc_h1_major, 0), NULL, "hx280enc_h1");

	result = ion_iommu_attach_dev(&pdev->dev);
	if(result < 0)
	{
		printk(KERN_ERR "hx280enc_h1: iommu_attach_device failed\n");
		goto error_iommu_attach_device;
	}

	return 0;

error_iommu_attach_device:
	device_destroy(hx280enc_h1_class, MKDEV(hx280enc_h1_major, 0));
	class_destroy(hx280enc_h1_class);
error_class_create:
	free_irq(hx280enc_h1_data.irq, (void *) &hx280enc_h1_data);
error_request_irq:
	ReleaseIO();
error_reserve_io:
	unregister_chrdev(hx280enc_h1_major, "hx280enc_h1");
error_register_chrdev:
	printk(KERN_INFO "hx280enc_h1: probe failed\n");
	return result;
}

void __exit hx280enc_h1_remove(struct platform_device *pdev)
{
	/* iommu detach device */
	ion_iommu_detach_dev(&pdev->dev);

	writel(0, hx280enc_h1_data.hwregs + 0x38);  /* disable HW */
	writel(0, hx280enc_h1_data.hwregs + 0x04); /* clear enc IRQ */

	device_destroy(hx280enc_h1_class, MKDEV(hx280enc_h1_major, 0));
	class_destroy(hx280enc_h1_class);

    /* free the encoder IRQ */
    if(hx280enc_h1_data.irq != -1)
    {
        free_irq(hx280enc_h1_data.irq, (void *) &hx280enc_h1_data);
    }

	ReleaseIO();

	unregister_chrdev(hx280enc_h1_major, "hx280enc_h1");

	return;
}

static struct platform_device hx280enc_h1_device = {
	.name = "hx280enc_h1",
	.id = -1,
#if defined(CONFIG_COMIP_IOMMU)
	.archdata = {
		.dev_id	= 1,	/*SMMU0 :1, SMMU1: 2*/
		.s_id	= 2,	/*Stream ID*/
	},
#endif
};

static struct platform_driver hx280enc_h1_driver = {
	.driver = {
	    .name = "hx280enc_h1",
	    .owner = THIS_MODULE,
	},
	.remove = __exit_p(hx280enc_h1_remove),
};

static int __init hx280enc_h1_init(void)
{
	platform_device_register(&hx280enc_h1_device);
	return platform_driver_probe(&hx280enc_h1_driver, hx280enc_h1_probe);
}

static void __exit hx280enc_h1_exit(void)
{
	platform_driver_unregister(&hx280enc_h1_driver);
	platform_device_unregister(&hx280enc_h1_device);
}

module_init(hx280enc_h1_init);
module_exit(hx280enc_h1_exit);

/*------------------------------------------------------------------------------
	Function name   : ReserveIO
	Description     : IO reserve

	Return type     : int
------------------------------------------------------------------------------*/
static int ReserveIO(void)
{
    long int hwid;

	if(!request_mem_region
	   (hx280enc_h1_data.iobaseaddr, hx280enc_h1_data.iosize, "hx280enc_h1"))
	{
	    printk(KERN_INFO "hx280enc_h1: failed to reserve HW regs\n");
	    return -EBUSY;
	}

	hx280enc_h1_data.hwregs =
	    (volatile u8 *) ioremap_nocache(hx280enc_h1_data.iobaseaddr,
	                                    hx280enc_h1_data.iosize);

	printk(KERN_INFO "hx280enc_h1_data.hwregs = %p\n",&hx280enc_h1_data.hwregs[0]);

	if(hx280enc_h1_data.hwregs == NULL)
	{
	    printk(KERN_INFO "hx280enc_h1: failed to ioremap HW regs\n");
	    ReleaseIO();
	    return -EBUSY;
	}

    hwid = readl(hx280enc_h1_data.hwregs);

    /* check for encoder HW ID */
    if((((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID1 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID2 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID3 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID4 >> 16) & 0xFFFF)) &&
       (((hwid >> 16) & 0xFFFF) != ((ENC_HW_ID5 >> 16) & 0xFFFF)))
    {
        printk(KERN_INFO "hx280enc_h1: HW not found at 0x%08lx\n",
               hx280enc_h1_data.iobaseaddr);
#ifdef HX280ENC_DEBUG
        dump_regs((unsigned long) &hx280enc_h1_data);
#endif
        ReleaseIO();
        return -EBUSY;
    }
    printk(KERN_INFO
           "hx280enc_h1: HW at base <0x%08lx> with ID <0x%08lx>\n",
           hx280enc_h1_data.iobaseaddr, hwid);

	return 0;
}

static void ReleaseIO(void)
{
	if(hx280enc_h1_data.hwregs)
	    iounmap((void *) hx280enc_h1_data.hwregs);
	release_mem_region(hx280enc_h1_data.iobaseaddr, hx280enc_h1_data.iosize);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
irqreturn_t hx280enc_h1_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
irqreturn_t hx280enc_h1_isr(int irq, void *dev_id)
#endif
{
    hx280enc_h1_t *dev = (hx280enc_h1_t *) dev_id;
    u32 irq_status;

    irq_status = readl(dev->hwregs + 0x04);

    if(irq_status & 0x01)
    {

        /* clear enc IRQ and slice ready interrupt bit */
        writel(irq_status & (~0x101), dev->hwregs + 0x04);

        /* Handle slice ready interrupts. The reference implementation
         * doesn't signal slice ready interrupts to EWL.
         * The EWL will poll the slices ready register value. */
        if ((irq_status & 0x1FE) == 0x100)
        {
            PDEBUG("Slice ready IRQ handled!\n");
            return IRQ_HANDLED;
        }

        /* All other interrupts will be signaled to EWL. */
        if(dev->async_queue)
            kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

        PDEBUG("IRQ handled!\n");
        return IRQ_HANDLED;
    }
    else
    {
        PDEBUG("IRQ received, but NOT handled!\n");
        return IRQ_NONE;
    }
}

/*------------------------------------------------------------------------------
	Function name   : ResetAsic
	Description     : reset asic

	Return type     :
------------------------------------------------------------------------------*/
static void ResetAsic(hx280enc_h1_t * dev)
{
	int i;

	writel(0, dev->hwregs + 0x38);

	for(i = 4; i < dev->iosize; i += 4)
	{
	    writel(0, dev->hwregs + i);
	}
}

/*------------------------------------------------------------------------------
	Function name   : dump_regs
	Description     : Dump registers

	Return type     :
------------------------------------------------------------------------------*/
#if 0
static void dump_regs(unsigned long data)
{
	hx280enc_h1_t *dev = (hx280enc_h1_t *) data;
	int i;

    PDEBUG("Reg Dump Start\n");
    for(i = 0; i < dev->iosize; i += 4)
    {
        PDEBUG("\toffset %02X = %08X\n", i, readl(dev->hwregs + i));
    }
    PDEBUG("Reg Dump End\n");
}
#endif

#else
/* CONFIG_ARCH_LC181X */
static const int EncHwId[] = { 0x6280, 0x7280, 0x8270, 0x8290, 0x4831 };

static int CheckHwId(hx280enc_h1_t * dev)
{
	long int hwid;

	size_t numHw = sizeof(EncHwId) / sizeof(*EncHwId);

	hwid = readl(dev->hwregs);
	printk(KERN_INFO "hx280enc_h1: HW ID=0x%08lx\n", hwid);

	hwid = (hwid >> 16) & 0xFFFF;   /* product version only */

	while(numHw--)
	{
	    if(hwid == EncHwId[numHw])
	    {
	        printk(KERN_INFO "hx280enc_h1: Compatible HW found at 0x%08lx\n",
	               dev->iobaseaddr);
	        return 1;
	    }
	}

	printk(KERN_INFO "hx280enc_h1: No Compatible HW found at 0x%08lx\n",
	       dev->iobaseaddr);
	return 0;
}

static void ReleaseIO(void)
{
	if(hx280enc_h1_data.hwregs)
	    iounmap((void *) hx280enc_h1_data.hwregs);
	release_mem_region(hx280enc_h1_data.iobaseaddr, hx280enc_h1_data.iosize);
}

/*------------------------------------------------------------------------------
	Function name   : ReserveIO
	Description     : IO reserve

	Return type     : int
------------------------------------------------------------------------------*/
static int ReserveIO(void)
{
	if(!request_mem_region
	   (hx280enc_h1_data.iobaseaddr, hx280enc_h1_data.iosize, "hx280enc_h1"))
	{
	    printk(KERN_INFO "hx280enc_h1: failed to reserve HW regs\n");
	    return -EBUSY;
	}

	hx280enc_h1_data.hwregs =
	    (volatile u8 *) ioremap_nocache(hx280enc_h1_data.iobaseaddr,
	                                    hx280enc_h1_data.iosize);

	printk(KERN_INFO "hx280enc_h1_data.hwregs = %p\n",&hx280enc_h1_data.hwregs[0]);

	if(hx280enc_h1_data.hwregs == NULL)
	{
	    printk(KERN_INFO "hx280enc_h1: failed to ioremap HW regs\n");
	    ReleaseIO();
	    return -EBUSY;
	}

	/* check for correct HW */
	if(!CheckHwId(&hx280enc_h1_data)) {
	    ReleaseIO();
	    return -EBUSY;
	}

	return 0;
}

irqreturn_t hx280enc_h1_isr(int irq, void *dev_id)
{
	unsigned int handled = 0;
	hx280enc_h1_t *dev = (hx280enc_h1_t *) dev_id;
	u32 irq_status;

	irq_status = readl(dev->hwregs + 0x04);

	if(irq_status & 0x01)
	{

	    /* FIX EDEN_Req00000003 BEGIN DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
	    /* clear enc IRQ and slice ready interrupt bit */
	    writel(irq_status & (~0x101), dev->hwregs + 0x04);

	    /* Handle slice ready interrupts. The reference implementation
	    * doesn't signal slice ready interrupts to EWL.
	    * The EWL will poll the slices ready register value. */
	    if ((irq_status & 0x1FE) == 0x100)
	    {
	        PDEBUG("Slice ready IRQ handled!\n");
	        return IRQ_HANDLED;
	    }
	    /* FIX EDEN_Req00000003 END DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
/* FIX L1810_Bug00000393 BEGIN DATE:2012-6-19 AUTHOR NAME ZHANGZIMING */
	    /* All other interrupts will be signaled to EWL. */
	    if(dev->async_queue)
	        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
/* FIX L1810_Bug00000393 END DATE:2012-6-19 AUTHOR NAME ZHANGZIMING */
	    PDEBUG("hx280enc_h1 IRQ handled!\n");
	    handled = 1;
	}

	return IRQ_RETVAL(handled);
}

/*------------------------------------------------------------------------------
	Function name   : ResetAsic
	Description     : reset asic

	Return type     :
------------------------------------------------------------------------------*/
static void ResetAsic(hx280enc_h1_t * dev)
{
	int i;

	writel(0, dev->hwregs + 0x38);

	for(i = 4; i < dev->iosize; i += 4)
	{
	    writel(0, dev->hwregs + i);
	}
}

/*------------------------------------------------------------------------------
	Function name   : dump_regs
	Description     : Dump registers

	Return type     :
------------------------------------------------------------------------------*/
#if 0
static void dump_regs(unsigned long data)
{
	hx280enc_h1_t *dev = (hx280enc_h1_t *) data;
	int i;

	printk("Reg Dump Start\n");
	for(i = 0; i < dev->iosize; i += 4)
	{
	    printk("\toffset %02X = %08X\n", i, readl(dev->hwregs + i));
	}
	printk("Reg Dump End\n");
}
#endif

/* VM operations */
static int hx280enc_h1_vm_fault(struct vm_area_struct *vma,
	                         struct vm_fault *vmf)
{
	PDEBUG("hx280enc_h1_vm_fault: problem with mem access\n");
	return VM_FAULT_SIGBUS;   /* send a SIGBUS */
}

static void hx280enc_h1_vm_open(struct vm_area_struct *vma)
{
	PDEBUG("hx280enc_h1_vm_open:\n");
}

static void hx280enc_h1_vm_close(struct vm_area_struct *vma)
{
	PDEBUG("hx280enc_h1_vm_close:\n");
}

static struct vm_operations_struct hx280enc_h1_vm_ops = {
  open:hx280enc_h1_vm_open,
  close:hx280enc_h1_vm_close,
  fault:hx280enc_h1_vm_fault,
};

/* the device's mmap method. The VFS has kindly prepared the process's
 * vm_area_struct for us, so we examine this to see what was requested.
 */
static int hx280enc_h1_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int result = -EINVAL;

	result = -EINVAL;

	vma->vm_ops = &hx280enc_h1_vm_ops;

	return result;
}

//remove"struct inode *inode, " for new kernel
//static int hx280enc_h1_ioctl(struct inode *inode, struct file *filp,
//	                      unsigned int cmd, unsigned long arg)
static long hx280enc_h1_ioctl(struct file *filp,
	                      unsigned int cmd, unsigned long arg)
{
	int err = 0;

	PDEBUG("ioctl cmd 0x%08ux\n", cmd);
	/*
	 * extract the type and number bitfields, and don't encode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if(_IOC_TYPE(cmd) != HX280ENC_H1_IOC_MAGIC)
	    return -ENOTTY;
	if(_IOC_NR(cmd) > HX280ENC_H1_IOC_MAXNR)
	    return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if(_IOC_DIR(cmd) & _IOC_READ)
	    err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	    err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
	if(err)
	    return -EFAULT;

	switch (cmd)
	{
	case HX280ENC_H1_IOCGHWOFFSET:
	    __put_user(hx280enc_h1_data.iobaseaddr, (unsigned long *) arg);
	    break;

	case HX280ENC_H1_IOCGHWIOSIZE:
	    __put_user(hx280enc_h1_data.iosize, (unsigned int *) arg);
	    break;
	}
	return 0;
}

static int hx280enc_h1_open(struct inode *inode, struct file *filp)
{
/* FIX L1810_Bug00000243 BEGIN DATE:2012-6-14 AUTHOR NAME FUHAIDONG */
#if 0
	/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
	on2_encode_clk = clk_get(hx280enc_h1_data.dev, "on2_encode_clk");
	if (IS_ERR(on2_encode_clk)) {
	    printk(KERN_ERR "get on2_encode_clk failed\n");
	    return -1;
	}
	clk_set_rate(on2_encode_clk,208000000);
	on2_ebus_clk = clk_get(hx280enc_h1_data.dev, "on2_ebus_clk");
	if (IS_ERR(on2_ebus_clk)) {
	    printk(KERN_ERR "get on2_ebus_clk failed\n");
	    return -1;
	}
	clk_enable(on2_ebus_clk);
	on2_e1_clk = clk_get(hx280enc_h1_data.dev, "on2_e1_clk");
	if (IS_ERR(on2_e1_clk)) {
	    printk(KERN_ERR "get on2_e1_clk failed\n");
	    return -1;
	}
	clk_enable(on2_e1_clk);
	/*FIX EDEN_Req00000003 END DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
#endif
/* FIX L1810_Bug00000243 END DATE:2012-6-14 AUTHOR NAME FUHAIDONG */

	filp->private_data = (void *) &hx280enc_h1_data;

	PDEBUG("dev opened\n");
	return 0;
}

static int hx280enc_h1_fasync(int fd, struct file *filp, int mode)
{
	hx280enc_h1_t *dev = (hx280enc_h1_t *) filp->private_data;

	PDEBUG("fasync called\n");

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int hx280enc_h1_release(struct inode *inode, struct file *filp)
{
	if(filp->f_flags & FASYNC) {
	    /* remove this filp from the asynchronusly notified filp's */
	    hx280enc_h1_fasync(-1, filp, 0);
	}
/* FIX L1810_Bug00000243 BEGIN DATE:2012-6-14 AUTHOR NAME FUHAIDONG */
#if 0
	/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
	clk_put(on2_encode_clk);
	clk_disable(on2_ebus_clk);
	clk_put(on2_ebus_clk);
	clk_disable(on2_e1_clk);
	clk_put(on2_e1_clk);
	/*FIX EDEN_Req00000003 END DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
#endif
/* FIX L1810_Bug00000243 END DATE:2012-6-14 AUTHOR NAME FUHAIDONG */

	PDEBUG("dev closed\n");
	return 0;
}

/* VFS methods */
static struct file_operations hx280enc_h1_fops = {
  mmap:hx280enc_h1_mmap,
  open:hx280enc_h1_open,
  release:hx280enc_h1_release,
  unlocked_ioctl:hx280enc_h1_ioctl,
  fasync:hx280enc_h1_fasync,
};

int __init hx280enc_h1_probe(struct platform_device *pdev)
{
	int result;
	printk("hx280enc_h1_probe\n");

	hx280enc_h1_data.iobaseaddr = ENC_IO_BASE;
	hx280enc_h1_data.iosize = ENC_IO_SIZE;
	hx280enc_h1_data.irq = ENC_IRQ;
	hx280enc_h1_data.async_queue = NULL;
	hx280enc_h1_data.hwregs = NULL;
	hx280enc_h1_data.dev = &pdev->dev;

	/* get the IRQ line */
	result = request_irq(hx280enc_h1_data.irq, hx280enc_h1_isr,
	                     IRQF_DISABLED | IRQF_SHARED,
	                     "hx280enc_h1", (void *) &hx280enc_h1_data);
	if(result != 0) {
	    printk(KERN_ERR "hx280enc_h1: request interrupt failed\n");
	    goto error_request_irq;
	}

	/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
	on2_encode_clk = clk_get(hx280enc_h1_data.dev, "on2_encode_clk");
	if (IS_ERR(on2_encode_clk)) {
	    printk(KERN_ERR "get on2_encode_clk failed\n");
	    return -1;
	}
	clk_set_rate(on2_encode_clk, HX280ENC_H1_CPU_FREQ_MIN);

	on2_ebus_clk = clk_get(hx280enc_h1_data.dev, "on2_ebus_clk");
	if (IS_ERR(on2_ebus_clk)) {
	    printk(KERN_ERR "get on2_ebus_clk failed\n");
	    return -1;
	}
	clk_enable(on2_ebus_clk);
	on2_e1_clk = clk_get(hx280enc_h1_data.dev, "on2_e1_clk");
	if (IS_ERR(on2_e1_clk)) {
	    printk(KERN_ERR "get on2_e1_clk failed\n");
	    return -1;
	}
	clk_enable(on2_e1_clk);

	result = ReserveIO();
	if(result < 0) {
	    goto error_reserve_io;
	}

	/* reset hardware */
	ResetAsic(&hx280enc_h1_data);

	/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-26 AUTHOR NAME FUHAIDONG */
	//clk_disable(on2_encode_clk);
	//clk_put(on2_encode_clk);
	//clk_disable(on2_ebus_clk);
	//clk_put(on2_ebus_clk);
	//clk_disable(on2_e1_clk);
	//clk_put(on2_e1_clk);
	/*FIX EDEN_Req00000003 END DATE:2012-4-26 AUTHOR NAME FUHAIDONG */

	result = register_chrdev(hx280enc_h1_major, "hx280enc_h1", &hx280enc_h1_fops);
	if(result < 0) {
	    printk(KERN_ERR "hx280enc_h1: register_chrdev failed\n");
	    goto error_register_chrdev;
	}
	hx280enc_h1_major = result;

	hx280enc_h1_class = class_create(THIS_MODULE, "hx280enc_h1");
	if (IS_ERR(hx280enc_h1_class)) {
	    printk(KERN_ERR "hx280enc_h1: class_create failed\n");
	    goto error_class_create;
	}
	device_create(hx280enc_h1_class, NULL, MKDEV(hx280enc_h1_major, 0), NULL, "hx280enc_h1");

	result = ion_iommu_attach_dev(hx280enc_h1_data.dev);
	if(result < 0)
	{
		printk(KERN_ERR "hx280enc_h1: iommu_attach_device failed\n");
		goto error_iommu_attach_device;
	}

	return 0;

 error_iommu_attach_device:
	device_destroy(hx280enc_h1_class, MKDEV(hx280enc_h1_major, 0));
	class_destroy(hx280enc_h1_class);
error_class_create:
	unregister_chrdev(hx280enc_h1_major, "hx280enc_h1");
error_register_chrdev:
	ReleaseIO();
error_reserve_io:
	free_irq(hx280enc_h1_data.irq, (void *) &hx280enc_h1_data);
error_request_irq:
	printk(KERN_INFO "hx280enc_h1: probe failed\n");
	return result;
}

void __exit hx280enc_h1_remove(struct platform_device *pdev)
{
	hx280enc_h1_t *dev = (hx280enc_h1_t *) &hx280enc_h1_data;

	/* iommu detach device */
	ion_iommu_detach_dev(hx280enc_h1_data.dev);

	writel(0, hx280enc_h1_data.hwregs + 0x38);  /* disable HW */
	writel(0, hx280enc_h1_data.hwregs + 0x04); /* clear enc IRQ */

	device_destroy(hx280enc_h1_class, MKDEV(hx280enc_h1_major, 0));
	class_destroy(hx280enc_h1_class);
	unregister_chrdev(hx280enc_h1_major, "hx280enc_h1");

	ReleaseIO();

	free_irq(dev->irq, (void *) dev);

	return;
}

static struct platform_device hx280enc_h1_device = {
	.name = "hx280enc_h1",
	.id = -1,
#if defined(CONFIG_COMIP_IOMMU)
	.archdata = {
		.dev_id	= 1,	/*SMMU0 :1, SMMU1: 2*/
		.s_id	= 2,	/*Stream ID*/
	},
#endif
};

static struct platform_driver hx280enc_h1_driver = {
	.driver = {
	    .name = "hx280enc_h1",
	    .owner = THIS_MODULE,
	},
	.remove = __exit_p(hx280enc_h1_remove),
};

static int __init hx280enc_h1_init(void)
{
	platform_device_register(&hx280enc_h1_device);
	return platform_driver_probe(&hx280enc_h1_driver, hx280enc_h1_probe);
}

static void __exit hx280enc_h1_exit(void)
{
	platform_driver_unregister(&hx280enc_h1_driver);
	platform_device_unregister(&hx280enc_h1_device);
}

module_init(hx280enc_h1_init);
module_exit(hx280enc_h1_exit);
#endif

MODULE_AUTHOR("Kai Liu <kai.liu@windriver.com>");
MODULE_DESCRIPTION("Hantro 6280/7280/8270 Encoder driver");
MODULE_LICENSE("GPL");
