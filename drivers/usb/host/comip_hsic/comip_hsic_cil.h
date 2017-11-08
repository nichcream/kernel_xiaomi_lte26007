/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/comip_hsic_cil.h $
 * $Revision: #118 $
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

#if !defined(__COMIP_HSIC_CIL_H__)
#define __COMIP_HSIC_CIL_H__

#include "comip_hsic_list.h"
#include "comip_hsic_hcd_dbg.h"
#include "comip_hsic_otg_regs.h"
#include <plat/usb.h>
# include <linux/irq.h>

/**
 * @file
 * This file contains the interface to the Core Interface Layer.
 */

#ifdef COMIP_HSIC_UTE_CFI

#define MAX_DMA_DESCS_PER_EP    256

/**
 * Enumeration for the data buffer mode
 */
typedef enum _data_buffer_mode {
    BM_STANDARD = 0,    /* data buffer is in normal mode */
    BM_SG = 1,      /* data buffer uses the scatter/gather mode */
    BM_CONCAT = 2,      /* data buffer uses the concatenation mode */
    BM_CIRCULAR = 3,    /* data buffer uses the circular DMA mode */
    BM_ALIGN = 4        /* data buffer is in buffer alignment mode */
} data_buffer_mode_e;
#endif //COMIP_HSIC_UTE_CFI

/** Macros defined for COMIP OTG HW Release version */

#define OTG_CORE_REV_2_60a  0x4F54260A
#define OTG_CORE_REV_2_71a  0x4F54271A
#define OTG_CORE_REV_2_72a  0x4F54272A
#define OTG_CORE_REV_2_80a  0x4F54280A
#define OTG_CORE_REV_2_81a  0x4F54281A
#define OTG_CORE_REV_2_90a  0x4F54290A
#define OTG_CORE_REV_2_91a  0x4F54291A
#define OTG_CORE_REV_2_92a  0x4F54292A
#define OTG_CORE_REV_2_93a  0x4F54293A

/**
 * Information for each ISOC packet.
 */
typedef struct iso_pkt_info {
    uint32_t offset;
    uint32_t length;
    int32_t status;
} iso_pkt_info_t;

/**
 * The <code>comip_hsic_ep</code> structure represents the state of a single
 * endpoint when acting in device mode. It contains the data items
 * needed for an endpoint to be activated and transfer packets.
 */
typedef struct comip_hsic_ep {
    /** EP number used for register address lookup */
    uint8_t num;
    /** EP direction 0 = OUT */
    uint32_t is_in:1;
    /** EP active. */
    uint32_t active:1;

    /**
     *Periodic Tx FIFO # for IN EPs For INTR EP set to 0 to use non-periodic Tx FIFO
     * If dedicated Tx FIFOs are enabled for all IN Eps - Tx FIFO # FOR IN EPs*/
    uint32_t tx_fifo_num:4;
    /** EP type: 0 - Control, 1 - ISOC,  2 - BULK,  3 - INTR */
    uint32_t type:2;
#define COMIP_HSIC_EP_TYPE_CONTROL     0
#define COMIP_HSIC_EP_TYPE_ISOC    1
#define COMIP_HSIC_EP_TYPE_BULK    2
#define COMIP_HSIC_EP_TYPE_INTR    3

    /** DATA start PID for INTR and BULK EP */
    uint32_t data_pid_start:1;
    /** Frame (even/odd) for ISOC EP */
    uint32_t even_odd_frame:1;
    /** Max Packet bytes */
    uint32_t maxpacket:11;

    /** Max Transfer size */
    uint32_t maxxfer;

    /** @name Transfer state */
    /** @{ */

    /**
     * Pointer to the beginning of the transfer buffer -- do not modify
     * during transfer.
     */

    comip_hsic_dma_t dma_addr;

    comip_hsic_dma_t dma_desc_addr;
    comip_hsic_dev_dma_desc_t *desc_addr;

    uint8_t *start_xfer_buff;
    /** pointer to the transfer buffer */
    uint8_t *xfer_buff;
    /** Number of bytes to transfer */
    unsigned xfer_len:19;
    /** Number of bytes transferred. */
    unsigned xfer_count:19;
    /** Sent ZLP */
    unsigned sent_zlp:1;
    /** Total len for control transfer */
    unsigned total_len:19;

    /** stall clear flag */
    unsigned stall_clear_flag:1;

#ifdef COMIP_HSIC_UTE_CFI
    /* The buffer mode */
    data_buffer_mode_e buff_mode;

    /* The chain of DMA descriptors.
     * MAX_DMA_DESCS_PER_EP will be allocated for each active EP.
     */
    comip_hsic_dev_dma_desc_t *descs;

    /* The DMA address of the descriptors chain start */
    dma_addr_t descs_dma_addr;
    /** This variable stores the length of the last enqueued request */
    uint32_t cfi_req_len;
#endif              //COMIP_HSIC_UTE_CFI

    /** Allocated DMA Desc count */
    uint32_t desc_cnt;

#ifdef COMIP_HSIC_UTE_PER_IO
    /** Next frame num for which will be setup DMA Desc */
    uint32_t xiso_frame_num;
    /** bInterval */
    uint32_t xiso_bInterval;
    /** Count of currently active transfers - shall be either 0 or 1 */
    int xiso_active_xfers;
    int xiso_queued_xfers;
#endif
#ifdef COMIP_HSIC_EN_ISOC
    /**
     * Variables specific for ISOC EPs
     *
     */
    /** DMA addresses of ISOC buffers */
    comip_hsic_dma_t dma_addr0;
    comip_hsic_dma_t dma_addr1;

    comip_hsic_dma_t iso_dma_desc_addr;
    comip_hsic_dev_dma_desc_t *iso_desc_addr;

    /** pointer to the transfer buffers */
    uint8_t *xfer_buff0;
    uint8_t *xfer_buff1;

    /** number of ISOC Buffer is processing */
    uint32_t proc_buf_num;
    /** Interval of ISOC Buffer processing */
    uint32_t buf_proc_intrvl;
    /** Data size for regular frame */
    uint32_t data_per_frame;

    /* todo - pattern data support is to be implemented in the future */
    /** Data size for pattern frame */
    uint32_t data_pattern_frame;
    /** Frame number of pattern data */
    uint32_t sync_frame;

    /** bInterval */
    uint32_t bInterval;
    /** ISO Packet number per frame */
    uint32_t pkt_per_frm;
    /** Next frame num for which will be setup DMA Desc */
    uint32_t next_frame;
    /** Number of packets per buffer processing */
    uint32_t pkt_cnt;
    /** Info for all isoc packets */
    iso_pkt_info_t *pkt_info;
    /** current pkt number */
    uint32_t cur_pkt;
    /** current pkt number */
    uint8_t *cur_pkt_addr;
    /** current pkt number */
    uint32_t cur_pkt_dma_addr;
#endif              /* COMIP_HSIC_EN_ISOC */

/** @} */
} comip_hsic_ep_t;

