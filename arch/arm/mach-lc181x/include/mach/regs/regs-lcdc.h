#ifndef __ASM_ARCH_REGS_LCDC_H
#define __ASM_ARCH_REGS_LCDC_H

#if defined (CONFIG_CPU_LC1810)
#define LCDC_MOD					(0x00)
#define LCDC_IR 					(0x04)
#define LCDC_CDR					(0x204)
#define LCDC_LAYER0_DR					(0x08)
#define LCDC_IREAD					(0x0C)
#define LCDC_PREAD					(0x10)
#define LCDC_INTC					(0x14)
#define LCDC_INTF					(0x18)
#define LCDC_INTE					(0x1C)
#define LCDC_ASYTIM0					(0x20)
#define LCDC_ASYCTL 					(0x24)
#define LCDC_ASYTIM1					(0x28)
#define LCDC_HV_CTL 					(0x30)
#define LCDC_HV_P					(0x34)
#define LCDC_HV_SYNC					(0x38)
#define LCDC_HV_HCTL					(0x3C)
#define LCDC_HV_VCTL0					(0x40)
#define LCDC_HV_VCTL1					(0x44)
#define LCDC_HV_HS					(0x48)
#define LCDC_HV_VS					(0x4C)
#define LCDC_HV_F					(0x50)
#define LCDC_HV_VINT					(0x54)
#define LCDC_HV_DM					(0x58)
#define LCDC_WB_DR					(0x5c)
#define LCDC_LAYER1_DR					(0x60)
#define LCDC_LAYER2_DR					(0x64)
#define LCDC_MWIN_SIZE					(0x74)
#define LCDC_MWIN_BCOLOR				(0x78)
#define LCDC_DISP_CTRL					(0x7c)
#define LCDC_WB_CTRL					(0x80)
#define LCDC_WB_SA					(0x84)
#define LCDC_WB_OFS 					(0x8c)
#define LCDC_WB_SIZE					(0x90)
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
#define LCDC_L2_WIN_CTRL				(0xc4)
#define LCDC_L2_SA0 					(0xc8)
#define LCDC_L2_KEY_COLOR				(0xcc)
#define LCDC_L2_WIN_SIZE				(0xd0)
#define LCDC_L2_WIN_OFS 				(0xd4)
#define LCDC_L2_SRC_WID 				(0xd8)
#define LCDC_DMA_STA					(0x128)
#define LCDC_L0DMA_CTA					(0x12c)
#define LCDC_L1DMA_CTA					(0x130)
#define LCDC_L2DMA_CTA					(0x134)
#define LCDC_FIFO_STATUS				(0x148)
#define LCDC_HC_PER 					(0x150)
#define LCDC_RF_PER 					(0x154)
#define LCDC_L0_SA1 					(0x160)
#define LCDC_L1_SA1 					(0x164)
#define LCDC_L2_SA1 					(0x168)
#define LCDC_CMD0					(0x180)
#define LCDC_CMD1					(0x184)
#define LCDC_CMD2					(0x188)
#define LCDC_CMD3					(0x18c)
#define LCDC_CMD4					(0x190)
#define LCDC_CMD5					(0x194)
#define LCDC_CMD6					(0x198)
#define LCDC_CMD7					(0x19c)
#define LCDC_CURSOR_CTRL				(0x1a0)
#define LCDC_CURSOR_COLOR0				(0x1a8)
#define LCDC_CURSOR_COLOR1				(0x1ac)
#define LCDC_CURSOR_OFS 				(0x1b0)
#define LCDC_CURSOR_SIZE				(0x1b4)
#define LCDC_LP_CTRL					(0x1c0)
#define LCDC_MIPI_IF_MODE				(0x1d0)
#define LCDC_HDMI_IF_MODE				(0x1d4)
#define LCDC_MIPI_DSI_CTRL				(0x1d8)
#define LCDC_AXIDMA_PRIOR				(0x1F0)
#define LCDC_CURSOR_RAM 				(0x400)

