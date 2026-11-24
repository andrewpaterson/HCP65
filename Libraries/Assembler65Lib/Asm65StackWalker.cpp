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
#include <string.h>
#include "BaseLib/ConstructorCall.h"
#include "AsmW65C816StackWalker.h"
#include "Asm65C64StackWalker.h"


//////////////////////////////////////////////////////////////////////////
//
// The code at uiPCIn in eSpace, no register known.
//
//////////////////////////////////////////////////////////////////////////
void SAsm65StackRegisters::Init(uint32 uiPCIn, EAsm65StackSpace eSpace)
{
	uiPC = uiPCIn;
	uiSpace = (uint8)eSpace;
	uiKnown = 0;
	memset(auiValues, 0, sizeof(auiValues));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SAsm65StackRegisters::Set(size uiRegister, uint64 uiValue)
{
	auiValues[uiRegister] = uiValue;
	uiKnown |= 1u << uiRegister;
}


void SAsm65StackRegisters::Forget(size uiRegister) { uiKnown &= ~(1u << uiRegister); }
bool SAsm65StackRegisters::IsKnown(size uiRegister) const { return (uiKnown & (1u << uiRegister)) != 0; }
uint64 SAsm65StackRegisters::Get(size uiRegister) const { return auiValues[uiRegister]; }
uint32 SAsm65StackRegisters::GetS(void) const { return (uint32)auiValues[ASM65_DEBUG_S]; }


//////////////////////////////////////////////////////////////////////////
//
// The address whose source line the frame is at: the instruction it runs
// next for the innermost frame (the first of a walk), else its call's,
// just before where the call returns to.
//
//////////////////////////////////////////////////////////////////////////
uint32 SAsm65StackFrame::GetLineAddress(void)
{
	return bInnermost ? uiPC : uiPC - 1;
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
// A program's records, for its code running in eSpace.  The records are
// the caller's, and must outlive the walker's walks.
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
// No programs' records: they are added again for the next walk.
//
//////////////////////////////////////////////////////////////////////////
void CAsm65StackWalker::ClearRecords(void)
{
	masRecords.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The calls the processor is in, innermost first, up to uiMaxFrames: the
// function running, then each caller while its callee's is found.  The
// last has no function (psFunction NULL) where the walk reached code the
// records don't describe.  A caller on the same stack is above its
// callee; an interrupt from user mode goes over to the process's stack.
// Returns how many.
//
//////////////////////////////////////////////////////////////////////////
size CAsm65StackWalker::Walk(SAsm65StackRegisters* psRegisters, CArrayAsm65StackFrames* pasFrames, size uiMaxFrames)
{
	SAsm65StackFrame*	psFrame;
	SAsm65StackFrame	sCaller;

	pasFrames->ReInit();
	if (uiMaxFrames == 0)
	{
		return 0;
	}

	psFrame = pasFrames->Add();
	*(SAsm65StackRegisters*)psFrame = *psRegisters;
	Find(psRegisters->uiPC, (EAsm65StackSpace)psRegisters->uiSpace, psFrame);
	psFrame->bInnermost = true;
	while ((pasFrames->NumElements() < uiMaxFrames) && psFrame->psFunction)
	{
		if (!Caller(psFrame, &sCaller) || ((sCaller.uiSpace == psFrame->uiSpace) && (sCaller.GetS() <= psFrame->GetS())))
		{
			break;
		}
		sCaller.bInnermost = false;
		psFrame = pasFrames->Add(&sCaller);
	}
	return pasFrames->NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// Where psVariable, one of psFrame's function's, is while it runs, in
// psFrame's space: its function's base plus its offset, or where the
// pointer kept there points; or, for a variable whose place moves
// (ADW_Ranges), where its range at the frame's line address says, which
// may be a register's value.  False if it isn't kept anywhere then, or a
// register it needs isn't known (a caller's that wasn't saved), or the
// pointer can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Locate(SAsm65StackFrame* psFrame, SAsm65DebugVariable* psVariable, SAsm65StackPlace* psPlace)
{
	SAsm65DebugRange*	psRange;

	if (psVariable->uiWhere != ADW_Ranges)
	{
		return LocateAt(psFrame, psVariable->uiWhere, psFrame->psFunction->uiBase, psVariable->iOffset, psPlace);
	}

	psRange = psFrame->pcRecords->FindRange(psVariable, psFrame->GetLineAddress());
	if (!psRange)
	{
		return false;
	}
	switch (psRange->uiWhere)
	{
	case ADW_Frame:
		return LocateAt(psFrame, ADW_Frame, psFrame->psFunction->uiBase, psRange->iOffset, psPlace);
	case ADW_Memory:
		return LocateAt(psFrame, ADW_Frame, psRange->uiRegister, psRange->iOffset, psPlace);
	case ADW_Register:
		if (!psFrame->IsKnown(psRange->uiRegister))
		{
			return false;
		}
		psPlace->bAddress = false;
		psPlace->uiAddress = 0;
		psPlace->uiValue = psFrame->Get(psRange->uiRegister);
		return true;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The value of uiBase in psFrame: a register's, or its frame address
// (ASM65_DEBUG_FRAME).  False if it isn't known.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Base(SAsm65StackFrame* psFrame, uint8 uiBase, uint64* puiValue)
{
	if (uiBase == ASM65_DEBUG_FRAME)
	{
		*puiValue = psFrame->uiFrame;
		return psFrame->bFrame;
	}
	if (!psFrame->IsKnown(uiBase))
	{
		return false;
	}
	*puiValue = psFrame->Get(uiBase);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The address at uiBase + iOffset (ADW_Frame), or the one kept there
// (ADW_Indirect).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::LocateAt(SAsm65StackFrame* psFrame, uint8 uiWhere, uint8 uiBase, int32 iOffset, SAsm65StackPlace* psPlace)
{
	uint64	uiValue;
	uint32	uiPlace;

	if (!Base(psFrame, uiBase, &uiValue))
	{
		return false;
	}

	uiPlace = StackAddress((int64)uiValue + iOffset);
	switch (uiWhere)
	{
	case ADW_Frame:
		break;
	case ADW_Indirect:
		if (!ReadValue((EAsm65StackSpace)psFrame->uiSpace, uiPlace, PointerBytes(), &uiValue))
		{
			return false;
		}
		uiPlace = (uint32)uiValue;
		break;
	default:
		return false;
	}
	psPlace->bAddress = true;
	psPlace->uiAddress = uiPlace;
	psPlace->uiValue = 0;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// psFrame's function and records for code at uiPC in eSpace; false
// (NULL) if no records describe it.  Its frame address isn't known yet.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Find(uint32 uiPC, EAsm65StackSpace eSpace, SAsm65StackFrame* psFrame)
{
	size				ui;
	SAsm65StackRecords*	psRecords;

	psFrame->uiPC = uiPC;
	psFrame->uiSpace = (uint8)eSpace;
	psFrame->uiFrame = 0;
	psFrame->bFrame = false;
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
// psFrame's caller, and psFrame's frame address: from the instructions
// from its start if it hasn't made its frame yet (or has no rule), from
// the instructions to its return if it is taking its frame apart, and
// from its rule and saved registers otherwise.  Its caller's registers are
// those calls keep and those it saved.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::Caller(SAsm65StackFrame* psFrame, SAsm65StackFrame* psCaller)
{
	SAsm65DebugFunction*	psFunction;
	SAsm65StackRegisters	sCaller;
	SAsm65StackRegisters	sReturning;
	uint32					uiEntryS;

	psFunction = psFrame->psFunction;
	sCaller = *psFrame;
	sCaller.uiKnown &= PreservedRegisters();
	if ((psFrame->uiPC < psFunction->uiBody) || (psFunction->uiFrame == ADF_FollowS))
	{
		if (!Follow(psFrame, &uiEntryS, &sCaller))
		{
			return false;
		}
		psFrame->uiFrame = uiEntryS + EntryBytes((EAsm65DebugEntry)psFunction->uiEntry);
	}
	else
	{
		sReturning = sCaller;
		if (Returning(psFrame, &uiEntryS, &sReturning))
		{
			sCaller = sReturning;
			psFrame->uiFrame = uiEntryS + EntryBytes((EAsm65DebugEntry)psFunction->uiEntry);
		}
		else if (!FrameAddress(psFrame, &psFrame->uiFrame) || !ReadSaved(psFrame, &sCaller, &uiEntryS))
		{
			return false;
		}
	}
	psFrame->bFrame = true;
	return ReturnTo(psFrame, uiEntryS, &sCaller, psCaller);
}


//////////////////////////////////////////////////////////////////////////
//
// psFrame's frame address by its function's rule: a register plus an
// offset, or the address held there plus an adjustment.  False if the
// register isn't known or the address can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::FrameAddress(SAsm65StackFrame* psFrame, uint32* puiFrame)
{
	SAsm65DebugFunction*	psFunction;
	uint64					uiValue;

	psFunction = psFrame->psFunction;
	if (!psFrame->IsKnown(psFunction->uiFrameRegister))
	{
		return false;
	}
	uiValue = psFrame->Get(psFunction->uiFrameRegister) + psFunction->iFrameOffset;
	if (psFunction->uiFrame == ADF_Memory)
	{
		if (!ReadValue((EAsm65StackSpace)psFrame->uiSpace, StackAddress((int64)uiValue), Asm65DebugRegisterBytes(GetProcessor(), ASM65_DEBUG_S), &uiValue))
		{
			return false;
		}
		uiValue += psFunction->iFrameAdjust;
	}
	*puiFrame = StackAddress((int64)uiValue);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The registers psFrame's function saved, read back from their places
// from its frame address into psCaller.  S at its entry is below its
// call's pushes, or where it saved S (a kernel's interrupt from user mode,
// on the kernel's own stack, keeps the process's S).
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::ReadSaved(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psCaller, uint32* puiEntryS)
{
	SAsm65DebugFunction*	psFunction;
	SAsm65DebugSaved*		psSaved;
	uint64					uiValue;
	size					ui;

	psFunction = psFrame->psFunction;
	*puiEntryS = psFrame->uiFrame - EntryBytes((EAsm65DebugEntry)psFunction->uiEntry);
	for (ui = 0; ui < psFunction->uiNumSaved; ui++)
	{
		psSaved = psFrame->pcRecords->GetSaved(psFunction, ui);
		if (psSaved->bMemory)
		{
			continue;
		}
		if (!ReadValue((EAsm65StackSpace)psFrame->uiSpace, StackAddress((int64)psFrame->uiFrame + psSaved->iOffset), psSaved->uiBytes, &uiValue))
		{
			return false;
		}
		if (psSaved->uiWhat == ASM65_DEBUG_S)
		{
			*puiEntryS = (uint32)uiValue;
		}
		else
		{
			psCaller->Set(psSaved->uiWhat, uiValue);
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// psFrame's caller, its registers psCallerRegisters: it runs on from where
// its call (psFrame's function's entry) goes back to, pushed above
// uiEntryS (S as the function was entered), with S as it was before the
// call.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::ReturnTo(SAsm65StackFrame* psFrame, uint32 uiEntryS, SAsm65StackRegisters* psCallerRegisters, SAsm65StackFrame* psCaller)
{
	EAsm65DebugEntry	eEntry;
	EAsm65StackSpace	eSpace;
	uint32				uiPBytes;
	uint32				uiReturnBytes;
	uint64				uiPushed;
	int64				iCallerS;

	eEntry = (EAsm65DebugEntry)psFrame->psFunction->uiEntry;
	eSpace = CallerSpace(psFrame);
	CallBytes(eEntry, &uiPBytes, &uiReturnBytes);
	iCallerS = (int64)uiEntryS + uiPBytes + uiReturnBytes;

	//A call can't have pushed past the top of the stack: S would have wrapped round.
	if ((StackAddress(iCallerS) != iCallerS) || !ReadValue(eSpace, uiEntryS + 1 + uiPBytes, uiReturnBytes, &uiPushed))
	{
		return false;
	}
	*(SAsm65StackRegisters*)psCaller = *psCallerRegisters;
	Find(ReturnAddress(eEntry, uiPushed, psFrame->uiPC), eSpace, psCaller);
	psCaller->Set(ASM65_DEBUG_S, (uint64)iCallerS);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// uiBytes (up to 8) little endian from uiAddress in eSpace.
//
//////////////////////////////////////////////////////////////////////////
bool CAsm65StackWalker::ReadValue(EAsm65StackSpace eSpace, uint32 uiAddress, size uiBytes, uint64* puiValue)
{
	uint8	auiBytes[8];
	size	ui;

	if ((uiBytes > 8) || !mpcMemory->Read(eSpace, uiAddress, auiBytes, uiBytes))
	{
		return false;
	}
	*puiValue = 0;
	for (ui = 0; ui < uiBytes; ui++)
	{
		*puiValue |= (uint64)auiBytes[ui] << (ui * 8);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The space psFrame's caller runs in: user mode's for a kernel's
// interrupt from it.
//
//////////////////////////////////////////////////////////////////////////
EAsm65StackSpace CAsm65StackWalker::CallerSpace(SAsm65StackFrame* psFrame)
{
	return (psFrame->psFunction->uiEntry == ADE_UserInterrupt) ? ASS_User : (EAsm65StackSpace)psFrame->uiSpace;
}


//////////////////////////////////////////////////////////////////////////
//
// The bytes a call got to as eEntry says pushes: its frame address is S at
// the function's entry plus these.
//
//////////////////////////////////////////////////////////////////////////
uint32 CAsm65StackWalker::EntryBytes(EAsm65DebugEntry eEntry)
{
	uint32	uiPBytes;
	uint32	uiReturnBytes;

	CallBytes(eEntry, &uiPBytes, &uiReturnBytes);
	return uiPBytes + uiReturnBytes;
}


//////////////////////////////////////////////////////////////////////////
//
// A walker for the processor's stacks, Init'ed on pcMemory, for SafeKill;
// NULL for a processor that has none.
//
//////////////////////////////////////////////////////////////////////////
CAsm65StackWalker* Asm65NewStackWalker(EAsm65DebugProcessor eProcessor, CAsm65StackMemory* pcMemory)
{
	CAsm65StackWalker*	pcWalker;

	switch (eProcessor)
	{
	case ADP_W65C816:
		pcWalker = NewMalloc<CAsmW65C816StackWalker>();
		break;
	case ADP_65C64:
		pcWalker = NewMalloc<CAsm65C64StackWalker>();
		break;
	default:
		return NULL;
	}
	pcWalker->Init(pcMemory);
	return pcWalker;
}
