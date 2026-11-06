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
#ifndef __ASM65_EXECUTABLE_H__
#define __ASM65_EXECUTABLE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayUint8.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"
#include "Asm65Output.h"
#include "Asm65Linker.h"


typedef CArrayTemplate<int64>	CArrayAsm65ExecutableValues;


//A linked program as a file a loader places: a CAsm65Object (as CAsm65Object::Write writes it) with one section at
//each linked segment's address, none relocated, a bss section of no bytes (__bss_start to __bss_end) that the loader
//clears, and absolute globals: the entry point (the linked address of the Entry symbol, under that name) and the
//symbols given here with their values, which a loader or the program reads (an HCP65 program's UserHeapSize).  The
//limit is the highest address the linker may place anything at (CCPP65Builder::SetLimit, zero for none).  A build
//configuration describes one as
//
//	<Executable File="boot.exe" Entry="UserStart" Limit="0x200000">
//		<Symbol Name="UserHeapSize" Value="0x20000"/>
//	</Executable>
//
//with numbers in hexadecimal (0x) or decimal.
class CAsm65Executable
{
protected:
	CChars						mszFile;
	CChars						mszEntry;
	uint32						muiLimit;
	CArrayChars					maszSymbols;
	CArrayAsm65ExecutableValues	maiValues;
	CArrayUint8					macBytes;

public:
	void		Init(void);
	void		Init(const char* szFile, const char* szEntry, uint32 uiLimit);
	void		Kill(void);

	void		AddSymbol(const char* szName, int64 iValue);

	void		SaveXML(CXMLWriter* pcWriter);
	bool		LoadXML(CMarkupTag* pcTag);

	bool		Make(CAsm65Output* pcOutput, CAsm65Linker* pcLinker, CChars* pszErrors);
	bool		Write(const char* szFilename);

	char*		GetFile(void);
	char*		GetEntry(void);
	uint32		GetLimit(void);
	size		NumSymbols(void);
	char*		GetSymbolName(size uiIndex);
	int64		GetSymbolValue(size uiIndex);
	uint8*		GetBytes(void);
	size		GetSize(void);
};


#endif // __ASM65_EXECUTABLE_H__

