#include <string.h>
#include "HCP65DebugTarget.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugTarget::Init(CHCP65* pcHCP65)
{
	mpcHCP65 = pcHCP65;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugTarget::Kill(void)
{
	mpcHCP65 = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65DebugTarget::GetProcessorName(void) const	{ return "W65C816"; }
size CHCP65DebugTarget::NumAddressBits(void) const			{ return 24; }
size CHCP65DebugTarget::NumRegisters(void) const				{ return HCP65_DEBUG_NUM_REGISTERS; }


//////////////////////////////////////////////////////////////////////////
//
// The W65C816's registers, then the process ID (PID).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugTarget::GetRegister(size uiRegister, SDebugRegister* psDest)
{
	if (uiRegister == HCP65_DEBUG_REGISTER_PROCESS)
	{
		SetDebugRegister(psDest, "PID", 8, DRK_Value);
	}
	else
	{
		GetW65C816DebugRegister(uiRegister, psDest);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65DebugTarget::GetRegisterValue(size uiRegister)
{
	if (uiRegister == HCP65_DEBUG_REGISTER_PROCESS)
	{
		return mpcHCP65->GetProcessID();
	}
	return GetW65C816DebugRegisterValue(mpcHCP65->GetMPU(), uiRegister);
}


//////////////////////////////////////////////////////////////////////////
//
// The process ID can't be changed.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::SetRegisterValue(size uiRegister, uint64 uiValue)
{
	if (uiRegister == HCP65_DEBUG_REGISTER_PROCESS)
	{
		return false;
	}
	return SetW65C816DebugRegisterValue(mpcHCP65->GetMPU(), uiRegister, uiValue);
}
uint32 CHCP65DebugTarget::GetProgramAddress(void)		{ return mpcHCP65->GetMPU()->GetProgramAddress(); }
uint32 CHCP65DebugTarget::GetStackPointer(void) const			{ return mpcHCP65->GetMPU()->GetS(); }
CBreakpoints* CHCP65DebugTarget::GetBreakpoints(void)	{ return mpcHCP65->GetModeBreakpoints(true); }
size CHCP65DebugTarget::NumBreakpointSpaces(void) const		{ return 2; }
bool CHCP65DebugTarget::IsAtBreakpoint(void) const			{ return mpcHCP65->IsAtBreakpoint(); }
bool CHCP65DebugTarget::IsStopped(void) const					{ return mpcHCP65->IsStopped(); }
void CHCP65DebugTarget::Run(uint64 uiBudget)			{ mpcHCP65->Run(uiBudget); }
void CHCP65DebugTarget::Reset(void)						{ mpcHCP65->Reset(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugTarget::SetProgramAddress(uint32 uiAddress)
{
	mpcHCP65->GetMPU()->TestSetProgramCounter((uint8)(uiAddress >> 16), (uint16)uiAddress);
}


//////////////////////////////////////////////////////////////////////////
//
// One instruction, with the devices kept up with it: a Run of a cycle
// always does exactly one step.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::Step(void)
{
	mpcHCP65->Run(1);
	return !mpcHCP65->IsStopped() && !mpcHCP65->IsAtBreakpoint();
}


//////////////////////////////////////////////////////////////////////////
//
// In the mode the processor is in.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::PeekMemory(uint32 uiAddress, void* pvDest, size uiSize)
{
	return Peek(uiAddress, pvDest, uiSize, mpcHCP65->IsKernelMode());
}


//////////////////////////////////////////////////////////////////////////
//
// Kernel mode's memory (space 0) or user mode's (1), the process's whose
// ID the processor board holds (GetUserProcess).
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::PeekSpaceMemory(size uiSpace, uint32 uiAddress, void* pvDest, size uiSize)
{
	return Peek(uiAddress, pvDest, uiSize, uiSpace == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Device addresses read as $FF.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::Peek(uint32 uiAddress, void* pvDest, size uiSize, bool bKernel)
{
	size	i;
	uint8*	puiMemory;
	uint8*	puiDest;

	puiDest = (uint8*)pvDest;
	for (i = 0; i < uiSize; i++)
	{
		puiMemory = mpcHCP65->MemoryAt((uint32)(uiAddress + i), bKernel);
		puiDest[i] = puiMemory ? *puiMemory : 0xFF;
	}
	return uiSize;
}


//////////////////////////////////////////////////////////////////////////
//
// In the mode the processor is in.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::PokeMemory(uint32 uiAddress, void* pvSource, size uiSize)
{
	return Poke(uiAddress, pvSource, uiSize, mpcHCP65->IsKernelMode());
}


//////////////////////////////////////////////////////////////////////////
//
// Kernel mode's memory (space 0) or user mode's (1), as PeekSpaceMemory.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::PokeSpaceMemory(size uiSpace, uint32 uiAddress, void* pvSource, size uiSize)
{
	return Poke(uiAddress, pvSource, uiSize, uiSpace == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Stops at the first device address.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::Poke(uint32 uiAddress, void* pvSource, size uiSize, bool bKernel)
{
	size	i;
	uint8*	puiMemory;
	uint8*	puiSource;

	puiSource = (uint8*)pvSource;
	for (i = 0; i < uiSize; i++)
	{
		puiMemory = mpcHCP65->MemoryAt((uint32)(uiAddress + i), bKernel);
		if (!puiMemory)
		{
			break;
		}
		*puiMemory = puiSource[i];
	}
	return i;
}


CDebugDecoder* CHCP65DebugTarget::GetDecoder(void)				{ return &mcDecoder; }
EDebugAddressFormat CHCP65DebugTarget::GetAddressFormat(void) const	{ return DAF_Bank; }


//////////////////////////////////////////////////////////////////////////
//
// Kernel mode's breakpoints, then user mode's: the kernal and a process
// both have code from bank 1.
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65DebugTarget::GetBreakpointSpaceName(size uiSpace)
{
	return (uiSpace == 0) ? "Kernel" : ((uiSpace == 1) ? "User" : "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CBreakpoints* CHCP65DebugTarget::GetSpaceBreakpoints(size uiSpace)
{
	return (uiSpace < 2) ? mpcHCP65->GetModeBreakpoints(uiSpace == 0) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// User mode's breakpoints for one process; kernel mode's are every
// process's (the kernal is the same in each).
//
//////////////////////////////////////////////////////////////////////////
CBreakpoints* CHCP65DebugTarget::GetProcessBreakpoints(size uiSpace, uint32 uiProcess)
{
	if ((uiSpace != 1) || (uiProcess >= HCP65_NUM_PROCESSES))
	{
		return NULL;
	}
	return mpcHCP65->GetProcessBreakpoints((uint8)uiProcess);
}


//////////////////////////////////////////////////////////////////////////
//
// Kernel mode is space 1 (its index plus one) and every process's; user
// mode is space 2, in the process the kernal says is running.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65DebugTarget::GetCurrentSpace(void) const		{ return mpcHCP65->IsKernelMode() ? 1 : 2; }
uint32 CHCP65DebugTarget::GetCurrentProcess(void) const	{ return mpcHCP65->IsKernelMode() ? DEBUG_PROCESS_ANY : mpcHCP65->GetProcessID(); }


//////////////////////////////////////////////////////////////////////////
//
// The process whose ID the processor board holds, which user mode's memory
// is (the last run: a kernal call's or interrupt's), if the kernal has
// said it is running.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65DebugTarget::GetUserProcess(void) const
{
	CHCP65DebugPort*	pcPort;
	SDebugProcess		sProcess;
	size				ui;

	pcPort = mpcHCP65->GetDebugPort();
	for (ui = 0; ui < pcPort->NumProcesses(); ui++)
	{
		if (pcPort->GetProcess(ui, &sProcess) && (sProcess.uiProcess == mpcHCP65->GetProcessID()))
		{
			return sProcess.uiProcess;
		}
	}
	return DEBUG_PROCESS_ANY;
}


void CHCP65DebugTarget::ClearBreakpoints(void)					{ mpcHCP65->ClearBreakpoints(); }
void CHCP65DebugTarget::EnableAllBreakpoints(bool bEnabled)		{ mpcHCP65->EnableAllBreakpoints(bEnabled); }


//////////////////////////////////////////////////////////////////////////
//
// The kernal's processes, as it tells the debug port of them.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::HasProcesses(void) const								{ return true; }
size CHCP65DebugTarget::NumProcesses(void) const								{ return mpcHCP65->GetDebugPort()->NumProcesses(); }
bool CHCP65DebugTarget::GetProcess(size uiIndex, SDebugProcess* psDest)	{ return mpcHCP65->GetDebugPort()->GetProcess(uiIndex, psDest); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugTarget::WatchEvents(bool bWatch)
{
	mpcHCP65->GetDebugPort()->Watch(bWatch);
}


//////////////////////////////////////////////////////////////////////////
//
// A process that has ended loses its breakpoints: its ID may be given to
// the next.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::TakeProcessEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest)
{
	if (!mpcHCP65->GetDebugPort()->TakeEvent(peEvent, psDest))
	{
		return false;
	}
	if ((*peEvent == DPE_Ended) && (psDest->uiProcess < HCP65_NUM_PROCESSES))
	{
		mpcHCP65->ClearProcessBreakpoints((uint8)psDest->uiProcess);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A driver the kernal loaded as the debugger sees it: its file on the
// card, where it is in MPU RAM and how big.
//
//////////////////////////////////////////////////////////////////////////
static void HCP65DebugModule(SHCP65DebugDriver* psDriver, SDebugModule* psDest)
{
	psDest->uiAddress = psDriver->uiAddress;
	psDest->uiSize = psDriver->uiSize;
	memcpy(psDest->szName, psDriver->szName, DEBUG_MODULE_NAME_SIZE);
}


bool CHCP65DebugTarget::HasModules(void) const								{ return true; }
size CHCP65DebugTarget::NumModules(void) const								{ return mpcHCP65->GetDebugPort()->NumDrivers(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::GetModule(size uiIndex, SDebugModule* psDest)
{
	CHCP65DebugPort*	pcPort;

	pcPort = mpcHCP65->GetDebugPort();
	if (uiIndex >= pcPort->NumDrivers())
	{
		return false;
	}
	HCP65DebugModule(pcPort->GetDriver(uiIndex), psDest);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The drivers dropped by a reset, then each one loaded since.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::TakeModuleEvent(EDebugModuleEvent* peEvent, SDebugModule* psDest)
{
	SHCP65DebugDriver	sDriver;
	CHCP65DebugPort*	pcPort;

	pcPort = mpcHCP65->GetDebugPort();
	if (pcPort->TakeDriversCleared())
	{
		*peEvent = DME_Cleared;
		return true;
	}
	if (!pcPort->TakeDriverEvent(&sDriver))
	{
		return false;
	}
	*peEvent = DME_Loaded;
	HCP65DebugModule(&sDriver, psDest);
	return true;
}

