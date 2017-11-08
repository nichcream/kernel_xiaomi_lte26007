#include "comipfb.h"
#include "comipfb_dev.h"
#include "mipi_cmd.h"
#include "mipi_interface.h"

//[0]: delay after transfer; [1]:count of data; [2]: word count ls; [3]:word count ms; [4]...: data for transfering
static u8 lcd_cmds_init[][ROW_LINE] = {
	{0x00, DCS_CMD, LW_PACK, 0x05, 0x03, 0x00, 0xF0, 0x5A, 0x5A},
	{0x00, DCS_CMD, LW_PACK, 0x05, 0x03, 0x00, 0xF1, 0x5A, 0x5A},
	{0x00, DCS_CMD, LW_PACK, 0x05, 0x03, 0x00, 0xFC, 0xA5, 0xA5},
	{0x00, DCS_CMD, LW_PACK, 0x05, 0x03, 0x00, 0xD0, 0x00, 0x10},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0xB1, 0x10},
	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0xB2, 0x14, 0x22, 0x2F, 0x04},
	{0x00, DCS_CMD, LW_PACK, 0x08, 0x06, 0x00, 0xF2, 0x02, 0x08, 0x08, 0x90, 0x10},
	{0x00, DCS_CMD, LW_PACK, 0x0D, 0x0B, 0x00, 0xF3, 0x01, 0xD7, 0xE2, 0x62, 0xF4, 0xF7, 0x77, 0x3C, 0x26, 0x00},
	{0x00, DCS_CMD, LW_PACK, 0x30, 0x2E, 0x00, 0xF4, 0x00, 0x02, 0x03, 0x26, 0x03, 0x02, 0x09, 0x00, 0x07, 0x16, 0x16, 0x03, 0x00, 0x08, 0x08, 0x03, 0x0E, 0x0F, 0x12, 0x1C, 0x1D, 0x1E, 0x0C, 0x09, 0x01, 0x04, 0x02, 0x61, 0x74, 0x75, 0x72, 0x83, 0x80, 0x80, 0xB0, 0x00, 0x01, 0x01, 0x28, 0x04, 0x03, 0x28, 0x01, 0xD1, 0x32},
	{0x00, DCS_CMD, LW_PACK, 0x1D, 0x1B, 0x00, 0xF5, 0x83, 0x28, 0x28, 0x5F, 0xAB, 0x98, 0x52, 0x0F, 0x33, 0x43, 0x04, 0x59, 0x54, 0x52, 0x05, 0x40, 0x60, 0x4E, 0x60, 0x40, 0x27, 0x26, 0x52, 0x25, 0x6D, 0x18},
	{0x00, DCS_CMD, LW_PACK, 0x0B, 0x09, 0x00, 0xEE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, DCS_CMD, LW_PACK, 0x0B, 0x09, 0x00, 0xEF, 0x12, 0x12, 0x43, 0x43, 0x90, 0x84, 0x24, 0x81},
	{0x00, DCS_CMD, LW_PACK, 0x09, 0x07, 0x00, 0xF6, 0x63, 0x25, 0xA6, 0x00, 0x00, 0x14},
	{0x00, DCS_CMD, LW_PACK, 0x23, 0x21, 0x00, 0xF7, 0x0A, 0x0A, 0x08, 0x08, 0x0B, 0x0B, 0x09, 0x09, 0x04, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x0A, 0x0A, 0x08, 0x08, 0x0B, 0x0B, 0x09, 0x09, 0x04, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
	{0x00, DCS_CMD, LW_PACK, 0x14, 0x12, 0x00, 0xFA, 0x00, 0x34, 0x01, 0x05, 0x0E, 0x07, 0x0C, 0x12, 0x14, 0x1C, 0x23, 0x2B, 0x34, 0x35, 0x2E, 0x2D, 0x30},
	{0x00, DCS_CMD, LW_PACK, 0x14, 0x12, 0x00, 0xFB, 0x00, 0x34, 0x01, 0x05, 0x0E, 0x07, 0x0C, 0x12, 0x14, 0x1C, 0x23, 0x2B, 0x34, 0x35, 0x2E, 0x2D, 0x30},
	{0x00, DCS_CMD, LW_PACK, 0x08, 0x06, 0x00, 0xFD, 0x16, 0x10, 0x11, 0x23, 0x09},
	{0x00, DCS_CMD, LW_PACK, 0x09, 0x07, 0x00, 0xFE, 0x00, 0x02, 0x03, 0x21, 0x00, 0x78},
	{0x00, DCS_CMD, SW_PACK1, 0x01, 0x35},
	{0xC8, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xC3, 0x40, 0x00, 0x28},
	{0x0A, DCS_CMD, SW_PACK1, 0x01, 0x29},
};

