#ifndef __LC1132H_PMIC_H__
#define __LC1132H_PMIC_H__

#include <linux/types.h>
#include <plat/comip-pmic.h>

/*-------------------------------------------------------------------------------
			LC1132 register data.
-------------------------------------------------------------------------------*/
/* DCDC Register */
#define LC1132_REG_DCEN					0x00
#define LC1132_REG_DC1OVS0					0x01
#define LC1132_REG_DC1OVS1					0x02
#define LC1132_REG_DC2OVS0          			0x03
#define LC1132_REG_DC2OVS1          			0x04
#define LC1132_REG_DC2OVS2          			0x05
#define LC1132_REG_DC2OVS3         			0x06
#define LC1132_REG_DC3OVS0					0x07
#define LC1132_REG_DC3OVS1					0x08
#define LC1132_REG_DC4OVS0					0x09
#define LC1132_REG_DC4OVS1					0x0a
#define LC1132_REG_DCMODE					0x0b
/* LDOs for analog Register */
#define LC1132_REG_LDOAEN         				0x0c
#define LC1132_REG_LDOAONEN				0x0e
#define LC1132_REG_LDOAECO				0x0f
#define LC1132_REG_LDOAOVSEL				0x10
#define LC1132_REG_LDOA6OVS		    		0x11
#define LC1132_REG_LDOA7OVS				0x12
/* LDOs for digital Register */	
#define LC1132_REG_LDODEN1   				0x13	
#define LC1132_REG_LDODEN2   				0x14	
#define LC1132_REG_LDODONEN  				0x15	
#define LC1132_REG_LDODECO1  				0x16	
#define LC1132_REG_LDODECO2  				0x17	
#define LC1132_REG_LDODOVSEL 				0x18	
#define LC1132_REG_LDOD3OVS  				0x19
#define LC1132_REG_LDOD45ENOVS  			0x1a
#define LC1132_REG_LDOD7OVS  				0x1b	
#define LC1132_REG_LDOD8OVS  				0x1c	
#define LC1132_REG_LDOD9OVS 				0x1d	
#define LC1132_REG_LDOD10OVS 				0x1e
#define LC1132_REG_LDOD11OVS 				0x1f
/* OCP and UVP control Register */
#define LC1132_REG_DCOCPEN				0x20
#define LC1132_REG_LDOAOCPEN				0x21
#define LC1132_REG_LDODOCPEN1			0x22
#define LC1132_REG_LDODOCPEN2			0x23
/* Sleep model Register */	
#define LC1132_REG_DC_SLEEP_MODE   		0x24
#define LC1132_REG_LDOA_SLEEP_MODE1		0x25
#define LC1132_REG_LDOD_SLEEP_MODE1		0x26
#define LC1132_REG_LDOD_SLEEP_MODE2		0x27
/* Led current sink driverl Register */	
#define LC1132_REG_SINKCR          				0x28
#define LC1132_REG_LEDIOUT         				0x29
#define LC1132_REG_VIBIOUT         				0x2a
#define LC1132_REG_INDDIM          				0x2c
/* Reference current Register */	
#define LC1132_REG_IREF           				0x2d
/* Battery charger Register */	
#define LC1132_REG_CHGFILTER         			0x2f
#define LC1132_REG_CHGCR         				0x30
#define LC1132_REG_CHGVSET         			0x31
#define LC1132_REG_CHGCSET       				0x32
#define LC1132_REG_CHGTSET            			0x33
#define LC1132_REG_CHGSTATUS  				0x34
/* Power status and control Register */
#define LC1132_REG_PCST     					0x35
#define LC1132_REG_STARTUP_STATUS		0x36
#define LC1132_REG_SHDN_STATUS     			0x37
#define LC1132_REG_SHDN_EN     				0x2e
#define LC1132_REG_BATUVCR     				0x3f
/* ADC Register */
#define LC1132_REG_ADCLDOEN        			0x38
#define LC1132_REG_ADCCR           				0x39
#define LC1132_REG_ADCCMD          			0x3a
#define LC1132_REG_ADCEN           				0x3b
#define LC1132_REG_ADCDAT0         			0x3c
#define LC1132_REG_ADCDAT1         			0x3d
#define LC1132_REG_ADCFORMAT       			0x3e
/* I2C Register */
#define LC1132_REG_I2CCR           				0x40
/* Clock Register */
#define LC1132_REG_CLKCR           				0x41
/* Interrupt Register */
#define LC1132_REG_IRQST_FINAL          		0x42
#define LC1132_REG_IRQST1          				0x43
#define LC1132_REG_IRQST2          				0x44
#define LC1132_REG_IRQEN1          				0x45
#define LC1132_REG_IRQEN2          				0x46
#define LC1132_REG_IRQMSK1         			0x47
#define LC1132_REG_IRQMSK2         			0x48
/* DCDC and LDOs softstart control Register */
#define LC1132_REG_DCSOFTEN           			0x49
#define LC1132_REG_LDOASOFTEN           		0x4a
#define LC1132_REG_LDODSOFTEN1           		0x4b
#define LC1132_REG_LDODSOFTEN2           		0x4c
/* DCDC and LDOs fast discharge control Register */
#define LC1132_REG_DCFASTDISC           		0x4d
#define LC1132_REG_LDOAFASTDISC           		0x4e
#define LC1132_REG_LDODFASTDISC1           	0x4f
#define LC1132_REG_LDODFASTDISC2           	0x50
/* thermal shutdown Register */
#define LC1132_REG_TSHUT           				0x51
/* test mode Register */
/* version Register */
#define LC1132_REG_VERSION         				0x5f
/* RTC Register */
#define LC1132_REG_RTC_SEC         			0x60
#define LC1132_REG_RTC_MIN         			0x61
#define LC1132_REG_RTC_HOUR        			0x62
#define LC1132_REG_RTC_DAY         			0x63
#define LC1132_REG_RTC_WEEK        			0x64
#define LC1132_REG_RTC_MONTH       			0x65
#define LC1132_REG_RTC_YEAR        			0x66
#define LC1132_REG_RTC_TIME_UPDATE 		0x67
#define LC1132_REG_RTC_32K_ADJL    			0x68
#define LC1132_REG_RTC_32K_ADJH    			0x69
#define LC1132_REG_RTC_SEC_CYCL    		0x6a
#define LC1132_REG_RTC_SEC_CYCH    		0x6b
#define LC1132_REG_RTC_CALIB_UPDATE		0x6c
#define LC1132_REG_RTC_AL1_SEC     			0x6d
#define LC1132_REG_RTC_AL1_MIN     			0x6e
#define LC1132_REG_RTC_AL2_SEC     			0x6f
#define LC1132_REG_RTC_AL2_MIN     			0x70
#define LC1132_REG_RTC_AL2_HOUR    			0x71
#define LC1132_REG_RTC_AL2_DAY     			0x72
#define LC1132_REG_RTC_AL2_MONTH   		0x73
#define LC1132_REG_RTC_AL2_YEAR    			0x74
#define LC1132_REG_RTC_AL_UPDATE   		0x75
#define LC1132_REG_RTC_INT_EN      			0x76
#define LC1132_REG_RTC_INT_STATUS  		0x77
#define LC1132_REG_RTC_INT_RAW     			0x78
#define LC1132_REG_RTC_CTRL        			0x79
#define LC1132_REG_RTC_BK          				0x7a
#define LC1132_REG_RTC_INT_MASK    			0x7b
#define LC1132_REG_RTC_AL1_HOUR    			0x7c
#define LC1132_REG_RTC_AL1_DAY    	    		0x7d
#define LC1132_REG_RTC_AL1_MONTH    		0x7e
#define LC1132_REG_RTC_AL1_YEAR    			0x7f

