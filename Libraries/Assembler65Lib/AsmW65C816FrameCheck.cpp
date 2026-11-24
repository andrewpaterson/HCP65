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
	CAsm65FrameCheck::Init();
	mpcMemory = pcMemory;
	mpcFast = pcFast;
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
	CAsm65FrameCheck::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// One instruction run, checked (Before and AfterStep).
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::Step(void)
{
	uint8	uiOpcode;

	uiOpcode = BeforeStep();
	mpcFast->Step();
	AfterStep(uiOpcode);
}


//////////////////////////////////////////////////////////////////////////
//
// The program ran to stp.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816FrameCheck::IsStopped(void)
{
	return mpcFast->IsStopped();
}


//////////////////////////////////////////////////////////////////////////
//
// Calls keep D.
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816FrameCheck::KeptRegister(void)
{
	return AW65DR_D;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CAsmW65C816FrameCheck::Kept(void)
{
	return mpcFast->GetD();
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
		AddCall(uiS, (uint16)(uiS - ((uiOpcode == 0x22) ? 2 : 1)), uiAddress + ((uiOpcode == 0x22) ? 4 : 3));
	}
	else if ((uiOpcode == 0x02) || (uiOpcode == 0x00))
	{
		//The program bank, where it comes back to (after the signature byte), then P.
		AddCall(uiS, (uint16)(uiS - 2), uiAddress + 2);
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
			AddCall((uint16)(uiS + 6), (uint16)(uiS + 4), uiAddress + 1);
		}
	}
	return uiOpcode;
}


//////////////////////////////////////////////////////////////////////////
//
// After the instruction (uiOpcode, BeforeStep's) ran: a return ends the
// calls it returns from (Returned).
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816FrameCheck::AfterStep(uint8 uiOpcode)
{
	if (AsmW65C816IsReturn(uiOpcode))
	{
		Returned(mpcFast->GetProgramAddress(), mpcFast->GetS());
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
	SAsm65FrameCall*	psCall;
	uint32				uiLimit;
	uint32				uiReturn;
	uint16				uiD;

	psCall = masCalls.Tail();
	uiD = (uint16)psCall->uiKept;
	uiLimit = muiStackTop + 1;
	if ((uiD >= mpcFast->GetS()) && (uiD <= muiStackTop))
	{
		uiLimit = (uint32)uiD + 1;
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
	if ((uiD == masCalls.Tail()->uiKept) || (uiLimit > muiStackTop))
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
