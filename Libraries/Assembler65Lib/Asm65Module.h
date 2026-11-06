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
#ifndef __ASM65_MODULE_H__
#define __ASM65_MODULE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayUint8.h"
#include "Asm65Output.h"
#include "Asm65Linker.h"


#define ASM65_MODULE_SECTION		"module"
#define ASM65_MODULE_INIT_START		"__init_start"	//The table of routines that construct the module's globals,
#define ASM65_MODULE_INIT_END		"__init_end"	//a 4 byte address each, which the loader calls in order.
#define ASM65_MODULE_MAX_SIZE		0x10000			//A module is kept in one bank.


//A relocatable link (CAsm65Linker::SetRelocatable) as a file a loader can put anywhere in one bank (an HCP65 kernal
//driver): a CAsm65Object (as CAsm65Object::Write writes it) with one section, ASM65_MODULE_SECTION, holding the link's
//bytes from its text to the end of its data, its size taking in its bss (which the loader clears).
//
//Its relocations are the link's fixups, all in that section: a target section of 0 is the section's own address
//plus the addend; a target symbol is an import, a name the loader finds (the kernal's exports), plus the addend.
//Its global symbols are offsets in the section: the entry, ASM65_MODULE_INIT_START and ASM65_MODULE_INIT_END.
class CAsm65Module
{
protected:
	CChars		mszEntry;
	CArrayUint8	macBytes;

public:
	void		Init(const char* szEntry);
	void		Kill(void);

	bool		Make(CAsm65Output* pcOutput, CAsm65Linker* pcLinker, CChars* pszErrors);
	bool		Write(const char* szFilename);

	char*		GetEntry(void);
	uint8*		GetBytes(void);
	size		GetSize(void);
};


#endif // __ASM65_MODULE_H__
