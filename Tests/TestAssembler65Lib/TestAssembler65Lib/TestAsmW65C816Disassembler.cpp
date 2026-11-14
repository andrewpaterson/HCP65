#include "BaseLib/Chars.h"
#include "Assembler65Lib/AsmW65C816Assembler.h"
#include "Assembler65Lib/AsmW65C816Disassembler.h"
#include "Assembler65Lib/AsmW65C816Opcodes.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// The instruction at $1000 disassembled with those widths.
//
//////////////////////////////////////////////////////////////////////////
static void AssertAsmW65C816Disassembly(const char* szExpected, size uiExpectedLength, uint8* puiBytes, size uiAvailable, bool bMemory8, bool bIndex8)
{
	SAsmW65C816Widths	sWidths;
	CChars			sz;

	sWidths.bMemory8 = bMemory8;
	sWidths.bIndex8 = bIndex8;
	sz.Init();
	AssertSize(uiExpectedLength, AsmW65C816Disassemble(0x1000, puiBytes, uiAvailable, &sWidths, &sz));
	AssertString(szExpected, sz.Text());
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Every opcode but the branches, with 16 bit widths, assembles back to the
// bytes it was disassembled from.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsmW65C816DisassembleRoundTrip(void)
{
	CAsmW65C816Assembler	cAssembler;
	SAsmW65C816Widths		sWidths;
	CChars				szSource;
	CChars				szDifferences;
	uint8				auiBytes[4];
	uint8				uiByte;
	uint8				uiMode;
	size				uiLength;
	size				i;
	int					iOpcode;

	szDifferences.Init();
	for (iOpcode = 0; iOpcode < 256; iOpcode++)
	{
		AsmW65C816Mnemonic((uint8)iOpcode, &uiMode);
		if ((uiMode == AW65C816M_Relative) || (uiMode == AW65C816M_RelativeLong))
		{
			continue;
		}

		auiBytes[0] = (uint8)iOpcode;
		auiBytes[1] = 0x34;
		auiBytes[2] = 0x12;
		auiBytes[3] = 0x56;
		sWidths.bMemory8 = false;
		sWidths.bIndex8 = false;
		szSource.Init(".org $1000\n.width (A, X), 16\n");
		uiLength = AsmW65C816Disassemble(0x1000, auiBytes, 4, &sWidths, &szSource);
		szSource.AppendNewLine();

		cAssembler.Init();
		if (!cAssembler.Assemble(szSource.Text()) || (cAssembler.GetOutput()->NumBytes() != uiLength))
		{
			szDifferences.Append(szSource.Text());
		}
		else
		{
			for (i = 0; i < uiLength; i++)
			{
				cAssembler.GetOutput()->Get((uint32)(0x1000 + i), &uiByte);
				if (uiByte != auiBytes[i])
				{
					szDifferences.Append(szSource.Text());
					break;
				}
			}
		}
		cAssembler.Kill();
		szSource.Kill();
	}
	AssertString("", szDifferences.Text());
	szDifferences.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void TestAsmW65C816DisassembleModes(void)
{
	SAsmW65C816Widths		sWidths;
	CChars				sz;
	static uint8		auiLda[] = { 0xA9, 0x34, 0x12 };
	static uint8		auiLdx[] = { 0xA2, 0x34, 0x12 };
	static uint8		auiBra[] = { 0x80, 0xFE };
	static uint8		auiBne[] = { 0xD0, 0x04 };
	static uint8		auiBrl[] = { 0x82, 0x00, 0x01 };
	static uint8		auiPer[] = { 0x62, 0xFD, 0xFF };
	static uint8		auiMvn[] = { 0x54, 0x34, 0x12 };
	static uint8		auiJsl[] = { 0x22, 0x56, 0x34, 0x12 };
	static uint8		auiCode[] = { 0xE2, 0x20, 0xA9, 0x01, 0xC2, 0x30, 0xA9, 0x01, 0x00, 0xA2, 0x02, 0x00 };

	//Immediates by the widths.
	AssertAsmW65C816Disassembly("lda $1234", 3, auiLda, 3, false, true);
	AssertAsmW65C816Disassembly("lda $34", 2, auiLda, 3, true, false);
	AssertAsmW65C816Disassembly("ldx $1234", 3, auiLdx, 3, true, false);
	AssertAsmW65C816Disassembly("ldx $34", 2, auiLdx, 3, false, true);

	//Branches go to addresses.
	AssertAsmW65C816Disassembly("bra $1000", 2, auiBra, 2, true, true);
	AssertAsmW65C816Disassembly("bne $1006", 2, auiBne, 2, true, true);
	AssertAsmW65C816Disassembly("brl $1103", 3, auiBrl, 3, true, true);
	AssertAsmW65C816Disassembly("per $1000", 3, auiPer, 3, true, true);

	AssertAsmW65C816Disassembly("mvn $12, $34", 3, auiMvn, 3, true, true);
	AssertAsmW65C816Disassembly("jsl $123456", 4, auiJsl, 4, true, true);

	//Too few bytes.
	AssertAsmW65C816Disassembly("", 0, auiJsl, 3, true, true);
	AssertAsmW65C816Disassembly("", 0, auiJsl, 0, true, true);

	//sep and rep change the widths of what follows.
	sWidths.bMemory8 = false;
	sWidths.bIndex8 = false;
	sz.Init();
	AssertSize(2, AsmW65C816Disassemble(0x1000, &auiCode[0], 12, &sWidths, &sz));
	AssertTrue(sWidths.bMemory8);
	AssertFalse(sWidths.bIndex8);
	AssertSize(2, AsmW65C816Disassemble(0x1002, &auiCode[2], 10, &sWidths, &sz));
	AssertSize(2, AsmW65C816Disassemble(0x1004, &auiCode[4], 8, &sWidths, &sz));
	AssertFalse(sWidths.bMemory8);
	AssertSize(3, AsmW65C816Disassemble(0x1006, &auiCode[6], 6, &sWidths, &sz));
	AssertSize(3, AsmW65C816Disassemble(0x1009, &auiCode[9], 3, &sWidths, &sz));
	AssertString("sep $20lda $01rep $30lda $0001ldx $0002", sz.Text());
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What the instructions do to the flow, and where they go.
//
//////////////////////////////////////////////////////////////////////////
static void TestAsmW65C816DisassembleFlow(void)
{
	uint32			uiTarget;
	static uint8	auiBne[] = { 0xD0, 0xFE };
	static uint8	auiBrl[] = { 0x82, 0x00, 0x01 };
	static uint8	auiJsr[] = { 0x20, 0x34, 0x12 };
	static uint8	auiJsl[] = { 0x22, 0x56, 0x34, 0x12 };
	static uint8	auiJmpX[] = { 0x7C, 0x34, 0x12 };

	AssertInt(AW65C816F_Call, AsmW65C816Flow(0x20));
	AssertInt(AW65C816F_Call, AsmW65C816Flow(0x00));
	AssertInt(AW65C816F_Return, AsmW65C816Flow(0x6B));
	AssertInt(AW65C816F_Jump, AsmW65C816Flow(0x80));
	AssertInt(AW65C816F_Branch, AsmW65C816Flow(0xD0));
	AssertInt(AW65C816F_Other, AsmW65C816Flow(0x62));
	AssertInt(AW65C816F_Other, AsmW65C816Flow(0xEA));

	AssertTrue(AsmW65C816FlowTarget(0x021000, auiBne, &uiTarget));
	AssertIntHex(0x021000, uiTarget);
	AssertTrue(AsmW65C816FlowTarget(0x021000, auiBrl, &uiTarget));
	AssertIntHex(0x021103, uiTarget);
	AssertTrue(AsmW65C816FlowTarget(0x021000, auiJsr, &uiTarget));
	AssertIntHex(0x021234, uiTarget);
	AssertTrue(AsmW65C816FlowTarget(0x021000, auiJsl, &uiTarget));
	AssertIntHex(0x123456, uiTarget);
	AssertFalse(AsmW65C816FlowTarget(0x021000, auiJmpX, &uiTarget));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestAsmW65C816Disassembler(void)
{
	BeginTests();

	TestAsmW65C816DisassembleModes();
	TestAsmW65C816DisassembleFlow();
	TestAsmW65C816DisassembleRoundTrip();

	TestStatistics();
}
