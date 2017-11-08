/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2011  LeadCoreTech Corp.
 *
 *	PURPOSE:			 This files contains the comip hardware plarform's rtc driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version 	 Date			 Author 		 Description
 *	 1.0.0		 2011-02-11 	  zouqiao		created
 *
 */

#ifndef __COMIP_SWITCH_H__
#define __COMIP_SWITCH_H__

#define COMIP_SWITCH_FLAG_PMIC					(0x80000000)
#define COMIP_SWITCH_FLAG_ACTIVE_LOW			(0x40000000)

#define COMIP_SWITCH_IS_PMIC(id)				(id & COMIP_SWITCH_FLAG_PMIC)
#define COMIP_SWITCH_IS_ACTIVE_LOW(id)			(id & COMIP_SWITCH_FLAG_ACTIVE_LOW)
#define COMIP_SWITCH_ID(id) 					(id & 0x0000ffff)

struct comip_switch_platform_data {
	unsigned int irq_gpio;
	const char *detect_name;
	const char *button_name;
	unsigned int detect_id;
	unsigned int button_id;
	int (*button_power)(int onoff);
};

struct comip_switch_hw_info {
	int (*comip_switch_isr)(int type);
	int irq_gpio;
};

struct comip_switch_ops {
	int (*hw_init)(struct comip_switch_hw_info *);
	int (*hw_deinit)(void);
	int (*jack_status_get)(int *);
	int (*button_status_get)(int *);
	int (*jack_int_mask)(int);
	int (*button_int_mask)(int);
	int (*button_enable)(int);
	int (*micbias_power)(int);
	int (*jack_active_level_set)(int);
	int (*button_active_level_set)(int);
	int (*button_type_get)(int *);
	int (*irq_handle_done)(int);
};

#define COMIP_SWITCH_INT_JACK			0x1
#define COMIP_SWITCH_INT_BUTTON			0x2

#define SWITCH_JACK_OUT					0x0
#define SWITCH_JACK_IN_MIC				0x1
#define SWITCH_JACK_IN_NOMIC			0x2

#define SWITCH_BUTTON_RELEASE			0x0
#define SWITCH_BUTTON_PRESS				0x1

#define SWITCH_KEY_INVAILD				0x0
#define SWITCH_KEY_HOOK					0x1
#define SWITCH_KEY_UP					0x2
#define SWITCH_KEY_DOWN					0x4

extern int comip_switch_register_ops(struct comip_switch_ops *ops);
extern int comip_switch_unregister_ops(void);
#endif /* __COMIP_SWITCH_H__ */
