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
#include "MicroprocessorLib/FastW65C816.h"
#include "AsmW65C816Disassembler.h"
#include "AsmW65C816Opcodes.h"
#include "AsmW65C816StackWalker.h"


#define STACK_WALKER_MAX_RETURNING	32		//The most instructions Returning looks through for the rtl.


//The opcodes the walker follows.
#define OP_PHD	0x0B
#define OP_PHA	0x48
#define OP_PHX	0xDA
#define OP_PHY	0x5A
#define OP_PHP	0x08
#define OP_PHB	0x8B
#define OP_PHK	0x4B
#define OP_PEA	0xF4
#define OP_PEI	0xD4
#define OP_PER	0x62
#define OP_PLD	0x2B
#define OP_PLA	0x68
#define OP_PLX	0xFA
#define OP_PLY	0x7A
#define OP_PLP	0x28
#define OP_PLB	0xAB
#define OP_TSC	0x3B
#define OP_TCS	0x1B
#define OP_TCD	0x5B
#define OP_TSX	0xBA
#define OP_TXS	0x9A
#define OP_TAX	0xAA
#define OP_TXA	0x8A
#define OP_TAY	0xA8
#define OP_TYA	0x98
#define OP_TXY	0x9B
#define OP_TYX	0xBB
#define OP_SEC	0x38
#define OP_CLC	0x18
#define OP_SBC	0xE9
#define OP_ADC	0x69
#define OP_REP	0xC2
#define OP_SEP	0xE2
#define OP_RTL	0x6B
#define OP_RTS	0x60
#define OP_RTI	0x40


//What a register holds while Entering follows a prologue: the function's entry S less iBelow, if it is known.
struct SStackWalkerValue
{
	int32	iBelow;
	bool	bKnown;

	void	Set(int32 iBelowEntry)		{ iBelow = iBelowEntry; bKnown = true; }
	void	Forget(void)				{ bKnown = false; }
};


//////////////////////////////////////////////////////////////////////////
//
// Whether uiOpcode is one of the accumulator's group: ora, and, eor, adc,
// sta, lda, cmp and sbc (its top three bits), and bit immediate.  Every
// odd opcode but the $xB column.
//
//////////////////////////////////////////////////////////////////////////
static bool IsAccumulatorGroup(uint8 uiOpcode)
{
	return ((uiOpcode & 1) != 0) && ((uiOpcode & 0x0F) != 0x0B);
}


