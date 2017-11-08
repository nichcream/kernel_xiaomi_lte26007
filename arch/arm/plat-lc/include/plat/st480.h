/*
 * Copyright (C) 2012 Senodia.
 *
 * Author: Tori Xu <xuezhi_xu@senodia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Definitions for senodia compass chip.
 */
#ifndef ST480_H
#define ST480_H


/*
 * ABS(min~max)
 */
#define ABSMIN_MAG	-32767
#define ABSMAX_MAG	32767

/*
 * device id
 */
#define ST480_DEVICE_ID 0x7C
#define IC_CHECK 0

/*
 *
 */
#define ST480_I2C_ADDRESS 0x0c

/*
 * I2C name
 */
#define ST480_I2C_NAME "st480"

/*
 * register shift
 */
#define ST480_REG_DRR_SHIFT 2

/*
 * BURST MODE(INT)
 */
#define ST480_BURST_MODE 0
#define BURST_MODE_CMD 0x1F
#define BURST_MODE_DATA_LOW 0x01

/*
 * SINGLE MODE
 */
#define ST480_SINGLE_MODE 1
#define SINGLE_MEASUREMENT_MODE_CMD 0x3F

/*
 * register
 */
#define READ_MEASUREMENT_CMD 0x4F
#define WRITE_REGISTER_CMD 0x60
#define READ_REGISTER_CMD 0x50
#define EXIT_REGISTER_CMD 0x80
#define MEMORY_RECALL_CMD 0xD0
#define MEMORY_STORE_CMD 0xE0
#define RESET_CMD 0xF0

#define CALIBRATION_REG (0x02 << ST480_REG_DRR_SHIFT)
#define CALIBRATION_DATA_LOW 0x1C
#define CALIBRATION_DATA_HIGH 0x00

#define ONE_INIT_DATA_LOW 0x7C
#define ONE_INIT_DATA_HIGH 0x00
#define ONE_INIT_REG (0x00 << ST480_REG_DRR_SHIFT)

#define TWO_INIT_DATA_LOW 0x00
#define TWO_INIT_DATA_HIGH 0x00
#define TWO_INIT_REG (0x02 << ST480_REG_DRR_SHIFT)

/*
 * Miscellaneous set.
 */
#define MAX_FAILURE_COUNT 3
#define ST480_DEFAULT_DELAY   30
#define ST480_AUTO_TEST 0
#define OLD_KERNEL_VERSION 0

struct platform_data_st480 {
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;
};

#endif

