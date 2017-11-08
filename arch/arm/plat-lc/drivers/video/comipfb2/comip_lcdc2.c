/* driver/video/comip/comipfb.c
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
#include <plat/cpu.h>

#include "comipfb_if.h"
#include "comip_lcdc.h"
#include "mipi_interface.h"

static struct comipfb_info *lcdc_fbi;

#define LCDC_WAIT_FIFO_EMPTY_TIMEOUT	(1000) /* us. */
#define LCDC_WAIT_DMA_OVER_TIMEOUT	(4) /* 10ms. */

#define LCDC_CLKCNT_SUB(clkcnt, sub)	\
	((clkcnt > sub) ? (clkcnt - sub) : 0)

#define LCDC_CLKCNT_CHECK(name, clkcnt, mask)	\
	if (clkcnt > mask) { \
		clkcnt = mask; \
		printk (KERN_WARNING "[%s] invalid clock count %d", name, clkcnt); \
	}

struct lcdc_video_timeing {
	unsigned int vpr;
	unsigned int hpr;
	unsigned int hstr;
	unsigned int hetr;
	unsigned int vstr0;
	unsigned int vetr0;
	unsigned int vstr1;
	unsigned int vetr1;
	unsigned int hso;
	unsigned int heo;
	unsigned int vso;
	unsigned int veo;
	unsigned int vintr;
	unsigned int hintr;
	unsigned int dm;
};

static DEFINE_SPINLOCK(lcdc_lock);

int lcdc_is_support_dsi_auto_reset(struct comipfb_info *fbi)
{
	return 1;
}

int lcdc_mipi_dsi_auto_reset_config(struct comipfb_info *fbi,
	u32 v, u32 h, u32 period)
{
	u32 val = 0;

	if (period > ((1 << 8) + 1)) {
		printk(KERN_ERR "lcdc: mipi dsi auto reset period (%d) is error\n",
			period);
		return -EINVAL;
	}

	val = (v << LCDC_MIPI_DSI_AUTORESETTIME_V) |
		(h << LCDC_MIPI_DSI_AUTORESETTIME_H);
	writel(val, io_p2v(fbi->res->start + LCDC_MIPI_DSI_AUTORESETTIME));

	val = readl(io_p2v(fbi->res->start + LCDC_MIPI_DSI_AUTORESETCTL));
	val = (val & 0xffff00ff) | ((period - 1) << LCDC_MIPI_DSI_AUTORESETCTL_PERIOD);
	writel(val, io_p2v(fbi->res->start + LCDC_MIPI_DSI_AUTORESETCTL));

	return 0;
}

void lcdc_mipi_dsi_auto_reset_enable(struct comipfb_info *fbi,
	u8 enable)
{
	u32 val;

	val = readl(io_p2v(fbi->res->start + LCDC_MIPI_DSI_AUTORESETCTL));
	val &= ~(0x1 << LCDC_MIPI_DSI_AUTORESETCTL_EN);
	val |= (enable << LCDC_MIPI_DSI_AUTORESETCTL_EN);
	writel(val, io_p2v(fbi->res->start + LCDC_MIPI_DSI_AUTORESETCTL));
}

static void lcdc_lay_fifo_clear(struct comipfb_info *fbi, unsigned char lay)
{
	unsigned int val;
	
	val = readl(io_p2v(fbi->res->start + LCDC_L0_WIN_CTRL + lay * 0x18));
	val |= (1 << LCDC_L_WIN_CTRL_FIFO_FLUSH);
	writel(val, io_p2v(fbi->res->start + LCDC_L0_WIN_CTRL + lay * 0x18));
	while (readl(io_p2v(fbi->res->start + LCDC_L0_WIN_CTRL + lay * 0x18)) & (1 << LCDC_L_WIN_CTRL_FIFO_FLUSH));
}

static void lcdc_lay_dma_enable(struct comipfb_info *fbi, unsigned char lay,unsigned char enable)
{
	unsigned int val;

	val = readl(io_p2v(fbi->res->start + LCDC_L0_WIN_CTRL + lay * 0x18));
	val &= ~(1 << LCDC_L_WIN_CTRL_DMA_CHEN);
	val |= (enable << LCDC_L_WIN_CTRL_DMA_CHEN);
	writel(val, io_p2v(fbi->res->start + LCDC_L0_WIN_CTRL + lay * 0x18));
}

static void lcdc_lay_enable(struct comipfb_info *fbi, unsigned char lay,unsigned char enable)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_DISP_CTRL));
	val &= ~(1 << (lay + 8));
	val |= (enable << (lay + 8));
	writel(val, io_p2v(fbi->res->start + LCDC_DISP_CTRL));
	spin_unlock_irqrestore(&lcdc_lock, flags);
}

