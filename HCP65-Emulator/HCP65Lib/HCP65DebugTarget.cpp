#include "HCP65DebugTarget.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugTarget::Init(CHCP65* pcHCP65, CHCP65DebugPort* pcPort)
{
	mpcHCP65 = pcHCP65;
	mpcPort = pcPort;
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
const char* CHCP65DebugTarget::GetProcessorName(void)	{ return "W65C816"; }
size CHCP65DebugTarget::NumAddressBits(void)			{ return 24; }
size CHCP65DebugTarget::NumRegisters(void)				{ return HCP65_DEBUG_NUM_REGISTERS; }


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
		Get65C816DebugRegister(uiRegister, psDest);
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
	return Get65C816DebugRegisterValue(mpcHCP65->GetMPU(), uiRegister);
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
	return Set65C816DebugRegisterValue(mpcHCP65->GetMPU(), uiRegister, uiValue);
}
uint32 CHCP65DebugTarget::GetProgramAddress(void)		{ return mpcHCP65->GetMPU()->GetProgramAddress(); }
uint32 CHCP65DebugTarget::GetStackPointer(void)			{ return mpcHCP65->GetMPU()->GetS(); }
CBreakpoints* CHCP65DebugTarget::GetBreakpoints(void)	{ return mpcHCP65->GetModeBreakpoints(true); }
size CHCP65DebugTarget::NumBreakpointSpaces(void)		{ return 2; }
bool CHCP65DebugTarget::IsAtBreakpoint(void)			{ return mpcHCP65->IsAtBreakpoint(); }
bool CHCP65DebugTarget::IsStopped(void)					{ return mpcHCP65->IsStopped(); }
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
// Device addresses read as $FF.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::PeekMemory(uint32 uiAddress, void* pvDest, size uiSize)
{
	size	i;
	uint8*	puiMemory;
	uint8*	puiDest;

	puiDest = (uint8*)pvDest;
	for (i = 0; i < uiSize; i++)
	{
		puiMemory = mpcHCP65->MemoryAt((uint32)(uiAddress + i));
		puiDest[i] = puiMemory ? *puiMemory : 0xFF;
	}
	return uiSize;
}


//////////////////////////////////////////////////////////////////////////
//
// Stops at the first device address.
//
//////////////////////////////////////////////////////////////////////////
size CHCP65DebugTarget::PokeMemory(uint32 uiAddress, void* pvSource, size uiSize)
{
	size	i;
	uint8*	puiMemory;
	uint8*	puiSource;

	puiSource = (uint8*)pvSource;
	for (i = 0; i < uiSize; i++)
	{
		puiMemory = mpcHCP65->MemoryAt((uint32)(uiAddress + i));
		if (!puiMemory)
		{
			break;
		}
		*puiMemory = puiSource[i];
	}
	return i;
}


CDebugDecoder* CHCP65DebugTarget::GetDecoder(void)				{ return &mcDecoder; }
EDebugAddressFormat CHCP65DebugTarget::GetAddressFormat(void)	{ return DAF_Bank; }


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
uint32 CHCP65DebugTarget::GetCurrentSpace(void)		{ return mpcHCP65->IsKernelMode() ? 1 : 2; }
uint32 CHCP65DebugTarget::GetCurrentProcess(void)	{ return mpcHCP65->IsKernelMode() ? DEBUG_PROCESS_ANY : mpcHCP65->GetProcessID(); }


void CHCP65DebugTarget::ClearBreakpoints(void)					{ mpcHCP65->ClearBreakpoints(); }
void CHCP65DebugTarget::EnableAllBreakpoints(bool bEnabled)		{ mpcHCP65->EnableAllBreakpoints(bEnabled); }


//////////////////////////////////////////////////////////////////////////
//
// The kernal's processes, as it tells the debug port of them (none
// without one).
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::HasProcesses(void)								{ return mpcPort != NULL; }
size CHCP65DebugTarget::NumProcesses(void)								{ return mpcPort ? mpcPort->NumProcesses() : 0; }
bool CHCP65DebugTarget::GetProcess(size uiIndex, SDebugProcess* psDest)	{ return mpcPort && mpcPort->GetProcess(uiIndex, psDest); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DebugTarget::WatchProcesses(bool bWatch)
{
	if (mpcPort)
	{
		mpcPort->Watch(bWatch);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A process that has ended loses its breakpoints: its ID may be given to
// the next.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DebugTarget::TakeProcessEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest)
{
	if (!mpcPort || !mpcPort->TakeEvent(peEvent, psDest))
	{
		return false;
	}
	if ((*peEvent == DPE_Ended) && (psDest->uiProcess < HCP65_NUM_PROCESSES))
	{
		mpcHCP65->ClearProcessBreakpoints((uint8)psDest->uiProcess);
	}
	return true;
}

