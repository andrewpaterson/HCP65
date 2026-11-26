#include <string.h>
#include "BaseLib/ArrayChars.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/YAMLFile.h"
#include "Assembler65Lib/Asm65C64Assembler.h"
#include "Assembler65Lib/Asm65Object.h"
#include "Assembler65Lib/Asm65Memory.h"
#include "MicroprocessorLib/Fast65C64.h"
#include "TestLib/Assert.h"
#include "TestAsm65C64Context.h"


#define TEST_ASM65_WEBSITE			".." _FS_ ".." _FS_ ".." _FS_ "Website"
#define TEST_ASM65_WEBSITE_ROOT		".." _FS_ ".." _FS_ ".." _FS_ ".."
#define TEST_ASM65_WEBSITE_65C64	"65c64/"
#define TEST_ASM65_WEBSITE_FENCE	"```asm"
#define TEST_ASM65_WEBSITE_END		"```"
#define TEST_ASM65_WEBSITE_INCLUDED	"--8<--"


//////////////////////////////////////////////////////////////////////////
//
// The length of the line at szLine, without its newline.
//
//////////////////////////////////////////////////////////////////////////
static size TestAsm65WebsiteLineLength(const char* szLine)
{
	const char*	szEnd;

	szEnd = strchr(szLine, '\n');
	if (szEnd)
	{
		return (size)(szEnd - szLine);
	}
	return strlen(szLine);
}


