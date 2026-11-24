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
#ifndef __ASM65_FRAME_CHECK_H__
#define __ASM65_FRAME_CHECK_H__
#include "BaseLib/ArrayTemplate.h"
#include "Asm65StackWalker.h"


//A call being run: S before it (S is below it while the call runs), the lowest byte of the return address it pushed,
//the instruction it returns to, and the register calls keep (the W65C816's D, the 65C64's V) as it was.
struct SAsm65FrameCall
{
	uint32	uiS;
	uint32	uiReturn;
	uint32	uiReturnTo;
	uint64	uiKept;
};


//Follows the calls a program runs, a step at a time, as the truth a stack walker's walk is checked against
//(WrongFrame): each caller where its call returns to, with S as it was before the call and the register calls keep.
//What isn't a processor's is here: the calls, their ends, the check.  A processor's (CAsmW65C816FrameCheck,
//CAsm65C64FrameCheck) says which instructions call and return, and runs the processor.
class CAsm65FrameCheck
{
protected:
	CArrayTemplate<SAsm65FrameCall>	masCalls;
	uint32							muiStackTop;	//S at the first call: frames are below it.

public:
	void				Init(void);
	virtual void		Kill(void);

	size				Run(size uiMaxSteps);
	virtual bool		IsStopped(void) = 0;

	size				NumCalls(void) const;
	SAsm65FrameCall*	GetCall(size uiIndex);
	size				WrongFrame(CArrayAsm65StackFrames* pasFrames);

protected:
	void				AddCall(uint32 uiS, uint32 uiReturn, uint32 uiReturnTo);
	void				Returned(uint32 uiPC, uint32 uiS);

	virtual void		Step(void) = 0;
	virtual size		KeptRegister(void) = 0;
	virtual uint64		Kept(void) = 0;
};


#endif // __ASM65_FRAME_CHECK_H__

