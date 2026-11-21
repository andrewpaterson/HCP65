#include <string.h>
#include "BaseLib/FatFile.h"
#include "BaseLib/BankMallocator.h"
#include "KernalDevices.h"
#include "KernalProcess.h"
#include "Kernal.h"
#include "KernalDriver.h"
#include "KernalDriverLoader.h"
#include "KernalMemory.h"
#include "KernalStartUp.h"
#include "KernalConsole.h"
#include "KernalScreen.h"


//Libc65's heap.
extern CBankMallocator	gcHeap;


//////////////////////////////////////////////////////////////////////////
//
// Loads the driver of each present board that has none, if its device ROM
// names one, from the SD card's \OS\Drivers.
//
//////////////////////////////////////////////////////////////////////////
static void KernalLoadDrivers(void)
{
	SKernalDeviceROM*	psROM;
	uint16				uiSlot;
	uint16				uiResult;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		psROM = gasKernalDevices[uiSlot].psROM;
		if (psROM && (psROM->szDriverFile[0] != '\0') && !KernalGetDriver(uiSlot))
		{
			uiResult = KernalLoadDriver(uiSlot);
			if (uiResult == KERNAL_LOAD_SUCCESS)
			{
				gsKernalStatus.uiLoadedDrivers |= (uint32)1 << uiSlot;
			}
			else
			{
				gsKernalStatus.uiDriverError = uiResult;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The start up program and its argument: STARTUP.TXT's first line, a path
// and then (after spaces) a decimal number, or KERNAL_DEFAULT_STARTUP and
// 0 without the file.  szPath holds KERNAL_STARTUP_LENGTH.
//
//////////////////////////////////////////////////////////////////////////
static void KernalReadStartUp(char* szPath, uint16* puiArgument)
{
	CFatFile	cFile;
	char		szLine[KERNAL_STARTUP_LENGTH];
	lsize		uiRead;
	char*		pc;
	uint16		uiLength;

	strcpy(szPath, KERNAL_DEFAULT_STARTUP);
	*puiArgument = 0;
	cFile.Init(&gcKernalVolume);
	if (cFile.Open(KERNAL_STARTUP_FILE, FAT_FILE_ACCESS_READ) != FAT_SUCCESS)
	{
		return;
	}
	uiRead = 0;
	if (cFile.Read((uint8*)szLine, KERNAL_STARTUP_LENGTH - 1, &uiRead) != FAT_SUCCESS)
	{
		uiRead = 0;
	}
	cFile.Close();
	szLine[uiRead] = '\0';

	//The first line, without the spaces around it.
	pc = szLine;
	while ((*pc == ' ') || (*pc == '\t'))
	{
		pc++;
	}
	uiLength = 0;
	while ((pc[uiLength] != '\0') && (pc[uiLength] != '\r') && (pc[uiLength] != '\n') && (pc[uiLength] != ' ') && (pc[uiLength] != '\t'))
	{
		uiLength++;
	}
	if (uiLength == 0)
	{
		return;
	}
	memcpy(szPath, pc, uiLength);
	szPath[uiLength] = '\0';

	pc += uiLength;
	while ((*pc == ' ') || (*pc == '\t'))
	{
		pc++;
	}
	while ((*pc >= '0') && (*pc <= '9'))
	{
		*puiArgument = (uint16)(*puiArgument * 10 + (*pc - '0'));
		pc++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// OS.SYS's main: the boot loader jumped to it (Start.asm), in kernel mode,
// native, with interrupts disabled, and the start up has initialised its
// globals.  Starts the devices and the SD card again, loads each board's
// driver from \OS\Drivers and runs the start up program, with the
// console (CKernalConsole) behind its standard handles.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	char	szStartUp[KERNAL_STARTUP_LENGTH];
	uint16	uiArgument;

	if (!KernalStartUp())
	{
		return 1;
	}
	KernalLoadDrivers();
	gcKernalConsole.Init();
	gcKernalScreen.Init();

	KernalReadStartUp(szStartUp, &uiArgument);
	gsKernalStatus.uiBootArgument = uiArgument;
	gsKernalStatus.uiBootResult = KernalRunExecutable(szStartUp, uiArgument, &gsKernalStatus.uiExitCode);
	gsKernalStatus.uiHeapPeak = (uint32)gcHeap.PeakBytes();
	gsKernalStatus.uiMainHeapPeak = KernalMainHeapPeak();
	gsKernalStatus.uiState = (gsKernalStatus.uiBootResult == KERNAL_PROCESS_EXITED) ? KERNAL_STATE_BOOT_EXITED : KERNAL_STATE_BOOT_FAILED;
	return 0;
}
