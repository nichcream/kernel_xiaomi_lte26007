
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/hwmon-sysfs.h>
#include <linux/uaccess.h>
#include <plat/comip-pmic.h>
#include <plat/comip-battery.h>
#include <plat/lc1132.h>
#include <plat/lc1132_adc.h>

#define ADC_WAIT_CONV_TIMES     (100)
/*
 * actual scaler gain is multiplied by 10 for fixed point operation
 * For channels 1, 2, 3, 4, 
 * 2.8 * 10 = 28
 * For channels 0, 
 * 2.8 * 10 * 4 = 112
 * is used, as scaler is Vref * divider
 * Vref = 2.8
*/
static const u16 lc1132_gain[ADC_MAX_CHANNELS] = {
    112,/* CHANNEL 0 */
    28,/* CHANNEL 1 */
    28,/* CHANNEL 2 */
    28,/* CHANNEL 3 */
    28,/* CHANNEL 4 */
};


struct lc1132_adc_data {
       struct device    *dev;
       struct mutex      lock;
       struct work_struct ws;
       struct lc1132_adc_request  requests[ADC_MAX_CHANNELS];
       int irq_n;
       unsigned long features;
};

static struct lc1132_adc_data *the_adc;

static const
struct lc1132_adc_conversion_method lc1132_conversion_methods_table[] = {
    [LC1132_ADC_SINGLE] = {
        .rbase = LC1132_ADCDATA0_REG,
        .ctrl = LC1132_ADCCTRL_REG,
        .enable = LC1132_ADCMODE_SINGLE,
        .mask = LC1132_ADC_INT_MASK,
    },

    [LC1132_ADC_CONTINUOUS] = {
        .rbase = LC1132_ADCDATA0_REG,
        .ctrl = LC1132_ADCCTRL_REG,
        .enable = LC1132_ADCMODE_CONTINUOUS,
        .mask = LC1132_ADC_INT_MASK,
	},
};

static const
struct lc1132_adc_conversion_method *lc1132_conversion_methods;

static int lc1132_adc_read(struct lc1132_adc_data *adc, u8 reg)
{
    int ret = 0;
    u8 val = 0;

    ret = lc1132_reg_read(reg,&val);

    if (ret) {
        dev_dbg(adc->dev, "unable to read register 0x%x\n", reg);
        return ret;
    }

	return val;
}

static int lc1132_adc_write(struct lc1132_adc_data *adc,
                    u8 reg, u8 val)
{
    int ret = 0;

    ret = lc1132_reg_write(reg, val);

    if (ret)
        dev_err(adc->dev, "unable to write register 0x%x\n", reg);

    return ret;
}


static int lc1132_adc_channel_raw_read(struct lc1132_adc_data *adc,
                                    u8 reg)
{
    u8 msb = 0, lsb = 0;

    /* For each ADC channel, we have MSB and LSB register pair.
    * MSB address is always LSB address+1. reg parameter is the
    * addr of LSB register
    */
    msb = lc1132_adc_read(adc, reg + 1);
    lsb = (lc1132_adc_read(adc, reg) &0xF);
    return (int)((msb << 4) | lsb);
}

static int lc1132_adc_read_channels(struct lc1132_adc_data *adc,
        u8 reg_base, u32 channels, struct lc1132_adc_request *req)
{
    int count = 0;
    u8 reg, i;
    s32 raw_code;
    s32 raw_channel_value;

    if(channels != (1 << req->adcsel))
        channels = (1 << req->adcsel);
    for (i = 0; i < ADC_MAX_CHANNELS; i++) {
        if (channels & BIT(i)){
            reg = reg_base;
            raw_code = lc1132_adc_channel_raw_read(adc, reg);
            req->buf[i].raw_code = raw_code;
            count++;
            /*
            * multiply by 1000 to convert the unit to milli
            * division by 4096 (>> 12) for 12 bit ADC
            * division by 10 for actual scaler gain
            * so the follow *100 = 1000/10
            */
            raw_channel_value = (raw_code * lc1132_gain[i]
                        * 100) >> 12;
            req->buf[i].raw_channel_value = raw_channel_value;

            req->buf[i].code = raw_code;
            req->rbuf[i] = raw_channel_value;
            
            dev_dbg(adc->dev, "ADC val: %d", req->rbuf[i]);
        }
    }
    return count;
}


