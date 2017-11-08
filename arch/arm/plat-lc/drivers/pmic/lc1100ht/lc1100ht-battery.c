#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
//#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>
#include <linux/usb/ch9.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <asm/io.h>

#include <plat/clock.h>
#include <plat/bootinfo.h>
#include <plat/comip-pmic.h>
#include <plat/lc1100ht.h>

#include "lc1100ht-battery.h"

#define __lc1100ht_reg_write	lc1100ht_reg_write
#define __lc1100ht_reg_read	lc1100ht_reg_read

enum {
	LC1100HT_CHG_STATE_CHG_OFF = 0x0,
	LC1100HT_CHG_STATE_TRANSITIONAL,
	LC1100HT_CHG_STATE_LOW_INPUT_STATE = 0x3,
	LC1100HT_CHG_STATE_PRECHARGE,
	LC1100HT_CHG_STATE_CHARGE_CC,
	LC1100HT_CHG_STATE_CHARGE_CV,
	LC1100HT_CHG_STATE_MAINTENANCE,
	LC1100HT_CHG_STATE_BATTERY_FAULT,
	LC1100HT_CHG_STATE_SYSTEM_SUPPORT = 0xc,
	LC1100HT_CHG_STATE_HIGH_CURRENT = 0xf
};

static inline int lc1100ht_bitmask2bitstart(u8 bitmask)
{
	int start = 0;
	if (bitmask == 0) {
		printk("Why bit mask is zero !!!\n");
		return -EINVAL;
	}
	while (!(bitmask & 0x01)) {
		bitmask >>= 1;
		start++;
	}
	return start;
}

#ifdef CONFIG_HAS_WAKELOCK
static struct wake_lock batt_wakelock;
static struct wake_lock charger_wakelock;
static struct wake_lock usb_insert_wakelock;
#endif

/* the interval to update the all parameters, unit is ms */
#define COMIP_BATTERY_PARAM_UPDATE_INTERVAL 5000
#define AVG_COUNT 20  //sample times

static int charging_index = 0;	//initialize lfor soft interrupt count

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

#define DBG_SETTING_SPECIAL_MODE		1
#define DBG_SETTING_NORMAL_MODE			0
struct comip_battery_dbg_setting {
	int stop; /* Sometimes user hope device not do charging, like do RF test */
	int mode; /* For test mode, like consume all current, without low battery 
			     voltage auto power off */
	int type; /* For debug use. Sometomes user want use specify type charger
	             like, In actually plug charger USB, but user want to use
	             AC charger mode */
};

struct comip_run_state {
	int state;
	__kernel_time_t timeval;
	void *pri_data;
};

#define MAX_TRACE 4
struct comip_trace_state {
	struct comip_run_state least_cap;
	struct comip_run_state vol[MAX_TRACE];
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
	INIT_POINT,
};

#define SYSTEM_STATE_LCD_ON	(1 << LCD_ONOFF)
#define SYSTEM_WIFI_ONOFF	(1 << WIFI_ONOFF)
#define SYSTEM_SUSPENED		(1 << SUSPENDED)
#define SYSTEM_INIT_POINT	(1 << INIT_POINT)

struct wake_up_reason {
	int reason;
};

struct comip_battery_curve {
	int w_index;
	int terminal_cnt;
	u16 vbat[AVG_COUNT];
	u16 ibat[AVG_COUNT];
	u16 temp[AVG_COUNT];
	u16 vchg[AVG_COUNT];
	u16 ichg[AVG_COUNT];
};

struct comip_current_set_request {
	int in_progress;
	int old;
	int new;
	int step;
	int interval;
	struct mutex lock;
};

struct comip_battery_event {
    enum comip_battery_event_id id;
    struct list_head list; /* For send to thread */
	struct list_head event_buf; /* For malloc and free event */
	int dummy[8];
};
#define COMIP_BATTERY_MAX_EVENT_CNT	64

#define INIT_CHARGING_CURRENT		100
#define USB_CHARGING_CURRENT		450
#define CHARGER_CHARGING_CURRENT	600
#define CHARGER_TYPE_DETECT_TIME	10

struct comip_battery_device_info {
    spinlock_t lock;
    struct semaphore sem;
	struct semaphore adc_sem;
    wait_queue_head_t wait;
    struct list_head event_q;

    struct power_supply battery;
    struct power_supply usb;
    struct power_supply ac;

    struct comip_battery_param bat_param;
    struct comip_charger_param charger_param;
	struct comip_trace_state trace;

    struct task_struct	*monitor_thread;
	struct delayed_work	 set_current_work;
	struct comip_current_set_request current_work_req;

    int update_interval;
	struct comip_battery_curve curve;
	struct comip_run_state current_state;
	struct comip_run_state old_state;
	struct comip_run_state old_cap;
	struct wake_up_reason wake_up_reason;
	u32 system_state; /* It will effect select different current curve */

	struct comip_battery_hw_ops *hw_ops;

	struct comip_battery_dbg_setting dbg_setting;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	spinlock_t event_lock;
	struct list_head free_event;
	struct comip_battery_event event_buf[COMIP_BATTERY_MAX_EVENT_CNT];
	struct hrtimer charger_detect_timer;
	struct pmic_usb_event_parm usb_event_parm;
};

enum comip_battery_state_id{
    COMIP_BATTERY_DISCHARGING,
    COMIP_BATTERY_CHARGING,
    COMIP_BATTERY_BAT_UNPACKED,
    COMIP_BATTERY_CHARGE_COMPLETED,
    COMIP_BATTERY_STATE_COUNT
};

struct comip_battery_state {
    const char *name;
    enum comip_battery_state_id id;
    int (*handle)(struct comip_battery_device_info*, struct comip_battery_event *event);
    int valid_events;
};

struct vct_request {
	/* Voltage/Current/Temperature */
	int cnt; /* Voltage & Current hits */
	int interval; /* If cnt with one, this parameter will be ignore */
};

static int comip_battery_probe(struct platform_device *pdev);
static int usb_detect_handle(void);
int comip_battery_late_resume(struct platform_device *pdev);
int comip_battery_late_suspend(struct platform_device *pdev, pm_message_t state);
static int comip_battery_remove(struct platform_device *pdev);
static struct comip_battery_event *
	comip_battery_recv_event(struct comip_battery_device_info *di);
static int comip_battery_schedule_current_req(int target, 
	struct comip_battery_device_info *di);
static int 
	comip_battery_current_req_kill(struct comip_battery_device_info *di);
static struct comip_battery_event* 
	comip_battery_get_event(struct comip_battery_device_info *di);
static void comip_battery_free_event(struct comip_battery_event* event, 
	struct comip_battery_device_info *di);
static int comip_battery_send_event(struct comip_battery_device_info *di,
		enum comip_battery_event_id id, void *user_data, int user_data_size);
static int comip_hand_plug_debouce(int plug_mode);

static enum comip_charge_type comip_battery_charger_type_detect(struct comip_battery_device_info *di)
{
	return di->hw_ops->charger_type_get ? 
				di->hw_ops->charger_type_get() : CHARGE_TYPE_UNKNOW;
}

static enum comip_charging_state charging_detect(struct comip_battery_device_info *di)
{
	return di->hw_ops->charging_state_get ? 
				di->hw_ops->charging_state_get() : CHARGING_STATE_UNKNOW;
}

static int handle_discharging(struct comip_battery_device_info *di, struct comip_battery_event *event)
{
	int current_value = 0;
	struct timeval  _now;

	switch (event->id) {
	case COMIP_BATTERY_EVENT_CHG_PLUG:
		printk("detect charging\n");
		if (di->bat_param.cap >= 100) {
			di->current_state.state = COMIP_BATTERY_CHARGE_COMPLETED;
			di->bat_param.status = POWER_SUPPLY_STATUS_FULL;
			di->bat_param.cap = 100;
			di->old_cap.state = 100;
			do_gettimeofday(&_now);
			di->old_cap.timeval = _now.tv_sec;
		} else {
			di->current_state.state = COMIP_BATTERY_CHARGING;
			if (di->dbg_setting.stop == DBG_SETTING_SPECIAL_MODE) {
				di->bat_param.status = POWER_SUPPLY_STATUS_DISCHARGING;
				printk("Detect charger plug in, but user want stop charging\n");
			} else {
				di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;
			}
		}
		di->charger_param.online = 1;
		di->charger_param.type = comip_battery_charger_type_detect(di);

		#ifdef CONFIG_HAS_WAKELOCK
		wake_lock_timeout(&usb_insert_wakelock, 15* HZ);
		printk("=====usb_insert_wakelock \n");
		#endif

		// power_routing_switch_off();
		power_supply_changed(&di->battery);
		di->usb_event_parm.plug = 1;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		if (di->charger_param.type == CHARGE_TYPE_USB) {
			power_supply_changed(&di->usb);
			current_value = USB_CHARGING_CURRENT;
			comip_battery_schedule_current_req(current_value, di);
		} else if (di->charger_param.type == CHARGE_TYPE_AC) {
			power_supply_changed(&di->ac);
			current_value = CHARGER_CHARGING_CURRENT;
			comip_battery_schedule_current_req(current_value, di);
		} else {
			printk("%s: Start charger type detect timer.\n", __func__);
			hrtimer_start(&di->charger_detect_timer, 
							ktime_set(CHARGER_TYPE_DETECT_TIME, 0), HRTIMER_MODE_REL);
		}
// 		queue_delayed_work(battery_charge_wq, &set_charge_current_work, 0*HZ);
//		queue_delayed_work(battery_charge_wq, &charger_detect_work, 10*HZ);
		break;
	case COMIP_BATTERY_EVENT_BATTERY_LOW:
		printk("Get battery low event(%d)!\n", *(int *)event->dummy);
		break;
	default:
		BATPRT("Event not supported.\n");
	}

	return 0;
}

static int handle_charging(struct comip_battery_device_info *di, struct comip_battery_event *event)
{
	struct timeval  _now;
	int current_value = 0;
	static int cnt = 0;

	switch (event->id) {
	case COMIP_BATTERY_EVENT_CHG_PLUG:
		if (di->current_work_req.new >= 200) {
			BATPRT("Charger plug in but send plug in event!back the current 100ma\n");
			current_value = di->current_work_req.new - 100; /* if Charger plug in but send plug in event, back the current 100ma*/
			comip_battery_schedule_current_req(current_value, di);
		}
		cnt++;
		if (cnt >= 15) {
			BATPRT("Charger plug in but send plug in event!start_plug_debounce_function\n");
			di->hw_ops->start_plug_debounce(comip_hand_plug_debouce);
		}
		break;

	case COMIP_BATTERY_EVENT_CHG_UNPLUG:
		printk("%s: Cancel charger type detect timer.\n", __func__);
		cnt = 0;
		hrtimer_cancel(&di->charger_detect_timer);
		di->usb_event_parm.plug = 0;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		di->current_state.state = COMIP_BATTERY_DISCHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->charger_param.online = 0;
		di->charger_param.type = CHARGE_TYPE_UNKNOW;
		di->charger_param.volt = 0;
		di->charger_param.ichg = 0;
		charging_index = 0;

		power_supply_changed(&di->battery);
		power_supply_changed(&di->usb);
		power_supply_changed(&di->ac);

		comip_battery_current_req_kill(di);
//		cancel_delayed_work(&set_charge_current_work);
//		cancel_delayed_work(&switch_on_work);
//		cancel_delayed_work(&charger_detect_work);

		//1TODO:
		/* Battery Charging Current Register */
		//lp3925_reg_write(LP3925_REG_CHARGING_CURRENT, 0X2);// 01000, IBATT = 100mA
		//lp3925_reg_write(LP3925_REG_INPUT_CURRENT_LIMIT, 0xa);//

//		AC_OR_USB_USER=0;
//		AC_OR_USB_KERNEL=0;
		break;

	case COMIP_BATTERY_EVENT_BAT_UNPACKED:
		di->current_state.state = COMIP_BATTERY_BAT_UNPACKED;
		di->bat_param.present = 0;
		di->bat_param.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		di->bat_param.volt = 0;
		di->bat_param.cur = 0;
		di->bat_param.temp = 0;

		power_supply_changed(&di->usb);
		break;

	case COMIP_BATTERY_EVENT_CHARGE_COMPLETED:
		di->current_state.state = COMIP_BATTERY_CHARGE_COMPLETED;
		di->bat_param.status = POWER_SUPPLY_STATUS_FULL;
		di->bat_param.cap = 100;
		di->old_cap.state = 100;
		do_gettimeofday(&_now);
		di->old_cap.timeval = _now.tv_sec;
		power_supply_changed(&di->battery);
		break;

	case COMIP_BATTERY_EVENT_CHARGER_CALLBACK:
		di->charger_param.type = CHARGE_TYPE_AC;
		power_supply_changed(&di->ac);
		comip_battery_schedule_current_req(CHARGER_CHARGING_CURRENT, di);
		break;

	case COMIP_BATTERY_EVENT_USB_CALLBACK:
		printk("%s: Cancel charger type detect timer.\n", __func__);
		hrtimer_cancel(&di->charger_detect_timer);
		di->charger_param.type = CHARGE_TYPE_USB;
		power_supply_changed(&di->usb);
		comip_battery_schedule_current_req(USB_CHARGING_CURRENT, di);
		break;

	default:
		BATPRT("Event not supported.\n");
	}

	return 0;
}

