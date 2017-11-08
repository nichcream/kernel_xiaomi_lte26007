/* arch/arm/mach-comip/comip-wdt-lc1813.c
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip hardware plarform's wdt driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version		Date		Author		Description
 *	1.0.0		2013-06-08	licheng		created
 *	1.0.1		2013-06-26	zhaoyuan	updated
 *
 */

#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/lockdep.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/smpboot.h>
#include <linux/sched/rt.h>
#include <asm/irq_regs.h>
#include <linux/kvm_para.h>
#include <linux/perf_event.h>

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <plat/hardware.h>
#include <plat/watchdog.h>
#include <plat/reset.h>

#define WDTDBG(fmt, arg...)	do{if(0)printk(KERN_INFO "WDT:%s:%d: " fmt ,__func__,__LINE__, ## arg);}while(0)
#define WDTINF(fmt, arg...)	do{if(1)printk(KERN_INFO "WDT:%s:%d: " fmt ,__func__,__LINE__, ## arg);}while(0)
#define WDTERR(fmt, arg...)	do{if(1)printk(KERN_ERR "WDT:%s:%d: " fmt ,__func__,__LINE__, ## arg);}while(0)

#define WDTDBGCODE 1

#define WDT_NUM				(5)
static int __read_mostly watchdog_disabled;
static void watchdog_set_timeout_all_cpus(void);

struct comip_wdt_data {
	void __iomem *base;
	struct clk *clk;
	unsigned long clk_rate;
	int id;
	int irq;
	char name[10];
};

struct comip_wdt_data comip_wdt[WDT_NUM];
static bool initialised[WDT_NUM] = {0};

static unsigned int wdt_count[]=
{
	0xffff,
	0x1ffff,
	0x3ffff,
	0x7ffff,
	0xfffff,
	0x1fffff,
	0x3fffff,
	0x7fffff,
	0xffffff,
	0x1ffffff,
	0x3ffffff,
	0x7ffffff,
	0xfffffff,
	0x1fffffff,
	0x3fffffff,
	0x7fffffff
};

static DEFINE_PER_CPU(struct task_struct *, comip_watchdog_task);
static DEFINE_PER_CPU(unsigned long , comip_wdt_stamp);

/* touch period, default 3000ms. */
unsigned int __read_mostly heartbeat = 3000;

/* softwatchdog stamp ms. */
static unsigned long softwatchdog_stamp;

/* if panic when softwatchdog timeout */
unsigned int __read_mostly softwdt_panic = 0;

/* softwatchdog timeout default 60000ms */
static unsigned int __read_mostly softwatchdog_threshold = 60000;

/* watchdog timeout default 10000ms */
static unsigned long timeout = 10000;

#define TIMEOUTMAX	20000

enum timeouttype {
	WDT_INFO = 0,	/* only give debug info. */
	WDT_PANIC,	/* panic when timeout. */
	WDT_DEBUG,	/* triger tl420 debug. */
	WDT_RESET,	/* chip reset. */
	WDT_MAX
};

static unsigned int timeout_type = WDT_PANIC;
static const char *wdt_work_type[WDT_MAX] = {
	[WDT_INFO]  = "info",
	[WDT_PANIC] = "panic",
	[WDT_DEBUG] = "debug",
	[WDT_RESET] = "reset"
};

static DECLARE_RWSEM(wdt_rwsem);

#define CHK_TIME_VALID(hb, to) (((to)>>2 > (hb)) ? 1 : 0)
#define GET_VALID_HB(to) ((to)>>2)
#define GET_VALID_TO(hb) ((hb)<<2)

static inline int watchdog_get_id_for_cpu(unsigned int cpu)
{
	return cpu;
}

static inline bool watchdog_is_initialised(unsigned int cpu)
{
	return initialised[watchdog_get_id_for_cpu(cpu)];
}

static inline struct comip_wdt_data * __comip_wdt_get(unsigned int cpu)
{
	if (watchdog_is_initialised(cpu))
		return &comip_wdt[watchdog_get_id_for_cpu(cpu)];
	else
		return NULL;
}

/*
 * Returns milliseconds, approximately.  We don't need nanosecond
 * resolution, and we don't need to waste time with a big divide when
 * 2^20ns == 1.048ms.
 */
