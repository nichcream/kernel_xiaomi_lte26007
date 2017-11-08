
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>

#include <plat/comip-smmu.h>

#include "comip-isp-dev.h"
#include "comip-video.h"
#include "comip-videobuf.h"
#include "camera-debug.h"

//#define COMIP_CAMERA_DEBUG
#ifdef COMIP_CAMERA_DEBUG
#define CAMERA_PRINT(fmt, args...) printk(KERN_ERR "camera: " fmt, ##args)
#else
#define CAMERA_PRINT(fmt, args...) printk(KERN_DEBUG "camera: " fmt, ##args)
#endif

#define STREAM_OFF_TIMEOUT	(500)

#if defined (CONFIG_VIDEO_COMIP_TORCH_LED)
#include <linux/leds.h>
#include <plat/comip-pmic.h>
#define TORCH_OFF		(0)
#define TORCH_ON		(1)
static struct led_classdev flashlight;
#endif

struct comip_camera_dev *cam_dev = NULL;

static struct comip_camera_format formats[] = {
	{
		.name     = "YUV 4:2:2 packed, YCbYCr",
		.code	  = V4L2_MBUS_FMT_SBGGR8_1X8,
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
	},
	{
		.name     = "YUV 4:2:0 semi planar, Y/CbCr",
		.code	  = V4L2_MBUS_FMT_SBGGR8_1X8,
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	},
	{
		.name     = "YUV 4:2:0 semi planar, Y/CrCb",
		.code	  = V4L2_MBUS_FMT_SBGGR8_1X8,
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
	},
};

static struct comip_camera_format yuv_formats[] = {
	{
		.name     = "YUV 4:2:2 packed, YCbYCr",
		.code	  = V4L2_MBUS_FMT_YUYV8_2X8,
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
	},
	{
		.name     = "YUV 4:2:0 semi planar, Y/CbCr",
		.code     = V4L2_MBUS_FMT_YUYV8_2X8,
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	},
	{
		.name     = "YUV 4:2:0 semi planar, Y/CrCb",
		.code     = V4L2_MBUS_FMT_YUYV8_2X8,
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
	},

};

#if defined (CONFIG_VIDEO_COMIP_TORCH_LED)
static void comip_torch_onoff_set(struct led_classdev *led_cdev,enum led_brightness torch_onoff);
#endif

#if defined(CONFIG_CPU_LC1813)
static int camera_flash_func(enum camera_led_mode mode, int onoff)
{
	struct isp_gpio_flash flash_param;
	struct isp_device *isp = cam_dev->isp;

	flash_param.mode = mode;
	flash_param.onoff = onoff;
	flash_param.brightness = isp->flash_parms->flash_brightness;
	isp_dev_call(isp, set_camera_flash, &flash_param);

	return 0;
}
#endif

