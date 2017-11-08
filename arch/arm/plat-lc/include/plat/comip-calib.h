/* 
**
** Use of source code is subject to the terms of the LeadCore license agreement under
** which you licensed source code. If you did not accept the terms of the license agreement,
** you are not authorized to use source code. For the terms of the license, please see the
** license agreement between you and LeadCore.
**
** Copyright (c) 2013-2019  LeadCoreTech Corp.
**
**  PURPOSE:		This files contains generic timer operation.
**
**  CHANGE HISTORY:
**
**	Version		Date		Author		Description
**	1.0.0		2013-05-29	zouqiao		created
**
*/

#ifndef __ASM_ARCH_COMIP_CALIB_H
#define __ASM_ARCH_COMIP_CALIB_H

typedef int (*calibration_get)(unsigned int, unsigned int *, unsigned int *);

#ifndef CONFIG_COMIP_CALIBRATION

#define register_calibration_notifier(nb)
#define unregister_calibration_notifier(nb)

#else

#define register_calibration_notifier(nb) \
		blocking_notifier_chain_register(&calibration_notifier_list, nb);

#define unregister_calibration_notifier(nb) \
		blocking_notifier_chain_unregister(&calibration_notifier_list, nb);

extern struct blocking_notifier_head calibration_notifier_list;

#endif /*CONFIG_COMIP_CALIBRATION*/

#endif /* __ASM_ARCH_COMIP_CALIB_H */

