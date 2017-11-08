/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/comip_hcd_linux.c $
 * $Revision: #18 $
 * $Date: 2011/05/17 $
 * $Change: 1774126 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */
/**
 * @file
 *
 * This file contains the implementation of the HCD. In Linux, the HCD
 * implements the hc_driver API.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#include <asm/io.h>
#include <linux/usb.h>
#include <linux/usb/otg.h>
#include <linux/usb/hcd.h>
#include <linux/platform_device.h>
#include <plat/comip-pmic.h>
#include <mach/comip-regs.h>
#include <plat/usb.h>
#include <linux/clk.h>
#include "comip_hcd_if.h"
#include "comip_hcd_dbg.h"
#include "comip_hcd.h"

#include <linux/dmapool.h>
/**
 * Gets the endpoint number from a _bEndpointAddress argument. The endpoint is
 * qualified with its direction (possible 32 endpoints per device).
 */
#define comip_ep_addr_to_endpoint(_bEndpointAddress_) ((_bEndpointAddress_ & USB_ENDPOINT_NUMBER_MASK) | \
                             ((_bEndpointAddress_ & USB_DIR_IN) != 0) << 4)
static comip_hcd_device_t *comip_hcd_dev;
static const char comip_hcd_name[] = "comip-hcd";
#ifdef CONFIG_USB_COMIP_OTG
static struct usb_phy *phy;
#endif
/** @name Linux HC Driver API Functions */
/** @{ */
static int urb_enqueue(struct usb_hcd *hcd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
               struct usb_host_endpoint *ep,
#endif
               struct urb *urb, gfp_t mem_flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static int urb_dequeue(struct usb_hcd *hcd, struct urb *urb);
#else
static int urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status);
#endif

static void endpoint_disable(struct usb_hcd *hcd, struct usb_host_endpoint *ep);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
static void endpoint_reset(struct usb_hcd *hcd, struct usb_host_endpoint *ep);
#endif
static irqreturn_t comip_hcd_irq(struct usb_hcd *hcd);
extern int hcd_start(struct usb_hcd *hcd);
extern void hcd_stop(struct usb_hcd *hcd);
static int get_frame_number(struct usb_hcd *hcd);
extern int hub_status_data(struct usb_hcd *hcd, char *buf);
extern int hub_control(struct usb_hcd *hcd,
               u16 typeReq,
               u16 wValue, u16 wIndex, char *buf, u16 wLength);
struct wrapper_priv_data {
    comip_hcd_t *comip_hcd;
};
static int comip_bus_suspend(struct usb_hcd *usb_hcd);
static int comip_bus_resume(struct usb_hcd *usb_hcd);

/** @} */

static struct hc_driver comip_hc_driver = {

    .description = comip_hcd_name,
    .product_desc = "COMIP Controller",
    .hcd_priv_size = sizeof(struct wrapper_priv_data),

    .irq = comip_hcd_irq,

    .flags = HCD_MEMORY | HCD_USB2,

    //.reset =              
    .start = hcd_start,
    //.suspend =            
    //.resume =             
    .stop = hcd_stop,

    .urb_enqueue = urb_enqueue,
    .urb_dequeue = urb_dequeue,
    .endpoint_disable = endpoint_disable,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    .endpoint_reset = endpoint_reset,
#endif
    .get_frame_number = get_frame_number,

    .hub_status_data = hub_status_data,
    .hub_control = hub_control,
    .bus_suspend = comip_bus_suspend,               
    .bus_resume = comip_bus_resume,
};

/** Gets the comip_hcd from a struct usb_hcd */
static inline comip_hcd_t *hcd_to_comip_hcd(struct usb_hcd *hcd)
{
    struct wrapper_priv_data *p;
    p = (struct wrapper_priv_data *)(hcd->hcd_priv);
    return p->comip_hcd;
}

/** Gets the struct usb_hcd that contains a comip_hcd_t. */
static inline struct usb_hcd *comip_hcd_to_hcd(comip_hcd_t * comip_hcd)
{
    return comip_hcd_get_priv_data(comip_hcd);
}

/** Gets the usb_host_endpoint associated with an URB. */
inline struct usb_host_endpoint *comip_urb_to_endpoint(struct urb *urb)
{
	if (!urb->ep)
		return NULL;
	else
		return urb->ep;
}

static int _disconnect(comip_hcd_t * hcd)
{
    struct usb_hcd *usb_hcd = comip_hcd_to_hcd(hcd);

    usb_hcd->self.is_b_host = 0;
    return 0;
}

static int _start(comip_hcd_t * hcd)
{
    struct usb_hcd *usb_hcd = comip_hcd_to_hcd(hcd);

    //usb_hcd->self.is_b_host = comip_hcd_is_b_host(hcd);
    hcd_start(usb_hcd);

    return 0;
}

