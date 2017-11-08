/* arch/arm/mach-comip/timer.c
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** Copyright (c) 2010-2019  LeadCoreTech Corp.
**
** PURPOSE: comip timer for linux time system.
**
** CHANGE HISTORY:
**
** Version	Date		Author		Description
** 1.0.0	2011-11-24	gaobing		created
**
*/

#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/sched.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/mach/time.h>
#include <asm/sched_clock.h>
#include <asm/localtimer.h>
#include <mach/timer.h>
#include <plat/clock.h>
#include <plat/hardware.h>
#include <plat/comip-timer.h>

#define DEBUG_PRINT	0
#if DEBUG_PRINT
#define TIMER_PR(fmt, args...)	printk(KERN_INFO fmt, ## args)
#else
#define TIMER_PR(fmt, args...)	printk(KERN_DEBUG fmt, ## args)
#endif

#define TIMER_CYCLES_MIN	10

#if defined(CONFIG_TIMER_DEBUG) || defined(CONFIG_IRQ_MON_DEBUG)
#define TIMER_TRY_COUNT		10
#define DEBUG_TIMER_ID		0
#define DEBUG_TIMER_IRQ		INT_TIMER0
#define DEBUG_TIMER_CLKSRC	"timer0_clk"
#define DEBUG_TIMER_PCLKSRC	"pll1_mclk"
#define DELTA_CYCLES		(CLOCK_TICK_RATE * 2) /*2s*/
#endif

struct comip_clock_timer {
	struct clk *clkevent;
	struct clk *clksrouce;
	struct clk *clkevent1;
#if defined(CONFIG_TIMER_DEBUG) || defined(CONFIG_IRQ_MON_DEBUG)
	struct clk *debug;
#endif
};

static struct comip_clock_timer comip_clock_timer = {
	.clkevent	= NULL,
	.clksrouce	= NULL,
	.clkevent1	= NULL,
#if defined(CONFIG_TIMER_DEBUG) || defined(CONFIG_IRQ_MON_DEBUG)
	.debug		= NULL,
#endif
};

#ifdef CONFIG_IRQ_MON_DEBUG
struct mon_data{
	int cpu;
	unsigned int irq_id;
	struct pt_regs *regs;
};

static struct mon_data irq_mon_data = {-1, -1, NULL};
#endif

#ifdef CONFIG_COMIP_USE_WK_TIMER
static void __init comip_init_wktimer(void);
#endif
static int timer_clk_set(char *timer_clk,
			char *parant_clk,
			unsigned long rate,
			struct clk **value);

static inline int timer_cycles_set(int timer_id,
			unsigned long cycles);

#ifdef CONFIG_IRQ_MON_DEBUG
static inline void irq_timer_stop(int cpu)
{
	comip_timer_stop(DEBUG_TIMER_ID);
	comip_timer_int_disable(DEBUG_TIMER_ID);
}

static inline void irq_timer_start(int cpu, unsigned long cycles)
{
	comip_timer_stop(DEBUG_TIMER_ID);
	comip_timer_int_enable(DEBUG_TIMER_ID);
	timer_cycles_set(DEBUG_TIMER_ID, cycles);
}

static void irq_monitor_show(struct mon_data *data)
{
	if (-1 == irq_mon_data.irq_id)
		return;

	printk("irq:%d is expired\n", irq_mon_data.irq_id);
	show_stack_frame(irq_mon_data.regs);
	panic("irq:%d is expired\n", irq_mon_data.irq_id);
}

static irqreturn_t irq_timer_irq_handle(int irq, void *dev_id)
{
	irq_timer_stop(0);
	irq_monitor_show(NULL);
	return IRQ_HANDLED;
}

static struct irqaction irq_timer_irq = {
	.name		= "irq timer",
	.flags		= IRQF_DISABLED | IRQF_IRQPOLL,
	.handler	= irq_timer_irq_handle,
	.irq		= DEBUG_TIMER_IRQ,
};

