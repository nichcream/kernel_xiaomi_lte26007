#include "comipfb.h"
#include "comipfb_dev.h"
#include "mipi_cmd.h"
#include "mipi_interface.h"

static u8 backlight_cmds[][ROW_LINE] = {
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0x51, 0xFF},
};

static u8 lcd_cmds_init[][ROW_LINE] = {
	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XFF, 0XAA, 0X55, 0X25, 0X01},
	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XFF, 0XAA, 0X55, 0X25, 0X01},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0X6F,0X14,},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XF2,0XFF,},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0X6F,0X15,},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XF2,0X01,},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0X6F,0X16,},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XF2,0Xff,},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0X6F,0X0D,},
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XF3,0XC0,},

	//=========================================================
	//  Main LDI Power ON Sequence
	//=========================================================
	// page=0
	{0x00, DCS_CMD, LW_PACK, 0x08, 0x06, 0x00, 0XF0, 0X55, 0XAA, 0X52, 0X08, 0X00},
	{0x00, DCS_CMD, LW_PACK, 0x08, 0x06, 0x00, 0XF0, 0X55, 0XAA, 0X52, 0X08, 0X00},

	//============================================
	//Display optional control
	//SET_GEN(4,
	//,0XB1,0X7C,0X00,0X00,

	//EQ Control Function for Gate Signals
	//SET_GEN(3,
	//,0XB7,0X72,0X72,

	//EQ Control Function for Source Driver , 2us
	//SET_GEN(5,
	//,0XB8,0X01,0X04,0X04,0X04,

	//Source Driver Control,GOP=3 SOP=5
	//SET_GEN(4,
	//,0XBB,0X53,0X03,0X53,
	//=======================================
	//  Inversion
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0XB1,0X60,0X00,0X00},
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0XBC,0X00,0X00,0X00},
	#ifdef CONFIG_FB_COMIP_ESD
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB3,0X00},
	#else
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB3,0X80},
	#endif

	// set frame rate 60Hz
	//mipi.write ,0X39,0XBD,0X01,0X38,
	//,0X08,0X40,0X01,

	//Display Control
	//Normal Quad Mode
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XC7,0X70,},

	{0x00, DCS_CMD, LW_PACK, 0x0e, 0x0c, 0x00, 0XCA,0X01,0XE4,0XE4,
						0XE4,0XE4,0XE4,0XE4,0X08,
						0X08,0X00,0X00},
	//=====================================================
	//=====================================================
	//page1
	{0x00, DCS_CMD, LW_PACK, 0x08, 0x06, 0x00, 0XF0,0X55,0XAA,0X52,
					0X08,0X01},
	{0x00, DCS_CMD, LW_PACK, 0x08, 0x06, 0x00, 0XF0,0X55,0XAA,0X52,
					0X08,0X01},

	//AVDD: 5.5V  (2.5)
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB0,0X0A},
	
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB6,0X44},
	
	//AVEE: -5.5V (2.5)
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB1,0X0A},

	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB7,0X34},

	//VCL: -3.5V
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB2,0X02},

	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB8,0X15},
	
	//VGH: 15.0V
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB3,0X10},
	
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB9,0X34},
	
	//VGLX: -10.0V
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XB4,0X06},
	
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XBA,0X24},
	
	// SET VOLTAGE
	//SET_GEN(3,
	//,0X39,0XFF,0XAA,
	//,0X55,0X25,0X01,
	//SET_GEN(3,
	//,0X39,0XFA,0X00,
	//,0X00,0X00,0X00,
	//,0X00,0X00,0X00,
	//,0X0C,0X00,0X00,
	//,0X00,0X03,

	//Set VGMP = 5V / VGSP = 0.4936V
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0XBC,0X00,0X8C,0X02},

	//Set VGMN = -5V / VGSN = -0.4936V
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0XBD,0X00,0X8D,0X03},
	
	//Set DCVCOM=-0.7375V
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0XBE,0X31},

	////////////////////////////////////////////////////////////////////////////////////////////////
	////GAMMA+
	////////////////////////////////////////////////R+
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XD1,0X00,0X00,0X00,
						0X1F,0X00,0X3F,0X00,0X60,
						0X00,0X7D,0X00,0X9F,0X00,
						0XC0,0X01,0X00},

	//   V0        V1        V3        V5        V7        V11       V15       V23

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XD2,0X01,0X2C,0X01,
						0X73,0X01,0XA8,0X02,0X02,
						0X02,0X47,0X02,0X49,0X02,
						0X84,0X02,0XC8},
	//   V31       V47       V63       V95       V127      V128      V160      V192

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XD3,0X02,0XEC,0X03,
						0X1E,0X03,0X3F,0X03,0X6B,
						0X03,0X85,0X03,0XAA,0X03,
						0XC0,0X03,0XD6},

	//   V208     V224       V232     V240      V244      V248      V250       V252

	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XD4,0X03,0XE0,0X03,0XFF},
	//   V254     V255

	////////////////////////////////////////////////G+
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XD5,0X00,0X00,0X00,
						0X1F,0X00,0X3F,0X00,0X60,
						0X00,0X7D,0X00,0X9F,0X00,
						0XC0,0X01,0X00},

	//   V0        V1        V3        V5        V7        V11       V15       V23

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XD6,0X01,0X2C,0X01,
						0X73,0X01,0XA8,0X02,0X02,
						0X02,0X47,0X02,0X49,0X02,
						0X84,0X02,0XC8},
	//   V31       V47       V63       V95       V127      V128      V160      V192

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XD7,0X02,0XEC,0X03,
						0X1E,0X03,0X3F,0X03,0X6B,
						0X03,0X85,0X03,0XAA,0X03,
						0XC0,0X03,0XD6},
	//   V208     V224       V232     V240      V244      V248      V250       V252

	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XD8,0X03,0XE0,0X03,0XFF},
	//   V254     V255
	////////////////////////////////////////////////B+
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XD9,0X00,0X00,0X00,
						0X1F,0X00,0X3F,0X00,0X60,
						0X00,0X7D,0X00,0X9F,0X00,
						0XC0,0X01,0X00},
	//   V0        V1        V3        V5        V7        V11       V15       V23
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XDD,0X01,0X2C,0X01,
						0X73,0X01,0XA8,0X02,0X02,
						0X02,0X47,0X02,0X49,0X02,
						0X84,0X02,0XC8},

	//   V31       V47       V63       V95       V127      V128      V160      V192

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XDE,0X02,0XEC,0X03,
						0X1E,0X03,0X3F,0X03,0X6B,
						0X03,0X85,0X03,0XAA,0X03,
						0XC0,0X03,0XD6},
	//   V208     V224       V232     V240      V244      V248      V250       V252

	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XDF,0X03,0XE0,0X03,0XFF},

	//                      V254     V255
	////////////////////////////////////////////////
	////GAMMA-
	////////////////////////////////////////////////R-
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE0,0X00,0X00,0X00,
						0X1F,0X00,0X3F,0X00,0X60,
						0X00,0X7D,0X00,0X9F,0X00,
						0XC0,0X01,0X00},

	//   V0        V1        V3        V5        V7        V11       V15       V23

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE1,0X01,0X2C,0X01,
						0X73,0X01,0XA8,0X02,0X02,
						0X02,0X47,0X02,0X49,0X02,
						0X84,0X02,0XC8},
	//   V31       V47       V63       V95       V127      V128      V160      V192

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE2,0X02,0XEC,0X03,
						0X1E,0X03,0X3F,0X03,0X6B,
						0X03,0X85,0X03,0XAA,0X03,
						0XC0,0X03,0XD6},
	//   V208     V224       V232     V240      V244      V248      V250       V252
	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XE3,0X03,0XE0,0X03,0XFF},

	//                      V254     V255
	////////////////////////////////////////////////G-
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE4,0X00,0X00,0X00,
						0X1F,0X00,0X3F,0X00,0X60,
						0X00,0X7D,0X00,0X9F,0X00,
						0XC0,0X01,0X00},
	//   V0        V1        V3        V5        V7        V11       V15       V23

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE5,0X01,0X2C,0X01,
						0X73,0X01,0XA8,0X02,0X02,
						0X02,0X47,0X02,0X49,0X02,
						0X84,0X02,0XC8},
	//  V31       V47       V63       V95       V127      V128      V160      V192
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE6,0X02,0XEC,0X03,
						0X1E,0X03,0X3F,0X03,0X6B,
						0X03,0X85,0X03,0XAA,0X03,
						0XC0,0X03,0XD6},
	//  V208     V224       V232     V240      V244      V248      V250       V252

	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XE7,0X03,0XE0,0X03,0XFF},
	//                      V254     V255
	////////////////////////////////////////////////B-
	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE8,0X00,0X00,0X00,
						0X1F,0X00,0X3F,0X00,0X60,
						0X00,0X7D,0X00,0X9F,0X00,
						0XC0,0X01,0X00},
	//  V0        V1        V3        V5        V7        V11       V15       V23

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XE9,0X01,0X2C,0X01,
						0X73,0X01,0XA8,0X02,0X02,
						0X02,0X47,0X02,0X49,0X02,
						0X84,0X02,0XC8},

	//   V31       V47       V63       V95       V127      V128      V160      V192

	{0x00, DCS_CMD, LW_PACK, 0x13, 0x11, 0x00, 0XEA,0X02,0XEC,0X03,
						0X1E,0X03,0X3F,0X03,0X6B,
						0X03,0X85,0X03,0XAA,0X03,
						0XC0,0X03,0XD6},

	//   V208     V224       V232     V240      V244      V248      V250       V252

	{0x00, DCS_CMD, LW_PACK, 0x07, 0x05, 0x00, 0XEB,0X03,0XE0,0X03,0XFF},
	
	//    V254     V255
	////////////////////////////////////////////////

	//	  TE Enable
	{0x00, DCS_CMD, SW_PACK2, 0x02, 0x35, 0x00},


	{0x96, DCS_CMD, SW_PACK2, 0x02, 0x11, 0X00},//SLEEP OUT


	{0x14, DCS_CMD, SW_PACK2, 0x02, 0x29, 0x00},//DISPLAY ON

};

