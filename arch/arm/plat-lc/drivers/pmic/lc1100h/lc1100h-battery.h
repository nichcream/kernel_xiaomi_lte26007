
#ifndef __LC1100H_BATTERY_H__
#define __LC1100H_BATTERY_H__

#include <linux/power_supply.h>

#define BATT_VOLT_SAMPLE_NUM 28

/* useful macros */
#ifndef ARRAYSIZE
#define ARRAYSIZE(a)  (sizeof(a)/sizeof(a[0]))
#endif /* ARRAYSIZE */

typedef struct curve_sample{
	int cap;    /* capacity percent, Max is 100, Min is 0 */
	u16 adc_volt;   /* voltage, mV */
}curve_sample;

#define LCD_OFF_WAKEUP 200
#define LCD_ON_WAKEUP  201
typedef struct cap_curve{
	int cur;
	curve_sample table[BATT_VOLT_SAMPLE_NUM];
}cap_curve;

#warning "Use default battery curve.."
#include "../comip_battery_coolpad.h"


#define COMIP_BAT_DEBUG
#ifdef COMIP_BAT_DEBUG
#define BATPRTD(fmt, args...) printk("%s: " fmt, __FUNCTION__, ## args)
#else
#define BATPRTD(fmt, args...)
#endif
#define BATPRT(fmt, args...) printk("%s: " fmt, __FUNCTION__, ## args)

enum comip_battery_event_id {
	COMIP_BATTERY_EVENT_CHG_PLUG			= 0x00000001,
	COMIP_BATTERY_EVENT_CHG_UNPLUG			= 0x00000002,
	COMIP_BATTERY_EVENT_BAT_PACKED 			= 0x00000004,
	COMIP_BATTERY_EVENT_BAT_UNPACKED 		= 0x00000008,
	COMIP_BATTERY_EVENT_CHARGE_COMPLETED 	= 0x00000010,
	COMIP_BATTERY_EVENT_BATTERY_LOW 		= 0x00000020,
	COMIP_BATTERY_EVENT_CHARGE_RESTART	 	= 0x00000040,
	COMIP_BATTERY_EVENT_UAER_WAKE		 	= 0x00000080,
	COMIP_BATTERY_EVENT_USB_CALLBACK		= 0x00000100,
	COMIP_BATTERY_EVENT_CHARGER_CALLBACK	= 0x00000200,
	COMIP_BATTERY_EVENT_COUNT				= 10
};

typedef enum {
	ADC_LDO_I,      //Charger LDO current
	ADC_CHG_I,      //Battery charge current
	ADC_DCHG_I,     //Battery discharge current
	ADC_BAT_V,      //BATT Voltage
	ADC_VDD_V,      //VDD Voltage
	ADC_VIN_CHG_V,  //VIN_CHG Voltage
	ADC_VCOIN_V,    //VCOIN Voltage
	ADC_TEMP,       //LP3925 junction temperature
	ADC_GPIO1,      //GPIO input 1
	ADC_GPIO2,      //GPIO input 2
	ADC_GPIO3,      //GPIO input 3
	ADC_GPIO4,      //GPIO input 4
	ADC_INPUT_MAX,
}comip_battery_adc_input;

#define COMIP_BATTERY_ADC_SUPPORT_CHG_I			(1 << ADC_CHG_I)
#define COMIP_BATTERY_ADC_SUPPORT_DCHG_I		(1 << ADC_DCHG_I)
#define COMIP_BATTERY_ADC_SUPPORT_BAT_V			(1 << ADC_BAT_V)
#define COMIP_BATTERY_ADC_SUPPORT_TEMP			(1 << ADC_TEMP)
#define COMIP_BATTERY_ADC_SUPPORT_VIN_CHG_V		(1 << ADC_VIN_CHG_V)

struct comip_battery_adc_request{
	u16 inputs;
	u16 adc_value[ADC_INPUT_MAX];
};

enum comip_charge_type {
	CHARGE_TYPE_UNKNOW = -1,
	CHARGE_TYPE_AC,
	CHARGE_TYPE_USB,
	CHARGE_TYPE_MAX,
};

enum comip_charging_state {
	CHARGING_STATE_UNKNOW = -1,
	CHARGING_STATE_CHARGING,
	CHARGING_STATE_DISCHARGING,
	CHARGING_STATE_MAX,
};

typedef enum{
	CHARGER_STATE_UNKNOW = -1,
	CHARGER_STATE_CHARGER_UNPLUG,
	CHARGER_STATE_CHARGER_PLUG,
	CHARGER_STATE_MAX,
}comip_charger_state;

enum {
	BATTERY_STATE_UNKNOW = -1,
	BATTERY_STATE_PACKED,
	BATTERY_STATE_UNPACKED,
	BATTERY_STATE_MAX,
};

struct comip_battery_hw_ops {
	const char* name;
	int (*probe)(void);
	int (*remove)(void);
	int (*reg_write)(u8 reg, u8 value);
	int (*reg_read)(u8 reg, u8 *value);
	u32 (*int2event)(u32 status);
	u32 adc_support_type;
	int (*charging_state_get)(void); /* Discharging or Charging */
	int (*charging_state_set)(int onoff); /* Sometimes user want stop charging,
											 like RF test or operated by menu */
	int (*charger_state_get)(void); /* Charger unplug or Charger plug */
	int (*charger_type_get)(void); /* AC or USB */
	int (*battery_state_get)(void); /* Packed or Unpacked */
	int (*adc_acquire)(struct comip_battery_adc_request* request);
	int (*routing_switch)(int onoff);
	int (*adc2voltage)(u16 data); /* Battery voltage */
	int (*adc2vbus)(u16 data); /* Charger voltage on VBus */
	int (*adc2charging)(u16 data); /* Charging current */
	int (*adc2discharging)(u16 data); /* Discharging current */
	int (*adc2temperature)(u16 data);
	/* Set charging current according different charger(wall charger or USB).
	   Return value:
	   Positive number: Current value
	   Negative number: Set failed
	   Zero: Set charging current finish */
	int (*set_charging_current)(int old, int new); 
};

#endif /* __LC1100H_BATTERY_H__ */