/*
 * Reasons for halting a host channel.
 */
typedef enum comip_hsic_halt_status {
    COMIP_HSIC_HC_XFER_NO_HALT_STATUS,
    COMIP_HSIC_HC_XFER_COMPLETE,
    COMIP_HSIC_HC_XFER_URB_COMPLETE,
    COMIP_HSIC_HC_XFER_ACK,
    COMIP_HSIC_HC_XFER_NAK,
    COMIP_HSIC_HC_XFER_NYET,
    COMIP_HSIC_HC_XFER_STALL,
    COMIP_HSIC_HC_XFER_XACT_ERR,
    COMIP_HSIC_HC_XFER_FRAME_OVERRUN,
    COMIP_HSIC_HC_XFER_BABBLE_ERR,
    COMIP_HSIC_HC_XFER_DATA_TOGGLE_ERR,
    COMIP_HSIC_HC_XFER_AHB_ERR,
    COMIP_HSIC_HC_XFER_PERIODIC_INCOMPLETE,
    COMIP_HSIC_HC_XFER_URB_DEQUEUE
} comip_hsic_halt_status_e;

/**
 * Host channel descriptor. This structure represents the state of a single
 * host channel when acting in host mode. It contains the data items needed to
 * transfer packets to an endpoint via a host channel.
 */
