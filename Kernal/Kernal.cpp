#include <string.h>
#include "BaseLib/FatVolume.h"
#include "BaseLib/TrueMallocator.h"
#include "BaseLib/GlobalMemory.h"
#include "KernalDevices.h"
#include "KernalSDCard.h"
#include "KernalProcess.h"
#include "Kernal.h"
#include "KernalInterrupts.h"
#include "KernalDriver.h"
#include "KernalSoundBoard.h"
#include "KernalDebugPort.h"


SKernalStatus	gsKernalStatus = { KERNAL_STATE_STARTING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
//The drivers in the ROM, for the HCP65's own boards: one board of each (a second has no driver).
CKernalSDCardBoard	gcKernalSDCardBoard;
CKernalSoundBoard	gcKernalSoundBoard;
CKernalDebugPort	gcKernalDebugPort;
uint16			guiKernalBootArgument = KERNAL_NO_BOOT_ARGUMENT;	//Not 0: zero initialised globals are bss, not in the ROM.
CFatVolume		gcKernalVolume;


//Libc65's heap.
extern CTrueMallocator	gcHeap;
extern bool				gbHeap;


//////////////////////////////////////////////////////////////////////////
//
// Called by the start up before the init table: the kernal's heap is main
// memory on the 4MB card (which is always there), where the SD Card Board
// can reach it too.
//
//////////////////////////////////////////////////////////////////////////
extern "C" void KernalInitHeap(void)
{
	gcHeap.Init((void*)KERNAL_HEAP_START, KERNAL_HEAP_END - KERNAL_HEAP_START);
	gbHeap = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The ROM's driver for szName (an HCP65 board), NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
static CKernalDriver* KernalBuiltInDriver(const char* szName)
{
	if (strcmp(szName, KERNAL_SD_CARD_NAME) == 0)
	{
		return &gcKernalSDCardBoard;
	}
	if (strcmp(szName, KERNAL_SOUND_BOARD_NAME) == 0)
	{
		return &gcKernalSoundBoard;
	}
	if (strcmp(szName, KERNAL_DEBUG_PORT_NAME) == 0)
	{
		return &gcKernalDebugPort;
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Starts the ROM's driver of each present HCP65 board that has one, the
// first board of each kind.  The rest stay in reset.
//
//////////////////////////////////////////////////////////////////////////
static void KernalStartBuiltInDrivers(void)
{
	SKernalDeviceROM*	psROM;
	CKernalDriver*		pcDriver;
	uint16				uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		psROM = gasKernalDevices[uiSlot].psROM;
		if (psROM && (strcmp(psROM->szManufacturer, KERNAL_DEVICE_MANUFACTURER) == 0))
		{
			pcDriver = KernalBuiltInDriver(psROM->szName);
			if (pcDriver && !KernalFindDriver(KERNAL_DEVICE_MANUFACTURER, psROM->szName, 0))
			{
				KernalStartDriver(pcDriver, uiSlot);
			}
		}
	}
	gpcKernalDebugPort = (CKernalDebugPort*)KernalFindDriver(KERNAL_DEVICE_MANUFACTURER, KERNAL_DEBUG_PORT_NAME, 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Called by the start up once MPU RAM holds the kernal and its globals are
// initialised.  Running in kernel mode, native, with interrupts disabled.
// Finds the devices, brings up the SD card, mounts its FAT32 volume and
// runs boot.exe from it.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	CKernalSDCardBoard*	pcSDCardBoard;
	EFatCode			eResult;
	uint16				uiSlot;
	uint8				uiSocket;
	CKernalSDCard*		pcCard;

	//The allocators a chunk file (an executable) names are looked up here.  Only the system allocator (malloc): BaseLib's
	//global memory would be too big for the ROM.
	gcMallocators.InitSystem();
	gcConstructors.Init();

	KernalScanDevices();
	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		if (gasKernalDevices[uiSlot].uiState == KERNAL_DEVICE_PRESENT)
		{
			gsKernalStatus.uiDevices |= (uint32)1 << uiSlot;
		}
		else if (gasKernalDevices[uiSlot].uiState != KERNAL_DEVICE_NONE)
		{
			gsKernalStatus.uiBadDevices |= (uint32)1 << uiSlot;
		}
	}
	gsKernalStatus.uiState = KERNAL_STATE_DEVICES_SCANNED;
	KernalInitInterrupts();
	KernalStartBuiltInDrivers();

	//The SD Card Board's driver brought up each socket's card; the first with a volume that mounts is used.
	pcSDCardBoard = (CKernalSDCardBoard*)KernalFindDriver(KERNAL_DEVICE_MANUFACTURER, KERNAL_SD_CARD_NAME, 0);
	if (!pcSDCardBoard)
	{
		gsKernalStatus.uiState = KERNAL_STATE_NO_SD_CARD_BOARD;
		return 1;
	}
	gsKernalStatus.uiSDCards = pcSDCardBoard->GetReadyCards();
	if (gsKernalStatus.uiSDCards == 0)
	{
		gsKernalStatus.uiSDError = pcSDCardBoard->GetCard(0)->GetError();
		gsKernalStatus.uiState = KERNAL_STATE_SD_CARD_FAILED;
		return 1;
	}
	gsKernalStatus.uiSDError = KERNAL_SD_SUCCESS;

	eResult = FAT_INVALID_FAT_VOLUME;
	pcCard = NULL;
	for (uiSocket = 0; uiSocket < KERNAL_SD_NUM_SOCKETS; uiSocket++)
	{
		if (gsKernalStatus.uiSDCards & (1 << uiSocket))
		{
			pcCard = pcSDCardBoard->GetCard(uiSocket);
			eResult = gcKernalVolume.Mount(pcCard);
			if (eResult == FAT_SUCCESS)
			{
				break;
			}
			gcKernalVolume.Unmount();
		}
	}
	gsKernalStatus.uiFatCode = (uint16)eResult;
	if (eResult != FAT_SUCCESS)
	{
		gsKernalStatus.uiSDError = pcCard->GetError();
		gsKernalStatus.uiState = KERNAL_STATE_MOUNT_FAILED;
		return 1;
	}
	gsKernalStatus.uiSDSocket = uiSocket;
	gsKernalStatus.uiNumSectors = (uint32)(pcCard->GetTotalSectors() + 1);
	gsKernalStatus.uiFileSystem = (uint16)gcKernalVolume.GetFileSystemType();
	gsKernalStatus.uiNumClusters = gcKernalVolume.GetNumClusters();
	gsKernalStatus.uiFreeClusters = gcKernalVolume.GetTotalFreeClusters();
	gsKernalStatus.uiState = KERNAL_STATE_MOUNTED;

	gsKernalStatus.uiBootArgument = (guiKernalBootArgument == KERNAL_NO_BOOT_ARGUMENT) ? 0 : guiKernalBootArgument;
	gsKernalStatus.uiBootResult = KernalRunExecutable(KERNAL_BOOT_FILE, gsKernalStatus.uiBootArgument, &gsKernalStatus.uiExitCode);
	gsKernalStatus.uiState = (gsKernalStatus.uiBootResult == KERNAL_PROCESS_EXITED) ? KERNAL_STATE_BOOT_EXITED : KERNAL_STATE_BOOT_FAILED;
	return 0;
}
