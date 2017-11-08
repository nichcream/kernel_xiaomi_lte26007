#ifndef __BMA2X2_H__
#define __BMA2X2_H__

/* The sensitivity is represented in counts/g. In 2g mode the
sensitivity is 1024 counts/g. In 4g mode the sensitivity is 512
counts/g and in 8g mode the sensitivity is 256 counts/g.
 */
enum {
	MODE_2G = 0,
	MODE_4G,
	MODE_8G,
};

struct bma2x2_platform_data {
	int position;
	//int mode;
};

#endif