static int handle_battery_unpacked(struct comip_battery_device_info *di, struct comip_battery_event *event)
{
	switch (event->id) {
	case COMIP_BATTERY_EVENT_BAT_PACKED:
		if (CHARGING_STATE_CHARGING == charging_detect(di)) {
			di->current_state.state = COMIP_BATTERY_CHARGING;
			di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;
//			power_routing_switch_off();
		} else {
			di->current_state.state = COMIP_BATTERY_CHARGE_COMPLETED;
			di->bat_param.status = POWER_SUPPLY_STATUS_FULL;//1TODO:  How dispose charger plug in but not in charing mode
		}
		di->bat_param.present = 1;

		power_supply_changed(&di->battery);
		break;

	default:
		BATPRT("Event not supported.\n");
	}

	return 0;
}

static int handle_charge_completed(struct comip_battery_device_info *di, struct comip_battery_event *event)
{
	int current_value = 0;
	static int cnt = 0;
	struct timeval  _now;

	switch (event->id) {
	case COMIP_BATTERY_EVENT_CHG_PLUG:
		if (di->current_work_req.new >= 200) {
			BATPRT("Charger plug in but send plug in event!back the current 100ma\n");
			current_value = di->current_work_req.new - 100; /* if Charger plug in but send plug in event, back the current 100ma*/
			comip_battery_schedule_current_req(current_value, di);
		}
		cnt++;
		if (cnt >= 15) {
			BATPRT("Charger plug in but send plug in event!start_plug_debounce_function\n");
			di->hw_ops->start_plug_debounce(comip_hand_plug_debouce);
		}
		break;

	case COMIP_BATTERY_EVENT_CHG_UNPLUG:
		printk("%s: Cancel charger detect timer.\n", __func__);
		cnt = 0;
		hrtimer_cancel(&di->charger_detect_timer);
		di->usb_event_parm.plug = 0;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		di->current_state.state = COMIP_BATTERY_DISCHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->charger_param.online = 0;
		di->charger_param.type = CHARGE_TYPE_UNKNOW;
		di->charger_param.volt = 0;
		di->charger_param.ichg = 0;
		charging_index = 0;

		power_supply_changed(&di->battery);
		power_supply_changed(&di->usb);
		power_supply_changed(&di->ac);

//		AC_OR_USB_USER=0;
//		AC_OR_USB_KERNEL=0;
		break;

	case COMIP_BATTERY_EVENT_BAT_UNPACKED:
		di->current_state.state = COMIP_BATTERY_BAT_UNPACKED;
		di->bat_param.present = 0;
		di->bat_param.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		di->bat_param.volt = 0;
		di->bat_param.cur = 0;
		di->bat_param.temp = 0;

		power_supply_changed(&di->battery);
		break;

	case COMIP_BATTERY_EVENT_CHARGE_RESTART:
		di->current_state.state = COMIP_BATTERY_CHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;

//		power_routing_switch_off();
		power_supply_changed(&di->battery);
		break;

	case COMIP_BATTERY_EVENT_BATTERY_LOW:
		printk("Get battery low event(%d)!\n", *(int*)event->dummy);
		break;

	case COMIP_BATTERY_EVENT_CHARGE_COMPLETED:
		di->current_state.state = COMIP_BATTERY_CHARGE_COMPLETED;
		di->bat_param.status = POWER_SUPPLY_STATUS_FULL;
		di->bat_param.cap = 100;
		di->old_cap.state = 100;
		do_gettimeofday(&_now);
		di->old_cap.timeval = _now.tv_sec;
		power_supply_changed(&di->battery);
		break;

	case COMIP_BATTERY_EVENT_CHARGER_CALLBACK:
		di->charger_param.type = CHARGE_TYPE_AC;
		power_supply_changed(&di->ac);
		comip_battery_schedule_current_req(CHARGER_CHARGING_CURRENT, di);
		break;

	case COMIP_BATTERY_EVENT_USB_CALLBACK:
		printk("%s: Cancel charger type detect timer.\n", __func__);
		hrtimer_cancel(&di->charger_detect_timer);
		di->charger_param.type = CHARGE_TYPE_USB;
		power_supply_changed(&di->usb);
		comip_battery_schedule_current_req(USB_CHARGING_CURRENT, di);
		break;

	default:
		BATPRT("Event not supported.\n");
	}

	return 0;
}


/* the table used to descript all charge state */
static struct comip_battery_state state_table[COMIP_BATTERY_STATE_COUNT] = {
	{
	.name = "Discharging",
	.id = COMIP_BATTERY_DISCHARGING,
	.handle = handle_discharging,
	.valid_events = COMIP_BATTERY_EVENT_CHG_PLUG |
		COMIP_BATTERY_EVENT_BATTERY_LOW,
	},
	{
	.name = "Charging",
	.id = COMIP_BATTERY_CHARGING,
	.handle = handle_charging,
	.valid_events = COMIP_BATTERY_EVENT_CHG_UNPLUG |
		COMIP_BATTERY_EVENT_BAT_UNPACKED |
		COMIP_BATTERY_EVENT_CHARGE_COMPLETED |
		COMIP_BATTERY_EVENT_CHARGER_CALLBACK |
		COMIP_BATTERY_EVENT_USB_CALLBACK |
		COMIP_BATTERY_EVENT_CHG_PLUG,
			//1TODO:
	},
	{
		.name = "Battery unpacked",
		.id = COMIP_BATTERY_BAT_UNPACKED,
		.handle = handle_battery_unpacked,
		.valid_events = COMIP_BATTERY_EVENT_BAT_PACKED,
	},
	{
		.name = "Charge completed",
		.id = COMIP_BATTERY_CHARGE_COMPLETED,
		.handle = handle_charge_completed,
		.valid_events = COMIP_BATTERY_EVENT_CHG_UNPLUG |
			COMIP_BATTERY_EVENT_BAT_UNPACKED |
			COMIP_BATTERY_EVENT_CHARGE_COMPLETED |
			COMIP_BATTERY_EVENT_CHARGE_RESTART |
			COMIP_BATTERY_EVENT_BATTERY_LOW |
			COMIP_BATTERY_EVENT_USB_CALLBACK |
			COMIP_BATTERY_EVENT_CHARGER_CALLBACK |
			COMIP_BATTERY_EVENT_CHG_PLUG,
	},
};

static struct comip_battery_device_info *g_di;

static int battery_adc_acquire
	(struct comip_battery_device_info *di, struct comip_battery_adc_request *req)
{
	int ret = 0;

	if (di->hw_ops->adc_acquire) {
		down(&di->adc_sem);
		ret = di->hw_ops->adc_acquire(req);
		up(&di->adc_sem);
	} else {
		printk("Battery device not support adc acquire?!?!\n");
		ret = 0;
	}
	return ret;
}

static inline int is_charging_mode(int mode)
{
	if (mode == POWER_SUPPLY_STATUS_CHARGING)
		return 1;
	if (mode == POWER_SUPPLY_STATUS_FULL)
		return 1;
	return 0;
}

static int comip_battery_vct_request
		(struct comip_battery_device_info *di, struct vct_request *req)
{
	struct comip_battery_adc_request adc_req;
	struct comip_battery_curve *curve;
	int ret = 0, cnt;

	if (!di->bat_param.present)
		return -EINVAL;
	if (di->system_state & SYSTEM_SUSPENED) {
		printk("%s: suspended return.", __func__);
		return 0;
	}

	memset(&adc_req, 0, sizeof(struct comip_battery_adc_request));

	if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_BAT_V)
		adc_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_BAT_V;
	if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_DCHG_I)
		adc_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_DCHG_I;
	if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_TEMP)
		adc_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_TEMP;

	if (is_charging_mode(di->bat_param.status)) {
		if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_VIN_CHG_V)
			adc_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_VIN_CHG_V; /* Charger voltage */
		if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_CHG_I)
			adc_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_CHG_I; /* Charger current */
	}

	cnt = req->cnt;
	curve = &di->curve;
	while (cnt) {
		ret = battery_adc_acquire(di, &adc_req);
		if (ret < 0)
			return -1;
		BATPRTD("Get battery ADC(%d), Writer index(%d), Requset(%d), Terminal(%d)\n", 
			adc_req.adc_value[ADC_BAT_V], curve->w_index, cnt, curve->terminal_cnt);
		curve->vbat[curve->w_index] = adc_req.adc_value[ADC_BAT_V];
		curve->ibat[curve->w_index] = adc_req.adc_value[ADC_DCHG_I];
		curve->temp[curve->w_index] = adc_req.adc_value[ADC_TEMP];
		if (is_charging_mode(di->bat_param.status)) {
			curve->vchg[curve->w_index] = adc_req.adc_value[ADC_VIN_CHG_V];
			curve->ichg[curve->w_index] = adc_req.adc_value[ADC_CHG_I];
		}
		curve->w_index++;
		cnt--;
		if (cnt) {
			if (di->system_state & SYSTEM_INIT_POINT) {
				mdelay(req->interval);
			} else {
				msleep(req->interval);
			}
		}
	}
	return ret < 0 ? ret : req->cnt;
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

static int g_test_cap = 0;

static const char* test_msg[15 + 1] = {
	[0] = "Charger off",
	[1] = "Transitional - warmup",
	[3] = "Low input state",
	[4] = "Precharge",
	[5] = "Charger cc",
	[6] = "Charger cv",
	[7] = "Charger Maintenance",
	[8] = "Battery fault",
	[12] = "System support",
	[15] = "High current",
};

