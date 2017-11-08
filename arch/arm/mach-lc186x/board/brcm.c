#if defined(CONFIG_BRCM_BLUETOOTH)
#include <plat/brcm-bluetooth.h>
#include <plat/mfp.h>

/* BT GPIO. */
static int brcm_bt_onoff = mfp_to_gpio(BRCM_BT_ONOFF_PIN);
static int brcm_bt_reset = mfp_to_gpio(BRCM_BT_RESET_PIN);
static int brcm_bt_wake_i = mfp_to_gpio(BRCM_BT_WAKE_I_PIN);
static int brcm_bt_wake_o = mfp_to_gpio(BRCM_BT_WAKE_O_PIN);

static struct mfp_pull_cfg comip_PCM1_mfp_pull_enable_cfg[] = {
       /* PCM1. */
       {MFP_PIN_GPIO(190),       MFP_PULL_UP},
       {MFP_PIN_GPIO(189),       MFP_PULL_DOWN},
       {MFP_PIN_GPIO(188),       MFP_PULL_DOWN},
       {MFP_PIN_GPIO(187),       MFP_PULL_DOWN},
};

static struct mfp_pull_cfg comip_PCM1_mfp_pull_disable_cfg[] = {
       /* PCM1. */
       {MFP_PIN_GPIO(190),       MFP_PULL_DISABLE},
       {MFP_PIN_GPIO(189),       MFP_PULL_DISABLE},
       {MFP_PIN_GPIO(188),       MFP_PULL_DISABLE},
       {MFP_PIN_GPIO(187),       MFP_PULL_DISABLE},
};

