#include "BaseLib/FileUtil.h"
#include "HCP65ProgramBuild.h"


//////////////////////////////////////////////////////////////////////////
//
// The Visual Studio project of the common library szLibrary
// (szCommon\szLibrary\szLibrary.vcxproj), linked as a library.
//
//////////////////////////////////////////////////////////////////////////
static bool HCP65AddCommonLibrary(CCPP65Builder* pcBuilder, CChars* pszCommon, const char* szLibrary)
{
	CFileUtil	cFileUtil;
	CChars		szPath;
	bool		bResult;

	szPath.Init(*pszCommon);
	cFileUtil.AppendToPath(&szPath, szLibrary);
	cFileUtil.AppendToPath(&szPath, szLibrary);
	szPath.Append(".vcxproj");
	bResult = pcBuilder->AddVisualStudioProject(szLibrary, szPath.Text(), true);
	szPath.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool HCP65AddProgram(CCPP65Builder* pcBuilder, const char* szCommonLibraries, const char* szHCP65Libraries, const char* szProjectFile, const char* szIncludeDirectory, const char** aszLibraries)
{
	CFileUtil	cFileUtil;
	CChars		szCommon;
	CChars		szLibc65;
	CChars		szProject;
	CChars		szName;
	CChars		szDirectory;
	bool		bResult;
	size		i;

	szCommon.Init(szCommonLibraries);
	cFileUtil.FullPath(&szCommon);
	szLibc65.Init(szHCP65Libraries);
	cFileUtil.FullPath(&szLibc65);
	cFileUtil.AppendToPath(&szLibc65, "Libc65");
	szProject.Init(szProjectFile);
	cFileUtil.FullPath(&szProject);
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szProject.Text(), &szName, &szDirectory);
	cFileUtil.RemoveExtension(&szName);

	pcBuilder->SetTarget(CPPT_W65C816);
	pcBuilder->SetRemoveUnused(true);
	pcBuilder->SetLineTable(true);
	pcBuilder->AddIncludeDirectory(szLibc65.Text(), false, true);
	pcBuilder->AddIncludeDirectory(szDirectory.Text(), true, false);
	if (szIncludeDirectory)
	{
		szDirectory.Set(szIncludeDirectory);
		cFileUtil.FullPath(&szDirectory);
		pcBuilder->AddIncludeDirectory(szDirectory.Text(), false, false);
	}
	pcBuilder->AddIncludeDirectory(szCommon.Text(), true, false);
	szDirectory.Set(szHCP65Libraries);
	cFileUtil.FullPath(&szDirectory);
	pcBuilder->AddIncludeDirectory(szDirectory.Text(), true, false);

	bResult = pcBuilder->AddVisualStudioProject(szName.Text(), szProject.Text(), false);
	bResult &= HCP65AddCommonLibrary(pcBuilder, &szCommon, "BaseLib");
	for (i = 0; aszLibraries && aszLibraries[i]; i++)
	{
		bResult &= HCP65AddCommonLibrary(pcBuilder, &szCommon, aszLibraries[i]);
	}
	pcBuilder->AddLibc65(szLibc65.Text());

	szDirectory.Kill();
	szName.Kill();
	szProject.Kill();
	szLibc65.Kill();
	szCommon.Kill();
	return bResult;
}

