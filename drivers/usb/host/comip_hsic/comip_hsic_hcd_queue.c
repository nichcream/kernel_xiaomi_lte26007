/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/comip_hsic_hcd_queue.c $
 * $Revision: #42 $
 * $Date: 2010/11/29 $
 * $Change: 1636033 $
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
 * This file contains the functions to manage Queue Heads and Queue
 * Transfer Descriptors.
 */

#include "comip_hsic_hcd.h"
#include "comip_hsic_otg_regs.h"

/** 
 * Free each QTD in the QH's QTD-list then free the QH.  QH should already be
 * removed from a list.  QTD list should already be empty if called from URB
 * Dequeue.
 *
 * @param hcd HCD instance.
 * @param qh The QH to free.
 */
void comip_hsic_hcd_qh_free(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh)
{
    comip_hsic_qtd_t *qtd, *qtd_tmp;
    comip_hsic_irqflags_t flags;


    /* Free each QTD in the QTD list */
//    COMIP_HSIC_SPINLOCK(hcd->lock);
   COMIP_HSIC_SPINLOCK_IRQSAVE(hcd->lock, &flags);
    COMIP_HSIC_CIRCLEQ_FOREACH_SAFE(qtd, qtd_tmp, &qh->qtd_list, qtd_list_entry) {
        COMIP_HSIC_CIRCLEQ_REMOVE(&qh->qtd_list, qtd, qtd_list_entry);
        comip_hsic_hcd_qtd_free(qtd);
    }
    if (hcd->core_if->dma_desc_enable) {
        comip_hsic_hcd_qh_free_ddma(hcd, qh);
    } else if (qh->dw_align_buf) {
        uint32_t buf_size;
        if (qh->ep_type == UE_ISOCHRONOUS) {
            buf_size = 4096;
        } else {
            buf_size = hcd->core_if->core_params->max_transfer_size;
        }
        COMIP_HSIC_DMA_FREE(buf_size, qh->dw_align_buf, qh->dw_align_buf_dma);
    }

    kfree(qh);
    //COMIP_HSIC_SPINUNLOCK(hcd->lock);
   COMIP_HSIC_SPINUNLOCK_IRQRESTORE(hcd->lock, flags);
    return;
}

#define BitStuffTime(bytecount)  ((8 * 7* bytecount) / 6)
#define HS_HOST_DELAY       5   /* nanoseconds */
#define FS_LS_HOST_DELAY    1000    /* nanoseconds */
#define HUB_LS_SETUP        333 /* nanoseconds */
#define NS_TO_US(ns)        ((ns + 500) / 1000)
                /* convert & round nanoseconds to microseconds */

static uint32_t calc_bus_time(int speed, int is_in, int is_isoc, int bytecount)
{
    uint32_t retval;

    switch (speed) {
    case USB_SPEED_HIGH:
        if (is_isoc) {
            retval =
                ((38 * 8 * 2083) +
                 (2083 * (3 + BitStuffTime(bytecount)))) / 1000 +
                HS_HOST_DELAY;
        } else {
            retval =
                ((55 * 8 * 2083) +
                 (2083 * (3 + BitStuffTime(bytecount)))) / 1000 +
                HS_HOST_DELAY;
        }
        break;
    case USB_SPEED_FULL:
        if (is_isoc) {
            retval =
                (8354 * (31 + 10 * BitStuffTime(bytecount))) / 1000;
            if (is_in) {
                retval = 7268 + FS_LS_HOST_DELAY + retval;
            } else {
                retval = 6265 + FS_LS_HOST_DELAY + retval;
            }
        } else {
            retval =
                (8354 * (31 + 10 * BitStuffTime(bytecount))) / 1000;
            retval = 9107 + FS_LS_HOST_DELAY + retval;
        }
        break;
    case USB_SPEED_LOW:
        if (is_in) {
            retval =
                (67667 * (31 + 10 * BitStuffTime(bytecount))) /
                1000;
            retval =
                64060 + (2 * HUB_LS_SETUP) + FS_LS_HOST_DELAY +
                retval;
        } else {
            retval =
                (66700 * (31 + 10 * BitStuffTime(bytecount))) /
                1000;
            retval =
                64107 + (2 * HUB_LS_SETUP) + FS_LS_HOST_DELAY +
                retval;
        }
        break;
    default:
        COMIP_HSIC_WARN("Unknown device speed\n");
        retval = -1;
    }

    return NS_TO_US(retval);
}

