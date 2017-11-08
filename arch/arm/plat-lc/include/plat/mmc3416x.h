
#ifndef MMC3416X_H
#define MMC3416X_H

struct platform_data_mmc3416x {
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;
};

#endif

