
#ifndef _MAX17058_BATTERY_H
#define _MAX17058_BATTERY_H

#define MAX17058_I2C_ADDR             (0x36)

#define DRIVER_VERSION               "1.0.0"


#define DISABLE_ACCESS_TIME             (2000)
#define I2C_RETRY_TIME_MAX              (3)
#define SLEEP_TIMEOUT_MS                (5)//ms

/*16bit reg*/
#define MAX17058_REG_VCELL      (0x02)
#define MAX17058_REG_SOC        (0x04)
#define MAX17058_REG_MODE       (0x06)
#define MAX17058_REG_VERSION    (0x08)
#define MAX17058_REG_CONFIG     (0x0C)
#define MAX17058_REG_OCV        (0x0E)
#define MAX17058_REG_VRESET     (0x18)
#define MAX17058_REG_STATUS     (0x1A)
#define MAX17058_REG_CMD        (0xFE)
/*8bit reg*/
#define MAX17058_REG_VCELL_MSB  (0x02)
#define MAX17058_REG_VCELL_LSB  (0x03)
#define MAX17058_REG_SOC_MSB    (0x04)
#define MAX17058_REG_SOC_LSB    (0x05)
#define MAX17058_REG_MODE_MSB   (0x06)
#define MAX17058_REG_MODE_LSB   (0x07)
#define MAX17058_REG_VERSION_MSB    (0x08)
#define MAX17058_REG_VERSION_LSB    (0x09)
#define MAX17058_REG_RCOMP_MSB  (0x0C)
#define MAX17058_REG_RCOMP_LSB  (0x0D)
#define MAX17058_REG_OCV_MSB    (0x0E)
#define MAX17058_REG_OCV_LSB    (0x0F)
#define MAX17058_REG_VRESET_MSB (0x18)
#define MAX17058_REG_VRESET_LSB (0x19)
#define MAX17058_REG_STATUS_MSB (0x1A)
#define MAX17058_REG_STATUS_LSB (0x1B)
#define MAX17058_REG_CMD_MSB    (0xFE)
#define MAX17058_REG_CMD_LSB    (0xFF)
#define MAX17058_REG_LOCK_MSB   (0x3E)
#define MAX17058_REG_LOCK_LSB   (0x3F)

#define MAX17058_QUICK_START    (1 << 14)
#define MAX17058_ENSLEEP        (1 << 13)
#define MAX17058_SLEEP          (1 << 7)
#define MAX17058_ALRT           (1 << 5)
#define MAX17058_HD             (1 << 12)
#define MAX17058_RI             (1 << 8)

#define CONST_VCELL_78125         (78125)
#define CONST_VCELL_1000          (1000)
#define CONST_TEMP_20             (20)
#define VRESET_VOLT               (2800)
#define ALERT_SOC                 (4)
#define RELEASE_ALERT_SOC         (6)
#define MAX17058_VERSION          (0x0012)
#define max17058_MODEL_ACCESS_UNLOCK    (0x4A57)
#define max17058_MODEL_ACCESS_LOCK      (0x0000)


#define MAX17058_ERR(format, arg...)     do { printk(KERN_ERR format, ## arg);  } while (0)
#define MAX17058_INFO(format, arg...)    do { printk(format);} while (0)

/*#define MAX17058_DEBUG_FLAG*/
#if defined(MAX17058_DEBUG_FLAG)
#define MAX17058_DBG(format, arg...)     do { printk(KERN_ALERT format, ## arg);  } while (0)
#define MAX17058_FAT(format, arg...)     do { printk(KERN_CRIT format, ## arg); } while (0)
#else
#define MAX17058_DBG(format, arg...)     do { (void)(format); } while (0)
#define MAX17058_FAT(format, arg...)     do { (void)(format); } while (0)
#endif



/*external functions*/
extern int max17058_battery_voltage(void);
extern short max17058_battery_current(void);
extern int max17058_battery_capacity(void);
extern int is_max17058_battery_exist(void);
extern int max17058_battery_temperature(void);
extern int max17058_compensation_rcomp(int temp);


#endif /* _MAX17058_BATTERY_H */