/* PWM Register. */
#define LC1132_REG_PWM_EN          			0x80
#define LC1132_REG_PWM_UP          			0x81
#define LC1132_REG_PWM_RESET       			0x82
#define LC1132_REG_PWM_P           				0x83
#define LC1132_REG_PWM_OCPY        			0x84

/* Jack and Hookswitch Register. */
#define LC1132_REG_JKHSIRQST				0x9a
#define LC1132_REG_JKHSMSK				0x9b

/* Codec Register. */
#define LC1132_REG_CODEC_R9				0xa5

/*-------------------------------------------------------------------------------
			LC1132 register bitmask.
-------------------------------------------------------------------------------*/

/* DCEN: DC/DC Enable Register(0x00) */
#define LC1132_REG_BITMASK_DC2_DVS_SEL		0x30
#define LC1132_REG_BITMASK_DC4EN      			0x08
#define LC1132_REG_BITMASK_DC3EN      			0x04
#define LC1132_REG_BITMASK_DC2EN      			0x02
#define LC1132_REG_BITMASK_DC1EN      			0x01

/* DC1OVS: DCDC1 Output Voltage Setting Register(0x01-0x02) */
#define LC1132_REG_BITMASK_DC1ONV			0x1f
#define LC1132_REG_BITMASK_DC1SLPV     			0x1f

/* DC2OVS: DCDC2 Output Voltage Setting Register(0x03-0x06) */
#define LC1132_REG_BITMASK_DC2OVS0VOUT0		0x1f
#define LC1132_REG_BITMASK_DC2OVS0VOUT1		0x1f
#define LC1132_REG_BITMASK_DC2OVS1VOUT2		0x1f
#define LC1132_REG_BITMASK_DC2OVS1VOUT3		0x1f

/* DC3OVS: DCDC3 Output Voltage Setting Register(0x07-0x08) */
#define LC1132_REG_BITMASK_DC3VOUT0			0x0f
#define LC1132_REG_BITMASK_DC3VOUT1			0x0f

/* DC4OVS: DCDC4 Output Voltage Setting Register(0x09-0x0a) */
#define LC1132_REG_BITMASK_DC4VOUT0			0x0f
#define LC1132_REG_BITMASK_DC4VOUT1			0x0f

/* DCMODE: DC-DC mode Control Register(0x0b) */
#define LC1132_REG_BITMASK_DC4_LP_IN_SLP		0x80
#define LC1132_REG_BITMASK_DC3_LP_IN_SLP		0x40
#define LC1132_REG_BITMASK_DC2_LP_IN_SLP		0x20
#define LC1132_REG_BITMASK_DC1_LP_IN_SLP		0x10
#define LC1132_REG_BITMASK_DC4_PWM_FORCE	0x08
#define LC1132_REG_BITMASK_DC3_PWM_FORCE	0x04
#define LC1132_REG_BITMASK_DC2_PWM_FORCE	0x02
#define LC1132_REG_BITMASK_DC1_PWM_FORCE	0x01

