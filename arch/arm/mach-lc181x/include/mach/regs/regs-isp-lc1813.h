#ifndef __ASM_ARCH_REGS_ISP_LC1813_H
#define __ASM_ARCH_REGS_ISP_LC1813_H

/* 1. progrem buffer(112KB): 0x0 ~ 0x01BFFF   */
/* 2. data buffer (12KB): 0x01C000 ~ 0x01EFFF */
/* 3. cache buffer (4KB): 0x01F000 ~ 0x01FFFF */
/* 4. line buffer(192KB): 0x020000 ~ 0x04FFFF */
/* 5. HW register(128KB): 0x050000 ~ 0x06FFFF */

#define FIRMWARE_BASE				(0)
#define COMMAND_BUFFER				(0x1ea00)//(0x16a00)
#define COMMAND_BUFFER_VIV			(0x1ec00)

/* Command set.*/
#define COMMAND_REG0				(0x63900)
#define COMMAND_REG1				(0x63901)
#define COMMAND_REG2				(0x63902)
#define COMMAND_REG3				(0x63903)
#define COMMAND_REG4				(0x63904)
#define COMMAND_REG5				(0x63905)
#define COMMAND_REG6				(0x63906)
#define COMMAND_REG7				(0x63907)
#define COMMAND_REG8				(0x63908)
#define COMMAND_REG9				(0x63909)
#define COMMAND_REG10				(0x6390a)
#define COMMAND_REG11				(0x6390b)
#define COMMAND_REG12				(0x6390c)
#define COMMAND_REG13				(0x6390d)
#define COMMAND_REG14				(0x6390e)
#define COMMAND_REG15				(0x6390f)
#define COMMAND_FINISHED			(0x63910)
#define COMMAND_RESULT				(0x63911)

/* Set format parameters. */
#define ISP_INPUT_FORMAT			(0X1e800)
#define ISP_OUTPUT_FORMAT			(0x1e802)
#define ISP_INPUT_MODE				(0X1e808)
#define SENSOR_OUTPUT_WIDTH			(0x1e80a)
#define SENSOR_OUTPUT_HEIGHT			(0x1e80c)
#define IDI_CONTROL				(0X1e80e)
#define ISP_INPUT_WIDTH				(0x1e810)
#define ISP_INPUT_HEIGHT			(0x1e812)
#define ISP_INPUT_H_START			(0x1e814)
#define ISP_INPUT_V_START			(0x1e816)
#define ISP_OUTPUT_WIDTH			(0x1e820)
#define ISP_OUTPUT_HEIGHT			(0x1e822)

#define ISP_FUNCTION_CTRL			(0x1e830)
#define RAW_YUV_DCW				(0x1e832)
#define MAC_MEMORY_WIDTH			(0x1e880)
#define ISP_SCALE_DOWN_H			(0x1e834)
#define ISP_SCALE_DOWN_V			(0x1e836)
#define ISP_SCALE_UP_H				(0x1e838)
#define ISP_SCALE_UP_V				(0x1e83a)

#define ISP_YUV_CROP_WIDTH			(0x1e83c)
#define ISP_YUV_CROP_HEIGHT			(0x1e83e)
#define ISP_YUV_CROP_H_START			(0x1e840)
#define ISP_YUV_CROP_V_START			(0x1e842)


#define ISP_EXPOSURE_RATIO			(0x1e860)
#define ISP_MAX_EXPOSURE			(0x1e862)
#define ISP_MIN_EXPOSURE			(0x1e864)
#define ISP_MAX_GAIN				(0x1e866)
#define ISP_MIN_GAIN				(0x1e868)
#define ISP_VTS					(0x1e872)
#define ISP_GAIN_MAX_H				(0x1c150)
#define ISP_GAIN_MAX_L				(0x1c151)
#define ISP_GAIN_MIN_H				(0x1c154)
#define ISP_GAIN_MIN_L				(0x1c155)

/* Capture parameters. */
#define ISP_SNAP_BANDING_50HZ			(0x1e874)
#define ISP_SNAP_BANDING_60HZ			(0x1e876)
#define ISP_PRE_BANDING_50HZ			(0x1c182)
#define ISP_PRE_BANDING_60HZ			(0x1c180)