#elif defined(CONFIG_CPU_LC1813)
#define LCDC_MOD					(0x00)
#define LCDC_LAYER0_DR					(0x08)
#define LCDC_INTC					(0x14)
#define LCDC_INTF					(0x18)
#define LCDC_INTE					(0x1C)
#define LCDC_TIME1000US					(0x28)
#define LCDC_HV_CTL 					(0x30)
#define LCDC_HV_P					(0x34)
#define LCDC_HV_HCTL					(0x3C)
#define LCDC_HV_VCTL0					(0x40)
#define LCDC_HV_VCTL1					(0x44)
#define LCDC_HV_HS					(0x48)
#define LCDC_HV_VS					(0x4C)
#define LCDC_HV_VINT					(0x54)
#define LCDC_HV_DM					(0x58)
#define LCDC_HV_CNTNOW					(0x5C)	/* L1813S */
#define LCDC_MIPI_DSI_AUTORESETTIME			(0x60)	/* L1813S */
#define LCDC_MIPI_DSI_AUTORESETCTL			(0x64)	/* L1813S */
#define LCDC_MWIN_SIZE					(0x74)
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

#define LCDC_L2_WIN_CTRL				(0xc4)
#define LCDC_L2_SA0 					(0xc8)
#define LCDC_L2_KEY_COLOR				(0xcc)
#define LCDC_L2_WIN_SIZE				(0xd0)
#define LCDC_L2_WIN_OFS 				(0xd4)
#define LCDC_L2_SRC_WID 				(0xd8)

#define LCDC_DMA_STA					(0x128)
#define LCDC_L0DMA_CTA					(0x12c)
#define LCDC_L1DMA_CTA					(0x130)
#define LCDC_L2DMA_CTA					(0x134)
#define LCDC_FIFO_STATUS				(0x148)
#define LCDC_HC_PER 					(0x150)
#define LCDC_RF_PER 					(0x154)
#define LCDC_L0_SA1 					(0x160)
#define LCDC_L1_SA1 					(0x164)
#define LCDC_L2_SA1 					(0x168)

#define LCDC_CURSOR_CTRL				(0x1a0)
#define LCDC_CURSOR_COLOR0				(0x1a8)
#define LCDC_CURSOR_COLOR1				(0x1ac)
#define LCDC_CURSOR_OFS 				(0x1b0)
#define LCDC_CURSOR_SIZE				(0x1b4)

#define LCDC_LP_CTRL					(0x1c0)
#define LCDC_MIPI_IF_MODE				(0x1d0)
#define LCDC_MIPI_DSI_CTRL				(0x1d8)
#define LCDC_AXIDMA_PRIOR				(0x1F0)
#define LCDC_CURSOR_RAM 				(0x400)

/* Definition controller bit for LCDC_MOD */
#define LCDC_MOD_EDPI_CMD_MOD                                   4
#define LCDC_MOD_DMA_CLS_FIFO_RST_EN                            3
#define LCDC_MOD_SRF_EN                                         2
#define LCDC_MOD_START                                          1
#define LCDC_MOD_EDPI_VIDEO_MOD                                 0

/*define all interrupt type for LCDC interface*/
#define LCDC_L2DMA_DEC_ERR_INTR                                 22
#define LCDC_L2DMA_ERR_INTR                                     21
#define LCDC_L2DMA_END_INTR                                     20

#define LCDC_L2FIFO_UF_INTR                                     19
#define LCDC_L1FIFO_UF_INTR                                     18
#define LCDC_L0FIFO_UF_INTR                                     17

#define LCDC_CURSOR_DONE_INTR                                   16
#define LCDC_HVINT_INTR                                         15
#define LCDC_TEAR_EFFECT_INTR                                   13

#define LCDC_L1DMA_DEC_ERR_INTR                                 6
#define LCDC_L1DMA_ERR_INTR                                     5
#define LCDC_L1DMA_END_INTR                                     4