/** 
 * Initializes a QH structure.
 *
 * @param hcd The HCD state structure for the COMIP OTG controller.
 * @param qh  The QH to init.
 * @param urb Holds the information about the device/endpoint that we need
 *        to initialize the QH. 
 */
#define SCHEDULE_SLOP 10
void hsic_qh_init(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh, comip_hsic_hcd_urb_t * urb)
{
    char *speed, *type;
    int dev_speed;
    uint32_t hub_addr, hub_port;

    memset(qh, 0, sizeof(comip_hsic_qh_t));

    /* Initialize QH */
    qh->ep_type = comip_hsic_hcd_get_pipe_type(&urb->pipe_info);
    qh->ep_is_in = comip_hsic_hcd_is_pipe_in(&urb->pipe_info) ? 1 : 0;

    qh->data_toggle = COMIP_HSIC_HC_PID_DATA0;
    qh->maxp = comip_hsic_hcd_get_mps(&urb->pipe_info);
    COMIP_HSIC_CIRCLEQ_INIT(&qh->qtd_list);
    COMIP_HSIC_LIST_INIT(&qh->qh_list_entry);
    qh->channel = NULL;

    /* FS/LS Enpoint on HS Hub 
     * NOT virtual root hub */
    dev_speed = hcd->fops->speed(hcd, urb->priv);

    hcd->fops->hub_info(hcd, urb->priv, &hub_addr, &hub_port);
    qh->do_split = 0;

    if (((dev_speed == USB_SPEED_LOW) ||
         (dev_speed == USB_SPEED_FULL)) &&
        (hub_addr != 0 && hub_addr != 1)) {
        COMIP_HSIC_DEBUGPL(DBG_HCD,
                "QH init: EP %d: TT found at hub addr %d, for port %d\n",
                comip_hsic_hcd_get_ep_num(&urb->pipe_info), hub_addr,
                hub_port);
        qh->do_split = 1;
    }

    if (qh->ep_type == UE_INTERRUPT || qh->ep_type == UE_ISOCHRONOUS) {
        /* Compute scheduling parameters once and save them. */
        hprt0_data_t hprt;

        /** @todo Account for split transfers in the bus time. */
        int bytecount =
            comip_hsic_hb_mult(qh->maxp) * comip_hsic_max_packet(qh->maxp);

        qh->usecs =
            calc_bus_time((qh->do_split ? USB_SPEED_HIGH : dev_speed),
                  qh->ep_is_in, (qh->ep_type == UE_ISOCHRONOUS),
                  bytecount);
        /* Start in a slightly future (micro)frame. */
        qh->sched_frame = comip_hsic_frame_num_inc(hcd->frame_number,
                            SCHEDULE_SLOP);
        qh->interval = urb->interval;

        hprt.d32 = readl(hcd->core_if->host_if->hprt0);
        if ((hprt.b.prtspd == COMIP_HSIC_HPRT0_PRTSPD_HIGH_SPEED) &&
            ((dev_speed == USB_SPEED_LOW) ||
             (dev_speed == USB_SPEED_FULL))) {
            qh->interval *= 8;
            qh->sched_frame |= 0x7;
            qh->start_split_frame = qh->sched_frame;
        }

    }

    COMIP_HSIC_DEBUGPL(DBG_HCD, "COMIP OTG HCD QH Initialized\n");
    COMIP_HSIC_DEBUGPL(DBG_HCDV, "COMIP OTG HCD QH  - qh = %p\n", qh);
    COMIP_HSIC_DEBUGPL(DBG_HCDV, "COMIP OTG HCD QH  - Device Address = %d\n",
            comip_hsic_hcd_get_dev_addr(&urb->pipe_info));
    COMIP_HSIC_DEBUGPL(DBG_HCDV, "COMIP OTG HCD QH  - Endpoint %d, %s\n",
            comip_hsic_hcd_get_ep_num(&urb->pipe_info),
            comip_hsic_hcd_is_pipe_in(&urb->pipe_info) ? "IN" : "OUT");
    switch (dev_speed) {
    case USB_SPEED_LOW:
        qh->dev_speed = COMIP_HSIC_EP_SPEED_LOW;
        speed = "low";
        break;
    case USB_SPEED_FULL:
        qh->dev_speed = COMIP_HSIC_EP_SPEED_FULL;
        speed = "full";
        break;
    case USB_SPEED_HIGH:
        qh->dev_speed = COMIP_HSIC_EP_SPEED_HIGH;
        speed = "high";
        break;
    default:
        speed = "?";
        break;
    }
    COMIP_HSIC_DEBUGPL(DBG_HCDV, "COMIP OTG HCD QH  - Speed = %s\n", speed);

    switch (qh->ep_type) {
    case UE_ISOCHRONOUS:
        type = "isochronous";
        break;
    case UE_INTERRUPT:
        type = "interrupt";
        break;
    case UE_CONTROL:
        type = "control";
        break;
    case UE_BULK:
        type = "bulk";
        break;
    default:
        type = "?";
        break;
    }

    COMIP_HSIC_DEBUGPL(DBG_HCDV, "COMIP OTG HCD QH  - Type = %s\n", type);

#ifdef DEBUG
    if (qh->ep_type == UE_INTERRUPT) {
        COMIP_HSIC_DEBUGPL(DBG_HCDV, "COMIP OTG HCD QH - usecs = %d\n",
                qh->usecs);
        COMIP_HSIC_DEBUGPL(DBG_HCDV, "COMIP OTG HCD QH - interval = %d\n",
                qh->interval);
    }
#endif

}

