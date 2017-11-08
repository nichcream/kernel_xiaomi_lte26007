#include "comip-aecgc-ctrl.h"
#include "comip-video.h"
#include "comip-isp-dev.h"


#define VTS_SUB_MAXEXP	(0x08)
//#define AECGC_BANDFILTER_CONTROL


struct aecgc_samsung {
	struct comip_camera_dev *camdev;
	wait_queue_head_t aecgc_wait;
	struct work_struct aecgc_work;
	struct work_struct sna_aecgc_work;
	struct work_struct aec_work;
	struct work_struct agc_work;
	atomic_t sna_gpoff;
	atomic_t pre_gpoff;
	atomic_t in_frames_after_sna_gpoff;
	atomic_t in_frames_after_pre_gpoff;
	int bandfilter_enable;
};

static struct aecgc_samsung aecgc_data;

#ifdef AECGC_BANDFILTER_CONTROL
static int aecgc_bandfilter_control(void)
{
	struct v4l2_control ctrl;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct comip_camera_capture *capture = &(camdev->capture);
	struct isp_device *isp = camdev->isp;
	struct comip_camera_frame *frame = &camdev->frame;
	struct v4l2_fmt_data *fmt_data = frame->ifmt.fmt_data;
	unsigned int bandfilter_range = fmt_data->bandfilter_range;
	unsigned int bandfilter_thres = fmt_data->bandfilter_thres;
	int exp = capture->preview_aecgc_parm.exp;
	int gain = capture->preview_aecgc_parm.gain;
	int bandfilter_value;
	int ret = 0;

	ctrl.id = V4L2_CID_POWER_LINE_FREQUENCY;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
		return ret;
	bandfilter_value = ctrl.value;

//	if ( bandfilter_value != FLICKER_OFF ) {
		AECGC_PRINT("(gain * exp) >> 6 = 0x%0x,band_enable = %d\n",((gain * exp) >> 6),aecgc_data.bandfilter_enable);
		if(((gain * exp) >> 6) <= bandfilter_thres) {
			if(aecgc_data.bandfilter_enable) {
				aecgc_data.bandfilter_enable = 0;
				ret = isp_dev_call(isp, set_bandfilter_enbale, 0);
				if (ret)
					return ret;

				AECGC_PRINT(" SET bandfilter enable =%d\n",aecgc_data.bandfilter_enable);
			}
		} else if (((gain * exp) >> 6) >= (bandfilter_thres + bandfilter_range)) {
			if(!aecgc_data.bandfilter_enable) {
				aecgc_data.bandfilter_enable = 1;
				ret = isp_dev_call(isp, set_bandfilter_enbale, 1);
				if (ret)
					return ret;
				AECGC_PRINT(" SET bandfilter enable =%d\n",aecgc_data.bandfilter_enable);
			}
		}
//	}
	return 0;
}
#endif

static void sna_aecgc_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &camdev->capture;
	int v = 0, ret = 0;
	unsigned int tmp = 0;

	atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	ret = wait_event_interruptible(aecgc_data.aecgc_wait,
		(v = atomic_read(&capture->frm_trans_status)) >= FRM_TRANS_SOF && v < FRM_TRANS_EOF);
	if (ret)
		return;

	ctrl.id = V4L2_CID_SNA_GAIN;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	parm.gain = ctrl.value;
	ctrl.id = V4L2_CID_SNA_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	parm.exp = ctrl.value;
	parm.vts = parm.exp + VTS_SUB_MAXEXP;
	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	atomic_set(&aecgc_data.sna_gpoff, 1);
	AECGC_PRINT("snapshot exp=%04x gain=%04x vts=%04x\n", parm.exp, parm.gain, parm.vts);
}

static void aecgc_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &(camdev->capture);
	int v = 0, ret = 0;
	unsigned int tmp = 0;

	atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	ret = wait_event_interruptible(aecgc_data.aecgc_wait,
	        ((v = atomic_read(&capture->frm_trans_status) >= FRM_TRANS_SOF && v < FRM_TRANS_EOF)) || v == FRM_TRANS_DROP);
	if (ret)
		return;

	ctrl.id = V4L2_CID_PRE_GAIN;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
	        return;
	parm.gain = ctrl.value;

	ctrl.id = V4L2_CID_PRE_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
		return;

	parm.exp = ctrl.value;
	parm.vts = parm.exp + VTS_SUB_MAXEXP;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	if (!capture->running || camdev->snapshot){
		printk("[AECGC]capture->running =%d,camdev->snapshot=%d\n",capture->running,camdev->snapshot);
		return;
	}

#if 0
	if (capture->preview_aecgc_parm.exp == parm.exp
	        && capture->preview_aecgc_parm.gain == parm.gain
	        && capture->preview_aecgc_parm.vts == parm.vts) {
	        return;
	}
#endif

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	if (ret)
		return;
#ifdef AECGC_BANDFILTER_CONTROL
	aecgc_bandfilter_control();
