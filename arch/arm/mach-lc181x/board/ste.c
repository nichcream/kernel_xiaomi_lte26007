#if defined(CONFIG_STE_WIFI)
#include <plat/ste-wlan.h>

static int STE_PDB_reset = mfp_to_gpio(STE_WIFI_ONOFF_PIN);
static int cw1200_clk_ctrl(const struct cw1200_platform_data *pdata,
		bool enable);
static struct resource cw1200_platform_resources[] = {
	{
		.start = mfp_to_gpio(STE_WIFI_ONOFF_PIN) ,
		.end = mfp_to_gpio(STE_WIFI_ONOFF_PIN) ,
		.flags = IORESOURCE_IO,
		.name = "cw1200_reset",
	},
	{
		.start =gpio_to_irq(mfp_to_gpio(STE_WIFI_WAKE_I_PIN)),
		.end = gpio_to_irq(mfp_to_gpio(STE_WIFI_WAKE_I_PIN)),
		.flags = IORESOURCE_IRQ,
		.name = "cw1200_irq",
	},
};
static struct cw1200_platform_data cw1200_platform_data = {
	.clk_ctrl = cw1200_clk_ctrl,
};
static struct platform_device cw1200_device = {
	.name = "cw1200_wlan",
	.dev = {
		.platform_data = &cw1200_platform_data,
		.init_name = "cw1200_wlan",
	},
};

const struct cw1200_platform_data *cw1200_get_platform_data(void)
{
	return &cw1200_platform_data;
}
EXPORT_SYMBOL_GPL(cw1200_get_platform_data);
extern void ste_cw1200_detection(void);
extern int cg2900_clk_enable(void);
extern void cg2900_clk_disable(void);
static int cw1200_clk_ctrl(const struct cw1200_platform_data *pdata, bool enable)
{
	int ret = 0;
	if (enable) {
		printk("\n %s enable ",__func__);
		//ret=cg2900_clk_enable();

	} else {

		printk("\n %s disable ",__func__);
		//cg2900_clk_disable();
	}

	return ret;
}
static int __init cw12xx_wlan_init(void)
{
	printk("cw12xx_wlan_init\n");
	cw1200_device.num_resources = ARRAY_SIZE(cw1200_platform_resources);
	cw1200_device.resource = cw1200_platform_resources;
	cw1200_platform_data.mmc_id = "mmc2";
	cw1200_platform_data.reset = &cw1200_device.resource[0];
	cw1200_platform_data.irq = &cw1200_device.resource[1];
	printk("%s WLAN RESET:0x%x, WLAN IRQ: 0x%x \n",__func__,cw1200_platform_data.reset->start,cw1200_platform_data.irq->start );
	gpio_request(STE_PDB_reset, "PDB reset");
	gpio_direction_output(STE_PDB_reset, 1);
	gpio_free(STE_PDB_reset);
	return platform_device_register(&cw1200_device);
}
static void __exit cw12xx_wlan_exit(void)
{
	platform_device_unregister(&cw1200_device);
}

module_init(cw12xx_wlan_init);
module_exit(cw12xx_wlan_exit);
#endif //ste

