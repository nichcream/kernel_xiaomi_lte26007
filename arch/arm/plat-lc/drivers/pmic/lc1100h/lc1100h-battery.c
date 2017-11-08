#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>
#include <linux/usb/ch9.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <asm/io.h>

#include <plat/clock.h>
#include <plat/bootinfo.h>
#include <plat/comip-pmic.h>
#include <plat/lc1100h.h>

#include "lc1100h-battery.h"

//#define LC1100H_BATTERY_DEBUG
#ifdef LC1100H_BATTERY_DEBUG
#define BATTERY_PRINT(fmt, args...) printk(KERN_ERR "[LC1100H-BATTERY]" fmt, ##args)
#else
#define BATTERY_PRINT(fmt, args...) printk(KERN_DEBUG "[LC1100H-BATTERY]" fmt, ##args)
#endif

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock batt_wakelock;
static struct wake_lock charger_wakelock;
static struct wake_lock usb_insert_wakelock;
#endif

/* the interval to update the all parameters, unit is ms */
#define COMIP_BATTERY_PARAM_UPDATE_INTERVAL 5000
#define AVG_COUNT 20  //sample times

enum {
	DEFAULT,
	AC_CHARGE_MODE,
	USB_CHARGE_MODE,
};

struct comip_battery_param {
    int volt;     //voltage, unit is mV
    int cur;      //current, unit is mA
    int temp;     //temperation
    int tech;     //technology
    int cap;      //capacity
    int present;  //present
    int health;   //health
    int status;   //status
};

struct comip_charger_param {
    enum comip_charge_type type;
    int ichg;    //current used to charge, unit is mA
    int volt;    //voltage used to charge, unit is mV
    int online;  //whether charge is pluged
};

struct comip_run_state {
	int state;
	__kernel_time_t timeval;
	void *pri_data;
};

struct wake_up_reason {
	int reason;
};

enum {
	WAKE_NORMAL = 0,
	WAKE_LCD_ON = 1,
	WAKE_BATT_LOW = 2,
	WAKE_BATT_FULL = 3,
};

enum {
	LCD_ONOFF,
	WIFI_ONOFF,
	SUSPENDED,
};

enum {
	COMIP_BATTERY_DISCHARGING,
	COMIP_BATTERY_CHARGING,
	COMIP_BATTERY_BAT_UNPACKED,
	COMIP_BATTERY_CHARGE_COMPLETED,
	COMIP_BATTERY_STATE_COUNT
};

#define SYSTEM_STATE_LCD_ON		(1 << LCD_ONOFF)
#define SYSTEM_WIFI_ONOFF		(1 << WIFI_ONOFF)
#define SYSTEM_SUSPENED			(1 << SUSPENDED)

#define USB_CHARGING_CURRENT		450
#define CHARGER_CHARGING_CURRENT	800
#define CHARGER_TYPE_DETECT_TIME	10

struct comip_battery_device_info {
	spinlock_t lock;
	struct semaphore sem;
	struct semaphore adc_sem;

	struct power_supply battery;
	struct power_supply usb;
	struct power_supply ac;

	struct comip_battery_param bat_param;
	struct comip_charger_param charger_param;

	struct delayed_work battery_work;

	int update_interval;
	struct comip_run_state current_state;
	struct comip_run_state old_state;
	struct comip_run_state old_cap;
	struct wake_up_reason wake_up_reason;
	u32 system_state; /* It will effect select different current curve */
	u32 sample_count;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	struct hrtimer charger_detect_timer;
	struct pmic_usb_event_parm usb_event_parm;
};

static struct comip_battery_device_info *g_di = NULL;

static int lc1100h_charging_current_set(int val)
{
	int ret = 0;
	int hex = val / 100;

	if ((val > 1200) || (hex <= 0)) {
		BATTERY_PRINT("Over current limit or parameter error\n");
		return -EINVAL;
	}

	hex -= 1;

	BATTERY_PRINT("Set charging current to %dmA(0x%02x)\n", val, hex);
	ret = lc1100h_reg_bit_write(LC1100H_REG_CHGCSET,
					LC1100H_REG_BITMASK_ACCSEL, hex);

	return ret;
}

