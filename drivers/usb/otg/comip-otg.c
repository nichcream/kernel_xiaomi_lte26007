#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/usb/gadget.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/gpio.h>

#include <plat/hardware.h>
#include <plat/usb.h>
#include <plat/comip-pmic.h>
#include <asm/unaligned.h>
#include <mach/comip-regs.h>
#include "comip-otg.h"

#define DRIVER_VERSION "Rev. 1.0"
#define DRIVER_AUTHOR "Tangerine"
#define DRIVER_DESC "Leadcore USB OTG  Driver"
#define DRIVER_INFO DRIVER_DESC " " DRIVER_VERSION

/* FSM timers */
struct comip_otg_timer *a_wait_vrise_tmr, *a_wait_bcon_tmr, *a_aidl_bdis_tmr,
	*b_ase0_brst_tmr, *b_se0_srp_tmr;

/* Driver specific timers */
struct comip_otg_timer *b_data_pulse_tmr, *b_vbus_pulse_tmr, *b_srp_fail_tmr,
	*b_srp_wait_tmr, *a_wait_enum_tmr;

const pm_message_t otg_suspend_state = {
	.event = 1,
};

int comip_hcd_driver_stop(struct platform_device *_dev);
int comip_hcd_driver_start(struct platform_device *_dev);


static const char driver_name[] = "comip-otg";
static struct comip_otg *p_comip_otg;
static unsigned int id_gpio;

static inline void comip_write_reg32(uint32_t value, int paddr)
{
	writel(value, io_p2v(paddr));
}

static inline unsigned int comip_read_reg32(int paddr)
{
	return readl(io_p2v(paddr));
}
static int hostmode = 0;
/* -------------------------------------------------------------*/
int comip_otg_hostmode(void)
{
	if(p_comip_otg){
		if((!(p_comip_otg->fsm.id))|| hostmode)
			return 1;
		else
			return 0;
	}else
		return 0;
}

/* Operations that will be called from OTG Finite State Machine */
/* Reset controller, not reset the bus */
static void otg_reset_controller(void)
{
	comip_write_reg32(0x02, AP_PWR_USB_RSTCTL);

	mdelay(10);
}

static void dev_reset_controller(void)
{
	comip_write_reg32(0x02, AP_PWR_USB_RSTCTL);

	mdelay(10);
}

/* A-device driver vbus, controlled through PP bit in PORTSC */
static void comip_otg_drv_vbus(int on)
{
	enum lcusb_xceiv_events status = 0;

	if(on){
		status = LCUSB_EVENT_OTG;
		comip_usb_xceiv_notify(status);
	}else{
		status = LCUSB_EVENT_NONE;
		comip_usb_xceiv_notify(status);
	}
}

static ssize_t comip_otg_plug_show(struct device *dev, struct device_attribute *drv, char *buf)
{
    return sprintf(buf, "%d", hostmode);
}


static DEVICE_ATTR(otgstatus, S_IRUGO, comip_otg_plug_show, NULL);

/* Call suspend/resume routines in host driver */
static int comip_otg_start_host(struct otg_fsm *fsm, int on)
{
	struct usb_otg *otg = fsm->otg;
	struct device *dev;
	struct platform_driver *host_pdrv;
	struct platform_device *host_pdev;
	struct comip_otg *otg_dev = container_of(otg->phy, struct comip_otg, phy);
	u32 ret = 0;

	printk(KERN_DEBUG"%s: %d, host=%p, on=%d, working=%d\n", __func__, __LINE__,
	otg->host, on, otg_dev->host_working);

	if (!otg->host){
		return -ENODEV;
	}
	dev = otg->host->controller;
	host_pdrv = container_of((dev->driver), struct platform_driver, driver);
	host_pdev = to_platform_device(dev);

	if (on) {
		if (otg_dev->host_working)
			goto end;
		else {
			otg_reset_controller();
			printk(KERN_DEBUG"%s:host on......\n",__func__);
			comip_hcd_driver_start(host_pdev);
			otg_dev->host_working = 1;
		}
	} else {
		if (!otg_dev->host_working)
			goto end;
		else {
			printk(KERN_DEBUG"%s:host off......\n",__func__);
			comip_hcd_driver_stop(host_pdev);
			dev_reset_controller();
			otg_dev->host_working = 0;
		}
	}
end:
	return ret;
}

/*
 * Call suspend and resume function in udc driver
 * to stop and start udc driver.
 */
