#include <string.h>
#include "BaseLib/FatFile.h"
#include "KernalInterrupts.h"
#include "Kernal.h"
#include "KernalStartUp.h"


//OS.SYS's first page: bank 0's stubs and vectors, copied to KERNAL_OS_ADDRESS last.
#define BOOT_FIRST_PAGE_SIZE	0x100
#define BOOT_JML				0x5C		//OS.SYS's first byte: ResetStub's jml to its start up.
#define BOOT_OS_BANKS_ADDRESS	0x010000	//The rest of OS.SYS, from bank 1.


typedef void	(*BootEntry)(void);


static uint8	gauiBootFirstPage[BOOT_FIRST_PAGE_SIZE];


//////////////////////////////////////////////////////////////////////////
//
// Reads \OS\OS.SYS: its first page into gauiBootFirstPage and the rest
// to bank 1 onwards, where nothing of the boot loader is.  Returns 0, or
// KERNAL_OS_* for what was wrong with it.
//
//////////////////////////////////////////////////////////////////////////
static uint16 BootReadOS(void)
{
	CFatFile	cFile;
	uint32		uiSize;
	lsize		uiRead;
	bool		bRead;

	cFile.Init(&gcKernalVolume);
	if (cFile.Open(KERNAL_OS_FILE, FAT_FILE_ACCESS_READ) != FAT_SUCCESS)
	{
		return KERNAL_OS_NOT_FOUND;
	}
	uiSize = cFile.Size();
	if ((uiSize <= BOOT_FIRST_PAGE_SIZE) || (uiSize > KERNAL_OS_MAX_SIZE))
	{
		cFile.Close();
		return KERNAL_OS_BAD_SIZE;
	}

	uiRead = 0;
	bRead = (cFile.Read(gauiBootFirstPage, BOOT_FIRST_PAGE_SIZE, &uiRead) == FAT_SUCCESS) && (uiRead == BOOT_FIRST_PAGE_SIZE);
	if (bRead)
	{
		uiRead = 0;
		bRead = (cFile.Read((uint8*)BOOT_OS_BANKS_ADDRESS, uiSize - BOOT_FIRST_PAGE_SIZE, &uiRead) == FAT_SUCCESS) && (uiRead == uiSize - BOOT_FIRST_PAGE_SIZE);
	}
	cFile.Close();
	if (!bRead)
	{
		return KERNAL_OS_READ_FAILED;
	}
	if (gauiBootFirstPage[0] != BOOT_JML)
	{
		return KERNAL_OS_BAD_ENTRY;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The boot loader's main, from the start up (BootStart.asm) in MPU RAM
// banks 4 and 5: native, in kernel mode, with interrupts disabled.  Brings
// up the devices and the SD card as the kernal does (KernalStartUp), reads
// OS.SYS into MPU RAM and jumps to it with every board back in reset and
// interrupts stopped.  Returns (to idle) if there is no card, volume or
// OS, the status saying why.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	uint16	uiResult;

	if (!KernalStartUp())
	{
		return 1;
	}
	uiResult = BootReadOS();
	if (uiResult != 0)
	{
		gsKernalStatus.uiBootResult = uiResult;
		gsKernalStatus.uiState = KERNAL_STATE_NO_OS;
		return 1;
	}

	//Nothing may interrupt once the vectors start to change: then OS.SYS is all in place.
	KernalStopDrivers();
	KernalStopInterrupts();
	gsKernalStatus.uiState = KERNAL_STATE_OS_STARTED;
	memcpy((void*)KERNAL_OS_ADDRESS, gauiBootFirstPage, BOOT_FIRST_PAGE_SIZE);
	((BootEntry)KERNAL_OS_ADDRESS)();
	return 0;
}
