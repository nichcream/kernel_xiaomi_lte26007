/*
 * Copyright (C) 
 * Author: 
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _LINUX_LC1100H_MONITOR_BATTERY_H
#define _LINUX_LC1100H_MONITOR_BATTERY_H
#include <linux/notifier.h>

/*pmic charger register*/
#define LC1100H_CHARGER_CTRL_REG0x30   (0x30)
#define LC1100H_EOC_SHIFT               5
#define LC1100H_NTC_SHIFT               4
#define LC1100H_CHGPROT_SHIFT       3
#define LC1100H_TEMPONEN_SHIFT     2
#define EOC_EN              (1)
#define EOC_DIS             (0)
#define NTC_EN               (1)
#define NTC_DIS              (0)
#define CHGPROT_EN       (1)
#define CHGPROT_DIS     (0)
#define TEMPON_EN        (1)
#define TEMPON_DIS       (0)
#define ACHGON_EN        (1)
#define ACHGON_DIS       (0)


#define LC1100H_CHARGER_VOLT_REG0x31   (0x31)
#define LC1100H_VOLT_SHIFT               4
#define LC1100H_VHYSSEL_SHIFT            3
#define CVSEL_4200    (0)
#define CVSEL_4350    (1)
#define VHYSSEL_100  (0)
#define VHYSSEL_150  (1)
#define RCHGSEL_MIN   (50)
#define RCHGSEL_MAX   (350)
#define RCHGSEL_STEP  (50)
#define RCHGSEL_VOLT  (100)
#define VHYSSEL      VHYSSEL_100

#define LC1100H_CHARGER_CURRENT_REG0x32  (0x32)
#define LC1100H_ITERM_SHIFT    4
#define TERM_CURRENT_MIN     (50)
#define TERM_CURRENT_MAX    (200)
#define TERM_CURRENT_STEP     (50)
#define CHARGE_CURRENT_MIN     (100)
#define CHARGE_CURRENT_MAX    (1200)
#define CHARGE_CURRENT_STEP     (100)

#define LC1100H_CHARGER_TIMER_REG0x33  (0x33)
#define LC1100H_CHARGE_TIMER_SHIFT    4
#define CHARGE_TIMER_MIN        (3) //hour
#define CHARGE_TIMER_4            (4)
#define CHARGE_TIMER_5            (5)
#define CHARGE_TIMER_6             (6)
#define CHARGE_TIMER_7             (7)
#define CHARGE_TIMER_MAX         (8)
#define RTIMCLRB_CLEAR            (0)
#define RTIMCLRB_RUN               (1)
#define RTIMSTP_STOP               (1)
#define RTIMSTP_START              (0)

#define LC1100H_CHARGER_STATUS_REG0x34  (0x34)
#define LC1100H_CHARGE_TIMEOUT                  (0x80)
#define LC1100H_CHGCV_STATUS                       (0x40)
#define LC1100H_CHGOC_STATUS                       (0x20)
#define LC1100H_ADPOV_STATUS                       (0x10)
#define LC1100H_BATOT_STATUS                       (0x08)
#define LC1100H_RECHG_STATUS                       (0x04)
#define LC1100H_CHGFAULT_STATUS                  (0x02)
#define LC1100H_CHARGE_DONE                         (0x01)

#define LC1100H_POWER_STATUS_REG0x35  (0x35)
#define POWER_KEY_PRESS    (1 << 5)
#define VBATV45                     (1 << 4)
#define VBATV32                     (1 << 3)
#define VBATV33                     (1 << 2)
#define VBUS_DET                   (1 << 0)



#endif