static int _hub_info(comip_hcd_t * hcd, void *urb_handle, uint32_t * hub_addr,
             uint32_t * port_addr)
{
    struct urb *urb = (struct urb *)urb_handle;
    if (urb->dev->tt) {
        *hub_addr = urb->dev->tt->hub->devnum;
    } else {
        *hub_addr = 0;
    }
    *port_addr = urb->dev->ttport;
    return 0;
}

static int _speed(comip_hcd_t * hcd, void *urb_handle)
{
    struct urb *urb = (struct urb *)urb_handle;
    return urb->dev->speed;
}

static int _get_b_hnp_enable(comip_hcd_t * hcd)
{
    struct usb_hcd *usb_hcd = comip_hcd_to_hcd(hcd);
    return usb_hcd->self.b_hnp_enable;
}

static void allocate_bus_bandwidth(struct usb_hcd *hcd, uint32_t bw,
                   struct urb *urb)
{
    hcd_to_bus(hcd)->bandwidth_allocated += bw / urb->interval;
    if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
        hcd_to_bus(hcd)->bandwidth_isoc_reqs++;
    } else {
        hcd_to_bus(hcd)->bandwidth_int_reqs++;
    }
}

static void free_bus_bandwidth(struct usb_hcd *hcd, uint32_t bw,
                   struct urb *urb)
{
    hcd_to_bus(hcd)->bandwidth_allocated -= bw / urb->interval;
    if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
        hcd_to_bus(hcd)->bandwidth_isoc_reqs--;
    } else {
        hcd_to_bus(hcd)->bandwidth_int_reqs--;
    }
}

/**
 * Sets the final status of an URB and returns it to the device driver. Any
 * required cleanup of the URB is performed.
 */
static int _complete(comip_hcd_t * hcd, void *urb_handle,
             comip_hcd_urb_t * comip_urb, int32_t status)
{
    struct urb *urb = (struct urb *)urb_handle;
#ifdef DEBUG
    if (CHK_DEBUG_LEVEL(DBG_HCDV | DBG_HCD_URB)) {
        COMIP_PRINTF("%s: urb %p, device %d, ep %d %s, status=%d\n",
               __func__, urb, usb_pipedevice(urb->pipe),
               usb_pipeendpoint(urb->pipe),
               usb_pipein(urb->pipe) ? "IN" : "OUT", status);
        if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
            int i;
            for (i = 0; i < urb->number_of_packets; i++) {
                COMIP_PRINTF("  ISO Desc %d status: %d\n",
                       i, urb->iso_frame_desc[i].status);
            }
        }
    }
#endif

    urb->actual_length = comip_hcd_urb_get_actual_length(comip_urb);
    /* Convert status value. */
    DBG("urb status %d\n", status);


    if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
        int i;

        urb->error_count = comip_hcd_urb_get_error_count(comip_urb);
        for (i = 0; i < urb->number_of_packets; ++i) {
            urb->iso_frame_desc[i].actual_length =
                comip_hcd_urb_get_iso_desc_actual_length
                (comip_urb, i);
            urb->iso_frame_desc[i].status =
                comip_hcd_urb_get_iso_desc_status(comip_urb, i);
        }
    }

    urb->status = status;
    urb->hcpriv = NULL;
    if (!status) {
        if ((urb->transfer_flags & URB_SHORT_NOT_OK) &&
            (urb->actual_length < urb->transfer_buffer_length)) {
            urb->status = -EREMOTEIO;
        }
    }

    if ((usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) ||
        (usb_pipetype(urb->pipe) == PIPE_INTERRUPT)) {
        struct usb_host_endpoint *ep = comip_urb_to_endpoint(urb);
        if (ep) {
            free_bus_bandwidth(comip_hcd_to_hcd(hcd),
                       comip_hcd_get_ep_bandwidth(hcd,
                                    ep->hcpriv),
                       urb);
        }
    }

    kfree(comip_urb);

    COMIP_SPINUNLOCK(hcd->lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
    usb_hcd_giveback_urb(comip_hcd_to_hcd(hcd), urb);
#else
    usb_hcd_giveback_urb(comip_hcd_to_hcd(hcd), urb, status);
#endif
    COMIP_SPINLOCK(hcd->lock);

    return 0;
}

static struct comip_hcd_function_ops hcd_fops = {
    .start = _start,
    .disconnect = _disconnect,
    .hub_info = _hub_info,
    .speed = _speed,
    .complete = _complete,
    .get_b_hnp_enable = _get_b_hnp_enable,
};

/**
 * Initializes the HCD. This function allocates memory for and initializes the
 * static parts of the usb_hcd and comip_hcd structures. It also registers the
 * USB bus with the core and calls the hc_driver->start() function. It returns
 * a negative error on failure.
 */
