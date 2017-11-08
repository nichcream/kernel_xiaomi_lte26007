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
** Version	Date		Author		Description
** 1.0.0	2014-1-23	xuxuefeng	created
**
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/input.h>
#include <linux/mmc/host.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <plat/comip-pmic.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <plat/mfp.h>
#include <plat/uart.h>
#include <plat/hardware.h>
#include <plat/comip-vibrator.h>
#include <plat/i2s.h>
#include <plat/lc_ion.h>
#include <plat/lc1160_codec.h>

#include <mach/timer.h>
#include <mach/suspend.h>
#include <mach/devices.h>
#include <mach/comip-modem.h>
#include <mach/comip-dvfs.h>
#include "../generic.h"

#if defined(CONFIG_BRCM_WIFI) || defined(CONFIG_BRCM_BLUETOOTH) || defined(CONFIG_BRCM_GPS)
#include "brcm.c"
#endif

#if defined(CONFIG_RTK_WLAN_SDIO) || defined(CONFIG_RTK_BLUETOOTH)
#include "realtek.c"
#endif

#if defined(CONFIG_GPS_UBLOX)
#include "plat/ublox-gps.h"
#endif

struct mfp_gpio_cfg {
	unsigned int gpio_id;
	unsigned int gpio_direction;
	unsigned int gpio_value;
};

enum {
	MFP_GPIO_INPUT = 0,
	MFP_GPIO_OUTPUT,
	MFP_GPIO_DIRECTION_MAX,
};

enum {
	MFP_GPIO_VALUE_LOW = 0,
	MFP_GPIO_VALUE_HIGH,
	MFP_GPIO_VALUE_MAX,
};

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

static DEFINE_SPINLOCK(core_dvfs_lock);

unsigned int get_cur_voltage_level(unsigned int core)
{
	unsigned long flags;
	unsigned int val;

	spin_lock_irqsave(&core_dvfs_lock, flags);
	val = __raw_readl(io_p2v(DDR_PWR_PMU_CTL));
	if (core == GPU_CORE)
		val =  (val >> 4) & 0x3;
	else if (core == CPU_CORE)
		val =  (val >> 12) & 0x3;
	else
		val =  -1;
	spin_unlock_irqrestore(&core_dvfs_lock, flags);
	return val;
}
EXPORT_SYMBOL(get_cur_voltage_level);

void set_cur_voltage_level(unsigned int core, unsigned int level)
{
	unsigned long flags;
	unsigned int val;

	spin_lock_irqsave(&core_dvfs_lock, flags);
	val = __raw_readl(io_p2v(DDR_PWR_PMU_CTL));
	if (core == GPU_CORE) {
		val &= ~(0x3 << 4);
		val |= ((level & 0x3) << 4);
	} else if (core == CPU_CORE) {
		val &= ~(0x3 << 12);
		val |= ((level & 0x3) << 12);
	} else {
		spin_unlock_irqrestore(&core_dvfs_lock, flags);
		return;
	}
	__raw_writel(val, io_p2v(DDR_PWR_PMU_CTL));
	dsb();
	spin_unlock_irqrestore(&core_dvfs_lock, flags);
	udelay(DVS_DELAY);
}
EXPORT_SYMBOL(set_cur_voltage_level);

static int comip_mfp_gpio_config(unsigned int id, unsigned int direction, unsigned int value)
{
	char buf[32];

	if (unlikely((id >= MFP_PIN_MAX)
	             || (direction >= MFP_GPIO_DIRECTION_MAX))|| (value >= MFP_GPIO_VALUE_MAX))
		return -EINVAL;

	snprintf(buf, 32, "gpio%u\n", id);
	gpio_request(id, buf);
	if(direction == MFP_GPIO_INPUT) {
		gpio_direction_input(id);
	} else if(direction == MFP_GPIO_OUTPUT){
		gpio_direction_output(id, value);
	}
	gpio_free(id);

	return 0;
}