typedef struct comip_hsic_hc {
    /** Host channel number used for register address lookup */
    uint8_t hc_num;

    /** Device to access */
    unsigned dev_addr:7;

    /** EP to access */
    unsigned ep_num:4;

    /** EP direction. 0: OUT, 1: IN */
    unsigned ep_is_in:1;

    /**
     * EP speed.
     * One of the following values:
     *  - COMIP_HSIC_EP_SPEED_LOW
     *  - COMIP_HSIC_EP_SPEED_FULL
     *  - COMIP_HSIC_EP_SPEED_HIGH
     */
    unsigned speed:2;
#define COMIP_HSIC_EP_SPEED_LOW 0
#define COMIP_HSIC_EP_SPEED_FULL    1
#define COMIP_HSIC_EP_SPEED_HIGH    2

    /**
     * Endpoint type.
     * One of the following values:
     *  - COMIP_HSIC_EP_TYPE_CONTROL: 0
     *  - COMIP_HSIC_EP_TYPE_ISOC: 1
     *  - COMIP_HSIC_EP_TYPE_BULK: 2
     *  - COMIP_HSIC_EP_TYPE_INTR: 3
     */
    unsigned ep_type:2;

    /** Max packet size in bytes */
    unsigned max_packet:11;

    /**
     * PID for initial transaction.
     * 0: DATA0,<br>
     * 1: DATA2,<br>
     * 2: DATA1,<br>
     * 3: MDATA (non-Control EP),
     *    SETUP (Control EP)
     */
    unsigned data_pid_start:2;
#define COMIP_HSIC_HC_PID_DATA0 0
#define COMIP_HSIC_HC_PID_DATA2 1
#define COMIP_HSIC_HC_PID_DATA1 2
#define COMIP_HSIC_HC_PID_MDATA 3
#define COMIP_HSIC_HC_PID_SETUP 3

    /** Number of periodic transactions per (micro)frame */
    unsigned multi_count:2;

    /** @name Transfer State */
    /** @{ */

    /** Pointer to the current transfer buffer position. */
    uint8_t *xfer_buff;
    /**
     * In Buffer DMA mode this buffer will be used
     * if xfer_buff is not DWORD aligned.
     */
    comip_hsic_dma_t align_buff;
    /** Total number of bytes to transfer. */
    uint32_t xfer_len;
    /** Number of bytes transferred so far. */
    uint32_t xfer_count;
    /** Packet count at start of transfer.*/
    uint16_t start_pkt_count;

    /**
     * Flag to indicate whether the transfer has been started. Set to 1 if
     * it has been started, 0 otherwise.
     */
    uint8_t xfer_started;

    /**
     * Set to 1 to indicate that a PING request should be issued on this
     * channel. If 0, process normally.
     */
    uint8_t do_ping;

    /**
     * Set to 1 to indicate that the error count for this transaction is
     * non-zero. Set to 0 if the error count is 0.
     */
    uint8_t error_state;

    /**
     * Set to 1 to indicate that this channel should be halted the next
     * time a request is queued for the channel. This is necessary in
     * slave mode if no request queue space is available when an attempt
     * is made to halt the channel.
     */
    uint8_t halt_on_queue;

    /**
     * Set to 1 if the host channel has been halted, but the core is not
     * finished flushing queued requests. Otherwise 0.
     */
    uint8_t halt_pending;

    /**
     * Reason for halting the host channel.
     */
    comip_hsic_halt_status_e halt_status;

    /*
     * Split settings for the host channel
     */
    uint8_t do_split;          /**< Enable split for the channel */
    uint8_t complete_split;    /**< Enable complete split */
    uint8_t hub_addr;          /**< Address of high speed hub */

    uint8_t port_addr;         /**< Port of the low/full speed device */
    /** Split transaction position
     * One of the following values:
     *    - COMIP_HSIC_HCSPLIT_XACTPOS_MID
     *    - COMIP_HSIC_HCSPLIT_XACTPOS_BEGIN
     *    - COMIP_HSIC_HCSPLIT_XACTPOS_END
     *    - COMIP_HSIC_HCSPLIT_XACTPOS_ALL */
    uint8_t xact_pos;

    /** Set when the host channel does a short read. */
    uint8_t short_read;

    /**
     * Number of requests issued for this channel since it was assigned to
     * the current transfer (not counting PINGs).
     */
    uint8_t requests;

    /**
     * Queue Head for the transfer being processed by this channel.
     */
    struct comip_hsic_qh *qh;

    /** @} */

    /** Entry in list of host channels. */
     COMIP_HSIC_CIRCLEQ_ENTRY(comip_hsic_hc) hc_list_entry;

    /** @name Descriptor DMA support */
    /** @{ */

    /** Number of Transfer Descriptors */
    uint16_t ntd;

    /** Descriptor List DMA address */
    comip_hsic_dma_t desc_list_addr;

    /** Scheduling micro-frame bitmap. */
    uint8_t schinfo;

    /** @} */
} comip_hsic_hc_t;

/**
 * The following parameters may be specified when starting the module. These
 * parameters define how the COMIP_HSIC_otg controller should be configured.
 */
