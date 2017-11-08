/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/comip_hsic_core_if.h $
 * $Revision: #10 $
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
#if !defined(__COMIP_HSIC_CORE_IF_H__)
#define __COMIP_HSIC_CORE_IF_H__

#include "comip_hsic_os.h"
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <plat/clock.h>
#include <plat/usb.h>

/** @file
 * This file defines COMIP_HSIC_OTG Core API
 */

struct comip_hsic_core_if;
typedef struct comip_hsic_core_if comip_hsic_core_if_t;

/** Maximum number of Periodic FIFOs */
#define MAX_PERIO_FIFOS 15
/** Maximum number of Periodic FIFOs */
#define MAX_TX_FIFOS 15

/** Maximum number of Endpoints/HostChannels */
#define MAX_EPS_CHANNELS 16
extern void set_usb_init_reg(comip_hsic_core_if_t *core_if,  usb_core_type type);
extern comip_hsic_core_if_t *comip_hsic_cil_init(const uint32_t * _reg_base_addr, const uint32_t * _ctl_reg_base_addr, struct platform_device *_dev);
extern void comip_hsic_cil_uninit(comip_hsic_core_if_t * _core_if, int id);
extern void comip_hsic_core_init(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_cil_remove(comip_hsic_core_if_t * _core_if);
//extern int usb_power_set(int onoff);
extern void comip_hsic_enable_global_interrupts(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_disable_global_interrupts(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_disable_all_interrupts_except_idchng(comip_hsic_core_if_t * core_if);
extern uint8_t comip_hsic_is_device_mode(comip_hsic_core_if_t * _core_if);
extern uint8_t comip_hsic_is_host_mode(comip_hsic_core_if_t * _core_if);
extern int comip_hsic_cil_suspend(comip_hsic_core_if_t * core_if);
extern int comip_hsic_cil_resume(comip_hsic_core_if_t * core_if);
extern uint8_t comip_hsic_is_dma_enable(comip_hsic_core_if_t * core_if);

/** This function should be called on every hardware interrupt. */
extern int32_t comip_hsic_handle_common_intr(comip_hsic_core_if_t * _core_if);

/** @name OTG Core Parameters */
/** @{ */

/**
 * Specifies the OTG capabilities. The driver will automatically
 * detect the value for this parameter if none is specified.
 * 0 - HNP and SRP capable (default)
 * 1 - SRP Only capable
 * 2 - No HNP/SRP capable
 */
extern int comip_hsic_set_param_otg_cap(comip_hsic_core_if_t * core_if, int32_t val);
extern int32_t comip_hsic_get_param_otg_cap(comip_hsic_core_if_t * core_if);
#define COMIP_HSIC_CAP_PARAM_HNP_SRP_CAPABLE 0
#define COMIP_HSIC_CAP_PARAM_SRP_ONLY_CAPABLE 1
#define COMIP_HSIC_CAP_PARAM_NO_HNP_SRP_CAPABLE 2
#define comip_hsic_param_otg_cap_default COMIP_HSIC_CAP_PARAM_HNP_SRP_CAPABLE

extern int comip_hsic_set_param_opt(comip_hsic_core_if_t * core_if, int32_t val);
extern int32_t comip_hsic_get_param_opt(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_opt_default 1

/**
 * Specifies whether to use slave or DMA mode for accessing the data
 * FIFOs. The driver will automatically detect the value for this
 * parameter if none is specified.
 * 0 - Slave
 * 1 - DMA (default, if available)
 */
extern int comip_hsic_set_param_dma_enable(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_dma_enable(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_dma_enable_default 1

/**
 * When DMA mode is enabled specifies whether to use
 * address DMA or DMA Descritor mode for accessing the data
 * FIFOs in device mode. The driver will automatically detect
 * the value for this parameter if none is specified.
 * 0 - address DMA
 * 1 - DMA Descriptor(default, if available)
 */
extern int comip_hsic_set_param_dma_desc_enable(comip_hsic_core_if_t * core_if,
                         int32_t val);
extern int32_t comip_hsic_get_param_dma_desc_enable(comip_hsic_core_if_t * core_if);
#ifdef CONFIG_COMIP_HSIC_USBHUB
#define comip_hsic_param_dma_desc_enable_default 0
#else
#define comip_hsic_param_dma_desc_enable_default 1
#endif


/** The DMA Burst size (applicable only for External DMA
 * Mode). 1, 4, 8 16, 32, 64, 128, 256 (default 32)
 */
extern int comip_hsic_set_param_dma_burst_size(comip_hsic_core_if_t * core_if,
                        int32_t val);
extern int32_t comip_hsic_get_param_dma_burst_size(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_dma_burst_size_default 32

/**
 * Specifies the maximum speed of operation in host and device mode.
 * The actual speed depends on the speed of the attached device and
 * the value of phy_type. The actual speed depends on the speed of the
 * attached device.
 * 0 - High Speed (default)
 * 1 - Full Speed
 */
extern int comip_hsic_set_param_speed(comip_hsic_core_if_t * core_if, int32_t val);
extern int32_t comip_hsic_get_param_speed(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_speed_default 0
#define COMIP_HSIC_SPEED_PARAM_HIGH 0
#define COMIP_HSIC_SPEED_PARAM_FULL 1

/** Specifies whether low power mode is supported when attached
 *  to a Full Speed or Low Speed device in host mode.
 * 0 - Don't support low power mode (default)
 * 1 - Support low power mode
 */
extern int comip_hsic_set_param_host_support_fs_ls_low_power(comip_hsic_core_if_t *
                              core_if, int32_t val);
extern int32_t comip_hsic_get_param_host_support_fs_ls_low_power(comip_hsic_core_if_t
                                  * core_if);
#define comip_hsic_param_host_support_fs_ls_low_power_default 0

/** Specifies the PHY clock rate in low power mode when connected to a
 * Low Speed device in host mode. This parameter is applicable only if
 * HOST_SUPPORT_FS_LS_LOW_POWER is enabled. If PHY_TYPE is set to FS
 * then defaults to 6 MHZ otherwise 48 MHZ.
 *
 * 0 - 48 MHz
 * 1 - 6 MHz
 */
extern int comip_hsic_set_param_host_ls_low_power_phy_clk(comip_hsic_core_if_t *
                               core_if, int32_t val);
extern int32_t comip_hsic_get_param_host_ls_low_power_phy_clk(comip_hsic_core_if_t *
                               core_if);
#define comip_hsic_param_host_ls_low_power_phy_clk_default 0
#define COMIP_HSIC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_48MHZ 0
#define COMIP_HSIC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_6MHZ 1

/**
 * 0 - Use cC FIFO size parameters
 * 1 - Allow dynamic FIFO sizing (default)
 */
extern int comip_hsic_set_param_enable_dynamic_fifo(comip_hsic_core_if_t * core_if,
                         int32_t val);
extern int32_t comip_hsic_get_param_enable_dynamic_fifo(comip_hsic_core_if_t *
                             core_if);
#define comip_hsic_param_enable_dynamic_fifo_default 1

/** Total number of 4-byte words in the data FIFO memory. This
 * memory includes the Rx FIFO, non-periodic Tx FIFO, and periodic
 * Tx FIFOs.
 * 32 to 32768 (default 8192)
 * Note: The total FIFO memory depth in the FPGA configuration is 8192.
 */
extern int comip_hsic_set_param_data_fifo_size(comip_hsic_core_if_t * core_if,
                        int32_t val);
extern int32_t comip_hsic_get_param_data_fifo_size(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_data_fifo_size_default 8192

/** Number of 4-byte words in the Rx FIFO in device mode when dynamic
 * FIFO sizing is enabled.
 * 16 to 32768 (default 1064)
 */
extern int comip_hsic_set_param_dev_rx_fifo_size(comip_hsic_core_if_t * core_if,
                          int32_t val);
extern int32_t comip_hsic_get_param_dev_rx_fifo_size(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_dev_rx_fifo_size_default 1064

/** Number of 4-byte words in the non-periodic Tx FIFO in device mode
 * when dynamic FIFO sizing is enabled.
 * 16 to 32768 (default 1024)
 */
extern int comip_hsic_set_param_dev_nperio_tx_fifo_size(comip_hsic_core_if_t *
                             core_if, int32_t val);
extern int32_t comip_hsic_get_param_dev_nperio_tx_fifo_size(comip_hsic_core_if_t *
                             core_if);
#define comip_hsic_param_dev_nperio_tx_fifo_size_default 1024

/** Number of 4-byte words in each of the periodic Tx FIFOs in device
 * mode when dynamic FIFO sizing is enabled.
 * 4 to 768 (default 256)
 */
extern int comip_hsic_set_param_dev_perio_tx_fifo_size(comip_hsic_core_if_t * core_if,
                            int32_t val, int fifo_num);
extern int32_t comip_hsic_get_param_dev_perio_tx_fifo_size(comip_hsic_core_if_t *
                            core_if, int fifo_num);
#define comip_hsic_param_dev_perio_tx_fifo_size_default 256

/** Number of 4-byte words in the Rx FIFO in host mode when dynamic
 * FIFO sizing is enabled.
 * 16 to 32768 (default 1024)
 */
extern int comip_hsic_set_param_host_rx_fifo_size(comip_hsic_core_if_t * core_if,
                           int32_t val);
extern int32_t comip_hsic_get_param_host_rx_fifo_size(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_host_rx_fifo_size_default 512//1024

/** Number of 4-byte words in the non-periodic Tx FIFO in host mode
 * when Dynamic FIFO sizing is enabled in the core.
 * 16 to 32768 (default 1024)
 */
extern int comip_hsic_set_param_host_nperio_tx_fifo_size(comip_hsic_core_if_t *
                              core_if, int32_t val);
extern int32_t comip_hsic_get_param_host_nperio_tx_fifo_size(comip_hsic_core_if_t *
                              core_if);
#define comip_hsic_param_host_nperio_tx_fifo_size_default 256//1024

/** Number of 4-byte words in the host periodic Tx FIFO when dynamic
 * FIFO sizing is enabled.
 * 16 to 32768 (default 1024)
 */
extern int comip_hsic_set_param_host_perio_tx_fifo_size(comip_hsic_core_if_t *
                             core_if, int32_t val);
extern int32_t comip_hsic_get_param_host_perio_tx_fifo_size(comip_hsic_core_if_t *
                             core_if);
#define comip_hsic_param_host_perio_tx_fifo_size_default 448//1024

/** The maximum transfer size supported in bytes.
 * 2047 to 65,535  (default 65,535)
 */
extern int comip_hsic_set_param_max_transfer_size(comip_hsic_core_if_t * core_if,
                           int32_t val);
extern int32_t comip_hsic_get_param_max_transfer_size(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_max_transfer_size_default 65535

/** The maximum number of packets in a transfer.
 * 15 to 511  (default 511)
 */
extern int comip_hsic_set_param_max_packet_count(comip_hsic_core_if_t * core_if,
                          int32_t val);
extern int32_t comip_hsic_get_param_max_packet_count(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_max_packet_count_default 511

/** The number of host channel registers to use.
 * 1 to 16 (default 12)
 * Note: The FPGA configuration supports a maximum of 12 host channels.
 */
extern int comip_hsic_set_param_host_channels(comip_hsic_core_if_t * core_if,
                       int32_t val);
extern int32_t comip_hsic_get_param_host_channels(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_host_channels_default 12

/** The number of endpoints in addition to EP0 available for device
 * mode operations.
 * 1 to 15 (default 6 IN and OUT)
 * Note: The FPGA configuration supports a maximum of 6 IN and OUT
 * endpoints in addition to EP0.
 */
extern int comip_hsic_set_param_dev_endpoints(comip_hsic_core_if_t * core_if,
                       int32_t val);
extern int32_t comip_hsic_get_param_dev_endpoints(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_dev_endpoints_default 6

/**
 * Specifies the type of PHY interface to use. By default, the driver
 * will automatically detect the phy_type.
 *
 * 0 - Full Speed PHY
 * 1 - UTMI+ (default)
 * 2 - ULPI
 */
extern int comip_hsic_set_param_phy_type(comip_hsic_core_if_t * core_if, int32_t val);
extern int32_t comip_hsic_get_param_phy_type(comip_hsic_core_if_t * core_if);
#define COMIP_HSIC_PHY_TYPE_PARAM_FS 0
#define COMIP_HSIC_PHY_TYPE_PARAM_UTMI 1
#define COMIP_HSIC_PHY_TYPE_PARAM_ULPI 2
#define comip_hsic_param_phy_type_default COMIP_HSIC_PHY_TYPE_PARAM_UTMI

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
extern int comip_hsic_set_param_phy_utmi_width(comip_hsic_core_if_t * core_if,
                        int32_t val);
extern int32_t comip_hsic_get_param_phy_utmi_width(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_phy_utmi_width_default 16

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
extern int comip_hsic_set_param_phy_ulpi_ddr(comip_hsic_core_if_t * core_if,
                      int32_t val);
extern int32_t comip_hsic_get_param_phy_ulpi_ddr(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_phy_ulpi_ddr_default 0

/**
 * Specifies whether to use the internal or external supply to
 * drive the vbus with a ULPI phy.
 */
extern int comip_hsic_set_param_phy_ulpi_ext_vbus(comip_hsic_core_if_t * core_if,
                           int32_t val);
extern int32_t comip_hsic_get_param_phy_ulpi_ext_vbus(comip_hsic_core_if_t * core_if);
#define COMIP_HSIC_PHY_ULPI_INTERNAL_VBUS 0
#define COMIP_HSIC_PHY_ULPI_EXTERNAL_VBUS 1
#define comip_hsic_param_phy_ulpi_ext_vbus_default COMIP_HSIC_PHY_ULPI_INTERNAL_VBUS

/**
 * Specifies whether to use the I2Cinterface for full speed PHY. This
 * parameter is only applicable if PHY_TYPE is FS.
 * 0 - No (default)
 * 1 - Yes
 */
extern int comip_hsic_set_param_i2c_enable(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_i2c_enable(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_i2c_enable_default 0

extern int comip_hsic_set_param_ulpi_fs_ls(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_ulpi_fs_ls(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_ulpi_fs_ls_default 0

extern int comip_hsic_set_param_ts_dline(comip_hsic_core_if_t * core_if, int32_t val);
extern int32_t comip_hsic_get_param_ts_dline(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_ts_dline_default 0

/**
 * Specifies whether dedicated transmit FIFOs are
 * enabled for non periodic IN endpoints in device mode
 * 0 - No
 * 1 - Yes
 */
extern int comip_hsic_set_param_en_multiple_tx_fifo(comip_hsic_core_if_t * core_if,
                         int32_t val);
extern int32_t comip_hsic_get_param_en_multiple_tx_fifo(comip_hsic_core_if_t *
                             core_if);
#define comip_hsic_param_en_multiple_tx_fifo_default 1

/** Number of 4-byte words in each of the Tx FIFOs in device
 * mode when dynamic FIFO sizing is enabled.
 * 4 to 768 (default 256)
 */
extern int comip_hsic_set_param_dev_tx_fifo_size(comip_hsic_core_if_t * core_if,
                          int fifo_num, int32_t val);
extern int32_t comip_hsic_get_param_dev_tx_fifo_size(comip_hsic_core_if_t * core_if,
                          int fifo_num);
#define comip_hsic_param_dev_tx_fifo_size_default 768

/** Thresholding enable flag-
 * bit 0 - enable non-ISO Tx thresholding
 * bit 1 - enable ISO Tx thresholding
 * bit 2 - enable Rx thresholding
 */
extern int comip_hsic_set_param_thr_ctl(comip_hsic_core_if_t * core_if, int32_t val);
extern int32_t comip_hsic_get_thr_ctl(comip_hsic_core_if_t * core_if, int fifo_num);
#define comip_hsic_param_thr_ctl_default 0

/** Thresholding length for Tx
 * FIFOs in 32 bit DWORDs
 */
extern int comip_hsic_set_param_tx_thr_length(comip_hsic_core_if_t * core_if,
                       int32_t val);
extern int32_t comip_hsic_get_tx_thr_length(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_tx_thr_length_default 64

/** Thresholding length for Rx
 *  FIFOs in 32 bit DWORDs
 */
extern int comip_hsic_set_param_rx_thr_length(comip_hsic_core_if_t * core_if,
                       int32_t val);
extern int32_t comip_hsic_get_rx_thr_length(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_rx_thr_length_default 64

/**
 * Specifies whether LPM (Link Power Management) support is enabled
 */
extern int comip_hsic_set_param_lpm_enable(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_lpm_enable(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_lpm_enable_default 1

/**
 * Specifies whether PTI enhancement is enabled
 */
extern int comip_hsic_set_param_pti_enable(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_pti_enable(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_pti_enable_default 0

/**
 * Specifies whether MPI enhancement is enabled
 */
extern int comip_hsic_set_param_mpi_enable(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_mpi_enable(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_mpi_enable_default 0

/**
 * Specifies whether ADP capability is enabled
 */
extern int comip_hsic_set_param_adp_enable(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_adp_enable(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_adp_enable_default 0

/**
 * Specifies whether IC_USB capability is enabled
 */

extern int comip_hsic_set_param_ic_usb_cap(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_ic_usb_cap(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_ic_usb_cap_default 0

extern int comip_hsic_set_param_ahb_thr_ratio(comip_hsic_core_if_t * core_if,
                       int32_t val);
extern int32_t comip_hsic_get_param_ahb_thr_ratio(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_ahb_thr_ratio_default 0

extern int comip_hsic_set_param_power_down(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_power_down(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_power_down_default 0

extern int comip_hsic_set_param_reload_ctl(comip_hsic_core_if_t * core_if,
                    int32_t val);
extern int32_t comip_hsic_get_param_reload_ctl(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_reload_ctl_default 0

extern int comip_hsic_set_param_dev_out_nak(comip_hsic_core_if_t * core_if,
                                        int32_t val);
extern int32_t comip_hsic_get_param_dev_out_nak(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_dev_out_nak_default 0

extern int comip_hsic_set_param_otg_ver(comip_hsic_core_if_t * core_if, int32_t val);
extern int32_t comip_hsic_get_param_otg_ver(comip_hsic_core_if_t * core_if);
#define comip_hsic_param_otg_ver_default 1

/** @} */

/** @name Access to registers and bit-fields */

/**
 * Dump core registers and SPRAM
 */
extern void comip_hsic_dump_dev_registers(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_dump_spram(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_dump_host_registers(comip_hsic_core_if_t * _core_if);
extern void comip_hsic_dump_global_registers(comip_hsic_core_if_t * _core_if);

/**
 * Get host negotiation status.
 */
extern uint32_t comip_hsic_get_hnpstatus(comip_hsic_core_if_t * core_if);

/**
 * Get srp status
 */
extern uint32_t comip_hsic_get_srpstatus(comip_hsic_core_if_t * core_if);

/**
 * Set hnpreq bit in the GOTGCTL register.
 */
extern void comip_hsic_set_hnpreq(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get Content of SNPSID register.
 */
extern uint32_t comip_hsic_get_gsnpsid(comip_hsic_core_if_t * core_if);

/**
 * Get current mode.
 * Returns 0 if in device mode, and 1 if in host mode.
 */
extern uint32_t comip_hsic_get_mode(comip_hsic_core_if_t * core_if);

/**
 * Get value of hnpcapable field in the GUSBCFG register
 */
extern uint32_t comip_hsic_get_hnpcapable(comip_hsic_core_if_t * core_if);
/**
 * Set value of hnpcapable field in the GUSBCFG register
 */
extern void comip_hsic_set_hnpcapable(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of srpcapable field in the GUSBCFG register
 */
extern uint32_t comip_hsic_get_srpcapable(comip_hsic_core_if_t * core_if);
/**
 * Set value of srpcapable field in the GUSBCFG register
 */
extern void comip_hsic_set_srpcapable(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of devspeed field in the DCFG register
 */
extern uint32_t comip_hsic_get_devspeed(comip_hsic_core_if_t * core_if);
/**
 * Set value of devspeed field in the DCFG register
 */
extern void comip_hsic_set_devspeed(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get the value of busconnected field from the HPRT0 register
 */
extern uint32_t comip_hsic_get_busconnected(comip_hsic_core_if_t * core_if);

/**
 * Gets the device enumeration Speed.
 */
extern uint32_t comip_hsic_get_enumspeed(comip_hsic_core_if_t * core_if);

/**
 * Get value of prtpwr field from the HPRT0 register
 */
extern uint32_t comip_hsic_get_prtpower(comip_hsic_core_if_t * core_if);

/**
 * Get value of flag indicating core state - hibernated or not
 */
extern uint32_t comip_hsic_get_core_state(comip_hsic_core_if_t * core_if);

/**
 * Set value of prtpwr field from the HPRT0 register
 */
extern void comip_hsic_set_prtpower(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of prtsusp field from the HPRT0 regsiter
 */
extern uint32_t comip_hsic_get_prtsuspend(comip_hsic_core_if_t * core_if);
/**
 * Set value of prtpwr field from the HPRT0 register
 */
extern void comip_hsic_set_prtsuspend(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of ModeChTimEn field from the HCFG regsiter
 */
extern uint32_t comip_hsic_get_mode_ch_tim(comip_hsic_core_if_t * core_if);
/**
 * Set value of ModeChTimEn field from the HCFG regsiter
 */
extern void comip_hsic_set_mode_ch_tim(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of Fram Interval field from the HFIR regsiter
 */
extern uint32_t comip_hsic_get_fr_interval(comip_hsic_core_if_t * core_if);
/**
 * Set value of Frame Interval field from the HFIR regsiter
 */
extern void comip_hsic_set_fr_interval(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Set value of prtres field from the HPRT0 register
 *FIXME Remove?
 */
extern void comip_hsic_set_prtresume(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of rmtwkupsig bit in DCTL register
 */
extern uint32_t comip_hsic_get_remotewakesig(comip_hsic_core_if_t * core_if);

/**
 * Get value of prt_sleep_sts field from the GLPMCFG register
 */
extern uint32_t comip_hsic_get_lpm_portsleepstatus(comip_hsic_core_if_t * core_if);

/**
 * Get value of rem_wkup_en field from the GLPMCFG register
 */
extern uint32_t comip_hsic_get_lpm_remotewakeenabled(comip_hsic_core_if_t * core_if);

/**
 * Get value of appl_resp field from the GLPMCFG register
 */
extern uint32_t comip_hsic_get_lpmresponse(comip_hsic_core_if_t * core_if);
/**
 * Set value of appl_resp field from the GLPMCFG register
 */
extern void comip_hsic_set_lpmresponse(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of hsic_connect field from the GLPMCFG register
 */
extern uint32_t comip_hsic_get_hsic_connect(comip_hsic_core_if_t * core_if);
/**
 * Set value of hsic_connect field from the GLPMCFG register
 */
extern void comip_hsic_set_hsic_connect(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * Get value of inv_sel_hsic field from the GLPMCFG register.
 */
extern uint32_t comip_hsic_get_inv_sel_hsic(comip_hsic_core_if_t * core_if);
/**
 * Set value of inv_sel_hsic field from the GLPMFG register.
 */
extern void comip_hsic_set_inv_sel_hsic(comip_hsic_core_if_t * core_if, uint32_t val);

/*
 * Some functions for accessing registers
 */

/**
 *  GOTGCTL register
 */
extern uint32_t comip_hsic_get_gotgctl(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_gotgctl(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * GUSBCFG register
 */
extern uint32_t comip_hsic_get_gusbcfg(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_gusbcfg(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * GRXFSIZ register
 */
extern uint32_t comip_hsic_get_grxfsiz(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_grxfsiz(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * GNPTXFSIZ register
 */
extern uint32_t comip_hsic_get_gnptxfsiz(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_gnptxfsiz(comip_hsic_core_if_t * core_if, uint32_t val);

extern uint32_t comip_hsic_get_gpvndctl(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_gpvndctl(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * GGPIO register
 */
extern uint32_t comip_hsic_get_ggpio(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_ggpio(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * GUID register
 */
extern uint32_t comip_hsic_get_guid(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_guid(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * HPRT0 register
 */
extern uint32_t comip_hsic_get_hprt0(comip_hsic_core_if_t * core_if);
extern void comip_hsic_set_hprt0(comip_hsic_core_if_t * core_if, uint32_t val);

/**
 * GHPTXFSIZE
 */
extern uint32_t comip_hsic_get_hptxfsiz(comip_hsic_core_if_t * core_if);

/** @} */
/* Type declarations */
struct comip_hsic_hcd;

typedef struct os_dependent {
    /** Base address returned from ioremap() */
    void *base[2];

    /** Register offset for Diagnostic API */
    uint32_t reg_offset;
    struct platform_device *platformdev;
} os_dependent_t;

/**
 * This structure is a wrapper that encapsulates the driver components used to
 * manage a single comip_hsic_otg controller.
 */
typedef struct comip_hsic_hcd_device {
    /** Structure containing OS-dependent stuff. KEEP THIS STRUCT AT THE
     * VERY BEGINNING OF THE DEVICE STRUCT. OSes such as FreeBSD and NetBSD
     * require this. */
    struct os_dependent os_dep;

    /** Pointer to the core interface structure. */
    comip_hsic_core_if_t *core_if;

    /** Pointer to the HCD structure. */
    struct comip_hsic_hcd *hcd;

    /** Flag to indicate whether the common IRQ handler is installed. */
    uint8_t common_irq_installed;

    uint8_t irq;
    
    struct wake_lock wakelock;
    
    struct mutex muxlock;

} comip_hsic_hcd_device_t;/* Type declarations */

#endif              /* __COMIP_HSIC_CORE_IF_H__ */