static void irq_timer_init(unsigned long rate)
{
	struct comip_clock_timer *cct = &comip_clock_timer;
	int ret;
	static int inited = 1;

	if (inited)
		timer_clk_set(DEBUG_TIMER_CLKSRC,
				DEBUG_TIMER_PCLKSRC,
				rate, &(cct->debug));

	comip_timer_stop(DEBUG_TIMER_ID);
	comip_timer_int_enable(DEBUG_TIMER_ID);
	comip_timer_autoload_disable(DEBUG_TIMER_ID);

	irq_set_affinity(DEBUG_TIMER_IRQ, cpumask_of(1));
	if (inited) {
		ret = setup_irq(DEBUG_TIMER_IRQ, &irq_timer_irq);
		if (ret)
			printk(KERN_ERR "can't setup irq for debug timer!\n");
	}

	barrier();
	if (inited)
		inited = 0;
}

void irq_monitor_start(int cpu, unsigned int irq_id, struct pt_regs *regs)
{
	if (cpu != 0)
		return;

	irq_mon_data.irq_id = irq_id;
	irq_mon_data.regs = regs;
	irq_timer_start(cpu, CLOCK_TICK_RATE * 10); /*10s*/
}

void irq_monitor_stop(int cpu)
{
	if (cpu != 0)
		return;

	irq_timer_stop(cpu);
	irq_mon_data.irq_id = -1;
	irq_mon_data.regs = NULL;
}

#elif defined(CONFIG_TIMER_DEBUG)
void timeres_stats_show(void)
{
	TIMER_PR("timer0 regs: \n");
	TIMER_PR("TIMER0_TLC:%x ,TIMER0_TCV:%x, TIMER0_TCR:%x, TIMER0_TIS:%x \n",
		__raw_readl(io_p2v(TIMER0_TLC)),
		__raw_readl(io_p2v(TIMER0_TCV)),
		__raw_readl(io_p2v(TIMER0_TCR)),
		__raw_readl(io_p2v(TIMER0_TIS)));

	TIMER_PR("timer1 regs: \n");
	TIMER_PR("TIMER1_TLC:%x ,TIMER1_TCV:%x, TIMER1_TCR:%x, TIMER1_TIS:%x \n",
		__raw_readl(io_p2v(TIMER1_TLC)),
		__raw_readl(io_p2v(TIMER1_TCV)),
		__raw_readl(io_p2v(TIMER1_TCR)),
		__raw_readl(io_p2v(TIMER1_TIS)));

	TIMER_PR("timer2 regs: \n");
	TIMER_PR("TIMER2_TLC:%x ,TIMER2_TCV:%x, TIMER2_TCR:%x, TIMER2_TIS:%x \n",
		__raw_readl(io_p2v(TIMER2_TLC)),
		__raw_readl(io_p2v(TIMER2_TCV)),
		__raw_readl(io_p2v(TIMER2_TCR)),
		__raw_readl(io_p2v(TIMER2_TIS)));

	TIMER_PR("timer3 regs: \n");
	TIMER_PR("TIMER3_TLC:%x ,TIMER3_TCV:%x, TIMER3_TCR:%x, TIMER3_TIS:%x \n",
		__raw_readl(io_p2v(TIMER3_TLC)),
		__raw_readl(io_p2v(TIMER3_TCV)),
		__raw_readl(io_p2v(TIMER3_TCR)),
		__raw_readl(io_p2v(TIMER3_TIS)));

	TIMER_PR("timer global regs: \n");
	TIMER_PR("g_TIS:%x, g_raw_TIS:%x \n",
		__raw_readl(io_p2v(TIMER_BASE + 0xa0)),
		__raw_readl(io_p2v(TIMER_BASE + 0xa8)));
}

static inline void debug_timer_stop(void)
{
	comip_timer_stop(DEBUG_TIMER_ID);
	comip_timer_int_disable(DEBUG_TIMER_ID);
}

