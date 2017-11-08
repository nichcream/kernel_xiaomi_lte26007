#include "comipfb.h"
#include "comipfb_dev.h"
#include "mipi_cmd.h"
#include "mipi_interface.h"

static u8 lcd_cmds_init[][ROW_LINE] = {

	// SET extc (B9)    delay 20ms
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xB9, 0XFF, 0X83, 0X94},

	// SET B0    delay 20ms
	{0x00, DCS_CMD, LW_PACK, 0X07, 0X05, 0X00, 0XB0, 0x00, 0X00, 0X7D, 0X0C},

	// set BA   DELAY 5ms
	{0x00, DCS_CMD, LW_PACK, 0x10, 0x0E, 0x00, 0xBA, 0X73, 0X83, 0XA8, 0XAD, 0XB6, 0X00, 0X00,\
	0X40, 0X10, 0XFF, 0X0F, 0X00, 0X80},

	// set B1 (POWER)  delay 20ms
	{0x00, DCS_CMD, LW_PACK, 0x12, 0X10, 0x00, 0xB1, 0x64, 0X15, 0X15, 0X34, 0X04, 0X11, 0XF1,\
	0X81, 0X76, 0X54, 0X23, 0X80, 0XC0, 0XD2, 0X5E},
	// SET B2(DISPLAY)
	{0x00, DCS_CMD, LW_PACK, 0x12, 0x10, 0x00, 0XB2, 0X00, 0X64, 0X0E, 0X0D, 0X32, 0X1C, 0X08,\
	0X08, 0X1C, 0X4D, 0X00, 0X00, 0X30, 0X44, 0X24},
	// SET B4(CYC)
	{0x00, DCS_CMD, LW_PACK, 0X19, 0X17, 0X00, 0XB4, 0X00, 0XFF, 0X03, 0X5A, 0X03, 0X5A, 0X03,\
	0X5A, 0X01, 0X70, 0X01, 0X70, 0X03, 0X5A, 0X03,\
	0X5A, 0X03, 0X5A, 0X01, 0X70, 0X1F, 0X70},
	//  SET D3
	{0x00, DCS_CMD, LW_PACK, 0X37, 0X35, 0x00, 0XD3, 0X00, 0X07, 0X00, 0X3C, 0X07, 0X10, 0X00,\
	0X08, 0X10, 0X09, 0X00, 0X09, 0X54, 0X15, 0X0F,\
	0X05, 0X04, 0X02, 0X12, 0X10, 0X05, 0X07, 0X37,\
	0X33, 0X0B, 0X0B, 0X3B, 0X10, 0X07, 0X07, 0X08,\
	0X00, 0X00, 0X00, 0X0A, 0X00, 0X01, 0X00, 0X00,\
	0X00, 0X00, 0X00, 0X00, 0X00, 0X09, 0X05, 0X04,\
	0X02, 0X10, 0X0B, 0X10, 0X00},
	// SET GPI, FORWARD
	{0x00, DCS_CMD, LW_PACK, 0X2F, 0X2D, 0X00, 0XD5, 0X1A, 0X1A, 0X1B, 0X1B, 0X00, 0X01, 0X02,\
	0X03, 0X04, 0X05, 0X06, 0X07, 0X08, 0X09, 0X0A,\
	0X0B, 0X24, 0X25, 0X18, 0X18, 0X26, 0X27, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X20,\
	0X21, 0X18, 0X18, 0X18, 0X18},
	//SET D6 BACKWARD
	{0x00, DCS_CMD, LW_PACK, 0X2F, 0X2D, 0X00, 0XD6, 0X1A, 0X1A, 0X1B, 0X1B, 0X0B, 0X0A, 0X09,\
	0X08, 0X07, 0X06, 0X05, 0X04, 0X03, 0X02, 0X01,\
	0X00, 0X21, 0X20, 0X58, 0X58, 0X27, 0X26, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X25,\
	0X24, 0X18, 0X18, 0X18, 0X18},

	// SET CC
	{0x00, DCS_CMD, SW_PACK2, 0X02, 0XCC, 0X01},

	// SET C7   TCON
	{0x00, DCS_CMD, LW_PACK ,0X07, 0X05, 0X00, 0XC7, 0X00, 0X00, 0X00, 0XC0},

	// SET BANK0 FOR D8
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X00},

	// SET D2
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XD2, 0X66},

	// SET D8
	{0X00, DCS_CMD, LW_PACK, 0X1B, 0X19, 0X00, 0XD8, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XF0, 0XFA,\
	0XAA, 0XAA, 0XAA, 0XAA, 0XA0, 0XAA, 0XAA, 0XAA,\
	0XAA, 0XAA, 0XA0, 0XAA, 0XAA, 0XAA, 0XAA, 0XAA,\
	0XA0},


	// SET BANK1 FOR D8
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X01},

	// SET D8
	{0X00, DCS_CMD, LW_PACK, 0X27, 0X25, 0X00, 0XD8, 0XAA, 0XAA, 0XAA, 0XAA, 0XAA, 0XA0, 0XAA,\
	0XAA, 0XAA, 0XAA, 0XAA, 0XA0, 0XFF, 0XFF, 0XFF,\
	0XFF, 0XFF, 0XF0, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,\
	0XF0, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,\
	0X00, 0X00, 0X00, 0X00, 0X00},

	// SET BANK2 FOR D8
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X02},

	// SET D8
	{0x00, DCS_CMD, LW_PACK, 0X0F, 0X0D, 0X00, 0XD8, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XF0, 0XFF,\
	0XFF, 0XFF, 0XFF, 0XFF, 0XF0},

	// BANK0
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X00},

	// SET BE
	{0x00, DCS_CMD, LW_PACK, 0X05, 0X03, 0X00, 0XBE, 0X01, 0XE5},

	// SET BF
	{0x00, DCS_CMD, LW_PACK, 0X06, 0X04, 0X00, 0XBF, 0X41, 0X0E, 0X01},

	// SET D9
	{0x00, DCS_CMD, LW_PACK, 0X06, 0X04, 0X00, 0XD9, 0X04, 0X01, 0X02},

	// SLEEP OUT  PacketHeader[0x05 0x11 0x00],than: delay 200ms
	{0XC8, DCS_CMD, SW_PACK1, 0X01, 0X11},

	// Display ON   packet header  [0x05 0x11 0x00 xx]  -- cmd with no data
	{0x14, DCS_CMD, SW_PACK1, 0x01, 0x29},

};