static int comip_subdev_mclk_on(struct comip_camera_dev *camdev,
			struct comip_camera_client *client,
			int index)
{
	int ret;

	if (camdev->csd[index].mclk) {
		clk_enable(camdev->csd[index].mclk);
	} else {
		ret = isp_dev_call(camdev->isp, mclk_on, client);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	return 0;
}

static int comip_subdev_mclk_off(struct comip_camera_dev *camdev,
			struct comip_camera_client *client,
			int index)
{
	int ret;

	if (camdev->csd[index].mclk) {
		clk_disable(camdev->csd[index].mclk);
	} else {
		ret = isp_dev_call(camdev->isp, mclk_off, client);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	return 0;
}

static int comip_subdev_power_on(struct comip_camera_dev *camdev,
					struct comip_camera_client *client,
					int index)
{
	struct comip_camera_subdev *csd = &camdev->csd[index];
	struct comip_camera_capture *capture = &camdev->capture;
	struct v4l2_control ctrl;
	struct v4l2_cropcap caps;
	unsigned char sensor_vendor;
	int ret = 0;

	ret = comip_subdev_mclk_on(camdev, client, index);
	if (ret)
		return ret;

	if (client->power) {
		ret = client->power(1);
		if (ret)
			goto err;
	}

	if (client->reset) {
		ret = client->reset();
		if (ret)
			goto err;
	}

	csd->first = 1;
	csd->framerate = FRAME_RATE_MAX;
	csd->prop.index = index;
	csd->prop.if_id = client->if_id;
	csd->prop.mipi = (client->flags & CAMERA_CLIENT_IF_MIPI) ? 1 : 0;
	csd->prop.mipi_lane_num = client->mipi_lane_num;
	csd->prop.if_pip = camdev->if_pip;
	if (camdev->input >= 0) {
		if (client->caps & CAMERA_CAP_VIV_CAPTURE) {
			if(camdev->if_pip) {
				ctrl.id = V4L2_CID_SET_VIV_MODE;
				ctrl.value = VIV_ON;
				ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
				if (ret < 0 && ret != -ENOIOCTLCMD)
					goto err;
			} else {
				ctrl.id = V4L2_CID_SET_VIV_MODE;
				ctrl.value = VIV_OFF;
				ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
				if (ret < 0 && ret != -ENOIOCTLCMD)
				goto err;
			}
		}
		ctrl.id = V4L2_CID_ISP_SETTING;
		ctrl.value = (int)&csd->prop.setting;
		ret = v4l2_subdev_call(csd->sd, core, g_ctrl, &ctrl);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;

		ctrl.id = V4L2_CID_ISP_PARM;
		ret = v4l2_subdev_call(csd->sd, core, g_ctrl, &ctrl);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;

		csd->prop.parm = (struct v4l2_isp_parm *)ctrl.value;
		if (!csd->yuv)
			capture->aecgc_control =
				comip_camera_get_aecgc_control(csd->prop.parm->sensor_vendor);
		else capture->aecgc_control = NULL;
		ret = isp_dev_call(camdev->isp, get_effect_func, csd->sd);

		ctrl.id = V4L2_CID_GET_ISP_FIRMWARE;
		ctrl.value = (int)&csd->prop.firmware_setting;
		ret = v4l2_subdev_call(csd->sd, core, g_ctrl, &ctrl);
		if (ret)
			csd->prop.firmware_setting.size = 0;

	} else {
		csd->prop.setting.setting = NULL;
		csd->prop.setting.size = 0;
		csd->prop.parm = NULL;
		capture->aecgc_control = NULL;
	}

	csd->prop.aecgc_control = capture->aecgc_control;

	ret = isp_dev_call(camdev->isp, open, &csd->prop);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		goto err;

	if (camdev->input >= 0) {
		ret = v4l2_subdev_call(csd->sd, core, s_power, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;

		ret = v4l2_subdev_call(csd->sd, core, reset, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;

		ret = v4l2_subdev_call(csd->sd, core, init, 1);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;

		sensor_vendor = csd->prop.parm->sensor_vendor;
		if ((sensor_vendor == SENSOR_VENDOR_ANY) || (sensor_vendor == SENSOR_VENDOR_OMNIVISION)) {
			ret = v4l2_subdev_call(csd->sd, video, s_stream, 0);
			if (ret < 0 && ret != -ENOIOCTLCMD)
				goto err;
		}

		ret = isp_dev_call(camdev->isp, config, NULL);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;

		ret = v4l2_subdev_call(csd->sd, video, cropcap, &caps);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			goto err;
		csd->max_width = caps.bounds.width;
		csd->max_height = caps.bounds.height;
		csd->max_video_width = caps.defrect.width;
		csd->max_video_height = caps.defrect.height;
		csd->max_frame_size = csd->max_width * csd->max_height * 2;
	}

	return 0;

err:
	comip_subdev_mclk_off(camdev, client, index);
	return -EINVAL;
}

static int comip_subdev_power_off(struct comip_camera_dev *camdev,
					struct comip_camera_client *client,
					int index)
{
	struct comip_camera_subdev *csd = &camdev->csd[index];
	int ret;

	if (camdev->input >= 0) {
		ret = v4l2_subdev_call(csd->sd, core, s_power, 0);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	ret = isp_dev_call(camdev->isp, close, &csd->prop);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	if (client->power) {
		ret = client->power(0);
		if (ret)
			return -EINVAL;
	}

	comip_subdev_mclk_off(camdev, client, index);

	return 0;
}

struct comip_camera_format *comip_camera_find_format(
					struct comip_camera_dev *camdev,
					struct v4l2_format *f)
{
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_format *fmt;
	unsigned int num;
	unsigned int i;

	if (csd->yuv) {
		fmt = yuv_formats;
		num = ARRAY_SIZE(yuv_formats);
	} else {
		fmt = formats;
		num = ARRAY_SIZE(formats);
	}

	for (i = 0; i < num; i++) {
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
		fmt++;
	}

	if (i == num)
		return NULL;

	return fmt;
}

static int comip_camera_get_mclk(struct comip_camera_dev *camdev,
					struct comip_camera_client *client,
					int index)
{
	struct comip_camera_subdev *csd = &camdev->csd[index];
	struct clk* mclk_parent;

	if (!(client->flags & CAMERA_CLIENT_CLK_EXT) || !client->mclk_name) {
		csd->mclk = NULL;
		return 0;
	}

	csd->mclk = clk_get(camdev->dev, client->mclk_name);
	if (IS_ERR(csd->mclk)) {
		CAMERA_PRINT("Cannot get sensor input clock \"%s\"\n",
			client->mclk_name);
		return PTR_ERR(csd->mclk);
	}

	if (client->mclk_parent_name) {
		mclk_parent = clk_get(camdev->dev, client->mclk_parent_name);
		if (IS_ERR(mclk_parent)) {
			CAMERA_PRINT("Cannot get sensor input parent clock \"%s\"\n",
				client->mclk_parent_name);
			clk_put(csd->mclk);
			return PTR_ERR(mclk_parent);
		}

		clk_set_parent(csd->mclk, mclk_parent);
		clk_put(mclk_parent);
	}

	clk_set_rate(csd->mclk, client->mclk_rate);

	return 0;
}

static int comip_camera_init_client(struct comip_camera_dev *camdev,
					struct comip_camera_client *client,
					int index)
{
	struct comip_camera_subdev *csd = &camdev->csd[index];

	int i2c_adapter_id;
	int err = 0;
	int ret;

	if (!client->board_info) {
		CAMERA_PRINT("Invalid client info\n");
		return -EINVAL;
	}

	if (client->flags & CAMERA_CLIENT_INDEP_I2C)
		i2c_adapter_id = client->i2c_adapter_id;
	else
		i2c_adapter_id = camdev->pdata->i2c_adapter_id;

	if (client->flags & CAMERA_CLIENT_YUV_DATA)
		csd->yuv = 1;
	else
		csd->yuv = 0;

	ret = comip_camera_get_mclk(camdev, client, index);
	if (ret)
		return ret;

	ret = isp_dev_call(camdev->isp, init, client);
	if (ret) {
		CAMERA_PRINT("Failed to init isp\n");
		clk_put(csd->mclk);
		return -EINVAL;
	}

	ret = comip_subdev_power_on(camdev, client, index);
	if (ret) {
		CAMERA_PRINT("Failed to power on subdev(%s)\n",
			client->board_info->type);
		clk_put(csd->mclk);
		err = -ENODEV;
		goto isp_dev_release;
	}

	csd->i2c_adap = i2c_get_adapter(i2c_adapter_id);
	if (!csd->i2c_adap) {
		CAMERA_PRINT("Cannot get I2C adapter(%d)\n",
			i2c_adapter_id);
		clk_put(csd->mclk);
		err = -ENODEV;
		goto subdev_power_off;
	}

	csd->sd = v4l2_i2c_new_subdev_board(&camdev->v4l2_dev,
		csd->i2c_adap,
		client->board_info,
		NULL);
	if (!csd->sd) {
		CAMERA_PRINT("Cannot get subdev(%s)\n",
			client->board_info->type);
		clk_put(csd->mclk);
		i2c_put_adapter(csd->i2c_adap);
		err = -EIO;
		goto subdev_power_off;
	}

subdev_power_off:
	ret = comip_subdev_power_off(camdev, client, index);
	if (ret) {
		CAMERA_PRINT("Failed to power off subdev(%s)\n",
			client->board_info->type);
		err = -EINVAL;
	}

isp_dev_release:
	ret = isp_dev_call(camdev->isp, release, NULL);
	if (ret) {
		CAMERA_PRINT("Failed to init isp\n");
		err = -EINVAL;
	}

	return err;
}

static void comip_camera_free_client(struct comip_camera_dev *camdev,
			int index)
{
	struct comip_camera_subdev *csd = &camdev->csd[index];

	v4l2_device_unregister_subdev(csd->sd);
	i2c_unregister_device(v4l2_get_subdevdata(csd->sd));
	i2c_put_adapter(csd->i2c_adap);
	clk_put(csd->mclk);
}

static int comip_camera_init_subdev(struct comip_camera_dev *camdev)
{
	struct comip_camera_platform_data *pdata = camdev->pdata;
	unsigned int i;
	int ret;

	if (!pdata->client) {
		CAMERA_PRINT("Invalid client data\n");
		return -EINVAL;
	}

	camdev->clients = 0;
	for (i = 0; i < pdata->client_num; i++) {
		ret = comip_camera_init_client(camdev,
					&pdata->client[i], camdev->clients);
		if (ret)
			continue;

		camdev->csd[camdev->clients++].client = &pdata->client[i];
		if (camdev->clients > COMIP_CAMERA_CLIENT_NUM) {
			CAMERA_PRINT("Too many clients\n");
			return -EINVAL;
		}
#if defined(CONFIG_CPU_LC1813)
		if ((pdata->client[i].flash == NULL) && (pdata->client[i].caps & CAMERA_CAP_FLASH)) {
			CAMERA_PRINT("client flash is NULL,Use isp gpio flash\n");
			comip_mfp_config(MFP_PIN_GPIO(229), MFP_GPIO229_ISP_FSIN0);
			comip_mfp_config(MFP_PIN_GPIO(71), MFP_GPIO71_ISP_PWM);
			pdata->client[i].flash = camera_flash_func;
		}
#endif
	}

	CAMERA_PRINT("Detect %d clients\n", camdev->clients);

	if (camdev->clients == 0)
		return -ENODEV;

	return 0;
}

static void comip_camera_free_subdev(struct comip_camera_dev *camdev)
{
	unsigned int i;

	for (i = 0; i < camdev->clients; i++)
		comip_camera_free_client(camdev, i);
}

static int comip_camera_active(struct comip_camera_dev *camdev)
{
	struct comip_camera_capture *capture = &camdev->capture;

	return capture->running;
}

static int comip_camera_update_buffer(struct comip_camera_dev *camdev)
{
	struct comip_camera_capture *capture = &camdev->capture;
	struct isp_buffer buf;
	int ret;

	buf.paddr = comip_vb2_plane_paddr(&capture->active->vb, 0);
	ret = isp_dev_call(camdev->isp, update_buffer, &buf);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	return 0;
}

static int comip_camera_enable_capture(struct comip_camera_dev *camdev)
{
	struct comip_camera_capture *capture = &camdev->capture;
	struct isp_buffer buf;
	int ret;

	buf.paddr = comip_vb2_plane_paddr(&capture->active->vb, 0);
	ret = isp_dev_call(camdev->isp, enable_capture, &buf);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	return 0;
}

static int comip_camera_disable_capture(struct comip_camera_dev *camdev)
{
	int ret;

	ret = isp_dev_call(camdev->isp, disable_capture, NULL);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	return 0;
}

static int comip_camera_start_capture(struct comip_camera_dev *camdev)
{
	struct comip_camera_capture *capture = &camdev->capture;
	struct isp_capture cap;
	int ret;

	if (capture->running || !capture->active)
		return 0;

	CAMERA_PRINT("Start capture(%s)\n",
				camdev->snapshot ? "snapshot" : "preview");

	cap.buf.paddr = comip_vb2_plane_paddr(&capture->active->vb, 0);
	cap.snapshot = camdev->snapshot;
	cap.snap_flag = camdev->snap_flag;
	cap.save_raw = camdev->save_raw;
	cap.load_preview_raw = camdev->load_preview_raw;
	cap.load_raw = camdev->load_raw;
	cap.vts = capture->cur_vts;

	if (camdev->hdr)
		cap.process = ISP_PROCESS_HDR_YUV;
	else {
		if ((camdev->save_raw)||(camdev->load_raw))
			if (camdev->offline.enable)
				cap.process = ISP_PROCESS_OFFLINE_YUV;
			else
				cap.process = ISP_PROCESS_RAW;
		else
			cap.process = ISP_PROCESS_YUV;
	}

	cap.client = camdev->csd[camdev->input].client;

	ret = isp_dev_call(camdev->isp, start_capture, &cap);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		CAMERA_PRINT("Start capture failed %d\n", ret);
		return -EINVAL;
	}

	capture->running = 1;

#if defined (CONFIG_VIDEO_COMIP_TORCH_LED)
	if(camdev->csd[0].client->flash && (flashlight.brightness)){
		comip_torch_onoff_set(&flashlight,TORCH_OFF);
	}
#endif
	return 0;
}

static int comip_camera_stop_capture(struct comip_camera_dev *camdev)
{
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	unsigned char sensor_vendor = csd->prop.parm->sensor_vendor;
	int ret, v = 0;
	struct v4l2_control ctrl;

	CAMERA_PRINT("Stop capture(%s)\n",
				camdev->snapshot ? "snapshot" : "preview");

	if (!camdev->snapshot) {
		ctrl.id = V4L2_CID_PRE_EXP;
		isp_dev_call(camdev->isp, g_ctrl, &ctrl);
		capture->preview_aecgc_parm.exp = ctrl.value;

		ctrl.id = V4L2_CID_PRE_GAIN;
		isp_dev_call(camdev->isp, g_ctrl, &ctrl);
		capture->preview_aecgc_parm.gain = ctrl.value;
	}

	if (!((camdev->hdr || camdev->save_raw) && camdev->snapshot)) {
		atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
		ret = wait_event_interruptible_timeout(capture->stream_off_wait,
			(v = atomic_read(&capture->frm_trans_status) >= FRM_TRANS_EOF),STREAM_OFF_TIMEOUT);
		if (!ret) {
			CAMERA_PRINT("Stream off wait EOF failed!\n");
		}
	}

	capture->active = NULL;
	capture->running = 0;

	if (capture->aecgc_control && capture->aecgc_control->wait_finish)
		capture->aecgc_control->wait_finish();

	ret = isp_dev_call(camdev->isp, stop_capture, NULL);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	if (!(((sensor_vendor == SENSOR_VENDOR_ANY) || (sensor_vendor == SENSOR_VENDOR_OMNIVISION)) /*&& camdev->save_raw*/)) {
		ret = v4l2_subdev_call(csd->sd, video, s_stream, 0);
		if (ret && ret != -ENOIOCTLCMD)
			CAMERA_PRINT("Failed to stream off!\n");
	}



	return 0;
}

static int comip_camera_start_streaming(struct comip_camera_dev *camdev)
{
	int ret = 0;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	unsigned char sensor_vendor = csd->prop.parm->sensor_vendor;
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_frame *frame = &camdev->frame;
	struct isp_prop *prop = &csd->prop;
	struct comip_camera_aecgc_control *aecgc_control = capture->aecgc_control;
	struct v4l2_control ctrl;
	struct v4l2_fmt_data *fmt_data = frame->ifmt.fmt_data;
	struct isp_exp_table exp_table;
	struct isp_spot_meter_win spot_meter_win;

	isp_dev_call(camdev->isp, lock);

	capture->running = 0;

	atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	atomic_set(&capture->sna_enable, 1);

	if (camdev->snap_flag) {
		frame->ifmt.dev_width = csd->max_width;
		frame->ifmt.dev_height = csd->max_height;
		frame->vmfmt.width = frame->ifmt.dev_width;
		frame->vmfmt.height = frame->ifmt.dev_height;
	}
	CAMERA_PRINT("Set format(%s). ISP %dx%d,%x/%x. device %dx%d,%x\n",
		camdev->snap_flag ? "snapshot" : "preview",
		frame->ifmt.width, frame->ifmt.height,
		frame->ifmt.code, frame->ifmt.fourcc,
		frame->vmfmt.width, frame->vmfmt.height,
		frame->ifmt.code);

	if (csd->framerate == FRAME_RATE_MAX) {//app hasn't set framerate yet
		if (csd->client->flags & CAMERA_CLIENT_FRAMERATE_DYN)
			csd->framerate = FRAME_RATE_AUTO;
		else
			csd->framerate = FRAME_RATE_DEFAULT;
	}
	ctrl.id = V4L2_CID_FRAME_RATE;
	ctrl.value = csd->framerate;
	if (!csd->yuv)
		isp_dev_call(camdev->isp, s_ctrl, &ctrl);
	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	if (ret)
		CAMERA_PRINT("camera %d don't support framerate %d\n", camdev->input, ctrl.value);

	if (!camdev->snapshot && !csd->first) {
		fmt_data->aecgc_parm.exp = capture->preview_aecgc_parm.exp;
		fmt_data->aecgc_parm.gain = capture->preview_aecgc_parm.gain;
	} else {
		fmt_data->aecgc_parm.vts = COMIP_CAMERA_AECGC_INVALID_VALUE;
		fmt_data->aecgc_parm.exp = COMIP_CAMERA_AECGC_INVALID_VALUE;
		fmt_data->aecgc_parm.gain = COMIP_CAMERA_AECGC_INVALID_VALUE;
	}

	ret = v4l2_subdev_call(csd->sd, video, s_mbus_fmt, (struct v4l2_mbus_framefmt*)(&frame->vmfmt));
	if (ret && ret != -ENOIOCTLCMD) {
		CAMERA_PRINT("Failed to set device format\n");
		isp_dev_call(camdev->isp, unlock);
		return -EINVAL;
	}

	if ((!csd->yuv) && (!camdev->snapshot)) {
		ctrl.id = V4L2_CID_MAX_GAIN;
		if(csd->framerate == FRAME_RATE_AUTO)
			ctrl.value = fmt_data->max_gain_dyn_frm;
		else
			ctrl.value = fmt_data->max_gain_fix_frm;
		ret = isp_dev_call(camdev->isp, s_ctrl, &ctrl);


		ctrl.id = V4L2_CID_MIN_GAIN;
		if(csd->framerate == FRAME_RATE_AUTO)
			ctrl.value = fmt_data->min_gain_dyn_frm;
		else
			ctrl.value = fmt_data->min_gain_fix_frm;
		ret = isp_dev_call(camdev->isp, s_ctrl, &ctrl);

	}
	ret = isp_dev_call(camdev->isp, pre_fmt, prop, &frame->ifmt);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		CAMERA_PRINT("Failed to set isp format\n");
		isp_dev_call(camdev->isp, unlock);
		return -EINVAL;
	}

	if(fmt_data->gain_ctrl_1 && fmt_data->gain_ctrl_2 && fmt_data->gain_ctrl_3) {
		exp_table.exp_table_enable = 1;
		exp_table.gain_ctrl_1 = fmt_data->gain_ctrl_1;
		exp_table.gain_ctrl_2 = fmt_data->gain_ctrl_2;
		exp_table.gain_ctrl_3 = fmt_data->gain_ctrl_3;
		exp_table.vts_gain_ctrl_1 = fmt_data->vts_gain_ctrl_1;
		exp_table.vts_gain_ctrl_2 = fmt_data->vts_gain_ctrl_2;
		exp_table.vts_gain_ctrl_3 = fmt_data->vts_gain_ctrl_3;

		ret = isp_dev_call(camdev->isp, set_exp_table, &exp_table);
		if(ret)
			CAMERA_PRINT("Failed to set isp gain_ctrl!\n");
	}

	if(fmt_data->spot_meter_win_width && fmt_data->spot_meter_win_height) {
		spot_meter_win.win_width = fmt_data->spot_meter_win_width;
		spot_meter_win.win_height = fmt_data->spot_meter_win_height;

		ret = isp_dev_call(camdev->isp, set_spot_meter_win_size, &spot_meter_win);
		if(ret)
			CAMERA_PRINT("Failed to set isp spot_meter_win_siz!\n");
	}

	if(fmt_data->framerate_div == 0)
		fmt_data->framerate_div = 1;

	if (aecgc_control && aecgc_control->start_streaming) {
		ret = aecgc_control->start_streaming();
		if (ret) {
			CAMERA_PRINT("Failed to start streaming of aecgc control\n");
			isp_dev_call(camdev->isp, unlock);
			return ret;
		}
	}

	capture->cur_vts = fmt_data->vts;

	if (csd->first) {
		csd->first = 0;
		if ((sensor_vendor == SENSOR_VENDOR_ANY) || (sensor_vendor == SENSOR_VENDOR_OMNIVISION)){
			ret = v4l2_subdev_call(csd->sd, video, s_stream, 1);
			if (ret && ret != -ENOIOCTLCMD) {
				CAMERA_PRINT("Failed to set device stream\n");
				isp_dev_call(camdev->isp, unlock);
				return -EINVAL;
			}
		}
	}

	if (!(((sensor_vendor == SENSOR_VENDOR_ANY) || (sensor_vendor == SENSOR_VENDOR_OMNIVISION)) /*&& camdev->save_raw*/)){
		ret = v4l2_subdev_call(csd->sd, video, s_stream, 1);
		if (ret && ret != -ENOIOCTLCMD) {
			CAMERA_PRINT("Failed to set device stream\n");
			isp_dev_call(camdev->isp, unlock);
			return -EINVAL;
		}
	}

	ret = isp_dev_call(camdev->isp, s_fmt, &frame->ifmt);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		CAMERA_PRINT("Failed to set isp format\n");
		isp_dev_call(camdev->isp, unlock);
		return -EINVAL;
	}
	ret = comip_camera_start_capture(camdev);
	isp_dev_call(camdev->isp, unlock);
	return ret;
}

static int comip_camera_stop_streaming(struct comip_camera_dev *camdev)
{
	int status;

	isp_dev_call(camdev->isp, lock);

	if (!comip_camera_active(camdev)) {
		isp_dev_call(camdev->isp, unlock);
		return 0;
	}

	status = comip_camera_stop_capture(camdev);
	isp_dev_call(camdev->isp, unlock);
	return status;
}

static int comip_camera_try_format(struct comip_camera_dev *camdev)
{
	struct comip_camera_frame *frame = &camdev->frame;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret;

	frame->ifmt.width = frame->width;
	frame->ifmt.height = frame->height;
	frame->ifmt.dev_width = frame->width;//csd->max_width;
	frame->ifmt.dev_height = frame->height;//csd->max_height;
	frame->ifmt.code = frame->fmt->code;
	frame->ifmt.fourcc = frame->fmt->fourcc;
	frame->ifmt.fmt_data = v4l2_get_fmt_data(&frame->vmfmt);
	frame->ifmt.fmt_data->reg_num = 0;
	ret = isp_dev_call(camdev->isp, try_fmt, &frame->ifmt);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return -EINVAL;

	frame->vmfmt.width = frame->ifmt.dev_width;
	frame->vmfmt.height = frame->ifmt.dev_height;
	frame->vmfmt.code = frame->ifmt.code;
	ret = v4l2_subdev_call(csd->sd, video, try_mbus_fmt, (struct v4l2_mbus_framefmt*)(&frame->vmfmt));
	if (ret && ret != -ENOIOCTLCMD)
		return -EINVAL;

	if ((frame->vmfmt.width < frame->ifmt.dev_width)
		|| (frame->vmfmt.height < frame->ifmt.dev_height)
		|| (frame->vmfmt.code != frame->ifmt.code))
		return -EINVAL;

	frame->ifmt.dev_width = frame->vmfmt.width;
	frame->ifmt.dev_height = frame->vmfmt.height;

	return 0;
}

static int comip_camera_change_frm_trans_status(struct comip_camera_dev *camdev,
												unsigned int status)
{
	struct comip_camera_capture *capture = &camdev->capture;

	if (status & ISP_NOTIFY_SOF) {
		atomic_set(&capture->frm_trans_status, FRM_TRANS_SOF);
	}

	if (status & ISP_NOTIFY_DATA_START) {
		atomic_set(&capture->frm_trans_status, FRM_TRANS_WRITE_START);
	}

	if (status & ISP_NOTIFY_EOF) {
		atomic_set(&capture->frm_trans_status, FRM_TRANS_EOF);
	}

	if (status & ISP_NOTIFY_DATA_DONE) {
		atomic_set(&capture->frm_trans_status, FRM_TRANS_WRITE_DONE);
	}

	if (status & ISP_NOTIFY_DROP_FRAME) {
		atomic_set(&capture->frm_trans_status, FRM_TRANS_DROP);
	}

	if (status & ISP_NOTIFY_OVERFLOW) {
		atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	}

	wake_up_interruptible(&capture->stream_off_wait);
	return 0;
}

static int comip_camera_irq_notify(unsigned int status, void *data)
{
	struct comip_camera_dev *camdev = (struct comip_camera_dev *)data;
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_frame *frame = &camdev->frame;
	struct comip_camera_aecgc_control *aecgc_control = capture->aecgc_control;
	struct comip_camera_buffer *buf;

	unsigned long flags;

	comip_camera_change_frm_trans_status(camdev, status);

	if (status & ISP_NOTIFY_DATA_START) {
		capture->in_frames++;
	}
#ifdef CONFIG_VIDEO_COMIP_ISP
	if ((status & ISP_NOTIFY_DATA_DONE) && (!(camdev->save_raw && camdev->snapshot))
		&&(!(camdev->hdr && camdev->snapshot))){
#else
	if ((status & ISP_NOTIFY_DATA_DONE) && (!(camdev->save_raw && camdev->snapshot))){
#endif

	spin_lock_irqsave(&camdev->slock, flags);
		if (capture->active
			&& capture->active->vb.state == VB2_BUF_STATE_ACTIVE) {
			if (atomic_read(&capture->sna_enable)) {
				buf = capture->active;
				capture->last = buf;
				list_del(&buf->list);

				buf->vb.v4l2_buf.field = frame->field;
				buf->vb.v4l2_buf.sequence = capture->out_frames++;
				do_gettimeofday(&buf->vb.v4l2_buf.timestamp);

				vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);

				if (!list_empty(&capture->list))
					capture->active = list_entry(capture->list.next,
							struct comip_camera_buffer, list);
				else
					capture->active = NULL;
			}

			if (capture->active)
				comip_camera_update_buffer(camdev);
			else
				comip_camera_disable_capture(camdev);
		} else
		capture->lose_frames++;

		spin_unlock_irqrestore(&camdev->slock, flags);
	}

	if (status & ISP_NOTIFY_OVERFLOW)
		capture->error_frames++;

	if (status & ISP_NOTIFY_DROP_FRAME)
		capture->drop_frames++;

	if (aecgc_control && aecgc_control->isp_irq_notify)
		aecgc_control->isp_irq_notify(status);

	return 0;
}

static int comip_camera_memory_alloc(struct comip_camera_dev *camdev)
{
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_client *client = csd->client;
	unsigned int frame_size;
	void *paddr;
	void *vaddr;
	unsigned long size = 0;

	if (camdev->alloc_ctx)
		comip_vb2_cleanup_ctx(camdev->alloc_ctx);

	/* Initialize contiguous memory allocator */
	camdev->alloc_ctx = comip_vb2_init_ctx(camdev->dev);
	if (!camdev->alloc_ctx)
		return -ENOMEM;

	vaddr = comip_vb2_ctx_vaddr(camdev->alloc_ctx);
	if (IS_ERR(vaddr))
		camdev->vaddr_valid = 0;
	else camdev->vaddr_valid = 1;

	paddr = (void *)comip_vb2_ctx_paddr(camdev->alloc_ctx);
	size = comip_vb2_ctx_size(camdev->alloc_ctx);
	frame_size = csd->max_frame_size;

	if (frame_size * 2 <= size) {
		camdev->offline.size = frame_size;
		camdev->offline.vaddr = (void *)(vaddr + size - frame_size);
		camdev->offline.paddr = (void *)(paddr + size - frame_size);
		camdev->offline.enable = 1;
	} else {
		camdev->offline.enable = 0;
		CAMERA_PRINT("offline function requires isp memory size up to 0x%x.\n",
				frame_size * 2);
	}

	/* use hardware hdr embeded in ISP, we need 3 fullsize buffers, so check first */
	if (client->caps & CAMERA_CAP_HDR_CAPTURE) {
		if (frame_size * 3 <= size) {
			camdev->bracket.second_frame.size = frame_size;
			camdev->bracket.second_frame.vaddr = (void *)(vaddr +
					size - frame_size);
			camdev->bracket.second_frame.paddr = (void *)(paddr +
					size - frame_size);
			camdev->bracket.first_frame.size = frame_size;
			camdev->bracket.first_frame.vaddr =
					camdev->bracket.second_frame.vaddr - frame_size;
			camdev->bracket.first_frame.paddr =
					camdev->bracket.second_frame.paddr - frame_size;
			camdev->bracket.enable = 1;
			CAMERA_PRINT("use hardware bracket(HDR) function\n");
		} else {
			camdev->bracket.enable = 0;
			CAMERA_PRINT("bracket(HDR) function requires isp memory size up to 0x%x.\n",
					frame_size * 3);
		}
	}
	/* use software hdr, we only need one buffer, it's always ok*/
	else if (client->caps & CAMERA_CAP_SW_HDR_CAPTURE) {
		camdev->bracket.enable = 1;
		CAMERA_PRINT("use software bracket(HDR) function\n");
	}
	else {
		camdev->bracket.enable = 0;
		CAMERA_PRINT("bracket(HDR) function cap was disabled\n");
	}

	return 0;
}

static int comip_camera_memory_free(struct comip_camera_dev *camdev)
{
	if (camdev->alloc_ctx) {
		comip_vb2_cleanup_ctx(camdev->alloc_ctx);
		camdev->alloc_ctx = NULL;
	}

	camdev->vaddr_valid = 0;
	return 0;
}

static int comip_vb2_queue_setup(struct vb2_queue *vq,
				const struct v4l2_format *fmt,
				unsigned int *nbuffers,
				unsigned int *nplanes,
				unsigned int sizes[],
				void *alloc_ctxs[])
{
	struct comip_camera_dev *camdev = vb2_get_drv_priv(vq);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_frame *frame = &camdev->frame;
	struct comip_camera_client *client = csd->client;
	unsigned long total_size;
	unsigned long size;

	total_size = comip_vb2_ctx_size(camdev->alloc_ctx);
	size = (frame->width * frame->height * frame->fmt->depth) >> 3;

	size = PAGE_ALIGN(size);

	if (0 == *nbuffers)
		*nbuffers = 32;

	while (size * *nbuffers > total_size)
		(*nbuffers)--;

	*nplanes = 1;
	sizes[0] = size;
	alloc_ctxs[0] = camdev->alloc_ctx;

	/* in the case of hardware hdr, we must reserve 2 fullsize buffers for ISP */
	if (camdev->snapshot
		&& client->caps & CAMERA_CAP_HDR_CAPTURE
		&& camdev->bracket.enable
		&& camdev->hdr) {
		while (size * *nbuffers > total_size - 2 * csd->max_frame_size)
			(*nbuffers)--;

		if (*nbuffers <= 0) {
			CAMERA_PRINT("no enough memory for hdr capture\n");
			return -EINVAL;
		}
	}

	INIT_LIST_HEAD(&capture->list);
	capture->active = NULL;
	capture->running = 0;
	capture->drop_frames = 0;
	capture->lose_frames = 0;
	capture->error_frames = 0;
	capture->in_frames = 0;
	capture->out_frames = 0;

	return 0;
}

static int comip_vb2_buffer_init(struct vb2_buffer *vb)
{
	vb->v4l2_buf.reserved = comip_vb2_plane_paddr(vb, 0);

	return 0;
}

static int comip_vb2_buffer_prepare(struct vb2_buffer *vb)
{
	struct comip_camera_buffer *buf =
			container_of(vb, struct comip_camera_buffer, vb);
	struct comip_camera_dev *camdev = vb2_get_drv_priv(vb->vb2_queue);
	struct comip_camera_frame *frame = &camdev->frame;
	unsigned long size;

	if (NULL == frame->fmt) {
		CAMERA_PRINT("Format not set\n");
		return -EINVAL;
	}

	if (frame->width  < 48 || frame->width  > COMIP_CAMERA_WIDTH_MAX ||
	    frame->height < 32 || frame->height > COMIP_CAMERA_HEIGHT_MAX) {
		CAMERA_PRINT("Invalid format (%dx%d)\n",
				frame->width, frame->height);
		return -EINVAL;
	}

	size = (frame->width * frame->height * frame->fmt->depth) >> 3;
	if (vb2_plane_size(vb, 0) < size) {
		CAMERA_PRINT("Data will not fit into plane (%lu < %lu)\n",
				vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(&buf->vb, 0, size);

	vb->v4l2_buf.reserved = comip_vb2_plane_paddr(vb, 0);

	return 0;
}

static void comip_vb2_buffer_queue(struct vb2_buffer *vb)
{
	struct comip_camera_buffer *buf =
			container_of(vb, struct comip_camera_buffer, vb);
	struct comip_camera_dev *camdev = vb2_get_drv_priv(vb->vb2_queue);
	struct comip_camera_capture *capture = &camdev->capture;
	unsigned long flags;

	spin_lock_irqsave(&camdev->slock, flags);
	list_add_tail(&buf->list, &capture->list);

	if (!capture->active) {
		capture->active = buf;
		if (capture->running)
			comip_camera_enable_capture(camdev);
	}

	spin_unlock_irqrestore(&camdev->slock, flags);
}

static int comip_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct comip_camera_dev *camdev = vb2_get_drv_priv(vq);

	if (comip_camera_active(camdev))
		return -EBUSY;

	return comip_camera_start_streaming(camdev);
}

static int comip_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct comip_camera_dev *camdev = vb2_get_drv_priv(vq);

	if (!comip_camera_active(camdev))
		return -EINVAL;

	return comip_camera_stop_streaming(camdev);
}

static void comip_vb2_lock(struct vb2_queue *vq)
{
	struct comip_camera_dev *camdev = vb2_get_drv_priv(vq);
	mutex_lock(&camdev->mlock);
}

static void comip_vb2_unlock(struct vb2_queue *vq)
{
	struct comip_camera_dev *camdev = vb2_get_drv_priv(vq);
	mutex_unlock(&camdev->mlock);
}

static struct vb2_ops comip_vb2_qops = {
	.queue_setup		= comip_vb2_queue_setup,
	.buf_init		= comip_vb2_buffer_init,
	.buf_prepare		= comip_vb2_buffer_prepare,
	.buf_queue		= comip_vb2_buffer_queue,
	.wait_prepare		= comip_vb2_unlock,
	.wait_finish		= comip_vb2_lock,
	.start_streaming	= comip_vb2_start_streaming,
	.stop_streaming		= comip_vb2_stop_streaming,
};

static int comip_vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct comip_camera_dev *camdev = video_drvdata(file);

	strcpy(cap->driver, "comip");
	strcpy(cap->card, "comip");
	strlcpy(cap->bus_info, camdev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = COMIP_CAMERA_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}

static int comip_vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_frame *frame = &camdev->frame;
	struct comip_camera_format *fmt;
	int ret;

	if (csd->yuv) {
		if (f->index >= ARRAY_SIZE(yuv_formats))
			return -EINVAL;

		fmt = &yuv_formats[f->index];
	} else {
		if (f->index >= ARRAY_SIZE(formats))
			return -EINVAL;

		fmt = &formats[f->index];

		memset(&frame->ifmt, 0, sizeof(frame->ifmt));
		frame->ifmt.fourcc = fmt->fourcc;
		ret = isp_dev_call(camdev->isp, check_fmt, &frame->ifmt);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EINVAL;
	}

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int comip_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_frame *frame = &camdev->frame;

	f->fmt.pix.width        = frame->width;
	f->fmt.pix.height       = frame->height;
	f->fmt.pix.field        = frame->field;
	f->fmt.pix.pixelformat  = frame->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * frame->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	return 0;
}

static int comip_vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_frame *frame = &camdev->frame;
	int ret;

	if (f->fmt.pix.field == V4L2_FIELD_ANY)
		f->fmt.pix.field = V4L2_FIELD_INTERLACED;
	else if (V4L2_FIELD_INTERLACED != f->fmt.pix.field)
		return -EINVAL;

	frame->fmt = comip_camera_find_format(camdev, f);
	if (!frame->fmt) {
		CAMERA_PRINT("Fourcc format (0x%08x) invalid\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	v4l_bound_align_image(&f->fmt.pix.width,
			48, COMIP_CAMERA_WIDTH_MAX, 2,
			&f->fmt.pix.height,
			32, COMIP_CAMERA_HEIGHT_MAX, 0, 0);
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * frame->fmt->depth) >> 3;
	f->fmt.pix.bytesperline = ALIGN(f->fmt.pix.bytesperline, 4);
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	frame->width = f->fmt.pix.width;
	frame->height = f->fmt.pix.height;
	frame->field = f->fmt.pix.field;

	ret = comip_camera_try_format(camdev);
	if (ret) {
		CAMERA_PRINT("Format(%dx%d,%x/%x) is unsupported\n",
			f->fmt.pix.width,
			f->fmt.pix.height,
			frame->fmt->code,
			frame->fmt->fourcc);
		return ret;
	}

	return 0;
}

