#ifndef __INC_PRECOMPINC_H
#define __INC_PRECOMPINC_H


//HAL Shared with Driver
#include "Include/StatusCode.h"
#include "HalDbgCmd.h"


//Prototype
#include "HalDef.h"

//MAC Header provided by SD1 HWSD
#include "HalHeader/HalHWCfg.h"
#include "HalHeader/HalComTXDesc.h"
#include "HalHeader/HalComRXDesc.h"
#include "HalHeader/HalComBit.h"
#include "HalHeader/HalComReg.h"
#include "HalHeader/HalComPhyBit.h"
#include "HalHeader/HalComPhyReg.h"

//Instance
#include "HalCommon.h"

#if IS_RTL88XX_GENERATION

#include "RTL88XX/Hal88XXPwrSeqCmd.h"
#include "RTL88XX/Hal88XXReg.h"
#include "RTL88XX/Hal88XXDesc.h"
#include "RTL88XX/Hal88XXTxDesc.h"
#include "RTL88XX/Hal88XXRxDesc.h"
#include "RTL88XX/Hal88XXFirmware.h"
#include "RTL88XX/Hal88XXIsr.h"
#include "RTL88XX/Hal88XXDebug.h"
#include "RTL88XX/Hal88XXPhyCfg.h"
#include "RTL88XX/Hal88XXDM.h"


#if IS_RTL8881A_SERIES
#include "RTL88XX/RTL8881A/Hal8881APwrSeqCmd.h"
#include "RTL88XX/RTL8881A/Hal8881ADef.h"
#include "RTL88XX/RTL8881A/Hal8881APhyCfg.h"
#endif

#if IS_RTL8192E_SERIES
#include "RTL88XX/RTL8192E/Hal8192EPwrSeqCmd.h"
#include "RTL88XX/RTL8192E/Hal8192EDef.h"
#include "RTL88XX/RTL8192E/Hal8192EPhyCfg.h"
#endif

#if IS_RTL8814A_SERIES
#include "Hal8814APwrSeqCmd.h"
#include "Hal8814ADef.h"
#include "Hal8814APhyCfg.h"
#include "Hal8814AFirmware.h"

#endif

#if IS_EXIST_RTL8192EE
#include "RTL88XX/RTL8192E/RTL8192EE/Hal8192EEDef.h"
#endif

#if IS_EXIST_RTL8814AE
#include "Hal8814AEDef.h"
#endif


#include "RTL88XX/Hal88XXDef.h"
#endif  //IS_RTL88XX_GENERATION

#endif  //#ifndef __INC_PRECOMPINC_H
