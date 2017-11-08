/*
 * MELFAS MMS Touchscreen Driver - Platform Data
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 * Default path : linux/platform_data/mms_ts.h
 *
 */

#ifndef _LINUX_MMS_TOUCH_H
#define _LINUX_MMS_TOUCH_H

struct mms_ts_platform_data {
	int (*reset)(struct device *);
	int (*power_i2c)(struct device *,unsigned int);
	int (*power_ic)(struct device *,unsigned int);
	int (*gpio_intr)(struct device *);

	int max_x;
	int max_y;

	int gpio_resetb;	//required (interrupt signal)
	int gpio_vdd_en;	//required (power control)
//	int gpio_intr;
	int gpio_sda;	  	//optional
	int gpio_scl;		//optional

	int power_i2c_flag;
	int power_ic_flag;
	int vendorid_a;
	int vendorid_b;
	int max_scrx;
	int max_scry;
	int virtual_scry;
	int max_points;

};

#endif /* _LINUX_MMS_TOUCH_H */
