#ifndef __KERNAL_DEBUG_H__
#define __KERNAL_DEBUG_H__
#include "BaseLib/PrimitiveTypes.h"


//What the kernal tells the emulator for a debugger (DebugLib/HCP65DebugMessage.h): each process started and ended, each
//main memory bank given to a process and each driver loaded from the SD card (where it is, before it runs).  They cost
//little, and on a real HCP65 they do nothing.
void	KernalDebugProcessStarted(uint8 uiProcess, const char* szPath);
void	KernalDebugProcessEnded(uint8 uiProcess, uint16 uiExitCode);
void	KernalDebugBank(uint8 uiPhysical);
void	KernalDebugDriver(uint16 uiSlot, const char* szPath, uint32 uiAddress, uint32 uiSize);


#endif // __KERNAL_DEBUG_H__