static unsigned long get_timestamp(void)
{
	return local_clock() >> 20LL;  /* 2^20 ~= 10^6 */
}

static void softwdt_touch(void)
{
	unsigned long timestamp;
	down_write(&wdt_rwsem);
	timestamp = get_timestamp();
	if (softwatchdog_stamp < timestamp)
		softwatchdog_stamp = timestamp;
	up_write(&wdt_rwsem);
}

static unsigned long time_since_last_touch(void)
{
	unsigned long now = get_timestamp();
	if (time_after(now, softwatchdog_stamp))
		return now - softwatchdog_stamp;

	return 0;
}

/* check for a softlockup
 * this is done by every watchdog thread.
 * !0: is softlockup
 * 0: not softlockup
 */
static int is_softlockup(void)
{
	unsigned long now = get_timestamp();

	/* Warn about unreasonable delays: */
	if (time_after(now, softwatchdog_stamp + softwatchdog_threshold))
		return now - softwatchdog_stamp;

	return 0;
}

static int __comip_wdt_enable(unsigned int cpu)
{
	struct comip_wdt_data *wdt = __comip_wdt_get(cpu);

	if (NULL == wdt)
		return -1;

	clk_enable(wdt->clk);
	writel(0x13, wdt->base + WDT_CR);
	writel(WDT_RESET_VAL, (wdt->base + WDT_CRR));
	WDTDBG("%s\n", wdt->name);
	return 0;
}

static int __comip_wdt_disable(unsigned int cpu)
{
	struct comip_wdt_data *wdt = __comip_wdt_get(cpu);

	if (NULL == wdt)
		return -1;

	clk_disable(wdt->clk);
	comip_reset(wdt->name);
	WDTDBG("%s\n", wdt->name);
	return 0;
}

static int __comip_wdt_touch(unsigned int cpu)
{
	struct comip_wdt_data *wdt = __comip_wdt_get(cpu);

	if (NULL == wdt)
		return -1;

	writel(WDT_RESET_VAL, (wdt->base + WDT_CRR));
	__this_cpu_write(comip_wdt_stamp, get_timestamp());

	return 0;
}

static int comip_wdt_reboot_handler(struct notifier_block *this,
			unsigned long code, void *unused)
{
	if ((code == SYS_POWER_OFF) || (code == SYS_HALT)) {
		timeout = TIMEOUTMAX;
		watchdog_set_timeout_all_cpus();
	}
	return NOTIFY_OK;
}

static int comip_wdt_panic_handler(struct notifier_block *this,
			unsigned long event, void *ptr)
{
	timeout = TIMEOUTMAX;
	watchdog_set_timeout_all_cpus();

	return NOTIFY_OK;
}

static struct notifier_block comip_wdt_reboot_notifier = {
	.notifier_call	= comip_wdt_reboot_handler,
};

static struct notifier_block comip_wdt_panic_notifier = {
	.notifier_call	= comip_wdt_panic_handler,
	.priority	= 150,
};

static int __comip_wdt_get_to_index(unsigned long to)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(wdt_count); i++) {
		if (to <= wdt_count[i]) {
			return i;
		}
	}
	return -1;
}

static int __comip_wdt_get_timeout(unsigned int cpu)
{
	struct comip_wdt_data *wdt = __comip_wdt_get(cpu);
	unsigned int i;
	unsigned long to;

	if (NULL == wdt)
		return -1;

	i = readl(wdt->base + WDT_TORR);
	to = wdt_count[i & 0x0f];
	to = to/(wdt->clk_rate/1000);

	WDTDBG("%s [%d]to:%ldms\n", wdt->name, i, to);
	return (int)to;
}

static int __comip_wdt_set_timeout(unsigned int cpu, unsigned int t)
{
	struct comip_wdt_data *wdt = __comip_wdt_get(cpu);
	int i=0;
	unsigned long to;

	if (NULL == wdt)
		return -1;

	to = t * (wdt->clk_rate /1000);
	i = __comip_wdt_get_to_index(to);
	if (i < 0)
		i = 15;
	writel(i, wdt->base + WDT_TORR);
	writel(WDT_RESET_VAL, (wdt->base + WDT_CRR));

	WDTDBG("%s %d\n", wdt->name, __comip_wdt_get_timeout(cpu));

	return 0;
}