/**
 * This function allocates and initializes a QH.
 *
 * @param hcd The HCD state structure for the COMIP OTG controller.
 * @param urb Holds the information about the device/endpoint that we need
 *        to initialize the QH.
 *
 * @return Returns pointer to the newly allocated QH, or NULL on error. */
comip_hsic_qh_t *comip_hsic_hcd_qh_create(comip_hsic_hcd_t * hcd,
                    comip_hsic_hcd_urb_t * urb, int atomic_alloc)
{
    comip_hsic_qh_t *qh;

    /* Allocate memory */
    /** @todo add memflags argument */
    qh = comip_hsic_hcd_qh_alloc(atomic_alloc);
    if (qh == NULL) {
        COMIP_HSIC_ERROR("qh allocation failed");
        return NULL;
    }

    hsic_qh_init(hcd, qh, urb);

    if (hcd->core_if->dma_desc_enable
        && (comip_hsic_hcd_qh_init_ddma(hcd, qh) < 0)) {
        comip_hsic_hcd_qh_free(hcd, qh);
        return NULL;
    }

    return qh;
}

/**
 * Checks that a channel is available for a periodic transfer.
 *
 * @return 0 if successful, negative error code otherise.
 */
static int periodic_channel_available(comip_hsic_hcd_t * hcd)
{
    /*
     * Currently assuming that there is a dedicated host channnel for each
     * periodic transaction plus at least one host channel for
     * non-periodic transactions.
     */
    int status;
    int num_channels;

    num_channels = hcd->core_if->core_params->host_channels;
    if ((hcd->periodic_channels + hcd->non_periodic_channels < num_channels)
        && (hcd->periodic_channels < num_channels - 1)) {
        status = 0;
    } else {
        COMIP_HSIC_INFO("%s: Total channels: %d, Periodic: %d, Non-periodic: %d\n",
            __func__, num_channels, hcd->periodic_channels, hcd->non_periodic_channels);    //NOTICE
        status = -ENOSPC;
    }

    return status;
}

/**
 * Checks that there is sufficient bandwidth for the specified QH in the
 * periodic schedule. For simplicity, this calculation assumes that all the
 * transfers in the periodic schedule may occur in the same (micro)frame.
 *
 * @param hcd The HCD state structure for the COMIP OTG controller.
 * @param qh QH containing periodic bandwidth required.
 *
 * @return 0 if successful, negative error code otherwise.
 */
