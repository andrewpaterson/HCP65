#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/MemoryFile.h"
#include "BaseLib/PointerFunctions.h"
#include "Assembler65Lib/Asm65Object.h"
#include "HCP65ExeBuilder.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ExeBuilder::Init(void)
{
	mcBuilder.Init();
	mcExecutable.Init("", HCP65_EXE_ENTRY_SYMBOL, HCP65_EXE_CODE_LIMIT);
	mszErrors.Init();
	muiHeapSize = HCP65_EXE_DEFAULT_HEAP;
}


//////////////////////////////////////////////////////////////////////////
//
// The size of the process's heap (from $200000), a multiple of 64KB.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ExeBuilder::SetHeapSize(uint32 uiHeapSize)
{
	muiHeapSize = (uiHeapSize + 0xFFFF) & ~(uint32)0xFFFF;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ExeBuilder::Kill(void)
{
	mszErrors.Kill();
	mcExecutable.Kill();
	mcBuilder.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// aszUnits (NULL terminated) are the program's files in
// szProgramDirectory; one of them has main.  aszLibraries (NULL terminated,
// or NULL) are more of the common libraries (StandardLib, SupportLib) to
// link from: their Visual Studio projects.  szHCP65Libraries holds Libc65.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65ExeBuilder::Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szUser, const char* szProgramDirectory, const char** aszUnits, const char** aszLibraries)
{
	CFileUtil	cFileUtil;
	CChars		szCommon;
	CChars		szLibraries;
	CChars		szUserDirectory;
	CChars		szProgram;
	CChars		szPath;
	bool		bResult;
	size		i;

	szCommon.Init(szCommonLibraries);
	cFileUtil.FullPath(&szCommon);
	szLibraries.Init(szHCP65Libraries);
	cFileUtil.FullPath(&szLibraries);
	szUserDirectory.Init(szUser);
	cFileUtil.FullPath(&szUserDirectory);
	szProgram.Init(szProgramDirectory);
	cFileUtil.FullPath(&szProgram);

	mcBuilder.SetTarget(CPPT_W65C816);
	mcBuilder.SetRemoveUnused(true);
	szPath.Init(szLibraries);
	cFileUtil.AppendToPath(&szPath, "Libc65");
	mcBuilder.AddIncludeDirectory(szPath.Text(), false, true);
	szPath.Kill();
	mcBuilder.AddIncludeDirectory(szCommon.Text(), true, false);
	mcBuilder.AddIncludeDirectory(szLibraries.Text(), true, false);
	mcBuilder.AddIncludeDirectory(szUserDirectory.Text(), false, false);
	mcBuilder.AddIncludeDirectory(szProgram.Text(), false, false);

	mcBuilder.AddUnit("User", szUserDirectory.Text(), "UserStart.asm");
	mcBuilder.AddUnit("User", szUserDirectory.Text(), "UserCalls.asm", true);
	mcBuilder.AddUnit("User", szUserDirectory.Text(), "UserMathTables.asm", true);
	for (i = 0; aszUnits[i]; i++)
	{
		mcBuilder.AddUnit("Program", szProgram.Text(), aszUnits[i]);
	}

	szPath.Init(szCommon);
	cFileUtil.AppendToPath(&szPath, "BaseLib");
	cFileUtil.AppendToPath(&szPath, "BaseLib.vcxproj");
	bResult = mcBuilder.AddVisualStudioProject("BaseLib", szPath.Text(), true);
	szPath.Kill();
	for (i = 0; aszLibraries && aszLibraries[i]; i++)
	{
		szPath.Init(szCommon);
		cFileUtil.AppendToPath(&szPath, aszLibraries[i]);
		cFileUtil.AppendToPath(&szPath, aszLibraries[i]);
		szPath.Append(".vcxproj");
		bResult &= mcBuilder.AddVisualStudioProject(aszLibraries[i], szPath.Text(), true);
		szPath.Kill();
	}

	szPath.Init(szLibraries);
	cFileUtil.AppendToPath(&szPath, "Libc65");
	mcBuilder.AddLibc65(szPath.Text());
	szPath.Kill();

	mcBuilder.SetLimit(mcExecutable.GetLimit());
	bResult &= mcBuilder.Link();
	mszErrors.Append(mcBuilder.GetErrors());
	szProgram.Kill();
	szUserDirectory.Kill();
	szLibraries.Kill();
	szCommon.Kill();
	if (!bResult)
	{
		return false;
	}
	return MakeExecutable();
}


//////////////////////////////////////////////////////////////////////////
//
// The linked segments as placed sections of an object, the entry, and
// the memory size Libc65 reads.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65ExeBuilder::MakeExecutable(void)
{
	mcExecutable.Kill();
	mcExecutable.Init("", HCP65_EXE_ENTRY_SYMBOL, HCP65_EXE_CODE_LIMIT);
	mcExecutable.AddSymbol(HCP65_EXE_MEMORY_SIZE_SYMBOL, HCP65_EXE_HIGH_MEMORY + muiHeapSize);
	return mcExecutable.Make(mcBuilder.GetOutput(), mcBuilder.GetLinker(), &mszErrors);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint8* CHCP65ExeBuilder::GetExecutable(void)
{
	return mcExecutable.GetBytes();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CHCP65ExeBuilder::GetExecutableSize(void)
{
	return mcExecutable.GetSize();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CHCP65ExeBuilder::GetErrors(void)
{
	return mszErrors.Text();
}
