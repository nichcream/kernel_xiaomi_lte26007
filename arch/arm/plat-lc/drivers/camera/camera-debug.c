
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
#include "isp-ctrl.h"
#include "comip-isp-dev.h"
#include "comip-video.h"
#include "comip-videobuf.h"
#include "camera-debug.h"
#include <plat/comip_devinfo.h>
#include <media/v4l2-subdev.h>

#define STATE_LINE_BEGIN 0
#define STATE_LINE_IGNORE 1
#define STATE_LEFT 2
#define STATE_ARRAY_0_0 3
#define STATE_ARRAY_0_X 4
#define STATE_ARRAY_0_DATA0 5
#define STATE_ARRAY_0_DATA1 6
#define STATE_ARRAY_0_DATA2 7
#define STATE_ARRAY_0_DATA3 8
#define STATE_ARRAY_0_DATA4 9
#define STATE_ARRAY_0_END 10
#define STATE_ARRAY_1_0 11
#define STATE_ARRAY_1_X 12
#define STATE_ARRAY_1_DATA0 13
#define STATE_ARRAY_1_DATA1 14
#define STATE_LINE_END 15

extern struct comip_camera_dev *cam_dev;

#ifdef COMIP_DEBUGTOOL_ENABLE

static u8 convert_ascii2num(u8 v)
{
	if (v >= '0'&& v <='9')
		return v - '0';
	else if (v >= 'a' && v <='f')
		return v - 'a' + 0xa;
	else if (v >= 'A' && v <='F')
		return v - 'A' + 0xa;
	else
		return 0xff;
}

int comip_debugtool_load_regs_blacklist(struct isp_device* isp, char *filename)
{
	unsigned int i;
	char v = 0;
	struct file *file1 = NULL;
	mm_segment_t old_fs;
	off_t file_len = 0;
	int state = STATE_LINE_BEGIN;
	unsigned int reg = 0;
	unsigned char value = 0;
	int ret = 0;

	file1 = filp_open(filename, O_RDONLY, 0);

	if (IS_ERR(file1)) {
		return 0;
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file_len = file1->f_dentry->d_inode->i_size;
		for (i = 0 ; i < file_len ; i++) {
			file1->f_op->read(file1, (char *)&v, 1, &file1->f_pos);
			if (' ' == v || '\t' == v)
				continue;
			if ('\n' == v) {
				state = STATE_LINE_BEGIN;
				continue;
			}

			if (STATE_LINE_BEGIN == state) {
				if ('{' == v)
					state = STATE_LEFT;
				if ('/' == v)
					state = STATE_LINE_IGNORE;
			} else if (STATE_LINE_IGNORE == state) {
				if ('\n' == v)
					state = STATE_LINE_BEGIN;
			} else if (STATE_LEFT == state) {
				if ('0' == v)
					state = STATE_ARRAY_0_0;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_0 == state) {
				if ('x' == v || 'X' == v)
					state = STATE_ARRAY_0_X;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_X == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  reg = ((u32)v)<<16;
					state = STATE_ARRAY_0_DATA0;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA0 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  reg |= ((u32)v)<<12;
					state = STATE_ARRAY_0_DATA1;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA1 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  reg |= ((u32)v)<<8;
					state = STATE_ARRAY_0_DATA2;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA2 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
					reg |= v<<4;
					state = STATE_ARRAY_0_DATA3;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA3 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
					reg |= v<<0;
					state = STATE_ARRAY_0_DATA4;
					if (isp_dev_call(isp, blacklist_add_reg, reg))
						printk(KERN_DEBUG"[ISP] regs blacklist is full, discard reg 0x%08x\n", reg);
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA4 == state) {
				if (',' == v)
					state = STATE_ARRAY_0_END;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_END == state) {
				if ('0' == v)
					state = STATE_ARRAY_1_0;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_1_0 == state) {
				if ('x' == v || 'X' == v)
					state = STATE_ARRAY_1_X;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_1_X == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  value = ((u32)v)<<4;
					state = STATE_ARRAY_1_DATA0;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_1_DATA0 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
					value |= v<<0;
					//isp_reg_writeb(isp, value, reg);
					//printk("{0x%4x, 0x%2x}, \n", reg, value);
				}
				state = STATE_LINE_IGNORE;
			}
		}
		set_fs(old_fs);
		filp_close(file1, NULL);
	}

	return ret;
}

