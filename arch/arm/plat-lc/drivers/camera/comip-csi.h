#ifndef __COMIP_CSI_H__
#define __COMIP_CSI_H__

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <plat/hardware.h>

#define CSI_NUM			(2)

#define CSI_ERROR_STAT(id)	(id ? CTL_CPHY1_ERROR_STAT : CTL_CPHY0_ERROR_STAT)
#define CSI_STAT(id)		(id ? CTL_CPHY1_STAT : CTL_CPHY0_STAT)
#define CSI_RSTZ(id)		(id ? CTL_CPHY1_RSTZ : CTL_CPHY0_RSTZ)
#define CSI_CTRL0(id)		(id ? CTL_CPHY1_CTRL0 : CTL_CPHY0_CTRL0)
#define CSI_CTRL1(id)		(id ? CTL_CPHY1_CTRL1 : CTL_CPHY0_CTRL1)
#define CSI_MODE(id)		(id ? CTL_CPHY1_MODE : CTL_CPHY0_MODE)

static inline unsigned int csi_reg_readl(unsigned int reg)
{
	return readl(io_p2v(reg));
}

static inline void csi_reg_writel(unsigned int value, unsigned int reg)
{
	writel(value, io_p2v(reg));
}

static inline void csi_reg_write_part(unsigned int reg, unsigned int value,
			unsigned char shift, unsigned char width)
{
         unsigned int mask = (1 << width) - 1;
         unsigned int temp = csi_reg_readl(reg);

         temp &= ~(mask << shift);
         temp |= (value & mask) << shift;
         csi_reg_writel(temp, reg);
}

extern int csi_phy_init(void);
extern int csi_phy_release(void);
extern int csi_phy_start(unsigned int id, unsigned int lane_num, unsigned int freq);
extern int csi_phy_stop(unsigned int id);

#endif/*__COMIP_CSI_H__*/
