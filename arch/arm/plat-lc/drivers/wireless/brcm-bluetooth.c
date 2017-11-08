#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/gpio.h>
#include <plat/brcm-bluetooth.h>

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
	struct brcm_bt_platform_data *pdata = data;

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


static struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_set_block,
};

#ifdef CONFIG_DEVINFO_COMIP
static ssize_t bluetooth_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "BT:broadcom-4343s\nFM:broadcom-4343s\n");
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
	struct brcm_bt_platform_data *pdata = pdev->dev.platform_data;

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

	if(bt_rfk)
		rfkill_destroy(bt_rfk);


	return 0;
}

static struct platform_driver bt_control_driver = {
	.remove = __exit_p(bt_control_remove),

	.driver = {
		.name = "brcm-bluetooth",
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

MODULE_DESCRIPTION("brcm bt control");
MODULE_LICENSE("GPL");
