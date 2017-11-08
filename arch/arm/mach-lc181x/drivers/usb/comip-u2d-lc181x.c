/* drivers/usb/gadget/comip_u2d.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement
 * under which you licensed source code. If you did not accept the terms of the
 * license agreement, you are not authorized to use source code. For the terms
 * of the license, please see the license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 * PURPOSE: This files contains comip uart driver.
 *
 * CHANGE HISTORY:
 *
 * Version	Date		Author		Description
 * 1.0.0	2012-3-27	lengyansong 	created
 *
 */

/**************************************************************************
 * Include Files
**************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/clk.h>

#include <asm/byteorder.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/cdc.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <plat/comip-pmic.h>
#include <plat/hardware.h>
#include <plat/usb.h>
#include <plat/cpu.h>
#include "comip-u2d-lc181x.h"

#define	DRIVER_VERSION			"6-28-2012"
#define	DRIVER_DESC			"COMIP USB 2.0 Device Controller driver"

static enum lcusb_xceiv_events plugin_stat;
/* debug log control */
//#define DBG(fmt, arg...)	printk(KERN_DEBUG "u2d:D: " fmt , ## arg)
#define DBG(fmt, arg...)
#define INFO(fmt, arg...)	printk(KERN_DEBUG "u2d:I: " fmt , ## arg)
#define ERR(fmt, arg...)	printk(KERN_DEBUG "u2d:E: " fmt , ## arg)

#define LYSDBG(fmt, arg...)	do{if(0)printk(KERN_DEBUG "u2d:%s:%d: " fmt ,__func__,__LINE__, ## arg);}while(0)
#define LYSINFO(fmt, arg...)	do{if(1)printk(KERN_DEBUG "u2d:%s:%d: " fmt ,__func__,__LINE__, ## arg);}while(0)

/******************************************************************************
*  Private Macro Definitions
******************************************************************************/
#define val_set(dst, bit, width, val)   do { \
				(dst) &= ~(((0x1<<(width))-1) << (bit));\
				(dst) |= ((val)<<(bit));\
				} while(0)
#define val_get(dst, bit, width)	(((dst) & (((0x1<<(width))-1) << (bit))) >> (bit))

/* ep0 request buf size */
#define REQ_BUFSIZ			(512)

/* hw wait count */
#define MAX_WAIT_COUNT			(5 * 1000)
#define MAX_FLUSH_FIFO_WAIT		(5 * 1000)

/* irq print control */
#define MAX_IRQ_FENCE			(100000)

/* emulation watchdog check */
//#define COMIP_U2D_EMU_TIMEOUT_WDT	1

/******************************************************************************
*  Private Data Declare
******************************************************************************/
static const char driver_name[] = "comip-u2d";
static const char ep0name[] = "ep0";

/* u2d device */
static struct comip_u2d memory;

/* ctrl register bank access */
static DEFINE_SPINLOCK(u2d_lock);

/**************************************************************************
* Function Declaration
**************************************************************************/
static void u2d_stop_activity(struct comip_u2d *dev);
static void u2d_disable(struct comip_u2d *dev);
static void u2d_enable(struct comip_u2d *dev);
static void u2d_setup_complete(struct usb_ep *ep, struct usb_request *req);
static void u2d_nuke(struct comip_ep *, s32 status);
static void u2d_ep0_out_start(struct comip_u2d *dev);
static s32 u2d_ep_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags);
static s32 u2d_ep_enable(struct usb_ep *_ep, const struct usb_endpoint_descriptor *desc);
static s32 u2d_eps_fifo_config(void);

#ifdef COMIP_U2D_EMU_TIMEOUT_WDT
static void u2d_emu_timeout(unsigned long data);
static DEFINE_TIMER(u2d_emu_wd, u2d_emu_timeout, 0, 0);

static unsigned u2d_emu_wdt_set = 0;
static DEFINE_SPINLOCK(u2d_emu_wd_lock);

static void u2d_emu_timeout(unsigned long data)
{
	ERR("%s**** U2D EMULATION timeout ****\n", __func__);
	//BUG();
	ERR("----------------------------------------------------------------------------------------------\n");
}

static void u2d_emu_wdset(void)
{
	struct comip_u2d *dev = &memory;
	unsigned long flags;

	spin_lock_irqsave(&u2d_emu_wd_lock, flags);
	
	u2d_emu_wdt_set = 1;
	u2d_emu_wd.data = (unsigned long) dev;
	mod_timer(&u2d_emu_wd, jiffies + (HZ * 10));		

	spin_unlock_irqrestore(&u2d_emu_wd_lock, flags);
}
static void u2d_emu_wdclr(void)
{
	spin_lock(&u2d_emu_wd_lock);
	
	if (u2d_emu_wdt_set) {
		u2d_emu_wdt_set = 0;
		del_timer_sync(&u2d_emu_wd);
	}
	
	spin_unlock(&u2d_emu_wd_lock);
}
#endif

/*-------------------------------------------------------------------------
		PROC File System Support
-------------------------------------------------------------------------*/
#ifdef CONFIG_USB_GADGET_DEBUG_FILES

#include <linux/seq_file.h>

static const char proc_filename[] = "driver/comip_udc";

