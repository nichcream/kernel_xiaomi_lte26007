
#include "comip-isp.h"
#include <mach/devices.h>
#include "comip-csi-dev.h"
#include "isp-i2c.h"
#include "isp-ctrl.h"
#include "isp-firmware.h"
#include "camera-debug.h"
#include "comip-video.h"

//#define COMIP_ISP_DEBUG
#ifdef COMIP_ISP_DEBUG
#define ISP_ERR(fmt, args...) printk(KERN_ERR "[ISP]" fmt, ##args)
#else
#define ISP_ERR(fmt, args...) printk(KERN_DEBUG "[ISP]" fmt, ##args)
#endif
#define ISP_PRINT(fmt, args...) printk(KERN_DEBUG "[ISP]" fmt, ##args)


//#define ISP_STATIC_FOCUS_RECT_SIZE

/* Timeouts. */
#define ISP_BOOT_TIMEOUT	(3000) /* ms. */
#define ISP_I2C_TIMEOUT		(3000) /* ms. */
#define ISP_ZOOM_TIMEOUT	(3000) /* ms. */
#define ISP_FORMAT_TIMEOUT	(3000) /* ms. */
#define ISP_CAPTURE_TIMEOUT	(8000) /* ms. */
#define ISP_OFFLINE_TIMEOUT	(8000) /* ms. */
#define ISP_HDR_TIMEOUT	(3000) /* ms. */

/* Clock flags. */
#define ISP_CLK_MAIN		(0x00000001)
#define ISP_CLK_CSI		(0x00000002)
#define ISP_CLK_DEV		(0x00000004)
#define ISP_CLK_ALL		(0xffffffff)

/* CCLK divider. */
#if defined (CONFIG_CPU_LC1860)
#define ISP_CCLK_DIVIDER	(0x40)
#else
#define ISP_CCLK_DIVIDER	(0x04)
#endif

#define CENTER_WIDTH  500
#define CENTER_HEIGHT 380
#define CENTER_LEFT   -250
#define CENTER_TOP   -190

#define DENOISE_GAIN_THRESHOLD 0x20


#define ISP_BRACKET_RATIO_1		(0x0a00)
#define ISP_BRACKET_RATIO_2		(0x06)

static int isp_update_buffer(struct isp_device *isp, struct isp_buffer *buf);
static int isp_get_zoom_ratio(struct isp_device *isp, int zoom);
static int isp_autofocus_result(struct isp_device *isp);
static int isp_full_hdr_process(struct isp_device *isp, unsigned long short_addr,unsigned long long_addr, unsigned long target_addr);
static int isp_focus_is_active(struct isp_device *isp);
static int isp_set_metering(struct isp_device *isp, struct v4l2_rect *spot);
static int isp_set_center_metering(struct isp_device *isp);
static int isp_set_central_weight_metering(struct isp_device *isp);
static int isp_set_matrix_metering(struct isp_device *isp);
static int isp_set_autofocus_area(struct isp_device *isp, struct v4l2_rect *spot);
static int isp_vb2_buffer_done(struct isp_device * isp);
static int isp_set_light_meters(struct isp_device * isp,int val);
static void saturation_work_handler(struct work_struct *work);
static void lens_threshold_work_handler(struct work_struct *work);


struct isp_clk_info {
	const char* name;
	unsigned long rate;
	unsigned long flags;
};

struct isp_regval_list {
	unsigned int   reg_num;
	unsigned char  value;
};

static struct isp_clk_info isp_clks[ISP_CLK_NUM] = {
	{"isp_cphy_cfg_clk",	26000000,  ISP_CLK_CSI},
	{"isp_axi_clk", 	156000000,  ISP_CLK_CSI},
	{"isp_p_sclk",		156000000,  ISP_CLK_MAIN | ISP_CLK_DEV},
	{"isp_sclk2",		156000000,  ISP_CLK_MAIN},
	{"isp_hclk",		200000000, ISP_CLK_MAIN},
};

static struct isp_clk_info isp_clks_middle[ISP_CLK_NUM] = {
	{"isp_cphy_cfg_clk",	26000000,  ISP_CLK_CSI},
	{"isp_axi_clk", 	208000000,  ISP_CLK_CSI},
	{"isp_p_sclk",		208000000,  ISP_CLK_MAIN | ISP_CLK_DEV},
	{"isp_sclk2",		208000000,  ISP_CLK_MAIN},
	{"isp_hclk",		200000000, ISP_CLK_MAIN},
};

static struct isp_clk_info isp_clks_high[ISP_CLK_NUM] = {
	{"isp_cphy_cfg_clk",	26000000,  ISP_CLK_CSI},
	{"isp_axi_clk", 	312000000,  ISP_CLK_CSI},//624
	{"isp_p_sclk",		312000000,  ISP_CLK_MAIN | ISP_CLK_DEV},
	{"isp_sclk2",		312000000,  ISP_CLK_MAIN},
	{"isp_hclk",		200000000, ISP_CLK_MAIN},
};

static struct isp_clk_info isp_clks_hhigh[ISP_CLK_NUM] = {
	{"isp_cphy_cfg_clk",	26000000,  ISP_CLK_CSI},
	{"isp_axi_clk", 	416000000,  ISP_CLK_CSI},
	{"isp_p_sclk",		416000000,  ISP_CLK_MAIN | ISP_CLK_DEV},
	{"isp_sclk2",		416000000,  ISP_CLK_MAIN},
	{"isp_hclk",		200000000, ISP_CLK_MAIN},
};
const struct isp_regval_list default_isp_regs_init[] = {
	{0x60100, 0x01},//Software reset
	{0x6301b, 0xf0},//isp clock enable
	{0x63025, 0x40},//clock divider
	{0x63c12, 0x01},//data type
	{0x63c13, 0x22},//divider
	{0x63c14, 0x01},//men_thre
	{0x63c15, 0x53},//mem valid high&low number

	/* ISP TOP REG */
	{0x65000, 0x3f},
	{0x65001, 0x6f},//turn off local boost
	{0x65002, 0x9b},
	{0x65003, 0xff},
	{0x65004, 0x21},//turn on EDR
	{0x65005, 0x52},
	{0x65006, 0x02},
	{0x65008, 0x00},
	{0x65009, 0x00},
	{0x63023, ISP_CCLK_DIVIDER},
};



static DEFINE_SPINLOCK(isp_suspend_lock);

static int camera_isp_flash(struct isp_device *isp, struct isp_gpio_flash *flash_parm)
{
	return 0;
}