/* LDOAEN1: LDOA2-9 Enable Register(0x0c) */
#define LC1132_REG_BITMASK_LDOA8EN			0x80
#define LC1132_REG_BITMASK_LDOA7EN			0x40
#define LC1132_REG_BITMASK_LDOA6EN			0x20
#define LC1132_REG_BITMASK_LDOA5EN			0x10
#define LC1132_REG_BITMASK_LDOA4EN			0x08
#define LC1132_REG_BITMASK_LDOA3EN			0x04
#define LC1132_REG_BITMASK_LDOA2EN			0x02
#define LC1132_REG_BITMASK_LDOA9EN			0x01

/* LDOAONEN: LDOA External Pin Enable Register(0x0e) */
#define LC1132_REG_BITMASK_LDOA7ONEN		0x20
#define LC1132_REG_BITMASK_LDOA5ONEN		0x10
#define LC1132_REG_BITMASK_LDOA4ONEN		0x08
#define LC1132_REG_BITMASK_LDOA3ONEN		0x04
#define LC1132_REG_BITMASK_LDOA2ONEN		0x02

/* LDOAECO: LDOA8/9 ECO Mode Control Register(0x0f) */
#define LC1132_REG_BITMASK_LDOA9ECO			0x02
#define LC1132_REG_BITMASK_LDOA8ECO			0x01

/* LDOA6OVS: LDOA2/3/4 Output Voltage Setting Register(0x10) */
#define LC1132_REG_BITMASK_LDOA4OV			0x08
#define LC1132_REG_BITMASK_LDOA3OV			0x04
#define LC1132_REG_BITMASK_LDOA2OV			0x02

/* LDOA6OVS: LDOA6 Output Voltage Setting Register(0x11) */
#define LC1132_REG_BITMASK_LDOA6OV			0x0f

/* LDOA7OVS: LDOA7 Output Voltage Setting Register(0x12) */
#define LC1132_REG_BITMASK_LDOA7OV			0x0f

/* LDODEN1 Register(0x13) */
#define LC1132_REG_BITMASK_LDOD8EN			0x80
#define LC1132_REG_BITMASK_LDOD7EN			0x40
#define LC1132_REG_BITMASK_LDOD6EN			0x20
#define LC1132_REG_BITMASK_LDOD3EN			0x04
#define LC1132_REG_BITMASK_LDOD2EN			0x02
#define LC1132_REG_BITMASK_LDOD1EN			0x01

/* LDODEN2 Register(0x14) */
#define LC1132_REG_BITMASK_LDOD11SW			0x08
#define LC1132_REG_BITMASK_LDOD11EN			0x04
#define LC1132_REG_BITMASK_LDOD10EN			0x02
#define LC1132_REG_BITMASK_LDOD9EN			0x01

/* LDODEN1: LDODONEN Register(0x15) */
#define LC1132_REG_BITMASK_LDOD11ONEN		0x04
#define LC1132_REG_BITMASK_LDOD9ONEN		0x02
#define LC1132_REG_BITMASK_LDOD7ONEN		0x01

/* LDODECO1: LDODECO1 Register(0x16) */
#define LC1132_REG_BITMASK_LDOD7ECO			0x40
#define LC1132_REG_BITMASK_LDOD6ECO			0x20
#define LC1132_REG_BITMASK_LDOD5ECO			0x10
#define LC1132_REG_BITMASK_LDOD4ECO			0x08
#define LC1132_REG_BITMASK_LDOD3ECO			0x04
#define LC1132_REG_BITMASK_LDOD2ECO			0x02
#define LC1132_REG_BITMASK_LDOD1ECO			0x01

/* LDODECO2: LDODECO2 Register(0x17) */
#define LC1132_REG_BITMASK_LDOD11ECO		0x04
#define LC1132_REG_BITMASK_LDOD9ECO			0x01

/* LDODECO2: LDODOVSEL Register(0x18) */
#define LC1132_REG_BITMASK_LDOD2OV			0x04
#define LC1132_REG_BITMASK_LDOD1OV			0x01

/* LDOD3OVS Register(0x19) */
#define LC1132_REG_BITMASK_LDOD3OV			0x0f

/* LDOD4/5ENOVS Register(0x1a) */
#define LC1132_REG_BITMASK_LDOD5OV			0x78
#define LC1132_REG_BITMASK_LDOD5EN			0x04
#define LC1132_REG_BITMASK_LDOD4OV			0x02
#define LC1132_REG_BITMASK_LDOD4EN			0x01

/* LDOD7OVS Register(0x1b) */
#define LC1132_REG_BITMASK_LDOD7OV			0x0f

/* LDOD8OVS Register(0x1c) */
#define LC1132_REG_BITMASK_LDOD8OV			0x0f

/* LDOD9OVS Register(0x1d) */
#define LC1132_REG_BITMASK_LDOD9OV			0x0f

/* LDOD10OVS Register(0x1e) */
#define LC1132_REG_BITMASK_LDOD10OV			0x0f

/* LDOD11OVS Register(0x1f) */
#define LC1132_REG_BITMASK_LDOD11OV			0x0f

