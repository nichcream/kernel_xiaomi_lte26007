/* driver/video/comip/comipfb_if.c
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
#include <linux/compiler.h>
#include <plat/mfp.h>
#include "comipfb.h"
#include "comipfb_hw.h"
#include "comipfb_if.h"
#include "comipfb_spi.h"
#include "mipi_cmd.h"

/*
 * MPU interface
 */
static struct mfp_pin_cfg comipfb_if_mpu_mfp_cfg[] = {

};

static void comipfb_if_mpu_pin_init(void)
{
	/* Sel mux pins as LCDC MPU func. */
//	comip_mfp_config_array(ARRAY_AND_SIZE(comipfb_if_mpu_mfp_cfg));
}

static int comipfb_if_mpu_dev_cmds(struct comipfb_info *fbi, struct comipfb_dev_cmds *cmds)
{
	unsigned int i;
	unsigned int type;
	unsigned int cmd;

	if (!cmds)
		return 0;

	if (!cmds->n_cmds || !cmds->cmds)
		return 0;

	for (i = 0; i < cmds->n_cmds; i++) {
		type = cmds->cmds[i] >> 24;
		cmd = cmds->cmds[i] & 0x00ffffff;
		if (CMD_CMD == type) {
			lcdc_cmd(fbi, cmd);
		} else if (CMD_PARA == type)
			lcdc_cmd_para(fbi, cmd);
		else if (CMD_DELAY == type)
			mdelay(cmd);
	}

	return 0;
}

static int comipfb_if_mpu_init(struct comipfb_info *fbi)
{
	comipfb_if_mpu_pin_init();

	/* Reset device. */
	if (fbi->cdev->reset)
		fbi->cdev->reset(fbi);

	return comipfb_if_mpu_dev_cmds(fbi, &fbi->cdev->cmds_init);
}

static int comipfb_if_mpu_exit(struct comipfb_info *fbi)
{
	return 0;
}

static int comipfb_if_mpu_suspend(struct comipfb_info *fbi)
{
	int ret = 0;

	if (!(fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF)) {
		ret = comipfb_if_mpu_dev_cmds(fbi, &fbi->cdev->cmds_suspend);

		if (!ret && fbi->cdev->suspend)
			fbi->cdev->suspend(fbi);
	}

	return ret;
}

static int comipfb_if_mpu_resume(struct comipfb_info *fbi)
{
	int ret = 0;

	if (fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF) {
		/* Reset device. */
		if (fbi->cdev->reset)
			fbi->cdev->reset(fbi);

		ret = comipfb_if_mpu_dev_cmds(fbi, &fbi->cdev->cmds_init);
	} else {
		if (fbi->cdev->resume)
			fbi->cdev->resume(fbi);

		comipfb_if_mpu_dev_cmds(fbi, &fbi->cdev->cmds_resume);
	}

	return ret;
}

static struct comipfb_if comipfb_if_mpu = {
	.init		= comipfb_if_mpu_init,
	.exit		= comipfb_if_mpu_exit,
	.suspend	= comipfb_if_mpu_suspend,
	.resume		= comipfb_if_mpu_resume,
	.dev_cmd	= comipfb_if_mpu_dev_cmds,
};

/*
 * RGB interface
 */
static struct mfp_pin_cfg comipfb_if_rgb_mfp_cfg[] = {

};

static void comipfb_if_rgb_pin_init(void)
{
	/* Sel mux pin as LCDC RGB func. */
//	comip_mfp_config_array(ARRAY_AND_SIZE(comipfb_if_rgb_mfp_cfg));
}

