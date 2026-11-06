#include "KernalDebugPort.h"


CKernalDebugPort*	gpcKernalDebugPort;


//////////////////////////////////////////////////////////////////////////
//
// The kernal's own.
//
//////////////////////////////////////////////////////////////////////////
uint16 CKernalDebugPort::Open(uint16 uiUnit, uint8 uiProcess)
{
	return KERNAL_DRIVER_NOT_SUPPORTED;
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess, running szPath, has started (KERNAL_DEBUG_PORT_STARTED) or
// ended with uiExitCode (KERNAL_DEBUG_PORT_ENDED).
//
//////////////////////////////////////////////////////////////////////////
void CKernalDebugPort::Tell(uint8 uiCommand, uint8 uiProcess, uint16 uiExitCode, const char* szPath)
{
	mpuiRegisters[KERNAL_DEBUG_PORT_PROCESS] = uiProcess;
	mpuiRegisters[KERNAL_DEBUG_PORT_EXIT_LOW] = (uint8)uiExitCode;
	mpuiRegisters[KERNAL_DEBUG_PORT_EXIT_HIGH] = (uint8)(uiExitCode >> 8);
	WriteName(szPath);
	mpuiRegisters[KERNAL_DEBUG_PORT_COMMAND] = uiCommand;
}


//////////////////////////////////////////////////////////////////////////
//
// The driver szPath was put at uiAddress (uiSize bytes, relocated, not run
// yet) for the board in uiSlot: a debugger can bind its breakpoints in it
// before its constructors run.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDebugPort::TellDriver(uint16 uiSlot, const char* szPath, uint32 uiAddress, uint32 uiSize)
{
	mpuiRegisters[KERNAL_DEBUG_PORT_PROCESS] = (uint8)uiSlot;
	mpuiRegisters[KERNAL_DEBUG_PORT_ADDRESS] = (uint8)uiAddress;
	mpuiRegisters[KERNAL_DEBUG_PORT_ADDRESS + 1] = (uint8)(uiAddress >> 8);
	mpuiRegisters[KERNAL_DEBUG_PORT_ADDRESS + 2] = (uint8)(uiAddress >> 16);
	mpuiRegisters[KERNAL_DEBUG_PORT_SIZE] = (uint8)uiSize;
	mpuiRegisters[KERNAL_DEBUG_PORT_SIZE + 1] = (uint8)(uiSize >> 8);
	mpuiRegisters[KERNAL_DEBUG_PORT_SIZE + 2] = (uint8)(uiSize >> 16);
	WriteName(szPath);
	mpuiRegisters[KERNAL_DEBUG_PORT_COMMAND] = KERNAL_DEBUG_PORT_DRIVER;
}


//////////////////////////////////////////////////////////////////////////
//
// szPath into the name buffer, cut short if it doesn't fit.
//
//////////////////////////////////////////////////////////////////////////
void CKernalDebugPort::WriteName(const char* szPath)
{
	uint16	i;

	for (i = 0; (i < KERNAL_DEBUG_PORT_NAME_SIZE - 1) && (szPath[i] != '\0'); i++)
	{
		mpuiRegisters[KERNAL_DEBUG_PORT_NAME_BUFFER + i] = (uint8)szPath[i];
	}
	mpuiRegisters[KERNAL_DEBUG_PORT_NAME_BUFFER + i] = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The main memory bank uiPhysical was given to a process: the emulator
// fills it in its fill mode.  The kernal doesn't clear the banks it gives
// (a program's start up clears its bss).
//
//////////////////////////////////////////////////////////////////////////
void CKernalDebugPort::GiveBank(uint8 uiPhysical)
{
	mpuiRegisters[KERNAL_DEBUG_PORT_PROCESS] = uiPhysical;
	mpuiRegisters[KERNAL_DEBUG_PORT_COMMAND] = KERNAL_DEBUG_PORT_BANK;
}
