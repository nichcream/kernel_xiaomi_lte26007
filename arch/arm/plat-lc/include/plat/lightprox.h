#ifndef __LIGHTPROX_H__
#define __LIGHTPROX__
/* Magic number for MPU Iocts */
#define LIGHT_IOCTL (0x82)
/* IOCTL commands for /dev/lights */

#define PROXIMITY_IOCTL (0x83)
/* IOCTL commands for /dev/proximity */

/*light and proximity  ioctl*/
#define LIGHT_SET_DELAY         _IOW(LIGHT_IOCTL, 0x1, unsigned long)
#define LIGHT_SET_ENABLE        _IOR(LIGHT_IOCTL, 0x2, unsigned long)
#define PROXIMITY_SET_DELAY     _IOR(PROXIMITY_IOCTL, 0x3,unsigned long)
#define PROXIMITY_SET_ENABLE    _IOW(PROXIMITY_IOCTL, 0x4, unsigned long) 

#define LIGHT_DEFAULT_DELAY            200   /* 200 ms */
#define LIGHT_MAX_DELAY                2000  /* 2000 ms */
#define LIGHT_MIN_DELAY 				100  /*100 ms*/

#define PROXIMITY_DEFAULT_DELAY           200   /* 200 ms */
#define PROXIMITY_MAX_DELAY                2000 /* 2000 ms */
#define PROXIMITY_MIN_DELAY 				100  /*100 ms*/

#endif

