#ifndef __HCP65_H__
#define __HCP65_H__
#include "BaseLib/PrimitiveTypes.h"
#include "MicroprocessorLib/Fast65C816.h"
#include "HCP65Device.h"
#include "HCP65Registers.h"


#define HCP65_MPU_RAM_SIZE			0x80000		//512KB.
#define HCP65_MPU_ROM_SIZE			0x80000		//512KB.
#define HCP65_MAIN_MEMORY_SIZE		0x1000000	//16MB.
#define HCP65_REMAP_TABLE			0x70000		//In MPU RAM: a 256 byte page of physical banks per process.
#define HCP65_NUM_DEVICE_SLOTS		20			//0 to 19; 8 to 15 do not exist.
#define HCP65_NUM_PROCESSES			256			//Process IDs 0 to $FF.
#define HCP65_CRYSTAL_RATE			32768		//The timer and sound crystal, in Hz.
#define HCP65_DEFAULT_CLOCK_RATE	25000000	//CPU cycles per second: the board's MPU clock (an LTC6903 set to 25MHz).
#define HCP65_DEBUG_FILL_BYTE		0xCC		//What memory nothing has written holds in fill mode.


//The HCP65 computer: a CFast65C816, the processor board's memory and registers, and the devices in its
//sockets.  See the HCP65 Computer pages of the documentation for the memory map.
//
//Kernel mode is a non-zero interrupt depth (counted up on each vector pull and down on each RTI), or the
//processor not yet having been in native mode.  In kernel mode the bottom 2MB is MPU RAM, MPU ROM and the
//devices, and main memory starts at $200000.  In user mode every address is main memory with its bank
//replaced from the remap table.  Until the processor first goes native, MPU RAM and ROM are swapped so
//that it boots from ROM.
//
//When the processor is in kernel mode and has been native, MPU RAM is the CFast65C816's flat block and
//costs nothing extra.  Everything else goes through the callbacks.
//
//Main memory is one flat 16MB here; the main memory channel (device 1) and memory boards are not modelled.
//The MPU ROM is flash on the real board; writes to it are ignored.
//
//Breakpoints are kept for each mode (GetModeBreakpoints): kernel mode and user mode see different memory at the same
//address, so the MPU stops only at those of the mode it is in.  User mode's can be for one process too
//(GetProcessBreakpoints): the MPU stops at those only while that process is running in user mode.
//
//StopRun (from a device, during an instruction) makes Run return after the instruction.
//
//Main memory contention: there is no arbiter on the main memory bus.  A device using it (the SD Card Board's DMA)
//says until when (SetMainMemoryBusy); a processor access to main memory before then latches Contention, as the
//mainboard's Main Memory Abort and Small Enable boards do: main memory is shut off (reads float to $FF, writes are
//lost) and NMI is held low until reset.
//
//Fill mode (a debugging aid; SetMemoryFill): main memory and MPU RAM are filled with HCP65_DEBUG_FILL_BYTE at reset,
//and each bank the kernal gives a process (it tells the emulator's debug port) is filled again, by the emulator, not by
//the processor, so a program that reads memory it never wrote sees $CC rather than a lucky zero.
class CHCP65
{
protected:
	CFast65C816			mcMPU;
	uint8*				mpuiRAM;
	uint8*				mpuiROM;
	uint8*				mpuiMain;
	CHCP65Registers		mcRegisters;
	CHCP65Device*		mapcDevices[HCP65_NUM_DEVICE_SLOTS];
	CBreakpoints		mcKernelBreakpoints;
	CBreakpoints		mcUserBreakpoints;
	CBreakpoints*		mapcProcessBreakpoints[HCP65_NUM_PROCESSES];	//User mode's for a process, NULL until it has any.
	bool				mbStopRun;
	bool				mbContention;
	bool				mbFill;
	uint8				muiFillByte;
	uint64				muiMainBusyUntil;	//The CPU cycle a device's use of main memory ends.

	uint64				muiClockRate;
	bool				mbNativeLatch;
	bool				mbKernel;
	uint8				muiInterruptDepth;
	uint8				muiProcessID;
	uint32				muiDeviceEnables;	//Bit n is device n; 0 holds it in reset.
	uint32				muiInstructions;
	uint32				muiInstructionsCleared;
	uint64				muiChunkEnd;		//Where Run's current chunk stops; 0 to stop after this instruction.

public:
	void				Init(uint64 uiClockRate = HCP65_DEFAULT_CLOCK_RATE);
	void				Kill(void);

	bool				SetDevice(int iSlot, CHCP65Device* pcDevice);
	CHCP65Device*		GetDevice(int iSlot);
	bool				LoadROM(uint8* puiData, size uiSize);

	void				Reset(void);
	uint64				Run(uint64 uiCycles);
	bool				IsStopped(void);
	bool				IsAtBreakpoint(void);
	CBreakpoints*		GetModeBreakpoints(bool bKernel);
	CBreakpoints*		GetProcessBreakpoints(uint8 uiProcess);
	void				ClearProcessBreakpoints(uint8 uiProcess);
	void				ClearBreakpoints(void);
	void				EnableAllBreakpoints(bool bEnabled);
	void				StopRun(void);
	void				SetBreak(int iBreak);

	CFast65C816*		GetMPU(void);
	CHCP65Registers*	GetRegisters(void);
	uint8*				GetMPURAM(void);
	uint8*				GetMPUROM(void);
	uint8*				GetMainMemory(void);
	uint64				NumCycles(void);
	uint64				GetClockRate(void);
	uint64				CrystalTick(uint64 uiCycle);
	uint64				CycleOfCrystalTick(uint64 uiTick);

	bool				IsKernelMode(void);
	bool				IsNativeLatched(void);
	uint8				GetInterruptDepth(void);
	void				SetInterruptDepth(uint8 uiDepth);
	uint8				GetProcessID(void);
	void				SetProcessID(uint8 uiProcessID);
	uint32				GetDeviceEnables(void);
	void				SetDeviceEnables(uint32 uiEnables);
	bool				IsDeviceInterrupting(int iSlot);
	uint32				GetInstructionCount(void);
	void				ClearInstructionCount(void);
	void				UpdateInterrupts(void);

	void				SetMemoryFill(bool bFill, uint8 uiFillByte = HCP65_DEBUG_FILL_BYTE);
	bool				IsMemoryFill(void);
	void				FillMainMemoryBank(uint8 uiBank);
	void				SetMainMemoryBusy(uint64 uiUntil);
	bool				IsContention(void);

	uint8				Read(uint32 uiAddress);
	void				Write(uint32 uiAddress, uint8 uiValue);
	uint8*				MemoryAt(uint32 uiAddress);

protected:
	void				UpdateMode(void);
	void				UseModeBreakpoints(void);
	void				EndChunk(void);
	bool				MainMemoryAccess(void);
	uint32				UserAddress(uint32 uiAddress);
	CHCP65Device*		DecodeDevice(uint32 uiAddress, uint32* puiOffset);
	void				CatchUpDevices(uint64 uiCycle);
	uint64				NextDeviceEvent(void);

	static uint8		ReadByte(void* pvContext, uint32 uiAddress);
	static void			WriteByte(void* pvContext, uint32 uiAddress, uint8 uiValue);
	static void			Signal(void* pvContext, uint8 uiSignal);
};


#endif // __HCP65_H__
