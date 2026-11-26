#include "BaseLib/ArrayChars.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/HashSetString.h"
#include "Assembler65Lib/Asm65C64Assembler.h"
#include "MicroprocessorLib/65C64InstructionFactory.h"
#include "MicroprocessorLib/65C64Instruction.h"
#include "TestLib/Assert.h"


//The spreadsheet's instructions as a plain text table, made from its Instructions sheet by the website's build
//(HCP65/Website/tools/instruction_tables.py, DocsPlan.txt C3).
#define TEST_ASM65_SPREADSHEET_INSTRUCTIONS	".." _FS_ ".." _FS_ ".." _FS_ "X65C64-Proposal" _FS_ "Generated" _FS_ "Instructions.txt"

//The hardware register instructions are in the spreadsheet but aren't simulated yet (the website's instruction index
//says so).
static const char*	gaszAsm65SpreadsheetUnsimulated[] = { "hws", "hwg", "hwsp", "hwcp" };


//////////////////////////////////////////////////////////////////////////
//
// The spreadsheet's rows: "<mnemonic> <instruction code>" each, and the
// mnemonics alone.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsm65SpreadsheetRead(CHashSetString<>* pcCoded, CHashSetString<>* pcMnemonics)
{
	CTextFile	cFile;
	CArrayChars	aszLines;
	CArrayChars	aszFields;
	CChars		szCoded;
	size		i;

	cFile.Init();
	AssertTrue(cFile.Read(TEST_ASM65_SPREADSHEET_INSTRUCTIONS));
	aszLines.Init();
	aszLines.Split(cFile.Text(), '\n');
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		if ((aszLines.GetText(i)[0] == '#') || (aszLines.GetText(i)[0] == '\0'))
		{
			continue;
		}
		aszFields.Init();
		aszFields.Split(aszLines.GetText(i), '\t');
		AssertSize(6, aszFields.NumElements());
		if (aszFields.NumElements() == 6)
		{
			szCoded.Init(aszFields.GetText(1));
			szCoded.Append(' ');
			szCoded.Append(aszFields.GetText(2));
			pcCoded->FindOrAddIndex(szCoded.Text());
			pcMnemonics->FindOrAddIndex(aszFields.GetText(1));
			szCoded.Kill();
		}
		aszFields.Kill();
	}
	aszLines.Kill();
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the simulator is excused szMnemonic.
//
//////////////////////////////////////////////////////////////////////////
static bool TestAsm65SpreadsheetIsUnsimulated(const char* szMnemonic)
{
	size	i;

	for (i = 0; i < sizeof(gaszAsm65SpreadsheetUnsimulated) / sizeof(char*); i++)
	{
		if (StringCompare(szMnemonic, gaszAsm65SpreadsheetUnsimulated[i]) == 0)
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The assembler knows the spreadsheet's mnemonics, and no others.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64SpreadsheetAssembler(void)
{
	CHashSetString<>	cCoded;
	CHashSetString<>	cMnemonics;
	CHashSetString<>	cAssembled;
	CChars				szMessage;
	const char*			szMnemonic;
	size				i;

	cCoded.Init();
	cMnemonics.Init();
	TestAsm65SpreadsheetRead(&cCoded, &cMnemonics);
	AssertTrue(cMnemonics.NumElements() >= 80);

	cAssembled.Init();
	szMessage.Init();
	for (i = 0; i < NumAsm65C64Mnemonics(); i++)
	{
		szMnemonic = GetAsm65C64Mnemonic(i);
		cAssembled.FindOrAddIndex(szMnemonic);
		if (!cMnemonics.HasKey(szMnemonic))
		{
			szMessage.Append("The assembler has ");
			szMessage.Append(szMnemonic);
			szMessage.Append(", which the spreadsheet hasn't.\n");
		}
	}
	for (i = 0; i < cMnemonics.NumElements(); i++)
	{
		szMnemonic = cMnemonics.GetKey(i);
		if (!cAssembled.HasKey(szMnemonic))
		{
			szMessage.Append("The spreadsheet has ");
			szMessage.Append(szMnemonic);
			szMessage.Append(", which the assembler hasn't.\n");
		}
	}
	AssertString("", szMessage.Text());

	szMessage.Kill();
	cAssembled.Kill();
	cMnemonics.Kill();
	cCoded.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The simulator decodes each of the spreadsheet's mnemonics from an
// opcode whose low four bits are its instruction code, and decodes
// nothing the spreadsheet hasn't: every opcode is tried.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64SpreadsheetSimulator(void)
{
	CHashSetString<>		cCoded;
	CHashSetString<>		cMnemonics;
	CHashSetString<>		cDecoded;
	C65C64InstructionFactory*	pcFactory;
	C65C64Instruction*		pcInstruction;
	CChars					szCoded;
	CChars					szMessage;
	const char*				szEntry;
	uint32					uiOpcode;
	size					i;

	cCoded.Init();
	cMnemonics.Init();
	TestAsm65SpreadsheetRead(&cCoded, &cMnemonics);

	cDecoded.Init();
	szMessage.Init();
	pcFactory = C65C64InstructionFactory::GetInstance();
	for (uiOpcode = 0; uiOpcode <= 0xFFFF; uiOpcode++)
	{
		pcInstruction = pcFactory->GetInstruction((uint16)uiOpcode);
		if ((pcInstruction == NULL) || (StringCompare(pcInstruction->GetName(), "ill") == 0))
		{
			continue;
		}
		szCoded.Init(pcInstruction->GetName());
		szCoded.Append(' ');
		szCoded.Append((int32)(uiOpcode & 0xF));
		if (!cDecoded.HasKey(szCoded.Text()))
		{
			cDecoded.FindOrAddIndex(szCoded.Text());
			if (!cCoded.HasKey(szCoded.Text()))
			{
				szMessage.Append("The simulator decodes ");
				szMessage.Append(szCoded);
				szMessage.Append(" (mnemonic and instruction code), which the spreadsheet hasn't.\n");
			}
		}
		szCoded.Kill();
	}
	for (i = 0; i < cCoded.NumElements(); i++)
	{
		szEntry = cCoded.GetKey(i);
		szCoded.Init(szEntry);
		szCoded.SetLength(szCoded.Find(' '));
		if (!cDecoded.HasKey(szEntry) && !TestAsm65SpreadsheetIsUnsimulated(szCoded.Text()))
		{
			szMessage.Append("The spreadsheet has ");
			szMessage.Append(szEntry);
			szMessage.Append(" (mnemonic and instruction code), which the simulator doesn't decode.\n");
		}
		szCoded.Kill();
	}
	AssertString("", szMessage.Text());

	szMessage.Kill();
	cDecoded.Kill();
	cMnemonics.Kill();
	cCoded.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The assembler and the simulator against the spreadsheet (DocsPlan.txt
// C4): the code follows the spreadsheet too.
//
//////////////////////////////////////////////////////////////////////////
void TestAsm65C64Spreadsheet(void)
{
	BeginTests();

	TestAsm65C64SpreadsheetAssembler();
	TestAsm65C64SpreadsheetSimulator();

	TestStatistics();
}

