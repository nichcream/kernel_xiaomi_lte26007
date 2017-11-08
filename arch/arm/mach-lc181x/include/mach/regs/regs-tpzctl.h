#ifndef __ASM_ARCH_REGS_TPZCTL_H
#define __ASM_ARCH_REGS_TPZCTL_H

#define TPZCTL_CP_TPZ_CFG            	(0x00)
#define TPZCTL_CP_RGN_EN             	(0x04)
#define TPZCTL_CP_RGN_UPDT           	(0x08)
#define TPZCTL_CP_TPZ_DADDR          	(0x0C)
#define TPZCTL_AP_TPZ_CFG0           	(0x10)
#define TPZCTL_AP_RGN0_EN            	(0x14)
#define TPZCTL_AP_RGN0_UPDT          	(0x18)
#define TPZCTL_AP_TPZ_CFG1           	(0x1C)
#define TPZCTL_AP_RGN1_EN            	(0x20)
#define TPZCTL_AP_RGN1_UPDT          	(0x24)
#define TPZCTL_AP_TPZ_CFG2           	(0x28)
#define TPZCTL_AP_RGN2_EN            	(0x2C)
#define TPZCTL_AP_RGN2_UPDT          	(0x30)
#define TPZCTL_AP_TPZ_CFG3           	(0x34)
#define TPZCTL_AP_RGN3_EN            	(0x38)
#define TPZCTL_AP_RGN3_UPDT          	(0x3C)
#define TPZCTL_AP_TPZ_DADDR          	(0x40)
#define TPZCTL_INTRAW                	(0x44)
#define TPZCTL_INTE                  	(0x48)
#define TPZCTL_INTS                  	(0x4C)
#define TPZCTL_RANDOM_NUM            	(0x50)
#define TPZCTL_M0_RAM_SEC            	(0x54)

/* TPZCTL_CP_TPZ_CFG. */
#define TPZCTL_CFG_END_ADDR		14
#define TPZCTL_CFG_BASE_ADDR		3
#define TPZCTL_CFG_RGN_ATTR		0

/* TPZCTL. */
#define TPZCTL_RANDOM_VALUE_INTR	4
#define TPZCTL_LCDC_TPZ_INTR   		3
#define TPZCTL_AXI_TPZ_INTR    		2
#define TPZCTL_AP_TPZ_INTR    		1
#define TPZCTL_CP_TPZ_INTR    		0

/* TPZCTL_M0_RAM_SEC. */
#define TPZCTL_M0_RAM_SEC_EN		6
#define TPZCTL_M0_RAM_SEC_CFG		0

#define TPZCTL_AP_TPZ_CFG(x)		(TPZCTL_AP_TPZ_CFG0 + 0x0C * (x))
#define TPZCTL_AP_RGN_EN(x)		(TPZCTL_AP_RGN0_EN + 0x0C * (x))
#define TPZCTL_AP_RGN_UPDT(x)		(TPZCTL_AP_RGN0_UPDT + 0x0C * (x))

#endif /* __ASM_ARCH_REGS_TPZCTL_H */