static int isp_int_mask(struct isp_device *isp, unsigned char mask)
{
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask) {
		isp->intr &= ~mask;
		isp_reg_writeb(isp, isp->intr, REG_ISP_INT_EN);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static int isp_int_unmask(struct isp_device *isp, unsigned char mask)
{
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask) {
		isp->intr |= mask;
		isp_reg_writeb(isp, isp->intr, REG_ISP_INT_EN);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static unsigned char isp_int_state(struct isp_device *isp)
{
	return isp_reg_readb(isp, REG_ISP_INT_STAT);
}

static int isp_mac_int_mask(struct isp_device *isp, unsigned short mask)
{
	unsigned char mask_l = mask & 0x00ff;
	unsigned char mask_h = (mask >> 8) & 0x00ff;
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask_l) {
		isp->mac_intr_l &= ~mask_l;
		isp_reg_writeb(isp, isp->mac_intr_l, REG_ISP_MAC_INT_EN_L);
	}
	if (mask_h) {
		isp->mac_intr_h &= ~mask_h;
		isp_reg_writeb(isp, isp->mac_intr_h, REG_ISP_MAC_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static int isp_mac_int_unmask(struct isp_device *isp, unsigned short mask)
{
	unsigned char mask_l = mask & 0x00ff;
	unsigned char mask_h = (mask >> 8) & 0x00ff;
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask_l) {
		isp->mac_intr_l |= mask_l;
		isp_reg_writeb(isp, isp->mac_intr_l, REG_ISP_MAC_INT_EN_L);
	}
	if (mask_h) {
		isp->mac_intr_h |= mask_h;
		isp_reg_writeb(isp, isp->mac_intr_h, REG_ISP_MAC_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static unsigned short isp_mac_int_state(struct isp_device *isp)
{
	unsigned short state_l;
	unsigned short state_h;

	state_l = isp_reg_readb(isp, REG_ISP_MAC_INT_STAT_L);
	state_h = isp_reg_readb(isp, REG_ISP_MAC_INT_STAT_H);

	return (state_h << 8) | state_l;
}

static int isp_wait_cmd_done(struct isp_device *isp, unsigned long timeout)
{
	unsigned long tm;
	int ret = 0;

	tm = wait_for_completion_timeout(&isp->completion,
					msecs_to_jiffies(timeout));
	if (!tm && !isp->completion.done) {
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int isp_send_cmd(struct isp_device *isp, unsigned char id,
				unsigned long timeout)
{
	int ret;

	INIT_COMPLETION(isp->completion);

	isp_int_unmask(isp, MASK_INT_CMDSET);
	isp_reg_writeb(isp, id, COMMAND_REG0);

	/* Wait for command set done interrupt. */
	ret = isp_wait_cmd_done(isp, timeout);

	isp_int_mask(isp, MASK_INT_CMDSET);

	return ret;
}

static int isp_set_address(struct isp_device *isp,
			unsigned int id, unsigned int addr)
{
	unsigned int reg = id ? REG_BASE_ADDR4 : REG_BASE_ADDR0;
	unsigned int uv_reg = id ? REG_BASE_ADDR5 : REG_BASE_ADDR1;
	unsigned int uv_addr = 0;

	isp_reg_writel(isp, addr, reg);

	if ((isp->parm.out_format == V4L2_PIX_FMT_NV12) || (isp->parm.out_format == V4L2_PIX_FMT_NV21)) {
		uv_addr = addr + isp->parm.out_width * isp->parm.out_height;
		isp_reg_writel(isp, uv_addr, uv_reg);
	}

	return 0;
}

static int isp_set_byteswitch(struct isp_device *isp, unsigned char value)
{
	isp_reg_writeb(isp, (value << 2), REG_ISP_SWITCH_CTRL);
	return 0;
}

static int isp_calc_zoom(struct isp_device *isp , int zoom_ratio)
{
	int crop_w;
	int crop_h;
	int crop_x;
	int crop_y;
	int downscale_w;
	int downscale_h;
	int dcw_w;
	int dcw_h;
	int Wi;
	int Wo;
	int Hi;
	int Ho;
	int ratio_h;
	int ratio_w;
	int ratio_gross;
	int ratio;
	int ratio_dcw;
	int dratio = 0;
	int ratio_d = 0;
	int dcwFlag;
	int downscaleFlag;
	int w_dcw;
	int h_dcw;
	int i;
	int j;
	int zoom_0;
	int crop_width;
	int crop_height;
	int in_width;
	int in_height;
	int out_height;
	int out_width;
	int final_crop_width;
	int final_crop_height;
	int crop_x0;
	int crop_y0;
	int ret = 1;
	int t1;
	int t2;

	out_width = isp->parm.out_width;
	out_height = isp->parm.out_height;
	in_width = isp->parm.in_width;
	in_height = isp->parm.in_height;

	zoom_0 = isp_get_zoom_ratio(isp, ZOOM_LEVEL_0);

	crop_width = (zoom_0 * in_width) / zoom_ratio;
	crop_height = (zoom_0 * in_height) / zoom_ratio;

	if (((crop_width * 1000) / crop_height)
			<= ((out_width * 1000) / out_height)) {
		final_crop_width = crop_width;
		final_crop_width = final_crop_width & 0xfffe;
		final_crop_height = (final_crop_width * out_height) / out_width;
		final_crop_height = final_crop_height & 0xfffe;
	} else {
		final_crop_height = crop_height;
		final_crop_height = final_crop_height & 0xfffe;
		final_crop_width = (final_crop_height * out_width) / out_height;
		final_crop_width = final_crop_width & 0xfffe;
	}

	crop_x0 = (in_width - final_crop_width) / 2;
	crop_y0 = (in_height - final_crop_height) / 2;

	Wo = isp->parm.out_width;
	Ho = isp->parm.out_height;
	Wi = final_crop_width;
	Hi = final_crop_height;

	crop_w = crop_h = crop_x = crop_y = downscale_w
		= downscale_h = dcw_w = dcw_h = 0;
	ratio_h = (Hi * 1000 / Ho);
	ratio_w = (Wi * 1000 / Wo);
	ratio_gross = (ratio_h >= ratio_w) ? ratio_w : ratio_h;
	ratio = ratio_gross / 1000;

	for (i = 0; i < 4; i++)
		if((1 << i) <= ratio && (1 << (i + 1)) > ratio)
			break;

	if(i == 4)
		i--;

	dcw_w = dcw_h = i;
	ratio_dcw = i;
	if (dcw_w == 0)
		dcwFlag = 0;
	else
		dcwFlag = 1;

	h_dcw = (1 << i) * Ho;
	w_dcw = (1 << i) * Wo;

	downscale_w = (256 * w_dcw + Wi) / (2 * Wi);
	downscale_h = (256 * h_dcw + Hi) / (2 * Hi);
	dratio = (downscale_w >= downscale_h) ? downscale_w : downscale_h;

	if (dratio == 128)
		downscaleFlag = 0;
	else {
		downscaleFlag = 1;
		dratio += 1;
	}

	crop_w = (256 * w_dcw + dratio) / (2 * dratio);
	crop_h = (256 * h_dcw + dratio) / (2 * dratio);
	crop_w = crop_w & 0xfffe;
	crop_h = crop_h & 0xfffe;

	for (j = -3; j <= 3; j++) {
		crop_w = (256 * w_dcw + (dratio + j)) / (2 * (dratio + j));
		crop_h = (256 * h_dcw + (dratio + j))/(2 * (dratio + j));
		crop_w = crop_w & 0xfffe;
		crop_h = crop_h & 0xfffe;

		for(i = 0; i <= 4; i += 2) {
			t1 = (crop_w + i) * (dratio + j) / 128;
			t2 = (crop_h + i) * (dratio + j) / 128;
			if((t1 & 0xfffe) == t1 && t1 >= w_dcw && (t2 & 0xfffe) == t2
					&& t2 >= h_dcw && (dratio + j) >= 64
					&& (dratio + j) <= 128 && (crop_w +i)<= Wi
					&& (crop_h+i) <= Hi) {
				ret = 0;
				break;
			}
		}

		if (ret == 0)
			break;
	}

	if (j == 4)
		j--;

	if (i == 6)
		i = i - 2;

	crop_w += i;
	crop_h += i;
	dratio += j;

	crop_x = (Wi - crop_w) / 2;
	crop_y = (Hi - crop_h) / 2;

	ratio_d = dratio;

	isp->parm.ratio_d = ratio_d;
	isp->parm.ratio_dcw = ratio_dcw;
	isp->parm.crop_width = crop_w;
	isp->parm.crop_height = crop_h;

	isp->parm.crop_x = crop_x + crop_x0;
	isp->parm.crop_y = crop_y + crop_y0;
	isp->parm.dcw_flag = dcwFlag;
	isp->parm.dowscale_flag = downscaleFlag;

	//make sure crop x & y can be divided by 2
	isp->parm.crop_x &= 0xfffffffe;
	isp->parm.crop_y &= 0xfffffffe;

	return 0;
}

static int isp_calc_exposure_ratio(struct isp_device *isp, unsigned short *exposure_ratio)
{
	unsigned int brightness_ratio = 0;
	unsigned short default_exposure_ratio = 0x100;
#if 0
	struct isp_parm *iparm = &(isp->parm);

	if (!iparm->pre_vts || !iparm->pre_framerate) {
		ISP_ERR("isp_calc_exposure_ratio: invalid parameter\n");
		return -EINVAL;
	}

	*snapshot_exposure_ratio = 100 * (iparm->sensor_binning * iparm->sna_vts *
		iparm->sna_framerate) / (iparm->pre_vts * iparm->pre_framerate) * 0x100;
	*snapshot_exposure_ratio /= 100;

	if (*snapshot_exposure_ratio)
		*preview_exposure_ratio = 0x100 * 0x100 / (*snapshot_exposure_ratio);
	else
#endif

	brightness_ratio = isp->flash_parms->brightness_ratio;

	if(isp->fmt_data.exp_ratio)
		default_exposure_ratio = isp->fmt_data.exp_ratio;
	else {
		if(isp->snapshot)
			default_exposure_ratio = isp->sd_isp_parm->default_snapshot_exposure_ratio;
		else
			default_exposure_ratio = isp->sd_isp_parm->default_preview_exposure_ratio;
	}

	if(brightness_ratio) {
		if(isp->sna_exp_mode == SNA_NORMAL_FLASH){
			if(isp->snapshot)
				*exposure_ratio =  (default_exposure_ratio * 10)/brightness_ratio;
			else
				*exposure_ratio =  (brightness_ratio * default_exposure_ratio)/10;
		} else {
			*exposure_ratio = default_exposure_ratio;
		}
	}else {
		*exposure_ratio = default_exposure_ratio;
	}

	return 0;
}


static int isp_set_parameters(struct isp_device *isp)
{
	struct comip_camera_client *client = isp->client;
	struct isp_parm *iparm = &isp->parm;
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_aecgc_control *aecgc_control = isp->aecgc_control;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	unsigned char iformat;
	unsigned char oformat;
	u8 func_control = 0x0;
	u32 snap_paddr;
	u32 snap_uv_paddr = 0;
	unsigned short exposure_ratio = 0;
	unsigned int band_value_50Hz = 0;
	unsigned int band_value_60Hz = 0;
	u8 af_contrl =0x0;
	int zoom_ratio = 0x100;
	int ret = 0;

	if (isp->process == ISP_PROCESS_OFFLINE_YUV)
		snap_paddr = (u32)camdev->offline.paddr;
	else if (isp->process == ISP_PROCESS_HDR_YUV)
		snap_paddr = (u32)camdev->bracket.first_frame.paddr;
	else
		snap_paddr = isp->buf_start.paddr;

	if (iparm->in_format == V4L2_MBUS_FMT_YUYV8_2X8)
		iformat = IFORMAT_YUV422;
	else
		iformat = IFORMAT_RAW10;

	if ((iparm->out_format == V4L2_PIX_FMT_NV21) || (iparm->out_format == V4L2_PIX_FMT_NV12))
		oformat = OFORMAT_NV12;
	else
		oformat = OFORMAT_YUV422;

	if (isp->snapshot &&
			(isp->process == ISP_PROCESS_RAW ||
			isp->process == ISP_PROCESS_OFFLINE_YUV ||
			isp->process == ISP_PROCESS_HDR_YUV)) {
		oformat = OFORMAT_RAW10;
		iformat &= (~ISP_PROCESS);
	} else
		iformat |= ISP_PROCESS;

	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		if (client->if_id == 1)
			iformat |= SENSOR_SECONDARY_MIPI;
		else
			iformat |= SENSOR_PRIMARY_MIPI;
	}

	if(isp->snapshot){
		isp_reg_writeb(isp, (snap_paddr >> 24) & 0xff, ISP_BASE_ADDR_LEFT);
		isp_reg_writeb(isp, (snap_paddr >> 16) & 0xff, ISP_BASE_ADDR_LEFT + 1);
		isp_reg_writeb(isp, (snap_paddr >> 8) & 0xff, ISP_BASE_ADDR_LEFT + 2);
		isp_reg_writeb(isp, (snap_paddr >> 0) & 0xff, ISP_BASE_ADDR_LEFT + 3);
	}

	if (oformat == OFORMAT_NV12) {
		snap_uv_paddr = snap_paddr + iparm->out_width * iparm->out_height;
		isp_reg_writel(isp, snap_uv_paddr, ISP_BASE_ADDR_NV12_UV_L);
		isp_reg_writew(isp, iparm->out_width / 2, ISP_MAC_UV_WIDTH);
	}

	isp_reg_writeb(isp, iformat, ISP_INPUT_FORMAT);
	isp_reg_writeb(isp, oformat, ISP_OUTPUT_FORMAT);

	isp_reg_writeb(isp, (iparm->in_width >> 8) & 0xff,
				SENSOR_OUTPUT_WIDTH);
	isp_reg_writeb(isp, iparm->in_width & 0xff,
				SENSOR_OUTPUT_WIDTH + 1);
	isp_reg_writeb(isp, (iparm->in_height >> 8) & 0xff,
				SENSOR_OUTPUT_HEIGHT);
	isp_reg_writeb(isp, iparm->in_height & 0xff,
				SENSOR_OUTPUT_HEIGHT + 1);

	isp_reg_writeb(isp, (iparm->in_width >> 8) & 0xff,
				ISP_INPUT_WIDTH);
	isp_reg_writeb(isp, iparm->in_width & 0xff,
				ISP_INPUT_WIDTH + 1);
	isp_reg_writeb(isp, (iparm->in_height >> 8) & 0xff,
				ISP_INPUT_HEIGHT);
	isp_reg_writeb(isp, iparm->in_height & 0xff,
				ISP_INPUT_HEIGHT + 1);

	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_START);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_START + 1);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_START);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_START + 1);

	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_SENSOR_START);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_SENSOR_START + 1);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_SENSOR_START);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_SENSOR_START + 1);

	if (isp->snapshot && (isp->process == ISP_PROCESS_RAW
			|| isp->process == ISP_PROCESS_OFFLINE_YUV
			|| isp->process == ISP_PROCESS_HDR_YUV)) {
		isp_reg_writeb(isp, (iparm->in_width >> 8) & 0xff, ISP_OUTPUT_WIDTH);
		isp_reg_writeb(isp, iparm->in_width & 0xff, ISP_OUTPUT_WIDTH + 1);
		isp_reg_writeb(isp, (iparm->in_height >> 8) & 0xff,	ISP_OUTPUT_HEIGHT);
		isp_reg_writeb(isp, iparm->in_height & 0xff, ISP_OUTPUT_HEIGHT + 1);
		isp_reg_writeb(isp, (iparm->in_width >> 8) & 0xff, MAC_MEMORY_WIDTH);
		isp_reg_writeb(isp, iparm->in_width & 0xff, MAC_MEMORY_WIDTH + 1);
	} else {
		isp_reg_writeb(isp, (iparm->out_width >> 8) & 0xff, ISP_OUTPUT_WIDTH);
		isp_reg_writeb(isp, iparm->out_width & 0xff, ISP_OUTPUT_WIDTH + 1);
		isp_reg_writeb(isp, (iparm->out_height >> 8) & 0xff, ISP_OUTPUT_HEIGHT);
		isp_reg_writeb(isp, iparm->out_height & 0xff, ISP_OUTPUT_HEIGHT + 1);
		isp_reg_writeb(isp, (iparm->out_width >> 8) & 0xff, MAC_MEMORY_WIDTH);
		isp_reg_writeb(isp, iparm->out_width & 0xff, MAC_MEMORY_WIDTH + 1);
	}

	if (camdev->load_preview_raw)
		comip_debugtool_preview_load_raw_file(camdev, COMIP_DEBUGTOOL_LOAD_PREVIEW_RAW_FILENAME);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_START_3D);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_START_3D + 1);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_START_3D);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_START_3D + 1);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_SENSOR_START_3D);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_H_SENSOR_START_3D + 1);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_SENSOR_START_3D);
	isp_reg_writeb(isp, 0x00, ISP_INPUT_V_SENSOR_START_3D + 1);

	isp_reg_writeb(isp, 0x00, ISP_INPUT_MODE);

	if((isp->process == ISP_PROCESS_HDR_YUV) && (isp->snapshot)) {
		zoom_ratio = isp_get_zoom_ratio(isp, ZOOM_LEVEL_0);
		isp_set_saturation_hdr(isp, isp->parm.saturation);
	}
	else {
		zoom_ratio = isp_get_zoom_ratio(isp, isp->parm.zoom);
		isp_set_saturation(isp, isp->parm.saturation);
	}
	isp_calc_zoom(isp, zoom_ratio);

	if ((iparm->crop_width != iparm->in_width)
			|| (iparm->crop_height != iparm->in_height))
		func_control |= FUNCTION_YUV_CROP;
	if (iparm->crop_width > iparm->out_width) {
		func_control |= FUNCTION_SCALE_DOWN;
		isp_reg_writeb(isp, func_control, ISP_FUNCTION_CTRL);
		isp_reg_writeb(isp, iparm->ratio_dcw, ISP_SCALE_DOWN_H_RATIO1);
		isp_reg_writeb(isp,  iparm->ratio_d, ISP_SCALE_DOWN_H_RATIO2);
		isp_reg_writeb(isp, iparm->ratio_dcw, ISP_SCALE_DOWN_V_RATIO1);
		isp_reg_writeb(isp,   iparm->ratio_d, ISP_SCALE_DOWN_V_RATIO2);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_UP_H_RATIO);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_UP_H_RATIO + 1);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_UP_V_RATIO);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_UP_V_RATIO + 1);
	} else if (iparm->crop_width < iparm->out_width) {
		func_control |= FUNCTION_SCALE_UP;
		isp_reg_writeb(isp, func_control, ISP_FUNCTION_CTRL);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_DOWN_H_RATIO1);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_DOWN_H_RATIO2);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_DOWN_V_RATIO1);
		isp_reg_writeb(isp, 0x00, ISP_SCALE_DOWN_V_RATIO2);
		isp_reg_writeb(isp, (((iparm->crop_width * 0x100)
			/ iparm->out_width) >> 8) & 0xff, ISP_SCALE_UP_H_RATIO);
		isp_reg_writeb(isp, (((iparm->crop_width * 0x100)
			/ iparm->out_width) >> 0) & 0xff, ISP_SCALE_UP_H_RATIO + 1);
		isp_reg_writeb(isp, (((iparm->crop_height * 0x100)
			/ iparm->out_height) >> 8) & 0xff, ISP_SCALE_UP_V_RATIO);
		isp_reg_writeb(isp, (((iparm->crop_height * 0x100)
			/ iparm->out_height) >> 0) & 0xff, ISP_SCALE_UP_V_RATIO + 1);
	} else
		isp_reg_writeb(isp, func_control | FUNCTION_NO_SCALE, ISP_FUNCTION_CTRL);

	isp_reg_writeb(isp, (iparm->crop_x >> 8) & 0xff,
				ISP_YUV_CROP_H_START);
	isp_reg_writeb(isp, iparm->crop_x & 0xff,
				ISP_YUV_CROP_H_START + 1);
	isp_reg_writeb(isp, (iparm->crop_y >> 8) & 0xff,
				ISP_YUV_CROP_V_START);
	isp_reg_writeb(isp, iparm->crop_y & 0xff,
				ISP_YUV_CROP_V_START + 1);
	isp_reg_writeb(isp, (iparm->crop_width >> 8) & 0xff,
				ISP_YUV_CROP_WIDTH);
	isp_reg_writeb(isp, iparm->crop_width & 0xff,
				ISP_YUV_CROP_WIDTH + 1);
	isp_reg_writeb(isp, (iparm->crop_height >> 8) & 0xff,
				ISP_YUV_CROP_HEIGHT);
	isp_reg_writeb(isp, iparm->crop_height & 0xff,
				ISP_YUV_CROP_HEIGHT + 1);
	if (csd->yuv)
		return 0;

	if(!isp->snapshot)
		isp_set_aecgc_win(isp, iparm->in_width, iparm->in_height, iparm->meter_mode);

	ret = isp_set_light_meters(isp,iparm->meter_mode);
	if (ret)
		ISP_ERR("set light meters error .\n");

	if (iparm->iso == ISO_AUTO) {
		isp_reg_writew(isp, iparm->auto_max_gain, ISP_MAX_GAIN);
		isp_reg_writew(isp, iparm->auto_min_gain, ISP_MIN_GAIN);
		iparm->max_gain = iparm->auto_max_gain;
		iparm->min_gain = iparm->auto_min_gain;
	}else {
		isp_reg_writew(isp, iparm->max_gain, ISP_MAX_GAIN);
		isp_reg_writew(isp, iparm->min_gain, ISP_MIN_GAIN);
	}
	if (aecgc_control && aecgc_control->get_vts_sub_maxexp) {
		isp_reg_writew(isp,
			(iparm->real_vts - aecgc_control->get_vts_sub_maxexp(isp->sd_isp_parm->sensor_vendor, isp->sd_isp_parm->sensor_main_type)),
			ISP_MAX_EXPOSURE);
	}
	else
		isp_reg_writew(isp, (iparm->real_vts - 0x20), ISP_MAX_EXPOSURE);
	isp_reg_writew(isp, isp->sd_isp_parm->min_exposure, ISP_MIN_EXPOSURE);
	isp_reg_writew(isp, iparm->real_vts, ISP_VTS);

	isp_calc_exposure_ratio(isp, &exposure_ratio);

	if (isp->snapshot) {
		band_value_50Hz = (isp->parm.sna_vts * isp->parm.sna_framerate * 16) / (100 * isp->parm.sna_framerate_div);
		band_value_60Hz = (isp->parm.sna_vts * isp->parm.sna_framerate * 16) / (120 * isp->parm.sna_framerate_div);

		if (isp->process == ISP_PROCESS_HDR_YUV) {
			exposure_ratio = exposure_ratio /ISP_BRACKET_RATIO_2;
			isp_reg_writew(isp, ISP_BRACKET_RATIO_1, ISP_BRACKET_RATIO1);
			isp->hdr_snashot = 1;
		}

		isp_reg_writew(isp, band_value_50Hz, ISP_SNAP_BANDING_50HZ);
		isp_reg_writew(isp, band_value_60Hz, ISP_SNAP_BANDING_60HZ);
	} else {
		band_value_50Hz = (isp->parm.pre_vts * isp->parm.pre_framerate * 16) / (100 * isp->parm.pre_framerate_div);
		band_value_60Hz = (isp->parm.pre_vts * isp->parm.pre_framerate * 16) / (120 * isp->parm.pre_framerate_div);

		if (isp->process == ISP_PROCESS_HDR_YUV && isp->hdr_snashot) {
			exposure_ratio = exposure_ratio * ISP_BRACKET_RATIO_2 * 0x100 /ISP_BRACKET_RATIO_1;
			isp->hdr_snashot = 0;
		}

		isp_reg_writew(isp, band_value_50Hz, ISP_PRE_BANDING_50HZ);
		isp_reg_writew(isp, band_value_60Hz, ISP_PRE_BANDING_60HZ);
	}
	ISP_PRINT("band_value_50HZ is 0x%x,band_value_60HZ is 0x%x\n",band_value_50Hz,band_value_60Hz);
	isp_reg_writew(isp, exposure_ratio, ISP_EXPOSURE_RATIO);

	//set dynamic framerate here
	if (iparm->frame_rate == FRAME_RATE_AUTO)
		isp_set_dynamic_framerate_enable(isp, 0x01);
	else
		isp_set_dynamic_framerate_enable(isp, 0x00);

	if(!isp->snapshot) {
		isp_reg_writew(isp, (isp->fmt_data.framerate/isp->fmt_data.framerate_div) & 0xffff, REG_ISP_AF_FRAME_RATE);
		af_contrl = isp_reg_readb(isp, REG_ISP_AF_HW_CTRL);
		af_contrl |= (isp->fmt_data.binning << 2);
		isp_reg_writeb(isp, af_contrl, REG_ISP_AF_HW_CTRL);

		isp_set_scene(isp, iparm->scene_mode);
	}

#if 0
	//tell isp sensor hvflip setting
	func_control = isp_reg_readb(isp, REG_ISP_HVFLIP);
	if (iparm->vflip)
		func_control |= 0x08;
	else
		func_control &= 0xf7;

	if (iparm->hflip)
		func_control |= 0x04;
	else
		func_control &= 0xfb;

	isp_reg_writeb(isp, func_control, REG_ISP_HVFLIP);
#endif

	return 0;
}

static int isp_i2c_config(struct isp_device *isp)
{
	unsigned char val;

	if (isp->pdata->flags & CAMERA_I2C_FAST_SPEED)
		val = I2C_SPEED_200;
	else
		val = I2C_SPEED_100;

	isp_reg_writeb(isp, val, REG_SCCB_MAST1_SPEED);
	return 0;
}
#ifdef CONFIG_CAMERA_DRIVING_RECODE
static int isp_get_menunal_foucs_sharpness(struct isp_device *isp)
{
       unsigned int sharpness_info[25];
       int i,val=0;

       for (i = 0; i < 25; i++) {
               sharpness_info[i] = isp_reg_readw(isp,0x66800+i);
               val += sharpness_info[i];
       }
      return val;
}
static int isp_get_preview_awb_gain(struct isp_device *isp, struct isp_pre_parm *pre_parm)
{
       unsigned int awb_b_gain;
       unsigned int awb_gb_gain;
      unsigned int awb_gr_gain;
       unsigned int awb_r_gain;

       awb_b_gain = isp_reg_readw(isp,0x66c84);
       awb_gb_gain = isp_reg_readw (isp,0x66c86);
       awb_gr_gain = isp_reg_readw (isp,0x66c88);
       awb_r_gain = isp_reg_readw (isp,0x66c8a);

       pre_parm->awb_b = awb_b_gain ;
       pre_parm->awb_gb = awb_gb_gain ;
       pre_parm->awb_gr = awb_gr_gain ;
       pre_parm->awb_r = awb_r_gain ;

       return 0;


}
static int isp_get_preview_parm_for_movie(struct isp_device *isp, struct isp_pre_parm *pre_parm)
{
       unsigned int v_exposure;
       unsigned int v_gain;
       unsigned int v_brightness;
       unsigned int cur_mean;
       int ret;

       ret = isp_get_pre_exp(isp,&v_exposure);
       if (ret){
               printk("[ISP]:get preview exposure error\n");
       }
       ret = isp_get_pre_gain(isp,&v_gain);
       if (ret){
               printk("[ISP]:get preview gain error\n");
       }

       cur_mean = isp_reg_readb(isp, 0x1c75e);

       ret = isp_get_preview_awb_gain(isp, pre_parm);
       if (ret){
               printk("[ISP]:get preview awb value error\n");
       }

    pre_parm->pre_exposure = v_exposure ;
    pre_parm->pre_gain= v_gain ;
    pre_parm->cur_mean= cur_mean ;

       return 0 ;
}
#endif
static int isp_i2c_xfer_cmd(struct isp_device *isp, struct isp_i2c_cmd *cmd)
{
	unsigned char val = 0;

	isp_reg_writew(isp, cmd->reg, COMMAND_BUFFER);
	if (!(cmd->flags & I2C_CMD_READ)) {
		if (cmd->flags & I2C_CMD_DATA_16BIT) {
			isp_reg_writew(isp, cmd->data, COMMAND_BUFFER + 2);
		} else {
			isp_reg_writeb(isp, cmd->data & 0xff, COMMAND_BUFFER + 2);
			isp_reg_writeb(isp, 0xff, COMMAND_BUFFER + 3);
		}
	}

	val |= SELECT_I2C_PRIMARY;
	if (!(cmd->flags & I2C_CMD_READ))
		val |= SELECT_I2C_WRITE;
	if (cmd->flags & I2C_CMD_ADDR_16BIT)
		val |= SELECT_I2C_16BIT_ADDR;
	if (cmd->flags & I2C_CMD_DATA_16BIT)
		val |= SELECT_I2C_16BIT_DATA;

	isp_reg_writeb(isp, val, COMMAND_REG1);
	isp_reg_writeb(isp, cmd->addr, COMMAND_REG2);
	isp_reg_writeb(isp, 0x01, COMMAND_REG3);

	/* Wait for command set done interrupt. */
	if (isp_send_cmd(isp, CMD_I2C_GRP_WR, ISP_I2C_TIMEOUT)) {
		ISP_ERR("Failed to wait i2c set done (%02x)!\n",
			isp_reg_readb(isp, REG_ISP_INT_EN));
		return -ETIME;
	}

	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT))
		|| (CMD_I2C_GRP_WR != isp_reg_readb(isp, COMMAND_FINISHED))) {
		ISP_ERR("Failed to write sequeue to I2C (%02x:%02x)\n",
			isp_reg_readb(isp, COMMAND_RESULT),
			isp_reg_readb(isp, COMMAND_FINISHED));
		return -EINVAL;
	}

	if (cmd->flags & I2C_CMD_READ) {
		if (cmd->flags & I2C_CMD_DATA_16BIT)
			cmd->data = isp_reg_readw(isp, COMMAND_BUFFER + 2);
		else
			cmd->data = isp_reg_readb(isp, COMMAND_BUFFER + 3);
	}

	return 0;
}