typedef struct comip_hsic_core_params {
    int32_t opt;

    /**
     * Specifies the OTG capabilities. The driver will automatically
     * detect the value for this parameter if none is specified.
     * 0 - HNP and SRP capable (default)
     * 1 - SRP Only capable
     * 2 - No HNP/SRP capable
     */
    int32_t otg_cap;

    /**
     * Specifies whether to use slave or DMA mode for accessing the data
     * FIFOs. The driver will automatically detect the value for this
     * parameter if none is specified.
     * 0 - Slave
     * 1 - DMA (default, if available)
     */
    int32_t dma_enable;

    /**
     * When DMA mode is enabled specifies whether to use address DMA or DMA Descritor mode for accessing the data
     * FIFOs in device mode. The driver will automatically detect the value for this
     * parameter if none is specified.
     * 0 - address DMA
     * 1 - DMA Descriptor(default, if available)
     */
    int32_t dma_desc_enable;
    /** The DMA Burst size (applicable only for External DMA
     * Mode). 1, 4, 8 16, 32, 64, 128, 256 (default 32)
     */
    int32_t dma_burst_size; /* Translate this to GAHBCFG values */

    /**
     * Specifies the maximum speed of operation in host and device mode.
     * The actual speed depends on the speed of the attached device and
     * the value of phy_type. The actual speed depends on the speed of the
     * attached device.
     * 0 - High Speed (default)
     * 1 - Full Speed
     */
    int32_t speed;
    /** Specifies whether low power mode is supported when attached
     *  to a Full Speed or Low Speed device in host mode.
     * 0 - Don't support low power mode (default)
     * 1 - Support low power mode
     */
    int32_t host_support_fs_ls_low_power;

    /** Specifies the PHY clock rate in low power mode when connected to a
     * Low Speed device in host mode. This parameter is applicable only if
     * HOST_SUPPORT_FS_LS_LOW_POWER is enabled. If PHY_TYPE is set to FS
     * then defaults to 6 MHZ otherwise 48 MHZ.
     *
     * 0 - 48 MHz
     * 1 - 6 MHz
     */
    int32_t host_ls_low_power_phy_clk;

    /**
     * 0 - Use cC FIFO size parameters
     * 1 - Allow dynamic FIFO sizing (default)
     */
    int32_t enable_dynamic_fifo;

    /** Total number of 4-byte words in the data FIFO memory. This
     * memory includes the Rx FIFO, non-periodic Tx FIFO, and periodic
     * Tx FIFOs.
     * 32 to 32768 (default 8192)
     * Note: The total FIFO memory depth in the FPGA configuration is 8192.
     */
    int32_t data_fifo_size;

    /** Number of 4-byte words in the Rx FIFO in device mode when dynamic
     * FIFO sizing is enabled.
     * 16 to 32768 (default 1064)
     */
    int32_t dev_rx_fifo_size;

    /** Number of 4-byte words in the non-periodic Tx FIFO in device mode
     * when dynamic FIFO sizing is enabled.
     * 16 to 32768 (default 1024)
     */
    int32_t dev_nperio_tx_fifo_size;

    /** Number of 4-byte words in each of the periodic Tx FIFOs in device
     * mode when dynamic FIFO sizing is enabled.
     * 4 to 768 (default 256)
     */
    uint32_t dev_perio_tx_fifo_size[MAX_PERIO_FIFOS];

    /** Number of 4-byte words in the Rx FIFO in host mode when dynamic
     * FIFO sizing is enabled.
     * 16 to 32768 (default 1024)
     */
    int32_t host_rx_fifo_size;

    /** Number of 4-byte words in the non-periodic Tx FIFO in host mode
     * when Dynamic FIFO sizing is enabled in the core.
     * 16 to 32768 (default 1024)
     */
    int32_t host_nperio_tx_fifo_size;

    /** Number of 4-byte words in the host periodic Tx FIFO when dynamic
     * FIFO sizing is enabled.
     * 16 to 32768 (default 1024)
     */
    int32_t host_perio_tx_fifo_size;

    /** The maximum transfer size supported in bytes.
     * 2047 to 65,535  (default 65,535)
     */
    int32_t max_transfer_size;

    /** The maximum number of packets in a transfer.
     * 15 to 511  (default 511)
     */
    int32_t max_packet_count;

    /** The number of host channel registers to use.
     * 1 to 16 (default 12)
     * Note: The FPGA configuration supports a maximum of 12 host channels.
     */
    int32_t host_channels;

    /** The number of endpoints in addition to EP0 available for device
     * mode operations.
     * 1 to 15 (default 6 IN and OUT)
     * Note: The FPGA configuration supports a maximum of 6 IN and OUT
     * endpoints in addition to EP0.
     */
    int32_t dev_endpoints;

        /**
         * Specifies the type of PHY interface to use. By default, the driver
         * will automatically detect the phy_type.
         *
         * 0 - Full Speed PHY
         * 1 - UTMI+ (default)
         * 2 - ULPI
         */
    int32_t phy_type;

    /**
     * Specifies the UTMI+ Data Width. This parameter is
     * applicable for a PHY_TYPE of UTMI+ or ULPI. (For a ULPI
     * PHY_TYPE, this parameter indicates the data width between
     * the MAC and the ULPI Wrapper.) Also, this parameter is
     * applicable only if the OTG_HSPHY_WIDTH cC parameter was set
     * to "8 and 16 bits", meaning that the core has been
     * configured to work at either data path width.
     *
     * 8 or 16 bits (default 16)
     */
    int32_t phy_utmi_width;

    /**
     * Specifies whether the ULPI operates at double or single
     * data rate. This parameter is only applicable if PHY_TYPE is
     * ULPI.
     *
     * 0 - single data rate ULPI interface with 8 bit wide data
     * bus (default)
     * 1 - double data rate ULPI interface with 4 bit wide data
     * bus
     */
    int32_t phy_ulpi_ddr;

    /**
     * Specifies whether to use the internal or external supply to
     * drive the vbus with a ULPI phy.
     */
    int32_t phy_ulpi_ext_vbus;

    /**
     * Specifies whether to use the I2Cinterface for full speed PHY. This
     * parameter is only applicable if PHY_TYPE is FS.
     * 0 - No (default)
     * 1 - Yes
     */
    int32_t i2c_enable;

    int32_t ulpi_fs_ls;

    int32_t ts_dline;

    /**
     * Specifies whether dedicated transmit FIFOs are
     * enabled for non periodic IN endpoints in device mode
     * 0 - No
     * 1 - Yes
     */
    int32_t en_multiple_tx_fifo;

    /** Number of 4-byte words in each of the Tx FIFOs in device
     * mode when dynamic FIFO sizing is enabled.
     * 4 to 768 (default 256)
     */
    uint32_t dev_tx_fifo_size[MAX_TX_FIFOS];

    /** Thresholding enable flag-
     * bit 0 - enable non-ISO Tx thresholding
     * bit 1 - enable ISO Tx thresholding
     * bit 2 - enable Rx thresholding
     */
    uint32_t thr_ctl;

    /** Thresholding length for Tx
     *  FIFOs in 32 bit DWORDs
     */
    uint32_t tx_thr_length;

    /** Thresholding length for Rx
     *  FIFOs in 32 bit DWORDs
     */
    uint32_t rx_thr_length;

    /**
     * Specifies whether LPM (Link Power Management) support is enabled
     */
    int32_t lpm_enable;

    /** Per Transfer Interrupt
     *  mode enable flag
     * 1 - Enabled
     * 0 - Disabled
     */
    int32_t pti_enable;

    /** Multi Processor Interrupt
     *  mode enable flag
     * 1 - Enabled
     * 0 - Disabled
     */
    int32_t mpi_enable;

    /** IS_USB Capability
     * 1 - Enabled
     * 0 - Disabled
     */
    int32_t ic_usb_cap;

    /** AHB Threshold Ratio
     * 2'b00 AHB Threshold =    MAC Threshold
     * 2'b01 AHB Threshold = 1/2    MAC Threshold
     * 2'b10 AHB Threshold = 1/4    MAC Threshold
     * 2'b11 AHB Threshold = 1/8    MAC Threshold
     */
    int32_t ahb_thr_ratio;

    /** ADP Support
     * 1 - Enabled
     * 0 - Disabled
     */
    int32_t adp_supp_enable;

    /** HFIR Reload Control
     * 0 - The HFIR cannot be reloaded dynamically.
     * 1 - Allow dynamic reloading of the HFIR register during runtime.
     */
    int32_t reload_ctl;

    /** DCFG: Enable device Out NAK 
     * 0 - The core does not set NAK after Bulk Out transfer complete.
     * 1 - The core sets NAK after Bulk OUT transfer complete.
     */
    int32_t dev_out_nak;

    /** Core Power down mode
     * 0 - No Power Down is enabled
     * 1 - Reserved
     * 2 - Complete Power Down (Hibernation)
     */
    int32_t power_down;

    /** OTG revision supported
     * 0 - OTG 1.3 revision
     * 1 - OTG 2.0 revision
     */
    int32_t otg_ver;

} comip_hsic_core_params_t;