#define ISP_BRACKET_RATIO1			(0x1e878)
#define ISP_BRACKET_RATIO2			(0x1e87a)

#define MAC_OUTPUT_ADDR_1			(0x1e890)//#define ISP_BASE_ADDR_LEFT			(0x1e94c)
#define MAC_OUTPUT_ADDR_2			(0x1e894)
#define MAC_OUTPUT_ADDR_3			(0x1e898)
#define MAC_INPUT_ADDR				(0x1e8b0)

#define ISP_MAC_UV_WIDTH_S			(0x1e882)
#define ISP_BASE_ADDR_NV12_UV_L			(0x1e894)

/* ISP registers */
/*ISP work mode; Combine Mode (HDR)*/
#define REG_ISP_TOP4				(0x65004)
#define REG_ISP_HVFLIP				(0x65006)
#define REG_ISP_SOFT_STANDBY		(0x60100)
#define REG_ISP_SOFT_RST			(0x60103)
#define REG_ISP_MCU_RST 			(0x63042)
#define REG_ISP_CORE_CTRL			(0x63023)

#define REG_ISP_MIPI0_LANE			(0x63c12)
#define REG_ISP_R_CTRL				(0x63108)

#define REG_ISP_GENERAL_PURPOSE_REG1		(0x63910)
#define REG_ISP_GENERAL_PURPOSE_REG2		(0x63912)
#define REG_ISP_GENERAL_PURPOSE_REG3		(0x63913)
#define REG_ISP_GENERAL_PURPOSE_REG7		(0x63917)

#define REG_ISP_INT_STAT			(0x6392b)
#define REG_ISP_INT_EN				(0x63927)

#define REG_ISP_MAC_INT_STAT_H		(0x63b32)
#define REG_ISP_MAC_INT_STAT_L		(0x63b33)
#define REG_ISP_MAC_INT_EN_H		(0x63b53)
#define REG_ISP_MAC_INT_EN_L		(0x63b54)
#define REG_ISP_FORMAT_CTRL			(0x63b34)
#define REG_ISP_SWITCH_CTRL			(0x63b35)

#define REG_BASE_ADDR0				(0x63b00)
#define REG_BASE_ADDR1				(0x63b04)
#define REG_BASE_ADDR2				(0x63b08)
#define REG_BASE_ADDR3				(0x63b0c)
#define REG_BASE_ADDR4				(0x63b10)
#define REG_BASE_ADDR5				(0x63b14)
#define REG_BASE_ADDR6				(0x63b18)
#define REG_BASE_ADDR7				(0x63b1c)
#define REG_BASE_ADDR8				(0x63b20)
#define REG_BASE_ADDR9				(0x63b24)
#define REG_BASE_ADDR10				(0x63b28)
#define REG_BASE_ADDR11				(0x63b2c)

#define REG_BASE_ADDR_READY			(0x63b30)
#define REG_BASE_WORKING_ADDR		(0x63b31)

#define REG_ISP_BANDFILTER_FLAG 		(0x1c13f)
#define REG_ISP_BANDFILTER_EN			(0x1c140)
#define REG_ISP_BANDFILTER_SHORT_EN		(0x1c141)

#define REG_ISP_SENSOR_VTS_ADDRESS1		(0x1c534)
#define REG_ISP_SENSOR_VTS_ADDRESS2		(0x1c536)
#define REG_ISP_DYN_FRAMERATE			(0x1c13a)
#define REG_ISP_DYN_FRM_INSERT_NUM		(0x1c17b)
#define REG_ISP_SENSOR_AEC_MASK_0_4		(0x1c560)
#define REG_ISP_SENSOR_AEC_MASK_0_5		(0x1c561)

#define REG_ISP_POLAR 					(0x63102)