static void __comip_wdt_set_chip_reset_en(unsigned int en)
{
	writel(!en, io_p2v(AP_PWR_WDT_RSTCTL));
}

static void watchdog_set_timeout_all_cpus(void)
{
	unsigned int cpu;

	for_each_online_cpu(cpu) {
		__comip_wdt_set_timeout(cpu, timeout);
		__comip_wdt_touch(cpu);
	}
}

#if 0
static void watchdog_set_disable_all_cpus(void)
{
	unsigned int cpu;

	for_each_online_cpu(cpu) {
		__comip_wdt_disable(cpu);
	}
}
#endif

static void watchdog_set_prio(unsigned int policy, unsigned int prio)
{
	struct sched_param param = { .sched_priority = prio };

	sched_setscheduler(current, policy, &param);
}

static void watchdog_enable(unsigned int cpu)
{
	/* initialize timestamp */
#if WDTDBGCODE
	/* need lower than test thread. */
	watchdog_set_prio(SCHED_FIFO, MAX_RT_PRIO - 2);
#else
	watchdog_set_prio(SCHED_FIFO, MAX_RT_PRIO - 1);
#endif
	if (__comip_wdt_enable(cpu) < 0)
		WDTINF("wdt %d not enable success\n", cpu);
	__comip_wdt_set_timeout(cpu, timeout);
	__comip_wdt_touch(cpu);

	WDTDBG("gettimeout %d\n", __comip_wdt_get_timeout(cpu));
}

static void watchdog_disable(unsigned int cpu)
{

	if (cpu)
		__comip_wdt_disable(cpu);
	WDTDBG("\n");
}

static int watchdog_should_run(unsigned int cpu)
{
	return initialised[watchdog_get_id_for_cpu(cpu)];
}

/*
 * The watchdog thread function - touches the timestamp.
 *
 * It only runs once every sample_period seconds (4 seconds by
 * default) to reset the softlockup timestamp. If this gets delayed
 * for more than 2*watchdog_thresh seconds then the debug-printout
 * triggers in watchdog_timer_fn().
 */
static void watchdog(unsigned int cpu)
{
	int duration;
	unsigned long timeout = msecs_to_jiffies(heartbeat) + 1;

	__comip_wdt_touch(cpu);
	duration = is_softlockup();
	if (duration) {
		if (softwdt_panic)
			panic("softlockup: hung tasks");
	}

	schedule_timeout_interruptible(timeout);
}

static struct smp_hotplug_thread comip_watchdog_threads = {
	.store			= &comip_watchdog_task,
	.thread_should_run	= watchdog_should_run,
	.thread_fn		= watchdog,
	.thread_comm		= "wdt/%u",
	.setup			= watchdog_enable,
	.park			= watchdog_disable,
	.unpark			= watchdog_enable,
};

int __init comip_watchdog_init(void)
{
	if (smpboot_register_percpu_thread(&comip_watchdog_threads)) {
		pr_err("Failed to create watchdog threads, disabled\n");
	}

	watchdog_disabled = 0;
	register_reboot_notifier(&comip_wdt_reboot_notifier);
	atomic_notifier_chain_register(&panic_notifier_list,
					&comip_wdt_panic_notifier);
#ifdef BUILD_MASS_PRODUCTION
	__comip_wdt_set_chip_reset_en(1);
	writel(0x640, io_p2v(AP_PWR_CHIPRSTN_CTL));
#endif
	return 0;
}

#if WDTDBGCODE

static DEFINE_PER_CPU(struct task_struct *, comip_watchdog_debug_task);

unsigned int en_delay = 0;

static int watchdog_debug_should_run(unsigned int cpu)
{
	return 1;
}

static void watchdog_debug_enable(unsigned int cpu)
{
	/* initialize timestamp */
	watchdog_set_prio(SCHED_FIFO, MAX_RT_PRIO - 1);

	WDTDBG("wdt debug %d\n", cpu);

}

static void watchdog_debug_disable(unsigned int cpu)
{
	WDTDBG("cpu:%d\n", cpu);
}

