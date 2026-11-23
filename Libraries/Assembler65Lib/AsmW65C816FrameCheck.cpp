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
#include <stdio.h>
#include "AsmW65C816Opcodes.h"
#include "AsmW65C816FrameCheck.h"


#define ASM_W65C816_FRAME_CHECK_MOST	4		//The stores to callers' stacks kept (GetWrite): the first ones found.


//////////////////////////////////////////////////////////////////////////
//
// pcFast has run reset on pcMemory.
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::Init(CAsm65Memory* pcMemory, CFastW65C816* pcFast)
{
	mpcMemory = pcMemory;
	mpcFast = pcFast;
	masCalls.Init();
	muiStackTop = 0;
	masWrites.Init();
	mszWrites.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::Kill(void)
{
	mszWrites.Kill();
	masWrites.Kill();
	masCalls.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Runs up to uiMaxSteps instructions, or until stp, checking each (Before
// and AfterStep).  Returns the steps run.
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816FrameCheck::Run(size uiMaxSteps)
{
	size	uiSteps;
	uint8	uiOpcode;

	for (uiSteps = 0; (uiSteps < uiMaxSteps) && !mpcFast->IsStopped(); uiSteps++)
	{
		uiOpcode = BeforeStep();
		mpcFast->Step();
		AfterStep(uiOpcode);
	}
	return uiSteps;
}


//////////////////////////////////////////////////////////////////////////
//
// Before the instruction at the program address runs: checks a direct
// page store (Check), and a jsl (jsr), a cop or brk, or an rtl that calls
// through a pointer (AsmW65C816IsIndirectCall), starts a call.  Returns
// its opcode, for AfterStep.
//
//////////////////////////////////////////////////////////////////////////
uint8 CAsmW65C816FrameCheck::BeforeStep(void)
{
	uint32	uiAddress;
	uint16	uiS;
	uint8	uiOpcode;
	uint8	auiStack[6];
	size	ui;

	uiAddress = mpcFast->GetProgramAddress();
	uiOpcode = mpcMemory->GetByte(uiAddress);
	uiS = mpcFast->GetS();
	Check(uiAddress, uiOpcode);
	if (AsmW65C816IsSubroutineCall(uiOpcode))
	{
		AddCall(uiS, uiS - ((uiOpcode == 0x22) ? 2 : 1), uiAddress + ((uiOpcode == 0x22) ? 4 : 3));
	}
	else if ((uiOpcode == 0x02) || (uiOpcode == 0x00))
	{
		//The program bank, where it comes back to (after the signature byte), then P.
		AddCall(uiS, uiS - 2, uiAddress + 2);
	}
	else if (uiOpcode == 0x6B)
	{
		for (ui = 0; ui < 6; ui++)
		{
			auiStack[ui] = mpcMemory->GetByte((uint16)(uiS + 1 + ui));
		}
		if (AsmW65C816IsIndirectCall(uiAddress, auiStack))
		{
			//S before its phk, its return address the three bytes it pushed.
			AddCall(uiS + 6, uiS + 4, uiAddress + 1);
		}
	}
	return uiOpcode;
}


//////////////////////////////////////////////////////////////////////////
//
// A call starting: S before it, the lowest byte of its return address,
// the instruction that is.  The stack's top is S at the first call (the
// start up's, once it has set S).
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::AddCall(uint16 uiS, uint16 uiReturn, uint32 uiReturnTo)
{
	SAsmW65C816FrameCall*	psCall;

	if (masCalls.NumElements() == 0)
	{
		muiStackTop = uiS;
	}
	psCall = masCalls.Add();
	psCall->uiS = uiS;
	psCall->uiD = mpcFast->GetD();
	psCall->uiReturn = uiReturn;
	psCall->uiReturnTo = uiReturnTo;
}


//////////////////////////////////////////////////////////////////////////
//
// After the instruction (uiOpcode, BeforeStep's) ran: a return ends the
// calls it returns from, those that started at or below S now, and the
// one it returned to the caller of (a subroutine that moves S, as a frame
// entry helper's jsr does, comes back with S below where it started).
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::AfterStep(uint8 uiOpcode)
{
	if (!AsmW65C816IsReturn(uiOpcode))
	{
		return;
	}
	if ((masCalls.NumElements() > 0) && (masCalls.Tail()->uiReturnTo == mpcFast->GetProgramAddress()))
	{
		masCalls.RemoveTail();
	}
	while ((masCalls.NumElements() > 0) && (masCalls.Tail()->uiS <= mpcFast->GetS()))
	{
		masCalls.RemoveTail();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The lowest byte of the stack the function running may not store to: its
// caller's frame (from the caller's D + 1, when that D is a frame on the
// stack) or, if lower, its caller's return address.  Past the stack's top
// when it has neither (main, called by the start up).
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsmW65C816FrameCheck::Limit(void)
{
	SAsmW65C816FrameCall*	psCall;
	uint32				uiLimit;
	uint32				uiReturn;

	psCall = masCalls.Tail();
	uiLimit = (uint32)muiStackTop + 1;
	if ((psCall->uiD >= mpcFast->GetS()) && (psCall->uiD <= muiStackTop))
	{
		uiLimit = (uint32)psCall->uiD + 1;
	}
	if (masCalls.NumElements() >= 2)
	{
		uiReturn = masCalls.Get(masCalls.NumElements() - 2)->uiReturn;
		if (uiReturn < uiLimit)
		{
			uiLimit = uiReturn;
		}
	}
	return uiLimit;
}


//////////////////////////////////////////////////////////////////////////
//
// Notes the instruction at uiAddress when it stores through the direct
// page to its caller's stack (Limit on) by a function that has moved D off
// its caller's.
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::Check(uint32 uiAddress, uint8 uiOpcode)
{
	SAsmW65C816DirectPageWrite	sWrite;
	uint32						uiLimit;
	uint32						uiFirst;
	uint32						uiLast;
	uint16						uiD;
	bool						bWide;
	char						sz[16];
	SAsmW65C816FrameWrite*		psWrite;

	if ((masCalls.NumElements() == 0) || !AsmW65C816DirectPageWrite(uiOpcode, &sWrite))
	{
		return;
	}
	uiD = mpcFast->GetD();
	uiLimit = Limit();
	if ((uiD == masCalls.Tail()->uiD) || (uiLimit > muiStackTop))
	{
		return;
	}

	uiFirst = uiD + mpcMemory->GetByte(uiAddress + 1);
	if (sWrite.cIndex == 'X')
	{
		uiFirst += mpcFast->GetX();
	}
	else if (sWrite.cIndex == 'Y')
	{
		uiFirst += mpcFast->GetY();
	}
	uiFirst &= 0xFFFF;
	bWide = !(mpcFast->GetP() & (sWrite.bIndexWidth ? 0x10 : 0x20));
	uiLast = uiFirst + (bWide ? 1 : 0);
	if ((uiLast < uiLimit) || (uiFirst > muiStackTop))
	{
		return;
	}

	if (masWrites.NumElements() < ASM_W65C816_FRAME_CHECK_MOST)
	{
		psWrite = masWrites.Add();
		psWrite->uiAddress = uiAddress;
		psWrite->uiFirst = (uint16)uiFirst;
		psWrite->uiLimit = (uint16)uiLimit;
		psWrite->uiD = uiD;
		snprintf(sz, sizeof(sz), "0x%06X", uiAddress);
		mszWrites.Append(sz);
		AppendWrite(psWrite, &mszWrites);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The calls being run, the first (outermost) at 0.
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816FrameCheck::NumCalls(void) const
{
	return masCalls.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsmW65C816FrameCall* CAsmW65C816FrameCheck::GetCall(size uiIndex)
{
	return masCalls.Get(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The first frame of a walk (CAsm65StackWalker's, from the registers as
// they are before the next step) that isn't the call it should be: each
// caller where its call returns to, with S as it was before the call and
// its D.  The walk may stop at code the debug records don't describe;
// stopping anywhere else, its index is the walk's length.  0 if the walk
// is right.
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816FrameCheck::WrongFrame(CArrayAsm65StackFrames* pasFrames)
{
	SAsm65StackFrame*		psFrame;
	SAsmW65C816FrameCall*	psCall;
	size					uiFrames;
	size					uiCalls;
	size					ui;

	uiFrames = pasFrames->NumElements();
	uiCalls = masCalls.NumElements();
	for (ui = 1; ui < uiFrames; ui++)
	{
		psFrame = pasFrames->Get(ui);
		psCall = (ui <= uiCalls) ? masCalls.Get(uiCalls - ui) : NULL;
		if (!psCall || (psFrame->uiPC != psCall->uiReturnTo) || (psFrame->uiS != psCall->uiS) || (psFrame->uiD != psCall->uiD))
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


//////////////////////////////////////////////////////////////////////////
//
// The stores to callers' stacks kept (the first few found).
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816FrameCheck::NumWrites(void) const
{
	return masWrites.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
SAsmW65C816FrameWrite* CAsmW65C816FrameCheck::GetWrite(size uiIndex)
{
	return masWrites.Get(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The stores to callers' stacks kept, a line each (the instruction's
// address and AppendWrite), or nothing.
//
//////////////////////////////////////////////////////////////////////////
const char* CAsmW65C816FrameCheck::GetWrites(void) const
{
	return mszWrites.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// What a store to a caller's stack did, after its instruction's address
// (which a caller may name, from its map): " writes 0x..., its caller's
// stack (from 0x...; D 0x...)" and a new line.
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::AppendWrite(SAsmW65C816FrameWrite* psWrite, CChars* pszDest)
{
	char	sz[96];

	snprintf(sz, sizeof(sz), " writes 0x%04X, its caller's stack (from 0x%04X; D 0x%04X)\n", psWrite->uiFirst, psWrite->uiLimit, psWrite->uiD);
	pszDest->Append(sz);
}