int comip_debugtool_load_isp_setting(struct isp_device* isp, char* filename)
{
	unsigned int i;
	char v = 0;
	struct file *file1 = NULL;
	mm_segment_t old_fs;
	off_t file_len = 0;
	int state = STATE_LINE_BEGIN;
	unsigned int reg = 0;
	unsigned char value = 0;

	if(NULL == filename)
		return -1;

	file1 = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(file1)) {
		printk("no isp setting file use default \n");
		return -1;
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file_len = file1->f_dentry->d_inode->i_size;
		printk("load isp setting from file: %s \n", filename);
		for (i = 0 ; i < file_len ; i++) {
			file1->f_op->read(file1, (char *)&v, 1, &file1->f_pos);
			if (' ' == v || '\t' == v)
				continue;
			if ('\n' == v) {
				state = STATE_LINE_BEGIN;
				continue;
			}

			if (STATE_LINE_BEGIN == state) {
				if ('{' == v)
					state = STATE_LEFT;
				if ('/' == v)
					state = STATE_LINE_IGNORE;
			} else if (STATE_LINE_IGNORE == state) {
				if ('\n' == v)
					state = STATE_LINE_BEGIN;
			} else if (STATE_LEFT == state) {
				if ('0' == v)
					state = STATE_ARRAY_0_0;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_0 == state) {
				if ('x' == v || 'X' == v)
					state = STATE_ARRAY_0_X;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_X == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  reg = ((u32)v)<<16;
					state = STATE_ARRAY_0_DATA0;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA0 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  reg |= ((u32)v)<<12;
					state = STATE_ARRAY_0_DATA1;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA1 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  reg |= ((u32)v)<<8;
					state = STATE_ARRAY_0_DATA2;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA2 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
					reg |= v<<4;
					state = STATE_ARRAY_0_DATA3;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA3 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
					reg |= v<<0;
					state = STATE_ARRAY_0_DATA4;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_DATA4 == state) {
				if (',' == v)
					state = STATE_ARRAY_0_END;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_0_END == state) {
				if ('0' == v)
					state = STATE_ARRAY_1_0;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_1_0 == state) {
				if ('x' == v || 'X' == v)
					state = STATE_ARRAY_1_X;
				else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_1_X == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
				  value = ((u32)v)<<4;
					state = STATE_ARRAY_1_DATA0;
				} else
					state = STATE_LINE_IGNORE;
			} else if (STATE_ARRAY_1_DATA0 == state) {
				v = convert_ascii2num(v);
				if (v >= 0 && v <= 0xf) {
					value |= v<<0;
					isp_reg_writeb(isp, value, reg);
					printk("{0x%4x, 0x%2x}, \n", reg, value);
				}
				state = STATE_LINE_IGNORE;
			}
		}
		set_fs(old_fs);
		filp_close(file1, NULL);
	}

	return 1;
}

/*
int comip_debugtool_save_file(char* filename, u8* buf, u32 len, int flag)
{
	struct file* file1;
	mm_segment_t old_fs;

	if(!(flag & O_APPEND))
		flag = 0;
	else
		flag = O_APPEND;

	file1 = filp_open(filename, O_CREAT | O_WRONLY | flag, 0644);
	if (IS_ERR(file1)) {
		printk("%s: open file: %s error!\n", __FUNCTION__, filename);
		return -1;
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file1->f_op->write(file1, buf, len, &file1->f_pos);
		set_fs(old_fs);
		filp_close(file1, NULL);
	}

	return 0;
}
*/

off_t comip_debugtool_filesize(char* filename)
{
	struct file *file1 = NULL;

	if(NULL == filename)
		return 0;

	file1 = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(file1)) {
		printk("%s: open file: %s error!\n", __FUNCTION__, filename);
		return 0;
	} else {
		return file1->f_dentry->d_inode->i_size;
	}
}

int comip_debugtool_read_file(char* filename, u8* buf, off_t len)
{
	struct file *file1 = NULL;
	mm_segment_t old_fs;
	off_t file_len = 0;
	int real_len = 0;

	if(NULL == filename)
		return 0;

	file1 = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(file1)) {
		printk("%s: open file: %s error!\n", __FUNCTION__, filename);
		return 0;
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file_len = file1->f_dentry->d_inode->i_size;
		if(file_len < len)
			len = file_len;
		real_len = file1->f_op->read(file1, (char *)buf, len, &file1->f_pos);
		set_fs(old_fs);
		filp_close(file1, NULL);
	}

	return real_len;
}