static int comip_u2d_proc_show(struct seq_file *m, void *v)
{
	int t, i;
	u32 tmp_reg;

	/* ------basic driver information ---- */
	t = seq_printf(m,
			"\n\n"DRIVER_DESC "\n"
			"%s version: %s\n"
			"Gadget driver: %s\n\n",
			driver_name, DRIVER_VERSION,
			"comip usb gadget");

	/* ------ DR Registers ----- */
	tmp_reg = comip_u2d_read_reg32(USB_BASE);
	t = seq_printf(m,
			"GOTGSC:0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_BASE + 0x04);
	t = seq_printf(m,
			"GOTGINT: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_GAHBCFG);
	t = seq_printf(m,
			"GAHBCFG: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_GUSBCFG);
	t = seq_printf(m,
			"GUSBCFG: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_GRSTCTL);
	t = seq_printf(m,
			"GRSTCTL: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_GINTSTS);
	t = seq_printf(m,
			"GINTSTS: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_GINTMSK);
	t = seq_printf(m,
			"GINTMSK: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_GRXSTSR);
	t = seq_printf(m,
			"GRXSTSR: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DCFG);
	t = seq_printf(m,
			"DCFG: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DCTL);
	t = seq_printf(m,
			"DCTL: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DSTS);
	t = seq_printf(m,
			"DSTS: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DIEPMSK);
	t = seq_printf(m,
			"DIEPMSK: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DOEPMSK);
	t = seq_printf(m,
			"DOEPMSK: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DAINT);
	t = seq_printf(m,
			"DAINT: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DAINTMSK);
	t = seq_printf(m,
			"DAINTMSK: 0x%x\n", tmp_reg);

	tmp_reg = comip_u2d_read_reg32(USB_DIEPCTL0);
	t = seq_printf(m,
			"DIEPCTL0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DIEPCTL(i + 1));
		t = seq_printf(m,
			"DIEPCTL[%d]: 0x%x\n", i + 1, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DOEPCTL0);
	t = seq_printf(m,
			"DOEPCTL0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DOEPCTL(i + 8));
		t = seq_printf(m,
			"DOEPCTL[%d]: 0x%x\n", i + 8, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DIEPINT0);
	t = seq_printf(m,
			"DIEPINT0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DIEPINT(i + 1));
		t = seq_printf(m,
			"DIEPINT[%d]: 0x%x\n", i + 1, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DOEPINT0);
	t = seq_printf(m,
			"DOEPINT0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DOEPINT(i + 8));
		t = seq_printf(m,
			"DOEPINT[%d]: 0x%x\n", i + 8, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DIEPSIZ0);
	t = seq_printf(m,
			"DIEPSIZ0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DIEPSIZ(i + 1));
		t = seq_printf(m,
			"DIEPSIZ[%d]: 0x%x\n", i + 1, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DOEPSIZ0);
	t = seq_printf(m,
			"DOEPSIZ0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DOEPSIZ(i + 8));
		t = seq_printf(m,
			"DOEPSIZ[%d]: 0x%x\n", i + 8, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DIEPDMA0);
	t = seq_printf(m,
			"DIEPDMA0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DIEPDMA(i + 1));
		t = seq_printf(m,
			"DIEPDMA[%d]: 0x%x\n", i + 1, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DOEPDMA0);
	t = seq_printf(m,
			"DOEPDMA0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DOEPDMA(i + 8));
		t = seq_printf(m,
			"DOEPDMA[%d]: 0x%x\n", i + 8, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DIEPDMAB0);
	t = seq_printf(m,
			"DIEPDMAB0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DIEPDMAB(i + 1));
		t = seq_printf(m,
			"DIEPDMAB[%d]: 0x%x\n", i + 1, tmp_reg);
	}

	tmp_reg = comip_u2d_read_reg32(USB_DOEPDMAB0);
	t = seq_printf(m,
			"DOEPDMAB0: 0x%x\n", tmp_reg);

	for (i = 0; i < 7; i++) {
		tmp_reg = comip_u2d_read_reg32(USB_DOEPDMAB(i + 8));
		t = seq_printf(m,
			"DOEPDMAB[%d]: 0x%x\n", i + 8, tmp_reg);
	}

	for (i=1; i<U2D_EP_NUM; i++) {
		t = seq_printf(m,
			"ep[%d].DMA_DESC:0x%x ,buff:%x\n",
			i,
			memory.ep[i].dma_desc?memory.ep[i].dma_desc->status:-1,
			memory.ep[i].dma_desc?memory.ep[i].dma_desc->buf_addr:-1);
	}

	return 0;
}

static int comip_u2d_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, comip_u2d_proc_show, PDE_DATA(inode));
}

static const struct file_operations comip_u2d_proc_fops = {
	.open		= comip_u2d_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#define comip_u2d_create_proc_file()	proc_create(proc_filename, \
				0, NULL, &comip_u2d_proc_fops)

#define comip_u2d_remove_proc_file()	remove_proc_entry(proc_filename, NULL)

#else				/* !CONFIG_USB_GADGET_DEBUG_FILES */

#define comip_u2d_create_proc_file()	do {} while (0)
#define comip_u2d_remove_proc_file()	do {} while (0)

#endif				/* CONFIG_USB_GADGET_DEBUG_FILES */


/******************
mask: 1:mask,0:unmask
*******************/
static void usb_ep_int_mask(u8 ep_num, u8 mask)
{
	unsigned int val;

	val = comip_u2d_read_reg32(USB_DAINTMSK);
	
	if (mask) { 		//mask
		if (ep_num) { 	// data-ep
			if (ep_num <= U2D_EP_NUM/2) {
				val &= ~(1 << ep_num);
			} else {
				val &= ~(1 << (ep_num+16));
			}
		} else {  	// ep0
			val &= ~(1<<0 | 1<<16);
		}
	} else { 		//ummask
		if (ep_num) { 	// data-ep
			if (ep_num <= U2D_EP_NUM/2) {
				val |= (1 << ep_num);
			} else {
				val |= (1 << (ep_num+16));
			}
		} else {  	// ep0
			val |= (1<<0 | 1<<16);
		}
	}

	comip_u2d_write_reg32(val, USB_DAINTMSK);
}

static void usb_reset(void)
{
	unsigned int val;
	u32 i;
	struct comip_u2d *dev = &memory;

	/*1.setup the NAK bit of all the output endpoint */
	for (i = U2D_EP_NUM / 2 + 1; i < U2D_EP_NUM; i++) {
		val = comip_u2d_read_reg32(USB_DOEPCTL(i));
		val |= USB_DIOEPCTL_SNAK;
		comip_u2d_write_reg32(val, USB_DOEPCTL(i));
	}

	/*2.to disable interrupt */
	val = 0x0; /*disable input/output ep0 interrupt */
	comip_u2d_write_reg32(val, USB_DAINTMSK);

	val = 0;
	val = USB_DOEPMSK_SETUPMSK | USB_DOEPMSK_XFCOMPLMSK | USB_DOEPMSK_SPRMSK
		| USB_DOEPMSK_AHBERRMSK /*| USB_DIEPMSK_BNAMSK*/;
	comip_u2d_write_reg32(val, USB_DOEPMSK);

	val = 0;
	val = USB_DIEPMSK_AHBERRMSK | USB_DIEPMSK_XFCOMPLMSK /*| USB_DIEPMSK_BNAMSK*/;	
	comip_u2d_write_reg32(val, USB_DIEPMSK);

	/*clear in ep tx  empty fifo intrrupt*/
	comip_u2d_write_reg32(0, USB_DIEPEMPMSK);

	/*enable in & out endpoint interrupt */
	val = comip_u2d_read_reg32(USB_GINTMSK);
	val &= ~ (USB_GINTMSK_NPTXFEMP | USB_GINTMSK_PTXFEMP | USB_GINTMSK_RXFLVL);
	val |= USB_GINTMSK_IEPINT | USB_GINTMSK_OEPINT;
	comip_u2d_write_reg32(val, USB_GINTMSK);

	dev->enum_done = 0;
}

static void usb_enum_done(void)
{
	unsigned int val;
	struct comip_u2d *dev = &memory;

	val = comip_u2d_read_reg32(USB_DSTS);

	/*1:to get the usb speed */
	switch ((val & USB_DSTS_ENUMSPD) >> 1) {
	case 0:
		dev->gadget.speed = USB_SPEED_HIGH;
		break;
	case 1:
		dev->gadget.speed = USB_SPEED_FULL;
		break;
	case 2:
		dev->gadget.speed = USB_SPEED_LOW;
		break;
	default:
		return;

	}

	INFO("%s: usb speed =0x%x\n", __func__, dev->gadget.speed);

	/*2:to set up the mps */
	val = comip_u2d_read_reg32(USB_DIEPCTL0);
	val_set(val, 0, 2, 0);	/*to set the mps=64 */
	comip_u2d_write_reg32(val, USB_DIEPCTL0);

	/* prepare ep0 out for receive */
	u2d_ep0_out_start(dev);

    /*4. enable ep0 interrupt*/
	val = 0x10001; /*enable input/output ep0 interrupt */
	comip_u2d_write_reg32(val, USB_DAINTMSK);
}

static void u2d_setaddress(u16 address)
{
	unsigned int val;

	val = comip_u2d_read_reg32(USB_DCFG);
	val_set(val, 4, 7, address);	/* to set the usb device address added by sdx */
	comip_u2d_write_reg32(val, USB_DCFG);
}

static void u2d_clk_set(s32 enable)
{
	struct comip_u2d *dev = &memory;
	static int clk_status = 0;

	if (enable) {
		if (!clk_status)
			clk_enable(dev->clk);
	} else {
		if (clk_status)
			clk_disable(dev->clk);
	}
	clk_status = enable;
	LYSINFO("usbotg_12m_clk:%d\n",clk_status);
}

static s32 u2d_soft_dis(s32 disable)
{
	unsigned int val;

	if (disable) {
		/*enable soft disconnect */
		val = comip_u2d_read_reg32(USB_DCTL);
		val |= USB_DCTL_SFTDISC;
		comip_u2d_write_reg32(val, USB_DCTL);
		INFO("USB soft disconnect!\n");
	} else {
		/*disable soft disconnect */
		val = comip_u2d_read_reg32(USB_DCTL);
		val &= ~USB_DCTL_SFTDISC;
		comip_u2d_write_reg32(val, USB_DCTL);
		INFO("USB soft connect!\n");
	}

	return 0;
}

static void u2d_ep_flush(int ep_num, int dir_in)
{
	unsigned int val;
	int i = 0;
	
	DBG("ep%d flush!\n", ep_num);

	if (dir_in) {
		val = comip_u2d_read_reg32(USB_GRSTCTL);
		val_set(val, 6, 5, ep_num);	/*Now NP Tx FIFO flush */
		val |= USB_GRSTCTL_TXFFLSH;
		comip_u2d_write_reg32(val, USB_GRSTCTL);

		val = comip_u2d_read_reg32(USB_GRSTCTL);
		while (val & USB_GRSTCTL_TXFFLSH) {
			i++;
			if (i > MAX_FLUSH_FIFO_WAIT) {
				DBG("%s: wait USB_GRSTCTL_TXFFLSH time out\n", __func__);
				return ;
			}
			udelay(1);
			val = comip_u2d_read_reg32(USB_GRSTCTL);
		}
	} else {
		val = comip_u2d_read_reg32(USB_GRSTCTL);
		val |= USB_GRSTCTL_RXFFLSH;
		comip_u2d_write_reg32(val, USB_GRSTCTL);

		i = 0;
		val = comip_u2d_read_reg32(USB_GRSTCTL);
		while (val & USB_GRSTCTL_RXFFLSH) {
			i++;
			if (i > MAX_FLUSH_FIFO_WAIT) {
				DBG("%s: wait USB_GRSTCTL_RXFFLSH time out\n", __func__);
				return ;
			}
			udelay(1);
			val = comip_u2d_read_reg32(USB_GRSTCTL);
		}
	}
}

static s32 get_mps(s32 speed, u8 bmAttributes)
{
	switch (bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
	case USB_ENDPOINT_XFER_CONTROL:
		return (EP0_MPS);
	case USB_ENDPOINT_XFER_ISOC:
		return (ISO_MPS(speed));
	case USB_ENDPOINT_XFER_BULK:
		return (BULK_MPS(speed));
	case USB_ENDPOINT_XFER_INT:
		return (INT_MPS(speed));
	default:
		return 0;
	}
}

static void u2d_change_mps(enum usb_device_speed speed)
{
	unsigned i;
	struct comip_ep *ep = NULL;
	struct comip_u2d *dev = &memory;

	/* find all validate EPs and change the MPS */
	for (i = 1; i < U2D_EP_NUM; i++) {
		if (dev->ep[i].assigned) {
			ep = &dev->ep[i];
			switch (ep->ep_type & USB_ENDPOINT_XFERTYPE_MASK) {
			case USB_ENDPOINT_XFER_CONTROL:
				ep->ep.maxpacket = EP0_MPS;
				break;
			case USB_ENDPOINT_XFER_ISOC:
				ep->ep.maxpacket = ISO_MPS(speed);
				break;
			case USB_ENDPOINT_XFER_BULK:
				ep->ep.maxpacket = BULK_MPS(speed);
				break;
			case USB_ENDPOINT_XFER_INT:
				ep->ep.maxpacket = INT_MPS(speed);
				break;
			default:
				break;
			}
		}
	}
}

static void done0(struct comip_ep *ep, struct comip_request *req, s32 status)
{
	/* unmap the req buf */
	if (req->map) {
		/* unmap dma */
		dma_unmap_single(ep->dev->dev, req->req.dma, req->req.length,
				 ep->dir_in? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		req->req.dma = 0;
		req->map	 = 0;
	}

	list_del_init(&req->queue);
	req->req.status = status;
	if (req->req.complete)
	{
		spin_unlock(ep->lock);
		DBG("req->req.complete:0x%p\n",(req->req.complete));
		req->req.complete(&ep->ep, &req->req);
		spin_lock(ep->lock);
	}
}
static void done(struct comip_ep *ep, struct comip_request *req, s32 status)
{
	/* unmap the req buf */
	if (req->map) {
		/* unmap dma */
		dma_unmap_single(ep->dev->dev, req->req.dma, req->req.length,
				 ep->dir_in? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		req->req.dma = 0;
		req->map	 = 0;
	}

	list_del_init(&req->queue);
	req->req.status = status;
	if (req->req.complete)
	{
		spin_unlock(ep->lock);
		DBG("req->req.complete:0x%p\n",(req->req.complete));
		req->req.complete(&ep->ep, &req->req);
		spin_lock(ep->lock);
	}
}

static void ep0_idle(struct comip_u2d *dev)
{
	dev->ep0state = EP0_IDLE;
}

static void cancel_dma(struct comip_ep *ep)
{
	unsigned int val;

	if (ep->ep_num != 0) {
		if (ep->dir_in) {
			/*set EP disable */
			val = comip_u2d_read_reg32(USB_DIEPCTL(ep->ep_num));
			val |= USB_DIOEPCTL_EPDIS;
			comip_u2d_write_reg32(val, USB_DIEPCTL(ep->ep_num));
		} else {
			/*set EP disable */
			val = comip_u2d_read_reg32(USB_DOEPCTL(ep->ep_num));
			val |= USB_DIOEPCTL_EPDIS;
			comip_u2d_write_reg32(val, USB_DOEPCTL(ep->ep_num));
		}
	} else {
		if (ep->dir_in) {
			/*set EP disable */
			val = comip_u2d_read_reg32(USB_DIEPCTL0);
			val |= USB_DIEPCTL0_EPDIS;
			comip_u2d_write_reg32(val, USB_DIEPCTL0);

		} else {
			/*set EP disable */
			val = comip_u2d_read_reg32(USB_DOEPCTL0);
			val |= USB_DOEPCTL0_EPDIS;
			comip_u2d_write_reg32(val, USB_DOEPCTL0);
		}
	}

	/* the last tx packet may be incomplete, so flush the fifo.
	 * FIXME correct req.actual if we can
	 */
	u2d_ep_flush(ep->ep_num, ep->dir_in);	/*only for TX */
}

static int usb_power_state = 0;
int comip_usb_power_set(int onoff)
{
	int ret = pmic_voltage_set(PMIC_POWER_USB, 0,
				onoff ? PMIC_POWER_VOLTAGE_ENABLE : PMIC_POWER_VOLTAGE_DISABLE);
	if (ret)
		return ret;
	usb_power_state = onoff ? 1 : 0;

	return 0;
}

static int comip_usb_power_get(void)
{
	return usb_power_state;
}

static int u2d_usb_init(void)
{
	unsigned int val;	
	int i;

	INFO("%s\n", __func__);

	if (!comip_usb_power_get()) {
		INFO("usb not power On!\n");
		return -EBUSY;
	}

	if (cpu_is_lc1813s()) {
		/* improve signal quality in eye diagram */
		val = comip_u2d_read_reg32(CTL_USB_OTG_PHY_CFG0);
		val &= ~( 	(0x03 << CTL_USB_OTG_PHY_CFG0_TXRESTUNE0) | \
					(0x03 << CTL_USB_OTG_PHY_CFG0_TXRISETUNE0) | \
					(0x01 << CTL_USB_OTG_PHY_CFG0_TXPREEMPPULSETUNE0) | \
					(0x03 << CTL_USB_OTG_PHY_CFG0_TXPREEMPAMPTUNE0) );
		val |= ( 	(0x03 << CTL_USB_OTG_PHY_CFG0_TXRESTUNE0) | \
					(0x02 << CTL_USB_OTG_PHY_CFG0_TXRISETUNE0) | \
					(0x01 << CTL_USB_OTG_PHY_CFG0_TXPREEMPPULSETUNE0) | \
					(0x03 << CTL_USB_OTG_PHY_CFG0_TXPREEMPAMPTUNE0) );
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_CFG0);

		/*set USB PHY reset */
		val = 0x00;
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_RST_CTRL);

		u2d_clk_set(0);

		/*set USB PHY no suspend */
		val = 0x02;
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_SUSPEND);
		mdelay(10);

		/*set USB PHY no reset */
		val = comip_u2d_read_reg32(CTL_USB_OTG_PHY_RST_CTRL);
		val |= 0x01;
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_RST_CTRL);

		u2d_clk_set(1);
		mdelay(10);

		u2d_soft_dis(1);
		mdelay(10);

		/*set USB PHY no suspend */
		val = comip_u2d_read_reg32(CTL_USB_OTG_PHY_SUSPEND);
		val |= 0x01;
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_SUSPEND);
		mdelay(10);

		/*set USB PHY no reset */
		val = comip_u2d_read_reg32(CTL_USB_OTG_PHY_RST_CTRL);
		val |= 0x10;
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_RST_CTRL);
	} else {

		u2d_clk_set(1);
		mdelay(10);

		/* soft disconnect first */
		u2d_soft_dis(1);
		mdelay(10);

		/*set USB PHY no suspend */
		val = 0x01;
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_SUSPEND);
		mdelay(10);

		/*set USB PHY no reset */
		val = 0x01;
		comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_RST_CTRL);
		mdelay(10);
	}

	/*Soft reset */
	val = USB_GRSTCTL_CSRST;
	comip_u2d_write_reg32(val, USB_GRSTCTL);
	mdelay(10);

	i = 0;
	val = comip_u2d_read_reg32(USB_GRSTCTL);
	while (!(val & USB_GRSTCTL_AHBIDLE) || (val & USB_GRSTCTL_CSRST)) {
		i++;
		if (i > MAX_WAIT_COUNT) {
			ERR("%s: wait USB_GRSTCTL_AHBIDLE  time out\n",  __func__);
			goto err;
		}
		val = comip_u2d_read_reg32(USB_GRSTCTL);
	}

	val = USB_GAHBCFG_DMAEN | USB_GAHBCFG_GLBINTMSK; /* DMA mode */
	comip_u2d_write_reg32(val, USB_GAHBCFG);
	val = comip_u2d_read_reg32(USB_GAHBCFG);

#ifndef CONFIG_USB_COMIP_OTG
	/*force device mode, add by zw */
	val = comip_u2d_read_reg32(USB_GINTSTS);
	if ((val & USB_GINTSTS_CURMOD)) {
		val = comip_u2d_read_reg32(USB_GUSBCFG);
		val |= USB_GUSBCFG_FRCDM;
		comip_u2d_write_reg32(val, USB_GUSBCFG);
		mdelay(50);
	}
#endif

	/*disable HNPCap and SRPCap,USB Turnaround Time to be 5,
	 *16bits USB 2.0 HS UTMI+ PHY ,FS Timeout Calibration to be 5 */
	val = 0x1408;
	comip_u2d_write_reg32(val, USB_GUSBCFG);

	/* ensable des DMA, FS, set the NZStsOUTHShk */
	val = 0x4;
	val |= USB_DCFG_DESCDMA | USB_DCFG_ENOUTNAK; /* enable desriptor DMA */
	comip_u2d_write_reg32(val, USB_DCFG);
	mdelay(10);

	/* configure fifo */
	u2d_eps_fifo_config();
	/* flush fifo after configure fifo */
	u2d_ep_flush(0x10, 1);
	u2d_ep_flush(0, 0);

	/* Clear all pending Device Interrupts */
	comip_u2d_write_reg32(0, USB_DIEPMSK);
	comip_u2d_write_reg32(0, USB_DOEPMSK);
	comip_u2d_write_reg32(0xffffffff, USB_DAINT);
	comip_u2d_write_reg32(0, USB_DAINTMSK);

	/* endpoint init */
	/* IN */
	/* endpoint 0-7 */
	for (i=0; i <=U2D_EP_NUM/2; i++) {		
		val = comip_u2d_read_reg32(USB_DIEPCTL(i));
		if (val & USB_DIOEPCTL_EPEN) { //endpoint is enabled
			val = USB_DIOEPCTL_EPDIS | USB_DIOEPCTL_SNAK;
			comip_u2d_write_reg32(val, USB_DIEPCTL(i));
		} else {
			val = 0;
			comip_u2d_write_reg32(val, USB_DIEPCTL(i));
		}
		comip_u2d_write_reg32(0, USB_DIEPSIZ(i));
		comip_u2d_write_reg32(0, USB_DIEPDMA(i));
		comip_u2d_write_reg32(0xff, USB_DIEPINT(i));
	}
	/* OUT */
	/* endpoint 0 */
	i = 0;
	val = comip_u2d_read_reg32(USB_DOEPCTL(i));
	if (val & USB_DIOEPCTL_EPEN) { //endpoint is enabled
		val = USB_DIOEPCTL_EPDIS | USB_DIOEPCTL_SNAK;
		comip_u2d_write_reg32(val, USB_DOEPCTL(i));
	} else {
		val = 0;
		comip_u2d_write_reg32(val, USB_DOEPCTL(i));
	}
	comip_u2d_write_reg32(0, USB_DOEPSIZ(i));
	comip_u2d_write_reg32(0, USB_DOEPDMA(i));
	comip_u2d_write_reg32(0xff, USB_DOEPINT(i));
	/* endpoint 8-14 */

	for (i=U2D_EP_NUM/2+1; i <U2D_EP_NUM; i++) {		
		val = comip_u2d_read_reg32(USB_DOEPCTL(i));
		if (val & USB_DIOEPCTL_EPEN) { //endpoint is enabled
			val = USB_DIOEPCTL_EPDIS | USB_DIOEPCTL_SNAK;
			comip_u2d_write_reg32(val, USB_DOEPCTL(i));
		} else {
			val = 0;
			comip_u2d_write_reg32(val, USB_DOEPCTL(i));
		}
		comip_u2d_write_reg32(0, USB_DOEPSIZ(i));
		comip_u2d_write_reg32(0, USB_DOEPDMA(i));
		comip_u2d_write_reg32(0xff, USB_DOEPINT(i));
	}

	i = 0;
#ifndef CONFIG_USB_COMIP_OTG
	/*wait for device mode, add by zw */
	val = comip_u2d_read_reg32(USB_GINTSTS);
	while (val & USB_GINTSTS_CURMOD) {
		i++;
		if (i > MAX_WAIT_COUNT) {
			ERR("%s: wait USB_GINTSTS_CURMOD  time out\n",  __func__);
			goto err;
		}
		udelay(1);
		val = comip_u2d_read_reg32(USB_GINTSTS);
	}
#endif
	return 0;

err:
	/*disable suspend*/
	val = 0x00;
	comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_SUSPEND);
	mdelay(10);

	/*disable phy */
	val = 0x00;
	comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_RST_CTRL);
	mdelay(10);

	return -EBUSY;
}

static void u2d_ep0_out_start(struct comip_u2d *dev)
{
	unsigned int val;

	memset(dev->ep0_out_desc, 0, sizeof(struct comip_dma_des));
	/* dma phy addr */
	dev->ep0_out_desc->buf_addr = dev->ep0_req.req.dma;
	/* status init */
	val_set(dev->ep0_out_desc->status, STAT_IO_BS_SHIFT, 2, STAT_IO_BS_H_BUSY);
	val_set(dev->ep0_out_desc->status, STAT_IO_L_SHIFT, 1, 1);
	val_set(dev->ep0_out_desc->status, STAT_IO_IOC_SHIFT, 1, 1);
	val_set(dev->ep0_out_desc->status, 0, 16, dev->ep[0].ep.maxpacket);
	val_set(dev->ep0_out_desc->status, STAT_IO_BS_SHIFT, 2, STAT_IO_BS_H_READY);

	/* wait phy addr */
	comip_u2d_write_reg32(dev->ep0_out_desc_dma_addr, USB_DOEPDMA0);

	// Programming Model for Bulk OUT Endpoints With OUT NAK Set for Device
	// Descriptor DMA Mode
	// Program the DOEPTSIZn register for the transfe r size and the
	// corresponding packet count
	val = comip_u2d_read_reg32(USB_DOEPSIZ0);
	val_set(val, 19, 10, 1);
	val_set(val, 0, 19, dev->ep[0].ep.maxpacket);
	comip_u2d_write_reg32(val, USB_DOEPSIZ0);

	smp_wmb();
	/* start ep0 out */
	comip_u2d_write_reg32(USB_DOEPCTL0_EPEN | USB_DOEPCTL0_CNAK, USB_DOEPCTL0);	
}

static void u2d_ep0_in_start(struct comip_u2d *dev, struct comip_request *req)
{
	unsigned int val;

	memset(dev->ep0_in_desc, 0, sizeof(struct comip_dma_des));
	/* status init */
	val_set(dev->ep0_in_desc->status, STAT_IO_BS_SHIFT, 2, STAT_IO_BS_H_BUSY);
	val_set(dev->ep0_in_desc->status, STAT_IO_L_SHIFT, 1, 1);
	val_set(dev->ep0_in_desc->status, STAT_IO_IOC_SHIFT, 1, 1);
	val_set(dev->ep0_in_desc->status, STAT_IO_SP_SHIFT, 1, 1);
	val_set(dev->ep0_in_desc->status, 0, 16,req->req.length);
	val_set(dev->ep0_in_desc->status, STAT_IO_BS_SHIFT, 2, STAT_IO_BS_H_READY);

	/* flush cache */
	if (!req->req.dma) {
		req->req.dma = dma_map_single(dev->dev, req->req.buf,
				       req->req.length, DMA_TO_DEVICE);
		req->map = 1;
	}	

	/* write the data address to DMA register */
	dev->ep0_in_desc->buf_addr = req->req.dma;
	/* init desc dma addr */
	comip_u2d_write_reg32(dev->ep0_in_desc_dma_addr, USB_DIEPDMA0);

	/* update actual length */
	req->req.actual += req->req.length;

	smp_wmb();
	/* start ep0 in */
	val = comip_u2d_read_reg32(USB_DIEPCTL0);
	val |= USB_DIOEPCTL_CNAK | USB_DIOEPCTL_EPEN;
	comip_u2d_write_reg32(val, USB_DIEPCTL0);
}


static int u2d_ep_out_start(struct comip_ep *ep, struct comip_request *req)
{
	unsigned int val;

	if (req->req.length==0) {
		ERR("%s, req->req.length = 0\n", __func__);
		return -EINVAL;
	}

	memset(ep->dma_desc, 0, sizeof(struct comip_dma_des));

	/* status init */
	val_set(ep->dma_desc->status, STAT_IO_BS_SHIFT,  2, STAT_IO_BS_H_BUSY);
	val_set(ep->dma_desc->status, STAT_IO_L_SHIFT, 	 1, 1);
	val_set(ep->dma_desc->status, STAT_IO_IOC_SHIFT, 1, 1);
	val_set(ep->dma_desc->status, 0, 16,\
			GET_DMA_BYTES(req->req.length, ep->ep.maxpacket));
	val_set(ep->dma_desc->status, STAT_IO_BS_SHIFT,  2, STAT_IO_BS_H_READY);

	/* only map dma buffer */
	if (!req->req.dma) {
		req->req.dma = dma_map_single(ep->dev->dev, req->req.buf,
				       req->req.length, DMA_FROM_DEVICE);
		req->map = 1;
	} else {
		INFO("req->req.dma:%d\n",req->req.dma);
	}

	/* write the data address to DMA register */
	ep->dma_desc->buf_addr = req->req.dma;
	/* init desc dma addr */
	comip_u2d_write_reg32(ep->dma_desc_dma_addr, USB_DOEPDMA(ep->ep_num));

	// Programming Model for Bulk OUT Endpoints With OUT NAK Set for Device
	// Descriptor DMA Mode
	// Program the DOEPTSIZn register for the transfe r size and the
	// corresponding packet count
	val = comip_u2d_read_reg32(USB_DOEPSIZ(ep->ep_num));
	val_set(val, 19, 10, GET_DMA_BYTES(req->req.length, ep->ep.maxpacket)/ep->ep.maxpacket);
	val_set(val, 0, 19, req->req.length);
	comip_u2d_write_reg32(val, USB_DOEPSIZ(ep->ep_num));

	smp_wmb();
	/* start out dma */
	val = comip_u2d_read_reg32(USB_DOEPCTL(ep->ep_num));
	val |= USB_DIOEPCTL_CNAK | USB_DIOEPCTL_EPEN;
	comip_u2d_write_reg32(val, USB_DOEPCTL(ep->ep_num));	

	return 0;
}
static void u2d_ep_in_start(struct comip_ep *ep, struct comip_request *req, int zero)
{
	unsigned int val;

	memset(ep->dma_desc, 0, sizeof(struct comip_dma_des));

	/* status init */
	val_set(ep->dma_desc->status, STAT_IO_BS_SHIFT,  2, STAT_IO_BS_H_BUSY);
	val_set(ep->dma_desc->status, STAT_IO_L_SHIFT, 	 1, 1);
	val_set(ep->dma_desc->status, STAT_IO_IOC_SHIFT, 1, 1);
	if ((req->req.length % ep->ep.maxpacket) || zero)
		val_set(ep->dma_desc->status, STAT_IO_SP_SHIFT,  1, 1);
	if (!zero)
		val_set(ep->dma_desc->status, 0, 16,req->req.length);
	val_set(ep->dma_desc->status, STAT_IO_BS_SHIFT, 2, STAT_IO_BS_H_READY);

	/* flush cache */
	if (!req->req.dma) {
		req->req.dma = dma_map_single(ep->dev->dev, req->req.buf,
					req->req.length, DMA_TO_DEVICE);
		req->map = 1;
	}	

	/* write the data address to DMA register */
	ep->dma_desc->buf_addr = req->req.dma;
	/* init desc dma addr */
	comip_u2d_write_reg32(ep->dma_desc_dma_addr, USB_DIEPDMA(ep->ep_num));

	/* update actual length */
	if (!zero)
		req->req.actual += req->req.length;

	smp_wmb();
	/* start ep in */
	val = comip_u2d_read_reg32(USB_DIEPCTL(ep->ep_num));
	val |= USB_DIOEPCTL_CNAK | USB_DIOEPCTL_EPEN;
	comip_u2d_write_reg32(val, USB_DIEPCTL(ep->ep_num));
}

static void u2d_nuke(struct comip_ep *ep, s32 status)
{
	unsigned int val;
	struct comip_request *req;

	/* called with irqs blocked */
	if (!ep->stopped)
		cancel_dma(ep);

	while (!list_empty(&ep->queue)) {
		req = container_of(ep->queue.next, struct comip_request, queue);
		done(ep, req, status);
	}

	if (ep->ep.desc && ep->ep_num) {
		if (ep->dir_in) {
			val = comip_u2d_read_reg32(USB_DIEPINT(ep->ep_num));
			val |= USB_DIOEPINT_BNA | USB_DIOEPINT_TXFUNDN
			    | USB_DIOEPINT_INNAKEFF | USB_DIOEPINT_INTXFEMP
			    | USB_DIOEPINT_TIMEOUT | USB_DIOEPINT_AHBERR |
			    USB_DIOEPINT_EPDIS;
			comip_u2d_write_reg32(val, USB_DIEPINT(ep->ep_num));
		} else {
			val = comip_u2d_read_reg32(USB_DOEPINT(ep->ep_num));
			val |= USB_DIOEPINT_BNA | USB_DIOEPINT_OUTPKTERR
			    | USB_DIOEPINT_B2BSETUP | USB_DIOEPINT_OUTEPDIS
			    | USB_DIOEPINT_SETUP | USB_DIOEPINT_AHBERR |
			    USB_DIOEPINT_EPDIS;
			comip_u2d_write_reg32(val, USB_DOEPINT(ep->ep_num));
		}
	}
}

static s32 u2d_eps_fifo_config(void)
{
	unsigned int val;
	#if 0
	u16 i, fifo_size, fifo_start;

	val = 0x01c0;		/*Default rx fifo depth */
	comip_u2d_write_reg32(val, USB_GRXFSIZ);

	val = comip_u2d_read_reg32(USB_GNPTXFSIZ);
	val_set(val, 16, 16, 0xC0);
	val_set(val, 0, 16, 0x01c0);	/*Tx fifo0 */
	comip_u2d_write_reg32(val, USB_GNPTXFSIZ);

	fifo_start = 0x0280;
	for (i = 1; i <= U2D_EP_NUM / 2; i++) {
		fifo_size = BULK_FIFO_SIZE / 4;
		val = comip_u2d_read_reg32(USB_DIEPTXF(i));
		val_set(val, 16, 16, fifo_size);
		val_set(val, 0, 16, fifo_start);	/*Tx fifo0 */
		comip_u2d_write_reg32(val, USB_DIEPTXF(i));
		fifo_start +=  fifo_size;
	}
	#else
	/*The default FIFO size of EP7 is 0x200, but than it more than DFIFO size
	   So the FIFO size is configurated as 0x100*/
	val = comip_u2d_read_reg32(USB_DIEPTXF(7));
	val_set(val, 16, 16, 0x100);
	comip_u2d_write_reg32(val, USB_DIEPTXF(7));
	#endif
	return 0;
}

static void u2d_disable(struct comip_u2d *dev)
{
	unsigned int val;

	INFO("comip_u2d: u2d_disable:disable u2d \n");

	/* clear U2D interrupts, include endpoints and U2DMAs */
	val = comip_u2d_read_reg32(USB_GINTSTS);
	val |=
	    USB_GINTSTS_WKUPINT | USB_GINTSTS_SREQINT | USB_GINTSTS_DISCONNINT |
	    USB_GINTSTS_CONIDSTSCHNG | USB_GINTSTS_FETSUSP |
	    USB_GINTSTS_INCOMPISOOUT | USB_GINTSTS_INCOMPISOIN |
	    USB_GINTSTS_EOPF | USB_GINTSTS_ISOOUTDROP | USB_GINTSTS_ENUMDONE |
	    USB_GINTSTS_USBRST | USB_GINTSTS_USBSUSP | USB_GINTSTS_ERLYSUSP |
	    USB_GINTSTS_SOF | USB_GINTSTS_MODEMIS;
	comip_u2d_write_reg32(val, USB_GINTSTS);

	/* mask all interrupt */
	val = 0x00;
	comip_u2d_write_reg32(val, USB_GINTMSK);

	/*disable suspend*/
	val = 0x00;
	comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_SUSPEND);
	mdelay(10);

	/*disable phy */
	val = 0x00;
	comip_u2d_write_reg32(val, CTL_USB_OTG_PHY_RST_CTRL);
	mdelay(10);

	#if defined(CONFIG_CPU_LC1810)
	/* disable and suspend host/hsic phy. */
	comip_u2d_write_reg32(val, CTL_USB_HOST_PHY_SUSPEND);
	comip_u2d_write_reg32(val, CTL_USB_HOST_PHY_RST_CTRL);

	comip_u2d_write_reg32(val, CTL_USB_HSIC_PHY_SUSPEND);
	comip_u2d_write_reg32(val, CTL_USB_HSIC_PHY_RST_CTRL);
	#endif

	mdelay(10);

	ep0_idle(dev);
	dev->gadget.speed = USB_SPEED_UNKNOWN;

	u2d_clk_set(0);

	mdelay(10);
}

static void u2d_reinit(struct comip_u2d *dev)
{
	u16 i;

	ep0_idle(dev);

	/* basic endpoint records init */
	for (i = 0; i < U2D_EP_NUM; i++) {
		struct comip_ep *ep = &dev->ep[i];

		ep->stopped = 0;
		ep->irqs = 0;
	}
}

static void u2d_enable(struct comip_u2d *dev)
{
	unsigned int val;

	ep0_idle(dev);
	/* default speed, or should be unknown here? */
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	/* reset statics */
	memset(&dev->stats, 0, sizeof(dev->stats));
	/*Clear irq */
	val = comip_u2d_read_reg32(USB_GINTSTS);
	val |=
	    USB_GINTSTS_WKUPINT | USB_GINTSTS_SREQINT | USB_GINTSTS_DISCONNINT |
	    USB_GINTSTS_CONIDSTSCHNG | USB_GINTSTS_FETSUSP |
	    USB_GINTSTS_INCOMPISOOUT | USB_GINTSTS_INCOMPISOIN |
	    USB_GINTSTS_EOPF | USB_GINTSTS_ISOOUTDROP | USB_GINTSTS_ENUMDONE |
	    USB_GINTSTS_USBRST | USB_GINTSTS_USBSUSP | USB_GINTSTS_ERLYSUSP |
	    USB_GINTSTS_SOF | USB_GINTSTS_MODEMIS;
	comip_u2d_write_reg32(val, USB_GINTSTS);

	/* enable reset/suspend/early suspend/ENUM/SOF irqs */
	val = comip_u2d_read_reg32(USB_GINTMSK);
	val &= ~ (USB_GINTMSK_NPTXFEMP | USB_GINTMSK_PTXFEMP | USB_GINTMSK_RXFLVL);
	val |= USB_GINTMSK_USBRST | USB_GINTMSK_ENUMDONE;
	comip_u2d_write_reg32(val, USB_GINTMSK);

	/*val = comip_u2d_read_reg32(ICTL1_IRQ_INTMASK);
	val &= ~(1 << 2);
	comip_u2d_write_reg32(val, ICTL1_IRQ_INTMASK);*/
	//enable_irq(IRQ_ARM1_USB);
}

static void u2d_stop_activity(struct comip_u2d *dev)
{
	int i;

	/* don't disconnect drivers more than once */
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	/* prevent new request submissions, kill any outstanding requests  */
	for (i = 0; i < U2D_EP_NUM; i++) {
		struct comip_ep *ep = &dev->ep[i];
		u2d_nuke(ep, -ESHUTDOWN);
		ep->stopped = 1;
	}

	dev->gadget_stoped = 1;
}

static void u2d_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	INFO("u2d_setup_complete\n");
	return;
}

static s32 u2d_do_request(struct usb_ctrlrequest *ctrl, struct comip_ep *ep)
{
	struct comip_u2d   *dev = &memory;
	struct usb_request *_req = &dev->ep0_req.req;
	s32 value = -EOPNOTSUPP;
	s32 pseudo = 0, ret = 0;
	s32 ep_num;

	switch (ctrl->bRequest) {
	case USB_REQ_SET_ADDRESS:
		LYSINFO("%s, USB_REQ_SET_ADDRESS 0x%x\n", __func__, ctrl->wValue);
		u2d_setaddress(ctrl->wValue);
		pseudo = 1;
		value = 0;
		break;
	case USB_REQ_CLEAR_FEATURE:
		LYSINFO("%s,ep%d USB_REQ_CLEAR_FEATURE\n", __func__, ep->ep_num);
		pseudo = 1;
		value = 0;
		break;
	case USB_REQ_SET_FEATURE:
		LYSINFO("%s,ep%d USB_REQ_SET_FEATURE\n", __func__, ep->ep_num);
		/*enter TEST_MODE*/
		//DBG("%s:SET_FEATURE:bRequestType=0x%x,wValue=0x%x,wIndex=0x%x\n",
		//	__func__, ctrl->bRequestType, ctrl->wValue, ctrl->wIndex);
		pseudo = 1;
		value = 0;
		break;
	case USB_REQ_GET_STATUS:
		/* send 2 zero bytes to host for get device status */
		//DBG("%s: GET_STATUS\n", __func__);
		//if(ctrl->wIndex & 0xf)
		//	break;
		if (le16_to_cpu(ctrl->wLength) != 2
			|| le16_to_cpu(ctrl->wValue) != 0) {
			INFO("Error wValue:0x%x wLength:0x%x!\n",
				ctrl->wValue, ctrl->wLength);
			break;
		}

		pseudo = 1;
		value = 2;
		memset(_req->buf, 0, value);
		if ((ctrl->bRequestType &  USB_RECIP_MASK) == USB_RECIP_DEVICE) {
			*(u16*)(_req->buf) = (1 << USB_DEVICE_SELF_POWERED) | 
					             (1 << USB_DEVICE_REMOTE_WAKEUP);
		}
		else if ((ctrl->bRequestType &  USB_RECIP_MASK) == USB_RECIP_ENDPOINT) {
			memset(_req->buf, 0, value);
			ep_num =  le16_to_cpu(ctrl->wIndex) & USB_ENDPOINT_NUMBER_MASK;
			*((u16 *)_req->buf) = (comip_u2d_read_reg32(USB_DIEPCTL(ep_num))
				& USB_DIOEPCTL_STALL) ? 1 : 0 ;
			LYSINFO("USB_REQ_GET_STATUS ENDPOINT: ep%d %s\n", ep->ep_num,
				(*((u16 *)_req->buf))?"halted":"idle");
		} else if ((ctrl->bRequestType &  USB_RECIP_MASK) == USB_RECIP_INTERFACE) {
			LYSINFO("USB_REQ_GET_STATUS INTERFACE\n");
		} else {
			LYSINFO("unrecognize bRequestType:0x%x!\n", ctrl->bRequestType);
		}
		DBG("handle usb_req_get_status packet at ep%d, buf 0x%p, *buf 0x%x,\n",
			ep->ep_num, _req->buf, *(u16*)(_req->buf));
		break;		
	default:
		pseudo = 0;
		ERR("%s, unrecognize request\n", __func__);
		break;
	}

	if (pseudo) {	
		_req->length = value;
		_req->no_interrupt = 0;
		_req->zero = (value <= ctrl->wLength) &&
		                ((value % ep->ep.maxpacket) == 0);
		DBG("%s, u2d_ep_queue zero:%d, value:%d, ctrl->wLength:%d, ep->ep.maxpacket:%d\n", \
				__func__, _req->zero, value, ctrl->wLength, ep->ep.maxpacket);
		_req->complete = u2d_setup_complete;
		ret = u2d_ep_queue(&ep->ep, _req, GFP_KERNEL);
		if (ret < 0) {
			_req->status = 0;
			u2d_setup_complete(&ep->ep, _req);
		}
		return value;
	} else {
		return -1;
	}
}

static int u2d_wait_dma_done(struct comip_ep *ep,
					struct comip_dma_des *dma_des)
{
	int timeout=0;
	int ret = 0;
	while ((dma_des->status >> STAT_IO_BS_SHIFT) == STAT_IO_BS_DMABUSY) {
		INFO("%s waiting for dma transfer compelete after int, des status 0x%x, AHB 0x%x\n",
			ep->name, dma_des->status, comip_u2d_read_reg32(USB_GAHBCFG));
		if (timeout ++ > 100) {
			if (timeout > 200) {
				INFO("time is too long!\n");
				ret = -ETIMEDOUT;
				break;
			}
			udelay(1);
		}
	}

	if ((dma_des->status >> STAT_IO_BS_SHIFT) != STAT_IO_BS_DMADONE) {
		INFO("check dma status!dma des status 0x%x, AHB 0x%x\n",
			dma_des->status, comip_u2d_read_reg32(USB_GAHBCFG));
		ret = -EIO;
	}
	return ret;
}

static void u2d_rx_dma_handle(struct comip_ep *ep,
	struct comip_dma_des *dma_des, struct comip_request *req)
{
	int length;
	unsigned int val;

#if 1
	u2d_wait_dma_done(ep, dma_des);
#else
	while ((dma_des->status >> STAT_IO_BS_SHIFT) != STAT_IO_BS_DMADONE) {
		INFO("waiting for dma transfer compelete after int, des status 0x%x, AHB 0x%x\n",
			dma_des->status, comip_u2d_read_reg32(USB_GAHBCFG));
		if (timeout ++ > 100) {
			if (timeout > 200) {
				INFO("time is too long!\n");
				break;
			}
			udelay(1);
		}
	}
#endif

	if ((ep->dma_desc->status >> STAT_IO_BS_SHIFT) == STAT_IO_BS_DMADONE) {
		/* update the received data size */
		length = (GET_DMA_BYTES(req->req.length, ep->ep.maxpacket) -
			(dma_des->status & STAT_IO_NON_ISO_RTB_MASK));		

		/* should never happen */
		if (length > U2D_DMA_BUF_SIZE) {
			ERR("%s:length is too large 0x%x,use maxpacket length:0x%lx\n",
				__func__, length, U2D_DMA_BUF_SIZE);
			req->req.actual += U2D_DMA_BUF_SIZE;
			return ;
		}

		val = (ep->dma_desc->status >> STAT_IO_RTS_SHIFT);
		if (val & 0x03) {
			ERR("%s:check buffer status:0x%x\n",
				__func__, ep->dma_desc->status);
		}

		DBG("ep%d-rx:%d\n",ep->ep_num,length);
		req->req.actual += length;
	} else {
		INFO("%s DMA not Done!ep->dma_desc->status:0x%x\n",
			__func__, ep->dma_desc->status);
		u2d_ep_enable(&ep->ep, ep->ep.desc);
	}
}

static s32 u2d_ep_enable(struct usb_ep *_ep,
			   const struct usb_endpoint_descriptor *desc)
{
	unsigned int val;
	struct comip_ep *ep;
	struct comip_u2d *dev;

	ep = container_of(_ep, struct comip_ep, ep);
	dev = ep->dev;

	ep->ep.desc = desc;
	if (!list_empty(&ep->queue))
		INFO("%s, %d enabling a non-empty endpoint!", __func__, ep->ep_num);

	ep->stopped = 0;
	ep->irqs = 0;
	ep->dir_in = usb_endpoint_dir_in(desc) ? 1 : 0;
	ep->ep_num  = usb_endpoint_num(desc);
	ep->ep_type  = usb_endpoint_type(desc);	
	ep->ep.maxpacket = get_mps(dev->gadget.speed, desc->bmAttributes);

	/* flush fifo */
	u2d_ep_flush(ep->ep_num, ep->dir_in);	/*only for TX */

	memset(ep->dma_desc, 0, sizeof(struct comip_dma_des));	

	if (ep->dir_in) {
		/* IN */
		val = comip_u2d_read_reg32(USB_DIEPCTL(ep->ep_num));
		val |= USB_DIOEPCTL_ACTEP;
		val_set(val, 0, 11, ep->ep.maxpacket);
		val_set(val, 18, 2, ep->ep_type);

		if (ep->ep_type & 0x2)	/*INT or BULK */
			val |= USB_DIOEPCTL_SETD0PID;

		val_set(val, 22, 4, ep->ep_num);
		comip_u2d_write_reg32(val, USB_DIEPCTL(ep->ep_num));
	} else {
		 /*OUT*/ 
		val = comip_u2d_read_reg32(USB_DOEPCTL(ep->ep_num));
		val |= USB_DIOEPCTL_ACTEP;
		val_set(val, 0, 11, ep->ep.maxpacket);
		val_set(val, 18, 2, ep->ep_type);

		if (ep->ep_type & 0x2)	/*INT or BULK */
			val |= USB_DIOEPCTL_SETD0PID;

		comip_u2d_write_reg32(val, USB_DOEPCTL(ep->ep_num));
	}

	INFO("%s:: ep%x enabled!\n", __func__, ep->ep_num);
	return 0;
}

static s32
u2d_ep_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags)
{
	struct comip_ep *ep 		= container_of(_ep, struct comip_ep, ep);
	struct comip_request *req 	= container_of(_req, struct comip_request, req);
	struct comip_u2d *dev	    = ep->dev;

	_req->status = -EINPROGRESS;
	_req->actual = 0;
	if(ep->stopped)
		return -ESHUTDOWN;
	//LYSDBG("list_empty(&ep->queue):%d,ep->stopped:%d\n",list_empty(&ep->queue),ep->stopped);
	/* kickstart this i/o queue? */
	if (list_empty(&ep->queue)) {
		//LYSDBG("ep->desc:0x%p,dev->ep0state:%d,ep->dir_in:%d\n",ep->desc,dev->ep0state,ep->dir_in);
		if (ep->ep.desc == 0 /* ep0 */ ) {
			switch (dev->ep0state) {
			case EP0_IN_DATA_PHASE:
				u2d_ep0_in_start(dev, req);			
				done0(ep, req, 0);
				req = 0;
				break;

			case EP0_OUT_DATA_PHASE:
				u2d_ep0_out_start(dev);
				break;
			case EP0_NO_ACTION:
				ep0_idle(dev);
				req = 0;
				break;
			case EP0_IN_FAKE:
				ep0_idle(dev);
				req->req.actual = req->req.length;
				done0(ep, req, 0);
				req = 0;
				break;
			default:
				return -EFAULT;
			}
			/* either start dma or prime pio pump */
		} else {	//PIO
			if (ep->dir_in) {
				/*IN*/
				u2d_ep_in_start(ep, req, 0);
			} else {
				 /*OUT*/				
				u2d_ep_out_start(ep, req);
			}
		}
	}

	/* pio or dma irq handler advances the queue. */
	if (req) {
		DBG("u2d_ep_queue:Add list EP%d :%p,0x%x,0x%x",
			   ep->ep_num, req, req->req.length, req->req.zero);
		list_add_tail(&req->queue, &ep->queue);
	}

	return 0;
}


static void ep_txtimeout(struct comip_ep *ep)
{
	INFO("ep%x timeout!", ep->ep_num);
	return;
}

static void ep_txemp(struct comip_ep *ep)
{
	unsigned int val;

	INFO("ep%x txemp!", ep->ep_num);
	val = USB_DIOEPINT_INTXFEMP;
	comip_u2d_write_reg32(val, USB_DIEPINT(ep->ep_num));

	return;
}

static void ep_txfifoemp(struct comip_ep *ep)
{
	unsigned int val;

	val = comip_u2d_read_reg32(USB_DTXFSTS(ep->ep_num));
	INFO("%s:DTXFSTS=0x%x\n", __func__, val);
}

static void ep_txnak(struct comip_ep *ep)
{
	INFO("ep%x txnak!", ep->ep_num);
	return;
}

static void ep_txundn(struct comip_ep *ep)
{
	INFO("ep%x txundn!", ep->ep_num);
	return;
}

static void ep_rxepdis(struct comip_ep *ep)
{
	INFO("ep%x rxepdis!", ep->ep_num);
	return;
}

static void ep_rxpkterr(struct comip_ep *ep)
{
	INFO("ep%x rxpkterr!", ep->ep_num);
	return;
}


static void handle_ep0(struct comip_u2d *dev)
{
	unsigned int val;
	u32 usb_diepint0;
	u32 usb_doepint0;
	s32 i;
	struct comip_ep *ep = &dev->ep[0];
	struct comip_request *req;
	struct usb_ctrlrequest r;
	u32 *buf;
	unsigned long dma_status;
	u32 doepmsk;
	u32 wait_count = 0;

	DBG("%s, enter\n", __func__);

	val = 0x00;
	usb_diepint0 = comip_u2d_read_reg32(USB_DIEPINT0);
	usb_doepint0 = comip_u2d_read_reg32(USB_DOEPINT0);

	/*clear USB_DIEPINT0 or USB_DOEPINT0*/
	comip_u2d_write_reg32(usb_diepint0, USB_DIEPINT0);
	comip_u2d_write_reg32(usb_doepint0, USB_DOEPINT0);

	if (list_empty(&ep->queue))
		req = 0;
	else
		req = container_of(ep->queue.next, struct comip_request, queue);

	switch (dev->ep0state) {
	case EP0_NO_ACTION:
		DBG("EP0_NO_ACTION\n");
		/*Fall through */
	case EP0_IDLE:
		DBG("EP0_IDLE\n");
		if (usb_diepint0 & USB_DIOEPINT_XFCOMPL) {
			DBG("usb_diepint0 USB_DIOEPINT_XFCOMPL\n");

			val = USB_DIOEPINT_XFCOMPL;
			comip_u2d_write_reg32(val, USB_DIEPINT0);

			u2d_ep0_out_start(dev);
		}

		if (usb_doepint0 & USB_DIOEPINT_XFCOMPL) {
			DBG("usb_doepint0 USB_DIOEPINT_XFCOMPL\n");

			val = USB_DIOEPINT_XFCOMPL;
			comip_u2d_write_reg32(val, USB_DOEPINT0);

			if (!(dev->ep0_out_desc->status & STAT_OUT_NON_ISO_SR)) {
				DBG("%s, dev->ep0_out_desc->status & STAT_OUT_NON_ISO_SR = 0 start rx\n", __func__);
				u2d_ep0_out_start(dev);
			}
		}
		if (!(usb_doepint0 & USB_DIOEPINT_SETUP))
			break;

		DBG("%s, receive setup package\n", __func__);

		val = USB_DIOEPINT_SETUP;	/*Clear */
		comip_u2d_write_reg32(val, USB_DOEPINT0);

		buf = (u32 *)dev->ep0_req.req.buf;
		dev->setup[0] = *buf;
		dev->setup[1] = *(buf + 1);

		*buf = 0;
		*(buf + 1) = 0;

		r.bRequest = (dev->setup[0] >> 8) & 0xff;
		r.bRequestType = (dev->setup[0]) & 0xff;
		r.wValue = (dev->setup[0] >> 16) & 0xffff;
		r.wLength = (dev->setup[1] >> 16) & 0xffff;
		r.wIndex = (dev->setup[1]) & 0xffff;

		if ((r.bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD){
			INFO("setup: bRequest:%x,bRequestType:%x,wValue:%x, wLength:%x,wIndex:%x\n",
				r.bRequest, r.bRequestType, r.wValue, r.wLength, r.wIndex);
		}
		if (r.bRequestType & USB_DIR_IN)
			dev->ep0state = EP0_IN_DATA_PHASE;
		else
			dev->ep0state = EP0_OUT_DATA_PHASE;

		if (r.wLength == 0)
			dev->ep0state = EP0_IN_DATA_PHASE;

		DBG("%s: dev->ep0state=0x%x\n", __func__, dev->ep0state);
		if ((r.bRequest == USB_REQ_SET_ADDRESS)
			||((r.bRequest == USB_REQ_CLEAR_FEATURE)
				&& ((r.bRequestType == 0x00)
					||(r.bRequestType == 0x01)
					||(r.bRequestType == 0x02)))
			||((r.bRequest == USB_REQ_SET_FEATURE)
				&& (r.bRequestType == USB_DIR_OUT)
				&& (r.wValue == 2))
			||((r.bRequest == USB_REQ_GET_STATUS)
				&&((r.bRequestType == (USB_DIR_IN|USB_RECIP_DEVICE))
					||(r.bRequestType == (USB_DIR_IN|USB_RECIP_ENDPOINT))
					||(r.bRequestType == (USB_DIR_IN|USB_RECIP_INTERFACE))))) {
			DBG("%s, u2d_do_request\n", __func__);
			i = u2d_do_request(&r, ep);
		} else {
			spin_unlock(ep->lock);
			DBG("%s, driver setup %p\n", __func__, dev->driver->setup);
			i = dev->driver->setup(&dev->gadget, &r);
			spin_lock(ep->lock);
		}
		DBG("%s setup return: %d", __func__, i);

		if ((i >= 0) && ((r.bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD)) {
			if (r.bRequest == USB_REQ_SET_CONFIGURATION) {
				/* first unmask endpoint 0 interrupt */
				int data = 0x00010001;

				INFO("%s USB_REQ_SET_CONFIGURATION", __func__);

#ifdef COMIP_U2D_EMU_TIMEOUT_WDT
				u2d_emu_wdclr();
#endif
				/* unmask endpoint interrupt */
				for (i = 1; i <=U2D_EP_NUM/2; i++) {
					if(dev->ep[i].assigned) 
						data |= (1<<i);
				}

				for (i=U2D_EP_NUM/2+1 ; i < U2D_EP_NUM; i++) {
					if(dev->ep[i].assigned)  
						data |= (1<<(i+16));
				}
				comip_u2d_write_reg32(data, USB_DAINTMSK);

				/* emulate done */
				dev->gadget_stoped = 0;
				dev->enum_done = 1;
			} else if (r.bRequest == USB_REQ_CLEAR_FEATURE) {
				DBG("%s USB_REQ_CLEAR_FEATURE", __func__);

				if ((r.bRequestType & USB_RECIP_MASK) == USB_RECIP_ENDPOINT) {
					if ((r.wIndex & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) {					
						DBG("%s USB_REQ_CLEAR_FEATURE: USB_DIR_IN", __func__);
						val = comip_u2d_read_reg32(USB_DIEPCTL(r.wIndex & 0x7f));
						val |= USB_DIOEPCTL_SETD0PID;
						if (val & USB_DIOEPCTL_STALL)
							val &=~USB_DIOEPCTL_STALL;

						comip_u2d_write_reg32(val, USB_DIEPCTL(r.wIndex & 0x7f));
						val = comip_u2d_read_reg32(USB_GRSTCTL);
						val_set(val, 6, 5, (r.wIndex & 0x7f));	/*Tx FIFO flush */
						val |= USB_GRSTCTL_TXFFLSH;
						comip_u2d_write_reg32(val, USB_GRSTCTL);

						val = comip_u2d_read_reg32(USB_GRSTCTL);

						wait_count = 0;
						while (val & USB_GRSTCTL_TXFFLSH) {
							val = comip_u2d_read_reg32(USB_GRSTCTL);

							wait_count++;
							if (wait_count > MAX_WAIT_COUNT) {
								INFO("%s: wait USB_GRSTCTL_TXFFLSH  time out\n", __func__);
								return ;
							}
						}						
					} else {
						DBG("%s USB_REQ_CLEAR_FEATURE: USB_DIR_OUT", __func__);

						val = comip_u2d_read_reg32(USB_DOEPCTL(r.wIndex & 0x7f));
						val |= USB_DIOEPCTL_SETD0PID;
						if (val & USB_DIOEPCTL_STALL)
							val &=~USB_DIOEPCTL_STALL;

						comip_u2d_write_reg32(val, USB_DOEPCTL(r.wIndex &0x7f));
					}
				}
			}
			else if (r.bRequest == USB_REQ_SET_FEATURE) 
			{
				INFO("%s USB_REQ_SET_FEATURE", __func__);

				if ((r.bRequestType == USB_DIR_OUT) && (r.wValue == 2) 
					/*&& ((ctrl->wIndex & 0xff) == 0)*/) {
					int test_mode = (r.wIndex >> 8);
					unsigned long test_mask = 0x7;
					unsigned long test_val;

					INFO("%s: enter test_mode=0x%x\n", __func__, test_mode);

					test_val = comip_u2d_read_reg32(USB_DCTL);
					test_val &=~ (test_mask<<USB_DCTL_TSTCTL_SHIFT);
					test_val |= (test_mode << USB_DCTL_TSTCTL_SHIFT);
					comip_u2d_write_reg32(test_val, USB_DCTL);
				}
			}
		}

		if (i < 0) {
			/* hardware automagic preventing STALL... */
			//stall:			
			INFO("setup: bRequest:%x,bRequestType:%x,wValue:%x, wLength:%x,wIndex:%x\n",
				r.bRequest, r.bRequestType, r.wValue, r.wLength, r.wIndex);
			val = comip_u2d_read_reg32(USB_DIEPCTL0);
			val |= USB_DIEPCTL0_STALL;
			comip_u2d_write_reg32(val, USB_DIEPCTL0);
			ep0_idle(dev);
			INFO("handle_ep0:STALL!!");
			u2d_ep0_out_start(dev);
		}
		break;
	case EP0_IN_DATA_PHASE:	/* GET_DESCRIPTOR etc */
		DBG("%s: EP0_IN_DATA_PHASE \n", __func__);

		if (usb_diepint0 & USB_DIOEPINT_XFCOMPL) {	/*tx finish */
			DBG("%s: tx finish\n", __func__);

			/* clear interrupt status */
			val = USB_DIOEPINT_XFCOMPL;
			comip_u2d_write_reg32(val, USB_DIEPINT0);

			if (req) {
				DBG("%s: start send req\n", __func__);
				u2d_ep0_in_start(dev, req);
				done0(ep, req, 0);
				req = 0;
			} else {
				DBG("%s: start rx, ep0_idle\n", __func__);				
				u2d_ep0_out_start(dev);
				ep0_idle(dev);
			}
		}
		break;
	case EP0_OUT_DATA_PHASE:	/* SET_DESCRIPTOR etc */
		DBG("%s: EP0_OUT_DATA_PHASE \n", __func__);
		if (usb_doepint0 & USB_DIOEPINT_XFCOMPL) {
			int length;
			DBG("%s: rx complete\n", __func__);

			/* clear int status */
			val = USB_DIOEPINT_XFCOMPL;
			comip_u2d_write_reg32(val, USB_DOEPINT0);

			length = ep->ep.maxpacket - \
				(dev->ep0_out_desc->status & STAT_IO_NON_ISO_RTB_MASK);			
			if (length > REQ_BUFSIZ) {
				ERR("%s:EP0_OUT_DATA_PHASE: length larger than 0x%x\n", __func__, length);
				dev->ep0state = EP0_STALL;
				break;
			}			
			memcpy((req->req.buf + req->req.actual),
					dev->ep0_req.req.buf, length);
			req->req.actual += length;

			if (req->req.actual < req->req.length) {
				DBG("%s: req->req.actual < req->req.length start to receive again\n", __func__);
				u2d_ep0_out_start(dev);
				break;
			}

			doepmsk = comip_u2d_read_reg32(USB_DOEPMSK);
			if (doepmsk & USB_DIOEPINT_SPR) {
				wait_count = 0;
				while(!(usb_doepint0 & USB_DIOEPINT_SPR)) {
					usb_doepint0 = comip_u2d_read_reg32(USB_DOEPINT0);

					wait_count++;
					if (wait_count > MAX_WAIT_COUNT) {
						ERR("%s: wait USB_DIOEPINT_SPR  time out\n", __func__);
						return ;
					}
				}
				val = USB_DIOEPINT_SPR;
				comip_u2d_write_reg32(val, USB_DOEPINT0);
			}

			if (req)
				done0(ep, req, 0);

			/* ep 0 idle */
			ep0_idle(dev);

			dev->ep0_req.req.length = 0;
			DBG("%s:start to send req\n", __func__);
			u2d_ep0_in_start(dev, &dev->ep0_req);
		}
		break;
	case EP0_STALL:
		INFO("handle_ep0:stall!!");
		break;
	case EP0_IN_FAKE:
		INFO("handle_ep0:fake!");
		break;
	default:
		break;
	}

	if(usb_doepint0 & USB_DIOEPINT_SETUP) {
		/*clear setup int*/
		val = USB_DIOEPINT_SETUP;
		comip_u2d_write_reg32(val, USB_DOEPINT0);
	}
	else if(usb_diepint0 & USB_DIOEPINT_TIMEOUT) {
		/*clear timeout int*/
		val = USB_DIOEPINT_TIMEOUT;
		comip_u2d_write_reg32(val, USB_DIEPINT0);
	}

	/*clear BNA int*/
	if (usb_diepint0 & USB_DIOEPINT_BNA) {
		dma_status = dev->ep0_in_desc->status;
		if ( (dma_status >> STAT_IO_BS_SHIFT)) {
			dev->ep0_in_desc->status &=~  ((1 << 30) | (1<<31));
		}
	}

	if (usb_doepint0 & USB_DIOEPINT_BNA) {
		dma_status = dev->ep0_out_desc->status;
		if ( (dma_status >> STAT_IO_BS_SHIFT)) {
			dev->ep0_out_desc->status &=~  ((1 << 30) | (1<<31));
		}
	}
}

static void handle_ep(struct comip_ep *ep, u32 epintsts)
{
	unsigned int val;
	struct comip_request *req, *req_next;
	struct comip_dma_des *dma_des;
	s32 completed;
	unsigned long dma_status;

	dma_des = (struct comip_dma_des *)ep->dma_desc;

	completed = 0;
	if (!list_empty(&ep->queue)) {
		req = container_of(ep->queue.next, struct comip_request, queue);
	} else {
		req = NULL;
		INFO("req = NULL\n");
	}

	/*clear USB_DIEPINT and  USB_DOEPINT*/
	if (ep->dir_in) {
		val = comip_u2d_read_reg32(USB_DIEPINT(ep->ep_num));
		comip_u2d_write_reg32(val, USB_DIEPINT(ep->ep_num));
	} else {
		val = comip_u2d_read_reg32(USB_DOEPINT(ep->ep_num));
		comip_u2d_write_reg32(val, USB_DOEPINT(ep->ep_num));
	}

	if (epintsts & USB_DIOEPINT_AHBERR) {
		ERR("USB_DIOEPINT_AHBERR epintsts:0x%x\n", epintsts);
		val = USB_DIOEPINT_AHBERR;
		if (ep->dir_in) /* IN */
			comip_u2d_write_reg32(val, USB_DIEPINT(ep->ep_num));
		else
			comip_u2d_write_reg32(val, USB_DOEPINT(ep->ep_num));

		u2d_ep_enable(&ep->ep, ep->ep.desc);
	}

	if (ep->dir_in) {
		if (epintsts & USB_DIOEPINT_TIMEOUT)
			ep_txtimeout(ep);
		if (epintsts & USB_DIOEPINT_INTXFEMP)
			ep_txemp(ep);
		if (epintsts & USB_DIOEPINT_INNAKEFF)
			ep_txnak(ep);
		if (epintsts & USB_DIOEPINT_TXFUNDN)
			ep_txundn(ep);
		if (epintsts & USB_DIOEPINT_TXFEMP)
			ep_txfifoemp(ep);

	} else {
		if (epintsts & USB_DIOEPINT_OUTEPDIS)
			ep_rxepdis(ep);
		if (epintsts & USB_DIOEPINT_OUTPKTERR)
			ep_rxpkterr(ep);
	}

	/*handle DMA error*/
	if (epintsts & USB_DIOEPINT_BNA) {
		dma_status = dma_des->status;
		ERR("USB_DIOEPINT_BNA error! dma_status:0x%lx\n", dma_status);
		if ( (dma_status >> STAT_IO_BS_SHIFT)) {
			dma_des->status &=~  ((1 << 30) | (1<<31));
			INFO("%s USB_DIOEPINT_BNA epintsts:0x%x, dma_des->status:0x%x\n",
				__func__, epintsts, dma_des->status);
		}
	}

	if (epintsts & USB_DIOEPINT_XFCOMPL) {
		if (req) {
			LYSDBG("ep%d-in:%d,%s\n",ep->ep_num,ep->dir_in,((req->queue.next != &ep->queue)?"More Pack":"Last pack"));

			if (req->queue.next != &ep->queue) {/*Next package? */
				req_next = container_of(req->queue.next, struct comip_request, queue);
				if (ep->dir_in) { /* IN */

					u2d_wait_dma_done(ep, dma_des);

					/* clear interrupt */
					val = USB_DIOEPINT_XFCOMPL;
					comip_u2d_write_reg32(val, USB_DIEPINT(ep->ep_num));

					/* ep complete */
					done(ep, req, 0);

					/* start next ep */
					u2d_ep_in_start(ep, req_next, 0);
				} else {	// OUT
					/* receive data */
					u2d_rx_dma_handle(ep, dma_des, req);

					/* clear interrupt */
					val = USB_DIOEPINT_XFCOMPL;//add 1225
					comip_u2d_write_reg32(val, USB_DOEPINT(ep->ep_num));

					/* done request */
					done(ep, req, 0);

					/* start next req */
					u2d_ep_out_start(ep, req_next);					
				}
			} else {	/*Last one! */
				if (ep->dir_in) {	//IN
					u2d_wait_dma_done(ep, dma_des);

					/* clear interrupt */
					val = USB_DIOEPINT_XFCOMPL; //add 1225
					comip_u2d_write_reg32(val, USB_DIEPINT(ep->ep_num));

					if (req->req.zero) {	//handle zlp
						/* start zero package */
						if ((0 == req->req.length % ep->ep.maxpacket)
							&& req->req.length)
							u2d_ep_in_start(ep, req, 1);
						else
							done(ep, req, 0);
						/* reset zero */
						req->req.zero = 0;
					} else {
						/* done request */
						done(ep, req, 0);
					}
				} else {

					/* receive data */
					u2d_rx_dma_handle(ep, dma_des, req);

					/* clear interrupt */
					val = USB_DIOEPINT_XFCOMPL;	//add 1225
					comip_u2d_write_reg32(val, USB_DOEPINT(ep->ep_num));

					/* dont request */
					done(ep, req, 0);
				}
			}
		} else { //no request pending, just clear interrupt
			INFO("%s no request pending , but we got dma int!!,dir:%d,req:0x%p\n", __func__,ep->dir_in,req);
			if (ep->dir_in) {
				val = USB_DIOEPINT_XFCOMPL;
				comip_u2d_write_reg32(val, USB_DIEPINT(ep->ep_num));
			} else {
				val = USB_DIOEPINT_XFCOMPL;
				comip_u2d_write_reg32(val, USB_DOEPINT(ep->ep_num));
			}
		}
	}
	ep->irqs++;
}
static irqreturn_t comip_u2d_irq(int irq, void *dev_id)
{
	struct comip_u2d *dev = &memory;
	unsigned int val;
	u32 usb_gintsts;
	u32 usb_gintmsk;

	///* spin lock */
	spin_lock(dev->lock);

	/* get raw interrupt & mask */
	usb_gintsts = comip_u2d_read_reg32(USB_GINTSTS);
	usb_gintmsk = comip_u2d_read_reg32(USB_GINTMSK);

#ifdef CONFIG_USB_COMIP_OTG
	if (comip_otg_hostmode())
	{ 
		spin_unlock(dev->lock);
		return IRQ_NONE;
	}	
#endif
	/* clear interrupt */
	comip_u2d_write_reg32(usb_gintsts, USB_GINTSTS);

	/* get unmasked interrupt */
	val = (usb_gintsts & usb_gintmsk);

	/* update interrupt count */
	dev->stats.irqs++;

	/*just for debug, print interrupt count*/
	if ((dev->stats.irqs % MAX_IRQ_FENCE) == 0) {	
		INFO("%s:irqs=%d, status=0x%x,msk=0x%x\n", __func__, dev->stats.irqs, usb_gintsts, usb_gintmsk);
	}

	/* SUSpend Interrupt Request */
	if (usb_gintsts & USB_GINTSTS_USBSUSP) {
		INFO("%s Suspend!%x\n",__func__, usb_gintsts);
		/* clear interrupt */
		val = USB_GINTSTS_USBSUSP;
		comip_u2d_write_reg32(val, USB_GINTSTS);
	}

	if (usb_gintsts & USB_GINTSTS_ERLYSUSP) {
		INFO("%s EarlySuspend!%x\n",__func__, usb_gintsts);
		/* clear interrupt */
		val = USB_GINTSTS_ERLYSUSP;
		comip_u2d_write_reg32(val, USB_GINTSTS);
	}

	/* RESume Interrupt Request */
	if (usb_gintsts & USB_GINTSTS_WKUPINT) {
		INFO("%s Resume!%x\n",__func__, usb_gintsts);
		/* clear interrupt */
		val = USB_GINTSTS_WKUPINT;
		comip_u2d_write_reg32(val, USB_GINTSTS);
	}

	/* ReSeT Interrupt Request - USB reset */
	if (usb_gintsts & USB_GINTSTS_USBRST) {
		INFO("%s Reset!%x\n",__func__, usb_gintsts);
		dev->gadget.speed = USB_SPEED_UNKNOWN;
		memset(&dev->stats, 0, sizeof(dev->stats));
		u2d_reinit(dev);

		/* set device address to zero */
		u2d_setaddress(0);

		/* reset controller */
		usb_reset();

		/* clear interrupt */
		val = USB_GINTSTS_USBRST;
		comip_u2d_write_reg32(val, USB_GINTSTS);
	}

	/*to get enum speed */
	if (usb_gintsts & USB_GINTSTS_ENUMDONE) {//speed enum completed
		INFO("%s Enumdone!%x\n",__func__, usb_gintsts);
		usb_enum_done();
		/* change the endpoint MPS */
		u2d_change_mps(dev->gadget.speed);

		/* clear interrupt */
		val = USB_GINTSTS_ENUMDONE;
		comip_u2d_write_reg32(val, USB_GINTSTS);

		INFO("notify pmic\n");
		if (dev->pmic_notify)
			dev->pmic_notify();

		if(dev->linkstat != LCUSB_EVENT_VBUS){
			dev->linkstat = LCUSB_EVENT_VBUS;
            plugin_stat = LCUSB_EVENT_VBUS;
			pr_info("PC detected\n");
            comip_usb_xceiv_notify(LCUSB_EVENT_VBUS);
		}
	}

	/* SOF/uSOF Interrupt */
	if (usb_gintsts & USB_GINTSTS_SOF) {
//		INFO("%s sof!%x",__func__, usb_gintsts);
		/* clear SOF/uSOF interrupt */
		val = USB_GINTSTS_SOF;
		comip_u2d_write_reg32(val, USB_GINTSTS);
	}

	/* handle endpoint interrupt */
	if ((usb_gintsts & USB_GINTSTS_OEPINT) || (usb_gintsts & USB_GINTSTS_IEPINT)) {		
		u32 usb_daint;
		u32 usb_diepint = 0;
		u32 usb_doepint = 0;
		u16 temp;
		s32 i;

		/* get the interrupted endpoint */
		usb_daint = comip_u2d_read_reg32(USB_DAINT);	//USB_DAINT;

		/* ep0 interrupt */
		if (usb_daint & 0x10001) {
			dev->ep[0].irqs++;
			handle_ep0(dev);
		}

		//spin_lock(dev->lock);

		/* ep1--ep7 IN endpoints */
		temp = usb_daint & 0xfe;
		if (temp) {	
			for (i = 1; i <= U2D_EP_NUM / 2; i++) {
				if (temp & (1 << i)) {
					/* get unmasked IN interrupt */
					usb_diepint = (comip_u2d_read_reg32(USB_DIEPINT(i)) & \
								   	comip_u2d_read_reg32(USB_DIEPMSK));
					//spin_lock(dev->lock);
					handle_ep(&dev->ep[i], usb_diepint);
					//spin_unlock(dev->lock);
				}
			}
		}

		/* ep8--ep14 OUT endpoints */
		temp = (usb_daint & 0x7f000000) >> 16;
		if (temp) {
			for (i = U2D_EP_NUM / 2 + 1; i < U2D_EP_NUM; i++) {
				if (temp & (1 << i)) {
					usb_doepint = (comip_u2d_read_reg32(USB_DOEPINT(i)) & \
									comip_u2d_read_reg32(USB_DOEPMSK));				
					//spin_lock(dev->lock);
					handle_ep(&dev->ep[i], usb_doepint);
					//spin_unlock(dev->lock);
				}
			}
		}
		//spin_unlock(dev->lock);
	}

	spin_unlock(dev->lock);
	return IRQ_HANDLED;
}

static s32 comip_ep_enable(struct usb_ep *_ep,
			   const struct usb_endpoint_descriptor *desc)
{
	unsigned long flags;
	struct comip_ep  *ep;
	struct comip_u2d *dev;
	s32 ret;

	ep = container_of(_ep, struct comip_ep, ep);

	if (!_ep || !desc || _ep->name == ep0name
	    || desc->bDescriptorType != USB_DT_ENDPOINT
	    || ep->fifo_size < (desc->wMaxPacketSize)) {
	    ERR("%s, invalid param\n", __func__);
		return -EINVAL;
	}

	dev = ep->dev;
	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN) {
		ERR("%s: driver=0x%p, speed=0x%x\n", __func__, dev->driver, dev->gadget.speed);
		return -ESHUTDOWN;
	}

	
	spin_lock_irqsave(ep->lock, flags);
	ret = u2d_ep_enable(_ep, desc);
	spin_unlock_irqrestore(ep->lock, flags);

	return ret;
}

static s32 comip_ep_disable(struct usb_ep *_ep)
{
	unsigned long flags;
	unsigned int val;
	struct comip_ep *ep;	

	ep = container_of(_ep, struct comip_ep, ep);

	if (!_ep || !ep->ep.desc)
		return -EINVAL;

	spin_lock_irqsave(ep->lock, flags);

	/* nuke the endpoint */
	u2d_nuke(ep, -ESHUTDOWN);

	/* fifo flush */
	u2d_ep_flush(ep->ep_num, ep->dir_in);	/*only for TX */

	if (ep->dir_in) {
		/* IN */
		val = comip_u2d_read_reg32(USB_DIEPCTL(ep->ep_num));
		val |= USB_DIOEPCTL_SNAK;	/*Set NAK */
		val &= ~(USB_DIOEPCTL_ACTEP | USB_DIOEPCTL_EPEN);/*Disable EP */
		comip_u2d_write_reg32(val, USB_DIEPCTL(ep->ep_num));
	} else {
		 /*OUT*/ val = comip_u2d_read_reg32(USB_DOEPCTL(ep->ep_num));
		val |= USB_DIOEPCTL_SNAK;	/*Set NAK */
		val &= ~(USB_DIOEPCTL_ACTEP | USB_DIOEPCTL_EPEN);/*Disable EP */
		comip_u2d_write_reg32(val, USB_DOEPCTL(ep->ep_num));
	}
	ep->ep.desc = 0;
	ep->stopped = 1;

	spin_unlock_irqrestore(ep->lock, flags);

	INFO("%s:: ep%x disable!\n",  __func__, ep->ep_num);
	
	return 0;
}

static struct usb_request *comip_ep_alloc_request(struct usb_ep *_ep,
						  unsigned gfp_flags)
{
	struct comip_request *req;

	if (_ep == NULL) {
		ERR("%s _ep is null\n", __func__);
		return NULL;
	}
	req = kzalloc(sizeof(struct comip_request), gfp_flags);
	if (!req)
		return NULL;

	INIT_LIST_HEAD(&req->queue);

	return &req->req;
}

static void comip_ep_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	unsigned long flags;
	struct comip_ep  *ep  		= container_of(_ep,  struct comip_ep, ep);	
	struct comip_request *req 	= container_of(_req, struct comip_request, req);


	if (_ep == NULL || _req == NULL) {
		ERR("%s, _ep: %x, _req:%x\n", __func__, (unsigned int)_ep, (unsigned int)_req);
		return;
	} else if (!list_empty(&req->queue)) {
		ERR("%s, req queue is not empty\n", __func__);
		return;
	}

	spin_lock_irqsave(ep->lock, flags);
	if (req)
		kfree((void *)req);
	spin_unlock_irqrestore(ep->lock, flags);
}

static s32
comip_ep_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags)
{
	unsigned long flags;
	struct comip_ep *ep 		= container_of(_ep, struct comip_ep, ep);
	struct comip_request *req 	= container_of(_req, struct comip_request, req);
	struct comip_u2d *dev;
	s32 ret;

	if (!_ep || !_req || !_req->complete || !_req->buf || !list_empty(&req->queue)) {
		ERR("comip_ep_queue:Initialze didn't finish yet %x:%x:%x:%x:%x!",
			(u32)_ep,(u32)_req,(u32)_req->complete,(u32)_req->buf,list_empty(&req->queue));
		return -EINVAL;
	}

	if (_req->length > U2D_DMA_BUF_SIZE) {
		ERR("%s:length is too larger 0x%x\n", __func__, _req->length);
		return -EINVAL;
	}
	
	dev = ep->dev;
	if (!dev->driver || \
		((dev->ep0state != EP0_IN_FAKE) && (dev->gadget.speed == USB_SPEED_UNKNOWN))) {
		ERR("%s:Enum not finished yet!, ep0state=0x%x, speed=0x%x\n", \
				__func__, dev->ep0state, dev->gadget.speed );
		return -ESHUTDOWN;
	}

	spin_lock_irqsave(ep->lock, flags);
	usb_ep_int_mask(ep->ep_num, 1);
	ret = u2d_ep_queue(_ep, _req, gfp_flags);
	usb_ep_int_mask(ep->ep_num, 0);
	spin_unlock_irqrestore(ep->lock, flags);

	return ret;
}

static s32 comip_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	unsigned long flags;
	struct comip_ep *ep			= container_of(_ep, struct comip_ep, ep);
	struct comip_request *req	= container_of(_req, struct comip_request, req);

	INFO("%s\n", __func__);


	if (!_ep || !_req || list_empty(&ep->queue) || list_empty(&req->queue)) {
		ERR("comip_ep_dequeue:return ep:%d %x:%x:%x:%x!!", ep->ep_num,
			(u32)_ep,(u32)_req,list_empty(&ep->queue),list_empty(&req->queue));
		return -EINVAL;
	}

	spin_lock_irqsave(ep->lock, flags);
	
	/* make sure it's actually queued on this endpoint */
	list_for_each_entry(req, &ep->queue, queue) {
		if (&req->req == _req) {
			break;
		}
	}		 
	if (&req->req != _req) {
		ERR("comip_ep_dequeue:error req!!");
		spin_unlock_irqrestore(ep->lock, flags);
		return -EINVAL;
	}

	/* if it's the first req on ep queue, we should cancel the dma*/
	if ((ep->queue.next == &req->queue) && !ep->stopped) {
		/* cancel dma first*/
		cancel_dma(ep);

		/* request done */
		done(ep, req, -ECONNRESET);

		/* if there is another req on the ep, restart i/o */
		if (!list_empty(&ep->queue)) {
			req = container_of(ep->queue.next, struct comip_request, queue);
			if (ep->dir_in) {
				u2d_ep_in_start(ep, req, 0);
			} else {	//OUT
				u2d_ep_out_start(ep, req);
			}
		}
	} else { //not the first req on the ep queue, just remove it
		done(ep, req, -ECONNRESET);
	}

	spin_unlock_irqrestore(ep->lock, flags);
	
	return 0;
}

static s32 comip_ep_set_halt(struct usb_ep *_ep, s32 value)
{
	unsigned long flags;
	unsigned int val;
	struct comip_ep *ep;

	ep = container_of(_ep, struct comip_ep, ep);
	if (!_ep || (!ep->ep.desc && ep->ep.name != ep0name)
	    || ep->ep_type == USB_ENDPOINT_XFER_ISOC) {
		return -EINVAL;
	}

	if (!list_empty(&ep->queue) && ep->dir_in) {
		return (-EAGAIN);
	}

	INFO("ep%d set halt\n", ep->ep_num);
	spin_lock_irqsave(ep->lock, flags);
	/* ep0 needs special care, may not necessary for U2D */
	if (!ep->ep.desc)
		ep->dev->ep0state = EP0_STALL;

	/* and bulk/intr endpoints like dropping stalls too */
	else {
		/* flush endpoint FIFO & force STALL */
		val = comip_u2d_read_reg32(USB_DIEPCTL(ep->ep_num));
		val |= USB_DIOEPCTL_STALL;
		comip_u2d_write_reg32(val, USB_DIEPCTL(ep->ep_num));
	}
	spin_unlock_irqrestore(ep->lock, flags);
	return 0;
}

static int comip_ep_fifo_status(struct usb_ep *_ep)
{
	return 0;
}

static void comip_ep_fifo_flush(struct usb_ep *_ep)
{
	//unsigned long flags;
	struct comip_ep *ep = container_of(_ep, struct comip_ep, ep);
	LYSINFO("fifo_flush:ep%d,in:%d\n",ep->ep_num, ep->dir_in);
	//spin_lock_irqsave(ep->lock, flags);
	//u2d_ep_flush(ep->ep_num, ep->dir_in);
	//spin_unlock_irqrestore(ep->lock, flags);
}


static struct usb_ep_ops comip_ep_ops = {
	.enable = comip_ep_enable,
	.disable = comip_ep_disable,
	.alloc_request = comip_ep_alloc_request,
	.free_request = comip_ep_free_request,
	.queue = comip_ep_queue,
	.dequeue = comip_ep_dequeue,
	.set_halt = comip_ep_set_halt,
	.fifo_status = comip_ep_fifo_status,
	.fifo_flush = comip_ep_fifo_flush,
};



struct usb_ep *comip_ep_config(struct usb_gadget *gadget,
			       struct usb_endpoint_descriptor *desc)
{
	struct comip_u2d *dev = &memory;
	unsigned long flags;
	unsigned i;
	u8 dir_in;
	struct usb_ep *ep = NULL;
	struct comip_ep *comip_ep = NULL;	
	int have_assigned = 0;

	spin_lock_irqsave(dev->lock, flags);

	dir_in = (desc->bEndpointAddress & USB_DIR_IN) ? 1 : 0;

	/* find empty endpoint */
	if (dir_in == 1) {	/*choose from IN endpoints */
		for (i = 1; i <= U2D_EP_NUM / 2; i++) {
			if (!dev->ep[i].assigned) {
				comip_ep = &dev->ep[i];
				comip_ep->assigned = 1;
				comip_ep->ep_num = i;
				comip_ep->dir_in = dir_in;
				desc->bEndpointAddress = USB_DIR_IN | i;
				have_assigned = 1;
				INFO("%s: ep in assigned :%d\n",__func__, i);
				break;
			}
		}
	} else {		//choose from OUT endpoints
		for (i = U2D_EP_NUM / 2 + 1; i < U2D_EP_NUM; i++) {
			if (!dev->ep[i].assigned) {
				comip_ep = &dev->ep[i];
				comip_ep->assigned = 1;
				comip_ep->ep_num = i;
				comip_ep->dir_in = dir_in;
				desc->bEndpointAddress = USB_DIR_OUT | i;
				have_assigned = 1;
				INFO("%s: ep out assigned :%d\n", __func__, i);
				break;
			}
		}
	}

	if ( !have_assigned) {
		spin_unlock_irqrestore(dev->lock, flags);
		ERR("comip_ep_config:find no ep!!\n");
		return NULL;
	}	
	
	comip_ep->dev = dev;
	comip_ep->ep.desc = desc;
	comip_ep->irqs = 0;
	comip_ep->ep_type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	comip_ep->stopped = 1;

	ep = &comip_ep->ep;
	ep->name = comip_ep->name;
	
	switch (comip_ep->ep_type) {
	case USB_ENDPOINT_XFER_BULK:
		comip_ep->fifo_size = BULK_FIFO_SIZE;
		comip_ep->ep.maxpacket = BULK_MPS(dev->gadget.speed);
		break;
	case USB_ENDPOINT_XFER_INT:
		comip_ep->fifo_size = INT_FIFO_SIZE;
		comip_ep->ep.maxpacket = INT_MPS(dev->gadget.speed);
		break;
	case USB_ENDPOINT_XFER_ISOC:
		comip_ep->fifo_size = ISO_FIFO_SIZE;
		comip_ep->ep.maxpacket = ISO_MPS(dev->gadget.speed);
		break;
	default:
		break;
	}

	if (!(desc->wMaxPacketSize))
		desc->wMaxPacketSize = comip_ep->ep.maxpacket;
	
	ep->ops = &comip_ep_ops;
	ep->maxpacket = MIN(comip_ep->fifo_size, desc->wMaxPacketSize);

	list_add_tail(&ep->ep_list, &gadget->ep_list);

	spin_unlock_irqrestore(dev->lock, flags);
	return ep;
}

EXPORT_SYMBOL(comip_ep_config);

static int comip_u2d_start(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{	
	struct comip_u2d *dev = &memory;
	unsigned long flags;
	s32 retval;
	int i;

	INFO("%s\n", __func__);

	spin_lock_irqsave(dev->lock, flags);

	/* re-init driver-visible data structures */
	u2d_reinit(dev);

	spin_unlock_irqrestore(dev->lock, flags);
	/* dma desc alloc */
	dev->dma_desc_pool = dma_pool_create("comip_u2d", &dev->gadget.dev,
				       sizeof(struct comip_dma_des),
				       64ul, 4096ul);
	if (dev->dma_desc_pool == NULL) {
		spin_lock_irqsave(dev->lock, flags);
		ERR("%s, dma_pool_create fail\n", __func__);
		retval = -ENOMEM;
		goto err;
	}

	/* endpoint 0 dma desc alloc */
	dev->ep0_in_desc =  dma_pool_alloc(dev->dma_desc_pool, GFP_KERNEL,
					   &dev->ep0_in_desc_dma_addr);
	dev->ep0_out_desc =  dma_pool_alloc(dev->dma_desc_pool, GFP_KERNEL,
					   &dev->ep0_out_desc_dma_addr);
	spin_lock_irqsave(dev->lock, flags);
	if (!dev->ep0_in_desc || !dev->ep0_out_desc) {
		ERR("%s cannot get ep0 dma desc \n", __func__);
		retval = -ENOMEM;
		goto err;
	}
	memset(dev->ep0_in_desc, 0, sizeof(struct comip_dma_des));
	memset(dev->ep0_out_desc, 0, sizeof(struct comip_dma_des));
	
	/* init ep0 control request queueand buffer */
	memset((void *)&dev->ep0_req, 0, sizeof(dev->ep0_req));
	INIT_LIST_HEAD(&dev->ep0_req.queue);	
	dev->ep0_req.req.complete = u2d_setup_complete;	
	spin_unlock_irqrestore(dev->lock, flags);
	dev->ep0_req.req.buf = dma_alloc_coherent(NULL, REQ_BUFSIZ,
				&dev->ep0_req.req.dma, GFP_KERNEL);

	if (!dev->ep0_req.req.buf) {
		spin_lock_irqsave(dev->lock, flags);
		ERR("%s: alloc ep0 req buf failed\n", __func__);
		goto err;
	}

	for ( i = 1; i < U2D_EP_NUM; i++) {
		/* alloc dma desc */
		dev->ep[i].dma_desc = dma_pool_alloc(dev->dma_desc_pool, GFP_KERNEL,
						   &dev->ep[i].dma_desc_dma_addr);
		if (!dev->ep[i].dma_desc) {
			spin_lock_irqsave(dev->lock, flags);
			ERR("%s: alloc comip_dma_des faild\n", __func__);
			goto err;
		}
	}
	spin_lock_irqsave(dev->lock, flags);

	/* first hook up the driver ... */
	driver->driver.bus     = NULL;
	dev->driver = driver;

	spin_unlock_irqrestore(dev->lock, flags);
#ifdef CONFIG_USB_COMIP_OTG
	retval = otg_set_peripheral(dev->phy->otg, &dev->gadget);
	if (retval < 0) {
			printk(KERN_ERR"can't bind to transceiver\n");
			driver->unbind(&dev->gadget);
			dev->gadget.dev.driver = 0;
			dev->driver = 0;
			return retval;
	}
#endif 
	INFO("%s done\n", __func__);	
	return 0;

err:
	if (dev->ep0_req.req.buf) {
		spin_unlock_irqrestore(dev->lock, flags);
		dma_free_coherent(NULL, REQ_BUFSIZ, dev->ep0_req.req.buf,
				dev->ep0_req.req.dma);
		spin_lock_irqsave(dev->lock, flags);
		dev->ep0_req.req.buf = NULL;
		dev->ep0_req.req.dma = 0;
	}
	if (dev->ep0_in_desc) {
	  dma_pool_free(dev->dma_desc_pool, dev->ep0_in_desc,\
						  dev->ep0_in_desc_dma_addr); 
	  dev->ep0_in_desc = NULL;
	  dev->ep0_in_desc_dma_addr = 0;
	}
	if (dev->ep0_out_desc) {
	  dma_pool_free(dev->dma_desc_pool, dev->ep0_out_desc,\
						  dev->ep0_out_desc_dma_addr);	  
	  dev->ep0_out_desc = NULL;
	  dev->ep0_out_desc_dma_addr = 0;
	}

	/* dma desc destroy */
	for ( i = 1; i < U2D_EP_NUM; i++) {
		if (dev->ep[i].dma_desc) {
			dma_pool_free(dev->dma_desc_pool, dev->ep[i].dma_desc,\
						dev->ep[i].dma_desc_dma_addr);
			dev->ep[i].dma_desc = NULL;
			dev->ep[i].dma_desc_dma_addr = 0;
		}
	}

	if (dev->dma_desc_pool) {
		spin_unlock_irqrestore(dev->lock, flags);
		dma_pool_destroy(dev->dma_desc_pool);
		spin_lock_irqsave(dev->lock, flags);
	}
	dev->dma_desc_pool = NULL;
	
	dev->driver = 0;

	spin_unlock_irqrestore(dev->lock, flags);
	return -1;
}

static s32 comip_u2d_stop(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{
	struct comip_u2d *dev = &memory;
	unsigned long flags;
	s32 i;	

	INFO("%s\n", __func__);

	spin_lock_irqsave(dev->lock, flags);

	/* hw disable */
#ifdef COMIP_U2D_EMU_TIMEOUT_WDT
	u2d_emu_wdclr();
#endif	
	u2d_soft_dis(1);
	u2d_disable(dev);
	u2d_stop_activity(dev);
	
	/* uninit dev driver */
	dev->driver = NULL;
	
	/* endpoint uninit */
	for (i = 1; i < U2D_EP_NUM; i++) {
		struct comip_ep *comip_ep = &dev->ep[i];
		
		comip_ep->assigned = 0;
		comip_ep->ep.desc = NULL;
	}

	/* free endpoint0 request buf */
	if (dev->ep0_req.req.buf) {
		spin_unlock_irqrestore(dev->lock, flags);
		dma_free_coherent(NULL, REQ_BUFSIZ, dev->ep0_req.req.buf,
				dev->ep0_req.req.dma);
		spin_lock_irqsave(dev->lock, flags);
		dev->ep0_req.req.buf = NULL;
		dev->ep0_req.req.dma = 0;
	}
	
	/* dma desc destroy */
	for ( i = 1; i < U2D_EP_NUM; i++) {
		if (dev->ep[i].dma_desc) {
			dma_pool_free(dev->dma_desc_pool, dev->ep[i].dma_desc,\
						dev->ep[i].dma_desc_dma_addr);
			dev->ep[i].dma_desc = NULL;
			dev->ep[i].dma_desc_dma_addr = 0;
		}
	}
	if (dev->ep0_in_desc) {
	  dma_pool_free(dev->dma_desc_pool, dev->ep0_in_desc,\
						  dev->ep0_in_desc_dma_addr);
	  dev->ep0_in_desc = NULL;
	  dev->ep0_in_desc_dma_addr = 0;
	}
	if (dev->ep0_out_desc) {
	  dma_pool_free(dev->dma_desc_pool, dev->ep0_out_desc,\
						  dev->ep0_out_desc_dma_addr);
	  dev->ep0_out_desc = NULL;
	  dev->ep0_out_desc_dma_addr = 0;
	}

	if (dev->dma_desc_pool) {
		spin_unlock_irqrestore(dev->lock, flags);
		dma_pool_destroy(dev->dma_desc_pool);
		spin_lock_irqsave(dev->lock, flags);
	}
	dev->dma_desc_pool = NULL;

	spin_unlock_irqrestore(dev->lock, flags);

	INFO("%s done\n", __func__);
	return 0;
}

static int comip_u2d_get_frame(struct usb_gadget *_gadget)
{
	INFO("%s\n", __func__);
	return 0;
}

static int comip_u2d_wakeup(struct usb_gadget *_gadget)
{
	INFO("%s\n", __func__);
	return 0;
}

static int comip_u2d_vbus_session(struct usb_gadget *_gadget, int is_active)
{
	INFO("%s\n", __func__);
	return 0;
}
extern bool android_have_functions(void);
static int comip_u2d_pullup(struct usb_gadget *gadget, int is_on)
{
	struct comip_u2d *dev = &memory;
	int ret;

	INFO("%s, ##is_on=0x%x\n", __func__, is_on);

	disable_irq(INT_USB_OTG);
	spin_lock(dev->lock);

	if (!is_on) {
		if(dev->insert_flag != 0){
			INFO("%s, pullup = 0\n", __func__);
#ifdef COMIP_U2D_EMU_TIMEOUT_WDT
			u2d_emu_wdclr();
#endif
			if (dev->driver && dev->driver->disconnect) {
				spin_unlock(dev->lock);
				dev->driver->disconnect(&dev->gadget);
				spin_lock(dev->lock);
			}

			u2d_soft_dis(1);
			u2d_disable(dev);
			dev->insert_flag = 0;
		}
	} else {
		if(android_have_functions()&&(dev->insert_flag == 0)){
			INFO("%s, pullup = 1\n", __func__);
			u2d_reinit(dev);
			ret = u2d_usb_init();
			if (ret != 0) {
				u2d_soft_dis(1);
				u2d_disable(dev);
				spin_unlock(dev->lock);
				enable_irq(INT_USB_OTG);
				ERR("%s:usb not init\n", __func__);

				return -EBUSY;
			}
			u2d_enable(dev);
#ifdef COMIP_U2D_EMU_TIMEOUT_WDT
			u2d_emu_wdset();
#endif
			dev->insert_flag = 1;
		}
	}

	spin_unlock(dev->lock);
	enable_irq(INT_USB_OTG);

	return 0;
}

static int comip_u2d_reconfig_pullup(struct usb_gadget *gadget, int is_on)
{
	struct comip_u2d *dev = &memory;
	int i;
	int ret;

	INFO("%s, ##is_on=0x%x\n", __func__, is_on);
#ifdef CONFIG_USB_COMIP_OTG
	if (comip_otg_hostmode())
	{
		return 0;
	}
#endif
	disable_irq(INT_USB_OTG);
	spin_lock(dev->lock);

	if (!is_on) {
		if(dev->insert_flag != 0){
			INFO("%s, pullup = 0\n", __func__);
#ifdef COMIP_U2D_EMU_TIMEOUT_WDT
			u2d_emu_wdclr();
#endif
			if (dev->driver && dev->driver->disconnect) {
				spin_unlock(dev->lock);
				dev->driver->disconnect(&dev->gadget);
				spin_lock(dev->lock);
			}

			u2d_soft_dis(1);
			u2d_disable(dev);
			dev->insert_flag = 0;
		}
		for (i = 1; i < U2D_EP_NUM; i++) {
			struct comip_ep *comip_ep = &dev->ep[i];
			comip_ep->assigned = 0;
		}
	} else {
		if(dev->insert_flag != 1){
			INFO("%s, pullup = 1\n", __func__);
			u2d_reinit(dev);
			ret = u2d_usb_init();
			if (ret != 0) {
				u2d_soft_dis(1);
				u2d_disable(dev);
				spin_unlock(dev->lock);
				enable_irq(INT_USB_OTG);
				ERR("%s:usb not init\n", __func__);

				return -EBUSY;
			}
			u2d_enable(dev);
#ifdef COMIP_U2D_EMU_TIMEOUT_WDT
			u2d_emu_wdset();
#endif
			dev->insert_flag = 1;
		}
	}

	spin_unlock(dev->lock);
	enable_irq(INT_USB_OTG);

	return 0;
}

/* battery driver will call this func when charger status changed */
static void comip_usb_handle_plug(int event, void *puser, void* pargs)
{
	struct comip_u2d *dev = &memory;
	struct pmic_usb_event_parm *usb_event_parm =
		(struct pmic_usb_event_parm *)pargs;
	int plugged = usb_event_parm->plug;

	dev->pmic_notify = usb_event_parm->confirm;

	INFO("%s, plugged=%d\n", __func__, plugged);
	#ifdef CONFIG_USB_COMIP_OTG
	if (comip_otg_hostmode())
	{
		INFO("%s, no usb device!\n", __func__);
		return;
	}
	#endif
	if (plugged) {
		comip_usb_power_set(1);
		msleep(2000);
		comip_u2d_pullup(NULL, plugged);
	} else {
		comip_u2d_pullup(NULL, plugged);
		comip_usb_power_set(0);
	}
}

static void comip_u2d_release(struct device *dev)
{
	INFO("%s\n", __func__);
	return;
}

static const struct usb_gadget_ops comip_u2d_ops = {
	.get_frame = comip_u2d_get_frame,
	.wakeup = comip_u2d_wakeup,
	/* current versions must always be self-powered */
	.vbus_session = comip_u2d_vbus_session,
	.pullup = comip_u2d_reconfig_pullup,
	.udc_start = comip_u2d_start,
	.udc_stop = comip_u2d_stop,
};

static struct comip_u2d memory = {
	.gadget = {
		   .ops = &comip_u2d_ops,
		   .ep0 = &memory.ep[0].ep,
		   .name = driver_name,
		   .dev = {
			   .init_name = "gadget",
			   .release = comip_u2d_release,
			   },
		   },

	/* control endpoint */
	.ep[0] = {
		  .ep = {
			 .name = ep0name,
			 .ops = &comip_ep_ops,
			 .maxpacket = EP0_FIFO_SIZE,
			 },
		  .dev = &memory,
		  .ep_num = 0,
		  .ep_type = USB_ENDPOINT_XFER_CONTROL,
		  .fifo_size = EP0_FIFO_SIZE,
	},
};

int comip_u2d_charger_detect(struct comip_u2d *dev)
{
    unsigned long timeout;
    int charger = 0;
    int chargertype = 0;

    timeout = jiffies + msecs_to_jiffies(10000);
    do {
        chargertype = dev->linkstat;//check usb enumerared done?
        if (chargertype == LCUSB_EVENT_VBUS)
            break;
        if(!dev->vchgstat)
            return 0;
        msleep_interruptible(50);
    } while (!time_after(jiffies, timeout));

    if(chargertype == LCUSB_EVENT_VBUS){
        charger = LCUSB_EVENT_VBUS;
        //pr_info("PC detected\n");
    }else{
        charger = LCUSB_EVENT_CHARGER;
        pr_info("Charger detected\n");
    }
    return charger;
}

static void comip_charger_work(struct work_struct *work)
{
    struct comip_u2d *dev = container_of(work, struct comip_u2d, charger_work);
    enum lcusb_xceiv_events status = 0;
    int charger_type = 0;
    unsigned long delaytime;

    if(dev->vchgstat){
        dev_info(dev->dev, "vbus insert\n");
        plugin_stat = LCUSB_EVENT_PLUGIN;
        comip_usb_xceiv_notify(LCUSB_EVENT_PLUGIN);
        delaytime = jiffies + msecs_to_jiffies(2000);
        comip_usb_power_set(1);
        do{
            if(!dev->vchgstat)
            goto exit;
            msleep_interruptible(250);
        }while(!time_after(jiffies, delaytime));
        comip_u2d_pullup(NULL, 1);

        charger_type = comip_u2d_charger_detect(dev);
        if(charger_type == LCUSB_EVENT_VBUS){
            status = LCUSB_EVENT_VBUS;
            plugin_stat = LCUSB_EVENT_VBUS;
            dev->lcusb.last_event = status;
        }else if(charger_type == LCUSB_EVENT_CHARGER){
            status = LCUSB_EVENT_CHARGER;
            plugin_stat = LCUSB_EVENT_CHARGER;
            dev->lcusb.last_event = status;
            dev->linkstat = status;
            comip_usb_xceiv_notify(status);
        }else{
            dev_err(dev->dev, "unkown event\n");
            dev->linkstat = 0;
            dev->vchgstat = 0;
            status = 0;
            goto exit;
        }
    }else{
        status = LCUSB_EVENT_NONE;
        plugin_stat = LCUSB_EVENT_NONE;
        dev->linkstat = status;
        dev->lcusb.last_event = status;
        dev_info(dev->dev, "vbus removed!\n");
        comip_usb_xceiv_notify(status);

        comip_u2d_pullup(NULL, 0);
        comip_usb_power_set(0);
    }
exit:
    return;
}

static void comip_charger_irq(int event, void *puser, void* pargs)
{
    struct comip_u2d *dev = &memory;

    if (comip_otg_hostmode()) {
        dev_info(dev->dev, "otg intr\n");
        return;
    }
    /*get pmic vchg input state*/
    dev->vchgstat = pmic_vchg_input_state_get();
    if(dev->vchgstat < 0){
        dev->vchgstat = 0;
        goto exit;
    }
    schedule_work(&dev->charger_work);
exit:
    return;
}

enum lcusb_xceiv_events comip_get_plugin_device_status(void)
{
	return plugin_stat;
}
EXPORT_SYMBOL_GPL(comip_get_plugin_device_status);

static int __init comip_u2d_probe(struct platform_device *pdev)
{
	//unsigned int val;
	struct comip_u2d *dev = &memory;
	struct device *_dev = &pdev->dev;
	s32 retval;
	int i;

	INFO("%s\n", __func__);

	dev->lock = &u2d_lock;
	dev->dev = _dev;
	platform_set_drvdata(pdev, dev);

	dev->gadget.max_speed = USB_SPEED_HIGH;
	dev->gadget.speed        = USB_SPEED_UNKNOWN;
	dev->gadget.is_otg       = 0;
	INIT_LIST_HEAD(&dev->gadget.ep_list);
	INIT_LIST_HEAD(&dev->gadget.ep0->ep_list);

	dev_set_name(&dev->gadget.dev, "gadget");
	dev->gadget.dev.dma_mask = _dev->dma_mask;
	dev->gadget.dev.coherent_dma_mask = _dev->coherent_dma_mask;
	dev->gadget.dev.parent = _dev;
	dev->gadget.dev.release = _dev->release;
	dev->lcusb.dev = &pdev->dev;
	lcusb_set_transceiver(&dev->lcusb);
	plugin_stat = LCUSB_EVENT_NONE;
	dev->linkstat = 0;
	dev->vchgstat = 0;

#ifdef CONFIG_USB_COMIP_OTG
	/* Memory and interrupt resources will be passed from OTG */
	dev->phy = usb_get_transceiver();
	if (!dev->phy) {
		printk(KERN_ERR "Can't find OTG driver!\n");
		return  -ENODEV;
	}
	dev->gadget.is_otg = 1;
#endif
	/* init endpoint struct */
	for (i = 0; i < U2D_EP_NUM; i++) {
		struct comip_ep *comip_ep = &dev->ep[i];

		if (likely(i != 0)) {
			memset(comip_ep, 0, sizeof(struct comip_ep));
			if (i<=U2D_EP_NUM/2) //in ep
				scnprintf(comip_ep->name, sizeof(comip_ep->name), "ep%i%s", i, "in");
			else if (i>U2D_EP_NUM/2 && i< U2D_EP_NUM) //out ep
				scnprintf(comip_ep->name, sizeof(comip_ep->name), "ep%i%s", i, "out");
		}
		comip_ep->ep_num = i;

		INIT_LIST_HEAD(&comip_ep->queue);
		comip_ep->lock = dev->lock;
	}

	/* dma desc init */
	dev->dma_desc_pool = NULL;
	dev->ep0_in_desc = NULL;
	dev->ep0_in_desc_dma_addr = 0;
	dev->ep0_out_desc = NULL;
	dev->ep0_out_desc_dma_addr = 0;

	/* ep0 idle */
	ep0_idle(dev);

	/*get clk*/
	dev->clk = clk_get(dev->dev, "usbotg_12m_clk");
	if (IS_ERR(dev->clk)) {
		INFO( "cannot get clock usbotg_12m_clk\n");
		retval = -EBUSY;
		goto clkreqfail;
	}
	//u2d_clk_set(1);

	/* request irq */
#ifdef CONFIG_USB_COMIP_OTG
	retval = request_irq(INT_USB_OTG, comip_u2d_irq, IRQF_SHARED|IRQF_DISABLED, driver_name, dev);
#else
	retval = request_irq(INT_USB_OTG, comip_u2d_irq, IRQF_DISABLED, driver_name, dev);
#endif
	if (retval != 0) {
		ERR("usb_init:register irq fail!!");
		retval = -EBUSY;
		goto irqreqfail;
	}
	/*val = comip_u2d_read_reg32(ICTL1_IRQ_INTEN);
	val |= 1 << 2;
	comip_u2d_write_reg32(val, ICTL1_IRQ_INTEN);
	val = comip_u2d_read_reg32(ICTL1_IRQ_INTMASK);
	val |= 1 << 2;
	comip_u2d_write_reg32(val, ICTL1_IRQ_INTMASK);*/
	dev->got_irq = 1;

	/* handle insert flag */
	dev->insert_flag = 0;

	retval = pmic_callback_register(PMIC_EVENT_USB, dev, comip_usb_handle_plug);
	if (retval) {
		ERR("%s: pmic_callback_register failed\n", __func__);
		goto err_register_pmic_callback;
	}

	retval = usb_add_gadget_udc(&pdev->dev, &dev->gadget);
	if (retval) {
		ERR("%s: usb_add_gadget_udc failed\n", __func__);
		goto err_add_udc;
	}

	comip_u2d_create_proc_file();

    INIT_WORK(&dev->charger_work, comip_charger_work);
	retval = pmic_callback_register(PMIC_EVENT_CHARGER, dev, comip_charger_irq);
	if (retval) {
		dev_err(dev->dev,"Fail to register charger_in_interrupt\n");
		goto chgin_err;
	}
	pmic_callback_unmask(PMIC_EVENT_CHARGER);

	return 0;

chgin_err:
	usb_del_gadget_udc(&dev->gadget);
err_add_udc:
	pmic_callback_unregister(PMIC_EVENT_USB);
err_register_pmic_callback:
	device_unregister(&dev->gadget.dev);
irqreqfail:
	clk_put(dev->clk);
clkreqfail:

	return retval;
}

static int comip_u2d_remove(struct platform_device *pdev)
{
	//unsigned int val;
	struct comip_u2d *dev = platform_get_drvdata(pdev);

	usb_del_gadget_udc(&dev->gadget);

	platform_set_drvdata(pdev, NULL);

	if (dev->got_irq) {
		free_irq(INT_USB_OTG, dev);
		/*val = comip_u2d_read_reg32(ICTL1_IRQ_INTEN);
		val &= ~(1 << 2);
		comip_u2d_write_reg32(val, ICTL1_IRQ_INTEN);*/
		dev->got_irq = 0;
		put_device(&dev->gadget.dev);
	}
	//u2d_clk_set(0);
	clk_put(dev->clk);

	device_unregister(&dev->gadget.dev);
	plugin_stat = LCUSB_EVENT_NONE;
	pmic_callback_unregister(PMIC_EVENT_CHARGER);
	comip_u2d_remove_proc_file();

	return 0;
}

#ifdef CONFIG_PM
static int comip_u2d_suspend(struct platform_device *pdev, pm_message_t state)
{
	INFO("%s enter suspend\n", __func__);
	//u2d_clk_set(0);
	return 0;
}

static int comip_u2d_resume(struct platform_device *pdev)
{
	INFO("%s enter resume\n", __func__);
	//u2d_clk_set(1);
	return 0;
}
#endif

static struct platform_driver comip_u2d_driver = {
	.driver = {
			.name = "comip-u2d",
			.owner  = THIS_MODULE,
		   },
	.probe = comip_u2d_probe,
	.remove = comip_u2d_remove,
#ifdef CONFIG_PM
	.suspend = comip_u2d_suspend,
	.resume = comip_u2d_resume,
#endif
};
module_platform_driver_probe(comip_u2d_driver, comip_u2d_probe);



MODULE_AUTHOR("lengyansong <lengyansong@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP USB 2.0 Device Controller driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:usb-otg");
