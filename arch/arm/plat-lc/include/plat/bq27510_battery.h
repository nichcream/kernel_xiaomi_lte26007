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

#ifndef _BQ27510_BATTERY_H
#define _BQ27510_BATTERY_H

#define BQ27510_I2C_ADDR             (0x55)
#define BQ27510_ROM_MODE_I2C_ADDR    (0x0B)
#define DRIVER_VERSION               "1.0.0"

#define BQ27510_REG_CTRL    (0x00)
#define BQ27510_REG_AR      (0x02)
#define BQ27510_REG_ARTTE   (0x04)
#define BQ27510_REG_TEMP    (0x06)
#define BQ27510_REG_VOLT    (0x08)
#define BQ27510_REG_FLAGS   (0x0A)
#define BQ27510_REG_NAC     (0x0C)
#define BQ27510_REG_FAC     (0x0E)
#define BQ27510_REG_RM      (0x10)
#define BQ27510_REG_FCC     (0x12)
#define BQ27510_REG_AI      (0x14)
#define BQ27510_REG_TTE     (0x16)
#define BQ27510_REG_TTF     (0x18)
#define BQ27510_REG_SI      (0x1A)
#define BQ27510_REG_STTE    (0x1C)
#define BQ27510_REG_MLI     (0x1E)
#define BQ27510_REG_MLTTE   (0x20)
#define BQ27510_REG_AE      (0x22)
#define BQ27510_REG_AP      (0x24)
#define BQ27510_REG_TTECP   (0x26)
//#define BQ27510_REG_SOH     (0x28)
#define BQ27510_REG_CC      (0x2A)
#define BQ27510_REG_SOC     (0x2C)
//#define BQ27510_REG_ICR      (0x30)
//#define BQ27510_REG_LOGIDX   (0x32)
//#define BQ27510_REG_LOGBUF   (0x34)


/*CONTROL() subcommand*/
#define BQ27510_SUBCMD_CONTROL_STATUS  (0x0000)
#define BQ27510_SUBCMD_DEVICES_TYPE    (0x0001)
#define BQ27510_SUBCMD_FW_VERSION      (0x0002)
#define BQ27510_SUBCMD_HW_VERSION      (0x0003)
#define BQ27510_SUBCMD_DF_CHECKSUM     (0x0004)
#define BQ27510_SUBCMD_RESET_DATA      (0x0005)
#define BQ27510_SUBCMD_PREV_MACWRITE   (0x0007)
#define BQ27510_SUBCMD_CHEM_ID         (0x0008)
#define BQ27510_SUBCMD_BD_OFFSET       (0x0009)
//#define BQ27510_SUBCMD_INT_OFFSET      (0x000A)
//#define BQ27510_SUBCMD_CC_VERSION      (0x000B)
//#define BQ27510_SUBCMD_OCV             (0x000C)
//#define BQ27510_SUBCMD_BAT_INSERT      (0x000D)
//#define BQ27510_SUBCMD_BAT_REMOVED     (0x000E)
#define BQ27510_SUBCMD_SET_HIB          (0x0011)
#define BQ27510_SUBCMD_CLR_HIB          (0x0012)
#define BQ27510_SUBCMD_SET_SLP          (0x0013)
#define BQ27510_SUBCMD_CLR_SLP          (0x0014)
#define BQ27510_SUBCMD_SEALED           (0x0020)
#define BQ27510_SUBCMD_IT_ENABLE        (0x0021)
#define BQ27510_SUBCMD_IF_CHECKSUM      (0x0022)
#define BQ27510_SUBCMD_CAL_MODE         (0x0040)
#define BQ27510_SUBCMD_RESET           ( 0x0041)
#define BQ27510_SUBCMD_ENTER_CLEAR_SEAL           (0x0414)
#define BQ27510_SUBCMD_CLEAR_SEALED               (0x3672)
#define BQ27510_SUBCMD_CLEAR_FULL_ACCESS_SEALED   (0xFFFF)

#define BQ27510_SEALED_MASK     (0x6000)
#define BQ27510_QEN_MASK        (1 << 0)

/*FLAGS BIT DEFINITION*/
/* Over-Temperature-Charge bit */
#define BQ27510_FLAG_OTC         (1 << 15)
/* Over-Temperature-Discharge bit */
#define BQ27510_FLAG_OTD         (1 << 14)
#define BQ27510_FLAG_XCHG        (1 << 10)
/* Full-charged bit */
#define BQ27510_FLAG_FC          (1 << 9)
#define BQ27510_FLAG_BAT_DET     (1 << 3)
/* State-of-Charge-Threshold 1 bit */
#define BQ27510_FLAG_SOC1        (1 << 2)
#define BQ27510_FLAG_SYSDOWN     (1 << 2)
#define BQ27510_FLAG_SOCF        (1 << 1)
#define BQ27510_FLAG_LOCK         (BQ27510_FLAG_SOC1 | BQ27510_FLAG_SOCF)
/* Discharging detected bit */
#define BQ27510_FLAG_DSG         (1 << 0)

#define CONST_NUM_10            (10)
#define CONST_NUM_2730          (2730)


