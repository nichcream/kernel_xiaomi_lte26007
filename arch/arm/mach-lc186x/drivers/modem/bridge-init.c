/*
 * LC186X bridge init driver
 *
 * Copyright (C) 2013 Leadcore Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/platform_device.h>
#include <mach/comip-bridge.h>


int register_comip_bridge(struct modem_bridge_info *bridge_info, struct device *parent)
{
	int retval = -EINVAL;
	struct bridge_drvdata *drvdata = NULL;

	if (!bridge_info || !parent) {
		BRIDGE_ERR("bridge_info:%x, parent: %x \n", (unsigned int)bridge_info, (unsigned int)parent);
		goto out;
	}

	drvdata = kzalloc(sizeof(struct bridge_drvdata), GFP_KERNEL);
	if (!drvdata) {
		BRIDGE_ERR("out of memory\n");
		retval = -ENOMEM;
		goto out;
	}
	drvdata->f_open = 0;
	drvdata->ops = get_bridge_ops();

	if (!drvdata->ops) {
		BRIDGE_ERR("drvdata ops is null\n");
		goto driver_init_err;
	}
	if (!drvdata->ops->drvinit) {
		BRIDGE_ERR("drvdata ops drvinit = %x\n",
			(unsigned int)drvdata->ops->drvinit);
		goto driver_init_err;
	}

	retval = drvdata->ops->drvinit(drvdata, bridge_info);
	if (retval < 0) {
		goto driver_init_err;
	}

	if (bridge_info->type == HS_IP_PACKET) {
		retval = bridge_net_device_init(drvdata, parent);
		if (retval < 0)
			goto device_init_err;
		bridge_info->dev = drvdata->net_dev;
	}else{
		retval = bridge_device_init(drvdata, parent);
		if (retval < 0)
			goto device_init_err;
		bridge_info->dev = &drvdata->misc_dev;
	}

	BRIDGE_PRT( "%s found! \n", drvdata->name);
	return 0;

device_init_err:
	if (!drvdata->ops->drvuninit)
		drvdata->ops->drvuninit(drvdata);
driver_init_err:
	kfree(drvdata);
out:
	return retval;
}

void deregister_comip_bridge(struct modem_bridge_info *bridge_info)
{
	struct bridge_drvdata *drvdata = NULL;

	if (!bridge_info) {
		BRIDGE_ERR("bridge_info = NULL \n");
		return;
	}

	if (!bridge_info->dev) {
		BRIDGE_INFO("dev = NULL \n");
		return;
	}

	if (bridge_info->type == HS_IP_PACKET) {
		drvdata = get_bridge_net_drvdata(bridge_info->dev);
		bridge_net_device_uninit(drvdata);
	}else{
		drvdata = container_of(bridge_info->dev,
						     struct bridge_drvdata, misc_dev);
		bridge_device_uninit(drvdata);
	}
	bridge_info->dev = NULL;
	drvdata->ops->drvuninit(drvdata);
	kfree(drvdata);

	return;
}
int save_rxbuf_comip_bridge(struct modem_bridge_info *bridge_info)
{
	struct bridge_drvdata *drvdata = NULL;

	if (!bridge_info) {
		BRIDGE_ERR("bridge_info = NULL \n");
		return -EINVAL;
	}

	if (!bridge_info->dev) {
		BRIDGE_INFO("dev = NULL \n");
		return -EINVAL;
	}

	if (bridge_info->type == HS_IP_PACKET) {
		drvdata = get_bridge_net_drvdata(bridge_info->dev);
	}else{
		drvdata = container_of(bridge_info->dev,
						     struct bridge_drvdata, misc_dev);
	}
	return drvdata->ops->rxsavebuf(drvdata);
}

int restore_rxbuf_comip_bridge(struct modem_bridge_info *bridge_info)
{
	struct bridge_drvdata *drvdata = NULL;

	if (!bridge_info) {
		BRIDGE_ERR("bridge_info = NULL \n");
		return -EINVAL;
	}

	if (!bridge_info->dev) {
		BRIDGE_INFO("dev = NULL \n");
		return -EINVAL;
	}

	if (bridge_info->type == HS_IP_PACKET) {
		drvdata = get_bridge_net_drvdata(bridge_info->dev);
	}else{
		drvdata = container_of(bridge_info->dev,
						     struct bridge_drvdata, misc_dev);
	}
	return drvdata->ops->rxstorebuf(drvdata);
}

EXPORT_SYMBOL(register_comip_bridge);
EXPORT_SYMBOL(deregister_comip_bridge);
EXPORT_SYMBOL(save_rxbuf_comip_bridge);
EXPORT_SYMBOL(restore_rxbuf_comip_bridge);