static void watchdog_debug(unsigned int cpu)
{
	if (en_delay & (1 << cpu))
		mdelay(1000);
	else
		msleep(1000);
}

static struct smp_hotplug_thread comip_watchdog_debug_threads = {
	.store			= &comip_watchdog_debug_task,
	.thread_should_run	= watchdog_debug_should_run,
	.thread_fn		= watchdog_debug,
	.thread_comm		= "comip_watchdog_debug/%u",
	.setup			= watchdog_debug_enable,
	.park			= watchdog_debug_disable,
	.unpark			= watchdog_debug_enable,
};

int comip_watchdog_debug_init(void)
{
	if (smpboot_register_percpu_thread(&comip_watchdog_debug_threads)) {
		pr_err("Failed to create watchdog threads, disabled\n");
	}

	return 0;
}

static ssize_t comip_wdt_debug_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(s,"0x%x\techo 98700 to start debug\n", en_delay);

	return (s - buf);
}

static ssize_t comip_wdt_debug_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long tt = 0;

	ret = kstrtoul(buf, 16, &tt);

	if (0x98700 == tt) {
		printk("comip_watchdog_debug_init\n");
		comip_watchdog_debug_init();
	} else {
		en_delay = tt;
		printk("en_delay:0x%x\n", en_delay);
	}

	return size;
}
static DEVICE_ATTR(wdt_debug_en, S_IRUGO | S_IWUSR, comip_wdt_debug_enable_show, comip_wdt_debug_enable_store);

#endif
static inline irqreturn_t comip_wdt_irq(int irq, void *dev)
{
	struct comip_wdt_data *wdt = dev;

	/* Clear wdt irq */
	__raw_readl(wdt->base + WDT_ICR);
	if (timeout_type == WDT_INFO) {
		__comip_wdt_touch(wdt->id);
		printk("watchdog %d trigger\n", wdt->id);
	} else if (timeout_type == WDT_PANIC) {
		panic("watchdog%d timeout!\n", wdt->id);
	} else if (timeout_type == WDT_DEBUG) {
		disable_irq_nosync(wdt->irq);
		printk("watchdog %d trigger, disable irq:%d, wait for tl420debug\n", wdt->id, wdt->irq);
	} else if (timeout_type == WDT_RESET) {
		__comip_wdt_set_chip_reset_en(1);
		panic("watchdog %d trigger, wait for chip reset\n", wdt->id);
	}

	return IRQ_HANDLED;
}

static enum timeouttype decode_timeouttype(const char *buf, size_t n)
{
	enum timeouttype type = WDT_INFO;

	const char ** s;
	char *p;
	int len;

	p = memchr(buf, '\n', n);
	len = p ? p - buf : n;

	for (s = &wdt_work_type[type]; type < WDT_MAX; s++, type++)
		if (*s && len == strlen(*s) && !strncmp(buf, *s, len))
			return type;

	return WDT_INFO;
}

static ssize_t comip_wdt_worktype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *s = buf;
	char *t = NULL;
	int i;

	for (i = 0; i < WDT_MAX; i++) {
		if (i == timeout_type)
			t = "  [active]\n";
		else
			t = "\n";

		s += sprintf(s,"%s%s", wdt_work_type[i],t);
	}

	return (s - buf);
}

static ssize_t comip_wdt_worktype_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	enum timeouttype type;

	type = decode_timeouttype(buf, size);

	down_write(&wdt_rwsem);
	timeout_type = type;
	up_write(&wdt_rwsem);

	if (timeout_type == WDT_RESET)
		__comip_wdt_set_chip_reset_en(1);
	else
		__comip_wdt_set_chip_reset_en(0);

	return size;
}
static DEVICE_ATTR(wdt_worktype, S_IRUGO | S_IWUSR, comip_wdt_worktype_show, comip_wdt_worktype_store);

static ssize_t comip_softwdt_touch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int ret;

	down_read(&wdt_rwsem);
	ret = sprintf(buf, "time since last touch:%ldms\n", time_since_last_touch());
	up_read(&wdt_rwsem);

	return ret;
}

static ssize_t comip_softwdt_touch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long tt = 0;

	softwdt_touch();

	ret = kstrtoul(buf, 10, &tt);

	return size;
}
static DEVICE_ATTR(wdt_softwdt_touch, S_IRUGO | S_IWUSR, comip_softwdt_touch_show, comip_softwdt_touch_store);

