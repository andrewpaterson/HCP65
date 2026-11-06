#include "BaseLib/FatVolume.h"
#include "BaseLib/TrueMallocator.h"
#include "BaseLib/GlobalMemory.h"
#include "KernalDevices.h"
#include "KernalSDCard.h"
#include "KernalProcess.h"
#include "Kernal.h"
#include "KernalInterrupts.h"


SKernalStatus	gsKernalStatus = { KERNAL_STATE_STARTING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
CKernalSDCard	gacKernalSDCards[KERNAL_SD_NUM_SOCKETS];
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
// Called by the start up once MPU RAM holds the kernal and its globals are
// initialised.  Running in kernel mode, native, with interrupts disabled.
// Finds the devices, brings up the SD card, mounts its FAT32 volume and
// runs boot.exe from it.
//
//////////////////////////////////////////////////////////////////////////
int main(void)
{
	SKernalDevice*	psSDCardBoard;
	EFatCode		eResult;
	uint16			uiSlot;
	uint8			uiSocket;
	uint16			uiError;
	CKernalSDCard*	pcCard;

	//The allocators a chunk file (an executable) names are looked up here.  Only the system allocator (malloc): BaseLib's
	//global memory would be too big for the ROM.
	gcMallocators.InitSystem();
	gcConstructors.Init();

	KernalScanDevices();
	gsKernalStatus.uiState = KERNAL_STATE_DEVICES_SCANNED;
	KernalInitInterrupts();

	psSDCardBoard = KernalFindDevice(KERNAL_SD_CARD_NAME);
	if (!psSDCardBoard)
	{
		gsKernalStatus.uiState = KERNAL_STATE_NO_SD_CARD_BOARD;
		return 1;
	}
	uiSlot = (uint16)(psSDCardBoard - gasKernalDevices);
	KernalEnableDevice(uiSlot);

	//Each socket's card is brought up; the first with a volume that mounts is used.
	gsKernalStatus.uiSDCards = 0;
	for (uiSocket = 0; uiSocket < KERNAL_SD_NUM_SOCKETS; uiSocket++)
	{
		uiError = gacKernalSDCards[uiSocket].Init(psSDCardBoard, uiSlot, uiSocket);
		if (uiError == KERNAL_SD_SUCCESS)
		{
			gsKernalStatus.uiSDCards |= (uint16)(1 << uiSocket);
		}
		else if (uiSocket == 0)
		{
			gsKernalStatus.uiSDError = uiError;
		}
	}
	if (gsKernalStatus.uiSDCards == 0)
	{
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
			pcCard = &gacKernalSDCards[uiSocket];
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
