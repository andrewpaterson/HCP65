#ifndef __HCP65_DEBUG_TARGET_H__
#define __HCP65_DEBUG_TARGET_H__
#include "EmulatorDebugLib/DebugTarget.h"
#include "EmulatorDebugLib/W65C816DebugRegisters.h"
#include "Asm65816DebugDecoder.h"
#include "HCP65.h"


//The HCP65 as a CDebugServer sees it: its W65C816 and the memory map it sees now.  Run's budget is CPU cycles,
//as CHCP65::Run's.  Memory is read and written without touching devices: device addresses read as $FF and a
//write stops at the first one.  Breakpoints are the processor's addresses, in whichever process is running.  Its
//instructions are decoded by what the W65C816's assembler knows (CAsm65816DebugDecoder).
class CHCP65DebugTarget : public CDebugTarget
{
protected:
	CHCP65*					mpcHCP65;
	CAsm65816DebugDecoder	mcDecoder;

public:
	void			Init(CHCP65* pcHCP65);
	void			Kill(void);

	const char*		GetProcessorName(void) override;
	size			NumAddressBits(void) override;
	EDebugAddressFormat	GetAddressFormat(void) override;
	size			NumRegisters(void) override;
	void			GetRegister(size uiRegister, SDebugRegister* psDest) override;
	uint64			GetRegisterValue(size uiRegister) override;
	bool			SetRegisterValue(size uiRegister, uint64 uiValue) override;

	uint32			GetProgramAddress(void) override;
	void			SetProgramAddress(uint32 uiAddress) override;
	uint32			GetStackPointer(void) override;

	CBreakpoints*	GetBreakpoints(void) override;
	bool			IsAtBreakpoint(void) override;
	bool			IsStopped(void) override;

	void			Run(uint64 uiBudget) override;
	bool			Step(void) override;
	void			Reset(void) override;

	size			PeekMemory(uint32 uiAddress, void* pvDest, size uiSize) override;
	size			PokeMemory(uint32 uiAddress, void* pvSource, size uiSize) override;
	CDebugDecoder*	GetDecoder(void) override;
};


#endif // __HCP65_DEBUG_TARGET_H__
