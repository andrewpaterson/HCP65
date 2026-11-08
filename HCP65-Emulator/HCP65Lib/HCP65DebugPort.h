#ifndef __HCP65_DEBUG_PORT_H__
#define __HCP65_DEBUG_PORT_H__
#include "BaseLib/ArrayTemplate.h"
#include "EmulatorDebugLib/DebugTarget.h"
#include "DebugLib/HCP65DebugMessage.h"
#include "HCP65Registers.h"


class CHCP65;


//A process started or ended, as the kernal told the debug port.
struct SHCP65DebugPortEvent
{
	EDebugProcessEvent	eEvent;
	SDebugProcess		sProcess;
};
typedef CArrayTemplate<SHCP65DebugPortEvent>	CArrayHCP65DebugPortEvents;


//A driver the kernal loaded: its file, where it is in MPU RAM and the slot of the first board it drives.
struct SHCP65DebugDriver
{
	uint8	uiSlot;
	uint32	uiAddress;
	uint32	uiSize;
	char	szName[DEBUG_MODULE_NAME_SIZE];
};
typedef CArrayTemplate<SHCP65DebugDriver>	CArrayHCP65DebugDrivers;


//What the kernal tells the emulator with WDM (DebugLib/HCP65DebugMessage.h): each process it starts and ends (its ID, its
//program's file and its exit code) for a debugger to follow, and each main memory bank it gives a process (for the
//emulator's fill mode, CHCP65::SetMemoryFill).  It isn't a board: the CHCP65 has one (GetDebugPort) and gives it the
//messages and its resets.  It keeps the processes running, and while they are watched (a debugger is connected) each
//is an event, and a process started stops the HCP65's Run (StopRun) before the process's first instruction so the
//debugger can give it its breakpoints first.  It keeps the drivers the kernal loaded since the last reset, and while
//watched a driver loaded is an event too (TakeDriverEvent) and stops the Run before the driver's code runs, and a reset
//that drops drivers is one (TakeDriversCleared).
class CHCP65DebugPort
{
protected:
	CHCP65*						mpcHCP65;
	bool						mabRunning[HCP65_NUM_PROCESSES];
	char						maszRunning[HCP65_NUM_PROCESSES][DEBUG_PROCESS_NAME_SIZE];
	bool						mbWatching;
	CArrayHCP65DebugPortEvents	masEvents;
	CArrayHCP65DebugDrivers		masDrivers;
	CArrayHCP65DebugDrivers		masDriverEvents;
	bool						mbDriversCleared;

public:
	void				Init(CHCP65* pcHCP65);
	void				Kill(void);

	void				Tell(SHCP65DebugMessage* psMessage);
	void				Reset(void);

	size				NumProcesses(void);
	bool				GetProcess(size uiIndex, SDebugProcess* psDest);
	void				Watch(bool bWatch);
	bool				TakeEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest);
	size				NumDrivers(void);
	SHCP65DebugDriver*	GetDriver(size uiIndex);
	bool				TakeDriverEvent(SHCP65DebugDriver* psDest);
	bool				TakeDriversCleared(void);

protected:
	void				TellProcess(SHCP65DebugMessage* psMessage);
	void				TellDriver(SHCP65DebugMessage* psMessage);
};


#endif // __HCP65_DEBUG_PORT_H__
