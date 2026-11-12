#include <string.h>
#include "BaseLib/ArrayChars.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "Assembler65Lib/Asm65C64Assembler.h"
#include "Assembler65Lib/Asm65Object.h"
#include "TestLib/Assert.h"
#include "TestAsm65C64Context.h"


#define TEST_ASM65_WEBSITE_65C64	".." _FS_ ".." _FS_ ".." _FS_ "Website" _FS_ "docs" _FS_ "65c64"
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
// The website says every 65C64 example on it assembles: assemble them all
// (Website/docs/65c64, every page).
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64WebsiteExamples(void)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszFiles;
	CChars		szMessage;
	size		i;
	size		uiExamples;

	aszFiles.Init();
	cFileUtil.FindFilesWithExtension(TEST_ASM65_WEBSITE_65C64, "md", &aszFiles, true);
	AssertTrue(aszFiles.NumElements() >= 20);

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
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64Website(void)
{
	BeginTests();

	TestAsm65C64WebsiteExamples();

	TestStatistics();
}
