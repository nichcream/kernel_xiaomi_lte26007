/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the pmic driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2012-03-06	zouqiao		created
 *
 */

#ifndef __COMIP_PMIC_H__
#define __COMIP_PMIC_H__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/spinlock_types.h>
#include <linux/notifier.h>

/* Special voltage value. */
#define PMIC_POWER_VOLTAGE_DISABLE		(0)
#define PMIC_POWER_VOLTAGE_ENABLE		(-1)
#define PMIC_POWER_VOLTAGE_MAX			(5000)

/* PMIC RTC Alarm ID. */
#define PMIC_RTC_ALARM_NORMAL			(0)
#define PMIC_RTC_ALARM_POWEROFF			(1)
#define PMIC_RTC_ALARM_REBOOT			(PMIC_RTC_ALARM_NORMAL)

/* PMIC Events. */
enum {
	PMIC_EVENT_POWER_KEY = 0,
	PMIC_EVENT_BATTERY,
	PMIC_EVENT_RTC1,
	PMIC_EVENT_RTC2,
	PMIC_EVENT_AUDIO,
	PMIC_EVENT_COMP1,
	PMIC_EVENT_COMP2,
	PMIC_EVENT_USB,
	PMIC_EVENT_ADC,
	PMIC_EVENT_CHARGER,
	PMIC_EVENT_MAX
};

enum {
	IRQ_POWER_KEY_RELEASE = 0,
	IRQ_POWER_KEY_PRESS,
	IRQ_POWER_KEY_PRESS_RELEASE
};

/* PMIC comparator id. */
enum {
	PMIC_COMP1 = 0,
	PMIC_COMP2
};

/* Power key state. */
enum {
	PMIC_POWER_KEY_OFF = 0,
	PMIC_POWER_KEY_ON
};

#define PMIC_DIRECT_POWER_ID(id)		((id) & 0x7f)
#define PMIC_IS_DIRECT_POWER_ID(id)		((id) & 0x80)
#define PMIC_TO_DIRECT_POWER_ID(id)		((id) | 0x80)

/* Power module. */
enum {
	PMIC_POWER_BATTERY = 0,
	PMIC_POWER_BLUETOOTH_CORE,
	PMIC_POWER_BLUETOOTH_IO,
	PMIC_POWER_CAMERA_CORE,
	PMIC_POWER_CAMERA_IO,
	PMIC_POWER_CAMERA_DIGITAL,
	PMIC_POWER_CAMERA_ANALOG,
	PMIC_POWER_CAMERA_CSI_PHY,
	PMIC_POWER_CAMERA_AF_MOTOR,
	PMIC_POWER_CMMB_BB_CORE,
	PMIC_POWER_CMMB_RF_CORE,
	PMIC_POWER_CMMB_IO,
	PMIC_POWER_FM,
	PMIC_POWER_GPS_CORE,
	PMIC_POWER_GPS_IO,
	PMIC_POWER_GPS_RF,
	PMIC_POWER_HDMI_CORE,
	PMIC_POWER_LCD_CORE,
	PMIC_POWER_LCD_IO,
	PMIC_POWER_LCD_ANALOG,
	PMIC_POWER_LED,
	PMIC_POWER_RF_GSM_CORE,
	PMIC_POWER_RF_GSM_IO,
	PMIC_POWER_RF_TDD_CORE,
	PMIC_POWER_RF_TDD_IO,
	PMIC_POWER_SDIO,
	PMIC_POWER_SDRAM,
	PMIC_POWER_SWITCH,
	PMIC_POWER_TOUCHSCREEN,
	PMIC_POWER_TOUCHSCREEN_I2C,
	PMIC_POWER_TOUCHSCREEN_IO,
	PMIC_POWER_UART,
	PMIC_POWER_USB,
	#ifdef CONFIG_USB_COMIP_HSIC
	PMIC_POWER_USB_ETH,
	PMIC_POWER_USB_HSIC,
	#endif
	PMIC_POWER_USIM,
	PMIC_POWER_VIBRATOR,
	PMIC_POWER_WLAN_CORE,
	PMIC_POWER_WLAN_IO,
	PMIC_POWER_RTK_CORE,
	PMIC_MODULE_MAX
};