static u8 lcd_cmds_suspend[][ROW_LINE] = {
	{0x14, DCS_CMD, SW_PACK1, 0x01, 0x28},
	{0x64, DCS_CMD, SW_PACK1, 0x01, 0x10},
};

static u8 lcd_cmds_resume[][ROW_LINE] = {
	{0x8C, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0x00, DCS_CMD, SW_PACK1, 0x01, 0x29},
};
static struct common_id_info lcd_common_id_info[] = {
	{ DCS_CMD, {0x94, 0x31}, 2, 0X04},
};
static struct common_id_info lcd_common_esd_info[] = {
	{ DCS_CMD, {0x94, 0x31}, 2, 0X04},
};


static int lcd_starry_s6d7a_power(struct comipfb_info *fbi, int onoff)
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
		mdelay(1);
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, 3300);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
	} else {
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		mdelay(10);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		mdelay(5);
		gpio_direction_output(gpio_rst, 0);
		mdelay(10);
	}

	gpio_free(gpio_rst);

	return 0;
}

static int lcd_starry_s6d7a_reset(struct comipfb_info *fbi)
{
	int gpio_rst = fbi->pdata->gpio_rst;

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
		mdelay(20);
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_free(gpio_rst);
	}
	return 0;
}

static int lcd_starry_s6d7a_suspend(struct comipfb_info *fbi)
{
	int ret=0;
	struct comipfb_dev_timing_mipi *mipi;
	//int gpio_rst = fbi->pdata->gpio_rst;

	mipi = &(fbi->cdev->timing.mipi);

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 1);
	}
#if 0
	comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_pre_suspend);

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 0);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_free(gpio_rst);
	}
#endif
	comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_suspend);

	mdelay(20);
	mipi_dsih_dphy_enable_hs_clk(fbi, 0);

	mipi_dsih_dphy_reset(fbi, 0);
	mipi_dsih_dphy_shutdown(fbi, 0);

	return ret;
}

static int lcd_starry_s6d7a_resume(struct comipfb_info *fbi)
{
	int ret=0;
	struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);

	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);
	//mipi_dsih_dphy_ulps_en(fbi, 0);

	//if (fbi->cdev->reset)
	//	fbi->cdev->reset(fbi);

	if (!(fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF))
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_resume);
	else
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);

	msleep(20);
	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 0);
	}
	mipi_dsih_dphy_enable_hs_clk(fbi, 1);

	return ret;
}

struct comipfb_dev lcd_starry_s6d7a_dev = {
	.name = "lcd_starry_s6d7a",
	.interface_info = COMIPFB_MIPI_IF,
	.lcd_id = LCD_ID_STARRY_S6D7A,
	.refresh_en = 1,
	.bpp = 32,
	.xres = 800,
	.yres = 1280,
	.flags = 0,
	.pclk = 52000000,
	.timing = {
		.mipi = {
			.hs_freq = 52000,//71500,		//Kbyte
			.lp_freq = 10000,//14300,		//KHZ
			.no_lanes = 4,
			.display_mode = MIPI_VIDEO_MODE,
			.im_pin_val = 1,
			.color_mode = {
				.color_bits = COLOR_CODE_24BIT,
			},
			.videomode_info = {
				.hsync = 8,
				.hbp = 160, //48
				.hfp = 16, //16
				.vsync = 4,
				.vbp = 12, //4
				.vfp = 8, //8
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
				//.clk_tprepare = 3, //HSBYTECLK
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
	.panel_id_info = {
		.id_info = lcd_common_id_info,
		.num_id_info = 1,
	},
	.cmds_init = {ARRAY_AND_SIZE(lcd_cmds_init)},
	.cmds_suspend = {ARRAY_AND_SIZE(lcd_cmds_suspend)},
	.cmds_resume = {ARRAY_AND_SIZE(lcd_cmds_resume)},
#ifdef CONFIG_FB_COMIP_ESD
	.esd_id_info = {
		.id_info = lcd_common_esd_info,
		.num_id_info = 1,
	},
#endif
	.power = lcd_starry_s6d7a_power,
	.reset = lcd_starry_s6d7a_reset,
	.suspend = lcd_starry_s6d7a_suspend,
	.resume = lcd_starry_s6d7a_resume,
};

static int __init lcd_starry_s6d7a_init(void)
{
	return comipfb_dev_register(&lcd_starry_s6d7a_dev);
}

subsys_initcall(lcd_starry_s6d7a_init);
