#ifndef __COMIP_VIDEO_H__
#define __COMIP_VIDEO_H__

#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <plat/comip-v4l2.h>
#include <plat/comip-memory.h>
#include <plat/camera.h>
#include <linux/workqueue.h>
#include <linux/wait.h>

#include "comip-isp-dev.h"
#include "comip-aecgc-ctrl.h"

#define COMIP_CAMERA_VERSION		KERNEL_VERSION(1, 0, 0)
#define COMIP_CAMERA_CLIENT_NUM		(2)
#define COMIP_CAMERA_WIDTH_MAX		(4160)
#define COMIP_CAMERA_HEIGHT_MAX		(3120)

#define COMIP_CAMERA_BUFFER_MAX		(ISP_MEMORY_SIZE)
#define COMIP_CAMERA_BUFFER_MIN		(720 * 1280 * 2 * 3)
//#define COMIP_CAMERA_BUFFER_THUMBNAIL	(640 * 480 * 2)
#define COMIP_CAMERA_BUFFER_THUMBNAIL	(7990272 + 15 * 1024)

#define COMIP_CAMERA_ESD_RESET_ENABLE	(0)

enum {
	FRM_TRANS_BEFORE_SOF = 0,
	FRM_TRANS_SOF,
	FRM_TRANS_WRITE_START,
	FRM_TRANS_EOF,
	FRM_TRANS_WRITE_DONE,
	FRM_TRANS_DROP,
};


struct comip_camera_buffer {
	struct vb2_buffer vb;
	struct list_head list;
};

struct comip_camera_format {
	char *name;
	enum v4l2_mbus_pixelcode code;
	unsigned int fourcc;
	unsigned int depth;
};

struct comip_camera_frame {
	unsigned int width;
	unsigned int height;
	enum v4l2_field field;
	struct comip_camera_format *fmt;
	struct v4l2_mbus_framefmt_ext vmfmt;
	struct isp_format ifmt;
};

struct comip_camera_capture {
	struct list_head list;
	struct comip_camera_buffer *active;
	struct comip_camera_buffer *last;
	struct comip_camera_aecgc_control *aecgc_control;
	struct v4l2_aecgc_parm	preview_aecgc_parm;
	unsigned int running;
	unsigned int drop_frames;
	unsigned int lose_frames;
	unsigned int error_frames;
	unsigned int in_frames;
	unsigned int out_frames;
	unsigned int cur_vts;
	atomic_t sna_enable;
	atomic_t frm_trans_status;
	wait_queue_head_t stream_off_wait;
};

struct comip_camera_subdev {
	struct comip_camera_client *client;
	struct i2c_adapter *i2c_adap;
	struct v4l2_subdev *sd;
	struct clk *mclk;
	struct isp_prop prop;
	unsigned int max_width;
	unsigned int max_height;
	unsigned int max_video_width;
	unsigned int max_video_height;
	unsigned int max_frame_size;
	int yuv;
	int torch;
	int first;
	int framerate;
};

struct comip_camera_bracket_buffer {
	void* paddr;
	void* vaddr;
	int size;
};

struct comip_camera_bracket {
	struct comip_camera_bracket_buffer first_frame;
	struct comip_camera_bracket_buffer second_frame;
	int enable;
};

struct comip_camera_offline {
	void* paddr;
	void* vaddr;
	int size;
	int enable;
};

struct comip_camera_dev {
	struct device *dev;
	struct isp_device *isp;
	struct video_device *vfd;
	struct v4l2_device v4l2_dev;
	struct comip_camera_platform_data *pdata;
	struct comip_camera_subdev csd[COMIP_CAMERA_CLIENT_NUM];
	struct comip_camera_frame frame;
	struct comip_camera_capture capture;
	struct comip_camera_offline offline;
	struct comip_camera_bracket bracket;
	struct vb2_queue vbq;
	spinlock_t slock;
	struct mutex mlock;
	void *alloc_ctx;
	struct workqueue_struct *aecgc_workqueue;
	int clients;
	int if_pip;
	int snapshot;
	int snap_flag;
	int hdr;
	int refcnt;
	int input;
	int save_yuv;
	int save_raw;
	int load_preview_raw;
	int load_raw;
	int vaddr_valid;
	void				*preview_vaddr;
	unsigned long			preview_paddr;
	
};

#endif /*__COMIP_VIDEO_H__*/
