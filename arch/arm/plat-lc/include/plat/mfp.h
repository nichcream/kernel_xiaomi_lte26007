#ifndef __ASM_ARCH_PLAT_MFP_H
#define __ASM_ARCH_PLAT_MFP_H

#if defined(CONFIG_CPU_LC1810)
#include <mach/mfp-lc1810.h>
#endif

#if defined(CONFIG_CPU_LC1813)
#include <mach/mfp-lc1813.h>
#endif

#if defined(CONFIG_CPU_LC1860)
#include <mach/mfp-lc1860.h>
#endif

#define mfp_to_gpio(m)		((m) % 256)
#define gpio_to_mfp(x)		(x)

#define MFP_PIN_GPIO(x)		(x)
#define MFP_CONFIG_ARRAY(x)	x, sizeof(x) / sizeof(x[0])

#if defined(CONFIG_MFP_COMIP)

enum {
	MFP_PIN_MODE_0 = 0,
	MFP_PIN_MODE_1,
	MFP_PIN_MODE_2,
	MFP_PIN_MODE_3,
	MFP_PIN_MODE_GPIO,
	MFP_PIN_MODE_MAX
};

enum {
	MFP_PULL_DISABLE = 0,
	MFP_PULL_UP,
	MFP_PULL_DOWN,
	MFP_PULL_MODE_MAX,
};

typedef unsigned int mfp_pin;
typedef unsigned int mfp_pin_mode;
typedef unsigned int mfp_pull_id;
typedef unsigned int mfp_pull_mode;

struct mfp_pin_cfg {
	mfp_pin id;
	mfp_pin_mode mode;
};

struct mfp_pull_cfg {
	mfp_pull_id id;
	mfp_pull_mode mode;
};

extern int comip_mfp_af_read(mfp_pin id);
extern int comip_mfp_pull_down_read(mfp_pin id);
extern int comip_mfp_pull_up_read(mfp_pin id);
extern int comip_mfp_config(mfp_pin id, mfp_pin_mode mode);
extern int comip_mfp_config_pull(mfp_pull_id id, mfp_pull_mode mode);
extern int comip_mfp_config_array(struct mfp_pin_cfg *pin, int size);
extern int comip_mfp_config_pull_array(struct mfp_pull_cfg *pull, int size);
extern int comip_pvdd_vol_ctrl_config(int reg, int volt);

#elif defined(CONFIG_MFP2_COMIP)

enum {
	MFP_PIN_MODE_0 = 0,
	MFP_PIN_MODE_1,
	MFP_PIN_MODE_2,
	MFP_PIN_MODE_GPIO = MFP_PIN_MODE_2,
	MFP_PIN_MODE_3,
	MFP_PIN_MODE_MAX
};

enum {
	MFP_PULL_DISABLE = 0,
	MFP_PULL_DOWN,
	MFP_PULL_UP,
	MFP_PULL_BOTH,
	MFP_PULL_MODE_MAX,
};

enum {
	MFP_DS_2MA = 1,
	MFP_DS_4MA,
	MFP_DS_6MA,
	MFP_DS_8MA,
	MFP_DS_10MA,
	MFP_DS_12MA,
	MFP_DS_MODE_MAX,
};

typedef unsigned int mfp_pin;
typedef unsigned int mfp_pin_mode;
typedef unsigned int mfp_pull_mode;
typedef unsigned int mfp_ds_mode;

struct mfp_pin_cfg {
	mfp_pin id;
	mfp_pin_mode mode;
};

struct mfp_pull_cfg {
	mfp_pin id;
	mfp_pull_mode mode;
};

struct mfp_ds_cfg {
	mfp_pin id;
	mfp_ds_mode mode;
};

extern int comip_mfp_af_read(mfp_pin id);
extern int comip_mfp_pull_down_read(mfp_pin id);
extern int comip_mfp_pull_up_read(mfp_pin id);
extern int comip_mfp_config(mfp_pin id, mfp_pin_mode mode);
extern int comip_mfp_config_pull(mfp_pin id, mfp_pull_mode mode);
extern int comip_mfp_config_ds(mfp_pin id, mfp_ds_mode mode);
extern int comip_mfp_config_array(struct mfp_pin_cfg *pin, int size);
extern int comip_mfp_config_pull_array(struct mfp_pull_cfg *pull, int size);
extern int comip_mfp_config_ds_array(struct mfp_ds_cfg *pull, int size);
extern int comip_pvdd_vol_ctrl_config(int reg, int volt);
#endif /* CONFIG_MFP2_COMIP */

#endif /* __ASM_ARCH_PLAT_MFP_H */