static void lcdc_lay_addr_choice(struct comipfb_info *fbi, unsigned char lay,unsigned char addr)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_DISP_CTRL));
	val &= ~(1 << (lay + 16));
	val |= (addr << (lay + 16));
	writel(val, io_p2v(fbi->res->start + LCDC_DISP_CTRL));
	spin_unlock_irqrestore(&lcdc_lock, flags);
}

static int lcdc_lay_wait_dma_transfer_over(struct comipfb_info *fbi, unsigned char lay)
{
	unsigned int timeout = LCDC_WAIT_DMA_OVER_TIMEOUT;
	unsigned int flag = (1 << lay);

	while (timeout-- && (readl(io_p2v(fbi->res->start + LCDC_DMA_STA)) & flag))
		msleep(10);

	return 0;
}

void lcdc_fifo_clear(struct comipfb_info *fbi)
{
	unsigned int i;

	for(i = 0; i < LAYER_MAX; i++)
		lcdc_lay_fifo_clear(fbi, i);
}

static void lcdc_para_update(struct comipfb_info *fbi)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_DISP_CTRL));
	val |= (1 << LCDC_DISP_CTRL_PARA_UP);
	writel(val, io_p2v(fbi->res->start + LCDC_DISP_CTRL));
	spin_unlock_irqrestore(&lcdc_lock, flags);
}

void lcdc_hvcnte_enable(struct comipfb_info *fbi, unsigned int enable)
{
	unsigned int val;
	unsigned long flags;
	
	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_HV_CTL));
	if (enable)
		val |= 1 << LCDC_HV_CTL_HVCNTE;
	else {
		val &= ~(1 << LCDC_HV_CTL_HVCNTE);
	}
	writel(val, io_p2v(fbi->res->start + LCDC_HV_CTL));
	spin_unlock_irqrestore(&lcdc_lock, flags);
	mdelay(2);
}

static void lcdc_auto_refresh(struct comipfb_info *fbi, unsigned char enable)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_MOD));
	if(enable)
		val |= (1 << LCDC_MOD_SRF_EN);
	else
		val &= ~(1 << LCDC_MOD_SRF_EN);
	writel(val, io_p2v(fbi->res->start + LCDC_MOD));
	spin_unlock_irqrestore(&lcdc_lock, flags);
}

static void lcdc_mipi_te_opt(struct comipfb_info *fbi)
{
	unsigned int val;
	unsigned long flags;
	struct te_info *teinfo;

	teinfo = &(fbi->cdev->timing.mipi.teinfo);
	
	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_MIPI_IF_MODE));
	val &= ~(1 << LCDC_EX_TE_MOD) | (1 << LCDC_TEAR_EFFECT_SOURCE) | (1 << LCDC_TEAR_EFFECT_SYNC_EN);
	val |= (teinfo->te_trigger_mode << LCDC_EX_TE_MOD) | (teinfo->te_source << LCDC_TEAR_EFFECT_SOURCE) 
			| (teinfo->te_sync_en << LCDC_TEAR_EFFECT_SYNC_EN);
	writel(val, io_p2v(fbi->res->start + LCDC_MIPI_IF_MODE));
	val = fbi->pixclock / 1000;
	writel(val, io_p2v(fbi->res->start + LCDC_TIME1000US));
	spin_unlock_irqrestore(&lcdc_lock, flags);

	if (teinfo->te_source == 0) {
		fbi->cdev->timing.mipi.commandmode_info.tear_fx_en = 1; // TODO  
		if (fbi->display_mode == MIPI_VIDEO_MODE)
			fbi->cdev->timing.mipi.commandmode_info.tear_fx_en = 0;	 //not support in video mode
	}
	else if (teinfo->te_source == 1)
		fbi->cdev->timing.mipi.commandmode_info.tear_fx_en = 0; // for source is external, set 0 ?? 
}

static void lcdc_mipi_color_mode(struct comipfb_info *fbi)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_MIPI_IF_MODE));
	val &= ~(0x7 << LCDC_MIPI_IF_MODE_BIT);
	val |= (fbi->cdev->timing.mipi.color_mode.color_bits << LCDC_MIPI_IF_MODE_BIT);
	writel(val, io_p2v(fbi->res->start + LCDC_MIPI_IF_MODE));
	spin_unlock_irqrestore(&lcdc_lock, flags);
}