static int usb_detect_handle(void)
{
	if (g_di) {
		hrtimer_cancel(&g_di->charger_detect_timer);
		g_di->charger_param.type = CHARGE_TYPE_USB;
		power_supply_changed(&g_di->usb);
		//lc1100h_charging_current_set(USB_CHARGING_CURRENT);
		BATTERY_PRINT("USB charger detect\n");
	}

	return 0;
}

static enum hrtimer_restart lc1100h_charger_detect_timer_func(struct hrtimer *timer)
{
	struct comip_battery_device_info *di =
		container_of(timer, struct comip_battery_device_info, charger_detect_timer);

	di->charger_param.type = CHARGE_TYPE_AC;
	power_supply_changed(&di->ac);
	//lc1100h_charging_current_set(CHARGER_CHARGING_CURRENT);
	BATTERY_PRINT("AC charger detect\n");

	return HRTIMER_NORESTART;
}

static int curve_get_sample(u16 vbat, curve_sample *table)
{
	int i;
	int cap;

	if (vbat >= table[0].adc_volt) {
		cap = table[0].cap;
	} else if (vbat <= table[BATT_VOLT_SAMPLE_NUM - 1].adc_volt) {
		cap = table[BATT_VOLT_SAMPLE_NUM - 1].cap;
	} else {
		for (i = 1; i <= BATT_VOLT_SAMPLE_NUM - 2; i++) {
			if (vbat >= table[i].adc_volt) {
				break;
			}
		}
		/* linear interpolation */
		cap = (table[i - 1].cap - table[i].cap) * (vbat - table[i].adc_volt) / (table[i - 1].adc_volt - table[i].adc_volt) + table[i].cap;
	}
	return cap;
}

static int cap_calculate(struct comip_battery_device_info *di,
                         u16 vbat, u16 ibat, struct cap_curve *curve, int curve_num)
{
	int i = 0;
	int cur1, cur2;
	int cap1, cap2;
	int cap;
	long snap=0;

	struct timeval  _now;
	struct comip_run_state *cap_history = &di->old_cap;
	int cur = ibat;

	do_gettimeofday(&_now);

	if (cur <= curve[0].cur) {
		cap = curve_get_sample(vbat, curve[0].table);
	} else if (cur >= curve[curve_num - 1].cur) {
		cap = curve_get_sample(vbat, curve[curve_num - 1].table);
	} else {
		for (i = 1; i <= curve_num - 2; i++){
			if (cur <= curve[i].cur) {
				break;
			}
		}
		cur1 = curve[i - 1].cur;
		cap1 = curve_get_sample(vbat, curve[i - 1].table);
		cur2 = curve[i].cur;
		cap2 = curve_get_sample(vbat, curve[i].table);
		/* linear interpolation */
		cap = (cap2 - cap1) * (cur - cur1) / (cur2 - cur1) + cap1;
	}

	if (di->wake_up_reason.reason == WAKE_NORMAL) {
		if ((cap - cap_history->state) < -2 && cap_history->state != 0) {
			cap = cap_history->state - 3;
		} else if ((cap -cap_history->state) > 2  &&  cap_history->state != 0) {
			cap = cap_history->state + 3;
		}
		if (((di->bat_param.status == POWER_SUPPLY_STATUS_DISCHARGING) &&
			(cap > cap_history->state)) && cap_history->state != 0) {
			cap = cap_history->state;
		}
	} else if (di->wake_up_reason.reason == WAKE_LCD_ON) {
		if (cap_history->timeval != 0 || cap_history->state != 0) {
			snap = _now.tv_sec - cap_history->timeval;
			if(snap < 60)
				goto out2;
			if (di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING) {
				if ((cap - cap_history->state) > (snap/60))
					cap = cap_history->state + (snap/60);
				else if(cap_history->state > cap)
					goto out1;					
			} else {
				if ((cap_history->state - cap)> (snap/40))
					cap = cap_history->state -(snap/40);
				else if (cap_history->state < cap )
					goto out1;

			}
		}
	} else if (di->wake_up_reason.reason == WAKE_BATT_LOW
			|| di->wake_up_reason.reason == WAKE_BATT_FULL) {
		if (di->wake_up_reason.reason == WAKE_BATT_FULL)
			cap = 99;
	}
	cap_history->state = ((cap>0) ? cap : 0);
out1:
	cap_history->timeval = _now.tv_sec;
out2:
	return cap_history->state;
}