#endif
	atomic_set(&aecgc_data.in_frames_after_pre_gpoff, 0);
	atomic_set(&aecgc_data.pre_gpoff, 1);
	memcpy(&(capture->preview_aecgc_parm), &parm, sizeof(parm));

	AECGC_PRINT("preview exp=0x%x gain=0x%x vts=0x%x\n",parm.exp, parm.gain, parm.vts);
}

static void aec_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &(camdev->capture);
	int v = 0, ret = 0;
	unsigned int tmp = 0;

	atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	ret = wait_event_interruptible(aecgc_data.aecgc_wait,
	        ((v = atomic_read(&capture->frm_trans_status) >= FRM_TRANS_SOF && v < FRM_TRANS_EOF)) || v == FRM_TRANS_DROP);
	if (ret)
		return;

	ctrl.id = V4L2_CID_PRE_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
		return;

	parm.exp = ctrl.value;
	parm.gain = COMIP_CAMERA_AECGC_INVALID_VALUE;
	parm.vts = parm.exp + VTS_SUB_MAXEXP;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	if (!capture->running || camdev->snapshot){
		printk("[AECGC]capture->running =%d,camdev->snapshot=%d\n",capture->running,camdev->snapshot);
		return;
	}

#if 0
	if (capture->preview_aecgc_parm.exp == parm.exp
	        && capture->preview_aecgc_parm.gain == parm.gain
	        && capture->preview_aecgc_parm.vts == parm.vts) {
	        return;
	}
#endif

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	if (ret)
		return;
#ifdef AECGC_BANDFILTER_CONTROL
	aecgc_bandfilter_control();
#endif
	atomic_set(&aecgc_data.in_frames_after_pre_gpoff, 0);
	atomic_set(&aecgc_data.pre_gpoff, 1);
	capture->preview_aecgc_parm.exp = parm.exp;
	capture->preview_aecgc_parm.vts = parm.vts;

	AECGC_PRINT("[exp]  exp = 0x%x, gain = 0x%x\n",parm.exp,capture->preview_aecgc_parm.gain);
}

static void agc_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &(camdev->capture);
	int v = 0, ret = 0;
	unsigned int tmp = 0;
	int tmp_exp = 0;

	atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	ret = wait_event_interruptible(aecgc_data.aecgc_wait,
	        ((v = atomic_read(&capture->frm_trans_status) >= FRM_TRANS_SOF && v < FRM_TRANS_EOF)) || v == FRM_TRANS_DROP);
	if (ret)
		return;

	ctrl.id = V4L2_CID_PRE_GAIN;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
	        return;
	parm.gain = ctrl.value;
	ctrl.id = V4L2_CID_PRE_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
		return;
	tmp_exp = ctrl.value;
	if (tmp_exp != capture->preview_aecgc_parm.exp){
		parm.exp = tmp_exp;
		parm.vts = parm.exp + VTS_SUB_MAXEXP;

		atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
		ret = wait_event_interruptible(aecgc_data.aecgc_wait,
		        ((v = atomic_read(&capture->frm_trans_status) >= FRM_TRANS_SOF && v < FRM_TRANS_EOF)) || v == FRM_TRANS_DROP);
		if (ret)
			return;
	}
	else {
		parm.exp = COMIP_CAMERA_AECGC_INVALID_VALUE;
		parm.vts = COMIP_CAMERA_AECGC_INVALID_VALUE;
	}

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	if (!capture->running || camdev->snapshot){
		printk("[AECGC]capture->running =%d,camdev->snapshot=%d\n",capture->running,camdev->snapshot);
		return;
	}

#if 0
	if (capture->preview_aecgc_parm.exp == parm.exp
	        && capture->preview_aecgc_parm.gain == parm.gain
	        && capture->preview_aecgc_parm.vts == parm.vts) {
	        return;
	}
#endif

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	if (ret)
		return;
#ifdef AECGC_BANDFILTER_CONTROL
	aecgc_bandfilter_control();
#endif
	atomic_set(&aecgc_data.in_frames_after_pre_gpoff, 0);
	atomic_set(&aecgc_data.pre_gpoff, 1);
	capture->preview_aecgc_parm.gain = parm.gain;
	if (tmp_exp != capture->preview_aecgc_parm.exp)
		capture->preview_aecgc_parm.exp = parm.exp;

	AECGC_PRINT("[gain] exp = 0x%x, gain = 0x%x\n",capture->preview_aecgc_parm.exp,parm.gain);
}

static int aecgc_init(void *data)
{
	aecgc_data.camdev = (struct comip_camera_dev*)data;
	atomic_set(&aecgc_data.sna_gpoff, 0);
	atomic_set(&aecgc_data.pre_gpoff, 0);
	atomic_set(&aecgc_data.in_frames_after_pre_gpoff, 0);
	atomic_set(&aecgc_data.in_frames_after_sna_gpoff, 0);
	init_waitqueue_head(&aecgc_data.aecgc_wait);
	INIT_WORK(&aecgc_data.sna_aecgc_work, sna_aecgc_work_handler);
	INIT_WORK(&aecgc_data.aecgc_work, aecgc_work_handler);
	INIT_WORK(&aecgc_data.aec_work, aec_work_handler);
	INIT_WORK(&aecgc_data.agc_work, agc_work_handler);
	aecgc_data.bandfilter_enable = 0;
	return 0;
}