//////////////////////////////////////////////////////////////////////////
//
// True when the line is the fence, with only white space around it.
//
//////////////////////////////////////////////////////////////////////////
static bool TestAsm65WebsiteIsFence(const char* szLine, size uiLength, const char* szFence)
{
	size	uiFence;

	while ((uiLength > 0) && ((szLine[0] == ' ') || (szLine[0] == '\t')))
	{
		szLine++;
		uiLength--;
	}
	while ((uiLength > 0) && ((szLine[uiLength - 1] == ' ') || (szLine[uiLength - 1] == '\t') || (szLine[uiLength - 1] == '\r')))
	{
		uiLength--;
	}

	uiFence = strlen(szFence);
	return (uiLength == uiFence) && (memcmp(szLine, szFence, uiFence) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Assembles one example.  One that uses the object directives is assembled
// as an object, the others in absolute mode.  The message is the errors,
// or empty.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65WebsiteAssemble(const char* szFilename, size uiLine, CChars* pszExample, CChars* pszMessage)
{
	CAsm65C64Assembler	cAssembler;
	CAsm65Object		cObject;
	CChars				szSource;
	bool				bObject;
	bool				bResult;

	bObject = pszExample->Contains(".section") || pszExample->Contains(".global") || pszExample->Contains(".weak") || pszExample->Contains(".extern");

	szSource.Init();
	if (!bObject)
	{
		//One line, so the errors' lines are one more than the example's.
		szSource.Append(".org $2000\n");
	}
	szSource.Append(pszExample->Text());

	cAssembler.Init();
	if (bObject)
	{
		cObject.Init("website");
		bResult = cAssembler.AssembleObject(szSource.Text(), &cObject);
		cObject.Kill();
	}
	else
	{
		bResult = cAssembler.Assemble(szSource.Text());
	}

	if (!bResult)
	{
		pszMessage->Append(szFilename);
		pszMessage->Append(" line ");
		pszMessage->Append((uint64)uiLine);
		pszMessage->Append(":\n");
		pszMessage->Append(cAssembler.GetErrors());
	}
	cAssembler.Kill();
	szSource.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Assembles every ```asm example in a page.  Syntax summaries are fenced
// as text, and an example that is another file's lines (a snippet) is one
// of the feature programs, which have tests of their own.  Returns the
// number of examples assembled.
//
//////////////////////////////////////////////////////////////////////////
static size TestAsm65WebsitePage(const char* szFilename, CChars* pszMessage)
{
	CTextFile	cFile;
	CChars		szExample;
	const char*	szLine;
	size		uiLength;
	size		uiLine;
	size		uiStart;
	size		uiExamples;
	bool		bInExample;

	uiExamples = 0;
	cFile.Init();
	AssertTrue(cFile.Read(szFilename));
	cFile.PassifyNewlines();

	szExample.Init();
	bInExample = false;
	uiStart = 0;
	uiLine = 1;
	szLine = cFile.Text();
	while (szLine[0] != '\0')
	{
		uiLength = TestAsm65WebsiteLineLength(szLine);
		if (!bInExample)
		{
			if (TestAsm65WebsiteIsFence(szLine, uiLength, TEST_ASM65_WEBSITE_FENCE))
			{
				bInExample = true;
				uiStart = uiLine + 1;
				szExample.Clear();
			}
		}
		else if (TestAsm65WebsiteIsFence(szLine, uiLength, TEST_ASM65_WEBSITE_END))
		{
			bInExample = false;
			if (!szExample.Contains(TEST_ASM65_WEBSITE_INCLUDED))
			{
				TestAsm65WebsiteAssemble(szFilename, uiStart, &szExample, pszMessage);
				uiExamples++;
			}
		}
		else
		{
			szExample.Append(szLine, uiLength);
			szExample.Append('\n');
		}

		szLine += uiLength;
		if (szLine[0] == '\n')
		{
			szLine++;
		}
		uiLine++;
	}
	AssertFalse(bInExample);

	szExample.Kill();
	cFile.Kill();
	return uiExamples;
}


//////////////////////////////////////////////////////////////////////////
//
// The pages the website puts in its 65c64 folder.  They live beside their
// code in several repositories; mkdocs.yml's extra.sources maps each
// folder (relative to the folder holding the repositories) to where its
// files land in the site.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65WebsiteFindPages(CArrayChars* paszPages)
{
	CYAMLFile		cYAMLFile;
	CMarkupTag*		pcSources;
	CMarkupTag*		pcSource;
	STagIterator	sIter;
	CFileUtil		cFileUtil;
	CArrayChars		aszFiles;
	CChars			szFolder;
	CChars			szStaged;
	const char*		szSite;
	const char*		szFile;
	size			i;

	cYAMLFile.Init();
	AssertTrue(cYAMLFile.Read("mkdocs.yml", TEST_ASM65_WEBSITE));
	pcSources = cYAMLFile.mcMarkup.mpcDoc->GetRootTag()->GetTag("extra")->GetTag("sources");
	AssertNotNull(pcSources);

	for (pcSource = pcSources->GetTag(&sIter); pcSource != NULL; pcSource = pcSources->GetNextTag(&sIter))
	{
		szFolder.Init(TEST_ASM65_WEBSITE_ROOT);
		cFileUtil.AppendToPath(&szFolder, pcSource->GetStringAttribute("folder"));
		cFileUtil.FullPath(&szFolder);
		AssertString(szFolder.Text(), cFileUtil.IsDirectory(szFolder.Text()) ? szFolder.Text() : "is not a folder");
		szSite = pcSource->GetStringAttribute("site");

		aszFiles.Init();
		cFileUtil.FindFilesWithExtension(szFolder.Text(), "md", &aszFiles, true);
		for (i = 0; i < aszFiles.NumElements(); i++)
		{
			szFile = aszFiles.GetText(i);
			AssertTrue(strncmp(szFile, szFolder.Text(), szFolder.Length()) == 0);

			szStaged.Init(szSite);
			if (!szStaged.Empty())
			{
				szStaged.Append('/');
			}
			szStaged.Append(&szFile[szFolder.Length() + 1]);
			szStaged.Replace('\\', '/');
			if (szStaged.StartsWith(TEST_ASM65_WEBSITE_65C64))
			{
				paszPages->Add(szFile);
			}
			szStaged.Kill();
		}
		aszFiles.Kill();
		szFolder.Kill();
	}
	cYAMLFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The website says every 65C64 example on it assembles: assemble them all
// (every page in the site's 65c64 folder, wherever it lives).
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64WebsiteExamples(void)
{
	CArrayChars	aszFiles;
	CChars		szMessage;
	size		i;
	size		uiExamples;

	aszFiles.Init();
	TestAsm65WebsiteFindPages(&aszFiles);
	AssertTrue(aszFiles.NumElements() >= 30);

	szMessage.Init();
	uiExamples = 0;
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		uiExamples += TestAsm65WebsitePage(aszFiles.GetText(i), &szMessage);
	}
	AssertString("", szMessage.Text());
	AssertTrue(uiExamples >= 60);

	szMessage.Kill();
	aszFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The 65C64 emulator page's example, run: MicroprocessorLib's
// Docs/65c64/emulator.md takes it from here by its two sections' names
// (pymdownx.snippets), so the page shows code that runs.  Asserts aren't
// in the sections.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64WebsiteEmulator(void)
{
	const char*	szSource;

	szSource = ".org $2000\n"
		"	cpy A, 42\n"
		"	stp\n";

	// --8<-- [start:emulator]
	CAsm65C64Assembler	cAssembler;
	CAsm65Memory		cMemory;
	CFast65C64			cMPU;

	cAssembler.Init();
	cAssembler.Assemble(szSource);

	cMemory.Init();
	cMemory.LoadProgram(cAssembler.GetOutput(), 0x2000);	//Also points the reset vector at $2000.
	cMemory.InitFast65C64(&cMPU);
	cAssembler.Kill();

	cMPU.Reset();
	cMPU.Run(1000000);		//Until an stp, or a million instructions.

	cMPU.GetA();			//The registers: GetB, GetX, GetY, GetS, GetV, GetW, GetP.
	cMPU.NumCycles();		//The clock cycles taken.
	cMemory.GetLong(0x3000);	//And the memory.
	// --8<-- [end:emulator]

	AssertLongLongIntHex(42, cMPU.GetA());
	AssertTrue(cMPU.NumCycles() > 0);
	AssertLongLongIntHex(0, cMemory.GetLong(0x3000));

	// --8<-- [start:emulator-kill]
	cMPU.Kill();
	cMemory.Kill();
	// --8<-- [end:emulator-kill]
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64Website(void)
{
	BeginTests();

	TestAsm65C64WebsiteExamples();
	TestAsm65C64WebsiteEmulator();

	TestStatistics();
}
