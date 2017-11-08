#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/io.h>
#include <plat/comip-snd-notifier.h>
#include <plat/clock.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <plat/comip-timer.h>

bool snd_state = false;

#define SND_LOWPOWER_DEV_NAME "comip-snd-lowpower"
#define TIMER_RATE  (32768)
#define TIMER_ID (0)
static DEFINE_SPINLOCK(snd_timer_lock);
struct lowpower_unit {
	unsigned int ms;
	unsigned int snd_wake_lock;
};

#define LOWPOWER_MAGIC_NUM		'l'
#define LOWPOWER_RW		_IOWR(LOWPOWER_MAGIC_NUM, 0, struct lowpower_unit)
#define TIMER_INT_CYCLES_MIN	2
#define TIMER_CYCLES_MIN	5
#define TIMER_RST_BIT_OFFSET	5
#define TIMER_RESET_MAGIC	0xdeaddead

struct snd_lowpower {
	struct device  *dev;
	bool snd_is_active;
	int timer_irq;
	bool snd_wake_lock;
	unsigned int sleep_timer; // ms
	int sleep_ms;
	struct clk *snd_timer_clk;

	struct workqueue_struct *wq;
	struct work_struct irq_work;
	struct wake_lock wake_lock;

	struct timeval suspend_time;
	struct timeval resume_time;
	spinlock_t irq_lock;
	int	irq_is_disable;
};


struct snd_lowpower  *lp;

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
static inline void interrupt_status_clear(unsigned int timer_id)
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
			printk(KERN_DEBUG"Warnning: timer%d int-excep\n", timer_id);
	}
}

static inline void snd_timer_stop_do(unsigned int timer_id)
{
	unsigned int now;
	int retry = 10;

	/* if timer enabled ? */
	now = comip_timer_enabled(timer_id);
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
					printk(KERN_DEBUG"No irq checked for timer%d\n", timer_id);

				break;
			}
			udelay(33);
		}
		if (unlikely(retry < 0))
			printk(KERN_DEBUG"timer%d wait irq retry timeout\n", timer_id);
	}

out:
	comip_timer_stop(timer_id);
}

static void snd_timer_shutdown(void)
{
	unsigned long flags;
	spin_lock_irqsave(&snd_timer_lock, flags);
	snd_timer_stop_do(TIMER_ID);
	comip_timer_int_disable(TIMER_ID);
	spin_unlock_irqrestore(&snd_timer_lock, flags);
}

static void snd_timer_restart(int ms)
{
	unsigned long flags;
	struct comip_timer_cfg cfg;

	cfg.autoload = 0;
	cfg.load_val = ((ms *TIMER_RATE) / 1000) ;
	if (cfg.load_val < TIMER_CYCLES_MIN)
		cfg.load_val = TIMER_CYCLES_MIN;

	spin_lock_irqsave(&snd_timer_lock, flags);
	snd_timer_stop_do(TIMER_ID);
	comip_timer_config(TIMER_ID, &cfg);
	comip_timer_int_enable(TIMER_ID);
	comip_timer_start(TIMER_ID);
	spin_unlock_irqrestore(&snd_timer_lock, flags);
}

static void snd_timer_irq_disable(struct snd_lowpower *lowpower)
{
	unsigned long flags;

	spin_lock_irqsave(&lowpower->irq_lock, flags);
	if (!lowpower->irq_is_disable) {
		disable_irq_nosync(INT_TIMER0);
		lowpower->irq_is_disable = 1;
	}
	spin_unlock_irqrestore(&lowpower->irq_lock, flags);
}

static void snd_timer_irq_enable(struct snd_lowpower *lowpower)
{
	unsigned long flags;

	spin_lock_irqsave(&lowpower->irq_lock, flags);
	if (lowpower->irq_is_disable) {
		enable_irq(INT_TIMER0);
		lowpower->irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&lowpower->irq_lock, flags);
}

