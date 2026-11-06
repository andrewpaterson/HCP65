#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/MemoryFile.h"
#include "BaseLib/PointerFunctions.h"
#include "Assembler65Lib/Asm65Object.h"
#include "HCP65ProgramBuild.h"
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
// Whether the program runs with the maths tables in memory (true unless
// set; CCPP65Builder::SetMathTables): boot.exe, which loads them, runs
// without.  Call before Build.
//
//////////////////////////////////////////////////////////////////////////
void CHCP65ExeBuilder::SetMathTables(bool bMathTables)
{
	mcBuilder.SetMathTables(bMathTables);
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
// szProjectFile is the program's Visual Studio project (its units, the
// user start up first, one of them with main), as HCP65-IDE builds it;
// szUser holds HCP65User.h.  aszLibraries (NULL terminated, or NULL) are
// more of the common libraries (StandardLib, SupportLib) to link from.
// szHCP65Libraries holds Libc65.
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65ExeBuilder::Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szUser, const char* szProjectFile, const char** aszLibraries)
{
	bool	bResult;

	bResult = HCP65AddProgram(&mcBuilder, szCommonLibraries, szHCP65Libraries, szProjectFile, szUser, aszLibraries);
	mcBuilder.SetLimit(mcExecutable.GetLimit());
	bResult &= mcBuilder.Link();
	mszErrors.Append(mcBuilder.GetErrors());
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