static int comipfb_if_rgb_dev_cmds(struct comipfb_info *fbi, struct comipfb_dev_cmds *cmds)
{
	unsigned int i;
	unsigned int type;
	unsigned int cmd;

	if (!cmds)
		return 0;

	if (!cmds->n_cmds || !cmds->cmds)
		return 0;

	for (i = 0; i < cmds->n_cmds; i++) {
		type = cmds->cmds[i] >> 24;
		cmd = cmds->cmds[i] & 0x00ffffff;
		if (CMD_CMD == type)
			comipfb_spi_write_cmd(cmd);
		else if (CMD_CMD2 == type)
			comipfb_spi_write_cmd2(cmd);
		else if (CMD_PARA == type)
			comipfb_spi_write_para(cmd);
		else if (CMD_DELAY == type)
			mdelay(cmd);
	}

	return 0;
}

static int comipfb_if_rgb_init(struct comipfb_info *fbi)
{
	comipfb_spi_init(fbi);
	comipfb_if_rgb_pin_init();

	/* Reset device. */
	if (fbi->cdev->reset)
		fbi->cdev->reset(fbi);

	return comipfb_if_rgb_dev_cmds(fbi, &fbi->cdev->cmds_init);
}

static int comipfb_if_rgb_exit(struct comipfb_info *fbi)
{
	return comipfb_spi_exit();
}

static int comipfb_if_rgb_suspend(struct comipfb_info *fbi)
{
	int ret = 0;

	if (!(fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF)) {
		ret = comipfb_if_rgb_dev_cmds(fbi, &fbi->cdev->cmds_suspend);

		if (!ret && fbi->cdev->suspend)
			fbi->cdev->suspend(fbi);
	}

	comipfb_spi_suspend();

	return ret;
}

static int comipfb_if_rgb_resume(struct comipfb_info *fbi)
{
	int ret = 0;

	comipfb_spi_resume();

	if (fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF) {
		/* Reset device. */
		if (fbi->cdev->reset)
			fbi->cdev->reset(fbi);

		ret = comipfb_if_rgb_dev_cmds(fbi, &fbi->cdev->cmds_init);
	} else {
		if (fbi->cdev->resume)
			fbi->cdev->resume(fbi);

		ret = comipfb_if_rgb_dev_cmds(fbi, &fbi->cdev->cmds_resume);
	}

	return ret;
}

static struct comipfb_if comipfb_if_rgb = {
	.init		= comipfb_if_rgb_init,
	.exit		= comipfb_if_rgb_exit,
	.suspend	= comipfb_if_rgb_suspend,
	.resume		= comipfb_if_rgb_resume,
	.dev_cmd	= comipfb_if_rgb_dev_cmds,
};

static int comipfb_mipi_mode_change(struct comipfb_info *fbi)
{
	int gpio_im = fbi->pdata->gpio_im;

	if (gpio_im >= 0) {
		gpio_request(gpio_im, "LCD IM");
		gpio_direction_output(gpio_im, 1);
	}

	return 0;
}

/*
 *
 * MIPI interface
 */
static struct mfp_pin_cfg comipfb_if_mipi_mfp_cfg[] = {
	{MFP_PIN_GPIO(105), MFP_PIN_MODE_GPIO},
	{MFP_PIN_GPIO(109), MFP_PIN_MODE_GPIO},
};

static void comipfb_if_mipi_pin_init(void)
{
	comip_mfp_config_array(comipfb_if_mipi_mfp_cfg, ARRAY_SIZE(comipfb_if_mipi_mfp_cfg));
}
int comipfb_if_mipi_dev_cmds(struct comipfb_info *fbi, struct comipfb_dev_cmds *cmds)
{
	int ret = -1;
	int loop = 0;
	unsigned char *p;
	int line = fbi->cdev->cmdline;

	if (!line)
		line = ROW_LINE;

	if (!cmds)
		return 0;
	if (!cmds->n_pack || !cmds->cmds)
		return 0;
	for (loop = 0; loop < cmds->n_pack; loop++) {
		p = cmds->cmds + loop * line;
		if (p[1] == DCS_CMD)
			ret = mipi_dsih_dcs_wr_cmd(fbi, 0, &(p[2]), (u16)p[3]);
		else if (p[1] == GEN_CMD)
			ret = mipi_dsih_gen_wr_cmd(fbi, 0, &(p[2]), (u16)p[3]);
		if (ret < 0) {
			printk("*MIPI send command failed !!*\n");
			return ret;
		}
		if (p[0] > 0) {
#ifdef CONFIG_FBCON_DRAW_PANIC_TEXT
			if (unlikely(kpanic_in_progress == 1)) {
				if (p[0] == 0xff)
					mdelay(200);
				else
					mdelay(p[0]);
			}
			else {

				if (p[0] == 0xff)
					msleep(200);
				else
					msleep(p[0]);
			}
#else
			if (p[0] == 0xff)
				msleep(200);
			else
				msleep(p[0]);
#endif

		}
	}
	return ret;
}