static int isp_i2c_fill_buffer(struct isp_device *isp)
{
	struct v4l2_fmt_data *data = &isp->fmt_data;
	unsigned char val = 0;
	unsigned char i;

	for (i = 0; i < data->reg_num; i++) {
		isp_reg_writew(isp, data->reg[i].addr, COMMAND_BUFFER + i * 4);
		if (data->flags & I2C_CMD_DATA_16BIT) {
			isp_reg_writew(isp, data->reg[i].data,
						COMMAND_BUFFER + i * 4 + 2);
		} else {
			isp_reg_writeb(isp, data->reg[i].data & 0xff,
						COMMAND_BUFFER + i * 4 + 2);
			isp_reg_writeb(isp, 0xff, COMMAND_BUFFER + i * 4 + 3);
		}
	}

	if (data->reg_num) {
		val |= SELECT_I2C_PRIMARY | SELECT_I2C_WRITE;
		if (data->flags & V4L2_I2C_ADDR_16BIT)
			val |= SELECT_I2C_16BIT_ADDR;
		if (data->flags & V4L2_I2C_DATA_16BIT)
			val |= SELECT_I2C_16BIT_DATA;

		isp_reg_writeb(isp, val, COMMAND_REG1);
		isp_reg_writeb(isp, data->slave_addr << 1, COMMAND_REG2);
		isp_reg_writeb(isp, data->reg_num, COMMAND_REG3);
	} else {
		isp_reg_writeb(isp, 0x00, COMMAND_REG1);
		isp_reg_writeb(isp, 0x00, COMMAND_REG2);
		isp_reg_writeb(isp, 0x00, COMMAND_REG3);
	}

	return 0;
}

static unsigned int isp_calc_shutter_speed(struct isp_device *isp)
{
	unsigned int speed = 0;

	speed = isp->sna_exp;

	if(isp->sna_exp_mode == SNA_ZSL_NORMAL || isp->sna_exp_mode == SNA_ZSL_FLASH){
		if(isp->parm.pre_framerate * isp->parm.pre_vts)
			speed = (speed * isp->parm.pre_framerate_div * 10000) / (isp->parm.pre_framerate * isp->parm.pre_vts);
		else
			ISP_ERR("isp_calc_shutter_speed: invalid parameter in ZSL snapshot\n");
	} else {
		if(isp->parm.sna_framerate * isp->parm.sna_vts)
			speed = (speed * isp->parm.sna_framerate_div * 10000) / (isp->parm.sna_framerate * isp->parm.sna_vts);
		else
			ISP_ERR("isp_calc_shutter_speed: invalid parameter in Normal snapshot\n");
	}

	speed = 100 * speed;

	return speed;
}

static int isp_set_format(struct isp_device *isp)
{
	struct v4l2_isp_parm *sensor_parm = isp->sd_isp_parm;
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	unsigned char cmd_finished = 0;

	isp_i2c_fill_buffer(isp);
	isp_reg_writeb(isp, ISP_CCLK_DIVIDER, COMMAND_REG4);

	if(isp->first_init){
		isp->first_init = 0;
		isp_reg_writeb(isp, 0x80, COMMAND_REG5);
	}
	else {
		if (sensor_parm && sensor_parm->sensor_sub_type)
			isp_reg_writeb(isp, 0x80, COMMAND_REG5);
		else
			isp_reg_writeb(isp, 0x00, COMMAND_REG5);
	}

	/* Wait for command set done interrupt. */
	if (isp_send_cmd(isp, CMD_SET_FORMAT, ISP_FORMAT_TIMEOUT)) {
		ISP_ERR("Failed to wait format set done!\n");
		return -ETIME;
	}

	cmd_finished = isp_reg_readb(isp, COMMAND_FINISHED);

	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT)) ||
		((CMD_SET_FORMAT != cmd_finished) && (CMD_SET_AECGC != cmd_finished)
		&& (CMD_SET_AEC != cmd_finished)  && (CMD_SET_AGC != cmd_finished))) {
			ISP_ERR("Failed to set format (%02x:%02x)\n",
			isp_reg_readb(isp, COMMAND_RESULT),
			isp_reg_readb(isp, COMMAND_FINISHED));
			return -EINVAL;
	}
	if(!csd->yuv){
		if(isp->snap_flag) {
			isp_get_pre_exp(isp, &isp->sna_exp);
			isp_get_pre_gain(isp, &isp->sna_gain);
		}
	}

	return 0;
}

static int isp_set_capture(struct isp_device *isp)
{
	//struct isp_parm *parm = &isp->parm;
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_aecgc_control *aecgc_control = isp->aecgc_control;
	int ret;
	int i;
	unsigned long tm;

	isp_i2c_fill_buffer(isp);
	isp_reg_writeb(isp, ISP_CCLK_DIVIDER, COMMAND_REG4);

	/* set anti-shake  */
/*
	if (CAP_IS_BITSET(isp->capability, V4L2_PRIVACY_CAP_ANTI_SHAKE_CAPTURE))
		isp_set_capture_anti_shake(isp, parm->anti_shake);
*/
	if (isp->process == ISP_PROCESS_HDR_YUV)
		isp_reg_writeb(isp, 0x41, COMMAND_REG5);
	else
		isp_reg_writeb(isp, 0x40, COMMAND_REG5);

	INIT_COMPLETION(isp->raw_completion);

	/* Wait for command set done interrupt. */
	if (isp_send_cmd(isp, CMD_CAPTURE, ISP_CAPTURE_TIMEOUT)) {
		ISP_ERR("Failed to wait capture set done!\n");
		return -ETIME;
	}

	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT))
		|| (CMD_CAPTURE != isp_reg_readb(isp, COMMAND_FINISHED))) {
		ISP_ERR("Failed to set capture (%02x:%02x)\n",
			isp_reg_readb(isp, COMMAND_RESULT),
			isp_reg_readb(isp, COMMAND_FINISHED));
		return -EINVAL;
	}

	if(!csd->yuv){
		isp_get_sna_exp(isp, &isp->sna_exp);
		isp_get_sna_gain(isp, &isp->sna_gain);
	}

	if (isp->process == ISP_PROCESS_HDR_YUV) {
		isp->hdr_bracket_exposure_state = 0;
		for(i = 0; i < 8; i++)
		{
			isp->hdr[i] = isp_reg_readb(isp, 0x1e95c + i);
		}
		if (aecgc_control && aecgc_control->bracket_short_exposure)
			aecgc_control->bracket_short_exposure();
		tm = wait_for_completion_timeout(&isp->raw_completion,ISP_HDR_TIMEOUT);
		if (!tm && !isp->raw_completion.done) {
			ISP_PRINT("wait HDR first raw_completion timeout\n");
			return -ETIMEDOUT;
		}

		if (aecgc_control && aecgc_control->bracket_long_exposure)
			aecgc_control->bracket_long_exposure();

		if (isp->pp_buf) {
			isp->buf_start.paddr = (u32)camdev->bracket.second_frame.paddr;
			isp_set_address(isp, 0, (u32)camdev->bracket.second_frame.paddr);
			isp_reg_writeb(isp, 0x01, REG_BASE_ADDR_READY);
		} else {
			isp->buf_start.paddr = (u32)camdev->bracket.second_frame.paddr;
			isp_set_address(isp, 1, (u32)camdev->bracket.second_frame.paddr);
			isp_reg_writeb(isp, 0x02, REG_BASE_ADDR_READY);
		}

		atomic_set(&capture->sna_enable, 1);
		INIT_COMPLETION(isp->raw_completion);
		tm = wait_for_completion_timeout(&isp->raw_completion,ISP_HDR_TIMEOUT);
		if (!tm && !isp->raw_completion.done) {
			ISP_PRINT("wait HDR second raw_completion timeout\n");
			return -ETIMEDOUT;
		}else {
			isp->hdr_bracket_exposure_state = 1;
		}
	} else {
		if (aecgc_control && aecgc_control->start_capture) {
			ret = aecgc_control->start_capture();
			if (ret)
				return ret;
		}
		if (isp->process == ISP_PROCESS_OFFLINE_YUV) {
			tm = wait_for_completion_timeout(&isp->raw_completion,ISP_OFFLINE_TIMEOUT);
			if (!tm && !isp->raw_completion.done) {
				ISP_PRINT("wait offline raw_completion timeout\n");
				return -ETIMEDOUT;
			}
		}
	}
	return 0;
}

static int isp_get_zoom_ratio(struct isp_device *isp, int zoom)
{
	int zoom_ratio;

	switch (zoom) {
	case ZOOM_LEVEL_5:
		zoom_ratio = 0x220;
		break;
	case ZOOM_LEVEL_4:
		zoom_ratio = 0x200;
		break;
	case ZOOM_LEVEL_3:
		zoom_ratio = 0x1f0;
		break;
	case ZOOM_LEVEL_2:
		zoom_ratio = 0x1a0;
		break;
	case ZOOM_LEVEL_1:
		zoom_ratio = 0x150;
		break;
	case ZOOM_LEVEL_0:
	default:
		zoom_ratio = 0x100;
		break;
	}

	return zoom_ratio;
}

static int isp_set_light_meters(struct isp_device *isp, int val)
{
	int ret = 0;
	switch(val) {
		case METER_MODE_CENTER :
			ret = isp_set_center_metering(isp);
			if(ret){
				ISP_ERR("Failed to set meter mode center!\n");
				return ret;
			}
			break;
		case METER_MODE_DOT:
			ret = isp_set_metering(isp, &(isp->parm.meter_rect));
			if(ret){
				ISP_ERR("Failed to set meter mode dot!\n");
				return ret;
			}
			break;
		case METER_MODE_MATRIX:
			ret = isp_set_matrix_metering(isp);
			if (ret){
				ISP_ERR("Failed to set meter mode matrix!\n");
				return ret;
			}
			break;
		case METER_MODE_CENTRAL_WEIGHT:
			ret = isp_set_central_weight_metering(isp);
			if(ret){
				ISP_ERR("Failed to set meter mode central weight!\n");
				return ret;
			}
			break;

		}
	return 0;
}

static int isp_set_zoom(struct isp_device *isp, int zoom)
{
	int zoom_ratio;

	zoom_ratio = isp_get_zoom_ratio(isp, zoom);
	isp->parm.zoom = zoom;

	isp_reg_writeb(isp, 0x01, COMMAND_REG1);
	isp_reg_writeb(isp, zoom_ratio >> 8, COMMAND_REG2);
	isp_reg_writeb(isp, zoom_ratio & 0xFF, COMMAND_REG3);
	isp_reg_writeb(isp, zoom_ratio >> 8, COMMAND_REG4);
	isp_reg_writeb(isp, zoom_ratio & 0xFF, COMMAND_REG5);

	/* Wait for command set done interrupt. */
	if (isp_send_cmd(isp, CMD_ZOOM_IN_MODE, ISP_ZOOM_TIMEOUT)) {
		ISP_ERR("Failed to wait zoom set done!\n");
		return -ETIME;
	}

	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT))
		|| (CMD_ZOOM_IN_MODE != isp_reg_readb(isp, COMMAND_FINISHED))) {
		ISP_ERR("Failed to set zoom (%02x:%02x)\n",
			isp_reg_readb(isp, COMMAND_RESULT),
			isp_reg_readb(isp, COMMAND_FINISHED));
		return -EINVAL;
	}

	return 0;
}



static int isp_set_autofocus(struct isp_device *isp, int val)
{
	int ret = 0;
	int focus_area_reset = 0;

	if (val >= AUTO_FOCUS_ON ){
		if (isp_focus_is_active(isp) && !isp_autofocus_result(isp)) {
			ISP_PRINT("isp focus is busy now, isp focus mode=%d\n", isp_reg_readb(isp, REG_ISP_FOCUS_MODE));
			return 0;//-EIO;
		}
		if(isp->parm.focus_rect.left == 0 && isp->parm.focus_rect.top == 0
			&& isp->parm.focus_rect.width == 0 && isp->parm.focus_rect.height ==0)
			focus_area_reset = 1;

		//set focus area first
		if (!isp->parm.focus_rect_set_flag || focus_area_reset) {
			isp->parm.focus_rect.left = -200;
			isp->parm.focus_rect.top = -200;
			isp->parm.focus_rect.width = 400;
			isp->parm.focus_rect.height = 400;
		}
		isp->parm.focus_rect_set_flag = 0;
		ret = isp_set_autofocus_area(isp, &(isp->parm.focus_rect));
		if (ret) {
			ISP_ERR("failed to set focus area\n");
			return ret;
		}
		switch (isp->parm.focus_mode) {
		case FOCUS_MODE_AUTO:
			if (isp->parm.meter_set_flag){
				isp_set_metering(isp, &(isp->parm.meter_rect));
				isp->parm.meter_set_flag = 0;
			}
			isp_reg_writeb(isp, 0x00, REG_ISP_FOCUS_MODE);
			break;
		case FOCUS_MODE_CONTINUOUS_AUTO:
		case FOCUS_MODE_CONTINUOUS_PICTURE:
		case FOCUS_MODE_CONTINUOUS_VIDEO:
			isp_reg_writeb(isp, 0x01, REG_ISP_FOCUS_MODE);
			break;
		case FOCUS_MODE_MACRO:
			if (isp->parm.meter_set_flag){
				isp_set_metering(isp, &(isp->parm.meter_rect));
				isp->parm.meter_set_flag = 0;
			}
			isp_reg_writeb(isp, 0x00, REG_ISP_FOCUS_MODE);
			break;
		case FOCUS_MODE_INFINITY:
			isp_reg_writeb(isp, 0x00, REG_ISP_FOCUS_MODE);
			break;
		default:
			ISP_ERR("invalid focus mode %d\n", isp->parm.focus_mode);
			return -EINVAL;
		}
		isp_reg_writeb(isp, 0x1, REG_ISP_FOCUS_ONOFF);//bAFActive on
		if (val == AUTO_FOCUS_ON_CLEAR_RESULT || isp_reg_readb(isp, REG_ISP_FOCUS_MODE) == 0)
			isp_reg_writeb(isp, 0x00, REG_ISP_FOCUS_RESULT);
	} else if (val == AUTO_FOCUS_FORCE_OFF) {
		isp_reg_writeb(isp, 0x00, REG_ISP_FOCUS_ONOFF);//bAFActiveoff
		//isp_reg_writeb(isp, 0x02, REG_ISP_FOCUS_RESULT);
	} else if (val == AUTO_FOCUS_OFF) {
		if (isp_reg_readb(isp, REG_ISP_FOCUS_MODE) != 0x00)
			isp_reg_writeb(isp, 0x00, REG_ISP_FOCUS_ONOFF);//bAFActive off
			//isp_reg_writeb(isp, 0x02, REG_ISP_FOCUS_RESULT);//nAFStatusFlag to busy for restart
	}

	return 0;
}

static int isp_autofocus_result(struct isp_device *isp)
{
	return isp_reg_readb(isp, REG_ISP_FOCUS_RESULT);

}

static int isp_focus_is_active(struct isp_device *isp)
{
	return isp_reg_readb(isp, REG_ISP_FOCUS_ONOFF);
}

static int isp_set_autofocus_area(struct isp_device *isp, struct v4l2_rect *spot)
{
	struct isp_parm *iparm = &isp->parm;
	int left, top, width, height;
/*
	if (isp_focus_is_active(isp) && !isp_autofocus_result(isp)) {
		ISP_ERR("isp focus is busy now try later\n");
		return -EIO;
	}
*/
	left = iparm->crop_x + (int)(((spot->left + 1000) * iparm->crop_width) / 2000);
	top = iparm->crop_y + (int)(((spot->top + 1000) * iparm->crop_height) / 2000);
	width = (int)((spot->width * iparm->crop_width) / 2000);
	height = (int)((spot->height * iparm->crop_height) / 2000);

	if (width % 6)
		width = (width / 6 + 1) * 6;

	if (height % 6)
		height = (height / 6 + 1) * 6;

	if (width > 630)
		width = 630;

	if (height > 630)
		height = 630;

	if((left + top ) % 2 == 0)
		left ++;

	isp_reg_writew(isp, left, REG_ISP_FOCUS_RECT_LEFT);
	isp_reg_writew(isp, top, REG_ISP_FOCUS_RECT_TOP);
#ifndef ISP_STATIC_FOCUS_RECT_SIZE
	isp_reg_writew(isp, width, REG_ISP_FOCUS_RECT_WIDTH);
	isp_reg_writew(isp, height, REG_ISP_FOCUS_RECT_HEIGHT);
#endif

	ISP_PRINT("focus area (%d,%d,%d,%d)=>(%d,%d,%d,%d) crop_width=%d crop_height=%d\n",
              spot->left, spot->top, spot->width, spot->height,
              left, top, width, height,
              iparm->crop_width, iparm->crop_height);

	return 0;
}