static int comip_vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct vb2_queue *q = &camdev->vbq;
	int ret;

	ret = comip_vidioc_try_fmt_vid_cap(file, priv, f);
	if (ret < 0)
		return ret;

	if (vb2_is_streaming(q))
		return -EBUSY;

	return 0;
}

static int comip_vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	return vb2_reqbufs(&camdev->vbq, p);
}

static int comip_vidioc_querybuf(struct file *file, void *priv,
			struct v4l2_buffer *p)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	return vb2_querybuf(&camdev->vbq, p);
}

static int comip_vidioc_qbuf(struct file *file, void *priv,
			struct v4l2_buffer *p)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	return vb2_qbuf(&camdev->vbq, p);
}

static int comip_vidioc_dqbuf(struct file *file, void *priv,
			struct v4l2_buffer *p)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	int status;
	status = vb2_dqbuf(&camdev->vbq, p, file->f_flags & O_NONBLOCK);

	/*
		save_yuv: 0x0 (disable), 0x1 (snapshot), 0x2 (preview),
				0x3 (snapshot & preview);
		save_raw: 0x0 (disable), 0x1 (snapshot).
	*/
	if (camdev->snapshot && camdev->save_raw && !camdev->offline.enable)
		comip_debugtool_save_raw_file(camdev);
	else if (camdev->snapshot && camdev->load_raw && !camdev->offline.enable)
		comip_debugtool_load_raw_file(camdev, COMIP_DEBUGTOOL_LOAD_RAW_FILENAME);
	else if (((camdev->save_yuv & 0x1) && camdev->snap_flag) ||
			((camdev->save_yuv & 0x2) && !camdev->snap_flag))
		comip_debugtool_save_yuv_file(camdev);

	return status;
}

