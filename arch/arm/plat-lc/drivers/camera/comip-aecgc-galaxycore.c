#include "comip-aecgc-ctrl.h"
#include "comip-video.h"
#include "comip-isp-dev.h"


#define GC_VTS_SUB_MAXEXP	(0)

enum {
	AECGC_NO_BUSY = 0,
	AECGC_BUSY,
};

struct aecgc_galaxycore {
	struct comip_camera_dev *camdev;
	wait_queue_head_t busy_wait;
	struct work_struct sna_aecgc_work;
	struct work_struct aec_work;
	struct work_struct agc_work;
	struct work_struct pre_aecgc_work;
	atomic_t sna_aecgc_writeback_done;
	atomic_t in_frames_after_sna_aecgc_writeback;
	atomic_t state;
};

static struct aecgc_galaxycore aecgc_data;

static void sna_aecgc_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;
	unsigned int tmp = 0;

	if (!camdev->snapshot)
		return;

	ctrl.id = V4L2_CID_SNA_GAIN;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	parm.gain = ctrl.value;
	ctrl.id = V4L2_CID_SNA_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	parm.exp = ctrl.value;
	parm.vts = COMIP_CAMERA_AECGC_INVALID_VALUE;
	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	atomic_set(&aecgc_data.sna_aecgc_writeback_done, 1);
	AECGC_PRINT("snapshot exp=%04x gain=%04x\n", parm.exp, parm.gain);
}

static void pre_aecgc_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &(camdev->capture);
	int ret = 0;
	unsigned int tmp = 0;

	atomic_set(&aecgc_data.state, AECGC_BUSY);

	ctrl.id = V4L2_CID_PRE_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret) {
		atomic_set(&aecgc_data.state, AECGC_NO_BUSY);
		wake_up_interruptible(&aecgc_data.busy_wait);
		return;
	}
	parm.exp = ctrl.value;
	ctrl.id = V4L2_CID_PRE_GAIN;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret) {
		atomic_set(&aecgc_data.state, AECGC_NO_BUSY);
		wake_up_interruptible(&aecgc_data.busy_wait);
		return;
	}
	parm.gain = ctrl.value;

	parm.vts = COMIP_CAMERA_AECGC_INVALID_VALUE;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	if (!capture->running || camdev->snapshot) {
		atomic_set(&aecgc_data.state, AECGC_NO_BUSY);
		wake_up_interruptible(&aecgc_data.busy_wait);
		return;
	}

	AECGC_PRINT("preview exp=0x%x gain=0x%x\n", parm.exp, parm.gain);

	v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	capture->preview_aecgc_parm.exp = parm.exp;
	capture->preview_aecgc_parm.gain = parm.gain;

	atomic_set(&aecgc_data.state, AECGC_NO_BUSY);
	wake_up_interruptible(&aecgc_data.busy_wait);
}

/*
static void aec_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &(camdev->capture);
	int ret = 0;
	unsigned int tmp = 0;

	ctrl.id = V4L2_CID_PRE_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
	        return;
	parm.exp = ctrl.value;
	parm.gain = COMIP_CAMERA_AECGC_INVALID_VALUE;
	parm.vts = COMIP_CAMERA_AECGC_INVALID_VALUE;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	if (!capture->running || camdev->snapshot)
	             return;

	if (capture->preview_aecgc_parm.exp == parm.exp)
		parm.exp = COMIP_CAMERA_AECGC_INVALID_VALUE;
	else
		capture->preview_aecgc_parm.exp = parm.exp;

	if (parm.exp == COMIP_CAMERA_AECGC_INVALID_VALUE) {
	        return;
	}

	AECGC_PRINT("preview exp=0x%x\n",parm.exp);

	v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
}

static void agc_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &(camdev->capture);
	int ret = 0;
	unsigned int tmp = 0;

	ctrl.id = V4L2_CID_PRE_GAIN;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
	        return;
	parm.gain = ctrl.value;
	parm.exp = COMIP_CAMERA_AECGC_INVALID_VALUE;
	parm.vts = COMIP_CAMERA_AECGC_INVALID_VALUE;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	if (!capture->running || camdev->snapshot)
	             return;

	if (capture->preview_aecgc_parm.gain== parm.gain)
		parm.gain = COMIP_CAMERA_AECGC_INVALID_VALUE;
	else
		capture->preview_aecgc_parm.gain= parm.gain;

	if (parm.gain== COMIP_CAMERA_AECGC_INVALID_VALUE) {
	        return;
	}

	AECGC_PRINT("preview gain=0x%x\n",parm.gain);

	v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
}
*/