static int check_periodic_bandwidth(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh)
{
    int status;
    int16_t max_claimed_usecs;

    status = 0;

    if ((qh->dev_speed == COMIP_HSIC_EP_SPEED_HIGH) || qh->do_split) {
        /*
         * High speed mode.
         * Max periodic usecs is 80% x 125 usec = 100 usec.
         */

        max_claimed_usecs = 100 - qh->usecs;
    } else {
        /*
         * Full speed mode.
         * Max periodic usecs is 90% x 1000 usec = 900 usec.
         */
        max_claimed_usecs = 900 - qh->usecs;
    }

    if (hcd->periodic_usecs > max_claimed_usecs) {
        COMIP_HSIC_INFO("%s: already claimed usecs %d, required usecs %d\n", __func__, hcd->periodic_usecs, qh->usecs); //NOTICE
        status = -ENOSPC;
    }

    return status;
}

/**
 * Checks that the max transfer size allowed in a host channel is large enough
 * to handle the maximum data transfer in a single (micro)frame for a periodic
 * transfer.
 *
 * @param hcd The HCD state structure for the COMIP OTG controller.
 * @param qh QH for a periodic endpoint.
 *
 * @return 0 if successful, negative error code otherwise.
 */
static int check_max_xfer_size(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh)
{
    int status;
    uint32_t max_xfer_size;
    uint32_t max_channel_xfer_size;

    status = 0;

    max_xfer_size = comip_hsic_max_packet(qh->maxp) * comip_hsic_hb_mult(qh->maxp);
    max_channel_xfer_size = hcd->core_if->core_params->max_transfer_size;

    if (max_xfer_size > max_channel_xfer_size) {
        COMIP_HSIC_INFO("%s: Periodic xfer length %d > " "max xfer length for channel %d\n",
                __func__, max_xfer_size, max_channel_xfer_size);    //NOTICE
        status = -ENOSPC;
    }

    return status;
}

/**
 * Schedules an interrupt or isochronous transfer in the periodic schedule.
 *
 * @param hcd The HCD state structure for the COMIP OTG controller.
 * @param qh QH for the periodic transfer. The QH should already contain the
 * scheduling information.
 *
 * @return 0 if successful, negative error code otherwise.
 */
static int schedule_periodic(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh)
{
    int status = 0;

    status = periodic_channel_available(hcd);
    if (status) {
        COMIP_HSIC_INFO("%s: No host channel available for periodic " "transfer.\n", __func__); //NOTICE
        return status;
    }

    status = check_periodic_bandwidth(hcd, qh);
    if (status) {
        COMIP_HSIC_INFO("%s: Insufficient periodic bandwidth for " "periodic transfer.\n", __func__);   //NOTICE
        return status;
    }

    status = check_max_xfer_size(hcd, qh);
    if (status) {
        COMIP_HSIC_INFO("%s: Channel max transfer size too small " "for periodic transfer.\n", __func__);   //NOTICE
        return status;
    }

    if (hcd->core_if->dma_desc_enable) {
        /* Don't rely on SOF and start in ready schedule */
        COMIP_HSIC_LIST_INSERT_TAIL(&hcd->periodic_sched_ready, &qh->qh_list_entry);
    }
    else {
    /* Always start in the inactive schedule. */
    COMIP_HSIC_LIST_INSERT_TAIL(&hcd->periodic_sched_inactive, &qh->qh_list_entry);
    }

    /* Reserve the periodic channel. */
    hcd->periodic_channels++;

    /* Update claimed usecs per (micro)frame. */
    hcd->periodic_usecs += qh->usecs;

    return status;
}

/**
 * This function adds a QH to either the non periodic or periodic schedule if
 * it is not already in the schedule. If the QH is already in the schedule, no
 * action is taken.
 *
 * @return 0 if successful, negative error code otherwise.
 */
int comip_hsic_hcd_qh_add(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh)
{
    int status = 0;

    if (!COMIP_HSIC_LIST_EMPTY(&qh->qh_list_entry)) {
        /* QH already in a schedule. */
        return status;
    }

    /* Add the new QH to the appropriate schedule */
    if (comip_hsic_qh_is_non_per(qh)) {
        /* Always start in the inactive schedule. */
        COMIP_HSIC_LIST_INSERT_TAIL(&hcd->non_periodic_sched_inactive,
                     &qh->qh_list_entry);
    } else {
        status = schedule_periodic(hcd, qh);
    }

    return status;
}