static u8 lcd_cmds_suspend[][ROW_LINE] = {
	{0x64, DCS_CMD, SW_PACK1, 0x01, 0x28},
	{0xff, DCS_CMD, SW_PACK1, 0x01, 0x10},
};

static u8 lcd_cmds_resume[][ROW_LINE] = {

	// SET extc (B9)    delay 20ms
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xB9, 0XFF, 0X83, 0X94},

	// SET B0    delay 20ms
	{0x00, DCS_CMD, LW_PACK, 0X07, 0X05, 0X00, 0XB0, 0x00, 0X00, 0X7D, 0X0C},

	// set BA   DELAY 5ms
	{0x00, DCS_CMD, LW_PACK, 0x10, 0x0E, 0x00, 0xBA, 0X73, 0X83, 0XA8, 0XAD, 0XB6, 0X00, 0X00,\
	0X40, 0X10, 0XFF, 0X0F, 0X00, 0X80},

	// set B1 (POWER)  delay 20ms
	{0x00, DCS_CMD, LW_PACK, 0x12, 0X10, 0x00, 0xB1, 0x64, 0X15, 0X15, 0X34, 0X04, 0X11, 0XF1,\
	0X81, 0X76, 0X54, 0X23, 0X80, 0XC0, 0XD2, 0X5E},
	// SET B2(DISPLAY)
	{0x00, DCS_CMD, LW_PACK, 0x12, 0x10, 0x00, 0XB2, 0X00, 0X64, 0X0E, 0X0D, 0X32, 0X1C, 0X08,\
	0X08, 0X1C, 0X4D, 0X00, 0X00, 0X30, 0X44, 0X24},
	// SET B4(CYC)
	{0x00, DCS_CMD, LW_PACK, 0X19, 0X17, 0X00, 0XB4, 0X00, 0XFF, 0X03, 0X5A, 0X03, 0X5A, 0X03,\
	0X5A, 0X01, 0X70, 0X01, 0X70, 0X03, 0X5A, 0X03,\
	0X5A, 0X03, 0X5A, 0X01, 0X70, 0X1F, 0X70},
	//  SET D3
	{0x00, DCS_CMD, LW_PACK, 0X37, 0X35, 0x00, 0XD3, 0X00, 0X07, 0X00, 0X3C, 0X07, 0X10, 0X00,\
	0X08, 0X10, 0X09, 0X00, 0X09, 0X54, 0X15, 0X0F,\
	0X05, 0X04, 0X02, 0X12, 0X10, 0X05, 0X07, 0X37,\
	0X33, 0X0B, 0X0B, 0X3B, 0X10, 0X07, 0X07, 0X08,\
	0X00, 0X00, 0X00, 0X0A, 0X00, 0X01, 0X00, 0X00,\
	0X00, 0X00, 0X00, 0X00, 0X00, 0X09, 0X05, 0X04,\
	0X02, 0X10, 0X0B, 0X10, 0X00},
	// SET GPI, FORWARD
	{0x00, DCS_CMD, LW_PACK, 0X2F, 0X2D, 0X00, 0XD5, 0X1A, 0X1A, 0X1B, 0X1B, 0X00, 0X01, 0X02,\
	0X03, 0X04, 0X05, 0X06, 0X07, 0X08, 0X09, 0X0A,\
	0X0B, 0X24, 0X25, 0X18, 0X18, 0X26, 0X27, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X20,\
	0X21, 0X18, 0X18, 0X18, 0X18},
	//SET D6 BACKWARD
	{0x00, DCS_CMD, LW_PACK, 0X2F, 0X2D, 0X00, 0XD6, 0X1A, 0X1A, 0X1B, 0X1B, 0X0B, 0X0A, 0X09,\
	0X08, 0X07, 0X06, 0X05, 0X04, 0X03, 0X02, 0X01,\
	0X00, 0X21, 0X20, 0X58, 0X58, 0X27, 0X26, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18,\
	0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X18, 0X25,\
	0X24, 0X18, 0X18, 0X18, 0X18},

	// SET CC
	{0x00, DCS_CMD, SW_PACK2, 0X02, 0XCC, 0X01},

	// SET C7   TCON
	{0x00, DCS_CMD, LW_PACK ,0X07, 0X05, 0X00, 0XC7, 0X00, 0X00, 0X00, 0XC0},

	// SET BANK0 FOR D8
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X00},

	// SET D2
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XD2, 0X66},

	// SET D8
	{0X00, DCS_CMD, LW_PACK, 0X1B, 0X19, 0X00, 0XD8, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XF0, 0XFA,\
	0XAA, 0XAA, 0XAA, 0XAA, 0XA0, 0XAA, 0XAA, 0XAA,\
	0XAA, 0XAA, 0XA0, 0XAA, 0XAA, 0XAA, 0XAA, 0XAA,\
	0XA0},


	// SET BANK1 FOR D8
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X01},

	// SET D8
	{0X00, DCS_CMD, LW_PACK, 0X27, 0X25, 0X00, 0XD8, 0XAA, 0XAA, 0XAA, 0XAA, 0XAA, 0XA0, 0XAA,\
	0XAA, 0XAA, 0XAA, 0XAA, 0XA0, 0XFF, 0XFF, 0XFF,\
	0XFF, 0XFF, 0XF0, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,\
	0XF0, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,\
	0X00, 0X00, 0X00, 0X00, 0X00},

	// SET BANK2 FOR D8
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X02},

	// SET D8
	{0x00, DCS_CMD, LW_PACK, 0X0F, 0X0D, 0X00, 0XD8, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XF0, 0XFF,\
	0XFF, 0XFF, 0XFF, 0XFF, 0XF0},

	// BANK0
	{0X00, DCS_CMD, SW_PACK2, 0X02, 0XBD, 0X00},

	// SET BE
	{0x00, DCS_CMD, LW_PACK, 0X05, 0X03, 0X00, 0XBE, 0X01, 0XE5},

	// SET BF
	{0x00, DCS_CMD, LW_PACK, 0X06, 0X04, 0X00, 0XBF, 0X41, 0X0E, 0X01},

	// SET D9
	{0x00, DCS_CMD, LW_PACK, 0X06, 0X04, 0X00, 0XD9, 0X04, 0X01, 0X02},

	// SLEEP OUT  PacketHeader[0x05 0x11 0x00],than: delay 200ms
	{0XC8, DCS_CMD, SW_PACK1, 0X01, 0X11},

	// Display ON   packet header  [0x05 0x11 0x00 xx]  -- cmd with no data
	{0x14, DCS_CMD, SW_PACK1, 0x01, 0x29},

};

