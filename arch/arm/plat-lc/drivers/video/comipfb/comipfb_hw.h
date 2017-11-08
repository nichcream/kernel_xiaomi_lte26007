/* driver/video/comipfb_hw.h
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 1999-2008  LeadCoreTech Corp.
**
**  PURPOSE:   		This files contains the driver of LCD controller.
**
**  CHANGE HISTORY:
**
**	Version		Date		Author			Description
**	0.1.0		2009-03-10	liuchangmin		created
**	0.1.1		2011-03-10	zouqiao	updated
**
*/

#ifndef __COMIP_HW_H__
#define __COMIP_HW_H__

#include <plat/comipfb.h>

/* Definition controller bit for LCDC_MOD */
#define LCDC_MOD_ACMD_EN    	        			9
#define LCDC_MOD_SRF_EN    	        			8
#define LCDC_MOD_LCD_OUT_FORMAT					6
#define LCDC_MOD_LCD_OUT_FORMAT_RGB				0
#define LCDC_MOD_LCD_OUT_FORMAT_YCRCB				1
#define LCDC_MOD_START    	        			4
#define LCDC_MOD_MIPI_EN					3 //MIPI ENABLE
#define LCDC_MOD_ASY_EN						2
#define LCDC_MOD_HV_EN						1
#define LCDC_MOD_INS_HIGH					0
#define LCDC_MOD_INS_HIGH_LOW	        			0
#define LCDC_MOD_INS_HIGH_HIGH					1

/*define all interrupt type for LCDC interface*/
#define LCDC_HVINT_INTR	  					12
#define LCDC_HS_DN_INTR  					11
#define LCDC_HS_UP_INTR  					10
#define LCDC_VS_DN_INTR  					9
#define LCDC_VS_UP_INTR  					8
#define LCDC_WBDMA_ERR_INTR  					7
#define LCDC_L5DMA_ERR_INTR 					6
#define LCDC_L4DMA_ERR_INTR  					5
#define LCDC_L3DMA_ERR_INTR  					4
#define LCDC_L2DMA_ERR_INTR  					3
#define LCDC_L1DMA_ERR_INTR  					2
#define LCDC_L0DMA_L0DMA_ERR  					1
#define LCDC_FTE_INTR  						0

/* Definition controller bit for LCDC_ASYTIM0-1 */
#define LCDC_ASYTIM_TH2						20
#define LCDC_ASYTIM_TS2    	        			18
#define LCDC_ASYTIM_TS1		        			16
#define LCDC_ASYTIM_RW_FP		        		8
#define LCDC_ASYTIM_AC_P	        			0

/* Definition controller bit for LCDC_ASYCTL */
#define LCDC_ASYCTL_SUB_PANEL					2
#define LCDC_ASYCTL_SUB_PANEL_MAIN				0
#define LCDC_ASYCTL_SUB_PANEL_SUB				1

#define LCDC_ASYCTL_ASY_MOD					0
#define LCDC_ASYCTL_ASY_MOD_16BIT				0
#define LCDC_ASYCTL_ASY_MOD_8BIT				1
#define LCDC_ASYCTL_ASY_MOD_18BIT				2
#define LCDC_ASYCTL_ASY_MOD_9BIT				3

/* Definition controller bit for LCDC_HV_CTL */
#define LCDC_HV_CTL_HSIN_POL					9
#define LCDC_HV_CTL_HSIN_POL_UP					0
#define LCDC_HV_CTL_HSIN_POL_DOWN				1

#define LCDC_HV_CTL_CLKOEN					8
#define LCDC_HV_CTL_CLKPOL					7
#define LCDC_HV_CTL_CLKPOL_DOWN					0
#define LCDC_HV_CTL_CLKPOL_UP					1

#define LCDC_HV_CTL_HVOEN					6
#define LCDC_HV_CTL_HVCNTE					5
#define LCDC_HV_CTL_VS_POL					4
#define LCDC_HV_CTL_VS_POL_LOW					0
#define LCDC_HV_CTL_VS_POL_HIGH					1

#define LCDC_HV_CTL_HS_POL					3
#define LCDC_HV_CTL_HS_POL_LOW					0
#define LCDC_HV_CTL_HS_POL_HIGH					1

#define LCDC_HV_CTL_BT656EN					2
#define LCDC_HV_CTL_HVPIX_MOD					0
#define LCDC_HV_CTL_HVPIX_MOD_8BITS				0
#define LCDC_HV_CTL_HVPIX_MOD_16BITS				1
#define LCDC_HV_CTL_HVPIX_MOD_18BITS				2

/* Definition controller bit for LCDC_HV_P */
#define LCDC_HV_P_VPR						16
#define LCDC_HV_P_HPR						0