//ISP focus regs
#define REG_ISP_FOCUS_ONOFF			(0x1cd0a)
#define REG_ISP_FOCUS_MODE			(0x1cd0b)
#define REG_ISP_FOCUS_RESULT		(0x1cdd4)
#define REG_ISP_FOCUS_STATE			(0x1cd0f)
#define REG_ISP_FOCUS_RECT_LEFT		(0x66104)
#define REG_ISP_FOCUS_RECT_TOP		(0x66106)
#define REG_ISP_FOCUS_RECT_WIDTH	(0x1ccfa)
#define REG_ISP_FOCUS_RECT_HEIGHT	(0x1ccfc)

//ISP meter regs
#define REG_ISP_ROI_LEFT			(0x66419)
#define REG_ISP_ROI_TOP				(0x6641d)
#define REG_ISP_ROI_RIGHT			(0x66421)
#define REG_ISP_ROI_BOTTOM			(0x66425)

#define REG_ISP_AECGC_STAT_WIN_LEFT		(0x66401)
#define REG_ISP_AECGC_STAT_WIN_TOP		(0x66403)
#define REG_ISP_AECGC_STAT_WIN_RIGHT	(0x66405)
#define REG_ISP_AECGC_STAT_WIN_BOTTOM	(0x66407)

#define REG_ISP_STRIDE_H			(0x63b38)
#define REG_ISP_STRIDE_L			(0x63b39)

#define REG_ISP_CROP_LEFT_H			(0x65f00)
#define REG_ISP_CROP_LEFT_L			(0x65f01)
#define REG_ISP_CROP_TOP_H			(0x65f02)
#define REG_ISP_CROP_TOP_L			(0x65f03)
#define REG_ISP_CROP_WIDTH_H		(0x65f04)
#define REG_ISP_CROP_WIDTH_L		(0x65f05)
#define REG_ISP_CROP_HEIGHT_H		(0x65f06)
#define REG_ISP_CROP_HEIGHT_L		(0x65f07)
#define REG_ISP_SCALEUP_X_H			(0x65026)
#define REG_ISP_SCALEUP_Y_L			(0x65027)
#define REG_ISP_CLK_USED_BY_MCU 	(0x63042)

#define REG_ISP_MAX_GAIN			(0x1c150)
#define REG_ISP_MIN_GAIN			(0x1c154)
#define REG_ISP_MAX_EXP				(0x1c158)
#define REG_ISP_MIN_EXP				(0x1c160)
#define REG_ISP_VTS					(0x1c176)

/* gain & expsure*/
#define REG_ISP_PRE_EXP1			(0x1c74c)//(0x1c16a)
#define REG_ISP_PRE_GAIN1			(0x1c754)//(0x1c170)
#define REG_ISP_PRE_EXP2			(0x1c168)
#define REG_ISP_PRE_GAIN2			(0x1c170)
#define REG_ISP_SNA_EXP				(0x1e910)
#define REG_ISP_SNA_GAIN			(0x1e912)
#define REG_ISP_AECGC_WRITEBACK_ENABLE	(0x1c139)
#define REG_ISP_AECGC_MANUAL_ENABLE	 (0x1c174)//manual


#define REG_ISP_MEAN				(0x1c758)
#define REG_ISP_TARGET_LOW			(0x1c146)
#define REG_ISP_AECGC_STABLE		(0x1c60c)

/*is gpio reg*/
#define REG_ISP_GPIO_BASE		(0x63d00)
#define REG_ISP_GPIO_NUM2		(0x63d10)
#define REG_ISP_GPIO_MODE0		(0x63d60)
#define REG_ISP_GPIO_MODE1		(0x63d61)
#define REG_ISP_GPIO_START0		(0x63d62)
#define REG_ISP_GPIO_START1		(0x63d63)
#define REG_ISP_GPIO_START2		(0x63d65)
#define REG_ISP_GPIO_START3		(0x63d67)
#define REG_ISP_GPIO_START_FW0		(0x63d69)
#define REG_ISP_GPIO_START_FW1		(0x63d6a)

#define REG_ISP_GPIO_FRE		(0x63d70)
#define REG_ISP_GPIO_EN0		(0x63d71)
#define REG_ISP_GPIO_EN1		(0x63d72)
#define REG_ISP_GPIO_VAL_INIT0		(0x63d73)
#define REG_ISP_GPIO_VAL_INIT1		(0x63d74)
#define REG_ISP_GPIO_REP_EN0		(0x63d79)
#define REG_ISP_GPIO_REP_EN1		(0x63d7a)
#define REG_ISP_GPIO_REP_CNT0		(0x63d7b)
#define REG_ISP_GPIO_REP_CNT1		(0x63d7c)

