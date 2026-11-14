/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela Assembler65Lib

Codaphela Assembler65Lib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela Assembler65Lib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela Assembler65Lib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "AsmW65C816Assembler.h"


#define ASM_W65C816_MAX_MNEMONICS	96


struct SAsmW65C816Opcode
{
	const char*	szMnemonic;
	uint8		uiMode;
};


//Each opcode's mnemonic and addressing mode (W65C816S datasheet table 5-4).  The
//test TestAsmW65C816Opcodes checks it against MicroprocessorLib's instruction factory.
static const SAsmW65C816Opcode	gasAsmW65C816Opcodes[256] = {
	{ "brk", AW65C816M_Immediate8 },  { "ora", AW65C816M_DirectIndirectX },  { "cop", AW65C816M_Immediate8 },  { "ora", AW65C816M_Stack },
	{ "tsb", AW65C816M_Direct },  { "ora", AW65C816M_Direct },  { "asl", AW65C816M_Direct },  { "ora", AW65C816M_DirectIndirectLong },
	{ "php", AW65C816M_Implied },  { "ora", AW65C816M_ImmediateM },  { "asl", AW65C816M_Accumulator },  { "phd", AW65C816M_Implied },
	{ "tsb", AW65C816M_Absolute },  { "ora", AW65C816M_Absolute },  { "asl", AW65C816M_Absolute },  { "ora", AW65C816M_Long },
	{ "bpl", AW65C816M_Relative },  { "ora", AW65C816M_DirectIndirectY },  { "ora", AW65C816M_DirectIndirect },  { "ora", AW65C816M_StackIndirectY },
	{ "trb", AW65C816M_Direct },  { "ora", AW65C816M_DirectX },  { "asl", AW65C816M_DirectX },  { "ora", AW65C816M_DirectIndirectLongY },
	{ "clc", AW65C816M_Implied },  { "ora", AW65C816M_AbsoluteY },  { "inc", AW65C816M_Accumulator },  { "tcs", AW65C816M_Implied },
	{ "trb", AW65C816M_Absolute },  { "ora", AW65C816M_AbsoluteX },  { "asl", AW65C816M_AbsoluteX },  { "ora", AW65C816M_LongX },
	{ "jsr", AW65C816M_JumpAbsolute },  { "and", AW65C816M_DirectIndirectX },  { "jsl", AW65C816M_JumpLong },  { "and", AW65C816M_Stack },
	{ "bit", AW65C816M_Direct },  { "and", AW65C816M_Direct },  { "rol", AW65C816M_Direct },  { "and", AW65C816M_DirectIndirectLong },
	{ "plp", AW65C816M_Implied },  { "and", AW65C816M_ImmediateM },  { "rol", AW65C816M_Accumulator },  { "pld", AW65C816M_Implied },
	{ "bit", AW65C816M_Absolute },  { "and", AW65C816M_Absolute },  { "rol", AW65C816M_Absolute },  { "and", AW65C816M_Long },
	{ "bmi", AW65C816M_Relative },  { "and", AW65C816M_DirectIndirectY },  { "and", AW65C816M_DirectIndirect },  { "and", AW65C816M_StackIndirectY },
	{ "bit", AW65C816M_DirectX },  { "and", AW65C816M_DirectX },  { "rol", AW65C816M_DirectX },  { "and", AW65C816M_DirectIndirectLongY },
	{ "sec", AW65C816M_Implied },  { "and", AW65C816M_AbsoluteY },  { "dec", AW65C816M_Accumulator },  { "tsc", AW65C816M_Implied },
	{ "bit", AW65C816M_AbsoluteX },  { "and", AW65C816M_AbsoluteX },  { "rol", AW65C816M_AbsoluteX },  { "and", AW65C816M_LongX },
	{ "rti", AW65C816M_Implied },  { "eor", AW65C816M_DirectIndirectX },  { "wdm", AW65C816M_Immediate8 },  { "eor", AW65C816M_Stack },
	{ "mvp", AW65C816M_BlockMove },  { "eor", AW65C816M_Direct },  { "lsr", AW65C816M_Direct },  { "eor", AW65C816M_DirectIndirectLong },
	{ "pha", AW65C816M_Implied },  { "eor", AW65C816M_ImmediateM },  { "lsr", AW65C816M_Accumulator },  { "phk", AW65C816M_Implied },
	{ "jmp", AW65C816M_JumpAbsolute },  { "eor", AW65C816M_Absolute },  { "lsr", AW65C816M_Absolute },  { "eor", AW65C816M_Long },
	{ "bvc", AW65C816M_Relative },  { "eor", AW65C816M_DirectIndirectY },  { "eor", AW65C816M_DirectIndirect },  { "eor", AW65C816M_StackIndirectY },
	{ "mvn", AW65C816M_BlockMove },  { "eor", AW65C816M_DirectX },  { "lsr", AW65C816M_DirectX },  { "eor", AW65C816M_DirectIndirectLongY },
	{ "cli", AW65C816M_Implied },  { "eor", AW65C816M_AbsoluteY },  { "phy", AW65C816M_Implied },  { "tcd", AW65C816M_Implied },
	{ "jml", AW65C816M_JumpLong },  { "eor", AW65C816M_AbsoluteX },  { "lsr", AW65C816M_AbsoluteX },  { "eor", AW65C816M_LongX },
	{ "rts", AW65C816M_Implied },  { "adc", AW65C816M_DirectIndirectX },  { "per", AW65C816M_RelativeLong },  { "adc", AW65C816M_Stack },
	{ "stz", AW65C816M_Direct },  { "adc", AW65C816M_Direct },  { "ror", AW65C816M_Direct },  { "adc", AW65C816M_DirectIndirectLong },
	{ "pla", AW65C816M_Implied },  { "adc", AW65C816M_ImmediateM },  { "ror", AW65C816M_Accumulator },  { "rtl", AW65C816M_Implied },
	{ "jmp", AW65C816M_JumpIndirect },  { "adc", AW65C816M_Absolute },  { "ror", AW65C816M_Absolute },  { "adc", AW65C816M_Long },
	{ "bvs", AW65C816M_Relative },  { "adc", AW65C816M_DirectIndirectY },  { "adc", AW65C816M_DirectIndirect },  { "adc", AW65C816M_StackIndirectY },
	{ "stz", AW65C816M_DirectX },  { "adc", AW65C816M_DirectX },  { "ror", AW65C816M_DirectX },  { "adc", AW65C816M_DirectIndirectLongY },
	{ "sei", AW65C816M_Implied },  { "adc", AW65C816M_AbsoluteY },  { "ply", AW65C816M_Implied },  { "tdc", AW65C816M_Implied },
	{ "jmp", AW65C816M_JumpIndirectX },  { "adc", AW65C816M_AbsoluteX },  { "ror", AW65C816M_AbsoluteX },  { "adc", AW65C816M_LongX },
	{ "bra", AW65C816M_Relative },  { "sta", AW65C816M_DirectIndirectX },  { "brl", AW65C816M_RelativeLong },  { "sta", AW65C816M_Stack },
	{ "sty", AW65C816M_Direct },  { "sta", AW65C816M_Direct },  { "stx", AW65C816M_Direct },  { "sta", AW65C816M_DirectIndirectLong },
	{ "dey", AW65C816M_Implied },  { "bit", AW65C816M_ImmediateM },  { "txa", AW65C816M_Implied },  { "phb", AW65C816M_Implied },
	{ "sty", AW65C816M_Absolute },  { "sta", AW65C816M_Absolute },  { "stx", AW65C816M_Absolute },  { "sta", AW65C816M_Long },
	{ "bcc", AW65C816M_Relative },  { "sta", AW65C816M_DirectIndirectY },  { "sta", AW65C816M_DirectIndirect },  { "sta", AW65C816M_StackIndirectY },
	{ "sty", AW65C816M_DirectX },  { "sta", AW65C816M_DirectX },  { "stx", AW65C816M_DirectY },  { "sta", AW65C816M_DirectIndirectLongY },
	{ "tya", AW65C816M_Implied },  { "sta", AW65C816M_AbsoluteY },  { "txs", AW65C816M_Implied },  { "txy", AW65C816M_Implied },
	{ "stz", AW65C816M_Absolute },  { "sta", AW65C816M_AbsoluteX },  { "stz", AW65C816M_AbsoluteX },  { "sta", AW65C816M_LongX },
	{ "ldy", AW65C816M_ImmediateX },  { "lda", AW65C816M_DirectIndirectX },  { "ldx", AW65C816M_ImmediateX },  { "lda", AW65C816M_Stack },
	{ "ldy", AW65C816M_Direct },  { "lda", AW65C816M_Direct },  { "ldx", AW65C816M_Direct },  { "lda", AW65C816M_DirectIndirectLong },
	{ "tay", AW65C816M_Implied },  { "lda", AW65C816M_ImmediateM },  { "tax", AW65C816M_Implied },  { "plb", AW65C816M_Implied },
	{ "ldy", AW65C816M_Absolute },  { "lda", AW65C816M_Absolute },  { "ldx", AW65C816M_Absolute },  { "lda", AW65C816M_Long },
	{ "bcs", AW65C816M_Relative },  { "lda", AW65C816M_DirectIndirectY },  { "lda", AW65C816M_DirectIndirect },  { "lda", AW65C816M_StackIndirectY },
	{ "ldy", AW65C816M_DirectX },  { "lda", AW65C816M_DirectX },  { "ldx", AW65C816M_DirectY },  { "lda", AW65C816M_DirectIndirectLongY },
	{ "clv", AW65C816M_Implied },  { "lda", AW65C816M_AbsoluteY },  { "tsx", AW65C816M_Implied },  { "tyx", AW65C816M_Implied },
	{ "ldy", AW65C816M_AbsoluteX },  { "lda", AW65C816M_AbsoluteX },  { "ldx", AW65C816M_AbsoluteY },  { "lda", AW65C816M_LongX },
	{ "cpy", AW65C816M_ImmediateX },  { "cmp", AW65C816M_DirectIndirectX },  { "rep", AW65C816M_Immediate8 },  { "cmp", AW65C816M_Stack },
	{ "cpy", AW65C816M_Direct },  { "cmp", AW65C816M_Direct },  { "dec", AW65C816M_Direct },  { "cmp", AW65C816M_DirectIndirectLong },
	{ "iny", AW65C816M_Implied },  { "cmp", AW65C816M_ImmediateM },  { "dex", AW65C816M_Implied },  { "wai", AW65C816M_Implied },
	{ "cpy", AW65C816M_Absolute },  { "cmp", AW65C816M_Absolute },  { "dec", AW65C816M_Absolute },  { "cmp", AW65C816M_Long },
	{ "bne", AW65C816M_Relative },  { "cmp", AW65C816M_DirectIndirectY },  { "cmp", AW65C816M_DirectIndirect },  { "cmp", AW65C816M_StackIndirectY },
	{ "pei", AW65C816M_Direct },  { "cmp", AW65C816M_DirectX },  { "dec", AW65C816M_DirectX },  { "cmp", AW65C816M_DirectIndirectLongY },
	{ "cld", AW65C816M_Implied },  { "cmp", AW65C816M_AbsoluteY },  { "phx", AW65C816M_Implied },  { "stp", AW65C816M_Implied },
	{ "jml", AW65C816M_JumpIndirectLong },  { "cmp", AW65C816M_AbsoluteX },  { "dec", AW65C816M_AbsoluteX },  { "cmp", AW65C816M_LongX },
	{ "cpx", AW65C816M_ImmediateX },  { "sbc", AW65C816M_DirectIndirectX },  { "sep", AW65C816M_Immediate8 },  { "sbc", AW65C816M_Stack },
	{ "cpx", AW65C816M_Direct },  { "sbc", AW65C816M_Direct },  { "inc", AW65C816M_Direct },  { "sbc", AW65C816M_DirectIndirectLong },
	{ "inx", AW65C816M_Implied },  { "sbc", AW65C816M_ImmediateM },  { "nop", AW65C816M_Implied },  { "xba", AW65C816M_Implied },
	{ "cpx", AW65C816M_Absolute },  { "sbc", AW65C816M_Absolute },  { "inc", AW65C816M_Absolute },  { "sbc", AW65C816M_Long },
	{ "beq", AW65C816M_Relative },  { "sbc", AW65C816M_DirectIndirectY },  { "sbc", AW65C816M_DirectIndirect },  { "sbc", AW65C816M_StackIndirectY },
	{ "pea", AW65C816M_Immediate16 },  { "sbc", AW65C816M_DirectX },  { "inc", AW65C816M_DirectX },  { "sbc", AW65C816M_DirectIndirectLongY },
	{ "sed", AW65C816M_Implied },  { "sbc", AW65C816M_AbsoluteY },  { "plx", AW65C816M_Implied },  { "xce", AW65C816M_Implied },
	{ "jsr", AW65C816M_JumpIndirectX },  { "sbc", AW65C816M_AbsoluteX },  { "inc", AW65C816M_AbsoluteX },  { "sbc", AW65C816M_LongX },
};


