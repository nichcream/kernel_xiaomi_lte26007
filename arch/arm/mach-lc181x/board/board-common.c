/*
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** Copyright (c) 2010-2019 LeadCoreTech Corp.
**
** PURPOSE: comip machine.
**
** CHANGE HISTORY:
**
** Version	Date		Author			Description
** 1.0.0	2013-03-21	liuyong			created
**
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio_keys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/irqchip/arm-gic.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <plat/hardware.h>
#include <plat/switch.h>
#include <plat/comip-vibrator.h>
#include <plat/comip-pmic.h>
#include <plat/comip-battery.h>
#include <plat/lc1132.h>
#include <plat/lc1132-pmic.h>
#include <plat/comip_codecs_interface.h>
#include <plat/i2s.h>
#include <plat/lc_ion.h>

#include <plat/tpz.h>
#include <mach/timer.h>
#include <mach/suspend.h>
#include <mach/devices.h>
#include <mach/comip-multicore.h>
#include <mach/comip-modem.h>

#include "../generic.h"
#include "brcm.c"
#include "ste.c"

#if defined(CONFIG_RTK_WLAN_SDIO) || defined(CONFIG_RTK_BLUETOOTH)
#include "realtek.c"
#endif
static struct platform_device comip_ureg_device = {
	.name = "comip-ureg",
	.id = 0,
};

static struct platform_device comip_rtc_device = {
	.name = "comip-rtc",
	.id = -1,
};

static struct platform_device comip_powerkey_device = {
	.name = "comip-powerkey",
};

static struct comip_vibrator_platform_data comip_vibrator_data = {
	.name = "vibrator",
	.vibrator_strength = 1,
};

static struct platform_device comip_vibrator_device = {
	.name = "comip-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &comip_vibrator_data,
	}
};

#if defined(CONFIG_SWITCH_COMIP)

#if defined(CONFIG_SND_COMIP_LC1132)
static struct comip_switch_platform_data comip_switch_data = {
	.irq_gpio = LC1132_INT_PIN,
	.detect_name = "h2w",
	.button_name = "h2w headset",
	.detect_id = COMIP_SWITCH_FLAG_ACTIVE_LOW,
};
#endif

static struct platform_device comip_switch_device = {
	.name = "comip-switch",
	.id = -1,
	.dev = {
		.platform_data = &comip_switch_data,
	}
};
#endif

#if defined(CONFIG_SND_LC181X_SOC)
static int codec_bt_switch(int sel)
{
	int pcm1_tx = mfp_to_gpio(MFP_PIN_GPIO(177));
	int pcm0_tx = mfp_to_gpio(MFP_PIN_GPIO(173));
	int val;
	if (sel == CODEC_PCM_SEL) {
		comip_mfp_config(MFP_PIN_GPIO(177), MFP_PIN_MODE_GPIO);
		gpio_request(pcm1_tx, "PCM1 TX");
		gpio_direction_output(pcm1_tx, 0);
		gpio_free(pcm1_tx);
		comip_mfp_config(MFP_PIN_GPIO(173), MFP_GPIO173_COM_PCM0_TX);

		val = readl(io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
		val &= ~0x10;
		writel(val, io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
		printk(KERN_DEBUG "codec_bt_switch: CODEC\n");
	} else if (sel == BT_PCM_SEL) {
		comip_mfp_config(MFP_PIN_GPIO(173), MFP_PIN_MODE_GPIO);
		gpio_request(pcm0_tx, "PCM0 TX");
		gpio_direction_output(pcm0_tx, 1);
		gpio_free(pcm0_tx);
		comip_mfp_config(MFP_PIN_GPIO(177), MFP_GPIO177_COM_PCM1_TX);

		val = readl(io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
		val |= 0x10;
		writel(val, io_p2v(MUX_PIN_SDMMC3_PAD_CTRL));
		printk(KERN_DEBUG "codec_bt_switch: BT\n");
	} else {
		printk(KERN_DEBUG "codec_bt_switch: error, sel=%d\n", sel);
	}

	return 0;
}

static struct comip_codec_interface_platform_data comip_codec_interface_data = {
	.pcm_switch = codec_bt_switch,
};

static struct platform_device comip_snd_soc_device = {
	.name = "comip_snd_soc",
	.id = 0,
};

static struct platform_device comip_codec_device = {
	.name =	"comip_codec",
	.id = -1,
	.dev = {
		.platform_data = &comip_codec_interface_data,
	}
};
#endif

#if defined(CONFIG_KEYBOARD_GPIO_COMIP)
static struct gpio_keys_button comip_gpio_key_buttons[] = {
	{
		.code		= KEY_VOLUMEUP,
		.gpio		= COMIP_GPIO_KEY_VOLUMEUP,
		.desc		= "volumeup Button",
		.active_low	= 1,
		.debounce_interval = 30,
	}, {
		.code		= KEY_VOLUMEDOWN,
		.gpio		= COMIP_GPIO_KEY_VOLUMEDOWN,
		.desc		= "volumedown Button",
		.active_low	= 1,
		.debounce_interval = 30,
	},
};

static struct gpio_keys_platform_data comip_gpio_key_button_data = {
	.buttons	= comip_gpio_key_buttons,
	.nbuttons	= ARRAY_SIZE(comip_gpio_key_buttons),
};

static struct platform_device comip_gpio_button_device = {
	.name		= "comip-gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &comip_gpio_key_button_data,
	},
};
#endif
#ifdef CONFIG_ION_LC
static struct ion_platform_heap lc_ion_heaps[] = {
	{	.type = ION_HEAP_TYPE_SYSTEM,
		.id = ION_HEAP_SYSTEM_ID,
		.name = "ion_system_heap",
	},
	{	.type = LC_ION_HEAP_TYPE_LCMEM,
		.id = LC_ION_HEAP_LCMEM_ID,
		.name = "lc_ion_lcmem_heap",
	},
	{	.type = LC_ION_HEAP_TYPE_SYSTEM,
		.id = LC_ION_HEAP_SYSTEM_ID,
		.name = "lc_ion_system_heap",
	},
	{	.type = LC_ION_HEAP_TYPE_DMA,
		.id = LC_ION_HEAP_DMA_ID,
		.name = "lc_ion_dma_heap",
	},
	{	.type	= LC_ION_HEAP_TYPE_LCMEM,
		.id = LC_ION_HEAP_LCMEM_ALIAS_ID,
		.name = "lc_ion_lcmem_heap",
	},
};

static struct ion_platform_data lc_ion_pdata = {
	.nr = 5,
	.heaps = lc_ion_heaps,
};

static struct platform_device lc_ion_device = {
	.name		= "ion-lc",
	.id		= -1,
	.dev		= {
		.platform_data = &lc_ion_pdata,
	}
};
#endif

static struct platform_device *common_devices[] __initdata = {
	&comip_ureg_device,
	&comip_rtc_device,
	&comip_powerkey_device,
	&comip_vibrator_device,
#if defined(CONFIG_SWITCH_COMIP)
	&comip_switch_device,
#endif
#if defined(CONFIG_SND_LC181X_SOC)
	&comip_codec_device,
	&comip_snd_soc_device,
#endif
#if defined(CONFIG_BRCM_GPS)
	&brcm_gps_device,
#endif
#if defined(CONFIG_BRCM_BLUETOOTH)
	&brcm_bt_device,
	&brcm_bluesleep_device,
#endif
#if defined(CONFIG_RTK_BLUETOOTH)
	&rtk_bt_device,
	&rtk_bluesleep_device,
#endif
#if defined(CONFIG_BRCM_WIFI)
	&brcm_wifi_device,
#endif
#if defined(CONFIG_RTK_WLAN_SDIO)
	&rtk_wifi_device,
#endif
#if defined(CONFIG_KEYBOARD_GPIO_COMIP)
	&comip_gpio_button_device,
#endif
#if defined(CONFIG_ION_LC)
	&lc_ion_device,
#endif
};

static void __init comip_init_common_devices(void)
{
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
}

static struct mfp_pin_cfg comip_common_mfp_cfg[] = {
#if defined(CONFIG_COMIP_SDCARD_DETECT)
	{SDCARD_DETECT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_USB_COMIP_OTG)
	/* OTG ID. */
	{USB_OTG_ID_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_COMIP_LC1132)
	/* LC1132. */
	{LC1132_INT_PIN,		MFP_PIN_MODE_GPIO},