static inline void debug_timer_start(unsigned long cycles)
{
	if (cycles < (cycles + (unsigned long)DELTA_CYCLES)) {
		comip_timer_stop(DEBUG_TIMER_ID);
		comip_timer_int_enable(DEBUG_TIMER_ID);
		timer_cycles_set(DEBUG_TIMER_ID,
				(cycles + DELTA_CYCLES));
	}
}

static irqreturn_t debug_timer_irq_handle(int irq, void *dev_id)
{
	TIMER_PR("debug timer is expired!\n");
	debug_timer_stop();
	timeres_stats_show();
	comip_timer_stop(COMIP_CLOCKEVENT1_TIMER_ID);
	timer_cycles_set(COMIP_CLOCKEVENT1_TIMER_ID, 
				CLOCK_TICK_RATE / 10); /*100ms*/ 

	return IRQ_HANDLED;
}

static struct irqaction debug_timer_irq = {
	.name		= "debug timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= debug_timer_irq_handle,
	.irq		= DEBUG_TIMER_IRQ,
};

static void debug_timer_init(unsigned long rate)
{
	struct comip_clock_timer *cct = &comip_clock_timer;
	int ret;

	timer_clk_set(DEBUG_TIMER_CLKSRC,
			DEBUG_TIMER_PCLKSRC,
			rate, &(cct->debug));

	comip_timer_stop(DEBUG_TIMER_ID);
	comip_timer_int_enable(DEBUG_TIMER_ID);
	comip_timer_autoload_disable(DEBUG_TIMER_ID);

	ret = setup_irq(DEBUG_TIMER_IRQ, &debug_timer_irq);
	if (ret)
		printk(KERN_ERR "can't setup irq for debug timer!\n");
}

static inline void mon_timer_stop(int timer_id)
{
	if (COMIP_CLOCKEVENT1_TIMER_ID == timer_id) {
		debug_timer_stop();
	}
}
#endif

#ifndef CONFIG_TIMER_DEBUG
void timeres_stats_show(void)
{
}
#endif

static inline void timer_restart(int timer_id,
				unsigned long cycles) 
{
	comip_timer_stop(timer_id);
	comip_timer_load_value_set(timer_id, cycles);
	comip_timer_start(timer_id);
}

#ifdef CONFIG_TIMER_DEBUG
static inline int timer_cycles_set(int timer_id,
				unsigned long cycles) 
{
	unsigned long value;
	unsigned long wait_cnt;
	unsigned long retry_cnt = TIMER_TRY_COUNT;

	if (unlikely(cycles < (unsigned long)TIMER_CYCLES_MIN)) {
		TIMER_PR("timer%d cycles (%lu) is too small.\n", timer_id, cycles);
		cycles = TIMER_CYCLES_MIN;
	}

retry:
	timer_restart(timer_id, cycles);

	for (wait_cnt = TIMER_TRY_COUNT; wait_cnt > 0; --wait_cnt) {
		value = comip_timer_read(timer_id);
		if (cycles >= value) {
			break;
		}
	}

	if (unlikely(wait_cnt == 0)) {
		TIMER_PR("timer%d wait timeout.\n", timer_id);
		if (--retry_cnt  > 0)
			goto retry;
	}

	if (unlikely(retry_cnt == 0))
		TIMER_PR("timer%d retry timeout.\n", timer_id);

	return 0;
}

#else
static inline int timer_cycles_set(int timer_id,
				unsigned long cycles) 
{
	if (unlikely(cycles < (unsigned long)TIMER_CYCLES_MIN)) {
		TIMER_PR("timer%d cycles (%lu) is too small.\n", timer_id, cycles);
		cycles = TIMER_CYCLES_MIN;
	}

	timer_restart(timer_id, cycles);

	return 0;
}

#endif

static int comip_set_next_event(unsigned long cycles,
				struct clock_event_device *evt)
{
	return timer_cycles_set(COMIP_CLOCKEVENT_TIMER_ID,
				cycles);
}