//Every mnemonic is three letters: as a key, five bits a letter (see AsmW65C816MnemonicKey).
#define ASM_W65C816_MNEMONIC_KEYS	(32 * 32 * 32)


//The mnemonics, and the opcode of each mnemonic in each mode (or -1), made from
//gasAsmW65C816Opcodes when first needed.  gaiAsmW65C816MnemonicByKey is each mnemonic's
//index by its key (or -1).
static const char*	gaszAsmW65C816Mnemonics[ASM_W65C816_MAX_MNEMONICS];
static int16		gaaiAsmW65C816Opcodes[ASM_W65C816_MAX_MNEMONICS][AW65C816M_NumModes];
static int8			gaiAsmW65C816MnemonicByKey[ASM_W65C816_MNEMONIC_KEYS];
static int			giAsmW65C816Mnemonics = 0;
static bool			gbAsmW65C816Indexed = false;


//////////////////////////////////////////////////////////////////////////
//
// The key of three letters (either case), or -1 if they aren't all letters.
//
//////////////////////////////////////////////////////////////////////////
static int AsmW65C816MnemonicKey(const char* sz)
{
	int		i;
	int		iKey;
	char	c;

	iKey = 0;
	for (i = 0; i < 3; i++)
	{
		c = sz[i];
		if ((c >= 'A') && (c <= 'Z'))
		{
			c = (char)(c - 'A' + 'a');
		}
		if ((c < 'a') || (c > 'z'))
		{
			return -1;
		}
		iKey = (iKey << 5) | (c - 'a');
	}
	return iKey;
}


