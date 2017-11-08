#ifndef __ASM_ARCH_REGS_MIPI_H
#define __ASM_ARCH_REGS_MIPI_H


#define MIPI_VERSION					(0x00)
#define MIPI_PWR_UP 					(0x04)
#define MIPI_CLKMGR_CFG 				(0x08)
#define MIPI_DPI_VCID 					(0x0C)
#define MIPI_DPI_COLOR_CODING				(0x10)
#define MIPI_DPI_CFG_POL				(0x14)
#define MIPI_DPI_LP_CMD_TIM				(0x18)
#define MIPI_PCKHDL_CFG 				(0x2C)
#define MIPI_GEN_VCID 					(0x30)
#define MIPI_MODE_CFG					(0x34)
#define MIPI_VID_MODE_CFG				(0x38)
#define MIPI_VID_PKT_SIZE				(0x3C)
#define MIPI_VID_NUM_CHUNKS				(0x40)
#define MIPI_VID_NULL_SIZE				(0x44)
#define MIPI_VID_HSA_TIME				(0x48)
#define MIPI_VID_HBP_TIME				(0x4C)
#define MIPI_VID_HLINE_TIME				(0x50)
#define MIPI_VID_VSA_LINES				(0x54)
#define MIPI_VID_VBP_LINES				(0x58)
#define MIPI_VID_VFP_LINES				(0x5C)
#define MIPI_VID_VACT_LINES				(0x60)
#define MIPI_EDPI_CMD_SIZE				(0x64)
#define MIPI_CMD_MODE_CFG				(0x68)
#define MIPI_GEN_HDR					(0x6C)
#define MIPI_GEN_PLD_DATA				(0x70)
#define MIPI_CMD_PKD_STATUS				(0x74)
#define MIPI_TO_CNT_CFG					(0x78)
#define MIPI_HS_RD_TO_CNT				(0x7C)
#define MIPI_LP_RD_TO_CNT				(0x80)
#define MIPI_HS_WR_TO_CNT				(0x84)
#define MIPI_LP_WR_TO_CNT				(0x88)
#define MIPI_BTA_TO_CNT					(0x8C)
#define MIPI_SDF_3D					(0x90)
#define MIPI_LPCLK_CTRL					(0x94)
#define MIPI_PHY_TMR_LPCLK_CFG				(0x98)
#define MIPI_PHY_TMR_CFG				(0x9C)
#define MIPI_PHY_RSTZ					(0xA0)
#define MIPI_PHY_IF_CFG					(0xA4)
#define MIPI_PHY_ULPS_CTRL				(0xA8)
#define MIPI_PHY_TX_TRIGGERS				(0xAC)
#define MIPI_PHY_STATUS					(0xB0)
#define MIPI_PHY_TEST_CTRL0				(0xB4)
#define MIPI_PHY_TEST_CTRL1				(0xB8)
#define MIPI_ERROR_ST0					(0xBC)
#define MIPI_ERROR_ST1					(0xC0)
#define MIPI_ERROR_MSK0					(0xC4)
#define MIPI_ERROR_MSK1					(0xC8)

/* MIPI_CLKMGR_CFG. */
#define MIPI_TO_CLK_DIVISION				8
#define MIPI_TX_ESC_CLK_DIVISION			0

/* MIPI_DPI_COLOR_CODING. */
#define MIPI_EN18_LOOSELY				8
#define MIPI_DPI_COLOR_CODING_BIT			0

/* MIPI_DPI_CFG_POL*/
#define MIPI_COLORM_ACTIVE_LOW	  			4
#define MIPI_SHUTD_ACTIVE_LOW				3
#define MIPI_HSYNC_ACTIVE_LOW				2
#define MIPI_VSYNC_ACTIVE_LOW		  		1
#define MIPI_DATAEN_ACTIVE_LOW	  			0

/* MIPI_DPI_LP_CMD_TIM */
#define MIPI_OUTVACT_LPCMD_TIME				16
#define MIPI_INVACT_LPCMD_TIME				0