static int aecgc_irq_notify(unsigned int status)
{
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct comip_camera_capture *capture = &camdev->capture;


	if (((status & ISP_NOTIFY_AEC_DONE)) && !camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.aec_work);
	}

	if (((status & ISP_NOTIFY_AGC_DONE)) && !camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.agc_work);
	}

	if ((status & ISP_NOTIFY_AEC_DONE && status & ISP_NOTIFY_AGC_DONE) &&
		!camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.aecgc_work);
	}

	if (status & ISP_NOTIFY_SOF) {
		wake_up_interruptible(&aecgc_data.aecgc_wait);
		if (camdev->snapshot) {
			if (atomic_read(&aecgc_data.sna_gpoff)) {
				if (atomic_inc_return(&aecgc_data.in_frames_after_sna_gpoff) >= 2)
					atomic_set(&capture->sna_enable, 1);
			}
		} else {
			if (atomic_read(&aecgc_data.pre_gpoff)) {
				if (atomic_inc_return(&aecgc_data.in_frames_after_pre_gpoff) >= 2) {
					//queue_work(camdev->aecgc_workqueue, &aecgc_data.isp_vts_work);
				}
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

	atomic_set(&aecgc_data.sna_gpoff, 0);
	atomic_set(&aecgc_data.pre_gpoff, 0);
	atomic_set(&aecgc_data.in_frames_after_pre_gpoff, 0);
	atomic_set(&aecgc_data.in_frames_after_sna_gpoff, 0);

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
	return 10;
}

static int aecgc_get_vts_sub_maxexp(int sensor_vendor, int main_type)
{
	return VTS_SUB_MAXEXP;
}

static int aecgc_bracket_short_exposure(void)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &camdev->capture;
	unsigned int tmp = 0;
	int v = 0, ret = 0;

	atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	ret = wait_event_interruptible(aecgc_data.aecgc_wait,
		 ((v = atomic_read(&capture->frm_trans_status) >= FRM_TRANS_SOF && v < FRM_TRANS_EOF)) || v == FRM_TRANS_DROP);
	if (ret)
		return ret;

	parm.exp = isp->hdr[0] & 0xff;
	parm.exp = (parm.exp << 8) |(isp->hdr[1] & 0xff);

	parm.gain = isp->hdr[2] & 0xff;
	parm.gain = (parm.gain << 8) | (isp->hdr[3] & 0xff);

	parm.vts = parm.exp + VTS_SUB_MAXEXP;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	atomic_set(&aecgc_data.sna_gpoff, 1);

	AECGC_PRINT("HDR:Short exp=%04x gain=%04x vts=%04x\n", parm.exp, parm.gain, parm.vts);

	return 0;
}


static int aecgc_bracket_long_exposure(void)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &camdev->capture;
	unsigned int tmp = 0;
	int v = 0, ret = 0;

	atomic_set(&aecgc_data.sna_gpoff, 0);
	atomic_set(&capture->frm_trans_status, FRM_TRANS_BEFORE_SOF);
	ret = wait_event_interruptible(aecgc_data.aecgc_wait,
		 ((v = atomic_read(&capture->frm_trans_status) >= FRM_TRANS_SOF && v < FRM_TRANS_EOF)) || v == FRM_TRANS_DROP);
	if (ret)
		return ret;

	parm.exp = isp->hdr[4] & 0xff;
	parm.exp = (parm.exp << 8) | (isp->hdr[5] & 0xff);

	parm.gain = isp->hdr[6] & 0xff;
	parm.gain = (parm.gain << 8) | (isp->hdr[7] & 0xff);

	parm.vts = parm.exp + VTS_SUB_MAXEXP;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	atomic_set(&aecgc_data.sna_gpoff, 1);
	AECGC_PRINT("HDR:Long exp=%04x gain=%04x vts=%04x\n", parm.exp, parm.gain, parm.vts);

	return 0;
}


struct comip_camera_aecgc_control samsung_aecgc_control = {
	.vendor = SENSOR_VENDOR_SAMSUMG,
	.init = aecgc_init,
	.isp_irq_notify = aecgc_irq_notify,
	.start_capture = aecgc_start_capture,
	.start_streaming = aecgc_start_streaming,
	.get_adjust_frm_duration = aecgc_get_adjust_frm_duration,
	.get_vts_sub_maxexp = aecgc_get_vts_sub_maxexp,
	.bracket_short_exposure = aecgc_bracket_short_exposure,
	.bracket_long_exposure = aecgc_bracket_long_exposure,
};