static void lcdc_if_enable(struct comipfb_info *fbi, unsigned char enable)
{
	unsigned int val;
	unsigned long flags;

	switch (fbi->cdev->interface_info) {
	case COMIPFB_MIPI_IF:
		spin_lock_irqsave(&lcdc_lock, flags);
		val = readl(io_p2v(fbi->res->start + LCDC_MOD));
		if (enable) {
			if (fbi->display_mode == MIPI_VIDEO_MODE) {
				val &= ~((1 << LCDC_MOD_EDPI_CMD_MOD) | (1 << LCDC_MOD_EDPI_VIDEO_MOD));
				val |= (1 << LCDC_MOD_EDPI_VIDEO_MOD);
			} else if (fbi->display_mode == MIPI_COMMAND_MODE) {
				val &= ~((1 << LCDC_MOD_EDPI_CMD_MOD) | (1 << LCDC_MOD_EDPI_VIDEO_MOD));
				val |= (1 << LCDC_MOD_EDPI_CMD_MOD);
			}
		} else {
			val &= ~((1 << LCDC_MOD_EDPI_CMD_MOD) | (1 << LCDC_MOD_EDPI_VIDEO_MOD));
		}
		writel(val, io_p2v(fbi->res->start + LCDC_MOD));
		spin_unlock_irqrestore(&lcdc_lock, flags);
		break;
	case COMIPFB_RGB_IF:
		break;
	default:
		printk(KERN_ERR "No lcd interface enable !!\n");
		break;
	}
}

void lcdc_refresh_enable(struct comipfb_info *fbi, unsigned char enable)
{
	if (enable) {
		//lcdc_fifo_clear(fbi);
		if (fbi->refresh_en > 0) {
			lcdc_auto_refresh(fbi, 1);
			lcdc_para_update(fbi);
		}
	} else {
		if (fbi->refresh_en > 0) {
			lcdc_auto_refresh(fbi, 0);
			lcdc_para_update(fbi);
		}
	}
	mdelay(2);
}

static void lcdc_rgb_config(struct comipfb_info *fbi)
{
	return;
}

static void lcdc_mipi_config(struct comipfb_info *fbi)
{
	struct comipfb_dev *cdev;
	struct fb_videomode *videomode;
	struct lcdc_video_timeing timeing;
	unsigned int val = 0;

	cdev = fbi->cdev;
	videomode = &fbi->display_info;

	if (fbi->display_mode == MIPI_VIDEO_MODE) {
		/* Check and set lcdc timing in video mode. */
		#if defined(CONFIG_RGB_LVDS_ZA7783)
		timeing.vpr = videomode->yres + videomode->upper_margin + videomode->lower_margin +videomode->vsync_len;
		timeing.hpr = videomode->xres + videomode->left_margin + videomode->right_margin + videomode->hsync_len;
		timeing.hstr = videomode->left_margin +videomode->hsync_len;//LVDS
		timeing.hetr = timeing.hstr + videomode->xres;
		timeing.vstr0 = videomode->upper_margin + videomode->vsync_len +1;//LVDS 
		timeing.vetr0 = timeing.vstr0 + videomode->yres;
		timeing.vstr1 = 0;
		timeing.vetr1 = 0;
		timeing.hso = 1; //LVDS
		timeing.heo = videomode->hsync_len +1;//LVDS
		timeing.vso = 1; 
		timeing.veo = videomode->vsync_len +1; 
		#else

		timeing.hso = 1 ;
		timeing.heo = timeing.hso + videomode->hsync_len ;
		timeing.hstr = timeing.heo + videomode->left_margin;
		timeing.hetr = timeing.hstr + videomode->xres;
		timeing.hpr = timeing.hetr + videomode->right_margin - timeing.hso;

		timeing.vso =  1;
		timeing.veo = timeing.vso + videomode->vsync_len;
		timeing.vstr0 = timeing.veo + videomode->upper_margin;
		timeing.vetr0 = timeing.vstr0 + videomode->yres;
       	 #endif
		timeing.vpr = timeing.vetr0 + videomode->lower_margin - timeing.vso;

		timeing.vintr = timeing.vpr;
		timeing.hintr = timeing.hpr;
		timeing.dm = 0;
		LCDC_CLKCNT_CHECK("vpr", timeing.vpr, 0xffff);
		LCDC_CLKCNT_CHECK("hpr", timeing.hpr, 0xffff);
		LCDC_CLKCNT_CHECK("hstr", timeing.hstr, 0xffff);
		LCDC_CLKCNT_CHECK("hetr", timeing.hetr, 0xffff);
		LCDC_CLKCNT_CHECK("vstr0", timeing.vstr0, 0xffff);
		LCDC_CLKCNT_CHECK("vetr0", timeing.vetr0, 0xffff);
		LCDC_CLKCNT_CHECK("hso", timeing.hso, 0xffff);
		LCDC_CLKCNT_CHECK("heo", timeing.heo, 0xffff);
		LCDC_CLKCNT_CHECK("vso", timeing.vso, 0xffff);
		LCDC_CLKCNT_CHECK("veo", timeing.veo, 0xffff);
		LCDC_CLKCNT_CHECK("vintr", timeing.vintr, 0xffff);
		LCDC_CLKCNT_CHECK("hintr", timeing.hintr, 0xffff);

		writel(((timeing.vpr << LCDC_HV_P_VPR) | (timeing.hpr << LCDC_HV_P_HPR)),
			io_p2v(fbi->res->start + LCDC_HV_P));
		writel(((timeing.hetr << LCDC_HV_HCTL_HETR) | (timeing.hstr << LCDC_HV_HCTL_HSTR)),
			io_p2v(fbi->res->start + LCDC_HV_HCTL));
		writel(((timeing.vetr0 << LCDC_HV_VCTL0_VETR0) | (timeing.vstr0 << LCDC_HV_VCTL0_VSTR0)),
			io_p2v(fbi->res->start + LCDC_HV_VCTL0));
		writel(((timeing.heo << LCDC_HV_HS_HEO) | (timeing.hso << LCDC_HV_HS_HSO)),
			io_p2v(fbi->res->start + LCDC_HV_HS));
		writel(((timeing.veo << LCDC_HV_VS_VEO) | (timeing.vso << LCDC_HV_VS_VSO)),
			io_p2v(fbi->res->start + LCDC_HV_VS));
		writel(((timeing.vintr << LCDC_HV_VINT_VINTR) | (timeing.hintr << LCDC_HV_VINT_HINTR)),
			io_p2v(fbi->res->start + LCDC_HV_VINT));
		writel(timeing.dm, io_p2v(fbi->res->start + LCDC_HV_DM));

		val = LCDC_HV_CTL_HS_POL_LOW << LCDC_HV_CTL_HS_POL;
		val |= LCDC_HV_CTL_VS_POL_LOW << LCDC_HV_CTL_VS_POL;
		//val |= 1 << LCDC_HV_CTL_HVCNTE;
		val |= ((1 << LCDC_HV_CTL_HVOEN) | (1 << LCDC_HV_CTL_CLKOEN));
		writel(val, io_p2v(fbi->res->start + LCDC_HV_CTL));
	}

	mipi_dsih_config(fbi);
}