int comip_debugtool_load_firmware(char* filename, u8* dest, const u8* array_src, int array_len)
{
	int len;
	u8* buf;
	int real_len = 0;
	len = comip_debugtool_filesize(filename);

	if(0 == len){
		printk("%s: enable comip debug tool, but %s file size is 0, use array instead \n", __func__, filename);
		memcpy(dest, array_src, array_len);
	}else{
		buf = kmalloc(len, GFP_KERNEL);
		real_len = comip_debugtool_read_file(filename, buf, len);
		if((real_len != array_len) || memcmp(buf, array_src, real_len)){
			printk("%s: isp_firmware array is different with %s \n", __func__, filename);
		}
		memcpy(dest, buf, real_len);
		kfree(buf);
		printk("firmware %s file size:%d, loaded:%d \n", filename, len, real_len);
	}

	return real_len;
}

int comip_debugtool_preview_load_raw_file(struct comip_camera_dev* camdev, char* filename)
{
	struct isp_device *isp = camdev->isp;
	struct isp_parm *iparm = &isp->parm;
	int size = iparm->in_width * iparm->in_height * 2;
	char val;
	off_t file_len = 0;
	mm_segment_t old_fs;
	unsigned char gain[2], exp[2], *g_info;
	unsigned int lexp;
	unsigned short sgain;

	//if (isp->vraw_addr == NULL) {
	//	printk("isp->vraw_addr **in_width = %d, in_height**lyly*****\n"iparm->in_width,iparm->in_height);
	//	isp->vraw_addr = dma_alloc_writecombine(isp->dev, size, (dma_addr_t *)&(isp->praw_addr), GFP_KERNEL);
	//}
	if (isp->raw_file == NULL) {
                printk("****isp->raw_open ***\n");
		isp->raw_file = filp_open(filename, O_RDWR, 0644);
		if (IS_ERR(isp->raw_file)) {
			printk(" %s: open file: %s error!\n", __FUNCTION__, filename);
		} else {
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			file_len = isp->raw_file->f_dentry->d_inode->i_size;
			//isp->raw_file->f_op->read(isp->raw_file, (char *)isp->vraw_addr, file_len, &file->f_pos);
			isp->raw_file->f_op->read(isp->raw_file, (char *)camdev->preview_vaddr, file_len, &isp->raw_file->f_pos);
			set_fs(old_fs);
		}
		printk("isp->raw write reg *********\n");
		g_info = (unsigned char *)(camdev->preview_vaddr + size);
                printk("**read val*00**\n");
		gain[0] = *(g_info + 4);
		gain[1] = *(g_info + 5);
		exp[0] = *(g_info + 6);
		exp[1] = *(g_info + 7);
                printk("**read val*0* gain[0] = %x gain[1] = %x exp[0] = %x, exp[1] = %x*\n", gain[0],gain[1],exp[0],exp[1]);
		sgain = (unsigned short) (gain[1] << 8 | gain[0]);
		lexp = (unsigned int) (exp[1] << 8 | exp[0]);
		lexp *= 16;
                printk("**read val***\n");
		isp_reg_writeb(isp,0x01, 0x1c174);

		isp_reg_writel(isp,lexp, 0x1c168);
		isp_reg_writew(isp,sgain, 0x1c170);


		isp_reg_writeb(isp,exp[1], 0x1e934);
		isp_reg_writeb(isp,exp[0], 0x1e935);
		isp_reg_writeb(isp,exp[1], 0x1e936);
		isp_reg_writeb(isp,exp[0], 0x1e937);

		isp_reg_writeb(isp,gain[1], 0x1e938);
		isp_reg_writeb(isp,gain[0], 0x1e939);
		isp_reg_writeb(isp,gain[1], 0x1e93a);
		isp_reg_writeb(isp,gain[0], 0x1e93b);
		isp_reg_writel(isp, (unsigned int)camdev->preview_paddr, 0x63b20);
		isp_reg_writel(isp, (unsigned int)camdev->preview_paddr, 0x63b28);
		val = isp_reg_readb(isp, 0x1e900);
		val &= 0xc7;
		val |= 0x20;
		isp_reg_writeb(isp, val, 0x1e900);
	}
    return 0;
}

