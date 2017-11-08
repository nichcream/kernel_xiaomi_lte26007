/* drivers/input/misc/alps_compass_d_io.h
 *
 * I/O controll header for alps sensor
 *
 * Copyright (C) 2011-2014 ALPS ELECTRIC CO., LTD.
 *
 */

#ifndef ___ALPS_D_IO_H_INCLUDED
#define ___ALPS_D_IO_H_INCLUDED

#include <linux/ioctl.h>

#define ALPSIO   0xAF

/* IOCTLs for SensoeService */
#define ALPSIO_SS_SET_ACCACTIVATE	_IOW(ALPSIO, 0x10, int)
#define ALPSIO_SS_SET_MAGACTIVATE	_IOW(ALPSIO, 0x11, int)
#define ALPSIO_SS_SET_ORIACTIVATE	_IOW(ALPSIO, 0x12, int)
#define ALPSIO_SS_SET_GYRACTIVATE	_IOW(ALPSIO, 0x13, int)
#define ALPSIO_SS_SET_LINACTIVATE	_IOW(ALPSIO, 0x14, int)
#define ALPSIO_SS_SET_ROTACTIVATE	_IOW(ALPSIO, 0x15, int)
#define ALPSIO_SS_SET_DELAY		_IOW(ALPSIO, 0x16, int)
#define ALPSIO_SS_GET_HWDATA		_IOR(ALPSIO, 0x17, int[3])
#define ALPSIO_SS_SET_ACCVALUE		_IOW(ALPSIO, 0x18, int[3])

/* IOCTLs for AcdApiDaemon */
#define ALPSIO_AD_GET_ACTIVATE		_IOR(ALPSIO, 0x20, int)
#define ALPSIO_AD_GET_DELAY		_IOR(ALPSIO, 0x21, int)
#define ALPSIO_AD_GET_DATA		_IOR(ALPSIO, 0x22, int[10])
#define ALPSIO_AD_SET_DATA		_IOW(ALPSIO, 0x23, int[29])
#define ALPSIO_AD_EXE_SELF_TEST_A	_IOR(ALPSIO, 0x24, int)
#define ALPSIO_AD_EXE_SELF_TEST_B	_IOR(ALPSIO, 0x25, int)
#define ALPSIO_AD_GET_HWDATA		_IOR(ALPSIO, 0x26, int[3])

enum {
	ACTIVE_SS_NUL = 0x00 ,
	ACTIVE_SS_ACC = 0x01 ,
	ACTIVE_SS_MAG = 0x02 ,
	ACTIVE_SS_ORI = 0x04 ,
	ACTIVE_SS_GYR = 0x08 ,
	ACTIVE_SS_LIN = 0x10 ,
	ACTIVE_SS_ROT = 0x20 ,
};

struct TAIFD_HW_DATA {
	int activate;
	int delay;
	int acc[4];
	int mag[4];
};

struct TAIFD_SW_DATA {
	int acc[5];
	int mag[5];
	int ori[5];
	int gyr[5];
	int lin[4];
	int rot[5];
};


#endif