static void charger_info(struct comip_battery_device_info *di, unsigned int real_bat_vlotage)
{
	u8 val = 0;
	struct rtc_time tm;
	struct timeval  now;

	/* Debug only */
	__lc1100ht_reg_read(0x07, &val);
	BATTERY_LC1100HT_PRINTKA("lc1100ht test 0x%02x", val);
	val &= LC1100HT_REG_BITMASK_CHG_STATE;
	val >>= 2;
	BATTERY_LC1100HT_PRINTKA(":0x%02x\n", val);
	if (val < 0x0f)
		BATTERY_LC1100HT_PRINTKA("  %s\n", test_msg[val]);


	do_gettimeofday(&now);
	rtc_time_to_tm(now.tv_sec, &tm);

	if (di->bat_param.status == POWER_SUPPLY_STATUS_DISCHARGING ||
		di->bat_param.status == POWER_SUPPLY_STATUS_NOT_CHARGING)
		printk("[Battery] (%d-%02d-%02d %02d:%02d:%02d UTC)"
		" update battery cap to %d,"
		" battery voltage = %d,"
		" avg battery discharge current = %d\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		di->bat_param.cap,di->bat_param.volt, di->bat_param.cur);
	else
		printk("[Battery] (%d-%02d-%02d %02d:%02d:%02d UTC)"
			" update battery cap to %d,"
			" battery voltage = %d,"
			" curv = %d, ichg=%d\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			di->bat_param.cap,di->bat_param.volt,real_bat_vlotage, di->charger_param.ichg);//
			BATPRTD("adc_vbat=%d, adc_ichg=%d, cap=%d, cov_vbat=%d,cov_ibat=%d,cov_temp=%d\n",
			real_bat_vlotage, di->charger_param.ichg, di->bat_param.cap, di->bat_param.volt, di->bat_param.cur, di->bat_param.temp);
		/* End debug message */
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

	BATPRTD("Reason: %d, Cap: %d\n", di->wake_up_reason.reason, cap);
	if (di->wake_up_reason.reason == WAKE_NORMAL) {
		BATPRTD("Handle wakeup reason is normal\n");
		if ((cap - cap_history->state) < -2 && cap_history->state != 0) {
			cap = cap_history->state - 3;
		} else if ((cap -cap_history->state) > 2  &&  cap_history->state != 0) {
			cap = cap_history->state + 3;
		}//这里对cap的修正值，是基于上报周期决定的。现在是150s上报一次。

		if (((di->bat_param.status == POWER_SUPPLY_STATUS_DISCHARGING) &&
			(cap > cap_history->state)) && cap_history->state != 0) {
			printk("%s: Charger plug out but battery voltage increase[%d:%d]\n", 
				__func__, cap_history->state, cap);
			cap = cap_history->state;
		} else if ((di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING ||
					di->bat_param.status == POWER_SUPPLY_STATUS_FULL) &&
					(cap < cap_history->state) && cap_history->state != 0) {
			printk("%s: Charger plug in but battery voltage decrease[%d:%d]\n", 
				__func__, cap_history->state, cap);
			cap = cap_history->state;
		}
	} else if (di->wake_up_reason.reason == WAKE_LCD_ON) {
		//monitor_wakeup.up_params_flag = 0;
		//power_routing_switch_off();
		if (cap_history->timeval != 0 || cap_history->state != 0) {
			BATPRTD("Handle wakeup reason is lcd on\n");
			snap = _now.tv_sec - cap_history->timeval;
			BATPRTD("update cap now! cap_history.old_cap = %d cap=%d snap=%ld\n",
					cap_history->state, cap, snap);
			if(snap<60)
				goto out2;
			if (di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING ||
				di->bat_param.status == POWER_SUPPLY_STATUS_FULL) {
				BATPRTD("Handle charging mode\n");
				if ((cap - cap_history->state) > (snap/60))
					cap = cap_history->state + (snap/60);
				else if(cap_history->state > cap)
					goto out1;					
			} else {
				BATPRTD("Handle discharging mode\n");
				if ((cap_history->state - cap)> (snap/40))
					cap = cap_history->state -(snap/40);
				else if (cap_history->state < cap )
					goto out1;

			}//防止不符合逻辑的跳变
		}else {
			BATPRT("First run?\n");
		}
	} else if (di->wake_up_reason.reason == WAKE_BATT_LOW 
						|| di->wake_up_reason.reason == WAKE_BATT_FULL){ 
		printk("Get battery %s with %d:%d\n", 
			di->wake_up_reason.reason == WAKE_BATT_LOW ? "low" : "full",
			cap, cap_history->state);
		if (di->wake_up_reason.reason == WAKE_BATT_FULL) {
			cap = 100;
		}
	}
	cap_history->state = ((cap > 0) ? (cap > 100 ? 100 : cap ): 0);
	BATPRTD("Set cap history to %d\n", cap_history->state);
out1:
	cap_history->timeval = _now.tv_sec;
out2:
	/* Test mode code add here */
	if (g_test_cap) {
		printk("%s: test mode.\n", __func__);
		return g_test_cap;
	}
	return cap_history->state;
}

static int adc_battery_temp(u16 data)
{
	return g_di->hw_ops->adc2temperature(data);
}

static int g_vbatt[4] = {4200,4200,3600,3600};
static int adc_battery_voltage(u16 data)
{
	int temp;
	int temp_k = 1000;
	int temp_b = 0;
	int vbatt;

	/* unit is mV */
	temp = g_di->hw_ops->adc2voltage(data);
	temp_k = 1000 * (g_vbatt[0] - g_vbatt[2]) / (g_vbatt[1] - g_vbatt[3]);
	temp_b = 1000 * g_vbatt[0] - temp_k * g_vbatt[1];
	vbatt = (temp_k * temp + temp_b) / 1000;

	return vbatt;
}

static int adc_battery_voltage_charging(u16 data)
{
	/* unit is mA */
	return g_di->hw_ops->adc2charging(data);
}

static int adc_battery_voltage_discharging(u16 data)
{
	/* unit is mA */
	return g_di->hw_ops->adc2discharging(data);
}
#if 0
static int adc_battery_voltage_charging_complete(u16 data)
{
	/* unit is mV */
	//return (int)(data * 4800 / 4096);   //use 4096 to replace 4095 for the optimization
	//1 Need according different hardware board
	return (int)(data * 4800 /4149+65);   //fumlar for leadcore 1809
}
#endif
static u16 average(u16 *p, int num)
{
	int i =0;
	u16 max, min;
	unsigned int sum = 0;
	u16 temp = 0;

	if (num <= 0)
		return 0;
	else if (num == 1)
		return *p;
	else if (num == 2)
		return (*p + *(p + 1)) / 2;
	else {
		max = min = *p;
		for (i = 0; i < num; i++) {
			temp = *(p + i);
			sum += temp;
			if (temp > max)
				max = temp;
			if (temp < min)
				min = temp;
		}

		/* discard the max and min */
		sum -= (max + min);

		return sum / (num - 2);
	}
}

static unsigned int  real_voltage_calculate(u16 bat_adc_voltage[], const int count)
{
	int loop_num = 0;
	int tmp_filled_num = count;
	unsigned int precision = 100;
	unsigned int most_vol_num = 0, tmp_most_vol_num = 0, most_vol = 0;
	unsigned int tmp_vol=0,real_vol = 0;
	unsigned int relative_num = 0;

	//get most vol and num
	while (0 < tmp_filled_num--) {
		tmp_vol = (adc_battery_voltage(bat_adc_voltage[tmp_filled_num]) / precision) * precision;
		if ((tmp_vol < 2000) || (tmp_vol > 4500))
			continue;
		tmp_most_vol_num = 0;
		for (loop_num = 0; loop_num < count; loop_num++) {
			if((adc_battery_voltage(bat_adc_voltage[loop_num]) / precision) * precision == tmp_vol)
				tmp_most_vol_num++;
		}
		if (most_vol_num < tmp_most_vol_num) {
			most_vol_num = tmp_most_vol_num;
			most_vol = tmp_vol;
		}

		if(most_vol_num > (count/2))
			break;
	}

	for (loop_num = 0; loop_num < count; loop_num++) {
		tmp_vol = adc_battery_voltage(bat_adc_voltage[loop_num]);
		if ((tmp_vol/precision)*precision == most_vol ){
			real_vol += tmp_vol;
			relative_num++;
		}
	}
	if (relative_num == 0) {
		printk("%s: Why not have available data?\n", __func__);
		return 0;
	}
	real_vol = real_vol / relative_num;
	return real_vol;
}

static int comip_battery_do_bat_calculate(struct comip_battery_device_info *di)
{
	unsigned int real_bat_vlotage = 0;
	if (!di->bat_param.present) {
		printk("%s: Why handle battery unpacked!?\n", __func__);
		return -EINVAL;
	}

	if (di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING ||
		di->bat_param.status == POWER_SUPPLY_STATUS_FULL) {
		/* Handle charging mode */
		BATPRTD("Handle charging mode, try get real voltage(%d)\n",
				di->curve.w_index);
		real_bat_vlotage = 
				real_voltage_calculate(di->curve.vbat, di->curve.w_index);
		if (di->current_state.state == COMIP_BATTERY_CHARGE_COMPLETED) {
			di->bat_param.volt = real_bat_vlotage;
				//1TODO: Why?
				//adc_battery_voltage_charging_complete(average(di->curve.vbat, di->curve.w_index));
		} else {
			di->bat_param.volt = real_bat_vlotage;
				//1TODO: Why?
				//adc_battery_voltage_charging(average(di->curve.vbat, di->curve.w_index));
		}

		if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_CHG_I) {
			di->charger_param.ichg =
			adc_battery_voltage_charging(average(di->curve.ichg, di->curve.w_index));
		}
	} else {
		/* Handle discharging mode */
		BATPRTD("Handle discharging mode, try get real voltage(%d)\n",
				di->curve.w_index);
		real_bat_vlotage = 
			real_voltage_calculate(di->curve.vbat, di->curve.w_index);
		di->bat_param.volt = real_bat_vlotage;
		if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_DCHG_I) {
			di->bat_param.cur =
			adc_battery_voltage_discharging(average(di->curve.ibat, di->curve.w_index));
		}
	}

	if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_TEMP) {
		di->bat_param.temp =
			adc_battery_temp(average(di->curve.temp, di->curve.w_index));
		BATPRTD("di->bat_param.temp = (%d)\n", di->bat_param.temp);
	}

	/* Now we get all parameter, try figure out capacitance */
	if (di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING ||
		di->bat_param.status == POWER_SUPPLY_STATUS_FULL) {
	/* In this case, we will calculate real capacitance of battery,
	   it based real voltage and real charging current, it get from above */
	/* Calculate battery capacity on charging mode */
	//1TODO: In CV mode, voltage very near, so need calculate depend on charging current
	//1TODO: Currently PMIC--lc1100l not support charging current detect, so...
		#if 0
		//1TODO: Add charging current table select code, if hardware support
		#else
		di->bat_param.cap = cap_calculate(di, real_bat_vlotage, 400, charge_curve, ARRAYSIZE(charge_curve));
		printk("Select charg_curve with %d\n", 400);
		#endif
	} else {
		/* Calculate battery capacity on discharging mode */
		//1TODO: Add discharging current table select code, if hardware suppord
		if (di->system_state & SYSTEM_STATE_LCD_ON) {
			BATPRTD("System state is LCD on\n");
			di->bat_param.cap = cap_calculate(di, real_bat_vlotage, LCD_ON_WAKEUP, discharge_curve, ARRAYSIZE(discharge_curve));
			printk("Select discharge_curve with %d\n", LCD_ON_WAKEUP);
		} else {
			di->bat_param.cap = cap_calculate(di, real_bat_vlotage, LCD_OFF_WAKEUP, discharge_curve, ARRAYSIZE(discharge_curve));
			printk("Select discharge_curve with %d\n", LCD_OFF_WAKEUP);
		}
	}
	{
		/* Debug only */
		charger_info(di, real_bat_vlotage);
		/* End debug message */
	}
	return 0;
}

static int coimp_battery_state_change_handle
	(struct comip_battery_device_info *di, struct comip_battery_event *event)
{
	struct comip_battery_state *state;

	#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(&charger_wakelock, 10 * HZ);
	#endif

	state = state_table + di->current_state.state;
	BATPRTD("handle event [%s].\n", state->name);
	//1TODO: Will add battery event trace for detect fast charger hot plug
	//memcpy(&di->old_state, state, sizeof(struct comip_battery_state));
	if (state->handle) {
		state->handle(di, event);
	}
	BATPRTD("state [%s] -->> state [%s].\n", 
			state->name, state_table[di->current_state.state].name);

	#ifdef CONFIG_HAS_WAKELOCK
	wake_unlock(&charger_wakelock);
	//1TODO: If system on sleep state, when PMIC send interrupt event, maybe system need update some info, such as battery capacitance, but system goto sleep. so how dispose?
	#endif

	return 0;
}

#define COMIP_CHARGING_COMPLETE_TIME	20 /* minute */
#define COMIP_CHARGING_COMPLETE_CNT		(COMIP_CHARGING_COMPLETE_TIME * \
										 (60 / (COMIP_BATTERY_PARAM_UPDATE_INTERVAL / 1000)))
struct charging_curve {
	int ibatt;
	int vbatt;
	int dbatt;
	int cap;
};

static struct charging_curve charging_curve[COMIP_CHARGING_COMPLETE_CNT];
static int comip_handle_charging_complete
		(struct comip_battery_device_info *di)
{
	int high_cap_sum = 0;
	int low_ichg_high_vol_sum = 0;
	int i;

	//printk("%s\n", __func__);
	if (di->hw_ops->is_charging_complete) {
		if (di->hw_ops->is_charging_complete())
			return comip_battery_send_event(di, 
							COMIP_BATTERY_EVENT_CHARGE_COMPLETED, NULL, 0);
		else
			return 0;
	}

	#define CHARGING_COMPLETE_CAP_LEVEL		99
	#define CHARGING_COMPLETE_IBATT_LEVEL	40
	#define CHARGING_COMPLETE_VBATT_LEVEL	4150
	#define CHARGING_COMPLETE_PERCENT		80 /* 80% */
	for (i = 0; i < COMIP_CHARGING_COMPLETE_CNT; i++ ) {
		if (charging_curve[i].cap >= CHARGING_COMPLETE_CAP_LEVEL)
			high_cap_sum++;

		if (di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_CHG_I) {
			if (di->charger_param.ichg <= CHARGING_COMPLETE_IBATT_LEVEL &&
				di->bat_param.volt >= CHARGING_COMPLETE_VBATT_LEVEL)
					low_ichg_high_vol_sum++;
		}
	}
	high_cap_sum *= 100;
	//printk("high_cap_sum: %d, percent: %d\n", 
	//		high_cap_sum, high_cap_sum / COMIP_CHARGING_COMPLETE_CNT);
	if ((high_cap_sum / COMIP_CHARGING_COMPLETE_CNT) >= CHARGING_COMPLETE_PERCENT) {
		printk("Handle high capacity send complete msg(%d)\n", 
					high_cap_sum / COMIP_CHARGING_COMPLETE_CNT);
		return comip_battery_send_event(di, 
							COMIP_BATTERY_EVENT_CHARGE_COMPLETED, NULL, 0);
	}

	low_ichg_high_vol_sum *= 100;
	if ((low_ichg_high_vol_sum / COMIP_CHARGING_COMPLETE_CNT) >= CHARGING_COMPLETE_PERCENT) {
		printk("%s: Detect chargring %d, battery voltage %d(%d)\n", 
					__func__, di->charger_param.ichg, di->bat_param.volt, 
					low_ichg_high_vol_sum / COMIP_CHARGING_COMPLETE_CNT);
		return comip_battery_send_event(di, 
							COMIP_BATTERY_EVENT_CHARGE_COMPLETED, NULL, 0);
	}
	return 0;
}

