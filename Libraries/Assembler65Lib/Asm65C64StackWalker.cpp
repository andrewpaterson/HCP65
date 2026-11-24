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
#include "MicroprocessorLib/65C64.h"
#include "MicroprocessorLib/Fast65C64.h"
#include "Asm65C64StackWalker.h"


#define STACK_WALKER_65C64_MAX_RETURNING	32		//The most instructions Returning looks through for the ret.


//The bytes of each width code in P.
static const uint8	gauiStackWalker65C64Widths[4] = { 1, 2, 4, 8 };


//////////////////////////////////////////////////////////////////////////
//
// The debug records' register (EAsm65C64DebugRegister) for a 65C64
// register, -1 for the zero register.
//
//////////////////////////////////////////////////////////////////////////
static int DebugRegister(E65Register eRegister)
{
	switch (eRegister)
	{
	case R65_A:	return A65DR_A;
	case R65_B:	return A65DR_B;
	case R65_S:	return A65DR_S;
	case R65_V:	return A65DR_V;
	case R65_W:	return A65DR_W;
	case R65_X:	return A65DR_X;
	case R65_Y:	return A65DR_Y;
	default:	return -1;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// An instruction's second operand as a number: an immediate (sign
// extended when the instruction is signed) or an embedded value.  False
// for any other operand.
//
//////////////////////////////////////////////////////////////////////////
static bool OperandValue(S65C64Decoded* psDecoded, uint64 uiImmediate, int64* piValue)
{
	uint32	uiBits;

	if (psDecoded->sSecond.eKind == OK65_Embedded)
	{
		*piValue = psDecoded->sSecond.iEmbedded;
		return true;
	}
	if (psDecoded->sSecond.eKind != OK65_Immediate)
	{
		return false;
	}
	uiBits = 8 * (uint32)psDecoded->sSecond.uiImmediateBytes;
	if (psDecoded->bSigned && (uiBits < 64) && ((uiImmediate >> (uiBits - 1)) & 1))
	{
		uiImmediate |= ~0ULL << uiBits;
	}
	*piValue = (int64)uiImmediate;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the instruction (that goes on at uiNext) branches: bra, bal or
// jmp always (pbAlways), the others when their flag says.  Its target, or
// 0 when it is a register's or memory's.
//
//////////////////////////////////////////////////////////////////////////
static bool BranchTarget(S65C64Decoded* psDecoded, uint32 uiNext, uint64 uiImmediate, uint32* puiTarget, bool* pbAlways)
{
	int64	iOffset;
	uint32	uiBits;

	switch (psDecoded->eInstruction)
	{
	case I65_Bra: case I65_Bal: case I65_Jmp:
		*pbAlways = true;
		break;
	case I65_Bras: case I65_Brac: case I65_Brasa: case I65_Braso: case I65_Brae: case I65_Brane:
		*pbAlways = false;
		break;
	default:
		return false;
	}

	*puiTarget = 0;
	if (psDecoded->sSecond.eKind == OK65_Embedded)
	{
		*puiTarget = uiNext + (uint32)psDecoded->sSecond.iEmbedded;
	}
	else if (psDecoded->sSecond.eKind == OK65_Immediate)
	{
		if ((psDecoded->eInstruction == I65_Jmp) && !psDecoded->bRelative)
		{
			*puiTarget = (uint32)uiImmediate;
		}
		else
		{
			uiBits = 8 * (uint32)psDecoded->sSecond.uiImmediateBytes;
			iOffset = (int64)uiImmediate;
			if ((uiBits < 64) && ((uiImmediate >> (uiBits - 1)) & 1))
			{
				iOffset = (int64)(uiImmediate | (~0ULL << uiBits));
			}
			*puiTarget = uiNext + (uint32)iOffset;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The state a forward branch left for uiAddress, NULL if none goes there.
//
//////////////////////////////////////////////////////////////////////////
static SAsm65C64StackBranch* FindBranch(CArrayTemplate<SAsm65C64StackBranch>* pasBranches, uint32 uiAddress)
{
	size	ui;

	for (ui = 0; ui < pasBranches->NumElements(); ui++)
	{
		if (pasBranches->Get(ui)->uiTarget == uiAddress)
		{
			return pasBranches->Get(ui);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// ret and rti end the code a function runs straight on through: what
// follows is got to from its body, with the stack as it was before that
// exit's pulls.
//
//////////////////////////////////////////////////////////////////////////
static bool IsExit(S65C64Decoded* psDecoded)
{
	return (psDecoded->eInstruction == I65_Ret) || (psDecoded->eInstruction == I65_Rti) || (psDecoded->eInstruction == I65_Pul);
}


//////////////////////////////////////////////////////////////////////////
//
// An exit's own instructions before its ret: pulls from S, add S, cmp
// (the carry an exit with exceptions leaves) and the widths it sets back.
//
//////////////////////////////////////////////////////////////////////////
static bool IsUnwinding(S65C64Decoded* psDecoded)
{
	switch (psDecoded->eInstruction)
	{
	case I65_Wth:
	case I65_Wtth:
	case I65_Wthx:
		return true;
	case I65_PulRegisters:
	case I65_PulOperand:
		return psDecoded->eStack == R65_S;
	case I65_Add:
	case I65_Cmp:
		return (psDecoded->sFirst.eKind == OK65_Register) && (psDecoded->sFirst.eRegister == R65_S);
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// pcFast's registers as they are, its code running in eSpace.
//
//////////////////////////////////////////////////////////////////////////
void SAsm65StackRegisters::Init(CFast65C64* pcFast, EAsm65StackSpace eSpace)
{
	Init(pcFast->GetProgramCounter(), eSpace);
	Set(A65DR_S, pcFast->GetS());
	Set(A65DR_V, pcFast->GetV());
	Set(A65DR_W, pcFast->GetW());
	Set(A65DR_A, pcFast->GetA());
	Set(A65DR_B, pcFast->GetB());
	Set(A65DR_X, pcFast->GetX());
	Set(A65DR_Y, pcFast->GetY());
	Set(A65DR_P, pcFast->GetP());
}


//////////////////////////////////////////////////////////////////////////
//
// At a function's start: S is the entry S, nothing else is known; A, B, X
// and Y are P's widths (bP), or 32 bits (as the back end leaves them
// round a narrow push).
//
//////////////////////////////////////////////////////////////////////////
void SAsm65C64StackState::Init(uint16 uiP, bool bP)
{
	ForgetAll();
	asRegisters[R65_S].Set(0);
	auiBytes[R65_Zero] = 0;
	auiBytes[R65_S] = 4;
	auiBytes[R65_V] = 4;
	auiBytes[R65_W] = 4;
	auiBytes[R65_A] = bP ? gauiStackWalker65C64Widths[(uiP >> STATUS65_WIDTH_A_SHIFT) & 3] : 4;
	auiBytes[R65_B] = bP ? gauiStackWalker65C64Widths[(uiP >> STATUS65_WIDTH_B_SHIFT) & 3] : 4;
	auiBytes[R65_X] = bP ? gauiStackWalker65C64Widths[(uiP >> STATUS65_WIDTH_X_SHIFT) & 3] : 4;
	auiBytes[R65_Y] = bP ? gauiStackWalker65C64Widths[(uiP >> STATUS65_WIDTH_Y_SHIFT) & 3] : 4;
	sPushedV.Forget();
	bCallerV = true;
}


//////////////////////////////////////////////////////////////////////////
//
// Every register but S may hold anything.
//
//////////////////////////////////////////////////////////////////////////
void SAsm65C64StackState::ForgetAll(void)
{
	int	i;

	for (i = 0; i < 8; i++)
	{
		if (i != R65_S)
		{
			asRegisters[i].Forget();
		}
	}
	bCallerV = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The bytes eRegister pushes and pulls: 0 if its width isn't known.
//
//////////////////////////////////////////////////////////////////////////
uint8 SAsm65C64StackState::Bytes(E65Register eRegister)
{
	return auiBytes[eRegister];
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
EAsm65DebugProcessor CAsm65C64StackWalker::GetProcessor(void)
{
	return ADP_65C64;
}


//////////////////////////////////////////////////////////////////////////
//
// The instruction at uiAddress in eSpace, decoded, and its operand words
// as a number (its immediate).  Returns its length, 0 if it can't be
// read.
//
//////////////////////////////////////////////////////////////////////////
size CAsm65C64StackWalker::Instruction(EAsm65StackSpace eSpace, uint32 uiAddress, S65C64Decoded* psDecoded, uint64* puiImmediate)
{
	uint8	auiBytes[2 + 2 * MAX_65C64_OPERAND_WORDS];
	size	uiLength;
	size	ui;

	if (!mpcMemory->Read(eSpace, uiAddress, auiBytes, 2))
	{
		return 0;
	}
	Decode65C64((uint16)(auiBytes[0] | (auiBytes[1] << 8)), psDecoded);
	if ((psDecoded->eInstruction == I65_Illegal) || (psDecoded->uiOperandWords > MAX_65C64_OPERAND_WORDS))
	{
		return 0;
	}
	uiLength = 2 + 2 * (size)psDecoded->uiOperandWords;
	if ((uiLength > 2) && !mpcMemory->Read(eSpace, uiAddress + 2, &auiBytes[2], uiLength - 2))
	{
		return 0;
	}
	*puiImmediate = 0;
	for (ui = 2; (ui < uiLength) && (ui < 10); ui++)
	{
		*puiImmediate |= (uint64)auiBytes[ui] << (8 * (ui - 2));
	}
	if ((psDecoded->sSecond.eKind == OK65_Immediate) && (psDecoded->sSecond.uiImmediateBytes < 8))
	{
		*puiImmediate &= (1ULL << (8 * psDecoded->sSecond.uiImmediateBytes)) - 1;
	}
	return uiLength;
}


//////////////////////////////////////////////////////////////////////////
//
// iBytes pushed on (pulled from, negative) the stack register eStack.
// False if they aren't known (0) and the stack is S.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65C64StackWalker::Stack(SAsm65C64StackState* psState, E65Register eStack, int64 iBytes)
{
	if (iBytes == 0)
	{
		if (eStack == R65_S)
		{
			return false;
		}
		psState->asRegisters[eStack].Forget();
		return true;
	}
	psState->asRegisters[eStack].iBelow += iBytes;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What an instruction does to the registers a walk follows: the stack
// registers' pushes and pulls, add and sub of a constant, cpy of one
// register to another, the widths wth sets, and where psh V put the
// caller's V.  A call (jsr, brsr, brk) comes back with S and V as they
// were, and mul and div keep them too; any other instruction may change
// the register it writes, or any but S.  False where S can't be followed (a register of unknown width
// pushed or pulled).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65C64StackWalker::Step(SAsm65C64StackState* psState, S65C64Decoded* psDecoded, uint64 uiImmediate)
{
	SAsm65C64StackValue*	psFirst;
	E65Register				eRegister;
	E65Register				eStack;
	int64					iValue;
	int						i;

	eStack = psDecoded->eStack;
	psFirst = (psDecoded->sFirst.eKind == OK65_Register) ? &psState->asRegisters[psDecoded->sFirst.eRegister] : NULL;
	switch (psDecoded->eInstruction)
	{
	case I65_PshRegisters:
		for (i = 0; i < 7; i++)
		{
			if (psDecoded->uiMask & (1 << i))
			{
				eRegister = (E65Register)(R65_A + i);
				if ((eRegister == R65_V) && psState->bCallerV && !psState->sPushedV.bKnown && (eStack == R65_S) && psState->asRegisters[R65_S].bKnown)
				{
					psState->sPushedV.Set(psState->asRegisters[R65_S].iBelow + 3);
				}
				if (!Stack(psState, eStack, psState->Bytes(eRegister)))
				{
					return false;
				}
			}
		}
		return true;

	case I65_PulRegisters:
		for (i = 6; i >= 0; i--)
		{
			if (psDecoded->uiMask & (1 << i))
			{
				eRegister = (E65Register)(R65_A + i);
				if (!Stack(psState, eStack, -(int64)psState->Bytes(eRegister)))
				{
					return false;
				}
				psState->asRegisters[eRegister].Forget();
				if (eRegister == R65_V)
				{
					psState->bCallerV = false;
				}
			}
		}
		return true;

	case I65_PshOperand:
		return Stack(psState, eStack, (psDecoded->sSecond.eKind == OK65_Immediate) ? psDecoded->sSecond.uiImmediateBytes : 4);

	case I65_PulOperand:
		return Stack(psState, eStack, -4);

	case I65_Psh:
		return Stack(psState, eStack, (psDecoded->bPushC ? 4 : 0) + (psDecoded->bPushP ? 2 : 0));

	case I65_Jsr:
	case I65_Brsr:
	case I65_Brk:
	case I65_Mul:
	case I65_Div:
		//A multiply or divide step changes A, B, W and Y alone.
		psState->asRegisters[R65_A].Forget();
		psState->asRegisters[R65_B].Forget();
		psState->asRegisters[R65_W].Forget();
		psState->asRegisters[R65_X].Forget();
		psState->asRegisters[R65_Y].Forget();
		return true;

	case I65_Wth:
	case I65_Wtth:
	case I65_Wthx:
		psState->auiBytes[psDecoded->eWidthFirst] = (psDecoded->sSecond.eKind == OK65_Register) ? 0 : psDecoded->uiWidthBytes;
		if (psDecoded->eInstruction == I65_Wtth)
		{
			psState->auiBytes[psDecoded->eWidthSecond] = psState->auiBytes[psDecoded->eWidthFirst];
		}
		if (psDecoded->eInstruction == I65_Wthx)
		{
			psState->asRegisters[psDecoded->eWidthFirst].Forget();
		}
		psState->auiBytes[R65_S] = 4;
		psState->auiBytes[R65_V] = 4;
		psState->auiBytes[R65_W] = 4;
		return true;

	case I65_Ret: case I65_Rti: case I65_Pul:
	case I65_Jmp: case I65_Bra: case I65_Bal:
	case I65_Bras: case I65_Brac: case I65_Brasa: case I65_Braso: case I65_Brae: case I65_Brane:
	case I65_Cmp: case I65_Tst: case I65_Nop: case I65_Sep: case I65_Cep:
		return true;

	case I65_Cpy:
		if (psFirst && (psDecoded->sSecond.eKind == OK65_Register) && (psDecoded->sSecond.eRegister != R65_Zero))
		{
			*psFirst = psState->asRegisters[psDecoded->sSecond.eRegister];
			break;
		}
		if (psFirst)
		{
			psFirst->Forget();
		}
		break;

	case I65_Add:
	case I65_Sub:
		if (psFirst && psFirst->bKnown && OperandValue(psDecoded, uiImmediate, &iValue))
		{
			psFirst->iBelow += (psDecoded->eInstruction == I65_Sub) ? iValue : -iValue;
			break;
		}
		if (psFirst)
		{
			psFirst->Forget();
		}
		break;

	case I65_And: case I65_Or: case I65_Xor: case I65_Shl: case I65_Shr: case I65_Cpyx: case I65_Neg: case I65_Abs: case I65_Gwth:
		//A store to memory changes no register.
		if (psFirst)
		{
			psFirst->Forget();
		}
		break;

	default:
		if (psFirst)
		{
			psFirst->Forget();
		}
		else
		{
			psState->ForgetAll();
		}
		break;
	}
	if (psFirst == &psState->asRegisters[R65_V])
	{
		psState->bCallerV = false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A function that hasn't made its frame yet (or has no rule): its code
// from its start to psFrame's PC, followed (Step), tells how far S is
// below its entry S, or a register copied from S is.  The code after a
// bra, bal or jmp goes on as the forward branches to it left it (A's
// width may differ from path to path); after a ret as the stack was before
// that exit's pulls.  The caller's V is where psh V pushed it, or V if the
// function hasn't changed it.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65C64StackWalker::Follow(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller)
{
	CArrayTemplate<SAsm65C64StackBranch>	asBranches;
	SAsm65C64StackBranch*	psBranch;
	SAsm65C64StackState		sState;
	SAsm65C64StackState		sBeforeExit;
	S65C64Decoded			sDecoded;
	uint64					uiImmediate;
	uint64					uiV;
	uint32					uiAddress;
	uint32					uiTarget;
	int64					iEntryS;
	size					uiLength;
	int						i;
	int						iRegister;
	bool					bCut;
	bool					bAlways;
	EAsm65StackSpace		eSpace;

	eSpace = (EAsm65StackSpace)psFrame->uiSpace;
	sState.Init(0, false);
	sBeforeExit = sState;
	asBranches.Init();
	bCut = false;
	for (uiAddress = psFrame->psFunction->sStart.uiOffset; uiAddress < psFrame->uiPC; uiAddress += (uint32)uiLength)
	{
		psBranch = bCut ? FindBranch(&asBranches, uiAddress) : NULL;
		if (psBranch)
		{
			sState = psBranch->sState;
		}
		uiLength = Instruction(eSpace, uiAddress, &sDecoded, &uiImmediate);
		if ((uiLength == 0) || !Step(&sState, &sDecoded, uiImmediate))
		{
			asBranches.Kill();
			return false;
		}
		bCut = IsExit(&sDecoded);
		if (bCut)
		{
			sState = sBeforeExit;
			continue;
		}
		if (BranchTarget(&sDecoded, uiAddress + (uint32)uiLength, uiImmediate, &uiTarget, &bAlways))
		{
			if ((uiTarget > uiAddress) && !FindBranch(&asBranches, uiTarget))
			{
				psBranch = asBranches.Add();
				psBranch->uiTarget = uiTarget;
				psBranch->sState = sState;
			}
			bCut = bAlways;
		}
		if (!IsUnwinding(&sDecoded))
		{
			sBeforeExit = sState;
		}
	}
	asBranches.Kill();

	iEntryS = -1;
	for (i = 0; (i < 8) && (iEntryS < 0); i++)
	{
		iRegister = DebugRegister((E65Register)i);
		if ((iRegister >= 0) && sState.asRegisters[i].bKnown && psFrame->IsKnown((size)iRegister))
		{
			iEntryS = (int64)(uint32)psFrame->Get((size)iRegister) + sState.asRegisters[i].iBelow;
		}
	}
	if (iEntryS < 0)
	{
		return false;
	}

	if (sState.sPushedV.bKnown)
	{
		if (!ReadValue(eSpace, (uint32)(iEntryS - sState.sPushedV.iBelow), 4, &uiV))
		{
			return false;
		}
		psCaller->Set(A65DR_V, uiV);
	}
	else if (!sState.bCallerV)
	{
		psCaller->Forget(A65DR_V);
	}
	*puiEntryS = (uint32)iEntryS;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A function taking its frame apart: its code from psFrame's PC to its
// return (ret, or rti as it was got to) runs straight on, pulls from S
// (pul V giving the caller's V), add S, cmp and wth only, so S at its
// return is its entry S.  False for any other.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65C64StackWalker::Returning(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller)
{
	SAsm65C64StackState	sState;
	S65C64Decoded		sDecoded;
	uint64				uiImmediate;
	uint64				uiV;
	uint32				uiAddress;
	uint32				uiS;
	size				uiLength;
	size				ui;
	int					i;
	int64				iValue;
	EAsm65StackSpace	eSpace;
	EAsm65DebugEntry	eEntry;

	eSpace = (EAsm65StackSpace)psFrame->uiSpace;
	eEntry = (EAsm65DebugEntry)psFrame->psFunction->uiEntry;
	sState.Init((uint16)psFrame->Get(A65DR_P), psFrame->IsKnown(A65DR_P));
	uiS = psFrame->GetS();
	uiAddress = psFrame->uiPC;
	for (ui = 0; ui < STACK_WALKER_65C64_MAX_RETURNING; ui++)
	{
		uiLength = Instruction(eSpace, uiAddress, &sDecoded, &uiImmediate);
		if (uiLength == 0)
		{
			return false;
		}
		if ((sDecoded.eInstruction == I65_Ret) || (sDecoded.eInstruction == I65_Rti))
		{
			if ((sDecoded.eInstruction == I65_Rti) != ((eEntry == ADE_Interrupt) || (eEntry == ADE_UserInterrupt)))
			{
				return false;
			}
			*puiEntryS = uiS;
			return true;
		}
		if (!IsUnwinding(&sDecoded))
		{
			return false;
		}
		switch (sDecoded.eInstruction)
		{
		case I65_PulRegisters:
			for (i = 6; i >= 0; i--)
			{
				if (sDecoded.uiMask & (1 << i))
				{
					if (sState.Bytes((E65Register)(R65_A + i)) == 0)
					{
						return false;
					}
					if (R65_A + i == R65_V)
					{
						if (!ReadValue(eSpace, uiS + 1, 4, &uiV))
						{
							return false;
						}
						psCaller->Set(A65DR_V, uiV);
					}
					uiS += sState.Bytes((E65Register)(R65_A + i));
				}
			}
			break;
		case I65_PulOperand:
			uiS += 4;
			break;
		case I65_Wth:
		case I65_Wtth:
		case I65_Wthx:
			Step(&sState, &sDecoded, uiImmediate);
			break;
		case I65_Add:
			if (!OperandValue(&sDecoded, uiImmediate, &iValue))
			{
				return false;
			}
			uiS += (uint32)iValue;
			break;
		default:
			break;
		}
		uiAddress += (uint32)uiLength;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// jsr and brsr push the 4 byte address they go back to; an interrupt (or
// brk) that, then P.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65C64StackWalker::CallBytes(EAsm65DebugEntry eEntry, uint32* puiPBytes, uint32* puiReturnBytes)
{
	*puiPBytes = ((eEntry == ADE_Interrupt) || (eEntry == ADE_UserInterrupt)) ? 2 : 0;
	*puiReturnBytes = 4;
}


//////////////////////////////////////////////////////////////////////////
//
// A call pushes the address it goes back to: the instruction after it, or
// the one interrupted.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65C64StackWalker::ReturnAddress(EAsm65DebugEntry eEntry, uint64 uiPushed, uint32 uiCalleePC)
{
	return (uint32)uiPushed;
}


//////////////////////////////////////////////////////////////////////////
//
// A call keeps V (a function that changes it pushes it first and pulls it
// before it returns).
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65C64StackWalker::PreservedRegisters(void)
{
	return 1u << A65DR_V;
}


//////////////////////////////////////////////////////////////////////////
//
// The stacks are anywhere in the 32 bit address space.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65C64StackWalker::StackAddress(int64 iAddress)
{
	return (uint32)iAddress;
}


//////////////////////////////////////////////////////////////////////////
//
// A pointer is 32 bits.
//
//////////////////////////////////////////////////////////////////////////
size CAsm65C64StackWalker::PointerBytes(void)
{
	return 4;
}