static int isp_set_matrix_metering(struct isp_device *isp)
{
	struct isp_parm *iparm = &isp->parm;

	ISP_PRINT("isp_set_matrix_metering\n");

	isp_set_aecgc_win(isp, iparm->in_width, iparm->in_height, METER_MODE_MATRIX);

	if(isp->sd_isp_parm->meter_light_roi) {
		isp_reg_writew(isp, 0, REG_ISP_ROI_LEFT);
		isp_reg_writew(isp, 0, REG_ISP_ROI_TOP);
		isp_reg_writew(isp, 0, REG_ISP_ROI_RIGHT);
		isp_reg_writew(isp, 0, REG_ISP_ROI_BOTTOM);

		isp_reg_writeb(isp,0x1,0x6642a);
		isp_reg_writeb(isp,0x0,0x6642b);
	}
	return 0;
}

static int isp_set_center_metering(struct isp_device *isp)
{
	struct isp_parm *iparm = &isp->parm;
	ISP_PRINT("isp_set_center_metering\n");

	isp_set_aecgc_win(isp, iparm->in_width, iparm->in_height, METER_MODE_CENTER);

	if(isp->sd_isp_parm->meter_light_roi) {
		isp_reg_writew(isp, 0, REG_ISP_ROI_LEFT);
		isp_reg_writew(isp, 0, REG_ISP_ROI_TOP);
		isp_reg_writew(isp, 0, REG_ISP_ROI_RIGHT);
		isp_reg_writew(isp, 0, REG_ISP_ROI_BOTTOM);

		isp_reg_writeb(isp,0x1,0x6642a);
		isp_reg_writeb(isp,0x0,0x6642b);
	}
	return 0;
}

static int isp_set_central_weight_metering(struct isp_device *isp)
{
	struct isp_parm *iparm = &isp->parm;

	ISP_PRINT("isp_set_central_weight_metering\n");

	isp_set_aecgc_win(isp, iparm->in_width, iparm->in_height, METER_MODE_CENTRAL_WEIGHT);

	if(isp->sd_isp_parm->meter_light_roi) {

		isp_reg_writew(isp, 0, REG_ISP_ROI_LEFT);
		isp_reg_writew(isp, 0, REG_ISP_ROI_TOP);
		isp_reg_writew(isp, 0, REG_ISP_ROI_RIGHT);
		isp_reg_writew(isp, 0, REG_ISP_ROI_BOTTOM);

		isp_reg_writeb(isp,0x1,0x6642a);
		isp_reg_writeb(isp,0x0,0x6642b);
	}
	return 0;
}


static int isp_set_metering(struct isp_device *isp, struct v4l2_rect *spot)
{
	struct isp_parm *iparm = &isp->parm;
	int aec_stat_left, aec_stat_top, aec_stat_right, aec_stat_bottom;
	int spot_bottom, spot_right;
	int roi_left, roi_top, roi_right, roi_bottom;
	int spot_center_left,spot_center_top;
	int win_left,win_right,win_top,win_bottom;
	int win_width,win_height;
	int aecgc_stable = 0;
	int i=0;

	if(isp->snapshot)
		return 0;

	//get aec stat window
	aec_stat_left = isp_reg_readw(isp, REG_ISP_AECGC_STAT_WIN_LEFT);
	aec_stat_top = isp_reg_readw(isp, REG_ISP_AECGC_STAT_WIN_TOP);
	aec_stat_right = isp_reg_readw(isp, REG_ISP_AECGC_STAT_WIN_RIGHT);
	aec_stat_bottom = isp_reg_readw(isp, REG_ISP_AECGC_STAT_WIN_BOTTOM);

	if(isp->sd_isp_parm->meter_light_roi) {
		spot_bottom = spot->top + spot->height;
		spot_right = spot->left + spot->width;
		roi_left = iparm->crop_x + (int)(((spot->left+1000)*iparm->crop_width)/2000);
		roi_top = iparm->crop_y + (int)(((spot->top+1000)*iparm->crop_height)/2000);
		roi_right = iparm->in_width - (iparm->crop_x + ((int)(((spot_right + 1000)*iparm->crop_width)/2000))); //right down point coordinates
		roi_bottom = iparm->in_height - (iparm->crop_y + ((int)(((spot_bottom + 1000)*iparm->crop_height)/2000)));

		roi_left -= aec_stat_left;
		roi_right -= aec_stat_right;
		roi_top -= aec_stat_top;
		roi_bottom -= aec_stat_bottom;
		if (roi_left < 0)
			roi_left = 0;
		if (roi_right < 0)
			roi_right = 0;
		if (roi_top < 0)
			roi_top = 0;
		if (roi_bottom < 0)
			roi_bottom = 0;

		while (iparm->in_width - (roi_left + roi_right + aec_stat_left + aec_stat_right) < 16) {
			if (roi_right)
				roi_right--;
			else if (roi_left)
				roi_left--;
			else {
				ISP_ERR("aec stat window width is too small < 16\n");
				return -EINVAL;
			}
		}

		while (iparm->in_height - (roi_top + roi_bottom + aec_stat_top + aec_stat_bottom) < 16) {
			if (roi_bottom)
				roi_bottom--;
			else if (roi_top)
				roi_top--;
			else {
				ISP_ERR("aec stat window height is too small < 16\n");
				return -EINVAL;
			}
		}

		isp_reg_writew(isp, roi_left, REG_ISP_ROI_LEFT);
		isp_reg_writew(isp, roi_top, REG_ISP_ROI_TOP);
		isp_reg_writew(isp, roi_right, REG_ISP_ROI_RIGHT);
		isp_reg_writew(isp, roi_bottom, REG_ISP_ROI_BOTTOM);

		isp_reg_writeb(isp,0x1,0x6642a);
		isp_reg_writeb(isp,0x0,0x6642b);

		for(i = 0; i<5; i++){
			isp_get_aecgc_stable(isp, &aecgc_stable);
			if(aecgc_stable)
				break;
		}

		ISP_PRINT("metering rect (%d,%d,%d,%d)=>(%d,%d,%d,%d) crop_width=%d crop_height=%d\n",
			spot->left, spot->top, spot_right, spot_bottom,
			roi_left - iparm->crop_x, roi_top - iparm->crop_y,
			iparm->in_width - (roi_left + roi_right + aec_stat_left + aec_stat_right),
			iparm->in_height - (roi_top + roi_bottom + aec_stat_top + aec_stat_bottom),
			iparm->crop_width,
			iparm->crop_height);
	}else {
		spot_center_left = iparm->crop_x + (int)(((spot->left+1000)*iparm->crop_width)/2000) + spot->width/2;
		spot_center_top = iparm->crop_y + (int)(((spot->top+1000)*iparm->crop_height)/2000) + spot->height/2;

		if(iparm->spot_meter_win_width && iparm->spot_meter_win_height) {
			win_width = iparm->spot_meter_win_width;
			win_height = iparm->spot_meter_win_height;
		} else {
			win_width= iparm->in_width/4;
			win_height = iparm->in_height/4;
		}
		win_left = spot_center_left - win_width/2;
		win_top = spot_center_top - win_height/2;
		win_right = iparm->in_width- win_left - win_width;
		win_bottom = iparm->in_height - win_top - win_height;

		if(win_left < aec_stat_left)
			win_left = aec_stat_left;

		if(win_top < aec_stat_top)
			win_top = aec_stat_top;

		if(win_right < aec_stat_right)
			win_left = iparm->in_width - aec_stat_right - win_width;

		if(win_bottom < aec_stat_bottom)
			win_top = iparm->in_height - aec_stat_bottom - win_height;

		isp_reg_writew(isp, win_left, REG_ISP_AECGC_WIN_LEFT);
		isp_reg_writew(isp, win_top, REG_ISP_AECGC_WIN_TOP);
		isp_reg_writew(isp, win_width, REG_ISP_AECGC_WIN_WIDTH);
		isp_reg_writew(isp, win_height, REG_ISP_AECGC_WIN_HEIGHT);

		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_4);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_5);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_6);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_7);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_8);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_9);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_10);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_11);
		isp_reg_writeb(isp, 0x1f, REG_ISP_AECGC_WIN_WEIGHT_12);

		for(i = 0; i<5; i++){
			isp_get_aecgc_stable(isp, &aecgc_stable);
			if(aecgc_stable)
				break;
		}

		ISP_PRINT("metering rect (%d,%d,%d,%d)=>(%d,%d,%d,%d),crop_width=%d,crop_height=%d\n",
				spot->left, spot->top, (spot->left + spot->width), (spot->top + spot->height),
				win_left,win_top,(win_left+win_width),(win_top + win_height),
				iparm->crop_width,iparm->crop_height);
	}
	return 0;
}

static int isp_boot(struct isp_device *isp, struct isp_prop *prop)
{
	unsigned char val;
	unsigned int reg_val = 0;

	if (isp->boot)
		return 0;

	/* Mask all interrupts. */
	isp_int_mask(isp, 0xff);
	isp_mac_int_mask(isp, 0xffff);

	/* Reset ISP.  */
	isp_reg_writeb(isp, DO_SOFT_RST, REG_ISP_SOFT_RST);

	/* Enable interrupt (only set_cmd_done interrupt). */
	isp_int_unmask(isp, MASK_INT_CMDSET);

	isp_reg_writeb(isp, DO_SOFTWARE_STAND_BY, REG_ISP_SOFT_STANDBY);

	/* Enable the clk used by mcu. */
	isp_reg_writeb(isp, 0xf1, REG_ISP_CLK_USED_BY_MCU);

	/* Download firmware to ram of mcu. */
	#ifdef COMIP_DEBUGTOOL_ENABLE
		if(prop->firmware_setting.size != 0)
			comip_debugtool_load_firmware(COMIP_DEBUGTOOL_FIRMWARE_FILENAME,
				(u8*)(isp->base + FIRMWARE_BASE), prop->firmware_setting.setting, prop->firmware_setting.size);
		else
			comip_debugtool_load_firmware(COMIP_DEBUGTOOL_FIRMWARE_FILENAME,
				(u8*)(isp->base + FIRMWARE_BASE), isp_firmware, ARRAY_SIZE(isp_firmware));
	#else
		if(prop->firmware_setting.size != 0)
			memcpy((unsigned char *)(isp->base + FIRMWARE_BASE), prop->firmware_setting.setting,
				prop->firmware_setting.size);
		else
			memcpy((unsigned char *)(isp->base + FIRMWARE_BASE), isp_firmware,
				ARRAY_SIZE(isp_firmware));
	#endif

	/* MCU initialize. */
	isp_reg_writeb(isp, 0xf0, REG_ISP_CLK_USED_BY_MCU);

	/* Wait for command set done interrupt. */
	if (isp_wait_cmd_done(isp, ISP_BOOT_TIMEOUT)) {
		ISP_ERR("MCU not respond when init ISP!\n");
		return -ETIME;
	}

	val = isp_reg_readb(isp, COMMAND_FINISHED);
	if (val != CMD_FIRMWARE_DOWNLOAD) {
		ISP_ERR("Failed to download isp firmware (%02x)\n",
			val);
		return -EINVAL;
	}
	reg_val = isp_reg_readw(isp, REG_ISP_FIRMWARE_VER);
	ISP_ERR("ISP firmware version=0x%04x\n", reg_val);

	isp_reg_writeb(isp, DO_SOFTWARE_STAND_BY, REG_ISP_SOFT_STANDBY);

	isp_i2c_config(isp);

	isp_reg_writeb(isp, ISP_CCLK_DIVIDER, REG_ISP_CORE_CTRL);

	return 0;
}

static int isp_irq_notify(struct isp_device *isp, unsigned int status)
{
	int ret = 0;

	if (isp->irq_notify)
		ret = isp->irq_notify(status, isp->data);

	return ret;
}

static irqreturn_t isp_irq(int this_irq, void *dev_id)
{
	struct isp_device *isp = dev_id;
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_capture *capture = &camdev->capture;
	unsigned char irq_status = 0, cmd_result = 0;
	unsigned short mac_irq_status = 0;
	unsigned int notify = 0;
#ifdef CONFIG_CAMERA_DRIVING_RECODE

	struct isp_pre_parm *pre_parm = &isp->pre_parm;
	unsigned int ret = 0;
	struct isp_parm *iparm = &isp->parm;
#endif

	irq_status = isp_int_state(isp);

	if (irq_status & MASK_INT_MAC)
		mac_irq_status = isp_mac_int_state(isp);

	/* Command set done interrupt. */
	if (irq_status & MASK_INT_CMDSET) {
		cmd_result = isp_reg_readb(isp, COMMAND_FINISHED);
		switch (cmd_result) {
		case CMD_SET_AECGC:
			notify |= (ISP_NOTIFY_AEC_DONE | ISP_NOTIFY_AGC_DONE);
			break;
		case CMD_SET_AEC:
			notify |= ISP_NOTIFY_AEC_DONE;
			break;
		case CMD_SET_AGC:
			notify |= ISP_NOTIFY_AGC_DONE;
			break;
		case CMD_SET_SUCCESS:
		default:
			complete(&isp->completion);
		}

	}

	/* Below is MAC IRQ process */
/*
	if (mac_irq_status && atomic_read(&isp->isp_mac_disable))
		return IRQ_HANDLED;
*/
	/* Drop. */
	if (mac_irq_status & (MASK_INT_DROP0 | MASK_INT_DROP1)) {
		notify |= (ISP_NOTIFY_DROP_FRAME | ISP_NOTIFY_SOF);
		if (!atomic_read(&isp->isp_mac_disable)) {
			if(mac_irq_status & MASK_INT_DROP0)
				isp->pp_buf = true;
			if(mac_irq_status & MASK_INT_DROP1)
				isp->pp_buf = false;
			if (!(((isp->process == ISP_PROCESS_OFFLINE_YUV)||(isp->process == ISP_PROCESS_HDR_YUV))&& isp->snapshot))
				isp_update_buffer(isp, &isp->buf_start);
			else {
				if (atomic_read(&capture->sna_enable))
					isp_update_buffer(isp, &isp->buf_start);
			}
		}
	}

	/* Frame start. */
	if (mac_irq_status & MASK_INT_FRAME_START)
		notify |= (ISP_NOTIFY_DATA_START | ISP_NOTIFY_SOF);

	/* Done. */
	if (mac_irq_status & (MASK_INT_WRITE_DONE0 | MASK_INT_WRITE_DONE1)){
		notify |= (ISP_NOTIFY_DATA_DONE | ISP_NOTIFY_EOF);
		if (mac_irq_status & MASK_INT_WRITE_DONE0)
			isp->pp_buf = false;
		else if (mac_irq_status & MASK_INT_WRITE_DONE1)
			isp->pp_buf = true;

		if (isp->hdr_drop_frames > 0) {
			isp->hdr_drop_frames--;
			notify &= ~ISP_NOTIFY_DATA_DONE;
			notify |= ISP_NOTIFY_DROP_FRAME;
			isp_update_buffer(isp, &isp->buf_start);
		}
#ifdef CONFIG_CAMERA_DRIVING_RECODE
		iparm->focus_sharpness = isp_get_menunal_foucs_sharpness(isp);
	/*report brightness/ exposure/gain/awb value*/
		ret = isp_get_preview_parm_for_movie(isp, pre_parm);
		if (ret) {
			printk("preview_parm error\n");
		}
#endif

		if ((isp->process == ISP_PROCESS_HDR_YUV || isp->process == ISP_PROCESS_OFFLINE_YUV)
			&& isp->snapshot && atomic_read(&capture->sna_enable)) {
			atomic_set(&capture->sna_enable, 0);
			complete(&isp->raw_completion);
		}

		if (!isp->snapshot &&
			isp->effect_ops &&
			isp->effect_ops->get_ct_saturation)
			queue_work(isp->isp_workqueue, &isp->saturation_work);

		if (!isp->snapshot &&
			isp->effect_ops &&
			isp->effect_ops->get_lens_threshold)
			queue_work(isp->isp_workqueue, &isp->lens_threshold_work);
	}

	/* FIFO overflow */
	if (mac_irq_status & (MASK_INT_OVERFLOW0 | MASK_INT_OVERFLOW1))
		notify |= ISP_NOTIFY_OVERFLOW;

	isp_irq_notify(isp, notify);

	return IRQ_HANDLED;
}

static int isp_mipi_init(struct isp_device *isp,
						int mipi_id,
						int lane_num,
						int yuv_data)
{
	unsigned char lane_reg_value = 0, reg_val = 0;

	if (lane_num == 1)
		lane_reg_value = 0;
	else if (lane_num == 2)
		lane_reg_value = 1;
	else if (lane_num == 4)
		lane_reg_value = 2;
	else {
		ISP_ERR("mipi%d do not support %d lanes.\n", mipi_id, lane_num);
		return -EINVAL;
	}

	if (mipi_id == 0) {
		isp_reg_writeb(isp, lane_reg_value, REG_ISP_MIPI0_LANE);
		isp_reg_writew(isp, INPUT_MIPI0 | PACK64_ENABLE | VRTM_ENABLE, REG_ISP_R_CTRL);
		if (yuv_data)
			isp_reg_writeb(isp, 0x04, 0x63000);
	}
	else {
		isp_reg_writeb(isp, lane_reg_value, REG_ISP_MIPI1_LANE);
		isp_reg_writew(isp, INPUT_MIPI1 | PACK64_ENABLE | VRTM_ENABLE, REG_ISP_R_CTRL);
		if (yuv_data)
			isp_reg_writeb(isp, 0x04, 0x63005);
	}

	if (yuv_data) {
		reg_val = isp_reg_readb(isp, 0x6502f);
		reg_val	|= (1 << 5);
		reg_val &= ~(1 << 4);
		isp_reg_writeb(isp, reg_val, 0x6502f);
	}

	return 0;
}