#ifdef DEBUG
struct comip_hsic_core_if;
typedef struct hc_xfer_info {
    struct comip_hsic_core_if *core_if;
    comip_hsic_hc_t *hc;
} hc_xfer_info_t;
#endif

typedef struct ep_xfer_info {
    struct comip_hsic_core_if *core_if;
    comip_hsic_ep_t *ep;
    uint8_t state;
} ep_xfer_info_t;
/*
 * Device States
 */
typedef enum comip_hsic_lx_state {
    /** On state */
    COMIP_HSIC_L0,
    /** LPM sleep state*/
    COMIP_HSIC_L1,
    /** USB suspend state*/
    COMIP_HSIC_L2,
    /** Off state*/
    COMIP_HSIC_L3
} comip_hsic_lx_state_e;

struct comip_hsic_global_regs_backup {
    uint32_t gotgctl_local;
    uint32_t gintmsk_local;
    uint32_t gahbcfg_local;
    uint32_t gusbcfg_local;
    uint32_t grxfsiz_local;
    uint32_t gnptxfsiz_local;
#ifdef CONFIG_USB_COMIP_HSIC_LPM
    uint32_t glpmcfg_local;
#endif
    uint32_t gi2cctl_local;
    uint32_t hptxfsiz_local;
    uint32_t pcgcctl_local;
    uint32_t gdfifocfg_local;
    uint32_t dtxfsiz_local[MAX_EPS_CHANNELS];
    uint32_t gpwrdn_local;
};

struct comip_hsic_host_regs_backup {
    uint32_t hcfg_local;
    uint32_t haintmsk_local;
    uint32_t hcintmsk_local[MAX_EPS_CHANNELS];
    uint32_t hprt0_local;
    uint32_t hfir_local;
};

struct comip_hsic_dev_regs_backup {
    uint32_t dcfg;
    uint32_t dctl;
    uint32_t daintmsk;
    uint32_t diepmsk;
    uint32_t doepmsk;
    uint32_t diepctl[MAX_EPS_CHANNELS];
    uint32_t dieptsiz[MAX_EPS_CHANNELS];
    uint32_t diepdma[MAX_EPS_CHANNELS];
    uint32_t doepfn[MAX_EPS_CHANNELS];
};
/**
 * The <code>comip_hsic_core_if</code> structure contains information needed to manage
 * the COMIP_HSIC_otg controller acting in either host or device mode. It
 * represents the programming view of the controller as a whole.
 */
struct comip_hsic_core_if {
	usb_core_type		type;
	uint32_t	clk_enabled;
    /*clk*/
    struct clk 					*clk;
    struct clk 					*clk_hsic;
    /** Parameters that define how the core should be configured.*/
    comip_hsic_core_params_t *core_params;

	comip_hsic_core_ctl_regs_t *ctl_regs;
    /** Core Global registers starting at offset 000h. */
    comip_hsic_core_global_regs_t *core_global_regs;

    /** Host-specific information */
    comip_hsic_host_if_t *host_if;

    /** Value from SNPSID register */
    uint32_t snpsid;

    /*
     * Set to 1 if the core PHY interface bits in USBCFG have been
     * initialized.
     */
    uint8_t phy_init_done;

    /* Common configuration information */
    /** Power and Clock Gating Control Register */
    volatile uint32_t *pcgcctl;
#define COMIP_HSIC_PCGCCTL_OFFSET 0xE00