static int comip_battery_monitor_thread(void *data)
{
	struct vct_request vct_setting = {.cnt = 1, .interval = 5000};
	struct comip_battery_device_info *di = 
				(struct comip_battery_device_info *)data;
	int ret = 0;
	int update = 0;
	int old_cap = 0;
	int old_temp = 0;
	const int need_update_imm_event = COMIP_BATTERY_EVENT_CHARGE_COMPLETED |
									  COMIP_BATTERY_EVENT_CHARGE_RESTART |
									  COMIP_BATTERY_EVENT_BATTERY_LOW;
	const int need_discard_event = COMIP_BATTERY_EVENT_CHG_PLUG |
								   COMIP_BATTERY_EVENT_CHG_UNPLUG |
								   COMIP_BATTERY_EVENT_BAT_PACKED |
								   COMIP_BATTERY_EVENT_BAT_UNPACKED;
	const int need_disp_reason_event = COMIP_BATTERY_EVENT_BATTERY_LOW |
							COMIP_BATTERY_EVENT_CHARGE_COMPLETED;
	struct comip_battery_event *event = NULL;

	di->monitor_thread = current;
	daemonize("COMIP Battery state process");
	while (1) {
		ret = wait_event_timeout(di->wait, 
							(event = comip_battery_recv_event(di)), 
							msecs_to_jiffies(di->update_interval));
		do {
			int event_cnt = 0;
			update = (ret == 0) ? 1 : 0;
			while (event) {
				struct comip_battery_state *state;
				event_cnt++;
				state = state_table + di->current_state.state;
				if (state->valid_events & event->id) {
					coimp_battery_state_change_handle(di, event);
					/* Add time format compare code for handle fast event */
					#if 1
					/* Belowe dbg only or need double check */
					if (0) {
						if (event->id & need_discard_event) {
							BATPRTD("Detect need discard event\n");
							di->curve.w_index = 0;
							break;
						}
					} else if (event->id & need_update_imm_event) {
						BATPRTD("Detect need update immediate event\n");
						#ifdef CONFIG_HAS_WAKELOCK
						wake_lock_timeout(&charger_wakelock, 3 * HZ);
						#endif
						update = 1;
						vct_setting.cnt = 8;
						vct_setting.interval = 50; /* ms */
						di->curve.w_index = 0;
						di->curve.terminal_cnt = 8;
					}
					if (event->id & need_disp_reason_event) {
						switch (event->id) {
							case COMIP_BATTERY_EVENT_BATTERY_LOW:
								di->wake_up_reason.reason = WAKE_BATT_LOW;
								break;
							case COMIP_BATTERY_EVENT_CHARGE_COMPLETED:
								di->wake_up_reason.reason = WAKE_BATT_FULL;
								break;
							default:
								break;
						}
					}
					#endif
				} else if (event->id == COMIP_BATTERY_EVENT_UAER_WAKE) {
					/* Handle unsolicited wakeup, need update immediately */
					struct wake_up_reason *msg;
					update = 1;
					BATPRTD("Handle unsolicited wakeup.\n");
					msg = (struct wake_up_reason *)event->dummy;
					di->wake_up_reason.reason = msg->reason;
					vct_setting.cnt = 8;
					vct_setting.interval = 50; /* ms */
					di->curve.w_index = 0;
					di->curve.terminal_cnt = 8;
				} else {
					BATTERY_LC1100HT_PRINTKA("Unknown event 0x%08x with %s)\n", event->id, state->name);
				}
				if (event) {
					comip_battery_free_event(event, di);
					event = NULL;
				}
				#if 0
				event = comip_battery_recv_event(di);
				#endif
			}

			if (event_cnt > 1) {
				printk("\nReceive event fast, mabey need wait voltage stable\n\n");
				break;
			}

			if (update) {
				#if 0
				struct comip_battery_event *new_event = NULL;
				#endif
				/* Timeout or need update battery flag */
				comip_battery_vct_request(di, &vct_setting);
				#if 0
				new_event = comip_battery_recv_event(di);
				if (new_event) {
					struct comip_battery_state *state;
					/* New event occur, we handle new event, 
					   should discard VCT result if need */
					printk("%s: New event occur, when "
						   "we try get voltage sample points!\n", __func__);
					state = state_table + di->current_state.state;
					if (state->valid_events & new_event->id) {
						coimp_battery_state_change_handle(di, new_event);
					}
					kfree(new_event);
					/* Add curce trace reset code here, and give up calculate */
					vct_setting.cnt = 1;
					di->curve.w_index = 0;
					di->curve.terminal_cnt = AVG_COUNT;
					di->wake_up_reason.reason = WAKE_NORMAL;
					break;
				}
				#endif
			}

			if (di->curve.w_index >= di->curve.terminal_cnt) {
				di->curve.w_index = di->curve.terminal_cnt;
				BATPRTD("Update battery info.\n");
				comip_battery_do_bat_calculate(di);
				/* Reset VCT request and buffer point */
				vct_setting.cnt = 1;
				di->curve.w_index = 0;
				di->curve.terminal_cnt = AVG_COUNT;
			}
			if (di->bat_param.cap != old_cap) {
				old_cap = di->bat_param.cap;
				power_supply_changed(&di->battery);
			}
			if (di->bat_param.temp != old_temp) {
				old_temp = di->bat_param.temp;
				power_supply_changed(&di->battery);
			}

			if (di->bat_param.status == POWER_SUPPLY_STATUS_CHARGING &&
				di->wake_up_reason.reason == WAKE_NORMAL) {
				charging_curve[charging_index].cap = di->bat_param.cap;
				charging_curve[charging_index].ibatt = di->charger_param.ichg;
				charging_curve[charging_index].vbatt = di->bat_param.volt;
				charging_curve[charging_index].dbatt = di->bat_param.cur;
				charging_index++;
				if (charging_index == COMIP_CHARGING_COMPLETE_CNT) {
					comip_handle_charging_complete(di);
					charging_index = 0;
				}
			}
			di->wake_up_reason.reason = WAKE_NORMAL;
		} while(0);
	}
	return 0;
}

static int comip_battery_init_set(struct comip_battery_device_info *di)
{
	struct vct_request vct_setting;
	int state = 1; int test = 0;

	spin_lock_init(&di->lock);
	sema_init(&di->sem, 1);
	sema_init(&di->adc_sem, 1);
	init_waitqueue_head(&di->wait);
	INIT_LIST_HEAD(&di->event_q);
	di->update_interval = COMIP_BATTERY_PARAM_UPDATE_INTERVAL;

	di->bat_param.health = POWER_SUPPLY_HEALTH_GOOD;
	di->bat_param.tech = POWER_SUPPLY_TECHNOLOGY_LION;
	di->bat_param.present = 1; //1TODO: Maybe need read battery state from pmic hardware
	di->bat_param.cap = 0;
	di->bat_param.temp = 0;

	di->usb_event_parm.plug = 0;
	di->usb_event_parm.confirm = usb_detect_handle;

	if (di->hw_ops->charger_state_get && 
		(di->hw_ops->charger_state_get() == CHARGER_STATE_CHARGER_PLUG)) {
		int current_value = 0;
		BATPRTD("Charger plugged.\n");
		di->usb_event_parm.plug = 1;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		/* Handle charger plugged, when phone power on. 
		   We need get charger type and according this type set different current
		*/
		if (CHARGING_STATE_CHARGING == charging_detect(di)) {
			di->current_state.state = COMIP_BATTERY_CHARGING;
			di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;
			//power_routing_switch_off();
		} else {
			printk("Charger init set is charginng or complete?\n");
			di->current_state.state = COMIP_BATTERY_CHARGING;
			di->bat_param.status = POWER_SUPPLY_STATUS_CHARGING;
		}
		di->charger_param.type = comip_battery_charger_type_detect(di);
		di->charger_param.online = 1;

		if (di->charger_param.type == CHARGE_TYPE_USB) {
			current_value = USB_CHARGING_CURRENT;
		} else if (di->charger_param.type == CHARGE_TYPE_AC) {
			current_value = CHARGER_CHARGING_CURRENT;
		} else {
			current_value = USB_CHARGING_CURRENT;
			printk("%s: Start charger type detect timer.\n", __func__);
			di->charger_param.type = CHARGE_TYPE_USB;
			hrtimer_start(&di->charger_detect_timer, 
							ktime_set(30, 0), HRTIMER_MODE_REL);
		}
		if (current_value != 0)
			comip_battery_schedule_current_req(current_value, di);
		//queue_delayed_work(battery_charge_wq, &set_charge_current_work, 0*HZ);
		//monitor_wakeup.wake_up_state = LCD_ON_WAKEUP;
	} else {
		BATPRTD("Charger unplugged.\n");
		di->usb_event_parm.plug = 0;
		pmic_event_handle(PMIC_EVENT_USB, &di->usb_event_parm);
		di->current_state.state = COMIP_BATTERY_DISCHARGING;
		di->bat_param.status = POWER_SUPPLY_STATUS_DISCHARGING;
		di->charger_param.type = CHARGE_TYPE_UNKNOW;
		di->charger_param.online = 0;
	}

	/* For init voltage */
	g_di->hw_ops->charging_state_set(0);
	mdelay(10);
	di->system_state |= SYSTEM_STATE_LCD_ON;
	di->old_cap.state = 0;
	di->old_cap.timeval = 0;
	di->system_state |= SYSTEM_INIT_POINT;
	vct_setting.cnt = 15;
	vct_setting.interval = 3; /* Use 3ms delay */
	if (di->current_state.state == COMIP_BATTERY_CHARGING) {
		printk("\n\nHandle charging\n\n");
		state = di->bat_param.status;
		di->bat_param.status = POWER_SUPPLY_STATUS_DISCHARGING;
		test = 1;
	}
	comip_battery_vct_request(di, &vct_setting);
	comip_battery_do_bat_calculate(di);
	di->system_state &= ~SYSTEM_INIT_POINT;
	di->curve.w_index = 0; /* Clear init sample data */
	if (test) {
		di->bat_param.status = state;
	}

#ifndef	CONFIG_COMIP_BOARD_LC1810_EVB
	g_di->hw_ops->charging_state_set(1);
#endif

	return 0;
}