//////////////////////////////////////////////////////////////////////////
//
// The widths after a rep or sep (puiBytes): m (A and memory) and x (X and
// Y) 8 bits or not.
//
//////////////////////////////////////////////////////////////////////////
static void SetWidths(uint8* puiBytes, bool* pbMemory8, bool* pbIndex8)
{
	bool	bSet;

	bSet = puiBytes[0] == OP_SEP;
	if (puiBytes[1] & 0x20)
	{
		*pbMemory8 = bSet;
	}
	if (puiBytes[1] & 0x10)
	{
		*pbIndex8 = bSet;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The instruction that returns from a function got to as eEntry says.
//
//////////////////////////////////////////////////////////////////////////
static uint8 ReturnOpcode(EAsm65DebugEntry eEntry)
{
	switch (eEntry)
	{
	case ADE_Short:
		return OP_RTS;
	case ADE_Interrupt:
	case ADE_UserInterrupt:
		return OP_RTI;
	default:
		return OP_RTL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// pcFast's registers as they are, its code running in eSpace.
//
//////////////////////////////////////////////////////////////////////////
void SAsm65StackRegisters::Init(CFastW65C816* pcFast, EAsm65StackSpace eSpace)
{
	Init(pcFast->GetProgramAddress(), eSpace);
	Set(AW65DR_S, pcFast->GetS());
	Set(AW65DR_D, pcFast->GetD());
	Set(AW65DR_C, pcFast->GetC());
	Set(AW65DR_X, pcFast->GetX());
	Set(AW65DR_Y, pcFast->GetY());
	Set(AW65DR_P, pcFast->GetP());
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
EAsm65DebugProcessor CAsmW65C816StackWalker::GetProcessor(void)
{
	return ADP_W65C816;
}


//////////////////////////////////////////////////////////////////////////
//
// A function taking its frame apart: its code from psFrame's PC to its
// return (rtl, rts or rti, as it was got to) runs straight on, pulls only
// (pld, pla, plx, ply, plb) touching the stack and D, so S at its return is
// its entry S and a pld's D its caller's.  Their widths are P's, or 16
// bits in a caller (as a call returns).  False for any other, and for an
// rtl that calls through a pointer.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816StackWalker::Returning(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller)
{
	uint8	auiBytes[4];
	uint32				uiAddress;
	uint16				uiS;
	uint16				uiD;
	size				uiLength;
	size				ui;
	bool				bMemory8;
	bool				bIndex8;
	EAsm65StackSpace	eSpace;
	EAsm65DebugEntry	eEntry;

	eSpace = (EAsm65StackSpace)psFrame->uiSpace;
	eEntry = (EAsm65DebugEntry)psFrame->psFunction->uiEntry;
	uiAddress = psFrame->uiPC;
	uiS = (uint16)psFrame->GetS();
	bMemory8 = psFrame->IsKnown(AW65DR_P) && (psFrame->Get(AW65DR_P) & 0x20);
	bIndex8 = psFrame->IsKnown(AW65DR_P) && (psFrame->Get(AW65DR_P) & 0x10);
	for (ui = 0; ui < STACK_WALKER_MAX_RETURNING; ui++)
	{
		uiLength = Instruction(eSpace, uiAddress, auiBytes, bIndex8, bMemory8);
		if (uiLength == 0)
		{
			return false;
		}

		switch (auiBytes[0])
		{
		case OP_RTL: case OP_RTS: case OP_RTI:
			if ((auiBytes[0] != ReturnOpcode(eEntry)) || ((auiBytes[0] == OP_RTL) && IsIndirectCall(eSpace, uiAddress, uiS)))
			{
				return false;
			}
			*puiEntryS = uiS;
			return true;
		case OP_PLD:
			if (!ReadWord(eSpace, uiS + 1, &uiD))
			{
				return false;
			}
			psCaller->Set(AW65DR_D, uiD);
			uiS += 2;
			break;
		case OP_PLA:
			uiS += bMemory8 ? 1 : 2;
			break;
		case OP_PLX:
		case OP_PLY:
			uiS += bIndex8 ? 1 : 2;
			break;
		case OP_PLB:
			uiS += 1;
			break;
		case OP_REP: case OP_SEP:
			SetWidths(auiBytes, &bMemory8, &bIndex8);
			break;
		case OP_PHD: case OP_PHA: case OP_PHX: case OP_PHY: case OP_PHP: case OP_PHB: case OP_PHK: case OP_PEA: case OP_PEI: case OP_PER:
		case OP_PLP: case OP_TCS: case OP_TXS: case OP_TCD:
			return false;
		default:
			if (AsmW65C816Flow(auiBytes[0]) != AW65C816F_Other)
			{
				return false;
			}
			break;
		}
		uiAddress += (uint32)uiLength;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A function that hasn't made its frame yet (or has no rule): its code
// from its start to psFrame's PC run straight through, following what S is
// below the function's entry S, and what A, X and Y are when they were
// copied from S (the entry S is found from a register once an aligned
// frame's "and" has lost S).  The caller's D is the one it pushed (phd),
// or the D running.  An indirect call (phk, per, the target pushed, rtl)
// comes back to S as it was before it.
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816StackWalker::Follow(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller)
{
	SStackWalkerValue	sS;
	SStackWalkerValue	sA;
	SStackWalkerValue	sX;
	SStackWalkerValue	sY;
	SStackWalkerValue	sPushedD;	//Where phd pushed the caller's D: its low byte is at entry S less iBelow, plus one.
	SStackWalkerValue	sCalling;	//S before an indirect call's phk, until its rtl.
	SAsmW65C816DirectPageWrite	sWrite;
	uint8				auiBytes[4];
	uint32				uiAddress;
	int32				iEntryS;
	size				uiLength;
	bool				bMemory8;
	bool				bIndex8;
	uint16				uiD;
	EAsm65StackSpace	eSpace;

	eSpace = (EAsm65StackSpace)psFrame->uiSpace;
	sS.Set(0);
	sA.Forget();
	sX.Forget();
	sY.Forget();
	sPushedD.Forget();
	sCalling.Forget();
	bMemory8 = false;
	bIndex8 = false;
	for (uiAddress = psFrame->psFunction->sStart.uiOffset; uiAddress < psFrame->uiPC; uiAddress += (uint32)uiLength)
	{
		uiLength = Instruction(eSpace, uiAddress, auiBytes, bIndex8, bMemory8);
		if (uiLength == 0)
		{
			return false;
		}

		switch (auiBytes[0])
		{
		case OP_PHD:
			sS.iBelow += 2;
			if (!sPushedD.bKnown)
			{
				sPushedD.Set(sS.iBelow);
			}
			break;
		case OP_PEA: case OP_PEI: case OP_PER:
			sS.iBelow += 2;
			break;
		case OP_PHA:
			sS.iBelow += bMemory8 ? 1 : 2;
			break;
		case OP_PHX: case OP_PHY:
			sS.iBelow += bIndex8 ? 1 : 2;
			break;
		case OP_PHK:
			//An indirect call's return bank: its rtl comes back to S as it was.
			sCalling = sS;
			sS.iBelow += 1;
			break;
		case OP_PHP: case OP_PHB:
			sS.iBelow += 1;
			break;
		case OP_RTL:
			if (sCalling.bKnown)
			{
				sS = sCalling;
				sCalling.Forget();
				sA.Forget();
				sX.Forget();
				sY.Forget();
			}
			break;
		case OP_PLD:
			sS.iBelow -= 2;
			break;
		case OP_PLA:
			sS.iBelow -= bMemory8 ? 1 : 2;
			sA.Forget();
			break;
		case OP_PLX:
			sS.iBelow -= bIndex8 ? 1 : 2;
			sX.Forget();
			break;
		case OP_PLY:
			sS.iBelow -= bIndex8 ? 1 : 2;
			sY.Forget();
			break;
		case OP_PLP: case OP_PLB:
			sS.iBelow -= 1;
			break;
		case OP_TSC: sA = sS; break;
		case OP_TSX: sX = sS; break;
		case OP_TCS: sS = sA; break;
		case OP_TXS: sS = sX; break;
		case OP_TAX: sX = sA; break;
		case OP_TXA: sA = sX; break;
		case OP_TAY: sY = sA; break;
		case OP_TYA: sA = sY; break;
		case OP_TXY: sY = sX; break;
		case OP_TYX: sX = sY; break;
		case OP_TCD: case OP_SEC: case OP_CLC:
			break;
		case OP_SBC:
			sA.iBelow += auiBytes[1] | (bMemory8 ? 0 : auiBytes[2] << 8);
			break;
		case OP_ADC:
			sA.iBelow -= auiBytes[1] | (bMemory8 ? 0 : auiBytes[2] << 8);
			break;
		case OP_REP: case OP_SEP:
			SetWidths(auiBytes, &bMemory8, &bIndex8);
			break;
		default:
			if (IsAccumulatorGroup(auiBytes[0]))
			{
				//ora, and, eor, adc, lda and sbc change A alone; sta, bit and cmp nothing.
				if (((auiBytes[0] & 0xE0) != 0x80) && ((auiBytes[0] & 0xE0) != 0xC0))
				{
					sA.Forget();
				}
			}
			else if ((AsmW65C816Flow(auiBytes[0]) == AW65C816F_Other) && !AsmW65C816DirectPageWrite(auiBytes[0], &sWrite))
			{
				//A store leaves the registers as they were; a call or a branch comes back to S as it was; anything else
				//may change any of them.
				sA.Forget();
				sX.Forget();
				sY.Forget();
			}
			break;
		}
	}

	if (sS.bKnown)
	{
		iEntryS = (int32)psFrame->GetS() + sS.iBelow;
	}
	else if (sA.bKnown && psFrame->IsKnown(AW65DR_C))
	{
		iEntryS = (int32)psFrame->Get(AW65DR_C) + sA.iBelow;
	}
	else if (sX.bKnown && psFrame->IsKnown(AW65DR_X))
	{
		iEntryS = (int32)psFrame->Get(AW65DR_X) + sX.iBelow;
	}
	else if (sY.bKnown && psFrame->IsKnown(AW65DR_Y))
	{
		iEntryS = (int32)psFrame->Get(AW65DR_Y) + sY.iBelow;
	}
	else
	{
		return false;
	}

	if (sPushedD.bKnown)
	{
		if (!ReadWord(eSpace, (uint32)(iEntryS - sPushedD.iBelow + 1), &uiD))
		{
			return false;
		}
		psCaller->Set(AW65DR_D, uiD);
	}
	*puiEntryS = (uint16)iEntryS;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// jsl pushes the return address less one, three bytes; jsr two, in the
// function's own bank; an interrupt (or cop, brk) P then the address it
// goes back to, three bytes.
//
//////////////////////////////////////////////////////////////////////////
void CAsmW65C816StackWalker::CallBytes(EAsm65DebugEntry eEntry, uint32* puiPBytes, uint32* puiReturnBytes)
{
	*puiPBytes = ((eEntry == ADE_Interrupt) || (eEntry == ADE_UserInterrupt)) ? 1 : 0;
	*puiReturnBytes = (eEntry == ADE_Short) ? 2 : 3;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a call goes on from what it pushed: the instruction after the jsl
// or jsr (in the callee's bank for jsr), or the one interrupted.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsmW65C816StackWalker::ReturnAddress(EAsm65DebugEntry eEntry, uint64 uiPushed, uint32 uiCalleePC)
{
	switch (eEntry)
	{
	case ADE_Short:
		return (uiCalleePC & 0xFF0000) | (((uint32)uiPushed + 1) & 0xFFFF);
	case ADE_Long:
		return (uint32)uiPushed + 1;
	default:
		return (uint32)uiPushed;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A call keeps D (a function that changes it pushes it first and pulls it
// before it returns).
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsmW65C816StackWalker::PreservedRegisters(void)
{
	return 1u << AW65DR_D;
}


//////////////////////////////////////////////////////////////////////////
//
// The stacks and frames are in bank 0: their addresses are 16 bits.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsmW65C816StackWalker::StackAddress(int64 iAddress)
{
	return (uint32)iAddress & 0xFFFF;
}


//////////////////////////////////////////////////////////////////////////
//
// A pointer's 24 bit address is kept in three bytes.
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816StackWalker::PointerBytes(void)
{
	return 3;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the rtl at uiAddress, S uiS, goes to a function called through
// a pointer (AsmW65C816IsIndirectCall).
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816StackWalker::IsIndirectCall(EAsm65StackSpace eSpace, uint32 uiAddress, uint16 uiS)
{
	uint8	auiStack[6];

	return mpcMemory->Read(eSpace, uiS + 1, auiStack, 6) && AsmW65C816IsIndirectCall(uiAddress, auiStack);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsmW65C816StackWalker::ReadWord(EAsm65StackSpace eSpace, uint32 uiAddress, uint16* puiWord)
{
	uint8	auiBytes[2];

	if (!mpcMemory->Read(eSpace, uiAddress, auiBytes, 2))
	{
		return false;
	}
	*puiWord = auiBytes[0] | (auiBytes[1] << 8);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the instruction at uiAddress in eSpace into puiBytes (its opcode
// and up to three operand bytes) with those widths.  Returns its length, 0
// if it can't be read.
//
//////////////////////////////////////////////////////////////////////////
size CAsmW65C816StackWalker::Instruction(EAsm65StackSpace eSpace, uint32 uiAddress, uint8* puiBytes, bool bIndex8, bool bMemory8)
{
	SAsmW65C816Widths	sWidths;
	size				uiLength;

	if (!mpcMemory->Read(eSpace, uiAddress, puiBytes, 1))
	{
		return 0;
	}
	sWidths.bMemory8 = bMemory8;
	sWidths.bIndex8 = bIndex8;
	uiLength = AsmW65C816InstructionLength(puiBytes[0], &sWidths);
	if ((uiLength > 1) && !mpcMemory->Read(eSpace, uiAddress + 1, &puiBytes[1], uiLength - 1))
	{
		return 0;
	}
	return uiLength;
}