int hcd_init(struct platform_device *_dev)
{
    struct usb_hcd *hcd = NULL;
    comip_hcd_t *comip_hcd = NULL;
    comip_hcd_device_t *hcd_dev = dev_get_drvdata(&(_dev)->dev);
    static u64 comip_dmamask = DMA_BIT_MASK(32);
    int retval = 0;

	printk("LZS %s: %d, enter\n", __func__, __LINE__);
    COMIP_DEBUGPL(DBG_HCD, "COMIP OTG HCD INIT\n");

    hcd_dev->irq = platform_get_irq(_dev, 0);
    
    /* Set device flags indicating whether the HCD supports DMA. */
    if (hcd_dev->core_if->core_params->dma_enable != 0) {
        _dev->dev.dma_mask = &comip_dmamask;//(void *)~0;
        _dev->dev.coherent_dma_mask = ~0;

    } else {
        _dev->dev.dma_mask = (void *)0;
        _dev->dev.coherent_dma_mask = 0;
    }
    /*
     * Allocate memory for the base HCD plus the COMIP OTG HCD.
     * Initialize the base HCD.
     */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
    hcd = usb_create_hcd(&comip_hc_driver, &_dev->dev, _dev->dev.bus_id);
#else
    hcd = usb_create_hcd(&comip_hc_driver, &_dev->dev, dev_name(&_dev->dev));
    hcd->has_tt = 1;
//      hcd->uses_new_polling = 1;
//      hcd->poll_rh = 0;
#endif
    if (!hcd) {
        retval = -ENOMEM;
        goto error1;
    }

    hcd->regs = hcd_dev->os_dep.base;

    /* Initialize the COMIP OTG HCD. */
    comip_hcd = comip_hcd_alloc_hcd();
    if (!comip_hcd) {
        goto error2;
    }
    ((struct wrapper_priv_data *)(hcd->hcd_priv))->comip_hcd =
        comip_hcd;
    hcd_dev->hcd = comip_hcd;

    if (comip_hcd_init(comip_hcd, hcd_dev->core_if)) {
        goto error2;
    }

	/* create  dma_pool resource */
	comip_hcd->qh_pool = dma_pool_create("comip_qh",
			&_dev->dev,
			sizeof(comip_host_dma_desc_t) *MAX_DMA_DESC_NUM_HS_ISOC,
			16,
			4096);

	comip_hcd->frame_pool = dma_pool_create("comip_frame",
			&_dev->dev,
			4 * MAX_FRLIST_EN_NUM,
			16,
			4096);

    hcd_dev->hcd->hcd_dev = hcd_dev;
    hcd->self.otg_port = comip_hcd_otg_port(comip_hcd);
  
    /*
     * Finish generic HCD initialization and start the HCD. This function
     * allocates the DMA buffer pool, registers the USB bus, requests the
     * IRQ line, and calls hcd_start method.
     */
    retval = usb_add_hcd(hcd, hcd_dev->irq, IRQF_SHARED | IRQF_DISABLED);
    if (retval < 0) {
        goto error2;
    }
#ifdef CONFIG_USB_COMIP_OTG
     /*
			* OTG driver takes care of PHY initialization, clock management,
			* powering up VBUS, mapping of registers address space and power
			* management.
			*/
			phy = usb_get_transceiver();
			if (!phy) {
				dev_err(&_dev->dev, "unable to find transceiver\n");
				retval = -ENODEV;
					goto error2;
				}

			COMIP_DEBUGPL(DBG_HCD, "usb_get_transceiver:0x%x\n",(unsigned int)phy);
			retval = otg_set_host(phy->otg, &hcd->self);
			if (retval < 0) {
				dev_err(&_dev->dev, "unable to register with transceiver\n");
				goto error2;
				}
#endif
		comip_hcd_set_priv_data(comip_hcd, hcd);
    return 0;

error2:
    usb_put_hcd(hcd);
error1:
    return retval;
}

/**
 * Removes the HCD.
 * Frees memory and resources associated with the HCD and deregisters the bus.
 */
void hcd_remove(struct platform_device *_dev)
{
    comip_hcd_device_t *hcd_dev = dev_get_drvdata(&(_dev)->dev);

    comip_hcd_t *comip_hcd;
    struct usb_hcd *hcd;

    COMIP_DEBUGPL(DBG_HCD, "COMIP OTG HCD REMOVE\n");

    if (!hcd_dev) {
        COMIP_DEBUGPL(DBG_ANY, "%s: hcd_dev NULL!\n", __func__);
        return;
    }

    comip_hcd = hcd_dev->hcd;

    if (!comip_hcd) {
        COMIP_DEBUGPL(DBG_ANY, "%s: hcd_dev->hcd NULL!\n", __func__);
        return;
    }

    hcd = comip_hcd_to_hcd(comip_hcd);

    if (!hcd) {
        COMIP_DEBUGPL(DBG_ANY,
                "%s: comip_hcd_to_hcd(comip_hcd) NULL!\n",
                __func__);
        return;
    }
    usb_remove_hcd(hcd);
    comip_hcd_set_priv_data(comip_hcd, NULL);
    comip_hcd_remove(comip_hcd);
    usb_put_hcd(hcd);
}

/* =========================================================================
 *  Linux HC Driver Functions
 * ========================================================================= */

/** Initializes the comip controller and its root hub and prepares it for host
 * mode operation. Activates the root port. Returns 0 on success and a negative
 * error code on failure. */
