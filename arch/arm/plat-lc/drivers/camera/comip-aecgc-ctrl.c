#include "comip-aecgc-ctrl.h"
#include "comip-video.h"

extern struct comip_camera_aecgc_control samsung_aecgc_control;
extern struct comip_camera_aecgc_control galaxycore_aecgc_control;
extern struct comip_camera_aecgc_control byd_aecgc_control;

static struct comip_camera_aecgc_control* aecgc_controls[] = {
	&galaxycore_aecgc_control,
	&byd_aecgc_control,
};

struct comip_camera_aecgc_control* comip_camera_get_aecgc_control(int vendor)
{
	struct comip_camera_aecgc_control *aecgc_control = NULL;
	int loop = 0;

	for (loop = 0; loop < ARRAY_SIZE(aecgc_controls); loop++) {
		if (vendor == aecgc_controls[loop]->vendor) {
			aecgc_control = aecgc_controls[loop];
			break;
		}
	}

	return aecgc_control;
}

int comip_camera_init_aecgc_controls(void *data)
{
	int ret = 0;
	int loop = 0;

	for (loop = 0; loop < ARRAY_SIZE(aecgc_controls); loop++) {
		if (aecgc_controls[loop]->init) {
			ret = aecgc_controls[loop]->init(data);
			if (ret)
				break;
		}
	}

	return ret;
}

