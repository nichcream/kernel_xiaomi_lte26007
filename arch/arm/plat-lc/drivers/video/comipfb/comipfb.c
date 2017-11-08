/* driver/video/comip/comipfb.c
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 1999-2015	LeadCoreTech Corp.
**
**	PURPOSE:			This files contains the driver of LCD controller.
**
**	CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	0.1.0		2012-03-10	liuyong		created
**
*/

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/mtd/mtd.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/cacheflush.h>

#include <plat/comip-memory.h>

#include "comipfb.h"
#include "comipfb_hw.h"
#include "comipfb_if.h"

//#define CONFIG_COMIPFB_DEBUG 1
#define BUFFER_NUMS	FB_MEMORY_NUM
#define LOGO_PHY_ADDR	FB_MEMORY_BASE

const unsigned char comipfb_ids[] = {0};
static struct comipfb_info *comipfb_fbi = NULL;
static struct comipfb_info *bl_fbi = NULL;

static inline void layer_mark_flags(struct comipfb_layer_info *layer,
			unsigned int flags)
{
	layer->parent->layer_status[layer->no] |= flags;
}

static inline void layer_unmark_flags(struct comipfb_layer_info *layer,
			unsigned int flags)
{
	layer->parent->layer_status[layer->no] &= ~flags;
}

static inline int layer_check_flags(struct comipfb_layer_info *layer,
			unsigned int flags)
{
	return ((layer->parent->layer_status[layer->no] & flags) == flags);
}

int comipfb_set_backlight(enum led_brightness brightness)
{
	if (!bl_fbi) {
		printk(KERN_ERR "comipfb_set_backlight: bl_fbi = NULL \n");
		return -ENODEV;
	}

	bl_fbi->cif->bl_change(bl_fbi, brightness);

	return 0;
}

EXPORT_SYMBOL_GPL(comipfb_set_backlight);

#ifdef CONFIG_COMIPFB_DEBUG
static void comipfb_layer_dump(struct comipfb_layer_info *layer)
{
	dev_info(layer->fb.dev, "%s layer %d @ %p\n", __func__, layer->no, layer);

	if (layer == NULL)
		return;

	dev_info(layer->fb.dev, "\t fb: %p\n", &layer->fb);
	dev_info(layer->fb.dev, "\t flags: 0x%x\n", layer->flags);
	dev_info(layer->fb.dev, "\t refcnt: 0x%x\n", atomic_read(&layer->refcnt));
	dev_info(layer->fb.dev, "\t parent: %p\n", layer->parent);
	dev_info(layer->fb.dev, "\t map_dma: 0x%x\n", layer->map_dma);
	dev_info(layer->fb.dev, "\t map_cpu: 0x%p\n", layer->map_cpu);
	dev_info(layer->fb.dev, "\t map_size: 0x%x\n", layer->map_size);
	dev_info(layer->fb.dev, "\t vm_start: 0x%lx\n", layer->vm_start);

	dev_info(layer->fb.dev, "\t pkd_en: %d\n", layer->pkd_en);
	dev_info(layer->fb.dev, "\t yuv2rgb_en: %d\n", layer->yuv2rgb_en);
	dev_info(layer->fb.dev, "\t gscale_en: %d\n", layer->gscale_en);
	dev_info(layer->fb.dev, "\t keyc_en: %d\n", layer->keyc_en);
	dev_info(layer->fb.dev, "\t alpha_en: %d\n", layer->alpha_en);
	dev_info(layer->fb.dev, "\t alpha: %d\n", layer->alpha);
	dev_info(layer->fb.dev, "\t byte_ex_en: %d\n", layer->byte_ex_en);
	dev_info(layer->fb.dev, "\t input_format: 0x%x\n", layer->input_format);
	dev_info(layer->fb.dev, "\t win_x_size: %d \n", layer->win_x_size);
	dev_info(layer->fb.dev, "\t win_y_size: %d\n", layer->win_y_size);
	dev_info(layer->fb.dev, "\t win_x_offset: %d\n", layer->win_x_offset);
	dev_info(layer->fb.dev, "\t win_y_offset %d\n", layer->win_y_offset);
	dev_info(layer->fb.dev, "\t src_width %d\n", layer->src_width);
	dev_info(layer->fb.dev, "\t buf_addr 0x%08x\n", layer->buf_addr);
	dev_info(layer->fb.dev, "\t buf_size 0x%08x\n", layer->buf_size);
	dev_info(layer->fb.dev, "\t buf_num %d\n", layer->buf_num);
}

static void comipfb_layer_hw_dump(struct comipfb_layer_info *layer)
{
	struct comipfb_info *fbi;
	dev_info(layer->fb.dev, "%s layer %d @ %p\n", __func__, layer->no, layer);

	fbi = layer->parent;
	if (layer == NULL)
		return;

	dev_info(layer->fb.dev, "\tcontrol: 0x%08x\n", readl(io_p2v(fbi->res->start + 0x94 + layer->no * 0x18)));
	dev_info(layer->fb.dev, "\tStart address 0: 0x%08x \n", readl(io_p2v(fbi->res->start + 0x98 + layer->no * 0x18)));
	dev_info(layer->fb.dev, "\tStart address 1: 0x%08x \n", readl(io_p2v(fbi->res->start + 0x160 + layer->no * 0x4)));
	dev_info(layer->fb.dev, "\tDMA current address: 0x%08x\n", readl(io_p2v(fbi->res->start + 0x12c + layer->no * 0x4)));
	dev_info(layer->fb.dev, "\tKey color: 0x%08x \n", readl(io_p2v(fbi->res->start + 0x9c + layer->no * 0x18)));
	dev_info(layer->fb.dev, "\tWin size: 0x%08x \n", readl(io_p2v(fbi->res->start + 0xA0 + layer->no * 0x18)));
	dev_info(layer->fb.dev, "\tWin_offset: 0x%08x \n", readl(io_p2v(fbi->res->start + 0xA4 + layer->no * 0x18)));
	dev_info(layer->fb.dev, "\tSource wid: 0x%08x \n", readl(io_p2v(fbi->res->start + 0xA8 + layer->no * 0x18)));
}

