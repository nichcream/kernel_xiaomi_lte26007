#ifndef __LC1160H_H__
#define __LC1160H_H__

#include <linux/types.h>
#include <plat/lc1160-pmic.h>

#define LC1160_FLAGS_NO_BATTERY_DEVICE		(0x00000001)
#define LC1160_FLAGS_DEVICE_CHECK		(0x00000002)

/* LC1160 platform data. */
struct lc1160_platform_data {
	int flags;
	struct lc1160_pmic_platform_data *pmic_data;
};
enum {
	LC1160_ECO0 = 0,
	LC1160_ECO1, //lc1160b
};
extern int lc1160_reg_write(u8 reg, u8 value);
extern int lc1160_reg_read(u8 reg, u8 *value);
extern int lc1160_reg_bit_write(u8 reg, u8 mask, u8 value);
extern int lc1160_reg_bit_set(u8 reg, u8 mask);
extern int lc1160_reg_bit_clr(u8 reg, u8 mask);
extern int lc1160_chip_version_get(void);

#if defined(CONFIG_COMIP_PROJECT_K706_V1_0)
#define CONFIG_OTG_GPIO_CTL    1
#endif

#endif
