#include <string.h>
#include "HCP65User.h"


//////////////////////////////////////////////////////////////////////////
//
// A system call's block, cleared, for uiCall on the path szPath.
//
//////////////////////////////////////////////////////////////////////////
static void UserSystemCall(SKernalSystemCall* psCall, uint16 uiCall, const char* szPath)
{
	memset(psCall, 0, sizeof(SKernalSystemCall));
	psCall->uiCall = uiCall;
	psCall->uiPath = (uint32)szPath;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalDirectoryEntry(const char* szPath, uint16 uiIndex, SKernalDirectoryEntry* psEntry)
{
	SKernalSystemCall	sCall;

	UserSystemCall(&sCall, KERNAL_SYSTEM_CALL_DIRECTORY, szPath);
	sCall.uiIndex = uiIndex;
	sCall.uiAddress = (uint32)psEntry;
	return KernalSystemCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalFileWrite(const char* szPath, const void* pvSource, uint32 uiSize)
{
	SKernalSystemCall	sCall;

	UserSystemCall(&sCall, KERNAL_SYSTEM_CALL_FILE_WRITE, szPath);
	sCall.uiAddress = (uint32)pvSource;
	sCall.uiLength = uiSize;
	return KernalSystemCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalFileDelete(const char* szPath)
{
	SKernalSystemCall	sCall;

	UserSystemCall(&sCall, KERNAL_SYSTEM_CALL_FILE_DELETE, szPath);
	return KernalSystemCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalMakeDirectory(const char* szPath)
{
	SKernalSystemCall	sCall;

	UserSystemCall(&sCall, KERNAL_SYSTEM_CALL_MAKE_DIRECTORY, szPath);
	return KernalSystemCall(&sCall);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
extern "C" uint16 KernalMemoryInfo(SKernalMemoryInfo* psInfo)
{
	SKernalSystemCall	sCall;

	UserSystemCall(&sCall, KERNAL_SYSTEM_CALL_MEMORY, NULL);
	sCall.uiAddress = (uint32)psInfo;
	return KernalSystemCall(&sCall);
}
