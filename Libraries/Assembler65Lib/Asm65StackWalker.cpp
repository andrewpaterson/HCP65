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
#include "Asm65StackWalker.h"


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
	uiPC = pcFast->GetProgramAddress();
	uiS = pcFast->GetS();
	uiD = pcFast->GetD();
	uiC = pcFast->GetC();
	uiX = pcFast->GetX();
	uiY = pcFast->GetY();
	uiP = pcFast->GetP();
	uiSpace = (uint8)eSpace;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65StackWalker::Init(CAsm65StackMemory* pcMemory)
{
	masRecords.Init();
	mpcMemory = pcMemory;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CAsm65StackWalker::Kill(void)
{
	masRecords.Kill();
	mpcMemory = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A loaded program's records, at its addresses (linked, Sort'ed) in
// eSpace: the kernel's, or the process's in user mode.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65StackWalker::AddRecords(CAsm65DebugRecords* pcRecords, EAsm65StackSpace eSpace)
{
	SAsm65StackRecords*	psRecords;

	psRecords = masRecords.Add();
	psRecords->pcRecords = pcRecords;
	psRecords->eSpace = eSpace;
}


//////////////////////////////////////////////////////////////////////////
//
// The calls the W65C816 is in, innermost first, up to uiMaxFrames: the
// function running, then each caller while its callee's is found.  The
// last has no function (psFunction NULL) where the walk reached code the
// records don't describe.  A caller on the same stack is above its
// callee; an interrupt from user mode goes over to the process's stack.
// Returns how many.
//
//////////////////////////////////////////////////////////////////////////
size CAsm65StackWalker::Walk(SAsm65StackRegisters* psRegisters, CArrayAsm65StackFrames* pasFrames, size uiMaxFrames)
{
	SAsm65StackFrame	sFrame;
	SAsm65StackFrame	sCaller;

	pasFrames->ReInit();
	if (uiMaxFrames == 0)
	{
		return 0;
	}

	Find(psRegisters->uiPC, (EAsm65StackSpace)psRegisters->uiSpace, &sFrame);
	sFrame.uiS = psRegisters->uiS;
	sFrame.uiD = psRegisters->uiD;
	pasFrames->Add(&sFrame);
	while ((pasFrames->NumElements() < uiMaxFrames) && sFrame.psFunction)
	{
		//Only the first frame's registers are known.
		if (!Caller(&sFrame, (pasFrames->NumElements() == 1) ? psRegisters : NULL, &sCaller) || ((sCaller.uiSpace == sFrame.uiSpace) && (sCaller.uiS <= sFrame.uiS)))
		{
			break;
		}
		pasFrames->Add(&sCaller);
		sFrame = sCaller;
	}
	return pasFrames->NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// Where psVariable, one of psFrame's function's, is while it runs, in
// psFrame's space: in its frame, or where the address kept there points
// (the low 24 bits of a pointer).  False if it isn't kept anywhere, or the
// address can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Locate(SAsm65StackFrame* psFrame, SAsm65DebugVariable* psVariable, uint32* puiAddress)
{
	uint8	auiPointer[3];
	uint16	uiPlace;

	uiPlace = (uint16)(psFrame->uiD + psVariable->iOffset);
	switch (psVariable->uiWhere)
	{
	case ADW_Frame:
		*puiAddress = uiPlace;
		return true;
	case ADW_Indirect:
		if (!mpcMemory->Read((EAsm65StackSpace)psFrame->uiSpace, uiPlace, auiPointer, 3))
		{
			return false;
		}
		*puiAddress = auiPointer[0] | (auiPointer[1] << 8) | (auiPointer[2] << 16);
		return true;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// psFrame's function and records for code at uiPC in eSpace; false
// (NULL) if no records describe it.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Find(uint32 uiPC, EAsm65StackSpace eSpace, SAsm65StackFrame* psFrame)
{
	size				ui;
	SAsm65StackRecords*	psRecords;

	psFrame->uiPC = uiPC;
	psFrame->uiSpace = (uint8)eSpace;
	for (ui = 0; ui < masRecords.NumElements(); ui++)
	{
		psRecords = masRecords.Get(ui);
		psFrame->pcRecords = psRecords->pcRecords;
		psFrame->psFunction = (psRecords->eSpace == eSpace) ? psFrame->pcRecords->FindFunction(uiPC) : NULL;
		if (psFrame->psFunction)
		{
			return true;
		}
	}
	psFrame->pcRecords = NULL;
	psFrame->psFunction = NULL;
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// psFrame's caller: from the instructions from its start if it hasn't
// made its frame yet (or makes none), from the instructions to its rtl if
// it has given its caller's D back, and from its frame's link otherwise.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Caller(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psRegisters, SAsm65StackFrame* psCaller)
{
	SAsm65DebugFunction*	psFunction;

	psFunction = psFrame->psFunction;
	if ((psFrame->uiPC < psFunction->uiBody) || (psFunction->uiLink == ADL_None))
	{
		return Entering(psFrame, psRegisters, psCaller);
	}
	if (Returning(psFrame, psRegisters, psCaller))
	{
		return true;
	}
	return Linked(psFrame, psCaller);
}


//////////////////////////////////////////////////////////////////////////
//
// The caller of a function taking its frame apart: its code from
// psFrame's PC to its return (rtl, rts or rti, as it was got to) runs
// straight on, pulls only (pld, pla, plx, ply, plb) touching the stack and
// D.  Their widths are P's, or 16 bits in a caller (as a call returns).
// False for any other, and for an rtl that calls through a pointer.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Returning(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psRegisters, SAsm65StackFrame* psCaller)
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
	uiS = psFrame->uiS;
	uiD = psFrame->uiD;
	bMemory8 = psRegisters && (psRegisters->uiP & 0x20);
	bIndex8 = psRegisters && (psRegisters->uiP & 0x10);
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
			return ReturnTo(psFrame, uiS, uiD, eEntry, eSpace, psCaller);
		case OP_PLD:
			if (!ReadWord(eSpace, uiS + 1, &uiD))
			{
				return false;
			}
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
// The caller of a function that hasn't made its frame yet (or makes none):
// its code from its start to psFrame's PC run straight through, following
// what S is below the function's entry S, and what A, X and Y are when
// they were copied from S (the entry S is found from a register once an
// aligned frame's "and" has lost S).  The caller's D is the one it pushed
// (phd), or the D running.  An indirect call (phk, per, the target pushed,
// rtl) comes back to S as it was before it.  An interrupt from user mode
// (ADL_User) came from the process's stack: its caller is the process's.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Entering(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psRegisters, SAsm65StackFrame* psCaller)
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
	EAsm65StackSpace	eCallerSpace;

	eSpace = (EAsm65StackSpace)psFrame->uiSpace;
	eCallerSpace = (psFrame->psFunction->uiLink == ADL_User) ? ASS_User : eSpace;
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
		iEntryS = psFrame->uiS + sS.iBelow;
	}
	else if (psRegisters && sA.bKnown)
	{
		iEntryS = psRegisters->uiC + sA.iBelow;
	}
	else if (psRegisters && sX.bKnown)
	{
		iEntryS = psRegisters->uiX + sX.iBelow;
	}
	else if (psRegisters && sY.bKnown)
	{
		iEntryS = psRegisters->uiY + sY.iBelow;
	}
	else
	{
		return false;
	}

	uiD = psFrame->uiD;
	if (sPushedD.bKnown && !ReadWord(eSpace, (uint32)(iEntryS - sPushedD.iBelow + 1), &uiD))
	{
		return false;
	}
	return ReturnTo(psFrame, (uint16)iEntryS, uiD, (EAsm65DebugEntry)psFrame->psFunction->uiEntry, eCallerSpace, psCaller);
}


//////////////////////////////////////////////////////////////////////////
//
// The caller of a function running on its own frame: S as it was after
// phd is D plus its link (ADL_Frame) or kept there (ADL_SavedS), the
// caller's D above it.  An interrupt from user mode (ADL_User) keeps the
// process's D and S above S plus its link, the interrupt's frame on the
// process's stack.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Linked(SAsm65StackFrame* psFrame, SAsm65StackFrame* psCaller)
{
	SAsm65DebugFunction*	psFunction;
	EAsm65StackSpace		eSpace;
	uint16					uiPushedD;
	uint16					uiCallerD;
	uint16					uiCallerS;

	psFunction = psFrame->psFunction;
	eSpace = (EAsm65StackSpace)psFrame->uiSpace;
	if (psFunction->uiLink == ADL_User)
	{
		uiPushedD = (uint16)(psFrame->uiS + psFunction->iLink);
		return ReadWord(eSpace, uiPushedD + 1, &uiCallerD) && ReadWord(eSpace, uiPushedD + 3, &uiCallerS) &&
			   ReturnTo(psFrame, uiCallerS, uiCallerD, ADE_Interrupt, ASS_User, psCaller);
	}

	uiPushedD = (uint16)(psFrame->uiD + psFunction->iLink);
	if ((psFunction->uiLink == ADL_SavedS) && !ReadWord(eSpace, uiPushedD, &uiPushedD))
	{
		return false;
	}
	return ReadWord(eSpace, uiPushedD + 1, &uiCallerD) && ReturnTo(psFrame, uiPushedD + 2, uiCallerD, (EAsm65DebugEntry)psFunction->uiEntry, eSpace, psCaller);
}


//////////////////////////////////////////////////////////////////////////
//
// psFrame's caller, in eSpace, whose call (eEntry: jsl, jsr or an
// interrupt) pushed where it goes back to above uiEntryS (S as psFrame's
// function was entered): it runs on from the instruction after the jsl
// or jsr, or the one interrupted, with S as it was before it and D uiD.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::ReturnTo(SAsm65StackFrame* psFrame, uint16 uiEntryS, uint16 uiD, EAsm65DebugEntry eEntry, EAsm65StackSpace eSpace, SAsm65StackFrame* psCaller)
{
	uint8	auiReturn[3];
	uint32	uiSkip;
	uint32	uiBytes;
	uint32	uiReturn;

	//An interrupt pushed P below where it goes back to; jsr only the address in the bank.
	uiSkip = (eEntry == ADE_Interrupt) ? 1 : 0;
	uiBytes = (eEntry == ADE_Short) ? 2 : 3;
	auiReturn[2] = (uint8)(psFrame->uiPC >> 16);
	if (((uint32)uiEntryS + uiSkip + uiBytes > 0xFFFF) || !mpcMemory->Read(eSpace, uiEntryS + 1 + uiSkip, auiReturn, uiBytes))
	{
		return false;
	}
	uiReturn = auiReturn[0] | (auiReturn[1] << 8) | (auiReturn[2] << 16);
	if (eEntry == ADE_Short)
	{
		uiReturn = (uiReturn & 0xFF0000) | ((uiReturn + 1) & 0xFFFF);
	}
	else if (eEntry == ADE_Long)
	{
		uiReturn++;
	}
	Find(uiReturn, eSpace, psCaller);
	psCaller->uiS = (uint16)(uiEntryS + uiSkip + uiBytes);
	psCaller->uiD = uiD;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the rtl at uiAddress, S uiS, goes to a function called through
// a pointer (AsmW65C816IsIndirectCall).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::IsIndirectCall(EAsm65StackSpace eSpace, uint32 uiAddress, uint16 uiS)
{
	uint8	auiStack[6];

	return mpcMemory->Read(eSpace, uiS + 1, auiStack, 6) && AsmW65C816IsIndirectCall(uiAddress, auiStack);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::ReadWord(EAsm65StackSpace eSpace, uint32 uiAddress, uint16* puiWord)
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
size CAsm65StackWalker::Instruction(EAsm65StackSpace eSpace, uint32 uiAddress, uint8* puiBytes, bool bIndex8, bool bMemory8)
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

