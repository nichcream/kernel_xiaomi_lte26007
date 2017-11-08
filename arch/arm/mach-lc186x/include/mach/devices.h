#ifndef __ARCH_ARM_MACH_DEVICES_H
#define __ARCH_ARM_MACH_DEVICES_H

#include <plat/camera.h>
#include <plat/comipfb.h>
#include <plat/i2c.h>
#include <plat/keypad.h>
#include <plat/mmc.h>
#include <plat/nand.h>
#include <plat/pwm.h>
#include <plat/spi.h>
#include <plat/uart.h>
#include <plat/usb.h>
#include <plat/watchdog.h>
#include <plat/i2s.h>
#include <plat/tpz.h>
#include <plat/mets.h>


/* CAMERA. */
extern void comip_set_camera_info(struct comip_camera_platform_data *info);

/* LCD. */
extern void comip_set_fb_info(struct comipfb_platform_data *info);

/* I2C. */
extern void comip_set_i2c0_info(struct comip_i2c_platform_data *info);
extern void comip_set_i2c1_info(struct comip_i2c_platform_data *info);
extern void comip_set_i2c2_info(struct comip_i2c_platform_data *info);
extern void comip_set_i2c3_info(struct comip_i2c_platform_data *info);
extern void comip_set_i2c4_info(struct comip_i2c_platform_data *info);

/* KEYPAD. */
extern void comip_set_keypad_info(struct comip_keypad_platform_data *info);

/* MMC. */
extern void comip_set_mmc0_info(struct comip_mmc_platform_data *info);
extern void comip_set_mmc1_info(struct comip_mmc_platform_data *info);
extern void comip_set_mmc2_info(struct comip_mmc_platform_data *info);
extern void comip_set_mmc3_info(struct comip_mmc_platform_data *info);

/* NAND. */
extern void comip_set_nand_info(struct comip_nand_platform_data *info);

/* PWM. */
extern void comip_set_pwm_info(struct comip_pwm_platform_data *info);

/* SPI. */
extern void comip_set_spi0_info(struct comip_spi_platform_data *info);
extern void comip_set_spi1_info(struct comip_spi_platform_data *info);
extern void comip_set_spi2_info(struct comip_spi_platform_data *info);
extern void comip_set_spi3_info(struct comip_spi_platform_data *info);

/* UART. */
extern void comip_set_uart0_info(struct comip_uart_platform_data *info);
extern void comip_set_uart1_info(struct comip_uart_platform_data *info);
extern void comip_set_uart2_info(struct comip_uart_platform_data *info);
extern void comip_set_uart3_info(struct comip_uart_platform_data *info);

/* WDT. */
extern void comip_set_wdt_info(struct comip_wdt_platform_data *info);

/* TPZ. */
extern void comip_set_tpz_info(struct comip_tpz_platform_data *info);

/* METS. */
extern void comip_set_mets_info(struct comip_mets_platform_data *info);

/* I2S. */
extern void comip_set_i2s0_info(struct comip_i2s_platform_data *info);
extern void comip_set_i2s1_info(struct comip_i2s_platform_data *info);
extern void comip_set_pcm_info(void);
extern void comip_set_virtual_pcm_info(void);
extern void comip_set_smmu_info(void);
extern void comip_set_flowcal_info(void);
/* Platform. */
extern struct platform_device comip_device_uart0;
extern struct platform_device comip_device_uart1;
extern struct platform_device comip_device_uart2;
extern struct platform_device comip_device_uart3;

extern void comip_register_device(struct platform_device *dev, void *data);

#endif/*__ARCH_ARM_MACH_DEVICES_H */

