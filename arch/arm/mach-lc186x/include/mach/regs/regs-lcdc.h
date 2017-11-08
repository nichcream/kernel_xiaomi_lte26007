#ifndef __ASM_ARCH_REGS_LCDC_H
#define __ASM_ARCH_REGS_LCDC_H

#define LCDC_MOD					(0x00)
#define LCDC_INTC					(0x14)
#define LCDC_INTF					(0x18)
#define LCDC_INTE					(0x1C)
#define LCDC_TIME1000US					(0x28)
#define LCDC_HV_CTL 					(0x30)
#define LCDC_HV_P					(0x34)
#define LCDC_HV_HCTL					(0x3C)
#define LCDC_HV_VCTL0					(0x40)
#define LCDC_HV_HS					(0x48)
#define LCDC_HV_VS					(0x4C)
#define LCDC_HV_F					(0x50)
#define LCDC_HV_VINT					(0x54)
#define LCDC_HV_DM					(0x58)
#define LCDC_HV_CNTNOW					(0x5C)
#define LCDC_MIPI_DSI_AUTORESETTIME			(0x60)
#define LCDC_MIPI_DSI_AUTORESETCTL			(0x64)
#define LCDC_MWIN_SIZE					(0x74)
#define LCDC_MWIN_BG					(0x78)
#define LCDC_DISP_CTRL					(0x7c)
#define LCDC_L0_WIN_CTRL				(0x94)
#define LCDC_L0_SA0 					(0x98)
#define LCDC_L0_KEY_COLOR				(0x9c)
#define LCDC_L0_WIN_SIZE				(0xa0)
#define LCDC_L0_WIN_OFS 				(0xa4)
#define LCDC_L0_SRC_WID 				(0xa8)
#define LCDC_L1_WIN_CTRL				(0xac)
#define LCDC_L1_SA0 					(0xb0)
#define LCDC_L1_KEY_COLOR				(0xb4)
#define LCDC_L1_WIN_SIZE				(0xb8)
#define LCDC_L1_WIN_OFS 				(0xbc)
#define LCDC_L1_SRC_WID 				(0xc0)
#define LCDC_DMA_STA					(0x128)
#define LCDC_FIFO_STATUS				(0x148)
#define LCDC_RF_PER 					(0x154)
#define LCDC_L0_SA1 					(0x160)
#define LCDC_L1_SA1 					(0x164)
#define LCDC_LP_CTRL					(0x1c0)
#define LCDC_MIPI_IF_MODE				(0x1d0)
#define LCDC_MIPI_DSI_CTRL				(0x1d8)
#define LCDC_AXIDMA_PRIOR				(0x1F0)
#define LCDC_DMA_CTRL					(0x1F4)
#define LCDC_DMA_READ_PERIOD				(0x1F8)


/* Definition controller bit for LCDC_MOD */
#define LCDC_MOD_LCD_OUT_FORMAT                         6
#define LCDC_MOD_RGB_EN                                 5
#define LCDC_MOD_EDPI_CMD_MOD                           4
#define LCDC_MOD_DMA_CLS_FIFO_RST_EN			3
#define LCDC_MOD_SRF_EN                                 2
#define LCDC_MOD_START                                  1
#define LCDC_MOD_EDPI_VIDEO_MOD                         0

/*define all interrupt type for LCDC interface*/
#define LCDC_TEAR_EFFECT_INTR                           23
#define LCDC_L1FIFO_UF_INTR                             18
#define LCDC_L0FIFO_UF_INTR                             17
#define LCDC_HVINT_INTR                                 15
#define LCDC_INT_HS_DN_INTR                             14
#define LCDC_INT_HS_UP_INTR                             13
#define LCDC_INT_VS_DN_INTR                             12
#define LCDC_INT_VS_UP_INTR                             11

#define LCDC_L1DMA_DEC_ERR_INTR                                 6
#define LCDC_L1DMA_ERR_INTR                                     5
#define LCDC_L1DMA_END_INTR                                     4

#define LCDC_L0DMA_DEC_ERR_INTR                                 3
#define LCDC_L0DMA_ERR_INTR                                     2
#define LCDC_L0DMA_END_INTR                                     1

#define LCDC_FTE_INTR                                           0

