#include "comipfb.h"
#include "comipfb_dev.h"
#include "mipi_cmd.h"
#include "mipi_interface.h"

static u8 backlight_cmds[][ROW_LINE] = {
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0x51, 0xBE},
//	{0x00, GEN_CMD, LW_PACK, 0x07, 0x05, 0x00, 0xBB, 0x7B, 0xFF, 0x0F, 0x02, 0x01, 0x00},
};

//[0]: delay after transfer; [1]:count of data; [2]: word count ls; [3]:word count ms; [4]...: data for transfering
static u8 lcd_cmds_init[][ROW_LINE] = {
/****Start Initial Sequence ***/
#if 1
	 {0x0a, DCS_CMD, SW_PACK2, 0x02, 0x80,0x58},

        {0x0a, DCS_CMD, SW_PACK2, 0x02, 0x81, 0x47},

        {0x0a, DCS_CMD, SW_PACK2, 0x02, 0x82, 0xD4},

        {0x0a, DCS_CMD, SW_PACK2, 0x02, 0x83, 0x88},

        {0x0a, DCS_CMD, SW_PACK2, 0x02, 0x84, 0xa9},

        {0x0a, DCS_CMD, SW_PACK2, 0x02, 0x85, 0xc3},

        {0x0a, DCS_CMD, SW_PACK2, 0x02, 0x86, 0x82},
#endif
	{0xa, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0xaa, DCS_CMD, SW_PACK1, 0x01, 0x29},		//dcs short no param
};

static u8 lcd_cmds_suspend[][ROW_LINE] = {
	{0x64, DCS_CMD, SW_PACK1, 0x01, 0x28},
	{0xff, DCS_CMD, SW_PACK1, 0x01, 0x10},	//TODO delay is 300ms.
};

static u8 lcd_cmds_resume[][ROW_LINE] = {
	{0xff, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0xff, DCS_CMD, SW_PACK1, 0x01, 0x29},
};

static int lcd_ws_l706_mipi_ek79007_ips_reset(struct comipfb_info *fbi)
{
	int gpio_rst = fbi->pdata->gpio_rst;
	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		mdelay(60);//T2 time
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
	        mdelay(60);
		gpio_direction_output(gpio_rst, 1);
		mdelay(20);
		gpio_free(gpio_rst);
	}
	return 0;
}

static int lcd_ws_l706_mipi_ek79007_ips_suspend(struct comipfb_info *fbi)
{
	int ret=0;
	int gpio_rst = fbi->pdata->gpio_rst;
	
        struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 1);
	}
//	if (!(fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF))
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_suspend);
	mdelay(20);
	mipi_dsih_dphy_enable_hs_clk(fbi, 0);
	msleep(10);

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 0);
		gpio_free(gpio_rst);
	}

	mipi_dsih_dphy_ulps_en(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 0);
	mipi_dsih_dphy_shutdown(fbi, 0);
  
	return ret;
}

static int lcd_ws_l706_mipi_ek79007_ips_resume(struct comipfb_info *fbi)
{
	int ret=0;
	struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);

	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);
	mipi_dsih_dphy_ulps_en(fbi, 0);

	if (fbi->cdev->reset)
    		fbi->cdev->reset(fbi);

#ifdef CONFIG_FBCON_DRAW_PANIC_TEXT
	if (unlikely(kpanic_in_progress == 1)) {
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
		mipi_dsih_dphy_enable_hs_clk(fbi, 1);
	}
	else
	{
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
		mipi_dsih_dphy_enable_hs_clk(fbi, 1);
	}
#else
	ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
#endif
	msleep(20);
	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 0);
	}
	mipi_dsih_dphy_enable_hs_clk(fbi, 1);

	return ret;
}

struct comipfb_dev lcd_ws_l706_mipi_ek79007_ips_dev = {
	.name = "lcd_ws_l706_mipi_ek79007_ips",
	.interface_info = COMIPFB_MIPI_IF,
	.lcd_id = LCD_ID_HS_EK79007,
	.refresh_en = 1,//mipi =1; RGB = 0
	.bpp = 32,
	.xres = 1024,
	.yres = 600,
	.flags = 0,
	.pclk = 52000000,//30M,62
	.timing = {
		.mipi = {
			.hs_freq = 54000,		//Kbyte
			.lp_freq = 10000,		//KHZ,for read id BTA
			.no_lanes = 4,
			.display_mode = MIPI_VIDEO_MODE,
			.im_pin_val = 1,		//im pin for mipi and lvds for standby
			.color_mode = {
				.color_bits = COLOR_CODE_24BIT,
			},
			.videomode_info = {
#if 0
				.hsync = 4,
				.hbp = 10,
				.hfp = 10,
				.vsync = 4,
				.vbp = 7,
				.vfp = 5,
#if 0
				.hsync = 6,
				.hbp = 20,
				.hfp = 20,
				.vsync = 6,
				.vbp = 20,
				.vfp = 20,
#endif
#else			
				.hsync = 60,//10,//60,160
				.hbp = 10,//10,
				.hfp = 20,//20,
				.vsync = 16,//16,//16
				.vbp = 20,//50
				.vfp = 20,//50
#endif
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
				.clk_tprepare = 3, //prepare time
			},
			.teinfo = {
				.te_source = 1, //external signal ?
				.te_trigger_mode = 0,
				.te_en = 0,
				.te_sync_en = 0,
				.te_cps = 0, // te count per second
			},
			.ext_info = {// eotp 
				.eotp_tx_en = 0,
				.eotp_rx_en = 0,
			},
		},
	},
	.cmds_init = {ARRAY_AND_SIZE(lcd_cmds_init)},
	.cmds_suspend= {ARRAY_AND_SIZE(lcd_cmds_suspend)},
	.cmds_resume = {ARRAY_AND_SIZE(lcd_cmds_resume)},
	.reset = lcd_ws_l706_mipi_ek79007_ips_reset,
	.suspend = lcd_ws_l706_mipi_ek79007_ips_suspend,
	.resume = lcd_ws_l706_mipi_ek79007_ips_resume,
	.backlight_info = {ARRAY_AND_SIZE(backlight_cmds),
				.brightness_bit = 8,},
};

static int __init lcd_ws_l706_mipi_ek79007_ips_init(void)
{
	return comipfb_dev_register(&lcd_ws_l706_mipi_ek79007_ips_dev);
}

subsys_initcall(lcd_ws_l706_mipi_ek79007_ips_init);
