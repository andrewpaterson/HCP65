#ifndef __KERNAL_DEBUG_PORT_H__
#define __KERNAL_DEBUG_PORT_H__
#include "KernalDriver.h"


//The emulator's debug port (HCP65Lib/HCP65DebugPort.h).  Its registers, from the slot's device registers ($400).
#define KERNAL_DEBUG_PORT_NAME			"Debug Port"
#define KERNAL_DEBUG_PORT_COMMAND		0x000
#define KERNAL_DEBUG_PORT_PROCESS		0x001
#define KERNAL_DEBUG_PORT_EXIT_LOW		0x002
#define KERNAL_DEBUG_PORT_EXIT_HIGH		0x003
#define KERNAL_DEBUG_PORT_ADDRESS		0x004	//3 bytes: where a driver was put.
#define KERNAL_DEBUG_PORT_SIZE			0x007	//3 bytes: how big it is.
#define KERNAL_DEBUG_PORT_NAME_BUFFER	0x100
#define KERNAL_DEBUG_PORT_NAME_SIZE		64
#define KERNAL_DEBUG_PORT_STARTED		1
#define KERNAL_DEBUG_PORT_ENDED			2
#define KERNAL_DEBUG_PORT_BANK			3
#define KERNAL_DEBUG_PORT_DRIVER		4		//A driver was loaded: PROCESS is its first board's slot, NAME its file.


//A device no real HCP65 has: the kernal tells it of each process started and ended, each main memory bank given to a
//process and each driver loaded from the SD card (where it is, before it runs), for a debugger to follow.  Only the
//kernal uses it: processes can't open it.
class CKernalDebugPort : public CKernalDriver
{
public:
	uint16	Open(uint16 uiUnit, uint8 uiProcess) override;

	void	Tell(uint8 uiCommand, uint8 uiProcess, uint16 uiExitCode, const char* szPath);
	void	GiveBank(uint8 uiPhysical);
	void	TellDriver(uint16 uiSlot, const char* szPath, uint32 uiAddress, uint32 uiSize);

protected:
	void	WriteName(const char* szPath);
};


extern CKernalDebugPort*	gpcKernalDebugPort;		//NULL without a debug port.


#endif // __KERNAL_DEBUG_PORT_H__
