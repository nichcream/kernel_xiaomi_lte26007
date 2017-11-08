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

#include "comipfb_if.h"
#include "comipfb_hw.h"
#include "mipi_interface.h"

struct lcdc_rgb_timing {
	unsigned int vpr;
	unsigned int hpr;
	unsigned int vt;
	unsigned int ht;
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
	unsigned int evnr;
	unsigned int oddr;
	unsigned int vintr;
	unsigned int hintr;
	unsigned int dm;
};

struct lcdc_mpu_timing {
	unsigned int ts1;
	unsigned int th1;
	unsigned int ts2;
	unsigned int th2;
	unsigned int rw_fp;
	unsigned int ac_p;
};

#define LCDC_WAIT_FIFO_EMPTY_TIMEOUT	(1000) /* us. */
#define LCDC_WAIT_DMA_OVER_TIMEOUT	(4) /* 10ms. */

#define LCDC_CLKCNT_SUB(clkcnt, sub)	\
	((clkcnt > sub) ? (clkcnt - sub) : 0)

#define LCDC_CLKCNT_CHECK(name, clkcnt, mask)	\
	if (clkcnt > mask) { \
		clkcnt = mask; \
		printk (KERN_WARNING "[%s] invalid clock count %d", name, clkcnt); \
	}

static DEFINE_SPINLOCK(lcdc_lock);

static unsigned int lcdc_ns2clkcnt(struct comipfb_info *fbi, unsigned int ns)
{
	unsigned int clkcnt = fbi->pixclock * ns;

	do_div(clkcnt, 1000000000);

	return (clkcnt + 1);
}

static void lcdc_lay_fifo_clear(struct comipfb_info *fbi, unsigned char lay)
{
	unsigned int val;
	
	val = readl(io_p2v(fbi->res->start + 0x94 + lay * 0x18));
	val |= (1 << LCDC_L_WIN_CTRL_FIFO_FLUSH);
	writel(val, io_p2v(fbi->res->start + 0x94 + lay * 0x18));
	while (readl(io_p2v(fbi->res->start + 0x94 + lay * 0x18)) & (1 << LCDC_L_WIN_CTRL_FIFO_FLUSH));
}

