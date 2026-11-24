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
#ifndef __ASM_W65C816_STACK_WALKER_H__
#define __ASM_W65C816_STACK_WALKER_H__
#include "Asm65StackWalker.h"


//The W65C816's part of a stack walk (CAsm65StackWalker): following a prologue from the function's start (S's distance
//below its entry S, the copies of S in A, X and Y, and where phd pushed the caller's D), and an epilogue to its rtl,
//rts or rti (pulls only); jsl's, jsr's and an interrupt's returns; its stacks in bank 0, its pointers 24 bits.  An
//indirect call (phk, per, the target pushed, rtl) is a call, not a return (AsmW65C816IsIndirectCall).
class CAsmW65C816StackWalker : public CAsm65StackWalker
{
public:
	EAsm65DebugProcessor	GetProcessor(void) override;

protected:
	bool	Follow(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller) override;
	bool	Returning(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller) override;
	void	CallBytes(EAsm65DebugEntry eEntry, uint32* puiPBytes, uint32* puiReturnBytes) override;
	uint32	ReturnAddress(EAsm65DebugEntry eEntry, uint64 uiPushed, uint32 uiCalleePC) override;
	uint32	PreservedRegisters(void) override;
	uint32	StackAddress(int64 iAddress) override;
	size	PointerBytes(void) override;

	bool	IsIndirectCall(EAsm65StackSpace eSpace, uint32 uiAddress, uint16 uiS);
	bool	ReadWord(EAsm65StackSpace eSpace, uint32 uiAddress, uint16* puiWord);
	size	Instruction(EAsm65StackSpace eSpace, uint32 uiAddress, uint8* puiBytes, bool bIndex8, bool bMemory8);
};


#endif // __ASM_W65C816_STACK_WALKER_H__
