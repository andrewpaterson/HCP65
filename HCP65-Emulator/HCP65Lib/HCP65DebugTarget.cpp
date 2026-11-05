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
const char* CHCP65DebugTarget::GetProcessorName(void)	{ return "W65C816"; }
size CHCP65DebugTarget::NumAddressBits(void)			{ return 24; }
size CHCP65DebugTarget::NumRegisters(void)				{ return D816R_NumRegisters; }
void CHCP65DebugTarget::GetRegister(size uiRegister, SDebugRegister* psDest)	{ Get65C816DebugRegister(uiRegister, psDest); }
uint64 CHCP65DebugTarget::GetRegisterValue(size uiRegister)	{ return Get65C816DebugRegisterValue(mpcHCP65->GetMPU(), uiRegister); }
bool CHCP65DebugTarget::SetRegisterValue(size uiRegister, uint64 uiValue)	{ return Set65C816DebugRegisterValue(mpcHCP65->GetMPU(), uiRegister, uiValue); }
uint32 CHCP65DebugTarget::GetProgramAddress(void)		{ return mpcHCP65->GetMPU()->GetProgramAddress(); }
uint32 CHCP65DebugTarget::GetStackPointer(void)			{ return mpcHCP65->GetMPU()->GetS(); }
CBreakpoints* CHCP65DebugTarget::GetBreakpoints(void)	{ return mpcHCP65->GetMPU()->GetBreakpoints(); }
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


//////////////////////////////////////////////////////////////////////////
//
// The opcode as it is now in the memory map (a device's address is $FF,
// not a call).
//
//////////////////////////////////////////////////////////////////////////
EDebugInstruction CHCP65DebugTarget::GetInstruction(uint32 uiAddress, size* puiCallLength)
{
	uint8	uiOpcode;

	PeekMemory(uiAddress, &uiOpcode, 1);
	return Get65C816Instruction(uiOpcode, puiCallLength);
}
