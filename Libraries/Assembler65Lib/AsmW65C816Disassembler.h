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
#ifndef __ASM_W65C816_DISASSEMBLER_H__
#define __ASM_W65C816_DISASSEMBLER_H__
#include "BaseLib/Chars.h"


//The widths a W65C816's immediates are decoded with: m's (A and memory) and x's (X and Y).  In emulation mode both
//are 8.  rep and sep change them as they are disassembled, so a run of code keeps them where it can.
struct SAsmW65C816Widths
{
	bool	bMemory8;
	bool	bIndex8;
};


//Disassembles the instruction at puiBytes (uiAvailable of them), which is at uiAddress, in AssemblerW65C816Syntax.txt's
//syntax: "lda [D + $04]".  It reassembles to the same bytes, except that a branch's operand (bra, brl, per) is
//written as the address it goes to, not C plus an offset.  Returns the instruction's length, or 0 (nothing
//appended) if there aren't that many bytes.
size	AsmW65C816Disassemble(uint32 uiAddress, uint8* puiBytes, size uiAvailable, SAsmW65C816Widths* psWidths, CChars* pszDest);

//The length of the instruction starting with uiOpcode, with those widths.
size	AsmW65C816InstructionLength(uint8 uiOpcode, SAsmW65C816Widths* psWidths);


#endif // __ASM_W65C816_DISASSEMBLER_H__