/* Definition controller bit for LCDC_HV_SYNC */
#define LCDC_HV_SYNC_VT						16
#define LCDC_HV_SYNC_HT						0

/* Definition controller bit for LCDC_HV_HCTL */
#define LCDC_HV_HCTL_HETR					16
#define LCDC_HV_HCTL_HSTR					0

/* Definition controller bit for LCDC_HV_VCTL0 */
#define LCDC_HV_VCTL0_VETR0					16
#define LCDC_HV_VCTL0_VSTR0					0

/* Definition controller bit for LCDC_HV_VCTL1 */
#define LCDC_HV_VCTL1_VETR1					16
#define LCDC_HV_VCTL1_VSTR1					0

/* Definition controller bit for LCDC_HV_HS */
#define LCDC_HV_HS_HEO						16
#define LCDC_HV_HS_HSO						0

/* Definition controller bit for LCDC_HV_VS */
#define LCDC_HV_VS_VEO						16
#define LCDC_HV_VS_VSO						0

/* Definition controller bit for LCDC_HV_F */
#define LCDC_HV_F_EVNR						16
#define LCDC_HV_F_ODDR						0

/* Definition controller bit for LCDC_HV_VINT */
#define LCDC_HV_VINT_VINTR					16
#define LCDC_HV_VINT_HINTR					0

/* Definition controller bit for LCDC_MWIN_SIZE */
#define LCDC_MWIN_SIZE_Y_SIZE					16
#define LCDC_MWIN_SIZE_X_SIZE					0

/*Definition conroller LP MODE*/
#define LCDC_IDLE_AHB_VAL					10
#define LCDC_IDLE_LIMIT_AHB					24
#define LCDC_LP							0
#define LCDC_LP_EN						1
#define LCDC_AHBIF_LP						16
#define LCDC_AHBIF_LP_EN					1

/*Definition Mipi Dsi Ctl*/
#define MIPI_DSI_CTL_EN						0
#define MIPI_DSI_CTL_DIS					3

/*HW CLR FIFO RST FUNC*/
#define LCDC_MOD_DMA_CLS_FIFO_RST				11
#define LCDC_MOD_DMA_CLS_FIFO_RST_EN				1
#define LCDC_MOD_DMA_CLS_FIFO_RST_DIS				0

/* Definition controller bit for LCDC_DISP_CTRL */
#define LCDC_DISP_CTRL_PARA_UP					24
#define LCDC_DISP_CTRL_LAYER5_EN				13
#define LCDC_DISP_CTRL_LAYER4_EN				12
#define LCDC_DISP_CTRL_LAYER3_EN				11
#define LCDC_DISP_CTRL_LAYER2_EN				10
#define LCDC_DISP_CTRL_LAYER1_EN				9
#define LCDC_DISP_CTRL_LAYER0_EN				8
#define LCDC_DISP_CTRL_LSB_FIRST				1
#define LCDC_DISP_CTRL_LSB_FIRST_LSB				1
#define LCDC_DISP_CTRL_LSB_FIRST_MSB				0

#define LCDC_DISP_CTRL_BGR_SEQ					0
#define LCDC_DISP_CTRL_BGR_SEQ_RGB				0
#define LCDC_DISP_CTRL_BGR_SEQ_BGR				1

/* Definition controller bit for LCDC_L0-5_WIN_CTRL */
#define LCDC_L_WIN_CTRL_BYTE_EX_EN				19

#define LCDC_L_WIN_CTRL_DMA_TRANS_MODE				17
#define LCDC_L_WIN_CTRL_DMA_HIGH_SPEED				1
#define LCDC_L_WIN_CTRL_DMA_HIGH_COMMON				0

#define LCDC_L_WIN_CTRL_FIFO_FLUSH				16

#define LCDC_L_WIN_CTRL_DMA_CHEN				15

#define LCDC_L_WIN_CTRL_DMA_MODE				14
#define LCDC_L_WIN_CTRL_DMA_MODE_DMA				1
#define LCDC_L_WIN_CTRL_DMA_MODE_CPU				0

#define LCDC_L_WIN_CTRL_TRANEN					13

#define LCDC_L_WIN_CTRL_KEYC_EN					12

#define LCDC_L_WIN_CTRL_FORMAT					9
#define LCDC_L_WIN_CTRL_FORMAT_RGB565				0
#define LCDC_L_WIN_CTRL_FORMAT_RGB666				1
#define LCDC_L_WIN_CTRL_FORMAT_RGB888				2
#define LCDC_L_WIN_CTRL_FORMAT_RGB332				3
#define LCDC_L_WIN_CTRL_FORMAT_YCRCB422				3
#define LCDC_L_WIN_CTRL_FORMAT_ARGB8888				5
#define LCDC_L_WIN_CTRL_FORMAT_ABGR8888				6