static enum power_supply_property comip_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property comip_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static enum power_supply_property comip_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int comip_battery_get_property(struct power_supply *psy,
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
		val->intval = di->bat_param.volt * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = di->bat_param.cur * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = di->bat_param.temp * 10;
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

static int comip_usb_get_property(struct power_supply *psy,
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

static int comip_ac_get_property(struct power_supply *psy,
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
		if (di->charger_param.type == CHARGE_TYPE_AC) {
			val->intval = di->charger_param.volt;
		} else {
			val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (di->charger_param.type == CHARGE_TYPE_AC) {
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

static struct comip_battery_event*
	comip_battery_recv_event(struct comip_battery_device_info *di)
{
	struct comip_battery_event *event = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&di->lock, flags);
	if (!list_empty(&di->event_q)) {
		event = list_first_entry(&di->event_q, struct comip_battery_event, list);
		list_del(&event->list);
	}
	spin_unlock_irqrestore(&di->lock, flags);
	return event;
}

static struct comip_battery_event* 
	comip_battery_get_event(struct comip_battery_device_info *di)
{
	unsigned long irqflags;
	struct comip_battery_event *event = NULL;
	spin_lock_irqsave(&di->event_lock, irqflags);
	if (!list_empty(&di->free_event)) {
		event = list_first_entry(&di->free_event, struct comip_battery_event, event_buf);
		list_del(&event->event_buf);
	}
	spin_unlock_irqrestore(&di->event_lock, irqflags);
	return event;
}

static void comip_battery_free_event(struct comip_battery_event* event, 
	struct comip_battery_device_info *di)
{
	unsigned long irqflags;
	spin_lock_irqsave(&di->event_lock, irqflags);
	list_add_tail(&event->event_buf, &di->free_event);
	spin_unlock_irqrestore(&di->event_lock, irqflags);
}

static int comip_battery_send_event(struct comip_battery_device_info *di,
		enum comip_battery_event_id id, void *user_data, int user_data_size)
{
	int ret = 0;
	struct comip_battery_state *state;
	struct comip_battery_event *event;
	unsigned long flags = 0;

	state = state_table + di->current_state.state;

	/*check whether the event is cared by current charge state*/
	event = comip_battery_get_event(di);
	if (!event) {
		BATTERY_LC1100HT_PRINTKA("No memory to create new event(0x%08x).\n", id);
		ret = -ENOMEM;
		goto error;
	}

	event->id = id;
	if (user_data_size) {
		if (user_data_size > sizeof(event->dummy)) {
			printk("%s:User data too large!!!\n", __func__);
			comip_battery_free_event(event, di);
			ret = -ENOMEM;
			goto error;
		}
		memcpy(event->dummy, user_data, user_data_size);
	}

	/* Insert a new event to the list tail and wakeup the process thread */
	BATPRTD("Send event 0x%08x to state [%s].\n", id, state->name);
	spin_lock_irqsave(&di->lock, flags);
	list_add_tail(&event->list, &di->event_q);
	wake_up(&di->wait);
	spin_unlock_irqrestore(&di->lock, flags);

error:
	return ret;
}

static void comip_battery_int_event(int event, void *puser, void* pargs)
{
	unsigned int int_status = *((unsigned int *)pargs);
	struct comip_battery_device_info *di = 
					(struct comip_battery_device_info *)puser;
	unsigned int status;

	status = di->hw_ops->int2event(int_status);

	if (status & COMIP_BATTERY_EVENT_CHG_UNPLUG) {
		comip_battery_send_event(di, COMIP_BATTERY_EVENT_CHG_UNPLUG, NULL, 0);
	}

	if (status & COMIP_BATTERY_EVENT_CHG_PLUG) {
		comip_battery_send_event(di, COMIP_BATTERY_EVENT_CHG_PLUG, NULL, 0);
	}

	if (status & COMIP_BATTERY_EVENT_CHARGE_RESTART) {
		comip_battery_send_event(di, COMIP_BATTERY_EVENT_CHARGE_RESTART, NULL, 0);
	}

	if (status & COMIP_BATTERY_EVENT_CHARGE_COMPLETED) {
		comip_battery_send_event(di, COMIP_BATTERY_EVENT_CHARGE_COMPLETED, NULL, 0);
	}

	if (status & COMIP_BATTERY_EVENT_BATTERY_LOW) {
		int voltage = di->hw_ops->get_battery_warning_level(1);
		comip_battery_send_event(di, COMIP_BATTERY_EVENT_BATTERY_LOW, &voltage, sizeof(int));
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void comip_battery_early_suspend(struct early_suspend *h)
{
	//monitor_wakeup.up_params_flag = 0;
	struct comip_battery_device_info *di;
	di = container_of(h, struct comip_battery_device_info, early_suspend);

	di->system_state &= ~SYSTEM_STATE_LCD_ON;
}

static void comip_battery_early_resume(struct early_suspend *h)
{
	//monitor_wakeup.up_params_flag = 1;
	//monitor_wakeup.wake_up_state=LCD_ON_WAKEUP;
	//wake_up(&monitor_wakeup.up_params_wait);
	struct comip_battery_device_info *di;
	struct wake_up_reason msg;
	di = container_of(h, struct comip_battery_device_info, early_suspend);
	msg.reason = WAKE_LCD_ON;
	di->system_state |= SYSTEM_STATE_LCD_ON;
	comip_battery_send_event(di, COMIP_BATTERY_EVENT_UAER_WAKE, 
								&msg, sizeof(struct wake_up_reason));
	wake_up(&di->wait);
	//power_routing_switch_off();
}
#endif

static void __comip_battery_set_current_work(struct work_struct *work)
{
	struct comip_battery_device_info *di = container_of(
		(struct delayed_work *)work, struct comip_battery_device_info, set_current_work);
	int ret = 0, old, new;
	if (!di->hw_ops->set_charging_current) {
		BATPRTD("Hardware not support charging current set!\n");
		return;
	}
	mutex_lock(&di->current_work_req.lock);
	old = di->current_work_req.old;
	new = di->current_work_req.new;
	ret = di->hw_ops->set_charging_current(old, new);
	BATPRTD("Old-%d, target-%d, ret-%d\n", old, new, ret);
	if (ret < 0) {
		printk("Hardware set charging current failed!\n");
		di->current_work_req.in_progress = 0;
		di->current_work_req.new = old; /* Set failed, restoration current info */
	} else if (ret > 0) {
		di->current_work_req.old = ret;
		schedule_delayed_work(&di->set_current_work, 
					msecs_to_jiffies(di->current_work_req.interval));
	} else {
		/* Setup finish */
		di->current_work_req.in_progress = 0;
	}
	mutex_unlock(&di->current_work_req.lock);
}

static int comip_battery_schedule_current_req(int target, 
	struct comip_battery_device_info *di)
{
	int ret = 0;
	mutex_lock(&di->current_work_req.lock);
	if (di->current_work_req.in_progress) {
		WARN(1, "Charger current in progress!");
		ret = -EAGAIN;
		goto out;
	}
	di->current_work_req.old = di->current_work_req.new;
	di->current_work_req.new = target;
	di->current_work_req.in_progress = 1;
	printk("Current set request: old-%d, target-%d\n", 
		di->current_work_req.old, di->current_work_req.new);
	schedule_delayed_work(&di->set_current_work, 0); /* immediately */
out:
	mutex_unlock(&di->current_work_req.lock);
	return ret;
}

static int comip_battery_current_req_kill
	(struct comip_battery_device_info *di)
{
	int ret = 0;
	cancel_delayed_work_sync(&di->set_current_work);
	mutex_lock(&di->current_work_req.lock);
	di->current_work_req.in_progress = 0;
	/* Set default charging current */
	if (di->hw_ops->set_charging_current)
		di->hw_ops->set_charging_current(di->current_work_req.new, INIT_CHARGING_CURRENT);
	di->current_work_req.new = INIT_CHARGING_CURRENT;
	mutex_unlock(&di->current_work_req.lock);
	return ret;
}

static int test_get_vbat(void)
{
	struct comip_battery_adc_request temp_req;
	int i; int sum = 0; int valid = 0;
	int voltage = 0;

	memset(&temp_req, 0, sizeof(struct comip_battery_adc_request));
	temp_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_BAT_V;
	for (i = 0; i < 3; i++) {
		int ret = battery_adc_acquire(g_di, &temp_req);
		if (ret < 0)
			continue;
		sum += (int)temp_req.adc_value[ADC_BAT_V];
		valid++;
	}
	if (valid != 0) {
		sum = sum / valid;
		//voltage = g_di->hw_ops->adc2voltage((u16)sum);
		voltage = adc_battery_voltage((u16)sum);
		BATPRTD("Get voltage %d\n", voltage);
	}
	return voltage;
}

static int test_get_vbus(void)
{
	struct comip_battery_adc_request temp_req;
	int i; int sum = 0; int valid = 0;
	int voltage = 0;
	memset(&temp_req, 0, sizeof(struct comip_battery_adc_request));
	temp_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_VIN_CHG_V;
	for (i = 0; i < 3; i++) {
		int ret = battery_adc_acquire(g_di, &temp_req);
		if (ret < 0)
			continue;
		sum += (int)temp_req.adc_value[ADC_VIN_CHG_V];
		valid++;
	}
	if (valid != 0) {
		sum = sum / valid;
		voltage = g_di->hw_ops->adc2vbus((u16)sum);
		BATPRTD("Get voltage %d\n", voltage);
	}
	return voltage;
}

#ifdef CONFIG_PROC_FS
/*amt adjust code begin*/
static char proc_cmd[100] = {0};
static int  proc_data[20] = {0};

#define G_VBATT_MIN(x)  (g_vbatt[0] <= x || g_vbatt[1] <= x || g_vbatt[2] <= x || g_vbatt[3] <= x)
#define G_VBATT_MAX(x)  (g_vbatt[0] >= x || g_vbatt[1] >= x || g_vbatt[2] >= x || g_vbatt[3] >= x)

static ssize_t comip_battery_proc_read(struct file *file, char __user *buffer,
                        size_t count, loff_t *ppos)
{
	if (proc_cmd[0] == '\0') {
		struct comip_battery_adc_request adc_req;
		memset(&adc_req, 0, sizeof(struct comip_battery_adc_request));
		if (g_di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_BAT_V)
			adc_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_BAT_V;

		if (is_charging_mode(g_di->bat_param.status)) {
			if (g_di->hw_ops->adc_support_type & COMIP_BATTERY_ADC_SUPPORT_CHG_I)
				adc_req.inputs |= COMIP_BATTERY_ADC_SUPPORT_CHG_I; /* Charger current */
		}
		g_di->hw_ops->adc_acquire(&adc_req);
		return snprintf(buffer, PAGE_SIZE, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
				0, adc_req.adc_value[ADC_CHG_I], /* Charger LDO current, Charger current */
				0, adc_req.adc_value[ADC_BAT_V], /* Discharger current, Battery voltage */
				0, 0, /* VDD voltage, VIN_CHG voltage */
				0, 0, /* VCOIN voltage, TEMP */
				0, 0, /* GPIO, GPIO */
				0, 0);/* GPIO */
	} else if (!strcmp(proc_cmd, "vbatt")) {
		return snprintf(buffer, PAGE_SIZE, "vbatt %d", test_get_vbat());
	} else if (!strcmp(proc_cmd, "vbus")) {
		return snprintf(buffer, PAGE_SIZE, "vbus %d", test_get_vbus());
	} else if (!strcmp(proc_cmd, "test_charging")) {
		int vbat_with_charging; int vbat;
		vbat_with_charging = test_get_vbat();
		g_di->hw_ops->charging_state_set(0); /* Stop */
		udelay(50);
		vbat = test_get_vbat();
		g_di->hw_ops->charging_state_set(1); /* restore */
		return snprintf(buffer, PAGE_SIZE, "test_charging %d:%d diff %d",
					vbat_with_charging, vbat, vbat_with_charging - vbat);
	} else if (!strcmp(proc_cmd, "amt_get_vbatt")) {
		//int state = g_di->hw_ops->charger_state_get();
		int vbatt;
		g_di->hw_ops->charging_state_set(0);
		udelay(100);
		vbatt = test_get_vbat();
		//state = state == CHARGING_STATE_CHARGING ? 1 : 0;
		g_di->hw_ops->charging_state_set(1); /* restore */
		return snprintf(buffer, PAGE_SIZE, "%d\n", vbatt);
	} else {
		return snprintf(buffer, PAGE_SIZE, "%s", "Unknown command");
	}
}

static int comip_battery_proc_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{
	static char kbuf[4096];
	char *buf = kbuf;
    struct monitor_battery_device_info *di = PDE_DATA(file_inode(file));
	char cmd;

	if (count >= 4096)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	cmd = buf[0];
	BATPRTD("%s:%s\n", __func__, buf);

	if ('p' == cmd) {
		/* Test USB/Charger plug */
		unsigned int cnt, interval;
		int i;
		sscanf(buf, "%c %d %d", &cmd, &cnt, &interval);
		if (cnt > 0xff) {
			BATPRTD("%s: USB plug test case time(%d) too large!\n", __func__, cnt);
			goto out;
		}
		for (i=0; i<cnt; i++) {
			unsigned int event = i % 2 ? 
				COMIP_BATTERY_EVENT_CHG_PLUG : COMIP_BATTERY_EVENT_CHG_UNPLUG;
			comip_battery_send_event(di, event, NULL, 0);
		}
	} else if ('c' == cmd) {
		sscanf(buf, "%c %d", &cmd, &g_test_cap);
	} else if ('s' == cmd) {
		int stop;
		sscanf(buf, "%c %d", &cmd, &stop);
		g_di->hw_ops->charging_state_set(!!stop);
	} else if ('g' == cmd) {
		char type;
		sscanf(buf, "%c %c", &cmd, &type);
		if (type == 'b') {
			sprintf(proc_cmd, "%s", "vbatt");
			proc_data[0] = test_get_vbat();
			printk("Get battery voltage %d", proc_data[0]);
		} else if (type == 'v') {
			sprintf(proc_cmd, "%s", "vbus");
			proc_data[0] = test_get_vbus();
			BATPRTD("Get vbus voltage %d", proc_data[0]);
		}
	} else if ('t' == cmd) {
		char type;
		sscanf(buf, "%c %c", &cmd, &type);
		if (type == 'c') {
			sprintf(proc_cmd, "%s", "test_charging");
			BATPRTD("Handle charging voltage test.");
		}
	} else if ('a' == cmd) {
		if (!strncmp(buf, "amt_get_vbatt", strlen("amt_get_vbatt"))) {
			sprintf(proc_cmd, "%s", "amt_get_vbatt");
		} else {
			int temp_val; int ret;
			BATPRTD("AMT vbatt init...");
			ret = sscanf(buf, "amt_set_vbatt[%d] [%d] [%d] [%d] \n", &g_vbatt[0], &g_vbatt[1], &g_vbatt[2], &g_vbatt[3]);
			BATPRT("AMT orignal g_vbatt[%d] [%d] [%d] [%d]\n", g_vbatt[0], g_vbatt[1], g_vbatt[2], g_vbatt[3]);
			if (!ret || (G_VBATT_MIN(3400)) || (G_VBATT_MAX(4400))) {
				g_vbatt[0] = 4200;
				g_vbatt[1] = 4200;
				g_vbatt[2] = 3600;
				g_vbatt[3] = 3600;
				BATPRTD("read from AMT param failed or the value is invalid! use fault value!\n");
			}
			// add check code here
			g_di->hw_ops->charging_state_set(0);
			udelay(100);
			temp_val = test_get_vbat();
			//state = state == CHARGING_STATE_CHARGING ? 1 : 0;
			g_di->hw_ops->charging_state_set(1); /* restore */
			g_di->bat_param.volt = temp_val;
			g_di->bat_param.cap = cap_calculate(g_di, g_di->bat_param.volt, LCD_ON_WAKEUP, discharge_curve, ARRAYSIZE(discharge_curve));
			power_supply_changed(&g_di->battery);
		}
	}
	BATPRT("\n");
out:
	return count;
}

static const struct file_operations comip_battery_operations = {
        .owner = THIS_MODULE,
        .read  = comip_battery_proc_read,
        .write = comip_battery_proc_write,
};
#endif

static void comip_battery_proc_init(struct comip_battery_device_info *data)
{
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry *proc_entry = NULL;
    proc_entry = proc_create_data("driver/comip_battery", 0, NULL,
    					&comip_battery_operations, data);
#endif
}


#ifdef LC1100HT_TEMP
static ssize_t lc1100ht_battery_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
       return sprintf(buf, "%s\n", "lc1100ht_battery_show");
}

static ssize_t lc1100ht_battery_store(struct kobject *kobj,
                                    struct kobj_attribute *attr, const char *buf, size_t count)
{
	char charge_mode[] = "charge";
	char no_charge_mode[] = "no_charge";

	if (!memcmp(charge_mode, buf,
		    min(sizeof(charge_mode)-1, count))) {
		BATPRT("lc1100ht_battery charge!\n");
		g_di->hw_ops->charging_state_set(1);
	} else if (!memcmp(no_charge_mode, buf,
		    min(sizeof(no_charge_mode)-1, count))) {
		BATPRT("lc1100ht_battery not charge!\n");
		g_di->hw_ops->charging_state_set(0);
	}

	return count;
}

static struct kobj_attribute lc1100ht_battery_attr = {
        .attr = {
                .name = "lc1100ht_battery",
                //.mode = S_IRUGO,
                .mode = 0644,
        },
        .show = lc1100ht_battery_show,
        .store = lc1100ht_battery_store,
};

static struct attribute *lc1100ht_battery_attributes[] = {
        &lc1100ht_battery_attr.attr,
        NULL
};

static struct attribute_group lc1100ht_battery_group = {
        .attrs = lc1100ht_battery_attributes
};
static void __init comip_battery_sys_init(void)
{
        struct kobject *properties_kobj;
        int ret = 0;
        properties_kobj = kobject_create_and_add("lc1100ht_power", NULL);
	if (!properties_kobj)
		return ENOMEM;
        else
                ret = sysfs_create_group(properties_kobj, &lc1100ht_battery_group);
        if (ret) {
                printk("Create lc1100ht_power failed!\n");
		kobject_put(properties_kobj);
        }
}
#endif

static int usb_detect_handle(void)
{
	BATPRTD("\n");
	hrtimer_cancel(&g_di->charger_detect_timer);
	if(g_di->charger_param.type != CHARGE_TYPE_USB)
		return comip_battery_send_event(g_di, COMIP_BATTERY_EVENT_USB_CALLBACK, NULL, 0);
	return 0;
}

static enum hrtimer_restart charger_detect_timer_func(struct hrtimer *timer)
{
	struct comip_battery_device_info *di =
		container_of(timer, struct comip_battery_device_info, charger_detect_timer);
	printk("%s\n", __func__);
	comip_battery_send_event(di, COMIP_BATTERY_EVENT_CHARGER_CALLBACK, NULL, 0);
	return HRTIMER_NORESTART;
}

static int comip_hand_plug_debouce(int plug_mode)
{
	dbg("comip_hand_plug_debouce hand %s\n", onoff ? "plug mode" : "unplug mode");
	if (plug_mode) {
		/* plug mode */
		return comip_battery_send_event(g_di, COMIP_BATTERY_EVENT_CHG_UNPLUG, NULL, 0);
	} else {
		/* unplug mode */
		return comip_battery_send_event(g_di, COMIP_BATTERY_EVENT_CHG_PLUG, NULL, 0);
	}
	return 0;
}

static int comip_battery_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct comip_battery_device_info *di;
	struct comip_battery_hw_ops *hw_ops;

	printk("%s\n", __func__);
	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (di == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}
	g_di = di;
	di = memset(di, 0, sizeof(*di));

	spin_lock_init(&di->event_lock);
	INIT_LIST_HEAD(&di->free_event);
	for (i = 0; i < COMIP_BATTERY_MAX_EVENT_CNT; i++)
		list_add_tail(&di->event_buf[i].event_buf, &di->free_event);
	hrtimer_init(&di->charger_detect_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	di->charger_detect_timer.function = charger_detect_timer_func;

	platform_set_drvdata(pdev, di);

	INIT_DELAYED_WORK(&di->set_current_work, __comip_battery_set_current_work);
	mutex_init(&di->current_work_req.lock);
	di->current_work_req.step = 200;
	di->current_work_req.interval = 10;
	di->curve.w_index = 0;
	di->curve.terminal_cnt = AVG_COUNT;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&batt_wakelock, WAKE_LOCK_SUSPEND, "batt_wakelock");
	wake_lock_init(&charger_wakelock, WAKE_LOCK_SUSPEND, "charger_wakelock");
	wake_lock_init(&usb_insert_wakelock, WAKE_LOCK_SUSPEND, "usb_insert_wakelock");
#endif

	hw_ops = &lc1100ht_battery_ops;
	ret = hw_ops->probe();

	if (ret < 0)
		goto err_data_alloc_failed;

	di->hw_ops = hw_ops;
	if (!di->hw_ops) {
		printk("Cannot get battery hardware ops !!!\n");
		//1TODO: Maybe need return failed
	}
	printk("Battery hardware ops: %s\n", di->hw_ops->name);

	comip_battery_init_set(di);

	di->battery.name = "battery";
	di->battery.properties = comip_battery_props;
	di->battery.num_properties = ARRAY_SIZE(comip_battery_props);
	di->battery.get_property = comip_battery_get_property;
	di->battery.type = POWER_SUPPLY_TYPE_BATTERY;

	di->usb.name = "usb";
	di->usb.properties = comip_usb_props;
	di->usb.num_properties = ARRAY_SIZE(comip_usb_props);
	di->usb.get_property = comip_usb_get_property;
	di->usb.type = POWER_SUPPLY_TYPE_USB;

	di->ac.name = "ac";
	di->ac.properties = comip_ac_props;
	di->ac.num_properties = ARRAY_SIZE(comip_ac_props);
	di->ac.get_property = comip_ac_get_property;
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

	ret = pmic_callback_register(PMIC_EVENT_BATTERY, di, comip_battery_int_event);
	if (ret) {
		BATPRT("Fail to register pmic event callback.\n");
		goto err_callback_register;
	}

	pmic_callback_unmask(PMIC_EVENT_BATTERY);
	ret = kernel_thread(comip_battery_monitor_thread, di, 0);
	if (ret < 0) {
		BATPRT("Fail to create battery monitor thread.\n");
		goto monitor_thread_fail;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	di->early_suspend.level = 55;
	di->early_suspend.suspend = comip_battery_early_suspend;
	di->early_suspend.resume = comip_battery_early_resume;
	register_early_suspend(&di->early_suspend);
#endif

	comip_battery_proc_init(di);

#ifdef LC1100HT_TEMP
	comip_battery_sys_init();
#endif

	return 0;

monitor_thread_fail:
	pmic_callback_unregister(PMIC_EVENT_BATTERY);

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

static int comip_battery_remove(struct platform_device *pdev)
{
	struct comip_battery_device_info *di = platform_get_drvdata(pdev);

	power_supply_unregister(&di->battery);
	power_supply_unregister(&di->usb);
	power_supply_unregister(&di->ac);

	if (di->monitor_thread) {
		release_thread(di->monitor_thread);
	}

	if(di->hw_ops->remove){
        di->hw_ops->remove();
    }
    
	kfree(di);
	platform_set_drvdata(pdev, NULL);
	return 0;
}


#ifdef CONFIG_PM
int comip_battery_late_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct comip_battery_device_info *di = platform_get_drvdata(pdev);
	di->system_state |= SYSTEM_SUSPENED;
	if (di->hw_ops->suspend)
		di->hw_ops->suspend();
	return 0;
}

int comip_battery_late_resume(struct platform_device *pdev)
{
	struct comip_battery_device_info *di = platform_get_drvdata(pdev);
	di->system_state &= ~SYSTEM_SUSPENED;
	if (di->hw_ops->resume)
		di->hw_ops->resume();
	return 0;
}
#endif

static struct platform_driver comip_battery_device = {
#ifdef CONFIG_PM
	.suspend = comip_battery_late_suspend,
	.resume = comip_battery_late_resume,
#endif
	.probe = comip_battery_probe,
	.remove = comip_battery_remove,
	.driver = {
		.name = "lc1100ht-battery"
	}
};

static int __init comip_battery_init(void)
{
	return platform_driver_register(&comip_battery_device);
}

static void __exit comip_battery_exit(void)
{
	platform_driver_unregister(&comip_battery_device);
}

//1 Below hardware relative

static int lc1100ht_battery_charging_state_set(int onoff);
static int lc1100ht_battery_charger_state_get(void);

#ifdef LC1100HT_PRSW
#define LC1100HT_PRSW_SWITCH_TIME		(5*60*1000)
static int lc1100ht_set_auto_prsw(int auto_mode)
{
	return lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_CTL,
					LC1100HT_REG_BITMASK_SYS_SUPPORT_AUTO_EXIT_EN, !!auto_mode);
}
static void switch_prsw_work(struct work_struct *work)
{
	printk("%s\n", __func__);
	lc1100ht_set_auto_prsw(1);
}
static DECLARE_DELAYED_WORK(switch_prsw, switch_prsw_work);
#endif

struct hrtimer charger_interrupt_timer;
static struct workqueue_struct *bat_stat_workq=NULL;
static struct work_struct       bat_stat_work;
#define CHARGER_INTERRUPT_DETECT_TIME	3

struct debounce {
	int (*lc1100ht_hand_plug_debouce)(int plug_mode);
};

struct debounce lc1100ht_debounce ;

static int lc1100ht_set_charger_plug_interrupt(int mask)
{
	return lc1100ht_reg_bit_write(LC1100HT_REG_INT_EN_2,
				LC1100HT_REG_BITMASK_CHG_INPUT_STATE_INT_EN, !!mask);
}

static void lc1100ht_charger_interrupt_timer_work(struct work_struct *work)
{
	int plug_change = 0;
	BATPRTD("\n");

	if (COMIP_BATTERY_EVENT_CHG_UNPLUG == lc1100ht_battery_charger_state_get()) {
		lc1100ht_debounce.lc1100ht_hand_plug_debouce(1);
		plug_change = 1;
	}
	lc1100ht_set_charger_plug_interrupt(1);
	if (plug_change == 1 && COMIP_BATTERY_EVENT_CHG_PLUG == lc1100ht_battery_charger_state_get())
		lc1100ht_debounce.lc1100ht_hand_plug_debouce(0);

}

static enum hrtimer_restart lc1100ht_charger_interrupt_callback(struct hrtimer *timer)
{
    if(bat_stat_workq){
        queue_work(bat_stat_workq, &bat_stat_work);
    }
	return HRTIMER_NORESTART;
}

static int lc1100ht_battery_probe(void)
{
	int ret = 0;
	u8 val = 0x00;
	/* Init battery charger */
	/* Try get battery state */
	__lc1100ht_reg_read(LC1100HT_REG_STATE_1, &val);
	printk("Battery state 0x%02x\n", val);

	/* 0x00 - Charger exits system support mode (turns switch off) only by 
	   command bit
	   0x01 - Charger exits system support mode (turns switch off) automatically
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_CTL,
					LC1100HT_REG_BITMASK_SYS_SUPPORT_AUTO_EXIT_EN, 0x01);
	/* Automatic end-of-charge and restart */
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_CTL,
					LC1100HT_REG_BITMASK_AUTO_START_STOP, 0x01);

	/* Battery voltage threshold, where the charger switches to full current 
	   charging.
	   00:2.4v;		01:2.6v;	10:2.8v;	11:3.0v
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_PRECHARGE,
					LC1100HT_REG_BITMASK_VFULLRATE, 0x01);
	/* Precharge current.
	   00:50ma;		10:100ma;	01:150ma;	11:200ma
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_PRECHARGE,
					LC1100HT_REG_BITMASK_IPRECHG, 0x01);
	/* Precharge timeout settings(min).
	   000:5min;	001:10min;	010:15min;	011:20min
	   100:30min;	101:45min;	110:60min;	111:disable
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_PRECHARGE,
					LC1100HT_REG_BITMASK_PRECHG_TIMEOUT, 0x06);
	/* Fullrate debounce time.
	   0:3.6s;	1:7.2s;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_PRECHARGE,
					LC1100HT_REG_BITMASK_FULLRATE_DEBOUNCE_TIME, 0x00);

	/* Charge input current limit(mA).
	   00000:50ma;	00001:100ma;	00010:150ma;	00011:200ma;
	   00100:250ma;	00101:300ma;	00110:350ma;	00111:400ma;
	   01000:450ma;	01001:500ma;	......
	   10101:1000ma;10110:1150ma;	10111:1200ma;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_INPUT_CURRENT_LIMIT,
					LC1100HT_REG_BITMASK_IDCIN, 0x01);

	/* Battery charging current selection(mA).
	   00000:50ma;	00001:100ma;	00010:150ma;	00011:200ma;
	   00100:250ma;	00101:300ma;	00110:350ma;	00111:400ma;
	   01000:450ma;	01001:500ma;	......
	   10101:1000ma;10110:1150ma;	10111:1200ma;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_BATTERY_CHARGING_CURRENT_SELECT,
					LC1100HT_REG_BITMASK_IBATT, 0x01);

	/* Charger Termination Voltage.
	   0000:4.1v;	0001:4.125v;	0010:4.150v;	0011:4.175v;
	   0100:4.2v;	0101:4.255v;	0110:4.250v;	0111:4.275v;
	   1000:4.3v;	1001:4.325v;	1010:4.350v;	1011:4.375v;
	   1100:4.4v;	1101:4.425v;	1110:4.450v;	1111:4.475v;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_VOLTAGE_SETTING,
					LC1100HT_REG_BITMASK_VTERM, 0x04);
	/* VDD (system supply voltage) Voltage.
	   0000:4.1v;	0001:4.125v;	0010:4.150v;	0011:4.175v;
	   0100:4.2v;	0101:4.255v;	0110:4.250v;	0111:4.275v;
	   1000:4.3v;	1001:4.325v;	1010:4.350v;	1011:4.375v;
	   1100:4.4v;	1101:4.425v;	1110:4.450v;	1111:4.475v;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_VOLTAGE_SETTING,
					LC1100HT_REG_BITMASK_VDD, 0x04);

	//1TODO: EOC mode
	/* End of charger time 5 min */
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_EOC,
					LC1100HT_REG_BITMASK_EOC_TIME, 0x01);

	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_EOC,
					LC1100HT_REG_BITMASK_EOC_MODE_SEL, 0x01);

	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_EOC,
					LC1100HT_REG_BITMASK_EOC_LEVEL, 0x01);

	/* Full Charging Timeout:(hr)
	   000:2;	001:3;	010:4;	011:5;
	   100:6;	101:7;	110:8;	111:disabled;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_TIMEOUT,
					LC1100HT_REG_BITMASK_FULL_CHG_TIMEOUT, 0x06);
	/* Auto Restart Timeout:(min)
	   000:30;	001:60;	010:90;	011:120;
	   100:150;	101:180;	110:210;	111:disabled;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_TIMEOUT,
					LC1100HT_REG_BITMASK_AUTO_RESTART_TIMEOUT, 0x07);

	/* Restart Time:(min)
	   000:Instant (only 400ms debounce applies);
	   001:1.5;	010:2.5;	011:3.5;	100:4.5;
	   101:5.5;	110:6.5;	111:disabled;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_RESTART,
					LC1100HT_REG_BITMASK_RESTART_TIME, 0x00);
	/* Restart Level:(min)
	   000:25mv;	001:50mv;	010:75mv;	011:100mv;
	   100:125mv;	101:150mv;	110:175mv;	111:200mv;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_RESTART,
					LC1100HT_REG_BITMASK_RESTART_LEVEL, 0x03);

	/* Charger OVP trip point
	   000:6v;	001:6.5v;	010:7.0v;	011:7.5v;
	   100~110:8.0v;		111:Disable;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_OVP,
					LC1100HT_REG_BITMASK_VIN_CHG_OVP, 0x01);
	/* VDDLOW relative level to trigger PRSW on
	   00:220mv;	01:325mv;	10:430mv;	11:540mv;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_OVP,
					LC1100HT_REG_BITMASK_VDDLOW_REL, 0x02);
	/* VDDLOW absolute level to trigger PRSW on
	   00:3.15v;	01:3.3v;	10:3.45v;	11:3.6v;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_OVP,
					LC1100HT_REG_BITMASK_VDDLOW_ABS, 0x02);

	//1TODO: Coin cell

	/* Charging && PRSW setting */
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_SYS_SUPPORT,
					LC1100HT_REG_BITMASK_START_CHARGING, 0x00);
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_SYS_SUPPORT,
					LC1100HT_REG_BITMASK_STOP_CHARGING, 0x00);
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_SYS_SUPPORT,
					LC1100HT_REG_BITMASK_SYS_SUPPORT_ON, 0x00);
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_SYS_SUPPORT,
					LC1100HT_REG_BITMASK_SYS_SUPPORT_OFF, 0x00);

	/* PRSW on debounce time - selects the debounce time for the VDDLOW Relative
	   comparator (us):
	   000:5us;		001:10us;	010:15us;	011:25us
	   100:40us;	101:60us;	110:100us;	111:disabled
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_PRSW,
					LC1100HT_REG_BITMASK_PRSW_VDDLOW_REL_DEBOUNCE, 0x05);
	/* Second PRSW on debounce time selects the debounce time for VDDLOW 
	   Absolute comparator(us):
	   000:5us;		001:10us;	010:15us;	011:25us
	   100:40us;	101:60us;	110:100us;	111:disabled
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_PRSW,
					LC1100HT_REG_BITMASK_PRSW_VDDLOW_ABS_DEBOUNCE, 0x00);
	/* System support mode exit debounce, timer start once the current flow from
	   VDD to BATT:
	   000:70ms;	01:160ms;	10:400ms;	11:1.2s
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_PRSW,
					LC1100HT_REG_BITMASK_SYS_SUPPORT_EXIT_DEBOUNCE, 0x03);

	/* Threshold for low battery warning voltage(V):
	   0000:3.1;	0001:3.15;	0010:3.2;	0011:3.25;
	   0100:3.3;	0101:3.35;	0110:3.4;	0111:3.45;
	   1000:3.5;	1001:3.55;	1010:3.6;	1011:3.65;
	   1100:3.7;	1101:3.75;	1110:3.8;	1111:3.85;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_LOW_BATTERY_THRESHOLD,
					LC1100HT_REG_BITMASK_BATT_LOW_FALL, 0x0d); /* 3.75V */
	/* Threshold for low battery warning voltage(V):
	   0000:3.1;	0001:3.15;	0010:3.2;	0011:3.25;
	   0100:3.3;	0101:3.35;	0110:3.4;	0111:3.45;
	   1000:3.5;	1001:3.55;	1010:3.6;	1011:3.65;
	   1100:3.7;	1101:3.75;	1110:3.8;	1111:3.85;
	*/
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_LOW_BATTERY_THRESHOLD,
					LC1100HT_REG_BITMASK_BATT_LOW_RISE, 0x0d); /* 3.75V */

