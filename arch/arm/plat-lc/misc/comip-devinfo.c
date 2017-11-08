/*
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019  LeadCoreTech Corp.
 *
 *  PURPOSE: This files contains the comip sysfs driver.
 *
 *  CHANGE HISTORY:
 *
 *  Version	Date		Author		Description
 *  1.0.0	2014-05-23			created
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <plat/comip_devinfo.h>

static struct kobject *devinfo_kobj = NULL;

int comip_devinfo_register(struct attribute **attributes, int count)
{
	int index = 0;
	int ret = 0;

	for (index = 0; index < count; index++) {
		ret = sysfs_create_file(devinfo_kobj, attributes[index]);
		if (ret)
			return ret;
	}

	return ret;
}

static int __init comip_devinfo_init(void)
{
	int ret = -ENOMEM;

	devinfo_kobj = kobject_create_and_add("devinfo", NULL);
	if (devinfo_kobj == NULL) {
		printk(KERN_ERR "comip_devinfo_init: kobject_create_and_add failed.\n");
		return ret;
	}

	return 0;
}

core_initcall(comip_devinfo_init);