#endif

#if defined(CONFIG_KEYBOARD_GPIO_COMIP)
	{COMIP_GPIO_KEY_VOLUMEUP,	MFP_PIN_MODE_GPIO},
	{COMIP_GPIO_KEY_VOLUMEDOWN, 	MFP_PIN_MODE_GPIO},
#endif

	/* I2C2. */
	{MFP_PIN_GPIO(171),		MFP_GPIO171_I2C2_SCL},
	{MFP_PIN_GPIO(172),		MFP_GPIO172_I2C2_SDA},

#if defined(CONFIG_SND_LC181X_SOC)
	/* PCM0. */
	{MFP_PIN_GPIO(173),		MFP_GPIO173_COM_PCM0_TX},
	{MFP_PIN_GPIO(174),		MFP_GPIO174_COM_PCM0_RX},
	{MFP_PIN_GPIO(175),		MFP_GPIO175_COM_PCM0_CLK},
	{MFP_PIN_GPIO(176),		MFP_GPIO176_COM_PCM0_SSN},
	/* PCM1. */
	{MFP_PIN_GPIO(177),		MFP_GPIO177_COM_PCM1_TX},
	{MFP_PIN_GPIO(178),		MFP_GPIO178_COM_PCM1_RX},
	{MFP_PIN_GPIO(179),		MFP_GPIO179_COM_PCM1_CLK},
	{MFP_PIN_GPIO(180),		MFP_GPIO180_COM_PCM1_SSN},
