/*
 * Low-level device IO routines for ST-Ericsson CW1200 drivers
 *
 * Copyright (c) 2010, ST-Ericsson
 * Author: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 *
 * Based on:
 * ST-Ericsson UMAC CW1200 driver, which is
 * Copyright (c) 2010, ST-Ericsson
 * Author: Ajitpal Singh <ajitpal.singh@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>

#include "cw1200_common.h"
#include "hwio.h"
#include "sbus.h"
#include <linux/mmc/sdio_func.h>
#include "debug.h"

 /* Sdio addr is 4*spi_addr */
#define SPI_REG_ADDR_TO_SDIO(spi_reg_addr) ((spi_reg_addr) << 2)

static int cw1200_sdio_memcpy_toio(struct CW1200_priv *priv,
                   unsigned int addr,
                   const void *src, int count)
{
    return sdio_memcpy_toio(priv->func, addr, (void *)src, count);
}

static int cw1200_sdio_memcpy_fromio(struct CW1200_priv *priv,
                     unsigned int addr,
                     void *dst, int count)
{
    return sdio_memcpy_fromio(priv->func, dst, addr, count);
}

static int __cw1200_reg_read(struct CW1200_priv *priv, u16 addr,
                void *buf, size_t buf_len, int buf_id)
{
    u16 addr_sdio;
    u32 sdio_reg_addr_17bit ;

    /* Check if buffer is aligned to 4 byte boundary */
    if (WARN_ON(((unsigned long)buf & 3) && (buf_len > 4))) {
        return -EINVAL;
    }

    /* Convert to SDIO Register Address */
    addr_sdio = SPI_REG_ADDR_TO_SDIO(addr);
    sdio_reg_addr_17bit = SDIO_ADDR17BIT(buf_id, 0, 0, addr_sdio);
    return cw1200_sdio_memcpy_fromio(priv,sdio_reg_addr_17bit,
                          buf, buf_len);
}

static int __cw1200_reg_write(struct CW1200_priv *priv, u16 addr,
                const void *buf, size_t buf_len, int buf_id)
{
    u16 addr_sdio;
    u32 sdio_reg_addr_17bit ;
    /* Convert to SDIO Register Address */
    addr_sdio = SPI_REG_ADDR_TO_SDIO(addr);
    sdio_reg_addr_17bit = SDIO_ADDR17BIT(buf_id, 0, 0, addr_sdio);
    return cw1200_sdio_memcpy_toio(priv,sdio_reg_addr_17bit,
                        buf, buf_len);
}

static inline int __cw1200_reg_read_32(struct CW1200_priv *priv,
                    u16 addr, u32 *val)
{
    return __cw1200_reg_read(priv, addr, val, sizeof(val), 0);
}

static inline int __cw1200_reg_write_32(struct CW1200_priv *priv,
                    u16 addr, u32 val)
{
    return __cw1200_reg_write(priv, addr, &val, sizeof(val), 0);
}

int cw1200_reg_read(struct CW1200_priv *priv, u16 addr, void *buf,
            size_t buf_len)
{
    int ret;
    sdio_claim_host(priv->func);
    ret = __cw1200_reg_read(priv, addr, buf, buf_len, 0);
    sdio_release_host(priv->func);
    return ret;
}

int cw1200_reg_write(struct CW1200_priv *priv, u16 addr, const void *buf,
            size_t buf_len)
{
    int ret;
    sdio_claim_host(priv->func);
    ret = __cw1200_reg_write(priv, addr, buf, buf_len, 0);
    sdio_release_host(priv->func);
    return ret;
}

int cw1200_indirect_read(struct CW1200_priv *priv, u32 addr, void *buf,
             size_t buf_len, u32 prefetch, u16 port_addr)
{
    u32 val32 = 0;
    int i, ret;

    if ((buf_len / 2) >= 0x1000) {
        DEBUG(DBG_ERROR,
                "%s: Can't read more than 0xfff words.\n",
                __func__);
        WARN_ON(1);
        return -EINVAL;
        goto out;
    }

    DEBUG(DBG_SBUS, "%s [%d]: Before claim host \n", __func__, __LINE__);
    sdio_claim_host(priv->func);

    /* Write address */
    ret = __cw1200_reg_write_32(priv, ST90TDS_SRAM_BASE_ADDR_REG_ID, addr);
    if (ret < 0) {
        DEBUG(DBG_ERROR,
                "%s: Can't write address register.\n",
                __func__);
        goto out;
    }

    /* Read CONFIG Register Value - We will read 32 bits */
    ret = __cw1200_reg_read_32(priv, ST90TDS_CONFIG_REG_ID, &val32);
    if (ret < 0) {
        DEBUG(DBG_ERROR,
                "%s: Can't read config register.\n",
                __func__);
        goto out;
    }

    /* Set PREFETCH bit */
    ret = __cw1200_reg_write_32(priv, ST90TDS_CONFIG_REG_ID,
                    val32 | prefetch);
    if (ret < 0) {
        DEBUG(DBG_ERROR,
                "%s: Can't write prefetch bit.\n",
                __func__);
        goto out;
    }

    /* Check for PRE-FETCH bit to be cleared */
    for (i = 0; i < 20; i++) {
        ret = __cw1200_reg_read_32(priv, ST90TDS_CONFIG_REG_ID, &val32);
        if (ret < 0) {
            DEBUG(DBG_ERROR,
                    "%s: Can't check prefetch bit.\n",
                    __func__);
            goto out;
        }
        if (!(val32 & prefetch))
            break;

        mdelay(i);
    }

    if (val32 & prefetch) {
        DEBUG(DBG_ERROR,
                "%s: Prefetch bit is not cleared.\n",
                __func__);
        goto out;
    }

    /* Read data port */
    ret = __cw1200_reg_read(priv, port_addr, buf, buf_len, 0);
    if (ret < 0) {
        DEBUG(DBG_ERROR,
                "%s: Can't read data port.\n",
                __func__);
        goto out;
    }

out:
    DEBUG(DBG_SBUS, "%s [%d]: After release host \n", __func__, __LINE__);
    sdio_release_host(priv->func);
    return ret;
}

