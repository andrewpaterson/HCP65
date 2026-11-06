#include <string.h>
#include "BaseLib/FatFile.h"
#include "BaseLib/FatAbstractFile.h"
#include "BaseLib/ChunkFile.h"
#include "Assembler65Lib/Asm65Object.h"
#include "KernalDriverLoader.h"


#define KERNAL_DRIVER_PATH_LENGTH	(sizeof(KERNAL_DRIVER_DIRECTORY) + DEVICE_ROM_DRIVER_LENGTH)
#define KERNAL_MODULE_SECTION		"module"			//Assembler65Lib's ASM65_MODULE_SECTION.
#define KERNAL_MODULE_INIT_START	"__init_start"
#define KERNAL_MODULE_INIT_END		"__init_end"


typedef void			(*KernalInitRoutine)(void);
typedef CKernalDriver*	(*KernalCreateDriverFunction)(void);


extern CFatVolume	gcKernalVolume;
extern "C" uint8	_end;		//The linker's __end: the end of the kernal's bss.

static uint32		guiKernalDriverMemory;		//The next free MPU RAM for a driver, 0 until the first.


//////////////////////////////////////////////////////////////////////////
//
// The kernal's export called szName, NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
void* KernalFindExport(const char* szName)
{
	const SKernalExport*	psExport;
	uint16					i;

	psExport = (const SKernalExport*)&gauiKernalExports;
	for (i = 0; i < guiKernalNumExports; i++)
	{
		if (strcmp(psExport[i].szName, szName) == 0)
		{
			return psExport[i].pvAddress;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// uiSize bytes of MPU RAM in one bank for a driver, after the kernal and
// the drivers loaded before it.  NULL if there's no room.  Drivers are
// never unloaded.
//
//////////////////////////////////////////////////////////////////////////
static uint8* KernalDriverMemory(uint32 uiSize)
{
	uint32	uiStart;

	if (guiKernalDriverMemory == 0)
	{
		guiKernalDriverMemory = (uint32)&_end;
	}
	uiStart = (guiKernalDriverMemory + 3) & ~(uint32)3;
	if (((uiStart & 0xFFFF) + uiSize) > 0x10000)
	{
		uiStart = (uiStart + 0xFFFF) & ~(uint32)0xFFFF;
	}
	if ((uiSize > 0x10000) || (uiStart + uiSize > KERNAL_DRIVER_MEMORY_END))
	{
		return NULL;
	}
	guiKernalDriverMemory = uiStart + uiSize;
	return (uint8*)uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
// The offset in the module of its symbol szName, -1 if it has none.
//
//////////////////////////////////////////////////////////////////////////
static int32 KernalModuleSymbol(CAsm65Object* pcObject, const char* szName)
{
	int32				iSymbol;
	SAsm65ObjectSymbol*	psSymbol;

	iSymbol = pcObject->FindSymbol(szName);
	if (iSymbol < 0)
	{
		return -1;
	}
	psSymbol = pcObject->GetSymbol((size)iSymbol);
	return (psSymbol->iSection == 0) ? (int32)psSymbol->iValue : -1;
}


//////////////////////////////////////////////////////////////////////////
//
// Writes the module's relocated fields now that it is at puiBase: its own
// places and the kernal's exports it imports.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalRelocateDriver(CAsm65Object* pcObject, uint8* puiBase)
{
	SAsm65Relocation*	psRelocation;
	void*				pvExport;
	int64				iField;
	int64				iValue;
	size				i;
	uint8				j;

	for (i = 0; i < pcObject->NumRelocations(); i++)
	{
		psRelocation = pcObject->GetRelocation(i);
		iField = (int64)(uint32)puiBase + psRelocation->uiOffset;
		if (psRelocation->iTargetSection == 0)
		{
			iValue = (int64)(uint32)puiBase;
		}
		else if (psRelocation->iTargetSection == ASM65_TARGET_SYMBOL)
		{
			pvExport = KernalFindExport(pcObject->GetSymbol(psRelocation->uiTargetSymbol)->szName.Text());
			if (!pvExport)
			{
				return KERNAL_LOAD_MISSING_EXPORT;
			}
			iValue = (int64)(uint32)pvExport;
		}
		else
		{
			return KERNAL_LOAD_BAD_FILE;
		}
		iValue += psRelocation->iAddend;
		if (psRelocation->bRelative)
		{
			iValue -= iField;
		}
		if (!Asm65ApplyForm(psRelocation->uiForm, iField, &iValue) || !Asm65Fits(iValue, psRelocation->uiBytes, psRelocation->bSigned))
		{
			return KERNAL_LOAD_BAD_RELOCATION;
		}
		for (j = 0; j < psRelocation->uiBytes; j++)
		{
			puiBase[psRelocation->uiOffset + j] = (uint8)(iValue >> (8 * j));
		}
	}
	return KERNAL_LOAD_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// The module read from pcFile (its section's bytes still in the file) is
// put in MPU RAM, relocated, its globals constructed and its driver
// started for the board in uiSlot.
//
//////////////////////////////////////////////////////////////////////////
static uint16 KernalPlaceDriver(CAsm65Object* pcObject, CFatFile* pcFile, uint16 uiSlot)
{
	SAsm65Section*				psSection;
	uint8*						puiBase;
	int32						iInitStart;
	int32						iInitEnd;
	int32						iEntry;
	int32						iInit;
	lsize						uiRead;
	uint16						uiResult;
	KernalCreateDriverFunction	fCreate;
	CKernalDriver*				pcDriver;

	if ((pcObject->NumSections() != 1) || (strcmp(pcObject->GetSection(0)->szName.Text(), KERNAL_MODULE_SECTION) != 0))
	{
		return KERNAL_LOAD_BAD_FILE;
	}
	iInitStart = KernalModuleSymbol(pcObject, KERNAL_MODULE_INIT_START);
	iInitEnd = KernalModuleSymbol(pcObject, KERNAL_MODULE_INIT_END);
	iEntry = KernalModuleSymbol(pcObject, KERNAL_DRIVER_ENTRY);
	if ((iInitStart < 0) || (iInitEnd < iInitStart) || (iEntry < 0))
	{
		return KERNAL_LOAD_BAD_FILE;
	}

	psSection = pcObject->GetSection(0);
	puiBase = KernalDriverMemory(psSection->uiSize);
	if (!puiBase)
	{
		return KERNAL_LOAD_NO_MEMORY;
	}
	if ((pcFile->Seek((uint32)psSection->iBytesPosition, FAT_SEEK_START) != FAT_SUCCESS) ||
		(pcFile->Read(puiBase, psSection->uiNumBytes, &uiRead) != FAT_SUCCESS) || (uiRead != psSection->uiNumBytes))
	{
		return KERNAL_LOAD_BAD_FILE;
	}
	memset(&puiBase[psSection->uiNumBytes], 0, psSection->uiSize - psSection->uiNumBytes);

	uiResult = KernalRelocateDriver(pcObject, puiBase);
	if (uiResult != KERNAL_LOAD_SUCCESS)
	{
		return uiResult;
	}

	//Its globals' constructors, in link order, as the start up runs the kernal's.
	for (iInit = iInitStart; iInit < iInitEnd; iInit += 4)
	{
		((KernalInitRoutine)(*(uint32*)&puiBase[iInit]))();
	}

	fCreate = (KernalCreateDriverFunction)&puiBase[iEntry];
	pcDriver = fCreate();
	if (!pcDriver || (KernalStartDriver(pcDriver, uiSlot) != KERNAL_DRIVER_SUCCESS))
	{
		return KERNAL_LOAD_NO_DRIVER;
	}
	return KERNAL_LOAD_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
//
// Loads the driver the device ROM of the board in uiSlot names, from
// KERNAL_DRIVER_DIRECTORY, and starts it for the board.  Returns
// KERNAL_LOAD_SUCCESS or why it couldn't.
//
//////////////////////////////////////////////////////////////////////////
uint16 KernalLoadDriver(uint16 uiSlot)
{
	char				szPath[KERNAL_DRIVER_PATH_LENGTH];
	CFatFile			cFile;
	CFatAbstractFile	cFatAbstractFile;
	CAsm65Object		cObject;
	CChunkFile			cChunkFile;
	bool				bLoaded;
	uint16				uiResult;

	strcpy(szPath, KERNAL_DRIVER_DIRECTORY);
	strcat(szPath, gasKernalDevices[uiSlot].psROM->szDriverFile);
	cFile.Init(&gcKernalVolume);
	if (cFile.Open(szPath, FAT_FILE_ACCESS_READ) != FAT_SUCCESS)
	{
		return KERNAL_LOAD_NOT_FOUND;
	}

	//The module's symbols and relocations go in the heap; its bytes are read straight into MPU RAM once it has a place.
	cFatAbstractFile.Init(&cFile);
	cObject.Init("");
	cObject.SetSkipSectionBytes(true);
	cChunkFile.Init(&cFatAbstractFile);
	bLoaded = cChunkFile.ReadOpen();
	bLoaded = bLoaded && cObject.ReadChunks(&cChunkFile);
	uiResult = bLoaded ? KernalPlaceDriver(&cObject, &cFile, uiSlot) : KERNAL_LOAD_BAD_FILE;
	if (bLoaded)
	{
		cChunkFile.ReadClose();
	}
	cChunkFile.Kill();
	cObject.Kill();
	cFile.Close();
	return uiResult;
}