static int comip_otg_start_gadget(struct otg_fsm *fsm, int on)
{
#if 0
	struct usb_otg *otg = fsm->otg;
	struct platform_driver *gadget_pdrv;
	struct platform_device *gadget_pdev;
	struct device *dev;

	if (!otg->gadget || !otg->gadget->dev.parent)
		return -ENODEV;

	dev = otg->gadget->dev.parent;

	gadget_pdrv = container_of((dev->driver),struct platform_driver, driver);
	gadget_pdev = to_platform_device(dev);
	
	if (on)
		gadget_pdrv->resume(gadget_pdev);
	else
		gadget_pdrv->suspend(gadget_pdev, otg_suspend_state);
#endif
	return 0;
}

/*
 * Called by initialization code of host driver. Register host controller to the OTG.
 */
static int comip_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct comip_otg *p_otg;

	if (!otg)
		return -ENODEV;

	p_otg = container_of(otg->phy, struct comip_otg, phy);
	if (p_otg != p_comip_otg)
		return -ENODEV;
	
	otg->host = host;

	if (host) {
		otg->host->otg_port = 1;
	}

	return 0;
}

/* Called by initialization code of udc.  Register udc to OTG. */
static int comip_otg_set_peripheral(struct usb_otg *otg,
				  struct usb_gadget *gadget)
{
	struct comip_otg *p_otg;

	if (!otg)
		return -ENODEV;

	p_otg = container_of(otg->phy, struct comip_otg, phy);
	if (p_otg != p_comip_otg)
		return -ENODEV;

	otg->gadget = gadget;
	return 0;
}

/* Set OTG port power, only for B-device */
static int comip_otg_set_power(struct usb_phy *phy, unsigned mA)
{
	return 0;
}


/* B-device start SRP */
static int comip_otg_start_srp(struct usb_otg *otg)
{
	return 0;
}

/* A_host suspend will call this function to start hnp */
static int comip_otg_start_hnp(struct usb_otg *otg)
{
	return 0;
}


/* Interrupt handler for gpio id pin */
irqreturn_t comip_otg_isr_gpio(int irq, void *dev_id)
{
	struct usb_phy *phy = usb_get_transceiver();
	struct comip_otg *p_otg;
	struct otg_fsm *fsm;

         printk(KERN_DEBUG"%s: %d, enter\n", __func__, __LINE__);

	p_otg = container_of(phy, struct comip_otg, phy);
	fsm = &p_otg->fsm;

	if ((id_gpio == 0) || (p_otg != p_comip_otg))
		return IRQ_HANDLED;

	wake_lock(&p_otg->wakelock);
	disable_irq_nosync(gpio_to_irq(id_gpio));
	
	spin_lock(&p_otg->id_lock);
	fsm->id = gpio_get_value(id_gpio) ? 1 : 0;
	queue_delayed_work(p_otg->otg_queue, &p_otg->otg_event, msecs_to_jiffies(10));
	spin_unlock(&p_otg->id_lock);

	return IRQ_HANDLED;
}


irqreturn_t comip_otg_isr(int irq, void *dev_id)
{
	struct comip_otg *p_otg = (struct comip_otg *)dev_id;
	u32 otg_int_src, otg_sc;
	u32 usb_gintsts, usb_gintmsk;
	irqreturn_t ret = IRQ_NONE;

      /* get raw interrupt & mask */
	usb_gintsts = comip_read_reg32(USB_GINTSTS);
	usb_gintmsk = comip_read_reg32(USB_GINTMSK);
	
	otg_int_src = usb_gintsts & USB_GINTSTS_OTGINT;
	otg_sc = usb_gintsts & USB_GINTSTS_CONIDSTSCHNG;
	
	if((!otg_sc) && (!otg_int_src) )
		return ret;
		
	printk(KERN_DEBUG"comip_otg_isr: usb_gintsts= 0x%x\n", usb_gintsts);
	/* clear interrupt */
	if(otg_int_src)
		comip_write_reg32(USB_GINTSTS_OTGINT, USB_GINTSTS);
	if(otg_sc)
		comip_write_reg32(USB_GINTSTS_CONIDSTSCHNG, USB_GINTSTS);

	p_otg->fsm.id = !(usb_gintsts & USB_GINTSTS_CURMOD);

	/* process OTG interrupts */
	if (otg_sc) {
			printk(KERN_DEBUG"ID int (ID is %d)\n", p_otg->fsm.id);
			queue_delayed_work(p_otg->otg_queue, &p_otg->otg_event, 0);
			ret = IRQ_HANDLED;
		}
	return IRQ_HANDLED;
}

/*
 * Delayed pin detect interrupt processing.
 *
 * When the Mini-A cable is disconnected from the board,
 * the pin-detect interrupt happens before the disconnnect
 * interrupts for the connected device(s).  In order to
 * process the disconnect interrupt(s) prior to switching
 * roles, the pin-detect interrupts are delayed, and handled
 * by this routine.
 */