#ifdef CONFIG_COMIP_BOARD_LC1810_EVB
	ret |= lc1100ht_battery_charging_state_set(0);
#endif

#ifdef LC1100HT_PRSW
	INIT_DELAYED_WORK(&switch_prsw, switch_prsw_work);
#endif

	hrtimer_init(&charger_interrupt_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	charger_interrupt_timer.function = lc1100ht_charger_interrupt_callback;

	INIT_WORK(&bat_stat_work, lc1100ht_charger_interrupt_timer_work);
	bat_stat_workq = create_singlethread_workqueue("BATTERY_STATUS_WORK_QUEUE");

	return ret;
}

static int lc1100ht_battery_remove(void)
{
    if(bat_stat_workq){
		destroy_workqueue(bat_stat_workq);
		bat_stat_workq=NULL;
    }

    return 0;
}

static int 
	lc1100ht_battery_adc_acquire(struct comip_battery_adc_request* req)
{
	int input;
	u16 temp;
	for (input = 0, temp = 0x1; input < ADC_INPUT_MAX; input++, temp <<= 1) {
		int res = req->inputs & temp;
		u8 channle;
		if (res == 0) {
			continue;
		} else if (input == ADC_BAT_V) {
			channle = LC1100HT_ADC_VBAT_5;
		} else if (input == ADC_TEMP) {
			channle = LC1100HT_ADC_ADC2;
		} else if (input == ADC_CHG_I) {
			channle = LC1100HT_ADC_IBATT;
		} else if (input == ADC_VIN_CHG_V) {
			channle = LC1100HT_ADC_VBUS;
		} else {
			dbg("Not support this adc request channle %d\n", input);
			continue;
		}
		lc1100ht_read_adc(channle, &(req->adc_value[input]));
		//dbg("ADC channle(%d): %d. Value: %d\n", 
		//				input, channle, req->adc_value[input]);
	}
	return 0;
}