//////////////////////////////////////////////////////////////////////////
//
// Makes the mnemonics' index, if it isn't made already.  Before units are assembled at the same time (it is
// made when first needed otherwise).
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816Assembler::IndexMnemonics(void)
{
	int		i;
	int		j;

	if (gbAsmW65C816Indexed)
	{
		return;
	}
	memset(gaiAsmW65C816MnemonicByKey, 0xFF, sizeof(gaiAsmW65C816MnemonicByKey));
	for (i = 0; i < 256; i++)
	{
		for (j = 0; j < giAsmW65C816Mnemonics; j++)
		{
			if (strcmp(gaszAsmW65C816Mnemonics[j], gasAsmW65C816Opcodes[i].szMnemonic) == 0)
			{
				break;
			}
		}
		if (j == giAsmW65C816Mnemonics)
		{
			gaszAsmW65C816Mnemonics[j] = gasAsmW65C816Opcodes[i].szMnemonic;
			memset(gaaiAsmW65C816Opcodes[j], 0xFF, sizeof(gaaiAsmW65C816Opcodes[j]));
			gaiAsmW65C816MnemonicByKey[AsmW65C816MnemonicKey(gasAsmW65C816Opcodes[i].szMnemonic)] = (int8)j;
			giAsmW65C816Mnemonics++;
		}
		gaaiAsmW65C816Opcodes[j][gasAsmW65C816Opcodes[i].uiMode] = (int16)i;
	}
	gbAsmW65C816Indexed = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The mnemonic's index, or -1.  Mnemonics are not case sensitive.
//
//////////////////////////////////////////////////////////////////////////
static int AsmW65C816FindMnemonic(SAsm65Token* psToken)
{
	int		iKey;

	CAsmW65C816Assembler::IndexMnemonics();
	if ((psToken->eType != AT65_Identifier) || (psToken->uiLength != 3))
	{
		return -1;
	}
	iKey = AsmW65C816MnemonicKey(psToken->szText);
	if (iKey < 0)
	{
		return -1;
	}
	return gaiAsmW65C816MnemonicByKey[iKey];
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool AsmW65C816Has(int iMnemonic, uint8 uiMode)
{
	return gaaiAsmW65C816Opcodes[iMnemonic][uiMode] >= 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool AsmW65C816IsJump(int iMnemonic)
{
	return AsmW65C816Has(iMnemonic, AW65C816M_JumpAbsolute) || AsmW65C816Has(iMnemonic, AW65C816M_JumpLong) || AsmW65C816Has(iMnemonic, AW65C816M_JumpIndirect) ||
		   AsmW65C816Has(iMnemonic, AW65C816M_JumpIndirectX) || AsmW65C816Has(iMnemonic, AW65C816M_JumpIndirectLong);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* AsmW65C816Mnemonic(uint8 uiOpcode, uint8* puiMode)
{
	*puiMode = gasAsmW65C816Opcodes[uiOpcode].uiMode;
	return gasAsmW65C816Opcodes[uiOpcode].szMnemonic;
}


//////////////////////////////////////////////////////////////////////////
//
// mnemonic {operand {, operand}}
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816Assembler::Instruction(SAsm65Token* psMnemonic)
{
	int				iMnemonic;
	SAsmW65C816Operand	sOperand;
	SAsmW65C816Operand	sSecond;
	uint8			uiMode;
	uint8			uiBits;
	bool			bOperand;
	bool			bResult;

	iMnemonic = AsmW65C816FindMnemonic(psMnemonic);
	if (iMnemonic < 0)
	{
		return Error(psMnemonic, "Unknown instruction");
	}

	memset(&sOperand, 0, sizeof(SAsmW65C816Operand));
	memset(&sSecond, 0, sizeof(SAsmW65C816Operand));
	bOperand = !Peek()->IsEndOfStatement();
	if (AsmW65C816Has(iMnemonic, AW65C816M_Immediate8) && (psMnemonic->IsIdentifierIgnoreCase("rep") || psMnemonic->IsIdentifierIgnoreCase("sep")))
	{
		sOperand.psToken = Peek();
		if (!bOperand)
		{
			return ErrorExpected(Peek(), "flags or a value");
		}
		if (!StatusBits(&uiBits))
		{
			return false;
		}
		ChangeWidths(psMnemonic->IsIdentifierIgnoreCase("sep"), uiBits);
		sOperand.sValue.iValue = uiBits;
		sOperand.sValue.bResolved = true;
		uiMode = AW65C816M_Immediate8;
	}
	else
	{
		mbRelative = AsmW65C816Has(iMnemonic, AW65C816M_Relative) || AsmW65C816Has(iMnemonic, AW65C816M_RelativeLong);
		if (bOperand && !OperandW65C816(&sOperand))
		{
			return false;
		}
		if (AsmW65C816Has(iMnemonic, AW65C816M_BlockMove))
		{
			if (!bOperand || !Expect(AP65_Comma, "','") || !OperandW65C816(&sSecond))
			{
				return bOperand ? false : ErrorExpected(Peek(), "the source bank");
			}
		}
		if (!ChooseMode(iMnemonic, bOperand ? &sOperand : NULL, psMnemonic, &uiMode))
		{
			return false;
		}
	}

	bResult = Encode(iMnemonic, uiMode, &sOperand, &sSecond, psMnemonic);

	//The widths are not known after the status is pulled or emulation is swapped.
	if (psMnemonic->IsIdentifierIgnoreCase("plp") || psMnemonic->IsIdentifierIgnoreCase("rti") || psMnemonic->IsIdentifierIgnoreCase("xce"))
	{
		muiWidthA = ASM_W65C816_WIDTH_UNKNOWN;
		muiWidthX = ASM_W65C816_WIDTH_UNKNOWN;
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The addressing mode an operand is written in, if the instruction has it.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816Assembler::ChooseMode(int iMnemonic, SAsmW65C816Operand* psOperand, SAsm65Token* psMnemonic, uint8* puiMode)
{
	uint8			uiMode;
	SAsm65Token*	psToken;

	psToken = psOperand ? psOperand->psToken : Peek();
	if (psOperand == NULL)
	{
		if (!AsmW65C816Has(iMnemonic, AW65C816M_Implied))
		{
			return ErrorExpected(psToken, "an operand");
		}
		*puiMode = AW65C816M_Implied;
		return true;
	}

	if (psOperand->bAccumulator)
	{
		uiMode = AW65C816M_Accumulator;
	}
	else if (!psOperand->bMemory)
	{
		if (AsmW65C816Has(iMnemonic, AW65C816M_ImmediateM))
		{
			uiMode = AW65C816M_ImmediateM;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_ImmediateX))
		{
			uiMode = AW65C816M_ImmediateX;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_Immediate8))
		{
			uiMode = AW65C816M_Immediate8;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_Immediate16))
		{
			uiMode = AW65C816M_Immediate16;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_Relative))
		{
			uiMode = AW65C816M_Relative;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_RelativeLong))
		{
			uiMode = AW65C816M_RelativeLong;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_JumpAbsolute))
		{
			uiMode = AW65C816M_JumpAbsolute;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_JumpLong))
		{
			uiMode = AW65C816M_JumpLong;
		}
		else if (AsmW65C816Has(iMnemonic, AW65C816M_BlockMove))
		{
			uiMode = AW65C816M_BlockMove;
		}
		else
		{
			return ErrorExpected(psToken, "memory in square brackets");
		}
		if ((psOperand->uiValueBits != 0) && (uiMode != AW65C816M_ImmediateM) && (uiMode != AW65C816M_ImmediateX))
		{
			return Error(psToken, "A width suffix is only for an immediate whose width is m or x");
		}
	}
	else if (AsmW65C816IsJump(iMnemonic))
	{
		//A jump's target is the address; in brackets it is read from memory.
		if ((psOperand->eBase != AW65C816B_Value) || psOperand->bPointer || (psOperand->cIndex == 'Y') || (psOperand->bLong && (psOperand->cIndex != 0)))
		{
			return Error(psToken, "A jump reads its target from [address], [address + X] or [address]:24");
		}
		uiMode = (psOperand->cIndex == 'X') ? AW65C816M_JumpIndirectX : (psOperand->bLong ? AW65C816M_JumpIndirectLong : AW65C816M_JumpIndirect);
	}
	else if (psOperand->eBase == AW65C816B_Direct)
	{
		if (psOperand->bLong)
		{
			return Error(psToken, "The direct page is in bank 0: write [[D + value]:24] for a 24 bit pointer");
		}
		if (!psOperand->bPointer)
		{
			uiMode = (psOperand->cIndex == 'X') ? AW65C816M_DirectX : ((psOperand->cIndex == 'Y') ? AW65C816M_DirectY : AW65C816M_Direct);
		}
		else if (psOperand->cInnerIndex == 'X')
		{
			if (psOperand->bLongPointer || (psOperand->cIndex != 0))
			{
				return Error(psToken, "[[D + value + X]] reads a 16 bit pointer and takes no more index");
			}
			uiMode = AW65C816M_DirectIndirectX;
		}
		else if (psOperand->bLongPointer)
		{
			uiMode = (psOperand->cIndex == 'Y') ? AW65C816M_DirectIndirectLongY : AW65C816M_DirectIndirectLong;
		}
		else
		{
			uiMode = (psOperand->cIndex == 'Y') ? AW65C816M_DirectIndirectY : AW65C816M_DirectIndirect;
		}
	}
	else if (psOperand->eBase == AW65C816B_Stack)
	{
		if (!psOperand->bPointer && (psOperand->cIndex == 0) && !psOperand->bLong)
		{
			uiMode = AW65C816M_Stack;
		}
		else if (psOperand->bPointer && (psOperand->cInnerIndex == 0) && !psOperand->bLongPointer && (psOperand->cIndex == 'Y') && !psOperand->bLong)
		{
			uiMode = AW65C816M_StackIndirectY;
		}
		else
		{
			return Error(psToken, "The stack is [S + value] or [[S + value] + Y]");
		}
	}
	else if (psOperand->bPointer)
	{
		return Error(psToken, "A pointer is read from the direct page or the stack: [[D + value]] or [[S + value] + Y]");
	}
	else if (psOperand->cIndex == 'Y')
	{
		if (psOperand->bLong)
		{
			return Error(psToken, "There is no long address indexed with Y");
		}
		uiMode = AW65C816M_AbsoluteY;
	}
	else if (psOperand->cIndex == 'X')
	{
		return AbsoluteOrLong(iMnemonic, psOperand, AW65C816M_AbsoluteX, AW65C816M_LongX, puiMode);
	}
	else
	{
		return AbsoluteOrLong(iMnemonic, psOperand, AW65C816M_Absolute, AW65C816M_Long, puiMode);
	}

	if (!AsmW65C816Has(iMnemonic, uiMode))
	{
		return Error(psToken, "The instruction has no such addressing mode");
	}
	*puiMode = uiMode;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// After .relax, whether a branch to a label must reach further than a byte:
