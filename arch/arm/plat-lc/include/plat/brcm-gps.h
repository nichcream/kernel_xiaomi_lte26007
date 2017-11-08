#if !defined(__ASM_ARCH_COMIP_GPS_GPIO_H)
#define __ASM_ARCH_COMIP_GPS_GPIO_H

#include <linux/wakelock.h>

struct brcm_gps_platform_data {
	int reset_state;
	int standby_state;
	int amt_state;
	int rf_trigger_state;
	int gpio_reset;
	int gpio_standby;
	int gpio_rf_trigger;
	struct wake_lock gps_wakelock;
	struct wake_lock gps_stoplock;
	int (*power)(int onoff);
};

#endif /* __ASM_ARCH_COMIP_GPS_GPIO_H */
