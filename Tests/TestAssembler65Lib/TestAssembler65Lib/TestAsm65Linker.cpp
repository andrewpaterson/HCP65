#include "BaseLib/MemoryFile.h"
#include "BaseLib/PointerFunctions.h"
#include "TestLib/Assert.h"
#include "Assembler65Lib/Asm65Module.h"
#include "Assembler65Lib/Asm65Executable.h"
#include "BaseLib/FileUtil.h"
#include "TestAsm65C64Context.h"


#define LINKER_MAX_OBJECTS	4


//Objects assembled from sources and the linker that links them.
struct SLinkTest
{
	CAsm65Object	acObjects[LINKER_MAX_OBJECTS];
	int				iNumObjects;
	CAsm65Library	cLibrary;
	CAsm65Linker	cLinker;
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void LinkTestInit(SLinkTest* psTest)
{
	psTest->iNumObjects = 0;
	psTest->cLibrary.Init("library");
	psTest->cLinker.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// The object is named a, b, c ... in the order added.
//
//////////////////////////////////////////////////////////////////////////
static void LinkTestObject(SLinkTest* psTest, const char* szSource)
{
	CAsm65Object*	pcObject;
	char			szName[2];

	szName[0] = (char)('a' + psTest->iNumObjects);
	szName[1] = '\0';
	pcObject = &psTest->acObjects[psTest->iNumObjects];
	psTest->iNumObjects++;
	pcObject->Init(szName);
	TestAsm65AssembleObject(pcObject, szSource);
	psTest->cLinker.AddObject(pcObject);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void LinkTestLibraryObject(SLinkTest* psTest, const char* szName, const char* szSource)
{
	TestAsm65AssembleObject(psTest->cLibrary.AddObject(szName), szSource);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void LinkTestKill(SLinkTest* psTest)
{
	int	i;

	psTest->cLinker.Kill();
	psTest->cLibrary.Kill();
	for (i = 0; i < psTest->iNumObjects; i++)
	{
		psTest->acObjects[i].Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Links, runs from the start of the text and returns A.
//
//////////////////////////////////////////////////////////////////////////
static uint64 LinkAndRun(SLinkTest* psTest, STestAsm65Run* psRun)
{
	AssertTrue(psTest->cLinker.Link());
	AssertString("", psTest->cLinker.GetErrors());
	TestAsm65BeginOutput(psRun, psTest->cLinker.GetOutput(), ASM65_DEFAULT_TEXT_ADDRESS);
	AssertTrue(TestAsm65Run(psRun, 10000) < 10000);
	return TestAsm65MPU(psRun)->GetA();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static int64 LinkedSymbol(SLinkTest* psTest, const char* szName)
{
	int64	iValue;

	iValue = -1;
	AssertTrue(psTest->cLinker.GetSymbol(szName, &iValue));
	return iValue;
}


//////////////////////////////////////////////////////////////////////////
//
// A call to a function in another object, data read through an address in another
// section and a branch between sections.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerCall(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"Start:\n"
		"	cpy A, [Four]\n"
		"	jsr AddThree\n"
		"	bras z, Done\n"
		"	stp\n"
		"	.section rodata\n"
		"Four:\n"
		"	.long 4\n"
		"	.section text.done\n"
		"Done:\n"
		"	add A, 100\n"
		"	stp\n");
	LinkTestObject(&sTest,
		"	.global AddThree\n"
		"AddThree:\n"
		"	add A, 3\n"
		"	cmp A, 7\n"
		"	ret\n");

	AssertLong(107, LinkAndRun(&sTest, &sRun));
	AssertString(
		"Sections\n"
		"  0x00010000 0x00000014 text (a)\n"
		"  0x00010014 0x00000006 text.done (a)\n"
		"  0x0001001a 0x00000006 text (b)\n"
		"  0x00010020 0x00000004 rodata (a)\n"
		"Symbols\n"
		"  0x00010000 L Start (a)\n"
		"  0x00010000 G __text_start (linker)\n"
		"  0x00010014 L Done (a)\n"
		"  0x0001001a G AddThree (b)\n"
		"  0x00010020 L Four (a)\n"
		"  0x00010020 G __rodata_start (linker)\n"
		"  0x00010020 G __text_end (linker)\n"
		"  0x00010024 G __bss_end (linker)\n"
		"  0x00010024 G __bss_start (linker)\n"
		"  0x00010024 G __data_end (linker)\n"
		"  0x00010024 G __data_start (linker)\n"
		"  0x00010024 G __end (linker)\n"
		"  0x00010024 G __fini_end (linker)\n"
		"  0x00010024 G __fini_start (linker)\n"
		"  0x00010024 G __init_end (linker)\n"
		"  0x00010024 G __init_start (linker)\n"
		"  0x00010024 G __rodata_end (linker)\n",
		sTest.cLinker.GetMap());
	AssertLongLongIntHex(0x10000, LinkedSymbol(&sTest, "__text_start"));
	AssertLongLongIntHex(0x1001A, LinkedSymbol(&sTest, "AddThree"));
	AssertLongLongIntHex(0x10020, LinkedSymbol(&sTest, "__text_end"));
	AssertLongLongIntHex(0x10020, LinkedSymbol(&sTest, "__rodata_start"));
	AssertLongLongIntHex(0x10024, LinkedSymbol(&sTest, "__end"));
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// :16 addresses work while the data is below 0001:0000 and are an error above.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerOffsetAddresses(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;
	const char*		szSource;

	szSource =
		"	wth A, 32\n"
		"	cpy A, [Value:16]\n"
		"	add A, Value:16\n"
		"	stp\n"
		"	.section data\n"
		"Value:\n"
		"	.long $1234\n";

	LinkTestInit(&sTest);
	sTest.cLinker.SetAddress(ASK65_Data, 0x8000);
	LinkTestObject(&sTest, szSource);
	AssertLongLongIntHex(0x1234 + 0x8000, LinkAndRun(&sTest, &sRun));
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);

	LinkTestInit(&sTest);
	sTest.cLinker.SetAddress(ASK65_Data, 0x30000);
	LinkTestObject(&sTest, szSource);
	AssertFalse(sTest.cLinker.Link());
	AssertString(
		"a:2:10: Relocated value does not fit '196608 in 2 bytes'\n"
		"a:3:9: Relocated value does not fit '196608 in 2 bytes'\n",
		sTest.cLinker.GetErrors());
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// A label that ends its section, with nothing after it, is at that
// section's end and not at the start of the section that follows (a
// vtable's label is, when the class has no virtual functions).
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerLabelEndsSection(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	cpy A, [Table - 8]\n"
		"	add A, [Table - 4]\n"
		"	stp\n"
		"	.section rodata.table\n"
		"	.global Table\n"
		"	.long 40\n"
		"	.long 2\n"
		"Table:\n"
		"	.section text.after\n"
		"After:\n"
		"	add A, 1000\n"
		"	stp\n");

	AssertLong(42, LinkAndRun(&sTest, &sRun));
	AssertLongLongIntHex(LinkedSymbol(&sTest, "__rodata_end"), LinkedSymbol(&sTest, "Table"));
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// A global definition replaces a weak one; an undefined weak name is 0.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerWeak(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	.weak Get, Optional\n"
		"	jsr Get\n"
		"	add A, Optional\n"
		"	stp\n"
		"Get:\n"
		"	cpy A, 1\n"
		"	ret\n");
	LinkTestObject(&sTest,
		"	.global Get\n"
		"Get:\n"
		"	cpy A, 2\n"
		"	ret\n");
	AssertLong(2, LinkAndRun(&sTest, &sRun));
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// Both objects have the COMDAT group Shared: the first copy is kept and both call it.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerComdat(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	cpy A, 0\n"
		"	jsr Shared\n"
		"	jsr Second\n"
		"	stp\n"
		"	.section text.Shared, comdat\n"
		"	.global Shared\n"
		"Shared:\n"
		"	add A, 1\n"
		"	ret\n");
	LinkTestObject(&sTest,
		"	.global Second\n"
		"Second:\n"
		"	jsr Shared\n"
		"	shl A, 4\n"
		"	ret\n"
		"	.section text.Shared, comdat\n"
		"	.global Shared\n"
		"Shared:\n"
		"	add A, 2\n"
		"	ret\n");
	AssertLong(0x20, LinkAndRun(&sTest, &sRun));
	AssertTrue(strstr(sTest.cLinker.GetMap(), "Dropped COMDAT sections\n  text.Shared comdat text.Shared (b)\n") != NULL);
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);

	//A local label in a dropped copy cannot be used.
	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	.section text.Shared, comdat\n"
		"Copy:\n"
		"	ret\n");
	LinkTestObject(&sTest,
		"	jsr Copy\n"
		"	.section text.Shared, comdat\n"
		"Copy:\n"
		"	ret\n");
	AssertFalse(sTest.cLinker.Link());
	AssertString("b:1:6: Refers to a dropped COMDAT section 'text.Shared'\n", sTest.cLinker.GetErrors());
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// A start up that calls the init table (in link order) then the fini table backwards.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerInitFini(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	wtth (A, X), 32\n"
		"	cpy A, 0\n"
		"	cpy X, __init_start\n"
		"NextInit:\n"
		"	cmp X, __init_end\n"
		"	bras c, Finalise\n"
		"	jsr [X]\n"
		"	add X, 4\n"
		"	bra NextInit\n"
		"Finalise:\n"
		"	cpy X, __fini_end\n"
		"NextFini:\n"
		"	cmp X, __fini_start\n"
		"	bras z, Run\n"
		"	sub X, 4\n"
		"	jsr [X]\n"
		"	bra NextFini\n"
		"Run:\n"
		"	stp\n"
		"	.section init\n"
		"	.long First\n"
		"	.section fini\n"
		"	.long FirstEnd\n"
		"	.section text\n"
		"First:\n"
		"	shl A, 4\n"
		"	add A, 1\n"
		"	ret\n"
		"FirstEnd:\n"
		"	shl A, 4\n"
		"	add A, 4\n"
		"	ret\n");
	LinkTestObject(&sTest,
		"	.section init\n"
		"	.long Second\n"
		"	.section fini\n"
		"	.long SecondEnd\n"
		"	.section text\n"
		"Second:\n"
		"	shl A, 4\n"
		"	add A, 2\n"
		"	ret\n"
		"SecondEnd:\n"
		"	shl A, 4\n"
		"	add A, 3\n"
		"	ret\n");
	AssertLongLongIntHex(0x1234, LinkAndRun(&sTest, &sRun));
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// Library objects are linked only when needed, including by other library objects.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerLibrary(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	wth A, 32\n"
		"	cpy A, 1\n"
		"	jsr Outer\n"
		"	stp\n");
	LinkTestLibraryObject(&sTest, "inner",
		"	.global Inner\n"
		"Inner:\n"
		"	add A, 20\n"
		"	ret\n");
	LinkTestLibraryObject(&sTest, "unused",
		"	.global Unused\n"
		"Unused:\n"
		"	ret\n");
	LinkTestLibraryObject(&sTest, "outer",
		"	.global Outer\n"
		"Outer:\n"
		"	add A, 300\n"
		"	jsr Inner\n"
		"	ret\n");
	sTest.cLinker.AddLibrary(&sTest.cLibrary);
	AssertLong(321, LinkAndRun(&sTest, &sRun));
	AssertTrue(LinkedSymbol(&sTest, "Inner") > 0);
	AssertTrue(strstr(sTest.cLinker.GetMap(), "unused") == NULL);
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// A library written as a PackFile reads back and links the same.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerLibraryFile(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;
	CAsm65Library	cLibrary;
	CMemoryFile*	pcMemoryFile;

	cLibrary.Init("maths");
	TestAsm65AssembleObject(cLibrary.AddObject("double"),
		"	.global Double\n"
		"Double:\n"
		"	add A, A\n"
		"	ret\n");
	TestAsm65AssembleObject(cLibrary.AddObject("square"),
		"	.global Square\n"
		"Square:\n"
		"	cpy B, [Table + A]\n"
		"	cpy A, B\n"
		"	ret\n"
		"	.section rodata\n"
		"Table:\n"
		"	.byte 0, 1, 4, 9, 16, 25, 36, 49\n");
	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	AssertTrue(cLibrary.Write(pcMemoryFile));
	cLibrary.Kill();

	LinkTestInit(&sTest);
	pcMemoryFile->mbBasicFileMustFree = false;
	AssertTrue(sTest.cLibrary.Read(pcMemoryFile));
	SafeKill(pcMemoryFile);
	AssertString("maths", sTest.cLibrary.GetName());
	AssertSize(2, sTest.cLibrary.NumObjects());
	AssertString("double", sTest.cLibrary.GetObject(0)->GetName());
	AssertString("square", sTest.cLibrary.GetObject(1)->GetName());

	LinkTestObject(&sTest,
		"	cpy A, 3\n"
		"	jsr Square\n"
		"	jsr Double\n"
		"	stp\n");
	sTest.cLinker.AddLibrary(&sTest.cLibrary);
	AssertLong(18, LinkAndRun(&sTest, &sRun));
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// Undefined and twice defined names, overlapping sections and the limit.  With
// SetStubMissing an undefined name is a stp.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerErrors(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest, "	jsr Missing\n");
	AssertFalse(sTest.cLinker.Link());
	AssertString("a: Undefined symbol 'Missing'\n", sTest.cLinker.GetErrors());
	LinkTestKill(&sTest);

	LinkTestInit(&sTest);
	sTest.cLinker.SetStubMissing(true);
	LinkTestObject(&sTest,
		"	cpy A, 5\n"
		"	jsr Missing\n"
		"	cpy A, 6\n"
		"	stp\n");
	AssertLong(5, LinkAndRun(&sTest, &sRun));
	AssertSize(1, sTest.cLinker.NumMissing());
	AssertString("Missing", sTest.cLinker.GetMissing(0));
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);

	LinkTestInit(&sTest);
	LinkTestObject(&sTest, "	.global Twice\nTwice:\n	ret\n");
	LinkTestObject(&sTest, "	.global Twice\nTwice:\n	ret\n");
	AssertFalse(sTest.cLinker.Link());
	AssertString("b: Symbol is also defined in a 'Twice'\n", sTest.cLinker.GetErrors());
	LinkTestKill(&sTest);

	LinkTestInit(&sTest);
	sTest.cLinker.SetAddress(ASK65_Data, 0x10002);
	sTest.cLinker.SetLimit(0x10004);
	LinkTestObject(&sTest, "	nop\n	nop\n	nop\n	.section data\n	.long 0\n");
	AssertFalse(sTest.cLinker.Link());
	AssertString(
		"link: Sections overlap: text and data\n"
		"link: Sections end past the limit 0x00010004 'text'\n"
		"link: Sections end past the limit 0x00010004 'data'\n",
		sTest.cLinker.GetErrors());
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// SetRemoveUnused: what the keep section reaches (Used, gUsed), the init entry that
// is not associated (InitAlways) and the one whose global is used (InitUsed) are
// linked.  Unused, gUnused, the init entry associated with gUnused and its routine
// are removed, and Missing (only Unused needs it) is not missing.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerRemoveUnused(void)
{
	SLinkTest			sTest;
	STestAsm65Run		sRun;
	const char*			szSource;
	int64				iValue;
	CAsm65Executable	cExecutable;
	CChars				szErrors;

	szSource =
		"	.section text, keep\n"
		"	wtth (A, X), 32\n"
		"	cpy A, 0\n"
		"	cpy X, __init_start\n"
		"NextInit:\n"
		"	cmp X, __init_end\n"
		"	bras c, Run\n"
		"	jsr [X]\n"
		"	add X, 4\n"
		"	bra NextInit\n"
		"Run:\n"
		"	jsr Used\n"
		"	add A, [gUsed]\n"
		"	stp\n"
		"	.section text.Used\n"
		"	.global Used\n"
		"Used:\n"
		"	add A, 100\n"
		"	ret\n"
		"	.section text.Unused\n"
		"	.global Unused\n"
		"Unused:\n"
		"	jsr Missing\n"
		"	cpy B, [gUnused]\n"
		"	ret\n"
		"	.section data.gUsed\n"
		"gUsed:\n"
		"	.long 20\n"
		"	.section data.gUnused\n"
		"gUnused:\n"
		"	.long 30\n"
		"	.section text.InitUsed\n"
		"InitUsed:\n"
		"	add A, 1\n"
		"	ret\n"
		"	.section init.InitUsed, associated data.gUsed\n"
		"	.long InitUsed\n"
		"	.section text.InitUnused\n"
		"InitUnused:\n"
		"	add A, 1000\n"
		"	ret\n"
		"	.section init.InitUnused, associated data.gUnused\n"
		"	.long InitUnused\n"
		"	.section text.InitAlways\n"
		"InitAlways:\n"
		"	add A, 3\n"
		"	ret\n"
		"	.section init.InitAlways\n"
		"	.long InitAlways\n";

	//Everything is linked without SetRemoveUnused, so Missing is needed.
	LinkTestInit(&sTest);
	LinkTestObject(&sTest, szSource);
	AssertFalse(sTest.cLinker.Link());
	AssertString("a: Undefined symbol 'Missing'\n", sTest.cLinker.GetErrors());
	LinkTestKill(&sTest);

	LinkTestInit(&sTest);
	sTest.cLinker.SetRemoveUnused(true);
	LinkTestObject(&sTest, szSource);
	AssertLong(1 + 3 + 100 + 20, LinkAndRun(&sTest, &sRun));
	AssertSize(0, sTest.cLinker.NumMissing());
	AssertTrue(sTest.cLinker.GetSymbol("Used", &iValue));
	AssertFalse(sTest.cLinker.GetSymbol("Unused", &iValue));

	//An executable whose entry was removed, or never defined, says which.
	cExecutable.Init("unused.exe", "Unused", 0);
	szErrors.Init();
	AssertFalse(cExecutable.Make(sTest.cLinker.GetOutput(), &sTest.cLinker, &szErrors));
	AssertString("The entry Unused was removed: nothing in the link uses it.\n", szErrors.Text());
	szErrors.Kill();
	cExecutable.Kill();
	szErrors.Init();
	sTest.cLinker.DescribeMissing("The entry", "Nowhere", &szErrors);
	AssertString("The entry Nowhere is not defined.\n", szErrors.Text());
	szErrors.Kill();
	AssertTrue(strstr(sTest.cLinker.GetMap(),
		"Removed unused sections\n"
		"  0x0000000e text.Unused (a)\n"
		"  0x00000004 data.gUnused (a)\n"
		"  0x00000006 text.InitUnused (a)\n"
		"  0x00000004 init.InitUnused (a)\n") != NULL);
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// With SetRemoveUnused a library object is only linked for a name a used section
// needs: Unused needs Helper, so helper (and its init entry, which would add 1000)
// is left out.  Used needs Other, so other is linked.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerRemoveUnusedLibrary(void)
{
	SLinkTest		sTest;
	STestAsm65Run	sRun;
	int64			iValue;

	LinkTestInit(&sTest);
	sTest.cLinker.SetRemoveUnused(true);
	LinkTestObject(&sTest,
		"	.section text, keep\n"
		"	wtth (A, X), 32\n"
		"	cpy A, 0\n"
		"	cpy X, __init_start\n"
		"NextInit:\n"
		"	cmp X, __init_end\n"
		"	bras c, Run\n"
		"	jsr [X]\n"
		"	add X, 4\n"
		"	bra NextInit\n"
		"Run:\n"
		"	jsr Other\n"
		"	stp\n"
		"	.section text.Unused\n"
		"Unused:\n"
		"	jsr Helper\n"
		"	ret\n");
	TestAsm65AssembleObject(sTest.cLibrary.AddObject("helper"),
		"	.global Helper\n"
		"Helper:\n"
		"	ret\n"
		"	.section text.Init\n"
		"Init:\n"
		"	add A, 1000\n"
		"	ret\n"
		"	.section init\n"
		"	.long Init\n");
	TestAsm65AssembleObject(sTest.cLibrary.AddObject("other"),
		"	.global Other\n"
		"Other:\n"
		"	add A, 7\n"
		"	ret\n");
	sTest.cLinker.AddLibrary(&sTest.cLibrary);
	AssertLong(7, LinkAndRun(&sTest, &sRun));
	AssertFalse(sTest.cLinker.GetSymbol("Helper", &iValue));
	AssertTrue(strstr(sTest.cLinker.GetMap(), "(helper)") == NULL);
	AssertTrue(strstr(sTest.cLinker.GetMap(), "(other)") != NULL);
	TestAsm65End(&sRun);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// Feature programs assembled as objects and linked give the same results.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerPrograms(void)
{
	STestAsm65Run	sRun;
	CChars			szSource;

	szSource.Init();
	TestAsm65ReadSource(&szSource, "Sieve.asm");
	AssertTrue(TestAsm65BeginLinked(&sRun, szSource.Text(), 0x1000));
	AssertTrue(TestAsm65Run(&sRun, 1000000) < 1000000);
	AssertInt(1229, sRun.pcContext->GetLong(0x0100));
	AssertInt(9973, sRun.pcContext->GetLong(0x20000 + 4 * 1228));
	TestAsm65End(&sRun);
	szSource.Kill();

	szSource.Init();
	TestAsm65ReadSource(&szSource, "Sha256.asm");
	szSource.Append("\t\t.byte \"abc\"\nMessageEnd:\n");
	AssertTrue(TestAsm65BeginLinked(&sRun, szSource.Text(), 0x1000));
	AssertTrue(TestAsm65Run(&sRun, 1000000) < 1000000);
	AssertIntHex(0xba7816bf, sRun.pcContext->GetLong(0x0200));
	AssertIntHex(0xf20015ad, sRun.pcContext->GetLong(0x0200 + 28));
	TestAsm65End(&sRun);
	szSource.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The objects' source lines (.line) are the linked program's line table, by
// address: past the end of a section's lines there is no line.  Written
// and read back in the program's debug info, the table is the same.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerLineTable(void)
{
	SLinkTest			sTest;
	CAsm65LineTable*	pcTable;
	CAsm65LineTable*	pcRead;
	CAsm65DebugInfo		cRead;
	CAsm65DebugInfo		cEmpty;
	CFileUtil			cFileUtil;
	filePos				iBigFile;
	CMemoryFile*		pcMemoryFile;
	SAsm65LineEntry*	psLine;
	SAsm65LineEntry*	psRead;
	CArrayUint32		auiAddresses;
	uint32				uiNext;
	int64				iStart;
	int64				iOther;
	size				ui;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	.global Start\n"
		"Start:\n"
		"	.line \"a.cpp\", 10\n"
		"	jsr Other\n"
		"	.line \"a.cpp\", 11\n"
		"	ret\n"
		"	.line \"a.cpp\", 10\n"
		"	nop\n");
	LinkTestObject(&sTest,
		"	.global Other\n"
		"Other:\n"
		"	.line \"b.asm\", 3\n"
		"	nop\n"
		"	ret\n");
	AssertTrue(sTest.cLinker.Link());
	AssertString("", sTest.cLinker.GetErrors());
	iStart = LinkedSymbol(&sTest, "Start");
	iOther = LinkedSymbol(&sTest, "Other");

	pcTable = sTest.cLinker.GetLineTable();
	psLine = pcTable->FindLine((uint32)iStart);
	AssertTrue(psLine != NULL);
	AssertString("a.cpp", pcTable->GetFile(psLine->uiFile));
	AssertInt(10, psLine->uiLine);
	psLine = pcTable->FindLine((uint32)iOther + 2);
	AssertTrue(psLine != NULL);
	AssertString("b.asm", pcTable->GetFile(psLine->uiFile));
	AssertInt(3, psLine->uiLine);

	//A line's code in each place it starts; a line without code is the next line with some; paths in either case
	//and with either slash are the same file.
	auiAddresses.Init();
	AssertInt(10, pcTable->FindLineAddresses("a.cpp", 10, &auiAddresses));
	AssertSize(2, auiAddresses.NumElements());
	AssertInt((uint32)iStart, auiAddresses.GetValue(0));
	AssertTrue(auiAddresses.GetValue(1) > auiAddresses.GetValue(0));
	auiAddresses.ReInit();
	AssertInt(10, pcTable->FindLineAddresses("A.CPP", 4, &auiAddresses));
	AssertSize(2, auiAddresses.NumElements());
	auiAddresses.ReInit();
	AssertInt(11, pcTable->FindLineAddresses("a.cpp", 11, &auiAddresses));
	AssertSize(1, auiAddresses.NumElements());
	auiAddresses.ReInit();
	AssertInt(0, pcTable->FindLineAddresses("a.cpp", 12, &auiAddresses));
	AssertInt(0, pcTable->FindLineAddresses("c.cpp", 1, &auiAddresses));
	AssertSize(0, auiAddresses.NumElements());

	//Line 10's code in both places, each up to the next line's; the address can be anywhere in the line's code.
	AssertTrue(pcTable->FindLineRanges((uint32)iStart + 1, &auiAddresses));
	AssertSize(4, auiAddresses.NumElements());
	AssertInt((uint32)iStart, auiAddresses.GetValue(0));
	pcTable->FindLineAddresses("a.cpp", 11, &auiAddresses);
	AssertSize(5, auiAddresses.NumElements());
	AssertInt(auiAddresses.GetValue(4), auiAddresses.GetValue(1));
	AssertTrue(auiAddresses.GetValue(2) > auiAddresses.GetValue(4));
	AssertTrue(auiAddresses.GetValue(3) > auiAddresses.GetValue(2));
	auiAddresses.ReInit();
	AssertFalse(pcTable->FindLineRanges(0, &auiAddresses));
	AssertSize(0, auiAddresses.NumElements());

	//The next line after an address: Other's first (b.asm 3) from before it; none after the last.
	AssertTrue(pcTable->FindNextLine((uint32)iOther - 1, &uiNext));
	AssertInt((uint32)iOther, uiNext);
	AssertFalse(pcTable->FindNextLine(0xFFFFFFF0, &uiNext));
	pcTable->AddLine(0x123456, "Dir\\Sub/File.asm", 7);
	pcTable->Sort();
	AssertInt(7, pcTable->FindLineAddresses("dir/sub\\file.ASM", 7, &auiAddresses));
	AssertSize(1, auiAddresses.NumElements());
	AssertInt(0x123456, auiAddresses.GetValue(0));
	auiAddresses.Kill();

	//Written in the program's debug info and read back.
	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	AssertTrue(sTest.cLinker.GetDebugInfo()->Write(pcMemoryFile));
	cRead.Init();
	AssertTrue(cRead.Read(MemoryFile(pcMemoryFile->GetBufferPointer(), pcMemoryFile->GetBufferSize())));
	pcRead = cRead.GetLines();
	AssertSize(pcTable->NumFiles(), pcRead->NumFiles());
	for (ui = 0; ui < pcTable->NumFiles(); ui++)
	{
		AssertString(pcTable->GetFile(ui), pcRead->GetFile(ui));
	}
	AssertSize(pcTable->NumLines(), pcRead->NumLines());
	for (ui = 0; ui < pcTable->NumLines(); ui++)
	{
		psLine = pcTable->GetLine(ui);
		psRead = pcRead->GetLine(ui);
		AssertInt(psLine->uiAddress, psRead->uiAddress);
		AssertInt(psLine->uiFile, psRead->uiFile);
		AssertInt(psLine->uiLine, psRead->uiLine);
	}
	psRead = pcRead->FindLine((uint32)iStart + 3);
	AssertTrue(psRead != NULL);
	AssertInt(10, psRead->uiLine);
	AssertSize(pcTable->NumFingerprints(), pcRead->NumFingerprints());

	//Moved (a module's, from its link's addresses to its own): each line is where it was less the start.
	cRead.Move(-iStart);
	psRead = pcRead->FindLine(3);
	AssertTrue(psRead != NULL);
	AssertInt(10, psRead->uiLine);
	AssertInt(pcTable->GetLine(0)->uiAddress - (uint32)iStart, pcRead->GetLine(0)->uiAddress);
	cRead.Kill();

	//Written over a bigger file, empty debug info's file is its own size (a rebuild's .debug doesn't grow).
	cFileUtil.TouchDir("Output");
	AssertTrue(sTest.cLinker.GetDebugInfo()->WriteFile("Output" _FS_ "Rewrite.debug"));
	iBigFile = cFileUtil.Size("Output" _FS_ "Rewrite.debug");
	cEmpty.Init();
	AssertTrue(cEmpty.WriteFile("Output" _FS_ "Rewrite.debug"));
	AssertTrue(cFileUtil.Size("Output" _FS_ "Rewrite.debug") < iBigFile);
	AssertTrue(cEmpty.ReadFile("Output" _FS_ "Rewrite.debug"));
	AssertSize(0, cEmpty.GetLines()->NumLines());
	cEmpty.Kill();
	cFileUtil.Delete("Output" _FS_ "Rewrite.debug");

	//Not debug info: nothing is read.
	cRead.Init();
	AssertFalse(cRead.Read(MemoryFile((void*)"Not a debug file at all.", 24)));
	AssertSize(0, cRead.GetLines()->NumLines());
	cRead.Kill();

	SafeKill(pcMemoryFile);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// The fixup at uiAddress, NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
static SAsm65Fixup* LinkedFixup(SLinkTest* psTest, uint32 uiAddress)
{
	size	i;

	for (i = 0; i < psTest->cLinker.NumFixups(); i++)
	{
		if (psTest->cLinker.GetFixup(i)->uiAddress == uiAddress)
		{
			return psTest->cLinker.GetFixup(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A relocatable link (a kernal driver's): a name nothing defines is an
// import, not an error or a stub; the fields that change when the link is
// moved are fixups (its own addresses, and the imports with their
// addends); a relative branch inside it isn't one.  KeepSymbol keeps the
// entry nothing calls.  The module made from it has one section of the
// link's size, its bytes up to the bss, the entry and the init table as
// offsets, the imports as undefined symbols and a relocation per fixup.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerRelocatable(void)
{
	SLinkTest			sTest;
	SAsm65Fixup*		psFixup;
	CAsm65Module		cModule;
	CAsm65Object		cObject;
	CMemoryFile*		pcFile;
	CChars				szErrors;
	SAsm65Section*		psSection;
	SAsm65Relocation*	psRelocation;
	int64				iEntry;
	int64				iData;
	int32				iSymbol;
	size				i;
	size				uiImports;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	.section text.Entry\n"
		"	.global Entry\n"
		"Entry:\n"
		"	jsr Kernal\n"
		"	add A, [gData]\n"
		"	bra Done\n"
		"Done:\n"
		"	ret\n"
		"	.section text.Unused\n"
		"Unused:\n"
		"	jsr Other\n"
		"	ret\n"
		"	.section data.gData\n"
		"	.global gData\n"
		"gData:\n"
		"	.long Entry\n"
		"	.long Kernal + 8\n"
		"	.section bss.gBss\n"
		"gBss:\n"
		"	.space 16\n"
		"	.section text.UsesBss\n"
		"UsesBss:\n"
		"	add A, [gBss]\n"
		"	ret\n"
		"	.section init.UsesBss\n"
		"	.long UsesBss\n");
	sTest.cLinker.SetRemoveUnused(true);
	sTest.cLinker.SetRelocatable(true);
	sTest.cLinker.SetStubMissing(true);
	sTest.cLinker.KeepSymbol("Entry");
	AssertTrue(sTest.cLinker.Link());
	AssertString("", sTest.cLinker.GetErrors());

	//Only Kernal is an import: Other is only used by removed code.  Nothing was stubbed.
	AssertSize(1, sTest.cLinker.NumMissing());
	AssertString("Kernal", sTest.cLinker.GetMissing(0));
	iEntry = LinkedSymbol(&sTest, "Entry");
	iData = LinkedSymbol(&sTest, "gData");
	AssertLong(ASM65_DEFAULT_TEXT_ADDRESS, iEntry);

	psFixup = LinkedFixup(&sTest, (uint32)iData);
	AssertNotNull(psFixup);
	AssertInt(-1, psFixup->iImport);
	AssertLong(iEntry, psFixup->iValue);
	psFixup = LinkedFixup(&sTest, (uint32)iData + 4);
	AssertNotNull(psFixup);
	AssertInt(0, psFixup->iImport);
	AssertLong(8, psFixup->iValue);
	AssertFalse(psFixup->bRelative);
	uiImports = 0;
	for (i = 0; i < sTest.cLinker.NumFixups(); i++)
	{
		psFixup = sTest.cLinker.GetFixup(i);
		if (psFixup->iImport >= 0)
		{
			uiImports++;
		}
		else
		{
			AssertFalse(psFixup->bRelative);
		}
	}
	AssertSize(2, uiImports);

	szErrors.Init();
	cModule.Init("Test.DRV", "Entry", 0);
	AssertTrue(cModule.Make(sTest.cLinker.GetOutput(), &sTest.cLinker, &szErrors));
	AssertString("", szErrors.Text());
	AssertInt((uint32)LinkedSymbol(&sTest, "__text_start"), cModule.GetBase());

	pcFile = MemoryFile(cModule.GetBytes(), (filePos)cModule.GetSize());  //Read frees it.
	cObject.Init("");
	AssertTrue(cObject.Read(pcFile));
	AssertSize(1, cObject.NumSections());
	psSection = cObject.GetSection(0);
	AssertString(ASM65_MODULE_SECTION, psSection->szName.Text());
	AssertInt((uint32)(LinkedSymbol(&sTest, "__end") - iEntry), psSection->uiSize);
	AssertInt((uint32)(LinkedSymbol(&sTest, "__bss_start") - iEntry), (uint32)psSection->cBytes.NumElements());
	iSymbol = cObject.FindSymbol("Entry");
	AssertTrue(iSymbol >= 0);
	AssertLong(0, cObject.GetSymbol((size)iSymbol)->iValue);
	iSymbol = cObject.FindSymbol(ASM65_MODULE_INIT_END);
	AssertTrue(iSymbol >= 0);
	AssertLong(LinkedSymbol(&sTest, "__init_end") - iEntry, cObject.GetSymbol((size)iSymbol)->iValue);
	iSymbol = cObject.FindSymbol("Kernal");
	AssertTrue(iSymbol >= 0);
	AssertInt(ASM65_SECTION_UNDEFINED, cObject.GetSymbol((size)iSymbol)->iSection);

	AssertSize(sTest.cLinker.NumFixups(), cObject.NumRelocations());
	for (i = 0; i < cObject.NumRelocations(); i++)
	{
		psRelocation = cObject.GetRelocation(i);
		if (psRelocation->uiOffset == (uint32)(iData + 4 - iEntry))
		{
			AssertInt(ASM65_TARGET_SYMBOL, psRelocation->iTargetSection);
			AssertInt((uint32)iSymbol, psRelocation->uiTargetSymbol);
			AssertLong(8, psRelocation->iAddend);
		}
		else if (psRelocation->uiOffset == (uint32)(iData - iEntry))
		{
			AssertInt(0, psRelocation->iTargetSection);
			AssertLong(0, psRelocation->iAddend);
		}
	}

	cObject.Kill();
	cModule.Kill();
	szErrors.Kill();
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// The debug directives describe each unit's functions, variables and
// types; linked, they are at their sections' addresses, a type two units
// describe is one (the complete description over an incomplete one), and
// a function left out (a dropped COMDAT copy, a library object nothing
// uses) is left out of them too.  Written and read back, moved.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerDebugRecords(void)
{
	SLinkTest				sTest;
	CAsm65DebugRecords*		pcRecords;
	CAsm65DebugInfo			cRead;
	CAsm65Object			cObject;
	CMemoryFile*			pcMemoryFile;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugVariable*	psVariable;
	SAsm65DebugGlobal*		psGlobal;
	SAsm65DebugType*		psType;
	SAsm65DebugType*		psInt;
	SAsm65DebugMember*		psMember;
	SAsm65DebugSaved*		psSaved;
	int64					iStart;
	int64					iBody;
	int64					iOther;
	int64					iHandler;
	int64					iCount;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	.global Start\n"
		"Start:\n"
		"	.dfunction \"Start\"\n"
		"	psh V\n"
		"	.global Body\n"
		"Body:\n"
		"	.dframe V, 14\n"
		"	.dsaved V, -7\n"
		"	.dsaved [0x80], 4, -11\n"
		"	.dbase V\n"
		"	jsr Other\n"
		"	jsr Shared\n"
		"	ret\n"
		"	.dvariable parameter, \"iCount\", 0, frame, 9, 10, 20\n"
		"	.dvariable local, \"pcShape\", 1, frame, 2, 12, 18\n"
		"	.dstatic \"siCalls\", 0, Calls\n"
		"	.section text.Shared, comdat\n"
		"	.global Shared\n"
		"Shared:\n"
		"	.dfunction \"Shared\"\n"
		"	.dframe [V - 4], 8\n"
		"	ret\n"
		"	.section data\n"
		"	.global Count\n"
		"Count:\n"
		"	.word 0\n"
		"Calls:\n"
		"	.word 0\n"
		"	.dglobal \"giCount\", 0, Count\n"
		"	.dtype 0, \"int\", \"int\", integer, 1, 2, -1, 0\n"
		"	.dtype 1, \"*CShape\", \"CShape*\", pointer, 0, 4, 2, 0\n"
		"	.dtype 2, \"CShape\", \"CShape\", class, 8, 0, -1, 0\n");
	LinkTestObject(&sTest,
		"	.global Other\n"
		"Other:\n"
		"	.dfunction \"CShape::Area\", short, 1\n"
		"	ret\n"
		"	.dend\n"
		"	.global Stray\n"
		"Stray:\n"
		"	ret\n"
		"	.global Handler\n"
		"Handler:\n"
		"	.dfunction \"[Interrupt]\", userinterrupt\n"
		"	.dframe S, 4\n"
		"	.dsaved S, -1\n"
		"	ret\n"
		"	.section text.Shared, comdat\n"
		"	.global Shared\n"
		"Shared:\n"
		"	.dfunction \"Shared\"\n"
		"	.dframe [V - 4], 8\n"
		"	ret\n"
		"	.dtype 0, \"CShape\", \"CShape\", class, 0, 8, -1, 0\n"
		"	.dmember vtable, \"\", -1, 0, 0, 0\n"
		"	.dmember field, \"meKind\", 2, 4, 0, 0\n"
		"	.dmember field, \"miSides\", 1, 6, 3, 5\n"
		"	.dtype 1, \"int\", \"int\", integer, 1, 2, -1, 0\n"
		"	.dtype 2, \"EKind\", \"EKind\", enum, 0, 2, 1, 0\n"
		"	.dmember enumerator, \"EK_One\", -1, 1, 0, 0\n"
		"	.dmember enumerator, \"EK_Two\", -1, -2, 0, 0\n");
	LinkTestLibraryObject(&sTest, "unused",
		"	.global Unused\n"
		"Unused:\n"
		"	.dfunction \"Unused\"\n"
		"	ret\n");
	AssertTrue(sTest.cLinker.Link());
	AssertString("", sTest.cLinker.GetErrors());
	iStart = LinkedSymbol(&sTest, "Start");
	iBody = LinkedSymbol(&sTest, "Body");
	iOther = LinkedSymbol(&sTest, "Other");
	iHandler = LinkedSymbol(&sTest, "Handler");
	iCount = LinkedSymbol(&sTest, "Count");
	pcRecords = sTest.cLinker.GetDebugInfo()->GetRecords();

	//Start, Other, Handler and one copy of Shared; Unused's object wasn't linked.
	AssertSize(4, pcRecords->NumFunctions());
	psFunction = pcRecords->FindFunction((uint32)iStart);
	AssertTrue(psFunction != NULL);
	AssertString("Start", psFunction->szName.Text());
	AssertInt(ASM65_DEBUG_ABSOLUTE, psFunction->sStart.uiSection);
	AssertInt((uint32)iStart, psFunction->sStart.uiOffset);
	AssertInt((uint32)iBody, psFunction->uiBody);
	AssertInt(ADE_Long, psFunction->uiEntry);
	AssertInt(ASM65_DEBUG_NONE, psFunction->uiResult);
	AssertInt(ADF_Register, psFunction->uiFrame);
	AssertInt(A65DR_V, psFunction->uiFrameRegister);
	AssertInt(14, psFunction->iFrameOffset);
	AssertInt(A65DR_V, psFunction->uiBase);
	AssertInt(2, psFunction->uiNumSaved);
	psSaved = pcRecords->GetSaved(psFunction, 0);
	AssertInt(A65DR_V, psSaved->uiWhat);
	AssertFalse(psSaved->bMemory);
	AssertInt(4, psSaved->uiBytes);
	AssertInt(-7, psSaved->iOffset);
	psSaved = pcRecords->GetSaved(psFunction, 1);
	AssertInt(0x80, psSaved->uiWhat);
	AssertTrue(psSaved->bMemory);
	AssertInt(4, psSaved->uiBytes);
	AssertInt(-11, psSaved->iOffset);
	AssertTrue(pcRecords->GetSaved(psFunction, 2) == NULL);
	AssertInt(ADP_65C64, pcRecords->GetProcessor());
	AssertTrue(pcRecords->FindFunction((uint32)iBody + 1) == psFunction);
	AssertTrue(pcRecords->FindFunction(psFunction->uiEnd - 1) == psFunction);
	AssertTrue(pcRecords->FindFunction(psFunction->uiEnd) != psFunction);
	psFunction = pcRecords->FindFunction((uint32)iOther);
	AssertString("CShape::Area", psFunction->szName.Text());
	AssertInt(ADE_Short, psFunction->uiEntry);
	AssertInt(ADF_FollowS, psFunction->uiFrame);
	AssertInt(ASM65_DEBUG_FRAME, psFunction->uiBase);
	AssertInt(0, psFunction->uiNumSaved);
	AssertInt((uint32)iOther, psFunction->uiBody);
	AssertInt((uint32)LinkedSymbol(&sTest, "Stray"), psFunction->uiEnd);
	AssertString("int", pcRecords->GetType(psFunction->uiResult)->szKey.Text());
	AssertTrue(pcRecords->FindFunction(0) == NULL);

	//Stray's code, after .dend, has none; Handler's starts again.
	AssertTrue(pcRecords->FindFunction(psFunction->uiEnd) == NULL);
	psFunction = pcRecords->FindFunction((uint32)iHandler);
	AssertString("[Interrupt]", psFunction->szName.Text());
	AssertInt(ADE_UserInterrupt, psFunction->uiEntry);
	AssertInt(ADF_Register, psFunction->uiFrame);
	AssertInt(A65DR_S, psFunction->uiFrameRegister);
	AssertInt(4, psFunction->iFrameOffset);
	AssertInt(1, psFunction->uiNumSaved);
	AssertInt(A65DR_S, pcRecords->GetSaved(psFunction, 0)->uiWhat);
	AssertInt(-1, pcRecords->GetSaved(psFunction, 0)->iOffset);

	//Shared's frame address is the address held at V - 4, plus 8.
	psFunction = pcRecords->FindFunction((uint32)LinkedSymbol(&sTest, "Shared"));
	AssertInt(ADF_Memory, psFunction->uiFrame);
	AssertInt(A65DR_V, psFunction->uiFrameRegister);
	AssertInt(-4, psFunction->iFrameOffset);
	AssertInt(8, psFunction->iFrameAdjust);

	//Start's variables, typed by the linked types.
	psFunction = pcRecords->FindFunction((uint32)iStart);
	AssertInt(2, psFunction->uiNumVariables);
	psVariable = pcRecords->GetVariable(psFunction, 0);
	AssertString("iCount", psVariable->szName.Text());
	AssertTrue(psVariable->bParameter);
	AssertInt(ADW_Frame, psVariable->uiWhere);
	AssertInt(9, psVariable->iOffset);
	AssertInt(10, psVariable->uiFirstLine);
	AssertInt(20, psVariable->uiLastLine);
	psInt = pcRecords->GetType(psVariable->uiType);
	AssertString("int", psInt->szName.Text());
	AssertInt(ADT_Integer, psInt->uiKind);
	AssertInt(ADTF_Signed, psInt->uiFlags);
	psVariable = pcRecords->GetVariable(psFunction, 1);
	AssertFalse(psVariable->bParameter);
	psType = pcRecords->GetType(psVariable->uiType);
	AssertString("CShape*", psType->szName.Text());
	AssertInt(ADT_Pointer, psType->uiKind);
	AssertTrue(pcRecords->GetVariable(psFunction, 2) == NULL);

	//CShape is b's complete description, its members typed by the linked types; the pointer to it is a's.
	psType = pcRecords->GetType(psType->uiTarget);
	AssertString("CShape", psType->szKey.Text());
	AssertInt(0, psType->uiFlags);
	AssertInt(8, psType->uiBytes);
	AssertInt(3, psType->uiNumMembers);
	AssertTrue(pcRecords->FindType("CShape") == psType);
	AssertTrue(pcRecords->GetType(pcRecords->FindTypeIndex("CShape")) == psType);
	AssertInt(ASM65_DEBUG_NONE, pcRecords->FindTypeIndex("CCircle"));
	AssertInt(ADM_VTable, pcRecords->GetMember(psType, 0)->uiKind);
	psMember = pcRecords->GetMember(psType, 2);
	AssertString("miSides", psMember->szName.Text());
	AssertTrue(pcRecords->GetType(psMember->uiType) == psInt);
	AssertLong(6, psMember->iValue);
	AssertInt(3, psMember->uiBitOffset);
	AssertInt(5, psMember->uiBits);
	psType = pcRecords->GetType(pcRecords->GetMember(psType, 1)->uiType);
	AssertString("EKind", psType->szName.Text());
	AssertTrue(pcRecords->GetType(psType->uiTarget) == psInt);
	AssertLong(-2, pcRecords->GetMember(psType, 1)->iValue);
	AssertSize(4, pcRecords->NumTypes());

	//The global and Start's static, at their labels.
	psGlobal = pcRecords->FindGlobal("giCount", NULL);
	AssertTrue(psGlobal != NULL);
	AssertInt((uint32)iCount, psGlobal->sPlace.uiOffset);
	AssertInt(ASM65_DEBUG_NONE, psGlobal->uiFunction);
	AssertTrue(pcRecords->FindGlobal("siCalls", NULL) == NULL);
	psGlobal = pcRecords->FindGlobal("siCalls", pcRecords->FindFunction((uint32)iStart));
	AssertTrue(psGlobal != NULL);
	AssertInt((uint32)iCount + 2, psGlobal->sPlace.uiOffset);

	//An object written and read back keeps its records.
	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	AssertTrue(sTest.acObjects[0].Write(pcMemoryFile));
	cObject.Init("read");
	AssertTrue(cObject.Read(MemoryFile(pcMemoryFile->GetBufferPointer(), pcMemoryFile->GetBufferSize())));
	AssertSize(2, cObject.GetDebug()->NumFunctions());
	AssertSize(2, cObject.GetDebug()->NumVariables());
	AssertSize(2, cObject.GetDebug()->NumGlobals());
	AssertSize(3, cObject.GetDebug()->NumTypes());
	AssertString("siCalls", cObject.GetDebug()->GetGlobal(0)->szName.Text());
	AssertInt(0, cObject.GetDebug()->GetGlobal(0)->uiFunction);
	cObject.Kill();
	SafeKill(pcMemoryFile);

	//Written and read back, then moved as a module's are.
	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	AssertTrue(sTest.cLinker.GetDebugInfo()->Write(pcMemoryFile));
	cRead.Init();
	AssertTrue(cRead.Read(MemoryFile(pcMemoryFile->GetBufferPointer(), pcMemoryFile->GetBufferSize())));
	pcRecords = cRead.GetRecords();
	AssertSize(4, pcRecords->NumFunctions());
	AssertSize(4, pcRecords->NumTypes());
	AssertSize(2, pcRecords->NumGlobals());
	psFunction = pcRecords->FindFunction((uint32)iOther);
	AssertString("CShape::Area", psFunction->szName.Text());
	AssertInt(ADE_Short, psFunction->uiEntry);
	AssertString("int", pcRecords->GetType(psFunction->uiResult)->szKey.Text());
	AssertInt(ADE_UserInterrupt, pcRecords->FindFunction((uint32)iHandler)->uiEntry);
	AssertInt(A65DR_S, pcRecords->GetSaved(pcRecords->FindFunction((uint32)iHandler), 0)->uiWhat);
	AssertInt(0x80, pcRecords->GetSaved(pcRecords->FindFunction((uint32)iStart), 1)->uiWhat);
	AssertInt(ADP_65C64, pcRecords->GetProcessor());
	AssertString("int", pcRecords->GetType(pcRecords->GetVariable(pcRecords->FindFunction((uint32)iStart), 0)->uiType)->szKey.Text());
	AssertTrue(pcRecords->FindType("EKind") != NULL);
	cRead.Move(-iStart);
	AssertTrue(pcRecords->FindFunction(0) != NULL);
	AssertString("Start", pcRecords->FindFunction(0)->szName.Text());
	AssertInt((uint32)(iBody - iStart), pcRecords->FindFunction(0)->uiBody);
	AssertInt((uint32)(iCount - iStart), pcRecords->FindGlobal("giCount", NULL)->sPlace.uiOffset);
	cRead.Kill();
	SafeKill(pcMemoryFile);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// A variable whose place moves (ranges): its .dranges, at the base, in a
// register, at a register and at an address, or a value and the address
// of a place in the frame, from label to label in its function's
// section; linked to addresses, written and read back, moved.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerDebugRanges(void)
{
	SLinkTest				sTest;
	CAsm65DebugRecords*		pcRecords;
	CAsm65Object			cObject;
	CMemoryFile*			pcMemoryFile;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugVariable*	psVariable;
	SAsm65DebugRange*		psRange;
	uint32					uiStart;
	uint32					uiMoved;
	uint32					uiInB;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	.global Pad\n"
		"Pad:\n"
		"	ret\n");
	LinkTestObject(&sTest,
		"	.global Start\n"
		"Start:\n"
		"	.dfunction \"Start\"\n"
		"	.dbase V\n"
		"	psh A\n"
		"__dbg1:\n"
		"	cpy B, A\n"
		"__dbg2:\n"
		"	pul A\n"
		"__dbg3:\n"
		"	ret\n"
		"Start.End:\n"
		"	.dvariable local, \"i\", 0, ranges, 0, 3, 4\n"
		"	.drange Start, __dbg1, frame, -2\n"
		"	.drange __dbg1, __dbg2, register, B\n"
		"	.drange __dbg2, __dbg3, memory, W, 0x80\n"
		"	.drange __dbg3, Start.End, address, 0x84\n"
		"	.dvariable local, \"j\", 0, frame, -4, 3, 4\n"
		"	.dvariable local, \"k\", 0, ranges, 0, 3, 4\n"
		"	.drange Start, __dbg1, value, -5000000000\n"
		"	.drange __dbg1, __dbg2, frameaddress, 6\n"
		"	.dtype 0, \"int\", \"int\", integer, 1, 2, -1, 0\n");
	AssertTrue(sTest.cLinker.Link());
	AssertString("", sTest.cLinker.GetErrors());
	uiStart = (uint32)LinkedSymbol(&sTest, "Start");
	pcRecords = sTest.cLinker.GetDebugInfo()->GetRecords();

	//The ranges at their linked addresses; j, of the frame, has none.
	psFunction = pcRecords->FindFunction(uiStart);
	AssertInt(3, psFunction->uiNumVariables);
	psVariable = pcRecords->GetVariable(psFunction, 0);
	AssertInt(ADW_Ranges, psVariable->uiWhere);
	AssertInt(4, psVariable->uiNumRanges);
	psRange = pcRecords->GetRange(psVariable, 0);
	AssertInt(uiStart, psRange->uiFrom);
	AssertInt(ADW_Frame, psRange->uiWhere);
	AssertInt(-2, psRange->iOffset);
	psRange = pcRecords->GetRange(psVariable, 1);
	uiInB = psRange->uiFrom;
	AssertTrue(uiInB > uiStart);
	AssertInt(ADW_Register, psRange->uiWhere);
	AssertInt(A65DR_B, psRange->uiRegister);
	psRange = pcRecords->GetRange(psVariable, 2);
	AssertInt(ADW_Memory, psRange->uiWhere);
	AssertInt(A65DR_W, psRange->uiRegister);
	AssertInt(0x80, psRange->iOffset);
	psRange = pcRecords->GetRange(psVariable, 3);
	AssertInt(ADW_Address, psRange->uiWhere);
	AssertInt(0x84, psRange->iOffset);
	AssertTrue(pcRecords->GetRange(psVariable, 4) == NULL);
	AssertTrue(pcRecords->FindRange(psVariable, uiStart) == pcRecords->GetRange(psVariable, 0));
	AssertTrue(pcRecords->FindRange(psVariable, uiInB) == pcRecords->GetRange(psVariable, 1));
	AssertTrue(pcRecords->FindRange(psVariable, psRange->uiTo - 1) == psRange);
	AssertTrue(pcRecords->FindRange(psVariable, psRange->uiTo) == NULL);
	AssertInt(0, pcRecords->GetVariable(psFunction, 1)->uiNumRanges);

	//k's values: a constant wider than 32 bits, then the address base + 6.
	psVariable = pcRecords->GetVariable(psFunction, 2);
	AssertInt(ADW_Value, pcRecords->GetRange(psVariable, 0)->uiWhere);
	AssertLong(-5000000000ll, pcRecords->GetRange(psVariable, 0)->iValue);
	AssertInt(ADW_FrameAddress, pcRecords->GetRange(psVariable, 1)->uiWhere);
	AssertInt(6, pcRecords->GetRange(psVariable, 1)->iOffset);

	//The object written and read back keeps them as offsets in Start's section.
	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	AssertTrue(sTest.acObjects[1].Write(pcMemoryFile));
	cObject.Init("read");
	AssertTrue(cObject.Read(MemoryFile(pcMemoryFile->GetBufferPointer(), pcMemoryFile->GetBufferSize())));
	psVariable = cObject.GetDebug()->GetVariable(cObject.GetDebug()->GetFunction(0), 0);
	AssertInt(4, psVariable->uiNumRanges);
	AssertInt(0, cObject.GetDebug()->GetRange(psVariable, 0)->uiFrom);
	AssertInt(uiInB - uiStart, cObject.GetDebug()->GetRange(psVariable, 1)->uiFrom);
	AssertLong(-5000000000ll, cObject.GetDebug()->GetRange(cObject.GetDebug()->GetVariable(cObject.GetDebug()->GetFunction(0), 2), 0)->iValue);
	cObject.Kill();
	SafeKill(pcMemoryFile);

	//Moved as a module's are.
	pcRecords->Move(0x100);
	uiMoved = pcRecords->GetRange(pcRecords->GetVariable(pcRecords->FindFunction(uiStart + 0x100), 0), 1)->uiFrom;
	AssertInt(uiInB + 0x100, uiMoved);
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// A function's inlined calls (.dinline, .dinspan): Area inlined into
// main, and Mul inlined into Area there; the innermost inline an address
// is in; a variable of an inline (.dvariable's inline), seen only by its
// code; linked to addresses, written and read back, moved.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerDebugInlines(void)
{
	SLinkTest				sTest;
	CAsm65DebugRecords*		pcRecords;
	CAsm65Object			cObject;
	CMemoryFile*			pcMemoryFile;
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugInline*		psInline;
	uint32					uiStart;
	uint32					uiArea;
	uint32					uiMul;
	uint32					uiAfter;

	LinkTestInit(&sTest);
	LinkTestObject(&sTest,
		"	.global Pad\n"
		"Pad:\n"
		"	ret\n");
	LinkTestObject(&sTest,
		"	.global Start\n"
		"Start:\n"
		"	.dfunction \"main\"\n"
		"	psh A\n"
		"	.global Area\n"
		"Area:\n"
		"	cpy B, A\n"
		"	.global Mul\n"
		"Mul:\n"
		"	mul\n"
		"MulEnd:\n"
		"	cpy A, B\n"
		"	.global After\n"
		"After:\n"
		"	pul A\n"
		"	ret\n"
		"	.dinline -1, \"Area\", \"Test.cpp\", 25\n"
		"	.dinspan Area, After\n"
		"	.dvariable local, \"i\", 0, frame, 2, 20, 30\n"
		"	.dvariable parameter, \"i\", 0, frame, 4, 5, 10, 0\n"
		"	.dinline 0, \"Mul\", \"Test.cpp\", 9\n"
		"	.dinspan Mul, MulEnd\n"
		"	.dstatic \"siCalls\", 0, Calls\n"
		"	.dstatic \"siCalls\", 0, AreaCalls, 0\n"
		"	.dtype 0, \"int\", \"int\", integer, 1, 4, -1, 0\n"
		"	.section data\n"
		"Calls:\n"
		"	.word 0\n"
		"AreaCalls:\n"
		"	.word 0\n");
	AssertTrue(sTest.cLinker.Link());
	AssertString("", sTest.cLinker.GetErrors());
	uiStart = (uint32)LinkedSymbol(&sTest, "Start");
	uiArea = (uint32)LinkedSymbol(&sTest, "Area");
	uiMul = (uint32)LinkedSymbol(&sTest, "Mul");
	uiAfter = (uint32)LinkedSymbol(&sTest, "After");
	pcRecords = sTest.cLinker.GetDebugInfo()->GetRecords();

	psFunction = pcRecords->FindFunction(uiStart);
	AssertInt(2, psFunction->uiNumInlines);
	psInline = pcRecords->GetInline(psFunction, 1);
	AssertString("Mul", psInline->szName.Text());
	AssertString("Test.cpp", psInline->szFile.Text());
	AssertInt(9, psInline->uiLine);
	AssertInt(0, psInline->iParent);
	AssertInt(uiMul, pcRecords->GetSpan(psInline, 0)->uiFrom);
	AssertTrue(pcRecords->GetSpan(psInline, 1) == NULL);
	AssertTrue(pcRecords->GetInline(psFunction, 2) == NULL);

	//Area's i is its inline's alone: main's code doesn't see it, nor Area's code main's.
	AssertInt(0, pcRecords->GetVariable(psFunction, 1)->iInline);
	AssertTrue(pcRecords->FindVariable(psFunction, "i", 25) == pcRecords->GetVariable(psFunction, 0));
	AssertTrue(pcRecords->FindVariable(psFunction, "i", 9, 0) == pcRecords->GetVariable(psFunction, 1));
	AssertTrue(pcRecords->FindVariable(psFunction, "i", 9) == NULL);
	AssertTrue(pcRecords->FindVariable(psFunction, "i", 25, 1) == NULL);

	//A static of each: main's code sees its own, Area's code Area's.
	AssertTrue(pcRecords->FindGlobal("siCalls", psFunction) != pcRecords->FindGlobal("siCalls", psFunction, 0));
	AssertInt(0, pcRecords->FindGlobal("siCalls", psFunction, 0)->iInline);
	AssertInt(-1, pcRecords->FindGlobal("siCalls", psFunction)->iInline);
	AssertTrue(pcRecords->FindGlobal("siCalls", psFunction, 1) == NULL);

	//The innermost an address is in.
	AssertInt(-1, pcRecords->FindInline(psFunction, uiStart));
	AssertInt(0, pcRecords->FindInline(psFunction, uiArea));
	AssertInt(1, pcRecords->FindInline(psFunction, uiMul));
	AssertInt(0, pcRecords->FindInline(psFunction, uiAfter - 1));
	AssertInt(-1, pcRecords->FindInline(psFunction, uiAfter));

	//The object written and read back keeps them as offsets in its section.
	pcMemoryFile = MemoryFile();
	pcMemoryFile->mbBasicFileMustFree = false;
	AssertTrue(sTest.acObjects[1].Write(pcMemoryFile));
	cObject.Init("read");
	AssertTrue(cObject.Read(MemoryFile(pcMemoryFile->GetBufferPointer(), pcMemoryFile->GetBufferSize())));
	psFunction = cObject.GetDebug()->GetFunction(0);
	AssertInt(2, psFunction->uiNumInlines);
	AssertString("Area", cObject.GetDebug()->GetInline(psFunction, 0)->szName.Text());
	AssertInt(0, cObject.GetDebug()->GetVariable(psFunction, 1)->iInline);
	AssertInt(-1, cObject.GetDebug()->GetVariable(psFunction, 0)->iInline);
	AssertInt(uiMul - uiStart, cObject.GetDebug()->GetSpan(cObject.GetDebug()->GetInline(psFunction, 1), 0)->uiFrom);
	cObject.Kill();
	SafeKill(pcMemoryFile);

	//Moved as a module's are.
	pcRecords->Move(0x100);
	AssertInt(1, pcRecords->FindInline(pcRecords->FindFunction(uiStart + 0x100), uiMul + 0x100));
	LinkTestKill(&sTest);
}


//////////////////////////////////////////////////////////////////////////
//
// The variables a function's code sees at a line: those in scope, an
// inner one hiding an outer one of its name, as a debugger's Locals and
// watches find them; this listed first.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerDebugVisible(void)
{
	CAsm65DebugRecords		cRecords;
	SAsm65DebugFunction*	psFunction;
	CArrayUint32			auiVisible;

	cRecords.Init();
	AssertTrue(cRecords.AddType("int", "int", ADT_Integer, ADTF_Signed, 2, ASM65_DEBUG_NONE, 0));
	psFunction = cRecords.AddFunction("CShape::Grow", 0, 0, ADE_Long, 0);
	AssertTrue(cRecords.AddVariable("iBy", 0, ADW_Frame, true, 1, 10, 30));
	AssertTrue(cRecords.AddVariable("this", 0, ADW_Frame, true, 3, 10, 30));
	AssertTrue(cRecords.AddVariable("i", 0, ADW_Frame, false, 5, 12, 30));
	AssertTrue(cRecords.AddVariable("i", 0, ADW_Frame, false, 7, 15, 20));
	AssertTrue(cRecords.AddVariable("iLast", 0, ADW_Frame, false, 9, 25, 30));
	AssertTrue(cRecords.IsValid());

	AssertTrue(cRecords.FindVariable(psFunction, "i", 11) == NULL);
	AssertTrue(cRecords.FindVariable(psFunction, "i", 12) == cRecords.GetVariable(psFunction, 2));
	AssertTrue(cRecords.FindVariable(psFunction, "i", 15) == cRecords.GetVariable(psFunction, 3));
	AssertTrue(cRecords.FindVariable(psFunction, "i", 21) == cRecords.GetVariable(psFunction, 2));
	AssertTrue(cRecords.FindVariable(psFunction, "i", 0) == cRecords.GetVariable(psFunction, 3));
	AssertFalse(cRecords.IsVisible(psFunction, cRecords.GetVariable(psFunction, 2), 18));
	AssertTrue(cRecords.IsVisible(psFunction, cRecords.GetVariable(psFunction, 2), 22));

	//this first, then as declared, the outer i hidden.
	auiVisible.Init();
	AssertSize(3, cRecords.FindVisible(psFunction, 18, &auiVisible));
	AssertInt(1, auiVisible.GetValue(0));
	AssertInt(0, auiVisible.GetValue(1));
	AssertInt(3, auiVisible.GetValue(2));
	auiVisible.ReInit();
	AssertSize(4, cRecords.FindVisible(psFunction, 26, &auiVisible));
	AssertInt(1, auiVisible.GetValue(0));
	AssertInt(0, auiVisible.GetValue(1));
	AssertInt(2, auiVisible.GetValue(2));
	AssertInt(4, auiVisible.GetValue(3));
	auiVisible.Kill();

	//A result of a type the records haven't makes them invalid.
	cRecords.AddFunction("Bad", 0, 4, ADE_Long, 7);
	AssertFalse(cRecords.IsValid());
	cRecords.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What the debug directives refuse.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65LinkerDebugErrors(void)
{
	CAsm65C64Assembler	cAssembler;
	CAsm65Object		cObject;

	cAssembler.Init();
	cObject.Init("errors");
	AssertFalse(cAssembler.AssembleObject(
		"	.dframe V, 2\n"
		"	.dvariable local, \"i\", -1, frame, 2, 1, 2\n"
		"Start:\n"
		"	.dfunction \"Start\"\n"
		"	.dframe up, 2\n"
		"	.dvariable local, \"i\", -1, frame, 2, 5, 4\n"
		"	.dtype 1, \"int\", \"int\", integer, 1, 2, -1, 0\n"
		"	.dtype 0, \"int\", \"int\", number, 1, 2, -1, 0\n"
		"	.dmember field, \"x\", -1, 0, 0, 0\n"
		"	.dvariable local, \"j\", 7, frame, 2, 1, 2\n"
		"	.dglobal \"g\", -1, 12\n", &cObject));
	AssertString(
		"1:2: .dframe must follow a .dfunction '.dframe'\n"
		"2:2: .dvariable must follow a .dfunction '.dvariable'\n"
		"5:10: Expected one of the processor's registers 'up'\n"
		"6:42: Expected a line no earlier than the first '4'\n"
		"7:9: A .dtype's index must be the number of types before it '1'\n"
		"8:26: Expected a kind of type 'number'\n"
		"9:2: .dmember must follow a .dtype '.dmember'\n"
		"11:20: A debug global must be at a label in one of the object's sections '12'\n"
		"12:1: A debug record names a type the unit doesn't describe 'end of file'\n", cAssembler.GetErrors());
	cObject.Kill();
	cAssembler.Kill();

	//.dend ends a function, once; an entry is one of four; a frame, a saved register and a base name registers.
	cAssembler.Init();
	cObject.Init("errors");
	AssertFalse(cAssembler.AssembleObject(
		"	.dend\n"
		"Start:\n"
		"	.dfunction \"Start\", far\n"
		"	.dfunction \"Next\"\n"
		"	.dend\n"
		"	.dend\n"
		"	.dframe [V * 2], 1\n"
		"	.dsaved Q, 2\n"
		"	.dsaved [0x80], 9, 2\n"
		"	.dbase R\n", &cObject));
	AssertString(
		"1:2: .dend must follow a .dfunction not yet ended '.dend'\n"
		"3:22: Expected long, short, interrupt or userinterrupt 'far'\n"
		"6:2: .dend must follow a .dfunction not yet ended '.dend'\n"
		"7:13: Expected '+' or '-' '*'\n"
		"8:10: Expected one of the processor's registers 'Q'\n"
		"9:18: Expected the bytes saved '9'\n"
		"10:9: Expected one of the processor's registers 'R'\n", cAssembler.GetErrors());
	cObject.Kill();
	cAssembler.Kill();

	//A .drange follows a variable of ranges, its labels in order in its function's section; its place is one of three.
	cAssembler.Init();
	cObject.Init("errors");
	AssertFalse(cAssembler.AssembleObject(
		"Start:\n"
		"	.dfunction \"Start\"\n"
		"	nop\n"
		"Next:\n"
		"	ret\n"
		"	.dvariable local, \"i\", 0, frame, 2, 1, 2\n"
		"	.drange Start, Next, frame, 2\n"
		"	.dvariable local, \"j\", 0, ranges, 0, 1, 2\n"
		"	.drange Next, Start, register, A\n"
		"	.drange Start, Other, register, A\n"
		"	.drange Start, Next, stack, 2\n"
		"	.drange Start, Next, register, Q\n"
		"	.drange Start, 12, frame, 2\n"
		"	.section data\n"
		"Other:\n"
		"	.word 0\n"
		"	.drange Other, Other, frame, 2\n"
		"	.dtype 0, \"int\", \"int\", integer, 1, 2, -1, 0\n", &cObject));
	AssertString(
		"7:2: .drange must follow a .dvariable of ranges, its labels in order in its function's section '.drange'\n"
		"9:2: .drange must follow a .dvariable of ranges, its labels in order in its function's section '.drange'\n"
		"10:2: .drange must follow a .dvariable of ranges, its labels in order in its function's section '.drange'\n"
		"11:23: Expected frame, register, memory, address, value or frameaddress 'stack'\n"
		"12:33: Expected one of the processor's registers 'Q'\n"
		"13:17: A debug range must be at a label in one of the object's sections '12'\n"
		"17:2: .drange must follow a .dvariable of ranges, its labels in order in its function's section '.drange'\n", cAssembler.GetErrors());
	cObject.Kill();
	cAssembler.Kill();

	//A .dinline follows a function, its parent an inline before it; a .dinspan follows a .dinline.
	cAssembler.Init();
	cObject.Init("errors");
	AssertFalse(cAssembler.AssembleObject(
		"	.dinline -1, \"F\", \"Test.cpp\", 2\n"
		"Start:\n"
		"	.dfunction \"Start\"\n"
		"	nop\n"
		"Next:\n"
		"	ret\n"
		"	.dinspan Start, Next\n"
		"	.dinline 0, \"F\", \"Test.cpp\", 2\n"
		"	.dinline -1, \"F\", \"Test.cpp\", 2\n"
		"	.dinspan Next, Start\n", &cObject));
	AssertString(
		"1:2: .dinline must follow a .dfunction, its parent an inline before it '.dinline'\n"
		"7:2: .dinspan must follow a .dinline, its labels in order in its function's section '.dinspan'\n"
		"8:2: .dinline must follow a .dfunction, its parent an inline before it '.dinline'\n"
		"10:2: .dinspan must follow a .dinline, its labels in order in its function's section '.dinspan'\n", cAssembler.GetErrors());
	cObject.Kill();
	cAssembler.Kill();

	//Only in an object.
	cAssembler.Init();
	AssertFalse(cAssembler.Assemble("	.dfunction \"Start\"\n"));
	AssertString("1:2: Debug directives are only allowed when assembling an object '.dfunction'\n", cAssembler.GetErrors());
	cAssembler.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65Linker(void)
{
	BeginTests();

	TestAsm65LinkerCall();
	TestAsm65LinkerOffsetAddresses();
	TestAsm65LinkerLabelEndsSection();
	TestAsm65LinkerWeak();
	TestAsm65LinkerComdat();
	TestAsm65LinkerInitFini();
	TestAsm65LinkerLibrary();
	TestAsm65LinkerLibraryFile();
	TestAsm65LinkerErrors();
	TestAsm65LinkerRemoveUnused();
	TestAsm65LinkerRemoveUnusedLibrary();
	TestAsm65LinkerPrograms();
	TestAsm65LinkerLineTable();
	TestAsm65LinkerDebugRecords();
	TestAsm65LinkerDebugRanges();
	TestAsm65LinkerDebugInlines();
	TestAsm65LinkerDebugVisible();
	TestAsm65LinkerDebugErrors();
	TestAsm65LinkerRelocatable();

	TestStatistics();
}
