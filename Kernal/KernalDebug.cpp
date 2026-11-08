#include "DebugLib/HCP65DebugMessage.h"
#include "KernalDebug.h"


SHCP65DebugMessage	gsKernalDebugMessage;	//Only read during the WDM that tells of it.


extern "C" void		KernalDebugTell(SHCP65DebugMessage* psMessage);		//KernalDebug.asm: the WDM.


//////////////////////////////////////////////////////////////////////////
//
// szPath into the message, cut short if it doesn't fit.
//
//////////////////////////////////////////////////////////////////////////
static void KernalDebugSetName(const char* szPath)
{
	uint16	i;

	for (i = 0; (i < HCP65_DEBUG_NAME_SIZE - 1) && (szPath[i] != '\0'); i++)
	{
		gsKernalDebugMessage.szName[i] = szPath[i];
	}
	gsKernalDebugMessage.szName[i] = '\0';
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess has started running szPath.
//
//////////////////////////////////////////////////////////////////////////
void KernalDebugProcessStarted(uint8 uiProcess, const char* szPath)
{
	gsKernalDebugMessage.uiCommand = HCP65_DEBUG_STARTED;
	gsKernalDebugMessage.uiValue = uiProcess;
	gsKernalDebugMessage.uiExitCode = 0;
	KernalDebugSetName(szPath);
	KernalDebugTell(&gsKernalDebugMessage);
}


//////////////////////////////////////////////////////////////////////////
//
// uiProcess has ended with uiExitCode.
//
//////////////////////////////////////////////////////////////////////////
void KernalDebugProcessEnded(uint8 uiProcess, uint16 uiExitCode)
{
	gsKernalDebugMessage.uiCommand = HCP65_DEBUG_ENDED;
	gsKernalDebugMessage.uiValue = uiProcess;
	gsKernalDebugMessage.uiExitCode = uiExitCode;
	KernalDebugTell(&gsKernalDebugMessage);
}


//////////////////////////////////////////////////////////////////////////
//
// The main memory bank uiPhysical was given to a process: the emulator
// fills it in its fill mode.  The kernal doesn't clear the banks it gives
// (a program's start up clears its bss).
//
//////////////////////////////////////////////////////////////////////////
void KernalDebugBank(uint8 uiPhysical)
{
	gsKernalDebugMessage.uiCommand = HCP65_DEBUG_BANK;
	gsKernalDebugMessage.uiValue = uiPhysical;
	KernalDebugTell(&gsKernalDebugMessage);
}


//////////////////////////////////////////////////////////////////////////
//
// The driver szPath was put at uiAddress (uiSize bytes, relocated, not run
// yet) for the board in uiSlot: a debugger can bind its breakpoints in it
// before its constructors run.
//
//////////////////////////////////////////////////////////////////////////
void KernalDebugDriver(uint16 uiSlot, const char* szPath, uint32 uiAddress, uint32 uiSize)
{
	gsKernalDebugMessage.uiCommand = HCP65_DEBUG_DRIVER;
	gsKernalDebugMessage.uiValue = (uint8)uiSlot;
	gsKernalDebugMessage.uiAddress = uiAddress;
	gsKernalDebugMessage.uiSize = uiSize;
	KernalDebugSetName(szPath);
	KernalDebugTell(&gsKernalDebugMessage);
}