static void lc1132_adc_enable_irq(u16 method)
{
    pmic_callback_unmask(PMIC_EVENT_ADC);
}

static void lc1132_adc_disable_irq(u16 method)
{
    pmic_callback_mask(PMIC_EVENT_ADC);
}


static void lc1132_adc_interrupt(int event, void *puser, void* pargs)
{
    //unsigned int int_status = *((unsigned int *)pargs);
    struct lc1132_adc_request *req = (struct lc1132_adc_request *)puser;

    /* Find the cause of the interrupt and enable the pending
        bit for the corresponding method */
    lc1132_adc_disable_irq(req->method);
    req->result_pending = 1;

    schedule_work(&the_adc->ws);
    return;
}

static void lc1132_adc_work(struct work_struct *ws)
{
    const struct lc1132_adc_conversion_method *method;
    struct lc1132_adc_data *adc;
    struct lc1132_adc_request *r;
    int len, i;

    adc = container_of(ws, struct lc1132_adc_data, ws);
    mutex_lock(&adc->lock);

    for (i = 0; i < LC1132_ADC_NUM_METHODS; i++) {
        r = &adc->requests[i];	
        if ((1 << r->adcsel)& BIT(i)){
            /* No pending results for this method, move to next one */
            if (!r->result_pending)
                continue;

            method = &lc1132_conversion_methods[r->method];

            /* Read results */
            len = lc1132_adc_read_channels(adc, method->rbase,
                        r->channels, r);

            /* Return results to caller */
            if (r->func_cb != NULL) {
                r->func_cb(r);
                r->func_cb = NULL;
            }

            /* Free request */
            r->result_pending = 0;
            r->active  = 0;
        }
    }

    mutex_unlock(&adc->lock);
}


static int lc1132_adc_set_irq(struct lc1132_adc_data *adc,
                struct lc1132_adc_request *req)
{
    struct lc1132_adc_request *p;

    p = &adc->requests[req->method];
    p->channels = req->channels;
    p->adcsel = req->adcsel;
    p->method = req->method;
    p->func_cb = req->func_cb;
    p->type = req->type;

    lc1132_adc_enable_irq(req->method);

    return 0;
}

static inline void
lc1132_adc_start_conversion(struct lc1132_adc_data *adc,
                    int conv_method,int adcsel)
{
    const struct lc1132_adc_conversion_method *method;

    method = &lc1132_conversion_methods[conv_method];

    lc1132_adc_write(adc,LC1132_ADCLDOEN_REG, LC1132_LDOADCEN);

    switch (conv_method) {
    	case LC1132_ADC_SINGLE:
            lc1132_adc_write(adc, method->ctrl, method->enable|adcsel);
            break;
        case LC1132_ADC_CONTINUOUS:
            lc1132_adc_write(adc, method->ctrl, method->enable|adcsel);
            break;
        default:
            break;
    }
    lc1132_adc_write(adc,LC1132_ADCEN_REG, LC1132_ADCEN);
    lc1132_adc_write(adc,LC1132_ADCCMD_REG, LC1132_ADCSTART);

}


static int lc1132_adc_is_conversion_ready(
        struct lc1132_adc_data *adc, u8 status_reg)
{
    u8 reg = lc1132_adc_read(adc,status_reg);

    return !!(reg & LC1132_ADCEND);
}


static int lc1132_adc_wait_conversion_ready(
        struct lc1132_adc_data *adc,
            unsigned int timeout_us, u8 status_reg)
{
    unsigned long timeout;

    timeout = jiffies + usecs_to_jiffies(timeout_us);
    do {
        if (lc1132_adc_is_conversion_ready(adc, status_reg))
            return 0;
        udelay(20);
    } while (!time_after(jiffies, timeout));

    /* one more checking against scheduler-caused timeout */
    if (lc1132_adc_is_conversion_ready(adc, status_reg))
        return 0;
    else
        return -EAGAIN;
}