int comip_debugtool_load_raw_file(struct comip_camera_dev* camdev, char* filename)
{
	int len;
	int real_len = 0;
	void *vaddr;
	struct comip_camera_capture *capture = &camdev->capture;

	if (!camdev->vaddr_valid) {
		printk("%s: vaddr is invalid\n", __FUNCTION__);
		return 0;
	}

	len = comip_debugtool_filesize(filename);

	if (camdev->offline.enable)
		vaddr = camdev->offline.vaddr;
	else
		vaddr = vb2_plane_vaddr(&capture->last->vb, 0);

	if(0 == len)
		printk("%s: enable comip debug tool, but %s file size is 0 \n", __func__, filename);
	else{
		real_len = comip_debugtool_read_file(filename, (u8 *)vaddr, len);
		printk("raw_file %s file size:%d\n", filename, len);
	}

	return real_len;
}
void comip_debugtool_save_raw_file(struct comip_camera_dev* camdev)
{
	struct file *file = NULL;
	mm_segment_t old_fs;
	char filename[100];
	struct timespec ts;
	struct rtc_time tm;
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_frame *frame = &camdev->frame;
	struct isp_device* isp = camdev->isp;
	void *vaddr;
	unsigned long size;
	u8 raw_info[24];

	if (!camdev->vaddr_valid) {
		printk("%s: vaddr is invalid\n", __FUNCTION__);
		return;
	}

	memset(filename, 0, sizeof(filename));

	if (camdev->offline.enable)
		vaddr = camdev->offline.vaddr;
	else
		vaddr = vb2_plane_vaddr(&capture->last->vb, 0);

	size = isp->parm.in_width * isp->parm.in_height * 2;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	sprintf(filename, "/data/media/DCIM/Camera/%d%02d%02d-%02d%02d%02d.raw",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
#if defined (CONFIG_VIDEO_COMIP_ISP)
	raw_info[0] = (u8)(frame->width & 0xff);
	raw_info[1] = (u8)((frame->width >> 8) & 0xff);
	raw_info[2] = (u8)(frame->height & 0xff);
	raw_info[3] = (u8)((frame->height >> 8) & 0xff);
	raw_info[4] = isp_reg_readb(isp, 0x1e95f);	//gain 0
	raw_info[5] = isp_reg_readb(isp, 0x1e95e);	//gain 1
	raw_info[6] = isp_reg_readb(isp, 0x1e95d);	//exposure 0
	raw_info[7] = isp_reg_readb(isp, 0x1e95c);	//exposure 1
	raw_info[8] = isp_reg_readb(isp, 0x1e945);	//vts 0
	raw_info[9] = isp_reg_readb(isp, 0x1e944);	//vts 1
	raw_info[10] = isp_reg_readb(isp, 0x1c735);	//awb gains section1
	raw_info[11] = isp_reg_readb(isp, 0x1c734);
	raw_info[12] = isp_reg_readb(isp, 0x1c737);
	raw_info[13] = isp_reg_readb(isp, 0x1c736);
	raw_info[14] = isp_reg_readb(isp, 0x1c739);
	raw_info[15] = isp_reg_readb(isp, 0x1c738);
	raw_info[16] = isp_reg_readb(isp, 0x65301);	//awb gains section2
	raw_info[17] = isp_reg_readb(isp, 0x65300);
	raw_info[18] = isp_reg_readb(isp, 0x65303);
	raw_info[19] = isp_reg_readb(isp, 0x65302);
	raw_info[20] = isp_reg_readb(isp, 0x65305);
	raw_info[21] = isp_reg_readb(isp, 0x65304);
	raw_info[22] = isp_reg_readb(isp, 0x65307);
	raw_info[23] = isp_reg_readb(isp, 0x65306);
#else
	raw_info[0] = (u8)(frame->width & 0xff);
	raw_info[1] = (u8)((frame->width >> 8) & 0xff);
	raw_info[2] = (u8)(frame->height & 0xff);
	raw_info[3] = (u8)((frame->height >> 8) & 0xff);
	raw_info[4] = isp_reg_readb(isp, 0x1e913);	//gain 0
	raw_info[5] = isp_reg_readb(isp, 0x1e912);	//gain 1
	raw_info[6] = isp_reg_readb(isp, 0x1e911);	//exposure 0
	raw_info[7] = isp_reg_readb(isp, 0x1e910);	//exposure 1
	raw_info[8] = isp_reg_readb(isp, 0x1e873);	//vts 0
	raw_info[9] = isp_reg_readb(isp, 0x1e872);	//vts 1
	raw_info[10] = isp_reg_readb(isp, 0x1c729);	//awb gains section1
	raw_info[11] = isp_reg_readb(isp, 0x1c728);
	raw_info[12] = isp_reg_readb(isp, 0x1c72b);
	raw_info[13] = isp_reg_readb(isp, 0x1c72a);
	raw_info[14] = isp_reg_readb(isp, 0x1c72d);
	raw_info[15] = isp_reg_readb(isp, 0x1c72c);
	raw_info[16] = isp_reg_readb(isp, 0x65301);	//awb gains section2
	raw_info[17] = isp_reg_readb(isp, 0x65300);
	raw_info[18] = isp_reg_readb(isp, 0x65303);
	raw_info[19] = isp_reg_readb(isp, 0x65302);
	raw_info[20] = isp_reg_readb(isp, 0x65305);
	raw_info[21] = isp_reg_readb(isp, 0x65304);
	raw_info[22] = isp_reg_readb(isp, 0x65307);
	raw_info[23] = isp_reg_readb(isp, 0x65306);
#endif
	file = filp_open(filename, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(file)) {
		printk("%s: open file: %s error!\n", __FUNCTION__, filename);
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file->f_op->write(file, (char *)vaddr, size, &file->f_pos);
		file->f_op->write(file, (char *)raw_info, sizeof(raw_info), &file->f_pos);
		set_fs(old_fs);
		filp_close(file, NULL);
	}
}

void comip_debugtool_save_yuv_file(struct comip_camera_dev* camdev)
{
	struct file *file = NULL;
	mm_segment_t old_fs;
	char filename[100];
	struct timespec ts;
	struct rtc_time tm;
	struct comip_camera_capture *capture = &camdev->capture;
	struct comip_camera_frame *frame = &camdev->frame;
	unsigned long size;
	void *vaddr;

	if (!camdev->vaddr_valid) {
		printk("%s: vaddr is invalid\n", __FUNCTION__);
		return;
	}

	memset(filename, 0, sizeof(filename));
	vaddr = vb2_plane_vaddr(&capture->last->vb, 0);
	size = (frame->width * frame->height * frame->fmt->depth) >> 3;

	if (camdev->snap_flag) {
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
		sprintf(filename, "/data/media/DCIM/Camera/%d%02d%02d-%02d%02d%02d.yuv",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
		file = filp_open(filename, O_CREAT | O_RDWR, 0644);
	} else {
		sprintf(filename, COMIP_DEBUGTOOL_PREVIEW_YUV_FILENAME);
		file = filp_open(filename, O_CREAT | O_WRONLY | O_APPEND, 0644);
	}

	if (IS_ERR(file)) {
		printk("%s: open file: %s error!\n", __FUNCTION__, filename);
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file->f_op->write(file, (char *)vaddr, size, &file->f_pos);
		set_fs(old_fs);
		filp_close(file, NULL);
	}
}

void comip_debugtool_save_raw_file_hdr(struct comip_camera_dev* camdev)
{
	struct file *file1 = NULL;
	struct file *file2 = NULL;
	mm_segment_t old_fs;
	char filename1[100];
	char filename2[100];
	struct timespec ts;
	struct rtc_time tm;
	struct comip_camera_frame *frame = &camdev->frame;
	struct isp_device* isp=camdev->isp;
	int width;
	int height;
	u8 raw_info[24];
	u8 raw_info1[24];
	char *odd;
	char temp;
	unsigned long i;

	if (!camdev->vaddr_valid) {
		printk("%s: vaddr is invalid\n", __FUNCTION__);
		return;
	}

	memset(filename1, 0, sizeof(filename1));
	memset(filename2, 0, sizeof(filename2));

	width = frame->width;
	height = frame->height;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	sprintf(filename1, "/data/media/DCIM/Camera/%d%02d%02d-%02d%02d%02d_1.raw",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	sprintf(filename2, "/data/media/DCIM/Camera/%d%02d%02d-%02d%02d%02d_2.raw",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);

	raw_info[0] = (u8)((width >> 8) & 0xff);
	raw_info[1] = (u8)(width & 0xff);
	raw_info[2] = (u8)((height >> 8) & 0xff);
	raw_info[3] = (u8)(height & 0xff);
	raw_info[4] = isp_reg_readb(isp, 0x1e95e);	//gain 0
	raw_info[5] = isp_reg_readb(isp, 0x1e95f);	//gain 1
	raw_info[6] = isp_reg_readb(isp, 0x1e95c);	//exposure 0
	raw_info[7] = isp_reg_readb(isp, 0x1e95d);	//exposure 1
	raw_info[8] = isp_reg_readb(isp, 0x1e944);	//vts 0
	raw_info[9] = isp_reg_readb(isp, 0x1e945);	//vts 1
	raw_info[10] = isp_reg_readb(isp, 0x1c734);	//awb gains section1
	raw_info[11] = isp_reg_readb(isp, 0x1c735);
	raw_info[12] = isp_reg_readb(isp, 0x1c736);
	raw_info[13] = isp_reg_readb(isp, 0x1c737);
	raw_info[14] = isp_reg_readb(isp, 0x1c738);
	raw_info[15] = isp_reg_readb(isp, 0x1c739);
	raw_info[16] = isp_reg_readb(isp, 0x65300);	//awb gains section2
	raw_info[17] = isp_reg_readb(isp, 0x65301);
	raw_info[18] = isp_reg_readb(isp, 0x65302);
	raw_info[19] = isp_reg_readb(isp, 0x65303);
	raw_info[20] = isp_reg_readb(isp, 0x65304);
	raw_info[21] = isp_reg_readb(isp, 0x65305);
	raw_info[22] = isp_reg_readb(isp, 0x65306);
	raw_info[23] = isp_reg_readb(isp, 0x65307);

	odd = raw_info;
	for(i=0;i<23;i=i+2) {
		temp=*(odd+i);
		*(odd+i)=*(odd+i+1);
		*(odd+i+1)=temp;
	}

	raw_info1[0] = (u8)((width >> 8) & 0xff);
	raw_info1[1] = (u8)(width & 0xff);
	raw_info1[2] = (u8)((height >> 8) & 0xff);
	raw_info1[3] = (u8)(height & 0xff);
	raw_info1[4] = isp_reg_readb(isp, 0x1e962);	//gain 0
	raw_info1[5] = isp_reg_readb(isp, 0x1e963);	//gain 1
	raw_info1[6] = isp_reg_readb(isp, 0x1e960);	//exposure 0
	raw_info1[7] = isp_reg_readb(isp, 0x1e961);	//exposure 1
	raw_info1[8] = isp_reg_readb(isp, 0x1e944);	//vts 0
	raw_info1[9] = isp_reg_readb(isp, 0x1e945);	//vts 1
	raw_info1[10] = isp_reg_readb(isp, 0x1c734);	//awb gains section1
	raw_info1[11] = isp_reg_readb(isp, 0x1c735);
	raw_info1[12] = isp_reg_readb(isp, 0x1c736);
	raw_info1[13] = isp_reg_readb(isp, 0x1c737);
	raw_info1[14] = isp_reg_readb(isp, 0x1c738);
	raw_info1[15] = isp_reg_readb(isp, 0x1c739);
	raw_info1[16] = isp_reg_readb(isp, 0x65300);	//awb gains section2
	raw_info1[17] = isp_reg_readb(isp, 0x65301);
	raw_info1[18] = isp_reg_readb(isp, 0x65302);
	raw_info1[19] = isp_reg_readb(isp, 0x65303);
	raw_info1[20] = isp_reg_readb(isp, 0x65304);
	raw_info1[21] = isp_reg_readb(isp, 0x65305);
	raw_info1[22] = isp_reg_readb(isp, 0x65306);
	raw_info1[23] = isp_reg_readb(isp, 0x65307);

	odd = raw_info1;
	for(i=0;i<23;i=i+2) {
		temp=*(odd+i);
		*(odd+i)=*(odd+i+1);
		*(odd+i+1)=temp;
	}

	file1 = filp_open(filename1, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(file1)) {
		printk("%s: open file: %s error!\n", __FUNCTION__, filename1);
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file1->f_op->write(file1, (char *)(camdev->bracket.first_frame.vaddr), width*height*2, &file1->f_pos);
		file1->f_op->write(file1, (char *)raw_info, sizeof(raw_info), &file1->f_pos);
		set_fs(old_fs);
		filp_close(file1, NULL);
	}

	file2 = filp_open(filename2, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(file2)) {
		printk("%s: open file: %s error!\n", __FUNCTION__, filename2);
	} else {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		file2->f_op->write(file2, (char *)(camdev->bracket.second_frame.vaddr), width*height*2, &file2->f_pos);
		file2->f_op->write(file2, (char *)raw_info1, sizeof(raw_info1), &file2->f_pos);
		set_fs(old_fs);
		filp_close(file2, NULL);
	}
}

static ssize_t anti_shake_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf,
			PAGE_SIZE,
			"a=%d,b=%d,c=%d,d=%d\n",
			cam_dev->isp->anti_shake_parms->a,
			cam_dev->isp->anti_shake_parms->b,
			cam_dev->isp->anti_shake_parms->c,
			cam_dev->isp->anti_shake_parms->d);
}

static ssize_t anti_shake_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "a=%d,b=%d,c=%d,d=%d",
		&cam_dev->isp->anti_shake_parms->a,
		&cam_dev->isp->anti_shake_parms->b,
		&cam_dev->isp->anti_shake_parms->c,
		&cam_dev->isp->anti_shake_parms->d);
	return count;
}

