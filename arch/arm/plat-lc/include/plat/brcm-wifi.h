#if !defined(__ASM_ARCH_COMIP_WIFI_BRCM_H)
#define __ASM_ARCH_COMIP_WIFI_BRCM_H

extern void *brcm_wifi_mem_prealloc(int section, unsigned long size);
extern int __init bcm_init_wifi_mem(void);

#endif /* __ASM_ARCH_COMIP_WIFI_BRCM_H */

