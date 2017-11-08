/*
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
** 1.0.0	2013-11-24	zhaoyuan	created
**
*/

#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/reboot.h>
#include <linux/delay.h>

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

#define TIMER_CYCLES_MIN		30
#define TIMER_INT_CYCLES_MIN		8 /*1000/13*8=614ns*/

#define SHADOW_TIMER_ID			6
#define SHADOW_TIMER_IRQ_ID		(INT_TIMER6)
#define SHADOW_TIMER_CYCLE_DELTA	((CLOCK_TICK_RATE / 1000) * 500)  /* 500 ms cycles*/

#define TIMER_RST_BIT_OFFSET		5

#define TIMER_RESET_MAGIC		0xdeaddead

#define ACT_FLAG_SET(id) { \
	.flags	= IRQF_DISABLED | IRQF_TIMER \
		| IRQF_IRQPOLL | IRQF_NOBALANCING | IRQF_PERCPU, \
	.irq	= INT_TIMER##id, \
}


#define RESOURCE_SET(id) [id] = { \
	.name	= "timer"#id"_clk", \
	 .lock	= __SPIN_LOCK_INITIALIZER(timer##id_lock), \
	 .act	= ACT_FLAG_SET(id), \
}

struct event_resourc {
	const char		*name;
	struct clk		*clock;
	struct irqaction	act;
	spinlock_t		lock;
};

static const void __iomem *clksrc_reg_va[] = {
	io_p2v(AP_PWR_TIMER0CLKCTL),
	io_p2v(AP_PWR_TIMER1CLKCTL),
	io_p2v(AP_PWR_TIMER2CLKCTL),
	io_p2v(AP_PWR_TIMER3CLKCTL),
	io_p2v(AP_PWR_TIMER4CLKCTL),
	io_p2v(AP_PWR_TIMER5CLKCTL),
	io_p2v(AP_PWR_TIMER6CLKCTL),
	io_p2v(AP_PWR_TIMER7CLKCTL),
};

static struct event_resourc event_res[] = {
	RESOURCE_SET(0),
	RESOURCE_SET(1),
	RESOURCE_SET(2),
	RESOURCE_SET(3),
	RESOURCE_SET(4),
	RESOURCE_SET(5),
	RESOURCE_SET(6),
	RESOURCE_SET(7),
};

static unsigned int cpu_timer_id[] = {
	COMIP_CLOCKEVENT_TIMER_ID,
	COMIP_CLOCKEVENT1_TIMER_ID,
	COMIP_CLOCKEVENT2_TIMER_ID,
	COMIP_CLOCKEVENT3_TIMER_ID,
	COMIP_CLOCKEVENT5_TIMER_ID,
};

static unsigned long shadow_timer_delta_max = -1;

static unsigned int
timer_clk_disable(unsigned int timer_id)
{
	void __iomem *va;
	unsigned int val, val_orig;

	va = (void __iomem *)clksrc_reg_va[timer_id];
	val_orig = val = __raw_readl(va);
	val &= ~(7 << 28);
	val |= 6 << 28;
	__raw_writel(val, va);
	dsb();
	return val_orig;
}

static inline void
timer_clk_enable(unsigned int timer_id,
		unsigned int val_orig)
{
	__raw_writel(val_orig,
		(void __iomem *)clksrc_reg_va[timer_id]);
	dsb();
}

static inline void timer_reset(unsigned int timer_id)
{
	writel(1 << (TIMER_RST_BIT_OFFSET + timer_id),
		(void __iomem *)io_p2v(AP_PWR_MOD_RSTCTL3));
	dsb();
}

static inline int timer_irq_is_pending(unsigned int timer_id)
{
	unsigned int status;

	if (timer_id == 7) {
		status = readl((void __iomem *)io_p2v(AP_PWR_INTR_FLAG0));
		status &= 1 << 9;
	} else {
		status = readl((void __iomem *)io_p2v(AP_PWR_INTR_FLAG1));
		status &= 1 << (timer_id + 11);
	}

	return !!status;
}