static unsigned int lc1100h_battery_real_voltage(struct comip_battery_device_info *di)
{
	unsigned int voltage = 0;
	u16 adc;
	u16 i;

	for (i = 0; i < 5; i++) {
		lc1100h_read_adc(LC1100H_ADC_VBATT, &adc);
		voltage += (int)(((int)adc * 2800 * 4) / 4096);
	}

	return (voltage / 5);
}

static int lc1100h_battery_do_bat_calculate(struct comip_battery_device_info *di)
{
	unsigned int real_bat_voltage = 0;

	if (di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING) {
		/* Handle charging mode */
		real_bat_voltage = 
			lc1100h_battery_real_voltage(di);
		di->bat_param.volt = real_bat_voltage;
	} else {
		/* Handle discharging mode */
		real_bat_voltage = 
			lc1100h_battery_real_voltage(di);
		di->bat_param.volt = real_bat_voltage;
	}

	/* Now we get all parameter, try figure out capacitance */
	if (di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING) {
		di->bat_param.cap = cap_calculate(di, real_bat_voltage, 400, charge_curve, ARRAYSIZE(charge_curve));
	} else {
		/* Calculate battery capacity on discharging mode */
		if (di->system_state & SYSTEM_STATE_LCD_ON) {
			di->bat_param.cap = cap_calculate(di, real_bat_voltage, LCD_ON_WAKEUP, discharge_curve, ARRAYSIZE(discharge_curve));
		} else {
			di->bat_param.cap = cap_calculate(di, real_bat_voltage, LCD_OFF_WAKEUP, discharge_curve, ARRAYSIZE(discharge_curve));
		}
	}

	di->sample_count++;
	if (!(di->sample_count % AVG_COUNT))
		BATTERY_PRINT("Charger online(%d), battery voltage(%d), cap(%d)\n",
			di->charger_param.online, di->bat_param.volt, di->bat_param.cap);

	return 0;
}

static int lc1100h_battery_charger_state_get(void)
{
	u8 state = 0;
	lc1100h_reg_read(LC1100H_REG_PCST, &state);
	return state & LC1100H_REG_BITMASK_ADPIN ? 
			CHARGER_STATE_CHARGER_PLUG : CHARGER_STATE_CHARGER_UNPLUG;
}

static int lc1100h_battery_charging_state_get(void)
{
	u8 state = 0;

	lc1100h_reg_read(LC1100H_REG_PCST, &state);
	if (state & LC1100H_REG_BITMASK_ADPIN)
		return CHARGING_STATE_CHARGING;

	lc1100h_reg_read(LC1100H_REG_CHGCR, &state);
	if (state & LC1100H_REG_BITMASK_ACHGON)
		return CHARGING_STATE_CHARGING;

	return CHARGING_STATE_UNKNOW;
}


static void lc1100h_battery_work(struct work_struct *work)
{
	struct comip_battery_device_info *di
		= container_of((struct delayed_work *)work, struct comip_battery_device_info, battery_work);
	int old_cap = 0;

	if (di->system_state & SYSTEM_SUSPENED)
		return;

	old_cap = di->bat_param.cap;
	lc1100h_battery_do_bat_calculate(di);
	if (di->bat_param.cap != old_cap) {
		old_cap = di->bat_param.cap;
		power_supply_changed(&di->battery);
	}

	schedule_delayed_work(&di->battery_work, msecs_to_jiffies(di->update_interval));
}

