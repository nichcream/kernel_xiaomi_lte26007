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
--  Abstract : 6280/7280/8270 Encoder device driver (kernel module)
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: hx280enc.c,v $
--  $Date: 2009/03/31 10:15:57 $
--  $Revision: 1.6 $
--
------------------------------------------------------------------------------*/
 /*******************************************************************************
* CR History:
********************************************************************************
*  1.0.0     EDEN_Req00000001       yangshudan     2012-2-26    add ON2 Codec  function *
*  1.0.1     EDEN_Req00000003       linhuichun     2012-4-24    modify hardware
*                                                               resource for 1810
*  1.0.2     L1810_Bug00000243       fuhaidong     2012-6-14    add on2 psm, here to abandon clk operation
*  1.0.3     L1810_Bug00000393    zhangziming    2012-6-19   modify 7280 and h1 isr mode to polling for vt*
*********************************************************************************/

#include <plat/hx280enc.h>
#include <asm/signal.h>
#include <asm-generic/siginfo.h>
#include <mach/irqs.h>
#include <mach/comip-regs.h>
#include <plat/comip-smmu.h>

#if defined(CONFIG_ARCH_LC186X)
#include <linux/delay.h>
#include <plat/hardware.h>
#endif

/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
#if 0
#define ENC_IO_BASE                 0x32150000
#define ENC_IO_SIZE                 (96 * 4)    /* bytes */
#define ENC_IRQ                     8
#else
#if defined(CONFIG_ARCH_LC181X)
#define ENC_IO_BASE                 ENCODER0_BASE
#define ENC_IO_SIZE                 (64 * 4)    /* bytes */
#define ENC_IRQ                     INT_ON2_ENCODER0
#elif defined(CONFIG_ARCH_LC186X)
#define ENC_IO_BASE                 ON2_CODER_BASE
#define ENC_IO_SIZE                 (96 * 4)    /* bytes */
#define ENC_IRQ                     INT_ON2_CODEC
#endif
#endif
/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-23 AUTHOR NAME LINHUICHUN */

static const int EncHwId[] = { 0x6280, 0x7280, 0x8270 };

/* and this is our MAJOR; use 0 for dynamic allocation (recommended)*/
static int hx280enc_major = 0;

static struct class *hx280enc_class;

/*FIX EDEN_Req00000003 BEGIN DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
#if 0
static struct clk *video_acc_clk = NULL;
#else
#if defined(CONFIG_ARCH_LC181X)
static struct clk *on2_encode_clk = NULL;
static struct clk *on2_e0_clk = NULL;
static struct clk *on2_ebus_clk = NULL;
#elif defined(CONFIG_ARCH_LC186X)
static struct clk *on2_codec_mclk = NULL;
static struct clk *ap_sw1_on2codec_clk = NULL;
static struct clk *ap_peri_sw1_on2codec_clk = NULL;
#endif
#endif
/*FIX EDEN_Req00000003 END DATE:2012-4-23 AUTHOR NAME LINHUICHUN */

static hx280enc_t hx280enc_data;

static int CheckHwId(hx280enc_t * dev)
{
	long int hwid;

	size_t numHw = sizeof(EncHwId) / sizeof(*EncHwId);

	hwid = readl(dev->hwregs);
	printk(KERN_INFO "hx280enc: HW ID=0x%08lx\n", hwid);

	hwid = (hwid >> 16) & 0xFFFF;   /* product version only */

	while(numHw--)
	{
	    if(hwid == EncHwId[numHw])
	    {
	        printk(KERN_INFO "hx280enc: Compatible HW found at 0x%08lx\n",
	               dev->iobaseaddr);
	        return 1;
	    }
	}

	printk(KERN_INFO "hx280enc: No Compatible HW found at 0x%08lx\n",
	       dev->iobaseaddr);
	return 0;
}

static void ReleaseIO(void)
{
	if(hx280enc_data.hwregs)
	    iounmap((void *) hx280enc_data.hwregs);
	release_mem_region(hx280enc_data.iobaseaddr, hx280enc_data.iosize);
}

/*------------------------------------------------------------------------------
	Function name   : ReserveIO
	Description     : IO reserve

	Return type     : int
------------------------------------------------------------------------------*/
static int ReserveIO(void)
{
	if(!request_mem_region
	   (hx280enc_data.iobaseaddr, hx280enc_data.iosize, "hx280enc"))
	{
	    printk(KERN_INFO "hx280enc: failed to reserve HW regs\n");
	    return -EBUSY;
	}

	hx280enc_data.hwregs =
	    (volatile u8 *) ioremap_nocache(hx280enc_data.iobaseaddr,
	                                    hx280enc_data.iosize);

	if(hx280enc_data.hwregs == NULL)
	{
	    printk(KERN_INFO "hx280enc: failed to ioremap HW regs\n");
	    ReleaseIO();
	    return -EBUSY;
	}

	/* check for correct HW */
	if(!CheckHwId(&hx280enc_data)) {
	    ReleaseIO();
	    return -EBUSY;
	}

	return 0;
}

