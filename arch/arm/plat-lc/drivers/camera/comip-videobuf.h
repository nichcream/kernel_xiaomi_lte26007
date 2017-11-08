#ifndef __COMIP_VIDEOBUF_H__
#define __COMIP_VIDEOBUF_H__

#include <media/videobuf2-core.h>
#include <linux/dma-mapping.h>
#include "camera-debug.h"
#include <ion/ion.h>

#if defined (CONFIG_VIDEO_COMIP_ISP)
#define COMIP_CAMERA_ALLOC_RAWBUF_NUM	(4)
#else
#ifdef COMIP_DEBUGTOOL_ENABLE
#define COMIP_CAMERA_ALLOC_RAWBUF_NUM	(2)
#else
#define COMIP_CAMERA_ALLOC_RAWBUF_NUM	(1)
#endif
#endif

static inline dma_addr_t
comip_vb2_plane_paddr(struct vb2_buffer *vb, unsigned int plane_no)
{
	dma_addr_t *paddr = vb2_plane_cookie(vb, plane_no);

	return *paddr;
}

void* comip_vb2_init_ctx(struct device *dev);
void comip_vb2_cleanup_ctx(void *alloc_ctx);
void* comip_vb2_ctx_vaddr(void *alloc_ctx);
ion_phys_addr_t comip_vb2_ctx_paddr(void *alloc_ctx);
unsigned long comip_vb2_ctx_size(void *alloc_ctx);

extern const struct vb2_mem_ops comip_vb2_memops;

#endif/* __COMIP_VIDEOBUF_H__ */
