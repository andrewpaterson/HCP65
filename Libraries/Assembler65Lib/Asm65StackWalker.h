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
#ifndef __ASM65_STACK_WALKER_H__
#define __ASM65_STACK_WALKER_H__
#include "BaseLib/ArrayTemplate.h"
#include "Asm65DebugRecords.h"


class CFastW65C816;
class CFast65C64;


//The memories a walk reads.  A computer with a user mode sees other memory there at the same addresses (the HCP65's
//processes, each its own); one without has only the kernel's.  A kernel's interrupt from user mode is called from user
//mode's code (EAsm65DebugEntry's ADE_UserInterrupt).
enum EAsm65StackSpace
{
	ASS_Kernel,
	ASS_User,
};


//The memory of a processor that has stopped, as a stack walker reads it: its code and its stacks.
class CAsm65StackMemory
{
public:
	virtual bool	Read(EAsm65StackSpace eSpace, uint32 uiAddress, void* pvDest, size uiBytes) = 0;
};


//A stopped processor's registers as a walk knows them, by the processor's own register names (Asm65DebugRecords.h's
//EAsmW65C816DebugRegister or EAsm65C64DebugRegister, S first), with the program address and the space its code runs
//in.  A caller's registers are those its callee saved for it, and those calls keep; the rest aren't known.
struct SAsm65StackRegisters
{
	uint32	uiPC;
	uint8	uiSpace;		//EAsm65StackSpace.
	uint32	uiKnown;		//A bit for each register known.
	uint64	auiValues[ASM65_DEBUG_REGISTERS];

	void	Init(uint32 uiPCIn, EAsm65StackSpace eSpace);
	void	Init(CFastW65C816* pcFast, EAsm65StackSpace eSpace);
	void	Init(CFast65C64* pcFast, EAsm65StackSpace eSpace);
	void	Set(size uiRegister, uint64 uiValue);
	void	Forget(size uiRegister);
	bool	IsKnown(size uiRegister) const;
	uint64	Get(size uiRegister) const;
	uint32	GetS(void) const;
};


//A function on the stack: where it is (uiPC, the instruction it runs next, or for a caller the one its call returns
//to), its registers (S as it was before the call it made; the first frame's are the processor's), the space its code
//and stack are in, and its frame address (S before its own call, bFrame once a walk has found it).  psFunction is NULL
//for code the debug records don't describe: the walk stops there.  bInnermost for the first call of a walk.
//
//A walk with inlines (DebuggerFPlan.txt F2-4) has a row for each inlined call the code is in, innermost first, before
//its function's: iInline is the row's inline (an index among psFunction's, -1 for the function itself), and iCalled
//the inline just inside it (-1 for the innermost row of its call), whose call is the row's line.
struct SAsm65StackFrame : public SAsm65StackRegisters
{
	SAsm65DebugFunction*	psFunction;
	CAsm65DebugRecords*		pcRecords;
	uint32					uiFrame;
	bool					bFrame;
	bool					bInnermost;
	int32					iInline;
	int32					iCalled;

	uint32				GetLineAddress(void);
	const char*			GetName(void);
	SAsm65DebugInline*	GetCalled(void);
	bool				IsInline(void);
};


#define ASM65_STACK_NO_REGISTER	0xFF		//A place's value not from a register (SAsm65StackPlace::uiRegister).


//Where a variable is while its frame runs (CAsm65StackWalker::Locate): at uiAddress in the frame's space, or, when
//bAddress is false, its value: uiRegister's, or (ASM65_STACK_NO_REGISTER) one the records know.
struct SAsm65StackPlace
{
	bool	bAddress;
	uint32	uiAddress;
	uint8	uiRegister;
	uint64	uiValue;
};


//A program's records and the space it runs in.
struct SAsm65StackRecords
{
	CAsm65DebugRecords*	pcRecords;
	EAsm65StackSpace	eSpace;
};


typedef CArrayTemplate<SAsm65StackFrame>	CArrayAsm65StackFrames;
typedef CArrayTemplate<SAsm65StackRecords>	CArrayAsm65StackRecords;


