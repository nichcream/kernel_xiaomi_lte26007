#ifndef __LC1160H_PMIC_H__
#define __LC1160H_PMIC_H__

#include <linux/types.h>
#include <plat/comip-pmic.h>

/*-------------------------------------------------------------------------------
			LC1160 register data.
-------------------------------------------------------------------------------*/
/* DCDC Register */
#define LC1160_REG_VERSION					0x00
#define LC1160_REG_DC1OVS0					0x01
#define LC1160_REG_DC1OVS1					0x02
#define LC1160_REG_DC2OVS0          			0x03
#define LC1160_REG_DC2OVS1          			0x04
#define LC1160_REG_DC2OVS2          			0x05
#define LC1160_REG_DC2OVS3         			0x06
#define LC1160_REG_DC3OVS0					0x07
#define LC1160_REG_DC3OVS1					0x08
#define LC1160_REG_DC3OVS2					0x09
#define LC1160_REG_DC3OVS3					0x0a
#define LC1160_REG_DC4OVS0					0x0b
#define LC1160_REG_DC4OVS1					0x0c
#define LC1160_REG_DC5OVS0					0x0d
#define LC1160_REG_DC5OVS1					0x0e
#define LC1160_REG_DC6OVS0					0x0f
#define LC1160_REG_DC6OVS1					0x10
#define LC1160_REG_DC7OVS0					0x11
#define LC1160_REG_DC7OVS1					0x12
#define LC1160_REG_DC8OVS0					0x13
#define LC1160_REG_DC8OVS1					0x14
#define LC1160_REG_DC9OVS0					0x15
#define LC1160_REG_DC9OVS1					0x16
#define LC1160_REG_DCBOOST1CR			0x17
#define LC1160_REG_DC2DVSCR				0x18
#define LC1160_REG_DC3DVSCR				0x19
#define LC1160_REG_DC1CR					0x1a
#define LC1160_REG_DC2CR					0x1b
#define LC1160_REG_DC3CR					0x1c
#define LC1160_REG_DC4CR					0x1d
#define LC1160_REG_DC5CR					0x1e
#define LC1160_REG_DC6CR					0x1f
#define LC1160_REG_DC7CR					0x20
#define LC1160_REG_DC8CR					0x21
#define LC1160_REG_DC8CR2					0x22
#define LC1160_REG_DC9CR					0x23
#define LC1160_REG_DC9CR2					0x24
#define LC1160_REG_DC2489CR				0x25
/* LDOs for analog Register */
#define LC1160_REG_DCLDOCONEN         		0x29
#define LC1160_REG_LDOAONEN				0x2a
#define LC1160_REG_LDOA1CR					0x2b
#define LC1160_REG_LDOA2CR					0x2c
#define LC1160_REG_LDOA2CR2		    		0x2d
#define LC1160_REG_LDOA3CR					0x2e
#define LC1160_REG_LDOA3CR2				0x2f
#define LC1160_REG_LDOA4CR					0x30
#define LC1160_REG_LDOA5CR					0x31
#define LC1160_REG_LDOA6CR					0x32
#define LC1160_REG_LDOA7CR		    			0x33
#define LC1160_REG_LDOA8CR					0x34
#define LC1160_REG_LDOA9CR					0x35
#define LC1160_REG_LDOA10CR				0x36
#define LC1160_REG_LDOA11CR				0x37
#define LC1160_REG_LDOA12CR				0x38
#define LC1160_REG_LDOA13CR		    		0x39
#define LC1160_REG_LDOA14CR				0x3a
#define LC1160_REG_LDOA15CR				0x3b
/* LDOs for digital Register */
#define LC1160_REG_LDOD1CR  				0x3e
#define LC1160_REG_LDOD2CR  				0x3f
#define LC1160_REG_LDOD3CR  				0x40
#define LC1160_REG_LDOD4CR  				0x41
#define LC1160_REG_LDOD5CR  				0x42
#define LC1160_REG_LDOD6CR 				0x43
#define LC1160_REG_LDOD7CR  				0x44
#define LC1160_REG_LDOD8CR  				0x45
#define LC1160_REG_LDOD9CR  				0x46
#define LC1160_REG_LDOD10CR  				0x47
#define LC1160_REG_LDOD11CR 				0x48
/* SPI Register */
#define LC1160_REG_SPICR					0x49
/* MBG Register */
#define LC1160_REG_MBG						0x4a
/* Sleep model Register */
#define LC1160_REG_LDOA_SLEEP_MODE1   	0x4c
#define LC1160_REG_LDOA_SLEEP_MODE2		0x4d
#define LC1160_REG_LDOA_SLEEP_MODE3		0x4e
#define LC1160_REG_LDOD_SLEEP_MODE1		0x4f
#define LC1160_REG_LDOD_SLEEP_MODE2		0x50
/* Led current sink driverl Register */
#define LC1160_REG_SINKCR          				0x51
#define LC1160_REG_LEDIOUT         				0x52
#define LC1160_REG_VIBI_FLASHIOUT         		0x53
#define LC1160_REG_INDDIM         				0x54
/* Battery charger Register */
#define LC1160_REG_CHGFILTER         			0x55
#define LC1160_REG_CHGCR         				0x56
#define LC1160_REG_CHGVSET         			0x57
#define LC1160_REG_CHGCSET       				0x58
#define LC1160_REG_CHGTSET            			0x59
#define LC1160_REG_CHGSTATUS  				0x5a
/* Power status and control Register */
#define LC1160_REG_PCST     					0x5b
#define LC1160_REG_STARTUP_STATUS		0x5c
#define LC1160_REG_SHDN_STATUS     			0x5d
#define LC1160_REG_SHDN_EN     				0x5e
#define LC1160_REG_BATUVCR     				0x5f
/* RTC Register */
#define LC1160_REG_RTC_SEC         			0x60
#define LC1160_REG_RTC_MIN         			0x61
#define LC1160_REG_RTC_HOUR        			0x62
#define LC1160_REG_RTC_DAY         			0x63
#define LC1160_REG_RTC_WEEK        			0x64
#define LC1160_REG_RTC_MONTH       			0x65
#define LC1160_REG_RTC_YEAR        			0x66
#define LC1160_REG_RTC_TIME_UPDATE 		0x67
#define LC1160_REG_RTC_32K_ADJL    			0x68
#define LC1160_REG_RTC_32K_ADJH    			0x69
#define LC1160_REG_RTC_SEC_CYCL    		0x6a
#define LC1160_REG_RTC_SEC_CYCH    		0x6b
#define LC1160_REG_RTC_CALIB_UPDATE		0x6c
#define LC1160_REG_RTC_AL1_SEC     			0x6d
#define LC1160_REG_RTC_AL1_MIN     			0x6e
#define LC1160_REG_RTC_AL2_SEC     			0x6f
#define LC1160_REG_RTC_AL2_MIN     			0x70
#define LC1160_REG_RTC_AL2_HOUR    			0x71
#define LC1160_REG_RTC_AL2_DAY     			0x72
#define LC1160_REG_RTC_AL2_MONTH   		0x73
#define LC1160_REG_RTC_AL2_YEAR    			0x74
#define LC1160_REG_RTC_AL_UPDATE   		0x75
#define LC1160_REG_RTC_INT_EN      			0x76
#define LC1160_REG_RTC_INT_STATUS  		0x77
#define LC1160_REG_RTC_INT_RAW     			0x78
#define LC1160_REG_RTC_CTRL        			0x79
#define LC1160_REG_RTC_BK          				0x7a
#define LC1160_REG_RTC_INT_MASK    			0x7b
#define LC1160_REG_RTC_AL1_HOUR    			0x7c
#define LC1160_REG_RTC_AL1_DAY    	    		0x7d
#define LC1160_REG_RTC_AL1_MONTH    		0x7e
#define LC1160_REG_RTC_AL1_YEAR    			0x7f
#define LC1160_REG_RTC_DATA1    				0x80
#define LC1160_REG_RTC_DATA2				0x81
#define LC1160_REG_RTC_DATA3    	    			0x82
#define LC1160_REG_RTC_DATA4		    		0x83
/* ADC Register */
#define LC1160_REG_ADCCR1        				0x87
#define LC1160_REG_ADCCR2           			0x88
#define LC1160_REG_ADCDAT0          			0x89
#define LC1160_REG_ADCDAT1          			0x8a
/* ALLCR Register */
#define LC1160_REG_I2C_CLK_TSCR           		0x8b
/* Interrupt Register */
#define LC1160_REG_IRQST_FINAL          		0x8c
#define LC1160_REG_IRQST1          				0x8d
#define LC1160_REG_IRQST2          				0x8e
#define LC1160_REG_IRQEN1          				0x8f
#define LC1160_REG_IRQEN2          				0x90
#define LC1160_REG_IRQMSK1         			0x91
#define LC1160_REG_IRQMSK2         			0x92
/* PWM Register. */
#define LC1160_REG_PWM_CTL          			0x95
#define LC1160_REG_PWM0_P          				0x96
#define LC1160_REG_PWM1_P       				0x97
#define LC1160_REG_PWM0_OCPY        			0x98
#define LC1160_REG_PWM1_OCPY        			0x99
/* OTP Register. */
#define LC1160_REG_OTPSEL0          			0x9a
#define LC1160_REG_OTPSEL1          			0x9b
#define LC1160_REG_OTPMODE       			0x9c
#define LC1160_REG_OTPCMD	        			0x9d
#define LC1160_REG_TRIM0        				0x9e
#define LC1160_REG_TRIM1        				0x9f
#define LC1160_REG_TRIM2        				0xa0
#define LC1160_REG_TRIM3        				0xa1
#define LC1160_REG_TRIM4        				0xa2
#define LC1160_REG_TRIM5        				0xa3
#define LC1160_REG_TRIM6        				0xa4
#define LC1160_REG_TRIM7        				0xa5
/* USB VBUS Register */
#define LC1160_REG_QPVBUS           			0xa6
/* Jack and Hookswitch */
#define LC1160_REG_JACKCR1           			0xa7
#define LC1160_REG_HS1CR           				0xa8
#define LC1160_REG_HS2CR           				0xa9
#define LC1160_REG_HS3CR           				0xaa
#define LC1160_REG_JKHSIR           				0xab

