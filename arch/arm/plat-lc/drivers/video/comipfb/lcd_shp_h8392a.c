#include "comipfb.h"
#include "comipfb_dev.h"
#include "mipi_cmd.h"
#include "mipi_interface.h"

static u8 backlight_cmds[][ROW_LINE] = {
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0x51, 0xBE},
};

//[0]: delay after transfer; [1]:count of data; [2]: word count ls; [3]:word count ms; [4]...: data for transfering
static u8 lcd_cmds_init[][ROW_LINE] = {
/****Start Initial Sequence ***/
	{0x78, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0x01, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xB9, 0xFF, 0x83, 0x92},
	{0x01, DCS_CMD, SW_PACK2, 0x02,0xD4, 0x0C},

	{0x01, DCS_CMD, LW_PACK, 0x14, 0x12, 0x00, 0xBA, 0x11, 0x83, 0x00, 0xD6, 0xC6, 0x00, 0x0A, 0xFF, 0x0F, 0x27, 0x03, 0x21, 0x27, 0x25, 0x20, 0x00, 0x00},
	{0x01, DCS_CMD, LW_PACK, 0x05, 0x03, 0x00, 0xC0, 0x01, 0x94},
	{0x01, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0xC6, 0x35, 0x00, 0x00, 0x04},
	{0x01, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xD5, 0x00, 0x00, 0x02},
	{0x01, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xBF, 0x05, 0x60, 0x02},
	{0x01, DCS_CMD, LW_PACK, 0x10, 0x0e, 0x00, 0xb1, 0x7c, 0x00, 0x34, 0x76, 0x00, 0x12, 0x12, 0x1f, 0x1b, 0x19, 0x19, 0x42, 0x4a},
	{0x01, DCS_CMD, SW_PACK2, 0x02, 0xC2, 0x03},
	{0x01, DCS_CMD, SW_PACK2, 0x02, 0x51, 0x80},
	{0x01, DCS_CMD, SW_PACK2, 0x02, 0x53, 0x24},
	{0x05, DCS_CMD, SW_PACK2, 0x02, 0x55, 0x00},
	{0x00, DCS_CMD, LW_PACK, 0x0C, 0x0A, 0x00, 0xC9, 0x0F, 0x01, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x3e},
	{0x0A, DCS_CMD, SW_PACK1, 0x01, 0x29},
};

static u8 lcd_cmds_suspend[][ROW_LINE] = {
	{0x63, DCS_CMD, SW_PACK1, 0x01, 0x28},
	{0xFF, DCS_CMD, SW_PACK1, 0x01, 0x10},	//TODO delay is 400ms.
};

static u8 lcd_cmds_resume[][ROW_LINE] = {
	{0x78, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0x01, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xB9, 0xFF, 0x83, 0x92},
	{0x01, DCS_CMD, SW_PACK2, 0x02,0xD4, 0x0C},
	{0x01, DCS_CMD, LW_PACK, 0x14, 0x12, 0x00, 0xBA, 0x11, 0x83, 0x00, 0xD6, 0xC6, 0x00, 0x0A, 0xFF, 0x0F, 0x27, 0x03, 0x21, 0x27, 0x25, 0x20, 0x00, 0x00},
	{0x01, DCS_CMD, LW_PACK, 0x05, 0x03, 0x00, 0xC0, 0x01, 0x94},
	{0x01, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0xC6, 0x35, 0x00, 0x00, 0x04},
	{0x01, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xD5, 0x00, 0x00, 0x02},
	{0x01, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xBF, 0x05, 0x60, 0x02},
	{0x01, DCS_CMD, LW_PACK, 0x0f, 0x0d, 0x00, 0xb1, 0x7c, 0x00, 0x34, 0x76, 0x00, 0x12, 0x12, 0x2a, 0x25, 0x1e, 0x1e, 0x42, 0x4a},
	{0x01, DCS_CMD, SW_PACK2, 0x02, 0xC2, 0x03},
	{0x01, DCS_CMD, SW_PACK2, 0x02, 0x51, 0x80},
	{0x01, DCS_CMD, SW_PACK2, 0x02, 0x53, 0x24},
	{0x05, DCS_CMD, SW_PACK2, 0x02, 0x55, 0x00},
	{0x00, DCS_CMD, LW_PACK, 0x0C, 0x0A, 0x00, 0xC9, 0x0F, 0x01, 0x1e, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x3e},
	{0x0A, DCS_CMD, SW_PACK1, 0x01, 0x29},
};