#endif

	/*boot_ctl[0~2] gpio mode config for arm0 boot*/
	{MFP_PIN_GPIO(226),		MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(227),		MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(228),		MFP_PIN_MODE_GPIO},
};

static struct mfp_pull_cfg comip_common_mfp_pull_cfg[] = {
#if defined(CONFIG_SND_LC181X_SOC)
	{MFP_PULL_COM_PCM_SSN,		MFP_PULL_UP},
	{MFP_PULL_COM_PCM_CLK,		MFP_PULL_DISABLE},
	{MFP_PULL_COM_PCM_RX,		MFP_PULL_DISABLE},
	{MFP_PULL_COM_PCM_TX,		MFP_PULL_DOWN},
	{MFP_PULL_COM_PCM0_SSN,		MFP_PULL_UP},
	{MFP_PULL_COM_PCM0_CLK,		MFP_PULL_DISABLE},
	{MFP_PULL_COM_PCM0_RX,		MFP_PULL_DISABLE},
	{MFP_PULL_COM_PCM0_TX,		MFP_PULL_DISABLE},
#endif
};

static void __init comip_init_common_mfp(void)
{
	comip_mfp_config_array(comip_common_mfp_cfg,
			ARRAY_SIZE(comip_common_mfp_cfg));
	comip_mfp_config_pull_array(comip_common_mfp_pull_cfg,
			ARRAY_SIZE(comip_common_mfp_pull_cfg));
}

#if defined(CONFIG_SERIAL_COMIP) || defined(CONFIG_SERIAL_COMIP_MODULE)
static int comip_bt_uart_power(void *param, int onoff)
{
#if defined(CONFIG_BRCM_BLUETOOTH)
	brcm_bt_uart_register(param, onoff);
#endif
	return 0;
}

/* GPS & BT.*/
static struct comip_uart_platform_data comip_uart1_info = {
	.flags = COMIP_UART_TX_USE_DMA | COMIP_UART_RX_USE_DMA \
			| COMIP_UART_SUPPORT_MCTRL | COMIP_UART_DISABLE_MSI,
	.power = comip_bt_uart_power,
};