/* MIPI_PCKHDL_CFG. */
#define MIPI_EN_CRC_RX					4
#define MIPI_EN_ECC_RX					3
#define MIPI_EN_BTA 					2
#define MIPI_EN_EOTN_RX					1
#define MIPI_EN_EOTP_TX					0

/* MIPI_VID_MODE_CFG. */
#define MIPI_LP_CMD_EN					15
#define MIPI_FRAME_BTA_ACK			  	14
#define MIPI_LP_HFP_EN					13
#define MIPI_LP_HBP_EN					12
#define MIPI_LP_VACT_EN 				11
#define MIPI_LP_VFP_EN					10
#define MIPI_LP_VBP_EN					9
#define MIPI_LP_VSA_EN					8
#define MIPI_VID_MODE_TYPE				0

/* MIPI_CMD_MODE_CFG. */
#define MIPI_MAX_RD_PKT_SIZE	  			24
#define MIPI_DCS_LW_TX			  		19
#define MIPI_DCS_SR_0P_TX		  		18
#define MIPI_DCS_SW_1P_TX				17
#define MIPI_DCS_SW_0P_TX				16
#define MIPI_GEN_LW_TX					14

#define MIPI_GEN_SR_2P_TX		  		13
#define MIPI_GEN_SR_1P_TX				12
#define MIPI_GEN_SR_0P_TX				11	
#define MIPI_GEN_SW_2P_TX		  		10
#define MIPI_GEN_SW_1P_TX				9
#define MIPI_GEN_SW_0P_TX				8	 
#define MIPI_ACK_RQST_EN				1
#define MIPI_TEAR_FX_EN				  	0

/* MIPI_GEN_HDR. */
#define MIPI_GEN_WC_MS			  		16
#define MIPI_GEN_WC_LS			  		8
#define MIPI_GEN_VC			  		6
#define MIPI_GEN_DT			  		0

/* MIPI_CMD_PKT_STATUS. */
#define MIPI_GEN_RD_CMD_BUSY			  	6
#define MIPI_GEN_PLD_R_FULL				5
#define MIPI_GEN_PLD_R_EMPTY			  	4
#define MIPI_GEN_PLD_W_FULL				3
#define MIPI_GEN_PLD_W_EMPTY			  	2
#define MIPI_GEN_CMD_FULL				1
#define MIPI_GEN_CMD_EMPTY				0

/* MIPI_TO_CNT_CFG. */
#define MIPI_HSTX_TO_CNT			  	16
#define MIPI_LPRX_TO_CNT			  	0

/* MIPI_SDF_3D */
#define MIPI_SEND_3D_CFG				16
#define MIPI_RIGHT_FIRST				5
#define MIPI_SECOND_VSYNC				4
#define MIPI_FORMAT_3D					2
#define MIPI_MODE_3D					0

/* MIPI_LPCLK_CTRL */
#define MIPI_PHY_AUTO_CLKLANE_CTRL			1
#define MIPI_PHY_TXREQUESTCLKHS				0

/*  MIPI_PHY_TMR_LPCLK_CFG */
#define MIPI_PHY_CLKHS2LP_TIME				16
#define MIPI_PHY_CLKLP2HS_TIME				0

/* MIPI_PHY_TMR_CFG */
#define MIPI_PHY_HS2LP_TIME				24
#define MIPI_PHY_LP2HS_TIME				16
#define MIPI_MAX_RD_TIME				0

/* MIPI_PHY_RSTZ. */
#define MIPI_PHY_FORCEPLL		  		3
#define MIPI_PHY_ENABLE			  	2
#define MIPI_PHY_RESET			  		1
#define MIPI_PHY_SHUTDOWN			  	0

/* MIPI_PHY_IF_CFG. */
#define MIPI_PHY_STOP_WAIT_TIME			  	8
#define MIPI_PHY_N_LANES			  	0

/* MIPI_PHY_ULPS_CTRL */
#define MIPI_PHY_TXEXITULPSLAN				3
#define MIPI_PHY_TXREQULPSLAN				2
#define MIPI_PHY_TXEXITULPSCLK				1
#define MIPI_PHY_TXREQULPSCLK				0

