#ifndef _LC_ION_ISP_H_
#define _LC_ION_ISP_H_

void lc_ion_isp_init_mem(void);

int lc_ion_isp_restore_mem(unsigned int phys_addr, unsigned int size);
int lc_ion_isp_remove_mem(unsigned int phys_addr);

void* lc_ion_isp_get_buffer(unsigned int size);
int lc_ion_isp_release_buffer(void* phys_addr);
void lc_ion_isp_reset_buffer(void);
int lc_ion_isp_get_offset(void);

#endif /* _LC_ION_ISP_H_ */

