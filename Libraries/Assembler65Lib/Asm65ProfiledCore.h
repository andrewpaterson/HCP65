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
#ifndef __ASM65_PROFILED_CORE_H__
#define __ASM65_PROFILED_CORE_H__
#include "MicroprocessorLib/FastW65C816.h"
#include "MicroprocessorLib/Fast65C64.h"
#include "Asm65Memory.h"


//A processor CAsm65Profile runs a program on an instruction at a time: where it is, what the instruction there does to
//the flow of the program, and the cycles it has run for.  Addresses are the program's (24 bits of them are profiled).
class CAsm65ProfiledCore
{
public:
	virtual					~CAsm65ProfiledCore(void);

	virtual bool			IsStopped(void) =0;
	virtual uint64			NumCycles(void) =0;
	virtual uint32			GetProgramAddress(void) =0;		//Of the instruction Step runs next.
	virtual uint32			GetStack(void) =0;				//S: lower while a call runs than when it was made.
	virtual void			Step(void) =0;

	virtual bool			IsCall(uint32 uiAddress) =0;	//The instruction there calls or jumps (to a function's start: a call of it).
	virtual bool			IsReturn(uint32 uiAddress) =0;

	//A frame register whose value says whose frame an instruction's operand is in (the W65C816's D): none by default.
	virtual bool			HasFrameOperand(uint32 uiAddress);	//The instruction there reads or writes through it.
	virtual bool			SetsFrame(uint32 uiAddress);		//The instruction there sets it.
	virtual uint16			GetFrame(void);

	//The letters after a function's label and a dot that name its register argument entry ("axy").
	virtual const char*		GetRegisterEntryLetters(void) =0;
};


//The W65C816 (CFastW65C816) running a program in pcMemory.
class CAsm65ProfiledW65C816 : public CAsm65ProfiledCore
{
protected:
	CAsm65Memory*	mpcMemory;
	CFastW65C816*	mpcFast;

public:
	void			Init(CAsm65Memory* pcMemory, CFastW65C816* pcFast);

	bool			IsStopped(void) override;
	uint64			NumCycles(void) override;
	uint32			GetProgramAddress(void) override;
	uint32			GetStack(void) override;
	void			Step(void) override;
	bool			IsCall(uint32 uiAddress) override;
	bool			IsReturn(uint32 uiAddress) override;
	bool			HasFrameOperand(uint32 uiAddress) override;
	bool			SetsFrame(uint32 uiAddress) override;
	uint16			GetFrame(void) override;
	const char*		GetRegisterEntryLetters(void) override;
};


//The 65C64 (CFast65C64) running a program in pcMemory.
class CAsm65Profiled65C64 : public CAsm65ProfiledCore
{
protected:
	CAsm65Memory*	mpcMemory;
	CFast65C64*		mpcFast;

public:
	void			Init(CAsm65Memory* pcMemory, CFast65C64* pcFast);

	bool			IsStopped(void) override;
	uint64			NumCycles(void) override;
	uint32			GetProgramAddress(void) override;
	uint32			GetStack(void) override;
	void			Step(void) override;
	bool			IsCall(uint32 uiAddress) override;
	bool			IsReturn(uint32 uiAddress) override;
	const char*		GetRegisterEntryLetters(void) override;
};


#endif // __ASM65_PROFILED_CORE_H__

