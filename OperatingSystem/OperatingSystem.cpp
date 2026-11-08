#include <string.h>
#include "HCP65User.h"
#include "HCP65SoundBoard.h"


#define BOOT_PROGRAM	"\\modplay.exe"
#define BOOT_SELF		"\\boot.exe"
#define BOOT_CHILD		0xB0C1	//The argument it runs itself with as the child (BootChild) when there is one sound board.
#define BOOT_CHILD_TWO	0xB0C2	//The child's argument when there is a second sound board, which it opens too.
#define BOOT_NO_TABLES	200		//The exit code when the maths tables couldn't be loaded, plus why.
#define BOOT_DEVICES	300		//The exit code when a device call didn't do what it should, plus which.
#define BOOT_MAX_BOARDS	20		//More boards than there are device sockets.


//In globals so the compiler can't work the answers out itself.
uint16	guiA = 300;
uint16	guiB = 200;
uint32	guiProduct;


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

	//A handle that isn't open, a board the kernal keeps (the SD Card Board boot.exe was loaded from, which every HCP65
	//has, unlike the emulator's Debug Port), one there isn't and a unit there isn't.
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
// The test's boot.exe: loads the maths tables from the SD card into main
// memory (MULT, and INVERT's upper half; it is built without them,
// MathTables="false", so it can),
// multiplies and divides and gives the kernal a value, checks the device
// calls (BootCheckDevices), then runs modplay.exe (built with the tables)
// with its own argument and exits with modplay's exit code (or 100 and
// why it couldn't run).  Run with BOOT_CHILD or BOOT_CHILD_TWO it is
// BootChild.
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

	uiExitCode = 0;
	uiResult = KernalExecute(BOOT_PROGRAM, KernalGetArgument(), &uiExitCode);
	if (uiResult != KERNAL_RESULT_EXITED)
	{
		return 100 + uiResult;
	}
	return uiExitCode;
}
