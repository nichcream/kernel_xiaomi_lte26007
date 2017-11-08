#ifndef __LC1132H_H__
#define __LC1132H_H__

#include <linux/types.h>
#include <plat/lc1132-pmic.h>

#define LC1132_FLAGS_NO_BATTERY_DEVICE		(0x00000001)
#define LC1132_FLAGS_DEVICE_CHECK		(0x00000002)

/* LC1132 platform data. */
struct lc1132_platform_data {
	int flags;
	struct lc1132_pmic_platform_data *pmic_data;
};

extern int lc1132_reg_write(u8 reg, u8 value);
extern int lc1132_reg_read(u8 reg, u8 *value);
extern int lc1132_reg_bit_write(u8 reg, u8 mask, u8 value);
extern int lc1132_reg_bit_set(u8 reg, u8 mask);
extern int lc1132_reg_bit_clr(u8 reg, u8 mask);

#endif