/* Codec Register. */
#define LC1160_REG_CODEC_R1_BAK			0xad

/* RegBak Control Register. */
#define LC1160_REG_REGBAK_CONTROL			0xfc

/*-------------------------------------------------------------------------------
			LC1160 register bitmask.
-------------------------------------------------------------------------------*/

/* DC1OVS0: DCDC1 Output Voltage Setting Register 0(0x01) */
#define LC1160_REG_BITMASK_DC1OVS0_DC1SLP			0x80
#define LC1160_REG_BITMASK_DC1OVS0_DC1EN     			0x40
#define LC1160_REG_BITMASK_DC1OVS0_DC1VOUT0      		0x3f
/* DC1OVS1: DCDC1 Output Voltage Setting Register 1(0x02) */
#define LC1160_REG_BITMASK_DC1OVS1_DC1SLP			0x80
#define LC1160_REG_BITMASK_DC1OVS1_DC1EN     			0x40
#define LC1160_REG_BITMASK_DC1OVS1_DC1VOUT1     		0x3f
/* DC2OVS0: DCDC2 Output Voltage Setting Register 1(0x03) */
#define LC1160_REG_BITMASK_DC2OVS0_DC2SLP			0x80
#define LC1160_REG_BITMASK_DC2OVS0_DC2EN     			0x40
#define LC1160_REG_BITMASK_DC2OVS0_DC2VOUT0     		0x3f
/* DC2OVS1: DCDC2 Output Voltage Setting Register 1(0x04) */
#define LC1160_REG_BITMASK_DC2OVS1_DC2SLP			0x80
#define LC1160_REG_BITMASK_DC2OVS1_DC2EN     			0x40
#define LC1160_REG_BITMASK_DC2OVS1_DC2VOUT1     		0x3f
/* DC2OVS2: DCDC2 Output Voltage Setting Register 2(0x05) */
#define LC1160_REG_BITMASK_DC2OVS2_DC2VOUT2     		0x3f
/* DC2OVS3: DCDC2 Output Voltage Setting Register 3(0x06) */
#define LC1160_REG_BITMASK_DC2OVS2_DC2VOUT3     		0x3f
/* DC3OVS0: DCDC3 Output Voltage Setting Register 0(0x07) */
#define LC1160_REG_BITMASK_DC3OVS0_DC3SLP			0x80
#define LC1160_REG_BITMASK_DC3OVS0_DC3EN     			0x40
#define LC1160_REG_BITMASK_DC3OVS0_DC3VOUT0     		0x3f
/* DC3OVS1: DCDC3 Output Voltage Setting Register 1(0x08) */
#define LC1160_REG_BITMASK_DC3OVS1_DC3SLP			0x80
#define LC1160_REG_BITMASK_DC3OVS1_DC3EN     			0x40
#define LC1160_REG_BITMASK_DC3OVS1_DC3VOUT1     		0x3f
/* DC3OVS2: DCDC3 Output Voltage Setting Register 2(0x09) */
#define LC1160_REG_BITMASK_DC3OVS2_DC3VOUT2     		0x3f
/* DC3OVS3: DCDC3 Output Voltage Setting Register 3(0x0a) */
#define LC1160_REG_BITMASK_DC3OVS3_DC3VOUT3     		0x3f
/* DC4OVS0: DCDC4 Output Voltage Setting Register 0(0x0b) */
#define LC1160_REG_BITMASK_DC4OVS0_DC4SLP			0x80
#define LC1160_REG_BITMASK_DC4OVS0_DC4EN     			0x40
#define LC1160_REG_BITMASK_DC4OVS0_DC4VOUT0     		0x3f
/* DC4OVS1: DCDC4 Output Voltage Setting Register 1(0x0c) */
#define LC1160_REG_BITMASK_DC4OVS1_DC4SLP			0x80
#define LC1160_REG_BITMASK_DC4OVS1_DC4EN     			0x40
#define LC1160_REG_BITMASK_DC4OVS1_DC4VOUT1     		0x3f
/* DC5OVS0: DCDC5 Output Voltage Setting Register 0(0x0d) */
#define LC1160_REG_BITMASK_DC5OVS0_DC5SLP			0x80
#define LC1160_REG_BITMASK_DC5OVS0_DC5EN     			0x40
#define LC1160_REG_BITMASK_DC5OVS0_DC5VOUT0     		0x1f
/* DC5OVS1: DCDC5 Output Voltage Setting Register 1(0x0e) */
#define LC1160_REG_BITMASK_DC5OVS1_DC5SLP			0x80
#define LC1160_REG_BITMASK_DC5OVS1_DC5EN     			0x40
#define LC1160_REG_BITMASK_DC5OVS1_DC5VOUT1     		0x1f
/* DC6OVS0: DCDC6 Output Voltage Setting Register 0(0x0f) */
#define LC1160_REG_BITMASK_DC6OVS0_DC6SLP			0x80
#define LC1160_REG_BITMASK_DC6OVS0_DC6EN     			0x40
#define LC1160_REG_BITMASK_DC6OVS0_DC6VOUT0     		0x1f
/* DC6OVS1: DCDC6 Output Voltage Setting Register 1(0x10) */
#define LC1160_REG_BITMASK_DC6OVS1_DC6SLP			0x80
#define LC1160_REG_BITMASK_DC6OVS1_DC6EN     			0x40
#define LC1160_REG_BITMASK_DC6OVS1_DC6VOUT1     		0x1f
/* DC7OVS0: DCDC7 Output Voltage Setting Register 0(0x11) */
#define LC1160_REG_BITMASK_DC7OVS0_DC7SLP			0x80
#define LC1160_REG_BITMASK_DC7OVS0_DC7EN     			0x40
#define LC1160_REG_BITMASK_DC7OVS0_DC7VOUT0     		0x1f
/* DC7OVS1: DCDC7 Output Voltage Setting Register 1(0x12) */
#define LC1160_REG_BITMASK_DC7OVS1_DC7SLP			0x80
#define LC1160_REG_BITMASK_DC7OVS1_DC7EN     			0x40
#define LC1160_REG_BITMASK_DC7OVS1_DC7VOUT1     		0x1f
/* DC8OVS0: DCDC8 Output Voltage Setting Register 0(0x13) */
#define LC1160_REG_BITMASK_DC8OVS0_DC8SLP			0x80
#define LC1160_REG_BITMASK_DC8OVS0_DC8EN     			0x40
#define LC1160_REG_BITMASK_DC8OVS0_DC8VOUT0     		0x3f
/* DC8OVS1: DCDC8 Output Voltage Setting Register 1(0x14) */
#define LC1160_REG_BITMASK_DC8OVS1_DC8SLP			0x80
#define LC1160_REG_BITMASK_DC8OVS1_DC8EN     			0x40
#define LC1160_REG_BITMASK_DC8OVS1_DC8VOUT1     		0x3f
/* DC9OVS0: DCDC9 Output Voltage Setting Register 0(0x15) */
#define LC1160_REG_BITMASK_DC9OVS0_DC9EN     			0x40
#define LC1160_REG_BITMASK_DC9OVS0_DC9VOUT0     		0x3f
/* DC9OVS1: DCDC9 Output Voltage Setting Register 1(0x16) */
#define LC1160_REG_BITMASK_DC9OVS1_DC9SLP			0x80
#define LC1160_REG_BITMASK_DC9OVS1_DC9EN     			0x40
#define LC1160_REG_BITMASK_DC9OVS1_DC9VOUT1     		0x3f
/* DC2DVSCR: DCDC2 DVS Control Register(0x18) */
#define LC1160_REG_BITMASK_DC2_DVS_UP_EN			0x20
#define LC1160_REG_BITMASK_DC2_DVS_STP_EN     			0x10
#define LC1160_REG_BITMASK_DC2_DVS_DOWN     			0x0c
#define LC1160_REG_BITMASK_DC2_DVS_SEL     			0x03
/* DC3DVSCR: DCDC3 DVS Control Register(0x19) */
#define LC1160_REG_BITMASK_DC3_DVS_UP_EN			0x20
#define LC1160_REG_BITMASK_DC3_DVS_STP_EN     			0x10
#define LC1160_REG_BITMASK_DC3_DVS_DOWN     			0x0c
#define LC1160_REG_BITMASK_DC3_DVS_SEL     			0x03
/* DC1CR: DCDC1 Control Register(0x1a) */
#define LC1160_REG_BITMASK_DC1COMP					0xc0
#define LC1160_REG_BITMASK_DC1_PWM_FORCE     			0x20
#define LC1160_REG_BITMASK_DC1_FST_DISC     			0x10
#define LC1160_REG_BITMASK_DC1_OVP_VTH     			0x08
#define LC1160_REG_BITMASK_DC1_PFM_ITH     				0x04
#define LC1160_REG_BITMASK_DC1RCP     					0x03
/* DC2CR: DCDC2 Control Register(0x1b) */
#define LC1160_REG_BITMASK_DC2COMP					0xc0
#define LC1160_REG_BITMASK_DC2_PWM_FORCE     			0x20
#define LC1160_REG_BITMASK_DC2_FST_DISC     			0x10
#define LC1160_REG_BITMASK_DC2_OVP_VTH     			0x08
#define LC1160_REG_BITMASK_DC2_PFM_ITH     				0x04
#define LC1160_REG_BITMASK_DC2RCP     					0x03
/* DC3CR: DCDC3 Control Register(0x1c) */
#define LC1160_REG_BITMASK_DC3COMP					0xc0
#define LC1160_REG_BITMASK_DC3_PWM_FORCE     			0x20
#define LC1160_REG_BITMASK_DC3_FST_DISC     			0x10
#define LC1160_REG_BITMASK_DC3_OVP_VTH     			0x08
#define LC1160_REG_BITMASK_DC3_PFM_ITH     				0x04
#define LC1160_REG_BITMASK_DC3RCP     					0x03
/* DC4CR: DCDC4 Control Register(0x1d) */
#define LC1160_REG_BITMASK_DC4COMP					0xc0
#define LC1160_REG_BITMASK_DC4_PWM_FORCE     			0x20
#define LC1160_REG_BITMASK_DC4_FST_DISC     			0x10
#define LC1160_REG_BITMASK_DC4_OVP_VTH     			0x08
#define LC1160_REG_BITMASK_DC4_PFM_ITH     				0x04
#define LC1160_REG_BITMASK_DC4RCP     					0x03
/* DC5CR: DCDC5 Control Register(0x1e) */
#define LC1160_REG_BITMASK_DC5_PWM_FORCE     			0x20
#define LC1160_REG_BITMASK_DC5_FST_DISC     			0x10
#define LC1160_REG_BITMASK_DC5_OVP_VTH     			0x08
#define LC1160_REG_BITMASK_DC5_PFM_ITH     				0x04
#define LC1160_REG_BITMASK_DC5RCP     					0x03
/* DC6CR: DCDC6 Control Register(0x1f) */
#define LC1160_REG_BITMASK_DC6_PWM_FORCE     			0x20
#define LC1160_REG_BITMASK_DC6_FST_DISC     			0x10
#define LC1160_REG_BITMASK_DC6_OVP_VTH     			0x08
#define LC1160_REG_BITMASK_DC6_PFM_ITH     				0x04
#define LC1160_REG_BITMASK_DC6RCP     					0x03
/* DC7CR: DCDC7 Control Register(0x20) */
#define LC1160_REG_BITMASK_DC7_PWM_FORCE     			0x20
#define LC1160_REG_BITMASK_DC7_FST_DISC     			0x10
#define LC1160_REG_BITMASK_DC7_OVP_VTH     			0x08
#define LC1160_REG_BITMASK_DC7_PFM_ITH     				0x04
#define LC1160_REG_BITMASK_DC7RCP     					0x03
/* DC8CR: DCDC8 Control Register(0x21) */
#define LC1160_REG_BITMASK_D8COMP     					0xf0
#define LC1160_REG_BITMASK_DC8_PWM_FORCE     			0x08
#define LC1160_REG_BITMASK_DC8_FST_DISC     			0x04
#define LC1160_REG_BITMASK_DC8_OVP_VTH     			0x02
#define LC1160_REG_BITMASK_DC8_PFM_ITH     				0x01
/* DC8CR2: DCDC8 Control Register(0x22) */
#define LC1160_REG_BITMASK_DC8_SOFTEN     				0x80
#define LC1160_REG_BITMASK_DC8RCP     					0x70
#define LC1160_REG_BITMASK_DC8SLOPE     				0x0d
#define LC1160_REG_BITMASK_DC8BYPASS     				0x02
#define LC1160_REG_BITMASK_DC8_DMAX_EN     			0x01
/* DC9CR: DCDC9 Control Register(0x23) */
#define LC1160_REG_BITMASK_D9COMP     					0xf0
#define LC1160_REG_BITMASK_DC9_PWM_FORCE     			0x08
#define LC1160_REG_BITMASK_DC9_FST_DISC     			0x04
#define LC1160_REG_BITMASK_DC9_OVP_VTH     			0x02
#define LC1160_REG_BITMASK_DC9_PFM_ITH     				0x01
/* DC9CR2: DCDC9 Control Register(0x24) */
#define LC1160_REG_BITMASK_DC9_SOFTEN     				0xf0
#define LC1160_REG_BITMASK_DC9RCP     					0x08
#define LC1160_REG_BITMASK_DC9SLOPE     				0x04
#define LC1160_REG_BITMASK_DC9BYPASS     				0x02
#define LC1160_REG_BITMASK_DC9_DMAX_EN     			0x01
/* DC2489CR: DCDC2489 Control Register2(0x25) */
#define LC1160_REG_BITMASK_DC9_TEST_EN     			0x20
#define LC1160_REG_BITMASK_DC8_TEST_EN     			0x10
#define LC1160_REG_BITMASK_DC4SLOPE     				0x0d
#define LC1160_REG_BITMASK_DC2SLOPE     				0x03
/* DCLDOCONEN: DC5/9 LDOA13/14 Control Select Register2(0x29) */
#define LC1160_REG_BITMASK_LDOA14CONEN     			0x40
#define LC1160_REG_BITMASK_LDOA13CONEN     			0x20
#define LC1160_REG_BITMASK_LDOA8CONEN     			0x10
#define LC1160_REG_BITMASK_LDOA4CONEN     			0x08
#define LC1160_REG_BITMASK_LDOA1CONEN     			0x04
#define LC1160_REG_BITMASK_DC9CONEN     				0x02
#define LC1160_REG_BITMASK_DC5CONEN     				0x01
/* LDOAONEN: LDOA External Pin Enable Register(0x2a) */
#define LC1160_REG_BITMASK_LDOA6_SLP_SEL     			0x40
#define LC1160_REG_BITMASK_LDOA5_SLP_SEL     			0x20
#define LC1160_REG_BITMASK_LDOA3_SLP_SEL     			0x10
#define LC1160_REG_BITMASK_LDOA2_SLP_SEL     			0x08
/* LDOA1CR: LDOA1 Control Register(0x2b) */
#define LC1160_REG_BITMASK_LDOA1EN     					0x80
#define LC1160_REG_BITMASK_LDOA1_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA1SLP     				0x20
#define LC1160_REG_BITMASK_LDOA1OV     					0x1f
/* LDOA2CR: LDOA2Control Register(0x2c) */
#define LC1160_REG_BITMASK_LDOA2EN     					0x80
#define LC1160_REG_BITMASK_LDOA2_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA2SLP     				0x20
#define LC1160_REG_BITMASK_LDOA2OV     					0x1f
/* LDOA2CR2: LDOA2Control Register2(0x2d) */
#define LC1160_REG_BITMASK_LDOA2OV_SLP     			0x1f
/* LDOA3CR: LDOA3 Control Register(0x2e) */
#define LC1160_REG_BITMASK_LDOA3EN     					0x80
#define LC1160_REG_BITMASK_LDOA3_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA3SLP     				0x20
#define LC1160_REG_BITMASK_LDOA3OV     					0x1f
/* LDOA3CR2: LDOA3 Contrl Register2(0x2f) */
#define LC1160_REG_BITMASK_LDOA3OV     					0x1f
/* LDOA4CR: LDOA4 Control Register(0x30) */
#define LC1160_REG_BITMASK_LDOA4EN     					0x80
#define LC1160_REG_BITMASK_LDOA4_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA4SLP     				0x20
#define LC1160_REG_BITMASK_LDOA4OV     					0x1f
/* LDOA5CR: LDOA5 Control Register(0x31) */
#define LC1160_REG_BITMASK_LDOA5EN     					0x80
#define LC1160_REG_BITMASK_LDOA5_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA5SLP     				0x20
#define LC1160_REG_BITMASK_LDOA5OV     					0x1f
/* LDOA6CR: LDOA6 Control Register(0x32) */
#define LC1160_REG_BITMASK_LDOA6EN     					0x80
#define LC1160_REG_BITMASK_LDOA6_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA6SLP     				0x20
#define LC1160_REG_BITMASK_LDOA6OV     					0x1f
/* LDOA7CR: LDOA7 Control Register(0x33) */
#define LC1160_REG_BITMASK_LDOA7EN     					0x80
#define LC1160_REG_BITMASK_LDOA7_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA7SLP     				0x20
#define LC1160_REG_BITMASK_LDOA7OV     					0x1f
/* LDOA8CR: LDOA8 Control Register(0x34) */
#define LC1160_REG_BITMASK_LDOA8EN     					0x80
#define LC1160_REG_BITMASK_LDOA8_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA8SLP     				0x20
#define LC1160_REG_BITMASK_LDOA8OV     					0x1f
/* LDOA9CR: LDOA9 Control Register(0x35) */
#define LC1160_REG_BITMASK_LDOA9EN     					0x80
#define LC1160_REG_BITMASK_LDOA9_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOA9SLP     				0x20
#define LC1160_REG_BITMASK_LDOA9OV     					0x1f
/* LDOA10CR: LDOA10 Control Register(0x36) */
#define LC1160_REG_BITMASK_LDOA10EN     				0x80
#define LC1160_REG_BITMASK_LDOA10_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOA10SLP     				0x20
#define LC1160_REG_BITMASK_LDOA10OV     				0x1f
/* LDOA11CR: LDOA11 Control Register(0x37) */
#define LC1160_REG_BITMASK_LDOA11EN     				0x80
#define LC1160_REG_BITMASK_LDOA11_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOA11SLP     				0x20
#define LC1160_REG_BITMASK_LDOA11OV     				0x1f
/* LDOA12CR: LDOA12 Control Register(0x38) */
#define LC1160_REG_BITMASK_LDOA12EN     				0x80
#define LC1160_REG_BITMASK_LDOA12_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOA12SLP     				0x20
#define LC1160_REG_BITMASK_LDOA12OV     				0x1f
/* LDOA13CR: LDOA13 Control Register(0x39) */
#define LC1160_REG_BITMASK_LDOA13EN     				0x80
#define LC1160_REG_BITMASK_LDOA13_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOA13SLP     				0x20
#define LC1160_REG_BITMASK_LDOA13OV     				0x1f
/* LDOA14CR: LDOA14 Control Register(0x3a) */
#define LC1160_REG_BITMASK_LDOA14EN     				0x80
#define LC1160_REG_BITMASK_LDOA14_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOA14SLP     				0x20
#define LC1160_REG_BITMASK_LDOA14OV     				0x1f
/* LDOA15CR: LDOA15 Control Register(0x3b) */
#define LC1160_REG_BITMASK_LDOA15EN     				0x80
#define LC1160_REG_BITMASK_LDOA15_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOA15SLP     				0x20
#define LC1160_REG_BITMASK_LDOA15OV     				0x1f
/* LDOD1CR: LDOD1 Control Register(0x3e) */
#define LC1160_REG_BITMASK_LDOD1EN     				0x80
#define LC1160_REG_BITMASK_LDOD1_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD1SLP     				0x20
#define LC1160_REG_BITMASK_LDOD1OV     				0x1f
/* LDOD2CR: LDOD2 Control Register(0x3f) */
#define LC1160_REG_BITMASK_LDOD2EN     				0x80
#define LC1160_REG_BITMASK_LDOD2_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD2SLP     				0x20
#define LC1160_REG_BITMASK_LDOD2OV     				0x1f
/* LDOD3CR: LDOD3 Control Register(0x40) */
#define LC1160_REG_BITMASK_LDOD3EN     				0x80
#define LC1160_REG_BITMASK_LDOD3_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD3SLP     				0x20
#define LC1160_REG_BITMASK_LDOD3OV     				0x1f
/* LDOD4CR: LDOD4 Control Register(0x41) */
#define LC1160_REG_BITMASK_LDOD4EN     				0x80
#define LC1160_REG_BITMASK_LDOD4_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD4SLP     				0x20
#define LC1160_REG_BITMASK_LDOD4OV     				0x1f
/* LDOD5CR: LDOD5 Control Register(0x42) */
#define LC1160_REG_BITMASK_LDOD5EN     				0x80
#define LC1160_REG_BITMASK_LDOD5_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD5SLP     				0x20
#define LC1160_REG_BITMASK_LDOD5OV     				0x1f
/* LDOD6CR: LDOD6 Control Register(0x43) */
#define LC1160_REG_BITMASK_LDOD6EN     				0x80
#define LC1160_REG_BITMASK_LDOD6_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD6SLP     				0x20
#define LC1160_REG_BITMASK_LDOD6OV     				0x1f
/* LDOD7CR: LDOD7 Control Register(0x44) */
#define LC1160_REG_BITMASK_LDOD7EN     				0x80
#define LC1160_REG_BITMASK_LDOD7_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD7SLP     				0x20
#define LC1160_REG_BITMASK_LDOD7OV     				0x1f
/* LDOD8CR: LDOD8 Control Register(0x45) */
#define LC1160_REG_BITMASK_LDOD8EN     				0x80
#define LC1160_REG_BITMASK_LDOD8_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD8SLP     				0x20
#define LC1160_REG_BITMASK_LDOD8OV     				0x1f
/* LDOD9CR: LDOD9 Control Register(0x46) */
#define LC1160_REG_BITMASK_LDOD9EN     				0x80
#define LC1160_REG_BITMASK_LDOD9_FST_DISC     			0x40
#define LC1160_REG_BITMASK_LDOD9SLP     				0x20
#define LC1160_REG_BITMASK_LDOD9OV     				0x1f
/* LDOD10CR: LDOD10 Control Register(0x47) */
#define LC1160_REG_BITMASK_LDOD10EN     				0x80
#define LC1160_REG_BITMASK_LDOD10_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOD10SLP     				0x20
#define LC1160_REG_BITMASK_LDOD10OV     				0x1f
/* LDOD11CR: LDOD11 Control Register(0x48) */
#define LC1160_REG_BITMASK_LDOD11EN     				0x80
#define LC1160_REG_BITMASK_LDOD11_FST_DISC     		0x40
#define LC1160_REG_BITMASK_LDOD11SLP     				0x20
#define LC1160_REG_BITMASK_LDOD11OV     				0x1f
/* SPICR: SPI Control Register(0x49) */
#define LC1160_REG_BITMASK_SPI_IIC_EN     				0x01
/* LDOA_SLEEP_MODE1: LDOA2-6 Sleep Mode Setting Register(0x4c) */
#define LC1160_REG_BITMASK_LDOA6_ALLOW_IN_SLP     	0x20
#define LC1160_REG_BITMASK_LDOA5_ALLOW_IN_SLP     	0x10
#define LC1160_REG_BITMASK_LDOA3_ALLOW_IN_SLP     	0x02
#define LC1160_REG_BITMASK_LDOA2_ALLOW_IN_SLP     	0x01
/* LDOA_SLEEP_MODE2: LDOA1-8 Sleep Mode Setting Register(0x4d) */
#define LC1160_REG_BITMASK_LDOA8_ALLOW_IN_SLP     	0x80
#define LC1160_REG_BITMASK_LDOA7_ALLOW_IN_SLP     	0x40
#define LC1160_REG_BITMASK_LDOA4_ALLOW_IN_SLP     	0x02
#define LC1160_REG_BITMASK_LDOA1_ALLOW_IN_SLP     	0x01
/* LDOA_SLEEP_MODE3: LDOA9-15 Sleep Mode Setting Register(0x4e) */
#define LC1160_REG_BITMASK_LDOA15_ALLOW_IN_SLP     	0x40
#define LC1160_REG_BITMASK_LDOA14_ALLOW_IN_SLP     	0x20
#define LC1160_REG_BITMASK_LDOA13_ALLOW_IN_SLP     	0x10
#define LC1160_REG_BITMASK_LDOA12_ALLOW_IN_SLP     	0x08
#define LC1160_REG_BITMASK_LDOA11_ALLOW_IN_SLP     	0x04
#define LC1160_REG_BITMASK_LDOA10_ALLOW_IN_SLP     	0x02
#define LC1160_REG_BITMASK_LDOA9_ALLOW_IN_SLP     	0x01
/* LDOD_SLEEP_MODE1: LDOD1-8 Sleep Mode Register(0x4f) */
#define LC1160_REG_BITMASK_LDOD8_ALLOW_IN_SLP     	0x80
#define LC1160_REG_BITMASK_LDOD7_ALLOW_IN_SLP     	0x40
#define LC1160_REG_BITMASK_LDOD6_ALLOW_IN_SLP     	0x20
#define LC1160_REG_BITMASK_LDOD5_ALLOW_IN_SLP     	0x10
#define LC1160_REG_BITMASK_LDOD4_ALLOW_IN_SLP     	0x08
#define LC1160_REG_BITMASK_LDOD3_ALLOW_IN_SLP     	0x04
#define LC1160_REG_BITMASK_LDOD2_ALLOW_IN_SLP     	0x02
#define LC1160_REG_BITMASK_LDOD1_ALLOW_IN_SLP     	0x01
/* LDOD_SLEEP_MODE2: LDOD9-11 Sleep Mode Register(0x50) */
#define LC1160_REG_BITMASK_LDOD11_ALLOW_IN_SLP     	0x04
#define LC1160_REG_BITMASK_LDOD10_ALLOW_IN_SLP     	0x02
#define LC1160_REG_BITMASK_LDOD9_ALLOW_IN_SLP     	0x01
/* SINKCR: Current Sinks Control Register(0x51) */
#define LC1160_REG_BITMASK_FLASH     					0x08
#define LC1160_REG_BITMASK_VIBRATOREN     				0x04
#define LC1160_REG_BITMASK_LCDEN     					0x02
#define LC1160_REG_BITMASK_KEYEN     					0x01
/* LEDIOUT: LED Driver Output Current Setting Register(0x52) */
#define LC1160_REG_BITMASK_LCDIOUT     					0xf0
#define LC1160_REG_BITMASK_KEYIOUT     					0x0f
/* VIBI_FLASHIOUT: Vibrator/flash driver Output Current Setting Register(0x53) */
#define LC1160_REG_BITMASK_FLASHIOUT     				0xf0
#define LC1160_REG_BITMASK_VIBIOUT     					0x0f
/* INDDIM: LCD current sink Dimming Register(0x54) */
#define LC1160_REG_BITMASK_DIMEN     					0x0c
#define LC1160_REG_BITMASK_FDIM     						0x03
/* CHGFILTER: Charging Filter Time Register(0x55) */
#define LC1160_REG_BITMASK_BATU3_TIME     				0xc0
#define LC1160_REG_BITMASK_EOC_TIME     				0x30
#define LC1160_REG_BITMASK_BATU33_TIME     				0x0c
#define LC1160_REG_BITMASK_RCHG_TIME     				0x03
/* CHGCR: Charging Control Register(0x56) */
#define LC1160_REG_BITMASK_EOCEN     					0x20
#define LC1160_REG_BITMASK_NTCEN     					0x10
#define LC1160_REG_BITMASK_ACHGON     					0x01
/* CHGVSET: Charging Voltage Setting Register(0x57) */
#define LC1160_REG_BITMASK_CVADJ     					0x60
#define LC1160_REG_BITMASK_CVSEL     					0x10
#define LC1160_REG_BITMASK_RCHGSEL     				0x07
/* CHGCSET: Charging Current Setting Register(0x58) */
#define LC1160_REG_BITMASK_CCPC     					0x30
#define LC1160_REG_BITMASK_ACCSEL     					0x0f
/* CHGTSET: Charging Timer Setting Register(0x59) */
#define LC1160_REG_BITMASK_EOC_FILTER     				0x80
#define LC1160_REG_BITMASK_RTIM     						0x70
#define LC1160_REG_BITMASK_RTIMCLRB     				0x08
#define LC1160_REG_BITMASK_RECCMPEN     				0x04
#define LC1160_REG_BITMASK_RTIMBP     					0x02
#define LC1160_REG_BITMASK_RTIMSTP     					0x01
/* CHGSTATUS: Reg Charging Status Register(0x5a) */
#define LC1160_REG_BITMASK_RTIMOV     					0x80
#define LC1160_REG_BITMASK_CHGCV     					0x40
#define LC1160_REG_BITMASK_ADPUV     					0x20
#define LC1160_REG_BITMASK_ADPOV     					0x10
#define LC1160_REG_BITMASK_BATOT     					0x08
#define LC1160_REG_BITMASK_RECSTATUS     				0x04
#define LC1160_REG_BITMASK_CHGSHDN     				0x02
#define LC1160_REG_BITMASK_CHGCP     					0x01
/* PCST: Power-related Status Register(0x5b) */
#define LC1160_REG_BITMASK_HFPWR     					0x40
#define LC1160_REG_BITMASK_KONMON     					0x20
#define LC1160_REG_BITMASK_BATV45     					0x10
#define LC1160_REG_BITMASK_VBATV32     					0x08
#define LC1160_REG_BITMASK_VBATV33     					0x04
#define LC1160_REG_BITMASK_BATOUT     					0x02
#define LC1160_REG_BITMASK_ADPIN     					0x01
/* STARTUP_STATUS: Start-up Status Register(0x5c) */
#define LC1160_REG_BITMASK_RSTINB_STARTUP     		0x20
#define LC1160_REG_BITMASK_ALARM2_STARTUP     		0x10
#define LC1160_REG_BITMASK_ALARM1_STARTUP     		0x08
#define LC1160_REG_BITMASK_ADPIN_STARTUP     			0x04
#define LC1160_REG_BITMASK_HF_PWR_STARTUP     		0x02
#define LC1160_REG_BITMASK_KEYON_STARTUP     			0x01
/* SHDN_STATUS: Shut-down Status Register(0x5d) */
#define LC1160_REG_BITMASK_RSTINB_SHDN     			0x20
#define LC1160_REG_BITMASK_KEYON_SHDN     			0x10
#define LC1160_REG_BITMASK_BATUV_SHDN     				0x04
#define LC1160_REG_BITMASK_TSD_SHDN     				0x02
#define LC1160_REG_BITMASK_PWREN_SHDN     			0x01
/* SHDN_EN: Shutdown Enable Register(0x5e) */
#define LC1160_REG_BITMASK_KEYON_SHDN_TIME     		0x40
#define LC1160_REG_BITMASK_RSTINB_SHDN_EN     		0x20
#define LC1160_REG_BITMASK_KEYON_SHDN_EN    			0x10
#define LC1160_REG_BITMASK_BATUV_SHDN_EN     			0x04
#define LC1160_REG_BITMASK_TSD_SHDN_EN     			0x02
#define LC1160_REG_BITMASK_PWREN_SHDN_EN     			0x01
/* BATUVCR: Battery Undervoltage Interrupt Control Register(0x5f) */
#define LC1160_REG_BITMASK_TRMOD     					0x10
#define LC1160_REG_BITMASK_BATUV_TH     				0x03
/* RTC_SEC: RTC Second Counter Register(0x60) */
#define LC1160_REG_BITMASK_RTC_SEC     						0x3f
/* RTC_MIN: RTC Minute Counter Register(0x61) */
#define LC1160_REG_BITMASK_RTC_MIN     						0x3f
/* RTC_HOUR: RTC Hour Counter Register(0x62) */
#define LC1160_REG_BITMASK_RTC_HOUR     					0x1f
/* RTC_DAY: RTC Day of a Month Counter Register(0x63) */
#define LC1160_REG_BITMASK_RTC_DAY     						0x1f
/* RTC_WEEK: RTC Current Day of a Week Counter Register(0x64) */
#define LC1160_REG_BITMASK_RTC_WEEK     					0x07
/* RTC_MONTH: RTC Month Counter Register(0x65) */
#define LC1160_REG_BITMASK_RTC_MONTH     					0x0f
/* RTC_YEAR: RTC Year Counter Register(0x66) */
#define LC1160_REG_BITMASK_RTC_YEAR     					0x7f
/* RTC_TIME_UPDATE: RTC Time Update Enable Register(0x67) */
#define LC1160_REG_BITMASK_RTC_TIME_UPDATE     			0xff
/* RTC_32K_ADJL: 8 LSBS of RTC Clock Adjust Register(0x68) */
#define LC1160_REG_BITMASK_RTC_ADJL     						0xff
/* RTC_32K_ADJH: 8 MSBS of RTC Clock Adjust Register(0x69) */
#define LC1160_REG_BITMASK_RTC_ADJH     						0x0f
/* RTC_SEC_CYCL: 8 LSBS of RTC Second Cycle Limit Register(0x6a) */
#define LC1160_REG_BITMASK_RTC_CYCL     					0xff
/* RTC_SEC_CYCH: 8 MSBS of RTC Second Cycle Limit Register(0x6b) */
#define LC1160_REG_BITMASK_RTC_CYCH     					0xff
/* RTC_CALIB_UPDATE: RTC Clock Calibration Update Register(0x6c) */
#define LC1160_REG_BITMASK_RTC_CALIB_UPDATE     			0xff
/* RTC_AL1_SEC: Second Setting for Alarm Interrupt 1(0x6d) */
#define LC1160_REG_BITMASK_RTC_AL1_SEC     					0x3f
/* RTC_AL1_MIN: Minute Setting for Alarm Interrupt 1(0x6e) */
#define LC1160_REG_BITMASK_RTC_AL1_MIN     					0x3f
/* RTC_AL1_MIN: Minute Setting for Alarm Interrupt 1(0x7c) */
#define LC1160_REG_BITMASK_RTC_AL1_HOUR     				0x1f
/* RTC_AL1_DAY: Day Setting for Alarm Interrupt 1(0x7d) */
#define LC1160_REG_BITMASK_RTC_AL1_DAY     					0x1f
/* RTC_AL1_MONTH: Month Setting for Alarm Interrupt 1(0x7e) */
#define LC1160_REG_BITMASK_RTC_AL1_MONTH     				0x0f
/* RTC_AL1_YEAR: Year Setting for Alarm Interrupt 1(0x7f) */
#define LC1160_REG_BITMASK_RTC_AL1_YEAR     				0x7f
/* RTC_AL2_SEC: Second Setting for Alarm Interrupt 2(0x6f) */
#define LC1160_REG_BITMASK_RTC_AL2_SEC     					0x3f
/* RTC_AL2_MIN: Minute Setting for Alarm Interrupt 2(0x70) */
#define LC1160_REG_BITMASK_RTC_AL2_MIN     					0x3f
/* RTC_AL2_HOUR: Hour Setting for Alarm Interrupt 2o(0x71) */
#define LC1160_REG_BITMASK_RTC_AL2_HOUR     				0x1f
/* RTC_AL2_HOUR: Hour Setting for Alarm Interrupt 2o(0x72) */
#define LC1160_REG_BITMASK_RTC_AL2_DAY     					0x1f
/* RTC_AL2_MONTH: Month Setting for Alarm Interrupt 2(0x73) */
#define LC1160_REG_BITMASK_RTC_AL2_MONTH     				0x0f
/* RTC_AL2_YEAR: Year Setting for Alarm Interrupt 2(0x74) */
#define LC1160_REG_BITMASK_RTC_AL2_YEAR     				0x7f
/* For RTC bug version */
#define LC1160_REG_BITMASK_RTC_AL2_YEAR2     				0x1f
/* RTC_AL_UPDATE: RTC Alarm Time Update Register(0x75) */
#define LC1160_REG_BITMASK_RTC_AL_UPDATE     				0xff
/* RTC_INT_EN: RTC Interrupt Enable Register(0x76) */
#define LC1160_REG_BITMASK_RTC_AL1_EN     					0x20
#define LC1160_REG_BITMASK_RTC_AL2_EN     					0x10
#define LC1160_REG_BITMASK_RTC_SEC_EN     					0x08
#define LC1160_REG_BITMASK_RTC_MIN_EN     					0x04
#define LC1160_REG_BITMASK_RTC_HOUR_EN     				0x02
#define LC1160_REG_BITMASK_RTC_DAY_EN     					0x01
/* RTC_INT_STATUS: RTC Interrupt Status Register(0x77) */
#define LC1160_REG_BITMASK_RTC_AL1IS     					0x20
#define LC1160_REG_BITMASK_RTC_AL2IS     					0x10
#define LC1160_REG_BITMASK_RTC_SECIS     					0x08
#define LC1160_REG_BITMASK_RTC_MINIS     					0x04
#define LC1160_REG_BITMASK_RTC_HOURIS     					0x02
#define LC1160_REG_BITMASK_RTC_DAYIS     					0x01
/* RTC_INT_RAW: RTC Raw Interrupt Status Register(0x78) */
#define LC1160_REG_BITMASK_RTC_AL1_RAW     					0x20
#define LC1160_REG_BITMASK_RTC_AL2_RAW     					0x10
#define LC1160_REG_BITMASK_RTC_SEC_RAW     				0x08
#define LC1160_REG_BITMASK_RTC_MIN_RAW     					0x04
#define LC1160_REG_BITMASK_RTC_HOUR_RAW     				0x02
#define LC1160_REG_BITMASK_RTC_DAY_RAW     					0x01
/* RTC_CTRL: RTC Control Register(0x79) */
#define LC1160_REG_BITMASK_RTC_RESET_FLAG     		0x02
#define LC1160_REG_BITMASK_RTC_CNT_EN     					0x01
/* RTC_MASK: RTC Interrupt Mask Register(0x7b) */
#define LC1160_REG_BITMASK_RTC_INT_MASK     				0xff
/* DATA1: DATA Register(0x80) */
#define LC1160_REG_BITMASK_DATA1     					0xff
/* DATA2: DATA Register(0x81) */
#define LC1160_REG_BITMASK_DATA2     					0xff
/* DATA2: DATA Register(0x82) */
#define LC1160_REG_BITMASK_DATA3     					0xff
/* DATA2: DATA Register(0x83) */
#define LC1160_REG_BITMASK_DATA4     					0xff
/* ADCCR1: A/D Converter Control Register1(0x87) */
#define LC1160_REG_BITMASK_TS_PCB_VREF     			0xe0
#define LC1160_REG_BITMASK_ADEND     					0x10
#define LC1160_REG_BITMASK_ADSTART     					0x08
#define LC1160_REG_BITMASK_ADFORMAT     				0x04
#define LC1160_REG_BITMASK_ADCEN     					0x02
#define LC1160_REG_BITMASK_LDOADCEN     				0x01
/* ADCCR2: A/D Converter Control Register2(0x88) */
#define LC1160_REG_BITMASK_TS_PCB_ON     				0x10
#define LC1160_REG_BITMASK_ADMODE     					0x08
#define LC1160_REG_BITMASK_ADSEL     					0x07
/* ADCDAT0: LSBs of A/D Conversion Data Register(0x89) */
#define LC1160_REG_BITMASK_ADDATL     					0x0f
/* ADCDAT1: MSBs of A/D Conversion Data Register(0x8a) */
#define LC1160_REG_BITMASK_ADDATH     					0xff
/* I2C_CLK_TSCR: I2C/CLK/TS Control Register(0x8b) */
#define LC1160_REG_BITMASK_TS_PRG     					0xe0
#define LC1160_REG_BITMASK_TSDEN     					0x10
#define LC1160_REG_BITMASK_CLK26M_DIV_EN     			0x08
#define LC1160_REG_BITMASK_CLK_SEL     					0x04
#define LC1160_REG_BITMASK_INCR     						0x03
/* IRQST_FINAL: Final Interrupt Status Register(0x8c) */
#define LC1160_REG_BITMASK_CODECIRQ     				0x10
#define LC1160_REG_BITMASK_JKHSIRQ     					0x08
#define LC1160_REG_BITMASK_IRQ2     						0x04
#define LC1160_REG_BITMASK_IRQ1     						0x02
#define LC1160_REG_BITMASK_RTCIRQ     					0x01
/* IRQST1: Interrupt Status Register 1(0x8d) */
#define LC1160_REG_BITMASK_TS_PCB_OUTIR     			0x80
#define LC1160_REG_BITMASK_CHGOVIR     					0x40
#define LC1160_REG_BITMASK_CHGUVIR     					0x20
#define LC1160_REG_BITMASK_RCHGIR     					0x10
#define LC1160_REG_BITMASK_CHGCVIR     					0x08
#define LC1160_REG_BITMASK_BATOVIR     					0x04
#define LC1160_REG_BITMASK_BATUVIR     					0x02
#define LC1160_REG_BITMASK_BATOTIR     					0x01
/* IRQST2: Interrupt Status Register 2(0x8e) */
#define LC1160_REG_BITMASK_TSDIR			     			0x80
#define LC1160_REG_BITMASK_RTIMIR     					0x40
#define LC1160_REG_BITMASK_CHGCPIR     					0x20
#define LC1160_REG_BITMASK_ADCCPIR     					0x10
#define LC1160_REG_BITMASK_KONIR     					0x08
#define LC1160_REG_BITMASK_KOFFIR     					0x04
#define LC1160_REG_BITMASK_ADPINIR     					0x02
#define LC1160_REG_BITMASK_ADPOUTIR     				0x01
/* IRQEN1: Interrupt Enable Register 1(0x8f) */
#define LC1160_REG_BITMASK_TS_PCB_OUTIREN			0x80
#define LC1160_REG_BITMASK_CHGOVIREN     				0x40
#define LC1160_REG_BITMASK_CHGUVIREN     				0x20
#define LC1160_REG_BITMASK_RCHGIREN     				0x10
#define LC1160_REG_BITMASK_CHGCVIREN    				0x08
#define LC1160_REG_BITMASK_BATOVIREN     				0x04
#define LC1160_REG_BITMASK_BATUVIREN     				0x02
#define LC1160_REG_BITMASK_BATOTIREN     				0x01
/* IRQEN2: Interrupt Enable Register 2(0x90) */
#define LC1160_REG_BITMASK_TSDIREN					0x80
#define LC1160_REG_BITMASK_RTIMIREN     				0x40
#define LC1160_REG_BITMASK_CHGCPIREN     				0x20
#define LC1160_REG_BITMASK_ADCCPIREN     				0x10
#define LC1160_REG_BITMASK_KONIREN    					0x08
#define LC1160_REG_BITMASK_KOFFIREN     				0x04
#define LC1160_REG_BITMASK_ADPINIREN     				0x02
#define LC1160_REG_BITMASK_ADPOUTIREN     				0x01
/* IRQMSK1: Interrupt Mask Register 1(0x91) */
#define LC1160_REG_BITMASK_TS_PCB_OUTIRMSK			0x80
#define LC1160_REG_BITMASK_CHGOVIRMSK     			0x40
#define LC1160_REG_BITMASK_CHGUVIRMSK     				0x20
#define LC1160_REG_BITMASK_RCHGIRMSK     				0x10
#define LC1160_REG_BITMASK_CHGCVIRMSK    				0x08
#define LC1160_REG_BITMASK_BATOVIRMSK     				0x04
#define LC1160_REG_BITMASK_BATUVIRMSK     				0x02
#define LC1160_REG_BITMASK_BATOTIRMSK    				0x01
/* IRQMSK2: Interrupt Mask Register 2(0x92) */
#define LC1160_REG_BITMASK_TSDIRMSK					0x80
#define LC1160_REG_BITMASK_RTIMIRMSK     				0x40
#define LC1160_REG_BITMASK_CHGCPIRMSK     			0x20
#define LC1160_REG_BITMASK_ADCCPIRMSK     				0x10
#define LC1160_REG_BITMASK_KONIRMSK    				0x08
#define LC1160_REG_BITMASK_KOFFIRMSK     				0x04
#define LC1160_REG_BITMASK_ADPINIRMSK     				0x02
#define LC1160_REG_BITMASK_ADPOUTIRMSK    			0x01
/* PWM_CTL: PWM Control Register(0x95) */
#define LC1160_REG_BITMASK_PWM1_EN     				0x40
#define LC1160_REG_BITMASK_PWM1_UPDATE     			0x20
#define LC1160_REG_BITMASK_PWM1_RESET    				0x10
#define LC1160_REG_BITMASK_PWM0_EN     				0x04
#define LC1160_REG_BITMASK_PWM0_UPDATE     			0x02
#define LC1160_REG_BITMASK_PWM0_RESET    				0x01
/* PWM0_P: PWM Period Setting Register(0x96) */
#define LC1160_REG_BITMASK_PWM0_PERIOD     			0xff
/* PWM1_P: PWM Period Setting Register(0x97) */
#define LC1160_REG_BITMASK_PWM1_PERIOD     			0xff
/* PWM0_OCPY: PWM Duty Cycle Setting Register(0x98) */
#define LC1160_REG_BITMASK_PWM0_OCPY_RATIO     		0x3f
/* PWM1_OCPY: PWM Duty Cycle Setting Register(0x99) */
#define LC1160_REG_BITMASK_PWM1_OCPY_RATIO     		0x3f
/* OTPSEL0: OTP Trimming Enable Register0(0x9a) */
#define LC1160_REG_BITMASK_DC4_VOUT_SEL     			0x80
#define LC1160_REG_BITMASK_DC3_VOUT_SEL     			0x40
#define LC1160_REG_BITMASK_DC2_VOUT_SEL     			0x20
#define LC1160_REG_BITMASK_DC1_VOUT_SEL     			0x10
#define LC1160_REG_BITMASK_ADC_REF_SEL     			0x08
#define LC1160_REG_BITMASK_IRO_F_SEL     				0x04
#define LC1160_REG_BITMASK_MBG_S_SEL     				0x02
#define LC1160_REG_BITMASK_CHGR_VERF_SEL     			0x01
/* OTPSEL1: OTP Trimming Enable Register1(0x9b) */
#define LC1160_REG_BITMASK_DC9_SLOPE_SEL     			0x04
#define LC1160_REG_BITMASK_DC8_SLOPE_SEL     			0x02
#define LC1160_REG_BITMASK_DC5_VOUT_SEL     			0x01
/* OTPMODE: OTP Mode Register(0x9c) */
#define LC1160_REG_BITMASK_OTPPTM     					0x06
#define LC1160_REG_BITMASK_OTPPROG     				0x01
/* OTPCMD: OTP Command Register(0x9d) */
#define LC1160_REG_BITMASK_OTPMGRD     				0x40
#define LC1160_REG_BITMASK_OTPRD     					0x20
#define LC1160_REG_BITMASK_OTPWR     					0x10
#define LC1160_REG_BITMASK_OTPPA     					0x07
/* TRIM0: Byte0 of Trimming Parameters Setting Register(0x9e) */
#define LC1160_REG_BITMASK_TRIM0     					0xff
/* TRIM1: Byte1 of Trimming Parameters Setting Register(0x9f) */
#define LC1160_REG_BITMASK_TRIM1     					0xff
/* TRIM2: Byte2 of Trimming Parameters Setting Register(0xa0) */
#define LC1160_REG_BITMASK_TRIM2     					0xff
/* TRIM3: Byte3 of Trimming Parameters Setting Register(0xa1) */
#define LC1160_REG_BITMASK_TRIM3     					0xff
/* TRIM4: Byte4 of Trimming Parameters Setting Register(0xa2) */
#define LC1160_REG_BITMASK_TRIM4     					0xff
/* TRIM5: Byte5 of Trimming Parameters Setting Register(0xa3) */
#define LC1160_REG_BITMASK_TRIM5     					0xff
/* TRIM6: Byte6 of Trimming Parameters Setting Register(0xa4) */
#define LC1160_REG_BITMASK_TRIM6     					0xff
/* TRIM7: Byte7 of Trimming Parameters Setting Register(0xa5) */
#define LC1160_REG_BITMASK_TRIM7     					0xff
/* QPVBUS: Charger pump for USB VBUS Register(0xa6) */
#define LC1160_REG_BITMASK_QPVBUS_PR     				0x1e
#define LC1160_REG_BITMASK_QPVBUS_ON     				0x01
/* JACKCR1: Jack Control register1(0xa7) */
#define LC1160_REG_BITMASK_JACKOUTIREN     			0x80
#define LC1160_REG_BITMASK_JACKINIREN     				0x40
#define LC1160_REG_BITMASK_JACKOUTIRMSK     			0x20
#define LC1160_REG_BITMASK_JACKINIRMSK     				0x10
#define LC1160_REG_BITMASK_JACKDETPOL     				0x08
#define LC1160_REG_BITMASK_JACK_CMP     				0x04
/* HS1CR: Hookswitch Control register(0xa8) */
#define LC1160_REG_BITMASK_HS1UPIREN     				0x80
#define LC1160_REG_BITMASK_HS1DNIREN     				0x40
#define LC1160_REG_BITMASK_HS1UPIRMSK     				0x20
#define LC1160_REG_BITMASK_HS1DNIRMSK     				0x10
#define LC1160_REG_BITMASK_HS1DETEN     				0x08
#define LC1160_REG_BITMASK_HS1_CMP     					0x04
#define LC1160_REG_BITMASK_HS1_VTH     					0x03


