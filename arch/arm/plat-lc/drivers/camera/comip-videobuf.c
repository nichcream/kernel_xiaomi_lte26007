
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>
#include <plat/lcmem.h>

#include "comip-video.h"
#include "comip-videobuf.h"
#include <plat/lc_ion.h>
#include <linux/vmalloc.h>

#define COMIP_CAMERA_DEBUG
#ifdef COMIP_CAMERA_DEBUG
#define CAMERA_PRINT(fmt, args...) printk(KERN_ERR "camera: " fmt, ##args)
#else
#define CAMERA_PRINT(fmt, args...) printk(KERN_DEBUG "camera: " fmt, ##args)
#endif

struct vb2_dc_conf {
	struct device			*dev;
	void				*vaddr;
	ion_phys_addr_t			paddr;
	unsigned long			size;
	unsigned long			used;
	struct ion_client *ion_camera_client;
	struct ion_handle *ion_camera_handle;
};

struct vb2_dc_buf {
	struct vb2_dc_conf		*conf;
	void				*vaddr;
	dma_addr_t			paddr;
	unsigned long			size;
	struct vm_area_struct		*vma;
	atomic_t			refcount;
	struct vb2_vmarea_handler	handler;
};

struct vb2_delayed_release_info {
	struct delayed_work	delayed_release_work;
	struct mutex lock;
	struct workqueue_struct *wq_delayed_release;
	struct vb2_dc_conf *alloc_ctx;
	unsigned long delayed_release_start_timestamp;
	unsigned long delayed_release_duration;
	int	input;
	int delayed_release_running;
	unsigned int memfree_threshold;

};

static struct vb2_delayed_release_info *delayed_release_info = NULL;

#define COMIP_VB2_LOCK()	do { \
								if (delayed_release_info) { \
									mutex_lock(&delayed_release_info->lock); \
								} \
							} while (0)

#define COMIP_VB2_UNLOCK()	do { \
								if (delayed_release_info) { \
									mutex_unlock(&delayed_release_info->lock); \
								} \
							} while (0)

#define COMIP_VB2_DELAYED_RELEASE_TIMESLICE	(10) //in sec unit

#define M(x) (((x) << (PAGE_SHIFT - 10)) >> 10)

static int comip_vb2_get_sys_freemem(void)
{
	struct sysinfo i;

	si_meminfo(&i);
	return (M(i.freeram));
}

static void __comip_vb2_cleanup_ctx(void *alloc_ctx);

static void work_delayed_release(struct work_struct *work)
{
	int memfree = 0, cleanflag = 0;
	COMIP_VB2_LOCK();
	if (!delayed_release_info->delayed_release_running) {
		COMIP_VB2_UNLOCK();
		return;
	}
	memfree = comip_vb2_get_sys_freemem();
	if (memfree <= delayed_release_info->memfree_threshold) {
		CAMERA_PRINT("work_delayed_release: system free mem is too low %d <= %d threshold, release camera mem immediately\n",
					memfree, delayed_release_info->memfree_threshold);
		cleanflag = 1;
	} else {
		if (time_is_before_eq_jiffies(delayed_release_info->delayed_release_start_timestamp + delayed_release_info->delayed_release_duration)) {
			CAMERA_PRINT("work_delayed_release: delay times up, release camera mem\n");
			cleanflag = 1;
		} else {
			queue_delayed_work(delayed_release_info->wq_delayed_release,
							&delayed_release_info->delayed_release_work,
							COMIP_VB2_DELAYED_RELEASE_TIMESLICE * HZ);
		}
	}

	if (cleanflag) {
		__comip_vb2_cleanup_ctx(delayed_release_info->alloc_ctx);
		delayed_release_info->alloc_ctx = NULL;
		delayed_release_info->input = -1;
		delayed_release_info->delayed_release_running = 0;
	}
	COMIP_VB2_UNLOCK();
}