/* locks held by caller */
static int _lc1132_adc_conversion(struct lc1132_adc_request *req,
            const struct lc1132_adc_conversion_method *method)
{
    int ret = 0;

    if ((req->type == LC1132_ADC_IRQ_ONESHOT) &&
        (req->func_cb != NULL)) {
        lc1132_adc_set_irq(the_adc, req);
        lc1132_adc_start_conversion(the_adc, req->method,req->adcsel);
        the_adc->requests[req->method].active = 1;
        ret = 0;
        goto out;
    }

    lc1132_adc_start_conversion(the_adc, req->method, req->adcsel);
    the_adc->requests[req->method].active = 1;

    /* Wait until conversion is ready (cmd register returns EOC) */
    ret = lc1132_adc_wait_conversion_ready(the_adc, ADC_WAIT_CONV_TIMES, LC1132_ADCCMD_REG);
    if (ret) {
        dev_err(the_adc->dev, "conversion timeout!\n");
        the_adc->requests[req->method].active = 0;
        goto out;
    }

    ret = lc1132_adc_read_channels(the_adc, method->rbase,
                                    req->channels, req);
    the_adc->requests[req->method].active = 0;
out:
    lc1132_adc_write(the_adc,LC1132_ADCEN_REG, LC1132_LDOADCDIS);
    return ret;
}

/*
 * Return channel value
 * Or < 0 on failure.
 */


int lc1132_adc_conversion(struct lc1132_adc_request *req)
{
    const struct lc1132_adc_conversion_method *method;
    int ret = 0;

    if (unlikely(!req))
        return -EINVAL;

    if (!the_adc)
        return -EAGAIN;

    mutex_lock(&the_adc->lock);

    if (req->method >= LC1132_ADC_NUM_METHODS) {
        dev_err(the_adc->dev, "unsupported conversion method\n");
        ret = -EINVAL;
        goto out;
    }

    /* Do we have a conversion request ongoing */
    if (the_adc->requests[req->method].active) {
        dev_err(the_adc->dev, "a conversion request ongoing\n");
        ret = -EBUSY;
        goto out;
    }

    method = &lc1132_conversion_methods[req->method];

    ret = _lc1132_adc_conversion(req, method);

out:
    mutex_unlock(&the_adc->lock);

    return ret;
}
EXPORT_SYMBOL(lc1132_adc_conversion);

int pmic_get_adc_conversion(int channel_no)
{
    struct lc1132_adc_request req;
    int temp = 0;
    int ret;

    req.channels = (1 << channel_no);
    req.adcsel = channel_no;
    req.method = LC1132_ADC_SINGLE;
    req.active = 0;
    req.func_cb = NULL;
    ret = lc1132_adc_conversion(&req);
    if (ret < 0)
        return ret;

    if (req.rbuf[channel_no] > 0)
        temp = req.rbuf[channel_no];

    return temp;
}
EXPORT_SYMBOL(pmic_get_adc_conversion);

/*sysfs*/
static ssize_t show_channel(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
    struct lc1132_adc_request req;
    int temp = 0;
    int ret;

    req.channels =(1 <<  attr->index);
    req.adcsel =  attr->index;
    req.method = LC1132_ADC_SINGLE;
    req.active = 0;
    req.func_cb = NULL;
    ret = lc1132_adc_conversion(&req);
    if (ret < 0)
        return ret;

    if (req.rbuf[attr->index] > 0)
        temp = req.rbuf[attr->index];

    ret = sprintf(buf, "%d\n", temp);

    return ret;
}