#define REG_ISP_FIRMWARE_VER		(0x1c59e)

/* I2C control. */
#define REG_SCCB_MAST1_SPEED			(0x63600)
#define REG_SCCB_MAST1_SLAVE_ID 		(0x63601)
#define REG_SCCB_MAST1_ADDRESS_H		(0x63602)
#define REG_SCCB_MAST1_ADDRESS_L		(0x63603)
#define REG_SCCB_MAST1_OUTPUT_DATA_H	(0x63604)
#define REG_SCCB_MAST1_OUTPUT_DATA_L	(0x63605)
#define REG_SCCB_MAST1_2BYTE_CONTROL	(0x63606)
#define REG_SCCB_MAST1_INPUT_DATA_H		(0x63607)
#define REG_SCCB_MAST1_INPUT_DATA_L		(0x63608)
#define REG_SCCB_MAST1_COMMAND			(0x63609)
#define REG_SCCB_MAST2_SPEED			(0x63700)
#define REG_SCCB_MAST2_SLAVE_ID			(0x63701)
#define REG_SCCB_MAST2_2BYTE_CONTROL	(0x63706)

#define REG_GPIO_R_REQ_CTRL_72			(0x63d72)
#define REG_GPIO_R_REQ_CTRL_74			(0x63d74)

/* Command set id. */
#define CMD_I2C_GRP_WR				(0x01)
#define CMD_SET_FORMAT				(0x02)
#define CMD_RESET_SENSOR			(0x03)
#define CMD_CAPTURE					(0x04)
#define CMD_OFFLINE_PROCESS			(0x05)
//#define CMD_HDR_PROCESS 		(0x06)
//#define CMD_ABORT				(0x07)
//#define CMD_FULL_HDR_PROCESS	(0x07)
#define CMD_ISP_REG_GRP_WRITE		(0x08)
#define CMD_AWB_MODE				(0x11)
#define CMD_ANTI_SHAKE_ENABLE		(0x12)
#define CMD_AUTOFOCUS_MODE			(0x13)
#define CMD_ZOOM_IN_MODE			(0x14)

/*Command set success flag. */
#define CMD_SET_SUCCESS				(0x01)
#define CMD_SET_AECGC				(0xf1)
#define CMD_SET_AEC					(0xf2)
#define CMD_SET_AGC					(0xf3)

/* Firmware download flag. */
#define CMD_FIRMWARE_DOWNLOAD		(0xff)

/* Command set mask. */
/* CMD_I2C_GRP_WR. */
/* REG1:I2C choice */
#define SELECT_I2C_NONE				(0x0 << 0)
#define SELECT_I2C_PRIMARY			(0x1 << 0)
#define SELECT_I2C_SECONDARY		(0x2 << 0)
#define SELECT_I2C_BOTH				(0x3 << 0)
#define SELECT_I2C_16BIT_DATA		(0x1 << 2)
#define SELECT_I2C_8BIT_DATA		(0x0 << 2)
#define SELECT_I2C_16BIT_ADDR		(0x1 << 3)
#define SELECT_I2C_8BIT_ADDR		(0x0 << 3)
#define SELECT_I2C_WRITE			(0x1 << 7)
#define SELECT_I2C_READ				(0x0 << 7)

/* REG_BASE_ADDR_READY. */
#define WRITE_ADDR0_READY			(1 << 0)
#define WRITE_ADDR1_READY			(1 << 1)
#define READ_ADDR0_READY			(1 << 2)
#define READ_ADDR1_READY			(1 << 3)

/* CMD_ZOOM_IN_MODE. */
/* REG1 bit[0] : 1 for high quality, 0 for save power mode */
#define HIGH_QUALITY_MODE			(0x1)
#define SAVE_POWER_MODE 			(0x0)

