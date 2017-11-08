#ifndef _COMIP_LEDS_SINK_H
#define _COMIP_LEDS_SINK_H

#include <linux/leds.h>

struct sink_led {
	const char *name;
	unsigned 	sink_no;
       unsigned	default_state;
	void		(*sink_brightness_set)(struct led_classdev *led_cdev,
					  enum led_brightness brightness);
};

struct sink_led_platform_data {
	int 		num_leds;
	const struct sink_led *leds;

};


#endif
