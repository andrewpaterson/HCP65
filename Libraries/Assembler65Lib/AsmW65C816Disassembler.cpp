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
#include <stdio.h>
#include "AsmW65C816Assembler.h"
#include "AsmW65C816Disassembler.h"


#define ASM65816_REP	0xC2
#define ASM65816_SEP	0xE2
#define ASM65816_M_FLAG	0x20
#define ASM65816_X_FLAG	0x10


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size Asm65816InstructionLength(uint8 uiOpcode, SAsmW65C816Widths* psWidths)
{
	uint8	uiMode;

	Asm65816Mnemonic(uiOpcode, &uiMode);
	switch (uiMode)
	{
	case AW65C816M_Implied:
	case AW65C816M_Accumulator:
		return 1;

	case AW65C816M_ImmediateM:
		return psWidths->bMemory8 ? 2 : 3;

	case AW65C816M_ImmediateX:
		return psWidths->bIndex8 ? 2 : 3;

	case AW65C816M_Immediate8:
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
	case AW65C816M_Relative:
		return 2;

	case AW65C816M_Long:
	case AW65C816M_LongX:
	case AW65C816M_JumpLong:
		return 4;

	default:
		//Immediate16, the absolutes, RelativeLong, the jumps through vectors and BlockMove.
		return 3;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void Asm65816AppendHex(CChars* pszDest, uint32 uiValue, int iDigits)
{
	char	sz[16];

	sprintf(sz, "$%0*X", iDigits, uiValue);
	pszDest->Append(sz);
}


//////////////////////////////////////////////////////////////////////////
//
// rep and sep make the widths 16 and 8.
//
//////////////////////////////////////////////////////////////////////////
static void Asm65816ChangeWidths(uint8 uiOpcode, uint8 uiFlags, SAsmW65C816Widths* psWidths)
{
	bool	bSet;

	if ((uiOpcode != ASM65816_REP) && (uiOpcode != ASM65816_SEP))
	{
		return;
	}
	bSet = uiOpcode == ASM65816_SEP;
	if (uiFlags & ASM65816_M_FLAG)
	{
		psWidths->bMemory8 = bSet;
	}
	if (uiFlags & ASM65816_X_FLAG)
	{
		psWidths->bIndex8 = bSet;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size Asm65816Disassemble(uint32 uiAddress, uint8* puiBytes, size uiAvailable, SAsmW65C816Widths* psWidths, CChars* pszDest)
{
	const char*	szMnemonic;
	uint8		uiMode;
	size		uiLength;
	uint32		uiOperand;
	uint32		uiTarget;
	size		i;

	if (uiAvailable == 0)
	{
		return 0;
	}
	uiLength = Asm65816InstructionLength(puiBytes[0], psWidths);
	if (uiLength > uiAvailable)
	{
		return 0;
	}

	uiOperand = 0;
	for (i = 1; i < uiLength; i++)
	{
		uiOperand |= (uint32)puiBytes[i] << (8 * (i - 1));
	}

	szMnemonic = Asm65816Mnemonic(puiBytes[0], &uiMode);
	pszDest->Append(szMnemonic);
	if (uiMode == AW65C816M_Implied)
	{
		return uiLength;
	}

	pszDest->Append(' ');
	switch (uiMode)
	{
	case AW65C816M_Accumulator:
		pszDest->Append('A');
		break;

	case AW65C816M_ImmediateM:
	case AW65C816M_ImmediateX:
	case AW65C816M_Immediate8:
	case AW65C816M_Immediate16:
		Asm65816AppendHex(pszDest, uiOperand, 2 * (int)(uiLength - 1));
		break;

	case AW65C816M_Absolute:
	case AW65C816M_JumpIndirect:
		pszDest->Append('[');
		Asm65816AppendHex(pszDest, uiOperand, 4);
		pszDest->Append(']');
		break;

	case AW65C816M_AbsoluteX:
	case AW65C816M_JumpIndirectX:
		pszDest->Append('[');
		Asm65816AppendHex(pszDest, uiOperand, 4);
		pszDest->Append(" + X]");
		break;

	case AW65C816M_AbsoluteY:
		pszDest->Append('[');
		Asm65816AppendHex(pszDest, uiOperand, 4);
		pszDest->Append(" + Y]");
		break;

	case AW65C816M_Long:
		pszDest->Append('[');
		Asm65816AppendHex(pszDest, uiOperand, 6);
		pszDest->Append("]:24");
		break;

	case AW65C816M_LongX:
		pszDest->Append('[');
		Asm65816AppendHex(pszDest, uiOperand, 6);
		pszDest->Append(" + X]:24");
		break;

	case AW65C816M_JumpIndirectLong:
		pszDest->Append('[');
		Asm65816AppendHex(pszDest, uiOperand, 4);
		pszDest->Append("]:24");
		break;

	case AW65C816M_Direct:
		pszDest->Append("[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append(']');
		break;

	case AW65C816M_DirectX:
		pszDest->Append("[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append(" + X]");
		break;

	case AW65C816M_DirectY:
		pszDest->Append("[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append(" + Y]");
		break;

	case AW65C816M_DirectIndirect:
		pszDest->Append("[[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append("]]");
		break;

	case AW65C816M_DirectIndirectX:
		pszDest->Append("[[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append(" + X]]");
		break;

	case AW65C816M_DirectIndirectY:
		pszDest->Append("[[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append("] + Y]");
		break;

	case AW65C816M_DirectIndirectLong:
		pszDest->Append("[[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append("]:24]");
		break;

	case AW65C816M_DirectIndirectLongY:
		pszDest->Append("[[D + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append("]:24 + Y]");
		break;

	case AW65C816M_Stack:
		pszDest->Append("[S + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append(']');
		break;

	case AW65C816M_StackIndirectY:
		pszDest->Append("[[S + ");
		Asm65816AppendHex(pszDest, uiOperand, 2);
		pszDest->Append("] + Y]");
		break;

	case AW65C816M_Relative:
		//In the program bank, from the next instruction.
		uiTarget = (uiAddress & 0xFF0000) | ((uiAddress + 2 + (uint32)(int32)(int8)uiOperand) & 0xFFFF);
		Asm65816AppendHex(pszDest, uiTarget, 4);
		break;

	case AW65C816M_RelativeLong:
		uiTarget = (uiAddress & 0xFF0000) | ((uiAddress + 3 + (uint32)(int32)(int16)uiOperand) & 0xFFFF);
		Asm65816AppendHex(pszDest, uiTarget, 4);
		break;

	case AW65C816M_JumpAbsolute:
		Asm65816AppendHex(pszDest, uiOperand, 4);
		break;

	case AW65C816M_JumpLong:
		Asm65816AppendHex(pszDest, uiOperand, 6);
		break;

	case AW65C816M_BlockMove:
		//The destination bank is first in memory and last in the source.
		Asm65816AppendHex(pszDest, uiOperand >> 8, 2);
		pszDest->Append(", ");
		Asm65816AppendHex(pszDest, uiOperand & 0xFF, 2);
		break;
	}

	if (uiLength > 1)
	{
		Asm65816ChangeWidths(puiBytes[0], puiBytes[1], psWidths);
	}
	return uiLength;
}
