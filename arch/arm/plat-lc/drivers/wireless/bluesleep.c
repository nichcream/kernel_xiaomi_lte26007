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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/platform_data/msm_serial_hs.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */

//#define BT_SLEEP_DBG
#ifdef BT_SLEEP_DBG
#define BT_SLP_DBG(fmt, arg...) printk(KERN_ERR "%s: " fmt "\n" , __func__ , ## arg)
#else
#define BT_SLP_DBG(fmt, arg...)
#endif
/*
 * Defines
 */

#define VERSION		"1.1"
#define PROC_DIR	"bluetooth/sleep"


#define BT_BLUEDROID_SUPPORT 1

struct bluesleep_info {
	unsigned host_wake;
	unsigned ext_wake;
	unsigned host_wake_irq;
	unsigned ext_wake_polarity;
	unsigned host_wake_polarity;
	struct uart_port *uport;
	struct uart_port* (*get_uport)(void);
	struct wake_lock wake_lock;
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)

/* 10 second timeout */
#define TX_TIMER_INTERVAL  10

/* state variable names and bit positions */
#define BT_PROTO	0x01
#define BT_TXDATA	0x02
#define BT_ASLEEP	0x04

#if !BT_BLUEDROID_SUPPORT
/* global pointer to a single hci device. */
static struct hci_dev *bluesleep_hdev;
#endif

static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Local function prototypes
 */
#if !BT_BLUEDROID_SUPPORT
static int bluesleep_hci_event(struct notifier_block *this,
			    unsigned long event, void *data);
#endif
static int bluesleep_start(void);
static void bluesleep_stop(void);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Transmission timer */
static struct timer_list tx_timer;

/** Lock for state transitions */
static spinlock_t rw_lock;

#if !BT_BLUEDROID_SUPPORT
/** Notifier block for HCI events */
struct notifier_block hci_event_nblock = {
	.notifier_call = bluesleep_hci_event,
};
#endif
struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */

static void hsuart_power(int on)
{
	if (on) {
		//To be fix: open uart clock and active it 
		//msm_hs_request_clock_on(bsi->uport);
		//msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
	} else {
		//To be fix: deactive uart and close uart clock
		//msm_hs_set_mctrl(bsi->uport, 0);
		//msm_hs_request_clock_off(bsi->uport);
	}
}


/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
static inline int bluesleep_can_sleep(void)
{
	/* check if MSM_WAKE_BT_GPIO and BT_WAKE_MSM_GPIO are both deasserted */
	if((gpio_get_value(bsi->ext_wake) == bsi->ext_wake_polarity) ||(gpio_get_value(bsi->host_wake) == bsi->host_wake_polarity) || (bsi->uport == NULL))
		return false;
	return true;
}

void bluesleep_sleep_wakeup(void)
{
	BT_SLP_DBG("bluesleep_sleep_wakeup");

	if (test_bit(BT_ASLEEP, &flags)) {
		BT_INFO("waking up...");
		/* Start the timer */
		wake_lock(&bsi->wake_lock);
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		gpio_set_value(bsi->ext_wake, bsi->ext_wake_polarity);
		clear_bit(BT_ASLEEP, &flags);
		disable_irq_nosync(bsi->host_wake_irq);
		/*Activating UART */
		hsuart_power(1);
	}
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
	struct irq_desc *desc = irq_to_desc(bsi->host_wake_irq);
	if (bluesleep_can_sleep()) {
		/* already asleep, this is an error case */
		if (test_bit(BT_ASLEEP, &flags)) {
			BT_SLP_DBG("already asleep");
			return;
		}

		if (bsi->uport && bsi->uport->ops->tx_empty(bsi->uport)) {
			BT_INFO("going to sleep...");
			desc->irq_data.chip->irq_ack(&desc->irq_data);
			enable_irq(bsi->host_wake_irq);
			set_bit(BT_ASLEEP, &flags);
			/*Deactivating UART */
			hsuart_power(0);
			//wake_lock_timeout(&bsi->wake_lock, HZ / 2);
			wake_unlock(&bsi->wake_lock);
		} else {
		    BT_SLP_DBG("uart is not empty");
            mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
			return;
		}
	} else {
	//modify sleep control for broadcom bluetooth chip ZTE_BT_QXX_20101025 begin
	   BT_SLP_DBG("bluetooth chip can't sleep");
	   if (test_bit(BT_ASLEEP, &flags)) 
	   {
		bluesleep_sleep_wakeup();
		 }
		 else
		 {
		   mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
	   }
	//modify sleep control for broadcom bluetooth chip ZTE_BT_QXX_20101025 end
	}
}

/**
 * A tasklet function that runs in tasklet context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param data Not used.
 */
