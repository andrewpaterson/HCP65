#include <string.h>
#include "HCP65User.h"
#include "HCP65SoundBoard.h"


#define BOOT_PROGRAM	"\\modplay.exe"
#define BOOT_SELF		"\\boottest.exe"
#define BOOT_CHILD		0xB0C1	//The argument it runs itself with as the child (BootChild) when there is one sound board.
#define BOOT_CHILD_TWO	0xB0C2	//The child's argument when there is a second sound board, which it opens too.
#define BOOT_NO_TABLES	200		//The exit code when the maths tables couldn't be loaded, plus why.
#define BOOT_DEVICES	300		//The exit code when a device call didn't do what it should, plus which.
#define BOOT_MAX_BOARDS	20		//More boards than there are device sockets.
#define BOOT_MEMORY		400		//The exit code when a heap bank call didn't do what it should, plus which.
#define BOOT_BANKS		0x300000	//Where it maps banks of its heap.
#define BOOT_MORE_BANKS	0x500000	//Where it maps the rest of main memory's free banks.
#define BOOT_STACK		0xB0C3	//The argument it runs itself with as a child whose stack grows too far (BootOverflow).
#define BOOT_NULL		0xB0C4	//The argument of a child that writes through a NULL pointer.
#define BOOT_CANARIES	500		//The exit code when such a child wasn't ended as it should be, plus which.
#define BOOT_FRAME		512		//The bytes of stack each of BootOverflow's calls takes.


//In globals so the compiler can't work the answers out itself.
uint16	guiA = 300;
uint16	guiB = 200;
uint32	guiProduct;
uint8*	gpuiNull = NULL;


