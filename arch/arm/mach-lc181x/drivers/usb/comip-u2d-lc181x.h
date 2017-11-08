/*************************************************************************
*  Copyright (c) 2008-2009  LeadCoreTech Corp.
**************************************************************************
*  File name:   comip_u2d.h
*
*  Author: gongyibin
*
*  Description:
*
*--------------------------------------------------------------------------
*  Change History:
*--------------------------------------------------------------------------
*  Version	Date		Author			Description
*--------------------------------------------------------------------------
*  1.00		2009-01-16	gongyibin			created
**************************************************************************/
#include <linux/usb/gadget.h>
#include <linux/types.h>
#include <linux/sched.h>

#ifndef _DD_USB_DTT_U2D_H
#define _DD_USB_DTT_U2D_H

#define U2D_PRINTK

#define STAT_IO_BS_H_READY		0x0
#define STAT_IO_BS_DMABUSY		0x1
#define STAT_IO_BS_DMADONE		0x2
#define STAT_IO_BS_H_BUSY		0x3

#define STAT_IO_BS_SHIFT		30
#define STAT_IO_RTS_SUCCESS		0x0
#define STAT_IN_TS_BUFFLUSH		0x1
#define STAT_IO_RTS_BUFERR		0x3
#define STAT_IO_RTS_SHIFT		28
#define STAT_IO_L				(1 << 27) /* Last buffer */
#define STAT_IO_L_SHIFT			(27) /* Last buffer */
#define STAT_IO_SP				(1 << 26) /* short packaget */
#define STAT_IO_SP_SHIFT		(26) /* short packaget */
#define STAT_IO_IOC				(1 << 25) /* Interrupt completed */
#define STAT_IO_IOC_SHIFT		(25) /* Interrupt completed */

#define STAT_OUT_NON_ISO_SR		(1 << 24) /* Setup package received */
#define STAT_OUT_NON_ISO_MTRF	(1 << 23)
#define STAT_OUT_ISO_PID_0		0x0
#define STAT_OUT_ISO_PID_1		0x1
#define STAT_OUT_ISO_PID_2		0x2
#define STAT_IN_ISO_PID_1		0x1 /* 1 package, start DATA0 */
#define STAT_IN_ISO_PID_2		0x2 /* 2 package, start DATA1 */
#define STAT_IN_ISO_PID_3		0x3 /* 3 package, start DATA2 */
#define STAT_IO_ISO_PID_SHIFT	23
#define STAT_IO_ISO_FN_SHIFT	12 /* Frame number */
#define STAT_IO_NON_ISO_RTB_MASK	0xffff
#define STAT_OUT_ISO_RB_MASK		0x7ff
#define STAT_IN_ISO_TX_MASK			0xfff

static inline void comip_u2d_write_reg32(uint32_t value, int paddr)
{
	writel(value, io_p2v(paddr));
}

static inline unsigned int comip_u2d_read_reg32(int paddr)
{
	return readl(io_p2v(paddr));
}


struct comip_request {
	struct usb_request			req;
	unsigned					map;
	struct list_head			queue;
};

enum ep0_state {
	EP0_IDLE,
	EP0_IN_DATA_PHASE,
	EP0_OUT_DATA_PHASE,
	EP0_STALL,
	EP0_IN_FAKE,
	EP0_NO_ACTION
};

/* 8KB EP FIFO */
#define EP0_MPS				((unsigned)64)
#define BULK_MPS(speed)		((speed)==USB_SPEED_HIGH)?((unsigned)512):((unsigned)64)
#define ISO_MPS(speed)		((speed)==USB_SPEED_HIGH)?((unsigned)1024):((unsigned)1023)
#define INT_MPS(speed)		((speed)==USB_SPEED_HIGH)?((unsigned)1024):((unsigned)8)

#define EP0_FIFO_SIZE		((unsigned)64)
#define BULK_FIFO_SIZE		((unsigned)512 )
#define ISO_FIFO_SIZE		((unsigned)1024 )
#define INT_FIFO_SIZE		((unsigned)100 )

#define U2D_DMA_BUF_SIZE	(4096 * 8ul)
#define DMA_DESC_SIZE		4096
#define DMA_DESC_NUM		(DMA_DESC_SIZE/16)
#define MIN(x,y) 			((x)<(y)?(x):(y))

#define GET_DMA_BYTES(len, mps)	(((len) % (mps) == 0)? (len) : ((len) / (mps) + 1) * (mps))

struct u2d_stats {
	u32			irqs;
};

#define	U2D_EP_NUM	15

struct comip_dma_des {
	unsigned int status;
	dma_addr_t buf_addr;
};

struct comip_ep {
	struct usb_ep				ep;
	struct comip_u2d			*dev;
	u8							name[16];
	const struct usb_endpoint_descriptor	*desc;
	struct list_head			queue;

	u16							fifo_size;
	u16							ep_num;
	u16							ep_type;

	u16							stopped : 1;
	u16							dir_in : 1;
	u16							assigned : 1;

	struct comip_dma_des *		dma_desc;
	dma_addr_t					dma_desc_dma_addr;
	/* dma irqs */
	u32							irqs;

	spinlock_t  				*lock;
};

struct comip_u2d {
	struct device				*dev;	
	struct usb_gadget			gadget;
	struct usb_gadget_driver	*driver;
#ifdef CONFIG_USB_COMIP_OTG
	struct usb_phy *phy;
#endif	
	/* ep0 */
	struct comip_request		ep0_req;	/* include the usb_req to respond to the get_desc request, etc */
	enum ep0_state				ep0state;
	/* eps */
	struct comip_ep				ep [U2D_EP_NUM];
	/* statics */
	struct u2d_stats			stats;
	/* probe got irq */
	char						got_irq : 1;
	u32   						setup[2];
	u32							gadget_stoped;	
	u8 							enum_done;/*enum done flag*/
	u8 							insert_flag;/*insert flag*/
	/* dma desc pool*/
	struct dma_pool				*dma_desc_pool;
	/* ep0 in dma desc */
	struct comip_dma_des 		*ep0_in_desc;
	dma_addr_t 					ep0_in_desc_dma_addr;
	/* ep0 out dma desc */
	struct comip_dma_des 		*ep0_out_desc;	
	dma_addr_t 					ep0_out_desc_dma_addr;
	/* usb clock */
	struct clk 					*clk;
	/* spin lock */
	spinlock_t  				*lock;
	int (*pmic_notify)(void);
	struct lcusb_transceiver lcusb;
	int linkstat;
	int vchgstat;;
    struct work_struct	charger_work;
};

#endif