static int lc1100ht_battery_charger_state_get(void)
{
	u8 state = 0;
	__lc1100ht_reg_read(LC1100HT_REG_STATE_1, &state);
	return state & LC1100HT_REG_BITMASK_CHG_INPUT_STATE ? 
			CHARGER_STATE_CHARGER_PLUG : CHARGER_STATE_CHARGER_UNPLUG; 
}

static int lc1100ht_battery_adc2voltage(u16 data)
{
#if 1
	//int temp  = (int)((((int)data * 10 + 5 ) * 5500) / 40950);
	//dbg("%s: %d\n", __func__, temp);
	//return temp;
	int temp = 1221 * (((int)data  * 1000 + 500) / 1000);
	dbg("%s: %d\n", __func__, temp);
	return temp / 1000;
#else
	int temp  = (int)((((int)data * 10 + 5) * 6000) / 19656);
	dbg("Voltage: %d\n", temp);
	temp += 95; //1  For different hardware board
	if (data == 0) {
		WARN(1, "Why data zero!!\n");
	}
	return temp;
#endif
}

static int lc1100ht_battery_adc2vbus(u16 data)
{
	int temp  = (int)((((int)data * 10 + 5) * 6000) / 19656);
	dbg("Voltage: %d\n", temp);
	return temp;
}

static int lc1100ht_battery_adc2charging(u16 data)
{
	int temp  = (int)((((int)data * 10 + 5 ) * 2500) / 40950);
	dbg("%s: %d\n", __func__, temp);
	return temp;
}

static int lc1100ht_battery_adc2discharging(u16 data)
{
	int temp  = (int)((((int)data * 10 + 5 ) * 2500) / 40950);
	dbg("%s: %d\n", __func__, temp);
	return temp;
}

static temp_curve_sample curve_temp[] = {
	[0] ={-20,7037},[1] ={-16,5780},[2] ={-12,4772},[3] ={-8,3959},	[4] ={-4,3300},
	[5] ={0,2764},	[6] ={4,2324},	[7] ={8,1963},	[8] ={12,1664},	[9] ={16,1417},
	[10]={20,1211},	[11]={24,1038},	[12]={28,894},	[13]={32,773},	[14]={36,670},
	[15]={40,583},	[16]={44,508},	[17]={48,445},	[18]={52,390},	[19]={56,343},
	[20]={60,303},	[21]={64,268},	[22]={68,237},	[23]={72,210},	[24]={76,187},
	[25]={80,167},
};

static int __lc1100ht_battery_vol2temperature(int rtem, struct temp_curve_sample *curve_temp, int curve_num)
{
	int i = 0;
	int temp = 0;

	if (rtem > 10000 || rtem < 100) {
		dbg("Over resistance limit\n");
		return -EINVAL;
	}

	if (rtem >= curve_temp[0].adc_res) {
		temp = curve_temp[0].temp;
	} else if (rtem <= curve_temp[curve_num - 1].adc_res) {
		temp = curve_temp[curve_num - 1].temp;
	} else {
		for (i = 0; i <= curve_num - 2; i++) {
			if (rtem >= curve_temp[i].adc_res)
				break;
		}
		/* linear interpolation*/
		temp = (curve_temp[i - 1].temp - curve_temp[i].temp) * (rtem - curve_temp[i].adc_res) / (curve_temp[i - 1].adc_res - curve_temp[i].adc_res) + curve_temp[i].temp;
	}

	return temp;
}