static int comip_set_next_event1(unsigned long cycles,
				struct clock_event_device *evt)
{
#ifdef CONFIG_TIMER_DEBUG
	debug_timer_start(cycles);
#endif
	return timer_cycles_set(COMIP_CLOCKEVENT1_TIMER_ID,
				cycles);
}

static inline void timer_mode_set(int timer_id,
				enum clock_event_mode mode)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		comip_timer_stop(timer_id);
		comip_timer_int_enable(timer_id);
		comip_timer_autoload_enable(timer_id);
		comip_timer_load_value_set(timer_id, (CLOCK_TICK_RATE / HZ));
		comip_timer_start(timer_id);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		comip_timer_stop(timer_id);
		comip_timer_int_enable(timer_id);
		comip_timer_autoload_disable(timer_id);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		comip_timer_stop(timer_id);
		comip_timer_int_disable(timer_id);
		break;
	case CLOCK_EVT_MODE_RESUME:
		break;
	}

#ifdef CONFIG_TIMER_DEBUG
	mon_timer_stop(timer_id);
#endif

}

static void comip_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	timer_mode_set(COMIP_CLOCKEVENT_TIMER_ID, mode);
}

static void comip_set_mode1(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	timer_mode_set(COMIP_CLOCKEVENT1_TIMER_ID, mode);
}

static inline int timer_interrupt(int timer_id, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	if (CLOCK_EVT_MODE_PERIODIC == evt->mode) {
		comip_timer_int_clear(timer_id);
	} else {
		comip_timer_stop(timer_id);
	}

	if (CLOCK_EVT_MODE_PERIODIC == evt->mode || 
		CLOCK_EVT_MODE_ONESHOT == evt->mode)
		if (evt->event_handler)
			evt->event_handler(evt);

	return 0;
}

static irqreturn_t comip_clockevent_interrupt(int irq, void *dev_id)
{
	timer_interrupt(COMIP_CLOCKEVENT_TIMER_ID, dev_id);
	return IRQ_HANDLED;
}

static irqreturn_t comip_clockevent1_interrupt(int irq, void *dev_id)
{
#ifdef CONFIG_TIMER_DEBUG
	debug_timer_stop();
#endif
	timer_interrupt(COMIP_CLOCKEVENT1_TIMER_ID, dev_id);
	return IRQ_HANDLED;
}

static struct irqaction comip_clockevent_irq = {
	.name		= COMIP_CLOCKEVENT_TIMER_NAME,
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= comip_clockevent_interrupt,
	.irq		= COMIP_CLOCKEVENT_TIMER_IRQ_ID,
};

static struct irqaction comip_clockevent1_irq = {
	.name		= COMIP_CLOCKEVENT1_TIMER_NAME,
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= comip_clockevent1_interrupt,
	.irq		= COMIP_CLOCKEVENT1_TIMER_IRQ_ID,
};

static int timer_clk_set(char *timer_clk,
			char *parant_clk,
			unsigned long rate,
			struct clk **value)
{
	struct clk *parent;
	struct clk *timer;

	parent = clk_get(NULL, parant_clk);
	if (!parent)
		printk(KERN_ERR "Can't get clock : %s!\n", parant_clk);

	timer = clk_get(NULL, timer_clk);
	if (!timer)
		printk(KERN_ERR "Can't get clock : %s!\n", timer_clk);

	if (timer) {
		if (parent) {
			clk_set_parent(timer, parent);
			clk_put(parent);
		}
		clk_set_rate(timer, rate);
		clk_enable(timer);

		if (value)
			*value = timer;
	}

	return 0;
}

static struct clock_event_device gp_dev = {
	.name			= COMIP_CLOCKEVENT_TIMER_NAME,
	.features		= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.shift			= 32,
	.rating 		= 300,
	.mode		= CLOCK_EVT_MODE_UNUSED,
	.set_next_event		= comip_set_next_event,
	.set_mode		= comip_set_mode,
};

