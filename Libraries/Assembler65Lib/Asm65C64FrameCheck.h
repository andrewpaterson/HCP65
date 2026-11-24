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
#ifndef __ASM_65C64_FRAME_CHECK_H__
#define __ASM_65C64_FRAME_CHECK_H__
#include "MicroprocessorLib/Fast65C64.h"
#include "Asm65Memory.h"
#include "Asm65FrameCheck.h"


//Runs a program on CFast65C64 a step at a time, following its calls (CAsm65FrameCheck): jsr, brsr and brk start one
//(S before it, its 4 byte return below), ret and rti end them.  Calls keep V.  BeforeStep and AfterStep go round each
//step of whatever runs the program.
class CAsm65C64FrameCheck : public CAsm65FrameCheck
{
protected:
	CAsm65Memory*	mpcMemory;
	CFast65C64*		mpcFast;

public:
	void	Init(CAsm65Memory* pcMemory, CFast65C64* pcFast);

	uint16	BeforeStep(void);
	void	AfterStep(uint16 uiOpcode);

	bool	IsStopped(void) override;
	void	GetRegisters(SAsm65StackRegisters* psRegisters) override;

protected:
	void	Step(void) override;
	size	KeptRegister(void) override;
	uint64	Kept(void) override;
};


#endif // __ASM_65C64_FRAME_CHECK_H__

