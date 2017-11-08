/* 
 *
 * Use of source code is subject to the terms of the LeadCore license agreement under
 * which you licensed source code. If you did not accept the terms of the license agreement,
 * you are not authorized to use source code. For the terms of the license, please see the
 * license agreement between you and LeadCore.
 *
 * Copyright (c) 2010-2019	LeadCoreTech Corp.
 *
 *	PURPOSE: This files contains the comip vibrator driver.
 *
 *	CHANGE HISTORY:
 *
 *	Version	Date		Author		Description
 *	1.0.0	2012-03-06	zouqiao		created
 *
 */

#ifndef __COMIP_VIBRATOR__
#define __COMIP_VIBRATOR__

struct comip_vibrator_platform_data{
	char *name;
	int vibrator_strength;
};

#endif /*__COMIP_VIBRATOR__*/