static void lc1100h_battery_int_event(int event, void *puser, void* pargs)
{
	unsigned int int_status = *((unsigned int *)pargs);
	struct comip_battery_device_info *di = 
					(struct comip_battery_device_info *)puser;
	struct timeval now;

	if (int_status & LC1100H_INT_ADPOUTIR) {
		BATTERY_PRINT("Charger plug out\n");
		hrtimer_cancel(&di->charger_detect_timer);
		di->usb_event_parm.plug = 0;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		di->current_state.state = COMIP_BATTERY_DISCHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->charger_param.online = 0;
		di->charger_param.type = CHARGE_TYPE_UNKNOW;
		di->charger_param.volt = 0;
		di->charger_param.ichg = 0;

		power_supply_changed(&di->battery);
		power_supply_changed(&di->usb);
		power_supply_changed(&di->ac);
		schedule_delayed_work(&di->battery_work, 0);
		lc1100h_int_mask(LC1100H_INT_RCHGIR | LC1100H_INT_CHGCPIR | LC1100H_INT_RTIMIR);
	}

	if (int_status & LC1100H_INT_ADPINIR) {
		BATTERY_PRINT("Charger plug in\n");
		di->current_state.state = COMIP_BATTERY_CHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;
		di->charger_param.type = CHARGE_TYPE_UNKNOW;
		di->charger_param.online = 1;

#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_timeout(&usb_insert_wakelock, 15 * HZ);
#endif
		di->usb_event_parm.plug = 1;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		power_supply_changed(&di->battery);
		hrtimer_start(&di->charger_detect_timer, 
				ktime_set(CHARGER_TYPE_DETECT_TIME, 0), HRTIMER_MODE_REL);
		lc1100h_int_unmask(LC1100H_INT_RCHGIR | LC1100H_INT_CHGCPIR | LC1100H_INT_RTIMIR);
	}

	if (int_status & LC1100H_INT_RCHGIR) {
		BATTERY_PRINT("Restart charge\n");
		di->current_state.state = COMIP_BATTERY_CHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;

		power_supply_changed(&di->battery);
	}

	if (int_status & (LC1100H_INT_CHGCPIR | LC1100H_INT_RTIMIR)) {
		BATTERY_PRINT("End of charge\n");
		di->current_state.state = COMIP_BATTERY_CHARGE_COMPLETED;
		di->bat_param.status = POWER_SUPPLY_STATUS_FULL;
		di->bat_param.cap = 100;
		di->old_cap.state = 100;
		do_gettimeofday(&now);
		di->old_cap.timeval = now.tv_sec;
		power_supply_changed(&di->battery);
	}

	if (int_status & LC1100H_INT_BATUVIR) {
		BATTERY_PRINT("Battery low\n");
	}
}

static enum power_supply_property lc1100h_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property lc1100h_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property lc1100h_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int lc1100h_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct comip_battery_device_info *di = 
				container_of(psy, struct comip_battery_device_info, battery);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = di->bat_param.status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = di->bat_param.volt;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = di->bat_param.cur;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = di->bat_param.temp;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = di->bat_param.health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->bat_param.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = di->bat_param.tech;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = di->bat_param.cap;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int lc1100h_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct comip_battery_device_info *di = 
				container_of(psy, struct comip_battery_device_info, usb);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (di->charger_param.online) && (di->charger_param.type == CHARGE_TYPE_USB);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (di->charger_param.type == CHARGE_TYPE_USB){
			val->intval = di->charger_param.volt;
		} else {
			val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (di->charger_param.type == CHARGE_TYPE_USB){
			val->intval = di->charger_param.ichg;
		} else {
			val->intval = 0;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int lc1100h_ac_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct comip_battery_device_info *di = 
					container_of(psy, struct comip_battery_device_info, ac);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = (di->charger_param.online) && (di->charger_param.type == CHARGE_TYPE_AC);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (di->charger_param.type == CHARGE_TYPE_AC){
			val->intval = di->charger_param.volt;
		} else {
			val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (di->charger_param.type == CHARGE_TYPE_AC){
			val->intval = di->charger_param.ichg;
		} else {
			val->intval = 0;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int lc1100h_battery_hw_init(void)
{
	int ret = 0;
	/* Init battery charger */
	/* Enable charging complete detect */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCR,
					LC1100H_REG_BITMASK_EOCEN, 0x01);
	/* Close charger if temperature abnormal */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCR,
					LC1100H_REG_BITMASK_NTCEN, 0x01);
	/* Auto protect charger */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCR,
					LC1100H_REG_BITMASK_CHGPROT, 0x01);
	/* Auto protect charger */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCR,
					LC1100H_REG_BITMASK_CHGPROT, 0x01);
	/* Enable battery temperature detect */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCR,
					LC1100H_REG_BITMASK_TMONEN, 0x01);
	/* Select battery type to Li-on */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCR,
					LC1100H_REG_BITMASK_BATSEL, 0x00);
	/* Enable USB charger */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCR,
					LC1100H_REG_BITMASK_ACHGON, 0x01);

	/* 00: 100mv    01: 150mv */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGVSET,
					LC1100H_REG_BITMASK_VHYSSEL, 0x01);
	/* 000: 4.15V    001: 4.10V    010: 4.05V    011: 4.00V
	   100: 3.95V    101: 3.90V    110: 3.85V
	*/
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGVSET,
					LC1100H_REG_BITMASK_VHYSSEL, 0x00);

	/* 00: 50ma    01: 100ma    10: 150ma    11: 200ma */
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCSET,
					LC1100H_REG_BITMASK_CCPC, 0x01);
	/* 0000: 100ma    0001: 200ma    0010: 300ma    0011: 400ma
	   0100: 500ma    0101: 600ma    0110: 700ma    0111: 800ma
	   1000: 900ma    1001: 1000ma   1010: 1100ma   1011: 1200ma
	*/
	ret |= lc1100h_reg_bit_write(LC1100H_REG_CHGCSET,
					LC1100H_REG_BITMASK_ACCSEL, 0x03);
	return ret;
}

