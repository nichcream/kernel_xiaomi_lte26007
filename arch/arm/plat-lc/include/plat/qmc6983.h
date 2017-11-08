/****************************************************************************
*   This software is licensed under the terms of the GNU General Public License version 2,
*   as published by the Free Software Foundation, and may be copied, distributed, and
*   modified under those terms.
*   This program is distributed in the hope that it will be useful, but WITHOUT ANY 
*   WARRANTY; without even the implied warranty of MERCHANTABILITY or
*   FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
*   for more details.
*
*   Copyright (C) 2012 by QST(Shanghai XiRui Keji) Corporation 
****************************************************************************/

#ifndef __QMC6983_H__
#define __QMC6983_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define QMC6983_DEFAULT_DELAY 100
#define QMC6983_IOCTL_BASE 'm' 
/* The following define the IOCTL command values via the ioctl macros */
#define QMC6983_SET_MODE								_IOW(QMC6983_IOCTL_BASE, 0x01, int)
#define QMC6983_SET_RANGE								_IOW(QMC6983_IOCTL_BASE, 0x02, int)
#define QMC6983_READ_MAGN_XYZ						_IOR(QMC6983_IOCTL_BASE, 0x03, char *)
#define QMC6983_SET_OUTPUT_DATA_RATE		_IOR(QMC6983_IOCTL_BASE, 0x04, char *)
#define QMC6983_SELF_TEST								_IOWR(QMC6983_IOCTL_BASE, 0x05, char *)
#define QMC6983_SET_OVERSAMPLE_RATIO		_IOWR(QMC6983_IOCTL_BASE, 0x06, char *)

#define ECS_IOCTL_SET_AFLAG					_IOW(QMC6983_IOCTL_BASE, 0x07, short)
#define ECS_IOCTL_GET_AFLAG					_IOR(QMC6983_IOCTL_BASE, 0x08, short)
#define ECS_IOCTL_SET_MFLAG					_IOW(QMC6983_IOCTL_BASE, 0x09, short)
#define ECS_IOCTL_GET_MFLAG					_IOR(QMC6983_IOCTL_BASE, 0x10, short)
#define ECS_IOCTL_SET_OFLAG					_IOW(QMC6983_IOCTL_BASE, 0x11, short)
#define ECS_IOCTL_GET_OFLAG					_IOR(QMC6983_IOCTL_BASE, 0x12, short)
#define ECS_IOCTL_SET_YPR						_IOW(QMC6983_IOCTL_BASE, 0x13, int[12])
#define ECS_IOCTL_GET_OPEN_STATUS		_IOR(QMC6983_IOCTL_BASE, 0x14, int)
#define ECS_IOCTL_SET_DELAY					_IOW(QMC6983_IOCTL_BASE, 0x15, short)
#define ECS_IOCTL_GET_DELAY					_IOR(QMC6983_IOCTL_BASE, 0x16, short)
#define ECS_IOCTL_MSENSOR_ENABLE		_IOW(QMC6983_IOCTL_BASE, 0x17, short)

#define QMC6983_READ_ACC_XYZ	_IOR(QMC6983_IOCTL_BASE, 0x18, short[3])
#define QMC6983_READ_QMC_XYZ	_IOR(QMC6983_IOCTL_BASE, 0x19, short[4])
#define QMC6983_READ_ORI_XYZ	_IOR(QMC6983_IOCTL_BASE, 0x20, short[4])
#define QMC6983_READ_ALL_REG	_IOR(QMC6983_IOCTL_BASE, 0x21, short[10])



//increase part
#define QMC6983_READ_CONTROL_REGISTER_ONE _IOR(QMC6983_IOCTL_BASE, 0x22, char *)
#define QMC6983_READ_CONTROL_REGISTER_TWO _IOR(QMC6983_IOCTL_BASE, 0x23, char *)
#define QMC6983_READ_MODE_REGISTER      	   _IOR(QMC6983_IOCTL_BASE, 0x24, char *)
#define QMC6983_READ_DATAOUTPUT_REGISTER       _IOR(QMC6983_IOCTL_BASE, 0x25, char *)
#define QMC6983_READ_STATUS_REGISTER_ONE 	       _IOR(QMC6983_IOCTL_BASE, 0x26, char *)
#define QMC6983_READ_STATUS_REGISTER_TWO 	       _IOR(QMC6983_IOCTL_BASE, 0x27, char *)
#define QMC6983_READ_TEMPERATURE_OUTPUT_MSB_REGISTER _IOR(QMC6983_IOCTL_BASE, 0x28, char *)
#define QMC6983_READ_TEMPERATURE_OUTPUT_LSB_REGISTER _IOR(QMC6983_IOCTL_BASE, 0x29, char *)

/************************************************/
/* 	Magnetometer section defines	 	*/
/************************************************/

/* Magnetic Sensor Operating Mode */
#define QMC6983_STANDBY_MODE	0x00
#define QMC6983_CC_MODE			0x01
#define QMC6983_SELFTEST_MODE	0x02
#define QMC6983_RESERVE_MODE	0x03


/* Magnetometer output data rate  */
#define QMC6983_ODR_10		0x00	/* 0.75Hz output data rate */
#define QMC6983_ODR_50		0x01	/* 1.5Hz output data rate */
#define QMC6983_ODR_100		0x02	/* 3Hz output data rate */
#define QMC6983_ODR7_200	0x03	/* 7.5Hz output data rate */


/* Magnetometer full scale  */
#define QMC6983_RNG_2G		0x00
#define QMC6983_RNG_8G		0x01
#define QMC6983_RNG_12G		0x02
#define QMC6983_RNG_20G		0x03

#define RNG_2G		2
#define RNG_8G		8
#define RNG_12G		12
#define RNG_20G		20

/*data output rate HZ*/
#define DATA_OUTPUT_RATE_10HZ 	0x00
#define DATA_OUTPUT_RATE_50HZ 	0x01
#define DATA_OUTPUT_RATE_100HZ 	0x02
#define DATA_OUTPUT_RATE_200HZ 	0x03

/*oversample Ratio */
#define OVERSAMPLE_RATIO_512 	0x00
#define OVERSAMPLE_RATIO_256 	0x01
#define OVERSAMPLE_RATIO_128 	0x02
#define OVERSAMPLE_RATIO_64 	0x03

#ifdef __KERNEL__

struct QMC6983_platform_data {

	u8 h_range;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;
};
#endif /* __KERNEL__ */


#endif  /* __QMC6983_H__ */