static void clk_evt_timer_init(struct clock_event_device *evt) 
{
	struct comip_timer_cfg cfg;
	unsigned long rate = CLOCK_TICK_RATE;
	int irq_id;
	struct irqaction *irq_act;
	int cpu;
	int timer_id;

	if (cpumask_equal(evt->cpumask, cpumask_of(0))) {
		irq_id = COMIP_CLOCKEVENT_TIMER_IRQ_ID;
		irq_act = &comip_clockevent_irq;
		cpu = 0;
		timer_id = COMIP_CLOCKEVENT_TIMER_ID;
	} else if (cpumask_equal(evt->cpumask, cpumask_of(1))) {
		irq_id = COMIP_CLOCKEVENT1_TIMER_IRQ_ID;
		irq_act = &comip_clockevent1_irq;
		cpu = 1;
		timer_id = COMIP_CLOCKEVENT1_TIMER_ID;
#ifdef CONFIG_IRQ_MON_DEBUG
		irq_timer_init(rate);
#endif
	} else {
		printk(KERN_ERR "clk_evt_timer_init, wrong id!\n");
		return;
	}

	cfg.autoload = 1;
	cfg.load_val = (rate / HZ);
	comip_timer_stop(timer_id);
	comip_timer_config(timer_id, &cfg);
	comip_timer_int_enable(timer_id);

	irq_act->dev_id = (void*)evt;
	irq_set_affinity(irq_id, cpumask_of(cpu));
}

