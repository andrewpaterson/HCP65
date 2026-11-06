#ifndef __HCP65_DEBUG_PORT_H__
#define __HCP65_DEBUG_PORT_H__
#include "BaseLib/ArrayTemplate.h"
#include "EmulatorDebugLib/DebugTarget.h"
#include "HCP65Device.h"


#define HCP65_DEBUG_PORT_NAME			"Debug Port"
#define HCP65_DEBUG_PORT_COMMAND		0x400		//Write: HCP65_DEBUG_PORT_STARTED ... HCP65_DEBUG_PORT_DRIVER.
#define HCP65_DEBUG_PORT_PROCESS		0x401		//The process's ID (HCP65_DEBUG_PORT_BANK: the bank; _DRIVER: the board's slot).
#define HCP65_DEBUG_PORT_EXIT_LOW		0x402		//Its exit code, once it has ended.
#define HCP65_DEBUG_PORT_EXIT_HIGH		0x403
#define HCP65_DEBUG_PORT_ADDRESS		0x404		//3 bytes: where a driver was put.
#define HCP65_DEBUG_PORT_SIZE			0x407		//3 bytes: how big it is.
#define HCP65_DEBUG_PORT_NAME_BUFFER	0x500		//Its program's (or driver's) file, zero terminated ($0500 to $053F).
#define HCP65_DEBUG_PORT_STARTED		1
#define HCP65_DEBUG_PORT_ENDED			2
#define HCP65_DEBUG_PORT_BANK			3			//A main memory bank was given to a process: the emulator fills it in fill mode.
#define HCP65_DEBUG_PORT_DRIVER			4			//A driver was loaded from the SD card, relocated but not run yet.


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
	char	szName[DEBUG_PROCESS_NAME_SIZE];
};


typedef CArrayTemplate<SHCP65DebugDriver>	CArrayHCP65DebugDrivers;


//The emulator's debug port, a device no real HCP65 has: the kernal tells it of each process it starts and ends (its
//ID, its program's file and its exit code) for a debugger to follow, and of each main memory bank it gives a process (for
//the emulator's fill mode, CHCP65::SetMemoryFill).  The kernal finds it by its name, as it finds
//any device; without one it tells nothing.  It keeps the processes running, and while they are watched (a debugger is
//connected) each event, and a process started stops the HCP65's Run (StopRun) before the process's first instruction
//so the debugger can give it its breakpoints first.  It keeps the drivers the kernal loaded since the last reset, and
//while watched a driver loaded is an event too (TakeDriverEvent) and stops the Run before the driver's code runs.
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
	uint32						muiAddress;
	uint32						muiSize;
	CArrayHCP65DebugDrivers		masDrivers;
	CArrayHCP65DebugDrivers		masDriverEvents;

public:
	void	Init(void);
	void	Kill(void) override;

	uint8	Read(uint32 uiOffset) override;
	void	Write(uint32 uiOffset, uint8 uiValue) override;
	void	Reset(void) override;

	size	NumProcesses(void);
	bool	GetProcess(size uiIndex, SDebugProcess* psDest);
	void	Watch(bool bWatch);
	bool	TakeEvent(EDebugProcessEvent* peEvent, SDebugProcess* psDest);
	size	NumDrivers(void);
	SHCP65DebugDriver*	GetDriver(size uiIndex);
	bool	TakeDriverEvent(SHCP65DebugDriver* psDest);

protected:
	void	Tell(uint8 uiCommand);
	void	TellDriver(void);
	uint32	SetByte(uint32 uiOld, uint32 uiByte, uint8 uiValue);
};


#endif // __HCP65_DEBUG_PORT_H__