static int __init brcm_bt_init(void)
{
	comip_mfp_config(BRCM_BT_RESET_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(BRCM_BT_WAKE_I_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(BRCM_BT_WAKE_O_PIN, MFP_PIN_MODE_GPIO);

	/* Power on BT. */
	if(BRCM_BT_ONOFF_PIN != MFP_PIN_GPIO(MFP_PIN_MAX)){
		comip_mfp_config(BRCM_BT_ONOFF_PIN, MFP_PIN_MODE_GPIO);
		gpio_request(brcm_bt_onoff, "BT Poweron");
		gpio_direction_output(brcm_bt_onoff, 0);
		gpio_free(brcm_bt_onoff);
	}

	/* Reset Bluetooth chip. */
	gpio_request(brcm_bt_reset, "Bluetooth Reset");
	gpio_direction_output(brcm_bt_reset, 0);
	gpio_free(brcm_bt_reset);

	gpio_request(brcm_bt_wake_o, "Bluetooth AP2BT Wake");
	gpio_direction_output(brcm_bt_wake_o, 0);
	gpio_free(brcm_bt_wake_o);

	gpio_request(brcm_bt_wake_i, "Bluetooth BT2AP Wake");
	gpio_direction_input(brcm_bt_wake_i);
	gpio_free(brcm_bt_wake_i);

	return 0;
}
rootfs_initcall(brcm_bt_init);

static int brcm_bt_power_on(void)
{
	printk(KERN_DEBUG "brcm bt power on\n");

	comip_mfp_config_pull_array(comip_PCM1_mfp_pull_disable_cfg,
		ARRAY_SIZE(comip_PCM1_mfp_pull_disable_cfg));

	gpio_request(brcm_bt_reset, "Bluetooth Reset");
	gpio_request(brcm_bt_wake_o, "Bluetooth AP2BT Wake");
	gpio_request(brcm_bt_wake_i, "Bluetooth BT2AP Wake");

	/* Power on. */
	if(BRCM_BT_ONOFF_PIN != MFP_PIN_GPIO(MFP_PIN_MAX)){
		gpio_request(brcm_bt_onoff, "BT Poweron");
		gpio_direction_output(brcm_bt_onoff, 1);
		gpio_free(brcm_bt_onoff);
	}

	/* AP wakeup bluetooth chip. */
	gpio_direction_output(brcm_bt_wake_o, 0);

	/* Bluetooth wakeup AP */
	gpio_direction_input(brcm_bt_wake_i);

	/* Reset bluetooth and enable bluetooth firmware run. */
	gpio_direction_output(brcm_bt_reset, 0);
	msleep(100);
	gpio_direction_output(brcm_bt_reset, 1);
	msleep(400);

	gpio_free(brcm_bt_reset);
	gpio_free(brcm_bt_wake_o);
	gpio_free(brcm_bt_wake_i);

	return 0;
}

static int brcm_bt_power_off(void)
{
	printk(KERN_DEBUG "brcm bt power off\n");

	comip_mfp_config_pull_array(comip_PCM1_mfp_pull_enable_cfg,
		ARRAY_SIZE(comip_PCM1_mfp_pull_enable_cfg));

	/* Reset bluetooth. */
	gpio_request(brcm_bt_reset, "Bluetooth Reset");
	gpio_direction_output(brcm_bt_reset, 0);
	gpio_free(brcm_bt_reset);

	/* Power off. */
	if(BRCM_BT_ONOFF_PIN != MFP_PIN_GPIO(MFP_PIN_MAX)){
		gpio_request(brcm_bt_onoff, "BT Poweron");
		gpio_direction_output(brcm_bt_onoff, 0);
		gpio_free(brcm_bt_onoff);
	}

	gpio_request(brcm_bt_wake_o, "Bluetooth AP2BT Wake");
	gpio_direction_input(brcm_bt_wake_o);
	gpio_free(brcm_bt_wake_o);

	msleep(100);

	return 0;
}

static struct uart_port *brcm_bt_uart = NULL;

static int brcm_bt_uart_register(void* host, int on)
{
	if (on)
		brcm_bt_uart = (struct uart_port *)host;
	else
		brcm_bt_uart = NULL;
	return 0;
}

static struct uart_port* brcm_bt_get_uart(void)
{
	return brcm_bt_uart;
}

static struct brcm_bt_platform_data brcm_bt_data = {
	.name = "brcm-bluetooth",
	.gpio_wake_i = mfp_to_gpio(BRCM_BT_WAKE_I_PIN),
	.gpio_wake_o = mfp_to_gpio(BRCM_BT_WAKE_O_PIN),
	.poweron = brcm_bt_power_on,
	.poweroff = brcm_bt_power_off,
};

static struct platform_device brcm_bt_device = {
	.name = "brcm-bluetooth",
	.dev = {
		.platform_data = &brcm_bt_data,
	}
};

static struct resource brcm_bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= mfp_to_gpio(BRCM_BT_WAKE_I_PIN),
		.end	= mfp_to_gpio(BRCM_BT_WAKE_I_PIN),
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= mfp_to_gpio(BRCM_BT_WAKE_O_PIN),
		.end	= mfp_to_gpio(BRCM_BT_WAKE_O_PIN),
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= gpio_to_irq(mfp_to_gpio(BRCM_BT_WAKE_I_PIN)),
		.end	= gpio_to_irq(mfp_to_gpio(BRCM_BT_WAKE_I_PIN)),
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "ext_wake_polarity",
		.start	= 1,
		.end	= 1,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake_polarity",
		.start	= 1,
		.end	= 1,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device brcm_bluesleep_device = {
	.name	= "bluesleep",
	.id	= -1,
	.num_resources = ARRAY_SIZE(brcm_bluesleep_resources),
	.resource = brcm_bluesleep_resources,
	.dev = {
		.platform_data = brcm_bt_get_uart,
	}
};

#endif

#if defined(CONFIG_BRCM_WIFI)
#include <plat/brcm-wifi.h>
#include <linux/wlan_plat.h>
#include <plat/comip_devinfo.h>

static struct mmc_host *brcm_wifi_sdio;
static void *brcm_wifi_param;
static void (*brcm_wifi_on_detect)(void *param, int on);

/* Wlan GPIO. */
static int brcm_wifi_onoff = mfp_to_gpio(BRCM_WIFI_ONOFF_PIN);
static int brcm_wifi_wake_i = mfp_to_gpio(BRCM_WIFI_WAKE_I_PIN);
static int brcm_lte2wl_TX = mfp_to_gpio(MFP_PIN_GPIO(14));
static int brcm_lte2wl_RX = mfp_to_gpio(MFP_PIN_GPIO(15));


static ssize_t wifi_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "WLAN:broadcom-4343s\n");
}
static DEVICE_ATTR(wifi_info, S_IRUGO | S_IWUSR, wifi_info_show, NULL);
static struct attribute *wifi_info_attributes[] = {
	&dev_attr_wifi_info.attr,
};

