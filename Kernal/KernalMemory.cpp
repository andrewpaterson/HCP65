#include <string.h>
#include "BaseLib/BankMallocator.h"
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
uint16					guiKernalProcessBanks;
uint16					guiKernalFreeBanks;
static uint16			guiKernalLowestFree;	//No bank below it is free (it may not be).
//The main memory heap's banks are the ones no process has: it takes them highest first.
class CKernalMainBanks : public CBankSource
{
public:
	bool	Acquire(void* pvBank, size uiCount) override;
	void	Release(void* pvBank, size uiCount) override;
};


static CKernalMainBanks	gcKernalMainBanks;
static CBankMallocator	gcKernalMainHeap;
static bool				gbKernalMainHeap;


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
		KernalSetBankUse((uint16)(uiBank + i), KERNAL_BANK_PROCESS);
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
// The physical bank is about to change (bAdd false) or has changed (true):
// the counts of process banks and free banks, and the lowest free bank,
// follow it.
//
//////////////////////////////////////////////////////////////////////////
static void KernalCountBank(uint16 uiPhysical, bool bAdd)
{
	if (gauiKernalBanks[uiPhysical] != KERNAL_BANK_PROCESS)
	{
		return;
	}
	if (bAdd)
	{
		guiKernalProcessBanks++;
		if (gauiKernalBankOwners[uiPhysical] == 0)
		{
			guiKernalFreeBanks++;
			if (uiPhysical < guiKernalLowestFree)
			{
				guiKernalLowestFree = uiPhysical;
			}
		}
	}
	else
	{
		guiKernalProcessBanks--;
		if (gauiKernalBankOwners[uiPhysical] == 0)
		{
			guiKernalFreeBanks--;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the physical bank is (KERNAL_BANK_PROCESS and the others).
//
//////////////////////////////////////////////////////////////////////////
void KernalSetBankUse(uint16 uiPhysical, uint8 uiUse)
{
	KernalCountBank(uiPhysical, false);
	gauiKernalBanks[uiPhysical] = uiUse;
	KernalCountBank(uiPhysical, true);
}


//////////////////////////////////////////////////////////////////////////
//
// The process the physical bank is given to, 0 if none.  Only the free
// count can change (unmapping is a bank at a time, so this is short).
//
//////////////////////////////////////////////////////////////////////////
void KernalSetBankOwner(uint16 uiPhysical, uint8 uiProcess)
{
	if (gauiKernalBanks[uiPhysical] == KERNAL_BANK_PROCESS)
	{
		if (gauiKernalBankOwners[uiPhysical] == 0)
		{
			guiKernalFreeBanks--;
		}
		if (uiProcess == 0)
		{
			guiKernalFreeBanks++;
			if (uiPhysical < guiKernalLowestFree)
			{
				guiKernalLowestFree = uiPhysical;
			}
		}
	}
	gauiKernalBankOwners[uiPhysical] = uiProcess;
}


//////////////////////////////////////////////////////////////////////////
//
// The uiBanks banks from uiFirst of the remap page puiPage that uiProcess
// has are given back and unmapped (one loop, not KernalSetBankOwner per
// bank: a process's bank is always KERNAL_BANK_PROCESS).
//
//////////////////////////////////////////////////////////////////////////
void KernalGiveBackBanks(uint8* puiPage, uint16 uiFirst, uint16 uiBanks, uint8 uiProcess)
{
	uint8*	puiEntry;
	uint8	uiPhysical;
	uint16	i;

	puiEntry = &puiPage[uiFirst];
	for (i = 0; i < uiBanks; i++)
	{
		uiPhysical = puiEntry[i];
		if ((uiPhysical != 0) && (gauiKernalBankOwners[uiPhysical] == uiProcess))
		{
			gauiKernalBankOwners[uiPhysical] = 0;
			puiEntry[i] = 0;
			guiKernalFreeBanks++;
			if (uiPhysical < guiKernalLowestFree)
			{
				guiKernalLowestFree = uiPhysical;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The lowest free bank.  False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool KernalFindFreeBank(uint16* puiPhysical)
{
	uint16	uiBank;

	if (guiKernalFreeBanks == 0)
	{
		return false;
	}
	for (uiBank = guiKernalLowestFree; uiBank < KERNAL_NUM_BANKS; uiBank++)
	{
		if (KernalIsBankFree(uiBank))
		{
			guiKernalLowestFree = uiBank;
			*puiPhysical = uiBank;
			return true;
		}
	}
	return false;
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
			KernalSetBankUse(uiBank, uiUse);
			*puiBank = (uint8)uiBank;
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// uiCount neighbouring banks from pvBank for the main memory heap, if
// kernel mode reaches them and no process has them.
//
//////////////////////////////////////////////////////////////////////////
bool CKernalMainBanks::Acquire(void* pvBank, size uiCount)
{
	uint16	uiBank;
	uint16	i;

	uiBank = (uint16)((uint32)pvBank >> 16);
	for (i = 0; i < uiCount; i++)
	{
		if (!KernalIsBankFree((uint16)(uiBank + i)))
		{
			return false;
		}
	}
	KernalSetBankUse(uiBank, KERNAL_BANK_HEAP);
	for (i = 1; i < uiCount; i++)
	{
		KernalSetBankUse((uint16)(uiBank + i), KERNAL_BANK_HEAP_MORE);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Banks the main memory heap is done with go back to the processes.
//
//////////////////////////////////////////////////////////////////////////
void CKernalMainBanks::Release(void* pvBank, size uiCount)
{
	uint16	uiBank;
	uint16	i;

	uiBank = (uint16)((uint32)pvBank >> 16);
	for (i = 0; i < uiCount; i++)
	{
		KernalSetBankUse((uint16)(uiBank + i), KERNAL_BANK_PROCESS);
	}
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
	void*	pvFirst;
	uint16	i;

	memset(gauiKernalBanks, KERNAL_BANK_ABSENT, sizeof(gauiKernalBanks));
	memset(gauiKernalBankOwners, 0, sizeof(gauiKernalBankOwners));
	guiKernalProcessBanks = 0;
	guiKernalFreeBanks = 0;
	guiKernalLowestFree = KERNAL_NUM_BANKS;
	auiRegisters[0] = 0;
	auiRegisters[1] = 0;
	uiWindows = 0;
	KernalGiveWindows(false, &uiWindows, auiRegisters);
	KernalGiveWindows(true, &uiWindows, auiRegisters);
	((volatile uint8*)KERNAL_MEMORY_CHANNEL)[0] = auiRegisters[0];
	((volatile uint8*)KERNAL_MEMORY_CHANNEL)[1] = auiRegisters[1];

	//A remap page's 0 is a bank not mapped: physical bank 0 can't be given.
	KernalSetBankUse(0, KERNAL_BANK_ABSENT);

	for (i = 0; i < KERNAL_NUM_TABLE_BANKS; i++)
	{
		if (!KernalTakeBank(KERNAL_BANK_TABLES, &gauiKernalTableBanks[i]))
		{
			return false;
		}
	}

	//The heap takes its first bank now (the highest no process has) and keeps it as its spare, so it has one however
	//many banks processes are given.
	if (!gcKernalMainHeap.Init((void*)((uint32)KERNAL_MAIN_MEMORY_BANK << 16), (uint32)(KERNAL_NUM_BANKS - KERNAL_MAIN_MEMORY_BANK) << 16, KERNAL_BANK_SIZE, &gcKernalMainBanks, 1, true))
	{
		return false;
	}
	pvFirst = gcKernalMainHeap.Malloc(1);
	if (!pvFirst)
	{
		return false;
	}
	gcKernalMainHeap.Free(pvFirst);
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
// Board's DMA): all in one bank if bOneBank (KERNAL_MAIN_ONE_BANK), else it
// may cross banks, and be more than one.  The heap takes the banks it
// needs for them from those no process has.  NULL if there's no main
// memory or not the room.
//
//////////////////////////////////////////////////////////////////////////
void* KernalMainMalloc(uint32 uiBytes, bool bOneBank)
{
	if (!gbKernalMainHeap || (bOneBank && (uiBytes > gcKernalMainHeap.MaxSmall())))
	{
		return NULL;
	}
	return gcKernalMainHeap.Malloc(uiBytes);
}


//////////////////////////////////////////////////////////////////////////
//
// A region the heap took that has nothing allocated in it any more goes
// back to the processes.
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
