/*
 * 
 * Copyright (C) 2013 Leadcore, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/setup.h>
#include <plat/comip-setup.h>
#include <linux/module.h>
#include <linux/device.h>

unsigned int g_comip_chip_id;
unsigned int g_comip_rom_id;
unsigned int g_comip_bb_id;
unsigned int g_comip_sn_id;

static int __init comip_parse_tag_chip_id(const struct tag *tag)
{
	struct tag_chip_id *chip_id = (struct tag_chip_id *)&tag->u;

	g_comip_chip_id = chip_id->chip_id;
	g_comip_rom_id = chip_id->rom_id;
	g_comip_bb_id = chip_id->bb_id;
	g_comip_sn_id = chip_id->sn_id;

	printk("Chip id: 0x%x, rom id: 0x%x, bb id: 0x%x, sn id: 0x%x\n",
		g_comip_chip_id, g_comip_rom_id, g_comip_bb_id, g_comip_sn_id);

	return 0;
}

__tagtable(ATAG_CHIP_ID, comip_parse_tag_chip_id);

#define sn_attr(_name, _mode) \
	static struct kobj_attribute _name##_attr = {   \
		.attr   = {                     \
			.name = __stringify(_name), \
			.mode = _mode,                  \
		},                                      \
		.show   = _name##_show,                 \
		.store  = _name##_store,                \
	}

static struct kobject *sn_kobj = NULL;

static ssize_t cpu_sn_id_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	return sprintf(buf, "%d", g_comip_sn_id);
}

static ssize_t cpu_sn_id_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}

sn_attr(cpu_sn_id, 0644);

static int __init comip_sn_id_init(void)
{
	unsigned int ret = 0;

	sn_kobj = kobject_create_and_add("cpu_sn", NULL);
	if (sn_kobj == NULL) {
		printk(KERN_ERR "comip_sn_id_init: kobject_create_and_add failed.\n");
		return -EINVAL;
	}

	ret = sysfs_create_file(sn_kobj, &cpu_sn_id_attr.attr);
	if (ret)
		printk(KERN_ERR "create sysfs file failed\n");

	return ret;
}
module_init(comip_sn_id_init);

MODULE_DESCRIPTION("COMIP sn_id sysfs");
MODULE_LICENSE("GPL");