static int lc1100h_battery_init_set(struct comip_battery_device_info *di)
{
	spin_lock_init(&di->lock);
	sema_init(&di->sem, 1);
	sema_init(&di->adc_sem, 1);
	di->update_interval = COMIP_BATTERY_PARAM_UPDATE_INTERVAL;
	di->sample_count = 0;

	di->bat_param.health = POWER_SUPPLY_HEALTH_GOOD;
	di->bat_param.tech = POWER_SUPPLY_TECHNOLOGY_LION;
	di->bat_param.present = 1;
	di->bat_param.cap=0;

	di->usb_event_parm.plug = 0;
	di->usb_event_parm.confirm = usb_detect_handle;

	INIT_DELAYED_WORK(&di->battery_work, lc1100h_battery_work);

	hrtimer_init(&di->charger_detect_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	di->charger_detect_timer.function = lc1100h_charger_detect_timer_func;

	if (lc1100h_battery_charger_state_get() == CHARGER_STATE_CHARGER_PLUG) {
		di->usb_event_parm.plug = 1;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		if (CHARGING_STATE_CHARGING == lc1100h_battery_charging_state_get()) {
			di->current_state.state = COMIP_BATTERY_CHARGING;
			di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			di->current_state.state = COMIP_BATTERY_CHARGE_COMPLETED;
			di->bat_param.status = POWER_SUPPLY_STATUS_FULL;
		}
		di->charger_param.type = CHARGE_TYPE_UNKNOW;
		di->charger_param.online = 1;
		hrtimer_start(&di->charger_detect_timer, 
				ktime_set(CHARGER_TYPE_DETECT_TIME, 0), HRTIMER_MODE_REL);
	} else {
		di->usb_event_parm.plug = 0;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		di->current_state.state = COMIP_BATTERY_DISCHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->charger_param.type = CHARGE_TYPE_UNKNOW;
		di->charger_param.online = 0;
	}

	/* For init voltage */
	di->old_cap.state = 0;
	di->old_cap.timeval = 0;
	schedule_delayed_work(&di->battery_work, 0);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lc1100h_battery_early_suspend(struct early_suspend *h)
{
	struct comip_battery_device_info *di
		= container_of(h, struct comip_battery_device_info, early_suspend);

	di->system_state &= ~SYSTEM_STATE_LCD_ON;
}

static void lc1100h_battery_early_resume(struct early_suspend *h)
{
	struct comip_battery_device_info *di
		= container_of(h, struct comip_battery_device_info, early_suspend);

	di->wake_up_reason.reason = WAKE_LCD_ON;
	di->system_state |= SYSTEM_STATE_LCD_ON;
	schedule_delayed_work(&di->battery_work, 0);
}
#endif

#ifdef CONFIG_PM
static int lc1100h_battery_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_battery_device_info *di = platform_get_drvdata(pdev);

	di->system_state |= SYSTEM_SUSPENED;
	cancel_delayed_work_sync(&di->battery_work);

	return 0;
}

static int lc1100h_battery_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_battery_device_info *di = platform_get_drvdata(pdev);

	di->system_state &= ~SYSTEM_SUSPENED;
	schedule_delayed_work(&di->battery_work, 0);

	return 0;
}

