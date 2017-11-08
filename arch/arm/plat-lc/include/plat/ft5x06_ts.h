#ifndef __LINUX_FT5X06_TS_H__
#define __LINUX_FT5X06_TS_H__

/* Dirver configure. */

#if defined(CONFIG_TOUCHSCREEN_FT5316_AUTO_UPG) || defined(CONFIG_TOUCHSCREEN_FT5406_AUTO_UPG)||\
        defined(CONFIG_TOUCHSCREEN_FT6206_AUTO_UPG) || defined(CONFIG_TOUCHSCREEN_FT5336_AUTO_UPG)
#define CFG_SUPPORT_AUTO_UPG			1
#else
#define CFG_SUPPORT_AUTO_UPG			0
#endif
#define CFG_SUPPORT_UPDATE_PROJECT_SETTING	0

#define FTS_CTL_IIC
#define FTS_APK_DEBUG
#define SYSFS_DEBUG

#define ft5x06_debug 				0
#define FT5X06_DEBUG

/* Touch key, HOME, SEARCH, RETURN etc. */
#ifdef CONFIG_SUPPORT_TOUCH_KEY
#define CFG_SUPPORT_TOUCH_KEY			1
#else
#define CFG_SUPPORT_TOUCH_KEY			0
#endif

#define CFG_MAX_TOUCH_POINTS			10

#define CFG_NUMOFKEYS				3
#define CFG_FTS_CTP_DRIVER_VERSION		"3.0"

#define SCREEN_MAX_X				720
#define SCREEN_MAX_Y				1280
#define SCREEN_VIRTUAL_Y			1020

#define PRESS_MAX				255
#define FT_PRESS				0x08

#define CFG_POINT_READ_BUF			(3 + 6 * (CFG_MAX_TOUCH_POINTS))

#define FT5X06_NAME				"ft5x06"

#define FT_MAX_ID				0x0F
#define FT_TOUCH_STEP				6
#define FT_TOUCH_X_H_POS			3
#define FT_TOUCH_X_L_POS			4
#define FT_TOUCH_Y_H_POS			5
#define FT_TOUCH_Y_L_POS			6
#define FT_TOUCH_EVENT_POS		        3
#define FT_TOUCH_ID_POS			        5

#define KEY_PRESS				1
#define KEY_RELEASE				0
#define ENABLE					1
#define DISABLE					0

#define CTP_CHARGER_DETECT
#define CHARGER_CONNECT         1
#define CHARGER_NONE            2

#define FT_TOUCH_DOWN           0
#define FT_TOUCH_UP             1
#define FT_TOUCH_CONTACT        2
#define VENDOR_BIEL             0x3B
#define VENDOR_O_FILM           0x51
#define VENDOR_GIS              0x8f
#define VENDOR_WINTEK           0x89
#define VENDOR_YILI		0x54

struct ft5x06_info {
	int (*reset)(struct device *);
	int (*power_i2c)(struct device *,unsigned int);
	int (*power_ic)(struct device *,unsigned int);
	int reset_gpio;
	int irq_gpio;
	int power_i2c_flag;
	int power_ic_flag;
	int vendorid_a;
	int vendorid_b;
	int max_scrx;
	int max_scry;
	int virtual_scry;
	int max_points;
};

struct Upgrade_Info {
	unsigned char  CHIP_ID;
	unsigned char FTS_NAME[20];
	unsigned char TPD_MAX_POINTS;
	unsigned short delay_aa;		/*delay of write FT_UPGRADE_AA */
	unsigned short delay_55;		/*delay of write FT_UPGRADE_55 */
	unsigned char upgrade_id_1;	/*upgrade id 1 */
	unsigned char upgrade_id_2;	/*upgrade id 2 */
	unsigned short delay_readid;	/*delay of read id */
	unsigned short delay_earse_flash; /*delay of earse flash*/
} ;

