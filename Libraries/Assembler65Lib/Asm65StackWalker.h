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
#ifndef __ASM65_STACK_WALKER_H__
#define __ASM65_STACK_WALKER_H__
#include "BaseLib/ArrayTemplate.h"
#include "Asm65DebugRecords.h"


class CFastW65C816;


//The memories a walk reads.  A computer with a user mode sees other memory there at the same addresses (the HCP65's
//processes, each its own); one without has only the kernel's.  A kernel's interrupt from user mode is called from user
//mode's code (EAsm65DebugLink's ADL_User).
enum EAsm65StackSpace
{
	ASS_Kernel,
	ASS_User,
};


//The memory of a W65C816 that has stopped, as a stack walker reads it: its code and its stacks.
class CAsm65StackMemory
{
public:
	virtual bool	Read(EAsm65StackSpace eSpace, uint32 uiAddress, void* pvDest, size uiBytes) = 0;
};


//The registers a walk starts from, and the space the code runs in.  A, X and Y find the frame of a function caught
//while its prologue makes it (an aligned frame's S is in a register before it is kept); P's m and x say how wide what
//it pulls before it returns is.
struct SAsm65StackRegisters
{
	uint32	uiPC;
	uint16	uiS;
	uint16	uiD;
	uint16	uiC;
	uint16	uiX;
	uint16	uiY;
	uint8	uiP;
	uint8	uiSpace;	//EAsm65StackSpace.

	void	Init(CFastW65C816* pcFast, EAsm65StackSpace eSpace);
};


//A function on the stack: where it is (uiPC, the instruction it runs next, or for a caller the one its call returns
//to), S as it was before the call it made (its own S for the first), its D, and the space its code and stack are in.
//psFunction is NULL for code the debug records don't describe: the walk stops there.
struct SAsm65StackFrame
{
	uint32					uiPC;
	uint16					uiS;
	uint16					uiD;
	uint8					uiSpace;	//EAsm65StackSpace.
	SAsm65DebugFunction*	psFunction;
	CAsm65DebugRecords*		pcRecords;

	uint32	GetLineAddress(bool bInnermost);
};


//A program's records and the space it runs in.
struct SAsm65StackRecords
{
	CAsm65DebugRecords*	pcRecords;
	EAsm65StackSpace	eSpace;
};


typedef CArrayTemplate<SAsm65StackFrame>	CArrayAsm65StackFrames;
typedef CArrayTemplate<SAsm65StackRecords>	CArrayAsm65StackRecords;


//Finds the calls a stopped W65C816 is in from its registers, its memory and the debug records of the programs loaded
//(a program's, the kernal's): each function's caller from how its frame was made (SAsm65DebugFunction's link), or
//while it makes it or takes it apart, from the instructions it runs (DebuggerPlan.txt B2).  How the function was got
//to (its entry: jsl, jsr or an interrupt) says what is above its entry S.
class CAsm65StackWalker
{
protected:
	CArrayAsm65StackRecords	masRecords;
	CAsm65StackMemory*		mpcMemory;

public:
	void	Init(CAsm65StackMemory* pcMemory);
	void	Kill(void);

	void	AddRecords(CAsm65DebugRecords* pcRecords, EAsm65StackSpace eSpace);
	size	Walk(SAsm65StackRegisters* psRegisters, CArrayAsm65StackFrames* pasFrames, size uiMaxFrames);
	bool	Locate(SAsm65StackFrame* psFrame, SAsm65DebugVariable* psVariable, uint32* puiAddress);

protected:
	bool	Find(uint32 uiPC, EAsm65StackSpace eSpace, SAsm65StackFrame* psFrame);
	bool	Caller(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psRegisters, SAsm65StackFrame* psCaller);
	bool	Returning(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psRegisters, SAsm65StackFrame* psCaller);
	bool	Entering(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psRegisters, SAsm65StackFrame* psCaller);
	bool	Linked(SAsm65StackFrame* psFrame, SAsm65StackFrame* psCaller);
	bool	ReturnTo(SAsm65StackFrame* psFrame, uint16 uiEntryS, uint16 uiD, EAsm65DebugEntry eEntry, EAsm65StackSpace eSpace, SAsm65StackFrame* psCaller);
	bool	IsIndirectCall(EAsm65StackSpace eSpace, uint32 uiAddress, uint16 uiS);
	bool	ReadWord(EAsm65StackSpace eSpace, uint32 uiAddress, uint16* puiWord);
	size	Instruction(EAsm65StackSpace eSpace, uint32 uiAddress, uint8* puiBytes, bool bIndex8, bool bMemory8);
};


#endif // __ASM65_STACK_WALKER_H__

