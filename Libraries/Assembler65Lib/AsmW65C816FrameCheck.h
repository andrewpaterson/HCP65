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
#ifndef __ASM65816_FRAME_CHECK_H__
#define __ASM65816_FRAME_CHECK_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "MicroprocessorLib/FastW65C816.h"
#include "Asm65Memory.h"


//A call being run: S before the jsl (S is below it while the call runs), the caller's D then, the lowest byte of the
//return address it pushed and the instruction it returns to.
struct SAsmW65C816FrameCall
{
	uint16	uiS;
	uint16	uiD;
	uint16	uiReturn;
	uint32	uiReturnTo;
};


//A store to a caller's stack: the instruction's address, the first byte it stores to, the lowest byte it may not
//(CAsmW65C816FrameCheck::Limit) and D then.
struct SAsmW65C816FrameWrite
{
	uint32	uiAddress;
	uint16	uiFirst;
	uint16	uiLimit;
	uint16	uiD;
};


//Runs a program on CFastW65C816 a step at a time and finds where a function writes its caller's stack through the
//direct page: a store (or read-modify-write) to the direct page by a function that has pointed D at a frame of its
//own, at or past the lower of its caller's frame (from the caller's D + 1) and its caller's return address, and
//within the stack.  A function's pushed arguments are below both, so it may write those; code running on its
//caller's D (a runtime routine, a stub) isn't checked; a store through a pointer isn't a direct page store.  As
//CAsmW65C816Lockstep checks the emulator, which runs it too (SetFrameCheck): BeforeStep and AfterStep go round each
//step of whatever runs the program.
class CAsmW65C816FrameCheck
{
protected:
	CAsm65Memory*						mpcMemory;
	CFastW65C816*						mpcFast;
	CArrayTemplate<SAsmW65C816FrameCall>	masCalls;
	uint16								muiStackTop;	//S at the first call: frames are below it.
	CArrayTemplate<SAsmW65C816FrameWrite>	masWrites;		//The first few found.
	CChars								mszWrites;

public:
	void	Init(CAsm65Memory* pcMemory, CFastW65C816* pcFast);
	void	Kill(void);

	size	Run(size uiMaxSteps);
	uint8	BeforeStep(void);
	void	AfterStep(uint8 uiOpcode);

	size					NumWrites(void);
	SAsmW65C816FrameWrite*	GetWrite(size uiIndex);
	char*					GetWrites(void);
	static void				AppendWrite(SAsmW65C816FrameWrite* psWrite, CChars* pszDest);

protected:
	void	Check(uint32 uiAddress, uint8 uiOpcode);
	uint32	Limit(void);
};


#endif // __ASM65816_FRAME_CHECK_H__
