#ifndef __HCP65_DEBUG_TARGET_H__
#define __HCP65_DEBUG_TARGET_H__
#include "EmulatorDebugLib/DebugTarget.h"
#include "EmulatorDebugLib/W65C816DebugRegisters.h"
#include "Asm65816DebugDecoder.h"
#include "HCP65.h"
#include "HCP65DebugPort.h"


//After the W65C816's registers: the process the MMU maps user mode for, read only (changing it would move the running
//process's memory from under it).
#define HCP65_DEBUG_REGISTER_PROCESS	D816R_NumRegisters
#define HCP65_DEBUG_NUM_REGISTERS		(D816R_NumRegisters + 1)


//The HCP65 as a CDebugServer sees it: its W65C816 and the memory map it sees now.  Run's budget is CPU cycles,
//as CHCP65::Run's.  Memory is read and written without touching devices: device addresses read as $FF and a
//write stops at the first one.  Breakpoints are the processor's addresses, in whichever process is running.  Its
//instructions are decoded by what the W65C816's assembler knows (CAsm65816DebugDecoder).  The kernal's processes
//and the drivers it loads (its modules) are what it tells the debug port.
class CHCP65DebugTarget : public CDebugTarget
{
protected:
	CHCP65*					mpcHCP65;
	CHCP65DebugPort*		mpcPort;		//Where the kernal tells of its processes, NULL for none.
	CAsm65816DebugDecoder	mcDecoder;

public:
	void			Init(CHCP65* pcHCP65, CHCP65DebugPort* pcPort = NULL);
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
	size			NumBreakpointSpaces(void) override;
	const char*		GetBreakpointSpaceName(size uiSpace) override;
	CBreakpoints*	GetSpaceBreakpoints(size uiSpace) override;
	CBreakpoints*	GetProcessBreakpoints(size uiSpace, uint32 uiProcess) override;
	void			ClearBreakpoints(void) override;
	void			EnableAllBreakpoints(bool bEnabled) override;

	bool			HasProcesses(void) override;
	size			NumProcesses(void) override;
	bool			GetProcess(size uiIndex, SDebugProcess* psDest) override;
	void			WatchEvents(bool bWatch) override;
	bool			TakeProcessEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest) override;
	bool			HasModules(void) override;
	size			NumModules(void) override;
	bool			GetModule(size uiIndex, SDebugModule* psDest) override;
	bool			TakeModuleEvent(EDebugModuleEvent* peEvent, SDebugModule* psDest) override;
	uint32			GetCurrentSpace(void) override;
	uint32			GetCurrentProcess(void) override;
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