enum ft5x06_ts_regs {
	FT5X06_REG_THGROUP			= 0x80, /* touch threshold, related to sensitivity */
	FT5X06_REG_THPEAK			= 0x81,
	FT5X06_REG_THCAL			= 0x82,
	FT5X06_REG_THWATER			= 0x83,
	FT5X06_REG_THTEMP			= 0x84,
	FT5X06_REG_THDIFF			= 0x85,
	FT5X06_REG_CTRL				= 0x86,
	FT5X06_REG_TIMEENTERMONITOR		= 0x87,
	FT5X06_REG_PERIODACTIVE			= 0x88,      /* report rate */
	FT5X06_REG_PERIODMONITOR		= 0x89,
	FT5X06_REG_HEIGHT_B			= 0x8a,
	FT5X06_REG_MAX_FRAME			= 0x8b,
	FT5X06_REG_DIST_MOVE			= 0x8c,
	FT5X06_REG_DIST_POINT			= 0x8d,
	FT5X06_REG_FEG_FRAME			= 0x8e,
	FT5X06_REG_SINGLE_CLICK_OFFSET		= 0x8f,
	FT5X06_REG_DOUBLE_CLICK_TIME_MIN	= 0x90,
	FT5X06_REG_SINGLE_CLICK_TIME		= 0x91,
	FT5X06_REG_LEFT_RIGHT_OFFSET		= 0x92,
	FT5X06_REG_UP_DOWN_OFFSET		= 0x93,
	FT5X06_REG_DISTANCE_LEFT_RIGHT		= 0x94,
	FT5X06_REG_DISTANCE_UP_DOWN		= 0x95,
	FT5X06_REG_ZOOM_DIS_SQR			= 0x96,
	FT5X06_REG_RADIAN_VALUE			= 0x97,
	FT5X06_REG_MAX_X_HIGH			= 0x98,
	FT5X06_REG_MAX_X_LOW			= 0x99,
	FT5X06_REG_MAX_Y_HIGH			= 0x9a,
	FT5X06_REG_MAX_Y_LOW			= 0x9b,
	FT5X06_REG_K_X_HIGH			= 0x9c,
	FT5X06_REG_K_X_LOW			= 0x9d,
	FT5X06_REG_K_Y_HIGH			= 0x9e,
	FT5X06_REG_K_Y_LOW			= 0x9f,
	FT5X06_REG_AUTO_CLB_MODE		= 0xa0,
	FT5X06_REG_LIB_VERSION_H		= 0xa1,
	FT5X06_REG_LIB_VERSION_L		= 0xa2,
	FT5X06_REG_CIPHER			= 0xa3,
	FT5X06_REG_MODE				= 0xa4,
	FT5X06_REG_PMODE			= 0xa5,	  /* Power Consume Mode		*/
	FT5X06_REG_FIRMID			= 0xa6,   /* Firmware version */
	FT5X06_REG_STATE			= 0xa7,
	FT5X06_REG_VENDOR_ID                    = 0xa8,
	FT5X06_REG_ERR				= 0xa9,
	FT5X06_REG_CLB				= 0xaa,
};

/* FT5X06_REG_PMODE. */
#define PMODE_ACTIVE	    	0x00
#define PMODE_MONITOR	    	0x01
#define PMODE_STANDBY	    	0x02
#define PMODE_HIBERNATE     	0x03

#ifdef CONFIG_TOUCHSCREEN_FTS_PROXIMITY
#define Proximity_Max	32
#define FT_FACE_DETECT_ON		0xc0
#define FT_FACE_DETECT_OFF		0xe0
#define FT_FACE_DETECT_ENABLE	1
#define FT_FACE_DETECT_DISABLE	0
#define FT_FACE_DETECT_REG		0xB0
#define FT_FACE_DETECT_POS		1
#endif

#ifndef ABS_MT_TOUCH_MAJOR
#define ABS_MT_TOUCH_MAJOR	0x30	/* touching ellipse */
#define ABS_MT_TOUCH_MINOR	0x31	/* (omit if circular) */
#define ABS_MT_WIDTH_MAJOR	0x32	/* approaching ellipse */
#define ABS_MT_WIDTH_MINOR	0x33	/* (omit if circular) */
#define ABS_MT_ORIENTATION	0x34	/* Ellipse orientation */
#define ABS_MT_POSITION_X	0x35	/* Center X ellipse position */
#define ABS_MT_POSITION_Y	0x36	/* Center Y ellipse position */
#define ABS_MT_TOOL_TYPE	0x37	/* Type of touching device */
#define ABS_MT_BLOB_ID		0x38	/* Group set of pkts as blob */
#endif /* ABS_MT_TOUCH_MAJOR */

#ifndef ABS_MT_TRACKING_ID
#define ABS_MT_TRACKING_ID 	0x39 /* Unique ID of initiated contact */
#endif

#endif