static struct comip_uart_platform_data comip_uart2_info = {
	.flags = COMIP_UART_TX_USE_DMA | COMIP_UART_RX_USE_DMA \
				| COMIP_UART_SUPPORT_MCTRL,
};

/* DEBUG. */
static struct comip_uart_platform_data comip_uart3_info = {
	.flags = COMIP_UART_USE_WORKQUEUE,
};

static void __init comip_init_uart(void)
{
	//comip_set_uart0_info(&comip_uart0_info);
	comip_set_uart1_info(&comip_uart1_info);
	comip_set_uart2_info(&comip_uart2_info);
	comip_set_uart3_info(&comip_uart3_info);
}
#else
static inline void comip_init_uart(void)
{
}
#endif

#if defined(CONFIG_MMC_COMIP) || defined(CONFIG_MMC_COMIP_MODULE)
#if defined(CONFIG_COMIP_SDCARD_DETECT)
static int comip_mmc0_init(struct device * dev,
	   irq_handler_t comip_detect_int, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_mmc_platform_data *pdata = pdev->dev.platform_data;
	int gpio_cd = pdata->detect_gpio;
	int irq_cd = gpio_to_irq(gpio_cd);
	int irq_type;
	int err;

	/*
	    Setup GPIO for comip MMC controller.
	*/
	err = gpio_request(gpio_cd,"sd card detect");
	if (err)
		goto err_request_cd;

	gpio_direction_input(gpio_cd);
	if (gpio_get_value(gpio_cd))
		pdata->cd = 0;
	else
		pdata->cd = 1;

	printk(KERN_DEBUG "mmc-%d: card detect %d. gpio %d.\n", pdev->id, pdata->cd, gpio_cd);

	irq_type = (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING);
	err = request_irq(irq_cd, comip_detect_int, irq_type, "sd card detect", data);
	if (err) {
		printk(KERN_ERR "mmc-%d: "
		"can't request card detect IRQ\n", pdev->id);

		goto err_request_irq;
	}

	irq_set_irq_wake(irq_cd, 1);

	return 0;

err_request_irq:
	gpio_free(gpio_cd);
err_request_cd:
	return err;
}

static int comip_mmc0_get_cd(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_mmc_platform_data *pdata = pdev->dev.platform_data;
	int gpio_cd = pdata->detect_gpio;

	if (gpio_get_value(gpio_cd))
		pdata->cd = 0;
	else
		pdata->cd = 1;

	return pdata->cd;
}

static void comip_mmc0_exit(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_mmc_platform_data *pdata = pdev->dev.platform_data;
	int gpio_cd = pdata->detect_gpio;
	int irq_cd = gpio_to_irq(gpio_cd);

	free_irq(irq_cd, data);
	gpio_free(gpio_cd);
}
#endif

static void comip_mmc0_set_power(struct device *dev, unsigned int vdd)
{
	/* Set power. */
	if (vdd)
		pmic_voltage_set(PMIC_POWER_SDIO, 0, PMIC_POWER_VOLTAGE_ENABLE);
	else
		pmic_voltage_set(PMIC_POWER_SDIO, 0, PMIC_POWER_VOLTAGE_DISABLE);
}

/* SD Card. */
static struct comip_mmc_platform_data comip_mmc0_info = {
	.flags = MMCF_IGNORE_PM  | MMCF_MONITOR_TIMER | MMCF_ERROR_RECOVERY,
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29,
	.clk_rate_max = 44500000,
	.clk_rate = 44500000,
	.clk_read_delay =  3,
	.clk_write_delay =  8,
#if defined(CONFIG_COMIP_SDCARD_DETECT)
	.detect_gpio = mfp_to_gpio(SDCARD_DETECT_PIN),
	.detect_delay = 100,
	.init = comip_mmc0_init,
	.get_cd = comip_mmc0_get_cd,
	.exit =comip_mmc0_exit,
#endif
	.setpower = comip_mmc0_set_power,
};

