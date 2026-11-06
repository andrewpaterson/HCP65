#ifndef __HCP65_DEBUG_PORT_H__
#define __HCP65_DEBUG_PORT_H__
#include "BaseLib/ArrayTemplate.h"
#include "EmulatorDebugLib/DebugTarget.h"
#include "HCP65Device.h"


#define HCP65_DEBUG_PORT_NAME			"Debug Port"
#define HCP65_DEBUG_PORT_COMMAND		0x400		//Write: HCP65_DEBUG_PORT_STARTED, HCP65_DEBUG_PORT_ENDED or HCP65_DEBUG_PORT_BANK.
#define HCP65_DEBUG_PORT_PROCESS		0x401		//The process's ID (HCP65_DEBUG_PORT_BANK: the bank).
#define HCP65_DEBUG_PORT_EXIT_LOW		0x402		//Its exit code, once it has ended.
#define HCP65_DEBUG_PORT_EXIT_HIGH		0x403
#define HCP65_DEBUG_PORT_NAME_BUFFER	0x500		//Its program's file, zero terminated ($0500 to $053F).
#define HCP65_DEBUG_PORT_STARTED		1
#define HCP65_DEBUG_PORT_ENDED			2
#define HCP65_DEBUG_PORT_BANK			3			//A main memory bank was given to a process: the emulator fills it in fill mode.


//A process started or ended, as the kernal told the debug port.
struct SHCP65DebugPortEvent
{
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;
};


typedef CArrayTemplate<SHCP65DebugPortEvent>	CArrayHCP65DebugPortEvents;


//The emulator's debug port, a device no real HCP65 has: the kernal tells it of each process it starts and ends (its
//ID, its program's file and its exit code) for a debugger to follow, and of each main memory bank it gives a process (for
//the emulator's fill mode, CHCP65::SetMemoryFill).  The kernal finds it by its name, as it finds
//any device; without one it tells nothing.  It keeps the processes running, and while they are watched (a debugger is
//connected) each event, and a process started stops the HCP65's Run (StopRun) before the process's first instruction
//so the debugger can give it its breakpoints first.
class CHCP65DebugPort : public CHCP65Device
{
protected:
	CDeviceROM				mcROM;
	uint8						muiProcess;
	uint16						muiExitCode;
	char						mszName[DEBUG_PROCESS_NAME_SIZE];
	bool						mabRunning[256];
	char						maszRunning[256][DEBUG_PROCESS_NAME_SIZE];
	bool						mbWatching;
	CArrayHCP65DebugPortEvents	masEvents;

public:
	void	Init(void);
	void	Kill(void) override;

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;

	size	NumProcesses(void);
	bool	GetProcess(size uiIndex, SDebugProcess* psDest);
	void	Watch(bool bWatch);
	bool	TakeEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest);

protected:
	void	Tell(uint8 uiCommand);
};


#endif // __HCP65_DEBUG_PORT_H__