static inline void timer_stop_special(unsigned int timer_id)
{
	unsigned int now;
	int retry = 10;

	now = comip_timer_enabled(timer_id);
	/* if timer enabled ? */
	if (unlikely(!(now & TIMER_TES)))
		return;
	/* if timer irq enabled ? */
	if (now & TIMER_TIM)
		goto out;
	/*timer irq enabled */
	now = comip_timer_read(timer_id);
	if (unlikely(now <= TIMER_INT_CYCLES_MIN)) {
		/*Wait for timer int */
		while(retry--) {
			if (comip_timer_read(timer_id) > TIMER_INT_CYCLES_MIN) {
				if (comip_timer_int_status(timer_id)) {
					comip_timer_int_clear(timer_id);
					dsb();
				} else
					TIMER_PR("No irq checked for timer%d\n", timer_id);

				break;
			}
			ndelay(80);
		}
		if (unlikely(retry < 0))
			TIMER_PR("timer%d wait irq retry timeout\n", timer_id);
	}
out:
	comip_timer_stop(timer_id);
}

static inline void timer_reset_special(unsigned int timer_id)
{
	int n = 50;

	/*
	 * 1) stop timer & disable timer-interrupt & TMS==0;
	 * 2) reset timer;
	 * 3) assure (TLC != 0);
	 * 4) start timer;
	 * 5) assure (TCV != 0);
	 * 6) stop timer;
	 */
	comip_timer_tcr_set(timer_id, TIMER_TIM);
	timer_reset(timer_id);
	comip_timer_load_value_set(timer_id, TIMER_RESET_MAGIC);
	comip_timer_start(timer_id);
	while (n-- > 0) {
		if (comip_timer_read(timer_id) != 0)
			break;
		cpu_relax();
	}
	comip_timer_stop(timer_id);
}

static inline void interrupt_status_clear_special(unsigned int timer_id)
{
	unsigned int tcr;
	unsigned int tlc;

	if (likely(comip_timer_int_status(timer_id)))
		comip_timer_int_clear(timer_id);
	else if (timer_irq_is_pending(timer_id)) {
			tcr = comip_timer_enabled(timer_id);
			if (tcr & TIMER_TMS) {
				tlc = comip_timer_tlc_read(timer_id);
				timer_reset_special(timer_id);
				comip_timer_load_value_set(timer_id, tlc);
				tcr &= TIMER_TMS | TIMER_TIM | TIMER_TES;
			} else {
				timer_reset_special(timer_id);
				tcr &= TIMER_TMS | TIMER_TIM;
			}
			comip_timer_tcr_set(timer_id, tcr);
			TIMER_PR("Warnning: timer%d int-excep\n", timer_id);
		}
}

static inline void timer_restart(unsigned int timer_id,
				unsigned long cycles) 
{
	unsigned long flag;

	spin_lock_irqsave(&event_res[timer_id].lock, flag);
	timer_stop_special(timer_id);
	comip_timer_load_value_set(timer_id, cycles);
	comip_timer_start(timer_id);
	spin_unlock_irqrestore(&event_res[timer_id].lock, flag);
}

static void mode_set_periodic(unsigned int timer_id)
{
	unsigned long flag;

	spin_lock_irqsave(&event_res[timer_id].lock, flag);
	timer_stop_special(timer_id);
	comip_timer_int_enable(timer_id);
	comip_timer_autoload_enable(timer_id);
	comip_timer_load_value_set(timer_id,
			(CLOCK_TICK_RATE / HZ));
	comip_timer_start(timer_id);
	spin_unlock_irqrestore(&event_res[timer_id].lock, flag);
}