DEVICE_ATTR(anti_shake, S_IRUGO | S_IWUSR, anti_shake_show, anti_shake_store);

static ssize_t anti_shake_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cam_dev->isp->parm.anti_shake);
}

static ssize_t anti_shake_on_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	sscanf(buf, "%d", &cam_dev->isp->parm.anti_shake);
	return count;
}

DEVICE_ATTR(anti_shake_on, S_IRUGO | S_IWUSR, anti_shake_on_show, anti_shake_on_store);

static ssize_t redeye_on_duration_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
			cam_dev->isp->flash_parms->redeye_on_duration);
}

static ssize_t redeye_on_duration_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	sscanf(buf, "%d", &(cam_dev->isp->flash_parms->redeye_on_duration));
	return count;
}

DEVICE_ATTR(redeye_on_duration, S_IRUGO | S_IWUSR, redeye_on_duration_show, redeye_on_duration_store);

static ssize_t snapshot_pre_duration_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
			 cam_dev->isp->flash_parms->snapshot_pre_duration);
}

static ssize_t snapshot_pre_duration_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	sscanf(buf, "%d", &(cam_dev->isp->flash_parms->snapshot_pre_duration));
	return count;
}

DEVICE_ATTR(snapshot_pre_duration, S_IRUGO | S_IWUSR, snapshot_pre_duration_show, snapshot_pre_duration_store);
static ssize_t hdr_capture_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cam_dev->hdr);
}