static u8 lcd_pre_read_id[][ROW_LINE] = {
	{0x00, DCS_CMD, LW_PACK, 0x06, 0x04, 0x00, 0xB9, 0xFF, 0x83, 0x94},
	{0x00, DCS_CMD, LW_PACK, 0x10, 0x0E, 0x00, 0xBA, 0x33, 0x83, 0xA8, 0xAD, 0xB6, 0x00, 0x00, 0x40, 0x10, 0xFF, 0x0F, 0x00, 0x80},
};
static struct common_id_info lcd_common_id_info[] = {
	{DCS_CMD, {0x83, 0x94, 0x0D}, 3, 0x04},
};
static struct common_id_info lcd_common_esd_info[] = {
	{DCS_CMD, {0x1C}, 1, 0x0A},
};

static int lcd_inx_h8394d_power(struct comipfb_info *fbi, int onoff)
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
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_ENABLE);
		mdelay(50);
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
		mdelay(180);
	} else {
		gpio_direction_output(gpio_rst, 0);
		pmic_voltage_set(PMIC_POWER_LCD_CORE, 0, PMIC_POWER_VOLTAGE_DISABLE);
		pmic_voltage_set(PMIC_POWER_LCD_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
		mdelay(10);
	}

	gpio_free(gpio_rst);

	return 0;
}