static struct dev_pm_ops lc1100h_battery_pm_ops = {
	.suspend = lc1100h_battery_suspend,
	.resume = lc1100h_battery_resume,
};
#endif

static int lc1100h_battery_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct comip_battery_device_info *di;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (di == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}

	g_di = di;

	platform_set_drvdata(pdev, di);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&batt_wakelock, WAKE_LOCK_SUSPEND, "batt_wakelock");
	wake_lock_init(&charger_wakelock, WAKE_LOCK_SUSPEND, "charger_wakelock");
	wake_lock_init(&usb_insert_wakelock, WAKE_LOCK_SUSPEND, "usb_insert_wakelock");
#endif

	lc1100h_battery_hw_init();

	di->battery.name = "battery";
	di->battery.properties = lc1100h_battery_props;
	di->battery.num_properties = ARRAY_SIZE(lc1100h_battery_props);
	di->battery.get_property = lc1100h_battery_get_property;
	di->battery.type = POWER_SUPPLY_TYPE_BATTERY;

	di->usb.name = "usb";
	di->usb.properties = lc1100h_usb_props;
	di->usb.num_properties = ARRAY_SIZE(lc1100h_usb_props);
	di->usb.get_property = lc1100h_usb_get_property;
	di->usb.type = POWER_SUPPLY_TYPE_USB;

	di->ac.name = "ac";
	di->ac.properties = lc1100h_ac_props;
	di->ac.num_properties = ARRAY_SIZE(lc1100h_ac_props);
	di->ac.get_property = lc1100h_ac_get_property;
	di->ac.type = POWER_SUPPLY_TYPE_MAINS;

	ret = power_supply_register(&pdev->dev, &di->battery);
	if (ret)
		goto err_battery_failed;

	ret = power_supply_register(&pdev->dev, &di->usb);
	if (ret)
		goto err_usb_failed;

	ret = power_supply_register(&pdev->dev, &di->ac);
	if (ret)
		goto err_ac_failed;

	lc1100h_battery_init_set(di);

	ret = pmic_callback_register(PMIC_EVENT_BATTERY, di, lc1100h_battery_int_event);
	if (ret) {
		BATPRT("Fail to register pmic event callback.\n");
		goto err_callback_register;
	}

	pmic_callback_unmask(PMIC_EVENT_BATTERY);

#ifdef CONFIG_HAS_EARLYSUSPEND
	di->early_suspend.level = 55;
	di->early_suspend.suspend = lc1100h_battery_early_suspend;
	di->early_suspend.resume = lc1100h_battery_early_resume;
	register_early_suspend(&di->early_suspend);
#endif

	return 0;

err_callback_register:
	power_supply_unregister(&di->ac);

err_ac_failed:
	power_supply_unregister(&di->usb);

err_usb_failed:
	power_supply_unregister(&di->battery);

err_battery_failed:
	kfree(di);
	platform_set_drvdata(pdev, NULL);

err_data_alloc_failed:
	return ret;
}

static int lc1100h_battery_remove(struct platform_device *pdev)
{
	struct comip_battery_device_info *di = platform_get_drvdata(pdev);

	power_supply_unregister(&di->battery);
	power_supply_unregister(&di->usb);
	power_supply_unregister(&di->ac);

	cancel_delayed_work_sync(&di->battery_work);

	kfree(di);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver lc1100h_battery_device = {
	.probe = lc1100h_battery_probe,
	.remove = lc1100h_battery_remove,
	.driver = {
		.name = "lc1100h-battery",
#ifdef CONFIG_PM
		.pm = &lc1100h_battery_pm_ops,
#endif
	}
};

static int __init lc1100h_battery_init(void)
{
	return platform_driver_register(&lc1100h_battery_device);
}

static void __exit lc1100h_battery_exit(void)
{
	platform_driver_unregister(&lc1100h_battery_device);
}

late_initcall(lc1100h_battery_init);
module_exit(lc1100h_battery_exit);

