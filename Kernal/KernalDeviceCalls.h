#ifndef __KERNAL_DEVICE_CALLS_H__
#define __KERNAL_DEVICE_CALLS_H__
#include "BaseLib/PrimitiveTypes.h"


#define KERNAL_MAX_HANDLES		32		//Handles open at once, every process's.


//The device calls (COP 12, UserLib/HCP65User.h's KernalDevice*): a process's handles on units of the boards, through
//their drivers.  The call block is at uiAddress in the running process's memory.  Returns a KERNAL_DEVICE_* result.
uint16	KernalCallDevice(uint32 uiAddress);

//The handles uiProcess has open are let go (it has ended); KernalCloseDrivers lets its boards go.
void	KernalCloseHandles(uint8 uiProcess);


#endif // __KERNAL_DEVICE_CALLS_H__