static void comip_otg_event(struct work_struct *work)
{
	struct comip_otg *p_otg = container_of(work, struct comip_otg, otg_event.work);
	struct otg_fsm *fsm = &p_otg->fsm;
	unsigned long flags;
	int value;
    //enum lcusb_xceiv_events status = 0;

	spin_lock_irqsave(&p_otg->id_lock,flags);
	value = gpio_get_value(id_gpio) ? 1 : 0;
	if(value != fsm->id){
		printk(KERN_INFO"%s: id= %d value=%d\n", __FUNCTION__, fsm->id,value);
		spin_unlock_irqrestore(&p_otg->id_lock, flags);
		wake_unlock(&p_otg->wakelock);
		enable_irq(gpio_to_irq(id_gpio));
	} else {
		spin_unlock_irqrestore(&p_otg->id_lock, flags);
		printk(KERN_INFO"%s: id= %d\n", __FUNCTION__, fsm->id);
		if (fsm->id) {		/* switch to gadget */
			comip_otg_start_host(fsm, 0);
			comip_otg_drv_vbus(0);
			msleep(300);
			comip_otg_start_gadget(fsm, 1);
			hostmode = 0;
		} else {			/* switch to host */
			hostmode = 1;
			comip_otg_start_gadget(fsm, 0);
			comip_otg_drv_vbus(1);
			msleep(3);

			comip_otg_start_host(fsm, 1);
		}
		wake_unlock(&p_otg->wakelock);
	enable_irq(gpio_to_irq(id_gpio));
	}
}

static struct otg_fsm_ops comip_otg_ops = {
	.drv_vbus = comip_otg_drv_vbus,
	.start_host = comip_otg_start_host,
	.start_gadget = comip_otg_start_gadget,
};

static int __init comip_otg_probe(struct platform_device *pdev)
{
	int ret;
	struct comip_otg *p_otg;
	struct comip_otg_platform_data *pdata = pdev->dev.platform_data;

	p_otg = p_comip_otg;

	if(!pdata->id_gpio)
		ret = request_irq(INT_USB_OTG, comip_otg_isr, IRQF_SHARED | IRQF_DISABLED, driver_name, pdata);
	else {
		gpio_request(pdata->id_gpio, "comip-otg");
		gpio_direction_input(pdata->id_gpio);
		gpio_set_debounce(pdata->id_gpio, pdata->debounce_interval * 1000);
		ret = request_irq(gpio_to_irq(pdata->id_gpio), comip_otg_isr_gpio,
				 IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, driver_name, pdata);
		enable_irq_wake(gpio_to_irq(pdata->id_gpio));
		id_gpio	= pdata->id_gpio;
		p_otg->fsm.id = gpio_get_value(id_gpio) ? 1 : 0;
		printk(KERN_INFO"%s p_otg->fsm.id=%d\n", __FUNCTION__,p_otg->fsm.id);
	}

	if (ret) {
		printk(KERN_ERR"IRQ already in use.\n");
		goto err;
	}

	if (p_otg->fsm.id==0) {
		printk(KERN_INFO"%s p_otg->fsm.id=0 first host mode\n", __FUNCTION__);
		wake_lock(&p_otg->wakelock);
		disable_irq_nosync(gpio_to_irq(id_gpio));

		spin_lock(&p_otg->id_lock);
		p_otg->fsm.id = gpio_get_value(id_gpio) ? 1 : 0;
		queue_delayed_work(p_otg->otg_queue, &p_otg->otg_event, msecs_to_jiffies(10));
		spin_unlock(&p_otg->id_lock);
	}

	device_create_file(&(pdev->dev), &dev_attr_otgstatus);

	printk(KERN_DEBUG"%s: %d, exit\n", __func__, __LINE__);
	return ret;
err:
	kfree(p_otg);
	usb_set_transceiver(NULL);
	kfree(p_comip_otg->phy.otg);
	return ret;
}

static int __exit comip_otg_remove(struct platform_device *pdev)
{
	struct comip_otg_platform_data *pdata = pdev->dev.platform_data;
	printk(KERN_DEBUG"comip_otg_remove.\n");
	disable_irq_wake(gpio_to_irq(pdata->id_gpio));
	wake_lock_destroy(&p_comip_otg->wakelock);
	kfree(p_comip_otg);
	usb_set_transceiver(NULL);
	kfree(p_comip_otg->phy.otg);
	unregister_chrdev(COMIP_OTG_MAJOR, COMIP_OTG_NAME);
	device_remove_file(&(pdev->dev), &dev_attr_otgstatus);

	return 0;
}
#ifdef CONFIG_PM
static int comip_otg_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;

}