/* LDODOCPEN1: LDOD1-8 Over-Current Protection Enable Register(0x22) */
#define LC1132_REG_BITMASK_LDOD8OCPEN			0x80
#define LC1132_REG_BITMASK_LDOD7OCPEN			0x40
#define LC1132_REG_BITMASK_LDOD6OCPEN			0x20
#define LC1132_REG_BITMASK_LDOD5OCPEN			0x10
#define LC1132_REG_BITMASK_LDOD4OCPEN			0x08
#define LC1132_REG_BITMASK_LDOD3OCPEN			0x04
#define LC1132_REG_BITMASK_LDOD2OCPEN			0x02
#define LC1132_REG_BITMASK_LDOD1OCPEN			0x01

/* LDODOCPEN2: LDOD9-11 Over-Current Protection Enable Register(0x23) */
#define LC1132_REG_BITMASK_LDOD11OCPEN			0x04
#define LC1132_REG_BITMASK_LDOD10OCPEN			0x02
#define LC1132_REG_BITMASK_LDOD9OCPEN			0x01

/* DC_SLEEP_MODE: DC/DC Sleep Mode Register(0x24) */
#define LC1132_REG_BITMASK_DC4_ALLOW_IN_SLP		0x08
#define LC1132_REG_BITMASK_DC3_ALLOW_IN_SLP		0x04
#define LC1132_REG_BITMASK_DC2_ALLOW_IN_SLP		0x02
#define LC1132_REG_BITMASK_DC1_ALLOW_IN_SLP		0x01

/* LDOA_SLEEP_MODE1: LDOA1-8 Sleep Mode Setting Register(0x25) */
#define LC1132_REG_BITMASK_LDOA8_ALLOW_IN_SLP	0x80
#define LC1132_REG_BITMASK_LDOA7_ALLOW_IN_SLP	0x40
#define LC1132_REG_BITMASK_LDOA6_ALLOW_IN_SLP	0x20
#define LC1132_REG_BITMASK_LDOA5_ALLOW_IN_SLP	0x10
#define LC1132_REG_BITMASK_LDOA4_ALLOW_IN_SLP	0x08
#define LC1132_REG_BITMASK_LDOA3_ALLOW_IN_SLP	0x04
#define LC1132_REG_BITMASK_LDOA2_ALLOW_IN_SLP	0x02
#define LC1132_REG_BITMASK_LDOA9_ALLOW_IN_SLP	0x01

/* LDOD_SLEEP_MODE1: LDOD1-8 Sleep Mode Register(0x26) */
#define LC1132_REG_BITMASK_LDOD8_ALLOW_IN_SLP	0x80
#define LC1132_REG_BITMASK_LDOD7_ALLOW_IN_SLP	0x40
#define LC1132_REG_BITMASK_LDOD6_ALLOW_IN_SLP	0x20
#define LC1132_REG_BITMASK_LDOD5_ALLOW_IN_SLP	0x10
#define LC1132_REG_BITMASK_LDOD4_ALLOW_IN_SLP	0x08
#define LC1132_REG_BITMASK_LDOD3_ALLOW_IN_SLP	0x04
#define LC1132_REG_BITMASK_LDOD2_ALLOW_IN_SLP	0x02
#define LC1132_REG_BITMASK_LDOD1_ALLOW_IN_SLP	0x01

/* LDOD_SLEEP_MODE2: LDOD9-11 Sleep Mode Register(0x27) */
#define LC1132_REG_BITMASK_LDOD11_ALLOW_IN_SLP	0x04
#define LC1132_REG_BITMASK_LDOD10_ALLOW_IN_SLP	0x02
#define LC1132_REG_BITMASK_LDOD9_ALLOW_IN_SLP	0x01

/* SINKCR: Current Sinks Control Register(0x28) */
#define LC1132_REG_BITMASK_LCDPWMEN		0x08
#define LC1132_REG_BITMASK_VIBRATOREN	0x04
#define LC1132_REG_BITMASK_LCDEN			0x02
#define LC1132_REG_BITMASK_KEYEN			0x01

/* LEDIOUT: LED Driver Output Current Setting Register(0x29) */
#define LC1132_REG_BITMASK_LCDIOUT		0xf0
#define LC1132_REG_BITMASK_KEYIOUT		0x0f

/* VIBIOUT: LED Driver Output Current Setting Register(0x2a) */
#define LC1132_REG_BITMASK_VIBIOUT		0x0f

/* INDDIM: LCD current sink Dimming Register(0x2c) */
#define LC1132_REG_BITMASK_DIMEN			0x0c
#define LC1132_REG_BITMASK_FDIM			0x03

/* CHGCR: Charging Control Register(0X30) */
#define LC1132_REG_BITMASK_FORCEOCEN	0x80
#define LC1132_REG_BITMASK_FORCEOC		0x40
#define LC1132_REG_BITMASK_EOCEN			0x20
#define LC1132_REG_BITMASK_NTCEN			0x10
#define LC1132_REG_BITMASK_CHGPROT		0x08
#define LC1132_REG_BITMASK_CSENSE		0x04
#define LC1132_REG_BITMASK_BATSEL			0x02
#define LC1132_REG_BITMASK_ACHGON		0x01

/* CHGVSET: Charging Voltage Setting Register(0X31) */
#define LC1132_REG_BITMASK_CVADJ			0x60
#define LC1132_REG_BITMASK_CVSEL			0x10
#define LC1132_REG_BITMASK_VHYSSEL		0x08
#define LC1132_REG_BITMASK_RCHGSEL		0x07