#define LCDC_L_WIN_CTRL_ALPHA_EN				8
#define LCDC_L_WIN_CTRL_ALPHA					0

/* Definition controller bit for LCDC_L0-5_WIN_SIZE */
#define LCDC_L_WIN_SIZE_Y_SIZE					16
#define LCDC_L_WIN_SIZE_X_SIZE					0

/* Definition controller bit for LCDC_L0-5_WIN_OFS */
#define LCDC_L_WIN_OFS_Y_OFFSET					16
#define LCDC_L_WIN_OFS_X_OFFSET					0

/* Definition controller bit for LCDC_DMA_STA */
#define LCDC_DMA_STA_WBDMA_STAT					6
#define LCDC_DMA_STA_L5DMA_STAT					5
#define LCDC_DMA_STA_L4DMA_STAT					4
#define LCDC_DMA_STA_L3DMA_STAT					3
#define LCDC_DMA_STA_L2DMA_STAT					2
#define LCDC_DMA_STA_L1DMA_STAT					1
#define LCDC_DMA_STA_L0DMA_STAT					0

/* Definition controller bit for LCDC_FIFO_STATUS */
#define LCDC_FIFO_STATUS_WB_FULL				27
#define LCDC_FIFO_STATUS_WB_AF					26
#define LCDC_FIFO_STATUS_WB_HF					25
#define LCDC_FIFO_STATUS_WB_NE					24
#define LCDC_FIFO_STATUS_L5_NF					23
#define LCDC_FIFO_STATUS_L5_HE					22
#define LCDC_FIFO_STATUS_L5_AE					21
#define LCDC_FIFO_STATUS_L5_EMPTY				20
#define LCDC_FIFO_STATUS_L4_NF					19
#define LCDC_FIFO_STATUS_L4_HE					18
#define LCDC_FIFO_STATUS_L4_AE					17
#define LCDC_FIFO_STATUS_L4_EMPTY				16
#define LCDC_FIFO_STATUS_L3_NF					15
#define LCDC_FIFO_STATUS_L3_HE					14
#define LCDC_FIFO_STATUS_L3_AE					13
#define LCDC_FIFO_STATUS_L3_EMPTY				12
#define LCDC_FIFO_STATUS_L2_NF					11
#define LCDC_FIFO_STATUS_L2_HE					10
#define LCDC_FIFO_STATUS_L2_AE					9
#define LCDC_FIFO_STATUS_L2_EMPTY				8
#define LCDC_FIFO_STATUS_L1_NF					7
#define LCDC_FIFO_STATUS_L1_HE					6
#define LCDC_FIFO_STATUS_L1_AE					5
#define LCDC_FIFO_STATUS_L1_EMPTY				4
#define LCDC_FIFO_STATUS_L0_NF					3
#define LCDC_FIFO_STATUS_L0_HE					2
#define LCDC_FIFO_STATUS_L0_AE					1
#define LCDC_FIFO_STATUS_L0_EMPTY				0

#define LCDC_LAYER_CFG_CTRL					(0x00000001)
#define LCDC_LAYER_CFG_WIN					(0x00000002)
#define LCDC_LAYER_CFG_BUF					(0x00000004)
#define LCDC_LAYER_CFG_ALL					(0xffffffff)

/******************************************************************************
*  Extern Functions
******************************************************************************/
struct comipfb_info;
struct comipfb_layer_info;

extern void lcdc_init(struct comipfb_info *fbi);
extern void lcdc_exit(struct comipfb_info *fbi);
extern void lcdc_enable(struct comipfb_info *fbi, unsigned char enable);
extern void lcdc_suspend(struct comipfb_info *fbi);
extern void lcdc_resume(struct comipfb_info *fbi);
extern void lcdc_start(struct comipfb_info *fbi, unsigned char start);
extern void lcdc_layer_config(struct comipfb_layer_info *layer, unsigned int flag);
extern void lcdc_layer_enable(struct comipfb_layer_info *layer, unsigned char enable);
extern void lcdc_cmd(struct comipfb_info *fbi, unsigned int cmd);
extern void lcdc_cmd_para(struct comipfb_info *fbi, unsigned int data);
extern void lcdc_int_enable(struct comipfb_info *fbi, unsigned int status, unsigned char enable);
extern void lcdc_int_clear(struct comipfb_info *fbi, unsigned int status);
extern unsigned int lcdc_int_status(struct comipfb_info *fbi);

#endif /*__COMIP_HW_H__*/