    /** Push/pop addresses for endpoints or host channels.*/
    uint32_t *data_fifo[MAX_EPS_CHANNELS];
#define COMIP_HSIC_DATA_FIFO_OFFSET 0x1000
#define COMIP_HSIC_DATA_FIFO_SIZE 0x1000

    /** Total RAM for FIFOs (Bytes) */
    uint16_t total_fifo_size;
    /** Size of Rx FIFO (Bytes) */
    uint16_t rx_fifo_size;
    /** Size of Non-periodic Tx FIFO (Bytes) */
    uint16_t nperio_tx_fifo_size;

    /** 1 if DMA is enabled, 0 otherwise. */
    uint8_t dma_enable;

    /** 1 if DMA descriptor is enabled, 0 otherwise. */
    uint8_t dma_desc_enable;

    /** 1 if PTI Enhancement mode is enabled, 0 otherwise. */
    uint8_t pti_enh_enable;

    /** 1 if MPI Enhancement mode is enabled, 0 otherwise. */
    uint8_t multiproc_int_enable;

    /** 1 if dedicated Tx FIFOs are enabled, 0 otherwise. */
    uint8_t en_multiple_tx_fifo;

    /** Set to 1 if multiple packets of a high-bandwidth transfer is in
     * process of being queued */
    uint8_t queuing_high_bandwidth;

    /** Hardware Configuration -- stored here for convenience.*/
    hwcfg1_data_t hwcfg1;
    hwcfg2_data_t hwcfg2;
    hwcfg3_data_t hwcfg3;
    hwcfg4_data_t hwcfg4;
    fifosize_data_t hptxfsiz;

    /** Host and Device Configuration -- stored here for convenience.*/
    hcfg_data_t hcfg;
    dcfg_data_t dcfg;

    /** The operational State, during transations
     * (a_host>>a_peripherial and b_device=>b_host) this may not
     * match the core but allows the software to determine
     * transitions.
     */
    uint8_t op_state;

    /**
     * Set to 1 if the HCD needs to be restarted on a session request
     * interrupt. This is required if no connector ID status change has
     * occurred since the HCD was last disconnected.
     */
    uint8_t restart_hcd_on_session_req;

    /** HCD callbacks */
    /** A-Device is a_host */
#define A_HOST      (1)
    /** A-Device is a_suspend */
#define A_SUSPEND   (2)
    /** A-Device is a_peripherial */
#define A_PERIPHERAL    (3)
    /** B-Device is operating as a Peripheral. */
#define B_PERIPHERAL    (4)
    /** B-Device is operating as a Host. */
#define B_HOST      (5)

    /** HCD callbacks */
    struct comip_hsic_cil_callbacks *hcd_cb;


    /** Workqueue object used for handling several interrupts */
    comip_hsic_workq_t *wq_otg;

    /** Timer object used for handling "Wakeup Detected" Interrupt */
    comip_hsic_timer_t *wkp_timer;
    /** This arrays used for debug purposes for DEV OUT NAK enhancement */
    uint32_t start_doeptsiz_val[MAX_EPS_CHANNELS];
    ep_xfer_info_t ep_xfer_info[MAX_EPS_CHANNELS];
    comip_hsic_timer_t *ep_xfer_timer[MAX_EPS_CHANNELS];
#ifdef DEBUG
    uint32_t start_hcchar_val[MAX_EPS_CHANNELS];

    hc_xfer_info_t hc_xfer_info[MAX_EPS_CHANNELS];
    comip_hsic_timer_t *hc_xfer_timer[MAX_EPS_CHANNELS];

    uint32_t hfnum_7_samples;
    uint64_t hfnum_7_frrem_accum;
    uint32_t hfnum_0_samples;
    uint64_t hfnum_0_frrem_accum;
    uint32_t hfnum_other_samples;
    uint64_t hfnum_other_frrem_accum;
#endif

#ifdef COMIP_HSIC_UTE_CFI
    uint16_t pwron_rxfsiz;
    uint16_t pwron_gnptxfsiz;
    uint16_t pwron_txfsiz[15];

    uint16_t init_rxfsiz;
    uint16_t init_gnptxfsiz;
    uint16_t init_txfsiz[15];
#endif

    /** Lx state of device */
    comip_hsic_lx_state_e lx_state;

    /** Saved Core Global registers */
    struct comip_hsic_global_regs_backup *gr_backup;
    /** Saved Host registers */
    struct comip_hsic_host_regs_backup *hr_backup;

    /** Power Down Enable */
    uint32_t power_down;

    /** hibernation/suspend flag */
    int hibernation_suspend;

    /** OTG revision supported */
    uint32_t otg_ver;

    /** OTG status flag used for HNP polling */
    uint8_t otg_sts;

    /** Pointer to either hcd->lock or pcd->lock */
    comip_hsic_spinlock_t *lock;

    /** Start predict NextEP based on Learning Queue if equal 1,
     * also used as counter of disabled NP IN EP's */
    uint8_t start_predict;

    /** NextEp sequence, including EP0: nextep_seq[] = EP if non-periodic and 
     * active, 0xff otherwise */
    uint8_t nextep_seq[MAX_EPS_CHANNELS];