static ssize_t hdr_capture_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &cam_dev->hdr);
	return count;
}

DEVICE_ATTR(hdr_capture_on, S_IRUGO | S_IWUSR, hdr_capture_show, hdr_capture_store);

static ssize_t save_yuv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cam_dev->save_yuv);
}

static ssize_t save_yuv_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &cam_dev->save_yuv);
	return count;
}

DEVICE_ATTR(save_yuv, S_IRUGO | S_IWUSR, save_yuv_show, save_yuv_store);

static ssize_t save_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cam_dev->save_raw);
}

static ssize_t save_raw_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &cam_dev->save_raw);
	return count;
}

DEVICE_ATTR(save_raw, S_IRUGO | S_IWUSR, save_raw_show, save_raw_store);


static ssize_t load_preview_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cam_dev->load_preview_raw);
}
static ssize_t load_preview_raw_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &cam_dev->load_preview_raw);
	return count;
}

DEVICE_ATTR(load_preview_raw, S_IRUGO | S_IWUSR, load_preview_raw_show, load_preview_raw_store);
static ssize_t load_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cam_dev->load_raw);
}

static ssize_t load_raw_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &cam_dev->load_raw);
	return count;
}
DEVICE_ATTR(load_raw, S_IRUGO | S_IWUSR, load_raw_show, load_raw_store);

