/*
* ST-Ericsson ETF driver
*
* Copyright (c) ST-Ericsson SA 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.

*/
/****************************************************************************
* dev_ioctl.h
*
* This file defines the ioctls called for character device
*
****************************************************************************/

#ifndef _DEV_IOCTL_H
#define _DEV_IOCTL_H

#include <linux/ioctl.h>
#define MAJOR_NUM 100

#define IOCTL_ST90TDS_ADAP_READ      _IOWR(MAJOR_NUM, 1, char*)
#define IOCTL_ST90TDS_ADAP_WRITE     _IOWR(MAJOR_NUM, 2, char*)
#define IOCTL_ST90TDS_REQ_CNF        _IOWR(MAJOR_NUM, 3, char*)
#define IOCTL_ST90TDS_AHA_REG_READ   _IOWR(MAJOR_NUM, 4, char*)
#define IOCTL_ST90TDS_AHA_REG_WRITE  _IOWR(MAJOR_NUM, 5, char*)
#define IOCTL_ST90TDS_CUSTOM_CMD     _IOWR(MAJOR_NUM, 6, char*)
#define IOCTL_ST90TDS_REQ_CNF_READ   _IOWR(MAJOR_NUM, 7, char*)
#define IOCTL_ST90TDS_REQ_CNF_SIZE   _IOWR(MAJOR_NUM, 8, char*)
#define IOCTL_ST90TDS_WRITE_CNF_SIZE _IOWR(MAJOR_NUM, 9, char*)
#define IOCTL_ST90TDS_ADAP_READ_SIZE _IOWR(MAJOR_NUM, 10, char*)
/*similarly define other ioctls*/
/* to detect chip version */
#define IOCTL_ST90TDS_CHIP_VERSION   _IOWR(MAJOR_NUM, 11, char*)

#define DEVICE_NAME "char_dev"

#endif /* _DEV_IOCTL_H */