static void mode_set_oneshot(unsigned int timer_id)
{
	unsigned long flag;

	spin_lock_irqsave(&event_res[timer_id].lock, flag);
	timer_stop_special(timer_id);
	comip_timer_int_enable(timer_id);
	comip_timer_autoload_disable(timer_id);
	spin_unlock_irqrestore(&event_res[timer_id].lock, flag);
}

static void mode_set_shutdown(unsigned int timer_id)
{
	unsigned long flag;

	spin_lock_irqsave(&event_res[timer_id].lock, flag);
	timer_stop_special(timer_id);
	comip_timer_int_disable(timer_id);
	spin_unlock_irqrestore(&event_res[timer_id].lock, flag);
}

static inline void timer_mode_set(unsigned int timer_id,
				enum clock_event_mode mode)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		mode_set_periodic(timer_id);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		mode_set_oneshot(timer_id);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		mode_set_shutdown(timer_id);
		break;
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static inline int timer_cycles_set(int cpu,
				unsigned long cycles)
{
	unsigned int timer_id = cpu_timer_id[cpu];

	if (unlikely(cycles < (unsigned long)TIMER_CYCLES_MIN))
		cycles = TIMER_CYCLES_MIN;

	if (0 == cpu) {
		if (cycles > shadow_timer_delta_max)
			cycles = shadow_timer_delta_max;

		timer_restart(SHADOW_TIMER_ID,
				cycles + SHADOW_TIMER_CYCLE_DELTA);
	}
	timer_restart(timer_id, cycles);
	return 0;
}

static int event_set_next_event(unsigned long cycles,
				struct clock_event_device *evt)
{
	int cpu = cpumask_first(evt->cpumask);

	timer_cycles_set(cpu, cycles);
	return 0;
}

static void event_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	int cpu = cpumask_first(evt->cpumask);
	unsigned int timer_id = cpu_timer_id[cpu];

	timer_mode_set(timer_id, mode);
	if (0 == cpu) {
		timer_mode_set(SHADOW_TIMER_ID, mode);
		if (CLOCK_EVT_MODE_ONESHOT == mode)
			timer_restart(SHADOW_TIMER_ID,
				SHADOW_TIMER_CYCLE_DELTA);
	}
}

static inline int timer_interrupt(unsigned int cpu,
	unsigned int timer_id, struct clock_event_device *evt)
{
	unsigned long flag;

	spin_lock_irqsave(&event_res[timer_id].lock, flag);
	interrupt_status_clear_special(timer_id);
	spin_unlock_irqrestore(&event_res[timer_id].lock, flag);

	if (0 == cpu)
		timer_restart(SHADOW_TIMER_ID,
				SHADOW_TIMER_CYCLE_DELTA);

	if (evt->event_handler)
		evt->event_handler(evt);

	return 0;
}

static irqreturn_t clockevent_interrupt_cb(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	int cpu = cpumask_first(evt->cpumask);
	unsigned int timer_id = cpu_timer_id[cpu];

	BUG_ON(smp_processor_id() != cpu);

	if (irq == SHADOW_TIMER_IRQ_ID && cpu == 0) {
		timer_id = SHADOW_TIMER_ID;
		TIMER_PR("shadow timer is expired now\n");
	}

	timer_interrupt(cpu, timer_id, evt);
	return IRQ_HANDLED;
}

static int timer_clk_set(const char *timer_clk,
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
		}
		clk_set_rate(timer, rate);
		clk_enable(timer);

		if (value)
			*value = timer;
	}
	return 0;
}

static void clk_evt_timer_init(struct clock_event_device *evt,
		unsigned int timer_id, unsigned int rate)
{
	struct comip_timer_cfg cfg;
	struct irqaction *irq_act;
	int cpu = cpumask_first(evt->cpumask);

	cfg.autoload = 1;
	cfg.load_val = (rate / HZ);
	comip_timer_stop(timer_id);
	comip_timer_config(timer_id, &cfg);
	comip_timer_int_enable(timer_id);

	irq_act = &event_res[timer_id].act;
	irq_act->dev_id = (void*)evt;
	irq_act->name = event_res[timer_id].name;
	irq_act->handler = clockevent_interrupt_cb;
	irq_set_affinity(irq_act->irq, cpumask_of(cpu));
}

