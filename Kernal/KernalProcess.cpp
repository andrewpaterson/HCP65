#include <stdlib.h>
#include <string.h>
#include "BaseLib/MemoryFile.h"
#include "BaseLib/ChunkFile.h"
#include "BaseLib/FatFile.h"
#include "BaseLib/FatAbstractFile.h"
#include "Assembler65Lib/Asm65Object.h"
#include "KernalDevices.h"
#include "Kernal.h"
#include "KernalProcess.h"
#include "KernalInterrupts.h"
#include "KernalDriver.h"
#include "KernalDeviceCalls.h"
#include "KernalDebugPort.h"
#include "KernalMemory.h"


#define KERNAL_PROCESS_ENTRY_SYMBOL		"UserStart"
#define KERNAL_PROCESS_HEAP_SYMBOL		"UserHeapSize"
#define KERNAL_PROCESS_STATUS			0x30		//The process starts with 8 bit registers and interrupts on.
#define KERNAL_MAX_PATH					128


//Banks every process gets: 0 (its stack and direct page) and $FF (where Libc65 reads its memory size).  Without a
//memory size its heap is banks $08-$0F.
static const uint8 gauiKernalProcessBanks[] = { 0x00, 0xFF };
static const uint8 gauiKernalLowHeapBanks[] = { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };


extern CFatVolume	gcKernalVolume;

SKernalCall			gsKernalCall;
static uint8		guiKernalProcess;							//The process running (or calling), 0 for none.
static uint16		gauiKernalArguments[256];					//Each process ID's argument.
static uint8		gauiKernalBankOwners[KERNAL_NUM_BANKS];					//The process ID a physical bank is given to.
static uint8*			gpuiKernalArguments;					//The call's arguments, on the process's stack in its bank 0.


//////////////////////////////////////////////////////////////////////////
//
// A process's page of the remap table: entry n is the physical bank its
// bank n is.
//
//////////////////////////////////////////////////////////////////////////
static uint8* KernalRemapPage(uint8 uiProcess)
{
	return (uint8*)(KERNAL_REMAP_TABLE + ((uint32)uiProcess << 8));
}


//////////////////////////////////////////////////////////////////////////
//
// The process running (or calling the kernal), 0 for none.
//
//////////////////////////////////////////////////////////////////////////
uint8 KernalCurrentProcess(void)
{
	return guiKernalProcess;
}