void lcdc_init(struct comipfb_info *fbi)
{
	unsigned int val;
	unsigned int i;
	int lcdcaxi_clk;

	lcdc_fbi = fbi;
	/**LCDC BASE CONFIG**/
	/* Disable all interrupts. */
	lcdc_int_enable(fbi, 0xffffff, 0);
	/* Main window size. */
	val = (fbi->display_info.yres << LCDC_MWIN_SIZE_Y_SIZE);
	val |= (fbi->display_info.xres << LCDC_MWIN_SIZE_X_SIZE);
	writel(val, io_p2v(fbi->res->start + LCDC_MWIN_SIZE));

	/* Set output format & disable RGB interface. */
	val = readl(io_p2v(fbi->res->start + LCDC_MOD));
	val &= ~(1 << LCDC_MOD_LCD_OUT_FORMAT);
	val &= ~(1 << LCDC_MOD_RGB_EN);
	val |= (1 << LCDC_MOD_DMA_CLS_FIFO_RST_EN);
	writel(val, io_p2v(fbi->res->start + LCDC_MOD));

	/* DMA control. */
	lcdcaxi_clk = fbi->pdata->lcdcaxi_clk / 1000000;	//MHz
	val = 5000000 / (1000000 / lcdcaxi_clk);	//ps
	writel(val, io_p2v(fbi->res->start + LCDC_DMA_READ_PERIOD));

	val = readl(io_p2v(fbi->res->start + LCDC_DMA_CTRL));
	val &= ~(1 << LCDC_DMA_CTRL_KEEP_MEM_ALIVE);
	val &= ~(0xf << LCDC_DMA_CTRL_MAX_OS);
	val |= (7 << LCDC_DMA_CTRL_MAX_OS);
	writel(val, io_p2v(fbi->res->start + LCDC_DMA_CTRL));


	/* Refresh period. */
	val = fbi->display_info.xres * fbi->display_info.yres;
	writel(val, io_p2v(fbi->res->start + LCDC_RF_PER));

	/*Choice layer display addr source*/
	for (i = 0; i < LAYER_MAX; i++)
		lcdc_lay_addr_choice(fbi, i, 0);

	/*NOTICE: hw bug, do not enable LCDC_LP*/
	/* Set layer LP mode */
	val = (LCDC_IDLE_AHB_VAL << LCDC_IDLE_LIMIT_AHB) | (LCDC_AHBIF_LP_EN << LCDC_AHBIF_LP);
	val &= ~(LCDC_LP_EN << LCDC_LP);
	writel(val, io_p2v(fbi->res->start + LCDC_LP_CTRL));

	/* Layer DMA prior config*/
	val = (1 << LCDC_LAY1_DMA_PRIOR) | (2 << LCDC_LAY0_DMA_PRIOR);
	writel(val, io_p2v(fbi->res->start + LCDC_AXIDMA_PRIOR));

	/* First disable DSI CTL , set TE and color mode*/
	if (fbi->cdev->interface_info == COMIPFB_MIPI_IF) {
		val = (0 << LCDC_MIPI_DSI_CTRL_DPICOLORM) | (0 << LCDC_MIPI_DSI_CTRL_DPISHUTD);
		writel(val, io_p2v(fbi->res->start + LCDC_MIPI_DSI_CTRL));
		lcdc_mipi_te_opt(fbi);
		lcdc_mipi_color_mode(fbi);
	}
	/*INTERFACE CONFIG*/
	switch(fbi->cdev->interface_info) {
	case COMIPFB_RGB_IF:
		lcdc_rgb_config(fbi);
		break;
	case COMIPFB_MIPI_IF:
		lcdc_mipi_config(fbi);
		break;
	default:
		break;
	}

	/* Enable RGB,MIPI  interface. */
	lcdc_if_enable(fbi, 1);

	/* Clear FIFO and update LCDC registers. */
	lcdc_fifo_clear(fbi);
	lcdc_para_update(fbi);

	/* Initialize LCD interface and device. */
	fbi->cif->init(fbi);
	mipi_dsih_hal_power(fbi, 0);
	mdelay(2);
	mipi_dsih_hal_power(fbi, 1);
	mdelay(2);
	lcdc_hvcnte_enable(fbi, 1);
	/* Enable LCD controller */
	lcdc_refresh_enable(fbi, 1);

	if (fbi->display_mode == MIPI_VIDEO_MODE)
		lcdc_start(fbi, 1);
}

