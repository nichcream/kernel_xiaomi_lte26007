/*
 * Gas_Gauge driver for CW2015/2013
 * Copyright (C) 2012, CellWise
 *
 * Authors: ChenGang <ben.chen@cellwise-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.And this driver depends on
 * I2C and uses IIC bus for communication with the host.
 *
 */

#ifndef CW2015_BATTERY_H
#define CW2015_BATTERY_H

#define SIZE_BATINFO    64

struct cw_bat_platform_data {
	int bat_low_pin;
	int *battery_tmp_tbl;
	unsigned int tmptblsize;
	u8* cw_bat_config_info;
};

#endif
extern int g_cw2015_capacity;
extern int g_cw2015_vol;