#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/PointerFunctions.h"
#include "HCP65ProgramBuild.h"
#include "HCP65KernalBuilder.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65KernalBuilder::Init(EHCP65KernalImage eImage)
{
	mcBuilder.Init();
	mbBuilder = true;
	meImage = eImage;
	if (eImage == HKI_Boot)
	{
		mcImage.Init("Boot.rom", HCP65_ROM_SIZE, 0xFF, HCP65_BOOT_END, 0);
		mcImage.SetText(HCP65_BOOT_TEXT);
		mcImage.AddRegion(HCP65_OS_START, 0x10000, HCP65_OS_START);
		mcImage.AddRegion(HCP65_BOOT_TEXT, HCP65_BOOT_END, HCP65_BOOT_TEXT);
	}
	else
	{
		//Code and data flow from bank 1; bss after them may go up to the remap table, but what is in OS.SYS must stay
		//in the banks the boot loader reads it into.
		mcImage.Init("OS.SYS", HCP65_OS_SIZE, 0xFF, HCP65_RAM_END, HCP65_OS_START);
		mcImage.AddRegion(HCP65_OS_START, HCP65_OS_CODE_END, 0);
	}
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
	Release();
}


//////////////////////////////////////////////////////////////////////////
//
// szCommonLibraries, szHCP65Libraries (which holds Libc65 and
// Assembler65Lib) and szKernal (which holds HCP65Kernal.vcxproj) are
// directories.  False with GetErrors if it didn't compile, link or fit.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65KernalBuilder::Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szKernal, const char* szBoot)
{
	CFileUtil	cFileUtil;
	CChars		szProject;
	bool		bResult;

	//Built as HCP65-IDE builds the solution: the units in link order (the kernal's Assembler65Lib's Asm65Object.cpp for
	//the executable format among them), then BaseLib and Libc65's runtime.
	if (meImage == HKI_Boot)
	{
		szProject.Init(szBoot);
		cFileUtil.AppendToPath(&szProject, "HCP65Boot.vcxproj");
	}
	else
	{
		szProject.Init(szKernal);
		cFileUtil.AppendToPath(&szProject, "HCP65Kernal.vcxproj");
	}
	mcBuilder.SetMathTables(false);  //The tables aren't in memory until the start up program loads them (MathTables="false").
	bResult = HCP65AddProgram(&mcBuilder, szCommonLibraries, szHCP65Libraries, szProject.Text(), (meImage == HKI_Boot) ? szKernal : NULL, NULL);
	szProject.Kill();

	mcBuilder.SetLimit(mcImage.GetLimit());
	if (mcImage.GetText() != 0)
	{
		mcBuilder.SetTextAddress(mcImage.GetText());
	}
	mcBuilder.SetNoFloats(true);  //No floats (the .hcp65build files' NoFloats).
	bResult &= mcBuilder.Link();
	mszErrors.Append(mcBuilder.GetErrors());
	if (!bResult)
	{
		return false;
	}
	return MakeImage();
}


//////////////////////////////////////////////////////////////////////////
//
// The linked segments in the image's layout, and where the code and data
// end (bank 0's stubs and vectors aren't counted).
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
		if ((uiStart >= 0x10000) && (uiEnd > muiCodeEnd))
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

	if (!mbBuilder || !mcBuilder.GetSymbol(szName, &lliValue))
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
uint8* CHCP65KernalBuilder::GetImage(void) const
{
	return mcImage.GetImage();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65KernalBuilder::GetImageSize(void) const
{
	return mcImage.GetSize();
}


//////////////////////////////////////////////////////////////////////////
//
// The image's byte for what is linked at uiAddress, NULL if no region has
// it.
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65KernalBuilder::GetImageAt(uint32 uiAddress)
{
	SAsm65ImageRegion*	psRegion;
	size				i;

	for (i = 0; i < mcImage.NumRegions(); i++)
	{
		psRegion = mcImage.GetRegion(i);
		if ((uiAddress >= psRegion->uiStart) && (uiAddress < psRegion->uiEnd))
		{
			return &mcImage.GetImage()[psRegion->uiOffset + (uiAddress - psRegion->uiStart)];
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint32 CHCP65KernalBuilder::GetCodeEnd(void) const
{
	return muiCodeEnd;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65KernalBuilder::GetErrors(void) const
{
	return mszErrors.Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CCPP65Builder* CHCP65KernalBuilder::GetBuilder(void)
{
	return mbBuilder ? &mcBuilder : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The compiler is freed: the image is kept (it is the linked bytes, copied), and GetSymbol, GetBuilder and Write's
// map have nothing after.  A whole program's units, objects and link, which every later build would run beside.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65KernalBuilder::Release(void)
{
	if (mbBuilder)
	{
		mcBuilder.Kill();
		mbBuilder = false;
	}
}
