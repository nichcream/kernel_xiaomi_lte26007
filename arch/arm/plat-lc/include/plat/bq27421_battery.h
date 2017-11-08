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

#ifndef _BQ27421_BATTERY_H
#define _BQ27421_BATTERY_H

#define BQ27421_I2C_ADDR             (0x55)
#define BQ27421_ROM_MODE_I2C_ADDR    (0x0B)
#define DRIVER_VERSION               "1.0.0"

#define BQ27421_REG_CTRL    (0x00)
#define BQ27421_REG_TEMP    (0x02)
#define BQ27421_REG_VOLT    (0x04)
#define BQ27421_REG_FLAGS   (0x06)
#define BQ27421_REG_NAC     (0x08)
#define BQ27421_REG_FAC     (0x0A)
#define BQ27421_REG_RM      (0x0C)
#define BQ27421_REG_FCC     (0x0E)
#define BQ27421_REG_AI      (0x10)
#define BQ27421_REG_SI      (0x12)
#define BQ27421_REG_MLI     (0x14)

#define BQ27421_REG_AP      (0x18)
#define BQ27421_REG_SOC     (0x1C)
#define BQ27421_REG_ITEMP   (0x1E)
#define BQ27421_REG_SOH     (0x20)
#define BQ27421_REG_RMCU    (0x28)
#define BQ27421_REG_RMCF    (0x2A)
#define BQ27421_REG_FCCU    (0x2C)
#define BQ27421_REG_FCCF    (0x2E)
#define BQ27421_REG_SOCU    (0x30)

#define BQ27421_REG_DOD0       (0x66)
#define BQ27421_REG_DODEOC     (0x68)
#define BQ27421_REG_TRC        (0x6A)
#define BQ27421_REG_PASSCHG    (0x6C)
#define BQ27421_REG_QSTART     (0x6E)

/*CONTROL() subcommand*/
#define BQ27421_SUBCMD_CONTROL_STATUS  (0x0000)
#define BQ27421_SUBCMD_DEVICES_TYPE    (0x0001)
#define BQ27421_SUBCMD_FW_VERSION      (0x0002)
#define BQ27421_SUBCMD_HW_VERSION      (0x0003)
#define BQ27421_SUBCMD_DM_CODE         (0x0004)
#define BQ27421_SUBCMD_CHEM_ID         (0x0008)
#define BQ27421_SUBCMD_BAT_INSERT      (0x000C)
#define BQ27421_SUBCMD_BAT_REMOVED     (0x000D)
#define BQ27421_SUBCMD_SET_HIB          (0x0011)
#define BQ27421_SUBCMD_CLR_HIB          (0x0012)
#define BQ27421_SUBCMD_SET_CFGUPDATE    (0x0013)
#define BQ27421_SUBCMD_SHUTDOWN_EN      (0x001B)
#define BQ27421_SUBCMD_SHUTDOWN         (0x001C)
#define BQ27421_SUBCMD_SEALED           (0x0020)
#define BQ27421_SUBCMD_TOGGLE_GPOUT     (0x0023)
#define BQ27421_SUBCMD_RESET            (0x0041)
#define BQ27421_SUBCMD_SOFT_RESET       (0x0042)
#define BQ27421_SUBCMD_EXIT_CFGUPDATE   (0x0043)
#define BQ27421_SUBCMD_EXIT_RESIM       (0x0044)
#define BQ27421_SUBCMD_ENTER_CLEAR_SEAL           (0x8000)
#define BQ27421_SUBCMD_CLEAR_SEALED               (0x8000)
//#define BQ27421_SUBCMD_CLEAR_FULL_ACCESS_SEALED   (0xFFFF)

#define BQ27421_SEALED_MASK     (0x6000)
#define BQ27421_QEN_MASK        (1 << 0)

/*FLAGS BIT DEFINITION*/
/* Over-Temperature bit */
#define BQ27421_FLAG_OT         (1 << 15)
/* under-Temperature bit */
#define BQ27421_FLAG_UT         (1 << 14)