void lcdc_exit(struct comipfb_info *fbi)
{
	/* Disable all interrupts. */
	lcdc_int_enable(fbi, 0xffffffff, 0);

	/* Disable LCD controller */
	lcdc_start(fbi, 0);
	lcdc_refresh_enable(fbi, 0);
	msleep(30);

	mipi_dsih_hal_power(fbi, 0);
	mdelay(2);
	mipi_dsih_hal_power(fbi, 1);
	mdelay(2);

	/* Deinitial LCDC IF. */
	fbi->cif->exit(fbi);

	/* Disable  interface. */
	lcdc_if_enable(fbi, 0);

	/* Display power off. */
	display_powerdown(fbi);
}

void lcdc_suspend(struct comipfb_info *fbi)
{
	/* Disable LCD controller */
	lcdc_start(fbi, 0);
	lcdc_refresh_enable(fbi, 0);
	mdelay(30);
	//mdelay(30);
	//lcdc_hvcnte_enable(fbi, 0);

	mipi_dsih_hal_power(fbi, 0);
	mdelay(2);
	mipi_dsih_hal_power(fbi, 1);
	mdelay(2);

	/* Suspend LCD interface and device. */
	fbi->cif->suspend(fbi);

	/* Disable interface. */
	lcdc_if_enable(fbi, 0);
}

void lcdc_resume(struct comipfb_info *fbi)
{
	/* Enable interface. */
	lcdc_if_enable(fbi, 1);

	/* Resume LCD interface and device. */
	fbi->cif->resume(fbi);

	lcdc_para_update(fbi);

	/* Enable LCD controller */
	lcdc_refresh_enable(fbi, 1);
	//lcdc_hvcnte_enable(fbi, 1);

	if (fbi->display_mode == MIPI_VIDEO_MODE)
		lcdc_start(fbi, 1);
}

void lcdc_start(struct comipfb_info *fbi, unsigned char start)
{
	unsigned int val;
	unsigned long flags;
	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_MOD));
	val &= ~(1 << LCDC_MOD_START);
	val |= (start << LCDC_MOD_START);
	writel(val, io_p2v(fbi->res->start +LCDC_MOD));
	spin_unlock_irqrestore(&lcdc_lock, flags);
}