static int lcd_inx_h8394d_reset(struct comipfb_info *fbi)
{
	int gpio_rst = fbi->pdata->gpio_rst;

	if (gpio_rst >= 0) {
		gpio_request(gpio_rst, "LCD Reset");
		gpio_direction_output(gpio_rst, 1);
		mdelay(10);
		gpio_direction_output(gpio_rst, 0);
		mdelay(10);
		gpio_direction_output(gpio_rst, 1);
		mdelay(180);
		gpio_free(gpio_rst);
	}
	return 0;
}

static int lcd_inx_h8394d_suspend(struct comipfb_info *fbi)
{
	int ret=0;
	struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 1);
	}

	comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_suspend);

	mdelay(20);
	mipi_dsih_dphy_enable_hs_clk(fbi, 0);

	mipi_dsih_dphy_reset(fbi, 0);
	mipi_dsih_dphy_shutdown(fbi, 0);

	return ret;
}

static int lcd_inx_h8394d_resume(struct comipfb_info *fbi)
{
	int ret=0;
	struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);

	mipi_dsih_dphy_shutdown(fbi, 1);
	mipi_dsih_dphy_reset(fbi, 1);

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

struct comipfb_dev lcd_inx_h8394d_dev = {
	.name = "lcd_inx_h8394d",
	.interface_info = COMIPFB_MIPI_IF,
	.lcd_id = LCD_ID_INX_H8394D,
	.refresh_en = 1,
	.bpp = 32,
	.xres = 720,
	.yres = 1280,
	.flags = 0,
	.pclk = 65000000,
	.timing = {
		.mipi = {
			.hs_freq = 52000,
			.lp_freq = 13000,		//KHZ
			.no_lanes = 4,
			.display_mode = MIPI_VIDEO_MODE,
			.im_pin_val = 1,
			.color_mode = {
				.color_bits = COLOR_CODE_24BIT,
			},
			.videomode_info = {
				.hsync = 16,
				.hbp = 90,
				.hfp = 90,
				.vsync = 4,
				.vbp = 12,
				.vfp = 15,
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
				.eotp_tx_en = 0,
			},
		},
	},
	.panel_id_info = {
		.id_info = lcd_common_id_info,
		.num_id_info = 1,
		.prepare_cmd = {ARRAY_AND_SIZE(lcd_pre_read_id)},
	},
	.cmds_init = {ARRAY_AND_SIZE(lcd_cmds_init)},
	.cmds_suspend = {ARRAY_AND_SIZE(lcd_cmds_suspend)},
	.cmds_resume = {ARRAY_AND_SIZE(lcd_cmds_resume)},
	.power = lcd_inx_h8394d_power,
	.reset = lcd_inx_h8394d_reset,
	.suspend = lcd_inx_h8394d_suspend,
	.resume = lcd_inx_h8394d_resume,
#ifdef CONFIG_FB_COMIP_ESD
	.esd_id_info = {
		.id_info = lcd_common_esd_info,
		.num_id_info = 1,
	},
#endif
};

static int __init lcd_inx_h8394d_init(void)
{
	return comipfb_dev_register(&lcd_inx_h8394d_dev);
}

subsys_initcall(lcd_inx_h8394d_init);