/* Full-charged bit */
#define BQ27421_FLAG_FC          (1 << 9)
/* charging detected bit */
#define BQ27421_FLAG_CHG         (1 << 8)
#define BQ27421_FLAG_ITPOR       (1 << 5)
#define BQ27421_FLAG_CFGUPMODE   (1 << 4)
#define BQ27421_FLAG_BAT_DET     (1 << 3)
/* State-of-Charge-Threshold 1 bit */
#define BQ27421_FLAG_SOC1        (1 << 2)
#define BQ27421_FLAG_SYSDOWN     (1 << 2)
#define BQ27421_FLAG_SOCF        (1 << 1)
#define BQ27421_FLAG_LOCK         (BQ27421_FLAG_SOC1 | BQ27421_FLAG_SOCF)
/* Discharging detected bit */
#define BQ27421_FLAG_DSG         (1 << 0)

#define CONST_NUM_10            (10)
#define CONST_NUM_2730          (2730)


/*extend data command*/
#define BQ27421_REG_OPCFG               (0x3A)
#define BQ27421_REG_DCAP                (0x3C)
#define BQ27421_REG_DESIGNEDCAPACITY    (BQ27421_REG_DCAP)
#define BQ27421_REG_DFCLS               (0x3E)
#define BQ27421_REG_DATA_FLASH_CLASS    (BQ27421_REG_DFCLS)
#define BQ27421_REG_DFBLK               (0x3F)
#define BQ27421_REG_DATA_FLASH_BLOCK    (BQ27421_REG_DFBLK)
#define BQ27421_REG_FLASH               (0x40)//64
#define BQ27421_REG_DFD                 (0x40)//BQ27421_REG_FLASH
#define BQ27421_REG_BLOCK_DATA          (BQ27421_REG_DFD)
#define BQ27421_REG_DFDCKS              (0x60)
#define BQ27421_REG_BLOCK_DATA_CHECKSUM (BQ27421_REG_DFDCKS)
#define BQ27421_REG_DFDCNTL             (0x61)
#define BQ27421_REG_BLOCK_DATA_CONTROL  (BQ27421_REG_DFDCNTL)
//#define BQ27421_REG_DNAMELEN            (0x62)
//#define BQ27421_REG_DNAME               (0x63)
//#define BQ27421_REG_APPSTAT             (0x6A)
//#define BQ27421_REG_APPSTATUS           (BQ27421_REG_APPSTAT)

/*Data Flash Summary*/
#define BQ27421_SUBCLASS_ID_SAFETY              (2)//0x02
//#define BQ27421_SUBCLASS_ID_CHGINH              (32)//0x20
//#define BQ27421_SUBCLASS_ID_CHG                 (34)//0x22
#define BQ27421_SUBCLASS_ID_CHG_ITERM           (36)//0x24
#define BQ27421_SUBCLASS_ID_DATA                (48)//0x30
#define BQ27421_SUBCLASS_ID_DISCHG              (49)//0x31
#define BQ27421_SUBCLASS_ID_FIRMWARE_VERSION    (57)//0x39
//#define BQ27421_SUBCLASS_ID_SYSDATA             (58)//0x3A
#define BQ27421_SUBCLASS_ID_REG                 (64)//0x40
#define BQ27421_SUBCLASS_ID_POWER               (68)//0x44
#define BQ27421_SUBCLASS_ID_IT_CFG              (80)//0x50
#define BQ27421_SUBCLASS_ID_CURR                (81)//0x51
#define BQ27421_SUBCLASS_ID_STATE               (82)//0x52
#define BQ27421_SUBCLASS_ID_RARAM               (89)//0x59
#define BQ27421_SUBCLASS_ID_DATA1              (104)//0x68
#define BQ27421_SUBCLASS_ID_CCCAL              (105)//0x69
#define BQ27421_SUBCLASS_ID_CUR                (107)//0x6B
#define BQ27421_SUBCLASS_ID_CODES              (112)//0x70
/*subclass id is 82 reg*/
#define BQ27421_REG_QMAX                      (0x40)/*block 0*/
#define BQ27421_REG_QMAX1                     (0x41)
#define BQ27421_REG_DESIGN_CAPACITY           (0x4A)
#define BQ27421_REG_DESIGN_ENERGE             (0x4C)
#define BQ27421_REG_TERMINATE_VOLT            (0x50)
#define BQ27421_REG_TAPER_RATE                (0x5B)
#define BQ27421_REG_TAPER_VOLT                (0x5D)
#define BQ27421_REG_VCHG_TERM                 (0x41)/*block 1 offset = 33*/
#define BQ27421_REG_FH3                       (0x41)/*block 1 offset = 33*/
#define BQ27421_REG_FH2                       (0x5A)/*block 0 offset = 26*/
#define BQ27421_REG_FH1                       (0x58)/*block 0 offset = 24*/
#define BQ27421_REG_FH0                       (0x4B)/*block 0 offset = 11*/
#define BQ27421_REG_HIBERNATE_V               (0x49)/*block 0 offset = 9*/

