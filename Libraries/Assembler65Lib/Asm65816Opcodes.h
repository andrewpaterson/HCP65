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
struct SAsm65816DirectPageWrite
{
	char	cIndex;
	bool	bIndexWidth;
};


bool	Asm65816HasDirectPageOperand(uint8 uiOpcode);
bool	Asm65816DirectPageWrite(uint8 uiOpcode, SAsm65816DirectPageWrite* psWrite);
bool	Asm65816IsSubroutineCall(uint8 uiOpcode);
bool	Asm65816IsCall(uint8 uiOpcode);
bool	Asm65816IsReturn(uint8 uiOpcode);


#endif // __ASM65816_OPCODES_H__