    /** Index of fisrt EP in nextep_seq array which should be re-enabled **/
    uint8_t first_in_nextep_seq;

};

#ifdef DEBUG
/*
 * This function is called when transfer is timed out.
 */
extern void hsic_hc_xfer_timeout(void *ptr);
#endif
/*
 * The following functions are functions for works
 * using during handling some interrupts
 */
extern void w_conn_id_status_change(void *p);

extern void hsic_w_wakeup_detected(void *p);

/** Saves global register values into system memory. */
extern int comip_hsic_save_global_regs(comip_hsic_core_if_t * core_if);
/** Saves host register values into system memory. */
extern int comip_hsic_save_host_regs(comip_hsic_core_if_t * core_if);
/** Restore global register values. */
extern int comip_hsic_restore_global_regs(comip_hsic_core_if_t * core_if);
/** Restore host register values. */
extern int comip_hsic_restore_host_regs(comip_hsic_core_if_t * core_if, int reset);
extern int hsic_restore_lpm_i2c_regs(comip_hsic_core_if_t * core_if);
extern int hsic_restore_essential_regs(comip_hsic_core_if_t * core_if, int rmode,
                  int is_host);

extern int comip_hsic_host_hibernation_restore(comip_hsic_core_if_t * core_if,
                        int restore_mode, int reset);
/*
 * The following functions support initialization of the CIL driver component
 * and the COMIP_HSIC_otg controller.
 */
extern void comip_hsic_core_host_init(comip_hsic_core_if_t * _core_if);

/**@}*/

/** @name Host CIL Functions
 * The following functions support managing the COMIP_HSIC_otg controller in host
 * mode.
 */
/**@{*/
extern void comip_hsic_hc_init(comip_hsic_core_if_t * _core_if, comip_hsic_hc_t * _hc);
extern void comip_hsic_hc_halt(comip_hsic_core_if_t * _core_if,
                comip_hsic_hc_t * _hc, comip_hsic_halt_status_e _halt_status);
extern void comip_hsic_hc_cleanup(comip_hsic_core_if_t * _core_if, comip_hsic_hc_t * _hc);
extern void comip_hsic_hc_start_transfer(comip_hsic_core_if_t * _core_if,
                      comip_hsic_hc_t * _hc);
extern int comip_hsic_hc_continue_transfer(comip_hsic_core_if_t * _core_if,
                    comip_hsic_hc_t * _hc);
extern void comip_hsic_hc_do_ping(comip_hsic_core_if_t * _core_if, comip_hsic_hc_t * _hc);
extern void comip_hsic_hc_write_packet(comip_hsic_core_if_t * _core_if,
                    comip_hsic_hc_t * _hc);