irqreturn_t hx280enc_isr(int irq, void *dev_id)
{
	unsigned int handled = 0;
	hx280enc_t *dev = (hx280enc_t *) dev_id;
	u32 irq_status;

	irq_status = readl(dev->hwregs + 0x04);

	if(irq_status & 0x01)
	{
	    writel(irq_status & (~0x01), dev->hwregs + 0x04);   /* clear enc IRQ */
/* FIX L1810_Bug00000393 BEGIN DATE:2012-6-19 AUTHOR NAME ZHANGZIMING */
	    if(dev->async_queue)
	        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
/* FIX L1810_Bug00000393 END DATE:2012-6-19 AUTHOR NAME ZHANGZIMING */
	    PDEBUG("hx280enc IRQ handled!\n");
	    handled = 1;
	}

	return IRQ_RETVAL(handled);
}

/*------------------------------------------------------------------------------
	Function name   : ResetAsic
	Description     : reset asic

	Return type     :
------------------------------------------------------------------------------*/
static void ResetAsic(hx280enc_t * dev)
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
	hx280enc_t *dev = (hx280enc_t *) data;
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
static int hx280enc_vm_fault(struct vm_area_struct *vma,
	                         struct vm_fault *vmf)
{
	PDEBUG("hx280enc_vm_fault: problem with mem access\n");
	return VM_FAULT_SIGBUS;   /* send a SIGBUS */
}

static void hx280enc_vm_open(struct vm_area_struct *vma)
{
	PDEBUG("hx280enc_vm_open:\n");
}

static void hx280enc_vm_close(struct vm_area_struct *vma)
{
	PDEBUG("hx280enc_vm_close:\n");
}

static struct vm_operations_struct hx280enc_vm_ops = {
  open:hx280enc_vm_open,
  close:hx280enc_vm_close,
  fault:hx280enc_vm_fault,
};

/* the device's mmap method. The VFS has kindly prepared the process's
 * vm_area_struct for us, so we examine this to see what was requested.
 */
static int hx280enc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int result = -EINVAL;

	result = -EINVAL;

	vma->vm_ops = &hx280enc_vm_ops;

	return result;
}

/* FIX EDEN_Req00000001 BEGIN DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
//remove"struct inode *inode, " for new kernel
//static int hx280enc_ioctl(struct inode *inode, struct file *filp,
//	                      unsigned int cmd, unsigned long arg)
static long hx280enc_ioctl(struct file *filp,
	                      unsigned int cmd, unsigned long arg)
/* FIX EDEN_Req00000001 END DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
{
	int err = 0;

	PDEBUG("ioctl cmd 0x%08ux\n", cmd);
	/*
	 * extract the type and number bitfields, and don't encode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if(_IOC_TYPE(cmd) != HX280ENC_IOC_MAGIC)
	    return -ENOTTY;
	if(_IOC_NR(cmd) > HX280ENC_IOC_MAXNR)
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
	case HX280ENC_IOCGHWOFFSET:
	    __put_user(hx280enc_data.iobaseaddr, (unsigned long *) arg);
	    break;

	case HX280ENC_IOCGHWIOSIZE:
	    __put_user(hx280enc_data.iosize, (unsigned int *) arg);
	    break;
	}
	return 0;
}

static int hx280enc_open(struct inode *inode, struct file *filp)
{
/* FIX L1810_Bug00000243 BEGIN DATE:2012-6-14 AUTHOR NAME FUHAIDONG */
#if 0
  /*FIX EDEN_Req00000003 BEGIN DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
#if 0
	video_acc_clk = clk_get(hx280enc_data.dev, "video_acc_clk");
	if (IS_ERR(video_acc_clk)) {
	    printk(KERN_ERR "get video_acc_clk failed\n");
	    return -1;
	}
	clk_enable(video_acc_clk);
#else
	on2_encode_clk = clk_get(hx280enc_data.dev, "on2_encode_clk");
	if (IS_ERR(on2_encode_clk)) {
	    printk(KERN_ERR "get on2_encode_clk failed\n");
	    return -1;
	}
	clk_set_rate(on2_encode_clk,208000000);
	on2_ebus_clk = clk_get(hx280enc_data.dev, "on2_ebus_clk");
	if (IS_ERR(on2_ebus_clk)) {
	    printk(KERN_ERR "get on2_ebus_clk failed\n");
	    return -1;
	}
	clk_enable(on2_ebus_clk);
	on2_e0_clk = clk_get(hx280enc_data.dev, "on2_e0_clk");
	if (IS_ERR(on2_e0_clk)) {
	    printk(KERN_ERR "get on2_e0_clk failed\n");
	    return -1;
	}
	clk_enable(on2_e0_clk);
#endif
	/*FIX EDEN_Req00000003 END DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
#endif
/* FIX L1810_Bug00000243 END DATE:2012-6-14 AUTHOR NAME FUHAIDONG */

	filp->private_data = (void *) &hx280enc_data;

	PDEBUG("dev opened\n");
	return 0;
}