int hcd_start(struct usb_hcd *hcd)
{
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);
    struct usb_bus *bus;

    COMIP_DEBUGPL(DBG_HCD, "COMIP OTG HCD START\n");
    bus = hcd_to_bus(hcd);

    hcd->state = HC_STATE_RUNNING;
    if (comip_hcd_start(comip_hcd, &hcd_fops)) {
        return 0;
    }

    /* Initialize and connect root hub if one is not already attached */
    if (bus->root_hub) {
        COMIP_DEBUGPL(DBG_HCD, "COMIP OTG HCD Has Root Hub\n");
        /* Inform the HUB driver to resume. */
        usb_hcd_resume_root_hub(hcd);
    }

    return 0;
}

/**
 * Halts the comip host mode operations in a clean manner. USB transfers are
 * stopped.
 */
void hcd_stop(struct usb_hcd *hcd)
{
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);

    comip_hcd_stop(comip_hcd);
}

/** Returns the current frame number. */
static int get_frame_number(struct usb_hcd *hcd)
{
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);

    return comip_hcd_get_frame_number(comip_hcd);
}

#ifdef DEBUG
static void dump_urb_info(struct urb *urb, char *fn_name)
{
    COMIP_PRINTF("%s, urb %p\n", fn_name, urb);
    COMIP_PRINTF("  Device address: %d\n", usb_pipedevice(urb->pipe));
    COMIP_PRINTF("  Endpoint: %d, %s\n", usb_pipeendpoint(urb->pipe),
           (usb_pipein(urb->pipe) ? "IN" : "OUT"));
    COMIP_PRINTF("  Endpoint type: %s\n", ( {
                         char *pipetype;
                         switch (usb_pipetype(urb->pipe)) {
case PIPE_CONTROL:
pipetype = "CONTROL"; break; case PIPE_BULK:
pipetype = "BULK"; break; case PIPE_INTERRUPT:
pipetype = "INTERRUPT"; break; case PIPE_ISOCHRONOUS:
pipetype = "ISOCHRONOUS"; break; default:
                         pipetype = "UNKNOWN"; break;};
                         pipetype;}
           )) ;
    COMIP_PRINTF("  Speed: %s\n", ( {
                     char *speed; switch (urb->dev->speed) {
case USB_SPEED_HIGH:
speed = "HIGH"; break; case USB_SPEED_FULL:
speed = "FULL"; break; case USB_SPEED_LOW:
speed = "LOW"; break; default:
                     speed = "UNKNOWN"; break;};
                     speed;}
           )) ;
    COMIP_PRINTF("  Max packet size: %d\n",
           usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe)));
    COMIP_PRINTF("  Data buffer length: %d\n", urb->transfer_buffer_length);
    COMIP_PRINTF("  Transfer buffer: %p, Transfer DMA: %d\n",
           urb->transfer_buffer, (u32)urb->transfer_dma);
    COMIP_PRINTF("  Setup buffer: %p, Setup DMA: %d\n",
           urb->setup_packet, (u32)urb->setup_dma);
    COMIP_PRINTF("  Interval: %d\n", urb->interval);
    if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
        int i;
        for (i = 0; i < urb->number_of_packets; i++) {
            COMIP_PRINTF("  ISO Desc %d:\n", i);
            COMIP_PRINTF("    offset: %d, length %d\n",
                   urb->iso_frame_desc[i].offset,
                   urb->iso_frame_desc[i].length);
        }
    }
}

#endif

/** Starts processing a USB transfer request specified by a USB Request Block
 * (URB). mem_flags indicates the type of memory allocation to use while
 * processing this URB. */
static int urb_enqueue(struct usb_hcd *hcd,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
               struct usb_host_endpoint *ep,
#endif
               struct urb *urb, gfp_t mem_flags)
{
    int retval = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    struct usb_host_endpoint *ep = urb->ep;
#endif
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);
    comip_hcd_urb_t *comip_urb;
    int i;
    int alloc_bandwidth = 0;
    uint8_t ep_type = 0;
    uint32_t flags = 0;
    void *buf;

#ifdef DEBUG
    if (CHK_DEBUG_LEVEL(DBG_HCDV | DBG_HCD_URB)) {
        dump_urb_info(urb, "urb_enqueue");
    }
