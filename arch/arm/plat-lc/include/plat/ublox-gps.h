#if !defined(__ASM_ARCH_COMIP_GPS_GPIO_H)
#define __ASM_ARCH_COMIP_GPS_GPIO_H

struct ublox_gps_platform_data {
	int gpio_reset;
	int power_state;
	void (*power)(int onoff);
};

#endif /* __ASM_ARCH_COMIP_GPS_GPIO_H */
