#ifndef __COMIP_BACKLIGHT_H__
#define __COMIP_BACKLIGHT_H__

enum{
	CTRL_PMU =0
	,CTRL_PWM
	,CTRL_LCDC
	,CTRL_PULSE
	,CTRL_EXTERNAL
};

struct bright_pulse{
	uint32_t bright;		//upper margin of brightless range
	uint8_t pulse_cnt;		//pulse cnt
};

struct comip_backlight_platform_data {
	int gpio_en;
	int ctrl_type;
	int pwm_en;
	int pwm_clk;
	int pwm_id;
	int pwm_ocpy_min;
	int pwm_ocpy_max;
	int pulse_gpio;
	struct bright_pulse *ranges;
	int range_cnt;
	void (*bl_control)(int onoff);
	void (*key_bl_control)(int onoff);
	void (*bl_set_external)(unsigned long brightness);
};

#define HZ_TO_NS(x) (1000000000UL / (x))

#endif /*__COMIP_BACKLIGHT_H__*/
