#include <stdlib.h>
#include <string.h>
#include "BaseLib/PointerFunctions.h"
#include "HCP65.h"


//////////////////////////////////////////////////////////////////////////
//
// uiClockRate is the CPU's clock in Hz; it turns cycles into time for the
// crystal driven timer and devices.  Erased flash reads $FF.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::Init(uint64 uiClockRate)
{
	int		i;

	muiClockRate = uiClockRate;
	mpuiRAM = (uint8*)malloc(HCP65_MPU_RAM_SIZE);
	mpuiROM = (uint8*)malloc(HCP65_MPU_ROM_SIZE);
	mpuiMain = (uint8*)malloc(HCP65_MAIN_MEMORY_SIZE);
	memset(mpuiRAM, 0, HCP65_MPU_RAM_SIZE);
	memset(mpuiROM, 0xFF, HCP65_MPU_ROM_SIZE);
	memset(mpuiMain, 0, HCP65_MAIN_MEMORY_SIZE);

	mcMPU.Init(ReadByte, WriteByte, this, mpuiRAM, HCP65_MPU_RAM_SIZE);
	mcMPU.SetSignal(Signal);
	mcKernelBreakpoints.Init();
	mcUserBreakpoints.Init();
	memset(mapcProcessBreakpoints, 0, sizeof(mapcProcessBreakpoints));
	mbStopRun = false;
	mbContention = false;
	muiMainBusyUntil = 0;
	mbFill = false;
	muiFillByte = HCP65_DEBUG_FILL_BYTE;

	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		mapcDevices[i] = NULL;
	}
	mcRegisters.Init();
	mcRegisters.Attach(this);
	mapcDevices[0] = &mcRegisters;

	mbNativeLatch = false;
	mbKernel = true;
	muiInterruptDepth = 0;
	muiProcessID = 0;
	muiDeviceEnables = 0;
	muiInstructions = 0;
	muiInstructionsCleared = 0;
	muiChunkEnd = 0;
	UpdateMode();
}


//////////////////////////////////////////////////////////////////////////
//
// The devices in the sockets belong to the caller.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::Kill(void)
{
	int		i;

	for (i = 1; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (mapcDevices[i])
		{
			mapcDevices[i]->Attach(NULL);
			mapcDevices[i] = NULL;
		}
	}
	mcRegisters.Kill();
	mcMPU.Kill();
	for (i = 0; i < HCP65_NUM_PROCESSES; i++)
	{
		ClearProcessBreakpoints((uint8)i);
	}
	mcUserBreakpoints.Kill();
	mcKernelBreakpoints.Kill();
	SafeFree(mpuiMain);
	SafeFree(mpuiROM);
	SafeFree(mpuiRAM);
}