static int aecgc_init(void *data)
{
	aecgc_data.camdev = (struct comip_camera_dev*)data;

	INIT_WORK(&aecgc_data.sna_aecgc_work, sna_aecgc_work_handler);
	//INIT_WORK(&aecgc_data.aec_work, aec_work_handler);
	//INIT_WORK(&aecgc_data.agc_work, agc_work_handler);
	INIT_WORK(&aecgc_data.pre_aecgc_work, pre_aecgc_work_handler);
	init_waitqueue_head(&aecgc_data.busy_wait);
	atomic_set(&aecgc_data.state, AECGC_NO_BUSY);

	return 0;
}

static int aecgc_irq_notify(unsigned int status)
{
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct comip_camera_capture *capture = &camdev->capture;
#if 0
	if (((status & ISP_NOTIFY_AEC_DONE)) &&
		!camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.aec_work);
	}

	if (((status & ISP_NOTIFY_AGC_DONE)) &&
		!camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.agc_work);
	}
#else
	if (((status & ISP_NOTIFY_AEC_DONE) || (status & ISP_NOTIFY_AGC_DONE)) &&
		!camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.pre_aecgc_work);
	}
#endif
	if (status & ISP_NOTIFY_SOF) {
		if (camdev->snapshot) {
			if (atomic_read(&aecgc_data.sna_aecgc_writeback_done)) {
				if (atomic_inc_return(&aecgc_data.in_frames_after_sna_aecgc_writeback) >= 2)
					atomic_set(&capture->sna_enable, 1);
			}
		}
	}

	return 0;
}

static int aecgc_start_streaming(void)
{
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct comip_camera_capture *capture = &camdev->capture;

	if (camdev->snapshot) {
		atomic_set(&capture->sna_enable, 0);
	}

	atomic_set(&aecgc_data.sna_aecgc_writeback_done, 0);
	atomic_set(&aecgc_data.in_frames_after_sna_aecgc_writeback, 0);

	return 0;
}


static int aecgc_start_capture(void)
{
	struct comip_camera_dev *camdev = aecgc_data.camdev;

	if (camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.sna_aecgc_work);
	}

	return 0;
}

static int aecgc_get_adjust_frm_duration(int sensor_vendor, int main_type)
{
	return 0;
}

static int aecgc_get_vts_sub_maxexp(int sensor_vendor, int main_type)
{
	return GC_VTS_SUB_MAXEXP;
}
static int aecgc_bracket_long_exposure(void)
{
	return 0;
}

static int aecgc_bracket_short_exposure(void)
{
	return 0;
}

static int aecgc_wait_finish(void)
{
	int v;
	wait_event_interruptible(aecgc_data.busy_wait,
			(v = atomic_read(&aecgc_data.state) == AECGC_NO_BUSY));

	return 0;
}

struct comip_camera_aecgc_control galaxycore_aecgc_control = {
	.vendor = SENSOR_VENDOR_GALAXYCORE,
	.init = aecgc_init,
	.isp_irq_notify = aecgc_irq_notify,
	.start_capture = aecgc_start_capture,
	.start_streaming = aecgc_start_streaming,
	.get_adjust_frm_duration = aecgc_get_adjust_frm_duration,
	.get_vts_sub_maxexp = aecgc_get_vts_sub_maxexp,
	.bracket_short_exposure = aecgc_bracket_short_exposure,
	.bracket_long_exposure = aecgc_bracket_long_exposure,
	.wait_finish = aecgc_wait_finish,
};

