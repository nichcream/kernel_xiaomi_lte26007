#if !defined(__ASM_ARCH_BRCM_BLUETOOTH_H)
#define __ASM_ARCH_BRCM_BLUETOOTH_H

struct brcm_bt_platform_data {
	const char *name;
	int gpio_wake_i;
	int gpio_wake_o;
	int (*poweron)(void);
	int (*poweroff)(void);
};

#endif /* __ASM_ARCH_BRCM_BLUETOOTH_H */
