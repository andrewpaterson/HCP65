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
extern "C" void		__HeapInit(void* pvStart, uint32 uiBytes);

uint8					gauiKernalBanks[KERNAL_NUM_BANKS];
uint8					gauiKernalBankOwners[KERNAL_NUM_BANKS];
uint8					gauiKernalTableBanks[KERNAL_NUM_TABLE_BANKS];
static CTrueMallocator	gcKernalMainHeap;
static bool				gbKernalMainHeap;
static uint8			guiKernalMainHeapBank;	//Its first bank, which it keeps.


//////////////////////////////////////////////////////////////////////////
//
// Called by the start up before the init table: the kernal's heap is MPU
// RAM, before anything can allocate.
//
//////////////////////////////////////////////////////////////////////////
extern "C" void KernalInitHeap(void)
{
	__HeapInit((void*)KERNAL_MPU_HEAP_START, KERNAL_MPU_HEAP_END - KERNAL_MPU_HEAP_START);
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
// True if the physical bank is one processes can have and none has.
//
//////////////////////////////////////////////////////////////////////////
bool KernalIsBankFree(uint16 uiPhysical)
{
	return (gauiKernalBanks[uiPhysical] == KERNAL_BANK_PROCESS) && (gauiKernalBankOwners[uiPhysical] == 0);
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
		if (KernalIsBankFree(uiBank))
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
// The highest run of uiCount neighbouring banks that kernel mode reaches
// and no process has, taken for a region of the main memory heap: *puiBank
// is the first of them.  False if there is no such run.
//
//////////////////////////////////////////////////////////////////////////
static bool KernalTakeHeapBanks(uint16 uiCount, uint8* puiBank)
{
	uint16	uiBank;
	uint16	uiRun;
	uint16	i;

	uiRun = 0;
	for (uiBank = KERNAL_NUM_BANKS; uiBank > KERNAL_MAIN_MEMORY_BANK; uiBank--)
	{
		uiRun = KernalIsBankFree((uint16)(uiBank - 1)) ? (uint16)(uiRun + 1) : 0;
		if (uiRun == uiCount)
		{
			gauiKernalBanks[uiBank - 1] = KERNAL_BANK_HEAP;
			for (i = 1; i < uiCount; i++)
			{
				gauiKernalBanks[uiBank - 1 + i] = KERNAL_BANK_HEAP_MORE;
			}
			*puiBank = (uint8)(uiBank - 1);
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Gives the next windows to the boards in the memory sockets that provide
// windows and are (bPrivate) or aren't private, setting their sockets' bits
// of the main memory channel's registers.  Only a board that isn't private
// is main memory.  *puiWindows counts the windows given.
//
//////////////////////////////////////////////////////////////////////////
static void KernalGiveWindows(bool bPrivate, uint8* puiWindows, uint8* puiRegisters)
{
	SKernalDevice*	psDevice;
	uint8			uiWindow;
	uint8			uiMainBus;
	uint16			i;

	for (i = 0; (i < KERNAL_NUM_MEMORY_SOCKETS) && (*puiWindows < sizeof(gauiKernalWindows)); i++)
	{
		psDevice = &gasKernalDevices[gauiKernalMemorySlots[i]];
		uiMainBus = psDevice->psROM ? psDevice->psROM->uiMainBus : 0;
		if (((uiMainBus & (DEVICE_MAIN_BUS_WINDOW_A | DEVICE_MAIN_BUS_WINDOW_B)) == 0) || (((uiMainBus & DEVICE_MAIN_BUS_PRIVATE) != 0) != bPrivate))
		{
			continue;
		}
		uiWindow = gauiKernalWindows[*puiWindows];
		(*puiWindows)++;
		psDevice->uiWindow = uiWindow;
		puiRegisters[gauiKernalMemoryRegisters[i]] |= (uint8)(uiWindow << gauiKernalMemoryShifts[i]);
		if (!bPrivate)
		{
			if (uiMainBus & DEVICE_MAIN_BUS_WINDOW_A)
			{
				KernalAddChip(uiWindow, 0);
			}
			if (uiMainBus & DEVICE_MAIN_BUS_WINDOW_B)
			{
				KernalAddChip(uiWindow, 1);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Gives each memory board found (KernalScanDevices) its window, then each
// private board (a coprocessor board, whose memory is its own), writes the
// main memory channel's registers and takes the tables' and the heap's
// banks.  False if there isn't enough main memory for them.
//
//////////////////////////////////////////////////////////////////////////
bool KernalInitMainMemory(void)
{
	uint8	auiRegisters[2];
	uint8	uiWindows;
	uint8	uiBank;
	uint16	i;

	memset(gauiKernalBanks, KERNAL_BANK_ABSENT, sizeof(gauiKernalBanks));
	auiRegisters[0] = 0;
	auiRegisters[1] = 0;
	uiWindows = 0;
	KernalGiveWindows(false, &uiWindows, auiRegisters);
	KernalGiveWindows(true, &uiWindows, auiRegisters);
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
	guiKernalMainHeapBank = uiBank;
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
// uiBytes from the main memory heap as it is: all in one bank if bOneBank.
//
//////////////////////////////////////////////////////////////////////////
static void* KernalMainHeapMalloc(uint32 uiBytes, bool bOneBank)
{
	return bOneBank ? gcKernalMainHeap.MallocWithin(uiBytes, KERNAL_BANK_SIZE) : gcKernalMainHeap.Malloc(uiBytes);
}


//////////////////////////////////////////////////////////////////////////
//
// uiBytes of main memory that kernel mode reaches (and the SD Card
// Board's DMA): all in one bank if bOneBank (KERNAL_MAIN_ONE_BANK), else it
// may cross banks, and be more than one.  The heap takes the banks it
// needs for them from those no process has.  NULL if there's no main
// memory or not the room.
//
//////////////////////////////////////////////////////////////////////////
void* KernalMainMalloc(uint32 uiBytes, bool bOneBank)
{
	void*	pv;
	uint32	uiBanks;
	uint8	uiBank;
	uint16	i;

	if (!gbKernalMainHeap)
	{
		return NULL;
	}
	pv = KernalMainHeapMalloc(uiBytes, bOneBank);
	if (pv)
	{
		return pv;
	}

	//A region of as many banks as hold it.
	if (uiBytes > ((uint32)(KERNAL_NUM_BANKS - KERNAL_MAIN_MEMORY_BANK) << 16) - KERNAL_MAIN_HEAP_SPARE)
	{
		return NULL;
	}
	uiBanks = (uiBytes + KERNAL_MAIN_HEAP_SPARE + KERNAL_BANK_SIZE - 1) >> 16;
	if ((bOneBank && (uiBanks != 1)) || !KernalTakeHeapBanks((uint16)uiBanks, &uiBank))
	{
		return NULL;
	}
	if (!gcKernalMainHeap.AddRegion((void*)((uint32)uiBank << 16), uiBanks << 16))
	{
		for (i = 0; i < uiBanks; i++)
		{
			gauiKernalBanks[uiBank + i] = KERNAL_BANK_PROCESS;
		}
		return NULL;
	}
	return KernalMainHeapMalloc(uiBytes, bOneBank);
}


//////////////////////////////////////////////////////////////////////////
//
// A region the heap took that has nothing allocated in it any more goes
// back to the processes.
//
//////////////////////////////////////////////////////////////////////////
void KernalMainFree(void* pv)
{
	uint16	uiBank;

	if (!gbKernalMainHeap || !pv)
	{
		return;
	}
	gcKernalMainHeap.Free(pv);

	uiBank = (uint16)((uint32)pv >> 16);
	while (gauiKernalBanks[uiBank] == KERNAL_BANK_HEAP_MORE)
	{
		uiBank--;
	}
	if ((uiBank != guiKernalMainHeapBank) && gcKernalMainHeap.IsRegionFree((void*)((uint32)uiBank << 16)) && gcKernalMainHeap.RemoveRegion((void*)((uint32)uiBank << 16)))
	{
		gauiKernalBanks[uiBank] = KERNAL_BANK_PROCESS;
		for (uiBank++; (uiBank < KERNAL_NUM_BANKS) && (gauiKernalBanks[uiBank] == KERNAL_BANK_HEAP_MORE); uiBank++)
		{
			gauiKernalBanks[uiBank] = KERNAL_BANK_PROCESS;
		}
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