//Finds the calls a stopped processor is in from its registers, its memory and the debug records of the programs
//loaded (a program's, the kernal's), by the frame model (DebuggerPlan.txt, Debugger65C64Plan.txt): each function's
//frame address (S before its call) found as its records say once its body runs (a register plus an offset, or the
//address kept there), or while it makes or takes apart its frame by following its instructions; the registers it
//saved read back from their places; the return address below the frame address.
//
//What isn't a processor's is here: the records, the walk, the rules, the saved registers, the return.  A processor's
//walker (CAsmW65C816StackWalker, CAsm65C64StackWalker) gives what the rules can't: how to follow its instructions in
//a prologue, an epilogue or code with no rule, how its calls push their return, and its addresses' widths.
class CAsm65StackWalker
{
protected:
	CArrayAsm65StackRecords	masRecords;
	CAsm65StackMemory*		mpcMemory;

public:
	void					Init(CAsm65StackMemory* pcMemory);
	virtual void			Kill(void);

	void					AddRecords(CAsm65DebugRecords* pcRecords, EAsm65StackSpace eSpace);
	void					ClearRecords(void);
	size					Walk(SAsm65StackRegisters* psRegisters, CArrayAsm65StackFrames* pasFrames, size uiMaxFrames, bool bInlines = false);
	bool					Locate(SAsm65StackFrame* psFrame, SAsm65DebugVariable* psVariable, SAsm65StackPlace* psPlace);
	virtual EAsm65DebugProcessor	GetProcessor(void) = 0;

protected:
	bool					Find(uint32 uiPC, EAsm65StackSpace eSpace, SAsm65StackFrame* psFrame);
	void					AddFrame(CArrayAsm65StackFrames* pasFrames, SAsm65StackFrame* psFrame, bool bInlines);
	bool					Caller(SAsm65StackFrame* psFrame, SAsm65StackFrame* psCaller);
	bool					FrameAddress(SAsm65StackFrame* psFrame, uint32* puiFrame);
	bool					Base(SAsm65StackFrame* psFrame, uint8 uiBase, uint64* puiValue);
	bool					LocateAt(SAsm65StackFrame* psFrame, uint8 uiWhere, uint8 uiBase, int32 iOffset, SAsm65StackPlace* psPlace);
	bool					ReadSaved(SAsm65StackFrame* psFrame, SAsm65StackRegisters* psCaller, uint32* puiEntryS);
	bool					ReturnTo(SAsm65StackFrame* psFrame, uint32 uiEntryS, SAsm65StackRegisters* psCallerRegisters, SAsm65StackFrame* psCaller);
	bool					ReadValue(EAsm65StackSpace eSpace, uint32 uiAddress, size uiBytes, uint64* puiValue);
	EAsm65StackSpace		CallerSpace(SAsm65StackFrame* psFrame);
	uint32					EntryBytes(EAsm65DebugEntry eEntry);

	//The processor's: S at entry (S below the call's pushes) and the caller's registers, from following the
	//function's instructions from its start (its prologue, or code with no rule), or to its return (its epilogue:
	//false if it isn't taking its frame apart).
	virtual bool			Follow(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller) = 0;
	virtual bool			Returning(SAsm65StackFrame* psFrame, uint32* puiEntryS, SAsm65StackRegisters* psCaller) = 0;
	//How a call got to as eEntry says pushed its return: P's bytes below the return address's, and where it goes on.
	virtual void			CallBytes(EAsm65DebugEntry eEntry, uint32* puiPBytes, uint32* puiReturnBytes) = 0;
	virtual uint32			ReturnAddress(EAsm65DebugEntry eEntry, uint64 uiPushed, uint32 uiCalleePC) = 0;
	virtual uint32			PreservedRegisters(void) = 0;
	virtual uint32			StackAddress(int64 iAddress) = 0;
	virtual size			PointerBytes(void) = 0;
};


CAsm65StackWalker*	Asm65NewStackWalker(EAsm65DebugProcessor eProcessor, CAsm65StackMemory* pcMemory);


#endif // __ASM65_STACK_WALKER_H__
