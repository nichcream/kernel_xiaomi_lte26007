#include <linux/module.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <plat/hardware.h>
#include <plat/brcm-gps.h>
#include <plat/comip_devinfo.h>

static ssize_t gps_standby_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	len += sprintf(buf + len, "%u\n", pdata->standby_state);

	return len;
}

static ssize_t gps_standby_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long state = simple_strtoul(buf, NULL, 10);
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	if(pdata->standby_state == state){
		return size;
	}

	pdata->standby_state = (int)state;
	printk(KERN_DEBUG "gps_standby set to %ld\n", state);
	if(state) {
		wake_lock(&pdata->gps_wakelock);
		if ((pdata->power) && (!pdata->amt_state))
			pdata->power(1);
		gpio_direction_output(pdata->gpio_standby, 1);
	} else {
		gpio_direction_output(pdata->gpio_standby, 0);
		if (pdata->power)
			pdata->power(0);
		/*to prevent system to sleeping immediately when gps wakelock unlock, while glgps can not release resource normally*/
		wake_lock_timeout(&pdata->gps_stoplock, msecs_to_jiffies(100));
		wake_unlock(&pdata->gps_wakelock);
	}

	return size;
}

static ssize_t gps_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	len += sprintf(buf + len, "%u\n", pdata->reset_state);

	return len;
}

static ssize_t gps_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long state = simple_strtoul(buf, NULL, 10);
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	pdata->reset_state = (int)state;
	if(state)
		gpio_direction_output(pdata->gpio_reset, 1);
	else
		gpio_direction_output(pdata->gpio_reset, 0);

	return size;
}

static ssize_t gps_amt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	len += sprintf(buf + len, "%u\n", pdata->amt_state);

	return len;
}

static ssize_t gps_amt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long state = simple_strtoul(buf, NULL, 10);
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	pdata->amt_state = (int)state;

	return size;
}

static ssize_t gps_rf_trigger_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	len += sprintf(buf + len, "%u\n", pdata->rf_trigger_state);

	return len;
}

static ssize_t gps_rf_trigger_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long state = simple_strtoul(buf, NULL, 10);
	struct brcm_gps_platform_data *pdata = dev->platform_data;

	if(state){
		gpio_direction_output(pdata->gpio_rf_trigger, 0);//trigger on
		pdata->rf_trigger_state = 1;
	}else{
		gpio_direction_output(pdata->gpio_rf_trigger, 1);//trigger off
		pdata->rf_trigger_state = 0;
	}

	return size;
}

static ssize_t gps_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "GPS:broadcom-4752\n");
}

static DEVICE_ATTR(gps_info, S_IRUGO | S_IWUSR, gps_info_show, NULL);
static struct attribute *gps_info_attributes[] = {
	&dev_attr_gps_info.attr,
};

static DEVICE_ATTR(reset, 0644, gps_reset_show, gps_reset_store);
static DEVICE_ATTR(standby, 0644, gps_standby_show, gps_standby_store);
static DEVICE_ATTR(amt, 0644, gps_amt_show, gps_amt_store);
static DEVICE_ATTR(rf_trigger, 0644, gps_rf_trigger_show, gps_rf_trigger_store);

static int __init  gps_gpio_probe(struct platform_device *pdev)
{
	int ret;
	struct brcm_gps_platform_data *pdata = pdev->dev.platform_data;

	gpio_request(pdata->gpio_standby, "GPS Standby GPIO");
	if (pdata->gpio_rf_trigger >= 0)
		gpio_request(pdata->gpio_rf_trigger, "GPS rf open/close GPIO");

	pdata->standby_state = 0;
	pdata->reset_state = 0;
	pdata->rf_trigger_state = 0;

	if (pdata->gpio_rf_trigger >= 0)
		gpio_direction_output(pdata->gpio_rf_trigger, 0);
	gpio_direction_output(pdata->gpio_standby, 0);

	if(pdata->gpio_reset >= 0){
		gpio_request(pdata->gpio_reset, "GPS Reset GPIO");

		gpio_direction_output(pdata->gpio_reset, 0);
		udelay(200);
		gpio_direction_output(pdata->gpio_reset, 1);

		ret = device_create_file(&pdev->dev, &dev_attr_reset);
		if(ret)
			return ret;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_standby);
	if(ret)
		return ret;

	ret = device_create_file(&pdev->dev, &dev_attr_amt);
	if(ret)
		return ret;

	if (pdata->gpio_rf_trigger >= 0)
		ret = device_create_file(&pdev->dev, &dev_attr_rf_trigger);

	wake_lock_init(&pdata->gps_wakelock, WAKE_LOCK_SUSPEND, "gps_wakelock");
	wake_lock_init(&pdata->gps_stoplock, WAKE_LOCK_SUSPEND, "gps_stoplock");

	comip_devinfo_register(gps_info_attributes, ARRAY_SIZE(gps_info_attributes));
	return ret;
}

static int __exit gps_gpio_remove(struct platform_device *pdev)
{
	struct brcm_gps_platform_data *pdata = pdev->dev.platform_data;

	gpio_direction_output(pdata->gpio_standby, 0);
	gpio_free(pdata->gpio_standby);
	if(pdata->gpio_reset >= 0){
		gpio_direction_output(pdata->gpio_reset, 0);
		gpio_free(pdata->gpio_reset);
	}
	if (pdata->gpio_rf_trigger >= 0)
		gpio_free(pdata->gpio_rf_trigger);

	wake_lock_destroy(&pdata->gps_wakelock);
	wake_lock_destroy(&pdata->gps_stoplock);
	return 0;
}

struct platform_driver gps_gpio_driver = {
	.remove = __exit_p(gps_gpio_remove),
	.driver	= {
		.name	= "brcm-gps",
		.owner	= THIS_MODULE,
	},
};

static int __init gps_gpio_init(void)
{
	return platform_driver_probe(&gps_gpio_driver, gps_gpio_probe);
}

static void __exit gps_gpio_exit(void)
{
	platform_driver_unregister(&gps_gpio_driver);
}

late_initcall(gps_gpio_init);
module_exit(gps_gpio_exit);

MODULE_LICENSE("GPL v2");