/* CHGCSET: Charging Current Setting Register(0X32) */
#define LC1132_REG_BITMASK_CCPC			0x30
#define LC1132_REG_BITMASK_ACCSEL		0x0f

/* CHGTSET: Charging Timer Setting Register(0x33) */
#define LC1132_REG_BITMASK_RTIM			0x70
#define LC1132_REG_BITMASK_RTIMCLRB		0x08
#define LC1132_REG_BITMASK_RTIMSTP		0x01

/* CHGSTATUS: Charging Status Register(0x34) */
#define LC1132_REG_BITMASK_RTIMOV			0x80
#define LC1132_REG_BITMASK_CHPUV			0x20
#define LC1132_REG_BITMASK_ADPOV			0x10
#define LC1132_REG_BITMASK_BATOT			0x08
#define LC1132_REG_BITMASK_RECSTATUS	0x04
#define LC1132_REG_BITMASK_CHGSHDN		0x02
#define LC1132_REG_BITMASK_CHGCP			0x01

/* PCST: Power-related Status Register(0x35) */
#define LC1132_REG_BITMASK_HFPWR			0x40
#define LC1132_REG_BITMASK_KONMON		0x20
#define LC1132_REG_BITMASK_BATV45			0x10
#define LC1132_REG_BITMASK_VBATV32		0x08
#define LC1132_REG_BITMASK_VBATV33		0x04
#define LC1132_REG_BITMASK_BATOUT		0x02
#define LC1132_REG_BITMASK_ADPIN			0x01

/* STARTUP_STATUS: Start-up Status Register(0X36) */
#define LC1132_REG_BITMASK_RSTINB_STARTUP 	0x20
#define LC1132_REG_BITMASK_ALARM2_STARTUP 	0x10
#define LC1132_REG_BITMASK_ALARM1_STARTUP 	0x08
#define LC1132_REG_BITMASK_ADPIN_STARTUP 	0x04
#define LC1132_REG_BITMASK_HF_PWR_STARTUP	0x02
#define LC1132_REG_BITMASK_KEYON_STARTUP 	0x01

/* SHDN_STATUS: Shut-down Status Register(0x37) */
#define LC1132_REG_BITMASK_RSTINB_SHDN		0x20
#define LC1132_REG_BITMASK_KEYON_SHDN		0x10
#define LC1132_REG_BITMASK_BATUV_SHDN		0x04
#define LC1132_REG_BITMASK_TSD_SHDN			0x02
#define LC1132_REG_BITMASK_PWREN_SHDN		0x01

/* BATUVCR: Battery Under Voltage Interrupt Control Register(0x3f) */
#define LC1132_REG_BITMASK_TRMOD				0x10
#define LC1132_REG_BITMASK_BATUV_TH			0x03

/* ADCLDOEN: LDO for ADC Enable Register(0x38) */
#define LC1132_REG_BITMASK_LDOADCEN			0x01

/* ADCCR: A/D Converter Control Register(0x39) */
#define LC1132_REG_BITMASK_ADMODE			0x10
#define LC1132_REG_BITMASK_ADSEL				0x07

/* ADCCMD: A/D Converter Command Register(0x3a) */
#define LC1132_REG_BITMASK_ADEND				0x10
#define LC1132_REG_BITMASK_ADSTART			0x01

/* ADCEN: ADC Enable Register(0x3b) */
#define LC1132_REG_BITMASK_ADCEN				0x01

/* ADCDAT0: 4 LSBs of A/D Conversion Data Register(0x3c) */
#define LC1132_REG_BITMASK_ADDATL				0x0f

/* ADCDAT1: 8 MSBs of A/D Conversion Data Register(0x3d) */
#define LC1132_REG_BITMASK_ADDATH			0xff

/* ADCFORMAT: A/D Conversion Data Format Register(0x3e) */
#define LC1132_REG_BITMASK_ADFORMAT			0x01

/* RTC_SEC: RTC Second Counter Register(0x60) */
#define LC1132_REG_BITMASK_RTC_SEC			0x3f

/* RTC_MIN: RTC Minute Counter Register(0x61) */
#define LC1132_REG_BITMASK_RTC_MIN			0x3f

/* RTC_HOUR: RTC Hour Counter Register(0x62) */
#define LC1132_REG_BITMASK_RTC_HOUR			0x1f

/* RTC_DAY: RTC Day of a Month Counter Register(0x63) */
#define LC1132_REG_BITMASK_RTC_DAY			0x1f

/* RTC_WEEK: RTC Current Day of a Week Counter Register(0x64) */
#define LC1132_REG_BITMASK_RTC_WEEK			0x07

/* RTC_MONTH£ºRTC Month Counter Register(0x65) */
#define LC1132_REG_BITMASK_RTC_MONTH		0x0f

/* RTC_YEAR: RTC Year Counter Register(0x66) */
#define LC1132_REG_BITMASK_RTC_YEAR			0x7f

/* RTC_TIME_UPDATE: RTC Time Update Enable Register(0x67) */
#define LC1132_REG_BITMASK_RTC_TIME_UPDATE	0xff

/* RTC_32K_ADJL: 8 LSBS of RTC Clock Adjust Register(0x68) */
#define LC1132_REG_BITMASK_RTC_32K_ADJL		0xff

/* RTC_32K_ADJH: 8 MSBS of RTC Clock Adjust Register(0x69) */
#define LC1132_REG_BITMASK_RTC_32K_ADJH		0x0f