#endif

    if ((usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS)
        || (usb_pipetype(urb->pipe) == PIPE_INTERRUPT)) {
        if (!comip_hcd_is_bandwidth_allocated
            (comip_hcd, &ep->hcpriv)) {
            alloc_bandwidth = 1;
        }
    }

    switch (usb_pipetype(urb->pipe)) {
    case PIPE_CONTROL:
        ep_type = USB_ENDPOINT_XFER_CONTROL;
        break;
    case PIPE_ISOCHRONOUS:
        ep_type = USB_ENDPOINT_XFER_ISOC;
        break;
    case PIPE_BULK:
        ep_type = USB_ENDPOINT_XFER_BULK;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
        if (urb->sg) {
            //COMIP_WARN("SG LIST received - we don't support it\n"); //del by wudt
        }
#endif
        break;
    case PIPE_INTERRUPT:
        ep_type = USB_ENDPOINT_XFER_INT;
        break;
    default:
        INFO("Wrong ep type\n");
    }

    comip_urb = comip_hcd_urb_alloc(comip_hcd,
                        urb->number_of_packets,
                        mem_flags == GFP_ATOMIC ? 1 : 0);

    comip_hcd_urb_set_pipeinfo(comip_urb, usb_pipedevice(urb->pipe),
                     usb_pipeendpoint(urb->pipe), ep_type,
                     usb_pipein(urb->pipe),
                     usb_maxpacket(urb->dev, urb->pipe,
                           !(usb_pipein(urb->pipe))));

    buf = urb->transfer_buffer;
    if (hcd->self.uses_dma) {
        /*
         * Calculate virtual address from physical address,
         * because some class driver may not fill transfer_buffer.
         * In Buffer DMA mode virual address is used,
         * when handling non DWORD aligned buffers.
         */
        buf = phys_to_virt(urb->transfer_dma);
    }

    if (!(urb->transfer_flags & URB_NO_INTERRUPT))
        flags |= URB_GIVEBACK_ASAP;
    if (urb->transfer_flags & URB_ZERO_PACKET)
        flags |= URB_SEND_ZERO_PACKET;

    comip_hcd_urb_set_params(comip_urb, urb, buf,
                   urb->transfer_dma,
                   urb->transfer_buffer_length,
                   urb->setup_packet,
                   urb->setup_dma, flags, urb->interval);

    for (i = 0; i < urb->number_of_packets; ++i) {
        comip_hcd_urb_set_iso_desc_params(comip_urb, i,
                            urb->
                            iso_frame_desc[i].offset,
                            urb->
                            iso_frame_desc[i].length);
    }

    urb->hcpriv = comip_urb;
    retval = comip_hcd_urb_enqueue(comip_hcd, comip_urb, &ep->hcpriv,
                     mem_flags == GFP_ATOMIC ? 1 : 0);
    if (!retval) {
        if (alloc_bandwidth) {
            allocate_bus_bandwidth(hcd,
                           comip_hcd_get_ep_bandwidth
                           (comip_hcd, ep->hcpriv), urb);
        }
    }

    return retval;
}

/** Aborts/cancels a USB transfer request. Always returns 0 to indicate
 * success.  */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static int urb_dequeue(struct usb_hcd *hcd, struct urb *urb)
#else
static int urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
#endif
{
    comip_irqflags_t flags;
    comip_hcd_t *comip_hcd;
    COMIP_DEBUGPL(DBG_HCD, "COMIP OTG HCD URB Dequeue\n");

    comip_hcd = hcd_to_comip_hcd(hcd);

#ifdef DEBUG
    if (CHK_DEBUG_LEVEL(DBG_HCDV | DBG_HCD_URB)) {
        dump_urb_info(urb, "urb_dequeue");
    }
#endif

    COMIP_SPINLOCK_IRQSAVE(comip_hcd->lock, &flags);

    comip_hcd_urb_dequeue(comip_hcd, urb->hcpriv);

    kfree(urb->hcpriv);
    urb->hcpriv = NULL;
    COMIP_SPINUNLOCK_IRQRESTORE(comip_hcd->lock, flags);

    /* Higher layer software sets URB status. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
    usb_hcd_giveback_urb(hcd, urb);
#else
    usb_hcd_giveback_urb(hcd, urb, status);
#endif
    if (CHK_DEBUG_LEVEL(DBG_HCDV | DBG_HCD_URB)) {
        COMIP_PRINTF("Called usb_hcd_giveback_urb()\n");
        COMIP_PRINTF("  urb->status = %d\n", urb->status);
    }

    return 0;
}

/* Frees resources in the comip controller related to a given endpoint. Also
 * clears state in the HCD related to the endpoint. Any URBs for the endpoint
 * must already be dequeued. */
static void endpoint_disable(struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);

    COMIP_DEBUGPL(DBG_HCD,
            "COMIP OTG HCD EP DISABLE: _bEndpointAddress=0x%02x, "
            "endpoint=%d\n", ep->desc.bEndpointAddress,
            comip_ep_addr_to_endpoint(ep->desc.bEndpointAddress));
    comip_hcd_endpoint_disable(comip_hcd, ep->hcpriv, 250);
    ep->hcpriv = NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
/* Resets endpoint specific parameter values, in current version used to reset 
 * the data toggle(as a WA). This function can be called from usb_clear_halt routine */
static void endpoint_reset(struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
    comip_irqflags_t flags;
    struct usb_device *udev = NULL;
    int epnum = usb_endpoint_num(&ep->desc);
    int is_out = usb_endpoint_dir_out(&ep->desc);
    int is_control = usb_endpoint_xfer_control(&ep->desc);
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);

    struct platform_device *_dev = comip_hcd->hcd_dev->os_dep.platformdev;

    if (_dev)
        udev = to_usb_device(&_dev->dev);
    else
        return;

    COMIP_DEBUGPL(DBG_HCD, "COMIP OTG HCD EP RESET: Endpoint Num=0x%02d\n", epnum);

    COMIP_SPINLOCK_IRQSAVE(comip_hcd->lock, &flags);
    usb_settoggle(udev, epnum, is_out, 0);
    if (is_control)
        usb_settoggle(udev, epnum, !is_out, 0);

    if (ep->hcpriv) {
        comip_hcd_endpoint_reset(comip_hcd, ep->hcpriv);
    }
    COMIP_SPINUNLOCK_IRQRESTORE(comip_hcd->lock, flags);
}
#endif