static ssize_t show_raw_code(struct device *dev,
            struct device_attribute *devattr, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
    struct lc1132_adc_request req;
    int temp = 0;
    int ret;

    req.channels =(1 <<  attr->index);
    req.adcsel=  attr->index;
    req.method = LC1132_ADC_SINGLE;
    req.active = 0;
    req.func_cb = NULL;
    ret = lc1132_adc_conversion(&req);
    if (ret < 0)
        return ret;

    if (req.buf[attr->index].raw_channel_value > 0)
        temp = req.buf[attr->index].raw_code;

    ret = sprintf(buf, "%d\n", temp);

    return ret;
}

#define in_channel(index) \
static SENSOR_DEVICE_ATTR(in##index##_channel, S_IRUGO, show_channel, \
    NULL, index); \
static SENSOR_DEVICE_ATTR(in##index##_raw_code, S_IRUGO, show_raw_code, \
    NULL, index)

in_channel(0);
in_channel(1);
in_channel(2);
in_channel(3);
in_channel(4);

#define IN_ATTRS_CHANNEL(X)\
    &sensor_dev_attr_in##X##_channel.dev_attr.attr,  \
    &sensor_dev_attr_in##X##_raw_code.dev_attr.attr  \

static struct attribute *lc1132_adc_attributes[] = {
    IN_ATTRS_CHANNEL(0),
    IN_ATTRS_CHANNEL(1),
    IN_ATTRS_CHANNEL(2),
    IN_ATTRS_CHANNEL(3),
    IN_ATTRS_CHANNEL(4),
    NULL
};

static const struct attribute_group lc1132_adc_group = {
    .attrs = lc1132_adc_attributes,
};

static long lc1132_adc_ioctl(struct file *filp, unsigned int cmd,
                    unsigned long arg)
{
    struct lc1132_adc_user_parms par;
    int val, ret;

    ret = copy_from_user(&par, (void __user *) arg, sizeof(par));
    if (ret) {
        dev_dbg(the_adc->dev, "copy_from_user: %d\n", ret);
        return -EACCES;
    }

    switch (cmd) {
    case LC1132_ADC_IOCX_ADC_READ:
    case LC1132_ADC_IOCX_ADC_RAW_READ: {
        struct lc1132_adc_request req;
        int max_channels = ADC_MAX_CHANNELS;

        if (par.channel >= max_channels)
            return -EINVAL;

        req.channels = 1 << par.channel;
        req.adcsel =  par.channel;
        req.method	= LC1132_ADC_SINGLE;
        req.func_cb	= NULL;
        req.type	= LC1132_ADC_WAIT;

        val = lc1132_adc_conversion(&req);
        if (likely(val > 0)) {
            par.status = 0;
            if (cmd == LC1132_ADC_IOCX_ADC_READ)
                par.result = (u16)req.rbuf[par.channel];
            else
                par.result = (u16)req.buf[par.channel].raw_code;

        } else if (val == 0) {
            par.status = -ENODATA;
        } else {
            par.status = val;
        }
        break;
    }
    default:
        return -EINVAL;
    }

    ret = copy_to_user((void __user *) arg, &par, sizeof(par));
    if (ret) {
        dev_dbg(the_adc->dev, "copy_to_user: %d\n", ret);
        return -EACCES;
    }

    return 0;
}

static const struct file_operations lc1132_adc_fileops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = lc1132_adc_ioctl,
};

static struct miscdevice lc1132_adc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "lc1132-adc",
    .fops = &lc1132_adc_fileops,
};


