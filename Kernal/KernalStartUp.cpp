#include <string.h>
#include "BaseLib/TrueMallocator.h"
#include "BaseLib/GlobalMemory.h"
#include "KernalDevices.h"
#include "KernalDriver.h"
#include "KernalInterrupts.h"
#include "KernalMemory.h"
#include "Kernal.h"
#include "KernalStartUp.h"


//The built in drivers: up to KERNAL_SD_MAX_BOARDS SD Card Boards.
CKernalSDCardBoard	gacKernalSDCardBoards[KERNAL_SD_MAX_BOARDS];
CFatVolume			gcKernalVolume;


//////////////////////////////////////////////////////////////////////////
//
// The built in driver for the uiIndex'th board called szName (an HCP65
// board), NULL if there isn't one for it.
//
//////////////////////////////////////////////////////////////////////////
static CKernalDriver* KernalBuiltInDriver(const char* szName, uint16 uiIndex)
{
	if ((strcmp(szName, KERNAL_SD_CARD_NAME) == 0) && (uiIndex < KERNAL_SD_MAX_BOARDS))
	{
		return &gacKernalSDCardBoards[uiIndex];
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// How many boards made by szManufacturer called szName have drivers.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalNumDrivers(const char* szManufacturer, const char* szName)
{
	uint16	uiCount;

	uiCount = 0;
	while (KernalFindDriver(szManufacturer, szName, uiCount))
	{
		uiCount++;
	}
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// Starts the built in driver of each present HCP65 board there is one
// for.  The rest stay in reset (their drivers may be on the SD card).
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
			pcDriver = KernalBuiltInDriver(psROM->szName, KernalNumDrivers(KERNAL_DEVICE_MANUFACTURER, psROM->szName));
			if (pcDriver)
			{
				KernalStartDriver(pcDriver, uiSlot);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Called once the image's globals are initialised, in kernel mode, native,
// with interrupts disabled.  The status is cleared, then each step that
// works says so in it.  True with the card's volume mounted; false, its
// state saying where it stopped, if there is no main memory, SD Card
// Board, card or volume.
//
//////////////////////////////////////////////////////////////////////////
bool KernalStartUp(void)
{
	CKernalSDCardBoard*	pcSDCardBoard;
	EFatCode			eResult;
	uint16				uiSlot;
	uint8				uiSocket;
	CKernalSDCard*		pcCard;

	memset(&gsKernalStatus, 0, sizeof(SKernalStatus));
	gsKernalStatus.uiState = KERNAL_STATE_STARTING;

	//The allocators a chunk file (an executable) names are looked up here.  Only the system allocator (malloc): BaseLib's
	//global memory would be too big.
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

	//Main memory is the memory boards': without it the SD Card Board has nothing to read into.
	if (!KernalInitMainMemory())
	{
		gsKernalStatus.uiMainBanks = KernalNumMainBanks();
		gsKernalStatus.uiState = KERNAL_STATE_NO_MAIN_MEMORY;
		return false;
	}
	gsKernalStatus.uiMainBanks = KernalNumMainBanks();
	KernalInitInterrupts();
	KernalStartBuiltInDrivers();

	//The SD Card Board's driver brought up each socket's card; the first with a volume that mounts is used.
	pcSDCardBoard = (CKernalSDCardBoard*)KernalFindDriver(KERNAL_DEVICE_MANUFACTURER, KERNAL_SD_CARD_NAME, 0);
	if (!pcSDCardBoard)
	{
		gsKernalStatus.uiState = KERNAL_STATE_NO_SD_CARD_BOARD;
		return false;
	}
	gsKernalStatus.uiSDCards = pcSDCardBoard->GetReadyCards();
	if (gsKernalStatus.uiSDCards == 0)
	{
		gsKernalStatus.uiSDError = pcSDCardBoard->GetCard(0)->GetError();
		gsKernalStatus.uiState = KERNAL_STATE_SD_CARD_FAILED;
		return false;
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
		return false;
	}
	gsKernalStatus.uiSDSocket = uiSocket;
	gsKernalStatus.uiNumSectors = (uint32)(pcCard->GetTotalSectors() + 1);
	gsKernalStatus.uiFileSystem = (uint16)gcKernalVolume.GetFileSystemType();
	gsKernalStatus.uiNumClusters = gcKernalVolume.GetNumClusters();
	gsKernalStatus.uiFreeClusters = gcKernalVolume.GetTotalFreeClusters();
	gsKernalStatus.uiState = KERNAL_STATE_MOUNTED;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Puts every board with a driver back in reset, its interrupt handler
// gone: as the next stage expects to find them.
//
//////////////////////////////////////////////////////////////////////////
void KernalStopDrivers(void)
{
	CKernalDriver*	pcDriver;
	uint16			uiSlot;

	for (uiSlot = 0; uiSlot < KERNAL_NUM_DEVICE_SLOTS; uiSlot++)
	{
		pcDriver = KernalGetDriver(uiSlot);
		if (pcDriver)
		{
			pcDriver->Kill();
		}
	}
}
