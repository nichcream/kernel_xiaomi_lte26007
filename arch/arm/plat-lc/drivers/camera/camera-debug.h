#ifndef __CAMERA_DEBUG_H__
#define __CAMERA_DEBUG_H__

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
#include <linux/rtc.h>
#include <linux/syscalls.h>
#include "comip-video.h"

/* Debug switch*/
#define COMIP_DEBUGTOOL_ENABLE

#define COMIP_DEBUGTOOL_ISPSETTING_FILENAME "/data/media/DCIM/Camera/camera_isp_setting"
#define COMIP_DEBUGTOOL_FIRMWARE_FILENAME "/data/media/DCIM/Camera/camera_isp_firmware"
#define COMIP_DEBUGTOOL_PREVIEW_YUV_FILENAME "/data/media/DCIM/Camera/preview.yuv"
#define COMIP_DEBUGTOOL_LOAD_RAW_FILENAME "/data/media/DCIM/Camera/comip_camera.raw"
#define COMIP_DEBUGTOOL_REGS_BLACKLIST_FILENAME "/data/media/DCIM/Camera/camera_isp_regs_blacklist"


#define COMIP_DEBUGTOOL_LOAD_PREVIEW_RAW_FILENAME "/data/media/DCIM/Camera/preview.raw"

int comip_debugtool_preview_load_raw_file(struct comip_camera_dev* camdev, char* filename);

int comip_debugtool_load_isp_setting(struct isp_device* isp, char* filename);
int comip_debugtool_load_regs_blacklist(struct isp_device* isp, char *filename);
void comip_debugtool_save_raw_file(struct comip_camera_dev* camdev);
//int comip_debugtool_save_file(char* filename, u8* buf, u32 len, int flag);
off_t comip_debugtool_filesize(char* filename);
int comip_debugtool_read_file(char* filename, u8* buf, off_t len);
int comip_debugtool_load_firmware(char* filename, u8* dest, const u8* array_src, int array_len);
int comip_debugtool_load_raw_file(struct comip_camera_dev* camdev, char* filename);
void comip_debugtool_save_raw_file_hdr(struct comip_camera_dev* camdev);
void comip_debugtool_save_yuv_file(struct comip_camera_dev* camdev);
int comip_camera_create_attr_files(struct device *dev);
void comip_camera_remove_attr_files(struct device *dev);
int comip_camera_create_debug_attr_files(struct device *dev);
void comip_camera_remove_debug_attr_files(struct device *dev);

int comip_sensor_attr_files(void);

#endif /* __CAMERA_DEBUG_H__ */
