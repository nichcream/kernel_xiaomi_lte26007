/* driver/video/comipfb_hw.h
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 1999-2008  LeadCoreTech Corp.
**
**  PURPOSE:   		This files contains the driver of LCD controller.
**
**  CHANGE HISTORY:
**
**	Version		Date		Author			Description
**	0.1.0		2009-03-10	liuchangmin		created
**	0.1.1		2011-03-10	zouqiao	updated
**
*/

#ifndef __COMIP_LCDC_H__
#define __COMIP_LCDC_H__

#include <linux/bitops.h>
#include "comipfb.h"

#define LCDC_LAYER_CFG_CTRL					(0x00000001)
#define LCDC_LAYER_CFG_WIN					(0x00000002)
#define LCDC_LAYER_CFG_BUF					(0x00000004)
#define LCDC_LAYER_CFG_ALL					(0xffffffff)

extern void lcdc_init(struct comipfb_info *fbi);
extern void lcdc_exit(struct comipfb_info *fbi);
extern void lcdc_suspend(struct comipfb_info *fbi);
extern void lcdc_resume(struct comipfb_info *fbi);
extern void lcdc_start(struct comipfb_info *fbi, unsigned char start);
extern void lcdc_layer_config(struct comipfb_layer_info *layer, unsigned int flag);
extern void lcdc_layer_enable(struct comipfb_layer_info *layer, unsigned char enable);
extern void lcdc_cmd(struct comipfb_info *fbi, unsigned int cmd);
extern void lcdc_cmd_para(struct comipfb_info *fbi, unsigned int data);
extern void lcdc_int_enable(struct comipfb_info *fbi, unsigned int status, unsigned char enable);
extern void lcdc_int_clear(struct comipfb_info *fbi, unsigned int status);
extern unsigned int lcdc_int_status(struct comipfb_info *fbi);
extern void lcdc_fifo_clear(struct comipfb_info *fbi);
extern void lcdc_hvcnte_enable(struct comipfb_info *fbi, unsigned int enable);
extern void lcdc_refresh_enable(struct comipfb_info *fbi, unsigned char enable);
extern int lcdc_mipi_dsi_auto_reset_config(struct comipfb_info *fbi,
	u32 v, u32 h, u32 period);
extern void lcdc_mipi_dsi_auto_reset_enable(struct comipfb_info *fbi,
	u8 enable);
extern int lcdc_is_support_dsi_auto_reset(struct comipfb_info *fbi);
extern int lcdc_init_lw(struct comipfb_info *fbi);
extern int pre_set_layer_input_format(struct comipfb_info *fbi);
extern int post_set_layer_input_format(struct comipfb_info *fbi);
extern void display_powerdown(struct comipfb_info *fbi);
extern void display_powerup(struct comipfb_info *fbi);
extern void set_fps(struct comipfb_info *fbi, unsigned int new_fps);
extern unsigned int get_fps(struct comipfb_info *fbi);
extern void comipfb_sysfs_dyn_fps(struct device *dev);

#endif /*__COMIP_LCDC_H__*/
