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
#ifndef __ASM_65C64_STACK_WALKER_H__
#define __ASM_65C64_STACK_WALKER_H__
#include "BaseLib/ArrayTemplate.h"
#include "MicroprocessorLib/65C64Decoded.h"
#include "Asm65StackWalker.h"


//What a register holds while CAsm65C64StackWalker follows a function's code: its entry S less iBelow, if known.
struct SAsm65C64StackValue
{
	int64	iBelow;
	bool	bKnown;

	void	Set(int64 iBelowEntry)	{ iBelow = iBelowEntry; bKnown = true; }
	void	Forget(void)			{ bKnown = false; }
};


//The 65C64's registers as a walker follows them: each one's distance below the entry S (S, and the registers copied
//from it), the widths A, B, X and Y push at (0 unknown), and where the caller's V was pushed.
struct SAsm65C64StackState
{
	SAsm65C64StackValue	asRegisters[8];		//By E65Register.
	uint8				auiBytes[8];		//By E65Register: S, V and W are always 4.
	SAsm65C64StackValue	sPushedV;			//The caller's V's lowest byte is at the entry S less iBelow.
	bool				bCallerV;			//V is still the caller's.

	void	Init(uint16 uiP, bool bP);
	void	ForgetAll(void);
	uint8	Bytes(E65Register eRegister);
};


//A forward branch's target and the registers as a walk follows them there (CAsm65C64StackWalker::Follow): the code
//after a bra or a ret is got to by its branches, not from the instruction before it.
struct SAsm65C64StackBranch
{
	uint32				uiTarget;
	SAsm65C64StackState	sState;
};


//The 65C64's part of a stack walk (CAsm65StackWalker): following a function's code from its start (pushes and pulls
//on any stack register, S moved by add and sub, copies of S in other registers, the widths wth sets, where psh V put
//the caller's V; after a bra the code goes on as its forward branches left it, after a ret as it was before that
//exit's pulls, as widths may differ from path to path though S doesn't) and its epilogue to its ret or rti
//(pulls and add S only).  jsr pushes its 4 byte return; an interrupt (or brk) the 4 byte address it goes back to and
//then P.  Calls keep S and V.  Stacks and pointers are 32 bits.
class CAsm65C64StackWalker : public CAsm65StackWalker
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

	size	Instruction(EAsm65StackSpace eSpace, uint32 uiAddress, S65C64Decoded* psDecoded, uint64* puiImmediate);
	bool	Step(SAsm65C64StackState* psState, S65C64Decoded* psDecoded, uint64 uiImmediate);
	bool	Stack(SAsm65C64StackState* psState, E65Register eStack, int64 iBytes);
};


#endif // __ASM_65C64_STACK_WALKER_H__

