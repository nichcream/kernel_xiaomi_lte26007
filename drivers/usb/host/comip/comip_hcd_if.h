/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/comip_hcd_if.h $
 * $Revision: #11 $
 * $Date: 2011/05/17 $
 * $Change: 1774110 $
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
#ifndef __COMIP_HCD_IF_H__
#define __COMIP_HCD_IF_H__

#include "comip_core_if.h"

/** @file
 * This file defines COMIP HCD Core API.
 */

struct comip_hcd;
typedef struct comip_hcd comip_hcd_t;

struct comip_hcd_urb;
typedef struct comip_hcd_urb comip_hcd_urb_t;

/** @name HCD Function Driver Callbacks */
/** @{ */

/** This function is called whenever core switches to host mode. */
typedef int (*comip_hcd_start_cb_t) (comip_hcd_t * hcd);

/** This function is called when device has been disconnected */
typedef int (*comip_hcd_disconnect_cb_t) (comip_hcd_t * hcd);

/** Wrapper provides this function to HCD to core, so it can get hub information to which device is connected */
typedef int (*comip_hcd_hub_info_from_urb_cb_t) (comip_hcd_t * hcd,
    void *urb_handle, uint32_t * hub_addr, uint32_t * port_addr);
/** Via this function HCD core gets device speed */
typedef int (*comip_hcd_speed_from_urb_cb_t) (comip_hcd_t * hcd,
    void *urb_handle);

/** This function is called when urb is completed */
typedef int (*comip_hcd_complete_urb_cb_t) (comip_hcd_t * hcd,
    void *urb_handle, comip_hcd_urb_t * comip_urb, int32_t status);

/** Via this function HCD core gets b_hnp_enable parameter */
typedef int (*comip_hcd_get_b_hnp_enable) (comip_hcd_t * hcd);

struct comip_hcd_function_ops {
    comip_hcd_start_cb_t start;
    comip_hcd_disconnect_cb_t disconnect;
    comip_hcd_hub_info_from_urb_cb_t hub_info;
    comip_hcd_speed_from_urb_cb_t speed;
    comip_hcd_complete_urb_cb_t complete;
    comip_hcd_get_b_hnp_enable get_b_hnp_enable;
};
/** @} */

/** @name HCD Core API */
/** @{ */
/** This function allocates comip_hcd structure and returns pointer on it. */
extern comip_hcd_t *comip_hcd_alloc_hcd(void);

/** This function should be called to initiate HCD Core.
 *
 * @param hcd The HCD
 * @param core_if The COMIP Core
 *
 * Returns -ENOMEM if no enough memory.
 * Returns 0 on success  
 */
extern int comip_hcd_init(comip_hcd_t * hcd, comip_core_if_t * core_if);

/** Frees HCD
 *
 * @param hcd The HCD
 */
extern void comip_hcd_remove(comip_hcd_t * hcd);

/** This function should be called on every hardware interrupt.
 *
 * @param comip_hcd The HCD
 *
 * Returns non zero if interrupt is handled
 * Return 0 if interrupt is not handled
 */
extern int32_t comip_hcd_handle_intr(comip_hcd_t * comip_hcd);

/**
 * Returns private data set by
 * comip_hcd_set_priv_data function.
 *
 * @param hcd The HCD
 */
extern void *comip_hcd_get_priv_data(comip_hcd_t * hcd);

/**
 * Set private data.
 *
 * @param hcd The HCD
 * @param priv_data pointer to be stored in private data
 */
extern void comip_hcd_set_priv_data(comip_hcd_t * hcd, void *priv_data);

/**
 * This function initializes the HCD Core.
 *
 * @param hcd The HCD
 * @param fops The Function Driver Operations data structure containing pointers to all callbacks.
 *
 * Returns -ENODEV if Core is currently is in device mode.
 * Returns 0 on success
 */
extern int comip_hcd_start(comip_hcd_t * hcd,
    struct comip_hcd_function_ops *fops);

/**
 * Halts the comip host mode operations in a clean manner. USB transfers are
 * stopped. 
 *
 * @param hcd The HCD
 */
extern void comip_hcd_stop(comip_hcd_t * hcd);