/* I2C control. */
#define MASK_16BIT_ADDR_ENABLE		(1 << 0)
#define MASK_16BIT_DATA_ENABLE		(1 << 1)
#define I2C_SPEED_100				(0x0C)
#define I2C_SPEED_200				(0x0A)

/* ISP interrupts mask. */
#define MASK_INT_AEC_DONE           (1 << 7)
#define MASK_INT_EOF                (1 << 5)
#define MASK_INT_SOF                (1 << 4)
#define MASK_INT_CMDSET				(1 << 3)
#define MASK_INT_MAC				(1 << 1)
#define MASK_INT_GPIO				(1 << 0)

/* ISP mac interrupts mask. */
/* Low mask. */
#define MASK_INT_WRITE_START0		(1 << 0)
#define MASK_INT_WRITE_DONE0		(1 << 1)
#define MASK_INT_DROP0				(1 << 2)
#define MASK_INT_OVERFLOW0			(1 << 3)
#define MASK_INT_WRITE_START1		(1 << 4)
#define MASK_INT_WRITE_DONE1		(1 << 5)
#define MASK_INT_DROP1				(1 << 6)
#define MASK_INT_OVERFLOW1			(1 << 7)
/* High mask. */
#define MASK_INT_FRAME_START		(1 << 8)
#define MASK_INT_DONE				(1 << 9)

/* Input format type. */
#define ISP_PROCESS					(1 << 6)
#define ISP_BYPASS					(0 << 6)
#define SENSOR_8LANES_MIPI			(6 << 3)
#define SENSOR_3D_MEMORY			(5 << 3)
#define SENSOR_MEMORY				(4 << 3)
#define SENSOR_3D_MIPI				(3 << 3)
#define SENSOR_SECONDARY_MIPI		(2 << 3)
#define SENSOR_PRIMARY_MIPI			(1 << 3)
#define SENSOR_DVP					(0 << 3)
#define IFORMAT_RGB888				(6 << 0)
#define IFORMAT_RGB565				(5 << 0)
#define IFORMAT_YUV422				(4 << 0)
#define IFORMAT_RAW14				(3 << 0)
#define IFORMAT_RAW12				(2 << 0)
#define IFORMAT_RAW10				(1 << 0)
#define IFORMAT_RAW8				(0 << 0)

/* Output format type. */
#define OFORMAT_RGB888				(7 << 4)
#define OFORMAT_RGB565				(6 << 4)
#define OFORMAT_NV12				(5 << 4)
#define OFORMAT_YUV422				(4 << 4)
#define OFORMAT_RAW14				(3 << 4)
#define OFORMAT_RAW12				(2 << 4)
#define OFORMAT_RAW10				(1 << 4)
#define OFORMAT_RAW8				(0 << 4)

/* offline output format type */
#define OFFLINE_OFORMAT_YUV420		(2 << 0)
#define OFFLINE_OFORMAT_NV12		(1 << 0)
#define OFFLINE_OFORMAT_YUV422		(0 << 0)

/* Reset type. */
/* REG_ISP_SOFT_STANDBY&REG_ISP_SOFT_RST&REG_ISP_MCU_RST. */
#define DO_SOFTWARE_STAND_BY		(0x01)
#define MCU_SOFT_RST				(0x01)
#define DO_SOFT_RST					(0x01)
#define RELEASE_SOFT_RST			(0x00)

/* ISP function. */
#define FUNCTION_RAW_DCW			(1 << 3)
#define FUNCTION_NO_SCALE			(0 << 1)
#define FUNCTION_SCALE_DOWN			(1 << 1)
#define FUNCTION_SCALE_UP			(2 << 1)
#define FUNCTION_YUV_CROP			(1 << 0)

/* Input select. */
#define INPUT_DVP					(0 << 8)
#define INPUT_RETIMING				(1 << 8)
#define INPUT_MAC					(2 << 8)
#define INPUT_MIPI0					(4 << 8)
#define INPUT_MIPI1					(8 << 8)

/* Re-timing module control signal. */
#define PACK64_ENABLE				(1 << 5)
#define VRTM_ENABLE					(1 << 0)

#endif /* __ASM_ARCH_REGS_ISP_LC1813_H */
