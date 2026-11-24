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
#include "Asm65FrameCheck.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65FrameCheck::Init(void)
{
	masCalls.Init();
	muiStackTop = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65FrameCheck::Kill(void)
{
	masCalls.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Runs up to uiMaxSteps instructions, or until the processor stops,
// following each (Step).  Returns the steps run.
//
//////////////////////////////////////////////////////////////////////////
size CAsm65FrameCheck::Run(size uiMaxSteps)
{
	size	uiSteps;

	for (uiSteps = 0; (uiSteps < uiMaxSteps) && !IsStopped(); uiSteps++)
	{
		Step();
	}
	return uiSteps;
}


//////////////////////////////////////////////////////////////////////////
//
// A call starting: S before it, the lowest byte of its return address,
// the instruction that is, the kept register now.  The stack's top is S
// at the first call (the start up's, once it has set S).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65FrameCheck::AddCall(uint32 uiS, uint32 uiReturn, uint32 uiReturnTo)
{
	SAsm65FrameCall*	psCall;

	if (masCalls.NumElements() == 0)
	{
		muiStackTop = uiS;
	}
	psCall = masCalls.Add();
	psCall->uiS = uiS;
	psCall->uiReturn = uiReturn;
	psCall->uiReturnTo = uiReturnTo;
	psCall->uiKept = Kept();
}


//////////////////////////////////////////////////////////////////////////
//
// A return ran, to uiPC with S uiS: it ends the call it returned to the
// caller of, and those that started at or below S now (a subroutine that
// moves S, as a frame entry helper's does, comes back with S below where
// it started).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65FrameCheck::Returned(uint32 uiPC, uint32 uiS)
{
	if ((masCalls.NumElements() > 0) && (masCalls.Tail()->uiReturnTo == uiPC))
	{
		masCalls.RemoveTail();
	}
	while ((masCalls.NumElements() > 0) && (masCalls.Tail()->uiS <= uiS))
	{
		masCalls.RemoveTail();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CAsm65FrameCheck::NumCalls(void) const
{
	return masCalls.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsm65FrameCall* CAsm65FrameCheck::GetCall(size uiIndex)
{
	return masCalls.Get(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The first frame of a walk (CAsm65StackWalker's, from the registers as
// they are before the next step) that isn't the call it should be: each
// caller where its call returns to, with S as it was before the call and
// the register calls keep.  The walk may stop at code the debug records
// don't describe; stopping anywhere else, its index is the walk's length.
// 0 if the walk is right.
//
//////////////////////////////////////////////////////////////////////////
size CAsm65FrameCheck::WrongFrame(CArrayAsm65StackFrames* pasFrames)
{
	SAsm65StackFrame*	psFrame;
	SAsm65FrameCall*	psCall;
	size				uiFrames;
	size				uiCalls;
	size				ui;
	size				uiKept;

	uiKept = KeptRegister();
	uiFrames = pasFrames->NumElements();
	uiCalls = masCalls.NumElements();
	for (ui = 1; ui < uiFrames; ui++)
	{
		psFrame = pasFrames->Get(ui);
		psCall = (ui <= uiCalls) ? masCalls.Get(uiCalls - ui) : NULL;
		if (!psCall || (psFrame->uiPC != psCall->uiReturnTo) || (psFrame->GetS() != psCall->uiS) || !psFrame->IsKnown(uiKept) || (psFrame->Get(uiKept) != psCall->uiKept))
		{
			return ui;
		}
	}
	if ((uiFrames > 0) && pasFrames->Tail()->psFunction && (uiFrames <= uiCalls))
	{
		return uiFrames;
	}
	return 0;
}