static ssize_t flip_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return 0;
}

static ssize_t flip_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct v4l2_control ctrl;
	sscanf(buf, "%d", &ctrl.value);
	ctrl.id = V4L2_CID_VFLIP;
	v4l2_subdev_call(cam_dev->csd[0].sd, core, s_ctrl, &ctrl);
	return count;
}
DEVICE_ATTR(flip, S_IRUGO | S_IWUSR, flip_show, flip_store);

static ssize_t mirror_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return 0;
}

static ssize_t mirror_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct v4l2_control ctrl;
	sscanf(buf, "%d", &ctrl.value);
	ctrl.id = V4L2_CID_HFLIP;
	v4l2_subdev_call(cam_dev->csd[0].sd, core, s_ctrl, &ctrl);
	return count;
}
DEVICE_ATTR(mirror, S_IRUGO | S_IWUSR, mirror_show, mirror_store);

extern int comip_camera_esd_reset(struct comip_camera_dev *camdev);
static ssize_t esd_reset_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (strncmp(buf, "1", 1) == 0)
		comip_camera_esd_reset(cam_dev);

	return 1;
}
static ssize_t esd_reset_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return 0;
}

DEVICE_ATTR(esd_reset, S_IRUGO | S_IWUSR, esd_reset_show, esd_reset_store);



static struct device_attribute* dev_debug_attrs[] = {
	&dev_attr_anti_shake,
	&dev_attr_anti_shake_on,
	&dev_attr_redeye_on_duration,
	&dev_attr_snapshot_pre_duration,
	&dev_attr_hdr_capture_on,
	&dev_attr_save_yuv,
	&dev_attr_save_raw,
	&dev_attr_load_raw,
	&dev_attr_load_preview_raw,
	&dev_attr_flip,
	&dev_attr_mirror,
	&dev_attr_esd_reset,
};

int comip_camera_create_debug_attr_files(struct device *dev)
{
	int ret = 0;
	int count = ARRAY_SIZE(dev_debug_attrs);
	int loop = 0;

	for (loop = 0; loop < count; loop++) {
		ret = device_create_file(dev, dev_debug_attrs[loop]);
		if (ret)
			return ret;
	}

	return ret;
}

void comip_camera_remove_debug_attr_files(struct device *dev)
{
	int count = ARRAY_SIZE(dev_debug_attrs);
	int loop = 0;

	for (loop = 0; loop < count; loop++) {
		device_remove_file(dev, dev_debug_attrs[loop]);
	}
}