static int __init brcm_wifi_init(void)
{
	comip_mfp_config(BRCM_WIFI_ONOFF_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(BRCM_WIFI_WAKE_I_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(MFP_PIN_GPIO(14), MFP_PIN_MODE_GPIO);
	comip_mfp_config(MFP_PIN_GPIO(15), MFP_PIN_MODE_GPIO);

	printk(KERN_DEBUG "enter %s \n", __func__);

	comip_devinfo_register(wifi_info_attributes, ARRAY_SIZE(wifi_info_attributes));

	/* Power on WLAN. */
	gpio_request(brcm_wifi_onoff, "Wlan Poweron");
	gpio_direction_output(brcm_wifi_onoff, 0);
	gpio_free(brcm_wifi_onoff);

	gpio_request(brcm_wifi_wake_i, "Wlan WLAN2AP Wake");
	gpio_direction_input(brcm_wifi_wake_i);
	gpio_free(brcm_wifi_wake_i);

	gpio_request(brcm_lte2wl_TX, "Wlan LTE coexsit 1");
	gpio_direction_input(brcm_lte2wl_TX);
	gpio_free(brcm_lte2wl_TX);

	gpio_request(brcm_lte2wl_RX, "Wlan LTE coexsit 2");
	gpio_direction_input(brcm_lte2wl_RX);
	gpio_free(brcm_lte2wl_RX);
	bcm_init_wifi_mem();

	printk(KERN_DEBUG "set mmc2 sdio driver current to 2ma\n");
	comip_mfp_config_ds(BRCM_WIFI_SDIO_CLK_PIN, MFP_DS_2MA);
	comip_mfp_config_ds(BRCM_WIFI_SDIO_CMD_PIN, MFP_DS_2MA);
	comip_mfp_config_ds(BRCM_WIFI_SDIO_DATA3_PIN, MFP_DS_2MA);
	comip_mfp_config_ds(BRCM_WIFI_SDIO_DATA2_PIN, MFP_DS_2MA);
	comip_mfp_config_ds(BRCM_WIFI_SDIO_DATA1_PIN, MFP_DS_2MA);
	comip_mfp_config_ds(BRCM_WIFI_SDIO_DATA0_PIN, MFP_DS_2MA);

	return 0;
}
rootfs_initcall(brcm_wifi_init);

static int brcm_wifi_sdio_register(void (*on_detect)(void *param, int on), void *param, void* sdio)
{
	brcm_wifi_param = param;
	brcm_wifi_sdio = (struct mmc_host *)sdio;
	brcm_wifi_on_detect = on_detect;
	return 0;
}

static int brcm_wifi_restore_host(void)
{
	struct mmc_host *host = brcm_wifi_sdio;
	int ret = 0;

	if (host)
		ret = mmc_power_restore_host(host);

	return ret;
}

static int brcm_wifi_save_host(void)
{
	struct mmc_host *host = brcm_wifi_sdio;
	int ret = 0;

	if (host)
		ret = mmc_power_save_host(host);

	return ret;
}

static int brcm_wifi_power(int on)
{
	printk(KERN_DEBUG "brcm wlan power: %d \n", on);

	gpio_request(brcm_wifi_onoff, "Wlan Poweron");

	if(on) {
		/* Power on. */
		gpio_direction_output(brcm_wifi_onoff, 0);
		gpio_set_value(brcm_wifi_onoff,0);
		mdelay(10);
		gpio_set_value(brcm_wifi_onoff,1);
		mdelay(150);
		brcm_wifi_restore_host();
		//mdelay(50);
	} else {
		/* Power off. */
		gpio_direction_output(brcm_wifi_onoff, 0);
		gpio_set_value(brcm_wifi_onoff,0);
		mdelay(10);
		brcm_wifi_save_host();
	}

	gpio_free(brcm_wifi_onoff);

	return 0;
}

static int brcm_wifi_reset(int on)
{
	printk(KERN_DEBUG "brcm wlan reset: %d\n", on);
	brcm_wifi_power(on);
	return 0;
}

static int brcm_wifi_set_carddetect(int on)
{
	if (brcm_wifi_on_detect && on) {
		brcm_wifi_on_detect(brcm_wifi_param, on);
		msleep(100);
	}
	return 0;
}

static struct resource brcm_wifi_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= gpio_to_irq(BRCM_WIFI_WAKE_I_PIN),
		.end	= gpio_to_irq(BRCM_WIFI_WAKE_I_PIN),
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct wifi_platform_data brcm_wifi_control = {
	.set_power      = brcm_wifi_power,
	.set_reset      = brcm_wifi_reset,
	.set_carddetect = brcm_wifi_set_carddetect,
	.mem_prealloc   = brcm_wifi_mem_prealloc,
};

static struct platform_device brcm_wifi_device = {
        .name           = "bcmdhd_wlan",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(brcm_wifi_resources),
        .resource       = brcm_wifi_resources,
        .dev            = {
                .platform_data = &brcm_wifi_control,
        },
};

#endif

#if defined(CONFIG_BRCM_GPS)
#include <plat/brcm-gps.h>
static struct brcm_gps_platform_data brcm_gps_data = {
	.gpio_reset = -1,
	.gpio_standby = mfp_to_gpio(BCM_GPS_STANDBY_PIN),
	.gpio_rf_trigger = -1,
};

static struct platform_device brcm_gps_device = {
	.name = "brcm-gps",
	.dev = {
		.platform_data = &brcm_gps_data,
	}
};
#endif

