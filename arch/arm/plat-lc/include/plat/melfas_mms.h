/*
 * MELFAS MMS Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 *
 * Platform Data
 *
 * Default path : linux/platform_data/melfas_mms.h
 *
 */

#ifndef _LINUX_MMS_TOUCH_H
#define _LINUX_MMS_TOUCH_H

#ifdef CONFIG_OF
#define MMS_USE_DEVICETREE		1
#else
#define MMS_USE_DEVICETREE		0
#endif

#define MMS_USE_CALLBACK	1	// 0 or 1 : Callback for inform charger, display, power, etc...

#define MMS_DEVICE_NAME	"mms438"

/**
* Platform Data
*/
struct mms_platform_data {
	int (*reset)(struct device *);
	int (*power_i2c)(struct device *,unsigned int);
	int (*power_ic)(struct device *,unsigned int);

	unsigned int max_x;
	unsigned int max_y;

	int gpio_intr;			//Required (interrupt signal)

	int gpio_vdd_en;		//Optional (power control)

	//int gpio_reset;		//Optional
	//int gpio_sda;			//Optional
	//int gpio_scl;			//Optional

#if MMS_USE_CALLBACK
	void (*register_callback) (void *);
#endif

	int power_i2c_flag;
	int power_ic_flag;
	int vendorid_a;
	int vendorid_b;
	int max_scrx;
	int max_scry;
	int virtual_scry;
	int max_points;

};

#endif