//////////////////////////////////////////////////////////////////////////
//
// Checks the device calls do what they should with what any HCP65 with a
// sound board has: 0 if they do, or which check failed.  The handle it
// opens is closed again, so modplay.exe can open the sound board.
//
//////////////////////////////////////////////////////////////////////////
uint16 BootCheckDevices(void)
{
	SKernalDeviceInfo	sInfo;
	uint16				uiIndex;
	uint16				uiHandle;
	uint16				uiRight;
	uint16				uiSoundBoards;
	uint16				uiExitCode;
	uint32				uiDone;

	//The boards end, and one of them is a sound board with its driver's two units.
	uiSoundBoards = 0;
	for (uiIndex = 0; uiIndex < BOOT_MAX_BOARDS; uiIndex++)
	{
		if (KernalDeviceInfo(uiIndex, &sInfo) != KERNAL_DEVICE_SUCCESS)
		{
			break;
		}
		if ((strcmp(sInfo.szManufacturer, SOUND_BOARD_MANUFACTURER) == 0) && (strcmp(sInfo.szName, SOUND_BOARD_NAME) == 0) && (sInfo.uiNumUnits == 2) && !sInfo.uiOpen)
		{
			uiSoundBoards++;
		}
	}
	if ((uiIndex == BOOT_MAX_BOARDS) || (KernalDeviceInfo(uiIndex, &sInfo) != KERNAL_DEVICE_NOT_FOUND))
	{
		return 1;
	}
	if (uiSoundBoards == 0)
	{
		return 2;
	}

	//A handle that isn't open, a board the kernal keeps (the SD Card Board boottest.exe was loaded from), one there isn't and
	//a unit there isn't.
	if (KernalDeviceClose(1) != KERNAL_DEVICE_BAD_HANDLE)
	{
		return 3;
	}
	if (KernalDeviceOpen("HCP65", "SD Card Board", 0, 0, &uiHandle) != KERNAL_DEVICE_NOT_SUPPORTED)
	{
		return 4;
	}
	if (KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, uiSoundBoards, 0, &uiHandle) != KERNAL_DEVICE_NOT_FOUND)
	{
		return 5;
	}
	if (KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, 2, &uiHandle) != KERNAL_DEVICE_BAD_UNIT)
	{
		return 6;
	}

	//The sound board opens (and shows open); it can't be read; the handle closes and can't be used again.
	if (KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_LEFT, &uiHandle) != KERNAL_DEVICE_SUCCESS)
	{
		return 7;
	}
	if (!KernalDeviceIsOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0))
	{
		return 8;
	}
	uiDone = 1;
	if ((KernalDeviceRead(uiHandle, 0, &sInfo, 1, &uiDone) != KERNAL_DEVICE_NOT_SUPPORTED) || (uiDone != 0))
	{
		return 9;
	}
	if ((KernalDeviceClose(uiHandle) != KERNAL_DEVICE_SUCCESS) || (KernalDeviceClose(uiHandle) != KERNAL_DEVICE_BAD_HANDLE))
	{
		return 10;
	}

	//The program has the whole sound board while either unit is open; a write says how much it wrote.
	if ((KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_LEFT, &uiHandle) != KERNAL_DEVICE_SUCCESS) ||
		(KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_RIGHT, &uiRight) != KERNAL_DEVICE_SUCCESS))
	{
		return 11;
	}
	if ((KernalDeviceWrite(uiRight, 0, &sInfo, 16, &uiDone) != KERNAL_DEVICE_SUCCESS) || (uiDone != 16))
	{
		return 12;
	}
	if ((KernalDeviceClose(uiHandle) != KERNAL_DEVICE_SUCCESS) || !KernalDeviceIsOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0))
	{
		return 13;
	}
	if ((KernalDeviceClose(uiRight) != KERNAL_DEVICE_SUCCESS) || KernalDeviceIsOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0))
	{
		return 14;
	}

	//While it has the left unit, another process (itself, BootChild) can open neither unit of the first sound board,
	//but can open a second sound board if there is one.
	if (KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_LEFT, &uiHandle) != KERNAL_DEVICE_SUCCESS)
	{
		return 15;
	}
	uiExitCode = 1;
	if ((KernalExecute(BOOT_SELF, (uiSoundBoards > 1) ? BOOT_CHILD_TWO : BOOT_CHILD, &uiExitCode) != KERNAL_RESULT_EXITED) || (uiExitCode != 0))
	{
		return 16;
	}
	if ((KernalDeviceClose(uiHandle) != KERNAL_DEVICE_SUCCESS) || KernalDeviceIsOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0))
	{
		return 17;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The free banks of main memory, or 0 if the memory call failed.
//
//////////////////////////////////////////////////////////////////////////
uint16 BootFreeBanks(void)
{
	SKernalMemoryInfo	sInfo;

	if (KernalMemoryInfo(&sInfo) != KERNAL_RESULT_SUCCESS)
	{
		return 0;
	}
	return sInfo.uiFreeBanks;
}


//////////////////////////////////////////////////////////////////////////
//
// Checks the heap's block and the map and unmap banks calls: banks are
// mapped and given back, all or none, never main memory's last free one,
// and only the heap's.  0 if they do what they should, or which check
// failed.  It gives back all it mapped.
//
//////////////////////////////////////////////////////////////////////////
uint16 BootCheckMemory(void)
{
	SKernalHeapInfo*	psHeap;
	volatile uint8*		puiBank;
	uint16				uiFree;

	psHeap = (SKernalHeapInfo*)KERNAL_HEAP_INFO_ADDRESS;
	if ((psHeap->uiMarker != KERNAL_HEAP_MARKER) || (psHeap->uiHeapStart != KERNAL_HEAP_START) || (psHeap->uiHeapEnd != KERNAL_HEAP_END))
	{
		return 1;
	}
	uiFree = BootFreeBanks();
	if (uiFree < 4)
	{
		return 2;
	}

	//Two banks, which hold what is written to them.
	if ((KernalMapBanks((void*)BOOT_BANKS, 2) != KERNAL_RESULT_SUCCESS) || (BootFreeBanks() != uiFree - 2))
	{
		return 3;
	}
	puiBank = (volatile uint8*)BOOT_BANKS;
	puiBank[0] = 0x5A;
	puiBank[0x1FFFF] = 0xA5;
	if ((puiBank[0] != 0x5A) || (puiBank[0x1FFFF] != 0xA5))
	{
		return 4;
	}

	//Banks it has are left as they are: one more.
	if ((KernalMapBanks((void*)(BOOT_BANKS + KERNAL_HEAP_BANK_SIZE), 2) != KERNAL_RESULT_SUCCESS) || (BootFreeBanks() != uiFree - 3) || (puiBank[0x1FFFF] != 0xA5))
	{
		return 5;
	}

	//Not the heap's: the tables', part way into a bank, below or past the heap, or no banks.
	if ((KernalMapBanks((void*)KERNAL_MATH_INVERT_ADDRESS, 1) != KERNAL_RESULT_BAD_ADDRESS) ||
		(KernalMapBanks((void*)(KERNAL_MATH_MULT_ADDRESS - KERNAL_HEAP_BANK_SIZE), 2) != KERNAL_RESULT_BAD_ADDRESS) ||
		(KernalMapBanks((void*)(BOOT_BANKS + 0x100), 1) != KERNAL_RESULT_BAD_ADDRESS) ||
		(KernalMapBanks((void*)(KERNAL_HEAP_START - KERNAL_HEAP_BANK_SIZE), 1) != KERNAL_RESULT_BAD_ADDRESS) ||
		(KernalMapBanks((void*)(KERNAL_HEAP_END - KERNAL_HEAP_BANK_SIZE), 2) != KERNAL_RESULT_BAD_ADDRESS) ||
		(KernalMapBanks((void*)BOOT_MORE_BANKS, 0) != KERNAL_RESULT_BAD_ADDRESS) ||
		(KernalUnmapBanks((void*)KERNAL_MATH_MULT_ADDRESS, 1) != KERNAL_RESULT_BAD_ADDRESS) ||
		(BootFreeBanks() != uiFree - 3))
	{
		return 6;
	}

	//All of the rest would take the last free bank, so none are mapped; all but it are.
	if ((KernalMapBanks((void*)BOOT_MORE_BANKS, uiFree - 3) != KERNAL_RESULT_NO_MEMORY) || (BootFreeBanks() != uiFree - 3))
	{
		return 7;
	}
	if ((KernalMapBanks((void*)BOOT_MORE_BANKS, uiFree - 4) != KERNAL_RESULT_SUCCESS) || (BootFreeBanks() != 1))
	{
		return 8;
	}
	if ((KernalMapBanks((void*)(BOOT_MORE_BANKS + (uint32)(uiFree - 4) * KERNAL_HEAP_BANK_SIZE), 1) != KERNAL_RESULT_NO_MEMORY) || (BootFreeBanks() != 1))
	{
		return 9;
	}

	//Given back, those it hasn't too.
	if ((KernalUnmapBanks((void*)BOOT_MORE_BANKS, uiFree - 3) != KERNAL_RESULT_SUCCESS) || (BootFreeBanks() != uiFree - 3))
	{
		return 10;
	}
	if ((KernalUnmapBanks((void*)BOOT_BANKS, 3) != KERNAL_RESULT_SUCCESS) || (BootFreeBanks() != uiFree))
	{
		return 11;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The child, while the parent has the first sound board's left unit:
// neither of its units can be opened (KERNAL_DEVICE_IN_USE), as the
// parent has the whole board, but, if bSecondBoard, the second sound
// board's can.  0 if so, or which check failed.
//
//////////////////////////////////////////////////////////////////////////
uint16 BootChild(bool bSecondBoard)
{
	uint16	uiHandle;

	if (KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_LEFT, &uiHandle) != KERNAL_DEVICE_IN_USE)
	{
		return 1;
	}
	if (KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 0, SOUND_BOARD_RIGHT, &uiHandle) != KERNAL_DEVICE_IN_USE)
	{
		return 2;
	}
	if (bSecondBoard)
	{
		if ((KernalDeviceOpen(SOUND_BOARD_MANUFACTURER, SOUND_BOARD_NAME, 1, SOUND_BOARD_RIGHT, &uiHandle) != KERNAL_DEVICE_SUCCESS) ||
			(KernalDeviceClose(uiHandle) != KERNAL_DEVICE_SUCCESS))
		{
			return 3;
		}
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Calls itself, each time with BOOT_FRAME more of the stack written to,
// and makes a kernal call each time: the kernal ends the process at the
// first call after the stack has gone over its canary.
//
//////////////////////////////////////////////////////////////////////////
uint16 BootOverflow(uint16 uiDepth)
{
	uint8	auiFrame[BOOT_FRAME];

	memset(auiFrame, (uint8)uiDepth, BOOT_FRAME);
	KernalGetArgument();
	return (uint16)(BootOverflow((uint16)(uiDepth + 1)) + auiFrame[uiDepth & (BOOT_FRAME - 1)]);
}


//////////////////////////////////////////////////////////////////////////
//
// Children that the kernal ends at their next kernal call: one whose
// stack grows past its room, one that writes through a NULL pointer.  0
// if they were, or which wasn't.
//
//////////////////////////////////////////////////////////////////////////
uint16 BootCheckCanaries(void)
{
	uint16	uiExitCode;

	uiExitCode = 1;
	if ((KernalExecute(BOOT_SELF, BOOT_STACK, &uiExitCode) != KERNAL_RESULT_STACK_OVERFLOW) || (uiExitCode != 0))
	{
		return 1;
	}
	uiExitCode = 1;
	if ((KernalExecute(BOOT_SELF, BOOT_NULL, &uiExitCode) != KERNAL_RESULT_NULL_WRITE) || (uiExitCode != 0))
	{
		return 2;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The boot test's start up program: loads the maths tables from the SD card into main
// memory (MULT, and INVERT's upper half; it is built without them,
// MathTables="false", so it can),
// multiplies and divides and gives the kernal a value, checks the device
// calls (BootCheckDevices), the heap's banks (BootCheckMemory) and the canaries (BootCheckCanaries), then runs modplay.exe (built with the tables)
// with its own argument and exits with modplay's exit code (or 100 and
// why it couldn't run).  Run with BOOT_CHILD or BOOT_CHILD_TWO it is
// BootChild; with BOOT_STACK, BootOverflow; with BOOT_NULL it writes
// through a NULL pointer and makes a kernal call.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiResult;
	uint16	uiExitCode;

	if (KernalGetArgument() == BOOT_CHILD)
	{
		return BootChild(false);
	}
	if (KernalGetArgument() == BOOT_CHILD_TWO)
	{
		return BootChild(true);
	}
	if (KernalGetArgument() == BOOT_STACK)
	{
		return BootOverflow(0);
	}
	if (KernalGetArgument() == BOOT_NULL)
	{
		gpuiNull[3] = 0x42;
		KernalGetArgument();
		return 1;
	}

	uiResult = KernalFileLoad(KERNAL_MATH_MULT_PATH, (void*)KERNAL_MATH_MULT_ADDRESS, KERNAL_MATH_MULT_SIZE);
	if (uiResult != KERNAL_RESULT_SUCCESS)
	{
		return BOOT_NO_TABLES + uiResult;
	}
	uiResult = KernalFileRead(KERNAL_MATH_INVERT_PATH, KERNAL_MATH_INVERT_OFFSET, (void*)KERNAL_MATH_INVERT_ADDRESS, KERNAL_MATH_INVERT_SIZE);
	if (uiResult != KERNAL_RESULT_SUCCESS)
	{
		return BOOT_NO_TABLES + uiResult;
	}

	guiProduct = (uint32)guiA * guiB;
	KernalPutValue((uint16)(guiProduct / 3));

	uiResult = BootCheckDevices();
	if (uiResult != 0)
	{
		return BOOT_DEVICES + uiResult;
	}

	uiResult = BootCheckMemory();
	if (uiResult != 0)
	{
		return BOOT_MEMORY + uiResult;
	}

	uiResult = BootCheckCanaries();
	if (uiResult != 0)
	{
		return BOOT_CANARIES + uiResult;
	}

	uiExitCode = 0;
	uiResult = KernalExecute(BOOT_PROGRAM, KernalGetArgument(), &uiExitCode);
	if (uiResult != KERNAL_RESULT_EXITED)
	{
		return 100 + uiResult;
	}
	return uiExitCode;
}