static int isp_dvp_init(struct isp_device *isp, int vsync_polar)
{
	unsigned char polar = 0;

	isp_reg_writew(isp, INPUT_DVP, REG_ISP_R_CTRL);

	polar = isp_reg_readb(isp, REG_ISP_POLAR);
	if (vsync_polar)
		polar |= 0x10;
	else
		polar &= 0xef;
	isp_reg_writeb(isp, polar, REG_ISP_POLAR);

	return 0;
}

static int __init isp_int_init(struct isp_device *isp)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&isp->lock, flags);

	isp->intr = 0;
	isp->mac_intr_l = 0;
	isp->mac_intr_h = 0;
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_L);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_H);

	spin_unlock_irqrestore(&isp->lock, flags);

	ret = request_irq(isp->irq, isp_irq, IRQF_SHARED,
			  "isp", isp);

	return ret;
}

static int __init isp_i2c_init(struct isp_device *isp)
{
	struct comip_i2c_platform_data *pdata = isp->i2c.pdata;
	int ret;

#if defined(CONFIG_CPU_LC1860)	//i2c set for LC1860
	if (isp->pdata->flags & CAMERA_USE_ISP_I2C) {
		comip_mfp_config(MFP_PIN_GPIO(167), MFP_GPIO167_ISP_I2C_SCL);
		comip_mfp_config(MFP_PIN_GPIO(168), MFP_GPIO168_ISP_I2C_SDA);
	}else{
		comip_mfp_config(MFP_PIN_GPIO(167), MFP_GPIO167_I2C3_SCL);
		comip_mfp_config(MFP_PIN_GPIO(168), MFP_GPIO168_I2C3_SDA);
	}
#else
	if (isp->pdata->flags & CAMERA_USE_ISP_I2C) {
		comip_mfp_config(MFP_PIN_GPIO(85), MFP_GPIO85_ISP_I2C_SCL);
		comip_mfp_config(MFP_PIN_GPIO(86), MFP_GPIO86_ISP_I2C_SDA);
	}else {
		comip_mfp_config(MFP_PIN_GPIO(85), MFP_GPIO85_I2C3_SCL);
		comip_mfp_config(MFP_PIN_GPIO(86), MFP_GPIO86_I2C3_SDA);
	}
#endif

	if (isp->pdata->flags & CAMERA_USE_ISP_I2C) {
		isp->i2c.xfer_cmd = isp_i2c_xfer_cmd;
		ret = isp_i2c_register(isp);
		if (ret)
			return ret;
	} else {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		if (isp->pdata->flags & CAMERA_I2C_PIO_MODE)
			pdata->use_pio = 1;

		if (isp->pdata->flags & CAMERA_I2C_HIGH_SPEED)
			pdata->flags = COMIP_I2C_HIGH_SPEED_MODE;
		else if (isp->pdata->flags & CAMERA_I2C_FAST_SPEED)
			pdata->flags = COMIP_I2C_FAST_MODE;
		else
			pdata->flags = COMIP_I2C_STANDARD_MODE;

		comip_set_i2c3_info(pdata);
	}

	return 0;
}

static int isp_i2c_release(struct isp_device *isp)
{
	if (isp->pdata->flags & CAMERA_USE_ISP_I2C) {
		isp_i2c_unregister(isp);
	} else {
		platform_device_register(to_platform_device(isp->dev));
		kfree(isp->i2c.pdata);
	}

	return 0;
}

static int isp_mfp_init(struct isp_device *isp, struct isp_prop *prop)
{
#if defined(CONFIG_CPU_LC1810)
	if (!prop->mipi) {
		comip_mfp_config(MFP_PIN_GPIO(74), MFP_GPIO74_ISP_SDATA_7);
		comip_mfp_config(MFP_PIN_GPIO(75), MFP_GPIO75_ISP_SDATA_6);
		comip_mfp_config(MFP_PIN_GPIO(76), MFP_GPIO76_ISP_SDATA_5);
		comip_mfp_config(MFP_PIN_GPIO(77), MFP_GPIO77_ISP_SDATA_4);
		comip_mfp_config(MFP_PIN_GPIO(78), MFP_GPIO78_ISP_SDATA_3);
		comip_mfp_config(MFP_PIN_GPIO(79), MFP_GPIO79_ISP_SDATA_2);
		comip_mfp_config(MFP_PIN_GPIO(82), MFP_GPIO82_ISP_SCLK);
		comip_mfp_config(MFP_PIN_GPIO(83), MFP_GPIO83_ISP_S_HSYNC);
		comip_mfp_config(MFP_PIN_GPIO(84), MFP_GPIO84_ISP_S_VSYNC);


		if (isp->pdata->flags & CAMERA_USE_HIGH_BYTE) {
			comip_mfp_config(MFP_PIN_GPIO(21), MFP_GPIO21_ISP_SDATA_9);
			comip_mfp_config(MFP_PIN_GPIO(20), MFP_GPIO20_ISP_SDATA_8);
		} else {
			comip_mfp_config(MFP_PIN_GPIO(80), MFP_GPIO80_ISP_SDATA_1);
			comip_mfp_config(MFP_PIN_GPIO(81), MFP_GPIO81_ISP_SDATA_0);
		}

	}
#elif defined(CONFIG_CPU_LC1860)
	comip_mfp_config(MFP_PIN_GPIO(173), MFP_GPIO173_ISP_CLKOUT1);
	comip_mfp_config(MFP_PIN_GPIO(178), MFP_GPIO178_CLK_OUT2);
#endif

	return 0;
}

static int isp_mfp_dinit(struct isp_device *isp)
{
#if defined(CONFIG_CPU_LC1860)
	comip_mfp_config(MFP_PIN_GPIO(173), MFP_PIN_MODE_GPIO);
	comip_mfp_config(MFP_PIN_GPIO(178), MFP_PIN_MODE_GPIO);

	gpio_request(MFP_PIN_GPIO(173), "isp_clk");
	gpio_request(MFP_PIN_GPIO(178), "clk_out");

	gpio_direction_output(MFP_PIN_GPIO(173), 0);
	gpio_direction_output(MFP_PIN_GPIO(178), 0);

	gpio_free(MFP_PIN_GPIO(173));
	gpio_free(MFP_PIN_GPIO(178));
#endif

	return 0;
}

static int isp_clk_init(struct isp_device *isp, struct comip_camera_client *client)
{
	int i;

	for (i = 0; i < ISP_CLK_NUM; i++) {
		if(client->flags & CAMERA_CLIENT_ISP_CLK_HIGH){
			isp->clk[i] = clk_get(isp->dev, isp_clks_high[i].name);
			if (IS_ERR(isp->clk[i])) {
				ISP_ERR("Failed to get %s clock %ld\n",
					isp_clks_high[i].name, PTR_ERR(isp->clk[i]));
				return PTR_ERR(isp->clk[i]);
			}

			clk_set_rate(isp->clk[i], isp_clks_high[i].rate);
			isp->clk_enable[i] = 0;
		} else if(client->flags & CAMERA_CLIENT_ISP_CLK_MIDDLE){
			isp->clk[i] = clk_get(isp->dev, isp_clks_middle[i].name);
			if (IS_ERR(isp->clk[i])) {
				ISP_ERR("Failed to get %s clock %ld\n",
					isp_clks_middle[i].name, PTR_ERR(isp->clk[i]));
				return PTR_ERR(isp->clk[i]);
			}
			clk_set_rate(isp->clk[i], isp_clks_middle[i].rate);
			isp->clk_enable[i] = 0;
		} else if(client->flags & CAMERA_CLIENT_ISP_CLK_HHIGH){
			isp->clk[i] = clk_get(isp->dev, isp_clks_hhigh[i].name);
			if (IS_ERR(isp->clk[i])) {
				ISP_ERR("Failed to get %s clock %ld\n",
					isp_clks_hhigh[i].name, PTR_ERR(isp->clk[i]));
				return PTR_ERR(isp->clk[i]);
			}
			clk_set_rate(isp->clk[i], isp_clks_hhigh[i].rate);
			isp->clk_enable[i] = 0;
		} else{
			isp->clk[i] = clk_get(isp->dev, isp_clks[i].name);
			if (IS_ERR(isp->clk[i])) {
				ISP_ERR("Failed to get %s clock %ld\n",
					isp_clks[i].name, PTR_ERR(isp->clk[i]));
				return PTR_ERR(isp->clk[i]);
			}

			clk_set_rate(isp->clk[i], isp_clks[i].rate);
			isp->clk_enable[i] = 0;
		}
	}
	return 0;
}

static int isp_clk_release(struct isp_device *isp)
{
	int i;

	for (i = 0; i < ISP_CLK_NUM; i++)
		clk_put(isp->clk[i]);

	return 0;
}

static int isp_clk_enable(struct isp_device *isp, unsigned int type,
						  struct comip_camera_client *client)
{
	int i;

	for (i = 0; i < ISP_CLK_NUM; i++) {
		if(client->flags & CAMERA_CLIENT_ISP_CLK_HIGH){
			if (!isp->clk_enable[i] && (isp_clks_high[i].flags & type)) {
				clk_enable(isp->clk[i]);
				isp->clk_enable[i] = 1;
			}
		} else if(client->flags & CAMERA_CLIENT_ISP_CLK_MIDDLE){
			if (!isp->clk_enable[i] && (isp_clks_middle[i].flags & type)) {
				clk_enable(isp->clk[i]);
				isp->clk_enable[i] = 1;
			}
		} else if(client->flags & CAMERA_CLIENT_ISP_CLK_HHIGH){
			if (!isp->clk_enable[i] && (isp_clks_hhigh[i].flags & type)) {
				clk_enable(isp->clk[i]);
				isp->clk_enable[i] = 1;
			}
		} else {
			if (!isp->clk_enable[i] && (isp_clks[i].flags & type)) {
				clk_enable(isp->clk[i]);
				isp->clk_enable[i] = 1;
			}
		}
	}
	return 0;
}

static int isp_clk_disable(struct isp_device *isp, unsigned int type,
						  struct comip_camera_client *client)
{
	int i;

	for (i = 0; i < ISP_CLK_NUM; i++) {
		if(client->flags & CAMERA_CLIENT_ISP_CLK_HIGH){
			if (isp->clk_enable[i] && (isp_clks_high[i].flags & type)) {
				clk_disable(isp->clk[i]);
				isp->clk_enable[i] = 0;
			}
		} else if(client->flags & CAMERA_CLIENT_ISP_CLK_MIDDLE){
			if (isp->clk_enable[i] && (isp_clks_middle[i].flags & type)) {
				clk_disable(isp->clk[i]);
				isp->clk_enable[i] = 0;
			}
		} else if(client->flags & CAMERA_CLIENT_ISP_CLK_HHIGH){
			if (isp->clk_enable[i] && (isp_clks_hhigh[i].flags & type)) {
				clk_disable(isp->clk[i]);
				isp->clk_enable[i] = 0;
			}
		} else {
			if (isp->clk_enable[i] && (isp_clks[i].flags & type)) {
					clk_disable(isp->clk[i]);
					isp->clk_enable[i] = 0;
			}
		}
	}

	return 0;
}

static int isp_powerdown(void)
{
	int reg,reg_int,reg_raw;
	unsigned int cnt = 50;

	spin_lock(&isp_suspend_lock);
	reg = readl(io_p2v(AP_PWR_ISP_PD_CTL));
	reg |= (1 << AP_PWR_PD_MK_WE);
	reg &= ~(1 << AP_PWR_PD_MK); //set the ISP_PD_MK = 0
	reg |= ((1 << AP_PWR_PD_EN)|(1 << AP_PWR_PD_EN_WE));//set the ISP_PD_EN = 1
	writel(reg, io_p2v(AP_PWR_ISP_PD_CTL));
	dsb();
	spin_unlock(&isp_suspend_lock);

	while (--cnt > 0) {
		reg_int = readl(io_p2v(AP_PWR_INT_RAW));
		if (reg_int & (1 << AP_PWR_ISP_PD_INTR)) {
			spin_lock(&isp_suspend_lock);
			//clear interrupt bit
			reg_raw = readl(io_p2v(AP_PWR_INTST_ARM));
			reg_raw |= 1 << AP_PWR_ISP_PD_INTR;
			writel(reg_raw, io_p2v(AP_PWR_INTST_ARM));

			reg = readl(io_p2v(AP_PWR_ISP_PD_CTL));
			reg |= (1 << AP_PWR_PD_EN_WE);
			reg &= ~(1 << AP_PWR_PD_EN); //set the ISP_PD_EN= 0
			writel(reg, io_p2v(AP_PWR_ISP_PD_CTL));
			spin_unlock(&isp_suspend_lock);
			ISP_PRINT("power down\n");
			return 0;
		}
		udelay(100);
	}
	return -EBUSY;
}

static int isp_powerup(void)
{
	int reg,reg_int,reg_raw;
	unsigned int cnt = 50;

	spin_lock(&isp_suspend_lock);
	reg = readl(io_p2v(AP_PWR_ISP_PD_CTL));
	reg |= ((1 << AP_PWR_WK_UP)|(1 << AP_PWR_WK_UP_WE));//set the ISP_WK_UP = 1
	writel(reg, io_p2v(AP_PWR_ISP_PD_CTL));
	dsb();
	spin_unlock(&isp_suspend_lock);

	while (--cnt > 0) {
		reg_int = readl(io_p2v(AP_PWR_INT_RAW));
		if (reg_int & (1 << AP_PWR_ISP_PU_INTR)) {
			spin_lock(&isp_suspend_lock);
			//clear interrupt bit
			reg_raw = readl(io_p2v(AP_PWR_INTST_ARM));
			reg_raw |= 1 << AP_PWR_ISP_PU_INTR;
			writel(reg_raw, io_p2v(AP_PWR_INTST_ARM));

			reg = readl(io_p2v(AP_PWR_ISP_PD_CTL));
			reg |= (1 << AP_PWR_WK_UP_WE);
			reg &= ~(1 << AP_PWR_WK_UP);		//set the ISP_WK_UP = 0
			writel(reg, io_p2v(AP_PWR_ISP_PD_CTL));
			spin_unlock(&isp_suspend_lock);
			ISP_PRINT("power up\n");
			return 0;
		}
		udelay(100);
	}
	return -EBUSY;
}

static int isp_init(struct isp_device *isp, void *data)
{
	struct comip_camera_client *client = data;
	int ret;

	isp->boot = 0;
	isp->poweron = 1;
	isp->snapshot = 0;
	isp->running = 0;
	isp->if_pip = 0;
	isp->parm.meter_mode = METER_MODE_CENTRAL_WEIGHT;
	isp->parm.brightness = BRIGHTNESS_H0;
	isp->parm.saturation = SATURATION_H0;
	ret = isp_clk_init(isp, client);
	if (ret)
		goto i2c_release;
	return 0;

i2c_release:
	isp_i2c_release(isp);
	return ret;
}

static int isp_release(struct isp_device *isp, void *data)
{
	isp->boot = 0;
	isp->poweron = 0;
	isp->snapshot = 0;
	isp->running = 0;
	isp->if_pip = 0;

	return 0;
}

static int isp_open(struct isp_device *isp, struct isp_prop *prop)
{
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_client *client;
	unsigned short afc_ctl0;
	unsigned int i;
	int ret = 0, dvp_vsync_polar = 0;

	if(camdev->csd[prop->index].client)
		client = camdev->csd[prop->index].client;
	else
		client = &isp->pdata->client[prop->index];

	if (!isp->poweron)
		return -ENODEV;

	isp->aecgc_control = prop->aecgc_control;

	isp_mfp_init(isp, prop);

	isp_powerup();
	isp_clk_enable(isp, ISP_CLK_MAIN, client);

	if (!isp->boot) {
		ret = isp_boot(isp, prop);
		if (ret)
			return ret;
		isp->boot = 1;
	}
	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		isp_clk_enable(isp, ISP_CLK_CSI, client);
		ret = isp_mipi_init(isp,
							prop->if_id,
							client->mipi_lane_num,
							(int)(client->flags & CAMERA_CLIENT_YUV_DATA));
	} else if (client->flags & CAMERA_CLIENT_IF_DVP) {
		if (prop->parm)
			dvp_vsync_polar = prop->parm->dvp_vsync_polar;
		ret = isp_dvp_init(isp, dvp_vsync_polar);
	}

	if (prop->setting.setting && prop->setting.size) {
		for (i = 0; i < prop->setting.size; i++) {
			isp_reg_writeb(isp, prop->setting.setting[i].val,
				prop->setting.setting[i].reg);
		}
	}
	else {
		for (i = 0; i < ARRAY_SIZE(default_isp_regs_init); i++) {
			isp_reg_writeb(isp, default_isp_regs_init[i].value,
				default_isp_regs_init[i].reg_num);
		}
	}

	if (prop->aecgc_control)
		isp_enable_aecgc_writeback(isp, 0);
	else
		isp_enable_aecgc_writeback(isp, 1);

	/* set single window & binning. */
	afc_ctl0 = isp_reg_readb(isp, 0x66100);