static void bluesleep_hostwake_task(unsigned long data)
{
	BT_SLP_DBG("hostwake line change");

	spin_lock(&rw_lock);

	if (gpio_get_value(bsi->host_wake) == bsi->host_wake_polarity)
		bluesleep_rx_busy();
	else
		bluesleep_rx_idle();

	spin_unlock(&rw_lock);
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
void bluesleep_outgoing_data(void)
{
	unsigned long irq_flags;

	BT_SLP_DBG("bluesleep_outgoing_data");

	spin_lock_irqsave(&rw_lock, irq_flags);

	/* log data passing by */
	set_bit(BT_TXDATA, &flags);

	/* if the tx side is sleeping... */
	if (gpio_get_value(bsi->ext_wake) == (!bsi->ext_wake_polarity)) {

		BT_SLP_DBG("tx was sleeping");
		bluesleep_sleep_wakeup();
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

#if BT_BLUEDROID_SUPPORT
static int bluesleep_read_proc_btwrite(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
        size_t ret;
        char tmp_buf[64] = {0};
        char *buf_info = tmp_buf;
        
        buf_info += sprintf(buf_info, "gpio %d, btwrite:%u\n", bsi->ext_wake, gpio_get_value(bsi->ext_wake));
        ret = simple_read_from_buffer(buf, size, ppos, tmp_buf, buf_info - tmp_buf);
	return ret;
}

static int bluesleep_write_proc_btwrite(struct file *file, const char *buffer,
					size_t count, loff_t *data)
{
	char b;
 
	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;
	if (!test_bit(BT_PROTO, &flags)) {
		BT_SLP_DBG("BT_PROTO is not set,return direct");
		return 0;
	}
    BT_SLP_DBG("enter, value is %c", b);
	/* HCI_DEV_WRITE */
	if (b != '0') {
		bluesleep_outgoing_data();
	}

	return count;
}	
#else

/**
 * Handles HCI device events.
 * @param this Not used.
 * @param event The event that occurred.
 * @param data The HCI device associated with the event.
 * @return <code>NOTIFY_DONE</code>.
 */
static int bluesleep_hci_event(struct notifier_block *this,
				unsigned long event, void *data)
{
	struct hci_dev *hdev = (struct hci_dev *) data;
	struct hci_uart *hu;
	struct uart_state *state;

	if (!hdev)
		return NOTIFY_DONE;

	switch (event) {
	case HCI_DEV_REG:
		if (!bluesleep_hdev) {
			bluesleep_hdev = hdev;
			hu  = (struct hci_uart *) hdev->driver_data;
			state = (struct uart_state *) hu->tty->driver_data;
			bsi->uport = state->uart_port;
		}
		break;
	case HCI_DEV_UNREG:
		bluesleep_hdev = NULL;
		bsi->uport = NULL;
		break;
	case HCI_DEV_WRITE:
		bluesleep_outgoing_data();
		break;
	}

	return NOTIFY_DONE;
}
#endif

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	BT_SLP_DBG("Tx timer expired");

	/* were we silent during the last timeout? */
	if (!test_bit(BT_TXDATA, &flags)) {
		BT_SLP_DBG("Tx has been idle");
		gpio_set_value(bsi->ext_wake, !bsi->ext_wake_polarity);
		bluesleep_tx_idle();
	} else {
		BT_SLP_DBG("Tx data during last period");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
	}

	/* clear the incoming data flag */
	clear_bit(BT_TXDATA, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	bluesleep_sleep_wakeup();
	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
	int retval = 0;
	unsigned long irq_flags;

	BT_SLP_DBG("bluesleep_start");
	spin_lock_irqsave(&rw_lock, irq_flags);

	if (test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return 0;
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (bsi->get_uport){
		bsi->uport = bsi->get_uport();
	}else
	    BT_ERR("bluesleep:bsi->get_uport is NULL\n");

	if (!atomic_dec_and_test(&open_count)) {
		atomic_inc(&open_count);
		return -EBUSY;
	}

	/* start the timer */

	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));

	/* assert BT_WAKE */
    if(bsi->ext_wake){
        gpio_set_value(bsi->ext_wake, bsi->ext_wake_polarity);
    }
    if(bsi->host_wake_irq){
        if (bsi->host_wake_polarity)
            retval = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
                    IRQF_DISABLED | IRQF_TRIGGER_RISING,
                        "bluetooth hostwake", NULL);
        else
            retval = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
                        IRQF_DISABLED | IRQF_TRIGGER_FALLING,
                        "bluetooth hostwake", NULL);

        if (retval  < 0) {
            BT_ERR("Couldn't acquire BT_HOST_WAKE IRQ");
            goto fail;
        }

        retval = enable_irq_wake(bsi->host_wake_irq);
        if (retval < 0) {
            BT_ERR("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
            goto fail;
        }
        disable_irq_nosync(bsi->host_wake_irq);
    }
    set_bit(BT_PROTO, &flags);
    wake_lock(&bsi->wake_lock);
    return 0;
fail:
	del_timer(&tx_timer);
	atomic_inc(&open_count);

	return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
	unsigned long irq_flags;

	BT_SLP_DBG("bluesleep_stop");

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (!test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return;
	}

	/* assert BT_WAKE */
    if(bsi->ext_wake){
	    gpio_set_value(bsi->ext_wake, 0);
    }
	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
		hsuart_power(1);
	}

	atomic_inc(&open_count);

    if(bsi->host_wake_irq){
        if (disable_irq_wake(bsi->host_wake_irq))
		    BT_ERR("Couldn't disable hostwake IRQ wakeup mode");
        free_irq(bsi->host_wake_irq, NULL);
    }
	bsi->uport = NULL;

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}

