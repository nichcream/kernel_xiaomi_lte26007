static atomic_t rtk_ps_flag = ATOMIC_INIT(0);
static void rtk_set_powersave(int on)
{
	/* Set power. */
#if defined(CONFIG_COMIP_PROJECT_LX70A)
/* on lx70a board, realtek8723bs not got power supply from our own pmu
 * so we do nothing here
 */
#else
	if (on) {
		atomic_dec(&rtk_ps_flag);
		if(atomic_read(&rtk_ps_flag)==0) {
			pmic_voltage_set(PMIC_POWER_WLAN_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
			pmic_power_ctrl_set(PMIC_POWER_WLAN_IO, 0, PMIC_POWER_CTRL_OSCEN);
			printk("rtk set powersave enter\n");
		}
		//mdelay(20);
	} else {
		atomic_inc(&rtk_ps_flag);
		if(atomic_read(&rtk_ps_flag)==1) {
			pmic_voltage_set(PMIC_POWER_WLAN_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
			pmic_power_ctrl_set(PMIC_POWER_WLAN_IO, 0, PMIC_POWER_CTRL_COSCEN);
			printk("rtk set powersave exit\n");
		}
	}
#endif
}

#if defined(CONFIG_RTK_WLAN_SDIO)
#include <linux/wlan_plat.h>
static struct mmc_host *rtk_wifi_sdio;
static void *rtk_wifi_param;
static void (*rtk_wifi_on_detect)(void *param, int on);

/* Wlan GPIO. */
static int rtk_wifi_onoff = mfp_to_gpio(RTK_WIFI_ONOFF_PIN);
static int rtk_wifi_wake_i = mfp_to_gpio(RTK_WIFI_WAKE_I_PIN);
static int rtk_btwifi_chipen = mfp_to_gpio(RTK_BTWIFI_CHIPEN_PIN);
/* BT GPIO. */
#ifdef CONFIG_BT
static int rtk_bt_onoff = mfp_to_gpio(RTK_BT_ONOFF_PIN);
static int rtk_bt_wake_i = mfp_to_gpio(RTK_BT_WAKE_I_PIN);
static int rtk_bt_wake_o = mfp_to_gpio(RTK_BT_WAKE_O_PIN);
static int rtk_bt_reset = mfp_to_gpio(RTK_BT_RESET_PIN);
#endif
static int __init rtk_wifi_init(void)
{
	printk("rtk  wifi init\n");
	comip_mfp_config(RTK_WIFI_ONOFF_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(RTK_WIFI_WAKE_I_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(RTK_BTWIFI_CHIPEN_PIN, MFP_PIN_MODE_GPIO);

	gpio_request(rtk_btwifi_chipen, "rtk chip sel");
	gpio_direction_output(rtk_btwifi_chipen, 1);
	gpio_free(rtk_btwifi_chipen);

	/* Power on WLAN. */
	gpio_request(rtk_wifi_onoff, "Wlan Poweron");
	gpio_direction_output(rtk_wifi_onoff, 0);
	gpio_free(rtk_wifi_onoff);

	gpio_request(rtk_wifi_wake_i, "Wlan WLAN2AP Wake");
	gpio_direction_input(rtk_wifi_wake_i);
	gpio_free(rtk_wifi_wake_i);

	return 0;
}
rootfs_initcall(rtk_wifi_init);
static void rtk_wifi_setpower(int on)
{
	printk("rtk wifi setpower: %d\n", on);
	/* Set power. */
#if defined(CONFIG_COMIP_PROJECT_LX70A)
/* on lx70a board, realtek8723bs not got power supply from our own pmu
 * so we do nothing here
 */
#else
	if (on) {
		pmic_voltage_set(PMIC_POWER_WLAN_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		/*turn off PA by default*/
		pmic_voltage_set(PMIC_POWER_WLAN_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_power_ctrl_set(PMIC_POWER_WLAN_IO, 0, PMIC_POWER_CTRL_OSCEN);
		mdelay(20);
	} else {
		pmic_voltage_set(PMIC_POWER_WLAN_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_WLAN_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
	}
#endif
}

static int rtk_wifi_sdio_register(void (*on_detect)(void *param, int on), void *param, void* sdio)
{
	rtk_wifi_param = param;
	rtk_wifi_sdio = (struct mmc_host *)sdio;
	rtk_wifi_on_detect = on_detect;
	return 0;
}

static int rtk_wifi_restore_host(void)
{
	struct mmc_host *host = rtk_wifi_sdio;
	int ret = 0;

	if (host)
		ret = mmc_power_restore_host(host);

	return ret;
}

static int rtk_wifi_save_host(void)
{
	struct mmc_host *host = rtk_wifi_sdio;
	int ret = 0;

	if (host)
		ret = mmc_power_save_host(host);

	return ret;
}

static int rtk_wifi_power(int flag)
{
	printk("rtk wlan power: %d\n", flag);

	gpio_request(rtk_wifi_onoff, "Wlan Poweron");

	if(1==flag) {
		rtk_wifi_setpower(1);
		/* Power on. */
		gpio_direction_output(rtk_wifi_onoff, 0);
		gpio_set_value(rtk_wifi_onoff,0);
		mdelay(10);
		gpio_set_value(rtk_wifi_onoff,1);
		mdelay(150);
		rtk_wifi_restore_host();
		mdelay(50);
	} else if(2==flag){
		/*Power save enter*/
		rtk_set_powersave(1);
	} else if(3==flag){
		/*Power save exit*/
		rtk_set_powersave(0);
	} else {
		/* Power off. */
		gpio_direction_output(rtk_wifi_onoff, 0);
		gpio_set_value(rtk_wifi_onoff,0);
		mdelay(10);
		rtk_wifi_save_host();
		rtk_wifi_setpower(0);
	}

	gpio_free(rtk_wifi_onoff);

	return 0;
}

static int rtk_wifi_reset(int on)
{
	printk("wlan reset: %d\n", on);
	return 0;
}

static int rtk_wifi_set_carddetect(int on)
{
	if (rtk_wifi_on_detect) {
		rtk_wifi_on_detect(rtk_wifi_param, on);
		msleep(1000);
	}
	return 0;
}

static struct resource rtk_wifi_resources[] = {
	{
		.name	= "rtk_wlan_irq",
		.start	= gpio_to_irq(RTK_WIFI_WAKE_I_PIN),
		.end	= gpio_to_irq(RTK_WIFI_WAKE_I_PIN),
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL,
	},
#ifdef CONFIG_BT
	{
		.name	= "wifi_BT_reset_pin",
		.start	= mfp_to_gpio(RTK_BT_RESET_PIN),
		.end	= mfp_to_gpio(RTK_BT_RESET_PIN),
		.flags  = IORESOURCE_IO,
	},
#endif
};

static struct wifi_platform_data rtk_wifi_control = {
	.set_power      = rtk_wifi_power,
	.set_reset      = rtk_wifi_reset,
	.set_carddetect = rtk_wifi_set_carddetect,
};

static struct platform_device rtk_wifi_device = {
        .name           = "rtk_wlan",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(rtk_wifi_resources),
        .resource       = rtk_wifi_resources,
        .dev            = {
                .platform_data = &rtk_wifi_control,
        },
};

#endif

#if defined(CONFIG_RTK_BLUETOOTH)
#include <plat/rtk-bluetooth.h>

static struct mfp_pull_cfg comip_PCM1_mfp_pull_enable_cfg[] = {
	/* PCM1. */
	{MFP_PIN_GPIO(190),	MFP_PULL_UP},
	{MFP_PIN_GPIO(189),	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(188),	MFP_PULL_DOWN},
	{MFP_PIN_GPIO(187),	MFP_PULL_DOWN},

};

static struct mfp_pull_cfg comip_PCM1_mfp_pull_disable_cfg[] = {
	/* PCM1. */
	{MFP_PIN_GPIO(190),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(189),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(188),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(187),	MFP_PULL_DISABLE},

};


static int __init rtk_bt_init(void)
{
	printk("rtk  bt init\n");
	comip_mfp_config(RTK_BT_RESET_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(RTK_BT_WAKE_I_PIN, MFP_PIN_MODE_GPIO);
	comip_mfp_config(RTK_BT_WAKE_O_PIN, MFP_PIN_MODE_GPIO);

	/* Power on BT. */
	if (RTK_BT_ONOFF_PIN != MFP_PIN_GPIO(MFP_PIN_MAX)){
	comip_mfp_config(RTK_BT_ONOFF_PIN, MFP_PIN_MODE_GPIO);
	gpio_request(rtk_bt_onoff, "BT Poweron");
	gpio_direction_output(rtk_bt_onoff, 1);
	gpio_free(rtk_bt_onoff);
	}

	/* Reset Bluetooth chip. */
	gpio_request(rtk_bt_reset, "Bluetooth Reset");
	gpio_direction_output(rtk_bt_reset, 0);
	gpio_free(rtk_bt_reset);

	gpio_request(rtk_bt_wake_o, "Bluetooth AP2BT Wake");
	gpio_direction_output(rtk_bt_wake_o, 0);
	gpio_free(rtk_bt_wake_o);

	gpio_request(rtk_bt_wake_i, "Bluetooth BT2AP Wake");
	gpio_direction_input(rtk_bt_wake_i);
	gpio_free(rtk_bt_wake_i);

	return 0;
}
rootfs_initcall(rtk_bt_init);

static int rtk_bt_power_on(void)
{
	printk("rtk bt power on\n");
	rtk_set_powersave(0);

	comip_mfp_config_pull_array(comip_PCM1_mfp_pull_disable_cfg,
			ARRAY_SIZE(comip_PCM1_mfp_pull_disable_cfg));

	gpio_request(rtk_bt_reset, "Bluetooth Reset");

	/* Power on. */
	if (RTK_BT_ONOFF_PIN != MFP_PIN_GPIO(MFP_PIN_MAX)){
	gpio_request(rtk_bt_onoff, "BT Poweron");
	gpio_direction_output(rtk_bt_onoff, 1);
	gpio_free(rtk_bt_onoff);
	}

	/* Reset bluetooth and enable bluetooth firmware run. */
	gpio_direction_output(rtk_bt_reset, 0);
	msleep(100);
	gpio_direction_output(rtk_bt_reset, 1);
	msleep(400);

	gpio_free(rtk_bt_reset);

	return 0;
}

static int rtk_bt_power_off(void)
{
	printk("rtk bt power off\n");
	rtk_set_powersave(1);
	/* Reset bluetooth. */
	gpio_request(rtk_bt_reset, "Bluetooth Reset");
	gpio_direction_output(rtk_bt_reset, 0);
	gpio_free(rtk_bt_reset);

	/* Power off. */
	if (RTK_BT_ONOFF_PIN != MFP_PIN_GPIO(MFP_PIN_MAX)){
	gpio_request(rtk_bt_onoff, "BT Poweron");
	gpio_direction_output(rtk_bt_onoff, 0);
	gpio_free(rtk_bt_onoff);
	}

	comip_mfp_config_pull_array(comip_PCM1_mfp_pull_enable_cfg,
			ARRAY_SIZE(comip_PCM1_mfp_pull_enable_cfg));

	return 0;
}

static struct rtk_bt_platform_data rtk_bt_data = {
	.name = "rtk-bluetooth",
	.gpio_wake_i = mfp_to_gpio(RTK_BT_WAKE_I_PIN),
	.gpio_wake_o = mfp_to_gpio(RTK_BT_WAKE_O_PIN),
	.poweron = rtk_bt_power_on,
	.poweroff = rtk_bt_power_off,
};

static struct platform_device rtk_bt_device = {
	.name = "rtk-bluetooth",
	.dev = {
		.platform_data = &rtk_bt_data,
	}
};

static struct uart_port *rtk_bt_uart;


static struct uart_port* rtk_bt_get_uart(void)
{
	return rtk_bt_uart;
}

static struct resource rtk_bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= mfp_to_gpio(RTK_BT_WAKE_I_PIN),
		.end	= mfp_to_gpio(RTK_BT_WAKE_I_PIN),
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= mfp_to_gpio(RTK_BT_WAKE_O_PIN),
		.end	= mfp_to_gpio(RTK_BT_WAKE_O_PIN),
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= gpio_to_irq(mfp_to_gpio(RTK_BT_WAKE_I_PIN)),
		.end	= gpio_to_irq(mfp_to_gpio(RTK_BT_WAKE_I_PIN)),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device rtk_bluesleep_device = {
	.name	= "bluesleep",
	.id	= -1,
	.num_resources = ARRAY_SIZE(rtk_bluesleep_resources),
	.resource = rtk_bluesleep_resources,
	.dev = {
		.platform_data = rtk_bt_get_uart,
	}
};

#endif