static void snd_timer_interrupt_work(struct work_struct *work)
{
	struct snd_lowpower *lowpower = container_of(work, struct snd_lowpower, irq_work);

	if(!lowpower->snd_wake_lock)
		wake_lock(&lowpower->wake_lock);
	lowpower->snd_wake_lock = true;
	snd_timer_irq_enable(lowpower);
}
static irqreturn_t snd_timer_interrupt(int irq, void *dev_id)
{
	struct snd_lowpower *lowpower = (struct snd_lowpower *)dev_id;
	unsigned long flags;

	snd_timer_irq_disable(lowpower);
	wake_lock_timeout(&lowpower->wake_lock, HZ / 4);
	spin_lock_irqsave(&snd_timer_lock, flags);
	interrupt_status_clear(TIMER_ID);
	snd_timer_stop_do(TIMER_ID);
	comip_timer_int_disable(TIMER_ID);
	spin_unlock_irqrestore(&snd_timer_lock, flags);
	queue_work(lowpower->wq, &lowpower->irq_work);

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
		if (value)
			*value = timer;
	}
	return 0;
}
struct irqaction snd_timer_irq = {
	       .name           = "snd_timer",
	       .flags          = IRQF_DISABLED,
	       .handler        = snd_timer_interrupt,
	       .dev_id         = NULL,
	       .irq            = INT_TIMER0,
	};
static int snd_timer_init(struct snd_lowpower  *lowpower)
{
	int err = 0;

	snd_timer_irq.dev_id = lowpower;
	err = setup_irq(snd_timer_irq.irq, &snd_timer_irq);
	if (err)
		BUG();

	lowpower->timer_irq = snd_timer_irq.irq;

	irq_set_irq_wake(lowpower->timer_irq, 1);

	INIT_WORK(&lowpower->irq_work, snd_timer_interrupt_work);

	lowpower->wq = create_freezable_workqueue("snd_timer");
	if (!lowpower->wq) {
		err = -ENOMEM;
	}

	/* timer init*/
	timer_clk_set("timer0_clk", "clk_32k", TIMER_RATE, &lowpower->snd_timer_clk);
	clk_enable(lowpower->snd_timer_clk);
	lowpower->irq_is_disable = 0;
	spin_lock_init(&lowpower->irq_lock);

	return err;
}


#ifdef CONFIG_PM
static int snd_lowpower_suspend(struct device *dev)
{
	struct snd_lowpower *lowpower = dev_get_drvdata(dev);
	int ret = 0;
	int tmp;
	do_gettimeofday(&lowpower->suspend_time);

	if(lowpower->snd_is_active) {
    	tmp = (lowpower->suspend_time.tv_sec - lowpower->resume_time.tv_sec)*1000000 +
				(lowpower->suspend_time.tv_usec - lowpower->resume_time.tv_usec);
		dev_dbg(lowpower->dev, "system work : %d ms", tmp/1000);
//	printk("suspend: AP TIMER0 TLC:0x%x,  AP_TIMER0_TCV: 0x%x, AP_TIMER0_TCR:0x%x , AP_TIMER0_TIC:0x%x, AP_TIMER0_TIS:0x%x",readl(io_p2v(0xA0894000)),
//		readl(io_p2v(0xA0894004)), readl(io_p2v(0xA0894008)),readl(io_p2v(0xA089400c)), readl(io_p2v(0xA089400c)) );
	}

    return ret;
}

static void snd_lowpower_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
	struct snd_lowpower *lowpower = platform_get_drvdata(pdev);
	int tmp;

	do_gettimeofday(&lowpower->resume_time);
	if(lowpower->snd_is_active) {
    	tmp = (lowpower->resume_time.tv_sec - lowpower->suspend_time.tv_sec)*1000000 +
				(lowpower->resume_time.tv_usec - lowpower->suspend_time.tv_usec);
		dev_dbg(lowpower->dev, "system sleep : %d ms", tmp/1000);
//	printk("resume: AP TIMER0 TLC:0x%x,  AP_TIMER0_TCV: 0x%x, AP_TIMER0_TCR:0x%x , AP_TIMER0_TIC:0x%x, AP_TIMER0_TIS:0x%x",readl(io_p2v(0xA0894000)),
//		readl(io_p2v(0xA0894004)), readl(io_p2v(0xA0894008)),readl(io_p2v(0xA089400c)), readl(io_p2v(0xA089400c)) );
	}

    return;
}
#else
#define snd_lowpower_suspend NULL
#define snd_lowpower_resume  NULL
#endif /* CONFIG_PM */