static u8 lcd_cmds_suspend[][ROW_LINE] = {
	{50, DCS_CMD, SHUTDOWN_SW_PACK,  0x00, 0x00},
	{0x64, DCS_CMD, SW_PACK1, 0x01, 0x28},
	{0x78, DCS_CMD, SW_PACK1, 0x01, 0x10},
};

static u8 lcd_cmds_resume[][ROW_LINE] = {
	{0x78, DCS_CMD, SW_PACK1, 0x01, 0x11},
	{0x64, DCS_CMD, SW_PACK1, 0x01, 0x29},
};
static struct common_id_info lcd_common_esd_info[] = {
	{DCS_CMD, {0x55, 0x17}, 2, 0xC5},
};
static int lcd_hs_nt35517_reset(struct comipfb_info *fbi)
{
	int gpio_rst = fbi->pdata->gpio_rst;

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
		mdelay(50);
		gpio_direction_output(gpio_rst, 1);
		mdelay(150);
		gpio_free(gpio_rst);
	}
	return 0;
}

static int lcd_hs_nt35517_suspend(struct comipfb_info *fbi)
{
	struct comipfb_dev_timing_mipi *mipi;
	int gpio_rst = fbi->pdata->gpio_rst;
	int ret = 0;

	mipi = &(fbi->cdev->timing.mipi);

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 1);
	}
	if (!(fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF))
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_suspend);
	msleep(20);
	mipi_dsih_dphy_enable_hs_clk(fbi, 0);

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
		mdelay(50);
	}

	mipi_dsih_dphy_ulps_en(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 0);
	mipi_dsih_dphy_shutdown(fbi, 0);
	
	return ret;
}

