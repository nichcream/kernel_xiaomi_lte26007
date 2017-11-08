#ifndef __COMIPFB_SPI_H__
#define __COMIPFB_SPI_H__

#include "comipfb.h"

extern int comipfb_spi_init(struct comipfb_info *fbi);
extern int comipfb_spi_exit(void);
extern int comipfb_spi_write_cmd(u8 cmd);
extern int comipfb_spi_write_cmd2(u16 cmd);
extern int comipfb_spi_write_para(u8 para);
extern int comipfb_spi_suspend(void);
extern int comipfb_spi_resume(void);

#endif /*__COMIPFB_SPI_H__*/
