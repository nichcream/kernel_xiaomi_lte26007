/* arch/arm/mach-comip/inclue/mach/generic.h
 *
 * Use of source code is subject to the terms of the LeadCore license agreement
 * under which you licensed source code. If you did not accept the terms of the
 * license agreement, you are not authorized to use source code. For the terms
 * of the license, please see the license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 * PURPOSE: Comip platform head file.
 *
 * CHANGE HISTORY:
 *
 * Version	Date		Author		Description
 * 1.0.0	2012-3-12	lengyansong	created
 *
 */

extern void __init comip_map_io(void);
extern void __init comip_generic_init(void);
extern void __init comip_cache_init(void);
extern void __init comip_irq_init(void);
extern void __init comip_dmas_init(void);
extern void __init comip_gpio_init(int enable);
extern void __init comip_clks_init(void);
extern void __init comip_mtc_init(void);

extern struct smp_operations lc1813_smp_ops;