static int comip_vidioc_streamon(struct file *file, void *priv,
			enum v4l2_buf_type i)
{
	struct comip_camera_dev *camdev = video_drvdata(file);

	if (comip_camera_active(camdev))
		return -EBUSY;

	return vb2_streamon(&camdev->vbq, i);
}

static int comip_vidioc_streamoff(struct file *file, void *priv,
			enum v4l2_buf_type i)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	return vb2_streamoff(&camdev->vbq, i);
}

static int comip_vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_client *client;

	if (inp->index >= camdev->clients)
		return -EINVAL;

	client = camdev->csd[inp->index].client;

	inp->capabilities = client->caps;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	strncpy(inp->name, client->board_info->type, I2C_NAME_SIZE);

	return 0;
}

static int comip_vidioc_g_input(struct file *file, void *priv,
			unsigned int *index)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	*index = camdev->input;
	return 0;
}

static int comip_vidioc_s_input(struct file *file, void *priv,
			unsigned int index)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	int ret;

	if (comip_camera_active(camdev))
		return -EBUSY;

	if (index == camdev->input)
		return 0;

	if (index >= camdev->clients) {
		CAMERA_PRINT("Input(%d) exceeds chip maximum(%d)\n",
			index, camdev->clients);
		return -EINVAL;
	}

	if (camdev->input >= 0) {
		/* Old client. */
		ret = comip_subdev_power_off(camdev,
				camdev->csd[camdev->input].client,
				camdev->input);
		if (ret) {
			CAMERA_PRINT("Failed to power off subdev\n");
			return ret;
		}

		comip_camera_memory_free(camdev);
	}

	/* New client. */
	camdev->input = index;

	ret = isp_dev_call(camdev->isp, init, camdev->csd[index].client);
	if (ret) {
		CAMERA_PRINT("Failed to init isp\n");
		ret = -EINVAL;
		goto err;
	}

	ret = comip_subdev_power_on(camdev,
		camdev->csd[index].client, index);
	if (ret) {
		CAMERA_PRINT("Failed to power on subdev\n");
		ret = -EIO;
		goto release_isp;
	}

	ret = comip_camera_memory_alloc(camdev);
	if (ret) {
		CAMERA_PRINT("Failed to alloc video memory\n");
		ret = -ENOMEM;
		goto release_isp;
	}

	CAMERA_PRINT("Select client %d\n", index);

	return 0;

