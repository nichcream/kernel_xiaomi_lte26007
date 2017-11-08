#include "comip-aecgc-ctrl.h"
#include "comip-video.h"
#include "comip-isp-dev.h"


#define GC_VTS_SUB_MAXEXP	(0)


struct aecgc_byd {
	struct comip_camera_dev *camdev;
	struct work_struct aecgc_work;
	struct work_struct sna_aecgc_work;
	struct work_struct isp_vts_work;
};

static struct aecgc_byd aecgc_data;

static void sna_aecgc_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct v4l2_aecgc_parm parm;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	int ret = 0;
	unsigned int tmp = 0;

	ctrl.id = V4L2_CID_SNA_GAIN;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	parm.gain = ctrl.value;
	ctrl.id = V4L2_CID_SNA_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	parm.exp = ctrl.value;
	parm.vts = parm.exp;
	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	AECGC_PRINT("snapshot exp=%04x gain=%04x\n", parm.exp, parm.gain);
}

static void aecgc_work_handler(struct work_struct *work)
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

	ctrl.id = V4L2_CID_PRE_EXP;
	ret = isp_dev_call(isp, g_ctrl, &ctrl);
	if (ret)
	        return;
	parm.exp = ctrl.value;
	parm.vts = capture->cur_vts;

	ctrl.id = V4L2_CID_AECGC_PARM;
	tmp = (unsigned int)(&parm);
	memcpy(&ctrl.value, &tmp, sizeof(unsigned int));

	if (!capture->running || camdev->snapshot)
	             return;
	if (capture->preview_aecgc_parm.exp == parm.exp
	        && capture->preview_aecgc_parm.gain == parm.gain
	        && capture->preview_aecgc_parm.vts == parm.vts) {
	        return;
	}

	AECGC_PRINT("preview exp=0x%x gain=0x%x vts=0x%x\n",
		parm.exp, parm.gain, parm.vts);

	ret = v4l2_subdev_call(csd->sd, core, s_ctrl, &ctrl);
	if (ret)
	        return;

	memcpy(&(capture->preview_aecgc_parm), &parm, sizeof(parm));
}

static void isp_vts_work_handler(struct work_struct *work)
{
	struct v4l2_control ctrl;
	struct comip_camera_dev *camdev = aecgc_data.camdev;
	struct isp_device *isp = camdev->isp;
	//struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	struct comip_camera_capture *capture = &(camdev->capture);
	u16 max_exp = 0, vts = 0;

	if (!capture->running || camdev->snapshot)
	             return;

	vts = capture->cur_vts;
	max_exp = vts - GC_VTS_SUB_MAXEXP;

	ctrl.id = V4L2_CID_VTS;
	ctrl.value = vts;
	isp_dev_call(isp, s_ctrl, &ctrl);

	ctrl.id = V4L2_CID_MAX_EXP;
	ctrl.value = max_exp;
	isp_dev_call(isp, s_ctrl, &ctrl);
}

static int aecgc_init(void *data)
{
	aecgc_data.camdev = (struct comip_camera_dev*)data;

	INIT_WORK(&aecgc_data.sna_aecgc_work, sna_aecgc_work_handler);
	INIT_WORK(&aecgc_data.aecgc_work, aecgc_work_handler);
	INIT_WORK(&aecgc_data.isp_vts_work, isp_vts_work_handler);

	return 0;
}

static int aecgc_irq_notify(unsigned int status)
{
	struct comip_camera_dev *camdev = aecgc_data.camdev;

	if ((status & ISP_NOTIFY_AEC_DONE) &&
		!camdev->snapshot) {
		queue_work(camdev->aecgc_workqueue, &aecgc_data.aecgc_work);
	}

	queue_work(camdev->aecgc_workqueue, &aecgc_data.isp_vts_work);

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

struct comip_camera_aecgc_control byd_aecgc_control = {
	.vendor = SENSOR_VENDOR_BYD,
	.init = aecgc_init,
	.isp_irq_notify = aecgc_irq_notify,
	.start_capture = aecgc_start_capture,
	.get_adjust_frm_duration = aecgc_get_adjust_frm_duration,
	.get_vts_sub_maxexp = aecgc_get_vts_sub_maxexp,
};