/* RTC_SEC_CYCL: 8 LSBS of RTC Second Cycle Limit Register(0x6a) */
#define LC1132_REG_BITMASK_RTC_SEC_CYCL		0xff

/* RTC_SEC_CYCH: 8 MSBS of RTC Second Cycle Limit Register(0x6b) */
#define LC1132_REG_BITMASK_RTC_SEC_CYCH		0xff

/* RTC_CALIB_UPDATE: RTC Clock Calibration Update Register(0x6c) */
#define LC1132_REG_BITMASK_RTC_CALIB_UPDATE	0xff

/* RTC_AL1_SEC: second Setting for Alarm Interrupt 1(0x6d) */
#define LC1132_REG_BITMASK_RTC_AL1_SEC		0x3f

/* RTC_AL1_MIN: Minute Setting for Alarm Interrupt 1(0x6e) */
#define LC1132_REG_BITMASK_RTC_AL1_MIN		0x3f

/* RTC_AL1_HOUR: Hour Setting for Alarm Interrupt 1(0x7c) */
#define LC1132_REG_BITMASK_RTC_AL1_HOUR		0x1f

/* RTC_AL1_DAY: Day Setting for Alarm Interrupt 1(0x7d) */
#define LC1132_REG_BITMASK_RTC_AL1_DAY		0x1f

/* RTC_AL1_MONTH: Month Setting for Alarm Interrupt 1(0x7e) */
#define LC1132_REG_BITMASK_RTC_AL1_MONTH	0x0f

/* RTC_AL1_YEAR: Year Setting for Alarm Interrupt 1(0x7f) */
#define LC1132_REG_BITMASK_RTC_AL1_YEAR		0x7f

/* RTC_AL2_SEC: second Setting for Alarm Interrupt 2(0x6f) */
#define LC1132_REG_BITMASK_RTC_AL2_SEC		0x3f

/* RTC_AL2_MIN: Minute Setting for Alarm Interrupt 2(0x70) */
#define LC1132_REG_BITMASK_RTC_AL2_MIN		0x3f

/* RTC_AL2_HOUR: Minute Setting for Alarm Interrupt 2(0x71) */
#define LC1132_REG_BITMASK_RTC_AL2_HOUR		0x1f

/* RTC_AL2_DAY: Day Setting for Alarm Interrupt 2(0x72) */
#define LC1132_REG_BITMASK_RTC_AL2_DAY		0x1f

/* RTC_AL2_MONTH: Month Setting for Alarm Interrupt 2(0x73) */
#define LC1132_REG_BITMASK_RTC_AL2_MONTH	0x0f

/* RTC_AL2_YEAR: Year Setting for Alarm Interrupt 2(0x74) */
#define LC1132_REG_BITMASK_RTC_AL2_YEAR		0x7f

/* RTC_AL_UPDATE: RTC Alarm Time Update Register(0x75) */
#define LC1132_REG_BITMASK_RTC_AL_UPDATE	0xff

/* RTC_INT_EN: RTC Interrupt Enable Register(0x76) */
#define LC1132_REG_BITMASK_RTC_AL1_EN		0x20
#define LC1132_REG_BITMASK_RTC_AL2_EN		0x10
#define LC1132_REG_BITMASK_RTC_SEC_EN		0x08
#define LC1132_REG_BITMASK_RTC_MIN_EN		0x04
#define LC1132_REG_BITMASK_RTC_HOUR_EN		0x02
#define LC1132_REG_BITMASK_RTC_DAY_EN		0x01

/* RTC_INT_STATUS: RTC Interrupt Status Register(0x77) */
#define LC1132_REG_BITMASK_RTC_AL1IS			0x20
#define LC1132_REG_BITMASK_RTC_AL2IS			0x10
#define LC1132_REG_BITMASK_RTC_SECIS			0x08
#define LC1132_REG_BITMASK_RTC_MINIS			0x04
#define LC1132_REG_BITMASK_RTC_HOURIS		0x02
#define LC1132_REG_BITMASK_RTC_DAYIS			0x01

/* RTC_INT_RAW: RTC Raw Interrupt Status Register(0x78) */
#define LC1132_REG_BITMASK_RTC_AL1_RAW		0x20
#define LC1132_REG_BITMASK_RTC_AL2_RAW		0x10
#define LC1132_REG_BITMASK_RTC_SEC_RAW		0x08
#define LC1132_REG_BITMASK_RTC_MIN_RAW		0x04
#define LC1132_REG_BITMASK_RTC_HOUR_RAW	0x02
#define LC1132_REG_BITMASK_RTC_DAY_RAW		0x01

/* RTC_CTRL: RTC Control Register(0x79) */
#define LC1132_REG_BITMASK_RTC_BAKBAT_VSEL	0x0c
#define LC1132_REG_BITMASK_RTC_RESET_FLAG	0x02
#define LC1132_REG_BITMASK_RTC_CNT_EN		0x01

/* RTC_BK: RTC Backup Register(0x7a) */
#define LC1132_REG_BITMASK_RTC_BK_DATA		0xff

/* RTC_MASK: RTC Interrupt Mask Register(0x7b) */
#define LC1132_REG_BITMASK_RTC_INT_MASK	0xff

/* I2CCR: I2C Interface Control Register(0x40) */
#define LC1132_REG_BITMASK_I2C_INCR		0x03