release_isp:
	isp_dev_call(camdev->isp, release, NULL);
err:
	camdev->input = -1;
	return ret;
}

static int comip_camera_query_camera_index(struct comip_camera_dev *camdev,
						struct v4l2_query_index *query_index)
{
	int i = 0;

	for (i = 0; i < camdev->clients; i++) {
		if (camdev->csd[i].client->camera_id == query_index->camera_id) {
			query_index->index= i;
			break;
		}else if (camdev->csd[i].client->camera_id == 0) {
			query_index->index= -1;
			if(i == (camdev->clients - 1))
				break;
		}
	}
	if (i >= camdev->clients)
		query_index->index= -2;

	return 0;
}
#if COMIP_CAMERA_ESD_RESET_ENABLE
extern int csi_phy_start(unsigned int id, unsigned int lane_num, unsigned int freq);
int comip_camera_esd_reset(struct comip_camera_dev *camdev)
{
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_client *client = csd->client;
	struct isp_device *isp = camdev->isp;
	struct isp_prop *prop = &csd->prop;
	struct comip_camera_frame *frame = &camdev->frame;
	struct isp_format *f = &frame->ifmt;
	struct v4l2_control ctrl;
	struct comip_camera_capture *capture = &camdev->capture;

	CAMERA_PRINT("camera esd reset\n");

	//power off sensor
	v4l2_subdev_call(csd->sd, core, s_power, 0);

	if (client->power) {
		client->power(0);
	}

	//stop isp capture
	capture->running = 0;
	isp_dev_call(isp, stop_capture, NULL);

	//isp esd reset
	isp_dev_call(isp, esd_reset);

	//power on sensor
	if (client->power) {
		client->power(1);
	}

	if (client->reset) {
		client->reset();
	}

	//reset cphy
	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		csi_phy_start((unsigned int)prop->if_id, (unsigned int)prop->mipi_lane_num, f->fmt_data->mipi_clk);
	}
	//call sensor esd_reset function
	ctrl.id = V4L2_CID_SENSOR_ESD_RESET;
	v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	//start isp capture
	comip_camera_start_capture(camdev);
	return 0;
}
#else
int comip_camera_esd_reset(struct comip_camera_dev *camdev)
{
	return 0;
}
#endif