static void comip_vb2_delayed_release_init(unsigned int delayed_release_duration, unsigned int memfree_threshold)
{
	delayed_release_info = kzalloc(sizeof(struct vb2_delayed_release_info), GFP_KERNEL);
	if (!delayed_release_info)
		return;

	mutex_init(&delayed_release_info->lock);
	delayed_release_info->wq_delayed_release = create_singlethread_workqueue("vb2_delayed_release");
	if (!delayed_release_info->wq_delayed_release)
		goto delayed_release_info_fail;

	INIT_DEFERRABLE_WORK(&delayed_release_info->delayed_release_work, work_delayed_release);

	delayed_release_info->input = -1;
	delayed_release_info->delayed_release_duration = delayed_release_duration * HZ;
	delayed_release_info->memfree_threshold = memfree_threshold;

	CAMERA_PRINT("using camera mem delayed release, delay_duration=%ds, memfree_threshold=%dMB\n",
				delayed_release_duration, memfree_threshold);

	return;
/*
wq_delayed_release_fail:
	destroy_workqueue(delayed_release_info->wq_delayed_release);
	delayed_release_info->wq_delayed_release = NULL;
*/
delayed_release_info_fail:
	kfree(delayed_release_info);
	delayed_release_info = NULL;
}

static void comip_vb2_put(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct vb2_dc_conf *conf = buf->conf;

	if (atomic_dec_and_test(&buf->refcount)) {
		conf->used = 0;
		kfree(buf);
	}
}

static void *comip_vb2_alloc(void *alloc_ctx, unsigned long size, gfp_t gfp_flags)
{
	struct vb2_dc_conf *conf = alloc_ctx;
	struct vb2_dc_buf *buf;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->vaddr = conf->vaddr + conf->used;
	buf->paddr = conf->paddr + conf->used;
	conf->used += size;

	buf->conf = conf;
	buf->size = size;
	buf->handler.refcount = &buf->refcount;
	buf->handler.put = comip_vb2_put;
	buf->handler.arg = buf;

	atomic_inc(&buf->refcount);

	return buf;
}

static void *comip_vb2_cookie(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return &buf->paddr;
}

static void *comip_vb2_vaddr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	if (!buf)
		return 0;

	return buf->vaddr;
}

static unsigned int comip_vb2_num_users(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static int comip_vb2_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct vb2_dc_conf *conf = buf->conf;
	struct dma_buf *video_dma_buf;
	unsigned long size, pgoff;
	int ret;

	if (!buf) {
		printk(KERN_ERR "No buffer to map\n");
		return -EINVAL;
	}

	video_dma_buf = ion_share_dma_buf(conf->ion_camera_client, conf->ion_camera_handle);
	pgoff = __phys_to_pfn(buf->paddr) - __phys_to_pfn(conf->paddr);
	ret = dma_buf_mmap(video_dma_buf, vma, pgoff);
	if (ret) {
		printk(KERN_ERR "dma_buf_mmap failed\n");
		return -EINVAL;
	}
	dma_buf_put(video_dma_buf);

	pr_debug("%s: mapped paddr 0x%lx at 0x%lx, size %lu\n",
		__func__, (unsigned long)conf->paddr, (unsigned long)vma->vm_start, size);

	return 0;
}

static void *comip_vb2_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write)
{
	struct vb2_dc_buf *buf;
	struct vm_area_struct *vma;
	dma_addr_t paddr = 0;
	int ret;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = vb2_get_contig_userptr(vaddr, size, &vma, &paddr);
	if (ret) {
		printk(KERN_ERR "Failed acquiring VMA for vaddr 0x%08lx\n",
				vaddr);
		kfree(buf);
		return ERR_PTR(ret);
	}

	buf->size = size;
	buf->paddr = paddr;
	buf->vma = vma;

	return buf;
}

static void comip_vb2_put_userptr(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	if (!buf)
		return;

	vb2_put_vma(buf->vma);
	kfree(buf);
}

