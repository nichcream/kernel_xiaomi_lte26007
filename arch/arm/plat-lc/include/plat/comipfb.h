/*
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2009-2018	LeadCoreTech Corp.
**
**	PURPOSE:			This files contains the driver of LCD backlight controller.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	0.1.0		2012-03-09	liuyong		created
**
*/

#ifndef __ASM_ARCH_PLAT_FB_H
#define __ASM_ARCH_PLAT_FB_H

#include <linux/types.h>
#include <linux/fb.h>

#define LCM_ID_TM 0
#define LCM_ID_DJN 1
#define LCM_ID_TCL 2

#define LCM_DJN_ADC_VALUE_MIN  1200	//0.73v
#define LCM_DJN_ADC_VALUE_MAX 1700	// 1.03v
#define LCM_TCL_ADC_VALUE_MIN  2000	// 1.22v
#define LCM_TCL_ADC_VALUE_MAX  2500	// 1.52v

#define COMIPFB_RGB_IF                  (0x00000002)
#define COMIPFB_MIPI_IF                 (0x00000003)
#define COMIPFB_REFRESH_THREAD				(0x00000001)
#define COMIPFB_REFRESH_SINGLE				(0x00000002)
#define COMIPFB_CACHED_BUFFER				(0x00000004)
#define COMIPFB_SLEEP_POWEROFF				(0x00010000)

#define COMIPFB_REFRESH_SOFTWARE			\
	(COMIPFB_REFRESH_THREAD | COMIPFB_REFRESH_SINGLE)

enum lcd_id {
	LCD_ID_HS_RM68190 = 1,
	LCD_ID_HS_NT35517,
	LCD_ID_TRULY_NT35595,
	LCD_ID_BT_ILI9806E,
	LCD_ID_DJN_ILI9806E,
	LCD_ID_SHARP_R69431,
	LCD_ID_AUO_NT35521,
	LCD_ID_BOE_NT35521S,
	LCD_ID_INX_H8394D,
	LCD_ID_INX_H8392B,
	LCD_ID_AUO_H8399A,
	LCD_ID_IVO_OTM1283A,
	LCD_ID_TRULY_H8394A,
	LCD_ID_STARRY_S6D7A,
	LCD_ID_AUO_B079XAN01,
	LCD_ID_JD_9161CPT5,
	LCD_ID_JD_9261AA,
	LCD_ID_JD_9365HSD,
	LCD_ID_AUO_R61308OTP,
	LCD_ID_AUO_OTM1285A_OTP,
	LCD_ID_HT_H8394D,
	LCD_ID_HS_EK79007,
	LCD_ID_LVDS_1280X800,
	LCD_ID_FL10802,
};

struct comipfb_spi_info {
	int gpio_cs;
	int gpio_clk;
	int gpio_txd;
	int gpio_rxd;
};

#if defined(CONFIG_FB_COMIP2)
struct comipfb_platform_data {
	int lcdc_support_interface; //lcdc support interface.
	int phy_ref_freq;
	int lcdcaxi_clk;
	int gpio_rst;
	int gpio_im;
	int flags;
	int (*power)(int onoff);
	int (*detect_dev)(void);
	void (*bl_control)(int onoff);
};
extern int is_lcm_avail(void);
#elif defined(CONFIG_FB_COMIP)
struct comipfb_platform_data {
	struct comipfb_spi_info spi_info;
	int lcdc_id;	//controllor and (screen or other dev) config id.
	int main_lcdc_flag; //main controllor flag.  default  0.
	int lcdc_support_interface; //lcdc support interface.
	int lcdcaxi_clk;
	int gpio_rst;
	int gpio_im;
	int flags;
	int (*power)(int onoff);
	const char* (*detect_dev)(void);
	void (*bl_control)(int onoff);
};
#else
struct comipfb_platform_data {
	int flags;
};
#endif

#endif /* __ASM_ARCH_PLAT_FB_H */
