#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <plat/hardware.h>
#include <plat/memory.h>
#include <plat/comip-memory.h>
#include <mach/devices.h>
#include <plat/tpz.h>
#include <mach/comip-modem.h>

void __init comip_register_device(struct platform_device *dev, void *data)
{
	int ret;

	dev->dev.platform_data = data;

	ret = platform_device_register(dev);
	if (ret)
		dev_err(&dev->dev, "unable to register device: %d\n", ret);
}

static u64 comip_uart_dma_mask = DMA_BIT_MASK(32);
static struct resource comip_resource_uart0[] = {
	[0] = {
		.start = UART0_BASE,
		.end = UART0_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_UART0,
		.end = INT_UART0,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH9,
		.end = DMAS_CH9,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH1,
		.end = DMAS_CH1,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_uart0 = {
	.name = "comip-uart",
	.id = 0,
	.dev = {
		.dma_mask = &comip_uart_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource = comip_resource_uart0,
	.num_resources = ARRAY_SIZE(comip_resource_uart0),
};

void __init comip_set_uart0_info(struct comip_uart_platform_data *info)
{
	comip_register_device(&comip_device_uart0, info);
}

static struct resource comip_resource_uart1[] = {
	[0] = {
		.start = UART1_BASE,
		.end = UART1_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_UART1,
		.end = INT_UART1,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH10,
		.end = DMAS_CH10,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH2,
		.end = DMAS_CH2,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_uart1 = {
	.name = "comip-uart",
	.id = 1,
	.dev = {
		.dma_mask = &comip_uart_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource = comip_resource_uart1,
	.num_resources = ARRAY_SIZE(comip_resource_uart1),
};

void __init comip_set_uart1_info(struct comip_uart_platform_data *info)
{
	comip_register_device(&comip_device_uart1, info);
}

static struct resource comip_resource_uart2[] = {
	[0] = {
		.start = UART2_BASE,
		.end = UART2_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_UART2,
		.end = INT_UART2,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH11,
		.end = DMAS_CH11,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH3,
		.end = DMAS_CH3,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_uart2 = {
	.name = "comip-uart",
	.id = 2,
	.dev = {
		.dma_mask = &comip_uart_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource = comip_resource_uart2,
	.num_resources = ARRAY_SIZE(comip_resource_uart2),
};

void __init comip_set_uart2_info(struct comip_uart_platform_data *info)
{
	comip_register_device(&comip_device_uart2, info);
}


/* COM_UART */
static struct resource comip_resource_uart3[] = {
	[0] = {
		.start = COM_UART_BASE,
		.end = COM_UART_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_COM_UART,
		.end = INT_COM_UART,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = AUDIO_DMAS_CH10,
		.end = AUDIO_DMAS_CH10,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = AUDIO_DMAS_CH2,
		.end = AUDIO_DMAS_CH2,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_uart3 = {
	.name = "comip-uart",
	.id = 3,
	.dev = {
		.dma_mask = &comip_uart_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource = comip_resource_uart3,
	.num_resources = ARRAY_SIZE(comip_resource_uart3),
};


void __init comip_set_uart3_info(struct comip_uart_platform_data *info)
{
	comip_register_device(&comip_device_uart3, info);
}

static struct resource comip_resource_i2c0[] = {
	[0] = {
		.start = I2C0_BASE,
		.end = I2C0_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_I2C0,
		.end = INT_I2C0,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_i2c0 = {
	.name = "comip-i2c",
	.id = 0,
	.resource = comip_resource_i2c0,
	.num_resources = ARRAY_SIZE(comip_resource_i2c0),
};

void __init comip_set_i2c0_info(struct comip_i2c_platform_data *info)
{
	comip_register_device(&comip_device_i2c0, info);
}

static struct resource comip_resource_i2c1[] = {
	[0] = {
		.start = I2C1_BASE,
		.end = I2C1_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_I2C1,
		.end = INT_I2C1,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_i2c1 = {
	.name = "comip-i2c",
	.id = 1,
	.resource = comip_resource_i2c1,
	.num_resources = ARRAY_SIZE(comip_resource_i2c1),
};

void __init comip_set_i2c1_info(struct comip_i2c_platform_data *info)
{
	comip_register_device(&comip_device_i2c1, info);
}

static struct resource comip_resource_i2c2[] = {
	[0] = {
		.start = I2C2_BASE,
		.end = I2C2_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_I2C2,
		.end = INT_I2C2,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_i2c2 = {
	.name = "comip-i2c",
	.id = 2,
	.resource = comip_resource_i2c2,
	.num_resources = ARRAY_SIZE(comip_resource_i2c2),
};

void __init comip_set_i2c2_info(struct comip_i2c_platform_data *info)
{
	comip_register_device(&comip_device_i2c2, info);
}

static struct resource comip_resource_i2c3[] = {
	[0] = {
		.start = I2C3_BASE,
		.end = I2C3_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_I2C3,
		.end = INT_I2C3,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_i2c3 = {
	.name = "comip-i2c",
	.id = 3,
	.resource = comip_resource_i2c3,
	.num_resources = ARRAY_SIZE(comip_resource_i2c3),
};

void __init comip_set_i2c3_info(struct comip_i2c_platform_data *info)
{
	comip_register_device(&comip_device_i2c3, info);
}

static struct resource comip_resource_i2c4[] = {
	[0] = {
		.start = COM_I2C_BASE,
		.end = COM_I2C_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_COM_I2C,
		.end = INT_COM_I2C,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_i2c4 = {
	.name = "comip-i2c",
	.id = 4,
	.resource = comip_resource_i2c4,
	.num_resources = ARRAY_SIZE(comip_resource_i2c4),
};

void __init comip_set_i2c4_info(struct comip_i2c_platform_data *info)
{
	comip_register_device(&comip_device_i2c4, info);
}

static struct resource comip_resource_spi0[] = {
	[0] = {
		.start = SSI0_BASE,
		.end = SSI0_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SSI0,
		.end = INT_SSI0,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH12,
		.end = DMAS_CH12,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH4,
		.end = DMAS_CH4,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_spi0 = {
	.name = "comip-spi",
	.id = 0,
	.resource = comip_resource_spi0,
	.num_resources = ARRAY_SIZE(comip_resource_spi0),
};

void __init comip_set_spi0_info(struct comip_spi_platform_data *info)
{
	comip_register_device(&comip_device_spi0, info);
}

static struct resource comip_resource_spi1[] = {
	[0] = {
		.start = SSI1_BASE,
		.end = SSI1_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SSI1,
		.end = INT_SSI1,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH13,
		.end = DMAS_CH13,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH5,
		.end = DMAS_CH5,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_spi1 = {
	.name = "comip-spi",
	.id = 1,
	.resource = comip_resource_spi1,
	.num_resources = ARRAY_SIZE(comip_resource_spi1),
};

void __init comip_set_spi1_info(struct comip_spi_platform_data *info)
{
	comip_register_device(&comip_device_spi1, info);
}

static struct resource comip_resource_spi2[] = {
	[0] = {
		.start = SSI2_BASE,
		.end = SSI2_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SSI2,
		.end = INT_SSI2,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH_MAX, //not support dma
		.end = DMAS_CH_MAX,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH_MAX, //not support dma
		.end = DMAS_CH_MAX,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_spi2 = {
	.name = "comip-spi",
	.id = 2,
	.resource = comip_resource_spi2,
	.num_resources = ARRAY_SIZE(comip_resource_spi2),
};

void __init comip_set_spi2_info(struct comip_spi_platform_data *info)
{
	comip_register_device(&comip_device_spi2, info);
}

static struct resource comip_resource_spi3[] = {
	[0] = {
		.start = SSI3_BASE,
		.end = SSI3_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SSI3,
		.end = INT_SSI3,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH15,
		.end = DMAS_CH15,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH7,
		.end = DMAS_CH7,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_spi3 = {
	.name = "comip-spi",
	.id = 3,
	.resource = comip_resource_spi3,
	.num_resources = ARRAY_SIZE(comip_resource_spi3),
};

void __init comip_set_spi3_info(struct comip_spi_platform_data *info)
{
	comip_register_device(&comip_device_spi3, info);
}

static u64 comip_mmc_dma_mask = 0xffffffffUL;
static struct resource comip_resource_mmc0[] = {
	[0] = {
		.start = SDMMC0_BASE,
		.end = SDMMC0_BASE + 0x7ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SDIO0,
		.end = INT_SDIO0,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_mmc0 = {
	.name = "comip-mmc",
	.id = 0,
	.dev = {
		.dma_mask = &comip_mmc_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(comip_resource_mmc0),
	.resource = comip_resource_mmc0,
};

void __init comip_set_mmc0_info(struct comip_mmc_platform_data *info)
{
	comip_register_device(&comip_device_mmc0, info);
}

static struct resource comip_resource_mmc1[] = {
	[0] = {
		.start = SDMMC1_BASE,
		.end = SDMMC1_BASE + 0x7ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SDIO1,
		.end = INT_SDIO1,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_mmc1 = {
	.name = "comip-mmc",
	.id = 1,
	.dev = {
		.dma_mask = &comip_mmc_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(comip_resource_mmc1),
	.resource = comip_resource_mmc1,
};

void __init comip_set_mmc1_info(struct comip_mmc_platform_data *info)
{
	comip_register_device(&comip_device_mmc1, info);
}

static struct resource comip_resource_mmc2[] = {
	[0] = {
		.start = SDMMC2_BASE,
		.end = SDMMC2_BASE + 0x7ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SDIO2,
		.end = INT_SDIO2,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_mmc2 = {
	.name = "comip-mmc",
	.id = 2,
	.dev = {
		.dma_mask = &comip_mmc_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(comip_resource_mmc2),
	.resource = comip_resource_mmc2,
};

void __init comip_set_mmc2_info(struct comip_mmc_platform_data *info)
{
	comip_register_device(&comip_device_mmc2, info);
}

#if defined(CONFIG_CPU_LC1810)
static struct resource comip_resource_mmc3[] = {
	[0] = {
		.start = SDMMC3_BASE,
		.end = SDMMC3_BASE + 0x7ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_SDIO3,
		.end = INT_SDIO3,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_mmc3 = {
	.name = "comip-mmc",
	.id = 3,
	.dev = {
		.dma_mask = &comip_mmc_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(comip_resource_mmc3),
	.resource = comip_resource_mmc3,
};

void __init comip_set_mmc3_info(struct comip_mmc_platform_data *info)
{
	comip_register_device(&comip_device_mmc3, info);
}
#endif

static u64 comip_nand_dma_mask = 0xffffffffUL;
static struct resource comip_resource_nand[] = {
	[0] = {
		.start = NFC_BASE,
		.end = NFC_BASE + 0x7ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_NFC,
		.end = INT_NFC,
		.flags = IORESOURCE_IRQ,
	},
	[2] = { /* RX */
		.start = DMAS_CH8,
		.end = DMAS_CH8,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = DMAS_CH0,
		.end = DMAS_CH0,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_nand = {
	.name = "comip-nand",
	.id = 0,
	.dev = {
		.dma_mask = &comip_nand_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(comip_resource_nand),
	.resource = comip_resource_nand,
};

void comip_set_nand_info(struct comip_nand_platform_data *info)
{
	comip_register_device(&comip_device_nand, info);
}

static struct resource comip_resource_keypad[] = {
	[0] = {
		.start = KBS_BASE,
		.end = KBS_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_KBS,
		.end = INT_KBS,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_keypad = {
	.name = "comip-keypad",
	.id = -1,
	.resource = comip_resource_keypad,
	.num_resources = ARRAY_SIZE(comip_resource_keypad),
};

void __init comip_set_keypad_info(struct comip_keypad_platform_data *info)
{
	comip_register_device(&comip_device_keypad, info);
}

static u64 comip_fb_dma_mask = ~(u64)0;
static struct resource comip_resource_fb0[] = {
	[0] = {
		.start = LCDC0_BASE,
		.end = LCDC0_BASE + 0x7ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MIPI_BASE,
		.end = MIPI_BASE + 0x68,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = IRQ_LCDC0,
		.end = IRQ_LCDC0,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_fb0 = {
	.name = "comipfb",
	.id = 0,
	.dev = {
		.dma_mask = &comip_fb_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(comip_resource_fb0),
	.resource = comip_resource_fb0,
};

void __init comip_set_fb_info(struct comipfb_platform_data *info)
{
	comip_register_device(&comip_device_fb0, info);
}

static u64 comip_camera_dma_mask = ~(u64)0;
static struct resource comip_resource_camera[] = {
	[0] = {
		.start = ISP_BASE,
		.end = ISP_BASE + 0x80000,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_ISP,
		.end = INT_ISP,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_camera = {
	.name = "comip-camera",
	.id = -1,
	.dev = {
		.dma_mask = &comip_camera_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = ARRAY_SIZE(comip_resource_camera),
	.resource = comip_resource_camera,
};

void __init comip_set_camera_info(struct comip_camera_platform_data *info)
{
	comip_register_device(&comip_device_camera, info);
}

static u64 comip_usb_dma_mask = DMA_BIT_MASK(32);
static struct resource comip_resource_udc[] = {
	[0] = {
		.start = USB_OTG_BASE,
		.end = USB_OTG_BASE + 0x3ffff,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_USB_OTG,
		.end = INT_USB_OTG,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_udc = {
	.name = "comip-u2d",
	.id = -1,
	.dev = {
		.dma_mask = &comip_usb_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.num_resources = ARRAY_SIZE(comip_resource_udc),
	.resource = comip_resource_udc,
};

#ifdef CONFIG_USB_COMIP_OTG
static struct resource comip_resource_otg[] = {
	[0] = {
		.start = USB_OTG_BASE,
		.end = USB_OTG_BASE + 0x3ffff,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_USB_OTG,
		.end = INT_USB_OTG,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_otg = {
	.name = "comip-otg",
	.id = -1,
	.num_resources = ARRAY_SIZE(comip_resource_otg),
	.resource = comip_resource_otg,
};
#endif

#ifdef CONFIG_USB_COMIP_HCD
static struct resource comip_resource_hcd[] = {
	[0] = {
		.start = USB_OTG_BASE,
		.end = USB_OTG_BASE + 0x3ffff,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = INT_USB_OTG,
		.end = INT_USB_OTG,
		.flags = IORESOURCE_IRQ,
	},
};
struct platform_device comip_device_hcd = {
	.name = "comip-hcd",
	.id = -1,
	.dev = {
		.dma_mask = &comip_usb_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.num_resources = ARRAY_SIZE(comip_resource_hcd),
	.resource = comip_resource_hcd,
};
#endif

void __init comip_set_usb_info(struct comip_usb_platform_data *info)
{
	comip_register_device(&comip_device_udc, &info->udc);
#ifdef CONFIG_USB_COMIP_HCD
	comip_register_device(&comip_device_hcd, &info->hcd);
#endif
#ifdef CONFIG_USB_COMIP_OTG
	comip_register_device(&comip_device_otg, &info->otg);
#endif
}

static u64 comip_tpz_dma_mask = DMA_BIT_MASK(32);
static struct resource comip_resource_tpz[] = {
	[0] = {
		.start = TPZCTL_BASE,
		.end = TPZCTL_BASE + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_TPZCTL,
		.end = INT_TPZCTL,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_tpz = {
	.name = "comip-tpz",
	.id = -1,
	.dev = {
		.dma_mask = &comip_tpz_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.num_resources = ARRAY_SIZE(comip_resource_tpz),
	.resource = comip_resource_tpz,
};

void __init comip_set_tpz_info(struct comip_tpz_platform_data *info)
{
	comip_register_device(&comip_device_tpz, info);
}

static struct resource comip_resource_pwm[] = {
	[0] = {
		.start = PWM_BASE,
		.end = PWM_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device comip_device_pwm = {
	.name = "comip-pwm",
	.id = -1,
	.resource = comip_resource_pwm,
	.num_resources = ARRAY_SIZE(comip_resource_pwm),
};

void __init comip_set_pwm_info(struct comip_pwm_platform_data *info)
{
	comip_register_device(&comip_device_pwm, info);
}

static struct resource comip_resource_wdt0[] = {
	[0] = {
		.start = WDT0_BASE,
		.end = WDT0_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_WDT,
		.end = INT_WDT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_wdt0 = {
	.name = "comip-wdt",
	.id = 0,
	.num_resources = ARRAY_SIZE(comip_resource_wdt0),
	.resource = comip_resource_wdt0,
};

#ifdef CONFIG_CPU_LC1813
static struct resource comip_resource_wdt1[] = {
	[0] = {
		.start = WDT1_BASE,
		.end = WDT1_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_WDT1,
		.end = INT_WDT1,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_wdt1 = {
	.name = "comip-wdt",
	.id = 1,
	.num_resources = ARRAY_SIZE(comip_resource_wdt1),
	.resource = comip_resource_wdt1,
};

static struct resource comip_resource_wdt2[] = {
	[0] = {
		.start = WDT2_BASE,
		.end = WDT2_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_WDT2,
		.end = INT_WDT2,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_wdt2 = {
	.name = "comip-wdt",
	.id = 2,
	.num_resources = ARRAY_SIZE(comip_resource_wdt2),
	.resource = comip_resource_wdt2,
};

static struct resource comip_resource_wdt3[] = {
	[0] = {
		.start = WDT3_BASE,
		.end = WDT3_BASE + 0x3ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_WDT3,
		.end = INT_WDT3,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device comip_device_wdt3 = {
	.name = "comip-wdt",
	.id = 3,
	.num_resources = ARRAY_SIZE(comip_resource_wdt3),
	.resource = comip_resource_wdt3,
};
#endif

void __init comip_set_wdt_info(struct comip_wdt_platform_data *info)
{
	comip_register_device(&comip_device_wdt0, info);
#ifdef CONFIG_CPU_LC1813
	comip_register_device(&comip_device_wdt1, info);
	comip_register_device(&comip_device_wdt2, info);
	comip_register_device(&comip_device_wdt3, info);
#endif
}

static struct resource comip_resource_modem[] = {
	[0] = {
		.start = MODEM_MEMORY_BASE,
		.end = MODEM_MEMORY_BASE + MODEM_MEMORY_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_CP(0),
		.end = IRQ_CP(7),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device comip_device_modem = {
        .name           = "comip-modem",
        .id             =  -1,
        .num_resources  = ARRAY_SIZE(comip_resource_modem),
        .resource       = comip_resource_modem,
};

void __init comip_set_modem_info(struct comip_modem_platform_data *info)
{
	if(!info->arm0_partition_name)
		info->arm0_partition_name="";
	if(!info->zsp0_partition_name)
		info->zsp0_partition_name="";
	if(!info->zsp1_partition_name)
		info->zsp1_partition_name="";
	comip_register_device(&comip_device_modem, info);
}

static u64 comip_i2s_dma_mask = DMA_BIT_MASK(32);
static struct resource comip_resource_i2s0[] = {
	[0] = {
		.start = I2S0_BASE,
		.end = I2S0_BASE + 0x1f,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = AP_SEC_RAM_BASE,
		.end = AP_SEC_RAM_BASE,
		.flags = IORESOURCE_MEM,
	},
	[2] = { /* RX */
		.start = AUDIO_DMAS_CH8,
		.end = AUDIO_DMAS_CH8,
		.flags = IORESOURCE_DMA,
	},
	[3] = { /* TX */
		.start = AUDIO_DMAS_CH0,
		.end = AUDIO_DMAS_CH0,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_i2s0 = {
	.name = "comip-i2s",
	.id = 0,
	.dev = {
		.dma_mask = &comip_i2s_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource = comip_resource_i2s0,
	.num_resources = ARRAY_SIZE(comip_resource_i2s0),
};

void __init comip_set_i2s0_info(struct comip_i2s_platform_data *info)
{
	comip_register_device(&comip_device_i2s0, info);
}

static struct resource comip_resource_i2s1[] = {
	[0] = {
		.start = I2S1_BASE,
		.end = I2S1_BASE + 0x1f,
		.flags = IORESOURCE_MEM,
	},
	[1] = { /* RX */
		.start = AUDIO_DMAS_CH9,
		.end = AUDIO_DMAS_CH9,
		.flags = IORESOURCE_DMA,
	},
	[2] = { /* TX */
		.start = AUDIO_DMAS_CH1,
		.end = AUDIO_DMAS_CH1,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device comip_device_i2s1 = {
	.name = "comip-i2s",
	.id = 1,
	.dev = {
		.dma_mask = &comip_i2s_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource = comip_resource_i2s1,
	.num_resources = ARRAY_SIZE(comip_resource_i2s1),
};

void __init comip_set_i2s1_info(struct comip_i2s_platform_data *info)
{
	comip_register_device(&comip_device_i2s1, info);
}

