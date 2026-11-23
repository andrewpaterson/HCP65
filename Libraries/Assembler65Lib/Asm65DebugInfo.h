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
#ifndef __ASM65_DEBUG_INFO_H__
#define __ASM65_DEBUG_INFO_H__
#include "Asm65LineTable.h"
#include "Asm65DebugRecords.h"


#define ASM65_DEBUG_INFO_EXTENSION	".debug"	//The file beside the program it is for: OS.SYS.debug.


//What a debugger knows of a linked program: its line table (address to source line, and its functions'
//fingerprints) and its records (functions' frames and variables, globals, types).  The linker makes it
//(CAsm65Linker::GetDebugInfo); a build writes it beside the program (WriteFile) and a debugger reads it (ReadFile).
//A module's is moved from its link's addresses to where it is loaded (Move).
class CAsm65DebugInfo
{
protected:
	CAsm65LineTable		mcLines;
	CAsm65DebugRecords	mcRecords;

public:
	void				Init(void);
	void				Kill(void);
	void				Clear(void);

	CAsm65LineTable*	GetLines(void);
	CAsm65DebugRecords*	GetRecords(void);
	void				Move(int64 iOffset);

	bool				Write(CAbstractFile* pcFile);
	bool				Read(CAbstractFile* pcFile);
	bool				WriteFile(const char* szFilename);
	bool				ReadFile(const char* szFilename);
};


#endif // __ASM65_DEBUG_INFO_H__
