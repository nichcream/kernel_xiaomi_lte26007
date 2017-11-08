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
#include "comipfb_if.h"
#include "mipi_cmd.h"

static struct comipfb_info *fs_fbi;

static u8 mipi_read_buff[30];

static ssize_t mipi_read_store(struct device *dev, struct device_attribute *attr,
								const char *buf, size_t count)
{
	int cmd_type, read_cmd, read_count;

	sscanf(buf,"%x%x%x", &cmd_type, &read_cmd, &read_count);

	memset(&mipi_read_buff, 0x0, ARRAY_SIZE(mipi_read_buff));

	mipi_dsih_gen_wr_packet(fs_fbi, 0, 0x37, (u8 *)&read_count, 1);
	if (cmd_type == 1)
		mipi_dsih_dcs_rd_cmd(fs_fbi, 0, (u8)read_cmd, (u8)read_count,
			(u8 *)&mipi_read_buff);
	else if (cmd_type == 0)
		mipi_dsih_gen_rd_cmd(fs_fbi, 0, (u8 *)&read_cmd, 1, (u8)read_count,
			(u8 *)&mipi_read_buff);

	return count;
}

static ssize_t mipi_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf,
			PAGE_SIZE,
			"buf[0]=0x%x, buf[1]=0x%x, buf[2]=0x%x, buf[3]=0x%x, buf[4]=0x%x\n",
			mipi_read_buff[0],
			mipi_read_buff[1],
			mipi_read_buff[2],
			mipi_read_buff[3],
			mipi_read_buff[4]);
}

DEVICE_ATTR(mipi_read, S_IRUGO | S_IWUSR, mipi_read_show, mipi_read_store);

void comipfb_sysfs_add_read(struct device *dev)
{
	device_create_file(dev, &dev_attr_mipi_read);
}

void comipfb_sysfs_remove_read(struct device *dev)
{
	device_remove_file(dev, &dev_attr_mipi_read);
}

EXPORT_SYMBOL(comipfb_sysfs_remove_read);
EXPORT_SYMBOL(comipfb_sysfs_add_read);

/*
 *
 * MIPI interface
 */
static int comipfb_mipi_mode_change(struct comipfb_info *fbi)
{
	int gpio_im, gpio_val;
	struct comipfb_dev_timing_mipi *mipi;

	mipi = &(fbi->cdev->timing.mipi);
	
	if (fbi != NULL) {
		gpio_im = fbi->pdata->gpio_im;
		gpio_val = mipi->im_pin_val;
		if (gpio_im >= 0) {
			gpio_request(gpio_im, "LCD IM");
			gpio_direction_output(gpio_im, gpio_val);
		}
	}
	return 0;
}