/**
 * Handles hub class-specific requests.
 *
 * @param comip_hcd The HCD
 * @param typeReq Request Type
 * @param wValue wValue from control request
 * @param wIndex wIndex from control request
 * @param buf data buffer 
 * @param wLength data buffer length
 *
 * Returns -EINVAL if invalid argument is passed
 * Returns 0 on success
 */
extern int comip_hcd_hub_control(comip_hcd_t * comip_hcd,
    uint16_t typeReq, uint16_t wValue, uint16_t wIndex, uint8_t * buf, uint16_t wLength);

/**
 * Returns otg port number.
 *
 * @param hcd The HCD
 */
extern uint32_t comip_hcd_otg_port(comip_hcd_t * hcd);

/**
 * Returns OTG version - either 1.3 or 2.0.
 *
 * @param hcd The HCD
 */
extern uint16_t comip_get_otg_version(comip_core_if_t * core_if);

/**
 * Returns 1 if currently core is acting as B host, and 0 otherwise.
 *
 * @param hcd The HCD
 */
extern uint32_t comip_hcd_is_b_host(comip_hcd_t * hcd);

/**
 * Returns current frame number.
 *
 * @param hcd The HCD
 */
extern int comip_hcd_get_frame_number(comip_hcd_t * hcd);

/**
 * Dumps hcd state.
 *
 * @param hcd The HCD
 */
extern void comip_hcd_dump_state(comip_hcd_t * hcd);

/**
 * Dump the average frame remaining at SOF. This can be used to
 * determine average interrupt latency. Frame remaining is also shown for
 * start transfer and two additional sample points.
 * Currently this function is not implemented.
 *
 * @param hcd The HCD
 */
extern void comip_hcd_dump_frrem(comip_hcd_t * hcd);

/**
 * Sends LPM transaction to the local device.
 *
 * @param hcd The HCD
 * @param devaddr Device Address
 * @param hird Host initiated resume duration
 * @param bRemoteWake Value of bRemoteWake field in LPM transaction
 *
 * Returns negative value if sending LPM transaction was not succeeded.
 * Returns 0 on success.
 */
extern int comip_hcd_send_lpm(comip_hcd_t * hcd, uint8_t devaddr,
    uint8_t hird, uint8_t bRemoteWake);

/* URB interface */

/**
 * Allocates memory for comip_hcd_urb structure.
 * Allocated memory should be freed by call of kfree.
 *
 * @param hcd The HCD
 * @param iso_desc_count Count of ISOC descriptors
 * @param atomic_alloc Specefies whether to perform atomic allocation.
 */
extern comip_hcd_urb_t *comip_hcd_urb_alloc(comip_hcd_t * hcd,
    int iso_desc_count, int atomic_alloc);

/**
 * Set pipe information in URB.
 *
 * @param hcd_urb comip URB
 * @param devaddr Device Address
 * @param ep_num Endpoint Number
 * @param ep_type Endpoint Type
 * @param ep_dir Endpoint Direction
 * @param mps Max Packet Size
 */
extern void comip_hcd_urb_set_pipeinfo(comip_hcd_urb_t * hcd_urb,
    uint8_t devaddr, uint8_t ep_num,
    uint8_t ep_type, uint8_t ep_dir, uint16_t mps);

/* Transfer flags */
#define URB_GIVEBACK_ASAP 0x1
#define URB_SEND_ZERO_PACKET 0x2

/**
 * Sets comip_hcd_urb parameters.
 *
 * @param urb comip URB allocated by comip_hcd_urb_alloc function.
 * @param urb_handle Unique handle for request, this will be passed back
 * to function driver in completion callback.
 * @param buf The buffer for the data
 * @param dma The DMA buffer for the data
 * @param buflen Transfer length
 * @param sp Buffer for setup data
 * @param sp_dma DMA address of setup data buffer
 * @param flags Transfer flags
 * @param interval Polling interval for interrupt or isochronous transfers.
 */
extern void comip_hcd_urb_set_params(comip_hcd_urb_t * urb,
    void *urb_handle, void *buf,
    comip_dma_t dma, uint32_t buflen, void *sp,
    comip_dma_t sp_dma, uint32_t flags,
    uint16_t interval);

/** Gets status from comip_hcd_urb
 *
 * @param comip_urb comip URB
 */
extern uint32_t comip_hcd_urb_get_status(comip_hcd_urb_t * comip_urb);

/** Gets actual length from comip_hcd_urb
 *
 * @param comip_urb comip URB
 */
