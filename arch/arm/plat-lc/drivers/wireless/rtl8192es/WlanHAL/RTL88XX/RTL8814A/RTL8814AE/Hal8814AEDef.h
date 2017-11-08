#ifndef __HAL8814AE_DEF_H__
#define __HAL8814AE_DEF_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8192EEDef.h
	
Abstract:
	Defined HAL 8814AE data structure & Define
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2013-05-28 Filen            Create.	
--*/


RT_STATUS
InitPON8814AE(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          ClkSel        
);

RT_STATUS
StopHW8814AE(
    IN  HAL_PADAPTER    Adapter
);


RT_STATUS
ResetHWForSurprise8814AE(
    IN  HAL_PADAPTER Adapter
);


RT_STATUS	
hal_Associate_8814AE(
    HAL_PADAPTER        Adapter,
    BOOLEAN             IsDefaultAdapter
);
















#endif  //__HAL8814AE_DEF_H__

