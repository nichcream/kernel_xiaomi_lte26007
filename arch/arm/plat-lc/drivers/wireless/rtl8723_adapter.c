#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/wlan_plat.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <plat/comip_devinfo.h>

static int g_wifidev_registered = 0;
static struct semaphore wifi_control_sem_fake;
static struct wifi_platform_data *wifi_control_data_fake = NULL;
static struct resource *wifi_irqres_fake = NULL;
static struct resource *wifi_BT_RESET_pin = NULL;
static struct resource wifi_irqres_resource;
static int check_probe_times = 0;

#define ADDR_MASK 0x10000
#define LOCAL_ADDR_MASK 0x00000
static int wifi_add_dev(void);
static void wifi_del_dev(void);

static ssize_t wifi_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "WLAN:realtek-8723bs\n");
}
static DEVICE_ATTR(wifi_info, S_IRUGO | S_IWUSR, wifi_info_show, NULL);
static struct attribute *wifi_info_attributes[] = {
	&dev_attr_wifi_info.attr,
};


static u8 wifi_readb(struct sdio_func *func, u32 addr)
{
	int err;
	u8 ret = 0;

	ret = sdio_readb(func, ADDR_MASK | addr, &err);

	return ret;
}

static void wifi_writeb(struct sdio_func *func, u8 val, u32 addr)
{
	int err;

	sdio_writeb(func, val, ADDR_MASK | addr, &err);
}

static u8 wifi_readb_local(struct sdio_func *func, u32 addr)
{
	int err;
	u8 ret = 0;

	ret = sdio_readb(func, LOCAL_ADDR_MASK | addr, &err);

	return ret;
}

static void wifi_writeb_local(struct sdio_func *func, u8 val, u32 addr)
{
	int err;

	sdio_writeb(func, val, LOCAL_ADDR_MASK | addr, &err);
}

const struct resource * rtk_get_resource(void)
{
	return &wifi_irqres_resource;
}
EXPORT_SYMBOL_GPL(rtk_get_resource);

const struct resource * rtk_get_BT_RESET_pin(void)
{
	return wifi_BT_RESET_pin;
}
EXPORT_SYMBOL_GPL(rtk_get_BT_RESET_pin);


static int rtw_fake_driver_probe(
		struct sdio_func *func,
		const struct sdio_device_id *id)
{
	u8 value8 = 0;
	
	printk("RTL871X(adapter): %s enter\n", __func__);

	comip_devinfo_register(wifi_info_attributes, ARRAY_SIZE(wifi_info_attributes));

	sdio_claim_host(func);

	/* unlock ISO/CLK/Power control register */
	wifi_writeb(func, 0x1C, 0x00);

	/* we should leave suspend first, or all MAC io will fail */
	wifi_writeb_local(func, wifi_readb_local(func, 0x86) & (~(BIT(0))), 0x86);
	msleep(20);

	wifi_writeb(func, 0, 0x93);
	//wifi_writeb(func, 0x1b, 0x23);
	//wifi_writeb(func, 0x20, 0x07);
	//0x68[13]=1 set clk req high
	value8 = wifi_readb(func, 0x69);
	wifi_writeb(func, value8 | BIT(5), 0x69);

	//set clk 26M
	value8 = wifi_readb(func, 0x28);
	wifi_writeb(func, value8 | BIT(2), 0x28);

	//0x66[7]=1 set clk ext enable
	value8 = wifi_readb(func, 0x66);
	wifi_writeb(func, value8 | BIT(7), 0x66);

	value8 = wifi_readb(func, 0x78);
	wifi_writeb(func, (value8 & 0x7F), 0x78);

	value8 = wifi_readb(func, 0x79);
	wifi_writeb(func, (value8 & 0xFC) | BIT(1), 0x79);

	//0x64[12]=1 host wake pad pull enable (power leakage)
	value8 = wifi_readb(func, 0x65);
	wifi_writeb(func, value8 | BIT(4), 0x65);
	
	//0x04[7]=1 chip power down en  (power leakage)
	value8 = wifi_readb(func, 0x04);
	wifi_writeb(func, value8 | BIT(7), 0x04);

	//0x66[2:0]=0x07 PCM interface en (for SCO)
	value8 = wifi_readb(func, 0x66);
	wifi_writeb(func, value8 | BIT(0) | BIT(1) | BIT(2), 0x66);

	//Ant select BT_RFE_CTRL control enable
	//0x38[11]=1 0x4c[23-24]=0
	value8 = wifi_readb(func, 0x39);
	wifi_writeb(func, value8 | BIT(3), 0x39);
	value8 = wifi_readb(func, 0x4E);//0x4c bit23
	wifi_writeb(func, value8 & ~BIT(7), 0x4E);
	value8 = wifi_readb(func, 0x4F);//0x4c bit24
	wifi_writeb(func, value8 & ~BIT(0), 0x4F);

	//Fix 0x2c 4 Bytes when internal clk and Handle the clk control to WIFI always
	value8 = wifi_readb(func, 0x2D);
	wifi_writeb(func, value8 & 0x0F, 0x2D);
	wifi_writeb(func, 0x82, 0x2E);
	value8 = wifi_readb(func,0x10);
	wifi_writeb(func, value8 | BIT(6), 0x10);

	//cardemue to suspend
	wifi_writeb(func, wifi_readb(func, 0x05) | (BIT(3)), 0x05);
	wifi_writeb(func, wifi_readb(func, 0x23) | (BIT(4)), 0x23);
	wifi_writeb(func, 0x20, 0x07);
	wifi_writeb_local(func, wifi_readb_local(func, 0x86) | (BIT(0)), 0x86);
	msleep(20);

	printk("RTL871X(adapter): write register ok:"
		"0x39 is 0x%x, 0x23 is 0x%x, 0x07 is 0x%x \n",
	 	wifi_readb(func, 0x39), wifi_readb(func, 0x23),
	 	wifi_readb(func, 0x07));

	printk("RTL871X(adpter): 0x39 is %x \n",wifi_readb(func,0x39));


	sdio_release_host(func);

	check_probe_times = 1;

	return 0;
}