/*-------------------------------------------------------------------------------
			LC1160 register data.
-------------------------------------------------------------------------------*/

/* LC1160 interrupt. */
#define LC1160_INT_BATOTIR 				(0x00000001)
#define LC1160_INT_BATUVIR				(0x00000002)
#define LC1160_INT_BATOVIR 				(0x00000004)
#define LC1160_INT_CHGCVIR 				(0x00000008)
#define LC1160_INT_RCHGIR 				(0x00000010)
#define LC1160_INT_CHGUVIR				(0x00000020)
#define LC1160_INT_CHGOVIR				(0x00000040)
#define LC1160_INT_TS_PCB_OUTIR		(0x00000080)

#define LC1160_INT_ADPOUTIR				(0x00000100)
#define LC1160_INT_ADPINIR				(0x00000200)
#define LC1160_INT_KOFFIR				(0x00000400)
#define LC1160_INT_KONIR 				(0x00000800)
#define LC1160_INT_ADCCPIR				(0x00001000)
#define LC1160_INT_CHGCPIR 				(0x00002000)
#define LC1160_INT_RTIMIR				(0x00004000)
#define LC1160_INT_TSDIR					(0x00008000)

#define LC1160_INT_RTC_DAYIS 			(0x00010000)
#define LC1160_INT_RTC_HOURIS			(0x00020000)
#define LC1160_INT_RTC_MINIS 			(0x00040000)
#define LC1160_INT_RTC_SECIS			(0x00080000)
#define LC1160_INT_RTC_AL2IS			(0x00100000)
#define LC1160_INT_RTC_AL1IS			(0x00200000)