static int comip_gpio_config_array(struct mfp_gpio_cfg *pin, int size)
{
	int i;

	if (unlikely(!pin || !size))
		return -EINVAL;

	for (i = 0; i < size; i++)
		comip_mfp_gpio_config(pin[i].gpio_id, pin[i].gpio_direction, pin[i].gpio_value);

	return 0;
}

#if defined(CONFIG_SND_LC186X_SOC)
static int codec_bt_switch(int sel)
{
	int pcm1_tx = mfp_to_gpio(MFP_PIN_GPIO(187));
	int pcm0_tx = mfp_to_gpio(MFP_PIN_GPIO(80));
	int val;
	if (sel == 0x1) { /* codec */
		comip_mfp_config(MFP_PIN_GPIO(187), MFP_PIN_MODE_GPIO);
		gpio_request(pcm1_tx, "PCM1 TX");
		gpio_direction_output(pcm1_tx, 0);
		gpio_free(pcm1_tx);
		comip_mfp_config(MFP_PIN_GPIO(80), MFP_PIN_MODE_0);

		val = readl(io_p2v(MUXPIN_PCM_IO_SEL));
		val &= ~0x01;
		writel(val, io_p2v(MUXPIN_PCM_IO_SEL));
		printk(KERN_DEBUG "codec_bt_switch: CODEC\n");
	} else if (sel == 0x0) { /* bt */
		comip_mfp_config(MFP_PIN_GPIO(80), MFP_PIN_MODE_GPIO);
		gpio_request(pcm0_tx, "PCM0 TX");
		gpio_direction_output(pcm0_tx, 1);
		gpio_free(pcm0_tx);
		comip_mfp_config(MFP_PIN_GPIO(187), MFP_PIN_MODE_0);

		val = readl(io_p2v(MUXPIN_PCM_IO_SEL));
		val |= 0x01;
		writel(val, io_p2v(MUXPIN_PCM_IO_SEL));
		printk(KERN_DEBUG "codec_bt_switch: BT\n");
	} else {
		printk(KERN_DEBUG "codec_bt_switch: error, sel=%d\n", sel);
	}
	return 0;
}

static int lc1160_power_save(int sel, int clk_id)
{
	int ret;
	if (sel) {
		if (clk_id == 0) {
			comip_mfp_config(LC1160_GPIO_MCLK, MFP_PIN_MODE_GPIO);
			ret = gpio_request(LC1160_GPIO_MCLK, "mclk_pin");
			if (ret) {
				printk("Failed  to request gpio_mclk pin\n");
				return -EACCES;
			}
			gpio_direction_output(LC1160_GPIO_MCLK, 0);
			gpio_free(LC1160_GPIO_MCLK);
		} else if (clk_id == 1) {
			comip_mfp_config(LC1160_GPIO_ECLK, MFP_PIN_MODE_GPIO);
			ret = gpio_request(LC1160_GPIO_ECLK, "eclk_pin");
			if (ret) {
				printk("Failed  to request gpio_eclk pin\n");
				return -EACCES;
			}
			gpio_direction_output(LC1160_GPIO_ECLK, 0);
			gpio_free(LC1160_GPIO_ECLK);
		}
	} else {
		if(clk_id == 1) {
			comip_mfp_config(LC1160_GPIO_ECLK, MFP_PIN_MODE_0);
		} else if (clk_id == 0) {
			comip_mfp_config(LC1160_GPIO_MCLK, MFP_PIN_MODE_0);
		}
	}
	return 0;
}

static int pa_enable(int enable)
{
#if defined(CODEC_PA_PIN)
	int pa_gpio=mfp_to_gpio(CODEC_PA_PIN);
	comip_mfp_config(pa_gpio,MFP_PIN_MODE_GPIO);
	gpio_request(pa_gpio, "codec_pa_gpio");
	gpio_direction_output(pa_gpio, enable);
	gpio_free(pa_gpio);
#endif
	return 0;
}

static struct platform_device comip_snd_soc_device = {
	.name = "comip_snd_soc",
	.id = 0,
};