void lcdc_layer_config(struct comipfb_layer_info *layer, unsigned int flag)
{
	unsigned char no = layer->no;
	unsigned int val;
	unsigned int format;
	struct comipfb_info * fbi;

	fbi = layer->parent;
	if (!flag)
		return;

	if (flag & LCDC_LAYER_CFG_WIN) {
		/* Set srouce picture width. */
		val = (layer->win_y_size << LCDC_L_WIN_SIZE_Y_SIZE);
		val |= (layer->win_x_size << LCDC_L_WIN_SIZE_X_SIZE);
		writel(val, io_p2v(fbi->res->start + LCDC_L0_WIN_SIZE + no * 0x18));
		writel(layer->src_width, io_p2v(fbi->res->start + LCDC_L0_SRC_WID + no * 0x18));
	}

	if (flag & LCDC_LAYER_CFG_BUF) {
		/* Set start address. */
		writel(layer->buf_addr, io_p2v(fbi->res->start + LCDC_L0_SA0 + no * 0x18));
	}

	if (flag & LCDC_LAYER_CFG_CTRL) {
		/* Set window offset. */
		val = (layer->win_y_offset << LCDC_L_WIN_OFS_Y_OFFSET);
		val |= (layer->win_x_offset << LCDC_L_WIN_OFS_X_OFFSET);
		writel(val, io_p2v(fbi->res->start + LCDC_L0_WIN_OFS + no * 0x18));

		/* Set key color. */
		writel(layer->key_color, io_p2v(fbi->res->start + LCDC_L0_KEY_COLOR + no * 0x18));

		/* Config window control. */
		val = readl(io_p2v(fbi->res->start + LCDC_L0_WIN_CTRL + no * 0x18));
		val &= ~(0x1 << LCDC_L_WIN_CTRL_TRANEN);

		switch (layer->input_format) {
		case LCDC_LAYER_INPUT_FORMAT_RGB666:
			format = LCDC_L_WIN_CTRL_FORMAT_RGB666;
			break;
		case LCDC_LAYER_INPUT_FORMAT_RGB888:
			format = LCDC_L_WIN_CTRL_FORMAT_RGB888;
			break;
		case LCDC_LAYER_INPUT_FORMAT_ARGB8888:
			format = LCDC_L_WIN_CTRL_FORMAT_ARGB8888;
			break;
		case LCDC_LAYER_INPUT_FORMAT_ABGR8888:
			format = LCDC_L_WIN_CTRL_FORMAT_ABGR8888;
			break;
		case LCDC_LAYER_INPUT_FORMAT_YUV422:
			val = (0x1 << LCDC_L_WIN_CTRL_TRANEN);
			format = LCDC_L_WIN_CTRL_FORMAT_YUV422;
			break;
		case LCDC_LAYER_INPUT_FORMAT_YUV420SP:
			val = (0x1 << LCDC_L_WIN_CTRL_TRANEN);
			format = LCDC_L_WIN_CTRL_FORMAT_YUV420SP;
			break;
		case LCDC_LAYER_INPUT_FORMAT_RGB565:
			format = LCDC_L_WIN_CTRL_FORMAT_RGB565;
			break;
		default:
			format = LCDC_L_WIN_CTRL_FORMAT_RGB888;
			break;
		}

		/* Format. */
		val &= ~(0x7 << LCDC_L_WIN_CTRL_FORMAT);
		val |= (format << LCDC_L_WIN_CTRL_FORMAT);

		/* Alpha, keyc. */
		val |= (layer->alpha << LCDC_L_WIN_CTRL_ALPHA_VAL);
		if (layer->alpha_en)
			val |= (0x1 << LCDC_L_WIN_CTRL_ALPHA_EN);
		else
			val &= ~(0x1 << LCDC_L_WIN_CTRL_ALPHA_EN);
		if (layer->keyc_en)
			val |= (0x1 << LCDC_L_WIN_CTRL_KEYC_EN);
		else
			val &= ~(0x1 << LCDC_L_WIN_CTRL_KEYC_EN);
		if (layer->byte_ex_en)
			val |= (0x1 << LCDC_L_WIN_CTRL_BYTE_EX_EN);
		else
			val &= ~(0x1 << LCDC_L_WIN_CTRL_BYTE_EX_EN);

		writel(val, io_p2v(fbi->res->start + LCDC_L0_WIN_CTRL + no * 0x18));
	}

	lcdc_para_update(fbi);
}

void lcdc_layer_enable(struct comipfb_layer_info *layer, unsigned char enable)
{
	unsigned char no = layer->no;
	struct comipfb_info *fbi;

	fbi = layer->parent;
	if (enable) {
		lcdc_lay_dma_enable(fbi, no, 1);
		lcdc_lay_enable(fbi, no, 1);
	} else {
		lcdc_lay_dma_enable(fbi, no, 0);
		lcdc_lay_enable(fbi, no, 0);
		lcdc_lay_wait_dma_transfer_over(fbi, no);
	}

	lcdc_para_update(fbi);
}

void lcdc_int_enable(struct comipfb_info *fbi, unsigned int status, unsigned char enable)
{
	unsigned int int_enable;

	int_enable = readl(io_p2v(fbi->res->start + LCDC_INTE));
	if (enable)
		int_enable |= status;
	else
		int_enable &= ~status;
	writel(int_enable, io_p2v(fbi->res->start + LCDC_INTE));
}

void lcdc_int_clear(struct comipfb_info *fbi, unsigned int status)
{
	writel(status, io_p2v(fbi->res->start + LCDC_INTF));
}

unsigned int lcdc_int_status(struct comipfb_info *fbi)
{
	return readl(io_p2v(fbi->res->start + LCDC_INTF));
}