extern void comip_hsic_enable_host_interrupts(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_disable_host_interrupts(comip_hsic_core_if_t * _core_if);

extern void comip_hsic_hc_start_transfer_ddma(comip_hsic_core_if_t * core_if,
                       comip_hsic_hc_t * hc);

extern uint32_t hsic_calc_frame_interval(comip_hsic_core_if_t * core_if);

/* Macro used to clear one channel interrupt */
#define clear_hc_int(_hc_regs_, _intr_) \
do { \
    hcint_data_t hcint_clear = {.d32 = 0}; \
    hcint_clear.b._intr_ = 1; \
    writel(&(_hc_regs_)->hcint, hcint_clear.d32); \
} while (0)

/*
 * Macro used to disable one channel interrupt. Channel interrupts are
 * disabled when the channel is halted or released by the interrupt handler.
 * There is no need to handle further interrupts of that type until the
 * channel is re-assigned. In fact, subsequent handling may cause crashes
 * because the channel structures are cleaned up when the channel is released.
 */
#define disable_hc_int(_hc_regs_, _intr_) \
do { \
    hcintmsk_data_t hcintmsk = {.d32 = 0}; \
    hcintmsk.b._intr_ = 1; \
    COMIP_HSIC_MODIFY_REG32(&(_hc_regs_)->hcintmsk, hcintmsk.d32, 0); \
} while (0)

/**
 * This function Reads HPRT0 in preparation to modify. It keeps the
 * WC bits 0 so that if they are read as 1, they won't clear when you
 * write it back
 */
static inline uint32_t comip_hsic_read_hprt0(comip_hsic_core_if_t * _core_if)
{
    hprt0_data_t hprt0;
    hprt0.d32 = readl(_core_if->host_if->hprt0);
    hprt0.b.prtena = 0;
    hprt0.b.prtconndet = 0;
    hprt0.b.prtenchng = 0;
    hprt0.b.prtovrcurrchng = 0;
    return hprt0.d32;
}

/**@}*/

/** @name Common CIL Functions
 * The following functions support managing the COMIP_HSIC_otg controller in either
 * device or host mode.
 */
/**@{*/

extern void comip_hsic_read_packet(comip_hsic_core_if_t * core_if,
                uint8_t * dest, uint16_t bytes);

extern void comip_hsic_flush_tx_fifo(comip_hsic_core_if_t * _core_if, const int _num);
extern void comip_hsic_flush_rx_fifo(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_core_reset(comip_hsic_core_if_t * _core_if);

/**
 * This function returns the Core Interrupt register.
 */
static inline uint32_t comip_hsic_read_core_intr(comip_hsic_core_if_t * core_if)
{
    return (readl(&core_if->core_global_regs->gintsts) &
        readl(&core_if->core_global_regs->gintmsk));
}

/**
 * This function returns the OTG Interrupt register.
 */
static inline uint32_t comip_hsic_read_otg_intr(comip_hsic_core_if_t * core_if)
{
    return (readl(&core_if->core_global_regs->gotgint));
}

/**
 * This function returns the Host All Channel Interrupt register
 */
static inline uint32_t comip_hsic_read_host_all_channels_intr(comip_hsic_core_if_t *
                               _core_if)
{
    return (readl(&_core_if->host_if->host_global_regs->haint));
}

static inline uint32_t comip_hsic_read_host_channel_intr(comip_hsic_core_if_t *
                              _core_if, comip_hsic_hc_t * _hc)
{
    return (readl(&_core_if->host_if->hc_regs[_hc->hc_num]->hcint));
}

/**
 * This function returns the mode of the operation, host or device.
 *
 * @return 0 - Device Mode, 1 - Host Mode
 */
static inline uint32_t comip_hsic_mode(comip_hsic_core_if_t * _core_if)
{
    return (readl(&_core_if->core_global_regs->gintsts) & 0x1);
}

/**@}*/

/**
 * COMIP_HSIC_otg CIL callback structure. This structure allows the HCD and
 * PCD to register functions used for starting and stopping the PCD
 * and HCD for role change on for a DRD.
 */
typedef struct comip_hsic_cil_callbacks {
    /** Start function for role change */
    int (*start) (void *_p);
    /** Stop Function for role change */
    int (*stop) (void *_p);
    /** Disconnect Function for role change */
    int (*disconnect) (void *_p);
    /** Resume/Remote wakeup Function */
    int (*resume_wakeup) (void *_p);
    /** Suspend function */
    int (*suspend) (void *_p);
    /** Session Start (SRP) */
    int (*session_start) (void *_p);
#ifdef CONFIG_USB_COMIP_HSIC_LPM
    /** Sleep (switch to L0 state) */
    int (*sleep) (void *_p);
#endif
    /** Pointer passed to start() and stop() */
    void *p;
} comip_hsic_cil_callbacks_t;

extern void comip_hsic_cil_register_hcd_callbacks(comip_hsic_core_if_t * _core_if,
                           comip_hsic_cil_callbacks_t * _cb,
                           void *_p);

//////////////////////////////////////////////////////////////////////
/** Start the HCD.  Helper function for using the HCD callbacks.
 *
 * @param core_if Programming view of COMIP_HSIC_otg controller.
 */
static inline void cil_hcd_start(comip_hsic_core_if_t * core_if)
{
    if (core_if->hcd_cb && core_if->hcd_cb->start) {
        core_if->hcd_cb->start(core_if->hcd_cb->p);
    }
}

/** Stop the HCD.  Helper function for using the HCD callbacks.
 *
 * @param core_if Programming view of COMIP_HSIC_otg controller.
 */
static inline void cil_hcd_stop(comip_hsic_core_if_t * core_if)
{
    if (core_if->hcd_cb && core_if->hcd_cb->stop) {
        core_if->hcd_cb->stop(core_if->hcd_cb->p);
    }
}

/** Disconnect the HCD.  Helper function for using the HCD callbacks.
 *
 * @param core_if Programming view of COMIP_HSIC_otg controller.
 */
static inline void cil_hcd_disconnect(comip_hsic_core_if_t * core_if)
{
    if (core_if->hcd_cb && core_if->hcd_cb->disconnect) {
        core_if->hcd_cb->disconnect(core_if->hcd_cb->p);
    }
}

/** Inform the HCD the a New Session has begun.  Helper function for
 * using the HCD callbacks.
 *
 * @param core_if Programming view of COMIP_HSIC_otg controller.
 */
static inline void cil_hcd_session_start(comip_hsic_core_if_t * core_if)
{
    if (core_if->hcd_cb && core_if->hcd_cb->session_start) {
        core_if->hcd_cb->session_start(core_if->hcd_cb->p);
    }
}

#ifdef CONFIG_USB_COMIP_HSIC_LPM
/**
 * Inform the HCD about LPM sleep.
 * Helper function for using the HCD callbacks.
 *
 * @param core_if Programming view of COMIP_HSIC_otg controller.
 */
static inline void cil_hcd_sleep(comip_hsic_core_if_t * core_if)
{
    if (core_if->hcd_cb && core_if->hcd_cb->sleep) {
        core_if->hcd_cb->sleep(core_if->hcd_cb->p);
    }
}
#endif

/** Resume the HCD.  Helper function for using the HCD callbacks.
 *
 * @param core_if Programming view of COMIP_HSIC_otg controller.
 */
static inline void cil_hcd_resume(comip_hsic_core_if_t * core_if)
{
    if (core_if->hcd_cb && core_if->hcd_cb->resume_wakeup) {
        core_if->hcd_cb->resume_wakeup(core_if->hcd_cb->p);
    }
}
#endif
