#ifndef __COMIP_ISP2_H__
#define __COMIP_ISP2_H__

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <plat/i2c.h>
#include <plat/camera.h>
#include <plat/hardware.h>
#include <plat/comip-v4l2.h>
#include <plat/mfp.h>

/* ISP notify types. */
#define ISP_NOTIFY_DATA_START			(1 << 0)//(0x00000001)
#define ISP_NOTIFY_DATA_DONE			(1 << 1)//(0x00000002)
#define ISP_NOTIFY_DROP_FRAME			(1 << 2)//(0x00000004)
#define ISP_NOTIFY_OVERFLOW				(1 << 3)//(0x00000008)
#define ISP_NOTIFY_SOF					(1 << 4)//(0x00000010)
#define ISP_NOTIFY_EOF					(1 << 5)//(0x00000020)
#define ISP_NOTIFY_AEC_DONE				(1 << 6)//(0x00000040)
#define ISP_NOTIFY_AGC_DONE				(1 << 7)


#define ISP_PROCESS_RAW				(0x1)
#define ISP_PROCESS_YUV				(0x2)
#define ISP_PROCESS_OFFLINE_YUV			(0x3)
#define ISP_PROCESS_HDR_YUV			(0x4)

/* ISP clock number. */
#define ISP_CLK_NUM				(5)

#define ISP_REG_END			0xffff

struct isp_device;

struct isp_gpio_flash {
	enum camera_led_mode mode;
	int onoff;
	int brightness;
};

struct isp_buffer {
	unsigned long paddr;
};

struct isp_format {
	unsigned int width;
	unsigned int height;
	unsigned int dev_width;
	unsigned int dev_height;
	unsigned int code;
	unsigned int fourcc;
	struct v4l2_fmt_data *fmt_data;
};

struct isp_capture {
	int snapshot;
	int snap_flag;
	int process;
	int save_raw;
	int load_raw;
	int vts;
	struct isp_buffer buf;
	struct comip_camera_client *client;
};

struct isp_prop {
	int index;
	int if_id;
	int if_pip;
	int mipi;
	int mipi_lane_num;
	struct v4l2_isp_setting setting;
	struct v4l2_isp_parm *parm;
	struct v4l2_isp_firmware_setting firmware_setting;
	struct comip_camera_aecgc_control *aecgc_control;
};

struct isp_parm {
	int contrast;
	int effects;
	int flicker;
	int anti_shake;
	int brightness;
	int flash_mode;
	int focus_mode;
	int meter_mode;
	int iso;
	int exposure;
	int saturation;
	int scene_mode;
	int sharpness;
	int white_balance;
	int zoom;
	int hflip;
	int vflip;
	int frame_rate;
	int in_width;
	int in_height;
	int in_format;
	int out_width;
	int out_height;
	int out_format;
	int crop_x;
	int crop_y;
	int crop_width;
	int crop_height;
	int ratio_d;
	int ratio_dcw;
	int dowscale_flag;
	int dcw_flag;
	int meter_set_flag;
	int focus_rect_set_flag;
	int pre_framerate;
	int sna_framerate;
	int pre_vts;
	int sna_vts;
	int real_vts;
	int sensor_binning;
	unsigned int min_gain;
	unsigned int max_gain;
	unsigned int auto_min_gain;
	unsigned int auto_max_gain;
	struct v4l2_rect meter_rect;
	struct v4l2_rect focus_rect;
};

struct isp_effect_ops {
	int (*get_exposure)(void **, int);
	int (*get_iso)(void **, int);
	int (*get_contrast)(void **, int);
	int (*get_saturation)(void **, int);
	int (*get_white_balance)(void **, int);
	int (*get_brightness)(void **, int);
	int (*get_sharpness)(void **, int);
	int (*get_vcm_range)(void **, int);
	int (*get_aecgc_win_setting)(int, int, int, void **);
};

struct isp_exp_table {
	unsigned int   exp_table_enable;
	unsigned char  vts_gain_ctrl_1;
	unsigned char  vts_gain_ctrl_2;
	unsigned char  vts_gain_ctrl_3;
	unsigned short gain_ctrl_1;
	unsigned short gain_ctrl_2;
	unsigned short gain_ctrl_3;
};

struct isp_ops {
	int (*init)(struct isp_device *, void *);
	int (*release)(struct isp_device *, void *);
	int (*open)(struct isp_device *, struct isp_prop *);
	int (*close)(struct isp_device *, struct isp_prop *);
	int (*config)(struct isp_device *, void *);
	int (*suspend)(struct isp_device *, void *);
	int (*resume)(struct isp_device *, void *);
	int (*mclk_on)(struct isp_device *, void *);
	int (*mclk_off)(struct isp_device *, void *);
	int (*start_capture)(struct isp_device *, struct isp_capture *);
	int (*stop_capture)(struct isp_device *, void *);
	int (*enable_capture)(struct isp_device *, struct isp_buffer *);
	int (*disable_capture)(struct isp_device *, void *);
	int (*update_buffer)(struct isp_device *, struct isp_buffer *);
	int (*check_fmt)(struct isp_device *, struct isp_format *);
	int (*try_fmt)(struct isp_device *, struct isp_format *);
	int (*pre_fmt)(struct isp_device *, struct isp_prop *, struct isp_format *);
	int (*s_fmt)(struct isp_device *, struct isp_format *);
	int (*s_ctrl)(struct isp_device *, struct v4l2_control *);
	int (*g_ctrl)(struct isp_device *, struct v4l2_control *);
	int (*s_parm)(struct isp_device *, struct v4l2_streamparm *);
	int (*g_parm)(struct isp_device *, struct v4l2_streamparm *);
	int (*set_autofocus)(struct isp_device *, struct v4l2_rect *);
	int (*set_metering)(struct isp_device *, struct v4l2_rect *);
	int (*set_camera_flash)(struct isp_device *isp, struct isp_gpio_flash *);
	int (*get_effect_func)(struct isp_device *isp, struct v4l2_subdev *);
	int (*set_bandfilter_enbale)(struct isp_device *isp, int);
	int (*set_exp_table)(struct isp_device * isp, struct isp_exp_table *);
	int (*set_aecgc_enable)(struct isp_device * isp, int);
};