/** Handles host mode interrupts for the comip controller. Returns IRQ_NONE if
 * there was no interrupt to handle. Returns IRQ_HANDLED if there was a valid
 * interrupt.
 *
 * This function is called by the USB core when an interrupt occurs */
static irqreturn_t comip_hcd_irq(struct usb_hcd *hcd)
{
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);
    int32_t retval = comip_hcd_handle_intr(comip_hcd);
    
    return IRQ_RETVAL(retval);
}

/** Creates Status Change bitmap for the root hub and root port. The bitmap is
 * returned in buf. Bit 0 is the status change indicator for the root hub. Bit 1
 * is the status change indicator for the single root port. Returns 1 if either
 * change indicator is 1, otherwise returns 0. */
int hub_status_data(struct usb_hcd *hcd, char *buf)
{
    comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);

    buf[0] = 0;
    buf[0] |= (comip_hcd_is_status_changed(comip_hcd, 1)) << 1;

    return (buf[0] != 0);
}

/** Handles hub class-specific requests. */
int hub_control(struct usb_hcd *hcd,
        u16 typeReq, u16 wValue, u16 wIndex, char *buf, u16 wLength)
{
    int retval;

    retval = comip_hcd_hub_control(hcd_to_comip_hcd(hcd),
                     typeReq, wValue, wIndex, buf, wLength);

    return retval;
}
static int comip_bus_suspend(struct usb_hcd *usb_hcd)
{
	return 0;
}
static int comip_bus_resume(struct usb_hcd *hcd)
{
	return 0;
}

/**
 * Global Debug Level Mask.
 */
uint32_t g_dbg_lvl = 0x00;     /* OFF */

/**
 * This function shows the driver Debug Level.
 */
static ssize_t dbg_level_show(struct device_driver *drv, char *buf)
{
    return sprintf(buf, "0x%0x\n", g_dbg_lvl);
}

/**
 * This function stores the driver Debug Level.
 */
static ssize_t dbg_level_store(struct device_driver *drv, const char *buf,
    size_t count)
{
    g_dbg_lvl = simple_strtoul(buf, NULL, 16);
    return count;
}

static DRIVER_ATTR(debuglevel, S_IRUGO | S_IWUSR, dbg_level_show,
    dbg_level_store);

/**
 * This function is called when a platform_device is unregistered with the
 * comip_hcd_driver. This happens, for example, when the rmmod command is
 * executed. The device may or may not be electrically present. If it is
 * present, the driver stops device processing. Any resources used on behalf
 * of this device are freed.
 *
 * @param _dev
 */
static int comip_hcd_driver_remove(struct platform_device *_dev)
{
    comip_hcd_device_t *hcd_dev = comip_hcd_dev;

    COMIP_DEBUGPL(DBG_ANY, "%s(%p)\n", __func__, _dev);

    if (!hcd_dev) {
        /* Memory allocation for the comip_otg_device failed. */
        COMIP_DEBUGPL(DBG_ANY, "%s: hcd_dev NULL!\n", __func__);
        return -ENODEV;
    }
#ifdef CONFIG_USB_COMIP_OTG
    if (phy) {
			otg_set_host(phy->otg, NULL);
			usb_put_transceiver(phy);
		}
#endif
    if (hcd_dev->hcd) {
        hcd_remove(_dev);
    } else {
        COMIP_DEBUGPL(DBG_ANY, "%s: otg_dev->hcd NULL!\n", __func__);
        return -EINVAL;
    }
    /*
     * Free the IRQ
     */
    if (hcd_dev->common_irq_installed) {
        free_irq(hcd_dev->irq, hcd_dev); 
    } else {
        COMIP_DEBUGPL(DBG_ANY, "%s: There is no installed irq!\n", __func__);
        return -EINVAL;
    }

    if (hcd_dev->core_if) {
        comip_cil_remove(hcd_dev->core_if);
        clk_put(hcd_dev->core_if->clk);
    } else {
        COMIP_DEBUGPL(DBG_ANY, "%s: otg_dev->core_if NULL!\n", __func__);
        return -EINVAL;
    }

    /*
     * Remove the device attributes
     */
    wake_lock_destroy(&hcd_dev->wakelock);
    mutex_destroy(&hcd_dev->muxlock);
    /*
     * Return the memory.
     */
    if (hcd_dev->os_dep.base) {
        iounmap(hcd_dev->os_dep.base);
    }
    kfree(hcd_dev);

    /*
     * Clear the drvdata pointer.
     */
    dev_set_drvdata(&(_dev)->dev, 0);
    comip_hcd_dev = 0;
    return 0;
}

