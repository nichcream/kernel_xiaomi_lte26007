/*
* ST-Ericsson ETF driver
*
* Copyright (c) ST-Ericsson SA 2011
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
/****************************************************************************
* debug.h
*
* This module serves as the lower interface header file for the adapter module
* and all driver's module.
*
****************************************************************************/

#ifndef _DEBUG_H
#define _DEBUG_H

extern int debug_level;

#define DBG_NONE    0x00000000
#define DBG_SBUS    0x00000001
#define DBG_MESSAGE 0x00000002
#define DBG_FUNC    0x00000004
#define DBG_ERROR   0x00000008
#define DBG_ALWAYS  DBG_ERROR

#define DEBUG(f, args... ) do{if(f & debug_level) printk(args);}while(0)

#endif /* _DEBUG_H */