int lcdc_init_lw(struct comipfb_info *fbi)
{
	unsigned int val;
	int ret = 0;

	/**LCDC BASE CONFIG**/
	/* Disable all interrupts. */
	lcdc_int_enable(fbi, 0xffffff, 0);

	/* Set output format & disable RGB interface. */
	val = readl(io_p2v(fbi->res->start + LCDC_MOD));
	val &= ~(1 << LCDC_MOD_LCD_OUT_FORMAT);
	val &= ~(1 << LCDC_MOD_RGB_EN);
	writel(val, io_p2v(fbi->res->start + LCDC_MOD));
#if 0
	/* DMA control. */
	val = readl(io_p2v(fbi->res->start + LCDC_DMA_CTRL));
	val &= ~(1 << LCDC_DMA_CTRL_KEEP_MEM_ALIVE);
	val |= (2 << LCDC_DMA_CTRL_MAX_OS);
	writel(val, io_p2v(fbi->res->start + LCDC_DMA_CTRL));
#endif
	ret = mipi_dsih_config_lw(fbi);
	if (ret < 0) {
		printk(KERN_WARNING "<%s>Failed to initialize mipi lowlevel\n", __func__);
		return -1;
	}

	/* Enable RGB,MIPI  interface. */
	//lcdc_if_enable(fbi, 1);

	/* Clear FIFO and update LCDC registers. */
	lcdc_fifo_clear(fbi);
	lcdc_para_update(fbi);

	return 0;
}

/*for LC1860 LCDC specific to switch input format while working*/
int pre_set_layer_input_format(struct comipfb_info *fbi)
{
	/* Disable LCD controller */
	lcdc_start(fbi, 0);
	lcdc_refresh_enable(fbi, 0);
	msleep(30);

	mipi_dsih_hal_power(fbi, 0);
	mdelay(2);
	mipi_dsih_hal_power(fbi, 1);
	mdelay(2);

	return 0;
}

int post_set_layer_input_format(struct comipfb_info *fbi)
{
	lcdc_refresh_enable(fbi, 1);

	if (fbi->display_mode == MIPI_VIDEO_MODE)
		lcdc_start(fbi, 1);
	msleep(30);

	return 0;
}

void display_powerdown(struct comipfb_info *fbi)
{
	unsigned int val;
	int timeout = 100;

/*
	//CNT1
	val = 0x60;
	writel(val, io_p2v(AP_PWR_DISPLAY_PD_CNT1));
*/
	/* Display power down. */
	val = readl(io_p2v(AP_PWR_DISPLAY_PD_CTL));
	val &= ~(1 << AP_PWR_PD_MK);
	val |= (1 << AP_PWR_PD_MK_WE);
	val |= ((1 << AP_PWR_PD_EN) | (1 << AP_PWR_PD_EN_WE));
	writel(val, io_p2v(AP_PWR_DISPLAY_PD_CTL));
	while((readl(io_p2v(AP_PWR_PDFSM_ST1)) &
			(0x7 << AP_PWR_PDFSM_ST1_DISPLAY_PD_ST)) != (0x7 << AP_PWR_PDFSM_ST1_DISPLAY_PD_ST) && timeout-- > 0)
		udelay(10);

}

void display_powerup(struct comipfb_info *fbi)
{
	unsigned int val;
	int timeout = 100;
/*
	//CNT2
	val = 0x60;
	writel(val, io_p2v(AP_PWR_DISPLYA_PD_CNT2));
	//CNT3
	writel(val, io_p2v(AP_PWR_DISPLAY_PD_CNT3));
*/
	/* Display power up. */
	val = readl(io_p2v(AP_PWR_DISPLAY_PD_CTL));
	val |= ((1 << AP_PWR_WK_UP) | (1 << AP_PWR_WK_UP_WE));
	writel(val, io_p2v(AP_PWR_DISPLAY_PD_CTL));
	while((readl(io_p2v(AP_PWR_PDFSM_ST1)) &
			(0x7 << AP_PWR_PDFSM_ST1_DISPLAY_PD_ST)) != 0 && timeout-- > 0)
		udelay(10);
}


/*dynamic frame rate, after set a new frame rate, you must restore the default frame rate
* before set another frame rate*/
#define VPR_BIT_LEN		12
#define VPR_REVERSE_BIT		3

