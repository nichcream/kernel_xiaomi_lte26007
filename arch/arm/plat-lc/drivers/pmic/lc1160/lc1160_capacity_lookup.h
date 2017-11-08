/*
 * Copyright (C) 
 * Author: 
 *curve for 4350mv battery.2150mAh and 4200mv battery 1500mAh.
 *
 */

#ifndef _BATTERY_VOLT_CAPCITY_CHART_H
#define _BATTERY_VOLT_CAPCITY_CHART_H

#include <plat/comip-battery.h>

/*400mA*/
struct batt_capacity_chart dischg_volt_cap_table_4200_normal[] = {
        { .volt = 3450, .cap = 0},
        { .volt = 3603, .cap = 4},
        { .volt = 3632, .cap = 8},
        { .volt = 3642, .cap = 12},
        { .volt = 3674, .cap = 16},
        { .volt = 3792, .cap = 20},
        { .volt = 3707, .cap = 24},
        { .volt = 3716, .cap = 28},
        { .volt = 3724, .cap = 32},
        { .volt = 3731, .cap = 36},
        { .volt = 3742, .cap = 40},
        { .volt = 3751, .cap = 44},
        { .volt = 3764, .cap = 48},
        { .volt = 3789, .cap = 52},
        { .volt = 3810, .cap = 56},
        { .volt = 3821, .cap = 60},
        { .volt = 3845, .cap = 64},
        { .volt = 3870, .cap = 68},
        { .volt = 3895, .cap = 72},
        { .volt = 3913, .cap = 76},
        { .volt = 3943, .cap = 80},
        { .volt = 3994, .cap = 84},
        { .volt = 4031, .cap = 88},
        { .volt = 4062, .cap = 92},
        { .volt = 4107, .cap = 96},
        { .volt = 4125, .cap = 100},
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
