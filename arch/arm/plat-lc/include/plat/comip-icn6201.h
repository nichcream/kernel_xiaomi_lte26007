#ifndef __COMIP_ICN6201_H__
#define __COMIP_ICN6201_H__

#define ICN6201_NAME "icn6201_lvds"

struct lvds_icn6201_platform_data {
	int gpio_irq;
	char *mclk_name;
	char *mclk_parent_name;
	int clk_rate;
	void (*power)(int onoff);
	void (*lvds_en)(int enable);
};

#endif
