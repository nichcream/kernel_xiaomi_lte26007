/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip boot information.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date		Author		Description
 *	1.0.0	2012-03-06	zouqiao		created
 *
 */

#ifndef __BOOTINFO_H__
#define __BOOTINFO_H__

enum {
	PU_REASON_HARDWARE_RESET,
	PU_REASON_WATCHDOG,
	PU_REASON_RTC_ALARM,
	PU_REASON_PWR_KEY_PRESS,
	PU_REASON_USB_CABLE,
	PU_REASON_USB_FACTORY,
	PU_REASON_USB_CHARGER,
	PU_REASON_REBOOT_NORMAL,
	PU_REASON_REBOOT_FACTORY,
	PU_REASON_REBOOT_RECOVERY,
	PU_REASON_REBOOT_FOTA,
	PU_REASON_REBOOT_CRITICAL,
	PU_REASON_REBOOT_UNKNOWN,
	PU_REASON_LOW_PWR_RESET,
	PU_REASON_MAX
};

enum {
	REBOOT_NONE,
	REBOOT_NORMAL,
	REBOOT_POWER_KEY,	/* Power key reboot in power off charge mode. */
	REBOOT_RTC_ALARM,	/* RTC alarm reboot in power off charge mode. */
	REBOOT_RECOVERY,
	REBOOT_FOTA,
	REBOOT_CRITICAL,
	REBOOT_FACTORY,
	REBOOT_UNKNOWN,
	REBOOT_MAX
};

enum {
	BOOT_MODE_NORMAL,
	BOOT_MODE_AMT1,
	BOOT_MODE_AMT3,
	BOOT_MODE_RECOVERY,
	BOOT_MODE_MAX
};

#define REBOOT_REASON_MASK		(0x1f)
#define REBOOT_USER_FLAG		(0x20)

unsigned int get_boot_reason(void);
const char* get_boot_reason_string(unsigned int boot_info);
void set_boot_reason(unsigned int boot_info);
unsigned int get_reboot_reason(void);
const char* get_reboot_reason_string(unsigned int reboot_info);
void set_reboot_reason(unsigned int reboot_info);
unsigned int get_boot_mode(void);
const char* get_boot_mode_string(unsigned int boot_mode);
void set_boot_mode(unsigned int boot_mode);

#endif/* __BOOTINFO_H__ */

