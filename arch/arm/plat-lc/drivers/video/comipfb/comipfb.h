/* driver/video/comip/comipfb.h
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

#ifndef __COMIPFB_H__
#define __COMIPFB_H__

#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/completion.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <plat/comipfb.h>
#include <plat/hardware.h>

#include "comipfb_if.h"
#include "comipfb_dev.h"

#define COMIP_NAME				"COMIP"

/*
 * Minimum X and Y resolutions
 */
#define MIN_XRES				64
#define MIN_YRES				64

#define LCDC_LAYER_INPUT_FORMAT_RGB565		0x1
#define LCDC_LAYER_INPUT_FORMAT_RGB666		0x2
#define LCDC_LAYER_INPUT_FORMAT_RGB888		0x3
#define LCDC_LAYER_INPUT_FORMAT_RGB332		0x4
#define LCDC_LAYER_INPUT_FORMAT_YCRCB422	0x5
#define LCDC_LAYER_INPUT_FORMAT_ARGB8888	0x6
#define LCDC_LAYER_INPUT_FORMAT_ABGR8888	0x7

/* IOCTL */
#define COMIPFB_IOCTL_SET_LAYER_INPUT_FORMAT	_IOW('C', 1, unsigned char)
#define COMIPFB_IOCTL_GET_LAYER_INPUT_FORMAT	_IOR('C', 2, unsigned char)
#define COMIPFB_IOCTL_SET_LAYER_WINDOW		_IOW('C', 3, struct comipfb_window)
#define COMIPFB_IOCTL_GET_LAYER_WINDOW		_IOW('C', 4, struct comipfb_window)
#define COMIPFB_IOCTL_SET_ALPHA			_IOW('C', 5, unsigned char)
#define COMIPFB_IOCTL_GET_ALPHA			_IOR('C', 6, unsigned char)
#define COMIPFB_IOCTL_EN_ALPHA			_IOW('C', 7, unsigned char)
#define COMIPFB_IOCTL_EN_GSCALE			_IOW('C', 8, unsigned char)

/* Only for debug */
#define COMIPFB_IOCTL_EN_LAYER			_IOW('C', 9, unsigned char)
#define COMIPFB_IOCTL_EN_KEYCOLOR		_IOW('C', 10, unsigned char)
#define COMIPFB_IOCTL_SET_KEYCOLOR		_IOW('C', 11, unsigned long)

enum {
	LAYER0 = 0,
	LAYER1,
	LAYER2,
	LAYER_MAX,
};

#define LAYER_INIT				(0x00000001)
#define LAYER_REGISTER				(0x00000002)
#define LAYER_ENABLE				(0x00000004)
#define LAYER_SUSPEND				(0x00000008)

#define LAYER_EXIST				(LAYER_INIT | LAYER_REGISTER)
#define LAYER_WORKING				(LAYER_EXIST | LAYER_ENABLE)

struct comipfb_window {
	int x;
	int y;
	int w;
	int h;
};

struct comipfb_layer_info
{
	/* Must place fb as the first member.
	 * We should use fb_info.par instead */
	struct fb_info		fb;

	unsigned char		no;
	unsigned int		flags;
	atomic_t		refcnt;
	struct comipfb_info	*parent;

	/*
	 * These are the addresses we mapped
	 * the framebuffer memory region to.
	 */
	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char*			map_cpu;	/* virtual */
	u_int			map_size;
	unsigned long	vm_start;	/* mmap virtual */
	u_int			cmap_inverse:1,
					cmap_static:1,
					unused:30;

	/*
	 * Hardware control information
	 */
	unsigned char	pkd_en;
	unsigned char	yuv2rgb_en;
	unsigned char	gscale_en;
	unsigned char	keyc_en;
	unsigned long	key_color;
	unsigned char	alpha_en;
	unsigned char	alpha;
	unsigned char	byte_ex_en;
	unsigned short	input_format;
	unsigned short	win_x_size;
	unsigned short	win_y_size;
	unsigned short	win_x_offset;
	unsigned short	win_y_offset;
	unsigned short	src_width;	/* source image width in pixel */
	unsigned int	buf_addr;	/* current buffer physical address. */
	unsigned int	buf_size;	/* frame buffer size. */
	unsigned int	buf_num;	/* frame buffer number. */

	struct semaphore	sema;
	struct proc_dir_entry	*proc_dir;
};

struct comipfb_info {
	struct device *dev;
	struct clk *clk;
	struct clk *dsi_cfgclk;
	struct clk *dsi_refclk;
	struct clk *lcdc_axi_clk;
	struct resource *res;
	struct resource *interface_res;
	int irq;
	int cntl_id;
	/*
	 * Hardware control information
	 */
	u_char		panel;
	u_int		refresh;
	u_int		pixclock;
	u_int		sync;
	u_int		bpp;
	u_short		xres;
	u_short		yres;
	u_char		left_margin;
	u_char		right_margin;
	u_char		upper_margin;
	u_char		lower_margin;
	u_char		hsync_len;
	u_char		vsync_len;

	/* Main window size */
	u_short		main_win_x_size;
	u_short		main_win_y_size;

	/* Layer contorl */
	u_char		layer_used;
	u_int 		layer_status[LAYER_MAX];
	struct comipfb_layer_info *layers[LAYER_MAX];

	/* Platform data. */
	struct comipfb_if	*cif;
	struct comipfb_dev	*cdev;
	struct comipfb_platform_data *pdata;

	struct semaphore	sema;
	struct completion	wait;
	atomic_t		wait_cnt;

	atomic_t		dma_vailable;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

#endif/*__COMIPFB_H__*/
