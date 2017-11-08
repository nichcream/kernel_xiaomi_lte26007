/*
 * comip_transceiver_lcusb.c -- CHARGER TRANSCEIVER utility code
 *
 * Copyright (C) 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <plat/comip-pmic.h>

static struct lcusb_transceiver *xceiv;
/* find the (single) lcusb transceiver*/
struct lcusb_transceiver *lcusb_get_transceiver(void)
{
    if (xceiv)
        get_device(xceiv->dev);
    return xceiv;
}
EXPORT_SYMBOL(lcusb_get_transceiver);


/* release the (single) lcusb transceiver*/
void lcusb_put_transceiver(struct lcusb_transceiver *x)
{
    if (x)
        put_device(x->dev);
}
EXPORT_SYMBOL(lcusb_put_transceiver);

/* declare the (single) lcusb transceiver*/
int lcusb_set_transceiver(struct lcusb_transceiver *x)
{
    if (xceiv && x)
        return -EBUSY;
    xceiv = x;
    return 0;
}
EXPORT_SYMBOL(lcusb_set_transceiver);

/*charger,usb,and otg status event notifiers */
static ATOMIC_NOTIFIER_HEAD(notifier_xceiv_events);

int comip_usb_register_notifier(struct notifier_block *nb)
{
    return atomic_notifier_chain_register(&notifier_xceiv_events, nb);
}
EXPORT_SYMBOL(comip_usb_register_notifier);

 int comip_usb_unregister_notifier(struct notifier_block *nb)
{
    return atomic_notifier_chain_unregister(&notifier_xceiv_events, nb);
}
EXPORT_SYMBOL(comip_usb_unregister_notifier);

int comip_usb_xceiv_notify(enum lcusb_xceiv_events status)
{
    return atomic_notifier_call_chain(&notifier_xceiv_events,
            (unsigned long)status,NULL);
}
EXPORT_SYMBOL(comip_usb_xceiv_notify);