static int lcd_hs_nt35517_resume(struct comipfb_info *fbi)
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
	}
	else
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
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

//the REG c181 must set to 0x77 ,max fps is 70HZ
/* 2012-12-19 pclk 31.2M 4-26-24-1-20-21----534*896  65.2fps */
struct comipfb_dev lcd_hs_nt35517_dev = {
	.name = "lcd_hs_nt35517",
	.interface_info = COMIPFB_MIPI_IF,
	.lcd_id = LCD_ID_HS_NT35517,
	.refresh_en = 1,
	.bpp = 32,
	.xres = 540,
	.yres = 960,
	.flags = 0,
	.pclk = 32000000,
	.timing = {
		.mipi = {
			.hs_freq = 65000,		//Kbyte
			.lp_freq = 13000,		//KHZ
			.no_lanes = 2,
			.display_mode = MIPI_VIDEO_MODE,
			.im_pin_val = 1,
			.color_mode = {
				.color_bits = COLOR_CODE_24BIT,
			},
			.videomode_info = {
				.hsync = 6,
				.hbp = 10,
				.hfp = 10,
				.vsync = 6,
				.vbp = 20,
				.vfp = 20,
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
				.te_en = 1,
				.te_sync_en = 0,
				.te_cps = 61, // te count per second
			},
			.ext_info = {
				.eotp_tx_en = 0,
			},
		},
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
	.reset = lcd_hs_nt35517_reset,
	.suspend = lcd_hs_nt35517_suspend,
	.resume = lcd_hs_nt35517_resume,
	.backlight_info = {{ARRAY_AND_SIZE(backlight_cmds)},
		.brightness_bit = 5
	},
};

static int __init lcd_hs_nt35517_init(void)
{
	return comipfb_dev_register(&lcd_hs_nt35517_dev);
}

subsys_initcall(lcd_hs_nt35517_init);