static ssize_t comip_softwdt_panic_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int ret;

	down_read(&wdt_rwsem);
	ret = sprintf(buf, "soft watchdog panic:%d\n", softwdt_panic);
	up_read(&wdt_rwsem);

	return ret;
}

static ssize_t comip_softwdt_panic_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long need_panic;
	int ret;

	ret = kstrtoul(buf, 10, &need_panic);
	if (ret)
		return ret;

	down_write(&wdt_rwsem);
	softwdt_panic = need_panic;
	up_write(&wdt_rwsem);

	return size;
}
static DEVICE_ATTR(wdt_softwdt_panic, S_IRUGO | S_IWUSR, comip_softwdt_panic_show, comip_softwdt_panic_store);


static ssize_t comip_softwdt_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int ret;

	down_read(&wdt_rwsem);
	ret = sprintf(buf, "%d\n", softwatchdog_threshold);
	up_read(&wdt_rwsem);

	return ret;
}

static ssize_t comip_softwdt_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long threshold;
	int ret;

	ret = kstrtoul(buf, 10, &threshold);
	if (ret)
		return ret;

	if (threshold < timeout) {
		WDTINF("threshold(%ld) must bigger than hw-timeout:%ld\n",
			threshold, timeout);
		return -1;
	}

	down_write(&wdt_rwsem);
	softwatchdog_threshold = threshold;
	up_write(&wdt_rwsem);

	return size;
}
static DEVICE_ATTR(wdt_softwdt_threshold, S_IRUGO | S_IWUSR, comip_softwdt_threshold_show, comip_softwdt_threshold_store);

static ssize_t comip_wdt_timeout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int ret;

	down_read(&wdt_rwsem);
	ret = sprintf(buf, "%ld\n", timeout);
	up_read(&wdt_rwsem);

	return ret;
}

static ssize_t comip_wdt_timeout_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long to;
	int ret;

	ret = kstrtoul(buf, 10, &to);
	if (ret)
		return ret;

	if (CHK_TIME_VALID(heartbeat, to)) {
		down_write(&wdt_rwsem);
		timeout = to;
		up_write(&wdt_rwsem);
	} else {
		WDTINF("timeout must 4 times bigger than heartbeat!"
			"timeout:%ldms, heartbeat:%dms\n", timeout, heartbeat);
	}

	watchdog_set_timeout_all_cpus();

	return size;
}
static DEVICE_ATTR(wdt_timeout, S_IRUGO | S_IWUSR, comip_wdt_timeout_show, comip_wdt_timeout_store);

static ssize_t comip_wdt_heartbeat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int ret;

	down_read(&wdt_rwsem);
	ret = sprintf(buf, "%d\n", heartbeat);
	up_read(&wdt_rwsem);

	return ret;
}

