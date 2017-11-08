#ifndef __ASM_ARCH_PLAT_KEYPAD_H
#define __ASM_ARCH_PLAT_KEYPAD_H

#include <linux/input.h>
#include <linux/input/matrix_keypad.h>

#define KBS_MATRIX_ROWS_MAX	(8)
#define KBS_MATRIX_COLS_MAX	(8)
#define KBS_MATRIX_ROW_SHIFT	(3)
#define KBS_DIRECT_KEYS_MAX	(8)

struct comip_keypad_platform_data {
	/* data pin internal pull up. */
	int		internal_pull_up;

	/* Code map for the matrix keys. */
	unsigned int	matrix_key_rows;
	unsigned int	matrix_key_cols;
	unsigned int	*matrix_key_map;
	int		matrix_key_map_size;

	/* Direct keys. */
	int		direct_key_num;
	unsigned int	direct_key_map[KBS_DIRECT_KEYS_MAX];

	/* Key interval, unit : ms. */
	unsigned int	debounce_interval;
	unsigned int	detect_interval;
};

#endif /* __ASM_ARCH_PLAT_KEYPAD_H */

