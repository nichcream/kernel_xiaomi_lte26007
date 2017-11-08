#ifndef COMIP_AECGC_CTRL_H
#define COMIP_AECGC_CTRL_H

#define AECGC_DEBUG
#ifdef AECGC_DEBUG
#define AECGC_PRINT(fmt, args...) printk(KERN_DEBUG "[AECGC]" fmt, ##args)
#else
#define AECGC_PRINT(fmt, args...)
#endif

struct comip_camera_aecgc_control {
	int vendor;
	int (*init)(void *data);
	int	(*isp_irq_notify)(unsigned int status);
	int (*start_capture)(void);
	int (*start_streaming)(void);
	int (*get_adjust_frm_duration)(int sensor_vendor, int main_type);
	int (*get_vts_sub_maxexp)(int sensor_vendor, int main_type);
	int (*bracket_short_exposure)(void);
	int (*bracket_long_exposure)(void);
	int (*wait_finish)(void);
};

int comip_camera_init_aecgc_controls(void *data);
struct comip_camera_aecgc_control* comip_camera_get_aecgc_control(int vendor);

#endif
