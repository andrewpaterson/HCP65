#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/PointerFunctions.h"
#include "HCP65KernalBuilder.h"


//The kernal's units, linked only when used.  Start.asm and Kernal.cpp (main) are always linked.
static const char* gaszHCP65KernalUnits[] = { "KernalDevices.cpp", "KernalSDCard.cpp", "KernalProcess.cpp", "KernalAudio.asm", NULL };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65KernalBuilder::Init(void)
{
	mcBuilder.Init();

	//Banks 0-3 hold what is linked at $000000-$03FFFF, banks 4-7 the tables.  Code and data flow from bank 1; bss
	//after them may go up to the remap table, but what is in the image must stay below the tables' ROM banks.
	mcImage.Init("Kernal.rom", HCP65_ROM_SIZE, 0xFF, HCP65_RAM_END, 0);
	mcImage.AddRegion(0, HCP65_ROM_CODE_END, 0);
	mcImage.AddRegion(HCP65_TABLES_START, HCP65_TABLES_END, HCP65_ROM_CODE_END);
	muiCodeEnd = 0;
	mszErrors.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65KernalBuilder::Kill(void)
{
	mszErrors.Kill();
	mcImage.Kill();
	mcBuilder.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szCommonLibraries, szHCP65Libraries (which holds Libc65 and
// Assembler65Lib) and szKernal are directories (relative ones are made
// full).  False with GetErrors if it didn't compile, link or fit.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65KernalBuilder::Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szKernal)
{
	CFileUtil	cFileUtil;
	CChars		szCommon;
	CChars		szLibraries;
	CChars		szKernalDirectory;
	CChars		szPath;
	bool		bResult;
	size		i;

	szCommon.Init(szCommonLibraries);
	cFileUtil.FullPath(&szCommon);
	szLibraries.Init(szHCP65Libraries);
	cFileUtil.FullPath(&szLibraries);
	szKernalDirectory.Init(szKernal);
	cFileUtil.FullPath(&szKernalDirectory);

	mcBuilder.SetTarget(CPPT_W65C816);
	mcBuilder.SetRemoveUnused(true);
	szPath.Init(szLibraries);
	cFileUtil.AppendToPath(&szPath, "Libc65");
	mcBuilder.AddIncludeDirectory(szPath.Text(), false, true);
	szPath.Kill();
	mcBuilder.AddIncludeDirectory(szCommon.Text(), true, false);
	mcBuilder.AddIncludeDirectory(szLibraries.Text(), true, false);
	mcBuilder.AddIncludeDirectory(szKernalDirectory.Text(), false, false);

	mcBuilder.AddUnit("Kernal", szKernalDirectory.Text(), "Start.asm");
	mcBuilder.AddUnit("Kernal", szKernalDirectory.Text(), "Kernal.cpp");
	mcBuilder.AddUnit("Kernal", szKernalDirectory.Text(), "MathTables.asm");
	for (i = 0; gaszHCP65KernalUnits[i]; i++)
	{
		mcBuilder.AddUnit("Kernal", szKernalDirectory.Text(), gaszHCP65KernalUnits[i], true);
	}

	szPath.Init(szCommon);
	cFileUtil.AppendToPath(&szPath, "BaseLib");
	cFileUtil.AppendToPath(&szPath, "BaseLib.vcxproj");
	bResult = mcBuilder.AddVisualStudioProject("BaseLib", szPath.Text(), true);
	szPath.Kill();

	//The executable format is Assembler65Lib's object.
	szPath.Init(szLibraries);
	cFileUtil.AppendToPath(&szPath, "Assembler65Lib");
	mcBuilder.AddUnit("Assembler65Lib", szPath.Text(), "Asm65Object.cpp", true);
	szPath.Kill();

	szPath.Init(szLibraries);
	cFileUtil.AppendToPath(&szPath, "Libc65");
	mcBuilder.AddLibc65(szPath.Text());
	szPath.Kill();

	mcBuilder.SetLimit(mcImage.GetLimit());
	bResult &= mcBuilder.Link();
	mszErrors.Append(mcBuilder.GetErrors());
	szKernalDirectory.Kill();
	szLibraries.Kill();
	szCommon.Kill();
	if (!bResult)
	{
		return false;
	}
	return MakeImage();
}


//////////////////////////////////////////////////////////////////////////
//
// The linked segments in the ROM's layout, and where the kernal's code
// and data end (bank 0's stubs and vectors aren't counted).
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65KernalBuilder::MakeImage(void)
{
	CAsm65Output*	pcOutput;
	SAsm65Segment*	psSegment;
	size			i;
	uint32			uiStart;
	uint32			uiEnd;

	pcOutput = mcBuilder.GetOutput();
	if (!mcImage.Make(pcOutput, &mszErrors))
	{
		return false;
	}

	muiCodeEnd = 0;
	for (i = 0; i < pcOutput->NumSegments(); i++)
	{
		psSegment = pcOutput->GetSegment(i);
		uiStart = psSegment->uiAddress;
		uiEnd = uiStart + (uint32)psSegment->cBytes.NumElements();
		if ((uiStart >= 0x10000) && (uiEnd <= HCP65_ROM_CODE_END) && (uiEnd > muiCodeEnd))
		{
			muiCodeEnd = uiEnd;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65KernalBuilder::Write(const char* szFilename)
{
	return mcImage.Write(szFilename);
}


//////////////////////////////////////////////////////////////////////////
//
// A global's address, with the compiler's underscore (C++ guiState is
// _guiState).
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65KernalBuilder::GetSymbol(const char* szName, uint32* puiAddress)
{
	int64	lliValue;

	if (!mcBuilder.GetSymbol(szName, &lliValue))
	{
		return false;
	}
	*puiAddress = (uint32)lliValue;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65KernalBuilder::GetROM(void)
{
	return mcImage.GetImage();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65KernalBuilder::GetCodeEnd(void)
{
	return muiCodeEnd;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65KernalBuilder::GetErrors(void)
{
	return mszErrors.Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CCPP65Builder* CHCP65KernalBuilder::GetBuilder(void)
{
	return &mcBuilder;
}
