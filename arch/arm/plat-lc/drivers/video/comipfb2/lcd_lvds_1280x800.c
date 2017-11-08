#include "comipfb.h"
#include "comipfb_dev.h"
#include "mipi_cmd.h"
#include "mipi_interface.h"

//[0]: delay after transfer; [1]:count of data; [2]: word count ls; [3]:word count ms; [4]...: data for transfering
static struct icn6201_reg reg_table[] = {
		{0x20, 0x00},   {0x21, 0x20},   {0x22, 0x35},
		{0x23, 0x28},   {0x24, 0x02},   {0x25, 0x28},
		{0x26, 0x00},   {0x27, 0x0a},   {0x28, 0x01},
		{0x29, 0x0a},   {0x34, 0x80},   {0x36, 0x28},
		{0xb5, 0xa0},   {0x5c, 0xff},   {0x13, 0x10},
		{0x56, 0x90},   {0x6b, 0x21},   {0x69, 0x24},
		{0xb6, 0x20},   {0x51, 0x20},   {0x09, 0x10}
};

static int lcd_lvds_1024x800_power(struct comipfb_info *fbi, int onoff)
{

	int gpio_rst = fbi->pdata->gpio_rst;

	if (gpio_rst < 0) {
		pr_err("no reset pin found\n");
		return -ENXIO;
	}

	gpio_request(gpio_rst, "LCD Reset");

	if (onoff) {
		gpio_direction_output(gpio_rst, 0);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(50);
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
		mdelay(60);
	} else {
		gpio_direction_output(gpio_rst, 0);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		mdelay(10);
	}

	gpio_free(gpio_rst);

	return 0;
}

static int lcd_lvds_1024x800_reset(struct comipfb_info *fbi)
{
	int gpio_rst = fbi->pdata->gpio_rst;

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 1);
		mdelay(1);
		gpio_direction_output(gpio_rst, 0);
		mdelay(50);
		gpio_direction_output(gpio_rst, 1);
		mdelay(5);
		gpio_free(gpio_rst);
	}

	return 0;
}

static int lcd_lvds_1024x800_suspend(struct comipfb_info *fbi)
{
	int ret=0;

	return ret;
}

static int lcd_lvds_1024x800_resume(struct comipfb_info *fbi)
{
	int ret=0;

	return ret;
}

struct comipfb_dev lcd_lvds_1024x800_dev = {
	.name = "lcd_lvds_1280x800",
	.interface_info = COMIPFB_MIPI_IF,
	.lcd_id = LCD_ID_LVDS_1280X800,
	.refresh_en = 1,
	.bpp = 32,
	.xres = 1280,
	.yres = 800,
	.flags = 0,
	.pclk = 65000000,
	.timing = {
		.mipi = {
			.hs_freq = 65000,//71500,		//Kbyte
			.lp_freq = 13000,//14300,		//KHZ
			.no_lanes = 4,
			.display_mode = MIPI_VIDEO_MODE,
			.im_pin_val = 1,
			.color_mode = {
				.color_bits = COLOR_CODE_24BIT,
			},
			.videomode_info = {
				.hsync = 2,
				.hbp = 40,
				.hfp = 40,
				.vsync = 1,
				.vbp = 10,
				.vfp = 10,
				.sync_pol = COMIPFB_VSYNC_HIGH_ACT,
				.lp_cmd_en = 1,
				.lp_hfp_en = 1,
				.lp_hbp_en = 1,
				.lp_vact_en = 1,
				.lp_vfp_en = 1,
				.lp_vbp_en = 1,
				.lp_vsa_en = 1,
				.mipi_trans_type = VIDEO_BURST_WITH_SYNC_PULSES,
			},
			.phytime_info = {
				.clk_tprepare = 3, //HSBYTECLK
			},
			.teinfo = {
				.te_source = 1, //external signal
				.te_trigger_mode = 0,
				.te_en = 0,
				.te_sync_en = 0,
			},
			.ext_info = {
				.eotp_tx_en = 1,
			},
		},
	},
	.lvds_reg = (void *)reg_table,
	.reg_num = ARRAY_SIZE(reg_table),
	.power = lcd_lvds_1024x800_power,
    //.reset = lcd_lvds_1024x800_reset,
	.suspend = lcd_lvds_1024x800_suspend,
	.resume = lcd_lvds_1024x800_resume,
};

static int __init lcd_lvds_1024x800_init(void)
{
	return comipfb_dev_register(&lcd_lvds_1024x800_dev);
}

subsys_initcall(lcd_lvds_1024x800_init);