/**
 * Removes an interrupt or isochronous transfer from the periodic schedule.
 *
 * @param hcd The HCD state structure for the COMIP OTG controller.
 * @param qh QH for the periodic transfer.
 */
static void deschedule_periodic(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh)
{
    COMIP_HSIC_LIST_REMOVE_INIT(&qh->qh_list_entry);

    /* Release the periodic channel reservation. */
    hcd->periodic_channels--;

    /* Update claimed usecs per (micro)frame. */
    hcd->periodic_usecs -= qh->usecs;
}

/** 
 * Removes a QH from either the non-periodic or periodic schedule.  Memory is
 * not freed.
 *
 * @param hcd The HCD state structure.
 * @param qh QH to remove from schedule. */
void comip_hsic_hcd_qh_remove(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh)
{
    if (COMIP_HSIC_LIST_EMPTY(&qh->qh_list_entry)) {
        /* QH is not in a schedule. */
        return;
    }

    if (comip_hsic_qh_is_non_per(qh)) {
        if (hcd->non_periodic_qh_ptr == &qh->qh_list_entry) {
            hcd->non_periodic_qh_ptr =
                hcd->non_periodic_qh_ptr->next;
        }
        COMIP_HSIC_LIST_REMOVE_INIT(&qh->qh_list_entry);
    } else {
        deschedule_periodic(hcd, qh);
    }
}

/**
 * Deactivates a QH. For non-periodic QHs, removes the QH from the active
 * non-periodic schedule. The QH is added to the inactive non-periodic
 * schedule if any QTDs are still attached to the QH.
 *
 * For periodic QHs, the QH is removed from the periodic queued schedule. If
 * there are any QTDs still attached to the QH, the QH is added to either the
 * periodic inactive schedule or the periodic ready schedule and its next
 * scheduled frame is calculated. The QH is placed in the ready schedule if
 * the scheduled frame has been reached already. Otherwise it's placed in the
 * inactive schedule. If there are no QTDs attached to the QH, the QH is
 * completely removed from the periodic schedule.
 */
void comip_hsic_hcd_qh_deactivate(comip_hsic_hcd_t * hcd, comip_hsic_qh_t * qh,
                   int sched_next_periodic_split)
{
    if (comip_hsic_qh_is_non_per(qh)) {
        comip_hsic_hcd_qh_remove(hcd, qh);
        if (!COMIP_HSIC_CIRCLEQ_EMPTY(&qh->qtd_list)) {
            /* Add back to inactive non-periodic schedule. */
            comip_hsic_hcd_qh_add(hcd, qh);
        }
    } else {
        uint16_t frame_number = comip_hsic_hcd_get_frame_number(hcd);

        if (qh->do_split) {
            /* Schedule the next continuing periodic split transfer */
            if (sched_next_periodic_split) {

                qh->sched_frame = frame_number;
                if (comip_hsic_frame_num_le(frame_number,
                             comip_hsic_frame_num_inc
                             (qh->start_split_frame,
                              1))) {
                    /*
                     * Allow one frame to elapse after start
                     * split microframe before scheduling
                     * complete split, but DONT if we are
                     * doing the next start split in the
                     * same frame for an ISOC out.
                     */
                    if ((qh->ep_type != UE_ISOCHRONOUS) ||
                        (qh->ep_is_in != 0)) {
                        qh->sched_frame =
                            comip_hsic_frame_num_inc(qh->sched_frame, 1);
                    }
                }
            } else {
                qh->sched_frame =
                    comip_hsic_frame_num_inc(qh->start_split_frame,
                              qh->interval);
                if (comip_hsic_frame_num_le
                    (qh->sched_frame, frame_number)) {
                    qh->sched_frame = frame_number;
                }
                qh->sched_frame |= 0x7;
                qh->start_split_frame = qh->sched_frame;
            }
        } else {
            qh->sched_frame =
                comip_hsic_frame_num_inc(qh->sched_frame, qh->interval);
            if (comip_hsic_frame_num_le(qh->sched_frame, frame_number)) {
                qh->sched_frame = frame_number;
            }
        }

        if (COMIP_HSIC_CIRCLEQ_EMPTY(&qh->qtd_list)) {
            comip_hsic_hcd_qh_remove(hcd, qh);
        } else {
            /*
             * Remove from periodic_sched_queued and move to
             * appropriate queue.
             */
            if (qh->sched_frame == frame_number) {
                COMIP_HSIC_LIST_MOVE_HEAD(&hcd->periodic_sched_ready,
                           &qh->qh_list_entry);
            } else {
                COMIP_HSIC_LIST_MOVE_HEAD
                    (&hcd->periodic_sched_inactive,
                     &qh->qh_list_entry);
            }
        }
    }
}