static int lc1100ht_battery_adc2temperature(u16 data)
{
	int temp_vol = 0;
	int temp_rs = 0;
	int temp_t = 0;

	temp_vol = (int)((((int)data * 10 + 5 ) * 2500) / 40950);
	dbg("temp_vol: [%d]\n", temp_vol);
	temp_rs = (100 * 1500 * temp_vol ) / (250000 - 115 * temp_vol); /* resistance formula: Rx = (1500 * vt) / (250 - 115vt) the unit of vt is v,not mv */
	dbg("temp_rs: [%d]\n", temp_rs);
	temp_t = __lc1100ht_battery_vol2temperature(temp_rs, curve_temp, ARRAYSIZE(curve_temp));
	dbg("temp_t: [%d]\n", temp_t);

	return temp_t;
}

static int lc1100ht_battery_battery_state_get(void)
{
	u8 state = 0x00;
	__lc1100ht_reg_read(LC1100HT_REG_STATE_1, &state);
	if (state & LC1100HT_REG_BITMASK_NO_BATT) {
		dbg("No battery.\n");
		return BATTERY_STATE_UNPACKED;
	} else if (state & LC1100HT_REG_BITMASK_BAD_BATT) {
		dbg("Detect bad battery.\n");
		return BATTERY_STATE_UNPACKED;
	}
	return BATTERY_STATE_PACKED;
}

static int lc1100ht_battery_charging_state_get(void)
{
	u8 state = 0;
	u8 chg_state;
	if (__lc1100ht_reg_read(LC1100HT_REG_STATE_1, &state))
		goto out;
	chg_state = state & LC1100HT_REG_BITMASK_CHG_STATE;
	chg_state >>= lc1100ht_bitmask2bitstart(LC1100HT_REG_BITMASK_CHG_STATE);
	switch (chg_state) {
		case LC1100HT_CHG_STATE_PRECHARGE:
		case LC1100HT_CHG_STATE_CHARGE_CC:
		case LC1100HT_CHG_STATE_CHARGE_CV:
			return CHARGING_STATE_CHARGING;
		default:
			return CHARGING_STATE_DISCHARGING;
	}
out:
	return CHARGING_STATE_UNKNOW;
}

static int lc1100ht_battery_charging_state_set(int onoff)
{
	dbg("User set charger to %s\n", onoff ? "auto mode" : "stop mode");
	if (onoff) {
		/* Enable charger */
		return lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_CTL, 
										LC1100HT_REG_BITMASK_CHARGER_EN, 0x01);
	} else {
		/* Disable charger */
		return lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_CTL, 
										LC1100HT_REG_BITMASK_CHARGER_EN, 0x00);
	}
	return 0;
}

static int low_voltage_tb[] = {
	3100, 3150, 3200, 3250,
	3300, 3350, 3400, 3450,
	3500, 3555, 3600, 3650,
	3700, 3750, 3800, 3850
};
static inline int lc1100ht_reg_2_low_voltage(int reg)
{
	if (reg > 0x0f)
		return -EINVAL;
	return low_voltage_tb[reg];
}

static int lc1100ht_get_battery_warning_level(int adjust)
{
	u8 val = 0x00; int voltage;
	int ret = __lc1100ht_reg_read(LC1100HT_REG_LOW_BATTERY_THRESHOLD, &val);
	if (ret < 0)
		return ret;
	val &= LC1100HT_REG_BITMASK_BATT_LOW_FALL;
	voltage = lc1100ht_reg_2_low_voltage(val);
	printk("Low battery with %d\n", voltage);

	if (!adjust)
		return voltage;

	val--;
	if (val == 0 || val > 0x0f) {
		printk("Try adjust low battery warning failed(%d).\n", val);
		/* Maybe current warning level is lowest, so disable detect */
		//1TODO: How to enable battery warning again? Maybe not need dipose, because the lowest voltage is 3100. current not be reach
		lc1100ht_reg_bit_clr(LC1100HT_REG_LOW_BATTERY_DET_EN,
								LC1100HT_REG_BITMASK_LOW_BATT_ENABLE);
	} else {
		lc1100ht_reg_bit_write(LC1100HT_REG_LOW_BATTERY_THRESHOLD,
								LC1100HT_REG_BITMASK_BATT_LOW_FALL, val);
	}
	return voltage;
}

static u32 lc1100ht_battery_int2event(u32 int_status)
{
	u32 status = 0;
	dbg("int_status = 0x%08x\n", int_status);

	if (int_status & LC1100HT_REG_BITMASK_TSDL_INT) {
		BATTERY_LC1100HT_PRINTKA("Chip temperature reached warning level\n");
	}

	if (int_status & LC1100HT_REG_BITMASK_TSDH_INT) {
		BATTERY_LC1100HT_PRINTKA("Chip temperature reached critical level\n");
	}

	if (int_status & LC1100HT_REG_BITMASK_UVLO_INT) {
		BATTERY_LC1100HT_PRINTKA("System supply has dropped below UVLO level\n");
	}

	int_status >>= 8;
	if (int_status & LC1100HT_REG_BITMASK_CHG_INPUT_STATE_INT) {
		int charger_state = lc1100ht_battery_charger_state_get();
		BATTERY_LC1100HT_PRINTKA("Charger plug %s\n",
			CHARGER_STATE_CHARGER_PLUG == charger_state ? "in" : "out");
		status |= 
			CHARGER_STATE_CHARGER_PLUG == charger_state ?
			COMIP_BATTERY_EVENT_CHG_PLUG : COMIP_BATTERY_EVENT_CHG_UNPLUG;
		if (CHARGER_STATE_CHARGER_PLUG == charger_state) {
			/* Enable EOC interrupter */
			lc1100ht_reg_bit_set(LC1100HT_REG_INT_EN_2, LC1100HT_REG_BITMASK_EOC_INT_EN);
		} else {
			/* Enable auto PRSW */
			#ifdef LC1100HT_PRSW
			cancel_delayed_work(&switch_prsw);
			lc1100ht_set_auto_prsw(1);
			#endif
		}
	}

	if (int_status & LC1100HT_REG_BITMASK_CHG_STATE_INT) {
		BATTERY_LC1100HT_PRINTKA("Charging state changed\n");
	}

	if (int_status & LC1100HT_REG_BITMASK_EOC_INT) {
		BATTERY_LC1100HT_PRINTKA("Handle charging completed interrupter\n");
		status |= COMIP_BATTERY_EVENT_CHARGE_COMPLETED;
		/* Disable EOC interrupter */
		lc1100ht_reg_bit_clr(LC1100HT_REG_INT_EN_2, LC1100HT_REG_BITMASK_EOC_INT);
	}

	if (int_status & LC1100HT_REG_BITMASK_CHG_RESTART_INT) {
		BATTERY_LC1100HT_PRINTKA("Charger restart\n");
	}

	if (int_status & LC1100HT_REG_BITMASK_RESTART_TIMEOUT_INT) {
		BATTERY_LC1100HT_PRINTKA("Charger restart timeout\n");
	}

	if (int_status & LC1100HT_REG_BITMASK_FULLCHG_TIMEOUT_INT) {
		BATTERY_LC1100HT_PRINTKA("Charge full timeout\n");
	}

	if (int_status & LC1100HT_REG_BITMASK_PRECHG_TIMEOUT_INT) {
		BATTERY_LC1100HT_PRINTKA("Pre-charge timeout\n");
	}

	int_status >>= 8;
	if (int_status & LC1100HT_REG_BITMASK_ENTER_SYS_SUPPORT_INT) {
		#ifdef LC1100HT_PRSW
		BATTERY_LC1100HT_PRINTKA("PRSW closed -- enter system support mode\n");
		lc1100ht_set_auto_prsw(0);
		schedule_delayed_work(&switch_prsw, msecs_to_jiffies(LC1100HT_PRSW_SWITCH_TIME));
		#endif
	}

	if (int_status & LC1100HT_REG_BITMASK_EXIT_SYS_SUPPORT_INT_EN) {
		#ifdef LC1100HT_PRSW
		BATTERY_LC1100HT_PRINTKA("PRSW open -- exit system support mode\n");
		#endif
	}

	if (int_status & LC1100HT_REG_BITMASK_BATT_LOW_INT) {
		BATTERY_LC1100HT_PRINTKA("Battery low\n");
		status |= COMIP_BATTERY_EVENT_BATTERY_LOW;
	}

	if (int_status & LC1100HT_REG_BITMASK_NO_BATT_INT) {
		BATTERY_LC1100HT_PRINTKA("No battery\n");
	}

	return status;
}

static int __lc1100ht_set_charging_current(int val)
{
	int ret = 0;
	int hex = val / 50;

	if (val > 1200) {
		dbg("Over current limit\n");
		return -EINVAL;
	}

	hex -= 1;
	hex = (hex < 1 ? 1 : hex);
	dbg("Target %d - hex %d\n", val, hex);
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_CHARGER_INPUT_CURRENT_LIMIT,
					LC1100HT_REG_BITMASK_IDCIN, hex);
	ret |= lc1100ht_reg_bit_write(LC1100HT_REG_BATTERY_CHARGING_CURRENT_SELECT,
					LC1100HT_REG_BITMASK_IBATT, hex - 1); /* ibat less than ibus 50ma*/
	return ret;
}

static int lc1100ht_battery_set_charging_current(int old, int new)
{
	const int STEP = 50;
	int diff = new - old;
	int target = 0;
	dbg("old-%d, new-%d, diff-%d\n", old, new, diff);

	if (diff > 0) {
		target = diff > STEP ? old + STEP : new;
	} else if (diff < 0) {
		target = new;
	} else {
		return 0;
	}
	__lc1100ht_set_charging_current(target);
	if (diff > 0) {
		u16 vbus[3]; int i; int sum = 0; int valid = 0;
		int voltage;
		for (i = 0; i < 3; i++) {
			int ret = lc1100ht_read_adc(LC1100HT_ADC_VBUS, &vbus[i]);
			if (ret < 0)
				continue;
			sum += vbus[i];
			valid++;
		}
		if (valid != 0) {
			vbus[0] = sum / valid;
			voltage = lc1100ht_battery_adc2vbus(vbus[0]);
			printk("%s: VBus voltage %d\n", __func__, voltage);
			if (voltage < 4400) {
				printk("%s: Detect vBus drop low(%d:%d)\n",
							__func__, voltage, target);
				target -= STEP;
				if (target < 0) {
					printk("Why target error?\n");
					return 0;
				}
				__lc1100ht_set_charging_current(target);
				return 0;
			}
		}
	}
	return target == new ? 0 : target;
}

static int lc1100ht_battery_suspend(void)
{
#ifdef LC1100HT_PRSW
	/* Switch PRSW to auto mode? */
	cancel_delayed_work(&switch_prsw);
	lc1100ht_set_auto_prsw(1);
#endif
	return 0;
}

static int lc1100ht_start_plug_debounce(int (*hand_plug_debouce)(int plug_mode))
{
	BATTERY_LC1100HT_PRINTKA("lc1100ht_start_plug_debounce_timer\n");
	lc1100ht_set_charger_plug_interrupt(0);
	hrtimer_start(&charger_interrupt_timer, 
				ktime_set(CHARGER_INTERRUPT_DETECT_TIME, 0), HRTIMER_MODE_REL);
	lc1100ht_debounce.lc1100ht_hand_plug_debouce = hand_plug_debouce;

	return 0;
}

struct comip_battery_hw_ops lc1100ht_battery_ops = {
	.name = "lc1100ht",
	.adc_support_type = COMIP_BATTERY_ADC_SUPPORT_BAT_V	|
						COMIP_BATTERY_ADC_SUPPORT_TEMP	|
						COMIP_BATTERY_ADC_SUPPORT_CHG_I,
	.probe = lc1100ht_battery_probe,
	.remove= lc1100ht_battery_remove,
	.suspend = lc1100ht_battery_suspend,
	.reg_write = __lc1100ht_reg_write,
	.reg_read = __lc1100ht_reg_read,
	.int2event = lc1100ht_battery_int2event,
	.adc_acquire = lc1100ht_battery_adc_acquire,
	.charger_state_get = lc1100ht_battery_charger_state_get,
	.adc2vbus = lc1100ht_battery_adc2vbus,
	.adc2voltage = lc1100ht_battery_adc2voltage,
	.adc2charging = lc1100ht_battery_adc2charging,
	.adc2discharging = lc1100ht_battery_adc2discharging,
	.adc2temperature = lc1100ht_battery_adc2temperature,
	.charging_state_get = lc1100ht_battery_charging_state_get,
	.charging_state_set = lc1100ht_battery_charging_state_set,
	.battery_state_get = lc1100ht_battery_battery_state_get,
	.set_charging_current = lc1100ht_battery_set_charging_current,
	.get_battery_warning_level = lc1100ht_get_battery_warning_level,
	.start_plug_debounce = lc1100ht_start_plug_debounce,
};

late_initcall_sync(comip_battery_init);
module_exit(comip_battery_exit);

MODULE_AUTHOR("Wangzexiang <wangzexiang@leadcoretech.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery driver for the COMIP board");