/**
 * This function is called when an platform_device is bound to a
 * comip_hcd_driver. It creates the driver components required to
 * control the device and it initializes the device. 
 * The driver components are stored in a comip_hcd_device
 * structure. A reference to the comip_hcd_device is saved in the
 * platform_device. This allows the driver to access the comip_hcd_device
 * structure on subsequent calls to driver methods for this device.
 *
 * @param _dev Bus device
 */
static int comip_hcd_driver_probe(struct platform_device *_dev)
{
    int retval = 0;
    struct resource *iomem;
    struct clk *clk=NULL;

    dev_dbg(&_dev->dev, "comip_hcd_driver_probe(%p)\n", _dev);

    comip_hcd_dev = kzalloc(sizeof(comip_hcd_device_t), GFP_KERNEL);
    if (!comip_hcd_dev) {
        dev_err(&_dev->dev, "kmalloc of comip_hcd_device failed\n");
        return -ENOMEM;
    }

    memset(comip_hcd_dev, 0, sizeof(*comip_hcd_dev));
    comip_hcd_dev->os_dep.reg_offset = 0xFFFFFFFF;
    /*
     * Map the comip_otg Core memory into virtual address space.
     */
    comip_hcd_dev->os_dep.base = NULL;
    iomem = platform_get_resource(_dev, IORESOURCE_MEM, 0);
    comip_hcd_dev->os_dep.base = ioremap(iomem->start, resource_size(iomem));

    if (!comip_hcd_dev->os_dep.base) {
        dev_err(&_dev->dev, "ioremap() failed\n");
        kfree(comip_hcd_dev);
        return -ENOMEM;
    }

    comip_hcd_dev->irq =  platform_get_irq(_dev, 0);

    wake_lock_init(&comip_hcd_dev->wakelock, WAKE_LOCK_SUSPEND, "usb_host_wakelock");
    mutex_init(&comip_hcd_dev->muxlock);
    mutex_lock(&comip_hcd_dev->muxlock);
    /*
     * Initialize driver data to point to the global comip_otg
     * Device structure.
     */
    comip_usb_power_set(1);

    dev_set_drvdata(&(_dev)->dev, comip_hcd_dev);

	clk = clk_get(&_dev->dev, "usbotg_12m_clk");
	if (IS_ERR(clk)) {
        dev_err(&_dev->dev, "cannot get clock usbotg_12m_clk\n");
        retval = -EBUSY;
        goto fail;
	}

	comip_hcd_dev->core_if = comip_cil_init(comip_hcd_dev->os_dep.base, clk);
    if (!comip_hcd_dev->core_if) {
        dev_err(&_dev->dev, "CIL initialization failed!\n");
        retval = -ENOMEM;
        goto fail;
    }
    /*
     * Attempt to ensure this device is really a VC0883_otg Controller.
     * Read and verify the SNPSID register contents. The value should be
     * 0x45F42XXX, which corresponds to "OT2", as in "OTG version 2.XX".
     */

    if ((comip_get_gsnpsid(comip_hcd_dev->core_if) & 0xFFFFF000) !=
        0x4F542000) {
        dev_err(&_dev->dev, "Bad value for SNPSID: 0x%08x\n",
            comip_get_gsnpsid(comip_hcd_dev->core_if));
        retval = -EINVAL;
        goto fail;
    }

	/*
	 * Disable the global interrupt until all the interrupt
	 * handlers are installed.
	 */
	//comip_disable_global_interrupts(comip_hcd_device->core_if);

    /*
     * Initialize the HCD
     */
    retval = hcd_init(_dev);
    if (retval != 0) {
        COMIP_ERROR("hcd_init failed\n");
        comip_hcd_dev->hcd = NULL;
        goto fail;
    }
    /*
     * Enable the global interrupt after all the interrupt
     * handlers are installed if there is no ADP support else 
     * perform initial actions required for Internal ADP logic.
     */
    if (!comip_get_param_adp_enable(comip_hcd_dev->core_if)) { 
        comip_enable_global_interrupts(comip_hcd_dev->core_if);
        }
        
    if(comip_is_device_mode(comip_hcd_dev->core_if)){
        COMIP_DEBUGPL(DBG_ANY, "%s device mode\n", __func__);
        comip_cil_uninit(comip_hcd_dev->core_if);
#ifndef CONFIG_USB_COMIP_HSIC
        comip_usb_power_set(0);
#endif
    }
    mutex_unlock(&comip_hcd_dev->muxlock);
    return 0;

fail:
    comip_hcd_driver_remove(_dev);
#ifndef CONFIG_USB_COMIP_HSIC
    comip_usb_power_set(0);
#endif
    mutex_unlock(&comip_hcd_dev->muxlock);
    return retval;
}
static int g_host_mode = 0;
extern int32_t comip_hcd_disconnect_cb(void *p);
int comip_hcd_driver_stop(struct platform_device *_dev)
{
        comip_irqflags_t flags;
	struct usb_hcd *hcd = dev_get_drvdata(&(_dev)->dev);
	comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);
	comip_hcd_device_t *hcd_dev = comip_hcd_dev;
	