static void lcdc_lay_dma_enable(struct comipfb_info *fbi, unsigned char lay,unsigned char enable)
{
	unsigned int val;

	val = readl(io_p2v(fbi->res->start + 0x94 + lay * 0x18));
	val &= ~(1 << LCDC_L_WIN_CTRL_DMA_CHEN);
	val |= (enable << LCDC_L_WIN_CTRL_DMA_CHEN);
	writel(val, io_p2v(fbi->res->start + 0x94 + lay * 0x18));
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

static int lcdc_lay_wait_fifo_empty(struct comipfb_info *fbi, unsigned char lay)
{
	unsigned int timeout = LCDC_WAIT_FIFO_EMPTY_TIMEOUT;
	unsigned int flag = (1 << (lay * 4));

	while (timeout-- && !(readl(io_p2v(fbi->res->start + LCDC_FIFO_STATUS)) & flag))
		udelay(1);

	if (!(readl(io_p2v(fbi->res->start + LCDC_FIFO_STATUS)) & flag))
		return -1;

	return 0;
}

static int lcdc_lay_wait_dma_transfer_over(struct comipfb_info *fbi, unsigned char lay)
{
	unsigned int timeout = LCDC_WAIT_DMA_OVER_TIMEOUT;
	unsigned int flag = (1 << lay);

	while (timeout-- && (readl(io_p2v(fbi->res->start + LCDC_DMA_STA)) & flag))
		msleep(10);

	return 0;
}

static void lcdc_fifo_clear(struct comipfb_info *fbi)
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

static void lcdc_mpu_config(struct comipfb_info *fbi)
{
	struct comipfb_dev *cdev = fbi->cdev;
	struct comipfb_dev_timing_mpu *mpu = &cdev->timing.mpu;
	struct lcdc_mpu_timing timing;
	unsigned int val;

	/* Check and set mpu timing. */
	timing.ts2 = (mpu->cs >= mpu->wrl) ? (mpu->cs - mpu->wrl) : 0;
	timing.ts1 = (mpu->as >= timing.ts2) ? (mpu->as - timing.ts2) : 0;
	timing.th2 = mpu->csh;
	timing.th1 = (mpu->ah >= mpu->csh) ? (mpu->ah - mpu->csh) : 0;
	timing.rw_fp = mpu->wrl;
	timing.ts1 = lcdc_ns2clkcnt(fbi, timing.ts1);
	timing.th1 = lcdc_ns2clkcnt(fbi, timing.th1);
	timing.ts2 = lcdc_ns2clkcnt(fbi, timing.ts2);
	timing.th2 = lcdc_ns2clkcnt(fbi, timing.th2);
	timing.rw_fp = lcdc_ns2clkcnt(fbi, timing.rw_fp);
	timing.ac_p = timing.ts1 + timing.th1 + timing.ts2 +
			timing.th2 + timing.rw_fp - 1;

	LCDC_CLKCNT_CHECK("th2", timing.th2, 0x3);
	LCDC_CLKCNT_CHECK("ts2", timing.ts2, 0x3);
	LCDC_CLKCNT_CHECK("ts1", timing.ts1, 0x3);
	LCDC_CLKCNT_CHECK("rw_fp", timing.rw_fp, 0x1f);
	LCDC_CLKCNT_CHECK("ac_p", timing.ac_p, 0x3f);

	val = (timing.th2 << LCDC_ASYTIM_TH2);
	val |= (timing.ts2 << LCDC_ASYTIM_TS2);
	val |= (timing.ts1 << LCDC_ASYTIM_TS1);
	val |= (timing.rw_fp << LCDC_ASYTIM_RW_FP);
	val |= (timing.ac_p << LCDC_ASYTIM_AC_P);
	writel(val, io_p2v(fbi->res->start + LCDC_ASYTIM0));
	writel(val, io_p2v(fbi->res->start + LCDC_ASYTIM1));

	/* Set panel and pixel width. */
	val = (fbi->panel << LCDC_ASYCTL_SUB_PANEL);
	if (fbi->cdev->flags & COMIPFB_BUS_WIDTH_16BIT)
		val |= (LCDC_ASYCTL_ASY_MOD_16BIT << LCDC_ASYCTL_ASY_MOD);
	else
		val |= (LCDC_ASYCTL_ASY_MOD_18BIT << LCDC_ASYCTL_ASY_MOD);

	writel(val, io_p2v(fbi->res->start + LCDC_ASYCTL));
}

static void lcdc_rgb_config(struct comipfb_info *fbi)
{
	struct lcdc_rgb_timing timing;
	unsigned int val;

	/* Check and set mpu timing. */
	timing.vpr = fbi->yres + fbi->upper_margin + fbi->lower_margin +fbi->vsync_len;
	timing.hpr = fbi->xres + fbi->left_margin + fbi->right_margin + fbi->hsync_len;
	timing.vt = fbi->vsync_len;
	timing.ht = fbi->hsync_len;
	timing.hstr = LCDC_CLKCNT_SUB(fbi->left_margin + fbi->hsync_len, 5);
	timing.hetr = timing.hstr + fbi->xres;
	timing.vstr0 = fbi->upper_margin + fbi->vsync_len;
	timing.vetr0 = timing.vstr0 + fbi->yres;
	timing.vstr1 = 0;
	timing.vetr1 = 0;
	timing.hso = LCDC_CLKCNT_SUB(fbi->hsync_len, 1);
	timing.heo = timing.hso + fbi->xres + fbi->left_margin + fbi->right_margin;
	timing.vso = fbi->vsync_len;
	timing.veo = timing.vso + fbi->yres + fbi->upper_margin + fbi->lower_margin;
	timing.evnr = 0;
	timing.oddr = 0;
	timing.vintr = fbi->xres;
	timing.hintr = fbi->yres;
	timing.dm = 0;

	LCDC_CLKCNT_CHECK("vpr", timing.vpr, 0xffff);
	LCDC_CLKCNT_CHECK("hpr", timing.hpr, 0xffff);
	LCDC_CLKCNT_CHECK("vh", timing.vt, 0xffff);
	LCDC_CLKCNT_CHECK("hh", timing.ht, 0xffff);
	LCDC_CLKCNT_CHECK("hstr", timing.hstr, 0xffff);
	LCDC_CLKCNT_CHECK("hetr", timing.hetr, 0xffff);
	LCDC_CLKCNT_CHECK("vstr0", timing.vstr0, 0xffff);
	LCDC_CLKCNT_CHECK("vetr0", timing.vetr0, 0xffff);
	LCDC_CLKCNT_CHECK("hso", timing.hso, 0xffff);
	LCDC_CLKCNT_CHECK("heo", timing.heo, 0xffff);
	LCDC_CLKCNT_CHECK("vso", timing.vso, 0xffff);
	LCDC_CLKCNT_CHECK("veo", timing.veo, 0xffff);
	LCDC_CLKCNT_CHECK("vintr", timing.vintr, 0xffff);
	LCDC_CLKCNT_CHECK("hintr", timing.hintr, 0xffff);

	writel(
		((timing.vpr << 16) | timing.hpr),
		io_p2v(fbi->res->start + LCDC_HV_P)
	);
	writel(
		((timing.vt << 16) | timing.ht),
		io_p2v(fbi->res->start + LCDC_HV_SYNC)
	);
	writel(
		((timing.hetr << 16) | timing.hstr),
		io_p2v(fbi->res->start + LCDC_HV_HCTL)
	);
	writel(
		((timing.vetr0 << 16) | timing.vstr0),
		io_p2v(fbi->res->start + LCDC_HV_VCTL0)
	);
	writel(
		((timing.vetr1 << 16) | timing.vstr1),
		io_p2v(fbi->res->start + LCDC_HV_VCTL1)
	);
	writel(
		((timing.heo << 16) | timing.hso),
		io_p2v(fbi->res->start + LCDC_HV_HS)
	);
	writel(
		((timing.veo << 16) | timing.vso),
		io_p2v(fbi->res->start + LCDC_HV_VS)
	);
	writel(
		((timing.evnr << 16) | timing.oddr),
		io_p2v(fbi->res->start + LCDC_HV_F)
	);
	writel(
		((timing.vintr << 16) | timing.hintr),
		io_p2v(fbi->res->start + LCDC_HV_VINT)
	);
	writel(timing.dm, io_p2v(fbi->res->start + LCDC_HV_DM));

	/* Set rgb control. */
	val = ((1 << LCDC_HV_CTL_CLKOEN) |
			(1 << LCDC_HV_CTL_HVOEN) |
			(1 << LCDC_HV_CTL_HVCNTE));
	if (fbi->cdev->flags & COMIPFB_BUS_WIDTH_16BIT)
		val |= (LCDC_HV_CTL_HVPIX_MOD_16BITS << LCDC_HV_CTL_HVPIX_MOD);
	else
		val |= (LCDC_HV_CTL_HVPIX_MOD_18BITS << LCDC_HV_CTL_HVPIX_MOD);
	if (fbi->cdev->flags & COMIPFB_VSYNC_HIGH_ACT)
		val |= (1 << LCDC_HV_CTL_VS_POL);
	else
		val &= ~(1 << LCDC_HV_CTL_VS_POL);
	if (fbi->cdev->flags & COMIPFB_HSYNC_HIGH_ACT)
		val |= (1 << LCDC_HV_CTL_HS_POL);
	else
		val &= ~(1 << LCDC_HV_CTL_HS_POL);
	if (fbi->cdev->flags & COMIPFB_PCLK_HIGH_ACT)
		val |= (1 << LCDC_HV_CTL_CLKPOL);
	else
		val &= ~(1 << LCDC_HV_CTL_CLKPOL);
	writel(val, io_p2v(fbi->res->start + LCDC_HV_CTL));
}

static void lcdc_mipi_config(struct comipfb_info *fbi)
{
	struct lcdc_rgb_timing timing;
	struct mipi_timing mipi_time;
        struct comipfb_dev *cdev;
        struct comipfb_dev_timing_mipi mipi;
	unsigned int val;

	cdev = fbi->cdev;
	mipi = cdev->timing.mipi;
	/* Check and set rgb timing. */
	timing.vpr = fbi->yres + fbi->upper_margin + fbi->lower_margin +fbi->vsync_len;
	timing.hpr = fbi->xres + fbi->left_margin + fbi->right_margin + fbi->hsync_len;
	timing.vt = fbi->vsync_len;
	timing.ht = fbi->hsync_len;
	timing.hstr = LCDC_CLKCNT_SUB(fbi->left_margin + fbi->hsync_len, 5);
	timing.hetr = timing.hstr + fbi->xres;
	timing.vstr0 = fbi->upper_margin + fbi->vsync_len;
	timing.vetr0 = timing.vstr0 + fbi->yres;
	timing.vstr1 = 0;
	timing.vetr1 = 0;
	timing.hso = LCDC_CLKCNT_SUB(fbi->hsync_len, 1);
	timing.heo = timing.hso + fbi->xres + fbi->left_margin + fbi->right_margin;
	timing.vso = fbi->vsync_len;
	timing.veo = timing.vso + fbi->yres + fbi->upper_margin + fbi->lower_margin;
	timing.evnr = 0;
	timing.oddr = 0;
	timing.vintr = fbi->xres;
	timing.hintr = fbi->yres;
	timing.dm = 0;

	LCDC_CLKCNT_CHECK("vpr", timing.vpr, 0xffff);
	LCDC_CLKCNT_CHECK("hpr", timing.hpr, 0xffff);
	LCDC_CLKCNT_CHECK("vh", timing.vt, 0xffff);
	LCDC_CLKCNT_CHECK("hh", timing.ht, 0xffff);
	LCDC_CLKCNT_CHECK("hstr", timing.hstr, 0xffff);
	LCDC_CLKCNT_CHECK("hetr", timing.hetr, 0xffff);
	LCDC_CLKCNT_CHECK("vstr0", timing.vstr0, 0xffff);
	LCDC_CLKCNT_CHECK("vetr0", timing.vetr0, 0xffff);
	LCDC_CLKCNT_CHECK("hso", timing.hso, 0xffff);
	LCDC_CLKCNT_CHECK("heo", timing.heo, 0xffff);
	LCDC_CLKCNT_CHECK("vso", timing.vso, 0xffff);
	LCDC_CLKCNT_CHECK("veo", timing.veo, 0xffff);
	LCDC_CLKCNT_CHECK("vintr", timing.vintr, 0xffff);
	LCDC_CLKCNT_CHECK("hintr", timing.hintr, 0xffff);

	writel(
		((timing.vpr << 16) | timing.hpr),
		io_p2v(fbi->res->start + LCDC_HV_P)
	);
	writel(
		((timing.vt << 16) | timing.ht),
		io_p2v(fbi->res->start + LCDC_HV_SYNC)
	);
	writel(
		((timing.hetr << 16) | timing.hstr),
		io_p2v(fbi->res->start + LCDC_HV_HCTL)
	);
	writel(
		((timing.vetr0 << 16) | timing.vstr0),
		io_p2v(fbi->res->start + LCDC_HV_VCTL0)
	);
	writel(
		((timing.vetr1 << 16) | timing.vstr1),
		io_p2v(fbi->res->start + LCDC_HV_VCTL1)
	);
	writel(
		((timing.heo << 16) | timing.hso),
		io_p2v(fbi->res->start + LCDC_HV_HS)
	);
	writel(
		((timing.veo << 16) | timing.vso),
		io_p2v(fbi->res->start + LCDC_HV_VS)
	);
	writel(
		((timing.evnr << 16) | timing.oddr),
		io_p2v(fbi->res->start + LCDC_HV_F)
	);
	writel(
		((timing.vintr << 16) | timing.hintr),
		io_p2v(fbi->res->start + LCDC_HV_VINT)
	);
	writel(timing.dm, io_p2v(fbi->res->start + LCDC_HV_DM));

	/* Set rgb control. */   //need to modify, LOW active for MIPI, byly
	val = ((1 << LCDC_HV_CTL_CLKOEN) |
			(1 << LCDC_HV_CTL_HVOEN) |
			(1 << LCDC_HV_CTL_HVCNTE));
	if (fbi->cdev->flags & COMIPFB_BUS_WIDTH_16BIT)
		val |= (LCDC_HV_CTL_HVPIX_MOD_16BITS << LCDC_HV_CTL_HVPIX_MOD);
	else
		val |= (LCDC_HV_CTL_HVPIX_MOD_18BITS << LCDC_HV_CTL_HVPIX_MOD);

	if (fbi->cdev->flags & COMIPFB_VSYNC_HIGH_ACT) {
		val |= (1 << LCDC_HV_CTL_VS_POL);
		mipi_time.v_polarity = MIPI_LEVEL_LOW;
	}
	else {
		val &= ~(1 << LCDC_HV_CTL_VS_POL);
		mipi_time.v_polarity = MIPI_LEVEL_HIGHT;
	}
	if (fbi->cdev->flags & COMIPFB_HSYNC_HIGH_ACT) {
		val |= (1 << LCDC_HV_CTL_HS_POL);
		mipi_time.h_polarity = MIPI_LEVEL_LOW;
	}
	else {
		val &= ~(1 << LCDC_HV_CTL_HS_POL);
		mipi_time.h_polarity = MIPI_LEVEL_HIGHT;
	}
	if (fbi->cdev->flags & COMIPFB_PCLK_HIGH_ACT)
		val |= (1 << LCDC_HV_CTL_CLKPOL);
	else
		val &= ~(1 << LCDC_HV_CTL_CLKPOL);

	writel(val, io_p2v(fbi->res->start + LCDC_HV_CTL));
	//config mipi interface mode
	writel(mipi.lcdc_mipi_if_mode, io_p2v(fbi->res->start + LCDC_MIPI_IF_MODE));
	
	mipi_time.hsa = timing.hpr - (timing.heo - timing.hso);
	//mipi_time.hbp = timing.heo - timing.hetr - 5;
	//mipi_time.hfp = timing.hstr + 5 - timing.hso;
	mipi_time.hbp = timing.hstr + 5 - timing.hso - 1;
	mipi_time.hfp = timing.heo + 1 - timing.hetr - 5;
	mipi_time.hact = timing.hetr - timing.hstr;
	mipi_time.htotal = mipi_time.hsa + mipi_time.hbp + mipi_time.hact + mipi_time.hfp;
	mipi_time.vsa = timing.vpr - (timing.veo - timing.vso);
	//mipi_time.vfp = timing.vstr0 - timing.vso;
	//mipi_time.vbp = timing.veo - timing.vetr0;
	mipi_time.vfp = timing.veo - timing.vetr0;
	mipi_time.vbp = timing.vstr0 - timing.vso;
	mipi_time.vactive = timing.vetr0 - timing.vstr0;
	mipi_time.data_en_polarity = MIPI_LEVEL_HIGHT;
	mipi_time.is_18_loosely = 0;
	mipi_time.receive_ack_packets = 0;
	mipi_time.trans_mode = VIDEO_BURST_WITH_SYNC_PULSES;
	mipi_time.virtual_channel = 0;
	mipi_time.color_coding = mipi.lcdc_mipi_if_mode;
	mipi_time.color_mode_polarity = MIPI_LEVEL_LOW;
	mipi_time.shut_down_polarity = MIPI_LEVEL_LOW;
	mipi_time.max_hs_to_lp_cycles = (151 * mipi.output_freq / 1000000) + 1;
	mipi_time.max_lp_to_hs_cycles = (865 * mipi.output_freq / 1000000) + 1;
	mipi_time.max_bta_cycles = 0;
	mipi_dsih_dpi_video(fbi, &mipi_time);
}

static void lcdc_hdmi_config(struct comipfb_info *fbi)
{

}

static void lcdc_if_enable(struct comipfb_info *fbi, unsigned char enable)
{
	unsigned int val;
	unsigned int bit;
	unsigned long flags;

	switch(fbi->cdev->flags) {
		case COMIPFB_MPU_IF:
			bit = LCDC_MOD_ASY_EN;
		break;
		case COMIPFB_RGB_IF:
			bit = LCDC_MOD_HV_EN;
		break;
		case COMIPFB_MIPI_IF:
			bit = LCDC_MOD_MIPI_EN;
		break;
		default:
			bit = LCDC_MOD_MIPI_EN;
		break;
	}
	/* Mode. */
	spin_lock_irqsave(&lcdc_lock, flags);
	val = readl(io_p2v(fbi->res->start + LCDC_MOD));
	if (enable)
		val |= (1 << bit);
	else
		val &= ~(1 << bit);
	writel(val, io_p2v(fbi->res->start + LCDC_MOD));
	spin_unlock_irqrestore(&lcdc_lock, flags);
}

void lcdc_init(struct comipfb_info *fbi)
{
	unsigned int val;

	/* Disable all interrupts. */
	lcdc_int_enable(fbi, 0xffffffff, 0);

	/* Main window size. */
	val = (fbi->main_win_y_size << LCDC_MWIN_SIZE_Y_SIZE);
	val |= (fbi->main_win_x_size << LCDC_MWIN_SIZE_X_SIZE);
	writel(val, io_p2v(fbi->res->start + LCDC_MWIN_SIZE));

	/* Refresh period. */
	val = fbi->pixclock / fbi->refresh;
	writel(val, io_p2v(fbi->res->start + LCDC_RF_PER));

	/* Display control. */
	val = (LCDC_DISP_CTRL_LSB_FIRST_MSB << LCDC_DISP_CTRL_LSB_FIRST);
	val |= (LCDC_DISP_CTRL_BGR_SEQ_RGB << LCDC_DISP_CTRL_BGR_SEQ);
	writel(val, io_p2v(fbi->res->start + LCDC_DISP_CTRL));

	/* Set layer LP mode */
	val = (LCDC_IDLE_AHB_VAL << LCDC_IDLE_LIMIT_AHB) | (LCDC_AHBIF_LP_EN << LCDC_AHBIF_LP) | (LCDC_LP_EN << LCDC_LP);
	writel(val, io_p2v(fbi->res->start + LCDC_LP_CTRL));

	/* First Disable DSI CTL */
	writel(MIPI_DSI_CTL_DIS, io_p2v(fbi->res->start + LCDC_MIPI_DSI_CTRL));

	/* Config interface. */
	switch(fbi->cdev->interface_info) {
		case COMIPFB_MPU_IF:
			lcdc_mpu_config(fbi);
		break;
		case COMIPFB_RGB_IF:
			lcdc_rgb_config(fbi);
		break;
		case COMIPFB_MIPI_IF:
			lcdc_mipi_config(fbi);
		break;
		case COMIPFB_HDMI_IF:
			lcdc_hdmi_config(fbi);
		break;
		default:
		break;
	}

	/* Mode. */
	val = (LCDC_MOD_LCD_OUT_FORMAT_RGB << LCDC_MOD_LCD_OUT_FORMAT);
	if (fbi->cdev->flags & COMIPFB_INS_HIGH)
		val |= (LCDC_MOD_INS_HIGH_HIGH << LCDC_MOD_INS_HIGH);
	else
		val |= (LCDC_MOD_INS_HIGH_LOW << LCDC_MOD_INS_HIGH);

	/* Enable HW CLS RESET FUN */
	val |= LCDC_MOD_DMA_CLS_FIFO_RST_EN << LCDC_MOD_DMA_CLS_FIFO_RST;
	writel(val, io_p2v(fbi->res->start + LCDC_MOD));

	/* Enable RGB,MIPI or MPU interface. */
	lcdc_if_enable(fbi, 1);

	/* Clear FIFO and update LCDC registers. */
	lcdc_fifo_clear(fbi);
	lcdc_para_update(fbi);

	/* Initialize LCD interface and device. */
	fbi->cif->init(fbi);

	/* Enable LCD controller */
	lcdc_enable(fbi, 1);
	lcdc_start(fbi, 1);
	msleep(3);
	if (fbi->pdata->bl_control)	//for disposing backlight open firstly.
		fbi->pdata->bl_control(1);
}

void lcdc_exit(struct comipfb_info *fbi)
{
	/* Deinitial LCDC IF. */
	fbi->cif->exit(fbi);

	/* Disable all interrupts. */
	lcdc_int_enable(fbi, 0xffffffff, 0);

	/* Disable LCD controller */
	lcdc_enable(fbi, 0);

	/* Disable  interface. */
	lcdc_if_enable(fbi, 1);
}

void lcdc_enable(struct comipfb_info *fbi, unsigned char enable)
{
	if (enable) {
		//lcdc_fifo_clear(fbi);
		lcdc_auto_refresh(fbi, 1);
		lcdc_para_update(fbi);
	} else {
		lcdc_auto_refresh(fbi, 0);
		lcdc_para_update(fbi);
	}
}

void lcdc_suspend(struct comipfb_info *fbi)
{
	/* Disable LCD controller */
	lcdc_enable(fbi, 0);

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
	lcdc_enable(fbi, 1);
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
		writel(val, io_p2v(fbi->res->start + 0xA0 + no * 0x18));
		writel(layer->src_width, io_p2v(fbi->res->start + 0xA8 + no * 0x18));
	}

	if (flag & LCDC_LAYER_CFG_BUF) {
		/* Set start address. */
		writel(layer->buf_addr, io_p2v(fbi->res->start + 0x98 + no * 0x18));
	}

	if (flag & LCDC_LAYER_CFG_CTRL) {
		/* Set window offset. */
		val = (layer->win_y_offset << LCDC_L_WIN_OFS_Y_OFFSET);
		val |= (layer->win_x_offset << LCDC_L_WIN_OFS_X_OFFSET);
		writel(val, io_p2v(fbi->res->start + 0xA4 + no * 0x18));

		/* Set key color. */
		writel(layer->key_color, io_p2v(fbi->res->start + 0x9C + no * 0x18));

		/* Config window control. */
		val = readl(io_p2v(fbi->res->start + 0x94 + no * 0x18));
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

		case LCDC_LAYER_INPUT_FORMAT_RGB332:
			format = LCDC_L_WIN_CTRL_FORMAT_RGB332;
		break;

		case LCDC_LAYER_INPUT_FORMAT_YCRCB422:
			val = (0x1 << LCDC_L_WIN_CTRL_TRANEN);
			format = LCDC_L_WIN_CTRL_FORMAT_YCRCB422;
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

		val |= (LCDC_L_WIN_CTRL_DMA_MODE_DMA << LCDC_L_WIN_CTRL_DMA_MODE);
		val |= (LCDC_L_WIN_CTRL_DMA_HIGH_SPEED << LCDC_L_WIN_CTRL_DMA_TRANS_MODE);

		/* Alpha, keyc. */
		val |= (layer->alpha << LCDC_L_WIN_CTRL_ALPHA);
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

		writel(val, io_p2v(fbi->res->start + 0x94 + no * 0x18));
	}

	lcdc_para_update(fbi);
}