static int hx280enc_fasync(int fd, struct file *filp, int mode)
{
	hx280enc_t *dev = (hx280enc_t *) filp->private_data;

	PDEBUG("fasync called\n");

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int hx280enc_release(struct inode *inode, struct file *filp)
{
	if(filp->f_flags & FASYNC) {
	    /* remove this filp from the asynchronusly notified filp's */
	    hx280enc_fasync(-1, filp, 0);
	}
/* FIX L1810_Bug00000243 BEGIN DATE:2012-6-14 AUTHOR NAME FUHAIDONG */
#if 0
  /*FIX EDEN_Req00000003 BEGIN DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
#if 0
	clk_disable(video_acc_clk);
	clk_put(video_acc_clk);
#else
    clk_put(on2_encode_clk);
	clk_disable(on2_ebus_clk);
	clk_put(on2_ebus_clk);
	clk_disable(on2_e0_clk);
	clk_put(on2_e0_clk);
#endif
  /*FIX EDEN_Req00000003 END DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
#endif
/* FIX L1810_Bug00000243 END DATE:2012-6-14 AUTHOR NAME FUHAIDONG */

	PDEBUG("dev closed\n");
	return 0;
}

/* VFS methods */
static struct file_operations hx280enc_fops = {
  mmap:hx280enc_mmap,
  open:hx280enc_open,
  release:hx280enc_release,
/* FIX EDEN_Req00000001 BEGIN DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
  unlocked_ioctl:hx280enc_ioctl,
  //ioctl:hx280enc_ioctl,
/* FIX EDEN_Req00000001 END DATE:2012-2-26 AUTHOR NAME YANGSHUDAN */
  fasync:hx280enc_fasync,
};

int __init hx280enc_probe(struct platform_device *pdev)
{
	int result;
#if defined(CONFIG_ARCH_LC186X)
	unsigned long reg_val = 0;
	int timeout = 100;
#endif

	printk("hx280enc_probe\n");

	hx280enc_data.iobaseaddr = ENC_IO_BASE;
	hx280enc_data.iosize = ENC_IO_SIZE;
	hx280enc_data.irq = ENC_IRQ;
	hx280enc_data.async_queue = NULL;
	hx280enc_data.hwregs = NULL;
	hx280enc_data.dev = &pdev->dev;

	/* get the IRQ line */
	result = request_irq(hx280enc_data.irq, hx280enc_isr,
	                     IRQF_DISABLED | IRQF_SHARED,
	                     "hx280enc", (void *) &hx280enc_data);
	if(result != 0) {
	    printk(KERN_ERR "hx280enc: request interrupt failed\n");
	    goto error_request_irq;
	}

#if defined(CONFIG_ARCH_LC181X)
    /*FIX EDEN_Req00000003 BEGIN DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
	on2_encode_clk = clk_get(hx280enc_data.dev, "on2_encode_clk");
	if (IS_ERR(on2_encode_clk)) {
	    printk(KERN_ERR "get on2_encode_clk failed\n");
	    return -1;
	}
	clk_set_rate(on2_encode_clk, HX280ENC_CPU_FREQ_MIN);
	on2_ebus_clk = clk_get(hx280enc_data.dev, "on2_ebus_clk");
	if (IS_ERR(on2_ebus_clk)) {
	    printk(KERN_ERR "get on2_ebus_clk failed\n");
	    return -1;
	}
	clk_enable(on2_ebus_clk);
	on2_e0_clk = clk_get(hx280enc_data.dev, "on2_e0_clk");
	if (IS_ERR(on2_e0_clk)) {
	    printk(KERN_ERR "get on2_e0_clk failed\n");
	    return -1;
	}
	clk_enable(on2_e0_clk);
	/*FIX EDEN_Req00000003 END DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
#elif defined(CONFIG_ARCH_LC186X)
	on2_codec_mclk = clk_get(hx280enc_data.dev, "on2_codec_mclk");
	if (IS_ERR(on2_codec_mclk)) {
	    printk(KERN_ERR "get on2_codec_mclk failed\n");
	    return -1;
	}
	clk_set_rate(on2_codec_mclk, HX280ENC_CPU_FREQ_MIN);
	ap_sw1_on2codec_clk = clk_get(hx280enc_data.dev, "ap_sw1_on2codec_clk");
	if (IS_ERR(ap_sw1_on2codec_clk)) {
	    printk(KERN_ERR "get ap_sw1_on2codec_clk failed\n");
	    return -1;
	}
	clk_enable(ap_sw1_on2codec_clk);
	ap_peri_sw1_on2codec_clk = clk_get(hx280enc_data.dev, "ap_peri_sw1_on2codec_clk");
	if (IS_ERR(ap_peri_sw1_on2codec_clk)) {
	    printk(KERN_ERR "get ap_peri_sw1_on2codec_clk failed\n");
	    return -1;
	}
	clk_enable(ap_peri_sw1_on2codec_clk);

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
#endif

	result = ReserveIO();
	if(result < 0) {
	    goto error_reserve_io;
	}

	/* reset hardware */
	ResetAsic(&hx280enc_data);

    /*FIX EDEN_Req00000003 BEGIN DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
	//clk_disable(on2_encode_clk);
	//clk_put(on2_encode_clk);
	//clk_disable(on2_ebus_clk);
	//clk_put(on2_ebus_clk);
	//clk_disable(on2_e0_clk);
	//clk_put(on2_e0_clk);
    /*FIX EDEN_Req00000003 END DATE:2012-4-23 AUTHOR NAME LINHUICHUN */
	result = register_chrdev(hx280enc_major, "hx280enc", &hx280enc_fops);
	if(result < 0) {
	    printk(KERN_ERR "hx280enc: register_chrdev failed\n");
	    goto error_register_chrdev;
	}
	hx280enc_major = result;

	hx280enc_class = class_create(THIS_MODULE, "hx280enc");
	if (IS_ERR(hx280enc_class)) {
	    printk(KERN_ERR "hx280enc: class_create failed\n");
	    goto error_class_create;
	}
	device_create(hx280enc_class, NULL, MKDEV(hx280enc_major, 0), NULL, "hx280enc");

	result = ion_iommu_attach_dev(hx280enc_data.dev);
	if(result < 0)
	{
		printk(KERN_ERR "hx280enc: iommu_attach_device failed\n");
		goto error_iommu_attach_device;
	}

	printk(KERN_INFO "hx280enc: probe successfully. Major = %d\n", hx280enc_major);

	return 0;

error_iommu_attach_device:
	device_destroy(hx280enc_class, MKDEV(hx280enc_major, 0));
	class_destroy(hx280enc_class);
error_class_create:
	unregister_chrdev(hx280enc_major, "hx280enc");
error_register_chrdev:
	ReleaseIO();
error_reserve_io:
	free_irq(hx280enc_data.irq, (void *) &hx280enc_data);
error_request_irq:
	printk(KERN_INFO "hx280enc: probe failed\n");
	return result;
}

void __exit hx280enc_remove(struct platform_device *pdev)
{
	hx280enc_t *dev = (hx280enc_t *) &hx280enc_data;

	/* iommu detach device */
	ion_iommu_detach_dev(hx280enc_data.dev);

	writel(0, hx280enc_data.hwregs + 0x38);  /* disable HW */
	writel(0, hx280enc_data.hwregs + 0x04); /* clear enc IRQ */

	device_destroy(hx280enc_class, MKDEV(hx280enc_major, 0));
	class_destroy(hx280enc_class);
	unregister_chrdev(hx280enc_major, "hx280enc");

	ReleaseIO();

	free_irq(dev->irq, (void *) dev);

	return;
}

static struct platform_device hx280enc_device = {
	.name = "hx280enc",
	.id = -1,
#if defined(CONFIG_COMIP_IOMMU)
	.archdata = {
		.dev_id	= 1,	/*SMMU0 :1, SMMU1: 2*/
		.s_id	= 0,	/*Stream ID*/
	},
#endif
};

static struct platform_driver hx280enc_driver = {
	.driver = {
	    .name = "hx280enc",
	    .owner = THIS_MODULE,
	},
	.remove = __exit_p(hx280enc_remove),
};

static int __init hx280enc_init(void)
{
	platform_device_register(&hx280enc_device);
	return platform_driver_probe(&hx280enc_driver, hx280enc_probe);
}

static void __exit hx280enc_exit(void)
{
	platform_driver_unregister(&hx280enc_driver);
	platform_device_unregister(&hx280enc_device);
}

module_init(hx280enc_init);
module_exit(hx280enc_exit);

MODULE_AUTHOR("Kai Liu <kai.liu@windriver.com>");
MODULE_DESCRIPTION("Hantro 6280/7280/8270 Encoder driver");
MODULE_LICENSE("GPL");