printk("LZS %s: %d, enter\n", __func__, __LINE__);

	if(comip_is_device_mode(comip_hcd->core_if)&& (!g_host_mode)){
		INFO( "%s device mode(%d,%d)\n", __func__,comip_is_device_mode(comip_hcd->core_if),g_host_mode);
		goto out;
	}
    COMIP_SPINLOCK_IRQSAVE(comip_hcd->lock, &flags);
	g_host_mode = 0;
	comip_hcd->flags.b.port_enable_change = 1;
	comip_hcd_disconnect_cb(comip_hcd);
	mdelay(10);
	comip_cil_uninit(comip_hcd->core_if);

	COMIP_SPINUNLOCK_IRQRESTORE(comip_hcd->lock, flags);
	mdelay(100);
#ifndef CONFIG_USB_COMIP_HSIC
	comip_usb_power_set(0);
#endif
	wake_unlock(&hcd_dev->wakelock);
	INFO( "%s end\n", __func__);
out:
	return 0;
}

int comip_hcd_driver_start(struct platform_device *_dev)
{
        comip_irqflags_t flags;
	struct usb_hcd *hcd = dev_get_drvdata(&(_dev)->dev);
	comip_hcd_t *comip_hcd = hcd_to_comip_hcd(hcd);
	comip_hcd_device_t *hcd_dev = comip_hcd_dev;

printk("LZS %s: %d, enter\n", __func__, __LINE__);


	if(comip_is_device_mode(comip_hcd->core_if)||g_host_mode){
		INFO("%s device mode(%d,%d)\n", __func__,comip_is_device_mode(comip_hcd->core_if),g_host_mode);
		goto out;
	}
	wake_lock(&hcd_dev->wakelock);
	comip_usb_power_set(1);
	mdelay(100);
	INFO("%s init_reg\n", __func__);
    COMIP_SPINLOCK_IRQSAVE(comip_hcd->lock, &flags);
	set_usb_init_reg(comip_hcd->core_if);
	/* Restore USB PHY settings and enable the controller. */
	comip_core_init(comip_hcd->core_if);
	comip_enable_global_interrupts(comip_hcd->core_if);
	_start(comip_hcd);
	g_host_mode = 1;
	COMIP_SPINUNLOCK_IRQRESTORE(comip_hcd->lock, flags);
	INFO("%s end\n", __func__);
out:
	return 0;
}

static int comip_hcd_driver_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;

}

static int comip_hcd_driver_resume(struct platform_device *pdev)
{
	return 0;
}
/**
 * This structure defines the methods to be called by a bus driver
 * during the lifecycle of a device on that bus. Both drivers and
 * devices are registered with a bus driver. The bus driver matches
 * devices to drivers based on information in the device and driver
 * structures.
 *
 * The probe function is called when the bus driver matches a device
 * to this driver. The remove function is called when a device is
 * unregistered with the bus driver.
 */
static struct platform_driver comip_hcd_driver = {
    .probe = comip_hcd_driver_probe,
    .remove = comip_hcd_driver_remove,
    .suspend = comip_hcd_driver_suspend,
    .resume = comip_hcd_driver_resume,
    .driver = {
        .name = (char *)comip_hcd_name,
        .owner = THIS_MODULE,
    },
};

/**
 * This function is called when the comip_hcd_driver is installed with the
 * insmod command. It registers the comip_hcd_driver structure with the
 * appropriate bus driver. This will cause the comip_hcd_driver_probe function
 * to be called. In addition, the bus driver will automatically expose
 * attributes defined for the device and driver in the special sysfs file
 * system.
 *
 * @return
 */
static int __init comip_hcd_driver_init(void)
{
    int retval = 0;
	int error;
	COMIP_DEBUGPL(DBG_ANY, "comip_hcd_driver_init\n");
    retval = platform_driver_register(&comip_hcd_driver);
	error = driver_create_file(&comip_hcd_driver.driver, &driver_attr_debuglevel);
    if (retval < 0) {
        printk(KERN_ERR "%s retval=%d\n", __func__, retval);
        return retval;
    }
	return retval;
}
module_init(comip_hcd_driver_init);

/**
 * This function is called when the driver is removed from the kernel
 * with the rmmod command. The driver unregisters itself with its bus
 * driver.
 *
 */
static void __exit comip_hcd_driver_exit(void)
{
	driver_remove_file(&comip_hcd_driver.driver, &driver_attr_debuglevel);
	platform_driver_unregister(&comip_hcd_driver);
}

module_exit(comip_hcd_driver_exit);