static void clk_evt_dev_init(struct clock_event_device *evt,
			unsigned int timer_id, unsigned int rate)
{
	evt->shift = 32;
	evt->rating = 300;
	evt->mult = div_sc(rate, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(-1, evt);
	evt->min_delta_ns = clockevent_delta2ns(1, evt);
	evt->features =  CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	evt->mode = CLOCK_EVT_MODE_UNUSED;
	evt->set_next_event = event_set_next_event;
	evt->set_mode = event_set_mode;
	evt->name = event_res[timer_id].name;
	evt->irq = event_res[timer_id].act.irq;

	clockevents_register_device(evt);
}

static unsigned int cs_rate_value;

static cycle_t notrace cs_rate_read(struct clocksource *cs)
{
	return ~ comip_timer_read(COMIP_CLOCKSROUCE_TIMER_ID);
}

static void cs_rate_suspend(struct clocksource *cs)
{
	cs_rate_value = timer_clk_disable(COMIP_CLOCKSROUCE_TIMER_ID);
}

static void cs_rate_resume(struct clocksource *cs)
{
	timer_clk_enable(COMIP_CLOCKSROUCE_TIMER_ID,
		cs_rate_value);
}

static struct clocksource clocksource_rate = {
	.name		= COMIP_CLOCKSROUCE_TIMER_NAME,
	.rating		= 350,
	.read		= cs_rate_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
	.suspend	= cs_rate_suspend,
	.resume		= cs_rate_resume,
};

static void __init clocksource_rate_init(unsigned int rate)
{
	struct comip_timer_cfg cfg;

	timer_clk_set(COMIP_CLOCKSROUCE_TIMER_NAME,
			COMIP_CLOCKSROUCE_TIMER_CLKSRC,
			rate, NULL);

	cfg.autoload = 1;
	cfg.load_val = ~0;
	comip_timer_stop(COMIP_CLOCKSROUCE_TIMER_ID);
	comip_timer_config(COMIP_CLOCKSROUCE_TIMER_ID, &cfg);
	comip_timer_int_disable(COMIP_CLOCKSROUCE_TIMER_ID);
	comip_timer_start(COMIP_CLOCKSROUCE_TIMER_ID);

	if (clocksource_register_hz(&clocksource_rate, rate))
		printk(KERN_ERR "Can't register clocksource %s\n",
				COMIP_CLOCKSROUCE_TIMER_NAME);

	return;
}

static cycle_t notrace comip_32k_read(struct clocksource *cs)
{
	return readl(io_p2v(AP_PWR_TM32K_CUR_VAL));
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

static int __init clocksource_32k_init(void)
{
	/* enable ap_pwr clock. */
	writel(0x70005, io_p2v(AP_PWR_TM32K_CTL));

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

static void comip_read_persistent_clock(struct timespec *ts)
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

static void __init timer_irq_clk_init(unsigned int timer_id,
			unsigned int rate)
{
	struct event_resourc *res = &event_res[timer_id];

	timer_clk_set(res->name, COMIP_CLOCKEVENT_TIMER_CLKSRC,
			rate, &(res->clock));
	if (setup_irq(res->act.irq, &(res->act)))
		printk(KERN_ERR "can't setup irq %d!\n", res->act.irq);
}

static __init void timer_setup_pre(unsigned int rate)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		timer_irq_clk_init(cpu_timer_id[cpu], rate);
	}
}

static int timer_setup_do(struct clock_event_device *evt,
			unsigned int rate) 
{
	int cpu = cpumask_first(evt->cpumask);
	unsigned int timer_id = cpu_timer_id[cpu];

	clk_evt_timer_init(evt, timer_id, rate);
	clk_evt_dev_init(evt, timer_id, rate);
	return 0;
}

static int timer_setup(struct clock_event_device *evt)
{
	if (cpumask_equal(evt->cpumask, cpumask_of(0)))
		return 0;

	return timer_setup_do(evt, CLOCK_TICK_RATE);
}

static void  timer_stop(struct clock_event_device *evt) 
{
	evt->set_mode(CLOCK_EVT_MODE_SHUTDOWN, evt);
	evt->mode = CLOCK_EVT_MODE_SHUTDOWN;
}

static struct local_timer_ops timer_ops = {
	.setup = timer_setup,
	.stop = timer_stop,
};

static struct clock_event_device gp_dev = {
	.name			= COMIP_CLOCKEVENT_TIMER_NAME,
	.features		= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.shift			= 32,
	.rating 		= 300,
	.mode			= CLOCK_EVT_MODE_UNUSED,
	.set_next_event		= event_set_next_event,
	.set_mode		= event_set_mode,
};

static void __init shadow_timer_init(struct clock_event_device *evt,
			unsigned int timer_id, unsigned int rate)
{
	timer_irq_clk_init(timer_id, rate);
	clk_evt_timer_init(evt, timer_id, rate);
}

static int __init shadow_timer_delta_init(void)
{
	u64 delta = timekeeping_max_deferment();
	struct clock_event_device *evt = &gp_dev;

	delta = (delta * evt->mult) >> evt->shift;
	shadow_timer_delta_max = (unsigned long)delta;
	printk(KERN_DEBUG "shadow_timer_delta=%d, shadow_timer_delta_max=%lu\n",
			SHADOW_TIMER_CYCLE_DELTA, shadow_timer_delta_max);

	return 0;
}
late_initcall(shadow_timer_delta_init);

static void __init comip_timer_setup(unsigned int rate)
{
	struct clock_event_device *evt = &gp_dev;

	timer_setup_pre(rate);
	evt->cpumask = cpumask_of(0);
	timer_setup_do(evt, rate);
	shadow_timer_init(evt, SHADOW_TIMER_ID, rate);
}

void __init comip_timer_init(void)
{
	unsigned int rate = CLOCK_TICK_RATE;

	timer_sysclk_sel();
	clocksource_32k_init();
	clocksource_rate_init(rate);
	comip_timer_setup(rate);
	local_timer_register(&timer_ops);
	register_persistent_clock(NULL, comip_read_persistent_clock);
}

/********************************************************/

#ifdef CONFIG_IRQ_MON_DEBUG

#define MON_TIMER_ID		0
#define MON_TIMER_IRQ		INT_TIMER0
#define MON_TIMER_CLKSRC	"timer0_clk"
#define MON_TIMER_PCLKSRC	"pll1_mclk"
#define MON_EXPIRE_TIME		15  /*15s*/

struct mon_data{
	unsigned int cpu;
	unsigned int irq_id;
	struct pt_regs *regs;
	seqcount_t seq; 
};

static int irq_mon_timer_inited = 0;
static DEFINE_PER_CPU(struct mon_data, irq_mon_data);

static inline void irq_mon_data_init_percpu(int cpu)
{
	struct mon_data *data = &per_cpu(irq_mon_data, cpu);

	data->cpu = -1;
	data->irq_id = -1;
	data->regs = NULL;
	data->seq.sequence = 0;
}

static inline void irq_mon_data_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		irq_mon_data_init_percpu(cpu);
}