#else

int comip_debugtool_load_regs_blacklist(struct isp_device* isp, char *filename)
{
	return 0;
}

void comip_debugtool_save_raw_file(struct comip_camera_dev* camdev)
{
	return;
}

void comip_debugtool_save_yuv_file(struct comip_camera_dev* camdev)
{
	return;
}

void comip_debugtool_save_raw_file_hdr(struct comip_camera_dev* camdev)
{
	return;
}
int comip_debugtool_load_raw_file(struct comip_camera_dev* camdev, char* filename)
{
	return 0;
}

int comip_camera_create_attr_files(struct device *dev)
{
	return 0;
}

void comip_camera_remove_attr_files(struct device *dev)
{
	return;
}

#endif

static ssize_t hdr_bracket_exposure_state_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return 0;
}
static ssize_t hdr_bracket_exposure_state_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cam_dev->isp->hdr_bracket_exposure_state);
}

DEVICE_ATTR(hdr_bracket_exposure_state, S_IRUGO | S_IWUSR, hdr_bracket_exposure_state_show, hdr_bracket_exposure_state_store);

static struct device_attribute* dev_attrs[] = {
	&dev_attr_hdr_bracket_exposure_state,

};

int comip_camera_create_attr_files(struct device *dev)
{
	int ret = 0;
	int count = ARRAY_SIZE(dev_attrs);
	int loop = 0;

	for (loop = 0; loop < count; loop++) {
		ret = device_create_file(dev, dev_attrs[loop]);
		if (ret)
			return ret;
	}
	return ret;
}

void comip_camera_remove_attr_files(struct device *dev)
{
	int count = ARRAY_SIZE(dev_attrs);
	int loop = 0;

	for (loop = 0; loop < count; loop++) {
		device_remove_file(dev, dev_attrs[loop]);
	}
}

static ssize_t camera_back_sensor_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client;

	if (cam_dev->clients < 1)
		return sprintf(buf, "Camera-main:probe failed\n");

	client = v4l2_get_subdevdata(cam_dev->csd[0].sd);

	return sprintf(buf, "Camera-main:%s\n", client->name);

}
DEVICE_ATTR(camera_back_sensor, S_IRUGO | S_IWUSR, camera_back_sensor_show, NULL);

static ssize_t camera_back_sensor_id_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	char *factory_name = NULL;
	long ret = 0;

	if (cam_dev->clients < 1)
		return sprintf(buf, "Camera-main-id:probe failed\n");

	ret = v4l2_subdev_call(cam_dev->csd[0].sd, core, ioctl,
		SUBDEVIOC_MODULE_FACTORY, &factory_name);
	if (ret)
		return sprintf(buf, "Camera-main-id:not support\n");

	return sprintf(buf, "Camera-main-id:%s\n", factory_name);

}
DEVICE_ATTR(camera_back_sensor_id, S_IRUGO | S_IWUSR, camera_back_sensor_id_show, NULL);


static ssize_t camera_front_sensor_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client;

	if (cam_dev->clients < 2)
		return sprintf(buf, "Camera-sub:probe failed\n");

	client = v4l2_get_subdevdata(cam_dev->csd[1].sd);

	return sprintf(buf, "Camera-sub:%s\n", client->name);
}
DEVICE_ATTR(camera_front_sensor, S_IRUGO | S_IWUSR, camera_front_sensor_show, NULL);

static ssize_t camera_front_sensor_id_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	char *factory_name = NULL;
	long ret = 0;

	if (cam_dev->clients < 2)
		return sprintf(buf, "Camera-sub-id:probe failed\n");

	ret = v4l2_subdev_call(cam_dev->csd[1].sd, core, ioctl,
		SUBDEVIOC_MODULE_FACTORY, &factory_name);
	if (ret)
		return sprintf(buf, "Camera-sub-id:not support\n");

	return sprintf(buf, "Camera-sub-id:%s\n", factory_name);

}
DEVICE_ATTR(camera_front_sensor_id, S_IRUGO | S_IWUSR, camera_front_sensor_id_show, NULL);


static struct attribute *sensor_name_attrs[] = {
	&dev_attr_camera_back_sensor.attr,
	&dev_attr_camera_front_sensor.attr,
	&dev_attr_camera_back_sensor_id.attr,
	&dev_attr_camera_front_sensor_id.attr,
};

int comip_sensor_attr_files(void)
{
	int ret = 0;

	ret = comip_devinfo_register(sensor_name_attrs, ARRAY_SIZE(sensor_name_attrs));

	return ret;
}