//	afc_ctl0 |= 1 << 2;
	afc_ctl0 &= ~ (1 << 1);
	isp_reg_writeb(isp, afc_ctl0, 0x66100);

	/* snapshot AF. */
	//isp_reg_writeb(isp, 0x0, 0x1cd0b);

	/* Set bAFActive off. */
	//isp_reg_writeb(isp, 0x0, 0x1cd0a);

	isp->input = prop->index;
	isp->sd_isp_parm = prop->parm;
	isp->snapshot = 0;
	memset(&isp->fmt_data, 0, sizeof(isp->fmt_data));

	isp->pp_buf = true;
	isp->first_init = 1;

	isp->parm.meter_rect.height = CENTER_WIDTH;
	isp->parm.meter_rect.width =  CENTER_HEIGHT;
	isp->parm.meter_rect.left = CENTER_LEFT;
	isp->parm.meter_rect.top = CENTER_TOP;

	memset(&isp->debug, 0, sizeof(isp->debug));

	if (prop->parm) {
		isp->parm.vflip = prop->parm->flip;
		isp->parm.hflip = prop->parm->mirror;
	}

	return ret;
}

static int isp_close(struct isp_device *isp, struct isp_prop *prop)
{
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_client *client;

	if(camdev->csd[prop->index].client)
		client = camdev->csd[prop->index].client;
	else
		client = &isp->pdata->client[prop->index];

	/* Notice: wait for mac wirte ram finish */
	msleep(80);

	if (!isp->poweron)
		return -ENODEV;

	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		csi_phy_stop(prop->if_id);
		isp_clk_disable(isp, ISP_CLK_CSI,client);
	}

	isp_reg_writeb(isp, DO_SOFT_RST, REG_ISP_SOFT_RST);
	isp_reg_writeb(isp, isp_reg_readb(isp, 0x6301b) | 0x02, 0x6301b);
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_H);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_L);

	isp_powerdown();
	isp_clk_disable(isp, ISP_CLK_MAIN, client);
	isp->input = -1;

	isp_mfp_dinit(isp);

	return 0;
}

static int isp_esd_reset(struct isp_device *isp)
{
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_client *client = camdev->csd[isp->input].client;
	struct isp_prop *prop = &camdev->csd[isp->input].prop;
	unsigned short afc_ctl0;
	unsigned int i;
	int dvp_vsync_polar = 0;

	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		csi_phy_stop(prop->if_id);
		isp_clk_disable(isp, ISP_CLK_CSI,client);
	}

	isp_reg_writeb(isp, DO_SOFT_RST, REG_ISP_SOFT_RST);
	isp_reg_writeb(isp, isp_reg_readb(isp, 0x6301b) | 0x02, 0x6301b);
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_H);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_L);

	isp_powerdown();
	isp_clk_disable(isp, ISP_CLK_MAIN, client);
	isp->boot = 0;

	isp_powerup();
	isp_clk_enable(isp, ISP_CLK_MAIN, client);
	INIT_COMPLETION(isp->completion);
	isp_boot(isp, prop);

	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		isp_clk_enable(isp, ISP_CLK_CSI, client);
		isp_mipi_init(isp,
			prop->if_id,
			client->mipi_lane_num,
			(int)(client->flags & CAMERA_CLIENT_YUV_DATA));
	} else if (client->flags & CAMERA_CLIENT_IF_DVP) {
		if (prop->parm)
			dvp_vsync_polar = prop->parm->dvp_vsync_polar;
		isp_dvp_init(isp, dvp_vsync_polar);
	}

	if (prop->setting.setting && prop->setting.size) {
		for (i = 0; i < prop->setting.size; i++) {
			isp_reg_writeb(isp, prop->setting.setting[i].val,
				prop->setting.setting[i].reg);
		}
	}
	else {
		for (i = 0; i < ARRAY_SIZE(default_isp_regs_init); i++) {
			isp_reg_writeb(isp, default_isp_regs_init[i].value,
				default_isp_regs_init[i].reg_num);
		}
	}

	if (prop->aecgc_control)
		isp_enable_aecgc_writeback(isp, 0);
	else
		isp_enable_aecgc_writeback(isp, 1);

	/* set single window & binning. */
	afc_ctl0 = isp_reg_readb(isp, 0x66100);
	afc_ctl0 &= ~ (1 << 1);
	isp_reg_writeb(isp, afc_ctl0, 0x66100);

	isp->first_init = 1;

	return 0;
}

static int isp_config(struct isp_device *isp, void *data)
{
	return 0;
}

static int isp_suspend(struct isp_device *isp, void *data)
{
	return 0;
}

static int isp_resume(struct isp_device *isp, void *data)
{
	return 0;
}

static int isp_mclk_on(struct isp_device *isp, void *data)
{

	struct comip_camera_client *client = data;

#if defined(CONFIG_CPU_LC1810)
	comip_mfp_config(MFP_PIN_GPIO(72), MFP_GPIO72_ISP_SCLK0);
#elif defined(CONFIG_CPU_LC1860)
	comip_mfp_config(MFP_PIN_GPIO(173), MFP_GPIO173_ISP_CLKOUT1);
#endif
	isp_clk_enable(isp, ISP_CLK_DEV, client);

	return 0;
}

static int isp_mclk_off(struct isp_device *isp, void *data)
{
	struct comip_camera_client *client = data;
	isp_clk_disable(isp, ISP_CLK_DEV, client);
#if defined(CONFIG_CPU_LC1810)
	comip_mfp_config(MFP_PIN_GPIO(72), MFP_PIN_MODE_GPIO);
#elif defined(CONFIG_CPU_LC1860)
	comip_mfp_config(MFP_PIN_GPIO(173), MFP_PIN_MODE_GPIO);
#endif
	return 0;
}

static int isp_update_buffer(struct isp_device *isp, struct isp_buffer *buf)
{
	isp->buf_start = *buf;
	if(isp->pp_buf){
		isp_set_address(isp, 0, buf->paddr);
		isp_reg_writeb(isp, 0x01, REG_BASE_ADDR_READY);
		isp->pp_buf = false;
	}else{
		isp_set_address(isp, 1, buf->paddr);
		isp_reg_writeb(isp, 0x02, REG_BASE_ADDR_READY);
		isp->pp_buf = true;
	}

	return 0;
}

static int isp_offline_process(struct isp_device *isp,
	unsigned long source_addr, unsigned long target_addr)
{
	struct isp_parm *iparm = &isp->parm;
	unsigned char iformat;
	unsigned char oformat;
	unsigned long uv_addr;
	u8 func_control = 0x0;
	uv_addr = target_addr + iparm->out_width * iparm->out_height;

	iformat = IFORMAT_RAW10;

	if ((iparm->out_format == V4L2_PIX_FMT_NV12) || (iparm->out_format == V4L2_PIX_FMT_NV21))
		oformat = OFFLINE_OFORMAT_NV12;
	else
		oformat = OFFLINE_OFORMAT_YUV422;

	isp_reg_writeb(isp, iformat, (ISP_INPUT_FORMAT));
	isp_reg_writeb(isp, oformat, (ISP_OUTPUT_FORMAT));

	isp_reg_writew(isp, iparm->in_width, ISP_OFFLINE_INPUT_WIDTH);
	isp_reg_writew(isp, iparm->in_height, ISP_OFFLINE_INPUT_HEIGHT);
	isp_reg_writew(isp, iparm->in_width, ISP_OFFLINE_MAC_READ_MEM_WIDTH);

	isp_reg_writew(isp, iparm->out_width, ISP_OFFLINE_OUTPUT_WIDTH);
	isp_reg_writew(isp, iparm->out_height, ISP_OFFLINE_OUTPUT_HEIGHT);
	isp_reg_writew(isp, iparm->out_width, ISP_OFFLINE_MAC_WRITE_MEM_WIDTH);

	isp_reg_writel(isp, (u32)source_addr, ISP_OFFLINE_INPUT_BASE_ADDR);
	isp_reg_writel(isp, (u32)target_addr, ISP_OFFLINE_OUTPUT_BASE_ADDR);
	isp_reg_writel(isp, (u32)uv_addr, ISP_OFFLINE_OUTPUT_BASE_ADDR_UV);

	if (oformat == OFFLINE_OFORMAT_NV12) {
		uv_addr = target_addr + iparm->out_width * iparm->out_height;
		isp_reg_writel(isp, uv_addr, ISP_OFFLINE_OUTPUT_BASE_ADDR_UV);
		isp_reg_writew(isp, iparm->out_width, ISP_OFFLINE_MAC_WRITE_WIDTH_UV);
	}

	if ((iparm->crop_width != iparm->in_width)
			|| (iparm->crop_height != iparm->in_height))
		func_control |= FUNCTION_YUV_CROP;

	if (iparm->crop_width > iparm->out_width) {
		func_control |= FUNCTION_SCALE_DOWN;
		isp_reg_writeb(isp, func_control, ISP_OFFLINE_FUNCTION_CTRL);
		isp_reg_writeb(isp, iparm->ratio_dcw, ISP_OFFLINE_SCALE_DOWN_H_RATIO1);
		isp_reg_writeb(isp,  iparm->ratio_d, ISP_OFFLINE_SCALE_DOWN_H_RATIO2);
		isp_reg_writeb(isp, iparm->ratio_dcw, ISP_OFFLINE_SCALE_DOWN_V_RATIO1);
		isp_reg_writeb(isp,   iparm->ratio_d, ISP_OFFLINE_SCALE_DOWN_V_RATIO2);
		isp_reg_writew(isp, 0x00, ISP_OFFLINE_SCALE_UP_H_RATIO);
		isp_reg_writew(isp, 0x00, ISP_OFFLINE_SCALE_UP_V_RATIO);
	} else if (iparm->crop_width < iparm->out_width) {
		func_control |= FUNCTION_SCALE_UP;
		isp_reg_writeb(isp, func_control, ISP_OFFLINE_FUNCTION_CTRL);
		isp_reg_writeb(isp, 0x00, ISP_OFFLINE_SCALE_DOWN_H_RATIO1);
		isp_reg_writeb(isp, 0x00, ISP_OFFLINE_SCALE_DOWN_H_RATIO2);
		isp_reg_writeb(isp, 0x00, ISP_OFFLINE_SCALE_DOWN_V_RATIO1);
		isp_reg_writeb(isp, 0x00, ISP_OFFLINE_SCALE_DOWN_V_RATIO2);
		isp_reg_writew(isp, (iparm->crop_width * 0x100) / iparm->out_width,
			ISP_OFFLINE_SCALE_UP_H_RATIO);
		isp_reg_writew(isp, (iparm->crop_height * 0x100) / iparm->out_height,
			ISP_OFFLINE_SCALE_UP_V_RATIO);
	} else
		isp_reg_writeb(isp, func_control | FUNCTION_NO_SCALE, ISP_OFFLINE_FUNCTION_CTRL);

	isp_reg_writew(isp, iparm->crop_x, ISP_OFFLINE_CROP_H_START);
	isp_reg_writew(isp, iparm->crop_y, ISP_OFFLINE_CROP_V_START);
	isp_reg_writew(isp, iparm->crop_width, ISP_OFFLINE_CROP_WIDTH);
	isp_reg_writeb(isp, iparm->crop_height, ISP_OFFLINE_CROP_HEIGHT);

	isp_reg_writeb(isp, 0x40, COMMAND_REG5);

	if (isp_send_cmd(isp, CMD_OFFLINE_PROCESS, ISP_OFFLINE_TIMEOUT)) {
		ISP_ERR("Failed to wait offline set done!\n");
		return -ETIME;
	}

	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT))
	        || (CMD_OFFLINE_PROCESS != isp_reg_readb(isp, COMMAND_FINISHED))) {
		ISP_ERR("Failed to set offline process (%02x:%02x)\n",
		       isp_reg_readb(isp, COMMAND_RESULT),
		       isp_reg_readb(isp, COMMAND_FINISHED));
		return -EINVAL;
	}

	msleep(150);

	return 0;
}

static int isp_start_capture(struct isp_device *isp, struct isp_capture *cap)
{
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct isp_parm *iparm = &(isp->parm);
//	struct comip_camera_capture *capture = &camdev->capture;
	int ret = 0;
	unsigned char byteswitch;

	isp->snapshot = cap->snapshot;
	isp->snap_flag = cap->snap_flag;
	isp->client = cap->client;
	isp->buf_start = cap->buf;
	isp->save_raw = cap->save_raw;
	isp->load_raw = cap->load_raw;
	isp->load_preview_raw = cap->load_preview_raw;
	isp->process = cap->process;
	iparm->real_vts = cap->vts;

	atomic_set(&isp->isp_mac_disable, 0);

	if (!(isp->snapshot || isp->snap_flag)) {
		iparm->pre_framerate = isp->fmt_data.framerate;
		iparm->pre_vts = isp->fmt_data.vts;
		iparm->pre_framerate_div = isp->fmt_data.framerate_div;
	}
	else {
		iparm->sna_framerate = isp->fmt_data.framerate;
		iparm->sna_vts = isp->fmt_data.vts;
		iparm->sna_framerate_div = isp->fmt_data.framerate_div;
	}

	iparm->sensor_binning = isp->fmt_data.binning;

	/*
	* if we have hdr_capture on, make sure isp output size is the
	* full size of input size, because isp hdr module can only do
	* fullsize hdr processing.
	*/
	if (isp->snapshot && isp->process == ISP_PROCESS_HDR_YUV
		&& ((iparm->in_width != iparm->out_width)
		|| (iparm->in_height != iparm->out_height))) {
		ISP_PRINT("HDR snapshot: isp output_size(%d,%d) != input_size(%d,%d), disable HDR\n",
			iparm->out_width, iparm->out_height, iparm->in_width, iparm->in_height);
		isp->process = ISP_PROCESS_YUV;
	}

	isp_set_parameters(isp);

	isp_mac_int_unmask(isp, 0xffff);
	isp_int_unmask(isp, MASK_INT_CMDSET | MASK_INT_MAC);

	if (isp->parm.out_format == V4L2_PIX_FMT_NV12) {
		isp_set_byteswitch(isp, 0);
		byteswitch = 0;
	}
	else if (isp->parm.out_format == V4L2_PIX_FMT_NV21) {
		isp_set_byteswitch(isp, 2);
		byteswitch = 2;
	}
	else {
		isp_set_byteswitch(isp, 1);
		byteswitch = 1;
	}
	if (isp->snapshot) {
		if (isp->process == ISP_PROCESS_RAW
			|| isp->process == ISP_PROCESS_HDR_YUV
			|| isp->process == ISP_PROCESS_OFFLINE_YUV)
			isp_set_byteswitch(isp, 0x0);

		ret = isp_set_capture(isp);
	} else {
		ret = isp_set_format(isp);
	}
	if (ret)
		return ret;

	isp->running = 1;

	if (isp->snapshot) {
		if(isp->process == ISP_PROCESS_HDR_YUV) {
			if (isp->save_raw)
				comip_debugtool_save_raw_file_hdr(camdev);
			isp_set_byteswitch(isp, byteswitch);
			isp_full_hdr_process(isp, (unsigned long)camdev->bracket.first_frame.paddr,
				(unsigned long)camdev->bracket.second_frame.paddr, cap->buf.paddr);
			isp_vb2_buffer_done(isp);
			isp->hdr_drop_frames = 2;
		} else if (isp->process == ISP_PROCESS_OFFLINE_YUV) {
			if (isp->save_raw)
				comip_debugtool_save_raw_file(camdev);
			if (isp->load_raw)
				comip_debugtool_load_raw_file(camdev, COMIP_DEBUGTOOL_LOAD_RAW_FILENAME);
			isp_set_byteswitch(isp, byteswitch);
			isp_offline_process(isp, (unsigned long)camdev->offline.paddr,
				cap->buf.paddr);
			isp_vb2_buffer_done(isp);
		}
	}

	isp_set_win_setting(isp, iparm->in_width, iparm->in_height);

	#ifdef COMIP_DEBUGTOOL_ENABLE
	if(!isp->debug.settingfile_loaded) {
		isp->debug.settingfile_loaded =
			comip_debugtool_load_isp_setting(isp, COMIP_DEBUGTOOL_ISPSETTING_FILENAME);
		comip_debugtool_load_regs_blacklist(isp, COMIP_DEBUGTOOL_REGS_BLACKLIST_FILENAME);
	}
	#endif
	return 0;
}


static int isp_vb2_buffer_done(struct isp_device *isp)
{
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_frame *frame = &camdev->frame;
	struct comip_camera_buffer *buf;

	buf = capture->active;
	capture->last = buf;
	list_del(&buf->list);

	buf->vb.v4l2_buf.field = frame->field;
	buf->vb.v4l2_buf.sequence = capture->out_frames++;
	do_gettimeofday(&buf->vb.v4l2_buf.timestamp);

	vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);

	return 0;
}


static int isp_stop_capture(struct isp_device *isp, void *data)
{
	isp->running = 0;
	isp->hdr_bracket_exposure_state = 0;
	isp_int_mask(isp, 0xff);
	isp_mac_int_mask(isp, 0xffff);
	isp_reg_writeb(isp, 0x00, REG_BASE_ADDR_READY);
	isp_int_state(isp);
	isp_mac_int_state(isp);

	if (isp->snapshot && isp->client->flash && isp->flash) {
		isp->client->flash(FLASH, 0);
		isp->flash = 0;
	}

	return 0;
}