/**
 * Read the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the
 * pin is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_btwake(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
        size_t ret;
        char tmp_buf[64] = {0};
        char *buf_info = tmp_buf;
        
        buf_info += sprintf(buf_info, "gpio %d, btwake:%u\n", bsi->ext_wake, gpio_get_value(bsi->ext_wake));
        ret = simple_read_from_buffer(buf, size, ppos, tmp_buf, buf_info - tmp_buf);
	return ret;
}

/**
 * Write the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluepower_write_proc_btwake(struct file *file, const char *buffer,
					size_t count, loff_t *data)
{
	char *buf;

	BT_SLP_DBG("bluepower_write_proc_btwake");

	if (count < 1)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}

	if (buf[0] == '0') {
        if (bsi->ext_wake){
		    gpio_set_value(bsi->ext_wake, 0);
	    }else{
	        BT_ERR("bluesleep: ext wake pin is not defined!");
	    }
	} else if (buf[0] == '1') {
        if (bsi->ext_wake){
		    gpio_set_value(bsi->ext_wake, 1);
	    }else{
	        BT_ERR("bluesleep: ext wake pin is not defined!");
	    }
	} else {
		kfree(buf);
		return -EINVAL;
	}

	kfree(buf);
	return count;
}

/**
 * Read the <code>BT_HOST_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the pin
 * is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_hostwake(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
        size_t ret = 0;
        char tmp_buf[64] = {0};
        char *buf_info = tmp_buf;
        if(bsi->host_wake){
            buf_info += sprintf(buf_info, "gpio:%u hostwake: %u\n",bsi->host_wake, gpio_get_value(bsi->host_wake));
            ret = simple_read_from_buffer(buf, size, ppos, tmp_buf, buf_info - tmp_buf);
        }else{
		    BT_ERR("bluesleep: host wake pin is not defined!");
		}
	return ret;
}


/**
 * Read the low-power status of the Host via the proc interface.
 * When this function returns, <code>page</code> contains a 1 if the Host
 * is asleep, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_asleep(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int asleep;
        size_t ret;
        char tmp_buf[64] = {0};
        char *buf_info = tmp_buf;

	asleep = test_bit(BT_ASLEEP, &flags) ? 1 : 0;
        buf_info += sprintf(buf_info, "asleep: %u\n", asleep);
        ret = simple_read_from_buffer(buf, size, ppos, tmp_buf, buf_info - tmp_buf);
	return ret;
}

/**
 * Read the low-power protocol being used by the Host via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the Host
 * is using the Sleep Mode Protocol, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_proto(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int proto;
        size_t ret;
        char tmp_buf[64] = {0};
        char *buf_info = tmp_buf;

	proto = test_bit(BT_PROTO, &flags) ? 1 : 0;
        buf_info += sprintf(buf_info, "proto: %u\n", proto);
        ret = simple_read_from_buffer(buf, size, ppos, tmp_buf, buf_info - tmp_buf);
	return ret;
}

/**
 * Modify the low-power protocol used by the Host via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluesleep_write_proc_proto(struct file *file, const char *buffer,
					size_t count, loff_t *data)
{
	char proto;

	BT_SLP_DBG("bluesleep_write_proc_proto");

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&proto, buffer, 1))
		return -EFAULT;

	if (proto == '0')
		bluesleep_stop();
	else
		bluesleep_start();

	/* claim that we wrote everything */
	return count;
}