static int comip_vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_client *client = csd->client;
	struct isp_device *isp = camdev->isp;
	struct v4l2_flash_enable flash_en;
	struct v4l2_flash_duration flash_duration;
	struct v4l2_query_index query_index;
	struct v4l2_vm_area vm_area;
	unsigned char sensor_vendor;
	int ret = 0;

	if (ctrl->id == V4L2_CID_SET_HDR_CAPTURE) {
		if (camdev->bracket.enable)
			camdev->hdr = ctrl->value;
		else {
			CAMERA_PRINT("failed to set hdr value %d\n", ctrl->value);
			ret = -EINVAL;
		}
		return ret;
	} else if (ctrl->id == V4L2_CID_SNAPSHOT) {
		camdev->snapshot = ctrl->value;
		camdev->snap_flag = ctrl->value;
		CAMERA_PRINT("Take %s.\n",
		camdev->snapshot ? "snapshot" : "preview");
		sensor_vendor = csd->prop.parm->sensor_vendor;
		if (camdev->snapshot
			&& (camdev->save_raw || camdev->hdr || client->flags & CAMERA_CLIENT_YUV_DATA || sensor_vendor == SENSOR_VENDOR_OMNIVISION || sensor_vendor == SENSOR_VENDOR_ANY))
			camdev->snapshot = 1;
		else
			camdev->snapshot = 0;
	} else if (ctrl->id == V4L2_CID_SET_FLASH) {
		if (copy_from_user(&flash_en, (const void __user *)ctrl->value,
				sizeof(flash_en))) {
			printk(KERN_ERR "get flash_en failed.\n");
			return -EACCES;
		}
		if (csd->client->flash) {
			CAMERA_PRINT("now set flash %d mode = %d\n",
					 flash_en.on, flash_en.mode);
			csd->client->flash(flash_en.mode, flash_en.on);
			if(flash_en.mode == FLASH && (!flash_en.on)) {
				ret = isp_dev_call(camdev->isp, set_aecgc_enable, 1);
				if(ret)
					CAMERA_PRINT("Failed to set aecgc off!\n");
			}
		}
		else {
			CAMERA_PRINT("Camera %d has no flash binded\n", camdev->input);
		}

		return 0;
	} else if (ctrl->id == V4L2_CID_GET_PREFLASH_DURATION) {
		flash_duration.redeye_off_duration = isp->flash_parms->redeye_off_duration;
		flash_duration.redeye_on_duration = isp->flash_parms->redeye_on_duration;
		flash_duration.snapshot_pre_duration = isp->flash_parms->snapshot_pre_duration;
		flash_duration.aecgc_stable_duration = isp->flash_parms->aecgc_stable_duration;
		flash_duration.torch_off_duration = isp->flash_parms->torch_off_duration;
		flash_duration.flash_on_ramp_time = isp->flash_parms->flash_on_ramp_time;
		if (copy_to_user((void __user *)(ctrl->value), &(flash_duration),
				sizeof(struct v4l2_flash_duration))) {
			printk(KERN_ERR "get flash_duration failed.\n");
			return -EACCES;
		}
		return 0;
	} else if (ctrl->id == V4L2_CID_FRAME_RATE) {
		if (ctrl->value < FRAME_RATE_AUTO || ctrl->value >= FRAME_RATE_MAX) {
			CAMERA_PRINT("wrong framerate %d, we don't support!\n", ctrl->value);
			ret = -EINVAL;
		} else
			csd->framerate = ctrl->value;
	} else if (ctrl->id == V4L2_CID_SET_VIV_MODE) {
		if (ctrl->value == 0)
			camdev->if_pip = 0;
		else
			camdev->if_pip = 1;
		CAMERA_PRINT("camdev->if_pip = %d\n", camdev->if_pip);
	} else if ((csd->yuv && ctrl->id != V4L2_CID_ZOOM_RELATIVE && ctrl->id != V4L2_CID_READ_REGISTER)
		|| ctrl->id == V4L2_CID_HFLIP
		|| ctrl->id == V4L2_CID_VFLIP) {
		ret = v4l2_subdev_call(csd->sd, core, s_ctrl, ctrl);
	} else if (ctrl->id == V4L2_CID_QUERY_CAMERA_INDEX) {
		if(copy_from_user(&query_index, (const void __user *)ctrl->value, sizeof(query_index)))
			printk(KERN_ERR "get camera_index from user error.\n");
		else {
			ret = comip_camera_query_camera_index(camdev, &query_index);
			if (copy_to_user((void __user *)(ctrl->value), &query_index, sizeof(query_index)))
			printk(KERN_ERR "send camera_index to user error.\n");

		}
	} else if (ctrl->id == V4L2_CID_CACHE_FLUSH) {
		if (copy_from_user(&vm_area, (const void __user *)ctrl->value, sizeof(vm_area))){
			printk(KERN_ERR "get vm_area parameters error.\n");
			return -EACCES;
		}
		dmac_map_area(vm_area.vaddr, vm_area.length, DMA_TO_DEVICE);
	} else if (ctrl->id == V4L2_CID_CACHE_INVALIDATE) {
		if (copy_from_user(&vm_area, (const void __user *)ctrl->value, sizeof(vm_area))){
			printk(KERN_ERR "get vm_area parameters error.\n");
			return -EACCES;
		}
		dmac_map_area(vm_area.vaddr, vm_area.length, DMA_FROM_DEVICE);
	} else if ( ctrl->id == V4L2_CID_SET_SENSOR_FLIP|| ctrl->id == V4L2_CID_SET_SENSOR_MIRROR) {
		ret = v4l2_subdev_call(camdev->csd[ctrl->value].sd, core, s_ctrl, ctrl);
	} else {
		isp_dev_call(camdev->isp, lock);
		ret = isp_dev_call(camdev->isp, s_ctrl, ctrl);
		isp_dev_call(camdev->isp, unlock);
	}
	if (ctrl->id == V4L2_CID_NO_DATA_DEBUG) {
		comip_camera_esd_reset(camdev);
	}

	return ret;
}