/* Definition controller bit for LCDC_HV_CTL */
#define LCDC_HV_CTL_BT656EN                                      10
#define LCDC_HV_CTL_CLKOEN                                      8
#define LCDC_HV_CTL_CLKPOL                                      7
#define LCDC_HV_CTL_HVOEN                                      6
#define LCDC_HV_CTL_HVCNTE                                      5

#define LCDC_HV_CTL_VS_POL                                      4
#define LCDC_HV_CTL_VS_POL_LOW                                  0
#define LCDC_HV_CTL_VS_POL_HIGH                                 1

#define LCDC_HV_CTL_HS_POL                                      3
#define LCDC_HV_CTL_HS_POL_LOW                                  0
#define LCDC_HV_CTL_HS_POL_HIGH                                 1

#define LCDC_HV_CTL_HVPIX_MOD                                    0

/* Definition controller bit for LCDC_HV_P */
#define LCDC_HV_P_VPR                                           16
#define LCDC_HV_P_HPR                                           0

/* Definition controller bit for LCDC_HV_HCTL */
#define LCDC_HV_HCTL_HETR                                       16
#define LCDC_HV_HCTL_HSTR                                       0

/* Definition controller bit for LCDC_HV_VCTL0 */
#define LCDC_HV_VCTL0_VETR0                                     16
#define LCDC_HV_VCTL0_VSTR0                                     0

/* Definition controller bit for LCDC_HV_HS */
#define LCDC_HV_HS_HEO                                          16
#define LCDC_HV_HS_HSO                                          0

/* Definition controller bit for LCDC_HV_VS */
#define LCDC_HV_VS_VEO                                          16
#define LCDC_HV_VS_VSO                                          0

/* Definition controller bit for LCDC_HV_F */
#define LCDC_HV_F_EVNR                                      16
#define LCDC_HV_F_ODDR                                      0

/* Definition controller bit for LCDC_HV_VINT */
#define LCDC_HV_VINT_VINTR                                      16
#define LCDC_HV_VINT_HINTR                                      0

/* Definition controller bit for LCDC_HV_CNTNOW */
#define LCDC_HV_CNTNOW_V_CNT                                      16
#define LCDC_HV_CNTNOW_H_CNT                                      0

/* Definition controller bit for LCDC_MIPI_DSI_AUTORESETTIME */
#define LCDC_MIPI_DSI_AUTORESETTIME_V		16
#define LCDC_MIPI_DSI_AUTORESETTIME_H		0

/* Definition controller bit for LCDC_MIPI_DSI_AUTORESETCTL */
#define LCDC_MIPI_DSI_AUTORESETCTL_PERIOD	8
#define LCDC_MIPI_DSI_AUTORESETCTL_EN		0

/* Definition controller bit for LCDC_MWIN_SIZE */
#define LCDC_MWIN_SIZE_Y_SIZE                                   16
#define LCDC_MWIN_SIZE_X_SIZE                                   0

/* Definition controller bit for LCDC_DISP_CTRL */
#define LCDC_DISP_CTRL_PARA_UP                                  24
#define LCDC_DISP_CTRL_LAYER1_EN                                9
#define LCDC_DISP_CTRL_LAYER0_EN                                8

/* Definition controller bit for LCDC_L0-1_WIN_CTRL */
#define LCDC_L_WIN_CTRL_DMA_PAUSE                                 25
#define LCDC_L_WIN_CTRL_UV_ORDER                                  24

#define LCDC_L_WIN_CTRL_BYTE_EX_EN                              19

#define LCDC_L_WIN_CTRL_DMA_FORCE_CLOSE                         18

#define LCDC_L_WIN_CTRL_FIFO_FLUSH                              16

#define LCDC_L_WIN_CTRL_DMA_CHEN                                15

#define LCDC_L_WIN_CTRL_TRANEN                                  13

#define LCDC_L_WIN_CTRL_KEYC_EN                                 12

