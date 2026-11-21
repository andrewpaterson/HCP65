#ifndef __KERNAL_SYSTEM_CALLS_H__
#define __KERNAL_SYSTEM_CALLS_H__
#include "BaseLib/PrimitiveTypes.h"


//The system calls (COP 13: HCP65User.h's SKernalSystemCall): the SD card's directories and files beyond reading them,
//the kernal's memory, and mapping the heap's banks.
uint16	KernalCallSystem(uint32 uiAddress);


#endif // __KERNAL_SYSTEM_CALLS_H__