/* CLKCR: Clock Control Register(0x41) */
#define LC1132_REG_BITMASK_CLKDIVEN		0x02
#define LC1132_REG_BITMASK_CLKSEL		0x01

/* IRQST_FINAL: Interrupt Status Register(0x42) */
#define LC1132_REG_BITMASK_CODECIRQ		0x10
#define LC1132_REG_BITMASK_JKHSIRQ		0x08
#define LC1132_REG_BITMASK_IRQ2			0x04
#define LC1132_REG_BITMASK_IRQ1			0x02
#define LC1132_REG_BITMASK_RTCIRQ		0x01

/* IRQST1: Interrupt Status Register 1(0x43) */
#define LC1132_REG_BITMASK_CHGOVIR		0x40
#define LC1132_REG_BITMASK_CHGOCIR		0x20
#define LC1132_REG_BITMASK_RCHGIR		0x10
#define LC1132_REG_BITMASK_CHGCVIR		0x08
#define LC1132_REG_BITMASK_BATOVIR		0x04
#define LC1132_REG_BITMASK_BATUVIR		0x02
#define LC1132_REG_BITMASK_BATOTIR		0x01

/* IRQST2: Interrupt Status Register 2(0x44) */
#define LC1132_REG_BITMASK_TSDIR			0x80
#define LC1132_REG_BITMASK_RTIMIR			0x40
#define LC1132_REG_BITMASK_CHGCPIR		0x20
#define LC1132_REG_BITMASK_ADCCPIR		0x10
#define LC1132_REG_BITMASK_KONIR			0x08
#define LC1132_REG_BITMASK_KOFFIR			0x04
#define LC1132_REG_BITMASK_ADPINIR		0x02
#define LC1132_REG_BITMASK_ADPOUTIR		0x01

/* IRQEN1: Interrupt Enable Register 1(0x45) */
#define LC1132_REG_BITMASK_CHGOVIREN		0x40
#define LC1132_REG_BITMASK_CHGOCIREN	0x20
#define LC1132_REG_BITMASK_RCHGIREN		0x10
#define LC1132_REG_BITMASK_CHGCVIREN		0x08
#define LC1132_REG_BITMASK_BATOVIREN		0x04
#define LC1132_REG_BITMASK_BATUVIREN		0x02
#define LC1132_REG_BITMASK_BATOTIREN		0x01

/* IRQEN2: Interrupt Enable Register 2(0x46) */
#define LC1132_REG_BITMASK_TSDIREN		0x80
#define LC1132_REG_BITMASK_RTIMIREN		0x40
#define LC1132_REG_BITMASK_CHGCPIREN		0x20
#define LC1132_REG_BITMASK_ADCCPIREN		0x10
#define LC1132_REG_BITMASK_KONIREN		0x08
#define LC1132_REG_BITMASK_KOFFIREN		0x04
#define LC1132_REG_BITMASK_ADPINIREN		0x02
#define LC1132_REG_BITMASK_ADPOUTIREN	0x01

/* IRQMSK1: Interrupt Mask Register 1(0x47) */
#define LC1132_REG_BITMASK_CHGOVIRMSK		0x40
#define LC1132_REG_BITMASK_CHGUVIRMSK		0x20
#define LC1132_REG_BITMASK_RCHGIRMSK		0x10
#define LC1132_REG_BITMASK_CHGCVIRMSK		0x08
#define LC1132_REG_BITMASK_BATOVIRMSK		0x04
#define LC1132_REG_BITMASK_BATUVIRMSK		0x02
#define LC1132_REG_BITMASK_BATOTIRMSK		0x01

/* IRQMSK2: Interrupt Mask Register 2(0x48) */
#define LC1132_REG_BITMASK_TSDIRMSK			0x80
#define LC1132_REG_BITMASK_RTIMIRMSK			0x40
#define LC1132_REG_BITMASK_CHGCPIRMSK		0x20
#define LC1132_REG_BITMASK_ADCCPIRMSK		0x10
#define LC1132_REG_BITMASK_KONIRMSK			0x08
#define LC1132_REG_BITMASK_KOFFIRMSK			0x04
#define LC1132_REG_BITMASK_ADPINIRMSK		0x02
#define LC1132_REG_BITMASK_ADPOUTIRMSK		0x01

/* TSHUT: Thermal Shutdown Control Register(0x51) */
#define LC1132_REG_BITMASK_TSDEN				0x01

/* PWM_EN: PWM Enable Register(0x80) */
#define LC1132_REG_BITMASK_PWM_EN			0x01

/* PWM_UP: PWM Update Register(0x81) */
#define LC1132_REG_BITMASK_PWM_UPDATE		0x01

/* PWM_RESET_PWM Reset Register(0x82) */
#define LC1132_REG_BITMASK_PWM_RESET		0x01

/* PWM_P: PWM Period Setting Register(0x83) */
#define LC1132_REG_BITMASK_PWM_PERIOD		0xff

/* PWM_OCPY: PWM Duty Cycle Setting Register(0x84) */
#define LC1132_REG_BITMASK_PWM_OCPY			0x3f

/* CHGTEST: Charger Test Control Register(0x52) */
#define LC1132_REG_BITMASK_DCTEST			0x70
#define LC1132_REG_BITMASK_CHGTEST			0x0f

/* VERSION: Version Number Register(0x5f) */
#define LC1132_REG_BITMASK_VERSION			0xff