void set_fps(struct comipfb_info *fbi, unsigned int fps_new)
{
	unsigned int val, vpr, hpr, vpr_new;
	unsigned int fps_ori, vpr_ori;	/*to store original vpr when fps_new >= fps_ori*/
	unsigned long bits_map, bit_mask;
	int bit_cnt = VPR_REVERSE_BIT;	/*only reverse the most significant several bits*/
	struct fb_videomode *videomode;

	spin_lock(&lcdc_lock);
	val = readl(io_p2v(fbi->res->start + LCDC_HV_P));
	vpr = (val >> LCDC_HV_P_VPR) & 0xfff;
	hpr = (val >> LCDC_HV_P_HPR) & 0xfff;

	/*caculate original vpr & fps*/
	videomode = &fbi->display_info;
	vpr_ori = videomode->yres + videomode->upper_margin + videomode->lower_margin + videomode->vsync_len;
	fps_ori = fbi->pixclock / (vpr_ori * hpr);

	/*to avoid flicker*/
	if (fps_new < 30)
		fps_new = 30;

	/*vpr_new may overflow*/
	vpr_new = fbi->pixclock / (fps_new * hpr);
	if ((vpr_new >> VPR_BIT_LEN))
		vpr_new = (1 << VPR_BIT_LEN) - 1;

	if (fps_new >= fps_ori)
		vpr_new = vpr_ori;

	/*bits should be changed*/
	bits_map = vpr ^ vpr_new;

	do {
		bit_mask = find_last_bit(&bits_map, VPR_BIT_LEN);
		if ((bit_mask < (VPR_BIT_LEN - VPR_REVERSE_BIT)) || (bit_mask == VPR_BIT_LEN))
			break;

		/*clear or set bit*/
		if (vpr & (1 << (bit_mask)))
			vpr &= ~(1 << (bit_mask));
		else
			vpr |= (1 << (bit_mask));

		val = ((vpr & 0xfff) << LCDC_HV_P_VPR) | ((hpr & 0xfff) << LCDC_HV_P_HPR);
		writel(val, io_p2v(fbi->res->start + LCDC_HV_P));
		bits_map &= ~(1 << bit_mask);
	} while (--bit_cnt);
	spin_unlock(&lcdc_lock);

	/*recaculate the actual frame rate*/
	fps_new = fbi->pixclock / (vpr * hpr);
	printk(KERN_DEBUG "<%s>new frame rate :%d\n", __func__, fps_new);
}

unsigned int get_fps(struct comipfb_info *fbi)
{
	unsigned int fps;
	unsigned int val, vpr, hpr;

	spin_lock(&lcdc_lock);
	val = readl(io_p2v(fbi->res->start + LCDC_HV_P));
	vpr = (val >> LCDC_HV_P_VPR) & 0xfff;
	hpr = (val >> LCDC_HV_P_HPR) & 0xfff;
	spin_unlock(&lcdc_lock);

	/*frame per second*/
	fps = fbi->pixclock / (vpr * hpr);

	return fps;
}

static ssize_t dyn_fps_store(struct device *dev, struct device_attribute *attr,
								const char *buf, size_t count)
{
	unsigned int fps;

	/*frame rate to be set*/
	sscanf(buf,"%d", &fps);
	printk(KERN_DEBUG "<%s>set fps = %d\n", __func__, fps);

	set_fps(lcdc_fbi, fps);

	return count;
}

static ssize_t dyn_fps_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int fps;

	fps = get_fps(lcdc_fbi);
	printk(KERN_DEBUG "<%s>current frame rate :%d\n", __func__, fps);

	return snprintf(buf,
			PAGE_SIZE,
			"current frame rate :%d\n", fps);

}

DEVICE_ATTR(dyn_fps, S_IRUGO | S_IWUSR, dyn_fps_show, dyn_fps_store);

void comipfb_sysfs_dyn_fps(struct device *dev)
{
	device_create_file(dev, &dev_attr_dyn_fps);
}

EXPORT_SYMBOL(lcdc_init);
EXPORT_SYMBOL(lcdc_exit);
EXPORT_SYMBOL(lcdc_suspend);
EXPORT_SYMBOL(lcdc_resume);
EXPORT_SYMBOL(lcdc_start);
EXPORT_SYMBOL(lcdc_layer_config);
EXPORT_SYMBOL(lcdc_layer_enable);
EXPORT_SYMBOL(lcdc_int_enable);
EXPORT_SYMBOL(lcdc_int_clear);
EXPORT_SYMBOL(lcdc_int_status);
EXPORT_SYMBOL(lcdc_fifo_clear);
EXPORT_SYMBOL(lcdc_hvcnte_enable);
EXPORT_SYMBOL(lcdc_refresh_enable);
EXPORT_SYMBOL(lcdc_mipi_dsi_auto_reset_config);
EXPORT_SYMBOL(lcdc_mipi_dsi_auto_reset_enable);
EXPORT_SYMBOL(lcdc_is_support_dsi_auto_reset);
EXPORT_SYMBOL(lcdc_init_lw);
EXPORT_SYMBOL(pre_set_layer_input_format);
EXPORT_SYMBOL(post_set_layer_input_format);
EXPORT_SYMBOL(display_powerdown);
EXPORT_SYMBOL(display_powerup);
EXPORT_SYMBOL(set_fps);
EXPORT_SYMBOL(get_fps);
EXPORT_SYMBOL(comipfb_sysfs_dyn_fps);