static int isp_enable_capture(struct isp_device *isp, struct isp_buffer *buf)
{

	//struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	//struct comip_camera_capture *capture = &camdev->capture;

	ISP_PRINT("%s\n", __func__);
	//isp_int_state(isp);
	//isp_mac_int_state(isp);
	isp->buf_start = *buf;
	atomic_set(&isp->isp_mac_disable, 0);
/*
	if (capture->aecgc_control)
		isp_mac_int_unmask(isp, 0xffff);
	else
		isp_mac_int_unmask(isp, 0xff);
*/
	return 0;
}

static int isp_disable_capture(struct isp_device *isp, void *data)
{
	atomic_set(&isp->isp_mac_disable, 1);
	//isp_mac_int_mask(isp, 0xffff);
	//isp_reg_writeb(isp, 0x00, REG_BASE_ADDR_READY);
	ISP_PRINT("%s\n", __func__);

	return 0;
}

static int isp_check_fmt(struct isp_device *isp, struct isp_format *f)
{
	switch (f->fourcc) {
		case V4L2_PIX_FMT_YUYV:
			break;
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			break;
		/* Now, we don't support yuv420sp. */
		default:
			return -EINVAL;
	}

	return 0;
}

static int isp_try_fmt(struct isp_device *isp, struct isp_format *f)
{
	int ret;

	ret = isp_check_fmt(isp, f);
	if (ret)
		return ret;

	return 0;
}

static int isp_pre_fmt(struct isp_device *isp, struct isp_prop *prop, struct isp_format *f)
{
	int ret = 0;

	if (prop->mipi && (isp->fmt_data.mipi_clk != f->fmt_data->mipi_clk)) {
		ret = csi_phy_start((unsigned int)prop->if_id,
				(unsigned int)prop->mipi_lane_num, f->fmt_data->mipi_clk);
		if (!ret)
			isp->fmt_data.mipi_clk = f->fmt_data->mipi_clk;
	}

	return ret;
}

static int isp_s_fmt(struct isp_device *isp, struct isp_format *f)
{
	struct isp_parm *iparm = &isp->parm;
	int in_width, in_height;

	in_width = f->dev_width;
	in_height = f->dev_height;

	/* Save the parameters. */
	iparm->in_width = in_width;
	iparm->in_height = in_height;
	iparm->in_format = f->code;
	iparm->out_width = f->width;
	iparm->out_height = f->height;
	iparm->out_format = f->fourcc;
	iparm->crop_width = iparm->in_width;
	iparm->crop_height = iparm->in_height;
	iparm->crop_x = 0;
	iparm->crop_y = 0;

	memcpy(&isp->fmt_data, f->fmt_data, sizeof(isp->fmt_data));

	return 0;
}

static struct isp_regval_list isp_print_reg_list[] = {
	{0x63c22, 0x00},
	{0x63c23, 0x00},
	{0x63c24, 0x00},
	{0x63c25, 0x00},
	{0x65037, 0x00},
	{0x65038, 0x00},
	{0x65039, 0x00},
	{0x6503a, 0x00},
	{REG_ISP_MAC_INT_EN_H, 0x00},
	{REG_ISP_MAC_INT_EN_L, 0x00},
};

static int isp_print_reg_infor(struct isp_device *isp, int val)
{
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	unsigned short isp_mac_disable;
	unsigned short loop_value;

	for (loop_value = 0;loop_value < (sizeof(isp_print_reg_list)/sizeof(isp_print_reg_list[0]));loop_value++) {
		isp_print_reg_list[loop_value].value = isp_reg_readb(isp,
			isp_print_reg_list[loop_value].reg_num);
	}

	isp_mac_disable = atomic_read(&isp->isp_mac_disable);

	for (loop_value = 0;loop_value < (sizeof(isp_print_reg_list)/sizeof(isp_print_reg_list[0]));loop_value++) {
		isp_print_reg_list[loop_value].value = isp_reg_readb(isp,
			isp_print_reg_list[loop_value].reg_num);
		ISP_PRINT("reg_0x%x_val = 0x%02x\n",isp_print_reg_list[loop_value].reg_num,
			isp_print_reg_list[loop_value].value);
	}

	ISP_PRINT("isp_mac_disable = %d\n     in_frames = %d\n     drop_frames = %d\n\
	  error_frames = %d\n     lose_frames = %d\n     out_frames = %d\n",
		isp_mac_disable,camdev->capture.in_frames,camdev->capture.drop_frames,
		camdev->capture.error_frames,camdev->capture.lose_frames,camdev->capture.out_frames);

	return 0;
}

static int isp_set_sna_zsl_flash(struct isp_device *isp)
{
	struct isp_parm *iparm = &isp->parm;
	//int aecgc_stable = 0;
	unsigned int exp = 0, gain = 0;
	unsigned int banding_step = 0;
	unsigned int brightness = 0;
	unsigned int brightness_ratio = 0;

	brightness_ratio = isp->flash_parms->brightness_ratio;

	isp_get_pre_exp(isp, &exp);
	isp_get_pre_gain(isp, &gain);
	isp_get_banding_step(isp, &banding_step);
	isp_set_aecgc_enable(isp, 0);

	exp = (exp << 4) & 0xfffffff0;
	exp = (exp*10)/brightness_ratio;
	brightness = exp * gain;
	if(exp > banding_step) {
		if((exp%banding_step) > (banding_step/2)){
			exp =(exp/banding_step+1)* banding_step;
		}
		else
			exp =(exp/banding_step)* banding_step;
	}
	gain = brightness/exp;
	if(gain >= iparm->max_gain)
		gain = iparm->max_gain;

	isp_set_pre_exp(isp, exp);
	isp_set_pre_gain(isp, gain);
	isp->sna_exp = exp>>4;
	isp->sna_gain = gain;

	return 0;
}

static void isp_print_debug_info(struct isp_device *isp)
{
	int exp = 0, gain = 0, brightness = 0;
	int awb_bgain = 0, awb_ggain = 0, awb_rgain = 0;
	int ct_awb_shift = 0, ct_lens_ccm = 0;

	isp_get_pre_exp(isp, &exp);
	isp_get_pre_gain(isp, &gain);
	brightness = (exp * gain) >> 2;

	awb_bgain = isp_reg_readw(isp, REG_ISP_AWB_BGAIN1);
	awb_ggain = isp_reg_readw(isp, REG_ISP_AWB_GGAIN1);
	awb_rgain = isp_reg_readw(isp, REG_ISP_AWB_RGAIN1);

	ct_awb_shift = (awb_rgain << 7) / awb_bgain;
	ct_lens_ccm = (awb_bgain << 8)  / awb_rgain;

	ISP_PRINT("ispdebug exp=0x%04x gain=0x%04x brightness=0x%04x bgain=0x%04x ggain=0x%04x rgain=0x%04x ct_awb_shift=0x%04x ct_lens_ccm=0x%04x\n",
				exp << 4, gain, brightness,
				awb_bgain, awb_ggain, awb_rgain,
				ct_awb_shift, ct_lens_ccm);
}

