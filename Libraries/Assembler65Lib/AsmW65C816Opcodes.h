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
#ifndef __ASM65816_OPCODES_H__
#define __ASM65816_OPCODES_H__
#include "BaseLib/PrimitiveTypes.h"


//How an instruction writes the direct page itself (not through a pointer there): the index register added to its
//operand (0, 'X' or 'Y') and whether the X flag sets its width (stx, sty) rather than M.
struct SAsmW65C816DirectPageWrite
{
	char	cIndex;
	bool	bIndexWidth;
};


//What an instruction does to the flow of the program, for a debugger stepping it.
enum EAsm65816Flow
{
	A65816F_Other,
	A65816F_Call,		//jsr, jsl, and brk and cop (a software interrupt returns past its signature byte).
	A65816F_Return,		//rts, rtl, rti.
	A65816F_Jump,		//jmp, jml, bra, brl.
	A65816F_Branch,		//A conditional branch.
};


bool	Asm65816HasDirectPageOperand(uint8 uiOpcode);
bool	Asm65816DirectPageWrite(uint8 uiOpcode, SAsmW65C816DirectPageWrite* psWrite);
bool	Asm65816IsSubroutineCall(uint8 uiOpcode);
bool	Asm65816IsCall(uint8 uiOpcode);
bool	Asm65816IsReturn(uint8 uiOpcode);
EAsm65816Flow	Asm65816Flow(uint8 uiOpcode);
bool	Asm65816FlowTarget(uint32 uiAddress, uint8* puiBytes, uint32* puiTarget);


#endif // __ASM65816_OPCODES_H__
