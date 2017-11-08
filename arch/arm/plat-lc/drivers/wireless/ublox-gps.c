/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.


   Copyright (C) 2006-2007 - Motorola
   Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.

   Date         Author           Comment
   -----------  --------------   --------------------------------
   2006-Apr-28	Motorola	 The kernel module for running the Bluetooth(R)
				 Sleep-Mode Protocol from the Host side
   2006-Sep-08  Motorola         Added workqueue for handling sleep work.
   2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.

*/

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/platform_device.h>

#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "plat/ublox-gps.h"
#include <plat/comip_devinfo.h>

#define VERSION		"1.0"

struct proc_dir_entry *gps_gpio_dir;

#define GPS_RESET_ON 0
#define GPS_RESET_OFF 1
/**
 * Read the <code>gps power state </code> via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the
 * pin is high, 0 otherwise.
 * @param file  the ublox gps data included.
 * @param buf for writing data.
 * @param size the maximum number of bytes to read.
 * @param ppos the current position in the buffer.
 * @return The number of bytes read.
 */
static int gps_read_proc_powerctrl(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	size_t ret;
	char tmp_buf[64] = {0};
	char *buf_info = tmp_buf;
	struct ublox_gps_platform_data *pdata = PDE_DATA(file_inode(file));

	buf_info += sprintf(buf_info, "gps power state is %d\n", pdata->power_state);
	ret = simple_read_from_buffer(buf, size, ppos, tmp_buf, buf_info - tmp_buf);
	return ret;
}

/**
 * <code>gps power control </code> via the proc interface.
 * @param file  the ublox gps data included.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be read.
 * @param data power on/off cmd.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int gps_write_proc_powerctrl(struct file *file, const char *buffer,
					size_t count, loff_t *data)
{
	char *buf;
	struct ublox_gps_platform_data *pdata = PDE_DATA(file_inode(file));

	if (count < 1)
		return -EINVAL;

	if(!pdata->power)
		return EPERM;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}

	if (buf[0] == '0') {
		if(pdata->power_state){
			printk("ublox gps power off\n");
			pdata->power(0);
			pdata->power_state = 0;
		}
	} else if (buf[0] == '1') {
		if(!pdata->power_state){
			printk("ublox gps power on\n");
			pdata->power(1);
			pdata->power_state = 1;
		}
	} else {
		kfree(buf);
		return -EINVAL;
	}

	kfree(buf);
	return count;
}

/**
 * <code>gps reset control </code> via the proc interface.
 * @param file the ublox gps data included.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be read.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int gps_write_proc_reset(struct file *file, const char *buffer,
					size_t count, loff_t *data)
{
	struct ublox_gps_platform_data *pdata = PDE_DATA(file_inode(file));

	if (count < 1)
		return -EINVAL;

	gpio_direction_output(pdata->gpio_reset, GPS_RESET_ON);
	msleep(10);
	gpio_direction_output(pdata->gpio_reset, GPS_RESET_OFF);

	return count;
}

static const struct file_operations file_ops_proc_powerctrl = {
	.write = gps_write_proc_powerctrl,
	.read = gps_read_proc_powerctrl,
};

static const struct file_operations file_ops_proc_reset = {
	.write = gps_write_proc_reset,
};

static ssize_t gps_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "GPS:ublox-G7020\n");
}

static DEVICE_ATTR(gps_info, S_IRUGO | S_IWUSR, gps_info_show, NULL);
static struct attribute *gps_info_attributes[] = {
	&dev_attr_gps_info.attr,
};

static int __init gps_gpio_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct proc_dir_entry *ent;

	struct ublox_gps_platform_data *pdata = pdev->dev.platform_data;

	printk("ublox gps_gpio_probe\n");
	gpio_request(pdata->gpio_reset, "GPS Reset GPIO");
	gpio_direction_output(pdata->gpio_reset, GPS_RESET_OFF);

	gps_gpio_dir = proc_mkdir("gps", NULL);
	if (gps_gpio_dir == NULL) {
		printk("Unable to create /proc/gps directory");
		return -ENOMEM;
	}

	/* Creating read/write "powerctrl" entry */
	ent = proc_create_data("powerctrl", 0, gps_gpio_dir, &file_ops_proc_powerctrl, pdata);
	if (ent == NULL) {
		printk("Unable to create /proc/gps/powerctrl entry");
		retval = -ENOMEM;
		goto fail;
	}

	/* Creating write "reset" entry */
	ent = proc_create_data("reset", 0, gps_gpio_dir, &file_ops_proc_reset, pdata);
	if (ent == NULL) {
		printk("Unable to create /proc/gps/reset entry");
		remove_proc_entry("powerctrl", gps_gpio_dir);
		retval = -ENOMEM;
		goto fail;
	}

	comip_devinfo_register(gps_info_attributes, ARRAY_SIZE(gps_info_attributes));
	return retval;

fail:
	remove_proc_entry("gps", 0);
	return retval;
}

static int __exit gps_gpio_remove(struct platform_device *pdev)
{
	struct ublox_gps_platform_data *pdata = pdev->dev.platform_data;

	remove_proc_entry("powerctrl", gps_gpio_dir);
	remove_proc_entry("reset", gps_gpio_dir);
	remove_proc_entry("gps", 0);
	gpio_direction_output(pdata->gpio_reset, GPS_RESET_OFF);
	gpio_free(pdata->gpio_reset);
	return 0;
}

static struct platform_driver gps_gpio_driver = {
	.remove = __exit_p(gps_gpio_remove),
	.driver = {
		.name = "ublox-gps",
		.owner = THIS_MODULE,
	},
};

/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init gps_gpio_init(void)
{
	return platform_driver_probe(&gps_gpio_driver, gps_gpio_probe);
}

/**
 * Cleans up the module.
 */
static void __exit gps_gpio_exit(void)
{
	platform_driver_unregister(&gps_gpio_driver);
}

late_initcall(gps_gpio_init);
module_exit(gps_gpio_exit);

MODULE_DESCRIPTION("GPS gpio Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
