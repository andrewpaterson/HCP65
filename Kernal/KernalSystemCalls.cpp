#include <string.h>
#include "BaseLib/FatFile.h"
#include "BaseLib/TrueMallocator.h"
#include "UserLib/HCP65User.h"
#include "KernalMemory.h"
#include "KernalProcess.h"
#include "KernalStartUp.h"
#include "KernalSystemCalls.h"


#define KERNAL_SYSTEM_CALL_RESULT	16		//SKernalSystemCall's uiResult.
#define KERNAL_SYSTEM_PATH_LENGTH	128		//The longest path a system call takes.
#define KERNAL_SYSTEM_COPY_SIZE		512		//What a file write moves from the process at a time.


//Libc65's heap: the kernal's, MPU RAM's bank 6.
extern CTrueMallocator	gcHeap;


static uint8	gauiKernalSystemBuffer[KERNAL_SYSTEM_COPY_SIZE];


//////////////////////////////////////////////////////////////////////////
//
// The uiIndex'th entry of the directory szPath (without "." and ".."), to
// the running process's memory at uiAddress.  KERNAL_RESULT_END after the
// last; KERNAL_RESULT_NOT_FOUND if it isn't a directory.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalDirectoryCall(const char* szPath, uint16 uiIndex, uint32 uiAddress)
{
	SFatFileSystemQuery		sQuery;
	SFatDirectoryEntry*		psEntry;
	SKernalDirectoryEntry	sEntry;
	EFatCode				eResult;
	uint16					uiResult;
	const char*				szName;

	sQuery.Init();
	uiResult = KERNAL_RESULT_NOT_FOUND;
	eResult = gcKernalVolume.FindFirstFATEntry((char*)szPath, 0, &psEntry, &sQuery);
	while (eResult == FAT_SUCCESS)
	{
		szName = (const char*)psEntry->szName;
		if (szName[0] == '\0')
		{
			uiResult = KERNAL_RESULT_END;
			break;
		}
		if ((strcmp(szName, ".") != 0) && (strcmp(szName, "..") != 0) && !(psEntry->uiAttributes & FAT_ATTR_VOLUME_ID))
		{
			if (uiIndex == 0)
			{
				memset(&sEntry, 0, sizeof(SKernalDirectoryEntry));
				strncpy(sEntry.szName, szName, KERNAL_FILE_NAME_LENGTH - 1);
				sEntry.uiAttributes = psEntry->uiAttributes;
				sEntry.uiSize = psEntry->uiSize;
				uiResult = KernalCopyToUser(uiAddress, (uint8*)&sEntry, sizeof(SKernalDirectoryEntry)) ? KERNAL_RESULT_SUCCESS : KERNAL_RESULT_BAD_ADDRESS;
				break;
			}
			uiIndex--;
		}
		eResult = gcKernalVolume.FindNextFATEntry(&psEntry, &sQuery);
	}
	sQuery.Kill(gcKernalVolume.GetSectorCache());
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file szPath made (or emptied) and given the running process's
// uiLength bytes at uiAddress.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalFileWriteCall(const char* szPath, uint32 uiAddress, uint32 uiLength)
{
	CFatFile	cFile;
	uint32		uiChunk;
	uint16		uiResult;

	cFile.Init(&gcKernalVolume);
	if (cFile.Open(szPath, FAT_FILE_ACCESS_WRITE | FAT_FILE_ACCESS_CREATE_OR_OVERWRITE) != FAT_SUCCESS)
	{
		return KERNAL_RESULT_NOT_FOUND;
	}
	uiResult = KERNAL_RESULT_SUCCESS;
	while (uiLength > 0)
	{
		uiChunk = (uiLength < KERNAL_SYSTEM_COPY_SIZE) ? uiLength : KERNAL_SYSTEM_COPY_SIZE;
		if (!KernalCopyFromUser(gauiKernalSystemBuffer, uiAddress, uiChunk))
		{
			uiResult = KERNAL_RESULT_BAD_ADDRESS;
			break;
		}
		if (cFile.Write(gauiKernalSystemBuffer, uiChunk) != FAT_SUCCESS)
		{
			uiResult = KERNAL_RESULT_BAD_FILE;
			break;
		}
		uiAddress += uiChunk;
		uiLength -= uiChunk;
	}
	if (cFile.Close() != FAT_SUCCESS)
	{
		uiResult = KERNAL_RESULT_BAD_FILE;
	}
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The kernal's memory: main memory's banks, and its heap.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalMemoryCall(uint32 uiAddress)
{
	SKernalMemoryInfo	sInfo;

	memset(&sInfo, 0, sizeof(SKernalMemoryInfo));
	sInfo.uiMainBanks = KernalNumMainBanks();
	KernalCountProcessBanks(&sInfo.uiProcessBanks, &sInfo.uiFreeBanks);
	sInfo.uiHeapSize = KERNAL_MPU_HEAP_END - KERNAL_MPU_HEAP_START;
	sInfo.uiHeapUsed = (uint32)gcHeap.AllocatedBytes();
	return KernalCopyToUser(uiAddress, (uint8*)&sInfo, sizeof(SKernalMemoryInfo)) ? KERNAL_RESULT_SUCCESS : KERNAL_RESULT_BAD_ADDRESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The system call whose block is at uiAddress in the running process's
// memory (HCP65User.h's KERNAL_SYSTEM_CALL_*).
//
//////////////////////////////////////////////////////////////////////////
uint16 KernalCallSystem(uint32 uiAddress)
{
	SKernalSystemCall	sCall;
	char				szPath[KERNAL_SYSTEM_PATH_LENGTH];

	if (!KernalCopyFromUser((uint8*)&sCall, uiAddress, sizeof(SKernalSystemCall)))
	{
		return KERNAL_RESULT_BAD_ADDRESS;
	}
	szPath[0] = '\0';
	if (sCall.uiPath && !KernalUserString(sCall.uiPath, szPath, KERNAL_SYSTEM_PATH_LENGTH))
	{
		return KERNAL_RESULT_BAD_ADDRESS;
	}

	switch (sCall.uiCall)
	{
	case KERNAL_SYSTEM_CALL_DIRECTORY:
		return KernalDirectoryCall(szPath, sCall.uiIndex, sCall.uiAddress);

	case KERNAL_SYSTEM_CALL_FILE_WRITE:
		return KernalFileWriteCall(szPath, sCall.uiAddress, sCall.uiLength);

	case KERNAL_SYSTEM_CALL_FILE_DELETE:
		return (gcKernalVolume.Delete(szPath) == FAT_SUCCESS) ? KERNAL_RESULT_SUCCESS : KERNAL_RESULT_NOT_FOUND;

	case KERNAL_SYSTEM_CALL_MAKE_DIRECTORY:
		return (gcKernalVolume.CreateDirectory(szPath) == FAT_SUCCESS) ? KERNAL_RESULT_SUCCESS : KERNAL_RESULT_BAD_FILE;

	case KERNAL_SYSTEM_CALL_MEMORY:
		return KernalMemoryCall(sCall.uiAddress);

	case KERNAL_SYSTEM_CALL_MAP_BANKS:
		return KernalMapHeapBanks(sCall.uiAddress, sCall.uiIndex);

	case KERNAL_SYSTEM_CALL_UNMAP_BANKS:
		return KernalUnmapHeapBanks(sCall.uiAddress, sCall.uiIndex);
	}
	return KERNAL_RESULT_NOT_FOUND;
}