/* EMMC. */
static struct comip_mmc_platform_data comip_mmc1_info = {
	.flags = MMCF_IGNORE_PM | MMCF_8BIT_DATA,
	.ocr_mask = 0x40ff8080,
	.clk_rate_max = 104000000,
	.clk_rate = 52000000,
	.clk_read_delay = 0,
	.clk_write_delay = 0,
	.detect_delay = 0,
};

#if defined(CONFIG_BRCM_WIFI)
static void comip_sdio_on_detect(void *param, int cd)
{
	struct comip_mmc_platform_data* pdata = param;

	if (pdata && pdata->detect_handle) {
		pdata->cd = cd;
		pdata->detect_handle(0, pdata->detect_data);
	}
}
#endif

#if defined(CONFIG_RTK_WLAN_SDIO)
static void comip_sdio_on_detect(void *param, int cd)
{
	struct comip_mmc_platform_data* pdata = param;

	if (pdata && pdata->detect_handle) {
		pdata->cd = cd;
		pdata->detect_handle(0, pdata->detect_data);
	}
}
#endif
static int comip_mmc2_get_cd(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_mmc_platform_data* pdata = pdev->dev.platform_data;

	return pdata->cd;
}

static int comip_mmc2_init(struct device *dev,
				irq_handler_t comip_detect_int, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_mmc_platform_data* pdata = pdev->dev.platform_data;

	pdata->detect_handle = comip_detect_int;
	pdata->detect_data = data;
	pdata->cd = 0;

#if defined(CONFIG_BRCM_WIFI)
	brcm_wifi_sdio_register(comip_sdio_on_detect, pdata, data);
#endif

#if defined(CONFIG_RTK_WLAN_SDIO)
	rtk_wifi_sdio_register(comip_sdio_on_detect, pdata, data);
#endif
	return 0;
}

static void comip_mmc2_exit(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct comip_mmc_platform_data* pdata = pdev->dev.platform_data;

	pdata->detect_handle = NULL;
	pdata->detect_data = NULL;
	pdata->cd = 0;
}

/* WIFI. */
static struct comip_mmc_platform_data comip_mmc2_info = {
#if defined(CONFIG_RTK_WLAN_SDIO)
	.flags = MMCF_IGNORE_PM|MMCF_KEEP_POWER|MMCF_SET_SEG_BLK_SIZE,
	.ocr_mask = MMC_VDD_30_31 | MMC_VDD_31_32,
	.clk_rate_max = 52000000,
	.clk_rate = 26000000,
	.max_seg_size = 8192,
	.max_blk_size = 8192,
#else
	.flags = MMCF_IGNORE_PM,
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29,
#if defined(CONFIG_STE_WIFI)
	.clk_rate_max = 38000000, 
	.clk_rate = 19000000, 
#else	
	.clk_rate_max = 78000000,
	.clk_rate = 39000000,
#endif
#endif
	.clk_read_delay = 0,
	.clk_write_delay = 0,
	.detect_delay = 0,
	.init = comip_mmc2_init,
	.exit = comip_mmc2_exit,
	.get_cd = comip_mmc2_get_cd,
};
#if defined(CONFIG_STE_WIFI)

static void comip_sdio_on_detect(int cd)
{
	struct comip_mmc_platform_data* pdata = &comip_mmc2_info;

	if (pdata->detect_handle) {
		pdata->cd = cd;
		pdata->detect_handle(0, pdata->detect_data);
	}
}

int ste_wifi_set_carddetect(int on)
{
	comip_sdio_on_detect(on);
	return 0;
}
EXPORT_SYMBOL_GPL(ste_wifi_set_carddetect);
#endif
static void __init comip_init_mmc(void)
{
	comip_set_mmc1_info(&comip_mmc1_info);
	comip_set_mmc0_info(&comip_mmc0_info);
	comip_set_mmc2_info(&comip_mmc2_info);
}
#else
static inline void comip_init_mmc(void)
{
}
#endif

#if defined(CONFIG_NAND_COMIP) || defined(CONFIG_NAND_COMIP_MODULE)
static struct comip_nand_platform_data comip_nand_info = {
	.use_dma = 0,
	.use_irq = 0,
};