const struct vb2_mem_ops comip_vb2_memops = {
	.alloc		= comip_vb2_alloc,
	.put		= comip_vb2_put,
	.cookie		= comip_vb2_cookie,
	.vaddr		= comip_vb2_vaddr,
	.mmap		= comip_vb2_mmap,
	.get_userptr	= comip_vb2_get_userptr,
	.put_userptr	= comip_vb2_put_userptr,
	.num_users	= comip_vb2_num_users,
};
EXPORT_SYMBOL_GPL(comip_vb2_memops);

static void *__comip_vb2_init_ctx(struct device *dev)
{
	struct vb2_dc_conf *conf;
	struct comip_camera_dev *camdev = (struct comip_camera_dev*)dev_get_drvdata(dev);
	struct comip_camera_subdev *csd = &camdev->csd[camdev->input];
	unsigned int max_frame_size = 0;
	int ret = 0;
	size_t len;
	unsigned long size_r;

	conf = kzalloc(sizeof *conf, GFP_KERNEL);
	if (!conf)
		return NULL;

	conf->dev = dev;
	conf->used = 0;

	conf->ion_camera_client = lc_ion_client_create("Camera");
	if (IS_ERR(conf->ion_camera_client))
		return  NULL;

	max_frame_size = PAGE_ALIGN(csd->max_frame_size);
	conf->size = COMIP_CAMERA_ALLOC_RAWBUF_NUM * max_frame_size;
	if (conf->size < COMIP_CAMERA_BUFFER_MIN)
		conf->size = COMIP_CAMERA_BUFFER_MIN;
	conf->size += COMIP_CAMERA_BUFFER_THUMBNAIL;
if (camdev->load_preview_raw)
{
	size_r = conf->size;
	conf->size += (csd->max_width *  csd->max_height * 2 + 32);
}
	conf->ion_camera_handle = ion_alloc(conf->ion_camera_client, conf->size, PAGE_SIZE, LC_ION_HEAP_LCMEM_MASK,
							ION_HEAP_LC_ISP_KERNEL_MASK | ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC);

	if (IS_ERR(conf->ion_camera_handle)) {
		ion_client_destroy(conf->ion_camera_client);
		return NULL;
	}

	conf->vaddr = ion_map_kernel(conf->ion_camera_client, conf->ion_camera_handle);
	if (IS_ERR(conf->vaddr)) {
		dev_err(conf->dev, "alloc mem for isp of size %ld failed\n",
			conf->size);
		//kfree(conf);
		//return NULL;
	}

	ret = ion_phys(conf->ion_camera_client, conf->ion_camera_handle, &conf->paddr, &len);
	if (ret) {
		ion_free(conf->ion_camera_client, conf->ion_camera_handle);
		ion_client_destroy(conf->ion_camera_client);
		kfree(conf);
		return NULL;
	}
if (camdev->load_preview_raw)
{
	camdev->preview_vaddr = conf->vaddr + size_r;
	camdev->preview_paddr = (unsigned long)conf->paddr + size_r;
//********************
conf->size -= (csd->max_width *  csd->max_height * 2 + 32);
}
	printk("isp memory: paddr=0x%lx, vaddr=0x%lx, size=0x%lx.\n",
		conf->paddr, (unsigned long)conf->vaddr, conf->size);

	return conf;
}

static void __comip_vb2_cleanup_ctx(void *alloc_ctx)
{
	struct vb2_dc_conf *conf = (struct vb2_dc_conf*)alloc_ctx;

	if (!IS_ERR(conf->vaddr))
		ion_unmap_kernel(conf->ion_camera_client, conf->ion_camera_handle);

	if (conf->paddr) {
		ion_free(conf->ion_camera_client, conf->ion_camera_handle);
		ion_client_destroy(conf->ion_camera_client);
	}
	kfree(alloc_ctx);
}