static inline void irq_timer_stop(int cpu)
{
	timer_stop_special(MON_TIMER_ID);
	comip_timer_int_disable(MON_TIMER_ID);
}

static inline void irq_timer_start(int cpu, unsigned long cycles)
{
	timer_stop_special(MON_TIMER_ID);
	irq_set_affinity(MON_TIMER_IRQ, cpumask_of(3));
	comip_timer_int_enable(MON_TIMER_ID);
	comip_timer_load_value_set(MON_TIMER_ID, cycles);
	comip_timer_start(MON_TIMER_ID);
}

static void irq_monitor_show(struct mon_data *data)
{
	struct pt_regs *regs;

	if (-1 == data->irq_id)
		return;

	printk("\n\n irq:%d is expired in cpu%d. level:%d\n",
			data->irq_id,
			data->cpu,
			data->seq.sequence);

	printk("entry mesg:\n");
	regs = data->regs;
	if (regs)
		show_stack_frame(regs);

	regs = per_cpu(__irq_regs, data->cpu);
	if (regs != data->regs) {
		printk("\ncurrent mesg:\n");
		if (regs)
			show_stack_frame(regs);
	}

	printk("\npanic mesg:\n");
	panic("irq:%d is expired-2\n", data->irq_id);
}

static irqreturn_t mon_timer_irq_handle(int irq, void *dev_id)
{
	struct mon_data *data = &per_cpu(irq_mon_data, 0);

	irq_timer_stop(data->cpu);
	irq_monitor_show(data);
	return IRQ_HANDLED;
}

