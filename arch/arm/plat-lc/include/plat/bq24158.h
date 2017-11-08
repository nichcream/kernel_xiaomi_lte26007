/*
 * Copyright (C) 2010 Texas Instruments
 * Author: Balaji T K
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
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BQ24158_CHARGER_H
#define _BQ24158_CHARGER_H

/* not a bq generated event,we use this to reset the
 * the timer from the twl driver.
 */
#define BQ2415x_RESET_TIMER      0x38

#define BQ2415x_STOP_CHARGING       (10 << 0)
#define BQ2415x_START_AC_CHARGING   (10 << 1)
#define BQ2415x_START_USB_CHARGING  (10 << 2)
#define BQ2415x_START_CHARGING      (10 << 3)
#define BQ2415x_NOT_CHARGING        (10 << 4)
#define BQ2415x_CHARGING_FULL       (10 << 5)
#define BQ2415x_OTG_MODE            (10 << 6)

#define BQ2415x_CHARGE_READY         0x00
#define BQ2415x_CHARGE_CHARGING      0x10
#define BQ2415x_CHARGE_DONE          0x20
#define BQ2415x_CHARGE_FAULT         0x30
#define BQ2415x_VBUS_OVP             0x01
#define BQ2415x_SLEEP_MODE           0x02
#define BQ2415x_BAD_ADAPTOR          0x03
#define BQ2415x_BAT_OVP              0x04
#define BQ2415x_THERMAL_SHUTDOWN     0x05
#define BQ2415x_TIMER_FAULT          0x06
#define BQ2415x_NO_BATTERY           0x07

#define BQ2415x_FAULT_VBUS_OVP          0x31
#define BQ2415x_FAULT_SLEEP             0x32
#define BQ2415x_FAULT_BAD_ADAPTOR       0x33
#define BQ2415x_FAULT_BAT_OVP           0x34
#define BQ2415x_FAULT_THERMAL_SHUTDOWN  0x35
#define BQ2415x_FAULT_TIMER             0x36
#define BQ2415x_FAULT_NO_BATTERY        0x37



/* BQ24153 / BQ24156 / BQ24158 */
/* Status/Control Register */
#define REG_STATUS_CONTROL      0x00
#define TIMER_RST           (1 << 7)
#define ENABLE_STAT_PIN     (1 << 6)
#define BOOST_MODE_SHIFT      3
#define BOOST_MODE           (1)
#define CHARGE_MODE          (0)

/* Control Register */
#define REG_CONTROL_REGISTER      0x01
#define INPUT_CURRENT_LIMIT_SHIFT   6
#define ENABLE_ITERM_SHIFT          3
#define ENABLE_CHARGER_SHIFT        2
#define ENABLE_CHARGER            (0)
#define DISABLE_CHARGER           (1)

/* Control/Battery Voltage Register */
#define REG_BATTERY_VOLTAGE      0x02
#define VOLTAGE_SHIFT            2
#define OTG_PL_EN                (1)
#define OTG_PL_DIS               (0)
#define OTG_EN                   (1)
#define OTG_DIS                  (0)

/* Vender/Part/Revision Register */
#define REG_PART_REVISION       0x03

/* Battery Termination/Fast Charge Current Register */
#define REG_BATTERY_CURRENT      0x04
#define BQ24156_CURRENT_SHIFT      3
#define BQ24153_CURRENT_SHIFT      4

/* Special Charger Voltage/Enable Pin Status Register */
#define REG_SPECIAL_CHARGER_VOLTAGE    0x05
#define LOW_CHG_SHIFT        5
#define LOW_CHG_EN           (1)
#define LOW_CHG_DIS          (0)

/* Safety Limit Register */
#define REG_SAFETY_LIMIT      0x06
#define MAX_CURRENT_SHIFT      4

/* chip-specific feature flags, for driver_data.features */

#define BQ24153 (1 << 3)

#define BQ24156 (1 << 6)
#define BQ24158 (1 << 8)
#define FAN5400 (1 << 10)
#define FAN5401 (1 << 11)
#define FAN5402 (1 << 12)
#define FAN5403 (1 << 13)
#define FAN5404 (1 << 14)
#define FAN5405 (1 << 15)
#define FAN54015 (1 << 16)
#define HL7005 (1 << 17)


#define BQ2415x_WATCHDOG_TIMEOUT      (10000)

struct bq24158_platform_data {
    int max_charger_currentmA;  /*charger max charge current*/
    int max_charger_voltagemV;  /*battery limit charge voltage*/
    int dpm_voltagemV;          /*vbus min voltage*/
    int termination_currentmA;  /*charge done term current*/
    int usb_cin_limit_currentmA;/*vbus PC charger input current*/
    int usb_battery_currentmA;  /*battery usb charge current*/
    int ac_cin_limit_currentmA; /*vbus ac charger input current*/
    int ac_battery_currentmA;   /*battery ac charge current*/
};

#endif /* __BQ2415x_CHARGER_H */