static struct comip_codec_platform_data lc1160_codec_platform_data = {
	.pcm_switch = codec_bt_switch,
	.codec_power_save = lc1160_power_save,
	.pa_enable=pa_enable,
#if defined (LC1160_INT2_PIN)
	.irq_gpio = mfp_to_gpio(LC1160_INT2_PIN),
#endif
	.mclk_id = "clkout3_clk",
	.eclk_id = "clkout4_clk",
#if defined (CODEC_JACK_POLARITY)
	.jack_polarity = CODEC_JACK_POLARITY,
#endif
	.vth1 = 0x01,
	.vth2 = 0x00,
	.vth3 = 0x00,
};

static struct platform_device comip_codec_device = {
	.name =	"comip_codec",
	.id = -1,
	.dev = {
		.platform_data = &lc1160_codec_platform_data,
	}
};

static struct platform_device snd_lowpower_device = {
	.name =	"comip-snd-lowpower",
	.id = -1,
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
	},
	{
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
	{	.type	= ION_HEAP_TYPE_SYSTEM,
		.id	= ION_HEAP_SYSTEM_ID,
		.name	= "ion_system_heap",
	},
#ifdef CONFIG_COMIP_IOMMU
	{	.type	= LC_ION_HEAP_TYPE_SYSTEM_MMU,
		.id	= LC_ION_HEAP_LCMEM_ID,
		.name	= "lc_ion_smmu_heap",
	},
#else
	{	.type	= LC_ION_HEAP_TYPE_LCMEM,
		.id	= LC_ION_HEAP_LCMEM_ID,
		.name	= "lc_ion_lcmem_heap",
	},
#endif
	{	.type	= LC_ION_HEAP_TYPE_SYSTEM,
		.id	= LC_ION_HEAP_SYSTEM_ID,
		.name	= "lc_ion_system_heap",
	},
	{	.type	= LC_ION_HEAP_TYPE_LCMEM,
		.id	= LC_ION_HEAP_LCMEM_ALIAS_ID,
		.name	= "lc_ion_lcmem_heap",
	},
};