static int __init bluesleep_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	BT_INFO("bluesleep_probe");

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	bsi->get_uport = pdev->dev.platform_data;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (!res) {
		BT_ERR("couldn't find host_wake gpio\n");
		ret = -ENODEV;
		goto free_bsi;
	}
	bsi->host_wake = res->start;

	ret = gpio_request(bsi->host_wake, "bt_host_wake");
	if (ret)
		goto free_bsi;
	ret = gpio_direction_input(bsi->host_wake);
	if (ret)
		goto free_bt_host_wake;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (!res) {
		BT_ERR("couldn't find ext_wake gpio\n");
		ret = -ENODEV;
		goto free_bt_host_wake;
	}
	bsi->ext_wake = res->start;

	ret = gpio_request(bsi->ext_wake, "bt_ext_wake");
	if (ret)
		goto free_bt_host_wake;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"ext_wake_polarity");
	if (!res) {
		BT_ERR("couldn't find ext_wake_polarity\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}
	bsi->ext_wake_polarity = res->start;

	/* assert bt wake */
	ret = gpio_direction_output(bsi->ext_wake, bsi->ext_wake_polarity);
	if (ret)
		goto free_bt_ext_wake;

	bsi->host_wake_irq = platform_get_irq_byname(pdev, "host_wake");
	if (bsi->host_wake_irq < 0) {
		BT_ERR("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}

	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"host_wake_polarity");
	if (!res) {
		BT_ERR("couldn't find host_wake_polarity\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}
	bsi->host_wake_polarity = res->start;
	
	return 0;

free_bt_ext_wake:
	gpio_free(bsi->ext_wake);
free_bt_host_wake:
	gpio_free(bsi->host_wake);
free_bsi:
	kfree(bsi);
	return ret;
}

static int __exit bluesleep_remove(struct platform_device *pdev)
{
	/* assert bt wake */
	if (bsi->ext_wake)
	    gpio_set_value(bsi->ext_wake, bsi->ext_wake_polarity);
	if (test_bit(BT_PROTO, &flags)) {
	    if (bsi->host_wake_irq){
		    if (disable_irq_wake(bsi->host_wake_irq))
			    BT_ERR("Couldn't disable hostwake IRQ wakeup mode \n");
		    free_irq(bsi->host_wake_irq, NULL);
		}
		del_timer(&tx_timer);
		if (test_bit(BT_ASLEEP, &flags))
			hsuart_power(1);
	}

    if (bsi->host_wake)
	    gpio_free(bsi->host_wake);
	if (bsi->ext_wake)
	    gpio_free(bsi->ext_wake);
	wake_lock_destroy(&bsi->wake_lock);
	kfree(bsi);
	return 0;
}

static struct platform_driver bluesleep_driver = {
	.remove = __exit_p(bluesleep_remove),
	.driver = {
		.name = "bluesleep",
		.owner = THIS_MODULE,
	},
};

static const struct file_operations file_ops_proc_btwake = {
	.write = bluepower_write_proc_btwake,
	.read = bluepower_read_proc_btwake,
};

static const struct file_operations file_ops_proc_btproto = {
	.read = bluesleep_read_proc_proto,
	.write = bluesleep_write_proc_proto,
};

static const struct file_operations file_ops_proc_btwrite = {
	.read = bluesleep_read_proc_btwrite,
	.write = bluesleep_write_proc_btwrite,
};

static const struct file_operations file_ops_proc_asleep = {
	.read = bluesleep_read_proc_asleep,
};

static const struct file_operations file_ops_proc_hostwake = {
	.read = bluepower_read_proc_hostwake,
};
/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	BT_INFO("Leadcore Sleep Mode Driver Ver %s", VERSION);

	retval = platform_driver_probe(&bluesleep_driver, bluesleep_probe);
	if (retval)
		return retval;

	if (bsi == NULL){
	    BT_ERR("bluesleep init failed,bsi is NULL");
		return 0;
	}

#if !BT_BLUEDROID_SUPPORT
	bluesleep_hdev = NULL;
#endif

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		BT_ERR("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		BT_ERR("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}

	/* Creating read/write "btwake" entry */
	ent = proc_create_data("btwake", 0, sleep_dir, &file_ops_proc_btwake, NULL);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/btwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read only proc entries */
	if (proc_create_data("hostwake", 0, sleep_dir,
				&file_ops_proc_hostwake, NULL) == NULL) {
		BT_ERR("Unable to create /proc/%s/hostwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = proc_create_data("proto", 0, sleep_dir, &file_ops_proc_btproto, NULL);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/proto entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read only proc entries */
	if (proc_create_data("asleep", 0,
			sleep_dir, &file_ops_proc_asleep, NULL) == NULL) {
		BT_ERR("Unable to create /proc/%s/asleep entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = proc_create_data("btwrite", 0, sleep_dir, &file_ops_proc_btwrite, NULL);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/btwrite entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = 0;

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

#if !BT_BLUEDROID_SUPPORT
	hci_register_notifier(&hci_event_nblock);
#endif

	return 0;

fail:
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
#if !BT_BLUEDROID_SUPPORT
	hci_unregister_notifier(&hci_event_nblock);
#endif
	platform_driver_unregister(&bluesleep_driver);

	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