//////////////////////////////////////////////////////////////////////////
//
// The physical main memory address of the running process's address, and
// how many bytes from it are in the same bank.  NULL if the process has no
// such bank.
//
//////////////////////////////////////////////////////////////////////////
uint8* KernalUserPointer(uint32 uiAddress, uint32* puiInBank)
{
	uint8	uiBank;

	uiBank = KernalRemapPage(guiKernalProcess)[(uiAddress >> 16) & 0xFF];
	if (uiBank == 0)
	{
		return NULL;
	}
	if (puiInBank)
	{
		*puiInBank = 0x10000 - (uiAddress & 0xFFFF);
	}
	return (uint8*)(((uint32)uiBank << 16) | (uiAddress & 0xFFFF));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static uint8* KernalUserAddress(uint32 uiAddress)
{
	return KernalUserPointer(uiAddress, NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// Copies between kernal memory and the running process's.  False if the
// process hasn't all of the banks.
//
//////////////////////////////////////////////////////////////////////////
bool KernalCopyToUser(uint32 uiAddress, const uint8* puiSource, uint32 uiLength)
{
	uint8*	puiDest;
	uint32	uiInBank;

	while (uiLength > 0)
	{
		puiDest = KernalUserPointer(uiAddress, &uiInBank);
		if (!puiDest)
		{
			return false;
		}
		uiInBank = (uiInBank < uiLength) ? uiInBank : uiLength;
		memcpy(puiDest, puiSource, uiInBank);
		uiAddress += uiInBank;
		puiSource += uiInBank;
		uiLength -= uiInBank;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads uiLength bytes of pcFile into the running process's memory at
// uiAddress, straight into its banks (they are main memory, where the SD
// Card Board's DMA puts sectors), a bank's worth at a time.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalReadToUser(CFatFile* pcFile, uint32 uiAddress, uint32 uiLength)
{
	uint8*	puiDest;
	uint32	uiInBank;
	lsize	uiRead;

	while (uiLength > 0)
	{
		puiDest = KernalUserPointer(uiAddress, &uiInBank);
		if (!puiDest)
		{
			return KERNAL_PROCESS_NO_MEMORY;
		}
		uiInBank = (uiInBank < uiLength) ? uiInBank : uiLength;
		if ((pcFile->Read(puiDest, uiInBank, &uiRead) != FAT_SUCCESS) || (uiRead != uiInBank))
		{
			return KERNAL_PROCESS_BAD_FILE;
		}
		uiAddress += uiInBank;
		uiLength -= uiInBank;
	}
	return KERNAL_PROCESS_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool KernalCopyFromUser(uint8* puiDest, uint32 uiAddress, uint32 uiLength)
{
	uint8*	puiSource;
	uint32	uiInBank;

	while (uiLength > 0)
	{
		puiSource = KernalUserPointer(uiAddress, &uiInBank);
		if (!puiSource)
		{
			return false;
		}
		uiInBank = (uiInBank < uiLength) ? uiInBank : uiLength;
		memcpy(puiDest, puiSource, uiInBank);
		uiAddress += uiInBank;
		puiDest += uiInBank;
		uiLength -= uiInBank;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The running process's call arguments: the words on its stack from
// uiOffset.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalArgumentWord(uint16 uiOffset)
{
	return (uint16)(gpuiKernalArguments[uiOffset] | (gpuiKernalArguments[uiOffset + 1] << 8));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static uint32 KernalArgumentLong(uint16 uiOffset)
{
	return (uint32)KernalArgumentWord(uiOffset) | ((uint32)KernalArgumentWord((uint16)(uiOffset + 2)) << 16);
}


//////////////////////////////////////////////////////////////////////////
//
// A zero terminated string from the running process.  False if it is too
// long or not in its memory.
//
//////////////////////////////////////////////////////////////////////////
bool KernalUserString(uint32 uiAddress, char* szDest, uint16 uiMax)
{
	uint8*	puiChar;
	uint16	i;

	for (i = 0; i < uiMax; i++)
	{
		puiChar = KernalUserAddress(uiAddress + i);
		if (!puiChar)
		{
			return false;
		}
		szDest[i] = (char)*puiChar;
		if (szDest[i] == '\0')
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Gives the process's bank uiBank a cleared physical bank, if it hasn't
// one.  False if there are none left.
//
//////////////////////////////////////////////////////////////////////////
static bool KernalMapBank(uint8 uiProcess, uint8 uiBank)
{
	uint8*	puiPage;
	uint16	uiPhysical;

	puiPage = KernalRemapPage(uiProcess);
	if (puiPage[uiBank] != 0)
	{
		return true;
	}
	for (uiPhysical = 0; uiPhysical < KERNAL_NUM_BANKS; uiPhysical++)
	{
		if ((gauiKernalBanks[uiPhysical] == KERNAL_BANK_PROCESS) && (gauiKernalBankOwners[uiPhysical] == 0))
		{
			gauiKernalBankOwners[uiPhysical] = uiProcess;
			puiPage[uiBank] = (uint8)uiPhysical;
			if (gpcKernalDebugPort)
			{
				gpcKernalDebugPort->GiveBank((uint8)uiPhysical);
			}
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Takes the process's banks back and clears its page of the remap table.
//
//////////////////////////////////////////////////////////////////////////
static void KernalFreeProcess(uint8 uiProcess)
{
	uint16	uiPhysical;

	for (uiPhysical = 0; uiPhysical < KERNAL_NUM_BANKS; uiPhysical++)
	{
		if (gauiKernalBankOwners[uiPhysical] == uiProcess)
		{
			gauiKernalBankOwners[uiPhysical] = 0;
		}
	}
	memset(KernalRemapPage(uiProcess), 0, 256);
}


//////////////////////////////////////////////////////////////////////////
//
// Maps the banks uiAddress to uiAddress + uiSize - 1 are in.
//
//////////////////////////////////////////////////////////////////////////
static bool KernalMapRange(uint8 uiProcess, uint32 uiAddress, uint32 uiSize)
{
	uint32	uiBank;

	if (uiSize == 0)
	{
		return true;
	}
	for (uiBank = uiAddress >> 16; uiBank <= ((uiAddress + uiSize - 1) >> 16); uiBank++)
	{
		if (!KernalMapBank(uiProcess, (uint8)uiBank))
		{
			return false;
		}
	}
	return true;
}


static uint16 KernalLoadExecutableObject(CAsm65Object* pcObject, CFatFile* pcFile, uint8 uiProcess, bool bLoaded);


//////////////////////////////////////////////////////////////////////////
//
// Whether uiSize bytes from uiAt are where a process's own code and data
// may go: in bank 1 up to KERNAL_USER_HIGH_MEMORY (bank 0 is its direct
// page and stack, and its heap is mapped from KERNAL_USER_HIGH_MEMORY),
// and, for a process without a heap there (bHighHeap false), not in the
// small heap's banks (gauiKernalLowHeapBanks).  The linker's -limit
// should catch a program that grew too big; this stops one that wasn't
// linked with it from being loaded over its heap.
//
//////////////////////////////////////////////////////////////////////////
static bool KernalIsProcessSection(uint32 uiAt, uint32 uiSize, bool bHighHeap)
{
	size	uiBank;
	size	uiLastBank;
	size	i;

	if (uiSize == 0)
	{
		return true;
	}
	if ((uiAt >= KERNAL_USER_HIGH_MEMORY) || (uiSize > KERNAL_USER_HIGH_MEMORY - uiAt))
	{
		return false;
	}
	uiBank = (size)(uiAt >> 16);
	uiLastBank = (size)((uiAt + uiSize - 1) >> 16);
	if (uiBank == 0)
	{
		return false;
	}
	if (!bHighHeap)
	{
		for (; uiBank <= uiLastBank; uiBank++)
		{
			for (i = 0; i < sizeof(gauiKernalLowHeapBanks); i++)
			{
				if (uiBank == gauiKernalLowHeapBanks[i])
				{
					return false;
				}
			}
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads an executable from the volume into a new process's banks: a
// CAsm65Object whose sections are all placed (at an address in the
// process's own banks), with a UserStart symbol and optionally a
// UserHeapSize: the heap the program needs, mapped from $200000
// (KERNAL_USER_HIGH_MEMORY), whose end is the memory size Libc65 reads.
// Without one it has the small heap below.  Every section must be where
// the process's code and data may go (KernalIsProcessSection), or nothing
// is mapped and it is KERNAL_PROCESS_BAD_ADDRESS.  Puts the interrupt
// frame RTI starts it with on its stack.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalLoadExecutable(const char* szPath, uint8 uiProcess)
{
	CFatFile			cFile;
	CFatAbstractFile	cFatAbstractFile;
	CAsm65Object		cObject;
	CChunkFile			cChunkFile;
	bool				bLoaded;
	uint16				uiResult;

	cFile.Init(&gcKernalVolume);
	if (cFile.Open(szPath, FAT_FILE_ACCESS_READ) != FAT_SUCCESS)
	{
		return KERNAL_PROCESS_NOT_FOUND;
	}

	//Read straight from the volume (chunk files without hashes don't link MD5, which the kernal hasn't).  The sections' bytes are
	//left in the file and read into the process's banks once they are mapped: an executable can be bigger than the
	//kernal's heap.
	cFatAbstractFile.Init(&cFile);
	cObject.Init("");
	cObject.SetSkipSectionBytes(true);
	cChunkFile.Init(&cFatAbstractFile);
	bLoaded = cChunkFile.ReadOpen();
	bLoaded = bLoaded && cObject.ReadChunks(&cChunkFile);
	uiResult = KernalLoadExecutableObject(&cObject, &cFile, uiProcess, bLoaded);
	if (bLoaded)
	{
		cChunkFile.ReadClose();
	}
	cChunkFile.Kill();
	cObject.Kill();
	cFile.Close();
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// KernalLoadExecutable once the object is read (bLoaded): maps the
// process's banks and reads the sections' bytes from the file into them.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalLoadExecutableObject(CAsm65Object* pcObject, CFatFile* pcFile, uint8 uiProcess, bool bLoaded)
{
	SAsm65Section*		psSection;
	SAsm65ObjectSymbol*	psSymbol;
	uint8*				puiPage;
	uint8*				puiStack;
	uint32				uiEntry;
	uint32				uiMemorySize;
	int32				iSymbol;
	size				i;

	if (!bLoaded)
	{
		return KERNAL_PROCESS_BAD_FILE;
	}
	iSymbol = pcObject->FindSymbol(KERNAL_PROCESS_ENTRY_SYMBOL);
	if (iSymbol < 0)
	{
		return KERNAL_PROCESS_NO_ENTRY;
	}
	psSymbol = pcObject->GetSymbol((size)iSymbol);
	uiEntry = (uint32)psSymbol->iValue;
	uiMemorySize = 0;
	iSymbol = pcObject->FindSymbol(KERNAL_PROCESS_HEAP_SYMBOL);
	if (iSymbol >= 0)
	{
		uiMemorySize = (uint32)pcObject->GetSymbol((size)iSymbol)->iValue;
		if (uiMemorySize != 0)
		{
			uiMemorySize += KERNAL_USER_HIGH_MEMORY;
		}
	}

	//Every section must be placed, and where the process's code and data may go, before anything is mapped.
	for (i = 0; i < pcObject->NumSections(); i++)
	{
		psSection = pcObject->GetSection(i);
		if (!psSection->bAt)
		{
			return KERNAL_PROCESS_BAD_FILE;
		}
		if (!KernalIsProcessSection(psSection->uiAt, psSection->uiSize, uiMemorySize > KERNAL_USER_HIGH_MEMORY))
		{
			return KERNAL_PROCESS_BAD_ADDRESS;
		}
	}

	//The process's page in the remap table: the banks every process has, its heap, the tables at their own address,
	//then the sections.
	puiPage = KernalRemapPage(uiProcess);
	for (i = 0; i < KERNAL_NUM_TABLE_BANKS; i++)
	{
		puiPage[KERNAL_TABLES_USER_BANK + i] = gauiKernalTableBanks[i];
	}
	bLoaded = true;
	for (i = 0; i < sizeof(gauiKernalProcessBanks); i++)
	{
		bLoaded &= KernalMapBank(uiProcess, gauiKernalProcessBanks[i]);
	}
	if (uiMemorySize > KERNAL_USER_HIGH_MEMORY)
	{
		bLoaded &= KernalMapRange(uiProcess, KERNAL_USER_HIGH_MEMORY, uiMemorySize - KERNAL_USER_HIGH_MEMORY);
	}
	else
	{
		uiMemorySize = 0;
		for (i = 0; i < sizeof(gauiKernalLowHeapBanks); i++)
		{
			bLoaded &= KernalMapBank(uiProcess, gauiKernalLowHeapBanks[i]);
		}
	}
	for (i = 0; (i < pcObject->NumSections()) && bLoaded; i++)
	{
		psSection = pcObject->GetSection(i);
		if (!psSection->bAt)
		{
			return KERNAL_PROCESS_BAD_FILE;
		}
		bLoaded = KernalMapRange(uiProcess, psSection->uiAt, psSection->uiSize);
	}
	if (!bLoaded)
	{
		return KERNAL_PROCESS_NO_MEMORY;
	}

	//The sections' bytes, from the file straight into the process's banks.
	for (i = 0; (i < pcObject->NumSections()) && bLoaded; i++)
	{
		psSection = pcObject->GetSection(i);
		if (psSection->uiNumBytes > 0)
		{
			bLoaded = (pcFile->Seek((uint32)psSection->iBytesPosition, FAT_SEEK_START) == FAT_SUCCESS) &&
					  (KernalReadToUser(pcFile, psSection->uiAt, psSection->uiNumBytes) == KERNAL_PROCESS_NONE);
		}
	}
	if (!bLoaded)
	{
		return KERNAL_PROCESS_BAD_FILE;
	}

	*(uint32*)KernalUserAddress(KERNAL_USER_MEMORY_SIZE) = uiMemorySize;

	//The interrupt frame RTI pulls from the process's stack: P, the entry's PC and its bank.
	puiStack = KernalUserAddress(KERNAL_USER_STACK_TOP - 3);
	puiStack[0] = KERNAL_PROCESS_STATUS;
	puiStack[1] = (uint8)uiEntry;
	puiStack[2] = (uint8)(uiEntry >> 8);
	puiStack[3] = (uint8)(uiEntry >> 16);
	return KERNAL_PROCESS_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// Runs an executable (see KernalLoadExecutable) as a new process in user
// mode until it exits.  boot.exe is process $FF; a process run by another
// (during the other's execute call) gets the next ID down and its own
// banks, and the other carries on once it exits.  The debug port is told
// of it starting and ending.  Returns KERNAL_PROCESS_EXITED with its exit
// code, or why it couldn't run.
//
//////////////////////////////////////////////////////////////////////////
uint16 KernalRunExecutable(const char* szPath, uint16 uiArgument, uint16* puiExitCode)
{
	SKernalCall		sCaller;
	uint8			uiCaller;
	uint8			uiProcess;
	uint16			uiResult;

	uiCaller = guiKernalProcess;
	uiProcess = (uiCaller == 0) ? KERNAL_FIRST_PROCESS_ID : (uint8)(uiCaller - 1);
	if ((uiProcess < KERNAL_LAST_PROCESS_ID) || (uiCaller == KERNAL_LAST_PROCESS_ID))
	{
		return KERNAL_PROCESS_NO_PROCESS;
	}

	//The process's memory is read and written through guiKernalProcess's remap page.
	guiKernalProcess = uiProcess;
	KernalFreeProcess(uiProcess);
	uiResult = KernalLoadExecutable(szPath, uiProcess);
	if (uiResult != KERNAL_PROCESS_NONE)
	{
		KernalFreeProcess(uiProcess);
		guiKernalProcess = uiCaller;
		return uiResult;
	}

	//The caller's registers are in gsKernalCall until its call returns: the new process's calls use it too.
	memcpy(&sCaller, &gsKernalCall, sizeof(SKernalCall));
	memset(&gsKernalCall, 0, sizeof(gsKernalCall));
	gsKernalCall.uiUserS = KERNAL_USER_STACK_TOP - 4;
	gauiKernalArguments[uiProcess] = uiArgument;

	if (gpcKernalDebugPort)
	{
		gpcKernalDebugPort->Tell(KERNAL_DEBUG_PORT_STARTED, uiProcess, 0, szPath);
	}
	KernalWriteRegister(8, uiProcess);
	KernalEnterUser();
	*puiExitCode = gsKernalCall.uiExitCode;
	if (gpcKernalDebugPort)
	{
		gpcKernalDebugPort->Tell(KERNAL_DEBUG_PORT_ENDED, uiProcess, *puiExitCode, szPath);
	}

	KernalCloseHandles(uiProcess);
	KernalCloseDrivers(uiProcess);
	KernalFreeProcess(uiProcess);
	memcpy(&gsKernalCall, &sCaller, sizeof(SKernalCall));
	guiKernalProcess = uiCaller;
	if (uiCaller != 0)
	{
		KernalWriteRegister(8, uiCaller);
	}
	return KERNAL_PROCESS_EXITED;
}


//////////////////////////////////////////////////////////////////////////
//
// The execute call: the path, the argument and where the exit code goes.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalCallExecute(void)
{
	char		szPath[KERNAL_MAX_PATH];
	uint32		uiExitCodeAddress;
	uint16		uiArgument;
	uint16		uiExitCode;
	uint16		uiResult;
	uint8		auiExitCode[2];

	if (!KernalUserString(KernalArgumentLong(0), szPath, KERNAL_MAX_PATH))
	{
		return KERNAL_PROCESS_NOT_FOUND;
	}
	uiArgument = KernalArgumentWord(4);
	uiExitCodeAddress = KernalArgumentLong(6);

	uiExitCode = 0;
	uiResult = KernalRunExecutable(szPath, uiArgument, &uiExitCode);
	auiExitCode[0] = (uint8)uiExitCode;
	auiExitCode[1] = (uint8)(uiExitCode >> 8);
	KernalCopyToUser(uiExitCodeAddress, auiExitCode, 2);
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file size call: the size in A (low) and X (high), $FFFFFFFF if the
// file isn't there.
//
//////////////////////////////////////////////////////////////////////////
static void KernalCallFileSize(void)
{
	char		szPath[KERNAL_MAX_PATH];
	CFatFile	cFile;
	uint32		uiSize;

	uiSize = 0xFFFFFFFF;
	if (KernalUserString(KernalArgumentLong(0), szPath, KERNAL_MAX_PATH))
	{
		cFile.Init(&gcKernalVolume);
		if (cFile.Open(szPath, FAT_FILE_ACCESS_READ) == FAT_SUCCESS)
		{
			uiSize = cFile.Size();
			cFile.Close();
		}
	}
	gsKernalCall.uiA = (uint16)uiSize;
	gsKernalCall.uiX = (uint16)(uiSize >> 16);
}


//////////////////////////////////////////////////////////////////////////
//
// The file load call: the path, the destination and the most to read.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalCallFileLoad(void)
{
	char		szPath[KERNAL_MAX_PATH];
	CFatFile	cFile;
	uint32		uiDest;
	uint32		uiMax;
	uint32		uiSize;
	uint16		uiResult;

	if (!KernalUserString(KernalArgumentLong(0), szPath, KERNAL_MAX_PATH))
	{
		return KERNAL_PROCESS_NOT_FOUND;
	}
	uiDest = KernalArgumentLong(4);
	uiMax = KernalArgumentLong(8);

	cFile.Init(&gcKernalVolume);
	if (cFile.Open(szPath, FAT_FILE_ACCESS_READ) != FAT_SUCCESS)
	{
		return KERNAL_PROCESS_NOT_FOUND;
	}
	uiSize = cFile.Size();
	uiResult = (uiSize <= uiMax) ? KERNAL_PROCESS_NONE : KERNAL_PROCESS_NO_MEMORY;
	uiSize = (uiSize < uiMax) ? uiSize : uiMax;
	if (uiResult == KERNAL_PROCESS_NONE)
	{
		uiResult = KernalReadToUser(&cFile, uiDest, uiSize);
	}
	cFile.Close();
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file read call: the path, the offset in the file, the destination
// and the number of bytes, which must all be in the file.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalCallFileRead(void)
{
	char		szPath[KERNAL_MAX_PATH];
	CFatFile	cFile;
	uint32		uiOffset;
	uint32		uiDest;
	uint32		uiSize;
	uint32		uiFileSize;
	uint16		uiResult;

	if (!KernalUserString(KernalArgumentLong(0), szPath, KERNAL_MAX_PATH))
	{
		return KERNAL_PROCESS_NOT_FOUND;
	}
	uiOffset = KernalArgumentLong(4);
	uiDest = KernalArgumentLong(8);
	uiSize = KernalArgumentLong(12);

	cFile.Init(&gcKernalVolume);
	if (cFile.Open(szPath, FAT_FILE_ACCESS_READ) != FAT_SUCCESS)
	{
		return KERNAL_PROCESS_NOT_FOUND;
	}
	uiFileSize = cFile.Size();
	if ((uiOffset > uiFileSize) || (uiSize > uiFileSize - uiOffset) || (cFile.Seek(uiOffset, FAT_SEEK_START) != FAT_SUCCESS))
	{
		uiResult = KERNAL_PROCESS_BAD_FILE;
	}
	else
	{
		uiResult = KernalReadToUser(&cFile, uiDest, uiSize);
	}
	cFile.Close();
	return uiResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A COP from the process.  Its signature byte (the byte before the return
// address it pushed on its stack) is the call.
//
//////////////////////////////////////////////////////////////////////////
extern "C" void KernalCopCall(void)
{
	uint8*			puiFrame;
	uint32			uiReturn;
	uint8			uiCall;

	//The frame: P, PC low, PC high, then the program bank.
	puiFrame = KernalUserAddress((uint32)gsKernalCall.uiUserS + 1);
	uiReturn = (uint32)puiFrame[1] | ((uint32)puiFrame[2] << 8) | ((uint32)puiFrame[3] << 16);
	uiCall = *KernalUserAddress(uiReturn - 1);
	gpuiKernalArguments = KernalUserAddress(gsKernalCall.uiA);

	switch (uiCall)
	{
	case KERNAL_CALL_EXIT:
		gsKernalCall.uiExitCode = gsKernalCall.uiA;
		gsKernalCall.uiExit = 1;
		break;

	case KERNAL_CALL_PUT_VALUE:
		gsKernalStatus.uiUserValue = KernalArgumentWord(0);
		break;

	case KERNAL_CALL_EXECUTE:
		gsKernalCall.uiA = KernalCallExecute();
		break;

	case KERNAL_CALL_GET_ARGUMENT:
		gsKernalCall.uiA = gauiKernalArguments[guiKernalProcess];
		break;

	case KERNAL_CALL_FILE_SIZE:
		KernalCallFileSize();
		break;

	case KERNAL_CALL_FILE_LOAD:
		gsKernalCall.uiA = KernalCallFileLoad();
		break;

	case KERNAL_CALL_FILE_READ:
		gsKernalCall.uiA = KernalCallFileRead();
		break;

	case KERNAL_CALL_DEVICE:
		gsKernalCall.uiA = KernalCallDevice(KernalArgumentLong(0));
		break;

	default:
		gsKernalCall.uiA = 0xFFFF;
		break;
	}
}