#define LC1160_INT_JACK_IN 				(0x01000000)
#define LC1160_INT_JACK_OUT			(0x02000000)
#define LC1160_INT_HOOK1_DOWN			(0x04000000)
#define LC1160_INT_HOOK1_UP 			(0x08000000)
#define LC1160_INT_HOOK2_DOWN			(0x10000000)
#define LC1160_INT_HOOK2_UP 			(0x20000000)
#define LC1160_INT_HOOK3_DOWN			(0x40000000)
#define LC1160_INT_HOOK3_UP 			(0x80000000)


/* LC1160 week Days. */
#define LC1160_RTC_WEEK_DAYS			(7)

/* LC1160 RTC base year. */
#define LC1160_RTC_YEAR_BASE			(1980)
#define LC1160_RTC_YEAR_MIN 			(1980)
#define LC1160_RTC_YEAR_MAX 			(2038)

/* LC1160 RTC base year for RTC 4times bug */
#define LC1160_RTC_YEAR_BASE2			(2000)
#define LC1160_RTC_YEAR_MIN2 			(2010)
#define LC1160_RTC_YEAR_MAX2 			(2017)

/* LC1160 RTC alarm number. */
#define LC1160_RTC_ALARM_NUM			(2)

/* LC1160 RTC bit values of alarm disable. */
#define LC1160_RTC_ALARM_TIME_IGNORE 		(0x80)
#define LC1160_RTC_ALARM_TIME_MASK 		(0x7f)