/* JKHSIRQST: Jack and Hookswitch interrupt status register(0x9a) */
#define LC1132_REG_BITMASK_JKDETPOL			0x80
#define LC1132_REG_BITMASK_HSDETEN			0x40
#define LC1132_REG_BITMASK_HS_CMP			0x20
#define LC1132_REG_BITMASK_JACK_CMP			0x10
#define LC1132_REG_BITMASK_HSUPIR			0x08
#define LC1132_REG_BITMASK_HSDNIR			0x04
#define LC1132_REG_BITMASK_JACKOUTIR			0x02
#define LC1132_REG_BITMASK_JACKINIR			0x01

/* JKHSMSK: Jack and Hookswitch interrupt enable register(0x9b) */
#define LC1132_REG_BITMASK_HSUPIREN			0x80
#define LC1132_REG_BITMASK_HSDNIREN			0x40
#define LC1132_REG_BITMASK_JACKOUTIREN			0x20
#define LC1132_REG_BITMASK_JACKINIREN			0x10
#define LC1132_REG_BITMASK_HSUPIRMSK			0x08
#define LC1132_REG_BITMASK_HSDNIRMSK			0x04
#define LC1132_REG_BITMASK_JACKOUTIRMSK			0x02
#define LC1132_REG_BITMASK_JACKINIRMSK			0x01

/*-------------------------------------------------------------------------------
			LC1132 register data.
-------------------------------------------------------------------------------*/

/* LC1132 interrupt. */
#define LC1132_INT_BATOTIR 			(0x00000001)
#define LC1132_INT_BATUVIR			(0x00000002)
#define LC1132_INT_BATOVIR 			(0x00000004)
#define LC1132_INT_CHGCVIR 			(0x00000008)
#define LC1132_INT_RCHGIR 			(0x00000010)
#define LC1132_INT_CHGUVIR			(0x00000020)
#define LC1132_INT_CHGOVIR			(0x00000040)

#define LC1132_INT_ADPOUTIR			(0x00000100)
#define LC1132_INT_ADPINIR			(0x00000200)
#define LC1132_INT_KOFFIR			(0x00000400)
#define LC1132_INT_KONIR 			(0x00000800)
#define LC1132_INT_ADCCPIR			(0x00001000)
#define LC1132_INT_CHGCPIR 			(0x00002000)
#define LC1132_INT_RTIMIR			(0x00004000)
#define LC1132_INT_TSDIR			(0x00008000)

#define LC1132_INT_RTC_DAYIS 			(0x00010000)
#define LC1132_INT_RTC_HOURIS			(0x00020000)
#define LC1132_INT_RTC_MINIS 			(0x00040000)
#define LC1132_INT_RTC_SECIS			(0x00080000)
#define LC1132_INT_RTC_AL2IS			(0x00100000)
#define LC1132_INT_RTC_AL1IS			(0x00200000)

#define LC1132_INT_JACK_IN 			(0x01000000)
#define LC1132_INT_JACK_OUT			(0x02000000)
#define LC1132_INT_HOOK_DOWN			(0x04000000)
#define LC1132_INT_HOOK_UP 			(0x08000000)

/* LC1132 week Days. */
#define LC1132_RTC_WEEK_DAYS			(7)

/* LC1132 RTC base year. */
#define LC1132_RTC_YEAR_BASE			(1980)
#define LC1132_RTC_YEAR_MIN 			(1980)
#define LC1132_RTC_YEAR_MAX 			(2038)

/* LC1132 RTC alarm number. */
#define LC1132_RTC_ALARM_NUM			(2)

/* LC1132 RTC bit values of alarm disable. */
#define LC1132_RTC_ALARM_TIME_IGNORE 		(0x80)
#define LC1132_RTC_ALARM_TIME_MASK 		(0x7f)

/*-------------------------------------------------------------------------------
				LC1132 platform data.
-------------------------------------------------------------------------------*/

/* LC1132 PMIC DCDC\DLDO\ALDO\SINK number. */
#define PMIC_DCDC_NUM				(4)
#define PMIC_DLDO_NUM				(11)
#define PMIC_ALDO_NUM				(9)
#define PMIC_ISINK_NUM				(3)
#define PMIC_EXTERNAL_NUM			(0)

/* LC1132 ADC channel. */
enum {
	LC1132_ADC_VBATT = 0,
	LC1132_ADC_TEMP,
	LC1132_ADC_ADCIN1,
	LC1132_ADC_ADCIN2,
	LC1132_ADC_ADCIN3,
};

/* LC1132 LDO VOUT map. */
struct lc1132_ldo_vout_map {
	u8 ldo_id;
	int *vout_range;
	int vout_range_size;
	unsigned int reg;
	u8 mask;
};

/* LC1132 PMIC platform data. */
struct lc1132_pmic_platform_data {
	int irq_gpio;
	struct pmic_power_ctrl_map* ctrl_map;
	int ctrl_map_num;
	struct pmic_power_module_map* module_map;
	int module_map_num;
	struct pmic_reg_st* init_regs;
	int init_regs_num;
	int (*init)(void);
};

extern int lc1132_read_adc(u8 val, u16 *out);
extern int lc1132_int_mask(int mask);
extern int lc1132_int_unmask(int mask);
extern int lc1132_int_status_get(void);

#endif /* __LC1132H_PMIC_H__ */