static int comip_vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;
	if (csd->yuv)
		ret = v4l2_subdev_call(csd->sd, core, g_ctrl, ctrl);
	else {
		isp_dev_call(camdev->isp, lock);
		ret = isp_dev_call(camdev->isp, g_ctrl, ctrl);
		isp_dev_call(camdev->isp, unlock);
	}
	return ret;
}

static int comip_vidioc_cropcap(struct file *file, void *priv,
			      struct v4l2_cropcap *a)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];

	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= csd->max_width;
	a->bounds.height		= csd->max_height;
	a->defrect.left			= 0;
	a->defrect.top			= 0;
	a->defrect.width		= csd->max_video_width;
	a->defrect.height		= csd->max_video_height;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	CAMERA_PRINT("Cropcap: capture max size(%dx%d), video max size(%dx%d)\n",
			 csd->max_width,
			 csd->max_height,
			 a->defrect.width,
			 a->defrect.height);

	return 0;
}

static int comip_vidioc_g_crop(struct file *file, void *priv,
			     struct v4l2_crop *a)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];

	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= csd->max_width;
	a->c.height	= csd->max_height;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int comip_vidioc_s_crop(struct file *file, void *priv,
			     const struct v4l2_crop *a)
{
	return 0;
}

static int comip_vidioc_s_parm(struct file *file, void *priv,
			struct v4l2_streamparm *parm)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;

	if (csd->yuv)
		ret = v4l2_subdev_call(csd->sd, video, s_parm, parm);
	else
		ret = isp_dev_call(camdev->isp, s_parm, parm);

	return ret;

}

static int comip_vidioc_g_parm(struct file *file, void *priv,
			struct v4l2_streamparm *parm)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;

	if (csd->yuv)
		ret = v4l2_subdev_call(csd->sd, video, g_parm, parm);
	else
		ret = isp_dev_call(camdev->isp, g_parm, parm);

	return ret;
}

static int comip_v4l2_open(struct file *file)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct isp_device * isp = camdev->isp;

	CAMERA_PRINT("Open camera. refcnt %d\n", camdev->refcnt);

	if (++camdev->refcnt == 1) {
		camdev->input = -1;
	}
	isp->raw_file = NULL;
	isp->vraw_addr = NULL;

	return 0;
}

static int comip_v4l2_close(struct file *file)
{
	struct comip_camera_dev *camdev = video_drvdata(file);

	CAMERA_PRINT("Close camera. refcnt %d\n", camdev->refcnt);

	if (--camdev->refcnt == 0) {
		comip_camera_stop_streaming(camdev);
		vb2_queue_release(&camdev->vbq);
		comip_subdev_power_off(camdev,
			camdev->csd[camdev->input].client, camdev->input);
		isp_dev_call(camdev->isp, release, NULL);
		comip_camera_memory_free(camdev);
		camdev->input = -1;
	}

	return 0;
}

static unsigned int comip_v4l2_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	return vb2_poll(&camdev->vbq, file, wait);
}

static int comip_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	return vb2_mmap(&camdev->vbq, vma);
}

static ssize_t comip_v4l2_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct comip_camera_dev *camdev = video_drvdata(file);
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_frame *frame = &camdev->frame;
	unsigned long p = *ppos;
	void *vaddr = NULL;
	u8 *buffer, *dst;
	u8 __iomem *src;
	int c, cnt = 0, err = 0;
	unsigned long total_size;

	vaddr = vb2_plane_vaddr(&capture->last->vb, 0);

	if (!vaddr)
		return -ENODEV;

	if (!comip_camera_active(camdev))
		return -EPERM;

	total_size = (frame->width * frame->height * frame->fmt->depth) >> 3;

	if (p >= total_size)
		return 0;

	if (count >= total_size)
		count = total_size;

	if (count + p > total_size)
		count = total_size - p;

	buffer = kmalloc((count > PAGE_SIZE) ? PAGE_SIZE : count,
			 GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	src = (u8 __iomem *) (vaddr + p);

	while (count) {
		c  = (count > PAGE_SIZE) ? PAGE_SIZE : count;
		dst = buffer;
		memcpy(dst, src, c);
		dst += c;
		src += c;

		if (copy_to_user(buf, buffer, c)) {
			err = -EFAULT;
			break;
		}
		*ppos += c;
		buf += c;
		cnt += c;
		count -= c;
	}

	kfree(buffer);

	return (err) ? err : cnt;
}

static const struct v4l2_ioctl_ops comip_v4l2_ioctl_ops = {
	.vidioc_querycap            = comip_vidioc_querycap,
	.vidioc_enum_fmt_vid_cap    = comip_vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap       = comip_vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap     = comip_vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap       = comip_vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs             = comip_vidioc_reqbufs,
	.vidioc_querybuf            = comip_vidioc_querybuf,
	.vidioc_qbuf                = comip_vidioc_qbuf,
	.vidioc_dqbuf               = comip_vidioc_dqbuf,
	.vidioc_enum_input          = comip_vidioc_enum_input,
	.vidioc_g_input             = comip_vidioc_g_input,
	.vidioc_s_input             = comip_vidioc_s_input,
	.vidioc_g_ctrl	            = comip_vidioc_g_ctrl,
	.vidioc_s_ctrl              = comip_vidioc_s_ctrl,
	.vidioc_cropcap             = comip_vidioc_cropcap,
	.vidioc_g_crop              = comip_vidioc_g_crop,
	.vidioc_s_crop              = comip_vidioc_s_crop,
	.vidioc_s_parm              = comip_vidioc_s_parm,
	.vidioc_g_parm              = comip_vidioc_g_parm,
	.vidioc_streamon            = comip_vidioc_streamon,
	.vidioc_streamoff           = comip_vidioc_streamoff,
};

static struct v4l2_file_operations comip_v4l2_fops = {
	.owner 		= THIS_MODULE,
	.open 		= comip_v4l2_open,
	.release 	= comip_v4l2_close,
	.poll		= comip_v4l2_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap 		= comip_v4l2_mmap,
	.read		= comip_v4l2_read,
};

static struct video_device comip_camera = {
	.name = "comip-camera",
	.minor = -1,
	.release = video_device_release,
	.fops = &comip_v4l2_fops,
	.ioctl_ops = &comip_v4l2_ioctl_ops,
};