// it did in an earlier pass (lengths only grow), it is in another section,
// or it is known to be too far.  A branch to C + offset never is.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816Assembler::IsLongBranch(SAsm65Value* psTarget)
{
	int64	lliOffset;

	if (!mbRelax || !psTarget->bLabel || (psTarget->iProgramCounter != 0))
	{
		return false;
	}
	if (GetMinimumLength() > 2)
	{
		return true;
	}
	if (!IsLocalTarget(psTarget) && IsRelocatable(psTarget))
	{
		return true;
	}
	if (!psTarget->bResolved)
	{
		return false;
	}
	lliOffset = psTarget->iValue - (int64)(muiStatementAddress + 2);
	return !Asm65Fits(lliOffset, 1, true);
}


//////////////////////////////////////////////////////////////////////////
//
// [value] is absolute (16 bits in the data bank) unless :24 is given or the value
// doesn't fit 16 bits, then long.  An instruction once long stays long in later passes.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816Assembler::AbsoluteOrLong(int iMnemonic, SAsmW65C816Operand* psOperand, uint8 uiAbsolute, uint8 uiLong, uint8* puiMode)
{
	bool	bLong;

	bLong = psOperand->bLong;
	if (!bLong && psOperand->sValue.bResolved && !IsRelocatable(&psOperand->sValue) && ((psOperand->sValue.iValue < 0) || (psOperand->sValue.iValue > 0xFFFF)))
	{
		if (!AsmW65C816Has(iMnemonic, uiLong))
		{
			return Error(psOperand->psToken, "Address doesn't fit 16 bits and the instruction has no long mode");
		}
		bLong = true;
	}
	if (!bLong && AsmW65C816Has(iMnemonic, uiLong) && (GetMinimumLength() > 3))
	{
		bLong = true;
	}
	if (bLong && !AsmW65C816Has(iMnemonic, uiLong))
	{
		return Error(psOperand->psToken, "The instruction has no long mode");
	}
	if (!bLong && !AsmW65C816Has(iMnemonic, uiAbsolute))
	{
		return Error(psOperand->psToken, "The instruction has no such addressing mode");
	}
	*puiMode = bLong ? uiLong : uiAbsolute;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// An immediate of the width m or x gives, or its :8 / :16 suffix.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816Assembler::Immediate(SAsmW65C816Instruction* psInstruction, SAsmW65C816Operand* psOperand, uint8 uiWidth, const char* szRegister)
{
	CChars	sz;
	bool	bResult;

	if (psOperand->uiValueBits == 24)
	{
		return Error(psOperand->psToken, "An immediate is :8 or :16");
	}
	if (uiWidth == ASM_W65C816_WIDTH_UNKNOWN)
	{
		if (psOperand->uiValueBits == 0)
		{
			sz.Init("The width of ");
			sz.Append(szRegister);
			sz.Append(" is not known here: use rep, sep, .width or a :8 or :16 suffix");
			bResult = Error(psOperand->psToken, sz.Text());
			sz.Kill();
			return bResult;
		}
		uiWidth = psOperand->uiValueBits;
	}
	else if ((psOperand->uiValueBits != 0) && (psOperand->uiValueBits != uiWidth))
	{
		sz.Init("The suffix is not the width of ");
		sz.Append(szRegister);
		sz.Append(" (");
		sz.Append((int)uiWidth);
		sz.Append(" bits)");
		bResult = Error(psOperand->psToken, sz.Text());
		sz.Kill();
		return bResult;
	}
	return AddField(psInstruction, &psOperand->sValue, uiWidth / 8, false, ASM65RF_Plain, psOperand->psToken);
}


//////////////////////////////////////////////////////////////////////////
//
// A direct page or stack offset: a constant from 0 to 255.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816Assembler::DirectOffset(SAsmW65C816Instruction* psInstruction, SAsmW65C816Operand* psOperand)
{
	if (IsRelocatable(&psOperand->sValue) || (psOperand->sValue.uiForm != ASM65RF_Plain))
	{
		return Error(psOperand->psToken, "A direct page or stack offset must be a constant");
	}
	if (psOperand->sValue.bResolved && ((psOperand->sValue.iValue < 0) || (psOperand->sValue.iValue > 0xFF)))
	{
		return Error(psOperand->psToken, "A direct page or stack offset is 0 to 255");
	}
	AddByte(psInstruction, (uint8)psOperand->sValue.iValue);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The opcode and its operand.  A jmp, jsr or jmp [Table + X] target is in the program
// bank: assembled absolute it must be in the instruction's bank, in an object the
// linker checks it (in bank).  So is a data address's bank: the data bank is taken to
// be the program bank; write :24 otherwise.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816Assembler::Encode(int iMnemonic, uint8 uiMode, SAsmW65C816Operand* psOperand, SAsmW65C816Operand* psSecond, SAsm65Token* psMnemonic)
{
	SAsmW65C816Instruction	sInstruction;
	SAsm65Value			sValue;
	bool				bResult;

	memset(&sInstruction, 0, sizeof(SAsmW65C816Instruction));
	AddByte(&sInstruction, (uint8)gaaiAsmW65C816Opcodes[iMnemonic][uiMode]);
	sValue = psOperand->sValue;

	switch (uiMode)
	{
	case AW65C816M_Implied:
	case AW65C816M_Accumulator:
		bResult = true;
		break;
	case AW65C816M_ImmediateM:
		bResult = Immediate(&sInstruction, psOperand, muiWidthA, "A");
		break;
	case AW65C816M_ImmediateX:
		bResult = Immediate(&sInstruction, psOperand, muiWidthX, "X");
		break;
	case AW65C816M_Immediate8:
		bResult = AddField(&sInstruction, &sValue, 1, false, ASM65RF_Plain, psOperand->psToken);
		break;
	case AW65C816M_Immediate16:
	case AW65C816M_JumpIndirect:
	case AW65C816M_JumpIndirectLong:
		bResult = AddField(&sInstruction, &sValue, 2, false, ASM65RF_Plain, psOperand->psToken);
		break;
	case AW65C816M_Absolute:
	case AW65C816M_AbsoluteX:
	case AW65C816M_AbsoluteY:
	case AW65C816M_JumpAbsolute:
	case AW65C816M_JumpIndirectX:
		if (!IsRelocatable(&sValue) && sValue.bResolved && (uiMode != AW65C816M_Absolute) && (uiMode != AW65C816M_AbsoluteX) && (uiMode != AW65C816M_AbsoluteY))
		{
			if ((sValue.iValue >> 16) != (muiStatementAddress >> 16))
			{
				return Error(psOperand->psToken, "Target is in another bank: use jml or jsl");
			}
			sValue.iValue &= 0xFFFF;
		}
		bResult = AddField(&sInstruction, &sValue, 2, false, ASM65RF_InBank, psOperand->psToken);
		break;
	case AW65C816M_Long:
	case AW65C816M_LongX:
	case AW65C816M_JumpLong:
		bResult = AddField(&sInstruction, &sValue, 3, false, ASM65RF_Plain, psOperand->psToken);
		break;
	case AW65C816M_Direct:
	case AW65C816M_DirectX:
	case AW65C816M_DirectY:
	case AW65C816M_DirectIndirect:
	case AW65C816M_DirectIndirectX:
	case AW65C816M_DirectIndirectY:
	case AW65C816M_DirectIndirectLong:
	case AW65C816M_DirectIndirectLongY:
	case AW65C816M_Stack:
	case AW65C816M_StackIndirectY:
		bResult = DirectOffset(&sInstruction, psOperand);
		break;
	case AW65C816M_Relative:
		if (IsLongBranch(&sValue))
		{
			//bra is brl; a conditional branch is the opposite branch over a brl (bit 5 of the opcode is the condition).
			if (sInstruction.auiBytes[0] == 0x80)
			{
				sInstruction.auiBytes[0] = 0x82;
				bResult = AddRelative(&sInstruction, &sValue, 2, 3, psOperand->psToken);
			}
			else
			{
				sInstruction.auiBytes[0] ^= 0x20;
				AddByte(&sInstruction, 3);
				AddByte(&sInstruction, 0x82);
				bResult = AddRelative(&sInstruction, &sValue, 2, 5, psOperand->psToken);
			}
		}
		else
		{
			bResult = AddRelative(&sInstruction, &sValue, 1, 2, psOperand->psToken);
		}
		break;
	case AW65C816M_RelativeLong:
		bResult = AddRelative(&sInstruction, &sValue, 2, 3, psOperand->psToken);
		break;
	case AW65C816M_BlockMove:
		//The destination bank's byte comes first.
		bResult = AddField(&sInstruction, &psSecond->sValue, 1, false, ASM65RF_Plain, psSecond->psToken) &&
				  AddField(&sInstruction, &sValue, 1, false, ASM65RF_Plain, psOperand->psToken);
		break;
	default:
		bResult = Error(psMnemonic, "Unknown addressing mode");
		break;
	}

	if (!bResult)
	{
		return false;
	}
	EmitW65C816(&sInstruction);
	return true;
}
