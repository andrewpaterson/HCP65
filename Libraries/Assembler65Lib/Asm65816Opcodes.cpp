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
#include "Asm65816Opcodes.h"


//////////////////////////////////////////////////////////////////////////
//
// Whether a W65C816 opcode has a direct page operand (dp, dp,x, dp,y,
// (dp), (dp),y, (dp,x), [dp], [dp],y or pei): a cycle more when D's low byte
// isn't 0.
//
//////////////////////////////////////////////////////////////////////////
bool Asm65816HasDirectPageOperand(uint8 uiOpcode)
{
	uint8	uiLow;

	uiLow = uiOpcode & 0x1F;
	if ((uiOpcode & 0x03) == 0x01)
	{
		//ora and eor adc sta lda cmp sbc: (dp,x), dp, [dp], (dp),y, dp,x, [dp],y.
		return (uiLow == 0x01) || (uiLow == 0x05) || (uiLow == 0x07) || (uiLow == 0x11) || (uiLow == 0x15) || (uiLow == 0x17);
	}
	if (uiLow == 0x12)
	{
		//(dp): ora and eor adc sta lda cmp sbc.
		return true;
	}
	switch (uiOpcode)
	{
	case 0x04: case 0x14: case 0x24: case 0x34: case 0x64: case 0x74:
	case 0x06: case 0x16: case 0x26: case 0x36: case 0x46: case 0x56: case 0x66: case 0x76:
	case 0x84: case 0x94: case 0x86: case 0x96: case 0xA4: case 0xB4: case 0xA6: case 0xB6:
	case 0xC4: case 0xC6: case 0xD6: case 0xE4: case 0xE6: case 0xF6: case 0xD4:
		return true;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether an instruction writes the direct page itself (sta, stx, sty and
// stz to dp or dp indexed, and the read-modify-writes asl, rol, lsr, ror,
// inc, dec, tsb and trb of dp or dp,x), and how (psWrite).  A store through
// a pointer in the direct page ((dp), [dp], ...) writes elsewhere.
//
//////////////////////////////////////////////////////////////////////////
bool Asm65816DirectPageWrite(uint8 uiOpcode, SAsmW65C816DirectPageWrite* psWrite)
{
	psWrite->cIndex = 0;
	psWrite->bIndexWidth = false;
	switch (uiOpcode)
	{
	case 0x85: case 0x64:
	case 0x06: case 0x26: case 0x46: case 0x66: case 0xC6: case 0xE6: case 0x04: case 0x14:
		return true;
	case 0x95: case 0x74:
	case 0x16: case 0x36: case 0x56: case 0x76: case 0xD6: case 0xF6:
		psWrite->cIndex = 'X';
		return true;
	case 0x86: case 0x84:
		psWrite->bIndexWidth = true;
		return true;
	case 0x96:
		psWrite->cIndex = 'Y';
		psWrite->bIndexWidth = true;
		return true;
	case 0x94:
		psWrite->cIndex = 'X';
		psWrite->bIndexWidth = true;
		return true;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether an instruction calls a subroutine that returns to the next one:
// jsl, jsr or jsr (a,x).
//
//////////////////////////////////////////////////////////////////////////
bool Asm65816IsSubroutineCall(uint8 uiOpcode)
{
	return (uiOpcode == 0x22) || (uiOpcode == 0x20) || (uiOpcode == 0xFC);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether an instruction gets to a function's start by calling it: jsl, jsr,
// jsr (a,x), a jump (a tail call: jml, jmp, jml [a], jmp (a), jmp (a,x)) or
// the rtl of an indirect call (the address pushed, then rtl).  A branch to it
// is a loop.
//
//////////////////////////////////////////////////////////////////////////
bool Asm65816IsCall(uint8 uiOpcode)
{
	switch (uiOpcode)
	{
	case 0x22: case 0x20: case 0xFC: case 0x5C: case 0x4C: case 0xDC: case 0x6C: case 0x7C: case 0x6B:
		return true;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether an instruction returns: rtl, rts or rti.
//
//////////////////////////////////////////////////////////////////////////
bool Asm65816IsReturn(uint8 uiOpcode)
{
	return (uiOpcode == 0x6B) || (uiOpcode == 0x60) || (uiOpcode == 0x40);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
EAsm65816Flow Asm65816Flow(uint8 uiOpcode)
{
	switch (uiOpcode)
	{
	case 0x20: case 0x22: case 0xFC: case 0x00: case 0x02:
		return A65816F_Call;
	case 0x60: case 0x6B: case 0x40:
		return A65816F_Return;
	case 0x4C: case 0x5C: case 0x6C: case 0x7C: case 0xDC: case 0x80: case 0x82:
		return A65816F_Jump;
	case 0x10: case 0x30: case 0x50: case 0x70: case 0x90: case 0xB0: case 0xD0: case 0xF0:
		return A65816F_Branch;
	default:
		return A65816F_Other;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where the instruction at uiAddress (puiBytes, all of it) goes, if its
// operand says: a branch's (from the next instruction, in the program
// bank), an absolute jump's or call's (in the program bank) or a long
// one's.  False through a vector, or for any other instruction.
//
//////////////////////////////////////////////////////////////////////////
bool Asm65816FlowTarget(uint32 uiAddress, uint8* puiBytes, uint32* puiTarget)
{
	uint32	uiBank;
	uint32	uiAbsolute;

	//Only the instruction's own bytes are read: a two byte branch has no puiBytes[2].
	uiBank = uiAddress & 0xFF0000;
	switch (puiBytes[0])
	{
	case 0x10: case 0x30: case 0x50: case 0x70: case 0x90: case 0xB0: case 0xD0: case 0xF0: case 0x80:
		*puiTarget = uiBank | ((uiAddress + 2 + (uint32)(int32)(int8)puiBytes[1]) & 0xFFFF);
		return true;
	case 0x82:
		uiAbsolute = (uint32)puiBytes[1] | ((uint32)puiBytes[2] << 8);
		*puiTarget = uiBank | ((uiAddress + 3 + (uint32)(int32)(int16)uiAbsolute) & 0xFFFF);
		return true;
	case 0x4C: case 0x20:
		uiAbsolute = (uint32)puiBytes[1] | ((uint32)puiBytes[2] << 8);
		*puiTarget = uiBank | uiAbsolute;
		return true;
	case 0x5C: case 0x22:
		uiAbsolute = (uint32)puiBytes[1] | ((uint32)puiBytes[2] << 8);
		*puiTarget = uiAbsolute | ((uint32)puiBytes[3] << 16);
		return true;
	default:
		return false;
	}
}