//////////////////////////////////////////////////////////////////////////
//
// Puts pcDevice in socket iSlot (1-7 or 16-19), or empties it with NULL.
// A device that decodes A16 needs a 128KB socket.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::SetDevice(int iSlot, CHCP65Device* pcDevice)
{
	uint32	uiSlotSize;

	if ((iSlot >= 1) && (iSlot <= 7))
	{
		uiSlotSize = HCP65_SMALL_SLOT_SIZE;
	}
	else if ((iSlot >= 16) && (iSlot <= 19))
	{
		uiSlotSize = HCP65_LARGE_SLOT_SIZE;
	}
	else
	{
		return false;
	}

	if (pcDevice && (pcDevice->GetMinimumSlotSize() > uiSlotSize))
	{
		return false;
	}

	if (mapcDevices[iSlot])
	{
		mapcDevices[iSlot]->Attach(NULL);
	}
	mapcDevices[iSlot] = pcDevice;
	if (pcDevice)
	{
		pcDevice->Attach(this);
		pcDevice->Reset();
		pcDevice->SetHeldInReset((muiDeviceEnables & (1u << iSlot)) == 0);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CHCP65Device* CHCP65::GetDevice(int iSlot)
{
	if ((iSlot < 0) || (iSlot >= HCP65_NUM_DEVICE_SLOTS))
	{
		return NULL;
	}
	return mapcDevices[iSlot];
}


//////////////////////////////////////////////////////////////////////////
//
// Copies a ROM image to the start of the MPU ROM.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::LoadROM(uint8* puiData, size uiSize)
{
	if (uiSize > HCP65_MPU_ROM_SIZE)
	{
		return false;
	}
	memcpy(mpuiROM, puiData, uiSize);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The system reset.  The interrupt depth is cleared and the reset vector
// pull counts it up to one.  Every device is held in reset.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::Reset(void)
{
	int		i;

	mbNativeLatch = false;
	muiInterruptDepth = 0;
	muiProcessID = 0;
	mbContention = false;
	muiMainBusyUntil = 0;
	muiInstructionsCleared = muiInstructions;
	if (mbFill)
	{
		memset(mpuiRAM, muiFillByte, HCP65_MPU_RAM_SIZE);
		memset(mpuiMain, muiFillByte, HCP65_MAIN_MEMORY_SIZE);
	}
	mcRegisters.Reset();
	SetDeviceEnables(0);
	for (i = 1; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (mapcDevices[i])
		{
			mapcDevices[i]->Reset();
		}
	}
	UpdateMode();
	mcMPU.SetIRQ(false);
	mcMPU.SetNMI(false);
	mcMPU.Reset();
}


//////////////////////////////////////////////////////////////////////////
//
// Runs for at least uiCycles cycles (it finishes the instruction that
// crosses the end) or until the processor stops or the next instruction is
// at one of its enabled breakpoints (IsAtBreakpoint).  The run is cut into
// chunks that end where a device's interrupt changes, so interrupts are
// raised on the instruction they should be.  Returns the cycles run.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65::Run(uint64 uiCycles)
{
	uint64	uiStart;
	uint64	uiEnd;
	uint64	uiEvent;
	bool	bRunning;
	bool	bWaiting;
	bool	bStepped;

	uiStart = mcMPU.NumCycles();
	uiEnd = uiStart + uiCycles;
	bRunning = true;
	mbStopRun = false;
	while (bRunning && !mbStopRun && (mcMPU.NumCycles() < uiEnd))
	{
		muiChunkEnd = uiEnd;
		uiEvent = NextDeviceEvent();
		if (uiEvent < muiChunkEnd)
		{
			muiChunkEnd = uiEvent;
		}

		//At least one step, so that an event that is already due cannot stop the run.
		do
		{
			bWaiting = mcMPU.IsWaiting();
			bStepped = mcMPU.Step();
			if (!bWaiting && !mcMPU.IsAtBreakpoint())
			{
				//Not a cycle of waiting, nor a return before an instruction at a breakpoint.
				muiInstructions++;
			}
			if (!bStepped)
			{
				bRunning = false;
				break;
			}
		}
		while (!mbStopRun && (mcMPU.NumCycles() < muiChunkEnd));

		CatchUpDevices(mcMPU.NumCycles());
		UpdateInterrupts();
	}
	muiChunkEnd = 0;
	return mcMPU.NumCycles() - uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
// Something may have changed when a device's next event is (a register was
// written, the timer started), so Run works the chunk out again after the
// current instruction.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::EndChunk(void)
{
	muiChunkEnd = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::IsStopped(void)
{
	return mcMPU.IsStopped();
}


//////////////////////////////////////////////////////////////////////////
//
// The last Run returned before the instruction at one of the processor's
// breakpoints; the next Run executes it.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::IsAtBreakpoint(void)
{
	return mcMPU.IsAtBreakpoint();
}


//////////////////////////////////////////////////////////////////////////
//
// The breakpoints of kernel mode (bKernel) or user mode.  Change them at
// any time; the MPU stops at those of the mode it is in.
//
//////////////////////////////////////////////////////////////////////////
CBreakpoints* CHCP65::GetModeBreakpoints(bool bKernel)
{
	return bKernel ? &mcKernelBreakpoints : &mcUserBreakpoints;
}


//////////////////////////////////////////////////////////////////////////
//
// User mode's breakpoints for the process uiProcess, made the first time
// it is asked for.  Change them at any time.
//
//////////////////////////////////////////////////////////////////////////
CBreakpoints* CHCP65::GetProcessBreakpoints(uint8 uiProcess)
{
	if (mapcProcessBreakpoints[uiProcess] == NULL)
	{
		mapcProcessBreakpoints[uiProcess] = (CBreakpoints*)malloc(sizeof(CBreakpoints));
		mapcProcessBreakpoints[uiProcess]->Init();
		UseModeBreakpoints();
	}
	return mapcProcessBreakpoints[uiProcess];
}


//////////////////////////////////////////////////////////////////////////
//
// The process's breakpoints go (it has ended: its ID may be given to
// another).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::ClearProcessBreakpoints(uint8 uiProcess)
{
	if (mapcProcessBreakpoints[uiProcess] != NULL)
	{
		mapcProcessBreakpoints[uiProcess]->Kill();
		free(mapcProcessBreakpoints[uiProcess]);
		mapcProcessBreakpoints[uiProcess] = NULL;
		UseModeBreakpoints();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every mode's and process's breakpoints go.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::ClearBreakpoints(void)
{
	size	i;

	mcKernelBreakpoints.Clear();
	mcUserBreakpoints.Clear();
	for (i = 0; i < HCP65_NUM_PROCESSES; i++)
	{
		ClearProcessBreakpoints((uint8)i);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::EnableAllBreakpoints(bool bEnabled)
{
	size	i;

	mcKernelBreakpoints.EnableAll(bEnabled);
	mcUserBreakpoints.EnableAll(bEnabled);
	for (i = 0; i < HCP65_NUM_PROCESSES; i++)
	{
		if (mapcProcessBreakpoints[i] != NULL)
		{
			mapcProcessBreakpoints[i]->EnableAll(bEnabled);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Run returns after the current instruction (a device asks, as the debug
// port does when a process starts).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::StopRun(void)
{
	mbStopRun = true;
	EndChunk();
}


//////////////////////////////////////////////////////////////////////////
//
// A falling edge on break input iBreak (1 to 3), an NMI source.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::SetBreak(int iBreak)
{
	mcRegisters.SetBreak(iBreak);
	UpdateInterrupts();
}


//////////////////////////////////////////////////////////////////////////
//
// IRQ is low while any device's interrupt line is.  NMI is the processor
// board's latched sources; the processor sees its falling edge.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::UpdateInterrupts(void)
{
	bool	bIrq;
	int		i;

	bIrq = false;
	for (i = 1; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (mapcDevices[i] && mapcDevices[i]->IsInterrupting())
		{
			bIrq = true;
			break;
		}
	}
	mcMPU.SetIRQ(bIrq);
	mcMPU.SetNMI(mcRegisters.IsNMILow() || mbContention);
}


//////////////////////////////////////////////////////////////////////////
//
// Fill mode: memory is filled with uiFillByte at reset (and banks given to
// processes as the kernal says, FillMainMemoryBank).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::SetMemoryFill(bool bFill, uint8 uiFillByte)
{
	mbFill = bFill;
	muiFillByte = uiFillByte;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::IsMemoryFill(void)
{
	return mbFill;
}


//////////////////////////////////////////////////////////////////////////
//
// In fill mode, the 64KB main memory bank uiBank is filled (by the host).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::FillMainMemoryBank(uint8 uiBank)
{
	if (mbFill)
	{
		memset(&mpuiMain[(uint32)uiBank << 16], muiFillByte, 0x10000);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A device (the SD Card Board's DMA) uses main memory until uiUntil.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::SetMainMemoryBusy(uint64 uiUntil)
{
	if (uiUntil > muiMainBusyUntil)
	{
		muiMainBusyUntil = uiUntil;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::IsContention(void)
{
	return mbContention;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a processor access to main memory reaches it: not while a device
// is using it (that latches Contention) or once Contention is latched.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::MainMemoryAccess(void)
{
	if (!mbContention && (muiMainBusyUntil != 0) && (mcMPU.NumCycles() < muiMainBusyUntil))
	{
		mbContention = true;
		UpdateInterrupts();
		EndChunk();
	}
	return !mbContention;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::CatchUpDevices(uint64 uiCycle)
{
	int		i;

	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (mapcDevices[i])
		{
			mapcDevices[i]->CatchUp(uiCycle);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65::NextDeviceEvent(void)
{
	uint64	uiNext;
	uint64	uiEvent;
	int		i;

	uiNext = HCP65_NEVER;
	for (i = 0; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (mapcDevices[i])
		{
			uiEvent = mapcDevices[i]->NextEvent();
			if (uiEvent < uiNext)
			{
				uiNext = uiEvent;
			}
		}
	}
	return uiNext;
}


//////////////////////////////////////////////////////////////////////////
//
// Kernel mode is a non-zero interrupt depth, or before the processor has
// been native.  MPU RAM is the processor's flat block only when it is at
// address 0: kernel mode after the swap.  The MPU looks at the mode's
// breakpoints.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::UpdateMode(void)
{
	mbKernel = (muiInterruptDepth != 0) || !mbNativeLatch;
	UseModeBreakpoints();
	if (mbKernel && mbNativeLatch)
	{
		mcMPU.SetHole(0, 0);
	}
	else
	{
		mcMPU.SetHole(0, HCP65_MPU_RAM_SIZE);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The processor's bus events.  The interrupt pushes happen before the
// vector pull (so in the interrupted mode) and RTI's pulls after its opcode
// fetch (so in the mode being returned to).
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::Signal(void* pvContext, uint8 uiSignal)
{
	CHCP65*		pcHCP65;

	pcHCP65 = (CHCP65*)pvContext;
	if (uiSignal == FAST_65C816_SIGNAL_VECTOR_PULL)
	{
		pcHCP65->muiInterruptDepth = (uint8)((pcHCP65->muiInterruptDepth + 1) & 0x7F);
		pcHCP65->UpdateMode();
	}
	else if (uiSignal == FAST_65C816_SIGNAL_RTI)
	{
		pcHCP65->muiInterruptDepth = (uint8)((pcHCP65->muiInterruptDepth - 1) & 0x7F);
		pcHCP65->UpdateMode();
	}
	else if (uiSignal == FAST_65C816_SIGNAL_NATIVE)
	{
		if (!pcHCP65->mbNativeLatch)
		{
			pcHCP65->mbNativeLatch = true;
			pcHCP65->mcRegisters.StartTimer(pcHCP65->mcMPU.NumCycles());
			pcHCP65->UpdateMode();
			pcHCP65->EndChunk();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A user mode address's physical main memory address.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65::UserAddress(uint32 uiAddress)
{
	uint8	uiBank;

	uiBank = mpuiRAM[HCP65_REMAP_TABLE + ((uint32)muiProcessID << 8) + (uiAddress >> 16)];
	return ((uint32)uiBank << 16) | (uiAddress & 0xFFFF);
}


//////////////////////////////////////////////////////////////////////////
//
// The device at a kernel mode address from $100000 to $1FFFFF, or NULL.
//
//////////////////////////////////////////////////////////////////////////
CHCP65Device* CHCP65::DecodeDevice(uint32 uiAddress, uint32* puiOffset)
{
	if (uiAddress < 0x140000)
	{
		*puiOffset = uiAddress & (HCP65_SMALL_SLOT_SIZE - 1);
		return mapcDevices[(uiAddress >> 15) & 7];
	}
	else if (uiAddress >= 0x180000)
	{
		*puiOffset = uiAddress & (HCP65_LARGE_SLOT_SIZE - 1);
		return mapcDevices[16 + ((uiAddress >> 17) & 3)];
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A bus read, as the processor would make it now.  Nothing drives the
// data bus for an empty socket.
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65::Read(uint32 uiAddress)
{
	CHCP65Device*	pcDevice;
	uint32			uiOffset;
	uint8			uiValue;

	uiAddress &= 0xFFFFFF;
	if (!mbKernel)
	{
		return MainMemoryAccess() ? mpuiMain[UserAddress(uiAddress)] : 0xFF;
	}

	if (uiAddress >= 0x200000)
	{
		return MainMemoryAccess() ? mpuiMain[uiAddress] : 0xFF;
	}

	if (uiAddress < 0x100000)
	{
		//ROM is the upper half once native, the lower half before.
		if ((uiAddress >= HCP65_MPU_RAM_SIZE) == mbNativeLatch)
		{
			return mpuiROM[uiAddress & (HCP65_MPU_ROM_SIZE - 1)];
		}
		return mpuiRAM[uiAddress & (HCP65_MPU_RAM_SIZE - 1)];
	}

	pcDevice = DecodeDevice(uiAddress, &uiOffset);
	if (!pcDevice)
	{
		return 0xFF;
	}
	if (pcDevice == &mcRegisters)
	{
		//The interrupt registers read every device.
		CatchUpDevices(mcMPU.NumCycles());
	}
	else
	{
		pcDevice->CatchUp(mcMPU.NumCycles());
	}
	uiValue = pcDevice->Read(uiOffset);
	UpdateInterrupts();
	EndChunk();
	return uiValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::Write(uint32 uiAddress, uint8 uiValue)
{
	CHCP65Device*	pcDevice;
	uint32			uiOffset;

	uiAddress &= 0xFFFFFF;
	if (!mbKernel)
	{
		if (MainMemoryAccess())
		{
			mpuiMain[UserAddress(uiAddress)] = uiValue;
		}
		return;
	}

	if (uiAddress >= 0x200000)
	{
		if (MainMemoryAccess())
		{
			mpuiMain[uiAddress] = uiValue;
		}
		return;
	}

	if (uiAddress < 0x100000)
	{
		if ((uiAddress >= HCP65_MPU_RAM_SIZE) != mbNativeLatch)
		{
			mpuiRAM[uiAddress & (HCP65_MPU_RAM_SIZE - 1)] = uiValue;
		}
		return;
	}

	pcDevice = DecodeDevice(uiAddress, &uiOffset);
	if (pcDevice)
	{
		pcDevice->CatchUp(mcMPU.NumCycles());
		pcDevice->Write(uiOffset, uiValue);
		UpdateInterrupts();
		EndChunk();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The memory a processor address is now (as Read and Write would find it),
// or NULL where it is a device or an empty socket.  For a debugger, which
// must read and write without touching devices.  ROM can be written here.
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65::MemoryAt(uint32 uiAddress)
{
	uiAddress &= 0xFFFFFF;
	if (!mbKernel)
	{
		return &mpuiMain[UserAddress(uiAddress)];
	}
	if (uiAddress >= 0x200000)
	{
		return &mpuiMain[uiAddress];
	}
	if (uiAddress < 0x100000)
	{
		if ((uiAddress >= HCP65_MPU_RAM_SIZE) == mbNativeLatch)
		{
			return &mpuiROM[uiAddress & (HCP65_MPU_ROM_SIZE - 1)];
		}
		return &mpuiRAM[uiAddress & (HCP65_MPU_RAM_SIZE - 1)];
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The MPU looks at the mode's breakpoints, and in user mode the running
// process's too.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::UseModeBreakpoints(void)
{
	if (mbKernel)
	{
		mcMPU.UseBreakpoints(&mcKernelBreakpoints);
	}
	else
	{
		mcMPU.UseBreakpoints(&mcUserBreakpoints, mapcProcessBreakpoints[muiProcessID]);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65::ReadByte(void* pvContext, uint32 uiAddress)
{
	return ((CHCP65*)pvContext)->Read(uiAddress);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::WriteByte(void* pvContext, uint32 uiAddress, uint8 uiValue)
{
	((CHCP65*)pvContext)->Write(uiAddress, uiValue);
}


//////////////////////////////////////////////////////////////////////////
//
// The ticks of a device's uiRate Hz clock completed by CPU cycle uiCycle.
// Whole seconds and the rest are taken apart, so it doesn't overflow.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65::DeviceTick(uint64 uiCycle, uint64 uiRate)
{
	return (uiCycle / muiClockRate) * uiRate + ((uiCycle % muiClockRate) * uiRate) / muiClockRate;
}


//////////////////////////////////////////////////////////////////////////
//
// The first CPU cycle by which tick uiTick of a device's uiRate Hz clock
// has happened.
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65::CycleOfDeviceTick(uint64 uiTick, uint64 uiRate)
{
	return (uiTick / uiRate) * muiClockRate + ((uiTick % uiRate) * muiClockRate + uiRate - 1) / uiRate;
}


//////////////////////////////////////////////////////////////////////////
//
// The 32.768kHz crystal's (the timer's and the sound board's).
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65::CrystalTick(uint64 uiCycle)
{
	return DeviceTick(uiCycle, HCP65_CRYSTAL_RATE);
}


uint64 CHCP65::CycleOfCrystalTick(uint64 uiTick)
{
	return CycleOfDeviceTick(uiTick, HCP65_CRYSTAL_RATE);
}


//////////////////////////////////////////////////////////////////////////
//
// The Device Reset/Enable registers.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::SetDeviceEnables(uint32 uiEnables)
{
	int		i;
	bool	bHeld;

	muiDeviceEnables = uiEnables & 0xF00FE;
	for (i = 1; i < HCP65_NUM_DEVICE_SLOTS; i++)
	{
		if (mapcDevices[i])
		{
			bHeld = (muiDeviceEnables & (1u << i)) == 0;
			if (bHeld != mapcDevices[i]->IsHeldInReset())
			{
				mapcDevices[i]->SetHeldInReset(bHeld);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65::GetDeviceEnables(void)
{
	return muiDeviceEnables;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::IsDeviceInterrupting(int iSlot)
{
	if ((iSlot < 1) || (iSlot >= HCP65_NUM_DEVICE_SLOTS) || !mapcDevices[iSlot])
	{
		return false;
	}
	return mapcDevices[iSlot]->IsInterrupting();
}


//////////////////////////////////////////////////////////////////////////
//
// Writing the depth can change the mode at once.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::SetInterruptDepth(uint8 uiDepth)
{
	muiInterruptDepth = (uint8)(uiDepth & 0x7F);
	UpdateMode();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65::GetInterruptDepth(void)
{
	return muiInterruptDepth;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::SetProcessID(uint8 uiProcessID)
{
	muiProcessID = uiProcessID;
	UseModeBreakpoints();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8 CHCP65::GetProcessID(void)
{
	return muiProcessID;
}


//////////////////////////////////////////////////////////////////////////
//
// Instructions (opcode fetches) since the counter was last cleared.
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65::GetInstructionCount(void)
{
	return muiInstructions - muiInstructionsCleared;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65::ClearInstructionCount(void)
{
	muiInstructionsCleared = muiInstructions;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::IsKernelMode(void)
{
	return mbKernel;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65::IsNativeLatched(void)
{
	return mbNativeLatch;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CFast65C816* CHCP65::GetMPU(void)
{
	return &mcMPU;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CHCP65Registers* CHCP65::GetRegisters(void)
{
	return &mcRegisters;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65::GetMPURAM(void)
{
	return mpuiRAM;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65::GetMPUROM(void)
{
	return mpuiROM;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65::GetMainMemory(void)
{
	return mpuiMain;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65::NumCycles(void)
{
	return mcMPU.NumCycles();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CHCP65::GetClockRate(void)
{
	return muiClockRate;
}
