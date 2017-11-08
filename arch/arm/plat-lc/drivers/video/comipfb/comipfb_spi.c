/* driver/video/comip/comipfb_spi.c
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 1999-2015      LeadCoreTech Corp.
**
**      PURPOSE:                        This files contains the driver of LCD controller.
**
**      CHANGE HISTORY:
**
**      Version         Date            Author          Description
**      0.1.0           2012-03-10      liuyong         created
**
*/




#include <linux/delay.h>
#include <plat/comipfb.h>

#include "comipfb_spi.h"

#define CLK_DELAY()		udelay(1)

static int spi_gpio_cs = -1;
static int spi_gpio_clk = -1;
static int spi_gpio_txd = -1;
static int spi_gpio_rxd = -1;

static void comipfb_spi_cs(int val)
{
	gpio_direction_output(spi_gpio_cs, val);
}

static void comipfb_spi_clk(int val)
{
	gpio_direction_output(spi_gpio_clk, val);
}

static void comipfb_spi_txd(int val)
{
	gpio_direction_output(spi_gpio_txd, val);
}

static void comipfb_spi_rxd(int val)
{
	if (spi_gpio_rxd >= 0)
		gpio_direction_output(spi_gpio_rxd, val);
}

int comipfb_spi_init(struct comipfb_info *fbi)
{
	int ret;
	struct comipfb_spi_info *info = &fbi->pdata->spi_info;

	spi_gpio_cs = info->gpio_cs;
	spi_gpio_clk = info->gpio_clk;
	spi_gpio_txd = info->gpio_txd;
	spi_gpio_rxd = info->gpio_rxd;

	ret = gpio_request(spi_gpio_cs,"comipfb_spics_pin");
	if(ret < 0) {
		printk("\nCan't request comipfb reset gpio\n");
		goto err;
	}

	ret = gpio_request(spi_gpio_clk,"comipfb_spi_clk_pin");
	if(ret < 0) {
		printk("\nCan't request comipfb reset gpio\n");
		goto err;
	}

	ret = gpio_request(spi_gpio_txd,"comipfb_spi_txd_pin");
	if(ret < 0) {
		printk("\nCan't request comipfb reset gpio\n");
		goto err;
	}

	if (spi_gpio_rxd >= 0) {
		ret = gpio_request(spi_gpio_rxd,"comipfb_spi_rxd_pin");
		if(ret < 0) {
			printk("\nCan't request comipfb reset gpio\n");
			goto err;
		}
	}

	comipfb_spi_cs(1);
	comipfb_spi_clk(0);
	comipfb_spi_txd(0);
	comipfb_spi_rxd(0);

	return 0;

err:
	return ret;
}
EXPORT_SYMBOL(comipfb_spi_init);

int comipfb_spi_exit(void)
{
	gpio_free(spi_gpio_cs);
	gpio_free(spi_gpio_clk);
	gpio_free(spi_gpio_txd);

	if (spi_gpio_rxd >= 0)
		gpio_free(spi_gpio_rxd);

	return 0;
}
EXPORT_SYMBOL(comipfb_spi_exit);

int comipfb_spi_write_cmd(u8 cmd)
{
	u8 i;

	comipfb_spi_cs(0);
	comipfb_spi_txd(0);
	comipfb_spi_clk(0);
	CLK_DELAY();
	comipfb_spi_clk(1);
	CLK_DELAY();

	for (i = 0; i < 8; i++) {
		comipfb_spi_txd(cmd & 0x80);
		comipfb_spi_clk(0);
		CLK_DELAY();
		comipfb_spi_clk(1);
		CLK_DELAY();
		cmd <<= 1;
	}

	comipfb_spi_cs(1);
	comipfb_spi_clk(0);

	return 0;
}
EXPORT_SYMBOL(comipfb_spi_write_cmd);

int comipfb_spi_write_cmd2(u16 cmd)
{
	u8 i;

	comipfb_spi_cs(0);

	for (i = 0; i < 16; i++) {
		comipfb_spi_txd(cmd & 0x8000);
		comipfb_spi_clk(0);
		CLK_DELAY();
		comipfb_spi_clk(1);
		CLK_DELAY();
		cmd <<= 1;
	}

	comipfb_spi_cs(1);
	comipfb_spi_clk(0);

	return 0;
}
EXPORT_SYMBOL(comipfb_spi_write_cmd2);

int comipfb_spi_write_para(u8 para)
{
	u8 i;

	comipfb_spi_cs(0);
	comipfb_spi_txd(1);
	comipfb_spi_clk(0);
	CLK_DELAY();
	comipfb_spi_clk(1);
	CLK_DELAY();

	for (i = 0; i < 8; i++) {
		comipfb_spi_txd(para & 0x80);
		comipfb_spi_clk(0);
		CLK_DELAY();
		comipfb_spi_clk(1);
		CLK_DELAY();
		para <<= 1;
	}

	comipfb_spi_cs(1);
	comipfb_spi_clk(0);

	return 0;
}
EXPORT_SYMBOL(comipfb_spi_write_para);

int comipfb_spi_suspend(void)
{
	comipfb_spi_cs(0);
	comipfb_spi_clk(0);
	comipfb_spi_txd(0);
	comipfb_spi_rxd(0);

	return 0;
}
EXPORT_SYMBOL(comipfb_spi_suspend);

int comipfb_spi_resume(void)
{
	comipfb_spi_cs(1);

	return 0;
}
EXPORT_SYMBOL(comipfb_spi_resume);