static long snd_lowpower_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int ret;
	void __user *argp = (void __user *)arg;
	struct lowpower_unit lp_unit;
	struct snd_lowpower  *lowpower = lp;

	switch (cmd) {
	case LOWPOWER_RW:
		if (copy_from_user(&lp_unit, argp, sizeof(lp_unit)))
			return -EFAULT;

		lp_unit.snd_wake_lock = lowpower->snd_wake_lock;
		snd_timer_restart(lp_unit.ms);
		if(lowpower->snd_wake_lock) {
			lowpower->snd_wake_lock = false;
			wake_unlock(&lowpower->wake_lock);
		}

		if(copy_to_user(argp, &lp_unit, sizeof(lp_unit)))
			ret = -EFAULT;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations snd_lowpower_fops = {
	.owner	= THIS_MODULE,
	.unlocked_ioctl	= snd_lowpower_ioctl,
};

static struct miscdevice snd_lowpower_misc_dev = {
	MISC_DYNAMIC_MINOR,
	SND_LOWPOWER_DEV_NAME,
	&snd_lowpower_fops
};

static int audio_active_notifier(struct notifier_block *blk,
                                  unsigned long code, void *_param)
{
	struct snd_lowpower  *lowpower = lp;

	switch (code){
		case AUDIO_START:
			lowpower->snd_is_active = true;
			snd_state = true;
			break;
		case AUDIO_STOP:
			lowpower->snd_is_active = false;
			snd_state = false;

			snd_timer_shutdown();
			if(lowpower->snd_wake_lock) {
				lowpower->snd_wake_lock = false;
				wake_unlock(&lowpower->wake_lock);
			}
			break;
		default:
			break;
	}
	dev_dbg(lowpower->dev, "snd is active = %d \n", lowpower->snd_is_active);

	return 0;
}
struct notifier_block audio_state_notifier_block = {
	.notifier_call = audio_active_notifier,
};


static  int snd_lowpower_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_lowpower  *lowpower;

	lowpower = kzalloc(sizeof(struct snd_lowpower), GFP_KERNEL);
	if (!lowpower) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		return ret;
	}
	lowpower->dev = &pdev->dev;
	lowpower->snd_wake_lock = false;

	platform_set_drvdata(pdev, lowpower);

	/* initialize ap timer-0 */
	ret = snd_timer_init(lowpower);
	if(ret) {
		dev_err(&pdev->dev, "snd timer init failed");
		return ret;
	}

	wake_lock_init(&lowpower->wake_lock, WAKE_LOCK_SUSPEND, "snd_lowpower");

	ret = misc_register(&snd_lowpower_misc_dev);
	if (ret) {
		dev_err(&pdev->dev, "misc_register failed");
		return ret;
	}
	register_audio_state_notifier(&audio_state_notifier_block);

	do_gettimeofday(&lowpower->suspend_time);
	do_gettimeofday(&lowpower->resume_time);

	lp = lowpower;
	dev_info(&pdev->dev, "snd_lowpower_probe\n");

	return ret;
}

static  int snd_lowpower_remove(struct platform_device *pdev)
{
	struct snd_lowpower *lowpower = platform_get_drvdata(pdev);

	kfree(lowpower);

	return 0;
}

static const struct dev_pm_ops pm_ops = {
    .prepare  = snd_lowpower_suspend,
    .complete = snd_lowpower_resume,
};

static struct platform_driver snd_lowpower_driver = {
	.probe  = snd_lowpower_probe,
	.remove = snd_lowpower_remove,
	.driver = {
		.name = "comip-snd-lowpower",
		.owner = THIS_MODULE,
		.pm	= &pm_ops,
	},
};

static int __init snd_lowpower_init(void)
{
	return platform_driver_register(&snd_lowpower_driver);
}

static void __exit snd_lowpower_exit(void)
{
	platform_driver_unregister(&snd_lowpower_driver);
}

module_init(snd_lowpower_init);
module_exit(snd_lowpower_exit);

MODULE_DESCRIPTION("comip sound low power driver");
MODULE_AUTHOR("yangshunlong <yangshunlong@leadcoretech.com>");
MODULE_LICENSE("GPL");
