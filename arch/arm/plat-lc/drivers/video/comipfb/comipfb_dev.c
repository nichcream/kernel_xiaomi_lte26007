/* driver/video/comip/comipfb_dev.c
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 1999-2015      LeadCoreTech Corp.
**
**      PURPOSE:                        This files contains the driver of LCD controller.
**
**      CHANGE HISTORY:
**
**      Version         Date            Author          Description
**      0.1.0           2012-03-10      liuyong         created
**
*/

#include <linux/mutex.h>
#include "comipfb_dev.h"
#include "comipfb.h"

struct comipfb_dev_info {
	struct comipfb_dev* dev;
	struct list_head list;
};

static DEFINE_MUTEX(comipfb_dev_lock);
static LIST_HEAD(comipfb_dev_list);

int comipfb_dev_register(struct comipfb_dev* dev)
{
	struct comipfb_dev_info *info;

	info = kzalloc(sizeof(struct comipfb_dev_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	INIT_LIST_HEAD(&info->list);
	info->dev = dev;

	mutex_lock(&comipfb_dev_lock);
	list_add_tail(&info->list, &comipfb_dev_list);
	mutex_unlock(&comipfb_dev_lock);

	return 0;
}
EXPORT_SYMBOL(comipfb_dev_register);

int comipfb_dev_unregister(struct comipfb_dev* dev)
{
	struct comipfb_dev_info *info;

	mutex_lock(&comipfb_dev_lock);
	list_for_each_entry(info, &comipfb_dev_list, list) {
		if (info->dev == dev) {
			list_del_init(&info->list);
			kfree(info);
		}
	}
	mutex_unlock(&comipfb_dev_lock);

	return 0;
}
EXPORT_SYMBOL(comipfb_dev_unregister);

struct comipfb_dev* comipfb_dev_get(struct comipfb_info *fbi, int screen_num, int init_flag) //screen_num: screen num; init_flag: init flag.
{
	struct comipfb_dev_info *info;
	struct comipfb_dev *dev = NULL;
	const char *name = NULL;
	if (init_flag > 0) {
		if (fbi->pdata->detect_dev)
			name = fbi->pdata->detect_dev(); // this operation is used in init only, currently.
	}
	mutex_lock(&comipfb_dev_lock);
	list_for_each_entry(info, &comipfb_dev_list, list) {
		if (((fbi->pdata->lcdc_support_interface & info->dev->interface_info) > 0) 
			&& ((fbi->pdata->lcdc_id == info->dev->control_id) || !strcmp(info->dev->name, name))) {       
			if (init_flag > 0) {
				if (info->dev->default_use > 0) {
					dev = info->dev;
					break;
				}
			}
			else {
				if (info->dev->screen_num == screen_num) {
					dev = info->dev;
					break;
				}
			}
			
		}
	}
	mutex_unlock(&comipfb_dev_lock);

	return dev;
}
EXPORT_SYMBOL(comipfb_dev_get);