static struct irqaction mon_timer_irq = {
	.name		= "irq timer",
	.flags		= IRQF_DISABLED | IRQF_IRQPOLL,
	.handler	= mon_timer_irq_handle,
	.irq		= MON_TIMER_IRQ,
};

void irq_monitor_start(int cpu, unsigned int irq_id, struct pt_regs *regs)
{
	struct mon_data *data;

	smp_rmb();
	if (!irq_mon_timer_inited)
		return;
	if (cpu != 0)
		return;
	data = &per_cpu(irq_mon_data, cpu);
	if (0 == data->seq.sequence++) {
		data->cpu = cpu;
		data->irq_id = irq_id;
		data->regs = regs;
		irq_timer_start(cpu, CLOCK_TICK_RATE * MON_EXPIRE_TIME);
	}
}

void irq_monitor_stop(int cpu)
{
	struct mon_data *data;

	smp_rmb();
	if (!irq_mon_timer_inited)
		return;
	if (cpu != 0)
		return;
	data = &per_cpu(irq_mon_data, cpu);
	if (0 == data->seq.sequence)
		return;
	if (0 == --data->seq.sequence) {
		irq_timer_stop(cpu);
		irq_mon_data_init_percpu(cpu);
	}
}

static int irq_timer_reboot_cb(struct notifier_block *this,
			unsigned long code, void *unused)
{
	irq_timer_stop(0);
	return NOTIFY_OK;
}

static struct notifier_block irq_timer_reboot_notifier = {
	.notifier_call	= irq_timer_reboot_cb,
};

static int irq_timer_panic_cb(struct notifier_block *this,
			unsigned long event, void *ptr)
{
	irq_timer_stop(0);
	return NOTIFY_OK;
}

static struct notifier_block irq_timer_panic_notifier = {
	.notifier_call	= irq_timer_panic_cb,
	.priority	= 150,
};

static int __init irq_timer_init(void)
{
	int ret;

	irq_mon_data_init();

	timer_clk_set(MON_TIMER_CLKSRC,
				MON_TIMER_PCLKSRC,
				CLOCK_TICK_RATE, NULL);

	timer_stop_special(MON_TIMER_ID);
	comip_timer_int_enable(MON_TIMER_ID);
	comip_timer_autoload_disable(MON_TIMER_ID);

	ret = setup_irq(MON_TIMER_IRQ, &mon_timer_irq);
	if (ret)
			printk(KERN_ERR "can't setup irq for debug timer!\n");

	register_reboot_notifier(&irq_timer_reboot_notifier);
	atomic_notifier_chain_register(&panic_notifier_list,
			&irq_timer_panic_notifier);

	barrier();
	irq_mon_timer_inited = 1;
	smp_wmb();
	return 0;
}

late_initcall(irq_timer_init);

#endif

