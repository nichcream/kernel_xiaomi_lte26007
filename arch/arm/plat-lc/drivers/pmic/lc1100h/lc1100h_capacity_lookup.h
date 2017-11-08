/*
 * Copyright (C) 
 * Author: 
 *curve for 4350mv battery.2150mAh and 4200mv battery 1500mAh.
 *
 */

#ifndef _BATTERY_VOLT_CAPCITY_CHART_H
#define _BATTERY_VOLT_CAPCITY_CHART_H
#include <plat/comip-battery.h>


/*200mA*/
struct batt_capacity_chart dischg_volt_cap_table_4200_normal[] = {
        { .volt = 3450, .cap = 0},
        { .volt = 3641, .cap = 4},
        { .volt = 3681, .cap = 8},
        { .volt = 3701, .cap = 12},
        { .volt = 3710, .cap = 16},
        { .volt = 3718, .cap = 20},
        { .volt = 3723, .cap = 24},
        { .volt = 3728, .cap = 28},
        { .volt = 3732, .cap = 32},
        { .volt = 3738, .cap = 36},
        { .volt = 3744, .cap = 40},
        { .volt = 3756, .cap = 44},
        { .volt = 3770, .cap = 48},
        { .volt = 3786, .cap = 52},
        { .volt = 3806, .cap = 56},
        { .volt = 3831, .cap = 60},
        { .volt = 3856, .cap = 64},
        { .volt = 3882, .cap = 68},
        { .volt = 3910, .cap = 72},
        { .volt = 3937, .cap = 76},
        { .volt = 3967, .cap = 80},
        { .volt = 3999, .cap = 84},
        { .volt = 4036, .cap = 88},
        { .volt = 4069, .cap = 92},
        { .volt = 4108, .cap = 96},
        { .volt = 4121, .cap = 100},
};

/*200mA*/
struct batt_capacity_chart dischg_volt_cap_table_4350_normal[] = {
        { .volt = 3450, .cap = 0},
        { .volt = 3592, .cap = 4},
        { .volt = 3648, .cap = 8},
        { .volt = 3660, .cap = 12},
        { .volt = 3683, .cap = 16},
        { .volt = 3711, .cap = 20},
        { .volt = 3729, .cap = 24},
        { .volt = 3741, .cap = 28},
        { .volt = 3750, .cap = 32},
        { .volt = 3761, .cap = 36},
        { .volt = 3774, .cap = 40},
        { .volt = 3788, .cap = 44},
        { .volt = 3807, .cap = 48},
        { .volt = 3827, .cap = 52},
        { .volt = 3851, .cap = 56},
        { .volt = 3884, .cap = 60},
        { .volt = 3920, .cap = 64},
        { .volt = 3957, .cap = 68},
        { .volt = 3992, .cap = 72},
        { .volt = 4032, .cap = 76},
        { .volt = 4073, .cap = 80},
        { .volt = 4115, .cap = 84},
        { .volt = 4160, .cap = 88},
        { .volt = 4205, .cap = 92},
        { .volt = 4221, .cap = 96},
        { .volt = 4252, .cap = 100},
};


#endif