static int isp_set_sna_exp_mode(struct isp_device *isp, int sna_exp_mode)
{
	int ret = 0;

	isp_print_debug_info(isp);

	switch(sna_exp_mode) {
		case SNA_NORMAL:
			break;
		case SNA_NORMAL_FLASH:
			break;
		case SNA_ZSL_NORMAL:
			isp_get_pre_exp(isp, &isp->sna_exp);
			isp_get_pre_gain(isp, &isp->sna_gain);
			break;
		case SNA_ZSL_FLASH:
			ret = isp_set_sna_zsl_flash(isp);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int isp_s_ctrl(struct isp_device *isp, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct isp_parm *iparm = &isp->parm;
	struct v4l2_rect rect;

	switch (ctrl->id) {
	case V4L2_CID_DO_WHITE_BALANCE:
		ISP_PRINT("set white_balance %d\n", ctrl->value);
		ret = isp_set_white_balance(isp, ctrl->value);
		if (!ret)
			iparm->white_balance = ctrl->value;
		break;
	case V4L2_CID_BRIGHTNESS:
		ISP_PRINT("set brightness %d\n", ctrl->value);
		ret = isp_set_brightness(isp, ctrl->value);
		if (!ret)
			iparm->brightness = ctrl->value;
		break;
	case V4L2_CID_EXPOSURE:
		ISP_PRINT("set exposure %d\n", ctrl->value);
		ret = isp_set_exposure(isp, ctrl->value);
		if (!ret)
			iparm->exposure = ctrl->value;
		break;
	case V4L2_CID_CONTRAST:
		ISP_PRINT("set contrast %d\n", ctrl->value);
		ret = isp_set_contrast(isp, ctrl->value);
		if (!ret)
			iparm->contrast = ctrl->value;
		break;
	case V4L2_CID_SATURATION:
		ISP_PRINT("set saturation %d\n", ctrl->value);
		if (isp->effect_ops && isp->effect_ops->get_ct_saturation)
			ret = 0;
		else
			ret = isp_set_saturation(isp, ctrl->value);
		if (!ret)
			iparm->saturation = ctrl->value;
		break;
	case V4L2_CID_SHARPNESS:
		ISP_PRINT("set sharpness %d\n", ctrl->value);
		ret = isp_set_sharpness(isp, ctrl->value);
		if (!ret)
			iparm->sharpness = ctrl->value;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ISP_PRINT("set flicker %d\n", ctrl->value);
		ret = isp_set_flicker(isp, ctrl->value);
		if (!ret)
			iparm->flicker = ctrl->value;
		break;
	case V4L2_CID_FOCUS_AUTO:
		ISP_PRINT("set focus mode %d\n", ctrl->value);
		ret = isp_set_vcm_range(isp,ctrl->value);
		if(!ret)
			iparm->focus_mode = ctrl->value;
		else
			ISP_PRINT("set focus mode error %d\n", ctrl->value);
		break;
	case V4L2_CID_FOCUS_RELATIVE:
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		break;
	case V4L2_CID_ZOOM_RELATIVE:
		ISP_PRINT("set zoom %d\n", ctrl->value);
		if (isp->running)
			ret = isp_set_zoom(isp, ctrl->value);
		if (!ret)
			iparm->zoom = ctrl->value;
		break;
	case V4L2_CID_HFLIP:
		ISP_PRINT("set hflip %d\n", ctrl->value);
		ret = isp_set_hflip(isp, ctrl->value);
		if (!ret)
			iparm->hflip = ctrl->value;
		break;
	case V4L2_CID_VFLIP:
		ISP_PRINT("set vflip %d\n", ctrl->value);
		ret = isp_set_vflip(isp, ctrl->value);
		if (!ret)
			iparm->vflip = ctrl->value;
		break;
	/* Private. */
	case V4L2_CID_ISO:
		ISP_PRINT("set iso %d\n", ctrl->value);
		ret = isp_set_iso(isp, ctrl->value);
		if (!ret)
			iparm->iso = ctrl->value;
		break;
	case V4L2_CID_EFFECT:
		ISP_PRINT("set effect %d\n", ctrl->value);
		ret = isp_set_effect(isp, ctrl->value);
		if (!ret)
			iparm->effects = ctrl->value;
		break;
	case V4L2_CID_FLASH_MODE:
		ISP_PRINT("set flash mode %d\n", ctrl->value);
		iparm->flash_mode = ctrl->value;
		break;
	case V4L2_CID_SCENE:
		ISP_PRINT("set scene %d\n", ctrl->value);
		if (isp->running)
			ret = isp_set_scene(isp, ctrl->value);
		if (!ret)
			iparm->scene_mode = ctrl->value;
		break;
	case V4L2_CID_FRAME_RATE:
		ISP_PRINT("set framerate %d\n", ctrl->value);
		iparm->frame_rate = ctrl->value;
		break;
	case V4L2_CID_SET_FOCUS:
		ISP_PRINT("set focus onoff = %d\n", ctrl->value);
		if (!isp->running)
			break;
		ret = isp_set_autofocus(isp,ctrl->value);
		if(ret)
			ISP_PRINT("failed set focus onoff\n");
		break;
   	case V4L2_CID_SET_FOCUS_RECT:
		if (copy_from_user(&rect, (const void __user *)ctrl->value,
				sizeof(rect))) {
			ISP_ERR("get focus rect failed.\n");
			return -EACCES;
		}
		iparm->focus_rect = rect;
		iparm->focus_rect_set_flag = 1;
		break;
	case V4L2_CID_SET_METER_RECT:
		ISP_PRINT("set meter rect\n");
		if (copy_from_user(&rect, (const void __user *)ctrl->value,
				sizeof(rect))) {
			ISP_ERR("get meter rect failed.\n");
			return -EACCES;
		}

		iparm->meter_rect = rect;
		ISP_PRINT("get metering rect (%d,%d,%d,%d)\n",iparm->meter_rect.left,iparm->meter_rect.top,iparm->meter_rect.width,iparm->meter_rect.height);
		if ((iparm->meter_rect.width == 0) || (iparm->meter_rect.height == 0)) {
			ret = isp_set_light_meters(isp,iparm->meter_mode);;
		}
		else {
			ret = isp_set_metering(isp, &(isp->parm.meter_rect));
			if(ret)
				ISP_ERR("set metering faild.\n");
		}
		if(iparm->meter_mode == METER_MODE_DOT)
			iparm->meter_set_flag = 1;
		break;
	case V4L2_CID_SET_METER_MODE:
		iparm->meter_mode = ctrl->value;
		ISP_PRINT("set meter mode = %d\n",ctrl->value);
		if(isp->running) {
			ret = isp_set_light_meters(isp,iparm->meter_mode);
			if (ret){
				ISP_ERR("set light meters error .\n");
				return ret;
			}
		}
		break;
	case V4L2_CID_SET_ANTI_SHAKE_CAPTURE:
		ISP_PRINT("set anti-shake %d\n", ctrl->value);
		iparm->anti_shake = ctrl->value;
		break;
	case V4L2_CID_NO_DATA_DEBUG:
		ret = isp_print_reg_infor(isp, ctrl->value);
		if (!ret)
			ISP_ERR("NO_DATA_DEBUG printk reg infor success \n");
		else
			ISP_ERR("NO_DATA_DEBUG printk reg infor fail \n");
		break;
	case V4L2_CID_MAX_EXP:
		ISP_PRINT("set max exp 0x%04x\n", ctrl->value);
		ret = isp_set_max_exp(isp, ctrl->value);
		break;
	case V4L2_CID_VTS:
		ISP_PRINT("set vts 0x%04x\n", ctrl->value);
		ret = isp_set_vts(isp, ctrl->value);
		if (!ret)
			iparm->real_vts = ctrl->value;
		break;
	case V4L2_CID_MIN_GAIN:
		ISP_PRINT("set min gain 0x%04x\n", ctrl->value);
		ret = isp_set_min_gain(isp, (u32)ctrl->value);
		if (!ret)
			iparm->auto_min_gain = (u32)ctrl->value;
		break;
	case V4L2_CID_MAX_GAIN:
		ISP_PRINT("set max gain 0x%04x\n", ctrl->value);
		ret = isp_set_max_gain(isp, (u32)ctrl->value);
		if (!ret)
			iparm->auto_max_gain = (u32)ctrl->value;
		break;
	case V4L2_CID_READ_REGISTER:
		ctrl->value = isp_reg_readb(isp, ctrl->value);
		break;
	case V4L2_CID_SET_SNA_EXP_MODE:
		ISP_PRINT("set sna exp mode 0x%04x\n", ctrl->value);
		isp->sna_exp_mode = ctrl->value;
		ret = isp_set_sna_exp_mode(isp, ctrl->value);
		break;
	case V4L2_CID_RESTART_FOCUS:
		ISP_PRINT("gyro trigger caf\n");
		if (!isp->running)
			break;
		ret = isp_trigger_caf(isp);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int isp_g_ctrl(struct isp_device *isp, struct v4l2_control *ctrl)
{
	int ret = 0;
	unsigned int tmp = 0;
	//unsigned short cur_gain = 0;
	unsigned char down_small_thres = 0, down_large_thres = 0;
	unsigned char up_small_thres = 0, up_large_thres = 0;
	struct isp_parm *iparm = &isp->parm;
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)(isp->data);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
#ifdef CONFIG_CAMERA_DRIVING_RECODE
	struct isp_pre_parm *pre_parm = &isp->pre_parm;
#endif
	switch (ctrl->id) {
	case V4L2_CID_DO_WHITE_BALANCE:
		ctrl->value = iparm->white_balance;
		break;
	case V4L2_CID_BRIGHTNESS:
		ctrl->value = iparm->brightness;
		break;
	case V4L2_CID_EXPOSURE:
		ctrl->value = iparm->exposure;
		break;
	case V4L2_CID_CONTRAST:
		ctrl->value = iparm->contrast;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value = iparm->saturation;
		break;
	case V4L2_CID_SHARPNESS:
		ctrl->value = iparm->sharpness;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ctrl->value = iparm->flicker;
		break;
	case V4L2_CID_FOCUS_AUTO:
		break;
	case V4L2_CID_FOCUS_RELATIVE:
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		break;
	case V4L2_CID_ZOOM_RELATIVE:
		ctrl->value = iparm->zoom;
		break;
	case V4L2_CID_HFLIP:
		ctrl->value = iparm->hflip;
		break;
	case V4L2_CID_VFLIP:
		ctrl->value = iparm->vflip;
		break;
	/* Private. */
	case V4L2_CID_ISO:
		ret = isp_get_iso(isp, &ctrl->value);
		if (ret)
			ISP_ERR("Get iso value failed.\n");
		break;
	case V4L2_CID_EFFECT:
		ctrl->value = iparm->effects;
		break;
	case V4L2_CID_FLASH_MODE:
		ctrl->value = iparm->flash_mode;
		break;
	case V4L2_CID_SCENE:
		ctrl->value = iparm->scene_mode;
		break;
	case V4L2_CID_FRAME_RATE:
		ctrl->value = iparm->frame_rate;
		break;
	case V4L2_CID_AUTO_FOCUS_RESULT:
		ctrl->value = isp_autofocus_result(isp);//(isp_focus_is_active(isp)) ? isp_autofocus_result(isp) : (isp_autofocus_result(isp), 1);
		break;
	case V4L2_CID_GET_AECGC_STABLE:
		 if(isp->client->flags & CAMERA_CLIENT_YUV_DATA) {
			ret=0;
			break;
		 }
		ret = isp_get_aecgc_stable(isp, &ctrl->value);
		if (ret)
			ISP_ERR("failed to get isp aecgc stable status\n");
		else
			ISP_PRINT("isp aecgc status %s\n", ctrl->value ? "stable" : "unstable");
		break;
	case V4L2_CID_GET_BRIGHTNESS_LEVEL:
		if(csd->yuv){
			ctrl->id = V4L2_CID_GET_BRIGHTNESS_LEVEL;
			ret = v4l2_subdev_call(csd->sd, core, g_ctrl, ctrl);
		}
		else
		ret = isp_get_brightness_level(isp, &ctrl->value);
		if (ret)
			ISP_PRINT("failed to get isp brightness level\n");
		else
			ISP_PRINT("isp brightness level %d\n", ctrl->value);
		break;
	case V4L2_CID_MIN_GAIN:
		ctrl->value = iparm->min_gain;
		break;
	case V4L2_CID_MAX_GAIN:
		ctrl->value = iparm->max_gain;
		break;
	case V4L2_CID_SNAPSHOT_GAIN:
		/*cur_gain = isp_reg_readb(isp, 0x1e95e);
		cur_gain <<= 8;
		cur_gain += isp_reg_readb(isp, 0x1e95f);
		ctrl->value = cur_gain;*/
		//ret = isp_get_pre_gain(isp, &ctrl->value);
		ctrl->value = isp->sna_gain;
		break;
	case V4L2_CID_SHUTTER_SPEED:
		isp->shutter_speed = isp_calc_shutter_speed(isp);
		ctrl->value = isp->shutter_speed;
		break;
	case V4L2_CID_PRE_GAIN:
	    ret = isp_get_pre_gain(isp, &ctrl->value);
	    break;
	case V4L2_CID_PRE_EXP:
		ret = isp_get_pre_exp(isp, &ctrl->value);
		break;
	case V4L2_CID_SNA_GAIN:
		ret = isp_get_sna_gain(isp, &ctrl->value);
		break;
	case V4L2_CID_SNA_EXP:
		ret = isp_get_sna_exp(isp, &ctrl->value);
		break;
	case V4L2_CID_DOWNFRM_QUERY:
		memcpy(&tmp, &(ctrl->value), sizeof(int));
		down_small_thres = (tmp >> 24) & 0xff;
		down_large_thres = (tmp >> 16) & 0xff;
		up_small_thres = (tmp >> 8) & 0xff;
		up_large_thres = tmp & 0xff;
		ret = isp_get_dynfrt_cond(isp,
								(int)down_small_thres,
								(int)down_large_thres,
								(int)up_small_thres,
								(int)up_large_thres);
		break;
	case V4L2_CID_VTS:
		ctrl->value = iparm->real_vts;
		break;
#ifdef CONFIG_CAMERA_DRIVING_RECODE
	case V4L2_CID_GET_MANUAL_FOUCS_SHARPNESS:
		ctrl->value = iparm->focus_sharpness;
		break;

	case V4L2_CID_GET_PREVIEW_PARM:
		if (copy_to_user((void __user *)(ctrl->value), pre_parm, sizeof(struct isp_pre_parm))) 
			ISP_ERR("get preview parm failed.\n");
		break;
#endif
	case V4L2_CID_DENOISE:
		if(isp->sna_gain < DENOISE_GAIN_THRESHOLD)
			ctrl->value = 0;
		else
			ctrl->value = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int isp_s_parm(struct isp_device *isp, struct v4l2_streamparm *parm)
{
	return 0;
}

static int isp_g_parm(struct isp_device *isp, struct v4l2_streamparm *parm)
{
	return 0;
}

static int isp_get_effect_func(struct isp_device *isp, struct v4l2_subdev *sd)
{
	struct v4l2_control ctrl;
	int ret = 0;

	ctrl.id = V4L2_CID_GET_EFFECT_FUNC;
	ret = v4l2_subdev_call(sd, core, g_ctrl, &ctrl);
	if (ret < 0)
		isp_get_func(isp, &isp->effect_ops);
	else
		isp->effect_ops = (struct isp_effect_ops *)ctrl.value;

	return 0;
}

int isp_set_bandfilter_enbale(struct isp_device* isp, int enable)
{
	isp_reg_writeb(isp, (u8)enable, REG_ISP_BANDFILTER_EN);
	return 0;
}

static int isp_set_exp_table(struct isp_device * isp, struct isp_exp_table *exp_table)
{
	struct isp_parm *iparm = &isp->parm;

	isp->exp_table_enable = 1;

	isp_reg_writeb(isp, (u8)exp_table->exp_table_enable, REG_ISP_EXP_TABLE_ENABLE);

	isp_reg_writeb(isp, (u8)exp_table->vts_gain_ctrl_1, REG_ISP_VTS_GAIN_CTRL_1);
	isp_reg_writeb(isp, (u8)exp_table->vts_gain_ctrl_2, REG_ISP_VTS_GAIN_CTRL_2);
	isp_reg_writeb(isp, (u8)exp_table->vts_gain_ctrl_3, REG_ISP_VTS_GAIN_CTRL_3);

	isp_reg_writew(isp, (u16)exp_table->gain_ctrl_1, REG_ISP_GAIN_CTRL_1);
	isp_reg_writew(isp, (u16)exp_table->gain_ctrl_2, REG_ISP_GAIN_CTRL_2);
	isp_reg_writew(isp, (u16)exp_table->gain_ctrl_3, REG_ISP_GAIN_CTRL_3);

	iparm->auto_gain_ctrl_1 = exp_table->gain_ctrl_1;
	iparm->auto_gain_ctrl_2 = exp_table->gain_ctrl_2;
	iparm->auto_gain_ctrl_3 = exp_table->gain_ctrl_3;

	return 0;
}

static int isp_set_spot_meter_win_size(struct isp_device * isp, struct isp_spot_meter_win *spot_meter_win)
{
	isp->parm.spot_meter_win_width = spot_meter_win->win_width;
	isp->parm.spot_meter_win_height = spot_meter_win->win_height;

	return 0;
}

static int isp_lock(struct isp_device *isp)
{
	return mutex_lock_interruptible(&isp->mlock);
	return 0;
}

static int isp_unlock(struct isp_device *isp)
{
	mutex_unlock(&isp->mlock);
	return 0;
}

static int isp_blacklist_add_reg(struct isp_device *isp, unsigned int reg)
{
	if (isp->debug.blacklist_len >= COMIP_ISP_REGS_BLACKLIST_LEN)
		return -1;

	isp->debug.regs_blacklist[isp->debug.blacklist_len++] = reg;

	return 0;
}

static int isp_reg_in_blacklist(struct isp_device *isp, unsigned int reg)
{
	int ret = 0;
	int loop = 0;

	for (loop = 0; loop < isp->debug.blacklist_len; loop++) {
		if (isp->debug.regs_blacklist[loop] == reg) {
			ret = 1;
			break;
		}
	}

	return ret;
}

static struct isp_ops isp_ops = {
	.init = isp_init,
	.release = isp_release,
	.open = isp_open,
	.close = isp_close,
	.config = isp_config,
	.suspend = isp_suspend,
	.resume = isp_resume,
	.mclk_on = isp_mclk_on,
	.mclk_off = isp_mclk_off,
	.start_capture = isp_start_capture,
	.stop_capture = isp_stop_capture,
	.enable_capture = isp_enable_capture,
	.disable_capture = isp_disable_capture,
	.update_buffer = isp_update_buffer,
	.check_fmt = isp_check_fmt,
	.try_fmt = isp_try_fmt,
	.pre_fmt = isp_pre_fmt,
	.s_fmt = isp_s_fmt,
	.s_ctrl = isp_s_ctrl,
	.g_ctrl = isp_g_ctrl,
	.s_parm = isp_s_parm,
	.g_parm = isp_g_parm,
	.set_autofocus = isp_set_autofocus_area,
	.set_metering = isp_set_metering ,
	.set_camera_flash = camera_isp_flash,
	.get_effect_func = isp_get_effect_func,
	.set_bandfilter_enbale = isp_set_bandfilter_enbale,
	.set_exp_table = isp_set_exp_table,
	.set_aecgc_enable = isp_set_aecgc_enable,
	.esd_reset = isp_esd_reset,
	.set_spot_meter_win_size = isp_set_spot_meter_win_size,
	.lock = isp_lock,
	.unlock = isp_unlock,
	.blacklist_add_reg = isp_blacklist_add_reg,
	.reg_in_blacklist = isp_reg_in_blacklist,
};

int __init isp_device_init(struct isp_device* isp)
{
	struct resource *res = isp->res;
	int ret = 0;

	isp->anti_shake_parms = &(isp->pdata->anti_shake_parms);
	isp->flash_parms = &(isp->pdata->flash_parms);
	memset(&isp->parm, 0, sizeof(isp->parm));
	spin_lock_init(&isp->lock);
	mutex_init(&isp->mlock);
	init_completion(&isp->completion);
	init_completion(&isp->raw_completion);
	isp->isp_workqueue = create_singlethread_workqueue("isp");
	INIT_WORK(&isp->saturation_work, saturation_work_handler);
	INIT_WORK(&isp->lens_threshold_work, lens_threshold_work_handler);

	atomic_set(&isp->isp_mac_disable, 0);

	isp->base = ioremap(res->start, res->end - res->start + 1);
	if (!isp->base) {
		ISP_ERR("Unable to ioremap registers.n");
		ret = -ENXIO;
		goto exit;
	}

	ret = isp_int_init(isp);
	if (ret)
		goto io_unmap;

	ret = isp_i2c_init(isp);
	if (ret)
		goto irq_free;
	csi_phy_init();

	isp->ops = &isp_ops;

	return 0;

irq_free:
	free_irq(isp->irq, isp);
io_unmap:
	iounmap(isp->base);
exit:
	return ret;
}
EXPORT_SYMBOL(isp_device_init);

int isp_device_release(struct isp_device* isp)
{
	if (!isp)
		return -EINVAL;

	csi_phy_release();
	isp_clk_release(isp);
	isp_i2c_release(isp);
	free_irq(isp->irq, isp);
	iounmap(isp->base);

	return 0;
}
EXPORT_SYMBOL(isp_device_release);

static int isp_full_hdr_process(struct isp_device *isp, unsigned long short_addr,unsigned long long_addr, unsigned long target_addr)
{
	struct isp_parm *iparm = &isp->parm;
	unsigned char iformat;
	unsigned char oformat;
	unsigned int target_uv_addr = 0;

	iformat = IFORMAT_RAW10;

	if ((iparm->out_format == V4L2_PIX_FMT_NV21) || (iparm->out_format == V4L2_PIX_FMT_NV12))
		oformat = OFFLINE_OFORMAT_NV12;
	else
		oformat = OFFLINE_OFORMAT_YUV422;

	isp_reg_writeb(isp, iformat, (ISP_INPUT_FORMAT));
	isp_reg_writeb(isp, oformat, (ISP_OUTPUT_FORMAT));

	isp_reg_writeb(isp, (iparm->out_width >> 8) & 0xff, (ISP_HDR_INPUT_WIDTH));
	isp_reg_writeb(isp, iparm->out_width & 0xff, (ISP_HDR_INPUT_WIDTH + 1));
	isp_reg_writeb(isp, (iparm->out_height >> 8) & 0xff, (ISP_HDR_INPUT_HEIGHT));
	isp_reg_writeb(isp, iparm->out_height & 0xff, (ISP_HDR_INPUT_HEIGHT + 1));

	isp_reg_writeb(isp, (iparm->out_width >> 8) & 0xff, (ISP_HDR_MAC_READ_WIDTH));
	isp_reg_writeb(isp, iparm->out_width & 0xff, (ISP_HDR_MAC_READ_WIDTH + 1));

	isp_reg_writeb(isp, (iparm->out_width >> 8) & 0xff, (ISP_HDR_OUTPUT_WIDTH));
	isp_reg_writeb(isp, iparm->out_width & 0xff, (ISP_HDR_OUTPUT_WIDTH + 1));
	isp_reg_writeb(isp, (iparm->out_height >> 8) & 0xff, (ISP_HDR_OUTPUT_HEIGHT));
	isp_reg_writeb(isp, iparm->out_height & 0xff, (ISP_HDR_OUTPUT_HEIGHT + 1));
	isp_reg_writeb(isp, (iparm->out_width >> 8) & 0xff, (ISP_HDR_MAC_WRITE_WIDTH));
	isp_reg_writeb(isp, iparm->out_width & 0xff, (ISP_HDR_MAC_WRITE_WIDTH + 1));

	isp_reg_writeb(isp, 0x10, (ISP_HDR_PROCESS_CONTROL));

	isp_reg_writeb(isp, ((u32)long_addr >>24) & 0xff, (ISP_HDR_INPUT_BASE_ADDR_LONG));
	isp_reg_writeb(isp, ((u32)long_addr>>16) & 0xff, (ISP_HDR_INPUT_BASE_ADDR_LONG + 1));
	isp_reg_writeb(isp, ((u32)long_addr>>8) & 0xff, (ISP_HDR_INPUT_BASE_ADDR_LONG + 2));
	isp_reg_writeb(isp, (u32)long_addr & 0xff, (ISP_HDR_INPUT_BASE_ADDR_LONG + 3));

	isp_reg_writeb(isp, ((u32)short_addr >>24) & 0xff, (ISP_HDR_INPUT_BASE_ADDR_SHORT));
	isp_reg_writeb(isp, ((u32)short_addr>>16) & 0xff, (ISP_HDR_INPUT_BASE_ADDR_SHORT + 1));
	isp_reg_writeb(isp, ((u32)short_addr>>8) & 0xff, (ISP_HDR_INPUT_BASE_ADDR_SHORT + 2));
	isp_reg_writeb(isp, (u32)short_addr & 0xff, (ISP_HDR_INPUT_BASE_ADDR_SHORT + 3));

	isp_reg_writeb(isp, ((u32)target_addr >>24) & 0xff, (ISP_HDR_OUTPUT_BASE_ADDR));
	isp_reg_writeb(isp, ((u32)target_addr>>16) & 0xff, (ISP_HDR_OUTPUT_BASE_ADDR + 1));
	isp_reg_writeb(isp, ((u32)target_addr>>8) & 0xff, (ISP_HDR_OUTPUT_BASE_ADDR + 2));
	isp_reg_writeb(isp, (u32)target_addr & 0xff, (ISP_HDR_OUTPUT_BASE_ADDR + 3));

	isp_reg_writeb(isp, 0x20, (ISP_HDR_OVERLAP_SIZE));

	if (oformat == OFFLINE_OFORMAT_NV12) {
		target_uv_addr = target_addr + iparm->out_width * iparm->out_height;
		isp_reg_writel(isp, target_uv_addr, ISP_HDR_OUTPUT_BASE_ADDR_UV);
		isp_reg_writew(isp, iparm->out_width / 2, ISP_HDR_MAC_WRITE_UV_WIDTH);
	}

	/* configure long / short exposure & gain .*/
	isp_reg_writeb(isp, isp->hdr[0], (ISP_HDR_SHORT_EXPOSURE));
	isp_reg_writeb(isp, isp->hdr[1], (ISP_HDR_SHORT_EXPOSURE + 1));
	isp_reg_writeb(isp, isp->hdr[2], (ISP_HDR_SHORT_GAIN));
	isp_reg_writeb(isp, isp->hdr[3], (ISP_HDR_SHORT_GAIN + 1));

	isp_reg_writeb(isp, isp->hdr[4], (ISP_HDR_LONG_EXPOSURE));
	isp_reg_writeb(isp, isp->hdr[5], (ISP_HDR_LONG_EXPOSURE + 1));
	isp_reg_writeb(isp, isp->hdr[6], (ISP_HDR_LONG_GAIN));
	isp_reg_writeb(isp, isp->hdr[7], (ISP_HDR_LONG_GAIN + 1));
	isp_reg_writeb(isp, 0x00, REG_BASE_ADDR_READY);

	isp_reg_writeb(isp, 0x40, COMMAND_REG5);

	if (isp_send_cmd(isp, CMD_FULL_HDR_PROCESS, ISP_OFFLINE_TIMEOUT)) {
		ISP_ERR("Failed to wait full_size hdr set done!\n");
		return -ETIME;
	}

	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT))
	        || (CMD_FULL_HDR_PROCESS != isp_reg_readb(isp, COMMAND_FINISHED))) {
		ISP_ERR("Failed to set full_size_hdr process (%02x:%02x)\n",
		       isp_reg_readb(isp, COMMAND_RESULT),
		       isp_reg_readb(isp, COMMAND_FINISHED));
		return -EINVAL;
	}

	return 0;
}

static void saturation_work_handler(struct work_struct *work)
{
	struct isp_device *isp = container_of(work, struct isp_device, saturation_work);
	struct isp_parm *parm = &isp->parm;

	if (!isp->running || isp->snapshot)
		return;

	isp_set_ct_saturation(isp, parm->saturation);
}

static void lens_threshold_work_handler(struct work_struct * work)
{
	struct isp_device *isp = container_of(work, struct isp_device, lens_threshold_work);

	if (!isp->running || isp->snapshot)
		return;

	isp_set_lens_threshold(isp);
}


