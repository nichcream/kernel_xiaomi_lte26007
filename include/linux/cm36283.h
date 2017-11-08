/*
Copyright(C), 2013, leadcore Inc.
Description: Definitions for cm36283 light-prox chip.
History:
<author>  <time>       <version>   <desc>
hudie     2013/8/20      1.0       build this module
*/

#ifndef _CM36283_H
#define _CM36283_H

struct cm36283_platform_data {
	u16	prox_threshold_hi;
	u16 	prox_threshold_lo;
};
#endif
