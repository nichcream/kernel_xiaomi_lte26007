#ifndef __ASM_ARCH_LC1160_PWM_H
#define __ASM_ARCH_LC1160_PWM_H

/* PWM Register. */
#define LC1160_REG_PWM_CTL          			0x95
#define LC1160_REG_PWM0_P          				0x96
#define LC1160_REG_PWM1_P       				0x97
#define LC1160_REG_PWM0_OCPY        			0x98
#define LC1160_REG_PWM1_OCPY        			0x99

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

#define LC1160_PWM_BASE	(2)
#define LC1160_PWM_NUM		(2)

/* LC1160 PWM platform data. */
struct lc1160_pwm_platform_data {
	void (*dev_init)(int id);
};

#endif /* __ASM_ARCH_LC1160_PWM_H */
