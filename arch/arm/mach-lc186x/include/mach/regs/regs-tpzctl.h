#ifndef __ASM_ARCH_REGS_TPZCTL_H
#define __ASM_ARCH_REGS_TPZCTL_H

#define TPZCTL_INTLV_RGN_CFG            (0x00)
#define TPZCTL_INTLV_RGN_UPDT           (0x04)
#define TPZCTL_INTLV_EN           		(0x08)

/* AP DDR */
#define TPZCTL_AP_TPZ_DADDR          	(0x0C)
#define TPZCTL_AP_TPZ_CFG0           	(0x10)
#define TPZCTL_AP_RGN0_EN            	(0x14)
#define TPZCTL_AP_RGN0_UPDT          	(0x18)
#define TPZCTL_AP_TPZ_ATTR0           	(0x1C)
#define TPZCTL_AP_TPZ_CFG1            	(0x20)
#define TPZCTL_AP_RGN1_EN          		(0x24)
#define TPZCTL_AP_RGN1_UPDT           	(0x28)
#define TPZCTL_AP_TPZ_ATTR1            	(0x2C)
#define TPZCTL_AP_TPZ_CFG2          	(0x30)
#define TPZCTL_AP_RGN2_EN           	(0x34)
#define TPZCTL_AP_RGN2_UPDT            	(0x38)
#define TPZCTL_AP_TPZ_ATTR2          	(0x3C)
#define TPZCTL_AP_TPZ_CFG3          	(0x40)
#define TPZCTL_AP_RGN3_EN               (0x44)
#define TPZCTL_AP_RGN3_UPDT             (0x48)
#define TPZCTL_AP_TPZ_ATTR3             (0x4C)

/* CP DDR */
#define TPZCTL_CP_TPZ_CFG0            	(0x50)
#define TPZCTL_CP_RGN0_EN            	(0x54)
#define TPZCTL_CP_RGN0_UPDT            	(0x58)
#define TPZCTL_CP_TPZ_CFG1            	(0x60)
#define TPZCTL_CP_RGN1_EN            	(0x64)
#define TPZCTL_CP_RGN1_UPDT            	(0x68)
#define TPZCTL_CP_TPZ_CFG2            	(0x70)
#define TPZCTL_CP_RGN2_EN            	(0x74)
#define TPZCTL_CP_RGN2_UPDT            	(0x78)
#define TPZCTL_CP_TPZ_CFG3            	(0x80)
#define TPZCTL_CP_RGN3_EN            	(0x84)
#define TPZCTL_CP_RGN3_UPDT            	(0x88)
#define TPZCTL_CP_TPZ_DADDR            	(0x8C)

/* CPA7 DDR */
#define TPZCTL_CPA7_TPZ_CFG0            (0x90)
#define TPZCTL_CPA7_RGN0_EN            	(0x94)
#define TPZCTL_CPA7_RGN0_UPDT           (0x98)
#define TPZCTL_CPA7_TPZ_CFG1            (0xA0)
#define TPZCTL_CPA7_RGN1_EN            	(0xA4)
#define TPZCTL_CPA7_RGN1_UPDT           (0xA8)
#define TPZCTL_CPA7_TPZ_CFG2            (0xB0)
#define TPZCTL_CPA7_RGN2_EN            	(0xB4)
#define TPZCTL_CPA7_RGN2_UPDT           (0xB8)
#define TPZCTL_CPA7_TPZ_CFG3            (0xC0)
#define TPZCTL_CPA7_RGN3_EN            	(0xC4)
#define TPZCTL_CPA7_RGN3_UPDT           (0xC8)
#define TPZCTL_CPA7_TPZ_DADDR           (0xCC)

/* TOP DDR */
#define TPZCTL_TOP_TPZ_CFG0            	(0xD0)
#define TPZCTL_TOP_RGN0_EN            	(0xD4)
#define TPZCTL_TOP_RGN0_UPDT            (0xD8)
#define TPZCTL_TOP_TPZ_CFG1            	(0xE0)
#define TPZCTL_TOP_RGN1_EN            	(0xE4)
#define TPZCTL_TOP_RGN1_UPDT            (0xE8)
#define TPZCTL_TOP_TPZ_CFG2            	(0xF0)
#define TPZCTL_TOP_RGN2_EN            	(0xF4)
#define TPZCTL_TOP_RGN2_UPDT            (0xF8)
#define TPZCTL_TOP_TPZ_CFG3            	(0x100)
#define TPZCTL_TOP_RGN3_EN            	(0x104)
#define TPZCTL_TOP_RGN3_UPDT            (0x108)
#define TPZCTL_TOP_TPZ_DADDR            (0x10C)

/* INT */
#define TPZCTL_INTRAW            		(0x110)
#define TPZCTL_INTE            			(0x114)
#define TPZCTL_INTS            			(0x118)

/* TPZCTL. */
#define TPZCTL_AP_DSI_ISP_TPZ_INTR   		7
#define TPZCTL_AP_VIDEO_TPZ_INTR   		6
#define TPZCTL_AP_SW2_TPZ_INTR   		5
#define TPZCTL_AP_CCI400_TPZ_INTR   		4
#define TPZCTL_TOP_TPZ_INTR   		3
#define TPZCTL_CPA7_TPZ_INTR    		1
#define TPZCTL_CP_TPZ_INTR    		0

#define TPZCTL_TOP_TPZ_CFG(x)		(TPZCTL_TOP_TPZ_CFG0 + 0x10 * (x))
#define TPZCTL_TOP_RGN_EN(x)		(TPZCTL_TOP_RGN0_EN + 0x10 * (x))
#define TPZCTL_TOP_RGN_UPDT(x)		(TPZCTL_TOP_RGN0_UPDT + 0x10 * (x))

#define TPZCTL_CP_TPZ_CFG(x)		(TPZCTL_CP_TPZ_CFG0 + 0x10 * (x))
#define TPZCTL_CP_RGN_EN(x)		(TPZCTL_CP_RGN0_EN + 0x10 * (x))
#define TPZCTL_CP_RGN_UPDT(x)		(TPZCTL_CP_RGN0_UPDT + 0x10 * (x))


#define TPZCTL_AP_TPZ_CFG(x)		(TPZCTL_AP_TPZ_CFG0 + 0x10 * (x))
#define TPZCTL_AP_TPZ_ATTR(x)		(TPZCTL_AP_TPZ_ATTR0 + 0x10 * (x))
#define TPZCTL_AP_RGN_EN(x)		(TPZCTL_AP_RGN0_EN + 0x10 * (x))
#define TPZCTL_AP_RGN_UPDT(x)		(TPZCTL_AP_RGN0_UPDT + 0x10 * (x))

#endif /* __ASM_ARCH_REGS_TPZCTL_H */