/* Power module parameter. */
/* PMIC_POWER_LED. */
enum {
	PMIC_LED_LCD = 0,
	PMIC_LED_KEYPAD,
	PMIC_LED_FLASH
};

/* Special power control values. */
#define PMIC_POWER_CTRL_GPIO_ID_NONE		(0xFFFFFFFF)

/* PMIC Power Control Selection. */
enum {
	PMIC_POWER_CTRL_ALWAYS_OFF,
	PMIC_POWER_CTRL_ALWAYS_ON,
	PMIC_POWER_CTRL_REG,
	PMIC_POWER_CTRL_GPIO,
	PMIC_POWER_CTRL_OSCEN,
	PMIC_POWER_CTRL_COSCEN,
	PMIC_POWER_CTRL_MAX
};

#define PMIC_IS_POWER(type, id)	\
	(((id) >= PMIC_##type##1) && ((id) <= (PMIC_##type##1 + PMIC_##type##_NUM)))

#define PMIC_IS_DCDC(id)	PMIC_IS_POWER(DCDC, id)
#define PMIC_IS_DLDO(id)	PMIC_IS_POWER(DLDO, id)
#define PMIC_IS_ALDO(id)	PMIC_IS_POWER(ALDO, id)
#define PMIC_IS_ISINK(id)	PMIC_IS_POWER(ISINK, id)
#define PMIC_IS_EXTERNAL(id)	PMIC_IS_POWER(EXTERNAL, id)

#define PMIC_DCDC_ID(id)	((id) - PMIC_DCDC1)
#define PMIC_DLDO_ID(id)	((id) - PMIC_DLDO1)
#define PMIC_ALDO_ID(id)	((id) - PMIC_ALDO1)
#define PMIC_ISINK_ID(id)	((id) - PMIC_ISINK1)
#define PMIC_EXTERNAL_ID(id)	((id) - PMIC_EXTERNAL1)

/* PMIC power index. */
enum {
	PMIC_DCDC1 = 0,
	PMIC_DCDC2,
	PMIC_DCDC3,
	PMIC_DCDC4,
	PMIC_DCDC5,
	PMIC_DCDC6,
	PMIC_DCDC7,
	PMIC_DCDC8,
	PMIC_DCDC9,
	PMIC_DCDC10,
	PMIC_DLDO1 = 16,
	PMIC_DLDO2,
	PMIC_DLDO3,
	PMIC_DLDO4,
	PMIC_DLDO5,
	PMIC_DLDO6,
	PMIC_DLDO7,
	PMIC_DLDO8,
	PMIC_DLDO9,
	PMIC_DLDO10,
	PMIC_DLDO11,
	PMIC_DLDO12,
	PMIC_DLDO13,
	PMIC_DLDO14,
	PMIC_DLDO15,
	PMIC_DLDO16,
	PMIC_ALDO1 = 48,
	PMIC_ALDO2,
	PMIC_ALDO3,
	PMIC_ALDO4,
	PMIC_ALDO5,
	PMIC_ALDO6,
	PMIC_ALDO7,
	PMIC_ALDO8,
	PMIC_ALDO9,
	PMIC_ALDO10,
	PMIC_ALDO11,
	PMIC_ALDO12,
	PMIC_ALDO13,
	PMIC_ALDO14,
	PMIC_ALDO15,
	PMIC_ALDO16,
	PMIC_ISINK1 = 80,
	PMIC_ISINK2,
	PMIC_ISINK3,
	PMIC_ISINK4,
	PMIC_ISINK5,
	PMIC_ISINK6,
	PMIC_ISINK7,
	PMIC_ISINK8,
	PMIC_EXTERNAL1 = 96,
	PMIC_EXTERNAL2,
	PMIC_POWER_MAX,
};

/* PMIC registers struct. */
struct pmic_reg_st {
	u16 reg;
	u16 value;
	u16 mask;
};

/* PMIC power control map. */
struct pmic_power_ctrl_map {
	u8 power_id;
	u8 ctrl;
	int gpio_id;
	int default_mv;
	int state;
};

/* PMIC power module map. */
struct pmic_power_module_map {
	u8 power_id;
	u8 module;
	u8 param;
	u8 operation;
};

/* PMIC ops. */
struct pmic_ops {
	/* Name. */
	const char *name;

	/* Registers. */
	int (*reg_write)(u16 reg, u16 val);
	int (*reg_read)(u16 reg, u16 *pval);

	/* Event. */
	int (*event_mask)(int event);
	int (*event_unmask)(int event);

	/* Power. */
	int (*voltage_get)(u8 module, u8 param, int *pmv);
	int (*voltage_set)(u8 module, u8 param, int mv);

	/* RTC. */
	int (*rtc_time_get)(struct rtc_time *tm);
	int (*rtc_time_set)(struct rtc_time *tm);
	int (*rtc_alarm_get)(u8 id, struct rtc_wkalrm *alrm);
	int (*rtc_alarm_set)(u8 id, struct rtc_wkalrm *alrm);

	/* Comparator. */
	int (*comp_polarity_set)(u8 id, u8 pol);
	int (*comp_state_get)(u8 id);

	/* Reboot type. */
	int (*reboot_type_set)(u8 type);

	/* Power key. */
	int (*power_key_state_get)(void);

	/* vchg/vbus state. */
	int (*vchg_input_state_get)(void);

	/* Control set */
	int (*power_ctrl_set)(u8 module, u8 param, u8 ctrl);
	/* Callback list. */
	struct list_head list;

	/* Spinlock for callback list. */
	spinlock_t cb_lock;

	/* Ext-power list. */
	struct list_head ep_list;

	/* Spinlock for ext-power list. */
	spinlock_t ep_lock;
};

/* PMIC callback. */
struct pmic_callback {
	unsigned long event;					/* The event which callback care about. */
	void *puser;						/* The user information which callback care about. */
	void (*func)(int event, void *puser, void* pargs);	/* Callback function. */
	struct list_head list;
};

/* PMIC callback. */
struct pmic_ext_power {
	u8 module;
	int (*voltage_get)(u8 module, u8 param, int *pmv);
	int (*voltage_set)(u8 module, u8 param, int mv);
	struct list_head list;
};

/* PMIC usb event parameter. */
struct pmic_usb_event_parm {
	int plug;
	int (*confirm)(void);
};

#ifdef CONFIG_COMIP_PMIC

/* Called by PMIC device driver. */
extern int pmic_ops_set(struct pmic_ops *ops);
extern int pmic_event_handle(int event, void* pargs);

/* Called by external power drivers. */
extern int pmic_ext_power_register(struct pmic_ext_power *ep);
extern int pmic_ext_power_unregister(struct pmic_ext_power *ep);

/* Called by others drivers. */
extern const char *pmic_name_get(void);
extern int pmic_callback_register(int event, void *puser, void (*func)(int event, void *puser, void* pargs));
extern int pmic_callback_unregister(int event);
extern int pmic_callback_mask(int event);
extern int pmic_callback_unmask(int event);
extern int pmic_reg_write(u16 reg, u16 val);
extern int pmic_reg_read(u16 reg, u16 *pval);
extern int pmic_voltage_get(u8 module, u8 param, int *pmv);
extern int pmic_voltage_set(u8 module, u8 param, int mv);
extern int pmic_rtc_time_get(struct rtc_time *tm);
extern int pmic_rtc_time_set(struct rtc_time *tm);
extern int pmic_rtc_alarm_get(u8 id, struct rtc_wkalrm *alrm);
extern int pmic_rtc_alarm_set(u8 id, struct rtc_wkalrm *alrm);
extern int pmic_comp_polarity_set(u8 id, u8 pol);
extern int pmic_comp_state_get(u8 id);
extern int pmic_reboot_type_set(u8 type);
extern int pmic_power_key_state_get(void);
extern int pmic_vchg_input_state_get(void);
extern int pmic_power_ctrl_set(u8 module, u8 param, u8 ctrl);

#else /*CONFIG_COMIP_PMIC*/

/* Called by PMIC device driver. */
static inline int pmic_ops_set(struct pmic_ops *ops){return 0;}
static inline int pmic_event_handle(int event, void* pargs){return 0;}

/* Called by external power drivers. */
static inline int pmic_ext_power_register(struct pmic_ext_power *ep){return 0;}
static inline int pmic_ext_power_unregister(struct pmic_ext_power *ep){return 0;}

/* Called by others drivers. */
static inline const char *pmic_name_get(void){return NULL;}
static inline int pmic_callback_register(int event, void *puser, void (*func)(int event, void *puser, void* pargs)){return 0;}
static inline int pmic_callback_unregister(int event){return 0;}
static inline int pmic_callback_mask(int event){return 0;}
static inline int pmic_callback_unmask(int event){return 0;}
static inline int pmic_reg_write(u16 reg, u16 val){return 0;}
static inline int pmic_reg_read(u16 reg, u16 *pval){return 0;}
static inline int pmic_voltage_get(u8 module, u8 param, int *pmv){return 0;}
static inline int pmic_voltage_set(u8 module, u8 param, int mv){return 0;}
static inline int pmic_rtc_time_get(struct rtc_time *tm){return 0;}
static inline int pmic_rtc_time_set(struct rtc_time *tm){return 0;}
static inline int pmic_rtc_alarm_get(u8 id, struct rtc_wkalrm *alrm){return 0;}
static inline int pmic_rtc_alarm_set(u8 id, struct rtc_wkalrm *alrm){return 0;}
static inline int pmic_comp_polarity_set(u8 id, u8 pol){return 0;}
static inline int pmic_comp_state_get(u8 id){return 0;}
static inline int pmic_reboot_type_set(u8 type){return 0;}
static inline int pmic_power_key_state_get(void){return 0;}
static inline int pmic_vchg_input_state_get(void){return 0;}
static inline int pmic_power_ctrl_set(u8 module, u8 param, u8 ctrl){return 0;}

#endif /*CONFIG_COMIP_PMIC*/

/*charger or usb status event*/
enum lcusb_xceiv_events {
    LCUSB_EVENT_NONE,        /* no events or cable disconnected */
    LCUSB_EVENT_PLUGIN,      /* vbus power on */
    LCUSB_EVENT_VBUS,        /* vbus valid event */
    LCUSB_EVENT_CHARGER,     /* usb dedicated charger */
    LCUSB_EVENT_OTG,         /* id was grounded/otg event*/
    LCUSB_EVENT_ENUMERATED,  /* gadget driver enumerated */
};

struct lcusb_transceiver {
    struct device  *dev;
    enum lcusb_xceiv_events last_event;
};

extern enum lcusb_xceiv_events comip_get_plugin_device_status(void);
extern int lcusb_set_transceiver(struct lcusb_transceiver *);

#if defined(CONFIG_COMIP_PMIC)
extern struct lcusb_transceiver *lcusb_get_transceiver(void);
extern void lcusb_put_transceiver(struct lcusb_transceiver *);
int comip_usb_register_notifier(struct notifier_block *nb);
int comip_usb_unregister_notifier(struct notifier_block *nb);
extern int comip_usb_xceiv_notify(enum lcusb_xceiv_events status);
#else
static inline struct lcusb_transceiver *lcusb_get_transceiver(void){return NULL;}
static inline void lcusb_put_transceiver(struct lcusb_transceiver *x){}
static inline int comip_usb_register_notifier(struct notifier_block *nb){return -EINVAL;}
static inline int comip_usb_unregister_notifier(struct notifier_block *nb){return -EINVAL;}
static inline int comip_usb_xceiv_notify(enum lcusb_xceiv_events status){return -EINVAL;}
#endif

#endif/*__COMIP_PMIC_H__*/