static int comipfb_if_mipi_init(struct comipfb_info *fbi)
{
	int ret = 0;

	comipfb_if_mipi_pin_init();
	comipfb_mipi_mode_change(fbi);
	mipi_dsih_cmd_mode(fbi, 1);
	/* Reset device. */
	if (fbi->cdev->reset)
		fbi->cdev->reset(fbi);
	ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);

	mipi_dsih_dphy_enable_hs_clk(fbi, 1);
	mipi_dsih_video_mode(fbi, 1);

	return ret;
}

static int comipfb_if_mipi_exit(struct comipfb_info *fbi)
{
	int gpio_im = fbi->pdata->gpio_im;
	if (gpio_im >= 0)
		gpio_free(gpio_im);

	return 0;
}

static int comipfb_if_mipi_suspend(struct comipfb_info *fbi)
{
	int ret = 0;
	
	if(fbi->cdev->suspend)
		ret = fbi->cdev->suspend(fbi);

	return ret;
}

static int comipfb_if_mipi_resume(struct comipfb_info *fbi)
{
	int ret = 0;
		
	if(fbi->cdev->resume)
		ret = fbi->cdev->resume(fbi);

	return ret;
}

static void comipfb_if_mipi_bl_change(struct comipfb_info *fbi, int val)
{
	unsigned int bit;
	struct comipfb_dev_cmds *lcd_backlight_cmd;

	if (fbi == NULL) {
		printk(KERN_ERR "%s ,fbi is NULL",__func__);
		return ;
	}

	bit = fbi->cdev->backlight_info.brightness_bit;
 	lcd_backlight_cmd = &(fbi->cdev->backlight_info.bl_cmd);
	lcd_backlight_cmd->cmds[bit] = (unsigned char)val;
	comipfb_if_mipi_dev_cmds(fbi, lcd_backlight_cmd);
}

struct comipfb_if comipfb_if_mipi = {
        .init           = comipfb_if_mipi_init,
        .exit           = comipfb_if_mipi_exit,
        .suspend        = comipfb_if_mipi_suspend,
        .resume         = comipfb_if_mipi_resume,
        .dev_cmd        = comipfb_if_mipi_dev_cmds,
	.bl_change	= comipfb_if_mipi_bl_change,
};

static struct comipfb_if comipfb_if_hdmi = {
        .init           = comipfb_if_rgb_init,
        .exit           = comipfb_if_rgb_exit,
        .suspend        = comipfb_if_rgb_suspend,
        .resume         = comipfb_if_rgb_resume,
        .dev_cmd        = comipfb_if_rgb_dev_cmds,
};

struct comipfb_if* comipfb_if_get(struct comipfb_info *fbi)
{
	struct comipfb_info *info;
	info = fbi;
	switch (info->cdev->interface_info) {
		case COMIPFB_MPU_IF:
			return &comipfb_if_mpu;
		case COMIPFB_RGB_IF:
			return &comipfb_if_rgb;
		case COMIPFB_MIPI_IF:
			return &comipfb_if_mipi;
		case COMIPFB_HDMI_IF:
			return &comipfb_if_hdmi;
		default:
			return NULL;
	}
}
EXPORT_SYMBOL(comipfb_if_get);
EXPORT_SYMBOL(comipfb_if_mipi_dev_cmds);

