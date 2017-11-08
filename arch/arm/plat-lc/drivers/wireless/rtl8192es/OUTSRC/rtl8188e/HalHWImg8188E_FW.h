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

#if (RTL8188E_SUPPORT == 1)
#ifndef __INC_FW_8188E_HW_IMG_H
#define __INC_FW_8188E_HW_IMG_H


/******************************************************************************
*                           FW_AP.TXT
******************************************************************************/

#define ArrayLength_8188E_FW_AP         11216
extern u1Byte Array_8188E_FW_AP[ArrayLength_8188E_FW_AP];

/******************************************************************************
*                           FW_NIC.TXT
******************************************************************************/

#if 0
#define ArrayLength_8188E_FW_NIC		14490
extern u1Byte Array_8188E_FW_NIC[ArrayLength_8188E_FW_NIC];
#endif

/******************************************************************************
*                           FW_WoWLAN.TXT
******************************************************************************/

#if 0
#define ArrayLength_8188E_FW_WoWLAN	14702
extern u1Byte Array_8188E_FW_WoWLAN[ArrayLength_8188E_FW_WoWLAN];
#endif

#endif
#endif // end of HWIMG_SUPPORT

