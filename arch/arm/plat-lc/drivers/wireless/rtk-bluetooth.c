#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <mach/gpio.h>
#include <linux/gpio.h>
#include <plat/rtk-bluetooth.h>

#ifdef CONFIG_DEVINFO_COMIP
#include <plat/comip_devinfo.h>
#endif

static enum rfkill_user_states btrfk_state = RFKILL_USER_STATE_SOFT_BLOCKED;
struct wake_lock bt2host_wakelock;

/* core code */
extern void rfkill_switch_all(const enum rfkill_type type, bool blocked);
extern bool rfkill_get_global_sw_state(const enum rfkill_type type);


static int bt_set_block(void *data, bool blocked)
{
	int rc = 0;
	struct rtk_bt_platform_data *pdata = data;

	if(btrfk_state == RFKILL_USER_STATE_HARD_BLOCKED)
		return 0;

	if(blocked) {
		if(btrfk_state != RFKILL_USER_STATE_SOFT_BLOCKED)
			rc = pdata->poweroff();
		btrfk_state = RFKILL_USER_STATE_SOFT_BLOCKED;
	} else {
		if(btrfk_state != RFKILL_USER_STATE_UNBLOCKED)
			rc = pdata->poweron();
		btrfk_state = RFKILL_USER_STATE_UNBLOCKED;
	}

	return rc;
}

static irqreturn_t bt2host_wakeup_irq(int irq, void *dev_id)
{
	struct wake_lock *bt2host_wakelock = dev_id;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_timeout(bt2host_wakelock, 5*HZ);
#endif

	return IRQ_HANDLED;
}

static struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_set_block,
};

#ifdef CONFIG_DEVINFO_COMIP
static ssize_t bluetooth_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "BT:realtek_8723bs\n");
}

static DEVICE_ATTR(bluetooth, S_IRUGO | S_IWUSR, bluetooth_show, NULL);

static struct attribute *bluetooth_attributes[] = {
	&dev_attr_bluetooth.attr,
};
#endif

static int __init bt_control_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct rfkill *bt_rfk;
	struct rtk_bt_platform_data *pdata = pdev->dev.platform_data;
	int irq = gpio_to_irq(pdata->gpio_wake_i);
	int a2b_wake = pdata->gpio_wake_o;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&bt2host_wakelock, WAKE_LOCK_SUSPEND, "bt2host_wakeup");
#endif

	rc = request_irq(irq, bt2host_wakeup_irq, IRQF_TRIGGER_FALLING | IRQF_SHARED,
					"bt2host_wakeup", &bt2host_wakelock);
	if (rc) {
		printk(KERN_ERR "bt2host wakeup irq request failed!\n");
		wake_lock_destroy(&bt2host_wakelock);
		return -1;
	}

	rc = enable_irq_wake(irq);
	if (rc < 0) {
		printk("Couldn't enable irq to wake up host\n");
		free_irq(irq, NULL);
		return -1;
	}

	rc = gpio_request(a2b_wake, "BT a2b wake");
	if (rc < 0) {
	    printk("Couldn't request gpio for BT a2b wake\n");
	    return -1;
	}
	gpio_direction_output(a2b_wake, 1);
	printk("%s:get gpio value %d\n",__func__, __gpio_get_value(a2b_wake));

	bt_rfk = rfkill_alloc(pdata->name, &pdev->dev, RFKILL_TYPE_BLUETOOTH, &bt_rfkill_ops, pdata);
	if (!bt_rfk)
		return -ENOMEM;

	rfkill_init_sw_state(bt_rfk, 1);

	rc = rfkill_register(bt_rfk);
	if (rc)
		rfkill_destroy(bt_rfk);
	else
		platform_set_drvdata(pdev, bt_rfk);

#ifdef CONFIG_DEVINFO_COMIP
	comip_devinfo_register(bluetooth_attributes, ARRAY_SIZE(bluetooth_attributes));
	printk("%s:comip_devinfo_register BT finished\n",__func__);
#endif

	return rc;
}

int __exit bt_control_remove(struct platform_device * pdev)
{
	struct rfkill *bt_rfk = platform_get_drvdata(pdev);
	struct rtk_bt_platform_data *pdata = pdev->dev.platform_data;
	int irq = gpio_to_irq(pdata->gpio_wake_i);
	int a2b_wake = pdata->gpio_wake_o;

	if(bt_rfk)
		rfkill_destroy(bt_rfk);

	if(disable_irq_wake(irq))
		printk("Couldn't disable hostwake IRQ wakeup mode\n");
	free_irq(irq, NULL);

	gpio_free(a2b_wake);

	return 0;
}

int bt_control_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct rtk_bt_platform_data *pdata = pdev->dev.platform_data;
	int a2b_wake = pdata->gpio_wake_o;

	gpio_direction_output(a2b_wake, 0);
	printk(KERN_DEBUG"%s:get gpio value %d\n",__func__, __gpio_get_value(a2b_wake));
	return 0;
}

int bt_control_resume(struct platform_device *pdev)
{
	struct rtk_bt_platform_data *pdata = pdev->dev.platform_data;
	int a2b_wake = pdata->gpio_wake_o;

	gpio_direction_output(a2b_wake, 1);
	printk(KERN_DEBUG"%s:get gpio value %d\n",__func__, __gpio_get_value(a2b_wake));
	return 0;
}

static struct platform_driver bt_control_driver = {
	.remove = __exit_p(bt_control_remove),
	.suspend = bt_control_suspend,
	.resume = bt_control_resume,

	.driver = {
		.name = "rtk-bluetooth",
		.owner = THIS_MODULE,
	},
};

static int __init bt_control_init(void)
{
	return platform_driver_probe(&bt_control_driver, bt_control_probe);
}

static void __exit bt_control_exit(void)
{
	platform_driver_unregister(&bt_control_driver);
}

module_init(bt_control_init);
module_exit(bt_control_exit);

MODULE_DESCRIPTION("rtk bt control");
MODULE_LICENSE("GPL");