int comipfb_if_mipi_dev_cmds(struct comipfb_info *fbi, struct comipfb_dev_cmds *cmds)
{
	int ret = 0;
	int loop = 0;
	unsigned char *p;

	if (!cmds) {
		dev_err(fbi->dev, "cmds null\n");
		return -EINVAL;
	}
	if (!cmds->n_pack || !cmds->cmds) {
		dev_err(fbi->dev, "cmds null\n");
		return -EINVAL;
	}
	for (loop = 0; loop < cmds->n_pack; loop++) {
		p = cmds->cmds + loop * ROW_LINE;
		if (p[1] == DCS_CMD)
			ret = mipi_dsih_dcs_wr_cmd(fbi, 0, &(p[2]), (u16)p[3]);
		else if (p[1] == GEN_CMD)
			ret = mipi_dsih_gen_wr_cmd(fbi, 0, &(p[2]), (u16)p[3]);
		if (ret < 0) {
			dev_err(fbi->dev,"*MIPI send command failed !!*\n");
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

void comipfb_if_mipi_reset(struct comipfb_info *fbi)
{
	struct comipfb_dev_timing_mipi *mipi;
	int stop_status = 0;
	int check_times = 3;
	int i = 0;

	mipi = &(fbi->cdev->timing.mipi);

	switch (mipi->no_lanes) {
		case 1:
			stop_status = 0x10;
			break;
		case 2:
			stop_status = 0x90;
			break;
		case 3:
			stop_status = 0x290;
			break;
		case 4:
			stop_status = 0xa90;
			break;
		default:
			break;
	}

	for (i = 0; i < check_times; i++) {
		if ((mipi_dsih_dphy_get_status(fbi) & stop_status) == stop_status)
			break;
		udelay(5);
	}

	mipi_dsih_hal_power(fbi, 0);
	mipi_dsih_hal_power(fbi, 1);
}

static int comipfb_if_mipi_init(struct comipfb_info *fbi)
{
	int ret = 0;
	struct comipfb_dev_timing_mipi *mipi;
	unsigned int dev_flags;

	fs_fbi = fbi;

	mipi = &(fbi->cdev->timing.mipi);

	comipfb_mipi_mode_change(fbi);

	mipi_dsih_dphy_enable_hs_clk(fbi, 1);
	mdelay(5);

	if (mipi->display_mode == MIPI_VIDEO_MODE)
		mipi_dsih_hal_mode_config(fbi, 1);

	mipi_dsih_dphy_enable_hs_clk(fbi, 0);
	/* Reset device. */
	if (!(fbi->cdev->power)) {
		if (fbi->cdev->reset)
			fbi->cdev->reset(fbi);
	}

	if (!fbi->cdev->init_last) {
#ifdef CONFIG_MIPI_LVDS_ICN6201
		icn6201_chip_init(fbi->cdev->lvds_reg, fbi->cdev->reg_num);
#else
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
#endif
	}
	dev_flags = fbi->cdev->flags;
	if ((fbi->display_prefer != 0) && (dev_flags & RESUME_WITH_PREFER)) {
		ret = comipfb_display_prefer_set(fbi, fbi->display_prefer);
		if (ret)
			dev_warn(fbi->dev, "set prefer within resume failed\n");
	}
	if ((fbi->display_ce != 0) && (dev_flags & RESUME_WITH_CE)) {
		ret = comipfb_display_ce_set(fbi, fbi->display_ce);
		if (ret)
			dev_warn(fbi->dev, "set ce within resume failed\n");
	}
	if (fbi->cdev->init_last)
		ret = comipfb_if_mipi_dev_cmds(fbi, &fbi->cdev->cmds_init);
	msleep(10);

	if (mipi->display_mode == MIPI_VIDEO_MODE) {
		mipi_dsih_hal_mode_config(fbi, 0);
	}else
		mipi_dsih_hal_dcs_wr_tx_type(fbi, 3, 0);

	mdelay(5);
	mipi_dsih_dphy_enable_hs_clk(fbi, 1);

	return ret;
}

static int comipfb_if_mipi_exit(struct comipfb_info *fbi)
{
	int gpio_im = fbi->pdata->gpio_im;
	int ret = 0;

	if(fbi->cdev->suspend)
		ret = fbi->cdev->suspend(fbi);

	if (gpio_im >= 0)
		gpio_free(gpio_im);

	return ret;
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

static unsigned char te_cmds[][10] = {
/****TE command***/
        {0x00, DCS_CMD, SW_PACK2, 0x02, 0x35, 0x00},
};

static void comipfb_if_mipi_te_trigger(struct comipfb_info *fbi)
{
	struct comipfb_dev_cmds te_pkt;

	te_pkt.cmds = (unsigned char *)te_cmds;
	te_pkt.n_pack = 1;

	comipfb_if_mipi_dev_cmds(fbi, &te_pkt);
}


static int comipfb_if_mipi_read_lcm_id(struct comipfb_info *fbi , struct comipfb_id_info *cur_id_info)
{
	unsigned char rd_cnt=0,lp_cnt=0;
	unsigned char cmd;
	unsigned char *id_val;
	u8 lcm_id[8] = {0};
	int i, ret = 0;

	/*ready to read id*/
	if(cur_id_info->prepare_cmd.cmds)
		comipfb_if_mipi_dev_cmds(fbi, &(cur_id_info->prepare_cmd));

	while (lp_cnt < cur_id_info->num_id_info) {
		cmd = cur_id_info->id_info[lp_cnt].cmd;
		rd_cnt = cur_id_info->id_info[lp_cnt].id_count;
		id_val = cur_id_info->id_info[lp_cnt].id;

		mipi_dsih_gen_wr_packet(fbi, 0, 0x37, &rd_cnt, 1);
		if (cur_id_info->id_info[lp_cnt].pack_type == DCS_CMD) {
			ret = mipi_dsih_dcs_rd_cmd(fbi, 0, cmd, rd_cnt, lcm_id);
		} else if (cur_id_info->id_info[lp_cnt].pack_type == GEN_CMD) {
			ret = mipi_dsih_gen_rd_cmd(fbi, 0, &cmd, 1, rd_cnt, lcm_id);
		}
		if (!ret) {
			dev_err(fbi->dev, "failed to read lcm id\n");
			mipi_dsih_hal_power(fbi, 0);
			mdelay(2);
			mipi_dsih_hal_power(fbi, 1);
			mdelay(2);
			mipi_dsih_dphy_reset(fbi, 0);
			mipi_dsih_dphy_shutdown(fbi, 0);
			mdelay(10);
			mipi_dsih_dphy_shutdown(fbi, 1);
			mipi_dsih_dphy_reset(fbi, 1);
			mdelay(10);
		}
		ret = strncmp(lcm_id, id_val, rd_cnt);
		if (ret) {
			printk("request:");
			for (i = 0; i< rd_cnt; i++)
				printk("0x%x,", id_val[i]);
			printk(" read:");
			for (i = 0; i< rd_cnt; i++)
				printk("0x%x,", lcm_id[i]);
			return -1;
		}
		lp_cnt++;
	}
	return 0;
}

int comipfb_if_mipi_esd_read_ic(struct comipfb_info *fbi )
{
	int check_result;

	if (!fbi)
		return 0;

	check_result = comipfb_if_mipi_read_lcm_id( fbi , &(fbi->cdev->esd_id_info));

	if (check_result) {
		printk("lcd esd read id failed \n");
		return -1;
	} else {
		return 0;
	}
}

int comipfb_read_lcm_id(struct comipfb_info *fbi, void *dev)
{
	static int check_result, common_pwup = 0;
		struct comipfb_dev *lcm_dev = (struct comipfb_dev *)dev;

	fbi->cdev = lcm_dev;

	/*power on LCM and reset*/
	if (fbi->cdev->power)
		fbi->cdev->power(fbi, 1);
	else {
		if ((!common_pwup) && fbi->pdata->power) {
			fbi->pdata->power(1);
			common_pwup = 1;
		}
		if (fbi->cdev->reset)
			fbi->cdev->reset(fbi);
	}
	check_result = comipfb_if_mipi_read_lcm_id(fbi, &(lcm_dev->panel_id_info));

	if (fbi->cdev->power)
		fbi->cdev->power(fbi, 0);
	if ( check_result ) {
		printk("read lcm id failed \n");
		return -1;
	} else {
		return 0;
	}
}
static DEFINE_SPINLOCK(cmd_lock);
int comipfb_display_prefer_ce_set(struct comipfb_info *fbi, struct comipfb_prefer_ce *prefer_ce, int mode)
{
	struct prefer_ce_info *info = prefer_ce->info;
	int cnt = 0;
	int ret = 0;

	while (cnt < prefer_ce->types) {
		if (info->type == mode) {
			break;
		}
		info++;
		cnt++;
	}

	if (cnt == prefer_ce->types) {
		dev_err(fbi->dev, "mode %d not supported\n", mode);
		return -EINVAL;
	}

	//dev_info(fbi->dev, "sending cmds : type = %d\n", info->type);//for debug
	ret = comipfb_if_mipi_dev_cmds(fbi, &info->cmds);

	return ret;

}

int comipfb_display_prefer_set(struct comipfb_info *fbi, int mode)
{
	struct comipfb_prefer_ce *prefer = &fbi->cdev->display_prefer_info;
	unsigned int dev_flags;
	unsigned long flags;
	int ret = 0;

	if (prefer->types == 0) {
		dev_info(fbi->dev, "%s do not support prefer set\n", fbi->cdev->name);
		return -EINVAL;
	}

	dev_flags = fbi->cdev->flags;
	/*cmds must be send continuous and cannot be interrupted*/
	if (dev_flags & PREFER_CMD_SEND_MONOLITHIC)
		spin_lock_irqsave(&cmd_lock, flags);
	ret = comipfb_display_prefer_ce_set(fbi, prefer, mode);
	if (dev_flags & PREFER_CMD_SEND_MONOLITHIC)
		spin_unlock_irqrestore(&cmd_lock, flags);

	if (ret)
		dev_err(fbi->dev, "set display prefer %d failed\n", mode);

	/*update current prefer mode, when resume from suspend, should check and send cmd if need*/
	fbi->display_prefer = mode;

	return ret;
}

int comipfb_display_ce_set(struct comipfb_info *fbi, int mode)
{
	struct comipfb_prefer_ce *ce = &fbi->cdev->display_ce_info;
	unsigned int dev_flags;
	unsigned long flags;
	int ret = 0;

	if (ce->types == 0) {
		dev_info(fbi->dev, "%s do not support ce set\n", fbi->cdev->name);
		return -EINVAL;
	}

	dev_flags = fbi->cdev->flags;
	/*cmds must be send continuous and cannot be interrupted*/
	if (dev_flags & CE_CMD_SEND_MONOLITHIC)
		spin_lock_irqsave(&cmd_lock, flags);
	ret = comipfb_display_prefer_ce_set(fbi, ce, mode);
	if (dev_flags & CE_CMD_SEND_MONOLITHIC)
		spin_unlock_irqrestore(&cmd_lock, flags);

	if (ret)
		dev_err(fbi->dev, "set display ce %d failed\n", mode);

	/*update current ce mode, when resume from suspend, should check and send cmd if need*/
	fbi->display_ce = mode;

	return ret;
}

static struct comipfb_if comipfb_if_mipi = {
        .init           = comipfb_if_mipi_init,
        .exit           = comipfb_if_mipi_exit,
        .suspend        = comipfb_if_mipi_suspend,
        .resume         = comipfb_if_mipi_resume,
        .dev_cmd        = comipfb_if_mipi_dev_cmds,
	.bl_change	= comipfb_if_mipi_bl_change,
	.te_trigger = comipfb_if_mipi_te_trigger,
};

struct comipfb_if* comipfb_if_get(struct comipfb_info *fbi)
{

	switch (fbi->cdev->interface_info) {
		case COMIPFB_MIPI_IF:
			return &comipfb_if_mipi;
		default:
			return NULL;
	}
}

EXPORT_SYMBOL(comipfb_if_get);
EXPORT_SYMBOL(comipfb_if_mipi_dev_cmds);
EXPORT_SYMBOL(comipfb_if_mipi_reset);
EXPORT_SYMBOL(comipfb_if_mipi_esd_read_ic);
EXPORT_SYMBOL(comipfb_read_lcm_id);
EXPORT_SYMBOL(comipfb_display_prefer_set);
EXPORT_SYMBOL(comipfb_display_ce_set);