#define LCDC_L0DMA_DEC_ERR_INTR                                 3
#define LCDC_L0DMA_ERR_INTR                                     2
#define LCDC_L0DMA_END_INTR                                     1

#define LCDC_FTE_INTR                                           0

/* Definition controller bit for LCDC_HV_CTL */
#define LCDC_HV_CTL_HVCNTE                                      5

#define LCDC_HV_CTL_VS_POL                                      4
#define LCDC_HV_CTL_VS_POL_LOW                                  0
#define LCDC_HV_CTL_VS_POL_HIGH                                 1

#define LCDC_HV_CTL_HS_POL                                      3
#define LCDC_HV_CTL_HS_POL_LOW                                  0
#define LCDC_HV_CTL_HS_POL_HIGH                                 1

/* Definition controller bit for LCDC_HV_P */
#define LCDC_HV_P_VPR                                           16
#define LCDC_HV_P_HPR                                           0

/* Definition controller bit for LCDC_HV_HCTL */
#define LCDC_HV_HCTL_HETR                                       16
#define LCDC_HV_HCTL_HSTR                                       0

/* Definition controller bit for LCDC_HV_VCTL0 */
#define LCDC_HV_VCTL0_VETR0                                     16
#define LCDC_HV_VCTL0_VSTR0                                     0

/* Definition controller bit for LCDC_HV_VCTL1 */
#define LCDC_HV_VCTL1_VETR1                                     16
#define LCDC_HV_VCTL1_VSTR1                                     0

/* Definition controller bit for LCDC_HV_HS */
#define LCDC_HV_HS_HEO                                          16
#define LCDC_HV_HS_HSO                                          0

/* Definition controller bit for LCDC_HV_VS */
#define LCDC_HV_VS_VEO                                          16
#define LCDC_HV_VS_VSO                                          0

/* Definition controller bit for LCDC_HV_VINT */
#define LCDC_HV_VINT_VINTR                                      16
#define LCDC_HV_VINT_HINTR                                      0

/* Definition controller bit for LCDC_HV_CNTNOW */
#define LCDC_HV_VINT_CNTNOW_V_CNT			16
#define LCDC_HV_VINT_CNTNOW_H_CNT			0

/* Definition controller bit for LCDC_MIPI_DSI_AUTORESETTIME */
#define LCDC_MIPI_DSI_AUTORESETTIME_V		16
#define LCDC_MIPI_DSI_AUTORESETTIME_H		0

/* Definition controller bit for LCDC_MIPI_DSI_AUTORESETCTL */
#define LCDC_MIPI_DSI_AUTORESETCTL_PERIOD	8
#define LCDC_MIPI_DSI_AUTORESETCTL_EN		0

/* Definition controller bit for LCDC_MWIN_SIZE */
#define LCDC_MWIN_SIZE_Y_SIZE                                   16
#define LCDC_MWIN_SIZE_X_SIZE                                   0

/*HW CLR FIFO RST FUNC*/
//#define LCDC_MOD_DMA_CLS_FIFO_RST                               11
//#define LCDC_MOD_DMA_CLS_FIFO_RST_EN                            1
//#define LCDC_MOD_DMA_CLS_FIFO_RST_DIS                           0

/* Definition controller bit for LCDC_DISP_CTRL */
#define LCDC_DISP_CTRL_PARA_UP                                  24

#define LCDC_DISP_CTRL_LAYER2_SA_SEL                            18
#define LCDC_DISP_CTRL_LAYER1_SA_SEL                            17
#define LCDC_DISP_CTRL_LAYER0_SA_SEL                            16

#define LCDC_DISP_CTRL_LAYER2_EN                                10
#define LCDC_DISP_CTRL_LAYER1_EN                                9
#define LCDC_DISP_CTRL_LAYER0_EN                                8

/* Definition controller bit for LCDC_L0-2_WIN_CTRL */
#define LCDC_L_WIN_CTRL_MAX_OS                                  20
#define LCDC_L_WIN_CTRL_MAX_OS_VAL								2