static int comip_otg_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

struct platform_driver comip_otg_driver = {
	.remove = __exit_p(comip_otg_remove),
	.driver = {
		.name = driver_name,
		.owner = THIS_MODULE,
	},
#ifdef CONFIG_PM
	.suspend = comip_otg_suspend,
	.resume = comip_otg_resume,
#endif
};

static int __init comip_usb_otg_init(void)
{
	printk(DRIVER_INFO "\n");
	return platform_driver_probe(&comip_otg_driver, comip_otg_probe);
}

static void __exit comip_usb_otg_exit(void)
{
	platform_driver_unregister(&comip_otg_driver);
}

late_initcall(comip_usb_otg_init);
module_exit(comip_usb_otg_exit);



/****************************************************************/
static int __init comip_otg_early_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct comip_otg *p_otg = NULL;
	struct comip_otg_platform_data *pdata = pdev->dev.platform_data;
	printk(KERN_INFO"%s\n", __FUNCTION__);

	/* allocate space to comip otg device */
	p_comip_otg = kzalloc(sizeof(struct comip_otg), GFP_KERNEL);
	if (!p_comip_otg){
		printk(KERN_ERR"unable to allocate mem\n");
		return -ENOMEM;
	}

	p_comip_otg->phy.otg = kzalloc(sizeof(struct usb_otg), GFP_KERNEL);
	if (!p_comip_otg->phy.otg) {
		kfree(p_comip_otg);
		return -ENOMEM;
	}

	p_otg = p_comip_otg;
	p_otg->otg_queue = create_singlethread_workqueue("otg_switch");
	if (p_otg->otg_queue == NULL) {
		printk(KERN_ERR"Coulndn't create work queue\n");
		return -ENOMEM;
	}

	wake_lock_init(&p_comip_otg->wakelock, WAKE_LOCK_SUSPEND, "otg_wakelock");
	INIT_DELAYED_WORK(&p_comip_otg->otg_event, comip_otg_event);
	spin_lock_init(&p_otg->id_lock);

	/* Set OTG state machine operations */
	p_otg->fsm.ops = &comip_otg_ops;
	/* initialize the otg structure */

	p_otg->phy.label = DRIVER_DESC;
	p_otg->phy.otg->phy = &p_otg->phy;
	p_otg->phy.otg->set_host = comip_otg_set_host;
	p_otg->phy.otg->set_peripheral = comip_otg_set_peripheral;
	p_otg->phy.set_power = comip_otg_set_power;
	p_otg->phy.otg->start_hnp = comip_otg_start_hnp;
	p_otg->phy.otg->start_srp = comip_otg_start_srp;

	/* Store the otg transceiver */
	ret = usb_set_transceiver(&p_otg->phy);
	if (ret) {
		printk(KERN_ERR"unable to register OTG transceiver.\n");
		goto err;
	}
	p_otg->fsm.otg = p_otg->phy.otg;

	if(pdata->id_gpio){
		gpio_request(pdata->id_gpio, "comip-otg");
		gpio_direction_input(pdata->id_gpio);
		gpio_set_debounce(pdata->id_gpio, pdata->debounce_interval * 1000);
		id_gpio	= pdata->id_gpio;
		p_otg->fsm.id = gpio_get_value(id_gpio) ? 1 : 0;
		printk(KERN_DEBUG"%s p_otg->fsm.id=%d\n", __FUNCTION__,p_otg->fsm.id);
	}

	return 0;
err:
	kfree(p_otg);
	usb_set_transceiver(NULL);
	kfree(p_comip_otg->phy.otg);
	return ret;
}

static int __exit comip_otg_early_remove(struct platform_device *pdev)
{
	printk(KERN_INFO"%s\n", __FUNCTION__);

	usb_set_transceiver(NULL);
	kfree(p_comip_otg->phy.otg);
	kfree(p_comip_otg);

	return 0;
}

struct platform_driver comip_otg_early_driver = {
	.remove = __exit_p(comip_otg_early_remove),
	.driver = {
		.name = "comip-early-otg",
		.owner = THIS_MODULE,
	},
};

static int __init comip_usb_otg_early_init(void)
{
	printk(KERN_INFO"%s\n", __FUNCTION__);

	return platform_driver_probe(&comip_otg_early_driver, comip_otg_early_probe);
}

static void __exit comip_usb_otg_early_exit(void)
{
	printk(KERN_INFO"%s\n", __FUNCTION__);
	platform_driver_unregister(&comip_otg_early_driver);
}



fs_initcall(comip_usb_otg_early_init);
module_exit(comip_usb_otg_early_exit);

MODULE_DESCRIPTION(DRIVER_INFO);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");

