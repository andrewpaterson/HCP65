#ifndef __HCP65_DEBUG_TARGET_H__
#define __HCP65_DEBUG_TARGET_H__
#include "EmulatorDebugLib/DebugTarget.h"
#include "EmulatorDebugLib/W65C816DebugRegisters.h"
#include "AsmW65C816DebugDecoder.h"
#include "HCP65.h"


//After the W65C816's registers: the process the MMU maps user mode for, read only (changing it would move the running
//process's memory from under it).
#define HCP65_DEBUG_REGISTER_PROCESS	DW65C816R_NumRegisters
#define HCP65_DEBUG_NUM_REGISTERS		(DW65C816R_NumRegisters + 1)


//The HCP65 as a CDebugServer sees it: its W65C816 and the memory map it sees now.  Run's budget is CPU cycles,
//as CHCP65::Run's.  Memory is read and written without touching devices: device addresses read as $FF and a
//write stops at the first one.  Breakpoints are the processor's addresses, in whichever process is running.  Its
//instructions are decoded by what the W65C816's assembler knows (CAsmW65C816DebugDecoder).  The kernal's processes
//and the drivers it loads (its modules) are what it tells the debug port.
class CHCP65DebugTarget : public CDebugTarget
{
protected:
	CHCP65*					mpcHCP65;
	CAsmW65C816DebugDecoder	mcDecoder;

public:
	void			Init(CHCP65* pcHCP65);
	void			Kill(void);

	const char*		GetProcessorName(void) const override;
	size			NumAddressBits(void) const override;
	EDebugAddressFormat	GetAddressFormat(void) const override;
	size			NumRegisters(void) const override;
	void			GetRegister(size uiRegister, SDebugRegister* psDest) override;
	uint64			GetRegisterValue(size uiRegister) override;
	bool			SetRegisterValue(size uiRegister, uint64 uiValue) override;

	uint32			GetProgramAddress(void) override;
	void			SetProgramAddress(uint32 uiAddress) override;
	uint32			GetStackPointer(void) const override;

	CBreakpoints*	GetBreakpoints(void) override;
	size			NumBreakpointSpaces(void) const override;
	const char*		GetBreakpointSpaceName(size uiSpace) override;
	CBreakpoints*	GetSpaceBreakpoints(size uiSpace) override;
	CBreakpoints*	GetProcessBreakpoints(size uiSpace, uint32 uiProcess) override;
	void			ClearBreakpoints(void) override;
	void			EnableAllBreakpoints(bool bEnabled) override;

	bool			HasProcesses(void) const override;
	size			NumProcesses(void) const override;
	bool			GetProcess(size uiIndex, SDebugProcess* psDest) override;
	void			WatchEvents(bool bWatch) override;
	bool			TakeProcessEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest) override;
	bool			HasModules(void) const override;
	size			NumModules(void) const override;
	bool			GetModule(size uiIndex, SDebugModule* psDest) override;
	bool			TakeModuleEvent(EDebugModuleEvent* peEvent, SDebugModule* psDest) override;
	uint32			GetCurrentSpace(void) const override;
	uint32			GetCurrentProcess(void) const override;
	uint32			GetUserProcess(void) const override;
	bool			IsAtBreakpoint(void) const override;
	bool			IsStopped(void) const override;

	void			Run(uint64 uiBudget) override;
	bool			Step(void) override;
	void			Reset(void) override;

	size			PeekMemory(uint32 uiAddress, void* pvDest, size uiSize) override;
	size			PokeMemory(uint32 uiAddress, void* pvSource, size uiSize) override;
	size			PeekSpaceMemory(size uiSpace, uint32 uiAddress, void* pvDest, size uiSize) override;
	size			PokeSpaceMemory(size uiSpace, uint32 uiAddress, void* pvSource, size uiSize) override;

protected:
	size			Peek(uint32 uiAddress, void* pvDest, size uiSize, bool bKernel);
	size			Poke(uint32 uiAddress, void* pvSource, size uiSize, bool bKernel);
	CDebugDecoder*	GetDecoder(void) override;
};


#endif // __HCP65_DEBUG_TARGET_H__