static void clk_evt_dev_init(struct clock_event_device *evt)
{
	unsigned long rate = CLOCK_TICK_RATE;

	evt->shift = 32;
	evt->rating = 300;
	evt->mult = div_sc(rate, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(-1, evt);
	evt->min_delta_ns = clockevent_delta2ns(1, evt);
	evt->features =  CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;

	if (cpumask_equal(evt->cpumask, cpumask_of(0))) {
		evt->name = COMIP_CLOCKEVENT_TIMER_NAME;
		evt->set_next_event = comip_set_next_event;
		evt->set_mode = comip_set_mode;
	} else if (cpumask_equal(evt->cpumask, cpumask_of(1))) {
		evt->name = COMIP_CLOCKEVENT1_TIMER_NAME;
		evt->set_next_event = comip_set_next_event1;
		evt->set_mode = comip_set_mode1;
	} else {
		printk(KERN_ERR "clk_evt_dev_init, wrong id!\n");
		return;
	}
	
	clockevents_register_device(evt);
}

static void __init comip_init_clocksource(unsigned long rate)
{
	struct comip_clock_timer *cct = &comip_clock_timer;
	struct comip_timer_cfg cfg;

	timer_clk_set(COMIP_CLOCKSROUCE_TIMER_NAME,
			COMIP_CLOCKSROUCE_TIMER_CLKSRC,
			rate, &(cct->clksrouce));

	cfg.autoload = 1;
	cfg.load_val = ~0;
	comip_timer_stop(COMIP_CLOCKSROUCE_TIMER_ID);
	comip_timer_config(COMIP_CLOCKSROUCE_TIMER_ID, &cfg);
	comip_timer_int_disable(COMIP_CLOCKSROUCE_TIMER_ID);
	comip_timer_start(COMIP_CLOCKSROUCE_TIMER_ID);

	if (clocksource_mmio_init((void __iomem *)io_p2v(COMIP_TIMER_TCV(COMIP_CLOCKSROUCE_TIMER_ID)), \
		COMIP_CLOCKSROUCE_TIMER_NAME, rate, 300, 32, clocksource_mmio_readl_down))
		printk(KERN_ERR "Can't clocksource_mmio_init : %s!\n", COMIP_CLOCKSROUCE_TIMER_NAME);

	return;
}

static cycle_t notrace comip_32k_read(struct clocksource *cs)
{
	return readl(io_p2v(AP_PWR_TM_CUR_VAL));
}

static struct clocksource clocksource_32k = {
	.name		= "32k_counter",
	.rating		= 250,
	.read		= comip_32k_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static notrace u32 comip_sched_clock_read(void)
{
	return ~clocksource_32k.read(&clocksource_32k);
}

static int __init comip_init_clocksource_32k(void)
{
	/* enable ap_pwr clock. */
	writel(0x0705, io_p2v(AP_PWR_TM_CTL));

	if (clocksource_register_hz(&clocksource_32k, 32768))
		printk(KERN_ERR "Can't register clocksource %s\n", clocksource_32k.name);

	setup_sched_clock(comip_sched_clock_read, 32, 32768);

	return 0;
}

/**
 * read_persistent_clock -  Return time from a persistent clock.
 *
 * Reads the time from a source which isn't disabled during PM, the
 * 32k sync timer.  Convert the cycles elapsed since last read into
 * nsecs and adds to a monotonically increasing timespec.
 */
static struct timespec persistent_ts;
static cycles_t cycles, last_cycles;

void read_persistent_clock(struct timespec *ts)
{
	unsigned long long nsecs;
	cycles_t delta;
	struct timespec *tsp = &persistent_ts;

	last_cycles = cycles;
	cycles = clocksource_32k.read(&clocksource_32k);
	delta = last_cycles - cycles;

	nsecs = clocksource_cyc2ns(delta,
			clocksource_32k.mult, clocksource_32k.shift);

	timespec_add_ns(tsp, nsecs);
	*ts = *tsp;
}

static void inline timer_sysclk_sel(void)
{
	unsigned int val = readl(io_p2v(AP_PWR_SLPCTL1));
	val &= ~(1 << 0);
	writel(val, io_p2v(AP_PWR_SLPCTL1));
}

static __init void timer_setup_pre(void)
{
	struct comip_clock_timer *cct = &comip_clock_timer;
	unsigned long rate = CLOCK_TICK_RATE;

	timer_clk_set(COMIP_CLOCKEVENT_TIMER_NAME,
			COMIP_CLOCKEVENT_TIMER_CLKSRC, 
			rate, &cct->clkevent);

	timer_clk_set(COMIP_CLOCKEVENT1_TIMER_NAME,
			COMIP_CLOCKEVENT1_TIMER_CLKSRC, 
			rate, &cct->clkevent1);

	if (setup_irq(COMIP_CLOCKEVENT_TIMER_IRQ_ID, &comip_clockevent_irq))
			printk(KERN_ERR "can't setup irq %d!\n", COMIP_CLOCKEVENT_TIMER_IRQ_ID);

	if (setup_irq(COMIP_CLOCKEVENT1_TIMER_IRQ_ID, &comip_clockevent1_irq))
			printk(KERN_ERR "can't setup irq %d!\n", COMIP_CLOCKEVENT1_TIMER_IRQ_ID);
}

static int  timer_setup_do(struct clock_event_device *evt) 
{
	clk_evt_timer_init(evt);
	clk_evt_dev_init(evt);
	return 0;
}

static int  timer_setup(struct clock_event_device *evt) 
{
	if (cpumask_equal(evt->cpumask, cpumask_of(0)))
		return 0;

	return timer_setup_do(evt);
}

static void  timer_stop(struct clock_event_device *evt) 
{
	evt->set_mode(CLOCK_EVT_MODE_SHUTDOWN, evt);
	return;
}

static struct local_timer_ops timer_ops = {
	.setup = timer_setup,
	.stop = timer_stop,
};

static void __init comip_timer_setup(void)
{
	struct clock_event_device *evt = &gp_dev;

	timer_setup_pre();
	evt->cpumask = cpumask_of(0);
	timer_setup_do(evt);
}

static void __init comip_timer_init(void)
{
	unsigned long rate = CLOCK_TICK_RATE;

	timer_sysclk_sel();
	comip_init_clocksource_32k();
	comip_init_clocksource(rate);
	#ifdef CONFIG_COMIP_USE_WK_TIMER
	comip_init_wktimer();
	#endif

#ifdef CONFIG_TIMER_DEBUG
	debug_timer_init(rate);
#endif
	comip_timer_setup();
	local_timer_register(&timer_ops);
}

#ifdef CONFIG_PM
static void comip_timer_suspend(void)
{
	struct comip_clock_timer *cct = &comip_clock_timer;

	if (cct->clkevent) {
		comip_timer_stop(COMIP_CLOCKEVENT_TIMER_ID);
		clk_disable(cct->clkevent);
	}

	if (cct->clkevent1) {
		comip_timer_stop(COMIP_CLOCKEVENT1_TIMER_ID);
		clk_disable(cct->clkevent1);
	}

	if (cct->clksrouce) {
		comip_timer_stop(COMIP_CLOCKSROUCE_TIMER_ID);
		clk_disable(cct->clksrouce);
	}
}

static void comip_timer_resume(void)
{
	struct comip_clock_timer *cct = &comip_clock_timer;

	if (cct->clksrouce) {
		clk_enable(cct->clksrouce);
		comip_timer_start(COMIP_CLOCKSROUCE_TIMER_ID);
	}

	if (cct->clkevent1) {
		clk_enable(cct->clkevent1);
		comip_timer_start(COMIP_CLOCKEVENT1_TIMER_ID);
	}

	if (cct->clkevent) {
		clk_enable(cct->clkevent);
		comip_timer_start(COMIP_CLOCKEVENT_TIMER_ID);
	}
}
#else
#define comip_timer_suspend NULL
#define comip_timer_resume NULL
#endif

struct sys_timer comip_timer = {
	.init		= comip_timer_init,
	.suspend	= comip_timer_suspend,
	.resume		= comip_timer_resume,
};

#ifdef CONFIG_COMIP_USE_WK_TIMER
static irqreturn_t comip_lp2wake_interrupt(int irq, void *dev_id)
{
	comip_timer_int_clear(COMIP_CLOCKEVENT_WKUP_TIMER_ID);
	return IRQ_HANDLED;
}

static struct irqaction comip_lp2wake_irq = {
	.name		= "timer_lp2wake",
	.flags		= IRQF_DISABLED,
	.handler	= comip_lp2wake_interrupt,
	.dev_id		= NULL,
	.irq		= INT_TIMER3,
};

static void __init comip_init_wktimer(void)
{
	struct comip_timer_cfg cfg;
	int ret;

	ret = setup_irq(comip_lp2wake_irq.irq, &comip_lp2wake_irq);
	if (ret) {
		printk(KERN_ERR "Failed to register LP2 timer IRQ: %d\n", ret);
		BUG();
	}
	
	cfg.autoload = 0;
	cfg.load_val = 0;
	comip_timer_stop(COMIP_CLOCKEVENT_WKUP_TIMER_ID);
	comip_timer_config(COMIP_CLOCKEVENT_WKUP_TIMER_ID, &cfg);
	comip_timer_int_enable(COMIP_CLOCKEVENT_WKUP_TIMER_ID);
}

void comip_lp2_set_trigger(unsigned long cycles)
{
	comip_timer_stop(COMIP_CLOCKEVENT_WKUP_TIMER_ID);
	comip_timer_load_value_set(COMIP_CLOCKEVENT_WKUP_TIMER_ID, cycles);
	if(cycles)
		comip_timer_start(COMIP_CLOCKEVENT_WKUP_TIMER_ID);
	else
		comip_timer_int_clear(COMIP_CLOCKEVENT_WKUP_TIMER_ID);
}
EXPORT_SYMBOL(comip_lp2_set_trigger);

unsigned long comip_lp2_timer_remain(void)
{
	return comip_timer_read(COMIP_CLOCKEVENT_WKUP_TIMER_ID);
}
#endif