/** 
 * This function allocates and initializes a QTD. 
 *
 * @param urb The URB to create a QTD from.  Each URB-QTD pair will end up
 *        pointing to each other so each pair should have a unique correlation.
 *
 * @return Returns pointer to the newly allocated QTD, or NULL on error. */
comip_hsic_qtd_t *comip_hsic_hcd_qtd_create(comip_hsic_hcd_urb_t * urb, int atomic_alloc)
{
    comip_hsic_qtd_t *qtd;

    qtd = comip_hsic_hcd_qtd_alloc(atomic_alloc);
    if (qtd == NULL) {
        return NULL;
    }

    comip_hsic_hcd_qtd_init(qtd, urb);
    return qtd;
}

/** 
 * Initializes a QTD structure.
 *
 * @param qtd The QTD to initialize.
 * @param urb The URB to use for initialization.  */
void comip_hsic_hcd_qtd_init(comip_hsic_qtd_t * qtd, comip_hsic_hcd_urb_t * urb)
{
    memset(qtd, 0, sizeof(comip_hsic_qtd_t));
    qtd->urb = urb;
    if (comip_hsic_hcd_get_pipe_type(&urb->pipe_info) == UE_CONTROL) {
        /*
         * The only time the QTD data toggle is used is on the data
         * phase of control transfers. This phase always starts with
         * DATA1.
         */
        qtd->data_toggle = COMIP_HSIC_HC_PID_DATA1;
        qtd->control_phase = COMIP_HSIC_CONTROL_SETUP;
    }

    /* start split */
    qtd->complete_split = 0;
    qtd->isoc_split_pos = COMIP_HSIC_HCSPLIT_XACTPOS_ALL;
    qtd->isoc_split_offset = 0;
    qtd->in_process = 0;

    /* Store the qtd ptr in the urb to reference what QTD. */
    urb->qtd = qtd;
    return;
}

/**
 * This function adds a QTD to the QTD-list of a QH.  It will find the correct
 * QH to place the QTD into.  If it does not find a QH, then it will create a
 * new QH. If the QH to which the QTD is added is not currently scheduled, it
 * is placed into the proper schedule based on its EP type.
 *
 * @param[in] qtd The QTD to add
 * @param[in] hcd The COMIP HCD structure
 * @param[out] qh out parameter to return queue head
 *
 * @return 0 if successful, negative error code otherwise.
 */
int comip_hsic_hcd_qtd_add(comip_hsic_qtd_t * qtd,
            comip_hsic_hcd_t * hcd, comip_hsic_qh_t ** qh, int atomic_alloc)
{
    int retval = 0;
    comip_hsic_irqflags_t flags;

    comip_hsic_hcd_urb_t *urb = qtd->urb;

    /*
     * Get the QH which holds the QTD-list to insert to. Create QH if it
     * doesn't exist.
     */
    if (*qh == NULL) {
        *qh = comip_hsic_hcd_qh_create(hcd, urb, atomic_alloc);
        if (*qh == NULL) {
            retval = -1;
            goto done;
        }
    }
    COMIP_HSIC_SPINLOCK_IRQSAVE(hcd->lock, &flags);
    retval = comip_hsic_hcd_qh_add(hcd, *qh);
    if (retval == 0) {
        COMIP_HSIC_CIRCLEQ_INSERT_TAIL(&((*qh)->qtd_list), qtd,
                    qtd_list_entry);
    }
    COMIP_HSIC_SPINUNLOCK_IRQRESTORE(hcd->lock, flags);

done:

    return retval;
}