#define LCDC_L_WIN_CTRL_FORMAT                                  9
#define LCDC_L_WIN_CTRL_FORMAT_RGB565                           0
#define LCDC_L_WIN_CTRL_FORMAT_RGB666                           1
#define LCDC_L_WIN_CTRL_FORMAT_RGB888                           2
#define LCDC_L_WIN_CTRL_FORMAT_YUV422                          3
#define LCDC_L_WIN_CTRL_FORMAT_RGB1555                          4
#define LCDC_L_WIN_CTRL_FORMAT_ARGB8888                         5
#define LCDC_L_WIN_CTRL_FORMAT_ABGR8888                         6
#define LCDC_L_WIN_CTRL_FORMAT_YUV420SP                         7

#define LCDC_L_WIN_CTRL_ALPHA_EN                                8
#define LCDC_L_WIN_CTRL_ALPHA_VAL                               0

/* Definition controller bit for LCDC_L0-1_WIN_SIZE */
#define LCDC_L_WIN_SIZE_Y_SIZE                                  16
#define LCDC_L_WIN_SIZE_X_SIZE                                  0

/* Definition controller bit for LCDC_L0-1_WIN_OFS */
#define LCDC_L_WIN_OFS_Y_OFFSET                                 16
#define LCDC_L_WIN_OFS_X_OFFSET                                 0

/* Definition controller bit for LCDC_DMA_STA */
#define LCDC_DMA_STA_L1DMA_STAT                                 1
#define LCDC_DMA_STA_L0DMA_STAT                                 0

/* Definition controller bit for LCDC_FIFO_STATUS */
#define LCDC_FIFO_STATUS_L1_NF1                                  23
#define LCDC_FIFO_STATUS_L1_HE1                                  22
#define LCDC_FIFO_STATUS_L1_AE1                                  21
#define LCDC_FIFO_STATUS_L1_EMPTY1                             20
#define LCDC_FIFO_STATUS_L0_NF1                                  19
#define LCDC_FIFO_STATUS_L0_HE1                                  18
#define LCDC_FIFO_STATUS_L0_AE1                                 17
#define LCDC_FIFO_STATUS_L0_EMPTY1                               16

#define LCDC_FIFO_STATUS_L1_NF0                                  7
#define LCDC_FIFO_STATUS_L1_HE0                                  6
#define LCDC_FIFO_STATUS_L1_AE0                                  5
#define LCDC_FIFO_STATUS_L1_EMPTY0                               4
#define LCDC_FIFO_STATUS_L0_NF0                                  3
#define LCDC_FIFO_STATUS_L0_HE0                                  2
#define LCDC_FIFO_STATUS_L0_AE0                                  1
#define LCDC_FIFO_STATUS_L0_EMPTY0                               0

/* Definition controller bit for LCDC_LP_CTRL */
#define LCDC_LP_CTRL_IDLE_LIMIT_AHB                                          24
#define LCDC_LP_CTRL_AHBIF_LP_EN                                 16
#define LCDC_LP_CTRL_LCDC_LP_EN                                   0

/* Definition controller bit for LCDC_MIPI_IF_MODE */
#define LCDC_EX_TE_MOD                                          5
#define LCDC_TEAR_EFFECT_SOURCE                                 4
#define LCDC_TEAR_EFFECT_SYNC_EN                                3
#define LCDC_MIPI_IF_MODE_BIT                                   0

/*Definition conroller LP MODE*/
#define LCDC_IDLE_AHB_VAL                                       10
#define LCDC_IDLE_LIMIT_AHB                                     24

#define LCDC_AHBIF_LP_EN                                        1
#define LCDC_AHBIF_LP_DIS                                       0
#define LCDC_AHBIF_LP                                           16

#define LCDC_LP_EN                                              1
#define LCDC_LP_DIS                                             0
#define LCDC_LP                                                 0

/* Definition controller bit for MIPI_DSI_CTRL */
#define LCDC_MIPI_DSI_CTRL_DPICOLORM                            1
#define LCDC_MIPI_DSI_CTRL_DPISHUTD                             0

/* Definition controller bit for LCDCn_AXIDMA_PRIOR */
#define LCDC_LAY1_DMA_PRIOR                                     4
#define LCDC_LAY0_DMA_PRIOR                                     0

/* Bit definition LCDC_DMA_CTRL */
#define LCDC_DMA_CTRL_KEEP_MEM_ALIVE				8
#define LCDC_DMA_CTRL_MAX_OS					0

#endif /* __ASM_ARCH_REGS_LCDC_H */
