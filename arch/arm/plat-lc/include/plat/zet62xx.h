#ifndef __LINUX_ZET62XX_H__
#define __LINUX_ZET62XX_H__

struct zet62xx_platform_data {
	int reset_gpio;
	int irq_gpio;
	int (*init_platform_hw)(void);
	int max_scr_x;
	int max_scr_y;
	int virtual_scr_y;
	int max_points;
};

#endif