void lcdc_layer_enable(struct comipfb_layer_info *layer, unsigned char enable)
{
	unsigned char no = layer->no;
	struct comipfb_info *fbi;

	fbi = layer->parent;
	if (enable) {
		if (layer->gscale_en == 0)
			lcdc_lay_dma_enable(fbi, no, 1);
		lcdc_lay_enable(fbi, no, 1);
	} else {
		lcdc_lay_dma_enable(fbi, no, 0);
		lcdc_lay_enable(fbi, no, 0);
		lcdc_lay_wait_dma_transfer_over(fbi, no);
	}

	lcdc_para_update(fbi);
}

void lcdc_cmd(struct comipfb_info *fbi, unsigned int cmd)
{
	writel(cmd, io_p2v(fbi->res->start + LCDC_IR));
	lcdc_lay_wait_fifo_empty(fbi, 0);
}

void lcdc_cmd_para(struct comipfb_info *fbi, unsigned int data)
{
	writel(data, io_p2v(fbi->res->start + LCDC_CDR));
	lcdc_lay_wait_fifo_empty(fbi, 0);
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

EXPORT_SYMBOL(lcdc_init);
EXPORT_SYMBOL(lcdc_exit);
EXPORT_SYMBOL(lcdc_enable);
EXPORT_SYMBOL(lcdc_suspend);
EXPORT_SYMBOL(lcdc_resume);
EXPORT_SYMBOL(lcdc_start);
EXPORT_SYMBOL(lcdc_layer_config);
EXPORT_SYMBOL(lcdc_layer_enable);
EXPORT_SYMBOL(lcdc_cmd);
EXPORT_SYMBOL(lcdc_cmd_para);
EXPORT_SYMBOL(lcdc_int_enable);
EXPORT_SYMBOL(lcdc_int_clear);
EXPORT_SYMBOL(lcdc_int_status);