static void comipfb_lcdc_dump(struct comipfb_info *fbi)
{
	dev_info(fbi->dev, "%s fbi @ %p\n", __func__, fbi);

	if (fbi == NULL)
		return;

	dev_info(fbi->dev, "LCDC_MOD: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_MOD)));
	dev_info(fbi->dev, "LCDC_INTC: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_INTC)));
	dev_info(fbi->dev, "LCDC_INTF: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_INTF)));
	dev_info(fbi->dev, "LCDC_INTE: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_INTE)));
	dev_info(fbi->dev, "LCDC_ASYTIM0: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_ASYTIM0)));
	dev_info(fbi->dev, "LCDC_ASYCTL: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_ASYCTL)));

	dev_info(fbi->dev, "LCDC_HV_CTL: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_CTL)));
	dev_info(fbi->dev, "LCDC_HV_SYNC: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_SYNC)));
	dev_info(fbi->dev, "LCDC_HV_HCTL: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_HCTL)));
	dev_info(fbi->dev, "LCDC_HV_VCTL0: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_VCTL0)));
	dev_info(fbi->dev, "LCDC_HV_VCTL1: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_VCTL1)));
	dev_info(fbi->dev, "LCDC_HV_HS: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_HS)));
	dev_info(fbi->dev, "LCDC_HV_VS: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_VS)));
	dev_info(fbi->dev, "LCDC_HV_F: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_F)));
	dev_info(fbi->dev, "LCDC_HV_VINT: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_VINT)));
	dev_info(fbi->dev, "LCDC_HV_DM: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_HV_DM)));

	dev_info(fbi->dev, "LCDC_MWIN_SIZE: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_MWIN_SIZE)));
	dev_info(fbi->dev, "LCDC_MWIN_BCOLOR: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_MWIN_BCOLOR)));
	dev_info(fbi->dev, "LCDC_DISP_CTRL: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_DISP_CTRL)));
	dev_info(fbi->dev, "LCDC_DMA_STA: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_DMA_STA)));
	dev_info(fbi->dev, "LCDC_FIFO_STATUS: 0x%08x\n",
			readl(io_p2v(fbi->res->start + LCDC_FIFO_STATUS)));
}
#endif

static int comipfb_read_proc_lcdc_info(char *page,
			char **start, off_t off, int count, int *eof, void *data)
{
	char *buf = page;
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)data;
	struct comipfb_info *fbi;
	fbi = layer->parent;
	down(&layer->sema);

	if (!layer_check_flags(layer, LAYER_SUSPEND)) {
	buf += snprintf(buf, PAGE_SIZE, "LCDC_MOD: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_MOD)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_INTC: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_INTC)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_INTF: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_INTF)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_INTE: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_INTE)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_ASYTIM0: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_ASYTIM0)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_ASYCTL: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_ASYCTL)));

	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_CTL: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_CTL)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_SYNC: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_SYNC)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_HCTL: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_HCTL)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_VCTL0: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_VCTL0)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_VCTL1: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_VCTL1)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_HS: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_HS)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_VS: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_VS)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_F: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_F)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_VINT: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_VINT)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_HV_DM: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_HV_DM)));

	buf += snprintf(buf, PAGE_SIZE, "LCDC_MWIN_SIZE: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_MWIN_SIZE)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_MWIN_BCOLOR: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_MWIN_BCOLOR)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_DISP_CTRL: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_DISP_CTRL)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_DMA_STA: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_DMA_STA)));
	buf += snprintf(buf, PAGE_SIZE, "LCDC_FIFO_STATUS: 0x%08x\n",
					readl(io_p2v(fbi->res->start + LCDC_FIFO_STATUS)));
	}

	up(&layer->sema);

	return (buf - page);
}

static int comipfb_read_proc_driver_info(char *page,
			char **start, off_t off, int count, int *eof, void *data)
{
	char *buf = page;
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)data;
	struct comipfb_info *fbi;

	fbi = layer->parent;
	down(&layer->sema);

	if (!layer_check_flags(layer, LAYER_SUSPEND)) {
		buf += snprintf(buf, PAGE_SIZE, "layer %d\n", layer->no);
		buf += snprintf(buf, PAGE_SIZE, " info:\n");

		buf += snprintf(buf, PAGE_SIZE,
				" input_format: 0x%x\n", layer->input_format);
		buf += snprintf(buf, PAGE_SIZE,
				" win_x_size: %d \n", layer->win_x_size);
		buf += snprintf(buf, PAGE_SIZE,
				" win_y_size: %d\n", layer->win_y_size);
		buf += snprintf(buf, PAGE_SIZE,
				" win_x_offset: %d\n", layer->win_x_offset);
		buf += snprintf(buf, PAGE_SIZE,
				" win_y_offset %d\n", layer->win_y_offset);
		buf += snprintf(buf, PAGE_SIZE,
				" src_width %d\n", layer->src_width);
		buf += snprintf(buf, PAGE_SIZE,
				" buf_addr 0x%08x\n", layer->buf_addr);
		buf += snprintf(buf, PAGE_SIZE,
				" buf_size 0x%08x\n", layer->buf_size);
		buf += snprintf(buf, PAGE_SIZE,
				" buf_num %d\n", layer->buf_num);

		buf += snprintf(buf, PAGE_SIZE,
				" map_dma: 0x%08x\n", layer->map_dma);
		buf += snprintf(buf, PAGE_SIZE,
				" map_cpu: 0x%p\n", layer->map_cpu);
		buf += snprintf(buf, PAGE_SIZE,
				" map_size: 0x%08x\n", layer->map_size);
		buf += snprintf(buf, PAGE_SIZE,
				" vm_start: 0x%lx\n", layer->vm_start);

		buf += snprintf(buf, PAGE_SIZE, " pkd_en: %d\n", layer->pkd_en);
		buf += snprintf(buf, PAGE_SIZE,
				" yuv2rgb_en: %d\n", layer->yuv2rgb_en);
		buf += snprintf(buf, PAGE_SIZE,
				" gscale_en: %d\n", layer->gscale_en);
		buf += snprintf(buf, PAGE_SIZE, " keyc_en: %d\n", layer->keyc_en);
		buf += snprintf(buf, PAGE_SIZE, " alpha_en: %d\n", layer->alpha_en);
		buf += snprintf(buf, PAGE_SIZE, " alpha: %d\n", layer->alpha);
		buf += snprintf(buf, PAGE_SIZE,
				" byte_ex_en: %d\n", layer->byte_ex_en);
		buf += snprintf(buf, PAGE_SIZE, " \nregisters:\n");
		buf += snprintf(buf, PAGE_SIZE,
			" control\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0x94 + layer->no * 0x18)));
	buf += snprintf(buf, PAGE_SIZE,
			" start address 0:\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0x98 + layer->no * 0x18)));
	buf += snprintf(buf, PAGE_SIZE,
			" start address 1:\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0x160 + layer->no * 0x4)));
	buf += snprintf(buf, PAGE_SIZE,
			" dma current address:\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0x12c + layer->no * 0x4)));
	buf += snprintf(buf, PAGE_SIZE,
			" key color:\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0x9c + layer->no * 0x18)));
	buf += snprintf(buf, PAGE_SIZE,
			" win size:\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0xA0 + layer->no * 0x18)));
	buf += snprintf(buf, PAGE_SIZE,
			" win offset:\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0xA4 + layer->no * 0x18)));
	buf += snprintf(buf, PAGE_SIZE,
			" source wid:\t 0x%08x\n", readl(io_p2v(fbi->res->start + 0xA8 + layer->no * 0x18)));
	}

	up(&layer->sema);

	return (buf - page);
}


static int comipfb_read_proc_fix_screeninfo(char *page,
			char **start, off_t off, int count, int *eof, void *data)
{
	char *buf;
	struct comipfb_layer_info *layer;
	struct fb_fix_screeninfo *fix;

	buf = page;
	layer = (struct comipfb_layer_info *)data;
	fix = &layer->fb.fix;

	buf += snprintf(buf, PAGE_SIZE, "layer %d\n", layer->no);
	buf += snprintf(buf, PAGE_SIZE, " fix_screeninfo:\n");
	buf += snprintf(buf, PAGE_SIZE, " id: %s\n", fix->id);
	buf += snprintf(buf, PAGE_SIZE, " smem_start: 0x%lx\n", fix->smem_start);
	buf += snprintf(buf, PAGE_SIZE, " smem_len: %d\n", fix->smem_len);
	buf += snprintf(buf, PAGE_SIZE, " type: 0x%x\n", fix->type);
	buf += snprintf(buf, PAGE_SIZE, " type_aux: 0x%x\n", fix->type_aux);
	buf += snprintf(buf, PAGE_SIZE, " visual: 0x%x\n", fix->visual);
	buf += snprintf(buf, PAGE_SIZE, " xpanstep: %d\n", fix->xpanstep);
	buf += snprintf(buf, PAGE_SIZE, " ypanstep: %d\n", fix->ypanstep);
	buf += snprintf(buf, PAGE_SIZE, " ywrapstop: %d\n", fix->ywrapstep);
	buf += snprintf(buf, PAGE_SIZE, " line_length: %d\n", fix->line_length);
	buf += snprintf(buf, PAGE_SIZE, " mmio_start: 0x%lx\n", fix->mmio_start);
	buf += snprintf(buf, PAGE_SIZE, " mmio_len: %d\n", fix->mmio_len);
	buf += snprintf(buf, PAGE_SIZE, " accel: 0x%x\n", fix->accel);

	return (buf - page);
}

static int comipfb_print_fb_bitfield(char *page,
			unsigned long size, char const *name, struct fb_bitfield *color)
{
	char *buf = page;

	buf += snprintf(buf, PAGE_SIZE, " %s", name);
	buf += snprintf(buf, PAGE_SIZE, " \toffset: %d ", color->offset);
	buf += snprintf(buf, PAGE_SIZE, " length: %d ", color->length);
	buf += snprintf(buf, PAGE_SIZE, " msb_right: %d\n", color->msb_right);

	return (buf - page);
}

static int comipfb_read_proc_var_screeninfo(char *page,
			char **start, off_t off, int count, int *eof, void *data)
{
	char *buf;
	struct comipfb_layer_info *layer;
	struct fb_var_screeninfo *var;

	buf = page;
	layer = (struct comipfb_layer_info *)data;
	var = &layer->fb.var;

	buf += snprintf(buf, PAGE_SIZE, "layer %d\n", layer->no);
	buf += snprintf(buf, PAGE_SIZE, " var_screeninfo:\n");
	buf += snprintf(buf, PAGE_SIZE,
			" xres:\t\t%d\tyres:\t\t%d\n", var->xres, var->yres);
	buf += snprintf(buf, PAGE_SIZE,
			" xres_virtual:\t%d\tyres_virtual:\t%d\n",
			var->xres_virtual, var->yres_virtual);
	buf += snprintf(buf, PAGE_SIZE,
			" xoffset:\t%d\tyoffset:\t%d\n", var->xoffset, var->yoffset);
	buf += snprintf(buf, PAGE_SIZE,
			" bits_per_pixel: %d\n", var->bits_per_pixel);
	buf += snprintf(buf, PAGE_SIZE, " grayscale: %d\n", var->grayscale);
	buf += comipfb_print_fb_bitfield(buf, PAGE_SIZE, "red:\t", &var->red);
	buf += comipfb_print_fb_bitfield(buf, PAGE_SIZE, "green:", &var->green);
	buf += comipfb_print_fb_bitfield(buf, PAGE_SIZE, "blue:\t", &var->blue);
	buf += comipfb_print_fb_bitfield(buf, PAGE_SIZE, "transp:", &var->transp);
	buf += snprintf(buf, PAGE_SIZE, " nonstd: 0x%x\n", var->nonstd);
	buf += snprintf(buf, PAGE_SIZE, " activate: 0x%x\n", var->activate);
	buf += snprintf(buf, PAGE_SIZE,
			" height: %d\twidth: %d\n", var->height, var->width);
	buf += snprintf(buf, PAGE_SIZE,
			" accel_flags: 0x%x\n", var->accel_flags);
	buf += snprintf(buf, PAGE_SIZE, " pixclock: %d\n", var->pixclock);
	buf += snprintf(buf, PAGE_SIZE,
			" left_margin: %d\tright_margin: %d\n",
			var->left_margin, var->right_margin);
	buf += snprintf(buf, PAGE_SIZE,
			" upper_margin: %d\tlower_margin: %d\n",
			var->upper_margin, var->lower_margin);
	buf += snprintf(buf, PAGE_SIZE,
			" hsync_len: %d\tvsync_len: %d\n",
			var->hsync_len, var->vsync_len);
	buf += snprintf(buf, PAGE_SIZE, " sync: %d\n", var->sync);
	buf += snprintf(buf, PAGE_SIZE, " vmode: 0x%x\n", var->vmode);
	buf += snprintf(buf, PAGE_SIZE, " rotate: 0x%x\n", var->rotate);

	return (buf - page);
}

static int comipfb_procfs_init(struct comipfb_layer_info *layer)
{
	char path[16];

	snprintf(path, sizeof(path), "comipfb%d", layer->no);
	layer->proc_dir = proc_mkdir(path, NULL);
	if (layer->proc_dir) {
		create_proc_read_entry("lcd_controller", S_IRUGO,
				layer->proc_dir,
				comipfb_read_proc_lcdc_info, layer);
		create_proc_read_entry("layer_info", S_IRUGO,
				layer->proc_dir,
				comipfb_read_proc_driver_info, layer);
		create_proc_read_entry("fix_screeninfo", S_IRUGO,
				layer->proc_dir,
				comipfb_read_proc_fix_screeninfo, layer);
		create_proc_read_entry("var_screeninfo", S_IRUGO,
				layer->proc_dir,
				comipfb_read_proc_var_screeninfo, layer);
	} else
		return -EINVAL;

	return 0;
}

static void comipfb_procfs_remove(struct comipfb_layer_info *layer)
{
	char path[16];

	snprintf(path, sizeof(path), "comipfb%d", layer->no);
	remove_proc_entry("var_screeninfo", layer->proc_dir);
	remove_proc_entry("fix_screeninfo", layer->proc_dir);
	remove_proc_entry("layer_info", layer->proc_dir);
	remove_proc_entry("lcd_controller", layer->proc_dir);
}

static void comipfb_unregister_framebuffers(struct comipfb_info *fbi)
{
	struct comipfb_layer_info *layer;
	unsigned char i;

	for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
		layer = fbi->layers[comipfb_ids[i]];
		if (layer_check_flags(layer, LAYER_REGISTER)) {
			unregister_framebuffer(&layer->fb);
			layer_unmark_flags(layer, LAYER_REGISTER);
		}
	}
}

static int comipfb_register_framebuffers(struct comipfb_info *fbi)
{
	struct comipfb_layer_info *layer;
	unsigned char i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
		layer = fbi->layers[comipfb_ids[i]];
		if (layer_check_flags(layer, LAYER_INIT)) {
			layer_mark_flags(layer, LAYER_REGISTER);
			ret = register_framebuffer(&layer->fb);
			if (ret < 0) {
				layer_unmark_flags(layer, LAYER_REGISTER);
				goto err;
			}
		}
	}

	return 0;

err:
	comipfb_unregister_framebuffers(fbi);
	return ret;
}

static int comipfb_map_video_memory(struct comipfb_layer_info *layer)
{
	layer->map_size = PAGE_ALIGN(layer->fb.fix.smem_len + PAGE_SIZE);
	layer->map_cpu = dma_alloc_writecombine(layer->fb.device,
				layer->map_size, &layer->map_dma, GFP_KERNEL);
	if (layer->map_cpu) {
		/* prevent initial garbage on screen */
		memset(layer->map_cpu, 0x00, layer->map_size);
		layer->fb.screen_base = layer->map_cpu;
		/*
		 * FIXME: this is actually the wrong thing to place in
		 * smem_start.	But fbdev suffers from the problem that
		 * it needs an API which doesn't exist (in this case,
		 * dma_writecombine_mmap)
		 */
		layer->fb.fix.smem_start = layer->map_dma;
	}

	return layer->map_cpu ? 0 : -ENOMEM;
}

static void comipfb_free_video_memory(struct comipfb_layer_info *layer)
{
	dma_free_writecombine(layer->fb.device, layer->map_size, layer->map_cpu, layer->map_dma);
}

static void comipfb_enable_layer(struct comipfb_layer_info *layer)
{
	down(&layer->sema);
	if (!layer_check_flags(layer, LAYER_ENABLE)) {
		if (!layer_check_flags(layer, LAYER_SUSPEND))
			lcdc_layer_enable(layer, 1);
		layer_mark_flags(layer, LAYER_ENABLE);
	}
	up(&layer->sema);
}

static void comipfb_disable_layer(struct comipfb_layer_info *layer)
{
	down(&layer->sema);
	if (layer_check_flags(layer, LAYER_ENABLE)) {
		if (!layer_check_flags(layer, LAYER_SUSPEND))
			lcdc_layer_enable(layer, 0);
		layer_unmark_flags(layer, LAYER_ENABLE);
	}
	up(&layer->sema);
}

static void comipfb_config_layer(struct comipfb_layer_info *layer,
				unsigned int flag)
{
	down(&layer->sema);
	if (!layer_check_flags(layer, LAYER_SUSPEND))
		lcdc_layer_config(layer, flag);
	up(&layer->sema);
}

static void comipfb_init_layer(struct comipfb_layer_info *layer,
				struct fb_var_screeninfo *var)
{
	layer->win_x_size = var->xres;
	layer->win_y_size = var->yres;
	layer->src_width = var->xres;
	layer->buf_addr = layer->map_dma;
	comipfb_config_layer(layer, LCDC_LAYER_CFG_ALL);
}

static void comipfb_get_var(struct fb_var_screeninfo *var,
				struct comipfb_info	*fbi)
{
	var->xres		= fbi->xres;
	var->yres		= fbi->yres;
	var->bits_per_pixel	= fbi->bpp;
	var->pixclock		= 1000000 / (fbi->pixclock / 1000000);
	var->hsync_len		= fbi->hsync_len;
	var->vsync_len		= fbi->vsync_len;
	var->left_margin	= fbi->left_margin;
	var->right_margin	= fbi->right_margin;
	var->upper_margin	= fbi->upper_margin;
	var->lower_margin	= fbi->lower_margin;
	var->sync		= fbi->sync;
	var->vmode		= FB_VMODE_NONINTERLACED;
}

/*
 *	comipfb_check_var():
 *	  Get the video params out of 'var'. If a value doesn't fit, round it up,
 *	  if it's too big, return -EINVAL.
 *
 *	  Round up in the following order: bits_per_pixel, xres,
 *	  yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *	  bitfields, horizontal timing, vertical timing.
 */
static int comipfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;

	if (var->xres < MIN_XRES)
		var->xres = MIN_XRES;
	if (var->yres < MIN_YRES)
		var->yres = MIN_YRES;

	comipfb_get_var(var, layer->parent);

	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres * layer->buf_num;
	/*
	 * Setup the RGB parameters for this display.
	 */
	switch(var->bits_per_pixel) {
	case 16:
		var->red.offset   = 11; var->red.length   = 5;
		var->green.offset = 5;	var->green.length = 6;
		var->blue.offset  = 0;	var->blue.length  = 5;
		var->transp.offset = var->transp.length = 0;
		break;
	case 18:
		var->red.offset   = 12; var->red.length   = 6;
		var->green.offset = 6;	var->green.length = 6;
		var->blue.offset  = 0;	var->blue.length  = 6;
		var->transp.offset = var->transp.length = 0;
	break;
	case 24:
		var->red.offset   = 16; var->red.length   = 8;
		var->green.offset = 8;	var->green.length = 8;
		var->blue.offset  = 0;	var->blue.length  = 8;
		var->transp.offset = var->transp.length = 0;
	break;
	case 32:
		var->red.offset   = 16; var->red.length   = 8;
		var->green.offset = 8;	var->green.length = 8;
		var->blue.offset  = 0;	var->blue.length  = 8;
		var->transp.offset = var->transp.length = 0;
	break;
	default:
		var->red.offset = var->green.offset = \
			var->blue.offset = var->transp.offset = 0;
		var->red.length   = 8;
		var->green.length = 8;
		var->blue.length  = 8;
		var->transp.length = 0;
	}
	return 0;
}

/*
 * comipfb_activate_var():
 *	Configures LCD Controller based on entries in var parameter.  Settings are
 *	only written to the controller if changes were made.
 */
static int comipfb_activate_var(struct fb_var_screeninfo *var,
				struct comipfb_layer_info *layer)
{
	unsigned int address;
	unsigned int offset;
	unsigned int cfg_flag = 0;

	offset = var->yoffset * layer->fb.fix.line_length +
				var->xoffset * var->bits_per_pixel / 8;
	address = layer->map_dma + offset;
	if (layer->buf_addr != address) {
		layer->buf_addr = address;
		cfg_flag |= LCDC_LAYER_CFG_BUF;
		if (layer->parent->pdata->flags & COMIPFB_CACHED_BUFFER)
			dmac_flush_range((void *)(layer->vm_start + offset),
				(void *)(layer->vm_start + offset +
				layer->win_x_size * layer->win_y_size * var->bits_per_pixel / 8));
	}
	if (cfg_flag)
		comipfb_config_layer(layer, cfg_flag);

	return 0;
}

/*
 * comipfb_set_par():
 *	Set the user defined part of the display for the specified console
 */
static int comipfb_set_par(struct fb_info *info)
{
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	struct fb_var_screeninfo *var = &info->var;

	pr_debug("comipfb: set_par\n");

	if (var->bits_per_pixel == 16 ||
		var->bits_per_pixel == 18 ||
		var->bits_per_pixel == 24 ||
		var->bits_per_pixel == 32)
		layer->fb.fix.visual = FB_VISUAL_TRUECOLOR;
	else if (!layer->cmap_static)
		layer->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else {
		/*
		 * Some people have weird ideas about wanting static
		 * pseudocolor maps.  I suspect their user space
		 * applications are broken.
		 */
		layer->fb.fix.visual = FB_VISUAL_STATIC_PSEUDOCOLOR;
	}

	layer->fb.fix.line_length = var->xres_virtual * var->bits_per_pixel / 8;

	if (layer->fb.var.bits_per_pixel == 16 ||
		layer->fb.var.bits_per_pixel == 18 ||
		layer->fb.var.bits_per_pixel == 24 ||
		layer->fb.var.bits_per_pixel == 32)
		fb_dealloc_cmap(&layer->fb.cmap);
	else
		fb_alloc_cmap(&layer->fb.cmap,
			1 << layer->fb.var.bits_per_pixel, 0);

	return 0;
}

static int comipfb_sync_frame(struct comipfb_info *fbi)
{
	unsigned long timeout;

	atomic_inc(&fbi->wait_cnt);

	down(&fbi->sema);
	if (atomic_read(&fbi->wait_cnt) == 1){
		INIT_COMPLETION(fbi->wait);
		timeout = wait_for_completion_timeout(&fbi->wait, HZ / 10);
		if (!timeout && !fbi->wait.done)
			printk(KERN_WARNING "comipfb: Sync frame timeout!\n");
	}
	up(&fbi->sema);

	atomic_dec(&fbi->wait_cnt);

	return 0;
}

static int comipfb_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	struct comipfb_info *fbi = layer->parent;

	comipfb_activate_var(var, layer);

	if (atomic_dec_and_test(&fbi->dma_vailable)) {
		/* Wait for frame end for sync. */
		down(&layer->sema);
		if ((layer->no == comipfb_ids[0])
			&& !layer_check_flags(layer, LAYER_SUSPEND))
			comipfb_sync_frame(layer->parent);
		up(&layer->sema);
	}

	return 0;
}

static int comipfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long start;
	u32 len;

	/* frame buffer memory */
	start = info->fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);
	if (off >= len) {
		/* memory mapped io */
		off -= len;
		if (info->var.accel_flags) {
			mutex_unlock(&info->mm_lock);
			return -EINVAL;
		}
		start = info->fix.mmio_start;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.mmio_len);
	}

	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO | VM_RESERVED;

	layer->vm_start = vma->vm_start;
	if (!(layer->parent->pdata->flags & COMIPFB_CACHED_BUFFER))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			     vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	return 0;

}

static inline u_int comipfb_chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int comipfb_setcolreg(u_int regno, u_int red, u_int green,
			u_int blue, u_int trans, struct fb_info *info)
{
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	unsigned int val;
	int ret = 1;

	/*
	 * If inverse mode was selected, invert all the colours
	 * rather than the register number.  The register number
	 * is what you poke into the framebuffer to produce the
	 * colour you requested.
	 */
	if (layer->cmap_inverse) {
		red	= 0xffff - red;
		green	= 0xffff - green;
		blue	= 0xffff - blue;
	}

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no matter what visual we are using.
	 */
	if (layer->fb.var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;

	switch (layer->fb.fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 16-bit True Colour.	We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u32 *pal = layer->fb.pseudo_palette;

			val = comipfb_chan_to_field(red, &layer->fb.var.red);
			val |= comipfb_chan_to_field(green, &layer->fb.var.green);
			val |= comipfb_chan_to_field(blue, &layer->fb.var.blue);

			pal[regno] = val;
			ret = 0;
		}
		break;

	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		/* CPU haven't support this function. */
		break;
	}

	return ret;
}

#if defined(CONFIG_FBCON_DRAW_PANIC_TEXT)
static int comipfb_blank(int blank, struct fb_info *info)
{
	int ret = 0;
	int i;
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	struct comipfb_info * fbi = layer->parent;

	switch (blank) {
		case FB_BLANK_UNBLANK:
			if (layer_check_flags(layer, LAYER_SUSPEND)) {
				if (fbi->cntl_id == 0) {
					if (fbi->lcdc_axi_clk > 0)
						clk_enable(fbi->lcdc_axi_clk);
					if (fbi->dsi_cfgclk > 0)
						clk_enable(fbi->dsi_cfgclk);
					if (fbi->dsi_refclk > 0)
						clk_enable(fbi->dsi_refclk);
					if (fbi->clk > 0)
						clk_enable(fbi->clk);
				}
				lcdc_resume(fbi);
				for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
					layer = fbi->layers[comipfb_ids[i]];
					layer_unmark_flags(layer, LAYER_SUSPEND);
					if (layer_check_flags(layer, LAYER_WORKING)) {
						lcdc_layer_config(layer, LCDC_LAYER_CFG_ALL);
						lcdc_layer_enable(layer, 1);
					}
				}
				fbi->cif->bl_change(fbi, 0xBE);
				if (unlikely(kpanic_in_progress == 1)) {
					mdelay(150);
				}
				else
					msleep(150);
				if (fbi->pdata->bl_control)
					fbi->pdata->bl_control(1);
			}
		break;
		case FB_BLANK_NORMAL:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_POWERDOWN:
			if (layer_check_flags(layer, LAYER_WORKING)) {
				if (fbi->pdata->bl_control)
					fbi->pdata->bl_control(0);
				for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
					layer = fbi->layers[comipfb_ids[i]];
					lcdc_layer_enable(layer, 0);
					layer_mark_flags(layer, LAYER_SUSPEND);
				}
				lcdc_suspend(fbi);
				if (fbi->cntl_id == 0) {
					if (fbi->clk > 0)
						clk_disable(fbi->clk);
					if (fbi->lcdc_axi_clk > 0)
						clk_disable(fbi->lcdc_axi_clk);
					if (fbi->dsi_cfgclk > 0)
						clk_disable(fbi->dsi_cfgclk);
					if (fbi->dsi_refclk > 0)
						clk_disable(fbi->dsi_refclk);
				}
			}
		break;
		default:
		ret = -EINVAL;
	}

	return ret;

}
#endif

static int comipfb_open(struct fb_info *info, int user)
{
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	int ret = 0;

	if (layer->no > LAYER_MAX) {
		ret = -EBADF;
		goto err;
	}

	if (!layer_check_flags(layer, LAYER_EXIST)) {
		ret = -ENODEV;
		goto err;
	}

	if ((ret = atomic_add_return(1, &layer->refcnt)) > 1)
		return 0;

	comipfb_set_par(info);
	comipfb_init_layer(layer, &info->var);
	if (layer->no == comipfb_ids[0])
		comipfb_enable_layer(layer);

	return 0;

err:
	return ret;
}

static int comipfb_release(struct fb_info *info, int user)
{
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	int ret;

	if (layer->no > LAYER_MAX)
		return -EBADF;

	if (!layer_check_flags(layer, LAYER_EXIST))
		return -ENODEV;

	if ((ret = atomic_sub_return(1, &layer->refcnt)))
		return 0;

	comipfb_disable_layer(layer);

	return 0;
}

static int comipfb_ioctl(struct fb_info *info, unsigned cmd,
						unsigned long arg)
{
	struct comipfb_window window;
	struct comipfb_layer_info *layer = (struct comipfb_layer_info *)info;
	unsigned long __user *argp = (void __user *)arg;
	unsigned char update = 0;
	int ret = 0;

	/* The the fb_info.locl are locked before
	 * It guarentee the serial access
	 */

	switch (cmd) {
	case COMIPFB_IOCTL_SET_ALPHA:
		if(arg > 255)
			arg = 255;
		layer->alpha = arg;
		update = 1;
		break;
	case COMIPFB_IOCTL_GET_ALPHA:
		if(put_user(layer->alpha, argp)) {
			ret = -EACCES;
			dev_info(info->dev,
				"Failing in puting alpha value to user\n");
			goto fail;
		}
		break;
	case COMIPFB_IOCTL_EN_ALPHA:
		if ((arg != 0) && (arg != 1)) {
			ret = -EINVAL;
			dev_info(info->dev,
				"Invalid opaque enable value = %ld"
				"expect 0 or 1\n", arg);
			goto fail;
		}
		layer->alpha_en = arg;
		update = 1;
		break;
	case COMIPFB_IOCTL_SET_LAYER_INPUT_FORMAT:
		switch (arg) {
		case LCDC_LAYER_INPUT_FORMAT_RGB332:
			if((layer->no == 1) || (layer->no == 2)) {
				ret = -EINVAL;
				dev_info(info->dev,
					"Layer 1 and 2 do not support"
					" RGB332 format.\n");
				goto fail;
			}
		case LCDC_LAYER_INPUT_FORMAT_YCRCB422:
			if(!((layer->no == 1) || (layer->no == 2))) {
				ret = -EINVAL;
				dev_info(info->dev,
					"Only Layer 1 and 2 support"
					" YCRCB422 format.\n");
				goto fail;
			}
		case LCDC_LAYER_INPUT_FORMAT_RGB565:
		case LCDC_LAYER_INPUT_FORMAT_RGB666:
		case LCDC_LAYER_INPUT_FORMAT_RGB888:
		case LCDC_LAYER_INPUT_FORMAT_ARGB8888:
		case LCDC_LAYER_INPUT_FORMAT_ABGR8888:
			break;
		}
		layer->input_format = arg;
		update = 1;
		break;
	case COMIPFB_IOCTL_GET_LAYER_INPUT_FORMAT:
		if(put_user(layer->input_format, argp)) {
			ret = -EACCES;
			dev_info(info->dev,
				"Failing in puting alpha value to user\n");
			goto fail;
		}
		break;
	case COMIPFB_IOCTL_SET_LAYER_WINDOW:
		if(copy_from_user((void __user *)&window, (void __user *)argp, sizeof(window))) {
			ret = -EACCES;
			dev_info(info->dev,
					"Failing in get window information"
					"from user\n");
			goto fail;
		}

		layer->win_x_offset = (window.x / 2) * 2;
		layer->win_y_offset = window.y;
		layer->win_x_size = window.w;
		layer->win_y_size = window.h;
		layer->src_width = window.w;
		update = 1;
		break;
	case COMIPFB_IOCTL_GET_LAYER_WINDOW:
		window.x = layer->win_x_offset;
		window.y = layer->win_y_offset;
		window.w = layer->win_x_size;
		window.h = layer->win_y_size;

		if(copy_to_user(argp, (void *)&window, sizeof(window))) {
			ret = -EACCES;
			dev_info(info->dev,
					"Failing in put window information"
					"to user\n");
			goto fail;
		}
		break;
	case COMIPFB_IOCTL_EN_GSCALE:
		if ((layer->no != 1) && (layer->no != 2)) {
			ret = -EINVAL;
			dev_info(info->dev,
					"Layer %d does not support gscale mode,"
					"only layer 1 and layer 2 support\n",
					layer->no);
			goto fail;
		}
		if ((arg != 0) && (arg != 1)) {
			ret = -EINVAL;
			dev_info(info->dev,
					"0 - Diable, 1 - Enable, other - invalid\n");
			goto fail;
		}
		layer->gscale_en = arg;
		update = 1;
		break;
	case COMIPFB_IOCTL_EN_LAYER:
		if (arg == 0)
			comipfb_disable_layer(layer);
		else
			comipfb_enable_layer(layer);
		break;
	case COMIPFB_IOCTL_EN_KEYCOLOR:
		if ((arg != 0) && (arg != 1)) {
			ret = -EINVAL;
			dev_info(info->dev,
					"0 - Diable, 1 - Enable, other - invalid\n");
			goto fail;
		}
		layer->keyc_en = arg;
		update = 1;
		break;
	case COMIPFB_IOCTL_SET_KEYCOLOR:
		layer->key_color = arg;
		update = 1;
		break;
	default:
		ret = -EINVAL;
		dev_info(info->dev, "Unspported cmd %d\n", cmd);
		goto fail;
	}

	if (update)
		comipfb_config_layer(layer, LCDC_LAYER_CFG_ALL);

	return 0;

fail:
	return ret;
}

static struct fb_ops comipfb_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= comipfb_open,
	.fb_release	= comipfb_release,
	.fb_ioctl	= comipfb_ioctl,
	.fb_check_var	= comipfb_check_var,
	.fb_set_par	= comipfb_set_par,
	.fb_setcolreg	= comipfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_pan_display	= comipfb_pan_display,
	.fb_mmap	= comipfb_mmap,
#if defined(CONFIG_FBCON_DRAW_PANIC_TEXT)
	.fb_blank	= comipfb_blank,
#endif
};

static irqreturn_t comipfb_handle_irq(int irq, void *dev_id)
{
	struct comipfb_info *fbi = (struct comipfb_info *)dev_id;
	unsigned int status;

	/* Get interrupt status. */
	status = lcdc_int_status(fbi);

	/* Clear and disable the interrupt. */
	lcdc_int_clear(fbi, status);
	//lcdc_int_enable(fbi, status, 0);

	/* Frame interrupt. */
	if ((status & (1 << LCDC_FTE_INTR)) && atomic_read(&fbi->wait_cnt) == 1)
		complete(&fbi->wait);

	if (atomic_read(&fbi->dma_vailable) < (BUFFER_NUMS - 1))
		atomic_inc(&fbi->dma_vailable);

	return IRQ_HANDLED;
}

static int comipfb_hw_release(struct comipfb_info *fbi)
{
	lcdc_exit(fbi);

	if (fbi->pdata->power)
		fbi->pdata->power(0);

	return 0;
}

static int comipfb_hw_init(struct comipfb_info *fbi)
{
	if (fbi->pdata->power)
		fbi->pdata->power(1);

	/* Initialize LCD controller */
	lcdc_init(fbi);

	return 0;
}

static void comipfb_layer_release(struct comipfb_layer_info *layer)
{
	comipfb_free_video_memory(layer);
	comipfb_procfs_remove(layer);
}

static int comipfb_layer_init(struct comipfb_layer_info *layer,
				struct comipfb_info *fbi)
{
	atomic_set(&layer->refcnt, 0);
	sema_init(&layer->sema, 1);

	INIT_LIST_HEAD(&layer->fb.modelist);
	layer->fb.device = fbi->dev;
	layer->fb.fbops = &comipfb_ops;
	layer->fb.flags = FBINFO_DEFAULT;
	layer->fb.node = -1;
	layer->fb.pseudo_palette = (void *)layer
				+ sizeof(struct comipfb_layer_info);

	strcpy(layer->fb.fix.id, COMIP_NAME);
	layer->fb.fix.type = FB_TYPE_PACKED_PIXELS;
	layer->fb.fix.type_aux = 0;
	layer->fb.fix.xpanstep = 0;
	layer->fb.fix.ypanstep = 1;
	layer->fb.fix.ywrapstep = 0;
	layer->fb.fix.accel = FB_ACCEL_NONE;

	layer->win_x_size = fbi->main_win_x_size;
	layer->win_y_size = fbi->main_win_y_size;
	layer->src_width = fbi->main_win_x_size;
	layer->alpha = 255; // 255 = solid 0 = transparent
	layer->alpha_en = 0;
	layer->input_format = LCDC_LAYER_INPUT_FORMAT_ARGB8888;
	layer->buf_num = BUFFER_NUMS;

	comipfb_get_var(&layer->fb.var, fbi);
	layer->fb.var.xres_virtual = fbi->xres;
	layer->fb.var.yres_virtual = fbi->yres * layer->buf_num;
	layer->fb.var.xoffset = 0;
	layer->fb.var.yoffset = 0;
	layer->fb.var.nonstd = 0;
	layer->fb.var.activate = FB_ACTIVATE_NOW;
	layer->fb.var.width = fbi->cdev->width;
	layer->fb.var.height = fbi->cdev->height;
	layer->fb.var.accel_flags = 0;

	layer->flags = 0;
	layer->cmap_inverse = 0;
	layer->cmap_static = 0;

	if (fbi->cdev->bpp <= 16 ) {
		/* 8, 16 bpp */
		layer->buf_size = fbi->cdev->xres *
			fbi->cdev->yres * fbi->cdev->bpp / 8;
	} else {
		/* 18, 32 bpp*/
		layer->buf_size = fbi->cdev->xres * fbi->cdev->yres * 4;
	}

	layer->fb.fix.smem_len = layer->buf_size * layer->buf_num;
	if (comipfb_map_video_memory(layer)) {
		dev_err(fbi->dev, "Fail to allocate video RAM\n");
		return -ENOMEM;
	}

	layer->buf_addr = layer->map_dma;

	return 0;
}

static void comipfb_fbinfo_release(struct comipfb_info *fbi)
{
	struct comipfb_layer_info *layer;
	unsigned char i;

	for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
		layer = fbi->layers[comipfb_ids[i]];
		if (layer) {
			comipfb_layer_release(layer);
			kfree(layer);
		}
	}
}

static int comipfb_fbinfo_init(struct device *dev, struct comipfb_info *fbi)
{
	struct comipfb_platform_data *pdata = dev->platform_data;
	struct comipfb_layer_info *layer;
	struct comipfb_dev *cdev;
	unsigned char i;
	
	fbi->dev = dev;
	fbi->pdata = pdata;
	cdev = comipfb_dev_get(fbi, 0, 1);
	if (!cdev)
		return -EINVAL;

	fbi->cdev = cdev;
	fbi->cif = comipfb_if_get(fbi);
	fbi->main_win_x_size = cdev->xres;
	fbi->main_win_y_size = cdev->yres;
	fbi->xres = cdev->xres;
	fbi->yres = cdev->yres;
	fbi->panel = 0;
	fbi->refresh = cdev->refresh;
	fbi->pixclock = cdev->pclk;
	fbi->bpp = cdev->bpp;
	fbi->sync = 0;

	clk_set_rate(fbi->clk, fbi->pixclock);
	fbi->pixclock = clk_get_rate(fbi->clk);
	printk("fbi->pixclock = %d\n", fbi->pixclock);
	clk_enable(fbi->clk);

	switch(cdev->interface_info) {
		case COMIPFB_MPU_IF: 
			fbi->hsync_len = 0;
			fbi->vsync_len = 0;
			fbi->left_margin = 0;
			fbi->right_margin = 0;
			fbi->upper_margin = 0;
			fbi->lower_margin = 0;
		break;
		case COMIPFB_RGB_IF:
			if (cdev->flags & COMIPFB_PCLK_HIGH_ACT)
				fbi->sync = FB_SYNC_COMP_HIGH_ACT;
			if (cdev->flags & COMIPFB_HSYNC_HIGH_ACT)
				fbi->sync = FB_SYNC_HOR_HIGH_ACT;
			if (cdev->flags & COMIPFB_VSYNC_HIGH_ACT)
				fbi->sync = FB_SYNC_VERT_HIGH_ACT;
			fbi->hsync_len = cdev->timing.rgb.hsync;
			fbi->vsync_len = cdev->timing.rgb.vsync;
			fbi->left_margin = cdev->timing.rgb.hbp;
			fbi->right_margin = cdev->timing.rgb.hfp;
			fbi->upper_margin = cdev->timing.rgb.vbp;
			fbi->lower_margin = cdev->timing.rgb.vfp;
		break;
		case COMIPFB_MIPI_IF:
			if (cdev->flags & COMIPFB_PCLK_HIGH_ACT)
				fbi->sync = FB_SYNC_COMP_HIGH_ACT;
			if (cdev->flags & COMIPFB_HSYNC_HIGH_ACT)
				fbi->sync = FB_SYNC_HOR_HIGH_ACT;
			if (cdev->flags & COMIPFB_VSYNC_HIGH_ACT)
				fbi->sync = FB_SYNC_VERT_HIGH_ACT;
			fbi->hsync_len = cdev->timing.mipi.hsync;
			fbi->vsync_len = cdev->timing.mipi.vsync;
			fbi->left_margin = cdev->timing.mipi.hbp;
			fbi->right_margin = cdev->timing.mipi.hfp;
			fbi->upper_margin = cdev->timing.mipi.vbp;
			fbi->lower_margin = cdev->timing.mipi.vfp;
		break;
		case COMIPFB_HDMI_IF:
			//TODO 
		break;	
		default:
		break;
	}

	atomic_set(&fbi->wait_cnt, 0);
	atomic_set(&fbi->dma_vailable, (BUFFER_NUMS - 1));
	init_completion(&fbi->wait);
	sema_init(&fbi->sema, 1);

	for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
		layer = kzalloc(
			sizeof(struct comipfb_layer_info) + sizeof(u32) * 16,
			GFP_KERNEL);
		if (!layer)
			goto failed;

		if (comipfb_layer_init(layer, fbi) < 0)
			goto failed;

		layer->no = comipfb_ids[i];
		layer->parent = fbi;
		layer_mark_flags(layer, LAYER_INIT);
		fbi->layers[layer->no] = layer;
		comipfb_procfs_init(layer);
	}

	return 0;

failed:
	comipfb_fbinfo_release(fbi);

	return -EINVAL;
}

static int comipfb_read_image(struct comipfb_layer_info *layer)
{
	unsigned int __iomem *logbase;

	logbase = ioremap(LOGO_PHY_ADDR, layer->buf_size);
	if (logbase == NULL) {
		printk(KERN_ERR "*comipfb logo addr ioremap failed*\n");
		return -ENOMEM;
	}
	memcpy(layer->map_cpu, logbase, layer->buf_size);
	iounmap(logbase);
	layer->buf_addr = layer->map_dma;
	comipfb_config_layer(layer, LCDC_LAYER_CFG_BUF);

	return 0;
}

static void comipfb_show_logo(struct comipfb_info *fbi)
{
	struct comipfb_layer_info *layer = fbi->layers[comipfb_ids[0]];
	int ret;

	comipfb_fbi = fbi;
	ret = comipfb_read_image(layer);
	if (ret == 0) {
		/* Get logo data. */
		comipfb_open(&layer->fb, 0);
	}
#ifdef CONFIG_COMIPFB_DEBUG
	/* Dump all registers. */
	comipfb_layer_dump(layer);
	comipfb_layer_hw_dump(layer);
	comipfb_lcdc_dump(fbi);
#endif
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void comipfb_early_suspend(struct early_suspend *h)
{
	struct comipfb_info *fbi =
		container_of(h, struct comipfb_info, early_suspend);
	struct comipfb_layer_info *layer;
	unsigned char i;
	printk(KERN_DEBUG "**enter %s **\n",__func__);
	for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
		layer = fbi->layers[comipfb_ids[i]];
		down(&layer->sema);
		if (layer_check_flags(layer, LAYER_WORKING)) {
			lcdc_layer_enable(layer, 0);
			layer_mark_flags(layer, LAYER_SUSPEND);
		}
		up(&layer->sema);
	}

	lcdc_suspend(fbi);
	if (fbi->cntl_id == 0) {
		if (fbi->clk > 0)
			clk_disable(fbi->clk);
		if (fbi->lcdc_axi_clk > 0)
			clk_disable(fbi->lcdc_axi_clk);
		if (fbi->dsi_cfgclk > 0)
			clk_disable(fbi->dsi_cfgclk);
		if (fbi->dsi_refclk > 0)
			clk_disable(fbi->dsi_refclk);
	}
	if (fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF) {
		if (fbi->pdata->gpio_rst >= 0) {
			gpio_request(fbi->pdata->gpio_rst, "LCD Reset");
			gpio_direction_output(fbi->pdata->gpio_rst, 0);
			gpio_free(fbi->pdata->gpio_rst);
		}

		if (fbi->pdata->power)
			fbi->pdata->power(0);
	}
	printk(KERN_DEBUG "**exit %s **\n",__func__);
}

static void comipfb_late_resume(struct early_suspend *h)
{
	struct comipfb_info *fbi =
		container_of(h, struct comipfb_info, early_suspend);
	struct comipfb_layer_info *layer;
	unsigned char i;
	printk(KERN_DEBUG "**enter %s **\n",__func__);
	if ((fbi->pdata->flags & COMIPFB_SLEEP_POWEROFF) && fbi->pdata->power)
		fbi->pdata->power(1);

	if (fbi->cntl_id == 0) {
		if (fbi->lcdc_axi_clk > 0)
			clk_enable(fbi->lcdc_axi_clk);
		if (fbi->dsi_cfgclk > 0)
			clk_enable(fbi->dsi_cfgclk);
		if (fbi->dsi_refclk > 0)
			clk_enable(fbi->dsi_refclk);
		if (fbi->clk > 0)
			clk_enable(fbi->clk);
	}

	lcdc_resume(fbi);

	for (i = 0; i < ARRAY_SIZE(comipfb_ids); i++) {
		layer = fbi->layers[comipfb_ids[i]];
		down(&layer->sema);
		if (layer_check_flags(layer, LAYER_SUSPEND)) {
			layer_unmark_flags(layer, LAYER_SUSPEND);
			if (layer_check_flags(layer, LAYER_WORKING)) {
				lcdc_layer_config(layer, LCDC_LAYER_CFG_ALL);
				lcdc_layer_enable(layer, 1);
			}
		}
		up(&layer->sema);
	}
	msleep(100);
	printk(KERN_DEBUG "**exit %s **\n",__func__);
}
#endif

static int comipfb_probe(struct platform_device *dev)
{
	struct comipfb_info *fbi = NULL;
	struct comipfb_platform_data *prdata;
	struct resource *r;
	struct resource *r1;
	struct clk *hdmi_clk;
	int irq;
	int ret;
	char clk_name[12];

	dev_info(&dev->dev, "comipfb_probe\n");
	if (!dev->dev.platform_data) {
		dev_err(&dev->dev, "Platform data not set.\n");
		return -EINVAL;
	}
	
	prdata = dev->dev.platform_data;
	fbi = kzalloc(sizeof(struct comipfb_info), GFP_KERNEL);
	if (!fbi)
		return -EINVAL;
	fbi->interface_res = NULL;
	r = platform_get_resource(dev, IORESOURCE_MEM, 0);
	fbi->res = r;
	r1 = platform_get_resource(dev, IORESOURCE_MEM, 1);
	if (r1 != NULL)
		fbi->interface_res = r1;
	
	irq = platform_get_irq(dev, 0);
	if (!r || irq < 0) {
		dev_err(&dev->dev, "Invalid platform data.\n");
		kfree(fbi);
		return -ENXIO;
	}
	fbi->irq = irq;
	fbi->cntl_id = dev->id;
	
	snprintf(clk_name, sizeof(clk_name), "lcdc%d_clk", fbi->cntl_id);	
	fbi->clk = clk_get(&dev->dev, clk_name);
	if (IS_ERR(fbi->clk)) {
		ret = PTR_ERR(fbi->clk);
		goto failed;
	}
	ret = comipfb_fbinfo_init(&dev->dev, fbi);
	if (ret < 0) {
		dev_err(&dev->dev, "Failed to initialize framebuffer device\n");
		goto failed_clk_get;
	}

	fbi->cif = &comipfb_if_mipi;

	if (!fbi->cdev ||
		!fbi->cif ||
		!fbi->cif->init ||
		!fbi->cif->exit ||
		!fbi->cif->suspend ||
		!fbi->cif->resume) {
		dev_err(&dev->dev, "Invalid interface or device\n");
		ret = -EINVAL;
		goto failed_check_val;
	}

	if (!fbi->xres ||
		!fbi->yres ||
		!fbi->pixclock ||
		!fbi->bpp) {
		dev_err(&dev->dev, "Invalid resolution or bit depth\n");
		ret = -EINVAL;
		goto failed_check_val;
	}

	dev_info(&dev->dev, "got a %dx%dx%d LCD(%s)\n",
		fbi->xres, fbi->yres, fbi->bpp, fbi->cdev->name);

	/*diable hdmi clock*/
	hdmi_clk = clk_get(NULL, "hdmi_pix_clk");
	if (hdmi_clk > 0) {
		clk_enable(hdmi_clk);
		clk_disable(hdmi_clk);
		clk_put(hdmi_clk);
	}
	/*enable lcdc0 clock*/
	fbi->lcdc_axi_clk = clk_get(NULL, "lcdc_axi_clk");
	if (IS_ERR(fbi->lcdc_axi_clk)) {
		ret = PTR_ERR(fbi->lcdc_axi_clk);
		goto failed_lcdc_axi_clk;
	}
	if (prdata->lcdcaxi_clk)
		clk_set_rate(fbi->lcdc_axi_clk, prdata->lcdcaxi_clk);
	clk_enable(fbi->lcdc_axi_clk);

	if (!fbi->refresh)
		fbi->refresh =  fbi->pixclock / (fbi->xres * fbi->yres);

	if (fbi->cntl_id == 0) {
		bl_fbi = fbi;
		fbi->dsi_refclk = clk_get(NULL, "dsi_ref_clk");
		if (IS_ERR(fbi->dsi_refclk)) {
			ret = PTR_ERR(fbi->dsi_refclk);
			goto failed_clk_dsi_ref_get;
		}
		clk_enable(fbi->dsi_refclk);

		fbi->dsi_cfgclk = clk_get(NULL, "dsi_cfg_clk");
		if (IS_ERR(fbi->dsi_cfgclk)) {
			ret = PTR_ERR(fbi->dsi_cfgclk);
			goto failed_clk_dsi_cfg_get;
		}
		clk_enable(fbi->dsi_cfgclk);
	}

	ret = request_irq(irq, comipfb_handle_irq,
		IRQF_DISABLED, "comipfb framebuffer", fbi);
	if (ret) {
		dev_err(&dev->dev, "request_irq failed: %d\n", ret);
		ret = -EBUSY;
		goto failed_request_irq;
	}

	comipfb_hw_init(fbi);
	platform_set_drvdata(dev, fbi);
	ret = comipfb_register_framebuffers(fbi);
	if (ret) {
		dev_err(&dev->dev, "comipfb_register_framebuffers failed: %d\n", ret);
		ret = -EBUSY;
		goto failed_register_framebuffers;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	fbi->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	fbi->early_suspend.suspend = comipfb_early_suspend;
	fbi->early_suspend.resume = comipfb_late_resume;
	register_early_suspend(&fbi->early_suspend);
#endif

	lcdc_int_clear(fbi, (1 << LCDC_FTE_INTR));
	lcdc_int_enable(fbi, (1 << LCDC_FTE_INTR), 1);

	if (prdata->main_lcdc_flag == 1)
		comipfb_show_logo(fbi);
	msleep(20);
	printk(KERN_DEBUG "*lcd init finish*\n");

	return 0;

failed_register_framebuffers:
	free_irq(irq, fbi);
failed_request_irq:
	if (fbi->dsi_cfgclk > 0) {
		clk_disable(fbi->dsi_cfgclk);
		clk_put(fbi->dsi_cfgclk);
	}
failed_clk_dsi_cfg_get:
	if (fbi->dsi_refclk > 0) {
		clk_disable(fbi->dsi_refclk);
		clk_put(fbi->dsi_refclk);
	}
failed_clk_dsi_ref_get:
	if (fbi->lcdc_axi_clk > 0) {
		clk_disable(fbi->lcdc_axi_clk);
		clk_put(fbi->lcdc_axi_clk);
	}
failed_lcdc_axi_clk:
failed_check_val:
	comipfb_fbinfo_release(fbi);
	platform_set_drvdata(dev, NULL);
failed_clk_get:
	clk_disable(fbi->clk);
	clk_put(fbi->clk);
failed:
	kfree(fbi);
	return ret;
}

static int comipfb_remove(struct platform_device *dev)
{
	struct comipfb_info *fbi = platform_get_drvdata(dev);

	comipfb_unregister_framebuffers(fbi);
	comipfb_hw_release(fbi);
	comipfb_fbinfo_release(fbi);

	free_irq(fbi->irq, fbi);

	if (fbi->dsi_cfgclk > 0) {
		clk_disable(fbi->dsi_cfgclk);
		clk_put(fbi->dsi_cfgclk);
	}
	if (fbi->dsi_refclk > 0) {
		clk_disable(fbi->dsi_refclk);
		clk_put(fbi->dsi_refclk);
	}
	if (fbi->clk > 0) {
		clk_disable(fbi->clk);
		clk_put(fbi->clk);
	}
	if (fbi->lcdc_axi_clk > 0) {
		clk_disable(fbi->lcdc_axi_clk);
		clk_put(fbi->lcdc_axi_clk);
	}
	kfree(fbi);

	return 0;
}

static struct platform_driver comipfb_driver = {
	.probe		= comipfb_probe,
	.remove		= comipfb_remove,
	.driver		= {
		.name	= "comipfb",
	},
};

static int __devinit comipfb_init(void)
{
	return platform_driver_register(&comipfb_driver);
}

static void __exit comipfb_cleanup(void)
{	
	platform_driver_unregister(&comipfb_driver);
}

fs_initcall(comipfb_init);
module_exit(comipfb_cleanup);

MODULE_AUTHOR("liuyong <liuyong4@leadcoretech.com>");
MODULE_DESCRIPTION("loadable framebuffer driver for COMIP");
MODULE_LICENSE("GPL");