static int __init lc1132_adc_probe(struct platform_device *pdev)
{
    struct lc1132_adc_data *adc;
    struct pmic_adc_platform_data *pdata = pdev->dev.platform_data;
    int ret = 0;

    adc = kzalloc(sizeof *adc, GFP_KERNEL);
    if (!adc)
        return -ENOMEM;

    if (!pdata) {
        dev_err(&pdev->dev, "adc platform_data not available\n");
        ret = -EINVAL;
        goto err_pdata;
    }

    adc->dev = &pdev->dev;
    adc->features = pdata->features;

    lc1132_conversion_methods = lc1132_conversion_methods_table;

    ret = misc_register(&lc1132_adc_device);
    if (ret) {
        dev_dbg(&pdev->dev, "could not register misc_device\n");
        goto err_misc;
    }

    ret = pmic_callback_register(PMIC_EVENT_ADC, adc, lc1132_adc_interrupt);
    if (ret) {
        dev_err(&pdev->dev, "failed to get irq\n");
        goto err_irq;
    }

    platform_set_drvdata(pdev, adc);
    mutex_init(&adc->lock);
    INIT_WORK(&adc->ws, lc1132_adc_work);

    ret = lc1132_adc_write(adc,LC1132_ADCLDOEN_REG, LC1132_LDOADCEN);
    if (ret)
        pr_err("%s: Error reseting LC1132_ADCLDOEN_REG (%d)!\n", __func__, ret);

    the_adc = adc;

    ret = sysfs_create_group(&pdev->dev.kobj, &lc1132_adc_group);
    if (ret)
        dev_err(&pdev->dev, "could not create sysfs files\n");

    dev_info(&pdev->dev, "%s success\n",__func__);
    return 0;

err_irq:
    misc_deregister(&lc1132_adc_device);

err_misc:
err_pdata:
    kfree(adc);

    return ret;
}

static int __exit lc1132_adc_remove(struct platform_device *pdev)
{
    struct lc1132_adc_data *adc = platform_get_drvdata(pdev);

    lc1132_adc_disable_irq(LC1132_ADC_SINGLE);
    pmic_callback_unregister(PMIC_EVENT_ADC);
    sysfs_remove_group(&pdev->dev.kobj, &lc1132_adc_group);
    cancel_work_sync(&adc->ws);
    misc_deregister(&lc1132_adc_device);

    return 0;
}
#ifdef CONFIG_PM
static int lc1132_adc_suspend(struct device *dev)
{
    int ret = 0;
    struct platform_device *pdev = to_platform_device(dev);
    struct lc1132_adc_data *adc = platform_get_drvdata(pdev);

    ret = lc1132_adc_write(adc,LC1132_ADCEN_REG, LC1132_ADCDIS);
    ret|= lc1132_adc_write(adc,LC1132_ADCLDOEN_REG, LC1132_LDOADCDIS);
    if (ret)
        pr_err("%s: Error reseting LC1132_ADCEN_REG (%d)!\n", __func__, ret);

    return 0;
}

static int lc1132_adc_resume(struct device *dev)
{
    int ret = 0;
    struct platform_device *pdev = to_platform_device(dev);
    struct lc1132_adc_data *adc = platform_get_drvdata(pdev);

    ret = lc1132_adc_write(adc,LC1132_ADCLDOEN_REG, LC1132_LDOADCEN);
    ret|= lc1132_adc_write(adc,LC1132_ADCEN_REG, LC1132_ADCEN);
    if (ret)
        pr_err("%s: Error setting LC1132_ADCEN_REG (%d)!\n", __func__, ret);

    return 0;
}

#else

#define lc1132_adc_suspend	NULL
#define lc1132_adc_resume	NULL

#endif /* CONFIG_PM */

static const struct dev_pm_ops lc1132_adc_pm_ops = {
    .suspend = lc1132_adc_suspend,
    .resume = lc1132_adc_resume,
};

static struct platform_driver lc1132_adc_driver = {
    .probe        = lc1132_adc_probe,
    .remove        = __exit_p(lc1132_adc_remove),
    .driver        = {
        .name    = "lc11xx_adc",
        .owner    = THIS_MODULE,
        .pm = &lc1132_adc_pm_ops,
    },
};

static int __init lc1132_adc_init(void)
{
    return platform_driver_register(&lc1132_adc_driver);
}
subsys_initcall_sync(lc1132_adc_init);

static void __exit lc1132_adc_exit(void)
{
    platform_driver_unregister(&lc1132_adc_driver);
}
module_exit(lc1132_adc_exit);

MODULE_ALIAS("platform:lc1132-adc");
MODULE_AUTHOR("leadcore tech.");
MODULE_DESCRIPTION("lc1132 ADC driver");
MODULE_LICENSE("GPL");