void *comip_vb2_init_ctx(struct device *dev)
{
	void *ret = NULL;
	struct comip_camera_dev *camdev = (struct comip_camera_dev*)dev_get_drvdata(dev);
	struct comip_camera_mem_delayed_release *mem_delayed_release = &camdev->pdata->mem_delayed_release;

	if (!delayed_release_info && mem_delayed_release->delayed_release_duration > 0)
		comip_vb2_delayed_release_init(mem_delayed_release->delayed_release_duration,
									mem_delayed_release->memfree_threshold);
	COMIP_VB2_LOCK();
	if (delayed_release_info) {
		cancel_delayed_work(&delayed_release_info->delayed_release_work);
		delayed_release_info->delayed_release_running = 0;
		if (delayed_release_info->input == camdev->input) {
			ret = (void*)delayed_release_info->alloc_ctx;
			COMIP_VB2_UNLOCK();
			return ret;
		} else if (delayed_release_info->input >= 0) {
			if (delayed_release_info->alloc_ctx) {
				__comip_vb2_cleanup_ctx(delayed_release_info->alloc_ctx);
				delayed_release_info->alloc_ctx = NULL;
			}
			delayed_release_info->input = -1;
		}
	}
	ret = __comip_vb2_init_ctx(dev);
	if (delayed_release_info) {
		delayed_release_info->alloc_ctx = (struct vb2_dc_conf*)ret;
		delayed_release_info->input = camdev->input;
	}
	COMIP_VB2_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(comip_vb2_init_ctx);

void comip_vb2_cleanup_ctx(void *alloc_ctx)
{
	unsigned int memfree = 0;
	COMIP_VB2_LOCK();
	if (delayed_release_info) {
		//make sure there is no delayed release work pending
		cancel_delayed_work(&delayed_release_info->delayed_release_work);
		delayed_release_info->delayed_release_running = 0;

		memfree = comip_vb2_get_sys_freemem();
		if (memfree <= delayed_release_info->memfree_threshold) {
			CAMERA_PRINT("system free mem is too low %d <= %d threshold, release camera mem immediately\n",
						memfree, delayed_release_info->memfree_threshold);
			__comip_vb2_cleanup_ctx(alloc_ctx);
			delayed_release_info->alloc_ctx = NULL;
			delayed_release_info->input = -1;
			COMIP_VB2_UNLOCK();
			return;
		}

		CAMERA_PRINT("system free mem is ok %d > %d threshold, start camera mem delayed release\n",
						memfree, delayed_release_info->memfree_threshold);
		delayed_release_info->delayed_release_running = 1;
		delayed_release_info->delayed_release_start_timestamp = jiffies;
		queue_delayed_work(delayed_release_info->wq_delayed_release,
						&delayed_release_info->delayed_release_work,
						COMIP_VB2_DELAYED_RELEASE_TIMESLICE * HZ);
	} else {
		__comip_vb2_cleanup_ctx(alloc_ctx);
	}
	COMIP_VB2_UNLOCK();
}
EXPORT_SYMBOL_GPL(comip_vb2_cleanup_ctx);

void* comip_vb2_ctx_vaddr(void *alloc_ctx)
{
	struct vb2_dc_conf *conf = (struct vb2_dc_conf*)alloc_ctx;
	return conf->vaddr;
}
EXPORT_SYMBOL_GPL(comip_vb2_ctx_vaddr);

ion_phys_addr_t comip_vb2_ctx_paddr(void *alloc_ctx)
{
	struct vb2_dc_conf *conf = (struct vb2_dc_conf*)alloc_ctx;
	return conf->paddr;
}
EXPORT_SYMBOL_GPL(comip_vb2_ctx_paddr);

unsigned long comip_vb2_ctx_size(void *alloc_ctx)
{
	struct vb2_dc_conf *conf = (struct vb2_dc_conf*)alloc_ctx;
	return conf->size;
}
EXPORT_SYMBOL_GPL(comip_vb2_ctx_size);

MODULE_DESCRIPTION("Comip memory handling routines for videobuf2");
MODULE_LICENSE("GPL");