/* MIPI_PHY_STATUS*/
#define MIPI_ULPSACTIVENOT3LANE				12
#define MIPI_PHYSTOPSTATE3LANE				11
#define MIPI_ULPSACTIVENOT2LANE				10
#define MIPI_PHYSTOPSTATE2LANE				9
#define MIPI_ULPSACTIVENOT1LANE				8
#define MIPI_PHYSTOPSTATE1LANE				7
#define MIPI_RXULPSESC0LANE				6
#define MIPI_ULPSACTIVENOT0LANE				5
#define MIPI_PHYSTOPSTATE0LANE				4
#define MIPI_PHYULPSACTIVENOTCLK			3
#define MIPI_PHYSTOPSTATECLKLANE			2
#define MIPI_PHYDIRECTION				1
#define MIPI_PHYCLOCK					0

/* MIPI_PHY_TEST_CTRL0*/
#define MIPI_CTRL0_PHY_TESTCLK				1
#define MIPI_CTRL0_PHY_TESTCLR				0

/* MIPI_PHY_TEST_CTRL1*/
#define MIPI_CTRL1_PHY_TESTEN				16
#define MIPI_CTRL1_PHY_TESTDOUT				8
#define MIPI_CTRL1_PHY_TESTDIN				0

/* MIPI_ERROR_ST0&MIPI_ERROR_MSK0. */
#define MIPI_ERROR_DPHY_ERRORS_4                        20
#define MIPI_ERROR_DPHY_ERRORS_3                        19
#define MIPI_ERROR_DPHY_ERRORS_2                        18
#define MIPI_ERROR_DPHY_ERRORS_1                        17
#define MIPI_ERROR_DPHY_ERRORS_0                        16
#define MIPI_ERROR_ACK_WITH_ERR_15                      15
#define MIPI_ERROR_ACK_WITH_ERR_14                      14
#define MIPI_ERROR_ACK_WITH_ERR_13                      13
#define MIPI_ERROR_ACK_WITH_ERR_12                      12
#define MIPI_ERROR_ACK_WITH_ERR_11                      11
#define MIPI_ERROR_ACK_WITH_ERR_10                      10
#define MIPI_ERROR_ACK_WITH_ERR_9                       9
#define MIPI_ERROR_ACK_WITH_ERR_8                       8
#define MIPI_ERROR_ACK_WITH_ERR_7                       7
#define MIPI_ERROR_ACK_WITH_ERR_6                       6
#define MIPI_ERROR_ACK_WITH_ERR_5                       5
#define MIPI_ERROR_ACK_WITH_ERR_4                       4
#define MIPI_ERROR_ACK_WITH_ERR_3                       3
#define MIPI_ERROR_ACK_WITH_ERR_2                       2
#define MIPI_ERROR_ACK_WITH_ERR_1                       1
#define MIPI_ERROR_ACK_WITH_ERR_0                       0

/* MIPI_ERROR_ST1&MIPI_ERROR_MSK1. */
#define MIPI_ERROR_GEN_PLD_RECV_ERR                     12
#define MIPI_ERROR_GEN_PLD_RD_ERR                       11
#define MIPI_ERROR_GEN_PLD_SEND_ERR                     10
#define MIPI_ERROR_GEN_PLD_WR_ERR                       9
#define MIPI_ERROR_GEN_CMD_WR_ERR                       8
#define MIPI_ERROR_DPI_PLD_WR_ERR                       7
#define MIPI_ERROR_EOPT_ERR                             6
#define MIPI_ERROR_PKT_SIZE_ERR                         5
#define MIPI_ERROR_CRC_ERR                              4
#define MIPI_ERROR_ECC_MULTI_ERR                        3
#define MIPI_ERROR_ECC_SINGLE_ERR                       2
#define MIPI_ERROR_TO_LP_RX                             1
#define MIPI_ERROR_TO_HS_RX                             0

#endif /* __ASM_ARCH_REGS_MIPI_H */