/*extend data command*/
#define BQ27510_REG_DCAP                (0x3C)
#define BQ27510_REG_DESIGNEDCAPACITY    (BQ27510_REG_DCAP)
#define BQ27510_REG_DFCLS               (0x3E)
#define BQ27510_REG_DATA_FLASH_CLASS    (BQ27510_REG_DFCLS)
#define BQ27510_REG_DFBLK               (0x3F)
#define BQ27510_REG_DATA_FLASH_BLOCK    (BQ27510_REG_DFBLK)
#define BQ27510_REG_FLASH               (0x40)//64
#define BQ27510_REG_DFD                 (0x40)//BQ27510_REG_FLASH
#define BQ27510_REG_BLOCK_DATA          (BQ27510_REG_DFD)
#define BQ27510_REG_DFDCKS              (0x60)
#define BQ27510_REG_BLOCK_DATA_CHECKSUM (BQ27510_REG_DFDCKS)
#define BQ27510_REG_DFDCNTL             (0x61)
#define BQ27510_REG_BLOCK_DATA_CONTROL  (BQ27510_REG_DFDCNTL)
#define BQ27510_REG_DNAMELEN            (0x62)
#define BQ27510_REG_DNAME               (0x63)
#define BQ27510_REG_APPSTAT             (0x6A)
#define BQ27510_REG_APPSTATUS           (BQ27510_REG_APPSTAT)

/*Data Flash Summary*/
#define BQ27510_SUBCLASS_ID_SAFETY              (2)//0x02
#define BQ27510_SUBCLASS_ID_CHGINH              (32)//0x20
#define BQ27510_SUBCLASS_ID_CHG                 (34)//0x22
#define BQ27510_SUBCLASS_ID_CHG_ITERM           (36)//0x24
#define BQ27510_SUBCLASS_ID_DATA                (48)//0x30
#define BQ27510_SUBCLASS_ID_DISCHG              (49)//0x31
#define BQ27510_SUBCLASS_ID_FIRMWARE_VERSION    (57)//0x39
#define BQ27510_SUBCLASS_ID_SYSDATA             (58)//0x3A
#define BQ27510_SUBCLASS_ID_REG                 (64)//0x40
#define BQ27510_SUBCLASS_ID_POWER               (68)//0x44
#define BQ27510_SUBCLASS_ID_IT_CFG              (80)//0x50
#define BQ27510_SUBCLASS_ID_CURR                (81)//0x51
#define BQ27510_SUBCLASS_ID_STATE               (82)//0x52
#define BQ27510_SUBCLASS_ID_PACK0               (91)//0x5B
#define BQ27510_SUBCLASS_ID_PACK1               (92)//0x5C
#define BQ27510_SUBCLASS_ID_PACK2               (93)//0x5D
#define BQ27510_SUBCLASS_ID_PACK3               (94)//0x5E
#define BQ27510_REG_QMAX                      (0x42)
#define BQ27510_REG_QMAX1                     (0x43)


/* added for Firmware upgrade begine */
#define BSP_UPGRADE_FIRMWARE_BQFS_CMD       "upgradebqfs"
#define BSP_UPGRADE_FIRMWARE_DFFS_CMD       "upgradedffs"
/*库仑计的完整的firmware，包含可执行镜像及数据*/
#define BSP_UPGRADE_FIRMWARE_BQFS_NAME      "/system/etc/coulometer/bq27510_pro.bqfs"
/*库仑计的的数据信息*/
#define BSP_UPGRADE_FIRMWARE_DFFS_NAME      "/system/etc/coulometer/bq27510_pro.dffs"

#define BSP_ROM_MODE_I2C_ADDR               (0x0B)
#define BSP_NORMAL_MODE_I2C_ADDR            (0x55)

#define BSP_FIRMWARE_FILE_SIZE              (400*1024)
#define BSP_I2C_MAX_TRANSFER_LEN            (128)
#define BSP_MAX_ASC_PER_LINE                (400)
#define BSP_FIRMWARE_DOWNLOAD_MODE          (0xDDDDDDDD)
#define BSP_NORMAL_MODE                     (0x00)

#define BSP_ENTER_ROM_MODE_CMD          (BQ27510_REG_CTRL)//0x00
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

#define BQ27510_ERR(format, arg...)     do { printk(KERN_ERR format, ## arg);  } while (0)
#define BQ27510_INFO(format, arg...)    do { printk(format);} while (0)

/*#define BQ27510_DEBUG_FLAG*/
#if defined(BQ27510_DEBUG_FLAG)
#define BQ27510_DBG(format, arg...)     do { printk(KERN_ALERT format, ## arg);  } while (0)
#define BQ27510_FAT(format, arg...)     do { printk(KERN_CRIT format, ## arg); } while (0)
#else
#define BQ27510_DBG(format, arg...)     do { (void)(format); } while (0)
#define BQ27510_FAT(format, arg...)     do { (void)(format); } while (0)
#endif


struct bq27510_device_info {
    struct device   *dev;
    int             id;
    struct i2c_client   *client;
    struct i2c_client   *rom_client;
    struct delayed_work notifier_work;
    unsigned long timeout_jiffies;
    int sealed;
};

extern struct bq27510_device_info *bq27510_device;
extern struct i2c_client *bq27510_i2c_client;

/*external functions*/ 
extern int bq27510_battery_voltage(void);
extern short bq27510_battery_current(void);
extern int bq27510_battery_capacity(void);
extern int bq27510_battery_temperature(void);
extern int is_bq27510_battery_exist(void);
extern int bq27510_battery_rm(void);
extern int bq27510_battery_fcc(void);
extern int is_bq27510_battery_full(void);
extern int bq27510_battery_health(void);
extern int bq27510_battery_capacity_level(void);
extern int bq27510_battery_technology(void);
extern int is_bq27510_battery_reach_threshold(void);



#endif /* _BQ27510_BATTERY_H */
