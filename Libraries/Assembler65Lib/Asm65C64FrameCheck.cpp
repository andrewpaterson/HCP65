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
#include "MicroprocessorLib/65C64Decoded.h"
#include "Asm65C64FrameCheck.h"


//////////////////////////////////////////////////////////////////////////
//
// pcFast has run reset on pcMemory.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65C64FrameCheck::Init(CAsm65Memory* pcMemory, CFast65C64* pcFast)
{
	CAsm65FrameCheck::Init();
	mpcMemory = pcMemory;
	mpcFast = pcFast;
}


//////////////////////////////////////////////////////////////////////////
//
// Before the instruction at the program address runs: jsr, brsr and brk
// start a call, S before it, the 4 byte address it goes back to (the next
// instruction) ending at S.  Returns its opcode, for AfterStep.
//
//////////////////////////////////////////////////////////////////////////
uint16 CAsm65C64FrameCheck::BeforeStep(void)
{
	S65C64Decoded	sDecoded;
	uint32			uiAddress;
	uint32			uiS;
	uint16			uiOpcode;

	uiAddress = mpcFast->GetProgramCounter();
	uiOpcode = (uint16)(mpcMemory->GetByte(uiAddress) | (mpcMemory->GetByte(uiAddress + 1) << 8));
	Decode65C64(uiOpcode, &sDecoded);
	if ((sDecoded.eInstruction == I65_Jsr) || (sDecoded.eInstruction == I65_Brsr) || (sDecoded.eInstruction == I65_Brk))
	{
		uiS = mpcFast->GetS();
		AddCall(uiS, uiS - 3, uiAddress + (uint32)CFast65C64::InstructionLength(uiOpcode));
	}
	return uiOpcode;
}


//////////////////////////////////////////////////////////////////////////
//
// After the instruction (uiOpcode, BeforeStep's) ran: a ret or rti ends
// the calls it returns from (Returned).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65C64FrameCheck::AfterStep(uint16 uiOpcode)
{
	if (CFast65C64::IsReturn(uiOpcode))
	{
		Returned(mpcFast->GetProgramCounter(), mpcFast->GetS());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One instruction run, followed (Before and AfterStep).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65C64FrameCheck::Step(void)
{
	uint16	uiOpcode;

	uiOpcode = BeforeStep();
	mpcFast->Step();
	AfterStep(uiOpcode);
}


//////////////////////////////////////////////////////////////////////////
//
// The processor's registers as they are (its code the kernel's space's).
//
//////////////////////////////////////////////////////////////////////////
void CAsm65C64FrameCheck::GetRegisters(SAsm65StackRegisters* psRegisters)
{
	psRegisters->Init(mpcFast, ASS_Kernel);
}


//////////////////////////////////////////////////////////////////////////
//
// The program ran to stp.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65C64FrameCheck::IsStopped(void)
{
	return mpcFast->IsStopped();
}


//////////////////////////////////////////////////////////////////////////
//
// Calls keep V.
//
//////////////////////////////////////////////////////////////////////////
size CAsm65C64FrameCheck::KeptRegister(void)
{
	return A65DR_V;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
uint64 CAsm65C64FrameCheck::Kept(void)
{
	return mpcFast->GetV();
}

