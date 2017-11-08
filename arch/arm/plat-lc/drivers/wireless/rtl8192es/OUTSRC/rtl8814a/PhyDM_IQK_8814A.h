/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

#ifndef __PHYDM_IQK_8814A_H__
#define __PHYDM_IQK_8814A_H__

/*--------------------------Define Parameters-------------------------------*/

#include "../HalPhyRf.h"


//1 7.	IQK

#if 0

void	
PHY_IQCalibrate_8814A(	
	IN	PADAPTER	pAdapter,	
	IN	BOOLEAN 	bReCovery
);

#else

                         
VOID	
phy_IQCalibrate_8814A(
	IN PDM_ODM_T	pDM_Odm,
	IN u1Byte		Channel
);


VOID
PHY_IQCalibrate_8814A(
		IN PDM_ODM_T pDM_Odm,
		IN	BOOLEAN 	bReCovery
);
#endif

#endif	// #ifndef __PHYDM_IQK_8814A_H__