struct isp_i2c_cmd {
	unsigned int flags;
#define I2C_CMD_READ		0x0001
#define I2C_CMD_ADDR_16BIT	0x0002
#define I2C_CMD_DATA_16BIT	0x0004
	unsigned char addr;
	unsigned short reg;
	unsigned short data;
};

struct isp_i2c {
	struct mutex lock;
	struct i2c_adapter adap;
	struct comip_i2c_platform_data *pdata;
	int (*xfer_cmd)(struct isp_device *, struct isp_i2c_cmd *);
};

struct isp_debug{
	int settingfile_loaded;
};

struct isp_device {
	struct device *dev;
	struct resource *res;
	void __iomem *base;
	struct isp_ops *ops;
	struct isp_i2c i2c;
	struct isp_parm parm;
	struct completion completion;
	struct completion raw_completion;
	int clk_enable[ISP_CLK_NUM];
	struct clk *clk[ISP_CLK_NUM];
	struct v4l2_fmt_data fmt_data;
	struct comip_camera_client *client;
	struct comip_camera_platform_data *pdata;
	struct comip_camera_anti_shake_parms *anti_shake_parms;
	struct comip_camera_flash_parms *flash_parms;
	struct v4l2_isp_parm *sd_isp_parm;	/* sub-device isp parameters */
	struct comip_camera_aecgc_control *aecgc_control;
	struct isp_effect_ops *effect_ops;
	int (*irq_notify)(unsigned int, void *);
	void *data;
	spinlock_t lock;
	unsigned char intr;
	unsigned char mac_intr_l;
	unsigned char mac_intr_h;
	int snapshot;
	int snap_flag;
	int running;
	int poweron;
	int boot;
	int input;
	int irq;
	int if_pip;
	bool pp_buf;
	struct isp_buffer buf_start;
	int first_init;
	struct isp_debug debug;
	u8 hdr[8];
	int flash;
	int process;
	int save_raw;
	int load_raw;
	int sna_exp_mode;
	atomic_t isp_mac_disable;
	unsigned int shutter_speed;
	unsigned int sna_exp;
	unsigned int sna_gain;
	unsigned int sna_flash_exp_ratio;
};

#define isp_dev_call(isp, f, args...)				\
	(!(isp) ? -ENODEV : (((isp)->ops && (isp)->ops->f) ?	\
		(isp)->ops->f((isp) , ##args) : -ENOIOCTLCMD))

#if 0 
static inline unsigned char isp_reg_readb(struct isp_device *isp,
		unsigned int offset)
{
	return readb(isp->base + offset);
}
#else
static inline unsigned char isp_reg_readb(struct isp_device *isp,
		unsigned int offset)
{
	unsigned int real_offset = 0;

	if (offset >= 0x50000)
		return readb(isp->base + offset);
	else {
		real_offset = (offset & 0xfffffffc) + (3 - (offset & 3));
		return readb(isp->base + real_offset);
	}
}
#endif

static inline unsigned int isp_reg_readl(struct isp_device *isp,
		unsigned int offset)
{
		return (((unsigned int)isp_reg_readb(isp, offset) << 24)
			| ((unsigned int)isp_reg_readb(isp, offset + 1) << 16)
			| ((unsigned int)isp_reg_readb(isp, offset + 2) << 8)
			| (unsigned int)isp_reg_readb(isp, offset + 3));
}

static inline unsigned short isp_reg_readw(struct isp_device *isp,
		unsigned int offset)
{
		return (((unsigned short)isp_reg_readb(isp, offset) << 8)
			| (unsigned short)isp_reg_readb(isp, offset + 1));
}

#if 0 
static inline void isp_reg_writeb(struct isp_device *isp,
		unsigned char value, unsigned int offset)
{
	writeb(value, isp->base + offset);
}
#else
static inline void isp_reg_writeb(struct isp_device *isp,
		unsigned char value, unsigned int offset)
{
	unsigned int real_offset = 0;

	if (offset >= 0x50000) {
		writeb(value, isp->base + offset);
	}
	else {
		real_offset = (offset & 0xfffffffc) + (3 - (offset & 3));
		writeb(value, isp->base + real_offset);
	}
}
#endif

static inline void isp_reg_writel(struct isp_device *isp,
		unsigned int value, unsigned int offset)
{
	isp_reg_writeb(isp, (value >> 24) & 0xff, offset);
	isp_reg_writeb(isp, (value >> 16) & 0xff, offset + 1);
	isp_reg_writeb(isp, (value >> 8) & 0xff,  offset + 2);
	isp_reg_writeb(isp, (value & 0xff),       offset + 3);
}

static inline void isp_reg_writew(struct isp_device *isp,
		unsigned short value, unsigned int offset)
{
	isp_reg_writeb(isp, (value >> 8) & 0xff, offset);
	isp_reg_writeb(isp, value & 0xff, offset + 1);
}

extern int isp_device_init(struct isp_device* isp);
extern int isp_device_release(struct isp_device* isp);

#endif/*__COMIP_ISP2_H__*/
