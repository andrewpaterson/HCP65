#include <string.h>
#include "BaseLib/TrueMallocator.h"
#include "KernalDevices.h"
#include "KernalMemory.h"


//The memory sockets, in the order they are given windows, and each one's place in the main memory channel's
//registers: register 1's D3-D2 is socket X (device 3), and so on.
#define KERNAL_NUM_MEMORY_SOCKETS	6

static const uint8	gauiKernalMemorySlots[KERNAL_NUM_MEMORY_SOCKETS] = { 3, 4, 16, 17, 18, 19 };
static const uint8	gauiKernalMemoryRegisters[KERNAL_NUM_MEMORY_SOCKETS] = { 1, 1, 1, 1, 0, 0 };
static const uint8	gauiKernalMemoryShifts[KERNAL_NUM_MEMORY_SOCKETS] = { 2, 0, 6, 4, 6, 4 };
static const uint8	gauiKernalWindows[] = { 1, 2, 3, 0 };


//Libc65's heap (malloc).
extern CTrueMallocator	gcHeap;
extern bool				gbHeap;

uint8					gauiKernalBanks[KERNAL_NUM_BANKS];
uint8					gauiKernalTableBanks[KERNAL_NUM_TABLE_BANKS];
static CTrueMallocator	gcKernalMainHeap;
static bool				gbKernalMainHeap;


//////////////////////////////////////////////////////////////////////////
//
// Called by the start up before the init table: the kernal's heap is MPU
// RAM, before anything can allocate.
//
//////////////////////////////////////////////////////////////////////////
extern "C" void KernalInitHeap(void)
{
	gcHeap.Init((void*)KERNAL_MPU_HEAP_START, KERNAL_MPU_HEAP_END - KERNAL_MPU_HEAP_START);
	gbHeap = true;
	gbKernalMainHeap = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The banks of window uiWindow's chip A (its lower 2MB) or B are main
// memory.
//
//////////////////////////////////////////////////////////////////////////
static void KernalAddChip(uint8 uiWindow, uint8 uiChip)
{
	uint16	uiBank;
	uint16	i;

	uiBank = (uint16)(((uint16)uiWindow << 6) + ((uint16)uiChip << 5));
	for (i = 0; i < 32; i++)
	{
		gauiKernalBanks[uiBank + i] = KERNAL_BANK_PROCESS;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The next process bank kernel mode reaches, taken for uiUse.  False if
// there is none.
//
//////////////////////////////////////////////////////////////////////////
static bool KernalTakeBank(uint8 uiUse, uint8* puiBank)
{
	uint16	uiBank;

	for (uiBank = KERNAL_MAIN_MEMORY_BANK; uiBank < KERNAL_NUM_BANKS; uiBank++)
	{
		if (gauiKernalBanks[uiBank] == KERNAL_BANK_PROCESS)
		{
			gauiKernalBanks[uiBank] = uiUse;
			*puiBank = (uint8)uiBank;
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Gives each memory board found (KernalScanDevices) its window, writes the
// main memory channel's registers and takes the tables' and the heap's
// banks.  False if there isn't enough main memory for them.
//
//////////////////////////////////////////////////////////////////////////
bool KernalInitMainMemory(void)
{
	SKernalDevice*	psDevice;
	uint8			auiRegisters[2];
	uint8			uiWindow;
	uint8			uiBoards;
	uint8			uiBank;
	uint8			uiMainBus;
	uint16			i;

	memset(gauiKernalBanks, KERNAL_BANK_ABSENT, sizeof(gauiKernalBanks));
	auiRegisters[0] = 0;
	auiRegisters[1] = 0;
	uiBoards = 0;
	for (i = 0; (i < KERNAL_NUM_MEMORY_SOCKETS) && (uiBoards < sizeof(gauiKernalWindows)); i++)
	{
		psDevice = &gasKernalDevices[gauiKernalMemorySlots[i]];
		uiMainBus = psDevice->psROM ? psDevice->psROM->uiMainBus : 0;
		if ((uiMainBus & (DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B)) == 0)
		{
			continue;
		}
		uiWindow = gauiKernalWindows[uiBoards];
		uiBoards++;
		auiRegisters[gauiKernalMemoryRegisters[i]] |= (uint8)(uiWindow << gauiKernalMemoryShifts[i]);
		if (uiMainBus & DEVICE_MAIN_BUS_WINDOW_A)
		{
			KernalAddChip(uiWindow, 0);
		}
		if (uiMainBus & DEVICE_MAIN_BUS_WINDOW_B)
		{
			KernalAddChip(uiWindow, 1);
		}
	}
	((volatile uint8*)KERNAL_MEMORY_CHANNEL)[0] = auiRegisters[0];
	((volatile uint8*)KERNAL_MEMORY_CHANNEL)[1] = auiRegisters[1];

	//A remap page's 0 is a bank not mapped: physical bank 0 can't be given.
	gauiKernalBanks[0] = KERNAL_BANK_ABSENT;

	for (i = 0; i < KERNAL_NUM_TABLE_BANKS; i++)
	{
		if (!KernalTakeBank(KERNAL_BANK_TABLES, &gauiKernalTableBanks[i]))
		{
			return false;
		}
	}
	if (!KernalTakeBank(KERNAL_BANK_HEAP, &uiBank))
	{
		return false;
	}
	gcKernalMainHeap.Init((void*)((uint32)uiBank << 16), KERNAL_MAIN_HEAP_SIZE);
	gbKernalMainHeap = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The banks of main memory there are, the kernal's included.
//
//////////////////////////////////////////////////////////////////////////
uint16 KernalNumMainBanks(void)
{
	uint16	uiBank;
	uint16	uiCount;

	uiCount = 0;
	for (uiBank = 0; uiBank < KERNAL_NUM_BANKS; uiBank++)
	{
		if (gauiKernalBanks[uiBank] != KERNAL_BANK_ABSENT)
		{
			uiCount++;
		}
	}
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// uiBytes of main memory that kernel mode reaches (and the SD Card
// Board's DMA), NULL if there's none or no room.
//
//////////////////////////////////////////////////////////////////////////
void* KernalMainMalloc(uint32 uiBytes)
{
	return gbKernalMainHeap ? gcKernalMainHeap.Malloc(uiBytes) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KernalMainFree(void* pv)
{
	if (gbKernalMainHeap && pv)
	{
		gcKernalMainHeap.Free(pv);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 KernalMainHeapPeak(void)
{
	return gbKernalMainHeap ? (uint32)gcKernalMainHeap.PeakBytes() : 0;
}