/* added for Firmware upgrade begine */
#define BSP_UPGRADE_FIRMWARE_BQFS_CMD       "upgradebqfs"
#define BSP_UPGRADE_FIRMWARE_DFFS_CMD       "upgradedffs"
/*库仑计的完整的firmware，包含可执行镜像及数据*/
#define BSP_UPGRADE_FIRMWARE_BQFS_NAME      "/system/etc/coulometer/bq27421_pro.bqfs"
/*库仑计的的数据信息*/
#define BSP_UPGRADE_FIRMWARE_DFFS_NAME      "/system/etc/coulometer/bq27421_pro.dffs"

#define BSP_ROM_MODE_I2C_ADDR               (0x0B)
#define BSP_NORMAL_MODE_I2C_ADDR            (0x55)

#define BSP_FIRMWARE_FILE_SIZE              (400*1024)
#define BSP_I2C_MAX_TRANSFER_LEN            (128)
#define BSP_MAX_ASC_PER_LINE                (400)
#define BSP_FIRMWARE_DOWNLOAD_MODE          (0xDDDDDDDD)
#define BSP_NORMAL_MODE                     (0x00)

#define BSP_ENTER_ROM_MODE_CMD          (BQ27421_REG_CTRL)//0x00
#define BSP_ENTER_ROM_MODE_DATA         (0x0F00)
#define BSP_LEAVE_ROM_MODE_REG1         (0x00)
#define BSP_LEAVE_ROM_MODE_DATA1        (0x0F)
#define BSP_LEAVE_ROM_MODE_REG2         (0x64)
#define BSP_LEAVE_ROM_MODE_DATA2        (0x0F)
#define BSP_LEAVE_ROM_MODE_REG3         (0x65)
#define BSP_LEAVE_ROM_MODE_DATA3        (0x00)

#define DISABLE_ACCESS_TIME             (2000)
#define I2C_RETRY_TIME_MAX              (3)
#define SLEEP_TIMEOUT_MS                (5)//ms

#define BQ27421_ERR(format, arg...)     do { printk(KERN_ERR format, ## arg);  } while (0)
#define BQ27421_INFO(format, arg...)    do { printk(format);} while (0)

/*#define BQ27421_DEBUG_FLAG*/
#if defined(BQ27421_DEBUG_FLAG)
#define BQ27421_DBG(format, arg...)     do { printk(KERN_ALERT format, ## arg);  } while (0)
#define BQ27421_FAT(format, arg...)     do { printk(KERN_CRIT format, ## arg); } while (0)
#else
#define BQ27421_DBG(format, arg...)     do { (void)(format); } while (0)
#define BQ27421_FAT(format, arg...)     do { (void)(format); } while (0)
#endif


struct bq27421_device_info {
    struct device   *dev;
    int             id;
    struct i2c_client   *client;
    struct i2c_client   *rom_client;
    struct comip_fuelgauge_info *pdata;
    struct delayed_work notifier_work;
    struct delayed_work gaugelog_work;
    bool   filp_open;
    struct work_struct  cfg_work;
    unsigned long timeout_jiffies;
    int sealed;
};

extern struct bq27421_device_info *bq27421_device;
extern struct i2c_client *bq27421_i2c_client;

/*external functions*/ 
extern int bq27421_battery_voltage(void);
extern short bq27421_battery_current(void);
extern int bq27421_battery_capacity(void);
extern int bq27421_battery_temperature(void);
extern int is_bq27421_battery_exist(void);
extern int bq27421_battery_rm(void);
extern int bq27421_battery_fcc(void);
extern int is_bq27421_battery_full(void);
extern int bq27421_battery_health(void);
extern int bq27421_battery_capacity_level(void);
extern int bq27421_battery_technology(void);
extern int is_bq27421_battery_reach_threshold(void);

/*bq27421 firmware data sotre*/
extern unsigned char *bq27421_xwd_2200_fw;
extern unsigned char *bq27421_xwd_2200_fw_end;
extern unsigned char *bq27421_gy_2200_fw;
extern unsigned char *bq27421_gy_2200_fw_end;
extern unsigned char *bq27421_ds_2200_fw;
extern unsigned char *bq27421_ds_2200_fw_end;
#endif /* _BQ27421_BATTERY_H */
