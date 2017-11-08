
#ifndef __ASM_ARCH_COMIP_MEMORY_H
#define __ASM_ARCH_COMIP_MEMORY_H

#define COMIP_MEMORY_BASE		(g_comip_memory_base)
#define COMIP_MEMORY_SIZE		(g_comip_memory_size)

#define KERNEL_MEMORY_BASE		(g_comip_kernel_memory_base)
#define KERNEL_MEMORY_SIZE		(g_comip_kernel_memory_size)

#define COMIP_MEMORY2_BASE		(g_comip_memory2_base)
#define COMIP_MEMORY2_SIZE		(g_comip_memory2_size)

#define KERNEL_MEMORY2_BASE		(g_comip_kernel_memory2_base)
#define KERNEL_MEMORY2_SIZE		(g_comip_kernel_memory2_size)

#define ON2_MEMORY_BASE			(g_comip_on2_memory_base)
#define ON2_MEMORY_SIZE			(g_comip_on2_memory_size)
#define ON2_MEMORY_FIX			(g_comip_on2_memory_fix)

#define FB_MEMORY_BASE			(g_comip_fb_memory_base)
#define FB_MEMORY_SIZE			(g_comip_fb_memory_size)
#define FB_MEMORY_NUM			(g_comip_fb_memory_num)
#define FB_MEMORY_FIX			(g_comip_fb_memory_fix)

/* Modem physical base address and size. */
#define MODEM_MEMORY_BASE		(0)
#define MODEM_MEMORY_SIZE		(CONFIG_PHYS_OFFSET)

extern phys_addr_t g_comip_memory_base;
extern phys_addr_t g_comip_memory_size;
extern phys_addr_t g_comip_kernel_memory_base;
extern phys_addr_t g_comip_kernel_memory_size;
extern phys_addr_t g_comip_memory2_base;
extern phys_addr_t g_comip_memory2_size;
extern phys_addr_t g_comip_kernel_memory2_base;
extern phys_addr_t g_comip_kernel_memory2_size;
extern phys_addr_t g_comip_on2_memory_base;
extern phys_addr_t g_comip_on2_memory_size;
extern unsigned char g_comip_on2_memory_fix;
extern phys_addr_t g_comip_fb_memory_base;
extern phys_addr_t g_comip_fb_memory_size;
extern unsigned char g_comip_fb_memory_num;
extern unsigned char g_comip_fb_memory_fix;

#endif /* __ASM_ARCH_COMIP_MEMORY_H */

