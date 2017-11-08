#ifndef __ASM_ARCH_COMIP_PWM_H
#define __ASM_ARCH_COMIP_PWM_H

#define COMIP_PWM_BASE	(0)
#define COMIP_PWM_NUM		(2)

struct comip_pwm_platform_data {
	void (*dev_init)(int id);
};

#endif /* __ASM_ARCH_COMIP_PWM_H */
