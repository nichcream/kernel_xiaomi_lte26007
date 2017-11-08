/*
 *
 * 
 * Copyright (C) 2013 Leadcore, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 */

#ifndef __ARCH_ARM_PLAT_CPU_H__
#define __ARCH_ARM_PLAT_CPU_H__

extern unsigned int g_comip_bb_id;
extern unsigned int g_comip_chip_id;
extern unsigned int g_comip_rom_id;

#define COMIP_BB_ID			(g_comip_bb_id)
#define COMIP_CHIP_ID			(g_comip_chip_id)
#define COMIP_ROM_ID			(g_comip_rom_id)

#if defined(CONFIG_CPU_LC1810) || defined(CONFIG_CPU_LC1813)
#define MF_ID_BIT			(30)
#define PART_ID_BIT			(18)
#define REV_ID_BIT			(16)
#define VOLT_ID_BIT			(9)
#elif defined(CONFIG_CPU_LC1860)
#define MF_ID_BIT			(22)
#define PART_ID_BIT			(10)
#define REV_ID_BIT			(8)
#define VOLT_ID_BIT			(1)
#endif

#define MF_ID_MASK			(0x3 << MF_ID_BIT)
#define MF_ID_TSMC			(0x0 << MF_ID_BIT)
#define MF_ID_SMIC			(0x1 << MF_ID_BIT)

#define PART_ID_MASK			(0x3ffc << REV_ID_BIT)
#define PART_ID_LC1810			(0x2420 << REV_ID_BIT)
#define PART_ID_LC1813			(0x2460 << REV_ID_BIT)
#define PART_ID_LC1813S			(0x2480 << REV_ID_BIT)
#define PART_ID_LC1860			(0x2820 << REV_ID_BIT)
#define PART_ID_LC1860C			(0x282c << REV_ID_BIT)

#define REV_ID_MASK			(0x3 << REV_ID_BIT)
#define REV_ID_1			(0x1 << REV_ID_BIT)
#define REV_ID_2			(0x2 << REV_ID_BIT)
#define REV_ID_3			(0x3 << REV_ID_BIT)

#define VOLT_ID_MASK			(0x3 << VOLT_ID_BIT)
#define VOLT_ID_INC1			(0x1 << VOLT_ID_BIT)
#define VOLT_ID_INC2			(0x2 << VOLT_ID_BIT)
#define VOLT_ID_INC3			(0x3 << VOLT_ID_BIT)

#if defined(CONFIG_CPU_LC1810)
#define cpu_is_lc1810()		((COMIP_BB_ID & PART_ID_MASK) == PART_ID_LC1810)
#else
#define cpu_is_lc1810()		(0)
#endif

#if defined(CONFIG_CPU_LC1813)
#define cpu_is_lc1813()		((COMIP_BB_ID & PART_ID_MASK) == PART_ID_LC1813)
#define cpu_is_lc1813s()	((COMIP_BB_ID & PART_ID_MASK) == PART_ID_LC1813S)
#define cpu_volt_flag()		(COMIP_BB_ID & VOLT_ID_MASK)
#else
#define cpu_is_lc1813()		(0)
#define cpu_is_lc1813s()	(0)
#define cpu_volt_flag()		(0)
#endif

#if defined(CONFIG_CPU_LC1860)
#define cpu_is_lc1860()		((COMIP_BB_ID & PART_ID_MASK) == PART_ID_LC1860)
#define cpu_is_lc1860c()	((COMIP_BB_ID & PART_ID_MASK) == PART_ID_LC1860C)
#define cpu_is_lc1860_eco1()	((COMIP_BB_ID & REV_ID_MASK) == REV_ID_1)
#define cpu_is_lc1860_eco2()	((COMIP_BB_ID & REV_ID_MASK) == REV_ID_2)
#else
#define cpu_is_lc1860()		(0)
#define cpu_is_lc1860c()	(0)
#define cpu_is_lc1860_eco1()	(0)
#define cpu_is_lc1860_eco2()	(0)
#endif

#endif /* __ARCH_ARM_PLAT_CPU_H__ */
