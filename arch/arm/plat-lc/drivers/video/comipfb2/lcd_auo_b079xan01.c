#include "comipfb.h"
#include "comipfb_dev.h"
#include "mipi_cmd.h"
#include "mipi_interface.h"

static u8 backlight_cmds[][ROW_LINE] = {
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0x51, 0xBE},
//	{0x00, GEN_CMD, LW_PACK, 0x07, 0x05, 0x00, 0xBB, 0x7B, 0xFF, 0x0F, 0x02, 0x01, 0x00},
};

//[0]: delay after transfer; [1]:count of data; [2]: word count ls; [3]:word count ms; [4]...: data for transfering
static u8 lcd_cmds_init0[][ROW_LINE] = {
/****Start Initial Sequence ***/
	{0x0a, DCS_CMD, SW_PACK1, 0x01, 0x11},
};
static u8 lcd_cmds_init1[][ROW_LINE] = {
/****Start Initial Sequence ***/
	{0x0a, DCS_CMD, SW_PACK1, 0x01, 0x29},		//dcs short no param
};

static u8 lcd_cmds_suspend0[][ROW_LINE] = {
	{0x64, DCS_CMD, SW_PACK1, 0x01, 0x28},
};
static u8 lcd_cmds_suspend1[][ROW_LINE] = {
	{0xff, DCS_CMD, SW_PACK1, 0x01, 0x10},	//TODO delay is 300ms.
};

static u8 lcd_cmds_resume[][ROW_LINE] = {
	{0xff, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0xff, DCS_CMD, SW_PACK1, 0x01, 0x29},
};

static int lcd_auo_b079xan01_reset(struct comipfb_info *fbi)
{
	int gpio_rst = fbi->pdata->gpio_rst;

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		mdelay(60);//T2 time
		gpio_direction_output(gpio_rst, 0);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
	        mdelay(60);
		gpio_direction_output(gpio_rst, 0);
		mdelay(20);
		gpio_free(gpio_rst);
	}
	return 0;
}

static int lcd_auo_b079xan01_suspend(struct comipfb_info *fbi)
{
	int ret=0;
	
        struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 1);
	}
	mipi_dsih_dphy_enable_hs_clk(fbi, 0);
	msleep(100);
	if (fbi->cdev->reset)
		fbi->cdev->reset(fbi);

	mipi_dsih_dphy_ulps_en(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 0);
	mipi_dsih_dphy_shutdown(fbi, 0);
  
	return ret;
}

static int lcd_auo_b079xan01_resume(struct comipfb_info *fbi)
{
	int ret=0;
	struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);

	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);
	mipi_dsih_dphy_ulps_en(fbi, 0);

	if (fbi->cdev->reset)
    		fbi->cdev->reset(fbi);

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 0);
	}
	mipi_dsih_dphy_enable_hs_clk(fbi, 1);

	return ret;
}

struct comipfb_dev lcd_auo_b079xan01_dev = {
	.name = "lcd_auo_b079xan01",
	.interface_info = COMIPFB_MIPI_IF,
	.lcd_id = LCD_ID_AUO_B079XAN01,
	.refresh_en = 1,//mipi =1; RGB = 0
	.bpp = 32,
	.xres = 1280,
	.yres = 800,
	.flags = 0,
	.pclk = 52000000,//30M,62
	.timing = {
		.mipi = {
			.hs_freq = 50000,		//Kbyte
			.lp_freq = 10000,		//KHZ,for read id BTA
			.no_lanes = 4,
			.display_mode = MIPI_VIDEO_MODE,
			.im_pin_val = 1,		//im pin for mipi and lvds for standby
			.color_mode = {
				.color_bits = COLOR_CODE_24BIT,
			},
			.videomode_info = {
		
				.hsync = 20,//10,//60,160
				.hbp = 100,//100
				.hfp = 80,//2//80
				.vsync = 3,//16,//16//20
				.vbp = 12,
				.vfp = 12,
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
	#if 0
	.cmds_init0 = {ARRAY_AND_SIZE(lcd_cmds_init0)},
	.cmds_init1 = {ARRAY_AND_SIZE(lcd_cmds_init1)},
	.cmds_suspend0 = {ARRAY_AND_SIZE(lcd_cmds_suspend0)},
	.cmds_suspend1 = {ARRAY_AND_SIZE(lcd_cmds_suspend1)},
	.cmds_resume = {ARRAY_AND_SIZE(lcd_cmds_resume)},
	.reset = lcd_auo_b079xan01_reset,
	.backlight_info = {ARRAY_AND_SIZE(backlight_cmds),
				.brightness_bit = 8,},
	#endif
	.suspend = lcd_auo_b079xan01_suspend,
	.resume = lcd_auo_b079xan01_resume,
};

static int __init lcd_auo_b079xan01_init(void)
{
	return comipfb_dev_register(&lcd_auo_b079xan01_dev);
}

subsys_initcall(lcd_auo_b079xan01_init);