static void rtw_fake_driver_remove(struct sdio_func *func)
{
	check_probe_times = 0;
	printk("RTL871X(adapter): %s \n", __func__);
}

static const struct sdio_device_id sdio_ids_fake_driver[] = {
		{ SDIO_DEVICE(0x024c, 0x8723) },
		{ SDIO_DEVICE(0x024c, 0xb723) },
};

static struct sdio_driver r871xs_fake_driver = {
	.probe = rtw_fake_driver_probe,
	.remove = rtw_fake_driver_remove,
	.name = "rtw_fake_driver",
	.id_table = sdio_ids_fake_driver,
};

int rtw_android_wifictrl_func_add(void)
{
	int ret = 0;
	sema_init(&wifi_control_sem_fake, 0);

	ret = wifi_add_dev();
	if (ret) {
		printk("%s: platform_driver_register failed\n", __FUNCTION__);
		return ret;
	}
	g_wifidev_registered = 1;

	return ret;
}

void rtw_android_wifictrl_func_del(void)
{
	if (g_wifidev_registered)
	{
		wifi_del_dev();
		g_wifidev_registered = 0;
	}
}


int wifi_set_power(int on, unsigned long msec)
{
	printk("%s = %d\n", __FUNCTION__, on);
	if (wifi_control_data_fake && wifi_control_data_fake->set_power) {
		wifi_control_data_fake->set_power(on);
	}
	if (msec)
		msleep(msec);
	return 0;
}
int wifi_set_powersave(int on, unsigned long msec)
{
	printk("%s = %d\n", __FUNCTION__, on);
	if (wifi_control_data_fake && wifi_control_data_fake->set_power) {
		wifi_control_data_fake->set_power(on+2);
	}
	if (msec)
		msleep(msec);
	return 0;
}
EXPORT_SYMBOL(wifi_set_powersave);

static int wifi_set_carddetect(int on)
{
	printk("%s = %d\n", __FUNCTION__, on);
	if (wifi_control_data_fake && wifi_control_data_fake->set_carddetect) {
		wifi_control_data_fake->set_carddetect(on);
	}
	return 0;
}

static int wifi_probe(struct platform_device *pdev)
{
	struct wifi_platform_data *wifi_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);

	printk("%s\n", __FUNCTION__);
	wifi_irqres_fake = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "rtk_wlan_irq");
	if (wifi_irqres_fake == NULL)
		wifi_irqres_fake = platform_get_resource_byname(pdev,
			IORESOURCE_IRQ, "rtk_wlan_irq");

	wifi_BT_RESET_pin = platform_get_resource_byname(pdev,IORESOURCE_IO,"wifi_BT_reset_pin");
	if (wifi_BT_RESET_pin == NULL)
			wifi_BT_RESET_pin = platform_get_resource_byname(pdev,IORESOURCE_IO,"wifi_BT_reset_pin");

	wifi_control_data_fake = wifi_ctrl;

	wifi_irqres_resource = *wifi_irqres_fake;

	wifi_set_power(1, 0);	/* Power On */
	wifi_set_carddetect(1); /* CardDetect (0->1) */

	up(&wifi_control_sem_fake);
	return 0;
}

static int wifi_remove(struct platform_device *pdev)
{
	struct wifi_platform_data *wifi_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);

	printk("%s\n", __FUNCTION__);
	wifi_control_data_fake = wifi_ctrl;

	wifi_set_carddetect(0); /* CardDetect (1->0) */

	printk("after wifi_remove before wifi_set_power \n");

	wifi_set_power(0, 0);	/* Power Off */

	up(&wifi_control_sem_fake);
	return 0;
}

/* temporarily use these two */
static struct platform_driver wifi_device = {
	.probe			= wifi_probe,
	.remove 		= wifi_remove,
	.driver 		= {
	.name	= "rtk_wlan",
	}
};


static int __init wlan_bt_late_init(void)
{
	s32 err;
	int i;

	printk("wlan_bt_late_init\n");

	if (rtw_android_wifictrl_func_add() < 0)
	{
		printk("wlan_bt_late_init: register error \n");
		msleep(5);
	}


	err = sdio_register_driver(&r871xs_fake_driver);
	if (err)
	{
		pr_err("RTL871X(adapter): register r871xs_fake_driver failed\n");
		return -1;
	}
	for (i = 0; i <= 20; i++) {
		msleep(10);
		if (check_probe_times)
			break;
	}

	printk("RTL871X(adapter): %s sdio_unregister_driver for r871xs_fake_driver %d\n", __func__, i);
	sdio_unregister_driver(&r871xs_fake_driver);
	printk("RTL871X(adapter): %s sdio_unregister_driver ok\n", __func__);

	return 0;
}


static int wifi_add_dev(void)
{
	printk("Calling platform_driver_register\n");
	platform_driver_register(&wifi_device);
	//platform_driver_register(&wifi_device_legacy);
	return 0;
}

static void wifi_del_dev(void)
{
	printk("Unregister platform_driver_register\n");
	platform_driver_unregister(&wifi_device);
	//platform_driver_unregister(&wifi_device_legacy);
}

late_initcall(wlan_bt_late_init);


MODULE_DESCRIPTION("Realtek wlan/bt init driver");
MODULE_LICENSE("GPL");

