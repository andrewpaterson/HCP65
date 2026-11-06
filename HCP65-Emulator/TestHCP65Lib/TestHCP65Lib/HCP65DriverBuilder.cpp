#include "HCP65ProgramBuild.h"
#include "HCP65DriverBuilder.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DriverBuilder::Init(void)
{
	mcBuilder.Init();
	mcModule.Init(HCP65_DRIVER_ENTRY_SYMBOL);
	mszErrors.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CHCP65DriverBuilder::Kill(void)
{
	mszErrors.Kill();
	mcModule.Kill();
	mcBuilder.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szProjectFile is the driver's Visual Studio project (its units in link
// order); szKernal holds the kernal's headers (KernalDriver.h).
//
//////////////////////////////////////////////////////////////////////////
bool CHCP65DriverBuilder::Build(const char* szCommonLibraries, const char* szHCP65Libraries, const char* szKernal, const char* szProjectFile)
{
	bool	bResult;

	mcBuilder.SetMathTables(false);
	bResult = HCP65AddProgram(&mcBuilder, szCommonLibraries, szHCP65Libraries, szProjectFile, szKernal, NULL);
	mcBuilder.SetLimit(HCP65_DRIVER_LIMIT);
	mcBuilder.SetNoFloats(true);
	mcBuilder.GetLinker()->SetRelocatable(true);
	mcBuilder.GetLinker()->KeepSymbol(HCP65_DRIVER_ENTRY_SYMBOL);
	bResult &= mcBuilder.Link();
	mszErrors.Append(mcBuilder.GetErrors());
	if (!bResult)
	{
		return false;
	}
	return mcModule.Make(mcBuilder.GetOutput(), mcBuilder.GetLinker(), &mszErrors);
}


uint8* CHCP65DriverBuilder::GetModule(void) { return mcModule.GetBytes(); }
size CHCP65DriverBuilder::GetModuleSize(void) { return mcModule.GetSize(); }
CAsm65Linker* CHCP65DriverBuilder::GetLinker(void) { return mcBuilder.GetLinker(); }
const char* CHCP65DriverBuilder::GetErrors(void) { return mszErrors.Text(); }