static int __init comip_camera_probe(struct platform_device *pdev)
{
	struct comip_camera_dev *camdev;
	struct comip_camera_platform_data *pdata;
	struct video_device *vfd;
	struct isp_device *isp;
	struct vb2_queue *q;
	struct resource *res;
	int irq;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		CAMERA_PRINT("Platform data not set\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || !irq) {
		CAMERA_PRINT("Not enough platform resources");
		return -ENODEV;
	}

	res = request_mem_region(res->start,
		res->end - res->start + 1, dev_name(&pdev->dev));
	if (!res) {
		CAMERA_PRINT("Not enough memory for resources\n");
		return -EBUSY;
	}

	camdev = kzalloc(sizeof(*camdev), GFP_KERNEL);
	if (!camdev) {
		CAMERA_PRINT("Failed to allocate camera device\n");
		ret = -ENOMEM;
		goto exit;
	}

	cam_dev = camdev;

	init_waitqueue_head(&camdev->capture.stream_off_wait);

	isp = kzalloc(sizeof(*isp), GFP_KERNEL);
	if (!isp) {
		CAMERA_PRINT("Failed to allocate isp device\n");
		ret = -ENOMEM;
		goto free_camera_dev;;
	}

	isp->irq = irq;
	isp->res = res;
	isp->dev = &pdev->dev;
	isp->pdata = pdata;
	isp->irq_notify = comip_camera_irq_notify;
	isp->data = camdev;

	ret = isp_device_init(isp);
	if (ret) {
		CAMERA_PRINT("Unable to init isp device.n");
		goto free_isp_dev;
	}

	snprintf(camdev->v4l2_dev.name, sizeof(camdev->v4l2_dev.name),
				"%s", dev_name(&pdev->dev));
	ret = v4l2_device_register(NULL, &camdev->v4l2_dev);
	if (ret) {
		CAMERA_PRINT("Failed to register v4l2 device\n");
		ret = -ENOMEM;
		goto release_isp_dev;
	}

	spin_lock_init(&camdev->slock);

	camdev->isp = isp;
	camdev->dev = &pdev->dev;
	camdev->pdata = pdata;
	camdev->input = -1;
	camdev->refcnt = 0;
	camdev->frame.fmt = &formats[0];
	camdev->frame.width = 0;
	camdev->frame.height = 0;
	camdev->frame.field = V4L2_FIELD_INTERLACED;
	camdev->save_yuv = 0;
	camdev->save_raw = 0;
	camdev->load_raw = 0;
	camdev->load_preview_raw = 0;
	camdev->alloc_ctx = NULL;
	camdev->if_pip = 0;

	if (sizeof(camdev->frame.vmfmt.reserved)
			< sizeof(*camdev->frame.ifmt.fmt_data)) {
		CAMERA_PRINT("V4l2 format info struct is too large\n");
		ret = -EINVAL;
		goto unreg_v4l2_dev;
	}

	ret = comip_camera_init_subdev(camdev);
	if (ret) {
		CAMERA_PRINT("Failed to register v4l2 device\n");
		ret = -ENOMEM;
		goto unreg_v4l2_dev;
	}

#if defined (CONFIG_VIDEO_COMIP_TORCH_LED)
	ret = led_classdev_register(&pdev->dev, &flashlight);
	if(ret)
		CAMERA_PRINT("Failed to register led classdev\n");
#endif

	platform_set_drvdata(pdev, camdev);

	/* Initialize queue. */
	q = &camdev->vbq;
	memset(q, 0, sizeof(camdev->vbq));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = camdev;
	q->buf_struct_size = sizeof(struct comip_camera_buffer);
	q->ops = &comip_vb2_qops;
	q->mem_ops = &comip_vb2_memops;
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	ret = vb2_queue_init(q);
	if (ret) {
		CAMERA_PRINT("failed to init vb2 queue\n");
		ret = -ENOMEM;
		goto free_i2c;
	}

	mutex_init(&camdev->mlock);

	vfd = video_device_alloc();
	if (!vfd) {
		CAMERA_PRINT("Failed to allocate video device\n");
		ret = -ENOMEM;
		goto free_i2c;
	}

	memcpy(vfd, &comip_camera, sizeof(comip_camera));
	vfd->lock = &camdev->mlock;
	vfd->v4l2_dev = &camdev->v4l2_dev;
	camdev->vfd = vfd;

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		CAMERA_PRINT("Failed to register video device\n");
		goto free_video_device;
	}

	ret = comip_camera_create_attr_files(&pdev->dev);
	if (ret) {
		CAMERA_PRINT("failed to create attr files\n");
		goto free_video_device;
	}

	ret = comip_camera_create_debug_attr_files(&pdev->dev);
	if (ret) {
		CAMERA_PRINT("failed to create debug attr files\n");
		goto remove_attr_files;
	}

	video_set_drvdata(vfd, camdev);

	ret = ion_iommu_attach_dev(&pdev->dev);
	if(ret) {
		CAMERA_PRINT("camera smmu attach failed\n");
		goto remove_debug_attr_files;
	}

	//initialize aecgc workqueue
	camdev->aecgc_workqueue = create_singlethread_workqueue("aecgc");
	if (camdev->aecgc_workqueue == NULL)
		goto remove_debug_attr_files;


	ret = comip_camera_init_aecgc_controls(camdev);
	if (ret) {
		CAMERA_PRINT("failed to init aecgc controls\n");
		goto destroy_wq;
	}
	ret = comip_sensor_attr_files();
	if (ret) {
		CAMERA_PRINT("create sensor attr files failed\n");
		goto destroy_wq;
	}

	return 0;

destroy_wq:
	destroy_workqueue(camdev->aecgc_workqueue);
	camdev->aecgc_workqueue = NULL;
remove_debug_attr_files:
	comip_camera_remove_debug_attr_files(&pdev->dev);
remove_attr_files:
	comip_camera_remove_attr_files(&pdev->dev);
free_video_device:
	video_device_release(vfd);
free_i2c:
	comip_camera_free_subdev(camdev);
unreg_v4l2_dev:
	v4l2_device_unregister(&camdev->v4l2_dev);
release_isp_dev:
	isp_device_release(camdev->isp);
free_isp_dev:
	kfree(camdev->isp);
free_camera_dev:
	kfree(camdev);
exit:
	return ret;

}

static int __exit comip_camera_remove(struct platform_device *pdev)
{
	struct comip_camera_dev *camdev = platform_get_drvdata(pdev);

	if (camdev->aecgc_workqueue) {
		destroy_workqueue(camdev->aecgc_workqueue);
		camdev->aecgc_workqueue = NULL;
	}
	comip_camera_remove_attr_files(&pdev->dev);
	ion_iommu_detach_dev(&pdev->dev);
	video_device_release(camdev->vfd);
	v4l2_device_unregister(&camdev->v4l2_dev);
	platform_set_drvdata(pdev, NULL);
	comip_camera_free_subdev(camdev);
	isp_device_release(camdev->isp);
	kfree(camdev->isp);
	kfree(camdev);

	return 0;
}

#ifdef CONFIG_PM
static int comip_camera_suspend(struct device *dev)
{
	struct comip_camera_dev *camdev = dev_get_drvdata(dev);

	isp_dev_call(camdev->isp, suspend, NULL);

	return 0;
}

static int comip_camera_resume(struct device *dev)
{
	struct comip_camera_dev *camdev = dev_get_drvdata(dev);

	isp_dev_call(camdev->isp, resume, NULL);

	return 0;
}

#if defined (CONFIG_VIDEO_COMIP_TORCH_LED)
static void comip_torch_onoff_set(struct led_classdev *led_cdev, enum led_brightness torch_onoff)
{
	int value = torch_onoff;

	if (cam_dev->csd[0].client->flash){
		if (value){
		//	pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_ENABLE);
			cam_dev->csd[0].client->flash(TORCH, TORCH_ON);
			led_cdev->brightness = TORCH_ON;
		}else{
			if(!cam_dev->capture.running){
		//		pmic_voltage_set(PMIC_POWER_CAMERA_IO, 0, PMIC_POWER_VOLTAGE_DISABLE);
			}
			cam_dev->csd[0].client->flash(TORCH, TORCH_OFF);
			led_cdev->brightness = TORCH_OFF;
		}
	}
}

static struct led_classdev flashlight = {
	.name = "flashlight",
	.brightness = 0,
	.brightness_set = comip_torch_onoff_set,
};
#endif

static struct dev_pm_ops comip_camera_pm_ops = {
	.suspend = comip_camera_suspend,
	.resume = comip_camera_resume,
};
#endif

static struct platform_driver comip_camera_driver = {
	.remove = __exit_p(comip_camera_remove),
	.driver = {
		.name = "comip-camera",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &comip_camera_pm_ops,
#endif
	},
};

static int __init comip_camera_init(void)
{
	return platform_driver_probe(&comip_camera_driver, comip_camera_probe);
}

static void __exit comip_camera_exit(void)
{
	platform_driver_unregister(&comip_camera_driver);
}

module_init(comip_camera_init);
module_exit(comip_camera_exit);

MODULE_DESCRIPTION("COMIP camera driver");
MODULE_LICENSE("GPL");