#define LCDC_L_WIN_CTRL_BYTE_EX_EN                              19

#define LCDC_L_WIN_CTRL_DMA_FORCE_CLOSE                         18

#define LCDC_L_WIN_CTRL_DMA_TRANS_MODE                          17
#define LCDC_L_WIN_CTRL_DMA_HIGH_SPEED                          1
#define LCDC_L_WIN_CTRL_DMA_HIGH_COMMON                         0

#define LCDC_L_WIN_CTRL_FIFO_FLUSH                              16

#define LCDC_L_WIN_CTRL_DMA_CHEN                                15

#define LCDC_L_WIN_CTRL_DMA_MODE                                14
#define LCDC_L_WIN_CTRL_DMA_MODE_DMA                            1
#define LCDC_L_WIN_CTRL_DMA_MODE_CPU                            0

#define LCDC_L_WIN_CTRL_TRANEN                                  13

#define LCDC_L_WIN_CTRL_KEYC_EN                                 12

#define LCDC_L_WIN_CTRL_FORMAT                                  9
#define LCDC_L_WIN_CTRL_FORMAT_RGB565                           0
#define LCDC_L_WIN_CTRL_FORMAT_RGB666                           1
#define LCDC_L_WIN_CTRL_FORMAT_RGB888                           2
#define LCDC_L_WIN_CTRL_FORMAT_YUV422	                        3
#define LCDC_L_WIN_CTRL_FORMAT_RGB1555                          4
#define LCDC_L_WIN_CTRL_FORMAT_ARGB8888                         5
#define LCDC_L_WIN_CTRL_FORMAT_ABGR8888                         6

#define LCDC_L_WIN_CTRL_ALPHA_EN                                8
#define LCDC_L_WIN_CTRL_ALPHA_VAL                               0

/* Definition controller bit for LCDC_L0-2_WIN_SIZE */
#define LCDC_L_WIN_SIZE_Y_SIZE                                  16
#define LCDC_L_WIN_SIZE_X_SIZE                                  0

/* Definition controller bit for LCDC_L0-2_WIN_OFS */
#define LCDC_L_WIN_OFS_Y_OFFSET                                 16
#define LCDC_L_WIN_OFS_X_OFFSET                                 0

/* Definition controller bit for LCDC_DMA_STA */
#define LCDC_DMA_STA_L2DMA_STAT                                 2
#define LCDC_DMA_STA_L1DMA_STAT                                 1
#define LCDC_DMA_STA_L0DMA_STAT                                 0

/* Definition controller bit for LCDC_FIFO_STATUS */
#define LCDC_FIFO_STATUS_L2_NF                                  11
#define LCDC_FIFO_STATUS_L2_HE                                  10
#define LCDC_FIFO_STATUS_L2_AE                                  9
#define LCDC_FIFO_STATUS_L2_EMPTY                               8
#define LCDC_FIFO_STATUS_L1_NF                                  7
#define LCDC_FIFO_STATUS_L1_HE                                  6
#define LCDC_FIFO_STATUS_L1_AE                                  5
#define LCDC_FIFO_STATUS_L1_EMPTY                               4
#define LCDC_FIFO_STATUS_L0_NF                                  3
#define LCDC_FIFO_STATUS_L0_HE                                  2
#define LCDC_FIFO_STATUS_L0_AE                                  1
#define LCDC_FIFO_STATUS_L0_EMPTY                               0

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

/*Definition Mipi Dsi Ctl*/
#define LCDC_MIPI_DSI_CTRL_DPICOLORM                            1
#define LCDC_MIPI_DSI_CTRL_DPISHUTD                             0

/* Definition controller bit for LCDCn_AXIDMA_PRIOR */
#define LCDC_LAY2_DMA_PRIOR                                     8
#define LCDC_LAY1_DMA_PRIOR                                     4
#define LCDC_LAY0_DMA_PRIOR                                     0
#endif

#endif /* __ASM_ARCH_REGS_LCDC_H */