static void __init comip_init_nand(void)
{
	comip_set_nand_info(&comip_nand_info);
}
#else
static inline void comip_init_nand(void)
{
}
#endif

#if defined(CONFIG_USB_COMIP) || defined(CONFIG_USB_COMIP_MODULE)
static struct comip_usb_platform_data comip_usb_info = {
#ifdef CONFIG_USB_COMIP_OTG
	.otg = {
		.id_gpio = mfp_to_gpio(USB_OTG_ID_PIN),
		.debounce_interval = 50,
	}
#endif
};

static void __init comip_init_usb(void)
{
	comip_set_usb_info(&comip_usb_info);
}
#else
static void inline comip_init_usb(void)
{
};
#endif

static void __init comip_init_watchdog(void)
{
	comip_set_wdt_info(NULL);
}

#if defined(CONFIG_PWM_COMIP) || defined(CONFIG_PWM_COMIP_MODULE)
static void pwm_device_init(int id)
{
	static struct mfp_pin_cfg pwm_mfp_pwm[COMIP_PWM_NUM] = {
		{MFP_PIN_GPIO(149), MFP_GPIO149_PWM_0},
		{MFP_PIN_GPIO(71), MFP_GPIO71_PWM_1},
	};

	if ((id >= 0) && (id < COMIP_PWM_NUM))
		comip_mfp_config(pwm_mfp_pwm[id].id, pwm_mfp_pwm[id].mode);
}

static struct comip_pwm_platform_data comip_pwm_info = {
	.dev_init = pwm_device_init,
};

static void __init comip_init_pwm(void)
{
	comip_set_pwm_info(&comip_pwm_info);
}
#else
static void inline comip_init_pwm(void)
{
};
#endif

#if defined(CONFIG_TPZ_COMIP) || defined(CONFIG_TPZ_COMIP_MODULE)
static void __init comip_init_tpz(void)
{
	comip_set_tpz_info(NULL);
}
#else
static void inline comip_init_tpz(void)
{
};
#endif

#if defined(CONFIG_COMIP_MODEM)
static struct comip_modem_platform_data comip_modem_info = {
	.img_flag = IN_BLOCK_EMMC,
	.td_rfid = SOC_RFIC_TD_RDA8208,
	.gsm_rfid = SOC_RFIC_GSM_RDA8208,
	.bbid = SOC_PARTID_BASICDEV_1813,
	.gsm_dual_rfid = SOC_RFIC_GSM_RDA8208,
	.arm0_partition_name = "modemarm",
	.zsp0_partition_name = "modemdsp0",
	.zsp1_partition_name = "modemdsp1",
};

static void __init comip_init_modem(void)
{
	comip_set_modem_info(&comip_modem_info);
}
#else
static inline void comip_init_modem(void)
{
}
#endif

#if defined(CONFIG_SND_COMIP_I2S)
static struct comip_i2s_platform_data comip_i2s0_info = {
	.flags = 0,
};

static void __init comip_init_i2s(void)
{
	comip_set_i2s0_info(&comip_i2s0_info);
}
#else
static inline void comip_init_i2s(void)
{
}
#endif

static void __init comip_init_board_early(void);
static void __init comip_init_board(void);

static void __init comip_init(void)
{
	comip_generic_init();
	comip_cache_init();

	comip_mtc_init();
	comip_dmas_init();
	comip_suspend_init();
	comip_init_common_mfp();
	comip_init_board_early();
	comip_init_tpz();
	comip_init_pwm();
	comip_init_mmc();
	comip_init_nand();
	comip_init_uart();
	comip_init_usb();
	comip_init_watchdog();
	comip_init_modem();
	comip_init_i2s();
	comip_init_common_devices();
	comip_init_board();
}

static void __init comip_init_irq(void)
{
	comip_irq_init();
	comip_gpio_init(0);
}

MACHINE_START(LC181X, "Leadcore Innopower")
	.smp = smp_ops(lc1813_smp_ops),
	.map_io = comip_map_io,
	.init_irq = comip_init_irq,
	.init_machine = comip_init,
	.init_time = comip_timer_init,
MACHINE_END