static ssize_t comip_wdt_heartbeat_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long hb;
	int ret;

	ret = kstrtoul(buf, 10, &hb);
	if (ret)
		return ret;

	if (CHK_TIME_VALID(hb, timeout)) {
		down_write(&wdt_rwsem);
		heartbeat = hb;
		up_write(&wdt_rwsem);
	} else {
		WDTINF("timeout must 4 times bigger than heartbeat!"
			"timeout:%ldms, heartbeat:%dms\n", timeout, heartbeat);
		return -ERANGE;
	}

	return size;
}
static DEVICE_ATTR(wdt_heartbeat, S_IRUGO | S_IWUSR, comip_wdt_heartbeat_show, comip_wdt_heartbeat_store);
/*Set cpu0 watchdog irq affinity to cpu3 */
static int __cpuinit comip_wdt_cpu_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	#define AFFINITY_CPU	3
	unsigned int cpu = (unsigned long)hcpu;
	struct comip_wdt_data *wdt = &comip_wdt[0];

	if (cpu == AFFINITY_CPU) {
		switch (action) {
		case CPU_ONLINE:
			irq_set_affinity(wdt->irq, cpumask_of(cpu));
			break;
		default:
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block __refdata comip_wdt_cpu_notifier = {
    .notifier_call = comip_wdt_cpu_callback,
};

#ifdef CONFIG_PM
static int comip_wdt_suspend_touch(struct device *dev)
{
	/* Touch  watchdog. */
	unsigned int val;
	struct comip_wdt_data *wdt = &comip_wdt[dev->id];

	val = __raw_readl(wdt->base + WDT_CR);
	if (val & WDT_ENABLE)
		__comip_wdt_touch(dev->id);

	return 0;
}

static struct dev_pm_ops comip_wdt_pm_ops = {
		.suspend = comip_wdt_suspend_touch,
		.resume = comip_wdt_suspend_touch,
};
#endif

static int comip_wdt_probe(struct platform_device * pdev)
{
	struct resource	*wdt_mem;
	struct comip_wdt_data * wdt = &comip_wdt[pdev->id];
	int ret = 0;

	pdev->dev.id = pdev->id;
	wdt->id = pdev->id;
	wdt->irq = platform_get_irq(pdev, 0);
	wdt_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!wdt_mem) {
		dev_err(&pdev->dev, "failed to get resource\n");
		ret = -ENOENT;
		goto eres;
	}

	wdt->base = devm_ioremap_resource(&pdev->dev, wdt_mem);
	if (IS_ERR(wdt->base)) {
		ret = PTR_ERR(wdt->base);
		dev_err(&pdev->dev, "wdt ioremap error %d\n", ret);
		goto eres;
	}

	wdt->clk = clk_get(&pdev->dev, "wdt_clk");
	if (IS_ERR(wdt->clk)) {
		ret = PTR_ERR(wdt->clk);
		goto ezalloc;
	}

	wdt->clk_rate = clk_get_rate(wdt->clk);

	sprintf(wdt->name, "WDT%d", wdt->id);
	WDTDBG("%s clk :%ld\n", wdt->name, wdt->clk_rate);

	clk_enable(wdt->clk);
	writel(0x0f, wdt->base + WDT_TORR);
	clk_disable(wdt->clk);

	// need to bind the irq on cpu?
	ret = request_irq(wdt->irq, comip_wdt_irq,
			IRQF_DISABLED, wdt->name, wdt);
	if (ret) {
		printk(KERN_ERR "Failed to wdt%d irq %d\n", wdt->id, wdt->irq);
		return ret;
	}

	platform_set_drvdata(pdev, wdt);

	initialised[pdev->id] = true;

	if (0 == pdev->id) {
		register_hotcpu_notifier(&comip_wdt_cpu_notifier);
		device_create_file(&pdev->dev, &dev_attr_wdt_timeout);
		device_create_file(&pdev->dev, &dev_attr_wdt_softwdt_threshold);
		device_create_file(&pdev->dev, &dev_attr_wdt_softwdt_panic);
		device_create_file(&pdev->dev, &dev_attr_wdt_softwdt_touch);
		device_create_file(&pdev->dev, &dev_attr_wdt_heartbeat);
		device_create_file(&pdev->dev, &dev_attr_wdt_worktype);
		#if WDTDBGCODE
		device_create_file(&pdev->dev, &dev_attr_wdt_debug_en);
		#endif
	}

	return ret;

ezalloc:
	devm_iounmap(&pdev->dev, wdt->base);
eres:

	return ret;
}

static int comip_wdt_remove(struct platform_device *pdev)
{
	struct comip_wdt_data *wdt = &comip_wdt[pdev->id];

	if (0 == pdev->id)
		unregister_hotcpu_notifier(&comip_wdt_cpu_notifier);
	initialised[pdev->id] = false;
	devm_iounmap(&pdev->dev, wdt->base);
	return 0;
}

static struct platform_driver comip_wdt_drv = {
	.driver = {
		.name = "comip-wdt",
	#if CONFIG_PM
		.pm = &comip_wdt_pm_ops,
	#endif
		.owner	= THIS_MODULE,
	},
	.probe	 = comip_wdt_probe,
	.remove  = comip_wdt_remove,
};

late_initcall(comip_watchdog_init);
module_platform_driver(comip_wdt_drv);
MODULE_AUTHOR("licheng <licheng@leadcoretech.com>");
MODULE_DESCRIPTION("COMIP Watchdog Driver");
MODULE_LICENSE("GPL");
