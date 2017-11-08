#ifndef __COMIP_MONITOR__
#define __COMIP_MONITOR__

#include <linux/hrtimer.h>

#ifdef CONFIG_MONITOR_COMIP
extern struct hrtimer *monitor_timer_register(const char *name);
extern int monitor_timer_unregister(const char *name);
extern int monitor_timer_start(struct hrtimer *hrtimer, long secs);
extern int monitor_timer_stop(struct hrtimer *hrtimer);
#else
static inline struct hrtimer *monitor_timer_register(const char *name)
{
	return NULL;
}

static inline int monitor_timer_unregister(const char *name)
{
	return 0;
}

static inline int monitor_timer_start(struct hrtimer *hrtimer, long secs)
{
	return 0;
}

static inline int monitor_timer_stop(struct hrtimer *hrtimer)
{
	return 0;
}
#endif

#endif