static struct ion_platform_data lc_ion_pdata = {
	.nr = 4,
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

#if defined(CONFIG_GPS_UBLOX)
static void gps_power_ctrl(int onoff)
{
	int ret = 0;
	if(onoff){
		ret = pmic_voltage_set(PMIC_POWER_GPS_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		msleep(5);
	}
	else
		ret = pmic_voltage_set(PMIC_POWER_GPS_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
}

static struct ublox_gps_platform_data ublox_gps_data = {
	.gpio_reset = mfp_to_gpio(UBLOX_GPS_RESET_PIN),
	.power_state = 0,
	.power = gps_power_ctrl,
};

static struct platform_device ublox_gps_device = {
	.name = "ublox-gps",
	.dev = {
		.platform_data = &ublox_gps_data,
	}
};
#endif

static struct platform_device *common_devices[] __initdata = {
	&comip_ureg_device,
	&comip_rtc_device,
	&comip_powerkey_device,
	&comip_vibrator_device,
#if defined(CONFIG_SND_LC186X_SOC)
	&comip_codec_device,
	&snd_lowpower_device,
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
#if defined(CONFIG_GPS_UBLOX)
	&ublox_gps_device,
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
	{SDCARD_DETECT_PIN,	MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_USB_COMIP_OTG)
	/* OTG ID. */
	{USB_OTG_ID_PIN,	MFP_PIN_MODE_GPIO},
#endif
#if defined(CONFIG_COMIP_LC1160)
	/* LC1160. */
	{LC1160_INT_PIN,	MFP_PIN_MODE_GPIO},
	 /*LC1160 CLK OUT 0/4 */
#endif
#if defined(CONFIG_KEYBOARD_GPIO_COMIP)
	{COMIP_GPIO_KEY_VOLUMEUP,	MFP_PIN_MODE_GPIO},
	{COMIP_GPIO_KEY_VOLUMEDOWN,	MFP_PIN_MODE_GPIO},
#endif
	/* I2C2. */
	{MFP_PIN_GPIO(163),	MFP_PIN_MODE_1},
	{MFP_PIN_GPIO(164),	MFP_PIN_MODE_1},

	/* SIM0. */
	{MFP_PIN_GPIO(66),	MFP_PIN_MODE_0}, /* clk */
	{MFP_PIN_GPIO(67),	MFP_PIN_MODE_0}, /* io */
	{MFP_PIN_GPIO(68),	MFP_PIN_MODE_0}, /* rst */

	/* SIM1. */
	{MFP_PIN_GPIO(69), MFP_PIN_MODE_0}, /* clk */
	{MFP_PIN_GPIO(70), MFP_PIN_MODE_0}, /* io */
	{MFP_PIN_GPIO(71), MFP_PIN_MODE_0}, /* rst */
#if defined(CONFIG_SND_LC186X_SOC)
	/* PCM0. */
	{MFP_PIN_GPIO(80),	MFP_PIN_MODE_0}, /* tx */
	{MFP_PIN_GPIO(81),	MFP_PIN_MODE_0}, /* rx */
	{MFP_PIN_GPIO(82),	MFP_PIN_MODE_0}, /* clk */
	{MFP_PIN_GPIO(83),	MFP_PIN_MODE_0}, /* ssn */
	/* PCM1. */
	{MFP_PIN_GPIO(187),	MFP_PIN_MODE_0}, /* tx */
	{MFP_PIN_GPIO(188),	MFP_PIN_MODE_0}, /* rx */
	{MFP_PIN_GPIO(189),	MFP_PIN_MODE_0}, /* clk */
	{MFP_PIN_GPIO(190),	MFP_PIN_MODE_0}, /* ssn */
#endif
	/*boot_ctl[0~2] gpio mode config for arm0 boot*/
	{MFP_PIN_GPIO(242),	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(241),	MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(240),	MFP_PIN_MODE_GPIO},

	/* UART2 RX */
	{MFP_PIN_GPIO(184),	MFP_GPIO184_UART2_RX_GPIO},
};
static struct mfp_pull_cfg comip_common_mfp_pull_cfg[] = {
#if defined(CONFIG_SND_LC186X_SOC)
	/* PCM0. */
	{MFP_PIN_GPIO(83),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(82),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(81),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(80),	MFP_PULL_DISABLE},
	/* PCM1. */
	//{MFP_PIN_GPIO(190),	MFP_PULL_UP},
	//{MFP_PIN_GPIO(189),	MFP_PULL_DISABLE},
	//{MFP_PIN_GPIO(188),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(187),	MFP_PULL_DOWN},
	/*I2S0*/
	{MFP_PIN_GPIO(84),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(85),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(86),	MFP_PULL_DISABLE},
	{MFP_PIN_GPIO(87),	MFP_PULL_DISABLE},
#endif

#if defined(CONFIG_BRCM_GPS)
	/* UART1 CTS pull disable for brcm gps*/
	{MFP_PIN_GPIO(181),	MFP_PULL_DISABLE},
#endif

#if defined(CONFIG_USB_COMIP_OTG)
	/* OTG ID. */
	{USB_OTG_ID_PIN,	MFP_PULL_UP},
#endif

	/* UART2 RX */
	{MFP_PIN_GPIO(184),	MFP_PULL_UP},

/* unused floating pin gpio201,gpio204 pull down */
	{MFP_PIN_GPIO(201), MFP_PULL_DOWN},
	{MFP_PIN_GPIO(204), MFP_PULL_DOWN},
};

static struct mfp_pull_cfg comip_eco_mfp_pull_cfg[] = {
	{MFP_PIN_GPIO(231), MFP_PULL_DOWN},
	{MFP_PIN_GPIO(232), MFP_PULL_DOWN},
};
static void __init comip_init_common_mfp(void)
{
#if defined(CONFIG_USB_COMIP_OTG)
	int  gpio_status=0x0;
#endif
	comip_mfp_config_array(comip_common_mfp_cfg,
			ARRAY_SIZE(comip_common_mfp_cfg));
	comip_mfp_config_pull_array(comip_common_mfp_pull_cfg,
			ARRAY_SIZE(comip_common_mfp_pull_cfg));
	if (cpu_is_lc1860_eco1() || cpu_is_lc1860_eco2()) {
		comip_mfp_config_pull_array(comip_eco_mfp_pull_cfg,
				ARRAY_SIZE(comip_eco_mfp_pull_cfg));
	}

#if defined(CONFIG_USB_COMIP_OTG)
	gpio_request(USB_OTG_ID_PIN, "comip-otg");
	gpio_direction_input(USB_OTG_ID_PIN);

	gpio_status = gpio_get_value(USB_OTG_ID_PIN) ? 1 : 0;
	printk("%s p_otg->fsm.id=%d\n", __FUNCTION__,gpio_status);
#endif
}
#if defined(CONFIG_SERIAL_COMIP) || defined(CONFIG_SERIAL_COMIP_MODULE)
/* GPS & BT.*/
static struct comip_uart_platform_data comip_uart1_info = {
#if defined(CONFIG_GPS_UBLOX)  //u-blox use uart without flow ctrl
	.flags = COMIP_UART_TX_USE_DMA | COMIP_UART_RX_USE_DMA,
#else
	.flags = COMIP_UART_TX_USE_DMA | COMIP_UART_RX_USE_DMA \
			| COMIP_UART_SUPPORT_MCTRL,
#endif
};

/*BT*/
static int comip_bt_uart_power(void *param, int onoff)
{
#if defined(CONFIG_BRCM_BLUETOOTH)
	brcm_bt_uart_register(param, onoff);
#endif
	return 0;
}

static struct comip_uart_platform_data comip_uart2_info = {
#if defined(CONFIG_BRCM_BLUETOOTH)
	.flags = COMIP_UART_TX_USE_DMA | COMIP_UART_RX_USE_DMA \
		| COMIP_UART_SUPPORT_MCTRL |COMIP_UART_DISABLE_MSI,
#else
	.flags = COMIP_UART_TX_USE_DMA | COMIP_UART_RX_USE_DMA \
		| COMIP_UART_SUPPORT_MCTRL | COMIP_UART_RX_USE_GPIO_IRQ,
#endif
	.rx_gpio = MFP_PIN_GPIO(184),
	.power = comip_bt_uart_power,
};

/* DEBUG. */
static struct comip_uart_platform_data comip_uart3_info = {
	.flags = COMIP_UART_USE_WORKQUEUE,
};

/* DEBUG. */
static struct comip_uart_platform_data comip_uart0_info = {
	.flags = COMIP_UART_USE_WORKQUEUE,
};

static void __init comip_init_uart(void)
{
	comip_set_uart0_info(&comip_uart0_info);
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
#if defined(CONFIG_COMIP_BOARD_LX70A_V1_0)
	if (gpio_get_value(gpio_cd))
		pdata->cd = 1;
	else
		pdata->cd = 0;
#else
	if (gpio_get_value(gpio_cd))
		pdata->cd = 0;
	else
		pdata->cd = 1;
#endif
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

#if defined(CONFIG_COMIP_BOARD_LX70A_V1_0)
	if (gpio_get_value(gpio_cd))
		pdata->cd = 1;
	else
		pdata->cd = 0;
#else
	if (gpio_get_value(gpio_cd))
		pdata->cd = 0;
	else
		pdata->cd = 1;
#endif
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

typedef struct _event_dispatch{
	int level;
	int module;
	int param;
	int mv;
}event_dispatch;

static event_dispatch card_dispatch[] = {
		{23, PMIC_POWER_SDIO, 0, 3300},
		{22, PMIC_POWER_SDIO, 0, 3300},
		{21, PMIC_POWER_SDIO, 0, 3300},
		{20, PMIC_POWER_SDIO, 0, 3300},
		{19, PMIC_POWER_SDIO, 0, 3000},
		{18, PMIC_POWER_SDIO, 0, 3000},
		{17, PMIC_POWER_SDIO, 0, 3000},
		{16, PMIC_POWER_SDIO, 0, 2800},
		{15, PMIC_POWER_SDIO, 0, 2800},
		{14, PMIC_POWER_SDIO, 0, 2800},
		{7,  PMIC_POWER_SDIO, 0, 1200},
		{0,  PMIC_POWER_SDIO, 0, PMIC_POWER_VOLTAGE_DISABLE},
};

#if defined(CONFIG_MMC_COMIP_IOPOWER)
static event_dispatch signal_dispatch[] = {
		{2, PMIC_POWER_SDIO, 1, 3300},
		{1, PMIC_POWER_SDIO, 1, 1800},
		{0, PMIC_POWER_SDIO, 1, PMIC_POWER_VOLTAGE_DISABLE},
};
#endif

static void comip_mmc0_set_power(struct device *dev, unsigned int vdd)
{
	int ret = 0;
	int i, done;
#ifndef CONFIG_COMIP_BOARD_LX70A_V1_0
	for(i = 0, done = 0;i < sizeof(card_dispatch)/sizeof(event_dispatch);i++) {
		if(card_dispatch[i].level == vdd) {
			ret = pmic_voltage_set(card_dispatch[i].module, card_dispatch[i].param,
						card_dispatch[i].mv);
			if(ret < 0)
				printk(KERN_ERR"%s: set card vdd  ret = %d vdd = %d\n", __func__, ret, vdd);

			break;
		}
	}

#endif
#if defined(CONFIG_MMC_COMIP_IOPOWER)
	for(i = 0, done = 0;i < sizeof(signal_dispatch)/sizeof(event_dispatch);i++) {
		if(signal_dispatch[i].level == vdd) {
			ret = pmic_voltage_set(signal_dispatch[i].module, signal_dispatch[i].param,
						signal_dispatch[i].mv);
			if(ret < 0)
				printk(KERN_ERR"%s set signal vdd ret = %d vdd = %d\n", __func__, ret, vdd);
			else
				done = 1;

			break;
		}
	}

	if (!done) {
		/*IOPOWER use default value. */
		ret = pmic_voltage_set(signal_dispatch[0].module, signal_dispatch[0].param,
						signal_dispatch[0].mv);
		if(ret < 0)
			printk(KERN_ERR"%s set signal vdd ret = %d vdd = %d\n",
					__func__, ret, signal_dispatch[0].level);
	}
#endif
}

/* SD Card. */
static struct comip_mmc_platform_data comip_mmc0_info = {
	.flags = MMCF_SUSPEND_HOST | MMCF_ERROR_RECOVERY | MMCF_MONITOR_TIMER | MMCF_USE_CMD23,
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29,
	.clk_rate_max = 44571428,
	.clk_rate = 44571428,
	.clk_read_delay = 3,
	.clk_write_delay = 8,
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
	.flags = MMCF_IGNORE_PM | MMCF_8BIT_DATA | MMCF_CAP2_HS200 | MMCF_ERROR_RECOVERY
		| MMCF_MONITOR_TIMER | MMCF_USE_CMD23,
	.ocr_mask = 0x40ff8080,
	.clk_rate_max = 104000000,
	.clk_rate = 104000000,
	.clk_read_delay = 3,
	.clk_write_delay = 3,
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
#if defined(CONFIG_RTK_WLAN_SDIO_3)
	.flags = MMCF_IGNORE_PM|MMCF_KEEP_POWER | MMCF_SET_SEG_BLK_SIZE|MMCF_UHS_I,//|MMCF_SUSPEND_HOST|MMCF_SUSPEND_SDIO,
	.ocr_mask = MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_165_195,
	.clk_rate_max = 104000000,
	.clk_rate = 104000000,
	.max_seg_size = 8192,
	.max_blk_size = 8192,
#else
	.flags = MMCF_IGNORE_PM|MMCF_KEEP_POWER|MMCF_SET_SEG_BLK_SIZE,//|MMCF_SUSPEND_HOST|MMCF_SUSPEND_SDIO,
	.ocr_mask = MMC_VDD_30_31 | MMC_VDD_31_32,
	.clk_rate_max = 78000000,
	.clk_rate = 39000000,
	.max_seg_size = 8192,
	.max_blk_size = 8192,

#endif
#else
	.flags = MMCF_IGNORE_PM | MMCF_KEEP_POWER,
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29,
	.clk_rate_max = 78000000,
	.clk_rate = 39000000,
#endif

	.clk_read_delay = 0,
	.clk_write_delay = 0,
	.detect_delay = 0,
	.init = comip_mmc2_init,
	.exit = comip_mmc2_exit,
	.get_cd = comip_mmc2_get_cd,
};

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
int usb_power_set(int onoff)
{
	return 0;
}
static struct comip_usb_platform_data comip_usb_info = {
#ifdef CONFIG_USB_COMIP_OTG
	.otg = {
		.id_gpio = mfp_to_gpio(USB_OTG_ID_PIN),
		.debounce_interval = 50,
	},
#endif
	.hcd = {
		.usb_power_set = usb_power_set,
#ifdef CONFIG_COMIP_HSIC_USBHUB
		.hub_power = mfp_to_gpio(USB_HUB_POWER),
		.hub_reset = mfp_to_gpio(USB_HUB_RESET),
		.eth_power = mfp_to_gpio(USB_ETH_POWER),
		.eth_reset = mfp_to_gpio(USB_ETH_RESET),
#endif
	}
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
		{MFP_PIN_GPIO(153), MFP_PIN_MODE_0},
		{MFP_PIN_GPIO(174), MFP_PIN_MODE_0},
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

#if defined(CONFIG_METS_COMIP) || defined(CONFIG_METS_COMIP_MODULE)
static void __init comip_init_mets(void)
{
	comip_set_mets_info(NULL);
}
#else
static void inline comip_init_mets(void)
{
};
#endif


#if defined(CONFIG_COMIP_MODEM)
static struct modem_platform_data comip_modem_info = {
	.img_flag = IN_BLOCK_EMMC,
	.arm_partition_name = "modemarm",
	.zsp_partition_name = "modemdsp",
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

#if defined(CONFIG_SND_COMIP_VIRTUAL)
static void __init comip_init_virtual_pcm(void)
{
	comip_set_virtual_pcm_info();
}
#else
static inline void comip_init_virtual_pcm(void)
{
}
#endif

#if defined(CONFIG_SND_COMIP_PCM)
static void __init comip_init_pcm(void)
{
	comip_set_pcm_info();
}
#else
static inline void comip_init_pcm(void)
{
}
#endif

#if defined(CONFIG_COMIP_IOMMU)
static void __init comip_init_smmu(void)
{
	comip_set_smmu_info();
}
#else
static inline void comip_init_smmu(void)
{
}
#endif
#if defined(CONFIG_LC186X_FLOWCAL)
static void __init  comip_init_flowcal(void)
{
	comip_set_flowcal_info();
}
#else
static inline void comip_init_flowcal(void)
{
}
#endif

static void __init comip_init_board_early(void);
static void __init comip_init_board(void);
static void __init comip_init(void)
{
	comip_generic_init();
	comip_dmas_init();
	comip_suspend_init();
	comip_init_common_mfp();
	comip_init_board_early();
	comip_init_tpz();
	comip_init_mets();
	comip_init_smmu();
	comip_init_pwm();
	comip_init_mmc();
	comip_init_nand();
	comip_init_uart();
	comip_init_usb();
	comip_init_watchdog();
	comip_init_modem();
	comip_init_i2s();
	comip_init_virtual_pcm();
	comip_init_pcm();
	comip_init_common_devices();
	comip_init_flowcal();
	comip_init_board();
}

static void __init comip_init_irq(void)
{
	comip_irq_init();
	comip_gpio_init(1);
}

MACHINE_START(LC186X, "Leadcore Innopower")
	.smp = smp_ops(lc1860_smp_ops),
	.smp_init = smp_init_ops(bL_smp_init_ops),
	.map_io = comip_map_io,
	.init_irq = comip_init_irq,
	.init_machine = comip_init,
	.init_time = comip_timer_init,
MACHINE_END