static int lcd_shp_h8392a_reset(struct comipfb_info *fbi)
{
	int gpio_rst = fbi->pdata->gpio_rst;

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
		mdelay(120);
		gpio_free(gpio_rst);
	}
	return 0;
}
EXPORT_SYMBOL(lcd_shp_h8392a_reset);

static int lcd_shp_h8392a_suspend(struct comipfb_info *fbi)
{
	int ret=0;

	if (!(fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF))
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_suspend);
	mdelay(20); /*20ms test find*/

	mipi_dsih_dphy_enable_hs_clk(fbi, 0);
	if (fbi->cdev->reset)
		fbi->cdev->reset(fbi);

	mipi_dsih_dphy_enter_ulps(fbi, ENTER_ULPS);
	msleep(5);

	mipi_dsih_dphy_reset(fbi, 0);
	mipi_dsih_dphy_shutdown(fbi, 0);
	msleep(10);

	return ret;
}
EXPORT_SYMBOL(lcd_shp_h8392a_suspend);

static int lcd_shp_h8392a_resume(struct comipfb_info *fbi)
{
	int ret=0;

	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);
	mdelay(15);//must >=15

	mipi_dsih_dphy_exit_ulps(fbi, EXIT_ULPS);
	if (fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF) {
		/* Reset device. */
		if (fbi->cdev->reset)
			fbi->cdev->reset(fbi);
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
  }

	mipi_dsih_cmd_mode(fbi, 1);
	if (fbi->cdev->reset)
		fbi->cdev->reset(fbi);

#ifdef CONFIG_FBCON_DRAW_PANIC_TEXT
	if (unlikely(kpanic_in_progress == 1)) {
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
	} else
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_resume);
#else
	ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_resume);
#endif
	msleep(20);// must >20 test find
	mipi_dsih_dphy_enable_hs_clk(fbi, 1);
	mipi_dsih_video_mode(fbi, 1);

	return ret;
}
EXPORT_SYMBOL(lcd_shp_h8392a_resume);

struct comipfb_dev lcd_shp_h8392a_dev = {
	.name = "lcd_shp_h8392a",
	.control_id = 0,
	.screen_num = 0,
	.interface_info = COMIPFB_MIPI_IF,
	.default_use = 1,
	.bpp = 32,
	.xres = 720,
	.yres = 1280,
	.flags = 0,
	.pclk = 62400000,
	.refresh = 0,
	.timing = {
		.mipi = {
			.reference_freq = 26000,	//KHZ
			.output_freq = 80000,		//Kbyte must less than 43000
			.no_lanes = 3,
			.lcdc_mipi_if_mode = COLOR_CODE_24BIT,
			.hsync = 80,
			.hbp = 80,
			.hfp = 80,
			.vsync = 2,
			.vbp = 4,
			.vfp = 14,
		},
	},
	.cmds_init = {ARRAY_AND_SIZE(lcd_cmds_init)},
	.cmds_suspend = {ARRAY_AND_SIZE(lcd_cmds_suspend)},
	.cmds_resume = {ARRAY_AND_SIZE(lcd_cmds_resume)},
	.reset = lcd_shp_h8392a_reset,
	.suspend = lcd_shp_h8392a_suspend,
	.resume = lcd_shp_h8392a_resume,
	.backlight_info = {{ARRAY_AND_SIZE(backlight_cmds)},
					.brightness_bit = 5,
				},
};

static int __init lcd_shp_h8392a_init(void)
{
	return comipfb_dev_register(&lcd_shp_h8392a_dev);
}

subsys_initcall(lcd_shp_h8392a_init);