extern uint32_t comip_hcd_urb_get_actual_length(comip_hcd_urb_t *
    comip_urb);

/** Gets error count from comip_hcd_urb. Only for ISOC URBs
 *
 * @param comip_urb comip URB
 */
extern uint32_t comip_hcd_urb_get_error_count(comip_hcd_urb_t *
    comip_urb);

/** Set ISOC descriptor offset and length
 *
 * @param comip_urb comip URB
 * @param desc_num ISOC descriptor number
 * @param offset Offset from beginig of buffer.
 * @param length Transaction length
 */
extern void comip_hcd_urb_set_iso_desc_params(comip_hcd_urb_t * comip_urb,
    int desc_num, uint32_t offset, uint32_t length);

/** Get status of ISOC descriptor, specified by desc_num
 *
 * @param comip_urb comip URB
 * @param desc_num ISOC descriptor number 
 */
extern uint32_t comip_hcd_urb_get_iso_desc_status(comip_hcd_urb_t *
    comip_urb, int desc_num);

/** Get actual length of ISOC descriptor, specified by desc_num
 *
 * @param comip_urb comip URB
 * @param desc_num ISOC descriptor number
 */
extern uint32_t comip_hcd_urb_get_iso_desc_actual_length(comip_hcd_urb_t *
    comip_urb, int desc_num);

/** Queue URB. After transfer is completes, the complete callback will be called with the URB status
 *
 * @param comip_hcd The HCD
 * @param comip_urb comip URB
 * @param ep_handle Out parameter for returning endpoint handle
 *
 * Returns -ENODEV if no device is connected.
 * Returns -ENOMEM if there is no enough memory.
 * Returns 0 on success.
 */
extern int comip_hcd_urb_enqueue(comip_hcd_t * comip_hcd,
    comip_hcd_urb_t * comip_urb, void **ep_handle, int atomic_alloc);

/** De-queue the specified URB
 *
 * @param comip_hcd The HCD
 * @param comip_urb comip URB
 */
extern int comip_hcd_urb_dequeue(comip_hcd_t * comip_hcd,
    comip_hcd_urb_t * comip_urb);

/** Frees resources in the comip controller related to a given endpoint.
 * Any URBs for the endpoint must already be dequeued.
 *
 * @param hcd The HCD
 * @param ep_handle Endpoint handle, returned by comip_hcd_urb_enqueue function
 * @param retry Number of retries if there are queued transfers.
 *
 * Returns -EINVAL if invalid arguments are passed.
 * Returns 0 on success
 */
extern int comip_hcd_endpoint_disable(comip_hcd_t * hcd, void *ep_handle,
    int retry);

/* Resets the data toggle in qh structure. This function can be called from
 * usb_clear_halt routine.
 *
 * @param hcd The HCD
 * @param ep_handle Endpoint handle, returned by comip_hcd_urb_enqueue function
 *
 * Returns -EINVAL if invalid arguments are passed.
 * Returns 0 on success
 */
extern int comip_hcd_endpoint_reset(comip_hcd_t * hcd, void *ep_handle);

/** Returns 1 if status of specified port is changed and 0 otherwise.
 *
 * @param hcd The HCD
 * @param port Port number
 */
extern int comip_hcd_is_status_changed(comip_hcd_t * hcd, int port);

/** Call this function to check if bandwidth was allocated for specified endpoint.
 * Only for ISOC and INTERRUPT endpoints.
 *
 * @param hcd The HCD
 * @param ep_handle Endpoint handle
 */
extern int comip_hcd_is_bandwidth_allocated(comip_hcd_t * hcd,
    void *ep_handle);

/** Call this function to check if bandwidth was freed for specified endpoint.
 *
 * @param hcd The HCD
 * @param ep_handle Endpoint handle
 */
extern int comip_hcd_is_bandwidth_freed(comip_hcd_t * hcd, void *ep_handle);

/** Returns bandwidth allocated for specified endpoint in microseconds.
 * Only for ISOC and INTERRUPT endpoints.
 *
 * @param hcd The HCD
 * @param ep_handle Endpoint handle
 */
extern uint8_t comip_hcd_get_ep_bandwidth(comip_hcd_t * hcd,
    void *ep_handle);

/** @} */

#endif /* __COMIP_HCD_IF_H__ */