/*-------------------------------------------------------------------------------
				LC1160 platform data.
-------------------------------------------------------------------------------*/

/* LC1160 PMIC DCDC\DLDO\ALDO\SINK number. */
#define PMIC_DCDC_NUM				(9)
#define PMIC_DLDO_NUM				(11)
#define PMIC_ALDO_NUM				(15)
#define PMIC_ISINK_NUM				(3)
#define PMIC_EXTERNAL_NUM			(0)

/* LC1160 ADC channel. */
enum {
	LC1160_ADC_VBATT = 0,
	LC1160_ADC_TEMP,
	LC1160_ADC_ADCIN1,
	LC1160_ADC_ADCIN2,
	LC1160_ADC_ADCIN3,
};

/* LC1160 LDO VOUT map. */
struct lc1160_ldo_vout_map {
	u8 ldo_id;
	int *vout_range;
	int vout_range_size;
	unsigned int reg;
	u8 mask;
};

/* LC1160 PMIC platform data. */
struct lc1160_pmic_platform_data {
	int irq_gpio;
	struct pmic_power_ctrl_map* ctrl_map;
	int ctrl_map_num;
	struct pmic_power_module_map* module_map;
	int module_map_num;
	struct pmic_reg_st* init_regs;
	int init_regs_num;
	int (*init)(void);
};

extern int lc1160_read_adc(u8 val, u16 *out);
extern int lc1160_int_mask(int mask);
extern int lc1160_int_unmask(int mask);
extern int lc1160_int_status_get(void);
extern int lc1160_power_type_get(void);

#endif /* __LC1160H_PMIC_H__ */

